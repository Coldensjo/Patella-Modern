// PATELLA_ASSERT and PATELLA_ASSERT_VALUES of src/compat/assertion.h (issue #79): the same tests in tests_unit
// (std::source_location) and tests_unit_cxx26_standin (PATELLA_SOURCE_LOCATION_STANDIN: the stand-in that the
// oracle-x86 build's C++17 takes). A false expression throws the TMyException that common.h's TASSERT, TASSERT1 and
// TASSERT2 threw, with this file and the assertion's line; a true one throws nothing; the expression is evaluated
// exactly once, the values only when it is false.

#include <doctest/doctest.h>

#include "TMyException.h"
#include "compat/assertion.h"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

int Calls = 0;

bool Counted(bool Result)
{
	++Calls;
	return Result;
}

int CountedValue(int Value)
{
	++Calls;
	return Value;
}

// Throws when Calls is not negative (always here; a condition, so that the compiler sees no unreachable code after it).
bool Throws()
{
	if (Calls >= 0)
		throw std::runtime_error("the expression threw");
	return true;
}

int Sum(int A, int B)
{
	return A + B;
}

enum TColor { RED = 3,
	GREEN = 7 };

struct TCaught {
	std::string Message;
	std::string Origin;
};

// The message and origin of what Assertion throws, which must be a TMyException.
template <class Function>
TCaught Caught(Function Assertion)
{
	try {
		Assertion();
	} catch (const TMyException &Exception) {
		return {Exception.GetMessage(), Exception.GetOrigin()};
	} catch (...) {
		FAIL("not a TMyException");
	}
	FAIL("nothing thrown");
	return {};
}

std::string At(int Line)
{
	return "assertion_test.cpp " + std::to_string(Line);
}

int Branch(int Value)
{
	if (Value > 0)
		PATELLA_ASSERT(Value > 0);
	else
		PATELLA_ASSERT_VALUES(Value <= 0, Value);
	return Value;
}

} // namespace

TEST_CASE("assertion.h selects the expected source location")
{
#if defined(PATELLA_SOURCE_LOCATION_STANDIN)
	CHECK(PATELLA_HAS_SOURCE_LOCATION == 0);
#else
	// Every product preset has std::source_location (MSVC /std:c++latest, GCC 16, Clang 22).
	CHECK(PATELLA_HAS_SOURCE_LOCATION == 1);
#endif
	const int Line = __LINE__ + 1;
	const patella::TSourceLocation Location = PATELLA_SOURCE_LOCATION();
	CHECK(Location.line() == static_cast<std::uint_least32_t>(Line));
	CHECK(std::string(Location.file_name()).ends_with("assertion_test.cpp"));
	CHECK(std::string(Location.function_name()).size() > 0);
}

TEST_CASE("PATELLA_ASSERT: a true expression is evaluated once and throws nothing")
{
	Calls = 0;
	CHECK_NOTHROW(PATELLA_ASSERT(Counted(true)));
	CHECK(Calls == 1);
	CHECK_NOTHROW(PATELLA_ASSERT(Counted(true) && Counted(true)));
	CHECK(Calls == 3);
	const int *const Pointer = &Calls;
	CHECK_NOTHROW(PATELLA_ASSERT(Pointer!=nullptr));
}

TEST_CASE("PATELLA_ASSERT: a false expression throws TMyException with this file, the line and the expression")
{
	Calls = 0;
	int Line = 0;
	TCaught Exception = Caught([&] {
		Line = __LINE__ + 1;
		PATELLA_ASSERT(Counted(false));
	});
	CHECK(Calls == 1);
	CHECK(Exception.Origin == At(Line));
	CHECK(Exception.Message == At(Line) + ": assertion failed, reason:\nCounted(false)");

	const int *Pointer = nullptr;
	Exception = Caught([&] {
		Line = __LINE__ + 1;
		PATELLA_ASSERT(Pointer!=nullptr);
	});
	CHECK(Exception.Message == At(Line) + ": assertion failed, reason:\nPointer!=nullptr");
}

TEST_CASE("PATELLA_ASSERT_VALUES: a true expression throws nothing and does not evaluate the values")
{
	Calls = 0;
	CHECK_NOTHROW(PATELLA_ASSERT_VALUES(Counted(true), CountedValue(1)));
	CHECK(Calls == 1);
	CHECK_NOTHROW(PATELLA_ASSERT_VALUES(Counted(true), CountedValue(1), CountedValue(2)));
	CHECK(Calls == 2);
}

TEST_CASE("PATELLA_ASSERT_VALUES: a false expression throws TMyException with the values")
{
	Calls = 0;
	int Line = 0;
	const int Width = -5;
	TCaught Exception = Caught([&] {
		Line = __LINE__ + 1;
		PATELLA_ASSERT_VALUES(Counted(Width>=0), CountedValue(Width));
	});
	CHECK(Calls == 2);
	CHECK(Exception.Origin == At(Line));
	CHECK(Exception.Message
		== At(Line) + ": assertion failed (CountedValue(Width) = -5), reason:\nCounted(Width>=0)");

	// Two values, commas inside the brackets of the values and of the expression, the conversion to int.
	const int Position = 12;
	const int NumberOfBuddies = 4;
	Calls = 0;
	Exception = Caught([&] {
		Line = __LINE__ + 1;
		PATELLA_ASSERT_VALUES(Counted(Sum(Position, 0)<NumberOfBuddies), Sum(Position, 1), NumberOfBuddies-1);
	});
	CHECK(Calls == 1);
	CHECK(Exception.Message
		== At(Line)
			+ ": assertion failed (Sum(Position, 1) = 13) (NumberOfBuddies-1 = 3), reason:\n"
			  "Counted(Sum(Position, 0)<NumberOfBuddies)");

	const unsigned Big = 0xfffffffeU;
	const TColor Color = GREEN;
	Exception = Caught([&] {
		Line = __LINE__ + 1;
		PATELLA_ASSERT_VALUES(Color==RED, Color, Big);
	});
	CHECK(Exception.Message == At(Line) + ": assertion failed (Color = 7) (Big = -2), reason:\nColor==RED");
	const bool Flag = true;
	Exception = Caught([&] {
		Line = __LINE__ + 1;
		PATELLA_ASSERT_VALUES(!Flag, Flag);
	});
	CHECK(Exception.Message == At(Line) + ": assertion failed (Flag = 1), reason:\n!Flag");
}

TEST_CASE("PATELLA_ASSERT: an exception of the expression passes unchanged")
{
	CHECK_THROWS_AS(PATELLA_ASSERT(Throws()), std::runtime_error);
	CHECK_THROWS_AS(PATELLA_ASSERT_VALUES(Throws(), Calls), std::runtime_error);
}

TEST_CASE("PATELLA_ASSERT is one statement")
{
	CHECK(Branch(2) == 2);
	CHECK(Branch(-2) == -2);
	Calls = 0;
	const int Line = __LINE__ + 6;
	const TCaught Exception = Caught([] {
		const int Value = Calls;
		if (Value == 1)
			PATELLA_ASSERT(Value == 1);
		else
			PATELLA_ASSERT_VALUES(Value == 1, Value);
	});
	CHECK(Exception.Message == At(Line) + ": assertion failed (Value = 0), reason:\nValue == 1");
}
