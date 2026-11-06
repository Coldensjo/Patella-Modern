// vlong and TRSA (src/vlong.cpp, src/TRSA.cpp). The cases of the oracle's vlongtest (the same generator
// and order; until issue #94), which it checked against Python's exact integers, digested
// here: tests/unit/core_expected.py computes the expected digest the same way.

#include <doctest/doctest.h>

#include "TRSA.h"
#include "compat/span.h"

#include <cstring>
#include <string>
#include <string_view>

namespace {

const char MODULUS[] =
	"124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284902061793";

// FNV-1a, 64 bits.
struct TDigest {
	unsigned long long Value = 0xcbf29ce484222325ull;
	void Update(const uint8 *Data, int Size)
	{
		for (int i = 0; i < Size; i++)
			Value = (Value ^ Data[i]) * 0x100000001b3ull;
	}
};

// vlongtest.cpp's generator.
struct TRandom {
	unsigned Seed = 4242;
	unsigned operator()()
	{
		Seed = Seed * 1103515245 + 12345;
		return Seed >> 8;
	}
};

void RandomBytes(TRandom &Random, uint8 *Data, int Size, int Used)
{
	memset(Data, 0, (size_t)Size);
	for (int i = Size - Used; i < Size; i++)
		Data[i] = (uint8)Random();
}

void FromHex(std::string_view Hex, uint8 *Data)
{
	auto Nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
	for (size_t i = 0; i < Hex.size() / 2; i++)
		Data[i] = (uint8)(Nibble(Hex[2 * i]) * 16 + Nibble(Hex[2 * i + 1]));
}

std::string Message(const char *Text)
{
	return Text;
}

} // namespace

TEST_CASE("vlong and TRSA: vlongtest's cases against Python's exact integers")
{
	TRandom Random;
	TDigest Digest;
	int Cases = 0;

	TRSA Rsa;
	Rsa.SetPublicKey(MODULUS);
	for (int t = 0; t < 200; t++) {
		uint8 Block[128];
		for (int i = 0; i < 128; i++)
			Block[i] = (uint8)Random();
		Block[0] &= 0x7f;
		if (t == 0)
			memset(Block, 0, sizeof(Block));
		Rsa.Encrypt(Block);
		Digest.Update(Block, 128);
		Cases++;
	}

	const char Ops[] = "+-*/%";
	for (int t = 0; t < 3000; t++) {
		uint8 A[64], B[64], R[128];
		RandomBytes(Random, A, 64, (int)(1 + Random() % 64));
		RandomBytes(Random, B, 64, (int)(1 + Random() % 64));
		char Op = Ops[t % 5];
		vlong x, y, r;
		ByteArrayToVlong(A, x);
		ByteArrayToVlong(B, y);
		if (Op == '-' && x < y) {
			vlong Swap = x;
			x = y;
			y = Swap;
		}
		if ((Op == '/' || Op == '%') && y == vlong(0))
			continue;
		switch (Op) {
		case '+': r = x + y; break;
		case '-': r = x - y; break;
		case '*': r = x * y; break;
		case '/': r = x / y; break;
		case '%': r = x % y; break;
		}
		VlongToByteArray(r, R);
		Digest.Update(R, 128);
		Cases++;
	}

	vlong Decimal;
	DecimalToVlong("123456789012345678901234567890", Decimal);
	uint8 D[16];
	VlongToByteArray(Decimal, D);
	Digest.Update(D, 16);
	Cases++;

	CHECK(Cases == 3201);
	CHECK(Digest.Value == 0x4850195d8e8c8047ull);
}

TEST_CASE("vlong: conversions and small numbers")
{
	vlong Decimal;
	DecimalToVlong("123456789012345678901234567890", Decimal);
	uint8 D[16], Expected[16];
	VlongToByteArray(Decimal, D);
	FromHex("000000018ee90ff6c373e0ee4e3f0ad2", Expected);
	CHECK(memcmp(D, Expected, 16) == 0);

	vlong Ten = vlong(3) * 3L + 1L;
	CHECK(Ten == vlong(10));
	CHECK(Ten < 11L);
	CHECK_FALSE(Ten < 10L);
	CHECK(vlong(7) - vlong(9) < 0L);
	CHECK(modinv(vlong(3), vlong(7)) == vlong(5));
	CHECK(modexp(vlong(4), vlong(13), vlong(497)) == vlong(445));

	// Non-digits count as 0 (atoi), as in the binary.
	DecimalToVlong("1x2", Decimal);
	CHECK(Decimal == vlong(102));
	CHECK_THROWS_AS(DecimalToVlong(NULL, Decimal), const char *);
}

TEST_CASE("vlong: the byte arrays as spans (issue #60)")
{
	// The original's null pointer: a span without data throws its message, whatever the size.
	vlong Number(258);
	const char *Reason = nullptr;
	try {
		ByteArrayToVlong(patella::TSpan<const uint8>(static_cast<const uint8 *>(nullptr), 16), Number);
	} catch (const char *Thrown) {
		Reason = Thrown;
	}
	REQUIRE(Reason != nullptr);
	CHECK(std::string(Reason) == "pData pointer is NULL");
	Reason = nullptr;
	try {
		VlongToByteArray(Number, patella::TSpan<uint8>(static_cast<uint8 *>(nullptr), 16));
	} catch (const char *Thrown) {
		Reason = Thrown;
	}
	REQUIRE(Reason != nullptr);
	CHECK(std::string(Reason) == "pData pointer is NULL");

	// A part of a buffer: the bytes around it are untouched, and it reads back.
	uint8 Buffer[8] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
	VlongToByteArray(Number, patella::TSpan<uint8>(Buffer).subspan(2, 4));
	const uint8 Expected[8] = {0xaa, 0xaa, 0x00, 0x00, 0x01, 0x02, 0xaa, 0xaa};
	CHECK(memcmp(Buffer, Expected, 8) == 0);
	vlong Back;
	ByteArrayToVlong(patella::TSpan<const uint8>(Buffer).subspan(2, 4), Back);
	CHECK(Back == vlong(258));

	// An empty span: nothing written, the number 0.
	VlongToByteArray(Number, patella::TSpan<uint8>(Buffer).first(0));
	CHECK(memcmp(Buffer, Expected, 8) == 0);
	ByteArrayToVlong(patella::TSpan<const uint8>(Buffer).first(0), Back);
	CHECK(Back == vlong(0));
}

TEST_CASE("TRSA: encryption with exponent 65537")
{
	TRSA Rsa;
	uint8 Block[128];
	memset(Block, 0, sizeof(Block));
	try {
		Rsa.Encrypt(Block);
		FAIL("no exception");
	} catch (const char *Reason) {
		CHECK(Message(Reason) == "m_ModulusM is null");
	}

	Rsa.SetPublicKey(MODULUS);
	Block[127] = 1;
	Rsa.Encrypt(Block);
	uint8 One[128] = {};
	One[127] = 1;
	CHECK(memcmp(Block, One, 128) == 0);

	memset(Block, 0, sizeof(Block));
	Block[127] = 2;
	Rsa.Encrypt(Block);
	uint8 Expected[128];
	FromHex("402f9ed90e11c62157584101f5068526c48961afb61389e513265470017985c2a122ca2a0cb67200969ba2c6b7df913c"
			"8d1dbbbe4f3374f1d82573927a7506c793568f1c33d6e91ebb01b52296f5704d96aad33acac4c84a4a370b03211e2bf2"
			"ce58efc51a88bb7847836a01387aec2040ab8f4ec5b5135625a44642d91d48c0",
		Expected);
	CHECK(memcmp(Block, Expected, 128) == 0);

	try {
		Rsa.Encrypt(patella::TSpan<uint8, 128>(static_cast<uint8 *>(nullptr), 128)); // the original's NULL (issue #59)
		FAIL("no exception");
	} catch (const char *Reason) {
		CHECK(Message(Reason) == "Data pointer is NULL");
	}
	Block[0] = 0x80;
	try {
		Rsa.Encrypt(Block);
		FAIL("no exception");
	} catch (const char *Reason) {
		CHECK(Message(Reason) == "first byte of data is larger than 127 which violates the condition 'plain < m'");
	}
	try {
		Rsa.SetPublicKey(NULL);
		FAIL("no exception");
	} catch (const char *Reason) {
		CHECK(Message(Reason) == "ModulusM pointer is NULL");
	}
}
