// The golden corpus (notes/modernization-plan.md, 4.3; issue #24): the original's inputs and outputs of
// difftest's comparisons, by value, so that the comparisons can be replayed without Tibia.exe and on any
// platform.
//
// One source of every ported test (tests/golden/*_tests.cpp) is compiled twice:
// - into the oracle-x86 test executables (difftest.exe, sysinctest.exe; GOLDEN_ORACLE defined), where
//   the original's code runs inside ORIGINAL(...) and #if GOLDEN_ORACLE, and each of its results is
//   passed through golden::Output, each generated input through golden::Input. With DIFFTEST_RECORD set
//   they are written to the corpus; without it the calls do nothing;
// - into the runner (tests/golden/runner.cpp, x64; GOLDEN_ORACLE 0), where ORIGINAL(...) is empty and
//   golden::Output reads the original's result back from the corpus, golden::Input compares the input
//   the test generated again with the recorded one.
// The CHECK calls are the same in both, so the runner makes the same checks as difftest.
//
// The format (version GOLDEN_FORMAT; little-endian, both targets are):
//	<dir>/<executable>/manifest.tsv   text, one line per field (tab-separated):
//	    format <version>, tibia_sha256 <hex>, executable <name>, then per test (difftest's order)
//	    test <name> <seed> <checks> <records>, and last total <checks> <failures>. It is written
//	    after the last test, so a recording that did not finish has none.
//	<dir>/<executable>/<test>.bin    per test with records: "PGLD", u32 version, the 32-byte SHA-256 of
//	    Tibia.exe, u32 Random()'s seed at the start of the test, u16 length + the test's name; then
//	    records, each a u8 tag:
//	        1 NAME   varint id, varint length, the name (a record's field name, defined once)
//	        2 VALUE  varint name id, u8 role ('I' input, 'O' the original's output), u8 type ('i'
//	                 signed, 'u' unsigned, 'f' floating, 'b' bytes), u8 element size, varint count,
//	                 count * element size bytes
//	        3 END    u64 the test's checks, u64 its VALUE records
//	A retired part of a test (RetiredBegin, issue #28) is two VALUE records around the part's own: the input
//	"retired" (bytes: the part's name) and the output "retired end" (two u64: the part's checks and its
//	VALUE records); the test's checks and records at END count the part's.
// A corpus is stale when its format version or its Tibia.exe differs from the runner's, or when a test
// asks for another record than the next one (the test changed since the recording); the runner then
// stops with a message instead of comparing.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <type_traits>

#ifndef GOLDEN_ORACLE
#define GOLDEN_ORACLE 0
#endif

// The original's side of a comparison: compiled into the oracle only.
// ORIGINAL_OR(Value, Otherwise): the original's value in the oracle, Otherwise in the runner.
// ORIGINAL_VALUE(Name, Type, Expression...): the original's value of Expression, recorded by the oracle
// and read back by the runner (golden::Recorded); ORIGINAL_TEXT(Name, Expression...) the same for a text
// that may be NULL (golden::TText).
#if GOLDEN_ORACLE
#define ORIGINAL(...) __VA_ARGS__
#define ORIGINAL_OR(Value, Otherwise) (Value)
#define ORIGINAL_VALUE(Name, Type, ...) golden::Recorded<Type>(Name, static_cast<Type>(__VA_ARGS__))
#define ORIGINAL_TEXT(Name, ...) golden::RecordedText(Name, __VA_ARGS__)
#else
#define ORIGINAL(...) ((void)0)
#define ORIGINAL_OR(Value, Otherwise) (Otherwise)
#define ORIGINAL_VALUE(Name, Type, ...) golden::Recorded<Type>(Name, Type{})
#define ORIGINAL_TEXT(Name, ...) golden::RecordedText(Name, nullptr)
#endif

namespace golden {

const uint32_t GOLDEN_FORMAT = 1;
// CLAUDE.md: Tibia.exe 7.72 of 2006-06-08.
const char *const TIBIA_SHA256 = "1a82fe97a39e536c29327d6d5dde316775f537dcc01506831429b1a0433b76f8";

enum class TMode { Off,
	Record,
	Replay };
TMode Mode();

// The recorder (the oracle's test executables). StartRecording reads DIFFTEST_RECORD (the corpus
// directory); without it nothing is recorded. It removes the executable's earlier recording.
void StartRecording(const char *Executable, const char *TibiaPath);
void BeginTest(const char *Test, uint32_t Seed, int Checks);
void EndTest(int Checks);
void FinishRecording(int Checks, int Failures);

// The replayer (the runner). A failure to read or a stale corpus throws TStale.
struct TStale {
	std::string Message;
};
struct TTestEntry {
	std::string Name;
	uint32_t Seed;
	long long Checks;
	long long Records;
};
struct TManifest {
	std::string Executable;
	std::string TibiaSha256;
	uint32_t Format = 0;
	long long Checks = -1;
	long long Failures = -1;
	TTestEntry Tests[64];
	int NumberOfTests = 0;
};
// False when the manifest does not exist; throws TStale when it is unreadable or stale.
bool ReadManifest(const std::string &Directory, const char *Executable, TManifest *Manifest);
// Opens <directory>/<executable>/<test>.bin; returns the seed the test started with.
uint32_t BeginReplay(const std::string &Directory, const char *Executable, const char *Test);
// Checks that every record was used and the test made the recorded number of checks (the ones it replayed,
// Checks, and the ones it skipped as retired, RetiredBegin).
void EndReplay(int Checks);

// One value: its role, its type, the size of one element and their count.
void Exchange(const char *Name, char Role, char Type, size_t ElementSize, size_t Count, void *Data);

template <class T> struct TElement {
	using Type = T;
	static const size_t Count = 1;
};
template <class T, size_t N> struct TElement<T[N]> {
	using Type = typename TElement<T>::Type;
	static const size_t Count = N * TElement<T>::Count;
};
template <class T> constexpr char TypeCode()
{
	static_assert(std::is_arithmetic<T>::value, "the corpus stores numbers, arrays of them and bytes");
	return std::is_floating_point<T>::value ? 'f' : std::is_signed<T>::value ? 'i'
																			 : 'u';
}

// An input of the comparison (the runner compares it with its own) and an output of the original
// (the runner gets the recorded one): a number or an array of numbers.
template <class T> void Input(const char *Name, const T &Value)
{
	using E = typename TElement<T>::Type;
	if (Mode() != TMode::Off)
		Exchange(Name, 'I', TypeCode<E>(), sizeof(E), TElement<T>::Count, const_cast<T *>(&Value));
}
template <class T> void Output(const char *Name, T &Value)
{
	using E = typename TElement<T>::Type;
	if (Mode() != TMode::Off)
		Exchange(Name, 'O', TypeCode<E>(), sizeof(E), TElement<T>::Count, &Value);
}
inline void InputBytes(const char *Name, const void *Data, size_t Size)
{
	if (Mode() != TMode::Off)
		Exchange(Name, 'I', 'b', 1, Size, const_cast<void *>(Data));
}
inline void OutputBytes(const char *Name, void *Data, size_t Size)
{
	if (Mode() != TMode::Off)
		Exchange(Name, 'O', 'b', 1, Size, Data);
}
// A text the original returned (Text: the oracle's; ignored by the runner).
std::string OutputText(const char *Name, const char *Text);
// The same for Length bytes of Text (any bytes: a file's contents, a field that need not end in a 0).
std::string OutputText(const char *Name, const char *Text, size_t Length);

// The helpers of the ported tests (issue #26).
//
// The original's value: the oracle records Value, the runner returns the recorded one. ORIGINAL_VALUE
// (Name, Type, Expression) evaluates Expression in the oracle only (the runner has no original to call):
//	int B = ORIGINAL_VALUE("count", int, At<IntFn>(0x4a5bd0)(c));
template <class T> T Recorded(const char *Name, T Value)
{
	Output(Name, Value);
	return Value;
}
// A text of the original that may be NULL, by value: c_str() is NULL where the original's was.
struct TText {
	bool Null = true;
	std::string Text;
	const char *c_str() const { return Null ? nullptr : Text.c_str(); }
};
// Text (the oracle's; ignored by the runner), at most MaxLength characters (strnlen: for a field that
// need not be terminated, or a comparison with strncmp).
TText RecordedText(const char *Name, const char *Text, size_t MaxLength = (size_t)-1);
// A buffer of the original that changes little from one record to the next (the 1,000-byte string of
// a script reader): only the bytes that differ from Last, the buffer as recorded the time before, which
// the caller keeps (Size bytes; the same start value on both sides). Last becomes the original's
// buffer on both sides; Value is the oracle's (ignored by the runner).
void OutputChanges(const char *Name, void *Last, const void *Value, size_t Size);
// The same for a buffer that changes in several places at once (a table of the original, issue #27): the
// runs of changed bytes (runs closer than 16 bytes are one run), each its offset, length and bytes.
void OutputRuns(const char *Name, void *Last, const void *Value, size_t Size);
// A text of the original that changes little from one record to the next (a window test's dump, issue #28):
// what it keeps of Last's start and end, and the bytes between. Last, the text as recorded the time before,
// which the caller keeps (empty at first on both sides), becomes the original's text on both sides and is
// returned; Text is the oracle's (ignored by the runner).
std::string OutputTextChanges(const char *Name, std::string *Last, const std::string &Text);
// An array of the original whose length the runner knows from an earlier record (Count).
template <class T> void OutputArray(const char *Name, T *Data, size_t Count)
{
	static_assert(std::is_arithmetic<T>::value, "the corpus stores numbers");
	if (Mode() != TMode::Off)
		Exchange(Name, 'O', TypeCode<T>(), sizeof(T), Count, Data);
}

// A part of a test that the runner does not replay (issue #28: Dialogs' rounds of the advanced graphics
// dialog, whose engine buttons the x64 client replaces by its device buttons, #31; they retire with the
// oracle). The oracle runs the part and records two values around it: "retired" (an input, the part's
// name) before it, "retired end" (the part's checks and the number of values recorded in it) after it.
// The runner skips the part's values up to "retired end" and counts its checks as retired, apart from the
// ones it replays (RetiredChecks). What keeps the replay in step after the part (Random()'s seed, the
// test's own counters) the test passes through golden::Output after RetiredEnd, as any value of the
// original:
//	if (!golden::RetiredBegin("advanced graphics", Checks)) {
//		... the part (the oracle only) ...
//	}
//	golden::RetiredEnd(Checks);
//	uint32 Seed = GetRandomSeed();
//	golden::Output("seed", Seed);
//	SetRandomSeed(Seed);
// RetiredBegin returns true in the runner (the part was skipped), false in the oracle (run it).
bool RetiredBegin(const char *Name, int Checks);
void RetiredEnd(int Checks);
// The checks the runner skipped so far (all tests; 0 in the oracle). EndReplay expects the oracle's checks
// of a test to be the replayed ones plus the test's retired ones.
long long RetiredChecks();

// A data file next to Tibia.exe that both sides load (Tibia.dat; issue #27). The oracle records whether
// it exists and its contents, and returns its path (empty when it does not exist); the runner writes the
// recorded contents to a directory of its TEMP and returns that path. So the runner needs no data files:
// they are in the corpus, which is local like them (never committed, pushed or uploaded).
std::string DataFile(const char *Name, const char *ExePath);

// SHA-256 of a file, in hex; empty when it cannot be read.
std::string FileSha256(const char *Path);

} // namespace golden
