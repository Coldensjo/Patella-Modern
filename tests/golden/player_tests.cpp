// difftest.exe's Player test (moved here from tools/difftest/playertest.cpp, issue #26): Player.cpp's
// text decoders, Surface.cpp's engine getters and MainFrm.cpp's TranslateKey against the original, as
// a golden-corpus test (golden.h). playertest.cpp keeps difftest.exe's link stubs of Player.cpp and
// MainFrm.cpp; the runner's are tests/golden/client_stubs.cpp.

#include "golden_test.h"

#include "MainFrm.h"
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

// MainFrm.cpp's TranslateKey: every virtual key, both extended flags and
// every modifier state (KeyFlags at +0x200 in the original frame).
void TestTranslateKey()
{
	alignas(8) static uint8 Ours[sizeof(CMainFrame)];
	CMainFrame *Frame = (CMainFrame *)Ours;
	for (int Flags = 0; Flags < 8; Flags++) {
		int KeyFlags = ((Flags & 1) ? KEY_SHIFT : 0) | ((Flags & 2) ? KEY_CONTROL : 0) | ((Flags & 4) ? KEY_ALT : 0);
		Frame->KeyFlags = KeyFlags;
		ORIGINAL(*(int *)(Original + 0x200) = KeyFlags);
		for (UINT nChar = 0; nChar < 0x120; nChar++) {
			for (UINT nFlags = 0; nFlags < 0x400; nFlags += 0x80) {
				int A = Frame->TranslateKey(nChar, 1, nFlags);
				int B = ORIGINAL_VALUE("TranslateKey", int, At<KeyFn>(0x4a8210)(Original, NULL, nChar, 1, nFlags));
				CHECK(A == B, "TranslateKey(%u, %u) with %x: %x, original %x", nChar, nFlags, KeyFlags, A, B);
			}
		}
	}
}

} // namespace

void TestPlayer()
{
	TestSurface();
	TestTranslateKey();
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
