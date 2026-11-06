// Differential tests of the error paths (issue #22): the assertions, TERROR,
// PROGRAM_ERROR throws and the TEND wrapping of the original code against the
// reconstructed code, through imageeh.cpp, plus TMyException's messages and
// HandleException's end (until Patella-Modern issue #74 the report it wrote
// to error.txt; since then: the same outcome as the original's, no file, the
// message on stderr). A golden-corpus test since
// Patella-Modern issue #26 (moved from tools/difftest/errortest.cpp; golden.h).
//
// What each call threw is compared by its outcome (Patella-Modern issue #76,
// golden_eh.cpp CompareThrown): thrown or not, and the kind (TMyException, a
// PROGRAM_ERROR with its value, another type). Until then the messages were
// compared in full, the line numbers too (issue #35). TestMyException still
// compares the texts of TMyException's two constructors for random arguments
// (the formatting itself, not the texts of the sites): since issue #80 ours
// are patella::ErrorText's two forms and ErrorOrigin (TMyException.h).
//
// DIFFTEST_ONLY=Errors runs this test alone; ER_TRACE=1 prints every compared
// exception, ER_SEED=<n> changes the random inputs.
//
// In the corpus (issue #26):
// - what each of the original's calls threw (ORIGINAL_THROWN: the outcome, since
//   issue #76), the texts of TestMyException's TMyExceptions, the FatalError texts (its
//   error.txt until issue #74), the counts and positions the comparisons read from it;
// - which comparisons were skipped because an earlier test redirected the
//   original's function or ours for good: the oracle finds that by decoding an
//   x86 jump at the function's start (imageeh.cpp's Redirected); the runner
//   takes it from the corpus, per comparison ("redirected");
// - the Statics round's SurfaceInitInstance part (the x86 engine selection of
//   DirectDraw, Direct3D 9 and OpenGL, which the x64 client replaces by the
//   Vulkan engine) is retired with the oracle (the user's decision of November
//   4, 2026; golden::RetiredBegin): the runner skips its checks and counts them
//   as retired.
// The time zone is UTC0 on both sides while HandleException runs (the
// original's report took the date's text from the C library's local time,
// issue #61), so the corpus does not depend on the recording machine's zone.

#include "golden_test.h"

#include "golden_eh.h"
#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#include "testsurface.h"
#else
#include "golden_patch.h"
#endif

#include "Utils.h"
#include "Player.h"
#include "Surface.h"
#include "Control.h"
#include "sysinc.h"
#include "Map.h"
#include "Creatures.h"
#include "Inventory.h"
#include "Talk.h"
#include "Objects.h"
#include "Status.h"
#include "Preferences.h"
#include "Help.h"
#include "graphics.h"
#include "Script.h"

#include <algorithm>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <vector>

namespace ErrorsTest {

// Own random numbers, so the later tests' inputs stay the same.
uint32 ErSeed = 22;
uint32 ErRandom()
{
	ErSeed = ErSeed * 1103515245 + 12345;
	return ErSeed >> 8;
}

bool Trace = false;
int Compared = 0;       // exceptions compared
int ComparedThrows = 0; // of them, calls that threw on both sides

// --- Comparing messages ---------------------------------------------------

// Compares what both sides threw (golden_eh.cpp's CompareThrown).
void Compare(const char *What, const TThrown &Ours, const TThrown &Original)
{
	Compared++;
	if (Original.Kind != TThrown::NOTHING)
		ComparedThrows++;
	if (Trace)
		printf("Errors %s: %s\n", What, Original.Describe().c_str());
	CompareThrown("Errors", What, Ours, Original);
}

// The test's work directory, %TEMP%\errortest (the script and error files).
std::string WorkDirectory()
{
	return std::string(getenv("TEMP") != nullptr ? getenv("TEMP") : ".") + GOLDEN_SLASH "errortest";
}

// Both sides as two calls, ours first; returns what the original threw (in the runner: what it threw in
// the oracle). Original runs in the oracle only. (Until issue #76 the messages' work directory was replaced by
// "%TEMP%\errortest", since TEMP differs between the recording and the replay; the texts are not compared now.)
template <class F, class G> TThrown Both2(const char *What, F Ours, [[maybe_unused]] G Original)
{
	const TThrown A = OurThrow(Ours);
	const TThrown B = golden::RecordedThrown("thrown", ORIGINAL_OR(ImageThrow(Original), TThrown()));
	Compare(What, A, B);
	return B;
}

// Ours first, then the original's (the old macro's two calls were the arguments of one call, in an
// unspecified order; they share no state).
#define COMPARE(What, OurCall, ImageCall) \
	do { \
		const TThrown Ours_ = OurThrow([&] { OurCall; }); \
		const TThrown Original_ = ORIGINAL_THROWN("thrown", ImageCall); \
		Compare(What, Ours_, Original_); \
	} while (0)

// A text of the original (the oracle's Text; the runner gets the recorded one).
std::string OriginalString(const char *Name, [[maybe_unused]] const std::string &Text)
{
	return golden::OutputText(Name, ORIGINAL_OR(Text.data(), ""), ORIGINAL_OR(Text.size(), 0));
}

// Whether a comparison is skipped: the oracle's answer (an earlier test redirected the image's function
// or ours for good, a jump at its start), recorded; the runner uses the recorded one.
bool RecordedSkip([[maybe_unused]] uint32 Address, [[maybe_unused]] const void *Ours)
{
	return ORIGINAL_VALUE("redirected", uint8, ImageRedirected(Address) || Redirected(Ours)) != 0;
}

// --- TMyException.cpp -----------------------------------------------------

#if GOLDEN_ORACLE
typedef void(__fastcall *ExceptionCtorFn)(void *This, void *, const void *File, int Line, const void *Kind,
	const void *Name1, int Value1, const void *Name2, int Value2, const void *Last);
typedef void(__fastcall *ThisFn)(void *This, void *);

struct TImageException {
	uint8 Bytes[0x3c];
	TImageException() { memset(Bytes, 0, sizeof(Bytes)); }
	~TImageException() { At<ThisFn>(0x508300)(Bytes, NULL); }
	std::string Message() const { return ImageString(Bytes + 0x04); }
	std::string Origin() const { return ImageString(Bytes + 0x20); }
};

// A VC7.1 std::string of the image for the length of a call.
struct TImageString {
	uint8 Bytes[0x1c];
	explicit TImageString(const std::string &Text) { MakeImageString(Bytes, Text.c_str()); }
	~TImageString() { FreeImageString(Bytes); }
};
#endif

std::string RandomPart(int MaxLength)
{
	static const char Chars[] = "abcXYZ019 _.,:()=-\\/\n\xe4\xdf";
	std::string Text;
	int Length = (int)(ErRandom() % (MaxLength + 1));
	for (int i = 0; i < Length; i++)
		Text += Chars[ErRandom() % (sizeof(Chars) - 1)];
	return Text;
}

std::string RandomFile()
{
	static const char *const Files[] = {
		"C:\\Tibia\\Source\\Map.cpp",
		"c:/cipsoft/client/GUI.cpp",
		"Utils.cpp",
		"..\\Container.h",
		"dir\\",
		"dir/",
		"",
		"/",
		"\\x",
		"a/b\\c.cpp",
		"TMyException.cpp",
		"C:\\a/b\\very long file name of more than sixteen.cpp",
	};
	if (ErRandom() % 4 == 0)
		return RandomPart(40);
	return Files[ErRandom() % (sizeof(Files) / sizeof(Files[0]))];
}

int RandomValue()
{
	switch (ErRandom() % 4) {
	case 0: return (int)(ErRandom() % 20) - 5;
	case 1: {
		// The old (int)(ErRandom() ^ (ErRandom() << 16)): the x86 oracle's order, the left operand first.
		const uint32 Low = ErRandom();
		return (int)(Low ^ (ErRandom() << 16));
	}
	case 2: return ErRandom() % 2 == 0 ? INT_MIN : INT_MAX;
	default: return (int)(ErRandom() % 100000);
	}
}

// The old test's declarators `std::string A = f(), B = g(), ...` and `int a = f(), b = g()` are sequenced
// in their order; kept as they were.
void TestMyException()
{
	golden::Input("seed", ErSeed);
	for (int t = 0; t < 2000; t++) {
		std::string File = RandomFile(), Kind = RandomPart(30), Name1 = ErRandom() % 2 ? RandomPart(20) : "",
					Name2 = ErRandom() % 2 ? RandomPart(20) : "", Last = ErRandom() % 2 ? RandomPart(40) : "";
		int Line = RandomValue(), Value1 = RandomValue(), Value2 = RandomValue();
		// Ours: ErrorText's two forms and ErrorOrigin (the original's constructors' formatting, issue #80), the
		// original's message and origin.
		TMyException Ours(patella::ErrorText(File, Line, Kind, Name1, Value1, Name2, Value2, Last),
			PATELLA_SOURCE_LOCATION());
		const std::string OurOrigin = patella::ErrorOrigin(File, Line);
#if GOLDEN_ORACLE
		TImageException Original;
		{
			TImageString F(File), K(Kind), N1(Name1), N2(Name2), L(Last);
			At<ExceptionCtorFn>(0x508400)(Original.Bytes, NULL, F.Bytes, Line, K.Bytes, N1.Bytes, Value1, N2.Bytes,
				Value2, L.Bytes);
		}
#endif
		const std::string Message = OriginalString("message", ORIGINAL_OR(Original.Message(), std::string()));
		const std::string Origin = OriginalString("origin", ORIGINAL_OR(Original.Origin(), std::string()));
		CHECK(Ours.GetMessage() == Message && OurOrigin == Origin,
			"TMyException(\"%s\", %d, \"%s\", ...): \"%s\" (\"%s\"), original \"%s\" (\"%s\")", File.c_str(), Line,
			Kind.c_str(), Ours.GetMessage().c_str(), OurOrigin.c_str(), Message.c_str(), Origin.c_str());

		// The wrapping constructor on both.
		std::string File2 = RandomFile(), Kind2 = RandomPart(20), Name3 = ErRandom() % 2 ? RandomPart(10) : "",
					Name4 = ErRandom() % 2 ? RandomPart(10) : "";
		int Line2 = RandomValue(), Value3 = RandomValue(), Value4 = RandomValue();
		// The wrapped exception's origin is the inner one's (the original's; ours has no origin, the message names
		// the inner site).
		TMyException OurOuter(patella::ErrorText(File2, Line2, Kind2, Name3, Value3, Name4, Value4, Ours),
			PATELLA_SOURCE_LOCATION());
#if GOLDEN_ORACLE
		TImageException OriginalOuter;
		{
			TImageString F(File2), K(Kind2), N1(Name3), N2(Name4);
			At<ExceptionCtorFn>(0x5086a0)(OriginalOuter.Bytes, NULL, F.Bytes, Line2, K.Bytes, N1.Bytes, Value3,
				N2.Bytes, Value4, Original.Bytes);
		}
#endif
		const std::string OuterMessage = OriginalString("outer message",
			ORIGINAL_OR(OriginalOuter.Message(), std::string()));
		const std::string OuterOrigin = OriginalString("outer origin", ORIGINAL_OR(OriginalOuter.Origin(), std::string()));
		CHECK(OurOuter.GetMessage() == OuterMessage && OurOrigin == OuterOrigin,
			"TMyException(\"%s\", %d, \"%s\", ..., inner): \"%s\" (\"%s\"), original \"%s\" (\"%s\")", File2.c_str(),
			Line2, Kind2.c_str(), OurOuter.GetMessage().c_str(), OurOrigin.c_str(), OuterMessage.c_str(),
			OuterOrigin.c_str());
	}
}

// --- Player.cpp: HandleException and HandleProgramError -------------------

std::string FatalText, FatalCaption; // the last FatalError call of either side
// FatalError ends the program (exit), and the original's callers rely on it:
// the code after their call is something else. The fake leaves the call
// another way, which UntilFatalError catches around the call: in the oracle an
// exception of its own (SEH; the image's code is in the call too), in the
// runner a C++ exception of its own type (our code only; under the runner's
// /EHsc an SEH exception would skip our destructors).
#if GOLDEN_ORACLE
const DWORD FATAL_ERROR_CALLED = 0xe0fa7a1e;
void GOLDEN_THISCALL FakeFatalError(GOLDEN_THIS, const char *Text, const char *Caption)
{
	FatalText = Text;
	FatalCaption = Caption;
	RaiseException(FATAL_ERROR_CALLED, EXCEPTION_NONCONTINUABLE, 0, NULL);
}
void UntilFatalErrorCall(void (*Function)(void *), void *Context)
{
	__try {
		Function(Context);
	} __except (GetExceptionCode() == FATAL_ERROR_CALLED ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
	}
}
template <class F> void UntilFatalError(F Function)
{
	UntilFatalErrorCall(ThrowThunk<F>, &Function);
}
#else
struct TFatalErrorCalled {};
void GOLDEN_THISCALL FakeFatalError(GOLDEN_THIS, const char *Text, const char *Caption)
{
	FatalText = Text;
	FatalCaption = Caption;
	throw TFatalErrorCalled();
}
template <class F> void UntilFatalError(F Function)
{
	try {
		Function();
	} catch (const TFatalErrorCalled &) {
	}
}
#endif
#if GOLDEN_ORACLE
uint32 __cdecl FakeImageTime(uint32 *Time)
{
	if (Time != NULL)
		*Time = 1149770096;
	return 1149770096;
}
#endif
__time64_t __cdecl FakeTime64(__time64_t *Time)
{
	if (Time != nullptr)
		*Time = 1149770096;
	return 1149770096;
}
#if GOLDEN_ORACLE
// The image's ctime: the UCRT's _ctime32 of its time (June 8, 2006: a one-digit day) in the form MSVC 7.1's asctime
// wrote, the day with two digits (src/compat/crt71.h). Ours formats the same time itself (FormatCRT71Time, issue
// #61) with the same local time (the test's zone, UTC0).
char *__cdecl FakeCtime(const __time32_t *Time)
{
	static char Text[26];
	if (_ctime32_s(Text, sizeof(Text), Time) != 0)
		return NULL;
	if (Text[8] == ' ')
		Text[8] = '0';
	return Text;
}
#endif
OSVERSIONINFOA FakeVersion;
BOOL FakeVersionResult;
BOOL WINAPI FakeGetVersionEx(OSVERSIONINFOA *Version)
{
	if (FakeVersionResult)
		memcpy(Version, &FakeVersion, sizeof(FakeVersion));
	return FakeVersionResult;
}
#if GOLDEN_ORACLE
void __cdecl IgnoreStream(std::ostream &) {}
#endif
const char *__cdecl FakeErrorText(int Code)
{
	static char Text[40];
	snprintf(Text, sizeof(Text), "error text %d", Code);
	return Text;
}
const char *__cdecl FakeErrorHint(int Code)
{
	static char Text[40];
	snprintf(Text, sizeof(Text), "hint %d\r\nline 2", Code);
	return Text;
}

#if GOLDEN_ORACLE
// ER_WATCHDOG=<seconds>: prints where the test thread is after that time
// (for hangs in the image's code).
HANDLE MainThread;
DWORD WINAPI Watchdog(void *Seconds)
{
	Sleep((DWORD)(uintptr_t)Seconds * 1000);
	SuspendThread(MainThread);
	CONTEXT Context;
	Context.ContextFlags = CONTEXT_FULL;
	GetThreadContext(MainThread, &Context);
	printf("watchdog: eip %08lx esp %08lx ebp %08lx\nwatchdog: stack", Context.Eip, Context.Esp, Context.Ebp);
	for (int i = 0; i < 64; i++)
		printf(" %08x", ((uint32 *)(uintptr_t)Context.Esp)[i]);
	printf("\n");
	fflush(stdout);
	TerminateProcess(GetCurrentProcess(), 5);
	return 0;
}
#endif

std::string ReadFile(const char *Name)
{
	std::string Text;
	FILE *File = fopen(Name, "rb");
	if (File == nullptr)
		return "(no file)";
	char Buffer[4096];
	size_t Count;
	while ((Count = fread(Buffer, 1, sizeof(Buffer), File)) > 0)
		Text.append(Buffer, Count);
	fclose(File);
	return Text;
}

#if GOLDEN_ORACLE
// The image's ofstream (basic_filebuf) reads the buffer pointers of its FILE
// directly (_ptr, _cnt, _base of VC7.1's FILE), so it cannot get one of our
// CRT's FILEs as script_tests.cpp's PatchScript gives it: it gets a VC7.1 FILE
// without a buffer (_ptr and _cnt 0, so every character goes through fputc or
// fwrite), which holds our FILE.
struct TImageFile {
	char *_ptr;
	int _cnt;
	char *_base;
	int _flag;
	int _file;
	int _charbuf;
	int _bufsiz;
	char *_tmpfname;
	FILE *Real;
};
TImageFile *__cdecl ImageFileOpen(const char *Name, const char *Mode)
{
	FILE *Real = fopen(Name, Mode);
	if (Real == NULL)
		return NULL;
	TImageFile *File = new TImageFile();
	File->Real = Real;
	return File;
}
int __cdecl ImageFileClose(TImageFile *File)
{
	int Result = fclose(File->Real);
	delete File;
	return Result;
}
int __cdecl ImageFilePutc(int c, TImageFile *File) { return fputc(c, File->Real); }
size_t __cdecl ImageFileWrite(const void *Buffer, size_t Size, size_t Count, TImageFile *File)
{
	return fwrite(Buffer, Size, Count, File->Real);
}
int __cdecl ImageFileFlush(TImageFile *File) { return fflush(File != NULL ? File->Real : NULL); }
int __cdecl ImageFileSeek(TImageFile *File, long Offset, int Origin) { return fseek(File->Real, Offset, Origin); }
#endif

// TextCriticalError (src/Text.h) since issue #74: the first sentence of the original's.
const char *const CriticalErrorText = "The Tibia client encountered a critical error and had to be stopped.";

// Our stderr (the C library's file descriptor 2) into a file while the object lives (issue #74: HandleException
// writes the exception's message there).
struct TStderrCapture {
	int Saved;
	explicit TStderrCapture(const char *Name)
	{
		fflush(stderr);
		Saved = _dup(2);
		const int File = _open(Name, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
		if (File >= 0) {
			_dup2(File, 2);
			_close(File);
		}
	}
	~TStderrCapture()
	{
		fflush(stderr);
		if (Saved >= 0) {
			_dup2(Saved, 2);
			_close(Saved);
		}
	}
	TStderrCapture(const TStderrCapture &) = delete;
	TStderrCapture &operator=(const TStderrCapture &) = delete;
};

void WriteEarlierReport()
{
	FILE *File = fopen("error.txt", "wb");
	fputs("earlier report\r\n", File);
	fclose(File);
}

// The code address of a member function (MSVC: a member pointer of a class without virtual bases is its
// address; the runner links without incremental-link thunks).
// On Linux (issue #91) an Itanium member pointer: the code address of a non-virtual member, then the adjustment of
// this.
template <class T> void *CodeAddress(T Member)
{
#if defined(__linux__)
	static_assert(sizeof(Member) == 2 * sizeof(void *), "an Itanium member pointer: the code address first");
#else
	static_assert(sizeof(Member) == sizeof(void *), "a member pointer that is a code address");
#endif
	void *Address;
	memcpy(&Address, &Member, sizeof(Address));
	return Address;
}

#if !GOLDEN_ORACLE
// The runner's patches of TestPlayer, undone where the oracle's DisableImageExceptions undoes its own: our
// functions to the fakes (golden::RedirectOwn), the C library's clock through the runner's import slots (ucrtbase
// is out of a rel32 jump's reach; GetVersionExA's too until issue #74, which ours no longer calls).
struct TOwnPatch {
	void *Function;
	uint8 Saved[5];
};
TOwnPatch OwnPatches[4];
int NumberOfOwnPatches = 0;
struct TImportPatch {
	void **Slot;
	void *Saved;
};
TImportPatch ImportPatches[2];
int NumberOfImportPatches = 0;

void PatchOwn(void *Function, void *Target)
{
	TOwnPatch &P = OwnPatches[NumberOfOwnPatches++];
	P.Function = Function;
	golden::SaveOwn(Function, P.Saved);
	golden::RedirectOwn(Function, Target);
}
void PatchImport(const char *Name, void *Target)
{
	void **Slot = golden::FindImport(Name);
	if (Slot == nullptr) {
		printf("golden: Errors: the runner imports no %s\n", Name);
		exit(1);
	}
	TImportPatch &P = ImportPatches[NumberOfImportPatches++];
	P.Slot = Slot;
	P.Saved = *Slot;
	golden::SetImport(Slot, Target);
}
void UndoPatches()
{
	while (NumberOfImportPatches > 0) {
		const TImportPatch &P = ImportPatches[--NumberOfImportPatches];
		golden::SetImport(P.Slot, P.Saved);
	}
	while (NumberOfOwnPatches > 0) {
		const TOwnPatch &P = OwnPatches[--NumberOfOwnPatches];
		golden::RestoreOwn(P.Function, P.Saved);
	}
}
#endif

// The time zone of HandleException's reports: UTC0 on both sides (our FormatCRT71Time and the image's ctime
// fake take the C library's local time), the earlier one back afterwards.
struct TUtcZone {
	std::string Saved;
	bool Had;
	TUtcZone()
	{
		const char *Old = getenv("TZ");
		Had = Old != nullptr;
		if (Had)
			Saved = Old;
		_putenv_s("TZ", "UTC0");
		_tzset();
	}
	~TUtcZone()
	{
		_putenv_s("TZ", Had ? Saved.c_str() : "");
		_tzset();
	}
	TUtcZone(const TUtcZone &) = delete;
	TUtcZone &operator=(const TUtcZone &) = delete;
};

void TestPlayer()
{
	golden::Input("seed", ErSeed);
#if GOLDEN_ORACLE
	PatchScript(); // sprintf (num_put) and the rest of the image's stdio
	PatchImageJump(0x51227f, (void *)ImageFileOpen);
	PatchImageJump(0x512122, (void *)ImageFileClose);
	PatchImageJump(0x5133da, (void *)ImageFilePutc);
	PatchImageJump(0x51338e, (void *)ImageFileWrite);
	PatchImageJump(0x5131de, (void *)ImageFileFlush);
	PatchImageJump(0x5134e3, (void *)ImageFileSeek);
	PatchImageJump(0x4d7d80, (void *)FakeFatalError);
	PatchOwnCode(CodeAddress(&CPlayerApp::FatalError), (void *)FakeFatalError);
	PatchImageJump(0x5121ea, (void *)FakeImageTime);
	PatchOwnCode((void *)_time64, (void *)FakeTime64);
	PatchImageJump(0x5121d1, (void *)FakeCtime);
	PatchImageJump(0x445900, (void *)IgnoreStream); // OutputControlErrorInformations (Control.cpp; ours is gone, #74)
	PatchImageJump(0x4fcbf0, (void *)FakeErrorText);
	PatchOwnCode((void *)GetErrorText, (void *)FakeErrorText);
	PatchImageJump(0x4fcc90, (void *)FakeErrorHint);
	PatchOwnCode((void *)GetErrorHint, (void *)FakeErrorHint);
	BindImageImport("KERNEL32.dll", "GetVersionExA", (void *)FakeGetVersionEx); // ours calls it no more (#74)
#else
	PatchOwn(CodeAddress(&CPlayerApp::FatalError), (void *)FakeFatalError);
	PatchOwn((void *)GetErrorText, (void *)FakeErrorText);
	PatchOwn((void *)GetErrorHint, (void *)FakeErrorHint);
	PatchImport("_time64", (void *)FakeTime64);
#endif
	const TUtcZone Zone;

	char Directory[MAX_PATH];
	if (_getcwd(Directory, sizeof(Directory)) == nullptr)
		Directory[0] = 0;
	const std::string Work = WorkDirectory();
	_mkdir(Work.c_str());
	if (_chdir(Work.c_str()) != 0)
		printf("note: Errors: cannot enter %s\n", Work.c_str());
	alignas(16) static uint8 OurApp[sizeof(CPlayerApp) + 16];
#if GOLDEN_ORACLE
	static uint8 ImageApp[0x200];
#endif
	for (int t = 0; t < 300; t++) {
		std::string File = RandomFile(), Kind = RandomPart(30), Name1 = ErRandom() % 2 ? RandomPart(20) : "",
					Last = RandomPart(60);
		int Line = RandomValue(), Value1 = RandomValue();
		TMyException Ours(patella::ErrorText(File, Line, Kind, Name1, Value1, "", 0, Last), PATELLA_SOURCE_LOCATION());
#if GOLDEN_ORACLE
		TImageException Original;
		{
			TImageString F(File), K(Kind), N1(Name1), N2(""), L(Last);
			At<ExceptionCtorFn>(0x508400)(Original.Bytes, NULL, F.Bytes, Line, K.Bytes, N1.Bytes, Value1, N2.Bytes, 0,
				L.Bytes);
		}
#endif
		memset(&FakeVersion, 0, sizeof(FakeVersion));
		FakeVersion.dwOSVersionInfoSize = sizeof(FakeVersion);
		FakeVersion.dwMajorVersion = ErRandom() % 11;
		FakeVersion.dwMinorVersion = ErRandom() % 4;
		FakeVersion.dwBuildNumber = ErRandom() % 30000;
		FakeVersion.dwPlatformId = ErRandom() % 3;
		// At most 29 characters: with 30 or more the original prints its
		// 30-byte buffer without a terminator (whatever follows on the stack).
		std::string Pack = ErRandom() % 3 == 0 ? "" : RandomPart(29);
		snprintf(FakeVersion.szCSDVersion, sizeof(FakeVersion.szCSDVersion), "%s", Pack.c_str());
		FakeVersionResult = ErRandom() % 8 != 0;
		int Engine = (int)(ErRandom() % 5) - 1;
		bool Terminating = ErRandom() % 10 == 0;
		bool Existing = ErRandom() % 3 == 0; // append to an earlier report

		std::string OriginalText, OriginalCaption;
		[[maybe_unused]] bool OriginalTerminating = false;
#if GOLDEN_ORACLE
		memset(ImageApp, 0, sizeof(ImageApp));
		ImageApp[0xa0] = Terminating;
		Global<int>(0x71d0c4) = Engine; // curEngine
		remove("error.txt");
		if (Existing)
			WriteEarlierReport();
		if (Trace)
			printf("Errors: HandleException %d\n", t);
		FatalText = FatalCaption = "(none)";
		UntilFatalError([&] { At<void(__fastcall *)(void *, void *, void *)>(0x4d9bd0)(ImageApp, NULL, Original.Bytes); });
		OriginalText = FatalText;
		OriginalCaption = FatalCaption;
		OriginalTerminating = ImageApp[0xa0] != 0;
#endif
		OriginalText = OriginalString("fatal text", OriginalText);
		OriginalCaption = OriginalString("fatal caption", OriginalCaption);
		OriginalTerminating = golden::Recorded<uint8>("terminating", OriginalTerminating ? 1 : 0) != 0;

		// Ours writes no error.txt (Patella-Modern issue #74, decision D2): an earlier file stays as it was, none is
		// made; the exception's message goes to stderr instead (none when a fatal error is already being shown).
		memset(OurApp, 0, sizeof(OurApp));
		CPlayerApp *App = (CPlayerApp *)OurApp;
		App->Terminating = Terminating;
		curEngine = Engine;
		remove("error.txt");
		if (Existing)
			WriteEarlierReport();
		const std::string FileBefore = ReadFile("error.txt");
		FatalText = FatalCaption = "(none)";
		std::string OurStderr;
		{
			const TStderrCapture Capture("stderr.txt");
			UntilFatalError([&] { App->HandleException(Ours); });
		}
		OurStderr = ReadFile("stderr.txt");
		OurStderr.erase(std::remove(OurStderr.begin(), OurStderr.end(), '\r'), OurStderr.end());
		remove("stderr.txt");
		const std::string OurFile = ReadFile("error.txt");
		// Through the developer log since issue #77, at the error level: its line, "[error] <message>".
		const std::string ExpectedStderr = Terminating ? "" : "[error] Debug Assertion 7.72 " + Ours.GetMessage() + "\n";
		CHECK(OurFile == FileBefore && OurStderr == ExpectedStderr,
			"HandleException (issue #74): error.txt\n%s\nbefore\n%s\nstderr \"%s\", expected \"%s\"", OurFile.c_str(),
			FileBefore.c_str(), OurStderr.c_str(), ExpectedStderr.c_str());
		// The outcome is the original's: both end in FatalError with its caption, or neither does (Terminating);
		// the text is the first sentence of the original's (issue #74: the rest named the error file).
		const bool OriginalCalled = OriginalText != "(none)", OursCalled = FatalText != "(none)";
		CHECK(OursCalled == OriginalCalled && App->Terminating == OriginalTerminating
				&& (!OursCalled
					|| (FatalCaption == OriginalCaption && FatalText == CriticalErrorText
						&& OriginalText.compare(0, FatalText.size(), FatalText) == 0
						&& OriginalText.size() > FatalText.size())),
			"HandleException: FatalError(\"%s\", \"%s\"), original (\"%s\", \"%s\")", FatalText.c_str(),
			FatalCaption.c_str(), OriginalText.c_str(), OriginalCaption.c_str());
		if (Trace && t < 3)
			printf("Errors: stderr\n%s\n", OurStderr.c_str());

		// HandleProgramError: the message of FatalError.
		int Code = (int)(ErRandom() % 20) - 2;
#if GOLDEN_ORACLE
		memset(ImageApp, 0, sizeof(ImageApp));
		ImageApp[0xa0] = Terminating;
		FatalText = FatalCaption = "(none)";
		UntilFatalError([&] { At<void(__fastcall *)(void *, void *, int)>(0x4d9910)(ImageApp, NULL, Code); });
		OriginalText = FatalText;
		OriginalCaption = FatalCaption;
#endif
		OriginalText = OriginalString("program error text", OriginalText);
		OriginalCaption = OriginalString("program error caption", OriginalCaption);
		App->Terminating = Terminating;
		FatalText = FatalCaption = "(none)";
		UntilFatalError([&] { App->HandleProgramError(static_cast<PROGRAM_ERROR>(Code)); });
		CHECK(FatalText == OriginalText && FatalCaption == OriginalCaption,
			"HandleProgramError(%d): FatalError(\"%s\", \"%s\"), original (\"%s\", \"%s\")", Code, FatalText.c_str(),
			FatalCaption.c_str(), OriginalText.c_str(), OriginalCaption.c_str());
	}
	remove("error.txt");
	if (_chdir(Directory) != 0)
		printf("note: Errors: cannot go back to %s\n", Directory);
}

// --- Utils.cpp --------------------------------------------------------------

#if GOLDEN_ORACLE
typedef void(__cdecl *StrFn)(char *s);
typedef int(__cdecl *CompareFn)(const char *s1, const char *s2, int Count);
typedef void(__cdecl *CopyFn)(char *Destination, const char *Source, int Size);
typedef void(__cdecl *TrimFn)(char *Destination, const char *Source);
typedef bool(__cdecl *ShortenFn)(char *Text, int MaxLength);
typedef void(__cdecl *FormatFn)(int Number, char *Buffer);
typedef void(__fastcall *BufInitFn)(void *This, void *, uint8 *Data, int Size);
typedef uint32(__fastcall *BufGetFn)(void *This, void *);
typedef void(__fastcall *BufGetStringFn)(void *This, void *, char *Buffer, int MaxLength);
typedef char *(__fastcall *BufNewStringFn)(void *This, void *);
typedef void(__fastcall *BufGetBytesFn)(void *This, void *, uint8 *Buffer, int Count);
typedef void(__fastcall *BufPutFn)(void *This, void *, uint32 Data);
typedef void(__fastcall *BufPutStringFn)(void *This, void *, const char *s);
typedef void(__fastcall *BufPutBytesFn)(void *This, void *, const uint8 *Buffer, int Count);
typedef void(__fastcall *BufSetSizeFn)(void *This, void *, int NewSize);
#endif

void TestUtils()
{
	golden::Input("seed", ErSeed);
	char A[64] = "Text";
	ORIGINAL(char B[64] = "Text");
	COMPARE("LowerString(NULL)", LowerString(nullptr), At<StrFn>(0x5093d0)(NULL));
	COMPARE("CapitalizeString(NULL)", CapitalizeString(nullptr), At<StrFn>(0x509540)(NULL));
	COMPARE("CompareStrings(NULL, s)", CompareStrings(nullptr, A, 3), At<CompareFn>(0x509710)(NULL, B, 3));
	COMPARE("CompareStrings(s, NULL)", CompareStrings(A, nullptr, 3), At<CompareFn>(0x509710)(B, NULL, 3));
	COMPARE("CopyString(NULL, s)", CopyString(OriginalSpan<char>(nullptr, 3), A), At<CopyFn>(0x509a60)(NULL, B, 3));
	COMPARE("CopyString(s, NULL)", CopyString(OriginalSpan<char>(A, 3), nullptr), At<CopyFn>(0x509a60)(B, NULL, 3));
	COMPARE("TrimString(NULL, s)", TrimString(nullptr, A), At<TrimFn>(0x509d10)(NULL, B));
	COMPARE("TrimString(s, NULL)", TrimString(A, nullptr), At<TrimFn>(0x509d10)(B, NULL));
	COMPARE("ShortenString(NULL)", ShortenString(nullptr, 5), At<ShortenFn>(0x50a040)(NULL, 5));
	COMPARE("FormatNumber(n, NULL)", FormatNumber(5, OriginalSpan<char>(nullptr, 0)), At<FormatFn>(0x50bce0)(5, NULL));
	for (int t = 0; t < 50; t++) {
		int Number = -(int)(ErRandom() % 100000) - 1;
		if (t == 0)
			Number = INT_MIN;
		COMPARE("FormatNumber(negative)", FormatNumber(Number, A), At<FormatFn>(0x50bce0)(Number, B));
	}

	// The buffers at random positions near their ends, in both orders of
	// the checks (the pointer first).
	uint8 Data[64];
	ORIGINAL(uint8 OriginalData[64]);
	for (int i = 0; i < 64; i++) {
		Data[i] = (uint8)ErRandom();
		ORIGINAL(OriginalData[i] = Data[i]);
	}
	COMPARE("TReadBuffer(NULL)", TReadBuffer(OriginalSpan<uint8>(nullptr, 4)), uint32 O[3]; At<BufInitFn>(0x50a200)(O, NULL, NULL, 4));
	COMPARE("TReadBuffer(size<0)", TReadBuffer(OriginalSpan(Data, -1)), uint32 O[3];
		At<BufInitFn>(0x50a200)(O, NULL, OriginalData, -1));
	COMPARE("TWriteBuffer(NULL)", TWriteBuffer(OriginalSpan<uint8>(nullptr, 4)), uint32 O[3]; At<BufInitFn>(0x50b030)(O, NULL, NULL, 4));
	COMPARE("TWriteBuffer(size<0)", TWriteBuffer(OriginalSpan(Data, -7)), uint32 O[3];
		At<BufInitFn>(0x50b030)(O, NULL, OriginalData, -7));
	for (int t = 0; t < 400; t++) {
		int Size = (int)(ErRandom() % 12);
		int Position = (int)(ErRandom() % 14);
		if (Position > Size + 1)
			Position = Size;
		int Operation = (int)(ErRandom() % 13);
		int Count = (int)(ErRandom() % 8) - 1;
		int MaxLength = (int)(ErRandom() % 10) - 2;
		Data[Position % 64] = (uint8)(ErRandom() % 12); // a string length
		Data[(Position + 1) % 64] = 0;
		ORIGINAL(OriginalData[Position % 64] = Data[Position % 64]; OriginalData[(Position + 1) % 64] = 0);
		TReadBuffer R(OriginalSpan(Data, Size));
		TWriteBuffer W(OriginalSpan(Data, Size));
		R.Position = W.Position = Position;
		ORIGINAL(uint32 O[3] = {(uint32)(uintptr_t)OriginalData, (uint32)Size, (uint32)Position});
		char Text[16];
		uint8 Bytes[16];
		ORIGINAL(char OriginalText[16]; uint8 OriginalBytes[16]);
		const char *Put = Count < 0 ? nullptr : &"abcdefg"[7 - Count];
		char What[64];
		snprintf(What, sizeof(What), "buffer operation %d (size %d, position %d, count %d)", Operation, Size, Position,
			Count);
		switch (Operation) {
		case 0: COMPARE(What, R.GetByte(), At<BufGetFn>(0x50a4b0)(O, NULL)); break;
		case 1: COMPARE(What, R.GetWord(), At<BufGetFn>(0x50a610)(O, NULL)); break;
		case 2: COMPARE(What, R.GetQuad(), At<BufGetFn>(0x50a780)(O, NULL)); break;
		case 3:
			COMPARE(What, R.GetString(OriginalSpan<char>(Count < 0 ? nullptr : Text, MaxLength)),
				At<BufGetStringFn>(0x50a900)(O, NULL, Count < 0 ? NULL : OriginalText, MaxLength));
			break;
		case 4: COMPARE(What, delete[] R.GetString(), At<BufNewStringFn>(0x50abb0)(O, NULL)); break;
		case 5:
			COMPARE(What, R.GetBytes(OriginalSpan<uint8>(Count == 0 ? nullptr : Bytes, Count < 0 ? 3 : Count)),
				At<BufGetBytesFn>(0x50ad50)(O, NULL, Count == 0 ? NULL : OriginalBytes, Count < 0 ? 3 : Count));
			break;
		case 6: COMPARE(What, W.PutByte(1), At<BufPutFn>(0x50b2e0)(O, NULL, 1)); break;
		case 7: COMPARE(What, W.PutWord(2), At<BufPutFn>(0x50b450)(O, NULL, 2)); break;
		case 8: COMPARE(What, W.PutQuad(3), At<BufPutFn>(0x50b5e0)(O, NULL, 3)); break;
		case 9: COMPARE(What, W.PutString(Put), At<BufPutStringFn>(0x50b780)(O, NULL, Put)); break;
		case 10:
			COMPARE(What, W.PutBytes(OriginalSpan<const uint8>(Count == 0 ? nullptr : Bytes, Count < 0 ? 3 : Count)),
				At<BufPutBytesFn>(0x50ba00)(O, NULL, Count == 0 ? NULL : OriginalBytes, Count < 0 ? 3 : Count));
			break;
		default: COMPARE(What, R.SetSize(Count), At<BufSetSizeFn>(0x4ca300)(O, NULL, Count)); break;
		}
		const int OriginalPosition = ORIGINAL_VALUE("position", int, (int)O[2]);
		CHECK(R.Position == OriginalPosition || W.Position == OriginalPosition, "%s: position differs", What);
	}
}

// --- Calls with arguments that fail the leading assertions ----------------

// Calls our function and the image's at Address (cdecl) with the same
// arguments, converted to our parameter types.
// Earlier tests redirect some functions for good (creatures_tests.cpp the
// image's and our GetFirstObject, CheckEventHelpName, ...): a function that
// starts with a jump now is a test's fake. Such comparisons are skipped and
// counted (the full run reports them; DIFFTEST_ONLY=Errors has none). Which
// ones is the oracle's answer, recorded (RecordedSkip).
int Skipped = 0;

template <class R, class... A, class... B>
void Both(const char *What, [[maybe_unused]] uint32 Address, R (*Ours)(A...), B &&...Arguments)
{
	if (RecordedSkip(Address, (const void *)Ours)) {
		Skipped++;
		return;
	}
	if (Trace)
		printf("Errors: calling %s\n", What);
	const TThrown OurResult = OurThrow([&] { Ours((A)Arguments...); });
	if (Trace)
		printf("Errors: calling the original %s\n", What);
	const TThrown OriginalResult = ORIGINAL_THROWN("thrown", At<R(__cdecl *)(A...)>(Address)((A)Arguments...));
	Compare(What, OurResult, OriginalResult);
}

// A value outside Min..Max (the edges, near them, the extremes).
int Outside(int Min, int Max)
{
	switch (ErRandom() % 5) {
	case 0: return Min - 1;
	case 1: return Max + 1;
	case 2: return Min - 2 - (int)(ErRandom() % 1000);
	case 3: return Max + 2 + (int)(ErRandom() % 1000);
	default: return ErRandom() % 2 == 0 ? INT_MIN : INT_MAX;
	}
}
int Inside(int Min, int Max)
{
	return Min + (int)(ErRandom() % (uint32)(Max - Min + 1));
}
// Out of range for the parameter to fail, in range for the others.
void Coordinates(int *x, int *y, int *z, int MaxX, int MaxY, int MaxZ)
{
	int Bad = (int)(ErRandom() % 3);
	*x = Bad == 0 ? Outside(0, MaxX) : Inside(0, MaxX);
	*y = Bad == 1 ? Outside(0, MaxY) : Inside(0, MaxY);
	*z = Bad == 2 ? Outside(0, MaxZ) : Inside(0, MaxZ);
}
const char *LongText(int Length)
{
	static char Text[400];
	memset(Text, 'x', (size_t)Length);
	Text[Length] = 0;
	return Text;
}

// The calls below take arguments in the old test's expressions: where one call's arguments call
// ErRandom() more than once (Outside, Inside, LongText's length), the old call evaluated them right to left
// (x86 MSVC); each is a statement of its own now, in that order (#55).
void TestMap()
{
	golden::Input("seed", ErSeed);
	typedef TObject (*ObjectAt)(int, int, int);
	for (int t = 0; t < 30; t++) {
		int x, y, z, Position = 0;
		Coordinates(&x, &y, &z, MAPSIZE_X - 1, MAPSIZE_Y - 1, MAPSIZE_Z - 1);
		Both("GetFirstObject", 0x4b6140, (ObjectAt)GetFirstObject, x, y, z);
		Both("GetMapObject", 0x4b6580, GetMapObject, x, y, z, 0);
		Both("GetTopMoveObject", 0x4b6a00, GetTopMoveObject, x, y, z, Position);
		Both("GetTopLookObject", 0x4b6ee0, GetTopLookObject, x, y, z, Position);
		Both("GetTopUseObject", 0x4b73a0, GetTopUseObject, x, y, z, Position);
		Both("GetTopMultiUseObject", 0x4b78d0, GetTopMultiUseObject, x, y, z, Position);
		Both("AddObject", 0x4b88f0, AddObject, x, y, z, TObject());
		Both("AppendObject", 0x4b8ec0, AppendObject, x, y, z, TObject());
		Both("DeleteObject", 0x4b9420, (bool (*)(int, int, int, int))DeleteObject, x, y, z, 0); // not GDI's DeleteObject
		Both("ChangeObject", 0x4b9940, ChangeObject, x, y, z, 0, TObject());
		Both("GetFirstEffect", 0x4ba100, GetFirstEffect, x, y, z);
		Both("ClearField", 0x4bc720, ClearField, x, y, z, false);
		int Effect = Outside(0, EFFECTS - 1);
		Both("GetEffectStatus", 0x4b1440, GetEffectStatus, Effect);
		Both("GetGraphicalEffectType", 0x4b15a0, GetGraphicalEffectType, Effect);
		Both("GetGraphicalEffectPhase", 0x4b1860, GetGraphicalEffectPhase, Effect);
		Both("GetTextualEffectColor", 0x4b1b20, GetTextualEffectColor, Effect);
		Both("GetTextualEffectText", 0x4b1de0, GetTextualEffectText, Effect);
		Both("GetTextualEffectPhase", 0x4b20a0, GetTextualEffectPhase, Effect);
		Both("GetMissileEffectType", 0x4b2360, GetMissileEffectType, Effect);
		Both("GetMissileEffectDirection", 0x4b2620, GetMissileEffectDirection, Effect);
		Both("GetMissileEffectX", 0x4b28e0, GetMissileEffectX, Effect);
		Both("GetMissileEffectY", 0x4b2bb0, GetMissileEffectY, Effect);
		Both("AddTextualEffect", 0x4bad00, AddTextualEffect, 1, 2, 3, 4, t % 2 == 0 ? nullptr : LongText(10 + t));
		const int Missile = -(int)(ErRandom() % 5);
		Both("AddMissileEffect", 0x4bb0c0, AddMissileEffect, 1, 2, 3, 4, 5, 6, Missile);
		int Floor = Outside(0, 15);
		Both("GetAutomapColor", 0x4b4370, GetAutomapColor, 100, 200, Floor);
		int Count = 0;
		Both("GetAutomapMarks", 0x4b4680, GetAutomapMarks, 100, 200, Floor, (vector<TMark> *)nullptr, &Count);
		TCoordinate Coordinate;
		memset(&Coordinate, 0, sizeof(Coordinate));
		Coordinate.x = t % 3 == 0 ? 100 : -(int)(ErRandom() % 3);
		Coordinate.y = t % 3 == 1 ? 100 : -(int)(ErRandom() % 3);
		Both("AddAutomapMark", 0x4b4f60, AddAutomapMark, Coordinate, t % 4 == 0 ? nullptr : "mark", 1);
	}
}

void TestCreatures()
{
	golden::Input("seed", ErSeed);
	for (int t = 0; t < 30; t++) {
		int Skill = Outside(0, SKILLS - 1);
		Both("GetPlayerSkillValue", 0x44d1c0, GetPlayerSkillValue, Skill);
		Both("GetPlayerSkillProgress", 0x44d320, GetPlayerSkillProgress, Skill);
		Both("GetSkillName", 0x44d480, GetSkillName, Skill);
		{
			// The old call's arguments right to left: Outside(0, 100) first.
			const int Progress = Outside(0, 100);
			const int Number = Inside(0, SKILLS - 1);
			Both("SetPlayerSkill", 0x44cf10, SetPlayerSkill, Number, 10, Progress);
		}
		{
			const int Progress = Outside(-1, 100);
			Both("SetPlayerExperience", 0x44cc00, SetPlayerExperience, 100, 2, Progress);
		}
		Both("AddCreature", 0x44c530, AddCreature, 5, 0);
		Both("GetCreature", 0x44c6e0, GetCreature, 0);
		Both("CheckMonsterHint", 0x451db0, CheckMonsterHint, (TCreature *)nullptr);
		Both("IsOpponent", 0x44d740, IsOpponent, (TCreature *)nullptr);
		{
			const int Opponent = Outside(0, INT_MAX - 3) < 0 ? -1 - (int)(ErRandom() % 9) : INT_MIN;
			Both("GetOpponent", 0x44dc70, GetOpponent, Opponent);
		}
		int Buddy = (int)(ErRandom() % 50);
		Both("CompareBuddies", 0x44de20, CompareBuddies, Buddy, Buddy);
		Both("SwapBuddies", 0x44e260, SwapBuddies, Buddy, Buddy);
		Both("AddBuddy", 0x44eab0, AddBuddy, 7, t % 2 == 0 ? nullptr : LongText(30 + t), true);
		Both("EditBuddy", 0x44f230, EditBuddy, t % 2 == 0 ? 0 : 7, 1, (const char *)nullptr, false);
	}
}

void TestInventory()
{
	golden::Input("seed", ErSeed);
	for (int t = 0; t < 30; t++) {
		int Container = Outside(0, OPEN_CONTAINERS - 1);
		Both("OpenContainer", 0x4a4b00, OpenContainer, Container, TObject(), "bag", 8, false);
		Both("CloseContainer", 0x4a4f70, CloseContainer, Container);
		Both("IsContainerOpen", 0x4a5220, IsContainerOpen, Container);
		Both("InsertContainerObject", 0x4a7f00, InsertContainerObject, Container, TObject());
		Both("AppendContainerObject", 0x4a5380, AppendContainerObject, Container, TObject());
		Both("ChangeContainerObject", 0x4a5630, ChangeContainerObject, Container, 0, TObject());
		Both("DeleteContainerObject", 0x4a58e0, DeleteContainerObject, Container, 0);
		Both("GetContainerObjectCount", 0x4a5bd0, GetContainerObjectCount, Container);
		Both("GetContainerObject", 0x4a5e80, GetContainerObject, Container);
		Both("GetContainerContent", 0x4a6150, GetContainerContent, Container, 0);
		Both("GetContainerName", 0x4a64e0, GetContainerName, Container);
		Both("GetContainerCapacity", 0x4a6790, GetContainerCapacity, Container);
		Both("GetContainerHasParent", 0x4a6a40, GetContainerHasParent, Container);
	}
}

void TestTalk()
{
	golden::Input("seed", ErSeed);
	alignas(8) static uint8 NoList[64]; // never read: the assertion comes first
	vector<TTalkFilter *> &List = *(vector<TTalkFilter *> *)NoList;
	for (int t = 0; t < 30; t++) {
		{
			const int Number = Outside(0, TALKBOXES - 1);
			Both("RemoveTalkbox", 0x4fe960, RemoveTalkbox, Number);
		}
		int Mode = t % 3 == 0 ? 0 : (t % 3 == 1 ? 100 + t : -t);
		Both("AddTalkbox", 0x5052e0, AddTalkbox, Mode, "Name", 1, 2, 3, "text");
		Both("AddMessageTalkbox", 0x505ac0, AddMessageTalkbox, t % 2 == 0 ? TALK_SAY : (int)TALK_INFO_MESSAGE,
			t % 2 == 0 ? "text" : nullptr);
		Both("FindTextBuffer", 0x4ffb90, FindTextBuffer, (TTalkBuffer *)nullptr, List);
		Both("AddConsoleText", 0x5061f0, AddConsoleText, 1, (const char *)nullptr, TALK_SAY, "Name", 0, 0);
		Both("ClearTextBuffer", 0x502340, ClearTextBuffer, (TTalkBuffer *)nullptr);
	}
}

void TestObjects()
{
	golden::Input("seed", ErSeed);
	for (int t = 0; t < 30; t++) {
		{
			const int Phase = Outside(0, 7);
			Both("GetMissileSpriteNumber", 0x4d6460, GetMissileSpriteNumber, 1, 0, 0, 0, Phase);
		}
		Both("LoadObjects", 0x4d7380, LoadObjects, (const char *)nullptr);
	}
}

void TestStatus()
{
	golden::Input("seed", ErSeed);
	for (int t = 0; t < 30; t++) {
		int Right = Outside(0, RIGHT_ANY - 1);
		Both("SetRight", 0x4e8580, SetRight, Right, 1);
		Both("CheckRight", 0x4e86e0, CheckRight, Right, 1);
		int Type = t % 2 == 0 ? -1 - (int)(ErRandom() % 5) : 1000 + t;
		Both("AddMessage", 0x4e9020, AddMessage, Type, 0, 0);
		Both("AddMessageFirst", 0x4e91d0, AddMessageFirst, Type, 0, 0);
		Both("SetAccount", 0x4e93d0, SetAccount, 1, t % 2 == 0 ? nullptr : LongText(30 + t));
		int Character = -1 - (int)(ErRandom() % 9);
		Both("SetCharacter", 0x4e96d0, SetCharacter, Character, "Name", "World", (const uint8 *)"\1\2\3\4", 7171);
		Both("SetActiveCharacter", 0x4e9c50, SetActiveCharacter, Character);
		Both("GetCharacterName", 0x4e9db0, GetCharacterName, Character);
		Both("GetCharacterWorld", 0x4e9f20, GetCharacterWorld, Character);
		Both("GetCharacterIPAddress", 0x4ea090, GetCharacterIPAddress, Character);
		Both("GetCharacterPort", 0x4ea200, GetCharacterPort, Character);
		Both("CopyToClipboard", 0x4ea5b0, CopyToClipboard, (const char *)nullptr);
		Both("InitStatus", 0x4ea860, InitStatus, t % 2 == 0, (void (*)()) nullptr);
	}
}

void TestPreferences()
{
	golden::Input("seed", ErSeed);
	bool AutoSend = false;
	for (int t = 0; t < 30; t++) {
		int Hotkey = Outside(0, HOTKEYS - 1);
		{
			// The old call's arguments right to left: the text, then the number.
			const char *Text = t % 3 == 1 ? nullptr : LongText(t % 3 == 0 ? 5 : 300);
			const int Number = t % 3 == 0 ? Hotkey : Inside(0, HOTKEYS - 1);
			Both("SetHotkey", 0x4dc450, SetHotkey, Number, Text, false);
		}
		Both("GetHotkey", 0x4dc7e0, GetHotkey, Hotkey, &AutoSend);
		int Negative = -1 - (int)(ErRandom() % 100);
		Both("AddOpenDialog", 0x4dc950, AddOpenDialog, Negative);
		Both("SetMotD", 0x4dcac0, SetMotD, (const char *)nullptr);
		Both("AddOpenChannel", 0x4dccb0, AddOpenChannel, Negative);
		Both("FindBuddy", 0x4dce20, FindBuddy, 0);
		Both("SetBuddy", 0x4dcfb0, SetBuddy, t % 3 == 0 ? 0 : 5, 1, t % 3 == 1 ? nullptr : LongText(200 + t), false);
		Both("DeleteBuddy", 0x4dd360, DeleteBuddy, 0);
		Both("GetBuddyIcon", 0x4dd4c0, GetBuddyIcon, 0);
		Both("GetBuddyDescription", 0x4dd640, GetBuddyDescription, 0);
		Both("GetBuddyNotify", 0x4dd7c0, GetBuddyNotify, 0);
		int Skill = Outside(0, SKILLS - 1);
		Both("GetSkillBar", 0x4dd940, GetSkillBar, Skill);
		Both("SetSkillBar", 0x4ddaa0, SetSkillBar, Skill, true);
	}
}

void TestHelp()
{
	golden::Input("seed", ErSeed);
	for (int t = 0; t < 30; t++) {
		int Number = Outside(1, EVENTHELPS);
		Both("AddEventHelp", 0x4a32e0, AddEventHelp, Number);
		Both("CheckEventHelpName", 0x4a3460, CheckEventHelpName, (const char *)nullptr);
		Both("GetEventHelpTitle", 0x4a3b60, GetEventHelpTitle, Number);
		Both("GetEventHelpText", 0x4a3fd0, GetEventHelpText, Number);
	}
}

void TestGraphics()
{
	golden::Input("seed", ErSeed);
	// The counts the assertions print, the same on both sides for the test.
	struct {
		int *Ours;
		uint32 Original;
		int Value;
	} Counts[] = {
		{&NumberOfFonts, 0x5cc750, 4},
		{&NumberOfPictures, 0x5cc754, 3},
		{&NumberOfSprites, 0x5ccb60, 2000},
		{&MemoryCacheSize, 0x5ccf70, 500},
		{&VideoCacheSize, 0x5ccf80, 300},
	};
	int Saved[5][2];
	for (int i = 0; i < 5; i++) {
		Saved[i][0] = *Counts[i].Ours;
		ORIGINAL(Saved[i][1] = Global<int>(Counts[i].Original));
		*Counts[i].Ours = Counts[i].Value;
		ORIGINAL(Global<int>(Counts[i].Original) = Counts[i].Value);
	}
	uint8 Buffer[16];
	for (int t = 0; t < 30; t++) {
		Both("LoadSprites", 0x48b550, LoadSprites, t % 2 == 0 ? nullptr : LongText(260 + t));
		Both("LoadPictures", 0x48bea0, LoadPictures, t % 2 == 0 ? nullptr : LongText(260 + t));
		int Sprite = Outside(1, 2000);
		Both("LoadSprite", 0x48b9a0, LoadSprite, t % 2 == 0 ? Sprite : 5, t % 2 == 0 ? Buffer : nullptr);
		{
			const int Picture = t % 2 == 0 ? Outside(1, 3) : 2;
			Both("LoadPictureTile", 0x48c3b0, LoadPictureTile, Picture, 0, 0, t % 2 == 0 ? Buffer : nullptr);
		}
		{
			const int Item = Outside(0, 499);
			Both("TouchMemoryItem", 0x48cc30, TouchMemoryItem, Item);
		}
		{
			const int Item = Outside(0, 499);
			Both("RemoveMemoryItem", 0x48cef0, RemoveMemoryItem, Item);
		}
		{
			// Right to left: the hash first.
			const int Hash = Outside(0, HASH_TABLE_SIZE - 1);
			const int Item = t % 2 == 0 ? Outside(0, 499) : 7;
			Both("InsertMemoryItem", 0x48d0a0, InsertMemoryItem, Item, Hash);
		}
		Both("GetMemorySprite", 0x48d440, GetMemorySprite, Sprite);
		{
			const int Picture = Outside(1, 3);
			Both("GetMemoryPicture", 0x48d740, GetMemoryPicture, Picture, 0, 0);
		}
		{
			const int Item = Outside(0, 299);
			Both("TouchVideoItem", 0x48dd70, TouchVideoItem, Item);
		}
		{
			const int Hash = Outside(0, HASH_TABLE_SIZE - 1);
			const int Item = t % 2 == 0 ? Outside(0, 299) : 9;
			Both("InsertVideoItem", 0x48e410, InsertVideoItem, Item, Hash);
		}
		Both("GetVideoSprite", 0x48e7b0, GetVideoSprite, Sprite);
		Both("GetVideoMaskedSprite", 0x48eb30, GetVideoMaskedSprite, t % 2 == 0 ? Sprite : 5, Sprite,
			(TRGBColor *)nullptr);
		{
			const int Picture = Outside(1, 3);
			Both("GetVideoPicture", 0x48f020, GetVideoPicture, Picture, 0, 0);
		}
		{
			const int Font = Outside(1, 4);
			Both("GetVideoFontChar", 0x48f430, GetVideoFontChar, Font, 0, 0, TRGBColor());
		}
		{
			const int Picture = Outside(1, 3);
			Both("DrawPicture", 0x490920, DrawPicture, 0, 0, 0, Picture);
		}
		int Font = Outside(1, 4);
		Both("GetFontHeight", 0x491710, GetFontHeight, Font);
		Both("GetTextHeight", 0x491880, GetTextHeight, t % 2 == 0 ? Font : 2, (const char *)nullptr);
		Both("GetTextWidth", 0x494570, GetTextWidth, t % 2 == 0 ? Font : 2, (const char *)nullptr);
	}
	for (int i = 0; i < 5; i++) {
		*Counts[i].Ours = Saved[i][0];
		ORIGINAL(Global<int>(Counts[i].Original) = Saved[i][1]);
	}
}

// --- TEND: errors raised in a callee, wrapped by the TBEGIN/TEND callers ---

#if GOLDEN_ORACLE
typedef void(__cdecl *LoadFn)(const char *FileName);
#endif

void TestWrapping([[maybe_unused]] const char *ExePath)
{
	golden::Input("seed", ErSeed);
#if GOLDEN_ORACLE
	// Map.cpp: the field queries call GetFirstObject (and others), which
	// asserts the coordinates; TEND2(rx, ry) wraps it. The image's field
	// matrix and object vector need their constructors (as in map_tests.cpp;
	// a second construction leaks the first one's memory). IsFieldPassable
	// only without CheckHeight, which reads the field (8, 6, GetMapLevel()).
	static bool MapConstructed = false;
	if (!MapConstructed) {
		MapConstructed = true;
		At<void(__fastcall *)(void *, void *, int, int)>(0x4b0bc0)((void *)(uintptr_t)0x5d4c18, NULL, 0, 0x7df);
		At<void(__fastcall *)(void *, void *, int, int, int, int, int, int)>(0x4b0d70)((void *)(uintptr_t)0x5d4c24, NULL,
			0, 17, 0, 13, 0, 7);
	}
#endif
	// The callees whose errors are wrapped must be the real ones (the oracle's answer, recorded).
	const bool Fields = !RecordedSkip(0x4b6140, (const void *)(TObject (*)(int, int, int))GetFirstObject);
	const bool Creatures = !RecordedSkip(0x44c6e0, (const void *)GetCreature);
	const bool BodyReal = !RecordedSkip(0x4a4550, (const void *)SetBodyObject);
	for (int t = 0; t < 40; t++) {
		int x, y, z;
		Coordinates(&x, &y, &z, MAPSIZE_X - 1, MAPSIZE_Y - 1, MAPSIZE_Z - 1);
		if (Fields) {
			Both("GetFieldHeight", 0x4bdf30, GetFieldHeight, x, y, z);
			Both("IsFieldPassable", 0x4be130, IsFieldPassable, x, y, z, false);
			Both("IsFieldThrowable", 0x4be390, IsFieldThrowable, x, y, z);
			Both("IsFieldTranslucent", 0x4be570, IsFieldTranslucent, x, y, z);
		} else {
			Skipped += 4;
		}
		Both("RequestFieldRefresh", 0x4bdd70, RequestFieldRefresh, x, y, z);
		Both("FieldRefreshRequested", 0x4be750, FieldRefreshRequested, x, y, z);
		// Inventory.cpp: the body positions 1..BODY_POSITIONS.
		int Body = Outside(1, BODY_POSITIONS);
		Both("GetBodyObject", 0x4a4880, GetBodyObject, Body);
		if (BodyReal)
			Both("DeleteBodyObject", 0x4a46d0, DeleteBodyObject, Body);
		Both("SetBodyObject", 0x4a4550, SetBodyObject, Body, TObject());
		// Creatures.cpp: GetCreature(0) asserts.
		if (Creatures)
			Both("HideCreature", 0x44c8c0, HideCreature, 0);
	}

	// Talk.cpp: the talkbox getters call GetTalkbox, which asserts
	// In(Number,1,GetNumberOfTalkboxes()) and prints the count: the same on
	// both sides when the earlier tests left both sides alike.
	const int OurTalkboxes = GetNumberOfTalkboxes();
	const int OriginalTalkboxes = ORIGINAL_VALUE("talkboxes", int, At<int(__cdecl *)()>(0x4fd450)());
	if (RecordedSkip(0x4fdd80, (const void *)GetTalkbox)) {
		printf("note: Errors: GetTalkbox is redirected; the talkbox getters are not compared\n");
	} else if (OurTalkboxes == OriginalTalkboxes) {
		for (int t = 0; t < 40; t++) {
			int Number = Outside(1, OurTalkboxes > 0 ? OurTalkboxes : 1);
			if (OurTalkboxes == 0 && t % 4 == 0)
				Number = 1;
			int a, b, c, d;
			Both("GetTalkbox", 0x4fdd80, GetTalkbox, Number);
			Both("GetTalkboxMode", 0x4fdf10, GetTalkboxMode, Number);
			Both("GetTalkboxPosition", 0x4fe0b0, GetTalkboxPosition, Number, &a, &b, &c);
			Both("GetTalkboxRectangle", 0x4fe2b0, GetTalkboxRectangle, Number, &a, &b, &c, &d);
			TRGBColor Color;
			Both("GetTalkboxColor", 0x4fe470, GetTalkboxColor, &Color, Number);
			Both("GetTalkboxLines", 0x4fe620, GetTalkboxLines, Number);
			Both("GetTalkboxLine", 0x4fe7c0, GetTalkboxLine, Number, 0);
			Both("SetTalkboxRectangle", 0x4fef70, SetTalkboxRectangle, Number, 1, 2, 3, 4);
		}
	} else {
		printf("note: Errors: %d talkboxes, the original %d; the talkbox getters are not compared\n", OurTalkboxes,
			OriginalTalkboxes);
	}

	// Objects.cpp on the real Tibia.dat: the getters index the property
	// tables through Container.h's array operator(), whose index error is
	// wrapped with the type (TEND1, TEND2) or by GetObjectFlag's own assertion.
	// Tibia.dat comes from the corpus in the runner (golden::DataFile).
	const std::string Dat = golden::DataFile("Tibia.dat", ExePath);
	if (Dat.empty()) {
		printf("note: Errors: Tibia.dat not found next to %s, the Objects.cpp getters are not compared\n",
			ExePath != nullptr ? ExePath : "Tibia.exe");
		return;
	}
	LoadObjects(Dat.c_str());
	ORIGINAL(At<LoadFn>(0x4d7380)(Dat.c_str()));
	typedef int (*IntOf)(int);
	struct {
		const char *Name;
		uint32 Address;
		IntOf Ours;
		int Kind;
	} Getters[] = {
		{"GetObjectWaypoints", 0x4d1f30, GetObjectWaypoints, 0},
		{"GetObjectBrightness", 0x4d20d0, GetObjectBrightness, 0},
		{"GetObjectLightColor", 0x4d2270, GetObjectLightColor, 0},
		{"GetObjectShiftX", 0x4d2410, GetObjectShiftX, 0},
		{"GetObjectShiftY", 0x4d25b0, GetObjectShiftY, 0},
		{"GetObjectElevation", 0x4d2750, GetObjectElevation, 0},
		{"GetObjectAutomapColor", 0x4d28f0, GetObjectAutomapColor, 0},
		{"GetObjectLensHelp", 0x4d2a90, GetObjectLensHelp, 0},
		{"GetObjectWidth", 0x4d2c30, GetObjectWidth, 0},
		{"GetObjectHeight", 0x4d2dd0, GetObjectHeight, 0},
		{"GetObjectExactSize", 0x4d2f70, GetObjectExactSize, 0},
		{"GetObjectLayers", 0x4d3110, GetObjectLayers, 0},
		{"GetObjectPatternWidth", 0x4d32b0, GetObjectPatternWidth, 0},
		{"GetObjectPatternHeight", 0x4d3450, GetObjectPatternHeight, 0},
		{"GetObjectPatternDepth", 0x4d35f0, GetObjectPatternDepth, 0},
		{"GetObjectPhases", 0x4d3790, GetObjectPhases, 0},
		{"GetOutfitBrightness", 0x4d3cc0, GetOutfitBrightness, 1},
		{"GetOutfitLightColor", 0x4d3e60, GetOutfitLightColor, 1},
		{"GetOutfitShiftX", 0x4d4000, GetOutfitShiftX, 1},
		{"GetOutfitShiftY", 0x4d41a0, GetOutfitShiftY, 1},
		{"GetOutfitWidth", 0x4d4340, GetOutfitWidth, 1},
		{"GetOutfitExactSize", 0x4d44e0, GetOutfitExactSize, 1},
		{"GetOutfitHeight", 0x4d4680, GetOutfitHeight, 1},
		{"GetOutfitLayers", 0x4d4820, GetOutfitLayers, 1},
		{"GetOutfitPhases", 0x4d49c0, GetOutfitPhases, 1},
		{"GetEffectBrightness", 0x4d4ef0, GetEffectBrightness, 2},
		{"GetEffectLightColor", 0x4d5090, GetEffectLightColor, 2},
		{"GetEffectWidth", 0x4d5230, GetEffectWidth, 2},
		{"GetEffectHeight", 0x4d53d0, GetEffectHeight, 2},
		{"GetEffectLayers", 0x4d5570, GetEffectLayers, 2},
		{"GetEffectPhases", 0x4d5710, GetEffectPhases, 2},
		{"GetMissileBrightness", 0x4d5c40, GetMissileBrightness, 3},
		{"GetMissileLightColor", 0x4d5de0, GetMissileLightColor, 3},
		{"GetMissileWidth", 0x4d5f80, GetMissileWidth, 3},
		{"GetMissileHeight", 0x4d6120, GetMissileHeight, 3},
		{"GetMissileLayers", 0x4d62c0, GetMissileLayers, 3},
	};
	array<TProperties> *Tables[4] = {ObjectProperties.get(), OutfitProperties.get(), EffectProperties.get(), // std::unique_ptr (issue #58)
		MissileProperties.get()};
	for (int t = 0; t < 400; t++) {
		auto &G = Getters[ErRandom() % (sizeof(Getters) / sizeof(Getters[0]))];
		array<TProperties> *Table = Tables[G.Kind];
		// Mostly outside the table; inside, the Entry getters assert their flag.
		int Type = ErRandom() % 3 == 0 ? Inside(Table->low(), Table->high()) : Outside(Table->low(), Table->high());
		Both(G.Name, G.Address, G.Ours, Type);
		int Flag = (int)(ErRandom() % 40);
		switch (G.Kind) {
		case 0: Both("GetObjectFlag", 0x4d1c40, (bool (*)(int, int))GetObjectFlag, Type, Flag); break;
		case 1: Both("GetOutfitFlag", 0x4d3af0, GetOutfitFlag, Type, Flag); break;
		case 2: Both("GetEffectFlag", 0x4d4d20, GetEffectFlag, Type, Flag); break;
		default: Both("GetMissileFlag", 0x4d5a70, GetMissileFlag, Type, Flag); break;
		}
		Both("GetOutfitSpriteNumber", 0x4d4b60, GetOutfitSpriteNumber, Type, 0, 0, 0, 0, 0);
		Both("GetEffectSpriteNumber", 0x4d58b0, GetEffectSpriteNumber, Type, 0, 0, 0, 0);
	}
	ExitObjects();
	ORIGINAL(At<void(__cdecl *)()>(0x4d79b0)());
}

// --- Script.cpp: the tokenizer's and the writer's errors -------------------

#if GOLDEN_ORACLE
typedef void(__fastcall *ScriptFn)(void *This, void *);
typedef void(__fastcall *ScriptStrFn)(void *This, void *, const char *);
typedef void(__fastcall *ScriptIntFn)(void *This, void *, int);
typedef uint32(__fastcall *ScriptRetFn)(void *This, void *);
typedef void(__fastcall *ScriptBytesFn)(void *This, void *, uint8 *, int);
#endif

std::string ErrorScript()
{
	// Mostly valid tokens with errors in between: long identifiers, open
	// strings and coordinates, stray characters, bytes, a missing end.
	static const char *const Parts[] = {
		"Name",
		"x_1",
		"123",
		"-5",
		"\"text\"",
		"\"open",
		"\"esc\\\"aped\\n\"",
		"[1,2,3]",
		"[1,2",
		"[a]",
		"1-2-3",
		"=",
		",",
		"<",
		">",
		"<=",
		">=",
		"<>",
		"-",
		"->",
		"#comment\n",
		"\n",
		" ",
		"\t",
		"@",
		"\\",
		"\"\n\"",
		"0x",
		"[-1,-2,-3]",
		"\"",
		"[",
		"1-",
		"1-2-",
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz",
	};
	std::string Text;
	int Count = (int)(ErRandom() % 30);
	for (int i = 0; i < Count; i++) {
		Text += Parts[ErRandom() % (sizeof(Parts) / sizeof(Parts[0]))];
		if (ErRandom() % 3 == 0)
			Text += " ";
	}
	if (ErRandom() % 6 == 0)
		Text += std::string(4100, 'a'); // longer than an identifier or a string may be
	return Text;
}

void WriteText(const std::string &Name, const std::string &Text)
{
	FILE *File = fopen(Name.c_str(), "wb");
	fwrite(Text.data(), 1, Text.size(), File);
	fclose(File);
}

void TestScript()
{
	golden::Input("seed", ErSeed);
	const std::string Work = WorkDirectory();
	_mkdir(Work.c_str());
	std::string Name = Work + GOLDEN_SLASH "script.txt", Missing = Work + GOLDEN_SLASH "missing" GOLDEN_SLASH "none.txt";
	ORIGINAL(static uint8 Original[sizeof(TReadScriptFile) + 16]);
	for (int Round = 0; Round < 400; Round++) {
		WriteText(Name, ErrorScript());
		ORIGINAL(At<ScriptFn>(0x4df550)(Original, NULL));
		TReadScriptFile Ours;
		switch (Round % 8) {
		case 0:
			Both2("TReadScriptFile::nextToken (no file)", [&] { Ours.nextToken(); }, [&] { ORIGINAL(At<ScriptFn>(0x4dfb60)(Original, NULL)); });
			break;
		case 1:
			Both2("TReadScriptFile::open (missing)", [&] { Ours.open(Missing.c_str()); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4df6d0)(Original, NULL, Missing.c_str())); });
			break;
		default:
			Ours.open(Name.c_str());
			ORIGINAL(At<ScriptStrFn>(0x4df6d0)(Original, NULL, Name.c_str()));
			if (Round % 8 == 2)
				Both2("TReadScriptFile::open (open)", [&] { Ours.open(Name.c_str()); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4df6d0)(Original, NULL, Name.c_str())); });
			for (int i = 0; i < 100; i++) {
				TThrown A = Both2("TReadScriptFile::nextToken", [&] { Ours.nextToken(); }, [&] { ORIGINAL(At<ScriptFn>(0x4dfb60)(Original, NULL)); });
				if (A.Kind != TThrown::NOTHING || Ours.Token == TOKEN::ENDOFFILE)
					break;
				int Getter = (int)(ErRandom() % 6);
				if (Getter == 0)
					A = Both2("TReadScriptFile::getIdentifier", [&] { Ours.getIdentifier(); }, [&] { ORIGINAL(At<ScriptFn>(0x4e0420)(Original, NULL)); });
				else if (Getter == 1)
					A = Both2("TReadScriptFile::getNumber", [&] { Ours.getNumber(); }, [&] { ORIGINAL(At<ScriptFn>(0x4e0450)(Original, NULL)); });
				else if (Getter == 2)
					A = Both2("TReadScriptFile::getString", [&] { Ours.getString(); }, [&] { ORIGINAL(At<ScriptFn>(0x4e0470)(Original, NULL)); });
				else if (Getter == 3)
					A = Both2("TReadScriptFile::getSpecial", [&] { Ours.getSpecial(); }, [&] { ORIGINAL(At<ScriptFn>(0x4e0490)(Original, NULL)); });
				if (A.Kind != TThrown::NOTHING)
					break;
			}
			break;
		}
		Ours.close();
		ORIGINAL(At<ScriptFn>(0x4df5d0)(Original, NULL));

		// The writer: open twice, a file that cannot be made, writing without a file.
		ORIGINAL(static uint8 Writer[sizeof(TWriteScriptFile) + 16]; At<ScriptFn>(0x4df640)(Writer, NULL));
		TWriteScriptFile OurWriter;
		std::string Out = Work + GOLDEN_SLASH "written.txt";
		int Case = (int)(ErRandom() % 6);
		if (Case == 0) {
			Both2("TWriteScriptFile::open (missing directory)", [&] { OurWriter.open(Missing.c_str()); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4e04b0)(Writer, NULL, Missing.c_str())); });
		} else if (Case == 1) {
			OurWriter.open(Out.c_str());
			ORIGINAL(At<ScriptStrFn>(0x4e04b0)(Writer, NULL, Out.c_str()));
			Both2("TWriteScriptFile::open (open)", [&] { OurWriter.open(Out.c_str()); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4e04b0)(Writer, NULL, Out.c_str())); });
			OurWriter.close();
			ORIGINAL(At<ScriptFn>(0x4df660)(Writer, NULL));
		} else {
			Both2("TWriteScriptFile::writeLn (no file)", [&] { OurWriter.writeLn(); }, [&] { ORIGINAL(At<ScriptFn>(0x4e0790)(Writer, NULL)); });
			Both2("TWriteScriptFile::writeText (no file)", [&] { OurWriter.writeText("x"); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4e08f0)(Writer, NULL, "x")); });
			Both2("TWriteScriptFile::writeString (no file)", [&] { OurWriter.writeString("x"); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4e0bb0)(Writer, NULL, "x")); });
			Both2("TWriteScriptFile::writeNumber (no file)", [&] { OurWriter.writeNumber(5); }, [&] { ORIGINAL(At<ScriptIntFn>(0x4e0ec0)(Writer, NULL, 5)); });
		}
		ORIGINAL(At<ScriptFn>(0x4df660)(Writer, NULL));
	}

	// The binary files: opening, and every access without a file; reading
	// past the end of a short file.
	ORIGINAL(static uint8 Reader[sizeof(TReadBinaryFile) + 16]; static uint8 BinaryWriter[sizeof(TWriteBinaryFile) + 16]);
	uint8 Bytes[16];
	for (int Round = 0; Round < 100; Round++) {
		std::string Data = RandomPart((int)(ErRandom() % 8));
		WriteText(Name, Data);
		ORIGINAL(At<ScriptFn>(0x4df690)(Reader, NULL));
		TReadBinaryFile OurReader;
		bool Open = Round % 3 != 0;
		if (Open) {
			OurReader.open(Name.c_str());
			ORIGINAL(At<ScriptStrFn>(0x4e1040)(Reader, NULL, Name.c_str()));
		}
		const std::string &Target = Round % 2 == 0 ? Missing : Name;
		Both2("TReadBinaryFile::open", [&] { OurReader.open(Target.c_str()); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4e1040)(Reader, NULL, Target.c_str())); });
		int Offset = (int)(ErRandom() % 12) - 3;
		Both2("TReadBinaryFile::seek", [&] { OurReader.seek(Offset); }, [&] { ORIGINAL(At<ScriptIntFn>(0x4e1320)(Reader, NULL, Offset)); });
		for (int i = 0; i < 4; i++) {
			int Count = (int)(ErRandom() % 5) - 1;
			switch (ErRandom() % 4) {
			case 0:
				Both2("TReadBinaryFile::readByte", [&] { OurReader.readByte(); }, [&] { ORIGINAL(At<ScriptRetFn>(0x4e1740)(Reader, NULL)); });
				break;
			case 1:
				Both2("TReadBinaryFile::readWord", [&] { OurReader.readWord(); }, [&] { ORIGINAL(At<ScriptRetFn>(0x4e1a00)(Reader, NULL)); });
				break;
			case 2:
				Both2("TReadBinaryFile::readQuad", [&] { OurReader.readQuad(); }, [&] { ORIGINAL(At<ScriptRetFn>(0x4e1ba0)(Reader, NULL)); });
				break;
			default:
				Both2("TReadBinaryFile::readBytes", [&] { OurReader.readBytes(OriginalSpan<uint8>(Count < 0 ? nullptr : Bytes, Count < 0 ? 2 : Count)); }, [&] { ORIGINAL(At<ScriptBytesFn>(0x4e1e50)(Reader, NULL, Count < 0 ? NULL : Bytes, Count < 0 ? 2 : Count)); });
				break;
			}
		}
		Both2("TReadBinaryFile::eof", [&] { OurReader.eof(); }, [&] { ORIGINAL(At<ScriptRetFn>(0x4e15e0)(Reader, NULL)); });
		OurReader.close();
		ORIGINAL(At<ScriptFn>(0x4df6b0)(Reader, NULL));

		ORIGINAL(At<ScriptFn>(0x4df690)(BinaryWriter, NULL));
		TWriteBinaryFile OurBinary;
		std::string Out = Work + GOLDEN_SLASH "written.bin";
		if (Round % 3 == 1) {
			OurBinary.open(Out.c_str());
			ORIGINAL(At<ScriptStrFn>(0x4e21a0)(BinaryWriter, NULL, Out.c_str()));
		}
		const std::string &WriteTarget = Round % 2 == 0 ? Missing : Out;
		Both2("TWriteBinaryFile::open", [&] { OurBinary.open(WriteTarget.c_str()); }, [&] { ORIGINAL(At<ScriptStrFn>(0x4e21a0)(BinaryWriter, NULL, WriteTarget.c_str())); });
		if (Round % 3 == 0) {
			Both2("TWriteBinaryFile::writeByte", [&] { OurBinary.writeByte(1); }, [&] { ORIGINAL(At<ScriptIntFn>(0x4e2480)(BinaryWriter, NULL, 1)); });
			Both2("TWriteBinaryFile::writeWord", [&] { OurBinary.writeWord(1); }, [&] { ORIGINAL(At<ScriptIntFn>(0x4e2750)(BinaryWriter, NULL, 1)); });
			Both2("TWriteBinaryFile::writeQuad", [&] { OurBinary.writeQuad(1); }, [&] { ORIGINAL(At<ScriptIntFn>(0x4e29e0)(BinaryWriter, NULL, 1)); });
			Both2("TWriteBinaryFile::writeBytes", [&] { OurBinary.writeBytes(OriginalSpan<const uint8>(Bytes, 2)); }, [&] { ORIGINAL(At<ScriptBytesFn>(0x4e2c80)(BinaryWriter, NULL, Bytes, 2)); });
		}
		OurBinary.close();
		ORIGINAL(At<ScriptFn>(0x4df6b0)(BinaryWriter, NULL));
		// InstallPatch (no name, or no patched file "<name>_") is gone with the patcher (Patella-Modern issue #75), and
		// its comparison with it.
	}
	remove(Name.c_str());
}

// --- Assertions that print a file's statics (issue #34) --------------------

#if GOLDEN_ORACLE
// The int a getter returns: the first "mov eax, [address]" or "mov reg,
// [address]" in the getter's first bytes whose int has the getter's value
// and whose change the getter sees. Works for our functions and the image's.
// (GetNumberOfBuddies reads its count after its try block's set-up, and only
// while offline buddies are shown: 256 bytes, BuddyHideOffline cleared.)
int *FindStatic(const uint8 *Code, int(__cdecl *Get)())
{
	for (int i = 0; i < 256; i++) {
		const uint8 *Op = Code + i;
		uint32 Address;
		if (Op[0] == 0xa1)
			Address = *(const uint32 *)(Op + 1);
		else if (Op[0] == 0x8b && (Op[1] & 0xc7) == 0x05)
			Address = *(const uint32 *)(Op + 2);
		else
			continue;
		MEMORY_BASIC_INFORMATION Memory;
		if (VirtualQuery((void *)(uintptr_t)Address, &Memory, sizeof(Memory)) == 0 || Memory.State != MEM_COMMIT
			|| (Memory.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) == 0)
			continue;
		int *Value = (int *)(uintptr_t)Address;
		int Old = *Value;
		if (Get() != Old)
			continue;
		*Value = Old ^ 0x5a5a;
		bool Seen = Get() == (Old ^ 0x5a5a);
		*Value = Old;
		if (Seen)
			return Value;
	}
	return NULL;
}
int *FindOwnStatic(int (*Get)())
{
	return FindStatic((const uint8 *)Get, Get);
}
#else
// The runner's (x64) FindStatic for our getters: the first "mov r32, [rip+disp32]" (8b /r with mod 00, r/m
// 101, a REX prefix without W allowed) in the getter's first bytes whose int has the getter's value and
// whose change the getter sees, as the oracle's x86 FindStatic. Under AddressSanitizer (the preset
// windows-x64-asan, issue #70) the getter's prologue poisons its stack frame first, so the
// load comes later (1,123 bytes in, in Debug): 2,048 bytes are searched there.
int *FindOwnStatic(int (*Get)())
{
#if defined(__SANITIZE_ADDRESS__)
	constexpr int Window = 2048;
#else
	constexpr int Window = 256;
#endif
	const uint8 *Code = (const uint8 *)Get;
	for (int i = 0; i < Window; i++) {
		const uint8 *Op = Code + i;
		const int Prefix = Op[0] >= 0x40 && Op[0] <= 0x47 ? 1 : 0;
		if (Op[Prefix] != 0x8b || (Op[Prefix + 1] & 0xc7) != 0x05)
			continue;
		int32_t Displacement;
		memcpy(&Displacement, Op + Prefix + 2, sizeof(Displacement));
		const uint8 *Address = Op + Prefix + 6 + Displacement;
		MEMORY_BASIC_INFORMATION Memory;
		if (VirtualQuery(Address, &Memory, sizeof(Memory)) == 0 || Memory.State != MEM_COMMIT
			|| (Memory.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) == 0)
			continue;
		int *Value = (int *)const_cast<uint8 *>(Address);
		const int Old = *Value;
		if (Get() != Old)
			continue;
		*Value = Old ^ 0x5a5a;
		const bool Seen = Get() == (Old ^ 0x5a5a);
		*Value = Old;
		if (Seen)
			return Value;
	}
	return nullptr;
}
#endif

// --- Surface.cpp's SurfaceInitInstance (issue #34) --------------------------
//
// Retired with the oracle (the user's decision of November 4, 2026): it compares the x86 Surface.cpp's
// engine selection (IsDirect3D9Installed, CreateSurfaceDX9, curEngine 0 to 2), which the x64 Surface.cpp
// replaces by the Vulkan engine. The oracle still runs and records it; the runner skips it
// (golden::RetiredBegin in TestErrorStatics).

#if GOLDEN_ORACLE
// Its try block around the DirectX 9 test turns every exception into
// PROGRAM_ERROR 12: IsDirect3D9Installed, the surface's creation (GetSurface,
// CreateSurfaceDX9) or its TestEngine throws an assertion of the calling side
// (fakes on both sides; the engine from Tibia.cfg random, no command line
// engine). The second try block (ChoosePixelFormat, DescribePixelFormat) has
// no handler in the binary and is not reached.
int InitSide, InitMode, InitEngine; // 0 ours, 1 the image's; what fails
bool __cdecl FakeInitD3D9Installed()
{
	if (InitMode == 0)
		ThrowSideAssertion(InitSide == 1, -1);
	return true;
}
int __cdecl FakeInitGetEngine() { return InitEngine; }
class TInitTestSurface : public TTestSurface {
public:
	bool TestEngine()
	{
		ThrowSideAssertion(InitSide == 1, -3);
		return true;
	}
};
TSurface *__cdecl FakeInitSurface()
{
	if (InitMode == 1)
		ThrowSideAssertion(InitSide == 1, -2);
	static TInitTestSurface *Made[2];
	if (Made[InitSide] == NULL)
		Made[InitSide] = new TInitTestSurface;
	return Made[InitSide];
}
struct TCodePatch {
	uint8 *Code;
	uint8 Old[5];
};
void PatchSaved(std::vector<TCodePatch> &List, void *Function, void *Target, bool Image)
{
	TCodePatch P;
	P.Code = (uint8 *)Function;
	memcpy(P.Old, P.Code, 5);
	List.push_back(P);
	if (Image)
		PatchImageJump((uint32)(uintptr_t)Function, Target);
	else
		PatchOwnCode(Function, Target);
}
void RestorePatches(std::vector<TCodePatch> &List)
{
	for (size_t i = List.size(); i-- > 0;) {
		DWORD Old;
		VirtualProtect(List[i].Code, 5, PAGE_EXECUTE_READWRITE, &Old);
		memcpy(List[i].Code, List[i].Old, 5);
		VirtualProtect(List[i].Code, 5, Old, &Old);
		FlushInstructionCache(GetCurrentProcess(), List[i].Code, 5);
	}
	List.clear();
}
void TestSurfaceInitInstance()
{
	std::vector<TCodePatch> Patches;
	PatchSaved(Patches, (void *)IsDirect3D9Installed, (void *)FakeInitD3D9Installed, false);
	PatchSaved(Patches, (void *)GetEngine, (void *)FakeInitGetEngine, false);
	PatchSaved(Patches, (void *)GetSurface, (void *)FakeInitSurface, false);
	PatchSaved(Patches, (void *)CreateSurfaceDX9, (void *)FakeInitSurface, false);
	PatchSaved(Patches, (void *)(uintptr_t)0x4efd10, (void *)FakeInitD3D9Installed, true);
	PatchSaved(Patches, (void *)(uintptr_t)0x4db420, (void *)FakeInitGetEngine, true);
	PatchSaved(Patches, (void *)(uintptr_t)0x4eaa90, (void *)FakeInitSurface, true);
	PatchSaved(Patches, (void *)(uintptr_t)0x4f1430, (void *)FakeInitSurface, true);
	int SavedCommandLine[2] = {CommandLineEngine, Global<int>(0x5c2564)};
	int SavedEngine[2] = {curEngine, Global<int>(0x71d0c4)};
	HWND SavedWindow = Global<HWND>(0x71d0c0);
	CommandLineEngine = -1;
	Global<int>(0x5c2564) = -1;
	for (int Round = 0; Round < 12; Round++) {
		InitMode = Round % 3;
		InitEngine = (int)(ErRandom() % 5) - 1;
		HWND Window = (HWND)(uintptr_t)(0x1000 + Round);
		char What[64];
		snprintf(What, sizeof(What), "SurfaceInitInstance (fails %d, engine %d)", InitMode, InitEngine);
		TThrown A = OurThrow([&] {
			InitSide = 0;
			SurfaceInitInstance(&Window);
		});
		TThrown B = ImageThrow([&] {
			InitSide = 1;
			At<void(__cdecl *)(HWND *)>(0x4eac20)(&Window);
		});
		CHECK(A.Kind != TThrown::NOTHING && B.Kind != TThrown::NOTHING, "Statics: %s did not throw", What);
		Compare(What, A, B);
		CHECK(curEngine == Global<int>(0x71d0c4), "Statics: %s: engine %d, original %d", What, curEngine,
			Global<int>(0x71d0c4));
	}
	RestorePatches(Patches);
	CommandLineEngine = SavedCommandLine[0];
	Global<int>(0x5c2564) = SavedCommandLine[1];
	curEngine = SavedEngine[0];
	Global<int>(0x71d0c4) = SavedEngine[1];
	Global<HWND>(0x71d0c0) = SavedWindow;
}
#endif

// Each count is set on both sides for the calls (out-of-range arguments, so
// that only the assertions run) and put back: NumberOfBuddies,
// NumberOfPlayerSteps, AccountCharacters, the two offers' counts. The effect
// getters run for every effect number with the status it has.
void TestErrorStatics()
{
	TErrorRound R("Statics");
	golden::Input("seed", ErSeed);
	struct TStatic {
		const char *Name;
		uint32 ImageGetter;
		int(__cdecl *OurGetter)();
		int *Ours, *Image;
	} Statics[] = {
		{"NumberOfBuddies", 0x44fba0, GetNumberOfBuddies, nullptr, nullptr},
		{"NumberOfPlayerSteps", 0x44b870, GetNumberOfPlayerSteps, nullptr, nullptr},
		{"AccountCharacters", 0x4e72a0, GetAccountCharacters, nullptr, nullptr},
		{"OwnOffer.Count", 0x4a4530, GetOwnOfferCount, nullptr, nullptr},
		{"CounterOffer.Count", 0x4a4540, GetCounterOfferCount, nullptr, nullptr},
	};
	bool Found[5];
	const bool SavedHide = GetBuddyHideOffline();
	ORIGINAL(const bool SavedImageHide = Global<bool>(0x719eac));
	SetBuddyHideOffline(false);
	ORIGINAL(Global<bool>(0x719eac) = false);
	for (int k = 0; k < 5; k++) {
		TStatic &S = Statics[k];
		Found[k] = !RecordedSkip(S.ImageGetter, (const void *)S.OurGetter);
		if (Found[k]) {
			S.Ours = FindOwnStatic(S.OurGetter);
			ORIGINAL(S.Image = FindStatic((const uint8 *)(uintptr_t)S.ImageGetter, At<int(__cdecl *)()>(S.ImageGetter)));
			// The oracle's answer; the runner must have found ours where the oracle found both.
			Found[k] = ORIGINAL_VALUE("static found", uint8, S.Ours != NULL && S.Image != NULL) != 0;
			if (Found[k] && S.Ours == nullptr) {
				Failures++;
				printf("FAIL Errors: Statics: our %s not found by its getter; its assertions are not compared\n", S.Name);
				Found[k] = false;
			}
		}
		if (!Found[k])
			printf("note: Statics: %s not found or its getter redirected; its assertions are not compared\n", S.Name);
	}
	for (int t = 0; t < 60; t++) {
		int Saved[5][2] = {};
		for (int k = 0; k < 5; k++)
			if (Found[k]) {
				Saved[k][0] = *Statics[k].Ours;
				ORIGINAL(Saved[k][1] = *Statics[k].Image);
			}
		auto Set = [&](int k, int Value) {
			*Statics[k].Ours = Value;
			ORIGINAL(*Statics[k].Image = Value);
		};
		if (Found[0]) { // the buddies: one index out of range at least
			int N = (int)(ErRandom() % 201);
			Set(0, N);
			int i = Outside(0, N - 1), j = ErRandom() % 2 == 0 ? Outside(0, N - 1) : (N > 0 ? Inside(0, N - 1) : -1);
			if (ErRandom() % 2 == 0)
				std::swap(i, j);
			Both("CompareBuddies", 0x44de20, CompareBuddies, i, j);
			Both("SwapBuddies", 0x44e260, SwapBuddies, i, j);
			const int Buddy = Outside(0, N - 1);
			Both("SortBuddy", 0x44e7d0, SortBuddy, Buddy);
		}
		if (Found[1]) {
			int N = (int)(ErRandom() % 300);
			Set(1, N);
			const int Step = Outside(0, N - 1);
			Both("GetPlayerStep", 0x44c3c0, GetPlayerStep, Step);
		}
		if (Found[2]) {
			int N = (int)(ErRandom() % 20);
			Set(2, N);
			int Number = Outside(0, N - 1);
			static const uint8 Address[4] = {127, 0, 0, 1};
			Both("SetCharacter", 0x4e96d0, SetCharacter, Number, "name", "world", Address, 7171);
			Both("SetActiveCharacter", 0x4e9c50, SetActiveCharacter, Number);
			Both("GetCharacterName", 0x4e9db0, GetCharacterName, Number);
			Both("GetCharacterWorld", 0x4e9f20, GetCharacterWorld, Number);
			Both("GetCharacterIPAddress", 0x4ea090, GetCharacterIPAddress, Number);
			Both("GetCharacterPort", 0x4ea200, GetCharacterPort, Number);
		}
		if (Found[3] && Found[4]) {
			// SetOwnOffer needs a count other than 0, the adders one of at least
			// OBJECTS_PER_OFFER, the names one below 1.
			int Full = OBJECTS_PER_OFFER + (int)(ErRandom() % 5), Some = 1 + (int)(ErRandom() % OBJECTS_PER_OFFER);
			int Empty = -(int)(ErRandom() % 3);
			Set(3, ErRandom() % 2 == 0 ? Some : Full);
			Both("SetOwnOffer", 0x4a6eb0, SetOwnOffer, "name");
			Set(3, Full);
			Set(4, Full);
			Both("AddOwnOfferObject", 0x4a7460, AddOwnOfferObject, TObject());
			Both("AddCounterOfferObject", 0x4a75e0, AddCounterOfferObject, TObject());
			Set(3, Empty);
			Set(4, Empty);
			Both("GetOwnOfferName", 0x4a7760, GetOwnOfferName);
			Both("GetCounterOfferName", 0x4a78b0, GetCounterOfferName);
			Set(3, Some);
			Set(4, Some);
			{
				const int Object = Outside(0, Some - 1);
				Both("GetOwnOfferObject", 0x4a7a00, GetOwnOfferObject, Object);
			}
			{
				const int Object = Outside(0, Some - 1);
				Both("GetCounterOfferObject", 0x4a7b90, GetCounterOfferObject, Object);
			}
		}
		for (int k = 0; k < 5; k++)
			if (Found[k]) {
				*Statics[k].Ours = Saved[k][0];
				ORIGINAL(*Statics[k].Image = Saved[k][1]);
			}
	}
	// The effect getters with every effect's status (read only).
	for (int Effect = 0; Effect < EFFECTS; Effect++) {
		Both("GetGraphicalEffectType", 0x4b15a0, GetGraphicalEffectType, Effect);
		Both("GetGraphicalEffectPhase", 0x4b1860, GetGraphicalEffectPhase, Effect);
		Both("GetTextualEffectColor", 0x4b1b20, GetTextualEffectColor, Effect);
		Both("GetTextualEffectText", 0x4b1de0, GetTextualEffectText, Effect);
		Both("GetTextualEffectPhase", 0x4b20a0, GetTextualEffectPhase, Effect);
		Both("GetMissileEffectType", 0x4b2360, GetMissileEffectType, Effect);
		Both("GetMissileEffectDirection", 0x4b2620, GetMissileEffectDirection, Effect);
		Both("GetMissileEffectX", 0x4b28e0, GetMissileEffectX, Effect);
		Both("GetMissileEffectY", 0x4b2bb0, GetMissileEffectY, Effect);
	}
	SetBuddyHideOffline(SavedHide);
	ORIGINAL(Global<bool>(0x719eac) = SavedImageHide);

	// SurfaceInitInstance: retired with the oracle (above). After it the runner takes from the corpus what
	// the round reads of it: the exceptions it compared (the round's summary) and the random state.
	const int RetiredCompared = ThrownCompared, RetiredThrew = ThrownThrew;
	if (!golden::RetiredBegin("SurfaceInitInstance", Checks)) {
#if GOLDEN_ORACLE
		TestSurfaceInitInstance();
#endif
	}
	golden::RetiredEnd(Checks);
	int Exceptions[2] = {ThrownCompared - RetiredCompared, ThrownThrew - RetiredThrew};
	golden::Output("exceptions", Exceptions);
	ThrownCompared = RetiredCompared + Exceptions[0];
	ThrownThrew = RetiredThrew + Exceptions[1];
	golden::Output("seed", ErSeed);
	R.Finish();
}

void Run([[maybe_unused]] const char *ExePath)
{
	Trace = getenv("ER_TRACE") != nullptr && atoi(getenv("ER_TRACE")) != 0;
	if (getenv("ER_SEED") != nullptr)
		ErSeed = strtoul(getenv("ER_SEED"), nullptr, 10);
	int Before = Checks;
#if GOLDEN_ORACLE
	if (getenv("ER_WATCHDOG") != NULL) {
		DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &MainThread, 0, FALSE,
			DUPLICATE_SAME_ACCESS);
		CreateThread(NULL, 0, Watchdog, (void *)(uintptr_t)atoi(getenv("ER_WATCHDOG")), 0, NULL);
	}
	EnableImageExceptions();
#endif
	if (Trace)
		printf("Errors: %s\n", "TestMyException");
	TestMyException();
	if (Trace)
		printf("Errors: %s\n", "TestWrapping");
	ORIGINAL(PatchScript()); // the image's stdio, for LoadObjects
	TestWrapping(ExePath);
	if (Trace)
		printf("Errors: %s\n", "TestScript");
	TestScript();
	if (Trace)
		printf("Errors: %s\n", "TestPlayer");
	TestPlayer();
	if (Trace)
		printf("Errors: %s\n", "TestUtils");
	TestUtils();
	if (Trace)
		printf("Errors: %s\n", "TestMap");
	TestMap();
	if (Trace)
		printf("Errors: %s\n", "TestCreatures");
	TestCreatures();
	if (Trace)
		printf("Errors: %s\n", "TestInventory");
	TestInventory();
	if (Trace)
		printf("Errors: %s\n", "TestTalk");
	TestTalk();
	if (Trace)
		printf("Errors: %s\n", "TestObjects");
	TestObjects();
	if (Trace)
		printf("Errors: %s\n", "TestStatus");
	TestStatus();
	if (Trace)
		printf("Errors: %s\n", "TestPreferences");
	TestPreferences();
	if (Trace)
		printf("Errors: %s\n", "TestHelp");
	TestHelp();
	if (Trace)
		printf("Errors: %s\n", "TestGraphics");
	TestGraphics();
#if GOLDEN_ORACLE
	DisableImageExceptions(); // also undoes TestPlayer's patches
#else
	UndoPatches();
#endif
	int Summary[4] = {Checks - Before, Compared, ComparedThrows, Skipped};
	printf("Errors: %d checks, %d exceptions compared (%d calls that threw), %d comparisons skipped (functions "
		   "redirected by earlier tests)\n",
		Summary[0], Summary[1], Summary[2], Summary[3]);
	// The corpus: the oracle's summary; the runner's must be the same (not a check: the counts stay
	// difftest's).
	int Recorded[4] = {Summary[0], Summary[1], Summary[2], Summary[3]};
	golden::Output("summary", Recorded);
	if (golden::Mode() == golden::TMode::Replay) {
		printf("Errors: the oracle's %d checks, %d exceptions compared (%d calls that threw), %d comparisons skipped\n",
			Recorded[0], Recorded[1], Recorded[2], Recorded[3]);
		if (memcmp(Summary, Recorded, sizeof(Summary)) != 0) {
			Failures++;
			printf("FAIL Errors: the summary differs from the oracle's\n");
		}
	}
	TestErrorStatics(); // issue #34, reported apart ("Statics errors: ...")
}

} // namespace ErrorsTest

void TestErrors(const char *ExePath)
{
	ErrorsTest::Run(ExePath);
}
