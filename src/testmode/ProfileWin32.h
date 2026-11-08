// ProfileWin32.h: the test build's profile windows (Patella-Modern issue #134; tools/profiling.py, notes/testmode.md,
// "Profile windows"). Test build only (PatellaTest.exe): Patella.exe contains none of it.

#pragma once

#include <string>

namespace patella::testmode {

// The control channel's "profile start <kind> <file>" and "profile stop" (kinds: time, allocs, gpu). Empty when
// done, else why not. On the system clock or the virtual one: the samples are always the performance counter's.
std::string ProfileCommand(const std::string &Text);

} // namespace patella::testmode
