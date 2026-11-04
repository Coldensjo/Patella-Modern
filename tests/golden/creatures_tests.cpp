// difftest.exe's Creatures test (moved here from tools/difftest/creaturestest.cpp, issue #27):
// Creatures.cpp against the original, as a golden-corpus test (golden.h).
//
// The map, path finder and help functions that Creatures.cpp calls are faked on both sides: the image's
// copies are patched to jump here, and so are ours (Map.cpp's and Help.cpp's, which the test links;
// difftest.exe's PatchJump, the runner's golden::RedirectOwn). The fakes read a shared, randomly filled
// screen; the help hints and the path finder calls are recorded per side and compared. The image's time,
// sprintf, _stricmp, _strdup and stdio are redirected to the test's CRT, and the test CRT's _time32 to
// the same fixed clock while the tests run (the runner's through its import slot of _time32, as Talk's).
// Objects come from the real Tibia.dat (both loaders; the runner's from the corpus), Preferences, Status
// and Talk are the linked reconstructions against the image's own copies. As in difftest, our Map.cpp's
// and Help.cpp's functions stay redirected after the test.
//
// The creature table and the VIP list are compared byte for byte (TCreature and TBuddyEntry have no
// pointers: the same layout on both sides): the reconstructed tables are found through AddCreature's
// first slot and GetBuddyListName(0), the original's are recorded by their changed bytes
// (golden::OutputRuns). A creature of the original is recorded as its slot. Failing assertions are not
// exercised.

#include "golden_test.h"
#include "golden_patch.h"

#include "Creatures.h"
#include "Help.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Status.h"
#include "Talk.h"
#include "sysinc.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static_assert(sizeof(TCreature) == 0x9c, "TCreature: the original's layout on both sides");
static_assert(sizeof(TBuddyEntry) == 0x2c, "TBuddyEntry: the original's layout on both sides");

namespace CreaturesTest {

enum { OURS = 0,
	ORIGINAL_SIDE = 1 };

// --- Shared fake world -----------------------------------------------------

int CenterX = 1000, CenterY = 1000, CenterZ = 7; // GetMapPosition
int FakeLevel = 3;                               // GetMapLevel
int FieldType[16][16];                           // GetFirstObject, by screen x/y
bool FieldFree[16][16];                          // IsFieldPassable
int FieldHeight[16][16];                         // GetFieldHeight
int PathResult, PathCount, PathDirections[10];   // FindPath
struct TPathCall {
	int Args[6];
	bool Exact, Diagonal;
};
std::vector<TPathCall> PathCalls[2];
std::vector<int> Hints[2];
std::vector<std::string> MonsterHints[2];
__time32_t FixedTime = 1700000000;

void GetCenter(int *x, int *y, int *z)
{
	*x = CenterX;
	*y = CenterY;
	*z = CenterZ;
}
TObject Field(int x, int y)
{
	TObject Object;
	Object.Type = FieldType[x & 15][y & 15];
	Object.Data = 0;
	Object.Data2 = 0;
	return Object;
}
void ToScreen(int *x, int *y, int *z)
{
	*x = *x - CenterX + 8;
	*y = *y - CenterY + 6;
	*z = *z == CenterZ ? FakeLevel : *z + 50;
}
int Path(int Side, int x, int y, int z, int DestX, int DestY, int DestZ, bool Exact, int *Steps, int *Directions,
	bool Diagonal)
{
	TPathCall Call = {{x, y, z, DestX, DestY, DestZ}, Exact, Diagonal};
	PathCalls[Side].push_back(Call);
	if (PathResult > 0) {
		*Steps = PathCount;
		for (int i = 0; i < PathCount; i++)
			Directions[i] = PathDirections[i];
	}
	return PathResult;
}

// Our side's Map.cpp functions are redirected to these fakes in PatchCreatures; map_tests.cpp runs
// before this test, so nothing needs them afterwards.
void __cdecl FakeGetMapPosition(int *x, int *y, int *z) { GetCenter(x, y, z); }
int __cdecl FakeGetMapLevel() { return FakeLevel; }
TObject __cdecl FakeGetFirstObject(int rx, int ry, int) { return Field(rx, ry); }
bool __cdecl FakeIsFieldPassable(int x, int y, int, bool) { return FieldFree[x & 15][y & 15]; }
int __cdecl FakeGetFieldHeight(int x, int y, int) { return FieldHeight[x & 15][y & 15]; }
void __cdecl FakeAbsoluteToRelative(int *x, int *y, int *z) { ToScreen(x, y, z); }
int __cdecl FakeFindPath(int x, int y, int z, int DestX, int DestY, int DestZ, bool Exact, int *Steps, int *Directions,
	bool Diagonal)
{
	return Path(OURS, x, y, z, DestX, DestY, DestZ, Exact, Steps, Directions, Diagonal);
}
// Help.cpp is linked into the test, so our side's AddEventHelp and CheckEventHelpName are redirected
// to these recorders while the creature tests run.
void __cdecl OurHint(int Value) { Hints[OURS].push_back(Value); }
void __cdecl OurMonsterHint(const char *Name) { MonsterHints[OURS].push_back(Name); }

__time32_t __cdecl ImageTime(__time32_t *Time)
{
	if (Time != nullptr)
		*Time = FixedTime;
	return FixedTime;
}

#if GOLDEN_ORACLE
void PatchOwn(void *Function, void *Target)
{
	DWORD Protection;
	VirtualProtect(Function, 5, PAGE_EXECUTE_READWRITE, &Protection);
	PatchJump((uint32)(uintptr_t)Function, Target);
	VirtualProtect(Function, 5, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Function, 5);
}

// Replacements patched into the image.
void __cdecl ImageGetCenter(int *x, int *y, int *z) { GetCenter(x, y, z); }
int __cdecl ImageGetLevel() { return FakeLevel; }
TObject __cdecl ImageField(int x, int y, int) { return Field(x, y); }
bool __cdecl ImageFieldFree(int x, int y, int, bool) { return FieldFree[x & 15][y & 15]; }
int __cdecl ImageFieldHeight(int x, int y, int) { return FieldHeight[x & 15][y & 15]; }
void __cdecl ImageToScreen(int *x, int *y, int *z) { ToScreen(x, y, z); }
int __cdecl ImagePath(int x, int y, int z, int DestX, int DestY, int DestZ, bool Exact, int *Steps, int *Directions,
	bool Diagonal)
{
	return Path(ORIGINAL_SIDE, x, y, z, DestX, DestY, DestZ, Exact, Steps, Directions, Diagonal);
}
void __cdecl ImageHint(int Value) { Hints[ORIGINAL_SIDE].push_back(Value); }
void __cdecl ImageMonsterHint(const char *Name) { MonsterHints[ORIGINAL_SIDE].push_back(Name); }
int __cdecl ImageSprintf(char *Buffer, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vsprintf(Buffer, Format, Args);
	va_end(Args);
	return Result;
}
int __cdecl ImageStricmp(const char *a, const char *b) { return _stricmp(a, b); }
char *__cdecl ImageStrdup(const char *s)
{
	char *t = (char *)malloc(strlen(s) + 1);
	strcpy(t, s);
	return t;
}
FILE *__cdecl ImageFopen(const char *Name, const char *Mode) { return fopen(Name, Mode); }
int __cdecl ImageFclose(FILE *File) { return fclose(File); }
size_t __cdecl ImageFread(void *Buffer, size_t Size, size_t Count, FILE *File) { return fread(Buffer, Size, Count, File); }

uint8 SavedClock[5];
void *ClockFunction;
#else
void PatchOwn(void *Function, void *Target) { golden::RedirectOwn(Function, Target); }

// The runner's _time32 is ucrtbase's: its import slot points at the fake while the test runs.
void **ClockSlot;
void *SavedSlot;
#endif

void PatchClock(bool On)
{
#if GOLDEN_ORACLE
	DWORD Protection;
	VirtualProtect(ClockFunction, 5, PAGE_EXECUTE_READWRITE, &Protection);
	if (On)
		PatchJump((uint32)(uintptr_t)ClockFunction, (void *)ImageTime);
	else
		memcpy(ClockFunction, SavedClock, 5);
	FlushInstructionCache(GetCurrentProcess(), ClockFunction, 5);
	VirtualProtect(ClockFunction, 5, Protection, &Protection);
#else
	golden::SetImport(ClockSlot, On ? (void *)ImageTime : SavedSlot);
#endif
}

void PatchCreatures()
{
#if GOLDEN_ORACLE
	PatchImage();
	PatchJump(0x4b08c0, (void *)ImageGetCenter);
	PatchJump(0x4b08f0, (void *)ImageGetLevel);
	PatchJump(0x4b6140, (void *)ImageField);
	PatchJump(0x4be130, (void *)ImageFieldFree);
	PatchJump(0x4bdf30, (void *)ImageFieldHeight);
	PatchJump(0x4b0960, (void *)ImageToScreen);
	PatchJump(0x4bee00, (void *)ImagePath);
	PatchJump(0x4a32e0, (void *)ImageHint);
	PatchJump(0x4a3460, (void *)ImageMonsterHint);
#endif
	PatchOwn((void *)AddEventHelp, (void *)OurHint);
	PatchOwn((void *)CheckEventHelpName, (void *)OurMonsterHint);
	PatchOwn((void *)GetMapPosition, (void *)FakeGetMapPosition);
	PatchOwn((void *)GetMapLevel, (void *)FakeGetMapLevel);
	PatchOwn((void *)GetFirstObject, (void *)FakeGetFirstObject);
	PatchOwn((void *)IsFieldPassable, (void *)FakeIsFieldPassable);
	PatchOwn((void *)GetFieldHeight, (void *)FakeGetFieldHeight);
	PatchOwn((void *)AbsoluteToRelative, (void *)FakeAbsoluteToRelative);
	PatchOwn((void *)FindPath, (void *)FakeFindPath);
#if GOLDEN_ORACLE
	PatchJump(0x5121ea, (void *)ImageTime);
	PatchJump(0x511d6b, (void *)ImageSprintf);
	PatchJump(0x512539, (void *)ImageStricmp);
	PatchJump(0x51491c, (void *)ImageStrdup);
	PatchJump(0x51227f, (void *)ImageFopen);
	PatchJump(0x512122, (void *)ImageFclose);
	PatchJump(0x513a29, (void *)ImageFread);
	ClockFunction = (void *)_time32;
	memcpy(SavedClock, ClockFunction, 5);
#else
	ClockSlot = golden::FindImport("_time32");
	if (ClockSlot == nullptr) {
		printf("golden: Creatures: the runner imports no _time32\n");
		exit(1);
	}
	SavedSlot = *ClockSlot;
#endif
	PatchClock(true);
}

// --- The original functions ------------------------------------------------

#if GOLDEN_ORACLE
typedef void(__cdecl *VoidFn)();
typedef int(__cdecl *IntFn)();
typedef bool(__cdecl *BoolFn)();
typedef void(__cdecl *SetFn)(int);
typedef void(__cdecl *Set2Fn)(int, int);
typedef void(__cdecl *Set3Fn)(int, int, int);
typedef int(__cdecl *GetFn)(int);
typedef bool(__cdecl *TestFn)(int);
typedef const char *(__cdecl *NameFn)(int);
typedef TCreature *(__cdecl *AddFn)(uint32, uint32);
typedef TCreature *(__cdecl *CreatureFn)(uint32);
typedef TCreature *(__cdecl *OpponentFn)(int);
typedef bool(__cdecl *CreatureTestFn)(TCreature *);
typedef void(__cdecl *CreatureDoFn)(TCreature *);
typedef void(__cdecl *PositionFn)(int *, int *, int *);
typedef int(__cdecl *DestinationFn)(int, int, int, bool, bool);
typedef bool(__cdecl *PairFn)(int, int);
typedef void(__cdecl *SwapFn)(int, int);
typedef void(__cdecl *AddBuddyFn)(uint32, const char *, bool);
typedef void(__cdecl *BuddyStateFn)(uint32, bool);
typedef void(__cdecl *EditBuddyFn)(int, int, const char *, bool);
typedef bool(__cdecl *NameTestFn)(const char *);
typedef void(__cdecl *SetBoolFn)(bool);
typedef void(__cdecl *SetBuddyFn)(int, int, const char *, bool);
typedef void(__cdecl *FetchFn)(int *, INT_PTR *, INT_PTR *);
typedef const char *(__cdecl *StatusFn)();
typedef void(__cdecl *LoadFn)(const char *);
// thiscall members through __fastcall: ECX = this, EDX unused, rest on the stack.
typedef TRGBColor *(__fastcall *ColorFn)(void *, void *, TRGBColor *);
typedef bool(__fastcall *MemberTestFn)(void *, void *);
typedef int(__fastcall *MemberGetFn)(void *, void *);
typedef int(__fastcall *MemberGet1Fn)(void *, void *, int);
typedef void(__fastcall *MemberSet1Fn)(void *, void *, int);
typedef void(__fastcall *MemberSkullFn)(void *, void *, int, bool);
typedef void(__fastcall *MemberSet3Fn)(void *, void *, int, int, int);

TCreature *const OriginalCreatures = (TCreature *)(uintptr_t)0x5c68b0;
TBuddyEntry *const OriginalBuddies = (TBuddyEntry *)(uintptr_t)0x5c4570;
#endif
TCreature *OurCreatures;
TBuddyEntry *OurBuddies;
// The original's tables as last recorded (the same start on both sides: the oracle's tables before the
// test are zero, as the static data of the image).
uint8 OriginalCreatureTable[CREATURES * sizeof(TCreature)];
uint8 OriginalBuddyTable[BUDDIES * sizeof(TBuddyEntry)];

// A creature of the original, as its slot (-1: NULL).
int OriginalSlot(const char *Name, [[maybe_unused]] TCreature *Creature)
{
	return ORIGINAL_VALUE(Name, int, Creature == NULL ? -1 : (int)(Creature - OriginalCreatures));
}

void CheckRecords(const char *What)
{
	// The original's records, by value.
	uint32 Count = ORIGINAL_VALUE("hints", uint32, Hints[ORIGINAL_SIDE].size());
	Hints[ORIGINAL_SIDE].resize(Count);
	if (Count != 0)
		golden::OutputArray("hint", Hints[ORIGINAL_SIDE].data(), Count);
	Count = ORIGINAL_VALUE("monster hints", uint32, MonsterHints[ORIGINAL_SIDE].size());
	MonsterHints[ORIGINAL_SIDE].resize(Count);
	for (std::string &Hint : MonsterHints[ORIGINAL_SIDE])
		Hint = golden::OutputText("monster hint", Hint.c_str());
	Count = ORIGINAL_VALUE("path calls", uint32, PathCalls[ORIGINAL_SIDE].size());
	PathCalls[ORIGINAL_SIDE].resize(Count);
	for (TPathCall &Call : PathCalls[ORIGINAL_SIDE]) {
		int Fields[8];
		memcpy(Fields, Call.Args, sizeof(Call.Args));
		Fields[6] = Call.Exact;
		Fields[7] = Call.Diagonal;
		golden::Output("path call", Fields);
		memcpy(Call.Args, Fields, sizeof(Call.Args));
		Call.Exact = Fields[6] != 0;
		Call.Diagonal = Fields[7] != 0;
	}

	CHECK(Hints[OURS] == Hints[ORIGINAL_SIDE], "%s: help hints differ (%d, original %d)", What, (int)Hints[OURS].size(),
		(int)Hints[ORIGINAL_SIDE].size());
	CHECK(MonsterHints[OURS] == MonsterHints[ORIGINAL_SIDE], "%s: monster hints differ (%d, original %d)", What,
		(int)MonsterHints[OURS].size(), (int)MonsterHints[ORIGINAL_SIDE].size());
	bool Same = PathCalls[OURS].size() == PathCalls[ORIGINAL_SIDE].size();
	for (size_t i = 0; Same && i < PathCalls[OURS].size(); i++) {
		const TPathCall &A = PathCalls[OURS][i], &B = PathCalls[ORIGINAL_SIDE][i];
		Same = memcmp(A.Args, B.Args, sizeof(A.Args)) == 0 && A.Exact == B.Exact && A.Diagonal == B.Diagonal;
	}
	CHECK(Same, "%s: path finder calls differ", What);
	for (int s = 0; s < 2; s++) {
		Hints[s].clear();
		MonsterHints[s].clear();
		PathCalls[s].clear();
	}
	const char *A = GetStatusMessage();
	golden::TText B = ORIGINAL_TEXT("GetStatusMessage", At<StatusFn>(0x4fd4d0)());
	CHECK((A == nullptr) == (B.c_str() == nullptr) && (A == nullptr || strcmp(A, B.c_str()) == 0),
		"%s: status message differs", What);
}

void CheckPlayer(const char *What)
{
	static const struct {
		uint32 Address;
		int (*Ours)();
		const char *Name;
	} Getters[] = {
		{0x44b870, GetNumberOfPlayerSteps, "GetNumberOfPlayerSteps"},
		{0x44b9d0, GetPlayerHealth, "GetPlayerHealth"},
		{0x44b9e0, GetPlayerMaxHealth, "GetPlayerMaxHealth"},
		{0x44b9f0, GetPlayerExperience, "GetPlayerExperience"},
		{0x44ba00, GetPlayerLevel, "GetPlayerLevel"},
		{0x44ba10, GetPlayerLevelProgress, "GetPlayerLevelProgress"},
		{0x44ba20, GetPlayerMana, "GetPlayerMana"},
		{0x44ba30, GetPlayerMaxMana, "GetPlayerMaxMana"},
		{0x44ba40, GetPlayerSoul, "GetPlayerSoul"},
		{0x44ba50, GetPlayerMagicLevel, "GetPlayerMagicLevel"},
		{0x44ba60, GetPlayerMagicLevelProgress, "GetPlayerMagicLevelProgress"},
		{0x44ba70, GetPlayerCapacity, "GetPlayerCapacity"},
		{0x44fba0, GetNumberOfBuddies, "GetNumberOfBuddies"},
	};
	for (auto &G : Getters)
		CHECK(G.Ours() == ORIGINAL_VALUE("getter", int, At<IntFn>(G.Address)()), "%s: %s", What, G.Name);
	static const struct {
		uint32 Address;
		bool (*Ours)();
	} States[] = {
		{0x44ba80, IsPlayerPoisoned},
		{0x44ba90, IsPlayerBurning},
		{0x44baa0, IsPlayerElectrified},
		{0x44bab0, IsPlayerDrunk},
		{0x44bac0, IsPlayerManaShielded},
		{0x44bad0, IsPlayerParalyzed},
		{0x44bae0, IsPlayerHasted},
		{0x44baf0, IsPlayerInFight},
	};
	for (auto &S : States)
		CHECK(S.Ours() == ORIGINAL_VALUE("state", bool, At<BoolFn>(S.Address)()), "%s: state bit at 0x%x", What,
			S.Address);
	uint32 IDs[4] = {};
	ORIGINAL(IDs[0] = (uint32)At<IntFn>(0x44b9c0)(); IDs[1] = (uint32)At<IntFn>(0x44bb50)();
		IDs[2] = (uint32)At<IntFn>(0x44bb60)(); IDs[3] = (uint32)At<IntFn>(0x44bb70)());
	golden::Output("IDs", IDs);
	// The old test's && stopped at the first that differed: the same result.
	CHECK(GetPlayerID() == IDs[0] && GetAttackID() == IDs[1] && GetFollowID() == IDs[2] && GetHighlightID() == IDs[3],
		"%s: IDs", What);
	int Steps = GetNumberOfPlayerSteps();
	for (int i = 0; i < Steps; i++)
		CHECK(GetPlayerStep(i) == ORIGINAL_VALUE("GetPlayerStep", int, At<GetFn>(0x44c3c0)(i)), "%s: GetPlayerStep(%d)",
			What, i);
	int x1, y1, z1, Position[3] = {};
	GetPlayerPosition(&x1, &y1, &z1);
	ORIGINAL(At<PositionFn>(0x44b880)(&Position[0], &Position[1], &Position[2]));
	golden::Output("GetPlayerPosition", Position);
	CHECK(x1 == Position[0] && y1 == Position[1] && z1 == Position[2], "%s: GetPlayerPosition", What);
	for (int i = 0; i < SKILLS; i++) {
		int Value = ORIGINAL_VALUE("GetPlayerSkillValue", int, At<GetFn>(0x44d1c0)(i));
		bool SameValue = GetPlayerSkillValue(i) == Value;
		// The old test's && did not call the second getter when the first differed.
		int Progress = SameValue ? ORIGINAL_VALUE("GetPlayerSkillProgress", int, At<GetFn>(0x44d320)(i)) : 0;
		CHECK(SameValue && GetPlayerSkillProgress(i) == Progress, "%s: skill %d", What, i);
	}
}

void CheckCreatures(const char *What)
{
	golden::OutputRuns("creature table", OriginalCreatureTable, ORIGINAL_OR(OriginalCreatures, nullptr),
		sizeof(OriginalCreatureTable));
	CHECK(memcmp(OurCreatures, OriginalCreatureTable, CREATURES * sizeof(TCreature)) == 0, "%s: creature table differs",
		What);
	int Number = GetNumberOfOpponents();
	CHECK(Number == ORIGINAL_VALUE("GetNumberOfOpponents", int, At<IntFn>(0x44d9f0)()), "%s: GetNumberOfOpponents",
		What);
	golden::OutputRuns("creature table", OriginalCreatureTable, ORIGINAL_OR(OriginalCreatures, nullptr),
		sizeof(OriginalCreatureTable));
	CHECK(memcmp(OurCreatures, OriginalCreatureTable, CREATURES * sizeof(TCreature)) == 0, "%s: battle list differs",
		What);
	for (int i = 0; i <= Number; i++) {
		TCreature *A = GetOpponent(i);
		int B = OriginalSlot("GetOpponent", ORIGINAL_OR(At<OpponentFn>(0x44dc70)(i), nullptr));
		CHECK((A == nullptr ? -1 : (int)(A - OurCreatures)) == B, "%s: GetOpponent(%d)", What, i);
	}
}

void CheckBuddies(const char *What)
{
	if (OurBuddies != nullptr) {
		golden::OutputRuns("VIP list", OriginalBuddyTable, ORIGINAL_OR(OriginalBuddies, nullptr),
			sizeof(OriginalBuddyTable));
		CHECK(memcmp(OurBuddies, OriginalBuddyTable, BUDDIES * sizeof(TBuddyEntry)) == 0, "%s: VIP list differs", What);
	}
	CHECK(GetNumberOfBuddies() == ORIGINAL_VALUE("GetNumberOfBuddies", int, At<IntFn>(0x44fba0)()),
		"%s: GetNumberOfBuddies", What);
}

// --- Creature members ------------------------------------------------------

void TestMembers(int Slot)
{
	TCreature *A = &OurCreatures[Slot];
	ORIGINAL(TCreature *B = &OriginalCreatures[Slot]);
	TRGBColor C1 = A->GetHealthColor();
	uint8 C2[3] = {};
	ORIGINAL(TRGBColor Color; At<ColorFn>(0x44b740)(B, NULL, &Color); C2[0] = Color.R; C2[1] = Color.G;
		C2[2] = Color.B);
	golden::Output("GetHealthColor", C2);
	CHECK(C1.R == C2[0] && C1.G == C2[1] && C1.B == C2[2], "GetHealthColor(%d)", A->Health);
	CHECK(A->IsMarked() == ORIGINAL_VALUE("IsMarked", bool, At<MemberTestFn>(0x44b800)(B, NULL)), "IsMarked");
	CHECK(A->GetDeltaX() == ORIGINAL_VALUE("GetDeltaX", int, At<MemberGetFn>(0x44bbf0)(B, NULL)), "GetDeltaX");
	CHECK(A->GetDeltaY() == ORIGINAL_VALUE("GetDeltaY", int, At<MemberGetFn>(0x44bc10)(B, NULL)), "GetDeltaY");
	CHECK(A->GetDrawDirection() == ORIGINAL_VALUE("GetDrawDirection", int, At<MemberGetFn>(0x44bc30)(B, NULL)),
		"GetDrawDirection");
	int Phases = 1 + (int)(Random() % 8);
	CHECK(A->GetAnimationPhase(Phases)
			== ORIGINAL_VALUE("GetAnimationPhase", int, At<MemberGet1Fn>(0x44c0d0)(B, NULL, Phases)),
		"GetAnimationPhase(%d)", Phases);
	CHECK(IsOpponent(A) == ORIGINAL_VALUE("IsOpponent", bool, At<CreatureTestFn>(0x44d740)(B)), "IsOpponent");
}

uint32 RandomID(int Count, uint32 *IDs)
{
	if (Count > 0 && Random() % 4 != 0)
		return IDs[Random() % Count];
	return Random() % 3 == 0 ? 1 + Random() % 1000 : 0x40000000 + Random() % 1000;
}

// Fills the screen fakes; the bank types are real Tibia.dat objects.
std::vector<int> Banks, NonBanks;
void RandomScreen()
{
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			int r = (int)(Random() % 10);
			FieldType[x][y] = r == 0 ? 0 : r < 8 ? Banks[Random() % Banks.size()]
												 : NonBanks[Random() % NonBanks.size()];
			FieldFree[x][y] = Random() % 4 != 0;
			FieldHeight[x][y] = (int)(Random() % 4);
		}
	}
	FakeLevel = (int)(Random() % 8);
}

void RandomPath()
{
	static const int Results[] = {-5, -4, -3, -2, -1, 0, 1, 1, 2, 2};
	PathResult = Results[Random() % 10];
	PathCount = 1 + (int)(Random() % 10);
	for (int i = 0; i < 10; i++)
		PathDirections[i] = (int)(Random() % 10);
}

// Writes the same fields into our creature and, in the oracle, into the original's (the test plays
// Communication.cpp's part).
#if GOLDEN_ORACLE
#define BOTH_CREATURES(A, B, Statement) \
	for (TCreature *T : {A, B}) \
	Statement
#else
#define BOTH_CREATURES(A, B, Statement) \
	for (TCreature *T : {A}) \
	Statement
#endif

void TestCreatureTable()
{
	ResetCreatures();
	ORIGINAL(At<VoidFn>(0x4509e0)());
	SetBeat(50);
	ORIGINAL(At<SetFn>(0x44b900)(50));
	// The first creature takes slot 0 on both sides: the player.
	uint32 IDs[CREATURES];
	int Count = 0;
	uint32 Player = 0x10000 + Random() % 1000;
	OurCreatures = AddCreature(0, Player);
	CHECK(ORIGINAL_VALUE("AddCreature slot 0", bool, At<AddFn>(0x44c530)(0, Player) == OriginalCreatures),
		"AddCreature slot 0");
	SetPlayerID(Player);
	ORIGINAL(At<SetFn>(0x44b920)(Player));
	IDs[Count++] = Player;
	BOTH_CREATURES(&OurCreatures[0], &OriginalCreatures[0], {
		strcpy(T->Name, "Player");
		T->RealX = CenterX;
		T->RealY = CenterY;
		T->RealZ = CenterZ;
		T->Health = 100;
		T->Speed = 220;
		T->Visible = true;
	});
	CheckCreatures("player");

	char What[64];
	for (int t = 0; t < 40000; t++) {
		int Op = (int)(Random() % 22);
		int Slot = (int)(Random() % CREATURES);
		TCreature *A = &OurCreatures[Slot];
		ORIGINAL(TCreature *B = &OriginalCreatures[Slot]);
		snprintf(What, sizeof(What), "creature op %d", Op);
		switch (Op) {
		case 0: { // new creature
			if (Count >= 140)
				break;
			uint32 Remove = Random() % 3 == 0 && Count > 1 ? IDs[1 + Random() % (Count - 1)] : Random() % 4 == 0 ? 12345
																												 : 0;
			uint32 New = RandomID(0, IDs);
			TCreature *P = AddCreature(Remove, New);
			int Q = OriginalSlot("AddCreature", ORIGINAL_OR(At<AddFn>(0x44c530)(Remove, New), nullptr));
			CHECK(P - OurCreatures == Q, "AddCreature slot");
			if (P - OurCreatures != 0 && P - OurCreatures < CREATURES - 1) {
				bool Known = false;
				for (int i = 0; i < Count; i++)
					if (IDs[i] == Remove && Remove != 0) {
						IDs[i] = New;
						Known = true;
					}
				if (!Known)
					IDs[Count++] = New;
			}
			break;
		}
		case 1: { // GetCreature
			uint32 ID = RandomID(Count, IDs);
			if (ID == 0)
				break;
			TCreature *P = GetCreature(ID);
			int Q = OriginalSlot("GetCreature", ORIGINAL_OR(At<CreatureFn>(0x44c6e0)(ID), nullptr));
			CHECK((P == nullptr ? -1 : (int)(P - OurCreatures)) == Q, "GetCreature");
			break;
		}
		case 2: // fields set by Communication.cpp
			if (Slot == 0)
				break;
			{
				int Health = (int)(Random() % 110) - 5, Outfit = (int)(Random() % 3), Color = (int)(Random() % 3);
				bool Visible = Random() % 4 != 0;
				int Speed = (int)(Random() % 400) - 20, Shield = (int)(Random() % 6);
				BOTH_CREATURES(A, B, {
					T->Health = Health;
					T->OutfitType = Outfit;
					T->Colors[0] = Color;
					T->Visible = Visible;
					T->Speed = Speed;
					T->PARTYFlag = Shield;
					snprintf(T->Name, sizeof(T->Name), "Creature %d", Slot);
				});
			}
			break;
		case 3: { // SetPosition
			if (A->CreatureID == 0)
				break;
			int x = CenterX + (int)(Random() % 21) - 10, y = CenterY + (int)(Random() % 17) - 8;
			int z = CenterZ + (Random() % 5 == 0 ? 1 : 0);
			A->SetPosition(x, y, z);
			ORIGINAL(At<MemberSet3Fn>(0x452320)(B, NULL, x, y, z));
			break;
		}
		case 4: { // SetSkull
			int Skull = (int)(Random() % 3);
			bool Delay = Random() % 2 != 0;
			A->SetSkull(Skull, Delay);
			ORIGINAL(At<MemberSkullFn>(0x44b820)(B, NULL, Skull, Delay));
			break;
		}
		case 5: // UpdateSkulls
			FixedTime += (int)(Random() % 4);
			if (Random() % 20 == 0)
				FixedTime -= 10;
			UpdateSkulls();
			ORIGINAL(At<VoidFn>(0x44c860)());
			break;
		case 6: { // SetMark
			if (A->CreatureID == 0)
				break;
			int Mark = (int)(Random() % 256);
			A->SetMark(Mark);
			ORIGINAL(At<MemberSet1Fn>(0x44bf20)(B, NULL, Mark));
			break;
		}
		case 7: { // StartMovement
			if (A->CreatureID == 0)
				break;
			int dx = (int)(Random() % 3) - 1, dy = (int)(Random() % 3) - 1;
			int Waypoints = Random() % 5 == 0 ? (int)(Random() % 3) - 1 : 100 + (int)(Random() % 200);
			if (Slot == 0 && Random() % 2 == 0) {
				// The step DoPlayerStep would have predicted.
				int sx, sy, sz;
				GetPlayerPosition(&sx, &sy, &sz);
				dx = sx - CenterX;
				dy = sy - CenterY;
			}
			A->StartMovement(dx, dy, Waypoints);
			ORIGINAL(At<MemberSet3Fn>(0x44bc70)(B, NULL, dx, dy, Waypoints));
			break;
		}
		case 8: // MoveCreatures
			SetTestClock(GetMilliseconds() + (int)(Random() % 120));
			RandomScreen();
			RandomPath();
			CHECK(MoveCreatures() == ORIGINAL_VALUE("MoveCreatures", bool, At<BoolFn>(0x451ab0)()), "MoveCreatures");
			break;
		case 9: { // HideCreature
			uint32 ID = RandomID(Count, IDs);
			if (ID == 0)
				break;
			HideCreature(ID);
			ORIGINAL(At<SetFn>(0x44c8c0)(ID));
			break;
		}
		case 10:
			if (Random() % 20 == 0) {
				HideAllCreatures();
				ORIGINAL(At<VoidFn>(0x44b8e0)());
			}
			break;
		case 11: // CheckMonsterHint
			if (A->CreatureID == 0)
				break;
			CheckMonsterHint(A);
			ORIGINAL(At<CreatureDoFn>(0x451db0)(B));
			break;
		case 12: { // player data
			int a = (int)(Random() % 2000), b = (int)(Random() % 2000);
			switch (Random() % 8) {
			case 0:
				SetPlayerHealth(a, b);
				ORIGINAL(At<Set2Fn>(0x44b930)(a, b));
				break;
			case 1:
				SetPlayerMagicLevel(a, b);
				ORIGINAL(At<Set2Fn>(0x44b950)(a, b));
				break;
			case 2:
				SetPlayerMana(a, b);
				ORIGINAL(At<Set2Fn>(0x44b970)(a, b));
				break;
			case 3:
				SetPlayerSoul(a);
				ORIGINAL(At<SetFn>(0x44b990)(a));
				break;
			case 4:
				SetPlayerCapacity(a);
				ORIGINAL(At<SetFn>(0x44b9a0)(a));
				break;
			case 5:
				a = (int)Random();
				SetPlayerState(a);
				ORIGINAL(At<SetFn>(0x44b9b0)(a));
				break;
			case 6:
				SetAttackID(a);
				ORIGINAL(At<SetFn>(0x44bb00)(a));
				break;
			case 7:
				SetFollowID(a);
				ORIGINAL(At<SetFn>(0x44bb20)(a));
				break;
			}
			if (Random() % 10 == 0) {
				int Beat = 1 + (int)(Random() % 100);
				SetBeat(Beat);
				ORIGINAL(At<SetFn>(0x44b900)(Beat));
			}
			if (Random() % 3 == 0) {
				SetHighlightID(b);
				ORIGINAL(At<SetFn>(0x44bb40)(b));
			}
			if (Random() % 10 == 0) {
				SetPlayerHealth(0, 100);
				ORIGINAL(At<Set2Fn>(0x44b930)(0, 100));
			} else if (GetPlayerHealth() == 0) {
				SetPlayerHealth(100, 100);
				ORIGINAL(At<Set2Fn>(0x44b930)(100, 100));
			}
			break;
		}
		case 13: { // experience and skills
			int Experience = Random() % 4 == 0 ? 0 : (int)(Random() % 100000) - 10;
			int Level = (int)(Random() % 25), Progress = (int)(Random() % 102) - 1;
			if (Random() % 10 == 0)
				Level = 0;
			SetPlayerExperience(Experience, Level, Progress);
			ORIGINAL(At<Set3Fn>(0x44cc00)(Experience, Level, Progress));
			int Skill = (int)(Random() % SKILLS), Value = (int)(Random() % 30);
			Progress = (int)(Random() % 101);
			SetPlayerSkill(Skill, Value, Progress);
			ORIGINAL(At<Set3Fn>(0x44cf10)(Skill, Value, Progress));
			golden::TText Name = ORIGINAL_TEXT("GetSkillName", At<NameFn>(0x44d480)(Skill));
			CHECK(strcmp(GetSkillName(Skill), Name.c_str()) == 0, "GetSkillName(%d)", Skill);
			break;
		}
		case 14: { // NotifySkull and the player's flags
			int Flag = (int)(Random() % 3);
			NotifySkull(Flag);
			ORIGINAL(At<SetFn>(0x44ca60)(Flag));
			CHECK(HasPlayerSkull() == ORIGINAL_VALUE("HasPlayerSkull", bool, At<BoolFn>(0x44d6d0)()), "HasPlayerSkull");
			CHECK(IsPlayerInParty() == ORIGINAL_VALUE("IsPlayerInParty", bool, At<BoolFn>(0x44d6f0)()), "IsPlayerInParty");
			CHECK(GetPlayerPartyFlag() == ORIGINAL_VALUE("GetPlayerPartyFlag", int, At<IntFn>(0x44d720)()),
				"GetPlayerPartyFlag");
			for (int i = 0; i < 3; i++) {
				uint32 ID = Random() % 3 == 0 ? Random() : RandomID(Count, IDs);
				CHECK(IsPlayer(ID) == ORIGINAL_VALUE("IsPlayer", bool, At<TestFn>(0x44b910)(ID)), "IsPlayer(%u)", ID);
			}
			break;
		}
		case 15: // the player's own creature
			A = &OurCreatures[0];
			ORIGINAL(B = &OriginalCreatures[0]);
			{
				int Skull = (int)(Random() % 3), Shield = (int)(Random() % 6);
				bool Moving = Random() % 2 != 0;
				BOTH_CREATURES(A, B, {
					T->PKFlag = Skull;
					T->PARTYFlag = Shield;
					T->Moving = Moving;
				});
			}
			break;
		case 16: { // SetPlayerDestination
			RandomScreen();
			RandomPath();
			int x, y, z;
			GetPlayerPosition(&x, &y, &z);
			x += (int)(Random() % 21) - 10;
			y += (int)(Random() % 17) - 8;
			if (Random() % 6 == 0)
				z += 1;
			bool Exact = Random() % 2 != 0, Diagonal = Random() % 2 != 0;
			int Ours = SetPlayerDestination(x, y, z, Exact, Diagonal);
			CHECK(Ours
					== ORIGINAL_VALUE("SetPlayerDestination", int, At<DestinationFn>(0x451400)(x, y, z, Exact, Diagonal)),
				"SetPlayerDestination");
			break;
		}
		case 17: { // CancelPlayerWalk
			RandomScreen();
			RandomPath();
			int Direction = (int)(Random() % 4);
			bool Ours = CancelPlayerWalk(Direction);
			CHECK(Ours == ORIGINAL_VALUE("CancelPlayerWalk", bool, At<TestFn>(0x4518b0)(Direction)), "CancelPlayerWalk");
			break;
		}
		case 18:
			if (Random() % 4 == 0) {
				StopPlayer();
				ORIGINAL(At<VoidFn>(0x4516e0)());
			} else if (Random() % 4 == 0) {
				ClearPlayerSteps();
				ORIGINAL(At<VoidFn>(0x44b8c0)());
			}
			break;
		case 19: { // FindPlayerPath
			RandomScreen();
			RandomPath();
			int Ours = FindPlayerPath();
			CHECK(Ours == ORIGINAL_VALUE("FindPlayerPath", int, At<IntFn>(0x4510f0)()), "FindPlayerPath");
			break;
		}
		case 20: { // the camera follows the confirmed position
			int dx = (int)(Random() % 3) - 1, dy = (int)(Random() % 3) - 1;
			CenterX += dx;
			CenterY += dy;
			break;
		}
		case 21:
			TestMembers(Slot);
			break;
		}
		CheckRecords(What);
		CheckCreatures(What);
		CheckPlayer(What);
		if (Slot != 0)
			TestMembers(Slot);
		if (Failures > FailuresPrinted)
			break;
	}
}

// --- VIP list --------------------------------------------------------------

void RandomName(char *Name)
{
	static const char *Parts[] = {"al", "Bo", "ce", "Da", "ex", "Fi", "go", "Ha", "a", "B", " ", "'"};
	Name[0] = 0;
	int n = 1 + (int)(Random() % 5);
	for (int i = 0; i < n; i++)
		strcat(Name, Parts[Random() % 12]);
}

void TestBuddies()
{
	ResetCreatures();
	ORIGINAL(At<VoidFn>(0x4509e0)());
	OurBuddies = nullptr;
	ClearMessages();
	ORIGINAL(At<VoidFn>(0x4e76e0)());
	uint32 IDs[64];
	for (int i = 0; i < 64; i++)
		IDs[i] = 1 + Random() % 100000;
	char What[64];
	for (int t = 0; t < 30000; t++) {
		int Op = (int)(Random() % 14);
		uint32 ID = IDs[Random() % 64];
		snprintf(What, sizeof(What), "buddy op %d", Op);
		switch (Op) {
		case 0:
		case 1: {          // AddBuddy
			char Name[32]; // at most 64 different IDs, so the list never fills
			RandomName(Name);
			bool Online = Random() % 2 != 0;
			AddBuddy(ID, Name, Online);
			ORIGINAL(At<AddBuddyFn>(0x44eab0)(ID, Name, Online));
			if (OurBuddies == nullptr)
				OurBuddies = (TBuddyEntry *)(GetBuddyListName(0) - 4);
			break;
		}
		case 2:
		case 3: { // SetBuddyState
			bool Online = Random() % 2 != 0;
			SetBuddyState(ID, Online);
			ORIGINAL(At<BuddyStateFn>(0x44f000)(ID, Online));
			break;
		}
		case 4: { // EditBuddy
			char Description[32];
			RandomName(Description);
			if (Random() % 3 == 0)
				Description[0] = 0;
			int Icon = (int)(Random() % 4);
			bool Notify = Random() % 2 != 0;
			EditBuddy(ID, Icon, Description, Notify);
			ORIGINAL(At<EditBuddyFn>(0x44f230)(ID, Icon, Description, Notify));
			break;
		}
		case 5: { // RemoveBuddy
			if (Random() % 3 != 0)
				break;
			bool Known = GetBuddyPosition(ID) != -1;
			RemoveBuddy(ID);
			ORIGINAL(At<SetFn>(0x44f5e0)(ID));
			if (!Known)
				break;
			int Type = 0;
			INT_PTR P1 = 0, P2 = 0;
			FetchMessage(&Type, &P1, &P2);
			int OriginalType = 0;
			long long OriginalP2 = 0;
			ORIGINAL(INT_PTR OriginalP1 = 0; INT_PTR Param2 = 0;
				At<FetchFn>(0x4e75f0)(&OriginalType, &OriginalP1, &Param2); OriginalP2 = Param2);
			golden::Output("FetchMessage type", OriginalType);
			golden::Output("FetchMessage parameter 2", OriginalP2);
			// The first parameter is a text when the type is 0x1a (which the check requires).
			golden::TText Text;
			if (OriginalType == 0x1a)
				Text = ORIGINAL_TEXT("FetchMessage text", (const char *)OriginalP1);
			CHECK(Type == 0x1a && OriginalType == 0x1a && P2 == 0 && OriginalP2 == 0
					&& strcmp((char *)P1, Text.c_str()) == 0,
				"RemoveBuddy message");
			if (Type == 0x1a)
				free((void *)P1);
			ORIGINAL(if (OriginalType == 0x1a) free((void *)OriginalP1));
			break;
		}
		case 6: { // SetBuddySortMethod
			int Method = (int)(Random() % 3);
			SetBuddySortMethod(Method);
			ORIGINAL(At<SetFn>(0x44f810)(Method));
			int A[3], B[3] = {};
			GetBuddySortmethods(A);
			ORIGINAL(At<void(__cdecl *)(int *)>(0x4db510)(B));
			golden::Output("GetBuddySortmethods", B);
			CHECK(memcmp(A, B, sizeof(A)) == 0, "stored sort methods");
			break;
		}
		case 7: { // HideOfflineBuddies
			bool Hide = Random() % 3 == 0;
			HideOfflineBuddies(Hide);
			ORIGINAL(At<SetBoolFn>(0x44fa00)(Hide));
			break;
		}
		case 8: { // list accessors
			// GetNumberOfBuddies counts the online buddies when the offline
			// ones are hidden, and AddBuddy can leave that count too high;
			// the whole list is NumberOfBuddies long.
			bool Hidden = GetBuddyHideOffline();
			SetBuddyHideOffline(false);
			ORIGINAL(At<SetBoolFn>(0x4db530)(false));
			int Number = GetNumberOfBuddies();
			CHECK(Number == ORIGINAL_VALUE("NumberOfBuddies", int, At<IntFn>(0x44fba0)()), "NumberOfBuddies");
			SetBuddyHideOffline(Hidden);
			ORIGINAL(At<SetBoolFn>(0x4db530)(Hidden));
			for (int p = 0; p < Number; p++) {
				CHECK(GetBuddyListID(p) == ORIGINAL_VALUE("GetBuddyListID", uint32, At<GetFn>(0x44fd40)(p)),
					"GetBuddyListID(%d)", p);
				golden::TText Name = ORIGINAL_TEXT("GetBuddyListName", At<NameFn>(0x44feb0)(p));
				CHECK(strcmp(GetBuddyListName(p), Name.c_str()) == 0, "GetBuddyListName(%d)", p);
				CHECK(GetBuddyListOnline(p) == ORIGINAL_VALUE("GetBuddyListOnline", bool, At<TestFn>(0x450020)(p)),
					"GetBuddyListOnline(%d)", p);
				CHECK(GetBuddyListNew(p) == ORIGINAL_VALUE("GetBuddyListNew", bool, At<TestFn>(0x450190)(p)),
					"GetBuddyListNew(%d)", p);
				CHECK(GetBuddyListIcon(p) == ORIGINAL_VALUE("GetBuddyListIcon", int, At<GetFn>(0x450330)(p)),
					"GetBuddyListIcon(%d)", p);
				const char *A = GetBuddyListDescription(p);
				golden::TText B = ORIGINAL_TEXT("GetBuddyListDescription", At<NameFn>(0x4504a0)(p));
				CHECK((A == nullptr) == (B.c_str() == nullptr) && (A == nullptr || strcmp(A, B.c_str()) == 0),
					"GetBuddyListDescription(%d)", p);
				CHECK(GetBuddyListNotify(p) == ORIGINAL_VALUE("GetBuddyListNotify", bool, At<TestFn>(0x450740)(p)),
					"GetBuddyListNotify(%d)", p);
			}
			break;
		}
		case 9: { // GetBuddyPosition, IsBuddy
			CHECK(GetBuddyPosition(ID) == ORIGINAL_VALUE("GetBuddyPosition", int, At<GetFn>(0x44bb80)(ID)),
				"GetBuddyPosition");
			char Name[32];
			if (OurBuddies != nullptr && GetBuddyPosition(ID) >= 0 && Random() % 2 == 0) {
				strcpy(Name, OurBuddies[GetBuddyPosition(ID)].Name);
				for (char *c = Name; *c; c++)
					if (Random() % 2 == 0)
						*c = (char)toupper((uint8)*c);
			} else {
				RandomName(Name);
			}
			CHECK(IsBuddy(Name) == ORIGINAL_VALUE("IsBuddy", bool, At<NameTestFn>(0x44bbb0)(Name)), "IsBuddy(%s)", Name);
			break;
		}
		case 10: { // CompareBuddies, SwapBuddies, SortBuddy, SortBuddies
			HideOfflineBuddies(false);
			ORIGINAL(At<SetBoolFn>(0x44fa00)(false));
			int Number = GetNumberOfBuddies();
			if (Number < 2)
				break;
			int i = (int)(Random() % Number), j = (int)(Random() % Number);
			if (i == j)
				break;
			CHECK(CompareBuddies(i, j) == ORIGINAL_VALUE("CompareBuddies", bool, At<PairFn>(0x44de20)(i, j)),
				"CompareBuddies(%d,%d)", i, j);
			switch (Random() % 3) {
			case 0:
				SwapBuddies(i, j);
				ORIGINAL(At<SwapFn>(0x44e260)(i, j));
				break;
			case 1:
				SortBuddy(i);
				ORIGINAL(At<SetFn>(0x44e7d0)(i));
				break;
			case 2:
				SortBuddies();
				ORIGINAL(At<VoidFn>(0x44e600)());
				break;
			}
			break;
		}
		case 11: // the action number for GetBuddyListNew
			IncrementActionNumber();
			ORIGINAL(At<VoidFn>(0x4e6e10)());
			break;
		case 12: // preferences entry behind the list
			if (Random() % 4 == 0) {
				int Icon = (int)(Random() % 4);
				bool Notify = Random() % 2 != 0;
				SetBuddy((int)ID, Icon, "note", Notify);
				ORIGINAL(At<SetBuddyFn>(0x4dcfb0)((int)ID, Icon, "note", Notify));
			}
			break;
		case 13:
			if (Random() % 50 == 0) {
				ResetCreatures();
				ORIGINAL(At<VoidFn>(0x4509e0)());
			}
			break;
		}
		CheckRecords(What);
		CheckBuddies(What);
		if (Failures > FailuresPrinted)
			break;
	}
	InitCreatures();
	ORIGINAL(At<VoidFn>(0x452190)());
	CheckBuddies("InitCreatures");
	CheckPlayer("InitCreatures");
}

} // namespace CreaturesTest

void TestCreatures([[maybe_unused]] const char *ExePath)
{
	using namespace CreaturesTest;
	int Before = Checks, BeforeFailures = Failures;
	PatchCreatures();
	std::string Dat = golden::DataFile("Tibia.dat", ExePath);
	if (Dat.empty()) {
		printf("note: Tibia.dat not found next to %s, Creatures tests skipped\n", ExePath != nullptr ? ExePath : "Tibia.exe");
		PatchClock(false);
		return;
	}
	LoadObjects(Dat.c_str());
	ORIGINAL(At<LoadFn>(0x4d7380)(Dat.c_str()));
	for (int Type = 100; Type < 1500; Type++) {
		bool Bank = GetObjectFlag(Type, BANK);
		CHECK(Bank == ORIGINAL_VALUE("bank flag", bool, At<bool(__cdecl *)(int, int)>(0x4d1c40)(Type, BANK)),
			"bank flag %d", Type);
		(Bank ? Banks : NonBanks).push_back(Type);
	}
	SetTestClock(100000);
	// The original's tables before the test, as the comparisons' starting point.
	ORIGINAL(memcpy(OriginalCreatureTable, OriginalCreatures, sizeof(OriginalCreatureTable));
		memcpy(OriginalBuddyTable, OriginalBuddies, sizeof(OriginalBuddyTable)));
	golden::OutputBytes("creature table", OriginalCreatureTable, sizeof(OriginalCreatureTable));
	golden::OutputBytes("VIP list", OriginalBuddyTable, sizeof(OriginalBuddyTable));
	TestCreatureTable();
	TestBuddies();
	ReleaseObjects();
	ORIGINAL(At<VoidFn>(0x4d70c0)());
	PatchClock(false);
	printf("Creatures: %d checks, %d failures\n", Checks - Before, Failures - BeforeFailures);
}
