// LinuxPlatform.cpp: the Linux client's platform services (Patella-Modern issue #131, plan M8; notes/decisions.md,
// "#131"). The counterpart of Win32Platform.cpp, for the Linux Patella and PatellaTest (cmake/LinuxX64.cmake): the system
// clock over std::chrono, the file system with the original names found without regard to ASCII case, the SDL3 window's
// input, and the process's Platform().

#include "platform/LoopTimer.h"
#include "platform/Platform.h"
#include "platform/Process.h"
#include "platform/SdlPlatform.h"

#include <SDL3/SDL_mouse.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace patella::platform {

namespace {

// timeGetTime's milliseconds, with its wrap at 32 bits (std::chrono::steady_clock: since boot on Linux, as timeGetTime),
// time(), and Sleep over the same clock.
class TLinuxClock final : public IClock {
public:
	[[nodiscard]] std::uint32_t Milliseconds() override
	{
		return static_cast<std::uint32_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	}
	[[nodiscard]] std::int64_t WallSeconds() override { return std::time(nullptr); }
	void FrameWait(std::uint32_t Duration) override { std::this_thread::sleep_for(std::chrono::milliseconds(Duration)); }
};

constinit TLinuxClock LinuxClockInstance;
constinit TSdlLoopInput SdlLoopInputInstance;

// The program's arguments, read once (the test mode's start reads them before main, as Windows' GetCommandLine answers
// at any time): /proc/self/cmdline, NUL-separated, without the program's name.
std::vector<std::string> &ArgumentList()
{
	static std::vector<std::string> &List = *[] {
		auto *Words = new std::vector<std::string>; // NOLINT(cppcoreguidelines-owning-memory): never destroyed
		std::ifstream File("/proc/self/cmdline", std::ios::binary);
		std::string Text((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		std::size_t Start = 0;
		bool First = true;
		while (Start < Text.size()) {
			std::size_t End = Text.find('\0', Start);
			if (End == std::string::npos)
				End = Text.size();
			if (!First)
				Words->push_back(Text.substr(Start, End - Start));
			First = false;
			Start = End + 1;
		}
		return Words;
	}();
	return List;
}

} // namespace

IClock &SystemClock()
{
	return LinuxClockInstance;
}

// SDL's global mouse position: where SDL cannot tell the screen's position (Wayland has none for a client) it is the
// position in the window with the mouse, which the client's ScreenToClient (SDL_GetWindowPosition, 0 there) leaves as it
// is, so the client still sees its window's coordinates.
bool TSdlLoopInput::CursorPosition(TPoint &Point)
{
	float X = 0.0F;
	float Y = 0.0F;
	SDL_GetGlobalMouseState(&X, &Y);
	Point.x = static_cast<int>(std::floor(X));
	Point.y = static_cast<int>(std::floor(Y));
	return true;
}

void TSdlLoopInput::StartTimer([[maybe_unused]] TWindowHandle Window, [[maybe_unused]] unsigned int Id, unsigned int Milliseconds)
{
	Timer.Start(Milliseconds, Platform().Clock.Milliseconds());
}

void TSdlLoopInput::StopTimer([[maybe_unused]] TWindowHandle Window, [[maybe_unused]] unsigned int Id)
{
	Timer.Stop();
}

// The data folder's files (platform/FileSystem.h), made on first use, never destroyed; set by ConfigureDataFiles
// (DataLocationLinux.cpp), until then the working folder's.
TDataFileSystem &SystemFileSystem()
{
	static TDataFileSystem &Instance = *new TDataFileSystem; // NOLINT(cppcoreguidelines-owning-memory): never destroyed
	return Instance;
}

TSdlLoopInput &SdlLoopInput()
{
	return SdlLoopInputInstance;
}

IInput &WindowInput()
{
	return SdlLoopInputInstance;
}

IClipboard &WindowClipboard()
{
	return SdlClipboard();
}

IWindow &WindowDesktop()
{
	return SdlWindow();
}

const std::vector<std::string> &Arguments()
{
	return ArgumentList();
}

#if !defined(PATELLA_TEST_MODE)
// Patella's platform (Platform.h); the test build's is TestModeWin32.cpp's. The clipboard and the desktop are SDL3's.
TPlatform &Platform()
{
	static TPlatform Instance{LinuxClockInstance, SystemFileSystem(), NetworkSocket(), WindowClipboard(), WindowInput(),
		WindowDesktop()};
	return Instance;
}
#endif

} // namespace patella::platform
