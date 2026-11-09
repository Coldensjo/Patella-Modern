// LinkSettings.cpp: where the client's web links go (LinkSettings.h; Patella-Modern issue #169).

#include "settings/LinkSettings.h"

#include "Text.h" // this object's own copy of the text table: the original addresses
#include "log/Log.h"
#include "platform/FileSystem.h"
#include "settings/Settings.h"
#include "types/Cp1252.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace patella::settings {

namespace {

struct TLinkKey {
	const char *Key;
	const char *Address;
};

// TextAdvancedGraphicsURL is the same text as TextGraphicsOptionsURL, so LinkGraphicsHelp is both buttons'; the login
// dialog passes TextSignupURL's text as a literal.
std::array<TLinkKey, 9> LinkKeys()
{
	return {{{"LinkWebsite", TextTibiaURL},
		{"LinkManual", TextManualURL},
		{"LinkFAQ", TextFAQURL},
		{"LinkOptionsHelp", TextOptionsURL},
		{"LinkGraphicsHelp", TextGraphicsOptionsURL},
		{"LinkConsoleHelp", TextConsoleOptionsURL},
		{"LinkNetworkHelp", TextNetworkOptionsURL},
		{"LinkAccount", TextAccountURL},
		{"LinkSignup", TextSignupURL}}};
}

constexpr bool IsControl(char32_t CodePoint) noexcept
{
	return CodePoint < 0x20 || (CodePoint >= 0x7f && CodePoint <= 0x9f);
}

// Unicode's White_Space outside the controls.
constexpr bool IsSpace(char32_t CodePoint) noexcept
{
	return CodePoint == 0x20 || CodePoint == 0xa0 || CodePoint == 0x1680 || (CodePoint >= 0x2000 && CodePoint <= 0x200a)
		|| CodePoint == 0x2028 || CodePoint == 0x2029 || CodePoint == 0x202f || CodePoint == 0x205f || CodePoint == 0x3000;
}

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

// A byte of the client's text that is a control character.
bool HasControl(std::string_view ClientBytes) noexcept
{
	for (const char c : ClientBytes) {
		if (IsControl(patella::text::Cp1252ToCodePoint(static_cast<unsigned char>(c))))
			return true;
	}
	return false;
}

// A value for a warning line: the client's text (the log converts its lines from that), not too long, '?' for a control
// character: the value is what the warning is about, and a line must stay one line.
std::string ForWarning(const TSettingsFile &File, std::string_view Value)
{
	constexpr std::size_t Shown = 200;
	std::string Text = ClientText(File, Value.substr(0, Shown));
	for (char &c : Text) {
		if (IsControl(patella::text::Cp1252ToCodePoint(static_cast<unsigned char>(c))))
			c = patella::text::Replacement;
	}
	return Text;
}

} // namespace

bool ValidLinkAddress(std::string_view Address)
{
	std::size_t Scheme = 0;
	if (StartsWithNoCase(Address, "http://"))
		Scheme = std::string_view("http://").size();
	else if (StartsWithNoCase(Address, "https://"))
		Scheme = std::string_view("https://").size();
	else
		return false;
	if (Address.size() == Scheme)
		return false;
	for (std::size_t i = Scheme; i < Address.size();) {
		const patella::text::detail::TDecoded Decoded = patella::text::detail::DecodeUtf8(Address, i);
		if (!Decoded.Valid || IsControl(Decoded.CodePoint) || IsSpace(Decoded.CodePoint))
			return false;
		i += Decoded.Length;
	}
	return true;
}

void TLinks::Configure(const TSettingsFile &File, patella::log::TLog &Log)
{
	Entries.clear();
	Label.clear();
	const bool Cp1252 = EncodingOf(File) == TEncoding::Cp1252;
	for (const TLinkKey &Link : LinkKeys()) {
		const std::optional<std::string> Value = File.Get(Link.Key);
		if (!Value.has_value() || Value->empty())
			continue;
		std::string Address = Cp1252 ? patella::text::Cp1252ToUtf8(*Value) : *Value;
		if (!ValidLinkAddress(Address)) {
			Log.Warn("{}: {} \"{}\" is not an http:// or https:// address without spaces and control characters; the original address is used",
				FileName, Link.Key, ForWarning(File, *Value));
			continue;
		}
		std::string Hover = ClientText(File, *Value);
		if (Hover.size() > MaxLinkHoverText)
			Hover.resize(MaxLinkHoverText);
		Entries.push_back({Link.Address, std::move(Address), std::move(Hover)});
	}
	if (const std::optional<std::string> Value = File.Get(LinkWebsiteLabelKey); Value.has_value() && !Value->empty()) {
		std::string Text = ClientText(File, *Value);
		if (HasControl(Text)) {
			Log.Warn("{}: {} \"{}\" has a control character; the original caption is used", FileName, LinkWebsiteLabelKey,
				ForWarning(File, *Value));
			return;
		}
		if (Text.size() > MaxLinkCaption) {
			Text.resize(MaxLinkCaption);
			Log.Warn("{}: {} is longer than {} characters; cut to \"{}\"", FileName, LinkWebsiteLabelKey, MaxLinkCaption, Text);
		}
		Label = std::move(Text);
	}
}

const TLinks::TEntry *TLinks::Find(const char *Address) const
{
	for (const TEntry &Entry : Entries) {
		if (Entry.Original == Address)
			return &Entry;
	}
	return nullptr;
}

std::string TLinks::Target(const char *Address) const
{
	const TEntry *const Entry = Find(Address);
	return Entry != nullptr ? Entry->Target : patella::text::Cp1252ToUtf8(Address);
}

const char *TLinks::HoverText(const char *Address) const
{
	const TEntry *const Entry = Find(Address);
	return Entry != nullptr ? Entry->HoverText.c_str() : Address;
}

TLinks &Links()
{
	static TLinks Instance;
	return Instance;
}

void LoadLinks(patella::platform::IFileSystem &Files)
{
	if (const std::optional<TSettingsFile> Settings = Load(Files, FileName))
		Links().Configure(*Settings, patella::log::Log());
}

} // namespace patella::settings
