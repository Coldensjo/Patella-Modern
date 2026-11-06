// assertion.h: PATELLA_ASSERT, the client's assertions (Patella-Modern issue #79; until then common.h's three assertion
// macros and nine local variants). Not part of the original source.
//
// - PATELLA_ASSERT(Expression): when the expression is false, throws the TMyException the old macros threw:
//   "<file> <line>: assertion failed, reason:\n<Expression>", with the expression's source text.
// - PATELLA_ASSERT_VALUES(Expression, Value1[, Value2]): the same with one or two values in the message,
//   "<file> <line>: assertion failed (<Value1's text> = <value>)[ (<Value2's text> = <value>)], reason:\n...", each
//   value converted to int (static_cast) as the old macros' (int) cast did.
// - PATELLA_DIVISOR(Value) (issue #67): an integer divisor from input; zero throws a TMyException instead of trapping
//   (at the end of this file).
//
// The expression is evaluated exactly once, the values only when it is false (after it, in an unspecified order, as
// the old macros' arguments were). The file and line are the assertion's own (std::source_location::current(), or
// __FILE__ and __LINE__ below), no longer the original's line. Each is an expression of type void, so it is one
// statement anywhere a statement fits (an if without braces, an else: no dangling else), and nothing is compiled out in any build: assertions on packets and files stay
// checked (notes/modernization-plan.md, M5).
//
// Not contract_assert, on any compiler (notes/decisions.md, issue #79): the outcome of a failed assertion must stay a
// TMyException (the golden corpus compares the kind), and a contract assertion gives it only through a
// throwing replaceable violation handler, one for the whole program (CONTRACT_ASSERT's, compat/cxx26.h, would throw
// too), whose semantic a compiler flag changes (ignore compiles the check out), which may evaluate the predicate more
// than once, and in which GCC 16.2 cannot tell a predicate that threw from a false one.
//
// The location: std::source_location where the library has it (PATELLA_HAS_SOURCE_LOCATION 1: every product preset),
// otherwise (the oracle's /std:c++17), or with PATELLA_SOURCE_LOCATION_STANDIN, which tests/unit's stand-in build
// defines for a whole target, a stand-in with file_name, line and function_name from __FILE__, __LINE__ and __func__.
// The failure functions are TMyException.cpp's.

#pragma once

#include <version> // IWYU pragma: keep (the feature-test macro of the #if line)

#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L \
	&& !defined(PATELLA_SOURCE_LOCATION_STANDIN)
#define PATELLA_HAS_SOURCE_LOCATION 1
#include <source_location> // IWYU pragma: export
#else
#define PATELLA_HAS_SOURCE_LOCATION 0
#include <cstdint>
#endif

namespace patella {

#if PATELLA_HAS_SOURCE_LOCATION

using TSourceLocation = std::source_location;

#define PATELLA_SOURCE_LOCATION() ::std::source_location::current()

#else

// The part of std::source_location the assertions use. An aggregate, so an assertion's failure path is three stores and
// the call, no constructor.
struct TSourceLocation {
	const char *File;
	std::uint_least32_t Line;
	const char *Function;

	[[nodiscard]] constexpr const char *file_name() const noexcept { return File; }
	[[nodiscard]] constexpr std::uint_least32_t line() const noexcept { return Line; }
	[[nodiscard]] constexpr const char *function_name() const noexcept { return Function; }
};

#define PATELLA_SOURCE_LOCATION() (::patella::TSourceLocation{__FILE__, __LINE__, __func__})

#endif

// Throw the assertion's TMyException. Names is the values' source text as the preprocessor stringized it
// ("Position, NumberOfBuddies-1"): it is split at its top-level commas.
[[noreturn]] void AssertionFailed(const char *Expression, const TSourceLocation &Location);
[[noreturn]] void AssertionFailed(const char *Expression, const TSourceLocation &Location, const char *Names, int Value1);
[[noreturn]] void AssertionFailed(const char *Expression, const TSourceLocation &Location, const char *Names, int Value1,
	int Value2);

// PATELLA_ASSERT_VALUES's call: the values converted to int.
template <class... T>
[[noreturn]] void AssertionFailedValues(const char *Expression, const TSourceLocation &Location, const char *Names,
	const T &...Values)
{
	static_assert(sizeof...(T) == 1 || sizeof...(T) == 2, "PATELLA_ASSERT_VALUES carries one or two values");
	AssertionFailed(Expression, Location, Names, static_cast<int>(Values)...);
}

// PATELLA_DIVISOR's failure (issue #67): the TMyException "<file> <line>: integer division by zero, reason:\n
// <Expression>".
[[noreturn]] void DivisionByZero(const char *Expression, const TSourceLocation &Location);

// PATELLA_DIVISOR's call: the divisor itself, or the failure when it is zero.
template <class T> T CheckedDivisor(T Value, const char *Expression, const TSourceLocation &Location)
{
	if (Value == 0)
		DivisionByZero(Expression, Location);
	return Value;
}

} // namespace patella

#define PATELLA_ASSERT(Expression) \
	(!(Expression) ? ::patella::AssertionFailed(#Expression, PATELLA_SOURCE_LOCATION()) : static_cast<void>(0))

#define PATELLA_ASSERT_VALUES(Expression, ...) \
	(!(Expression) ? ::patella::AssertionFailedValues(#Expression, PATELLA_SOURCE_LOCATION(), #__VA_ARGS__, __VA_ARGS__) \
				   : static_cast<void>(0))

// PATELLA_DIVISOR(Value) (Patella-Modern issue #67): an integer divisor that input can make zero (a packet's field, a
// value of Tibia.dat), as the division's (or the remainder's) right operand. It is the value, evaluated once; when it is
// zero, a TMyException is thrown instead of the division: the original (MSVC 7.1, whose catch-all handlers caught structured
// exceptions) trapped there and its catch wrapper turned the trap into a TMyException; the x64 client builds with
// /EHsc, where nothing catches a trap (notes/decisions.md, "#67, part 2").
#define PATELLA_DIVISOR(Value) (::patella::CheckedDivisor((Value), #Value, PATELLA_SOURCE_LOCATION()))
