// difftest.exe's Dialogs test (moved here from tools/difftest/dialogstest.cpp, issue #28): Dialogs.cpp
// against the original code, as a golden-corpus test (golden.h).
//
// Every round opens one dialog on both sides through its creator (the
// image's at its address, ours directly): the frame TDialog with its body,
// built from the real DialogElements.cpp controls (the image's code for the
// original, ours for ours). Both get the same random operations: drawing,
// keys (Return, Escape, the answer letters, digits, arrows, editing keys),
// clicks on the dialog's controls (presses with releases, double and right
// clicks, the wheel), mouse moves and drags of the caption, the body's and
// the frame's events (each dialog's own and the callbacks of its buttons and
// lists), Arrange and the bodies' members. After each operation the logs of
// every callee with an effect and a dump of the frame (every component of the
// tree with its geometry, drawing list and members, the timers and the radio
// and focus groups) are compared; at the end the frames are deleted.
//
// The callees are fakes for both sides that answer from a world chosen at
// random for each round: the text metrics and drawing (graphics.cpp), the
// surface, Status.cpp (action state, messages, account, characters, dragging,
// rights, error reports), Preferences.cpp (one copy per side: the dialogs set
// and read them in one operation), Talk.cpp's ignore lists, Creatures.cpp's
// VIP list, Communication.cpp's Send functions, the automap marks, the event
// helps, the object flags, Control.cpp's Resize, the clock, the clipboard,
// OpenURL and the display mode callback. The image's operator new and ours
// fill memory with the same pattern; the image's sprintf, _snprintf, qsort,
// atol and _stricmp are the test's CRT. Assertions are not exercised: the
// inputs keep to what the game does (events each dialog handles, a selected
// hotkey for event 11, colours of the 133 buttons).
// The dumps print every member of every component by its type (issue #28: before, every 32-bit word of the
// x86 object, which the runner's x64 objects do not have): a number as a number, a text buffer as its text and
// the bytes after it, a pointer by where it points (a tree component, a string, a timer, a block of the round's
// allocations, H or D when freed, with the offset into it; an image or module, S; the stack, K), never by the
// readability of the memory, so they do not depend on the heap layout of the run (issue #32) or on where the
// modules are.
// DIFFTEST_ONLY=Dialogs runs it alone; DG_TRACE=1 traces rounds and
// operations, DG_LOGS=<file> writes every compared log and dump,
// DG_HEAPSHIFT=<bytes> moves the heap layout, DG_BLOCKS=1 counts the blocks
// left in the table at the end.
//
// By value (plan 4.3): the original's side exists in the oracle only (its calls inside ORIGINAL, #if
// GOLDEN_ORACLE, CallI or At, which the runner never reaches); the runner has our dialogs alone. Per
// comparison the corpus holds the original's log (text), its dump (text, by its changes since the last one,
// golden::OutputTextChanges), how many fake calls its call made (the error round draws the failing call from
// them), whether it has a frame and what it threw. The runner redirects its own functions with
// golden::RedirectOwn (operator new and new[] to the pattern, operator delete and delete[] to the table of
// blocks).
// The rounds that open the advanced graphics dialog retire with the oracle (issue #28, decided by the user on
// November 4, 2026): the x64 Dialogs.cpp has the graphics device buttons of issue #31 where the oracle compares
// the DirectX5, DirectX9 and OpenGL buttons. The oracle runs them as before; the runner skips them
// (golden::RetiredBegin) and gets what the later rounds need from the corpus (RetiredEnd below).

#include "golden_test.h"

#include "golden_eh.h"
#include "testsurface.h"
#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#else
#include "golden_patch.h"
#endif

#include "Communication.h"
#include "Container.h"
#include "Control.h"
#include "Creatures.h"
#include "DialogElements.h"
#include "Dialogs.h"
#include "GUI.h"
#include "Help.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Utils.h"
#include "graphics.h"
#include "sysinc.h"

#include <initializer_list>
#include <map>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <typeinfo>
#include <vector>

#if GOLDEN_ORACLE
static_assert(sizeof(TDialog) == 0xc4 && sizeof(TQuestionLabel) == 0x5c && sizeof(TLogoutLabel) == 0x5c
		&& sizeof(TWaitLabel) == 0x54 && sizeof(TReconnectWindow) == 0x23c && sizeof(TLoginDialog) == 0x38
		&& sizeof(TCharacterListDialog) == 0x38 && sizeof(TMoveObjectsDialog) == 0x60 && sizeof(TOutfitDialog) == 0x27c,
	"sizes 1");
static_assert(sizeof(TTextDialog) == 0x44 && sizeof(TEditListDialog) == 0x38 && sizeof(TIgnoreEntry) == 0x1f
		&& sizeof(TIgnoreListDialog) == 0xd0 && sizeof(TBugReportDialog) == 0x30 && sizeof(TNewChannelDialog) == 0x44
		&& sizeof(TPrivateChannelDialog) == 0x34 && sizeof(TReportDialog) == 0x30 && sizeof(TAddBuddyDialog) == 0x30
		&& sizeof(TEditBuddyDialog) == 0x68 && sizeof(TUpdateDialog) == 0x44 && sizeof(THelpDialog) == 0x30,
	"sizes 2");
static_assert(sizeof(THintsDialog) == 0x30 && sizeof(TRuleViolationDialog) == 0x114 && sizeof(TEditMarkDialog) == 0xe8
		&& sizeof(TOptionsDialog) == 0x2c && sizeof(TGeneralOptionsDialog) == 0x44 && sizeof(TGraphicsOptionsDialog) == 0x4c
		&& sizeof(TAdvancedGraphicsDialog) == 0x54 && sizeof(TConsoleOptionsDialog) == 0x44
		&& sizeof(TNetworkOptionsDialog) == 0x34 && sizeof(THotkeyDialog) == 0x2464 && sizeof(TErrorReportDialog) == 0x30,
	"sizes 3");
static_assert(offsetof(TDialog, CancelCallback) == 0xbc && offsetof(TIgnoreListDialog, WhiteEntries) == 0x98
		&& offsetof(TOutfitDialog, Part) == 0x278 && offsetof(THotkeyDialog, OpenURL) == 0x2460
		&& offsetof(TRuleViolationDialog, Reasons) == 0x4c && offsetof(TEditMarkDialog, TypeButtons) == 0xb0
		&& offsetof(TReconnectWindow, TimeLabel) == 0x238 && offsetof(TLoginDialog, FocusGroup) == 0x34,
	"layouts");
#endif

// The test's own definitions: in a namespace of their own (the other tests, linked into the same executable,
// define classes of the same names at global scope).
namespace DialogsTest {

// --- Logs and names ------------------------------------------------------------------

enum { OURS = 0,
	ORIGINAL = 1 };
// The sides that have objects: both in the oracle, ours in the runner.
static const int SIDES = GOLDEN_ORACLE ? 2 : 1;
static int Side;
static bool Testing;
static std::string Log[2];
static FILE *LogFile;

// The error round (issue #34): the FailAt-th call of a fake (a logged call or
// a getter) throws an assertion of the calling side (0: none); Ticks counts
// one side's calls.
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
	char Buffer[2048];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}
static void Add(std::string &Out, const char *Format, ...)
{
	char Buffer[2048];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Out += Buffer;
}
template <class T> static void *Member(T Function) { return *(void **)&Function; }

// Image calls (thiscall members through a fastcall pointer, functions at their address). The runner has no
// image: the original's calls are never reached there (Both runs our side only); one that were would stop
// the runner.
#if GOLDEN_ORACLE
template <class R, class... A> static R CallI(uint32 Address, const void *This, A... Args)
{
	return ((R(__fastcall *)(const void *, void *, A...))(uintptr_t)Address)(This, NULL, Args...);
}
#else
static void NoOriginal()
{
	printf("golden: Dialogs: a call of the original in the runner\n");
	exit(1);
}
// (Called through a volatile pointer, so that the code after a call is not unreachable to the compiler, C4702.)
static void (*volatile StopNoOriginal)() = NoOriginal;
template <class R, class... A> static R CallI(uint32, const void *, A...)
{
	StopNoOriginal();
	return R();
}
template <class T> static T At(uint32)
{
	StopNoOriginal();
	return nullptr;
}
#endif

// The names of the objects of the current tree and the receivers.
static std::map<const void *, std::string> Names[2];

// --- Allocations and values ----------------------------------------------------------------
//
// A value is a pointer only by where it points, never by whether that memory
// is readable now (the heap grows and shrinks between the two sides' dumps):
// into a block one of the sides allocated while a round runs (both sides'
// operator new and new[], the image's malloc, the fakes' memory), into an
// image or module (the Tibia image, difftest.exe or the runner, the DLLs) or
// into the stack. The table of blocks is the same for both sides; a block
// freed during a round stays allocated (and in the table, as freed) until the
// round ends, so that a dangling pointer cannot point into a newer block of
// one side only.

// The table lives on a heap of its own: its nodes must not come from the
// operator new it records.
static HANDLE BlockHeap;
template <class T> struct TBlockAllocator {
	typedef T value_type;
	TBlockAllocator() {}
	template <class U> TBlockAllocator(const TBlockAllocator<U> &) {}
	T *allocate(size_t Count)
	{
		void *Memory = HeapAlloc(BlockHeap, 0, Count * sizeof(T));
		if (Memory == NULL)
			throw std::bad_alloc();
		return (T *)Memory;
	}
	void deallocate(T *Memory, size_t) { HeapFree(BlockHeap, 0, Memory); }
	template <class U> bool operator==(const TBlockAllocator<U> &) const { return true; }
	template <class U> bool operator!=(const TBlockAllocator<U> &) const { return false; }
};
struct TBlock {
	size_t Size;
	bool Freed;
};
typedef std::map<uintptr_t, TBlock, std::less<uintptr_t>, TBlockAllocator<std::pair<const uintptr_t, TBlock>>> TBlocks;
static TBlocks *Blocks;
// Frees of recorded blocks while the table must not change (the dumps).
static std::vector<void *, TBlockAllocator<void *>> *PendingFrees;
static int Untracked; // > 0: the test's own work (names, dumps), not recorded
static bool Tracking() { return Testing && Untracked == 0 && Blocks != NULL; }

static void *Record(void *Memory, size_t Size)
{
	if (Memory != NULL && Tracking())
		(*Blocks)[(uintptr_t)Memory] = TBlock{Size == 0 ? 1 : Size, false};
	return Memory;
}
// New memory of both sides gets the same byte pattern.
static void *__cdecl PatternNew(size_t Size)
{
	void *Memory = malloc(Size == 0 ? 1 : Size);
	if (Memory == NULL)
		throw std::bad_alloc();
	memset(Memory, 0xab, Size);
	return Record(Memory, Size);
}
#if GOLDEN_ORACLE
static void *__cdecl TrackedMalloc(size_t Size) { return Record(malloc(Size), Size); }
#endif
// Both sides' operator delete and delete[] and the image's free.
static void __cdecl TrackedFree(void *Memory)
{
	if (Memory == NULL)
		return;
	TBlocks::iterator B = Blocks == NULL ? TBlocks::iterator() : Blocks->find((uintptr_t)Memory);
	if (Blocks == NULL || B == Blocks->end()) {
		free(Memory);
		return;
	}
	if (B->second.Freed)
		return; // freed twice: it stays until the round ends
	if (Testing && Untracked > 0) {
		PendingFrees->push_back(Memory);
		return;
	}
	if (Testing) {
		B->second.Freed = true;
		return;
	}
	Blocks->erase(B);
	free(Memory);
}
#if !GOLDEN_ORACLE
// The runner's sized operator delete and delete[] (x64: the compiler calls them for complete types).
static void __cdecl TrackedSizedFree(void *Memory, size_t) { TrackedFree(Memory); }
#endif
static void ApplyPendingFrees()
{
	std::vector<void *, TBlockAllocator<void *>> Pending;
	Pending.swap(*PendingFrees);
	for (void *Memory : Pending)
		TrackedFree(Memory);
}
// At the end of a round: the freed blocks really go.
static void ReleaseFreedBlocks()
{
	for (TBlocks::iterator B = Blocks->begin(); B != Blocks->end();) {
		if (B->second.Freed) {
			free((void *)B->first);
			B = Blocks->erase(B);
		} else {
			++B;
		}
	}
}

// The images and modules and the stack (fixed for the test).
struct TRange {
	uintptr_t Start, End;
};
static std::vector<TRange> StaticRanges;
static TRange StackRange;
static void InitRanges()
{
#if GOLDEN_ORACLE
	IMAGE_NT_HEADERS32 *NT = (IMAGE_NT_HEADERS32 *)(uintptr_t)(0x400000 + ((IMAGE_DOS_HEADER *)0x400000)->e_lfanew);
	StaticRanges.push_back(TRange{0x400000, 0x400000 + NT->OptionalHeader.SizeOfImage});
#endif
	MEMORY_BASIC_INFORMATION M;
	// (The x86 oracle: below 2 GB, as before.)
	const uintptr_t Limit = GOLDEN_ORACLE ? 0x7fff0000 : ~(uintptr_t)0;
	for (uintptr_t Address = 0x10000; Address < Limit && VirtualQuery((void *)Address, &M, sizeof(M)) == sizeof(M);) {
		if (M.Type == MEM_IMAGE)
			StaticRanges.push_back(TRange{(uintptr_t)M.BaseAddress, (uintptr_t)M.BaseAddress + M.RegionSize});
		uintptr_t Next = (uintptr_t)M.BaseAddress + M.RegionSize;
		if (Next <= Address)
			break;
		Address = Next;
	}
	// The whole reservation of the stack, from its allocation base to the top.
	VirtualQuery(&M, &M, sizeof(M));
	StackRange = TRange{(uintptr_t)M.AllocationBase, (uintptr_t)((NT_TIB *)NtCurrentTeb())->StackBase};
}

enum { NUMBER,
	BLOCK,
	STATIC,
	STACK };
struct TPlace {
	int Kind;
	uintptr_t Start;
	size_t Size, Offset;
	bool Freed;
};
static TPlace Locate(uintptr_t Value)
{
	TPlace P = {NUMBER, 0, 0, 0, false};
	if (Blocks != NULL) {
		TBlocks::iterator B = Blocks->upper_bound(Value);
		if (B != Blocks->begin()) {
			--B;
			if (Value - B->first < B->second.Size) {
				P.Kind = BLOCK;
				P.Start = B->first;
				P.Size = B->second.Size;
				P.Offset = (size_t)(Value - B->first);
				P.Freed = B->second.Freed;
				return P;
			}
		}
	}
	for (TRange &R : StaticRanges) {
		if (Value >= R.Start && Value < R.End) {
			P.Kind = STATIC;
			P.Start = R.Start;
			P.Size = (size_t)(R.End - R.Start);
			P.Offset = (size_t)(Value - R.Start);
			return P;
		}
	}
	if (Value >= StackRange.Start && Value < StackRange.End) {
		P.Kind = STACK;
		P.Start = StackRange.Start;
		P.Size = (size_t)(StackRange.End - StackRange.Start);
		P.Offset = (size_t)(Value - StackRange.Start);
	}
	return P;
}
// A C string of printable characters that ends inside the memory it points into.
static bool ReadString(const TPlace &P, uintptr_t Value, std::string *Out)
{
	if (P.Kind == NUMBER)
		return false;
	const unsigned char *p = (const unsigned char *)Value;
	size_t Left = P.Size - P.Offset;
	std::string S;
	for (size_t i = 0; i < 300 && i < Left; i++) {
		if (P.Kind == STATIC && (i == 0 || ((uintptr_t)(p + i) & 0xfff) == 0)) {
			// Sections are mapped readable; the gaps of a module are not read.
			MEMORY_BASIC_INFORMATION M;
			if (VirtualQuery(p + i, &M, sizeof(M)) != sizeof(M) || M.State != MEM_COMMIT
				|| (M.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
				return false;
		}
		if (P.Kind == STACK && (uintptr_t)(p + i) < (uintptr_t)&S)
			return false; // below the current frame
		if (p[i] == 0) {
			*Out = S;
			return true;
		}
		if ((p[i] < 32 && p[i] != '\n' && p[i] != '\r') || p[i] >= 0x7f)
			return false;
		S += (char)p[i];
	}
	return false;
}

// The fill pattern in every byte of a pointer: a member nobody set (0xabababab on x86, the same in 64 bits).
static uintptr_t PatternPointer()
{
	uintptr_t Pattern;
	memset(&Pattern, 0xab, sizeof(Pattern));
	return Pattern;
}

static void *OurTimerVtable;
// A value: a known object's name, a string (Text 1: any, 2: only a non-empty
// one), a timer, a block (H, or D when freed, with the offset into it when
// it is not the start), an image or module address (S), a stack address (K),
// the fill pattern (ab) or a number.
static std::string Describe(int s, uintptr_t Value, int Text)
{
	auto N = Names[s].find((const void *)Value);
	if (N != Names[s].end())
		return N->second;
	if (Value == 0)
		return "0";
	if (Value == PatternPointer())
		return "ab";
	TPlace P = Locate(Value);
	char Buffer[48];
	std::string String;
	if (Text != 0 && ReadString(P, Value, &String) && (Text == 1 || String.size() > 0))
		return "'" + String + "'";
	switch (P.Kind) {
	case BLOCK:
		if (!P.Freed && P.Offset == 0 && P.Size >= sizeof(TTimer)) {
			void *V = *(void **)Value;
			if (V == OurTimerVtable || ORIGINAL_OR((uintptr_t)V == 0x5646e8, false)) {
				TTimer *T = (TTimer *)Value;
				return "Timer(" + std::to_string(T->Remaining) + "," + std::to_string(T->Active ? 1 : 0) + ","
					+ Describe(s, (uintptr_t)T->Callback.Receiver, 2) + "," + std::to_string(T->Callback.Event)
					+ ")";
			}
		}
		if (P.Offset == 0)
			return P.Freed ? "D" : "H";
		snprintf(Buffer, sizeof(Buffer), "%s+%zx", P.Freed ? "D" : "H", P.Offset);
		return Buffer;
	case STATIC:
		return "S";
	case STACK:
		return "K";
	}
	snprintf(Buffer, sizeof(Buffer), "%llx", (unsigned long long)Value);
	return Buffer;
}
static std::string Val(int s, uintptr_t Value) { return Describe(s, Value, 2); }
static std::string Val(uintptr_t Value) { return Val(Side, Value); }
static std::string Str(const char *Text) { return Text == NULL ? "(null)" : std::string("'") + Text + "'"; }

// --- The world -------------------------------------------------------------------------

struct TPreferences {
	int ViewMode, CreatureInfo, ConfineFramerate;
	bool ClassicControl, AutoChaseOff, ShowNewHints, TextualEffects, LightEffects, NoStretching, WindowsMousepointer;
	bool InfoMessages, EventMessages, StatusMessages, TimeStamps, PrivateMessages, TestServerLogin;
	TFullscreenMode FullscreenMode;
	char Hotkeys[HOTKEYS][256];
	bool AutoSend[HOTKEYS];
};

static struct {
	int ActionState, ActionNumber, MessageType, ConnectionState, Progress;
	int Characters, PremiumDays, ActiveCharacter, WindowWidth, WindowHeight;
	char CharacterNames[8][32], CharacterWorlds[8][32];
	TObject DragObject;
	int DragIndex, DragX, DragY, DragZ, TargetX, TargetY, TargetZ;
	uint32 RightSeed, FlagSeed, IgnoreSeed;
	int ErrorReports;
	bool ClipboardEmpty;
	const char *Clipboard;
	TPreferences Pref[2];
	bool KnownHints[16];
	char IgnoreText[400], BuddyIgnoreText[400];
	bool IgnoreListEnabled, IgnoreEnabled, BuddyIgnore, ModeIgnored[8];
	bool RequestOpen;
	uint32 PlayerID;
	int BuddyPosition, BuddyIcon;
	bool BuddyNotify, BuddyDescription;
	int Modes, Engines, Engine, FPSHundredths;
	TFullscreenMode ModeList[6];
	bool NoPlayer, NoHelpTitles; // the error round's assertions: no player, no help title matches
} World;

static TCreature Player;

static const char *const CharacterNamePool[] = {"Knight", "Sorcerer", "Druid", "Paladin", "Eremo", "Cip"};
static const char *const WorldPool[] = {"Antica", "Secura", "Nova", "Amera"};
static const char *const NamePool[] = {"Alpha", "beta", "Gamma Ray", "delta", "Eremo", "cipsoft", "Zed", "omega one",
	"new entry", "Bubble"};
static const char *const RightNames[] = {"Offensive Name", "Invalid Name Format", "Unsuitable Name", "Name Inciting",
	"Offensive Statement", "Spamming", "Illegal Advertising", "Off-Topic Public", "Non-English", "Inciting",
	"Bug Abuse", "Game Weakness", "Macro", "Unofficial Software", "Hacking", "Multi-Clienting", "Account Trading",
	"Account Sharing", "Threatening", "Pretending", "Destructive", "False Report"};
static const char *const ActionNames[] = {"Notation", "Name Report", "Banishment", "Name Report+Banishment",
	"Banishment+Final Warning", "Name Report+Banishment+Final Warning", "Statement Report"};
static const char *const HelpTitles[] = {"", "Moving", "Fighting", "Looting", "Shops", "Depots", "Houses", "Spells",
	"Food", "Death", "Parties", "Guilds", "Trade", "Quests", "Skills", "Last"};
static const char *const HelpTexts[] = {"", "Text of moving", "How to fight\nwell", "Loot <b>all</b>", "Buy here",
	"Depot text", "Houses text", "Spells text", "Eat", "Dying", "Join a party", "Guild text", "Trading", "Quest text",
	"Skill text", "Last text"};
static const char *const ReportTitles[] = {"Crash 1", "Second crash", "Third"};
static const char *const ReportTexts[] = {"Stack:\n0x401000", "Registers eax=1", "Text of three"};

static bool Hash(uint32 Seed, int a, int b, int Percent)
{
	uint32 h = Seed ^ (uint32)(a * 2654435761u) ^ (uint32)(b * 40503u);
	h ^= h >> 13;
	h *= 0x5bd1e995;
	h ^= h >> 15;
	return (int)(h % 100) < Percent;
}

// --- The fakes ---------------------------------------------------------------------------

static int CharWidth(int Font, char c) { return 4 + ((unsigned char)c + Font) % 5; }
static int __cdecl FakeGetTextPartWidth(int Font, const char *Text, int Length)
{
	Tick();
	int Width = 0;
	for (int i = 0; i < Length && Text[i] != 0; i++)
		Width += CharWidth(Font, Text[i]);
	return Width;
}
static int __cdecl FakeGetTextWidth(int Font, const char *Text)
{
	Tick();
	return FakeGetTextPartWidth(Font, Text, 1 << 30);
}
static int __cdecl FakeGetFittingTextLength(int Font, const char *Text, int Width)
{
	Tick();
	int Length = 0, Used = 0;
	while (Text[Length] != 0 && Used + CharWidth(Font, Text[Length]) <= Width)
		Used += CharWidth(Font, Text[Length++]);
	return Length;
}
static int __cdecl FakeGetFontHeight(int Font)
{
	Tick();
	return 8 + (Font & 3);
}
static int __cdecl FakeGetFontLineHeight(int Font)
{
	Tick();
	return 11 + (Font & 3);
}
static void __cdecl FakeGetTextPosition(int *TextX, int *TextY, int x, int y, int Width, int Height, TFont *Format)
{
	Tick();
	*TextX = x + Width / 3 + Format->Alignment;
	*TextY = y + Height / 4 + Format->FontNumber;
}
static void __cdecl FakePrintTextClipped(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Alignment, TRect Clip)
{
	Logf("PrintC%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d;", Surface, x, y, FontNumber, Color.R, Color.G, Color.B, Text,
		Alignment, Clip.x, Clip.y, Clip.width, Clip.height);
}
static void __cdecl FakePrintFormattedText(int Surface, int x, int y, TFont F, const char *Text)
{
	Logf("PrintF%d,%d,%d,%d,%d,%d,%d,%d,%s;", Surface, x, y, F.FontNumber, F.Color.R, F.Color.G, F.Color.B, F.Alignment,
		Text);
}
static void __cdecl FakePrintFormattedTextPart(int Surface, int x, int y, TFont F, const char *Text, int Length)
{
	std::string Part(Text, Length > 0 ? Length : 0);
	Logf("PrintP%d,%d,%d,%d,%d,%d,%d,%d,%s,%d;", Surface, x, y, F.FontNumber, F.Color.R, F.Color.G, F.Color.B,
		F.Alignment, Part.c_str(), Length);
}
static void __cdecl FakeDrawObject(int Surface, int x, int y, int Size, TObject Obj, TRGBColor H, TRect Clip, TFont F,
	bool ShowCount)
{
	Logf("Obj%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Size, Obj.Type, Obj.Data,
		Obj.Data2, H.R, H.G, H.B, Clip.x, Clip.y, Clip.width, Clip.height, F.FontNumber, F.Color.R, F.Color.G, F.Color.B,
		F.Alignment, ShowCount ? 1 : 0);
}
static void __cdecl FakeDrawOutfit(int Surface, int x, int y, int Size, int Outfit, int *Colors, TRGBColor H, TRect Clip,
	TFont F)
{
	Logf("Outfit%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Size, Outfit, Colors[0],
		Colors[1], Colors[2], Colors[3], H.R, H.G, H.B, Clip.x, Clip.y, Clip.width, Clip.height, F.FontNumber, F.Alignment);
}
static void __cdecl FakeDrawPicturePart(int Surface, int x, int y, int Picture, int SourceX, int SourceY, int Width,
	int Height)
{
	Logf("Pic%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Picture, SourceX, SourceY, Width, Height);
}
static void __cdecl FakeSetMouseOverText(const char *Text) { Logf("Over%s;", Text == NULL ? "-" : Text); }
static void __cdecl FakeSetMouseCursor(int Cursor) { Logf("Cursor%d;", Cursor); }
static int __cdecl FakeGetActionState()
{
	Tick();
	return World.ActionState;
}
static int __cdecl FakeGetActionNumber()
{
	Tick();
	return World.ActionNumber;
}
static void __cdecl FakeSetActionState(int State) { Logf("Action%d;", State); }
static void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	Logf("Msg%d,%s,%s;", Type, Val((uintptr_t)Param1).c_str(), Val((uintptr_t)Param2).c_str());
}
static void __cdecl FakeAddMessageFirst(int Type, INT_PTR Param1, INT_PTR Param2)
{
	Logf("First%d,%s,%s;", Type, Val((uintptr_t)Param1).c_str(), Val((uintptr_t)Param2).c_str());
}
static int __cdecl FakePeekMessageType()
{
	Tick();
	return World.MessageType;
}
static void __cdecl FakeClearMessages() { Logf("ClearMessages;"); }
static int __cdecl FakeGetConnectionState()
{
	Tick();
	return World.ConnectionState;
}
static int __cdecl FakeGetProgress()
{
	Tick();
	return World.Progress;
}
static int __cdecl FakeGetAccountCharacters()
{
	Tick();
	return World.Characters;
}
static int __cdecl FakeGetPremiumDays()
{
	Tick();
	return World.PremiumDays;
}
static int __cdecl FakeGetActiveCharacter()
{
	Tick();
	return World.ActiveCharacter;
}
static void __cdecl FakeSetActiveCharacter(int Number) { Logf("Active%d;", Number); }
static const char *__cdecl FakeGetCharacterName(int Number)
{
	Tick();
	return World.CharacterNames[Number & 7];
}
static const char *__cdecl FakeGetCharacterWorld(int Number)
{
	Tick();
	return World.CharacterWorlds[Number & 7];
}
static void __cdecl FakeSetAccount(int Number, const char *Password) { Logf("Account%d,%s;", Number, Str(Password).c_str()); }
static TObject *__cdecl FakeGetDragObject(int *Index, int *x, int *y, int *z)
{
	Tick();
	*Index = World.DragIndex;
	*x = World.DragX;
	*y = World.DragY;
	*z = World.DragZ;
	return &World.DragObject;
}
static void __cdecl FakeGetDragTarget(int *x, int *y, int *z)
{
	Tick();
	*x = World.TargetX;
	*y = World.TargetY;
	*z = World.TargetZ;
}
static int __cdecl FakeGetWindowWidth()
{
	Tick();
	return World.WindowWidth;
}
static int __cdecl FakeGetWindowHeight()
{
	Tick();
	return World.WindowHeight;
}
static bool __cdecl FakeCheckRight(int Right, int Bit)
{
	Tick();
	return Hash(World.RightSeed, Right, Bit, Bit < 0 ? 60 : 50);
}
static const char *__cdecl FakeGetRightName(int Right)
{
	Tick();
	return RightNames[Right % 22];
}
static const char *__cdecl FakeGetActionName(int Action)
{
	Tick();
	return ActionNames[Action % 7];
}
static int __cdecl FakeGetNumberOfErrorReports()
{
	Tick();
	return World.ErrorReports;
}
static const char *__cdecl FakeGetErrorReportTitle(int Number)
{
	Tick();
	return ReportTitles[Number % 3];
}
static const char *__cdecl FakeGetErrorReportDate(int Number)
{
	Tick();
	return Number % 2 ? "2006-06-08 12:00" : "2006-06-09";
}
static const char *__cdecl FakeGetErrorReportText(int Number)
{
	Tick();
	return ReportTexts[Number % 3];
}
static void __cdecl FakeSetErrorReportComment(const char *Comment) { Logf("Comment%s;", Str(Comment).c_str()); }
static void __cdecl FakeClearErrorReports() { Logf("ClearReports;"); }
static void __cdecl FakeDeleteErrorFile() { Logf("DeleteErrorFile;"); }
static void __cdecl FakeCheckClipboardText(const char *Text, int Length)
{
	Logf("Clip%s,%d;", std::string(Text, Length > 0 ? Length : 0).c_str(), Length);
}
static bool __cdecl FakeIsClipboardEmpty()
{
	Tick();
	return World.ClipboardEmpty;
}
static char *__cdecl FakeGetClipboardText()
{
	Tick();
	return (char *)World.Clipboard;
}

// Preferences.cpp: one copy per side (not every setter is redirected).
#define PREF(Type, Name) \
	static Type __cdecl FakeGet##Name() \
	{ \
		Tick(); \
		return World.Pref[Side].Name; \
	} \
	[[maybe_unused]] static void __cdecl FakeSet##Name(Type Value) \
	{ \
		Logf("Set" #Name "%d;", (int)Value); \
		World.Pref[Side].Name = Value; \
	}
PREF(int, ViewMode)
PREF(int, CreatureInfo)
PREF(int, ConfineFramerate)
PREF(bool, ClassicControl)
PREF(bool, AutoChaseOff)
PREF(bool, ShowNewHints)
PREF(bool, TextualEffects)
PREF(bool, LightEffects)
PREF(bool, NoStretching)
PREF(bool, WindowsMousepointer)
PREF(bool, InfoMessages)
PREF(bool, EventMessages)
PREF(bool, StatusMessages)
PREF(bool, TimeStamps)
PREF(bool, PrivateMessages)
PREF(bool, TestServerLogin)
#undef PREF
static TFullscreenMode *__cdecl FakeGetFullscreenMode()
{
	Tick();
	return &World.Pref[Side].FullscreenMode;
}
static void __cdecl FakeSetFullscreenMode(const TFullscreenMode *Mode)
{
	Logf("SetMode%d,%d,%d,%d;", Mode->Width, Mode->Height, Mode->Depth, Mode->Frequency);
	World.Pref[Side].FullscreenMode = *Mode;
}
static const char *__cdecl FakeGetHotkey(int Number, bool *AutoSend)
{
	Tick();
	*AutoSend = World.Pref[Side].AutoSend[Number];
	return World.Pref[Side].Hotkeys[Number];
}
static void __cdecl FakeSetHotkey(int Number, const char *Text, bool AutoSend)
{
	Logf("Hotkey%d,%s,%d;", Number, Str(Text).c_str(), AutoSend ? 1 : 0);
}
static void __cdecl FakeSavePreferences() { Logf("SavePreferences;"); }
static bool __cdecl FakeIsKnownHint(int Number)
{
	Tick();
	return World.KnownHints[Number & 15];
}

// Talk.cpp: the lists come as texts the dialog frees.
static char *__cdecl FakeGetIgnoreListText()
{
	Tick();
	if (World.IgnoreText[0] == 0 && Hash(World.IgnoreSeed, 1, 1, 30))
		return NULL;
	char *Text = new char[strlen(World.IgnoreText) + 1];
	strcpy(Text, World.IgnoreText);
	return Text;
}
static char *__cdecl FakeGetBuddyIgnoreText()
{
	Tick();
	if (World.BuddyIgnoreText[0] == 0 && Hash(World.IgnoreSeed, 2, 2, 30))
		return NULL;
	char *Text = new char[strlen(World.BuddyIgnoreText) + 1];
	strcpy(Text, World.BuddyIgnoreText);
	return Text;
}
static bool __cdecl FakeIsPermanentIgnore(const char *Name)
{
	Tick();
	return Hash(World.IgnoreSeed, Name[0], (int)strlen(Name), 50);
}
static bool __cdecl FakeIsPermanentBuddyIgnore(const char *Name)
{
	Tick();
	return Hash(World.IgnoreSeed, Name[1], 7, 50);
}
static bool __cdecl FakeIsModeIgnored(int Mode)
{
	Tick();
	return World.ModeIgnored[Mode & 7];
}
static bool __cdecl FakeGetIgnoreListEnabled()
{
	Tick();
	return World.IgnoreListEnabled;
}
static void __cdecl FakeSetIgnoreListEnabled(bool Value) { Logf("IgnoreList%d;", Value ? 1 : 0); }
static bool __cdecl FakeGetIgnoreEnabled()
{
	Tick();
	return World.IgnoreEnabled;
}
static void __cdecl FakeSetIgnoreEnabled(bool Value) { Logf("IgnoreEnabled%d;", Value ? 1 : 0); }
static bool __cdecl FakeGetBuddyIgnore()
{
	Tick();
	return World.BuddyIgnore;
}
static void __cdecl FakeSetBuddyIgnore(bool Value) { Logf("BuddyIgnore%d;", Value ? 1 : 0); }
static void __cdecl FakeClearIgnoreList() { Logf("ClearIgnore;"); }
static void __cdecl FakeIgnoreName(const char *Name, bool Permanent) { Logf("Ignore%s,%d;", Str(Name).c_str(), Permanent ? 1 : 0); }
static void __cdecl FakeIgnoreMode(int Mode) { Logf("IgnoreMode%d;", Mode); }
static void __cdecl FakeUnignoreMode(int Mode) { Logf("UnignoreMode%d;", Mode); }
static void __cdecl FakeClearBuddyIgnore() { Logf("ClearBuddyIgnore;"); }
static void __cdecl FakeAddBuddyIgnore(const char *Name, bool Permanent)
{
	Logf("AddBuddyIgnore%s,%d;", Str(Name).c_str(), Permanent ? 1 : 0);
}
static void __cdecl FakeSetConsoleChanged() { Logf("ConsoleChanged;"); }
static void __cdecl FakeSetStatusMessage(const char *Text, bool Permanent) { Logf("Status%s,%d;", Str(Text).c_str(), Permanent ? 1 : 0); }
static int RuleViolationBuffer;
static TTalkBuffer *__cdecl FakeGetTextBuffer(int Mode, const char *Name, int Channel)
{
	Logf("GetTextBuffer%d,%s,%d;", Mode, Str(Name).c_str(), Channel);
	return World.RequestOpen ? (TTalkBuffer *)&RuleViolationBuffer : NULL;
}

// Creatures.cpp
static uint32 __cdecl FakeGetPlayerID()
{
	Tick();
	return World.PlayerID;
}
static TCreature *__cdecl FakeGetCreature(uint32 ID)
{
	Tick();
	return ID == World.PlayerID && !World.NoPlayer ? &Player : NULL;
}
static int __cdecl FakeGetBuddyPosition(uint32)
{
	Tick();
	return World.BuddyPosition;
}
static const char *__cdecl FakeGetBuddyListName(int Position)
{
	Tick();
	return NamePool[Position % 10];
}
static int __cdecl FakeGetBuddyListIcon(int)
{
	Tick();
	return World.BuddyIcon;
}
static const char *__cdecl FakeGetBuddyListDescription(int)
{
	Tick();
	return World.BuddyDescription ? "a friend\nfrom the old days" : NULL;
}
static bool __cdecl FakeGetBuddyListNotify(int)
{
	Tick();
	return World.BuddyNotify;
}
static void __cdecl FakeEditBuddy(int ID, int Icon, const char *Description, bool Notify)
{
	Logf("EditBuddy%d,%d,%s,%d;", ID, Icon, Str(Description).c_str(), Notify ? 1 : 0);
}

// Help.cpp, Objects.cpp, Map.cpp, Control.cpp
static const char *__cdecl FakeGetEventHelpTitle(int Number)
{
	Tick();
	return World.NoHelpTitles ? "Unknown" : HelpTitles[Number & 15];
}
static const char *__cdecl FakeGetEventHelpText(int Number)
{
	Tick();
	return HelpTexts[Number & 15];
}
static bool __cdecl FakeGetObjectFlag(int Type, int Flag)
{
	Tick();
	return Hash(World.FlagSeed, Type, Flag, 40);
}
static void __cdecl FakeGetCumulativePattern(TObject &Obj, int *PatternX, int *PatternY)
{
	Logf("Pattern%d,%d;", Obj.Type, Obj.Data);
	*PatternX = Obj.Data % 4;
	*PatternY = Obj.Data / 4;
}
static void __cdecl FakeAddAutomapMark(TCoordinate &C, const char *Text, int Type)
{
	Logf("AddMark%d,%d,%d,%s,%d;", C.x, C.y, C.z, Str(Text).c_str(), Type);
}
static void __cdecl FakeDeleteAutomapMark(TMark &Mark, int z)
{
	Logf("DeleteMark%d,%d,%d,%s,%d;", Mark.x, Mark.y, Mark.Type, Str(Mark.Text).c_str(), z);
}
static void __cdecl FakeResize(int Width, int Height) { Logf("Resize%d,%d;", Width, Height); }

// Communication.cpp
static void __cdecl FakeSendLogout() { Logf("SendLogout;"); }
static void __cdecl FakeSendMoveObject(int x, int y, int z, int Type, int Position, int dx, int dy, int dz, int Count)
{
	Logf("SendMove%d,%d,%d,%d,%d,%d,%d,%d,%d;", x, y, z, Type, Position, dx, dy, dz, Count);
}
static void __cdecl FakeSendJoinChannel(int ID) { Logf("SendJoin%d;", ID); }
static void __cdecl FakeSendOpenPrivateConversation(const char *Name) { Logf("SendConversation%s;", Str(Name).c_str()); }
static void __cdecl FakeSendInviteToPrivateChannel(const char *Name) { Logf("SendInvite%s;", Str(Name).c_str()); }
static void __cdecl FakeSendExcludeFromPrivateChannel(const char *Name) { Logf("SendExclude%s;", Str(Name).c_str()); }
static void __cdecl FakeSendAddBuddy(const char *Name) { Logf("SendAddBuddy%s;", Str(Name).c_str()); }
static void __cdecl FakeSendEditText(uint32 ObjectNumber, const char *Text)
{
	Logf("SendEditText%u,%s;", ObjectNumber, Str(Text).c_str());
}
static void __cdecl FakeSendEditHouseText(int Type, uint32 Identifier, const char *Text)
{
	Logf("SendHouseText%d,%u,%s;", Type, Identifier, Str(Text).c_str());
}
static void __cdecl FakeSendOpenPrivateChannel() { Logf("SendOpenPrivateChannel;"); }
static void __cdecl FakeSendOutfit(int OutfitType, const int *Colors)
{
	Logf("SendOutfit%d,%d,%d,%d,%d;", OutfitType, Colors[0], Colors[1], Colors[2], Colors[3]);
}
static void __cdecl FakeSendBugReport(const char *Text) { Logf("SendBugReport%s;", Str(Text).c_str()); }
static void __cdecl FakeSendRuleViolationReport(const char *Name, uint8 Reason, uint8 Action, const char *Comment,
	uint32 StatementID, bool IPBanishment)
{
	Logf("SendReport%s,%d,%d,%s,%u,%d;", Str(Name).c_str(), Reason, Action, Str(Comment).c_str(), StatementID,
		IPBanishment ? 1 : 0);
}
static void __cdecl FakeCancelPatch() { Logf("CancelPatch;"); }

} // namespace DialogsTest

#if GOLDEN_ORACLE
// Not defined elsewhere in difftest.exe: our side's definitions (the image's go to the fakes as well). The
// runner has client_stubs.cpp's and redirects them.
void SendEditText(uint32 ObjectNumber, const char *Text) { DialogsTest::FakeSendEditText(ObjectNumber, Text); }
void SendEditHouseText(int Type, uint32 Identifier, const char *Text)
{
	DialogsTest::FakeSendEditHouseText(Type, Identifier, Text);
}
void SendOpenPrivateChannel() { DialogsTest::FakeSendOpenPrivateChannel(); }
void SendOutfit(int OutfitType, const int *Colors) { DialogsTest::FakeSendOutfit(OutfitType, Colors); }
void SendBugReport(const char *Text) { DialogsTest::FakeSendBugReport(Text); }
void SendRuleViolationReport(const char *Name, uint8 Reason, uint8 Action, const char *Comment, uint32 StatementID,
	bool IPBanishment)
{
	DialogsTest::FakeSendRuleViolationReport(Name, Reason, Action, Comment, StatementID, IPBanishment);
}
void CancelPatch() { DialogsTest::FakeCancelPatch(); }
#endif

namespace DialogsTest {

static void __cdecl FakeOpenURL(const char *URL) { Logf("URL%s;", URL); }
static void __cdecl FakeSetDisplayMode(int Mode) { Logf("DisplayMode%d;", Mode); }

#if GOLDEN_ORACLE
// The CRT of the image.
static int __cdecl ImageSprintf(char *Buffer, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vsprintf(Buffer, Format, Args);
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
static void __cdecl ImageQsort(void *Base, size_t Number, size_t Width, int(__cdecl *Compare)(const void *, const void *))
{
	qsort(Base, Number, Width, Compare);
}
static long __cdecl ImageAtol(const char *Text) { return atol(Text); }
static int __cdecl ImageStricmp(const char *a, const char *b) { return _stricmp(a, b); }
#endif

// The surface: the calls DialogElements.cpp and Dialogs.cpp make. The modes
// are new memory the graphics dialog frees.
class TDGSurface : public TTestSurface {
public:
	void FillRectangle(int Surface, TRect R, TRGBColor C) override
	{
		Logf("Fill%d,%d,%d,%d,%d,%d,%d,%d;", Surface, R.x, R.y, R.width, R.height, C.R, C.G, C.B);
	}
	void DrawVerticalLine(int Surface, int x, int y, int LineHeight, TRGBColor C) override
	{
		Logf("VLine%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, LineHeight, C.R, C.G, C.B);
	}
	bool CanShadeRectangles() override
	{
		Tick();
		return (World.Engines & 4) != 0;
	}
	int GetFullscreenModes(TFullscreenMode **Modes) override
	{
		Tick();
		*Modes = new TFullscreenMode[World.Modes == 0 ? 1 : World.Modes];
		for (int i = 0; i < World.Modes; i++)
			(*Modes)[i] = World.ModeList[i];
		return World.Modes;
	}
	void SetFramerateLimit(int Limit) override { Logf("Framerate%d;", Limit); }
	double GetFPS() override
	{
		Tick();
		return World.FPSHundredths / 100.0;
	}
};
static TDGSurface *Surface;
static TSurface *__cdecl FakeGetSurface()
{
	Tick();
	return Surface;
}
// A TSurface member without arguments (thiscall: the surface in ECX).
static uint8 GOLDEN_THISCALL FakeGetAvailableEngines(GOLDEN_THIS)
{
	Tick();
	return (uint8)(World.Engines & 3);
}

// The receivers of the callbacks that do not go to the dialog.
class TDGReceiver : public TElement {
public:
	void HandleEvent(int Event) override { Logf("Event%s,%d;", Val((uintptr_t)this).c_str(), Event); }
};
static TDGReceiver *Receiver[2];

// --- Patches -----------------------------------------------------------------------------

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
}

// Ours of the functions the oracle's difftest.exe defines in this file (SendEditText ... CancelPatch): NULL
// there (the image goes to the fakes directly), the runner's stubs (client_stubs.cpp) here.
#if GOLDEN_ORACLE
#define DEFINED_HERE(Function) NULL
#else
#define DEFINED_HERE(Function) ((void *)Function)
#endif

#define F(Name) (void *)Fake##Name
static void PatchCallees()
{
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		// graphics.cpp, Surface.cpp
		{0x491b80, (void *)GetTextPartWidth, F(GetTextPartWidth)},
		{0x494570, (void *)GetTextWidth, F(GetTextWidth)},
		{0x491f50, (void *)GetFittingTextLength, F(GetFittingTextLength)},
		{0x491710, (void *)GetFontHeight, F(GetFontHeight)},
		{0x48b430, (void *)GetFontLineHeight, F(GetFontLineHeight)},
		{0x492260, (void *)GetTextPosition, F(GetTextPosition)},
		{0x4934e0, (void *)PrintTextClipped, F(PrintTextClipped)},
		{0x493f60, (void *)PrintFormattedText, F(PrintFormattedText)},
		{0x4943b0, (void *)PrintFormattedTextPart, F(PrintFormattedTextPart)},
		{0x494ca0, (void *)DrawObject, F(DrawObject)},
		{0x495080, (void *)DrawOutfit, F(DrawOutfit)},
		{0x490c60, (void *)DrawPicturePart, F(DrawPicturePart)},
		{0x4eaa90, (void *)GetSurface, F(GetSurface)},
		{0x4eabe0, Member(&TSurface::GetAvailableEngines), F(GetAvailableEngines)},
		// Status.cpp
		{0x4e72e0, (void *)SetMouseOverText, F(SetMouseOverText)},
		{0x4e6e80, (void *)SetMouseCursor, F(SetMouseCursor)},
		{0x4e6e30, (void *)GetActionState, F(GetActionState)},
		{0x4e6e00, (void *)GetActionNumber, F(GetActionNumber)},
		{0x4e6e20, (void *)SetActionState, F(SetActionState)},
		{0x4e9020, (void *)AddMessage, F(AddMessage)},
		{0x4e91d0, (void *)AddMessageFirst, F(AddMessageFirst)},
		{0x4e75d0, (void *)PeekMessageType, F(PeekMessageType)},
		{0x4e76e0, (void *)ClearMessages, F(ClearMessages)},
		{0x4e71f0, (void *)GetConnectionState, F(GetConnectionState)},
		{0x4e7210, (void *)GetProgress, F(GetProgress)},
		{0x4e72a0, (void *)GetAccountCharacters, F(GetAccountCharacters)},
		{0x4e72b0, (void *)GetPremiumDays, F(GetPremiumDays)},
		{0x4e72c0, (void *)GetActiveCharacter, F(GetActiveCharacter)},
		{0x4e9c50, (void *)SetActiveCharacter, F(SetActiveCharacter)},
		{0x4e9db0, (void *)GetCharacterName, F(GetCharacterName)},
		{0x4e9f20, (void *)GetCharacterWorld, F(GetCharacterWorld)},
		{0x4e93d0, (void *)SetAccount, F(SetAccount)},
		{0x4e7030, (void *)GetDragObject, F(GetDragObject)},
		{0x4e7070, (void *)GetDragTarget, F(GetDragTarget)},
		{0x4e6f20, (void *)GetWindowWidth, F(GetWindowWidth)},
		{0x4e6f30, (void *)GetWindowHeight, F(GetWindowHeight)},
		{0x4e86e0, (void *)CheckRight, F(CheckRight)},
		{0x4e88c0, (void *)GetRightName, F(GetRightName)},
		{0x4e8de0, (void *)GetActionName, F(GetActionName)},
		{0x4e7e10, (void *)GetNumberOfErrorReports, F(GetNumberOfErrorReports)},
		{0x4e7e40, (void *)GetErrorReportTitle, F(GetErrorReportTitle)},
		{0x4e7e70, (void *)GetErrorReportDate, F(GetErrorReportDate)},
		{0x4e7ea0, (void *)GetErrorReportText, F(GetErrorReportText)},
		{0x4e7d60, (void *)SetErrorReportComment, F(SetErrorReportComment)},
		{0x4e7bb0, (void *)ClearErrorReports, F(ClearErrorReports)},
		{0x4e7380, (void *)DeleteErrorFile, F(DeleteErrorFile)},
		{0x4ea370, (void *)CheckClipboardText, F(CheckClipboardText)},
		// Ours of IsClipboardEmpty is contextmenus_tests.cpp's, of GetClipboardText dialogelements_tests.cpp's
		// (the runner's: sysinc.cpp's, golden_sysinc).
		{0x4fcbe0, (void *)IsClipboardEmpty, F(IsClipboardEmpty)},
		{0x4fcff0, (void *)GetClipboardText, F(GetClipboardText)},
		// Preferences.cpp
		{0x4db3a0, (void *)GetViewMode, F(GetViewMode)},
		{0x4db3b0, (void *)GetFullscreenMode, F(GetFullscreenMode)},
		{0x4db2d0, (void *)SetFullscreenMode, F(SetFullscreenMode)},
		{0x4db3f0, (void *)GetCreatureInfo, F(GetCreatureInfo)},
		{0x4db330, (void *)SetCreatureInfo, F(SetCreatureInfo)},
		{0x4db410, (void *)GetConfineFramerate, F(GetConfineFramerate)},
		{0x4db380, (void *)SetConfineFramerate, F(SetConfineFramerate)},
		{0x4db640, (void *)GetClassicControl, F(GetClassicControl)},
		{0x4db620, (void *)SetClassicControl, F(SetClassicControl)},
		{0x4db650, (void *)GetAutoChaseOff, F(GetAutoChaseOff)},
		{0x4db630, (void *)SetAutoChaseOff, F(SetAutoChaseOff)},
		{0x4db6d0, (void *)GetShowNewHints, F(GetShowNewHints)},
		{0x4db6c0, (void *)SetShowNewHints, F(SetShowNewHints)},
		{0x4db3e0, (void *)GetTextualEffects, F(GetTextualEffects)},
		{0x4db320, (void *)SetTextualEffects, F(SetTextualEffects)},
		{0x4db3d0, (void *)GetLightEffects, F(GetLightEffects)},
		{0x4db310, (void *)SetLightEffects, F(SetLightEffects)},
		{0x4db400, (void *)GetNoStretching, F(GetNoStretching)},
		{0x4db340, (void *)SetNoStretching, F(SetNoStretching)},
		{0x4db3c0, (void *)GetWindowsMousepointer, F(GetWindowsMousepointer)},
		{0x4db300, (void *)SetWindowsMousepointer, F(SetWindowsMousepointer)},
		{0x4db5c0, (void *)GetInfoMessages, F(GetInfoMessages)},
		{0x4db570, (void *)SetInfoMessages, F(SetInfoMessages)},
		{0x4db5d0, (void *)GetEventMessages, F(GetEventMessages)},
		{0x4db580, (void *)SetEventMessages, F(SetEventMessages)},
		{0x4db5e0, (void *)GetStatusMessages, F(GetStatusMessages)},
		{0x4db590, (void *)SetStatusMessages, F(SetStatusMessages)},
		{0x4db5f0, (void *)GetTimeStamps, F(GetTimeStamps)},
		{0x4db5a0, (void *)SetTimeStamps, F(SetTimeStamps)},
		{0x4db600, (void *)GetPrivateMessages, F(GetPrivateMessages)},
		{0x4db5b0, (void *)SetPrivateMessages, F(SetPrivateMessages)},
		{0x4db2a0, (void *)GetTestServerLogin, F(GetTestServerLogin)},
		{0x4db2b0, (void *)SetTestServerLogin, F(SetTestServerLogin)},
		{0x4dc7e0, (void *)GetHotkey, F(GetHotkey)},
		{0x4dc450, (void *)SetHotkey, F(SetHotkey)},
		{0x4de890, (void *)SavePreferences, F(SavePreferences)},
		{0x4dbe50, (void *)IsKnownHint, F(IsKnownHint)},
		// Talk.cpp
		{0x501010, (void *)GetIgnoreListText, F(GetIgnoreListText)},
		{0x5002d0, (void *)GetBuddyIgnoreText, F(GetBuddyIgnoreText)},
		{0x501ae0, (void *)IsPermanentIgnore, F(IsPermanentIgnore)},
		{0x5008d0, (void *)IsPermanentBuddyIgnore, F(IsPermanentBuddyIgnore)},
		{0x501260, (void *)IsModeIgnored, F(IsModeIgnored)},
		{0x4fd530, (void *)GetIgnoreListEnabled, F(GetIgnoreListEnabled)},
		{0x4fd520, (void *)SetIgnoreListEnabled, F(SetIgnoreListEnabled)},
		{0x4fd4f0, (void *)GetIgnoreEnabled, F(GetIgnoreEnabled)},
		{0x4fd4e0, (void *)SetIgnoreEnabled, F(SetIgnoreEnabled)},
		{0x4fd510, (void *)GetBuddyIgnore, F(GetBuddyIgnore)},
		{0x4fd500, (void *)SetBuddyIgnore, F(SetBuddyIgnore)},
		{0x501400, (void *)ClearIgnoreList, F(ClearIgnoreList)},
		{0x501940, (void *)IgnoreName, F(IgnoreName)},
		{0x5017b0, (void *)IgnoreMode, F(IgnoreMode)},
		{0x501e70, (void *)UnignoreMode, F(UnignoreMode)},
		{0x500520, (void *)ClearBuddyIgnore, F(ClearBuddyIgnore)},
		{0x5006f0, (void *)AddBuddyIgnore, F(AddBuddyIgnore)},
		{0x4fd590, (void *)SetConsoleChanged, F(SetConsoleChanged)},
		{0x4ff500, (void *)SetStatusMessage, F(SetStatusMessage)},
		{0x5025c0, (void *)GetTextBuffer, F(GetTextBuffer)},
		// Creatures.cpp
		{0x44b9c0, (void *)GetPlayerID, F(GetPlayerID)},
		{0x44c6e0, (void *)GetCreature, F(GetCreature)},
		{0x44bb80, (void *)GetBuddyPosition, F(GetBuddyPosition)},
		{0x44feb0, (void *)GetBuddyListName, F(GetBuddyListName)},
		{0x450330, (void *)GetBuddyListIcon, F(GetBuddyListIcon)},
		{0x4504a0, (void *)GetBuddyListDescription, F(GetBuddyListDescription)},
		{0x450740, (void *)GetBuddyListNotify, F(GetBuddyListNotify)},
		{0x44f230, (void *)EditBuddy, F(EditBuddy)},
		// Help.cpp, Objects.cpp, Map.cpp, Control.cpp (ours of Resize is playertest.cpp's; the runner's
		// client_stubs.cpp's)
		{0x4a3b60, (void *)GetEventHelpTitle, F(GetEventHelpTitle)},
		{0x4a3fd0, (void *)GetEventHelpText, F(GetEventHelpText)},
		{0x4d1c40, (void *)(bool (*)(int, int))GetObjectFlag, F(GetObjectFlag)},
		{0x4d6800, (void *)GetCumulativePattern, F(GetCumulativePattern)},
		{0x4b4f60, (void *)AddAutomapMark, F(AddAutomapMark)},
		{0x4b5410, (void *)DeleteAutomapMark, F(DeleteAutomapMark)},
		{0x4459b0, (void *)Resize, F(Resize)},
		// Communication.cpp: ours of the first ones are the other tests' fakes (the runner's stubs); the rest
		// are this file's (the oracle's) or the runner's stubs.
		{0x404350, (void *)SendLogout, F(SendLogout)},
		{0x404db0, (void *)SendMoveObject, F(SendMoveObject)},
		{0x4074a0, (void *)SendJoinChannel, F(SendJoinChannel)},
		{0x407800, (void *)SendOpenPrivateConversation, F(SendOpenPrivateConversation)},
		{0x408580, (void *)SendInviteToPrivateChannel, F(SendInviteToPrivateChannel)},
		{0x408730, (void *)SendExcludeFromPrivateChannel, F(SendExcludeFromPrivateChannel)},
		{0x409b00, (void *)SendAddBuddy, F(SendAddBuddy)},
		{0x406250, DEFINED_HERE(SendEditText), F(SendEditText)},
		{0x406410, DEFINED_HERE(SendEditHouseText), F(SendEditHouseText)},
		{0x4083e0, DEFINED_HERE(SendOpenPrivateChannel), F(SendOpenPrivateChannel)},
		{0x409830, DEFINED_HERE(SendOutfit), F(SendOutfit)},
		{0x409f40, DEFINED_HERE(SendBugReport), F(SendBugReport)},
		{0x40a0f0, DEFINED_HERE(SendRuleViolationReport), F(SendRuleViolationReport)},
		{0x40bac0, DEFINED_HERE(CancelPatch), F(CancelPatch)},
	// The CRT
#if GOLDEN_ORACLE
		{0x511d6b, NULL, (void *)ImageSprintf},
		{0x51233d, NULL, (void *)ImageSnprintf},
		{0x5126a0, NULL, (void *)ImageQsort},
		{0x51262a, NULL, (void *)ImageAtol},
		{0x512539, NULL, (void *)ImageStricmp},
#endif
		{0x5237e7, (void *)(void *(__cdecl *)(size_t)) operator new, (void *)PatternNew},
		{0x523817, (void *)(void *(__cdecl *)(size_t)) operator new[], (void *)PatternNew},
	// The blocks: the image's malloc and free (its operator delete frees with
	// free), our operator delete and delete[].
#if GOLDEN_ORACLE
		{0x512d18, NULL, (void *)TrackedMalloc},
		{0x51165d, NULL, (void *)TrackedFree},
#endif
		{0, (void *)(void(__cdecl *)(void *)) operator delete, (void *)TrackedFree},
		{0, (void *)(void(__cdecl *)(void *)) operator delete[], (void *)TrackedFree},
#if !GOLDEN_ORACLE
		{0, (void *)(void(__cdecl *)(void *, size_t)) operator delete, (void *)TrackedSizedFree},
		{0, (void *)(void(__cdecl *)(void *, size_t)) operator delete[], (void *)TrackedSizedFree},
#endif
	};
	for (auto &T : Table) {
		if (T.Image != 0)
			ORIGINAL(Redirect((void *)(uintptr_t)T.Image, T.Fake));
		if (T.Ours != NULL)
			Redirect(T.Ours, T.Fake);
	}
}
#undef F

// --- The dialogs ------------------------------------------------------------------------------

enum {
	K_MESSAGE,
	K_FORMATTED,
	K_CONNECTING,
	K_WAIT,
	K_RECONNECT,
	K_QUESTION,
	K_LOGOUT,
	K_INFO,
	K_MOVE,
	K_LOGIN,
	K_CHARACTERS,
	K_TEXT,
	K_EDITLIST,
	K_OUTFIT,
	K_BUGREPORT,
	K_NEWCHANNEL,
	K_INVITE,
	K_EXCLUDE,
	K_IGNORE,
	K_REPORT,
	K_ADDBUDDY,
	K_EDITBUDDY,
	K_UPDATE,
	K_HELP,
	K_HINTS,
	K_RULEVIOLATION,
	K_EDITMARK,
	K_OPTIONS,
	K_GENERAL,
	K_GRAPHICS,
	K_ADVANCED,
	K_CONSOLE,
	K_NETWORK,
	K_HOTKEYS,
	K_ERRORREPORT,
	KINDS
};
static const char *const KindNames[KINDS] = {"message", "formatted message", "connecting", "wait", "reconnect",
	"question", "logout", "info", "move objects", "login", "character list", "text", "edit list", "outfit",
	"bug report", "new channel", "invite", "exclude", "ignore list", "report", "add VIP", "edit VIP", "update", "help",
	"hints", "rule violation", "edit mark", "options", "general options", "graphics options", "advanced graphics",
	"console options", "network options", "hotkeys", "error report"};
// The events each body handles (its own and those of its controls).
static const std::vector<int> Events[KINDS] = {
	{}, {}, {29, 31, 5}, {29, 31, 5}, {29, 31, 5}, {}, {}, {}, {1, 2}, {1, 2}, {1, 2, 31, 5}, {1, 2}, {1, 2},
	{1000, 1001, 1002, 1003, 2000, 2001, 2050, 2132, 6, 1, 2}, {1, 2}, {1, 301, 302, 2}, {1, 2}, {1, 2},
	{602, 709, 710, 712, 713, 11, 2}, {1, 2}, {1, 2}, {1, 2}, {2, 1}, {1, 2, 702, 703, 704, 705, 706, 707}, {1, 2},
	{1, 2, 21}, {1, 2}, {1, 2, 400, 401, 402, 403, 404, 406}, {1, 2, 701}, {1, 2, 407, 701}, {1, 2, 30, 701},
	{1, 2, 701}, {1, 2, 701}, {1, 2, 11}, {1, 2}};

static int Kind;
static TDialog *Frame[2];
static TWindow *ParentWindow[2];

// The arguments of a round (shared read-only data; what the dialogs delete is made per side).
static char Caption[64], MessageText[200];
static int OutfitData[7], TextObject[3], HouseData[2], ChannelIDs[6], Channels, Timeout, YesEvent, NoEvent;
static char TextBuffer[300], AuthorBuffer[40], FileName[40];
static char ChannelNames[6][30];
static TTalkLine *Statement[2]; // raw memory (TTalkLine has no default constructor)
static bool HasStatement, HasName, HasMark;
static TCoordinate MarkCoordinate;
static TMark MarkData;
static int CharacterID, DataSize;

static const char *const MessagePool[] = {"Hello", "A message\nof two lines", "OK",
	"<font=2>formatted <red=200>text<br=5>break", "Sorry, not possible.", "averylongwordwithoutanyspacesatallinthisline"};

static void RandomWorld()
{
	World.ActionState = 0;
	World.ActionNumber = Random() % 100;
	World.MessageType = 0;
	World.ConnectionState = Random() % 10;
	World.Progress = Random() % 3 == 0 ? -1 : Random() % 101;
	World.Characters = 1 + Random() % 6;
	for (int i = 0; i < 8; i++) {
		strcpy(World.CharacterNames[i], CharacterNamePool[Random() % 6]);
		strcpy(World.CharacterWorlds[i], WorldPool[Random() % 4]);
	}
	static const int Premium[] = {0, 0xffff, 1, 2, 13, 14, 300};
	World.PremiumDays = Premium[Random() % 7];
	World.ActiveCharacter = Random() % 8;
	World.WindowWidth = 400 + Random() % 400;
	World.WindowHeight = 300 + Random() % 300;
	World.DragObject.Type = Random() % 5 == 0 ? 0 : 100 + Random() % 3000;
	World.DragObject.Data = Random() % 5 == 0 ? Random() % 200 : 1 + Random() % 100;
	World.DragObject.Data2 = Random() % 10;
	World.DragIndex = Random() % 10;
	World.DragX = 32000 + Random() % 100;
	World.DragY = 32000 + Random() % 100;
	World.DragZ = Random() % 16;
	World.TargetX = 32000 + Random() % 100;
	World.TargetY = 32000 + Random() % 100;
	World.TargetZ = Random() % 16;
	World.RightSeed = Random();
	World.FlagSeed = Random();
	World.IgnoreSeed = Random();
	World.ErrorReports = Random() % 4;
	World.ClipboardEmpty = Random() % 4 == 0;
	static const char *const Clips[] = {"pasted", "two\nlines", "a much longer pasted text for the field", NULL};
	World.Clipboard = Clips[Random() % 4];
	TPreferences &P = World.Pref[OURS];
	P.ViewMode = 1 + Random() % 2;
	P.CreatureInfo = Random() % 3;
	P.ConfineFramerate = Random() % 4 == 0 ? Random() % 10 : 10 + Random() % 192;
	P.ClassicControl = Random() % 2 != 0;
	P.AutoChaseOff = Random() % 2 != 0;
	P.ShowNewHints = Random() % 2 != 0;
	P.TextualEffects = Random() % 2 != 0;
	P.LightEffects = Random() % 2 != 0;
	P.NoStretching = Random() % 2 != 0;
	P.WindowsMousepointer = Random() % 2 != 0;
	P.InfoMessages = Random() % 2 != 0;
	P.EventMessages = Random() % 2 != 0;
	P.StatusMessages = Random() % 2 != 0;
	P.TimeStamps = Random() % 2 != 0;
	P.PrivateMessages = Random() % 2 != 0;
	P.TestServerLogin = Random() % 2 != 0;
	World.Modes = Random() % 6;
	static const int Sizes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 1024}};
	for (int i = 0; i < 6; i++) {
		World.ModeList[i].Width = Sizes[Random() % 4][0];
		World.ModeList[i].Height = Sizes[Random() % 4][1];
		World.ModeList[i].Depth = Random() % 2 ? 16 : 32;
		World.ModeList[i].Frequency = Random() % 3 == 0 ? 0 : 60 + Random() % 30;
	}
	P.FullscreenMode = World.Modes > 0 && Random() % 2 ? World.ModeList[Random() % World.Modes] : World.ModeList[5];
	for (int i = 0; i < HOTKEYS; i++) {
		static const char *const Hotkeys[] = {"", "exura", "utevo lux", "hi", "a longer hotkey text of the player"};
		strcpy(P.Hotkeys[i], Hotkeys[Random() % 5]);
		P.AutoSend[i] = P.Hotkeys[i][0] != 0 && Random() % 2 != 0;
	}
	World.Pref[ORIGINAL] = P;
	for (int i = 0; i < 16; i++)
		World.KnownHints[i] = Random() % 2 != 0;
	for (int l = 0; l < 2; l++) {
		char *Text = l == 0 ? World.IgnoreText : World.BuddyIgnoreText;
		Text[0] = 0;
		int NumberOfNames = Random() % 6;
		for (int i = 0; i < NumberOfNames; i++) {
			strcat(Text, NamePool[Random() % 10]);
			strcat(Text, "\n");
		}
	}
	World.IgnoreListEnabled = Random() % 2 != 0;
	World.IgnoreEnabled = Random() % 2 != 0;
	World.BuddyIgnore = Random() % 2 != 0;
	for (int i = 0; i < 8; i++)
		World.ModeIgnored[i] = Random() % 2 != 0;
	World.RequestOpen = Random() % 5 == 0;
	World.PlayerID = 0x40000000 + Random() % 1000;
	strcpy(Player.Name, NamePool[Random() % 10]);
	World.BuddyPosition = Random() % 10;
	World.BuddyIcon = Random() % 12;
	World.BuddyNotify = Random() % 2 != 0;
	World.BuddyDescription = Random() % 2 != 0;
	World.Engines = Random() % 8;
	World.Engine = Random() % 3;
	World.FPSHundredths = Random() % 10000;
}

static void RandomArguments()
{
	{
		// In the order the x86 oracle evaluated sprintf's arguments, right to left (#55).
		int Number = Random() % 100;
		const char *Word = Random() % 2 ? "Caption" : "Dialog";
		sprintf(Caption, "%s %d", Word, Number);
	}
	strcpy(MessageText, MessagePool[Random() % 6]);
	if (Kind == K_FORMATTED && MessageText[0] == 0)
		strcpy(MessageText, "x");
	Timeout = Random() % 3 == 0 ? 0 : 1 + Random() % 200;
	if (Kind == K_RECONNECT && Timeout == 0)
		Timeout = 1; // the reconnect countdown always has a time
	YesEvent = 30 + Random() % 5;
	NoEvent = 2 + Random() % 3;
	OutfitData[5] = 128 + Random() % 5;
	OutfitData[6] = OutfitData[5] + Random() % 12;
	OutfitData[0] = 120 + Random() % 30;
	for (int i = 1; i <= 4; i++)
		OutfitData[i] = Random() % 133;
	TextObject[0] = Random();
	TextObject[1] = 1000 + Random() % 200;
	TextObject[2] = 50 + Random() % 400;
	static const char *const BookTexts[] = {"", "A short text.", "Line one\nLine two\nLine three",
		"a longer text that goes on and on and on, words and words and more words to break the lines"};
	strcpy(TextBuffer, BookTexts[Random() % 4]);
	strcpy(AuthorBuffer, Random() % 2 ? NamePool[Random() % 10] : "");
	HouseData[0] = Random() % 3;
	HouseData[1] = Random();
	Channels = 1 + Random() % 6;
	for (int i = 0; i < 6; i++) {
		ChannelIDs[i] = Random() % 4 == 0 ? 0xffff : Random() % 20;
		sprintf(ChannelNames[i], "Channel %d", Random() % 100);
	}
	static const char *const Files[] = {"tibia.pic", "TIBIA.DAT", "tibia.spr", "update.exe"};
	strcpy(FileName, Files[Random() % 4]);
	HasName = Random() % 2 != 0;
	HasStatement = Random() % 2 != 0;
	static const char *const Statements[] = {"you noob", "buy gold at", "hello there friend", ""};
	const char *StatementText = Statements[Random() % 4];
	uint32 StatementID = Random() % 3 == 0 ? 0 : Random();
	const char *Author = NamePool[Random() % 10];
	for (int s = 0; s < 2; s++) {
		if (Statement[s] == NULL)
			Statement[s] = (TTalkLine *)calloc(1, sizeof(TTalkLine));
		memset((void *)Statement[s], 0, sizeof(TTalkLine));
		Statement[s]->StatementID = StatementID;
		strcpy(Statement[s]->Addressee, Author);
		Statement[s]->Text = (char *)StatementText;
	}
	MarkCoordinate.x = 32000 + Random() % 200;
	MarkCoordinate.y = 32000 + Random() % 200;
	MarkCoordinate.z = Random() % 16;
	HasMark = Random() % 2 != 0;
	MarkData.x = Random() % 3 == 0 ? 0 : 32000 + Random() % 200;
	MarkData.y = Random() % 3 == 0 ? 0 : 32000 + Random() % 200;
	MarkData.Type = Random() % 14;
	strcpy(MarkData.Text, Random() % 2 ? "depot" : "");
	CharacterID = Random();
	DataSize = Random() % 100000;
	// The VIP dialog asserts a VIP; the move dialog only exists for a pile of 1-100.
	if (Kind == K_EDITBUDDY && World.BuddyPosition < 0)
		World.BuddyPosition = 0;
}

typedef void(__cdecl *URLFn)(const char *URL);
#define CREATE(Address, Function, ...) \
	(s == OURS ? Function(__VA_ARGS__) : At<decltype(&Function)>(Address)(__VA_ARGS__))
static TDialog *Create(int s)
{
	URLFn URL = FakeOpenURL;
	TComponent *R = Receiver[s];
	switch (Kind) {
	case K_MESSAGE: return CREATE(0x47f8a0, CreateMessageDialog, Caption, MessageText);
	case K_FORMATTED: return CREATE(0x47faf0, CreateFormattedMessageDialog, Caption, MessageText);
	case K_CONNECTING: return CREATE(0x47fd30, CreateConnectingDialog, Caption, MessageText, R);
	case K_WAIT: return CREATE(0x47ff60, CreateWaitDialog, Caption, MessageText, Timeout);
	case K_RECONNECT: return CREATE(0x4801c0, CreateReconnectDialog, Caption, MessageText, Timeout);
	case K_QUESTION: return CREATE(0x4803f0, CreateQuestionDialog, Caption, MessageText, R, YesEvent, NoEvent);
	case K_LOGOUT: return CREATE(0x480640, CreateLogoutDialog, Caption, MessageText, R);
	case K_INFO: return CREATE(0x4808b0, CreateInfoDialog, URL);
	case K_MOVE: return CREATE(0x480cd0, CreateMoveObjectsDialog);
	case K_LOGIN: return CREATE(0x480f40, CreateLoginDialog, URL);
	case K_CHARACTERS: return CREATE(0x481160, CreateCharacterListDialog, Timeout);
	case K_TEXT: return CREATE(0x481370, CreateTextDialog, TextObject, AuthorBuffer, TextBuffer);
	case K_EDITLIST: return CREATE(0x4817c0, CreateEditListDialog, HouseData, TextBuffer);
	case K_OUTFIT: return CREATE(0x4819f0, CreateOutfitDialog, OutfitData);
	case K_BUGREPORT: return CREATE(0x481bf0, CreateBugReportDialog);
	case K_NEWCHANNEL:
		return CREATE(0x481e00, CreateNewChannelDialog, Channels, ChannelIDs, (const char (*)[30])ChannelNames);
	case K_INVITE: return CREATE(0x482010, CreateInviteDialog);
	case K_EXCLUDE: return CREATE(0x482210, CreateExcludeDialog);
	case K_IGNORE: return CREATE(0x482410, CreateIgnoreListDialog);
	case K_REPORT: return CREATE(0x482640, CreateReportDialog);
	case K_ADDBUDDY: return CREATE(0x482870, CreateAddBuddyDialog);
	case K_EDITBUDDY: return CREATE(0x482a70, CreateEditBuddyDialog, CharacterID);
	case K_UPDATE: return CREATE(0x482c80, CreateUpdateDialog, FileName, DataSize);
	case K_HELP: return CREATE(0x482ea0, CreateHelpDialog, URL);
	case K_HINTS: return CREATE(0x4830c0, CreateHintsDialog);
	case K_RULEVIOLATION:
		return CREATE(0x485290, CreateRuleViolationDialog, HasName ? AuthorBuffer : NULL,
			HasStatement ? Statement[s] : NULL);
	case K_EDITMARK: {
		TCoordinate *C = (TCoordinate *)PatternNew(sizeof(TCoordinate));
		*C = MarkCoordinate;
		TMark *M = NULL;
		if (HasMark) {
			M = (TMark *)PatternNew(sizeof(TMark));
			*M = MarkData;
		}
		return CREATE(0x4832e0, CreateEditMarkDialog, C, M);
	}
	case K_OPTIONS: return CREATE(0x483500, CreateOptionsDialog);
	case K_GENERAL: return CREATE(0x483730, CreateGeneralOptionsDialog, URL);
	case K_GRAPHICS: return CREATE(0x483950, CreateGraphicsOptionsDialog, URL, FakeSetDisplayMode, ParentWindow[s]);
	case K_ADVANCED: return CREATE(0x483b80, CreateAdvancedGraphicsDialog, URL, R);
	case K_CONSOLE: return CREATE(0x483dc0, CreateConsoleOptionsDialog, URL);
	case K_NETWORK: return CREATE(0x483fe0, CreateNetworkOptionsDialog, URL);
	case K_HOTKEYS: return CREATE(0x484200, CreateHotkeyDialog, URL);
	case K_ERRORREPORT: return CREATE(0x484400, CreateErrorReportDialog);
	}
	return NULL;
}
#undef CREATE

// --- Dumps ------------------------------------------------------------------------------------

// The classes: the original's by vtable (the image's), ours by type.
#if GOLDEN_ORACLE
static const struct {
	uint32 Vtable;
	const char *Name;
} ImageClasses[] = {
	{0x560bb8, "TLabel"},
	{0x560c40, "TFormattedText"},
	{0x560ce8, "TLink"},
	{0x560d88, "TObjectIcon"},
	{0x560e10, "TMultiStateButton"},
	{0x560e98, "TEditLine"},
	{0x560f60, "TPasswordEditLine"},
	{0x561028, "TProgressBar"},
	{0x5610b0, "TSeparator"},
	{0x561138, "TListBox"},
	{0x5611e0, "TFontListBox"},
	{0x5612b0, "TResizer"},
	{0x561358, "TBorder"},
	{0x5614f8, "TSkinIcon"},
	{0x5617e0, "THorizontalScrollBarRange"},
	{0x561868, "TScrollBarRange"},
	{0x561970, "TOutfitIcon"},
	{0x5619f8, "TSkinButton"},
	{0x561a80, "TColorButton"},
	{0x561b08, "TIconButton"},
	{0x561b90, "TBlinkingButton"},
	{0x561c40, "TTextButton"},
	{0x561cd0, "TToggleButton"},
	{0x561d58, "TTextToggleButton"},
	{0x561de8, "TCheckBox"},
	{0x561ed0, "THorizontalScrollBar"},
	{0x561f70, "TScrollBar"},
	{0x562040, "TEditText"},
	{0x564528, "TWindow"},
	{0x562110, "TQuestionLabel"},
	{0x562198, "TLogoutLabel"},
	{0x562220, "TWaitLabel"},
	{0x5622b0, "TReconnectWindow"},
	{0x562370, "TCharacterListDialog"},
	{0x562418, "TNewChannelDialog"},
	{0x5624b8, "TGraphicsOptionsDialog"},
	{0x562558, "TEditBuddyDialog"},
	{0x5625f8, "TRuleViolationDialog"},
	{0x562698, "TEditMarkDialog"},
	{0x562738, "TAdvancedGraphicsDialog"},
	{0x5627d8, "TLoginDialog"},
	{0x562878, "TOutfitDialog"},
	{0x562b68, "TMoveObjectsDialog"},
	{0x562c28, "TTextDialog"},
	{0x562cd8, "TBugReportDialog"},
	{0x562db8, "TPrivateChannelDialog"},
	{0x562e58, "TReportDialog"},
	{0x562ef8, "TAddBuddyDialog"},
	{0x562ff0, "TUpdateDialog"},
	{0x5630c0, "THelpDialog"},
	{0x563160, "THintsDialog"},
	{0x563258, "TOptionsDialog"},
	{0x563360, "TGeneralOptionsDialog"},
	{0x563598, "TConsoleOptionsDialog"},
	{0x563650, "TNetworkOptionsDialog"},
	{0x563778, "TDialog"},
	{0x563868, "TEditListDialog"},
	{0x5639d8, "TIgnoreListDialog"},
	{0x563a90, "THotkeyDialog"},
	{0x563b30, "TErrorReportDialog"},
};
static std::map<uint32, std::string> ImageClass;
#endif
static void InitClasses()
{
#if GOLDEN_ORACLE
	for (auto &C : ImageClasses)
		ImageClass[C.Vtable] = C.Name;
#endif
}
static std::string ClassOf([[maybe_unused]] int s, TComponent *C)
{
#if GOLDEN_ORACLE
	if (s == ORIGINAL) {
		auto I = ImageClass.find((uint32)(uintptr_t)*(void **)C);
		return I == ImageClass.end() ? "?" : I->second;
	}
#endif
	std::string N = typeid(*C).name();
	return N.compare(0, 6, "class ") == 0 ? N.substr(6) : N;
}

static bool IsWindowClass(int s, TComponent *C)
{
	if (s == OURS)
		return dynamic_cast<TWindow *>(C) != NULL;
	std::string K = ClassOf(s, C);
	static const char *const Elements[] = {"TLabel", "TObjectIcon", "TMultiStateButton", "TProgressBar", "TSeparator",
		"TResizer", "TSkinIcon", "THorizontalScrollBarRange", "TScrollBarRange", "TOutfitIcon", "TSkinButton",
		"TColorButton", "TIconButton", "TBlinkingButton", "TTextButton", "TToggleButton", "TTextToggleButton",
		"TQuestionLabel", "TLogoutLabel", "TWaitLabel"};
	for (auto E : Elements)
		if (K == E)
			return false;
	return true;
}

static void NameTree(int s, TComponent *C, const std::string &Path)
{
	Names[s][C] = Path;
	if (!IsWindowClass(s, C))
		return;
	int i = 0;
	for (TComponent *Child = ((TWindow *)C)->FirstChild; Child != NULL; Child = Child->Next, i++) {
		char Buffer[16];
		snprintf(Buffer, sizeof(Buffer), ".%d", i);
		NameTree(s, Child, Path + Buffer);
	}
}

// The members, by their types (the same text from the original's x86 objects and from ours, x86 in the
// oracle and x64 in the runner).
static void Number(std::string &Out, int Value) { Add(Out, " %d", Value); }
// A bool or a byte as its byte (a member nobody set shows the fill pattern).
template <class T> static void Byte(std::string &Out, const T &Value)
{
	static_assert(sizeof(T) == 1, "a byte");
	uint8 Byte;
	memcpy(&Byte, &Value, 1);
	Add(Out, " %x", Byte);
}
static void Pointer(std::string &Out, int s, const void *Value, int Text = 0)
{
	Out += ' ';
	Out += Describe(s, (uintptr_t)Value, Text);
}
static void String(std::string &Out, int s, const char *Value) { Pointer(Out, s, Value, 1); }
// A text buffer in the object: its text, then the bytes after the text's end, in runs.
static void Chars(std::string &Out, const char *Buffer, size_t Size)
{
	Out += " '";
	size_t i = 0;
	for (; i < Size && Buffer[i] != 0; i++) {
		unsigned char c = (unsigned char)Buffer[i];
		if (c >= 32 && c < 0x7f && c != '\'' && c != '\\')
			Out += (char)c;
		else
			Add(Out, "\\%02x", c);
	}
	Out += "'";
	for (i++; i < Size;) {
		size_t Run = 1;
		while (i + Run < Size && Buffer[i + Run] == Buffer[i])
			Run++;
		Add(Out, Run > 1 ? "%02x*%d" : "%02x", (unsigned char)Buffer[i], (int)Run);
		i += Run;
	}
}
static void Rect(std::string &Out, const TRect &R) { Add(Out, " %d,%d,%d,%d", R.x, R.y, R.width, R.height); }
static void Color(std::string &Out, const TRGBColor &C) { Add(Out, " %d,%d,%d", C.R, C.G, C.B); }
static void Font(std::string &Out, const TFont &F)
{
	Add(Out, " font %d,%d,%d,%d,%d", F.FontNumber, F.Color.R, F.Color.G, F.Color.B, F.Alignment);
}
static void Callback(std::string &Out, int s, const TCallback &C)
{
	Out += " cb";
	Pointer(Out, s, C.Receiver);
	Add(Out, "/%d", C.Event);
}
static void Object(std::string &Out, const TObject &O) { Add(Out, " obj %d,%d,%d", O.Type, O.Data, O.Data2); }
// A vector's fields, then the entries from First to Last (those set).
template <class T, class F> static void Vector(std::string &Out, int s, vector<T> &V, int First, int Last, F Entry)
{
	Add(Out, " vec %d,%d,%d,%d,%d", V.min, V.max, V.start, V.space, V.block);
	Byte(Out, V.initialized);
	Pointer(Out, s, V.entry);
	if (V.entry == NULL)
		return;
	for (int i = First; i <= Last && i - First < 1000; i++) {
		if (i < V.min || i > V.max)
			Out += " -";
		else
			Entry(V.entry[i - V.start]);
	}
}
static void Radio(std::string &Out, int s, TRadioButtonGroup *G)
{
	Out += " radio";
	Pointer(Out, s, G);
	if (G == NULL)
		return;
	Pointer(Out, s, G->Selected);
	Number(Out, G->Count);
	Vector(Out, s, G->Buttons, 0, G->Count - 1, [&](TSkinButton *B) { Pointer(Out, s, B); });
}
static void Focus(std::string &Out, int s, TFocusGroup *G)
{
	Out += " focus";
	Pointer(Out, s, G);
	if (G == NULL)
		return;
	Pointer(Out, s, G->Active);
	Number(Out, G->Members);
	Vector(Out, s, G->MemberList, 0, G->Members - 1, [&](TEditLine *E) { Pointer(Out, s, E); });
}
template <class T, size_t N> static void Pointers(std::string &Out, int s, T *const (&Array)[N])
{
	for (size_t i = 0; i < N; i++)
		Pointer(Out, s, Array[i]);
}
template <size_t N> static void Numbers(std::string &Out, const int (&Array)[N])
{
	for (size_t i = 0; i < N; i++)
		Number(Out, Array[i]);
}
// A block of the round as an array (the members that point to one): Count entries, when it is one.
static bool IsBlock(const void *Value)
{
	const TPlace P = Locate((uintptr_t)Value);
	return P.Kind == BLOCK || P.Kind == STATIC;
}

static bool In(const std::string &K, std::initializer_list<const char *> List)
{
	for (const char *Name : List)
		if (K == Name)
			return true;
	return false;
}

// The members of a component of class K (not its geometry: Dump has that).
static void DumpMembers(std::string &Out, int s, TComponent *C, const std::string &K)
{
	// DialogElements.cpp
	if (K == "TSeparator") {
		Byte(Out, ((TSeparator *)C)->Horizontal);
	} else if (K == "TResizer") {
		TResizer *R = (TResizer *)C;
		Pointer(Out, s, R->ToResize);
		Byte(Out, R->Dragging);
		Add(Out, " %d,%d", R->StartX, R->StartY);
		Byte(Out, R->ResizeWidth);
		Byte(Out, R->ResizeHeight);
		Byte(Out, R->Invisible);
	} else if (K == "TBorder") {
		TBorder *B = (TBorder *)C;
		Pointer(Out, s, B->Window);
		Add(Out, " skins %d,%d,%d,%d,%d,%d,%d,%d,%d", B->TopLeft, B->Top, B->TopRight, B->Left, B->Right, B->BottomLeft,
			B->Bottom, B->BottomRight, B->Background);
		Rect(Out, B->WindowArea);
		Number(Out, B->Border);
	} else if (In(K, {"TLabel", "TQuestionLabel", "TLogoutLabel", "TWaitLabel"})) {
		TLabel *L = (TLabel *)C;
		String(Out, s, L->Text);
		String(Out, s, L->FullText);
		Font(Out, L->Font);
		Number(Out, L->Lines);
		Pointer(Out, s, L->Line);
		if (L->Line != NULL && IsBlock(L->Line) && L->Text != NULL)
			for (int i = 0; i < L->Lines && i < 100; i++) {
				Add(Out, " [%d]", (int)(L->Line[i] - L->Text));
				String(Out, s, L->Line[i]);
			}
		Byte(Out, L->Shortened);
		if (K == "TQuestionLabel") {
			Callback(Out, s, ((TQuestionLabel *)C)->YesCallback);
			Callback(Out, s, ((TQuestionLabel *)C)->NoAnswerCallback);
		} else if (K == "TLogoutLabel") {
			Callback(Out, s, ((TLogoutLabel *)C)->LogoutCallback);
			Callback(Out, s, ((TLogoutLabel *)C)->ExitCallback);
		} else if (K == "TWaitLabel") {
			TWaitLabel *W = (TWaitLabel *)C;
			Pointer(Out, s, W->Timer);
			Byte(Out, W->CloseOnMessage);
			Byte(Out, W->ClearOnAbort);
		}
	} else if (K == "TFormattedText") {
		TFormattedText *F = (TFormattedText *)C;
		String(Out, s, F->Text);
		Pointer(Out, s, F->Line);
		Byte(Out, F->AutoWidth);
		Byte(Out, F->AutoHeight);
	} else if (K == "TLink") {
		TLink *L = (TLink *)C;
		Pointer(Out, s, (const void *)L->OpenURL);
		String(Out, s, L->Destination);
		Pointer(Out, s, L->Button);
		Pointer(Out, s, L->Label);
	} else if (K == "TOutfitIcon") {
		TOutfitIcon *O = (TOutfitIcon *)C;
		Number(Out, O->Outfit);
		Numbers(Out, O->Colors);
		Number(Out, O->Direction);
		Color(Out, O->Highlight);
	} else if (K == "TSkinIcon") {
		Number(Out, ((TSkinIcon *)C)->Skin);
	} else if (K == "TObjectIcon") {
		TObjectIcon *O = (TObjectIcon *)C;
		Rect(Out, O->Unused24);
		Byte(Out, O->Framed);
		Color(Out, O->Highlight);
		Object(Out, O->Obj);
	} else if (In(K, {"TSkinButton", "TColorButton", "TIconButton", "TBlinkingButton", "TTextButton", "TToggleButton", "TTextToggleButton", "TMultiStateButton"})) {
		TSkinButton *B = (TSkinButton *)C;
		Add(Out, " skins %d,%d", B->ButtonUp, B->ButtonDown);
		Byte(Out, B->Pressed);
		Byte(Out, B->MouseInside);
		Byte(Out, B->Repeat);
		Callback(Out, s, B->Callback);
		Pointer(Out, s, B->RadioButtonGroup);
		String(Out, s, B->Tooltip);
		Byte(Out, B->Enabled);
		if (K == "TColorButton") {
			Color(Out, ((TColorButton *)C)->Color);
		} else if (K == "TIconButton") {
			Number(Out, ((TIconButton *)C)->Icon);
		} else if (K == "TBlinkingButton") {
			TBlinkingButton *L = (TBlinkingButton *)C;
			Add(Out, " %d,%d,%d,%d,%d,%d", L->NextChange, L->Phase, L->ButtonUp1, L->ButtonUp2, L->ButtonDown1,
				L->ButtonDown2);
		} else if (K == "TTextButton" || K == "TTextToggleButton") {
			Chars(Out, ((TTextButton *)C)->Text, sizeof(((TTextButton *)C)->Text));
			if (K == "TTextToggleButton") {
				TTextToggleButton *T = (TTextToggleButton *)C;
				Byte(Out, T->State);
				String(Out, s, T->Tooltip1);
				String(Out, s, T->Tooltip2);
			}
		} else if (K == "TToggleButton") {
			TToggleButton *T = (TToggleButton *)C;
			Byte(Out, T->State);
			String(Out, s, T->Tooltip1);
			String(Out, s, T->Tooltip2);
		} else if (K == "TMultiStateButton") {
			TMultiStateButton *M = (TMultiStateButton *)C;
			Add(Out, " state %d of %d", M->State, M->States);
			Pointer(Out, s, M->ButtonUps);
			Pointer(Out, s, M->ButtonDowns);
			Pointer(Out, s, M->Callbacks);
			if (IsBlock(M->ButtonUps) && IsBlock(M->ButtonDowns) && IsBlock(M->Callbacks))
				for (int i = 0; i < M->States && i < 100; i++) {
					Add(Out, " %d,%d", M->ButtonUps[i], M->ButtonDowns[i]);
					Callback(Out, s, M->Callbacks[i]);
				}
		}
	} else if (K == "TCheckBox") {
		TCheckBox *B = (TCheckBox *)C;
		Pointer(Out, s, B->Label);
		Byte(Out, B->Checked);
		Byte(Out, B->Enabled);
	} else if (In(K, {"TEditLine", "TPasswordEditLine", "TEditText"})) {
		TEditLine *E = (TEditLine *)C;
		String(Out, s, E->Text);
		Add(Out, " %d,%d", E->MaxLength, E->Cursor);
		Byte(Out, E->SingleLine);
		Pointer(Out, s, E->FocusGroup);
		Font(Out, E->Font);
		Number(Out, E->FirstVisible);
		Byte(Out, E->Active);
		Add(Out, " %d,%d", E->SelectionStart, E->SelectionLength);
		Byte(Out, E->Selecting);
		Byte(Out, E->Disabled);
		Byte(Out, E->ReadOnly);
		if (K == "TPasswordEditLine") {
			String(Out, s, ((TPasswordEditLine *)C)->Stars);
		} else if (K == "TEditText") {
			TEditText *T = (TEditText *)C;
			Vector(Out, s, T->LineStart, 0, T->Lines, [&](int Start) { Number(Out, Start); });
			Pointer(Out, s, T->ScrollBar);
			Add(Out, " %d,%d,%d,%d", T->Lines, T->FieldWidth, T->FieldHeight, T->VisibleLines);
		}
	} else if (K == "THorizontalScrollBarRange") {
		THorizontalScrollBarRange *R = (THorizontalScrollBarRange *)C;
		Add(Out, " %d,%d,%d,%d,%d", R->ButtonRange, R->Min, R->Max, R->Value, R->Visible);
		Byte(Out, R->Dragging);
		Byte(Out, R->Pressed);
		Add(Out, " %d,%d,%d", R->DragOffset, R->SliderWidth, R->SliderPosition);
		Byte(Out, R->PageScroll);
		Byte(Out, R->Enabled);
		Callback(Out, s, R->Callback);
	} else if (K == "TScrollBarRange") {
		TScrollBarRange *R = (TScrollBarRange *)C;
		Add(Out, " %d,%d,%d,%d,%d", R->ButtonRange, R->Min, R->Max, R->Value, R->Visible);
		Byte(Out, R->Pressed);
		Byte(Out, R->Dragging);
		Add(Out, " %d,%d,%d", R->DragOffset, R->SliderHeight, R->SliderPosition);
		Byte(Out, R->PageScroll);
		Byte(Out, R->Enabled);
	} else if (K == "THorizontalScrollBar") {
		THorizontalScrollBar *B = (THorizontalScrollBar *)C;
		Pointer(Out, s, B->Range);
		Pointer(Out, s, B->LeftButton);
		Pointer(Out, s, B->RightButton);
	} else if (K == "TScrollBar") {
		TScrollBar *B = (TScrollBar *)C;
		Pointer(Out, s, B->Range);
		Pointer(Out, s, B->UpButton);
		Pointer(Out, s, B->DownButton);
	} else if (K == "TListBox" || K == "TFontListBox") {
		TListBox *L = (TListBox *)C;
		Add(Out, " %d,%d", L->Lines, L->TextWidth);
		Vector(Out, s, L->LineText, 0, L->Lines - 1, [&](char *Text) { String(Out, s, Text); });
		Number(Out, L->Selected);
		Callback(Out, s, L->SelectCallback);
		Callback(Out, s, L->ActionCallback);
		Pointer(Out, s, L->ScrollBar);
		Number(Out, L->VisibleLines);
		Byte(Out, L->Editable);
		Byte(Out, L->Editing);
		Pointer(Out, s, L->EditLine);
		Number(Out, L->EditScroll);
		if (K == "TFontListBox")
			Vector(Out, s, ((TFontListBox *)C)->FontLines, 0, L->Lines - 1, [&](TListBoxLine &Line) {
				String(Out, s, Line.Text);
				Font(Out, Line.Font);
			});
	} else if (K == "TProgressBar") {
		TProgressBar *P = (TProgressBar *)C;
		Add(Out, " %d,%d", P->Maximum, P->Current);
	}
	// Dialogs.cpp
	else if (K == "TDialog") {
		TDialog *D = (TDialog *)C;
		Pointer(Out, s, D->Dialog);
		Pointer(Out, s, D->OkButton);
		Pointer(Out, s, D->CancelButton);
		Pointer(Out, s, D->ExtraButton);
		Pointer(Out, s, D->CaptionLabel);
		Pointer(Out, s, D->Separator);
		Byte(Out, D->Dragging);
		Add(Out, " %d,%d", D->DragX, D->DragY);
		Chars(Out, D->Caption, sizeof(D->Caption));
		Callback(Out, s, D->OkCallback);
		Callback(Out, s, D->CancelCallback);
	} else if (K == "TReconnectWindow") {
		TReconnectWindow *R = (TReconnectWindow *)C;
		Number(Out, R->Timeout);
		Chars(Out, R->Text, sizeof(R->Text));
		Pointer(Out, s, R->Timer);
		Pointer(Out, s, R->ProgressBar);
		Pointer(Out, s, R->TimeLabel);
	} else if (K == "TLoginDialog") {
		TLoginDialog *L = (TLoginDialog *)C;
		Pointer(Out, s, L->Account);
		Pointer(Out, s, L->Password);
		Focus(Out, s, L->FocusGroup);
	} else if (K == "TCharacterListDialog") {
		TCharacterListDialog *L = (TCharacterListDialog *)C;
		Pointer(Out, s, L->ListBox);
		Pointer(Out, s, L->Timer);
		Number(Out, L->Seconds);
	} else if (K == "TMoveObjectsDialog") {
		TMoveObjectsDialog *M = (TMoveObjectsDialog *)C;
		Object(Out, M->DraggedObject);
		Add(Out, " %d %d,%d,%d %d,%d,%d", M->SourceIndex, M->SourceX, M->SourceY, M->SourceZ, M->DestinationX,
			M->DestinationY, M->DestinationZ);
		Pointer(Out, s, M->ScrollBar);
		Add(Out, " %d,%d", M->Unused58, M->LastDigit);
	} else if (K == "TOutfitDialog") {
		TOutfitDialog *O = (TOutfitDialog *)C;
		Pointer(Out, s, O->OutfitIcon);
		Pointers(Out, s, O->PartButtons);
		Radio(Out, s, O->PartGroup);
		Pointers(Out, s, O->ColorButtons);
		Radio(Out, s, O->ColorGroup);
		Numbers(Out, O->Colors);
		Add(Out, " %d,%d,%d,%d", O->Outfit, O->FirstOutfit, O->LastOutfit, O->Part);
	} else if (K == "TTextDialog") {
		TTextDialog *T = (TTextDialog *)C;
		Add(Out, " %u", T->ObjectNumber);
		Object(Out, T->Obj);
		Pointer(Out, s, T->ObjectIcon);
		Pointer(Out, s, T->EditText);
	} else if (K == "TEditListDialog") {
		TEditListDialog *E = (TEditListDialog *)C;
		Add(Out, " %d,%u", E->Type, E->Identifier);
		Pointer(Out, s, E->EditText);
	} else if (K == "TIgnoreListDialog") {
		TIgnoreListDialog *I = (TIgnoreListDialog *)C;
		Pointer(Out, s, I->IgnoreListCheckBox);
		Pointer(Out, s, I->IgnoreList);
		Pointer(Out, s, I->AddIgnoreButton);
		Pointer(Out, s, I->DeleteIgnoreButton);
		Pointer(Out, s, I->IgnorePermanently);
		Pointer(Out, s, I->IgnoreYelling);
		Pointer(Out, s, I->IgnorePrivate);
		Pointer(Out, s, I->WhiteListCheckBox);
		Pointer(Out, s, I->WhiteList);
		Pointer(Out, s, I->AddWhiteButton);
		Pointer(Out, s, I->DeleteWhiteButton);
		Pointer(Out, s, I->AllowPermanently);
		Pointer(Out, s, I->AllowBuddies);
		for (vector<TIgnoreEntry> *V : {&I->IgnoreEntries, &I->WhiteEntries})
			Vector(Out, s, *V, V->min, V->max, [&](TIgnoreEntry &E) {
				Chars(Out, E.Name, sizeof(E.Name));
				Byte(Out, E.Permanent);
			});
	} else if (K == "TBugReportDialog") {
		Pointer(Out, s, ((TBugReportDialog *)C)->EditText);
	} else if (K == "TNewChannelDialog") {
		TNewChannelDialog *N = (TNewChannelDialog *)C;
		Pointer(Out, s, N->ListBox);
		Pointer(Out, s, N->NameEdit);
		Add(Out, " %d,%d,%d", N->Unused34, N->Unused38, N->NumberOfChannels);
		Pointer(Out, s, N->ChannelIDs);
		if (IsBlock(N->ChannelIDs))
			for (int i = 0; i < N->NumberOfChannels && i < 100; i++)
				Number(Out, N->ChannelIDs[i]);
	} else if (K == "TPrivateChannelDialog") {
		TPrivateChannelDialog *P = (TPrivateChannelDialog *)C;
		Byte(Out, P->Exclude);
		Pointer(Out, s, P->NameEdit);
	} else if (K == "TReportDialog") {
		Pointer(Out, s, ((TReportDialog *)C)->TextEdit);
	} else if (K == "TAddBuddyDialog") {
		Pointer(Out, s, ((TAddBuddyDialog *)C)->NameEdit);
	} else if (K == "TEditBuddyDialog") {
		TEditBuddyDialog *E = (TEditBuddyDialog *)C;
		Number(Out, E->CharacterID);
		Radio(Out, s, E->IconGroup);
		Pointer(Out, s, E->Notify);
		Pointer(Out, s, E->Description);
		Pointers(Out, s, E->IconButtons);
	} else if (K == "TUpdateDialog") {
		TUpdateDialog *U = (TUpdateDialog *)C;
		Number(Out, U->Unused2C);
		Pointer(Out, s, U->ProgressBar);
		Add(Out, " %d,%d,%d", U->StartTime, U->DataSize, U->Progress);
		Byte(Out, U->Done);
	} else if (K == "THelpDialog") {
		Pointer(Out, s, (const void *)((THelpDialog *)C)->OpenURL);
	} else if (K == "THintsDialog") {
		Pointer(Out, s, ((THintsDialog *)C)->ListBox);
	} else if (K == "TRuleViolationDialog") {
		TRuleViolationDialog *R = (TRuleViolationDialog *)C;
		Focus(Out, s, R->FocusGroup);
		Pointer(Out, s, R->NameEdit);
		Pointer(Out, s, R->ReasonList);
		Pointer(Out, s, R->ActionList);
		Pointer(Out, s, R->CommentEdit);
		Pointer(Out, s, R->StatementEdit);
		Pointer(Out, s, R->IPBanishment);
		Add(Out, " %u", R->StatementID);
		Numbers(Out, R->Reasons);
	} else if (K == "TEditMarkDialog") {
		TEditMarkDialog *E = (TEditMarkDialog *)C;
		Add(Out, " %d,%d,%d mark %d,%d,%d", E->Coordinate.x, E->Coordinate.y, E->Coordinate.z, E->Mark.x, E->Mark.y,
			E->Mark.Type);
		Chars(Out, E->Mark.Text, sizeof(E->Mark.Text));
		Pointer(Out, s, E->TextEdit);
		Radio(Out, s, E->TypeGroup);
		Pointers(Out, s, E->TypeButtons);
	} else if (K == "TGeneralOptionsDialog") {
		TGeneralOptionsDialog *G = (TGeneralOptionsDialog *)C;
		Pointer(Out, s, G->TextualEffects);
		Pointer(Out, s, G->CreatureNames);
		Pointer(Out, s, G->ShowHints);
		Pointer(Out, s, G->ClassicControl);
		Pointer(Out, s, G->AutoChaseOff);
		Pointer(Out, s, (const void *)G->OpenURL);
	} else if (K == "TGraphicsOptionsDialog") {
		TGraphicsOptionsDialog *G = (TGraphicsOptionsDialog *)C;
		Pointer(Out, s, G->Fullscreen);
		Pointer(Out, s, G->ResolutionList);
		Number(Out, G->NumberOfModes);
		Pointer(Out, s, G->Modes);
		if (IsBlock(G->Modes))
			for (int i = 0; i < G->NumberOfModes && i < 100; i++)
				Add(Out, " %dx%dx%d@%d", G->Modes[i].Width, G->Modes[i].Height, G->Modes[i].Depth, G->Modes[i].Frequency);
		Pointer(Out, s, (const void *)G->SetDisplayMode);
		Pointer(Out, s, G->Parent);
		Pointer(Out, s, G->AdvancedButton);
		Pointer(Out, s, (const void *)G->OpenURL);
	} else if (K == "TAdvancedGraphicsDialog") {
		// (The oracle's: the runner skips the rounds that open it, golden::RetiredBegin.)
		TAdvancedGraphicsDialog *A = (TAdvancedGraphicsDialog *)C;
		Pointer(Out, s, A->Receiver);
		Pointer(Out, s, A->LightEffects);
		Pointer(Out, s, A->NoStretching);
		Pointer(Out, s, A->WindowsMousepointer);
		Pointer(Out, s, A->Framerate);
		Pointer(Out, s, (const void *)A->OpenURL);
		Pointer(Out, s, A->DirectX5Button);
		Pointer(Out, s, A->DirectX9Button);
		Pointer(Out, s, A->OpenGLButton);
		Radio(Out, s, A->EngineGroup);
	} else if (K == "TConsoleOptionsDialog") {
		TConsoleOptionsDialog *O = (TConsoleOptionsDialog *)C;
		Pointer(Out, s, O->InfoMessages);
		Pointer(Out, s, O->EventMessages);
		Pointer(Out, s, O->StatusMessages);
		Pointer(Out, s, O->TimeStamps);
		Pointer(Out, s, O->PrivateMessages);
		Pointer(Out, s, (const void *)O->OpenURL);
	} else if (K == "TNetworkOptionsDialog") {
		TNetworkOptionsDialog *N = (TNetworkOptionsDialog *)C;
		Pointer(Out, s, N->TestServerLogin);
		Pointer(Out, s, (const void *)N->OpenURL);
	} else if (K == "THotkeyDialog") {
		THotkeyDialog *H = (THotkeyDialog *)C;
		for (int i = 0; i < HOTKEYS; i++)
			Chars(Out, H->Texts[i], sizeof(H->Texts[i]));
		for (int i = 0; i < HOTKEYS; i++)
			Byte(Out, H->AutoSend[i]);
		Pointer(Out, s, H->ListBox);
		Pointer(Out, s, H->TextEdit);
		Pointer(Out, s, H->AutoSendCheckBox);
		Number(Out, H->Selected);
		Pointer(Out, s, (const void *)H->OpenURL);
	} else if (K == "TErrorReportDialog") {
		Pointer(Out, s, ((TErrorReportDialog *)C)->CommentEdit);
	}
	// (TWindow, TOptionsDialog: no members of their own.)
}

static void Dump(std::string &Out, int s, TComponent *C, int Depth)
{
	std::string K = ClassOf(s, C);
	Add(Out, "\n%*s%s %s at %d,%d size %d,%d", Depth * 2, "", Names[s][C].c_str(), K.c_str(), C->OffsetX, C->OffsetY,
		C->Width, C->Height);
	Add(Out, " draw %d:", C->DrawingListLength);
	for (int i = 0; i < C->DrawingListLength; i++) {
		TDrawing &D = C->DrawingList[i];
		Add(Out, " %d,%d,%d,%d,%d,%d,%d", D.skin, D.x, D.y, D.width, D.height, D.srcx, D.srcy);
	}
	Out += " |";
	Pointer(Out, s, C->Parent);
	Pointer(Out, s, C->Next);
	const bool Window = IsWindowClass(s, C);
	if (Window) {
		Pointer(Out, s, ((TWindow *)C)->FirstChild);
		Pointer(Out, s, ((TWindow *)C)->Capture);
	}
	DumpMembers(Out, s, C, K);
	if (Window) {
		for (TComponent *Child = ((TWindow *)C)->FirstChild; Child != NULL; Child = Child->Next)
			Dump(Out, s, Child, Depth + 1);
	}
}

static void RefreshNames(int s)
{
	Untracked++;
	Names[s].clear();
	Names[s][Receiver[s]] = "Recv";
	Names[s][ParentWindow[s]] = "Parent";
	if (Statement[s] != NULL)
		Names[s][Statement[s]] = "Statement";
	if (Frame[s] != NULL)
		NameTree(s, Frame[s], "F");
	Untracked--;
}
static std::string DumpAll(int s)
{
	RefreshNames(s);
	std::string Out;
	if (Frame[s] == NULL)
		return "no dialog";
	Dump(Out, s, Frame[s], 0);
	return Out;
}

static int OperationCount[32];
static const char *OperationName;

// The error round: the action whose calls fail (CurrentAction: the operation,
// 16 the creator, 17 the destructor, 18 a key press straight to the body),
// the most fake calls and the last count
// one call of each action made for each kind of dialog, whether a call threw
// (the dialogs are then abandoned: members may point to deleted parts, so
// only the logs are compared).
enum { ACTION_CREATOR = 16,
	ACTION_DESTRUCTOR = 17,
	ACTION_BODYKEY = 18,
	ACTIONS = 19 };
static TErrorRound *DialogsRound;
static int CurrentAction, TargetAction = -1, MaxTicks[KINDS][ACTIONS], LastTicks[KINDS][ACTIONS];
static bool Threw;

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
	std::string Text = DumpAll(ORIGINAL);
#else
	std::string Text;
#endif
	return golden::OutputTextChanges("dump", &LastDump, Text);
}

// Whether the original has a frame (after a call that may have made or deleted it): the oracle's, recorded;
// in the runner a placeholder that is never read, or NULL.
static char OriginalPlaceholder;
static void OriginalFrame()
{
	const bool Has = ORIGINAL_VALUE("frame", bool, Frame[ORIGINAL] != NULL);
	ORIGINAL_OR((void)Has, Frame[ORIGINAL] = Has ? (TDialog *)(void *)&OriginalPlaceholder : NULL);
}

static void Compare()
{
	// The table of blocks does not change between the two dumps.
	Untracked++;
	std::string D[2];
	if (!Threw) {
		D[OURS] = DumpAll(OURS);
		D[ORIGINAL] = OriginalDump();
	}
	Untracked--;
	ApplyPendingFrees();
	OutputOriginalLog();
	if (LogFile != NULL)
		fprintf(LogFile, "== %s %s\n%s\n%s\n", KindNames[Kind], OperationName, Log[OURS].c_str(), D[OURS].c_str());
	CHECK(Log[OURS] == Log[ORIGINAL], "Dialogs %s (%s): logs differ\n  ours: %s\n  orig: %s", OperationName,
		KindNames[Kind], Log[OURS].c_str(), Log[ORIGINAL].c_str());
	CHECK(D[OURS] == D[ORIGINAL], "Dialogs %s (%s): dialogs differ\n  ours: %s\n  orig: %s", OperationName,
		KindNames[Kind], D[OURS].c_str(), D[ORIGINAL].c_str());
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

// Runs F for our side and then for the image's (the names of the trees are
// refreshed first, for the logs).
static bool Trace;
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
// In the error round the calls of the round's action fail at one of the fake
// calls such a call made before (for the same kind of dialog), and what both
// sides threw is compared; after a throw nothing runs.
template <class Fn> static void Both(Fn Function)
{
	if (DialogsRound != NULL) {
		if (Threw)
			return;
		int Count = Random() % 2 == 0 ? MaxTicks[Kind][CurrentAction] : LastTicks[Kind][CurrentAction];
		int Fail = Count > 0 && CurrentAction == TargetAction ? FailIndex(Count + 1) : 0;
		char What[96];
		snprintf(What, sizeof(What), "%s %s (fail at %d)", KindNames[Kind], OperationName, Fail);
		DialogsRound->Calling(What, "ours");
		TThrown A = OurThrow([&] {
			Side = OURS;
			RefreshNames(OURS);
			Ticks = 0;
			FailAt = Fail;
			Function(OURS);
		});
		DialogsRound->Calling(What, "the original");
		TThrown B = ORIGINAL_THROWN("thrown", Side = ORIGINAL; RefreshNames(ORIGINAL); Ticks = 0; FailAt = Fail;
			Function(ORIGINAL));
		Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls (it ran last)
		OriginalFrame();
		FailAt = 0;
		Side = OURS;
		DialogsRound->Compare(What, A, B);
		if (A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING) {
			Threw = true;
		} else {
			LastTicks[Kind][CurrentAction] = Ticks;
			if (Ticks > MaxTicks[Kind][CurrentAction])
				MaxTicks[Kind][CurrentAction] = Ticks;
		}
		return;
	}
	for (int s = 0; s < SIDES; s++) {
		Side = s;
		if (Trace)
			fprintf(stderr, "    %s side %d\n", OperationName, s);
		RefreshNames(s);
		Ticks = 0;
		if (s == OURS) {
			try {
				Function(s);
			} catch (TMyException &E) {
				printf("Dialogs: our side threw: %s (%s)\n", E.GetMessage().c_str(), E.GetOrigin().c_str());
				fflush(stdout);
				abort();
			}
		} else {
			Function(s);
		}
	}
	// (the counts of the error round's failures)
	Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls (it ran last)
	OriginalFrame();
	if (Ticks > MaxTicks[Kind][CurrentAction])
		MaxTicks[Kind][CurrentAction] = Ticks;
	Side = OURS;
}

// --- Operations ---------------------------------------------------------------------------

static int Key()
{
	static const int Keys[] = {13, 27, 13, 27, 'y', 'Y', 'n', 'N', 'l', 'L', 'e', 'E', 'c', 'C', 'a', 'Z', '0', '5', '9',
		'1', '2', ' ', '.', 8, 9, 0x209, 0x10c, 0x10d, 0x10e, 0x10f, 0x110, 0x111, 0x112, 0x113, 0x114, 0x115, 0x30d,
		0x310, 0x476, 0x463, 0x441, 'q', 'w'};
	return Keys[Random() % (sizeof(Keys) / sizeof(Keys[0]))];
}

// A point on one of our side's components, relative to the frame.
static void ChildPoint(int *x, int *y)
{
	std::vector<std::pair<TComponent *, std::pair<int, int>>> All;
	std::vector<std::pair<TComponent *, std::pair<int, int>>> Work = {{Frame[OURS], {0, 0}}};
	while (!Work.empty()) {
		auto W = Work.back();
		Work.pop_back();
		All.push_back(W);
		if (dynamic_cast<TWindow *>(W.first) != NULL)
			for (TComponent *C = ((TWindow *)W.first)->FirstChild; C != NULL; C = C->Next)
				Work.push_back({C, {W.second.first + C->OffsetX, W.second.second + C->OffsetY}});
	}
	auto &P = All[Random() % All.size()];
	int w = P.first->Width > 0 ? P.first->Width : 1, h = P.first->Height > 0 ? P.first->Height : 1;
	*x = P.second.first + (Random() % 2 ? w / 2 : Random() % w);
	*y = P.second.second + (Random() % 2 ? h / 2 : Random() % h);
}

static TWindow *Body(int s) { return (TWindow *)Frame[s]->Dialog; }

static void Members(uint32 r)
{
	int Choice = r % 8;
	if (Choice < 3) {
		// The frame's buttons: a new text, or removal of an existing one.
		static const char *const ButtonTexts[] = {"Ok", "Cancel", "Again", "Close"};
		const char *Text = ButtonTexts[(r >> 4) % 4];
		bool Remove = (r >> 8) % 4 == 0;
		TTextButton *Existing = Choice == 0 ? Frame[OURS]->OkButton : Choice == 1 ? Frame[OURS]->CancelButton
																				  : Frame[OURS]->ExtraButton;
		if (Remove && Existing == NULL)
			Remove = false;
		int Event = 1 + (r >> 12) % 3;
		bool ToReceiver = (r >> 16) % 2 != 0;
		static const uint32 Image[] = {0x46c3e0, 0x46c710, 0x46ca40};
		OperationName = Choice == 0 ? "SetOkButton" : Choice == 1 ? "SetCancelButton"
																  : "SetExtraButton";
		Both([&](int s) {
			TCallback C = ToReceiver ? TCallback(Receiver[s], Event) : TCallback(NULL, 0);
			if (s == OURS) {
				if (Choice == 0)
					Frame[s]->SetOkButton(Remove ? NULL : Text, C);
				else if (Choice == 1)
					Frame[s]->SetCancelButton(Remove ? NULL : Text, C);
				else
					Frame[s]->SetExtraButton(Remove ? NULL : Text, C);
			} else {
				CallI<void>(Image[Choice], Frame[s], Remove ? (const char *)NULL : Text, C);
			}
		});
		return;
	}
	OperationName = "members";
	switch (Kind) {
	case K_RECONNECT: {
		int Time = (int)((r >> 4) % 2000);
		OperationName = "SetTime";
		Both([&](int s) {
			if (s == OURS)
				((TReconnectWindow *)Body(s))->SetTime(Time);
			else
				CallI<void>(0x46ba10, Body(s), Time);
		});
		break;
	}
	case K_CHARACTERS: {
		int Seconds = 1 + (int)((r >> 4) % 100);
		OperationName = "SetTimeout";
		Both([&](int s) { ((TCharacterListDialog *)Body(s))->SetTimeout(Seconds); });
		break;
	}
	case K_CONNECTING:
	case K_WAIT: {
		int Time = 1 + (int)((r >> 4) % 100);
		OperationName = "SetTimer";
		Both([&](int s) { ((TWaitLabel *)Frame[s]->Dialog)->SetTimer(Time, TCallback(Frame[s]->Dialog, 31)); });
		break;
	}
	case K_RULEVIOLATION:
		if ((r >> 4) % 2 != 0) {
			// Select a reason and an action and fill the texts (the list boxes'
			// and edit lines' own members), then send OK.
			TRuleViolationDialog *D = (TRuleViolationDialog *)Body(OURS);
			int Reason = D->ReasonList->Lines > 0 ? (int)((r >> 8) % D->ReasonList->Lines) : -1;
			uint32 r2 = Random();
			static const char *const Comments[] = {"", "he insulted me", "spam"};
			const char *Comment = Comments[r2 % 3], *Name = (r2 >> 4) % 3 == 0 ? "" : "Offender";
			OperationName = "report";
			Both([&](int s) {
				TRuleViolationDialog *B = (TRuleViolationDialog *)Body(s);
				if (s == OURS)
					B->ReasonList->SetSelection(Reason);
				else
					CallI<void>(0x46ad10, B->ReasonList, Reason);
				B->HandleEvent(21);
			});
			int Action = D->ActionList->Lines > 0 ? (int)((r2 >> 8) % D->ActionList->Lines) : -1;
			Both([&](int s) {
				TRuleViolationDialog *B = (TRuleViolationDialog *)Body(s);
				if (s == OURS)
					B->ActionList->SetSelection(Action);
				else
					CallI<void>(0x46ad10, B->ActionList, Action);
				B->CommentEdit->SetText(Comment);
				if (!B->NameEdit->ReadOnly)
					B->NameEdit->SetText(Name);
				B->HandleEvent(1);
			});
			break;
		}
		OperationName = "UpdateActions";
		Both([&](int s) {
			if (s == OURS) {
				TRuleViolationDialog *D = (TRuleViolationDialog *)Body(s);
				D->UpdateActions();
				// (two calls as on the image's side: the error round counts them)
				Logf("Reason%d,", D->GetReason());
				Logf("Action%d;", D->GetAction());
			} else {
				CallI<void>(0x47eae0, Body(s));
				Logf("Reason%d,", CallI<int>(0x4776b0, Body(s)));
				Logf("Action%d;", CallI<int>(0x477860, Body(s)));
			}
		});
		break;
	case K_HOTKEYS: {
		if ((r >> 12) % 2 != 0) {
			// A selection by the list's member, then its event.
			int Line = (int)((r >> 4) % HOTKEYS);
			OperationName = "select";
			Both([&](int s) {
				if (s == OURS)
					((THotkeyDialog *)Body(s))->ListBox->SetSelection(Line);
				else
					CallI<void>(0x46ad10, ((THotkeyDialog *)Body(s))->ListBox, Line);
				Body(s)->HandleEvent(11);
			});
			break;
		}
		int Number = (int)((r >> 4) % HOTKEYS);
		OperationName = "UpdateLine";
		Both([&](int s) {
			if (s == OURS)
				((THotkeyDialog *)Body(s))->UpdateLine(Number);
			else
				CallI<void>(0x47b9c0, Body(s), Number);
		});
		break;
	}
	case K_NEWCHANNEL:
	case K_INVITE:
	case K_EXCLUDE:
	case K_ADDBUDDY: {
		// A name: the player's own (the new channel dialog refuses it), another or none.
		const char *Name = (r >> 4) % 3 == 0 ? Player.Name : (r >> 4) % 3 == 1 ? "Someone"
																			   : "";
		OperationName = "name";
		Both([&](int s) {
			TEditLine *E = Kind == K_NEWCHANNEL ? ((TNewChannelDialog *)Body(s))->NameEdit
				: Kind == K_ADDBUDDY            ? ((TAddBuddyDialog *)Body(s))->NameEdit
												: ((TPrivateChannelDialog *)Body(s))->NameEdit;
			E->SetText(Name);
			Body(s)->HandleEvent(1);
		});
		break;
	}
	case K_HINTS:
	case K_GRAPHICS: {
		// A selection by the list's member, then its event.
		TListBox *L = Kind == K_HOTKEYS ? ((THotkeyDialog *)Body(OURS))->ListBox
			: Kind == K_HINTS           ? ((THintsDialog *)Body(OURS))->ListBox
										: ((TGraphicsOptionsDialog *)Body(OURS))->ResolutionList;
		if (L->Lines == 0)
			break;
		int Line = (int)((r >> 4) % L->Lines);
		int Event = Kind == K_HOTKEYS ? 11 : 1;
		OperationName = "select";
		Both([&](int s) {
			TListBox *List = Kind == K_HOTKEYS ? ((THotkeyDialog *)Body(s))->ListBox
				: Kind == K_HINTS              ? ((THintsDialog *)Body(s))->ListBox
											   : ((TGraphicsOptionsDialog *)Body(s))->ResolutionList;
			if (s == OURS)
				List->SetSelection(Line);
			else
				CallI<void>(0x46ad10, List, Line);
			Body(s)->HandleEvent(Event);
		});
		break;
	}
	default:
		OperationName = "minimum size";
		Both([&](int s) { Logf("Min%d,%d;", Frame[s]->getMinimumWidth(), Frame[s]->getMinimumHeight()); });
		break;
	}
}

static int Clock; // the test clock of the round (the dialogs start at 1000)
// The clock at our side's last UpdateTimers call (GUI.cpp's function-local LastTime, which the next call
// measures the elapsed time from; -1: none yet in this test). RetiredEnd needs it.
static int OurTimersClock = -1;
// The engine the dialogs see (the advanced graphics dialog's selection): ours and the image's.
static void SetEngine(int Engine)
{
	curEngine = Engine;
	ORIGINAL(Global<int>(0x71d0c4) = Engine);
}
// Forced >= 0: that operation (the error round), with the same random numbers.
static void Operation(int Forced = -1)
{
	TDialog *F0 = Frame[OURS];
	int Op = Random() % 16;
	if (Forced >= 0)
		Op = Forced;
	int x, y, x2, y2;
	ChildPoint(&x, &y);
	ChildPoint(&x2, &y2);
	if (Random() % 4 == 0) {
		x = (int)(Random() % (F0->Width + 10)) - 5;
		y = (int)(Random() % (F0->Height + 10)) - 5;
	}
	uint32 r1 = Random(), r2 = Random(), r3 = Random();
	World.ActionNumber += 1 + Random() % 3;
	World.MessageType = Random() % 6 == 0 ? 1 + Random() % 60 : 0;
	World.Progress = Random() % 4 == 0 ? -1 : Random() % 101;
	World.Engine = Random() % 3;
	World.FPSHundredths = Random() % 10000;
	SetEngine(World.Engine);
	Clock += Kind == K_UPDATE ? Random() % 600 : Random() % 3000; // the update dialog closes 2 s after the end
	SetTestClock(Clock);
	// The hotkey list sends event 11 for a click beside its lines; without a
	// selection the dialog then reads before its texts (garbage of each side).
	if (Kind == K_HOTKEYS && Op >= 3 && Op <= 5 && ((THotkeyDialog *)Body(OURS))->ListBox->Selected < 0)
		Op = 1;
	OperationCount[Op]++;
	CurrentAction = Op;
	switch (Op) {
	case 0: {
		int SurfaceNumber = 1 + r1 % 3, dx = r2 % 50, dy = r3 % 50;
		OperationName = "Draw";
		Both([&](int s) { Frame[s]->Draw(SurfaceNumber, dx, dy); });
		break;
	}
	case 1:
	case 2: {
		int K = Key();
		OperationName = "KeyPress";
		Both([&](int s) { Frame[s]->KeyPress(K); });
		break;
	}
	case 3:
	case 4: {
		bool Drag = r1 % 3 == 0;
		int Flags = r3 % 5 == 0 ? 0x200 : 0;
		OperationName = "Click";
		Both([&](int s) {
			Logf("D%d;", Frame[s]->LeftButtonDown(x, y, Flags) ? 1 : 0);
			if (Drag)
				Logf("M%d;", Frame[s]->MouseMove(x2, y2) ? 1 : 0);
			Logf("U%d;", Frame[s]->LeftButtonUp(Drag ? x2 : x, Drag ? y2 : y, Flags) ? 1 : 0);
		});
		break;
	}
	case 5:
		OperationName = "DoubleClick";
		Both([&](int s) {
			Logf("D%d;", Frame[s]->LeftButtonDown(x, y, 0) ? 1 : 0);
			Logf("U%d;", Frame[s]->LeftButtonUp(x, y, 0) ? 1 : 0);
			Logf("DD%d;", Frame[s]->LeftButtonDoubleClick(x, y, 0) ? 1 : 0);
			Logf("U%d;", Frame[s]->LeftButtonUp(x, y, 0) ? 1 : 0);
		});
		break;
	case 6:
		OperationName = "RightButtonUp";
		Both([&](int s) { Logf("R%d;", Frame[s]->RightButtonUp(x, y, 0) ? 1 : 0); });
		break;
	case 7: {
		int Code = 1 + r1 % 2;
		OperationName = "MouseWheel";
		Both([&](int s) { Logf("W%d;", Frame[s]->MouseWheel(x, y, Code) ? 1 : 0); });
		break;
	}
	case 8: {
		// A drag of the caption line.
		int cx = (int)(r1 % (F0->Width > 0 ? F0->Width : 1)), cy = (int)(r2 % 16);
		int dx = (int)(r3 % 200) - 100, dy = (int)((r3 >> 8) % 200) - 100;
		OperationName = "DragCaption";
		Both([&](int s) {
			Logf("D%d;", Frame[s]->LeftButtonDown(cx, cy, 0) ? 1 : 0);
			Logf("M%d;", Frame[s]->MouseMove(cx + dx, cy + dy) ? 1 : 0);
			Logf("U%d;", Frame[s]->LeftButtonUp(cx + dx, cy + dy, 0) ? 1 : 0);
		});
		break;
	}
	case 9:
	case 10: {
		if (Events[Kind].empty())
			break;
		int Event = Events[Kind][r1 % Events[Kind].size()];
		if (Kind == K_HOTKEYS && Event == 11 && ((THotkeyDialog *)Body(OURS))->ListBox->GetSelection() < 0)
			break;
		// Event 31 comes from the timer only.
		if (Event == 31 && (Kind == K_CONNECTING || Kind == K_WAIT) && ((TWaitLabel *)Frame[OURS]->Dialog)->Timer == NULL)
			break;
		if (Event == 31 && Kind == K_CHARACTERS && ((TCharacterListDialog *)Body(OURS))->Timer == NULL)
			break;
		OperationName = "HandleEvent";
		Both([&](int s) { Frame[s]->Dialog->HandleEvent(Event); });
		break;
	}
	case 11: {
		int Event = 1 + r1 % 2;
		OperationName = "frame HandleEvent";
		Both([&](int s) { Frame[s]->HandleEvent(Event); });
		break;
	}
	case 12:
		OperationName = "Arrange";
		Both([&](int s) { Frame[s]->Arrange(); });
		break;
	case 13:
		OperationName = "MouseHover";
		Both([&](int s) {
			Frame[s]->MouseHover(x, y);
			Logf("H%d;", Frame[s]->MouseMove(x, y) ? 1 : 0);
		});
		break;
	case 14:
		OperationName = "UpdateTimers";
		Both([&](int s) {
			if (s == OURS) {
				OurTimersClock = Clock;
				UpdateTimers();
			} else
				At<void(__cdecl *)()>(0x4a27c0)();
		});
		break;
	case 15:
		Members(r1);
		break;
	}
	Compare();
}

// --- Retired rounds (issue #28) -------------------------------------------------------------

// The rounds that open the advanced graphics dialog retire with the oracle (#31: the x64 dialog has the
// device buttons). The runner skips each such part (golden::RetiredBegin) and takes from the corpus what
// the later rounds read of it: Random()'s seed after it, the operation counts (the summary line), the fake
// call counts (the error round's failing calls), the exceptions the error round compared in it (its
// summary), the original's last dump (the next dump is recorded by its changes against it) and the clock of
// our side's last UpdateTimers call (GUI.cpp's function-local LastTime: the elapsed time of the next call). The world, the arguments and the clock are new in each round; the table of blocks is empty
// between rounds.
static int RetiredCompared, RetiredThrew;
static bool RetiredBegin(const char *Name)
{
	RetiredCompared = ThrownCompared;
	RetiredThrew = ThrownThrew;
	return golden::RetiredBegin(Name, Checks);
}
static void RetiredEnd()
{
	golden::RetiredEnd(Checks);
	uint32 Seed = GetRandomSeed();
	golden::Output("seed", Seed);
	SetRandomSeed(Seed);
	golden::Output("operations", OperationCount);
	golden::Output("most ticks", MaxTicks);
	golden::Output("last ticks", LastTicks);
	int Exceptions[2] = {ThrownCompared - RetiredCompared, ThrownThrew - RetiredThrew};
	golden::Output("exceptions", Exceptions);
	ThrownCompared = RetiredCompared + Exceptions[0];
	ThrownThrew = RetiredThrew + Exceptions[1];
	// The original's last dump, which the next one is recorded against (OriginalDump).
	LastDump = golden::OutputText("last dump", LastDump.data(), LastDump.size());
	// Our UpdateTimers' LastTime (GUI.cpp, a function-local static): the clock of our last call in the part.
	// The runner's own call at that clock sets it the same (no timer is left between rounds, so the call
	// updates none).
	int TimersClock = OurTimersClock;
	golden::Output("timers clock", TimersClock);
	if (TimersClock != OurTimersClock) {
		OurTimersClock = TimersClock;
		SetTestClock(TimersClock);
		UpdateTimers();
		SetTestClock(Clock);
	}
}

// --- The test -----------------------------------------------------------------------------

static void CheckCompareNames()
{
	for (int i = 0; i < 200; i++) {
		const char *a = NamePool[Random() % 10], *b = NamePool[Random() % 10];
		int Ours = CompareNames(&a, &b);
		int Original = ORIGINAL_VALUE("CompareNames", int, At<int(__cdecl *)(const void *, const void *)>(0x46bb00)(&a, &b));
		CHECK((Ours < 0) == (Original < 0) && (Ours > 0) == (Original > 0), "CompareNames %s %s: %d, original %d", a, b,
			Ours, Original);
	}
}

// The error report body: our constructor on both sides, the second object with
// the image's vtable (0x563b30), so that its KeyPress and HandleEvent (and its
// destructor) are the image's code.
static void TestErrorReport()
{
	Kind = K_ERRORREPORT;
	for (int Round = 0; Round < 60; Round++) {
		RandomWorld();
		Testing = true;
		TErrorReportDialog *ReportBody[2] = {};
		for (int s = 0; s < SIDES; s++) {
			Side = s;
			ReportBody[s] = new TErrorReportDialog();
#if GOLDEN_ORACLE
			if (s == ORIGINAL)
				*(void **)ReportBody[s] = (void *)(uintptr_t)0x563b30;
#endif
		}
		Side = OURS;
		OperationName = "error report constructor";
		OutputOriginalLog();
		CHECK(Log[OURS] == Log[ORIGINAL], "Dialogs error report constructor: logs differ");
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		for (int i = 0; i < 30; i++) {
			bool IsKey = Random() % 3 != 0;
			int K = Key(), Event = 1 + Random() % 3;
			OperationName = IsKey ? "error report KeyPress" : "error report HandleEvent";
			for (int s = 0; s < SIDES; s++) {
				Side = s;
				Names[s].clear();
				if (IsKey)
					ReportBody[s]->KeyPress(K);
				else
					ReportBody[s]->HandleEvent(Event);
				Logf("Comment %s;", Str(ReportBody[s]->CommentEdit->Text).c_str());
			}
			Side = OURS;
			OutputOriginalLog();
			CHECK(Log[OURS] == Log[ORIGINAL], "Dialogs %s: logs differ\n  ours: %s\n  orig: %s", OperationName,
				Log[OURS].c_str(), Log[ORIGINAL].c_str());
			Log[OURS].clear();
			Log[ORIGINAL].clear();
		}
		for (int s = 0; s < SIDES; s++) {
			Side = s;
			delete ReportBody[s];
		}
		Side = OURS;
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		Testing = false;
		ReleaseFreedBlocks();
	}
}

// One round of the dialog of the current kind: the creator, the operations, the destructor.
static void MainRound()
{
	RandomWorld();
	RandomArguments();
	SetEngine(World.Engine);
	Clock = 1000;
	SetTestClock(Clock);
	Testing = true;
	OperationName = "creator";
	CurrentAction = ACTION_CREATOR;
	Both([&](int s) { Frame[s] = Create(s); });
	Compare();
	if (Frame[OURS] != NULL && Frame[ORIGINAL] != NULL) {
		int Operations = 20 + Random() % 40;
		for (int i = 0; i < Operations; i++) {
			Operation();
			if (Trace)
				fprintf(stderr, "  %s\n", OperationName);
		}
		OperationName = "destructor";
		CurrentAction = ACTION_DESTRUCTOR;
		Both([&](int s) {
			delete Frame[s];
			Frame[s] = NULL;
		});
		OutputOriginalLog();
		CHECK(Log[OURS] == Log[ORIGINAL], "Dialogs destructor (%s): logs differ\n  ours: %s\n  orig: %s",
			KindNames[Kind], Log[OURS].c_str(), Log[ORIGINAL].c_str());
	}
	Frame[OURS] = Frame[ORIGINAL] = NULL;
	Log[OURS].clear();
	Log[ORIGINAL].clear();
	Testing = false;
	ReleaseFreedBlocks();
}

// --- The error round (issue #34) -------------------------------------------------------------

// Abandoned dialogs (a call threw) leave their timers in both timer lists:
// they are taken out (ours with RemoveTimer, the image's with 0x49b0d0),
// found by the timer vtables at the start of a live block.
static void RemoveAbandonedTimers()
{
	for (auto &B : *Blocks) {
		if (B.second.Freed || B.second.Size < sizeof(TTimer))
			continue;
		void *V = *(void **)B.first;
		if (V == OurTimerVtable)
			RemoveTimer((TTimer *)B.first);
#if GOLDEN_ORACLE
		else if ((uintptr_t)V == 0x5646e8)
			At<void(__cdecl *)(void *)>(0x49b0d0)((void *)B.first);
#endif
	}
}

// One call on both sides (failing at the Fail-th fake call, 0: none), what
// they threw compared, then the logs (and the dialogs unless a call threw).
template <class F> static void DialogCall(const char *What, F Call, int Fail = 0)
{
	OperationName = What;
	DialogsRound->Calling(What, "ours");
	TThrown A = OurThrow([&] {
		Side = OURS;
		RefreshNames(OURS);
		Ticks = 0;
		FailAt = Fail;
		Call(OURS);
	});
	DialogsRound->Calling(What, "the original");
	TThrown B = ORIGINAL_THROWN("thrown", Side = ORIGINAL; RefreshNames(ORIGINAL); Ticks = 0; FailAt = Fail;
		Call(ORIGINAL));
	Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls (it ran last)
	OriginalFrame();
	FailAt = 0;
	Side = OURS;
	DialogsRound->Compare(What, A, B);
	Threw = Threw || A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING;
	Compare();
}

// A new pair of dialogs of the kind (random world and arguments); false when
// the creator threw.
static bool NewDialogs()
{
	RandomWorld();
	RandomArguments();
	SetEngine(World.Engine);
	Clock = 1000;
	SetTestClock(Clock);
	Testing = true;
	Threw = false;
	Frame[OURS] = Frame[ORIGINAL] = NULL;
	OperationName = "creator";
	CurrentAction = ACTION_CREATOR;
	Both([&](int s) { Frame[s] = Create(s); });
	Compare();
	return !Threw && Frame[OURS] != NULL && Frame[ORIGINAL] != NULL;
}

// The end of a round: the dialogs deleted, or abandoned after a throw.
static void EndDialogs()
{
	if (!Threw && Frame[OURS] != NULL && Frame[ORIGINAL] != NULL) {
		OperationName = "destructor";
		CurrentAction = ACTION_DESTRUCTOR;
		Both([&](int s) {
			delete Frame[s];
			Frame[s] = NULL;
		});
		OutputOriginalLog();
		CHECK(Log[OURS] == Log[ORIGINAL], "Dialogs errors destructor (%s): logs differ\n  ours: %s\n  orig: %s",
			KindNames[Kind], Log[OURS].c_str(), Log[ORIGINAL].c_str());
	}
	Frame[OURS] = Frame[ORIGINAL] = NULL;
	RemoveAbandonedTimers();
	Log[OURS].clear();
	Log[ORIGINAL].clear();
	Testing = false;
	ReleaseFreedBlocks();
	Threw = false;
}

// A dialog of the kind for a direct call; false when the creator threw or
// made none.
static bool NewKind(int k)
{
	Kind = k;
	if (NewDialogs())
		return true;
	EndDialogs();
	return false;
}

// A call on new dialogs of the kind, first without a failure (its fake calls
// counted), then failing at each of its first fake calls in turn, each on new
// dialogs. Prepare (calls without failures) returns whether the call applies.
static int SweepCalls;
template <class P, class F> static void Sweep(int k, const char *What, P Prepare, F Call, int Limit = 40)
{
	int Count = 0;
	for (int Fail = 0; Fail <= Count && Fail <= Limit; Fail++) {
		if (NewKind(k)) {
			if (Prepare() && !Threw) {
				DialogCall(What, Call, Fail);
				if (Fail == 0) {
					Count = Threw ? 0 : Ticks;
					SweepCalls += Count;
					if (DialogsRound->Trace)
						printf("Dialogs errors: sweep %s: %d fake calls\n", What, Count);
				}
			}
		}
		EndDialogs();
	}
}

typedef TDialog *(__cdecl *CreateMessageFn)(const char *, const char *);
static char LongText[2100];

// The direct calls of one kind in the second loop of the assertions: the frame's buttons removed twice, an
// event the body does not know.
static void ButtonsAndEvents(int Round)
{
	int k = Round % KINDS;
	// The frame's buttons removed twice.
	if (NewKind(k)) {
		static const uint32 Image[] = {0x46c3e0, 0x46c710, 0x46ca40};
		int Which = Round / KINDS;
		for (int i = 0; i < 2 && !Threw; i++) {
			DialogCall("remove a button", [&](int s) {
				if (s == OURS) {
					if (Which == 0)
						Frame[s]->SetOkButton(NULL, TCallback());
					else if (Which == 1)
						Frame[s]->SetCancelButton(NULL, TCallback());
					else
						Frame[s]->SetExtraButton(NULL, TCallback());
				} else {
					CallI<void>(Image[Which], Frame[s], (const char *)NULL, TCallback());
				}
			});
		}
	}
	EndDialogs();
	// Events the body does not know.
	if (NewKind(k)) {
		int Event = Round < KINDS ? 9999 : Round < 2 * KINDS ? -1
															 : 3;
		DialogCall("body HandleEvent unknown", [&](int s) { Frame[s]->Dialog->HandleEvent(Event); });
	}
	EndDialogs();
}

// The assertions and TERRORs, with direct calls (each on a new dialog, or
// without one: a constructor or creator with arguments the game does not give).
static void TestDialogsAssertions()
{
	for (int Round = 0; Round < 3; Round++) {
		// TDialog's constructor: no caption, a caption of 100 characters, no body.
		memset(LongText, 'a', sizeof(LongText) - 1);
		LongText[100 + Round] = 0;
		Kind = K_MESSAGE;
		RandomWorld();
		Testing = true;
		DialogCall("CreateMessageDialog without a caption", [&](int s) { Frame[s] = s == OURS ? CreateMessageDialog(NULL, "x") : At<CreateMessageFn>(0x47f8a0)(NULL, "x"); });
		EndDialogs();
		Testing = true;
		DialogCall("CreateMessageDialog long caption", [&](int s) { Frame[s] = s == OURS ? CreateMessageDialog(LongText, "x") : At<CreateMessageFn>(0x47f8a0)(LongText, "x"); });
		EndDialogs();
		Testing = true;
		DialogCall("TDialog::TDialog without a body", [&](int s) {
			if (s == OURS)
				new TDialog("x", NULL);
			else
				CallI<void *>(0x47c970, PatternNew(sizeof(TDialog)), "x", (TComponent *)NULL); });
		EndDialogs();
		// The move dialog without an object or with a pile outside 1-100.
		Kind = K_MOVE;
		RandomWorld();
		World.DragObject.Type = Round == 0 ? 0 : 100;
		World.DragObject.Data = Round == 1 ? 0 : 101 + Round;
		Testing = true;
		DialogCall("TMoveObjectsDialog::TMoveObjectsDialog", [&](int s) {
			if (s == OURS)
				new TMoveObjectsDialog();
			else
				CallI<void *>(0x470670, PatternNew(sizeof(TMoveObjectsDialog))); });
		EndDialogs();
		// The outfit dialog without data, a text dialog without a text, an edit
		// list without a text or with a text too long.
		Kind = K_OUTFIT;
		RandomWorld();
		Testing = true;
		DialogCall("CreateOutfitDialog without data", [&](int s) { Frame[s] = s == OURS ? CreateOutfitDialog(NULL) : At<TDialog *(__cdecl *)(int *)>(0x4819f0)(NULL); });
		EndDialogs();
		Kind = K_TEXT;
		Testing = true;
		DialogCall("TTextDialog::TTextDialog without a text", [&](int s) {
			TObject Obj;
			Obj.Type = 1000 + Round;
			if (s == OURS)
				new TTextDialog(100, NULL, 7, Obj, "Description");
			else
				CallI<void *>(0x471500, PatternNew(sizeof(TTextDialog)), 100, (const char *)NULL, (uint32)7, Obj,
					"Description"); });
		EndDialogs();
		Kind = K_EDITLIST;
		Testing = true;
		memset(LongText, 'a', sizeof(LongText) - 1);
		LongText[1999 + Round] = 0;
		DialogCall("CreateEditListDialog without a text", [&](int s) { Frame[s] = s == OURS ? CreateEditListDialog(HouseData, NULL)
																							: At<TDialog *(__cdecl *)(int *, const char *)>(0x4817c0)(HouseData, NULL); });
		EndDialogs();
		Testing = true;
		DialogCall("CreateEditListDialog long text", [&](int s) { Frame[s] = s == OURS ? CreateEditListDialog(HouseData, LongText)
																					   : At<TDialog *(__cdecl *)(int *, const char *)>(0x4817c0)(HouseData, LongText); });
		EndDialogs();
		// The new channel dialog without channel IDs or with no channels.
		Kind = K_NEWCHANNEL;
		Testing = true;
		typedef TDialog *(__cdecl * NewChannelFn)(int, int *, const char (*)[30]);
		int Number = Round == 0 ? 3 : -(Round - 1);
		int *IDs = Round == 0 ? NULL : ChannelIDs;
		DialogCall("CreateNewChannelDialog", [&](int s) { Frame[s] = s == OURS ? CreateNewChannelDialog(Number, IDs, (const char (*)[30])ChannelNames)
																			   : At<NewChannelFn>(0x481e00)(Number, IDs, (const char (*)[30])ChannelNames); });
		EndDialogs();
		// The VIP dialog of a character that is no VIP; the mark dialog without a
		// coordinate.
		Kind = K_EDITBUDDY;
		RandomWorld();
		World.BuddyPosition = -1 - Round;
		Testing = true;
		DialogCall("CreateEditBuddyDialog no VIP", [&](int s) { Frame[s] = s == OURS ? CreateEditBuddyDialog(77) : At<TDialog *(__cdecl *)(int)>(0x482a70)(77); });
		EndDialogs();
		Kind = K_EDITMARK;
		Testing = true;
		DialogCall("CreateEditMarkDialog without a coordinate", [&](int s) { Frame[s] = s == OURS ? CreateEditMarkDialog(NULL, NULL)
																								  : At<TDialog *(__cdecl *)(TCoordinate *, TMark *)>(0x4832e0)(NULL, NULL); });
		EndDialogs();
		// The ignore lists with a last name that has no line end.
		Kind = K_IGNORE;
		RandomWorld();
		strcpy(Round == 1 ? World.BuddyIgnoreText : World.IgnoreText, Round == 2 ? "Alpha" : "Alpha\nbeta");
		Testing = true;
		DialogCall("CreateIgnoreListDialog", [&](int s) { Frame[s] = s == OURS ? CreateIgnoreListDialog() : At<TDialog *(__cdecl *)()>(0x482410)(); });
		EndDialogs();
	}
	for (int Round = 0; Round < 3 * KINDS; Round++) {
		// The advanced graphics dialog's calls retire with the oracle (#28/#31).
		const bool Retired = Round % KINDS == K_ADVANCED;
		if (!Retired || !RetiredBegin("advanced graphics: buttons and events"))
			ButtonsAndEvents(Round);
		if (Retired)
			RetiredEnd();
	}
	for (int Round = 0; Round < 6; Round++) {
		// The login dialog's keys without an active edit line.
		if (NewKind(K_LOGIN)) {
			for (int s = 0; s < SIDES; s++)
				((TLoginDialog *)Frame[s]->Dialog)->FocusGroup->Active = NULL;
			DialogCall("TLoginDialog::KeyPress without an edit line", [&](int s) { Frame[s]->Dialog->KeyPress('a' + Round); });
		}
		EndDialogs();
		// A private conversation without the player.
		if (NewKind(K_NEWCHANNEL)) {
			World.NoPlayer = true;
			DialogCall("TNewChannelDialog::HandleEvent without the player", [&](int s) {
				((TNewChannelDialog *)Frame[s]->Dialog)->NameEdit->SetText("Someone");
				Frame[s]->Dialog->HandleEvent(1); });
			World.NoPlayer = false;
		}
		EndDialogs();
		// A hint whose title is no help title.
		if (NewKind(K_HINTS) && ((THintsDialog *)Frame[OURS]->Dialog)->ListBox->Lines > 0) {
			int Line = Round % ((THintsDialog *)Frame[OURS]->Dialog)->ListBox->Lines;
			DialogCall("THintsDialog select", [&](int s) {
				if (s == OURS)
					((THintsDialog *)Frame[s]->Dialog)->ListBox->SetSelection(Line);
				else
					CallI<void>(0x46ad10, ((THintsDialog *)Frame[s]->Dialog)->ListBox, Line); });
			World.NoHelpTitles = true;
			DialogCall("THintsDialog::HandleEvent unknown title", [&](int s) { Frame[s]->Dialog->HandleEvent(1); });
			World.NoHelpTitles = false;
		}
		EndDialogs();
		// The action named by the selected line; a release with a selected hotkey;
		// the mouse on the character list.
		Sweep(K_RULEVIOLATION, "TRuleViolationDialog::GetAction", [&] {
			TListBox *R = ((TRuleViolationDialog *)Frame[OURS]->Dialog)->ReasonList;
			if (R->Lines == 0)
				return false;
			int Reason = Round % R->Lines;
			DialogCall("select reason", [&](int s) {
				TRuleViolationDialog *B = (TRuleViolationDialog *)Frame[s]->Dialog;
				if (s == OURS)
					B->ReasonList->SetSelection(Reason);
				else
					CallI<void>(0x46ad10, B->ReasonList, Reason);
				B->HandleEvent(21); });
			TListBox *L = ((TRuleViolationDialog *)Frame[OURS]->Dialog)->ActionList;
			if (Threw || L->Lines == 0)
				return false;
			int Line = Round % L->Lines;
			DialogCall("select action", [&](int s) {
				TListBox *List = ((TRuleViolationDialog *)Frame[s]->Dialog)->ActionList;
				if (s == OURS)
					List->SetSelection(Line);
				else
					CallI<void>(0x46ad10, List, Line); });
			return true; }, [&](int s) {
			if (s == OURS)
				Logf("Action%d;", ((TRuleViolationDialog *)Frame[s]->Dialog)->GetAction());
			else
				Logf("Action%d;", CallI<int>(0x477860, Frame[s]->Dialog)); });
		Sweep(K_HOTKEYS, "THotkeyDialog::LeftButtonUp", [&] {
			int Line = Round % HOTKEYS;
			DialogCall("select hotkey", [&](int s) {
				THotkeyDialog *D = (THotkeyDialog *)Frame[s]->Dialog;
				if (s == OURS)
					D->ListBox->SetSelection(Line);
				else
					CallI<void>(0x46ad10, D->ListBox, Line);
				D->HandleEvent(11); });
			return true; }, [&](int s) { Logf("U%d;", Frame[s]->Dialog->LeftButtonUp(3 + Round, 3, 0) ? 1 : 0); });
		for (int Mouse = 0; Mouse < 2; Mouse++) {
			Sweep(K_CHARACTERS, Mouse == 0 ? "TCharacterListDialog::RightButtonUp" : "TCharacterListDialog::MouseWheel", [&] { return true; }, [&](int s) {
				TComponent *B = Frame[s]->Dialog;
				int x = B->Width / 2, y = B->Height / 2;
				if (Mouse == 0)
					Logf("R%d;", B->RightButtonUp(x, y, 0) ? 1 : 0);
				else
					Logf("W%d;", B->MouseWheel(x, y, 1 + Round % 2) ? 1 : 0); });
		}
		// A hotkey line outside the table.
		if (NewKind(K_HOTKEYS)) {
			int Number = Round % 2 ? HOTKEYS + Round : -1 - Round;
			DialogCall("THotkeyDialog::UpdateLine outside", [&](int s) {
				if (s == OURS)
					((THotkeyDialog *)Frame[s]->Dialog)->UpdateLine(Number);
				else
					CallI<void>(0x47b9c0, Frame[s]->Dialog, Number); });
		}
		EndDialogs();
	}
}

// One round of the error round: the kind's dialogs with operations, the round's action failing.
static void ErrorRound()
{
	if (NewDialogs()) {
		int Operations = 5 + Random() % 30;
		for (int i = 0; i < Operations && !Threw; i++) {
			bool Own = Random() % 2 != 0;
			if (Own && TargetAction == ACTION_BODYKEY) {
				// The body's own keys (Return and Escape do not reach it through the frame).
				int K = Random() % 3 == 0 ? Key() : Random() % 2 ? 13
																 : 27;
				OperationName = "body KeyPress";
				CurrentAction = ACTION_BODYKEY;
				Both([&](int s) { Frame[s]->Dialog->KeyPress(K); });
				Compare();
			} else {
				Operation(Own && TargetAction < 16 ? TargetAction : -1);
			}
		}
	}
	EndDialogs();
}

// Every TASSERT/TERROR of Dialogs.cpp and the TEND blocks: the test's
// operations on new dialogs, the calls of one action per round failing at
// one of the fake calls it made before, until one throws; then the
// assertions with direct calls.
static void TestDialogsErrors()
{
	TErrorRound R("Dialogs");
	DialogsRound = &R;
	// Each kind with each action failing (half of the operations are the
	// action's own), ten times.
	for (int Round = 0; Round < 10 * KINDS * ACTIONS; Round++) {
		Kind = Round % KINDS;
		TargetAction = Round / KINDS % ACTIONS;
		// The advanced graphics dialog's rounds retire with the oracle (#28/#31).
		const bool Retired = Kind == K_ADVANCED;
		if (!Retired || !RetiredBegin("advanced graphics: error round"))
			ErrorRound();
		if (Retired)
			RetiredEnd();
	}
	TargetAction = -1;
	TestDialogsAssertions();
	if (getenv("EH_TRACE") != NULL)
		printf("Dialogs errors: sweeps over %d fake calls\n", SweepCalls);
	DialogsRound = NULL;
	R.Finish();
}

} // namespace DialogsTest

void TestDialogs()
{
	using namespace DialogsTest;
	int Before = Checks, BeforeFailures = Failures;
	// The checks counted as the oracle counts them: those the runner skipped as retired too.
	const long long RetiredBefore = golden::RetiredChecks();
	auto AllChecks = [&] { return Checks + (int)(golden::RetiredChecks() - RetiredBefore); };
	Trace = getenv("DG_TRACE") != NULL;
	if (Trace)
		setvbuf(stdout, NULL, _IONBF, 0);
	if (getenv("DG_LOGS") != NULL)
		LogFile = fopen(getenv("DG_LOGS"), "w");
	// DG_HEAPSHIFT=<bytes>: a block of that size, allocated first and kept, moves
	// the heap layout (the dump must not depend on it).
	if (getenv("DG_HEAPSHIFT") != NULL) {
		size_t Shift = strtoul(getenv("DG_HEAPSHIFT"), NULL, 0);
		if (Shift > 0) {
			void *Block = malloc(Shift);
			if (Block != NULL)
				memset(Block, 0, Shift);
		}
	}
	ORIGINAL(PatchImage());
	InitSkins();
	ORIGINAL(Global<int>(0x71c450) = NumberOfSkins; Global<TSkin *>(0x71c454) = Skins);
	InitSkinProperties();
	ORIGINAL(At<void(__cdecl *)()>(0x498160)());
	InitClasses();
	LastDump.clear();
#if GOLDEN_ORACLE
	// The image's static timer vector (initialiser 0x559e30, constructor 0x4973f0) and NoCallback (0x559e10).
	typedef void(__fastcall * VectorInitFn)(void *, void *, int, int, int, void *);
	if (Global<void *>(0x5ccfa4 + 0x1c) == NULL)
		At<VectorInitFn>(0x4973f0)((void *)(uintptr_t)0x5ccfa4, NULL, 0, 0, 2, NULL);
	At<void(__cdecl *)()>(0x559e10)();
#endif
	{
		TTimer Probe(1, TCallback(NULL, 0));
		OurTimerVtable = *(void **)&Probe;
	}
	Surface = new TDGSurface;
	BlockHeap = HeapCreate(0, 0, 0);
	Blocks = new TBlocks();
	PendingFrees = new std::vector<void *, TBlockAllocator<void *>>();
	InitRanges();
	PatchCallees();
	CheckCompareNames();
	for (int s = 0; s < SIDES; s++) {
		Receiver[s] = new TDGReceiver;
		ParentWindow[s] = new TWindow;
	}

	TestErrorReport();
	int KindCount[KINDS] = {};
	for (int Count = 0; Count < 1200; Count++) {
		// The image's error report constructor builds a std::stringstream: the
		// image's iostreams need the CRT's static initialisers (which reach
		// imports the child does not resolve). TestErrorReport covers the rest.
		Kind = Random() % (KINDS - 1);
		KindCount[Kind]++;
		if (Trace)
			fprintf(stderr, "round %d kind %s\n", Count, KindNames[Kind]);
		// The advanced graphics dialog's rounds retire with the oracle (#28/#31).
		const bool Retired = Kind == K_ADVANCED;
		if (!Retired || !RetiredBegin("advanced graphics: round"))
			MainRound();
		if (Retired)
			RetiredEnd();
	}

	// The error round (issue #34): its checks are reported apart.
	int RoundChecks = AllChecks(), SavedCounts[32];
	memcpy(SavedCounts, OperationCount, sizeof(SavedCounts));
	TestDialogsErrors();
	memcpy(OperationCount, SavedCounts, sizeof(SavedCounts));
	RoundChecks = AllChecks() - RoundChecks;

	for (int s = 0; s < SIDES; s++) {
		delete Receiver[s];
		delete ParentWindow[s];
	}
	RestoreAll();
	if (getenv("DG_BLOCKS") != NULL) {
		int Live = 0, Freed = 0;
		for (auto &B : *Blocks)
			(B.second.Freed ? Freed : Live)++;
		fprintf(stderr, "Dialogs: blocks left %d live, %d freed\n", Live, Freed);
	}
	if (LogFile != NULL)
		fclose(LogFile);
	int MinKind = 1 << 30, MinOperation = 1 << 30;
	for (int k = 0; k < KINDS - 1; k++)
		MinKind = KindCount[k] < MinKind ? KindCount[k] : MinKind;
	for (int i = 0; i < 16; i++)
		MinOperation = OperationCount[i] < MinOperation ? OperationCount[i] : MinOperation;
	printf("Dialogs: %d checks, %d failures (each dialog at least %d times, each operation at least %d times)\n",
		AllChecks() - Before - RoundChecks, Failures - BeforeFailures, MinKind, MinOperation);
	const int Retired = (int)(golden::RetiredChecks() - RetiredBefore);
	if (Retired != 0)
		printf("Dialogs: %d checks replayed, %d retired with the oracle (the advanced graphics dialog, #28/#31)\n",
			Checks - Before, Retired);
}
