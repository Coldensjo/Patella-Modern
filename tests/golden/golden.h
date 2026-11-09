// The golden corpus (notes/modernization-plan.md, 4.3; issue #24): the original's inputs and outputs of
// the 32-bit oracle's differential comparisons, by value, so that the comparisons are replayed without Tibia.exe and on
// any platform.
//
// The ported tests (tests/golden/*_tests.cpp) were compiled twice until Patella-Modern issue #94: into the oracle's
// test executables, where the original's code ran inside ORIGINAL(...) and each of its results was recorded, and
// into the runner (tests/golden/runner.cpp), where ORIGINAL(...) is empty and golden::Output reads the original's
// result back from the corpus, golden::Input compares the input the test generated again with the recorded one. The
// oracle and its recorder left the fork with issue #94 (they are in upstream Patella and at the tag
// layout-frozen-final, where the frozen corpus of issue #93 was recorded); the runner is what is left. The
// ORIGINAL(...) arguments stay in the tests as the record of what the oracle's side ran.
//
// The format (version GOLDEN_FORMAT; little-endian, both targets are):
//	<dir>/<executable>/manifest.tsv   text, one line per field (tab-separated):
//	    format <version>, tibia_sha256 <hex>, executable <name>, then per test (the recording's order)
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
#include <vector>

// The original's side of a comparison: the oracle compiled it (until issue #94), the runner has none.
// ORIGINAL_OR(Value, Otherwise): Otherwise (the oracle's was Value).
// ORIGINAL_VALUE(Name, Type, Expression...): the original's value of Expression, as the oracle recorded it, read
// back from the corpus (golden::Recorded); ORIGINAL_TEXT(Name, Expression...) the same for a text that may be NULL
// (golden::TText).
#define ORIGINAL(...) ((void)0)
#define ORIGINAL_OR(Value, Otherwise) (Otherwise)
#define ORIGINAL_VALUE(Name, Type, ...) golden::Recorded<Type>(Name, Type{})
#define ORIGINAL_TEXT(Name, ...) golden::RecordedText(Name, nullptr)

namespace golden {

// 2 since Patella-Modern issue #76: an exception's records are its outcome (kind, value, type), no texts.
const uint32_t GOLDEN_FORMAT = 2;
// CLAUDE.md: Tibia.exe 7.72 of 2006-06-08.
const char *const TIBIA_SHA256 = "1a82fe97a39e536c29327d6d5dde316775f537dcc01506831429b1a0433b76f8";

// Off: no corpus (the tests run our side alone); Replay: a test of the corpus is replayed. (Record was the oracle's
// recorder, until issue #94.)
enum class TMode { Off,
	Replay };
TMode Mode();

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
// The corpus's MANIFEST.tsv (issue #93; tools/data_manifest.py has the format and writes it after golden_corpus's
// recording): every file's size and SHA-256, the commit, the tools, the date and the time zone of the recording.
const char *const DATA_MANIFEST = "MANIFEST.tsv";
const char *const DATA_MANIFEST_FORMAT = "patella-data-manifest 1";
struct TDataManifest {
	bool Found = false; // false: the directory has no MANIFEST.tsv
	std::string Kind, Commit, Clean, Date, TimeZone, TibiaSha256;
	long long Files = -1, Bytes = -1;
};
// Reads <directory>/MANIFEST.tsv and checks the directory against it: Problems gets every difference (none when
// every listed file is there with its size and SHA-256, no other file is, the counts are the manifest's and its
// kind is Kind). Without a manifest Manifest->Found is false and Problems gets that.
void VerifyDataManifest(
	const std::string &Directory, const char *Kind, TDataManifest *Manifest, std::vector<std::string> *Problems);
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
// Count checks of the running test retired after the corpus was frozen (issue #127): the recording has no "retired"
// markers around them, so the test reads their recorded values itself and calls this instead of comparing them.
void RetireChecks(long long Count);

// A deliberate difference (Patella-Modern issue #72; notes/decisions.md, "#72 and #86"): a part of a test from the
// point where our side differs from the original on purpose to the point where both sides are in step again. The
// frozen corpus cannot be recorded again (issue #93), so the part is not skipped as a retired one (which needs the
// oracle's records around it): the runner runs it and reads its values as everywhere (its inputs are still compared,
// a test that generates other inputs is still stale), and each of its checks still counts in Checks, but a
// difference inside it is not a failure: the check is counted as the issue's (Deliberates: its checks, and those that
// differed). The test opens the part where its own side reaches the difference (BeginDeliberate, with the issue) and
// closes it (EndDeliberate). Only the tests of the listed issues open one:
//	#72  network_tests.cpp: our PrepareNextPacket drops a frame whose inner length runs past it (a warn line in the
//	     developer log, the next frame prepared; until #139 it threw), where the original read on past the frame; the
//	     part runs to the end of the round (ResetSides).
//	#139 network_tests.cpp: the same for a frame whose size is no multiple of 8 (item 7; none in the corpus) and the
//	     part's name for any other drop of TNetwork's (TFrameDrop).
//	#166 dialogs_tests.cpp: the Info dialog's text, the fork's two lines where the corpus has the original's five (the
//	     owner's choice); a part around one check, opened only when the text explains its difference.
// A check's result: true when it is a failure (Same false, outside a deliberate part).
void BeginDeliberate(const char *Issue);
void EndDeliberate();
bool InDeliberate();
bool CheckFailed(bool Same);
struct TDeliberate {
	std::string Issue;
	long long Parts = 0;  // the parts opened
	long long Checks = 0; // the checks inside them
	long long Differ = 0; // those of them that differed
};
const std::vector<TDeliberate> &Deliberates();

// A data file next to Tibia.exe that both sides load (Tibia.dat; issue #27). The oracle records whether
// it exists and its contents, and returns its path (empty when it does not exist); the runner writes the
// recorded contents to a directory of its TEMP and returns that path. So the runner needs no data files:
// they are in the corpus, which is local like them (never committed, pushed or uploaded).
std::string DataFile(const char *Name, const char *ExePath);

// SHA-256 of a file, in hex; empty when it cannot be read.
std::string FileSha256(const char *Path);

} // namespace golden
