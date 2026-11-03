// Quads.h: the Direct3D 9 quads of the Vulkan engine, as Vulkan vertices (issue #19;
// notes/vulkan.md, "Pixels").
//
// surfaceDX9.cpp, the reference engine (plan D7), draws every copy as a textured quad: integer corners
// on the target, texture coordinates (x + 0.5) / width of its corners, one colour per corner. Direct3D
// 9 puts pixel centres on integer coordinates, Vulkan (and the hardware under a Direct3D 9 driver) at
// + 0.5. ConvertQuad gives the vertices that make Vulkan compute what Direct3D 9 computes at every
// pixel, in two ways:
//
// - A one-colour quad that maps texels one to one (the texture's texel centres on the pixel centres:
//   every unscaled blit) is a copy, marked Nearest: its corners stay on whole pixels (no pixel centre
//   lies on an edge) and its coordinates put the texel edges on the pixel edges, so the engine samples
//   it without filtering and gets the exact texels, whatever a device's filter rounds.
// - Every other quad (a stretch, the light's coloured corners, a coordinate between texels) goes to
//   the hardware as a Direct3D 9 driver gives it: the corners half a pixel on, the attributes as they
//   are. The hardware then covers, interpolates and filters as under Direct3D 9; its filter is the
//   device's, so these pixels are as exact as the device's bilinear filter (notes/vulkan.md, "Pixels").
//
// Portable C++ without Vulkan: tests_render checks it on every preset.

#pragma once

#include <array>
#include <cstdint>

namespace patella::vulkan {

// One vertex of the engine's pipelines: the position in pixels of the target, the texture
// coordinate, the colour (0-1, the texture is multiplied by it).
struct TVertex {
	float x, y;
	float u, v;
	float r, g, b, a;
};

// The texture filter of the quad's draw (surfaceDX9.cpp's sampler state: linear, point for the
// automap's upload).
enum class TFilter {
	Point,
	Linear,
};

// A quad as surfaceDX9.cpp's AddQuad and AddShadeQuad take it: the corners on the target, the texture
// coordinates of the left/right and top/bottom edges (Direct3D 9 gets them as floats), and the corner
// colours as D3DCOLOR (0xAARRGGBB).
struct TQuad {
	int Left, Top, Right, Bottom;
	float LeftU, RightU, TopV, BottomV;
	uint32_t TopLeft, TopRight, BottomRight, BottomLeft;
};

// The two triangles of a quad in Direct3D 9's order (top left, top right, bottom right; bottom
// right, bottom left, top left). Nearest: sample without filtering (an exact copy).
struct TQuadVertices {
	std::array<TVertex, 6> Vertices;
	bool Nearest;
};

// TextureWidth/Height: the size of the sampled texture (the u/v scale).
TQuadVertices ConvertQuad(const TQuad &Quad, int TextureWidth, int TextureHeight, TFilter Filter);

// A colour fill (Direct3D 9's ColorFill): two triangles of one colour, alpha 1 (D3DCOLOR_XRGB).
std::array<TVertex, 6> FillVertices(int Left, int Top, int Right, int Bottom, uint32_t Color);

// The first texel a filter reads at the pixel centre Position on one axis, in texel units, as the
// hardware sees it: Direct3D 9's coordinate s = u * Size (point) or u * Size - 0.5 (linear). An axis is
// a copy when its scale is 1 and its start is a whole texel (within 1/512): CopyStart gives that texel.
struct TAxis {
	double Start; // s at the quad's first pixel
	double Scale; // s per pixel
	bool Copy;
	int CopyStart;
};
TAxis MapAxis(int From, int To, float FromCoordinate, float ToCoordinate, int Size, TFilter Filter);

} // namespace patella::vulkan
