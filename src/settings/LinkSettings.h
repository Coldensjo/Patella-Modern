// LinkSettings.h: where the client's web links go (Patella-Modern issue #169, the owner's decision after #166). Not part
// of the original source.
//
// Keys of Patella.ini (settings/Settings.h), read once at the start (Player.cpp InitInstance). The user adds them; the
// client never writes them. One per destination, and the caption of Info's button:
//
//	LinkWebsite        Info: "www.tibia.com"
//	LinkManual         Help: "Manual"
//	LinkFAQ            Help: "FAQ"
//	LinkOptionsHelp    General Options: "Help"
//	LinkGraphicsHelp   Graphics Options and Advanced Graphics: "Help" (one address in the original)
//	LinkConsoleHelp    Console Options: "Help"
//	LinkNetworkHelp    Network Options: "Help"
//	LinkAccount        the title screen: "Access Account"
//	LinkSignup         Login: "Create Account"
//	LinkWebsiteLabel   Info's button says this in place of "www.tibia.com"
//
// An address that is unset or empty is the original's, so a client with none of the keys does what it did. Any other
// value must start with http:// or https:// (ASCII case-insensitive), have something after that, and hold no space or
// control character; else it is reported at the warn level and the original's address is used. The check is the safety
// part: SDL_OpenURL is ShellExecute's default verb on Windows, so a program path or a file: address in the setting
// would be started by a button (notes/decisions.md, "#169").
//
// Every link ends in patella::sdlframe::OpenURL, whose address comes from Target below: the dialogs and the golden
// tests' callbacks keep passing the original's text. A link's button also shows its address when the mouse rests on it
// (TLink, HoverText below). Part of patella_core (no platform header beyond the portable file system), tested by
// tests_core (tests/unit/core_link_settings_test.cpp) and fuzzed by fuzz_settings.

#pragma once

#include "log/Log.h"
#include "platform/FileSystem.h"
#include "settings/Settings.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace patella::settings {

inline constexpr const char *LinkWebsiteLabelKey = "LinkWebsiteLabel";

// The longest caption in bytes of the client's text: what TTextButton's Text (a TFixedString<50>) holds. A longer value
// is cut here, with a warning, whatever the fonts; Dialogs.cpp cuts it again to the button's width.
inline constexpr std::size_t MaxLinkCaption = 49;

// The most of a configured address the hover text shows (GUI drawing is per character and per frame): the address that
// is opened stays whole.
inline constexpr std::size_t MaxLinkHoverText = 255;

// True where Address (UTF-8) may be opened: the rule above. Ill-formed UTF-8 is not an address.
[[nodiscard]] bool ValidLinkAddress(std::string_view Address);

class TLinks {
public:
	// Reads the keys of File, replacing what an earlier call read. A value of a file with PreferencesVersion=1 is the
	// client's Windows-1252, any other UTF-8 (settings/Settings.h, EncodingOf); the warnings go to Log.
	void Configure(const TSettingsFile &File, patella::log::TLog &Log);

	// What to open for the client's Address (Windows-1252), in UTF-8 as SDL_OpenURL takes it: the configured address,
	// not converted again, where Address is an original with one; else Address converted, as the client always did.
	[[nodiscard]] std::string Target(const char *Address) const;

	// The hover text of a link to Address, the client's text: the configured address (at most MaxLinkHoverText bytes)
	// where it has one, else Address itself, the same pointer. Valid until the next Configure.
	[[nodiscard]] const char *HoverText(const char *Address) const;

	// LinkWebsiteLabel as the client's text, at most MaxLinkCaption bytes; empty where the original's caption stays.
	[[nodiscard]] const std::string &WebsiteLabel() const { return Label; }

private:
	struct TEntry {
		std::string_view Original; // Text.h's address
		std::string Target;        // UTF-8
		std::string HoverText;     // the client's text
	};
	[[nodiscard]] const TEntry *Find(const char *Address) const;

	std::vector<TEntry> Entries;
	std::string Label;
};

// The client's links: as if no key were set until LoadLinks.
[[nodiscard]] TLinks &Links();

// Reads Patella.ini (FileName) through Files and configures Links() with it, the warnings to the process's log; nothing
// without the file.
void LoadLinks(patella::platform::IFileSystem &Files);

} // namespace patella::settings
