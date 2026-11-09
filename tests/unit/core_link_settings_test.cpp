// Where the client's web links go (src/settings/LinkSettings.h, issue #169): each key's button, the original address for
// an unset or a refused value, the encodings, the caption, and that saving the options leaves the keys alone.

#include <doctest/doctest.h>

#include "Text.h" // IWYU pragma: keep (the original addresses)
#include "log/Log.h"
#include "settings/LinkSettings.h"
#include "settings/PreferenceKeys.h"
#include "settings/Settings.h"

#include <cstddef>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <string>
#include <string_view>
#include <vector>

using patella::log::TLog;
using patella::settings::MaxLinkCaption;
using patella::settings::MaxLinkHoverText;
using patella::settings::TLinks;
using patella::settings::TSettingsFile;
using patella::settings::ValidLinkAddress;

namespace {

// The lines a log gives its console, captured.
std::string Captured;
void Capture(std::string_view Line) noexcept
{
	try {
		Captured.append(Line);
	} catch (...) { // NOLINT(bugprone-empty-catch): std::bad_alloc; the test's comparison fails
	}
}

// A TLinks configured from Text, the warnings in Captured.
TLinks Configured(std::string_view Text)
{
	Captured.clear();
	TLog Log(Capture);
	TLinks Links;
	Links.Configure(TSettingsFile::Parse(Text), Log);
	return Links;
}

struct TLinkCase {
	const char *Key;
	const char *Original;
};

// The issue's table, by Text.h's names.
std::vector<TLinkCase> AllLinks()
{
	return {{"LinkWebsite", TextTibiaURL}, {"LinkManual", TextManualURL}, {"LinkFAQ", TextFAQURL},
		{"LinkOptionsHelp", TextOptionsURL}, {"LinkGraphicsHelp", TextGraphicsOptionsURL}, {"LinkConsoleHelp", TextConsoleOptionsURL},
		{"LinkNetworkHelp", TextNetworkOptionsURL}, {"LinkAccount", TextAccountURL}, {"LinkSignup", TextSignupURL}};
}

} // namespace

TEST_CASE("Links: without the keys the original addresses, and no warning")
{
	const TLinks Links = Configured("# no link keys\nDevice=cpu\n");
	for (const TLinkCase &Link : AllLinks()) {
		CHECK(Links.Target(Link.Original) == Link.Original);
		CHECK(Links.HoverText(Link.Original) == Link.Original); // the same pointer
	}
	CHECK(Links.WebsiteLabel().empty());
	CHECK(Captured.empty());
}

TEST_CASE("Links: the process's links are the originals until they are configured")
{
	CHECK(patella::settings::Links().Target(TextTibiaURL) == TextTibiaURL);
	CHECK(patella::settings::Links().WebsiteLabel().empty());
}

TEST_CASE("Links: an empty value is unset")
{
	const TLinks Links = Configured("LinkManual=\nLinkManual=https://ignored.example/\nLinkWebsiteLabel=  \n");
	CHECK(Links.Target(TextManualURL) == TextManualURL); // the first line with a key counts
	CHECK(Links.WebsiteLabel().empty());
	CHECK(Captured.empty());
}

TEST_CASE("Links: each key sends its own button, and only that one")
{
	for (const TLinkCase &Link : AllLinks()) {
		CAPTURE(Link.Key);
		const std::string Address = std::string("https://example.org/") + Link.Key;
		const TLinks Links = Configured(std::string(Link.Key) + "=" + Address + "\n");
		CHECK(Captured.empty());
		for (const TLinkCase &Other : AllLinks()) {
			CAPTURE(Other.Key);
			if (std::string_view(Other.Key) == Link.Key) {
				CHECK(Links.Target(Other.Original) == Address);
				CHECK(std::string_view(Links.HoverText(Other.Original)) == Address);
			} else {
				CHECK(Links.Target(Other.Original) == Other.Original);
				CHECK(Links.HoverText(Other.Original) == Other.Original);
			}
		}
	}
}

TEST_CASE("Links: the graphics help's two buttons have one address, the login dialog's literal is TextSignupURL")
{
	CHECK(std::string_view(TextAdvancedGraphicsURL) == TextGraphicsOptionsURL);
	const TLinks Links = Configured("LinkGraphicsHelp=https://example.org/graphics\nLinkSignup=https://example.org/signup\n");
	CHECK(Links.Target(TextAdvancedGraphicsURL) == "https://example.org/graphics");
	CHECK(Links.Target(TextGraphicsOptionsURL) == "https://example.org/graphics");
	CHECK(Links.Target("http://www.tibia.com/home/?subtopic=signup") == "https://example.org/signup"); // Dialogs.cpp's literal
}

TEST_CASE("Links: another address goes through as the client's text")
{
	const TLinks Links = Configured("LinkManual=https://example.org/\n");
	CHECK(Links.Target("http://other.example/\xe4") == "http://other.example/\xc3\xa4");
	const char *const Other = "http://other.example/";
	CHECK(Links.HoverText(Other) == Other);
}

TEST_CASE("Links: keys are found without regard to case, values keep theirs")
{
	const TLinks Links = Configured("linkmanual = hTTpS://Example.Org/Manual \n");
	CHECK(Links.Target(TextManualURL) == "hTTpS://Example.Org/Manual");
	CHECK(Captured.empty());
}

TEST_CASE("Links: what is an address")
{
	for (const std::string_view Good : {"http://x", "https://x", "HTTP://x", "HtTpS://example.org:8080/a?b=c&d#e", "http://?",
			 "http://%20", "https://example.org/\xc3\xa4", "https://\xe6\x97\xa5\xe6\x9c\xac.example/"}) {
		CAPTURE(Good);
		CHECK(ValidLinkAddress(Good));
	}
	for (const std::string_view Bad : {
			 "",
			 "http://",
			 "https://",
			 "HTTPS://",
			 "http:",
			 "http:/x",
			 "http:\\\\x",
			 "ftp://x",
			 "file:///C:/Windows/system32/calc.exe",
			 "file://x",
			 "C:\\Windows\\system32\\calc.exe",
			 "calc.exe",
			 "javascript:alert(1)",
			 "mailto:a@example.org",
			 " http://x",
			 "http://a b",
			 "http://a\tb",
			 "http://a\rb",
			 "http://a\x7f"
			 "b",
			 "http://a\xc2\x85"
			 "b", // U+0085, a C1 control
			 "http://a\xc2\xa0"
			 "b", // U+00A0
			 "http://a\xe2\x80\x83"
			 "b", // U+2003
			 "http://a\xe2\x80\xa8"
			 "b", // U+2028
			 "http://a\xe3\x80\x80"
			 "b", // U+3000
			 "http://a\xff"
			 "b",            // ill-formed UTF-8
			 "http://a\xc3", // a cut sequence
		 }) {
		CAPTURE(Bad);
		CHECK_FALSE(ValidLinkAddress(Bad));
	}
	CHECK_FALSE(ValidLinkAddress(std::string_view("http://a\0b", 10)));
}

TEST_CASE("Links: a refused value gives the original's address and a warning")
{
	for (const char *const Bad : {"file:///C:/Windows/system32/calc.exe", "C:\\Windows\\system32\\calc.exe", "http://", "http://a b"}) {
		CAPTURE(Bad);
		const TLinks Links = Configured(std::string("LinkManual=") + Bad + "\nLinkFAQ=https://example.org/faq\n");
		CHECK(Links.Target(TextManualURL) == TextManualURL);
		CHECK(Links.HoverText(TextManualURL) == TextManualURL);
		CHECK(Links.Target(TextFAQURL) == "https://example.org/faq");
		CHECK(Captured == std::string("[warn] Patella.ini: LinkManual \"") + Bad + "\" is not an http:// or https:// address without spaces and control characters; the original address is used\n");
	}
}

TEST_CASE("Links: a warning stays one line and not too long")
{
	const std::string Long = "file://" + std::string(100000, 'x');
	Configured("LinkManual=" + Long + "\n");
	CHECK(Captured.size() < 400);
	Configured("LinkManual=http://a\rb\n");
	CHECK(Captured.find('\r') == std::string::npos);
	CHECK(Captured.find("http://a?b") != std::string::npos);
}

TEST_CASE("Links: a UTF-8 address reaches SDL_OpenURL unchanged, the hover text is the client's")
{
	const TLinks Links = Configured("LinkWebsite=https://example.org/caf\xc3\xa9/\xe6\x97\xa5\n");
	CHECK(Captured.empty());
	CHECK(Links.Target(TextTibiaURL) == "https://example.org/caf\xc3\xa9/\xe6\x97\xa5"); // not converted a second time
	CHECK(std::string_view(Links.HoverText(TextTibiaURL)) == "https://example.org/caf\xe9/?");
}

TEST_CASE("Links: a file of PreferencesVersion 1 has the client's bytes")
{
	const TLinks Links = Configured("PreferencesVersion=1\nLinkWebsite=https://example.org/caf\xe9\nLinkWebsiteLabel=caf\xe9\n");
	CHECK(Captured.empty());
	CHECK(Links.Target(TextTibiaURL) == "https://example.org/caf\xc3\xa9");
	CHECK(std::string_view(Links.HoverText(TextTibiaURL)) == "https://example.org/caf\xe9");
	CHECK(Links.WebsiteLabel() == "caf\xe9");

	// Its 0x81 is U+0081, a control; its 0xa0 is U+00A0, a space.
	CHECK(Configured("PreferencesVersion=1\nLinkFAQ=https://example.org/\x81\n").Target(TextFAQURL) == TextFAQURL);
	CHECK(Configured("PreferencesVersion=1\nLinkFAQ=https://example.org/\xa0x\n").Target(TextFAQURL) == TextFAQURL);
}

TEST_CASE("Links: a long address is opened whole, its hover text is cut")
{
	const std::string Address = "https://example.org/" + std::string(1000, 'a');
	const TLinks Links = Configured("LinkSignup=" + Address + "\n");
	CHECK(Captured.empty());
	CHECK(Links.Target(TextSignupURL) == Address);
	CHECK(std::string_view(Links.HoverText(TextSignupURL)) == Address.substr(0, MaxLinkHoverText));
}

TEST_CASE("Links: the caption")
{
	CHECK(Configured("LinkWebsiteLabel=www.example.org\n").WebsiteLabel() == "www.example.org");
	CHECK(Captured.empty());
	CHECK(Configured("LinkWebsiteLabel=Our Site\n").WebsiteLabel() == "Our Site"); // a space is fine in a caption
	CHECK(Captured.empty());
	CHECK(Configured("LinkWebsiteLabel=caf\xc3\xa9\n").WebsiteLabel() == "caf\xe9"); // the client's text
	CHECK(Configured("LinkWebsiteLabel=\xe6\x97\xa5\n").WebsiteLabel() == "?");
	CHECK(Captured.empty());
}

TEST_CASE("Links: a caption with a control character is refused")
{
	CHECK(Configured("LinkWebsiteLabel=a\tb\n").WebsiteLabel().empty());
	CHECK(Captured == "[warn] Patella.ini: LinkWebsiteLabel \"a?b\" has a control character; the original caption is used\n");
	CHECK(Configured("LinkWebsiteLabel=a\xc2\x81"
					 "b\n")
			.WebsiteLabel()
			.empty()); // U+0081: the client's byte 0x81
	CHECK_FALSE(Captured.empty());
	// A character the client's code page does not have is '?', as in every text of Patella.ini: no control any more.
	CHECK(Configured("LinkWebsiteLabel=a\xc2\x85"
					 "b\n")
			  .WebsiteLabel()
		== "a?b");
	CHECK(Captured.empty());
}

TEST_CASE("Links: a caption TTextButton would throw on is cut")
{
	const std::size_t Longest = MaxLinkCaption;
	CHECK(Longest == 49);
	CHECK(Configured("LinkWebsiteLabel=" + std::string(Longest, 'x') + "\n").WebsiteLabel() == std::string(Longest, 'x'));
	CHECK(Captured.empty());
	CHECK(Configured("LinkWebsiteLabel=" + std::string(Longest + 1, 'x') + "\n").WebsiteLabel() == std::string(Longest, 'x'));
	CHECK(Captured.find("is longer than 49 characters") != std::string::npos);
	// Bytes of the client's text, so a value of several bytes per character in the file is not cut early.
	CHECK(Configured("LinkWebsiteLabel=" + std::string("\xc3\xa9") + std::string(Longest - 1, 'x') + "\n").WebsiteLabel().size() == Longest);
	CHECK(Captured.empty());
}

TEST_CASE("Links: a second Configure starts afresh")
{
	Captured.clear();
	TLog Log(Capture);
	TLinks Links;
	Links.Configure(TSettingsFile::Parse("LinkManual=https://example.org/\nLinkWebsiteLabel=Example\n"), Log);
	CHECK(Links.Target(TextManualURL) == "https://example.org/");
	Links.Configure(TSettingsFile::Parse("Device=cpu\n"), Log);
	CHECK(Links.Target(TextManualURL) == TextManualURL);
	CHECK(Links.WebsiteLabel().empty());
}

TEST_CASE("Links: saving the options never writes the keys and keeps the user's")
{
	using patella::settings::TPreferenceValues;
	using patella::settings::WritePreferences;
	const TPreferenceValues Values{};

	TSettingsFile Without = TSettingsFile::Parse("# Patella-Modern's settings\r\nDevice=cpu\r\n");
	WritePreferences(Without, Values);
	for (const TLinkCase &Link : AllLinks())
		CHECK_FALSE(Without.Get(Link.Key).has_value());
	CHECK_FALSE(Without.Get(patella::settings::LinkWebsiteLabelKey).has_value());

	TSettingsFile With = TSettingsFile::Parse("LinkManual=https://example.org/\nDevice=cpu\nLinkWebsiteLabel=Example\n");
	WritePreferences(With, Values);
	const TLinks Links = Configured(With.Text());
	CHECK(Links.Target(TextManualURL) == "https://example.org/");
	CHECK(Links.WebsiteLabel() == "Example");
	CHECK(With.Text().starts_with("LinkManual=https://example.org/\nDevice=cpu\nLinkWebsiteLabel=Example\n"));
}
