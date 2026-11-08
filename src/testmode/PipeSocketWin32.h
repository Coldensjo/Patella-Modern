// PipeSocketWin32.h: the test build's ISocket for the server replay (Patella-Modern issue #120; notes/testmode.md,
// "The server replay"). Not part of the original source. Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake).
//
// With --test-replay the test build's Platform() (TestModeWin32.cpp) gives TNetwork this socket instead of
// the live runs' socket: its connections are TPipeNetwork's (testmode/PipeNetwork.h), whose server side the replay's
// driver plays, so a replay opens no port and two replays run side by side. The connection is still a TSocket (since
// issue #125 the portable one, platform/PortableSocket.h, which never gets a socket handle here), so its events reach
// TNetwork as they did: TPipeNetwork's events are posted to a message-only window of the client's thread, as WinSock posts
// WSAAsyncSelect's messages to MFC's socket window, and its window procedure calls the TSocket's OnConnect, OnSend,
// OnReceive (when Readable says so, as MFC's select) or OnClose, from the message loop or a modal loop's, as MFC's.
// An event of a connection closed since it was posted is dropped (MFC's handle map has it no more).
//
// The calls the replay does not change go to the live runs' socket (NetworkSocket(), TPortableSocket since issue #125):
// Startup and the name resolution (no recording resolves a name: the client is patched to 127.0.0.1), and Poll, for
// its lookups. Without --test-replay (a live run against the test server) the test build's socket is that one itself.

#pragma once

#include "common.h"
#include "platform/Platform.h"
#include "platform/PortableSocket.h" // IWYU pragma: keep (TSocket, whose events Deliver calls)
#include "testmode/PipeNetwork.h"

#include <deque>
#include <unordered_map>

namespace patella::testmode {

class TPipeSocketWin32 final : public platform::ISocket {
public:
	TPipeSocketWin32();
	TPipeSocketWin32(const TPipeSocketWin32 &) = delete;
	TPipeSocketWin32 &operator=(const TPipeSocketWin32 &) = delete;
	TPipeSocketWin32(TPipeSocketWin32 &&) = delete;
	TPipeSocketWin32 &operator=(TPipeSocketWin32 &&) = delete;
	~TPipeSocketWin32() = default;

	// The events' window, on the calling thread (the client's); false with GetLastError where it cannot be made.
	[[nodiscard]] bool Start();
	// The network: the replay's listeners and its server side.
	[[nodiscard]] TPipeNetwork &Network() { return Pipes; }

	[[nodiscard]] bool Startup() override;
	[[nodiscard]] int LastError() override;
	[[nodiscard]] platform::TSocket *New(platform::ISocketEvents &Events) override;
	void Delete(platform::TSocket *Connection) override;
	[[nodiscard]] bool Create(platform::TSocket *Connection) override;
	[[nodiscard]] bool Connect(platform::TSocket *Connection, const char *Address, int Port) override;
	int Send(platform::TSocket *Connection, const void *Data, int Length) override;
	int Receive(platform::TSocket *Connection, void *Buffer, int Length) override;
	void Shutdown(platform::TSocket *Connection, platform::TShutdown How) override;
	void Close(platform::TSocket *Connection) override;
	[[nodiscard]] void *ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer,
		int Length) override;
	void CancelResolve(void *Request) override;
	[[nodiscard]] const char *ResolvedAddress(const char *Buffer) override;
	void Poll() override;

private:
#if defined(_WIN32)
	static LRESULT CALLBACK WindowProcedure(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);
#endif
	// TPipeNetwork's notification: the window message.
	void Post(int Id, TPipeEvent Event, bool Failed);
	// The window message: the TSocket's event.
	void Deliver(int Id, TPipeEvent Event, bool Failed);
	// The connection's id, -1: not created or closed (LastError WSAENOTSOCK).
	[[nodiscard]] int IdOf(platform::TSocket *Connection);
	int Fail(TPipeError Error);

	TPipeNetwork Pipes;
	std::unordered_map<platform::TSocket *, int> Ids;
	std::unordered_map<int, platform::TSocket *> Sockets;
#if defined(_WIN32)
	HWND Window = nullptr;
#else
	// Linux (issue #131): the events' queue, what PostMessage made on Windows; delivered by Poll, in order, until empty.
	struct TPosted {
		int Id;
		TPipeEvent Event;
		bool Failed;
	};
	std::deque<TPosted> Posted;
#endif
	int Error = 0;
};

} // namespace patella::testmode
