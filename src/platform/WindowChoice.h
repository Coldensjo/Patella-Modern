// WindowChoice.h: which window and event loop the Windows x64 client runs on (Patella-Modern issue #122, plan M8;
// notes/decisions.md, "#122: the SDL3 window and its event loop"). Not part of the original source. Portable: no
// platform header.
//
// Until the MFC removal (issue #127) the client has two: the SDL3 window and its loop (SdlFrame.cpp; the default),
// and MFC's frame window and message loop (MainFrm.cpp's CMainFrame, CWinApp::Run). The environment variable
// PATELLA_WINDOW chooses at start-up, for the whole run: `mfc` the MFC path, `sdl` (or unset, or empty) the SDL3 one,
// any other value the SDL3 one (Player.cpp's InitInstance logs it). A run-time switch, not a build option, so that one
// build's Patella.exe and PatellaTest.exe run either way and the comparisons check both from the same build.

#pragma once

#include "compat/msvc.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>

namespace patella::platform {

enum class TWindowSystem : unsigned char { Sdl,
	Mfc };

// PATELLA_WINDOW's value (null: not set): `mfc` (any case) the MFC path, everything else SDL3's.
[[nodiscard]] inline TWindowSystem ParseWindowSystem(const char *Value)
{
	if (Value != nullptr && MsvcStricmp(Value, "mfc") == 0)
		return TWindowSystem::Mfc;
	return TWindowSystem::Sdl;
}

// True where PATELLA_WINDOW names neither path (it is set, not empty, and neither `sdl` nor `mfc`).
[[nodiscard]] inline bool IsUnknownWindowSystem(const char *Value)
{
	return Value != nullptr && *Value != 0 && MsvcStricmp(Value, "sdl") != 0 && MsvcStricmp(Value, "mfc") != 0;
}

// PATELLA_WINDOW's value, read once (before main: the platform's input is chosen with it, platform/Win32Platform.cpp
// and testmode/TestModeWin32.cpp); empty when it is not set.
[[nodiscard]] inline const char *WindowSystemSetting()
{
	static const char *const Value = [] {
		static std::array<char, 16> Text{}; // a fixed copy; longer values are cut
		char *Copy = nullptr;
		std::size_t Length = 0;
		if (MsvcDupenv(&Copy, &Length, "PATELLA_WINDOW") == 0 && Copy != nullptr) {
			const std::string_view View(Copy);
			View.copy(Text.data(), View.size() < Text.size() - 1 ? View.size() : Text.size() - 1);
			std::free(Copy); // NOLINT(cppcoreguidelines-no-malloc): _dupenv_s's
		}
		return static_cast<const char *>(Text.data());
	}();
	return Value;
}

// The process's window path (above).
[[nodiscard]] inline TWindowSystem WindowSystem()
{
	static const TWindowSystem System = ParseWindowSystem(WindowSystemSetting());
	return System;
}

[[nodiscard]] inline bool UseSdlWindow()
{
	return WindowSystem() == TWindowSystem::Sdl;
}

} // namespace patella::platform
