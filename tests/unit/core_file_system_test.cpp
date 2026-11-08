// The platform's file system (src/platform/FileSystem.h, issue #109): TSystemFileSystem opens the names of the working
// folder as TFile::Open does and tells whether a file is there as std::filesystem::exists does; TFolderFileSystem, the
// test implementation, opens them in its folder, lists the opens and leaves out the names it is told are missing.
// Issue #130: TDataFileSystem (the data and settings folders, FindNoCase: mixed-case names on Linux) and
// settings::ChooseLocations (where they are).

#include <doctest/doctest.h>

#include "platform/FileSystem.h"
#include "settings/Settings.h"
#include "types/File.h"

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <random>
#include <string>
#include <system_error>
#include <vector>

namespace {

using patella::platform::IFileSystem;
using patella::platform::TFolderFileSystem;
using patella::platform::TSystemFileSystem;

// A folder of its own in the temporary folder (other sessions' tests share it: issue #66), removed at the end. Its
// name is ASCII, so it is the same in the client's text.
struct TTemporaryFolder {
	TTemporaryFolder()
	{
		std::random_device Random;
		Path = std::filesystem::temp_directory_path()
			/ ("patella_file_system_test_" + std::to_string(Random()) + "_" + std::to_string(Random()));
		std::filesystem::create_directories(Path);
	}
	~TTemporaryFolder()
	{
		std::error_code Error;
		std::filesystem::remove_all(Path, Error);
	}
	TTemporaryFolder(const TTemporaryFolder &) = delete;
	TTemporaryFolder &operator=(const TTemporaryFolder &) = delete;
	TTemporaryFolder(TTemporaryFolder &&) = delete;
	TTemporaryFolder &operator=(TTemporaryFolder &&) = delete;

	std::filesystem::path Path;
};

std::string ReadAll(const std::filesystem::path &Path)
{
	std::ifstream In(Path, std::ios::binary);
	return {std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>()};
}

void WriteAll(const std::filesystem::path &Path, const std::string &Text)
{
	std::ofstream Out(Path, std::ios::binary);
	Out << Text;
}

} // namespace

TEST_CASE("TSystemFileSystem: Open is TFile::Open, Exists is std::filesystem::exists")
{
	const TTemporaryFolder Folder;
	const std::string Name = (Folder.Path / "system.dat").string();
	TSystemFileSystem System;
	IFileSystem &Files = System;

	CHECK_FALSE(Files.Exists(Name.c_str()));
	TFile Missing;
	errno = 0;
	CHECK_FALSE(Files.Open(Missing, Name.c_str(), TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(errno == ENOENT);
	CHECK_FALSE(Missing.IsOpen());

	TFile Written;
	REQUIRE(Files.Open(Written, Name.c_str(), TFile::TAccess::Write, TFile::TContent::Text));
	CHECK(Written.Write("a\nb"));
	CHECK(Written.Close() == 0);
	CHECK(Files.Exists(Name.c_str()));
	CHECK(ReadAll(Name) == "a\r\nb");

	TFile Read;
	REQUIRE(Files.Open(Read, Name.c_str(), TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(Read.GetChar() == 'a');
}

TEST_CASE("TFolderFileSystem: the names are its folder's, every open is listed")
{
	const TTemporaryFolder Folder;
	WriteAll(Folder.Path / "Tibia.cfg", "Data");
	TFolderFileSystem Test(Folder.Path.string());
	IFileSystem &Files = Test;

	CHECK(Files.Exists("Tibia.cfg"));
	CHECK_FALSE(Files.Exists("Tibia.dat"));

	TFile Read;
	REQUIRE(Files.Open(Read, "Tibia.cfg", TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(Read.GetChar() == 'D');
	CHECK(Read.Close() == 0);

	TFile Appended;
	REQUIRE(Files.Open(Appended, "channel.txt", TFile::TAccess::Append, TFile::TContent::Text));
	CHECK(Appended.Write("line\n"));
	CHECK(Appended.Close() == 0);
	CHECK(ReadAll(Folder.Path / "channel.txt") == "line\r\n");

	REQUIRE(Test.Opened.size() == 2);
	CHECK(Test.Opened[0].Name == "Tibia.cfg");
	CHECK(Test.Opened[0].Access == TFile::TAccess::Read);
	CHECK(Test.Opened[1].Name == "channel.txt");
	CHECK(Test.Opened[1].Access == TFile::TAccess::Append);
	CHECK(Test.Opened[1].Content == TFile::TContent::Text);
}

TEST_CASE("TFolderFileSystem: a missing name is not there, whatever the folder has")
{
	const TTemporaryFolder Folder;
	WriteAll(Folder.Path / "Tibia.spr", "Data");
	TFolderFileSystem Test(Folder.Path.string());
	Test.Missing.emplace_back("Tibia.spr");

	CHECK_FALSE(Test.Exists("Tibia.spr"));
	TFile File;
	errno = 0;
	CHECK_FALSE(Test.Open(File, "Tibia.spr", TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(errno == ENOENT);
	CHECK_FALSE(File.IsOpen());
	CHECK(Test.Opened.size() == std::size_t{1});
}

// --- Issue #130: the data folder, the settings folder, the lookup of the original names without regard to case -------

namespace {

using patella::platform::FindNoCase;
using patella::platform::TDataFileSystem;
using patella::platform::TFileLocations;

bool Same(const std::filesystem::path &Left, const std::filesystem::path &Right)
{
	std::error_code Error;
	return std::filesystem::equivalent(Left, Right, Error);
}

// A folder of the client's files in mixed case, as an unpacked Linux copy may have them.
struct TMixedCaseFolder : TTemporaryFolder {
	TMixedCaseFolder()
	{
		WriteAll(Path / "Tibia.dat", "dat");
		std::filesystem::create_directories(Path / "AutoMap");
		WriteAll(Path / "AutoMap" / "12312307.MAP", "map");
	}
};

} // namespace

TEST_CASE("SameNameNoCase: ASCII letters only")
{
	using patella::platform::SameNameNoCase;
	CHECK(SameNameNoCase<char>("Tibia.dat", "TIBIA.DAT"));
	CHECK(SameNameNoCase<char>("", ""));
	CHECK_FALSE(SameNameNoCase<char>("Tibia.dat", "Tibia.da"));
	CHECK_FALSE(SameNameNoCase<char>("Tibia.dat", "Tibia_dat"));
	CHECK_FALSE(SameNameNoCase<char>("[", "{"));               // '[' + 32 is '{': not letters
	CHECK_FALSE(SameNameNoCase<char>("\xC3\xA9", "\xC3\x89")); // e and E with acute in UTF-8: compared as they are
	CHECK(SameNameNoCase<wchar_t>(L"Patella.ini", L"patella.INI"));
}

TEST_CASE("FindNoCase: each part of the name as it is on the disk")
{
	const TMixedCaseFolder Folder;
	// The file as written, and in another case: the same file on both platforms.
	CHECK(Same(FindNoCase(Folder.Path, "Tibia.dat"), Folder.Path / "Tibia.dat"));
	CHECK(Same(FindNoCase(Folder.Path, "tibia.DAT"), Folder.Path / "Tibia.dat"));
	CHECK(Same(FindNoCase(Folder.Path, "automap/12312307.map"), Folder.Path / "AutoMap" / "12312307.MAP"));
	// Not there: as written, in the folder found (a file to create opens there).
	CHECK(FindNoCase(Folder.Path, "Tibia.spr") == Folder.Path / "Tibia.spr");
	const std::filesystem::path New = FindNoCase(Folder.Path, "AUTOMAP/12312308.map");
	CHECK(New.filename() == "12312308.map");
	CHECK(Same(New.parent_path(), Folder.Path / "AutoMap"));
	CHECK(FindNoCase(Folder.Path, "none/Tibia.dat") == Folder.Path / "none" / "Tibia.dat");
	// An absolute name is looked up from its root.
	CHECK(Same(FindNoCase("unused", Folder.Path / "TIBIA.dat"), Folder.Path / "Tibia.dat"));
#if !defined(_WIN32)
	// Linux: the disk's spelling, which the exact name would not open.
	CHECK(FindNoCase(Folder.Path, "tibia.DAT") == Folder.Path / "Tibia.dat");
	CHECK(FindNoCase(Folder.Path, "automap/12312307.map") == Folder.Path / "AutoMap" / "12312307.MAP");
	CHECK(FindNoCase(Folder.Path, "AUTOMAP/12312308.map") == Folder.Path / "AutoMap" / "12312308.map");
#endif
}

#if !defined(_WIN32)
TEST_CASE("FindNoCase (Linux): the exact name first, else the first in name order")
{
	const TTemporaryFolder Folder;
	WriteAll(Folder.Path / "tibia.cfg", "lower");
	WriteAll(Folder.Path / "Tibia.cfg", "mixed");
	WriteAll(Folder.Path / "TIBIA.CFG", "upper");
	CHECK(FindNoCase(Folder.Path, "tibia.cfg") == Folder.Path / "tibia.cfg");
	CHECK(FindNoCase(Folder.Path, "Tibia.cfg") == Folder.Path / "Tibia.cfg");
	CHECK(FindNoCase(Folder.Path, "tIBIA.cFG") == Folder.Path / "TIBIA.CFG"); // "TIBIA" < "Tibia" < "tibia"
}
#endif

TEST_CASE("TDataFileSystem: by default the working folder's names, as TSystemFileSystem")
{
	const TTemporaryFolder Folder;
	const std::string Name = (Folder.Path / "default.dat").string();
	TDataFileSystem Data;
	IFileSystem &Files = Data;
	CHECK(Data.Locations().Data.empty());
	CHECK_FALSE(Files.Exists(Name.c_str()));
	TFile Missing;
	errno = 0;
	CHECK_FALSE(Files.Open(Missing, Name.c_str(), TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(errno == ENOENT);
	TFile Written;
	REQUIRE(Files.Open(Written, Name.c_str(), TFile::TAccess::Write, TFile::TContent::Text));
	CHECK(Written.Write("a\nb"));
	CHECK(Written.Close() == 0);
	CHECK(Files.Exists(Name.c_str()));
	CHECK(ReadAll(Name) == "a\r\nb");
	CHECK(Data.PathOf("Tibia.dat") == std::filesystem::path("Tibia.dat"));
}

TEST_CASE("TDataFileSystem: the data folder's files, Patella.ini in the settings folder")
{
	const TMixedCaseFolder Folder;
	const TTemporaryFolder Settings;
	TDataFileSystem Data(TFileLocations{Folder.Path, Settings.Path, false});
	IFileSystem &Files = Data;

	CHECK(Files.Exists("Tibia.dat"));
	TFile Read;
	REQUIRE(Files.Open(Read, "Tibia.dat", TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(Read.GetChar() == 'd');
	CHECK(Read.Close() == 0);

	TFile Ini;
	REQUIRE(Files.Open(Ini, "Patella.ini", TFile::TAccess::Write, TFile::TContent::Binary));
	CHECK(Ini.Write("Device=cpu\r\n"));
	CHECK(Ini.Close() == 0);
	CHECK(ReadAll(Settings.Path / "Patella.ini") == "Device=cpu\r\n");
	CHECK_FALSE(std::filesystem::exists(Folder.Path / "Patella.ini"));
	CHECK(Files.Exists("Patella.ini"));

	TFile Channel;
	REQUIRE(Files.Open(Channel, "Default.txt", TFile::TAccess::Append, TFile::TContent::Text));
	CHECK(Channel.Write("line\n"));
	CHECK(Channel.Close() == 0);
	CHECK(ReadAll(Folder.Path / "Default.txt") == "line\r\n");

	// Without a settings folder Patella.ini is the data folder's.
	const TDataFileSystem Portable(TFileLocations{Folder.Path, {}, false});
	CHECK(Portable.PathOf("Patella.ini") == Folder.Path / "Patella.ini");
}

TEST_CASE("TDataFileSystem: mixed-case names with the lookup, and without it")
{
	const TMixedCaseFolder Folder;
	TDataFileSystem Data(TFileLocations{Folder.Path, {}, true});
	IFileSystem &Files = Data;

	CHECK(Files.Exists("TIBIA.DAT"));
	TFile Read;
	REQUIRE(Files.Open(Read, "TIBIA.DAT", TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(Read.GetChar() == 'd');
	CHECK(Read.Close() == 0);
	TFile Map;
	REQUIRE(Files.Open(Map, "automap/12312307.map", TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(Map.GetChar() == 'm');
	CHECK(Map.Close() == 0);
	// A new file goes into the folder found, under its own name.
	TFile New;
	REQUIRE(Files.Open(New, "AUTOMAP/12312308.map", TFile::TAccess::Write, TFile::TContent::Binary));
	CHECK(New.Write("new"));
	CHECK(New.Close() == 0);
	CHECK(ReadAll(Folder.Path / "AutoMap" / "12312308.map") == "new");
	TFile Missing;
	errno = 0;
	CHECK_FALSE(Files.Open(Missing, "Tibia.spr", TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(errno == ENOENT);
	CHECK_FALSE(Files.Exists("Tibia.spr"));

#if !defined(_WIN32)
	// Linux without the lookup: the exact names only.
	TDataFileSystem Exact(TFileLocations{Folder.Path, {}, false});
	CHECK_FALSE(Exact.Exists("TIBIA.DAT"));
	TFile Refused;
	errno = 0;
	CHECK_FALSE(Exact.Open(Refused, "TIBIA.DAT", TFile::TAccess::Read, TFile::TContent::Binary));
	CHECK(errno == ENOENT);
	CHECK(Exact.Exists("Tibia.dat"));
#endif
}

TEST_CASE("ChooseLocations: --data, the working folder, the executable's, Patella.ini's DataFolder")
{
	using patella::settings::ChooseLocations;
	using patella::settings::TDataSource;
	using patella::settings::TLocationInputs;
	const TTemporaryFolder Working;
	const TTemporaryFolder Executable;

	SUBCASE("nothing anywhere: the working folder, as the original")
	{
		const auto Choice = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, false, {}, false});
		CHECK(Choice.Source == TDataSource::Default);
		CHECK(Choice.Locations.Data.empty());
		CHECK(Choice.Locations.Settings.empty());
	}
	SUBCASE("the working folder's data: the names as written, as before")
	{
		WriteAll(Working.Path / "Tibia.dat", "dat");
		WriteAll(Executable.Path / "Tibia.dat", "dat");
		const auto Choice = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, false, {}, false});
		CHECK(Choice.Source == TDataSource::Working);
		CHECK(Choice.Locations.Data.empty());
		CHECK(Choice.Locations.Settings.empty());
		CHECK_FALSE(Choice.Locations.CaseInsensitive);
	}
	SUBCASE("the executable's folder; Patella.ini next to the data")
	{
		WriteAll(Executable.Path / "Tibia.dat", "dat");
		const auto Choice = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, false, {}, false});
		CHECK(Choice.Source == TDataSource::Executable);
		CHECK(Same(Choice.Locations.Data, Executable.Path));
		CHECK(Choice.Locations.Settings.empty());
	}
	SUBCASE("--data first, relative to the working folder")
	{
		WriteAll(Working.Path / "Tibia.dat", "dat");
		std::filesystem::create_directories(Working.Path / "game");
		const auto Choice = ChooseLocations(
			TLocationInputs{Working.Path, Executable.Path, std::filesystem::path("game"), false, {}, false});
		CHECK(Choice.Source == TDataSource::Option);
		CHECK(Same(Choice.Locations.Data, Working.Path / "game"));
		CHECK(Choice.Locations.Settings.empty());
	}
	SUBCASE("Patella.ini's DataFolder (relative to it); the settings stay in its folder")
	{
		std::filesystem::create_directories(Working.Path / "game");
		WriteAll(Working.Path / "game" / "Tibia.dat", "dat");
		WriteAll(Working.Path / "Patella.ini", "DataFolder=game\r\n");
		const auto Choice = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, false, {}, false});
		CHECK(Choice.Source == TDataSource::Settings);
		CHECK(Same(Choice.Locations.Data, Working.Path / "game"));
		CHECK(Same(Choice.Locations.Settings, Working.Path));
	}
	SUBCASE("a DataFolder without Tibia.dat is not taken")
	{
		std::filesystem::create_directories(Working.Path / "empty");
		WriteAll(Working.Path / "Patella.ini", "DataFolder=empty\r\n");
		const auto Choice = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, false, {}, false});
		CHECK(Choice.Source == TDataSource::Default);
		CHECK(Choice.Locations.Data.empty());
	}
	SUBCASE("the XDG option: Patella.ini in its folder, and its DataFolder")
	{
		const TTemporaryFolder Xdg;
		WriteAll(Executable.Path / "Tibia.dat", "dat");
		const auto Next = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, true, Xdg.Path, false});
		CHECK(Next.Source == TDataSource::Executable);
		CHECK(Same(Next.Locations.Data, Executable.Path));
		CHECK(Same(Next.Locations.Settings, Xdg.Path));

		std::filesystem::remove(Executable.Path / "Tibia.dat");
		std::filesystem::create_directories(Working.Path / "data");
		WriteAll(Working.Path / "data" / "Tibia.dat", "dat");
		WriteAll(Working.Path / "Patella.ini", "DataFolder=elsewhere\r\n"); // not read with the XDG option
		WriteAll(Xdg.Path / "Patella.ini", "DataFolder=" + (Working.Path / "data").string() + "\r\n");
		const auto Configured = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, true, Xdg.Path, false});
		CHECK(Configured.Source == TDataSource::Settings);
		CHECK(Same(Configured.Locations.Data, Working.Path / "data"));
		CHECK(Same(Configured.Locations.Settings, Xdg.Path));
	}
	SUBCASE("the lookup without regard to case finds a TIBIA.DAT")
	{
		WriteAll(Working.Path / "TIBIA.DAT", "dat");
		const auto Choice = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, false, {}, true});
		CHECK(Choice.Source == TDataSource::Working);
		CHECK(Choice.Locations.CaseInsensitive);
#if !defined(_WIN32)
		const auto Exact = ChooseLocations(TLocationInputs{Working.Path, Executable.Path, {}, false, {}, false});
		CHECK(Exact.Source == TDataSource::Default);
#endif
	}
}

TEST_CASE("XdgSettingsFolder and DataOption")
{
	using patella::settings::DataOption;
	using patella::settings::XdgSettingsFolder;
#if defined(_WIN32)
	const char *Absolute = "C:\\config";
	const char *Home = "C:\\home";
#else
	const char *Absolute = "/config";
	const char *Home = "/home/user";
#endif
	CHECK(XdgSettingsFolder(Absolute, Home) == std::filesystem::path(Absolute) / "patella");
	CHECK(XdgSettingsFolder(nullptr, Home) == std::filesystem::path(Home) / ".config" / "patella");
	CHECK(XdgSettingsFolder("", Home) == std::filesystem::path(Home) / ".config" / "patella");
	CHECK(XdgSettingsFolder("relative", Home) == std::filesystem::path(Home) / ".config" / "patella");
	CHECK(XdgSettingsFolder(nullptr, nullptr).empty());

	CHECK_FALSE(DataOption(std::vector<std::string>{}).has_value());
	CHECK_FALSE(DataOption(std::vector<std::string>{"--data"}).has_value());
	CHECK(DataOption(std::vector<std::string>{"gamemaster", "--data", "my data"}) == std::filesystem::path("my data"));
	CHECK(DataOption(std::vector<std::string>{"--data", "a", "--data", "b"}) == std::filesystem::path("b"));
	CHECK(DataOption(std::vector<std::wstring>{L"--test-headless", L"--data", L"Spiel"}) == std::filesystem::path(L"Spiel"));
}
