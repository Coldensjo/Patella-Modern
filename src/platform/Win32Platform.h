// Win32Platform.h: the Win32 implementations of the platform's interfaces (Patella-Modern issue #109, plan M7;
// platform/Platform.h, notes/decisions.md, "#109: the platform interfaces"). Not part of the original source.
//
// The code behind each is what the client called before #109, call for call: timeGetTime, time and Sleep (the
// clock). The file system is the portable TDataFileSystem (platform/FileSystem.h; issue #130); the network (MFC's CAsyncSocket
// and WinSock until issue #125) is NetworkSocket(), below. Since issue #119 also the clipboard (sysinc.cpp's until then),
// the input (GetCursorPos, SetTimer, KillTimer: MainFrm.cpp's and Player.cpp's) and the desktop's windows and display
// (MessageBoxA, EnumWindows, EnumDisplaySettingsA: Player.cpp's and the Vulkan engine's; ChangeDisplaySettingsA since
// issue #124). Since issue #124 the SDL3 window's clipboard and desktop are SDL3's (platform/SdlPlatform.h), these the
// MFC window's and the golden runner's.

#pragma once

#include "common.h"
#include "platform/LoopTimer.h"
#include "platform/Platform.h"

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

// The SDL3 window's input (issue #122; SdlFrame.cpp): the main timer is the loop's (TLoopTimer: SdlFrame.cpp's loop
// asks TimerDue and calls TimerEvent, where the MFC frame got WM_TIMER from SetTimer); the cursor is Win32's GetCursorPos
// until issue #127 (issue #123: the original's last position where Windows cannot read it, which SDL would not keep). The timer's
// Id is not kept: the client has one timer (1).
class TSdlLoopInput final : public IInput {
public:
	bool CursorPosition(TPoint &Point) override;
	void StartTimer(TWindowHandle Window, unsigned int Id, unsigned int Milliseconds) override;
	void StopTimer(TWindowHandle Window, unsigned int Id) override;
	void BeginIdle() override {}
	[[nodiscard]] bool IgnoreCaptureChange() override { return false; }
	[[nodiscard]] bool IgnoreDesktopInput() override { return false; }
	// One tick of the main timer when it is due at Now (the platform clock's milliseconds).
	[[nodiscard]] bool TimerDue(std::uint32_t Now) { return Timer.Due(Now); }

private:
	TLoopTimer Timer;
};

// MessageBoxA, EnumWindows (GetWindowTextA, GetClassNameA), EnumDisplaySettingsA, ChangeDisplaySettingsA (issue #124:
// the Vulkan engine's until then).
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

// The cursor's screen position into a POINT, as GetCursorPos(&Point) wrote it: unchanged where it cannot be read.
inline void ReadCursorPosition(IInput &Input, POINT &Point)
{
	TPoint Position;
	if (Input.CursorPosition(Position)) {
		Point.x = Position.x;
		Point.y = Position.y;
	}
}

// The Win32 implementations, made on first use and never destroyed: Patella.exe's Platform(), and the parts of the
// test build's (TestModeWin32.cpp) that are not the test mode's.
TWin32Clock &Win32Clock();
TDataFileSystem &SystemFileSystem();
// Issue #130 (DataLocationWin32.cpp, Patella.exe's and PatellaTest.exe's): sets SystemFileSystem's folders
// (settings::ChooseLocations: --data, the working folder, the executable's, Patella.ini's DataFolder), once, at the
// start (Player.cpp InitInstance), before the first file is opened; LogDataFiles logs them once the log's settings
// (Patella.ini's, in the folder chosen) are read.
void ConfigureDataFiles();
void LogDataFiles();
// The process's network (issue #125): TPortableSocket in Patella.exe and PatellaTest.exe (platform/NetworkWin32.cpp:
// the lookups' replies to the window, the modal loops' polling), the original's CAsyncSocket calls in the golden runner
// (tests/golden/network/MfcSocket.cpp, whose fakes its Network test logs).
ISocket &NetworkSocket();
TWin32Clipboard &Win32Clipboard();
TWin32Input &Win32Input();
TWin32Window &Win32Window();
// Issue #122: the SDL3 window's input, and the input of the process's window path (platform/WindowChoice.h):
// SdlLoopInput() on the SDL3 path, Win32Input() on MFC's. Patella.exe's Platform() gives it; the test build's TTestInput
// passes its timer on to it without a script. The golden runner, built without PATELLA_SDL3, has Win32's only.
TSdlLoopInput &SdlLoopInput();
IInput &WindowInput();
// Issue #124: the clipboard and the desktop of the process's window path: SDL3's (platform/SdlPlatform.h) on the SDL3
// path, Win32's on MFC's. Patella.exe's Platform() gives them; the test build's TTestWindow passes on to WindowDesktop()
// what it does not do itself, and its clipboard without a script is WindowClipboard(). The golden runner has Win32's.
IClipboard &WindowClipboard();
IWindow &WindowDesktop();

} // namespace patella::platform
