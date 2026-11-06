// Preferences.cpp: the options stored in Tibia.cfg by the original, in
// Patella.ini by the x64 client (Patella-Modern issue #45). The option names in
// the comments are the keys that the loader and saver after Preferences.cpp
// (0x4ddc00, 0x4de890) use, which give the names of the globals.
//
// Names are ours unless a comment says they come from the binary.
//
// Patella-Modern issue #104 (M7, notes/decisions.md, "#104: TClient and the subsystem objects"): the data is
// TPreferences', which TClient owns (Client.h); the free functions of the original's names are the bridge
// that the callers use until they hold the object (bridge::Preferences(), defined in Client.cpp).

#pragma once

#include "common.h" // IWYU pragma: keep (first: it configures the CRT for the C headers of types/FixedString.h)
#include "Container.h"
#include "settings/PreferenceKeys.h"
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

namespace patella {

// The two lists of Talk.cpp that the options hold (Tibia.cfg's White and Ignore; issue #104): TPreferences reaches
// them only through this, so that it does not depend on Talk. The functions are Talk.h's of the same names.
class TTalkLists {
public:
	TTalkLists() = default;
	TTalkLists(const TTalkLists &) = delete;
	TTalkLists &operator=(const TTalkLists &) = delete;
	TTalkLists(TTalkLists &&) = delete;
	TTalkLists &operator=(TTalkLists &&) = delete;
	virtual void ClearIgnoreList() = 0;
	virtual char *GetBuddyIgnoreText() = 0;
	virtual void AddBuddyIgnore(const char *Addressee, bool Permanent) = 0;
	virtual bool IsPermanentBuddyIgnore(const char *Addressee) = 0;
	virtual char *GetIgnoreListText() = 0;
	virtual void IgnoreName(const char *Addressee, bool Permanent) = 0;
	virtual bool IsPermanentIgnore(const char *Addressee) = 0;

protected:
	~TTalkLists() = default;
};

// Preferences.cpp's data and functions (issue #104): the globals of the original (0x719e70-0x71c44f, 0x5c2558) are
// the members, with the same initial values (the others 0, as the original's .bss), and the functions the members
// of the same names. The ones that reach Talk's lists take them as Lists.
class TPreferences {
public:
	TPreferences() = default;
	TPreferences(const TPreferences &) = delete;
	TPreferences &operator=(const TPreferences &) = delete;
	TPreferences(TPreferences &&) = delete;
	TPreferences &operator=(TPreferences &&) = delete;
	~TPreferences() = default;

	bool GetTestServerLogin() const;
	void SetTestServerLogin(bool Value);
	void SetViewMode(int Value);
	void SetFullscreenMode(const TFullscreenMode *Mode);
	void SetWindowsMousepointer(bool Value);
	void SetLightEffects(bool Value);
	void SetTextualEffects(bool Value);
	void SetCreatureInfo(int Value);
	void SetNoStretching(bool Value);
	void SetDefaultNoStretching(bool Value);
	void SetConfineFramerate(int Value);
	void SetEngine(int Value);
	int GetViewMode() const;
	TFullscreenMode *GetFullscreenMode();
	bool GetWindowsMousepointer() const;
	bool GetLightEffects() const;
	bool GetTextualEffects() const;
	int GetCreatureInfo() const;
	bool GetNoStretching() const;
	int GetConfineFramerate() const;
	int GetEngine() const;
	void SetWindowedMode(int x, int y, int Width, int Height, bool Maximized);
	void GetWindowedMode(int *x, int *y, int *Width, int *Height, bool *Maximized) const;
	void SetConsoleHeight(int Value);
	int GetConsoleHeight() const;
	void SetAutomapZoom(int Value);
	int GetAutomapZoom() const;
	void SetBuddySortmethods(const int *Methods);
	void GetBuddySortmethods(int *Methods) const;
	void SetBuddyHideOffline(bool Value);
	bool GetBuddyHideOffline() const;
	void ClearOpenDialogs();
	const char *GetMotD() const;
	void SetInfoMessages(bool Value);
	void SetEventMessages(bool Value);
	void SetStatusMessages(bool Value);
	void SetTimeStamps(bool Value);
	void SetPrivateMessages(bool Value);
	bool GetInfoMessages() const;
	bool GetEventMessages() const;
	bool GetStatusMessages() const;
	bool GetTimeStamps() const;
	bool GetPrivateMessages() const;
	void ClearOpenChannels();
	void SetClassicControl(bool Value);
	void SetAutoChaseOff(bool Value);
	bool GetClassicControl() const;
	bool GetAutoChaseOff() const;
	void SetAttackMode(int Value);
	void SetChaseMode(int Value);
	void SetSecureMode(bool Value);
	int GetAttackMode() const;
	int GetChaseMode() const;
	bool GetSecureMode() const;
	void SetShowNewHints(bool Value);
	bool GetShowNewHints() const;
	bool GetLevelBar() const;
	bool GetMagLevelBar() const;
	void SetLevelBar(bool Value);
	void SetMagLevelBar(bool Value);
	void SetDefaultPreferences(TTalkLists &Lists);
	void SetContentWindowSize(int ID, int Size);
	int GetContentWindowSize(int ID);
	int GetOpenDialogs(int **List);
	int GetOpenChannels(int **List);
	void AddKnownHint(int Hint);
	bool IsKnownHint(int Hint);
	void SetHotkey(int Number, const char *Text, bool AutoSend);
	const char *GetHotkey(int Number, bool *AutoSend) const;
	void AddOpenDialog(int Identifier);
	bool SetMotD(const char *Text);
	void AddOpenChannel(int Identifier);
	int FindBuddy(int CharacterID);
	void SetBuddy(int CharacterID, int Icon, const char *Description, bool Notify);
	void DeleteBuddy(int CharacterID);
	int GetBuddyIcon(int CharacterID);
	const char *GetBuddyDescription(int CharacterID);
	bool GetBuddyNotify(int CharacterID);
	bool GetSkillBar(int SkillNumber) const;
	void SetSkillBar(int SkillNumber, bool Value);
	void ReadTibiaCfg(TTalkLists &Lists);
	void WriteTibiaCfg(TTalkLists &Lists);
	patella::settings::TPreferenceValues GetPreferenceValues(TTalkLists &Lists);
	void SetPreferenceValues(const patella::settings::TPreferenceValues &Values, TTalkLists &Lists);
	void InitPreferences(TTalkLists &Lists);
	void SavePreferences(TTalkLists &Lists);
	void ExitPreferences(TTalkLists &Lists);

private:
	// Data, in the original's .data/.bss order.
	int WindowedWidth = 600;                                    // 0x5c2558, "WindowedMode = (x,y,Width,Height,Maximized)"
	int WindowedHeight = 400;                                   // 0x5c255c
	std::array<bool, SKILLS> SkillBars{};                       // 0x719e70, "SkillBars = {...}"
	bool MagLevelBar{};                                         // 0x719e77
	bool LevelBar{};                                            // 0x719e78
	bool ShowNewHints{};                                        // 0x719e79
	int NumberOfKnownHints{};                                   // 0x719e7c, "KnownHints = {...}"
	int NumberOfBuddyEntries{};                                 // 0x719e80, "Buddy = (...)", deleted entries included
	bool SecureMode{};                                          // 0x719e84
	int ChaseMode{};                                            // 0x719e88
	int AttackMode{};                                           // 0x719e8c
	bool AutoChaseOff{};                                        // 0x719e90
	bool ClassicControl{};                                      // 0x719e91
	int NumberOfOpenChannels{};                                 // 0x719e94, "OpenChannel = ..."
	bool PrivateMessages{};                                     // 0x719e98
	bool TimeStamps{};                                          // 0x719e99
	bool StatusMessages{};                                      // 0x719e9a
	bool EventMessages{};                                       // 0x719e9b
	bool InfoMessages{};                                        // 0x719e9c
	char *MotD = nullptr;                                       // 0x719ea0, text of the message of the day
	int LastMotD{};                                             // 0x719ea4, its number
	int NumberOfOpenDialogs{};                                  // 0x719ea8, "OpenDialogs = ..."
	bool BuddyHideOffline{};                                    // 0x719eac
	std::array<int, 3> BuddySortmethods{};                      // 0x719eb0
	int NumberOfContentWindows{};                               // 0x719ebc, "ContentWindow = (...)"
	int AutomapZoom{};                                          // 0x719ec0
	int ConsoleHeight{};                                        // 0x719ec4
	bool WindowedMaximized{};                                   // 0x719ec8
	std::array<bool, HOTKEYS> HotkeyAutoSend{};                 // 0x719ecc, saved as a '\n' after the text
	std::array<TFixedString<HOTKEY_LENGTH>, HOTKEYS> Hotkeys{}; // 0x719ef0, "Hotkey = (Number,Text)"
	int Engine{};                                               // 0x71c2f0, -1 = not chosen
	int ConfineFramerate{};                                     // 0x71c2f4
	int NoStretching{};                                         // 0x71c2f8, 0 or 1, 2 = not set yet
	int CreatureInfo{};                                         // 0x71c2fc
	bool TextualEffects{};                                      // 0x71c300
	bool LightEffects{};                                        // 0x71c301
	bool WindowsMousepointer{};                                 // 0x71c302
	TFullscreenMode FullscreenMode{};                           // 0x71c304, Width 0 = not set
	int ViewMode{};                                             // 0x71c314, "ViewMode" in a MainFrm.cpp catch block
	bool TestServerLogin{};                                     // 0x71c318, "TestServer = yes"
	bool PreferencesLoaded{};                                   // 0x71c319, set by InitPreferences; SavePreferences writes only then
	int WindowedX{};                                            // 0x71c31c
	int WindowedY{};                                            // 0x71c320
	vector<TContentWindowSize> ContentWindows{0, 9, 10};        // 0x71c324, initialiser at 0x55a050
	vector<int> OpenDialogs{0, 9, 10, -1};                      // 0x71c348, initialiser at 0x55a070
	vector<int> OpenChannels{0, 9, 10};                         // 0x71c368, initialiser at 0x55a090
	vector<TBuddy> Buddies{0, 9, 10};                           // 0x71c388, initialiser at 0x55a0b0
	vector<int> KnownHints{0, 9, 10};                           // 0x71c430, initialiser at 0x55a0d0

	// The arrays std::arrays (issue #60).
	static_assert(sizeof(SkillBars) == SKILLS && sizeof(BuddySortmethods) == 3 * sizeof(int)
			&& sizeof(HotkeyAutoSend) == HOTKEYS && sizeof(Hotkeys) == HOTKEYS * HOTKEY_LENGTH,
		"Preferences' arrays");
};

// The transition's access path (issue #104; notes/decisions.md, "#104: TClient and the subsystem objects"): the
// options of the one TClient (Client.cpp), and the Talk lists they hold. They go when no free function needs them.
namespace bridge {
TPreferences &Preferences();
TTalkLists &TalkLists();
} // namespace bridge

} // namespace patella

// The original's functions (the callers' names, and the golden tests'): each calls TPreferences' member of the
// same name on bridge::Preferences(), with bridge::TalkLists() where it takes the lists.
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
// The original's Tibia.cfg reader and writer (0x4ddc00, 0x4de890; issue #45): the import and the golden
// corpus's reference. The client calls InitPreferences, SavePreferences and ExitPreferences, which read and
// write Patella.ini (settings/PreferenceKeys.h); it never writes Tibia.cfg.
void ReadTibiaCfg();
void WriteTibiaCfg();
patella::settings::TPreferenceValues GetPreferenceValues();
void SetPreferenceValues(const patella::settings::TPreferenceValues &Values);
void InitPreferences();
void SavePreferences();
void ExitPreferences();
