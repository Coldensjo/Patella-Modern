// Socket.h: ISocket, the platform's network (Patella-Modern issue #109, plan M7; notes/decisions.md, "#109: the
// platform interfaces"). Not part of the original source. Portable: no platform header.
//
// TNetwork (Network.cpp) has one connection at a time, to the login server or the game server, and resolves the
// login servers' names. What it asked of WinSock and of MFC's CAsyncSocket it asks of the process's ISocket
// (platform/Platform.h), call for call: the same calls, in the same order, with the same arguments, so the golden
// Network test's fakes of WinSock and CAsyncSocket see what they saw.
//
//	TWin32Socket   MFC's CAsyncSocket and WinSock (platform/Win32Platform.h): Patella.exe's, PatellaTest.exe's (whose
//	               server replay is a server on the loopback ports, notes/testmode.md) and the golden runner's
//
// A connection is a TSocket, which each implementation defines (Win32's: a CAsyncSocket); TNetwork holds it as the
// original held its CAsyncSocket, and the golden Network test reaches its events through it. Its events go to the
// ISocketEvents it was made for (TNetwork).

#pragma once

namespace patella::platform {

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
	// CAsyncSocket::Connect: false with LastError WSAEWOULDBLOCK while it connects (OnConnect follows).
	[[nodiscard]] virtual bool Connect(TSocket *Connection, const char *Address, int Port) = 0;
	// CAsyncSocket::Send and Receive (flags 0): the bytes sent or read, 0 at the end, SOCKET_ERROR (-1).
	virtual int Send(TSocket *Connection, const void *Data, int Length) = 0;
	virtual int Receive(TSocket *Connection, void *Buffer, int Length) = 0;
	// shutdown of the connection's socket.
	virtual void Shutdown(TSocket *Connection, TShutdown How) = 0;
	// CAsyncSocket::Close.
	virtual void Close(TSocket *Connection) = 0;

	// WSAAsyncGetHostByName: the request (nullptr where it could not start, LastError says why); Window gets Message
	// when it is done, with the host's entry in Buffer.
	[[nodiscard]] virtual void *ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer,
		int Length) = 0;
	// WSACancelAsyncRequest.
	virtual void CancelResolve(void *Request) = 0;
	// inet_ntoa of the first address of the host's entry in Buffer (ResolveHostname's): the text, valid until the next
	// call.
	[[nodiscard]] virtual const char *ResolvedAddress(const char *Buffer) = 0;

protected:
	~ISocket() = default;
};

} // namespace patella::platform
