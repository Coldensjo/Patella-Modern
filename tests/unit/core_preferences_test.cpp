// PreferenceKeys (src/settings/PreferenceKeys.cpp, issue #45): the options of Tibia.cfg as keys of Patella.ini, the
// fields of a value and the escaping of texts. The import of a Tibia.cfg itself (Preferences.cpp InitPreferences, the
// original's reader) is tests/golden/preferences_file_test.cpp's.

#include <doctest/doctest.h>

#include "settings/PreferenceKeys.h"
#include "settings/Settings.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

using patella::settings::ParseFields;
using patella::settings::QuoteText;
using patella::settings::ReadPreferences;
using patella::settings::TField;
using patella::settings::TPreferenceValues;
using patella::settings::TSettingsFile;
using patella::settings::WritePreferences;

namespace {

// Every option away from its default, the texts with what needs escaping.
TPreferenceValues Changed()
{
	TPreferenceValues Values;
	Values.TestServer = true;
	Values.FullscreenMode = std::array<int, 3>{1024, 768, 32};
	Values.WindowsMousepointer = false;
	Values.LightEffects = false;
	Values.TextualEffects = false;
	Values.CreatureInfo = 0;
	Values.NoStretching = true;
	Values.ConfineFramerate = 198;
	Values.Engine = 2;
	Values.Hotkeys[0] = "hello patella";
	Values.Hotkeys[1] = "exura";
	Values.HotkeyAutoSend[1] = true;
	Values.Hotkeys[35] = "a=b \"quoted\" back\\slash, comma \xe4\xf6\xfc\xdf \x7f\x01 \ttab ";
	Values.WindowedMode = {-4, 6, 800, 600};
	Values.WindowedMaximized = true;
	Values.ConsoleHeight = 120;
	Values.AutomapZoom = 4;
	Values.ContentWindows = {{1, 200}, {3, -1}, {7, 150}};
	Values.BuddySortmethods = {0, 1, 2};
	Values.BuddyHideOffline = true;
	Values.OpenDialogs = {4, 1};
	Values.LastMotD = 12345;
	Values.InfoMessages = true;
	Values.EventMessages = false;
	Values.StatusMessages = true;
	Values.TimeStamps = true;
	Values.PrivateMessages = false;
	Values.OpenChannels = {5, 0, 7};
	Values.ClassicControl = true;
	Values.AutoChaseOff = false;
	Values.AttackMode = 1;
	Values.ChaseMode = 1;
	Values.SecureMode = false;
	Values.Buddies = {{123456, 3, "line one\nline two\r\n= \"x\"", true}, {7, 0, "", false}, {99, 1, "\xc4rger", false}};
	Values.White = {"Some Friend", "Ein G\xe4ste, Gruss"};
	Values.Ignore = {"Spammer", "\"Quoted\""};
	Values.KnownHints = {1, 2, 30};
	Values.ShowNewHints = false;
	Values.LevelBar = false;
	Values.MagLevelBar = false;
	Values.SkillBars = {true, false, true, false, false, false, true};
	return Values;
}

TPreferenceValues RoundTrip(const TPreferenceValues &Values, std::vector<std::string> *Problems = nullptr)
{
	TSettingsFile File;
	WritePreferences(File, Values);
	const TSettingsFile Again = TSettingsFile::Parse(File.Text());
	TPreferenceValues Read;
	const std::vector<std::string> Found = ReadPreferences(Again, Read);
	if (Problems != nullptr)
		*Problems = Found;
	else
		CHECK(Found.empty());
	return Read;
}

} // namespace

TEST_CASE("Preferences: a text in quotes, every byte back as it was")
{
	CHECK(QuoteText("plain") == "\"plain\"");
	CHECK(QuoteText("a\"b\\c") == "\"a\\\"b\\\\c\"");
	CHECK(QuoteText("1\n2\r3\t4") == "\"1\\n2\\r3\\t4\"");
	CHECK(QuoteText(std::string("\x01\x1f\x7f", 3)) == "\"\\x01\\x1f\\x7f\"");
	// Latin-1 bytes as they are (plan D4 undecided): not UTF-8, not escaped.
	CHECK(QuoteText("\xe4\xff") == "\"\xe4\xff\"");
	std::string All;
	for (int c = 1; c < 256; c++)
		All += static_cast<char>(c);
	const std::optional<std::vector<TField>> Fields = ParseFields(QuoteText(All));
	REQUIRE(Fields.has_value());
	REQUIRE(Fields->size() == 1);
	CHECK((*Fields)[0].Kind == TField::TKind::Text);
	CHECK((*Fields)[0].Text == All);
	// The quoted text is a value Patella.ini keeps: no line end, no space at its ends.
	TSettingsFile File;
	CHECK(File.Set("Text", QuoteText(" " + All + " ")));
	CHECK(ParseFields(*TSettingsFile::Parse(File.Text()).Get("Text"))->at(0).Text == " " + All + " ");
}

TEST_CASE("Preferences: the fields of a value")
{
	const auto Fields = ParseFields(" 12 , -3,yes,NO , \"a,b\" ,\"\"");
	REQUIRE(Fields.has_value());
	REQUIRE(Fields->size() == 6);
	CHECK((*Fields)[0].Number == 12);
	CHECK((*Fields)[1].Number == -3);
	CHECK(((*Fields)[2].Kind == TField::TKind::Flag && (*Fields)[2].Flag));
	CHECK(((*Fields)[3].Kind == TField::TKind::Flag && !(*Fields)[3].Flag));
	CHECK((*Fields)[4].Text == "a,b");
	CHECK(((*Fields)[5].Kind == TField::TKind::Text && (*Fields)[5].Text.empty()));
	CHECK(ParseFields("")->empty());
	CHECK(ParseFields("  ")->empty());
	for (const char *Bad : {"1,", ",1", "1 2", "maybe", "\"open", "\"a\"b", "\"\\q\"", "\"\\x4\"", "\"\\x00\"", "99999999999",
			 "1,,2", "0x10"})
		CHECK_MESSAGE(!ParseFields(Bad).has_value(), Bad);
}

TEST_CASE("Preferences: every option written and read back")
{
	const TPreferenceValues Values = Changed();
	CHECK(RoundTrip(Values) == Values);
	CHECK(RoundTrip(TPreferenceValues()) == TPreferenceValues());
	TSettingsFile File;
	CHECK_FALSE(patella::settings::HasPreferences(File));
	WritePreferences(File, Values);
	CHECK(patella::settings::HasPreferences(File));
	CHECK(File.Get("PreferencesVersion") == "1");
	CHECK(File.Get("TestServer") == "yes");
	CHECK(File.Get("FullscreenMode") == "1024,768,32");
	CHECK(File.Get("Hotkey0") == "\"hello patella\",no");
	CHECK(File.Get("Hotkey1") == "\"exura\",yes");
	CHECK_FALSE(File.Get("Hotkey2").has_value());
	CHECK(File.Get("WindowedMode") == "-4,6,800,600,yes");
	CHECK(File.Get("ContentWindow2") == "3,-1");
	CHECK(File.Get("OpenDialogs") == "4,1");
	CHECK(File.Get("OpenChannel") == "5,0,7");
	CHECK(File.Get("Buddy3") == "99,1,\"\xc4rger\",no");
	CHECK(File.Get("White") == "\"Some Friend\",\"Ein G\xe4ste, Gruss\"");
	CHECK(File.Get("SkillBars") == "0,2,6");
	CHECK(File.Get("SecureMode") == "no");
	CHECK(File.Get("Engine") == "2");
	CHECK(File.BadLines().empty());
}

TEST_CASE("Preferences: the other keys and lines stay, the entries that are gone go")
{
	TSettingsFile File = TSettingsFile::Parse("# Patella-Modern's settings\r\nDevice=cpu\r\nLogLevel=debug\r\n");
	TPreferenceValues Values = Changed();
	WritePreferences(File, Values);
	const std::string First = File.Text();
	CHECK(First.starts_with("# Patella-Modern's settings\r\nDevice=cpu\r\nLogLevel=debug\r\nPreferencesVersion=1\r\n"));
	Values.FullscreenMode.reset();
	Values.Hotkeys[35].clear();
	Values.ContentWindows.resize(1);
	Values.Buddies.clear();
	WritePreferences(File, Values);
	CHECK(File.Get("Device") == "cpu");
	CHECK(File.Get("LogLevel") == "debug");
	CHECK_FALSE(File.Get("FullscreenMode").has_value());
	CHECK_FALSE(File.Get("Hotkey35").has_value());
	CHECK(File.Get("ContentWindow1") == "1,200");
	CHECK_FALSE(File.Get("ContentWindow2").has_value());
	CHECK_FALSE(File.Get("Buddy1").has_value());
	CHECK(File.Get("BuddySortmethods") == "0,1,2");
	CHECK(File.Get("BuddyHideOffline") == "yes");
	TPreferenceValues Read;
	CHECK(ReadPreferences(TSettingsFile::Parse(File.Text()), Read).empty());
	CHECK(Read == Values);
	// Written again unchanged: the same text.
	const std::string Second = File.Text();
	WritePreferences(File, Values);
	CHECK(File.Text() == Second);
}

TEST_CASE("Preferences: read as the original's reader reads its own Tibia.cfg")
{
	const TSettingsFile File = TSettingsFile::Parse("PreferencesVersion=1\n"
													"chasemode=2\n"
													"OpenDialogs=3,5,3,5,1\n"
													"Hotkey3=\"one\\ntwo\\n\",no\n"
													"Hotkey4=\"x\",yes\n"
													"HOTKEY5=\"y\\n\",yes\n"
													"Buddy10=2,0,\"b\",no\n"
													"Buddy2=1,0,\"a\",yes\n"
													"ContentWindow1=4,10\n");
	TPreferenceValues Values;
	CHECK(ReadPreferences(File, Values).empty());
	CHECK(Values.ChaseMode == 0);
	CHECK(Values.OpenDialogs == std::vector<int>{3, 5, 1});
	// A hotkey's final line break: sent at once; the others: spaces.
	CHECK(Values.Hotkeys[3] == "one two");
	CHECK(Values.HotkeyAutoSend[3]);
	CHECK(Values.Hotkeys[4] == "x");
	CHECK(Values.HotkeyAutoSend[4]);
	CHECK(Values.Hotkeys[5] == "y ");
	CHECK(Values.HotkeyAutoSend[5]);
	// The numbered entries in their numbers' order.
	REQUIRE(Values.Buddies.size() == 2);
	CHECK(Values.Buddies[0].CharacterID == 1);
	CHECK(Values.Buddies[1].CharacterID == 2);
	CHECK(Values.ContentWindows == std::vector<patella::settings::TContentWindowValue>{{4, 10}});
	// A hotkey text is cut to 255 bytes, as the client's buffer.
	TSettingsFile Long;
	Long.Set("Hotkey0", QuoteText(std::string(300, 'x')) + ",yes");
	TPreferenceValues LongValues;
	CHECK(ReadPreferences(Long, LongValues).empty());
	CHECK(LongValues.Hotkeys[0] == std::string(255, 'x'));
	CHECK_FALSE(LongValues.HotkeyAutoSend[0]);
}

TEST_CASE("Preferences: a value that cannot be read keeps the option and is reported")
{
	const TSettingsFile File = TSettingsFile::Parse("PreferencesVersion=1\n"
													"TestServer=maybe\n"
													"CreatureInfo=\"two\"\n"
													"WindowedMode=1,2,3\n"
													"Hotkey36=\"x\",no\n"
													"Hotkey1=\"x\"\n"
													"SkillBars=0,7\n"
													"White=\"a\",1\n"
													"LightEffects=no\n");
	TPreferenceValues Values;
	const std::vector<std::string> Problems = ReadPreferences(File, Values);
	CHECK(Problems.size() == 7);
	const TPreferenceValues Defaults;
	CHECK(Values.TestServer == Defaults.TestServer);
	CHECK(Values.CreatureInfo == Defaults.CreatureInfo);
	CHECK(Values.WindowedMode == Defaults.WindowedMode);
	CHECK(Values.Hotkeys[1].empty());
	CHECK(Values.SkillBars == Defaults.SkillBars);
	CHECK(Values.White.empty());
	CHECK_FALSE(Values.LightEffects);
	CHECK(std::any_of(Problems.begin(), Problems.end(), [](const std::string &Text) { return Text.starts_with("TestServer=maybe"); }));
	CHECK(std::any_of(Problems.begin(), Problems.end(), [](const std::string &Text) { return Text.starts_with("Hotkey36"); }));
}

TEST_CASE("Settings: Remove and Keys")
{
	TSettingsFile File = TSettingsFile::Parse("# c\nA=1\nb=2\na=3\n");
	CHECK(File.Keys() == std::vector<std::string>{"A", "b", "a"});
	CHECK(File.Remove("a") == 2);
	CHECK(File.Remove("a") == 0);
	CHECK(File.Text() == "# c\nb=2\n");
	CHECK(File.Keys() == std::vector<std::string>{"b"});
}
