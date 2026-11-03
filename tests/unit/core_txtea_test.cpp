// TXTEA (src/TXTEA.cpp): XTEA with 32 cycles on two little-endian words; the expected blocks are
// tests/unit/core_expected.py's (the first is XTEA's all-zero vector, dee9d4d8 f7131ed9 as words).

#include <doctest/doctest.h>

#include "TXTEA.h"

#include <cstring>
#include <string_view>

namespace {

struct TVector {
	const char *Key;
	const char *Plain;
	const char *Cipher;
};

const TVector Vectors[] = {
	{"00000000000000000000000000000000", "0000000000000000", "d8d4e9ded91e13f7"},
	{"000102030405060708090a0b0c0d0e0f", "4142434445464748", "cae7697e006ee921"},
	{"2bd6459f82c5b300952c49104881ff48", "ea024714ad5c4d84", "a8f5be73a0f724f5"},
};

void FromHex(std::string_view Hex, uint8 *Data)
{
	auto Nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
	for (size_t i = 0; i < Hex.size() / 2; i++)
		Data[i] = (uint8)(Nibble(Hex[2 * i]) * 16 + Nibble(Hex[2 * i + 1]));
}

} // namespace

TEST_CASE("TXTEA: encryption and decryption of a block")
{
	for (const TVector &Vector : Vectors) {
		CAPTURE(Vector.Key);
		// SetKey and the blocks read 32-bit words through the byte pointers.
		alignas(4) uint8 Key[16];
		alignas(4) uint8 Block[8];
		uint8 Plain[8], Cipher[8];
		FromHex(Vector.Key, Key);
		FromHex(Vector.Plain, Plain);
		FromHex(Vector.Cipher, Cipher);

		TXTEA Xtea;
		Xtea.SetKey(Key);
		memcpy(Block, Plain, 8);
		Xtea.EncryptBlock(Block);
		CHECK(memcmp(Block, Cipher, 8) == 0);
		Xtea.DecryptBlock(Block);
		CHECK(memcmp(Block, Plain, 8) == 0);
	}
}

TEST_CASE("TXTEA: no key is the zero key; NULL does nothing")
{
	TXTEA Xtea;
	alignas(4) uint8 Block[8] = {};
	Xtea.SetKey(NULL);
	Xtea.EncryptBlock(Block);
	uint8 Cipher[8];
	FromHex("d8d4e9ded91e13f7", Cipher);
	CHECK(memcmp(Block, Cipher, 8) == 0);
	Xtea.EncryptBlock(NULL);
	Xtea.DecryptBlock(NULL);
}
