// Network.cpp: login server list, connecting, packet framing and XTEA.
// Function names are ours; parameter names come from the binary's assertion
// and catch strings.
//
// Patella-Modern issue #105 (M7, notes/decisions.md, "#104: TClient and the subsystem objects" and "#105"): the data
// is TNetwork's, which TClient owns (Client.h); the free functions of the original's names are the bridge that the
// callers use until they hold the object (bridge::Network(), defined in Client.cpp).

#pragma once

#include "common.h"
#include "TXTEA.h"
#include "Utils.h"
#include "compat/span.h"
#include "platform/Socket.h"
#include "platform/Window.h"
#include "types/FixedString.h"
#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr int NUMBEROFSERVER = 4;

// Error codes besides the WinSock ones.
inline constexpr int ERROR_LOGINSERVER_OVERLOADED = 200001;
inline constexpr int ERROR_DISCONNECTED = 200002;

// The message NetworkWindow gets when ResolveHostname's lookup is done (WM_USER + 1; MainFrm.cpp's OnHostResolved,
// which asserts it): a number, so that Network.cpp needs no Windows header (issue #110).
inline constexpr unsigned int HOSTNAME_RESOLVED_MESSAGE = 0x0400 + 1;

// The size of the buffer GetErrorMessage writes into: every caller passes new char[1024].
inline constexpr std::size_t ERROR_MESSAGE_SIZE = 1024;

struct TServer {
	TFixedString<100> Address; // +0x00
	int Port;                  // +0x64
	bool Available;            // +0x68, false after a failed attempt
	bool Resolved;             // +0x69, Address is a numeric IP address
	int ErrorCode;             // +0x6c, last error for this server
};
// The original's layout (no pointers: the same on x64), with Address a TFixedString (issue #57).
static_assert(IsFixedStringLayout<100>);
static_assert(sizeof(TServer) == 0x70 && alignof(TServer) == 4, "TServer is 0x70 bytes");
static_assert(offsetof(TServer, Address) == 0x00 && offsetof(TServer, Port) == 0x64
		&& offsetof(TServer, Available) == 0x68 && offsetof(TServer, Resolved) == 0x69
		&& offsetof(TServer, ErrorCode) == 0x6c,
	"TServer's fields");

namespace patella {

class TPreferences;

// What Network.cpp calls in the files above it (issue #105): Status.cpp's connection state and the active
// character's address (TCommunication's session), and the message queue (TGUI's). TNetwork reaches them only
// through this, so that it does not depend on those files; the functions are Status.h's of the same names. Until
// their objects implement it, Client.cpp's adapter calls Status.cpp's functions.
class TNetworkSession {
public:
	TNetworkSession() = default;
	TNetworkSession(const TNetworkSession &) = delete;
	TNetworkSession &operator=(const TNetworkSession &) = delete;
	TNetworkSession(TNetworkSession &&) = delete;
	TNetworkSession &operator=(TNetworkSession &&) = delete;
	virtual void SetConnectionState(int State) = 0;
	virtual int GetConnectionState() = 0;
	virtual int GetActiveCharacter() = 0;
	virtual const char *GetCharacterIPAddress(int Number) = 0;
	virtual int GetCharacterPort(int Number) = 0;
	virtual void AddMessage(int Type, std::intptr_t Param1, std::intptr_t Param2) = 0;

protected:
	~TNetworkSession() = default;
};

// Why TNetwork dropped a server frame (issues #72 and #139; notes/decisions.md, "#139"). The original had no such
// outcome: it read on past the frame (Length), ended the client at every later read (Size), or waited for the frame
// forever (TooLarge).
enum class TFrameDrop : unsigned char {
	None,
	Length,   // #72: a frame of size 0, or one whose inner length runs past it (PrepareNextPacket)
	Size,     // #139, item 7: a frame size that is no multiple of 8, XTEA's block (PrepareNextPacket)
	TooLarge, // #139, item 6: a frame larger than ReceiveData, which can never hold it (OnReceive)
};

// Network.cpp's data and functions (issue #105): the globals of the original (0x5c22c4, 0x7152f0-0x719d8b), with
// Control.cpp's ReconnectTimeout and Reconnect (0x5c4520-0x5c4524) and MainFrm.cpp's PacketHeader (0x5d1644), are
// the members, with the same initial values (the others 0, as the original's .bss), and the functions the members of
// the same names. The data is public: the golden Network test reads and sets every member (network_tests.cpp's
// views), the MainFrm and Control tests PacketHeader, Reconnect and ReconnectTimeout, Control.cpp the last two,
// MainFrm.cpp and Player.cpp PacketHeader.
//
// Issue #109: the socket and the name resolution are the platform's ISocket (platform/Socket.h), which TNetwork takes
// in its constructor and calls where it called WinSock and CAsyncSocket; the connection's events come to it as
// ISocketEvents (OnConnect, OnReceive, OnClose: the TSocket's of Network.cpp until then).
class TNetwork final : public platform::ISocketEvents {
public:
	TNetwork(TPreferences &PreferencesRef, TNetworkSession &SessionRef, platform::ISocket &SocketsRef);
	TNetwork(const TNetwork &) = delete;
	TNetwork &operator=(const TNetwork &) = delete;
	TNetwork(TNetwork &&) = delete;
	TNetwork &operator=(TNetwork &&) = delete;
	~TNetwork() = default;

	void InitServerList();
	void SetTestServerPort(int Port);
	void ResetServerAvailability();
	void SetConnectionTimeout(int Timeout);
	void SetNetworkWindow(platform::TWindowHandle Window);
	void SetTestServerAddress(const char *Address);
	void SetServerError(int errorcode);
	void SetServerIPAddress(const char *ipaddress);
	bool GetCurrentServer(TServer **Server);
	int GetServerErrorCode();

	void BeginCommand(int CommandType);
	void SendByte(uint8 Data);
	void SendWord(uint16 Data);
	void SendQuad(uint32 Data);
	void SendString(const char *Text);
	void SendBytes(TSpan<const uint8> Bytes);
	void SendCommand(bool Encrypt);

	void PrepareNextPacket();
	int GetNextCommand();
	uint8 GetByte();
	uint16 GetWord();
	uint32 GetQuad();
	void GetString(TSpan<char> Text);
	char *GetString();
	void GetBytes(TSpan<uint8> Bytes);

	void CheckConnectionTimeout();
	void GetErrorMessage(TSpan<char, ERROR_MESSAGE_SIZE> MessageBuffer, int ErrorCode, int ErrorType);
	bool ResolveHostname(const char *Hostname);
	[[nodiscard]] const char *GetResolvedIPAddress() const;
	int CreateSocket();
	int ConnectSocket(const char *IPAddress, int Port, bool SetTimeout);
	void SetSymmetricKey(const uint8 *Key);
	void Disconnect(bool force);
	void ConnectToLoginServer();
	void ConnectToGameServer();
	void ShutdownConnection() const;
	void InitNetwork(void (*ConnectHandler)(), void (*ReceiveHandler)(), void (*CloseHandler)());
	void ExitNetwork();

	// The socket's events: TSocket's OnConnect, OnReceive and OnClose (0x4cf5e0, 0x4ce530, 0x4ce750) call these.
	void OnConnect(int ErrorCode) override;
	void OnReceive(platform::TSocket &Receiver, int ErrorCode) override;
	void OnClose() override;

	// Data, in the original's .data/.bss order.
	int HostnameResolveBufferSize = platform::HostEntrySize; // 0x5c22c4, initialised to 0x400 (MAXGETHOSTSTRUCT)
	int ReconnectTimeout{};                                  // 0x5c4520, Control.cpp's: GetMilliseconds time
	bool Reconnect{};                                        // 0x5c4524, Control.cpp's: set by event 0x3f
	std::array<uint8, 16> PacketHeader{};                    // 0x5d1644, MainFrm.cpp's: the first bytes of the last packet
	int CurrentServer{};                                     // 0x7152f0
	TServer TestServer{};                                    // 0x7152f8
	std::array<TServer, NUMBEROFSERVER> Servers{};           // 0x715368
	void *HostnameRequest{};                                 // 0x715528, ResolveHostname's request (a HANDLE)
	char *HostnameResolveBuffer{};                           // 0x71552c, a HOSTENT filled by WSAAsyncGetHostByName
	int ReceiveDataSize{};                                   // 0x715530, bytes in ReceiveData
	int ReceivePacketEnd{};                                  // 0x715534, end of the complete packets
	int ReceiveRead{};                                       // 0x715538, start of the next unread packet
	// A frame whose inner length runs past it is dropped in PrepareNextPacket (issues #72 and #139), so
	// ReceiveBuffer's reads stay inside the frame: Tibia.exe read on past ReceiveData, into SendData.
	std::array<uint8, 0x4000> ReceiveData{};            // 0x715540
	std::array<uint8, 0x800> SendData{};                // 0x719540
	platform::TWindowHandle NetworkWindow{};            // 0x719d40, receives WM_USER+1 when a name is resolved
	void (*CloseCallback)(){};                          // 0x719d44
	void (*ReceiveCallback)(){};                        // 0x719d48
	void (*ConnectCallback)(){};                        // 0x719d4c
	platform::TSocket *Socket{};                        // 0x719d50, the platform's connection (issue #109)
	int TimeoutCounter{};                               // 0x719d54
	int TimeoutLimit{};                                 // 0x719d58, in CheckConnectionTimeout calls
	TWriteBuffer SendBuffer{SendData};                  // 0x719d5c, initialiser at 0x559fd0
	TReadBuffer ReceiveBuffer{{ReceiveData.data(), 0}}; // 0x719d68, initialiser at 0x559ff0
	TXTEA SymmetricKey;                                 // 0x719d74, initialiser at 0x55a010
	int RecursionProtection{};                          // 0x719d88

	// Not the original's (issues #72 and #139, deliberate differences; notes/decisions.md, "#139"): a server frame
	// the client cannot use is dropped, a warn line goes to the developer log, and the connection goes on with the
	// next frame. ReceiveSkip: the bytes still to come of a frame too large for ReceiveData (OnReceive reads and
	// drops them). DroppedFrames and LastDrop: how many frames were dropped and why the last one was (the tests').
	int ReceiveSkip{};
	int DroppedFrames{};
	TFrameDrop LastDrop{TFrameDrop::None};
	// Not the original's either (issue #142, a deliberate difference): the empty packets (inner length 0) that
	// PrepareNextPacket went on past to the next frame, where Tibia.exe stopped at them (the tests').
	int EmptyPackets{};

private:
	void DropFrame(TFrameDrop Reason, int FrameEnd);

	TPreferences &Preferences;
	TNetworkSession &Session;
	platform::ISocket &Sockets;

	// The arrays std::arrays (issue #60).
	static_assert(sizeof(Servers) == NUMBEROFSERVER * sizeof(TServer) && sizeof(ReceiveData) == 0x4000
			&& sizeof(SendData) == 0x800 && sizeof(PacketHeader) == 16,
		"Network.cpp's arrays");
};

// The transition's access path (issue #105; notes/decisions.md, "#104: TClient and the subsystem objects"): the
// network of the one TClient (Client.cpp). It goes when no free function needs it.
namespace bridge {
TNetwork &Network();
} // namespace bridge

} // namespace patella

// The original's functions (the callers' names, and the golden tests'): each calls TNetwork's member of the same
// name on bridge::Network().
void InitServerList();
void SetTestServerPort(int Port);
void ResetServerAvailability();
void SetConnectionTimeout(int Timeout);
void SetNetworkWindow(patella::platform::TWindowHandle Window);
void SetTestServerAddress(const char *Address);
void SetServerError(int errorcode);
void SetServerIPAddress(const char *ipaddress);
bool GetCurrentServer(TServer **Server);
int GetServerErrorCode();

void BeginCommand(int CommandType);
void SendByte(uint8 Data);
void SendWord(uint16 Data);
void SendQuad(uint32 Data);
void SendString(const char *Text);
void SendBytes(patella::TSpan<const uint8> Bytes);
void SendCommand(bool Encrypt);

void PrepareNextPacket();
int GetNextCommand();
uint8 GetByte();
uint16 GetWord();
uint32 GetQuad();
void GetString(patella::TSpan<char> Text); // at most Text.size() - 1 characters and the NUL
char *GetString();
void GetBytes(patella::TSpan<uint8> Bytes);

void CheckConnectionTimeout();
// Message is the caller's new char[ERROR_MESSAGE_SIZE]: a fixed extent, passed as the pointer was (issue #69).
void GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE> MessageBuffer, int ErrorCode, int ErrorType);
static_assert(patella::IsSpanLayout<char, ERROR_MESSAGE_SIZE>, "a pointer's size: the golden tests' fakes take char *");
bool ResolveHostname(const char *Hostname);
const char *GetResolvedIPAddress();
int CreateSocket();
int ConnectSocket(const char *IPAddress, int Port, bool SetTimeout);
void SetSymmetricKey(const uint8 *Key);
void Disconnect(bool force);
void ConnectToLoginServer();
void ConnectToGameServer();
void ShutdownConnection();
void InitNetwork(void (*ConnectHandler)(), void (*ReceiveHandler)(), void (*CloseHandler)());
void ExitNetwork();
