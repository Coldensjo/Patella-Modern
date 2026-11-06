// The golden runner's stand-in for the Microsoft CRT's <process.h> on Linux (issue #91).

#pragma once

#include <unistd.h>

inline int _getpid() { return static_cast<int>(getpid()); }
