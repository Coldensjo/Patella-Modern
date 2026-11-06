// The platform's file system (src/platform/FileSystem.h, issue #109): TSystemFileSystem opens the names of the working
// folder as TFile::Open does and tells whether a file is there as std::filesystem::exists does; TFolderFileSystem, the
// test implementation, opens them in its folder, lists the opens and leaves out the names it is told are missing.

#include <doctest/doctest.h>

#include "platform/FileSystem.h"
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
