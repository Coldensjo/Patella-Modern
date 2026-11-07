// The SDL3 window's event loop (src/platform/LoopTimer.h, WindowChoice.h; issue #122): the main timer in the loop
// (WM_TIMER's rules: due once its interval has passed, no backlog after a late pass, across the clock's wrap) and the
// choice of the window path by PATELLA_WINDOW's value.

#include <doctest/doctest.h>

#include "platform/LoopTimer.h"
#include "platform/WindowChoice.h"

#include <cstdint>

namespace {

using patella::platform::IsUnknownWindowSystem;
using patella::platform::ParseWindowSystem;
using patella::platform::TLoopTimer;
using patella::platform::TWindowSystem;

} // namespace

TEST_CASE("Loop timer: nothing is due before Start or after Stop")
{
	TLoopTimer Timer;
	CHECK_FALSE(Timer.IsRunning());
	CHECK_FALSE(Timer.Due(0));
	CHECK_FALSE(Timer.Due(100000));
	Timer.Start(100, 1000);
	CHECK(Timer.IsRunning());
	CHECK(Timer.Due(1100));
	Timer.Stop();
	CHECK_FALSE(Timer.IsRunning());
	CHECK_FALSE(Timer.Due(5000));
}

TEST_CASE("Loop timer: a tick once the interval has passed, the next one interval after it was seen")
{
	TLoopTimer Timer;
	Timer.Start(100, 1000);
	CHECK_FALSE(Timer.Due(1000));
	CHECK_FALSE(Timer.Due(1099));
	CHECK(Timer.Due(1100));
	CHECK_FALSE(Timer.Due(1100)); // one tick per due time
	CHECK_FALSE(Timer.Due(1199));
	CHECK(Timer.Due(1230)); // a late pass: the tick, and the next one 100 after it
	CHECK_FALSE(Timer.Due(1329));
	CHECK(Timer.Due(1330));
}

TEST_CASE("Loop timer: a loop held up for several intervals ticks once (no backlog, as WM_TIMER)")
{
	TLoopTimer Timer;
	Timer.Start(100, 0);
	CHECK(Timer.Due(1000));
	CHECK_FALSE(Timer.Due(1000));
	CHECK_FALSE(Timer.Due(1050));
	CHECK(Timer.Due(1100));
}

TEST_CASE("Loop timer: the clock's wrap at 2^32 milliseconds")
{
	TLoopTimer Timer;
	const std::uint32_t Start = 0xffffffc0U; // 64 ms before the wrap
	Timer.Start(100, Start);
	CHECK_FALSE(Timer.Due(0xffffffffU));
	CHECK_FALSE(Timer.Due(35)); // 99 ms after the start
	CHECK(Timer.Due(36));       // 100 ms
	CHECK_FALSE(Timer.Due(135));
	CHECK(Timer.Due(136));
}

TEST_CASE("Loop timer: an interval of 0 ticks at every new millisecond (SetTimer's minimum)")
{
	TLoopTimer Timer;
	Timer.Start(0, 10);
	CHECK_FALSE(Timer.Due(10));
	CHECK(Timer.Due(11));
	CHECK_FALSE(Timer.Due(11));
	CHECK(Timer.Due(12));
}

TEST_CASE("Loop timer: Start again restarts the interval")
{
	TLoopTimer Timer;
	Timer.Start(100, 0);
	Timer.Start(50, 80);
	CHECK_FALSE(Timer.Due(100));
	CHECK_FALSE(Timer.Due(129));
	CHECK(Timer.Due(130));
}

TEST_CASE("Window path: PATELLA_WINDOW=mfc is MFC's, everything else SDL3's")
{
	CHECK(ParseWindowSystem(nullptr) == TWindowSystem::Sdl);
	CHECK(ParseWindowSystem("") == TWindowSystem::Sdl);
	CHECK(ParseWindowSystem("sdl") == TWindowSystem::Sdl);
	CHECK(ParseWindowSystem("SDL") == TWindowSystem::Sdl);
	CHECK(ParseWindowSystem("mfc") == TWindowSystem::Mfc);
	CHECK(ParseWindowSystem("MFC") == TWindowSystem::Mfc);
	CHECK(ParseWindowSystem("Mfc") == TWindowSystem::Mfc);
	CHECK(ParseWindowSystem("mfc ") == TWindowSystem::Sdl);
	CHECK(ParseWindowSystem("win32") == TWindowSystem::Sdl);
}

TEST_CASE("Window path: an unknown PATELLA_WINDOW is reported")
{
	CHECK_FALSE(IsUnknownWindowSystem(nullptr));
	CHECK_FALSE(IsUnknownWindowSystem(""));
	CHECK_FALSE(IsUnknownWindowSystem("sdl"));
	CHECK_FALSE(IsUnknownWindowSystem("Mfc"));
	CHECK(IsUnknownWindowSystem("win32"));
	CHECK(IsUnknownWindowSystem("mfc "));
}
