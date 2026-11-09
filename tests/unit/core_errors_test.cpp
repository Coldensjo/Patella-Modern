// The error sites of common.h (issue #80): TERROR*, TMYEXCEPTION and TProgramError, through patella_core's common.h.
// A TERROR throws a TMyException whose message names this file and the site's line, the values with their source text
// and a reason; TMYEXCEPTION wraps a caught one; a PROGRAM_ERROR is thrown inside a TProgramError and caught by
// reference, never as the bare enum.

#include <doctest/doctest.h>

#include "common.h"

#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

std::string At(int Line)
{
	return "core_errors_test.cpp " + std::to_string(Line);
}

template <class F> TMyException CaughtError(F Function)
{
	try {
		Function();
	} catch (const TMyException &Exception) {
		return Exception;
	}
	FAIL("no TMyException");
	return TMyException("", PATELLA_SOURCE_LOCATION());
}

int FailsLine = 0; // Fails' TERROR1

int Fails(int Value)
{
	FailsLine = __LINE__ + 2;
	if (Value >= 0)
		TERROR1("unknown packet type during game", Value);
	return Value;
}

PROGRAM_ERROR ThrownCode(int Value)
{
	try {
		if (Value >= 0)
			throw TProgramError{static_cast<PROGRAM_ERROR>(Value)};
	} catch (const TProgramError &Error) {
		return Error.Code;
	}
	return PROGRAM_ERROR{};
}

} // namespace

TEST_CASE("TERROR and its variants: the message with this file, the site's line, the values and the reason")
{
	int Line = 0;
	TMyException Exception = CaughtError([&] {
		Line = __LINE__ + 1;
		TERROR("no file open");
	});
	CHECK(Exception.GetMessage() == At(Line) + ": no file open");
	CHECK(Exception.GetLocation().line() == static_cast<std::uint_least32_t>(Line));

	Exception = CaughtError([] { Fails(255); });
	CHECK(Exception.GetMessage() == At(FailsLine) + ": unknown packet type during game (Value = 255)");

	const int x = 3;
	const unsigned y = 0xffffffffU;
	Exception = CaughtError([&] {
		Line = __LINE__ + 1;
		TERROR2("out of range", x, y);
	});
	CHECK(Exception.GetMessage() == At(Line) + ": out of range (x = 3) (y = -1)");

	const std::string FileName = "Tibia.cfg";
	Exception = CaughtError([&] {
		Line = __LINE__ + 1;
		TERROR_REASON("failed to open file", FileName);
	});
	CHECK(Exception.GetMessage() == At(Line) + ": failed to open file, reason:\nTibia.cfg");
	Exception = CaughtError([&] {
		Line = __LINE__ + 1;
		TERROR1_REASON("bad", x, FileName.c_str());
	});
	CHECK(Exception.GetMessage() == At(Line) + ": bad (x = 3), reason:\nTibia.cfg");
}

TEST_CASE("TMYEXCEPTION wraps a caught TMyException: its message becomes the reason")
{
	int Line = 0;
	const TMyException Exception = CaughtError([&] {
		try {
			Fails(7);
		} catch (TMyException &Caught) {
			Line = __LINE__ + 1;
			throw TMYEXCEPTION("exception occurred", "", 0, "", 0, Caught);
		}
	});
	CHECK(Exception.GetMessage()
		== At(Line) + ": exception occurred, reason:\n" + At(FailsLine) + ": unknown packet type during game (Value = 7)");
	CHECK(patella::ErrorOrigin(Exception) == At(Line));
}

namespace {

// What Player.cpp's handlers (patella::ThrowTopLevelError) report for an exception, without the "<file> <line>: ".
template <class T> std::string TopLevelText(T Thrown)
{
	const TMyException Exception = CaughtError([&] {
		try {
			throw Thrown;
		} catch (...) {
			patella::ThrowTopLevelError(PATELLA_SOURCE_LOCATION());
		}
	});
	const std::string Message = Exception.GetMessage();
	return Message.substr(Message.find(": ") + 2);
}

} // namespace

TEST_CASE("ThrowTopLevelError: the original's texts; out of memory is \"unknown exception occurred\"")
{
	CHECK(TopLevelText(std::bad_alloc()) == "unknown exception occurred"); // MFC's operator new: not a std::bad_alloc there
	CHECK(TopLevelText(std::bad_array_new_length()) == "unknown exception occurred");
	CHECK(TopLevelText(std::out_of_range("x")) == "out_of_range_exception occurred");
	CHECK(TopLevelText(std::runtime_error("x")) == "exception occurred");
	CHECK(TopLevelText("x") == "const char* exception occurred");
	CHECK(TopLevelText(7) == "unknown exception occurred");
	CHECK(TopLevelText(TMyException("inner", PATELLA_SOURCE_LOCATION())) == "exception occurred, reason:\ninner");
}

TEST_CASE("PROGRAM_ERROR: an enum class of int, thrown inside TProgramError")
{
	static_assert(std::is_enum_v<PROGRAM_ERROR> && !std::is_convertible_v<PROGRAM_ERROR, int>,
		"PROGRAM_ERROR is an enum class");
	static_assert(sizeof(TProgramError) == 4);
	CHECK(static_cast<int>(PROGRAM_ERROR::OBJECTS_FILE) == 1);
	CHECK(static_cast<int>(PROGRAM_ERROR::SOCKET_INIT) == 6);
	CHECK(static_cast<int>(PROGRAM_ERROR::SURFACE) == 12);
	CHECK(static_cast<int>(PROGRAM_ERROR::SPRITE_SURFACE) == 18);
	CHECK(ThrownCode(12) == PROGRAM_ERROR::SURFACE);
	// Not a bare enum, nor a TMyException: a catch of either lets it pass.
	bool Passed = false;
	try {
		try {
			if (!Passed)
				throw TProgramError{PROGRAM_ERROR::SOCKET_INIT};
		} catch (PROGRAM_ERROR) {
			FAIL("caught as the bare enum");
		} catch (const TMyException &) {
			FAIL("caught as TMyException");
		}
	} catch (const TProgramError &Error) {
		Passed = Error.Code == PROGRAM_ERROR::SOCKET_INIT;
	}
	CHECK(Passed);
}
