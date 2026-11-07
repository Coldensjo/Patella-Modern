// decode.h: the harness's own reading of a sprite or picture tile of Tibia.spr or Tibia.pic, for fuzz_sprites and
// fuzz_pictures (Patella-Modern issue #132).
//
// graphics.cpp's LoadSprite and LoadPictureTile decode the runs of a sprite or tile into 32 x 32 RGB triples without a
// bound: a run whose pixels pass the 1024th writes past the buffer (the original's, kept: graphics.cpp's comment
// "Malformed runs write past the buffer"; filed by #132, notes/decisions.md). Overflows reads the input as they do and
// tells whether they would write past it; the harness decodes only the ones that would not, so the fuzzer goes on.

#pragma once

#include <cstddef>
#include <cstdint>

namespace patella::fuzz {

// The bytes of the file from Offset on, as TReadBinaryFile reads them: false past the end.
class TFileReader {
public:
	TFileReader(const std::uint8_t *Data, std::size_t Size, long long Offset) : Begin(Data), Length(Size), At(Offset) {}
	bool Byte(int &Value)
	{
		if (At < 0 || static_cast<unsigned long long>(At) >= Length)
			return false;
		Value = Begin[At++];
		return true;
	}
	bool Word(int &Value)
	{
		int Low = 0;
		int High = 0;
		if (!Byte(Low) || !Byte(High))
			return false;
		Value = Low | (High << 8);
		return true;
	}

private:
	const std::uint8_t *Begin;
	std::size_t Length;
	long long At;
};

// Whether decoding the runs at Offset (after the colour key's three bytes for a sprite) writes past 32 x 32 pixels.
// A read past the file's end throws first (TMyException) and writes nothing more; a negative offset is seek's error.
inline bool Overflows(const std::uint8_t *Data, std::size_t Size, int Offset, bool ColorKey)
{
	if (Offset < 0)
		return false;
	TFileReader File(Data, Size, Offset);
	int Value = 0;
	if (ColorKey && (!File.Byte(Value) || !File.Byte(Value) || !File.Byte(Value)))
		return false;
	int RunsSize = 0;
	if (!File.Word(RunsSize))
		return false;
	int Read = 0;
	long long Pixel = 0;
	while (Read < RunsSize) {
		int Skip = 0;
		int Count = 0;
		if (!File.Word(Skip))
			return false;
		Pixel += Skip;
		if (!File.Word(Count))
			return false;
		Read += 4;
		for (int i = 0; i < Count; i++) {
			for (int c = 0; c < 3; c++) {
				// LoadSprite's Buffer[Pixel * 3 + c] = File.readByte(): the read comes first.
				if (!File.Byte(Value))
					return false;
				if (Pixel >= 32 * 32)
					return true;
			}
			Read += 3;
			Pixel++;
		}
	}
	return false;
}

} // namespace patella::fuzz
