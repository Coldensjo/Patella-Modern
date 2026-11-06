// The client's code page and Unicode, src/types/Cp1252.h (issue #112, plan D4): every byte's character, the round
// trips through UTF-8 and UTF-16, the characters the code page does not have and ill-formed UTF-8 and UTF-16 ('?',
// one per character or maximal subpart), every code point against the table, and on Windows Windows' own code page
// 1252 (MultiByteToWideChar, and WideCharToMultiByte without best fit) as the reference.

#include <doctest/doctest.h>

#include "types/Cp1252.h"

#include <cstddef>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using patella::text::CodePointToCp1252;
using patella::text::Cp1252ToCodePoint;
using patella::text::Cp1252ToUtf16;
using patella::text::Cp1252ToUtf8;
using patella::text::Utf16ToCp1252;
using patella::text::Utf8ToCp1252;

namespace {

// Every byte, 0 to 255, in order.
std::string AllBytes()
{
	std::string Bytes;
	for (int i = 0; i < 256; i++)
		Bytes += static_cast<char>(i);
	return Bytes;
}

// A code point's UTF-8, written out by hand (not the header's).
std::string Utf8Of(char32_t CodePoint)
{
	std::string Out;
	const auto Unit = [](char32_t Value) { return static_cast<char>(static_cast<unsigned char>(Value)); };
	if (CodePoint < 0x80) {
		Out += Unit(CodePoint);
	} else if (CodePoint < 0x800) {
		Out += Unit(0xc0 + (CodePoint / 64));
		Out += Unit(0x80 + (CodePoint % 64));
	} else if (CodePoint < 0x10000) {
		Out += Unit(0xe0 + (CodePoint / 4096));
		Out += Unit(0x80 + ((CodePoint / 64) % 64));
		Out += Unit(0x80 + (CodePoint % 64));
	} else {
		Out += Unit(0xf0 + (CodePoint / 262144));
		Out += Unit(0x80 + ((CodePoint / 4096) % 64));
		Out += Unit(0x80 + ((CodePoint / 64) % 64));
		Out += Unit(0x80 + (CodePoint % 64));
	}
	return Out;
}

} // namespace

TEST_CASE("cp1252: the characters of the bytes")
{
	// ASCII and Latin-1: the byte's value.
	for (int i = 0; i < 0x80; i++)
		CHECK(Cp1252ToCodePoint(static_cast<unsigned char>(i)) == static_cast<char32_t>(i));
	for (int i = 0xa0; i < 0x100; i++)
		CHECK(Cp1252ToCodePoint(static_cast<unsigned char>(i)) == static_cast<char32_t>(i));
	// 0x80-0x9f: the printable characters, and the five undefined bytes as the C1 controls of their value.
	CHECK(Cp1252ToCodePoint(0x80) == U'\x20ac');
	CHECK(Cp1252ToCodePoint(0x8a) == U'\x160');
	CHECK(Cp1252ToCodePoint(0x99) == U'\x2122');
	CHECK(Cp1252ToCodePoint(0x9f) == U'\x178');
	for (const int Undefined : {0x81, 0x8d, 0x8f, 0x90, 0x9d})
		CHECK(Cp1252ToCodePoint(static_cast<unsigned char>(Undefined)) == static_cast<char32_t>(Undefined));
	// Every byte has its own character, and the character its byte.
	for (int i = 0; i < 256; i++)
		CHECK(CodePointToCp1252(Cp1252ToCodePoint(static_cast<unsigned char>(i))) == i);
	// The C1 controls Windows-1252 puts printable characters in have no byte.
	for (const char32_t Control : {U'\u0080', U'\u0082', U'\u0099', U'\u009f'})
		CHECK(CodePointToCp1252(Control) == -1);
	CHECK(CodePointToCp1252(U'\x100') == -1);
	CHECK(CodePointToCp1252(U'\xffff') == -1);
	CHECK(CodePointToCp1252(U'\U0010ffff') == -1);
}

TEST_CASE("cp1252: UTF-8")
{
	CHECK(Cp1252ToUtf8("") == "");
	CHECK(Cp1252ToUtf8("Tibia 7.72\r\n") == "Tibia 7.72\r\n");
	CHECK(Cp1252ToUtf8("J\xf6rg") == "J\xc3\xb6rg");
	CHECK(Cp1252ToUtf8("\x80 \xff \x81") == "\xe2\x82\xac \xc3\xbf \xc2\x81");
	CHECK(Cp1252ToUtf8(std::string_view("a\0b", 3)) == std::string("a\0b", 3)); // NUL is a character
	// Every byte there and back.
	const std::string Bytes = AllBytes();
	CHECK(Utf8ToCp1252(Cp1252ToUtf8(Bytes)) == Bytes);
	for (int i = 0; i < 256; i++)
		CHECK(Cp1252ToUtf8(std::string(1, static_cast<char>(i))) == Utf8Of(Cp1252ToCodePoint(static_cast<unsigned char>(i))));

	// Characters the code page does not have: one Missing each, whatever their length.
	CHECK(Utf8ToCp1252("\xc4\x80") == "?"); // U+0100
	CHECK(Utf8ToCp1252("a\xe2\x88\x9e"
					   "b")
		== "a?b");                                              // U+221E
	CHECK(Utf8ToCp1252("\xf0\x9f\x98\x80\xc3\xa4") == "?\xe4"); // U+1F600, U+00E4
	CHECK(Utf8ToCp1252("\xc2\x80\xc2\x81") == "?\x81");         // U+0080 (0x80 is the euro), U+0081
	CHECK(Utf8ToCp1252("\xc4\x80", '_') == "_");                // another Missing
	// Ill-formed UTF-8: one Missing per maximal subpart (Unicode 15, section 3.9, table 3-8).
	CHECK(Utf8ToCp1252("\xc3") == "?");     // cut off
	CHECK(Utf8ToCp1252("\xe2\x82") == "?"); // cut off after two of three
	CHECK(Utf8ToCp1252("\xe2\x82"
					   "a")
		== "?a");
	CHECK(Utf8ToCp1252("\x80\xbf") == "??");                                                     // continuation bytes alone
	CHECK(Utf8ToCp1252("\xc0\xaf") == "??");                                                     // overlong
	CHECK(Utf8ToCp1252("\xe0\x80\xaf") == "???");                                                // overlong, three bytes
	CHECK(Utf8ToCp1252("\xed\xa0\x80") == "???");                                                // a surrogate
	CHECK(Utf8ToCp1252("\xf4\x90\x80\x80") == "????");                                           // past U+10FFFF
	CHECK(Utf8ToCp1252("\xf5\xff\xfe") == "???");                                                // bytes UTF-8 never has
	CHECK(Utf8ToCp1252("\x61\xf1\x80\x80\xe1\x80\xc2\x62\x80\x63\x80\xbf\x64") == "a???b?c??d"); // table 3-8
}

TEST_CASE("cp1252: every code point")
{
	for (char32_t CodePoint = 0; CodePoint <= 0x10ffff; CodePoint++) {
		if (CodePoint >= 0xd800 && CodePoint <= 0xdfff)
			continue;
		const int Byte = CodePointToCp1252(CodePoint);
		const std::string Expected = Byte >= 0 ? std::string(1, static_cast<char>(Byte)) : std::string("?");
		const std::string Converted = Utf8ToCp1252(Utf8Of(CodePoint));
		if (Converted != Expected) { // one CHECK per failure, not 1.1 million passes
			CHECK(Converted == Expected);
			break;
		}
		if (Byte >= 0 && Cp1252ToCodePoint(static_cast<unsigned char>(Byte)) != CodePoint) {
			CHECK(Cp1252ToCodePoint(static_cast<unsigned char>(Byte)) == CodePoint);
			break;
		}
	}
}

TEST_CASE("cp1252: UTF-16")
{
	CHECK(Cp1252ToUtf16("J\xf6rg \x80") == u"J\xf6"
										   u"rg \x20ac");
	const std::string Bytes = AllBytes();
	const std::u16string Wide = Cp1252ToUtf16(Bytes);
	REQUIRE(Wide.size() == 256);
	for (std::size_t i = 0; i < Wide.size(); i++)
		CHECK(static_cast<char32_t>(Wide[i]) == Cp1252ToCodePoint(static_cast<unsigned char>(i)));
	CHECK(Utf16ToCp1252(Wide) == Bytes);
	CHECK(Utf16ToCp1252(u"\x100"
						u"\x221e")
		== "??");
	CHECK(Utf16ToCp1252(u"a\U0001f600b") == "a?b");                           // a surrogate pair is one character
	CHECK(Utf16ToCp1252(std::u16string(1, char16_t{0xd83d}) + u"a") == "?a"); // a lone high surrogate
	CHECK(Utf16ToCp1252(u"a" + std::u16string(1, char16_t{0xde00})) == "a?"); // a lone low surrogate
	CHECK(Utf16ToCp1252(std::u16string(1, char16_t{0xd83d})) == "?");         // cut off
	CHECK(Utf16ToCp1252(u"\x100", '_') == "_");
#if defined(_WIN32)
	CHECK(patella::text::Cp1252ToWide("J\xf6rg") == L"J\xf6"
													L"rg");
	CHECK(patella::text::WideToCp1252(L"J\xf6"
									  L"rg \x20ac"
									  L" \x100")
		== "J\xf6rg \x80 ?");
#endif
}

#if defined(_WIN32)
TEST_CASE("cp1252: Windows' code page 1252 is the same")
{
	// The bytes: MultiByteToWideChar's characters, the five undefined bytes among them.
	const std::string Bytes = AllBytes();
	std::wstring Windows(256, L'\0');
	REQUIRE(MultiByteToWideChar(1252, 0, Bytes.data(), 256, Windows.data(), 256) == 256);
	CHECK(patella::text::Cp1252ToWide(Bytes) == Windows);
	// The characters back: WideCharToMultiByte without best fit, '?' for the ones the code page does not have.
	for (char32_t CodePoint = 0; CodePoint < 0x10000; CodePoint++) {
		if (CodePoint >= 0xd800 && CodePoint <= 0xdfff)
			continue;
		const wchar_t Unit = static_cast<wchar_t>(CodePoint);
		char Byte = 0;
		const char Default = '?';
		REQUIRE(WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, &Unit, 1, &Byte, 1, &Default, nullptr) == 1);
		const std::string Ours = patella::text::WideToCp1252(std::wstring_view(&Unit, 1));
		if (Ours != std::string(1, Byte)) {
			CHECK(Ours == std::string(1, Byte));
			break;
		}
	}
}
#endif
