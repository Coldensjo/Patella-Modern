// The developer log's keys in Patella.ini (src/log/LogSettings.h, issue #77): LogLevel and LogFile, the defaults
// without them, and the warn lines for values that cannot be used.

#include <doctest/doctest.h>

#include "log/Log.h"
#include "log/LogSettings.h"
#include "settings/Settings.h"
#include "platform/FileSystem.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
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
using patella::settings::TSettingsFile;

namespace {

std::string UniqueName(const char *Name)
{
	std::random_device Random;
	return "patella_log_settings_test_" + std::to_string(Random()) + "_" + std::to_string(Random()) + "_" + Name;
}

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

TEST_CASE("Log settings: the defaults")
{
	const TCapture Console;
	TLog Log(Capture);
	patella::log::Configure(Log, TSettingsFile::Parse("# no log keys\nDevice=cpu\n"), Files);
	CHECK(Log.GetLevel() == TLevel::Info);
	CHECK_FALSE(Log.HasFile());
	CHECK(Console.Text().empty());
	patella::log::Configure(Log, TSettingsFile::Parse("LogFile=\nLogFile=ignored.log\n"), Files);
	CHECK_FALSE(Log.HasFile());
	CHECK(Console.Text().empty());
	CHECK_FALSE(std::filesystem::exists("ignored.log"));
}

TEST_CASE("Log settings: LogLevel")
{
	for (const TLevel Level : {TLevel::Error, TLevel::Warn, TLevel::Info, TLevel::Debug}) {
		const TCapture Console;
		TLog Log(Capture);
		patella::log::Configure(Log, TSettingsFile::Parse("loglevel = " + std::string(patella::log::LevelName(Level)) + "\r\n"), Files);
		CHECK(Log.GetLevel() == Level);
		CHECK(Console.Text() == (Level == TLevel::Debug ? "[debug] Patella.ini: level debug, no log file\n" : ""));
	}
	const TCapture Console;
	TLog Log(Capture);
	patella::log::Configure(Log, TSettingsFile::Parse("LogLevel=DEBUG\n"), Files);
	CHECK(Log.GetLevel() == TLevel::Debug);
	Log.SetLevel(TLevel::Warn);
	patella::log::Configure(Log, TSettingsFile::Parse("LogLevel=verbose\n"), Files);
	CHECK(Log.GetLevel() == TLevel::Warn);
	CHECK(Console.Text()
		== "[debug] Patella.ini: level debug, no log file\n"
		   "[warn] Patella.ini: LogLevel \"verbose\" is not error, warn, info or debug; warn kept\n");
	// At error the warning itself is not written.
	Log.SetLevel(TLevel::Error);
	patella::log::Configure(Log, TSettingsFile::Parse("LogLevel=\n"), Files);
	CHECK(Log.GetLevel() == TLevel::Error);
	CHECK(Console.Text()
		== "[debug] Patella.ini: level debug, no log file\n"
		   "[warn] Patella.ini: LogLevel \"verbose\" is not error, warn, info or debug; warn kept\n");
}

TEST_CASE("Log settings: LogFile")
{
	const std::string Path = (std::filesystem::temp_directory_path() / UniqueName("patella.log")).string();
	{
		const TCapture Console;
		TLog Log(Capture);
		patella::log::Configure(Log, TSettingsFile::Parse("LogLevel=debug\nLogFile=" + Path + "\n"), Files);
		CHECK(Log.HasFile());
		Log.Error("to both");
		const std::string Opened = "[debug] Patella.ini: level debug, log file " + Path + "\n";
		CHECK(Console.Text() == Opened + "[error] to both\n");
		CHECK(FileBytes(Path) == Opened + "[error] to both\n");
	}
	// A file that cannot be opened (a folder): a warning, no file.
	{
		const TCapture Console;
		TLog Log(Capture);
		const std::string Folder = std::filesystem::temp_directory_path().string();
		patella::log::Configure(Log, TSettingsFile::Parse("LogFile=" + Folder + "\n"), Files);
		CHECK_FALSE(Log.HasFile());
		CHECK(Console.Text() == "[warn] Patella.ini: LogFile \"" + Folder + "\" cannot be opened for appending; no log file\n");
	}
	std::error_code Error;
	std::filesystem::remove(Path, Error);
}

TEST_CASE("Log settings: LoadSettings")
{
	// Patella.ini in the working directory; without it the client's log keeps its defaults.
	const std::filesystem::path Saved = std::filesystem::current_path();
	const std::filesystem::path Folder = std::filesystem::temp_directory_path() / UniqueName("folder");
	std::filesystem::create_directory(Folder);
	std::filesystem::current_path(Folder);
	const TCapture Console;
	patella::log::Log().SetConsole(Capture);
	patella::log::LoadSettings(Files);
	CHECK(patella::log::Log().GetLevel() == TLevel::Info);
	CHECK_FALSE(patella::log::Log().HasFile());
	{
		std::ofstream Out(patella::settings::FileName, std::ios::binary);
		Out << "Device=cpu\r\nLogLevel=warn\r\nLogFile=dev.log\r\n";
	}
	patella::log::LoadSettings(Files);
	CHECK(patella::log::Log().GetLevel() == TLevel::Warn);
	CHECK(patella::log::Log().HasFile());
	patella::log::Warn("in the file");
	CHECK(FileBytes("dev.log") == "[warn] in the file\n");
	CHECK(Console.Text() == "[warn] in the file\n");
	// Back to the defaults for the other tests.
	patella::log::Log().CloseFile();
	patella::log::Log().SetLevel(patella::log::DefaultLevel);
	patella::log::Log().SetConsole(patella::log::WriteToStderr);
	std::filesystem::current_path(Saved);
	std::error_code Error;
	std::filesystem::remove_all(Folder, Error);
}
