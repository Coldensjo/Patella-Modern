// difftest.exe's Player test (moved here from tools/difftest/playertest.cpp, issue #26): Player.cpp's
// text decoders and Surface.cpp's engine getters against the original, as a golden-corpus test (golden.h). The
// runner's link stubs are tests/golden/client_stubs.cpp.

#include "golden_test.h"

#include "Player.h"
#include "Surface.h"
#include "testsurface.h"

#include <string.h>


namespace {

// Surface.cpp's getters of curEngine and AvailableEngines (0x71d0c4, 0x71d0c8).
void TestSurface()
{
	for (int Engine = -2; Engine < 5; Engine++) {
		patella::bridge::GraphicsEngine().curEngine = Engine;
		ORIGINAL(Global<int>(0x71d0c4) = Engine);
		int Original = ORIGINAL_VALUE("GetCurrentEngine", int, At<int(__cdecl *)()>(0x4eaa80)());
		CHECK(GetCurrentEngine() == Original, "GetCurrentEngine %d", Engine);
	}
	// A member that does not read this in the original (the binary passes the surface in ECX). Ours reads the engine's
	// shared data through the surface (issue #138), so a real object on every platform (on Linux since issue #91: a
	// call on a null pointer is undefined, and UBSan reports it); its data is the client's TGraphicsEngine.
	TTestSurface Object;
	TSurface *Surface = &Object;
	uint8 Available = ORIGINAL_VALUE("GetAvailableEngines", uint8, At<uint8(__cdecl *)()>(0x4eabe0)());
	CHECK(Surface->GetAvailableEngines() == 0 && Available == 0, "GetAvailableEngines");
}

// The original frame's TranslateKey (every modifier state, virtual key and extended flag): retired after issue #127
// with the MFC frame it compared. The client's key rules are tests/unit/core_key_input_test.cpp's. The frozen recording
// still holds the original's values, so they are read here and not compared.
void RetireTranslateKey()
{
	const int Values = 8 * 0x120 * 8;
	for (int i = 0; i < Values; i++)
		(void)ORIGINAL_VALUE("TranslateKey", int, 0);
	golden::RetireChecks(Values);
}

} // namespace

void TestPlayer()
{
	TestSurface();
	RetireTranslateKey();
	struct {
		uint32 Original;
		void (*Ours)(char *);
		const char *Expected;
	} Decoders[] = {
		{0x4d9140, GetWindowTitle, "Tibia   "},
		{0x4d92b0, GetWindowClassName, "TibiaClient"},
	};
	for (auto &D : Decoders) {
		char A[64], B[64];
		memset(A, 0x55, sizeof(A));
		memset(B, 0x55, sizeof(B));
		D.Ours(A);
		ORIGINAL(At<TextFn>(D.Original)(B));
		golden::Output("decoded text", B);
		CHECK(memcmp(A, B, sizeof(A)) == 0 && strcmp(A, D.Expected) == 0, "decoded text '%s', original '%s'", A, B);
	}
}
