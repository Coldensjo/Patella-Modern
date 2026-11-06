// TMyException (src/TMyException.h, issue #80): a message and the source location of the site that threw it, and the
// texts of the original's two constructors (patella::ErrorText, ErrorOrigin) that the sites' messages are made of.
// The same tests in tests_unit (std::source_location) and tests_unit_cxx26_standin (the oracle's C++17 stand-in,
// PATELLA_SOURCE_LOCATION_STANDIN). difftest's Errors test compares ErrorText with Tibia.exe's constructors for
// random arguments; these are the fixed cases, on every compiler.

#include <doctest/doctest.h>

#include "TMyException.h"
#include "compat/assertion.h"

#include <cstdint>
#include <string>
#include <type_traits>

namespace {

std::string At(int Line)
{
	return "exception_test.cpp " + std::to_string(Line);
}

} // namespace

TEST_CASE("TMyException: a message and a source location, copied with the exception")
{
	static_assert(std::is_polymorphic_v<TMyException>, "TMyException keeps its virtual destructor (RTTI)");
	static_assert(std::is_nothrow_move_constructible_v<patella::TSourceLocation>);
	const int Line = __LINE__ + 1;
	const TMyException Exception("the message", PATELLA_SOURCE_LOCATION());
	CHECK(Exception.GetMessage() == "the message");
	CHECK(Exception.GetLocation().line() == static_cast<std::uint_least32_t>(Line));
	CHECK(std::string(Exception.GetLocation().file_name()).ends_with("exception_test.cpp"));
	CHECK(patella::ErrorOrigin(Exception) == At(Line));
	const TMyException Copy = Exception;
	CHECK(Copy.GetMessage() == "the message");
	CHECK(Copy.GetLocation().line() == Exception.GetLocation().line());
}

TEST_CASE("ErrorText: the text of the original's constructor 0x508400")
{
	CHECK(patella::ErrorText("Map.cpp", 426, "kind", "", 0, "", 0, "") == "Map.cpp 426: kind");
	CHECK(patella::ErrorText("C:\\Tibia\\Source\\Map.cpp", -3, "kind", "x", 5, "", 0, "") == "Map.cpp -3: kind (x = 5)");
	CHECK(patella::ErrorText("c:/cipsoft/client/GUI.cpp", 1, "kind", "", 7, "y", -2, "") == "GUI.cpp 1: kind (y = -2)");
	CHECK(patella::ErrorText("a/b\\c.cpp", 2, "kind", "x", 1, "y", 2, "why") == "c.cpp 2: kind (x = 1) (y = 2), reason:\nwhy");
	// A path that ends in a separator, and no file at all, stay as they are.
	CHECK(patella::ErrorText("dir\\", 0, "", "", 0, "", 0, "") == "dir\\ 0: ");
	CHECK(patella::ErrorText("", 0, "k", "", 0, "", 0, "r") == " 0: k, reason:\nr");
	CHECK(patella::ErrorOrigin("C:\\a/b\\x.cpp", 12) == "x.cpp 12");
	CHECK(patella::ErrorOrigin("dir/", 12) == "dir/ 12");
}

TEST_CASE("ErrorText: the wrapping constructor 0x5086a0 appends the inner message, also an empty one")
{
	const TMyException Inner("Control.cpp 1274: unknown packet type during game (Type = 255)", PATELLA_SOURCE_LOCATION());
	CHECK(patella::ErrorText("Player.cpp", 383, "exception occurred", "", 0, "", 0, Inner)
		== "Player.cpp 383: exception occurred, reason:\nControl.cpp 1274: unknown packet type during game (Type = 255)");
	CHECK(patella::ErrorText("Control.cpp", 145, "ExitSurfaces failed", "oldEngine", 2, "", 0, Inner).starts_with(
		"Control.cpp 145: ExitSurfaces failed (oldEngine = 2), reason:\nControl.cpp 1274"));
	const TMyException Empty("", PATELLA_SOURCE_LOCATION());
	CHECK(patella::ErrorText("x.cpp", 1, "k", "", 0, "", 0, Empty) == "x.cpp 1: k, reason:\n");
}

TEST_CASE("MakeError: the message with the site's file and line, and the site's location")
{
	const int Line = __LINE__ + 1;
	const TMyException Exception = patella::MakeError(PATELLA_SOURCE_LOCATION(), "kind", "Value", 3, "", 0, "reason");
	CHECK(Exception.GetMessage() == At(Line) + ": kind (Value = 3), reason:\nreason");
	CHECK(Exception.GetLocation().line() == static_cast<std::uint_least32_t>(Line));

	const int Outer = __LINE__ + 1;
	const TMyException Wrapped = patella::MakeError(PATELLA_SOURCE_LOCATION(), "outer", "", 0, "", 0, Exception);
	CHECK(Wrapped.GetMessage() == At(Outer) + ": outer, reason:\n" + At(Line) + ": kind (Value = 3), reason:\nreason");
	CHECK(patella::ErrorOrigin(Wrapped) == At(Outer));
}

TEST_CASE("MakeError: thrown and caught as TMyException")
{
	int Line = 0;
	try {
		// A condition, so that MSVC sees no unreachable code after it (C4702).
		Line = __LINE__ + 2;
		if (Line > 0)
			throw patella::MakeError(PATELLA_SOURCE_LOCATION(), "thrown", "", 0, "", 0, "");
	} catch (const TMyException &Exception) {
		CHECK(Exception.GetMessage() == At(Line) + ": thrown");
		return;
	}
	FAIL("nothing caught");
}
