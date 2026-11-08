// The display's modes over SDL3 (src/platform/DisplayModeList.h; issue #124): SDL's list in Windows' order, SDL's
// four-byte formats as Windows' 32 bits per pixel, and the mode ChangeDisplaySettingsA would set for a size and depth
// without a refresh rate.

#include <doctest/doctest.h>

#include "platform/DisplayModeList.h"
#include "platform/Window.h"

#include <cstddef>
#include <vector>

namespace {

using patella::platform::ChooseFullscreenMode;
using patella::platform::DisplayDepth;
using patella::platform::ListedModeIndex;
using patella::platform::TDisplayMode;
using patella::platform::TListedMode;

TListedMode Listed(int Width, int Height, int Depth, int Refresh)
{
	TListedMode Mode;
	Mode.Mode.Width = Width;
	Mode.Mode.Height = Height;
	Mode.Mode.Depth = Depth;
	Mode.Refresh = Refresh;
	return Mode;
}

TDisplayMode Wanted(int Width, int Height, int Depth)
{
	TDisplayMode Mode;
	Mode.Width = Width;
	Mode.Height = Height;
	Mode.Depth = Depth;
	return Mode;
}

} // namespace

TEST_CASE("Display modes: SDL's formats as Windows' bits per pixel")
{
	// SDL_PIXELFORMAT_XRGB8888 (Windows' 32-bit modes): 24 bits in 4 bytes; ARGB2101010: 32 in 4.
	CHECK(DisplayDepth(24, 4) == 32);
	CHECK(DisplayDepth(32, 4) == 32);
	// RGB24, RGB565, XRGB1555.
	CHECK(DisplayDepth(24, 3) == 24);
	CHECK(DisplayDepth(16, 2) == 16);
	CHECK(DisplayDepth(15, 2) == 15);
}

TEST_CASE("Display modes: Windows' order is SDL's backwards")
{
	std::size_t SdlIndex = 99;
	CHECK_FALSE(ListedModeIndex(0, 0, SdlIndex));
	CHECK(SdlIndex == 99);
	REQUIRE(ListedModeIndex(3, 0, SdlIndex));
	CHECK(SdlIndex == 2);
	REQUIRE(ListedModeIndex(3, 2, SdlIndex));
	CHECK(SdlIndex == 0);
	CHECK_FALSE(ListedModeIndex(3, 3, SdlIndex));

	// SDL's list (largest first, each rate) read in Windows' order, through the Graphics Options dialog's filter
	// (SurfaceVulkan.cpp GetFullscreenModes: each size and depth once) gives the sizes rising, as EnumDisplaySettings'.
	const std::vector<TListedMode> Sdl = {Listed(1920, 1080, 32, 144000), Listed(1920, 1080, 32, 60000),
		Listed(1280, 1024, 32, 60000), Listed(1280, 720, 32, 60000), Listed(800, 600, 32, 60000),
		Listed(800, 600, 16, 60000), Listed(640, 480, 32, 60000)};
	std::vector<TDisplayMode> Dialog;
	for (std::size_t Index = 0; ListedModeIndex(Sdl.size(), Index, SdlIndex); Index++) {
		const TDisplayMode &Mode = Sdl[SdlIndex].Mode;
		bool Known = false;
		for (const TDisplayMode &Other : Dialog)
			Known = Known || (Other.Width == Mode.Width && Other.Height == Mode.Height && Other.Depth == Mode.Depth);
		if (!Known)
			Dialog.push_back(Mode);
	}
	REQUIRE(Dialog.size() == 6);
	CHECK((Dialog[0].Width == 640 && Dialog[0].Height == 480));
	CHECK((Dialog[1].Width == 800 && Dialog[1].Depth == 16));
	CHECK((Dialog[2].Width == 800 && Dialog[2].Depth == 32));
	CHECK((Dialog[3].Width == 1280 && Dialog[3].Height == 720));
	CHECK((Dialog[4].Width == 1280 && Dialog[4].Height == 1024));
	CHECK((Dialog[5].Width == 1920 && Dialog[5].Height == 1080));
}

TEST_CASE("Display modes: the mode a size and depth without a refresh rate sets")
{
	const std::vector<TListedMode> Sdl = {Listed(1920, 1080, 32, 144000), Listed(1920, 1080, 32, 60000),
		Listed(1024, 768, 32, 75000), Listed(1024, 768, 32, 70000), Listed(1024, 768, 16, 60000)};
	// The desktop's rate where the mode has it.
	CHECK(ChooseFullscreenMode(Sdl, Wanted(1920, 1080, 32), 60000) == 1);
	CHECK(ChooseFullscreenMode(Sdl, Wanted(1920, 1080, 32), 144000) == 0);
	// Else SDL's first: the highest rate.
	CHECK(ChooseFullscreenMode(Sdl, Wanted(1024, 768, 32), 60000) == 2);
	// The depth counts.
	CHECK(ChooseFullscreenMode(Sdl, Wanted(1024, 768, 16), 144000) == 4);
	// No such mode: ChangeDisplaySettingsA's DISP_CHANGE_BADMODE.
	CHECK(ChooseFullscreenMode(Sdl, Wanted(1024, 768, 24), 60000) == -1);
	CHECK(ChooseFullscreenMode(Sdl, Wanted(800, 600, 32), 60000) == -1);
	CHECK(ChooseFullscreenMode({}, Wanted(800, 600, 32), 60000) == -1);
}
