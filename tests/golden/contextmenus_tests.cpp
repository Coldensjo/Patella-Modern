// difftest.exe's ContextMenus test (moved here from tools/difftest/contextmenustest.cpp, issue #28):
// ContextMenus.cpp against the original, as a golden-corpus test (golden.h).
//
// Every round builds one of the ten windows twice, from the image's
// constructors and from ours: the help bubble and the nine context menus.
// The callees from other files are logging fakes on both sides that answer
// from a random "world" (the looked-at and used objects, the creatures, the
// VIP list, the console lines, the preferences, the rights, the flags of
// object types, ...): Communication.cpp's Send functions, the getters and
// setters of Status.cpp, Creatures.cpp, Preferences.cpp, Talk.cpp,
// Inventory.cpp, Objects.cpp, Help.cpp and Map.cpp, the text measurement and
// output of graphics.cpp, DrawSkin, DrawPicturePart and the surface. The
// classes of the files that are not reconstructed are test objects shared by
// both sides: the console window and its text window (ConsoleWindow.cpp),
// the edit line, the formatted text and the separator (DialogElements.cpp;
// the image's constructors 0x465e60 and 0x452cb0 build the same fakes), the
// skill window's body (ContentWindows.cpp) and the text buffers. TWindow,
// TElement and TCallback are the image's for the original objects and ours
// for ours.
//
// After the constructor and after each of a few random operations (drawing
// with the mouse anywhere, clicks on the entries, right clicks, keys, every
// event of the menu's entries and, for the menus without "unknown event",
// arbitrary events; the help bubble's SetSize, Place, FitsIn and Arrange;
// AddEntry, AddSeparator and Show on a fresh menu) the logs and a dump of
// both windows (geometry, drawing lists, every member, the entries with
// their texts, shortcuts and callbacks) are compared; then both are deleted
// (the skill menu's destructor clears the body's pointer). The menus created
// by the rule violation entries are compared by their fields (the image's
// clock is a fake, so the time stamps are left out). Failing assertions and
// "unknown event" errors are not exercised.
//
// By value (plan 4.3): the original's side exists in the oracle only (every use of it is inside ORIGINAL or
// #if GOLDEN_ORACLE); the runner has our windows alone. Per comparison the corpus holds the original's log
// (text), its dump (text, by its changes since the last one, golden::OutputTextChanges), whether it has a
// window, the results of its calls and in the error round what each call threw (GOLDEN_ROUND_COMPARE). The
// world, the shared test objects and the operands are the test's, as before. ConsoleWindow.cpp's methods go
// to member-function stand-ins (GOLDEN_THISCALL, golden_test.h); the runner redirects its own functions with
// golden::RedirectOwn (restored at the end) and has Communication.cpp's Send functions as stubs
// (client_stubs.cpp), which the oracle's difftest.exe has here.

#include "golden_test.h"

#include "elementfakes.h"
#include "golden_eh.h"
#include "testsurface.h"
#include "golden_patch.h"

#include "Communication.h"
#include "ConsoleWindow.h"
#include "ContentWindows.h"
#include "ContextMenus.h"
#include "Creatures.h"
#include "GUI.h"
#include "Help.h"
#include "Inventory.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "sysinc.h"
#include "Surface.h"
#include "Talk.h"
#include "graphics.h"

#include <map>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>


// The test's own definitions: in a namespace of their own (the other window tests, linked into the same
// executable, define classes of the same names at global scope).
namespace ContextMenusTest {

// --- Logs and names ------------------------------------------------------------------

enum { OURS = 0,
	ORIGINAL = 1 };
// The sides that have objects: both in the oracle, ours in the runner.
static const int SIDES = 1; // ours alone (the oracle had the original's side too, until issue #94)
static int Side;
static std::string Log[2];

static bool Running; // the shared fakes (TSeparator) log only while this test runs
static void Logf(const char *Format, ...)
{
	if (!Running)
		return;
	char Buffer[512];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

static void *VtableOf(const void *Object) { return *(void **)Object; }
template <class T> static void *Member(T Function) { return *(void **)&Function; }

// The test objects of both sides, by role (one object per role and side for
// the per-side objects, NULL for the shared ones).
static std::map<const void *, std::string> Names[2];
[[maybe_unused]] static std::string Name(const void *Object) // unused (since before issue #28)
{
	if (Object == NULL)
		return "0";
	auto I = Names[Side].find(Object);
	return I == Names[Side].end() ? "?" : I->second;
}

static uint32 Hash(uint32 a, uint32 b)
{
	uint32 h = a * 2654435761u ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
	h ^= h >> 15;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	return h;
}
static uint32 HashText(const char *Text)
{
	uint32 h = 2166136261u;
	for (; *Text != 0; Text++)
		h = (h ^ (uint8)*Text) * 16777619u;
	return h;
}

// --- The world ---------------------------------------------------------------------------

static const char *const CreatureNames[] = {"Bubble", "Eternal Oblivion", "Xa", "Mr Very Long Namexx",
	"Seymour", "Cip Tester", "Lady Ohne Namen", "Avar Tar"};
enum { NCREATURES = 6,
	NLINES = 8,
	NBUDDIES = 5 };

class TTestBuffer;

struct TWorld {
	uint32 Seed;
	int MouseX, MouseY;
	TObject Trade, Use;
	int TradeIndex, TradeX, TradeY, TradeZ;
	int UseIndex, UseX, UseY, UseZ;
	uint32 TradeInfo;
	uint32 PlayerID, AttackID, FollowID;
	TCreature Creatures[NCREATURES];
	int PartyFlag;
	bool InParty, InFight, AnyRight, Classic, HideOffline, LevelBar, MagLevelBar, SkillBar[7];
	bool Unread, ClipboardEmpty, HasParent, TimeStamps, HasSelection, ServerMessages;
	int FreeContainer, Destination;
	int HelpX, HelpY, HelpWidth, HelpHeight;
	uint32 BuddyIDs[NBUDDIES];
	bool BuddyOnline[NBUDDIES];
	const char *BuddyNames[NBUDDIES];
	TTalkLine *Lines[NLINES];
	const char *Selection;
	bool RuleViolation, Selected, PrivateChannel, Default, RuleViolationChannel;
};
static TWorld World;
static TTestBuffer *Buffer;

// The console lines (TTalkLine without its constructors: the fields only).
static TTalkLine *NewLine(uint32 ID, int Mode, const char *Speaker, const char *Text)
{
	TTalkLine *L = (TTalkLine *)calloc(1, sizeof(TTalkLine));
	L->StatementID = ID;
	L->Mode = Mode;
	strcpy(L->Addressee, Speaker);
	char Line[200];
	sprintf(Line, "12:34 %s: %s", Speaker, Text);
	L->Line = _strdup(Line);
	L->WithoutTime = L->Line + 6;
	L->Text = L->WithoutTime + strlen(Speaker) + 2;
	return L;
}

static void RandomFlags()
{
	TWorld &W = World;
	W.Seed = Random();
	W.MouseX = (int)(Random() % 400) - 20;
	W.MouseY = (int)(Random() % 400) - 20;
	W.PartyFlag = Random() % 5;
	W.InParty = Random() % 2 != 0;
	W.InFight = Random() % 2 != 0;
	W.AnyRight = Random() % 2 != 0;
	W.Classic = Random() % 2 != 0;
	W.HideOffline = Random() % 2 != 0;
	W.LevelBar = Random() % 2 != 0;
	W.MagLevelBar = Random() % 2 != 0;
	for (int i = 0; i < 7; i++)
		W.SkillBar[i] = Random() % 2 != 0;
	W.Unread = Random() % 2 != 0;
	W.ClipboardEmpty = Random() % 2 != 0;
	W.HasParent = Random() % 2 != 0;
	W.TimeStamps = Random() % 2 != 0;
	W.HasSelection = Random() % 2 != 0;
	W.ServerMessages = Random() % 2 != 0;
	W.FreeContainer = Random() % 16;
	W.Destination = Random() % 4;
	W.RuleViolation = Random() % 3 == 0;
	W.Selected = Random() % 2 != 0;
	W.PrivateChannel = Random() % 2 != 0;
	W.Default = Random() % 3 == 0;
	W.RuleViolationChannel = Random() % 3 == 0;
	W.Selection = Random() % 3 == 0 ? NULL : "selected text";
	for (int i = 0; i < NCREATURES; i++)
		W.Creatures[i].PARTYFlag = Random() % 5;
	for (int i = 0; i < NBUDDIES; i++)
		W.BuddyOnline[i] = Random() % 2 != 0;
}

static void RandomWorld()
{
	TWorld &W = World;
	RandomFlags();
	static const int Types[] = {0, 99, 100, 101, 102, 1987, 2000, 3031, 2148, 1740};
	W.Trade.Type = Types[1 + Random() % 9];
	W.Trade.Data = Random() % 100;
	W.Trade.Data2 = Random() % 3;
	W.Use.Type = Types[Random() % 10];
	W.Use.Data = Random() % 100;
	W.Use.Data2 = 0;
	bool TradeInContainer = Random() % 2 != 0, UseInContainer = Random() % 2 != 0;
	W.TradeX = TradeInContainer ? 0xFFFF : 32000 + Random() % 100;
	W.TradeY = TradeInContainer ? (Random() % 3 == 0 ? Random() % 11 : 64 + Random() % 16) : 32000 + Random() % 100;
	W.TradeZ = Random() % 16;
	W.TradeIndex = Random() % 10;
	W.UseX = UseInContainer ? 0xFFFF : 32000 + Random() % 100;
	W.UseY = UseInContainer ? (Random() % 3 == 0 ? Random() % 11 : 64 + Random() % 16) : 32000 + Random() % 100;
	W.UseZ = Random() % 16;
	W.UseIndex = Random() % 10;
	for (int i = 0; i < NCREATURES; i++) {
		TCreature &C = W.Creatures[i];
		C.CreatureID = (i % 3 == 2 ? 0x40000000u : 0x10000000u) + Random() % 1000;
		strcpy(C.Name, CreatureNames[Random() % 8]);
	}
	W.PlayerID = W.Creatures[0].CreatureID;
	uint32 Some[] = {0, W.PlayerID, W.Creatures[1].CreatureID, W.Creatures[2].CreatureID,
		W.Creatures[3].CreatureID, 0x10000999u};
	W.TradeInfo = Some[Random() % 6];
	W.AttackID = Some[Random() % 6];
	W.FollowID = Some[Random() % 6];
	W.HelpX = Random() % 500;
	W.HelpY = Random() % 400;
	W.HelpWidth = Random() % 200;
	W.HelpHeight = Random() % 200;
	for (int i = 0; i < NBUDDIES; i++) {
		W.BuddyIDs[i] = i == 0 ? W.PlayerID : 0x10000000u + Random() % 1000;
		W.BuddyNames[i] = CreatureNames[Random() % 8];
	}
	static const int Modes[] = {1, 4, 5, 7, 8, 19};
	for (int i = 0; i < NLINES; i++) {
		const char *Speaker = i == 0 ? "" : i == 1 ? W.Creatures[0].Name
												   : CreatureNames[Random() % 8];
		// The arguments in the order the x86 oracle evaluated them, right to left (#55).
		int Mode = Modes[Random() % 6];
		uint32 ID = Random();
		W.Lines[i] = NewLine(ID, Mode, Speaker, "some words");
	}
}

static void FreeWorld()
{
	for (int i = 0; i < NLINES; i++) {
		free(World.Lines[i]->Line);
		free(World.Lines[i]);
		World.Lines[i] = NULL;
	}
}

// --- Shared test objects -----------------------------------------------------------------

class TTestBuffer : public TTalkBuffer {
public:
	TTestBuffer() : TTalkBuffer(1) {}
	void Clear() { Logf("BufClear;"); }
	bool SetWriteEnabled(bool Value)
	{
		Logf("BufWrite%d;", Value ? 1 : 0);
		return true;
	}
	bool isDefault()
	{
		Logf("BufDef;");
		return World.Default;
	}
	bool IsRuleViolation()
	{
		Logf("BufRV;");
		return World.RuleViolation;
	}
	bool IsRuleViolationChannel()
	{
		Logf("BufRVC;");
		return World.RuleViolationChannel;
	}
	bool IsPrivateChannel()
	{
		Logf("BufPriv;");
		return World.PrivateChannel;
	}
	bool GetSelected()
	{
		Logf("BufSel;");
		return World.Selected;
	}
};

// ConsoleWindow.cpp's text window and console (memory only: the menus read
// CurrentConsole, TextWindow and TextBuffer and call the methods below,
// which are fakes on both sides; TTextWindow::HasSelection, inlined in our
// code, runs on the text window's selection on both sides).
static TTextWindow *TextWindow;
static TConsole *ConsoleView;
static TConsoleWindow *Console;

static TTalkLine *GOLDEN_THISCALL FakeGetTalkLine(GOLDEN_THIS, int LineNumber)
{
	Logf("GetLine%d;", LineNumber);
	return LineNumber >= 0 && LineNumber < NLINES ? World.Lines[LineNumber] : NULL;
}
static const char *GOLDEN_THISCALL FakeGetSelectedText(GOLDEN_THIS)
{
	Logf("GetSel;");
	return World.Selection;
}
static bool GOLDEN_THISCALL FakeSelectAll(GOLDEN_THIS)
{
	Logf("SelAll;");
	return true;
}

// The world's selection on the text window (TTextWindow::HasSelection reads it).
static void SetSelection()
{
	TextWindow->SelectionStartLine = World.HasSelection ? 0 : -1;
	TextWindow->SelectionStartOffset = World.HasSelection ? 1 : -1;
	TextWindow->SelectionEndLine = World.HasSelection ? 1 : -1;
	TextWindow->SelectionEndOffset = World.HasSelection ? 2 : -1;
}

static bool GOLDEN_THISCALL FakeGetServerMessages(GOLDEN_THIS)
{
	Logf("ServerMsgs;");
	return World.ServerMessages;
}
static void GOLDEN_THISCALL FakeSetServerMessages(GOLDEN_THIS, bool Show) { Logf("ShowServerMsgs%d;", Show ? 1 : 0); }
static TTalkBuffer *GOLDEN_THISCALL FakeConsoleTextBuffer(GOLDEN_THIS)
{
	Logf("CurBuf;");
	return Buffer;
}
static bool GOLDEN_THISCALL FakeSaveChannel(GOLDEN_THIS)
{
	Logf("SaveWin;");
	return true;
}
static void GOLDEN_THISCALL FakeCloseChannel(GOLDEN_THIS) { Logf("CloseChan;"); }
static TTalkBuffer *GOLDEN_THISCALL FakeOpenChannel(GOLDEN_THIS, int RecvMode, int SendMode, const char *Name,
	int Channel, const char *Caption)
{
	Logf("OpenChan%d,%d,%s,%d,%s;", RecvMode, SendMode, Name, Channel, Caption);
	return NULL;
}

// DialogElements.cpp's formatted text (with one child, so that it has a
// size) and separator, the same objects for both sides: their constructors
// give them the vtables of the test classes below (Draw logs).
class TTestChild : public TElement {
public:
	void Draw(int Surface, int x, int y) { Logf("DChild%d,%d,%d;", Surface, x, y); }
};
static void *TextVtable, *SeparatorVtable;
static int TextChildWidth = 10, TextChildHeight = 10;

} // namespace ContextMenusTest

TFakeFormattedText::TFakeFormattedText(const char *_Text)
{
	using namespace ContextMenusTest;
	memset(Data2C, 0, sizeof(Data2C));
	Logf("NText%s;", _Text);
	TTestChild *Child = new TTestChild;
	Child->SetSize(TextChildWidth, TextChildHeight);
	Child->SetPosition(1, 2);
	AppendChild(Child);
	if (TextVtable != NULL)
		*(void **)this = TextVtable;
}

namespace ContextMenusTest {
// The separators are real on both sides (TSeparator's constructor is inline
// in DialogElements.h): SwapSeparators gives the new ones the test vtable.
static void *OurSeparatorVtable;
static void SwapSeparators(int s, TWindow *W)
{
	if (W == NULL || SeparatorVtable == NULL)
		return;
	void *Real = s == OURS ? OurSeparatorVtable : ORIGINAL_OR((void *)(uintptr_t)0x5610b0, nullptr);
	int Saved = Side;
	Side = s;
	for (TComponent *C = W->FirstChild; C != NULL; C = C->Next) {
		if (VtableOf(C) == Real) {
			Logf("NSep%d;", ((TSeparator *)C)->Horizontal ? 1 : 0);
			*(void **)C = SeparatorVtable;
		}
	}
	Side = Saved;
}
class TTestSeparator : public TSeparator {
public:
	TTestSeparator() : TSeparator(true) {}
	void Draw(int Surface, int x, int y) { Logf("DSep%d,%d,%d,%dx%d;", Surface, x, y, Width, Height); }
};
class TTestFormattedText : public TFakeFormattedText {
public:
	TTestFormattedText() : TFakeFormattedText("") {}
	void Draw(int Surface, int x, int y)
	{
		Logf("DText%d,%d,%d,%dx%d;", Surface, x, y, Width, Height);
		TWindow::Draw(Surface, x, y);
	}
};

// The edit line and the skill window's body, one per side.
class TTestEdit : public TFakeEditLine {
public:
	TTestEdit() : TFakeEditLine(TRect(0, 0, 0, 0), 0, 0) {}
	void HandleEvent(int Event) { Logf("EditEvent%d;", Event); }
};
// A stand-in of the skill window body (ContentWindows.cpp) with its layout up
// to ContextMenu: the menu sets ContextMenu and calls Arrange (+0x48).
class TMenuSkillsStandIn : public TWindow {
public:
	void Arrange() { Logf("SkillsArrange;"); }
	bool Minimized;                 // +0x2c
	TSkillContextMenu *ContextMenu; // +0x30
};
static_assert(offsetof(TMenuSkillsStandIn, ContextMenu) == offsetof(TSkillsWindowBody, ContextMenu),
	"the skill window stand-in has the body's layout");
static TTestEdit *Edit;
static TSkillsWindowBody *SkillsWindow[2];

// --- Fakes of the callees ------------------------------------------------------------------

static TCreature *FindCreature(uint32 ID)
{
	for (int i = 0; i < NCREATURES; i++) {
		if (World.Creatures[i].CreatureID == ID)
			return &World.Creatures[i];
	}
	return NULL;
}

static void __cdecl FakeSetActionState(int State) { Logf("Action%d;", State); }
static int __cdecl FakeGetMouseX() { return World.MouseX; }
static int __cdecl FakeGetMouseY() { return World.MouseY; }
static TObject *__cdecl FakeGetTradeObject(int *Index, int *x, int *y, int *z)
{
	Logf("GetTrade;");
	*Index = World.TradeIndex;
	*x = World.TradeX;
	*y = World.TradeY;
	*z = World.TradeZ;
	return &World.Trade;
}
static TObject *__cdecl FakeGetUseObject(int *Index, int *x, int *y, int *z)
{
	Logf("GetUse;");
	*Index = World.UseIndex;
	*x = World.UseX;
	*y = World.UseY;
	*z = World.UseZ;
	return &World.Use;
}
static int __cdecl FakeGetTradeInfo()
{
	Logf("TradeInfo;");
	return (int)World.TradeInfo;
}
static void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	Logf("Msg%d", Type);
	if (Type == 0x1c) {
		if (Param1 != 0) {
			Logf(",name %s", (const char *)Param1);
			delete[] (char *)Param1;
		}
		if (Param2 != 0) {
			TTalkLine *L = (TTalkLine *)Param2;
			Logf(",line %u,%d,%s,%s", L->StatementID, L->Mode, L->Addressee.Chars, L->Text);
			// The line's text was allocated by the side's operator new[] (the
			// image's goes to the same CRT).
			delete[] L->Line;
			free(L);
		}
	} else if (Type == 0x1d || Type == 0x1e) {
		TCoordinate *C = (TCoordinate *)Param1;
		Logf(",coord %d,%d,%d", C->x, C->y, C->z);
		delete C;
		if (Param2 != 0) {
			TMark *M = (TMark *)Param2;
			Logf(",mark %d,%d,%d,%s", M->x, M->y, M->Type, M->Text.Chars);
			delete M;
		}
	} else {
		Logf(",%d,%d", (int)Param1, (int)Param2);
	}
	Logf(";");
}
static void __cdecl FakeCopyToClipboard(const char *Text) { Logf("Clip%s;", Text); }
static bool __cdecl FakeCheckAnyRight()
{
	Logf("Right;");
	return World.AnyRight;
}
static uint32 __cdecl FakeGetPlayerID()
{
	Logf("PID;");
	return World.PlayerID;
}
static TCreature *__cdecl FakeGetCreature(uint32 ID)
{
	Logf("Cr%u;", ID);
	return FindCreature(ID);
}
static bool __cdecl FakeIsPlayer(uint32 ID)
{
	Logf("IsPl%u;", ID);
	return ID < 0x40000000u;
}
static uint32 __cdecl FakeGetAttackID()
{
	Logf("AID;");
	return World.AttackID;
}
static uint32 __cdecl FakeGetFollowID()
{
	Logf("FID;");
	return World.FollowID;
}
static void __cdecl FakeSetAttackID(uint32 ID) { Logf("SetA%u;", ID); }
static void __cdecl FakeSetFollowID(uint32 ID) { Logf("SetF%u;", ID); }
static bool __cdecl FakeIsBuddy(const char *Name)
{
	Logf("IsBuddy%s;", Name);
	return Hash(HashText(Name), World.Seed) % 2 != 0;
}
static int __cdecl FakeGetBuddyPosition(uint32 ID)
{
	Logf("BPos%u;", ID);
	for (int i = 0; i < NBUDDIES; i++) {
		if (World.BuddyIDs[i] == ID)
			return i;
	}
	return -1;
}
static const char *__cdecl FakeGetBuddyListName(int Position)
{
	Logf("BName%d;", Position);
	return World.BuddyNames[Position];
}
static bool __cdecl FakeGetBuddyListOnline(int Position)
{
	Logf("BOn%d;", Position);
	return World.BuddyOnline[Position];
}
static bool __cdecl FakeIsPlayerInParty()
{
	Logf("InParty;");
	return World.InParty;
}
static bool __cdecl FakeIsPlayerInFight()
{
	Logf("InFight;");
	return World.InFight;
}
static int __cdecl FakeGetPlayerPartyFlag()
{
	Logf("Party;");
	return World.PartyFlag;
}
static void __cdecl FakeRemoveBuddy(uint32 ID) { Logf("RemBuddy%u;", ID); }
static void __cdecl FakeSetBuddySortMethod(int Method) { Logf("Sort%d;", Method); }
static void __cdecl FakeHideOfflineBuddies(bool Hide) { Logf("HideOff%d;", Hide ? 1 : 0); }
static int __cdecl FakeSetPlayerDestination(int x, int y, int z, bool Exact, bool Diagonal)
{
	Logf("Dest%d,%d,%d,%d,%d;", x, y, z, Exact ? 1 : 0, Diagonal ? 1 : 0);
	return World.Destination;
}
static bool __cdecl FakeGetClassicControl()
{
	Logf("Classic;");
	return World.Classic;
}
static bool __cdecl FakeGetBuddyHideOffline()
{
	Logf("GetHideOff;");
	return World.HideOffline;
}
static bool __cdecl FakeGetLevelBar()
{
	Logf("GLB;");
	return World.LevelBar;
}
static bool __cdecl FakeGetMagLevelBar()
{
	Logf("GMB;");
	return World.MagLevelBar;
}
static bool __cdecl FakeGetSkillBar(int Skill)
{
	Logf("GSB%d;", Skill);
	return World.SkillBar[Skill];
}
static void __cdecl FakeSetLevelBar(bool Value) { Logf("SLB%d;", Value ? 1 : 0); }
static void __cdecl FakeSetMagLevelBar(bool Value) { Logf("SMB%d;", Value ? 1 : 0); }
static void __cdecl FakeSetSkillBar(int Skill, bool Value) { Logf("SSB%d,%d;", Skill, Value ? 1 : 0); }
static bool __cdecl FakeGetTimeStamps() { return World.TimeStamps; }
static bool __cdecl FakeHasOwnPrivateChannel()
{
	Logf("Unread;");
	return World.Unread;
}
static bool __cdecl FakeIsTalkIgnored(int Mode, const char *Name, int Channel)
{
	Logf("Ign%d,%s,%d;", Mode, Name, Channel);
	return Hash(HashText(Name), World.Seed + 7) % 2 != 0;
}
static void __cdecl FakeIgnoreName(const char *Name, bool Permanent) { Logf("DoIgn%s,%d;", Name, Permanent ? 1 : 0); }
static void __cdecl FakeUnignoreName(const char *Name) { Logf("Unign%s;", Name); }
static void __cdecl FakeAddConsoleText(uint32 ID, const char *Text, int Mode, const char *Name, int Channel, int Time)
{
	Logf("Console%u,%s,%d,%s,%d,%d;", ID, Text, Mode, Name == NULL ? "-" : Name, Channel, Time);
}
static TTalkBuffer *__cdecl FakeGetTextBuffer(int Mode, const char *Name, int Channel)
{
	Logf("GetBuf%d,%s,%d;", Mode, Name, Channel);
	return Buffer;
}
static void __cdecl FakeAddTextBufferLine(TTalkBuffer *Target, uint32 ID, const char *Text, int Mode,
	const char *Name, int Time)
{
	Logf("AddLine%d,%u,%s,%d,%s,%d;", Target == Buffer ? 1 : 0, ID, Text, Mode, Name, Time);
}
static void __cdecl FakeClearTextBuffer(TTalkBuffer *Target) { Logf("ClearBuf%d;", Target == Buffer ? 1 : 0); }
static void __cdecl FakeSendLook(int x, int y, int z, int Type, int Position)
{
	Logf("Look%d,%d,%d,%d,%d;", x, y, z, Type, Position);
}
static void __cdecl FakeSendUseObject(int x, int y, int z, int Type, int Position, int Container)
{
	Logf("Use%d,%d,%d,%d,%d,%d;", x, y, z, Type, Position, Container);
}
static void __cdecl FakeSendRotateObject(int x, int y, int z, int Type, int Position)
{
	Logf("Rot%d,%d,%d,%d,%d;", x, y, z, Type, Position);
}
static void __cdecl FakeSendAttack(uint32 ID) { Logf("Att%u;", ID); }
static void __cdecl FakeSendFollow(uint32 ID) { Logf("Fol%u;", ID); }
static void __cdecl FakeSendRequestOutfit() { Logf("Outfit;"); }
static void __cdecl FakeSendMoveObject(int x, int y, int z, int Type, int Position, int dx, int dy, int dz, int Count)
{
	Logf("Move%d,%d,%d,%d,%d,%d,%d,%d,%d;", x, y, z, Type, Position, dx, dy, dz, Count);
}
static void __cdecl FakeSendAddBuddy(const char *Name) { Logf("AddBuddy%s;", Name); }
static void __cdecl FakeSendInvite(const char *Name) { Logf("Invite%s;", Name); }
static void __cdecl FakeSendExclude(const char *Name) { Logf("Exclude%s;", Name); }
static void __cdecl FakeSendOpenPrivate(const char *Name) { Logf("Private%s;", Name); }
static void __cdecl FakeSendInviteToParty(uint32 ID) { Logf("PInv%u;", ID); }
static void __cdecl FakeSendJoinParty(uint32 ID) { Logf("PJoin%u;", ID); }
static void __cdecl FakeSendRevoke(uint32 ID) { Logf("PRev%u;", ID); }
static void __cdecl FakeSendPass(uint32 ID) { Logf("PPass%u;", ID); }
static void __cdecl FakeSendLeaveParty() { Logf("PLeave;"); }
static void __cdecl FakeSendRemoveBuddy(uint32 ID) { Logf("SRemBuddy%u;", ID); }
static void __cdecl FakeSendProcess(const char *Name) { Logf("Process%s;", Name); }
static void __cdecl FakeSendClose(const char *Name) { Logf("CloseRV%s;", Name); }
static void __cdecl FakeSendWalk() { Logf("Walk;"); }
static void __cdecl FakeSendStop() { Logf("Stop;"); }
static bool __cdecl FakeGetContainerHasParent(int Container)
{
	Logf("HasParent%d;", Container);
	return World.HasParent;
}
static int __cdecl FakeGetFreeContainer()
{
	Logf("FreeCont;");
	return World.FreeContainer;
}
static bool __cdecl FakeGetObjectFlag(int Type, int Flag)
{
	Logf("Flag%d,%d;", Type, Flag);
	return Hash((uint32)(Type * 64 + Flag), World.Seed + 3) % 3 == 0;
}
static void __cdecl FakeGetHelpArea(int *x, int *y, int *Width, int *Height)
{
	Logf("HelpArea;");
	*x = World.HelpX;
	*y = World.HelpY;
	*Width = World.HelpWidth;
	*Height = World.HelpHeight;
}
static bool __cdecl FakeIsClipboardEmpty()
{
	Logf("ClipEmpty;");
	return World.ClipboardEmpty;
}
static void __cdecl FakeDeleteAutomapMark(TMark &Mark, int z) { Logf("DelMark%d,%d,%d;", Mark.x, Mark.y, z); }
static int __cdecl FakeGetFontLineHeight(int Font)
{
	Logf("LineH%d;", Font);
	return 10 + Font;
}
static int __cdecl FakeGetFontHeight(int Font)
{
	Logf("FontH%d;", Font);
	return 8 + Font % 3;
}
// The error round's throwing fakes (issue #34): a fake whose bit is set
// throws an assertion of the side that calls it, with -Bit as its value.
static int ThrowMask;
static void MaybeThrow(int Bit)
{
	if ((ThrowMask & Bit) != 0)
		ThrowSideAssertion(Side == ORIGINAL, -Bit);
}
static int __cdecl FakeGetTextWidth(int Font, const char *Text)
{
	Logf("TextW%d,%s;", Font, Text);
	MaybeThrow(1);
	return (int)strlen(Text) * (5 + Font % 3);
}
static void __cdecl FakePrintText(int Surface, int x, int y, int Font, TRGBColor Color, const char *Text, int Alignment)
{
	Logf("Print%d,%d,%d,%d,%d,%d,%d,%s,%d;", Surface, x, y, Font, Color.R, Color.G, Color.B, Text, Alignment);
	MaybeThrow(2);
}
static void __cdecl FakeDrawSkin(int Surface, int x, int y, int width, int height, int skin, int srcx, int srcy)
{
	Logf("Skin%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, width, height, skin, srcx, srcy);
	MaybeThrow(4);
}
static void __cdecl FakeDrawPicturePart(int Surface, int x, int y, int PictureNumber, int srcx, int srcy, int Width,
	int Height)
{
	Logf("Part%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, PictureNumber, srcx, srcy, Width, Height);
}

} // namespace ContextMenusTest


namespace ContextMenusTest {

class TMenuSurface : public TTestSurface {
public:
	void FillRectangle(int Surface, TRect R, TRGBColor C)
	{
		Logf("Fill%d,%d,%d,%d,%d,%d,%d,%d;", Surface, R.x, R.y, R.width, R.height, C.R, C.G, C.B);
	}
};
static TMenuSurface *MenuSurface;
static TSurface *__cdecl FakeGetSurface() { return MenuSurface; }


// Patches that are undone at the end (later tests use these functions).
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
	golden::SaveOwn(Function, S.Bytes);
	golden::RedirectOwn(Function, Target);
	SavedCode.push_back(S);
}

static void RestoreAll()
{
	for (size_t i = SavedCode.size(); i-- > 0;) {
		golden::RestoreOwn(SavedCode[i].Code, SavedCode[i].Bytes);
	}
	SavedCode.clear();
}

// Ours of the functions the oracle's difftest.exe defines in this file (the Send functions, IsClipboardEmpty):
// NULL there, the runner's stubs (client_stubs.cpp) and sysinc.cpp's IsClipboardEmpty here.
#define DEFINED_HERE(Function) ((void *)Function)

static void PatchCallees()
{
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		{0x4e6e20, (void *)SetActionState, (void *)FakeSetActionState},
		{0x4e6e60, (void *)GetMouseX, (void *)FakeGetMouseX},
		{0x4e6e70, (void *)GetMouseY, (void *)FakeGetMouseY},
		{0x4e7150, (void *)GetTradeObject, (void *)FakeGetTradeObject},
		{0x4e7190, (void *)GetUseObject, (void *)FakeGetUseObject},
		{0x4e71d0, (void *)GetTradeInfo, (void *)FakeGetTradeInfo},
		{0x4e9020, (void *)AddMessage, (void *)FakeAddMessage},
		{0x4ea5b0, (void *)CopyToClipboard, (void *)FakeCopyToClipboard},
		{0x4e6dc0, (void *)CheckAnyRight, (void *)FakeCheckAnyRight},
		{0x44b9c0, (void *)GetPlayerID, (void *)FakeGetPlayerID},
		{0x44c6e0, (void *)GetCreature, (void *)FakeGetCreature},
		{0x44b910, (void *)IsPlayer, (void *)FakeIsPlayer},
		{0x44bb50, (void *)GetAttackID, (void *)FakeGetAttackID},
		{0x44bb60, (void *)GetFollowID, (void *)FakeGetFollowID},
		{0x44bb00, (void *)SetAttackID, (void *)FakeSetAttackID},
		{0x44bb20, (void *)SetFollowID, (void *)FakeSetFollowID},
		{0x44bbb0, (void *)IsBuddy, (void *)FakeIsBuddy},
		{0x44bb80, (void *)GetBuddyPosition, (void *)FakeGetBuddyPosition},
		{0x44feb0, (void *)GetBuddyListName, (void *)FakeGetBuddyListName},
		{0x450020, (void *)GetBuddyListOnline, (void *)FakeGetBuddyListOnline},
		{0x44d6f0, (void *)IsPlayerInParty, (void *)FakeIsPlayerInParty},
		{0x44baf0, (void *)IsPlayerInFight, (void *)FakeIsPlayerInFight},
		{0x44d720, (void *)GetPlayerPartyFlag, (void *)FakeGetPlayerPartyFlag},
		{0x44f5e0, (void *)RemoveBuddy, (void *)FakeRemoveBuddy},
		{0x44f810, (void *)SetBuddySortMethod, (void *)FakeSetBuddySortMethod},
		{0x44fa00, (void *)HideOfflineBuddies, (void *)FakeHideOfflineBuddies},
		{0x451400, (void *)SetPlayerDestination, (void *)FakeSetPlayerDestination},
		{0x4db640, (void *)GetClassicControl, (void *)FakeGetClassicControl},
		{0x4db540, (void *)GetBuddyHideOffline, (void *)FakeGetBuddyHideOffline},
		{0x4db6e0, (void *)GetLevelBar, (void *)FakeGetLevelBar},
		{0x4db6f0, (void *)GetMagLevelBar, (void *)FakeGetMagLevelBar},
		{0x4dd940, (void *)GetSkillBar, (void *)FakeGetSkillBar},
		{0x4db700, (void *)SetLevelBar, (void *)FakeSetLevelBar},
		{0x4db710, (void *)SetMagLevelBar, (void *)FakeSetMagLevelBar},
		{0x4ddaa0, (void *)SetSkillBar, (void *)FakeSetSkillBar},
		{0x4db5f0, (void *)GetTimeStamps, (void *)FakeGetTimeStamps},
		{0x502880, (void *)HasOwnPrivateChannel, (void *)FakeHasOwnPrivateChannel},
		{0x500060, (void *)IsTalkIgnored, (void *)FakeIsTalkIgnored},
		{0x501940, (void *)IgnoreName, (void *)FakeIgnoreName},
		{0x502010, (void *)UnignoreName, (void *)FakeUnignoreName},
		{0x5061f0, (void *)AddConsoleText, (void *)FakeAddConsoleText},
		{0x5025c0, (void *)GetTextBuffer, (void *)FakeGetTextBuffer},
		{0x505f10, (void *)AddTextBufferLine, (void *)FakeAddTextBufferLine},
		{0x502340, (void *)ClearTextBuffer, (void *)FakeClearTextBuffer},
		{0x4065e0, (void *)SendLook, (void *)FakeSendLook},
		{0x4056f0, (void *)SendUseObject, (void *)FakeSendUseObject},
		{0x405d10, DEFINED_HERE(SendRotateObject), (void *)FakeSendRotateObject},
		{0x408080, (void *)SendAttack, (void *)FakeSendAttack},
		{0x408230, DEFINED_HERE(SendFollow), (void *)FakeSendFollow},
		{0x409680, DEFINED_HERE(SendRequestOutfit), (void *)FakeSendRequestOutfit},
		{0x404db0, (void *)SendMoveObject, (void *)FakeSendMoveObject},
		{0x409b00, DEFINED_HERE(SendAddBuddy), (void *)FakeSendAddBuddy},
		{0x408580, DEFINED_HERE(SendInviteToPrivateChannel), (void *)FakeSendInvite},
		{0x408730, DEFINED_HERE(SendExcludeFromPrivateChannel), (void *)FakeSendExclude},
		{0x407800, DEFINED_HERE(SendOpenPrivateConversation), (void *)FakeSendOpenPrivate},
		{0x4088e0, DEFINED_HERE(SendInviteToParty), (void *)FakeSendInviteToParty},
		{0x408a90, DEFINED_HERE(SendJoinParty), (void *)FakeSendJoinParty},
		{0x408c40, DEFINED_HERE(SendRevokePartyInvitation), (void *)FakeSendRevoke},
		{0x408df0, DEFINED_HERE(SendPassPartyLeadership), (void *)FakeSendPass},
		{0x408fa0, DEFINED_HERE(SendLeaveParty), (void *)FakeSendLeaveParty},
		{0x409d90, DEFINED_HERE(SendRemoveBuddy), (void *)FakeSendRemoveBuddy},
		{0x4079b0, DEFINED_HERE(SendProcessRuleViolation), (void *)FakeSendProcess},
		{0x407b60, DEFINED_HERE(SendCloseRuleViolation), (void *)FakeSendClose},
		{0x414700, (void *)SendWalk, (void *)FakeSendWalk},
		{0x414a10, (void *)SendStop, (void *)FakeSendStop},
		{0x4a6a40, (void *)GetContainerHasParent, (void *)FakeGetContainerHasParent},
		{0x4a6cf0, (void *)GetFreeContainer, (void *)FakeGetFreeContainer},
		{0x4d1c40, (void *)(bool (*)(int, int))GetObjectFlag, (void *)FakeGetObjectFlag},
		{0x4a2a90, (void *)GetHelpArea, (void *)FakeGetHelpArea},
		{0x4fcbe0, DEFINED_HERE(IsClipboardEmpty), (void *)FakeIsClipboardEmpty},
		{0x4b5410, (void *)DeleteAutomapMark, (void *)FakeDeleteAutomapMark},
		{0x48b430, (void *)GetFontLineHeight, (void *)FakeGetFontLineHeight},
		{0x491710, (void *)GetFontHeight, (void *)FakeGetFontHeight},
		{0x494570, (void *)GetTextWidth, (void *)FakeGetTextWidth},
		{0x493af0, (void *)PrintText, (void *)FakePrintText},
		{0x497a00, (void *)DrawSkin, (void *)FakeDrawSkin},
		{0x490c60, (void *)DrawPicturePart, (void *)FakeDrawPicturePart},
		{0x4eaa90, (void *)GetSurface, (void *)FakeGetSurface},
	};
	// Ours is NULL for the functions this file defines (the linker may fold
	// them with their fakes; a jump would then point at itself).
	for (auto &T : Table) {
		ORIGINAL(Redirect((void *)(uintptr_t)T.Image, T.Fake));
		if (T.Ours != NULL)
			Redirect(T.Ours, T.Fake);
	}
	// DialogElements.cpp: both sides build the fakes (elementfakes.cpp).
	PatchElementFakes(Redirect);
	// ConsoleWindow.cpp's methods: the image's and ours.
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} ConsoleTable[] = {
		{0x419030, Member(&TTextWindow::GetTalkLine), (void *)FakeGetTalkLine},
		{0x419bd0, Member(&TTextWindow::GetSelectedText), (void *)FakeGetSelectedText},
		{0x4199e0, Member(&TTextWindow::SelectAll), (void *)FakeSelectAll},
		{0x41f1c0, Member(&TConsoleWindow::GetServerMessages), (void *)FakeGetServerMessages},
		{0x41f470, Member(&TConsoleWindow::SetServerMessages), (void *)FakeSetServerMessages},
		{0x41f7f0, Member(&TConsoleWindow::GetTextBuffer), (void *)FakeConsoleTextBuffer},
		{0x420090, Member(&TConsoleWindow::SaveChannel), (void *)FakeSaveChannel},
		{0x4234b0, Member(&TConsoleWindow::CloseChannel), (void *)FakeCloseChannel},
		{0x425070, Member(&TConsoleWindow::OpenChannel), (void *)FakeOpenChannel},
	};
	for (auto &T : ConsoleTable) {
		ORIGINAL(Redirect((void *)(uintptr_t)T.Image, T.Fake));
		Redirect(T.Ours, T.Fake);
	}
	ORIGINAL(Redirect((void *)(uintptr_t)0x5121ea, (void *)ImageTime);
		Redirect((void *)(uintptr_t)0x513eb1, (void *)ImageLocaltime);
		Redirect((void *)(uintptr_t)0x511d6b, (void *)ImageSprintf);
		Redirect((void *)(uintptr_t)0x51233d, (void *)ImageSnprintf));
}

// --- The windows ---------------------------------------------------------------------------

enum {
	KIND_HELP,
	KIND_OBJECT,
	KIND_CREATURE,
	KIND_BUDDY,
	KIND_TEXT,
	KIND_REQUEST,
	KIND_CHANNEL,
	KIND_EDIT,
	KIND_MARK,
	KIND_SKILL,
	KINDS
};
static const char *const KindNames[] = {"help", "object", "creature", "buddy", "text", "request", "channel",
	"edit", "mark", "skill"};
[[maybe_unused]] static void *OurVtables[KINDS];
static void *OurEntryVtable;


struct TArguments {
	int Kind;
	const char *HelpText;
	int x, y, ParentWidth, ParentHeight;
	uint32 ID;
	int LineNumber;
	TMarkMenuData Data;
};

static TWindow *Window[2];
static int Kind;

static TWindow *Build(int s, TArguments &A)
{
	static const size_t Sizes[] = {sizeof(THelpBubble), sizeof(TObjectContextMenu), sizeof(TCreatureContextMenu),
		sizeof(TBuddyContextMenu), sizeof(TTextContextMenu), sizeof(TRequestContextMenu),
		sizeof(TChannelContextMenu), sizeof(TEditContextMenu), sizeof(TMarkContextMenu), sizeof(TSkillContextMenu)};
	// Both sides get memory with the same contents: members the constructors
	// leave alone (TChannelContextMenu::ShowServerMessages of the default
	// channel) compare equal.
	void *M = operator new(Sizes[A.Kind]);
	memset(M, 0xa5, Sizes[A.Kind]);
	TWindow *W = NULL;
	if (s == OURS) {
		switch (A.Kind) {
		case KIND_HELP: W = new (M) THelpBubble(A.HelpText); break;
		case KIND_OBJECT: W = new (M) TObjectContextMenu(A.x, A.y, A.ParentWidth, A.ParentHeight); break;
		case KIND_CREATURE: W = new (M) TCreatureContextMenu(A.ID, A.x, A.y, A.ParentWidth, A.ParentHeight); break;
		case KIND_BUDDY: W = new (M) TBuddyContextMenu(A.ID, A.x, A.y, A.ParentWidth, A.ParentHeight); break;
		case KIND_TEXT:
			W = new (M) TTextContextMenu(TextWindow, A.LineNumber, A.x, A.y, A.ParentWidth, A.ParentHeight);
			break;
		case KIND_REQUEST:
			W = new (M) TRequestContextMenu(Console, A.LineNumber, A.x, A.y, A.ParentWidth, A.ParentHeight);
			break;
		case KIND_CHANNEL: W = new (M) TChannelContextMenu(Console, A.x, A.y, A.ParentWidth, A.ParentHeight); break;
		case KIND_EDIT: W = new (M) TEditContextMenu((TEditLine *)Edit, A.x, A.y, A.ParentWidth, A.ParentHeight); break;
		case KIND_MARK:
			W = new (M) TMarkContextMenu(new TMarkMenuData(A.Data), A.x, A.y, A.ParentWidth, A.ParentHeight);
			break;
		case KIND_SKILL:
			W = new (M) TSkillContextMenu(SkillsWindow[s], A.x, A.y, A.ParentWidth, A.ParentHeight);
			break;
		}
		SwapSeparators(s, W);
		return W;
	}

	return W;
}

// Runs one call on both sides: F(s) with Side set. An error of our side is
// reported and ends the test.
template <class F> static void Both(F Call)
{
	Side = OURS;
	try {
		Call(OURS);
	} catch (TMyException &E) {
		printf("FAIL ContextMenus (%s): our side raised %s\n", KindNames[Kind], E.GetMessage().c_str());
		exit(1);
	}
	ORIGINAL(Side = ORIGINAL; Call(ORIGINAL));
}

// --- Dumps ---------------------------------------------------------------------------------

static void DumpGeometry(std::string &Out, TComponent *C)
{
	char Text[256];
	sprintf(Text, " %d,%d %dx%d", C->OffsetX, C->OffsetY, C->Width, C->Height);
	Out += Text;
	if (C->DrawingListLength > 0 && C->DrawingList != NULL) {
		sprintf(Text, " DL%d", C->DrawingListLength);
		Out += Text;
		for (int i = 0; i < C->DrawingListLength; i++) {
			TDrawing &D = C->DrawingList[i];
			sprintf(Text, " %d/%d/%d/%d/%d/%d/%d", D.skin, D.x, D.y, D.width, D.height, D.srcx, D.srcy);
			Out += Text;
		}
	}
}

static std::string Dump(int s)
{
	int SaveSide = Side;
	Side = s;
	TWindow *W = Window[s];
	std::string Out;
	char Text[512];
	Out += KindNames[Kind];
	DumpGeometry(Out, W);
	Out += " capture " + std::string(W->Capture == NULL ? "0" : "?");
	if (Kind == KIND_HELP) {
		THelpBubble *H = (THelpBubble *)W;
		Out += " text";
		DumpGeometry(Out, H->Text);
		if (H->Text->FirstChild != NULL)
			DumpGeometry(Out, H->Text->FirstChild);
	} else {
		TContextMenu *M = (TContextMenu *)W;
		sprintf(Text, " ew %d sep %d empty %d", M->EntryWidth, M->Separator ? 1 : 0, M->Empty ? 1 : 0);
		Out += Text;
		switch (Kind) {
		case KIND_OBJECT: {
			TObjectContextMenu *O = (TObjectContextMenu *)W;
			sprintf(Text, " looked %d/%d/%d %d %d,%d,%d used %d/%d/%d %d %d,%d,%d cr %u", O->LookedObject.Type,
				O->LookedObject.Data, O->LookedObject.Data2, O->LookedObjectIndex, O->LookedObjectX, O->LookedObjectY,
				O->LookedObjectZ, O->UsedObject.Type, O->UsedObject.Data, O->UsedObject.Data2, O->UsedObjectIndex,
				O->UsedObjectX, O->UsedObjectY, O->UsedObjectZ, O->CreatureID);
			break;
		}
		case KIND_CREATURE: sprintf(Text, " id %u", ((TCreatureContextMenu *)W)->CreatureID); break;
		case KIND_BUDDY: sprintf(Text, " id %u", ((TBuddyContextMenu *)W)->CharacterID); break;
		case KIND_TEXT:
			sprintf(Text, " tw %d line %d", ((TTextContextMenu *)W)->TextWindow == TextWindow ? 1 : 0,
				((TTextContextMenu *)W)->LineNumber);
			break;
		case KIND_REQUEST:
			sprintf(Text, " cw %d line %d", ((TRequestContextMenu *)W)->ConsoleWindow == Console ? 1 : 0,
				((TRequestContextMenu *)W)->LineNumber);
			break;
		case KIND_CHANNEL:
			// The bool's byte (the default channel's is the memory's 0xa5): a bool that is neither 0 nor 1 is not a
			// value, GCC reads it as its byte (issue #91).
			sprintf(Text, " cw %d show %d", ((TChannelContextMenu *)W)->ConsoleWindow == Console ? 1 : 0,
				*(const uint8 *)&((TChannelContextMenu *)W)->ShowServerMessages != 0 ? 1 : 0);
			break;
		case KIND_EDIT: sprintf(Text, " edit %d", ((TEditContextMenu *)W)->Edit == (TEditLine *)Edit ? 1 : 0); break;
		case KIND_MARK: {
			TMarkMenuData &D = ((TMarkContextMenu *)W)->Data;
			// The window pointer is a number of the test's (0x1000-0x1fff): printed as the x86 oracle's %p printed
			// it, eight hex digits (x64's %p has sixteen).
			sprintf(Text, " data %d,%d,%d %d,%d,%d,%s %08X", D.Coordinate.x, D.Coordinate.y, D.Coordinate.z, D.Mark.x,
				D.Mark.y, D.Mark.Type, D.Mark.Text.Chars, (unsigned)(uintptr_t)D.Window);
			break;
		}
		case KIND_SKILL: {
			TSkillContextMenu *K = (TSkillContextMenu *)W;
			sprintf(Text, " skills %d back %d", K->SkillsWindow == SkillsWindow[s] ? 1 : 0,
				SkillsWindow[s]->ContextMenu == (TSkillContextMenu *)W ? 1 : 0);
			break;
		}
		default:
			Text[0] = 0;
		}
		Out += Text;
	}
	Out += "\n";
	for (TComponent *C = W->FirstChild; C != NULL; C = C->Next) {
		bool IsEntry = s == OURS ? VtableOf(C) == OurEntryVtable : VtableOf(C) == ORIGINAL_OR((void *)(uintptr_t)0x55f6a8, nullptr);
		Out += IsEntry ? "  entry" : "  child";
		DumpGeometry(Out, C);
		if (IsEntry) {
			TContextMenuEntry *E = (TContextMenuEntry *)C;
			sprintf(Text, " \"%s\" \"%s\" %d %d", E->Text.Chars, E->Shortcut.Chars, E->Callback.Event,
				E->Callback.Receiver == W ? 1 : 0);
			Out += Text;
		} else if (Kind != KIND_HELP) {
			sprintf(Text, " sep %d", ((TSeparator *)C)->Horizontal ? 1 : 0);
			Out += Text;
		}
		Out += C->Parent == W ? " p\n" : " ?\n";
	}
	Side = SaveSide;
	return Out;
}

// The original's log of a comparison: the oracle's, recorded; the runner's from the corpus.
static void OutputOriginalLog()
{
	std::string Text = golden::OutputText("log", Log[ORIGINAL].data(), Log[ORIGINAL].size());
	ORIGINAL_OR((void)0, Log[ORIGINAL] = Text);
}

// Whether the original has a window (the oracle's answer, recorded).
static bool OriginalWindow() { return ORIGINAL_VALUE("original window", bool, Window[ORIGINAL] != NULL); }

// The original's dump: the oracle's, recorded by its changes since the last one; the runner's from the
// corpus.
static std::string LastDump;
static std::string OriginalDump()
{
	std::string Text;
	return golden::OutputTextChanges("dump", &LastDump, Text);
}

static void Compare(const char *What)
{
	OutputOriginalLog();
	Checks++;
	if (Log[OURS] != Log[ORIGINAL]) {
		Failures++;
		if (Failures <= FailuresPrinted) {
			printf("FAIL ContextMenus %s (%s): logs differ\n  ours:     %.1500s\n  original: %.1500s\n", What,
				KindNames[Kind], Log[OURS].c_str(), Log[ORIGINAL].c_str());
		}
	}
	if (Window[OURS] != NULL && OriginalWindow()) {
		std::string A = Dump(OURS), B = OriginalDump();
		Checks++;
		if (A != B) {
			Failures++;
			if (Failures <= FailuresPrinted)
				printf("FAIL ContextMenus %s (%s): windows differ\n--- ours\n%s--- original\n%s", What,
					KindNames[Kind], A.c_str(), B.c_str());
		}
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

// --- Random properties -----------------------------------------------------------------------

static void RandomSkinProperties()
{
	// The image's skin properties get the same values as ours (the oracle only). Each constructor's arguments
	// are drawn in the order the x86 oracle evaluated them, right to left (#55).
	TSkinProperties &M = SkinProperties;
	TSkinProperties &O = ORIGINAL_OR(Global<TSkinProperties>(0x5ccfc8), M); // the runner: ours twice
	M.Data09C = O.Data09C = 1 + Random() % 40;
	{
		int Height = (int)(Random() % 9) - 4, Width = (int)(Random() % 9) - 4, y = (int)(Random() % 9) - 4,
			x = (int)(Random() % 9) - 4;
		M.Data0A0 = O.Data0A0 = TRect(x, y, Width, Height);
	}
	M.Data0B0.FontNumber = O.Data0B0.FontNumber = 1 + Random() % 4;
	{
		int Blue = Random() % 256, Green = Random() % 256, Red = Random() % 256;
		M.Data0B0.Color = O.Data0B0.Color = TRGBColor(Red, Green, Blue);
	}
	M.Data0C4 = O.Data0C4 = 1 + Random() % 10;
	{
		int Blue = Random() % 256, Green = Random() % 256, Red = Random() % 256;
		M.Data0C8 = O.Data0C8 = TRGBColor(Red, Green, Blue);
	}
	M.Data0D4 = O.Data0D4 = Random() % 4;
	M.Data150 = O.Data150 = 10 + Random() % 80;
	M.Data154 = O.Data154 = 10 + Random() % 80;
	M.Data158 = O.Data158 = 10 + Random() % 400;
	M.Data15C = O.Data15C = 10 + Random() % 400;
	// The text keeps a width and height above 0 in the smallest bubble.
	{
		int Height = -(int)(Random() % 4), Width = -(int)(Random() % 4), y = Random() % 4, x = Random() % 4;
		M.Data160 = O.Data160 = TRect(x, y, Width, Height);
	}
	M.Data170 = O.Data170 = 1 + Random() % 6;
	M.Data174 = O.Data174 = Random() % M.Data170;
	M.Data178 = O.Data178 = Random() % 6;
}

static TArguments RandomArguments(int K)
{
	static const char *const HelpTexts[] = {"Help", "This is the {inventory}.", "", "A longer help text."};
	TArguments A;
	A.Kind = K;
	A.HelpText = HelpTexts[Random() % 4];
	A.ParentWidth = 200 + Random() % 800;
	A.ParentHeight = 200 + Random() % 600;
	A.x = (int)(Random() % (A.ParentWidth + 40)) - 20;
	A.y = (int)(Random() % (A.ParentHeight + 40)) - 20;
	uint32 IDs[] = {World.Creatures[0].CreatureID, World.Creatures[1].CreatureID, World.Creatures[2].CreatureID,
		World.Creatures[4].CreatureID, 0x10000777u, World.BuddyIDs[1], World.BuddyIDs[2]};
	A.ID = IDs[Random() % 7];
	A.LineNumber = (int)(Random() % (NLINES + 2)) - 1;
	A.Data.Coordinate.x = 32000 + Random() % 500;
	A.Data.Coordinate.y = 32000 + Random() % 500;
	A.Data.Coordinate.z = Random() % 16;
	if (Random() % 2 == 0) {
		A.Data.Mark.x = 32000 + Random() % 500;
		A.Data.Mark.y = Random() % 4 == 0 ? 0 : 32000 + Random() % 500;
	} else {
		A.Data.Mark.x = Random() % 4 == 0 ? 32000 : 0;
		A.Data.Mark.y = 0;
	}
	A.Data.Mark.Type = Random() % 20;
	strcpy(A.Data.Mark.Text, Random() % 2 ? "Depot" : "");
	A.Data.Window = (TAutomapWindow *)(uintptr_t)(0x1000 + Random() % 0x1000);
	TextChildWidth = 1 + Random() % 300;
	TextChildHeight = 1 + Random() % 300;
	return A;
}

static void RandomEdit()
{
	memset(Edit->Data30, 0, sizeof(Edit->Data30));
	Edit->Data5C = Random() % 3 == 0 ? -1 : Random() % 20;
	Edit->Data60 = Random() % 3 == 0 ? 0 : Random() % 20;
	Edit->Data66 = Random() % 3 == 0;
}

// The events a menu kind handles without "unknown event" (the text and
// request menus ignore unknown ones).
static bool KnownEvent(int K, int Event)
{
	static const int Object[] = {10001, 10002, 10003, 10004, 10005, 10006, 10007, 10008, 10012, 10013, 10014,
		10015, 10016, 10018, 10019, 10020, 10021, 10022, 10023, 10100, 10101, 10203, 0};
	static const int Creature[] = {10004, 10005, 10006, 10007, 10015, 10016, 10018, 10019, 10021, 10022, 10023,
		10100, 10101, 10203, 0};
	static const int Buddy[] = {10016, 10023, 10204, 10300, 10301, 10302, 10303, 10304, 10305, 10306, 10307, 0};
	static const int Channel[] = {10016, 10017, 10100, 10102, 10103, 10105, 0};
	static const int EditEvents[] = {10200, 10201, 10202, 10203, 10206, 0};
	static const int Mark[] = {10400, 10401, 10402, 10403, 0};
	static const int Skill[] = {10500, 10501, 10502, 10503, 10504, 10505, 10506, 10507, 10508, 10509, 10510, 0};
	const int *List = NULL;
	switch (K) {
	case KIND_TEXT:
	case KIND_REQUEST: return true;
	case KIND_OBJECT: List = Object; break;
	case KIND_CREATURE: List = Creature; break;
	case KIND_BUDDY: List = Buddy; break;
	case KIND_CHANNEL: List = Channel; break;
	case KIND_EDIT: List = EditEvents; break;
	case KIND_MARK: List = Mark; break;
	case KIND_SKILL: List = Skill; break;
	default: return false;
	}
	for (; *List != 0; List++) {
		if (*List == Event)
			return true;
	}
	return false;
}

static int RandomEvent()
{
	static const int Events[] = {0, 10001, 10002, 10003, 10004, 10005, 10006, 10007, 10008, 10010, 10011, 10012,
		10013, 10014, 10015, 10016, 10017, 10018, 10019, 10020, 10021, 10022, 10023, 10100, 10101, 10102, 10103,
		10105, 10106, 10107, 10200, 10201, 10202, 10203, 10204, 10205, 10206, 10300, 10301, 10302, 10303, 10304,
		10305, 10306, 10307, 10400, 10401, 10402, 10403, 10500, 10501, 10502, 10503, 10504, 10505, 10506, 10507,
		10508, 10509, 10510, 12345};
	return Events[Random() % (sizeof(Events) / sizeof(Events[0]))];
}

// Events of the entries of our menu, and whether the object menu's
// assertions hold for one.
static std::vector<int> EntryEvents()
{
	std::vector<int> Events;
	for (TComponent *C = Window[OURS]->FirstChild; C != NULL; C = C->Next) {
		if (VtableOf(C) == OurEntryVtable)
			Events.push_back(((TContextMenuEntry *)C)->Callback.Event);
	}
	return Events;
}
static bool SafeEvent(int Event)
{
	if (Kind == KIND_OBJECT) {
		TObjectContextMenu *O = (TObjectContextMenu *)Window[OURS];
		if (Event == 10010)
			return O->LookedObjectX == 0xFFFF && O->LookedObject.Type != CREATURE;
		if (Event == 10011)
			return O->UsedObjectX == 0xFFFF && O->UsedObjectY >= 64;
	}
	return Kind != KIND_HELP && (Event != 0 || Kind == KIND_TEXT || Kind == KIND_REQUEST);
}

// A click on an entry may send an event the menu does not know (the empty
// entry of a menu without entries); only the text and request menus have
// one of those, and they ignore it.
static bool ClickIsSafe(int x, int y)
{
	for (TComponent *C = Window[OURS]->FirstChild; C != NULL; C = C->Next) {
		if (In(x - C->OffsetX, 0, C->Width - 1) && In(y - C->OffsetY, 0, C->Height - 1)) {
			if (VtableOf(C) == OurEntryVtable)
				return SafeEvent(((TContextMenuEntry *)C)->Callback.Event);
			return true;
		}
	}
	return true;
}

static int KindCount[KINDS];
static int ActionCount[12];

static void Action()
{
	int A = Random() % 12;
	if (getenv("CM_TRACE") != NULL)
		fprintf(stderr, "  action %d\n", A);
	RandomFlags();
	char What[64];
	sprintf(What, "action %d", A);
	TWindow *O = Window[OURS];
	switch (A) {
	case 0:
	case 1: {
		int Surface = Random() % 3, x = Random() % 50, y = Random() % 50;
		World.MouseX = x + (int)(Random() % (O->Width + 20)) - 10;
		World.MouseY = y + (int)(Random() % (O->Height + 20)) - 10;
		Both([&](int s) { Window[s]->Draw(Surface, x, y); });
		break;
	}
	case 2:
	case 3: {
		int x = (int)(Random() % (O->Width + 20)) - 10, y = (int)(Random() % (O->Height + 20)) - 10;
		if (Kind != KIND_HELP && !ClickIsSafe(x, y))
			return;
		bool Results[2];
		Both([&](int s) { Results[s] = Window[s]->LeftButtonUp(x, y, 0); });
		golden::Output("LeftButtonUp", Results[ORIGINAL]);
		CHECK(Results[OURS] == Results[ORIGINAL], "ContextMenus LeftButtonUp %d, original %d", Results[OURS],
			Results[ORIGINAL]);
		break;
	}
	case 4: {
		bool Results[2];
		int x = Random() % 100, y = Random() % 100;
		Both([&](int s) { Results[s] = Window[s]->RightButtonUp(x, y, 0); });
		golden::Output("RightButtonUp", Results[ORIGINAL]);
		CHECK(Results[OURS] == Results[ORIGINAL], "ContextMenus RightButtonUp %d, original %d", Results[OURS],
			Results[ORIGINAL]);
		break;
	}
	case 5: {
		int Key = Random() % 2 ? 27 : Random() % 300;
		Both([&](int s) { Window[s]->KeyPress(Key); });
		break;
	}
	case 6:
	case 7: {
		if (Kind == KIND_HELP)
			return;
		int Event;
		std::vector<int> Events = EntryEvents();
		if (Random() % 3 != 0 && !Events.empty())
			Event = Events[Random() % Events.size()];
		else
			Event = RandomEvent();
		if (!KnownEvent(Kind, Event) || !SafeEvent(Event))
			return;
		Both([&](int s) { Window[s]->HandleEvent(Event); });
		break;
	}
	case 8: {
		Both([&](int s) { Window[s]->Arrange(); });
		break;
	}
	case 9: {
		if (Kind == KIND_HELP) {
			int w = Random() % 500, h = Random() % 500;
			Both([&](int s) { Window[s]->SetSize(w, h); });
		} else {
			// Another Show (positions change, the last entry shrinks again; both
			// versions fail when it would get a negative height).
			TComponent *Last = O->GetLastChild();
			int Font = SkinProperties.Data0B0.FontNumber;
			if (Last != NULL && Last->Height - ((10 + Font) - (8 + Font % 3)) <= 0)
				return;
			int x = (int)(Random() % 900) - 20, y = (int)(Random() % 700) - 20, pw = 200 + Random() % 800,
				ph = 200 + Random() % 600;
			Both([&](int s) {
				if (s == OURS)
					((TContextMenu *)Window[s])->Show(x, y, pw, ph);
				else
					ORIGINAL(At<ShowFn>(0x442de0)(Window[s], NULL, x, y, pw, ph));
			});
		}
		break;
	}
	case 10: {
		if (Kind == KIND_HELP) {
			// Place in a window large enough for the fallback corners.
			World.HelpX = Random() % 700;
			World.HelpY = Random() % 500;
			World.HelpWidth = Random() % 300;
			World.HelpHeight = Random() % 300;
			TWindow Parent;
			Parent.Width = O->Width + SkinProperties.Data178 + Random() % 800;
			Parent.Height = O->Height + SkinProperties.Data178 + Random() % 600;
			Both([&](int s) {
				if (s == OURS)
					((THelpBubble *)Window[s])->Place(&Parent);
				else
					ORIGINAL(At<PlaceFn>(0x442110)(Window[s], NULL, &Parent));
			});
		} else {
			// More entries and a separator.
			// The new entry sends an event the menu knows (a click may follow).
			std::vector<int> Events = EntryEvents();
			int Event = Events.empty() ? 0 : Events[Random() % Events.size()], What2 = Random() % 3;
			if (!KnownEvent(Kind, Event) || !SafeEvent(Event))
				return;
			bool Sep = Random() % 2 != 0;
			const char *Text = Random() % 2 ? "Extra" : "A rather long entry text here";
			const char *Shortcut = Random() % 2 ? NULL : "(Ctrl+X)";
			Both([&](int s) {
				TContextMenu *M = (TContextMenu *)Window[s];
				M->Separator = Sep;
				// A separator is never the last child in the game (Show would
				// shrink it as the last entry).
				if (What2 == 0) {
					if (s == OURS)
						M->AddSeparator();
					else
						ORIGINAL(At<VoidFn>(0x43f1d0)(M, NULL));
					SwapSeparators(s, M);
				}
				{
					if (s == OURS)
						M->AddEntry(Event, Text, Shortcut);
					else
						ORIGINAL(At<AddEntryFn>(0x442910)(M, NULL, Event, Text, Shortcut));
				}
			});
		}
		break;
	}
	case 11: {
		if (Kind != KIND_HELP)
			return;
		int x = (int)(Random() % 60) - 5, y = (int)(Random() % 60) - 5, w = Random() % 300, h = Random() % 300;
		// Often at the margin, where the tests change.
		if (Random() % 2 == 0)
			x = SkinProperties.Data178 + (int)(Random() % 3) - 1;
		if (Random() % 2 == 0)
			y = SkinProperties.Data178 + (int)(Random() % 3) - 1;
		int Width = 50 + Random() % 500, Height = 50 + Random() % 500;
		TWindow Parent;
		Parent.Width = Width;
		Parent.Height = Height;
		int X[2] = {x, x}, Y[2] = {y, y};
		bool Results[2];
		Both([&](int s) {
			int ww = w, hh = h;
			if (s == OURS)
				Results[s] = ((THelpBubble *)Window[s])->FitsIn(X[s], Y[s], ww, hh, &Parent);
			else
				ORIGINAL(Results[s] = At<FitsInFn>(0x43dca0)(Window[s], NULL, &X[s], &Y[s], &ww, &hh, &Parent));
		});
		golden::Output("FitsIn", Results[ORIGINAL]);
		golden::Output("FitsIn x", X[ORIGINAL]);
		golden::Output("FitsIn y", Y[ORIGINAL]);
		CHECK(Results[OURS] == Results[ORIGINAL] && X[OURS] == X[ORIGINAL] && Y[OURS] == Y[ORIGINAL],
			"ContextMenus FitsIn %d %d,%d, original %d %d,%d", Results[OURS], X[OURS], Y[OURS], Results[ORIGINAL],
			X[ORIGINAL], Y[ORIGINAL]);
		break;
	}
	}
	ActionCount[A]++;
	Compare(What);
}

// --- Error paths (issue #34) ------------------------------------------------------------------

template <class F> static void ErrorCall(TErrorRound &R, const char *What, F Call)
{
	GOLDEN_ROUND_COMPARE(R, What, (Side = OURS, Call(OURS)), (Side = ORIGINAL, Call(ORIGINAL)));
}

// Every PATELLA_ASSERT/TERROR of ContextMenus.cpp that the menus can reach, and the
// TEND blocks: the text and skin fakes throw (ThrowMask) while the menus are
// built and drawn, HandleEvent gets events the menu does not know (and the
// object menu's asserted ones), AddEntry texts that are too long, and the
// constructors NULL for their windows.
static void TestContextMenusErrors()
{
	TErrorRound R("ContextMenus");
	static const int Masks[] = {0, 1, 2, 4, 7};
	static const char Long[] = "0123456789012345678901234567890123456789012345678901234567890123456789";
	char What[96];
	for (int Round = 0; Round < 500; Round++) {
		RandomSkinProperties();
		RandomWorld();
		SetSelection();
		RandomEdit();
		Kind = Random() % KINDS;
		TArguments A = RandomArguments(Kind);
		ThrowMask = Masks[Round % 5];
		snprintf(What, sizeof(What), "%s constructor, mask %d", KindNames[Kind], ThrowMask);
		Window[OURS] = Window[ORIGINAL] = NULL;
		// A party flag the menus do not know (their TERROR1).
		int SavedParty = World.PartyFlag;
		if ((Kind == KIND_OBJECT || Kind == KIND_CREATURE) && Round % 4 == 3)
			World.PartyFlag = 5 + (int)(Random() % 5);
		ErrorCall(R, What, [&](int s) { Window[s] = Build(s, A); });
		World.PartyFlag = SavedParty;
		Compare("errors: constructor");
		if (Window[OURS] != NULL && OriginalWindow()) {
			TWindow *O = Window[OURS];
			int x = Random() % 50, y = Random() % 50;
			World.MouseX = x + (int)(Random() % (O->Width + 20)) - 10;
			World.MouseY = y + (int)(Random() % (O->Height + 20)) - 10;
			snprintf(What, sizeof(What), "%s Draw, mask %d", KindNames[Kind], ThrowMask);
			ErrorCall(R, What, [&](int s) { Window[s]->Draw(1, x, y); });
			Compare("errors: draw");
			ThrowMask = 0;
			if (Kind != KIND_HELP && Kind != KIND_TEXT && Kind != KIND_REQUEST) {
				int Event;
				do
					Event = RandomEvent();
				while (KnownEvent(Kind, Event) && SafeEvent(Event));
				if (Kind == KIND_OBJECT && Round % 2 == 0)
					Event = 10010 + (int)(Random() % 2); // asserted unless SafeEvent
				if (Kind == KIND_OBJECT && SafeEvent(Event) && (Event == 10010 || Event == 10011))
					Event = 12345;
				snprintf(What, sizeof(What), "%s HandleEvent(%d)", KindNames[Kind], Event);
				ErrorCall(R, What, [&](int s) { Window[s]->HandleEvent(Event); });
				Compare("errors: event");
			}
			if (Kind == KIND_CHANNEL) {
				// Event 10105 with no text buffer behind the console.
				TTestBuffer *SavedBuffer = Buffer;
				Buffer = NULL;
				ErrorCall(R, "channel HandleEvent(10105) without a buffer", [&](int s) { Window[s]->HandleEvent(10105); });
				Buffer = SavedBuffer;
				Compare("errors: event");
			}
			if (Kind != KIND_HELP) {
				// AddEntry: no text, a text or a shortcut too long.
				int Case = Random() % 3, Length = 48 + Random() % 6;
				std::string Text(Long, Length);
				const char *T = Case == 0 ? NULL : Case == 1 ? Text.c_str()
															 : "entry";
				const char *Shortcut = Case == 2 ? Text.c_str() : NULL;
				snprintf(What, sizeof(What), "%s AddEntry case %d length %d", KindNames[Kind], Case, Length);
				ErrorCall(R, What, [&](int s) {
					if (s == OURS)
						((TContextMenu *)Window[s])->AddEntry(10001, T, Shortcut);
					else
						ORIGINAL(At<AddEntryFn>(0x442910)(Window[s], NULL, 10001, T, Shortcut));
				});
				Compare("errors: entry");
			}
		}
		ThrowMask = 0;
		for (int s = 0; s < SIDES; s++) {
			Side = s;
			DeleteComponent(Window[s]);
			Window[s] = NULL;
		}
		Compare("errors: destructor");
		FreeWorld();
		// The constructors without their window (or text).
		static const int NullKinds[] = {KIND_HELP, KIND_TEXT, KIND_REQUEST, KIND_CHANNEL, KIND_EDIT, KIND_SKILL,
			KIND_CHANNEL, KIND_MARK};
		int Case = Round % 8, K = NullKinds[Case];
		RandomWorld();
		Kind = K;
		TArguments B = RandomArguments(K);
		B.HelpText = NULL;
		TTextWindow *SavedText = TextWindow;
		TConsoleWindow *SavedConsole = Console;
		TTestEdit *SavedEdit = Edit;
		TSkillsWindowBody *SavedSkills[2] = {SkillsWindow[OURS], SkillsWindow[ORIGINAL]};
		TTestBuffer *SavedBuffer = Buffer;
		if (Case == 6) {
			Buffer = NULL; // the console without a text buffer
		} else {
			TextWindow = NULL;
			Console = NULL;
			Edit = NULL;
			SkillsWindow[OURS] = SkillsWindow[ORIGINAL] = NULL;
		}
		snprintf(What, sizeof(What), "%s constructor without its %s", KindNames[K], Case == 6 ? "buffer" : "window");
		TWindow *Built[2] = {NULL, NULL};
		if (K == KIND_MARK)
			ErrorCall(R, What, [&](int s) {
				void *M = operator new(sizeof(TMarkContextMenu));
				memset(M, 0xa5, sizeof(TMarkContextMenu));
				if (s == OURS)
					Built[s] = new (M) TMarkContextMenu(NULL, B.x, B.y, B.ParentWidth, B.ParentHeight);
				else
					ORIGINAL(Built[s] = (TWindow *)At<PtrCtorFn>(0x4451e0)(M, NULL, NULL, B.x, B.y, B.ParentWidth,
								 B.ParentHeight));
			});
		else
			ErrorCall(R, What, [&](int s) { Built[s] = Build(s, B); });
		Buffer = SavedBuffer;
		TextWindow = SavedText;
		Console = SavedConsole;
		Edit = SavedEdit;
		SkillsWindow[OURS] = SavedSkills[OURS];
		SkillsWindow[ORIGINAL] = SavedSkills[ORIGINAL];
		for (int s = 0; s < SIDES; s++) {
			Side = s;
			DeleteComponent(Built[s]);
		}
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		FreeWorld();
	}
	R.Finish();
}

} // namespace ContextMenusTest

void TestContextMenus()
{
	using namespace ContextMenusTest;
	int Before = Checks, BeforeFailures = Failures;
	ORIGINAL(PatchImage());
	InitSkins();
	ORIGINAL(Global<int>(0x71c450) = NumberOfSkins; Global<TSkin *>(0x71c454) = Skins.get());
	InitSkinProperties();
	ORIGINAL(At<void(__cdecl *)()>(0x498160)());
	LastDump.clear();
	MenuSurface = new TMenuSurface;
	PatchCallees();
	Running = true;

	Buffer = new TTestBuffer;
	TextWindow = (TTextWindow *)calloc(1, sizeof(TTextWindow));
	TextWindow->TextBuffer = Buffer;
	ConsoleView = (TConsole *)calloc(1, sizeof(TConsole));
	ConsoleView->TextWindow = TextWindow;
	Console = (TConsoleWindow *)calloc(1, sizeof(TConsoleWindow));
	Console->CurrentConsole = ConsoleView;
	Edit = new TTestEdit;
	for (int s = 0; s < 2; s++) {
		TMenuSkillsStandIn *StandIn = new TMenuSkillsStandIn;
		StandIn->Minimized = false;
		StandIn->ContextMenu = NULL;
		// Not a TSkillsWindowBody (smaller): reinterpret_cast, as the C-style cast was meant (a static_cast downcast
		// that UBSan checks, issue #91).
		SkillsWindow[s] = reinterpret_cast<TSkillsWindowBody *>(static_cast<TWindow *>(StandIn));
	}

	// Our vtables, from instances.
	{
		Side = OURS;
		TTestSeparator *S = new TTestSeparator;
		SeparatorVtable = VtableOf(S);
		DeleteComponent(S);
		TSeparator *Real = new TSeparator(true);
		OurSeparatorVtable = VtableOf(Real);
		DeleteComponent(Real);
		TTestFormattedText *T = new TTestFormattedText;
		TextVtable = VtableOf(T);
		DeleteComponent(T);
		RandomWorld();
		TContextMenuEntry *E = new TContextMenuEntry("x", NULL, TCallback(NULL, 0));
		OurEntryVtable = VtableOf(E);
		DeleteComponent(E);
		FreeWorld();
		Log[OURS].clear();
	}

	for (int Round = 0; Round < 3000; Round++) {
		RandomSkinProperties();
		RandomWorld();
		SetSelection();
		RandomEdit();
		Kind = Random() % KINDS;
		KindCount[Kind]++;
		if (getenv("CM_TRACE") != NULL)
			fprintf(stderr, "round %d kind %d\n", Round, Kind);
		TArguments A = RandomArguments(Kind);
		Both([&](int s) { Window[s] = Build(s, A); });
		Compare("constructor");
		int Actions = Random() % 8;
		for (int i = 0; i < Actions; i++)
			Action();
		Both([&](int s) {
			DeleteComponent(Window[s]);
			Window[s] = NULL;
		});
		CHECK(SkillsWindow[OURS]->ContextMenu == NULL
				&& ORIGINAL_VALUE("skill menu cleared", bool, SkillsWindow[ORIGINAL]->ContextMenu == NULL),
			"ContextMenus: the skill window keeps a deleted menu");
		Compare("destructor");
		FreeWorld();
	}
	int RoundChecks = Checks;
	TestContextMenusErrors();
	RoundChecks = Checks - RoundChecks;

	RestoreAll();
	Running = false;
	SeparatorVtable = NULL;
	delete MenuSurface;
	delete Buffer;
	free(TextWindow);
	free(ConsoleView);
	free(Console);
	DeleteComponent(Edit);
	for (int s = 0; s < 2; s++)
		DeleteComponent((TMenuSkillsStandIn *)(TWindow *)SkillsWindow[s]);
	ExitSkins();
	ORIGINAL(Global<TSkin *>(0x71c454) = NULL);
	int Least = KindCount[0], LeastAction = ActionCount[0];
	for (int i = 1; i < KINDS; i++)
		Least = KindCount[i] < Least ? KindCount[i] : Least;
	for (int i = 1; i < 12; i++)
		LeastAction = ActionCount[i] < LeastAction ? ActionCount[i] : LeastAction;
	printf("ContextMenus: %d checks, %d failures (each window at least %d times, each action at least %d times)\n",
		Checks - Before - RoundChecks, Failures - BeforeFailures, Least, LeastAction);
}
