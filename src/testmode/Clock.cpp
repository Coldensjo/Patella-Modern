// Clock.cpp: the virtual clock of the test mode (Clock.h; issue #21).

#include "Clock.h"
#include <cstdint>

namespace patella::testmode {

void TVirtualClock::FrameWait(std::uint32_t /*Duration*/)
{
	Waits++;
	if (YieldHook != nullptr)
		YieldHook();
}

bool TVirtualClock::AdvanceTo(std::uint32_t Target)
{
	if (Target < Now)
		return false;
	Now = Target;
	return true;
}

} // namespace patella::testmode
