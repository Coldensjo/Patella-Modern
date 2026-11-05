// difftest.exe's Status test (moved here from tools/difftest/statustest.cpp, issue #26): Status.cpp
// against the original, as a golden-corpus test (golden.h): the oracle calls the original inside
// ORIGINAL and #if GOLDEN_ORACLE and records its results, the runner gets them from the corpus.
//
// In the oracle the image's malloc (0x512d18) and free (0x51165d) are redirected to ours (PatchImage),
// so operator new/delete and regcomp of the original work without its CRT. The functions that would
// reach Windows (timeGetTime, remove, the clipboard) are redirected to recorders here, and our side
// gets the same recorders as its definitions of those external functions (sysinc.h; in the runner
// sysinc.cpp's own are renamed, tests/golden/CMakeLists.txt). Failing assertions are not exercised:
// the original would need its exception machinery.
//
// The original's object state is recorded by value: its counters and texts at each check, a TObject
// as its 12 bytes, the error reports as their bytes (char arrays only), offsets as numbers. Where the
// original returns a pointer it was given (the mouse-over text, the messages' parameters), it is
// recorded as which of the test's own pointers it is, and the runner turns that back into the
// runner's pointer.

#include "golden_test.h"

#include "Objects.h"
#include "Status.h"

#include <string.h>

// --- Recorders shared by both sides --------------------------------------

namespace {

enum { OURS = 0,
	ORIGINAL_SIDE = 1 };
int Clock;
char Clipboard[2][4096];
int ClipboardCalls[2];
char Removed[2][64];
int RemoveCalls[2];
int LinkWarnings[2];

void Record(char *Buffer, size_t Size, const char *Text)
{
	strncpy(Buffer, Text, Size - 1);
	Buffer[Size - 1] = 0;
}

} // namespace

void SetTestClock(int Value) { Clock = Value; }

// The reconstructed side's external functions.
int GetMilliseconds() { return Clock; }
bool RemoveFile(const char *FileName)
{
	Record(Removed[OURS], 64, FileName);
	RemoveCalls[OURS]++;
	return true;
}
bool SetClipboardText(const char *textdata)
{
	Record(Clipboard[OURS], 4096, textdata);
	ClipboardCalls[OURS]++;
	return true;
}

#if GOLDEN_ORACLE
// Replacements patched into the image.
static void *__cdecl ImageMalloc(size_t Size) { return malloc(Size); }
static void __cdecl ImageFree(void *Block) { free(Block); }
static int __cdecl ImageGetMilliseconds() { return Clock; }
static bool __cdecl ImageRemoveFile(const char *FileName)
{
	Record(Removed[ORIGINAL_SIDE], 64, FileName);
	RemoveCalls[ORIGINAL_SIDE]++;
	return true;
}
static int __cdecl ImageSetClipboardText(char *textdata)
{
	Record(Clipboard[ORIGINAL_SIDE], 4096, textdata);
	ClipboardCalls[ORIGINAL_SIDE]++;
	return 1;
}
static void OriginalLinkWarning() { LinkWarnings[ORIGINAL_SIDE]++; }

void PatchImage()
{
	static bool Done = false;
	if (Done)
		return;
	Done = true;
	PatchJump(0x512d18, (void *)ImageMalloc);
	PatchJump(0x51165d, (void *)ImageFree);
	PatchJump(0x4fcb90, (void *)ImageGetMilliseconds);
	PatchJump(0x4fcbc0, (void *)ImageRemoveFile);
	PatchJump(0x4fd1b0, (void *)ImageSetClipboardText);
}
#endif
static void OurLinkWarning() { LinkWarnings[OURS]++; }

// --- Status.cpp ----------------------------------------------------------

#if GOLDEN_ORACLE
typedef void(__cdecl *VoidFn)();
typedef int(__cdecl *IntFn)();
typedef void(__cdecl *SetIntFn)(int);
typedef void(__cdecl *SetInt2Fn)(int, int);
typedef void(__cdecl *SetInt3Fn)(int, int, int);
typedef bool(__cdecl *BoolFn)();
typedef const char *(__cdecl *StrGetFn)();
typedef const char *(__cdecl *StrIndexFn)(int);
typedef void(__cdecl *StrSetFn)(const char *);
typedef void(__cdecl *ObjSetFn)(TObject, int, int, int, int);
typedef TObject *(__cdecl *ObjGetFn)(int *, int *, int *, int *);
typedef void(__cdecl *Get2Fn)(int *, int *);
typedef void(__cdecl *Get3Fn)(int *, int *, int *);
typedef void(__fastcall *VectorCtorFn)(void *This, void *, int Min, int Max, int Block);
#endif

namespace {

static_assert(sizeof(TObject) == 12, "TObject: three ints on both sides");

// The mouse-over texts the test sets; the original returns the pointer it was given.
const char *const MouseOverTexts[] = {"Scroll automap (Alt Cursor)", "", nullptr};
const char MouseOverOther = 0;

// The original's mouse-over text pointer as the index of the test's text it is (-1: none of them),
// and back.
const char *OriginalMouseOverText(const char *Name)
{
	int Index = -1;
#if GOLDEN_ORACLE
	const char *Pointer = At<StrGetFn>(0x4e7310)();
	for (int i = 0; i < 3; i++)
		if (Pointer == MouseOverTexts[i])
			Index = i;
#endif
	golden::Output(Name, Index);
	return Index >= 0 ? MouseOverTexts[Index] : &MouseOverOther;
}

// The original's TObject, by value.
TObject OriginalObject(const char *Name, [[maybe_unused]] const TObject *Original)
{
	TObject Object;
	ORIGINAL(Object = *Original);
	golden::OutputBytes(Name, &Object, sizeof(Object));
	return Object;
}

// Simple setter/getter pairs: original setter, original getter, ours.
struct TIntPair {
	uint32 Set, Get;
	void (*OurSet)(int);
	int (*OurGet)();
	const char *Name;
};

void TestAccessors()
{
	static const TIntPair Pairs[] = {
		{0x4e6e20, 0x4e6e30, SetActionState, GetActionState, "ActionState"},
		{0x4e7140, 0x4e71d0, SetTradeInfo, GetTradeInfo, "TradeInfo"},
		{0x4e71e0, 0x4e71f0, SetConnectionState, GetConnectionState, "ConnectionState"},
		{0x4e7200, 0x4e7210, SetProgress, GetProgress, "Progress"},
		{0x4e7290, 0x4e72b0, SetPremiumDays, GetPremiumDays, "PremiumDays"},
	};
	for (int t = 0; t < 2000; t++) {
		const TIntPair &P = Pairs[Random() % (sizeof(Pairs) / sizeof(Pairs[0]))];
		int Value = (int)(Random() ^ (Random() << 16));
		ORIGINAL(At<SetIntFn>(P.Set)(Value));
		P.OurSet(Value);
		int Got = ORIGINAL_VALUE("get", int, At<IntFn>(P.Get)());
		CHECK(P.OurGet() == Got, "%s", P.Name);

		int a = (int)Random() - (1 << 23), b = (int)Random() - (1 << 23), c = (int)Random() % 16;
		int a1, b1, c1, a2 = 0, b2 = 0, c2 = 0;
		switch (Random() % 6) {
		case 0: {
			ORIGINAL(At<SetInt2Fn>(0x4e6e40)(a, b));
			SetMousePosition(a, b);
			int X = ORIGINAL_VALUE("mouse x", int, At<IntFn>(0x4e6e60)());
			int Y = ORIGINAL_VALUE("mouse y", int, At<IntFn>(0x4e6e70)());
			CHECK(GetMouseX() == X && GetMouseY() == Y, "mouse position");
			break;
		}
		case 1: {
			ORIGINAL(At<SetInt2Fn>(0x4e6ec0)(a, b));
			SetPendingAction(a, b);
			int State = ORIGINAL_VALUE("pending state", int, At<IntFn>(0x4e6ee0)());
			int Number = ORIGINAL_VALUE("pending number", int, At<IntFn>(0x4e6ef0)());
			CHECK(GetPendingActionState() == State && GetPendingActionNumber() == Number, "pending action");
			break;
		}
		case 2: {
			ORIGINAL(At<SetInt2Fn>(0x4e6f00)(a, b));
			SetWindowSize(a, b);
			int Width = ORIGINAL_VALUE("window width", int, At<IntFn>(0x4e6f20)());
			int Height = ORIGINAL_VALUE("window height", int, At<IntFn>(0x4e6f30)());
			CHECK(GetWindowWidth() == Width && GetWindowHeight() == Height, "window size");
			break;
		}
		case 3: {
			ORIGINAL(At<SetInt3Fn>(0x4e6fc0)(a, b, c));
			SetDragTarget(a, b, c);
			GetDragTarget(&a1, &b1, &c1);
			ORIGINAL(At<Get3Fn>(0x4e7070)(&a2, &b2, &c2));
			int Target[3] = {a2, b2, c2};
			golden::Output("drag target", Target);
			CHECK(a1 == Target[0] && b1 == Target[1] && c1 == Target[2], "drag target");
			break;
		}
		case 4: {
			const char *Text = MouseOverTexts[Random() % 3];
			ORIGINAL(At<StrSetFn>(0x4e72e0)(Text));
			SetMouseOverText(Text);
			ORIGINAL(At<SetInt2Fn>(0x4e72f0)(a, b));
			SetMouseOverTextPosition(a, b);
			GetMouseOverTextPosition(&a1, &b1);
			ORIGINAL(At<Get2Fn>(0x4e7320)(&a2, &b2));
			int Position[2] = {a2, b2};
			const char *OriginalText = OriginalMouseOverText("mouse-over text");
			golden::Output("mouse-over position", Position);
			CHECK(GetMouseOverText() == OriginalText && a1 == Position[0] && b1 == Position[1], "mouse-over text");
			break;
		}
		case 5: {
			ORIGINAL(At<VoidFn>(0x4e6e10)());
			IncrementActionNumber();
			int Number = ORIGINAL_VALUE("action number", int, At<IntFn>(0x4e6e00)());
			CHECK(GetActionNumber() == Number, "action number");
			break;
		}
		}
	}
}

struct TSlot {
	uint32 Set, Get;
	void (*OurSet)(TObject, int, int, int, int);
	TObject *(*OurGet)(int *, int *, int *, int *);
};

void TestMouse()
{
	for (int t = 0; t < 5000; t++) {
		int State = (int)(Random() % 12);
		int Cursor = (int)(Random() % 8);
		ORIGINAL(At<SetIntFn>(0x4e6e20)(State));
		SetActionState(State);
		ORIGINAL(At<SetIntFn>(0x4e6e80)(Cursor));
		SetMouseCursor(Cursor);
		int OriginalCursor = ORIGINAL_VALUE("cursor", int, At<IntFn>(0x4e6e90)());
		CHECK(GetMouseCursor() == OriginalCursor, "GetMouseCursor in state %d", State);

		int x = (int)(Random() % 100), y = (int)(Random() % 100);
		ORIGINAL(At<SetInt2Fn>(0x4e6f40)(x, y));
		SetDragStart(x, y);
		TObject Obj = {(int)(Random() % 3), (int)Random(), (int)Random()};
		int Coordinates[4] = {(int)Random(), (int)Random(), (int)Random(), (int)Random()};
		if (Random() % 2) {
			ORIGINAL(At<ObjSetFn>(0x4e6f70)(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]));
			SetDragObject(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]);
		}
		int x2 = x + (int)(Random() % 11) - 5, y2 = y + (int)(Random() % 11) - 5;
		ORIGINAL(At<SetInt2Fn>(0x4e6fe0)(x2, y2));
		CheckDragStart(x2, y2);
		int OriginalState = ORIGINAL_VALUE("action state", int, At<IntFn>(0x4e6e30)());
		CHECK(GetActionState() == OriginalState, "CheckDragStart(%d,%d) from %d,%d", x2, y2, x, y);

		// The three object slots.
		TSlot Slots[] = {
			{0x4e6f70, 0x4e7030, SetDragObject, GetDragObject},
			{0x4e70a0, 0x4e7150, SetTradeObject, GetTradeObject},
			{0x4e70f0, 0x4e7190, SetUseObject, GetUseObject},
		};
		auto &S = Slots[Random() % 3];
		ORIGINAL(At<ObjSetFn>(S.Set)(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]));
		S.OurSet(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]);
		int A[4], B[4] = {};
		TObject *Ours = S.OurGet(&A[0], &A[1], &A[2], &A[3]);
		const TObject *Got = nullptr;
		ORIGINAL(Got = At<ObjGetFn>(S.Get)(&B[0], &B[1], &B[2], &B[3]));
		golden::Output("slot coordinates", B);
		TObject OriginalSlot = OriginalObject("slot object", Got);
		const TObject *Original = &OriginalSlot;
		CHECK(memcmp(A, B, sizeof(A)) == 0 && memcmp(Ours, Original, sizeof(TObject)) == 0, "object slot");
	}
}

void TestRights()
{
	for (int t = 0; t < 20000; t++) {
		int Id = (int)(Random() % RIGHT_ANY);
		switch (Random() % 8) {
		case 0: {
			int Value = (Random() % 3 == 0) ? 0 : (int)(Random() ^ (Random() << 16));
			ORIGINAL(At<SetInt2Fn>(0x4e8580)(Id, Value));
			SetRight(Id, Value);
			break;
		}
		case 1:
			if (Random() % 20 == 0) {
				ORIGINAL(At<VoidFn>(0x4e6de0)());
				ClearRights();
			}
			break;
		default: {
			int Bit = (int)(Random() % 33) - 1;
			bool Ours = CheckRight(Id, Bit);
			bool Original = ORIGINAL_VALUE("CheckRight", bool, At<bool(__cdecl *)(int, int)>(0x4e86e0)(Id, Bit));
			CHECK(Ours == Original, "CheckRight(%d,%d)", Id, Bit);
			break;
		}
		}
		bool Any = ORIGINAL_VALUE("CheckAnyRight", bool, At<BoolFn>(0x4e6dc0)());
		CHECK(CheckAnyRight() == Any, "CheckAnyRight");
	}
	for (int i = 0; i < 32; i++) {
		golden::TText Name = ORIGINAL_TEXT("right name", At<StrIndexFn>(0x4e88c0)(i));
		CHECK(strcmp(GetRightName(i), Name.c_str()) == 0, "GetRightName(%d)", i);
	}
	for (int i = 0; i < 8; i++) {
		golden::TText Name = ORIGINAL_TEXT("action name", At<StrIndexFn>(0x4e8de0)(i));
		CHECK(strcmp(GetActionName(i), Name.c_str()) == 0, "GetActionName(%d)", i);
	}
}

// Messages own some of their parameters (see ClearMessages); each side gets
// its own blocks, allocated the way that side frees them.
bool OwnsParam1(int Type) { return Type == 5 || Type == 6 || Type == 7 || Type == 14 || Type == 15 || Type == 16 || Type == 18 || Type == 25 || Type == 26 || Type == 28 || Type == 51; }
bool OwnsParam2(int Type) { return Type == 1 || Type == 2 || (Type >= 47 && Type <= 50) || Type == 14 || Type == 15 || Type == 18 || Type == 28; }

INT_PTR NewBlock(int Side)
{
	return Side == OURS ? (INT_PTR) new char[8] : (INT_PTR)malloc(8);
}

void MakeParams(int Type, int Side, INT_PTR Shared1, INT_PTR Shared2, INT_PTR *P1, INT_PTR *P2)
{
	*P1 = Shared1;
	*P2 = Shared2;
	bool Null = (Shared1 & 1) != 0;
	if (OwnsParam1(Type))
		*P1 = (Type == 28 && Null) ? 0 : NewBlock(Side);
	if (OwnsParam2(Type)) {
		if ((Type == 18 || Type == 28) && (Shared2 & 1))
			*P2 = 0;
		else if (Type == 14) {
			char **Pair = Side == OURS ? (char **)new char[2 * sizeof(char *)] : (char **)malloc(2 * sizeof(char *));
			Pair[0] = (char *)NewBlock(Side);
			Pair[1] = (char *)NewBlock(Side);
			*P2 = (INT_PTR)Pair;
		} else
			*P2 = NewBlock(Side);
	}
}

void FreeParams(int Type, INT_PTR P1, INT_PTR P2)
{
	// Both sides' blocks come from this process's heap.
	if (OwnsParam1(Type) && P1 != 0)
		free((void *)P1);
	if (OwnsParam2(Type) && P2 != 0) {
		if (Type == 14) {
			free(((void **)P2)[0]);
			free(((void **)P2)[1]);
		}
		free((void *)P2);
	}
}

struct TModel {
	int Type;
	INT_PTR P[2][2];
};

// A parameter the original's FetchMessage returned, by value: 1 or 2 when it is the first or second
// parameter the test gave the original for the message at the head of the model, else 0 and the
// number itself (a parameter the message does not own, or 0).
INT_PTR OriginalParam(const char *Name, [[maybe_unused]] INT_PTR Value, const TModel *Head)
{
	int Kind[2] = {0, 0};
#if GOLDEN_ORACLE
	if (Head != nullptr && Value == Head->P[ORIGINAL_SIDE][0])
		Kind[0] = 1;
	else if (Head != nullptr && Value == Head->P[ORIGINAL_SIDE][1])
		Kind[0] = 2;
	else
		Kind[1] = (int)Value;
#endif
	golden::Output(Name, Kind);
	if (Kind[0] != 0)
		return Head->P[ORIGINAL_SIDE][Kind[0] - 1];
	return (INT_PTR)Kind[1];
}

#if !GOLDEN_ORACLE
// The runner: the blocks the original would have freed (ClearMessages).
void FreeOriginalParams(TModel *Model, int Count)
{
	for (int i = 0; i < Count; i++)
		FreeParams(Model[i].Type, Model[i].P[ORIGINAL_SIDE][0], Model[i].P[ORIGINAL_SIDE][1]);
}
#endif

void TestMessages()
{
	TModel Model[64];
	int Count = 0;
	for (int t = 0; t < 20000; t++) {
		int Op = (int)(Random() % 10);
		if (Op < 4 && Count < 60) {
			int Type = (int)(Random() % (MESSAGE_STARTPATCHER + 1));
			INT_PTR Shared1 = (INT_PTR)Random(), Shared2 = (INT_PTR)Random();
			TModel M;
			M.Type = Type;
			MakeParams(Type, OURS, Shared1, Shared2, &M.P[OURS][0], &M.P[OURS][1]);
			MakeParams(Type, ORIGINAL_SIDE, Shared1, Shared2, &M.P[ORIGINAL_SIDE][0], &M.P[ORIGINAL_SIDE][1]);
#if GOLDEN_ORACLE
			typedef void(__cdecl * AddFn)(int, INT_PTR, INT_PTR);
#endif
			if (Op < 3) {
				AddMessage(Type, M.P[OURS][0], M.P[OURS][1]);
				ORIGINAL(At<AddFn>(0x4e9020)(Type, M.P[ORIGINAL_SIDE][0], M.P[ORIGINAL_SIDE][1]));
				Model[Count++] = M;
			} else {
				AddMessageFirst(Type, M.P[OURS][0], M.P[OURS][1]);
				ORIGINAL(At<AddFn>(0x4e91d0)(Type, M.P[ORIGINAL_SIDE][0], M.P[ORIGINAL_SIDE][1]));
				memmove(&Model[1], &Model[0], Count * sizeof(TModel));
				Model[0] = M;
				Count++;
			}
		} else if (Op < 8) {
			int Type[2] = {0, 0};
			INT_PTR P1[2] = {0, 0}, P2[2] = {0, 0};
			FetchMessage(&Type[OURS], &P1[OURS], &P2[OURS]);
			ORIGINAL(At<void(__cdecl *)(int *, INT_PTR *, INT_PTR *)>(0x4e75f0)(&Type[ORIGINAL_SIDE], &P1[ORIGINAL_SIDE],
				&P2[ORIGINAL_SIDE]));
			golden::Output("fetched type", Type[ORIGINAL_SIDE]);
			const TModel *Head = Count == 0 ? nullptr : &Model[0];
			P1[ORIGINAL_SIDE] = OriginalParam("fetched 1", P1[ORIGINAL_SIDE], Head);
			P2[ORIGINAL_SIDE] = OriginalParam("fetched 2", P2[ORIGINAL_SIDE], Head);
			if (Count == 0) {
				CHECK(Type[0] == 0 && P1[0] == 0 && P2[0] == 0 && Type[1] == 0 && P1[1] == 0 && P2[1] == 0,
					"FetchMessage on an empty queue");
			} else {
				TModel &M = Model[0];
				CHECK(Type[OURS] == M.Type && P1[OURS] == M.P[OURS][0] && P2[OURS] == M.P[OURS][1], "FetchMessage");
				CHECK(Type[ORIGINAL_SIDE] == M.Type && P1[ORIGINAL_SIDE] == M.P[ORIGINAL_SIDE][0]
						&& P2[ORIGINAL_SIDE] == M.P[ORIGINAL_SIDE][1],
					"original FetchMessage");
				FreeParams(M.Type, P1[OURS], P2[OURS]);
				FreeParams(M.Type, P1[ORIGINAL_SIDE], P2[ORIGINAL_SIDE]);
				memmove(&Model[0], &Model[1], (Count - 1) * sizeof(TModel));
				Count--;
			}
		} else if (Op == 8 && Random() % 10 == 0) {
			ClearMessages();
			ORIGINAL(At<VoidFn>(0x4e76e0)());
#if !GOLDEN_ORACLE
			FreeOriginalParams(Model, Count);
#endif
			Count = 0;
		}
		int Peeked = ORIGINAL_VALUE("PeekMessageType", int, At<IntFn>(0x4e75d0)());
		CHECK(PeekMessageType() == Peeked, "PeekMessageType");
		int Number = ORIGINAL_VALUE("NumberOfMessages", int, Global<int>(0x71c5f0));
		CHECK(Number == Count, "original NumberOfMessages %d, expected %d", Number, Count);
	}
	ClearMessages();
	ORIGINAL(At<VoidFn>(0x4e76e0)());
#if !GOLDEN_ORACLE
	FreeOriginalParams(Model, Count);
#endif
}

void TestAccount()
{
	for (int t = 0; t < 2000; t++) {
		char Password[40];
		RandomText(Password, (int)(Random() % 30));
		int Number = (int)(Random() ^ (Random() << 16));
		if (t % 5 == 0)
			Number = -Number;
		SetAccount(Number, Password);
		ORIGINAL(At<void(__cdecl *)(int, const char *)>(0x4e93d0)(Number, Password));
		int OriginalNumber = ORIGINAL_VALUE("account number", int, At<IntFn>(0x4e7220)());
		CHECK(GetAccountNumber() == OriginalNumber, "GetAccountNumber");
		golden::TText NumberString = ORIGINAL_TEXT("account number string", At<StrGetFn>(0x4e7230)());
		CHECK(strcmp(GetAccountNumberString(), NumberString.c_str()) == 0, "GetAccountNumberString %s, original %s",
			GetAccountNumberString(), NumberString.c_str());
		char OriginalPassword[30] = {};
		ORIGINAL(memcpy(OriginalPassword, At<StrGetFn>(0x4e7240)(), 30));
		golden::OutputBytes("account password", OriginalPassword, 30);
		CHECK(memcmp(GetAccountPassword(), OriginalPassword, 30) == 0, "GetAccountPassword");

		int Characters = (int)(Random() % 8);
		SetAccountCharacters(Characters);
		ORIGINAL(At<SetIntFn>(0x4e7250)(Characters));
		int OriginalCharacters = ORIGINAL_VALUE("account characters", int, At<IntFn>(0x4e72a0)());
		CHECK(GetAccountCharacters() == OriginalCharacters, "GetAccountCharacters");
		for (int i = 0; i < Characters; i++) {
			char Name[40], World[40];
			RandomText(Name, (int)(Random() % 30));
			RandomText(World, (int)(Random() % 30));
			uint8 Address[4] = {(uint8)Random(), (uint8)Random(), (uint8)Random(), (uint8)Random()};
			int Port = (int)(Random() % 70000);
			SetCharacter(i, Name, World, Address, Port);
			ORIGINAL(At<void(__cdecl *)(int, const char *, const char *, const uint8 *, int)>(0x4e96d0)(i, Name, World,
				Address, Port));
		}
		for (int i = 0; i < Characters; i++) {
			golden::TText Name = ORIGINAL_TEXT("character name", At<StrIndexFn>(0x4e9db0)(i));
			CHECK(strcmp(GetCharacterName(i), Name.c_str()) == 0, "GetCharacterName(%d)", i);
			golden::TText World = ORIGINAL_TEXT("character world", At<StrIndexFn>(0x4e9f20)(i));
			CHECK(strcmp(GetCharacterWorld(i), World.c_str()) == 0, "GetCharacterWorld(%d)", i);
			golden::TText Address = ORIGINAL_TEXT("character address", At<StrIndexFn>(0x4ea090)(i));
			CHECK(strcmp(GetCharacterIPAddress(i), Address.c_str()) == 0, "GetCharacterIPAddress(%d)", i);
			int Port = ORIGINAL_VALUE("character port", int, At<int(__cdecl *)(int)>(0x4ea200)(i));
			CHECK(GetCharacterPort(i) == Port, "GetCharacterPort(%d)", i);
			// The raw IP field at +0x3c.
			uint32 IP = ORIGINAL_VALUE("character IP", uint32, *(const uint32 *)(At<StrIndexFn>(0x4e9db0)(i) + 0x3c));
			CHECK(*(const uint32 *)(GetCharacterName(i) + 0x3c) == IP, "IP");
		}
		if (Characters > 0) {
			int Active = (int)(Random() % Characters);
			SetActiveCharacter(Active);
			ORIGINAL(At<SetIntFn>(0x4e9c50)(Active));
			int OriginalActive = ORIGINAL_VALUE("active character", int, At<IntFn>(0x4e72c0)());
			CHECK(GetActiveCharacter() == OriginalActive, "GetActiveCharacter");
		}
	}
}

// The error report list (AddErrorReport and the rest) is gone (Patella-Modern issue #74, decision D2), and its
// comparisons with it. Its rounds' random numbers are still drawn, as they were, so that the comparisons after them
// keep their inputs and their counts (the shared Random() runs on into the later tests).
void SkipErrorReports()
{
	for (int t = 0; t < 3000; t++) {
		char Text[400];
		RandomText(Text, (int)(Random() % (t % 3 == 0 ? 300 : 80)));
		if (Random() % 8 == 5)
			Random(); // ClearErrorReports' one in ten
	}
}

// The original's recorders at a check.
void OriginalClipboard()
{
	golden::Output("link warnings", LinkWarnings[ORIGINAL_SIDE]);
	golden::Output("clipboard calls", ClipboardCalls[ORIGINAL_SIDE]);
	std::string Text = golden::OutputText("clipboard", Clipboard[ORIGINAL_SIDE]);
	Record(Clipboard[ORIGINAL_SIDE], sizeof(Clipboard[ORIGINAL_SIDE]), Text.c_str());
}

void TestClipboard()
{
	static const char *Samples[] = {
		"hello world",
		"visit www.tibia.com now",
		"tibia.com",
		"go to 192.168.0.1:7171",
		"a.b",
		"x.de.",
		"mail me@example.org",
		"WWW.EXAMPLE.COM",
		"foo%2ebar%2ecom",
		"1.2.3",
		"text.museum!",
		"just text, no link.",
		"",
		"end with link.net",
	};
	for (int t = 0; t < 3000; t++) {
		char Text[200];
		if (Random() % 2) {
			strcpy(Text, Samples[Random() % (sizeof(Samples) / sizeof(Samples[0]))]);
		} else {
			RandomText(Text, (int)(Random() % 40));
			if (Random() % 3 == 0)
				strcat(Text, Samples[Random() % (sizeof(Samples) / sizeof(Samples[0]))]);
		}
		CopyToClipboard(Text);
		ORIGINAL(At<StrSetFn>(0x4ea5b0)(Text));
		OriginalClipboard();
		CHECK(LinkWarnings[OURS] == LinkWarnings[ORIGINAL_SIDE] && ClipboardCalls[OURS] == ClipboardCalls[ORIGINAL_SIDE]
				&& strcmp(Clipboard[OURS], Clipboard[ORIGINAL_SIDE]) == 0,
			"CopyToClipboard(\"%s\")", Text);
		if (Random() % 4 == 0) {
			CopyTaintedClipboardText();
			ORIGINAL(At<VoidFn>(0x4ea710)());
			OriginalClipboard();
			CHECK(strcmp(Clipboard[OURS], Clipboard[ORIGINAL_SIDE]) == 0, "CopyTaintedClipboardText");
		}
	}
	CHECK(LinkWarnings[OURS] > 0, "no link warnings");
}

void TestBlinkAndFiles()
{
	for (int t = 0; t < 5000; t++) {
		Clock += (int)(Random() % 200);
		UpdateBlinkState();
		ORIGINAL(At<VoidFn>(0x4e7340)());
		bool Blink = ORIGINAL_VALUE("blink state", bool, At<BoolFn>(0x4e7370)());
		CHECK(GetBlinkState() == Blink, "GetBlinkState at %d", Clock);
	}
	// DeleteErrorFile's comparison went with it (issue #74).
}

void CheckInitialState(const char *When)
{
	int Values[12] = {};
#if GOLDEN_ORACLE
	Values[0] = At<IntFn>(0x4e6e30)();
	Values[1] = At<IntFn>(0x4e71f0)();
	Values[2] = At<IntFn>(0x4e7210)();
	Values[3] = At<IntFn>(0x4e7220)();
	Values[4] = At<IntFn>(0x4e72a0)();
	Values[5] = At<IntFn>(0x4e72b0)();
	Values[6] = At<IntFn>(0x4e72c0)();
	Values[7] = At<BoolFn>(0x4e72d0)();
	Values[8] = At<IntFn>(0x4e71d0)();
	Values[9] = At<IntFn>(0x4e6ee0)();
	Values[10] = At<IntFn>(0x4e6ef0)();
	Values[11] = At<IntFn>(0x4e75d0)();
#endif
	golden::Output("initial state", Values);
	const char *MouseOverText = OriginalMouseOverText("initial mouse-over text");
	bool Any = ORIGINAL_VALUE("initial CheckAnyRight", bool, At<BoolFn>(0x4e6dc0)());
	golden::TText Password = ORIGINAL_TEXT("initial password", At<StrGetFn>(0x4e7240)());
	CHECK(GetActionState() == Values[0] && GetConnectionState() == Values[1] && GetProgress() == Values[2]
			&& GetAccountNumber() == Values[3] && GetAccountCharacters() == Values[4] && GetPremiumDays() == Values[5]
			&& GetActiveCharacter() == Values[6] && GetGamemaster() == (Values[7] != 0)
			&& GetMouseOverText() == MouseOverText && GetTradeInfo() == Values[8]
			&& GetPendingActionState() == Values[9] && GetPendingActionNumber() == Values[10]
			&& PeekMessageType() == Values[11] && CheckAnyRight() == Any
			&& strcmp(GetAccountPassword(), Password.c_str()) == 0,
		"state after %s", When);
}

} // namespace

void TestStatus()
{
	ORIGINAL(PatchImage());
	// The original's static initialisers (0x55a0f0, 0x55a110) did not run. The error report vector (0x71c648) is
	// the original's only (issue #74): its ExitStatus clears it.
	ORIGINAL(At<VectorCtorFn>(0x4e7f50)((void *)(uintptr_t)0x71c5f4, 0, 0, 9, 10));
	ORIGINAL(At<VectorCtorFn>(0x4e8230)((void *)(uintptr_t)0x71c648, 0, 0, 0, 1));

	for (int Round = 0; Round < 2; Round++) {
		bool Gamemaster = Round == 1;
		InitStatus(Gamemaster, OurLinkWarning);
		ORIGINAL(At<void(__cdecl *)(bool, void (*)())>(0x4ea860)(Gamemaster, OriginalLinkWarning));
		CheckInitialState("InitStatus");
		TestAccessors();
		TestMouse();
		TestRights();
		TestMessages();
		TestAccount();
		SkipErrorReports();
		TestClipboard();
		TestBlinkAndFiles();
		ExitStatus();
		ORIGINAL(At<VoidFn>(0x4e7f00)());
	}
}
