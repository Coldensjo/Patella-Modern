// SdlFrame.cpp: the client's main window and event loop on SDL3 (Patella-Modern issue #122, plan M8; SdlFrame.h,
// notes/decisions.md, "#122: the SDL3 window and its event loop"). Not part of the original source.
//
// Each function says which of MainFrm.cpp's (CMainFrame) or MFC's it stands for; the client calls are theirs, in
// their order. What SDL3 does here (the window, its placement and size, the fullscreen switch, its activation, close
// and display events, the loop): SDL_CreateWindow, SDL_SetWindowPosition/Size, SDL_SetWindowFullscreen, the window and
// display events. What stays Win32's until its own issue, in the "Win32 until ..." parts below: the keys, buttons and
// wheel as window messages and the Windows cursors (#123), the host lookup's message WM_USER + 1 (the socket, #125),
// ShellExecute (#124), and on Windows the message pump itself (SDL's own is off: the loop dispatches the thread's
// messages as MFC's did, so the MFC sockets' and the test mode's messages are dispatched in the same order).
//
// Events: the size, move and activation changes come through an event watch (SDL_AddEventWatch), which SDL calls as it
// sends the event, inside the window procedure, so the client sees a size change within the call that made it, as
// MFC's WM_SIZE handler did (SetDisplayMode's Resize(0, 0) comes after the fullscreen size's). What would change the
// window from inside SDL's window procedure (the close, leaving fullscreen on deactivation, a display change's move)
// is only noted there and done by the loop right after the message that caused it.

#include "SdlFrame.h"

#include "Control.h"
#include "MainFrm.h"
#include "MainWindow.h"
#include "Network.h"
#include "Preferences.h"
#include "Status.h"
#include "Surface.h"
#include "common.h"
#include "compat/span.h"
#include "log/Log.h"
#include "platform/Platform.h"
#include "platform/Win32Platform.h"

#define SDL_MAIN_HANDLED // NOLINT(cppcoreguidelines-macro-usage): SDL_main.h's switch; MFC's WinMain is the entry
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <commctrl.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace patella::sdlframe {

namespace {

// The window's state: CMainFrame's members and MainFrm.cpp's file-local data, for this window.
SDL_Window *Window = nullptr;
HWND WindowHandle = nullptr;
bool Created = false;         // InitMainWindow done: size changes go to the client
bool CloseWanted = false;     // a close request (WM_CLOSE, SDL_EVENT_WINDOW_CLOSE_REQUESTED) to answer
bool DestroyWanted = false;   // OnClose accepted: the window is destroyed after the current message
bool Destroyed = false;       // the window is gone: the loop ends
bool LeaveFullscreen = false; // an activation change in fullscreen mode (OnActivateApp)
bool DisplayChange = false;   // a display mode change (OnDisplayChange), with the display's new size
int DisplayWidth = 0;
int DisplayHeight = 0;

int KeyFlags = 0;               // CMainFrame::KeyFlags
bool LeftButtonPressed = false; // MainFrm.cpp's
bool RightButtonPressed = false;
bool Closing = false;
int WheelDelta = 0;

// The window rectangle as the client stores it (Patella.ini's window position and size, SetWindowedMode): the MFC
// frame's outer rectangle (GetWindowRect, WINDOWPLACEMENT's rcNormalPosition).
struct TOuterRect {
	int Left = 0;
	int Top = 0;
	int Right = 0;
	int Bottom = 0;
};

// CMainFrame::Placement: the windowed placement while fullscreen, and the normal (not maximized) rectangle.
TOuterRect Placement;
bool PlacementMaximized = false;
TOuterRect NormalRect; // the last rectangle the window had while neither maximized, minimized nor fullscreen
bool NormalKnown = false;

// The frame around the client area, which the client's stored rectangles include: the window's borders (GetWindowRect
// against GetClientRect, the original's AddFrameSize). The window has the MFC frame's client edge (SetFrameStyle).
struct TFrame {
	int Top = 0;
	int Left = 0;
	int Bottom = 0;
	int Right = 0;
};

TFrame FrameSize()
{
	TFrame Frame;
	if (!SDL_GetWindowBordersSize(Window, &Frame.Top, &Frame.Left, &Frame.Bottom, &Frame.Right))
		Frame = TFrame{};
	return Frame;
}

// Win32 until issue #127: the extended styles of MFC's frame window, which SDL3 does not set. MFC's frame windows have
// a client edge (CFrameWnd::PreCreateWindow's WS_EX_CLIENTEDGE: 2 pixels around the client area), which SetDisplayMode
// takes away with WS_EX_APPWINDOW for fullscreen mode and gives back for windowed mode (ModifyStyleEx). SDL3 reads the
// window's extended styles when it computes the outer rectangle, so its sizes stay the client area's, and the outer
// rectangle (Patella.ini's, the comparisons' GetWindowPlacement) is the MFC frame's for the same client area.
void SetFrameStyle(DWORD Remove, DWORD Add)
{
	const LONG_PTR Style = GetWindowLongPtrA(WindowHandle, GWL_EXSTYLE);
	SetWindowLongPtrA(WindowHandle, GWL_EXSTYLE, static_cast<LONG_PTR>((static_cast<DWORD>(Style) & ~Remove) | Add));
	SetWindowPos(WindowHandle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// GetSystemMetrics(SM_CXSCREEN), SM_CYSCREEN: the primary display's size.
void ScreenSize(int &Width, int &Height)
{
	SDL_Rect Bounds{};
	if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &Bounds))
		Bounds = SDL_Rect{};
	Width = Bounds.w;
	Height = Bounds.h;
}

// GetWindowPlacement's rcNormalPosition is in workspace coordinates: relative to the primary display's work area,
// which differs from the screen where the taskbar is at the top or the left. The offset, to keep the stored numbers
// the original's (its window then moves by the taskbar at each start, as Tibia.exe's does).
void WorkspaceOffset(int &X, int &Y)
{
	const SDL_DisplayID Display = SDL_GetPrimaryDisplay();
	SDL_Rect Bounds{};
	SDL_Rect Usable{};
	X = 0;
	Y = 0;
	if (SDL_GetDisplayBounds(Display, &Bounds) && SDL_GetDisplayUsableBounds(Display, &Usable)) {
		X = Usable.x - Bounds.x;
		Y = Usable.y - Bounds.y;
	}
}

// GetWindowRect: the outer rectangle on the screen.
TOuterRect WindowRect()
{
	int X = 0;
	int Y = 0;
	int Width = 0;
	int Height = 0;
	SDL_GetWindowPosition(Window, &X, &Y);
	SDL_GetWindowSize(Window, &Width, &Height);
	const TFrame Frame = FrameSize();
	return TOuterRect{X - Frame.Left, Y - Frame.Top, X + Width + Frame.Right, Y + Height + Frame.Bottom};
}

bool IsNormal()
{
	const SDL_WindowFlags Flags = SDL_GetWindowFlags(Window);
	return (Flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN)) == 0;
}

bool IsMaximized()
{
	return (SDL_GetWindowFlags(Window) & SDL_WINDOW_MAXIMIZED) != 0;
}

void NoteNormalRect()
{
	if (Window != nullptr && IsNormal()) {
		NormalRect = WindowRect();
		NormalKnown = true;
	}
}

// GetWindowPlacement: the normal rectangle (in workspace coordinates) and whether the window is maximized.
void ReadPlacement()
{
	NoteNormalRect();
	TOuterRect Rect = NormalKnown ? NormalRect : WindowRect();
	int X = 0;
	int Y = 0;
	WorkspaceOffset(X, Y);
	Rect.Left -= X;
	Rect.Right -= X;
	Rect.Top -= Y;
	Rect.Bottom -= Y;
	Placement = Rect;
	PlacementMaximized = IsMaximized();
}

// MoveWindow(x, y, Width, Height, TRUE) with the outer rectangle: the client area inside the frame.
void MoveWindowTo(int X, int Y, int Width, int Height)
{
	const TFrame Frame = FrameSize();
	SDL_SetWindowPosition(Window, X + Frame.Left, Y + Frame.Top);
	SDL_SetWindowSize(Window, Width - Frame.Left - Frame.Right, Height - Frame.Top - Frame.Bottom);
	NoteNormalRect();
}

// SetWindowPlacement(&Placement): the normal rectangle back (from workspace coordinates), maximized again if it was.
void RestorePlacement()
{
	int X = 0;
	int Y = 0;
	WorkspaceOffset(X, Y);
	if (IsMaximized())
		SDL_RestoreWindow(Window);
	MoveWindowTo(Placement.Left + X, Placement.Top + Y, Placement.Right - Placement.Left, Placement.Bottom - Placement.Top);
	if (PlacementMaximized)
		SDL_MaximizeWindow(Window);
}

// CMainFrame::AddFrameSize.
void AddFrameSize(int *Width, int *Height)
{
	const TFrame Frame = FrameSize();
	*Width += Frame.Left + Frame.Right;
	*Height += Frame.Top + Frame.Bottom;
}

// CMainFrame::SetDefaultWindowPosition: 600x450 client area, centred.
void SetDefaultWindowPosition()
{
	int Width = 600;
	int Height = 450;
	AddFrameSize(&Width, &Height);
	int ScreenWidth = 0;
	int ScreenHeight = 0;
	ScreenSize(ScreenWidth, ScreenHeight);
	const int x = (ScreenWidth - Width) / 2;
	const int y = (ScreenHeight - Height) / 2;
	SetWindowedMode(x, y, Width, Height, false);
	MoveWindowTo(x, y, Width, Height);
}

// CMainFrame::OnSize: the client area at least 550x450 (SDL_SetWindowMinimumSize keeps the window there; a size below
// it still reaches the client as 550x450).
void OnSize(int cx, int cy)
{
	if ((SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED) != 0)
		return;
	Resize(cx < 550 ? 550 : cx, cy < 450 ? 450 : cy);
}

// CMainFrame::OnActivateApp: forgets the held modifiers and leaves fullscreen mode (after the message).
void OnActivateApp()
{
	KeyFlags = 0;
	if (GetViewMode() == 2)
		LeaveFullscreen = true;
}

// CMainFrame::OnDisplayChange: a screen smaller than the window in windowed mode moves it back to the default place.
void OnDisplayChange(int Width, int Height)
{
	if (GetViewMode() == 1) {
		const TOuterRect Rect = WindowRect();
		if (Width < Rect.Right - Rect.Left || Height < Rect.Bottom - Rect.Top)
			SetDefaultWindowPosition();
		DisplayChanged = true;
	}
}

// The event watch (above): called by SDL as it sends each event.
bool SDLCALL WatchEvent([[maybe_unused]] void *UserData, SDL_Event *Event)
{
	if (Window == nullptr)
		return true;
	switch (Event->type) {
	case SDL_EVENT_WINDOW_RESIZED:
		if (Event->window.windowID == SDL_GetWindowID(Window)) {
			NoteNormalRect();
			if (Created)
				OnSize(Event->window.data1, Event->window.data2);
		}
		break;
	case SDL_EVENT_WINDOW_RESTORED:
		// WM_SIZE with SIZE_RESTORED after a minimize: Resize with the size the window has.
		if (Event->window.windowID == SDL_GetWindowID(Window) && Created) {
			int Width = 0;
			int Height = 0;
			SDL_GetWindowSize(Window, &Width, &Height);
			OnSize(Width, Height);
		}
		break;
	case SDL_EVENT_WINDOW_MOVED:
		if (Event->window.windowID == SDL_GetWindowID(Window))
			NoteNormalRect();
		break;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		if (Event->window.windowID == SDL_GetWindowID(Window) && Created)
			OnActivateApp();
		break;
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		if (Event->window.windowID == SDL_GetWindowID(Window))
			CloseWanted = true;
		break;
	case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
		if (const SDL_DisplayMode *Mode = SDL_GetCurrentDisplayMode(Event->display.displayID); Mode != nullptr) {
			DisplayChange = true;
			DisplayWidth = Mode->w;
			DisplayHeight = Mode->h;
		}
		break;
	default: break;
	}
	return true;
}

// CMainFrame::OnClose: Control.cpp may refuse (the logout warning in the game).
void OnClose()
{
	if (!Closing && !CloseRequest())
		return;
	DestroyWanted = true;
}

// Win32 until issue #123: the Windows cursors (MainFrm.cpp's), and the keys, buttons and wheel as window messages.
HCURSOR HandCursor = nullptr;    // cursor resource 0x89 (mouse cursor 6)
HCURSOR SizeCursor = nullptr;    // IDC_SIZENS (mouse cursor 2)
HCURSOR WaitCursor = nullptr;    // IDC_WAIT (mouse cursor 3)
HCURSOR ArrowCursor = nullptr;   // IDC_ARROW (mouse cursor 1)
HCURSOR DefaultCursor = nullptr; // cursor resource 0x84 (the others)

constexpr UINT_PTR InputSubclassId = 122;

// CMainFrame::TranslateKey.
int TranslateKey(UINT nChar, UINT nFlags)
{
	const bool Extended = (nFlags & 0x100) == 0x100;
	switch (nChar) {
	case VK_SHIFT: return KEY_SHIFT;
	case VK_CONTROL: return KEY_CONTROL;
	case VK_MENU: return KEY_ALT;
	case VK_PRIOR: return Extended ? 0x114 : 0x119;
	case VK_NEXT: return Extended ? 0x115 : 0x117;
	case VK_END: return Extended ? 0x113 : 0x116;
	case VK_HOME: return Extended ? 0x112 : 0x118;
	case VK_LEFT: return 0x10d;
	case VK_UP: return 0x10c;
	case VK_RIGHT: return 0x10e;
	case VK_DOWN: return 0x10f;
	case VK_INSERT: return 0x110;
	case VK_DELETE: return 0x111;
	default: break;
	}
	if (nChar >= VK_F1 && nChar <= VK_F12)
		return static_cast<int>(nChar + 0x90);
	if ((KeyFlags & KEY_ALT) != 0 && nChar == VK_RETURN)
		return VK_RETURN;
	return 0;
}

// CMainFrame::KeyDown: the modifiers remembered, Alt+F4 closes, the other translated keys go to KeyPress.
void KeyDown(UINT nChar, UINT nFlags)
{
	const int Key = TranslateKey(nChar, nFlags);
	if (Key == KEY_SHIFT) {
		KeyFlags |= KEY_SHIFT;
	} else if (Key == KEY_CONTROL) {
		KeyFlags |= KEY_CONTROL;
	} else if (Key == KEY_ALT) {
		KeyFlags |= KEY_ALT;
	} else if (Key == 0x103) {
		if ((KeyFlags & KEY_ALT) != 0) {
			OnClose();
			return;
		}
	} else if (Key < 1) {
		return;
	}
	KeyPress(KeyFlags | Key);
}

// CMainFrame::KeyUp.
void KeyUp(UINT nChar, UINT nFlags)
{
	const int Key = TranslateKey(nChar, nFlags);
	if (Key == KEY_SHIFT)
		KeyFlags &= ~KEY_SHIFT;
	else if (Key == KEY_CONTROL)
		KeyFlags &= ~KEY_CONTROL;
	else if (Key == KEY_ALT)
		KeyFlags &= ~KEY_ALT;
}

// CMainFrame::OnChar: characters up to 255; with Control (and not Alt) 1-26 become 'a'-'z'.
void OnChar(UINT nChar)
{
	if ((KeyFlags & KEY_CONTROL) != 0 && (KeyFlags & KEY_ALT) == 0) {
		if (nChar == 0 || nChar > 26)
			return;
		nChar += 0x60;
	} else if (nChar > 255) {
		return;
	}
	KeyPress(KeyFlags | static_cast<int>(nChar));
}

// The button handlers of CMainFrame: in fullscreen mode the points are screen points; the first button down takes the
// mouse, the last one up releases it.
POINT MessagePoint(HWND Hwnd, LPARAM LParam)
{
	POINT Point = {static_cast<short>(LOWORD(LParam)), static_cast<short>(HIWORD(LParam))};
	if (GetViewMode() == 2)
		::ClientToScreen(Hwnd, &Point);
	return Point;
}

void ButtonDown(HWND Hwnd, LPARAM LParam, bool Left, bool Double)
{
	const POINT Point = MessagePoint(Hwnd, LParam);
	if (Left) {
		if (Double)
			LeftButtonDoubleClick(Point.x, Point.y, KeyFlags);
		else
			LeftButtonDown(Point.x, Point.y, KeyFlags);
		LeftButtonPressed = true;
		if (!RightButtonPressed)
			::SetCapture(Hwnd);
	} else {
		RightButtonDown(Point.x, Point.y, KeyFlags); // a right double click is a click (OnRButtonDblClk)
		RightButtonPressed = true;
		if (!LeftButtonPressed)
			::SetCapture(Hwnd);
	}
}

void ButtonUp(HWND Hwnd, LPARAM LParam, bool Left)
{
	if (Left) {
		if (LeftButtonPressed) {
			const POINT Point = MessagePoint(Hwnd, LParam);
			LeftButtonUp(Point.x, Point.y, KeyFlags);
			LeftButtonPressed = false;
			if (!RightButtonPressed)
				::ReleaseCapture();
		}
	} else if (RightButtonPressed) {
		const POINT Point = MessagePoint(Hwnd, LParam);
		RightButtonUp(Point.x, Point.y, KeyFlags);
		RightButtonPressed = false;
		if (!LeftButtonPressed)
			::ReleaseCapture();
	}
}

// CMainFrame::OnCaptureChanged: another window took the mouse; the held buttons are released at the cursor.
void OnCaptureChanged(HWND Hwnd)
{
	if (platform::Platform().Input.IgnoreCaptureChange())
		return;
	POINT Point;
	platform::ReadCursorPosition(platform::Platform().Input, Point); // GetCursorPos (issue #119)
	if (GetViewMode() == 1)
		::ScreenToClient(Hwnd, &Point);
	if (LeftButtonPressed) {
		LeftButtonUp(Point.x, Point.y, KeyFlags);
		LeftButtonPressed = false;
	}
	if (RightButtonPressed) {
		RightButtonUp(Point.x, Point.y, KeyFlags);
		RightButtonPressed = false;
	}
}

// CMainFrame::OnMouseWheel: one MouseWheel call per notch (120) at the client's mouse position.
void OnMouseWheel(short zDelta)
{
	const int Delta = WheelDelta + zDelta;
	WheelDelta = Delta % 120;
	for (int i = 0; i < std::abs(Delta / 120); i++) {
		// The original's order: a call's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
		const int MouseY = GetMouseY();
		MouseWheel(GetMouseX(), MouseY, Delta > 0 ? 1 : 2);
	}
}

// CMainFrame::OnSetCursor: outside the client area Windows sets the cursor.
bool OnSetCursor(HWND Hwnd)
{
	if (GetViewMode() != 2) {
		RECT Client;
		::GetClientRect(Hwnd, &Client);
		::ClientToScreen(Hwnd, reinterpret_cast<POINT *>(&Client.left));
		::ClientToScreen(Hwnd, reinterpret_cast<POINT *>(&Client.right));
		POINT Cursor;
		platform::ReadCursorPosition(platform::Platform().Input, Cursor); // GetCursorPos (issue #119)
		if (!PtInRect(&Client, Cursor))
			return false;
	}
	UpdateMouseCursor();
	return true;
}

// CMainFrame::OnHostResolved: the name lookup is done (Error 0: the address is there). Since issue #125 the portable
// socket's Poll calls it (HostResolved, below), where WSAAsyncGetHostByName posted WM_USER + 1 to the window.
void OnHostResolved(int Error)
{
	if (Error == 0) {
		if (GetConnectionState() != 1) {
			SetConnectionTimeout(0);
			SetConnectionState(0);
			Disconnect(true);
			char *Text = new char[1024];
			GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Text, ERROR_MESSAGE_SIZE), -1, 1);
			AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Text));
			return;
		}
		SetConnectionTimeout(0);
		SetServerIPAddress(GetResolvedIPAddress());
		SetConnectionState(2);
	} else {
		SetConnectionTimeout(0);
		SetConnectionState(0);
		SetServerError(Error);
	}
	ConnectToLoginServer();
}

// The SDL window's subclass: the messages CMainFrame's message map handled as input go to its handlers; Default()
// there is DefWindowProc (the MFC frame's window procedure's default), so SDL never sees the keys, the buttons or the
// wheel (no SDL key or mouse events, no SDL mouse capture). The keyboard and mouse messages the frame had no handler
// for go to DefWindowProc too, as the frame's. Everything else is SDL's.
LRESULT CALLBACK InputProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam, [[maybe_unused]] UINT_PTR Id,
	[[maybe_unused]] DWORD_PTR Data)
{
	switch (Message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		KeyDown(static_cast<UINT>(WParam), HIWORD(LParam));
		return 0;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		KeyUp(static_cast<UINT>(WParam), HIWORD(LParam));
		return 0;
	case WM_CHAR:
		::DefWindowProcA(Hwnd, Message, WParam, LParam);
		OnChar(static_cast<UINT>(WParam));
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		::DefWindowProcA(Hwnd, Message, WParam, LParam);
		ButtonDown(Hwnd, LParam, Message == WM_LBUTTONDOWN || Message == WM_LBUTTONDBLCLK, Message == WM_LBUTTONDBLCLK);
		return 0;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		::DefWindowProcA(Hwnd, Message, WParam, LParam);
		ButtonUp(Hwnd, LParam, Message == WM_LBUTTONUP);
		return 0;
	case WM_MOUSEWHEEL:
		::DefWindowProcA(Hwnd, Message, WParam, LParam);
		OnMouseWheel(static_cast<short>(HIWORD(WParam)));
		return TRUE;
	case WM_CAPTURECHANGED:
		OnCaptureChanged(Hwnd);
		return ::DefWindowProcA(Hwnd, Message, WParam, LParam);
	case WM_SETCURSOR:
		if (OnSetCursor(Hwnd))
			return TRUE;
		return ::DefWindowProcA(Hwnd, Message, WParam, LParam);
	case WM_TIMER:
		// CMainFrame::OnTimer: the test mode's ticks (with a script the driver dispatches WM_TIMER 1); the loop's own
		// timer does not use WM_TIMER.
		::DefWindowProcA(Hwnd, Message, WParam, LParam);
		TimerEvent();
		return 0;
	case WM_NCHITTEST:
		if (GetViewMode() == 2)
			return HTCLIENT;
		break;
	case WM_NCDESTROY: RemoveWindowSubclass(Hwnd, InputProc, InputSubclassId); break;
	default:
		if ((Message >= WM_KEYFIRST && Message <= WM_KEYLAST) || (Message >= WM_MOUSEFIRST && Message <= WM_MOUSELAST))
			return ::DefWindowProcA(Hwnd, Message, WParam, LParam);
		break;
	}
	return DefSubclassProc(Hwnd, Message, WParam, LParam);
}

// What a message noted for the loop (above): a close answered (OnClose), fullscreen left (OnActivateApp), a display
// change (OnDisplayChange), the window destroyed (OnDestroy and the frame's destructor).
void DestroyFrame();

void HandleNoted()
{
	if (LeaveFullscreen) {
		LeaveFullscreen = false;
		if (Window != nullptr && GetViewMode() == 2)
			SetDisplayMode(1);
	}
	if (DisplayChange) {
		DisplayChange = false;
		if (Window != nullptr)
			OnDisplayChange(DisplayWidth, DisplayHeight);
	}
	if (CloseWanted) {
		CloseWanted = false;
		if (Window != nullptr)
			OnClose();
	}
	if (DestroyWanted) {
		DestroyWanted = false;
		DestroyFrame();
	}
}

// CMainFrame::DestroyWindow (fullscreen mode left first), OnDestroy (the windowed position saved for Patella.ini, the
// timer stopped, the cursors destroyed), the window destroyed, and the frame's destructor (ExitControl).
void DestroyFrame()
{
	if (Window == nullptr)
		return;
	if (GetViewMode() == 2)
		SetDisplayMode(1);
	if (GetViewMode() == 1)
		ReadPlacement();
	SetWindowedMode(Placement.Left, Placement.Top, Placement.Right - Placement.Left, Placement.Bottom - Placement.Top,
		IsMaximized());
	platform::Platform().Input.StopTimer(WindowHandle, 1); // KillTimer (issue #119; the loop's timer, #122)
	DestroyCursor(DefaultCursor);
	DestroyCursor(HandCursor);
	Created = false;
	SDL_DestroyWindow(Window);
	Window = nullptr;
	WindowHandle = nullptr;
	Destroyed = true;
	ExitControl();
}

} // namespace

bool Create(bool Gamemaster, const char *Title, const char *ClassName)
{
	// SDL3's settings for the client (before SDL_Init): its own message pump stays off on Windows (the loop below
	// pumps); no DPI awareness (the original's process is not DPI aware: no manifest; SDL3 would declare per-monitor
	// awareness); no mouse capture of its own (the client takes the capture itself); the class icon is the client's
	// (resource 128, which InitInstance gave the MFC frame's class and WM_SETICON); a fullscreen window is not
	// minimized when it loses the focus (the client leaves fullscreen mode instead, OnActivateApp); Alt+F4 is the
	// client's key (KeyDown).
	SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
	SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "unaware");
	SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
	SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "128");
	SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "128");
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
	SDL_SetHint(SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, "0");
	SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
	SDL_SetMainReady();
	// The window class: InitInstance's (the client's class name, CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW), so
	// that the start's count of the Tibia windows (CountTibiaWindows) finds this window by its class as by its title,
	// and Windows makes the double clicks the client handles.
	if (!SDL_RegisterApp(ClassName, CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, GetModuleHandleA(nullptr))) {
		log::Error("SDL_RegisterApp failed: {}", SDL_GetError());
		return false;
	}
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		log::Error("SDL_Init failed: {}", SDL_GetError());
		return false;
	}
	log::Info("SDL {}.{}.{} (built with {}.{}.{})", SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
		SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()), SDL_MAJOR_VERSION,
		SDL_MINOR_VERSION, SDL_MICRO_VERSION);
	SDL_AddEventWatch(WatchEvent, nullptr);

	// CMainFrame's constructor.
	KeyFlags = 0;
	memset(patella::bridge::Network().PacketHeader.data(), 0, sizeof(patella::bridge::Network().PacketHeader));
	ArrowCursor = LoadCursorA(nullptr, IDC_ARROW);
	SizeCursor = LoadCursorA(nullptr, IDC_SIZENS);
	WaitCursor = LoadCursorA(nullptr, IDC_WAIT);
	DefaultCursor = LoadCursorA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(0x84));
	HandCursor = LoadCursorA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(0x89));
	SetWindowedMode(-1, -1, 0, 0, false);
	InitControl(Gamemaster);
	SetViewMode(1);

	// Create (CFrameWnd::Create with WS_OVERLAPPEDWINDOW: SDL's resizable window), hidden, as the MFC frame is until
	// InitInstance shows it.
	Window = SDL_CreateWindow(Title, 600, 450, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
	if (Window == nullptr) {
		log::Error("SDL_CreateWindow failed: {}", SDL_GetError());
		return false;
	}
	WindowHandle = static_cast<HWND>(
		SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
	if (WindowHandle == nullptr || !SetWindowSubclass(WindowHandle, InputProc, InputSubclassId, 0)) {
		log::Error("the SDL window has no window handle to subclass");
		SDL_DestroyWindow(Window);
		Window = nullptr;
		WindowHandle = nullptr;
		return false;
	}
	SetFrameStyle(0, WS_EX_CLIENTEDGE); // CFrameWnd::PreCreateWindow's
	// OnGetMinMaxInfo: the client area at least 550x450.
	SDL_SetWindowMinimumSize(Window, 550, 450);

	// CMainFrame::OnCreate.
	int ClientWidth = 0;
	int ClientHeight = 0;
	SDL_GetWindowSize(Window, &ClientWidth, &ClientHeight);
	InitMainWindow(ClientWidth, ClientHeight, ExitProgram, SetDisplayMode, OpenURL);
	InitWindow(WindowHandle, ExitProgram, SetDisplayMode, OpenURL);
	Created = true;
	int x, y, Width, Height;
	bool Maximized;
	GetWindowedMode(&x, &y, &Width, &Height, &Maximized);
	int ScreenWidth = 0;
	int ScreenHeight = 0;
	ScreenSize(ScreenWidth, ScreenHeight);
	if (x >= 0 && y >= 0 && x + Width <= ScreenWidth && y + Height <= ScreenHeight)
		MoveWindowTo(x, y, Width, Height);
	else
		SetDefaultWindowPosition();
	platform::Platform().Input.StartTimer(WindowHandle, 1, 100); // SetTimer until #122: the loop's timer
	// The WM_SIZE that CreateWindowEx sends after WM_CREATE: the client gets the window's size once more.
	SDL_GetWindowSize(Window, &ClientWidth, &ClientHeight);
	OnSize(ClientWidth, ClientHeight);
	return true;
}

platform::TWindowHandle Handle()
{
	return WindowHandle;
}

void Show(bool Maximized)
{
	if (Window == nullptr)
		return;
	SDL_ShowWindow(Window);
	if (Maximized)
		SDL_MaximizeWindow(Window);
	SDL_SyncWindow(Window);
	HandleNoted();
}

void Hide()
{
	if (Window != nullptr)
		SDL_HideWindow(Window);
}

int Run(void (*Idle)(long Count))
{
	long IdleCount = 0;
	while (!Destroyed) {
		// Win32 until the MFC removal (issue #127): the thread's messages, as MFC's PumpMessage dispatched them (the
		// test build drops the desktop's input with a script, CPlayerApp::PreTranslateMessage's filter), each followed
		// by what it noted; SDL's own pump is off (SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP).
		MSG Message = {};
		while (!Destroyed && ::PeekMessageA(&Message, nullptr, 0, 0, PM_REMOVE)) {
			if (Message.message == WM_QUIT) {
				SDL_Quit();
				return static_cast<int>(Message.wParam);
			}
#if defined(PATELLA_TEST_MODE)
			if (patella::testmode::DropInput(&Message))
				continue;
#endif
			::TranslateMessage(&Message);
			::DispatchMessageA(&Message);
			IdleCount = 0;
			HandleNoted();
		}
		if (Destroyed)
			break;
		// The rest of SDL's work; its events have been seen by the watch.
		SDL_PumpEvents();
		SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
		HandleNoted();
		if (Destroyed)
			break;
		// The main timer in the loop (issue #122): WM_TIMER came when the queue was otherwise empty.
		if (platform::SdlLoopInput().TimerDue(platform::Platform().Clock.Milliseconds()))
			TimerEvent();
		Idle(IdleCount);
		IdleCount++;
	}
	SDL_Quit();
	return 0;
}

void HostResolved(int Error)
{
	OnHostResolved(Error);
}

// MainFrm.cpp's ExitProgram: closes the client without asking. Win32 until issue #127: a posted WM_CLOSE (SDL's
// window procedure makes it the close request), so that it comes after the messages already queued, as MFC's.
void ExitProgram()
{
	Closing = true;
	if (WindowHandle != nullptr)
		PostMessageA(WindowHandle, WM_CLOSE, 0, 0);
}

// MainFrm.cpp's SetDisplayMode: view mode 2 is fullscreen (SDL's fullscreen desktop window, the original's topmost
// popup window over the whole screen), 1 a normal window at its old placement.
void SetDisplayMode(int ViewMode)
{
	if (Window == nullptr)
		return;
	if (GetViewMode() == 1)
		ReadPlacement();
	SetViewMode(ViewMode);
	if (ViewMode == 2) {
		SetFrameStyle(WS_EX_APPWINDOW | WS_EX_CLIENTEDGE, 0);
		Redraw(false);
		SDL_SetWindowFullscreen(Window, true);
		SDL_SyncWindow(Window);
		if (patella::bridge::GraphicsEngine().curEngine == 2)
			Resize(0, 0);
	} else {
		SetFrameStyle(0, WS_EX_APPWINDOW | WS_EX_CLIENTEDGE);
		Redraw(false);
		SDL_SetWindowFullscreen(Window, false);
		SDL_SyncWindow(Window);
		RestorePlacement();
	}
}

// MainFrm.cpp's OpenURL: leaves fullscreen mode first. ShellExecute until issue #124 (SDL_OpenURL).
void OpenURL(const char *URL)
{
	if (GetViewMode() == 2)
		SetDisplayMode(1);
	ShellExecuteA(nullptr, nullptr, URL, nullptr, nullptr, SW_SHOWNORMAL);
}

// MainFrm.cpp's UpdateMouseCursor: the Windows cursor for the client's mouse cursor (Status.cpp), or none when the
// client draws its own. Win32 until issue #123 (SDL_CreateColorCursor).
void UpdateMouseCursor()
{
	HCURSOR Cursor;
	if (!GetWindowsMousepointer()) {
		Cursor = nullptr;
	} else {
		Cursor = DefaultCursor;
		switch (GetMouseCursor()) {
		case 1: Cursor = ArrowCursor; break;
		case 2: Cursor = SizeCursor; break;
		case 3: Cursor = WaitCursor; break;
		case 4:
		case 5: break;
		case 6: Cursor = HandCursor; break;
		default: return;
		}
	}
	SetCursor(Cursor);
}

} // namespace patella::sdlframe
