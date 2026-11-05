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

// Skins is one pointer, as the original's, raw or a std::unique_ptr (issue #58).
static_assert(sizeof(Skins) == sizeof(TSkin *) && alignof(decltype(Skins)) == alignof(TSkin *));

TEST_CASE("Skins: InitSkins over a table leaves the old one unfreed, as the original")
{
	InitSkins();
	REQUIRE(Skins != nullptr);
	const TSkin *const First = &Skins[0]; // the same with the raw pointer and the unique_ptr (issue #58)
	InitSkins();
	REQUIRE(Skins != nullptr);
	// The first table is still allocated (so the second one has another address) and still holds its rectangles.
	CHECK(&Skins[0] != First);
	CHECK(Is(First[1], 290, 12, 11, 19));
	CHECK(Is(Skins[1], 290, 12, 11, 19));
	ExitSkins();
	CHECK(Skins == nullptr);
	// The test frees the leaked table itself (LeakSanitizer in the Clang presets).
	delete[] First; // NOLINT(cppcoreguidelines-owning-memory): the block InitSkins dropped, as the original did
}

TEST_CASE("Skins: ExitSkins without a table")
{
	ExitSkins();
	CHECK(Skins == nullptr);
	ExitSkins();
	CHECK(Skins == nullptr);
}
