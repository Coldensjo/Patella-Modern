// The golden corpus's error rounds (issue #54); see golden_eh.h. The comparison moved here from
// tools/difftest/imageeh.cpp unchanged, so that the oracle and the runner run the same code.

#include "golden_eh.h"

#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#endif

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <vector>

std::string TThrown::Describe() const
{
	char Text[64];
	switch (Kind) {
	case NOTHING: return "nothing thrown";
	case PROGRAMERROR: snprintf(Text, sizeof(Text), "PROGRAM_ERROR %d", Code); return Text;
	case MYEXCEPTION: return "TMyException \"" + Message + "\" (origin \"" + Origin + "\")";
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
	Result.Origin = Exception.GetOrigin();
	return Result;
}

// --- Comparing what both sides threw (moved from imageeh.cpp, errortest.cpp's before) ---------------

static TLineMap OursToOriginal, OriginalToOurs;
int ThrownCompared = 0, ThrownThrew = 0;
const TLineMap &ThrownLineMap() { return OursToOriginal; }

// "<file> <line>" at Start (a message's start or a "reason:\n"): the
// position of the line number and its end, or false.
static bool FindLine(const std::string &Text, size_t Start, size_t *Number, size_t *End)
{
	size_t Space = Start;
	while (Space < Text.size() && Text[Space] != ' ' && Text[Space] != '\n')
		Space++;
	if (Space == Start || Space >= Text.size() || Text[Space] != ' ')
		return false;
	size_t Digits = Space + 1;
	if (Digits < Text.size() && Text[Digits] == '-')
		Digits++;
	size_t Stop = Digits;
	while (Stop < Text.size() && Text[Stop] >= '0' && Text[Stop] <= '9')
		Stop++;
	if (Stop == Digits || (Stop < Text.size() && Text[Stop] != ':'))
		return false;
	*Number = Space + 1;
	*End = Stop;
	return true;
}

namespace {

struct TLineRef {
	std::string File;
	int Line;
};

} // namespace

// Replaces the line numbers of every "<file> <line>: " that starts the
// message or follows ", reason:\n", and of the origin "<file> <line>".
static std::string WithoutLines(const std::string &Text, std::vector<TLineRef> *Lines)
{
	std::string Result;
	size_t Position = 0;
	size_t Start = 0;
	for (;;) {
		size_t Number, End;
		if (FindLine(Text, Start, &Number, &End)) {
			Result.append(Text, Position, Number - Position);
			Result += "#";
			TLineRef Ref = {Text.substr(Start, Number - 1 - Start), atoi(Text.c_str() + Number)};
			Lines->push_back(Ref);
			Position = End;
		}
		size_t Next = Text.find(", reason:\n", (std::max)(Position, Start));
		if (Next == std::string::npos)
			break;
		Start = Next + 10;
	}
	Result.append(Text, Position, std::string::npos);
	return Result;
}

// One check per line pair: our line must be the original's (issue #35: every
// site names the original's line). EH_LINEMAP=1 compares through a map instead,
// as before issue #35 (each of our lines must stand for one original line and
// each original line for one of ours over the whole run), for debugging.
static void CompareLines(const char *Test, const char *What, const std::vector<TLineRef> &Ours,
	const std::vector<TLineRef> &Original)
{
	static const bool Map = getenv("EH_LINEMAP") != nullptr && atoi(getenv("EH_LINEMAP")) != 0;
	for (size_t i = 0; i < Ours.size() && i < Original.size(); i++) {
		if (Ours[i].File != Original[i].File)
			continue; // the messages differ; reported there
		const std::string &File = Ours[i].File;
		std::map<int, int> &Forward = OursToOriginal[File];
		std::map<int, int> &Backward = OriginalToOurs[File];
		if (Map) {
			auto F = Forward.find(Ours[i].Line);
			auto B = Backward.find(Original[i].Line);
			CHECK((F == Forward.end() || F->second == Original[i].Line)
					&& (B == Backward.end() || B->second == Ours[i].Line),
				"%s %s: our %s line %d stands for the original's line %d, earlier for %d; the original's line %d "
				"earlier for our %d",
				Test, What, File.c_str(), Ours[i].Line, Original[i].Line, F == Forward.end() ? -1 : F->second,
				Original[i].Line, B == Backward.end() ? -1 : B->second);
		} else {
			CHECK(Ours[i].Line == Original[i].Line, "%s %s: our %s line %d, the original's %d", Test, What,
				File.c_str(), Ours[i].Line, Original[i].Line);
		}
		Forward[Ours[i].Line] = Original[i].Line;
		Backward[Original[i].Line] = Ours[i].Line;
	}
}

void CompareThrown(const char *Test, const char *What, const TThrown &Ours, const TThrown &Original)
{
	ThrownCompared++;
	if (Original.Kind != TThrown::NOTHING)
		ThrownThrew++;
	std::vector<TLineRef> OurLines, OriginalLines;
	TThrown A = Ours, B = Original;
	A.Message = WithoutLines(Ours.Message, &OurLines);
	A.Origin = WithoutLines(Ours.Origin + ":", &OurLines);
	B.Message = WithoutLines(Original.Message, &OriginalLines);
	B.Origin = WithoutLines(Original.Origin + ":", &OriginalLines);
	CHECK(A == B && OurLines.size() == OriginalLines.size(), "%s %s: ours %s, original %s", Test, What,
		Ours.Describe().c_str(), Original.Describe().c_str());
	// The lines of a comparison that matched otherwise (a different message is
	// reported once, above).
	if (A == B && OurLines.size() == OriginalLines.size())
		CompareLines(Test, What, OurLines, OriginalLines);
}

// --- Our side's assertion -------------------------------------------------------------------------

void OurSideAssertion(int Value)
{
	// GUI.cpp's TComponent::SetSize: TASSERT1(1092, _Width>=0, _Width), expanded (common.h's
	// TMYEXCEPTION; the message names the file without its path).
	throw TMyException("GUI.cpp", 1092, "assertion failed", "_Width", Value, "", 0, "_Width>=0");
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
	if (getenv("EH_LINES") != nullptr) { // the lines seen so far (every test's): our line -> the original's
		for (auto &File : OursToOriginal)
			for (auto &Line : File.second)
				printf("%s errors line: %s %d -> %d\n", Test, File.first.c_str(), Line.first, Line.second);
	}
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

void OutputThrown(const char *Name, TThrown *Thrown)
{
	std::string Prefix = Name;
	int Numbers[2] = {(int)Thrown->Kind, Thrown->Code};
	Output((Prefix + " kind").c_str(), Numbers);
	Thrown->Kind = (TThrown::TKind)Numbers[0];
	Thrown->Code = Numbers[1];
	Thrown->Type = OutputText((Prefix + " type").c_str(), Thrown->Type.data(), Thrown->Type.size());
	Thrown->Message = OutputText((Prefix + " message").c_str(), Thrown->Message.data(), Thrown->Message.size());
	Thrown->Origin = OutputText((Prefix + " origin").c_str(), Thrown->Origin.data(), Thrown->Origin.size());
}

} // namespace golden
