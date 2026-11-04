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

#include "Preferences.h"
#include "Script.h"
#include "Talk.h"
#include "Utils.h"
#include "Container.h"
#include "common.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace {

// Data, in .data/.bss order.
int WindowedWidth = 600;                             // 0x5c2558, "WindowedMode = (x,y,Width,Height,Maximized)"
int WindowedHeight = 400;                            // 0x5c255c
bool SkillBars[SKILLS];                              // 0x719e70, "SkillBars = {...}"
bool MagLevelBar;                                    // 0x719e77
bool LevelBar;                                       // 0x719e78
bool ShowNewHints;                                   // 0x719e79
int NumberOfKnownHints;                              // 0x719e7c, "KnownHints = {...}"
int NumberOfBuddyEntries;                            // 0x719e80, "Buddy = (...)", deleted entries included
bool SecureMode;                                     // 0x719e84
int ChaseMode;                                       // 0x719e88
int AttackMode;                                      // 0x719e8c
bool AutoChaseOff;                                   // 0x719e90
bool ClassicControl;                                 // 0x719e91
int NumberOfOpenChannels;                            // 0x719e94, "OpenChannel = ..."
bool PrivateMessages;                                // 0x719e98
bool TimeStamps;                                     // 0x719e99
bool StatusMessages;                                 // 0x719e9a
bool EventMessages;                                  // 0x719e9b
bool InfoMessages;                                   // 0x719e9c
char *MotD;                                          // 0x719ea0, text of the message of the day
int LastMotD;                                        // 0x719ea4, its number
int NumberOfOpenDialogs;                             // 0x719ea8, "OpenDialogs = ..."
bool BuddyHideOffline;                               // 0x719eac
int BuddySortmethods[3];                             // 0x719eb0
int NumberOfContentWindows;                          // 0x719ebc, "ContentWindow = (...)"
int AutomapZoom;                                     // 0x719ec0
int ConsoleHeight;                                   // 0x719ec4
bool WindowedMaximized;                              // 0x719ec8
bool HotkeyAutoSend[HOTKEYS];                        // 0x719ecc, saved as a '\n' after the text
char Hotkeys[HOTKEYS][HOTKEY_LENGTH];                // 0x719ef0, "Hotkey = (Number,Text)"
int Engine;                                          // 0x71c2f0, -1 = not chosen
int ConfineFramerate;                                // 0x71c2f4
int NoStretching;                                    // 0x71c2f8, 0 or 1, 2 = not set yet
int CreatureInfo;                                    // 0x71c2fc
bool TextualEffects;                                 // 0x71c300
bool LightEffects;                                   // 0x71c301
bool WindowsMousepointer;                            // 0x71c302
TFullscreenMode FullscreenMode;                      // 0x71c304, Width 0 = not set
int ViewMode;                                        // 0x71c314, "ViewMode" in a MainFrm.cpp catch block
bool TestServerLogin;                                // 0x71c318, "TestServer = yes"
bool PreferencesLoaded;                              // 0x71c319, set by InitPreferences; SavePreferences writes only then
int WindowedX;                                       // 0x71c31c
int WindowedY;                                       // 0x71c320
vector<TContentWindowSize> ContentWindows(0, 9, 10); // 0x71c324, initialiser at 0x55a050
vector<int> OpenDialogs(0, 9, 10, -1);               // 0x71c348, initialiser at 0x55a070
vector<int> OpenChannels(0, 9, 10);                  // 0x71c368, initialiser at 0x55a090
vector<TBuddy> Buddies(0, 9, 10);                    // 0x71c388, initialiser at 0x55a0b0
vector<int> KnownHints(0, 9, 10);                    // 0x71c430, initialiser at 0x55a0d0

} // namespace

// 0x4db2a0
bool GetTestServerLogin()
{
	return TestServerLogin;
}

// 0x4db2b0
void SetTestServerLogin(bool Value)
{
	TestServerLogin = Value;
}

// 0x4db2c0
void SetViewMode(int ViewMode)
{
	::ViewMode = ViewMode;
}

// 0x4db2d0
void SetFullscreenMode(const TFullscreenMode *Mode)
{
	FullscreenMode = *Mode;
}

// 0x4db300
void SetWindowsMousepointer(bool Value)
{
	WindowsMousepointer = Value;
}

// 0x4db310
void SetLightEffects(bool Value)
{
	LightEffects = Value;
}

// 0x4db320
void SetTextualEffects(bool Value)
{
	TextualEffects = Value;
}

// 0x4db330
void SetCreatureInfo(int Value)
{
	CreatureInfo = Value;
}

// 0x4db340
void SetNoStretching(bool Value)
{
	NoStretching = Value ? 1 : 0;
}

// 0x4db360. Only takes effect while the option has never been set.
void SetDefaultNoStretching(bool Value)
{
	if (NoStretching == 2)
		NoStretching = Value ? 1 : 0;
}

// 0x4db380
void SetConfineFramerate(int Value)
{
	ConfineFramerate = Value;
}

// 0x4db390
void SetEngine(int Value)
{
	Engine = Value;
}

// 0x4db3a0
int GetViewMode()
{
	return ViewMode;
}

// 0x4db3b0
TFullscreenMode *GetFullscreenMode()
{
	return &FullscreenMode;
}

// 0x4db3c0
bool GetWindowsMousepointer()
{
	return WindowsMousepointer;
}

// 0x4db3d0
bool GetLightEffects()
{
	return LightEffects;
}

// 0x4db3e0
bool GetTextualEffects()
{
	return TextualEffects;
}

// 0x4db3f0
int GetCreatureInfo()
{
	return CreatureInfo;
}

// 0x4db400. "Not set yet" (2) reads as false.
bool GetNoStretching()
{
	return NoStretching == 1;
}

// 0x4db410
int GetConfineFramerate()
{
	return ConfineFramerate;
}

// 0x4db420
int GetEngine()
{
	return Engine;
}

// 0x4db430
void SetWindowedMode(int x, int y, int Width, int Height, bool Maximized)
{
	WindowedX = x;
	WindowedY = y;
	WindowedWidth = Width;
	WindowedHeight = Height;
	WindowedMaximized = Maximized;
}

// 0x4db470
void GetWindowedMode(int *x, int *y, int *Width, int *Height, bool *Maximized)
{
	*x = WindowedX;
	*y = WindowedY;
	*Width = WindowedWidth;
	*Height = WindowedHeight;
	*Maximized = WindowedMaximized;
}

// 0x4db4b0
void SetConsoleHeight(int Value)
{
	ConsoleHeight = Value;
}

// 0x4db4c0
int GetConsoleHeight()
{
	return ConsoleHeight;
}

// 0x4db4d0
void SetAutomapZoom(int Value)
{
	AutomapZoom = Value;
}

// 0x4db4e0
int GetAutomapZoom()
{
	return AutomapZoom;
}

// 0x4db4f0
void SetBuddySortmethods(const int *Methods)
{
	BuddySortmethods[0] = Methods[0];
	BuddySortmethods[1] = Methods[1];
	BuddySortmethods[2] = Methods[2];
}

// 0x4db510
void GetBuddySortmethods(int *Methods)
{
	Methods[0] = BuddySortmethods[0];
	Methods[1] = BuddySortmethods[1];
	Methods[2] = BuddySortmethods[2];
}

// 0x4db530
void SetBuddyHideOffline(bool Value)
{
	BuddyHideOffline = Value;
}

// 0x4db540
bool GetBuddyHideOffline()
{
	return BuddyHideOffline;
}

// 0x4db550
void ClearOpenDialogs()
{
	NumberOfOpenDialogs = 0;
}

// 0x4db560
const char *GetMotD()
{
	if (MotD == nullptr)
		return "No current information.";
	return MotD;
}

// 0x4db570
void SetInfoMessages(bool Value)
{
	InfoMessages = Value;
}

// 0x4db580
void SetEventMessages(bool Value)
{
	EventMessages = Value;
}

// 0x4db590
void SetStatusMessages(bool Value)
{
	StatusMessages = Value;
}

// 0x4db5a0
void SetTimeStamps(bool Value)
{
	TimeStamps = Value;
}

// 0x4db5b0
void SetPrivateMessages(bool Value)
{
	PrivateMessages = Value;
}

// 0x4db5c0
bool GetInfoMessages()
{
	return InfoMessages;
}

// 0x4db5d0
bool GetEventMessages()
{
	return EventMessages;
}

// 0x4db5e0
bool GetStatusMessages()
{
	return StatusMessages;
}

// 0x4db5f0
bool GetTimeStamps()
{
	return TimeStamps;
}

// 0x4db600
bool GetPrivateMessages()
{
	return PrivateMessages;
}

// 0x4db610
void ClearOpenChannels()
{
	NumberOfOpenChannels = 0;
}

// 0x4db620
void SetClassicControl(bool Value)
{
	ClassicControl = Value;
}

// 0x4db630
void SetAutoChaseOff(bool Value)
{
	AutoChaseOff = Value;
}

// 0x4db640
bool GetClassicControl()
{
	return ClassicControl;
}

// 0x4db650
bool GetAutoChaseOff()
{
	return AutoChaseOff;
}

// 0x4db660
void SetAttackMode(int Value)
{
	AttackMode = Value;
}

// 0x4db670
void SetChaseMode(int Value)
{
	ChaseMode = Value;
}

// 0x4db680. A byte, although it is saved as a number like AttackMode.
void SetSecureMode(bool Value)
{
	SecureMode = Value;
}

// 0x4db690
int GetAttackMode()
{
	return AttackMode;
}

// 0x4db6a0
int GetChaseMode()
{
	return ChaseMode;
}

// 0x4db6b0
bool GetSecureMode()
{
	return SecureMode;
}

// 0x4db6c0
void SetShowNewHints(bool Value)
{
	ShowNewHints = Value;
}

// 0x4db6d0
bool GetShowNewHints()
{
	return ShowNewHints;
}

// 0x4db6e0
bool GetLevelBar()
{
	return LevelBar;
}

// 0x4db6f0
bool GetMagLevelBar()
{
	return MagLevelBar;
}

// 0x4db700
void SetLevelBar(bool Value)
{
	LevelBar = Value;
}

// 0x4db710
void SetMagLevelBar(bool Value)
{
	MagLevelBar = Value;
}

// 0x4db720. Not reset: the windowed position and size, and the hotkey texts
// past their first byte. MotD is cleared without being freed.
void SetDefaultPreferences()
{
	memset(HotkeyAutoSend, 0, sizeof(HotkeyAutoSend));
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
	NumberOfContentWindows = 0;
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
	NumberOfOpenChannels = 0;
	ClassicControl = false;
	AutoChaseOff = true;
	AttackMode = 2;
	ChaseMode = 0;
	SecureMode = true;
	NumberOfBuddyEntries = 0;
	NumberOfKnownHints = 0;
	ShowNewHints = true;
	LevelBar = true;
	MagLevelBar = true;
	for (int i = 0; i < SKILLS; i++)
		SkillBars[i] = true;
	TestServerLogin = false;
	ClearIgnoreList();
}

// 0x4db880: vector<TContentWindowSize>::operator() (Container.h).
// 0x4dba60: vector<TBuddy>::operator() (Container.h).

// 0x4dbcd0. Updates the entry for ID or appends one.
void SetContentWindowSize(int ID, int Size)
{
	for (int i = 0; i < NumberOfContentWindows; i++) {
		if (ContentWindows(i).ID == ID) {
			ContentWindows(i).Size = Size;
			return;
		}
	}
	ContentWindows(NumberOfContentWindows).ID = ID;
	ContentWindows(NumberOfContentWindows).Size = Size;
	NumberOfContentWindows++;
}

// 0x4dbd50. 0 for an unknown ID.
int GetContentWindowSize(int ID)
{
	for (int i = 0; i < NumberOfContentWindows; i++) {
		if (ContentWindows(i).ID == ID)
			return ContentWindows(i).Size;
	}
	return 0;
}

// 0x4dbd90. Returns a new array in *List (left unchanged when there are
// none) and the number of entries.
int GetOpenDialogs(int **List)
{
	if (NumberOfOpenDialogs == 0)
		return 0;
	*List = new int[NumberOfOpenDialogs];
	for (int i = 0; i < NumberOfOpenDialogs; i++)
		(*List)[i] = OpenDialogs(i);
	return NumberOfOpenDialogs;
}

// 0x4dbde0. Like GetOpenDialogs.
int GetOpenChannels(int **List)
{
	if (NumberOfOpenChannels == 0)
		return 0;
	*List = new int[NumberOfOpenChannels];
	for (int i = 0; i < NumberOfOpenChannels; i++)
		(*List)[i] = OpenChannels(i);
	return NumberOfOpenChannels;
}

// 0x4dbe30. Does not check for duplicates.
void AddKnownHint(int Hint)
{
	KnownHints(NumberOfKnownHints) = Hint;
	NumberOfKnownHints++;
}

// 0x4dbe50
bool IsKnownHint(int Hint)
{
	for (int i = 0; i < NumberOfKnownHints; i++) {
		if (KnownHints(i) == Hint)
			return true;
	}
	return false;
}

// 0x4dbe90: vector<TContentWindowSize>::vector(min, max, block) (Container.h).
// 0x4dc170: vector<TBuddy>::vector(min, max, block) (Container.h).

// 0x4dc450, lines 223-225
void SetHotkey(int Number, const char *Text, bool AutoSend)
{
	TASSERT1(223, In(Number,0,HOTKEYS-1), Number);
	TASSERT(224, Text!=NULL);
	TASSERT1(225, strlen(Text)<HOTKEY_LENGTH, strlen(Text));
	strcpy(Hotkeys[Number], Text);
	HotkeyAutoSend[Number] = AutoSend;
}

// 0x4dc7e0, line 235
const char *GetHotkey(int Number, bool *AutoSend)
{
	TASSERT1(235, In(Number,0,HOTKEYS-1), Number);
	*AutoSend = HotkeyAutoSend[Number];
	return Hotkeys[Number];
}

// 0x4dc950, line 393
void AddOpenDialog(int Identifier)
{
	TASSERT1(393, Identifier>=0, Identifier);
	OpenDialogs(NumberOfOpenDialogs) = Identifier;
	NumberOfOpenDialogs++;
}

// 0x4dcac0, line 425. Text is "<number>\n<message>"; without a line break
// nothing changes. Returns true when the number differs from the last one.
bool SetMotD(const char *Text)
{
	TASSERT(425, Text != NULL);
	const char *Break = strchr(Text, '\n');
	if (Break == nullptr)
		return false;
	const int Number = atoi(Text);
	const bool New = Number != LastMotD;
	LastMotD = Number;
	if (MotD != nullptr)
		delete[] MotD;
	MotD = new char[strlen(Break + 1) + 1];
	strcpy(MotD, Break + 1);
	return New;
}

// 0x4dccb0, line 551
void AddOpenChannel(int Identifier)
{
	TASSERT1(551, Identifier>=0, Identifier);
	OpenChannels(NumberOfOpenChannels) = Identifier;
	NumberOfOpenChannels++;
}

// 0x4dce20, line 680. Index of the buddy entry, -1 if there is none.
int FindBuddy(int CharacterID)
{
	TASSERT1(680, CharacterID!=0, CharacterID);
	for (int i = 0; i < NumberOfBuddyEntries; i++) {
		if (Buddies(i).CharacterID == CharacterID)
			return i;
	}
	return -1;
}

// 0x4dcfb0, lines 691-693. Deleted entries are not reused.
void SetBuddy(int CharacterID, int Icon, const char *Description, bool Notify)
{
	TASSERT1(691, CharacterID!=0, CharacterID);
	TASSERT(692, Description!=NULL);
	TASSERT1(693, strlen(Description)<BUDDYDESCRIPTION_LENGTH, strlen(Description));
	int Number = FindBuddy(CharacterID);
	if (Number == -1)
		Number = NumberOfBuddyEntries++;
	TBuddy *Buddy = &Buddies(Number);
	Buddy->CharacterID = CharacterID;
	Buddy->Icon = Icon;
	strcpy(Buddy->Description, Description);
	Buddy->Notify = Notify;
}

// 0x4dd360, line 711. Only marks the entry as free.
void DeleteBuddy(int CharacterID)
{
	TASSERT1(711, CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number != -1)
		Buddies(Number).CharacterID = 0;
}

// 0x4dd4c0, line 722
int GetBuddyIcon(int CharacterID)
{
	TASSERT1(722, CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number == -1)
		return 0;
	return Buddies(Number).Icon;
}

// 0x4dd640, line 733. NULL for an unknown character.
const char *GetBuddyDescription(int CharacterID)
{
	TASSERT1(733, CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number == -1)
		return nullptr;
	return Buddies(Number).Description;
}

// 0x4dd7c0, line 744
bool GetBuddyNotify(int CharacterID)
{
	TASSERT1(744, CharacterID!=0, CharacterID);
	const int Number = FindBuddy(CharacterID);
	if (Number == -1)
		return false;
	return Buddies(Number).Notify;
}

// 0x4dd940, line 825
bool GetSkillBar(int SkillNumber)
{
	TASSERT1(825, In(SkillNumber,0,SKILLS-1), SkillNumber);
	return SkillBars[SkillNumber];
}

// 0x4ddaa0, line 850
void SetSkillBar(int SkillNumber, bool Value)
{
	TASSERT1(850, In(SkillNumber,0,SKILLS-1), SkillNumber);
	SkillBars[SkillNumber] = Value;
}

// 0x4ddc00. Loads Tibia.cfg. Every key is tested on its own (no else), so
// a key runs one block only because the names differ. Unknown keys raise
// no error but leave their value unread, which then fails as the next key.
// Any error (also a missing file) resets everything to the defaults. In
// both cases the options count as loaded, so SavePreferences writes them.
// ServerAddress, ServerPort and LoginMessages are read and dropped.
void InitPreferences()
{
	SetDefaultPreferences();
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
			char Identifier[MAX_IDENT_LENGTH];
			CopyString(Identifier, Script.getIdentifier(), MAX_IDENT_LENGTH);
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
				CopyString(Text, Script.readString(), HOTKEY_LENGTH);
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
				Script.readSymbol('(');
				ContentWindows(NumberOfContentWindows).ID = Script.readNumber();
				Script.readSymbol(',');
				ContentWindows(NumberOfContentWindows).Size = Script.readNumber();
				Script.readSymbol(')');
				NumberOfContentWindows++;
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
				OpenChannels(NumberOfOpenChannels) = Script.readNumber();
				NumberOfOpenChannels++;
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
				// The description is copied without a length limit.
				Script.readSymbol('(');
				Buddies(NumberOfBuddyEntries).CharacterID = Script.readNumber();
				Script.readSymbol(',');
				Buddies(NumberOfBuddyEntries).Icon = Script.readNumber();
				Script.readSymbol(',');
				const char *Description = Script.readString();
				strcpy(Buddies(NumberOfBuddyEntries).Description, Description);
				Script.readSymbol(',');
				const char *Notify = Script.readIdentifier();
				Buddies(NumberOfBuddyEntries).Notify = strcmp(Notify, "yes") == 0;
				Script.readSymbol(')');
				NumberOfBuddyEntries++;
			}
			if (strcmp(Identifier, "white") == 0) {
				Script.readSymbol('{');
				for (;;) {
					Script.nextToken();
					if (Script.Token == TOKEN::SPECIAL && Script.getSpecial() == '}')
						break;
					if (Script.Token != TOKEN::SPECIAL || Script.getSpecial() != ',')
						AddBuddyIgnore(Script.getString(), true);
				}
			}
			if (strcmp(Identifier, "ignore") == 0) {
				Script.readSymbol('{');
				for (;;) {
					Script.nextToken();
					if (Script.Token == TOKEN::SPECIAL && Script.getSpecial() == '}')
						break;
					if (Script.Token != TOKEN::SPECIAL || Script.getSpecial() != ',')
						IgnoreName(Script.getString(), true);
				}
			}
			if (strcmp(Identifier, "knownhints") == 0) {
				Script.readSymbol('{');
				for (;;) {
					Script.nextToken();
					if (Script.Token == TOKEN::SPECIAL && Script.getSpecial() == '}')
						break;
					if (Script.Token != TOKEN::SPECIAL || Script.getSpecial() != ',') {
						KnownHints(NumberOfKnownHints) = Script.getNumber();
						NumberOfKnownHints++;
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
		SetDefaultPreferences();
	}
	PreferencesLoaded = true;
}

// 0x4de890. Writes Tibia.cfg, only after InitPreferences ran; errors are
// ignored. Odd behaviour kept: one counter serves all loops, and the White
// list does not reset it, so it starts at NumberOfBuddyEntries and writes a
// ',' before its first entry when there were buddies. SkillBars writes ','
// before every skill above 0, also when nothing came before it. Deleted
// buddies (CharacterID 0) are skipped. The fourth FullscreenMode value is
// not saved.
void SavePreferences()
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
			char Help[HOTKEY_LENGTH + 1];
			Script.writeText("Hotkey = (");
			Script.writeNumber(i);
			Script.writeText(",");
			if (HotkeyAutoSend[i]) {
				MsvcSnprintf(Help, HOTKEY_LENGTH + 1, "%s\n", Text);
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
		for (i = 0; i < NumberOfContentWindows; i++) {
			Script.writeText("ContentWindow = (");
			Script.writeNumber(ContentWindows(i).ID);
			Script.writeText(",");
			Script.writeNumber(ContentWindows(i).Size);
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
		for (i = 0; i < NumberOfOpenChannels; i++) {
			Script.writeText("OpenChannel = ");
			Script.writeNumber(OpenChannels(i));
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
		for (i = 0; i < NumberOfBuddyEntries; i++) {
			if (Buddies(i).CharacterID == 0)
				continue;
			Script.writeText("Buddy = (");
			Script.writeNumber(Buddies(i).CharacterID);
			Script.writeText(",");
			Script.writeNumber(Buddies(i).Icon);
			Script.writeText(",");
			Script.writeString(Buddies(i).Description);
			Script.writeText(",");
			Script.writeText(Buddies(i).Notify ? "yes" : "no");
			Script.writeText(")");
			Script.writeLn();
		}
		char *List = GetBuddyIgnoreText();
		Script.writeText("White = {");
		if (List != nullptr) {
			char *Name = List;
			while ((Name = strtok(Name, "\n")) != nullptr) {
				const int Length = (int)strlen(Name);
				if (Length != 0 && IsPermanentBuddyIgnore(Name)) {
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
		List = GetIgnoreListText();
		i = 0;
		Script.writeText("Ignore = {");
		if (List != nullptr) {
			char *Name = List;
			while ((Name = strtok(Name, "\n")) != nullptr) {
				const int Length = (int)strlen(Name);
				if (Length != 0 && IsPermanentIgnore(Name)) {
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
		for (i = 0; i < NumberOfKnownHints; i++) {
			if (i > 0)
				Script.writeText(",");
			Script.writeNumber(KnownHints(i));
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
	} catch (TMyException &) {
	}
}

// 0x4df520. Frees the message of the day and saves Tibia.cfg.
void ExitPreferences()
{
	if (MotD != nullptr)
		free(MotD);
	MotD = nullptr;
	SavePreferences();
}
