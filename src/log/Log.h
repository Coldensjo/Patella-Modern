// Log.h: the developer log (Patella-Modern issue #77; plan D16). Not part of the original source.
//
// A small log of our own, no library: four levels, each line written to stderr (through TConsole: the unit tests
// capture the lines there) and, when one is open, to a log file, as "[<level>] <message>\n" (a message may hold line breaks of its own). Nothing is sent anywhere.
//
//	patella::log::Error("Debug Assertion 7.72 {}", Message);   // std::format's language
//	if (patella::log::Enabled(patella::log::TLevel::Debug)) ... // work done only for a debug line
//
// The levels, most severe first: error (the fatal path: FatalError's box text, HandleException's message), warn
// (a setting that could not be used), info (the default level) and debug (the diagnostics error.txt held until issue
// #74: the engine, the last packet types, the last packet's first bytes and the player's position). A line is
// written when its level is the log's level or a more severe one. The level and the file come from Patella.ini
// (log/LogSettings.h, the x64 client: LogLevel, LogFile); without them, or where nothing reads them (the tests),
// the log is at info and writes to stderr only.
//
// The text comes from std::format where the library has it (PATELLA_HAS_FORMAT), elsewhere (a C++17 library and
// tests_unit_cxx26_standin) from compat/format.h's stand-in, as compat/print.h and TFile::Print do. Nothing here
// throws: a message that cannot be formatted (the stand-in's TFormatError, std::bad_alloc) is dropped, and a failed
// write is ignored (a GUI client's stderr is valid only when it is redirected). The file is opened for appending, in
// binary ('\n' line ends on every platform); stderr's own mode does the rest (the Microsoft C runtime writes "\r\n").
// Each line is flushed at once: the next thing the fatal path does may be the end of the process.
//
// The messages are the client's text, Windows-1252 (plan D4); the lines go out in UTF-8 (issue #112,
// types/Cp1252.h), to stderr and to the file alike, so that a terminal, an editor and M8's Linux console show them as
// they are. An ASCII message is written unchanged. LogFile's name is the client's text too (TFile::Open).
//
// The fatal path's extras for Debug builds (Player.cpp FatalError), here so that every compiler tests them:
// CurrentStacktrace (std::stacktrace's text where the standard library has it: MSVC's, and libstdc++'s with
// libstdc++exp, GCC 16 and Clang 23 on Linux; nothing on mingw-w64's GCC 16 and on the oracle's C++17), and
// BreakIfDebugged (a breakpoint when a debugger is attached: IsDebuggerPresent and DebugBreak on Windows, the
// TracerPid of /proc/self/status and SIGTRAP on Linux; nothing elsewhere).

#pragma once

#include "compat/format.h"
#include "platform/FileSystem.h"
#include "types/Cp1252.h"
#include "types/File.h"
#include "types/FixedString.h" // IWYU pragma: keep (std::format's branch: TFormatArgument)

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility> // IWYU pragma: keep (std::format's branch below)
#include <version> // IWYU pragma: keep (the feature-test macro of the #if line)

#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#define PATELLA_HAS_STACKTRACE 1
#include <stacktrace>
#else
#define PATELLA_HAS_STACKTRACE 0
#endif

#if defined(_WIN32)
// kernel32's, as <windows.h> declares them (BOOL is int), without the header.
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void); // NOLINT(readability-redundant-declaration,modernize-redundant-void-arg)
extern "C" __declspec(dllimport) void __stdcall DebugBreak(void);       // NOLINT(readability-redundant-declaration,modernize-redundant-void-arg)
#elif defined(__linux__)
#include <csignal>
#include <fstream>
#endif

namespace patella::log {

enum class TLevel : unsigned char {
	Error,
	Warn,
	Info,
	Debug,
};

inline constexpr TLevel DefaultLevel = TLevel::Info;

// "error", "warn", "info", "debug": the names in the lines and in Patella.ini's LogLevel.
constexpr std::string_view LevelName(TLevel Level) noexcept
{
	switch (Level) {
	case TLevel::Error:
		return "error";
	case TLevel::Warn:
		return "warn";
	case TLevel::Info:
		return "info";
	case TLevel::Debug:
		return "debug";
	}
	return "?";
}

// The level of a name of LevelName, without regard to ASCII case; nothing for any other text.
inline std::optional<TLevel> ParseLevel(std::string_view Name) noexcept
{
	for (const TLevel Level : {TLevel::Error, TLevel::Warn, TLevel::Info, TLevel::Debug}) {
		const std::string_view Expected = LevelName(Level);
		if (Name.size() != Expected.size())
			continue;
		bool Same = true;
		for (std::size_t i = 0; i < Name.size() && Same; i++) {
			const char c = Name[i];
			Same = (c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c) == Expected[i];
		}
		if (Same)
			return Level;
	}
	return std::nullopt;
}

// The line written for a message (the client's text, Windows-1252): "[<level>] <message>\n", in UTF-8 (issue #112).
inline std::string FormatLine(TLevel Level, std::string_view Message)
{
	const std::string_view Name = LevelName(Level);
	std::string Line;
	Line.reserve(Name.size() + Message.size() + 4);
	Line += '[';
	Line += Name;
	Line += "] ";
	Line += patella::text::Cp1252ToUtf8(Message);
	Line += '\n';
	return Line;
}

// Where a log writes its lines besides the file: a function given each whole line (WriteToStderr by default; the
// unit tests capture them), or nullptr for nowhere.
using TConsole = void (*)(std::string_view Line) noexcept;

// The C library's stderr: the line in one write, then flushed.
inline void WriteToStderr(std::string_view Line) noexcept
{
	std::fwrite(Line.data(), 1, Line.size(), stderr);
	std::fflush(stderr);
}

class TLog {
public:
	// A log at DefaultLevel that writes to Console and no file.
	explicit TLog(TConsole Target = WriteToStderr) noexcept : Console(Target) {}
	TLog(const TLog &) = delete;
	TLog &operator=(const TLog &) = delete;
	TLog(TLog &&) = delete;
	TLog &operator=(TLog &&) = delete;
	~TLog() = default;

	[[nodiscard]] TLevel GetLevel() const noexcept { return Level.load(std::memory_order_relaxed); }
	void SetLevel(TLevel NewLevel) noexcept { Level.store(NewLevel, std::memory_order_relaxed); }
	// Whether a line of this level is written.
	[[nodiscard]] bool Enabled(TLevel LineLevel) const noexcept { return LineLevel <= GetLevel(); }

	void SetConsole(TConsole Target) noexcept
	{
		const std::lock_guard<std::mutex> Lock(Mutex);
		Console = Target;
	}

	// Opens Path for appending (created where it does not exist) through Files (the platform's IFileSystem, issue
	// #119), instead of an open one. False (and no file) where that fails.
	bool OpenFile(patella::platform::IFileSystem &Files, const std::string &Path) noexcept
	{
		const std::lock_guard<std::mutex> Lock(Mutex);
		File.Close();
		try {
			return Files.Open(File, Path.c_str(), TFile::TAccess::Append, TFile::TContent::Binary);
		} catch (...) { // an implementation's std::bad_alloc (TFolderFileSystem's list)
			File.Close();
			return false;
		}
	}

	void CloseFile() noexcept
	{
		const std::lock_guard<std::mutex> Lock(Mutex);
		File.Close();
	}

	[[nodiscard]] bool HasFile() noexcept
	{
		const std::lock_guard<std::mutex> Lock(Mutex);
		return File.IsOpen();
	}

	// Writes the message's line (FormatLine) when its level is enabled.
	void Write(TLevel LineLevel, std::string_view Message) noexcept
	{
		if (!Enabled(LineLevel))
			return;
		try {
			const std::string Line = FormatLine(LineLevel, Message);
			const std::lock_guard<std::mutex> Lock(Mutex);
			if (Console != nullptr)
				Console(Line);
			if (File.IsOpen()) {
				File.Write(Line);
				File.Flush();
			}
		} catch (...) { // NOLINT(bugprone-empty-catch): std::bad_alloc or std::system_error of the mutex; dropped
		}
	}

#if PATELLA_HAS_FORMAT
	template <typename... Arguments>
	void Print(TLevel LineLevel, std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
	{
		if (!Enabled(LineLevel))
			return;
		try {
			Write(LineLevel, std::format(Format, PassFormatArgument(std::forward<Arguments>(Values))...));
		} catch (...) { // NOLINT(bugprone-empty-catch): std::bad_alloc (std::format_error is a compile-time error here)
		}
	}

	template <typename... Arguments>
	void Error(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
	{
		Print(TLevel::Error, Format, std::forward<Arguments>(Values)...);
	}
	template <typename... Arguments>
	void Warn(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
	{
		Print(TLevel::Warn, Format, std::forward<Arguments>(Values)...);
	}
	template <typename... Arguments>
	void Info(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
	{
		Print(TLevel::Info, Format, std::forward<Arguments>(Values)...);
	}
	template <typename... Arguments>
	void Debug(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
	{
		Print(TLevel::Debug, Format, std::forward<Arguments>(Values)...);
	}
#else
	template <typename... Arguments>
	void Print(TLevel LineLevel, std::string_view Format, const Arguments &...Values) noexcept
	{
		if (!Enabled(LineLevel))
			return;
		try {
			const std::ptrdiff_t Size = patella::compat::FormatToNStandIn(nullptr, 0, Format, Values...).Size;
			std::string Text(static_cast<std::size_t>(Size), '\0');
			patella::compat::FormatToNStandIn(Text.data(), Size, Format, Values...);
			Write(LineLevel, Text);
		} catch (...) { // NOLINT(bugprone-empty-catch): the stand-in's TFormatError or std::bad_alloc; dropped
		}
	}

	template <typename... Arguments>
	void Error(std::string_view Format, const Arguments &...Values) noexcept
	{
		Print(TLevel::Error, Format, Values...);
	}
	template <typename... Arguments>
	void Warn(std::string_view Format, const Arguments &...Values) noexcept
	{
		Print(TLevel::Warn, Format, Values...);
	}
	template <typename... Arguments>
	void Info(std::string_view Format, const Arguments &...Values) noexcept
	{
		Print(TLevel::Info, Format, Values...);
	}
	template <typename... Arguments>
	void Debug(std::string_view Format, const Arguments &...Values) noexcept
	{
		Print(TLevel::Debug, Format, Values...);
	}
#endif

private:
	std::mutex Mutex;
	std::atomic<TLevel> Level{DefaultLevel};
	TConsole Console;
	TFile File;
};

// The client's log.
inline TLog &Log() noexcept
{
	static TLog Instance;
	return Instance;
}

inline bool Enabled(TLevel Level) noexcept
{
	return Log().Enabled(Level);
}

#if PATELLA_HAS_FORMAT
template <typename... Arguments>
void Error(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
{
	Log().Print(TLevel::Error, Format, std::forward<Arguments>(Values)...);
}
template <typename... Arguments>
void Warn(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
{
	Log().Print(TLevel::Warn, Format, std::forward<Arguments>(Values)...);
}
template <typename... Arguments>
void Info(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
{
	Log().Print(TLevel::Info, Format, std::forward<Arguments>(Values)...);
}
template <typename... Arguments>
void Debug(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values) noexcept
{
	Log().Print(TLevel::Debug, Format, std::forward<Arguments>(Values)...);
}
#else
template <typename... Arguments>
void Error(std::string_view Format, const Arguments &...Values) noexcept
{
	Log().Print(TLevel::Error, Format, Values...);
}
template <typename... Arguments>
void Warn(std::string_view Format, const Arguments &...Values) noexcept
{
	Log().Print(TLevel::Warn, Format, Values...);
}
template <typename... Arguments>
void Info(std::string_view Format, const Arguments &...Values) noexcept
{
	Log().Print(TLevel::Info, Format, Values...);
}
template <typename... Arguments>
void Debug(std::string_view Format, const Arguments &...Values) noexcept
{
	Log().Print(TLevel::Debug, Format, Values...);
}
#endif

// The stack of the caller (Skip frames of it left out: 0, the caller's own frame first), as std::stacktrace's
// to_string writes it; nothing where the standard library has no std::stacktrace or the text cannot be made.
inline std::optional<std::string> CurrentStacktrace([[maybe_unused]] std::size_t Skip = 0) noexcept
{
#if PATELLA_HAS_STACKTRACE
	try {
		// +1: this function's own frame.
		return std::to_string(std::stacktrace::current(static_cast<std::stacktrace::size_type>(Skip + 1)));
	} catch (...) { // NOLINT(bugprone-empty-catch): std::bad_alloc; no trace
	}
#endif
	return std::nullopt;
}

// Writes the caller's stack (CurrentStacktrace) as one error line, or a line that says there is none.
inline void WriteStacktrace(TLog &Target, std::size_t Skip = 0) noexcept
{
	const std::optional<std::string> Trace = CurrentStacktrace(Skip + 1);
	if (!Trace.has_value()) {
		Target.Write(TLevel::Error, "Stack trace: not available (the standard library of this build has no std::stacktrace)");
		return;
	}
	try {
		Target.Write(TLevel::Error, "Stack trace:\n" + *Trace);
	} catch (...) { // NOLINT(bugprone-empty-catch): std::bad_alloc; no trace
	}
}

// Whether a debugger is attached to the process (false where that cannot be told).
inline bool DebuggerAttached() noexcept
{
#if defined(_WIN32)
	return IsDebuggerPresent() != 0;
#elif defined(__linux__)
	try {
		std::ifstream Status("/proc/self/status");
		std::string Line;
		while (std::getline(Status, Line)) {
			constexpr std::string_view Key = "TracerPid:";
			if (Line.compare(0, Key.size(), Key) != 0)
				continue;
			const std::size_t Digit = Line.find_first_not_of(" \t", Key.size());
			return Digit != std::string::npos && Line[Digit] != '0';
		}
	} catch (...) { // NOLINT(bugprone-empty-catch): not readable; no debugger known
	}
	return false;
#else
	return false;
#endif
}

// Stops in the attached debugger, if there is one (true); without a debugger nothing happens (false).
inline bool BreakIfDebugged() noexcept
{
	if (!DebuggerAttached())
		return false;
#if defined(_WIN32)
	DebugBreak();
#elif defined(__linux__)
	std::raise(SIGTRAP); // NOLINT(misc-include-cleaner): <csignal> provides it (glibc defines it in a bits header)
#endif
	return true;
}

} // namespace patella::log
