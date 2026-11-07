// Settings (src/settings/Settings.cpp, issue #31): Patella.ini's parser and writer.

#include <doctest/doctest.h>

#include "settings/Settings.h"
#include "platform/FileSystem.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {
// The working folder's files: Load, Save, LoadSettings, Configure and OpenFile take the platform's IFileSystem
// (issue #119).
patella::platform::TSystemFileSystem Files; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): the tests' file system
} // namespace

using patella::settings::TSettingsFile;

TEST_CASE("Settings: keys, values and comments")
{
	TSettingsFile File = TSettingsFile::Parse("# Patella-Modern\n"
											  "; another comment\n"
											  "\n"
											  "Device=gpu1\n"
											  "  Scale \t=  2 \n"
											  "Empty=\n"
											  "Equals=a=b\n");
	CHECK(File.Get("Device") == "gpu1");
	CHECK(File.Get("device") == "gpu1");
	CHECK(File.Get("DEVICE") == "gpu1");
	CHECK(File.Get("Scale") == "2");
	CHECK(File.Get("Empty") == "");
	CHECK(File.Get("Equals") == "a=b");
	CHECK_FALSE(File.Get("Missing").has_value());
	CHECK_FALSE(File.Get("# Patella-Modern").has_value());
	CHECK(File.BadLines().empty());
	CHECK(File.Newline() == "\n");
}

TEST_CASE("Settings: the first line with a key counts")
{
	TSettingsFile File = TSettingsFile::Parse("Device=cpu\ndevice=gpu\n");
	CHECK(File.Get("Device") == "cpu");
	REQUIRE(File.Set("Device", "auto"));
	CHECK(File.Text() == "Device=auto\ndevice=gpu\n");
	CHECK(File.Get("DEVICE") == "auto");
}

TEST_CASE("Settings: round trip, unknown keys and comments kept")
{
	const char *Text = "# Patella-Modern settings\r\n"
					   "Future = something\r\n"
					   "\r\n"
					   "Device = cpu\r\n"
					   "  ; indented comment\r\n";
	TSettingsFile File = TSettingsFile::Parse(Text);
	CHECK(File.Text() == Text);
	CHECK(File.Newline() == "\r\n");
	REQUIRE(File.Set("Device", "gpu"));
	CHECK(File.Text()
		== "# Patella-Modern settings\r\n"
		   "Future = something\r\n"
		   "\r\n"
		   "Device=gpu\r\n"
		   "  ; indented comment\r\n");
	CHECK(File.Get("Future") == "something");
	// A second parse of the written text reads the same.
	TSettingsFile Again = TSettingsFile::Parse(File.Text());
	CHECK(Again.Text() == File.Text());
	CHECK(Again.Get("Device") == "gpu");
}

TEST_CASE("Settings: line ends")
{
	SUBCASE("CRLF from the first line")
	{
		TSettingsFile File = TSettingsFile::Parse("A=1\r\nB=2\r\n");
		CHECK(File.Get("A") == "1");
		CHECK(File.Get("B") == "2");
		REQUIRE(File.Set("C", "3"));
		CHECK(File.Text() == "A=1\r\nB=2\r\nC=3\r\n");
	}
	SUBCASE("LF, a stray CR is a blank")
	{
		TSettingsFile File = TSettingsFile::Parse("A=1\nB=2 \r\n");
		CHECK(File.Newline() == "\n");
		CHECK(File.Get("B") == "2");
		CHECK(File.Text() == "A=1\nB=2 \r\n");
	}
	SUBCASE("no line end at the end")
	{
		TSettingsFile File = TSettingsFile::Parse("A=1\r\nB=2");
		CHECK(File.Text() == "A=1\r\nB=2");
		REQUIRE(File.Set("B", "3"));
		CHECK(File.Text() == "A=1\r\nB=3");
		REQUIRE(File.Set("C", "4"));
		CHECK(File.Text() == "A=1\r\nB=3\r\nC=4\r\n");
	}
	SUBCASE("a CR at the end of a last line without a line end stays (issue #132)")
	{
		TSettingsFile File = TSettingsFile::Parse("A=1\r\nB=2\r");
		CHECK(File.Get("B") == "2");
		CHECK(File.Text() == "A=1\r\nB=2\r");
		CHECK(TSettingsFile::Parse(File.Text()).Text() == "A=1\r\nB=2\r");
		CHECK(TSettingsFile::Parse("A=1\r\nB=2\r\r").Text() == "A=1\r\nB=2\r\r");
		CHECK(TSettingsFile::Parse("A=1\r\n\r").Text() == "A=1\r\n\r");
	}
	SUBCASE("a single line without an end takes the default")
	{
		TSettingsFile File = TSettingsFile::Parse("A=1", "\r\n");
		CHECK(File.Newline() == "\r\n");
		REQUIRE(File.Set("B", "2"));
		CHECK(File.Text() == "A=1\r\nB=2\r\n");
	}
	SUBCASE("an empty file")
	{
		TSettingsFile File = TSettingsFile::Parse("", "\r\n");
		CHECK(File.Text().empty());
		REQUIRE(File.Set("Device", "auto"));
		CHECK(File.Text() == "Device=auto\r\n");
		TSettingsFile New;
		CHECK(New.Text().empty());
		REQUIRE(New.Set("Device", "cpu"));
		CHECK(New.Text() == "Device=cpu\n");
	}
}

TEST_CASE("Settings: bad lines are kept and never read")
{
	TSettingsFile File = TSettingsFile::Parse("just text\n"
											  "=no key\n"
											  "  = still none\n"
											  "Device=cpu\n"
											  "[section]\n");
	CHECK(File.BadLines() == std::vector<int>{1, 2, 3, 5});
	CHECK(File.Get("Device") == "cpu");
	CHECK_FALSE(File.Get("just text").has_value());
	CHECK_FALSE(File.Get("").has_value());
	CHECK_FALSE(File.Get("[section]").has_value());
	REQUIRE(File.Set("Device", "gpu"));
	CHECK(File.Text() == "just text\n=no key\n  = still none\nDevice=gpu\n[section]\n");
}

TEST_CASE("Settings: a byte order mark")
{
	TSettingsFile File = TSettingsFile::Parse("\xEF\xBB\xBF"
											  "Device=cpu\r\n");
	CHECK(File.Get("Device") == "cpu");
	CHECK(File.BadLines().empty());
	REQUIRE(File.Set("Device", "auto"));
	CHECK(File.Text()
		== "\xEF\xBB\xBF"
		   "Device=auto\r\n");
}

TEST_CASE("Settings: keys and values Set refuses")
{
	TSettingsFile File = TSettingsFile::Parse("Device=cpu\n");
	CHECK_FALSE(File.Set("", "x"));
	CHECK_FALSE(File.Set("A=B", "x"));
	CHECK_FALSE(File.Set("#A", "x"));
	CHECK_FALSE(File.Set(";A", "x"));
	CHECK_FALSE(File.Set(std::string("\xEF\xBB\xBF") + "A", "x")); // a byte order mark first (issue #132)
	CHECK_FALSE(File.Set(" A", "x"));
	CHECK_FALSE(File.Set("A\n", "x"));
	CHECK_FALSE(File.Set("Device", "a\nb"));
	CHECK_FALSE(File.Set("Device", "a\rb"));
	CHECK_FALSE(File.Set("Device", std::string("a\0b", 3)));
	CHECK_FALSE(File.Set("Device", " gpu"));
	CHECK_FALSE(File.Set("Device", "gpu\t"));
	CHECK(File.Text() == "Device=cpu\n");
	CHECK(File.Set("Device", ""));
	CHECK(File.Text() == "Device=\n");
	CHECK(File.Set("Path", "C:\\Tibia data\\x=y"));
	CHECK(File.Get("path") == "C:\\Tibia data\\x=y");
}

TEST_CASE("Settings: Load and Save")
{
	const std::string Path = "core_settings_test.ini";
	std::remove(Path.c_str());
	CHECK_FALSE(patella::settings::Load(Files, Path).has_value());
	{
		std::ofstream Out(Path, std::ios::binary);
		Out << "# kept\r\nDevice=cpu\r\nOther=1\r\n";
	}
	std::optional<TSettingsFile> File = patella::settings::Load(Files, Path);
	REQUIRE(File.has_value());
	CHECK(File->Get("Device") == "cpu");
	REQUIRE(File->Set("Device", "gpu2"));
	REQUIRE(patella::settings::Save(Files, Path, *File));
	std::ifstream In(Path, std::ios::binary);
	std::string Written((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	In.close();
	CHECK(Written == "# kept\r\nDevice=gpu2\r\nOther=1\r\n");
	// Too large to be a settings file.
	{
		std::ofstream Out(Path, std::ios::binary);
		std::string Line(1023, 'x');
		Line += '\n';
		for (int i = 0; i < 1025; i++)
			Out << Line;
	}
	CHECK_FALSE(patella::settings::Load(Files, Path).has_value());
	std::remove(Path.c_str());
	CHECK_FALSE(patella::settings::Save(Files, "no-such-directory/core_settings_test.ini", TSettingsFile()));
}
