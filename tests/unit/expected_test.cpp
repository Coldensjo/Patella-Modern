// patella::TExpected and TUnexpected (src/compat/expected.h, issue #80) and TError (src/TMyException.h): the same tests
// in tests_unit (std::expected) and tests_unit_cxx26_standin (PATELLA_EXPECTED_STANDIN: the C++17 stand-in that the
// 32-bit oracle returned the recoverable failures with). A TExpected holds a value or an error; a TError holds what a
// TMyException would carry, and TMyException(TError) throws it after all with the same message and location.

#include <doctest/doctest.h>

#include "TMyException.h"
#include "compat/assertion.h"
#include "compat/expected.h"

#include <cstdint>
#include <string>
#include <utility>

namespace {

struct TPoint {
	int x;
	int y;
};

patella::TExpected<int, TError> Parse(int Value)
{
	if (Value < 0)
		return patella::TUnexpected<TError>(TError{"negative", PATELLA_SOURCE_LOCATION()});
	return Value * 2;
}

int CheckLine = 0; // Check's failure

patella::TExpected<void, TError> Check(bool Good)
{
	CheckLine = __LINE__ + 2;
	if (!Good)
		return patella::TUnexpected<TError>(patella::MakeFailure(PATELLA_SOURCE_LOCATION(), "failed to open file", "",
			0, "", 0, "Tibia.cfg"));
	return {};
}

} // namespace

TEST_CASE("expected.h selects the expected path")
{
#if defined(PATELLA_EXPECTED_STANDIN)
	CHECK(PATELLA_HAS_EXPECTED == 0);
#else
	// Every product preset has std::expected (MSVC /std:c++latest, GCC 16, Clang 22).
	CHECK(PATELLA_HAS_EXPECTED == 1);
#endif
}

TEST_CASE("TExpected<T, E>: a value or an error")
{
	const patella::TExpected<int, TError> Good = Parse(21);
	CHECK(Good.has_value());
	CHECK(static_cast<bool>(Good));
	CHECK(*Good == 42);
	CHECK(Good.value_or(-1) == 42);

	const patella::TExpected<int, TError> Bad = Parse(-1);
	CHECK(!Bad.has_value());
	CHECK(!static_cast<bool>(Bad));
	CHECK(Bad.error().Message == "negative");
	CHECK(Bad.value_or(-1) == -1);

	patella::TExpected<TPoint, std::string> Point = TPoint{3, 4};
	CHECK(Point->x == 3);
	Point->y = 7;
	CHECK((*Point).y == 7);
	const patella::TExpected<TPoint, std::string> Copy = Point;
	CHECK(Copy->y == 7);

	patella::TExpected<std::string, int> Empty;
	CHECK(Empty.has_value());
	CHECK(Empty->empty());
	patella::TExpected<std::string, int> Code = patella::TUnexpected<int>(5);
	CHECK(Code.error() == 5);
	Code = std::string("text");
	CHECK(*Code == "text");
}

TEST_CASE("TExpected<void, E>: success or an error")
{
	const patella::TExpected<void, TError> Good = Check(true);
	CHECK(Good.has_value());
	CHECK(static_cast<bool>(Good));
	patella::TExpected<void, TError> Bad = Check(false);
	const int Line = CheckLine;
	REQUIRE(!Bad.has_value());
	CHECK(Bad.error().Message == "expected_test.cpp " + std::to_string(Line) + ": failed to open file, reason:\nTibia.cfg");
	CHECK(Bad.error().Location.line() == static_cast<std::uint_least32_t>(Line));
	const patella::TExpected<void, TError> Copy = Bad;
	CHECK(Copy.error().Message == Bad.error().Message);
}

TEST_CASE("TUnexpected: the error, by copy and by move")
{
	const patella::TUnexpected<std::string> A(std::string("a"));
	CHECK(A.error() == "a");
	patella::TUnexpected<std::string> B(std::string("b"));
	const std::string Moved = std::move(B).error();
	CHECK(Moved == "b");
}

TEST_CASE("TMyException(TError): a returned failure thrown after all, the same message and location")
{
	patella::TExpected<void, TError> Bad = Check(false);
	REQUIRE(!Bad.has_value());
	const std::string Message = Bad.error().Message;
	const std::uint_least32_t Line = Bad.error().Location.line();
	try {
		if (!Bad)
			throw TMyException(std::move(Bad.error()));
		FAIL("nothing thrown");
	} catch (const TMyException &Exception) {
		CHECK(Exception.GetMessage() == Message);
		CHECK(Exception.GetLocation().line() == Line);
	}
}
