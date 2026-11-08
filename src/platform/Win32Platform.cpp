// Win32Platform.cpp: the Win32 implementations of the platform's interfaces (Patella-Modern issue #109;
// platform/Win32Platform.h). Not part of the original source: each function is the call the client made itself
// before #109 (sysinc.cpp's timeGetTime, the wall clock's time, the frame timer's Sleep; Network.cpp's CAsyncSocket
// and WinSock calls until issue #125, now platform/PortableSocket.cpp's and, in the golden runner,
// tests/golden/network/MfcSocket.cpp's) or before #119 (sysinc.cpp's clipboard, MainFrm.cpp's and Player.cpp's GetCursorPos, SetTimer,
// KillTimer, MessageBoxA and EnumWindows, the Vulkan engine's EnumDisplaySettingsA) or #124 (the Vulkan engine's
// ChangeDisplaySettingsA), with the same arguments.

#include "platform/Win32Platform.h"

#include "common.h"
#include "platform/Platform.h"
#include "types/Cp1252.h"
#include "types/FixedString.h"

#include <mmsystem.h>

#if defined(PATELLA_SDL3)
#include "platform/SdlPlatform.h"
#include "platform/WindowChoice.h"
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace patella::platform {

std::uint32_t TWin32Clock::Milliseconds()
{
	return timeGetTime();
}

std::int64_t TWin32Clock::WallSeconds()
{
	return std::time(nullptr);
}

void TWin32Clock::FrameWait(std::uint32_t Duration)
{
	Sleep(Duration);
}

bool TWin32Clipboard::HasText()
{
	return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
}

// sysinc.cpp's GetClipboardText until issue #119 (issue #112: CF_UNICODETEXT, converted to Windows-1252).
std::optional<std::string> TWin32Clipboard::Text()
{
	if (!OpenClipboard(nullptr))
		return std::nullopt;
	HANDLE Memory = GetClipboardData(CF_UNICODETEXT);
	const SIZE_T Units = GlobalSize(Memory) / sizeof(char16_t);
	const auto *Data = static_cast<const char16_t *>(GlobalLock(Memory));
	std::string Text;
	if (Data != nullptr) {
		std::size_t Length = 0;
		while (Length < Units && Data[Length] != 0)
			Length++;
		Text = patella::text::Utf16ToCp1252(std::u16string_view(Data, Length));
		const BOOL result = GlobalUnlock(Memory);
		PATELLA_ASSERT(result!=0 || GetLastError()==NO_ERROR);
	}
	CloseClipboard();
	return Text;
}

// sysinc.cpp's SetClipboardText until issue #119 (issue #112: the client's text as CF_UNICODETEXT).
bool TWin32Clipboard::SetText(const char *Text)
{
	const std::u16string Wide = patella::text::Cp1252ToUtf16(Text);
	const size_t Length = Wide.size();
	if (!OpenClipboard(nullptr))
		return false;
	HGLOBAL Memory;
	if (EmptyClipboard() && (Memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (Length + 1) * sizeof(char16_t))) != nullptr) {
		auto *textdata = static_cast<char16_t *>(GlobalLock(Memory));
		PATELLA_ASSERT(textdata!=NULL);
		memcpy(textdata, Wide.data(), Length * sizeof(char16_t));
		textdata[Length] = 0;
		const BOOL result = GlobalUnlock(Memory);
		PATELLA_ASSERT_VALUES(result==0 || GetLastError()==NO_ERROR, result, GetLastError());
		if (SetClipboardData(CF_UNICODETEXT, Memory) != nullptr) {
			CloseClipboard();
			return true;
		}
	}
	CloseClipboard();
	return false;
}

bool TWin32Input::CursorPosition(TPoint &Point)
{
	POINT Cursor;
	if (GetCursorPos(&Cursor) == FALSE)
		return false;
	Point.x = Cursor.x;
	Point.y = Cursor.y;
	return true;
}

void TWin32Input::StartTimer(TWindowHandle Window, unsigned int Id, unsigned int Milliseconds)
{
	::SetTimer(static_cast<HWND>(Window), Id, Milliseconds, nullptr);
}

void TWin32Input::StopTimer(TWindowHandle Window, unsigned int Id)
{
	::KillTimer(static_cast<HWND>(Window), Id);
}

// Win32's GetCursorPos until issue #127 (the owner's decision on #123): where Windows cannot read the cursor (another
// desktop: the lock screen, UAC's secure desktop) it fails and the point stays the last one, as the original's;
// SDL_GetGlobalMouseState would report (0, 0) there.
bool TSdlLoopInput::CursorPosition(TPoint &Point)
{
	return Win32Input().CursorPosition(Point);
}

void TSdlLoopInput::StartTimer([[maybe_unused]] TWindowHandle Window, [[maybe_unused]] unsigned int Id,
	unsigned int Milliseconds)
{
	Timer.Start(Milliseconds, Platform().Clock.Milliseconds());
}

void TSdlLoopInput::StopTimer([[maybe_unused]] TWindowHandle Window, [[maybe_unused]] unsigned int Id)
{
	Timer.Stop();
}

void TWin32Window::ShowMessageBox(TWindowHandle Owner, const char *Text, const char *Caption, TMessageBoxIcon Icon)
{
	MessageBoxA(static_cast<HWND>(Owner), Text, Caption, Icon == TMessageBoxIcon::Error ? MB_ICONHAND : MB_ICONEXCLAMATION);
}

namespace {

// Player.cpp's CountTibiaWindows until issue #119, its first half: the window's title and class as it read them.
BOOL CALLBACK VisitWindow(HWND Window, LPARAM Parameter)
{
	const auto Visit = reinterpret_cast<TWindowVisitor>(Parameter);
	TFixedString<32> WindowText;
	TFixedString<32> WindowClass;
	const int TextLength = GetWindowTextA(Window, WindowText, 30);
	const int ClassLength = GetClassNameA(Window, WindowClass, 30);
	Visit(WindowText, TextLength, WindowClass, ClassLength);
	return TRUE;
}

} // namespace

void TWin32Window::VisitWindows(TWindowVisitor Visit)
{
	EnumWindows(VisitWindow, reinterpret_cast<LPARAM>(Visit));
}

namespace {

// EnumDisplaySettingsA of the display with Index (a mode's number or ENUM_CURRENT_SETTINGS), its fields read.
bool ReadDisplaySettings(DWORD Index, TDisplayMode &Mode)
{
	DEVMODEA Settings;
	memset(&Settings, 0, sizeof(Settings));
	Settings.dmSize = sizeof(DEVMODEA);
	if (EnumDisplaySettingsA(nullptr, Index, &Settings) == FALSE)
		return false;
	Mode.Width = (int)Settings.dmPelsWidth;
	Mode.Height = (int)Settings.dmPelsHeight;
	Mode.Depth = (int)Settings.dmBitsPerPel;
	return true;
}

} // namespace

// The Vulkan engine's GetFullscreenModes until issue #119.
bool TWin32Window::DisplayMode(unsigned int Index, TDisplayMode &Mode)
{
	return ReadDisplaySettings(Index, Mode);
}

// The Vulkan engine's CreateSurfaces until issue #138 (it read only the depth).
bool TWin32Window::DesktopMode(TDisplayMode &Mode)
{
	return ReadDisplaySettings(ENUM_CURRENT_SETTINGS, Mode);
}

// The Vulkan engine's CreateSurfaces and ExitDisplay until issue #124 (Direct3D 9's fullscreen device's mode change):
// the primary display's mode, whatever the window.
bool TWin32Window::SetFullscreenMode([[maybe_unused]] TWindowHandle Window, const TDisplayMode *Mode)
{
	if (Mode == nullptr)
		return ChangeDisplaySettingsA(nullptr, 0) == DISP_CHANGE_SUCCESSFUL;
	DEVMODEA Settings;
	memset(&Settings, 0, sizeof(Settings));
	Settings.dmSize = sizeof(DEVMODEA);
	Settings.dmPelsWidth = (DWORD)Mode->Width;
	Settings.dmPelsHeight = (DWORD)Mode->Height;
	Settings.dmBitsPerPel = (DWORD)Mode->Depth;
	Settings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	return ChangeDisplaySettingsA(&Settings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
}

// Stateless, with trivial destructors: constant-initialised, so they are there before any static initialiser and
// after every static destructor.
namespace {
constinit TWin32Clock Win32ClockInstance;
constinit TSystemFileSystem SystemFileSystemInstance;
constinit TWin32Clipboard Win32ClipboardInstance;
constinit TWin32Input Win32InputInstance;
constinit TWin32Window Win32WindowInstance;
constinit TSdlLoopInput SdlLoopInputInstance;
} // namespace

TWin32Clock &Win32Clock()
{
	return Win32ClockInstance;
}

TSystemFileSystem &SystemFileSystem()
{
	return SystemFileSystemInstance;
}

TWin32Clipboard &Win32Clipboard()
{
	return Win32ClipboardInstance;
}

TWin32Input &Win32Input()
{
	return Win32InputInstance;
}

TWin32Window &Win32Window()
{
	return Win32WindowInstance;
}

TSdlLoopInput &SdlLoopInput()
{
	return SdlLoopInputInstance;
}

IInput &WindowInput()
{
#if defined(PATELLA_SDL3)
	if (UseSdlWindow())
		return SdlLoopInputInstance;
#endif
	return Win32InputInstance;
}

IClipboard &WindowClipboard()
{
#if defined(PATELLA_SDL3)
	if (UseSdlWindow())
		return SdlClipboard();
#endif
	return Win32ClipboardInstance;
}

IWindow &WindowDesktop()
{
#if defined(PATELLA_SDL3)
	if (UseSdlWindow())
		return SdlWindow();
#endif
	return Win32WindowInstance;
}

#if !defined(PATELLA_TEST_MODE)
// Patella.exe's and the golden runner's platform (Platform.h); the test build's is TestModeWin32.cpp's. Issue #122: the
// input is the window path's (WindowInput); issue #124: the clipboard and the desktop too.
TPlatform &Platform()
{
	static TPlatform Instance{Win32ClockInstance, SystemFileSystemInstance, NetworkSocket(), WindowClipboard(),
		WindowInput(), WindowDesktop()};
	return Instance;
}
#endif

} // namespace patella::platform
