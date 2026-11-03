// The regexp (src/Extern/regexp, Henry Spencer's of 1986, compiled as C17): compiling, matching, the
// subexpressions, the errors, and Status.cpp's link check with Status.h's LinkPattern.

#include <doctest/doctest.h>

#include "Extern/regexp/regexp.h"

#include <cstdlib>
#include <string>

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

// regcomp and regexec take char *, as the K&R originals did.
regexp *Compile(const char *Pattern)
{
	std::string Copy(Pattern);
	return regcomp(Copy.data());
}

int Match(regexp *Program, const char *Text)
{
	std::string Copy(Text);
	return regexec(Program, Copy.data());
}

} // namespace

TEST_CASE("regexp: matching and subexpressions")
{
	regexp *Program = Compile("([a-z]+)@([a-z]+)\\.(com|de)$");
	REQUIRE(Program != nullptr);
	char Text[] = "mail: knight@tibia.com";
	CHECK(regexec(Program, Text) == 1);
	CHECK(std::string(Program->startp[0], Program->endp[0]) == "knight@tibia.com");
	CHECK(std::string(Program->startp[1], Program->endp[1]) == "knight");
	CHECK(std::string(Program->startp[2], Program->endp[2]) == "tibia");
	CHECK(std::string(Program->startp[3], Program->endp[3]) == "com");
	CHECK(Match(Program, "knight@tibia.org") == 0);
	CHECK(Match(Program, "knight@tibia.com ") == 0);
	free(Program);

	Program = Compile("^a*b+c?[^x]$");
	REQUIRE(Program != nullptr);
	CHECK(Match(Program, "bbz") == 1);
	CHECK(Match(Program, "aaabcd") == 1);
	CHECK(Match(Program, "aaab") == 0);
	CHECK(Match(Program, "abx") == 0);
	free(Program);

	// Latin-1 characters in a class (UCHARAT reads them unsigned).
	Program = Compile("^[\xe4\xf6\xfc]+$");
	REQUIRE(Program != nullptr);
	CHECK(Match(Program, "\xe4\xfc\xf6") == 1);
	CHECK(Match(Program, "\xe4u") == 0);
	free(Program);
}

TEST_CASE("regexp: errors go to regerrorhandler")
{
	CHECK(regerrorhandler == nullptr);
	CHECK(Compile("a(b") == nullptr);
	regerrorhandler = RecordError;
	CHECK(Compile("a(b") == nullptr);
	CHECK(LastError == "unmatched ()");
	CHECK(Compile("a**") == nullptr);
	CHECK(LastError == "nested *?+");
	CHECK(regcomp(nullptr) == nullptr);
	CHECK(LastError == "NULL argument");
	CHECK(regexec(nullptr, nullptr) == 0);
	CHECK(LastError == "NULL parameter");
	regerrorhandler = nullptr;
}

TEST_CASE("regexp: Status.cpp's link check")
{
	regexp *Program = Compile(LinkPattern);
	REQUIRE(Program != nullptr);
	CHECK(Match(Program, "visit www.tibia.com now") == 1);
	CHECK(Match(Program, "see tibia.de.") == 1);
	CHECK(Match(Program, "server 192.168.0.1:7171") == 1);
	CHECK(Match(Program, "www%2etibia%2ecom") == 1);
	CHECK(Match(Program, "hello world") == 0);
	CHECK(Match(Program, "tibia.xyz") == 0);
	CHECK(Match(Program, "1.2.3") == 0);
	free(Program);
}
