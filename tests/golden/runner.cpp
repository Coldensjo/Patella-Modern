// golden_runner: replays the golden corpus (golden.h, issue #24) against this build's code, without
// Tibia.exe: the ported tests of tests/golden/*_tests.cpp make the same checks as in the oracle, with
// the original's results read from the corpus.
//
// Usage: golden_runner [--require] [--unverified] [corpus directory]
// The directory defaults to build/golden of the source tree (GOLDEN_DEFAULT_DIR), where the oracle-x86
// preset's golden_corpus target records it. Without a corpus the runner says so and exits with 77,
// which CTest reports as skipped (hosted CI has no corpus: it is derived from Tibia.exe and never leaves
// the machine that recorded it); --require makes that a failure (exit 1).
//
// Before it replays, the runner checks the corpus against its MANIFEST.tsv (issue #93; golden::VerifyDataManifest,
// tools/data_manifest.py): every file with its size and SHA-256, no other file. A missing or mismatched manifest is
// an error (exit 1); --unverified, or PATELLA_GOLDEN_UNVERIFIED=1 in the environment, replays anyway (local
// experiments only: a corpus recorded again by hand, a changed file). The manifest's time zone (UTC0: the recorder's
// TZ) is the process's for the replay (TZ, then _tzset): the corpus's local times (Talk's and Creatures' fixed
// clocks) are of that zone. A TZ of the environment that names another zone is an error too (with --unverified the
// environment's zone is kept).
//
// Per test executable of the oracle (difftest, sysinctest, communicationtest), the runner replays the
// ported tests and checks that each makes exactly the checks the oracle made and passes them (a test's
// retired parts, golden::RetiredBegin, are skipped: their checks are counted apart, and the replayed and the
// retired ones together must be the oracle's); it lists the tests retired with the oracle as a whole
// (RetiredTests: issue #29's engine tests), counted with the retired checks, and the oracle's tests that are
// not ported yet, with their checks. Exit code: 0 when every replayed test
// passed with the oracle's check count, 1 on a failure or a stale corpus, 77 without a corpus.
//
// Each run has a temporary folder of its own (issue #66, TRunFolder): %TEMP%\golden-run-<process id>-<n>, the
// process's TEMP, TMP and current directory while the tests run, removed at the end. Every file the tests and the
// code under test write goes there (they write below TEMP or GetTempPath, or to the current directory), so two
// runners at once, of one build or of two worktrees, do not share a file.

#include "golden_test.h"

#include <filesystem>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system_error>
#include <time.h>
#include <vector>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

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
	const char *Retired = nullptr; // the issue that retired parts of the test with the oracle (RetiredBegin)
	// The issue that added the test after corpora had been recorded without it (issue #67's Divisions): a manifest
	// without the test is not stale for it; the test is skipped with a note until the corpus is recorded again.
	const char *Since = nullptr;
};
struct TExecutable {
	const char *Name;
	TPorted Tests[40];
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
			{"DialogElements", TestDialogElements}, {"Dialogs", TestDialogs, "#28/#31"},
			{"Errors", [] { TestErrors(nullptr); }, "#26"},
			{"Network", TestNetwork}, {"MainFrm", TestMainFrm},
			{"MapWindow", [] { TestMapWindow(nullptr); }}, {"MainWindow", TestMainWindow},
			{"Divisions", [] { TestDivisions(nullptr); }, nullptr, "#67"}}},
	{"sysinctest", {{"Sysinc", TestSysinc}}},
	{"communicationtest",
		{{"Communication", [] { TestCommunication(nullptr); }}, {"Control", [] { TestControl(nullptr); }, "#30"}}},
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

// The run's temporary folder (issue #66): created below the TEMP the runner was started with, made the process's
// TEMP and TMP (MsvcPutenv, _putenv_s on Windows, sets the C runtime's copy and the process environment: getenv, GetTempPathA and
// std::filesystem::temp_directory_path all see it) and its current directory; at the end the previous directory
// again, and the folder removed with everything in it. A static object, so that the tests' exit(1) removes it too.
class TRunFolder {
public:
	bool Enter()
	{
		std::error_code Error;
		const std::filesystem::path Base = std::filesystem::temp_directory_path(Error);
		if (Error)
			return false;
		for (int n = 0; n < 1000 && Path.empty(); n++) {
			std::filesystem::path Candidate = Base / ("golden-run-" + std::to_string(_getpid()) + "-" + std::to_string(n));
			if (std::filesystem::create_directory(Candidate, Error))
				Path = Candidate;
			else if (Error)
				return false;
		}
		if (Path.empty())
			return false;
		Previous = std::filesystem::current_path(Error);
		const std::string Native = Path.string();
		if (Error || MsvcPutenv("TEMP", Native.c_str()) != 0 || MsvcPutenv("TMP", Native.c_str()) != 0)
			return false;
		std::filesystem::current_path(Path, Error);
		return !Error;
	}
	~TRunFolder()
	{
		if (Path.empty())
			return;
		std::error_code Error;
		if (!Previous.empty())
			std::filesystem::current_path(Previous, Error);
		std::filesystem::remove_all(Path, Error);
		if (Error)
			fprintf(stderr, "golden_runner: cannot remove %s: %s\n", Path.string().c_str(), Error.message().c_str());
	}

private:
	std::filesystem::path Path, Previous;
};
TRunFolder RunFolder;

} // namespace

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
#if defined(_MSC_VER) && defined(_DEBUG)
	// The Debug library's checks (a std::array index out of range, issue #60) report to stderr and end the run with
	// exit code 3, without the CRT's message box or Windows Error Reporting.
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_set_invalid_parameter_handler([](const wchar_t *, const wchar_t *, const wchar_t *, unsigned, uintptr_t) {
		fputs("golden_runner: invalid parameter (a Debug library check)\n", stderr);
		_exit(3);
	});
#endif
	bool Require = false;
	const char *UnverifiedVariable = getenv("PATELLA_GOLDEN_UNVERIFIED");
	bool Unverified = UnverifiedVariable != nullptr && *UnverifiedVariable != 0 && strcmp(UnverifiedVariable, "0") != 0;
	std::string Directory = GOLDEN_DEFAULT_DIR;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--require") == 0)
			Require = true;
		else if (strcmp(argv[i], "--unverified") == 0)
			Unverified = true;
		else if (argv[i][0] == '-') {
			printf("usage: golden_runner [--require] [--unverified] [corpus directory]\n");
			return 2;
		} else
			Directory = argv[i];
	}
	if (const char *Fails = getenv("DIFFTEST_FAILS"))
		FailuresPrinted = atoi(Fails);
	// The corpus is read after the change of directory (TRunFolder): a relative path as the full one.
	if (std::error_code Error; std::filesystem::path(Directory).is_relative())
		Directory = std::filesystem::absolute(Directory, Error).string();

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
	std::error_code ExistsError;
	if (Found == 0 && !std::filesystem::exists(std::filesystem::path(Directory) / golden::DATA_MANIFEST, ExistsError)) {
		printf("golden: no corpus in %s: it is recorded from Tibia.exe on the oracle machine (cmake --build --preset "
			   "oracle-x86 --target golden_corpus) and never leaves it; %s\n",
			Directory.c_str(), Require ? "failed (--require)" : "skipped");
		return Require ? 1 : 77;
	}
	// The corpus against its manifest (issue #93), and the manifest's time zone.
	golden::TDataManifest Data;
	std::vector<std::string> Problems;
	golden::VerifyDataManifest(Directory, "golden", &Data, &Problems);
	const char *Zone = getenv("TZ");
	const bool ZoneSet = Zone != nullptr && *Zone != 0;
	if (Data.Found && !Data.TimeZone.empty() && ZoneSet && Data.TimeZone != Zone)
		Problems.push_back("TZ=" + std::string(Zone) + " in the environment, but the corpus's local times are of the zone "
			+ Data.TimeZone + " (its manifest's): unset TZ, or set it to " + Data.TimeZone);
	if (Data.Found)
		printf("golden: the corpus in %s: recorded at %s (tree clean: %s) on %s, %lld files, %lld bytes, time zone %s\n",
			Directory.c_str(), Data.Commit.c_str(), Data.Clean.c_str(), Data.Date.c_str(), Data.Files, Data.Bytes,
			Data.TimeZone.c_str());
	if (!Problems.empty()) {
		const size_t Shown = Problems.size() < 20 ? Problems.size() : 20;
		printf("golden: the corpus in %s does not match its manifest (%s, issue #93):\n", Directory.c_str(),
			golden::DATA_MANIFEST);
		for (size_t i = 0; i < Shown; i++)
			printf("golden:   %s\n", Problems[i].c_str());
		if (Shown < Problems.size())
			printf("golden:   ... and %zu more\n", Problems.size() - Shown);
		if (!Unverified) {
			printf("golden: refused: the corpus is frozen (notes/reconstruction.md, \"The golden corpus\"); install the "
				   "final one again, or replay this one anyway with --unverified or PATELLA_GOLDEN_UNVERIFIED=1 (local "
				   "experiments only)\n");
			return 1;
		}
		printf("golden: replayed anyway, unverified (--unverified or PATELLA_GOLDEN_UNVERIFIED)\n");
	} else
		printf("golden: every file matches the manifest\n");
	// The replay's zone: the manifest's (with --unverified an environment's TZ stays).
	if (Data.Found && !Data.TimeZone.empty() && !(Unverified && ZoneSet)) {
		if (MsvcPutenv("TZ", Data.TimeZone.c_str()) != 0) {
			printf("golden: cannot set TZ to %s\n", Data.TimeZone.c_str());
			return 1;
		}
#if defined(_WIN32)
		_tzset();
#else
		tzset();
#endif
	}
	if (Stale)
		return 1;
	if (Found != Count) {
		printf("golden: the corpus in %s is incomplete (a manifest is missing); record it again\n", Directory.c_str());
		return 1;
	}

	if (!RunFolder.Enter()) {
		printf("golden: cannot create the run's temporary folder below %s\n", getenv("TEMP") != nullptr ? getenv("TEMP") : "TEMP");
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
			if (!Listed && Test.Since != nullptr) {
				printf("golden: %s's manifest has no test %s (added by %s): not replayed; record the corpus again to "
					   "replay it\n",
					Executable.Name, Test.Name, Test.Since);
			} else if (!Listed) {
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
				printf("%s %s: %d checks replayed + %lld retired with the oracle (%s) = %lld, %d failures (the "
					   "oracle: %lld checks)\n",
					Executable.Name, Test->Name, TestChecks, TestRetired, Test->Retired != nullptr ? Test->Retired : "?",
					TestChecks + TestRetired, TestFailures, Entry.Checks);
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
