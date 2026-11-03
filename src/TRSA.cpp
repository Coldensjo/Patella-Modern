// TRSA.cpp: reconstructed from Tibia.exe 7.72 (0x508970-0x508c64).
// 0x508970 in this range is vlong's inline operator>=, emitted here.

#include "TRSA.h"
#include "common.h"
#include "vlong.h"
#include <string>

static vlong PublicExponent(65537); // 0x71f004, initialiser at 0x55a1e0

// 0x5089a0
TRSA::TRSA() : m_ModulusM(0)
{
}

// 0x5089c0
TRSA::~TRSA() = default;

// 0x5089f0
void TRSA::SetPublicKey(const char *ModulusM)
{
	if (ModulusM == nullptr)
		throw "ModulusM pointer is NULL";
	try {
		DecimalToVlong(ModulusM, m_ModulusM);
	} catch (const char *Reason) {
		std::string Message("failed to convert decimal text to large number, reason: ");
		Message += Reason;
		throw Message.c_str(); // points into a destroyed string, as in the binary
	}
}

// 0x508aa0
void TRSA::Encrypt(uint8 *Data)
{
	if (Data == nullptr)
		throw "Data pointer is NULL";
	if (m_ModulusM == vlong(0))
		throw "m_ModulusM is null";
	if (Data[0] > 127)
		throw "first byte of data is larger than 127 which violates the condition 'plain < m'";
	vlong Plain(0);
	try {
		ByteArrayToVlong(Data, 128, Plain);
	} catch (const char *Reason) {
		std::string Message("failed to convert byte array to large number, reason: ");
		Message += Reason;
		throw Message.c_str();
	}
	if (Plain >= m_ModulusM)
		throw "Plain is not smaller than m_ModulusM which violates the condition 'plain < m'";
	vlong Cipher = modexp(Plain, PublicExponent, m_ModulusM);
	try {
		VlongToByteArray(Cipher, Data, 128);
	} catch (const char *Reason) {
		std::string Message("failed to convert large number to byte array, reason: ");
		Message += Reason;
		throw Message.c_str();
	}
}
