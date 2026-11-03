// TXTEA: the XTEA cipher used for the session (0x508c70-0x508f98). The original
// file name is unknown; "TXTEA" fits the alphabetical link order (after TRSA,
// before Utils).

#pragma once

#include "common.h"

// The destructor's empty body stays (issue #34): "= default" changes the image (the x86 .text grows by
// 0x60 bytes; on x64 the RTTI and the code move), and the build must stay the same.
class TXTEA {
public:
	TXTEA();                         // 0x508c70
	virtual ~TXTEA() {}              // NOLINT(modernize-use-equals-default): see above; scalar deleting destructor at 0x508f40 (vtable 0x573efc)
	void SetKey(const uint8 *Key);   // 0x508ca0
	void EncryptBlock(uint8 *Block); // 0x508f60, 8 bytes in place
	void DecryptBlock(uint8 *Block); // 0x508f80

private:
	uint32 Key[4]; // +0x4
};
