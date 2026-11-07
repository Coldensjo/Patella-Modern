// PortableSocket.h: TPortableSocket, the platform's network over Winsock or POSIX sockets (Patella-Modern issue #125,
// plan M8; notes/decisions.md, "#125"). Not part of the original source. Portable: no platform header.
//
// The ISocket of Patella.exe and of PatellaTest.exe's live runs since #125, where MFC's CAsyncSocket and
// WSAAsyncGetHostByName were (the golden runner keeps those calls: tests/golden/network/MfcSocket.h). One
// implementation for Windows (Winsock 2) and POSIX (Linux), the same code but for the system calls' spellings:
//
// - The connections are non-blocking sockets, IPv6 with IPv4 mapped where the system has IPv6, else IPv4. Nothing
//   posts their events: Poll, the loop's turn (Player.cpp's OnIdle), asks each connection and calls its TSocket's
//   events as WSAAsyncSelect's messages called CAsyncSocket's: OnConnect once when the connect is done (0 or its
//   error), OnReceive while the connection has bytes and the handler takes some (WinSock's FD_READ, posted again after
//   a recv that leaves bytes; MFC's DoCallBack selects first), OnClose once at the peer's end or a reset (FD_CLOSE).
//   A connection closed by an event's handler has no further event (MFC's handle map has it no more).
// - The name lookup is getaddrinfo on a std::jthread (IPv4 and IPv6): the thread writes nothing of the client's; the
//   loop's Poll takes its result, writes the address's text into the client's buffer and gives the request to the
//   Resolved hook (the window's message: the frames' OnHostResolved). A cancelled lookup's thread runs to its end and
//   is dropped at a later Poll.
// - The error codes are WinSock's numbers (platform/Socket.h's socket_error), on POSIX from errno and getaddrinfo's
//   codes.
//
// The client's thread is the only one that calls it; the lookups' threads share only their own result with it.

#pragma once

#include "platform/Socket.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace patella::platform {

// The system's socket: SOCKET (an unsigned pointer-sized integer) on Windows, a file descriptor elsewhere.
#if defined(_WIN32)
using TNativeSocket = std::uintptr_t;
inline constexpr TNativeSocket InvalidNativeSocket = ~TNativeSocket{0};
#else
using TNativeSocket = int;
inline constexpr TNativeSocket InvalidNativeSocket = -1;
#endif

// A connection: the original's CAsyncSocket of Network.cpp (vtable 0x56d388) with its four events, which call the
// ISocketEvents it was made for (OnSend: the original's empty 0x4eac00). TPortableSocket keeps its state here; the
// replay's TPipeSocketWin32 calls the events itself.
class TSocket final {
public:
	explicit TSocket(ISocketEvents &Owner) : Events(Owner) {}
	TSocket(const TSocket &) = delete;
	TSocket &operator=(const TSocket &) = delete;
	TSocket(TSocket &&) = delete;
	TSocket &operator=(TSocket &&) = delete;
	~TSocket() = default;

	void OnReceive(int ErrorCode) { Events.OnReceive(*this, ErrorCode); }
	void OnSend(int /*ErrorCode*/) {}
	void OnConnect(int ErrorCode) { Events.OnConnect(ErrorCode); }
	void OnClose(int /*ErrorCode*/) { Events.OnClose(); }

private:
	friend class TPortableSocket;

	ISocketEvents &Events;
	TNativeSocket Handle = InvalidNativeSocket;
	std::uint64_t Id = 0;    // TPortableSocket's registry, 0: not created or closed
	int Family = 0;          // AF_INET6 (IPv4 mapped) or AF_INET
	bool Connecting = false; // Connect has started, OnConnect has not come
	bool Connected = false;  // OnConnect(0) has come: OnReceive and OnClose may
	bool Ended = false;      // OnClose has come
	std::uint64_t Taken = 0; // the bytes Receive has read (whether OnReceive took any)
};

class TPortableSocket final : public ISocket {
public:
	// A lookup is done (Poll): Window's Message for Request, Error 0 (the address's text in the buffer) or the
	// WinSock number. Nothing where it is null.
	using TResolved = void (*)(void *Window, unsigned int Message, void *Request, int Error);
	// The socket has its first connection or lookup (true), or none any more (false): the Windows client polls its
	// modal loops then (NetworkWin32.cpp). Nothing where it is null.
	using TBusy = void (*)(bool Busy);

	explicit TPortableSocket(TResolved ResolvedHook = nullptr, TBusy OnBusy = nullptr);
	TPortableSocket(const TPortableSocket &) = delete;
	TPortableSocket &operator=(const TPortableSocket &) = delete;
	TPortableSocket(TPortableSocket &&) = delete;
	TPortableSocket &operator=(TPortableSocket &&) = delete;
	// Closes what is open; waits for the lookups' threads (the client's instance is never destroyed).
	~TPortableSocket();

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
	void Poll() override;

	// Whether it has a connection or a lookup (the Busy hook's state).
	[[nodiscard]] bool Busy() const;

	// OnReceive rounds of one connection in one Poll at most (the bytes that keep coming wait for the next one).
	static constexpr int ReadRounds = 64;

private:
	struct TLookup;

	// The connection's events at this Poll: OnConnect, then OnReceive or OnClose.
	void PollConnection(std::uint64_t Id);
	[[nodiscard]] bool Alive(std::uint64_t Id, const TSocket *Connection) const;
	// Takes the finished lookups and delivers those not cancelled.
	void PollLookups();
	// The Busy hook, when Busy() has changed.
	void UpdateBusy();
	int Fail(int Code);

	TResolved Resolved;
	TBusy BusyHook;
	bool WasBusy = false;
	bool Started = false;
	int Error = 0;
	std::uint64_t NextId = 1;
	std::unordered_map<std::uint64_t, TSocket *> Connections;
	std::uintptr_t NextRequest = 1;
	std::vector<std::unique_ptr<TLookup>> Lookups;
};

} // namespace patella::platform
