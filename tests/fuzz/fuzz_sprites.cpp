// fuzz_sprites: Tibia.spr (graphics.cpp TGraphics::LoadSprites and LoadSprite; Patella-Modern issue #132), on a
// TGraphics of its own: the input is the file. The header (signature, the number of sprites, an offset per sprite),
// then the sprites: a colour key and runs of transparent and coloured pixels. LoadSprites reads the header (a short
// file is PROGRAM_ERROR 2), then the harness decodes the first 16 sprites (each opens the file again, as the client's LoadSprite) into a buffer of their 32 x 32 RGB triples
// (an allocation of its own size), as the client's memory cache does when it draws one (PROGRAM_ERROR 4 for a short
// sprite). A sprite whose runs pass the 1024th pixel must be a data error (issue #139, item 3; decode.h Overflows): the
// original wrote past the buffer there.

#include "client.h"
#include "decode.h"

#include "graphics.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

const char *const Target = "fuzz_sprites";

patella::TGraphics *Graphics = nullptr;

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	patella::fuzz::EnterWorkFolder("sprites");
	static patella::TGraphics Instance;
	Graphics = &Instance;
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	patella::fuzz::WriteFile("Tibia.spr", Data, Size);
	bool Loaded = false;
	patella::fuzz::Guard(Target, [&] {
		Graphics->LoadSprites("Tibia.spr");
		Loaded = true;
	});
	if (Loaded) {
		std::vector<uint8> Buffer(32 * 32 * 3);
		const int Count = Graphics->NumberOfSprites < 16 ? Graphics->NumberOfSprites : 16;
		for (int Sprite = 1; Sprite <= Count; Sprite++) {
			bool Decoded = false;
			patella::fuzz::Guard(Target, [&] {
				Graphics->LoadSprite(Sprite, Buffer.data());
				Decoded = true;
			});
			const int Offset = Graphics->Sprites[Sprite - 1].Offset; // 0: an empty sprite, nothing read
			if (Decoded && Offset != 0 && patella::fuzz::Overflows(Data, Size, Offset, true))
				patella::fuzz::Fail(Target, "a sprite whose runs pass the 1024th pixel was decoded");
		}
	}
	// The table goes with the next load (LoadSprites lets the one it holds leak, as the original's): freed here.
	Graphics->Sprites.reset();
	return 0;
}
