// crt71.h: behaviour of MSVC 7.1's CRT (libcmt.lib, linked into Tibia.exe) that the CRT Patella.exe links
// (the UCRT, or glibc) does differently and that reaches a file the client writes. Not part of the original
// source.
//
// ctime/asctime: the client writes ctime's text into saved channels (ConsoleWindow.cpp SaveChannel; error.txt
// too, Player.cpp HandleException, until Patella-Modern issue #74). MSVC 7.1's asctime (0x517f6d in Tibia.exe)
// writes every number with two digits, the day of the month too (tm_mday / 10 + '0', tm_mday % 10 + '0'):
// "Mon Nov 02 12:14:46 2026\n". The UCRT and glibc pad the day with a space, "Mon Nov  2 12:14:46 2026\n", as the C standard asks (found by
// tools/compare_files.py, issue #21). FormatCRT71Time writes MSVC 7.1's text itself (Patella-Modern issue #61;
// before it, the client patched the 9th character of ctime's buffer).
//
// Local time is the C library's localtime (_localtime64_s on Windows, localtime_r elsewhere), not std::chrono's
// time zone database: MSVC 7.1's localtime takes the zone from the TZ variable or from Windows'
// GetTimeZoneInformation and applies its rules to every year, and the UCRT does the same, so the UCRT's fields are
// the original's (std::chrono::current_zone() reads ICU's zones with their history and ignores TZ). On Linux it is
// glibc's localtime: TZ, else /etc/localtime. notes/decisions.md, issue #61.

#pragma once

#include "types/FixedString.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string_view>

#if !defined(_WIN32)
#include <time.h> // NOLINT(modernize-deprecated-headers): localtime_r is POSIX's, declared by <time.h>
#endif

// ctime's text in MSVC 7.1's form: "Www Mmm dd hh:mm:ss yyyy\n", 25 characters and the NUL.
using TCRT71TimeText = TFixedString<26>;

namespace patella::compat {

// The last time the UCRT's localtime converts: 3000-12-31 23:59:59 UTC (_MAX__TIME64_T, UCRT time/localtime.cpp).
// The same limit everywhere, so that every platform formats the same times.
inline constexpr std::int64_t CRT71LastTime = 32'535'215'999;

// The local time of Time (seconds since 1970-01-01 00:00 UTC) as the C library's localtime gives it. False where
// it gives none. On Windows _localtime64_s for every time (issue #62; from issue #61 to it, _localtime32_s up to
// 2038-01-18 23:59:59 UTC, what _ctime32 gave the client before: tests/unit/crt71_test.cpp holds the two equal on
// every hour of 32 bits and every 61st second of their last four days, in four zones), elsewhere localtime_r.
inline bool CRT71LocalTime(std::int64_t Time, std::tm &Local)
{
#if defined(_WIN32)
	const __time64_t Value = Time;
	return _localtime64_s(&Local, &Value) == 0;
#else
	const std::time_t Value = Time;
	return localtime_r(&Value, &Local) != nullptr;
#endif
}

} // namespace patella::compat

// ctime(&Time)'s text as MSVC 7.1 wrote it, into Text; returns Text, or nullptr where its ctime returned NULL:
// for a time before 1970 (its time_t had 32 bits and its localtime rejected a negative one, as _ctime32 does) and
// for a time after CRT71LastTime, which 32 bits never reached.
inline const char *FormatCRT71Time(TCRT71TimeText &Text, std::int64_t Time)
{
	static constexpr std::array<std::string_view, 7> DayNames = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static constexpr std::array<std::string_view, 12> MonthNames = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
		"Aug", "Sep", "Oct", "Nov", "Dec"};
	std::tm Local{};
	if (Time < 0 || Time > patella::compat::CRT71LastTime || !patella::compat::CRT71LocalTime(Time, Local))
		return nullptr;
	// asctime's fields: each number in two digits, the year as its century (19 + tm_year / 100) and the rest.
	Text.Format("{} {} {:02} {:02}:{:02}:{:02} {:02}{:02}\n", DayNames.at(static_cast<std::size_t>(Local.tm_wday)),
		MonthNames.at(static_cast<std::size_t>(Local.tm_mon)),
		Local.tm_mday, Local.tm_hour, Local.tm_min, Local.tm_sec, 19 + Local.tm_year / 100, Local.tm_year % 100);
	return Text;
}
