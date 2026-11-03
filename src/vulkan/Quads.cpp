// Quads.cpp: the Direct3D 9 quads of the Vulkan engine (Quads.h; notes/vulkan.md, "Pixels").

#include "vulkan/Quads.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace patella::vulkan {

namespace {

// The hardware snaps texture coordinates to 1/256 of a texel before it filters (8 bits of
// sub-texel precision: Direct3D 10 class hardware, and lavapipe); what is closer to a whole texel than
// that is that texel.
constexpr double SubTexel = 256.0;

struct TColor {
	float r, g, b, a;
};

TColor ColorOf(uint32_t Color)
{
	return {static_cast<float>((Color >> 16) & 0xff) / 255.0f, static_cast<float>((Color >> 8) & 0xff) / 255.0f,
		static_cast<float>(Color & 0xff) / 255.0f, static_cast<float>((Color >> 24) & 0xff) / 255.0f};
}

TVertex MakeVertex(double x, double y, double u, double v, TColor Color)
{
	return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(u), static_cast<float>(v), Color.r,
		Color.g, Color.b, Color.a};
}

} // namespace

TAxis MapAxis(int From, int To, float FromCoordinate, float ToCoordinate, int Size, TFilter Filter)
{
	TAxis Axis{};
	const int Pixels = To - From;
	const double Offset = Filter == TFilter::Linear ? 0.5 : 0.0;
	Axis.Start = static_cast<double>(FromCoordinate) * Size - Offset;
	if (Pixels <= 0 || Size <= 0) {
		Axis.Scale = 0.0;
		Axis.Copy = false;
		Axis.Edge = false;
		Axis.CopyStart = 0;
		return Axis;
	}
	Axis.Scale = (static_cast<double>(ToCoordinate) - static_cast<double>(FromCoordinate)) * Size / Pixels;
	// The scale must be 1 to within half a sub-texel over the whole quad.
	const bool OneToOne = std::fabs(Axis.Scale - 1.0) * Pixels < 0.5 / SubTexel;
	const double Snapped = std::round(Axis.Start * SubTexel) / SubTexel;
	if (Filter == TFilter::Point) {
		// Point sampling reads floor(s): a copy. When the samples fall on the texel edges (the
		// automap's upload: u from 0 to 1 over 106 pixels, SurfaceVulkan.cpp WriteImage), the
		// interpolated coordinate lands a hair below each edge except at the corner itself, where it
		// is exact: under Direct3D 9 on the NVIDIA GPU Tibia.exe's frames show texel max(k - 1, 0) at
		// pixel k (issue #19's live comparison; tests_vulkan shows the same on that GPU through
		// Vulkan, while lavapipe rounds to texel k). Starting at edge 0 that is an Edge copy: texel
		// k - 1, clamped to the texture (the engine's nearest, clamp-to-edge sampler). An edge start
		// elsewhere goes to the hardware as Direct3D 9 gives it.
		const bool OnEdge = Snapped == std::floor(Snapped);
		Axis.Edge = OneToOne && OnEdge && Snapped == 0.0;
		Axis.Copy = OneToOne && (!OnEdge || Axis.Edge);
		Axis.CopyStart = Axis.Edge ? -1 : static_cast<int>(std::floor(Snapped));
	} else {
		// Linear filtering reads one texel only on its centre.
		Axis.Edge = false;
		Axis.Copy = OneToOne && Snapped == std::floor(Snapped);
		Axis.CopyStart = static_cast<int>(Snapped);
	}
	return Axis;
}

TQuadVertices ConvertQuad(const TQuad &Quad, int TextureWidth, int TextureHeight, TFilter Filter)
{
	TQuadVertices Result{};
	const TAxis Horizontal = MapAxis(Quad.Left, Quad.Right, Quad.LeftU, Quad.RightU, TextureWidth, Filter);
	const TAxis Vertical = MapAxis(Quad.Top, Quad.Bottom, Quad.TopV, Quad.BottomV, TextureHeight, Filter);
	const bool OneColor = Quad.TopLeft == Quad.TopRight && Quad.TopLeft == Quad.BottomRight && Quad.TopLeft == Quad.BottomLeft;
	const TColor TopLeft = ColorOf(Quad.TopLeft);
	const TColor TopRight = ColorOf(Quad.TopRight);
	const TColor BottomRight = ColorOf(Quad.BottomRight);
	const TColor BottomLeft = ColorOf(Quad.BottomLeft);
	double Left, Top, Right, Bottom, LeftU, RightU, TopV, BottomV;
	if (Horizontal.Copy && Vertical.Copy && OneColor) {
		// A copy: the corners on whole pixels (no pixel centre on an edge) and the texel edges on the
		// pixel edges, so nearest sampling at the pixel centres reads the texel centres, the exact texels.
		Result.Nearest = true;
		Result.Clamp = Horizontal.Edge || Vertical.Edge;
		Left = Quad.Left;
		Top = Quad.Top;
		Right = Quad.Right;
		Bottom = Quad.Bottom;
		LeftU = static_cast<double>(Horizontal.CopyStart) / TextureWidth;
		RightU = static_cast<double>(Horizontal.CopyStart + (Quad.Right - Quad.Left)) / TextureWidth;
		TopV = static_cast<double>(Vertical.CopyStart) / TextureHeight;
		BottomV = static_cast<double>(Vertical.CopyStart + (Quad.Bottom - Quad.Top)) / TextureHeight;
	} else {
		// Filtered or shaded: what a Direct3D 9 driver gives the hardware, the corners half a pixel
		// on (Direct3D 9's pixel centre (x, y) is the hardware's (x + 0.5, y + 0.5)) with the
		// attributes as they are, so the hardware interpolates and filters as for Direct3D 9.
		Result.Nearest = false;
		Result.Clamp = false;
		Left = Quad.Left + 0.5;
		Top = Quad.Top + 0.5;
		Right = Quad.Right + 0.5;
		Bottom = Quad.Bottom + 0.5;
		LeftU = Quad.LeftU;
		RightU = Quad.RightU;
		TopV = Quad.TopV;
		BottomV = Quad.BottomV;
	}
	auto &V = Result.Vertices;
	V[0] = MakeVertex(Left, Top, LeftU, TopV, TopLeft);
	V[1] = MakeVertex(Right, Top, RightU, TopV, TopRight);
	V[2] = MakeVertex(Right, Bottom, RightU, BottomV, BottomRight);
	V[3] = MakeVertex(Right, Bottom, RightU, BottomV, BottomRight);
	V[4] = MakeVertex(Left, Bottom, LeftU, BottomV, BottomLeft);
	V[5] = MakeVertex(Left, Top, LeftU, TopV, TopLeft);
	return Result;
}

std::array<TVertex, 6> FillVertices(int Left, int Top, int Right, int Bottom, uint32_t Color)
{
	const TColor C = ColorOf(Color | 0xff000000u);
	return {MakeVertex(Left, Top, 0.0, 0.0, C), MakeVertex(Right, Top, 0.0, 0.0, C), MakeVertex(Right, Bottom, 0.0, 0.0, C),
		MakeVertex(Right, Bottom, 0.0, 0.0, C), MakeVertex(Left, Bottom, 0.0, 0.0, C), MakeVertex(Left, Top, 0.0, 0.0, C)};
}

} // namespace patella::vulkan
