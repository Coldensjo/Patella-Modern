// The file cmake/BuildConfig.cmake compiles on every configure (issue #169): BuildConfig.h and its checks, with the values
// the compile names. No function: only whether it compiles counts.

#include "BuildConfig.h" // IWYU pragma: keep (its static_asserts are what is compiled)
