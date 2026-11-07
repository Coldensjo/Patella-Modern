// Skins (src/Skins.cpp): the rectangles of the GUI graphics in Tibia.pic. Since issue #116 Skins.cpp's data and
// functions are TSkins' (Skins.h), a part of the client's TGraphics; each test makes one of its own.

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
	patella::TSkins Table; // Skins.cpp's data, a part of TGraphics (issue #116)
	Table.InitSkins();
	REQUIRE(Table.Skins != nullptr);
	CHECK(Table.NumberOfSkins == LASTSKIN);
	CHECK(Is(Table.Skins[0], 0, 0, 0, 0));
	CHECK(Is(Table.Skins[1], 290, 12, 11, 19));
	CHECK(Is(Table.Skins[100], 98, 240, 108, 13));
	CHECK(Is(Table.Skins[153], 264, 0, 12, 96));
	CHECK(Is(Table.Skins[261], 180, 75, 6, 11));
	// Skins[152] is never written (Skins.cpp).
	Table.ExitSkins();
	CHECK(Table.Skins == nullptr);
}

// Skins is one pointer, as the original's, raw or a std::unique_ptr (issue #58).
static_assert(sizeof(patella::TSkins::Skins) == sizeof(TSkin *) && alignof(decltype(patella::TSkins::Skins)) == alignof(TSkin *));

TEST_CASE("Skins: InitSkins over a table leaves the old one unfreed, as the original")
{
	patella::TSkins Table; // Skins.cpp's data, a part of TGraphics (issue #116)
	Table.InitSkins();
	REQUIRE(Table.Skins != nullptr);
	const TSkin *const First = &Table.Skins[0]; // the same with the raw pointer and the unique_ptr (issue #58)
	Table.InitSkins();
	REQUIRE(Table.Skins != nullptr);
	// The first table is still allocated (so the second one has another address) and still holds its rectangles.
	CHECK(&Table.Skins[0] != First);
	CHECK(Is(First[1], 290, 12, 11, 19));
	CHECK(Is(Table.Skins[1], 290, 12, 11, 19));
	Table.ExitSkins();
	CHECK(Table.Skins == nullptr);
	// The test frees the leaked table itself (LeakSanitizer in the Clang presets).
	delete[] First; // NOLINT(cppcoreguidelines-owning-memory): the block InitSkins dropped, as the original did
}

TEST_CASE("Skins: ExitSkins without a table")
{
	patella::TSkins Table; // Skins.cpp's data, a part of TGraphics (issue #116)
	Table.ExitSkins();
	CHECK(Table.Skins == nullptr);
	Table.ExitSkins();
	CHECK(Table.Skins == nullptr);
}
