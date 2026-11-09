// BuildConfig.h: the values a build for another server fixes at compile time, and their checks (Patella-Modern issue
// #169, the owner's decision after #166). Not part of the original source.
//
// The values are BuildConfigDefaults.h's, or those of a developer's own copy named by the CMake option PATELLA_BUILD_CONFIG
// (cmake/BuildConfig.cmake). Every consumer includes this header, so a value cannot skip the checks below: a bad one fails
// the compile with a message that says what is wrong. The checks are the safety part: SDL_OpenURL is ShellExecute's default
// verb on Windows, so a link that is a program path or a file: address would be started by a button.
//
// These are the only copies of the values in the product (the test build's PATELLA_LOGIN_SERVER has its own default port, issue #158): the login servers and port (Network.cpp), the RSA modulus (Communication.cpp,
// twice), the test server (Player.cpp), the links (Text.h), Info's caption (Dialogs.cpp), the site named in texts (Network.cpp,
// Help.cpp), the bug report address (sysinc.cpp). The values are plain text in the program: someone with a hex editor can still
// change them, and only the server can enforce anything. notes/decisions.md, "#169".
//
// Portable and C++17: nothing but the standard library.

#pragma once

#if defined(PATELLA_BUILD_CONFIG_FILE)
#include PATELLA_BUILD_CONFIG_FILE // IWYU pragma: export
#else
#include "BuildConfigDefaults.h" // IWYU pragma: export
#endif

#include <array>
#include <cstddef>
#include <string_view>

namespace patella::buildconfig {

// The fields the values go into: TServer::Address (Network.h, a TFixedString<100>), TTextButton::Text (DialogElements.h, a
// TFixedString<50>). Network.cpp and Dialogs.cpp hold these to the fields' sizes.
inline constexpr std::size_t MaxHostName = 99;
inline constexpr std::size_t MaxCaption = 49;

// The digits a modulus can have with the protocol's 128-byte block: it must be at most 2^1024, and larger than the block's
// text, which starts with a zero byte, so at least about 2^1016 (306 to 309 digits). A bound on the digit count, not an
// exact check of the number's range.
inline constexpr std::size_t MinModulusDigits = 306;
inline constexpr std::size_t MaxModulusDigits = 309;

namespace check {

constexpr bool StartsWithNoCase(std::string_view Text, std::string_view LowerPrefix) noexcept
{
	if (Text.size() < LowerPrefix.size())
		return false;
	for (std::size_t i = 0; i < LowerPrefix.size(); i++) {
		const char c = Text[i];
		if ((c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c) != LowerPrefix[i])
			return false;
	}
	return true;
}

// Every character is in [Low, '~'], so ASCII without the controls, the DEL and, with Low '!', the space.
constexpr bool IsAsciiRange(std::string_view Text, char Low) noexcept
{
	for (const char c : Text) {
		if (c < Low || c > '~')
			return false;
	}
	return true;
}

constexpr bool IsLink(const char *Value) noexcept
{
	if (Value == nullptr)
		return false;
	const std::string_view Text(Value);
	const std::size_t Scheme = StartsWithNoCase(Text, "http://") ? 7 : StartsWithNoCase(Text, "https://") ? 8
																										  : 0;
	return Scheme != 0 && Text.size() > Scheme && IsAsciiRange(Text, '!');
}

constexpr bool IsHost(const char *Value) noexcept
{
	if (Value == nullptr)
		return false;
	const std::string_view Text(Value);
	return !Text.empty() && Text.size() <= MaxHostName && IsAsciiRange(Text, '!');
}

template <std::size_t Count>
constexpr bool AreHosts(const std::array<const char *, Count> &Values) noexcept
{
	for (const char *const Value : Values) {
		if (!IsHost(Value))
			return false;
	}
	return true;
}

constexpr bool IsPort(int Port) noexcept
{
	return Port >= 1 && Port <= 65535;
}

constexpr bool IsModulus(const char *Value) noexcept
{
	if (Value == nullptr)
		return false;
	const std::string_view Text(Value);
	if (Text.size() < MinModulusDigits || Text.size() > MaxModulusDigits)
		return false;
	for (const char c : Text) {
		if (c < '0' || c > '9')
			return false;
	}
	return true;
}

// Printable ASCII (a space is allowed), not empty.
constexpr bool IsPrintable(const char *Value) noexcept
{
	return Value != nullptr && !std::string_view(Value).empty() && IsAsciiRange(Value, ' ');
}

constexpr bool IsCaption(const char *Value) noexcept
{
	return IsPrintable(Value) && std::string_view(Value).size() <= MaxCaption;
}

} // namespace check

static_assert(LoginServers.size() == 4, "LoginServers must have exactly four entries: repeat one if you have fewer");
static_assert(check::AreHosts(LoginServers), "LoginServers must be non-empty host names of at most 99 ASCII characters without spaces or control characters");
static_assert(check::IsPort(LoginPort), "LoginPort must be a port from 1 to 65535");
static_assert(check::IsModulus(RsaModulus), "RsaModulus must be 306 to 309 decimal digits");
static_assert(check::IsHost(TestServerHost), "TestServerHost must be a non-empty host name of at most 99 ASCII characters without spaces or control characters");
static_assert(check::IsPort(TestServerPort), "TestServerPort must be a port from 1 to 65535");
static_assert(check::IsLink(LinkWebsite), "LinkWebsite must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkManual), "LinkManual must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkFAQ), "LinkFAQ must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkOptionsHelp), "LinkOptionsHelp must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkGraphicsHelp), "LinkGraphicsHelp must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkConsoleHelp), "LinkConsoleHelp must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkNetworkHelp), "LinkNetworkHelp must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkAccount), "LinkAccount must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsLink(LinkSignup), "LinkSignup must start with http:// or https://, have something after that and hold only ASCII without spaces or control characters");
static_assert(check::IsCaption(WebsiteCaption), "WebsiteCaption must be 1 to 49 printable ASCII characters");
static_assert(check::IsPrintable(SiteName), "SiteName must be printable ASCII and not empty");
static_assert(check::IsPrintable(BugReportAddress), "BugReportAddress must be printable ASCII and not empty");

// A text with a value inside it, composed at compile time: no run-time work and no allocation, the original's literal in
// another form. ComposedSize(A, B, ...) is the size of the array ComposeText<Size>(A, B, ...) returns, with its NUL.
template <typename... TParts>
constexpr std::size_t ComposedSize(TParts... Parts) noexcept
{
	return (std::string_view(Parts).size() + ... + 1);
}

template <std::size_t Size, typename... TParts>
constexpr std::array<char, Size> ComposeText(TParts... Parts) noexcept
{
	std::array<char, Size> Text{};
	std::size_t End = 0;
	const auto Append = [&Text, &End](std::string_view Part) constexpr noexcept {
		for (const char c : Part)
			Text[End++] = c;
	};
	(Append(Parts), ...);
	return Text;
}

} // namespace patella::buildconfig
