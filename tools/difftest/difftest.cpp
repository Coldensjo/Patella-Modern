// Differential test: calls functions in the original Tibia.exe image and the
// reconstructed ones on the same inputs and compares the results.
//
// Tibia.exe is copied to its base address (see RunChild and MapImage): no
// imports are bound and no code runs except the functions called here. Only functions that do not
// touch imports are called. The original C runtime is not initialised, so
// statustest.cpp redirects the image's malloc and free to the test's own heap
// (PatchJump) before it calls functions that allocate.
//
// Usage: difftest.exe [path to Tibia.exe]

#include "difftest.h"
#include "../../src/Utils.h"
#include "../../src/TXTEA.h"
#include "../../src/TMyException.h"
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

// Original functions.
typedef void (__cdecl *XTEAFn)(uint32 *Block, const uint32 *Key);
typedef int (__cdecl *CharFn)(int c);
typedef void (__cdecl *HSVFn)(double H, double S, double V, int *R, int *G, int *B);
typedef void (__cdecl *OutfitFn)(int Color, int *R, int *G, int *B);
typedef void (__cdecl *StrFn)(char *s);
typedef int (__cdecl *CompareFn)(const char *s1, const char *s2, int Count);
typedef void (__cdecl *CopyFn)(char *Destination, const char *Source, int Size);
typedef void (__cdecl *TrimFn)(char *Destination, const char *Source);
typedef bool (__cdecl *ShortenFn)(char *Text, int MaxLength);
typedef void (__cdecl *FormatFn)(int Number, char *Buffer);
// thiscall members through __fastcall: ECX = this, EDX unused, rest on the stack.
typedef void (__fastcall *BufInitFn)(void *This, void *, uint8 *Data, int Size);
typedef uint32 (__fastcall *BufGetFn)(void *This, void *);
typedef void (__fastcall *BufGetStringFn)(void *This, void *, char *Buffer, int MaxLength);
typedef void (__fastcall *BufGetBytesFn)(void *This, void *, uint8 *Buffer, int Count);
typedef void (__fastcall *BufPutFn)(void *This, void *, uint32 Data);
typedef void (__fastcall *BufPutStringFn)(void *This, void *, const char *s);
typedef void (__fastcall *BufPutBytesFn)(void *This, void *, const uint8 *Buffer, int Count);
typedef void (__fastcall *XTEAKeyFn)(void *This, void *, const uint8 *Key);
typedef void (__fastcall *XTEABlockFn)(void *This, void *, uint8 *Block);

void RandomText(char *Text, int Length)
{
	static const char Chars[] = "aAzZ09 \t\n\r\f\v.,-_\xe4\xf6\xfc\xc4\xd6\xdc\xdf\xe9\xc9\xff\xc0\xde\xdf\xe0";
	for (int i = 0; i < Length; i++)
		Text[i] = Chars[Random() % (sizeof(Chars) - 1)];
	Text[Length] = 0;
}

static void TestXTEA()
{
	XTEAFn Encrypt = At<XTEAFn>(0x508cd0);
	XTEAFn Decrypt = At<XTEAFn>(0x508e00);
	XTEAKeyFn SetKey = At<XTEAKeyFn>(0x508ca0);
	XTEABlockFn EncryptBlock = At<XTEABlockFn>(0x508f60);
	XTEABlockFn DecryptBlock = At<XTEABlockFn>(0x508f80);
	for (int t = 0; t < 2000; t++) {
		uint8 Key[16];
		for (int i = 0; i < 16; i++)
			Key[i] = (uint8)Random();
		uint32 Plain[2] = {Random() ^ (Random() << 16), Random() ^ (Random() << 16)};

		uint32 Original[2] = {Plain[0], Plain[1]};
		Encrypt(Original, (const uint32 *)Key);
		TXTEA Cipher;
		Cipher.SetKey(Key);
		uint32 Ours[2] = {Plain[0], Plain[1]};
		Cipher.EncryptBlock((uint8 *)Ours);
		CHECK(Ours[0] == Original[0] && Ours[1] == Original[1], "XTEA encrypt differs");

		// The original object: vtable, then the key.
		uint32 Object[5] = {0, 0, 0, 0, 0};
		SetKey(Object, 0, Key);
		uint32 Wrapped[2] = {Plain[0], Plain[1]};
		EncryptBlock(Object, 0, (uint8 *)Wrapped);
		CHECK(Wrapped[0] == Original[0] && Wrapped[1] == Original[1], "TXTEA::EncryptBlock differs");

		Decrypt(Original, (const uint32 *)Key);
		Cipher.DecryptBlock((uint8 *)Ours);
		DecryptBlock(Object, 0, (uint8 *)Wrapped);
		CHECK(Original[0] == Plain[0] && Ours[0] == Plain[0] && Ours[1] == Plain[1] && Wrapped[0] == Plain[0],
			"XTEA decrypt differs");
	}
}

static void TestCharacters()
{
	struct { uint32 Address; int (*Ours)(int); const char *Name; } Functions[] = {
		{0x508fa0, IsSpace, "IsSpace"}, {0x508fd0, IsAlpha, "IsAlpha"}, {0x509020, IsDigit, "IsDigit"},
		{0x509040, ToLower, "ToLower"}, {0x509060, ToUpper, "ToUpper"},
	};
	for (auto &F : Functions) {
		CharFn Original = At<CharFn>(F.Address);
		for (int c = -300; c < 600; c++)
			CHECK(F.Ours(c) == Original(c), "%s(%d): %d, original %d", F.Name, c, F.Ours(c), Original(c));
	}
}

static void TestColors()
{
	HSVFn HSV = At<HSVFn>(0x509080);
	OutfitFn Outfit = At<OutfitFn>(0x509280);
	for (int Color = -5; Color < 140; Color++) {
		int r1, g1, b1, r2, g2, b2;
		GetOutfitColor(Color, &r1, &g1, &b1);
		Outfit(Color, &r2, &g2, &b2);
		CHECK(r1 == r2 && g1 == g2 && b1 == b2, "GetOutfitColor(%d): %d,%d,%d original %d,%d,%d",
			Color, r1, g1, b1, r2, g2, b2);
	}
	for (int t = 0; t < 20000; t++) {
		double H = (Random() % 1001) / 1000.0, S = (Random() % 1001) / 1000.0, V = (Random() % 1001) / 1000.0;
		if (t % 7 == 0) S = 0.0;
		if (t % 11 == 0) V = 0.0;
		int r1, g1, b1, r2, g2, b2;
		HSVToRGB(H, S, V, &r1, &g1, &b1);
		HSV(H, S, V, &r2, &g2, &b2);
		CHECK(r1 == r2 && g1 == g2 && b1 == b2, "HSVToRGB(%g,%g,%g): %d,%d,%d original %d,%d,%d",
			H, S, V, r1, g1, b1, r2, g2, b2);
	}
}

static void TestStrings()
{
	StrFn Lower = At<StrFn>(0x5093d0);
	StrFn Capitalize = At<StrFn>(0x509540);
	CompareFn Compare = At<CompareFn>(0x509710);
	CopyFn Copy = At<CopyFn>(0x509a60);
	TrimFn Trim = At<TrimFn>(0x509d10);
	ShortenFn Shorten = At<ShortenFn>(0x50a040);
	for (int t = 0; t < 5000; t++) {
		char A[64], B[64], C[64], D[64];
		int Length = Random() % 40;
		RandomText(A, Length);
		strcpy(B, A);
		LowerString(A);
		Lower(B);
		CHECK(strcmp(A, B) == 0, "LowerString differs");

		RandomText(A, Length);
		strcpy(B, A);
		CapitalizeString(A);
		Capitalize(B);
		CHECK(strcmp(A, B) == 0, "CapitalizeString differs");

		RandomText(A, Length);
		strcpy(B, A);
		if (Random() % 3 == 0)
			for (int i = 0; B[i]; i++)
				if (Random() % 4 == 0) B[i] = (char)ToUpper(B[i]);
		if (Random() % 4 == 0 && Length > 0)
			B[Random() % Length] = (char)Random();
		int Count = Random() % 45;
		CHECK(CompareStrings(A, B, Count) == Compare(A, B, Count), "CompareStrings differs");

		int Size = 1 + Random() % 50;
		memset(C, 'x', sizeof(C));
		memset(D, 'x', sizeof(D));
		CopyString(C, A, Size);
		Copy(D, A, Size);
		CHECK(memcmp(C, D, sizeof(C)) == 0, "CopyString differs");

		memset(C, 'x', sizeof(C));
		memset(D, 'x', sizeof(D));
		TrimString(C, A);
		Trim(D, A);
		CHECK(memcmp(C, D, sizeof(C)) == 0, "TrimString differs");

		strcpy(C, A);
		strcpy(D, A);
		int MaxLength = Random() % 45;
		bool r1 = ShortenString(C, MaxLength);
		bool r2 = Shorten(D, MaxLength);
		CHECK(r1 == r2 && strcmp(C, D) == 0, "ShortenString(\"%s\", %d) differs", A, MaxLength);
	}
}

static void TestFormatNumber()
{
	FormatFn Format = At<FormatFn>(0x50bce0);
	__try {
		char Probe[32];
		Format(1234567, Probe);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		printf("skip FormatNumber: the original sprintf needs the original CRT\n");
		return;
	}
	for (int t = 0; t < 2000; t++) {
		int Number = t < 1000 ? t * t * 37 : (int)(Random() & 0x7fffffff);
		char A[32], B[32];
		FormatNumber(Number, A);
		Format(Number, B);
		CHECK(strcmp(A, B) == 0, "FormatNumber(%d): \"%s\" original \"%s\"", Number, A, B);
	}
}

static void TestBuffers()
{
	BufInitFn ReadInit = At<BufInitFn>(0x50a200);
	BufGetFn GetByte = At<BufGetFn>(0x50a4b0), GetWord = At<BufGetFn>(0x50a610), GetQuad = At<BufGetFn>(0x50a780);
	BufGetStringFn GetStr = At<BufGetStringFn>(0x50a900);
	BufGetBytesFn GetBytes = At<BufGetBytesFn>(0x50ad50);
	BufInitFn WriteInit = At<BufInitFn>(0x50b030);
	BufPutFn PutByte = At<BufPutFn>(0x50b2e0), PutWord = At<BufPutFn>(0x50b450), PutQuad = At<BufPutFn>(0x50b5e0);
	BufPutStringFn PutStr = At<BufPutStringFn>(0x50b780);
	BufPutBytesFn PutBytes = At<BufPutBytesFn>(0x50ba00);

	for (int t = 0; t < 2000; t++) {
		uint8 Ours[1024], Original[1024];
		memset(Ours, 0xcd, sizeof(Ours));
		memset(Original, 0xcd, sizeof(Original));
		TWriteBuffer W(Ours, sizeof(Ours));
		uint32 WO[3];
		WriteInit(WO, 0, Original, sizeof(Original));
		// Random sequence of writes, at most ~800 bytes.
		uint32 Ops[40], Values[40];
		char Texts[40][32];
		int OpCount = Random() % 40;
		for (int i = 0; i < OpCount; i++) {
			Ops[i] = Random() % 5;
			Values[i] = Random() ^ (Random() << 16);
			RandomText(Texts[i], Random() % 20);
			switch (Ops[i]) {
			case 0: W.PutByte((uint8)Values[i]); PutByte(WO, 0, (uint8)Values[i]); break;
			case 1: W.PutWord((uint16)Values[i]); PutWord(WO, 0, (uint16)Values[i]); break;
			case 2: W.PutQuad(Values[i]); PutQuad(WO, 0, Values[i]); break;
			case 3: W.PutString(Texts[i]); PutStr(WO, 0, Texts[i]); break;
			case 4: W.PutBytes((const uint8 *)Texts[i], 7); PutBytes(WO, 0, (const uint8 *)Texts[i], 7); break;
			}
		}
		CHECK(memcmp(Ours, Original, sizeof(Ours)) == 0 && W.Position == (int)WO[2], "TWriteBuffer differs");

		TReadBuffer R(Ours, W.Position);
		uint32 RO[3];
		ReadInit(RO, 0, Original, WO[2]);
		for (int i = 0; i < OpCount; i++) {
			char A[32], B[32];
			uint8 X[8], Y[8];
			int MaxLength = (int)(Random() % 24) - 2;
			if (MaxLength == 0) MaxLength = -1;
			switch (Ops[i]) {
			case 0: CHECK(R.GetByte() == (uint8)GetByte(RO, 0), "GetByte differs"); break;
			case 1: CHECK(R.GetWord() == (uint16)GetWord(RO, 0), "GetWord differs"); break;
			case 2: CHECK(R.GetQuad() == GetQuad(RO, 0), "GetQuad differs"); break;
			case 3:
				memset(A, 'x', sizeof(A));
				memset(B, 'x', sizeof(B));
				R.GetString(A, MaxLength);
				GetStr(RO, 0, B, MaxLength);
				CHECK(memcmp(A, B, sizeof(A)) == 0, "GetString(MaxLength %d) differs", MaxLength);
				break;
			case 4:
				R.GetBytes(X, 7);
				GetBytes(RO, 0, Y, 7);
				CHECK(memcmp(X, Y, 7) == 0, "GetBytes differs");
				break;
			}
			CHECK(R.Position == (int)RO[2], "read position differs");
		}
	}
}

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
	snprintf(CommandLine, sizeof(CommandLine), "\"%s\" --child \"%s\" %lu", Self,
		argc > 1 ? argv[1] : "C:\\Tibia\\Tibia.exe", (unsigned long)(uintptr_t)Shared);
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
static void BeginTest(const char *Name)
{
	strncpy(Status->Test, Name, sizeof(Status->Test) - 1);
	Status->State = CHILD_IN_TEST;
}
static void EndTest()
{
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
	InstallCrashReports(argc > 3 ? argv[3] : NULL);
	if (!MapImage(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe"))
		return 2;
#ifdef DIFFTEST_SYSINC
	TestSysinc();
#elif defined(DIFFTEST_COMMUNICATION)
	// communicationtest.exe (issue #36): DIFFTEST_ONLY=Communication or Control runs one of its two tests.
	const char *Only = getenv("DIFFTEST_ONLY");
	if (getenv("DIFFTEST_FAILS") != NULL)
		FailuresPrinted = atoi(getenv("DIFFTEST_FAILS"));
	if (Only == NULL || strcmp(Only, "Communication") == 0)
		TestCommunication(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
	if (Only == NULL || strcmp(Only, "Control") == 0)
		TestControl(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
#else
	// DIFFTEST_ONLY=MainWindow, MapWindow, DialogWindow, ContextMenus, ConsoleWindow,
	// ContentWindows, DialogElements, Dialogs, SurfaceDX9 or Container runs only mainwindowtest.cpp,
	// dialogwindowtest.cpp, contextmenustest.cpp, consolewindowtest.cpp,
	// contentwindowstest.cpp, dialogelementstest.cpp, dialogstest.cpp, surfacedx9test.cpp or containertest.cpp
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
		TestErrors(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
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
		TestMapWindow(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
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
	TestObjects(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
	TestMap(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
	TestCreatures(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
	TestGraphics(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
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
	TestErrors(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
	TestNetwork();
	TestMainFrm();
	TestMapWindow(argc > 2 ? argv[2] : "C:\\Tibia\\Tibia.exe");
	TestMainWindow();
#endif
	printf("%d checks, %d failures\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
