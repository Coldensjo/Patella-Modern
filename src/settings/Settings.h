// Settings.h: Patella.ini, the fork's settings file (Patella-Modern issues #31, #45; plan D12).
//
// Since issue #45 it is the x64 client's only settings file: the options the original kept in Tibia.cfg
// (settings/PreferenceKeys.h) and everything new the fork lets the user set, in the working directory. An
// existing Tibia.cfg is imported once (Preferences.cpp InitPreferences) and never written again. The format
// is plain `key=value`, one per line:
//
//	# a comment (so is a line starting with ';')
//	Device=gpu
//
// Keys are compared without regard to ASCII case; spaces and tabs around keys and values are dropped;
// the first line with a key counts. Every other line (comments, blank lines, unknown keys, lines without
// '=') is kept as it is when the file is written again, and so are the file's line ends (CRLF or LF, as
// its first line has them). A parser of our own (D12: no dependency); no platform header, so it is part
// of patella_core and tested on every preset (tests/unit/core_settings_test.cpp).
//
// Keys today: Device (auto, gpu, gpuN, cpu; notes/vulkan.md, "Choosing the device"), LogLevel and LogFile
// (log/LogSettings.h, issue #77), the options of Tibia.cfg (settings/PreferenceKeys.h, issue #45), DataFolder (issue
// #130, ChooseLocations below). The other D12 keys (servers, scale, vsync) come with their issues.
//
// The text (issue #117, plan D4): the file is UTF-8, and a value that is the client's text (a hotkey, a name, LogFile)
// is converted to the client's Windows-1252 when read (types/Cp1252.h; '?' for a character it does not have). A file
// with PreferencesVersion=1 (written by #45's client until issue #117) has the client's Windows-1252 bytes as they are:
// it is read as such and converted to UTF-8 as a whole at the next save of the options (PreferenceKeys.h).

#pragma once

#include "platform/FileSystem.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace patella::settings {

// The file's name, next to the data files by default (ChooseLocations below; the working folder's until issue #130).
inline constexpr const char *FileName = patella::platform::SettingsFileName;

// The first line of a file the client creates, and its line ends.
inline constexpr const char *NewFileComment = "# Patella-Modern's settings";
inline constexpr const char *NewFileNewline = "\r\n";

// The largest file Load reads (a settings file is a few lines; anything larger is not one).
inline constexpr std::size_t MaxFileSize = std::size_t{1024} * 1024;

// The key that marks a file with the options (settings/PreferenceKeys.h) and the file's text encoding.
inline constexpr const char *VersionKey = "PreferencesVersion";

enum class TEncoding {
	Utf8,  // PreferencesVersion 2 or later, or a file without it
	Cp1252 // PreferencesVersion 1: the client's bytes as they are
};

class TSettingsFile {
public:
	// An empty file; Text() of it is empty, a Set adds the first line with Newline as its end.
	explicit TSettingsFile(std::string_view Newline = "\n");

	// The file's text. The line ends are taken from the first line that has one (CRLF or LF; Newline
	// when there is none).
	static TSettingsFile Parse(std::string_view Text, std::string_view Newline = "\n");

	// The value of the first line with Key (ASCII case-insensitive), without the spaces around it;
	// nothing when no line has it.
	[[nodiscard]] std::optional<std::string> Get(std::string_view Key) const;

	// Sets the first line with Key to "Key=Value", or appends that line. False (and nothing changed)
	// for a key that is empty, has spaces around it, starts with '#', ';' or a byte order mark or holds '=', or a key or
	// value with a line end, a NUL or spaces around it.
	bool Set(std::string_view Key, std::string_view Value);

	// Removes every line with Key (ASCII case-insensitive); the number of lines removed. A byte order mark on
	// a removed first line goes with it.
	int Remove(std::string_view Key);

	// The keys of the lines that have one, in the file's order, as written (duplicates included).
	[[nodiscard]] std::vector<std::string> Keys() const;

	// The contents to write: every line as it was read but the ones Set changed or appended.
	[[nodiscard]] std::string Text() const;

	// The lines (1 on) that are neither blank, a comment nor `key=value`: kept, never read.
	[[nodiscard]] std::vector<int> BadLines() const;

	[[nodiscard]] const std::string &Newline() const { return LineEnd; }

private:
	struct TLine {
		std::string Text; // as read, without the line end
		std::string Key;  // empty: no key
		std::string Value;
		bool Bad = false;
	};
	static TLine ParseLine(std::string Text, bool First);

	std::vector<TLine> Lines;
	std::string LineEnd;
	bool FinalNewline = true; // the text ended with a line end (a new or empty file: yes)
};

// The file's text encoding: Cp1252 where its PreferencesVersion is 1, else Utf8.
[[nodiscard]] TEncoding EncodingOf(const TSettingsFile &File);

// A value of File as the client's text (Windows-1252): converted from UTF-8, or as it is in a file of version 1.
[[nodiscard]] std::string ClientText(const TSettingsFile &File, std::string_view Value);

// The file is opened through Files (the platform's IFileSystem, issue #119: Patella.exe's TDataFileSystem, the
// data folder or the settings folder; Path is the client's text, as every name TFile opens), as a binary file (TFile, _wfsopen with
// _SH_DENYNO on Windows: what std::fstream opened until then).

// Reads the file. Nothing when it does not exist, cannot be read or is larger than MaxFileSize.
std::optional<TSettingsFile> Load(patella::platform::IFileSystem &Files, const std::string &Path,
	std::string_view Newline = "\n");

// The file to change and write back: the file as Load reads it; a new one (NewFileComment, NewFileNewline)
// when there is no file at Path (its open fails with ENOENT); nothing when there is one that Load cannot read (it is
// left alone).
std::optional<TSettingsFile> LoadForUpdate(patella::platform::IFileSystem &Files, const std::string &Path);

// Writes Settings.Text() to the file (created or replaced). False when that fails.
bool Save(patella::platform::IFileSystem &Files, const std::string &Path, const TSettingsFile &Settings);

// --- Where the files are (issue #130, plans D4, D12, D14) ------------------------------------------------------------
//
// The data folder (Tibia.dat, Tibia.spr, Tibia.pic, Tibia.cfg, the automap and the saved channels), the first of:
//	1. Option      --data <folder> of the command line (relative to the working folder);
//	2. Working     the working folder, where it has Tibia.dat (the original's place, and the tools' run folders);
//	3. Executable  the executable's folder, where it has Tibia.dat;
//	4. Settings    DataFolder of a Patella.ini (relative to that file's folder), where the folder it names has
//	               Tibia.dat: the XDG folder's with the XDG option, else the working folder's, then the executable's;
//	5. Default     the working folder (the original's error where Tibia.dat is missing).
// Tibia.dat is looked for without regard to ASCII case where CaseInsensitive (Linux; platform::FindNoCase).
// Patella.ini is in the settings folder: XdgFolder with the XDG option (Linux, D4), else the folder of the Patella.ini
// that step 4 read, else the data folder (portable). A folder that is the working folder stays an empty path in the
// locations, so the files open by their names as written, as before.

inline constexpr const char *DataFolderKey = "DataFolder";

enum class TDataSource : unsigned char { Option,
	Working,
	Executable,
	Settings,
	Default };

struct TLocationInputs {
	std::filesystem::path Working;                   // the working folder (absolute)
	std::filesystem::path Executable;                // the executable's folder; empty: not known
	std::optional<std::filesystem::path> DataOption; // --data <folder>
	bool Xdg = false;                                // the XDG option (Linux): Patella.ini in XdgFolder
	std::filesystem::path XdgFolder;                 // XdgSettingsFolder's
	bool CaseInsensitive = false;                    // Linux's lookup of the original names
};

struct TLocationChoice {
	patella::platform::TFileLocations Locations; // for platform::TDataFileSystem
	TDataSource Source = TDataSource::Default;
	std::filesystem::path DataFolder;     // absolute (the log's)
	std::filesystem::path SettingsFolder; // absolute (the log's)
};

[[nodiscard]] TLocationChoice ChooseLocations(const TLocationInputs &Inputs);

// The XDG option's folder: $XDG_CONFIG_HOME/patella, else $HOME/.config/patella (an XDG_CONFIG_HOME that is empty or
// not absolute is not used, as the XDG Base Directory Specification says); empty where neither is set. The variables'
// values are passed (null: not set), so the rule is tested on every platform.
[[nodiscard]] std::filesystem::path XdgSettingsFolder(const char *XdgConfigHome, const char *Home);

// The folder of the last "--data <folder>" of the arguments (without the program's name); nothing without one.
template <class String>
[[nodiscard]] std::optional<std::filesystem::path> DataOption(const std::vector<String> &Arguments)
{
	std::optional<std::filesystem::path> Folder;
	for (std::size_t i = 0; i + 1 < Arguments.size(); i++)
		if (std::filesystem::path(Arguments[i]).native() == std::filesystem::path("--data").native())
			Folder = std::filesystem::path(Arguments[i + 1]);
	return Folder;
}

// The source's words in the log.
[[nodiscard]] const char *SourceName(TDataSource Source);

} // namespace patella::settings
