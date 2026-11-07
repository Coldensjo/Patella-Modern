// Socket.h: ISocket, the platform's network (Patella-Modern issue #109, plan M7; notes/decisions.md, "#109: the
// platform interfaces"). Not part of the original source. Portable: no platform header.
//
// TNetwork (Network.cpp) has one connection at a time, to the login server or the game server, and resolves the
// login servers' names. What it asked of WinSock and of MFC's CAsyncSocket it asks of the process's ISocket
// (platform/Platform.h), call for call: the same calls, in the same order, with the same arguments, so the golden
// Network test's fakes of WinSock and CAsyncSocket see what they saw.
//
//	TPortableSocket   Winsock or POSIX sockets, non-blocking, polled from the loop, the name lookup on a std::jthread
//	                  (platform/PortableSocket.h, issue #125): Patella.exe's, and PatellaTest.exe's live runs'
//	TPipeSocketWin32  the test build's server replay inside the process (testmode/PipeSocketWin32.h, issue #120)
//	TMfcSocket        MFC's CAsyncSocket and WinSock, the original's calls (tests/golden/network/MfcSocket.h): the
//	                  golden runner's, whose Network test fakes those calls (Patella.exe's too until issue #125)
//
// A connection is a TSocket, which each executable defines once (PortableSocket.h's in Patella.exe and PatellaTest.exe,
// a CAsyncSocket in the golden runner); TNetwork holds it as the original held its CAsyncSocket, and the golden Network
// test reaches its events through it. Its events go to the ISocketEvents it was made for (TNetwork).

#pragma once

namespace patella::platform {

// The numbers the interface carries (issue #110): WinSock's, which every implementation reports and TNetwork's error
// messages name (GetErrorMessage's table; the protocol's numbers here, whatever the system's own are). PortableSocket.cpp
// asserts on Windows that they are WinSock's, and gives them for POSIX's errno and getaddrinfo's codes (issue #125).
namespace socket_error {
inline constexpr int Interrupted = 10004;         // WSAEINTR
inline constexpr int AccessDenied = 10013;        // WSAEACCES
inline constexpr int Fault = 10014;               // WSAEFAULT
inline constexpr int Invalid = 10022;             // WSAEINVAL
inline constexpr int TooManySockets = 10024;      // WSAEMFILE
inline constexpr int WouldBlock = 10035;          // WSAEWOULDBLOCK: Connect's while it connects
inline constexpr int Already = 10037;             // WSAEALREADY
inline constexpr int NotSocket = 10038;           // WSAENOTSOCK
inline constexpr int MessageSize = 10040;         // WSAEMSGSIZE
inline constexpr int FamilyNotSupported = 10047;  // WSAEAFNOSUPPORT
inline constexpr int AddressInUse = 10048;        // WSAEADDRINUSE
inline constexpr int AddressNotAvailable = 10049; // WSAEADDRNOTAVAIL
inline constexpr int NetworkDown = 10050;         // WSAENETDOWN
inline constexpr int NetworkUnreachable = 10051;  // WSAENETUNREACH
inline constexpr int NetworkReset = 10052;        // WSAENETRESET
inline constexpr int ConnectionAborted = 10053;   // WSAECONNABORTED
inline constexpr int ConnectionReset = 10054;     // WSAECONNRESET
inline constexpr int NoBuffers = 10055;           // WSAENOBUFS
inline constexpr int AlreadyConnected = 10056;    // WSAEISCONN
inline constexpr int NotConnected = 10057;        // WSAENOTCONN
inline constexpr int Shutdown = 10058;            // WSAESHUTDOWN
inline constexpr int TimedOut = 10060;            // WSAETIMEDOUT
inline constexpr int ConnectionRefused = 10061;   // WSAECONNREFUSED
inline constexpr int HostUnreachable = 10065;     // WSAEHOSTUNREACH
inline constexpr int HostNotFound = 11001;        // WSAHOST_NOT_FOUND
inline constexpr int TryAgain = 11002;            // WSATRY_AGAIN
inline constexpr int NoRecovery = 11003;          // WSANO_RECOVERY
inline constexpr int NoData = 11004;              // WSANO_DATA
} // namespace socket_error

// Send's and Receive's failure: SOCKET_ERROR.
inline constexpr int SocketFailed = -1;
// The size of ResolveHostname's buffer for the host's entry: MAXGETHOSTSTRUCT (TPortableSocket writes the address's
// text there).
inline constexpr int HostEntrySize = 1024;

// What shutdown stops: SD_SEND, SD_BOTH.
enum class TShutdown : unsigned char { Send,
	Both };

class TSocket;

// The events of a connection, as CAsyncSocket's OnConnect, OnReceive and OnClose (TNetwork's).
class ISocketEvents {
public:
	ISocketEvents() = default;
	ISocketEvents(const ISocketEvents &) = delete;
	ISocketEvents &operator=(const ISocketEvents &) = delete;
	ISocketEvents(ISocketEvents &&) = delete;
	ISocketEvents &operator=(ISocketEvents &&) = delete;

	virtual void OnConnect(int ErrorCode) = 0;
	virtual void OnReceive(TSocket &Receiver, int ErrorCode) = 0;
	virtual void OnClose() = 0;

protected:
	~ISocketEvents() = default;
};

class ISocket {
public:
	ISocket() = default;
	ISocket(const ISocket &) = delete;
	ISocket &operator=(const ISocket &) = delete;
	ISocket(ISocket &&) = delete;
	ISocket &operator=(ISocket &&) = delete;

	// AfxSocketInit: false where the sockets cannot be used (InitNetwork's PROGRAM_ERROR 6).
	[[nodiscard]] virtual bool Startup() = 0;
	// WSAGetLastError: the last call's error code.
	[[nodiscard]] virtual int LastError() = 0;

	// new TSocket: a connection whose events go to Events, not created yet.
	[[nodiscard]] virtual TSocket *New(ISocketEvents &Events) = 0;
	// delete: the connection (nullptr: nothing), closed or not.
	virtual void Delete(TSocket *Connection) = 0;
	// CAsyncSocket::Create: a stream socket, non-blocking, with every event.
	[[nodiscard]] virtual bool Create(TSocket *Connection) = 0;
	// CAsyncSocket::Connect: false with LastError socket_error::WouldBlock while it connects (OnConnect follows).
	[[nodiscard]] virtual bool Connect(TSocket *Connection, const char *Address, int Port) = 0;
	// CAsyncSocket::Send and Receive (flags 0): the bytes sent or read, 0 at the end, SocketFailed (SOCKET_ERROR).
	virtual int Send(TSocket *Connection, const void *Data, int Length) = 0;
	virtual int Receive(TSocket *Connection, void *Buffer, int Length) = 0;
	// shutdown of the connection's socket.
	virtual void Shutdown(TSocket *Connection, TShutdown How) = 0;
	// CAsyncSocket::Close: no event of the connection after it.
	virtual void Close(TSocket *Connection) = 0;

	// WSAAsyncGetHostByName: the request (nullptr where it could not start, LastError says why); Window gets Message
	// (HOSTNAME_RESOLVED_MESSAGE, WM_USER + 1: the frames' OnHostResolved) when it is done, with the host's entry in
	// Buffer. TPortableSocket's lookup is done at a later Poll.
	[[nodiscard]] virtual void *ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer,
		int Length) = 0;
	// WSACancelAsyncRequest: no message for the request (nullptr, or one that is done: nothing).
	virtual void CancelResolve(void *Request) = 0;
	// inet_ntoa of the first address of the host's entry in Buffer (ResolveHostname's): the text, valid until the next
	// call.
	[[nodiscard]] virtual const char *ResolvedAddress(const char *Buffer) = 0;

	// The loop's turn (issue #125): the connections' events (OnConnect, OnReceive, OnClose) and the finished name
	// lookups, on the calling thread (the client's), where the message loop dispatched WinSock's messages. The client
	// calls it at the start of every idle step (Player.cpp's OnIdle, on both window paths). Nothing where the events
	// come otherwise (the golden runner's fakes, the replay's window messages).
	virtual void Poll() = 0;

protected:
	~ISocket() = default;
};

} // namespace patella::platform
