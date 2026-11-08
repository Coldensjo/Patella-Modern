// DisplayModeList.h: the display's modes over SDL3 as the client lists and sets them (Patella-Modern issue #124, plan
// M8; notes/decisions.md, "#124: display modes, clipboard, URLs and message boxes over SDL3"). Not part of the original
// source. Portable: no platform header.
//
// The client reads the display's modes as EnumDisplaySettingsA(nullptr, Index) gave them (IWindow::DisplayMode: the
// Graphics Options dialog's list, vulkan/SurfaceVulkan.cpp GetFullscreenModes) and sets one with
// ChangeDisplaySettingsA's width, height and bits per pixel, without a refresh rate (IWindow::SetFullscreenMode).
// SDL3's list (SDL_GetFullscreenDisplayModes) is the same modes in the other order: largest first (width, then height,
// then bits per pixel, then refresh rate, each falling), where Windows lists them smallest first; its four-byte formats
// count 24 bits (SDL_PIXELFORMAT_XRGB8888), where Windows says 32. SDL's TSdlWindow (platform/SdlPlatform.cpp) maps
// them with these rules, which the unit tests check (tests/unit/core_display_modes_test.cpp).

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

// The position in SDL's list of Count modes of EnumDisplaySettings' Index-th mode: Windows' order is SDL's backwards
// (the sizes rising). False past the last mode.
[[nodiscard]] constexpr bool ListedModeIndex(std::size_t Count, std::size_t Index, std::size_t &SdlIndex)
{
	if (Index >= Count)
		return false;
	SdlIndex = Count - 1 - Index;
	return true;
}

// The mode ChangeDisplaySettingsA sets for Wanted (width, height and bits per pixel, no refresh rate) among SDL's
// modes, in SDL's order: the one with Wanted's size and depth at DesktopRefresh (the display's rate before the change)
// where there is one, else the first with them (SDL's highest rate). -1: no mode has them (ChangeDisplaySettingsA's
// DISP_CHANGE_BADMODE).
[[nodiscard]] inline int ChooseFullscreenMode(const std::vector<TListedMode> &Modes, const TDisplayMode &Wanted,
	int DesktopRefresh)
{
	int First = -1;
	for (std::size_t i = 0; i < Modes.size(); i++) {
		const TDisplayMode &Mode = Modes[i].Mode;
		if (Mode.Width != Wanted.Width || Mode.Height != Wanted.Height || Mode.Depth != Wanted.Depth)
			continue;
		if (Modes[i].Refresh == DesktopRefresh)
			return static_cast<int>(i);
		if (First < 0)
			First = static_cast<int>(i);
	}
	return First;
}

} // namespace patella::platform
