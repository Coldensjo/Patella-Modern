// FileSystem.h: IFileSystem, the platform's files (Patella-Modern issue #109, plan M7; notes/decisions.md, "#109: the
// platform interfaces"). Not part of the original source. Portable: the C library and std::filesystem only.
//
// The client's files are opened by name, a name in the client's text (Windows-1252, issue #112; types/File.h): the
// script and binary files of Script.cpp (Tibia.dat, Tibia.spr, Tibia.pic, Tibia.cfg, the automap, the options), the
// saved channels of ConsoleWindow.cpp, and Preferences.cpp's test whether Tibia.cfg is there. Each goes through the
// process's IFileSystem (platform/Platform.h):
//
//	TSystemFileSystem  the files of the working folder, as the original opened them (TFile::Open, std::filesystem)
//	TDataFileSystem    issue #130 (plan M8, D14): the files of the data folder (Patella.ini of the settings folder),
//	                   with the original names found without regard to ASCII case where asked (Linux): Patella.exe's,
//	                   PatellaTest.exe's and the golden runner's (platform/Win32Platform.h SystemFileSystem). Until
//	                   the client's start-up sets its folders (settings::ChooseLocations, Player.cpp InitInstance), and
//	                   wherever the data folder is the working folder, it is TSystemFileSystem: the same calls.
//	TFolderFileSystem  the files of a folder of the caller's: the unit tests' (tests/unit/platform_test.cpp)
//
// Both are portable, so the platform's file system is the same code on every platform; what is Windows' is TFile's
// (its UTF-16 names, _wfsopen). Settings.cpp's Patella.ini and the developer log's LogFile are the fork's own files
// and open themselves (std::fstream, TFile): not the original's, and part of patella_core (#109's "Found, not done").

#pragma once

#include "compat/msvc.h"
#include "types/Cp1252.h"
#include "types/File.h"

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <optional>
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

// Issue #130: the fork's settings file (settings/Settings.h's FileName), which TDataFileSystem opens in its settings
// folder; every other name is the data folder's.
inline constexpr const char *SettingsFileName = "Patella.ini";

// Equal without regard to ASCII case (A-Z and a-z only: the original names are ASCII, and a byte or unit outside ASCII
// is compared as it is), in the system's form of a name (std::filesystem::path's value_type).
template <class Char>
[[nodiscard]] constexpr bool SameNameNoCase(std::basic_string_view<Char> Left, std::basic_string_view<Char> Right) noexcept
{
	if (Left.size() != Right.size())
		return false;
	const auto Lower = [](Char Unit) {
		return (Unit >= Char('A') && Unit <= Char('Z')) ? static_cast<Char>(Unit - Char('A') + Char('a')) : Unit;
	};
	for (std::size_t i = 0; i < Left.size(); i++)
		if (Lower(Left[i]) != Lower(Right[i]))
			return false;
	return true;
}

// The path of Relative in Folder (the working folder when empty) with each of Relative's parts as it is on the disk:
// the part as written where it is there, else the folder's entry of the same name without regard to ASCII case (the
// first in name order, where there are several), else the part as written (a file to create, or one that is not there:
// opening it fails as it did). An absolute Relative is looked up from its root. Throws what std::filesystem::path
// throws (std::bad_alloc); a folder that cannot be read has no other entries.
// GCC's analyzer (-fanalyzer, issue #130) follows a std::filesystem::path copy whose string throws length_error into
// "use of uninitialized value" after the throw: a false positive of its exception paths through libstdc++.
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-use-of-uninitialized-value\"")
inline std::filesystem::path FindNoCase(const std::filesystem::path &Folder, const std::filesystem::path &Relative)
{
	std::filesystem::path Current = Relative.has_root_path() ? Relative.root_path() : Folder;
	for (const std::filesystem::path &Part : Relative.relative_path()) {
		std::filesystem::path Next = Current / Part;
		std::error_code Error;
		if (Part.empty() || Part == "." || Part == ".." || std::filesystem::exists(Next, Error)) {
			Current = std::move(Next);
			continue;
		}
		std::optional<std::filesystem::path> Found;
		std::filesystem::directory_iterator Entries(Current.empty() ? std::filesystem::path(".") : Current, Error);
		for (; !Error && Entries != std::filesystem::directory_iterator(); Entries.increment(Error)) {
			std::filesystem::path Name = Entries->path().filename();
			if (SameNameNoCase<std::filesystem::path::value_type>(Name.native(), Part.native())
				&& (!Found || Name.native() < Found->native()))
				Found = std::move(Name);
		}
		Current /= Found ? *Found : Part;
	}
	return Current;
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")

// Issue #130: where TDataFileSystem opens the client's files.
struct TFileLocations {
	std::filesystem::path Data;     // Tibia.dat, .spr, .pic, Tibia.cfg, the automap, the channels; empty: the working folder
	std::filesystem::path Settings; // Patella.ini (SettingsFileName); empty: the data folder
	bool CaseInsensitive = false;   // the names found without regard to ASCII case (FindNoCase): Linux's
};

// The client's files in the data folder, Patella.ini in the settings folder (TFileLocations). With the default
// locations (the working folder, the names as written), Open and Exists are TSystemFileSystem's, call for call.
class TDataFileSystem final : public IFileSystem {
public:
	TDataFileSystem() = default;
	explicit TDataFileSystem(TFileLocations Locations) : Where(std::move(Locations)) {}

	// Set once, by the client's start-up, before the first file is opened.
	void SetLocations(TFileLocations Locations) { Where = std::move(Locations); }
	[[nodiscard]] const TFileLocations &Locations() const { return Where; }

	bool Open(TFile &File, const char *Name, TFile::TAccess Access, TFile::TContent Content) override
	{
		if (Name == nullptr || Plain())
			return File.Open(Name, Access, Content);
		try {
			const std::filesystem::path Path = PathOf(Name);
			return File.OpenSystemName(Path.c_str(), Access, Content);
		} catch (...) { // std::bad_alloc of the path
			errno = ENOMEM;
			return false;
		}
	}
	[[nodiscard]] bool Exists(const char *Name) override
	{
		std::error_code Error;
		try {
			return std::filesystem::exists(Plain() ? PathOfName(Name) : PathOf(Name), Error);
		} catch (...) { // std::bad_alloc of the path
			return false;
		}
	}

	// The path Name (the client's text) opens: in the settings folder for SettingsFileName, else in the data folder;
	// found without regard to ASCII case (FindNoCase) where CaseInsensitive.
	[[nodiscard]] std::filesystem::path PathOf(std::string_view Name) const
	{
		const std::filesystem::path Relative = PathOfName(Name);
		const bool Settings = SameNameNoCase<char>(Name, SettingsFileName) && !Where.Settings.empty();
		const std::filesystem::path &Folder = Settings ? Where.Settings : Where.Data;
		return Where.CaseInsensitive ? FindNoCase(Folder, Relative) : Folder / Relative;
	}

private:
	TFileLocations Where;

	[[nodiscard]] bool Plain() const { return Where.Data.empty() && Where.Settings.empty() && !Where.CaseInsensitive; }
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
