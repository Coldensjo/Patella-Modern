// Utils (src/Utils.cpp) and TMyException's messages: the Latin-1 helpers, the outfit colours, the
// packet buffers. The expected values are the reconstructed code's, which the oracle's difftest compares
// with Tibia.exe; here they show that the 64-bit builds and the Linux compilers give the same.

#include <doctest/doctest.h>

#include "Utils.h"
#include "compat/span.h"

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <string>

namespace {

std::string MessageOf(void (*Function)())
{
	try {
		Function();
	} catch (const TMyException &Exception) {
		return Exception.GetMessage();
	}
	return "no exception";
}

} // namespace

TEST_CASE("Utils: Latin-1 character classes, by char (signed) value")
{
	CHECK(IsAlpha('a'));
	CHECK(IsAlpha('\xe4')); // ä
	CHECK(IsAlpha('\xdf')); // ß
	// The comparison is with (char)0xe4, so the unsigned value is no letter where char is signed.
	CHECK_FALSE(IsAlpha(0xe4));
	CHECK_FALSE(IsAlpha('\xe9')); // é
	CHECK(IsSpace('\v'));
	CHECK_FALSE(IsSpace(0));
	CHECK(IsDigit('7'));
	CHECK(ToLower('A') == 'a');
	CHECK(ToLower('\xc4') == '\xe4');
	CHECK(ToLower('\xdf') == '\xdf');
	CHECK(ToUpper('\xe4') == '\xc4');
	CHECK(ToUpper('\xff') == '\xff');
	CHECK(ToUpper(0xe4) == 0xc4);
}

TEST_CASE("Utils: strings")
{
	char Text[64];
	strcpy(Text, "Hello W\xd6RLD");
	LowerString(Text);
	CHECK(std::string(Text) == "hello w\xf6rld");
	strcpy(Text, "\xe4rger im b\xfcro, x-ray");
	CapitalizeString(Text);
	CHECK(std::string(Text) == "\xc4rger Im B\xfcro, X-Ray");
	CHECK(CompareStrings("\xc4pfel", "\xe4PFEL", 10) == 0);
	CHECK(CompareStrings("abc", "abd", 3) == -1);
	CHECK(CompareStrings("abd", "abc", 2) == 0);
	CHECK(CompareStrings("b", "a", -1) == 1);
	CopyString(Text, "abcdef", 4);
	CHECK(std::string(Text) == "abc");
	TrimString(Text, " \t two words \r\n");
	CHECK(std::string(Text) == "two words");
	strcpy(Text, "a long name");
	CHECK(ShortenString(Text, 8));
	CHECK(std::string(Text) == "a lon...");
	CHECK_FALSE(ShortenString(Text, 8));
	FormatNumber(1234567, Text);
	CHECK(std::string(Text) == "1,234,567");
	FormatNumber(999, Text);
	CHECK(std::string(Text) == "999");
	CHECK(MessageOf([] { LowerString(NULL); }) == "Utils.cpp 66: assertion failed, reason:\ns!=NULL");
	CHECK(MessageOf([] {
		char Buffer[16];
		FormatNumber(-1, Buffer);
	}) == "Utils.cpp 468: assertion failed (Number = -1), reason:\nNumber>=0");
}

TEST_CASE("Utils: outfit colours")
{
	int R, G, B;
	GetOutfitColor(0, &R, &G, &B);
	CHECK((R == 255 && G == 255 && B == 255));
	GetOutfitColor(1, &R, &G, &B);
	CHECK((R == 255 && G == 212 && B == 191));
	GetOutfitColor(57, &R, &G, &B);
	CHECK((R == 145 && G == 145 && B == 145));
	GetOutfitColor(132, &R, &G, &B);
	CHECK((R == 127 && G == 0 && B == 0));

	// FNV-1a over R, G, B of colours 0..133 (133 is colour 0), as tests/unit/core_expected.py.
	unsigned long long Digest = 0xcbf29ce484222325ull;
	for (int Color = 0; Color <= 133; Color++) {
		GetOutfitColor(Color, &R, &G, &B);
		for (int Value : {R, G, B})
			Digest = (Digest ^ (uint8)Value) * 0x100000001b3ull;
	}
	CHECK(Digest == 0x993f0e92d2b0c345ull);
}

TEST_CASE("Utils: packet buffers, little-endian")
{
	uint8 Data[32];
	TWriteBuffer Out(Data);
	Out.PutByte(0xab);
	Out.PutWord(0x1234);
	Out.PutQuad(0xdeadbeef);
	Out.PutString("Tibia");
	const uint8 Bytes[] = {1, 2, 3};
	Out.PutBytes(Bytes);
	CHECK(Out.Position == 1 + 2 + 4 + 2 + 5 + 3);
	const uint8 Expected[] = {0xab, 0x34, 0x12, 0xef, 0xbe, 0xad, 0xde, 5, 0, 'T', 'i', 'b', 'i', 'a', 1, 2, 3};
	CHECK(memcmp(Data, Expected, sizeof(Expected)) == 0);

	TReadBuffer In(patella::TSpan<uint8>(Data).first(static_cast<std::size_t>(Out.Position)));
	CHECK(In.GetByte() == 0xab);
	CHECK(In.GetWord() == 0x1234);
	CHECK(In.GetQuad() == 0xdeadbeef);
	char Name[4];
	In.GetString(Name);
	CHECK(std::string(Name) == "Tib");
	uint8 Rest[3];
	In.GetBytes(Rest);
	CHECK((Rest[0] == 1 && Rest[2] == 3));

	In.Position = 7;
	char *Copy = In.GetString();
	CHECK(std::string(Copy) == "Tibia");
	delete[] Copy;

	try {
		In.GetQuad();
		FAIL("no exception");
	} catch (const TMyException &Exception) {
		CHECK(Exception.GetMessage()
			== "Utils.cpp 321: assertion failed (Position = 14) (Size = 17), reason:\nPosition<=Size-4");
		CHECK(Exception.GetOrigin() == "Utils.cpp 321");
	}
}

TEST_CASE("Utils: packet buffers, the original's pointer and length as spans")
{
	// Issue #59: the pointer and the size as an int are the original's parameters, which the assertions name. A null
	// pointer still fails them; a size above INT_MAX reads as the original's negative size or MaxLength. Only tests
	// make such spans; the size is never used to read.
	CHECK(MessageOf([] { static_cast<void>(TReadBuffer(patella::TSpan<uint8>())); })
		== "Utils.cpp 286: assertion failed, reason:\ndata!=NULL");
	CHECK(MessageOf([] { static_cast<void>(TWriteBuffer(patella::TSpan<uint8>())); })
		== "Utils.cpp 387: assertion failed, reason:\ndata!=NULL");
	CHECK(MessageOf([] {
		static uint8 Data[4];
		static_cast<void>(TReadBuffer(patella::TSpan<uint8>(Data, static_cast<std::size_t>(-1))));
	}) == "Utils.cpp 287: assertion failed, reason:\nsize>=0");
	CHECK(MessageOf([] {
		static uint8 Data[4];
		static_cast<void>(TWriteBuffer(patella::TSpan<uint8>(Data, static_cast<std::size_t>(-7))));
	}) == "Utils.cpp 388: assertion failed, reason:\nsize>=0");
	CHECK(MessageOf([] {
		static uint8 Data[4];
		TReadBuffer In(Data);
		In.GetBytes(patella::TSpan<uint8>());
	}) == "Utils.cpp 371: assertion failed, reason:\nBuffer!=NULL");
	CHECK(MessageOf([] {
		static uint8 Data[4];
		TWriteBuffer Out(Data);
		Out.PutBytes(patella::TSpan<const uint8>());
	}) == "Utils.cpp 451: assertion failed, reason:\nBuffer!=NULL");
	CHECK(MessageOf([] {
		static uint8 Data[4] = {1, 0, 'x', 0};
		TReadBuffer In(Data);
		In.GetString(patella::TSpan<char>());
	}) == "Utils.cpp 334: assertion failed, reason:\nBuffer!=NULL");
	CHECK(MessageOf([] {
		static uint8 Data[4];
		TWriteBuffer Out(Data);
		const uint8 Bytes[5] = {};
		Out.PutBytes(Bytes);
	}) == "Utils.cpp 452: assertion failed (Position = 0) (Size-Count = -1), reason:\nPosition<=Size-Count");

	// MaxLength: the span's size cuts the text (its last byte is the NUL); a size above INT_MAX is no limit.
	uint8 Data[16];
	TWriteBuffer Out(Data);
	Out.PutString("Tibia");
	Out.PutString("Tibia");
	TReadBuffer In(patella::TSpan<uint8>(Data).first(static_cast<std::size_t>(Out.Position)));
	char Text[8];
	memset(Text, 'x', sizeof(Text));
	In.GetString(patella::TSpan<char>(Text).first(3));
	CHECK(std::string(Text) == "Ti");
	CHECK(Text[3] == 'x');
	memset(Text, 'x', sizeof(Text));
	In.GetString(patella::TSpan<char>(Text, static_cast<std::size_t>(-1)));
	CHECK(std::string(Text) == "Tibia");
	CHECK(In.Position == In.Size);
}
