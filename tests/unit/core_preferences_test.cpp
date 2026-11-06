// PreferenceKeys (src/settings/PreferenceKeys.cpp, issue #45): the options of Tibia.cfg as keys of Patella.ini, the
// fields of a value and the escaping of texts. The import of a Tibia.cfg itself (Preferences.cpp InitPreferences, the
// original's reader) is tests/golden/preferences_file_test.cpp's.

#include <doctest/doctest.h>

#include "settings/PreferenceKeys.h"
#include "settings/Settings.h"
#include "types/Cp1252.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using patella::settings::ConvertToUtf8;
using patella::settings::ParseFields;
using patella::settings::QuoteText;
using patella::settings::ReadPreferences;
using patella::settings::TEncoding;
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
	// Issue #117: the client's Windows-1252 as UTF-8 (version 2), as it is in a file of version 1.
	CHECK(QuoteText("\xe4\xff\x80") == "\"\xc3\xa4\xc3\xbf\xe2\x82\xac\"");
	CHECK(QuoteText("\xe4\xff\x80", TEncoding::Cp1252) == "\"\xe4\xff\x80\"");
	// The five bytes Windows-1252 leaves undefined (C1 controls) are escaped in UTF-8, as they are in version 1.
	CHECK(QuoteText("\x81\x8d\x8f\x90\x9d") == "\"\\x81\\x8d\\x8f\\x90\\x9d\"");
	CHECK(QuoteText("\x81", TEncoding::Cp1252) == "\"\x81\"");
	std::string All;
	for (int c = 1; c < 256; c++)
		All += static_cast<char>(c);
	for (const TEncoding Encoding : {TEncoding::Utf8, TEncoding::Cp1252}) {
		const std::optional<std::vector<TField>> Fields = ParseFields(QuoteText(All, Encoding), Encoding);
		REQUIRE(Fields.has_value());
		REQUIRE(Fields->size() == 1);
		CHECK((*Fields)[0].Kind == TField::TKind::Text);
		CHECK((*Fields)[0].Text == All);
		// The quoted text is a value Patella.ini keeps: no line end, no space at its ends.
		TSettingsFile File;
		CHECK(File.Set("Text", QuoteText(" " + All + " ", Encoding)));
		CHECK(ParseFields(*TSettingsFile::Parse(File.Text()).Get("Text"), Encoding)->at(0).Text == " " + All + " ");
	}
}

TEST_CASE("Preferences: every Windows-1252 byte in UTF-8 and back (issue #117)")
{
	for (int c = 1; c < 256; c++) {
		const auto Byte = static_cast<unsigned char>(c);
		const std::string Text(1, static_cast<char>(Byte));
		const std::string Quoted = QuoteText(Text);
		CAPTURE(c);
		const bool Escaped = Byte < 0x20 || Byte == 0x7f || Byte == 0x81 || Byte == 0x8d || Byte == 0x8f || Byte == 0x90 || Byte == 0x9d;
		if (Escaped || Byte == '"' || Byte == '\\') {
			CHECK(Quoted[1] == '\\');
		} else {
			// The UTF-8 of the byte's character (types/Cp1252.h), nothing else.
			CHECK(Quoted == "\"" + patella::text::Cp1252ToUtf8(Text) + "\"");
		}
		const auto Fields = ParseFields(Quoted);
		REQUIRE(Fields.has_value());
		CHECK((*Fields)[0].Text == Text);
		// The character itself, unescaped, reads as its byte too (a C1 control written by an editor).
		if (Byte >= 0x80)
			CHECK(ParseFields("\"" + patella::text::Cp1252ToUtf8(Text) + "\"")->at(0).Text == Text);
		// `\xHH` is the client's byte in both encodings, never part of the UTF-8.
		static constexpr std::string_view Hex = "0123456789abcdef";
		const std::string Escape = std::string("\"\\x") + Hex[Byte >> 4] + Hex[Byte & 15] + "\"";
		CHECK(ParseFields(Escape)->at(0).Text == Text);
		CHECK(ParseFields(Escape, TEncoding::Cp1252)->at(0).Text == Text);
	}
	// A character Windows-1252 does not have, and each ill-formed part of the UTF-8: '?'.
	CHECK(ParseFields("\"\xc5\x81\xc3\xb3"
					  "d\xc5\xba \xe6\x97\xa5\xf0\x9f\x98\x80\"")
			  ->at(0)
			  .Text
		== "?\xf3"
		   "d? ??");
	CHECK(ParseFields("\"a\xff"
					  "b\xc3\"")
			  ->at(0)
			  .Text
		== "a?b?");
	CHECK(ParseFields("\"\xe2\x82\\x41\"")->at(0).Text == "?A");
	// In a file of version 1 the same bytes are the client's as they are.
	CHECK(ParseFields("\"\xc3\xa4\"", TEncoding::Cp1252)->at(0).Text == "\xc3\xa4");
}

TEST_CASE("Preferences: a file of version 1 is read as Windows-1252 and written once as version 2 (issue #117)")
{
	// #45's client wrote the client's bytes as they are: ö (0xf6), € (0x80) and the undefined 0x81.
	const std::string Version1 = "# Patella-Modern's settings, caf\xe9\r\n"
								 "Device=cpu\r\n"
								 "LogFile=log-\xe9.txt\r\n"
								 "PreferencesVersion=1\r\n"
								 "Hotkey0=\"K\xf6nig \x80 \x81!\",yes\r\n"
								 "Buddy1=42,1,\"Gr\xf6\xdf"
								 "e \x80\x81\",no\r\n"
								 "White=\"J\xf6rg\"\r\n"
								 "Ignore=\"\x80uro\"\r\n";
	TSettingsFile File = TSettingsFile::Parse(Version1);
	CHECK(patella::settings::EncodingOf(File) == TEncoding::Cp1252);
	CHECK(patella::settings::ClientText(File, *File.Get("LogFile")) == "log-\xe9.txt");
	TPreferenceValues Values;
	CHECK(ReadPreferences(File, Values).empty());
	CHECK(Values.Hotkeys[0] == "K\xf6nig \x80 \x81!");
	CHECK(Values.HotkeyAutoSend[0]);
	REQUIRE(Values.Buddies.size() == 1);
	CHECK(Values.Buddies[0].Description == "Gr\xf6\xdf"
										   "e \x80\x81");
	CHECK(Values.White == std::vector<std::string>{"J\xf6rg"});
	CHECK(Values.Ignore == std::vector<std::string>{"\x80uro"});

	WritePreferences(File, Values);
	CHECK(patella::settings::EncodingOf(File) == TEncoding::Utf8);
	CHECK(File.Get("PreferencesVersion") == "2");
	const std::string Text = File.Text();
	// Every line in UTF-8: the comment and LogFile converted too, the texts of the options written anew.
	CHECK(Text.starts_with("# Patella-Modern's settings, caf\xc3\xa9\r\nDevice=cpu\r\nLogFile=log-\xc3\xa9.txt\r\nPreferencesVersion=2\r\n"));
	CHECK(File.Get("Hotkey0") == "\"K\xc3\xb6nig \xe2\x82\xac \\x81!\",yes");
	CHECK(File.Get("Buddy1") == "42,1,\"Gr\xc3\xb6\xc3\x9f"
								"e \xe2\x82\xac\\x81\",no");
	CHECK(File.Get("White") == "\"J\xc3\xb6rg\"");
	CHECK(File.Get("Ignore") == "\"\xe2\x82\xacuro\"");
	CHECK(patella::settings::ClientText(File, *File.Get("LogFile")) == "log-\xe9.txt");
	// The values by value unchanged, and written again: the same text.
	const TSettingsFile Again = TSettingsFile::Parse(Text);
	TPreferenceValues Read;
	CHECK(ReadPreferences(Again, Read).empty());
	CHECK(Read == Values);
	WritePreferences(File, Values);
	CHECK(File.Text() == Text);
	// A file in UTF-8 is not converted again.
	TSettingsFile Utf8 = TSettingsFile::Parse(Text);
	ConvertToUtf8(Utf8);
	CHECK(Utf8.Text() == Text);
	// A byte order mark stays as it is.
	TSettingsFile Marked = TSettingsFile::Parse("\xEF\xBB\xBFPreferencesVersion=1\nWhite=\"\xe9\"\n");
	ConvertToUtf8(Marked);
	CHECK(Marked.Text() == "\xEF\xBB\xBFPreferencesVersion=1\nWhite=\"\xc3\xa9\"\n");
}

TEST_CASE("Preferences: the encoding of a file by its version (issue #117)")
{
	using patella::settings::ClientText;
	using patella::settings::EncodingOf;
	CHECK(EncodingOf(TSettingsFile::Parse("")) == TEncoding::Utf8);
	CHECK(EncodingOf(TSettingsFile::Parse("Device=cpu\n")) == TEncoding::Utf8);
	CHECK(EncodingOf(TSettingsFile::Parse("PreferencesVersion = 1\n")) == TEncoding::Cp1252);
	CHECK(EncodingOf(TSettingsFile::Parse("preferencesversion=2\n")) == TEncoding::Utf8);
	CHECK(ClientText(TSettingsFile::Parse("PreferencesVersion=2\n"), "J\xc3\xb6rg \xe2\x82\xac \xc5\x81") == "J\xf6rg \x80 ?");
	// A version this client does not know: reported, read as UTF-8.
	const TSettingsFile Newer = TSettingsFile::Parse("PreferencesVersion=3\nWhite=\"J\xc3\xb6rg\"\n");
	TPreferenceValues Values;
	const std::vector<std::string> Problems = ReadPreferences(Newer, Values);
	REQUIRE(Problems.size() == 1);
	CHECK(Problems[0].starts_with("PreferencesVersion=3"));
	CHECK(Values.White == std::vector<std::string>{"J\xf6rg"});
	// A problem's value is the client's text (the log writes it as UTF-8 again).
	TPreferenceValues Bad;
	const std::vector<std::string> Found = ReadPreferences(TSettingsFile::Parse("PreferencesVersion=2\nWhite=\"\xc3\xb6\",1\n"), Bad);
	REQUIRE(Found.size() == 1);
	CHECK(Found[0].starts_with("White=\"\xf6\",1:"));
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
	CHECK(File.Get("PreferencesVersion") == "2");
	CHECK(File.Get("TestServer") == "yes");
	CHECK(File.Get("FullscreenMode") == "1024,768,32");
	CHECK(File.Get("Hotkey0") == "\"hello patella\",no");
	CHECK(File.Get("Hotkey1") == "\"exura\",yes");
	CHECK_FALSE(File.Get("Hotkey2").has_value());
	CHECK(File.Get("WindowedMode") == "-4,6,800,600,yes");
	CHECK(File.Get("ContentWindow2") == "3,-1");
	CHECK(File.Get("OpenDialogs") == "4,1");
	CHECK(File.Get("OpenChannel") == "5,0,7");
	CHECK(File.Get("Buddy3") == "99,1,\"\xc3\x84rger\",no");
	CHECK(File.Get("White") == "\"Some Friend\",\"Ein G\xc3\xa4ste, Gruss\"");
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
	CHECK(First.starts_with("# Patella-Modern's settings\r\nDevice=cpu\r\nLogLevel=debug\r\nPreferencesVersion=2\r\n"));
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
