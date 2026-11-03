// What patella_core assumes of the platform (notes/modernization-plan.md, M1: char signedness,
// 4-byte wchar_t, 8-byte long). The client's sources were written for 32-bit MSVC; on Windows x64 and
// Linux x64 these hold too, apart from the sizes of long and wchar_t, which no core source uses
// (vlong.h's long overloads convert to unsigned explicitly).

#include <doctest/doctest.h>

#include "common.h"

#include <bit>
#include <climits>
#include <type_traits>

// IsAlpha compares with (char)0xe4 and the like, which is negative where char is signed.
static_assert(std::is_signed_v<char>, "the core assumes a signed char (x86 and x64)");
static_assert(CHAR_BIT == 8);
static_assert(sizeof(uint8) == 1 && sizeof(uint16) == 2 && sizeof(uint32) == 4);
static_assert(sizeof(int) == 4 && sizeof(unsigned) == 4, "vlong's units and the packet code are 32-bit");
static_assert(std::endian::native == std::endian::little, "TXTEA and the packet buffers read little-endian words");

TEST_CASE("platform: the sizes that differ between the targets")
{
#ifdef _WIN32
	CHECK(sizeof(long) == 4);
	CHECK(sizeof(wchar_t) == 2);
#else
	CHECK(sizeof(long) == 8);
	CHECK(sizeof(wchar_t) == 4);
#endif
	CHECK(sizeof(void *) == 8);
	CHECK(PATELLA_ORIGINAL_LAYOUT == 0);
}
