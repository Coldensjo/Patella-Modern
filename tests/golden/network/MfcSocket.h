// MfcSocket.h: the golden runner's ISocket, the original's calls into MFC's CAsyncSocket and WinSock (Patella-Modern
// issue #125; until then src/platform/Win32Platform.h's TWin32Socket, Patella.exe's network since issue #109).
//
// The client's network is TPortableSocket since #125 (src/platform/PortableSocket.h). The golden corpus's Network test
// (network_tests.cpp) compares the calls Network.cpp makes with the original's at the level of MFC and WinSock: its
// fakes stand in for CAsyncSocket (the MFC shim's declarations, defined there), AfxSocketInit and the WinSock
// functions, and log each call. So the runner's ISocket is still those calls, call for call, and its connection still
// the original's CAsyncSocket class, whose vtable the test fires the events through (FireEvent) and whose objects it
// dumps.

#pragma once

#include "common.h"
#include "platform/Socket.h"

namespace patella::platform {

// A connection: the original's CAsyncSocket of Network.cpp (vtable 0x56d388), whose events call the ISocketEvents
// it was made for. The golden Network test fires them through this vtable (network_tests.cpp FireEvent).
class TSocket final : public CAsyncSocket {
public:
	explicit TSocket(ISocketEvents &Owner) : Events(Owner) {}
	void OnReceive(int ErrorCode) override { Events.OnReceive(*this, ErrorCode); }
	void OnSend(int /*ErrorCode*/) override {} // 0x4eac00, an empty function shared by several vtables
	void OnConnect(int ErrorCode) override { Events.OnConnect(ErrorCode); }
	void OnClose(int /*ErrorCode*/) override { Events.OnClose(); }

private:
	ISocketEvents &Events;
};

// CAsyncSocket and WinSock (ISocket's comments name each call). Poll does nothing: the test fires the events.
class TMfcSocket final : public ISocket {
public:
	[[nodiscard]] bool Startup() override;
	[[nodiscard]] int LastError() override;
	[[nodiscard]] TSocket *New(ISocketEvents &Events) override;
	void Delete(TSocket *Connection) override;
	[[nodiscard]] bool Create(TSocket *Connection) override;
	[[nodiscard]] bool Connect(TSocket *Connection, const char *Address, int Port) override;
	int Send(TSocket *Connection, const void *Data, int Length) override;
	int Receive(TSocket *Connection, void *Buffer, int Length) override;
	void Shutdown(TSocket *Connection, TShutdown How) override;
	void Close(TSocket *Connection) override;
	[[nodiscard]] void *ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer,
		int Length) override;
	void CancelResolve(void *Request) override;
	[[nodiscard]] const char *ResolvedAddress(const char *Buffer) override;
	void Poll() override {}
};

} // namespace patella::platform
