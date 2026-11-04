// TXTEA (src/TXTEA.cpp): XTEA with 32 cycles on two little-endian words; the expected blocks are
// tests/unit/core_expected.py's (the first is XTEA's all-zero vector, dee9d4d8 f7131ed9 as words).

#include <doctest/doctest.h>

#include "TXTEA.h"
#include "compat/span.h"

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
	// The original's null pointers: spans with a null pointer (issue #59).
	TXTEA Xtea;
	alignas(4) uint8 Block[8] = {};
	Xtea.SetKey(TXTEA::TKey(static_cast<const uint8 *>(nullptr), 16));
	Xtea.EncryptBlock(Block);
	uint8 Cipher[8];
	FromHex("d8d4e9ded91e13f7", Cipher);
	CHECK(memcmp(Block, Cipher, 8) == 0);
	Xtea.EncryptBlock(TXTEA::TBlock(static_cast<uint8 *>(nullptr), 8));
	Xtea.DecryptBlock(TXTEA::TBlock(static_cast<uint8 *>(nullptr), 8));
	CHECK(memcmp(Block, Cipher, 8) == 0);
}

TEST_CASE("TXTEA: a block inside a larger buffer, as the packets are encrypted")
{
	// Network.cpp encrypts a packet's 8-byte blocks in place; the bytes around a block stay.
	alignas(4) uint8 Key[16];
	FromHex("000102030405060708090a0b0c0d0e0f", Key);
	alignas(4) uint8 Packet[24];
	memset(Packet, 0x5a, sizeof(Packet));
	FromHex("4142434445464748", &Packet[8]);
	TXTEA Xtea;
	Xtea.SetKey(Key);
	Xtea.EncryptBlock(TXTEA::TBlock(&Packet[8], 8));
	uint8 Cipher[8];
	FromHex("cae7697e006ee921", Cipher);
	CHECK(memcmp(&Packet[8], Cipher, 8) == 0);
	CHECK((Packet[7] == 0x5a && Packet[16] == 0x5a));
	Xtea.DecryptBlock(patella::TSpan<uint8>(Packet).subspan(8).first<8>());
	uint8 Plain[8];
	FromHex("4142434445464748", Plain);
	CHECK(memcmp(&Packet[8], Plain, 8) == 0);
}
