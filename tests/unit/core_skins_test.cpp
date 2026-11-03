// Skins (src/Skins.cpp): the rectangles of the GUI graphics in Tibia.pic.

#include <doctest/doctest.h>

#include "Skins.h"

namespace {

bool Is(const TSkin &Skin, int x, int y, int width, int height)
{
	return Skin.x == x && Skin.y == y && Skin.width == width && Skin.height == height;
}

} // namespace

TEST_CASE("Skins: InitSkins and ExitSkins")
{
	InitSkins();
	REQUIRE(Skins != nullptr);
	CHECK(NumberOfSkins == LASTSKIN);
	CHECK(Is(Skins[0], 0, 0, 0, 0));
	CHECK(Is(Skins[1], 290, 12, 11, 19));
	CHECK(Is(Skins[100], 98, 240, 108, 13));
	CHECK(Is(Skins[153], 264, 0, 12, 96));
	CHECK(Is(Skins[261], 180, 75, 6, 11));
	// Skins[152] is never written (Skins.cpp).
	ExitSkins();
	CHECK(Skins == nullptr);
}
