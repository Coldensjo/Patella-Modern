// What a ported test (tests/golden/*_tests.cpp) sees on both sides (golden.h): CHECK, the check and
// failure counters and difftest's random numbers. In the oracle they are difftest's own
// (tools/difftest/difftest.h); in the runner tests/golden/runner.cpp defines the same.

#pragma once

// common.h first: it sets _CRT_SECURE_NO_WARNINGS before any C runtime header.
#if GOLDEN_ORACLE
#include "../../tools/difftest/difftest.h"
#else
#include "common.h"
#endif
#include "golden.h"
#include "compat/span.h"

#include <cstddef>

// The original's (pointer, count) arguments as our span parameters (issue #59: the packet buffers, Network.cpp's
// GetString, GetBytes and SendBytes), also those no valid span holds, which the error comparisons pass: a null
// pointer with a count, and a negative count (as a std::size_t, which the functions read back as that int).
template <typename T>
patella::TSpan<T> OriginalSpan(T *Pointer, int Count)
{
	return patella::TSpan<T>(Pointer, static_cast<std::size_t>(Count));
}

#if !GOLDEN_ORACLE
extern int Failures;
extern int Checks;
extern int FailuresPrinted;

// tools/difftest/difftest.h's CHECK.
#define CHECK(Cond, ...) \
	do { \
		Checks++; \
		if (!(Cond)) { \
			Failures++; \
			if (Failures <= FailuresPrinted) { \
				printf("FAIL %s:%d: ", __FILE__, __LINE__); \
				printf(__VA_ARGS__); \
				printf("\n"); \
			} \
		} \
	} while (0)

// tools/difftest/difftest.cpp's random numbers (the runner starts each test at the recorded seed).
uint32 Random();
uint32 GetRandomSeed();
void SetRandomSeed(uint32 Value);
void RandomText(char *Text, int Length);
#endif

// Issue #55: the order of the calls of Random() in one expression is unspecified (C++17 sequences only the
// operands of && || , ?: << >> and assignments, and an initializer list's elements), so every such
// expression of the tests is written as its calls in the order the oracle's compiler (MSVC x86) made them,
// which the unchanged recordings prove. Random() ^ (Random() << 16): the left operand first.
inline uint32 Random32()
{
	const uint32 Low = Random();
	const uint32 High = Random() << 16;
	return Low ^ High;
}

// A fake that stands in for a member function (its address goes where the member function's starts, issue
// #28): on x86 thiscall passes this in ECX, which a __fastcall function takes as its first parameter, with a
// dummy second one for EDX; on x64 every member function takes this as its first argument, as a plain
// function does. So: `R GOLDEN_THISCALL Fake(GOLDEN_THIS, the member's parameters...)` (this unnamed).
// A fake that reads its object names this: `R GOLDEN_THISCALL Fake(GOLDEN_THIS_NAMED(TCreature *This), ...)`
// (issue #27).
#if GOLDEN_ORACLE
#define GOLDEN_THISCALL __fastcall
#define GOLDEN_THIS void *, void *
#define GOLDEN_THIS_NAMED(Declaration) Declaration, void *
#else
#define GOLDEN_THISCALL
#define GOLDEN_THIS void *
#define GOLDEN_THIS_NAMED(Declaration) Declaration
#endif

// status_tests.cpp: the clock GetMilliseconds returns (both sides' in the oracle).
void SetTestClock(int Value);

// The ported tests (each in difftest's order).
void TestXTEA();
void TestCharacters();
void TestColors();
void TestStrings();
void TestFormatNumber();
void TestBuffers();
void TestSkins();
void TestRegexp();
void TestSysinc();
// Issue #26.
void TestStatus();
void TestPreferences();
void TestScript();
void TestPreferencesFile();
void TestPlayer();
void TestHelp();
void TestInventory();
void TestTalk();
void TestContainer();
// Issue #27 (ExePath: where the oracle finds Tibia.dat; the runner passes nullptr, golden::DataFile).
void TestObjects(const char *ExePath);
void TestMap(const char *ExePath);
void TestCreatures(const char *ExePath);
// Issue #54: the first test with an error round.
void TestMainFrm();
// Issue #30.
void TestNetwork();
// Issue #28 (ExePath as TestObjects').
void TestGraphics(const char *ExePath);
void TestGUI();
// Issue #28 (second part).
void TestDialogWindow();
void TestContextMenus();
void TestConsoleWindow();
void TestContentWindows();
void TestDialogElements();
// Issue #28 (third part).
void TestDialogs();
// Issue #27 (second part; ExePath as TestObjects').
void TestMapWindow(const char *ExePath);
void TestMainWindow();
// Issue #26 (second part; ExePath as TestObjects').
void TestErrors(const char *ExePath);
// divisions_tests.cpp (issue #67): after every other test of difftest.
void TestDivisions(const char *ExePath);
// Issue #30 (second part; communicationtest.exe's, ExePath as TestObjects').
void TestCommunication(const char *ExePath);
void TestControl(const char *ExePath);
