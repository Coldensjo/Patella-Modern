// FileSystem.h: IFileSystem, the platform's files (Patella-Modern issue #109, plan M7; notes/decisions.md, "#109: the
// platform interfaces"). Not part of the original source. Portable: the C library and std::filesystem only.
//
// The client's files are opened by name, a name in the client's text (Windows-1252, issue #112; types/File.h): the
// script and binary files of Script.cpp (Tibia.dat, Tibia.spr, Tibia.pic, Tibia.cfg, the automap, the options), the
// saved channels of ConsoleWindow.cpp, and Preferences.cpp's test whether Tibia.cfg is there. Each goes through the
// process's IFileSystem (platform/Platform.h):
//
//	TSystemFileSystem  the files of the working folder, as the original opened them (TFile::Open, std::filesystem):
//	                   Patella.exe's, PatellaTest.exe's and the golden runner's (below)
//	TFolderFileSystem  the files of a folder of the caller's: the unit tests' (tests/unit/platform_test.cpp)
//
// Both are portable, so the platform's file system is the same code on every platform; what is Windows' is TFile's
// (its UTF-16 names, _wfsopen). Settings.cpp's Patella.ini and the developer log's LogFile are the fork's own files
// and open themselves (std::fstream, TFile): not the original's, and part of patella_core (#109's "Found, not done").

#pragma once

#include "types/Cp1252.h"
#include "types/File.h"

#include <cerrno>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace patella::platform {

class IFileSystem {
public:
	IFileSystem() = default;
	IFileSystem(const IFileSystem &) = delete;
	IFileSystem &operator=(const IFileSystem &) = delete;
	IFileSystem(IFileSystem &&) = delete;
	IFileSystem &operator=(IFileSystem &&) = delete;

	// TFile::Open(Name, Access, Content) of the file Name (the client's text): false where it failed, with errno.
	virtual bool Open(TFile &File, const char *Name, TFile::TAccess Access, TFile::TContent Content) = 0;
	// True when the file Name is there (std::filesystem::exists; false where it cannot be told).
	[[nodiscard]] virtual bool Exists(const char *Name) = 0;

protected:
	// Never destroyed through the interface (Clock.h's IClock).
	~IFileSystem() = default;
};

// The path of a name in the client's text: its UTF-16 on Windows, its UTF-8 elsewhere (as TFile::Open opens it).
inline std::filesystem::path PathOfName(std::string_view Name)
{
#if defined(_WIN32)
	return {patella::text::Cp1252ToWide(Name)};
#else
	return {patella::text::Cp1252ToUtf8(Name)};
#endif
}

// The working folder's files: what the original's fopen and Preferences.cpp's exists (issue #45) did.
class TSystemFileSystem final : public IFileSystem {
public:
	bool Open(TFile &File, const char *Name, TFile::TAccess Access, TFile::TContent Content) override
	{
		return File.Open(Name, Access, Content);
	}
	[[nodiscard]] bool Exists(const char *Name) override
	{
		std::error_code Error;
		try {
			return std::filesystem::exists(PathOfName(Name), Error);
		} catch (...) { // std::bad_alloc of the path
			return false;
		}
	}
};

// The test implementation: the names are those of the files in Root (the client's text too), and every open is
// listed (Opened). A name in Missing is not there: Open fails with ENOENT and Exists is false, whatever the folder
// has (the error paths of the unit tests).
class TFolderFileSystem final : public IFileSystem {
public:
	struct TOpen {
		std::string Name;
		TFile::TAccess Access;
		TFile::TContent Content;
	};

	explicit TFolderFileSystem(std::string RootFolder) : Root(std::move(RootFolder)) {}

	bool Open(TFile &File, const char *Name, TFile::TAccess Access, TFile::TContent Content) override
	{
		Opened.push_back({Name, Access, Content});
		if (IsMissing(Name)) {
			errno = ENOENT;
			return false;
		}
		return File.Open(InRoot(Name).c_str(), Access, Content);
	}
	[[nodiscard]] bool Exists(const char *Name) override
	{
		if (IsMissing(Name))
			return false;
		std::error_code Error;
		return std::filesystem::exists(PathOfName(InRoot(Name)), Error);
	}

	std::vector<std::string> Missing;
	std::vector<TOpen> Opened;

private:
	std::string Root;

	[[nodiscard]] std::string InRoot(std::string_view Name) const
	{
		std::string Path = Root;
		if (!Path.empty() && Path.back() != '/' && Path.back() != '\\')
			Path += '/';
		Path += Name;
		return Path;
	}
	[[nodiscard]] bool IsMissing(std::string_view Name) const
	{
		for (const std::string &Each : Missing)
			if (Each == Name)
				return true;
		return false;
	}
};

} // namespace patella::platform
