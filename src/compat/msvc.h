// msvc.h: the Microsoft C runtime's and compiler's own spellings that the client uses, behind portable
// names (Patella-Modern issue #43; notes/decisions.md, "MSVC-only spellings behind portable functions").
// Not part of the original source.
//
// Each name keeps the semantics of the Microsoft function it stands for, not those of the similar standard or
// POSIX function: MsvcSnprintf is _snprintf (no NUL when the text fills the buffer, -1 when it does not fit),
// not snprintf. On Windows (MSVC, and mingw-w64, whose CRT is Microsoft's) each name is the Microsoft
// function itself (a macro: below, "On Windows"), so the images do not change; elsewhere (Linux, M8) a
// function with the same semantics from the C and POSIX libraries. The pure ones (formatting, comparison,
// copy, the 32-bit time) are in compat/msvc_portable.h, which this header includes only off Windows and
// tests/unit/core_msvc_test.cpp on every platform, so that the test holds them against the Microsoft
// functions on Windows and against the same expected values everywhere.
//
// Not here, the Windows platform's own spellings, which stay where they are: the Windows API with its
// calling conventions (WINAPI, CALLBACK, APIENTRY on callbacks and imports), the Windows SDK's and the
// CRT's configuration macros (_CRT_SECURE_NO_WARNINGS, _WINSOCK_DEPRECATED_NO_WARNINGS, WIN32_LEAN_AND_MEAN),
// the CRT's UTF-16 file names of the Windows-only test mode (_wfopen_s, src/testmode/TestModeWin32.cpp) and
// sysinc.cpp's #pragma comment(lib, "winmm.lib") (Windows-only code: timeGetTime, the clipboard).
//
// The descriptor functions of <io.h> (_access, _eof) are in compat/msvc_io.h, which only the files that use
// them include, where they included <io.h> before: <io.h> included through common.h makes MSVC x64 emit the
// <charconv> digit table std::_Digit_pairs<char> first among an object's constants, which moves it in the x64
// images (measured, issue #43).

#pragma once

#include <cerrno>  // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#include <cstdarg> // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#include <cstddef> // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#include <cstdint> // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#include <cstdio>  // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#include <cstdlib> // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#include <cstring> // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#include <ctime>   // IWYU pragma: keep (on Windows the Msvc* macros' expansions need it)
#if defined(_WIN32)
#include <process.h>
#endif


// --- the compiler's spellings ---

// __cdecl, the calling convention of a function whose address goes to a caller that expects it (patcap.dll's
// frame observers, the CRT): on 32-bit x86 Windows the convention is part of the function's type and of how
// it is called; on x64 and on Linux there is only one convention, and the keyword means nothing.
#if defined(_MSC_VER) && defined(_M_IX86)
#define PATELLA_CDECL __cdecl
#else
#define PATELLA_CDECL
#endif

// #pragma warning(...) of MSVC as a macro (__pragma), for a statement or a declaration: nothing for the
// other compilers, which do not know MSVC's warning numbers (GCC and Clang warn about an unknown pragma).
// PATELLA_MSVC_WARNING(push), PATELLA_MSVC_WARNING(disable : 4297), PATELLA_MSVC_WARNING(pop).
#if defined(_MSC_VER)
#define PATELLA_MSVC_WARNING(...) __pragma(warning(__VA_ARGS__))
#else
#define PATELLA_MSVC_WARNING(...)
#endif

// printf's format check of GCC and Clang on a function with a format parameter (MSVC has none).
#if defined(__GNUC__)
#define PATELLA_PRINTF_FORMAT(FormatIndex, FirstArgument) [[gnu::format(printf, FormatIndex, FirstArgument)]]
#else
#define PATELLA_PRINTF_FORMAT(FormatIndex, FirstArgument)
#endif

// --- the CRT's semantics, portable ---

// The 32-bit time of _time32, __time32_t: seconds since 1970-01-01 00:00 UTC in 32 bits (Creatures.h's skull
// time and Talk.h's line time are 4-byte fields of the original's layout).
#if defined(_WIN32)
using Time32 = __time32_t;
#else
using Time32 = std::int32_t;
#endif
static_assert(sizeof(Time32) == 4);

#if defined(_WIN32)

// On Windows each name is the Microsoft function's own (a macro, not a forwarder), so that every call is the
// same call as before and the client's translation units declare nothing more: a forwarder template for
// _snprintf inlines into other code (MSVC x64 copies SetCharacter's four uint8 before their promotion to int:
// 16 or 32 bytes more code in Status.cpp, by value or by reference), and the inline forwarders, though they
// inline away, are declarations that move globals (MSVC emits an object's globals in an order that depends on
// how many names the translation unit declares: with them, Surface.cpp's globals came out rotated, its .bss in
// the oracle and its .data on x64). Measured on the images (issue #43; notes/decisions.md).
#define MsvcSnprintf _snprintf
#define MsvcVsnprintf _vsnprintf
#define MsvcStricmp _stricmp
#define MsvcStrdup _strdup
#define MsvcTime32 _time32
#define MsvcLocaltime32 _localtime32
#define MsvcFileno _fileno
#define MsvcExecl(Path, Argument) _execl(Path, Argument, nullptr)
#define MsvcDupenv _dupenv_s
#define MsvcPutenv _putenv_s

#else

// The implementations of the other platforms (Linux, M8): compat/msvc_portable.h, the pure ones and, at its end,
// the Msvc* names over them and POSIX. They are there since issue #49, so that msvc_portable.h compiles on its own:
// it needs this header's spellings and Time32, which come first, and this header needs nothing after them.
#include "compat/msvc_portable.h" // IWYU pragma: export

#endif
