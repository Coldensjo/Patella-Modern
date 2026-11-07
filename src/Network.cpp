// Network.cpp: reconstructed from Tibia.exe 7.72 (0x4ca0e0-0x4cf982).
//
// Functions are in original source order, which comes from the assertion and
// catch line numbers; the comment before each gives its address and those
// lines. The functions without line evidence come first, as in the binary.
// Where the original's try block (its catch wrapper, common.h) began relative
// to the assertions followed the try-block states in the exception tables
// (tools/ehinfo.py). The try blocks and catch lines the comments name are the
// original's; since issue #78 the functions here have none apart from the
// encryption failures' handlers (SendCommand, PrepareNextPacket,
// SetSymmetricKey).
// Wire format, from SendCommand and PrepareNextPacket:
//   unencrypted: [u16 length][payload]
//   encrypted:   [u16 total][XTEA([u16 length][payload][random padding to 8])]

#include "Network.h"
#include "Utils.h"
#include "TXTEA.h"
#include "Preferences.h"
#include "common.h"
#include "log/Log.h"
#include "compat/span.h"
#include "platform/Socket.h"
#include "platform/Window.h"
#include "types/FixedString.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// The data is TNetwork's (Network.h, issue #105), in the original's order with its addresses. The socket is the
// platform's (issue #109: ISocket, platform/Socket.h; the original's CAsyncSocket class of this file is Win32's
// TSocket, platform/Win32Platform.h), and its events come to TNetwork's OnConnect, OnReceive and OnClose.

patella::TNetwork::TNetwork(TPreferences &PreferencesRef, TNetworkSession &SessionRef, platform::ISocket &SocketsRef)
	: Preferences(PreferencesRef), Session(SessionRef), Sockets(SocketsRef)
{
}

// 0x4ca0e0
void patella::TNetwork::InitServerList()
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
void patella::TNetwork::SetTestServerPort(int Port)
{
	TestServer.Port = Port;
}

// 0x4ca270
void patella::TNetwork::ResetServerAvailability()
{
	for (int i = 0; i < NUMBEROFSERVER; i++)
		Servers[i].Available = true;
	TestServer.Available = true;
}

// 0x4ca290
void patella::TNetwork::SetConnectionTimeout(int Timeout)
{
	TimeoutCounter = 0;
	TimeoutLimit = 0;
	if (Timeout > 0)
		TimeoutLimit = Timeout;
}

// 0x4ca2b0
void patella::TNetwork::SetNetworkWindow(platform::TWindowHandle Window)
{
	NetworkWindow = Window;
}

// 0x4ca450, lines 132-133
void patella::TNetwork::SetTestServerAddress(const char *Address)
{
	PATELLA_ASSERT(Address!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Address)<100, strlen(Address));
	TestServer.Address.Assign(Address);
}

// 0x4ca730, lines 152-164
void patella::TNetwork::SetServerError(int errorcode)
{
	PATELLA_ASSERT_VALUES(CurrentServer<NUMBEROFSERVER, CurrentServer);
	if (Preferences.GetTestServerLogin()) {
		TestServer.Available = false;
		TestServer.ErrorCode = errorcode;
		return;
	}
	Servers[CurrentServer].Available = false;
	Servers[CurrentServer].ErrorCode = errorcode;
}

// 0x4ca9f0, lines 182-184
void patella::TNetwork::SetServerIPAddress(const char *ipaddress)
{
	PATELLA_ASSERT_VALUES(CurrentServer<NUMBEROFSERVER, CurrentServer);
	PATELLA_ASSERT(ipaddress!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(ipaddress)<100, strlen(ipaddress));
	if (Preferences.GetTestServerLogin()) {
		TestServer.Address.Assign(ipaddress);
		TestServer.Resolved = true;
		return;
	}
	Servers[CurrentServer].Address.Assign(ipaddress);
	Servers[CurrentServer].Resolved = true;
}

// 0x4cadb0, lines 202-233. Stores the server to try next; false when none is left.
bool patella::TNetwork::GetCurrentServer(TServer **Server)
{
	PATELLA_ASSERT_VALUES(CurrentServer<NUMBEROFSERVER, CurrentServer);
	if (Preferences.GetTestServerLogin()) {
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
}

// 0x4cb0d0, line 255
int patella::TNetwork::GetServerErrorCode()
{
	if (Preferences.GetTestServerLogin())
		return TestServer.ErrorCode;
	for (int i = 0; i < NUMBEROFSERVER; i++) {
		if (Servers[i].ErrorCode == ERROR_LOGINSERVER_OVERLOADED)
			return ERROR_LOGINSERVER_OVERLOADED;
	}
	TServer *Server;
	GetCurrentServer(&Server);
	return Server->ErrorCode;
}

// 0x4cb2a0, lines 268-275. Reserves the 4 header bytes that SendCommand fills in.
void patella::TNetwork::BeginCommand(int CommandType)
{
	PATELLA_ASSERT_VALUES(In(CommandType,0,255), CommandType);
	SendBuffer.Position = 0;
	SendBuffer.PutWord(0);
	SendBuffer.PutWord(0);
	SendBuffer.PutByte((uint8)CommandType);
}

// 0x4cb540, line 286
void patella::TNetwork::SendByte(uint8 Data)
{
	SendBuffer.PutByte(Data);
}

// 0x4cb6e0, line 297
void patella::TNetwork::SendWord(uint16 Data)
{
	SendBuffer.PutWord(Data);
}

// 0x4cb880, line 308
void patella::TNetwork::SendQuad(uint32 Data)
{
	SendBuffer.PutQuad(Data);
}

// 0x4cba20, lines 316-320
void patella::TNetwork::SendString(const char *Text)
{
	PATELLA_ASSERT(Text!=nullptr);
	SendBuffer.PutString(Text);
}

// 0x4cbca0, line 328. Buffer is the span's pointer, the name of the assertion's text (issue #59).
void patella::TNetwork::SendBytes(patella::TSpan<const uint8> Bytes)
{
	const uint8 *const Buffer = Bytes.data();
	PATELLA_ASSERT(Buffer!=nullptr);
	SendBuffer.PutBytes(Bytes);
}

// 0x4cbe00, lines 355-367
void patella::TNetwork::SendCommand(bool Encrypt)
{
	if (Socket == nullptr)
		return;
	const int Size = SendBuffer.Position - 4;
	if (!Encrypt) {
		SendBuffer.Position = 0;
		SendBuffer.PutWord(0);
		SendBuffer.PutWord((uint16)Size);
		PATELLA_TEST_PACKET_SENT(SendData, Size, 0)
		Sockets.Send(Socket, &SendData[2], Size + 2);
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
		TERROR("Symmetric encryption failed with unknown exception");
	}
	Sockets.Send(Socket, SendData.data(), Length + 2);
}

// 0x4cc130, lines 391-410. Drops the packet already read, then decrypts the next
// complete packet in place and points ReceiveBuffer at its payload.
//
// Patella-Modern issues #72 and #139, deliberate differences (notes/decisions.md, "#139"): a frame the client cannot
// use is dropped (DropFrame), a warn line goes to the developer log, and the next complete frame is prepared in its
// place, so the connection goes on. Two such frames:
// - #139, item 7: a size that is no multiple of 8 (XTEA's block). Tibia.exe threw "packet size does not fit to
//   symmetric encryption" before it dropped the frame, so every later call threw the same; the frame is not decrypted.
// - #72: the payload, [u16 length][length bytes], must lie inside the frame, its PacketSize bytes after the size (a
//   length that runs into the XTEA padding is still inside it and read as before). A frame of size 0 (no length in
//   it) or one whose length runs past it is dropped. Tibia.exe read the length past an empty frame and set the read
//   buffer's size from it as it was, so its reads went on past the frame: into the next frames, and past ReceiveData
//   into SendData. Until the owner's change of November 7 (#139) such a frame threw "packet length exceeds the
//   received data" and ended the client.
// - #142: a packet of inner length 0 (nothing in it) is not a bad frame and is not dropped or logged, but nothing of
//   it is left to read, so the next frame is prepared in its place too, in the same call. Tibia.exe returned it as the
//   prepared packet: GetNextCommand returned -1 with complete frames behind it, and the empty packet's frame stayed in
//   ReceiveData until the next PrepareNextPacket, so a following frame that fits only without it never completed and
//   the connection stalled. EmptyPackets counts them (the tests').
void patella::TNetwork::PrepareNextPacket()
{
	while (true) {
		if (ReceiveRead > 0) {
			if (ReceiveRead < ReceiveDataSize)
				memmove(ReceiveData.data(), &ReceiveData[ReceiveRead], ReceiveDataSize - ReceiveRead);
			ReceiveDataSize -= ReceiveRead;
			ReceivePacketEnd -= ReceiveRead;
			ReceiveRead = 0;
		}
		if (ReceivePacketEnd <= 0)
			return;
		ReceiveBuffer.Size = 4;
		ReceiveBuffer.Position = 0;
		const int PacketSize = ReceiveBuffer.GetWord();
		const int FrameEnd = PacketSize + 2;
		if (PacketSize % 8 != 0) {
			patella::log::Warn("server frame dropped: its size {} is no multiple of 8", PacketSize);
			DropFrame(TFrameDrop::Size, FrameEnd);
			continue;
		}
		try {
			for (int i = 2; i < PacketSize + 2; i += 8)
				SymmetricKey.DecryptBlock(TXTEA::TBlock(&ReceiveData[i], 8));
		} catch (...) {
			TERROR("Symmetric decryption failed with unknown exception");
		}
		const int Size = PacketSize >= 2 ? ReceiveBuffer.GetWord() + 4 : 0;
		PATELLA_TEST_PACKET_RECEIVED(ReceiveData, PacketSize)
		memset(PacketHeader.data(), 0, sizeof(PacketHeader));
		ReceiveRead = FrameEnd;
		memcpy(PacketHeader.data(), &ReceiveData[2], PacketSize < 16 ? PacketSize : 16);
		if (PacketSize < 2 || Size > FrameEnd) {
			if (PacketSize < 2)
				patella::log::Warn("server frame dropped: it has no length (size {})", PacketSize);
			else
				patella::log::Warn("server frame dropped: its length {} runs past its size {}", Size - 4, PacketSize);
			DropFrame(TFrameDrop::Length, FrameEnd);
			continue;
		}
		if (Size == 4) {
			// Issue #142: an empty packet (inner length 0) has no command; the next frame is prepared in its place.
			EmptyPackets++;
			continue;
		}
		ReceiveBuffer.SetSize(Size);
		return;
	}
}

// Not the original's (issues #72 and #139): the frame up to FrameEnd is dropped (the next PrepareNextPacket moves the
// data after it down), the read buffer is empty, and the drop is counted. The caller has written the log line.
void patella::TNetwork::DropFrame(TFrameDrop Reason, int FrameEnd)
{
	ReceiveRead = FrameEnd;
	ReceiveBuffer.SetSize(ReceiveBuffer.Position);
	DroppedFrames++;
	LastDrop = Reason;
}

// 0x4cc550, line 423. Returns the next command byte, or -1 when no packet is complete.
int patella::TNetwork::GetNextCommand()
{
	if (ReceiveBuffer.Position == ReceiveBuffer.Size) {
		PrepareNextPacket();
		if (ReceiveBuffer.Position == ReceiveBuffer.Size)
			return -1;
	}
	return ReceiveBuffer.GetByte();
}

// 0x4cc720, line 434
uint8 patella::TNetwork::GetByte()
{
	return ReceiveBuffer.GetByte();
}

// 0x4cc8b0, line 445
uint16 patella::TNetwork::GetWord()
{
	return ReceiveBuffer.GetWord();
}

// 0x4cca40, line 456
uint32 patella::TNetwork::GetQuad()
{
	return ReceiveBuffer.GetQuad();
}

// 0x4ccbd0, lines 466-470. Buffer and MaxLength are the span's pointer and size, the names of the macros' text
// (issue #59; TReadBuffer::GetString); MaxLength only the original's catch wrapper named (issue #78).
void patella::TNetwork::GetString(patella::TSpan<char> Text)
{
	const char *const Buffer = Text.data();
	[[maybe_unused]] const int MaxLength = static_cast<int>(Text.size());
	PATELLA_ASSERT(Buffer!=nullptr);
	ReceiveBuffer.GetString(Text);
}

// 0x4cce50, line 482. The caller owns the returned string.
char *patella::TNetwork::GetString()
{
	return ReceiveBuffer.GetString();
}

// 0x4ccfe0, line 493
void patella::TNetwork::GetBytes(patella::TSpan<uint8> Bytes)
{
	ReceiveBuffer.GetBytes(Bytes);
}

// 0x4cf850, line 518. Called periodically; reconnects when the limit is passed.
void patella::TNetwork::CheckConnectionTimeout()
{
	if (TimeoutLimit != 0 && ++TimeoutCounter > TimeoutLimit) {
		TimeoutLimit = 0;
		TimeoutCounter = 0;
		SetServerError(platform::socket_error::TimedOut);
		Disconnect(false);
		Session.SetConnectionState(0);
		ConnectToLoginServer();
	}
}

// 0x4cd180, lines 544-602. ErrorType 1 is the login server, 2 the game server.
void patella::TNetwork::GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE> MessageBuffer, int ErrorCode, int ErrorType)
{
	char *const Message = MessageBuffer.data();
	PATELLA_ASSERT(Message!=nullptr);
	TFixedString<100> Text;
	bool Offline = false;
	Message[0] = 0;
	// Every caller passes new char[1024] (ERROR_MESSAGE_SIZE); the longest message has about 300 characters.
	TStringBuffer Out(Message, MessageBuffer.size());
	switch (ErrorCode) {
	case platform::socket_error::Invalid:
	case platform::socket_error::AddressNotAvailable:
		Text.Assign("Invalid server address.");
		break;
	case platform::socket_error::MessageSize:
		Text.Assign("Connection error.");
		break;
	case platform::socket_error::NetworkDown:
	case platform::socket_error::NetworkUnreachable:
		Text.Assign("Network is unreachable.");
		break;
	case platform::socket_error::TimedOut:
		Text.Assign("Connection timeout.");
		break;
	case platform::socket_error::ConnectionRefused:
		Text.Assign("Connection refused.");
		Offline = true;
		break;
	case platform::socket_error::HostUnreachable:
		Text.Assign("Server is unreachable.");
		break;
	case platform::socket_error::HostNotFound:
		Text.Assign("Server name does not exist.");
		break;
	case platform::socket_error::TryAgain:
		Text.Assign("Could not resolve server name.");
		break;
	case platform::socket_error::NoData:
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

	if (Preferences.GetTestServerLogin()) {
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
			TERROR1("unknown error type", ErrorType);
		Out.Append("Cannot connect to the game server.\n\nError: ");
		Out.Append(Text);
		if (Offline) {
			Out.Append("\n\nThe game server is offline. Check www.tibia.com");
			Out.Append("\nfor more information.");
		}
	}
	Out.Append("\n\nFor more information take a look at the FAQs in the ");
	Out.Append("\nguide section at www.tibia.com.");
}

// 0x4cd9a0, lines 611-625. Starts an asynchronous lookup; NetworkWindow gets
// HOSTNAME_RESOLVED_MESSAGE (WM_USER+1) when it completes.
bool patella::TNetwork::ResolveHostname(const char *Hostname)
{
	PATELLA_ASSERT(Hostname!=nullptr);
	HostnameRequest = Sockets.ResolveHostname(NetworkWindow, HOSTNAME_RESOLVED_MESSAGE, Hostname, HostnameResolveBuffer,
		HostnameResolveBufferSize);
	if (HostnameRequest == nullptr) {
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), Sockets.LastError(), 1);
		Session.AddMessage(1, reinterpret_cast<std::intptr_t>("Connection failed"), reinterpret_cast<std::intptr_t>(Message));
		Session.SetConnectionState(0);
		return false;
	}
	TimeoutCounter = 0;
	TimeoutLimit = 150;
	return true;
}

// 0x4cdca0, lines 634-640
const char *patella::TNetwork::GetResolvedIPAddress() const
{
	PATELLA_ASSERT(HostnameResolveBuffer!=nullptr);
	return Sockets.ResolvedAddress(HostnameResolveBuffer);
}

// 0x4cee30, line 661. Returns 0 or a WinSock error code.
int patella::TNetwork::CreateSocket()
{
	if (Socket != nullptr)
		Disconnect(false);
	Socket = Sockets.New(*this);
	if (!Sockets.Create(Socket)) {
		const int Error = Sockets.LastError();
		Sockets.Delete(Socket);
		Socket = nullptr;
		Session.SetConnectionState(0);
		return Error;
	}
	return 0;
}

// 0x4cdf20, lines 671-687. Returns 0 or a WinSock error code.
int patella::TNetwork::ConnectSocket(const char *IPAddress, int Port, bool SetTimeout)
{
	PATELLA_ASSERT(IPAddress!=nullptr);
	if (!Sockets.Connect(Socket, IPAddress, Port)) {
		if (Sockets.LastError() != platform::socket_error::WouldBlock) {
			const int Error = Sockets.LastError();
			Sockets.Delete(Socket);
			Socket = nullptr;
			Session.SetConnectionState(0);
			return Error;
		}
	}
	ReceiveRead = 0;
	ReceivePacketEnd = 0;
	ReceiveDataSize = 0;
	ReceiveSkip = 0;
	if (SetTimeout) {
		TimeoutLimit = 150;
		TimeoutCounter = 0;
	}
	return 0;
}

// 0x4ce210, line 698
void patella::TNetwork::SetSymmetricKey(const uint8 *Key)
{
	try {
		SymmetricKey.SetKey(TXTEA::TKey(Key, 16));
	} catch (const char *) {
		TERROR("Key initialization failed");
	}
}

// 0x4ce300, line 727
void patella::TNetwork::Disconnect(bool force)
{
	if (force || Session.GetConnectionState() == 1) {
		Sockets.CancelResolve(HostnameRequest);
		HostnameRequest = nullptr;
	}
	if (Socket != nullptr) {
		Sockets.Shutdown(Socket, platform::TShutdown::Send);
		TFixedString<256> Buffer;
		int Count;
		do {
			Count = Sockets.Receive(Socket, Buffer, sizeof(Buffer));
			if (Count == 0)
				break;
		} while (Count != platform::SocketFailed);
		Sockets.Shutdown(Socket, platform::TShutdown::Both);
		Sockets.Close(Socket);
		Sockets.Delete(Socket);
		Socket = nullptr;
		TimeoutLimit = 0;
		TimeoutCounter = 0;
	}
}

// 0x4cf050, lines 739-773. Tries the current login server and moves on to the
// next one on failure; recursion is bounded by the number of servers.
void patella::TNetwork::ConnectToLoginServer()
{
	RecursionProtection++;
	PATELLA_ASSERT_VALUES(RecursionProtection<=NUMBEROFSERVER+1, RecursionProtection);
	TServer *Server;
	if (!GetCurrentServer(&Server)) {
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), GetServerErrorCode(), 1);
		Session.AddMessage(1, reinterpret_cast<std::intptr_t>("Connection failed"), reinterpret_cast<std::intptr_t>(Message));
		for (int i = 0; i < NUMBEROFSERVER; i++)
			Servers[i].Available = true;
		TestServer.Available = true;
	} else if (!Server->Resolved) {
		Session.SetConnectionState(1);
		ResolveHostname(Server->Address);
	} else {
		Session.SetConnectionState(2);
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
}

// 0x4cf390, line 802
void patella::TNetwork::ConnectToGameServer()
{
	Session.SetConnectionState(5);
	const int Character = Session.GetActiveCharacter();
	const char *IPAddress = Session.GetCharacterIPAddress(Character);
	Session.GetCharacterPort(Character); // result unused in the binary
	int Error = CreateSocket();
	if (Error != 0) {
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), Error, 2);
		Session.AddMessage(1, reinterpret_cast<std::intptr_t>("Connection failed"), reinterpret_cast<std::intptr_t>(Message));
		Session.AddMessage(10, 300, 0);
	}
	Error = ConnectSocket(IPAddress, Session.GetCharacterPort(Character), false);
	if (Error != 0) {
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), Error, 2);
		Session.AddMessage(1, reinterpret_cast<std::intptr_t>("Connection failed"), reinterpret_cast<std::intptr_t>(Message));
		Session.AddMessage(10, 300, 0);
	}
}

// 0x4cf5e0, line 835. vtable 0x56d388 +0x2c.
void patella::TNetwork::OnConnect(int ErrorCode)
{
	TimeoutLimit = 0;
	TimeoutCounter = 0;
	if (ErrorCode == 0) {
		ConnectCallback();
		for (int i = 0; i < NUMBEROFSERVER; i++)
			Servers[i].Available = true;
		TestServer.Available = true;
		return;
	}
	if (Session.GetConnectionState() == 2) {
		// A login server refused: try the next one.
		TimeoutLimit = 0;
		TimeoutCounter = 0;
		Session.SetConnectionState(0);
		SetServerError(ErrorCode);
		ConnectToLoginServer();
		return;
	}
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	char *Message = new char[1024];
	GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), ErrorCode, 2);
	Session.AddMessage(1, reinterpret_cast<std::intptr_t>("Connection failed"), reinterpret_cast<std::intptr_t>(Message));
	Session.AddMessage(10, 300, 0);
	Disconnect(false);
	Session.SetConnectionState(0);
}

// 0x4ce530, line 885. vtable +0x1c. Appends received bytes and calls the
// receive handler once at least one complete packet is buffered.
void patella::TNetwork::OnReceive(platform::TSocket &Receiver, int ErrorCode)
{
	if (ErrorCode != 0)
		return;
	// The free part's start as a pointer: with ReceiveData full (frames the handler has not read yet) it is the
	// array's end, which the hardened library's operator[] refuses (issue #139; Receive then gets no room).
	const int Count =
		Sockets.Receive(&Receiver, ReceiveData.data() + ReceiveDataSize, sizeof(ReceiveData) - ReceiveDataSize);
	if (Count == 0 || Count == platform::SocketFailed)
		return;
	// Patella-Modern issue #139, item 6, a deliberate difference (notes/decisions.md, "#139"): a frame larger than
	// ReceiveData can never be complete there. Tibia.exe kept its first bytes and waited: ReceiveData filled up,
	// Receive got no room, and no later frame came through, forever. Now its bytes are dropped as they come
	// (ReceiveSkip: those still to come), a warn line goes to the developer log, and the frames after it are read.
	int Kept = Count;
	if (ReceiveSkip > 0) {
		const int Skipped = Count < ReceiveSkip ? Count : ReceiveSkip;
		ReceiveSkip -= Skipped;
		Kept = Count - Skipped;
		if (Kept > 0)
			memmove(&ReceiveData[ReceiveDataSize], &ReceiveData[ReceiveDataSize + Skipped], Kept);
	}
	ReceiveDataSize += Kept;
	bool Complete = false;
	while (ReceivePacketEnd + 2 <= ReceiveDataSize) {
		TReadBuffer Header(patella::TSpan<uint8>(&ReceiveData[ReceivePacketEnd], 2));
		const int Length = Header.GetWord() + 2;
		if (Length > static_cast<int>(ReceiveData.size())) {
			patella::log::Warn("server frame dropped: its size {} does not fit the receive buffer of {} bytes", Length - 2,
				ReceiveData.size());
			ReceiveSkip = Length - (ReceiveDataSize - ReceivePacketEnd);
			ReceiveDataSize = ReceivePacketEnd;
			DroppedFrames++;
			LastDrop = TFrameDrop::TooLarge;
			break;
		}
		if (ReceiveDataSize - ReceivePacketEnd < Length)
			break;
		Complete = true;
		ReceivePacketEnd += Length;
	}
	if (Complete)
		ReceiveCallback();
}

// 0x4ce750, line 897. vtable +0x30.
void patella::TNetwork::OnClose()
{
	Disconnect(false);
	CloseCallback();
}

// 0x4ce8f0
void patella::TNetwork::ShutdownConnection() const
{
	if (Socket != nullptr)
		Sockets.Shutdown(Socket, platform::TShutdown::Both);
}

// 0x4ce930, lines 932-934
void patella::TNetwork::InitNetwork(void (*ConnectHandler)(), void (*ReceiveHandler)(), void (*CloseHandler)())
{
	if (!Sockets.Startup())
		throw TProgramError{PROGRAM_ERROR::SOCKET_INIT};
	PATELLA_ASSERT(ConnectHandler!=nullptr);
	PATELLA_ASSERT(ReceiveHandler!=nullptr);
	PATELLA_ASSERT(CloseHandler!=nullptr);
	ReceiveCallback = ReceiveHandler;
	ConnectCallback = ConnectHandler;
	CloseCallback = CloseHandler;
	ReceiveRead = 0;
	ReceivePacketEnd = 0;
	ReceiveDataSize = 0;
	ReceiveSkip = 0;
	Socket = nullptr;
	HostnameResolveBuffer = nullptr;
	// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): the original's malloc, freed in ExitNetwork
	HostnameResolveBuffer = static_cast<char *>(malloc(HostnameResolveBufferSize));
	InitServerList();
}

// 0x4cec70, line 961
void patella::TNetwork::ExitNetwork()
{
	Sockets.Delete(Socket);
	Socket = nullptr;
	if (HostnameResolveBuffer != nullptr)
		// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): InitNetwork's malloc
		free(HostnameResolveBuffer);
	HostnameResolveBuffer = nullptr;
}

// The original's functions (issue #105): each calls its member on the one TClient's TNetwork.

void InitServerList()
{
	patella::bridge::Network().InitServerList();
}

void SetTestServerPort(int Port)
{
	patella::bridge::Network().SetTestServerPort(Port);
}

void ResetServerAvailability()
{
	patella::bridge::Network().ResetServerAvailability();
}

void SetConnectionTimeout(int Timeout)
{
	patella::bridge::Network().SetConnectionTimeout(Timeout);
}

void SetNetworkWindow(patella::platform::TWindowHandle Window)
{
	patella::bridge::Network().SetNetworkWindow(Window);
}

void SetTestServerAddress(const char *Address)
{
	patella::bridge::Network().SetTestServerAddress(Address);
}

void SetServerError(int errorcode)
{
	patella::bridge::Network().SetServerError(errorcode);
}

void SetServerIPAddress(const char *ipaddress)
{
	patella::bridge::Network().SetServerIPAddress(ipaddress);
}

bool GetCurrentServer(TServer **Server)
{
	return patella::bridge::Network().GetCurrentServer(Server);
}

int GetServerErrorCode()
{
	return patella::bridge::Network().GetServerErrorCode();
}

void BeginCommand(int CommandType)
{
	patella::bridge::Network().BeginCommand(CommandType);
}

void SendByte(uint8 Data)
{
	patella::bridge::Network().SendByte(Data);
}

void SendWord(uint16 Data)
{
	patella::bridge::Network().SendWord(Data);
}

void SendQuad(uint32 Data)
{
	patella::bridge::Network().SendQuad(Data);
}

void SendString(const char *Text)
{
	patella::bridge::Network().SendString(Text);
}

void SendBytes(patella::TSpan<const uint8> Bytes)
{
	patella::bridge::Network().SendBytes(Bytes);
}

void SendCommand(bool Encrypt)
{
	patella::bridge::Network().SendCommand(Encrypt);
}

void PrepareNextPacket()
{
	patella::bridge::Network().PrepareNextPacket();
}

int GetNextCommand()
{
	return patella::bridge::Network().GetNextCommand();
}

uint8 GetByte()
{
	return patella::bridge::Network().GetByte();
}

uint16 GetWord()
{
	return patella::bridge::Network().GetWord();
}

uint32 GetQuad()
{
	return patella::bridge::Network().GetQuad();
}

void GetString(patella::TSpan<char> Text)
{
	patella::bridge::Network().GetString(Text);
}

char *GetString()
{
	return patella::bridge::Network().GetString();
}

void GetBytes(patella::TSpan<uint8> Bytes)
{
	patella::bridge::Network().GetBytes(Bytes);
}

void CheckConnectionTimeout()
{
	patella::bridge::Network().CheckConnectionTimeout();
}

void GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE> MessageBuffer, int ErrorCode, int ErrorType)
{
	patella::bridge::Network().GetErrorMessage(MessageBuffer, ErrorCode, ErrorType);
}

bool ResolveHostname(const char *Hostname)
{
	return patella::bridge::Network().ResolveHostname(Hostname);
}

const char *GetResolvedIPAddress()
{
	return patella::bridge::Network().GetResolvedIPAddress();
}

int CreateSocket()
{
	return patella::bridge::Network().CreateSocket();
}

int ConnectSocket(const char *IPAddress, int Port, bool SetTimeout)
{
	return patella::bridge::Network().ConnectSocket(IPAddress, Port, SetTimeout);
}

void SetSymmetricKey(const uint8 *Key)
{
	patella::bridge::Network().SetSymmetricKey(Key);
}

void Disconnect(bool force)
{
	patella::bridge::Network().Disconnect(force);
}

void ConnectToLoginServer()
{
	patella::bridge::Network().ConnectToLoginServer();
}

void ConnectToGameServer()
{
	patella::bridge::Network().ConnectToGameServer();
}

void ShutdownConnection()
{
	patella::bridge::Network().ShutdownConnection();
}

void InitNetwork(void (*ConnectHandler)(), void (*ReceiveHandler)(), void (*CloseHandler)())
{
	patella::bridge::Network().InitNetwork(ConnectHandler, ReceiveHandler, CloseHandler);
}

void ExitNetwork()
{
	patella::bridge::Network().ExitNetwork();
}
