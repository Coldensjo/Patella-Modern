// The developer log of src/log/Log.h (issue #77): the levels and their names, the lines, the console and the file,
// the formats, and the fatal path's Debug extras (the stack trace, the debugger break). tests_unit runs them with
// std::format, tests_unit_cxx26_standin (PATELLA_FORMAT_STANDIN) with the stand-in the oracle's C++17 build uses.

#include <doctest/doctest.h>

#include "log/Log.h"
#include "types/FixedString.h"
#include "platform/FileSystem.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace {
// The working folder's files: Load, Save, LoadSettings, Configure and OpenFile take the platform's IFileSystem
// (issue #119).
patella::platform::TSystemFileSystem Files; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): the tests' file system
} // namespace

using patella::log::TLevel;
using patella::log::TLog;

namespace {

// A file name of its own in the temporary folder (other sessions' tests share it: issue #66), removed at the end.
struct TTemporaryPath {
	explicit TTemporaryPath(const char *Name)
	{
		std::random_device Random;
		const std::string Unique = std::to_string(Random()) + "_" + std::to_string(Random());
		Path = (std::filesystem::temp_directory_path() / ("patella_log_test_" + Unique + "_" + Name)).string();
	}
	~TTemporaryPath()
	{
		std::error_code Error;
		std::filesystem::remove(Path, Error);
	}
	TTemporaryPath(const TTemporaryPath &) = delete;
	TTemporaryPath &operator=(const TTemporaryPath &) = delete;
	std::string Path;
};

std::string FileBytes(const std::string &Path)
{
	std::ifstream In(Path, std::ios::binary);
	return {std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>()};
}

// The lines a log gives its console (TConsole), captured; a TCapture starts the capture afresh.
std::string Captured;
void Capture(std::string_view Line) noexcept
{
	try {
		Captured.append(Line);
	} catch (...) { // NOLINT(bugprone-empty-catch): std::bad_alloc; the test's comparison fails
	}
}
struct TCapture {
	TCapture() { Captured.clear(); }
	[[nodiscard]] static std::string Text() { return Captured; }
};

} // namespace

TEST_CASE("Log: the level names")
{
	CHECK(patella::log::LevelName(TLevel::Error) == "error");
	CHECK(patella::log::LevelName(TLevel::Warn) == "warn");
	CHECK(patella::log::LevelName(TLevel::Info) == "info");
	CHECK(patella::log::LevelName(TLevel::Debug) == "debug");
	for (const TLevel Level : {TLevel::Error, TLevel::Warn, TLevel::Info, TLevel::Debug})
		CHECK(patella::log::ParseLevel(patella::log::LevelName(Level)) == Level);
	CHECK(patella::log::ParseLevel("DEBUG") == TLevel::Debug);
	CHECK(patella::log::ParseLevel("Warn") == TLevel::Warn);
	CHECK(patella::log::ParseLevel("eRRoR") == TLevel::Error);
	CHECK_FALSE(patella::log::ParseLevel("").has_value());
	CHECK_FALSE(patella::log::ParseLevel("warning").has_value());
	CHECK_FALSE(patella::log::ParseLevel("inf").has_value());
	CHECK_FALSE(patella::log::ParseLevel(" info").has_value());
	CHECK_FALSE(patella::log::ParseLevel("trace").has_value());
	CHECK(patella::log::DefaultLevel == TLevel::Info);
}

TEST_CASE("Log: the lines")
{
	CHECK(patella::log::FormatLine(TLevel::Error, "Tibia Error: text") == "[error] Tibia Error: text\n");
	CHECK(patella::log::FormatLine(TLevel::Warn, "") == "[warn] \n");
	CHECK(patella::log::FormatLine(TLevel::Debug, "a\r\n\r\nb") == "[debug] a\r\n\r\nb\n");
	// The message is the client's text (Windows-1252), the line UTF-8 (issue #112).
	CHECK(patella::log::FormatLine(TLevel::Info, "J\xf6rg: \x80 5") == "[info] J\xc3\xb6rg: \xe2\x82\xac 5\n");
}

TEST_CASE("Log: the lines in the file and on the console are UTF-8 (issue #112)")
{
	const TTemporaryPath Path("patella_utf8.log");
	const TCapture Console;
	{
		TLog Log(Capture);
		REQUIRE(Log.OpenFile(Files, Path.Path));
		Log.Info("{} \xe4\xdf", "J\xf6rg");
	}
	CHECK(FileBytes(Path.Path) == "[info] J\xc3\xb6rg \xc3\xa4\xc3\x9f\n");
	CHECK(Console.Text() == "[info] J\xc3\xb6rg \xc3\xa4\xc3\x9f\n");
}

TEST_CASE("Log: the levels written")
{
	const TCapture Console;
	TLog Log(Capture);
	CHECK(Log.GetLevel() == TLevel::Info);
	CHECK(Log.Enabled(TLevel::Error));
	CHECK(Log.Enabled(TLevel::Warn));
	CHECK(Log.Enabled(TLevel::Info));
	CHECK_FALSE(Log.Enabled(TLevel::Debug));
	Log.Write(TLevel::Error, "e1");
	Log.Write(TLevel::Warn, "w1");
	Log.Write(TLevel::Info, "i1");
	Log.Write(TLevel::Debug, "d1");
	CHECK(Console.Text() == "[error] e1\n[warn] w1\n[info] i1\n");

	Log.SetLevel(TLevel::Debug);
	Log.Write(TLevel::Debug, "d2");
	Log.SetLevel(TLevel::Error);
	CHECK_FALSE(Log.Enabled(TLevel::Warn));
	Log.Write(TLevel::Warn, "w2");
	Log.Write(TLevel::Info, "i2");
	Log.Write(TLevel::Error, "e2");
	Log.SetLevel(TLevel::Warn);
	Log.Write(TLevel::Warn, "w3");
	Log.Write(TLevel::Info, "i3");
	CHECK(Console.Text() == "[error] e1\n[warn] w1\n[info] i1\n[debug] d2\n[error] e2\n[warn] w3\n");

	// No console: nothing written, nothing fails.
	Log.SetConsole(nullptr);
	Log.Write(TLevel::Error, "nowhere");
	CHECK(Console.Text() == "[error] e1\n[warn] w1\n[info] i1\n[debug] d2\n[error] e2\n[warn] w3\n");
}

TEST_CASE("Log: the formats")
{
	const TCapture Console;
	TLog Log(Capture);
	Log.SetLevel(TLevel::Debug);
	Log.Error("{}: {}", "Tibia Error", "The Tibia client encountered a critical error and had to be stopped.");
	Log.Warn("{} \"{}\" {}", "LogLevel", std::string("loud"), 7);
	Log.Info("{:03}|{:>4}|{:x}", 5U, "ab", 255);
	TFixedString<8> Fixed;
	Fixed.Format("{}", "fixed");
	Log.Debug("Player position: [{},{},{}] {}", 32000, 31000, 7, Fixed);
	Log.Print(TLevel::Info, "{{}} {}", true);
	CHECK(Console.Text()
		== "[error] Tibia Error: The Tibia client encountered a critical error and had to be stopped.\n"
		   "[warn] LogLevel \"loud\" 7\n"
		   "[info] 005|  ab|ff\n"
		   "[debug] Player position: [32000,31000,7] fixed\n"
		   "[info] {} true\n");
	// Below the level: not formatted, not written.
	Log.SetLevel(TLevel::Warn);
	Log.Info("{}", "hidden");
	Log.Debug("{}", "hidden");
	CHECK(Console.Text().find("hidden") == std::string::npos);
#if !PATELLA_HAS_FORMAT
	// The stand-in throws for a format it does not know (std::format rejects it at compile time): the line is
	// dropped, nothing escapes.
	Log.Error("{:b}", 5);
	CHECK(Console.Text().find("[error] 101") == std::string::npos);
	Log.Error("after");
	CHECK(Console.Text().ends_with("[error] after\n"));
#endif
}

TEST_CASE("Log: the file")
{
	const TTemporaryPath Path("patella.log");
	const TCapture Console;
	{
		TLog Log(Capture);
		CHECK_FALSE(Log.HasFile());
		Log.Info("before the file");
		REQUIRE(Log.OpenFile(Files, Path.Path));
		CHECK(Log.HasFile());
		Log.Info("one");
		Log.Debug("not at info");
		Log.Error("two");
		// Each line is on the disk at once (the fatal path may end the process next).
		CHECK(FileBytes(Path.Path) == "[info] one\n[error] two\n");
		Log.CloseFile();
		CHECK_FALSE(Log.HasFile());
		Log.Info("after the file");
	}
	CHECK(Console.Text() == "[info] before the file\n[info] one\n[error] two\n[info] after the file\n");
	// Opened again: appended, never emptied.
	{
		TLog Log(nullptr);
		REQUIRE(Log.OpenFile(Files, Path.Path));
		Log.Warn("three");
	}
	CHECK(FileBytes(Path.Path) == "[info] one\n[error] two\n[warn] three\n");
	// A file that cannot be opened (a folder): false, no file, the console still written.
	{
		const TCapture Second;
		TLog Log(Capture);
		REQUIRE(Log.OpenFile(Files, Path.Path));
		CHECK_FALSE(Log.OpenFile(Files, std::filesystem::temp_directory_path().string()));
		CHECK_FALSE(Log.HasFile());
		Log.Error("console only");
		CHECK(Second.Text() == "[error] console only\n");
	}
	CHECK(FileBytes(Path.Path) == "[info] one\n[error] two\n[warn] three\n");
}

TEST_CASE("Log: the client's log")
{
	// One log for the program, at the defaults until Patella.ini sets it (log/LogSettings.h).
	CHECK(&patella::log::Log() == &patella::log::Log());
	CHECK(patella::log::Log().GetLevel() == TLevel::Info);
	CHECK(patella::log::Enabled(TLevel::Info));
	CHECK_FALSE(patella::log::Enabled(TLevel::Debug));
	const TCapture Console;
	patella::log::Log().SetConsole(Capture);
	patella::log::Error("e {}", 1);
	patella::log::Warn("w {}", 2);
	patella::log::Info("i {}", 3);
	patella::log::Debug("d {}", 4);
	patella::log::Log().SetConsole(patella::log::WriteToStderr);
	CHECK(Console.Text() == "[error] e 1\n[warn] w 2\n[info] i 3\n");
}

TEST_CASE("Log: the stack trace")
{
	// Where the standard library has std::stacktrace: MSVC's, and libstdc++'s on Linux (GCC 16, and Clang 22 with
	// libstdc++), with libstdc++exp; not mingw-w64's GCC 16, which has no backtrace for Windows.
#if defined(_MSC_VER) || (defined(__linux__) && defined(__GLIBCXX__))
	CHECK(PATELLA_HAS_STACKTRACE == 1);
#endif
	const std::optional<std::string> Trace = patella::log::CurrentStacktrace();
	CHECK(Trace.has_value() == (PATELLA_HAS_STACKTRACE == 1));
	if (Trace.has_value())
		CHECK_FALSE(Trace->empty());

	const TCapture Console;
	TLog Log(Capture);
	patella::log::WriteStacktrace(Log);
	const std::string Text = Console.Text();
#if PATELLA_HAS_STACKTRACE
	CHECK(Text.starts_with("[error] Stack trace:\n"));
	CHECK(Text.size() > std::string("[error] Stack trace:\n\n").size());
#else
	CHECK(Text == "[error] Stack trace: not available (the standard library of this build has no std::stacktrace)\n");
#endif
	CHECK(Text.ends_with("\n"));
	// At the error level, so always written.
	Log.SetLevel(TLevel::Error);
	patella::log::WriteStacktrace(Log);
	CHECK(Console.Text().size() > Text.size());
}

TEST_CASE("Log: the debugger break")
{
	// The tests run without a debugger (under one, the break stops there once and the test goes on).
	if (!patella::log::DebuggerAttached())
		CHECK_FALSE(patella::log::BreakIfDebugged());
	else
		CHECK(patella::log::BreakIfDebugged());
}
