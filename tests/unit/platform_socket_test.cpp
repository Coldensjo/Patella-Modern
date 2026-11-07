// tests_platform (issue #125): TPortableSocket (src/platform/PortableSocket.h), the client's network over Winsock or
// POSIX sockets, on the loopback interface of the machine that runs it (Windows and Linux): the connect and its
// OnConnect, the bytes both ways and OnReceive's rounds (WinSock's FD_READ rules), the peer's end and OnClose once, a
// refused connect's error, a connection closed inside its own event, the name lookups on their thread and their
// delivery at Poll (IPv4, IPv6, failure, cancel, a short buffer), the Busy hook, and the errors of calls on no
// connection. The server side is this file's, with the system's sockets.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "platform/PortableSocket.h"
#include "platform/Socket.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace {

using patella::platform::ISocketEvents;
using patella::platform::TPortableSocket;
using patella::platform::TSocket;
namespace socket_error = patella::platform::socket_error;

// The test's server side: a listener on the loopback address of Family and the connection it accepts (blocking).
#if defined(_WIN32)
using TRaw = SOCKET;
using TRawLength = int;
constexpr TRaw NoRaw = INVALID_SOCKET;
void CloseRaw(TRaw Handle)
{
	closesocket(Handle);
}
#else
using TRaw = int;
using TRawLength = std::size_t;
constexpr TRaw NoRaw = -1;
void CloseRaw(TRaw Handle)
{
	close(Handle);
}
#endif

class TServer {
public:
	explicit TServer(int Family = AF_INET)
	{
		Listener = socket(Family, SOCK_STREAM, IPPROTO_TCP);
		if (Listener == NoRaw)
			return;
		sockaddr_storage Address{};
		socklen_t Size = 0;
		if (Family == AF_INET) {
			sockaddr_in Four{};
			Four.sin_family = AF_INET;
			Four.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			std::memcpy(&Address, &Four, sizeof(Four));
			Size = sizeof(Four);
		} else {
			sockaddr_in6 Six{};
			Six.sin6_family = AF_INET6;
			Six.sin6_addr = in6addr_loopback;
			std::memcpy(&Address, &Six, sizeof(Six));
			Size = sizeof(Six);
		}
		if (bind(Listener, reinterpret_cast<const sockaddr *>(&Address), Size) != 0 || listen(Listener, 4) != 0) {
			CloseRaw(Listener);
			Listener = NoRaw;
			return;
		}
		sockaddr_storage Bound{};
		socklen_t BoundSize = sizeof(Bound);
		getsockname(Listener, reinterpret_cast<sockaddr *>(&Bound), &BoundSize);
		Port = ntohs(Family == AF_INET ? reinterpret_cast<const sockaddr_in *>(&Bound)->sin_port
									   : reinterpret_cast<const sockaddr_in6 *>(&Bound)->sin6_port);
	}
	TServer(const TServer &) = delete;
	TServer &operator=(const TServer &) = delete;
	TServer(TServer &&) = delete;
	TServer &operator=(TServer &&) = delete;
	~TServer()
	{
		Drop();
		StopListening();
	}

	[[nodiscard]] bool Ready() const { return Listener != NoRaw; }
	[[nodiscard]] int GetPort() const { return Port; }
	void Accept() { Peer = accept(Listener, nullptr, nullptr); }
	void StopListening()
	{
		if (Listener != NoRaw)
			CloseRaw(Listener);
		Listener = NoRaw;
	}
	void Send(const std::string &Text) const
	{
		const auto Sent = send(Peer, Text.data(), static_cast<TRawLength>(Text.size()), 0);
		REQUIRE((Sent >= 0 && static_cast<std::size_t>(Sent) == Text.size()));
	}
	[[nodiscard]] std::string Receive(std::size_t Count) const
	{
		std::string Text;
		std::array<char, 256> Buffer{};
		while (Text.size() < Count) {
			const auto Got = recv(Peer, Buffer.data(), static_cast<TRawLength>(Buffer.size()), 0);
			if (Got <= 0)
				break;
			Text.append(Buffer.data(), static_cast<std::size_t>(Got));
		}
		return Text;
	}
	void Drop()
	{
		if (Peer != NoRaw)
			CloseRaw(Peer);
		Peer = NoRaw;
	}

private:
	TRaw Listener = NoRaw;
	TRaw Peer = NoRaw;
	int Port = 0;
};

// The client's events, recorded; OnReceive reads at most Take bytes per event (0: none) and runs Then after.
class TEvents final : public ISocketEvents {
public:
	explicit TEvents(TPortableSocket &SocketsRef) : Sockets(SocketsRef) {}
	void OnConnect(int ErrorCode) override
	{
		Connects++;
		ConnectError = ErrorCode;
	}
	void OnReceive(TSocket &Receiver, int ErrorCode) override
	{
		Receives++;
		CHECK(ErrorCode == 0);
		if (Take > 0) {
			std::vector<char> Buffer(Take);
			const int Got = Sockets.Receive(&Receiver, Buffer.data(), static_cast<int>(Buffer.size()));
			if (Got > 0)
				Text.append(Buffer.data(), static_cast<std::size_t>(Got));
		}
		if (Then)
			Then();
	}
	void OnClose() override { Closes++; }

	TPortableSocket &Sockets;
	int Connects = 0;
	int ConnectError = -1;
	int Receives = 0;
	int Closes = 0;
	std::size_t Take = 4096;
	std::string Text;
	std::function<void()> Then;
};

// Polls until Done holds, at most Seconds.
bool PollUntil(TPortableSocket &Sockets, const std::function<bool()> &Done, int Seconds = 20)
{
	const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(Seconds);
	while (!Done()) {
		if (std::chrono::steady_clock::now() > End)
			return false;
		Sockets.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return true;
}

// The lookups delivered to the Resolved hook.
struct TReply {
	void *Window;
	unsigned int Message;
	void *Request;
	int Error;
};
std::vector<TReply> Replies;
void RecordReply(void *Window, unsigned int Message, void *Request, int Error)
{
	Replies.push_back({Window, Message, Request, Error});
}
std::vector<bool> BusyChanges;
void RecordBusy(bool Busy)
{
	BusyChanges.push_back(Busy);
}

// Connects a new connection of Sockets to 127.0.0.1:Port (or Address); OnConnect has come when it returns.
TSocket *Connected(TPortableSocket &Sockets, TEvents &Events, TServer &Server, const char *Address = "127.0.0.1")
{
	TSocket *Connection = Sockets.New(Events);
	REQUIRE(Sockets.Create(Connection));
	const bool Now = Sockets.Connect(Connection, Address, Server.GetPort());
	if (!Now)
		REQUIRE(Sockets.LastError() == socket_error::WouldBlock);
	Server.Accept();
	REQUIRE(PollUntil(Sockets, [&] { return Events.Connects > 0; }));
	REQUIRE(Events.ConnectError == 0);
	return Connection;
}

} // namespace

TEST_CASE("TPortableSocket: a connection's connect, bytes both ways, the peer's end")
{
	TPortableSocket Sockets;
	REQUIRE(Sockets.Startup());
	TServer Server;
	REQUIRE(Server.Ready());
	TEvents Events(Sockets);
	TSocket *Connection = Connected(Sockets, Events, Server);
	CHECK(Events.Connects == 1);
	CHECK(Events.Receives == 0);

	Server.Send("hello");
	REQUIRE(PollUntil(Sockets, [&] { return Events.Text.size() == 5; }));
	CHECK(Events.Text == "hello");

	const std::string Ping = "ping";
	CHECK(Sockets.Send(Connection, Ping.data(), static_cast<int>(Ping.size())) == 4);
	CHECK(Server.Receive(4) == "ping");

	// Nothing to read: Receive fails with WSAEWOULDBLOCK, and Poll calls nothing.
	std::array<char, 8> Buffer{};
	CHECK(Sockets.Receive(Connection, Buffer.data(), static_cast<int>(Buffer.size())) == patella::platform::SocketFailed);
	CHECK(Sockets.LastError() == socket_error::WouldBlock);
	const int Before = Events.Receives;
	Sockets.Poll();
	CHECK(Events.Receives == Before);

	// The peer's end after its last bytes: OnReceive for the bytes, then OnClose, once.
	Server.Send("bye");
	Server.Drop();
	REQUIRE(PollUntil(Sockets, [&] { return Events.Closes > 0; }));
	CHECK(Events.Text == "hellobye");
	for (int i = 0; i < 5; i++)
		Sockets.Poll();
	CHECK(Events.Closes == 1);
	CHECK(Events.Connects == 1);

	// TNetwork's Disconnect on it: shutdown, Receive until 0 or an error, shutdown, Close, delete.
	Sockets.Shutdown(Connection, patella::platform::TShutdown::Send);
	CHECK(Sockets.Receive(Connection, Buffer.data(), static_cast<int>(Buffer.size())) <= 0);
	Sockets.Shutdown(Connection, patella::platform::TShutdown::Both);
	Sockets.Close(Connection);
	CHECK_FALSE(Sockets.Busy());
	Sockets.Delete(Connection);
}

TEST_CASE("TPortableSocket: OnReceive's rounds follow WinSock's FD_READ")
{
	TPortableSocket Sockets;
	REQUIRE(Sockets.Startup());
	TServer Server;
	REQUIRE(Server.Ready());
	TEvents Events(Sockets);
	TSocket *Connection = Connected(Sockets, Events, Server);

	// A handler that takes one byte per event: again while bytes are left (FD_READ after each recv), at most ReadRounds
	// times in one Poll.
	Events.Take = 1;
	Server.Send(std::string(100, 'x'));
	REQUIRE(PollUntil(Sockets, [&] { return Events.Receives > 0; }));
	REQUIRE(PollUntil(Sockets, [&] { return Events.Text.size() == 100; }));
	CHECK(Events.Receives == 100);

	// Once the bytes are all there, one Poll gives ReadRounds events.
	Events.Text.clear();
	Events.Receives = 0;
	Server.Send(std::string(100, 'y'));
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	Sockets.Poll();
	CHECK(Events.Receives == TPortableSocket::ReadRounds);
	CHECK(Events.Text.size() == static_cast<std::size_t>(TPortableSocket::ReadRounds));
	REQUIRE(PollUntil(Sockets, [&] { return Events.Text.size() == 100; }));

	// A handler that takes nothing (its buffer full): one event per Poll, the bytes stay.
	Events.Take = 0;
	Events.Receives = 0;
	Server.Send("zz");
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	Sockets.Poll();
	CHECK(Events.Receives == 1);
	Sockets.Poll();
	CHECK(Events.Receives == 2);
	Events.Take = 16;
	Events.Text.clear();
	Sockets.Poll();
	CHECK(Events.Text == "zz");
	Sockets.Delete(Connection);
}

TEST_CASE("TPortableSocket: a connection closed inside its own event has no further event")
{
	TPortableSocket Sockets;
	REQUIRE(Sockets.Startup());
	TServer Server;
	REQUIRE(Server.Ready());
	TEvents Events(Sockets);
	TSocket *Connection = Connected(Sockets, Events, Server);
	Events.Take = 1;
	Events.Then = [&] {
		Sockets.Delete(Connection); // TNetwork's OnClose and the handlers' Disconnect delete the TSocket in its event
		Connection = nullptr;
	};
	Server.Send("abc");
	Server.Drop();
	REQUIRE(PollUntil(Sockets, [&] { return Events.Receives > 0; }));
	for (int i = 0; i < 5; i++)
		Sockets.Poll();
	CHECK(Events.Receives == 1);
	CHECK(Events.Closes == 0);
	CHECK(Connection == nullptr);
	CHECK_FALSE(Sockets.Busy());
}

TEST_CASE("TPortableSocket: a refused connect")
{
	TPortableSocket Sockets;
	REQUIRE(Sockets.Startup());
	int Port = 0;
	{
		TServer Closed;
		REQUIRE(Closed.Ready());
		Port = Closed.GetPort();
	}
	TEvents Events(Sockets);
	TSocket *Connection = Sockets.New(Events);
	REQUIRE(Sockets.Create(Connection));
	if (!Sockets.Connect(Connection, "127.0.0.1", Port)) {
		const int Error = Sockets.LastError();
		if (Error != socket_error::WouldBlock) {
			// Refused in the call (some systems on loopback): the error is WinSock's.
			CHECK(Error == socket_error::ConnectionRefused);
			Sockets.Delete(Connection);
			return;
		}
	}
	REQUIRE(PollUntil(Sockets, [&] { return Events.Connects > 0; }));
	CHECK(Events.ConnectError == socket_error::ConnectionRefused);
	for (int i = 0; i < 5; i++)
		Sockets.Poll();
	CHECK(Events.Connects == 1);
	CHECK(Events.Receives == 0);
	CHECK(Events.Closes == 0);
	Sockets.Delete(Connection);
}

TEST_CASE("TPortableSocket: an IPv6 connection")
{
	TPortableSocket Sockets;
	REQUIRE(Sockets.Startup());
	TServer Server(AF_INET6);
	if (!Server.Ready()) {
		MESSAGE("no IPv6 loopback here: skipped");
		return;
	}
	TEvents Events(Sockets);
	TSocket *Connection = Connected(Sockets, Events, Server, "::1");
	Server.Send("six");
	REQUIRE(PollUntil(Sockets, [&] { return Events.Text.size() == 3; }));
	CHECK(Events.Text == "six");
	Sockets.Delete(Connection);
}

TEST_CASE("TPortableSocket: the calls on no connection, and a bad address")
{
	TPortableSocket Sockets;
	REQUIRE(Sockets.Startup());
	std::array<char, 4> Buffer{};
	CHECK_FALSE(Sockets.Connect(nullptr, "127.0.0.1", 7171)); // TNetwork's ConnectSocket after a failed Create
	CHECK(Sockets.LastError() == socket_error::NotSocket);
	CHECK_FALSE(Sockets.Create(nullptr));
	CHECK(Sockets.LastError() == socket_error::NotSocket);

	TServer Server;
	REQUIRE(Server.Ready());
	TEvents Events(Sockets);
	TSocket *Connection = Sockets.New(Events);
	CHECK(Sockets.Receive(Connection, Buffer.data(), static_cast<int>(Buffer.size())) == patella::platform::SocketFailed);
	CHECK(Sockets.LastError() == socket_error::NotSocket);
	CHECK(Sockets.Send(Connection, Buffer.data(), static_cast<int>(Buffer.size())) == patella::platform::SocketFailed);
	CHECK(Sockets.LastError() == socket_error::NotSocket);
	REQUIRE(Sockets.Create(Connection));
	CHECK_FALSE(Sockets.Create(Connection));
	CHECK(Sockets.LastError() == socket_error::Invalid);
	// MFC's Connect: an address that is neither a number nor a known name is WSAEINVAL. (An empty one is the local host
	// for Windows' gethostbyname and getaddrinfo, not for POSIX's getaddrinfo.)
	CHECK_FALSE(Sockets.Connect(Connection, "patella-no-such-host.invalid", Server.GetPort()));
	CHECK(Sockets.LastError() == socket_error::Invalid);
	CHECK(Sockets.Busy());
	Sockets.Delete(Connection);
	CHECK_FALSE(Sockets.Busy());
	Sockets.Delete(nullptr);
}

TEST_CASE("TPortableSocket: the name lookups, delivered at Poll")
{
	Replies.clear();
	BusyChanges.clear();
	TPortableSocket Sockets(RecordReply, RecordBusy);
	REQUIRE(Sockets.Startup());
	int Owner = 0;
	void *Window = &Owner;
	std::array<char, patella::platform::HostEntrySize> Buffer{};

	// A number: its own address.
	void *Request = Sockets.ResolveHostname(Window, 0x401, "127.0.0.1", Buffer.data(), static_cast<int>(Buffer.size()));
	REQUIRE(Request != nullptr);
	CHECK(BusyChanges == std::vector<bool>{true});
	CHECK(Replies.empty()); // nothing before a Poll, whatever the thread has done
	REQUIRE(PollUntil(Sockets, [] { return !Replies.empty(); }));
	REQUIRE(Replies.size() == 1);
	CHECK(Replies[0].Window == Window);
	CHECK(Replies[0].Message == 0x401);
	CHECK(Replies[0].Request == Request);
	CHECK(Replies[0].Error == 0);
	CHECK(std::string(Sockets.ResolvedAddress(Buffer.data())) == "127.0.0.1");
	CHECK(BusyChanges == std::vector<bool>{true, false});

	// localhost: IPv4 where it has both.
	Replies.clear();
	Request = Sockets.ResolveHostname(Window, 0x401, "localhost", Buffer.data(), static_cast<int>(Buffer.size()));
	REQUIRE(PollUntil(Sockets, [] { return !Replies.empty(); }));
	CHECK(Replies[0].Request == Request);
	CHECK(Replies[0].Error == 0);
	const std::string Local = Sockets.ResolvedAddress(Buffer.data());
	CHECK((Local == "127.0.0.1" || Local == "::1"));

	// An IPv6 number.
	Replies.clear();
	Request = Sockets.ResolveHostname(Window, 0x401, "::1", Buffer.data(), static_cast<int>(Buffer.size()));
	REQUIRE(PollUntil(Sockets, [] { return !Replies.empty(); }));
	CHECK(Replies[0].Error == 0);
	CHECK(std::string(Sockets.ResolvedAddress(Buffer.data())) == "::1");

	// A name that does not exist: WinSock's numbers for it.
	Replies.clear();
	Request = Sockets.ResolveHostname(Window, 0x401, "patella-no-such-host.invalid", Buffer.data(),
		static_cast<int>(Buffer.size()));
	REQUIRE(PollUntil(Sockets, [] { return !Replies.empty(); }, 60));
	CHECK(Replies[0].Request == Request);
	const int Error = Replies[0].Error;
	CHECK((Error == socket_error::HostNotFound || Error == socket_error::TryAgain || Error == socket_error::NoData
		|| Error == socket_error::NoRecovery));

	// A cancelled lookup gives nothing.
	Replies.clear();
	Request = Sockets.ResolveHostname(Window, 0x401, "127.0.0.1", Buffer.data(), static_cast<int>(Buffer.size()));
	Sockets.CancelResolve(Request);
	CHECK_FALSE(Sockets.Busy());
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	for (int i = 0; i < 10; i++)
		Sockets.Poll();
	CHECK(Replies.empty());
	Sockets.CancelResolve(nullptr);

	// A buffer too small for the address: WSAENOBUFS.
	std::array<char, 4> Short{};
	Request = Sockets.ResolveHostname(Window, 0x401, "127.0.0.1", Short.data(), static_cast<int>(Short.size()));
	REQUIRE(PollUntil(Sockets, [] { return !Replies.empty(); }));
	CHECK(Replies[0].Error == socket_error::NoBuffers);

	// No name: refused in the call.
	CHECK(Sockets.ResolveHostname(Window, 0x401, nullptr, Buffer.data(), static_cast<int>(Buffer.size())) == nullptr);
	CHECK(Sockets.LastError() == socket_error::Fault);
}
