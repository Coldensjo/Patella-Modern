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
