// Network.cpp against the original (0x4ca0e0-0x4cf982), issue #37.
//
// Nothing reaches Windows or ws2_32.dll. Network.cpp is compiled into this
// file (#include below, not linked on its own), so its statics are this
// file's and the test reads and sets them by name; the image's are at the
// addresses its source comments give. Every function the file calls outside
// itself is a fake on both sides, one function for both, which logs into the
// calling side's log (Side):
// - the WinSock functions it calls directly (WSAAsyncGetHostByName,
//   WSAGetLastError, WSACancelAsyncRequest, shutdown, inet_ntoa): the image
//   calls them through its import slots (by ordinal), which get the fakes'
//   addresses; ours through the __imp_ symbols, which are this file's NWSlot_
//   variables (/alternatename): difftest.exe does not link ws2_32;
// - MFC's CAsyncSocket (constructor, destructor, Create, Connect, Close, the
//   virtual Receive and Send) and AfxSocketInit: MFC is linked into both
//   clients (MFC 7.1 in the image, MFC 14 in Patella.exe), so the
//   comparison is at the level of the calls into MFC: the image's MFC
//   functions start with a jump to the fakes, ours (the shim's
//   declarations) are defined here and call the same fakes;
// - Status.cpp's connection state, AddMessage, the active character's
//   address and port, Preferences.cpp's GetTestServerLogin: both sides'
//   functions jump to the fakes (ours restored at the end);
// - rand(): the image's 0x511f33 and our CRT's, one MSVC generator per side
//   from the same seed before every operation;
// - the image's sprintf: tests/golden/script_tests.cpp's PatchScript.
// The three handlers InitNetwork takes are this file's functions: the
// connect handler sends a scripted command, the receive handler reads the
// packets with a scripted sequence of GetNextCommand, GetByte, GetWord,
// GetQuad, both GetStrings and GetBytes (never past the data both sides
// hold), the close handler logs.
//
// Each round starts both sides from the same cleared statics, calls
// InitNetwork and then random operations: every function of the file
// directly, the socket events (OnConnect, OnReceive, OnClose through each
// side's TSocket vtable), server data in random packets (valid XTEA frames
// with the current key, in random pieces: fragmented, coalesced, empty
// packets, inner lengths shorter and longer than the frame), connection
// failures at every step (Create, Connect, the host lookup, Receive
// returning 0 or an error) and timeouts. After each operation the logs
// (every fake call with its arguments, the bytes sent) and the whole state
// (every static of the file, both buffers byte for byte, the socket objects,
// PacketHeader) are compared.
//
// The error round (TErrorRound, issue #34's form) after the counted checks:
// every assertion with bad arguments, reads and writes past the buffers'
// ends, a frame size that is not a multiple of 8, an unknown error type, a
// failing AfxSocketInit, and failure injection: every operation again with
// the n-th fake call throwing an assertion of the calling side, for every n
// (sweeps), so every TEND of the file sees an exception from each of its
// callees.
//
// DIFFTEST_ONLY=Network runs it alone (NW_TRACE=1 traces the operations,
// NW_LOGS=<file> writes every compared log, NW_ROUNDS=<n>).
//
// A golden-corpus test (issue #30; tests/golden/golden.h, golden_eh.h): the
// oracle runs both sides as above and records the original's side of every
// comparison by value: its log, its state (the dump's text, formatted
// without pointers, golden::OutputRuns), both of its buffers (OutputRuns), in
// the error round what its call threw (ORIGINAL_THROWN) and how many fake
// calls it made. The runner runs our side alone, against the same fakes (our
// functions redirected, rand()'s import slot of the runner bound to its fake,
// the WinSock functions' __imp_ symbols this file's slots, the MFC shim's
// CAsyncSocket defined here; nothing reaches ws2_32 or a network), and
// compares with the recorded original. NW_ROUNDS changes the inputs: not with
// a corpus. On x64 our CAsyncSocket's m_hSocket is at +8, our TXTEA's key at
// +8 and our HOSTENT has 8-byte pointers: the fakes and the dump read and
// write ours where they are (the image's at +4 and in the x86 layout, as
// before).

// golden_test.h first (common.h: it sets _CRT_SECURE_NO_WARNINGS before any C runtime header).
#include "golden_test.h"

#include "golden_eh.h"
#include "golden_patch.h"
// Network.cpp is compiled into this file (its statics are in an unnamed namespace), at its own warning
// level (cmake/Warnings.cmake's baseline).
#ifdef _MSC_VER
#pragma warning(push, 3)
#endif
#include "../../src/Network.cpp"
// Status.cpp's functions the test fakes (Network.cpp reaches them through TNetworkSession since issue #105).
#include "../../src/Status.h"
// The socket class (issue #109: the platform's TSocket, whose vtable FireEvent calls; since issue #125 the runner's,
// network/MfcSocket.cpp, which is linked).
#include "network/MfcSocket.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <string>
#include <vector>
#include <stdarg.h>

#ifdef _MSC_VER
#pragma warning(disable : 4100)
#endif

static_assert(sizeof(TServer) == 0x70, "TServer");

namespace {

enum { IMAGE = 0,
	OURS = 1 };
// The sides this build has: both in the oracle, ours in the runner (the original's comes from the corpus).
const int FIRST_SIDE = OURS; // the oracle began with the original's side (IMAGE), until issue #94
int Side;
std::string Log[2];
bool Trace, Trace2;
FILE *LogFile;
const char *What = "";

void Logf(const char *Format, ...)
{
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
	if (Trace2)
		printf("%s%s\n", Side == IMAGE ? "  image: " : "  ours: ", Buffer);
}

// --- Patches ---------------------------------------------------------------------------------

struct TSaved {
	void *Address;
	size_t Size;
	uint8 Bytes[8];
};
std::vector<TSaved> Saved;
void Write(void *Address, const void *Data, size_t Size)
{
	TSaved S;
	S.Address = Address;
	S.Size = Size;
	memcpy(S.Bytes, Address, Size);
	DWORD Protection;
	VirtualProtect(Address, Size, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(Address, Data, Size);
	VirtualProtect(Address, Size, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Address, Size);
	Saved.push_back(S);
}
void Redirect(void *Function, void *Target)
{
	intptr_t Distance = (intptr_t)Target - ((intptr_t)Function + 5);
	if (Distance != (int32_t)Distance) { // the runner (x64): both in its image, within reach
		printf("FAIL Network: cannot redirect %p to %p: out of a rel32 jump's reach\n", Function, Target);
		exit(1);
	}
	uint8 Jump[5];
	Jump[0] = 0xe9;
	int32_t Offset = (int32_t)Distance;
	memcpy(Jump + 1, &Offset, 4);
	Write(Function, Jump, 5);
}
// The address of a member function (non-virtual, single inheritance; commworld.h's).
template <class T> void *MemberAddress(T Member)
{
	union {
		T m;
		void *p;
	} u;
	u.m = Member;
	return u.p;
}
void RestoreAll()
{
	for (size_t i = Saved.size(); i-- > 0;) {
		DWORD Protection;
		VirtualProtect(Saved[i].Address, Saved[i].Size, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(Saved[i].Address, Saved[i].Bytes, Saved[i].Size);
		VirtualProtect(Saved[i].Address, Saved[i].Size, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), Saved[i].Address, Saved[i].Size);
	}
	Saved.clear();
}


// --- The world the fakes answer from ---------------------------------------------------------

// What the fakes answer, decided before both sides run (the same for both).
// The k-th call of a kind within one operation takes entry k (the last one
// for later calls).
struct TScript {
	bool SocketInit;
	BOOL Create[6];
	int CreateError[6];
	BOOL Connect[6];
	int ConnectError[6];
	bool HostOk[6];
	int HostError[6];
	uint8 HostIP[6][4];
	int Chunk[8]; // Receive: >0 at most this many bytes, 0 closed, -1 an error
	int ReceiveError;
	int SendResult; // logged only: Network.cpp ignores Send's result
	int ShutdownResult;
	// The receive handler: GetNextCommand at most Packets times, after each
	// the reads Reads[(packet * 7 + i) % 32] for i = 0.. while they fit.
	int Packets;
	int Reads[32];
	int ReadValue[32]; // string limit or byte count
	int ReadsPerPacket;
	// The connect handler: a command to send (Command < 0: none).
	int Command;
	bool CommandEncrypt;
	int SendOps[24][2];
	int SendCount;
	uint32 RandSeed;
};
TScript Script;

struct TFakeSocket {
	void *Object;
	SOCKET Handle;
	bool Closed, Deleted;
};
struct TSideWorld {
	int LastError;
	int ConnectionState;
	bool TestServerLogin;
	int ActiveCharacter;
	char CharacterIP[40];
	int CharacterPort;
	uint32 Rand;
	std::vector<TFakeSocket> Sockets;
	SOCKET NextHandle;
	uint32 NextRequest;
	std::string Incoming; // server bytes not received yet
	size_t InPos;
	int Creates, Connects, Hosts, Receives;
	bool HostFilled; // the resolve buffer holds a HOSTENT
	int Ticks;
	char Address[6][20];
};
TSideWorld World[2];
int FailAt;  // 0: no failure injection
bool NoTick; // inside destructors (ours are noexcept)

void Tick()
{
	if (FailAt == 0 || NoTick)
		return;
	if (++World[Side].Ticks == FailAt) {
		Logf("throw;");
		ThrowSideAssertion(Side == IMAGE, -World[Side].Ticks);
	}
}

int SocketIndex(void *Object)
{
	for (size_t i = 0; i < World[Side].Sockets.size(); i++)
		if (World[Side].Sockets[i].Object == Object && !World[Side].Sockets[i].Deleted)
			return (int)i;
	return Object == NULL ? -1 : -2;
}
int SocketIndexOf(int S, void *Object)
{
	int SavedSide = Side;
	Side = S;
	int i = SocketIndex(Object);
	Side = SavedSide;
	return i;
}

// --- The views of both sides' statics ----------------------------------------------------------

struct TView {
	int *CurrentServer;
	TServer *TestServer;
	TServer *Servers;
	HANDLE *HostnameRequest;
	char **HostnameResolveBuffer;
	int *ReceiveDataSize, *ReceivePacketEnd, *ReceiveRead;
	uint8 *ReceiveData, *SendData;
	patella::platform::TWindowHandle *NetworkWindow;
	void (**CloseCallback)(), (**ReceiveCallback)(), (**ConnectCallback)();
	void **Socket;
	int *TimeoutCounter, *TimeoutLimit;
	TBuffer *SendBuffer, *ReceiveBuffer;
	uint8 *SymmetricKey; // the TXTEA object (vtable, then the key)
	int KeyOffset;       // the key's offset in it: after the vtable pointer (4 in the image, 8 in ours on x64)
	int *RecursionProtection, *HostnameResolveBufferSize;
	uint8 *PacketHeader;
};
TView View[2];

void MakeViews()
{
	TView &O = View[OURS];
	// Network.cpp's data: the members of the one TClient's TNetwork (issue #105).
	patella::TNetwork &N = patella::bridge::Network();
	O.CurrentServer = &N.CurrentServer;
	O.TestServer = &N.TestServer;
	O.Servers = N.Servers.data();
	O.HostnameRequest = &N.HostnameRequest;
	O.HostnameResolveBuffer = &N.HostnameResolveBuffer;
	O.ReceiveDataSize = &N.ReceiveDataSize;
	O.ReceivePacketEnd = &N.ReceivePacketEnd;
	O.ReceiveRead = &N.ReceiveRead;
	O.ReceiveData = N.ReceiveData.data();
	O.SendData = N.SendData.data();
	O.NetworkWindow = &N.NetworkWindow;
	O.CloseCallback = &N.CloseCallback;
	O.ReceiveCallback = &N.ReceiveCallback;
	O.ConnectCallback = &N.ConnectCallback;
	O.Socket = (void **)&N.Socket;
	O.TimeoutCounter = &N.TimeoutCounter;
	O.TimeoutLimit = &N.TimeoutLimit;
	O.SendBuffer = &N.SendBuffer;
	O.ReceiveBuffer = &N.ReceiveBuffer;
	O.SymmetricKey = (uint8 *)&N.SymmetricKey;
	O.KeyOffset = (int)sizeof(void *); // TXTEA: a vtable pointer, then uint32 Key[4]
	O.RecursionProtection = &N.RecursionProtection;
	O.HostnameResolveBufferSize = &N.HostnameResolveBufferSize;
	O.PacketHeader = N.PacketHeader.data();
}

// --- The fakes -------------------------------------------------------------------------------

int __cdecl FakeRand()
{
	Tick();
	World[Side].Rand = World[Side].Rand * 214013 + 2531011;
	return (int)((World[Side].Rand >> 16) & 0x7fff);
}

void __cdecl FakeSetConnectionState(int State)
{
	Logf("Conn%d;", State);
	Tick();
	World[Side].ConnectionState = State;
}
int __cdecl FakeGetConnectionState()
{
	Tick();
	return World[Side].ConnectionState;
}
bool __cdecl FakeGetTestServerLogin()
{
	Tick();
	return World[Side].TestServerLogin;
}
int __cdecl FakeGetActiveCharacter()
{
	Logf("Active;");
	Tick();
	return World[Side].ActiveCharacter;
}
const char *__cdecl FakeGetCharacterIPAddress(int Number)
{
	Logf("IP%d;", Number);
	Tick();
	return World[Side].CharacterIP;
}
int __cdecl FakeGetCharacterPort(int Number)
{
	Logf("Port%d;", Number);
	Tick();
	return World[Side].CharacterPort;
}
// The text belongs to the receiver (Status.cpp's message queue): freed here
// with the side's allocator (the image's operator new[] is malloc, PatchImage).
void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	if (Type == 1)
		Logf("Msg1(%s|%s);", (const char *)Param1, (const char *)Param2);
	else
		Logf("Msg%d(%d,%d);", Type, (int)Param1, (int)Param2);
	char *Text = Type == 1 ? (char *)Param2 : NULL;
	if (Text != NULL) {
		if (Side == IMAGE)
			free(Text);
		else
			delete[] Text;
	}
	Tick();
}

// WinSock.
HANDLE WINAPI FakeWSAAsyncGetHostByName(HWND Window, u_int Message, const char *Name, char *Buffer, int Length)
{
	TSideWorld &W = World[Side];
	char *OwnBuffer = *View[Side].HostnameResolveBuffer;
	Logf("Resolve(%x,%x,%s,%s,%d);", (unsigned)(uintptr_t)Window, Message, Name,
		Buffer == NULL ? "NULL" : Buffer == OwnBuffer ? "buffer"
													  : "other",
		Length);
	Tick();
	int k = W.Hosts < 5 ? W.Hosts : 5;
	W.Hosts++;
	if (!Script.HostOk[k]) {
		W.LastError = Script.HostError[k];
		return NULL;
	}
	if (Buffer != NULL && Length >= 0x50 && (Side == IMAGE || sizeof(void *) == 4)) {
		// A HOSTENT as Windows writes it, pointers into the buffer.
		memset(Buffer, 0, 0x50);
		*(char **)(Buffer + 0x00) = Buffer + 0x40;
		*(char **)(Buffer + 0x04) = Buffer + 0x20;
		*(short *)(Buffer + 0x08) = AF_INET;
		*(short *)(Buffer + 0x0a) = 4;
		*(char **)(Buffer + 0x0c) = Buffer + 0x10;
		*(char **)(Buffer + 0x10) = Buffer + 0x18;
		memcpy(Buffer + 0x18, Script.HostIP[k], 4);
		strcpy(Buffer + 0x40, "resolved");
		W.HostFilled = Buffer == OwnBuffer;
	} else if (Buffer != NULL && Length >= 0x50) {
		// Ours on x64 (the runner): the same HOSTENT with 8-byte pointers (0x20 bytes), the address list
		// at +0x20, the aliases' end at +0x30, the address at +0x38, the name at +0x40.
		memset(Buffer, 0, 0x50);
		HOSTENT *Host = (HOSTENT *)Buffer;
		char **Addresses = (char **)(Buffer + 0x20);
		Host->h_name = Buffer + 0x40;
		Host->h_aliases = (char **)(Buffer + 0x30);
		Host->h_addrtype = AF_INET;
		Host->h_length = 4;
		Host->h_addr_list = Addresses;
		Addresses[0] = Buffer + 0x38;
		memcpy(Buffer + 0x38, Script.HostIP[k], 4);
		strcpy(Buffer + 0x40, "resolved");
		W.HostFilled = Buffer == OwnBuffer;
	}
	return (HANDLE)(uintptr_t)(W.NextRequest++);
}
int WINAPI FakeWSAGetLastError()
{
	Logf("LastError;");
	Tick();
	return World[Side].LastError;
}
int WINAPI FakeWSACancelAsyncRequest(HANDLE Request)
{
	Logf("Cancel(%x);", (unsigned)(uintptr_t)Request);
	Tick();
	return 0;
}
int WINAPI FakeShutdown(SOCKET Handle, int How)
{
	Logf("shutdown(%x,%d);", (unsigned)Handle, How);
	Tick();
	return Script.ShutdownResult;
}
char *WINAPI FakeInetNtoa(in_addr Address)
{
	static char Text[2][32];
	Logf("ntoa;");
	Tick();
	snprintf(Text[Side], sizeof(Text[Side]), "%u.%u.%u.%u", Address.S_un.S_un_b.s_b1, Address.S_un.S_un_b.s_b2,
		Address.S_un.S_un_b.s_b3, Address.S_un.S_un_b.s_b4);
	return Text[Side];
}

// CAsyncSocket. The image's objects have the handle at +4 (MFC 7.1's
// m_hSocket after the vtable), ours at the shim's m_hSocket (+4 on x86, +8
// on x64).
SOCKET &Handle([[maybe_unused]] int S, void *Object)
{
	return static_cast<CAsyncSocket *>(Object)->m_hSocket;
}
SOCKET &Handle(void *Object) { return Handle(Side, Object); }

void SocketConstructed(void *Object)
{
	TFakeSocket S;
	S.Object = Object;
	S.Handle = INVALID_SOCKET;
	S.Closed = S.Deleted = false;
	World[Side].Sockets.push_back(S);
	Logf("new%d;", (int)World[Side].Sockets.size() - 1);
	Handle(Object) = INVALID_SOCKET;
}
void SocketClose(void *Object)
{
	Logf("Close%d(%x);", SocketIndex(Object), (unsigned)Handle(Object));
	Tick();
	int i = SocketIndex(Object);
	if (i >= 0)
		World[Side].Sockets[i].Closed = true;
	Handle(Object) = INVALID_SOCKET;
}
void SocketDestroyed(void *Object)
{
	Logf("delete%d;", SocketIndex(Object));
	bool SavedNoTick = NoTick;
	NoTick = true;
	if (Handle(Object) != INVALID_SOCKET)
		SocketClose(Object);
	NoTick = SavedNoTick;
	int i = SocketIndex(Object);
	if (i >= 0)
		World[Side].Sockets[i].Deleted = true;
}
BOOL SocketCreate(void *Object, UINT Port, int Type, long Events, LPCTSTR Address)
{
	TSideWorld &W = World[Side];
	Logf("Create%d(%u,%d,%lx,%s);", SocketIndex(Object), Port, Type, Events, Address == NULL ? "NULL" : Address);
	Tick();
	int k = W.Creates < 5 ? W.Creates : 5;
	W.Creates++;
	if (!Script.Create[k]) {
		W.LastError = Script.CreateError[k];
		return FALSE;
	}
	Handle(Object) = W.NextHandle++;
	W.Incoming.clear();
	W.InPos = 0;
	return TRUE;
}
BOOL SocketConnect(void *Object, LPCTSTR Address, UINT Port)
{
	TSideWorld &W = World[Side];
	Logf("Connect%d(%s,%u);", SocketIndex(Object), Address == NULL ? "NULL" : Address, Port);
	Tick();
	int k = W.Connects < 5 ? W.Connects : 5;
	W.Connects++;
	// A new connection: the server's data starts with a new frame.
	W.Incoming.clear();
	W.InPos = 0;
	if (!Script.Connect[k]) {
		W.LastError = Script.ConnectError[k];
		return FALSE;
	}
	return TRUE;
}
int SocketReceive(void *Object, void *Buffer, int Length, int Flags)
{
	TSideWorld &W = World[Side];
	uint8 *Data = View[Side].ReceiveData;
	char Where[24];
	if ((uint8 *)Buffer >= Data && (uint8 *)Buffer <= Data + 0x4000)
		snprintf(Where, sizeof(Where), "R+%d", (int)((uint8 *)Buffer - Data));
	else
		snprintf(Where, sizeof(Where), "other");
	Logf("Receive%d(%s,%d,%d)", SocketIndex(Object), Where, Length, Flags);
	Tick();
	int k = W.Receives < 7 ? W.Receives : 7;
	W.Receives++;
	int Chunk = W.Receives > 64 ? -2 : Script.Chunk[k];
	int Result;
	if (Chunk == 0) {
		Result = 0;
	} else if (Chunk == -1) {
		W.LastError = Script.ReceiveError;
		Result = SOCKET_ERROR;
	} else if (Chunk == -2 || W.InPos >= W.Incoming.size()) {
		W.LastError = WSAEWOULDBLOCK;
		Result = SOCKET_ERROR;
	} else {
		int Count = Chunk;
		if (Count > (int)(W.Incoming.size() - W.InPos))
			Count = (int)(W.Incoming.size() - W.InPos);
		if (Count > Length)
			Count = Length;
		if (Count > 0)
			memcpy(Buffer, W.Incoming.data() + W.InPos, Count);
		W.InPos += Count;
		Result = Count;
	}
	Logf("=%d;", Result);
	return Result;
}
int SocketSend(void *Object, const void *Buffer, int Length, int Flags)
{
	uint8 *Data = View[Side].SendData;
	Logf("Send%d(S+%d,%d,%d:", SocketIndex(Object), (int)((const uint8 *)Buffer - Data), Length, Flags);
	for (int i = 0; i < Length && i < 0x900; i++)
		Logf("%02x", ((const uint8 *)Buffer)[i]);
	Logf(");");
	Tick();
	return Script.SendResult;
}

BOOL SocketInit()
{
	Logf("AfxSocketInit;");
	Tick();
	return Script.SocketInit;
}

// The handlers InitNetwork gets.
void RunSendProgram();
void __cdecl ConnectHandler()
{
	Logf("Connected;");
	Tick();
	RunSendProgram();
}
void ReadPackets();
void __cdecl ReceiveHandler()
{
	Logf("Received;");
	Tick();
	ReadPackets();
}
void __cdecl CloseHandler()
{
	Logf("Closed;");
	Tick();
}

} // namespace

// Ours: the shim's MFC declarations, defined as the fakes.
CAsyncSocket::CAsyncSocket() { SocketConstructed(this); }
CAsyncSocket::~CAsyncSocket() { SocketDestroyed(this); }
BOOL CAsyncSocket::Create(UINT Port, int Type, long Events, LPCTSTR Address)
{
	return SocketCreate(this, Port, Type, Events, Address);
}
BOOL CAsyncSocket::Connect(LPCTSTR Address, UINT Port) { return SocketConnect(this, Address, Port); }
void CAsyncSocket::Close() { SocketClose(this); }
int CAsyncSocket::Receive(void *Buffer, int Length, int Flags) { return SocketReceive(this, Buffer, Length, Flags); }
int CAsyncSocket::Send(const void *Buffer, int Length, int Flags) { return SocketSend(this, Buffer, Length, Flags); }
void CAsyncSocket::OnReceive(int) {}
void CAsyncSocket::OnSend(int) {}
void CAsyncSocket::OnOutOfBandData(int) {}
void CAsyncSocket::OnAccept(int) {}
void CAsyncSocket::OnConnect(int) {}
void CAsyncSocket::OnClose(int) {}
BOOL AfxSocketInit(WSADATA *) { return SocketInit(); }

// Ours: the import slots of the WinSock functions (neither difftest.exe nor the runner links ws2_32).
extern "C" {
void *NWSlot_WSAAsyncGetHostByName = (void *)FakeWSAAsyncGetHostByName;
void *NWSlot_WSAGetLastError = (void *)FakeWSAGetLastError;
void *NWSlot_WSACancelAsyncRequest = (void *)FakeWSACancelAsyncRequest;
void *NWSlot_shutdown = (void *)FakeShutdown;
void *NWSlot_inet_ntoa = (void *)FakeInetNtoa;
}
#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:__imp__WSAAsyncGetHostByName@20=_NWSlot_WSAAsyncGetHostByName")
#pragma comment(linker, "/alternatename:__imp__WSAGetLastError@0=_NWSlot_WSAGetLastError")
#pragma comment(linker, "/alternatename:__imp__WSACancelAsyncRequest@4=_NWSlot_WSACancelAsyncRequest")
#pragma comment(linker, "/alternatename:__imp__shutdown@8=_NWSlot_shutdown")
#pragma comment(linker, "/alternatename:__imp__inet_ntoa@4=_NWSlot_inet_ntoa")
#elif defined(_MSC_VER)
// x64 (the runner): no decoration. On Linux (issue #91) linux/winsock2.h calls through the slots itself.
#pragma comment(linker, "/alternatename:__imp_WSAAsyncGetHostByName=NWSlot_WSAAsyncGetHostByName")
#pragma comment(linker, "/alternatename:__imp_WSAGetLastError=NWSlot_WSAGetLastError")
#pragma comment(linker, "/alternatename:__imp_WSACancelAsyncRequest=NWSlot_WSACancelAsyncRequest")
#pragma comment(linker, "/alternatename:__imp_shutdown=NWSlot_shutdown")
#pragma comment(linker, "/alternatename:__imp_inet_ntoa=NWSlot_inet_ntoa")
#endif

namespace {

// --- Both sides' functions ---------------------------------------------------------------------


// The functions under test, by side: ours and the image's (all cdecl).
struct TFunctions {
	void (*InitServerList)();
	void (*SetTestServerPort)(int);
	void (*ResetServerAvailability)();
	void (*SetConnectionTimeout)(int);
	void (*SetNetworkWindow)(patella::platform::TWindowHandle);
	void (*SetTestServerAddress)(const char *);
	void (*SetServerError)(int);
	void (*SetServerIPAddress)(const char *);
	bool (*GetCurrentServer)(TServer **);
	int (*GetServerErrorCode)();
	void (*BeginCommand)(int);
	void (*SendByte)(uint8);
	void (*SendWord)(uint16);
	void (*SendQuad)(uint32);
	void (*SendString)(const char *);
	void (*SendBytes)(const uint8 *, int);
	void (*SendCommand)(bool);
	void (*PrepareNextPacket)();
	int (*GetNextCommand)();
	uint8 (*GetByte)();
	uint16 (*GetWord)();
	uint32 (*GetQuad)();
	void (*GetStringBuffer)(char *, int);
	char *(*GetStringNew)();
	void (*GetBytes)(uint8 *, int);
	void (*CheckConnectionTimeout)();
	void (*GetErrorMessage)(char *, int, int);
	bool (*ResolveHostname)(const char *);
	const char *(*GetResolvedIPAddress)();
	int (*CreateSocket)();
	int (*ConnectSocket)(const char *, int, bool);
	void (*SetSymmetricKey)(const uint8 *);
	void (*Disconnect)(bool);
	void (*ConnectToLoginServer)();
	void (*ConnectToGameServer)();
	void (*ShutdownConnection)();
	void (*InitNetwork)(void (*)(), void (*)(), void (*)());
	void (*ExitNetwork)();
};
TFunctions Fn[2];

// Issues #72 and #139, deliberate differences (notes/decisions.md, "#72 and #86" and "#139"): our TNetwork drops a
// server frame the client cannot use, writes a warn line to the developer log and goes on with the next frame, where
// the original read on past the frame (#72: an inner length past it, a frame of size 0; PrepareNextPacket), threw at
// every later call (#139, item 7: a size that is no multiple of 8; none in the corpus), or kept the frame's first
// bytes and waited for the rest (#139, item 6: a frame larger than ReceiveData; OnReceive: the corpus's streams have
// such "frames" where their bytes are out of step with the frames). From that call on the sides' receive buffers
// differ (ours reads the next frame, the original past the frame or nothing more) until the next round's
// ResetSides, so that part of the round is a deliberate part (golden::BeginDeliberate): still run, its values read,
// its checks counted, a difference not a failure. It opens in our side's call that dropped a frame (Both: TNetwork's
// DroppedFrames before and after it), named by the drop's reason. Until #139 our side threw "packet length exceeds
// the received data" at #72's frames (the part opened at that throw) and had no other drop; the two sides never agree
// again within the round, so the part still ends at the round's end.
//
// Issue #142, the same way (notes/decisions.md, "#142"): our PrepareNextPacket goes on past an empty packet (inner
// length 0) to the next frame and moves the empty packet's frame out of ReceiveData at once, where the original
// returned the empty packet (GetNextCommand -1, the frame left in ReceiveData until the next PrepareNextPacket). From
// that call on the sides' receive buffers differ until ResetSides: a part named "#142", opened in our side's call that
// went on past one (TNetwork's EmptyPackets before and after it), unless a drop in the same call opened one first.
struct TFrameCounts {
	int Dropped;
	int Empty;
};
TFrameCounts FrameCounts()
{
	const patella::TNetwork &N = patella::bridge::Network();
	return {N.DroppedFrames, N.EmptyPackets};
}
void OpenDropPart(TFrameCounts Before)
{
	const patella::TNetwork &N = patella::bridge::Network();
	if (N.DroppedFrames != Before.Dropped)
		golden::BeginDeliberate(N.LastDrop == patella::TFrameDrop::Length ? "#72" : "#139");
	else if (N.EmptyPackets != Before.Empty)
		golden::BeginDeliberate("#142");
}

template <class T> void SetFn(T &Slot, uint32 Address) { Slot = (T)(uintptr_t)Address; }
void MakeFunctions()
{
	TFunctions &O = Fn[OURS];
	O.InitServerList = InitServerList;
	O.SetTestServerPort = SetTestServerPort;
	O.ResetServerAvailability = ResetServerAvailability;
	O.SetConnectionTimeout = SetConnectionTimeout;
	O.SetNetworkWindow = SetNetworkWindow;
	O.SetTestServerAddress = SetTestServerAddress;
	O.SetServerError = SetServerError;
	O.SetServerIPAddress = SetServerIPAddress;
	O.GetCurrentServer = GetCurrentServer;
	O.GetServerErrorCode = GetServerErrorCode;
	O.BeginCommand = BeginCommand;
	O.SendByte = SendByte;
	O.SendWord = SendWord;
	O.SendQuad = SendQuad;
	O.SendString = SendString;
	O.SendBytes = [](const uint8 *Buffer, int Count) { SendBytes(OriginalSpan(Buffer, Count)); };
	O.SendCommand = SendCommand;
	O.PrepareNextPacket = PrepareNextPacket;
	O.GetNextCommand = GetNextCommand;
	O.GetByte = GetByte;
	O.GetWord = GetWord;
	O.GetQuad = GetQuad;
	O.GetStringBuffer = [](char *Buffer, int MaxLength) { GetString(OriginalSpan(Buffer, MaxLength)); };
	O.GetStringNew = (char *(*)())GetString;
	O.GetBytes = [](uint8 *Buffer, int Count) { GetBytes(OriginalSpan(Buffer, Count)); };
	O.CheckConnectionTimeout = CheckConnectionTimeout;
	O.GetErrorMessage = [](char *Message, int Code, int Type) {
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), Code, Type);
	};
	O.ResolveHostname = ResolveHostname;
	O.GetResolvedIPAddress = GetResolvedIPAddress;
	O.CreateSocket = CreateSocket;
	O.ConnectSocket = ConnectSocket;
	O.SetSymmetricKey = SetSymmetricKey;
	O.Disconnect = Disconnect;
	O.ConnectToLoginServer = ConnectToLoginServer;
	O.ConnectToGameServer = ConnectToGameServer;
	O.ShutdownConnection = ShutdownConnection;
	O.InitNetwork = InitNetwork;
	O.ExitNetwork = ExitNetwork;
}

// A socket event through the side's own TSocket vtable (+0x1c OnReceive,
// +0x20 OnSend, +0x2c OnConnect, +0x30 OnClose in the image's 0x56d388).
void FireEvent(int Slot, int ErrorCode)
{
	void *Object = *View[Side].Socket;
	if (Object == NULL) { // only after a difference: reported, not a crash
		Logf("no socket;");
		return;
	}
	patella::platform::TSocket *S = (patella::platform::TSocket *)Object;
	switch (Slot) {
	case 0x1c: S->OnReceive(ErrorCode); break;
	case 0x20: S->OnSend(ErrorCode); break;
	case 0x2c: S->OnConnect(ErrorCode); break;
	case 0x30: S->OnClose(ErrorCode); break;
	}
}

// The receive handler's reads: never past min(Size, 0x4000), the data both
// sides hold (a frame's inner length can make Size larger than the frame).
void ReadPackets()
{
	TFunctions &F = Fn[Side];
	TBuffer &B = *View[Side].ReceiveBuffer;
	for (int p = 0; p < Script.Packets; p++) {
		int Command = F.GetNextCommand();
		Logf("cmd%d;", Command);
		if (Command == -1)
			break;
		for (int i = 0; i < Script.ReadsPerPacket; i++) {
			int r = (p * 7 + i) % 32;
			int Limit = B.Size < 0x4000 ? B.Size : 0x4000;
			int Left = Limit - B.Position;
			int Kind = Script.Reads[r], Value = Script.ReadValue[r];
			if (Kind == 0 && Left >= 1) {
				Logf("b%u;", F.GetByte());
			} else if (Kind == 1 && Left >= 2) {
				Logf("w%u;", F.GetWord());
			} else if (Kind == 2 && Left >= 4) {
				Logf("q%u;", F.GetQuad());
			} else if ((Kind == 3 || Kind == 4) && Left >= 2) {
				int Length = B.Data[B.Position] | B.Data[B.Position + 1] << 8;
				if (2 + Length > Left)
					break;
				if (Kind == 3) {
					static char Text[0x4010];
					memset(Text, 0x5a, Value > 0 ? Value + 1 : Length + 2);
					F.GetStringBuffer(Text, Value);
					Logf("s%d:%s;", Value, Text);
				} else {
					char *Text = F.GetStringNew();
					Logf("S:%s;", Text);
					if (Side == IMAGE)
						free(Text);
					else
						delete[] Text;
				}
			} else if (Kind == 5 && Value >= 0 && Left >= Value) {
				static uint8 Bytes[0x4000];
				F.GetBytes(Bytes, Value);
				Logf("B%d:", Value);
				for (int j = 0; j < Value; j++)
					Logf("%02x", Bytes[j]);
				Logf(";");
			} else {
				break;
			}
		}
	}
}

void RunSendProgram()
{
	if (Script.Command < 0)
		return;
	TFunctions &F = Fn[Side];
	F.BeginCommand(Script.Command);
	for (int i = 0; i < Script.SendCount; i++) {
		int Kind = Script.SendOps[i][0], Value = Script.SendOps[i][1];
		static const char *Texts[] = {"", "a", "hello world", "Patella Tester", "\xe4\xf6\xfc", "0123456789012345678901234567890123456789"};
		static const uint8 Bytes[] = {1, 2, 3, 0, 255, 128, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
		switch (Kind) {
		case 0: F.SendByte((uint8)Value); break;
		case 1: F.SendWord((uint16)Value); break;
		case 2: F.SendQuad((uint32)Value); break;
		case 3: F.SendString(Texts[(unsigned)Value % 6]); break;
		case 4: F.SendBytes(Bytes, (unsigned)Value % 21); break;
		}
	}
	F.SendCommand(Script.CommandEncrypt);
}

// --- Comparison ------------------------------------------------------------------------------

std::string Hex(const void *Data, int Size)
{
	std::string Text;
	char Buffer[4];
	for (int i = 0; i < Size; i++) {
		snprintf(Buffer, sizeof(Buffer), "%02x", ((const uint8 *)Data)[i]);
		Text += Buffer;
	}
	return Text;
}

// The handlers InitNetwork stores, by name (their addresses differ between the oracle and the runner).
const char *CallbackName(void (*Function)())
{
	if (Function == NULL)
		return "NULL";
	if (Function == ConnectHandler)
		return "connect";
	if (Function == ReceiveHandler)
		return "receive";
	if (Function == CloseHandler)
		return "close";
	return "other";
}

std::string Dump(int S)
{
	TView &V = View[S];
	char Buffer[512];
	std::string Text;
	snprintf(Buffer, sizeof(Buffer), "CurrentServer %d\nTestServer %s\n", *V.CurrentServer,
		Hex(V.TestServer, sizeof(TServer)).c_str());
	Text += Buffer;
	for (int i = 0; i < NUMBEROFSERVER; i++)
		Text += "Servers[" + std::to_string(i) + "] " + Hex(&V.Servers[i], sizeof(TServer)) + "\n";
	char *Resolve = *V.HostnameResolveBuffer;
	snprintf(Buffer, sizeof(Buffer),
		"HostnameRequest %x HostnameResolveBuffer %s ReceiveDataSize %d ReceivePacketEnd %d ReceiveRead %d\n"
		"NetworkWindow %x Callbacks %s %s %s Socket %d TimeoutCounter %d TimeoutLimit %d\n"
		"SendBuffer +%d %d %d ReceiveBuffer +%d %d %d Key %s RecursionProtection %d HostnameResolveBufferSize %d\n"
		"PacketHeader %s\n",
		(unsigned)(uintptr_t)*V.HostnameRequest, Resolve == NULL ? "NULL" : "set", *V.ReceiveDataSize,
		*V.ReceivePacketEnd, *V.ReceiveRead, (unsigned)(uintptr_t)*V.NetworkWindow, CallbackName(*V.CloseCallback),
		CallbackName(*V.ReceiveCallback), CallbackName(*V.ConnectCallback), SocketIndexOf(S, *V.Socket), *V.TimeoutCounter,
		*V.TimeoutLimit, (int)(V.SendBuffer->Data - V.SendData), V.SendBuffer->Size, V.SendBuffer->Position,
		(int)(V.ReceiveBuffer->Data - V.ReceiveData), V.ReceiveBuffer->Size, V.ReceiveBuffer->Position,
		Hex(V.SymmetricKey + V.KeyOffset, 16).c_str(), *V.RecursionProtection, *V.HostnameResolveBufferSize,
		Hex(V.PacketHeader, 16).c_str());
	Text += Buffer;
	TSideWorld &W = World[S];
	snprintf(Buffer, sizeof(Buffer), "world: state %d error %d received %d of %d, sockets", W.ConnectionState,
		W.LastError, (int)W.InPos, (int)W.Incoming.size());
	Text += Buffer;
	for (auto &K : W.Sockets) {
		snprintf(Buffer, sizeof(Buffer), " %x%s%s", (unsigned)(K.Deleted ? 0 : Handle(S, K.Object)), K.Closed ? "c" : "",
			K.Deleted ? "d" : "");
		Text += Buffer;
	}
	Text += "\n";
	return Text;
}

// The original's side as last recorded (golden::OutputRuns records its changes): its dump's text (in a
// buffer of a fixed size, zero after the text) and both of its buffers.
const size_t DUMP_SIZE = 4096;
char LastDump[DUMP_SIZE];
uint8 LastReceiveData[0x4000];
uint8 LastSendData[0x800];

std::string OriginalDump()
{
	golden::OutputRuns("state", LastDump, nullptr, DUMP_SIZE);
	return std::string(LastDump, strnlen(LastDump, DUMP_SIZE));
}

int Compared = 0;
void Compare(const char *Operation)
{
	Checks++;
	// The runner gets the original's log from the corpus.
	Log[IMAGE] = golden::OutputText("log", Log[IMAGE].c_str());
	if (golden::CheckFailed(Log[IMAGE] == Log[OURS])) {
		Failures++;
		if (Failures <= FailuresPrinted)
			printf("FAIL Network %s (%s): log\n%s\noriginal\n%s\n", Operation, What, Log[OURS].c_str(),
				Log[IMAGE].c_str());
	}
	if (LogFile != NULL)
		fprintf(LogFile, "%s: %s\n", Operation, Log[OURS].c_str());
	Log[IMAGE].clear();
	Log[OURS].clear();
	std::string A = Dump(OURS), B = OriginalDump();
	CHECK(A == B, "Network %s (%s): state\n%s\noriginal\n%s", Operation, What, A.c_str(), B.c_str());
	golden::OutputRuns("receive data", LastReceiveData, ORIGINAL_OR(View[IMAGE].ReceiveData, nullptr), 0x4000);
	golden::OutputRuns("send data", LastSendData, ORIGINAL_OR(View[IMAGE].SendData, nullptr), 0x800);
	int Size = 0;
	for (int i = 0; i < 0x4000; i++)
		if (View[OURS].ReceiveData[i] != LastReceiveData[i]) {
			Size = i + 1;
			break;
		}
	CHECK(Size == 0, "Network %s (%s): ReceiveData differs at %d", Operation, What, Size - 1);
	Size = 0;
	for (int i = 0; i < 0x800; i++)
		if (View[OURS].SendData[i] != LastSendData[i]) {
			Size = i + 1;
			break;
		}
	CHECK(Size == 0, "Network %s (%s): SendData differs at %d", Operation, What, Size - 1);
	Compared++;
}

// --- Rounds ----------------------------------------------------------------------------------

TErrorRound *Round; // set in the error round: both sides' calls are made through OurThrow/ImageThrow
uint8 TestKey[16];  // the key both sides have, for the server's frames

// Both sides from the same cleared statics (the image's buffers and key
// constructed once by TestNetwork).
void ResetSides()
{
	// A part of #72, #139 or #142 (above) ends: both sides from here on are in step again. ReceiveSkip is ours only (#139:
	// the rest of a frame larger than ReceiveData still to be dropped), cleared with the receive state below.
	golden::EndDeliberate();
	patella::bridge::Network().ReceiveSkip = 0;
	for (int s = FIRST_SIDE; s < 2; s++) {
		TView &V = View[s];
		*V.CurrentServer = 0;
		memset(V.TestServer, 0, sizeof(TServer));
		memset(V.Servers, 0, sizeof(TServer) * NUMBEROFSERVER);
		*V.HostnameRequest = NULL;
		if (*V.HostnameResolveBuffer != NULL)
			free(*V.HostnameResolveBuffer);
		*V.HostnameResolveBuffer = NULL;
		*V.ReceiveDataSize = *V.ReceivePacketEnd = *V.ReceiveRead = 0;
		memset(V.ReceiveData, 0, 0x4000);
		memset(V.SendData, 0, 0x800);
		*V.NetworkWindow = NULL;
		*V.CloseCallback = *V.ReceiveCallback = *V.ConnectCallback = NULL;
		if (*V.Socket != NULL) {
			if (s == IMAGE)
				free(*V.Socket);
			else
				::operator delete(*V.Socket);
		}
		*V.Socket = NULL;
		*V.TimeoutCounter = *V.TimeoutLimit = 0;
		V.SendBuffer->Data = V.SendData;
		V.SendBuffer->Size = 0x800;
		V.SendBuffer->Position = 0;
		V.ReceiveBuffer->Data = V.ReceiveData;
		V.ReceiveBuffer->Size = 0;
		V.ReceiveBuffer->Position = 0;
		memset(V.SymmetricKey + V.KeyOffset, 0, 16);
		*V.RecursionProtection = 0;
		*V.HostnameResolveBufferSize = MAXGETHOSTSTRUCT;
		memset(V.PacketHeader, 0, 16);
	}
	for (int s = 0; s < 2; s++) {
		TSideWorld &W = World[s];
		W.LastError = 0;
		W.ConnectionState = 0;
		W.Sockets.clear();
		W.NextHandle = 0x100;
		W.NextRequest = 0x5000;
		W.Incoming.clear();
		W.InPos = 0;
		W.HostFilled = false;
	}
	memset(TestKey, 0, sizeof(TestKey));
	Log[0].clear();
	Log[1].clear();
}

template <class F> void Both(const char *Name, F Call)
{
	What = Name;
	for (int s = 0; s < 2; s++) {
		TSideWorld &W = World[s];
		W.Rand = Script.RandSeed;
		W.Creates = W.Connects = W.Hosts = W.Receives = 0;
		W.Ticks = 0;
	}
	if (Trace)
		printf("Network: %s\n", Name);
	const TFrameCounts Dropped = FrameCounts();
	if (Round == NULL) {
		Side = OURS;
		try {
			Call(OURS);
		} catch (const TMyException &) {
			// Our side's throw inside a deliberate part of #72, #139 or #142 (after a dropped frame or an empty
			// packet our side reads other packets than the original); any other is the test's. Until #139 our side threw at the #72 frame itself.
			OpenDropPart(Dropped);
			if (!golden::InDeliberate())
				throw;
		}
		OpenDropPart(Dropped);
	} else {
		Round->Calling(Name, "ours");
		Side = OURS;
		TThrown Ours = OurThrow([&] { Call(OURS); });
		OpenDropPart(Dropped);
		Round->Calling(Name, "the original");
		TThrown Original = ORIGINAL_THROWN("thrown", Side = IMAGE; Call(IMAGE));
		Round->Compare(Name, Ours, Original);
		// How many fake calls the original made (whether its n-th was reached).
		golden::Output("original ticks", World[IMAGE].Ticks);
	}
	Compare(Name);
}

bool Chance(int Percent) { return (int)(Random() % 100) < Percent; }

std::string RandomAddress(int Length)
{
	static const char Chars[] = "abcdefghijklmnopqrstuvwxyz0123456789.-";
	std::string Text;
	for (int i = 0; i < Length; i++)
		Text += Chars[Random() % (sizeof(Chars) - 1)];
	return Text;
}
// The order of the calls of Random() in one expression is unspecified: each such expression of the old
// test is written as the calls in the order the oracle's compiler made them (issue #30: the runner's
// Debug build made another).
std::string RandomIP()
{
	char Text[32];
	uint32 D = Random() % 256, C = Random() % 256, B = Random() % 256, A = Random() % 256;
	snprintf(Text, sizeof(Text), "%u.%u.%u.%u", A, B, C, D);
	return Text;
}
int RandomErrorCode()
{
	static const int Codes[] = {WSAEINVAL, WSAEADDRNOTAVAIL, WSAEMSGSIZE, WSAENETDOWN, WSAENETUNREACH, WSAETIMEDOUT,
		WSAECONNREFUSED, WSAEHOSTUNREACH, WSAHOST_NOT_FOUND, WSATRY_AGAIN, WSANO_DATA, ERROR_LOGINSERVER_OVERLOADED,
		ERROR_DISCONNECTED, WSAEWOULDBLOCK, WSAECONNRESET, -1, 0, 1};
	if (Chance(10)) {
		int High = (int)(Random() << 8);
		int Low = (int)Random();
		return Low - High;
	}
	return Codes[Random() % (sizeof(Codes) / sizeof(Codes[0]))];
}

// What the fakes will answer in the next operation.
void RandomScript()
{
	TScript &S = Script;
	S.SocketInit = true;
	for (int k = 0; k < 6; k++) {
		S.Create[k] = Chance(85);
		S.CreateError[k] = RandomErrorCode();
		S.Connect[k] = Chance(60);
		S.ConnectError[k] = Chance(70) ? WSAEWOULDBLOCK : RandomErrorCode();
		S.HostOk[k] = Chance(80);
		S.HostError[k] = RandomErrorCode();
		for (int i = 0; i < 4; i++)
			S.HostIP[k][i] = (uint8)Random();
	}
	for (int k = 0; k < 8; k++) {
		int c = Random() % 10;
		S.Chunk[k] = c == 0 ? 0 : c == 1 ? -1
			: c < 5                      ? 1 + Random() % 16
			: c < 8                      ? 1 + Random() % 300
										 : 0x4000;
	}
	S.ReceiveError = RandomErrorCode();
	S.SendResult = Chance(80) ? 0 : SOCKET_ERROR;
	S.ShutdownResult = Chance(80) ? 0 : SOCKET_ERROR;
	S.Packets = Random() % 9;
	S.ReadsPerPacket = Random() % 12;
	for (int i = 0; i < 32; i++) {
		S.Reads[i] = Random() % 6;
		int v = Random() % 4;
		S.ReadValue[i] = v == 0 ? -1 : v == 1 ? 1 + Random() % 4
											  : 1 + Random() % 40;
	}
	S.Command = Chance(60) ? (int)(Random() % 256) : -1;
	S.CommandEncrypt = Chance(70);
	S.SendCount = Random() % 24;
	for (int i = 0; i < S.SendCount; i++) {
		S.SendOps[i][0] = Random() % 5;
		{
			uint32 Low = Random();
			uint32 High = Random() << 12;
			S.SendOps[i][1] = (int)(Low ^ High);
		}
	}
	{
		uint32 Low = Random();
		uint32 High = Random() << 16;
		S.RandSeed = Low ^ High;
	}
}

// One server frame: [u16 size][XTEA([u16 length][payload][padding])], the
// size a multiple of 8; Mode 1 an inner length below the payload's, 2 above
// it, 3 a frame of size 0.
std::string MakeFrame(int Mode)
{
	if (Mode == 3)
		return std::string(2, '\0');
	int Length = Random() % 48;
	std::string Plain(2, '\0');
	for (int i = 0; i < Length; i++) {
		int v = Random() % 6;
		Plain += (char)(v == 0 ? 0 : v == 1 ? 1 + Random() % 6
											: Random());
	}
	int Inner = Length;
	if (Mode == 1)
		Inner = Length > 0 ? (int)(Random() % Length) : 0;
	else if (Mode == 2)
		Inner = Length + 1 + (Chance(50) ? Random() % 16 : Random() % 0x10000);
	Inner &= 0xffff;
	Plain[0] = (char)Inner;
	Plain[1] = (char)(Inner >> 8);
	while (Plain.size() % 8 != 0)
		Plain += (char)Random();
	TXTEA Cipher;
	Cipher.SetKey(TestKey);
	for (size_t i = 0; i < Plain.size(); i += 8)
		Cipher.EncryptBlock(TXTEA::TBlock((uint8 *)&Plain[i], 8));
	std::string Frame(2, '\0');
	Frame[0] = (char)Plain.size();
	Frame[1] = (char)(Plain.size() >> 8);
	return Frame + Plain;
}
void AddServerData(int Frames)
{
	std::string Data;
	for (int i = 0; i < Frames; i++) {
		int m = Random() % 10;
		Data += MakeFrame(m < 6 ? 0 : m < 8 ? 1
				: m < 9                     ? 2
											: 3);
	}
	for (int s = 0; s < 2; s++)
		World[s].Incoming += Data;
}

// For the socket events and ConnectSocket: usually a socket is made first
// (a CreateSocket that succeeds), else the operation is left out.
bool OurSocket()
{
	if (patella::bridge::Network().Socket != NULL)
		return true;
	if (!Chance(70))
		return false;
	TScript Kept = Script;
	Script.Create[0] = TRUE;
	Both("CreateSocket", [&](int s) { Fn[s].CreateSocket(); });
	Script = Kept;
	return patella::bridge::Network().Socket != NULL;
}

// One random operation (inputs decided here, from Random() and our side's state).
enum { OPERATIONS = 30 };
[[maybe_unused]] const char *OperationName(int Kind)
{
	static const char *Names[OPERATIONS] = {"SetTestServerAddress", "SetTestServerPort", "SetServerIPAddress",
		"SetServerError", "ResetServerAvailability", "SetConnectionTimeout", "SetNetworkWindow", "GetCurrentServer",
		"GetServerErrorCode", "GetErrorMessage", "ResolveHostname", "GetResolvedIPAddress", "CreateSocket",
		"ConnectSocket", "SetSymmetricKey", "Disconnect", "ConnectToLoginServer", "ConnectToGameServer",
		"CheckConnectionTimeout", "ShutdownConnection", "OnConnect", "OnReceive", "OnClose", "send", "read",
		"OnSend", "ExitNetwork, InitNetwork", "InitServerList", "world", "PrepareNextPacket"};
	return Names[Kind];
}

void Operation(int Kind)
{
	RandomScript();
	switch (Kind) {
	case 0: {
		std::string Address = RandomAddress(Random() % 100);
		Both("SetTestServerAddress", [&](int s) { Fn[s].SetTestServerAddress(Address.c_str()); });
		break;
	}
	case 1: {
		int Port = Chance(80) ? (int)(Random() % 65536) : (int)Random() - 0x800000;
		Both("SetTestServerPort", [&](int s) { Fn[s].SetTestServerPort(Port); });
		break;
	}
	case 2: {
		std::string Address = Chance(70) ? RandomIP() : RandomAddress(Random() % 100);
		Both("SetServerIPAddress", [&](int s) { Fn[s].SetServerIPAddress(Address.c_str()); });
		break;
	}
	case 3: {
		int Code = RandomErrorCode();
		Both("SetServerError", [&](int s) { Fn[s].SetServerError(Code); });
		break;
	}
	case 4:
		Both("ResetServerAvailability", [&](int s) { Fn[s].ResetServerAvailability(); });
		break;
	case 5: {
		int Timeout = (int)(Random() % 300) - 50;
		Both("SetConnectionTimeout", [&](int s) { Fn[s].SetConnectionTimeout(Timeout); });
		break;
	}
	case 6: {
		HWND Window = (HWND)(uintptr_t)(Random() % 4 == 0 ? 0 : 0x10000 + Random() % 0x1000);
		Both("SetNetworkWindow", [&](int s) { Fn[s].SetNetworkWindow(Window); });
		break;
	}
	case 7:
		Both("GetCurrentServer", [&](int s) {
			TServer *Server = (TServer *)(uintptr_t)0x12345678;
			bool Result = Fn[s].GetCurrentServer(&Server);
			int Index = Server == View[s].TestServer ? 9 : (int)(Server - View[s].Servers);
			Logf("=%d,%d;", Result ? 1 : 0, Index);
		});
		break;
	case 8:
		Both("GetServerErrorCode", [&](int s) { Logf("=%d;", Fn[s].GetServerErrorCode()); });
		break;
	case 9: {
		int Code = RandomErrorCode(), Type = Chance(50) ? 1 : 2;
		Both("GetErrorMessage", [&](int s) {
			static char Message[1100];
			memset(Message, 0x5a, sizeof(Message));
			Message[sizeof(Message) - 1] = 0;
			Fn[s].GetErrorMessage(Message, Code, Type);
			Logf("=%s;", Message);
		});
		break;
	}
	case 10: {
		std::string Name = Chance(50) ? RandomAddress(1 + Random() % 30) : "server.tibia.com";
		Both("ResolveHostname", [&](int s) { Logf("=%d;", Fn[s].ResolveHostname(Name.c_str()) ? 1 : 0); });
		break;
	}
	case 11:
		if (!World[OURS].HostFilled || patella::bridge::Network().HostnameResolveBuffer == NULL)
			return;
		Both("GetResolvedIPAddress", [&](int s) {
			if (!World[s].HostFilled || *View[s].HostnameResolveBuffer == NULL)
				Logf("not filled;"); // only after a difference
			else
				Logf("=%s;", Fn[s].GetResolvedIPAddress());
		});
		break;
	case 12:
		Both("CreateSocket", [&](int s) { Logf("=%d;", Fn[s].CreateSocket()); });
		break;
	case 13: {
		if (!OurSocket())
			return;
		std::string Address = RandomIP();
		int Port = Random() % 65536;
		bool Timeout = Chance(50);
		Both("ConnectSocket", [&](int s) { Logf("=%d;", Fn[s].ConnectSocket(Address.c_str(), Port, Timeout)); });
		break;
	}
	case 14: {
		uint8 Key[16];
		for (int i = 0; i < 16; i++)
			Key[i] = (uint8)Random();
		Both("SetSymmetricKey", [&](int s) { Fn[s].SetSymmetricKey(Key); });
		memcpy(TestKey, Key, 16);
		break;
	}
	case 15: {
		bool Force = Chance(50);
		if (Chance(30))
			AddServerData(1 + Random() % 3);
		Both("Disconnect", [&](int s) { Fn[s].Disconnect(Force); });
		break;
	}
	case 16:
		Both("ConnectToLoginServer", [&](int s) { Fn[s].ConnectToLoginServer(); });
		break;
	case 17:
		Both("ConnectToGameServer", [&](int s) { Fn[s].ConnectToGameServer(); });
		break;
	case 18: {
		int Count = Chance(70) ? 1 + Random() % 5 : 140 + Random() % 20;
		Both("CheckConnectionTimeout", [&](int s) {
			for (int i = 0; i < Count; i++)
				Fn[s].CheckConnectionTimeout();
		});
		break;
	}
	case 19:
		Both("ShutdownConnection", [&](int s) { Fn[s].ShutdownConnection(); });
		break;
	case 20: {
		if (!OurSocket())
			return;
		int Error = Chance(60) ? 0 : RandomErrorCode();
		Both("OnConnect", [&](int) { FireEvent(0x2c, Error); });
		break;
	}
	case 21: {
		if (!OurSocket())
			return;
		AddServerData(Random() % 6);
		int Error = Chance(90) ? 0 : RandomErrorCode();
		Both("OnReceive", [&](int) { FireEvent(0x1c, Error); });
		break;
	}
	case 22: {
		if (!OurSocket())
			return;
		int Error = Chance(60) ? 0 : RandomErrorCode();
		Both("OnClose", [&](int) { FireEvent(0x30, Error); });
		break;
	}
	case 23:
		if (Script.Command < 0)
			Script.Command = Random() % 256;
		Both("send", [&](int) { RunSendProgram(); });
		break;
	case 24:
		Both("read", [&](int) { ReadPackets(); });
		break;
	case 25: {
		if (!OurSocket())
			return;
		int Error = RandomErrorCode();
		Both("OnSend", [&](int) { FireEvent(0x20, Error); });
		break;
	}
	case 26:
		Both("ExitNetwork, InitNetwork", [&](int s) {
			Fn[s].ExitNetwork();
			Fn[s].InitNetwork(ConnectHandler, ReceiveHandler, CloseHandler);
		});
		World[0].HostFilled = World[1].HostFilled = false;
		break;
	case 27:
		Both("InitServerList", [&](int s) { Fn[s].InitServerList(); });
		break;
	case 28: {
		// The world the fakes answer from (both sides alike).
		bool TestServerLogin = Chance(25);
		int State = Random() % 7, Character = Random() % 20, Port = Random() % 65536;
		bool SetState = Chance(50);
		std::string IP = RandomIP();
		for (int s = 0; s < 2; s++) {
			World[s].TestServerLogin = TestServerLogin;
			if (SetState)
				World[s].ConnectionState = State;
			World[s].ActiveCharacter = Character;
			strcpy(World[s].CharacterIP, IP.c_str());
			World[s].CharacterPort = Port;
		}
		break;
	}
	case 29:
		Both("PrepareNextPacket", [&](int s) { Fn[s].PrepareNextPacket(); });
		break;
	}
}

// The operations' weights: the connection and packet paths more often.
int RandomKind()
{
	static const int Weights[OPERATIONS] = {2, 2, 2, 3, 2, 2, 1, 3, 3, 4, 3, 2, 4, 4, 3, 4, 6, 4, 4, 2, 5, 24, 3, 6, 4,
		1, 1, 2, 3, 2};
	int Total = 0;
	for (int w : Weights)
		Total += w;
	int r = Random() % Total;
	for (int k = 0; k < OPERATIONS; k++) {
		if (r < Weights[k])
			return k;
		r -= Weights[k];
	}
	return 0;
}

void StartRound()
{
	ResetSides();
	World[0].TestServerLogin = World[1].TestServerLogin = Chance(15);
	World[0].ActiveCharacter = World[1].ActiveCharacter = Random() % 10;
	std::string IP = RandomIP();
	strcpy(World[0].CharacterIP, IP.c_str());
	strcpy(World[1].CharacterIP, IP.c_str());
	World[0].CharacterPort = World[1].CharacterPort = 7172;
	RandomScript();
	Both("InitNetwork", [&](int s) { Fn[s].InitNetwork(ConnectHandler, ReceiveHandler, CloseHandler); });
	Operation(14); // a key
	Operation(28);
}

void EndRound()
{
	RandomScript();
	Both("ExitNetwork", [&](int s) { Fn[s].ExitNetwork(); });
}

// --- The error round ---------------------------------------------------------------------------

// A call that may throw on both sides; the state is compared afterwards and
// the round's state is rebuilt by the caller.
template <class F> void Call(const char *Name, F Function)
{
	Both(Name, Function);
}

void TestNetworkErrors()
{
	ORIGINAL(VerifyOurSideAssertion("Network"));
	TErrorRound R("Network");
	Round = &R;
	// The assertions and errors with their arguments (each from a fresh round state).
	for (int t = 0; t < 40; t++) {
		StartRound();
		RandomScript();
		static const int Bad[] = {4, 5, 100, 0x7fffffff};
		int BadServer = Bad[t % 4];
		switch (t % 20) {
		case 0:
			Call("SetTestServerAddress(NULL)", [&](int s) { Fn[s].SetTestServerAddress(NULL); });
			break;
		case 1: {
			std::string Long = RandomAddress(100 + Random() % 60);
			Call("SetTestServerAddress(long)", [&](int s) { Fn[s].SetTestServerAddress(Long.c_str()); });
			break;
		}
		case 2:
			for (int s = FIRST_SIDE; s < 2; s++)
				*View[s].CurrentServer = BadServer;
			Call("SetServerError(CurrentServer)", [&](int s) { Fn[s].SetServerError(17); });
			break;
		case 3:
			for (int s = FIRST_SIDE; s < 2; s++)
				*View[s].CurrentServer = BadServer;
			Call("SetServerIPAddress(CurrentServer)", [&](int s) { Fn[s].SetServerIPAddress("1.2.3.4"); });
			break;
		case 4:
			Call("SetServerIPAddress(NULL)", [&](int s) { Fn[s].SetServerIPAddress(NULL); });
			break;
		case 5: {
			std::string Long = RandomAddress(100 + Random() % 60);
			Call("SetServerIPAddress(long)", [&](int s) { Fn[s].SetServerIPAddress(Long.c_str()); });
			break;
		}
		case 6:
			for (int s = FIRST_SIDE; s < 2; s++)
				*View[s].CurrentServer = BadServer;
			World[0].TestServerLogin = World[1].TestServerLogin = (t & 1) != 0;
			Call("GetCurrentServer(CurrentServer)", [&](int s) {
				TServer *Server;
				Fn[s].GetCurrentServer(&Server);
			});
			Call("GetServerErrorCode(CurrentServer)", [&](int s) { Fn[s].GetServerErrorCode(); });
			break;
		case 7: {
			static const int Commands[] = {-1, 256, (int)0x80000000, 0x7fffffff};
			int Command = Commands[t % 4];
			Call("BeginCommand", [&](int s) { Fn[s].BeginCommand(Command); });
			break;
		}
		case 8:
			Call("SendString(NULL)", [&](int s) { Fn[s].SendString(NULL); });
			Call("SendBytes(NULL)", [&](int s) { Fn[s].SendBytes(NULL, 3); });
			Call("GetString(NULL)", [&](int s) { Fn[s].GetStringBuffer(NULL, 10); });
			break;
		case 9: {
			int Type = t < 20 ? 0 : 3;
			World[0].TestServerLogin = World[1].TestServerLogin = false;
			Call("GetErrorMessage(type)", [&](int s) {
				static char Message[1100];
				Fn[s].GetErrorMessage(Message, WSAETIMEDOUT, Type);
			});
			Call("GetErrorMessage(NULL)", [&](int s) { Fn[s].GetErrorMessage(NULL, 0, 1); });
			break;
		}
		case 10:
			Call("ResolveHostname(NULL)", [&](int s) { Fn[s].ResolveHostname(NULL); });
			Call("ConnectSocket(NULL)", [&](int s) { Fn[s].ConnectSocket(NULL, 7171, true); });
			break;
		case 11:
			Call("ExitNetwork", [&](int s) { Fn[s].ExitNetwork(); });
			Call("GetResolvedIPAddress(no buffer)", [&](int s) { Fn[s].GetResolvedIPAddress(); });
			break;
		case 12:
			for (int s = FIRST_SIDE; s < 2; s++)
				*View[s].RecursionProtection = 5 + t / 20;
			Call("ConnectToLoginServer(recursion)", [&](int s) { Fn[s].ConnectToLoginServer(); });
			break;
		case 13: {
			int Which = t < 20 ? 0 : 1;
			Call("InitNetwork(NULL)", [&](int s) {
				Fn[s].InitNetwork(Which == 0 ? NULL : ConnectHandler, Which == 0 ? ReceiveHandler : NULL, CloseHandler);
			});
			Call("InitNetwork(NULL close)", [&](int s) { Fn[s].InitNetwork(ConnectHandler, ReceiveHandler, NULL); });
			Script.SocketInit = false;
			Call("InitNetwork(AfxSocketInit fails)", [&](int s) {
				Fn[s].InitNetwork(ConnectHandler, ReceiveHandler, CloseHandler);
			});
			break;
		}
		case 14: {
			// Writes past the send buffer's end.
			int Position = 0x800 - (int)(Random() % 4);
			for (int s = FIRST_SIDE; s < 2; s++)
				View[s].SendBuffer->Position = Position;
			Call("SendByte(full)", [&](int s) { Fn[s].SendByte(7); });
			Call("SendWord(full)", [&](int s) { Fn[s].SendWord(0x1234); });
			Call("SendQuad(full)", [&](int s) { Fn[s].SendQuad(0x12345678); });
			Call("SendString(full)", [&](int s) { Fn[s].SendString("hello"); });
			Call("SendBytes(full)", [&](int s) { Fn[s].SendBytes((const uint8 *)"abcdef", 6); });
			// BeginCommand's own writes (the header) need a buffer of 5 bytes: one
			// that is smaller (set by the test; the client's is 0x800) reaches its TEND.
			int Size = Random() % 5;
			for (int s = FIRST_SIDE; s < 2; s++)
				View[s].SendBuffer->Size = Size;
			Call("BeginCommand(small buffer)", [&](int s) { Fn[s].BeginCommand(0x0a); });
			break;
		}
		case 15: {
			// SendCommand's padding past the end (a socket is needed).
			Script.Create[0] = TRUE;
			Call("CreateSocket", [&](int s) { Fn[s].CreateSocket(); });
			int Position = 0x800 - 1 - (int)(Random() % 6);
			for (int s = FIRST_SIDE; s < 2; s++)
				View[s].SendBuffer->Position = Position;
			Call("SendCommand(full)", [&](int s) { Fn[s].SendCommand(true); });
			break;
		}
		case 16: {
			// Reads past the packet's end.
			int Size = Random() % 12, Position = Size - (int)(Random() % 3);
			if (Position < 0)
				Position = 0;
			for (int s = FIRST_SIDE; s < 2; s++) {
				View[s].ReceiveBuffer->Size = Size;
				View[s].ReceiveBuffer->Position = Position;
				memset(View[s].ReceiveData, 0x07, 64);
			}
			Call("GetByte(end)", [&](int s) { Fn[s].GetByte(); });
			Call("GetWord(end)", [&](int s) { Fn[s].GetWord(); });
			Call("GetQuad(end)", [&](int s) { Fn[s].GetQuad(); });
			Call("GetString(end)", [&](int s) {
				char Text[64];
				Fn[s].GetStringBuffer(Text, 64);
			});
			Call("GetString new(end)", [&](int s) {
				char *Text = Fn[s].GetStringNew();
				if (s == IMAGE)
					free(Text);
				else
					delete[] Text;
			});
			Call("GetBytes(end)", [&](int s) {
				uint8 Bytes[64];
				Fn[s].GetBytes(Bytes, 9);
			});
			break;
		}
		case 17:
		case 18: {
			// A frame size that is not a multiple of 8, through GetNextCommand and
			// directly, and through OnReceive's handler.
			int Size = 1 + (int)(Random() % 40);
			if (Size % 8 == 0)
				Size++;
			std::string Frame(2 + Size, '\x11');
			Frame[0] = (char)Size;
			Frame[1] = 0;
			if (t % 20 == 17) {
				for (int s = FIRST_SIDE; s < 2; s++) {
					memcpy(View[s].ReceiveData, Frame.data(), Frame.size());
					*View[s].ReceiveDataSize = *View[s].ReceivePacketEnd = (int)Frame.size();
				}
				Call("GetNextCommand(bad size)", [&](int s) { Fn[s].GetNextCommand(); });
				Call("PrepareNextPacket(bad size)", [&](int s) { Fn[s].PrepareNextPacket(); });
			} else {
				Script.Create[0] = TRUE;
				Call("CreateSocket", [&](int s) { Fn[s].CreateSocket(); });
				for (int s = 0; s < 2; s++)
					World[s].Incoming += Frame;
				Script.Packets = 2;
				Script.Chunk[0] = 0x4000;
				Call("OnReceive(bad size)", [&](int) { FireEvent(0x1c, 0); });
			}
			break;
		}
		case 19: {
			// A reader that runs past the end inside OnReceive's handler.
			Script.Create[0] = TRUE;
			Call("CreateSocket", [&](int s) { Fn[s].CreateSocket(); });
			{
				std::string Frame = MakeFrame(0); // reads stay inside ReceiveData
				World[0].Incoming += Frame;
				World[1].Incoming += Frame;
			}
			Script.Chunk[0] = 0x4000;
			Script.Packets = 1;
			Call("OnReceive(read past)", [&](int s) {
				int Command = Fn[s].GetNextCommand();
				(void)Command;
				for (int i = 0; i < 0x10010; i++)
					Fn[s].GetByte();
			});
			break;
		}
		}
	}
	// Failure injection: each operation with the n-th fake call throwing, for
	// every n, from a state built by the same random operations.
	for (int Kind = 0; Kind < OPERATIONS; Kind++) {
		if (Kind == 28)
			continue;
		for (int Trial = 0; Trial < 8; Trial++) {
			uint32 Seed;
			{
				uint32 Low = Random();
				uint32 High = Random() << 16;
				Seed = Low ^ High;
			}
			int Length = Random() % 12;
			for (int n = 1; n < 400; n++) {
				SetRandomSeed(Seed);
				golden::Input("error seed", Seed);
				FailAt = 0;
				StartRound();
				for (int i = 0; i < Length; i++)
					Operation(RandomKind());
				if (((Kind >= 20 && Kind <= 22) || Kind == 25 || Kind == 13) && patella::bridge::Network().Socket == NULL) {
					RandomScript();
					Script.Create[0] = TRUE;
					Both("CreateSocket", [&](int s) { Fn[s].CreateSocket(); });
				}
				if (Kind == 18) {
					// A limit that the operation's calls pass.
					int Limit = 1 + Random() % 3;
					RandomScript();
					Both("SetConnectionTimeout", [&](int s) { Fn[s].SetConnectionTimeout(Limit); });
				}
				FailAt = n;
				Operation(Kind);
				bool Threw = World[0].Ticks >= n || World[1].Ticks >= n;
				FailAt = 0;
				EndRound();
				if (!Threw)
					break;
			}
		}
	}
	FailAt = 0;
	Round = NULL;
	R.Finish();
}

} // namespace

void TestNetwork()
{
	// Random()'s state is put back at the end: the later tests (MapWindow, MainWindow) get
	// the inputs they had before this test existed.
	uint32 SavedSeed = GetRandomSeed();
	// Our side's dropped frames (#72, #139) write a warn line each: not the test's output.
	const patella::log::TLevel SavedLevel = patella::log::Log().GetLevel();
	patella::log::Log().SetLevel(patella::log::TLevel::Error);
	ORIGINAL(PatchImage());
	ORIGINAL(PatchScript()); // the image's sprintf
	memset(LastDump, 0, sizeof(LastDump));
	memset(LastReceiveData, 0, sizeof(LastReceiveData));
	memset(LastSendData, 0, sizeof(LastSendData));
	Trace = getenv("NW_TRACE") != NULL;
	Trace2 = Trace && atoi(getenv("NW_TRACE")) >= 2;
	const char *LogName = getenv("NW_LOGS");
	LogFile = LogName != NULL ? fopen(LogName, "w") : NULL;
	MakeViews();
	MakeFunctions();
	Side = OURS;
	// rand(): the runner's import slot of ucrtbase's (out of a rel32 jump's reach; Network.cpp calls it
	// through the slot).
	void **RandSlot = golden::FindImport("rand");
	if (RandSlot == nullptr) {
		printf("FAIL Network: no import slot for rand\n");
		Failures++;
		return;
	}
	void *SavedRand = *RandSlot;
	golden::SetImport(RandSlot, (void *)FakeRand);
	// Status.cpp's and Preferences.cpp's functions: one fake for both sides.
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		{0x4e71e0, (void *)SetConnectionState, (void *)FakeSetConnectionState},
		{0x4e71f0, (void *)GetConnectionState, (void *)FakeGetConnectionState},
		{0x4e9020, (void *)AddMessage, (void *)FakeAddMessage},
		{0x4e72c0, (void *)GetActiveCharacter, (void *)FakeGetActiveCharacter},
		{0x4ea090, (void *)GetCharacterIPAddress, (void *)FakeGetCharacterIPAddress},
		{0x4ea200, (void *)GetCharacterPort, (void *)FakeGetCharacterPort},
		{0x4db2a0, (void *)GetTestServerLogin, (void *)FakeGetTestServerLogin},
	};
	for (auto &T : Table) {
		ORIGINAL(Redirect((void *)(uintptr_t)T.Image, T.Fake));
		Redirect(T.Ours, T.Fake);
	}
	// Since issue #105 TNetwork asks its TPreferences for GetTestServerLogin: the member goes to the same fake (it
	// takes no argument but this, which the fake does not read).
	Redirect(MemberAddress(&patella::TPreferences::GetTestServerLogin), (void *)FakeGetTestServerLogin);

	int Before = Checks;
	int Rounds = getenv("NW_ROUNDS") != NULL ? atoi(getenv("NW_ROUNDS")) : 300;
	for (int r = 0; r < Rounds; r++) {
		if (Trace)
			printf("Network: round %d\n", r);
		golden::Input("round seed", GetRandomSeed());
		StartRound();
		int Count = 20 + Random() % 80;
		for (int i = 0; i < Count; i++)
			Operation(RandomKind());
		EndRound();
	}
	printf("Network: %d checks\n", Checks - Before);
	TestNetworkErrors();
	ResetSides();
	for (const golden::TDeliberate &Part : golden::Deliberates())
		if (Part.Issue == "#72" || Part.Issue == "#139")
			printf("Network: %lld checks in %lld parts after a frame dropped by %s (a deliberate difference), %lld of "
				   "them differ\n",
				Part.Checks, Part.Parts, Part.Issue.c_str(), Part.Differ);
		else if (Part.Issue == "#142")
			printf("Network: %lld checks in %lld parts after an empty packet read past by %s (a deliberate "
				   "difference), %lld of them differ\n",
				Part.Checks, Part.Parts, Part.Issue.c_str(), Part.Differ);
	RestoreAll();
	golden::SetImport(RandSlot, SavedRand);
	SetRandomSeed(SavedSeed);
	patella::log::Log().SetLevel(SavedLevel);
	if (LogFile != NULL)
		fclose(LogFile);
}

// Network.cpp's buffers for the communication tests (tests/golden/commworld.cpp, issue #30): Communication.cpp
// reads and builds packets in them. In the oracle communicationtest.exe links Network.cpp on its own (commworld.cpp
// finds them in its code); here Network.cpp is this file's. Reset: their state at the program's start (the
// Network test above leaves its last round's).
TBuffer *GoldenNetworkReceiveBuffer() { return &patella::bridge::Network().ReceiveBuffer; }
TBuffer *GoldenNetworkSendBuffer() { return &patella::bridge::Network().SendBuffer; }
void GoldenNetworkResetBuffers()
{
	patella::TNetwork &N = patella::bridge::Network();
	N.SendBuffer.Data = N.SendData.data();
	N.SendBuffer.Size = sizeof(N.SendData);
	N.SendBuffer.Position = 0;
	N.ReceiveBuffer.Data = N.ReceiveData.data();
	N.ReceiveBuffer.Size = 0;
	N.ReceiveBuffer.Position = 0;
}
