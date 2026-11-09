// BuildConfigDefaults.h: the values someone fixes when building the client for their own server (Patella-Modern issue
// #169, the owner's decision after #166). Not part of the original source. BuildConfig.h includes this file and checks
// every value at compile time; include BuildConfig.h, not this.
//
// To build for your own server, copy this file somewhere outside the repository, change the values in the copy and
// configure with -DPATELLA_BUILD_CONFIG=<your copy>: your file replaces this one, so pulling updates never conflicts with
// your values, and it goes through the same checks. These are today's values, byte for byte: a build that doesn't change
// them behaves exactly as the original. README.md, "Building for your own server".
//
// Every value must be ASCII. A bad value fails the compile with a message that says what is wrong. Nothing here is read
// from Patella.ini or the environment (the test build's PATELLA_LOGIN_SERVER and PATELLA_RSA_MODULUS are the test build's,
// issue #158). The values end up as plain text in the program: they are not a secret.

#pragma once

#include <array>

namespace patella::buildconfig {

// The login servers: always four entries (if you have fewer, repeat one), each at most 99 characters without spaces.
inline constexpr std::array<const char *, 4> LoginServers = {"server.tibia.com", "server2.tibia.com", "tibia1.cipsoft.com",
	"tibia2.cipsoft.com"};

// Their port, 1 to 65535.
inline constexpr int LoginPort = 7171;

// The RSA modulus of the login, in decimal digits: the public half of your server's 1024-bit key (306 to 309 digits).
inline constexpr const char *RsaModulus = "124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284902061793";

// The test server (Network Options' "Test Server Login"): its name and port.
inline constexpr const char *TestServerHost = "test.cipsoft.com";
inline constexpr int TestServerPort = 7172;

// Where the buttons that open a web page go. Each must start with http:// or https:// (ASCII case-insensitive), have
// something after that, and hold no space or control character: a program path or a file: address would be started by a
// button (SDL_OpenURL is ShellExecute's default verb on Windows).
inline constexpr const char *LinkWebsite = "http://www.tibia.com/";                                           // Info's button
inline constexpr const char *LinkManual = "http://www.tibia.com/guide/?subtopic=manual";                      // Help
inline constexpr const char *LinkFAQ = "http://www.tibia.com/guide/?subtopic=faq";                            // Help
inline constexpr const char *LinkOptionsHelp = "http://www.tibia.com/guide/?subtopic=manual&section=options"; // General Options
// Graphics Options and Advanced Graphics
inline constexpr const char *LinkGraphicsHelp = "http://www.tibia.com/guide/?subtopic=manual&section=options#graphicsoptions";
inline constexpr const char *LinkConsoleHelp = "http://www.tibia.com/guide/?subtopic=manual&section=options#consoleoptions"; // Console Options
inline constexpr const char *LinkNetworkHelp = "http://www.tibia.com/guide/?subtopic=manual&section=options#networkoptions"; // Network Options
inline constexpr const char *LinkAccount = "http://www.tibia.com/home/?subtopic=account";                                    // the title screen's Access Account
inline constexpr const char *LinkSignup = "http://www.tibia.com/home/?subtopic=signup";                                      // Login's Create Account

// The caption of Info's website button: at most 49 characters. The button is 86 pixels wide and its text is not clipped,
// so keep it to about 17 characters or it runs over the text beside it.
inline constexpr const char *WebsiteCaption = "www.tibia.com";

// The site the connection errors and the first hint name, and the address the error text gives for bug reports.
inline constexpr const char *SiteName = "www.tibia.com";
inline constexpr const char *BugReportAddress = "cip@tibia.com";

} // namespace patella::buildconfig
