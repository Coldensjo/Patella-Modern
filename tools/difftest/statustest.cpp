// Differential tests of Skins.cpp and Status.cpp against the original code.
//
// The image's malloc (0x512d18) and free (0x51165d) are redirected to ours,
// so operator new/delete and regcomp of the original work without its CRT.
// Our side links src/Extern/regexp/regexp.c (regexptest.cpp compares it).
// The functions that would reach Windows (timeGetTime, remove, the
// clipboard) are redirected to recorders here, and the reconstructed side
// gets the same recorders as its definitions of those external functions.
// Failing assertions are not exercised: the original would need its
// exception machinery.

#include "difftest.h"
#include "../../src/Skins.h"
#include "../../src/Status.h"

// --- Recorders shared by both sides --------------------------------------

enum { OURS = 0, ORIGINAL = 1 };
static int Clock;
void SetTestClock(int Value) { Clock = Value; }
static char Clipboard[2][4096];
static int ClipboardCalls[2];
static char Removed[2][64];
static int RemoveCalls[2];
static int LinkWarnings[2];

static void Record(char *Buffer, size_t Size, const char *Text)
{
	strncpy(Buffer, Text, Size - 1);
	Buffer[Size - 1] = 0;
}

// The reconstructed side's external functions.
int GetMilliseconds() { return Clock; }
bool RemoveFile(const char *FileName) { Record(Removed[OURS], 64, FileName); RemoveCalls[OURS]++; return true; }
bool SetClipboardText(const char *textdata) { Record(Clipboard[OURS], 4096, textdata); ClipboardCalls[OURS]++; return true; }

// Replacements patched into the image.
static void *__cdecl ImageMalloc(size_t Size) { return malloc(Size); }
static void __cdecl ImageFree(void *Block) { free(Block); }
static int __cdecl ImageGetMilliseconds() { return Clock; }
static bool __cdecl ImageRemoveFile(const char *FileName)
{
	Record(Removed[ORIGINAL], 64, FileName);
	RemoveCalls[ORIGINAL]++;
	return true;
}
static int __cdecl ImageSetClipboardText(char *textdata)
{
	Record(Clipboard[ORIGINAL], 4096, textdata);
	ClipboardCalls[ORIGINAL]++;
	return 1;
}
static void OurLinkWarning() { LinkWarnings[OURS]++; }
static void OriginalLinkWarning() { LinkWarnings[ORIGINAL]++; }

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

// --- Skins.cpp -----------------------------------------------------------

// TestSkins is a golden-corpus test (tests/golden/core_tests.cpp, issue #24).

// --- Status.cpp ----------------------------------------------------------

typedef void (__cdecl *VoidFn)();
typedef int (__cdecl *IntFn)();
typedef void (__cdecl *SetIntFn)(int);
typedef void (__cdecl *SetInt2Fn)(int, int);
typedef void (__cdecl *SetInt3Fn)(int, int, int);
typedef bool (__cdecl *BoolFn)();
typedef const char *(__cdecl *StrGetFn)();
typedef const char *(__cdecl *StrIndexFn)(int);
typedef void (__cdecl *StrSetFn)(const char *);
typedef void (__cdecl *ObjSetFn)(TObject, int, int, int, int);
typedef TObject *(__cdecl *ObjGetFn)(int *, int *, int *, int *);
typedef void (__cdecl *Get2Fn)(int *, int *);
typedef void (__cdecl *Get3Fn)(int *, int *, int *);
typedef void (__fastcall *VectorCtorFn)(void *This, void *, int Min, int Max, int Block);

// Simple setter/getter pairs: original setter, original getter, ours.
struct TIntPair {
	uint32 Set, Get;
	void (*OurSet)(int);
	int (*OurGet)();
	const char *Name;
};

static void TestAccessors()
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
		At<SetIntFn>(P.Set)(Value);
		P.OurSet(Value);
		CHECK(P.OurGet() == At<IntFn>(P.Get)(), "%s", P.Name);

		int a = (int)Random() - (1 << 23), b = (int)Random() - (1 << 23), c = (int)Random() % 16;
		int a1, b1, c1, a2, b2, c2;
		switch (Random() % 6) {
		case 0:
			At<SetInt2Fn>(0x4e6e40)(a, b);
			SetMousePosition(a, b);
			CHECK(GetMouseX() == At<IntFn>(0x4e6e60)() && GetMouseY() == At<IntFn>(0x4e6e70)(), "mouse position");
			break;
		case 1:
			At<SetInt2Fn>(0x4e6ec0)(a, b);
			SetPendingAction(a, b);
			CHECK(GetPendingActionState() == At<IntFn>(0x4e6ee0)()
				&& GetPendingActionNumber() == At<IntFn>(0x4e6ef0)(), "pending action");
			break;
		case 2:
			At<SetInt2Fn>(0x4e6f00)(a, b);
			SetWindowSize(a, b);
			CHECK(GetWindowWidth() == At<IntFn>(0x4e6f20)() && GetWindowHeight() == At<IntFn>(0x4e6f30)(), "window size");
			break;
		case 3:
			At<SetInt3Fn>(0x4e6fc0)(a, b, c);
			SetDragTarget(a, b, c);
			GetDragTarget(&a1, &b1, &c1);
			At<Get3Fn>(0x4e7070)(&a2, &b2, &c2);
			CHECK(a1 == a2 && b1 == b2 && c1 == c2, "drag target");
			break;
		case 4: {
			static const char *Texts[] = {"Scroll automap (Alt Cursor)", "", NULL};
			const char *Text = Texts[Random() % 3];
			At<StrSetFn>(0x4e72e0)(Text);
			SetMouseOverText(Text);
			At<SetInt2Fn>(0x4e72f0)(a, b);
			SetMouseOverTextPosition(a, b);
			GetMouseOverTextPosition(&a1, &b1);
			At<Get2Fn>(0x4e7320)(&a2, &b2);
			CHECK(GetMouseOverText() == At<StrGetFn>(0x4e7310)() && a1 == a2 && b1 == b2, "mouse-over text");
			break;
		}
		case 5:
			At<VoidFn>(0x4e6e10)();
			IncrementActionNumber();
			CHECK(GetActionNumber() == At<IntFn>(0x4e6e00)(), "action number");
			break;
		}
	}
}

static void TestMouse()
{
	for (int t = 0; t < 5000; t++) {
		int State = (int)(Random() % 12);
		int Cursor = (int)(Random() % 8);
		At<SetIntFn>(0x4e6e20)(State);
		SetActionState(State);
		At<SetIntFn>(0x4e6e80)(Cursor);
		SetMouseCursor(Cursor);
		CHECK(GetMouseCursor() == At<IntFn>(0x4e6e90)(), "GetMouseCursor in state %d", State);

		int x = (int)(Random() % 100), y = (int)(Random() % 100);
		At<SetInt2Fn>(0x4e6f40)(x, y);
		SetDragStart(x, y);
		TObject Obj = {(int)(Random() % 3), (int)Random(), (int)Random()};
		int Coordinates[4] = {(int)Random(), (int)Random(), (int)Random(), (int)Random()};
		if (Random() % 2) {
			At<ObjSetFn>(0x4e6f70)(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]);
			SetDragObject(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]);
		}
		int x2 = x + (int)(Random() % 11) - 5, y2 = y + (int)(Random() % 11) - 5;
		At<SetInt2Fn>(0x4e6fe0)(x2, y2);
		CheckDragStart(x2, y2);
		CHECK(GetActionState() == At<IntFn>(0x4e6e30)(), "CheckDragStart(%d,%d) from %d,%d", x2, y2, x, y);

		// The three object slots.
		struct { uint32 Set, Get; ObjSetFn OurSet; ObjGetFn OurGet; } Slots[] = {
			{0x4e6f70, 0x4e7030, SetDragObject, GetDragObject},
			{0x4e70a0, 0x4e7150, SetTradeObject, GetTradeObject},
			{0x4e70f0, 0x4e7190, SetUseObject, GetUseObject},
		};
		auto &S = Slots[Random() % 3];
		At<ObjSetFn>(S.Set)(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]);
		S.OurSet(Obj, Coordinates[0], Coordinates[1], Coordinates[2], Coordinates[3]);
		int A[4], B[4];
		TObject *Ours = S.OurGet(&A[0], &A[1], &A[2], &A[3]);
		TObject *Original = At<ObjGetFn>(S.Get)(&B[0], &B[1], &B[2], &B[3]);
		CHECK(memcmp(A, B, sizeof(A)) == 0 && memcmp(Ours, Original, sizeof(TObject)) == 0, "object slot");
	}
}

static void TestRights()
{
	for (int t = 0; t < 20000; t++) {
		int Id = (int)(Random() % RIGHT_ANY);
		switch (Random() % 8) {
		case 0: {
			int Value = (Random() % 3 == 0) ? 0 : (int)(Random() ^ (Random() << 16));
			At<SetInt2Fn>(0x4e8580)(Id, Value);
			SetRight(Id, Value);
			break;
		}
		case 1:
			if (Random() % 20 == 0) {
				At<VoidFn>(0x4e6de0)();
				ClearRights();
			}
			break;
		default: {
			int Bit = (int)(Random() % 33) - 1;
			bool Ours = CheckRight(Id, Bit);
			bool Original = At<bool (__cdecl *)(int, int)>(0x4e86e0)(Id, Bit);
			CHECK(Ours == Original, "CheckRight(%d,%d)", Id, Bit);
			break;
		}
		}
		CHECK(CheckAnyRight() == At<BoolFn>(0x4e6dc0)(), "CheckAnyRight");
	}
	for (int i = 0; i < 32; i++)
		CHECK(strcmp(GetRightName(i), At<StrIndexFn>(0x4e88c0)(i)) == 0, "GetRightName(%d)", i);
	for (int i = 0; i < 8; i++)
		CHECK(strcmp(GetActionName(i), At<StrIndexFn>(0x4e8de0)(i)) == 0, "GetActionName(%d)", i);
}

// Messages own some of their parameters (see ClearMessages); each side gets
// its own blocks, allocated the way that side frees them.
static bool OwnsParam1(int Type) { return Type == 5 || Type == 6 || Type == 7 || Type == 14 || Type == 15 || Type == 16 || Type == 18 || Type == 25 || Type == 26 || Type == 28 || Type == 51; }
static bool OwnsParam2(int Type) { return Type == 1 || Type == 2 || (Type >= 47 && Type <= 50) || Type == 14 || Type == 15 || Type == 18 || Type == 28; }

static INT_PTR NewBlock(int Side)
{
	return Side == OURS ? (INT_PTR)new char[8] : (INT_PTR)malloc(8);
}

static void MakeParams(int Type, int Side, INT_PTR Shared1, INT_PTR Shared2, INT_PTR *P1, INT_PTR *P2)
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
			char **Pair = Side == OURS ? (char **)new char[8] : (char **)malloc(8);
			Pair[0] = (char *)NewBlock(Side);
			Pair[1] = (char *)NewBlock(Side);
			*P2 = (INT_PTR)Pair;
		} else
			*P2 = NewBlock(Side);
	}
}

static void FreeParams(int Type, INT_PTR P1, INT_PTR P2)
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

static void TestMessages()
{
	struct TModel { int Type; INT_PTR P[2][2]; } Model[64];
	int Count = 0;
	for (int t = 0; t < 20000; t++) {
		int Op = (int)(Random() % 10);
		if (Op < 4 && Count < 60) {
			int Type = (int)(Random() % (MESSAGE_STARTPATCHER + 1));
			INT_PTR Shared1 = (INT_PTR)Random(), Shared2 = (INT_PTR)Random();
			TModel M;
			M.Type = Type;
			MakeParams(Type, OURS, Shared1, Shared2, &M.P[OURS][0], &M.P[OURS][1]);
			MakeParams(Type, ORIGINAL, Shared1, Shared2, &M.P[ORIGINAL][0], &M.P[ORIGINAL][1]);
			typedef void (__cdecl *AddFn)(int, INT_PTR, INT_PTR);
			if (Op < 3) {
				AddMessage(Type, M.P[OURS][0], M.P[OURS][1]);
				At<AddFn>(0x4e9020)(Type, M.P[ORIGINAL][0], M.P[ORIGINAL][1]);
				Model[Count++] = M;
			} else {
				AddMessageFirst(Type, M.P[OURS][0], M.P[OURS][1]);
				At<AddFn>(0x4e91d0)(Type, M.P[ORIGINAL][0], M.P[ORIGINAL][1]);
				memmove(&Model[1], &Model[0], Count * sizeof(TModel));
				Model[0] = M;
				Count++;
			}
		} else if (Op < 8) {
			int Type[2];
			INT_PTR P1[2], P2[2];
			FetchMessage(&Type[OURS], &P1[OURS], &P2[OURS]);
			At<void (__cdecl *)(int *, INT_PTR *, INT_PTR *)>(0x4e75f0)(&Type[ORIGINAL], &P1[ORIGINAL], &P2[ORIGINAL]);
			if (Count == 0) {
				CHECK(Type[0] == 0 && P1[0] == 0 && P2[0] == 0 && Type[1] == 0 && P1[1] == 0 && P2[1] == 0,
					"FetchMessage on an empty queue");
			} else {
				TModel &M = Model[0];
				CHECK(Type[OURS] == M.Type && P1[OURS] == M.P[OURS][0] && P2[OURS] == M.P[OURS][1], "FetchMessage");
				CHECK(Type[ORIGINAL] == M.Type && P1[ORIGINAL] == M.P[ORIGINAL][0] && P2[ORIGINAL] == M.P[ORIGINAL][1],
					"original FetchMessage");
				FreeParams(M.Type, P1[OURS], P2[OURS]);
				FreeParams(M.Type, P1[ORIGINAL], P2[ORIGINAL]);
				memmove(&Model[0], &Model[1], (Count - 1) * sizeof(TModel));
				Count--;
			}
		} else if (Op == 8 && Random() % 10 == 0) {
			ClearMessages();
			At<VoidFn>(0x4e76e0)();
			Count = 0;
		}
		CHECK(PeekMessageType() == At<IntFn>(0x4e75d0)(), "PeekMessageType");
		CHECK(Global<int>(0x71c5f0) == Count, "original NumberOfMessages %d, expected %d", Global<int>(0x71c5f0), Count);
	}
	ClearMessages();
	At<VoidFn>(0x4e76e0)();
}

static void TestAccount()
{
	for (int t = 0; t < 2000; t++) {
		char Password[40];
		RandomText(Password, (int)(Random() % 30));
		int Number = (int)(Random() ^ (Random() << 16));
		if (t % 5 == 0)
			Number = -Number;
		SetAccount(Number, Password);
		At<void (__cdecl *)(int, const char *)>(0x4e93d0)(Number, Password);
		CHECK(GetAccountNumber() == At<IntFn>(0x4e7220)(), "GetAccountNumber");
		CHECK(strcmp(GetAccountNumberString(), At<StrGetFn>(0x4e7230)()) == 0, "GetAccountNumberString %s, original %s",
			GetAccountNumberString(), At<StrGetFn>(0x4e7230)());
		CHECK(memcmp(GetAccountPassword(), At<StrGetFn>(0x4e7240)(), 30) == 0, "GetAccountPassword");

		int Characters = (int)(Random() % 8);
		SetAccountCharacters(Characters);
		At<SetIntFn>(0x4e7250)(Characters);
		CHECK(GetAccountCharacters() == At<IntFn>(0x4e72a0)(), "GetAccountCharacters");
		for (int i = 0; i < Characters; i++) {
			char Name[40], World[40];
			RandomText(Name, (int)(Random() % 30));
			RandomText(World, (int)(Random() % 30));
			uint8 Address[4] = {(uint8)Random(), (uint8)Random(), (uint8)Random(), (uint8)Random()};
			int Port = (int)(Random() % 70000);
			SetCharacter(i, Name, World, Address, Port);
			At<void (__cdecl *)(int, const char *, const char *, const uint8 *, int)>(0x4e96d0)(i, Name, World, Address, Port);
		}
		for (int i = 0; i < Characters; i++) {
			CHECK(strcmp(GetCharacterName(i), At<StrIndexFn>(0x4e9db0)(i)) == 0, "GetCharacterName(%d)", i);
			CHECK(strcmp(GetCharacterWorld(i), At<StrIndexFn>(0x4e9f20)(i)) == 0, "GetCharacterWorld(%d)", i);
			CHECK(strcmp(GetCharacterIPAddress(i), At<StrIndexFn>(0x4ea090)(i)) == 0, "GetCharacterIPAddress(%d)", i);
			CHECK(GetCharacterPort(i) == At<int (__cdecl *)(int)>(0x4ea200)(i), "GetCharacterPort(%d)", i);
			// The raw IP field at +0x3c.
			CHECK(*(const uint32 *)(GetCharacterName(i) + 0x3c) == *(const uint32 *)(At<StrIndexFn>(0x4e9db0)(i) + 0x3c), "IP");
		}
		if (Characters > 0) {
			int Active = (int)(Random() % Characters);
			SetActiveCharacter(Active);
			At<SetIntFn>(0x4e9c50)(Active);
			CHECK(GetActiveCharacter() == At<IntFn>(0x4e72c0)(), "GetActiveCharacter");
		}
	}
}

static void TestErrorReports()
{
	typedef const char *(__cdecl *ReportGetFn)(int);
	for (int t = 0; t < 3000; t++) {
		char Text[400];
		RandomText(Text, (int)(Random() % (t % 3 == 0 ? 300 : 80)));
		switch (Random() % 8) {
		case 0:
			if (GetNumberOfErrorReports() < 12) {
				AddErrorReport(Text);
				At<StrSetFn>(0x4e7c20)(Text);
			}
			break;
		case 1:
			SetErrorReportDate(Text);
			At<StrSetFn>(0x4e7c70)(Text);
			break;
		case 2:
		case 3:
			AddErrorReportText(Text);
			At<StrSetFn>(0x4e7cc0)(Text);
			break;
		case 4:
			SetErrorReportComment(Text);
			At<StrSetFn>(0x4e7d60)(Text);
			break;
		case 5:
			if (Random() % 10 == 0) {
				ClearErrorReports();
				At<VoidFn>(0x4e7bb0)();
			}
			break;
		default:
			break;
		}
		int Count = GetNumberOfErrorReports();
		CHECK(Count == At<IntFn>(0x4e7e10)(), "GetNumberOfErrorReports %d, original %d", Count, At<IntFn>(0x4e7e10)());
		for (int i = -1; i <= Count + 1; i++) {
			const char *A = GetErrorReportTitle(i), *B = At<ReportGetFn>(0x4e7e40)(i);
			if (i >= 0 && i < Count) {
				// Reports in use: the whole record, unterminated fields included.
				CHECK(memcmp(A, B, sizeof(TErrorReport)) == 0, "error report %d differs", i);
				CHECK(GetErrorReportDate(i) - A == 0x40 && At<ReportGetFn>(0x4e7e70)(i) - B == 0x40, "Date offset");
				CHECK(GetErrorReportText(i) - A == 0x5a && At<ReportGetFn>(0x4e7ea0)(i) - B == 0x5a, "Text offset");
				CHECK(GetErrorReportComment(i) - A == 0x85a && At<ReportGetFn>(0x4e7ed0)(i) - B == 0x85a, "Comment offset");
			} else {
				// Possibly outside the vector, where the getters return "".
				CHECK(strncmp(A, B, 64) == 0, "title %d", i);
				CHECK(strncmp(GetErrorReportDate(i), At<ReportGetFn>(0x4e7e70)(i), 26) == 0, "date %d", i);
				CHECK(strncmp(GetErrorReportText(i), At<ReportGetFn>(0x4e7ea0)(i), 2048) == 0, "text %d", i);
				CHECK(strncmp(GetErrorReportComment(i), At<ReportGetFn>(0x4e7ed0)(i), 512) == 0, "comment %d", i);
			}
		}
		CHECK(strcmp(GetErrorReportText(1000), At<ReportGetFn>(0x4e7ea0)(1000)) == 0, "out of range");
	}
}

static void TestClipboard()
{
	static const char *Samples[] = {
		"hello world", "visit www.tibia.com now", "tibia.com", "go to 192.168.0.1:7171",
		"a.b", "x.de.", "mail me@example.org", "WWW.EXAMPLE.COM", "foo%2ebar%2ecom",
		"1.2.3", "text.museum!", "just text, no link.", "", "end with link.net",
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
		At<StrSetFn>(0x4ea5b0)(Text);
		CHECK(LinkWarnings[OURS] == LinkWarnings[ORIGINAL] && ClipboardCalls[OURS] == ClipboardCalls[ORIGINAL]
			&& strcmp(Clipboard[OURS], Clipboard[ORIGINAL]) == 0, "CopyToClipboard(\"%s\")", Text);
		if (Random() % 4 == 0) {
			CopyTaintedClipboardText();
			At<VoidFn>(0x4ea710)();
			CHECK(strcmp(Clipboard[OURS], Clipboard[ORIGINAL]) == 0, "CopyTaintedClipboardText");
		}
	}
	CHECK(LinkWarnings[OURS] > 0, "no link warnings");
}

static void TestBlinkAndFiles()
{
	for (int t = 0; t < 5000; t++) {
		Clock += (int)(Random() % 200);
		UpdateBlinkState();
		At<VoidFn>(0x4e7340)();
		CHECK(GetBlinkState() == At<BoolFn>(0x4e7370)(), "GetBlinkState at %d", Clock);
	}
	DeleteErrorFile();
	At<VoidFn>(0x4e7380)();
	CHECK(RemoveCalls[OURS] == RemoveCalls[ORIGINAL] && strcmp(Removed[OURS], Removed[ORIGINAL]) == 0,
		"DeleteErrorFile");
}

static void CheckInitialState(const char *When)
{
	CHECK(GetActionState() == At<IntFn>(0x4e6e30)() && GetConnectionState() == At<IntFn>(0x4e71f0)()
		&& GetProgress() == At<IntFn>(0x4e7210)() && GetAccountNumber() == At<IntFn>(0x4e7220)()
		&& GetAccountCharacters() == At<IntFn>(0x4e72a0)() && GetPremiumDays() == At<IntFn>(0x4e72b0)()
		&& GetActiveCharacter() == At<IntFn>(0x4e72c0)() && GetGamemaster() == At<BoolFn>(0x4e72d0)()
		&& GetMouseOverText() == At<StrGetFn>(0x4e7310)() && GetTradeInfo() == At<IntFn>(0x4e71d0)()
		&& GetPendingActionState() == At<IntFn>(0x4e6ee0)() && GetPendingActionNumber() == At<IntFn>(0x4e6ef0)()
		&& PeekMessageType() == At<IntFn>(0x4e75d0)() && CheckAnyRight() == At<BoolFn>(0x4e6dc0)()
		&& strcmp(GetAccountPassword(), At<StrGetFn>(0x4e7240)()) == 0, "state after %s", When);
}

void TestStatus()
{
	PatchImage();
	// The original's static initialisers (0x55a0f0, 0x55a110) did not run.
	At<VectorCtorFn>(0x4e7f50)((void *)(uintptr_t)0x71c5f4, 0, 0, 9, 10);
	At<VectorCtorFn>(0x4e8230)((void *)(uintptr_t)0x71c648, 0, 0, 0, 1);

	for (int Round = 0; Round < 2; Round++) {
		bool Gamemaster = Round == 1;
		InitStatus(Gamemaster, OurLinkWarning);
		At<void (__cdecl *)(bool, void (*)())>(0x4ea860)(Gamemaster, OriginalLinkWarning);
		CheckInitialState("InitStatus");
		TestAccessors();
		TestMouse();
		TestRights();
		TestMessages();
		TestAccount();
		TestErrorReports();
		TestClipboard();
		TestBlinkAndFiles();
		ExitStatus();
		At<VoidFn>(0x4e7f00)();
	}
}
