// The test mode's input script (src/testmode/InputScript.h): the parser, the key table and the
// scheduler, and the driver's loop simulated on the virtual clock (src/testmode/TestModeWin32.cpp
// runs the same loop in the client's OnIdle).

#include <doctest/doctest.h>

#include "testmode/Clock.h"
#include "testmode/InputScript.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace patella::testmode;

namespace {

// A short form of an event: its time, kind, key or point, modifiers and buttons.
std::string Short(const TInputEvent &Event)
{
	std::string Text = std::to_string(Event.Time) + " ";
	auto Point = [&Event] {
		return std::string(Event.X.FromEnd ? "R-" : "") + std::to_string(Event.X.Value) + ","
			+ (Event.Y.FromEnd ? "B-" : "") + std::to_string(Event.Y.Value);
	};
	switch (Event.Kind) {
	case TEventKind::KeyDown: Text += "down " + std::to_string(Event.Key); break;
	case TEventKind::KeyUp: Text += "up " + std::to_string(Event.Key); break;
	case TEventKind::Char: Text += "char " + std::to_string(Event.Key); break;
	case TEventKind::MouseMove: Text += "move " + Point(); break;
	case TEventKind::LeftDown: Text += "ldown " + Point(); break;
	case TEventKind::LeftUp: Text += "lup " + Point(); break;
	case TEventKind::LeftDouble: Text += "ldouble " + Point(); break;
	case TEventKind::RightDown: Text += "rdown " + Point(); break;
	case TEventKind::RightUp: Text += "rup " + Point(); break;
	case TEventKind::RightDouble: Text += "rdouble " + Point(); break;
	case TEventKind::Wheel: Text += "wheel " + std::to_string(Event.Delta); break;
	case TEventKind::Quit: Text += "quit"; break;
	case TEventKind::StateDump: Text += "dump"; break;
	case TEventKind::FrameDump: Text += "frame"; break;
	case TEventKind::Advance: Text += "advance " + std::to_string(Event.Duration); break;
	case TEventKind::Post: Text += "post " + std::to_string(Event.Key); break;
	case TEventKind::Save: Text += "save " + Event.Name; break;
	}
	if (Event.Modifiers != 0)
		Text += " m" + std::to_string(Event.Modifiers);
	if (Event.Buttons != 0)
		Text += " b" + std::to_string(Event.Buttons);
	return Text;
}

std::vector<std::string> Parse(std::string_view Text)
{
	TParseResult Result = ParseScript(Text);
	REQUIRE_MESSAGE(Result.Ok, Result.ErrorLine, ": ", Result.Error);
	std::vector<std::string> Out;
	for (const TInputEvent &Event : Result.Script.Events)
		Out.push_back(Short(Event));
	return Out;
}

void CheckError(std::string_view Text, int Line, std::string_view Message)
{
	CAPTURE(Text);
	TParseResult Result = ParseScript(Text);
	CHECK_FALSE(Result.Ok);
	CHECK(Result.ErrorLine == Line);
	CHECK(Result.Error.find(Message) != std::string::npos);
	if (Result.Error.find(Message) == std::string::npos)
		MESSAGE("error: ", Result.Error);
}

using TLines = std::vector<std::string>;

// The driver's loop (TestModeWin32.cpp Idle) with pace 0: one event per call, otherwise the clock
// moves to the next tick or event, and a tick is a WM_TIMER. What the client would get, in order.
std::vector<std::string> Drive(TScript Script, std::uint32_t Until)
{
	TScheduler Scheduler(std::move(Script));
	TVirtualClock Clock;
	std::vector<std::string> Out;
	while (Clock.Elapsed() < Until) {
		if (const TInputEvent *Event = Scheduler.TakeDue(Clock.Elapsed())) {
			Out.push_back(Describe(*Event) + " at timeGetTime " + std::to_string(Clock.Milliseconds()));
			continue;
		}
		std::uint32_t Stop = Scheduler.NextStop(Clock.Elapsed());
		REQUIRE(Stop > Clock.Elapsed());
		REQUIRE(Scheduler.RealDeadline(Stop) == 0);
		REQUIRE(Clock.AdvanceTo(Stop));
		if (TScheduler::IsTick(Stop))
			Out.push_back("tick " + std::to_string(Stop));
	}
	return Out;
}

} // namespace

TEST_CASE("InputScript: an empty script; comments, blank lines, CRLF and a UTF-8 BOM")
{
	TParseResult Empty = ParseScript("");
	CHECK(Empty.Ok);
	CHECK(Empty.Script.Events.empty());
	CHECK(Empty.Script.RealPerTick == 100);
	CHECK(Parse("\xef\xbb\xbf# a comment\r\n\r\n   \t\r\nchar 65 # the letter A\r\n") == TLines{"0 char 65"});
	TParseResult WithLines = ParseScript("\n\nchar 1\n");
	REQUIRE(WithLines.Ok);
	CHECK(WithLines.Script.Events.at(0).Line == 3);
}

TEST_CASE("InputScript: wait, at and pace")
{
	CHECK(Parse("pace 0\nchar 1\nwait 250\nchar 2\nwait 50\nchar 3\nat 1000\nchar 4\nat 1000\nchar 5")
		== TLines{"0 char 1", "250 char 2", "300 char 3", "1000 char 4", "1000 char 5"});
	CHECK(ParseScript("pace 0").Script.RealPerTick == 0);
	CHECK(ParseScript("pace 250\nchar 1").Script.RealPerTick == 250);
	CHECK(ParseScript("wait 100\npace 50\nchar 1").Ok); // a wait is not an event
	CheckError("char 1\npace 50", 2, "pace comes before the first event");
	CheckError("wait 500\nat 400", 2, "earlier than the script's time 500");
	CheckError("wait -1", 1, "is not between 0 and");
	CheckError("wait 1000000000\nwait 1", 2, "passes 1000000000 ms");
	CheckError("wait", 1, "wait takes 1 argument, not 0");
	CheckError("wait 1 2", 1, "wait takes 1 argument, not 2");
	CheckError("wait 1x", 1, "not a number: 1x");
}

TEST_CASE("InputScript: key makes the keys and the character TranslateMessage would")
{
	// Return: VK_RETURN 13, its character 13.
	CHECK(Parse("key Return") == TLines{"0 down 13", "0 char 13", "0 up 13"});
	// The arrows have no character; they are extended keys (MainFrm.cpp TranslateKey).
	TParseResult Up = ParseScript("key Up");
	REQUIRE(Up.Ok);
	REQUIRE(Up.Script.Events.size() == 2);
	CHECK(Up.Script.Events[0].Key == 0x26);
	CHECK(Up.Script.Events[0].Scan == 0x48);
	CHECK(Up.Script.Events[0].Extended);
	// Ctrl+L: the logout hotkey; the character is 12, which OnChar turns into Ctrl+'l'.
	CHECK(Parse("key Ctrl+L") == TLines{"0 down 17 m2", "0 down 76 m2", "0 char 12 m2", "0 up 76 m2", "0 up 17"});
	CHECK(Parse("key Shift+Ctrl+Down")
		== TLines{"0 down 16 m1", "0 down 17 m3", "0 down 40 m3", "0 up 40 m3", "0 up 17 m1", "0 up 16"});
	CHECK(Parse("key shift+a") == TLines{"0 down 16 m1", "0 down 65 m1", "0 char 65 m1", "0 up 65 m1", "0 up 16"});
	CHECK(Parse("key 1") == TLines{"0 down 49", "0 char 49", "0 up 49"});
	CHECK(Parse("key Shift+1") == TLines{"0 down 16 m1", "0 down 49 m1", "0 char 33 m1", "0 up 49 m1", "0 up 16"});
	// Alt: no character (the client has no WM_SYSCHAR handler).
	CHECK(Parse("key Alt+F4") == TLines{"0 down 18 m4", "0 down 115 m4", "0 up 115 m4", "0 up 18"});
	// A modifier held with keydown is not pressed again, and stays held.
	CHECK(Parse("keydown Ctrl\nkey Ctrl+L\nkeyup Ctrl")
		== TLines{"0 down 17 m2", "0 down 76 m2", "0 char 12 m2", "0 up 76 m2", "0 up 17"});
	CheckError("key Ctrl+", 1, "unknown key: ");
	CheckError("key A+B", 1, "not a modifier (Shift, Ctrl, Alt): A");
	CheckError("key Hyper", 1, "unknown key: Hyper");
	CheckError("keydown Hyper", 1, "unknown key: Hyper");
}

TEST_CASE("InputScript: the key table")
{
	TKeyInfo Info;
	REQUIRE(FindKey("return", &Info));
	CHECK(Info.Key == 0x0d);
	CHECK(Info.Scan == 0x1c);
	REQUIRE(FindKey("Enter", &Info));
	CHECK(Info.Key == 0x0d);
	REQUIRE(FindKey("f1", &Info));
	CHECK(Info.Key == 0x70);
	CHECK(Info.Scan == 0x3b);
	REQUIRE(FindKey("F12", &Info));
	CHECK(Info.Key == 0x7b);
	CHECK(Info.Scan == 0x58);
	CHECK_FALSE(FindKey("F13", &Info));
	CHECK_FALSE(FindKey("F0", &Info));
	REQUIRE(FindKey("F", &Info)); // the letter
	CHECK(Info.Key == 'F');
	REQUIRE(FindKey("q", &Info));
	CHECK(Info.Key == 'Q');
	CHECK(Info.Scan == 0x10);
	REQUIRE(FindKey("M", &Info));
	CHECK(Info.Scan == 0x32);
	REQUIRE(FindKey("0", &Info));
	CHECK(Info.Key == '0');
	CHECK(Info.Scan == 0x0b);
	// Home on the block above the arrows is extended, on the keypad it is not (TranslateKey: 0x112 and
	// 0x118).
	REQUIRE(FindKey("Home", &Info));
	CHECK(Info.Extended);
	REQUIRE(FindKey("NumHome", &Info));
	CHECK(Info.Key == 0x24);
	CHECK_FALSE(Info.Extended);
	REQUIRE(FindKey("Control", &Info));
	CHECK(Info.Key == 0x11);
	CHECK_FALSE(FindKey("", &Info));

	CHECK(KeyCharacter('A', 0) == 'a');
	CHECK(KeyCharacter('A', ModifierShift) == 'A');
	CHECK(KeyCharacter('Z', ModifierControl) == 26);
	CHECK(KeyCharacter('A', ModifierAlt) == -1);
	CHECK(KeyCharacter('A', ModifierControl | ModifierAlt) == -1);
	CHECK(KeyCharacter('0', ModifierShift) == ')');
	CHECK(KeyCharacter('9', ModifierShift) == '(');
	CHECK(KeyCharacter('5', ModifierControl) == -1);
	CHECK(KeyCharacter(0x20, 0) == ' ');
	CHECK(KeyCharacter(0x0d, ModifierControl) == 10);
	CHECK(KeyCharacter(0x1b, ModifierControl) == 27);
	CHECK(KeyCharacter(0x09, 0) == 9);
	CHECK(KeyCharacter(0x09, ModifierControl) == -1);
	CHECK(KeyCharacter(0x08, 0) == 8);
	CHECK(KeyCharacter(0x08, ModifierControl) == 127);
	CHECK(KeyCharacter(0x26, 0) == -1);
	CHECK(KeyCharacter(0x70, 0) == -1);
}

TEST_CASE("InputScript: char and text")
{
	CHECK(Parse("char 0\nchar 255") == TLines{"0 char 0", "0 char 255"});
	CheckError("char 256", 1, "is not between 0 and 255");
	// text: one character each, no key messages (as tools/login_client.py types).
	CHECK(Parse("text \"hi you\"") == TLines{"0 char 104", "0 char 105", "0 char 32", "0 char 121", "0 char 111", "0 char 117"});
	CHECK(Parse("text \"\"").empty());
	// Escapes, and UTF-8 for the Latin-1 characters: e with acute accent is 0xe9.
	CHECK(Parse("text \"\\\"\\\\\\x41\\xff\xc3\xa9\" # comment")
		== TLines{"0 char 34", "0 char 92", "0 char 65", "0 char 255", "0 char 233"});
	// A '#' inside the string is text.
	CHECK(Parse("text \"#1\"") == TLines{"0 char 35", "0 char 49"});
	// A held Shift does not change text (the characters are given), but is recorded.
	CHECK(Parse("keydown Shift\ntext \"a\"\nkeyup Shift") == TLines{"0 down 16 m1", "0 char 97 m1", "0 up 16"});
	CheckError("text \"abc", 1, "no closing quote");
	CheckError("text abc", 1, "text takes a string in double quotes");
	CheckError("text \"\xe2\x82\xac\"", 1, "not Latin-1 (or not UTF-8)"); // the euro sign
	CheckError("text \"\xc4\x80\"", 1, "not Latin-1");                    // U+0100
	CheckError("text \"\\q\"", 1, "unknown escape: \\q");
	CheckError("text \"\\x4\"", 1, "\\x takes two hexadecimal digits");
	CheckError("text \"\\xg0\"", 1, "\\x takes two hexadecimal digits");
	CheckError("\n\ntext \"a\" \"b\"", 3, "text takes 1 argument, not 2");
}

TEST_CASE("InputScript: the mouse")
{
	CHECK(Parse("move 10 20") == TLines{"0 move 10,20"});
	// Enter Game in tools/login_client.py: (113, height - 220).
	CHECK(Parse("click 113 B-220") == TLines{"0 move 113,B-220", "0 ldown 113,B-220 b1", "0 lup 113,B-220"});
	CHECK(Parse("rclick R-5 7 Shift")
		== TLines{"0 down 16 m1", "0 move R-5,7 m1", "0 rdown R-5,7 m1 b2", "0 rup R-5,7 m1", "0 up 16"});
	CHECK(Parse("rclick 1 2 Shift+Ctrl")
		== TLines{"0 down 16 m1", "0 down 17 m3", "0 move 1,2 m3", "0 rdown 1,2 m3 b2", "0 rup 1,2 m3", "0 up 17 m1",
			"0 up 16"});
	// A modifier already held is not pressed again.
	CHECK(Parse("keydown Ctrl\nclick 1 2 Ctrl\nkeyup Ctrl")
		== TLines{"0 down 17 m2", "0 move 1,2 m2", "0 ldown 1,2 m2 b1", "0 lup 1,2 m2", "0 up 17"});
	CHECK(Parse("dclick 3 4") == TLines{"0 move 3,4", "0 ldown 3,4 b1", "0 lup 3,4", "0 ldouble 3,4 b1", "0 lup 3,4"});
	CHECK(Parse("rdclick 3 4") == TLines{"0 move 3,4", "0 rdown 3,4 b2", "0 rup 3,4", "0 rdouble 3,4 b2", "0 rup 3,4"});
	// Both buttons: the left one stays held over the right click.
	CHECK(Parse("ldown 5 5\nrclick 6 6\nlup 6 6")
		== TLines{"0 move 5,5", "0 ldown 5,5 b1", "0 move 6,6 b1", "0 rdown 6,6 b3", "0 rup 6,6 b1", "0 move 6,6 b1",
			"0 lup 6,6"});
	CHECK(Parse("rdown 1 1\nrup 1 1") == TLines{"0 move 1,1", "0 rdown 1,1 b2", "0 move 1,1 b2", "0 rup 1,1"});
	// The wheel acts at the last point.
	CHECK(Parse("move 9 9\nwheel -2\nwheel 1") == TLines{"0 move 9,9", "0 wheel -2", "0 wheel 1"});
	CheckError("wheel 0", 1, "other than 0");
	CheckError("wheel 201", 1, "is not between -200 and 200");
	CheckError("move 1", 1, "move takes 2 arguments, not 1");
	CheckError("move 5 R-3", 1, "not a number: R-3");
	CheckError("move B-5 3", 1, "not a number: B-5");
	CheckError("click 1 2 Shift Ctrl", 1, "click takes 2 to 3 arguments, not 4");
	CheckError("click 1 2 A", 1, "not a modifier (Shift, Ctrl, Alt): A");
	CheckError("move 100001 0", 1, "is not between -100000 and 100000");

	TCoordinate Bottom{220, true};
	CHECK(Bottom.Resolve(561) == 341);
	TCoordinate Left{-1, false};
	CHECK(Left.Resolve(800) == -1);
}

TEST_CASE("InputScript: quit, unknown commands, case")
{
	CHECK(Parse("wait 10\nQUIT") == TLines{"10 quit"});
	CheckError("quit now", 1, "quit takes 0 arguments, not 1");
	CheckError("char 1\n\njump 3", 3, "unknown command: jump");
}

TEST_CASE("InputScript: Describe, the driver's log line")
{
	TParseResult Result = ParseScript("wait 1200\nkey Ctrl+Up\nrclick R-10 B-20 Shift\nwheel -1\nquit");
	REQUIRE(Result.Ok);
	std::vector<std::string> Lines;
	for (const TInputEvent &Event : Result.Script.Events)
		Lines.push_back(Describe(Event));
	CHECK(Lines
		== TLines{"1200 ms, line 2: key down 0x11 scan 0x1d modifiers 2",
			"1200 ms, line 2: key down 0x26 scan 0x48 extended modifiers 2",
			"1200 ms, line 2: key up 0x26 scan 0x48 extended modifiers 2", "1200 ms, line 2: key up 0x11 scan 0x1d",
			"1200 ms, line 3: key down 0x10 scan 0x2a modifiers 1", "1200 ms, line 3: move R-10 B-20 modifiers 1",
			"1200 ms, line 3: right down R-10 B-20 modifiers 1 buttons 2", "1200 ms, line 3: right up R-10 B-20 modifiers 1",
			"1200 ms, line 3: key up 0x10 scan 0x2a", "1200 ms, line 4: wheel -1", "1200 ms, line 5: quit"});
}

TEST_CASE("TScheduler: one event at a time, then the next tick or event")
{
	TParseResult Result = ParseScript("char 1\nchar 2\nwait 250\nchar 3\nat 400\nchar 4");
	REQUIRE(Result.Ok);
	TScheduler Scheduler(std::move(Result.Script));
	CHECK(Scheduler.Remaining() == 4);
	const TInputEvent *First = Scheduler.TakeDue(0);
	REQUIRE(First != nullptr);
	CHECK(First->Key == 1);
	const TInputEvent *Second = Scheduler.TakeDue(0);
	REQUIRE(Second != nullptr);
	CHECK(Second->Key == 2);
	CHECK(Scheduler.TakeDue(0) == nullptr);
	CHECK(Scheduler.NextStop(0) == 100);
	CHECK(Scheduler.TakeDue(100) == nullptr);
	CHECK(Scheduler.NextStop(100) == 200);
	CHECK(Scheduler.NextStop(200) == 250);
	CHECK(Scheduler.TakeDue(249) == nullptr);
	const TInputEvent *Third = Scheduler.TakeDue(250);
	REQUIRE(Third != nullptr);
	CHECK(Third->Key == 3);
	CHECK(Scheduler.NextStop(250) == 300);
	CHECK(Scheduler.NextStop(300) == 400); // the tick and the event at 400
	// Late (the clock passed it): due at once.
	const TInputEvent *Fourth = Scheduler.TakeDue(450);
	REQUIRE(Fourth != nullptr);
	CHECK(Fourth->Key == 4);
	CHECK(Scheduler.Finished());
	CHECK(Scheduler.TakeDue(10000) == nullptr);
	CHECK(Scheduler.NextStop(450) == 500);
	CHECK(Scheduler.NextStop(500) == 600);

	CHECK_FALSE(TScheduler::IsTick(0));
	CHECK(TScheduler::IsTick(100));
	CHECK(TScheduler::IsTick(1000));
	CHECK_FALSE(TScheduler::IsTick(250));

	TScheduler Empty;
	CHECK(Empty.Finished());
	CHECK(Empty.NextStop(0) == 100);
	CHECK(Empty.NextStop(99) == 100);
}

TEST_CASE("TScheduler: the pace turns virtual into real milliseconds")
{
	TScheduler RealTime(ParseScript("char 1").Script);
	CHECK(RealTime.RealDeadline(0) == 0);
	CHECK(RealTime.RealDeadline(250) == 250);
	CHECK(RealTime.RealDeadline(60000) == 60000);
	TScheduler Half(ParseScript("pace 50").Script);
	CHECK(Half.RealDeadline(250) == 125);
	TScheduler Slow(ParseScript("pace 1000").Script);
	CHECK(Slow.RealDeadline(4000000000U) == 40000000000ULL);
	TScheduler Fast(ParseScript("pace 0").Script);
	CHECK(Fast.RealDeadline(4000000000U) == 0);
}

TEST_CASE("Driver loop: the same script gives the same input at the same virtual times, twice")
{
	const char *Text = "pace 0\n"
					   "wait 150\n"
					   "click 113 B-220\n"
					   "wait 50\n"
					   "text \"ab\"\n"
					   "at 300\n"
					   "key Ctrl+L\n"
					   "at 300\n"
					   "quit\n";
	TParseResult Result = ParseScript(Text);
	REQUIRE(Result.Ok);
	std::vector<std::string> Once = Drive(Result.Script, 500);
	std::vector<std::string> Again = Drive(ParseScript(Text).Script, 500);
	CHECK(Once == Again);
	CHECK(Once
		== TLines{"tick 100", "150 ms, line 3: move 113 B-220 at timeGetTime 100150",
			"150 ms, line 3: left down 113 B-220 buttons 1 at timeGetTime 100150",
			"150 ms, line 3: left up 113 B-220 at timeGetTime 100150", "tick 200",
			"200 ms, line 5: char 97 at timeGetTime 100200", "200 ms, line 5: char 98 at timeGetTime 100200",
			"tick 300", "300 ms, line 7: key down 0x11 scan 0x1d modifiers 2 at timeGetTime 100300",
			"300 ms, line 7: key down 0x4c scan 0x26 modifiers 2 at timeGetTime 100300",
			"300 ms, line 7: char 12 modifiers 2 at timeGetTime 100300",
			"300 ms, line 7: key up 0x4c scan 0x26 modifiers 2 at timeGetTime 100300",
			"300 ms, line 7: key up 0x11 scan 0x1d at timeGetTime 100300", "300 ms, line 9: quit at timeGetTime 100300",
			"tick 400", "tick 500"});
}

TEST_CASE("The scripts in tests/testmode parse")
{
	for (const char *Name : {"login-walk.script"}) {
		CAPTURE(Name);
		std::ifstream In(std::string(PATELLA_SOURCE_DIR) + "/tests/testmode/" + Name, std::ios::binary);
		REQUIRE(In.good());
		std::string Text((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
		TParseResult Result = ParseScript(Text);
		CHECK_MESSAGE(Result.Ok, Result.ErrorLine, ": ", Result.Error);
		CHECK_FALSE(Result.Script.Events.empty());
		CHECK(Result.Script.Events.back().Kind == TEventKind::Quit);
	}
}

TEST_CASE("TScheduler: the control channel's lines after the last event (issue #92)")
{
	TParseResult Start = ParseScript("clock manual\ncursor 1020 760\nadvance 2000 20\n");
	REQUIRE(Start.Ok);
	TScheduler Scheduler(std::move(Start.Script));
	const TInputEvent *First = Scheduler.TakeDue(0);
	REQUIRE(First != nullptr);
	CHECK(First->Kind == TEventKind::Advance);
	CHECK(Scheduler.Finished());
	// The driver parses a command's lines as a script of their own (with a manual clock after clock manual)
	// and appends them at the clock's time.
	TParseResult Lines = ParseScript("clock manual\npost 0x200 0 0x10001\nadvance 1000 10\nframe\n");
	REQUIRE(Lines.Ok);
	Scheduler.Append(std::move(Lines.Script.Events), 2000);
	CHECK_FALSE(Scheduler.Finished());
	CHECK(Scheduler.Remaining() == 3);
	CHECK(Scheduler.TakeDue(1999) == nullptr);
	const TInputEvent *Post = Scheduler.TakeDue(2000);
	REQUIRE(Post != nullptr);
	CHECK(Post->Kind == TEventKind::Post);
	CHECK(Post->Time == 2000);
	const TInputEvent *Advance = Scheduler.TakeDue(2000);
	REQUIRE(Advance != nullptr);
	CHECK(Advance->Kind == TEventKind::Advance);
	// The frame comes after the advance's time.
	CHECK(Scheduler.TakeDue(2000) == nullptr);
	const TInputEvent *Frame = Scheduler.TakeDue(3000);
	REQUIRE(Frame != nullptr);
	CHECK(Frame->Kind == TEventKind::FrameDump);
	CHECK(Frame->Time == 3000);
	CHECK(Scheduler.Finished());
	CHECK(Scheduler.ManualClock());
	CHECK(Scheduler.FixedCursor());
}
