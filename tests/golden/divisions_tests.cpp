// difftest.exe's Divisions test (Patella-Modern issue #67, part 2), a golden-corpus test (golden.h): the integer
// divisions whose divisor input can make zero, against the original.
//
// The original (MSVC 7.1) trapped there (EXCEPTION_INT_DIVIDE_BY_ZERO), and its catch wrapper's catch (...), which
// caught structured exceptions, turned the trap into a TMyException. The x64 client builds with /EHsc, where nothing
// catches a trap: our code checks the divisor first (PATELLA_DIVISOR, compat/assertion.h) and throws the
// TMyException itself. Each case is compared by its outcome (golden_eh.h: TMyException, a PROGRAM_ERROR, another
// type, or nothing), with a divisor that is not zero first (the values compared) and then with zero:
// - TCreature::StartMovement with Beat 0, the game login's word (Communication.cpp ReceiveGameLogin);
// - GetCumulativePattern and GetLiquidPattern with a pattern width, then a height, of 0 in Tibia.dat (both sides'
//   property tables, loaded from the real Tibia.dat, the runner's from the corpus).
// In the oracle the test runs our code compiled with /EHa, as every difftest test: before the checks, our side's
// trap was "another type" (OurThrow's catch (...)) against the original's TMyException; in the runner (/EHsc) it
// ended the runner. The original's own traps are expected (difftest.h, ExpectDivideByZero: no crash report).
//
// The test comes after every other of difftest; the corpora recorded before it have no Divisions test, which the
// runner accepts (runner.cpp, TPorted::Since).

#include "golden_test.h"

#include "golden_eh.h"

#include "Creatures.h"
#include "Objects.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace DivisionsTest {


int Compared = 0;
int Threw = 0;

void Compare(const char *What, const TThrown &Ours, const TThrown &Original)
{
	Compared++;
	if (Original.Kind != TThrown::NOTHING)
		Threw++;
	CompareThrown("Divisions", What, Ours, Original);
}

// One creature on each side (TCreature has the original's layout and no pointers), not the player.
void TestStartMovement()
{
	static TCreature Ours;
	ORIGINAL(static TCreature Original);
	const int Beats[] = {50, 1, 0};
	for (const int Beat : Beats) {
		memset(static_cast<void *>(&Ours), 0, sizeof(Ours));
		Ours.CreatureID = 0x7ffffff0;
		Ours.Speed = 220;
		ORIGINAL(memcpy(static_cast<void *>(&Original), &Ours, sizeof(Ours)));
		SetBeat(Beat);
		ORIGINAL(At<SetFn>(0x44b900)(Beat));
		char What[48];
		snprintf(What, sizeof(What), "StartMovement, Beat %d", Beat);
		const TThrown A = OurThrow([&] { Ours.StartMovement(1, 0, 150); });
		ORIGINAL(ExpectDivideByZero = true);
		const TThrown B = ORIGINAL_THROWN("thrown", At<MemberSet3Fn>(0x44bc70)(&Original, NULL, 1, 0, 150));
		ORIGINAL(ExpectDivideByZero = false);
		Compare(What, A, B);
		if (A.Kind == TThrown::NOTHING && B.Kind == TThrown::NOTHING) {
			const int Duration = ORIGINAL_VALUE("StepDuration", int, Original.StepDuration);
			CHECK(Ours.StepDuration == Duration, "%s: StepDuration %d, the original %d", What, Ours.StepDuration,
				Duration);
		}
	}
	// Back to the start's 0 (the static data of both sides); no later test moves a creature.
	SetBeat(0);
	ORIGINAL(At<SetFn>(0x44b900)(0));
}

// The first object type of Tibia.dat with the flag (100 ... the highest type), or 0.
int FirstWithFlag(int Flag)
{
	for (int Type = 100; Type <= patella::bridge::ObjectTypes().ObjectProperties->high(); Type++)
		if (GetObjectFlag(Type, Flag))
			return Type;
	return 0;
}

// Sets one pattern size of Type on both sides.
void SetPatternSize(int Type, bool Height, int Value)
{
	TProperties &Entry = (*patella::bridge::ObjectTypes().ObjectProperties)(Type);
	(Height ? Entry.PatternHeight : Entry.PatternWidth) = Value;
}

void TestPatterns(const char *Name, [[maybe_unused]] uint32 Address, void (*Function)(TObject &, int *, int *),
	int Flag, int Data)
{
	const int Type = FirstWithFlag(Flag);
	CHECK(Type == ORIGINAL_VALUE("type", int, Type), "%s: object type", Name);
	if (Type == 0) {
		printf("note: Divisions: no object with flag %d in Tibia.dat, %s not compared\n", Flag, Name);
		return;
	}
	const TProperties Saved = (*patella::bridge::ObjectTypes().ObjectProperties)(Type);
	for (int Round = 0; Round < 3; Round++) {
		SetPatternSize(Type, false, Round == 1 ? 0 : Saved.PatternWidth);
		SetPatternSize(Type, true, Round == 2 ? 0 : Saved.PatternHeight);
		TObject Obj;
		Obj.Type = Type;
		Obj.Data = Data;
		int OursX = -1, OursY = -1;
		ORIGINAL(int OriginalX = -1; int OriginalY = -1);
		char What[64];
		static const char *const Rounds[] = {"as in Tibia.dat", "width 0", "height 0"};
		snprintf(What, sizeof(What), "%s, pattern %s", Name, Rounds[Round]);
		const TThrown A = OurThrow([&] { Function(Obj, &OursX, &OursY); });
		ORIGINAL(ExpectDivideByZero = true);
		const TThrown B = ORIGINAL_THROWN("thrown", At<PatternFn>(Address)(&Obj, &OriginalX, &OriginalY));
		ORIGINAL(ExpectDivideByZero = false);
		Compare(What, A, B);
		if (A.Kind == TThrown::NOTHING && B.Kind == TThrown::NOTHING) {
			const int X = ORIGINAL_VALUE("x", int, OriginalX);
			const int Y = ORIGINAL_VALUE("y", int, OriginalY);
			CHECK(OursX == X && OursY == Y, "%s: (%d, %d), the original (%d, %d)", What, OursX, OursY, X, Y);
		}
	}
	SetPatternSize(Type, false, Saved.PatternWidth);
	SetPatternSize(Type, true, Saved.PatternHeight);
}

} // namespace DivisionsTest

void TestDivisions([[maybe_unused]] const char *ExePath)
{
	using namespace DivisionsTest;
	const int Before = Checks, BeforeFailures = Failures;
	Compared = 0;
	Threw = 0;
	TestStartMovement();
	const std::string Dat = golden::DataFile("Tibia.dat", ExePath);
	if (Dat.empty()) {
		printf("note: Tibia.dat not found next to %s, the Divisions pattern tests skipped\n",
			ExePath != nullptr ? ExePath : "Tibia.exe");
	} else {
		LoadObjects(Dat.c_str());
		ORIGINAL(At<LoadFn>(0x4d7380)(Dat.c_str()));
		TestPatterns("GetCumulativePattern", 0x4d6800, GetCumulativePattern, CUMULATIVE, 30);
		TestPatterns("GetLiquidPattern", 0x4d6b20, GetLiquidPattern, LIQUIDCONTAINER, 6);
		ReleaseObjects();
		ORIGINAL(At<VoidFn>(0x4d70c0)());
	}
	printf("Divisions: %d checks, %d failures, %d exceptions compared (%d calls that threw)\n", Checks - Before,
		Failures - BeforeFailures, Compared, Threw);
}
