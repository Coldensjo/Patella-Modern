// LoopTimer.h: TLoopTimer, the main frame's timer as a check of the event loop (Patella-Modern issue #122, plan M8;
// notes/decisions.md, "#122: the SDL3 window and its event loop"). Not part of the original source. Portable: no
// platform header.
//
// The MFC frame's 100 ms timer is Windows' SetTimer: a WM_TIMER message, made by the message queue only when it is
// otherwise empty, and at most one at a time (ticks that come due while one waits are not queued: no backlog). The
// SDL3 window's loop asks Due once per pass instead, with the platform's clock (IClock::Milliseconds, a 32-bit
// millisecond count that wraps): true once Interval milliseconds have passed since the last tick (or the start), and
// the next tick is then due Interval after this one was seen, as the next WM_TIMER after a late one. So a loop held
// up for longer than several intervals ticks once, not once per interval.

#pragma once

#include <cstdint>

namespace patella::platform {

class TLoopTimer {
public:
	// Starts the timer (SetTimer): the first tick is due Interval milliseconds after Now. An interval of 0 counts as 1,
	// as SetTimer's USER_TIMER_MINIMUM raises it.
	void Start(std::uint32_t Interval, std::uint32_t Now)
	{
		Period = Interval > 0 ? Interval : 1;
		Last = Now;
		Running = true;
	}
	// Stops it (KillTimer): no tick is due until the next Start.
	void Stop() { Running = false; }
	[[nodiscard]] bool IsRunning() const { return Running; }
	// One tick when it is due at Now (the elapsed time computed modulo 2^32, so it holds across the clock's wrap).
	[[nodiscard]] bool Due(std::uint32_t Now)
	{
		if (!Running || static_cast<std::uint32_t>(Now - Last) < Period)
			return false;
		Last = Now;
		return true;
	}

private:
	std::uint32_t Period = 1;
	std::uint32_t Last = 0;
	bool Running = false;
};

} // namespace patella::platform
