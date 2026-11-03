// tests_render: src/vulkan/FilterMask.cpp, which pixels of the Vulkan engine's images come from the
// device's filter (issue #19; notes/vulkan.md, "Pixels"). Portable, no Vulkan.

#include <doctest/doctest.h>

#include "vulkan/FilterMask.h"

#include <vector>

using namespace patella::vulkan;

namespace {

constexpr uint8_t F = TFilterMasks::Filtered;
constexpr uint8_t T = TFilterMasks::Transparent;

// A blit's quad as surfaceDX9.cpp's BlitRectClipped and StretchBlitRect compute it.
TQuad Blit(int x, int y, int Width, int Height, int FromX, int FromY, int FromWidth, int FromHeight, int Size)
{
	TQuad Q{};
	Q.Left = x;
	Q.Top = y;
	Q.Right = x + Width;
	Q.Bottom = y + Height;
	Q.LeftU = static_cast<float>((FromX + 0.5) / Size);
	Q.RightU = static_cast<float>((FromX + FromWidth + 0.5) / Size);
	Q.TopV = static_cast<float>((FromY + 0.5) / Size);
	Q.BottomV = static_cast<float>((FromY + FromHeight + 0.5) / Size);
	Q.TopLeft = Q.TopRight = Q.BottomRight = Q.BottomLeft = 0xffffffffu;
	return Q;
}

int Count(const TFilterMasks &Masks, int Image, int Width, int Height, uint8_t Value)
{
	int Result = 0;
	for (int y = 0; y < Height; y++)
		for (int x = 0; x < Width; x++)
			Result += Masks.At(Image, x, y) == Value;
	return Result;
}

} // namespace

TEST_CASE("new images are transparent; fills and uploads are exact")
{
	TFilterMasks Masks;
	Masks.Ensure(0, 8, 4);
	CHECK(Count(Masks, 0, 8, 4, T) == 32);
	Masks.Fill(0, 2, 1, 6, 3);
	CHECK(Masks.At(0, 2, 1) == 0);
	CHECK(Masks.At(0, 5, 2) == 0);
	CHECK(Masks.At(0, 6, 2) == T);
	CHECK(Count(Masks, 0, 8, 4, 0) == 8);
	std::vector<uint32_t> Pixels = {0xff000000u, 0x00ffffffu, 0x80123456u, 0x00000000u};
	Masks.Upload(0, 6, 2, 2, 2, Pixels.data());
	CHECK(Masks.At(0, 6, 2) == 0);
	CHECK(Masks.At(0, 7, 2) == T);
	CHECK(Masks.At(0, 6, 3) == 0);
	CHECK(Masks.At(0, 7, 3) == T);
	// A new size is a new image.
	Masks.Ensure(0, 3, 3);
	CHECK(Count(Masks, 0, 3, 3, T) == 9);
	CHECK(Masks.At(0, 5, 5) == 0); // outside
	CHECK(Masks.At(7, 0, 0) == 0); // unknown image
}

TEST_CASE("a stretch marks its pixels; a copy carries the marks, except through transparent texels")
{
	TFilterMasks Masks;
	// 0: TEMP_SURFACE (64x64, opaque), 1: the canvas (100x80), 2: a sprite surface with a glyph.
	Masks.Ensure(0, 64, 64);
	Masks.Ensure(1, 100, 80);
	Masks.Ensure(2, 64, 64);
	Masks.Fill(0, 0, 0, 64, 64);
	// The start screen: TEMP_SURFACE stretched over the whole canvas (and beyond it: clipped).
	TQuad Stretch = Blit(0, 0, 120, 90, 0, 0, 64, 64, 64);
	Masks.Draw(1, 0, TFilter::Linear, &Stretch, 1);
	CHECK(Count(Masks, 1, 100, 80, F) == 100 * 80);
	// A glyph over it: a 4x4 copy whose middle texels are opaque, the rest transparent.
	std::vector<uint32_t> Glyph(16, 0x00000000u);
	Glyph[5] = Glyph[6] = Glyph[9] = Glyph[10] = 0xffffffffu;
	Masks.Upload(2, 8, 8, 4, 4, Glyph.data());
	TQuad Copy = Blit(30, 20, 4, 4, 8, 8, 4, 4, 64);
	Masks.Draw(1, 2, TFilter::Linear, &Copy, 1);
	CHECK(Masks.At(1, 31, 21) == 0); // an opaque texel: exact
	CHECK(Masks.At(1, 32, 22) == 0);
	CHECK(Masks.At(1, 30, 20) == F); // a transparent texel: the stretched background stays
	CHECK(Masks.At(1, 33, 23) == F);
	CHECK(Count(Masks, 1, 100, 80, 0) == 4);
}

TEST_CASE("the light: shaded quads mark, the copy to the screen carries them, a fill clears them")
{
	TFilterMasks Masks;
	Masks.Ensure(0, 64, 64);  // TEMP_SURFACE
	Masks.Ensure(1, 64, 64);  // PRIMARY_SURFACE
	Masks.Ensure(2, 100, 80); // the canvas
	Masks.Fill(0, 0, 0, 64, 64);
	// FlushShade: TEMP to PRIMARY (a copy: nothing filtered), then coloured quads into TEMP.
	TQuad Copy = Blit(0, 0, 64, 64, 0, 0, 64, 64, 64);
	Masks.Draw(1, 0, TFilter::Linear, &Copy, 1);
	CHECK(Count(Masks, 1, 64, 64, 0) == 64 * 64);
	TQuad Shade = Blit(0, 0, 32, 32, 0, 0, 32, 32, 64);
	Shade.TopLeft = 0xff102030u;
	Masks.Draw(0, 1, TFilter::Linear, &Shade, 1);
	CHECK(Count(Masks, 0, 64, 64, F) == 32 * 32);
	// The map view copied to the screen without stretching (unscaled): the lit part stays marked.
	TQuad Map = Blit(10, 5, 64, 64, 0, 0, 64, 64, 64);
	Masks.Draw(2, 0, TFilter::Linear, &Map, 1);
	CHECK(Masks.At(2, 10, 5) == F);
	CHECK(Masks.At(2, 41, 36) == F);
	CHECK(Masks.At(2, 42, 36) == 0);
	CHECK(Masks.At(2, 9, 5) == T);
	Masks.Fill(2, 0, 0, 20, 20);
	CHECK(Masks.At(2, 10, 5) == 0);
	CHECK(Masks.At(2, 20, 20) == F);
}

TEST_CASE("the automap's Edge copy reads texel max(k - 1, 0); a self-draw reads the mask as it was")
{
	TFilterMasks Masks;
	Masks.Ensure(0, 106, 106); // SCREEN_SURFACE's texture
	Masks.Ensure(1, 106, 106); // AUTOMAP_SURFACE
	Masks.Fill(0, 0, 0, 106, 106);
	std::vector<uint32_t> Clear(1, 0x00000000u);
	Masks.Upload(0, 5, 7, 1, 1, Clear.data()); // one transparent texel
	TQuad Q{0, 0, 106, 106, 0.0f, 1.0f, 0.0f, 1.0f, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
	Masks.Draw(1, 0, TFilter::Point, &Q, 1);
	CHECK(Masks.At(1, 6, 8) == T); // texel (5, 7) at pixel (6, 8): left as it was (transparent)
	CHECK(Masks.At(1, 5, 7) == 0);
	CHECK(Count(Masks, 1, 106, 106, 0) == 106 * 106 - 1);
	// A copy of an image into itself, overlapping: the source is the mask before the draw.
	Masks.Ensure(2, 16, 1);
	Masks.Fill(2, 0, 0, 8, 1);
	TQuad Shift = Blit(4, 0, 8, 1, 0, 0, 8, 1, 16);
	Shift.TopV = 0.5f;
	Shift.BottomV = 1.5f;
	Masks.Draw(2, 2, TFilter::Linear, &Shift, 1);
	CHECK(Count(Masks, 2, 16, 1, 0) == 12);
	CHECK(Masks.At(2, 12, 0) == T);
}
