// DisplayModeList.h: the display's modes over SDL3 as the client lists and sets them (Patella-Modern issues #124 and
// #152, plan M8; notes/decisions.md, "#124: display modes, clipboard, URLs and message boxes over SDL3" and "#152").
// Not part of the original source. Portable: no platform header.
//
// The client reads the display's modes as the original's EnumDisplaySettingsA(nullptr, Index) gave them
// (IWindow::DisplayMode: the Graphics Options dialog's list, vulkan/SurfaceVulkan.cpp GetFullscreenModes) and sets one
// by width, height and bits per pixel, without a refresh rate (IWindow::SetFullscreenMode), on every system through
// SDL3 since issue #152. SDL3's list (SDL_GetFullscreenDisplayModes) has the modes in the other order: largest first
// (width, then height, then bits per pixel, then refresh rate, each falling), where Windows lists them roughly smallest
// first; its four-byte formats count 24 bits (SDL_PIXELFORMAT_XRGB8888), where Windows says 32. SDL's TSdlWindow
// (platform/SdlPlatform.cpp) maps them with these rules, which the unit tests check
// (tests/unit/core_display_modes_test.cpp). Two deliberate differences from the original (issue #152): the list is
// SDL's, so it has the desktop's mode also where Windows does not enumerate it (a custom mode: one more line in Graphics
// Options) and its order is SDL's sorted sizes where Windows' own list is not sorted.

#pragma once

#include "platform/Window.h"

#include <cstddef>
#include <vector>

namespace patella::platform {

// One of SDL's fullscreen modes: its size and depth as Windows gives them, and its refresh rate in millihertz (SDL's
// refresh_rate, rounded).
struct TListedMode {
	TDisplayMode Mode;
	int Refresh = 0;
};

// dmBitsPerPel of an SDL pixel format with BitsPerPixel and BytesPerPixel (SDL_BITSPERPIXEL, SDL_BYTESPERPIXEL): 32 for
// the four-byte formats (Windows' 32-bit modes, which SDL makes XRGB8888, 24 bits), the bits otherwise (16, 15, 24).
[[nodiscard]] constexpr int DisplayDepth(int BitsPerPixel, int BytesPerPixel)
{
	return BytesPerPixel == 4 ? 32 : BitsPerPixel;
}

// The position in SDL's list of Count modes of the client's Index-th mode: SDL's order backwards (the sizes rising, as
// the original's dialog lists them where Windows' own list is sorted). False past the last mode.
[[nodiscard]] constexpr bool ListedModeIndex(std::size_t Count, std::size_t Index, std::size_t &SdlIndex)
{
	if (Index >= Count)
		return false;
	SdlIndex = Count - 1 - Index;
	return true;
}

// The mode for Wanted (width, height and bits per pixel, no refresh rate) among SDL's modes, in SDL's order. SDL can set
// only one of its modes, each with a rate, where the original left the rate to Windows. Of the modes with Wanted's size
// and depth, the one whose rate is closest to DesktopRefresh (the display's rate before the change, in millihertz; the
// lower at equal distance). The same nominal rate differs by a fraction of a hertz from size to size (59.81, 60.00,
// 60.32 for "60 Hz"), so a desktop's exact rate is rarely another size's. With DesktopRefresh unknown (0: SDL has none)
// the first with them (SDL's highest rate). -1: no mode has them (the change fails, DISP_CHANGE_BADMODE on Windows).
[[nodiscard]] inline int ChooseFullscreenMode(const std::vector<TListedMode> &Modes, const TDisplayMode &Wanted,
	int DesktopRefresh)
{
	int Chosen = -1;
	int ChosenDistance = 0;
	for (std::size_t i = 0; i < Modes.size(); i++) {
		const TDisplayMode &Mode = Modes[i].Mode;
		if (Mode.Width != Wanted.Width || Mode.Height != Wanted.Height || Mode.Depth != Wanted.Depth)
			continue;
		if (DesktopRefresh <= 0)
			return static_cast<int>(i);
		const int Distance = Modes[i].Refresh > DesktopRefresh ? Modes[i].Refresh - DesktopRefresh : DesktopRefresh - Modes[i].Refresh;
		if (Chosen < 0 || Distance <= ChosenDistance) {
			Chosen = static_cast<int>(i);
			ChosenDistance = Distance;
		}
	}
	return Chosen;
}

} // namespace patella::platform
