// TXTEA: the XTEA cipher used for the session (0x508c70-0x508f98). The original
// file name is unknown; "TXTEA" fits the alphabetical link order (after TRSA,
// before Utils).

#pragma once

#include "common.h"
#include "compat/span.h"
#include <array>

// The destructor is "= default" since issue #47: issue #34 kept the empty body because "= default" changed the
// oracle's image (the x86 .text grew by 0x60 bytes); the oracle left with issue #94. It stays virtual (the vtable
// pointer first, as in the original).
class TXTEA {
public:
	// The key's 16 bytes and a block's 8 (issue #59: the original took pointers). A span whose pointer is null is
	// ignored, as the original's null pointer was.
	using TKey = patella::TSpan<const uint8, 16>;
	using TBlock = patella::TSpan<uint8, 8>;

	TXTEA();                         // 0x508c70
	virtual ~TXTEA() = default;      // scalar deleting destructor at 0x508f40 (vtable 0x573efc)
	void SetKey(TKey Key);           // 0x508ca0
	void EncryptBlock(TBlock Block); // 0x508f60, in place
	void DecryptBlock(TBlock Block); // 0x508f80

private:
	std::array<uint32, 4> Key; // +0x4
};
