// TRSA.cpp: reconstructed from Tibia.exe 7.72 (0x508970-0x508c64).
// 0x508970 in this range is vlong's inline operator>=, emitted here.

#include "TRSA.h"
#include "common.h"
#include "compat/span.h"
#include "vlong.h"
#include <string>

namespace {

// NOLINTNEXTLINE(bugprone-throwing-static-initialization): the binary's global (vlong allocates; issue #63)
vlong PublicExponent(65537); // 0x71f004, initialiser at 0x55a1e0

} // namespace

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

// 0x508aa0. Textbook RSA: the 128 bytes are the number, encrypted as they are (no padding, by the protocol;
// notes/decisions.md, "#128").
void TRSA::Encrypt(patella::TSpan<uint8, 128> Data)
{
	if (Data.data() == nullptr)
		throw "Data pointer is NULL";
	if (m_ModulusM == vlong(0))
		throw "m_ModulusM is null";
	if (Data[0] > 127)
		throw "first byte of data is larger than 127 which violates the condition 'plain < m'";
	vlong Plain(0);
	try {
		ByteArrayToVlong(Data, Plain);
	} catch (const char *Reason) {
		std::string Message("failed to convert byte array to large number, reason: ");
		Message += Reason;
		throw Message.c_str();
	}
	if (Plain >= m_ModulusM)
		throw "Plain is not smaller than m_ModulusM which violates the condition 'plain < m'";
	const vlong Cipher = modexp(Plain, PublicExponent, m_ModulusM);
	try {
		VlongToByteArray(Cipher, Data);
	} catch (const char *Reason) {
		std::string Message("failed to convert large number to byte array, reason: ");
		Message += Reason;
		throw Message.c_str();
	}
}
