// FilterMask.h: which pixels of the engine's images come from the device's filter (issue #19;
// notes/vulkan.md, "Pixels").
//
// The comparison with Tibia.exe's frames is exact except on the pixels whose values the device's
// bilinear filter or the light's interpolation computes (the user's decision of November 3, 2026: up to
// 2 per channel there). TFilterMasks follows the engine's operations with a byte per pixel and image:
// Filtered for such a pixel, Transparent for alpha 0 (a copy leaves the target's pixel there). Uploads
// and fills set the mask exactly; a quad that Quads.cpp makes a copy carries the source's mask over,
// texel for texel as the engine samples it (wrapping, or clamped for an Edge copy), except through
// transparent texels; every other quad marks every pixel it covers. New images are transparent black,
// as the engine clears them. Portable C++, no Vulkan: tests_render checks it on every preset.

#pragma once

#include "vulkan/Quads.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace patella::vulkan {

class TFilterMasks {
public:
	static constexpr uint8_t Filtered = 1;
	static constexpr uint8_t Transparent = 2;

	// Image's size; a new image, or a new size, is transparent black (no pixel filtered).
	void Ensure(int Image, int Width, int Height);
	void Clear();
	void Fill(int Image, int Left, int Top, int Right, int Bottom);
	// Width x Height pixels (D3DCOLOR, rows Width apart) at (x, y): alpha 0 is transparent.
	void Upload(int Image, int x, int y, int Width, int Height, const uint32_t *Pixels);
	// Quads of Texture into Target, as TRenderer::Draw draws them.
	void Draw(int Target, int Texture, TFilter Filter, const TQuad *Quads, size_t Count);
	// The mask's byte (Filtered, Transparent); 0 for an unknown image or a pixel outside it.
	[[nodiscard]] uint8_t At(int Image, int x, int y) const;

private:
	struct TMask {
		int Width = 0;
		int Height = 0;
		std::vector<uint8_t> Pixels;
	};
	TMask *Find(int Image);
	std::vector<TMask> Masks;
};

} // namespace patella::vulkan
