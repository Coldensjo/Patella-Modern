// A developer's own copy of src/BuildConfigDefaults.h, for the tests (issue #169): every value differs from the defaults, and
// every one passes BuildConfig.h's checks. cmake/BuildConfig.cmake compiles it through the same definition as
// PATELLA_BUILD_CONFIG, and tests_build_config_own shows it reaches the values the client reads.

#pragma once

#include <array>

namespace patella::buildconfig {

inline constexpr std::array<const char *, 4> LoginServers = {"login1.example.org", "login2.example.org", "login1.example.org",
	"192.0.2.10"};
inline constexpr int LoginPort = 7200;
inline constexpr const char *RsaModulus = "109120132967399429278860960508995541528237502902798129123468757937266291492576446330739696001110603907230888610072655818825358503429057592827629436413108566029093628212635953836686562675849720620786279431090218017681061521755056710823876476444260558147179707119674283982419152118103759076030616683978566631413";
inline constexpr const char *TestServerHost = "test.example.org";
inline constexpr int TestServerPort = 7300;
inline constexpr const char *LinkWebsite = "https://example.org/";
inline constexpr const char *LinkManual = "https://example.org/manual";
inline constexpr const char *LinkFAQ = "HTTPS://example.org/faq";
inline constexpr const char *LinkOptionsHelp = "http://example.org/options";
inline constexpr const char *LinkGraphicsHelp = "https://example.org/graphics";
inline constexpr const char *LinkConsoleHelp = "https://example.org/console";
inline constexpr const char *LinkNetworkHelp = "https://example.org/network";
inline constexpr const char *LinkAccount = "https://example.org/account";
inline constexpr const char *LinkSignup = "https://example.org/signup";
inline constexpr const char *WebsiteCaption = "example.org";
inline constexpr const char *SiteName = "example.org";
inline constexpr const char *BugReportAddress = "bugs@example.org";

} // namespace patella::buildconfig
