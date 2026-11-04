// golden_runner: replays the golden corpus (golden.h, issue #24) against this build's code, without
// Tibia.exe: the ported tests of tests/golden/*_tests.cpp make the same checks as in the oracle, with
// the original's results read from the corpus.
//
// Usage: golden_runner [--require] [corpus directory]
// The directory defaults to build/golden of the source tree (GOLDEN_DEFAULT_DIR), where the oracle-x86
// preset's golden_corpus target records it. Without a corpus the runner says so and exits with 77,
// which CTest reports as skipped (hosted CI has no corpus: it is derived from Tibia.exe and never leaves
// the machine that recorded it); --require makes that a failure (exit 1).
//
// Per test executable of the oracle (difftest, sysinctest, communicationtest), the runner replays the
// ported tests and checks that each makes exactly the checks the oracle made and passes them (a test's
// retired parts, golden::RetiredBegin, are skipped: their checks are counted apart, and the replayed and the
// retired ones together must be the oracle's); it lists the tests retired with the oracle as a whole
// (RetiredTests: issue #29's engine tests), counted with the retired checks, and the oracle's tests that are
// not ported yet, with their checks. Exit code: 0 when every replayed test
// passed with the oracle's check count, 1 on a failure or a stale corpus, 77 without a corpus.

#include "golden_test.h"

#include <stdlib.h>
#include <string.h>

int Failures = 0;
int Checks = 0;
int FailuresPrinted = 20;

// tools/difftest/difftest.cpp's random numbers, the same sequence (golden::Input checks that the inputs
// are the recorded ones).
static uint32 Seed = 12345;
uint32 Random()
{
	Seed = Seed * 1103515245 + 12345;
	return Seed >> 8;
}
uint32 GetRandomSeed() { return Seed; }
void SetRandomSeed(uint32 Value) { Seed = Value; }
void RandomText(char *Text, int Length)
{
	static const char Chars[] = "aAzZ09 \t\n\r\f\v.,-_\xe4\xf6\xfc\xc4\xd6\xdc\xdf\xe9\xc9\xff\xc0\xde\xdf\xe0";
	for (int i = 0; i < Length; i++)
		Text[i] = Chars[Random() % (sizeof(Chars) - 1)];
	Text[Length] = 0;
}

namespace {

struct TPorted {
	const char *Name; // the oracle's test name without "Test"
	void (*Run)();
};
struct TExecutable {
	const char *Name;
	TPorted Tests[32];
};

// The ported tests per test executable of the oracle.
const TExecutable Executables[] = {
	{"difftest",
		{{"XTEA", TestXTEA}, {"Characters", TestCharacters}, {"Colors", TestColors}, {"Strings", TestStrings},
			{"FormatNumber", TestFormatNumber}, {"Buffers", TestBuffers}, {"Skins", TestSkins},
			{"Regexp", TestRegexp}, {"Status", TestStatus},
			{"Preferences", TestPreferences}, {"Script", TestScript}, {"PreferencesFile", TestPreferencesFile},
			{"Player", TestPlayer}, {"Help", TestHelp}, {"Inventory", TestInventory},
			{"Talk", TestTalk}, {"Container", TestContainer},
			{"Objects", [] { TestObjects(nullptr); }}, {"Map", [] { TestMap(nullptr); }},
			{"Creatures", [] { TestCreatures(nullptr); }},
			{"Graphics", [] { TestGraphics(nullptr); }}, {"GUI", TestGUI}, {"DialogWindow", TestDialogWindow},
			{"ContextMenus", TestContextMenus},
			{"ConsoleWindow", TestConsoleWindow},
			{"ContentWindows", TestContentWindows},
			{"DialogElements", TestDialogElements}, {"Dialogs", TestDialogs},
			{"Network", TestNetwork}, {"MainFrm", TestMainFrm},
			{"MapWindow", [] { TestMapWindow(nullptr); }}, {"MainWindow", TestMainWindow}}},
	{"sysinctest", {{"Sysinc", TestSysinc}}},
	{"communicationtest", {}},
};

// The oracle's tests that are retired with it as a whole, by the user's decision (issue #29, closed "not
// planned"; plan M6): the engine tests of the DirectDraw, Direct3D 9 and OpenGL engines, which the x64 client
// does not have (its only engine is the Vulkan engine, issue #20). They are never replayed; their checks are
// counted as retired, with the other retired checks, not as "not ported yet".
struct TRetired {
	const char *Executable;
	const char *Name;
	const char *Reason;
};
const TRetired RetiredTests[] = {
	{"difftest", "SurfaceDX", "#29"},
	{"difftest", "SurfaceDX9", "#29"},
	{"difftest", "SurfaceGL", "#29"},
};

const TRetired *FindRetired(const TExecutable &Executable, const std::string &Name)
{
	for (const TRetired &Test : RetiredTests)
		if (strcmp(Test.Executable, Executable.Name) == 0 && Name == Test.Name)
			return &Test;
	return nullptr;
}

const TPorted *FindPorted(const TExecutable &Executable, const std::string &Name)
{
	for (const TPorted &Test : Executable.Tests)
		if (Test.Name != nullptr && Name == Test.Name)
			return &Test;
	return nullptr;
}

} // namespace

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	bool Require = false;
	std::string Directory = GOLDEN_DEFAULT_DIR;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--require") == 0)
			Require = true;
		else if (argv[i][0] == '-') {
			printf("usage: golden_runner [--require] [corpus directory]\n");
			return 2;
		} else
			Directory = argv[i];
	}
	if (const char *Fails = getenv("DIFFTEST_FAILS"))
		FailuresPrinted = atoi(Fails);

	// The corpus: every executable's manifest, or none.
	golden::TManifest Manifests[sizeof(Executables) / sizeof(Executables[0])];
	int Found = 0, Count = 0;
	bool Stale = false;
	for (const TExecutable &Executable : Executables) {
		try {
			if (golden::ReadManifest(Directory, Executable.Name, &Manifests[Count]))
				Found++;
		} catch (const golden::TStale &Error) {
			printf("golden: stale corpus: %s\n", Error.Message.c_str());
			Stale = true;
			Found++;
		}
		Count++;
	}
	if (Found == 0) {
		printf("golden: no corpus in %s: it is recorded from Tibia.exe on the oracle machine (cmake --build --preset "
			   "oracle-x86 --target golden_corpus) and never leaves it; %s\n",
			Directory.c_str(), Require ? "failed (--require)" : "skipped");
		return Require ? 1 : 77;
	}
	if (Stale)
		return 1;
	if (Found != Count) {
		printf("golden: the corpus in %s is incomplete (a manifest is missing); record it again\n", Directory.c_str());
		return 1;
	}

	bool Ok = true;
	long long TotalReplayed = 0, TotalRetired = 0, TotalMissing = 0, TotalRecorded = 0;
	for (int e = 0; e < Count; e++) {
		const TExecutable &Executable = Executables[e];
		const golden::TManifest &Manifest = Manifests[e];
		long long Replayed = 0, Retired = 0, Missing = 0;
		int ReplayedTests = 0;
		std::string NotPorted, RetiredWhole;
		for (const TPorted &Test : Executable.Tests) {
			if (Test.Name == nullptr)
				continue;
			bool Listed = false;
			for (int t = 0; t < Manifest.NumberOfTests; t++)
				Listed = Listed || Manifest.Tests[t].Name == Test.Name;
			if (!Listed) {
				printf("golden: stale corpus: %s's manifest has no test %s; record it again\n", Executable.Name,
					Test.Name);
				Ok = false;
			}
		}
		for (int t = 0; t < Manifest.NumberOfTests; t++) {
			const golden::TTestEntry &Entry = Manifest.Tests[t];
			const TPorted *Test = FindPorted(Executable, Entry.Name);
			if (const TRetired *Whole = FindRetired(Executable, Entry.Name); Test == nullptr && Whole != nullptr) {
				Retired += Entry.Checks;
				char Item[96];
				snprintf(Item, sizeof(Item), "%s%s %lld (%s)", RetiredWhole.empty() ? "" : ", ", Entry.Name.c_str(),
					Entry.Checks, Whole->Reason);
				RetiredWhole += Item;
				printf("%s %s: %lld checks retired with the oracle (%s), not replayed\n", Executable.Name,
					Entry.Name.c_str(), Entry.Checks, Whole->Reason);
				continue;
			}
			if (Test == nullptr) {
				Missing += Entry.Checks;
				char Item[96];
				snprintf(Item, sizeof(Item), "%s%s %lld", NotPorted.empty() ? "" : ", ", Entry.Name.c_str(), Entry.Checks);
				NotPorted += Item;
				continue;
			}
			int ChecksBefore = Checks, FailuresBefore = Failures;
			const long long RetiredBefore = golden::RetiredChecks();
			try {
				SetRandomSeed(golden::BeginReplay(Directory, Executable.Name, Test->Name));
				if (GetRandomSeed() != Entry.Seed)
					throw golden::TStale{std::string(Test->Name) + ": the seed differs from the manifest's"};
				Test->Run();
				golden::EndReplay(Checks - ChecksBefore);
			} catch (const golden::TStale &Error) {
				printf("golden: stale corpus: %s\n", Error.Message.c_str());
				Ok = false;
				continue;
			}
			int TestChecks = Checks - ChecksBefore, TestFailures = Failures - FailuresBefore;
			// The checks of the test's retired parts (golden::RetiredBegin, issue #28): the oracle's, skipped.
			const long long TestRetired = golden::RetiredChecks() - RetiredBefore;
			if (TestRetired == 0)
				printf("%s %s: %d checks, %d failures (the oracle: %lld checks)\n", Executable.Name, Test->Name,
					TestChecks, TestFailures, Entry.Checks);
			else
				printf("%s %s: %d checks replayed + %lld retired with the oracle (#28/#31) = %lld, %d failures (the "
					   "oracle: %lld checks)\n",
					Executable.Name, Test->Name, TestChecks, TestRetired, TestChecks + TestRetired, TestFailures,
					Entry.Checks);
			if (TestFailures != 0 || TestChecks + TestRetired != Entry.Checks) {
				if (TestChecks + TestRetired != Entry.Checks)
					printf("golden: %s %s: %d replayed + %lld retired checks, not the oracle's %lld\n", Executable.Name,
						Test->Name, TestChecks, TestRetired, Entry.Checks);
				Ok = false;
			}
			Replayed += TestChecks;
			Retired += TestRetired;
			ReplayedTests++;
		}
		printf("%s: %lld of the oracle's %lld checks replayed (%d tests)", Executable.Name, Replayed, Manifest.Checks,
			ReplayedTests);
		if (Retired != 0)
			printf(", %lld retired with the oracle", Retired);
		if (!RetiredWhole.empty())
			printf(" (whole tests: %s)", RetiredWhole.c_str());
		if (!NotPorted.empty())
			printf("; not ported yet: %lld checks (%s)", Missing, NotPorted.c_str());
		printf("\n");
		TotalReplayed += Replayed;
		TotalRetired += Retired;
		TotalMissing += Missing;
		TotalRecorded += Manifest.Checks;
	}
	printf("golden: %lld of the oracle's %lld checks replayed, %lld retired with the oracle, %lld not ported yet, %d "
		   "failures\n",
		TotalReplayed, TotalRecorded, TotalRetired, TotalMissing, Failures);
	return Ok && Failures == 0 ? 0 : 1;
}
