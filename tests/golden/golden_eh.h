// The golden corpus's error rounds (issue #54): what a call threw, compared between our side and the
// original's, in the oracle (difftest's error rounds, tools/difftest/imageeh.h, issue #34) and in the
// runner alike.
//
// difftest's error rounds make both sides throw (a fake that throws an assertion of the calling side
// at its n-th call) and compare the exceptions that leave the call: TThrown, CompareThrown, TErrorRound. Those parts are portable and live here, the same code in the oracle and in the
// runner; the oracle's own parts (catching the original's exception at the image's boundary,
// ImageThrow, and making the image's exception machinery work) stay in imageeh.h.
//
// In a ported test the original's exception passes through the corpus by value:
//	TThrown Ours = OurThrow([&] { ... our call ... });
//	TThrown Original = ORIGINAL_THROWN("thrown", ... the original's call ...);
//	Round.Compare("What", Ours, Original);
// The oracle runs the original's call inside ImageThrow and records what it threw (golden::OutputThrown:
// kind, type, PROGRAM_ERROR value); the runner gets the recorded one. CompareThrown's check (one per call)
// is the same code on both sides, so their counts are equal.
//
// Since Patella-Modern issue #76 (plan 7, M5) the error paths are compared by outcome only: whether a call
// threw, and what kind of exception (TMyException, a PROGRAM_ERROR with its value, another type by its
// name). A TMyException's message and origin (the file names, line numbers, stringized expressions, values
// and TEND levels) are neither compared nor recorded: M5 changes them (TBEGIN/TEND removed, TASSERT to
// PATELLA_ASSERT), and a change of an assertion's text or line is no failure.
// TErrorRound::Finish records the round's counts (checks, calls compared, calls that threw) and the
// runner compares its own with them.

#pragma once

#include "golden_test.h"

#include <stdint.h>
#include <stdio.h>
#include <string>

// What a call threw, comparable between both sides by its outcome (Kind, Type, Code; issue #76).
struct TThrown {
	enum TKind { NOTHING,
		MYEXCEPTION,
		PROGRAMERROR,
		OTHER } Kind;
	std::string Type; // the decorated name of the thrown type, ".?AVTMyException@@"
	int Code;         // the PROGRAM_ERROR value
	// The message and the origin (TMyException::GetMessage() and patella::ErrorOrigin(), the image's message and
	// origin), for the failure reports only (not compared; the runner has none for the original, the corpus does not
	// record them).
	std::string Message;
	std::string Origin;
	TThrown() : Kind(NOTHING), Code(0) {}
	bool operator==(const TThrown &Other) const
	{
		return Kind == Other.Kind && Type == Other.Type && Code == Other.Code;
	}
	bool operator!=(const TThrown &Other) const { return !(*this == Other); }
	std::string Describe() const; // for failure reports
};

// Our side: catches PROGRAM_ERROR, TMyException and the rest.
TThrown ThrownOf(const TMyException &Exception);
template <class F> TThrown OurThrow(F Function)
{
	TThrown Result;
	try {
		Function();
	} catch (PROGRAM_ERROR Error) {
		Result.Kind = TThrown::PROGRAMERROR;
		Result.Type = ".?AW4PROGRAM_ERROR@@";
		Result.Code = (int)Error;
	} catch (TMyException &Exception) {
		Result = ThrownOf(Exception);
	} catch (...) {
		Result.Kind = TThrown::OTHER;
	}
	return Result;
}

// Compares what both sides threw (errortest.cpp's comparison, shared since
// issue #34), by the outcome (issue #76): one check per call, that both threw
// the same kind (TThrown's ==: kind, type, PROGRAM_ERROR value) or neither
// threw. Until issue #76 it compared a TMyException's message and origin too,
// and each "<file> <line>" pair in them with a check of its own (issue #35).
// Failures are reported as "<Test> <What>: ..."; ThrownCompared and
// ThrownThrew count the calls and those in which the original threw.
void CompareThrown(const char *Test, const char *What, const TThrown &Ours, const TThrown &Original);
extern int ThrownCompared, ThrownThrew;

// The error round of a window or surface test (issue #34): the constructor
// enables the image's exceptions (the oracle), Compare compares one pair of
// calls, Finish disables them again and prints "<Test> errors: N checks, M
// exceptions compared (K calls that threw)". Its checks are counted in
// Checks like every other, reported apart from the test's own count.
// Random() runs from a seed of the round's own (EH_SEED=<n> changes it) and
// gets its old state back in Finish: the round may use the test's random
// world builders, and the later tests' inputs stay the same.
// EH_TRACE=1 prints every comparison.
// In the corpus (issue #54) Finish records N, M and K; the runner prints the
// oracle's next to its own and fails when they differ.
struct TErrorRound {
	const char *Test;
	int Checks0, Compared0, Threw0;
	long long Retired0; // golden::RetiredChecks() at the start (issue #28)
	bool Trace, Trace2;
	uint32_t SavedSeed;
	explicit TErrorRound(const char *Test);
	void Compare(const char *What, const TThrown &Ours, const TThrown &Original);
	void Calling(const char *What, const char *Side)
	{
		if (Trace2)
			printf("%s errors: calling %s %s\n", Test, Side, What);
	}
	void Finish();
};

// Throws an assertion of one side's own code, for the test objects of the
// window tests whose methods throw on demand (the callers' TEND blocks wrap
// it): TComponent::SetSize's "_Width>=0" (GUI.cpp, no test redirects it),
// ours or the image's (0x49bee0), with Value (negative) as the printed
// width. A test object on the image side must throw the image's exception:
// ours would pass through the image's catch (TMyException &) and be read
// with the image's layout. It is gui_tests.cpp's, our GUI.cpp's
// TComponent::SetSize, in the oracle and (since issue #28, GUI.cpp in the
// runner) in the runner; the runner aborts when asked for the image's side.
void ThrowSideAssertion(bool Image, int Value);
// What our TComponent::SetSize(Value, 0) throws for a negative Value: GUI.cpp's
// PATELLA_ASSERT_VALUES(_Width>=0, _Width), our TMyException (the same kind; since issue #79 its line differs). The runner's stand-in until issue #28;
// the oracle still checks it against SetSize (VerifyOurSideAssertion).
[[noreturn]] void OurSideAssertion(int Value);
#if GOLDEN_ORACLE
// The oracle, in a ported test that uses ThrowSideAssertion, before its error round: OurSideAssertion
// must throw what our TComponent::SetSize throws (by outcome, TThrown's ==, since issue #76). Not a check
// (the counts stay difftest's); a difference is a failure.
inline void VerifyOurSideAssertion(const char *Test)
{
	TThrown Real = OurThrow([] { ThrowSideAssertion(false, -17); });
	TThrown StandIn = OurThrow([] { OurSideAssertion(-17); });
	if (Real != StandIn) {
		Failures++;
		printf("FAIL %s errors: OurSideAssertion throws %s, our TComponent::SetSize %s\n", Test,
			StandIn.Describe().c_str(), Real.Describe().c_str());
	}
}
#endif

namespace golden {

// The original's exception by value (an output of the original): the oracle records Thrown, the runner
// gets the recorded one into it.
void OutputThrown(const char *Name, TThrown *Thrown);
inline TThrown RecordedThrown(const char *Name, TThrown Thrown)
{
	OutputThrown(Name, &Thrown);
	return Thrown;
}

} // namespace golden

// ORIGINAL_THROWN(Name, Statements...): what the original's Statements threw (the oracle runs them
// inside ImageThrow, tools/difftest/imageeh.h), recorded by the oracle and read back by the runner.
#if GOLDEN_ORACLE
#define ORIGINAL_THROWN(Name, ...) golden::RecordedThrown(Name, ImageThrow([&] { __VA_ARGS__; }))
#else
#define ORIGINAL_THROWN(Name, ...) golden::RecordedThrown(Name, TThrown())
#endif

// imageeh.h's ROUND_COMPARE by value (issue #28; gui_tests.cpp's GUI_ROUND_COMPARE): our call, then the
// original's (the oracle runs it inside ImageThrow and records what it threw; the runner gets the recorded
// exception and has no ImageCall), then the round's comparison.
#define GOLDEN_ROUND_COMPARE(Round, What, OurCall, ImageCall) \
	do { \
		(Round).Calling(What, "ours"); \
		TThrown Ours_ = OurThrow([&] { OurCall; }); \
		(Round).Calling(What, "the original"); \
		TThrown Original_ = ORIGINAL_THROWN("thrown", ImageCall); \
		(Round).Compare(What, Ours_, Original_); \
	} while (0)
