// Window.h: IWindow, the platform's windows and display (Patella-Modern issue #119, plan M7; notes/decisions.md,
// "#119: the platform interfaces, part 2"). Not part of the original source. Portable: no platform header.
//
// What the client asks of the desktop besides its own window's messages: the message boxes (Player.cpp FatalError,
// the start's two), the other top-level windows (InitInstance's count of the Tibia clients, CountTibiaWindows) and
// the display's modes (the Graphics Options dialog's list, vulkan/SurfaceVulkan.cpp GetFullscreenModes). Each goes
// through the process's IWindow (platform/Platform.h):
//
//	TWin32Window     MessageBoxA, EnumWindows with GetWindowTextA and GetClassNameA, EnumDisplaySettingsA
//	                 (platform/Win32Platform.h): Patella.exe's and the golden runner's
//	TTestWindow      the test mode's (testmode/TestModeWin32.cpp): with a script a box is not shown (messages.txt);
//	                 only the process's own windows are visited (issue #96); with --test-display-modes the file's
//	                 modes (issue #48); the rest Win32's
//
// The frame window itself (MainFrm.cpp's CMainFrame, MFC) is the Win32 side's, as the application (Player.cpp's
// CPlayerApp) is: M8's SDL3 window takes their place behind this interface and IInput (Input.h).

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
	// The display's Index-th mode (EnumDisplaySettingsA(nullptr, Index)): false past the last one.
	[[nodiscard]] virtual bool DisplayMode(unsigned int Index, TDisplayMode &Mode) = 0;

protected:
	// Never destroyed through the interface (Clock.h's IClock).
	~IWindow() = default;
};

} // namespace patella::platform
