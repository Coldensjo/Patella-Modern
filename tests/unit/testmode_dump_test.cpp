// The dumps of the test mode (issue #22): the script's dump and frame commands, the state dump's JSON
// (src/testmode/StateDump.h), the packet log's lines and connections (src/testmode/PacketLog.h) and the
// frame dump's PNG pictures (src/testmode/Png.h).

#include <doctest/doctest.h>

#include "testmode/InputScript.h"
#include "testmode/PacketLog.h"
#include "testmode/Png.h"
#include "testmode/StateDump.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace patella::testmode;

namespace {

std::span<const std::uint8_t> Bytes(std::string_view Text)
{
	return {reinterpret_cast<const std::uint8_t *>(Text.data()), Text.size()};
}

std::uint32_t Big32(const std::vector<std::uint8_t> &Data, std::size_t At)
{
	return (static_cast<std::uint32_t>(Data.at(At)) << 24U) | (static_cast<std::uint32_t>(Data.at(At + 1)) << 16U)
		| (static_cast<std::uint32_t>(Data.at(At + 2)) << 8U) | Data.at(At + 3);
}

// The data of a zlib stream of stored deflate blocks (what ZlibStored writes); checks the header, the
// block lengths and the Adler-32.
std::vector<std::uint8_t> Unstore(const std::vector<std::uint8_t> &Stream)
{
	REQUIRE(Stream.size() >= 7);
	CHECK(Stream[0] == 0x78);
	CHECK((Stream[0] * 256 + Stream[1]) % 31 == 0);
	std::vector<std::uint8_t> Out;
	std::size_t At = 2;
	bool Last = false;
	while (!Last) {
		REQUIRE(At + 5 <= Stream.size());
		Last = (Stream[At] & 1U) != 0;
		CHECK((Stream[At] & 6U) == 0); // BTYPE 00: stored
		auto Length = static_cast<std::size_t>(Stream[At + 1]) | (static_cast<std::size_t>(Stream[At + 2]) << 8U);
		auto Complement = static_cast<std::size_t>(Stream[At + 3]) | (static_cast<std::size_t>(Stream[At + 4]) << 8U);
		CHECK(Length == (~Complement & 0xffffU));
		At += 5;
		REQUIRE(At + Length <= Stream.size());
		Out.insert(Out.end(), Stream.begin() + static_cast<std::ptrdiff_t>(At),
			Stream.begin() + static_cast<std::ptrdiff_t>(At + Length));
		At += Length;
	}
	REQUIRE(At + 4 == Stream.size());
	CHECK(Big32(Stream, At) == Adler32(Out));
	return Out;
}

struct TChunk {
	std::string Kind;
	std::vector<std::uint8_t> Data;
};

// The chunks of a PNG; checks the signature and every chunk's CRC.
std::vector<TChunk> Chunks(const std::vector<std::uint8_t> &Png)
{
	const std::vector<std::uint8_t> Signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	REQUIRE(Png.size() >= 8);
	CHECK(std::vector<std::uint8_t>(Png.begin(), Png.begin() + 8) == Signature);
	std::vector<TChunk> Out;
	std::size_t At = 8;
	while (At < Png.size()) {
		REQUIRE(At + 12 <= Png.size());
		std::uint32_t Length = Big32(Png, At);
		REQUIRE(At + 12 + Length <= Png.size());
		std::span<const std::uint8_t> Typed(Png.data() + At + 4, Length + 4);
		CHECK(Big32(Png, At + 8 + Length) == Crc32(Typed));
		Out.push_back(TChunk{std::string(Png.begin() + static_cast<std::ptrdiff_t>(At + 4), Png.begin() + static_cast<std::ptrdiff_t>(At + 8)),
			std::vector<std::uint8_t>(Png.begin() + static_cast<std::ptrdiff_t>(At + 8), Png.begin() + static_cast<std::ptrdiff_t>(At + 8 + Length))});
		At += 12 + Length;
	}
	return Out;
}

// The state of the expected JSON below (tools: json.dumps(..., separators=(",", ":")) of the same data).
TStateDump SmallDump()
{
	TStateDump Dump;
	Dump.Step = 2;
	Dump.Time = 4500;
	Dump.Line = 17;
	TClientState &State = Dump.State;
	State.Connection = 4;
	for (std::size_t i = 0; i < State.Player.size(); i++)
		State.Player.at(i) = static_cast<std::int32_t>(i + 1);
	State.Skills = {10, 11};
	State.SkillProgress = {0, -1};
	State.MapCentre = {32097, 32219, 7};
	State.Map = {{}, {{102, 0, 0}, {99, 268435457, 0}}};
	TCreatureValue Creature;
	Creature.Slot = 3;
	Creature.Id = 268435457;
	Creature.Name = "Patella \xe9";
	Creature.Pos = {32097, 32219, 7};
	Creature.Direction = 2;
	Creature.Outfit = {128, 1, 2, 3, 4};
	Creature.Light = {0, 215};
	Creature.Mark = -1;
	Creature.Health = 100;
	Creature.Speed = 220;
	Creature.Visible = 1;
	State.Creatures.push_back(Creature);
	State.Body = {{0, 0, 0}, {1988, 0, 0}};
	TContainerValue Container;
	Container.Object = {1988, 0, 0};
	Container.Name = "backpack";
	Container.Capacity = 20;
	Container.Objects = {{2148, 7, 0}};
	State.Containers.push_back(Container);
	State.Trade.at(1).Name = "Cipfried";
	State.Trade.at(1).Count = 1;
	State.Trade.at(1).Objects = {{2148, 1, 0}};
	State.Console.push_back(TConsoleLine{1, true, "Cipfried: \"Hello\""});
	State.Console.push_back(TConsoleLine{17, false, ""});
	TGuiNode Main;
	Main.Class = "TMainWindow";
	Main.W = 800;
	Main.H = 600;
	Main.Ptr = 1234;
	Main.Vtable = 5678;
	TGuiNode Label;
	Label.Depth = 1;
	Label.Class = "TLabel";
	Label.X = 5;
	Label.Y = -6;
	Label.W = 10;
	Label.H = 11;
	Label.Ptr = 1;
	Label.Vtable = 2;
	Label.TextKind = TGuiText::Null;
	TGuiNode Button;
	Button.Depth = 1;
	Button.Class = "TTextButton";
	Button.X = 7;
	Button.Y = 8;
	Button.W = 9;
	Button.H = 10;
	Button.Ptr = 3;
	Button.Vtable = 4;
	Button.TextKind = TGuiText::Value;
	Button.Text = "Ok";
	TGuiNode Modal;
	Modal.Class = "(modal)";
	Dump.Gui = {Main, Label, Button, Modal};
	return Dump;
}

// \u00e9 outside the raw strings: MSVC turns it into the character there.
constexpr std::string_view SmallDumpJson =
	R"json({"format":"patella-state 1","step":2,"time":4500,"line":17,"state":{"connection":4,"player":{"PlayerID":1,"PlayerHealth":2,"PlayerMaxHealth":3,"PlayerExperience":4,"PlayerLevel":5,"PlayerLevelProgress":6,"PlayerMagicLevel":7,"PlayerMagicLevelProgress":8,"PlayerMana":9,"PlayerMaxMana":10,"PlayerSoul":11,"PlayerCapacity":12,"PlayerState":13,"AttackID":14,"FollowID":15},"skills":[10,11],"skill_progress":[0,-1],"map_centre":[32097,32219,7],"map":[[],[[102,0,0],[99,268435457,0]]],"creatures":[{"slot":3,"id":268435457,"name":"Patella )json"
	"\\u00e9"
	R"json(","pos":[32097,32219,7],"direction":2,"outfit":[128,1,2,3,4],"light":[0,215],"mark":-1,"health":100,"speed":220,"visible":1,"skull":0,"shield":0}],"body":[[0,0,0],[1988,0,0]],"containers":[{"index":0,"object":[1988,0,0],"name":"backpack","capacity":20,"has_parent":0,"objects":[[2148,7,0]]}],"trade":[{"name":"","count":0,"objects":[]},{"name":"Cipfried","count":1,"objects":[[2148,1,0]]}],"console":[[1,"Cipfried: \"Hello\""],[17,null]]},"gui":[{"depth":0,"class":"TMainWindow","x":0,"y":0,"w":800,"h":600,"ptr":1234,"vtable":5678},{"depth":1,"class":"TLabel","x":5,"y":-6,"w":10,"h":11,"ptr":1,"vtable":2,"text":null},{"depth":1,"class":"TTextButton","x":7,"y":8,"w":9,"h":10,"ptr":3,"vtable":4,"text":"Ok"},{"depth":0,"class":"(modal)","x":0,"y":0,"w":0,"h":0,"ptr":0,"vtable":0}]})json";

} // namespace

TEST_CASE("InputScript: dump and frame are events of their own")
{
	TParseResult Result = ParseScript("wait 100\ndump\nkey A\nframe # the next frame\nwait 50\nDUMP\n");
	REQUIRE(Result.Ok);
	const std::vector<TInputEvent> &Events = Result.Script.Events;
	REQUIRE(Events.size() == 6);
	CHECK(Events[0].Kind == TEventKind::StateDump);
	CHECK(Events[0].Time == 100);
	CHECK(Events[0].Line == 2);
	CHECK(Events[4].Kind == TEventKind::FrameDump);
	CHECK(Events[4].Line == 4);
	CHECK(Events[5].Kind == TEventKind::StateDump);
	CHECK(Events[5].Time == 150);
	CHECK(Describe(Events[0]) == "100 ms, line 2: dump");
	CHECK(Describe(Events[4]) == "100 ms, line 4: frame");

	TParseResult Bad = ParseScript("dump 1");
	CHECK_FALSE(Bad.Ok);
	CHECK(Bad.ErrorLine == 1);
	CHECK(Bad.Error == "dump takes 0 arguments, not 1");
	CHECK(ParseScript("frame now").Error == "frame takes 0 arguments, not 1");

	// The scheduler hands them out in their turn, one per call, like the input.
	TScheduler Scheduler(Result.Script);
	CHECK(Scheduler.TakeDue(99) == nullptr);
	CHECK(Scheduler.TakeDue(100)->Kind == TEventKind::StateDump);
	CHECK(Scheduler.TakeDue(100)->Kind == TEventKind::KeyDown);
}

TEST_CASE("StateDump: JsonString escapes as Python's json.dumps")
{
	CHECK(JsonString("") == R"("")");
	CHECK(JsonString("Hello, World!") == R"("Hello, World!")");
	CHECK(JsonString("a\"b\\c/d") == R"("a\"b\\c/d")");
	CHECK(JsonString("\n\r\t\b\f") == R"("\n\r\t\b\f")");
	CHECK(JsonString(std::string_view("\x00\x01\x1f", 3)) == R"("\u0000\u0001\u001f")");
	// Not a raw string: MSVC turns a raw string's \u00e9 into the character.
	CHECK(JsonString("\x7e\x7f\x80\xe9\xff") == "\"~\\u007f\\u0080\\u00e9\\u00ff\"");
}

TEST_CASE("StateDump: the JSON of a dump, as json.dumps writes client_state.py's reading")
{
	CHECK(StateDumpJson(SmallDump()) == std::string(SmallDumpJson) + "\n");

	// An empty state: every key is there, the lists empty.
	TStateDump Empty;
	std::string Json = StateDumpJson(Empty);
	CHECK(Json.starts_with(R"({"format":"patella-state 1","step":0,"time":0,"line":0,"state":{"connection":0,"player":{"PlayerID":0,)"));
	CHECK(Json.ends_with(R"("skills":[],"skill_progress":[],"map_centre":[0,0,0],"map":[],"creatures":[],"body":[],"containers":[],"trade":[{"name":"","count":0,"objects":[]},{"name":"","count":0,"objects":[]}],"console":[]},"gui":[]})"
						 "\n"));
	// Extremes of the integer fields.
	TStateDump Extremes;
	Extremes.Time = 4294967295U;
	Extremes.State.Connection = -2147483647 - 1;
	TCreatureValue Creature;
	Creature.Id = 4294967295U;
	Extremes.State.Creatures.push_back(Creature);
	TGuiNode Node;
	Node.Ptr = 0x7ffe12345678ULL;
	Extremes.Gui.push_back(Node);
	Json = StateDumpJson(Extremes);
	CHECK(Json.find(R"("time":4294967295,)") != std::string::npos);
	CHECK(Json.find(R"("connection":-2147483648,)") != std::string::npos);
	CHECK(Json.find(R"({"slot":0,"id":4294967295,"name":"",)") != std::string::npos);
	CHECK(Json.find(R"("ptr":140729203840632,)") != std::string::npos);
}

TEST_CASE("StateDump: the key lists name every key once")
{
	auto Unique = [](auto Keys) {
		std::set<std::string_view> Seen(Keys.begin(), Keys.end());
		return Seen.size() == Keys.size();
	};
	CHECK(Unique(StateKeys));
	CHECK(Unique(PlayerFields));
	CHECK(Unique(CreatureKeys));
	CHECK(Unique(ContainerKeys));
	CHECK(Unique(OfferKeys));
	CHECK(Unique(GuiKeys));
	std::string Json = StateDumpJson(SmallDump());
	auto Quoted = [](std::string_view Key) { return "\"" + std::string(Key) + "\":"; };
	for (auto Keys : {std::span<const std::string_view>(StateKeys), std::span<const std::string_view>(PlayerFields),
			 std::span<const std::string_view>(CreatureKeys), std::span<const std::string_view>(ContainerKeys),
			 std::span<const std::string_view>(OfferKeys), std::span<const std::string_view>(GuiKeys)}) {
		for (std::string_view Key : Keys) {
			CAPTURE(Key);
			CHECK(Json.find(Quoted(Key)) != std::string::npos);
		}
	}
}

TEST_CASE("StateDump: the probe of the control channel (issue #92)")
{
	TProbe Probe;
	Probe.Step = 3;
	Probe.Time = 2000;
	Probe.Characters = {"Patella Tester", "Patella \xe9"};
	Probe.Skin.push_back(TProbeSkin{"Data62C", false, {4}});
	Probe.Skin.push_back(TProbeSkin{"Style288.Data60", true, {1, 2, 3, 4, 5, 6, 7, 8}});
	Probe.Components.push_back(TProbeComponent{1234, {{"TComponent.Parent", 0}, {"TCheckBox.Checked", 1}}});
	Probe.MapNames.push_back(TProbeNames{77, {{268435457U, 240, 160}}});
	Probe.Device = TProbeDevice{true, "llvmpipe", 4, 1};
	Probe.Frames = 41;
	Probe.Ticks = 20;
	const std::string Json = ProbeJson(Probe);
	CHECK(Json.starts_with(R"({"format":"patella-probe 1","step":3,"time":2000,"state":{"connection":0,)"));
	CHECK(Json.find(R"(,"gui":[],"characters":["Patella Tester","Patella )") != std::string::npos);
	CHECK(Json.find("\\u00e9\"]") != std::string::npos);
	CHECK(Json.ends_with(R"("skin":{"Data62C":4,"Style288.Data60":[[1,2,3,4],[5,6,7,8]]},"members":{"1234":{"TComponent.Parent":0,"TCheckBox.Checked":1}},"names":{"77":[[268435457,240,160]]},"device":{"name":"llvmpipe","type":4,"cpu_driver":1},"frames":41,"ticks":20})"
						 "\n"));
	// No device yet: null.
	CHECK(ProbeJson(TProbe{}).find(R"("names":{},"device":null,"frames":0,"ticks":0})") != std::string::npos);
	// The state and the window tree are the state dump's.
	TStateDump Dump = SmallDump();
	Probe.State = Dump.State;
	Probe.Gui = Dump.Gui;
	const std::string Full = ProbeJson(Probe);
	const std::string DumpJson = StateDumpJson(Dump);
	const std::string Inner = DumpJson.substr(DumpJson.find(R"("state":)"), DumpJson.size() - 2 - DumpJson.find(R"("state":)"));
	CHECK(Full.find(Inner + R"(,"characters":)") != std::string::npos);
}

TEST_CASE("PacketLog: the line of a packet, and back")
{
	TPacketRecord Record;
	Record.Time = 12300;
	Record.Direction = TPacketDirection::ClientToServer;
	Record.Kind = "game";
	Record.Ordinal = 1;
	Record.Payload = {0x65};
	Record.Padding = {0x29, 0x23, 0xbe, 0x84, 0xe1};
	CHECK(PacketLogLine(Record) == "12300 c2s game 1 xtea 65 2923be84e1");
	TPacketRecord Parsed;
	REQUIRE(ParsePacketLogLine("12300 c2s game 1 xtea 65 2923be84e1\r", &Parsed));
	CHECK(Parsed == Record);

	Record.Direction = TPacketDirection::ServerToClient;
	Record.Kind = "login";
	Record.Ordinal = 2;
	Record.Payload = {0x14, 0x00, 0xab};
	Record.Padding.clear();
	CHECK(PacketLogLine(Record) == "12300 s2c login 2 xtea 1400ab -");
	REQUIRE(ParsePacketLogLine(PacketLogLine(Record), &Parsed));
	CHECK(Parsed == Record);

	Record.Direction = TPacketDirection::ClientToServer;
	Record.Plain = true;
	Record.Payload = {0x01, 0x02, 0x00};
	CHECK(PacketLogLine(Record) == "12300 c2s login 2 plain 010200 -");
	REQUIRE(ParsePacketLogLine(PacketLogLine(Record), &Parsed));
	CHECK(Parsed == Record);

	Record.Payload.clear();
	CHECK(PacketLogLine(Record) == "12300 c2s login 2 plain - -");

	for (std::string_view Bad : {std::string_view(PacketLogHeader), std::string_view(""), std::string_view("1 c2s game 1 xtea 65"),
			 std::string_view("1 c2x game 1 xtea 65 -"), std::string_view("1 c2s lobby 1 xtea 65 -"),
			 std::string_view("1 c2s game 1 rsa 65 -"), std::string_view("1 c2s game 1 xtea 6 -"),
			 std::string_view("1 c2s game 1 xtea 6G -"), std::string_view("1 c2s game 1 xtea 65 -  "),
			 std::string_view("x c2s game 1 xtea 65 -"), std::string_view("1 c2s game -1 xtea 65 -")}) {
		CAPTURE(Bad);
		CHECK_FALSE(ParsePacketLogLine(Bad, &Parsed));
	}
}

TEST_CASE("PacketLog: every unencrypted packet starts a connection")
{
	TConnections Connections;
	const std::vector<std::uint8_t> Login = {LoginOpcode, 0x02, 0x00};
	const std::vector<std::uint8_t> Game = {GameOpcode, 0x02, 0x00};
	const std::vector<std::uint8_t> Pad = {1, 2, 3};
	TPacketRecord Record = Connections.Sent(10, true, Login, Pad);
	CHECK(PacketLogLine(Record) == "10 c2s login 1 plain 010200 -");
	CHECK(PacketLogLine(Connections.Received(20, Bytes("\x14"), Pad)) == "20 s2c login 1 xtea 14 010203");
	CHECK(PacketLogLine(Connections.Sent(30, true, Game, {})) == "30 c2s game 1 plain 0a0200 -");
	CHECK(PacketLogLine(Connections.Sent(40, false, Bytes("\x65"), Pad)) == "40 c2s game 1 xtea 65 010203");
	CHECK(PacketLogLine(Connections.Received(50, Bytes("\x1e"), {})) == "50 s2c game 1 xtea 1e -");
	CHECK(PacketLogLine(Connections.Sent(60, true, Login, {})) == "60 c2s login 2 plain 010200 -");
	CHECK(PacketLogLine(Connections.Sent(70, true, Game, {})) == "70 c2s game 2 plain 0a0200 -");
	CHECK(PacketLogLine(Connections.Sent(80, true, Bytes("\x33"), {})) == "80 c2s other 1 plain 33 -");

	// Before the first connection: other 0.
	TConnections Fresh;
	CHECK(PacketLogLine(Fresh.Received(5, Bytes("\x0a"), {})) == "5 s2c other 0 xtea 0a -");
}

TEST_CASE("Png: CRC-32 and Adler-32")
{
	CHECK(Crc32(Bytes("")) == 0);
	CHECK(Crc32(Bytes("123456789")) == 0xcbf43926U);
	CHECK(Crc32(Bytes("IEND")) == 0xae426082U);
	CHECK(Crc32(Bytes("6789"), Crc32(Bytes("12345"))) == 0xcbf43926U);
	CHECK(Adler32(Bytes("")) == 1);
	CHECK(Adler32(Bytes("Wikipedia")) == 0x11e60398U);
	std::vector<std::uint8_t> Large(100000, 0xff);
	CHECK(Adler32(Large) == 0x149a302cU);
}

TEST_CASE("Png: zlib with stored blocks")
{
	CHECK(ZlibStored({}) == std::vector<std::uint8_t>{0x78, 0x01, 0x01, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01});
	for (std::size_t Size : {std::size_t(1), std::size_t(65535), std::size_t(65536), std::size_t(140000)}) {
		CAPTURE(Size);
		std::vector<std::uint8_t> Data(Size);
		for (std::size_t i = 0; i < Size; i++)
			Data[i] = static_cast<std::uint8_t>(i * 7 + i / 300);
		std::vector<std::uint8_t> Stream = ZlibStored(Data);
		CHECK(Stream.size() == 2 + 5 * ((Size + 65534) / 65535) + Size + 4);
		CHECK(Unstore(Stream) == Data);
	}
}

TEST_CASE("Png: a frame and its mask as 8-bit RGB pictures")
{
	// 3x2 D3DCOLOR pixels (B, G, R, X), rows 16 bytes apart.
	const std::vector<std::uint8_t> Bits = {
		0x01, 0x02, 0x03, 0xff, 0x04, 0x05, 0x06, 0x00, 0x07, 0x08, 0x09, 0x80, 0xee, 0xee, 0xee, 0xee,
		0x10, 0x20, 0x30, 0xff, 0x40, 0x50, 0x60, 0xff, 0x70, 0x80, 0x90, 0xff, 0xee, 0xee, 0xee, 0xee};
	std::vector<std::uint8_t> Rgb = BgrxToRgb(Bits.data(), 16, 3, 2);
	CHECK(Rgb == std::vector<std::uint8_t>{3, 2, 1, 6, 5, 4, 9, 8, 7, 0x30, 0x20, 0x10, 0x60, 0x50, 0x40, 0x90, 0x80, 0x70});
	std::vector<std::uint8_t> Png = EncodePngRgb(3, 2, Rgb);
	std::vector<TChunk> Parts = Chunks(Png);
	REQUIRE(Parts.size() == 3);
	CHECK(Parts[0].Kind == "IHDR");
	CHECK(Parts[0].Data == std::vector<std::uint8_t>{0, 0, 0, 3, 0, 0, 0, 2, 8, 2, 0, 0, 0});
	CHECK(Parts[1].Kind == "IDAT");
	CHECK(Parts[2].Kind == "IEND");
	CHECK(Parts[2].Data.empty());
	std::vector<std::uint8_t> Rows = Unstore(Parts[1].Data);
	CHECK(Rows == std::vector<std::uint8_t>{0, 3, 2, 1, 6, 5, 4, 9, 8, 7, 0, 0x30, 0x20, 0x10, 0x60, 0x50, 0x40, 0x90, 0x80, 0x70});

	const std::vector<std::uint8_t> Mask = {0, 1, 2, 0, 0, 255};
	CHECK(MaskToRgb(Mask) == std::vector<std::uint8_t>{0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255});
	// A picture larger than one stored block.
	std::vector<std::uint8_t> Wide(static_cast<std::size_t>(200) * 150 * 3, 0x5a);
	std::vector<TChunk> Large = Chunks(EncodePngRgb(200, 150, Wide));
	REQUIRE(Large.size() == 3);
	CHECK(Unstore(Large[1].Data).size() == static_cast<std::size_t>(150) * (200 * 3 + 1));
}
