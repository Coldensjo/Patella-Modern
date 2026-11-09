// Every value of src/BuildConfigDefaults.h bad, each in its own way (issue #169): cmake/BuildConfig.cmake's compile of this
// file must fail, and the compiler's output must name each value.

#pragma once

#include <array>

namespace patella::buildconfig {

inline constexpr std::array<const char *, 3> LoginServers = {"a b", "", "server3.example.org"}; // three entries, a space, empty
inline constexpr int LoginPort = 0;
inline constexpr const char *RsaModulus = "12a";
inline constexpr const char *TestServerHost = "caf\xc3\xa9.example.org"; // not ASCII
inline constexpr int TestServerPort = 65536;
inline constexpr const char *LinkWebsite = "file:///C:/Windows/system32/calc.exe"; // the wrong scheme
inline constexpr const char *LinkManual = "C:\\Windows\\system32\\calc.exe";       // a program's path
inline constexpr const char *LinkFAQ = "http://";                                  // nothing after the scheme
inline constexpr const char *LinkOptionsHelp = "http://a b/";                      // a space
inline constexpr const char *LinkGraphicsHelp = "http://a\tb/";                    // a control character
inline constexpr const char *LinkConsoleHelp = "http://caf\xc3\xa9/";              // not ASCII
inline constexpr const char *LinkNetworkHelp = "javascript:alert(1)";
inline constexpr const char *LinkAccount = "";
inline constexpr const char *LinkSignup = "ftp://example.org/";
inline constexpr const char *WebsiteCaption = "A caption of more than the forty nine bytes the button holds"; // too long
inline constexpr const char *SiteName = "";                                        // empty
inline constexpr const char *BugReportAddress = "bugs\x01@example.org";            // a control character

} // namespace patella::buildconfig
