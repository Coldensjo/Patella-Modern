// The test mode's clocks (src/testmode/Clock.h): the virtual clock starts where tools/capture/patcap.c's
// does and moves only when it is advanced.

#include <doctest/doctest.h>

#include "testmode/Clock.h"

#include <chrono>
#include <cstdint>

using namespace patella::testmode;

namespace {

int Yields = 0;

void CountYield()
{
	Yields++;
}

} // namespace

TEST_CASE("TVirtualClock: the start is patcap.c's (timeGetTime 100000, 2026-11-02 12:00:00 UTC)")
{
	TVirtualClock Clock;
	CHECK(Clock.Elapsed() == 0);
	CHECK(Clock.Milliseconds() == 100000);
	// FILETIME_BASE of patcap.c, 0x01dd6ac1afb3a000, in seconds since 1970.
	CHECK(VirtualStartWallSeconds == 0x01dd6ac1afb3a000LL / 10000000 - 11644473600LL);
	using namespace std::chrono;
	constexpr auto Start = sys_days{year{2026} / November / 2} + hours{12};
	CHECK(Clock.WallSeconds() == duration_cast<seconds>(Start.time_since_epoch()).count());
	CHECK(TickMilliseconds == 100);
}

TEST_CASE("TVirtualClock: advances forward only; the wall clock counts whole seconds")
{
	TVirtualClock Clock;
	CHECK(Clock.AdvanceTo(999));
	CHECK(Clock.Milliseconds() == 100999);
	CHECK(Clock.WallSeconds() == VirtualStartWallSeconds);
	CHECK(Clock.AdvanceTo(1000));
	CHECK(Clock.WallSeconds() == VirtualStartWallSeconds + 1);
	CHECK(Clock.AdvanceTo(1000)); // the same time: allowed
	CHECK_FALSE(Clock.AdvanceTo(999));
	CHECK(Clock.Elapsed() == 1000);
	CHECK(Clock.AdvanceTo(61500));
	CHECK(Clock.Milliseconds() == 161500);
	CHECK(Clock.WallSeconds() == VirtualStartWallSeconds + 61);
}

TEST_CASE("TVirtualClock: the frame timer's wait does not move the time")
{
	Yields = 0;
	TVirtualClock Clock(CountYield);
	IClock &Interface = Clock;
	Interface.FrameWait(20);
	Interface.FrameWait(1000);
	CHECK(Interface.Milliseconds() == VirtualStartMilliseconds);
	CHECK(Clock.FrameWaits() == 2);
	CHECK(Yields == 2);

	TVirtualClock Silent;
	Silent.FrameWait(5);
	CHECK(Silent.FrameWaits() == 1);
	CHECK(Silent.Elapsed() == 0);
}
