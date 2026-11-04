// Differential test of src/Extern/regexp/regexp.c (Henry Spencer's regexp of
// 1986 as the client has it, issue #15) against the image's regcomp
// (0x50fa40) and regexec (0x50f360).
//
// Both sides allocate from the test's heap: PatchImage (status_tests.cpp)
// redirects the image's malloc and free to ours, so _msize gives the size of
// both programs. The image's other callees (strcspn 0x511ee0, strchr
// 0x513620, strncmp 0x514250) are plain string functions without state.
// regerror's hook (the image's 0x71f034, which the client never sets, and our
// regerrorhandler) logs the message on each side; some rounds leave both
// hooks NULL, as in the client. Nothing reaches Windows.
//
// Compared: regcomp's result (NULL or not), the error messages, the size of
// the allocation, regstart, reganch, regmust (as an offset into the program)
// and regmlen, and every program byte; regexec's result, its messages and
// all ten startp/endp (as offsets into the string; both programs' arrays are
// set to the same sentinel before the call, so an untouched array is seen as
// such). regexec also runs on the other side's program, and on hand-made
// corrupted programs for its error paths.
//
// Inputs come from the test's own random numbers (RX_SEED), so the later
// tests' inputs are unchanged: random strings of pattern characters (mostly
// syntax errors), patterns from a small grammar (valid, with matches),
// hand-made patterns for every message and boundary, the client's LinkPattern
// (the image's 0x5c2560, used by Status.cpp) with URL-like texts, and the
// size limit (32767). RX_ROUNDS sets the number of rounds, RX_TRACE=1 prints
// the coverage counters. DIFFTEST_ONLY=Regexp runs this test alone.
//
// A golden-corpus test (golden.h; moved here from tools/difftest/regexptest.cpp, issue #24): the
// original's regcomp and regexec run in the oracle only (RxImageCompile, RxImageExecute), which records
// their results by value: the result, the messages, the program's fields, its size (the allocation
// less the header, sizeof(regexp) of x86) and its bytes, and regexec's startp/endp as offsets into the
// string. The runner rebuilds the original's program from them in its own layout (a malloc of
// sizeof(regexp) + the program's size, as regcomp allocates), so the comparisons, the cross runs
// (ours on the original's program) and the allocation sizes (_msize less sizeof(regexp): the program's
// size on either side) are the same code on both sides.

#include "golden_test.h"

#include "Extern/regexp/regexp.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace {

enum { RX_OURS = 0,
	RX_ORIGINAL = 1 };

#if GOLDEN_ORACLE
typedef regexp *(__cdecl *RxCompileFn)(char *Pattern);
typedef int(__cdecl *RxExecuteFn)(regexp *Program, char *Text);
typedef void(__cdecl *RxErrorFn)(char *Message);

RxCompileFn const ImageRegcomp = (RxCompileFn)(uintptr_t)0x50fa40;
RxExecuteFn const ImageRegexec = (RxExecuteFn)(uintptr_t)0x50f360;
const uint32 RX_IMAGE_HANDLER = 0x71f034;
const uint32 RX_IMAGE_LINKPATTERN = 0x5c2560;
#endif

uint32 RxSeed = 15;
uint32 RxRandom()
{
	RxSeed = RxSeed * 1103515245 + 12345;
	return RxSeed >> 8;
}
int RxBelow(int n) { return n <= 0 ? 0 : (int)(RxRandom() % (uint32)n); }

std::string RxLog[2];
bool RxTrace = false;

// Coverage counters (RX_TRACE=1).
struct TRxCoverage {
	int Messages[32];
	int Opcodes[256];
	int Compiled, CompileFailed, Matched, NotMatched, RegMust, RegMustRejected, Anchored, RegStart;
} RxCoverage;

const char *const RxMessages[] = {"NULL argument", "regexp too big", "out of space", "too many ()",
	"unmatched ()", "junk on end", "*+ operand could be empty", "nested *?+", "invalid [] range",
	"unmatched []", "internal urp", "?+* follows nothing", "trailing \\", "internal disaster",
	"NULL parameter", "corrupted program", "memory corruption", "corrupted pointers", "internal foulup"};
const int RX_MESSAGES = sizeof(RxMessages) / sizeof(RxMessages[0]);

void RxCount(const char *Message)
{
	for (int i = 0; i < RX_MESSAGES; i++)
		if (strcmp(Message, RxMessages[i]) == 0) {
			RxCoverage.Messages[i]++;
			return;
		}
	RxCoverage.Messages[RX_MESSAGES]++;
}

#if GOLDEN_ORACLE
void __cdecl RxImageError(char *Message)
{
	RxLog[RX_ORIGINAL] += "E:";
	RxLog[RX_ORIGINAL] += Message;
	RxLog[RX_ORIGINAL] += ";";
}
#endif

void RxOurError(char *Message)
{
	RxLog[RX_OURS] += "E:";
	RxLog[RX_OURS] += Message;
	RxLog[RX_OURS] += ";";
	RxCount(Message);
}

void RxSetHandlers(bool On)
{
	ORIGINAL(Global<RxErrorFn>(RX_IMAGE_HANDLER) = On ? RxImageError : NULL);
	regerrorhandler = On ? RxOurError : NULL;
}

void RxClearLogs()
{
	RxLog[RX_OURS].clear();
	RxLog[RX_ORIGINAL].clear();
}

std::string RxShow(const char *Text, size_t Max = 60)
{
	std::string Out;
	for (size_t i = 0; Text[i] != 0 && i < Max; i++) {
		unsigned char c = (unsigned char)Text[i];
		if (c >= 0x20 && c < 0x7f)
			Out += (char)c;
		else {
			char Hex[8];
			snprintf(Hex, sizeof(Hex), "\\x%02x", c);
			Out += Hex;
		}
	}
	if (strlen(Text) > Max)
		Out += "...";
	return Out;
}

// The program of a compiled expression; regsize = allocation - sizeof(regexp).
size_t RxProgramSize(regexp *Program) { return _msize(Program) - sizeof(regexp); }

char *const RX_UNTOUCHED = (char *)(uintptr_t)1;

long RxOffset(char *p, char *Text)
{
	if (p == NULL)
		return -1;
	if (p == RX_UNTOUCHED)
		return -2;
	return (long)(p - Text);
}

// --- The original's side ---------------------------------------------------

// The original's messages of the call (RxLog[RX_ORIGINAL] since RxClearLogs).
void RxImageMessages()
{
	std::string Log = golden::OutputText("messages", RxLog[RX_ORIGINAL].c_str());
	RxLog[RX_ORIGINAL] = Log;
}

// The original's regcomp; the runner rebuilds its program from the recorded fields.
regexp *RxImageCompile([[maybe_unused]] char *Pattern)
{
	regexp *Program = NULL;
	ORIGINAL(Program = ImageRegcomp(Pattern));
	RxImageMessages();
	uint8 Compiled = Program != NULL;
	golden::Output("compiled", Compiled);
	if (Compiled == 0)
		return NULL;
	// The program's size, its fields (regmust as an offset into the program) and its bytes.
	int32_t Fields[4] = {};
#if GOLDEN_ORACLE
	Fields[0] = (int32_t)RxProgramSize(Program);
	Fields[1] = Program->regstart;
	Fields[2] = Program->reganch;
	Fields[3] = Program->regmlen;
#endif
	golden::Output("program fields", Fields);
	int32_t Must = ORIGINAL_OR(Program->regmust != NULL ? (int32_t)(Program->regmust - Program->program) : -1, 0);
	golden::Output("regmust", Must);
#if !GOLDEN_ORACLE
	Program = (regexp *)malloc(sizeof(regexp) + (size_t)Fields[0]);
	if (Program == NULL)
		return NULL;
	memset(Program, 0, sizeof(regexp));
	Program->regstart = (char)Fields[1];
	Program->reganch = (char)Fields[2];
	Program->regmlen = Fields[3];
	Program->regmust = Must >= 0 ? Program->program + Must : NULL;
#endif
	golden::OutputBytes("program", Program->program, (size_t)Fields[0]);
	return Program;
}

// The original's regexec; the runner sets startp/endp of Program as the original left them.
int RxImageExecute(regexp *Program, char *Text)
{
	int Result = 0;
	ORIGINAL(Result = ImageRegexec(Program, Text));
	golden::Output("matched", Result);
	RxImageMessages();
	if (Program != NULL && Text != NULL) {
		int32_t Offsets[2 * NSUBEXP];
		for (int i = 0; i < NSUBEXP; i++) {
			Offsets[i] = (int32_t)RxOffset(Program->startp[i], Text);
			Offsets[NSUBEXP + i] = (int32_t)RxOffset(Program->endp[i], Text);
		}
		golden::Output("startp endp", Offsets);
		for (int i = 0; i < 2 * NSUBEXP; i++) {
			char *p = Offsets[i] == -1 ? NULL : Offsets[i] == -2 ? RX_UNTOUCHED
																 : Text + Offsets[i];
			if (i < NSUBEXP)
				Program->startp[i] = p;
			else
				Program->endp[i - NSUBEXP] = p;
		}
	}
	return Result;
}

// Compiles Pattern on both sides and compares the results; the programs are
// returned (NULL where compilation failed) for regexec.
void RxCompileBoth(char *Pattern, regexp **Ours, regexp **Original)
{
	RxClearLogs();
	regexp *A = regcomp(Pattern);
	regexp *B = RxImageCompile(Pattern);
	*Ours = A;
	*Original = B;
	std::string Shown = RxShow(Pattern);
	CHECK((A == NULL) == (B == NULL), "regcomp(\"%s\"): ours %s, original %s", Shown.c_str(),
		A ? "a program" : "NULL", B ? "a program" : "NULL");
	CHECK(RxLog[RX_OURS] == RxLog[RX_ORIGINAL], "regcomp(\"%s\") messages: ours \"%s\", original \"%s\"",
		Shown.c_str(), RxLog[RX_OURS].c_str(), RxLog[RX_ORIGINAL].c_str());
	if (A == NULL || B == NULL) {
		RxCoverage.CompileFailed++;
		return;
	}
	RxCoverage.Compiled++;
	size_t SizeA = _msize(A), SizeB = _msize(B);
	CHECK(SizeA == SizeB, "regcomp(\"%s\"): allocation %u, original %u", Shown.c_str(), (unsigned)SizeA, (unsigned)SizeB);
	CHECK(A->regstart == B->regstart && A->reganch == B->reganch && A->regmlen == B->regmlen,
		"regcomp(\"%s\"): regstart %d/%d, reganch %d/%d, regmlen %d/%d", Shown.c_str(), A->regstart, B->regstart,
		A->reganch, B->reganch, A->regmlen, B->regmlen);
	long MustA = A->regmust ? (long)(A->regmust - A->program) : -1;
	long MustB = B->regmust ? (long)(B->regmust - B->program) : -1;
	CHECK(MustA == MustB, "regcomp(\"%s\"): regmust at %ld, original %ld", Shown.c_str(), MustA, MustB);
	if (SizeA == SizeB) {
		size_t Size = RxProgramSize(A);
		size_t Diff = Size;
		for (size_t i = 0; i < Size; i++)
			if (A->program[i] != B->program[i]) {
				Diff = i;
				break;
			}
		CHECK(Diff == Size, "regcomp(\"%s\"): program byte %u is 0x%02x, original 0x%02x", Shown.c_str(),
			(unsigned)Diff, Diff < Size ? (uint8)A->program[Diff] : 0, Diff < Size ? (uint8)B->program[Diff] : 0);
		for (size_t i = 0; i < Size; i++)
			RxCoverage.Opcodes[(uint8)A->program[i]]++;
	}
	if (A->regmust != NULL)
		RxCoverage.RegMust++;
	if (A->reganch)
		RxCoverage.Anchored++;
	if (A->regstart != 0)
		RxCoverage.RegStart++;
}

std::string RxSubexpressions(regexp *Program, char *Text)
{
	std::string Out;
	char Part[32];
	for (int i = 0; i < NSUBEXP; i++) {
		snprintf(Part, sizeof(Part), "%ld-%ld ", RxOffset(Program->startp[i], Text), RxOffset(Program->endp[i], Text));
		Out += Part;
	}
	return Out;
}

void RxMarkUntouched(regexp *Program)
{
	if (Program == NULL)
		return;
	for (int i = 0; i < NSUBEXP; i++)
		Program->startp[i] = Program->endp[i] = RX_UNTOUCHED;
}

// regexec on both sides: ours runs Ours, the image runs Original.
void RxExecuteBoth(regexp *Ours, regexp *Original, char *Text, const char *What)
{
	RxMarkUntouched(Ours);
	RxMarkUntouched(Original);
	RxClearLogs();
	int A = regexec(Ours, Text);
	int B = RxImageExecute(Original, Text);
	std::string Shown = RxShow(Text != NULL ? Text : "(null)");
	CHECK(A == B, "%s regexec(\"%s\"): ours %d, original %d", What, Shown.c_str(), A, B);
	CHECK(RxLog[RX_OURS] == RxLog[RX_ORIGINAL], "%s regexec(\"%s\") messages: ours \"%s\", original \"%s\"", What,
		Shown.c_str(), RxLog[RX_OURS].c_str(), RxLog[RX_ORIGINAL].c_str());
	if (Ours != NULL && Original != NULL && Text != NULL) {
		std::string SA = RxSubexpressions(Ours, Text), SB = RxSubexpressions(Original, Text);
		CHECK(SA == SB, "%s regexec(\"%s\"): startp/endp %s, original %s", What, Shown.c_str(), SA.c_str(), SB.c_str());
		if (Ours->regmust != NULL && A == 0 && Ours->startp[0] == RX_UNTOUCHED)
			RxCoverage.RegMustRejected++;
	}
	if (A)
		RxCoverage.Matched++;
	else
		RxCoverage.NotMatched++;
}

// Both programs on both sides (the cross runs need identical programs).
void RxExecuteAll(regexp *Ours, regexp *Original, char *Text)
{
	RxExecuteBoth(Ours, Original, Text, "own");
	if (Ours != NULL && Original != NULL && _msize(Ours) == _msize(Original)
		&& memcmp(Ours->program, Original->program, RxProgramSize(Ours)) == 0)
		RxExecuteBoth(Original, Ours, Text, "cross");
}

void RxFree(regexp *Ours, regexp *Original)
{
	free(Ours);
	free(Original);
}

// --- Inputs ----------------------------------------------------------------

const char RxPatternChars[] = "^$.[]()|?+*\\-abc-ab]a(b)a|b";

void RxRandomPattern(char *Out, int MaxLength)
{
	int Length = RxBelow(MaxLength + 1);
	for (int i = 0; i < Length; i++) {
		int r = RxBelow(40);
		if (r == 0)
			Out[i] = '\xe4';
		else if (r == 1)
			Out[i] = 'x';
		else
			Out[i] = RxPatternChars[RxBelow(sizeof(RxPatternChars) - 1)];
	}
	Out[Length] = 0;
}

// A pattern from a small grammar of valid expressions (and some invalid
// quantifiers on purpose), over the letters a, b and c.
void RxGrammarAtom(std::string &Out, int Depth);

void RxGrammarBranch(std::string &Out, int Depth)
{
	int Pieces = 1 + RxBelow(Depth >= 2 ? 2 : 4);
	for (int i = 0; i < Pieces; i++) {
		RxGrammarAtom(Out, Depth);
		int q = RxBelow(10);
		if (q == 0)
			Out += '*';
		else if (q == 1)
			Out += '+';
		else if (q == 2)
			Out += '?';
	}
}

void RxGrammarExpression(std::string &Out, int Depth)
{
	int Branches = 1 + (RxBelow(3) == 0 ? RxBelow(3) : 0);
	for (int i = 0; i < Branches; i++) {
		if (i > 0)
			Out += '|';
		if (RxBelow(12) != 0)
			RxGrammarBranch(Out, Depth);
	}
}

void RxGrammarAtom(std::string &Out, int Depth)
{
	static const char Letters[] = "abcab";
	int r = RxBelow(Depth >= 2 ? 14 : 18);
	if (r < 6) {
		int n = 1 + RxBelow(3);
		for (int i = 0; i < n; i++)
			Out += Letters[RxBelow(5)];
	} else if (r < 7)
		Out += '.';
	else if (r < 8)
		Out += RxBelow(2) ? '^' : '$';
	else if (r < 9) {
		Out += '\\';
		Out += "a.*[(b\\|"[RxBelow(8)];
	} else if (r < 14) {
		Out += '[';
		if (RxBelow(3) == 0)
			Out += '^';
		if (RxBelow(6) == 0)
			Out += RxBelow(2) ? ']' : '-';
		int n = 1 + RxBelow(3);
		for (int i = 0; i < n; i++) {
			Out += Letters[RxBelow(5)];
			if (RxBelow(4) == 0) {
				Out += '-';
				Out += "abcz"[RxBelow(4)];
			}
		}
		if (RxBelow(8) == 0)
			Out += '-';
		Out += ']';
	} else {
		Out += '(';
		RxGrammarExpression(Out, Depth + 1);
		Out += ')';
	}
}

void RxRandomText(char *Out, int MaxLength, const char *Pattern)
{
	static const char Others[] = "abcabcab.x-]\xe4";
	int Length = RxBelow(MaxLength + 1);
	int PatternLength = (int)strlen(Pattern);
	for (int i = 0; i < Length; i++) {
		if (PatternLength > 0 && RxBelow(3) == 0)
			Out[i] = Pattern[RxBelow(PatternLength)];
		else
			Out[i] = Others[RxBelow(sizeof(Others) - 1)];
		if (Out[i] == 0)
			Out[i] = 'a';
	}
	Out[Length] = 0;
}

// URL-like texts for LinkPattern (lower case, as Status.cpp passes them).
void RxLinkText(char *Out, int Size)
{
	static const char *const Parts[] = {"www", "tibia", "com", "de", "net", "org", "museum", "travel", "xx",
		"q", "1", "12", "123", "255", "0", "a-b", "-", "co", "uk", "info", "jobs", "mobi", "name", "zz9",
		"%2e", ".", ".", ".", "..", " ", ",", ":", ";", "/", "@", "<", ">", "|", "(", ")", "\"", "\n", "\r",
		"%", "?", "#", "&", "x", "e", "2", "http://", "cipsoft", "aero", "arpa", "biz", "coop", "pro",
		"edu", "gov", "mil", "int", "eu", "yu", "zw"};
	int n = RxBelow(12);
	std::string s;
	for (int i = 0; i < n && s.size() < 60; i++)
		s += Parts[RxBelow(sizeof(Parts) / sizeof(Parts[0]))];
	if (RxBelow(10) == 0)
		s += (char)(0x80 + RxBelow(0x80));
	strncpy(Out, s.c_str(), Size - 1);
	Out[Size - 1] = 0;
}

void RxPatternWithTexts(char *Pattern, int Texts, int MaxLength)
{
	regexp *A, *B;
	RxCompileBoth(Pattern, &A, &B);
	if (A != NULL && B != NULL) {
		char Text[64];
		for (int t = 0; t < Texts; t++) {
			RxRandomText(Text, MaxLength, Pattern);
			RxExecuteAll(A, B, Text);
		}
	}
	RxFree(A, B);
}

// A program made by hand (regexec's error paths): Code follows the magic byte.
regexp *RxMakeProgram(const uint8 *Code, int Length, int Magic)
{
	regexp *Program = (regexp *)malloc(sizeof(regexp) + Length + 1);
	memset(Program, 0, sizeof(regexp) + Length + 1);
	Program->program[0] = (char)Magic;
	memcpy(Program->program + 1, Code, Length);
	return Program;
}

void RxHandMadePrograms()
{
	// Each is executed on the same bytes by both sides.
	struct TProgram {
		uint8 Code[16];
		int Length;
	};
	const TProgram Programs[] = {
		{{9, 0, 0}, 3},                    // NOTHING without a next node: "corrupted pointers"
		{{8, 0, 0, 'a', 0}, 5},            // EXACTLY "a", then no next node
		{{10, 0, 6, 6, 0, 0, 0, 0, 0}, 9}, // STAR of a BRANCH: "internal foulup", then END
		{{11, 0, 6, 6, 0, 0, 0, 0, 0}, 9}, // PLUS of a BRANCH: "internal foulup", fails
		{{10, 0, 6, 7, 0, 0, 0, 0, 0}, 9}, // STAR of a BACK
		{{6, 0, 3, 7, 0, 0}, 6},           // BRANCH to a BACK without a next node
		{{12, 0, 0}, 3},                   // opcode 12: "memory corruption"
		{{19, 0, 0}, 3},                   // opcode 19
		{{20, 0, 3, 0, 0, 0}, 6},          // OPEN+0 (not a case): "memory corruption"
		{{30, 0, 3, 0, 0, 0}, 6},          // CLOSE+0
		{{40, 0, 0}, 3},                   // past CLOSE+9
		{{0x80, 0, 0}, 3},                 // a negative opcode
		{{0xff, 0, 0}, 3},
		{{21, 0, 3, 31, 0, 3, 0, 0, 0}, 9}, // OPEN1 CLOSE1 END
		{{1, 0, 3, 2, 0, 3, 0, 0, 0}, 9},   // BOL EOL END
	};
	const char *Texts[] = {"", "a", "ab", "ba", "aaa", "\xe4"};
	for (const TProgram &P : Programs) {
		for (const char *T : Texts) {
			regexp *A = RxMakeProgram(P.Code, P.Length, 0x9c);
			regexp *B = RxMakeProgram(P.Code, P.Length, 0x9c);
			char Text[8];
			strcpy(Text, T);
			RxExecuteBoth(A, B, Text, "hand-made");
			RxFree(A, B);
		}
	}
	// A wrong magic byte, a NULL program, a NULL text.
	regexp *A, *B;
	RxCompileBoth((char *)"abc", &A, &B);
	if (A != NULL && B != NULL) {
		char Text[] = "xabcx";
		A->program[0] = B->program[0] = '\x9d';
		RxExecuteBoth(A, B, Text, "magic 0x9d");
		A->program[0] = B->program[0] = 0;
		RxExecuteBoth(A, B, Text, "magic 0");
		A->program[0] = B->program[0] = '\x9c';
		RxExecuteBoth(A, B, NULL, "NULL text");
	}
	RxFree(A, B);
	char Text[] = "abc";
	RxExecuteBoth(NULL, NULL, Text, "NULL program");
	RxExecuteBoth(NULL, NULL, NULL, "NULL both");
	// regcomp(NULL)
	RxClearLogs();
	regexp *P = regcomp(NULL);
	regexp *Q = RxImageCompile(NULL);
	CHECK(P == NULL && Q == NULL, "regcomp(NULL): ours %p, original %p", (void *)P, (void *)Q);
	CHECK(RxLog[RX_OURS] == RxLog[RX_ORIGINAL], "regcomp(NULL) messages: ours \"%s\", original \"%s\"",
		RxLog[RX_OURS].c_str(), RxLog[RX_ORIGINAL].c_str());
}

const char *const RxFixedPatterns[] = {"", "a", "abc", "^abc$", "^", "$", "^$", "a*", "a+", "a?", "a*b", "a+b",
	"a?b", "ab*c", "ab+c", "x*", ".", ".*", ".+", "a.c", "(a)", "(a)*", "(ab)*", "(ab)+", "(ab)?", "(a|b)*c",
	"(a|b)+", "a|b|c", "ab|cd", "|a", "a|", "()", "(|a)", "(a|)", "[abc]", "[^abc]", "[a-c]", "[]a]", "[^]a]",
	"[-a]", "[a-]", "[a-a]", "[b-a]", "[c-a]", "[a-b-c]", "[]-a]", "[", "[a", "[^", "]", "\\", "a\\", "\\.",
	"\\a", "\\\\", "\\*", "a\\*", "*a", "+", "?", "*", ")", "(", "a)", "(a", "((a)", "(a))", "a**", "a*+",
	"a+?", "a??", "(a*)*", "(a*)+", "(a?)*", "(a+)*", "(a+)+", "(a|b*)*", "((a))", "(((((((((a)))))))))",
	"((((((((((a))))))))))", "(a)(b)(c)(d)(e)(f)(g)(h)(i)", "(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)", "^*", "$*", "^+",
	"(^a)", "(a$)", "a^b", "a$b", "abc*", "abcd+", "ab?", "abc|abd", "(abc|abd)e", "a.*b.*c", "x*y*z*",
	"[0-9][0-9]?[0-9]?", "[a-z0-9-]+", "(\\.|\\%2e)", "([a-z]+\\.)+com", "a[bc]+d", "a[^bc]*d", "a(b|c)*d",
	"\xe4", "\xe4*", "[\xe4-\xff]", "[a-\xe4]", "\xe4|a", "[\xff-a]", "a.b.c.", ".a.b", "^.*$", "(a|ab)(c|bcd)(d*)"};

void RxFixed()
{
	for (const char *Pattern : RxFixedPatterns) {
		char Copy[64];
		strcpy(Copy, Pattern);
		RxPatternWithTexts(Copy, 24, 12);
	}
}

void RxSizeLimit()
{
	// regsize = 1 (magic) + 3 (BRANCH) + 3 + length + 1 (EXACTLY) + 3 (END): 32766 fits, 32767 does not.
	const int Lengths[] = {32755, 32756, 32757, 40000};
	for (int Length : Lengths) {
		char *Pattern = (char *)malloc(Length + 3);
		for (int i = 0; i < Length; i++)
			Pattern[i] = 'a' + i % 3;
		Pattern[Length] = 0;
		regexp *A, *B;
		RxCompileBoth(Pattern, &A, &B);
		if (A != NULL && B != NULL) {
			char *Text = (char *)malloc(Length + 3);
			memcpy(Text, Pattern, Length + 1);
			RxExecuteAll(A, B, Text);
			Text[Length - 1] = 'x';
			RxExecuteAll(A, B, Text);
			free(Text);
		}
		RxFree(A, B);
		// A star near the limit: x* makes it STAR plus EXACTLY.
		Pattern[Length - 2] = '*';
		RxCompileBoth(Pattern, &A, &B);
		RxFree(A, B);
		free(Pattern);
	}
}

void RxLinkPattern(int Rounds)
{
	// The original's LinkPattern (Status.cpp's is ours).
	std::string Link = golden::OutputText("link pattern", ORIGINAL_OR(Global<char *>(RX_IMAGE_LINKPATTERN), ""));
	char *Pattern = &Link[0];
	regexp *A, *B;
	RxCompileBoth(Pattern, &A, &B);
	CHECK(A != NULL && B != NULL, "LinkPattern does not compile");
	if (A != NULL && B != NULL) {
		static const char *const Known[] = {"www.tibia.com", "see www.tibia.com.", "tibia.com", "x.museum",
			"1.2.3.4", "1.2.3", "255.255.255.255/", "123.1%2e2.3", "a.b", "a.b.c.de", "hello world", "",
			"(www.tibia.de)", "mail@tibia.com", "ftp:\"x.org\"", "x.travel?y", "x.travelx", "x.co.uk#",
			"x%2ecom", "x.com.", "-.com", "1.2.3.4.5", ".com", "x..com", "\x80tibia.com", "x.c\xe4m"};
		for (const char *K : Known) {
			char Text[64];
			strcpy(Text, K);
			RxExecuteAll(A, B, Text);
		}
		for (int i = 0; i < Rounds; i++) {
			char Text[64];
			RxLinkText(Text, sizeof(Text));
			RxExecuteAll(A, B, Text);
		}
	}
	RxFree(A, B);
}

} // namespace

void TestRegexp()
{
	int Before = Checks;
	ORIGINAL(PatchImage());
	if (getenv("RX_SEED") != NULL)
		RxSeed = (uint32)strtoul(getenv("RX_SEED"), NULL, 0);
	int Rounds = getenv("RX_ROUNDS") != NULL ? atoi(getenv("RX_ROUNDS")) : 4000;
	golden::Input("seed", RxSeed);
	golden::Input("rounds", Rounds);
	RxTrace = getenv("RX_TRACE") != NULL;
	memset(&RxCoverage, 0, sizeof(RxCoverage));

	RxSetHandlers(true);
	RxFixed();
	RxHandMadePrograms();
	RxSizeLimit();
	RxLinkPattern(Rounds / 2);
	for (int i = 0; i < Rounds; i++) {
		// The client's state (no handler) in every fourth round.
		RxSetHandlers(i % 4 != 3);
		char Pattern[64];
		RxRandomPattern(Pattern, 16);
		RxPatternWithTexts(Pattern, 4, 10);
		std::string G;
		RxGrammarExpression(G, 0);
		if (G.size() < 60) {
			strcpy(Pattern, G.c_str());
			RxPatternWithTexts(Pattern, 8, 14);
		}
	}
	RxSetHandlers(false);
	RxHandMadePrograms();
	RxLinkPattern(20);

	if (RxTrace) {
		printf("regexp: compiled %d, failed %d; matched %d, not %d; regmust %d (rejected %d), anchored %d, regstart %d\n",
			RxCoverage.Compiled, RxCoverage.CompileFailed, RxCoverage.Matched, RxCoverage.NotMatched,
			RxCoverage.RegMust, RxCoverage.RegMustRejected, RxCoverage.Anchored, RxCoverage.RegStart);
		for (int i = 0; i <= RX_MESSAGES; i++)
			printf("regexp: message %-28s %d\n", i < RX_MESSAGES ? RxMessages[i] : "(other)", RxCoverage.Messages[i]);
		static const char *const Ops[] = {"END", "BOL", "EOL", "ANY", "ANYOF", "ANYBUT", "BRANCH", "BACK",
			"EXACTLY", "NOTHING", "STAR", "PLUS"};
		for (int i = 0; i < 12; i++)
			printf("regexp: program bytes 0x%02x (%s) %d\n", i, Ops[i], RxCoverage.Opcodes[i]);
		for (int i = 21; i < 40; i++)
			if (i != 30)
				printf("regexp: program bytes 0x%02x (%s%d) %d\n", i, i < 30 ? "OPEN" : "CLOSE", i < 30 ? i - 20 : i - 30,
					RxCoverage.Opcodes[i]);
	}
	printf("Regexp: %d checks\n", Checks - Before);
}
