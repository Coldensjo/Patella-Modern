// A developer's own values reach what the client reads (Patella-Modern issue #169): the test compiles BuildConfig.h and the
// client's Text.h with tests/unit/build_config/own_values.h, through the definition the option PATELLA_BUILD_CONFIG sets
// (cmake/BuildConfig.cmake), and links nothing of the client, whose values are the defaults'. cmake/BuildConfig.cmake's
// compile of the same file proves its checks pass, and the full build with the option is checked by hand (notes/decisions.md).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#undef PATELLA_BUILD_CONFIG_FILE
#define PATELLA_BUILD_CONFIG_FILE "build_config/own_values.h"
#include "BuildConfig.h"
#include "Text.h" // IWYU pragma: keep (the client's table of the links, this object's own copy)

#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <string_view>

namespace bc = patella::buildconfig;

TEST_CASE("BuildConfig: a developer's file reaches the client's values")
{
	CHECK(std::string_view(bc::LoginServers[0]) == "login1.example.org");
	CHECK(std::string_view(bc::LoginServers[3]) == "192.0.2.10");
	CHECK(bc::LoginPort == 7200);
	CHECK(std::string_view(bc::RsaModulus).substr(0, 12) == "109120132967");
	CHECK(std::string_view(bc::TestServerHost) == "test.example.org");
	CHECK(bc::TestServerPort == 7300);
	CHECK(std::string_view(bc::WebsiteCaption) == "example.org");
	CHECK(std::string_view(bc::SiteName) == "example.org");
	CHECK(std::string_view(bc::BugReportAddress) == "bugs@example.org");

	// What Text.h's table holds for the dialogs, TLink's hover text and OpenURL.
	CHECK(std::string_view(TextTibiaURL) == "https://example.org/");
	CHECK(std::string_view(TextManualURL) == "https://example.org/manual");
	CHECK(std::string_view(TextFAQURL) == "HTTPS://example.org/faq");
	CHECK(std::string_view(TextOptionsURL) == "http://example.org/options");
	CHECK(std::string_view(TextGraphicsOptionsURL) == "https://example.org/graphics");
	CHECK(std::string_view(TextAdvancedGraphicsURL) == "https://example.org/graphics");
	CHECK(std::string_view(TextConsoleOptionsURL) == "https://example.org/console");
	CHECK(std::string_view(TextNetworkOptionsURL) == "https://example.org/network");
	CHECK(std::string_view(TextAccountURL) == "https://example.org/account");
	CHECK(std::string_view(TextSignupURL) == "https://example.org/signup");
}
