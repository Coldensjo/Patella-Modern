// tests_render: src/vulkan/Quads.cpp, the Direct3D 9 quads of the Vulkan engine as Vulkan vertices
// (issue #19; notes/vulkan.md, "Pixels"). Portable, no Vulkan: every preset runs it.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "vulkan/Quads.h"

#include <cmath>

using namespace patella::vulkan;

namespace {

// A blit's quad as surfaceDX9.cpp's BlitRectClipped computes it.
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

// The value Vulkan interpolates at the pixel centre (x + 0.5, y + 0.5) inside the triangle
// (vertices a, b, c) of an attribute.
double Interpolate(const TVertex &a, const TVertex &b, const TVertex &c, float TVertex::*Attribute, double x, double y)
{
	double Det = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
	double l1 = ((x - a.x) * (c.y - a.y) - (c.x - a.x) * (y - a.y)) / Det;
	double l2 = ((b.x - a.x) * (y - a.y) - (x - a.x) * (b.y - a.y)) / Det;
	return a.*Attribute + l1 * (b.*Attribute - a.*Attribute) + l2 * (c.*Attribute - a.*Attribute);
}

} // namespace

TEST_CASE("a texel-centred blit is a copy")
{
	TAxis Axis = MapAxis(10, 42, static_cast<float>(5.5 / 512), static_cast<float>(37.5 / 512), 512, TFilter::Linear);
	CHECK(Axis.Copy);
	CHECK(Axis.CopyStart == 5);
	CHECK(Axis.Scale == doctest::Approx(1.0));
	// Point sampling of the same coordinates reads the same texels.
	Axis = MapAxis(10, 42, static_cast<float>(5.5 / 512), static_cast<float>(37.5 / 512), 512, TFilter::Point);
	CHECK(Axis.Copy);
	CHECK(Axis.CopyStart == 5);
}

TEST_CASE("the automap's u 0 to 1: a copy for point sampling, between texels for linear")
{
	TAxis Point = MapAxis(0, 106, 0.0f, 1.0f, 106, TFilter::Point);
	CHECK(Point.Copy);
	CHECK(Point.CopyStart == 0);
	TAxis Linear = MapAxis(0, 106, 0.0f, 1.0f, 106, TFilter::Linear);
	CHECK_FALSE(Linear.Copy);
	CHECK(Linear.Start == doctest::Approx(-0.5));
}

TEST_CASE("a stretch is no copy")
{
	CHECK_FALSE(MapAxis(0, 48, static_cast<float>(0.5 / 640), static_cast<float>(32.5 / 640), 640, TFilter::Linear).Copy);
	CHECK_FALSE(MapAxis(0, 48, static_cast<float>(0.5 / 640), static_cast<float>(32.5 / 640), 640, TFilter::Point).Copy);
	CHECK_FALSE(MapAxis(5, 5, 0.0f, 0.0f, 64, TFilter::Point).Copy); // no pixels
}

TEST_CASE("float coordinates of large textures still count as a copy")
{
	// The float rounding of (x + 0.5) / 640 over 640 pixels stays far below 1/512 of a texel.
	for (int From = 0; From < 600; From += 37) {
		TQuad Q = Blit(3, 4, 640 - From, 20, From, 7, 640 - From, 20, 640);
		CHECK(MapAxis(Q.Left, Q.Right, Q.LeftU, Q.RightU, 640, TFilter::Linear).Copy);
		CHECK(MapAxis(Q.Left, Q.Right, Q.LeftU, Q.RightU, 640, TFilter::Linear).CopyStart == From);
	}
}

TEST_CASE("a copy gets texel-centred coordinates for nearest sampling")
{
	TQuad Q = Blit(100, 50, 32, 16, 64, 96, 32, 16, 512);
	TQuadVertices V = ConvertQuad(Q, 512, 512, TFilter::Linear);
	REQUIRE(V.Nearest);
	// At the pixel centre of the quad's first pixel (100.5, 50.5) Vulkan reads texel (64.5, 96.5).
	double u = Interpolate(V.Vertices[0], V.Vertices[1], V.Vertices[2], &TVertex::u, 100.5, 50.5) * 512;
	double v = Interpolate(V.Vertices[0], V.Vertices[1], V.Vertices[2], &TVertex::v, 100.5, 50.5) * 512;
	CHECK(u == doctest::Approx(64.5));
	CHECK(v == doctest::Approx(96.5));
	// And the last pixel's centre reads the last texel's.
	u = Interpolate(V.Vertices[3], V.Vertices[4], V.Vertices[5], &TVertex::u, 131.5, 65.5) * 512;
	v = Interpolate(V.Vertices[3], V.Vertices[4], V.Vertices[5], &TVertex::v, 131.5, 65.5) * 512;
	CHECK(u == doctest::Approx(95.5));
	CHECK(v == doctest::Approx(111.5));
	// The corners stay on whole pixels, in Direct3D 9's order.
	CHECK(V.Vertices[0].x == 100.0f);
	CHECK(V.Vertices[0].y == 50.0f);
	CHECK(V.Vertices[2].x == 132.0f);
	CHECK(V.Vertices[2].y == 66.0f);
	CHECK(V.Vertices[4].x == 100.0f);
	CHECK(V.Vertices[4].y == 66.0f);
}

TEST_CASE("a stretch gets Direct3D 9's coordinates at Vulkan's pixel centres")
{
	TQuad Q = Blit(0, 0, 48, 24, 0, 0, 32, 16, 640);
	TQuadVertices V = ConvertQuad(Q, 640, 640, TFilter::Linear);
	REQUIRE_FALSE(V.Nearest);
	// Direct3D 9 interpolates at the pixel (x, y) itself: u = LeftU + x * (RightU - LeftU) / 48.
	for (int x : {0, 1, 17, 47}) {
		for (int y : {0, 5, 23}) {
			double Expected = Q.LeftU + x * (static_cast<double>(Q.RightU) - Q.LeftU) / 48;
			double ExpectedV = Q.TopV + y * (static_cast<double>(Q.BottomV) - Q.TopV) / 24;
			bool Upper = x >= y * 2;
			const TVertex *T = Upper ? &V.Vertices[0] : &V.Vertices[3];
			CHECK(Interpolate(T[0], T[1], T[2], &TVertex::u, x + 0.5, y + 0.5) == doctest::Approx(Expected).epsilon(1e-6));
			CHECK(Interpolate(T[0], T[1], T[2], &TVertex::v, x + 0.5, y + 0.5) == doctest::Approx(ExpectedV).epsilon(1e-6));
		}
	}
}

TEST_CASE("corner colours: Vulkan's pixel centres get Direct3D 9's values, per triangle")
{
	TQuad Q = Blit(10, 20, 32, 32, 10, 20, 32, 32, 640);
	Q.TopLeft = 0xff102030u;
	Q.TopRight = 0xffff8000u;
	Q.BottomRight = 0xff00ff40u;
	Q.BottomLeft = 0xff8080ffu;
	TQuadVertices V = ConvertQuad(Q, 640, 480, TFilter::Linear);
	auto Channel = [](uint32_t c, int Shift) { return ((c >> Shift) & 0xff) / 255.0; };
	for (int y = 20; y < 52; y += 3) {
		for (int x = 10; x < 42; x += 3) {
			double fx = (x - 10) / 32.0, fy = (y - 20) / 32.0;
			bool Upper = fx >= fy;
			const TVertex *T = Upper ? &V.Vertices[0] : &V.Vertices[3];
			// Direct3D 9 at (x, y): the first triangle (top left, top right, bottom right) or the second.
			double tl = Channel(Q.TopLeft, 16), tr = Channel(Q.TopRight, 16), br = Channel(Q.BottomRight, 16),
				   bl = Channel(Q.BottomLeft, 16);
			double Red = Upper ? tl + fx * (tr - tl) + fy * (br - tr) : tl + fy * (bl - tl) + fx * (br - bl);
			CHECK(Interpolate(T[0], T[1], T[2], &TVertex::r, x + 0.5, y + 0.5) == doctest::Approx(Red).epsilon(1e-5));
		}
	}
	// One colour stays one colour.
	TQuad White = Blit(0, 0, 8, 8, 0, 0, 8, 8, 64);
	TQuadVertices W = ConvertQuad(White, 64, 64, TFilter::Linear);
	for (const TVertex &Vertex : W.Vertices) {
		CHECK(Vertex.r == 1.0f);
		CHECK(Vertex.g == 1.0f);
		CHECK(Vertex.b == 1.0f);
		CHECK(Vertex.a == 1.0f);
	}
}

TEST_CASE("a fill is opaque")
{
	auto V = FillVertices(1, 2, 3, 4, 0x00112233u);
	CHECK(V[0].x == 1.0f);
	CHECK(V[0].y == 2.0f);
	CHECK(V[2].x == 3.0f);
	CHECK(V[2].y == 4.0f);
	for (const TVertex &Vertex : V) {
		CHECK(Vertex.a == 1.0f);
		CHECK(Vertex.r == doctest::Approx(0x11 / 255.0));
		CHECK(Vertex.b == doctest::Approx(0x33 / 255.0));
	}
}
