// difftest.exe's Talk test (moved here from tools/difftest/talktest.cpp, issue #26): Talk.cpp and its
// Talk.h/Container.h COMDATs against the original, as a golden-corpus test (golden.h). Only mapped
// functions run: no imports, entry point or original client loader.
//
// Both sides' wall clock is fixed: the oracle patches the image's _time32 and its own static CRT's _time64
// (ours is time(), 64 bits, since issue #62); the runner's CRT is the DLL's (ucrtbase), so the runner points
// the import slot of _time64 at the same fake for the length of the test. Both sides use the process's
// localtime (the image's _localtime32, ours _localtime64_s: the same time zone at the recording and the
// replay, one machine).
//
// The original's objects are recorded by value. A talk buffer (TTalkBuffer, 0x2c bytes in the
// original, our oracle classes have its layout) is recorded as its fields, the indices of its cursor and
// last line, and its list of lines: per line an identity (the original's line, and its contents: a line
// is recorded in full the first time it is seen, and again if the same address holds another line
// later) and the index of its previous line. The talkboxes (char and int arrays only) are recorded by
// their changes. A pointer the original returns into its own objects is recorded as which of them it is.

#include "golden_test.h"
#include "golden_patch.h"

#include "Container.h"
#include "Preferences.h"
#include "Talk.h"
#include "Utils.h"
#include "graphics.h"

#include <map>
#include <new>
#include <stdarg.h>
#include <string>
#include <vector>

#if GOLDEN_ORACLE
typedef void(__cdecl *VoidFn)();
typedef bool(__cdecl *BoolFn)();
typedef int(__cdecl *IntFn)();
typedef void(__cdecl *SetBoolFn)(bool);
typedef void(__fastcall *ThisFn)(void *, void *);
typedef void(__fastcall *ThisIntFn)(void *, void *, int);
typedef void(__fastcall *ThisLineFn)(void *, void *, TTalkLine *);
typedef void(__fastcall *ThisTextFn)(void *, void *, int, const char *, int);
typedef bool(__fastcall *ThisBoolFn)(void *, void *);
typedef bool(__fastcall *ThisSetBoolFn)(void *, void *, bool);
typedef TTalkLine *(__fastcall *GetLineFn)(void *, void *, int);
typedef TTalkLine *(__fastcall *NextFn)(void *, void *);
typedef void(__fastcall *LineFn)(void *, void *, uint32, int, const char *, int);
typedef void(__fastcall *NamedLineFn)(void *, void *, uint32, int, const char *, const char *, int);
typedef void(__fastcall *VectorFn)(void *, void *, int, int, int, TTalkFilter *);
typedef TTalkBuffer *(__cdecl *CreateFn)(int, int, const char *, int);
typedef void(__cdecl *DeleteFn)(TTalkBuffer *);
typedef bool(__fastcall *MatchFn)(void *, void *, int, const char *, int);

static_assert(sizeof(TTalkBuffer) == 0x2c, "TTalkBuffer: the original's layout in the oracle");
#endif

namespace {

__time32_t FixedTime = 1700000000;
#if GOLDEN_ORACLE
__time32_t __cdecl ImageTime(__time32_t *Time)
{
	if (Time != nullptr)
		*Time = FixedTime;
	return FixedTime;
}
#endif
// Our clock (issue #62: time(), the CRT's _time64): the same fixed time.
__time64_t __cdecl OurTime(__time64_t *Time)
{
	if (Time != nullptr)
		*Time = FixedTime;
	return FixedTime;
}

#if GOLDEN_ORACLE
uint8 SavedClock[5];
void *TestClockFunction;
tm *__cdecl ImageLocaltime(const __time32_t *Time) { return _localtime32(Time); }
int __cdecl ImageSprintf(char *Buffer, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vsprintf(Buffer, Format, Args);
	va_end(Args);
	return Result;
}
int __cdecl ImageFprintf(FILE *File, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vfprintf(File, Format, Args);
	va_end(Args);
	return Result;
}
#else
// The import slot of _time64 in the runner and what it held.
void **ClockSlot;
void *SavedSlot;

void SetClockSlot(void *Value)
{
	golden::SetImport(ClockSlot, Value);
}
#endif

void PatchTalk()
{
#if GOLDEN_ORACLE
	PatchImage();
	PatchJump(0x5121ea, (void *)ImageTime);
	PatchJump(0x513eb1, (void *)ImageLocaltime);
	PatchJump(0x511d6b, (void *)ImageSprintf);
	PatchJump(0x512173, (void *)ImageFprintf);
	DWORD Protection;
	void *ClockFunction = (void *)_time64;
	TestClockFunction = ClockFunction;
	memcpy(SavedClock, ClockFunction, 5);
	CHECK(VirtualProtect(ClockFunction, 5, PAGE_EXECUTE_READWRITE, &Protection) != 0, "patch test CRT clock");
	PatchJump((uint32)(uintptr_t)ClockFunction, (void *)OurTime);
	FlushInstructionCache(GetCurrentProcess(), ClockFunction, 5);
	VirtualProtect(ClockFunction, 5, Protection, &Protection);
	for (uint32 Address : {0x71ef70u, 0x71ef90u, 0x71efb0u, 0x71efd0u})
		if (Global<TTalkFilter **>(Address + 28) == NULL)
			At<VectorFn>(0x4fd6e0)((void *)(uintptr_t)Address, NULL, 0, Address == 0x71efd0 ? 4 : 9,
				Address == 0x71efd0 ? 5 : 10, NULL);
#else
	ClockSlot = golden::FindImport("_time64");
	CHECK(ClockSlot != nullptr, "patch test CRT clock");
	if (ClockSlot == nullptr) {
		printf("golden: Talk: the runner imports no _time64\n");
		exit(1);
	}
	SavedSlot = *ClockSlot;
	SetClockSlot((void *)OurTime);
#endif
}

void UnpatchTalk()
{
#if GOLDEN_ORACLE
	DWORD Protection;
	VirtualProtect(TestClockFunction, 5, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(TestClockFunction, SavedClock, 5);
	FlushInstructionCache(GetCurrentProcess(), TestClockFunction, 5);
	VirtualProtect(TestClockFunction, 5, Protection, &Protection);
#else
	SetClockSlot(SavedSlot);
#endif
}

bool SameText(const char *Ours, const char *Original)
{
	return (Ours == nullptr) == (Original == nullptr) && (Ours == nullptr || strcmp(Ours, Original) == 0);
}

// --- The original's lines and buffers, by value ---------------------------------

// A line of the original: its fields, its texts, where WithoutTime and Text point in Line.
struct TLineState {
	uint32 StatementID = 0;
	int Mode = 0, Time = 0, WithoutTimeOffset = 0, TextOffset = 0;
	golden::TText Addressee, Line, WithoutTime, Text;
};

#if GOLDEN_ORACLE
TLineState LineOf(const TTalkLine *O)
{
	TLineState State;
	State.StatementID = O->StatementID;
	State.Mode = O->Mode;
	State.Time = (int)O->Time;
	State.WithoutTimeOffset = (int)(O->WithoutTime - O->Line);
	State.TextOffset = (int)(O->Text - O->Line);
	auto Text = [](const char *Value, size_t Max = (size_t)-1) {
		golden::TText Result;
		Result.Null = Value == nullptr;
		if (Value != nullptr)
			Result.Text = std::string(Value, strnlen(Value, Max));
		return Result;
	};
	State.Addressee = Text(O->Addressee, sizeof(O->Addressee));
	State.Line = Text(O->Line);
	State.WithoutTime = Text(O->WithoutTime);
	State.Text = Text(O->Text);
	return State;
}

bool SameState(const TLineState &a, const TLineState &b)
{
	auto Same = [](const golden::TText &x, const golden::TText &y) { return x.Null == y.Null && x.Text == y.Text; };
	return a.StatementID == b.StatementID && a.Mode == b.Mode && a.Time == b.Time
		&& a.WithoutTimeOffset == b.WithoutTimeOffset && a.TextOffset == b.TextOffset && Same(a.Addressee, b.Addressee)
		&& Same(a.Line, b.Line) && Same(a.WithoutTime, b.WithoutTime) && Same(a.Text, b.Text);
}
#endif

// Records a line's contents (the oracle: State), returns them as recorded.
TLineState RecordLine([[maybe_unused]] const TLineState &State)
{
	TLineState Result;
	int Fields[5] = {};
#if GOLDEN_ORACLE
	Fields[0] = (int)State.StatementID;
	Fields[1] = State.Mode;
	Fields[2] = State.Time;
	Fields[3] = State.WithoutTimeOffset;
	Fields[4] = State.TextOffset;
	Result = State;
#endif
	golden::Output("line", Fields);
	Result.StatementID = (uint32)Fields[0];
	Result.Mode = Fields[1];
	Result.Time = Fields[2];
	Result.WithoutTimeOffset = Fields[3];
	Result.TextOffset = Fields[4];
	Result.Addressee = golden::RecordedText("line addressee", State.Addressee.c_str());
	Result.Line = golden::RecordedText("line text", State.Line.c_str());
	Result.WithoutTime = golden::RecordedText("line without time", State.WithoutTime.c_str());
	Result.Text = golden::RecordedText("line text only", State.Text.c_str());
	return Result;
}

// A line of the original on its own (not in a buffer's list).
TLineState OriginalLineState([[maybe_unused]] const TTalkLine *Line)
{
	TLineState State;
	ORIGINAL(State = LineOf(Line));
	return RecordLine(State);
}

// Every line of the original seen in a buffer, by identity.
std::vector<TLineState> LineTable;
#if GOLDEN_ORACLE
std::map<const TTalkLine *, int> LineIds;
#endif

int OriginalLineId([[maybe_unused]] const TTalkLine *Line)
{
	int Entry[2] = {0, 0}; // the identity, whether it is new
	TLineState State;
#if GOLDEN_ORACLE
	State = LineOf(Line);
	auto Found = LineIds.find(Line);
	if (Found != LineIds.end() && SameState(LineTable[(size_t)Found->second], State))
		Entry[0] = Found->second;
	else {
		Entry[0] = (int)LineTable.size();
		Entry[1] = 1;
	}
#endif
	golden::Output("line identity", Entry);
	if (Entry[1] != 0) {
		if (Entry[0] != (int)LineTable.size())
			throw golden::TStale{"Talk: a new line out of order"};
		LineTable.push_back(RecordLine(State));
		ORIGINAL(LineIds[Line] = Entry[0]);
	} else if (Entry[0] < 0 || Entry[0] >= (int)LineTable.size())
		throw golden::TStale{"Talk: an unknown line"};
	return Entry[0];
}

// A buffer of the original.
struct TBufferState {
	int ReceiveTalkMode = 0, SendTalkMode = 0;
	uint8 Changed = 0, Closed = 0;
	int lines = 0, limit = 0;
	uint8 Flags[6] = {};
	int Cursor = -1, Last = -1;
	std::vector<int> Ids, Previous;
};

#if GOLDEN_ORACLE
int OriginalIndex(const TTalkBuffer *Buffer, const TTalkLine *Line)
{
	int Index = 0;
	for (TTalkLine *Search = Buffer->first; Search != NULL; Search = Search->Next, Index++)
		if (Search == Line)
			return Index;
	return -1;
}
#endif

TBufferState OriginalBuffer([[maybe_unused]] const TTalkBuffer *O)
{
	TBufferState State;
	int Fields[8] = {};
	uint8 Bytes[8] = {};
	int Count = 0;
#if GOLDEN_ORACLE
	Fields[0] = O->ReceiveTalkMode;
	Fields[1] = O->SendTalkMode;
	Fields[2] = O->lines;
	Fields[3] = O->limit;
	Fields[4] = OriginalIndex(O, O->cursor);
	Fields[5] = OriginalIndex(O, O->last);
	memcpy(&Bytes[0], &O->Changed, 1);
	memcpy(&Bytes[1], &O->Closed, 1);
	memcpy(&Bytes[2], &O->ShowMessages, 6);
	for (const TTalkLine *Line = O->first; Line != NULL && Count < 1000000; Line = Line->Next)
		Count++;
	Fields[6] = Count;
#endif
	golden::Output("buffer", Fields);
	golden::Output("buffer flags", Bytes);
	State.ReceiveTalkMode = Fields[0];
	State.SendTalkMode = Fields[1];
	State.lines = Fields[2];
	State.limit = Fields[3];
	State.Cursor = Fields[4];
	State.Last = Fields[5];
	State.Changed = Bytes[0];
	State.Closed = Bytes[1];
	memcpy(State.Flags, &Bytes[2], 6);
	Count = Fields[6];
	State.Ids.resize((size_t)Count);
	State.Previous.resize((size_t)Count);
#if GOLDEN_ORACLE
	int i = 0;
	for (const TTalkLine *Line = O->first; i < Count; Line = Line->Next, i++) {
		State.Ids[(size_t)i] = OriginalLineId(Line);
		State.Previous[(size_t)i] = OriginalIndex(O, Line->Previous);
	}
#else
	for (int i = 0; i < Count; i++)
		State.Ids[(size_t)i] = OriginalLineId(nullptr);
#endif
	if (Count > 0)
		golden::OutputArray("previous lines", State.Previous.data(), State.Previous.size());
	return State;
}

void CheckLine(const TTalkLine *Ours, const TLineState *Original)
{
	CHECK(Ours->StatementID == Original->StatementID && Ours->Mode == Original->Mode
			&& (int)Ours->Time == Original->Time,
		"statement scalars");
	CHECK(strcmp(Ours->Addressee, Original->Addressee.c_str()) == 0, "statement addressee");
	CHECK(SameText(Ours->Line, Original->Line.c_str()) && SameText(Ours->WithoutTime, Original->WithoutTime.c_str())
			&& SameText(Ours->Text, Original->Text.c_str()),
		"statement text: '%s', original '%s'", Ours->Line, Original->Line.c_str());
	CHECK(Ours->WithoutTime - Ours->Line == Original->WithoutTimeOffset
			&& Ours->Text - Ours->Line == Original->TextOffset,
		"statement offsets");
}
int LineIndex(const TTalkBuffer *Buffer, const TTalkLine *Line)
{
	int Index = 0;
	for (TTalkLine *Search = Buffer->first; Search != nullptr; Search = Search->Next, Index++)
		if (Search == Line)
			return Index;
	return -1;
}
uint8 Byte(const bool &Value)
{
	uint8 Result;
	memcpy(&Result, &Value, 1);
	return Result;
}
void CheckBuffer(const TTalkBuffer *Ours, const TBufferState &Original)
{
	CHECK(Ours->ReceiveTalkMode == Original.ReceiveTalkMode && Ours->SendTalkMode == Original.SendTalkMode,
		"buffer modes");
	CHECK(Ours->lines == Original.lines && Ours->limit == Original.limit && Byte(Ours->Changed) == Original.Changed
			&& Byte(Ours->Closed) == Original.Closed,
		"buffer state");
	CHECK(memcmp(&Ours->ShowMessages, Original.Flags, 6) == 0, "buffer flags");
	CHECK(LineIndex(Ours, Ours->cursor) == Original.Cursor, "cursor index");
	TTalkLine *OurLine = Ours->first;
	size_t OriginalLine = 0;
	int Count = 0;
	while (OurLine != nullptr && OriginalLine < Original.Ids.size() && Count <= Ours->lines) {
		CheckLine(OurLine, &LineTable[(size_t)Original.Ids[OriginalLine]]);
		CHECK(LineIndex(Ours, OurLine->Previous) == Original.Previous[OriginalLine], "previous link");
		OurLine = OurLine->Next;
		OriginalLine++;
		Count++;
	}
	CHECK(OurLine == nullptr && OriginalLine == Original.Ids.size() && Count == Ours->lines, "linked list length");
	CHECK(LineIndex(Ours, Ours->last) == Original.Last, "last line");
}
void CheckBuffer(const TTalkBuffer *Ours, [[maybe_unused]] const TTalkBuffer *Original)
{
	CheckBuffer(Ours, OriginalBuffer(Original));
}

#if GOLDEN_ORACLE
TTalkLine *OriginalLine(uint32 ID, int Mode, const char *Name, const char *Text, int DiffTime)
{
	TTalkLine *Line = (TTalkLine *)malloc(sizeof(TTalkLine));
	memset(Line, 0xa5, sizeof(TTalkLine));
	if (Name == NULL)
		At<LineFn>(0x5044b0)(Line, NULL, ID, Mode, Text, DiffTime);
	else
		At<NamedLineFn>(0x5048a0)(Line, NULL, ID, Mode, Name, Text, DiffTime);
	return Line;
}
void FreeLine(TTalkLine *Line)
{
	free(Line->Line);
	free(Line);
}
TTalkBuffer *OriginalDefault() { return Global<TTalkBuffer *>(0x71ef68); }
#else
TTalkBuffer *OriginalDefault() { return nullptr; }
#endif
void FreeOurLine(TTalkLine *Line)
{
	free(Line->Line);
	free(Line);
}

void TestHelpers()
{
	for (int Mode = 1; Mode <= 25; Mode++) {
		TRGBColor Ours, Original;
		GetTalkColor(&Ours, Mode);
		uint8 Returned = ORIGINAL_VALUE("talk color returned", uint8,
			At<TRGBColor *(__cdecl *)(TRGBColor *, int)>(0x4fd9e0)(&Original, Mode) == &Original);
		static_assert(sizeof(TRGBColor) == 12, "TRGBColor: three ints on both sides");
		golden::OutputBytes("talk color", &Original, sizeof(Original));
		CHECK(Returned != 0 && memcmp(&Ours, &Original, sizeof(Ours)) == 0, "talk color %d", Mode);
	}
	struct {
		uint32 Set, Get;
		void (*OurSet)(bool);
		bool (*OurGet)();
	} Flags[] = {{0x4fd4e0, 0x4fd4f0, SetIgnoreEnabled, GetIgnoreEnabled}, {0x4fd500, 0x4fd510, SetBuddyIgnore, GetBuddyIgnore},
		{0x4fd520, 0x4fd530, SetIgnoreListEnabled, GetIgnoreListEnabled}};
	for (auto &Flag : Flags)
		for (bool Value : {false, true}) {
			Flag.OurSet(Value);
			ORIGINAL(At<SetBoolFn>(Flag.Set)(Value));
			bool Got = ORIGINAL_VALUE("talk flag", bool, At<BoolFn>(Flag.Get)());
			CHECK(Flag.OurGet() == Got, "Talk flag");
		}
	for (int Round = 0; Round < 30; Round++) {
		SetTalkChanged();
		ORIGINAL(At<VoidFn>(0x4fd4c0)());
		bool First = ORIGINAL_VALUE("talk changed", bool, At<BoolFn>(0x4fd440)());
		bool Second = ORIGINAL_VALUE("talk changed again", bool, At<BoolFn>(0x4fd440)());
		CHECK(GetTalkChanged() == First && !GetTalkChanged() && !Second, "talk changed latch");
		SetConsoleChanged();
		ORIGINAL(At<VoidFn>(0x4fd590)());
		First = ORIGINAL_VALUE("console changed", bool, At<BoolFn>(0x4fd580)());
		Second = ORIGINAL_VALUE("console changed again", bool, At<BoolFn>(0x4fd580)());
		CHECK(GetConsoleChanged() == First && !GetConsoleChanged() && !Second, "console changed latch");
		for (int Delta : {0, 999, 1000, 1001}) {
			SetTestClock(10000);
			SetTalkThrottle();
			ORIGINAL(At<VoidFn>(0x4fd540)());
			SetTestClock(10000 + Delta);
			bool Throttled = ORIGINAL_VALUE("throttled", bool, At<BoolFn>(0x4fd550)());
			CHECK(IsTalkThrottled() == Throttled, "throttle %d", Delta);
			Throttled = ORIGINAL_VALUE("throttled again", bool, At<BoolFn>(0x4fd550)());
			CHECK(IsTalkThrottled() == Throttled, "throttle repeated");
		}
	}
	for (bool Permanent : {false, true}) {
		SetStatusMessage("status", Permanent);
		ORIGINAL(At<void(__cdecl *)(const char *, bool)>(0x4ff500)("status", Permanent));
		for (int Tick = 0; Tick < 70; Tick++) {
			golden::TText Status = ORIGINAL_TEXT("status message", At<const char *(__cdecl *)()>(0x4fd4d0)());
			CHECK(SameText(GetStatusMessage(), Status.c_str()), "status timer %d", Tick);
			UpdateStatusMessage();
			ORIGINAL(At<VoidFn>(0x4ff7d0)());
			bool Changed = ORIGINAL_VALUE("status changed", bool, At<BoolFn>(0x4fd440)());
			CHECK(GetTalkChanged() == Changed, "status changed");
		}
	}
	SetStatusMessage(nullptr, false);
	ORIGINAL(At<void(__cdecl *)(const char *, bool)>(0x4ff500)(NULL, false));
	bool Cleared = ORIGINAL_VALUE("status cleared", bool, At<const char *(__cdecl *)()>(0x4fd4d0)() == NULL);
	CHECK(GetStatusMessage() == nullptr && Cleared, "clear status");
	const char *Edges[] = {"", "\n", "\n\n", "a\nb\n", "                                        ",
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "abc\tdef\rghi"};
	static_assert(sizeof(TTalkbox) == 0x1c0, "TTalkbox: int and char arrays on both sides");
	for (int Round = 0; Round < 4000; Round++) {
		char Text[900];
		if (Round < 8)
			strcpy(Text, Edges[Round]);
		else {
			RandomText(Text, Random() % 899);
			for (char *Search = Text; *Search != 0; Search++)
				if (Random() % 13 == 0)
					*Search = '\n';
		}
		TTalkbox Ours, Original;
		memset(&Ours, 0xa5, sizeof(Ours));
		Ours.Lines = Round < 8 ? 0 : Random() % 12;
		Original = Ours;
		WrapTalkbox(&Ours, Text);
		ORIGINAL(At<void(__cdecl *)(TTalkbox *, const char *)>(0x4fecc0)(&Original, Text));
		golden::OutputBytes("WrapTalkbox", &Original, sizeof(Original));
		CHECK(memcmp(&Ours, &Original, sizeof(Ours)) == 0, "WrapTalkbox round %d", Round);
	}
}

void TestFilters()
{
	for (int Round = 0; Round < 6000; Round++) {
		TTalkFilter Filter = {};
		Filter.Mode = Random() % 27;
		Filter.Channel = (int)(Random() % 10) - 1;
		strcpy(Filter.Addressee, Round % 3 == 0 ? "" : "Alice");
		int Mode = Random() % 30;
		int Channel = (int)(Random() % 10) - 1;
		const char *Name = Round % 3 == 0 ? nullptr : Round % 3 == 1 ? "ALICE"
																	 : "Bob";
		bool Matches = ORIGINAL_VALUE("filter match", bool, At<MatchFn>(0x4ff810)(&Filter, NULL, Mode, Name, Channel));
		CHECK(Filter.Matches(Mode, Name, Channel) == Matches, "filter match");
	}
	vector<TTalkFilter *> Ours(0, 4, 5, nullptr);
#if GOLDEN_ORACLE
	alignas(4) uint8 OriginalData[32] = {};
	At<VectorFn>(0x4fd6e0)(OriginalData, NULL, 0, 4, 5, NULL);
	vector<TTalkFilter *> &Original = *(vector<TTalkFilter *> *)OriginalData;
#endif
	for (int Count = 0; Count < 65; Count++) {
		char Name[30];
		sprintf(Name, "filter%d", Count);
		TTalkBuffer *Buffer = (TTalkBuffer *)(uintptr_t)(Count + 1);
		AddTalkFilter(Count % 9, Name, Count, Buffer, Ours, (Count & 1) != 0);
		ORIGINAL(At<void(__cdecl *)(int, const char *, int, TTalkBuffer *, void *, bool)>(0x4ffe40)(Count % 9, Name, Count,
			Buffer, &Original, (Count & 1) != 0));
		uint8 Header[21] = {};
		ORIGINAL(memcpy(Header, OriginalData, sizeof(Header)));
		golden::Output("filter vector", Header);
		CHECK(memcmp(&Ours, Header, 21) == 0, "filter vector layout %d", Count);
		int Index = FindTextBuffer(Buffer, Ours);
		int Found = ORIGINAL_VALUE("find buffer", int, At<int(__cdecl *)(TTalkBuffer *, void *)>(0x4ffb90)(Buffer, &Original));
		CHECK(Index == Found, "find buffer");
		Found = ORIGINAL_VALUE("find filter", int,
			At<int(__cdecl *)(int, const char *, int, void *)>(0x4ff9c0)(Count % 9, Name, Count, &Original));
		CHECK(FindTalkFilter(Count % 9, Name, Count, Ours) == Found, "find filter");
		int Entry[3] = {};
		ORIGINAL(Entry[0] = Original(Index)->Mode; Entry[1] = Original(Index)->Channel; Entry[2] = Original(Index)->Permanent);
		golden::Output("new filter", Entry);
		golden::TText Addressee = ORIGINAL_TEXT("new filter addressee", Original(Index)->Addressee, 30);
		CHECK(Ours(Index)->Mode == Entry[0] && Ours(Index)->Channel == Entry[1] && Ours(Index)->Permanent == (Entry[2] != 0)
				&& strcmp(Ours(Index)->Addressee, Addressee.c_str()) == 0,
			"new filter");
	}
	for (int Index = Ours.min; Index <= Ours.max; Index++) {
		free(Ours(Index));
		ORIGINAL(free(Original(Index)));
	}
	// vector<T>'s destructor deletes the entries again at the end of the function.
	Ours.entry.reset();
	Ours.entry = nullptr;
	ORIGINAL(free(Original.entry.get()));
	const char *Names[] = {"Alice", "Bob", "alice", "Carol", "", "Daniel"};
	for (int Round = 0; Round < 2500; Round++) {
		const char *Name = Names[Random() % 6];
		bool Permanent = (Random() & 1) != 0;
		int Mode = Random() % 27;
		switch (Random() % 17) {
		case 0:
			AddBuddyIgnore(Name, Permanent);
			ORIGINAL(At<void(__cdecl *)(const char *, bool)>(0x5006f0)(Name, Permanent));
			break;
		case 1:
			IgnoreName(Name, Permanent);
			ORIGINAL(At<void(__cdecl *)(const char *, bool)>(0x501940)(Name, Permanent));
			break;
		case 2:
			IgnoreMode(Mode);
			ORIGINAL(At<void(__cdecl *)(int)>(0x5017b0)(Mode));
			break;
		case 3:
			UnignoreName(Name);
			ORIGINAL(At<void(__cdecl *)(const char *)>(0x502010)(Name));
			break;
		case 4:
			UnignoreMode(Mode);
			ORIGINAL(At<void(__cdecl *)(int)>(0x501e70)(Mode));
			break;
		case 5:
			AddTemporaryIgnore(Name);
			ORIGINAL(At<void(__cdecl *)(const char *)>(0x500c60)(Name));
			break;
		case 6:
			RemoveTemporaryIgnore(Name);
			ORIGINAL(At<void(__cdecl *)(const char *)>(0x500e40)(Name));
			break;
		case 7:
			ClearBuddyIgnore();
			ORIGINAL(At<VoidFn>(0x500520)());
			break;
		case 8:
			ClearTemporaryIgnore();
			ORIGINAL(At<VoidFn>(0x500a90)());
			break;
		case 9:
			ClearIgnoreList();
			ORIGINAL(At<VoidFn>(0x501400)());
			break;
		case 10:
			SetIgnoreEnabled(Permanent);
			ORIGINAL(At<SetBoolFn>(0x4fd4e0)(Permanent));
			break;
		case 11:
			SetBuddyIgnore(Permanent);
			ORIGINAL(At<SetBoolFn>(0x4fd500)(Permanent));
			break;
		case 12:
			SetIgnoreListEnabled(Permanent);
			ORIGINAL(At<SetBoolFn>(0x4fd520)(Permanent));
			break;
		case 13:
			AddIgnoreFilter(Mode, Name, Permanent);
			ORIGINAL(At<void(__cdecl *)(int, const char *, bool)>(0x5015d0)(Mode, Name, Permanent));
			break;
		case 14:
			RemoveIgnoreFilter(Mode, Name);
			ORIGINAL(At<void(__cdecl *)(int, const char *)>(0x501ca0)(Mode, Name));
			break;
		case 15:
			ShowPermanentIgnoreWarning();
			ORIGINAL(At<VoidFn>(0x5076e0)());
			break;
		case 16:
			ClearDefaultTextBuffer();
			ORIGINAL(At<VoidFn>(0x5021a0)());
			break;
		}
		bool Ignored = ORIGINAL_VALUE("ignored mode", bool, At<bool(__cdecl *)(int)>(0x501260)(Mode));
		CHECK(IsModeIgnored(Mode) == Ignored, "ignored mode");
		Ignored = ORIGINAL_VALUE("permanent ignore", bool, At<bool(__cdecl *)(const char *)>(0x501ae0)(Name));
		CHECK(IsPermanentIgnore(Name) == Ignored, "permanent ignore");
		Ignored = ORIGINAL_VALUE("permanent buddy ignore", bool, At<bool(__cdecl *)(const char *)>(0x5008d0)(Name));
		CHECK(IsPermanentBuddyIgnore(Name) == Ignored, "permanent buddy ignore");
		for (int TestMode : {0, 1, 4, 9, 10, 11, 12, 18, 26}) {
			Ignored = ORIGINAL_VALUE("ignore decision", bool,
				At<bool(__cdecl *)(int, const char *, int)>(0x500060)(TestMode, Name, -1));
			CHECK(IsTalkIgnored(TestMode, Name, -1) == Ignored, "ignore decision");
		}
		char *OurBuddy = GetBuddyIgnoreText();
		golden::TText OriginalBuddy, OriginalIgnore;
		ORIGINAL(char *Got = At<char *(__cdecl *)()>(0x5002d0)());
		OriginalBuddy = ORIGINAL_TEXT("buddy ignore text", Got);
		ORIGINAL(free(Got));
		char *OurIgnore = GetIgnoreListText();
		ORIGINAL(Got = At<char *(__cdecl *)()>(0x501010)());
		OriginalIgnore = ORIGINAL_TEXT("ignore list text", Got);
		ORIGINAL(free(Got));
		CHECK(SameText(OurBuddy, OriginalBuddy.c_str()) && SameText(OurIgnore, OriginalIgnore.c_str()), "serialized filters");
		free(OurBuddy);
		free(OurIgnore);
		CheckBuffer(DefaultTextBuffer, OriginalDefault());
	}
}

void TestTalkboxes()
{
	ResetTalk();
	ORIGINAL(At<VoidFn>(0x504d70)());
	SetPrivateMessages(true);
	ORIGINAL(At<SetBoolFn>(0x4db5b0)(true));
	AddTalkbox(1, "Alice", 1, 2, 3, "seed");
	ORIGINAL(At<void(__cdecl *)(int, const char *, int, int, int, const char *)>(0x5052e0)(1, "Alice", 1, 2, 3, "seed"));
	TTalkbox *OurBoxes = GetTalkbox(1);
	// The original's ten talkboxes (0x71dde8) as recorded the time before: only the changes are recorded.
	static TTalkbox OriginalBoxes[10];
	SetTestClock(10000);
	UpdateTalkboxes();
	ORIGINAL(At<VoidFn>(0x4ff130)());
	int Clock = 10000;
	int Modes[] = {1, 2, 3, 4, 9, 11, 16, 17};
	for (int Round = 0; Round < 6000; Round++) {
		int Choice = Random() % 12;
		if (Choice < 7) {
			char Text[500];
			RandomText(Text, Random() % 499);
			int Mode = Modes[Random() % 8];
			const char *Speaker = Round % 2 == 0 ? "Alice" : "Bob";
			int x = Random() % 3, y = Random() % 3, z = Random() % 2;
			AddTalkbox(Mode, Speaker, x, y, z, Text);
			ORIGINAL(At<void(__cdecl *)(int, const char *, int, int, int, const char *)>(0x5052e0)(Mode, Speaker, x, y, z, Text));
		} else if (Choice == 7) {
			int Mode = Round % 3 == 0 ? 18 : Round % 3 == 1 ? 19
															: 22;
			AddMessageTalkbox(Mode, "information\nsecond line");
			ORIGINAL(At<void(__cdecl *)(int, const char *)>(0x505ac0)(Mode, "information\nsecond line"));
		} else if (Choice == 8) {
			int Position = Random() % 10;
			RemoveTalkbox(Position);
			ORIGINAL(At<void(__cdecl *)(int)>(0x4fe960)(Position));
		} else if (Choice == 9) {
			RemoveOldestTalkbox();
			ORIGINAL(At<VoidFn>(0x4ff330)());
		} else if (Choice == 10) {
			Clock += Random() % 1500;
			SetTestClock(Clock);
			UpdateTalkboxes();
			ORIGINAL(At<VoidFn>(0x4ff130)());
		} else {
			bool Enabled = (Random() & 1) != 0;
			SetPrivateMessages(Enabled);
			ORIGINAL(At<SetBoolFn>(0x4db5b0)(Enabled));
		}
		golden::OutputChanges("talkboxes", OriginalBoxes, ORIGINAL_OR((const void *)(uintptr_t)0x71dde8, nullptr),
			sizeof(OriginalBoxes));
		CHECK(memcmp(OurBoxes, OriginalBoxes, sizeof(TTalkbox) * 10) == 0, "talkbox state round %d", Round);
		bool Changed = ORIGINAL_VALUE("talkbox changed", bool, At<BoolFn>(0x4fd440)());
		CHECK(GetTalkChanged() == Changed, "talkbox changed");
		int Count = GetNumberOfTalkboxes();
		int OriginalCount = ORIGINAL_VALUE("talkbox count", int, At<IntFn>(0x4fd450)());
		CHECK(Count == OriginalCount, "talkbox count");
		for (int Number = 1; Number <= Count; Number++) {
			int Index = ORIGINAL_VALUE("talkbox lookup", int,
				At<TTalkbox *(__cdecl *)(int)>(0x4fdd80)(Number) - (TTalkbox *)(uintptr_t)0x71dde8);
			CHECK(GetTalkbox(Number) - OurBoxes == Index, "talkbox lookup");
			int Mode = ORIGINAL_VALUE("talkbox mode", int, At<int(__cdecl *)(int)>(0x4fdf10)(Number));
			CHECK(GetTalkboxMode(Number) == Mode, "talkbox mode");
			int OurPosition[3], OriginalPosition[3] = {};
			GetTalkboxPosition(Number, &OurPosition[0], &OurPosition[1], &OurPosition[2]);
			ORIGINAL(At<void(__cdecl *)(int, int *, int *, int *)>(0x4fe0b0)(Number, &OriginalPosition[0], &OriginalPosition[1],
				&OriginalPosition[2]));
			golden::Output("talkbox position", OriginalPosition);
			CHECK(memcmp(OurPosition, OriginalPosition, sizeof(OurPosition)) == 0, "talkbox position");
			int Left = Random() % 100, Top = Random() % 100;
			SetTalkboxRectangle(Number, Left, Top, 80, 30);
			ORIGINAL(At<void(__cdecl *)(int, int, int, int, int)>(0x4fef70)(Number, Left, Top, 80, 30));
			int OurRect[4], OriginalRect[4] = {};
			GetTalkboxRectangle(Number, &OurRect[0], &OurRect[1], &OurRect[2], &OurRect[3]);
			ORIGINAL(At<void(__cdecl *)(int, int *, int *, int *, int *)>(0x4fe2b0)(Number, &OriginalRect[0], &OriginalRect[1],
				&OriginalRect[2], &OriginalRect[3]));
			golden::Output("talkbox rectangle", OriginalRect);
			CHECK(memcmp(OurRect, OriginalRect, sizeof(OurRect)) == 0, "talkbox rectangle");
			TRGBColor OurColor, OriginalColor;
			GetTalkboxColor(&OurColor, Number);
			ORIGINAL(At<TRGBColor *(__cdecl *)(TRGBColor *, int)>(0x4fe470)(&OriginalColor, Number));
			golden::OutputBytes("talkbox color", &OriginalColor, sizeof(OriginalColor));
			CHECK(memcmp(&OurColor, &OriginalColor, sizeof(OurColor)) == 0, "talkbox color");
			int Lines = ORIGINAL_VALUE("talkbox lines", int, At<int(__cdecl *)(int)>(0x4fe620)(Number));
			CHECK(GetTalkboxLines(Number) == Lines, "talkbox lines");
			for (int Line = 0; Line < GetTalkboxLines(Number); Line++) {
				golden::TText Text = ORIGINAL_TEXT("talkbox line", At<const char *(__cdecl *)(int, int)>(0x4fe7c0)(Number, Line));
				CHECK(strcmp(GetTalkboxLine(Number, Line), Text.c_str()) == 0, "talkbox line");
			}
		}
		int Free = ORIGINAL_VALUE("free talkbox", int, At<IntFn>(0x505120)());
		CHECK(FindFreeTalkbox() == Free, "free talkbox");
	}
}

// The bytes of a constructed buffer that no field covers (from the end of the vtable pointer to the
// end of Data): -1 unless they all hold the same value, else that value.
int UntouchedBytes(const uint8 *Data, size_t Size, const TTalkBuffer *Buffer)
{
	std::vector<bool> Field(Size, false);
	auto Mark = [&](const void *Member, size_t Length) {
		size_t Offset = (size_t)((const uint8 *)Member - Data);
		for (size_t i = 0; i < Length && Offset + i < Size; i++)
			Field[Offset + i] = true;
	};
	Mark(&Buffer->ReceiveTalkMode, sizeof(int));
	Mark(&Buffer->SendTalkMode, sizeof(int));
	Mark(&Buffer->Changed, 1);
	Mark(&Buffer->Closed, 1);
	Mark(&Buffer->first, sizeof(void *));
	Mark(&Buffer->last, sizeof(void *));
	Mark(&Buffer->cursor, sizeof(void *));
	Mark(&Buffer->lines, sizeof(int));
	Mark(&Buffer->limit, sizeof(int));
	Mark(&Buffer->ShowMessages, 6);
	int Value = -2;
	for (size_t i = sizeof(void *); i < Size; i++) {
		if (Field[i])
			continue;
		if (Value == -2)
			Value = Data[i];
		else if (Value != Data[i])
			return -1;
	}
	return Value;
}

void TestLinesAndBuffers()
{
	for (int Round = 0; Round < 1500; Round++) {
		char Text[700], Name[80];
		RandomText(Text, Random() % 699);
		RandomText(Name, Random() % 79);
		uint32 ID = Random();
		int Mode = Random() % 27;
		int DiffTime = (int)(Random() % 200000) - 100000;
		bool Named = (Round & 1) != 0;
		ORIGINAL(TTalkLine *Original = OriginalLine(ID, Mode, Named ? Name : NULL, Text, DiffTime));
		TTalkLine *Ours = Named ? new TTalkLine(ID, Mode, Name, Text, DiffTime) : new TTalkLine(ID, Mode, Text, DiffTime);
		TLineState State = OriginalLineState(ORIGINAL_OR(Original, nullptr));
		CheckLine(Ours, &State);
		for (bool Stamps : {false, true}) {
			SetTimeStamps(Stamps);
			ORIGINAL(At<SetBoolFn>(0x4db5a0)(Stamps));
			golden::TText Got = ORIGINAL_TEXT("GetText", At<const char *(__fastcall *)(void *, void *)>(0x4fd6c0)(Original, NULL));
			CHECK(strcmp(Ours->GetText(), Got.c_str()) == 0, "GetText");
		}
		FreeOurLine(Ours);
		ORIGINAL(FreeLine(Original));
	}
	alignas(8) uint8 OurData[80];
	memset(OurData, 0x55, sizeof(OurData));
	TTalkBuffer *Ours = new (OurData) TTalkBuffer(1);
#if GOLDEN_ORACLE
	alignas(8) uint8 OriginalData[80];
	memset(OriginalData, 0x55, sizeof(OriginalData));
	TTalkBuffer *Original = (TTalkBuffer *)OriginalData;
	At<ThisIntFn>(0x4fd5a0)(Original, NULL, 1);
#else
	TTalkBuffer *Original = nullptr;
#endif
	{
		// The base constructor, the bytes it leaves untouched included: field by field (the layouts
		// differ), and the bytes no field covers.
		TBufferState Constructed = OriginalBuffer(Original);
		int Pointers = ORIGINAL_VALUE("constructed pointers", int,
			(Original->first != NULL) | (Original->last != NULL) << 1 | (Original->cursor != NULL) << 2);
		int Untouched = ORIGINAL_VALUE("constructed untouched", int, UntouchedBytes(OriginalData, sizeof(OriginalData), Original));
		CHECK(Ours->ReceiveTalkMode == Constructed.ReceiveTalkMode && Ours->SendTalkMode == Constructed.SendTalkMode
				&& Byte(Ours->Changed) == Constructed.Changed && Byte(Ours->Closed) == Constructed.Closed
				&& Ours->first == nullptr && Ours->last == nullptr && Ours->cursor == nullptr && Pointers == 0
				&& Ours->lines == Constructed.lines && Ours->limit == Constructed.limit
				&& memcmp(&Ours->ShowMessages, Constructed.Flags, 6) == 0
				&& UntouchedBytes(OurData, sizeof(OurData), Ours) == Untouched && Untouched >= 0,
			"base constructor including untouched bytes");
	}
	Ours->Closed = false;
	ORIGINAL(Original->Closed = false);
	for (int Round = 0; Round < 2500; Round++) {
		int Choice = Random() % 10;
		if (Choice < 4) {
			char Text[100];
			RandomText(Text, Random() % 99);
			int Mode = Random() % 27;
			if (Choice == 0) {
				Ours->AddText(Mode, Text, 42);
				ORIGINAL(At<ThisTextFn>(0x506930)(Original, NULL, Mode, Text, 42));
			} else {
				TTalkLine *OurLine = new TTalkLine(Round, Mode, "Alice", Text, 42);
				ORIGINAL(TTalkLine *OtherLine = OriginalLine(Round, Mode, "Alice", Text, 42));
				Ours->Append(OurLine);
				ORIGINAL(At<ThisLineFn>(0x502c00)(Original, NULL, OtherLine));
			}
		} else if (Choice == 4 && Ours->lines > 0) {
			int Index = Random() % Ours->lines;
			TTalkLine *OurLine = Ours->GetLine(Index);
			ORIGINAL(TTalkLine *OtherLine = At<GetLineFn>(0x503670)(Original, NULL, Index));
			Ours->Remove(OurLine);
			ORIGINAL(At<ThisLineFn>(0x502f00)(Original, NULL, OtherLine));
		} else if (Choice == 5 && Ours->lines > 0) {
			Ours->Remove(0);
			ORIGINAL(At<ThisIntFn>(0x503210)(Original, NULL, 0));
		} else if (Choice == 6) {
			Ours->Clear();
			ORIGINAL(At<ThisFn>(0x502a60)(Original, NULL));
		} else if (Choice == 7) {
			bool Enabled = (Random() & 1) != 0;
			Ours->Enabled = Enabled;
			ORIGINAL(Original->Enabled = Enabled);
			int Limit = Random() % 10;
			Ours->limit = Limit;
			ORIGINAL(Original->limit = Limit);
		} else if (Choice == 8 && Ours->lines > 0) {
			int Index = Random() % Ours->lines;
			TTalkLine *OurLine = Ours->GetLine(Index);
			TLineState OtherLine = OriginalLineState(ORIGINAL_OR(At<GetLineFn>(0x503670)(Original, NULL, Index), nullptr));
			CheckLine(OurLine, &OtherLine);
			bool OtherEnd = ORIGINAL_VALUE("GetNext", bool, At<NextFn>(0x503920)(Original, NULL) == NULL);
			CHECK((Ours->GetNext() == nullptr) == OtherEnd, "GetNext");
		} else if (Choice == 9) {
			Ours->Close();
			ORIGINAL(At<ThisFn>(0x4fd600)(Original, NULL));
		}
		CheckBuffer(Ours, Original);
		bool Value = ORIGINAL_VALUE("buffer changed latch", bool, At<ThisBoolFn>(0x4fd5f0)(Original, NULL));
		CHECK(Ours->GetChanged() == Value, "buffer changed latch");
		Value = ORIGINAL_VALUE("buffer closed latch", bool, At<ThisBoolFn>(0x4fd6b0)(Original, NULL));
		CHECK(Ours->GetClosed() == Value, "buffer closed latch");
		Value = ORIGINAL_VALUE("isDefault", bool, At<ThisBoolFn>(0x4fd620)(Original, NULL));
		CHECK(Ours->isDefault() == Value, "isDefault");
		Value = ORIGINAL_VALUE("AcceptMessages", bool, At<ThisBoolFn>(0x4fd630)(Original, NULL));
		CHECK(Ours->AcceptMessages() == Value, "AcceptMessages");
		Value = ORIGINAL_VALUE("base predicate", bool, At<ThisBoolFn>(0x4fd640)(Original, NULL));
		CHECK(Ours->IsRuleViolation() == Value, "base predicate");
		Value = (Random() & 1) != 0;
		Ours->SetSelected(Value);
		ORIGINAL(At<ThisSetBoolFn>(0x4fd660)(Original, NULL, Value));
		Ours->SetLocked(Value);
		ORIGINAL(At<ThisSetBoolFn>(0x4fd670)(Original, NULL, Value));
		bool Selected = ORIGINAL_VALUE("buffer selected", bool, At<ThisBoolFn>(0x4fd650)(Original, NULL));
		bool Locked = ORIGINAL_VALUE("buffer locked", bool, At<ThisBoolFn>(0x4fd680)(Original, NULL));
		CHECK(Ours->GetSelected() == Selected && Ours->GetLocked() == Locked, "buffer lock/select");
		int Receive = ORIGINAL_VALUE("inline receive mode", int, At<int(__fastcall *)(void *, void *)>(0x4fd690)(Original, NULL));
		int Send = ORIGINAL_VALUE("inline send mode", int, At<int(__fastcall *)(void *, void *)>(0x4fd6a0)(Original, NULL));
		CHECK(Ours->GetReceiveTalkMode() == Receive && Ours->GetSendTalkMode() == Send, "inline modes");
	}
	Ours->Enabled = true;
	ORIGINAL(Original->Enabled = true);
	Ours->limit = 100;
	ORIGINAL(Original->limit = 100);
	Ours->AddText(1, "saved text", 0);
	ORIGINAL(At<ThisTextFn>(0x506930)(Original, NULL, 1, "saved text", 0));
	for (bool Stamps : {false, true}) {
		SetTimeStamps(Stamps);
		ORIGINAL(At<SetBoolFn>(0x4db5a0)(Stamps));
		TFile OurFile; // TTalkBuffer::Write takes a TFile (issue #64); a tmpfile as before
		OurFile.OpenTemporary();
		ORIGINAL(FILE *OtherFile = tmpfile());
		bool OtherOpen = ORIGINAL_VALUE("temporary console log", bool, OtherFile != NULL);
		CHECK(OurFile != nullptr && OtherOpen, "temporary console logs");
		if (OurFile != nullptr && OtherOpen) {
			int A = Ours->Write(OurFile);
			int B = ORIGINAL_VALUE("Write result", int, At<int(__fastcall *)(void *, void *, FILE *)>(0x503a70)(Original, NULL, OtherFile));
			CHECK(A == B, "Write result");
			OurFile.Seek(0, SEEK_SET);
			ORIGINAL(rewind(OtherFile));
			char OurText[4096] = {}, OtherText[4096] = {};
			OurFile.Read(OurText, 1, 4095);
			ORIGINAL(fread(OtherText, 1, 4095, OtherFile));
			std::string Other = golden::OutputText("console log output", OtherText);
			CHECK(strcmp(OurText, Other.c_str()) == 0, "console log output");
		}
		OurFile.Close();
		ORIGINAL(if (OtherFile != NULL) fclose(OtherFile));
	}
	Ours->~TTalkBuffer();
	ORIGINAL(At<ThisFn>(0x506920)(Original, NULL));
	CheckBuffer(Ours, Original);
}

#if GOLDEN_ORACLE
template <class Function> Function Virtual(TTalkBuffer *Buffer, int Offset)
{
	return (Function)(*(void ***)Buffer)[Offset / 4];
}
#endif

void TestRouting()
{
	struct {
		int Receive, Send;
		const char *Name;
		int Channel;
	} Cases[] = {{1, 1, nullptr, -1}, {4, 4, "Alice", -1}, {5, 5, nullptr, 2}, {5, 5, nullptr, 8}, {7, 8, "Reporter", -1},
		{6, 0, nullptr, 3}};
	std::vector<TTalkBuffer *> Ours, Original; // the runner keeps NULL for the original's buffers
	for (auto &Case : Cases) {
		TTalkBuffer *OurBuffer = CreateTextBuffer(Case.Receive, Case.Send, Case.Name, Case.Channel);
		TTalkBuffer *OtherBuffer = ORIGINAL_OR(At<CreateFn>(0x507bc0)(Case.Receive, Case.Send, Case.Name, Case.Channel), nullptr);
		bool OtherCreated = ORIGINAL_VALUE("CreateTextBuffer", bool, OtherBuffer != NULL);
		CHECK(OurBuffer != nullptr && OtherCreated, "CreateTextBuffer receive %d send %d", Case.Receive, Case.Send);
		if (OurBuffer == nullptr || !OtherCreated)
			return;
		OurBuffer->Closed = false;
		ORIGINAL(OtherBuffer->Closed = false);
		OurBuffer->ShowMessages = true;
		ORIGINAL(OtherBuffer->ShowMessages = true);
		CheckBuffer(OurBuffer, OtherBuffer);
		int Receive = ORIGINAL_VALUE("factory receive mode", int, OtherBuffer->ReceiveTalkMode);
		CHECK(OurBuffer->GetReceiveTalkMode() == Case.Receive && Receive == Case.Receive, "factory receive mode");
		int Send = ORIGINAL_VALUE("factory send mode", int, Virtual<int(__fastcall *)(void *, void *)>(OtherBuffer, 0x50)(OtherBuffer, NULL));
		CHECK(OurBuffer->GetSendTalkMode() == Send, "factory send mode");
		golden::TText Addressee = ORIGINAL_TEXT("factory addressee",
			Virtual<const char *(__fastcall *)(void *, void *)>(OtherBuffer, 0x54)(OtherBuffer, NULL));
		CHECK(SameText(OurBuffer->GetAddressee(), Addressee.c_str()), "factory addressee");
		int Channel = ORIGINAL_VALUE("factory channel ID", int, Virtual<int(__fastcall *)(void *, void *)>(OtherBuffer, 0x58)(OtherBuffer, NULL));
		CHECK(OurBuffer->GetChannelID() == Channel, "factory channel ID");
		bool Duplicate = ORIGINAL_VALUE("duplicate factory", bool,
			At<CreateFn>(0x507bc0)(Case.Receive, Case.Send, Case.Name, Case.Channel) == NULL);
		CHECK((CreateTextBuffer(Case.Receive, Case.Send, Case.Name, Case.Channel) == nullptr) == Duplicate, "duplicate factory");
		int LookupChannel = Case.Receive == 6 ? -1 : Case.Channel;
		// The original's lookup as whether it found the buffer the original created.
		bool Found = ORIGINAL_VALUE("buffer lookup", bool,
			At<TTalkBuffer *(__cdecl *)(int, const char *, int)>(0x5025c0)(Case.Receive, Case.Name, LookupChannel) == OtherBuffer);
		CHECK(GetTextBuffer(Case.Receive, Case.Name, LookupChannel) == OurBuffer && Found, "buffer lookup");
		Ours.push_back(OurBuffer);
		Original.push_back(OtherBuffer);
	}
	for (size_t Index = 0; Index < Ours.size(); Index++) {
		TTalkBuffer *OurBuffer = Ours[Index];
		[[maybe_unused]] TTalkBuffer *OtherBuffer = Original[Index];
		for (int Offset : {0x0c, 0x28, 0x2c, 0x30, 0x34, 0x38}) {
			bool OurValue = false;
			switch (Offset) {
			case 0x0c: OurValue = OurBuffer->CanWrite(); break;
			case 0x28: OurValue = OurBuffer->isDefault(); break;
			case 0x2c: OurValue = OurBuffer->AcceptMessages(); break;
			case 0x30: OurValue = OurBuffer->IsRuleViolation(); break;
			case 0x34: OurValue = OurBuffer->IsRuleViolationChannel(); break;
			case 0x38: OurValue = OurBuffer->IsPrivateChannel(); break;
			}
			bool Value = ORIGINAL_VALUE("factory virtual", bool, Virtual<ThisBoolFn>(OtherBuffer, Offset)(OtherBuffer, NULL));
			CHECK(OurValue == Value, "factory virtual %x", Offset);
		}
		for (int Count = 0; Count < 15; Count++) {
			const char *Name = Count % 2 == 0 ? "Alice" : "Bob";
			OurBuffer->SetWriteEnabled(false);
			ORIGINAL(Virtual<ThisSetBoolFn>(OtherBuffer, 0x10)(OtherBuffer, NULL, false));
			uint8 OtherClosed = ORIGINAL_VALUE("closed after write disabling", uint8, Byte(OtherBuffer->Closed));
			CHECK(!OurBuffer->Closed && OtherClosed == 0, "write disabling is not Close");
			OurBuffer->Append(new TTalkLine(Count, 7, Name, "report", 0));
			ORIGINAL(Virtual<ThisLineFn>(OtherBuffer, 0x14)(OtherBuffer, NULL, OriginalLine(Count, 7, Name, "report", 0)));
			if (!OurBuffer->IsRuleViolationChannel()) {
				OurBuffer->AddText(19, "notice", 3);
				ORIGINAL(Virtual<ThisTextFn>(OtherBuffer, 0x18)(OtherBuffer, NULL, 19, "notice", 3));
			}
			CheckBuffer(OurBuffer, OtherBuffer);
			bool CanWrite = ORIGINAL_VALUE("write-enabled override", bool, Virtual<ThisBoolFn>(OtherBuffer, 0x0c)(OtherBuffer, NULL));
			CHECK(OurBuffer->CanWrite() == CanWrite, "write-enabled override");
		}
		OurBuffer->Remove(0);
		ORIGINAL(Virtual<ThisIntFn>(OtherBuffer, 0x1c)(OtherBuffer, NULL, 0));
		CheckBuffer(OurBuffer, OtherBuffer);
		OurBuffer->Remove(OurBuffer->last);
		ORIGINAL(Virtual<ThisLineFn>(OtherBuffer, 0x20)(OtherBuffer, NULL, OtherBuffer->last));
		CheckBuffer(OurBuffer, OtherBuffer);
		OurBuffer->Clear();
		ORIGINAL(Virtual<ThisFn>(OtherBuffer, 0x24)(OtherBuffer, NULL));
		CheckBuffer(OurBuffer, OtherBuffer);
	}
	for (int Round = 0; Round < 500; Round++) {
		int Mode = Random() % 27;
		int Channel = Round % 2 == 0 ? 2 : 8;
		const char *Name = Round % 2 == 0 ? "Alice" : "Bob";
		if (Round % 5 == 0)
			Name = nullptr;
		bool Enabled = (Random() & 1) != 0;
		SetEventMessages(Enabled);
		ORIGINAL(At<SetBoolFn>(0x4db580)(Enabled));
		SetStatusMessages(!Enabled);
		ORIGINAL(At<SetBoolFn>(0x4db590)(!Enabled));
		SetInfoMessages(Enabled);
		ORIGINAL(At<SetBoolFn>(0x4db570)(Enabled));
		AddConsoleText(Round, "routed text", Mode, Name, Channel, 100);
		ORIGINAL(At<void(__cdecl *)(uint32, const char *, int, const char *, int, int)>(0x5061f0)(Round, "routed text", Mode,
			Name, Channel, 100));
		AddChannelNotice("notice", "channel event", Mode, Name, Channel);
		ORIGINAL(At<void(__cdecl *)(const char *, const char *, int, const char *, int)>(0x506720)("notice", "channel event",
			Mode, Name, Channel));
		AddTextBufferLine(DefaultTextBuffer, Round, "direct", Mode, Name, 300);
		ORIGINAL(At<void(__cdecl *)(TTalkBuffer *, uint32, const char *, int, const char *, int)>(0x505f10)(OriginalDefault(),
			Round, "direct", Mode, Name, 300));
		bool Changed = ORIGINAL_VALUE("console changed after routing", bool, At<BoolFn>(0x4fd580)());
		CHECK(GetConsoleChanged() == Changed, "console changed after routing");
		for (size_t Index = 0; Index < Ours.size(); Index++) {
			CheckBuffer(Ours[Index], Original[Index]);
			bool Selected = Random() % 4 == 0;
			bool Locked = Random() % 4 == 0;
			Ours[Index]->SetSelected(Selected);
			ORIGINAL(Virtual<ThisSetBoolFn>(Original[Index], 0x40)(Original[Index], NULL, Selected));
			Ours[Index]->SetLocked(Locked);
			ORIGINAL(Virtual<ThisSetBoolFn>(Original[Index], 0x44)(Original[Index], NULL, Locked));
		}
		CheckBuffer(DefaultTextBuffer, OriginalDefault());
		bool Private = ORIGINAL_VALUE("own private channel", bool, At<BoolFn>(0x502880)());
		CHECK(HasOwnPrivateChannel() == Private, "own private channel");
		if (Round % 5 == 0) {
			ClearTextBuffer(Ours[0]);
			ORIGINAL(At<void(__cdecl *)(TTalkBuffer *)>(0x502340)(Original[0]));
			ClearDefaultTextBuffer();
			ORIGINAL(At<VoidFn>(0x5021a0)());
		}
	}
	for (size_t Index = 0; Index < Ours.size(); Index++) {
		DeleteTextBuffer(Ours[Index]);
		ORIGINAL(At<DeleteFn>(0x507fa0)(Original[Index]));
	}
	TTalkBuffer *RuleChannel = CreateTextBuffer(6, 1, nullptr, 4);
	TTalkBuffer *RuleChannelOriginal = ORIGINAL_OR(At<CreateFn>(0x507bc0)(6, 1, NULL, 4), nullptr);
	bool OriginalCreated = ORIGINAL_VALUE("rule-violation factory", bool, RuleChannelOriginal != NULL);
	CHECK(RuleChannel != nullptr && OriginalCreated, "rule-violation factory send mode 1");
	if (RuleChannel != nullptr && OriginalCreated) {
		RuleChannel->SetSelected(true);
		ORIGINAL(Virtual<ThisSetBoolFn>(RuleChannelOriginal, 0x40)(RuleChannelOriginal, NULL, true));
		bool Selected = ORIGINAL_VALUE("console selection slot", bool, RuleChannelOriginal->Selected);
		CHECK(RuleChannel->GetSelected() && Selected, "console selection slot");
		for (const char *Name : {"Alice", "Bob", "Carol"}) {
			AddTextBufferLine(RuleChannel, 1, "request", 6, Name, 0);
			ORIGINAL(At<void(__cdecl *)(TTalkBuffer *, uint32, const char *, int, const char *, int)>(0x505f10)(RuleChannelOriginal,
				1, "request", 6, Name, 0));
		}
		RuleChannel->cursor = RuleChannel->first;
		ORIGINAL(RuleChannelOriginal->cursor = RuleChannelOriginal->first);
		TTalkLine *OurStatement = RuleChannel->first;
		ORIGINAL(TTalkLine *OtherStatement = RuleChannelOriginal->first);
		bool OtherMore = ORIGINAL_VALUE("rule-violation statement", bool, OtherStatement != NULL);
		while (OurStatement != nullptr && OtherMore) {
			TTalkLine *OurCurrent = OurStatement;
			ORIGINAL(TTalkLine *OtherCurrent = OtherStatement);
			OurStatement = RuleChannel->GetNext();
			ORIGINAL(OtherStatement = At<NextFn>(0x503920)(RuleChannelOriginal, NULL));
			OtherMore = ORIGINAL_VALUE("rule-violation statement", bool, OtherStatement != NULL);
			golden::TText Addressee = ORIGINAL_TEXT("rule-violation statement view", OtherCurrent->Addressee, 30);
			CHECK(SameText(OurCurrent->Addressee, Addressee.c_str()), "rule-violation statement view");
			if (CompareStrings(OurCurrent->Addressee, "aLiCe", -1) == 0)
				RuleChannel->Remove(OurCurrent);
			ORIGINAL(if (CompareStrings(OtherCurrent->Addressee, "aLiCe", -1) == 0)
					Virtual<ThisLineFn>(RuleChannelOriginal, 0x20)(RuleChannelOriginal, NULL, OtherCurrent));
		}
		CHECK(OurStatement == nullptr && !OtherMore, "rule-violation cursor end");
		int OriginalLines = ORIGINAL_VALUE("rule-violation lines", int, RuleChannelOriginal->lines);
		CHECK(RuleChannel->lines == 2 && OriginalLines == 2, "rule-violation matching removal");
		CheckBuffer(RuleChannel, RuleChannelOriginal);
		DeleteTextBuffer(RuleChannel);
		ORIGINAL(At<DeleteFn>(0x507fa0)(RuleChannelOriginal));
	}
	DeleteTextBuffer(DefaultTextBuffer);
	ORIGINAL(At<DeleteFn>(0x507fa0)(OriginalDefault()));
	bool Private = ORIGINAL_VALUE("deleted console buffers", bool, At<BoolFn>(0x502880)());
	CHECK(!HasOwnPrivateChannel() && !Private, "deleted console buffers");
	ResetTalk();
	ORIGINAL(At<VoidFn>(0x504d70)());
	CheckBuffer(DefaultTextBuffer, OriginalDefault());
}

} // namespace

void TestTalk()
{
	int Before = Checks, BeforeFailures = Failures;
	PatchTalk();
	InitTalk();
	ORIGINAL(At<VoidFn>(0x504f30)());
	DefaultTextBuffer->Closed = false;
	ORIGINAL(OriginalDefault()->Closed = false);
	CheckBuffer(DefaultTextBuffer, OriginalDefault());
	TestHelpers();
	TestFilters();
	TestTalkboxes();
	TestLinesAndBuffers();
	TestRouting();
	ExitTalk();
	ORIGINAL(At<VoidFn>(0x507a00)());
	bool Exited = ORIGINAL_VALUE("ExitTalk", bool, OriginalDefault() == NULL);
	CHECK(DefaultTextBuffer == nullptr && Exited, "ExitTalk");
	UnpatchTalk();
	printf("Talk: %d checks, %d failures\n", Checks - Before, Failures - BeforeFailures);
}
