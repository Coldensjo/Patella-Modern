// The golden corpus's error rounds (issue #54); see golden_eh.h. The comparison moved here from
// tools/difftest/imageeh.cpp unchanged, so that the oracle and the runner run the same code; since issue #76
// it compares the outcome only.

#include "golden_eh.h"

#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#endif

#include <stdlib.h>
#include <string.h>

std::string TThrown::Describe() const
{
	char Text[64];
	switch (Kind) {
	case NOTHING: return "nothing thrown";
	case PROGRAMERROR: snprintf(Text, sizeof(Text), "PROGRAM_ERROR %d", Code); return Text;
	case MYEXCEPTION:
		// The texts where this side has them (not the runner's original: the corpus does not record them).
		if (Message.empty() && Origin.empty())
			return "TMyException";
		return "TMyException \"" + Message + "\" (origin \"" + Origin + "\")";
	default: return "other exception " + Type;
	}
}

TThrown ThrownOf(const TMyException &Exception)
{
	TThrown Result;
	Result.Kind = TThrown::MYEXCEPTION;
	// MSVC's typeid(TMyException).raw_name(), as the image's throw information names the type (the
	// filter of imageeh.cpp); spelled out, the same on every compiler.
	Result.Type = ".?AVTMyException@@";
	Result.Message = Exception.GetMessage();
	Result.Origin = patella::ErrorOrigin(Exception); // "<file> <line>" of the site that threw it (issue #80)
	return Result;
}

TThrown ThrownOf(const TProgramError &Error)
{
	TThrown Result;
	Result.Kind = TThrown::PROGRAMERROR;
	// The original's type (the image's throw information: the bare enum, ".?AW4PROGRAM_ERROR@@"), not
	// TProgramError's: the kind is a PROGRAM_ERROR with its value on both sides (issue #80).
	Result.Type = ".?AW4PROGRAM_ERROR@@";
	Result.Code = static_cast<int>(Error.Code);
	return Result;
}

// --- Comparing what both sides threw (moved from imageeh.cpp, errortest.cpp's before) ---------------

int ThrownCompared = 0, ThrownThrew = 0;

// One check per call: the outcome (issue #76). Until then the messages and origins were compared with their
// "<file> <line>" pairs replaced, and each pair with a check of its own (issue #35: every site names the
// original's line; EH_LINEMAP=1 compared through a map, EH_LINES=1 printed the lines seen).
void CompareThrown(const char *Test, const char *What, const TThrown &Ours, const TThrown &Original)
{
	ThrownCompared++;
	if (Original.Kind != TThrown::NOTHING)
		ThrownThrew++;
	CHECK(Ours == Original, "%s %s: ours %s, original %s", Test, What, Ours.Describe().c_str(),
		Original.Describe().c_str());
}

// --- Our side's assertion -------------------------------------------------------------------------

void OurSideAssertion(int Value)
{
	// GUI.cpp's TComponent::SetSize: PATELLA_ASSERT_VALUES(_Width>=0, _Width), with the original's line (the
	// message names the file without its path; since issue #79 ours names our own line, which nothing compares).
	throw TMyException(patella::ErrorText("GUI.cpp", 1092, "assertion failed", "_Width", Value, "", 0, "_Width>=0"),
		PATELLA_SOURCE_LOCATION());
}

// ThrowSideAssertion is gui_tests.cpp's, in the oracle and (issue #28) in the runner.

// --- The round ---------------------------------------------------------------------------------------

TErrorRound::TErrorRound(const char *_Test)
	: Test(_Test), Checks0(Checks), Compared0(ThrownCompared), Threw0(ThrownThrew), Retired0(golden::RetiredChecks())
{
	Trace = getenv("EH_TRACE") != nullptr && atoi(getenv("EH_TRACE")) != 0;
	Trace2 = Trace && atoi(getenv("EH_TRACE")) >= 2;
	SavedSeed = GetRandomSeed();
	uint32_t Seed = 34;
	for (const char *p = Test; *p != 0; p++)
		Seed = Seed * 31 + (uint8_t)*p;
	if (getenv("EH_SEED") != nullptr)
		Seed ^= strtoul(getenv("EH_SEED"), nullptr, 10);
	SetRandomSeed(Seed);
#if GOLDEN_ORACLE
	EnableImageExceptions();
#endif
}

void TErrorRound::Compare(const char *What, const TThrown &Ours, const TThrown &Original)
{
	if (Trace)
		printf("%s errors %s: %s\n", Test, What, Original.Describe().c_str());
	CompareThrown(Test, What, Ours, Original);
}

void TErrorRound::Finish()
{
#if GOLDEN_ORACLE
	DisableImageExceptions();
#endif
	SetRandomSeed(SavedSeed);
	// The round's checks: those the runner skipped as retired (golden::RetiredBegin) count as the oracle's
	// (the test restores ThrownCompared and ThrownThrew after a retired part).
	const int Retired = (int)(golden::RetiredChecks() - Retired0);
	int Counts[3] = {Checks - Checks0 + Retired, ThrownCompared - Compared0, ThrownThrew - Threw0};
	printf("%s errors: %d checks, %d exceptions compared (%d calls that threw)\n", Test, Counts[0], Counts[1],
		Counts[2]);
	if (Retired != 0)
		printf("%s errors: %d of the checks replayed, %d retired with the oracle\n", Test, Counts[0] - Retired,
			Retired);
	// The corpus: the oracle's counts; the runner's must be the same (not a check: the counts stay
	// difftest's).
	int Recorded[3] = {Counts[0], Counts[1], Counts[2]};
	golden::Output("error round", Recorded);
	if (golden::Mode() == golden::TMode::Replay) {
		printf("%s errors: the oracle's %d checks, %d exceptions compared (%d calls that threw)\n", Test, Recorded[0],
			Recorded[1], Recorded[2]);
		if (memcmp(Counts, Recorded, sizeof(Counts)) != 0) {
			Failures++;
			printf("FAIL %s errors: the counts differ from the oracle's\n", Test);
		}
	}
}

// --- The corpus ------------------------------------------------------------------------------------

namespace golden {

// The outcome only (issue #76): the kind, the PROGRAM_ERROR value and the type's name. The texts stay out of
// the corpus; the oracle keeps its own for its failure reports, the runner's are empty.
void OutputThrown(const char *Name, TThrown *Thrown)
{
	std::string Prefix = Name;
	int Numbers[2] = {(int)Thrown->Kind, Thrown->Code};
	Output((Prefix + " kind").c_str(), Numbers);
	Thrown->Kind = (TThrown::TKind)Numbers[0];
	Thrown->Code = Numbers[1];
	Thrown->Type = OutputText((Prefix + " type").c_str(), Thrown->Type.data(), Thrown->Type.size());
}

} // namespace golden
