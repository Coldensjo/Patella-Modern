// TXTEA.cpp: reconstructed from Tibia.exe 7.72 (0x508c70-0x508f98).
// Standard XTEA with 32 cycles; the compiler unrolled the loop four cycles per
// iteration. Blocks are two little-endian 32-bit words.

#include "TXTEA.h"
#include "common.h"

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
void TXTEA::SetKey(const uint8 *Key)
{
	if (Key != nullptr) {
		this->Key[0] = (reinterpret_cast<const uint32 *>(Key))[0];
		this->Key[1] = (reinterpret_cast<const uint32 *>(Key))[1];
		this->Key[2] = (reinterpret_cast<const uint32 *>(Key))[2];
		this->Key[3] = (reinterpret_cast<const uint32 *>(Key))[3];
	}
}

// 0x508f60
void TXTEA::EncryptBlock(uint8 *Block)
{
	if (Block != nullptr)
		EncryptXTEA(reinterpret_cast<uint32 *>(Block), Key);
}

// 0x508f80
void TXTEA::DecryptBlock(uint8 *Block)
{
	if (Block != nullptr)
		DecryptXTEA(reinterpret_cast<uint32 *>(Block), Key);
}
