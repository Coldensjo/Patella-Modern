// PipeNetwork.h: the server replay's network inside the process (Patella-Modern issue #120; notes/testmode.md,
// "The server replay"). Not part of the original source. Portable: no platform header.
//
// Until issue #120 the server replay (ServerReplay.h, TestModeWin32.cpp) was a server on the loopback ports of its
// file's listen lines (7271, 7272), and two replays at the same time (two reference compares, another agent's live
// run) collided on them. TPipeNetwork is that loopback network without the ports: the replay's listen lines are its
// listeners, the client's connections (the test build's ISocket, testmode/ReplaySocketWin32.h) reach them by address
// and port, and each connection is two byte queues, one per direction, with TCP's ends (a side's shutdown for
// sending, its close).
//
// It keeps WinSock's rules for what the client's socket is told, as the window messages of WSAAsyncSelect that MFC's
// CAsyncSocket turns into OnConnect, OnSend, OnReceive and OnClose:
//
// - Connect never completes in the call (a non-blocking connect: WSAEWOULDBLOCK); Connect, then Write, are posted,
//   or Connect with Refused where nothing listens on the address and port (WSAECONNREFUSED);
// - Read is posted when bytes come while none is posted, and again after a receive that leaves bytes (WinSock's
//   re-enabling receive); its receiver checks Readable when it comes (MFC's select before OnReceive);
// - Close is posted once, when the server shuts its side down or closes it.
//
// The server's side (the replay) polls: Accept, ServerReceive, ServerSend, ServerShutdown, ServerClose.

#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace patella::testmode {

// What the client's side of a connection is told (WinSock's FD_CONNECT, FD_WRITE, FD_READ, FD_CLOSE).
enum class TPipeEvent : std::uint8_t {
	Connect,
	Write,
	Read,
	Close,
};

// Why a call did not move bytes.
enum class TPipeError : std::uint8_t {
	None,
	WouldBlock,   // nothing to read yet (WSAEWOULDBLOCK)
	NotConnected, // not connected, or refused (WSAENOTCONN)
	Shutdown,     // this side was shut down for it (WSAESHUTDOWN)
	Reset,        // the other side closed with bytes it had not read (WSAECONNRESET)
};

// A receive or send: Count bytes (0 at the end of the stream), or -1 and Error.
struct TPipeResult {
	int Count = 0;
	TPipeError Error = TPipeError::None;
};

class TPipeNetwork {
public:
	// Posted for the client's connection Id; Failed only with Connect (refused) and Close (reset).
	using TNotify = std::function<void(int Id, TPipeEvent Event, bool Failed)>;

	TPipeNetwork() = default;
	explicit TPipeNetwork(TNotify Handler) : Notify(std::move(Handler)) {}

	// The replay's listen line: connections to Address:Port are Kind's (login, game).
	void Listen(std::string Address, int Port, std::string Kind);

	// --- the client's side ---

	// A new connection, not connected (socket and Create).
	[[nodiscard]] int Open();
	// connect: posts Connect (and Write) or Connect refused; false where the connection cannot connect (already
	// connected or connecting, or released).
	bool Connect(int Id, std::string_view Address, int Port);
	// send: all of Data (the queue has no limit), or -1.
	TPipeResult Send(int Id, std::span<const std::uint8_t> Data);
	// recv: up to Buffer's size, 0 at the server's end, or -1 (WouldBlock while the server's side is open).
	TPipeResult Receive(int Id, std::span<std::uint8_t> Buffer);
	// shutdown: for sending (the server reads to the end) or both.
	void Shutdown(int Id, bool Both);
	// closesocket: the server reads to the end, or Reset where bytes for the client were left unread.
	void Release(int Id);
	// What select says before OnReceive: bytes to read, or the end, or an error.
	[[nodiscard]] bool Readable(int Id) const;
	// The connection exists and is not released.
	[[nodiscard]] bool Live(int Id) const;

	// --- the server's side ---

	struct TAccepted {
		int Id = -1;
		std::string Kind;
	};
	// The next connection to a listener that the server has not accepted, in the order of their connects.
	[[nodiscard]] std::optional<TAccepted> Accept();
	// The client's bytes: up to Buffer's size, 0 at the client's end, or -1 (WouldBlock, Reset).
	TPipeResult ServerReceive(int Id, std::span<std::uint8_t> Buffer);
	// All of Data to the client, or -1 (the client released it: Reset; the server shut down: Shutdown).
	TPipeResult ServerSend(int Id, std::span<const std::uint8_t> Data);
	// shutdown for sending: the client reads to the end and is told Close.
	void ServerShutdown(int Id);
	// closesocket of the server's side.
	void ServerClose(int Id);

private:
	struct TListener {
		std::string Address;
		int Port = 0;
		std::string Kind;
	};

	struct TConnection {
		bool Connecting = false; // Connect called
		bool Connected = false;  // reached a listener
		bool Refused = false;
		std::string Kind;
		bool Accepted = false;
		std::deque<std::uint8_t> ToServer;
		std::deque<std::uint8_t> ToClient;
		bool ClientSendShut = false;
		bool ClientReceiveShut = false;
		bool ClientReleased = false;
		bool ClientReset = false; // released with bytes it had not read
		bool ServerSendShut = false;
		bool ServerClosed = false;
		bool ServerReset = false; // closed with bytes it had not read
		bool ReadPosted = false;
		bool ClosePosted = false;
	};

	[[nodiscard]] TConnection *Find(int Id);
	[[nodiscard]] const TConnection *Find(int Id) const;
	void Post(int Id, TPipeEvent Event, bool Failed) const;
	void PostRead(int Id, TConnection &Connection) const;
	void PostClose(int Id, TConnection &Connection, bool Reset) const;

	TNotify Notify;
	std::vector<TListener> Listeners;
	std::vector<TConnection> Connections; // by Id
	std::deque<int> Pending;              // connected, not yet accepted
};

} // namespace patella::testmode
