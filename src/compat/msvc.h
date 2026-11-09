// msvc.h: the Microsoft C runtime's and compiler's own spellings that the client uses, behind portable
// names (Patella-Modern issue #43; notes/decisions.md, "MSVC-only spellings behind portable functions").
// Not part of the original source.
//
// Each name keeps the semantics of the Microsoft function it stands for, not those of the similar standard or
// POSIX function: MsvcSnprintf is _snprintf (no NUL when the text fills the buffer, -1 when it does not fit),
// not snprintf. On Windows (MSVC, and mingw-w64, whose CRT is Microsoft's) each name is the Microsoft
// function itself (a macro: below, "On Windows"), so the images do not change; elsewhere (Linux, M8) a
// function with the same semantics from the C and POSIX libraries. The pure ones (formatting, comparison,
// copy) are in compat/msvc_portable.h, which this header includes only off Windows and
// tests/unit/core_msvc_test.cpp on every platform, so that the test holds them against the Microsoft
// functions on Windows and against the same expected values everywhere.
//
// Not here, the Windows platform's own spellings, which stay where they are: the Windows API with its
// calling conventions (WINAPI, CALLBACK, APIENTRY on callbacks and imports), the Windows SDK's and the
// CRT's configuration macros (_CRT_SECURE_NO_WARNINGS, _WINSOCK_DEPRECATED_NO_WARNINGS, WIN32_LEAN_AND_MEAN),
// the CRT's file functions behind types/File.h's TFile (fopen as _fsopen, _fileno, the test mode's UTF-16 names
// with _wfopen_s; issue #64) and
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
#include <ctime>   // IWYU pragma: keep (the client's time_t; PATELLA_TIME's expansion until issue #109; here since #43)


// --- the compiler's spellings ---

// __cdecl, the calling convention of a function whose address goes to a caller that expects it (the Vulkan
// engine's frame observers, the CRT): on 32-bit x86 Windows the convention is part of the function's type and of how
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

// #pragma GCC diagnostic of GCC as a macro (_Pragma), for the suppressions of GCC's static analyzer (-fanalyzer,
// patella_core; issue #63): nothing for the other compilers. Clang knows the pragma but not GCC's analyzer
// warnings, and warns about an unknown warning option. The text is the pragma's, in quotes:
// PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push"), PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-W...\""),
// PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop").
#if defined(__GNUC__) && !defined(__clang__)
#define PATELLA_GCC_DIAGNOSTIC(Text) _Pragma(Text)
#else
#define PATELLA_GCC_DIAGNOSTIC(Text)
#endif

// AddressSanitizer's checks off for a function, or (MSVC and Clang) the redzones of a global (issue #70): the
// original's out-of-bounds reads that ASan reports and that stay until behaviour may change, each listed with
// its reason in notes/decisions.md ("Sanitizers over the replayed sessions"). Only in a build with ASan (MSVC
// /fsanitize=address and GCC define __SANITIZE_ADDRESS__, Clang has __has_feature(address_sanitizer));
// everywhere else nothing, so no other build's code changes.
#if defined(__SANITIZE_ADDRESS__) && defined(_MSC_VER) && !defined(__clang__)
#define PATELLA_NO_SANITIZE_ADDRESS __declspec(no_sanitize_address)
#elif defined(__clang__) && defined(__has_feature)
#if __has_feature(address_sanitizer)
#define PATELLA_NO_SANITIZE_ADDRESS __attribute__((no_sanitize("address")))
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define PATELLA_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#endif
#if !defined(PATELLA_NO_SANITIZE_ADDRESS)
#define PATELLA_NO_SANITIZE_ADDRESS
#endif

// A function kept out of line, as the original has it, where a test replaces its code (the golden corpus's
// fakes: tests/golden/commworld.cpp InstallFakes) and the compiler would otherwise inline it into a caller now
// that it has no exception state (Patella-Modern issue #78): MSVC's __declspec(noinline), GCC's and Clang's
// noinline attribute. Only the place of the code changes, not what it does.
#if defined(_MSC_VER)
#define PATELLA_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define PATELLA_NOINLINE __attribute__((noinline))
#else
#define PATELLA_NOINLINE
#endif

// printf's format check of GCC and Clang on a function with a format parameter (MSVC has none).
#if defined(__GNUC__)
#define PATELLA_PRINTF_FORMAT(FormatIndex, FirstArgument) [[gnu::format(printf, FormatIndex, FirstArgument)]]
#else
#define PATELLA_PRINTF_FORMAT(FormatIndex, FirstArgument)
#endif

// --- the CRT's semantics, portable ---

// No 32-bit time here since issue #62: the client's time is time()'s 64 bits, and the original's 4-byte time
// fields convert at their boundary, types/Time32.h.

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
#define MsvcDupenv _dupenv_s
#define MsvcPutenv _putenv_s

#else

// The implementations of the other platforms (Linux, M8): compat/msvc_portable.h, the pure ones and, at its end,
// the Msvc* names over them and POSIX. They are there since issue #49, so that msvc_portable.h compiles on its own:
// it needs this header's spellings, which come first, and this header needs nothing after them.
#include "compat/msvc_portable.h" // IWYU pragma: export

// The Linux client (issue #131, cmake/LinuxX64.cmake): the original's rand, MSVC's (the state * 214013 + 2531011, bits
// 16-30), so the client's random numbers (the XTEA keys and the login's filler, Network.cpp's first server, the frames'
// padding) are the ones a seed gives on Windows; glibc's are another sequence. Not the golden runner's: its rand is its
// import slot (tests/golden/linux/windows.h).
#if defined(PATELLA_MSVC_RAND) && !defined(_WIN32)
inline unsigned int &PatellaRandState() noexcept
{
	static unsigned int State = 1;
	return State;
}
inline void PatellaSrand(unsigned int Seed) noexcept
{
	PatellaRandState() = Seed;
}
inline int PatellaRand() noexcept
{
	unsigned int &State = PatellaRandState();
	State = State * 214013U + 2531011U;
	return static_cast<int>((State >> 16U) & 0x7fffU);
}
// The macros below also rename the std::rand in libstdc++'s own headers (bits/stl_algo.h), so these names must exist in std.
namespace std {
using ::PatellaRand;  // NOLINT(bugprone-std-namespace-modification): the target of the macro rand, below
using ::PatellaSrand; // NOLINT(bugprone-std-namespace-modification): the target of the macro srand, below
} // namespace std
#define rand PatellaRand
#define srand PatellaSrand
#endif

#endif
