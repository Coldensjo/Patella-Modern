// difftest.exe's Preferences and PreferencesFile tests (moved here from
// tools/difftest/preferencestest.cpp, issue #26): Preferences.cpp against the original, as golden-corpus
// tests (golden.h). In the oracle they use the image patches of status_tests.cpp (malloc/free
// redirected), Talk.cpp for the real ignore-list reset, and the image's atol redirected to ours.
//
// The original's state is recorded by value at each check: its options, lists (their length, then
// their entries), texts and the Tibia.cfg it writes (the file's bytes).
//
// Patella-Modern issue #45: the original's InitPreferences and SavePreferences (0x4ddc00, 0x4de890) are
// ReadTibiaCfg and WriteTibiaCfg in Preferences.cpp (the x64 client's import of Tibia.cfg, and the writer these
// tests read its results by); ours of the original's names read and write Patella.ini. The checks are the same:
// the original's ExitPreferences (free the message of the day, save) is ours followed by WriteTibiaCfg.

#include "golden_test.h"

#include "Preferences.h"
#include "Talk.h"

#include <stdarg.h>
#include <string.h>
#include <string>


namespace {

struct TBoolOption {
	uint32 Set, Get;
	void (*OurSet)(bool);
	bool (*OurGet)();
	const char *Name;
};
struct TIntOption {
	uint32 Set, Get;
	void (*OurSet)(int);
	int (*OurGet)();
	const char *Name;
};

const TBoolOption BoolOptions[] = {
	{0x4db2b0, 0x4db2a0, SetTestServerLogin, GetTestServerLogin, "TestServer"},
	{0x4db300, 0x4db3c0, SetWindowsMousepointer, GetWindowsMousepointer, "WindowsMousepointer"},
	{0x4db310, 0x4db3d0, SetLightEffects, GetLightEffects, "LightEffects"},
	{0x4db320, 0x4db3e0, SetTextualEffects, GetTextualEffects, "TextualEffects"},
	{0x4db340, 0x4db400, SetNoStretching, GetNoStretching, "NoStretching"},
	{0x4db360, 0x4db400, SetDefaultNoStretching, GetNoStretching, "NoStretching (default)"},
	{0x4db530, 0x4db540, SetBuddyHideOffline, GetBuddyHideOffline, "BuddyHideOffline"},
	{0x4db570, 0x4db5c0, SetInfoMessages, GetInfoMessages, "InfoMessages"},
	{0x4db580, 0x4db5d0, SetEventMessages, GetEventMessages, "EventMessages"},
	{0x4db590, 0x4db5e0, SetStatusMessages, GetStatusMessages, "StatusMessages"},
	{0x4db5a0, 0x4db5f0, SetTimeStamps, GetTimeStamps, "TimeStamps"},
	{0x4db5b0, 0x4db600, SetPrivateMessages, GetPrivateMessages, "PrivateMessages"},
	{0x4db620, 0x4db640, SetClassicControl, GetClassicControl, "ClassicControl"},
	{0x4db630, 0x4db650, SetAutoChaseOff, GetAutoChaseOff, "AutoChaseOff"},
	{0x4db680, 0x4db6b0, SetSecureMode, GetSecureMode, "SecureMode"},
	{0x4db6c0, 0x4db6d0, SetShowNewHints, GetShowNewHints, "ShowNewHints"},
	{0x4db700, 0x4db6e0, SetLevelBar, GetLevelBar, "LevelBar"},
	{0x4db710, 0x4db6f0, SetMagLevelBar, GetMagLevelBar, "MagLevelBar"},
};

const TIntOption IntOptions[] = {
	{0x4db2c0, 0x4db3a0, SetViewMode, GetViewMode, "ViewMode"},
	{0x4db330, 0x4db3f0, SetCreatureInfo, GetCreatureInfo, "CreatureInfo"},
	{0x4db380, 0x4db410, SetConfineFramerate, GetConfineFramerate, "ConfineFramerate"},
	{0x4db390, 0x4db420, SetEngine, GetEngine, "Engine"},
	{0x4db4b0, 0x4db4c0, SetConsoleHeight, GetConsoleHeight, "ConsoleHeight"},
	{0x4db4d0, 0x4db4e0, SetAutomapZoom, GetAutomapZoom, "AutomapZoom"},
	{0x4db660, 0x4db690, SetAttackMode, GetAttackMode, "AttackMode"},
	{0x4db670, 0x4db6a0, SetChaseMode, GetChaseMode, "ChaseMode"},
};

void CheckAllOptions(const char *When)
{
	for (auto &O : BoolOptions) {
		bool Value = ORIGINAL_VALUE("bool option", bool, At<GetBoolFn>(O.Get)());
		CHECK(O.OurGet() == Value, "%s after %s", O.Name, When);
	}
	for (auto &O : IntOptions) {
		int Value = ORIGINAL_VALUE("int option", int, At<GetIntFn>(O.Get)());
		CHECK(O.OurGet() == Value, "%s after %s", O.Name, When);
	}
	static_assert(sizeof(TFullscreenMode) == 16, "TFullscreenMode: four ints on both sides");
	TFullscreenMode Fullscreen = {};
	ORIGINAL(Fullscreen = *At<TFullscreenMode *(__cdecl *)()>(0x4db3b0)());
	golden::OutputBytes("fullscreen mode", &Fullscreen, sizeof(Fullscreen));
	CHECK(memcmp(GetFullscreenMode(), &Fullscreen, sizeof(TFullscreenMode)) == 0, "FullscreenMode after %s", When);
	int A[5], B[5] = {};
	bool MA, MB = false;
	GetWindowedMode(&A[0], &A[1], &A[2], &A[3], &MA);
	ORIGINAL(At<void(__cdecl *)(int *, int *, int *, int *, bool *)>(0x4db470)(&B[0], &B[1], &B[2], &B[3], &MB));
	golden::OutputArray("windowed mode", B, 4);
	golden::Output("windowed maximized", MB);
	CHECK(memcmp(A, B, 4 * sizeof(int)) == 0 && MA == MB, "WindowedMode after %s", When);
	GetBuddySortmethods(A);
	ORIGINAL(At<void(__cdecl *)(int *)>(0x4db510)(B));
	golden::OutputArray("buddy sort methods", B, 3);
	CHECK(memcmp(A, B, 3 * sizeof(int)) == 0, "BuddySortmethods after %s", When);
	golden::TText MotD = ORIGINAL_TEXT("MotD", At<const char *(__cdecl *)()>(0x4db560)());
	CHECK(strcmp(GetMotD(), MotD.c_str()) == 0, "MotD after %s", When);
	for (int i = 0; i < SKILLS; i++) {
		bool Bar = ORIGINAL_VALUE("skill bar", bool, At<bool(__cdecl *)(int)>(0x4dd940)(i));
		CHECK(GetSkillBar(i) == Bar, "SkillBar %d after %s", i, When);
	}
	for (int i = 0; i < HOTKEYS; i++) {
		bool AutoA = false, AutoB = false;
		const char *TextA = GetHotkey(i, &AutoA);
		golden::TText TextB = ORIGINAL_TEXT("hotkey", At<const char *(__cdecl *)(int, bool *)>(0x4dc7e0)(i, &AutoB));
		golden::Output("hotkey auto", AutoB);
		CHECK(strcmp(TextA, TextB.c_str()) == 0 && AutoA == AutoB, "hotkey %d after %s", i, When);
	}
}

void CheckLists(const char *When)
{
	struct {
		uint32 Original;
		int (*Ours)(int **);
		const char *Name;
	} Lists[] = {
		{0x4dbd90, GetOpenDialogs, "OpenDialogs"},
		{0x4dbde0, GetOpenChannels, "OpenChannels"},
	};
	for (auto &L : Lists) {
		int *A = nullptr;
		int CountA = L.Ours(&A);
		// The original's list: its length, whether it is NULL, then the entries.
		[[maybe_unused]] int *Got = nullptr;
		int CountB = ORIGINAL_VALUE("list length", int, At<ListFn>(L.Original)(&Got));
		uint8 Null = ORIGINAL_VALUE("list null", uint8, Got == nullptr);
		std::string Entries(CountB > 0 ? (size_t)CountB * sizeof(int) : 0, '\0');
		ORIGINAL(if (Got != nullptr && CountB > 0) memcpy(&Entries[0], Got, Entries.size()));
		if (CountB > 0)
			golden::OutputArray("list entries", (int *)&Entries[0], (size_t)CountB);
		const int *B = Null != 0 ? nullptr : CountB > 0 ? (const int *)Entries.data()
														: (const int *)&CountB;
		CHECK(CountA == CountB && (CountA == 0 ? A == nullptr && B == nullptr : memcmp(A, B, CountA * sizeof(int)) == 0),
			"%s after %s", L.Name, When);
		delete[] A;
		ORIGINAL(free(Got));
	}
	for (int Hint = 0; Hint < 40; Hint++) {
		bool Known = ORIGINAL_VALUE("IsKnownHint", bool, At<bool(__cdecl *)(int)>(0x4dbe50)(Hint));
		CHECK(IsKnownHint(Hint) == Known, "IsKnownHint(%d) after %s", Hint, When);
	}
	for (int ID = 0; ID < 20; ID++) {
		int Size = ORIGINAL_VALUE("GetContentWindowSize", int, At<int(__cdecl *)(int)>(0x4dbd50)(ID));
		CHECK(GetContentWindowSize(ID) == Size, "GetContentWindowSize(%d)", ID);
	}
	for (int ID = 1; ID < 30; ID++) {
		int Buddy = ORIGINAL_VALUE("FindBuddy", int, At<int(__cdecl *)(int)>(0x4dce20)(ID));
		CHECK(FindBuddy(ID) == Buddy, "FindBuddy(%d) after %s", ID, When);
		int Icon = ORIGINAL_VALUE("GetBuddyIcon", int, At<int(__cdecl *)(int)>(0x4dd4c0)(ID));
		CHECK(GetBuddyIcon(ID) == Icon, "GetBuddyIcon(%d)", ID);
		bool Notify = ORIGINAL_VALUE("GetBuddyNotify", bool, At<bool(__cdecl *)(int)>(0x4dd7c0)(ID));
		CHECK(GetBuddyNotify(ID) == Notify, "GetBuddyNotify(%d)", ID);
		const char *A = GetBuddyDescription(ID);
		golden::TText Description = ORIGINAL_TEXT("GetBuddyDescription", At<const char *(__cdecl *)(int)>(0x4dd640)(ID));
		const char *B = Description.c_str();
		CHECK((A == nullptr) == (B == nullptr) && (A == nullptr || strcmp(A, B) == 0), "GetBuddyDescription(%d)", ID);
	}
}

} // namespace

void TestPreferences()
{
	ORIGINAL(PatchImage());
	ORIGINAL(At<VectorInitCtorFn>(0x4fd6e0)((void *)(uintptr_t)0x71efb0, 0, 0, 9, 10, 0));
	ORIGINAL(PatchJump(0x51262a, (void *)ImageAtol));
	// The original's static initialisers (0x55a050-0x55a0d0) did not run.
	ORIGINAL(At<VectorCtorFn>(0x4dbe90)((void *)(uintptr_t)0x71c324, 0, 0, 9, 10));
	ORIGINAL(At<VectorInitCtorFn>(0x453300)((void *)(uintptr_t)0x71c348, 0, 0, 9, 10, -1));
	ORIGINAL(At<VectorCtorFn>(0x4a2b90)((void *)(uintptr_t)0x71c368, 0, 0, 9, 10));
	ORIGINAL(At<VectorCtorFn>(0x4dc170)((void *)(uintptr_t)0x71c388, 0, 0, 9, 10));
	ORIGINAL(At<VectorCtorFn>(0x4a2b90)((void *)(uintptr_t)0x71c430, 0, 0, 9, 10));

	CheckAllOptions("start");
	IgnoreName("reset probe", true);
	ORIGINAL(At<void(__cdecl *)(const char *, bool)>(0x501940)("reset probe", true));
	SetDefaultPreferences();
	ORIGINAL(At<void(__cdecl *)()>(0x4db720)());
	bool Reset = ORIGINAL_VALUE("Talk list reset", bool, At<char *(__cdecl *)()>(0x501010)() == nullptr);
	CHECK(GetIgnoreListText() == nullptr && Reset, "Talk list reset");
	CheckAllOptions("SetDefaultPreferences");

	for (int t = 0; t < 20000; t++) {
		char Text[300];
		switch (Random() % 16) {
		case 0:
		case 1: {
			const TBoolOption &O = BoolOptions[Random() % (sizeof(BoolOptions) / sizeof(BoolOptions[0]))];
			bool Value = (Random() & 1) != 0;
			O.OurSet(Value);
			ORIGINAL(At<SetBoolFn>(O.Set)(Value));
			break;
		}
		case 2: {
			const TIntOption &O = IntOptions[Random() % (sizeof(IntOptions) / sizeof(IntOptions[0]))];
			int Value = (int)Random32();
			O.OurSet(Value);
			ORIGINAL(At<SetIntFn>(O.Set)(Value));
			break;
		}
		case 3: {
			TFullscreenMode Mode = {(int)Random(), (int)Random(), (int)Random(), (int)Random()};
			SetFullscreenMode(&Mode);
			ORIGINAL(At<void(__cdecl *)(const TFullscreenMode *)>(0x4db2d0)(&Mode));
			int x = (int)Random(), y = (int)Random(), w = (int)Random(), h = (int)Random();
			bool Max = (Random() & 1) != 0;
			SetWindowedMode(x, y, w, h, Max);
			ORIGINAL(At<void(__cdecl *)(int, int, int, int, bool)>(0x4db430)(x, y, w, h, Max));
			int Methods[3] = {(int)Random() % 3, (int)Random() % 3, (int)Random() % 3};
			SetBuddySortmethods(Methods);
			ORIGINAL(At<void(__cdecl *)(const int *)>(0x4db4f0)(Methods));
			break;
		}
		case 4: {
			int Number = (int)(Random() % HOTKEYS);
			RandomText(Text, (int)(Random() % HOTKEY_LENGTH));
			bool Auto = (Random() & 1) != 0;
			SetHotkey(Number, Text, Auto);
			ORIGINAL(At<void(__cdecl *)(int, const char *, bool)>(0x4dc450)(Number, Text, Auto));
			break;
		}
		case 5: {
			int Identifier = (int)(Random() % 50);
			if (Random() & 1) {
				AddOpenDialog(Identifier);
				ORIGINAL(At<SetIntFn>(0x4dc950)(Identifier));
			} else {
				AddOpenChannel(Identifier);
				ORIGINAL(At<SetIntFn>(0x4dccb0)(Identifier));
			}
			break;
		}
		case 6:
			if (Random() % 8 == 0) {
				ClearOpenDialogs();
				ORIGINAL(At<void(__cdecl *)()>(0x4db550)());
			}
			if (Random() % 8 == 0) {
				ClearOpenChannels();
				ORIGINAL(At<void(__cdecl *)()>(0x4db610)());
			}
			break;
		case 7: {
			int Hint = (int)(Random() % 40);
			AddKnownHint(Hint);
			ORIGINAL(At<SetIntFn>(0x4dbe30)(Hint));
			break;
		}
		case 8: {
			int ID = (int)(Random() % 20), Size = (int)Random();
			SetContentWindowSize(ID, Size);
			ORIGINAL(At<void(__cdecl *)(int, int)>(0x4dbcd0)(ID, Size));
			break;
		}
		case 9:
		case 10: {
			int ID = 1 + (int)(Random() % 29), Icon = (int)(Random() % 10);
			RandomText(Text, (int)(Random() % BUDDYDESCRIPTION_LENGTH));
			bool Notify = (Random() & 1) != 0;
			SetBuddy(ID, Icon, Text, Notify);
			ORIGINAL(At<void(__cdecl *)(int, int, const char *, bool)>(0x4dcfb0)(ID, Icon, Text, Notify));
			break;
		}
		case 11: {
			int ID = 1 + (int)(Random() % 29);
			DeleteBuddy(ID);
			ORIGINAL(At<SetIntFn>(0x4dd360)(ID));
			break;
		}
		case 12: {
			char Body[100];
			RandomText(Body, (int)(Random() % 60));
			switch (Random() % 4) {
			case 0: snprintf(Text, sizeof(Text), "%d\n%s", (int)(Random() % 5), Body); break;
			case 1: snprintf(Text, sizeof(Text), "%s", Body); break;
			case 2: snprintf(Text, sizeof(Text), "  -%u\n%s", Random(), Body); break;
			default: snprintf(Text, sizeof(Text), "x\n%s", Body); break;
			}
			bool A = SetMotD(Text);
			bool B = ORIGINAL_VALUE("SetMotD", bool, At<bool(__cdecl *)(const char *)>(0x4dcac0)(Text));
			CHECK(A == B, "SetMotD(\"%s\")", Text);
			break;
		}
		case 13: {
			int Skill = (int)(Random() % SKILLS);
			bool Value = (Random() & 1) != 0;
			SetSkillBar(Skill, Value);
			ORIGINAL(At<void(__cdecl *)(int, bool)>(0x4ddaa0)(Skill, Value));
			break;
		}
		default:
			break;
		}
		if (t % 50 == 0) {
			CheckAllOptions("random changes");
			CheckLists("random changes");
		}
	}
	CheckAllOptions("the end");
	CheckLists("the end");
}

// --- Tibia.cfg loader and saver ------------------------------------------------
//
// Runs after TestScript: uses its stdio redirection (PatchScript). Only valid
// files are generated; a parse error would throw inside the original.


namespace {

std::string ReadWholeFile(const char *Name, bool *Exists)
{
	std::string Text;
	FILE *File = fopen(Name, "rb");
	*Exists = File != nullptr;
	if (File == nullptr)
		return Text;
	int c;
	while ((c = getc(File)) != EOF)
		Text += (char)c;
	fclose(File);
	return Text;
}

// The Tibia.cfg the original wrote (the oracle: Save, then read and removed), by value.
std::string OriginalFile(const char *Name, bool *Exists, [[maybe_unused]] uint32 Save)
{
	std::string Text;
	uint8 Have = 0;
	golden::Output(Name, Have);
	*Exists = Have != 0;
	return golden::OutputText(Name, Text.data(), Text.size());
}

std::string CfgNumber(bool Negative = false)
{
	char Help[16];
	int Value = (int)(Random() % 4 == 0 ? Random() % 100000 : Random() % 50);
	if (Negative && Random() % 4 == 0)
		Value = -Value;
	sprintf(Help, "%d", Value);
	return Help;
}

std::string CfgYes()
{
	switch (Random() % 4) {
	case 0: return "no";
	case 1: return "YES"; // identifiers are lower-cased
	case 2: return "maybe";
	default: return "yes";
	}
}

std::string CfgString(int MaxLength)
{
	static const char Chars[] = "abc XYZ 019 .,:!?()=-#'";
	std::string s = "\"";
	int n = (int)(Random() % (MaxLength + 1));
	for (int i = 0; i < n; i++) {
		switch (Random() % 14) {
		case 0: s += "\\\""; break;
		case 1: s += "\\\\"; break;
		case 2: s += "\\n"; break;
		default: s += Chars[Random() % (sizeof(Chars) - 1)]; break;
		}
	}
	return s + "\"";
}

std::string CfgName()
{
	std::string s = "\"";
	int n = 1 + (int)(Random() % 12);
	for (int i = 0; i < n; i++)
		s += (char)((i == 0 ? 'A' : 'a') + Random() % 26);
	return s + "\"";
}

std::string CfgList(std::string (*Item)())
{
	std::string s = "{";
	int n = (int)(Random() % 6);
	for (int i = 0; i < n; i++) {
		if (i > 0)
			s += Random() % 8 ? "," : " ";
		s += Item();
	}
	return s + "}";
}

std::string CfgSkill()
{
	char Help[4];
	sprintf(Help, "%d", (int)(Random() % SKILLS));
	return Help;
}
std::string CfgHint() { return CfgNumber(); }

std::string CfgLine()
{
	switch (Random() % 41) {
	case 0: return "TestServer = " + CfgYes();
	case 1: return "ServerAddress = " + CfgString(20);
	case 2: return "ServerPort = " + CfgNumber();
	case 3: {
		const std::string C = CfgNumber(), B = CfgNumber(), A = CfgNumber();
		return "FullscreenMode = (" + A + "," + B + "," + C + ")";
	}
	case 4: return "WindowsMousepointer = " + CfgYes();
	case 5: return "LightEffects = " + CfgYes();
	case 6: return "TextualEffects = " + CfgYes();
	case 7: return "CreatureInfo = " + CfgNumber(true);
	case 8: return "NoStretching = " + CfgYes();
	case 9: return "ConfineFramerate = " + CfgNumber();
	case 10: return "Engine = " + CfgNumber(true);
	case 11:
	case 12: {
		char Help[8];
		sprintf(Help, "%d", (int)(Random() % HOTKEYS));
		std::string Text = CfgString(Random() % 4 ? 40 : 300);
		if (Random() % 3 == 0)
			Text.insert(Text.size() - 1, "\\n");
		return std::string("Hotkey = (") + Help + "," + Text + ")";
	}
	case 13: {
		const std::string E = CfgYes(), D = CfgNumber(), C = CfgNumber(), B = CfgNumber(true), A = CfgNumber(true);
		return "WindowedMode = (" + A + "," + B + "," + C + "," + D + "," + E + ")";
	}
	case 14: return "ConsoleHeight = " + CfgNumber();
	case 15: return "AutomapZoom = " + CfgNumber(true);
	case 16: {
		const std::string B = CfgNumber(), A = CfgNumber();
		return "ContentWindow = (" + A + "," + B + ")";
	}
	case 17: {
		const std::string C = CfgNumber(), B = CfgNumber(), A = CfgNumber();
		return "BuddySortmethods = (" + A + "," + B + "," + C + ")";
	}
	case 18: return "BuddyHideOffline = " + CfgYes();
	case 19:
	case 20: {
		char Help[8];
		sprintf(Help, "%d", (int)(Random() % 12) - 1);
		return std::string("OpenDialogs = ") + Help;
	}
	case 21: return "LastMotD = " + CfgNumber();
	case 22: return "InfoMessages = " + CfgYes();
	case 23: return "EventMessages = " + CfgYes();
	case 24: return "LoginMessages = " + CfgYes();
	case 25: return "StatusMessages = " + CfgYes();
	case 26: return "TimeStamps = " + CfgYes();
	case 27: return "PrivateMessages = " + CfgYes();
	case 28: return "OpenChannel = " + CfgNumber();
	case 29: return "ClassicControl = " + CfgYes();
	case 30: return "AutoChaseOff = " + CfgYes();
	case 31: return "AttackMode = " + CfgNumber();
	case 32: {
		char Help[4];
		sprintf(Help, "%d", (int)(Random() % 4));
		return std::string("ChaseMode = ") + Help;
	}
	case 33: return "SecureMode = " + CfgNumber();
	case 34:
	case 35: {
		char Help[8];
		sprintf(Help, "%d", Random() % 5 == 0 ? 0 : (int)(1 + Random() % 30));
		const std::string Notify = CfgYes(), Name = CfgString(60), Id = CfgNumber();
		return std::string("Buddy = (") + Help + "," + Id + "," + Name + "," + Notify + ")";
	}
	case 36: return "White = " + CfgList(CfgName);
	case 37: return "Ignore = " + CfgList(CfgName);
	case 38: return "KnownHints = " + CfgList(CfgHint);
	case 39: return "SkillBars = " + CfgList(CfgSkill);
	default: {
		if (Random() % 2)
			return "ShowNewHints = " + CfgYes();
		const std::string Value = CfgYes(); // operator+'s arguments right to left
		return (Random() % 2 ? "LevelBar = " : "MagLevelBar = ") + Value;
	}
	}
}

void CheckTalkLists(const char *When)
{
	char *A = GetIgnoreListText();
	golden::TText Ignore;
	ORIGINAL(char *Got = At<char *(__cdecl *)()>(0x501010)());
	Ignore = ORIGINAL_TEXT("ignore list", Got);
	ORIGINAL(free(Got));
	const char *B = Ignore.c_str();
	CHECK((A == nullptr) == (B == nullptr) && (A == nullptr || strcmp(A, B) == 0), "ignore list after %s", When);
	free(A);
	A = GetBuddyIgnoreText();
	golden::TText White;
	ORIGINAL(Got = At<char *(__cdecl *)()>(0x5002d0)());
	White = ORIGINAL_TEXT("white list", Got);
	ORIGINAL(free(Got));
	B = White.c_str();
	CHECK((A == nullptr) == (B == nullptr) && (A == nullptr || strcmp(A, B) == 0), "white list after %s", When);
	free(A);
}

void CompareSaved(const char *When)
{
	WriteTibiaCfg();
	bool HaveA, HaveB;
	std::string A = ReadWholeFile("Tibia.cfg", &HaveA);
	remove("Tibia.cfg");
	std::string B = OriginalFile("Tibia.cfg written", &HaveB, 0x4de890);
	CHECK(HaveA == HaveB && A == B, "Tibia.cfg written after %s differs (%d and %d bytes)", When, (int)A.size(),
		(int)B.size());
}

} // namespace

void TestPreferencesFile()
{
	ORIGINAL(PatchScript());
	ORIGINAL(PatchJump(0x523812, (void *)ImageFree2));
	ORIGINAL(PatchJump(0x513544, (void *)ImageStrtok));
	ORIGINAL(PatchJump(0x51233d, (void *)ImageSnprintf));
	// The original's Talk filter lists (static initialisers, see talk_tests.cpp).
	char OldDirectory[MAX_PATH], Help[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, OldDirectory);
	GetTempPathA(MAX_PATH, Help);
	std::string Directory = std::string(Help) + "tibia-difftest-cfg";
	CreateDirectoryA(Directory.c_str(), nullptr);
	SetCurrentDirectoryA(Directory.c_str());
	remove("Tibia.cfg");

	// Before ReadTibiaCfg (the original's InitPreferences) nothing is written.
	CompareSaved("no InitPreferences");

	for (int Round = 0; Round < 400; Round++) {
		std::string Text;
		int n = (int)(Random() % 60);
		for (int i = 0; i < n; i++) {
			if (Random() % 10 == 0)
				Text += "# comment\n";
			Text += CfgLine();
			Text += Random() % 6 ? "\r\n" : " ";
		}
		golden::InputBytes("Tibia.cfg", Text.data(), Text.size());
		FILE *File = fopen("Tibia.cfg", "wb");
		fwrite(Text.data(), 1, Text.size(), File);
		fclose(File);
		if (getenv("DIFFTEST_TRACE") != nullptr) {
			fprintf(stderr, "cfg round %d\n", Round);
			FILE *Copy = fopen("last.cfg", "wb");
			fwrite(Text.data(), 1, Text.size(), Copy);
			fclose(Copy);
		}
		ReadTibiaCfg();
		ORIGINAL(At<void(__cdecl *)()>(0x4ddc00)());
		bool Loaded = ORIGINAL_VALUE("PreferencesLoaded", bool, Global<bool>(0x71c319));
		CHECK(Loaded, "PreferencesLoaded not set");
		CheckAllOptions("InitPreferences");
		CheckLists("InitPreferences");
		CheckTalkLists("InitPreferences");
		CompareSaved("InitPreferences");
		// Saving and loading again must give the same file.
		if (Round % 10 == 0) {
			bool Trace = getenv("DIFFTEST_TRACE") != nullptr;
			WriteTibiaCfg();
			if (Trace)
				fprintf(stderr, "ours init\n");
			ReadTibiaCfg();
			remove("Tibia.cfg");
			if (Trace)
				fprintf(stderr, "original save\n");
			ORIGINAL(At<void(__cdecl *)()>(0x4de890)());
			if (Trace)
				fprintf(stderr, "original init\n");
			ORIGINAL(At<void(__cdecl *)()>(0x4ddc00)());
			if (Trace)
				fprintf(stderr, "done\n");
			CheckAllOptions("a second InitPreferences");
			CheckLists("a second InitPreferences");
			CompareSaved("a second InitPreferences");
		}
	}

	// Long buddy descriptions (issue #85), whatever the random rounds drew (their CfgString(60) never reaches
	// them): the reader copied the description without a limit, so one of 128 characters kept all of them with
	// its NUL in Notify, and up to 131 the rest of the entry held the characters after it. With Notify "no"
	// (yes would read on past Notify into the padding, whose bytes are each side's own). No random numbers.
	{
		std::string Text;
		const int Lengths[] = {127, 128, 129, 131};
		for (int i = 0; i < 4; i++) {
			std::string Description(Lengths[i], (char)('a' + i));
			for (int j = 0; j < Lengths[i]; j += 10)
				Description[j] = (char)('0' + j / 10 % 10);
			Text += "Buddy = (" + std::to_string(i + 1) + "," + std::to_string(i) + ",\"" + Description + "\","
				+ (i == 0 ? "yes" : "no") + ")\r\n";
		}
		golden::InputBytes("Tibia.cfg", Text.data(), Text.size());
		FILE *File = fopen("Tibia.cfg", "wb");
		fwrite(Text.data(), 1, Text.size(), File);
		fclose(File);
		ReadTibiaCfg();
		ORIGINAL(At<void(__cdecl *)()>(0x4ddc00)());
		CheckLists("long buddy descriptions");
		CompareSaved("long buddy descriptions");
	}

	// ExitPreferences frees the MotD and saves.
	SetMotD("7\nToday's news");
	ORIGINAL(At<bool(__cdecl *)(const char *)>(0x4dcac0)("7\nToday's news"));
	ExitPreferences();
	// Ours saves into Patella.ini since issue #45; WriteTibiaCfg writes what the original's ExitPreferences wrote.
	WriteTibiaCfg();
	remove("Patella.ini");
	bool HaveA, HaveB;
	std::string A = ReadWholeFile("Tibia.cfg", &HaveA);
	remove("Tibia.cfg");
	std::string B = OriginalFile("ExitPreferences output", &HaveB, 0x4df520);
	CHECK(HaveA && HaveB && A == B, "ExitPreferences output differs");
	CheckAllOptions("ExitPreferences");

	SetCurrentDirectoryA(OldDirectory);
	RemoveDirectoryA(Directory.c_str());
}
