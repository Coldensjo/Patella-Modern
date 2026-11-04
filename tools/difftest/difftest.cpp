// Differential test: calls functions in the original Tibia.exe image and the
// reconstructed ones on the same inputs and compares the results.
//
// Tibia.exe is copied to its base address (see RunChild and MapImage): no
// imports are bound and no code runs except the functions called here. Only functions that do not
// touch imports are called. The original C runtime is not initialised, so
// tests/golden/status_tests.cpp redirects the image's malloc and free to the test's own heap
// (PatchJump) before it calls functions that allocate.
//
// Usage: difftest.exe <path to Tibia.exe> (as sysinctest.exe and communicationtest.exe). CTest and the
// golden_corpus target pass the build's PATELLA_TIBIA_EXE (cmake/OracleX86.cmake); there is no default
// path, so no folder of the machine is assumed.

#include "difftest.h"
#include "../../src/Utils.h"
#include "../../src/TXTEA.h"
#include "../../src/TMyException.h"
#include "../../tests/golden/golden_test.h"
#include <exception>
#include <intrin.h>
#include <malloc.h>
#include <signal.h>
#include <typeinfo>

#ifndef STATUS_HEAP_CORRUPTION
#define STATUS_HEAP_CORRUPTION ((DWORD)0xc0000374)
#endif

int Failures = 0;
int Checks = 0;
int FailuresPrinted = 20;

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

// TestXTEA, TestCharacters, TestColors, TestStrings, TestFormatNumber and TestBuffers are golden-corpus
// tests (tests/golden/core_tests.cpp, issue #24).

// Tibia.exe has no relocations, so it must sit at its base 0x400000, but a
// running 32-bit process already has heaps and code-page tables there. So the
// harness starts a suspended copy of itself, reserves 0x400000-0x728000 in it
// before any of its start-up code runs, and resumes it. The copy (--child)
// then copies Tibia.exe's sections into that range. No loader code, import
// binding or entry point of Tibia.exe runs.
static const uintptr_t TIBIA_BASE = 0x400000;
static const SIZE_T TIBIA_SIZE = 0x328000; // SizeOfImage of Tibia.exe 7.72

// What the child tells the parent (issue #23): a page shared between them,
// written before and after each test. It outlives the child, so the parent
// can name the test in which the child died even when the child could not
// print anything (__fastfail, heap corruption, a stack overflow).
enum { CHILD_STARTING, CHILD_IN_TEST, CHILD_HEAP_CHECK, CHILD_BETWEEN_TESTS, CHILD_FINISHED };
struct TChildStatus {
	volatile LONG State;
	char Test[64];                  // the test that runs or ran last
	volatile DWORD ExceptionCode;   // the last fatal exception the child saw (ReportCrash)
	volatile uint32 ExceptionAddress;
};
static TChildStatus OwnStatus;
static TChildStatus *Status = &OwnStatus;

// The child's exit codes of its own: 0 all checks passed, 1 failures (the
// totals are printed), 2 it could not start, 3 abort(), 4 the heap check
// between the tests failed; the others are Windows status codes.
static const char *ExitCodeName(DWORD Code)
{
	switch (Code) {
	case 0: return "success, but the tests did not finish: ExitProcess in a test?";
	case 1: return "check failures";
	case 2: return "the tests could not start";
	case 3: return "abort()";
	case 4: return "the heap check between the tests failed";
	case 0x80000003: return "breakpoint";
	case 0xc0000005: return "access violation";
	case 0xc000001d: return "illegal instruction";
	case 0xc0000025: return "noncontinuable exception";
	case 0xc0000094: return "integer division by zero";
	case 0xc0000096: return "privileged instruction";
	case 0xc00000fd: return "stack overflow";
	case 0xc000013a: return "Ctrl+C";
	case 0xc0000374: return "heap corruption";
	case 0xc0000409: return "__fastfail, a /GS stack cookie or a pure virtual call";
	case 0xc0000417: return "invalid parameter passed to a C runtime function";
	case 0xe06d7363: return "uncaught C++ exception";
	}
	return "unknown";
}

// Prints why the child ended unless it ended normally (it printed the totals
// itself then); returns the exit code of difftest.exe.
static int ReportChild(DWORD ExitCode, bool TimedOut, DWORD Timeout)
{
	if (Status->State == CHILD_FINISHED && (ExitCode == 0 || ExitCode == 1))
		return (int)ExitCode;
	if (TimedOut)
		printf("difftest: the test process did not end within %lu s (DIFFTEST_TIMEOUT) and was stopped", Timeout);
	else
		printf("difftest: the test process ended with exit code 0x%08lx (%s)", ExitCode, ExitCodeName(ExitCode));
	switch (Status->State) {
	case CHILD_STARTING: printf(" before the first test"); break;
	case CHILD_IN_TEST: printf(" in %s", Status->Test); break;
	case CHILD_HEAP_CHECK: printf(" in the heap check after %s", Status->Test); break;
	case CHILD_BETWEEN_TESTS: printf(" after %s (between the tests, or in a test that does not announce itself)", Status->Test); break;
	case CHILD_FINISHED: printf(" after the last test"); break;
	}
	if (Status->ExceptionCode != 0)
		printf("; last fatal exception 0x%08lx at 0x%08lx", (unsigned long)Status->ExceptionCode,
			(unsigned long)Status->ExceptionAddress);
	printf("\n");
	fflush(stdout);
	return ExitCode != 0 && ExitCode != 1 ? (int)ExitCode : 2;
}

static int RunChild(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	if (argc < 2) {
		printf("usage: %s <path to Tibia.exe>\n", argc > 0 ? argv[0] : "difftest");
		return 2;
	}
	SECURITY_ATTRIBUTES Inherit = {sizeof(Inherit), NULL, TRUE};
	HANDLE Shared = CreateFileMappingA(INVALID_HANDLE_VALUE, &Inherit, PAGE_READWRITE, 0, sizeof(TChildStatus), NULL);
	void *View = Shared != NULL ? MapViewOfFile(Shared, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(TChildStatus)) : NULL;
	if (View == NULL) {
		printf("cannot share memory with the test process (error %lu)\n", GetLastError());
		return 2;
	}
	Status = (TChildStatus *)View;
	char CommandLine[2 * MAX_PATH + 64];
	char Self[MAX_PATH];
	GetModuleFileNameA(NULL, Self, sizeof(Self));
	snprintf(CommandLine, sizeof(CommandLine), "\"%s\" --child \"%s\" %lu", Self, argv[1],
		(unsigned long)(uintptr_t)Shared);
	// The child is killed with the parent (a job that dies with its handle):
	// a child left running would keep the build from linking difftest.exe
	// again. The job also keeps Windows Error Reporting from holding a crashed
	// child.
	HANDLE Job = CreateJobObjectA(NULL, NULL);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION Limits = {};
	Limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
	if (Job != NULL)
		SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Limits, sizeof(Limits));
	STARTUPINFOA Startup = {sizeof(Startup)};
	Startup.dwFlags = STARTF_USESTDHANDLES;
	Startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	Startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	Startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	PROCESS_INFORMATION Process;
	if (!CreateProcessA(Self, CommandLine, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &Startup, &Process)) {
		printf("cannot start the test process (error %lu)\n", GetLastError());
		return 2;
	}
	if (Job == NULL || !AssignProcessToJobObject(Job, Process.hProcess))
		printf("difftest: warning: the test process is not in a job (error %lu); it may outlive this one\n", GetLastError());
	void *Reserved = VirtualAllocEx(Process.hProcess, (void *)TIBIA_BASE, TIBIA_SIZE,
		MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (Reserved != (void *)TIBIA_BASE) {
		printf("cannot reserve 0x%x in the test process (error %lu)\n", (unsigned)TIBIA_BASE, GetLastError());
		TerminateProcess(Process.hProcess, 2);
		return 2;
	}
	// DIFFTEST_TIMEOUT=<seconds>, default 30 minutes (a full run takes a few).
	DWORD Timeout = getenv("DIFFTEST_TIMEOUT") != NULL ? strtoul(getenv("DIFFTEST_TIMEOUT"), NULL, 10) : 1800;
	ResumeThread(Process.hThread);
	bool TimedOut = WaitForSingleObject(Process.hProcess, Timeout != 0 ? Timeout * 1000 : INFINITE) == WAIT_TIMEOUT;
	if (TimedOut) {
		TerminateProcess(Process.hProcess, 2);
		WaitForSingleObject(Process.hProcess, INFINITE);
	}
	DWORD ExitCode = 2;
	GetExitCodeProcess(Process.hProcess, &ExitCode);
	CloseHandle(Process.hThread);
	CloseHandle(Process.hProcess);
	return ReportChild(ExitCode, TimedOut, Timeout);
}

static bool MapImage(const char *Path)
{
	MEMORY_BASIC_INFORMATION Info;
	VirtualQuery((void *)TIBIA_BASE, &Info, sizeof(Info));
	if (Info.AllocationBase != (void *)TIBIA_BASE || Info.State != MEM_COMMIT || Info.Type != MEM_PRIVATE) {
		printf("0x%x was not reserved for the image\n", (unsigned)TIBIA_BASE);
		return false;
	}
	FILE *File = fopen(Path, "rb");
	if (File == NULL) {
		printf("cannot open %s\n", Path);
		return false;
	}
	static uint8 Data[4 << 20];
	size_t Size = fread(Data, 1, sizeof(Data), File);
	fclose(File);
	IMAGE_NT_HEADERS32 *NT = (IMAGE_NT_HEADERS32 *)(Data + ((IMAGE_DOS_HEADER *)Data)->e_lfanew);
	if (NT->OptionalHeader.ImageBase != TIBIA_BASE || NT->OptionalHeader.SizeOfImage != TIBIA_SIZE) {
		printf("%s is not the expected Tibia.exe 7.72 image\n", Path);
		return false;
	}
	uint8 *Image = (uint8 *)TIBIA_BASE;
	memcpy(Image, Data, NT->OptionalHeader.SizeOfHeaders);
	IMAGE_SECTION_HEADER *Section = IMAGE_FIRST_SECTION(NT);
	for (int i = 0; i < NT->FileHeader.NumberOfSections; i++, Section++) {
		if (Section->PointerToRawData + Section->SizeOfRawData <= Size)
			memcpy(Image + Section->VirtualAddress, Data + Section->PointerToRawData, Section->SizeOfRawData);
	}
	return true;
}

// Reports where a crash happened; the original code has no symbols. This
// vectored handler sees each exception first, also the ones a test catches
// (none of these codes in a passing run).
bool ExpectDivideByZero;

static LONG CALLBACK ReportCrash(EXCEPTION_POINTERS *Info)
{
	DWORD Code = Info->ExceptionRecord->ExceptionCode;
	if (Code == EXCEPTION_INT_DIVIDE_BY_ZERO && ExpectDivideByZero)
		return EXCEPTION_CONTINUE_SEARCH;
	if (Code == EXCEPTION_ACCESS_VIOLATION || Code == EXCEPTION_ILLEGAL_INSTRUCTION
	 || Code == EXCEPTION_STACK_OVERFLOW || Code == EXCEPTION_INT_DIVIDE_BY_ZERO
	 || Code == EXCEPTION_PRIV_INSTRUCTION || Code == EXCEPTION_IN_PAGE_ERROR
	 || Code == STATUS_HEAP_CORRUPTION || Code == STATUS_STACK_BUFFER_OVERRUN) {
		Status->ExceptionCode = Code;
		Status->ExceptionAddress = (uint32)(uintptr_t)Info->ExceptionRecord->ExceptionAddress;
		printf("crash: exception 0x%08lx (%s) at 0x%p in %s, eip 0x%08lx esp 0x%08lx\n", Code, ExitCodeName(Code),
			Info->ExceptionRecord->ExceptionAddress, Status->Test, (unsigned long)Info->ContextRecord->Eip,
			(unsigned long)Info->ContextRecord->Esp);
		if (Code == EXCEPTION_ACCESS_VIOLATION)
			printf("crash: %s 0x%08lx\n", Info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
				(unsigned long)Info->ExceptionRecord->ExceptionInformation[1]);
		uint32 *Stack = (uint32 *)(uintptr_t)Info->ContextRecord->Esp;
		for (int i = 0; i < 12; i++) // return addresses show the caller
			printf("crash: [esp%+d] 0x%08x\n", i * 4, Stack[i]);
		printf("crash: ebp 0x%08lx\n", (unsigned long)Info->ContextRecord->Ebp);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

// The paths that end the child without the vectored handler's report, or
// after it: each prints what it knows, then ends the child at once (without
// Windows Error Reporting). The parent adds the exit code and the test.
static void Exit(DWORD Code)
{
	fflush(stdout);
	TerminateProcess(GetCurrentProcess(), Code);
}

// The thrown type of a C++ exception (x86: [1] the object, [2] its ThrowInfo,
// whose catchable types start with the thrown type) and its message where we
// know the class.
static void ReportMessage(const std::type_info *Type, const void *Object)
{
	if (Type == &typeid(TMyException)) // ours, not the image's class of that name
		printf("crash: TMyException: %s\n", ((const TMyException *)Object)->GetMessage().c_str());
	else if (Type == &typeid(std::exception))
		printf("crash: what(): %s\n", ((const std::exception *)Object)->what());
}
static void ReportCppException(const EXCEPTION_RECORD *Record)
{
	__try {
		if (Record->NumberParameters < 3 || Record->ExceptionInformation[0] != 0x19930520)
			return;
		const uint8 *Object = (const uint8 *)Record->ExceptionInformation[1];
		const uint32 *ThrowInfo = (const uint32 *)Record->ExceptionInformation[2];
		const uint32 *Types = (const uint32 *)(uintptr_t)ThrowInfo[3];
		for (uint32 i = 0; i < Types[0]; i++) {
			const uint32 *Catchable = (const uint32 *)(uintptr_t)Types[1 + i];
			const std::type_info *Type = (const std::type_info *)(uintptr_t)Catchable[1];
			if (i == 0)
				printf("crash: thrown type %s\n", (const char *)Type + 8);
			ReportMessage(Type, Object + (int)Catchable[2]); // + mdisp: the base class part
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		printf("crash: (the exception object cannot be read)\n");
	}
}
static LONG WINAPI ReportUnhandled(EXCEPTION_POINTERS *Info)
{
	DWORD Code = Info->ExceptionRecord->ExceptionCode;
	Status->ExceptionCode = Code;
	Status->ExceptionAddress = (uint32)(uintptr_t)Info->ExceptionRecord->ExceptionAddress;
	printf("crash: unhandled exception 0x%08lx (%s) at 0x%p in %s\n", Code, ExitCodeName(Code),
		Info->ExceptionRecord->ExceptionAddress, Status->Test);
	if (Code == 0xe06d7363)
		ReportCppException(Info->ExceptionRecord);
	Exit(Code);
	return EXCEPTION_EXECUTE_HANDLER;
}
static void __cdecl ReportTerminate()
{
	printf("crash: terminate() in %s (a C++ exception left a noexcept function or a destructor)\n", Status->Test);
	abort();
}
static void __cdecl ReportAbort(int)
{
	printf("crash: abort() in %s\n", Status->Test);
	Exit(3);
}
static void __cdecl ReportPurecall()
{
	printf("crash: pure virtual function call in %s (caller 0x%p)\n", Status->Test, _ReturnAddress());
	Exit(0xc0000409);
}
static void __cdecl ReportInvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned, uintptr_t)
{
	printf("crash: invalid parameter passed to a C runtime function in %s\n", Status->Test);
	Exit(0xc0000417);
}
// /GS: a function whose stack cookie is overwritten jumps from
// __security_check_cookie to __report_gsfailure, which ends the process with
// __fastfail; the start of __report_gsfailure is patched into a jump here, so
// the return address is that function's.
extern "C" void __cdecl __report_gsfailure();
static void __cdecl ReportStackCookie()
{
	printf("crash: /GS stack cookie overwritten in the function that returns to 0x%p, in %s\n", _ReturnAddress(),
		Status->Test);
	Exit(0xc0000409);
}
static void PatchOwnJump(void *Address, void *Target)
{
	DWORD Old;
	if (VirtualProtect(Address, 5, PAGE_EXECUTE_READWRITE, &Old)) {
		PatchJump((uint32)(uintptr_t)Address, Target);
		VirtualProtect(Address, 5, Old, &Old);
		FlushInstructionCache(GetCurrentProcess(), Address, 5);
	}
}
static void __cdecl ReportFinished()
{
	Status->State = CHILD_FINISHED;
}

static void InstallCrashReports(const char *StatusHandle)
{
	setvbuf(stdout, NULL, _IONBF, 0); // keep the output of a crashed run
	if (StatusHandle != NULL) {
		void *View = MapViewOfFile((HANDLE)(uintptr_t)strtoul(StatusHandle, NULL, 10), FILE_MAP_ALL_ACCESS, 0, 0,
			sizeof(TChildStatus));
		if (View != NULL)
			Status = (TChildStatus *)View;
	}
	strcpy(Status->Test, "the start-up");
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	ULONG Guarantee = 64 << 10; // stack left for ReportCrash after a stack overflow
	SetThreadStackGuarantee(&Guarantee);
	AddVectoredExceptionHandler(1, ReportCrash);
	SetUnhandledExceptionFilter(ReportUnhandled);
	set_terminate(ReportTerminate);
	signal(SIGABRT, ReportAbort);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_set_purecall_handler(ReportPurecall);
	_set_invalid_parameter_handler(ReportInvalidParameter);
	PatchOwnJump((void *)__report_gsfailure, (void *)ReportStackCookie);
	atexit(ReportFinished);
}

// Every test below announces itself before it runs and checks the heap
// after it (_heapchk, cheap): heap corruption shows up after the test that
// caused it, not later in another test or as a 0xc0000374 without a message.
// The macros keep the calls as they are written: a function-like macro is
// not expanded again in its own expansion, so TestXTEA() calls the function.
// A new test needs a line here, else a crash in it is reported as after the
// test before it.
// With DIFFTEST_RECORD set (the golden corpus, tests/golden/golden.h, issue #24) each test is a
// recording of its own; the tests of tests/golden write their records into it.
static void BeginTest(const char *Name)
{
	strncpy(Status->Test, Name, sizeof(Status->Test) - 1);
	Status->State = CHILD_IN_TEST;
	golden::BeginTest(Name, GetRandomSeed(), Checks);
}
static void EndTest()
{
	golden::EndTest(Checks);
	Status->State = CHILD_HEAP_CHECK;
	int Heap = _heapchk();
	if (Heap != _HEAPOK && Heap != _HEAPEMPTY) {
		printf("difftest: the heap is corrupted after %s (_heapchk() %d)\n", Status->Test, Heap);
		Exit(4);
	}
	Status->State = CHILD_BETWEEN_TESTS;
}
// DIFFTEST_SKIP=<names> (comma-separated, without "Test": SurfaceDX,GUI) leaves
// those tests out of the full run, to find which earlier test leaves state
// that a later one depends on.
static bool Skipped(const char *Name)
{
	const char *List = getenv("DIFFTEST_SKIP");
	if (List == NULL || strncmp(Name, "Test", 4) != 0)
		return false;
	Name += 4;
	size_t Length = strlen(Name);
	for (const char *p = List; *p != 0;) {
		const char *End = strchr(p, ',');
		size_t Item = End != NULL ? (size_t)(End - p) : strlen(p);
		if (Item == Length && strncmp(p, Name, Length) == 0) {
			printf("%s: skipped (DIFFTEST_SKIP)\n", Name);
			return true;
		}
		p += Item;
		if (*p == ',')
			p++;
	}
	return false;
}
#define RUN_TEST(Name, ...) (Skipped(#Name) ? (void)0 : (BeginTest(#Name), Name(__VA_ARGS__), EndTest()))
#define TestXTEA(...) RUN_TEST(TestXTEA, __VA_ARGS__)
#define TestCharacters(...) RUN_TEST(TestCharacters, __VA_ARGS__)
#define TestColors(...) RUN_TEST(TestColors, __VA_ARGS__)
#define TestStrings(...) RUN_TEST(TestStrings, __VA_ARGS__)
#define TestFormatNumber(...) RUN_TEST(TestFormatNumber, __VA_ARGS__)
#define TestBuffers(...) RUN_TEST(TestBuffers, __VA_ARGS__)
#define TestSkins(...) RUN_TEST(TestSkins, __VA_ARGS__)
#define TestStatus(...) RUN_TEST(TestStatus, __VA_ARGS__)
#define TestPreferences(...) RUN_TEST(TestPreferences, __VA_ARGS__)
#define TestScript(...) RUN_TEST(TestScript, __VA_ARGS__)
#define TestPreferencesFile(...) RUN_TEST(TestPreferencesFile, __VA_ARGS__)
#define TestPlayer(...) RUN_TEST(TestPlayer, __VA_ARGS__)
#define TestHelp(...) RUN_TEST(TestHelp, __VA_ARGS__)
#define TestInventory(...) RUN_TEST(TestInventory, __VA_ARGS__)
#define TestSysinc(...) RUN_TEST(TestSysinc, __VA_ARGS__)
#define TestTalk(...) RUN_TEST(TestTalk, __VA_ARGS__)
#define TestObjects(...) RUN_TEST(TestObjects, __VA_ARGS__)
#define TestMap(...) RUN_TEST(TestMap, __VA_ARGS__)
#define TestCreatures(...) RUN_TEST(TestCreatures, __VA_ARGS__)
#define TestGraphics(...) RUN_TEST(TestGraphics, __VA_ARGS__)
#define TestGUI(...) RUN_TEST(TestGUI, __VA_ARGS__)
#define TestDialogWindow(...) RUN_TEST(TestDialogWindow, __VA_ARGS__)
#define TestContextMenus(...) RUN_TEST(TestContextMenus, __VA_ARGS__)
#define TestConsoleWindow(...) RUN_TEST(TestConsoleWindow, __VA_ARGS__)
#define TestContentWindows(...) RUN_TEST(TestContentWindows, __VA_ARGS__)
#define TestDialogElements(...) RUN_TEST(TestDialogElements, __VA_ARGS__)
#define TestDialogs(...) RUN_TEST(TestDialogs, __VA_ARGS__)
#define TestSurfaceDX(...) RUN_TEST(TestSurfaceDX, __VA_ARGS__)
#define TestSurfaceDX9(...) RUN_TEST(TestSurfaceDX9, __VA_ARGS__)
#define TestSurfaceGL(...) RUN_TEST(TestSurfaceGL, __VA_ARGS__)
#define TestContainer(...) RUN_TEST(TestContainer, __VA_ARGS__)
#define TestRegexp(...) RUN_TEST(TestRegexp, __VA_ARGS__)
#define TestErrors(...) RUN_TEST(TestErrors, __VA_ARGS__)
#define TestMapWindow(...) RUN_TEST(TestMapWindow, __VA_ARGS__)
#define TestMainWindow(...) RUN_TEST(TestMainWindow, __VA_ARGS__)
#define TestCommunication(...) RUN_TEST(TestCommunication, __VA_ARGS__)
#define TestControl(...) RUN_TEST(TestControl, __VA_ARGS__)
#define TestNetwork(...) RUN_TEST(TestNetwork, __VA_ARGS__)
#define TestMainFrm(...) RUN_TEST(TestMainFrm, __VA_ARGS__)

int main(int argc, char **argv)
{
	if (argc < 2 || strcmp(argv[1], "--child") != 0)
		return RunChild(argc, argv);
	if (argc < 3)
		return 2;
	// The original to compare against: RunChild passes its own argument on.
	const char *TibiaExe = argv[2];
	InstallCrashReports(argc > 3 ? argv[3] : NULL);
	if (!MapImage(TibiaExe))
		return 2;
	// DIFFTEST_RECORD=<directory>: record the golden corpus of the full run (tests/golden/golden.h).
#ifdef DIFFTEST_SYSINC
	golden::StartRecording("sysinctest", TibiaExe);
#elif defined(DIFFTEST_COMMUNICATION)
	golden::StartRecording("communicationtest", TibiaExe);
#else
	golden::StartRecording("difftest", TibiaExe);
#endif
#ifdef DIFFTEST_SYSINC
	TestSysinc();
#elif defined(DIFFTEST_COMMUNICATION)
	// communicationtest.exe (issue #36): DIFFTEST_ONLY=Communication or Control runs one of its two tests.
	const char *Only = getenv("DIFFTEST_ONLY");
	if (getenv("DIFFTEST_FAILS") != NULL)
		FailuresPrinted = atoi(getenv("DIFFTEST_FAILS"));
	if (Only == NULL || strcmp(Only, "Communication") == 0)
		TestCommunication(TibiaExe);
	if (Only == NULL || strcmp(Only, "Control") == 0)
		TestControl(TibiaExe);
#else
	// DIFFTEST_ONLY=MainWindow, MapWindow, DialogWindow, ContextMenus, ConsoleWindow,
	// ContentWindows, DialogElements, Dialogs, SurfaceDX9 or Container runs only mainwindowtest.cpp,
	// dialogwindowtest.cpp, contextmenustest.cpp, consolewindowtest.cpp,
	// contentwindowstest.cpp, dialogelementstest.cpp, tests/golden/dialogs_tests.cpp, surfacedx9test.cpp or tests/golden/container_tests.cpp
	// (they need none of the earlier tests), for quick runs while working on
	// them.
	const char *Only = getenv("DIFFTEST_ONLY");
	if (getenv("DIFFTEST_FAILS") != NULL)
		FailuresPrinted = atoi(getenv("DIFFTEST_FAILS"));
	if (Only != NULL && strcmp(Only, "MainWindow") == 0) {
		TestMainWindow();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "DialogWindow") == 0) {
		TestDialogWindow();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "ContextMenus") == 0) {
		TestContextMenus();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "ConsoleWindow") == 0) {
		TestConsoleWindow();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "ContentWindows") == 0) {
		TestContentWindows();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "DialogElements") == 0) {
		TestDialogElements();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "SurfaceDX") == 0) {
		TestSurfaceDX();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "Dialogs") == 0) {
		TestDialogs();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "Container") == 0) {
		TestContainer();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "SurfaceDX9") == 0) {
		TestSurfaceDX9();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "SurfaceGL") == 0) {
		TestSurfaceGL();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "Regexp") == 0) {
		TestRegexp();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "Errors") == 0) {
		TestErrors(TibiaExe);
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "Network") == 0) {
		TestNetwork();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "MainFrm") == 0) {
		TestMainFrm();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "MapWindow") == 0) {
		TestMapWindow(TibiaExe);
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "GUI") == 0) {
		TestGUI();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	if (Only != NULL && strcmp(Only, "Script") == 0) {
		TestScript();
		printf("%d checks, %d failures\n", Checks, Failures);
		return Failures == 0 ? 0 : 1;
	}
	TestXTEA();
	TestCharacters();
	TestColors();
	TestStrings();
	TestFormatNumber();
	TestBuffers();
	TestSkins();
	TestStatus();
	TestPreferences();
	TestScript();
	TestPreferencesFile();
	TestPlayer();
	TestHelp();
	TestInventory();
	TestTalk();
	TestObjects(TibiaExe);
	TestMap(TibiaExe);
	TestCreatures(TibiaExe);
	TestGraphics(TibiaExe);
	TestGUI();
	TestDialogWindow();
	TestContextMenus();
	TestConsoleWindow();
	TestContentWindows();
	TestDialogElements();
	TestDialogs();
	TestSurfaceDX();
	TestSurfaceDX9();
	TestSurfaceGL();
	TestContainer();
	TestRegexp();
	TestErrors(TibiaExe);
	TestNetwork();
	TestMainFrm();
	TestMapWindow(TibiaExe);
	TestMainWindow();
#endif
	golden::FinishRecording(Checks, Failures);
	printf("%d checks, %d failures\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
