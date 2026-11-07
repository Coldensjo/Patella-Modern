// Issue #120: the server replay's network inside the process (src/testmode/PipeNetwork.h): the listeners, the two
// byte queues of a connection and their ends, and WinSock's rules for what the client's socket is told (connect and
// write after a connect, one read until a receive, a read again after a receive that leaves bytes, one close).

#include <doctest/doctest.h>

#include "testmode/PipeNetwork.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

using namespace patella::testmode;

namespace {

struct TPosted {
	int Id = 0;
	TPipeEvent Event = TPipeEvent::Connect;
	bool Failed = false;

	friend bool operator==(const TPosted &, const TPosted &) = default;
};

struct TFixture {
	std::vector<TPosted> Posted;
	TPipeNetwork Network{[this](int Id, TPipeEvent Event, bool Failed) { Posted.push_back(TPosted{Id, Event, Failed}); }};

	TFixture()
	{
		Network.Listen("127.0.0.1", 7271, "login");
		Network.Listen("127.0.0.1", 7272, "game");
	}

	std::vector<TPosted> Take()
	{
		std::vector<TPosted> Result;
		Result.swap(Posted);
		return Result;
	}
};

using TBytes = std::vector<std::uint8_t>;

TBytes ServerRead(TPipeNetwork &Network, int Id)
{
	std::array<std::uint8_t, 64> Buffer{};
	const TPipeResult Result = Network.ServerReceive(Id, Buffer);
	REQUIRE(Result.Count >= 0);
	return {Buffer.begin(), Buffer.begin() + Result.Count};
}

} // namespace

TEST_CASE("PipeNetwork: a connection to a listener, its bytes both ways and its events")
{
	TFixture F;
	const int Client = F.Network.Open();
	CHECK(F.Network.Live(Client));
	std::array<std::uint8_t, 8> Buffer{};
	CHECK(F.Network.Receive(Client, Buffer).Error == TPipeError::NotConnected);
	CHECK_FALSE(F.Network.Accept().has_value());

	REQUIRE(F.Network.Connect(Client, "127.0.0.1", 7272));
	CHECK_FALSE(F.Network.Connect(Client, "127.0.0.1", 7272)); // already connecting
	CHECK(F.Take() == std::vector<TPosted>{{Client, TPipeEvent::Connect, false}, {Client, TPipeEvent::Write, false}});

	// The client sends before the server has accepted, as over loopback (the backlog).
	const TBytes Login = {1, 2, 3};
	CHECK(F.Network.Send(Client, Login).Count == 3);
	const std::optional<TPipeNetwork::TAccepted> Accepted = F.Network.Accept();
	REQUIRE(Accepted.has_value());
	CHECK(Accepted->Id == Client);
	CHECK(Accepted->Kind == "game");
	CHECK_FALSE(F.Network.Accept().has_value());
	CHECK(ServerRead(F.Network, Client) == Login);
	CHECK(F.Network.ServerReceive(Client, Buffer).Error == TPipeError::WouldBlock);

	// Nothing to read: would block, not readable.
	CHECK(F.Network.Receive(Client, Buffer).Error == TPipeError::WouldBlock);
	CHECK_FALSE(F.Network.Readable(Client));

	// One read for two sends until a receive; a receive that leaves bytes posts another.
	const TBytes First = {10, 11, 12, 13, 14, 15};
	const TBytes Second = {16, 17, 18, 19, 20, 21};
	CHECK(F.Network.ServerSend(Client, First).Count == 6);
	CHECK(F.Network.ServerSend(Client, Second).Count == 6);
	CHECK(F.Take() == std::vector<TPosted>{{Client, TPipeEvent::Read, false}});
	CHECK(F.Network.Readable(Client));
	TPipeResult Result = F.Network.Receive(Client, Buffer);
	CHECK(Result.Count == 8);
	CHECK(TBytes(Buffer.begin(), Buffer.end()) == TBytes{10, 11, 12, 13, 14, 15, 16, 17});
	CHECK(F.Take() == std::vector<TPosted>{{Client, TPipeEvent::Read, false}});
	Result = F.Network.Receive(Client, Buffer);
	CHECK(Result.Count == 4);
	CHECK(F.Take().empty());
	CHECK_FALSE(F.Network.Readable(Client));

	// The server's close (shutdown for sending): one close, then the end of the stream.
	F.Network.ServerShutdown(Client);
	F.Network.ServerShutdown(Client);
	CHECK(F.Take() == std::vector<TPosted>{{Client, TPipeEvent::Close, false}});
	CHECK(F.Network.Readable(Client));
	CHECK(F.Network.Receive(Client, Buffer).Count == 0);
	CHECK(F.Network.ServerSend(Client, First).Error == TPipeError::Shutdown);

	// The client's answer (TNetwork's Disconnect): shutdown for sending, read to the end, shutdown, close.
	F.Network.Shutdown(Client, false);
	CHECK(F.Network.Send(Client, Login).Error == TPipeError::Shutdown);
	CHECK(F.Network.ServerReceive(Client, Buffer).Count == 0);
	F.Network.Shutdown(Client, true);
	CHECK(F.Network.Receive(Client, Buffer).Error == TPipeError::Shutdown);
	F.Network.Release(Client);
	CHECK_FALSE(F.Network.Live(Client));
	CHECK(F.Network.Receive(Client, Buffer).Error == TPipeError::NotConnected);
	CHECK(F.Network.ServerReceive(Client, Buffer).Count == 0);
	F.Network.ServerClose(Client);
	CHECK(F.Take().empty());
}

TEST_CASE("PipeNetwork: refused, other ports and addresses, the order of the accepts")
{
	TFixture F;
	const int Refused = F.Network.Open();
	REQUIRE(F.Network.Connect(Refused, "127.0.0.1", 7171));
	const int Elsewhere = F.Network.Open();
	REQUIRE(F.Network.Connect(Elsewhere, "127.0.0.2", 7271));
	CHECK(F.Take()
		== std::vector<TPosted>{{Refused, TPipeEvent::Connect, true}, {Elsewhere, TPipeEvent::Connect, true}});
	std::array<std::uint8_t, 4> Buffer{};
	CHECK(F.Network.Send(Refused, Buffer).Error == TPipeError::NotConnected);
	CHECK_FALSE(F.Network.Accept().has_value());

	const int Login = F.Network.Open();
	const int Game = F.Network.Open();
	REQUIRE(F.Network.Connect(Game, "127.0.0.1", 7272));
	REQUIRE(F.Network.Connect(Login, "127.0.0.1", 7271));
	std::optional<TPipeNetwork::TAccepted> Accepted = F.Network.Accept();
	REQUIRE(Accepted.has_value());
	CHECK(Accepted->Id == Game);
	CHECK(Accepted->Kind == "game");
	Accepted = F.Network.Accept();
	REQUIRE(Accepted.has_value());
	CHECK(Accepted->Id == Login);
	CHECK(Accepted->Kind == "login");
}

TEST_CASE("PipeNetwork: a close with bytes the other side did not read is a reset")
{
	TFixture F;
	const int Client = F.Network.Open();
	REQUIRE(F.Network.Connect(Client, "127.0.0.1", 7271));
	REQUIRE(F.Network.Accept().has_value());
	F.Take();
	std::array<std::uint8_t, 4> Buffer{};

	// The client releases its socket with a server packet unread: the server's receive fails, its send too.
	const TBytes Packet = {1, 2};
	CHECK(F.Network.ServerSend(Client, Packet).Count == 2);
	F.Network.Release(Client);
	CHECK(F.Network.ServerReceive(Client, Buffer).Error == TPipeError::Reset);
	CHECK(F.Network.ServerSend(Client, Packet).Error == TPipeError::Reset);
	F.Network.ServerClose(Client);
	CHECK(F.Take() == std::vector<TPosted>{{Client, TPipeEvent::Read, false}}); // no close for a released socket

	// The server closes with client bytes it did not read: the client's close says reset, its receive fails.
	const int Second = F.Network.Open();
	REQUIRE(F.Network.Connect(Second, "127.0.0.1", 7271));
	REQUIRE(F.Network.Accept().has_value());
	F.Take();
	CHECK(F.Network.Send(Second, Packet).Count == 2);
	F.Network.ServerClose(Second);
	CHECK(F.Take() == std::vector<TPosted>{{Second, TPipeEvent::Close, true}});
	CHECK(F.Network.Receive(Second, Buffer).Error == TPipeError::Reset);
	CHECK(F.Network.ServerReceive(Second, Buffer).Error == TPipeError::NotConnected);
}
