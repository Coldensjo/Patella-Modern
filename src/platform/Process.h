// Process.h: the platform services of the process that are not interfaces of platform/Platform.h, for both systems
// (Patella-Modern issue #131, plan M8). Not part of the original source. Until #131 these were declared in
// Win32Platform.h; the Linux client's platform layer (LinuxPlatform.cpp, DataLocationLinux.cpp, NetworkLinux.cpp) defines
// them too. Win32Platform.h includes this file.

#pragma once

#include "common.h"
#include "platform/LoopTimer.h"
#include "platform/Platform.h"

#include <cstdint>
#include <string>
#include <vector>

namespace patella::platform {

// The system clock: Windows' timeGetTime, time and Sleep (TWin32Clock); on Linux steady_clock, time and sleep_for.
IClock &SystemClock();

// The SDL3 window's input (issue #122; SdlFrame.cpp): the main timer is the loop's (TLoopTimer: SdlFrame.cpp's loop asks
// TimerDue and calls TimerEvent); the cursor is Win32's GetCursorPos on Windows (issue #123), SDL's global mouse position
// on Linux. The timer's Id is not kept: the client has one timer (1).
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

// The cursor's screen position into a POINT, as GetCursorPos(&Point) wrote it: unchanged where it cannot be read.
inline void ReadCursorPosition(IInput &Input, POINT &Point)
{
	TPoint Position;
	if (Input.CursorPosition(Position)) {
		Point.x = Position.x;
		Point.y = Position.y;
	}
}

// Issue #130: the process's file system (platform/FileSystem.h TDataFileSystem), set once at the start
// (DataLocation*.cpp, Player.cpp InitInstance), before the first file is opened; until then the working folder's. On
// Windows DataLocationWin32.cpp, on Linux DataLocationLinux.cpp (the executable's folder from /proc/self/exe, the
// original names found without regard to ASCII case, the XDG option). LogDataFiles logs them once the log's settings
// (Patella.ini's, in the folder chosen) are read.
TDataFileSystem &SystemFileSystem();
void ConfigureDataFiles();
void LogDataFiles();
// The process's network: TPortableSocket (issue #125), the golden runner's on its side.
ISocket &NetworkSocket();
// The SDL3 window's input, and the input of the process's window (WindowInput: SdlLoopInput() where the client has its
// SDL3 window). Patella.exe's Platform() gives it; the test build's TTestInput passes its timer on to it without a script.
TSdlLoopInput &SdlLoopInput();
IInput &WindowInput();
// The clipboard and the desktop of the process's window: SDL3's (platform/SdlPlatform.h) where built with PATELLA_SDL3.
IClipboard &WindowClipboard();
IWindow &WindowDesktop();

#if !defined(_WIN32)
// The command line of the Linux client, without the program's name: --data and the test mode's words (LinuxPlatform.cpp:
// /proc/self/cmdline, at any time, also before main, as Windows' GetCommandLine).
[[nodiscard]] const std::vector<std::string> &Arguments();
#endif

} // namespace patella::platform
