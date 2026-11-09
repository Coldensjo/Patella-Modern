// Window.h: IWindow, the platform's windows and display (Patella-Modern issue #119, plan M7; notes/decisions.md,
// "#119: the platform interfaces, part 2"). Not part of the original source. Portable: no platform header.
//
// What the client asks of the desktop besides its own window's messages: the message boxes (Player.cpp FatalError,
// the start's two), the other top-level windows (InitInstance's count of the Tibia clients, CountTibiaWindows) and
// the display's modes (the Graphics Options dialog's list, vulkan/SurfaceVulkan.cpp GetFullscreenModes; the desktop's
// own, CreateSurfaces' depth check, issue #138; the fullscreen mode's change, CreateSurfaces and ExitDisplay, issue
// #124). Each goes through the process's IWindow (platform/Platform.h):
//
//	TWin32Window     MessageBoxA, EnumWindows with GetWindowTextA and GetClassNameA (platform/Win32Platform.h): the
//	                 golden runner's, which asks no display modes (issue #152)
//	TSdlWindow       SDL3's (issue #124; platform/SdlPlatform.h): SDL_ShowSimpleMessageBox; the display's modes
//	                 SDL3's on every system (issue #152); the other windows Win32's EnumWindows on Windows (SDL3
//	                 has no such call): Patella.exe's on the SDL3 window
//	TTestWindow      the test mode's (testmode/TestModeWin32.cpp): with a script a box is not shown (messages.txt);
//	                 only the process's own windows are visited (issue #96); with --test-display-modes the file's
//	                 modes (issue #48); the rest the window's (TSdlWindow, as Patella.exe's; the desktop's mode too:
//	                 the machine's); with --test-headless headless (issue #145)
//
// The frame window itself is SdlFrame.cpp's SDL3 window (issue #122; since #127 the only one).

#pragma once

namespace patella::platform {

// A window of the platform: an HWND on Windows (MFC's m_hWnd), M8's SDL_Window. Null: none (a box without owner).
using TWindowHandle = void *;

// The message box's icon: MB_ICONHAND (FatalError's), MB_ICONEXCLAMATION (the start's two boxes).
enum class TMessageBoxIcon : unsigned char { Error,
	Warning };

// A display mode: its size in pixels and its bits per pixel (EnumDisplaySettings' dmPelsWidth, dmPelsHeight,
// dmBitsPerPel).
struct TDisplayMode {
	int Width = 0;
	int Height = 0;
	int Depth = 0;
};

// One top-level window: its title and its class name, each at most 29 characters (the original's GetWindowTextA and
// GetClassNameA with 30), and their lengths (0: none, or it could not be read; the text is then undefined).
using TWindowVisitor = void (*)(const char *Title, int TitleLength, const char *ClassName, int ClassLength);

class IWindow {
public:
	IWindow() = default;
	IWindow(const IWindow &) = delete;
	IWindow &operator=(const IWindow &) = delete;
	IWindow(IWindow &&) = delete;
	IWindow &operator=(IWindow &&) = delete;

	// A modal message box with an OK button; returns when it is closed (MessageBoxA). Text and Caption are ASCII
	// (notes/decisions.md, "#112").
	virtual void ShowMessageBox(TWindowHandle Owner, const char *Text, const char *Caption, TMessageBoxIcon Icon) = 0;
	// Visit for every top-level window of the desktop, in the platform's order (EnumWindows).
	virtual void VisitWindows(TWindowVisitor Visit) = 0;
	// The display's Index-th mode: false past the last one.
	[[nodiscard]] virtual bool DisplayMode(unsigned int Index, TDisplayMode &Mode) = 0;
	// The display's current mode, the desktop's (issue #138): false when it cannot be read.
	[[nodiscard]] virtual bool DesktopMode(TDisplayMode &Mode) = 0;
	// Issue #124: switches the display to Mode (width, height, bits per pixel) for the fullscreen window Window: false
	// where the display has no such mode or the change failed. With Mode null the display goes back to the desktop's
	// mode. Since issue #152 SDL3's on every system.
	virtual bool SetFullscreenMode(TWindowHandle Window, const TDisplayMode *Mode) = 0;
	// True when the client runs without a desktop (the test build's --test-headless, issue #145; Patella.exe's
	// false): its main window is never shown (it exists, hidden, for its messages), the Vulkan engine renders without
	// a surface and presents nothing, and fullscreen mode leaves the display's mode alone.
	[[nodiscard]] virtual bool Headless() = 0;

protected:
	// Never destroyed through the interface (Clock.h's IClock).
	~IWindow() = default;
};

} // namespace patella::platform
