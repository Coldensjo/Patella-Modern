// fuzz_pictures: Tibia.pic (graphics.cpp TGraphics::LoadPictures and LoadPictureTile; Patella-Modern issue #132), on a
// TGraphics of its own: the input is the file. The header (signature, the number of pictures), per picture its size in
// tiles, its colour key and an offset per tile, then the tiles' runs as a sprite's. LoadPictures reads the table (a
// short file is PROGRAM_ERROR 3), then the harness decodes up to 16 tiles of the first pictures into a buffer of their
// 32 x 32 RGB triples (an allocation of its own size), as the client's caches do (PROGRAM_ERROR 5 for a short tile or
// one outside the picture). Not the tiles whose runs would write past the buffer (decode.h Overflows: the original's).
//
// A load that fails leaves the pictures it did not reach with their Offsets never set (new TPicture[n], as the
// original's), and ExitGraphics deletes them: the original's, filed as a finding of #132 (notes/decisions.md). The
// harness reads the table as LoadPictures does and frees the Offsets of the pictures it reached itself.

#include "client.h"
#include "decode.h"

#include "graphics.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

const char *const Target = "fuzz_pictures";

patella::TGraphics *Graphics = nullptr;

// The pictures LoadPictures gave an Offsets table (it allocates it after a picture's five bytes).
int Allocated(const std::uint8_t *Data, std::size_t Size)
{
	patella::fuzz::TInput Input(Data, Size);
	if (Input.Left() < 6)
		return 0;
	static_cast<void>(Input.Bytes(4));
	const int Pictures = Input.Word();
	int Result = 0;
	for (int i = 0; i < Pictures; i++) {
		if (Input.Left() < 5)
			return Result;
		const int TilesX = Input.Byte();
		const int TilesY = Input.Byte();
		static_cast<void>(Input.Bytes(3));
		Result++;
		const std::size_t Bytes = 4 * static_cast<std::size_t>(TilesX) * static_cast<std::size_t>(TilesY);
		if (Input.Left() < Bytes)
			return Result;
		static_cast<void>(Input.Bytes(Bytes));
	}
	return Result;
}

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
					if (Inside && patella::fuzz::Overflows(Data, Size, Entry.Offsets[Entry.TilesX * y + x], false))
						continue;
					patella::fuzz::Guard(Target, [&] { Graphics->LoadPictureTile(Picture, x, y, Buffer.data()); });
				}
			}
		}
	}
	// What ExitGraphics frees, the pictures LoadPictures did not reach left out (above).
	if (Graphics->Pictures != nullptr) {
		const int Reached = Loaded ? Graphics->NumberOfPictures : Allocated(Data, Size);
		for (int i = 0; i < Reached; i++)
			delete[] Graphics->Pictures[i].Offsets;
		Graphics->Pictures.reset();
	}
	return 0;
}
