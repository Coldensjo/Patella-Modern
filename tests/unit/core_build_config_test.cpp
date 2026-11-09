// The build-time values (src/BuildConfig.h, Patella-Modern issue #169): every default is the original's value, byte for byte,
// Text.h's links read the constants, and the compile-time checks accept and refuse the right things. The other places are
// held by the golden corpus (byte-identical with the defaults) and a full build with other values (notes/decisions.md).
//
// Compiled without a developer's own file (PATELLA_BUILD_CONFIG, cmake/BuildConfig.cmake), which has other values on purpose.

#include <doctest/doctest.h>

#include "BuildConfig.h"
#include "Text.h" // IWYU pragma: keep (the client's table of the links, this object's own copy)

#include <array>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <string>
#include <string_view>

namespace bc = patella::buildconfig;
namespace check = patella::buildconfig::check;

#if !defined(PATELLA_BUILD_CONFIG_FILE)

TEST_CASE("BuildConfig: the defaults are the original's values")
{
	CHECK(std::string_view(bc::LoginServers[0]) == "server.tibia.com");
	CHECK(std::string_view(bc::LoginServers[1]) == "server2.tibia.com");
	CHECK(std::string_view(bc::LoginServers[2]) == "tibia1.cipsoft.com");
	CHECK(std::string_view(bc::LoginServers[3]) == "tibia2.cipsoft.com");
	CHECK(bc::LoginPort == 7171);
	CHECK(std::string_view(bc::RsaModulus) == "124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284902061793");
	CHECK(std::string_view(bc::TestServerHost) == "test.cipsoft.com");
	CHECK(bc::TestServerPort == 7172);
	CHECK(std::string_view(bc::LinkWebsite) == "http://www.tibia.com/");
	CHECK(std::string_view(bc::LinkManual) == "http://www.tibia.com/guide/?subtopic=manual");
	CHECK(std::string_view(bc::LinkFAQ) == "http://www.tibia.com/guide/?subtopic=faq");
	CHECK(std::string_view(bc::LinkOptionsHelp) == "http://www.tibia.com/guide/?subtopic=manual&section=options");
	CHECK(std::string_view(bc::LinkGraphicsHelp) == "http://www.tibia.com/guide/?subtopic=manual&section=options#graphicsoptions");
	CHECK(std::string_view(bc::LinkConsoleHelp) == "http://www.tibia.com/guide/?subtopic=manual&section=options#consoleoptions");
	CHECK(std::string_view(bc::LinkNetworkHelp) == "http://www.tibia.com/guide/?subtopic=manual&section=options#networkoptions");
	CHECK(std::string_view(bc::LinkAccount) == "http://www.tibia.com/home/?subtopic=account");
	CHECK(std::string_view(bc::LinkSignup) == "http://www.tibia.com/home/?subtopic=signup");
	CHECK(std::string_view(bc::WebsiteCaption) == "www.tibia.com");
	CHECK(std::string_view(bc::SiteName) == "www.tibia.com");
	CHECK(std::string_view(bc::BugReportAddress) == "cip@tibia.com");
}

TEST_CASE("BuildConfig: Text.h's links are the constants")
{
	CHECK(std::string_view(TextTibiaURL) == bc::LinkWebsite);
	CHECK(std::string_view(TextManualURL) == bc::LinkManual);
	CHECK(std::string_view(TextFAQURL) == bc::LinkFAQ);
	CHECK(std::string_view(TextOptionsURL) == bc::LinkOptionsHelp);
	CHECK(std::string_view(TextGraphicsOptionsURL) == bc::LinkGraphicsHelp);
	CHECK(std::string_view(TextAdvancedGraphicsURL) == bc::LinkGraphicsHelp);
	CHECK(std::string_view(TextConsoleOptionsURL) == bc::LinkConsoleHelp);
	CHECK(std::string_view(TextNetworkOptionsURL) == bc::LinkNetworkHelp);
	CHECK(std::string_view(TextAccountURL) == bc::LinkAccount);
	CHECK(std::string_view(TextSignupURL) == bc::LinkSignup);
}

// The texts composed with SiteName and BugReportAddress (Network.cpp's, Help.cpp's, sysinc.cpp's) are compared with the
// original's, byte for byte, by the golden corpus; they need the client's objects, which this program does not link.

#endif

TEST_CASE("BuildConfig: what is a link")
{
	for (const char *const Good : {"http://x", "https://x", "HTTP://x", "HtTpS://example.org:8080/a?b=c&d#e%20", "http://?"}) {
		CAPTURE(Good);
		CHECK(check::IsLink(Good));
	}
	for (const char *const Bad : {"", "http://", "https://", "HTTPS://", "http:", "http:/x", "http:\\\\x", "ftp://x", "file:///C:/Windows/system32/calc.exe",
			 "file://x", "C:\\Windows\\system32\\calc.exe", "calc.exe", "javascript:alert(1)", "mailto:a@example.org", " http://x", "http://a b", "http://a\tb", "http://a\nb",
			 "http://a\x7f"
			 "b",
			 "http://caf\xc3\xa9/",
			 "http://a\xff"
			 "b"}) {
		CAPTURE(Bad);
		CHECK_FALSE(check::IsLink(Bad));
	}
	CHECK_FALSE(check::IsLink(nullptr));
}

TEST_CASE("BuildConfig: what is a host, a port, a modulus, a caption")
{
	CHECK(check::IsHost("server.example.org"));
	CHECK(check::IsHost("127.0.0.1"));
	CHECK(check::IsHost("a"));
	CHECK_FALSE(check::IsHost(""));
	CHECK_FALSE(check::IsHost(nullptr));
	CHECK_FALSE(check::IsHost("a b"));
	CHECK_FALSE(check::IsHost("a\tb"));
	CHECK_FALSE(check::IsHost("caf\xc3\xa9"));
	const std::string Longest(bc::MaxHostName, 'x');
	CHECK(check::IsHost(Longest.c_str()));
	CHECK_FALSE(check::IsHost(std::string(bc::MaxHostName + 1, 'x').c_str()));

	CHECK(check::IsPort(1));
	CHECK(check::IsPort(65535));
	CHECK_FALSE(check::IsPort(0));
	CHECK_FALSE(check::IsPort(-1));
	CHECK_FALSE(check::IsPort(65536));

	const std::string Digits(308, '7');
	CHECK(check::IsModulus(Digits.c_str()));
	CHECK_FALSE(check::IsModulus(nullptr));
	CHECK_FALSE(check::IsModulus(""));
	CHECK_FALSE(check::IsModulus(std::string(bc::MinModulusDigits - 1, '7').c_str()));
	CHECK_FALSE(check::IsModulus(std::string(bc::MaxModulusDigits + 1, '7').c_str()));
	CHECK_FALSE(check::IsModulus((std::string(307, '7') + "a").c_str()));

	CHECK(check::IsCaption("www.example.org"));
	CHECK(check::IsCaption("Our Site"));
	CHECK_FALSE(check::IsCaption(""));
	CHECK_FALSE(check::IsCaption(nullptr));
	CHECK_FALSE(check::IsCaption("a\tb"));
	CHECK_FALSE(check::IsCaption("caf\xc3\xa9"));
	CHECK(check::IsCaption(std::string(bc::MaxCaption, 'x').c_str()));
	CHECK_FALSE(check::IsCaption(std::string(bc::MaxCaption + 1, 'x').c_str()));

	CHECK(check::IsPrintable("www.example.org"));
	CHECK(check::IsPrintable("a b"));
	CHECK_FALSE(check::IsPrintable(""));
	CHECK_FALSE(check::IsPrintable("a\x01"
								   "b"));
	CHECK_FALSE(check::IsPrintable("caf\xc3\xa9"));
}

TEST_CASE("BuildConfig: ComposeText joins the parts at compile time")
{
	constexpr const char *Before = "see ";
	constexpr const char *Site = "example.org";
	constexpr const char *After = ".";
	static constexpr auto Text = bc::ComposeText<bc::ComposedSize(Before, Site, After)>(Before, Site, After);
	CHECK(std::string_view(Text.data()) == "see example.org.");
	CHECK(Text.size() == 17);
	CHECK(Text[16] == '\0');
}
