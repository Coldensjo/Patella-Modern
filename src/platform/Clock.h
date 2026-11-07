// Clock.h: IClock, the platform's clocks (Patella-Modern issue #109, plan M7; notes/decisions.md, "#109: the platform
// interfaces"). Not part of the original source. Portable: no platform header.
//
// The client reads three clocks: timeGetTime (sysinc.cpp's GetMilliseconds and the seed of rand, the frame timer), the
// wall clock (time, 64 bits since issue #62: the talk lines' stamps, the skull times, the saved channels) and the frame
// timer's Sleep (WaitForNextFrame). Every one goes through the process's IClock (platform/Platform.h):
//
//	TWin32Clock    timeGetTime, time and Sleep (platform/Win32Platform.h): Patella.exe's, and the golden runner's
//	TVirtualClock  the test mode's clock of a scripted run (testmode/Clock.h): PatellaTest.exe's with --test-script

#pragma once

#include <cstdint>

namespace patella::platform {

class IClock {
public:
	IClock() = default;
	IClock(const IClock &) = delete;
	IClock &operator=(const IClock &) = delete;
	IClock(IClock &&) = delete;
	IClock &operator=(IClock &&) = delete;

	// timeGetTime: milliseconds, wrapping at 2^32.
	[[nodiscard]] virtual std::uint32_t Milliseconds() = 0;
	// The wall clock: seconds since 1970-01-01 00:00:00 UTC.
	[[nodiscard]] virtual std::int64_t WallSeconds() = 0;
	// The frame timer's wait (WaitForNextFrame) of Duration milliseconds.
	virtual void FrameWait(std::uint32_t Duration) = 0;

protected:
	// Never destroyed through the interface: the implementations live as long as the process (the client reads the
	// clocks until its very end, static destructors included).
	~IClock() = default;
};

} // namespace patella::platform
