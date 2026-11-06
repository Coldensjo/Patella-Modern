// PreferenceKeys.h: the options of Tibia.cfg as keys of Patella.ini (Patella-Modern issue #45; plan D12).
//
// Since issue #45 the x64 client keeps every option the original kept in Tibia.cfg (Preferences.cpp) in
// Patella.ini (settings/Settings.h), one `key=value` line each, and writes Tibia.cfg no more. The keys are
// Tibia.cfg's names (ASCII case-insensitive, as every key of Patella.ini), written in this case:
//
//	PreferencesVersion=2                  the file has the options (without it an existing Tibia.cfg is imported);
//	                                      2: the file is UTF-8 (issue #117), 1: Windows-1252 (below)
//	TestServer=no                         yes or no, as every flag below
//	FullscreenMode=640,480,32             width, height, depth; no line while none was chosen (width 0)
//	WindowsMousepointer, LightEffects, TextualEffects, NoStretching
//	CreatureInfo, ConfineFramerate, Engine, ConsoleHeight, AutomapZoom, LastMotD, AttackMode, ChaseMode
//	Hotkey<N>="text",no                   N 0-35 (Tibia.cfg's numbers: F1 is 0), the text, sent at once (yes/no);
//	                                      a line only for a hotkey with a text
//	WindowedMode=10,10,800,600,no         x, y, width, height, maximized
//	ContentWindow<n>=ID,Size              n 1, 2, ...: the side bar's window sizes, in their order
//	BuddySortmethods=2,1,0
//	BuddyHideOffline, InfoMessages, EventMessages, StatusMessages, TimeStamps, PrivateMessages
//	OpenDialogs=1,4                       the numbers, in their order (empty: none)
//	OpenChannel=5,7                       the same
//	ClassicControl, AutoChaseOff, SecureMode
//	Buddy<n>=CharacterID,Icon,"description",notify   n 1, 2, ...
//	White="name","name"                   the permanent entries of the buddy (white) list
//	Ignore="name"                         the permanent entries of the ignore list
//	KnownHints=1,2,3
//	ShowNewHints, LevelBar, MagLevelBar
//	SkillBars=0,1,2,3,4,5,6               the skills whose bars are shown
//
// A value is a list of fields separated by ',' (spaces and tabs around them dropped): a number (decimal,
// 32 bits, optionally '-'), yes or no, or a text in double quotes. In a text `\\` is a backslash, `\"` a quote,
// `\n`, `\r` and `\t` line feed, carriage return and tab, `\xHH` the byte HH (two hex digits); the writer
// writes every other byte below 0x20 and 0x7f as `\xHH`, and so the five bytes Windows-1252 leaves undefined (0x81,
// 0x8d, 0x8f, 0x90, 0x9d: the C1 controls of their value, types/Cp1252.h). `\xHH` is always the client's byte HH.
//
// The encoding (issue #117, plan D4; notes/decisions.md "#117"): the client's texts are Windows-1252 (#112), the file
// is UTF-8. Every other character of a text is written as the UTF-8 of its Windows-1252 byte, and read back as the
// byte of its character: '?' for a character Windows-1252 does not have (typed in an editor) and for each ill-formed
// part of the UTF-8 (Utf8ToCp1252). A file of PreferencesVersion 1 (#45's client) has the bytes from 0x80 on as they
// are; ReadPreferences reads it so (TEncoding::Cp1252, Settings.h EncodingOf), and WritePreferences converts the
// whole file to UTF-8 first (its comments and other keys too) and writes it as version 2: a version-1 file is
// converted once, at the next save, its options unchanged by value.
//
// The reading mirrors the original's Tibia.cfg reader on the file its writer wrote (Preferences.cpp
// ReadTibiaCfg, WriteTibiaCfg), so the options after a restart are the original's: ChaseMode 2 reads as 0, a
// repeated OpenDialogs number once, a hotkey's line breaks as spaces (a final one: sent at once), its text
// cut to 255 bytes. Unlike the original's reader, a line it cannot read leaves that option as it was (the
// client passes the defaults) and is reported, instead of resetting every option. Keys it does not know are
// not its own (Device, LogLevel, ...): left alone, as the comments.
//
// No platform header: part of patella_core, tested on every preset (tests/unit/core_preferences_test.cpp).

#pragma once

#include "settings/Settings.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace patella::settings {

// Preferences.h's HOTKEYS and SKILLS (Preferences.cpp checks that they are the same).
inline constexpr int PreferenceHotkeys = 36;
inline constexpr int PreferenceSkills = 7;
// The longest hotkey text (Preferences.h HOTKEY_LENGTH - 1).
inline constexpr std::size_t PreferenceHotkeyLength = 255;

inline constexpr const char *PreferencesVersionKey = VersionKey;
inline constexpr int PreferencesVersion = 2;

struct TContentWindowValue {
	int ID = 0;
	int Size = 0;
	bool operator==(const TContentWindowValue &) const = default;
};

struct TBuddyValue {
	int CharacterID = 0;
	int Icon = 0;
	std::string Description;
	bool Notify = false;
	bool operator==(const TBuddyValue &) const = default;
};

// The options as Tibia.cfg's writer writes them (Preferences.cpp GetPreferenceValues fills it, after which
// SetPreferenceValues gives the client the options the original's reader would give it).
struct TPreferenceValues {
	bool TestServer = false;
	std::optional<std::array<int, 3>> FullscreenMode; // width, height, depth
	bool WindowsMousepointer = true;
	bool LightEffects = true;
	bool TextualEffects = true;
	int CreatureInfo = 2;
	bool NoStretching = false;
	int ConfineFramerate = 0;
	int Engine = -1;
	std::array<std::string, PreferenceHotkeys> Hotkeys;
	std::array<bool, PreferenceHotkeys> HotkeyAutoSend{};
	std::array<int, 4> WindowedMode{0, 0, 600, 400}; // x, y, width, height
	bool WindowedMaximized = false;
	int ConsoleHeight = 0;
	int AutomapZoom = 2;
	std::vector<TContentWindowValue> ContentWindows;
	std::array<int, 3> BuddySortmethods{2, 1, 0};
	bool BuddyHideOffline = false;
	std::vector<int> OpenDialogs;
	int LastMotD = 0;
	bool InfoMessages = false;
	bool EventMessages = true;
	bool StatusMessages = false;
	bool TimeStamps = false;
	bool PrivateMessages = true;
	std::vector<int> OpenChannels;
	bool ClassicControl = false;
	bool AutoChaseOff = true;
	int AttackMode = 2;
	int ChaseMode = 0;
	bool SecureMode = true;
	std::vector<TBuddyValue> Buddies;
	std::vector<std::string> White;
	std::vector<std::string> Ignore;
	std::vector<int> KnownHints;
	bool ShowNewHints = true;
	bool LevelBar = true;
	bool MagLevelBar = true;
	std::array<bool, PreferenceSkills> SkillBars{true, true, true, true, true, true, true};

	bool operator==(const TPreferenceValues &) const = default;
};

// The file has the options (PreferencesVersionKey).
[[nodiscard]] bool HasPreferences(const TSettingsFile &File);

// Writes every option into File, as version 2 (UTF-8): a file of version 1 is converted to UTF-8 first, every line
// (ConvertToUtf8). The keys it has are set where they are, the others appended; the Hotkey<N>, ContentWindow<n> and
// Buddy<n> lines of entries that are gone are removed, and so is FullscreenMode while none is chosen. Other keys and
// lines stay.
void WritePreferences(TSettingsFile &File, const TPreferenceValues &Values);

// Reads the options of File into Values, the texts in File's encoding (EncodingOf); an option without a line keeps
// its value. Returns the problems: a line per value it could not read (that option kept as well), and one for a
// PreferencesVersion other than 1 or 2 (read as 2).
std::vector<std::string> ReadPreferences(const TSettingsFile &File, TPreferenceValues &Values);

// A file of version 1 (Windows-1252) as UTF-8: every line's text converted (a byte order mark kept as it is); a file
// in UTF-8 is left as it is. Its PreferencesVersion stays: WritePreferences sets it.
void ConvertToUtf8(TSettingsFile &File);

// A text (the client's Windows-1252) as a quoted field in Encoding, and the fields of a value in Encoding (nothing
// when it is not a list of fields), their texts the client's Windows-1252.
std::string QuoteText(std::string_view Text, TEncoding Encoding = TEncoding::Utf8);

struct TField {
	enum class TKind { Number,
		Flag,
		Text } Kind = TKind::Number;
	int Number = 0;
	bool Flag = false;
	std::string Text;
};
std::optional<std::vector<TField>> ParseFields(std::string_view Value, TEncoding Encoding = TEncoding::Utf8);

} // namespace patella::settings
