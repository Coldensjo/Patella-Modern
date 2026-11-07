// Preferences.cpp: reconstructed from Tibia.exe 7.72 (0x4db2a0-0x4df53f).
//
// The functions without line evidence (0x4db2a0-0x4dc170, the gap after
// Player.cpp) come first, in binary order; they use only Preferences data
// (0x719e70-0x71c44f, after Player.cpp's last global at 0x719e6c, and
// 0x5c2558-0x5c255f in .data). The asserted functions follow in line order,
// which is also their binary order. None of them has a try block.
//
// The Tibia.cfg loader, saver and ExitPreferences (0x4ddc00-0x4df53f, in the
// gap before Script.cpp) use the same data and end the file, in binary
// order. The inline TReadScriptFile/TWriteScriptFile code after them
// (0x4df550-0x4df6c9) is Script.h's, kept from this object.
//
// Patella-Modern issue #45: Patella.ini is the x64 client's only settings file.
// The original's loader and saver are ReadTibiaCfg and WriteTibiaCfg: the first
// imports an existing Tibia.cfg once, the second only the golden corpus calls
// (the reference its tests read ReadTibiaCfg's results by, byte for byte); the
// client never writes Tibia.cfg. InitPreferences, SavePreferences and
// ExitPreferences, the names the client calls, read and write Patella.ini
// (settings/PreferenceKeys.h) and come last.
//
// Patella-Modern issue #104: the data and the functions are TPreferences' (Preferences.h), which TClient owns; the
// free functions at the end, the original's names, call them on bridge::Preferences(). Talk's lists (White, Ignore)
// are reached through TTalkLists, so this file no longer includes Talk.h.

#include "Preferences.h"
#include "Script.h"
#include "Utils.h"
#include "types/IndexedVector.h"
#include "common.h"
#include "types/FixedString.h"
#include "log/Log.h"
#include "settings/PreferenceKeys.h"
#include "settings/Settings.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory off for the file:
// the original's malloc'd entries and texts, freed by their lists; the original's raw owners (notes/ownership.md):
// the window tree owns its children, the message queue its blocks, the lists their entries
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)


// Patella.ini's keys have the same numbers of hotkeys and skills (issue #45).
static_assert(HOTKEYS == patella::settings::PreferenceHotkeys && SKILLS == patella::settings::PreferenceSkills
		&& HOTKEY_LENGTH - 1 == patella::settings::PreferenceHotkeyLength,
	"Preferences.h and settings/PreferenceKeys.h");

namespace patella {

// 0x4db2a0
bool TPreferences::GetTestServerLogin() const
{
	return TestServerLogin;
}

// 0x4db2b0
void TPreferences::SetTestServerLogin(bool Value)
{
	TestServerLogin = Value;
}

// 0x4db2c0
void TPreferences::SetViewMode(int Value)
{
	ViewMode = Value;
}

// 0x4db2d0
void TPreferences::SetFullscreenMode(const TFullscreenMode *Mode)
{
	FullscreenMode = *Mode;
}

// 0x4db300
void TPreferences::SetWindowsMousepointer(bool Value)
{
	WindowsMousepointer = Value;
}

// 0x4db310
void TPreferences::SetLightEffects(bool Value)
{
	LightEffects = Value;
}

// 0x4db320
void TPreferences::SetTextualEffects(bool Value)
{
	TextualEffects = Value;
}

// 0x4db330
void TPreferences::SetCreatureInfo(int Value)
{
	CreatureInfo = Value;
}

// 0x4db340
void TPreferences::SetNoStretching(bool Value)
{
	NoStretching = Value ? 1 : 0;
}

// 0x4db360. Only takes effect while the option has never been set.
void TPreferences::SetDefaultNoStretching(bool Value)
{
	if (NoStretching == 2)
		NoStretching = Value ? 1 : 0;
}

// 0x4db380
void TPreferences::SetConfineFramerate(int Value)
{
	ConfineFramerate = Value;
}

// 0x4db390
void TPreferences::SetEngine(int Value)
{
	Engine = Value;
}

// 0x4db3a0
int TPreferences::GetViewMode() const
{
	return ViewMode;
}

// 0x4db3b0
TFullscreenMode *TPreferences::GetFullscreenMode()
{
	return &FullscreenMode;
}

// 0x4db3c0
bool TPreferences::GetWindowsMousepointer() const
{
	return WindowsMousepointer;
}

// 0x4db3d0
bool TPreferences::GetLightEffects() const
{
	return LightEffects;
}

// 0x4db3e0
bool TPreferences::GetTextualEffects() const
{
	return TextualEffects;
}

// 0x4db3f0
int TPreferences::GetCreatureInfo() const
{
	return CreatureInfo;
}

// 0x4db400. "Not set yet" (2) reads as false.
bool TPreferences::GetNoStretching() const
{
	return NoStretching == 1;
}

// 0x4db410
int TPreferences::GetConfineFramerate() const
{
	return ConfineFramerate;
}

// 0x4db420
int TPreferences::GetEngine() const
{
	return Engine;
}

// 0x4db430
void TPreferences::SetWindowedMode(int x, int y, int Width, int Height, bool Maximized)
{
	WindowedX = x;
	WindowedY = y;
	WindowedWidth = Width;
	WindowedHeight = Height;
	WindowedMaximized = Maximized;
}

// 0x4db470
void TPreferences::GetWindowedMode(int *x, int *y, int *Width, int *Height, bool *Maximized) const
{
	*x = WindowedX;
	*y = WindowedY;
	*Width = WindowedWidth;
	*Height = WindowedHeight;
	*Maximized = WindowedMaximized;
}

// 0x4db4b0
void TPreferences::SetConsoleHeight(int Value)
{
	ConsoleHeight = Value;
}

// 0x4db4c0
int TPreferences::GetConsoleHeight() const
{
	return ConsoleHeight;
}

// 0x4db4d0
void TPreferences::SetAutomapZoom(int Value)
{
	AutomapZoom = Value;
}

// 0x4db4e0
int TPreferences::GetAutomapZoom() const
{
	return AutomapZoom;
}

// 0x4db4f0
void TPreferences::SetBuddySortmethods(const int *Methods)
{
	BuddySortmethods[0] = Methods[0];
	BuddySortmethods[1] = Methods[1];
	BuddySortmethods[2] = Methods[2];
}

// 0x4db510
void TPreferences::GetBuddySortmethods(int *Methods) const
{
	Methods[0] = BuddySortmethods[0];
	Methods[1] = BuddySortmethods[1];
	Methods[2] = BuddySortmethods[2];
}

// 0x4db530
void TPreferences::SetBuddyHideOffline(bool Value)
{
	BuddyHideOffline = Value;
}

// 0x4db540
bool TPreferences::GetBuddyHideOffline() const
{
	return BuddyHideOffline;
}

// 0x4db550
void TPreferences::ClearOpenDialogs()
{
	NumberOfOpenDialogs = 0;
}

// 0x4db560
const char *TPreferences::GetMotD() const
{
	if (MotD == nullptr)
		return "No current information.";
	return MotD;
}

// 0x4db570
void TPreferences::SetInfoMessages(bool Value)
{
	InfoMessages = Value;
}

// 0x4db580
void TPreferences::SetEventMessages(bool Value)
{
	EventMessages = Value;
}

// 0x4db590
void TPreferences::SetStatusMessages(bool Value)
{
	StatusMessages = Value;
}

// 0x4db5a0
void TPreferences::SetTimeStamps(bool Value)
{
	TimeStamps = Value;
}

// 0x4db5b0
void TPreferences::SetPrivateMessages(bool Value)
{
	PrivateMessages = Value;
}

// 0x4db5c0
bool TPreferences::GetInfoMessages() const
{
	return InfoMessages;
}

// 0x4db5d0
bool TPreferences::GetEventMessages() const
{
	return EventMessages;
}

// 0x4db5e0
bool TPreferences::GetStatusMessages() const
{
	return StatusMessages;
}

// 0x4db5f0
bool TPreferences::GetTimeStamps() const
{
	return TimeStamps;
}

// 0x4db600
bool TPreferences::GetPrivateMessages() const
{
	return PrivateMessages;
}

// 0x4db610
void TPreferences::ClearOpenChannels()
{
	OpenChannels.clear();
}

// 0x4db620
void TPreferences::SetClassicControl(bool Value)
{
	ClassicControl = Value;
}

// 0x4db630
void TPreferences::SetAutoChaseOff(bool Value)
{
	AutoChaseOff = Value;
}

// 0x4db640
bool TPreferences::GetClassicControl() const
{
	return ClassicControl;
}

// 0x4db650
bool TPreferences::GetAutoChaseOff() const
{
	return AutoChaseOff;
}

// 0x4db660
void TPreferences::SetAttackMode(int Value)
{
	AttackMode = Value;
}

// 0x4db670
void TPreferences::SetChaseMode(int Value)
{
	ChaseMode = Value;
}

// 0x4db680. A byte, although it is saved as a number like AttackMode.
void TPreferences::SetSecureMode(bool Value)
{
	SecureMode = Value;
}

// 0x4db690
int TPreferences::GetAttackMode() const
{
	return AttackMode;
}

// 0x4db6a0
int TPreferences::GetChaseMode() const
{
	return ChaseMode;
}

// 0x4db6b0
bool TPreferences::GetSecureMode() const
{
	return SecureMode;
}

// 0x4db6c0
void TPreferences::SetShowNewHints(bool Value)
{
	ShowNewHints = Value;
}

// 0x4db6d0
bool TPreferences::GetShowNewHints() const
{
	return ShowNewHints;
}

// 0x4db6e0
bool TPreferences::GetLevelBar() const
{
	return LevelBar;
}

// 0x4db6f0
bool TPreferences::GetMagLevelBar() const
{
	return MagLevelBar;
}

// 0x4db700
void TPreferences::SetLevelBar(bool Value)
{
	LevelBar = Value;
}

// 0x4db710
void TPreferences::SetMagLevelBar(bool Value)
{
	MagLevelBar = Value;
}

// 0x4db720. Not reset: the windowed position and size, and the hotkey texts
// past their first byte. MotD is cleared without being freed.
void TPreferences::SetDefaultPreferences(TTalkLists &Lists)
{
	memset(HotkeyAutoSend.data(), 0, sizeof(HotkeyAutoSend));
	ViewMode = 1;
	FullscreenMode.Width = 0;
	WindowsMousepointer = true;
	LightEffects = true;
	TextualEffects = true;
	CreatureInfo = 2;
	NoStretching = 2;
	ConfineFramerate = 0;
	Engine = -1;
	for (int i = 0; i < HOTKEYS; i++)
		Hotkeys[i][0] = 0;
	WindowedMaximized = false;
	ConsoleHeight = 0;
	AutomapZoom = 2;
	ContentWindows.clear();
	BuddySortmethods[0] = 2;
	BuddySortmethods[1] = 1;
	BuddySortmethods[2] = 0;
	BuddyHideOffline = false;
	NumberOfOpenDialogs = 0;
	LastMotD = 0;
	MotD = nullptr;
	InfoMessages = false;
	EventMessages = true;
	StatusMessages = false;
	TimeStamps = false;
	PrivateMessages = true;
	OpenChannels.clear();
	ClassicControl = false;
	AutoChaseOff = true;
	AttackMode = 2;
	ChaseMode = 0;
	SecureMode = true;
	Buddies.clear();
	KnownHints.clear();
	ShowNewHints = true;
	LevelBar = true;
	MagLevelBar = true;
	for (int i = 0; i < SKILLS; i++)
		SkillBars[i] = true;
	TestServerLogin = false;
	Lists.ClearIgnoreList();
}

// 0x4db880: vector<TContentWindowSize>::operator() (Container.h).
// 0x4dba60: vector<TBuddy>::operator() (Container.h).

// 0x4dbcd0. Updates the entry for ID or appends one.
void TPreferences::SetContentWindowSize(int ID, int Size)
{
	for (TContentWindowSize &Entry : ContentWindows) {
		if (Entry.ID == ID) {
			Entry.Size = Size;
			return;
		}
	}
	ContentWindows.push_back({ID, Size});
}

// 0x4dbd50. 0 for an unknown ID.
int TPreferences::GetContentWindowSize(int ID)
{
	for (const TContentWindowSize &Entry : ContentWindows) {
		if (Entry.ID == ID)
			return Entry.Size;
	}
	return 0;
}

// 0x4dbd90. Returns a new array in *List (left unchanged when there are
// none) and the number of entries.
int TPreferences::GetOpenDialogs(int **List)
{
	if (NumberOfOpenDialogs == 0)
		return 0;
	*List = new int[NumberOfOpenDialogs];
	for (int i = 0; i < NumberOfOpenDialogs; i++)
		(*List)[i] = OpenDialogs(i);
	return NumberOfOpenDialogs;
}

// 0x4dbde0. Like GetOpenDialogs.
int TPreferences::GetOpenChannels(int **List)
{
	if (OpenChannels.empty())
		return 0;
	*List = new int[OpenChannels.size()];
	std::ranges::copy(OpenChannels, *List);
	return static_cast<int>(OpenChannels.size());
}

// 0x4dbe30. Does not check for duplicates.
void TPreferences::AddKnownHint(int Hint)
{
	KnownHints.push_back(Hint);
}

// 0x4dbe50
bool TPreferences::IsKnownHint(int Hint)
{
	return std::ranges::find(KnownHints, Hint) != KnownHints.end();
}

// 0x4dbe90: vector<TContentWindowSize>::vector(min, max, block) (Container.h).
// 0x4dc170: vector<TBuddy>::vector(min, max, block) (Container.h).

// 0x4dc450, lines 223-225
void TPreferences::SetHotkey(int Number, const char *Text, bool AutoSend)
{
	PATELLA_ASSERT_VALUES(In(Number,0,HOTKEYS-1), Number);
	PATELLA_ASSERT(Text!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Text)<HOTKEY_LENGTH, strlen(Text));
	Hotkeys[Number].Assign(Text);
	HotkeyAutoSend[Number] = AutoSend;
}

// 0x4dc7e0, line 235
const char *TPreferences::GetHotkey(int Number, bool *AutoSend) const
{
	PATELLA_ASSERT_VALUES(In(Number,0,HOTKEYS-1), Number);
	*AutoSend = HotkeyAutoSend[Number];
	return Hotkeys[Number];
}

// 0x4dc950, line 393
void TPreferences::AddOpenDialog(int Identifier)
{
	PATELLA_ASSERT_VALUES(Identifier>=0, Identifier);
	OpenDialogs(NumberOfOpenDialogs) = Identifier;
	NumberOfOpenDialogs++;
}

// 0x4dcac0, line 425. Text is "<number>\n<message>"; without a line break
// nothing changes. Returns true when the number differs from the last one.
bool TPreferences::SetMotD(const char *Text)
{
	PATELLA_ASSERT(Text != nullptr);
	const char *Break = strchr(Text, '\n');
	if (Break == nullptr)
		return false;
	const int Number = atoi(Text); // NOLINT(bugprone-unchecked-string-to-number-conversion): the original's; no number is 0
	const bool New = Number != LastMotD;
	LastMotD = Number;
	if (MotD != nullptr)
		delete[] MotD;
	const std::size_t Size = strlen(Break + 1) + 1;
	MotD = new char[Size];
	TStringBuffer(MotD, Size).Assign(Break + 1);
	return New;
}

// 0x4dccb0, line 551
void TPreferences::AddOpenChannel(int Identifier)
{
	PATELLA_ASSERT_VALUES(Identifier>=0, Identifier);
	OpenChannels.push_back(Identifier);
}

// 0x4dce20, line 680. Index of the buddy entry, -1 if there is none.
int TPreferences::FindBuddy(int CharacterID)
{
	PATELLA_ASSERT_VALUES(CharacterID!=0, CharacterID);
	for (int i = 0; i < std::ssize(Buddies); i++) {
		if (Buddies[static_cast<std::size_t>(i)].CharacterID == CharacterID)
			return i;
	}
	return -1;
}

// 0x4dcfb0, lines 691-693. Deleted entries are not reused.
void TPreferences::SetBuddy(int CharacterID, int Icon, const char *Description, bool Notify)
{
	PATELLA_ASSERT_VALUES(CharacterID!=0, CharacterID);
	PATELLA_ASSERT(Description!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Description)<BUDDYDESCRIPTION_LENGTH, strlen(Description));
	const int Number = FindBuddy(CharacterID);
	TBuddy *Buddy = Number == -1 ? &Buddies.emplace_back() : &Buddies[static_cast<std::size_t>(Number)];
	Buddy->CharacterID = CharacterID;
	Buddy->Icon = Icon;
	Buddy->Description.Assign(Description);
	Buddy->Notify = Notify;
}

// 0x4dd360, line 711. Only marks the entry as free.
void TPreferences::DeleteBuddy(int CharacterID)
{
	PATELLA_ASSERT_VALUES(CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number != -1)
		Buddies[static_cast<std::size_t>(Number)].CharacterID = 0;
}

// 0x4dd4c0, line 722
int TPreferences::GetBuddyIcon(int CharacterID)
{
	PATELLA_ASSERT_VALUES(CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number == -1)
		return 0;
	return Buddies[static_cast<std::size_t>(Number)].Icon;
}

// 0x4dd640, line 733. NULL for an unknown character.
const char *TPreferences::GetBuddyDescription(int CharacterID)
{
	PATELLA_ASSERT_VALUES(CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number == -1)
		return nullptr;
	BuddyDescription = Buddies[static_cast<std::size_t>(Number)].DescriptionText();
	return BuddyDescription;
}

// 0x4dd7c0, line 744
bool TPreferences::GetBuddyNotify(int CharacterID)
{
	PATELLA_ASSERT_VALUES(CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number == -1)
		return false;
	return Buddies[static_cast<std::size_t>(Number)].Notify;
}

// 0x4dd940, line 825
bool TPreferences::GetSkillBar(int SkillNumber) const
{
	PATELLA_ASSERT_VALUES(In(SkillNumber,0,SKILLS-1), SkillNumber);
	return SkillBars[SkillNumber];
}

// 0x4ddaa0, line 850
void TPreferences::SetSkillBar(int SkillNumber, bool Value)
{
	PATELLA_ASSERT_VALUES(In(SkillNumber,0,SKILLS-1), SkillNumber);
	SkillBars[SkillNumber] = Value;
}

// 0x4ddc00 (InitPreferences in the original). Loads Tibia.cfg. Every key is tested on its own (no else), so
// a key runs one block only because the names differ. Unknown keys raise
// no error but leave their value unread, which then fails as the next key.
// Any error (also a missing file) resets everything to the defaults. In
// both cases the options count as loaded, so SavePreferences writes them.
// ServerAddress, ServerPort and LoginMessages are read and dropped.
void TPreferences::ReadTibiaCfg(TTalkLists &Lists)
{
	SetDefaultPreferences(Lists);
	try {
		TReadScriptFile Script;
		Script.open("Tibia.cfg");
		for (;;) {
			Script.nextToken();
			if (Script.Token == TOKEN::ENDOFFILE) {
				Script.close();
				break;
			}
			if (Script.Token != TOKEN::IDENTIFIER)
				Script.error("Identifier expected");
			TFixedString<MAX_IDENT_LENGTH> Identifier;
			CopyString(Identifier.Chars, Script.getIdentifier());
			Script.readSymbol('=');
			if (strcmp(Identifier, "testserver") == 0)
				TestServerLogin = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "serveraddress") == 0)
				Script.readString();
			if (strcmp(Identifier, "serverport") == 0)
				Script.readNumber();
			if (strcmp(Identifier, "fullscreenmode") == 0) {
				Script.readSymbol('(');
				FullscreenMode.Width = Script.readNumber();
				Script.readSymbol(',');
				FullscreenMode.Height = Script.readNumber();
				Script.readSymbol(',');
				FullscreenMode.Depth = Script.readNumber();
				Script.readSymbol(')');
			}
			if (strcmp(Identifier, "windowsmousepointer") == 0)
				WindowsMousepointer = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "lighteffects") == 0)
				LightEffects = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "textualeffects") == 0)
				TextualEffects = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "creatureinfo") == 0)
				CreatureInfo = Script.readNumber();
			if (strcmp(Identifier, "nostretching") == 0)
				NoStretching = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "confineframerate") == 0)
				ConfineFramerate = Script.readNumber();
			if (strcmp(Identifier, "engine") == 0)
				Engine = Script.readNumber();
			if (strcmp(Identifier, "hotkey") == 0) {
				// A trailing line break marks a hotkey that is sent at once;
				// other line breaks become spaces.
				Script.readSymbol('(');
				const int Number = Script.readNumber();
				if (Number < 0 || Number > HOTKEYS - 1)
					Script.error("Illegal number");
				Script.readSymbol(',');
				char *Text = Hotkeys[Number];
				CopyString(Hotkeys[Number].Chars, Script.readString());
				int Length = (int)strlen(Text);
				if (Length != 0) {
					if (Text[Length - 1] == '\n') {
						Text[Length - 1] = 0;
						HotkeyAutoSend[Number] = true;
						Length--;
					}
					for (int i = 0; i < Length; i++) {
						if (Text[i] == '\n')
							Text[i] = ' ';
					}
				}
				Script.readSymbol(')');
			}
			if (strcmp(Identifier, "windowedmode") == 0) {
				Script.readSymbol('(');
				WindowedX = Script.readNumber();
				Script.readSymbol(',');
				WindowedY = Script.readNumber();
				Script.readSymbol(',');
				WindowedWidth = Script.readNumber();
				Script.readSymbol(',');
				WindowedHeight = Script.readNumber();
				Script.readSymbol(',');
				WindowedMaximized = strcmp(Script.readIdentifier(), "yes") == 0;
				Script.readSymbol(')');
			}
			if (strcmp(Identifier, "consoleheight") == 0)
				ConsoleHeight = Script.readNumber();
			if (strcmp(Identifier, "automapzoom") == 0)
				AutomapZoom = Script.readNumber();
			if (strcmp(Identifier, "contentwindow") == 0) {
				// Appended when it is complete; the original wrote the entry in place and counted it then (an error on
				// the way resets the options, SetDefaultPreferences, so a partial entry was never read; issue #100).
				TContentWindowSize Entry{};
				Script.readSymbol('(');
				Entry.ID = Script.readNumber();
				Script.readSymbol(',');
				Entry.Size = Script.readNumber();
				Script.readSymbol(')');
				ContentWindows.push_back(Entry);
			}
			if (strcmp(Identifier, "buddysortmethods") == 0) {
				Script.readSymbol('(');
				BuddySortmethods[0] = Script.readNumber();
				Script.readSymbol(',');
				BuddySortmethods[1] = Script.readNumber();
				Script.readSymbol(',');
				BuddySortmethods[2] = Script.readNumber();
				Script.readSymbol(')');
			}
			if (strcmp(Identifier, "buddyhideoffline") == 0)
				BuddyHideOffline = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "opendialogs") == 0) {
				// The duplicate check reads the vector's fields directly: from
				// min to max-1 (not up to NumberOfOpenDialogs) and without the
				// start offset that operator() subtracts.
				const int Dialog = Script.readNumber();
				int i = OpenDialogs.min;
				for (;;) {
					if (i >= OpenDialogs.max) {
						OpenDialogs(NumberOfOpenDialogs) = Dialog;
						NumberOfOpenDialogs++;
						break;
					}
					if (OpenDialogs.entry[i++] == Dialog)
						break;
				}
			}
			if (strcmp(Identifier, "lastmotd") == 0)
				LastMotD = Script.readNumber();
			if (strcmp(Identifier, "infomessages") == 0)
				InfoMessages = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "eventmessages") == 0)
				EventMessages = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "loginmessages") == 0)
				Script.readIdentifier();
			if (strcmp(Identifier, "statusmessages") == 0)
				StatusMessages = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "timestamps") == 0)
				TimeStamps = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "privatemessages") == 0)
				PrivateMessages = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "openchannel") == 0) {
				OpenChannels.push_back(Script.readNumber());
			}
			if (strcmp(Identifier, "classiccontrol") == 0)
				ClassicControl = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "autochaseoff") == 0)
				AutoChaseOff = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "attackmode") == 0)
				AttackMode = Script.readNumber();
			if (strcmp(Identifier, "chasemode") == 0) {
				ChaseMode = Script.readNumber();
				if (ChaseMode == 2)
					ChaseMode = 0;
			}
			if (strcmp(Identifier, "securemode") == 0)
				SecureMode = Script.readNumber() == 1;
			if (strcmp(Identifier, "buddy") == 0) {
				// The description is copied without a length limit: a description of 128 characters is kept
				// whole, its NUL in Notify, set below; one of 132 or more wrote past the entry in the original
				// (undefined), cut to 131 characters here (issues #57, #85).
				// Appended when it is complete, as a ContentWindow's entry.
				TBuddy Buddy{};
				Script.readSymbol('(');
				Buddy.CharacterID = Script.readNumber();
				Script.readSymbol(',');
				Buddy.Icon = Script.readNumber();
				Script.readSymbol(',');
				const char *Description = Script.readString();
				Buddy.AssignDescriptionRead(Description);
				Script.readSymbol(',');
				const char *Notify = Script.readIdentifier();
				Buddy.Notify = strcmp(Notify, "yes") == 0;
				Script.readSymbol(')');
				Buddies.push_back(Buddy);
			}
			if (strcmp(Identifier, "white") == 0) {
				Script.readSymbol('{');
				for (;;) {
					Script.nextToken();
					if (Script.Token == TOKEN::SPECIAL && Script.getSpecial() == '}')
						break;
					if (Script.Token != TOKEN::SPECIAL || Script.getSpecial() != ',')
						Lists.AddBuddyIgnore(Script.getString(), true);
				}
			}
			if (strcmp(Identifier, "ignore") == 0) {
				Script.readSymbol('{');
				for (;;) {
					Script.nextToken();
					if (Script.Token == TOKEN::SPECIAL && Script.getSpecial() == '}')
						break;
					if (Script.Token != TOKEN::SPECIAL || Script.getSpecial() != ',')
						Lists.IgnoreName(Script.getString(), true);
				}
			}
			if (strcmp(Identifier, "knownhints") == 0) {
				Script.readSymbol('{');
				for (;;) {
					Script.nextToken();
					if (Script.Token == TOKEN::SPECIAL && Script.getSpecial() == '}')
						break;
					if (Script.Token != TOKEN::SPECIAL || Script.getSpecial() != ',') {
						KnownHints.push_back(Script.getNumber());
					}
				}
			}
			if (strcmp(Identifier, "shownewhints") == 0)
				ShowNewHints = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "levelbar") == 0)
				LevelBar = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "maglevelbar") == 0)
				MagLevelBar = strcmp(Script.readIdentifier(), "yes") == 0;
			if (strcmp(Identifier, "skillbars") == 0) {
				for (int i = 0; i < SKILLS; i++)
					SetSkillBar(i, false);
				Script.readSymbol('{');
				for (;;) {
					Script.nextToken();
					if (Script.Token == TOKEN::SPECIAL && Script.getSpecial() == '}')
						break;
					if (Script.Token != TOKEN::SPECIAL || Script.getSpecial() != ',')
						SetSkillBar(Script.getNumber(), true);
				}
			}
		}
	} catch (TMyException &) {
		SetDefaultPreferences(Lists);
	}
	PreferencesLoaded = true;
}

// 0x4de890 (SavePreferences in the original). Writes Tibia.cfg, only after the
// options were loaded (ReadTibiaCfg or InitPreferences); errors are
// ignored. Odd behaviour kept: one counter serves all loops, and the White
// list does not reset it, so it starts at the number of buddy entries and writes a
// ',' before its first entry when there were buddies. SkillBars writes ','
// before every skill above 0, also when nothing came before it. Deleted
// buddies (CharacterID 0) are skipped. The fourth FullscreenMode value is
// not saved.
void TPreferences::WriteTibiaCfg(TTalkLists &Lists)
{
	int i = 0;
	if (!PreferencesLoaded)
		return;
	try {
		TWriteScriptFile Script;
		Script.open("Tibia.cfg");
		Script.writeText("# Tibia - graphical Multi-User-Dungeon");
		Script.writeLn();
		Script.writeText("# DO NOT EDIT THIS FILE");
		Script.writeLn();
		Script.writeLn();
		Script.writeText("TestServer = ");
		Script.writeText(TestServerLogin ? "yes" : "no");
		Script.writeLn();
		if (FullscreenMode.Width != 0) {
			Script.writeText("FullscreenMode = (");
			Script.writeNumber(FullscreenMode.Width);
			Script.writeText(",");
			Script.writeNumber(FullscreenMode.Height);
			Script.writeText(",");
			Script.writeNumber(FullscreenMode.Depth);
			Script.writeText(")");
			Script.writeLn();
		}
		Script.writeText("WindowsMousepointer = ");
		Script.writeText(WindowsMousepointer ? "yes" : "no");
		Script.writeLn();
		Script.writeText("LightEffects = ");
		Script.writeText(LightEffects ? "yes" : "no");
		Script.writeLn();
		Script.writeText("TextualEffects = ");
		Script.writeText(TextualEffects ? "yes" : "no");
		Script.writeLn();
		Script.writeText("CreatureInfo = ");
		Script.writeNumber(CreatureInfo);
		Script.writeLn();
		Script.writeText("NoStretching = ");
		Script.writeText((NoStretching == 1) ? "yes" : "no");
		Script.writeLn();
		Script.writeText("ConfineFramerate = ");
		Script.writeNumber(ConfineFramerate);
		Script.writeLn();
		for (; i < HOTKEYS; i++) {
			const char *Text = Hotkeys[i];
			if (Text[0] == 0)
				continue;
			TFixedString<HOTKEY_LENGTH + 1> Help;
			Script.writeText("Hotkey = (");
			Script.writeNumber(i);
			Script.writeText(",");
			if (HotkeyAutoSend[i]) {
				Help.Format("{}\n", Text); // at most HOTKEY_LENGTH - 1 characters and "\n": they fit
				Text = Help;
			}
			Script.writeString(Text);
			Script.writeText(")");
			Script.writeLn();
		}
		Script.writeText("WindowedMode = (");
		Script.writeNumber(WindowedX);
		Script.writeText(",");
		Script.writeNumber(WindowedY);
		Script.writeText(",");
		Script.writeNumber(WindowedWidth);
		Script.writeText(",");
		Script.writeNumber(WindowedHeight);
		Script.writeText(",");
		Script.writeText(WindowedMaximized ? "yes" : "no");
		Script.writeText(")");
		Script.writeLn();
		Script.writeText("ConsoleHeight = ");
		Script.writeNumber(ConsoleHeight);
		Script.writeLn();
		Script.writeText("AutomapZoom = ");
		Script.writeNumber(AutomapZoom);
		Script.writeLn();
		for (const TContentWindowSize &Entry : ContentWindows) {
			Script.writeText("ContentWindow = (");
			Script.writeNumber(Entry.ID);
			Script.writeText(",");
			Script.writeNumber(Entry.Size);
			Script.writeText(")");
			Script.writeLn();
		}
		Script.writeText("BuddySortmethods = (");
		Script.writeNumber(BuddySortmethods[0]);
		Script.writeText(",");
		Script.writeNumber(BuddySortmethods[1]);
		Script.writeText(",");
		Script.writeNumber(BuddySortmethods[2]);
		Script.writeText(")");
		Script.writeLn();
		Script.writeText("BuddyHideOffline = ");
		Script.writeText(BuddyHideOffline ? "yes" : "no");
		Script.writeLn();
		for (i = 0; i < NumberOfOpenDialogs; i++) {
			Script.writeText("OpenDialogs = ");
			Script.writeNumber(OpenDialogs(i));
			Script.writeLn();
		}
		Script.writeText("LastMotD = ");
		Script.writeNumber(LastMotD);
		Script.writeLn();
		Script.writeText("InfoMessages = ");
		Script.writeText(InfoMessages ? "yes" : "no");
		Script.writeLn();
		Script.writeText("EventMessages = ");
		Script.writeText(EventMessages ? "yes" : "no");
		Script.writeLn();
		Script.writeText("StatusMessages = ");
		Script.writeText(StatusMessages ? "yes" : "no");
		Script.writeLn();
		Script.writeText("TimeStamps = ");
		Script.writeText(TimeStamps ? "yes" : "no");
		Script.writeLn();
		Script.writeText("PrivateMessages = ");
		Script.writeText(PrivateMessages ? "yes" : "no");
		Script.writeLn();
		for (const int Channel : OpenChannels) {
			Script.writeText("OpenChannel = ");
			Script.writeNumber(Channel);
			Script.writeLn();
		}
		Script.writeText("ClassicControl = ");
		Script.writeText(ClassicControl ? "yes" : "no");
		Script.writeLn();
		Script.writeText("AutoChaseOff = ");
		Script.writeText(AutoChaseOff ? "yes" : "no");
		Script.writeLn();
		Script.writeText("AttackMode = ");
		Script.writeNumber(AttackMode);
		Script.writeLn();
		Script.writeText("ChaseMode = ");
		Script.writeNumber(ChaseMode);
		Script.writeLn();
		Script.writeText("SecureMode = ");
		Script.writeNumber(SecureMode);
		Script.writeLn();
		// i ends at the number of entries, the deleted ones included: the White list counts on from there.
		for (i = 0; i < std::ssize(Buddies); i++) {
			const TBuddy &Buddy = Buddies[static_cast<std::size_t>(i)];
			if (Buddy.CharacterID == 0)
				continue;
			Script.writeText("Buddy = (");
			Script.writeNumber(Buddy.CharacterID);
			Script.writeText(",");
			Script.writeNumber(Buddy.Icon);
			Script.writeText(",");
			Script.writeString(Buddy.DescriptionText());
			Script.writeText(",");
			Script.writeText(Buddy.Notify ? "yes" : "no");
			Script.writeText(")");
			Script.writeLn();
		}
		char *List = Lists.GetBuddyIgnoreText();
		Script.writeText("White = {");
		if (List != nullptr) {
			char *Name = List;
			while ((Name = strtok(Name, "\n")) != nullptr) {
				const int Length = (int)strlen(Name);
				if (Length != 0 && Lists.IsPermanentBuddyIgnore(Name)) {
					if (i > 0)
						Script.writeText(",");
					Script.writeString(Name);
					i++;
				}
				Name = nullptr;
			}
			free(List);
		}
		Script.writeText("}");
		Script.writeLn();
		List = Lists.GetIgnoreListText();
		i = 0;
		Script.writeText("Ignore = {");
		if (List != nullptr) {
			char *Name = List;
			while ((Name = strtok(Name, "\n")) != nullptr) {
				const int Length = (int)strlen(Name);
				if (Length != 0 && Lists.IsPermanentIgnore(Name)) {
					if (i > 0)
						Script.writeText(",");
					Script.writeString(Name);
					i++;
				}
				Name = nullptr;
			}
			free(List);
		}
		Script.writeText("}");
		Script.writeLn();
		Script.writeText("KnownHints = {");
		for (i = 0; i < std::ssize(KnownHints); i++) {
			if (i > 0)
				Script.writeText(",");
			Script.writeNumber(KnownHints[static_cast<std::size_t>(i)]);
		}
		Script.writeText("}");
		Script.writeLn();
		Script.writeText("ShowNewHints = ");
		Script.writeText(ShowNewHints ? "yes" : "no");
		Script.writeLn();
		Script.writeText("LevelBar = ");
		Script.writeText(LevelBar ? "yes" : "no");
		Script.writeLn();
		Script.writeText("MagLevelBar = ");
		Script.writeText(MagLevelBar ? "yes" : "no");
		Script.writeLn();
		Script.writeText("SkillBars = {");
		for (i = 0; i < SKILLS; i++) {
			if (GetSkillBar(i)) {
				if (i > 0)
					Script.writeText(",");
				Script.writeNumber(i);
			}
		}
		Script.writeText("}");
		Script.writeLn();
		Script.writeText("Engine = ");
		Script.writeNumber(Engine);
		Script.writeLn();
		Script.close();
	} catch (TMyException &) { // NOLINT(bugprone-empty-catch): the original's; Tibia.cfg not written leaves the old file
	}
}

// The options as WriteTibiaCfg writes them: the hotkeys with a text, the windowed
// mode, the lists up to their counts, the buddies without the deleted ones, the
// permanent entries of the Talk lists, NoStretching's "not set yet" as no, no
// fourth FullscreenMode value. A description is read as WriteTibiaCfg reads it,
// up to its NUL, at most its 128 characters (issue #86).
patella::settings::TPreferenceValues TPreferences::GetPreferenceValues(TTalkLists &Lists)
{
	patella::settings::TPreferenceValues Values;
	Values.TestServer = TestServerLogin;
	if (FullscreenMode.Width != 0)
		Values.FullscreenMode = std::array<int, 3>{FullscreenMode.Width, FullscreenMode.Height, FullscreenMode.Depth};
	Values.WindowsMousepointer = WindowsMousepointer;
	Values.LightEffects = LightEffects;
	Values.TextualEffects = TextualEffects;
	Values.CreatureInfo = CreatureInfo;
	Values.NoStretching = NoStretching == 1;
	Values.ConfineFramerate = ConfineFramerate;
	Values.Engine = Engine;
	for (std::size_t i = 0; i < HOTKEYS; i++) {
		Values.Hotkeys[i] = static_cast<const char *>(Hotkeys[i]);
		Values.HotkeyAutoSend[i] = !Values.Hotkeys[i].empty() && HotkeyAutoSend[i];
	}
	Values.WindowedMode = {WindowedX, WindowedY, WindowedWidth, WindowedHeight};
	Values.WindowedMaximized = WindowedMaximized;
	Values.ConsoleHeight = ConsoleHeight;
	Values.AutomapZoom = AutomapZoom;
	for (const TContentWindowSize &Entry : ContentWindows)
		Values.ContentWindows.push_back({Entry.ID, Entry.Size});
	Values.BuddySortmethods = BuddySortmethods;
	Values.BuddyHideOffline = BuddyHideOffline;
	for (int i = 0; i < NumberOfOpenDialogs; i++)
		Values.OpenDialogs.push_back(OpenDialogs(i));
	Values.LastMotD = LastMotD;
	Values.InfoMessages = InfoMessages;
	Values.EventMessages = EventMessages;
	Values.StatusMessages = StatusMessages;
	Values.TimeStamps = TimeStamps;
	Values.PrivateMessages = PrivateMessages;
	Values.OpenChannels = OpenChannels;
	Values.ClassicControl = ClassicControl;
	Values.AutoChaseOff = AutoChaseOff;
	Values.AttackMode = AttackMode;
	Values.ChaseMode = ChaseMode;
	Values.SecureMode = SecureMode;
	for (const TBuddy &Buddy : Buddies) {
		if (Buddy.CharacterID == 0)
			continue;
		Values.Buddies.push_back({Buddy.CharacterID, Buddy.Icon, std::string(Buddy.Description.View()), Buddy.Notify});
	}
	const auto Names = [&Lists](char *List, bool (TTalkLists::*Permanent)(const char *)) {
		std::vector<std::string> Result;
		if (List == nullptr)
			return Result;
		std::string_view Rest = List;
		while (!Rest.empty()) {
			const std::size_t End = Rest.find('\n');
			const std::string Name(Rest.substr(0, End));
			Rest = End == std::string_view::npos ? std::string_view() : Rest.substr(End + 1);
			if (!Name.empty() && (Lists.*Permanent)(Name.c_str()))
				Result.push_back(Name);
		}
		free(List);
		return Result;
	};
	Values.White = Names(Lists.GetBuddyIgnoreText(), &TTalkLists::IsPermanentBuddyIgnore);
	Values.Ignore = Names(Lists.GetIgnoreListText(), &TTalkLists::IsPermanentIgnore);
	Values.KnownHints = KnownHints;
	Values.ShowNewHints = ShowNewHints;
	Values.LevelBar = LevelBar;
	Values.MagLevelBar = MagLevelBar;
	for (std::size_t i = 0; i < SKILLS; i++)
		Values.SkillBars[i] = SkillBars[i];
	return Values;
}

// The options as ReadTibiaCfg reads the file WriteTibiaCfg wrote with Values,
// after SetDefaultPreferences: the lists are appended to. PreferenceKeys.cpp's
// ReadPreferences already did what the original's reader does to the values
// (a hotkey's line breaks, ChaseMode 2, a repeated dialog).
void TPreferences::SetPreferenceValues(const patella::settings::TPreferenceValues &Values, TTalkLists &Lists)
{
	TestServerLogin = Values.TestServer;
	if (Values.FullscreenMode) {
		FullscreenMode.Width = (*Values.FullscreenMode)[0];
		FullscreenMode.Height = (*Values.FullscreenMode)[1];
		FullscreenMode.Depth = (*Values.FullscreenMode)[2];
	}
	WindowsMousepointer = Values.WindowsMousepointer;
	LightEffects = Values.LightEffects;
	TextualEffects = Values.TextualEffects;
	CreatureInfo = Values.CreatureInfo;
	NoStretching = Values.NoStretching ? 1 : 0;
	ConfineFramerate = Values.ConfineFramerate;
	Engine = Values.Engine;
	for (std::size_t i = 0; i < HOTKEYS; i++) {
		if (Values.Hotkeys[i].empty())
			continue;
		Hotkeys[i].Assign(Values.Hotkeys[i].c_str());
		HotkeyAutoSend[i] = Values.HotkeyAutoSend[i];
	}
	WindowedX = Values.WindowedMode[0];
	WindowedY = Values.WindowedMode[1];
	WindowedWidth = Values.WindowedMode[2];
	WindowedHeight = Values.WindowedMode[3];
	WindowedMaximized = Values.WindowedMaximized;
	ConsoleHeight = Values.ConsoleHeight;
	AutomapZoom = Values.AutomapZoom;
	for (const patella::settings::TContentWindowValue &Entry : Values.ContentWindows)
		ContentWindows.push_back({Entry.ID, Entry.Size});
	BuddySortmethods = Values.BuddySortmethods;
	BuddyHideOffline = Values.BuddyHideOffline;
	for (const int Dialog : Values.OpenDialogs) {
		// ReadTibiaCfg's duplicate check, on the vector's fields as it reads them (entries an earlier load
		// left count too; in the client InitPreferences runs once).
		int i = OpenDialogs.min;
		for (;;) {
			if (i >= OpenDialogs.max) {
				OpenDialogs(NumberOfOpenDialogs) = Dialog;
				NumberOfOpenDialogs++;
				break;
			}
			if (OpenDialogs.entry[i++] == Dialog)
				break;
		}
	}
	LastMotD = Values.LastMotD;
	InfoMessages = Values.InfoMessages;
	EventMessages = Values.EventMessages;
	StatusMessages = Values.StatusMessages;
	TimeStamps = Values.TimeStamps;
	PrivateMessages = Values.PrivateMessages;
	OpenChannels.insert(OpenChannels.end(), Values.OpenChannels.begin(), Values.OpenChannels.end());
	ClassicControl = Values.ClassicControl;
	AutoChaseOff = Values.AutoChaseOff;
	AttackMode = Values.AttackMode;
	ChaseMode = Values.ChaseMode;
	SecureMode = Values.SecureMode;
	for (const patella::settings::TBuddyValue &Entry : Values.Buddies) {
		TBuddy &Buddy = Buddies.emplace_back();
		Buddy.CharacterID = Entry.CharacterID;
		Buddy.Icon = Entry.Icon;
		Buddy.AssignDescriptionRead(Entry.Description.c_str());
		Buddy.Notify = Entry.Notify;
	}
	for (const std::string &Name : Values.White)
		Lists.AddBuddyIgnore(Name.c_str(), true);
	for (const std::string &Name : Values.Ignore)
		Lists.IgnoreName(Name.c_str(), true);
	KnownHints.insert(KnownHints.end(), Values.KnownHints.begin(), Values.KnownHints.end());
	ShowNewHints = Values.ShowNewHints;
	LevelBar = Values.LevelBar;
	MagLevelBar = Values.MagLevelBar;
	for (std::size_t i = 0; i < SKILLS; i++)
		SkillBars[i] = Values.SkillBars[i];
}

// Patella-Modern issue #45. Loads the options from Patella.ini; when it has
// none (no PreferencesVersion key), imports Tibia.cfg once: the original's
// reader (ReadTibiaCfg; the defaults without a Tibia.cfg or with one it cannot
// read), then the options go into Patella.ini at once. Tibia.cfg is left as it is.
void TPreferences::InitPreferences(TTalkLists &Lists)
{
	using namespace patella::settings;
	SetDefaultPreferences(Lists);
	if (const std::optional<TSettingsFile> File = Load(FileSystem, FileName, NewFileNewline); File && HasPreferences(*File)) {
		TPreferenceValues Values = GetPreferenceValues(Lists);
		for (const std::string &Problem : ReadPreferences(*File, Values))
			patella::log::Warn("{}: {}", FileName, Problem);
		// Issue #117: a file of version 1 (Windows-1252) is written as version 2 (UTF-8) at the next save.
		if (EncodingOf(*File) == TEncoding::Cp1252)
			patella::log::Info("{}: PreferencesVersion 1 (Windows-1252) read; written as version 2 (UTF-8) at the next save", FileName);
		SetPreferenceValues(Values, Lists);
		PreferencesLoaded = true;
		return;
	}
	const bool Import = FileSystem.Exists("Tibia.cfg");
	ReadTibiaCfg(Lists);
	if (Import)
		patella::log::Info("{}: the options imported from Tibia.cfg (Tibia.cfg is not written)", FileName);
	else
		patella::log::Info("{}: no options and no Tibia.cfg: the default options", FileName);
	SavePreferences(Lists);
}

// Patella-Modern issue #45. Writes the options into Patella.ini (its other keys
// and lines stay), only after InitPreferences loaded them; a failure is
// reported in the log.
void TPreferences::SavePreferences(TTalkLists &Lists)
{
	using namespace patella::settings;
	if (!PreferencesLoaded)
		return;
	std::optional<TSettingsFile> File = LoadForUpdate(FileSystem, FileName);
	if (!File) {
		patella::log::Warn("{} cannot be read: the options are not saved", FileName);
		return;
	}
	WritePreferences(*File, GetPreferenceValues(Lists));
	if (!Save(FileSystem, FileName, *File))
		patella::log::Warn("{} cannot be written: the options are not saved", FileName);
}

// 0x4df520. Frees the message of the day and saves the options (Patella.ini
// since issue #45; Tibia.cfg in the original).
void TPreferences::ExitPreferences(TTalkLists &Lists)
{
	if (MotD != nullptr)
		free(MotD);
	MotD = nullptr;
	SavePreferences(Lists);
}

} // namespace patella

// The original's functions, the bridge of issue #104 (Preferences.h): each calls its member on the one TClient's
// TPreferences.

bool GetTestServerLogin()
{
	return patella::bridge::Preferences().GetTestServerLogin();
}

void SetTestServerLogin(bool Value)
{
	patella::bridge::Preferences().SetTestServerLogin(Value);
}

void SetViewMode(int ViewMode)
{
	patella::bridge::Preferences().SetViewMode(ViewMode);
}

void SetFullscreenMode(const TFullscreenMode *Mode)
{
	patella::bridge::Preferences().SetFullscreenMode(Mode);
}

void SetWindowsMousepointer(bool Value)
{
	patella::bridge::Preferences().SetWindowsMousepointer(Value);
}

void SetLightEffects(bool Value)
{
	patella::bridge::Preferences().SetLightEffects(Value);
}

void SetTextualEffects(bool Value)
{
	patella::bridge::Preferences().SetTextualEffects(Value);
}

void SetCreatureInfo(int Value)
{
	patella::bridge::Preferences().SetCreatureInfo(Value);
}

void SetNoStretching(bool Value)
{
	patella::bridge::Preferences().SetNoStretching(Value);
}

void SetDefaultNoStretching(bool Value)
{
	patella::bridge::Preferences().SetDefaultNoStretching(Value);
}

void SetConfineFramerate(int Value)
{
	patella::bridge::Preferences().SetConfineFramerate(Value);
}

void SetEngine(int Value)
{
	patella::bridge::Preferences().SetEngine(Value);
}

int GetViewMode()
{
	return patella::bridge::Preferences().GetViewMode();
}

TFullscreenMode *GetFullscreenMode()
{
	return patella::bridge::Preferences().GetFullscreenMode();
}

bool GetWindowsMousepointer()
{
	return patella::bridge::Preferences().GetWindowsMousepointer();
}

bool GetLightEffects()
{
	return patella::bridge::Preferences().GetLightEffects();
}

bool GetTextualEffects()
{
	return patella::bridge::Preferences().GetTextualEffects();
}

int GetCreatureInfo()
{
	return patella::bridge::Preferences().GetCreatureInfo();
}

bool GetNoStretching()
{
	return patella::bridge::Preferences().GetNoStretching();
}

int GetConfineFramerate()
{
	return patella::bridge::Preferences().GetConfineFramerate();
}

int GetEngine()
{
	return patella::bridge::Preferences().GetEngine();
}

void SetWindowedMode(int x, int y, int Width, int Height, bool Maximized)
{
	patella::bridge::Preferences().SetWindowedMode(x, y, Width, Height, Maximized);
}

void GetWindowedMode(int *x, int *y, int *Width, int *Height, bool *Maximized)
{
	patella::bridge::Preferences().GetWindowedMode(x, y, Width, Height, Maximized);
}

void SetConsoleHeight(int Value)
{
	patella::bridge::Preferences().SetConsoleHeight(Value);
}

int GetConsoleHeight()
{
	return patella::bridge::Preferences().GetConsoleHeight();
}

void SetAutomapZoom(int Value)
{
	patella::bridge::Preferences().SetAutomapZoom(Value);
}

int GetAutomapZoom()
{
	return patella::bridge::Preferences().GetAutomapZoom();
}

void SetBuddySortmethods(const int *Methods)
{
	patella::bridge::Preferences().SetBuddySortmethods(Methods);
}

void GetBuddySortmethods(int *Methods)
{
	patella::bridge::Preferences().GetBuddySortmethods(Methods);
}

void SetBuddyHideOffline(bool Value)
{
	patella::bridge::Preferences().SetBuddyHideOffline(Value);
}

bool GetBuddyHideOffline()
{
	return patella::bridge::Preferences().GetBuddyHideOffline();
}

void ClearOpenDialogs()
{
	patella::bridge::Preferences().ClearOpenDialogs();
}

const char *GetMotD()
{
	return patella::bridge::Preferences().GetMotD();
}

void SetInfoMessages(bool Value)
{
	patella::bridge::Preferences().SetInfoMessages(Value);
}

void SetEventMessages(bool Value)
{
	patella::bridge::Preferences().SetEventMessages(Value);
}

void SetStatusMessages(bool Value)
{
	patella::bridge::Preferences().SetStatusMessages(Value);
}

void SetTimeStamps(bool Value)
{
	patella::bridge::Preferences().SetTimeStamps(Value);
}

void SetPrivateMessages(bool Value)
{
	patella::bridge::Preferences().SetPrivateMessages(Value);
}

bool GetInfoMessages()
{
	return patella::bridge::Preferences().GetInfoMessages();
}

bool GetEventMessages()
{
	return patella::bridge::Preferences().GetEventMessages();
}

bool GetStatusMessages()
{
	return patella::bridge::Preferences().GetStatusMessages();
}

bool GetTimeStamps()
{
	return patella::bridge::Preferences().GetTimeStamps();
}

bool GetPrivateMessages()
{
	return patella::bridge::Preferences().GetPrivateMessages();
}

void ClearOpenChannels()
{
	patella::bridge::Preferences().ClearOpenChannels();
}

void SetClassicControl(bool Value)
{
	patella::bridge::Preferences().SetClassicControl(Value);
}

void SetAutoChaseOff(bool Value)
{
	patella::bridge::Preferences().SetAutoChaseOff(Value);
}

bool GetClassicControl()
{
	return patella::bridge::Preferences().GetClassicControl();
}

bool GetAutoChaseOff()
{
	return patella::bridge::Preferences().GetAutoChaseOff();
}

void SetAttackMode(int Value)
{
	patella::bridge::Preferences().SetAttackMode(Value);
}

void SetChaseMode(int Value)
{
	patella::bridge::Preferences().SetChaseMode(Value);
}

void SetSecureMode(bool Value)
{
	patella::bridge::Preferences().SetSecureMode(Value);
}

int GetAttackMode()
{
	return patella::bridge::Preferences().GetAttackMode();
}

int GetChaseMode()
{
	return patella::bridge::Preferences().GetChaseMode();
}

bool GetSecureMode()
{
	return patella::bridge::Preferences().GetSecureMode();
}

void SetShowNewHints(bool Value)
{
	patella::bridge::Preferences().SetShowNewHints(Value);
}

bool GetShowNewHints()
{
	return patella::bridge::Preferences().GetShowNewHints();
}

bool GetLevelBar()
{
	return patella::bridge::Preferences().GetLevelBar();
}

bool GetMagLevelBar()
{
	return patella::bridge::Preferences().GetMagLevelBar();
}

void SetLevelBar(bool Value)
{
	patella::bridge::Preferences().SetLevelBar(Value);
}

void SetMagLevelBar(bool Value)
{
	patella::bridge::Preferences().SetMagLevelBar(Value);
}

void SetDefaultPreferences()
{
	patella::bridge::Preferences().SetDefaultPreferences(patella::bridge::TalkLists());
}

void SetContentWindowSize(int ID, int Size)
{
	patella::bridge::Preferences().SetContentWindowSize(ID, Size);
}

int GetContentWindowSize(int ID)
{
	return patella::bridge::Preferences().GetContentWindowSize(ID);
}

int GetOpenDialogs(int **List)
{
	return patella::bridge::Preferences().GetOpenDialogs(List);
}

int GetOpenChannels(int **List)
{
	return patella::bridge::Preferences().GetOpenChannels(List);
}

void AddKnownHint(int Hint)
{
	patella::bridge::Preferences().AddKnownHint(Hint);
}

bool IsKnownHint(int Hint)
{
	return patella::bridge::Preferences().IsKnownHint(Hint);
}

void SetHotkey(int Number, const char *Text, bool AutoSend)
{
	patella::bridge::Preferences().SetHotkey(Number, Text, AutoSend);
}

const char *GetHotkey(int Number, bool *AutoSend)
{
	return patella::bridge::Preferences().GetHotkey(Number, AutoSend);
}

void AddOpenDialog(int Identifier)
{
	patella::bridge::Preferences().AddOpenDialog(Identifier);
}

bool SetMotD(const char *Text)
{
	return patella::bridge::Preferences().SetMotD(Text);
}

void AddOpenChannel(int Identifier)
{
	patella::bridge::Preferences().AddOpenChannel(Identifier);
}

int FindBuddy(int CharacterID)
{
	return patella::bridge::Preferences().FindBuddy(CharacterID);
}

void SetBuddy(int CharacterID, int Icon, const char *Description, bool Notify)
{
	patella::bridge::Preferences().SetBuddy(CharacterID, Icon, Description, Notify);
}

void DeleteBuddy(int CharacterID)
{
	patella::bridge::Preferences().DeleteBuddy(CharacterID);
}

int GetBuddyIcon(int CharacterID)
{
	return patella::bridge::Preferences().GetBuddyIcon(CharacterID);
}

const char *GetBuddyDescription(int CharacterID)
{
	return patella::bridge::Preferences().GetBuddyDescription(CharacterID);
}

bool GetBuddyNotify(int CharacterID)
{
	return patella::bridge::Preferences().GetBuddyNotify(CharacterID);
}

bool GetSkillBar(int SkillNumber)
{
	return patella::bridge::Preferences().GetSkillBar(SkillNumber);
}

void SetSkillBar(int SkillNumber, bool Value)
{
	patella::bridge::Preferences().SetSkillBar(SkillNumber, Value);
}

void ReadTibiaCfg()
{
	patella::bridge::Preferences().ReadTibiaCfg(patella::bridge::TalkLists());
}

void WriteTibiaCfg()
{
	patella::bridge::Preferences().WriteTibiaCfg(patella::bridge::TalkLists());
}

patella::settings::TPreferenceValues GetPreferenceValues()
{
	return patella::bridge::Preferences().GetPreferenceValues(patella::bridge::TalkLists());
}

void SetPreferenceValues(const patella::settings::TPreferenceValues &Values)
{
	patella::bridge::Preferences().SetPreferenceValues(Values, patella::bridge::TalkLists());
}

void InitPreferences()
{
	patella::bridge::Preferences().InitPreferences(patella::bridge::TalkLists());
}

void SavePreferences()
{
	patella::bridge::Preferences().SavePreferences(patella::bridge::TalkLists());
}

void ExitPreferences()
{
	patella::bridge::Preferences().ExitPreferences(patella::bridge::TalkLists());
}
// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
