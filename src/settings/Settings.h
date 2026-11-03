// Settings.h: Patella.ini, the fork's own settings file (Patella-Modern issue #31; plan D12).
//
// Tibia.cfg keeps exactly the original's format (Preferences.cpp), so it stays readable by Tibia.exe and
// the comparisons check it byte for byte; everything new the fork lets the user set goes into
// Patella.ini, next to Tibia.cfg (the working directory). The format is plain `key=value`, one per line:
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
// Keys today (notes/vulkan.md, "Choosing the device"): Device (auto, gpu, gpuN, cpu). The other D12 keys
// (servers, scale, vsync, data folder, log level) come with their issues.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace patella::settings {

// The file's name, in the working directory as Tibia.cfg.
inline constexpr const char *FileName = "Patella.ini";

// The largest file Load reads (a settings file is a few lines; anything larger is not one).
inline constexpr std::size_t MaxFileSize = 1024 * 1024;

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
	// for a key that is empty, has spaces around it, starts with '#' or ';' or holds '=', or a key or
	// value with a line end, a NUL or spaces around it.
	bool Set(std::string_view Key, std::string_view Value);

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

// Reads the file. Nothing when it does not exist, cannot be read or is larger than MaxFileSize.
std::optional<TSettingsFile> Load(const std::string &Path, std::string_view Newline = "\n");

// Writes Settings.Text() to the file (created or replaced). False when that fails.
bool Save(const std::string &Path, const TSettingsFile &Settings);

} // namespace patella::settings
