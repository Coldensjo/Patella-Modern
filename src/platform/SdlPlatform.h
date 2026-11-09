// SdlPlatform.h: the SDL3 implementations of the platform's clipboard and desktop (Patella-Modern issue #124, plan M8;
// notes/decisions.md, "#124: display modes, clipboard, URLs and message boxes over SDL3"). Not part of the original
// source.
//
// The SDL3 window's (SdlFrame.cpp, issue #122) Platform(): what TWin32Clipboard and TWin32Window (Win32Platform.h) do
// with Win32's calls, done with SDL3's. The golden runner keeps Win32's. Each needs SDL's video subsystem, which SdlFrame.cpp's Create starts before the client asks them
// anything (a message box needs none).

#pragma once

#include "platform/Platform.h"

#include <optional>
#include <string>

namespace patella::platform {

// SDL_HasClipboardText, SDL_GetClipboardText, SDL_SetClipboardText: the clipboard's text in UTF-8, converted to and from
// the client's Windows-1252 (types/Cp1252.h; issue #112: '?' for a character Windows-1252 does not have).
class TSdlClipboard final : public IClipboard {
public:
	[[nodiscard]] bool HasText() override;
	[[nodiscard]] std::optional<std::string> Text() override;
	bool SetText(const char *Text) override;
};

// SDL_ShowSimpleMessageBox; the other top-level windows Win32's EnumWindows (TWin32Window's), which SDL3 has no call
// for. The display's modes SDL3's on every system (SdlDisplayMode, below; issue #152).
class TSdlWindow final : public IWindow {
public:
	void ShowMessageBox(TWindowHandle Owner, const char *Text, const char *Caption, TMessageBoxIcon Icon) override;
	void VisitWindows(TWindowVisitor Visit) override;
	[[nodiscard]] bool DisplayMode(unsigned int Index, TDisplayMode &Mode) override;
	[[nodiscard]] bool DesktopMode(TDisplayMode &Mode) override;
	bool SetFullscreenMode(TWindowHandle Window, const TDisplayMode *Mode) override;
	// Patella.exe always has a desktop.
	[[nodiscard]] bool Headless() override { return false; }
#if !defined(_WIN32)
	// Issue #131: this process's own SDL windows (the test build's count of the Tibia windows, issue #96).
	void VisitOwnWindows(TWindowVisitor Visit);
#endif
};

// SDL3's display modes, as IWindow's DisplayMode, DesktopMode and SetFullscreenMode (platform/DisplayModeList.h's
// rules): the primary display's SDL_GetFullscreenDisplayModes backwards, SDL_GetCurrentDisplayMode,
// SDL_SetWindowFullscreenMode on SDL's fullscreen window of that handle (the mode of that size and depth at the rate
// closest to the desktop's; null: back to the desktop's mode). TSdlWindow's on every system (issue #152).
[[nodiscard]] bool SdlDisplayMode(unsigned int Index, TDisplayMode &Mode);
[[nodiscard]] bool SdlDesktopMode(TDisplayMode &Mode);
bool SdlSetFullscreenMode(TWindowHandle Window, const TDisplayMode *Mode);

// Made on first use and never destroyed (Win32Platform.h's).
TSdlClipboard &SdlClipboard();
TSdlWindow &SdlWindow();

} // namespace patella::platform
