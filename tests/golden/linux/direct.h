// The golden runner's stand-in for the Microsoft CRT's <direct.h> on Linux (issue #91): the folder functions the
// ported tests call, over POSIX.

#pragma once

#include <sys/stat.h>
#include <unistd.h>

inline int _mkdir(const char *Path) { return mkdir(Path, 0777); }
inline int _rmdir(const char *Path) { return rmdir(Path); }
inline int _chdir(const char *Path) { return chdir(Path); }
inline char *_getcwd(char *Buffer, int Size) { return getcwd(Buffer, static_cast<size_t>(Size)); }
