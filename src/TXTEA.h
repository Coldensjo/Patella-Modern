// TXTEA: the XTEA cipher used for the session (0x508c70-0x508f98). The original
// file name is unknown; "TXTEA" fits the alphabetical link order (after TRSA,
// before Utils).

#pragma once

#include "common.h"
#include "compat/span.h"

// The destructor's empty body stays (issue #34): "= default" changes the image (the x86 .text grows by
// 0x60 bytes; on x64 the RTTI and the code move), and the build must stay the same.
class TXTEA {
public:
	// The key's 16 bytes and a block's 8 (issue #59: the original took pointers). A span whose pointer is null is
	// ignored, as the original's null pointer was.
	using TKey = patella::TSpan<const uint8, 16>;
	using TBlock = patella::TSpan<uint8, 8>;

	TXTEA();                         // 0x508c70
	virtual ~TXTEA() {}              // NOLINT(modernize-use-equals-default): see above; scalar deleting destructor at 0x508f40 (vtable 0x573efc)
	void SetKey(TKey Key);           // 0x508ca0
	void EncryptBlock(TBlock Block); // 0x508f60, in place
	void DecryptBlock(TBlock Block); // 0x508f80

private:
	uint32 Key[4]; // +0x4
};
