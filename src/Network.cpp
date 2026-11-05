// Network.cpp: reconstructed from Tibia.exe 7.72 (0x4ca0e0-0x4cf982).
//
// Functions are in original source order, which comes from the assertion and
// catch line numbers; the comment before each gives its address and those
// lines. The functions without line evidence come first, as in the binary.
// Where TBEGIN sits relative to the assertions follows the try-block states
// in the exception tables (tools/ehinfo.py).
// Wire format, from SendCommand and PrepareNextPacket:
//   unencrypted: [u16 length][payload]
//   encrypted:   [u16 total][XTEA([u16 length][payload][random padding to 8])]

#include "Network.h"
#include "MainFrm.h"
#include "Utils.h"
#include "TXTEA.h"
#include "Preferences.h"
#include "Status.h"
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

class TSocket : public CAsyncSocket {
public:
	void OnReceive(int ErrorCode) override;
	void OnSend(int /*ErrorCode*/) override {} // 0x4eac00, an empty function shared by several vtables
	void OnConnect(int ErrorCode) override;
	void OnClose(int ErrorCode) override;
};

namespace {

// Data, in .data/.bss order.
int HostnameResolveBufferSize = MAXGETHOSTSTRUCT; // 0x5c22c4, initialised to 0x400
int CurrentServer;                                // 0x7152f0
TServer TestServer;                               // 0x7152f8
std::array<TServer, NUMBEROFSERVER> Servers;      // 0x715368
HANDLE HostnameRequest;                           // 0x715528
char *HostnameResolveBuffer;                      // 0x71552c, a HOSTENT filled by WSAAsyncGetHostByName
int ReceiveDataSize;                              // 0x715530, bytes in ReceiveData
int ReceivePacketEnd;                             // 0x715534, end of the complete packets
int ReceiveRead;                                  // 0x715538, start of the next unread packet

// Without AddressSanitizer's redzones (issue #70): a packet whose inner length is larger than the data read
// makes TReadBuffer read on past ReceiveData, into SendData and the globals after it, as in the original.
PATELLA_NO_SANITIZE_ADDRESS std::array<uint8, 0x4000> ReceiveData; // 0x715540
PATELLA_NO_SANITIZE_ADDRESS std::array<uint8, 0x800> SendData;     // 0x719540

HWND NetworkWindow;                                 // 0x719d40, receives WM_USER+1 when a name is resolved
void (*CloseCallback)();                            // 0x719d44
void (*ReceiveCallback)();                          // 0x719d48
void (*ConnectCallback)();                          // 0x719d4c
TSocket *Socket;                                    // 0x719d50
int TimeoutCounter;                                 // 0x719d54
int TimeoutLimit;                                   // 0x719d58, in CheckConnectionTimeout calls
TWriteBuffer SendBuffer(SendData);                  // 0x719d5c, initialiser at 0x559fd0
TReadBuffer ReceiveBuffer({ReceiveData.data(), 0}); // 0x719d68, initialiser at 0x559ff0
TXTEA SymmetricKey;                                 // 0x719d74, initialiser at 0x55a010
int RecursionProtection;                            // 0x719d88

} // namespace

// The arrays std::arrays (issue #60).
static_assert(sizeof(Servers) == NUMBEROFSERVER * sizeof(TServer) && sizeof(ReceiveData) == 0x4000 && sizeof(SendData) == 0x800,
	"Network.cpp's arrays");

// 0x4ca0e0
void InitServerList()
{
	memset(Servers.data(), 0, sizeof(Servers));
	Servers[0].Address.Assign("server.tibia.com");
	Servers[1].Address.Assign("server2.tibia.com");
	Servers[2].Address.Assign("tibia1.cipsoft.com");
	Servers[3].Address.Assign("tibia2.cipsoft.com");
	TestServer.ErrorCode = 0;
	TestServer.Resolved = false;
	TestServer.Available = true;
	for (int i = 0; i < NUMBEROFSERVER; i++) {
		Servers[i].Port = 7171;
		Servers[i].Available = true;
	}
	CurrentServer = rand() % NUMBEROFSERVER;
}

// 0x4ca260
void SetTestServerPort(int Port)
{
	TestServer.Port = Port;
}

// 0x4ca270
void ResetServerAvailability()
{
	for (int i = 0; i < NUMBEROFSERVER; i++)
		Servers[i].Available = true;
	TestServer.Available = true;
}

// 0x4ca290
void SetConnectionTimeout(int Timeout)
{
	TimeoutCounter = 0;
	TimeoutLimit = 0;
	if (Timeout > 0)
		TimeoutLimit = Timeout;
}

// 0x4ca2b0
void SetNetworkWindow(HWND Window)
{
	NetworkWindow = Window;
}

// 0x4ca450, lines 132-133
void SetTestServerAddress(const char *Address)
{
	TASSERT(132, Address!=NULL);
	TASSERT1(133, strlen(Address)<100, strlen(Address));
	TestServer.Address.Assign(Address);
}

// 0x4ca730, lines 152-164
void SetServerError(int errorcode)
{
	TASSERT1(152, CurrentServer<NUMBEROFSERVER, CurrentServer);
	TBEGIN
	if (GetTestServerLogin()) {
		TestServer.Available = false;
		TestServer.ErrorCode = errorcode;
		return;
	}
	Servers[CurrentServer].Available = false;
	Servers[CurrentServer].ErrorCode = errorcode;
	TEND1(164, errorcode)
}

// 0x4ca9f0, lines 182-184
void SetServerIPAddress(const char *ipaddress)
{
	TASSERT1(182, CurrentServer<NUMBEROFSERVER, CurrentServer);
	TASSERT(183, ipaddress!=NULL);
	TASSERT1(184, strlen(ipaddress)<100, strlen(ipaddress));
	if (GetTestServerLogin()) {
		TestServer.Address.Assign(ipaddress);
		TestServer.Resolved = true;
		return;
	}
	Servers[CurrentServer].Address.Assign(ipaddress);
	Servers[CurrentServer].Resolved = true;
}

// 0x4cadb0, lines 202-233. Stores the server to try next; false when none is left.
bool GetCurrentServer(TServer **Server)
{
	TASSERT1(202, CurrentServer<NUMBEROFSERVER, CurrentServer);
	TBEGIN
	if (GetTestServerLogin()) {
		*Server = &TestServer;
		const bool Available = TestServer.Available;
		for (int i = 0; i < NUMBEROFSERVER; i++)
			Servers[i].Available = true;
		TestServer.Available = true;
		return Available;
	}
	*Server = &Servers[CurrentServer];
	for (int i = 0; i < NUMBEROFSERVER; i++) {
		if (Servers[CurrentServer].Available) {
			*Server = &Servers[CurrentServer];
			return true;
		}
		CurrentServer++;
		if (CurrentServer >= NUMBEROFSERVER)
			CurrentServer = 0;
	}
	return false;
	TEND(233)
}

// 0x4cb0d0, line 255
int GetServerErrorCode()
{
	TBEGIN
	if (GetTestServerLogin())
		return TestServer.ErrorCode;
	for (int i = 0; i < NUMBEROFSERVER; i++) {
		if (Servers[i].ErrorCode == ERROR_LOGINSERVER_OVERLOADED)
			return ERROR_LOGINSERVER_OVERLOADED;
	}
	TServer *Server;
	GetCurrentServer(&Server);
	return Server->ErrorCode;
	TEND(255)
}

// 0x4cb2a0, lines 268-275. Reserves the 4 header bytes that SendCommand fills in.
void BeginCommand(int CommandType)
{
	TASSERT1(268, In(CommandType,0,255), CommandType);
	TBEGIN
	SendBuffer.Position = 0;
	SendBuffer.PutWord(0);
	SendBuffer.PutWord(0);
	SendBuffer.PutByte((uint8)CommandType);
	TEND1(275, CommandType)
}

// 0x4cb540, line 286
void SendByte(uint8 Data)
{
	TBEGIN
	SendBuffer.PutByte(Data);
	TEND1(286, Data)
}

// 0x4cb6e0, line 297
void SendWord(uint16 Data)
{
	TBEGIN
	SendBuffer.PutWord(Data);
	TEND1(297, Data)
}

// 0x4cb880, line 308
void SendQuad(uint32 Data)
{
	TBEGIN
	SendBuffer.PutQuad(Data);
	TEND1(308, Data)
}

// 0x4cba20, lines 316-320
void SendString(const char *Text)
{
	TASSERT(316, Text!=NULL);
	TBEGIN
	SendBuffer.PutString(Text);
	TEND(320)
}

// 0x4cbca0, line 328. Buffer is the span's pointer, the name of the assertion's text (issue #59).
void SendBytes(patella::TSpan<const uint8> Bytes)
{
	const uint8 *const Buffer = Bytes.data();
	TASSERT(328, Buffer!=NULL);
	SendBuffer.PutBytes(Bytes);
}

// 0x4cbe00, lines 355-367
void SendCommand(bool Encrypt)
{
	TBEGIN
	if (Socket == nullptr)
		return;
	const int Size = SendBuffer.Position - 4;
	if (!Encrypt) {
		SendBuffer.Position = 0;
		SendBuffer.PutWord(0);
		SendBuffer.PutWord((uint16)Size);
		PATELLA_TEST_PACKET_SENT(SendData, Size, 0)
		Socket->Send(&SendData[2], Size + 2, 0);
		return;
	}
	int Length = Size + 2;
	while (Length % 8 != 0) {
		SendBuffer.PutByte((uint8)(rand() % 256));
		Length++;
	}
	SendBuffer.Position = 0;
	SendBuffer.PutWord((uint16)Length);
	SendBuffer.PutWord((uint16)Size);
	PATELLA_TEST_PACKET_SENT(SendData, Size, Length)
	try {
		for (int i = 2; i < Length + 2; i += 8)
			SymmetricKey.EncryptBlock(TXTEA::TBlock(&SendData[i], 8));
	} catch (...) {
		TERROR(355, "Symmetric encryption failed with unknown exception");
	}
	Socket->Send(SendData.data(), Length + 2, 0);
	TEND(367)
}

// 0x4cc130, lines 391-410. Drops the packet already read, then decrypts the next
// complete packet in place and points ReceiveBuffer at its payload.
void PrepareNextPacket()
{
	TBEGIN
	if (ReceiveRead > 0) {
		if (ReceiveRead < ReceiveDataSize)
			memmove(ReceiveData.data(), &ReceiveData[ReceiveRead], ReceiveDataSize - ReceiveRead);
		ReceiveDataSize -= ReceiveRead;
		ReceivePacketEnd -= ReceiveRead;
		ReceiveRead = 0;
	}
	if (ReceivePacketEnd > 0) {
		ReceiveBuffer.Size = 4;
		ReceiveBuffer.Position = 0;
		const int PacketSize = ReceiveBuffer.GetWord();
		if (PacketSize % 8 != 0)
			TERROR1(391, "packet size does not fit to symmetric encryption", PacketSize);
		try {
			for (int i = 2; i < PacketSize + 2; i += 8)
				SymmetricKey.DecryptBlock(TXTEA::TBlock(&ReceiveData[i], 8));
		} catch (...) {
			TERROR(398, "Symmetric decryption failed with unknown exception");
		}
		ReceiveBuffer.SetSize(ReceiveBuffer.GetWord() + 4);
		PATELLA_TEST_PACKET_RECEIVED(ReceiveData, PacketSize)
		memset(PacketHeader.data(), 0, sizeof(PacketHeader));
		ReceiveRead = PacketSize + 2;
		memcpy(PacketHeader.data(), &ReceiveData[2], PacketSize < 16 ? PacketSize : 16);
	}
	TEND(410)
}

// 0x4cc550, line 423. Returns the next command byte, or -1 when no packet is complete.
int GetNextCommand()
{
	TBEGIN
	if (ReceiveBuffer.Position == ReceiveBuffer.Size) {
		PrepareNextPacket();
		if (ReceiveBuffer.Position == ReceiveBuffer.Size)
			return -1;
	}
	return ReceiveBuffer.GetByte();
	TEND(423)
}

// 0x4cc720, line 434
uint8 GetByte()
{
	TBEGIN
	return ReceiveBuffer.GetByte();
	TEND(434)
}

// 0x4cc8b0, line 445
uint16 GetWord()
{
	TBEGIN
	return ReceiveBuffer.GetWord();
	TEND(445)
}

// 0x4cca40, line 456
uint32 GetQuad()
{
	TBEGIN
	return ReceiveBuffer.GetQuad();
	TEND(456)
}

// 0x4ccbd0, lines 466-470. Buffer and MaxLength are the span's pointer and size, the names of the macros' text
// (issue #59; TReadBuffer::GetString).
void GetString(patella::TSpan<char> Text)
{
	const char *const Buffer = Text.data();
	const int MaxLength = static_cast<int>(Text.size());
	TASSERT(466, Buffer!=NULL);
	TBEGIN
	ReceiveBuffer.GetString(Text);
	TEND1(470, MaxLength)
}

// 0x4cce50, line 482. The caller owns the returned string.
char *GetString()
{
	TBEGIN
	return ReceiveBuffer.GetString();
	TEND(482)
}

// 0x4ccfe0, line 493
void GetBytes(patella::TSpan<uint8> Bytes)
{
	TBEGIN
	ReceiveBuffer.GetBytes(Bytes);
	TEND(493)
}

// 0x4cf850, line 518. Called periodically; reconnects when the limit is passed.
void CheckConnectionTimeout()
{
	TBEGIN
	if (TimeoutLimit != 0 && ++TimeoutCounter > TimeoutLimit) {
		TimeoutLimit = 0;
		TimeoutCounter = 0;
		SetServerError(WSAETIMEDOUT);
		Disconnect(false);
		SetConnectionState(0);
		ConnectToLoginServer();
	}
	TEND(518)
}

// 0x4cd180, lines 544-602. ErrorType 1 is the login server, 2 the game server.
void GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE> MessageBuffer, int ErrorCode, int ErrorType)
{
	char *const Message = MessageBuffer.data();
	TASSERT(544, Message!=NULL);
	TBEGIN
	TFixedString<100> Text;
	bool Offline = false;
	Message[0] = 0;
	// Every caller passes new char[1024] (ERROR_MESSAGE_SIZE); the longest message has about 300 characters.
	TStringBuffer Out(Message, MessageBuffer.size());
	switch (ErrorCode) {
	case WSAEINVAL:
	case WSAEADDRNOTAVAIL:
		Text.Assign("Invalid server address.");
		break;
	case WSAEMSGSIZE:
		Text.Assign("Connection error.");
		break;
	case WSAENETDOWN:
	case WSAENETUNREACH:
		Text.Assign("Network is unreachable.");
		break;
	case WSAETIMEDOUT:
		Text.Assign("Connection timeout.");
		break;
	case WSAECONNREFUSED:
		Text.Assign("Connection refused.");
		Offline = true;
		break;
	case WSAEHOSTUNREACH:
		Text.Assign("Server is unreachable.");
		break;
	case WSAHOST_NOT_FOUND:
		Text.Assign("Server name does not exist.");
		break;
	case WSATRY_AGAIN:
		Text.Assign("Could not resolve server name.");
		break;
	case WSANO_DATA:
		Text.Assign("Error while resolving the server name.");
		break;
	case ERROR_LOGINSERVER_OVERLOADED:
		Text.Assign("Login server is overloaded. Please try again later.");
		break;
	case ERROR_DISCONNECTED:
		Text.Assign("Disconnected from server.");
		break;
	default:
		Text.Format("{}", ErrorCode);
		break;
	}

	if (GetTestServerLogin()) {
		Out.Append("Cannot connect to the test server.\n\nError: ");
		Out.Append(Text);
		if (Offline) {
			Out.Append("\n\nThe test server is offline. Check www.tibia.com");
			Out.Append("\nfor more information on any current test.");
		} else {
			Out.Append("\n\nCheck www.tibia.com for more information on ");
			Out.Append("\nany current test.");
		}
		Out.Append("\nIf you don't want to use the test server go to");
		Out.Append("\nthe network option and uncheck \"Test Server Login\".");
	} else if (ErrorType == 1) {
		Out.Append("Cannot connect to a login server.\n\nError: ");
		Out.Append(Text);
		if (Offline) {
			Out.Append("\n\nAll login server are offline. Check www.tibia.com");
			Out.Append("\nfor more information.");
		}
	} else {
		if (ErrorType != 2)
			TERROR1(597, "unknown error type", ErrorType);
		Out.Append("Cannot connect to the game server.\n\nError: ");
		Out.Append(Text);
		if (Offline) {
			Out.Append("\n\nThe game server is offline. Check www.tibia.com");
			Out.Append("\nfor more information.");
		}
	}
	Out.Append("\n\nFor more information take a look at the FAQs in the ");
	Out.Append("\nguide section at www.tibia.com.");
	TEND1(602, ErrorCode)
}

// 0x4cd9a0, lines 611-625. Starts an asynchronous lookup; NetworkWindow gets
// WM_USER+1 when it completes.
bool ResolveHostname(const char *Hostname)
{
	TASSERT(611, Hostname!=NULL);
	TBEGIN
	HostnameRequest = WSAAsyncGetHostByName(NetworkWindow, WM_USER + 1, Hostname,
		HostnameResolveBuffer, HostnameResolveBufferSize);
	if (HostnameRequest == nullptr) {
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), WSAGetLastError(), 1);
		AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Message));
		SetConnectionState(0);
		return false;
	}
	TimeoutCounter = 0;
	TimeoutLimit = 150;
	return true;
	TEND(625)
}

// 0x4cdca0, lines 634-640
const char *GetResolvedIPAddress()
{
	TBEGIN
	TASSERT(634, HostnameResolveBuffer!=NULL);
	return inet_ntoa(*reinterpret_cast<in_addr *>(reinterpret_cast<HOSTENT *>(HostnameResolveBuffer)->h_addr_list[0]));
	TEND(640)
}

// 0x4cee30, line 661. Returns 0 or a WinSock error code.
int CreateSocket()
{
	TBEGIN
	if (Socket != nullptr)
		Disconnect(false);
	Socket = new TSocket;
	if (!Socket->Create(0, SOCK_STREAM, FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE, nullptr)) {
		const int Error = WSAGetLastError();
		delete Socket;
		Socket = nullptr;
		SetConnectionState(0);
		return Error;
	}
	return 0;
	TEND(661)
}

// 0x4cdf20, lines 671-687. Returns 0 or a WinSock error code.
int ConnectSocket(const char *IPAddress, int Port, bool SetTimeout)
{
	TASSERT(671, IPAddress!=NULL);
	TBEGIN
	if (!Socket->Connect(IPAddress, Port)) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			const int Error = WSAGetLastError();
			delete Socket;
			Socket = nullptr;
			SetConnectionState(0);
			return Error;
		}
	}
	ReceiveRead = 0;
	ReceivePacketEnd = 0;
	ReceiveDataSize = 0;
	if (SetTimeout) {
		TimeoutLimit = 150;
		TimeoutCounter = 0;
	}
	return 0;
	TEND(687)
}

// 0x4ce210, line 698
void SetSymmetricKey(const uint8 *Key)
{
	try {
		SymmetricKey.SetKey(TXTEA::TKey(Key, 16));
	} catch (const char *) {
		TERROR(698, "Key initialization failed");
	}
}

// 0x4ce300, line 727
void Disconnect(bool force)
{
	TBEGIN
	if (force || GetConnectionState() == 1) {
		WSACancelAsyncRequest(HostnameRequest);
		HostnameRequest = nullptr;
	}
	if (Socket != nullptr) {
		shutdown(Socket->m_hSocket, SD_SEND);
		TFixedString<256> Buffer;
		int Count;
		do {
			Count = Socket->Receive(Buffer, sizeof(Buffer), 0);
			if (Count == 0)
				break;
		} while (Count != SOCKET_ERROR);
		shutdown(Socket->m_hSocket, SD_BOTH);
		Socket->Close();
		delete Socket;
		Socket = nullptr;
		TimeoutLimit = 0;
		TimeoutCounter = 0;
	}
	TEND1(727, (force?1:0))
}

// 0x4cf050, lines 739-773. Tries the current login server and moves on to the
// next one on failure; recursion is bounded by the number of servers.
void ConnectToLoginServer()
{
	RecursionProtection++;
	TBEGIN
	TASSERT1(739, RecursionProtection<=NUMBEROFSERVER+1, RecursionProtection);
	TServer *Server;
	if (!GetCurrentServer(&Server)) {
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), GetServerErrorCode(), 1);
		AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Message));
		for (int i = 0; i < NUMBEROFSERVER; i++)
			Servers[i].Available = true;
		TestServer.Available = true;
	} else if (!Server->Resolved) {
		SetConnectionState(1);
		ResolveHostname(Server->Address);
	} else {
		SetConnectionState(2);
		int Error = CreateSocket();
		if (Error == 0) {
			Error = ConnectSocket(Server->Address, Server->Port, true);
			if (Error != 0) {
				SetServerError(Error);
				ConnectToLoginServer();
			}
		} else {
			ResetServerAvailability();
		}
	}
	RecursionProtection--;
	TEND(773)
}

// 0x4cf390, line 802
void ConnectToGameServer()
{
	TBEGIN
	SetConnectionState(5);
	const int Character = GetActiveCharacter();
	const char *IPAddress = GetCharacterIPAddress(Character);
	GetCharacterPort(Character); // result unused in the binary
	int Error = CreateSocket();
	if (Error != 0) {
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), Error, 2);
		AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Message));
		AddMessage(10, 300, 0);
	}
	Error = ConnectSocket(IPAddress, GetCharacterPort(Character), false);
	if (Error != 0) {
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), Error, 2);
		AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Message));
		AddMessage(10, 300, 0);
	}
	TEND(802)
}

// 0x4cf5e0, line 835. vtable 0x56d388 +0x2c.
void TSocket::OnConnect(int ErrorCode)
{
	TBEGIN
	TimeoutLimit = 0;
	TimeoutCounter = 0;
	if (ErrorCode == 0) {
		ConnectCallback();
		for (int i = 0; i < NUMBEROFSERVER; i++)
			Servers[i].Available = true;
		TestServer.Available = true;
		return;
	}
	if (GetConnectionState() == 2) {
		// A login server refused: try the next one.
		TimeoutLimit = 0;
		TimeoutCounter = 0;
		SetConnectionState(0);
		SetServerError(ErrorCode);
		ConnectToLoginServer();
		return;
	}
	char *Message = new char[1024];
	GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), ErrorCode, 2);
	AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Message));
	AddMessage(10, 300, 0);
	Disconnect(false);
	SetConnectionState(0);
	TEND1(835, ErrorCode)
}

// 0x4ce530, line 885. vtable +0x1c. Appends received bytes and calls the
// receive handler once at least one complete packet is buffered.
void TSocket::OnReceive(int ErrorCode)
{
	TBEGIN
	if (ErrorCode != 0)
		return;
	const int Count = Receive(&ReceiveData[ReceiveDataSize], sizeof(ReceiveData) - ReceiveDataSize, 0);
	if (Count == 0 || Count == SOCKET_ERROR)
		return;
	ReceiveDataSize += Count;
	bool Complete = false;
	while (ReceivePacketEnd + 2 <= ReceiveDataSize) {
		TReadBuffer Header(patella::TSpan<uint8>(&ReceiveData[ReceivePacketEnd], 2));
		const int Length = Header.GetWord() + 2;
		if (ReceiveDataSize - ReceivePacketEnd < Length)
			break;
		Complete = true;
		ReceivePacketEnd += Length;
	}
	if (Complete)
		ReceiveCallback();
	TEND1(885, ErrorCode)
}

// 0x4ce750, line 897. vtable +0x30.
void TSocket::OnClose(int /*ErrorCode*/)
{
	TBEGIN
	Disconnect(false);
	CloseCallback();
	TEND(897)
}

// 0x4ce8f0
void ShutdownConnection()
{
	if (Socket != nullptr)
		shutdown(Socket->m_hSocket, SD_BOTH);
}

// 0x4ce930, lines 932-934
void InitNetwork(void (*ConnectHandler)(), void (*ReceiveHandler)(), void (*CloseHandler)())
{
	if (!AfxSocketInit(nullptr))
		throw PROGRAM_ERROR_SOCKET_INIT;
	TASSERT(932, ConnectHandler!=NULL);
	TASSERT(933, ReceiveHandler!=NULL);
	TASSERT(934, CloseHandler!=NULL);
	ReceiveCallback = ReceiveHandler;
	ConnectCallback = ConnectHandler;
	CloseCallback = CloseHandler;
	ReceiveRead = 0;
	ReceivePacketEnd = 0;
	ReceiveDataSize = 0;
	Socket = nullptr;
	HostnameResolveBuffer = nullptr;
	HostnameResolveBuffer = static_cast<char *>(malloc(HostnameResolveBufferSize));
	InitServerList();
}

// 0x4cec70, line 961
void ExitNetwork()
{
	TBEGIN
	delete Socket;
	Socket = nullptr;
	if (HostnameResolveBuffer != nullptr)
		free(HostnameResolveBuffer);
	HostnameResolveBuffer = nullptr;
	TEND(961)
}
