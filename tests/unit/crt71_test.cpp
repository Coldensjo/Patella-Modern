// FormatCRT71Time (src/compat/crt71.h, Patella-Modern issue #61): ctime's text as MSVC 7.1's CRT wrote it, the day of
// the month with two digits. Compiled into tests_unit (std::format_to_n) and tests_unit_cxx26_standin (the C++17
// stand-in of compat/format.h, which the oracle-x86 build formats with), so both paths of TFixedString::Format
// write the text.
//
// What the text is held against:
//	- tables of times and their texts, computed apart (Python's datetime), in UTC (every month and weekday, the
//	  two-digit days, leap days, 2038 and after) and in PST8PDT around the changes of 2026;
//	- std::chrono's calendar and its "%a %b %d %H:%M:%S %Y" (which writes the day with two digits too) for a
//	  spread of times in UTC up to the year 3000, and in PST8PDT around every change of daylight saving time
//	  from 2007 to 2037 (the rules computed with year_month_weekday);
//	- on Windows, the UCRT's own _ctime32 with its 9th character made MSVC 7.1's (what the client wrote before
//	  issue #61, which tools/compare_files.py and difftest held against Tibia.exe), for every hour of 1970 to
//	  2038 and the second before it, in UTC, in PST8PDT (the UCRT's rules of before and after 2007), east of UTC
//	  with half an hour, and in the machine's own zone.
// The zone is set with TZ for the length of a case and restored after it.

#include "compat/crt71.h"
#include "compat/msvc.h"

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <version>

#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L && defined(__cpp_lib_format)
#include <format>
#define PATELLA_TEST_CHRONO_FORMAT 1
#else
#define PATELLA_TEST_CHRONO_FORMAT 0
#endif

#if !defined(_WIN32)
#include <time.h> // NOLINT(modernize-deprecated-headers): tzset is POSIX's
#endif

namespace {

// TZ set to Zone (or removed: nullptr) and the C library told, until the end of the scope.
class TTimeZone {
public:
	explicit TTimeZone(const char *Zone)
	{
		char *Old = nullptr;
		std::size_t Length = 0;
		if (MsvcDupenv(&Old, &Length, "TZ") == 0 && Old != nullptr) {
			Saved = Old;
			std::free(Old); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): _dupenv_s's contract
		}
		Set(Zone);
	}
	~TTimeZone() { Set(Saved ? Saved->c_str() : nullptr); }
	TTimeZone(const TTimeZone &) = delete;
	TTimeZone &operator=(const TTimeZone &) = delete;
	TTimeZone(TTimeZone &&) = delete;
	TTimeZone &operator=(TTimeZone &&) = delete;

private:
	std::optional<std::string> Saved;

	static void Set(const char *Zone)
	{
		MsvcPutenv("TZ", Zone != nullptr ? Zone : ""); // an empty value removes it
#if defined(_WIN32)
		_tzset();
#else
		tzset();
#endif
	}
};

// The United States' Pacific zone: the UCRT reads "PST8PDT" with its own rules (from 2007 the second Sunday of
// March to the first of November, before it the first Sunday of April to the last of October, 2 a.m.), glibc
// needs today's written out.
#if defined(_WIN32)
constexpr const char *PacificZone = "PST8PDT";
#else
constexpr const char *PacificZone = "PST8PDT,M3.2.0,M11.1.0";
#endif

std::string Format(std::int64_t Time)
{
	TCRT71TimeText Text;
	const char *Result = FormatCRT71Time(Text, Time);
	return Result != nullptr ? std::string(Result) : std::string("(null)");
}

struct TExpected {
	std::int64_t Time;
	std::string_view Text;
};

// Python: datetime.fromtimestamp(Time, timezone.utc).strftime("%a %b %d %H:%M:%S %Y").
constexpr std::array UTCTexts = {
	TExpected{0, "Thu Jan 01 00:00:00 1970\n"},
	TExpected{1, "Thu Jan 01 00:00:01 1970\n"},
	TExpected{59, "Thu Jan 01 00:00:59 1970\n"},
	TExpected{3599, "Thu Jan 01 00:59:59 1970\n"},
	TExpected{86399, "Thu Jan 01 23:59:59 1970\n"},
	TExpected{86400, "Fri Jan 02 00:00:00 1970\n"},
	TExpected{951782400, "Tue Feb 29 00:00:00 2000\n"},
	TExpected{951868799, "Tue Feb 29 23:59:59 2000\n"},
	TExpected{1000000000, "Sun Sep 09 01:46:40 2001\n"},
	TExpected{1149770096, "Thu Jun 08 12:34:56 2006\n"},
	TExpected{1700000000, "Tue Nov 14 22:13:20 2023\n"},
	TExpected{1709208000, "Thu Feb 29 12:00:00 2024\n"},
	TExpected{1793621686, "Mon Nov 02 12:14:46 2026\n"},
	TExpected{2147483647, "Tue Jan 19 03:14:07 2038\n"},
	TExpected{2147483648, "Tue Jan 19 03:14:08 2038\n"},
	TExpected{4102444800, "Fri Jan 01 00:00:00 2100\n"},
	TExpected{4107542399, "Sun Feb 28 23:59:59 2100\n"},
	TExpected{4107542400, "Mon Mar 01 00:00:00 2100\n"},
	TExpected{13574585228, "Tue Feb 29 06:07:08 2400\n"},
	TExpected{32535215999, "Wed Dec 31 23:59:59 3000\n"},
	TExpected{1231203723, "Tue Jan 06 01:02:03 2009\n"},
	TExpected{1233972246, "Sat Feb 07 02:04:06 2009\n"},
	TExpected{1236481569, "Sun Mar 08 03:06:09 2009\n"},
	TExpected{1239250092, "Thu Apr 09 04:08:12 2009\n"},
	TExpected{1241932215, "Sun May 10 05:10:15 2009\n"},
	TExpected{1244700738, "Thu Jun 11 06:12:18 2009\n"},
	TExpected{1247382861, "Sun Jul 12 07:14:21 2009\n"},
	TExpected{1250151384, "Thu Aug 13 08:16:24 2009\n"},
	TExpected{1252919907, "Mon Sep 14 09:18:27 2009\n"},
	TExpected{1255602030, "Thu Oct 15 10:20:30 2009\n"},
	TExpected{1258370553, "Mon Nov 16 11:22:33 2009\n"},
	TExpected{1261052676, "Thu Dec 17 12:24:36 2009\n"},
};

// PST8PDT (8 hours behind UTC, 7 in summer) around 2026's changes, 2026-03-08 10:00 UTC and 2026-11-01
// 09:00 UTC (the first of November: a one-digit day). Python, with the offsets.
constexpr std::array PacificTexts = {
	TExpected{1772963999, "Sun Mar 08 01:59:59 2026\n"},
	TExpected{1772964000, "Sun Mar 08 03:00:00 2026\n"},
	TExpected{1793516399, "Sat Oct 31 23:59:59 2026\n"},
	TExpected{1793516400, "Sun Nov 01 00:00:00 2026\n"},
	TExpected{1793520000, "Sun Nov 01 01:00:00 2026\n"},
	TExpected{1793523599, "Sun Nov 01 01:59:59 2026\n"},
	TExpected{1793523600, "Sun Nov 01 01:00:00 2026\n"},
	TExpected{1149770096, "Thu Jun 08 05:34:56 2006\n"},
	TExpected{1700000000, "Tue Nov 14 14:13:20 2023\n"},
	TExpected{2147483647, "Mon Jan 18 19:14:07 2038\n"},
	TExpected{4102444800, "Thu Dec 31 16:00:00 2099\n"},
};

#if PATELLA_TEST_CHRONO_FORMAT
// std::chrono's text of a time: "%a %b %d %H:%M:%S %Y" writes the day with two digits, as MSVC 7.1 did.
template <typename Clock>
std::string ChronoText(std::chrono::time_point<Clock, std::chrono::seconds> Time)
{
	return std::format("{:%a %b %d %H:%M:%S %Y}\n", Time);
}

// PST8PDT's local time of Time from the calendar (the rules of 2007 and later).
std::string PacificChronoText(std::chrono::sys_seconds Time)
{
	using namespace std::chrono;
	const year Year = year_month_day(floor<days>(Time)).year();
	const sys_seconds Start = sys_days(Year / March / Sunday[2]) + hours(2 + 8);
	const sys_seconds End = sys_days(Year / November / Sunday[1]) + hours(2 + 7);
	const hours Offset = Time >= Start && Time < End ? hours(-7) : hours(-8);
	return ChronoText(local_seconds((Time + Offset).time_since_epoch()));
}
#endif

#if defined(_WIN32)
// The UCRT's _ctime32 with the day as MSVC 7.1 wrote it (the 9th character: a space before a one-digit day). In
// the last 3 h 14 min of 32 bits, where _ctime32 gives NULL and MSVC 7.1's ctime did not (its localtime, 0x513eb1,
// takes every time up to INT32_MAX), the UCRT's _ctime64.
std::string CRT71Ctime32(std::int64_t Time)
{
	std::array<char, 26> Buffer = {};
	errno_t Result = 0;
	if (Time > patella::compat::UCRTLastTime32 && Time <= INT32_MAX) {
		const __time64_t Value = Time;
		Result = _ctime64_s(Buffer.data(), Buffer.size(), &Value);
	} else {
		const auto Value = static_cast<__time32_t>(Time);
		Result = _ctime32_s(Buffer.data(), Buffer.size(), &Value);
	}
	if (Result != 0)
		return "(null)";
	if (Buffer[8] == ' ')
		Buffer[8] = '0';
	return Buffer.data();
}

// FormatCRT71Time against CRT71Ctime32 for every hour of 32 bits and the second before it, and every 61st second
// of the last four days (where the UCRT's localtime adjusts the tm itself). Returns the differences; First: the
// first of them.
int CompareWithCtime32(std::string &First)
{
	int Differences = 0;
	const auto Compare = [&](std::int64_t Time) {
		const std::string Ours = Format(Time);
		const std::string Theirs = CRT71Ctime32(Time);
		if (Ours != Theirs && Differences++ == 0)
			First = std::to_string(Time) + ": " + Ours + " / " + Theirs;
	};
	for (std::int64_t Time = 0; Time <= INT32_MAX; Time += 3600) {
		Compare(Time);
		if (Time > 0)
			Compare(Time - 1);
	}
	for (std::int64_t Time = INT32_MAX - (4 * 86400); Time <= INT32_MAX; Time += 61)
		Compare(Time);
	Compare(INT32_MAX);
	return Differences;
}
#endif

} // namespace

TEST_CASE("crt71: the texts in UTC")
{
	const TTimeZone Zone("UTC0");
	for (const TExpected &Each : UTCTexts) {
		CAPTURE(Each.Time);
		CHECK(Format(Each.Time) == Each.Text);
	}
}

TEST_CASE("crt71: the day of the month in two digits")
{
	const TTimeZone Zone("UTC0");
	// 2026-01-01 12:00 UTC is a Thursday: every day of January
	constexpr std::int64_t January1 = 1767268800;
	constexpr std::array<std::string_view, 7> Days = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};
	for (int Day = 1; Day <= 31; Day++) {
		CAPTURE(Day);
		std::string Expected(Days.at(static_cast<std::size_t>((Day - 1) % 7)));
		Expected += " Jan ";
		Expected += static_cast<char>('0' + (Day / 10));
		Expected += static_cast<char>('0' + (Day % 10));
		Expected += " 12:00:00 2026\n";
		CHECK(Format(January1 + (std::int64_t{Day - 1} * 86400)) == Expected);
	}
	const std::string Text = Format(1793621686); // Mon Nov 02 12:14:46 2026
	CHECK(Text.size() == 25);
	CHECK(Text.substr(8, 2) == "02");
}

TEST_CASE("crt71: no text where MSVC 7.1's ctime gave none")
{
	const TTimeZone Zone("UTC0");
	TCRT71TimeText Text;
	CHECK(FormatCRT71Time(Text, 0) == Text.Chars);
	CHECK(FormatCRT71Time(Text, -1) == nullptr);
	CHECK(Format(INT32_MIN) == "(null)");
	CHECK(Format(-86400LL * 365) == "(null)");
	CHECK(Format(patella::compat::CRT71LastTime) == "Wed Dec 31 23:59:59 3000\n");
	CHECK(Format(patella::compat::CRT71LastTime + 1) == "(null)");
	CHECK(Format(INT64_MAX) == "(null)");
}

TEST_CASE("crt71: daylight saving time in a fixed zone")
{
	const TTimeZone Zone(PacificZone);
	for (const TExpected &Each : PacificTexts) {
		CAPTURE(Each.Time);
		CHECK(Format(Each.Time) == Each.Text);
	}
	// the first second of 1970 is still 1969 eight hours behind UTC
	CHECK(Format(0) == "Wed Dec 31 16:00:00 1969\n");
}

#if PATELLA_TEST_CHRONO_FORMAT
TEST_CASE("crt71: std::chrono's calendar in UTC")
{
	const TTimeZone Zone("UTC0");
	int Differences = 0;
	std::string First;
	const auto Compare = [&](std::int64_t Time) {
		const std::string Ours = Format(Time);
		const std::string Expected = ChronoText(std::chrono::sys_seconds(std::chrono::seconds(Time)));
		if (Ours != Expected && Differences++ == 0)
			First = std::to_string(Time) + ": " + Ours + " / " + Expected;
	};
	// a prime step from 1970 to 3000, then every day of 2024 to 2028, each a few seconds later
	for (std::int64_t Time = 0; Time <= patella::compat::CRT71LastTime; Time += 104'729)
		Compare(Time);
	for (std::int64_t Time = 1'704'067'200; Time < 1'861'920'000; Time += 86'400 + 7)
		Compare(Time);
	CAPTURE(First);
	CHECK(Differences == 0);
}

TEST_CASE("crt71: std::chrono's calendar around daylight saving time")
{
	const TTimeZone Zone(PacificZone);
	using namespace std::chrono;
	int Differences = 0;
	std::string First;
	for (int Number = 2007; Number <= 2037; Number++) {
		const year Year(Number);
		for (const sys_seconds Change : {sys_seconds(sys_days(Year / March / Sunday[2]) + hours(10)),
				 sys_seconds(sys_days(Year / November / Sunday[1]) + hours(9))}) {
			for (seconds Delta = hours(-26); Delta <= hours(26); Delta += minutes(30)) {
				for (const seconds Near : {Delta - seconds(1), Delta}) {
					const sys_seconds Time = Change + Near;
					const std::string Ours = Format(Time.time_since_epoch().count());
					const std::string Expected = PacificChronoText(Time);
					if (Ours != Expected && Differences++ == 0)
						First = std::to_string(Time.time_since_epoch().count()) + ": " + Ours + " / " + Expected;
				}
			}
		}
	}
	CAPTURE(First);
	CHECK(Differences == 0);
}
#endif

#if defined(_WIN32)
TEST_CASE("crt71: the UCRT's _ctime32 in MSVC 7.1's form, every hour of 32 bits")
{
	// UTC; the UCRT's rules of the United States; east of UTC with half an hour; the machine's own zone
	// (GetTimeZoneInformation's rules)
	for (const char *Each : {"UTC0", PacificZone, "IST-5:30", static_cast<const char *>(nullptr)}) {
		const TTimeZone Zone(Each);
		std::string First;
		const int Differences = CompareWithCtime32(First);
		const std::string Name = Each != nullptr ? Each : "the machine's zone";
		CAPTURE(Name);
		CAPTURE(First);
		CHECK(Differences == 0);
	}
	CHECK(CRT71Ctime32(-1) == "(null)");
	CHECK(Format(-1) == "(null)");
	// _ctime32 itself stops at 2038-01-18 23:59:59 UTC; MSVC 7.1's text goes on
	const TTimeZone Zone("UTC0");
	const __time32_t After = patella::compat::UCRTLastTime32 + 1;
	std::array<char, 26> Buffer = {};
	CHECK(_ctime32_s(Buffer.data(), Buffer.size(), &After) != 0);
	CHECK(Format(patella::compat::UCRTLastTime32) == "Mon Jan 18 23:59:59 2038\n");
	CHECK(Format(After) == "Tue Jan 19 00:00:00 2038\n");
}
#endif
