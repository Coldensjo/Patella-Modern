// Preferences.cpp: the options stored in Tibia.cfg. The option names in the
// comments are the keys that the loader and saver after Preferences.cpp
// (0x4ddc00, 0x4de890) use, which give the names of the globals.
//
// Names are ours unless a comment says they come from the binary.

#pragma once

#include "common.h" // IWYU pragma: keep (first: it configures the CRT for the C headers of types/FixedString.h)
#include "types/FixedString.h"
#include <cstddef>

// From the assertions.
inline constexpr int HOTKEYS = 36;
inline constexpr int HOTKEY_LENGTH = 256;
inline constexpr int BUDDYDESCRIPTION_LENGTH = 128;
inline constexpr int SKILLS = 7;

// "FullscreenMode = (Width,Height,Depth)"; the fourth value is not saved.
struct TFullscreenMode {
	int Width;
	int Height;
	int Depth;
	int Frequency;
};

// "ContentWindow = (ID,Size)": the stored size of a side-bar window.
struct TContentWindowSize {
	int ID;
	int Size;
};

// "Buddy = (CharacterID,Icon,Description,Notify)", 0x8c bytes. Notify
// shows "%s has logged in."; CharacterID 0 marks a deleted entry.
struct TBuddy {
	// strcpy(Description, Text) as the original's Tibia.cfg reader wrote it (issue #85): the text and its NUL
	// within the rest of the entry (Description, Notify and the padding, 132 bytes). A description of 128
	// characters is kept whole, its NUL in Notify, which the reader sets next, and up to 131 characters the
	// bytes are the original's; a longer one, which the original wrote past the entry, is cut at 131.
	void AssignDescriptionRead(const char *Text) noexcept
	{
		TStringBuffer(reinterpret_cast<char *>(this) + offsetof(TBuddy, Description), sizeof(TBuddy) - offsetof(TBuddy, Description))
			.Assign(Text);
	}

	int CharacterID;
	int Icon;
	TFixedString<BUDDYDESCRIPTION_LENGTH> Description;
	bool Notify;
};
// The original's layout (no pointers: the same on x64), with Description a TFixedString (issue #57).
static_assert(sizeof(TBuddy) == 0x8c && offsetof(TBuddy, Description) == 0x08 && offsetof(TBuddy, Notify) == 0x88,
	"TBuddy's layout");

bool GetTestServerLogin();
void SetTestServerLogin(bool Value);
void SetViewMode(int ViewMode);
void SetFullscreenMode(const TFullscreenMode *Mode);
void SetWindowsMousepointer(bool Value);
void SetLightEffects(bool Value);
void SetTextualEffects(bool Value);
void SetCreatureInfo(int Value);
void SetNoStretching(bool Value);
void SetDefaultNoStretching(bool Value);
void SetConfineFramerate(int Value);
void SetEngine(int Value);
int GetViewMode();
TFullscreenMode *GetFullscreenMode();
bool GetWindowsMousepointer();
bool GetLightEffects();
bool GetTextualEffects();
int GetCreatureInfo();
bool GetNoStretching();
int GetConfineFramerate();
int GetEngine();
void SetWindowedMode(int x, int y, int Width, int Height, bool Maximized);
void GetWindowedMode(int *x, int *y, int *Width, int *Height, bool *Maximized);
void SetConsoleHeight(int Value);
int GetConsoleHeight();
void SetAutomapZoom(int Value);
int GetAutomapZoom();
void SetBuddySortmethods(const int *Methods);
void GetBuddySortmethods(int *Methods);
void SetBuddyHideOffline(bool Value);
bool GetBuddyHideOffline();
void ClearOpenDialogs();
const char *GetMotD();
void SetInfoMessages(bool Value);
void SetEventMessages(bool Value);
void SetStatusMessages(bool Value);
void SetTimeStamps(bool Value);
void SetPrivateMessages(bool Value);
bool GetInfoMessages();
bool GetEventMessages();
bool GetStatusMessages();
bool GetTimeStamps();
bool GetPrivateMessages();
void ClearOpenChannels();
void SetClassicControl(bool Value);
void SetAutoChaseOff(bool Value);
bool GetClassicControl();
bool GetAutoChaseOff();
void SetAttackMode(int Value);
void SetChaseMode(int Value);
void SetSecureMode(bool Value);
int GetAttackMode();
int GetChaseMode();
bool GetSecureMode();
void SetShowNewHints(bool Value);
bool GetShowNewHints();
bool GetLevelBar();
bool GetMagLevelBar();
void SetLevelBar(bool Value);
void SetMagLevelBar(bool Value);
void SetDefaultPreferences();
void SetContentWindowSize(int ID, int Size);
int GetContentWindowSize(int ID);
int GetOpenDialogs(int **List);
int GetOpenChannels(int **List);
void AddKnownHint(int Hint);
bool IsKnownHint(int Hint);
void SetHotkey(int Number, const char *Text, bool AutoSend);
const char *GetHotkey(int Number, bool *AutoSend);
void AddOpenDialog(int Identifier);
bool SetMotD(const char *Text);
void AddOpenChannel(int Identifier);
int FindBuddy(int CharacterID);
void SetBuddy(int CharacterID, int Icon, const char *Description, bool Notify);
void DeleteBuddy(int CharacterID);
int GetBuddyIcon(int CharacterID);
const char *GetBuddyDescription(int CharacterID);
bool GetBuddyNotify(int CharacterID);
bool GetSkillBar(int SkillNumber);
void SetSkillBar(int SkillNumber, bool Value);
void InitPreferences();
void SavePreferences();
void ExitPreferences();
