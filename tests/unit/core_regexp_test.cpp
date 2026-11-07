// The regexp (src/Extern/regexp, Henry Spencer's of 1986, compiled as C17) through TRegexp (types/Regexp.h, issue
// #129), which owns regcomp's program: compiling, matching, the subexpressions, the errors, the ownership, and
// Status.cpp's link check with Status.h's LinkPattern.

#include <doctest/doctest.h>

#include "Extern/regexp/regexp.h"
#include "types/Regexp.h"

#include <cstddef>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string_view with operator<<)
#include <string>
#include <string_view>
#include <utility>

namespace {

// Status.h's LinkPattern (Status.h is not part of patella_core yet).
const char LinkPattern[] =
	"(^|[ .,:;/@<>|()\"\n\r%])(([a-z0-9-]+(\\.|\\%2e))+(ac|ad|ae|aero|af|ag|ai|al|am|an|ao|aq|a"
	"r|arpa|as|at|au|aw|az|ba|bb|bd|be|bf|bg|bh|bi|biz|bj|bm|bn|bo|br|bs|bt|bv|bw|by|bz|ca|cc|c"
	"d|cf|cg|ch|ci|ck|cl|cm|cn|co|com|coop|cr|cu|cv|cx|cy|cz|de|dj|dk|dm|do|dz|ec|edu|ee|eg|er|"
	"es|et|eu|fi|fj|fk|fm|fo|fr|ga|gb|gd|ge|gf|gg|gh|gi|gl|gm|gn|gov|gp|gq|gr|gs|gt|gu|gw|gy|hk"
	"|hm|hn|hr|ht|hu|id|ie|il|im|in|info|int|io|iq|ir|is|it|je|jm|jo|jobs|jp|ke|kg|kh|ki|km|kn|"
	"kr|kw|ky|kz|la|lb|lc|li|lk|lr|ls|lt|lu|lv|ly|ma|mc|md|mg|mh|mil|mk|ml|mm|mn|mo|mobi|mp|mq|"
	"mr|ms|mt|mu|museum|mv|mw|mx|my|mz|na|name|nc|ne|net|nf|ng|ni|nl|no|np|nr|nu|nz|om|org|pa|p"
	"e|pf|pg|ph|pk|pl|pm|pn|pr|pro|ps|pt|pw|py|qa|re|ro|ru|rw|sa|sb|sc|sd|se|sg|sh|si|sj|sk|sl|"
	"sm|sn|so|sr|st|su|sv|sy|sz|tc|td|tf|tg|th|tj|tk|tl|tm|tn|to|tp|tr|travel|tt|tv|tw|tz|ua|ug"
	"|uk|um|us|uy|uz|va|vc|ve|vg|vi|vn|vu|wf|ws|ye|yt|yu|za|zm|zw)\\.?|[0-9][0-9]?[0-9]?(\\.|\\"
	"%2e)[0-9][0-9]?[0-9]?(\\.|\\%2e)[0-9][0-9]?[0-9]?(\\.|\\%2e)[0-9][0-9]?[0-9]?)([?#& .,:;/@"
	"<>|()\"\r\n%]|$)";

std::string LastError;

void RecordError(char *Message)
{
	LastError = Message;
}

// Sets regerrorhandler for one test and clears it again (the client never sets it).
struct TErrorRecorder {
	TErrorRecorder()
	{
		LastError.clear();
		regerrorhandler = RecordError;
	}
	~TErrorRecorder() { regerrorhandler = nullptr; }
	TErrorRecorder(const TErrorRecorder &) = delete;
	TErrorRecorder &operator=(const TErrorRecorder &) = delete;
	TErrorRecorder(TErrorRecorder &&) = delete;
	TErrorRecorder &operator=(TErrorRecorder &&) = delete;
};

} // namespace

using patella::TRegexp;

TEST_CASE("regexp: matching and subexpressions")
{
	TRegexp Program("([a-z]+)@([a-z]+)\\.(com|de)$");
	REQUIRE(Program.IsCompiled());
	const std::string Text = "mail: knight@tibia.com";
	CHECK(Program.Execute(Text.c_str()) == 1);
	CHECK(Program.Subexpression(0) == "knight@tibia.com");
	CHECK(Program.Subexpression(1) == "knight");
	CHECK(Program.Subexpression(2) == "tibia");
	CHECK(Program.Subexpression(3) == "com");
	// The subexpressions point into the text itself.
	CHECK(Program.Subexpression(0).data() == Text.c_str() + 6);
	CHECK(Program.Subexpression(4).empty());
	CHECK(Program.Subexpression(-1).empty());
	CHECK(Program.Subexpression(NSUBEXP).empty());
	CHECK(Program.Execute("knight@tibia.org") == 0);
	CHECK(Program.Execute("knight@tibia.com ") == 0);

	Program = TRegexp("^a*b+c?[^x]$");
	REQUIRE(Program);
	CHECK(Program.Execute("bbz") == 1);
	CHECK(Program.Execute("aaabcd") == 1);
	CHECK(Program.Execute("aaab") == 0);
	CHECK(Program.Execute("abx") == 0);

	// Latin-1 characters in a class (UCHARAT reads them unsigned).
	Program = TRegexp("^[\xe4\xf6\xfc]+$");
	REQUIRE(Program);
	CHECK(Program.Execute("\xe4\xfc\xf6") == 1);
	CHECK(Program.Execute("\xe4u") == 0);

	// Leftmost-first alternation, not POSIX's longest match (one reason std::regex is no drop-in).
	Program = TRegexp("(a|ab)(c|bcd)");
	REQUIRE(Program);
	CHECK(Program.Execute("abcd") == 1);
	CHECK(Program.Subexpression(0) == "abcd");
	CHECK(Program.Subexpression(1) == "a");
	CHECK(Program.Subexpression(2) == "bcd");

	// A subexpression that takes no part in the match.
	Program = TRegexp("x(y)?z");
	REQUIRE(Program);
	CHECK(Program.Execute("xz") == 1);
	CHECK(Program.Subexpression(0) == "xz");
	CHECK(Program.Subexpression(1).empty());
}

TEST_CASE("regexp: the program's fields")
{
	// regstart, reganch and regmust, the hints regexec uses (regexp.h).
	const TRegexp Anchored("^abc");
	REQUIRE(Anchored.Get() != nullptr);
	CHECK(Anchored.Get()->reganch == 1);
	CHECK(Anchored.Get()->regstart == 0); // the first node is BOL, not EXACTLY
	const TRegexp Plain("abc|abd");
	REQUIRE(Plain.Get() != nullptr);
	CHECK(Plain.Get()->reganch == 0);
	CHECK(Plain.Get()->regstart == 0); // two branches: no single first node
	const TRegexp Literal("abc");
	REQUIRE(Literal.Get() != nullptr);
	CHECK(Literal.Get()->regstart == 'a');
	const TRegexp Star("x*abcd");
	REQUIRE(Star.Get() != nullptr);
	REQUIRE(Star.Get()->regmust != nullptr);
	CHECK(std::string_view(Star.Get()->regmust, static_cast<std::size_t>(Star.Get()->regmlen)) == "abcd");
	// The program starts with the magic byte 0234 (regmagic.h).
	CHECK(static_cast<unsigned char>(Star.Get()->program[0]) == 0234);
}

TEST_CASE("regexp: errors go to regerrorhandler")
{
	CHECK(regerrorhandler == nullptr);
	CHECK_FALSE(TRegexp("a(b").IsCompiled());
	const TErrorRecorder Recorder;
	CHECK_FALSE(TRegexp("a(b"));
	CHECK(LastError == "unmatched ()");
	CHECK_FALSE(TRegexp("a**"));
	CHECK(LastError == "nested *?+");
	CHECK_FALSE(TRegexp("a)"));
	CHECK(LastError == "unmatched ()");
	CHECK_FALSE(TRegexp("[a"));
	CHECK(LastError == "unmatched []");
	CHECK_FALSE(TRegexp("a\\"));
	CHECK(LastError == "trailing \\");
	CHECK_FALSE(TRegexp("*a"));
	CHECK(LastError == "?+* follows nothing");
	CHECK_FALSE(TRegexp(nullptr));
	CHECK(LastError == "NULL argument");
	// Executing an empty TRegexp is regexec(NULL, ...).
	TRegexp Empty;
	LastError.clear();
	CHECK(Empty.Execute("text") == 0);
	CHECK(LastError == "NULL parameter");
	CHECK(Empty.Subexpression(0).empty());
	CHECK(Empty.Get() == nullptr);
	// And a NULL text.
	TRegexp Program("a");
	REQUIRE(Program);
	LastError.clear();
	CHECK(Program.Execute(nullptr) == 0);
	CHECK(LastError == "NULL parameter");
}

TEST_CASE("regexp: TRegexp owns the program")
{
	TRegexp First("abc");
	const regexp *const Compiled = First.Get();
	REQUIRE(Compiled != nullptr);
	// Moving hands the program over; the moved-from one is empty.
	TRegexp Second(std::move(First));
	CHECK(Second.Get() == Compiled);
	CHECK_FALSE(First.IsCompiled()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move): the moved-from state is the test
	CHECK(Second.Execute("xabcx") == 1);
	// Assigning frees the old program (ASan's leak check on Linux, the CRT's on the Debug presets).
	Second = TRegexp("def");
	CHECK(Second.Execute("abc") == 0);
	CHECK(Second.Execute("def") == 1);
	Second.Reset();
	CHECK_FALSE(Second);
	CHECK(Second.Execute("def") == 0);
}

TEST_CASE("regexp: Status.cpp's link check")
{
	TRegexp Program(LinkPattern);
	REQUIRE(Program);
	CHECK(Program.Execute("visit www.tibia.com now") == 1);
	CHECK(Program.Execute("see tibia.de.") == 1);
	CHECK(Program.Execute("server 192.168.0.1:7171") == 1);
	CHECK(Program.Execute("www%2etibia%2ecom") == 1);
	CHECK(Program.Execute("http://test.org/page") == 1);
	CHECK(Program.Execute("(mail.example.net)") == 1);
	CHECK(Program.Execute("ends with shop.museum") == 1);
	CHECK(Program.Execute("hello world") == 0);
	CHECK(Program.Execute("tibia.xyz") == 0);
	CHECK(Program.Execute("1.2.3") == 0);
	CHECK(Program.Execute("1234.5.6.7") == 0);
	CHECK(Program.Execute("word.") == 0);
	// The client lowers the text first (LowerString); the pattern itself has no upper case.
	CHECK(Program.Execute("WWW.TIBIA.COM") == 0);
	CHECK(Program.Execute("") == 0);
}
