// fuzz_pictures: Tibia.pic (graphics.cpp TGraphics::LoadPictures and LoadPictureTile; Patella-Modern issue #132), on a
// TGraphics of its own: the input is the file. The header (signature, the number of pictures), per picture its size in
// tiles, its colour key and an offset per tile, then the tiles' runs as a sprite's. LoadPictures reads the table (a
// short file is PROGRAM_ERROR 3), then the harness decodes up to 16 tiles of the first pictures into a buffer of their
// 32 x 32 RGB triples (an allocation of its own size), as the client's caches do (PROGRAM_ERROR 5 for a short tile or
// one outside the picture). A tile whose runs pass the 1024th pixel must be a data error (issue #139, item 3; decode.h
// Overflows): the original wrote past the buffer there.
//
// After each input ExitGraphics frees the table, as the client's exit does, also after a load that failed: since issue
// #139 (item 2) the pictures LoadPictures did not reach have no Offsets table (the original deleted indeterminate
// pointers there, filed by #132).

#include "client.h"
#include "decode.h"

#include "graphics.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

const char *const Target = "fuzz_pictures";

patella::TGraphics *Graphics = nullptr;

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	patella::fuzz::EnterWorkFolder("pictures");
	static patella::TGraphics Instance;
	Graphics = &Instance;
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	patella::fuzz::WriteFile("Tibia.pic", Data, Size);
	bool Loaded = false;
	patella::fuzz::Guard(Target, [&] {
		Graphics->LoadPictures("Tibia.pic");
		Loaded = true;
	});
	if (Loaded) {
		std::vector<uint8> Buffer(32 * 32 * 3);
		int Tiles = 0;
		for (int Picture = 1; Picture <= Graphics->NumberOfPictures && Tiles < 16; Picture++) {
			const TPicture &Entry = Graphics->Pictures[Picture - 1];
			// One tile more than the picture has in each direction: LoadPictureTile's assertions.
			for (int y = 0; y <= Entry.TilesY && Tiles < 16; y++) {
				for (int x = 0; x <= Entry.TilesX && Tiles < 16; x++, Tiles++) {
					const bool Inside = x < Entry.TilesX && y < Entry.TilesY;
					bool Decoded = false;
					patella::fuzz::Guard(Target, [&] {
						Graphics->LoadPictureTile(Picture, x, y, Buffer.data());
						Decoded = true;
					});
					if (Decoded && Inside && patella::fuzz::Overflows(Data, Size, Entry.Offsets[Entry.TilesX * y + x], false))
						patella::fuzz::Fail(Target, "a tile whose runs pass the 1024th pixel was decoded");
				}
			}
		}
	}
	// The client's exit (issue #139, item 2: after a failed load too).
	Graphics->ExitGraphics();
	// The next input starts as a new process does (ExitGraphics leaves the count; a load that fails before it reads
	// the count would leave it for a table that is gone).
	Graphics->NumberOfPictures = 0;
	return 0;
}
