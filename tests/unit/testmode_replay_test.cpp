// Issue #23: the scripts of the reference recordings (clock manual, advance, cursor, post, save;
// src/testmode/InputScript.h) and the server replay (src/testmode/ServerReplay.h): its file, its
// schedule (tools/proxy.py's replay), its XTEA frames.

#include <doctest/doctest.h>

#include "testmode/InputScript.h"
#include "testmode/ServerReplay.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace patella::testmode;

namespace {

using TBytes = std::vector<std::uint8_t>;

std::array<std::uint8_t, 16> Key()
{
	std::array<std::uint8_t, 16> Result{};
	for (std::size_t i = 0; i < Result.size(); i++)
		Result.at(i) = static_cast<std::uint8_t>(i + 1);
	return Result;
}

TServerReplay Schedule(const char *Text)
{
	TReplayParseResult Parsed = ParseReplay(Text);
	REQUIRE_MESSAGE(Parsed.Ok, Parsed.ErrorLine, ": ", Parsed.Error);
	return TServerReplay(Parsed.File.Connections);
}

} // namespace

TEST_CASE("InputScript: clock manual, advance, cursor, post, save")
{
	TParseResult Result = ParseScript("clock manual\ncursor 1020 760\npost 0x200 0x0 0x00460071\nadvance 2000 20\n"
									  "post 0x101 0x11 0xc01d0001\nsave Tibia.cfg\nadvance 150 0\ndump\n");
	REQUIRE_MESSAGE(Result.Ok, Result.ErrorLine, ": ", Result.Error);
	const TScript &Script = Result.Script;
	CHECK(Script.ManualClock);
	CHECK(Script.FixedCursor);
	CHECK(Script.CursorX == 1020);
	CHECK(Script.CursorY == 760);
	REQUIRE(Script.Events.size() == 6);
	CHECK(Script.Events[0].Kind == TEventKind::Post);
	CHECK(Script.Events[0].Key == 0x200);
	CHECK(Script.Events[0].LParam == 0x00460071);
	CHECK(Script.Events[0].Time == 0);
	CHECK(Script.Events[1].Kind == TEventKind::Advance);
	CHECK(Script.Events[1].Duration == 2000);
	CHECK(Script.Events[1].Ticks == 20);
	CHECK(Script.Events[1].Time == 0);
	// After an advance the commands happen later.
	CHECK(Script.Events[2].Time == 2000);
	CHECK(Script.Events[2].WParam == 0x11);
	CHECK(Script.Events[2].LParam == 0xc01d0001);
	CHECK(Script.Events[3].Kind == TEventKind::Save);
	CHECK(Script.Events[3].Name == "Tibia.cfg");
	CHECK(Script.Events[5].Time == 2150);
	CHECK(Describe(Script.Events[1]) == "0 ms, line 4: advance 2000 ms, 20 ticks");
	CHECK(Describe(Script.Events[2]) == "2000 ms, line 5: post 0x101 0x11 0xc01d0001");
	CHECK(Describe(Script.Events[3]) == "2000 ms, line 6: save Tibia.cfg");
}

TEST_CASE("InputScript: the manual clock's errors")
{
	auto Error = [](const char *Text) { return ParseScript(Text).Error; };
	CHECK(Error("advance 100 1") == "advance needs clock manual");
	CHECK(Error("clock manual\nwait 100") == "wait with clock manual: the time moves only by advance");
	CHECK(Error("clock manual\nat 100") == "at with clock manual: the time moves only by advance");
	CHECK(Error("char 1\nclock manual") == "clock comes before the first event");
	CHECK(Error("char 1\ncursor 1 2") == "cursor comes before the first event");
	CHECK(Error("clock sometimes") == "clock takes manual or auto, not sometimes");
	CHECK(Error("post 0x10000 0 0") == "0x10000 is larger than 65535");
	CHECK(Error("post 0x100 x 0") == "not a number: x");
	CHECK(Error("save ..\\Tibia.cfg") == "save takes a file name without a path: ..\\Tibia.cfg");
	CHECK(ParseScript("save a\nquit").Ok);
}

TEST_CASE("TScheduler: a manual clock never moves by itself")
{
	TParseResult Result = ParseScript("clock manual\nchar 1\nadvance 300 3\nchar 2");
	REQUIRE(Result.Ok);
	TScheduler Scheduler(std::move(Result.Script));
	CHECK(Scheduler.ManualClock());
	CHECK(Scheduler.NextStop(0) == 0);
	REQUIRE(Scheduler.TakeDue(0) != nullptr);
	const TInputEvent *Advance = Scheduler.TakeDue(0);
	REQUIRE(Advance != nullptr);
	CHECK(Advance->Kind == TEventKind::Advance);
	// The driver moves the clock by the advance; only then is the next event due.
	CHECK(Scheduler.TakeDue(299) == nullptr);
	CHECK(Scheduler.NextStop(299) == 299);
	REQUIRE(Scheduler.TakeDue(300) != nullptr);
	CHECK(Scheduler.Finished());
}

TEST_CASE("ParseReplay: the file")
{
	TReplayParseResult Parsed = ParseReplay("# patella-replay 1\nlisten login 7271\nlisten game 7272\nlogin 1 c2s\n"
											"login 1 s2c 0 14aa\nlogin 1 close server 2000\ngame 1 c2s\n"
											"game 1 s2c 4000 -\ngame 1 c2s\ngame 1 close client\n");
	REQUIRE_MESSAGE(Parsed.Ok, Parsed.ErrorLine, ": ", Parsed.Error);
	REQUIRE(Parsed.File.Listen.size() == 2);
	CHECK(Parsed.File.Listen[1].Kind == "game");
	CHECK(Parsed.File.Listen[1].Port == 7272);
	REQUIRE(Parsed.File.Connections.size() == 2);
	const TReplayConnection &Login = Parsed.File.Connections[0];
	CHECK(Login.Kind == "login");
	REQUIRE(Login.Events.size() == 3);
	CHECK(Login.Events[1].Kind == TReplayKind::ServerPacket);
	CHECK(Login.Events[1].Payload == TBytes{0x14, 0xaa});
	CHECK(Login.Events[2].Kind == TReplayKind::ServerClose);
	CHECK(Login.Events[2].Time == 2000);
	const TReplayConnection &Game = Parsed.File.Connections[1];
	REQUIRE(Game.Events.size() == 4);
	CHECK(Game.Events[1].Payload.empty());
	CHECK(Game.Events[1].Line == 8);
	CHECK(Game.Events[3].Kind == TReplayKind::ClientClose);

	auto Error = [](const char *Text) { return ParseReplay(Text).Error; };
	CHECK(Error("login 1 c2s\n") == "the file does not start with \"# patella-replay 1\"");
	CHECK(Error("# patella-replay 1\nlogin 1 s2c 0 aa\n") == "a connection starts with the client's packet (c2s)");
	CHECK(Error("# patella-replay 1\nlogin 0 c2s\n") == "not a connection ordinal: 0");
	CHECK(Error("# patella-replay 1\nlogin 1 c2s\nlogin 1 s2c 0 abc\n") == "s2c takes a time and a hex payload");
	CHECK(Error("# patella-replay 1\nlisten other 1\n") == "listen takes login or game and a port");
	CHECK(Error("# patella-replay 1\nlogin 1 c2s\nlogin 1 close\n") == "close takes server <time> or client");
}

TEST_CASE("TServerReplay: the proxy's schedule")
{
	TServerReplay Replay = Schedule("# patella-replay 1\nlogin 1 c2s\nlogin 1 s2c 100 01\nlogin 1 s2c 100 02\n"
									"login 1 c2s\nlogin 1 s2c 300 03\nlogin 1 close server 300\n"
									"login 2 c2s\nlogin 2 s2c 0 04\n");
	int First = Replay.Open("login");
	CHECK(First == 0);
	CHECK(Replay.Open("game") == -1);
	// Nothing before the client's first packet.
	CHECK(Replay.Due(First, 1000) == nullptr);
	Replay.ClientPacket(First);
	// Not before its time.
	CHECK(Replay.Due(First, 99) == nullptr);
	std::uint32_t Time = 0;
	CHECK(Replay.NextTime(First, &Time));
	CHECK(Time == 100);
	const TReplayEvent *Event = Replay.Due(First, 100);
	REQUIRE(Event != nullptr);
	CHECK(Event->Payload == TBytes{0x01});
	// A client packet that comes early waits for its place.
	Replay.ClientPacket(First);
	Replay.Take(First);
	Event = Replay.Due(First, 100);
	REQUIRE(Event != nullptr);
	CHECK(Event->Payload == TBytes{0x02});
	Replay.Take(First);
	// The early client packet passed its place: the next server packet is due at its time.
	CHECK(Replay.Due(First, 299) == nullptr);
	Event = Replay.Due(First, 300);
	REQUIRE(Event != nullptr);
	CHECK(Event->Payload == TBytes{0x03});
	Replay.Take(First);
	Event = Replay.Due(First, 300);
	REQUIRE(Event != nullptr);
	CHECK(Event->Kind == TReplayKind::ServerClose);
	Replay.Take(First);
	CHECK(Replay.Finished(First));
	// The second login connection is the recording's second.
	int Second = Replay.Open("login");
	CHECK(Second == 1);
	Replay.ClientPacket(Second);
	REQUIRE(Replay.Due(Second, 0) != nullptr);
	Replay.Take(Second);
	// A client packet after the end of the recording.
	Replay.ClientPacket(Second);
	CHECK(Replay.Unexpected() == 1);
	CHECK(Replay.Open("login") == -1);
}

TEST_CASE("TServerReplay: nothing more is sent once the client closed")
{
	TServerReplay Replay = Schedule("# patella-replay 1\ngame 1 c2s\ngame 1 s2c 0 01\ngame 1 c2s\ngame 1 s2c 0 02\n");
	int Game = Replay.Open("game");
	Replay.ClientPacket(Game);
	REQUIRE(Replay.Due(Game, 0) != nullptr);
	Replay.Take(Game);
	Replay.ClientClosed(Game);
	CHECK(Replay.Due(Game, 10000) == nullptr);
}

TEST_CASE("EncryptFrame: tools/proxy.py's encrypt_frame")
{
	TBytes Payload = {0x14, 0x01, 0x02};
	TBytes Frame = EncryptFrame(Payload, Key());
	// [u16 8][XTEA([u16 3][payload][3 zero bytes])]
	REQUIRE(Frame.size() == 10);
	CHECK(Frame[0] == 8);
	CHECK(Frame[1] == 0);
	TBytes Body(Frame.begin() + 2, Frame.end());
	XteaDecrypt(Body, Key());
	CHECK(Body == TBytes{3, 0, 0x14, 0x01, 0x02, 0, 0, 0});
	// The XTEA of src/TXTEA.cpp: a known block (tools/proxy.py xtea_encrypt with key 01..10).
	TBytes Block(8, 0);
	XteaEncrypt(Block, Key());
	TBytes Again = Block;
	XteaDecrypt(Again, Key());
	CHECK(Again == TBytes(8, 0));
	CHECK(Block != TBytes(8, 0));
	CHECK(EncryptFrame({}, Key()).size() == 10);
}
