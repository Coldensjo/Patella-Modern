// Win32Platform.h: the Win32 implementations of the platform's interfaces (Patella-Modern issue #109, plan M7;
// platform/Platform.h, notes/decisions.md, "#109: the platform interfaces"). Not part of the original source.
//
// The code behind each is what the client called before #109, call for call: timeGetTime, time and Sleep (the
// clock). The file system is the portable TDataFileSystem (platform/FileSystem.h; issue #130); the network (MFC's CAsyncSocket
// and WinSock until issue #125) is NetworkSocket(), below. Since issue #119 also the clipboard (sysinc.cpp's until then),
// the input (GetCursorPos, SetTimer, KillTimer: MainFrm.cpp's and Player.cpp's) and the desktop's windows and display
// (MessageBoxA, EnumWindows: Player.cpp's). Since issue #124 the SDL3 window's clipboard and desktop are SDL3's
// (platform/SdlPlatform.h), and these the golden runner's; the display's modes are SDL3's alone since issue #152.

#pragma once

#include "common.h"
#include "platform/Platform.h"
#include "platform/Process.h" // IWYU pragma: export

#include <cstdint>
#include <optional>
#include <string>

namespace patella::platform {

// timeGetTime, time, Sleep.
class TWin32Clock final : public IClock {
public:
	[[nodiscard]] std::uint32_t Milliseconds() override;
	[[nodiscard]] std::int64_t WallSeconds() override;
	void FrameWait(std::uint32_t Duration) override;
};

// The clipboard's CF_UNICODETEXT, converted to and from the client's Windows-1252 (issue #112).
class TWin32Clipboard final : public IClipboard {
public:
	[[nodiscard]] bool HasText() override;
	[[nodiscard]] std::optional<std::string> Text() override;
	bool SetText(const char *Text) override;
};

// GetCursorPos, SetTimer, KillTimer; nothing at the idle step, every capture change the client's.
class TWin32Input final : public IInput {
public:
	bool CursorPosition(TPoint &Point) override;
	void StartTimer(TWindowHandle Window, unsigned int Id, unsigned int Milliseconds) override;
	void StopTimer(TWindowHandle Window, unsigned int Id) override;
	void BeginIdle() override {}
	[[nodiscard]] bool IgnoreCaptureChange() override { return false; }
	[[nodiscard]] bool IgnoreDesktopInput() override { return false; }
};

// MessageBoxA, EnumWindows (GetWindowTextA, GetClassNameA). No display modes (issue #152): the golden runner asks none.
class TWin32Window final : public IWindow {
public:
	void ShowMessageBox(TWindowHandle Owner, const char *Text, const char *Caption, TMessageBoxIcon Icon) override;
	void VisitWindows(TWindowVisitor Visit) override;
	[[nodiscard]] bool DisplayMode(unsigned int Index, TDisplayMode &Mode) override;
	[[nodiscard]] bool DesktopMode(TDisplayMode &Mode) override;
	bool SetFullscreenMode(TWindowHandle Window, const TDisplayMode *Mode) override;
	// Patella.exe always has a desktop.
	[[nodiscard]] bool Headless() override { return false; }
};

// The Win32 implementations, made on first use and never destroyed: Patella.exe's Platform(), and the parts of the
// test build's (TestModeWin32.cpp) that are not the test mode's.
TWin32Clock &Win32Clock();
TWin32Clipboard &Win32Clipboard();
TWin32Input &Win32Input();
TWin32Window &Win32Window();
} // namespace patella::platform
