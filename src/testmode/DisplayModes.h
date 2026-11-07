// DisplayModes.h: the display modes of the test build (issue #48; notes/testmode.md, "The display
// modes").
//
// The Graphics Options dialog lists the display modes Windows reports (EnumDisplaySettings, in
// TSurfaceVulkan::GetFullscreenModes), and those depend on the machine's monitor, driver and scaling,
// which can change between the recording of a reference session and its comparison. PatellaTest.exe
// --test-display-modes <file> answers the client's EnumDisplaySettings(nullptr, Index) from a fixed
// list instead (the test mode's IWindow::DisplayMode, TestModeWin32.cpp TTestWindow; issue #119);
// tools/reference.py writes the file from the session's meta.json (the modes the recording machine reported). Without the option the
// test build asks Windows, as Patella.exe does. This file is the portable half: the file format and
// the choice. Portable: no platform header.
//
// The file, one mode per line, fields separated by spaces; # starts a comment line:
//
//	# patella-display-modes 1
//	<width> <height> <bits per pixel>
//
// in the order EnumDisplaySettings reports them (Index 0 first); at least one mode.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace patella::testmode {

inline constexpr std::string_view DisplayModesHeader = "# patella-display-modes 1";

struct TDisplayMode {
	std::uint32_t Width = 0;
	std::uint32_t Height = 0;
	std::uint32_t Depth = 0; // bits per pixel

	friend bool operator==(const TDisplayMode &, const TDisplayMode &) = default;
};

struct TDisplayModesParseResult {
	bool Ok = false;
	std::vector<TDisplayMode> Modes;
	int ErrorLine = 0;
	std::string Error;
};

[[nodiscard]] TDisplayModesParseResult ParseDisplayModes(std::string_view Text);

// The test build's EnumDisplaySettings(nullptr, Index): with a fixed list (Fixed, from
// --test-display-modes) its Index-th mode, false past its end, and the machine is never asked;
// without one (Fixed null) the machine's, System(Index, Mode), as in Patella.exe.
template <class TSystem>
[[nodiscard]] bool EnumerateDisplayMode(const std::vector<TDisplayMode> *Fixed, std::uint32_t Index, TDisplayMode *Mode, TSystem &&System)
{
	if (Fixed == nullptr)
		return System(Index, Mode);
	if (Index >= Fixed->size())
		return false;
	*Mode = (*Fixed)[Index];
	return true;
}

} // namespace patella::testmode
