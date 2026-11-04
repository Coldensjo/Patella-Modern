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

#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
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
#define MsvcCtime32 _ctime32
#define MsvcLocaltime32 _localtime32
#define MsvcFileno _fileno
#define MsvcExecl(Path, Argument) _execl(Path, Argument, nullptr)
#define MsvcDupenv _dupenv_s
#define MsvcPutenv _putenv_s

#else

// The implementations of the other platforms (Linux, M8): the pure ones in compat/msvc_portable.h, the system
// ones here.
#include "compat/msvc_portable.h"

// _snprintf: PortableVsnprintf's semantics.
PATELLA_PRINTF_FORMAT(3, 4)
inline int MsvcSnprintf(char *Buffer, std::size_t Count, const char *Format, ...) // NOLINT(cppcoreguidelines-pro-type-vararg): _snprintf's interface
{
	std::va_list Values; // NOLINT(cppcoreguidelines-pro-type-vararg): _snprintf's interface
	va_start(Values, Format); // NOLINT(cppcoreguidelines-pro-type-vararg)
	const int Result = patella::compat::PortableVsnprintf(Buffer, Count, Format, Values);
	va_end(Values); // NOLINT(cppcoreguidelines-pro-type-vararg)
	return Result;
}

// _vsnprintf.
PATELLA_PRINTF_FORMAT(3, 0)
inline int MsvcVsnprintf(char *Buffer, std::size_t Count, const char *Format, std::va_list Values)
{
	return patella::compat::PortableVsnprintf(Buffer, Count, Format, Values);
}

// _stricmp.
inline int MsvcStricmp(const char *Left, const char *Right)
{
	return patella::compat::PortableStricmp(Left, Right);
}

// _strdup.
inline char *MsvcStrdup(const char *Text)
{
	return patella::compat::PortableStrdup(Text);
}

// _time32: the time now in 32 bits, also stored in *Time when Time is not null.
inline Time32 MsvcTime32(Time32 *Time)
{
	const Time32 Now = patella::compat::PortableTime32(std::time(nullptr));
	if (Time != nullptr)
		*Time = Now;
	return Now;
}

// _ctime32.
inline char *MsvcCtime32(const Time32 *Time)
{
	return patella::compat::PortableCtime32(Time);
}

// _localtime32.
inline std::tm *MsvcLocaltime32(const Time32 *Time)
{
	return patella::compat::PortableLocaltime32(Time);
}

// _fileno: the descriptor under a FILE.
inline int MsvcFileno(std::FILE *File)
{
	return fileno(File);
}

// _execl with one argument: replaces the process with the program Path, which gets Argument as its argv[0]
// (Player.cpp: patch.exe). Returns only when that fails (-1).
inline std::intptr_t MsvcExecl(const char *Path, const char *Argument)
{
	return execl(Path, Argument, static_cast<char *>(nullptr)); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

// _dupenv_s: *Value is a copy of the environment variable Name from malloc (the caller frees it) and *Length
// its size with the NUL, or null and 0 when it is not set; returns 0, or an errno value (EINVAL without
// Value or Name, ENOMEM).
inline int MsvcDupenv(char **Value, std::size_t *Length, const char *Name)
{
	if (Value == nullptr || Name == nullptr)
		return EINVAL;
	*Value = nullptr;
	if (Length != nullptr)
		*Length = 0;
	const char *Text = std::getenv(Name);
	if (Text == nullptr)
		return 0;
	*Value = patella::compat::PortableStrdup(Text);
	if (*Value == nullptr)
		return ENOMEM;
	if (Length != nullptr)
		*Length = std::strlen(Text) + 1;
	return 0;
}

// _putenv_s: sets the environment variable Name to Value; an empty Value removes it. Returns 0, or an errno
// value (EINVAL: no Name or Value, an empty Name, or a Name with '=').
inline int MsvcPutenv(const char *Name, const char *Value)
{
	if (Name == nullptr || Value == nullptr || *Name == 0 || std::strchr(Name, '=') != nullptr)
		return EINVAL;
	const int Result = *Value == 0 ? unsetenv(Name) : setenv(Name, Value, 1);
	return Result == 0 ? 0 : errno;
}

#endif
