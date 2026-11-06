// Cp1252.h: the client's 8-bit text and Unicode (Patella-Modern issue #112; plan D4). Not part of the original source.
//
// The client keeps the original's 8-bit text inside: the 7.72 protocol, Tibia.dat, Tibia.cfg and Patella.ini (#45)
// are in Windows-1252 (Latin-1 with the printable characters of 0x80-0x9f), and so is every string of the client.
// Where its text meets the platform, the text is converted (notes/decisions.md, "#112: text encoding at the platform
// boundary"): UTF-8 for Linux and the developer log, UTF-16 for Windows' wide functions.
//
//	std::string Name = patella::text::Utf8ToCp1252(Utf8);      // the platform's text into the client
//	std::string Utf8 = patella::text::Cp1252ToUtf8(Name);      // the client's text out to the platform
//
// The choices (notes/decisions.md, #112):
//	- The five bytes Windows-1252 leaves undefined (0x81, 0x8d, 0x8f, 0x90, 0x9d) are the C1 controls of the same
//	  value (U+0081 ...), as Windows' own code page 1252 maps them (MultiByteToWideChar) and as the WHATWG encoding
//	  standard does: every byte has a character, so the client's text goes out and comes back unchanged.
//	- A character with no Windows-1252 byte becomes Replacement ('?', the default character of Windows' conversion
//	  without best fit), one per character; so does each ill-formed part of the UTF-8 or UTF-16 (a maximal subpart
//	  of UTF-8, as Unicode's chapter 3 counts them, or a lone surrogate). The C0 controls and NUL convert as
//	  themselves; nothing is dropped.
//
// Header only and without a platform header: patella_core's code, types/File.h (tests_unit) and the platform layer
// use the same functions.

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace patella::text {

// The default for a character the client's code page does not have.
inline constexpr char Replacement = '?';

namespace detail {

// The characters of 0x80-0x9f; the rest of Windows-1252 is Latin-1 (the byte's value is the code point).
inline constexpr std::array<char16_t, 32> HighControls = {
	0x20ac,
	0x0081,
	0x201a,
	0x0192,
	0x201e,
	0x2026,
	0x2020,
	0x2021, // 0x80-0x87
	0x02c6,
	0x2030,
	0x0160,
	0x2039,
	0x0152,
	0x008d,
	0x017d,
	0x008f, // 0x88-0x8f
	0x0090,
	0x2018,
	0x2019,
	0x201c,
	0x201d,
	0x2022,
	0x2013,
	0x2014, // 0x90-0x97
	0x02dc,
	0x2122,
	0x0161,
	0x203a,
	0x0153,
	0x009d,
	0x017e,
	0x0178, // 0x98-0x9f
};

inline void AppendUtf8(std::string &Out, char32_t CodePoint)
{
	if (CodePoint < 0x80) {
		Out += static_cast<char>(CodePoint);
	} else if (CodePoint < 0x800) {
		Out += static_cast<char>(0xc0 | (CodePoint >> 6));
		Out += static_cast<char>(0x80 | (CodePoint & 0x3f));
	} else if (CodePoint < 0x10000) {
		Out += static_cast<char>(0xe0 | (CodePoint >> 12));
		Out += static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3f));
		Out += static_cast<char>(0x80 | (CodePoint & 0x3f));
	} else {
		Out += static_cast<char>(0xf0 | (CodePoint >> 18));
		Out += static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3f));
		Out += static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3f));
		Out += static_cast<char>(0x80 | (CodePoint & 0x3f));
	}
}

// The code point of the UTF-8 sequence at Text[Index] and its length; Valid false (and Length the bytes of its
// maximal subpart, at least 1) where it is ill-formed. Unicode 15, table 3-7.
struct TDecoded {
	char32_t CodePoint;
	std::size_t Length;
	bool Valid;
};

constexpr TDecoded DecodeUtf8(std::string_view Text, std::size_t Index) noexcept
{
	const auto Byte = [&](std::size_t i) { return static_cast<unsigned char>(Text[i]); };
	const unsigned char Lead = Byte(Index);
	if (Lead < 0x80)
		return {Lead, 1, true};
	std::size_t Count = 0;    // the continuation bytes
	unsigned char Low = 0x80; // the second byte's range
	unsigned char High = 0xbf;
	char32_t CodePoint = 0;
	if (Lead >= 0xc2 && Lead <= 0xdf) {
		Count = 1;
		CodePoint = Lead & 0x1fU;
	} else if (Lead >= 0xe0 && Lead <= 0xef) {
		Count = 2;
		CodePoint = Lead & 0x0fU;
		if (Lead == 0xe0)
			Low = 0xa0;
		else if (Lead == 0xed)
			High = 0x9f;
	} else if (Lead >= 0xf0 && Lead <= 0xf4) {
		Count = 3;
		CodePoint = Lead & 0x07U;
		if (Lead == 0xf0)
			Low = 0x90;
		else if (Lead == 0xf4)
			High = 0x8f;
	} else {
		return {0, 1, false};
	}
	for (std::size_t i = 1; i <= Count; i++) {
		if (Index + i >= Text.size())
			return {0, i, false};
		const unsigned char Next = Byte(Index + i);
		const unsigned char Min = i == 1 ? Low : 0x80;
		const unsigned char Max = i == 1 ? High : 0xbf;
		if (Next < Min || Next > Max)
			return {0, i, false};
		CodePoint = (CodePoint << 6) | (Next & 0x3fU);
	}
	return {CodePoint, Count + 1, true};
}

template <typename Unit>
std::basic_string<Unit> ToUtf16(std::string_view Text)
{
	static_assert(sizeof(Unit) == 2);
	std::basic_string<Unit> Out;
	Out.reserve(Text.size());
	for (const char c : Text) {
		const auto Byte = static_cast<unsigned char>(c);
		Out += static_cast<Unit>(Byte >= 0x80 && Byte <= 0x9f ? HighControls[Byte - 0x80U] : char16_t{Byte});
	}
	return Out;
}

template <typename Unit>
std::string FromUtf16(std::basic_string_view<Unit> Text, char Missing);

} // namespace detail

// The code point of a byte of the client's code page.
constexpr char32_t Cp1252ToCodePoint(unsigned char Byte) noexcept
{
	return Byte >= 0x80 && Byte <= 0x9f ? char32_t{detail::HighControls[Byte - 0x80U]} : char32_t{Byte};
}

// The byte of a code point in the client's code page; -1 where it has none.
constexpr int CodePointToCp1252(char32_t CodePoint) noexcept
{
	if (CodePoint < 0x80 || (CodePoint >= 0xa0 && CodePoint <= 0xff))
		return static_cast<int>(CodePoint);
	for (std::size_t i = 0; i < detail::HighControls.size(); i++) {
		if (char32_t{detail::HighControls[i]} == CodePoint)
			return static_cast<int>(0x80 + i);
	}
	return -1;
}

// The client's text as UTF-8.
inline std::string Cp1252ToUtf8(std::string_view Text)
{
	std::string Out;
	Out.reserve(Text.size());
	for (const char c : Text)
		detail::AppendUtf8(Out, Cp1252ToCodePoint(static_cast<unsigned char>(c)));
	return Out;
}

// UTF-8 as the client's text: Missing for each character the code page does not have and for each ill-formed part.
inline std::string Utf8ToCp1252(std::string_view Text, char Missing = Replacement)
{
	std::string Out;
	Out.reserve(Text.size());
	for (std::size_t i = 0; i < Text.size();) {
		const detail::TDecoded Decoded = detail::DecodeUtf8(Text, i);
		i += Decoded.Length;
		const int Byte = Decoded.Valid ? CodePointToCp1252(Decoded.CodePoint) : -1;
		Out += Byte >= 0 ? static_cast<char>(Byte) : Missing;
	}
	return Out;
}

// The client's text as UTF-16 (Windows' wide functions: Cp1252ToWide).
inline std::u16string Cp1252ToUtf16(std::string_view Text)
{
	return detail::ToUtf16<char16_t>(Text);
}

// UTF-16 as the client's text: Missing for each character the code page does not have and for each lone surrogate.
inline std::string Utf16ToCp1252(std::u16string_view Text, char Missing = Replacement)
{
	return detail::FromUtf16<char16_t>(Text, Missing);
}

#if defined(_WIN32)
// The same with Windows' wchar_t (UTF-16 there).
inline std::wstring Cp1252ToWide(std::string_view Text)
{
	return detail::ToUtf16<wchar_t>(Text);
}

inline std::string WideToCp1252(std::wstring_view Text, char Missing = Replacement)
{
	return detail::FromUtf16<wchar_t>(Text, Missing);
}
#endif

template <typename Unit>
std::string detail::FromUtf16(std::basic_string_view<Unit> Text, char Missing)
{
	static_assert(sizeof(Unit) == 2);
	std::string Out;
	Out.reserve(Text.size());
	for (std::size_t i = 0; i < Text.size(); i++) {
		const auto Unit0 = static_cast<char16_t>(Text[i]);
		auto CodePoint = static_cast<char32_t>(Unit0);
		if (Unit0 >= 0xd800 && Unit0 <= 0xdbff && i + 1 < Text.size()) {
			const auto Unit1 = static_cast<char16_t>(Text[i + 1]);
			if (Unit1 >= 0xdc00 && Unit1 <= 0xdfff) {
				CodePoint = 0x10000 + ((static_cast<char32_t>(Unit0) - 0xd800) << 10) + (static_cast<char32_t>(Unit1) - 0xdc00);
				i++;
			}
		}
		const bool Surrogate = CodePoint >= 0xd800 && CodePoint <= 0xdfff; // a lone one
		const int Byte = Surrogate ? -1 : CodePointToCp1252(CodePoint);
		Out += Byte >= 0 ? static_cast<char>(Byte) : Missing;
	}
	return Out;
}

} // namespace patella::text
