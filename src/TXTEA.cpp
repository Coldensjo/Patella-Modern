// TXTEA.cpp: reconstructed from Tibia.exe 7.72 (0x508c70-0x508f98).
// Standard XTEA with 32 cycles; the compiler unrolled the loop four cycles per
// iteration. Blocks are two little-endian 32-bit words.

#include "TXTEA.h"
#include "common.h"
#include <array>
#include <cstring>

namespace {

const uint32 DELTA = 0x9e3779b9;

// 0x508cd0
void EncryptXTEA(uint32 *Block, const uint32 *Key)
{
	uint32 v0 = Block[0];
	uint32 v1 = Block[1];
	uint32 Sum = 0;
	for (int i = 0; i < 32; i++) {
		v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (Sum + Key[Sum & 3]);
		Sum += DELTA;
		v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (Sum + Key[(Sum >> 11) & 3]);
	}
	Block[0] = v0;
	Block[1] = v1;
}

// 0x508e00
void DecryptXTEA(uint32 *Block, const uint32 *Key)
{
	uint32 v0 = Block[0];
	uint32 v1 = Block[1];
	uint32 Sum = DELTA * 32;
	for (int i = 0; i < 32; i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (Sum + Key[(Sum >> 11) & 3]);
		Sum -= DELTA;
		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (Sum + Key[Sum & 3]);
	}
	Block[0] = v0;
	Block[1] = v1;
}

} // namespace


// 0x508c70
TXTEA::TXTEA()
{
	Key[0] = 0;
	Key[1] = 0;
	Key[2] = 0;
	Key[3] = 0;
}

// 0x508ca0
void TXTEA::SetKey(TKey Key)
{
	if (Key.data() != nullptr) {
		this->Key[0] = (reinterpret_cast<const uint32 *>(Key.data()))[0];
		this->Key[1] = (reinterpret_cast<const uint32 *>(Key.data()))[1];
		this->Key[2] = (reinterpret_cast<const uint32 *>(Key.data()))[2];
		this->Key[3] = (reinterpret_cast<const uint32 *>(Key.data()))[3];
	}
}

// 0x508f60
void TXTEA::EncryptBlock(TBlock Block)
{
	if (Block.data() != nullptr) {
		// The block's two words through an aligned copy (Patella-Modern issue #91): a packet's blocks start at any
		// byte (the length before them), and a uint32 read there is undefined (UBSan: misaligned); x86 reads it all
		// the same, as the original did. The same bytes, little-endian.
		std::array<uint32, 2> Words{};
		std::memcpy(Words.data(), Block.data(), sizeof(Words));
		EncryptXTEA(Words.data(), Key.data());
		std::memcpy(Block.data(), Words.data(), sizeof(Words));
	}
}

// 0x508f80
void TXTEA::DecryptBlock(TBlock Block)
{
	if (Block.data() != nullptr) {
		// The block's two words through an aligned copy (Patella-Modern issue #91): a packet's blocks start at any
		// byte (the length before them), and a uint32 read there is undefined (UBSan: misaligned); x86 reads it all
		// the same, as the original did. The same bytes, little-endian.
		std::array<uint32, 2> Words{};
		std::memcpy(Words.data(), Block.data(), sizeof(Words));
		DecryptXTEA(Words.data(), Key.data());
		std::memcpy(Block.data(), Words.data(), sizeof(Words));
	}
}
