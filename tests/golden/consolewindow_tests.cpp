// difftest.exe's ConsoleWindow test (moved here from tools/difftest/consolewindowtest.cpp, issue #28):
// ConsoleWindow.cpp against the original, as a golden-corpus test (golden.h).
//
// Every round builds two consoles, one from the image's constructor (0x422410)
// and one from ours, in memory with the same contents, sizes them and opens
// the default channel, as MainWindow.cpp does. Both get the same random
// operations: sizes, drawing (with Talk.cpp's change flag), keys (typing,
// sending the edit line with every kind of command and addressee, the
// selection keys, hotkeys, the history, Tab and Shift+Tab), the console's
// events, opening, adding and closing channels, the rule violation report, the
// server message switch, saving a channel to a file, mouse events in the tab
// bar (clicks and drags) and the text (selections, double and right clicks,
// the wheel), the tab bar's scroll events, the fake buttons' callbacks, and
// new and removed lines in the channels. After each one the logs and a dump of
// both window trees (geometry, drawing lists, captures and every member of
// the five classes, the formatted lines of the text windows with their talk
// lines, the hidden tabs, the fakes' state) are compared; at the end both are
// deleted and the logs compared again.
//
// TComponent, TElement, TWindow and TCallback are the image's for the
// original objects and ours for ours. The DialogElements.cpp classes the
// console creates (the edit line, the scroll bar and the three kinds of
// buttons) are the fakes below on both sides: the image's constructors and
// non-virtual members jump to them. They are also the definitions the other
// tests link (contextmenustest.cpp's edit line, dialogwindowtest.cpp's skin
// buttons) and log only while this test runs. The text buffers are test
// subclasses of Talk.cpp's TTalkBuffer, one set per side with the same lines
// (the image calls their virtual members and its own copies of GetNext,
// GetLine and Write). The other callees (Talk.cpp's functions, the
// preferences, Status.cpp, Help.cpp, the hotkeys, the text measurement and
// output, DrawSkin, the surface, the creatures and Communication.cpp's Send
// functions) are logging fakes for both sides. Failing assertions are not
// exercised: the inputs keep to what the game does (a channel to write in
// has the data its talk mode needs, a dragged tab stays in the bar, Talk.cpp's
// changes are followed by the console's change flag).
//
// By value (plan 4.3): the original's side exists in the oracle only (every use of it is inside ORIGINAL or
// #if GOLDEN_ORACLE); the runner has our console alone. Per comparison the corpus holds the original's log
// (text), its dump (text, by its changes since the last one, golden::OutputTextChanges), the results of its
// calls (and the file its SaveChannel wrote), how many logged fake calls its call made (the error round's
// failing call is drawn from them) and in the error round what each call threw. The world, the buffers and
// the operands are the test's, as before; the original's buffers exist in the oracle only. The DialogElements
// fakes defined here (the edit line, the scroll bar, the buttons) are elementfakes.h's, with our x64 layout
// in the runner. Our clock (time(), the CRT's _time64 since issue #62) is the runner's import slot, as in Talk's
// test (golden::FindImport).

#include "golden_test.h"

#include "elementfakes.h"
#include "golden_eh.h"
#include "testsurface.h"
#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#else
#include "golden_patch.h"
#endif

#include "Communication.h"
#include "ConsoleWindow.h"
#include "Creatures.h"
#include "GUI.h"
#include "Help.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Utils.h"
#include "graphics.h"

#include <direct.h>
#include <map>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <vector>

#if GOLDEN_ORACLE
static_assert(sizeof(TLineFormat) == 0x28, "TLineFormat is 0x28 bytes");
static_assert(offsetof(TLineFormat, FontNumber) == 0x1c && offsetof(TLineFormat, LineEnd) == 0x24, "TLineFormat layout");
static_assert(sizeof(TTextWindow) == 0x94, "TTextWindow is 0x94 bytes");
static_assert(offsetof(TTextWindow, Lines) == 0x28 && offsetof(TTextWindow, CurrentLineNumber) == 0x6c,
	"TTextWindow::Lines, CurrentLineNumber");
static_assert(offsetof(TTextWindow, SelectedText) == 0x8c && offsetof(TTextWindow, AtEnd) == 0x90, "TTextWindow layout");
static_assert(sizeof(TConsole) == 0x38 && offsetof(TConsole, ScrollToEnd) == 0x34, "TConsole layout");
static_assert(sizeof(TTab) == 0x58 && offsetof(TTab, Callback) == 0x4c && offsetof(TTab, Shortened) == 0x54,
	"TTab layout");
static_assert(sizeof(TTabBar) == 0x40 && offsetof(TTabBar, Busy) == 0x3c && offsetof(TTabBar, Changed) == 0x3d,
	"TTabBar layout");
static_assert(sizeof(TConsoleWindow) == 0x6c && offsetof(TConsoleWindow, History) == 0x48
		&& offsetof(TConsoleWindow, TalkMode) == 0x68,
	"TConsoleWindow layout");
static_assert(sizeof(TFakeEditLine) == 0x68 && offsetof(TFakeEditLine, Text) == 0x2c && offsetof(TFakeEditLine, Data60) == 0x60,
	"TEditLine layout");
static_assert(sizeof(TSkinButton) == 0x44 && sizeof(TToggleButton) == 0x50 && offsetof(TToggleButton, State) == 0x44,
	"button layouts");
static_assert(sizeof(TMultiStateButton) == 0x58, "TMultiStateButton is 0x58 bytes");
static_assert(sizeof(TScrollBar) == 0x38 && offsetof(TScrollBar, Range) == 0x2c && offsetof(TScrollBarRange, Value) == 0x30,
	"scroll bar layout");
#endif

// TFakeEditLine's Active (TEditLine::Active), a byte of Data30: +0x58 - 0x30 in the oracle.
static const size_t EditActive = offsetof(TEditLine, Active) - offsetof(TEditLine, MaxLength);

// The test's own definitions: in a namespace of their own (the other window tests, linked into the same
// executable, define classes of the same names at global scope). The fakes' members (elementfakes.h's
// classes) are global; they see the namespace through the using-directive below.
namespace ConsoleWindowTest {

// --- Logs and names ------------------------------------------------------------------

enum { OURS = 0,
	ORIGINAL = 1 };
// The sides that have objects: both in the oracle, ours in the runner.
static const int SIDES = GOLDEN_ORACLE ? 2 : 1;
static int Side;
static bool Testing; // the DialogElements fakes log only while this test runs
static std::string Log[2];

// The error round (issue #34): the FailAt-th logged call of a fake throws an
// assertion of the calling side (0: none); Ticks counts one side's calls.
static int FailAt, Ticks;
static void Tick()
{
	Ticks++;
	if (FailAt > 0 && --FailAt == 0)
		ThrowSideAssertion(Side == ORIGINAL, -Ticks);
}

static void Logf(const char *Format, ...)
{
	if (!Testing)
		return;
	Tick();
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

static void *VtableOf(const void *Object) { return *(void **)Object; }
static void SetVtable(void *Object, void *Vtable) { *(void **)Object = Vtable; }
template <class T> static void *Member(T Function) { return *(void **)&Function; }

// The objects of each side by role; the kinds of objects by vtable.
static std::map<const void *, std::string> Roles[2];
static std::map<void *, std::string> Kinds[2];
static int Serial[2];

static std::string Name(int s, const void *Object);
static std::string Name(const void *Object) { return Name(Side, Object); }
static void Role(const void *Object, const char *Base)
{
	char Buffer[64];
	sprintf(Buffer, "%s%d", Base, Serial[Side]++);
	Roles[Side][Object] = Buffer;
}

// --- DialogElements.cpp: the fakes of the classes the console creates ------------------

static void *TestEditVtable, *TestScrollVtable, *TestButtonVtable, *TestToggleVtable, *TestMultiVtable;
static int ScrollMinimumHeight;

// The vtable a fake gets: in the error round the image side's fakes call the
// image's TWindow or TElement code for what they do not override (imageeh.h,
// ImageSideVtable), named like ours.
static TErrorRound *ConsoleRound;
static void *OurWindowVtable, *OurElementVtable;
static void *FakeVtable(void *Ours, bool Window)
{
	if (ConsoleRound == NULL || Side != ORIGINAL)
		return Ours;
#if GOLDEN_ORACLE
	void *Hybrid = ImageSideVtable(Ours, Window ? OurWindowVtable : OurElementVtable, Window ? 0x564528 : 0x5645c8);
	Kinds[ORIGINAL][Hybrid] = Kinds[ORIGINAL][Ours];
	return Hybrid;
#else
	(void)Window;
	abort(); // the runner has no original side
#endif
}

} // namespace ConsoleWindowTest
using namespace ConsoleWindowTest;

TFakeEditLine::TFakeEditLine(TRect Area, int MaxLength, int Param)
{
	Text = new char[256];
	Text[0] = 0;
	memset(Data30, 0, sizeof(Data30));
	Data5C = -1;
	Data60 = 0;
	memset(Data64, 0, sizeof(Data64));
	Data66 = false;
	memset(Data67, 0, sizeof(Data67));
	if (Testing) {
		Role(this, "Edit");
		Logf("NEdit%d,%d,%d,%d,%d,%d;", Area.x, Area.y, Area.width, Area.height, MaxLength, Param);
		SetVtable(this, FakeVtable(TestEditVtable, true));
	}
}
void TFakeEditLine::SetActive(bool Active)
{
	Logf("EActive%d;", Active ? 1 : 0);
	Data30[EditActive] = Active ? 1 : 0;
	if (!Active)
		Data60 = 0;
}
void TFakeEditLine::SetText(const char *_Text)
{
	Logf("ESet%s;", _Text);
	strncpy(Text, _Text, 255);
	Text[255] = 0;
	Data5C = -1;
	Data60 = 0;
}
void TFakeEditLine::Clear()
{
	Logf("EClear;");
	Text[0] = 0;
	Data5C = -1;
	Data60 = 0;
}
void TFakeEditLine::UnknownA8() { Logf("EA8;"); }
void TFakeEditLine::UnknownAC() { Logf("EAC;"); }
void TFakeEditLine::UnknownB0() { Logf("EB0;"); }
void TFakeEditLine::UnknownB4() { Logf("EB4;"); }
void TFakeEditLine::SetDisabled(bool ReadOnly)
{
	Logf("EReadOnly%d;", ReadOnly ? 1 : 0);
	Data66 = ReadOnly;
}
void TFakeEditLine::UnknownBC() { Logf("EBC;"); }
void TFakeEditLine::SetFont(TFont Font)
{
	Logf("EFont%d,%d,%d,%d,%d;", Font.FontNumber, Font.Color.R, Font.Color.G, Font.Color.B, Font.Alignment);
}
namespace ConsoleWindowTest {

// The console's edit line: typing appends printable characters, Backspace
// deletes one.
class TTestEditLine : public TFakeEditLine {
public:
	TTestEditLine() : TFakeEditLine(TRect(0, 0, 0, 0), 255, 0) {}
	~TTestEditLine() { delete[] Text; }
	void Draw(int Surface, int x, int y) { Logf("DEdit%d,%d,%d,%dx%d;", Surface, x, y, Width, Height); }
	void KeyPress(int Key)
	{
		Logf("EKey%d;", Key);
		size_t Length = strlen(Text);
		if (Key >= 32 && Key < 127 && Length < 255) {
			Text[Length] = (char)Key;
			Text[Length + 1] = 0;
		} else if (Key == 8 && Length > 0) {
			Text[Length - 1] = 0;
		}
	}
	void HandleEvent(int Event) { Logf("EEvent%d;", Event); }
};

} // namespace ConsoleWindowTest

TFakeScrollBar::TFakeScrollBar(TRect Area, int Param2, int Param3, int Param4, int Param5, int Step)
{
	Range = new TFakeScrollBarRange;
	memset(Range, 0, sizeof(*Range));
	memset(Data30, 0, sizeof(Data30));
	SetSize(Area.width, Area.height);
	if (Testing) {
		Role(this, "Scroll");
		Logf("NScroll%d,%d,%d,%d,%d,%d,%d,%d,%d;", Area.x, Area.y, Area.width, Area.height, Param2, Param3, Param4,
			Param5, Step);
		SetVtable(this, FakeVtable(TestScrollVtable, true));
	}
	ReportElement(this, "ScrollBar", "%d,%d,%d,%d,%d,%d,%d,%d,%d", Area.x, Area.y, Area.width, Area.height, Param2,
		Param3, Param4, Param5, Step);
}
// The range is kept in Data00 for the wheel; the value stays inside it.
void TFakeScrollBarRange::SetRange(int Minimum, int Maximum, int _Value, int Visible)
{
	Logf("Range%d,%d,%d,%d;", Minimum, Maximum, _Value, Visible);
	((int *)Data00)[0] = Minimum;
	((int *)Data00)[1] = Maximum;
	if (_Value > Maximum)
		_Value = Maximum;
	if (_Value < Minimum)
		_Value = Minimum;
	Value = _Value;
}
namespace ConsoleWindowTest {

class TTestScrollBar : public TFakeScrollBar {
public:
	TTestScrollBar() : TFakeScrollBar(TRect(0, 0, 0, 0), 0, 0, 0, 0, 0) {}
	~TTestScrollBar() { delete Range; }
	int getMinimumHeight() { return ScrollMinimumHeight; }
	void Draw(int Surface, int x, int y) { Logf("DScroll%d,%d,%d,%dx%d;", Surface, x, y, Width, Height); }
	bool MouseWheel(int x, int y, int Direction)
	{
		Logf("WScroll%d,%d,%d;", x, y, Direction);
		int Minimum = ((int *)Range->Data00)[0], Maximum = ((int *)Range->Data00)[1];
		int Value = Range->Value + (Direction == 1 ? -1 : 1);
		if (Value >= Minimum && Value <= Maximum)
			Range->Value = Value;
		return true;
	}
};

} // namespace ConsoleWindowTest

// The button's callback is kept in Data2C (the test presses buttons).
TFakeSkinButton::TFakeSkinButton(int x, int y, int _ButtonUp, int _ButtonDown, TCallback Callback, const char *Tooltip,
	int Param)
{
	ButtonUp = _ButtonUp;
	ButtonDown = _ButtonDown;
	memset(Data2C, 0, sizeof(Data2C));
	memcpy(Data2C, &Callback, sizeof(Callback));
	SetPosition(x, y);
	SetSize(16, 16);
	if (Testing) {
		Role(this, "Button");
		Logf("NButton%d,%d,%d,%d,%s,%d,%s,%d;", x, y, _ButtonUp, _ButtonDown, Name(Callback.Receiver).c_str(),
			Callback.Event, Tooltip == NULL ? "-" : Tooltip, Param);
		SetVtable(this, FakeVtable(TestButtonVtable, false));
	}
	ReportElement(this, "SkinButton", "%d,%d,%d,%d,@%p/%d,%s,%d", x, y, _ButtonUp, _ButtonDown, (void *)Callback.Receiver,
		Callback.Event, Tooltip == NULL ? "-" : Tooltip, Param);
}
static TCallback &ButtonCallback(TSkinButton *Real) { return *(TCallback *)((TFakeSkinButton *)Real)->Data2C; }
TFakeToggleButton::TFakeToggleButton(int x, int y, int _ButtonUp, int _ButtonDown, TCallback Callback, const char *Tooltip1,
	const char *Tooltip2, int Param)
	: TFakeSkinButton(x, y, _ButtonUp, _ButtonDown, Callback, Tooltip1, Param)
{
	State = false;
	memset(Data45, 0, sizeof(Data45));
	if (Testing) {
		Logf("NToggle%s;", Tooltip2);
		SetVtable(this, FakeVtable(TestToggleVtable, false));
	}
	ReportElement(this, "ToggleButton", "%s,%s", Tooltip1 == NULL ? "-" : Tooltip1, Tooltip2 == NULL ? "-" : Tooltip2);
}
static void InitStates(TFakeMultiStateButton *Button, int States, va_list Arguments)
{
	Button->States = States;
	Button->ButtonUps = new int[States + 1];
	Button->ButtonDowns = new int[States + 1];
	for (int i = 0; i < States; i++) {
		Button->ButtonUps[i] = va_arg(Arguments, int);
		Button->ButtonDowns[i] = va_arg(Arguments, int);
		TCallback Callback = va_arg(Arguments, TCallback);
		Logf("State%d,%d,%s,%d;", Button->ButtonUps[i], Button->ButtonDowns[i], Name(Callback.Receiver).c_str(),
			Callback.Event);
	}
}
TFakeMultiStateButton::TFakeMultiStateButton(int x, int y, const char *Tooltip, int _States, ...)
	: TFakeSkinButton(x, y, 0, 0, TCallback(), Tooltip, 0)
{
	State = 0;
	memset(Data54, 0, sizeof(Data54));
	va_list Arguments;
	va_start(Arguments, _States);
	InitStates(this, _States, Arguments);
	va_end(Arguments);
	if (Testing)
		SetVtable(this, FakeVtable(TestMultiVtable, false));
}
namespace ConsoleWindowTest {

class TTestSkinButtonC : public TFakeSkinButton {
public:
	TTestSkinButtonC() : TFakeSkinButton(0, 0, 0, 0, TCallback(), NULL, 0) {}
	void Draw(int Surface, int x, int y)
	{
		Logf("D%s,%d,%d,%d,%d,%d;", Name(this).c_str(), Surface, x, y, ButtonUp, ButtonDown);
	}
};
class TTestToggleButton : public TFakeToggleButton {
public:
	TTestToggleButton() : TFakeToggleButton(0, 0, 0, 0, TCallback(), NULL, NULL, 0) {}
	void Draw(int Surface, int x, int y) { Logf("D%s,%d,%d,%d,%d;", Name(this).c_str(), Surface, x, y, State ? 1 : 0); }
};
class TTestMultiStateButton : public TFakeMultiStateButton {
public:
	TTestMultiStateButton() : TFakeMultiStateButton(0, 0, NULL, 0) {}
	~TTestMultiStateButton()
	{
		delete[] ButtonUps;
		delete[] ButtonDowns;
	}
	void Draw(int Surface, int x, int y) { Logf("D%s,%d,%d,%d,%d;", Name(this).c_str(), Surface, x, y, State); }
};

} // namespace ConsoleWindowTest

// Both sides' 0x4672d0 (elementfakes.cpp): a varargs constructor, __cdecl with this as its first argument (on
// x64 a member function's this comes first as well).
void *__cdecl NewMultiStateButton(void *This, int x, int y, const char *Tooltip, int States, ...)
{
	TFakeMultiStateButton *Button = new (This) TFakeMultiStateButton(x, y, Tooltip, 0);
	delete[] Button->ButtonUps;
	delete[] Button->ButtonDowns;
	va_list Arguments;
	va_start(Arguments, States);
	InitStates(Button, States, Arguments);
	va_end(Arguments);
	return Button;
}

namespace ConsoleWindowTest {

// --- The text buffers ------------------------------------------------------------------

struct TBufferSpec {
	int Recv, Send;
	bool Default, RuleViolation, RuleViolationChannel, PrivateChannel, Writable;
	int ChannelID;
	const char *Addressee;
	const char *Caption;
};

// A channel; the same buffer exists once per side (Index).
class TConsoleBuffer : public TTalkBuffer {
public:
	TConsoleBuffer(int _Index, const TBufferSpec &_Spec) : TTalkBuffer(_Spec.Recv), Index(_Index), Spec(_Spec)
	{
		SendTalkMode = _Spec.Send;
		Writable = _Spec.Writable;
		Closed = false;
	}
	bool CanWrite()
	{
		Logf("B%d.Can;", Index);
		return Writable;
	}
	bool SetWriteEnabled(bool Value)
	{
		Logf("B%d.SetW%d;", Index, Value ? 1 : 0);
		Writable = Value;
		return Value;
	}
	bool isDefault()
	{
		Logf("B%d.Def;", Index);
		return Spec.Default;
	}
	// The default channel and the gamemasters' rule violation channel (TRuleViolationChannel).
	bool AcceptMessages()
	{
		Logf("B%d.Acc;", Index);
		return Spec.Default || Spec.RuleViolationChannel;
	}
	bool IsRuleViolation()
	{
		Logf("B%d.RV;", Index);
		return Spec.RuleViolation;
	}
	bool IsRuleViolationChannel()
	{
		Logf("B%d.RVC;", Index);
		return Spec.RuleViolationChannel;
	}
	bool IsPrivateChannel()
	{
		Logf("B%d.Priv;", Index);
		return Spec.PrivateChannel;
	}
	const char *GetAddressee()
	{
		Logf("B%d.Addr;", Index);
		return Spec.Addressee;
	}
	int GetChannelID()
	{
		Logf("B%d.Chan;", Index);
		return Spec.ChannelID;
	}
	int Index;
	TBufferSpec Spec;
	bool Writable;
};
static std::vector<TConsoleBuffer *> Buffers[2];
static std::vector<bool> Deleted[2];

static const TBufferSpec Specs[] = {
	// Recv, Send, Default, RV, RVC, Priv, Writable, ChannelID, Addressee, Caption
	{1, 1, true, false, false, false, true, -1, NULL, "Default"},
	{5, 5, false, false, false, false, true, 4, NULL, "Game-Chat"},
	{5, 5, false, false, false, false, true, 5, NULL, "Trade"},
	{5, 5, false, false, false, false, false, 7, NULL, "Help"},
	{4, 4, false, false, false, false, true, -1, "Mr Very Long Name", "Mr Very Long Name"},
	{4, 4, false, false, false, false, true, -1, "Seymour", "Seymour"},
	{5, 5, false, false, false, true, true, 0x1234, NULL, "Tester's Channel"},
	{5, 5, false, false, false, true, false, 0x2345, NULL, "Bubble's Channel"},
	{7, 8, false, true, false, false, true, -1, NULL, "Report Rule Violation"},
	{8, 7, false, true, false, false, true, -1, "Xa", "Xa's Report"},
	{6, 6, false, false, true, false, false, 3, NULL, "Rule Violations"},
	{1, 1, false, false, false, false, true, -1, NULL, "A very long channel name that does not fit"},
	{10, 10, false, false, false, false, true, 9, NULL, "Gamemasters"},
};
enum { NSPECS = sizeof(Specs) / sizeof(Specs[0]) };

static const char *const Texts[] = {"Hello", "a b c d e f g h i j k l m n o p q r s t u v w x y z",
	"Averyveryverylongwordwithoutanyblanksinitatalltobreakitsomewhere and more",
	"line one\nline two\nline three", "  spaces  around  ", "x", "Two words\n\nempty line before",
	"Hello there, how are you doing today? Fine, thanks. And you?", "\xe4\xf6\xfc \xc4\xd6\xdc \xdf"};
enum { NTEXTS = sizeof(Texts) / sizeof(Texts[0]) };

[[maybe_unused]] static TTalkLine *CloneLine(const TTalkLine *Line) // the oracle's original buffers
{
	TTalkLine *Copy = (TTalkLine *)malloc(sizeof(TTalkLine));
	memcpy(Copy, Line, sizeof(TTalkLine));
	size_t Size = strlen(Line->Line) + 1;
	Copy->Line = (char *)malloc(Size);
	memcpy(Copy->Line, Line->Line, Size);
	Copy->WithoutTime = Copy->Line + (Line->WithoutTime - Line->Line);
	Copy->Text = Copy->Line + (Line->Text - Line->Line);
	Copy->Previous = Copy->Next = NULL;
	return Copy;
}

// A new channel on both sides (the same index), with a few lines.
static void AddRandomLines(int Index, int Count);
static int NewBufferPair(int SpecIndex)
{
	int Index = (int)Buffers[0].size();
	for (int s = 0; s < SIDES; s++) {
		Buffers[s].push_back(new TConsoleBuffer(Index, Specs[SpecIndex]));
		Deleted[s].push_back(false);
	}
	AddRandomLines(Index, Random() % 4);
	return Index;
}
static void AddRandomLines(int Index, int Count)
{
	static const int Modes[] = {1, 2, 3, 4, 5, 7, 8, 18, 19, 20, 22};
	for (int i = 0; i < Count; i++) {
		int Mode = Modes[Random() % 11];
		const char *Text = Texts[Random() % NTEXTS];
		// The constructor's arguments in the order the x86 oracle evaluated them, right to left (#55).
		TTalkLine *Line;
		if (Random() % 2 == 0) {
			Line = new TTalkLine(Random() % 1000, Mode, Text, 0);
		} else {
			const char *Speaker = Random() % 2 ? "Xa" : "Mr Very Long Name";
			Line = new TTalkLine(Random() % 1000, Mode, Speaker, Text, 0);
		}
		bool Quiet = Testing;
		Testing = false;
		Buffers[OURS][Index]->Append(Line);
		ORIGINAL(Buffers[ORIGINAL][Index]->Append(CloneLine(Line)));
		Testing = Quiet;
	}
}
static int LineIndex(const TTalkBuffer *Buffer, const TTalkLine *Line)
{
	int i = 0;
	for (TTalkLine *L = Buffer->first; L != NULL; L = L->Next, i++) {
		if (L == Line)
			return i;
	}
	return -1;
}

// --- The world and the fakes of the callees ----------------------------------------------

struct TConsoleWorld {
	int ActionNumber, ActionState;
	bool ConsoleChanged, Throttled, OwnPrivate, TimeStamps;
	const char *Account, *Password;
	int HotkeyNumber;
	bool HotkeyAuto;
	const char *HotkeyText;
	int OpenChannels[3], OpenCount;
	bool CreateFails;                            // CreateTextBuffer returns NULL, GetTextBuffer an existing buffer
	int PendingSpec;                             // the spec of the next CreateTextBuffer
	int Lookup;                                  // the buffer GetTextBuffer returns (-1 NULL)
	int LookupQueue[2], LookupCount, LookupNext; // answers before Lookup
};
static TConsoleWorld World;

static TCreature Player;

static TConsoleBuffer *Buffer(int Index) { return Index < 0 ? NULL : Buffers[Side][Index]; }
static int BufferIndex(const TTalkBuffer *TextBuffer)
{
	for (size_t i = 0; i < Buffers[Side].size(); i++) {
		if (Buffers[Side][i] == TextBuffer)
			return (int)i;
	}
	return -1;
}

static TRGBColor *__cdecl FakeGetTalkColor(TRGBColor *Color, int Mode)
{
	Logf("Color%d;", Mode);
	Color->R = Mode * 10;
	Color->G = Mode * 3;
	Color->B = 200;
	return Color;
}
static bool __cdecl FakeGetConsoleChanged()
{
	Logf("ConsChanged;");
	return World.ConsoleChanged;
}
static bool __cdecl FakeIsTalkThrottled()
{
	Logf("Throttled;");
	return World.Throttled;
}
static TTalkBuffer *__cdecl FakeCreateTextBuffer(int Recv, int Send, const char *Name, int Channel)
{
	Logf("Create%d,%d,%s,%d;", Recv, Send, Name == NULL ? "-" : Name, Channel);
	if (World.CreateFails)
		return NULL;
	// One new buffer per call and side; the indices agree because both sides make the same calls.
	int Index = (int)Buffers[Side].size();
	Buffers[Side].push_back(new TConsoleBuffer(Index, Specs[World.PendingSpec]));
	Deleted[Side].push_back(false);
	return Buffers[Side][Index];
}
static TTalkBuffer *__cdecl FakeGetTextBuffer(int Mode, const char *Name, int Channel)
{
	Logf("GetBuf%d,%s,%d;", Mode, Name == NULL ? "-" : Name, Channel);
	if (World.LookupNext < World.LookupCount)
		return Buffer(World.LookupQueue[World.LookupNext++]);
	return Buffer(World.Lookup);
}
static void __cdecl FakeDeleteTextBuffer(TTalkBuffer *TextBuffer)
{
	int Index = BufferIndex(TextBuffer);
	Logf("DelBuf%d;", Index);
	if (Index >= 0)
		Deleted[Side][Index] = true;
}
static void __cdecl FakeAddTextBufferLine(TTalkBuffer *Target, uint32 ID, const char *Text, int Mode, const char *Name,
	int Time)
{
	Logf("AddLine%d,%u,%s,%d,%s,%d;", BufferIndex(Target), ID, Text, Mode, Name == NULL ? "-" : Name, Time);
}
static bool __cdecl FakeHasOwnPrivateChannel()
{
	Logf("OwnPriv;");
	return World.OwnPrivate;
}
static void __cdecl FakeAddChannelNotice(const char *Addressee, const char *Text, int Mode, const char *Name, int Channel)
{
	Logf("Notice%s,%s,%d,%s,%d;", Addressee, Text, Mode, Name == NULL ? "-" : Name, Channel);
}
static void __cdecl FakeSetStatusMessage(const char *Message, bool Permanent)
{
	Logf("Status%s,%d;", Message, Permanent ? 1 : 0);
}
static bool __cdecl FakeGetTimeStamps() { return World.TimeStamps; }
static int CharWidth(char c) { return 4 + (uint8)c % 4; }
static int __cdecl FakeGetFittingTextLength(int Font, const char *Text, int Width)
{
	Logf("Fit%d,%d;", Font, Width);
	int i = 0, Sum = 0;
	while (Text[i] != 0 && Sum + CharWidth(Text[i]) <= Width)
		Sum += CharWidth(Text[i++]);
	return i;
}
static int __cdecl FakeGetTextPartWidth(int Font, const char *Text, int Length)
{
	Logf("PartW%d,%d;", Font, Length);
	int Sum = 0;
	for (int i = 0; i < Length; i++)
		Sum += CharWidth(Text[i]);
	return Sum;
}
static int __cdecl FakeGetFontHeight(int Font)
{
	Logf("FontH%d;", Font);
	return 11 + Font % 2;
}
static int __cdecl FakeGetFontLineHeight(int Font)
{
	Logf("LineH%d;", Font);
	return 13 + Font % 3;
}
static void __cdecl FakePrintTextPart(int Surface, int x, int y, int Font, TRGBColor Color, const char *Text, int Length,
	int Alignment)
{
	Logf("Print%d,%d,%d,%d,%d,%d,%d,%.*s,%d;", Surface, x, y, Font, Color.R, Color.G, Color.B, Length, Text, Alignment);
}
static void __cdecl FakePrintFormattedText(int Surface, int x, int y, TFont Format, const char *Text)
{
	Logf("PrintF%d,%d,%d,%d,%d,%d,%d,%d,%s;", Surface, x, y, Format.FontNumber, Format.Color.R, Format.Color.G,
		Format.Color.B, Format.Alignment, Text);
}
// DrawSkin is real on both sides (ours is inlined into TComponent::Draw); the
// picture parts it draws are logged.
static void __cdecl FakeDrawPicturePart(int Surface, int x, int y, int PictureNumber, int srcx, int srcy, int Width,
	int Height)
{
	Logf("Part%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, PictureNumber, srcx, srcy, Width, Height);
}
static int __cdecl FakeGetActionNumber() { return World.ActionNumber; }
static int __cdecl FakeGetActionState()
{
	Logf("AState;");
	return World.ActionState;
}
static void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	Logf("Msg%d", Type);
	if (Type == 0xc)
		Logf(",%s", (const char *)Param1);
	else
		Logf(",%d", (int)Param1);
	Logf(",%d;", (int)Param2);
}
static void __cdecl FakeCopyToClipboard(const char *Text) { Logf("Clip%s;", Text); }
static void __cdecl FakeSetMouseOverText(const char *Text) { Logf("Over%s;", Text); }
static void __cdecl FakeSetHelpHandle(int Handle) { Logf("HelpH%d;", Handle); }
static void __cdecl FakeSetHelpSize(int Width, int Height) { Logf("HelpS%d,%d;", Width, Height); }
static void __cdecl FakeMoveHelpPosition(int dx, int dy) { Logf("HelpM%d,%d;", dx, dy); }
static void __cdecl FakeAddHotkey(int Key, TCallback Callback)
{
	Logf("AddHK%x,%s,%d;", Key, Name(Callback.Receiver).c_str(), Callback.Event);
}
static void __cdecl FakeRemoveHotkey(int Key) { Logf("RemHK%x;", Key); }
static const char *__cdecl FakeGetHotkey(int Number, bool *AutoSend)
{
	Logf("GetHK%d;", Number);
	if (Number != World.HotkeyNumber)
		return NULL;
	*AutoSend = World.HotkeyAuto;
	return World.HotkeyText;
}
static int __cdecl FakeGetOpenChannels(int **List)
{
	Logf("GetOpen;");
	if (World.OpenCount > 0) {
		*List = new int[World.OpenCount];
		for (int i = 0; i < World.OpenCount; i++)
			(*List)[i] = World.OpenChannels[i];
	}
	return World.OpenCount;
}
static void __cdecl FakeClearOpenChannels() { Logf("ClearOpen;"); }
static void __cdecl FakeAddOpenChannel(int Channel) { Logf("AddOpen%d;", Channel); }
static void __cdecl FakeSetConsoleHeight(int Height) { Logf("SetCH%d;", Height); }
static const char *__cdecl FakeGetAccountNumberString()
{
	Logf("Acc;");
	return World.Account;
}
static const char *__cdecl FakeGetAccountPassword()
{
	Logf("Pw;");
	return World.Password;
}
static uint32 __cdecl FakeGetPlayerID()
{
	Logf("PID;");
	return Player.CreatureID;
}
static bool NoPlayer; // the error round: the player is not in the table
static TCreature *__cdecl FakeGetCreature(uint32 ID)
{
	Logf("Cr%u;", ID);
	return ID == Player.CreatureID && !NoPlayer ? &Player : NULL;
}
static void __cdecl FakeSendTalk1(int Mode, const char *Text) { Logf("Talk%d,%s;", Mode, Text); }
static void __cdecl FakeSendTalk2(int Mode, const char *Addressee, const char *Text) { Logf("TalkTo%d,%s,%s;", Mode, Addressee, Text); }
static void __cdecl FakeSendTalk3(int Mode, int Channel, const char *Text) { Logf("TalkIn%d,%d,%s;", Mode, Channel, Text); }
static void __cdecl FakeSendJoinChannel(int Channel) { Logf("Join%d;", Channel); }
static void __cdecl FakeSendLeaveChannel(int Channel) { Logf("Leave%d;", Channel); }
static void __cdecl FakeSendCloseRuleViolation(const char *Name) { Logf("CloseRV%s;", Name); }
static void __cdecl FakeSendCancelRuleViolation() { Logf("CancelRV;"); }
static void __cdecl FakeSendRequestChannels() { Logf("ReqChannels;"); }
static void __cdecl FakeSendInvite(const char *Name) { Logf("Invite%s;", Name); }
static void __cdecl FakeSendExclude(const char *Name) { Logf("Exclude%s;", Name); }
static void HookSetSkins(TSkinButton *Button, int ButtonUp, int ButtonDown)
{
	Logf("Skins%s,%d,%d;", Name(Button).c_str(), ButtonUp, ButtonDown);
	Button->ButtonUp = ButtonUp;
	Button->ButtonDown = ButtonDown;
}

} // namespace ConsoleWindowTest

#if GOLDEN_ORACLE
// Communication.cpp is not in the test: our side's definitions of the Send
// functions no other test defines. The runner has them as stubs (client_stubs.cpp) and redirects those.
void SendTalk(int Mode, const char *Text) { FakeSendTalk1(Mode, Text); }
void SendTalk(int Mode, const char *Addressee, const char *Text) { FakeSendTalk2(Mode, Addressee, Text); }
void SendTalk(int Mode, int ChannelID, const char *Text) { FakeSendTalk3(Mode, ChannelID, Text); }
void SendLeaveChannel(int ChannelID) { FakeSendLeaveChannel(ChannelID); }
void SendCancelRuleViolation() { FakeSendCancelRuleViolation(); }
void SendRequestChannels() { FakeSendRequestChannels(); }
#endif

namespace ConsoleWindowTest {

class TConsoleSurface : public TTestSurface {
public:
	void FillRectangle(int Surface, TRect R, TRGBColor C)
	{
		Logf("Fill%d,%d,%d,%d,%d,%d,%d,%d;", Surface, R.x, R.y, R.width, R.height, C.R, C.G, C.B);
	}
};
static TConsoleSurface *ConsoleSurface;
static TSurface *__cdecl FakeGetSurface() { return ConsoleSurface; }

// The clock (fixed on both sides), localtime, ctime and fprintf of the image.
static __time32_t __cdecl FakeTime(__time32_t *Time)
{
	if (Time != NULL)
		*Time = 1700000000;
	return 1700000000;
}
// Our clock (issue #62: time(), the CRT's _time64; before it _time32): the same fixed time.
static __time64_t __cdecl FakeTime64(__time64_t *Time)
{
	if (Time != NULL)
		*Time = 1700000000;
	return 1700000000;
}
#if GOLDEN_ORACLE
static tm *__cdecl ImageLocaltime(const __time32_t *Time) { return _localtime32(Time); }
static char *__cdecl ImageCtime(const __time32_t *Time) { return _ctime32(Time); }
static int __cdecl ImageFprintf(FILE *File, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vfprintf(File, Format, Args);
	va_end(Args);
	return Result;
}
static int __cdecl ImageSnprintf(char *Buffer, size_t Count, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = _vsnprintf(Buffer, Count, Format, Args);
	va_end(Args);
	return Result;
}
#endif

// Patches that are undone at the end.
struct TSaved {
	uint8 *Code;
	uint8 Bytes[5];
};
static std::vector<TSaved> SavedCode;
// The oracle patches the image and difftest.exe's code with PatchJump; the runner its own code with
// golden::RedirectOwn (golden_patch.h), saved and restored the same way.
static void Redirect(void *Function, void *Target)
{
	TSaved S;
	S.Code = (uint8 *)Function;
#if GOLDEN_ORACLE
	DWORD Protection;
	VirtualProtect(Function, 5, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(S.Bytes, Function, 5);
	PatchJump((uint32)(uintptr_t)Function, Target);
	VirtualProtect(Function, 5, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Function, 5);
#else
	golden::SaveOwn(Function, S.Bytes);
	golden::RedirectOwn(Function, Target);
#endif
	SavedCode.push_back(S);
}
#if !GOLDEN_ORACLE
// Our _time64 (time(), issue #62): the runner's import slot of ucrtbase's (Talk's test does the same).
static void **ClockSlot;
static void *SavedClock;
#endif
static void RestoreAll()
{
	for (size_t i = SavedCode.size(); i-- > 0;) {
#if GOLDEN_ORACLE
		DWORD Protection;
		VirtualProtect(SavedCode[i].Code, 5, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(SavedCode[i].Code, SavedCode[i].Bytes, 5);
		VirtualProtect(SavedCode[i].Code, 5, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), SavedCode[i].Code, 5);
#else
		golden::RestoreOwn(SavedCode[i].Code, SavedCode[i].Bytes);
#endif
	}
	SavedCode.clear();
#if !GOLDEN_ORACLE
	golden::SetImport(ClockSlot, SavedClock);
#endif
}

// Ours of the functions the oracle's difftest.exe defines in this file (the Send functions): NULL there, the
// runner's stubs (client_stubs.cpp) here.
#if GOLDEN_ORACLE
#define DEFINED_HERE(Function) NULL
#else
#define DEFINED_HERE(Function) ((void *)(Function))
#endif

static void PatchCallees()
{
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		{0x4fd9e0, (void *)GetTalkColor, (void *)FakeGetTalkColor},
		{0x4fd580, (void *)GetConsoleChanged, (void *)FakeGetConsoleChanged},
		{0x4fd550, (void *)IsTalkThrottled, (void *)FakeIsTalkThrottled},
		{0x507bc0, (void *)CreateTextBuffer, (void *)FakeCreateTextBuffer},
		{0x5025c0, (void *)GetTextBuffer, (void *)FakeGetTextBuffer},
		{0x507fa0, (void *)DeleteTextBuffer, (void *)FakeDeleteTextBuffer},
		{0x505f10, (void *)AddTextBufferLine, (void *)FakeAddTextBufferLine},
		{0x502880, (void *)HasOwnPrivateChannel, (void *)FakeHasOwnPrivateChannel},
		{0x506720, (void *)AddChannelNotice, (void *)FakeAddChannelNotice},
		{0x4ff500, (void *)SetStatusMessage, (void *)FakeSetStatusMessage},
		{0x4db5f0, (void *)GetTimeStamps, (void *)FakeGetTimeStamps},
		{0x491f50, (void *)GetFittingTextLength, (void *)FakeGetFittingTextLength},
		{0x491b80, (void *)GetTextPartWidth, (void *)FakeGetTextPartWidth},
		{0x491710, (void *)GetFontHeight, (void *)FakeGetFontHeight},
		{0x48b430, (void *)GetFontLineHeight, (void *)FakeGetFontLineHeight},
		{0x492e30, (void *)PrintTextPart, (void *)FakePrintTextPart},
		{0x493f60, (void *)PrintFormattedText, (void *)FakePrintFormattedText},
		{0x490c60, (void *)DrawPicturePart, (void *)FakeDrawPicturePart},
		{0x4eaa90, (void *)GetSurface, (void *)FakeGetSurface},
		{0x4e6e00, (void *)GetActionNumber, (void *)FakeGetActionNumber},
		{0x4e6e30, (void *)GetActionState, (void *)FakeGetActionState},
		{0x4e9020, (void *)AddMessage, (void *)FakeAddMessage},
		{0x4ea5b0, (void *)CopyToClipboard, (void *)FakeCopyToClipboard},
		{0x4e72e0, (void *)SetMouseOverText, (void *)FakeSetMouseOverText},
		{0x4a2a70, (void *)SetHelpHandle, (void *)FakeSetHelpHandle},
		{0x4a2a50, (void *)SetHelpSize, (void *)FakeSetHelpSize},
		{0x4a2a20, (void *)MoveHelpPosition, (void *)FakeMoveHelpPosition},
		{0x4a10a0, (void *)AddHotkey, (void *)FakeAddHotkey},
		{0x4a12a0, (void *)RemoveHotkey, (void *)FakeRemoveHotkey},
		{0x4dc7e0, (void *)GetHotkey, (void *)FakeGetHotkey},
		{0x4dbde0, (void *)GetOpenChannels, (void *)FakeGetOpenChannels},
		{0x4db610, (void *)ClearOpenChannels, (void *)FakeClearOpenChannels},
		{0x4dccb0, (void *)AddOpenChannel, (void *)FakeAddOpenChannel},
		{0x4db4b0, (void *)SetConsoleHeight, (void *)FakeSetConsoleHeight},
		{0x4e7230, (void *)GetAccountNumberString, (void *)FakeGetAccountNumberString},
		{0x4e7240, (void *)GetAccountPassword, (void *)FakeGetAccountPassword},
		{0x44b9c0, (void *)GetPlayerID, (void *)FakeGetPlayerID},
		{0x44c6e0, (void *)GetCreature, (void *)FakeGetCreature},
		// Communication.cpp: ours are this file's definitions (NULL) or other tests' (redirected).
		{0x4067c0, DEFINED_HERE((void (*)(int, const char *))SendTalk), (void *)FakeSendTalk1},
		{0x406b40, DEFINED_HERE((void (*)(int, const char *, const char *))SendTalk), (void *)FakeSendTalk2},
		{0x406f90, DEFINED_HERE((void (*)(int, int, const char *))SendTalk), (void *)FakeSendTalk3},
		{0x407650, DEFINED_HERE(SendLeaveChannel), (void *)FakeSendLeaveChannel},
		{0x407d10, DEFINED_HERE(SendCancelRuleViolation), (void *)FakeSendCancelRuleViolation},
		{0x407300, DEFINED_HERE(SendRequestChannels), (void *)FakeSendRequestChannels},
		{0x4074a0, (void *)SendJoinChannel, (void *)FakeSendJoinChannel},
		{0x407b60, (void *)SendCloseRuleViolation, (void *)FakeSendCloseRuleViolation},
		{0x408580, (void *)SendInviteToPrivateChannel, (void *)FakeSendInvite},
		{0x408730, (void *)SendExcludeFromPrivateChannel, (void *)FakeSendExclude},
		// The fixed clock (ours below: our CRT's _time64, for SaveChannel and the history lines; the runner's
		// import slot).
		{0x5121ea, NULL, (void *)FakeTime},
	};
	for (auto &T : Table) {
		ORIGINAL(Redirect((void *)(uintptr_t)T.Image, T.Fake));
		if (T.Ours != NULL)
			Redirect(T.Ours, T.Fake);
	}
	ORIGINAL(Redirect((void *)_time64, (void *)FakeTime64));
#if !GOLDEN_ORACLE
	ClockSlot = golden::FindImport("_time64");
	if (ClockSlot == nullptr) {
		printf("golden: ConsoleWindow: the runner imports no _time64\n");
		exit(1);
	}
	SavedClock = *ClockSlot;
	golden::SetImport(ClockSlot, (void *)FakeTime64);
#endif
	// DialogElements.cpp: both sides' constructors and members build and are the fakes
	// (elementfakes.cpp); TMultiStateButton::Reset is inline and runs for real.
	PatchElementFakes(Redirect);
	// The image's CRT reaches Windows: localtime, ctime, fprintf and _snprintf.
	ORIGINAL(Redirect((void *)(uintptr_t)0x513eb1, (void *)ImageLocaltime);
		Redirect((void *)(uintptr_t)0x5121d1, (void *)ImageCtime);
		Redirect((void *)(uintptr_t)0x512173, (void *)ImageFprintf);
		Redirect((void *)(uintptr_t)0x51233d, (void *)ImageSnprintf));
}

// --- The consoles ------------------------------------------------------------------------

#if GOLDEN_ORACLE
typedef void *(__fastcall *CtorFn)(void *, void *);
typedef void(__fastcall *VoidFn)(void *, void *);
typedef bool(__fastcall *BoolFn)(void *, void *);
typedef void(__fastcall *BoolArgFn)(void *, void *, bool);
typedef void(__fastcall *BufferStringFn)(void *, void *, TTalkBuffer *, const char *);
typedef TTalkBuffer *(__fastcall *OpenChannelFn)(void *, void *, int, int, const char *, int, const char *);
typedef void(__fastcall *StringFn)(void *, void *, const char *);
typedef TTalkBuffer *(__fastcall *BufferFn)(void *, void *);
typedef const char *(__fastcall *TextFn)(void *, void *);
typedef TTalkLine *(__fastcall *TalkLineFn)(void *, void *, int);
typedef void(__fastcall *PointerFn)(void *, void *, void *);
typedef void(__fastcall *TabFn)(void *, void *, void *);
typedef void *(__fastcall *TabCtorFn)(void *, void *, const char *, TTalkBuffer *, TCallback);
#endif

static TConsoleWindow *Console[2];
static TConsoleWindow *C(int s) { return Console[s]; }

static std::string Name(int s, const void *Object)
{
	if (Object == NULL)
		return "0";
	auto R = Roles[s].find(Object);
	if (R != Roles[s].end())
		return R->second;
	auto K = Kinds[s].find(VtableOf(Object));
	if (K != Kinds[s].end()) {
		if (K->second == "Tab") {
			char Buffer[32];
			int Saved = Side;
			Side = s;
			sprintf(Buffer, "Tab%d", BufferIndex(((const TTab *)Object)->TextBuffer));
			Side = Saved;
			return Buffer;
		}
		return K->second;
	}
	return "?";
}

static void NameParts(int s)
{
	TConsoleWindow *W = C(s);
	Roles[s][W] = "Console";
	Roles[s][W->TabBar] = "Bar";
	Roles[s][W->TabBar->LeftTabs] = "Left";
	Roles[s][W->TabBar->RightTabs] = "Right";
	Roles[s][W->CurrentConsole] = "View";
	Roles[s][W->CurrentConsole->TextWindow] = "Text";
	Roles[s][W->Input] = "Input";
	Roles[s][W->CurrentConsole->ScrollBar] = "Scroll";
	Roles[s][W->VolumeButton] = "Volume";
	Roles[s][W->CloseButton] = "Close";
	Roles[s][W->ServerMessagesButton] = "ServerMsgs";
	Roles[s][W->IgnoreButton] = "Ignore";
	Roles[s][W->NewChannelButton] = "NewChannel";
	Roles[s][W->MoveRightButton] = "MoveRight";
	Roles[s][W->MoveLeftButton] = "MoveLeft";
}

// The error round: the action whose calls fail (CurrentAction; 22 the
// constructor, 23 the default channel, 24 the destructor), the most fake calls
// one call of each action made, whether a call threw (the console is then
// left as it is: members may point to deleted parts).
static int CurrentAction, TargetAction = -1, MaxTicks[25], LastTicks[25];
static bool Threw;

static int FailIndex(int Count)
{
	int Choice = Random() % 3, Range = 1 << (Random() % 12);
	Range = Range < Count ? Range : Count;
	if (Choice == 0)
		return 1 + (int)(Random() % Count);
	if (Choice == 1)
		return 1 + (int)(Random() % Range);
	return Count - (int)(Random() % Range);
}

// Runs F for our side and then for the image's. In the error round the calls
// of the round's action fail at one of the fake calls such a call made
// before, and what both sides threw is compared; after a throw nothing runs.
template <class F> static void Both(F Function)
{
	if (ConsoleRound != NULL) {
		if (Threw)
			return;
		// Within the most calls of the action, or of its last call.
		int Count = Random() % 2 == 0 ? MaxTicks[CurrentAction] : LastTicks[CurrentAction];
		int Fail = Count > 0 && CurrentAction == TargetAction ? FailIndex(Count + 1) : 0;
		char What[64];
		sprintf(What, "action %d (fail at %d)", CurrentAction, Fail);
		ConsoleRound->Calling(What, "ours");
		TThrown A = OurThrow([&] { Side = OURS; Ticks = 0; FailAt = Fail; Function(OURS); });
		ConsoleRound->Calling(What, "the original");
		TThrown B = ORIGINAL_THROWN("thrown", Side = ORIGINAL; Ticks = 0; FailAt = Fail; Function(ORIGINAL));
		Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls
		FailAt = 0;
		Side = OURS;
		ConsoleRound->Compare(What, A, B);
		if (A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING) {
			Threw = true;
		} else {
			LastTicks[CurrentAction] = Ticks;
			if (Ticks > MaxTicks[CurrentAction])
				MaxTicks[CurrentAction] = Ticks;
		}
		return;
	}
	for (int s = 0; s < SIDES; s++) {
		Ticks = 0;
		Side = s;
		if (s == OURS) {
			// An exception on our side would end the child without a message.
			try {
				Function(s);
			} catch (TMyException &E) {
				printf("ConsoleWindow: our side threw: %s (%s)\n", E.GetMessage().c_str(), E.GetOrigin().c_str());
				fflush(stdout);
				abort();
			} catch (...) {
				printf("ConsoleWindow: our side threw\n");
				fflush(stdout);
				abort();
			}
		} else {
			Function(s);
		}
	}
	Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls (it ran last)
	if (Ticks > MaxTicks[CurrentAction])
		MaxTicks[CurrentAction] = Ticks;
	Side = OURS;
}

static char Pattern;
static void Build()
{
	Pattern = (char)Random();
	World.OpenCount = Random() % 4;
	for (int i = 0; i < World.OpenCount; i++)
		World.OpenChannels[i] = Random() % 20;
	Both([&](int s) {
		Serial[s] = 0;
		void *Memory = operator new(sizeof(TConsoleWindow));
		memset(Memory, Pattern, sizeof(TConsoleWindow));
		Roles[s][Memory] = "Console";
		if (s == OURS)
			Console[s] = new (Memory) TConsoleWindow;
		else
			ORIGINAL(Console[s] = (TConsoleWindow *)At<CtorFn>(0x422410)(Memory, NULL));
		NameParts(s);
	});
}

// --- Dumps ---------------------------------------------------------------------------------

static void Add(std::string &Out, const char *Format, ...)
{
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Out += Buffer;
}

// TTabBar::Changed is uninitialised until the first tab or size.
static bool DumpChanged;

static void DumpComponent(int s, std::string &Out, TComponent *Component, int Depth);
static void DumpChildren(int s, std::string &Out, TWindow *Window, int Depth)
{
	for (TComponent *Child = Window->FirstChild; Child != NULL; Child = Child->Next)
		DumpComponent(s, Out, Child, Depth + 1);
}

static void DumpText(int s, std::string &Out, TTextWindow *T)
{
	int Saved = Side;
	Side = s;
	Add(Out, " buf %d min %d max %d cur %d inv %d last %d sel %d,%d-%d,%d %d atend %d text %s",
		BufferIndex(T->TextBuffer), T->Lines.min, T->Lines.max, T->CurrentLineNumber, T->InvisibleLines,
		T->LastLineNumber, T->SelectionStartLine, T->SelectionStartOffset, T->SelectionEndLine, T->SelectionEndOffset,
		T->Selecting ? 1 : 0, T->AtEnd ? 1 : 0, T->SelectedText == NULL ? "-" : T->SelectedText.get());
	for (int i = T->Lines.min; i <= T->Lines.max; i++) {
		TLineFormat &F = T->Lines.entry[i - T->Lines.start];
		if (F.TextLine == NULL) {
			Add(Out, " [%d:%s,%d,%d,%d,%d,%d,%d,%d,%d]", i, F.LineText == NULL ? "-" : "?", F.LineLength,
				F.OriginalOffset, F.Indent, F.Color.R, F.Color.G, F.Color.B, F.FontNumber, F.LineEnd ? 1 : 0);
		} else if (LineIndex(T->TextBuffer, F.TextLine) < 0) {
			// A talk line removed since the lines were formatted (freed; not read).
			Add(Out, " [%d:gone,%d,%d,%d]", i, F.LineLength, F.OriginalOffset, F.Indent);
		} else {
			Add(Out, " [%d:%d@%d,%d,%d,%d,%d,%d,%d,%d,%d]", i, LineIndex(T->TextBuffer, F.TextLine),
				(int)(F.LineText - F.TextLine->Line), F.LineLength, F.OriginalOffset, F.Indent, F.Color.R, F.Color.G,
				F.Color.B, F.FontNumber, F.LineEnd ? 1 : 0);
		}
	}
	if (T->TextBuffer != NULL) {
		TTalkBuffer *B = T->TextBuffer;
		Add(Out, " flags %d%d%d%d%d%d lines %d cursor %d", B->Changed ? 1 : 0, B->Closed ? 1 : 0, B->Complete ? 1 : 0,
			B->LinesChanged ? 1 : 0, B->Locked ? 1 : 0, B->ShowMessages ? 1 : 0, B->lines, LineIndex(B, B->cursor));
	}
	Side = Saved;
}

static void DumpComponent(int s, std::string &Out, TComponent *Component, int Depth)
{
	Add(Out, "\n%*s%s %d,%d %dx%d", Depth * 2, "", Name(s, Component).c_str(), Component->OffsetX, Component->OffsetY,
		Component->Width, Component->Height);
	Add(Out, " parent %s", Name(s, Component->Parent).c_str());
	for (int i = 0; i < Component->DrawingListLength && Component->DrawingList != NULL; i++) {
		TDrawing &D = Component->DrawingList[i];
		Add(Out, " {%d,%d,%d,%d,%d,%d,%d}", D.skin, D.x, D.y, D.width, D.height, D.srcx, D.srcy);
	}
	auto K = Kinds[s].find(VtableOf(Component));
	std::string Kind = K == Kinds[s].end() ? "?" : K->second;
	if (Kind == "Console") {
		TConsoleWindow *W = (TConsoleWindow *)Component;
		Add(Out, " cap %s default %s switch %d hist %d mode %d", Name(s, W->Capture).c_str(),
			Name(s, W->DefaultTab).c_str(), W->SwitchTime, W->HistoryPosition, W->TalkMode);
		for (TTalkLine *L = W->History->first; L != NULL; L = L->Next)
			Add(Out, " h(%u,%d,%s,%s)", L->StatementID, L->Mode, L->Line, L->Text);
		Add(Out, " hcount %d", W->History->lines);
		// The buttons that are not children.
		TComponent *Parts[] = {W->VolumeButton, W->CloseButton, W->ServerMessagesButton, W->MoveRightButton,
			W->MoveLeftButton};
		for (TComponent *P : Parts) {
			if (P->Parent != W)
				Add(Out, " out %s %d,%d", Name(s, P).c_str(), P->OffsetX, P->OffsetY);
		}
		Add(Out, " volstate %d servermsgs %d", W->VolumeButton->State, W->ServerMessagesButton->State ? 1 : 0);
	} else if (Kind == "Bar") {
		TTabBar *B = (TTabBar *)Component;
		Add(Out, " cap %s moved %s cur %s busy %d changed %d", Name(s, B->Capture).c_str(), Name(s, B->Moved).c_str(),
			Name(s, B->CurrentTab).c_str(), B->Busy ? 1 : 0, DumpChanged ? (B->Changed ? 1 : 0) : -1);
		DumpChildren(s, Out, B, Depth);
		Add(Out, "\n%*sleft", Depth * 2 + 2, "");
		DumpChildren(s, Out, B->LeftTabs, Depth + 1);
		Add(Out, "\n%*sright", Depth * 2 + 2, "");
		DumpChildren(s, Out, B->RightTabs, Depth + 1);
		return;
	} else if (Kind == "View") {
		TConsole *V = (TConsole *)Component;
		Add(Out, " cap %s toend %d value %d", Name(s, V->Capture).c_str(), V->ScrollToEnd ? 1 : 0, V->ScrollBar->Range->Value);
	} else if (Kind == "Text") {
		DumpText(s, Out, (TTextWindow *)Component);
	} else if (Kind == "Tab") {
		TTab *T = (TTab *)Component;
		Add(Out, " '%s' '%s' lines %d drag %d,%d,%d,%d active %d unread %d blink %d cb %s/%d short %d", T->Caption.get(),
			T->FullCaption.get(), T->LineCount, T->Dragging, T->DragX, T->DragY, T->DragRight, T->Active ? 1 : 0,
			T->Unread ? 1 : 0, T->BlinkEnd, Name(s, T->Callback.Receiver).c_str(), T->Callback.Event,
			T->Shortened ? 1 : 0);
	} else if (Kind == "Edit") {
		TFakeEditLine *E = (TFakeEditLine *)Component;
		Add(Out, " '%s' %d,%d,%d,%d", E->Text, E->Data5C, E->Data60, E->Data66 ? 1 : 0, E->Data30[EditActive]);
	} else if (Kind == "Button" || Kind == "Toggle" || Kind == "Multi") {
		TSkinButton *B = (TSkinButton *)Component;
		Add(Out, " skins %d,%d", B->ButtonUp, B->ButtonDown);
	}
	if (Kind == "Console" || Kind == "View" || Kind == "Window")
		DumpChildren(s, Out, (TWindow *)Component, Depth);
}

static std::string Dump(int s)
{
	std::string Out;
	DumpComponent(s, Out, C(s), 0);
	return Out;
}

// The original's log of a comparison: the oracle's, recorded; the runner's from the corpus.
static void OutputOriginalLog()
{
	std::string Text = golden::OutputText("log", Log[ORIGINAL].data(), Log[ORIGINAL].size());
	ORIGINAL_OR((void)0, Log[ORIGINAL] = Text);
}

// The original's dump: the oracle's, recorded by its changes since the last one; the runner's from the
// corpus.
static std::string LastDump;
static std::string OriginalDump()
{
#if GOLDEN_ORACLE
	std::string Text = Dump(ORIGINAL);
#else
	std::string Text;
#endif
	return golden::OutputTextChanges("dump", &LastDump, Text);
}

static const char *Stage = "";
static void Compare(const char *What)
{
	std::string D0 = Threw ? "" : Dump(OURS), D1 = Threw ? "" : OriginalDump();
	OutputOriginalLog();
	CHECK(Log[OURS] == Log[ORIGINAL], "ConsoleWindow %s (%s): logs differ\n  ours: %s\n  orig: %s", What, Stage,
		Log[OURS].c_str(), Log[ORIGINAL].c_str());
	CHECK(D0 == D1, "ConsoleWindow %s (%s): windows differ\n  ours: %s\n  orig: %s", What, Stage, D0.c_str(), D1.c_str());
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

// --- Operations -----------------------------------------------------------------------------

static int TabCount(int s)
{
	int Count = 0;
	TTabBar *B = C(s)->TabBar;
	TWindow *Parts[] = {B, B->LeftTabs, B->RightTabs};
	for (TWindow *P : Parts) {
		for (TComponent *Child = P->FirstChild; Child != NULL; Child = Child->Next)
			Count++;
	}
	return Count;
}
static TTalkBuffer *CurrentBuffer(int s) { return C(s)->CurrentConsole->TextWindow->TextBuffer; }

// The buffers that have a tab (OURS's view; the image's must agree).
static std::vector<int> OpenBuffers()
{
	std::vector<int> List;
	int Saved = Side;
	Side = OURS;
	TTabBar *B = C(OURS)->TabBar;
	TWindow *Parts[] = {B, B->LeftTabs, B->RightTabs};
	for (TWindow *P : Parts) {
		for (TComponent *Child = P->FirstChild; Child != NULL; Child = Child->Next)
			List.push_back(BufferIndex(((TTab *)Child)->TextBuffer));
	}
	Side = Saved;
	return List;
}

static void RandomWorld()
{
	World.ActionNumber += 1 + Random() % 8;
	World.ActionState = Random() % 4 == 0 ? 11 : 0;
	World.Throttled = Random() % 3 == 0;
	World.OwnPrivate = Random() % 2 == 0;
	World.TimeStamps = Random() % 2 == 0;
	World.HotkeyNumber = Random() % 36;
	World.HotkeyAuto = Random() % 2 == 0;
	static const char *const Hotkeys[] = {"hi", "#y help", "*Seymour* hi there", "", "exura"};
	World.HotkeyText = Hotkeys[Random() % 5];
	static const char *const Accounts[] = {"1234567", "999"};
	static const char *const Passwords[] = {"secret", "", "pw"};
	World.Account = Accounts[Random() % 2];
	World.Password = Passwords[Random() % 3];
}

static void Draw()
{
	int Surface = Random() % 3, x = Random() % 30, y = Random() % 30;
	Both([&](int s) { C(s)->Draw(Surface, x, y); });
}

static const char *const Inputs[] = {"hello", "#w whisper this", "#y YELL", "#s say", "#b broadcast", "#a anon",
	"#c channel call", "#d anon call", "#i Seymour", "#x Seymour", "#I", "*Seymour* private", "*Tester* to me",
	"@Xa@ gm message", "$Xa$ anon message", "*unclosed", "** empty", "  ", "", "my account 1234567",
	"secret password", "#q unknown", "hi pw there", "a long message with many many words in it to send",
	"*Mr Very Long Name* hi"};
enum { NINPUTS = sizeof(Inputs) / sizeof(Inputs[0]) };

static void SetInput(const char *Text)
{
	if (getenv("CW_TRACE") != NULL)
		fprintf(stderr, "    input '%s' in buffer %d (spec caption %s, mode %d)\n", Text,
			BufferIndex(C(OURS)->CurrentConsole->TextWindow->TextBuffer),
			((TConsoleBuffer *)C(OURS)->CurrentConsole->TextWindow->TextBuffer)->Spec.Caption,
			C(OURS)->CurrentConsole->TextWindow->TextBuffer->SendTalkMode);
	Both([&](int s) {
		strcpy(C(s)->Input->Text, Text);
		C(s)->Input->SelectionStart = -1;
		C(s)->Input->SelectionLength = 0;
	});
}

static void Key()
{
	static const int Keys[] = {'a', 'b', ' ', 'Z', 8, 13, 13, 9, 0x209, 0x461, 0x510, 0x463, 0x311, 0x478, 0x511,
		0x100, 0x105, 0x10b, 0x300, 0x50a, 0x10f, 0x10f, 0x10c, 0x10c, 0x30f, 0x10d, 0x41, 0x1b};
	int Code = Keys[Random() % (sizeof(Keys) / sizeof(Keys[0]))];
	if (Code == 13 && Random() % 2 == 0)
		SetInput(Inputs[Random() % NINPUTS]);
	Both([&](int s) { C(s)->KeyPress(Code); });
}

static void Event()
{
	static const int Events[] = {201, 202, 203, 204, 205, 206, 207, 208, 210, 211, 212, 213, 403, 212, 213};
	int Code = Events[Random() % (sizeof(Events) / sizeof(Events[0]))];
	Both([&](int s) { C(s)->HandleEvent(Code); });
}

static void Open()
{
	int Spec = 1 + Random() % (NSPECS - 1);
	World.PendingSpec = Spec;
	World.CreateFails = Random() % 4 == 0 && !Buffers[OURS].empty();
	// When CreateTextBuffer fails, GetTextBuffer finds an existing buffer (one
	// with a tab or one without).
	World.Lookup = World.CreateFails ? (int)(Random() % Buffers[OURS].size()) : -1;
	if (World.CreateFails && Deleted[OURS][World.Lookup])
		World.CreateFails = false;
	const char *Name = Specs[Spec].Addressee;
	const char *Title = Specs[Spec].Caption;
	int Channel = Specs[Spec].ChannelID;
	Both([&](int s) {
		if (s == OURS)
			C(s)->OpenChannel(Specs[Spec].Recv, Specs[Spec].Send, Name, Channel, Title);
		else
			ORIGINAL(At<OpenChannelFn>(0x425070)(C(s), NULL, Specs[Spec].Recv, Specs[Spec].Send, Name, Channel, Title));
	});
	// A new buffer gets lines on both sides.
	if (!World.CreateFails)
		AddRandomLines((int)Buffers[OURS].size() - 1, Random() % 5);
}

static void AddExisting()
{
	// A buffer made directly (Control.cpp's message 0x32) or one with a tab.
	int Index = Random() % 2 == 0 || Buffers[OURS].empty() ? NewBufferPair(1 + Random() % (NSPECS - 1))
														   : (int)(Random() % Buffers[OURS].size());
	if (Deleted[OURS][Index])
		return;
	const char *Title = Buffers[OURS][Index]->Spec.Caption;
	Both([&](int s) {
		if (s == OURS)
			C(s)->AddChannel(Buffers[s][Index], Title);
		else
			ORIGINAL(At<BufferStringFn>(0x423060)(C(s), NULL, Buffers[s][Index], Title));
	});
}

static void Close()
{
	Both([&](int s) {
		if (s == OURS)
			C(s)->CloseChannel();
		else
			ORIGINAL(At<VoidFn>(0x4234b0)(C(s), NULL));
	});
}

static void Report()
{
	// Either the player's request is open (GetTextBuffer finds it) or a new
	// one is opened (GetTextBuffer finds nothing, then CreateTextBuffer's).
	World.PendingSpec = 8;
	World.CreateFails = false;
	int Existing = -1;
	std::vector<int> List = OpenBuffers();
	for (int b : List) {
		if (Buffers[OURS][b]->Spec.Recv == 7 && Buffers[OURS][b]->Spec.RuleViolation)
			Existing = b;
	}
	static const char *const Questions[] = {"How do I open a channel?", "x", "Help! Somebody is spamming."};
	const char *Question = Questions[Random() % 3];
	int New = (int)Buffers[OURS].size();
	Both([&](int s) {
		World.LookupQueue[0] = Existing;
		World.LookupQueue[1] = New;
		World.LookupCount = Existing >= 0 ? 1 : 2;
		World.LookupNext = 0;
		if (s == OURS)
			C(s)->ReportRuleViolation(Question);
		else
			ORIGINAL(At<StringFn>(0x425330)(C(s), NULL, Question));
	});
	World.LookupCount = 0;
}

static void ServerMessages()
{
	if (((TConsoleBuffer *)CurrentBuffer(OURS))->Spec.Default)
		return;
	if (Random() % 2 == 0) {
		bool Results[2] = {false, false};
		Both([&](int s) {
			Results[s] = s == OURS ? C(s)->GetServerMessages() : ORIGINAL_OR(At<BoolFn>(0x41f1c0)(C(s), NULL), false);
		});
		golden::Output("GetServerMessages", Results[ORIGINAL]);
		if (!Threw) // (the error round: a call that threw leaves no result)
			CHECK(Results[OURS] == Results[ORIGINAL], "ConsoleWindow GetServerMessages %d, original %d", Results[OURS],
				Results[ORIGINAL]);
	} else {
		bool Show = Random() % 2 == 0;
		Both([&](int s) {
			if (s == OURS)
				C(s)->SetServerMessages(Show);
			else
				ORIGINAL(At<BoolArgFn>(0x41f470)(C(s), NULL, Show));
		});
	}
}

static void TextBufferQuery()
{
	int Results[2] = {0, 0};
	Both([&](int s) {
		TTalkBuffer *B = s == OURS ? C(s)->GetTextBuffer() : ORIGINAL_OR(At<BufferFn>(0x41f7f0)(C(s), NULL), nullptr);
		Results[s] = BufferIndex(B);
	});
	golden::Output("GetTextBuffer", Results[ORIGINAL]);
	if (!Threw)
		CHECK(Results[OURS] == Results[ORIGINAL], "ConsoleWindow GetTextBuffer %d, original %d", Results[OURS],
			Results[ORIGINAL]);
}

static std::string TempDir;
static std::string ReadFile(const std::string &Path)
{
	std::string Text;
	FILE *File = fopen(Path.c_str(), "rb");
	if (File == NULL)
		return "(none)";
	char Buffer[512];
	size_t n;
	while ((n = fread(Buffer, 1, sizeof(Buffer), File)) > 0)
		Text.append(Buffer, n);
	fclose(File);
	return Text;
}
static void Save()
{
	TTab *Tab = C(OURS)->TabBar->CurrentTab;
	std::string File = std::string(Tab->getCaption()) + ".txt";
	char Old[512];
	_getcwd(Old, sizeof(Old));
	_chdir(TempDir.c_str());
	std::string Contents[2];
	bool Results[2] = {false, false};
	Both([&](int s) {
		remove(File.c_str());
		Results[s] = s == OURS ? C(s)->SaveChannel() : ORIGINAL_OR(At<BoolFn>(0x420090)(C(s), NULL), false);
		Contents[s] = ReadFile(File);
		remove(File.c_str());
	});
	_chdir(Old);
	golden::Output("SaveChannel", Results[ORIGINAL]);
	Contents[ORIGINAL] = golden::OutputText("SaveChannel file", Contents[ORIGINAL].data(), Contents[ORIGINAL].size());
	if (Threw)
		return;
	CHECK(Results[OURS] == Results[ORIGINAL], "ConsoleWindow SaveChannel %d, original %d", Results[OURS],
		Results[ORIGINAL]);
	CHECK(Contents[OURS] == Contents[ORIGINAL], "ConsoleWindow SaveChannel file\n  ours: %s\n  orig: %s",
		Contents[OURS].c_str(), Contents[ORIGINAL].c_str());
}

static void Active()
{
	bool A = Random() % 2 == 0;
	Both([&](int s) {
		if (s == OURS)
			C(s)->SetActive(A);
		else
			ORIGINAL(At<BoolArgFn>(0x41e960)(C(s), NULL, A));
	});
}

// New or removed lines in a channel (Talk.cpp), then the console's change flag.
static void TalkChanges()
{
	std::vector<int> List = OpenBuffers();
	int Index = List[Random() % List.size()];
	if (Random() % 3 != 0 || Buffers[OURS][Index]->lines == 0) {
		AddRandomLines(Index, 1 + Random() % 3);
	} else {
		// By pointer: Talk.cpp's Remove(int) only finds line 0.
		int Line = Random() % Buffers[OURS][Index]->lines;
		bool Quiet = Testing;
		Testing = false;
		for (int s = 0; s < SIDES; s++) {
			TTalkLine *L = Buffers[s][Index]->first;
			for (int i = 0; i < Line; i++)
				L = L->Next;
			Buffers[s][Index]->Remove(L);
		}
		Testing = Quiet;
	}
	World.ConsoleChanged = true;
	Draw();
	World.ConsoleChanged = false;
}

// A point in a part of the console (from our side's geometry).
static void PointIn(TComponent *Part, int &x, int &y)
{
	int px = 0, py = 0;
	for (TComponent *P = Part; P != NULL && P != C(OURS); P = P->Parent) {
		px += P->OffsetX;
		py += P->OffsetY;
	}
	x = px + (Part->Width > 0 ? (int)(Random() % (Part->Width + 4)) - 2 : 0);
	y = py + (Part->Height > 0 ? (int)(Random() % (Part->Height + 4)) - 2 : 0);
}

static void Mouse(int Kind, int x, int y, int Flags)
{
	int Direction = 1 + Random() % 2;
	Both([&](int s) {
		TConsoleWindow *W = C(s);
		switch (Kind) {
		case 0: W->LeftButtonDown(x, y, Flags); break;
		case 1: W->MouseMove(x, y); break;
		case 2: W->LeftButtonUp(x, y, Flags); break;
		case 3: W->LeftButtonDoubleClick(x, y, Flags); break;
		case 4: W->RightButtonUp(x, y, Flags); break;
		case 5: W->MouseWheel(x, y, Direction); break;
		case 6: W->HelpMouseMove(x, y); break;
		case 7: W->MouseHover(x, y); break;
		}
	});
}

// A drag: a press in the tab bar (no tab is hidden, so that the dragged tab
// stays in the bar) or in the text, a few moves, the release.
static void Drag()
{
	TTabBar *Bar = C(OURS)->TabBar;
	bool InBar = Random() % 2 == 0 && Bar->LeftTabs->FirstChild == NULL && Bar->RightTabs->FirstChild == NULL;
	int x, y;
	PointIn(InBar ? (TComponent *)Bar : (TComponent *)C(OURS)->CurrentConsole->TextWindow, x, y);
	Mouse(0, x, y, 0);
	Compare("press");
	int Moves = Random() % 5;
	for (int i = 0; i < Moves; i++) {
		x += (int)(Random() % 81) - 40;
		y += (int)(Random() % 21) - 10;
		Mouse(1, x, y, 0);
		Compare("drag");
	}
	Mouse(2, x, y, 0);
}

static void Click()
{
	TComponent *Parts[] = {C(OURS)->TabBar, C(OURS)->CurrentConsole->TextWindow, C(OURS), C(OURS)->Input,
		C(OURS)->CurrentConsole};
	int x, y;
	PointIn(Parts[Random() % 5], x, y);
	int Kind = 3 + Random() % 5;
	Mouse(Kind, x, y, 0);
	// A double click is followed by the release (a tab takes it as a press).
	if (Kind == 3) {
		Compare("double click");
		Mouse(2, x, y, 0);
	}
}

static void Press()
{
	// A button's callback (the arrows send their event to the tab bar).
	int Which = Random() % 7;
	Both([&](int s) {
		TConsoleWindow *W = C(s);
		TSkinButton *Buttons[] = {W->CloseButton, W->ServerMessagesButton, W->IgnoreButton, W->NewChannelButton,
			W->MoveRightButton, W->MoveLeftButton, W->IgnoreButton};
		TSkinButton *B = Buttons[Which];
		if (B == W->CloseButton && W->TabBar->CurrentTab == W->DefaultTab)
			return;
		// The side's own TCallback::Send (in the error round ours would catch
		// the image's exception and read it with our layout).
		if (s == OURS)
			ButtonCallback(B).Send();
		else
			ORIGINAL(At<void(__fastcall *)(void *, void *)>(0x4a0f00)(&ButtonCallback(B), NULL));
	});
}

static void BarEvent()
{
	static const int Events[] = {5, 6, 25, 26};
	int Code = Events[Random() % 4];
	Both([&](int s) { C(s)->TabBar->HandleEvent(Code); });
}

static void TextQueries()
{
	int What = Random() % 4;
	int Line = (int)(Random() % 12) - 1;
	std::string Results[2];
	Both([&](int s) {
		TTextWindow *T = C(s)->CurrentConsole->TextWindow;
		char Buffer[64];
		switch (What) {
		case 0: {
			const char *Text = s == OURS ? T->GetSelectedText() : ORIGINAL_OR(At<TextFn>(0x419bd0)(T, NULL), nullptr);
			Results[s] = Text == NULL ? "(null)" : Text;
			break;
		}
		case 1:
			Results[s] = (s == OURS ? T->SelectAll() : ORIGINAL_OR(At<BoolFn>(0x4199e0)(T, NULL), false)) ? "1" : "0";
			break;
		case 2: {
			TTalkLine *L = s == OURS ? T->GetTalkLine(Line) : ORIGINAL_OR(At<TalkLineFn>(0x419030)(T, NULL, Line), nullptr);
			sprintf(Buffer, "%d", L == NULL ? -2 : LineIndex(T->TextBuffer, L));
			Results[s] = Buffer;
			break;
		}
		case 3:
			Results[s] = (s == OURS ? T->HasSelection() : ORIGINAL_OR(At<BoolFn>(0x417590)(T, NULL), false)) ? "1" : "0";
			break;
		}
	});
	Results[ORIGINAL] = golden::OutputText("text query", Results[ORIGINAL].data(), Results[ORIGINAL].size());
	if (!Threw)
		CHECK(Results[OURS] == Results[ORIGINAL], "ConsoleWindow text query %d: %s, original %s", What,
			Results[OURS].c_str(), Results[ORIGINAL].c_str());
}

static void Size()
{
	int Width = 200 + Random() % 400, Height = 106 + Random() % 200;
	Both([&](int s) { C(s)->SetSize(Width, Height); });
}

static int ActionCount[24];
static void Action()
{
	RandomWorld();
	int A = Random() % 22;
	ActionCount[A]++;
	CurrentAction = A;
	if (getenv("CW_TRACE") != NULL)
		fprintf(stderr, "    kind %d\n", A);
	switch (A) {
	case 0:
		Size();
		Stage = "size";
		break;
	case 1:
		Both([&](int s) { C(s)->Arrange(); });
		Stage = "arrange";
		break;
	case 2:
	case 3:
		World.ConsoleChanged = Random() % 4 == 0;
		Draw();
		World.ConsoleChanged = false;
		Stage = "draw";
		break;
	case 4:
	case 5:
		Key();
		Stage = "key";
		break;
	case 6:
		SetInput(Inputs[Random() % NINPUTS]);
		Both([&](int s) { C(s)->KeyPress(13); });
		Stage = "send";
		break;
	case 7:
		Event();
		Stage = "event";
		break;
	case 8:
		if (TabCount(OURS) < 12) Open();
		Stage = "open";
		break;
	case 9:
		if (TabCount(OURS) < 12) AddExisting();
		Stage = "add";
		break;
	case 10:
		Close();
		Stage = "close";
		break;
	case 11:
		Report();
		Stage = "report";
		break;
	case 12:
		ServerMessages();
		Stage = "server messages";
		break;
	case 13:
		TextBufferQuery();
		Stage = "text buffer";
		break;
	case 14:
		Save();
		Stage = "save";
		break;
	case 15:
		Active();
		Stage = "active";
		break;
	case 16:
		TalkChanges();
		Stage = "talk";
		break;
	case 17:
		Drag();
		Stage = "drag";
		break;
	case 18:
		Click();
		Stage = "click";
		break;
	case 19:
		Press();
		Stage = "press";
		break;
	case 20:
		BarEvent();
		Stage = "bar";
		break;
	case 21:
		TextQueries();
		Stage = "text";
		break;
	}
}

// --- Error paths (issue #34) ---------------------------------------------------

// A console pair as the test builds it: the constructor, the size, the
// default channel (each can be the round's failing action).
static bool NewConsoles()
{
	Console[OURS] = Console[ORIGINAL] = NULL;
	for (int s = 0; s < 2; s++) {
		Roles[s].clear();
		Buffers[s].clear();
		Deleted[s].clear();
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();
	Threw = false;
	RandomWorld();
	World.ConsoleChanged = false;
	ScrollMinimumHeight = Random() % 120;
	CurrentAction = 22;
	Build();
	DumpChanged = false;
	Compare("errors: constructor");
	DumpChanged = true;
	if (Threw)
		return false;
	CurrentAction = 0;
	Size();
	int Default = NewBufferPair(0);
	CurrentAction = 23;
	Both([&](int s) {
		if (s == OURS)
			C(s)->OpenDefaultChannel(Buffers[s][Default], "Default");
		else
			ORIGINAL(At<BufferStringFn>(0x422e70)(C(s), NULL, Buffers[s][Default], "Default"));
	});
	Compare("errors: default channel");
	return !Threw;
}

// One call on both sides without failures (the assertions), what they threw
// compared; the console is left after a throw.
template <class F> static void ConsoleCall(const char *What, F Call, bool Dump = true, int Fail = 0)
{
	ConsoleRound->Calling(What, "ours");
	TThrown A = OurThrow([&] { Side = OURS; Ticks = 0; FailAt = Fail; Call(OURS); });
	ConsoleRound->Calling(What, "the original");
	TThrown B = ORIGINAL_THROWN("thrown", Side = ORIGINAL; Ticks = 0; FailAt = Fail; Call(ORIGINAL));
	FailAt = 0;
	Side = OURS;
	ConsoleRound->Compare(What, A, B);
	bool Thrown = A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING;
	Threw = Threw || Thrown || !Dump; // (no dump of a console with a member cleared for the call)
	Compare(What);
	Threw = Threw && (Thrown || Dump);
}

// Every PATELLA_ASSERT/TERROR of ConsoleWindow.cpp and the TEND blocks: the test's
// actions on new consoles, the calls of one action per round failing at one
// of the fake calls it made before, until one throws; then the assertions.
static void TestConsoleWindowErrors()
{
	TErrorRound R("ConsoleWindow");
	ConsoleRound = &R;
	for (int Round = 0; Round < 5000; Round++) {
		TargetAction = Round % 25;
		if (!NewConsoles())
			continue;
		int Actions = 5 + Random() % 20;
		for (int i = 0; i < Actions && !Threw; i++) {
			Action();
			Compare("errors: action");
		}
		if (Threw)
			continue;
		CurrentAction = 24;
		Both([&](int s) {
			delete C(s);
			Console[s] = NULL;
		});
		OutputOriginalLog();
		CHECK(Log[OURS] == Log[ORIGINAL], "ConsoleWindow errors destructor: logs differ\n  ours: %s\n  orig: %s",
			Log[OURS].c_str(), Log[ORIGINAL].c_str());
		Log[OURS].clear();
		Log[ORIGINAL].clear();
	}
	TargetAction = -1;
	// The assertions, on new consoles: each comes before any change (the
	// members set for one are restored after it).
	for (int Round = 0; Round < 40; Round++) {
		TargetAction = -1;
		if (!NewConsoles())
			continue;
		int Actions = Random() % 15;
		for (int i = 0; i < Actions && !Threw; i++) {
			Action();
			Compare("errors: action");
		}
		if (Threw)
			continue;
		int Bad = Random() % 2 ? -1 - (int)(Random() % 5) : 1000 + (int)(Random() % 5);
		ConsoleCall("TTextWindow::HandleEvent unknown", [&](int s) { C(s)->CurrentConsole->TextWindow->HandleEvent(Bad); });
		ConsoleCall("TTextWindow::SetCurrentLineNumber", [&](int s) {
			TTextWindow *T = C(s)->CurrentConsole->TextWindow;
			if (s == OURS) T->SetCurrentLineNumber(Bad);
			else ORIGINAL(At<void(__fastcall *)(void *, void *, int)>(0x4193d0)(T, NULL, Bad)); });
		ConsoleCall("TConsoleWindow::HandleEvent unknown", [&](int s) { C(s)->HandleEvent(Bad); });
		Threw = false;
		// Tabs without a caption or a buffer, and one that is in no bar.
		int Index = NewBufferPair(1 + Random() % (NSPECS - 1));
		ConsoleCall("TTab::TTab without a caption", [&](int s) {
			if (s == OURS) new TTab(NULL, Buffers[s][Index], TCallback());
			else ORIGINAL(At<TabCtorFn>(0x41a870)(operator new(sizeof(TTab)), NULL, NULL, Buffers[s][Index], TCallback())); });
		ConsoleCall("TTab::TTab without a buffer", [&](int s) {
			if (s == OURS) new TTab("x", NULL, TCallback());
			else ORIGINAL(At<TabCtorFn>(0x41a870)(operator new(sizeof(TTab)), NULL, "x", NULL, TCallback())); });
		Threw = false;
		TTab *Stranger[2] = {NULL, NULL};
		CurrentAction = 9;
		Both([&](int s) {
			if (s == OURS) Stranger[s] = new TTab("Stranger", Buffers[s][Index], TCallback());
			else ORIGINAL(Stranger[s] = (TTab *)At<TabCtorFn>(0x41a870)(operator new(sizeof(TTab)), NULL, "Stranger",
							  Buffers[s][Index], TCallback()));
		});
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		ConsoleCall("TTabBar::SetCurrentTab NULL", [&](int s) {
			if (s == OURS) C(s)->TabBar->SetCurrentTab(NULL); else ORIGINAL(At<TabFn>(0x421d10)(C(s)->TabBar, NULL, NULL)); });
		ConsoleCall("TTabBar::SetCurrentTab not in the bar", [&](int s) {
			if (s == OURS) C(s)->TabBar->SetCurrentTab(Stranger[s]);
			else ORIGINAL(At<TabFn>(0x421d10)(C(s)->TabBar, NULL, Stranger[s])); });
		ConsoleCall("TTabBar::GetContainer not in the bar", [&](int s) {
			if (s == OURS) C(s)->TabBar->GetContainer(Stranger[s]);
			else ORIGINAL(At<TabFn>(0x41cb20)(C(s)->TabBar, NULL, Stranger[s])); });
		ConsoleCall("TTabBar::Rearrange not in the bar", [&](int s) {
			if (s == OURS) C(s)->TabBar->Rearrange(Stranger[s]);
			else ORIGINAL(At<TabFn>(0x41d810)(C(s)->TabBar, NULL, Stranger[s])); });
		// The child functions with children TWindow's refuse (NULL, a stranger).
		ConsoleCall("TTabBar::InsertChild NULL", [&](int s) { C(s)->TabBar->InsertChild(NULL); });
		ConsoleCall("TTabBar::AppendChild NULL", [&](int s) { C(s)->TabBar->AppendChild(NULL); });
		ConsoleCall("TTabBar::InsertChildAfter NULL", [&](int s) { C(s)->TabBar->InsertChildAfter(C(s)->TabBar->CurrentTab, NULL); });
		ConsoleCall("TTabBar::DeleteChild stranger", [&](int s) { C(s)->TabBar->DeleteChild(Stranger[s]); });
		ConsoleCall("TTabBar::RemoveChild stranger", [&](int s) { C(s)->TabBar->RemoveChild(Stranger[s]); });
		ConsoleCall("TTabBar::SwapChildren NULL", [&](int s) { C(s)->TabBar->SwapChildren(NULL, Stranger[s]); });
		ConsoleCall("TTabBar::IsChild NULL", [&](int s) { C(s)->TabBar->IsChild(NULL); });
		Threw = false;
		int Neg = -1 - (int)(Random() % 9);
		bool NegX = Random() % 2 != 0;
		ConsoleCall("TTab::Draw outside", [&](int s) { C(s)->TabBar->CurrentTab->Draw(1, NegX ? Neg : 0, NegX ? 0 : Neg); });
		Threw = false;
		{
			decltype(C(OURS)->TabBar->Parent) Parent[2] = {C(OURS)->TabBar->Parent,
				ORIGINAL_OR(C(ORIGINAL)->TabBar->Parent, nullptr)};
			for (int s = 0; s < SIDES; s++)
				C(s)->TabBar->Parent = NULL;
			ConsoleCall("TTabBar::HelpMouseMove without a parent", [&](int s) { C(s)->TabBar->HelpMouseMove(3, 3); });
			for (int s = 0; s < SIDES; s++)
				C(s)->TabBar->Parent = Parent[s];
			Threw = false;
		}
		// The default channel's server messages.
		ConsoleCall("TConsoleWindow::GetServerMessages default", [&](int s) {
			if (s == OURS) C(s)->GetServerMessages(); else ORIGINAL(At<BoolFn>(0x41f1c0)(C(s), NULL)); });
		ConsoleCall("TConsoleWindow::SetServerMessages default", [&](int s) {
			if (s == OURS) C(s)->SetServerMessages(true); else ORIGINAL(At<BoolArgFn>(0x41f470)(C(s), NULL, true)); });
		ConsoleCall("TConsoleWindow::AddChannel NULL", [&](int s) {
			if (s == OURS) C(s)->AddChannel(NULL, "x");
			else ORIGINAL(At<BufferStringFn>(0x423060)(C(s), NULL, NULL, "x")); });
		ConsoleCall("TConsoleWindow::ReportRuleViolation NULL", [&](int s) {
			if (s == OURS) C(s)->ReportRuleViolation(NULL); else ORIGINAL(At<StringFn>(0x425330)(C(s), NULL, NULL)); });
		ConsoleCall("TConsoleWindow::ReportRuleViolation empty", [&](int s) {
			if (s == OURS) C(s)->ReportRuleViolation(""); else ORIGINAL(At<StringFn>(0x425330)(C(s), NULL, "")); });
		Threw = false;
		// Without the text buffer, the view, the edit line, the default tab, the
		// current tab or its caption.
		for (int What = 0; What < 13; What++) {
			TTalkBuffer *SavedBuffer[2];
			TConsole *SavedView[2];
			decltype(C(OURS)->Input) SavedInput[2];
			TTab *SavedDefault[2], *SavedCurrent[2];
			char *SavedCaption[2];
			for (int s = 0; s < SIDES; s++) {
				SavedBuffer[s] = C(s)->CurrentConsole->TextWindow->TextBuffer;
				SavedView[s] = C(s)->CurrentConsole;
				SavedInput[s] = C(s)->Input;
				SavedDefault[s] = C(s)->DefaultTab;
				SavedCurrent[s] = C(s)->TabBar->CurrentTab;
				SavedCaption[s] = SavedCurrent[s]->Caption.get();
				World.ConsoleChanged = What == 0 || What == 8; // (Draw asserts only then)
				if (What <= 7)
					C(s)->CurrentConsole->TextWindow->TextBuffer = NULL;
				else if (What == 8)
					C(s)->CurrentConsole = NULL;
				else if (What == 9)
					C(s)->Input = NULL;
				else if (What == 10)
					C(s)->DefaultTab = NULL;
				else if (What == 11)
					C(s)->TabBar->CurrentTab = NULL;
				else
					(void)SavedCurrent[s]->Caption.release(); // a std::unique_ptr since issue #58: taken out, not freed
			}
			static const char *const Names[] = {"Draw", "KeyPress", "SetActive", "UpdateButtons", "GetServerMessages",
				"SetServerMessages", "GetTextBuffer", "SaveChannel", "Draw without a view", "SetActive without an edit line",
				"CloseChannel without a default tab", "SaveChannel without a tab", "SaveChannel without a caption"};
			char Name[96];
			snprintf(Name, sizeof(Name), "TConsoleWindow::%s without a text buffer", Names[What]);
			// SaveChannel opens the channel's file before it asserts and leaves
			// it open when it throws: in a folder of the round's own, not the
			// working directory, nor Save()'s folder (an open file there cannot
			// be removed, and Save() would append to it).
			char OldDir[512];
			_getcwd(OldDir, sizeof(OldDir));
			std::string ErrorDir = TempDir + "\\errors";
			_mkdir(ErrorDir.c_str());
			_chdir(ErrorDir.c_str());
			ConsoleCall(What <= 7 ? Name : Names[What], [&](int s) {
				TConsoleWindow *W = C(s);
				switch (What) {
				case 0: case 8: W->Draw(1, 0, 0); break;
				case 1: W->KeyPress('a'); break;
				case 2: case 9: if (s == OURS) W->SetActive(true); else ORIGINAL(At<BoolArgFn>(0x41e960)(W, NULL, true)); break;
				case 3: if (s == OURS) W->UpdateButtons(); else ORIGINAL(At<VoidFn>(0x41ed00)(W, NULL)); break;
				case 4: if (s == OURS) W->GetServerMessages(); else ORIGINAL(At<BoolFn>(0x41f1c0)(W, NULL)); break;
				case 5: if (s == OURS) W->SetServerMessages(true); else ORIGINAL(At<BoolArgFn>(0x41f470)(W, NULL, true)); break;
				case 6: if (s == OURS) W->GetTextBuffer(); else ORIGINAL(At<BufferFn>(0x41f7f0)(W, NULL)); break;
				case 10: if (s == OURS) W->CloseChannel(); else ORIGINAL(At<VoidFn>(0x4234b0)(W, NULL)); break;
				default: if (s == OURS) W->SaveChannel(); else ORIGINAL(At<BoolFn>(0x420090)(W, NULL)); break;
				} }, false);
			_chdir(OldDir);
			World.ConsoleChanged = false;
			for (int s = 0; s < SIDES; s++) {
				C(s)->CurrentConsole = SavedView[s];
				C(s)->CurrentConsole->TextWindow->TextBuffer = SavedBuffer[s];
				C(s)->Input = SavedInput[s];
				C(s)->DefaultTab = SavedDefault[s];
				C(s)->TabBar->CurrentTab = SavedCurrent[s];
				if (SavedCurrent[s]->Caption.get() != SavedCaption[s])
					SavedCurrent[s]->Caption.reset(SavedCaption[s]);
			}
			Threw = false;
		}
		// Sending with talk modes the default channel's volume does not give.
		static const int Modes[] = {99, TALK_CHANNEL_CALL, TALK_PRIVATE_MESSAGE, TALK_GAMEMASTER_ANSWER,
			TALK_PRIVATE_MESSAGE, TALK_PLAYER_ANSWER};
		int Which = Round % 6;
		SetInput(Which == 4 ? "*Seymour* hi" : "hello there");
		NoPlayer = Which >= 4;
		int Mode = Modes[Which];
		ConsoleCall("TConsoleWindow::SendInput talk mode", [&](int s) {
			C(s)->TalkMode = Mode;
			C(s)->KeyPress(13);
		});
		NoPlayer = false;
		Threw = false; // (the assertions come before any change but the history)
		Both([&](int s) { C(s)->TalkMode = 1; });
		// The text window's release while selecting, a tab's while dragging,
		// without the capture; the bar with a negative width; a tab that is in
		// no bar shown; the console's scroll range with more invisible lines.
		ConsoleCall("TTextWindow::LeftButtonUp selecting", [&](int s) {
			TTextWindow *T = C(s)->CurrentConsole->TextWindow;
			T->Selecting = true;
			T->LeftButtonUp(0, 0, 0);
		});
		Both([&](int s) { C(s)->CurrentConsole->TextWindow->Selecting = false; });
		ConsoleCall("TTab::LeftButtonUp dragging", [&](int s) {
			TTab *T = C(s)->TabBar->CurrentTab;
			T->Dragging = true;
			T->LeftButtonUp(0, 0, 0);
		});
		Both([&](int s) { C(s)->TabBar->CurrentTab->Dragging = false; });
		int NegWidth = -1 - (int)(Random() % 9);
		ConsoleCall("TTabBar::SetSize negative", [&](int s) { C(s)->TabBar->SetSize(NegWidth, 20); });
		ConsoleCall("TTabBar::ShowTab not in the bar", [&](int s) {
			if (s == OURS) C(s)->TabBar->ShowTab(Stranger[s]);
			else ORIGINAL(At<TabFn>(0x41d450)(C(s)->TabBar, NULL, Stranger[s])); });
		Threw = false;
		{
			int Saved[2];
			ConsoleCall("TConsole::Arrange scroll range", [&](int s) {
				TTextWindow *T = C(s)->CurrentConsole->TextWindow;
				Saved[s] = T->InvisibleLines;
				T->InvisibleLines = T->getFirstLineNumber() - 1;
				C(s)->CurrentConsole->Arrange(); }, false);
			for (int s = 0; s < SIDES; s++)
				C(s)->CurrentConsole->TextWindow->InvisibleLines = Saved[s];
			Threw = false;
		}
		// Reports: a buffer without a tab, no buffer after opening it.
		{
			int Tabless = NewBufferPair(8);
			World.Lookup = Tabless;
			World.LookupCount = 0;
			ConsoleCall("TConsoleWindow::ReportRuleViolation without a tab", [&](int s) {
				if (s == OURS) C(s)->ReportRuleViolation("x"); else ORIGINAL(At<StringFn>(0x425330)(C(s), NULL, "x")); });
			World.Lookup = -1;
			Threw = false;
		}
		if (TabCount(OURS) < 10) {
			World.PendingSpec = 8;
			World.CreateFails = false;
			ConsoleCall("TConsoleWindow::ReportRuleViolation lost buffer", [&](int s) {
				World.LookupQueue[0] = World.LookupQueue[1] = -1;
				World.LookupCount = 2;
				World.LookupNext = 0;
				if (s == OURS) C(s)->ReportRuleViolation("x");
				else ORIGINAL(At<StringFn>(0x425330)(C(s), NULL, "x"));
			});
			World.LookupCount = 0;
			Threw = false; // (the new channel is open, the assertion follows)
		}
		// A gamemaster's answer without the player.
		if (TabCount(OURS) < 10) {
			int AnswerIndex = NewBufferPair(9);
			ConsoleCall("errors: answer channel", [&](int s) {
				if (s == OURS) C(s)->AddChannel(Buffers[s][AnswerIndex], "Answer");
				else ORIGINAL(At<BufferStringFn>(0x423060)(C(s), NULL, Buffers[s][AnswerIndex], "Answer")); });
			if (Threw)
				continue;
			ConsoleCall("errors: answer tab", [&](int s) {
				TTab *Tab = s == OURS ? C(s)->TabBar->FindTab(Buffers[s][AnswerIndex])
					: ORIGINAL_OR(At<TTab *(__fastcall *)(void *, void *, void *)>(0x41d270)(C(s)->TabBar, NULL, Buffers[s][AnswerIndex]), nullptr);
				if (s == OURS) C(s)->TabBar->SetCurrentTab(Tab); else ORIGINAL(At<TabFn>(0x421d10)(C(s)->TabBar, NULL, Tab)); });
			if (Threw)
				continue;
			// Its server messages, failing at the second fake call (the first in
			// HandleEvent(203), after isDefault).
			for (int Fail = 2; Fail <= 5; Fail++) {
				ConsoleCall("TConsoleWindow::SetServerMessages", [&](int s) {
					if (s == OURS) C(s)->SetServerMessages(true); else ORIGINAL(At<BoolArgFn>(0x41f470)(C(s), NULL, true)); }, true, Fail);
				Threw = false;
			}
			SetInput("an answer");
			NoPlayer = true;
			ConsoleCall("TConsoleWindow::SendInput answer without a player", [&](int s) { C(s)->KeyPress(13); });
			NoPlayer = false;
			Threw = false;
		}
		// The mouse over a shortened tab caption (the current tab's, marked so),
		// failing at SetMouseOverText.
		{
			bool Saved[2] = {C(OURS)->TabBar->CurrentTab->Shortened,
				ORIGINAL_OR(C(ORIGINAL)->TabBar->CurrentTab->Shortened, false)};
			ConsoleCall("TTab::MouseHover", [&](int s) {
				TTab *Tab = C(s)->TabBar->CurrentTab;
				Tab->Shortened = true;
				Tab->MouseHover(1, 1); }, false, 1);
			for (int s = 0; s < SIDES; s++)
				C(s)->TabBar->CurrentTab->Shortened = Saved[s];
			Threw = false;
		}
		// No buffer for a new channel or a report; no player for a report.
		World.CreateFails = true;
		World.Lookup = -1;
		World.LookupCount = 0;
		World.PendingSpec = 1;
		ConsoleCall("TConsoleWindow::OpenChannel without a buffer", [&](int s) {
			if (s == OURS) C(s)->OpenChannel(5, 5, NULL, 4, "Game-Chat");
			else ORIGINAL(At<OpenChannelFn>(0x425070)(C(s), NULL, 5, 5, NULL, 4, "Game-Chat")); });
		ConsoleCall("TConsoleWindow::ReportRuleViolation without a buffer", [&](int s) {
			if (s == OURS) C(s)->ReportRuleViolation("x"); else ORIGINAL(At<StringFn>(0x425330)(C(s), NULL, "x")); });
		World.CreateFails = false;
		Threw = false;
		if (TabCount(OURS) < 10) {
			// A new report (no request open) without the player.
			World.PendingSpec = 8;
			World.CreateFails = false;
			int New = (int)Buffers[OURS].size();
			NoPlayer = true;
			ConsoleCall("TConsoleWindow::ReportRuleViolation without a player", [&](int s) {
				World.LookupQueue[0] = -1;
				World.LookupQueue[1] = New;
				World.LookupCount = 2;
				World.LookupNext = 0;
				if (s == OURS) C(s)->ReportRuleViolation("x");
				else ORIGINAL(At<StringFn>(0x425330)(C(s), NULL, "x"));
			});
			NoPlayer = false;
			World.LookupCount = 0;
			if (Threw)
				continue;
		}
		// A player's report without a name: its tab's destructor asserts.
		if (Round % 4 == 0 && TabCount(OURS) < 10) {
			static const TBufferSpec Nameless = {8, 7, false, true, false, false, true, -1, NULL, "Nameless Report"};
			int ReportIndex = (int)Buffers[OURS].size();
			for (int s = 0; s < SIDES; s++) {
				Buffers[s].push_back(new TConsoleBuffer(ReportIndex, Nameless));
				Deleted[s].push_back(false);
			}
			ConsoleCall("TConsoleWindow::AddChannel nameless report", [&](int s) {
				if (s == OURS) C(s)->AddChannel(Buffers[s][Index], "Nameless");
				else ORIGINAL(At<BufferStringFn>(0x423060)(C(s), NULL, Buffers[s][Index], "Nameless")); });
			if (Threw)
				continue;
			ConsoleCall("TConsoleWindow::~TConsoleWindow nameless report", [&](int s) { delete C(s); });
			continue;
		}
	}
	// A chat channel's server messages, failing in HandleEvent(203); a player's
	// report without a name, whose tab's destructor asserts.
	for (int Round = 0; Round < 8; Round++) {
		if (!NewConsoles())
			continue;
		static const TBufferSpec Nameless = {8, 7, false, true, false, false, true, -1, NULL, "Nameless Report"};
		int Index = Round % 2 == 0 ? NewBufferPair(1) : (int)Buffers[OURS].size();
		if (Round % 2 != 0) {
			for (int s = 0; s < SIDES; s++) {
				Buffers[s].push_back(new TConsoleBuffer(Index, Nameless));
				Deleted[s].push_back(false);
			}
		}
		ConsoleCall("errors: channel", [&](int s) {
			if (s == OURS) C(s)->AddChannel(Buffers[s][Index], "Channel");
			else ORIGINAL(At<BufferStringFn>(0x423060)(C(s), NULL, Buffers[s][Index], "Channel"));
			TTab *Tab = s == OURS ? C(s)->TabBar->FindTab(Buffers[s][Index])
								  : ORIGINAL_OR(At<TTab *(__fastcall *)(void *, void *, void *)>(0x41d270)(C(s)->TabBar, NULL, Buffers[s][Index]), nullptr);
			if (s == OURS) C(s)->TabBar->SetCurrentTab(Tab);
			else ORIGINAL(At<void(__fastcall *)(void *, void *, void *)>(0x421d10)(C(s)->TabBar, NULL, Tab));
		});
		if (Threw)
			continue;
		if (Round % 2 != 0) {
			ConsoleCall("TConsoleWindow::~TConsoleWindow nameless report", [&](int s) { delete C(s); }, false);
			continue;
		}
		for (int Fail = 2; Fail <= 6; Fail++) {
			ConsoleCall("TConsoleWindow::SetServerMessages", [&](int s) {
				if (s == OURS) C(s)->SetServerMessages(Fail % 2 == 0);
				else ORIGINAL(At<BoolArgFn>(0x41f470)(C(s), NULL, Fail % 2 == 0)); }, true, Fail);
			Threw = false;
		}
	}
	Console[OURS] = Console[ORIGINAL] = NULL;
	Threw = false;
	ConsoleRound = NULL;
	R.Finish();
}

} // namespace ConsoleWindowTest

void TestConsoleWindow()
{
	using namespace ConsoleWindowTest;
	int Before = Checks, BeforeFailures = Failures;
	if (getenv("CW_TRACE") != NULL)
		setvbuf(stdout, NULL, _IONBF, 0);
	ORIGINAL(PatchImage(); PatchScript());
	InitSkins();
	ORIGINAL(Global<int>(0x71c450) = NumberOfSkins; Global<TSkin *>(0x71c454) = Skins.get());
	InitSkinProperties();
	ORIGINAL(At<void(__cdecl *)()>(0x498160)());
	LastDump.clear();
	ConsoleSurface = new TConsoleSurface;
	TempDir = std::string(getenv("TEMP") != NULL ? getenv("TEMP") : ".") + "\\consolewindow";
	_mkdir(TempDir.c_str());
	Player.CreatureID = 0x10000001;
	strcpy(Player.Name, "Tester");

	// The vtables of the fakes, from instances.
	{
		TTestEditLine *E = new TTestEditLine;
		TestEditVtable = VtableOf(E);
		delete E;
		TTestScrollBar *S = new TTestScrollBar;
		TestScrollVtable = VtableOf(S);
		delete S;
		TTestSkinButtonC *B = new TTestSkinButtonC;
		TestButtonVtable = VtableOf(B);
		delete B;
		TTestToggleButton *T = new TTestToggleButton;
		TestToggleVtable = VtableOf(T);
		delete T;
		TTestMultiStateButton *M = new TTestMultiStateButton;
		TestMultiVtable = VtableOf(M);
		delete M;
		TWindow *W = new TWindow;
		Kinds[OURS][VtableOf(W)] = "Window";
		OurWindowVtable = VtableOf(W);
		delete W;
		TElement *E2 = new TElement;
		OurElementVtable = VtableOf(E2);
		delete E2;
	}
	for (int s = 0; s < 2; s++) {
		Kinds[s][TestEditVtable] = "Edit";
		Kinds[s][TestScrollVtable] = "Scroll";
		Kinds[s][TestButtonVtable] = "Button";
		Kinds[s][TestToggleVtable] = "Toggle";
		Kinds[s][TestMultiVtable] = "Multi";
	}
	ORIGINAL(Kinds[ORIGINAL][(void *)(uintptr_t)0x55e1a0] = "Console";
		Kinds[ORIGINAL][(void *)(uintptr_t)0x55dcd8] = "Bar";
		Kinds[ORIGINAL][(void *)(uintptr_t)0x55dd80] = "Text";
		Kinds[ORIGINAL][(void *)(uintptr_t)0x55dfc0] = "View";
		Kinds[ORIGINAL][(void *)(uintptr_t)0x55e098] = "Tab";
		Kinds[ORIGINAL][(void *)(uintptr_t)0x564528] = "Window");
	PatchCallees();
	DialogFakes.SetSkins = HookSetSkins;
	{
		// Our vtables, from a console and a tab made quietly.
		World.OpenCount = 0;
		TConsoleWindow *W = new TConsoleWindow;
		Kinds[OURS][VtableOf(W)] = "Console";
		Kinds[OURS][VtableOf(W->TabBar)] = "Bar";
		Kinds[OURS][VtableOf(W->CurrentConsole)] = "View";
		Kinds[OURS][VtableOf(W->CurrentConsole->TextWindow)] = "Text";
		TConsoleBuffer *B = new TConsoleBuffer(-1, Specs[1]);
		TTab *T = new TTab("x", B, TCallback());
		Kinds[OURS][VtableOf(T)] = "Tab";
		delete T;
		delete W;
	}
	Testing = true;

	int Rounds = 300;
	for (int Round = 0; Round < Rounds; Round++) {
		if (getenv("CW_TRACE") != NULL)
			fprintf(stderr, "round %d\n", Round);
		RandomWorld();
		World.ConsoleChanged = false;
		Stage = "constructor";
		ScrollMinimumHeight = Random() % 120;
		Build();
		DumpChanged = false;
		Compare("constructor");
		DumpChanged = true;
		// MainWindow.cpp: the size, then the default channel.
		Size();
		int Default = NewBufferPair(0);
		Both([&](int s) {
			if (s == OURS)
				C(s)->OpenDefaultChannel(Buffers[s][Default], "Default");
			else
				ORIGINAL(At<BufferStringFn>(0x422e70)(C(s), NULL, Buffers[s][Default], "Default"));
		});
		Stage = "default channel";
		Compare("default channel");
		int Actions = 20 + Random() % 40;
		for (int i = 0; i < Actions; i++) {
			if (getenv("CW_TRACE") != NULL)
				fprintf(stderr, "  action %d\n", i);
			Action();
			Compare("action");
		}
		Stage = "destructor";
		Both([&](int s) {
			delete C(s);
			Console[s] = NULL;
		});
		OutputOriginalLog();
		CHECK(Log[OURS] == Log[ORIGINAL], "ConsoleWindow destructor: logs differ\n  ours: %s\n  orig: %s",
			Log[OURS].c_str(), Log[ORIGINAL].c_str());
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		for (int s = 0; s < 2; s++) {
			Roles[s].clear();
			// The buffers stay allocated (the tabs' destructors only report them).
			Buffers[s].clear();
			Deleted[s].clear();
		}
	}

	// The error round (issue #34): its checks are reported apart.
	int RoundChecks = Checks, SavedCounts[24];
	memcpy(SavedCounts, ActionCount, sizeof(SavedCounts));
	TestConsoleWindowErrors();
	memcpy(ActionCount, SavedCounts, sizeof(SavedCounts));
	RoundChecks = Checks - RoundChecks;
	for (int s = 0; s < 2; s++) {
		Roles[s].clear();
		Buffers[s].clear();
		Deleted[s].clear();
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();

	Testing = false;
	DialogFakes.SetSkins = NULL;
	RestoreAll();
	delete ConsoleSurface;
	ExitSkins();
	ORIGINAL(Global<TSkin *>(0x71c454) = NULL);
	int Least = ActionCount[0];
	for (int i = 1; i < 22; i++)
		Least = ActionCount[i] < Least ? ActionCount[i] : Least;
	printf("ConsoleWindow: %d checks, %d failures (each action at least %d times)\n", Checks - Before - RoundChecks,
		Failures - BeforeFailures, Least);
}
