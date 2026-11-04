// msvc_portable.h: the Microsoft CRT's semantics of compat/msvc.h written with the C library, for the platforms
// without the Microsoft CRT (Patella-Modern issue #43). Not part of the original source. compat/msvc.h includes
// it off Windows, where its Msvc* functions are these; tests/unit/core_msvc_test.cpp includes it everywhere and
// holds it against the Microsoft functions on Windows. The client's Windows translation units do not include it
// (compat/msvc.h says why).

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#if !defined(_WIN32)
// The Msvc* names at the end: errno values, and POSIX's fileno, setenv, unsetenv and execl, which the C headers
// declare, not <cstdio> and <cstdlib>.
#include <cerrno>
#include <stdio.h>  // NOLINT(modernize-deprecated-headers)
#include <stdlib.h> // NOLINT(modernize-deprecated-headers)
#include <unistd.h>
#endif

// PATELLA_PRINTF_FORMAT and Time32 (compat/msvc.h includes this header off Windows after them).
#include "compat/msvc.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996) // ctime and localtime are "unsafe" to MSVC
#endif

namespace patella::compat {

// _vsnprintf: formats at most Count characters into Buffer. The text and its NUL fit (length < Count): both are
// written, the length is returned. The text fits exactly (length == Count): written without a NUL, Count is
// returned. Longer: the first Count characters are written without a NUL, -1 is returned. A null Buffer with
// Count 0 returns the text's length; an output error -1. (UCRT stdio/output.cpp common_vsprintf with
// _CRT_INTERNAL_PRINTF_LEGACY_VSPRINTF_NULL_TERMINATION, which _vsnprintf and _snprintf pass.)
PATELLA_PRINTF_FORMAT(3, 0)
inline int PortableVsnprintf(char *Buffer, std::size_t Count, const char *Format, std::va_list Arguments)
{
	std::va_list Again; // NOLINT(cppcoreguidelines-pro-type-vararg): _vsnprintf's interface
	va_copy(Again, Arguments);
	const int Length = std::vsnprintf(Buffer, Count, Format, Arguments);
	int Result = Length;
	if (Length >= 0 && Buffer != nullptr && static_cast<std::size_t>(Length) >= Count) {
		// vsnprintf ended the text with a NUL at Buffer[Count - 1]; _vsnprintf has a character there.
		if (Count != 0) {
			std::string Text(static_cast<std::size_t>(Length), '\0');
			std::vsnprintf(Text.data(), Text.size() + 1, Format, Again); // its NUL goes to Text.data()[Length]
			std::memcpy(Buffer, Text.data(), Count);
		}
		if (static_cast<std::size_t>(Length) != Count)
			Result = -1;
	}
	va_end(Again);
	return Result < 0 ? -1 : Result;
}

// _snprintf: PortableVsnprintf's semantics.
PATELLA_PRINTF_FORMAT(3, 4)
inline int PortableSnprintf(char *Buffer, std::size_t Count, const char *Format, ...) // NOLINT(cppcoreguidelines-pro-type-vararg): _snprintf's interface
{
	std::va_list Arguments;      // NOLINT(cppcoreguidelines-pro-type-vararg): _snprintf's interface
	va_start(Arguments, Format); // NOLINT(cppcoreguidelines-pro-type-vararg)
	const int Result = PortableVsnprintf(Buffer, Count, Format, Arguments);
	va_end(Arguments); // NOLINT(cppcoreguidelines-pro-type-vararg)
	return Result;
}

// _stricmp in the "C" locale (the client never calls setlocale): compares the bytes as unsigned char, a pair
// that differs with A-Z mapped to a-z (so "abc_" < "ABCD", '_' < 'd'), and returns the difference of the first
// pair that still differs, or 0. Bytes from 0x80 are not folded. (UCRT string/stricmp.cpp __ascii_stricmp.)
inline int PortableStricmp(const char *Left, const char *Right)
{
	const auto Lower = [](int Value) { return Value >= 'A' && Value <= 'Z' ? Value - 'A' + 'a' : Value; };
	const auto *L = reinterpret_cast<const unsigned char *>(Left);
	const auto *R = reinterpret_cast<const unsigned char *>(Right);
	int Result = 0;
	int LeftValue = 0;
	do {
		LeftValue = *L++;
		int RightValue = *R++;
		if (LeftValue != RightValue) {
			LeftValue = Lower(LeftValue);
			RightValue = Lower(RightValue);
		}
		Result = LeftValue - RightValue;
	} while (Result == 0 && LeftValue != 0);
	return Result;
}

// _strdup: a copy in memory from malloc (the caller frees it with free), or null when Text is null or the
// memory is not there. (UCRT string/strdup.cpp.)
inline char *PortableStrdup(const char *Text)
{
	if (Text == nullptr)
		return nullptr;
	const std::size_t Size = std::strlen(Text) + 1;
	auto *Copy = static_cast<char *>(std::malloc(Size)); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): _strdup's contract, freed with free
	if (Copy == nullptr)
		return nullptr;
	std::memcpy(Copy, Text, Size);
	return Copy;
}

// _time32's value for a time: -1 after 2038-01-19 03:14:07 UTC, where 32 bits end (UCRT time/time.cpp
// common_time), and for -1 (no time).
inline Time32 PortableTime32(std::time_t Now)
{
	if (Now < 0 || Now > static_cast<std::time_t>(INT32_MAX))
		return -1;
	return static_cast<Time32>(Now);
}

// _ctime32: ctime's text of a 32-bit time, null for a time before 1970 (UCRT time/ctime.cpp; ctime itself
// would format it).
inline char *PortableCtime32(const Time32 *Time)
{
	if (*Time < 0)
		return nullptr;
	const std::time_t Value = *Time;
	return std::ctime(&Value); // NOLINT(bugprone-unsafe-functions): _ctime32 returns its static buffer too
}

// _localtime32: localtime of a 32-bit time, null for a time before 1970 (UCRT time/localtime.cpp).
inline std::tm *PortableLocaltime32(const Time32 *Time)
{
	if (*Time < 0)
		return nullptr;
	const std::time_t Value = *Time;
	return std::localtime(&Value); // _localtime32 returns its static tm too
}

} // namespace patella::compat

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if !defined(_WIN32)

// The Msvc* names of compat/msvc.h off Windows (on Windows they are the Microsoft functions themselves; issue
// #49 moved them here from compat/msvc.h).

// _snprintf: PortableVsnprintf's semantics.
PATELLA_PRINTF_FORMAT(3, 4)
inline int MsvcSnprintf(char *Buffer, std::size_t Count, const char *Format, ...) // NOLINT(cppcoreguidelines-pro-type-vararg): _snprintf's interface
{
	std::va_list Values;      // NOLINT(cppcoreguidelines-pro-type-vararg): _snprintf's interface
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
