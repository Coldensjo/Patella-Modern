// Clock.h: the clocks of the test mode (issue #21; notes/modernization-plan.md, 4.4).
//
// The client reads three clocks: timeGetTime (sysinc.cpp GetMilliseconds and the seed of rand, the
// engines' frame timer), the wall clock (time, 64 bits since issue #62: talk line stamps, skull times, error.txt,
// saved channels) and the frame timer's Sleep (WaitForNextFrame). The test build (PatellaTest.exe)
// sends all three through one IClock (src/TestHooks.h); the product and oracle builds call the system
// functions directly and contain none of this.
//
// TVirtualClock is the clock of a scripted run: it starts where tools/capture/patcap.c's virtual
// clock starts (timeGetTime 100000 at the first call, the wall clock at 2026-11-02 12:00:00 UTC), so
// srand gets the same seed in every run, and it moves only when the driver advances it
// (TestModeWin32.cpp, InputScript.h). Portable: no platform header.

#pragma once

#include <cstdint>

namespace patella::testmode {

// The start of the virtual clock: tools/capture/patcap.c's BASE_TIME and FILETIME_BASE.
inline constexpr std::uint32_t VirtualStartMilliseconds = 100000;
inline constexpr std::int64_t VirtualStartWallSeconds = 1793620800; // 2026-11-02 12:00:00 UTC
// The main frame's timer (MainFrm.cpp OnCreate: SetTimer id 1, 100 ms): one tick of ActionNumber.
inline constexpr std::uint32_t TickMilliseconds = 100;

class IClock {
public:
	IClock() = default;
	IClock(const IClock &) = delete;
	IClock &operator=(const IClock &) = delete;
	IClock(IClock &&) = delete;
	IClock &operator=(IClock &&) = delete;
	virtual ~IClock() = default;

	// timeGetTime: milliseconds, wrapping at 2^32.
	[[nodiscard]] virtual std::uint32_t Milliseconds() = 0;
	// The wall clock: seconds since 1970-01-01 00:00:00 UTC.
	[[nodiscard]] virtual std::int64_t WallSeconds() = 0;
	// The frame timer's wait (WaitForNextFrame) of Duration milliseconds.
	virtual void FrameWait(std::uint32_t Duration) = 0;
};

class TVirtualClock final : public IClock {
public:
	// YieldFunction is called by FrameWait instead of sleeping (the test build sleeps a real millisecond
	// there, so that a client waiting for the driver does not spin); nullptr: nothing.
	explicit TVirtualClock(void (*YieldFunction)() = nullptr) : YieldHook(YieldFunction) {}

	[[nodiscard]] std::uint32_t Milliseconds() override { return VirtualStartMilliseconds + Now; }
	[[nodiscard]] std::int64_t WallSeconds() override { return VirtualStartWallSeconds + Now / 1000; }
	// The virtual time does not move: frames are not time.
	void FrameWait(std::uint32_t Duration) override;

	// Milliseconds since the start.
	[[nodiscard]] std::uint32_t Elapsed() const { return Now; }
	// Moves the clock to Target milliseconds after the start; never backwards (false, unchanged).
	bool AdvanceTo(std::uint32_t Target);
	// How often FrameWait was called (the unit tests, the driver's log).
	[[nodiscard]] std::uint64_t FrameWaits() const { return Waits; }

private:
	void (*YieldHook)();
	std::uint32_t Now = 0;
	std::uint64_t Waits = 0;
};

} // namespace patella::testmode
