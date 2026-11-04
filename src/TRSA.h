// TRSA: RSA encryption with a public key, used for the login packet
// (0x508970-0x508c64). The original file name is unknown; "TRSA" fits the
// alphabetical link order (after TMyException, before TXTEA). Errors are
// thrown as const char*.

#pragma once

#include "vlong.h"
#include "common.h"
#include "compat/span.h"

class TRSA {
public:
	TRSA();          // 0x5089a0
	virtual ~TRSA(); // 0x5089c0; scalar deleting destructor 0x5089d0
	// 0x5089f0, decimal text up to its NUL. It stays a C string (issue #59): the digits end at the first NUL, which
	// tools/patch_client.py relies on (a shorter modulus is padded with NULs in the literal's place), and a null
	// pointer is its own error.
	void SetPublicKey(const char *ModulusM);
	void Encrypt(patella::TSpan<uint8, 128> Data); // 0x508aa0, in place, exponent 65537; a null pointer is an error

private:
	vlong m_ModulusM; // +0x4
};

// Every object that includes this header constructs its own copy (static initialisers with an atexit
// destructor: Communication.obj 0x5c435c by 0x559da0, TRSA.obj 0x71eff8 by 0x55a1c0, before TRSA.cpp's
// own PublicExponent 0x71f004) and never uses it. The name is ours (issue #24). It stays static (issue #44): the
// builds put TRSA.obj's copy after PublicExponent, and in an anonymous namespace MSVC puts it before (the image's
// order, but a change of the builds' .bss, oracle and x64).
static vlong HeaderExponent(65537); // NOLINT(bugprone-throwing-static-initialization): the binary's (issue #63)
