// Time32, StoreTime32 and LoadTime32 of src/types/Time32.h (issue #62): the original's 4-byte time fields and their
// conversion from and to the client's 64-bit time. Held against what the original computed in 32 bits (its time()
// truncated the 64-bit count of seconds, Tibia.exe 0x5121ea; its arithmetic wrapped as MSVC's does), with the
// 32-bit arithmetic done here in uint32 (defined wrapping): up to 2038 the same values and comparisons, after
// 2038 the comparisons as before it. Portable: the same expected values on every platform.

#include <doctest/doctest.h>

#include "types/Time32.h"

#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <type_traits>

namespace {

constexpr std::int64_t Last32 = INT32_MAX;           // 2038-01-19 03:14:07 UTC
constexpr std::int64_t UCRTLast32 = 0x7fffd27f;      // 2038-01-18 23:59:59 UTC, the UCRT's last _time32
constexpr std::int64_t Wrap = std::int64_t{1} << 32; // 2106-02-07 06:28:16 UTC, the original's 0 again
constexpr std::int64_t Half = std::int64_t{1} << 31;
constexpr std::int64_t November2023 = 1'700'000'000; // the golden tests' fixed clock

// The original's time() in 32 bits for a time: its low 32 bits (the cast of MSVC 7.1's time()).
std::int32_t OriginalTime(std::int64_t Time)
{
	return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(Time)));
}

// The original's skull check (UpdateSkulls, ContentWindows), in 32 bits with MSVC's wrapping: the removed skull is
// still shown at Now.
bool OriginalSkullKept(std::int32_t Skull, std::int32_t Now)
{
	const auto End = static_cast<std::int32_t>(static_cast<std::uint32_t>(Skull) + 4U);
	return Skull != 0 && !(Now < Skull || Now > End);
}

// TCreature::SkullKept's check (Creatures.h), in 64 bits.
bool SkullKept(Time32 Skull, std::int64_t Now)
{
	if (Skull == 0)
		return false;
	const std::int64_t Removed = LoadTime32(Skull, Now);
	return Now >= Removed && Now <= Removed + 4;
}

} // namespace

TEST_CASE("time32: the type")
{
	static_assert(std::is_same_v<Time32, std::int32_t>);
	static_assert(sizeof(Time32) == 4 && alignof(Time32) == 4);
	// the client's own time has 64 bits on every platform (Windows x64, Linux x64)
	static_assert(sizeof(std::time_t) == 8);
	static_assert(std::is_signed_v<std::time_t>);
}

TEST_CASE("time32: StoreTime32 is the original's truncation")
{
	static_assert(StoreTime32(0) == 0);
	static_assert(StoreTime32(November2023) == November2023);
	static_assert(StoreTime32(Last32) == INT32_MAX);
	// one semantics everywhere: not the UCRT's _time32 (-1 after 2038-01-18 23:59:59 UTC) nor the old Linux
	// stand-in (-1 after INT32_MAX), but the original's low 32 bits
	static_assert(StoreTime32(UCRTLast32) == UCRTLast32);
	static_assert(StoreTime32(UCRTLast32 + 1) == UCRTLast32 + 1);
	static_assert(StoreTime32(Last32 + 1) == INT32_MIN);
	static_assert(StoreTime32(Wrap - 1) == -1);
	static_assert(StoreTime32(Wrap) == 0);
	static_assert(StoreTime32(Wrap + November2023) == November2023);
	static_assert(StoreTime32(-1) == -1);

	// the time itself up to 2038, the original's 32 bits after it, for a spread of times from 1970 to 2300
	int Differences = 0;
	int Identity = 0;
	for (std::int64_t Time = 0; Time < 10'500'000'000; Time += 999'983) {
		Differences += StoreTime32(Time) != OriginalTime(Time) ? 1 : 0;
		if (Time <= Last32)
			Identity += StoreTime32(Time) != Time ? 1 : 0;
	}
	CHECK(Differences == 0);
	CHECK(Identity == 0);

	// a talk line's time, the clock and the server's offset (TTalkLine): the original's wrapping 32-bit sum
	Differences = 0;
	for (const std::int64_t Now : {std::int64_t{0}, November2023, UCRTLast32, Last32, Last32 + 1, Wrap - 1, Wrap,
			 Wrap + Half}) {
		for (const int Offset : {0, -1, -60, -86'400, 1, INT32_MIN + 1, INT32_MAX}) {
			const std::uint32_t Original = static_cast<std::uint32_t>(OriginalTime(Now)) + static_cast<std::uint32_t>(Offset);
			Differences += StoreTime32(Now + Offset) != static_cast<std::int32_t>(Original) ? 1 : 0;
		}
	}
	CHECK(Differences == 0);
}

TEST_CASE("time32: LoadTime32 is the field itself up to 2038")
{
	static_assert(LoadTime32(0, 0) == 0);
	static_assert(LoadTime32(INT32_MAX, 0) == Last32);
	static_assert(LoadTime32(0, Last32) == 0);
	static_assert(LoadTime32(November2023, November2023 + 4) == November2023);
	int Differences = 0;
	for (std::int64_t Stored = 0; Stored <= Last32; Stored += 4'194'301) {
		for (std::int64_t Now = 0; Now <= Last32; Now += 4'194'319)
			Differences += LoadTime32(static_cast<Time32>(Stored), Now) != Stored ? 1 : 0;
		Differences += LoadTime32(static_cast<Time32>(Stored), Last32) != Stored ? 1 : 0;
	}
	CHECK(Differences == 0);
}

TEST_CASE("time32: LoadTime32 gives back a stored time within 68 years, past 2038 and 2106")
{
	int Differences = 0;
	for (std::int64_t Time = 0; Time < 10'500'000'000; Time += 9'999'991) {
		const Time32 Stored = StoreTime32(Time);
		for (const std::int64_t Distance : {std::int64_t{0}, std::int64_t{4}, std::int64_t{-4}, std::int64_t{86'400},
				 Half, -(Half - 1), std::int64_t{-1'000'000'000}})
			Differences += LoadTime32(Stored, Time + Distance) != Time ? 1 : 0;
	}
	CHECK(Differences == 0);
	// the window's ends: Now - 2^31 to Now + 2^31 - 1
	CHECK(LoadTime32(StoreTime32(Wrap), Wrap + Half) == Wrap);
	CHECK(LoadTime32(StoreTime32(Wrap), Wrap - Half + 1) == Wrap);
	CHECK(LoadTime32(StoreTime32(Wrap), Wrap - Half) == 0);            // out of the window: the time 2^32 earlier
	CHECK(LoadTime32(StoreTime32(Wrap), Wrap + Half + 1) == 2 * Wrap); // and 2^32 later
	CHECK(LoadTime32(StoreTime32(Last32 + 1), Last32) == Last32 + 1);
	CHECK(LoadTime32(StoreTime32(Wrap - 1), Wrap + 3) == Wrap - 1);
}

TEST_CASE("time32: the skull's check is the original's up to 2038")
{
	// every pair of a skull time and a clock near each other, at the start, in 2023 and in 2038
	int Differences = 0;
	int Kept = 0;
	for (const std::int64_t Base : {std::int64_t{1}, November2023, UCRTLast32, Last32 - 100}) {
		for (std::int64_t Skull = Base; Skull < Base + 20; Skull++) {
			for (std::int64_t Now = Base - 10; Now < Base + 40; Now++) {
				if (Now < 0 || Skull + 4 > Last32 || Now > Last32)
					continue;
				const bool Original = OriginalSkullKept(OriginalTime(Skull), OriginalTime(Now));
				Differences += SkullKept(StoreTime32(Skull), Now) != Original ? 1 : 0;
				Kept += Original ? 1 : 0;
			}
		}
	}
	CHECK(Differences == 0);
	CHECK(Kept == 4 * 20 * 5);
	CHECK_FALSE(SkullKept(0, November2023));
}

TEST_CASE("time32: the skull is kept four seconds past 2038 and 2106")
{
	for (const std::int64_t Removed : {Last32 - 1, Last32, Last32 + 1, Last32 + 3'600, Wrap - 2, Wrap + 1,
			 std::int64_t{9'000'000'000}}) {
		const Time32 Skull = StoreTime32(Removed);
		REQUIRE(Skull != 0);
		CAPTURE(Removed);
		CHECK_FALSE(SkullKept(Skull, Removed - 1));
		for (int Second = 0; Second <= 4; Second++)
			CHECK(SkullKept(Skull, Removed + Second));
		CHECK_FALSE(SkullKept(Skull, Removed + 5));
		CHECK_FALSE(SkullKept(Skull, Removed + 86'400));
	}
	// the original's 32 bits are 0 at the wrap (2106-02-07 06:28:16 UTC): no skull is kept from that second, as
	// in the original
	CHECK(StoreTime32(Wrap) == 0);
	// the original: at the last four seconds of 32 bits its Skull + 4 wrapped and the skull went at once; and
	// from 2038 to 2106 its fields were negative, which MapWindow's and ContentWindows' "> 0" took for no skull
	CHECK_FALSE(OriginalSkullKept(OriginalTime(Last32 - 1), OriginalTime(Last32 - 1)));
	CHECK(SkullKept(StoreTime32(Last32 - 1), Last32 - 1));
	CHECK(StoreTime32(Last32 + 3'600) < 0);
}
