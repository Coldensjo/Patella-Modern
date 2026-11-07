// SdlFrame.cpp: the client's main window and event loop on SDL3 (Patella-Modern issue #122, plan M8; SdlFrame.h,
// notes/decisions.md, "#122: the SDL3 window and its event loop"). Not part of the original source.
//
// Each function says which of MainFrm.cpp's (CMainFrame) or MFC's it stands for; the client calls are theirs, in
// their order. What SDL3 does here (the window, its placement and size, the fullscreen switch, its activation, close
// and display events, the loop): SDL_CreateWindow, SDL_SetWindowPosition/Size, SDL_SetWindowFullscreen, the window and
// display events; since issue #123 the input (the key, text, button, wheel and motion events, SDL_CaptureMouse, SDL's
// cursors and mouse position; platform/KeyInput.h has the client's rules). What stays Win32's until its own issue, in
// the "Win32 until ..." parts below: the loss of the mouse capture (WM_CAPTURECHANGED: SDL has no event for it), the
// host lookup's message WM_USER + 1 (the socket, #125), ShellExecute (#124), and on Windows the message pump itself
// (SDL's own is off: the loop dispatches the thread's messages as MFC's did, so the MFC sockets' and the test mode's
// messages are dispatched in the same order).
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
#include "platform/CursorImage.h"
#include "platform/KeyInput.h"
#include "platform/Platform.h"
#include "platform/Win32Platform.h"
#include "types/Cp1252.h"

#define SDL_MAIN_HANDLED // NOLINT(cppcoreguidelines-macro-usage): SDL_main.h's switch; MFC's WinMain is the entry
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <commctrl.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <string>

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

// The client's input state (issue #123: platform/KeyInput.h's rules over SDL's events): CMainFrame::KeyFlags and
// MainFrm.cpp's file-local button flags and wheel rest, with SDL's click counts made into Windows' double clicks.
static_assert(platform::KeyFlagShift == KEY_SHIFT && platform::KeyFlagControl == KEY_CONTROL && platform::KeyFlagAlt == KEY_ALT);
platform::TKeyFlags Keys;
platform::TClickCounter Clicks;
platform::TWheel Wheel;
bool AltGrControl = false;      // the left Control that Windows' AltGr holds (OnKey)
bool LeftButtonPressed = false; // MainFrm.cpp's
bool RightButtonPressed = false;
bool Closing = false;

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
	Keys.Reset();
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

void Input(const SDL_Event &Event, bool Scripted);

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
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
	case SDL_EVENT_TEXT_INPUT:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	case SDL_EVENT_MOUSE_WHEEL:
	case SDL_EVENT_MOUSE_MOTION:
		// Issue #123: the input, as it comes (WM_KEYDOWN, WM_CHAR, WM_LBUTTONDOWN ... in SDL's window procedure); the
		// test build drops the desktop's with a script (IInput::IgnoreDesktopInput, as its message filter DropInput).
		if (!platform::Platform().Input.IgnoreDesktopInput())
			Input(*Event, false);
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

// Issue #123: the cursors (MainFrm.cpp's: CMainFrame's constructor loaded them, UpdateMouseCursor sets them) as SDL's.
SDL_Cursor *HandCursor = nullptr;    // cursor resource 0x89 (mouse cursor 6)
SDL_Cursor *SizeCursor = nullptr;    // IDC_SIZENS (mouse cursor 2)
SDL_Cursor *WaitCursor = nullptr;    // IDC_WAIT (mouse cursor 3)
SDL_Cursor *ArrowCursor = nullptr;   // IDC_ARROW (mouse cursor 1)
SDL_Cursor *DefaultCursor = nullptr; // cursor resource 0x84 (the others)

// LoadCursorA(GetModuleHandleA(nullptr), Id) as an SDL cursor: the group's image for the system's cursor size (as
// LoadCursor picks it: LookupIconIdFromDirectoryEx), a monochrome RT_CURSOR resource, decoded (platform/CursorImage.h)
// into SDL_CreateCursor's bitmaps (which make the colour cursor, SDL_CreateColorCursor, with Windows' inverted
// pixels). Null where the resource is missing or not monochrome, as LoadCursor's null cursor.
SDL_Cursor *LoadClientCursor(int Id)
{
	const HMODULE Module = GetModuleHandleA(nullptr);
	HRSRC Group = FindResourceA(Module, MAKEINTRESOURCEA(Id), MAKEINTRESOURCEA(12)); // RT_GROUP_CURSOR
	HGLOBAL GroupData = Group != nullptr ? LoadResource(Module, Group) : nullptr;
	auto *Directory = GroupData != nullptr ? static_cast<PBYTE>(LockResource(GroupData)) : nullptr;
	const int ImageId = Directory != nullptr ? LookupIconIdFromDirectoryEx(Directory, FALSE, 0, 0, LR_DEFAULTCOLOR) : 0;
	HRSRC Image = ImageId != 0 ? FindResourceA(Module, MAKEINTRESOURCEA(ImageId), MAKEINTRESOURCEA(1)) : nullptr; // RT_CURSOR
	HGLOBAL ImageData = Image != nullptr ? LoadResource(Module, Image) : nullptr;
	const auto *Bytes = ImageData != nullptr ? static_cast<const std::uint8_t *>(LockResource(ImageData)) : nullptr;
	if (Bytes == nullptr) {
		log::Warn("cursor resource {} not found", Id);
		return nullptr;
	}
	const std::optional<platform::TMonochromeCursor> Decoded =
		platform::DecodeMonochromeCursor(std::span<const std::uint8_t>(Bytes, SizeofResource(Module, Image)));
	if (!Decoded) {
		log::Warn("cursor resource {} is not a monochrome cursor", Id);
		return nullptr;
	}
	SDL_Cursor *Cursor = SDL_CreateCursor(Decoded->Data.data(), Decoded->Mask.data(), Decoded->Width, Decoded->Height,
		Decoded->HotX, Decoded->HotY);
	if (Cursor == nullptr)
		log::Warn("SDL_CreateCursor of cursor resource {} failed: {}", Id, SDL_GetError());
	return Cursor;
}

constexpr UINT_PTR InputSubclassId = 122;

// SDL's scan code as one of the keys the client tells apart: the position on the keyboard, as Windows' scan code; the
// virtual keys TranslateKey reads follow from it (the keypad's with NumLock: platform::PadNavigates).
platform::TKey KeyOf(SDL_Scancode Code)
{
	using platform::TKey;
	if (Code >= SDL_SCANCODE_F1 && Code <= SDL_SCANCODE_F12)
		return static_cast<TKey>(static_cast<int>(TKey::F1) + (Code - SDL_SCANCODE_F1));
	switch (Code) {
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT: return TKey::Shift;
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL: return TKey::Control;
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT: return TKey::Alt;
	case SDL_SCANCODE_PAGEUP: return TKey::PageUp;
	case SDL_SCANCODE_PAGEDOWN: return TKey::PageDown;
	case SDL_SCANCODE_END: return TKey::End;
	case SDL_SCANCODE_HOME: return TKey::Home;
	case SDL_SCANCODE_LEFT: return TKey::Left;
	case SDL_SCANCODE_UP: return TKey::Up;
	case SDL_SCANCODE_RIGHT: return TKey::Right;
	case SDL_SCANCODE_DOWN: return TKey::Down;
	case SDL_SCANCODE_INSERT: return TKey::Insert;
	case SDL_SCANCODE_DELETE: return TKey::Delete;
	case SDL_SCANCODE_KP_9: return TKey::PadPageUp;
	case SDL_SCANCODE_KP_3: return TKey::PadPageDown;
	case SDL_SCANCODE_KP_1: return TKey::PadEnd;
	case SDL_SCANCODE_KP_7: return TKey::PadHome;
	case SDL_SCANCODE_KP_4: return TKey::PadLeft;
	case SDL_SCANCODE_KP_8: return TKey::PadUp;
	case SDL_SCANCODE_KP_6: return TKey::PadRight;
	case SDL_SCANCODE_KP_2: return TKey::PadDown;
	case SDL_SCANCODE_KP_0: return TKey::PadInsert;
	case SDL_SCANCODE_KP_PERIOD: return TKey::PadDelete;
	case SDL_SCANCODE_RETURN: return TKey::Return;
	case SDL_SCANCODE_KP_ENTER: return TKey::PadEnter;
	case SDL_SCANCODE_ESCAPE: return TKey::Escape;
	case SDL_SCANCODE_TAB: return TKey::Tab;
	case SDL_SCANCODE_BACKSPACE: return TKey::Backspace;
	default: return TKey::Other;
	}
}

// An SDL key event as the client's key: the key, its letter in the active layout (SDL's key code without modifiers:
// with SDL's default "latin_letters" the Latin letter of a letter key in every layout, as Windows' virtual keys A-Z),
// and SDL's modifier state at the key.
platform::TKeyEvent DescribeKey(const SDL_KeyboardEvent &Key)
{
	platform::TKeyEvent Event;
	Event.Key = KeyOf(Key.scancode);
	const SDL_Keycode Letter = SDL_GetKeyFromScancode(Key.scancode, SDL_KMOD_NONE, true);
	if (Letter >= SDLK_A && Letter <= SDLK_Z)
		Event.Letter = static_cast<char>('a' + (Letter - SDLK_A));
	Event.NumLock = (Key.mod & SDL_KMOD_NUM) != 0;
	Event.Shift = (Key.mod & SDL_KMOD_SHIFT) != 0;
	Event.Control = (Key.mod & SDL_KMOD_CTRL) != 0;
	Event.Alt = (Key.mod & SDL_KMOD_ALT) != 0;
	return Event;
}

// CMainFrame::OnChar: a character of the client's code page.
void OnChar(unsigned int Char)
{
	const int Code = Keys.Character(Char);
	if (Code >= 0)
		KeyPress(Code);
}

// CMainFrame::KeyDown with its key's character: a modifier remembered, Alt+F4 closes, the other translated keys go to
// KeyPress; then the control character TranslateMessage makes of the key (WM_CHAR right after the WM_KEYDOWN), which
// SDL's text input does not deliver (it has the printable characters only: OnText). A scripted key's characters are
// the script's own events.
void KeyDown(const platform::TKeyEvent &Key, bool Scripted)
{
	const platform::TKeyAction Action = Keys.Down(Key);
	if (Action.Kind == platform::TKeyAction::TKind::Close) {
		OnClose();
		return;
	}
	if (Action.Kind == platform::TKeyAction::TKind::Press)
		KeyPress(Action.Code);
	if (!Scripted) {
		const int Char = platform::KeyCharacter(Key);
		if (Char >= 0)
			OnChar(static_cast<unsigned int>(Char));
	}
}

// SDL_EVENT_KEY_DOWN and SDL_EVENT_KEY_UP (also SDL's repeats, which are Windows' repeated WM_KEYDOWNs). Windows' AltGr
// (the right Alt of a layout that has one) holds a left Control with it, a WM_KEYDOWN of its own that the original's
// KeyDown counted; SDL leaves that Control out (SDL_windowsevents.c SkipAltGrLeftControl). So a right Alt that comes
// while Windows' keyboard state has the left Control down and SDL's has not is given that Control first, as the
// original saw it, and the Control goes up with the Alt.
void OnKey(const SDL_KeyboardEvent &Event, bool Scripted)
{
	const platform::TKeyEvent Key = DescribeKey(Event);
	const bool RightAlt = Event.scancode == SDL_SCANCODE_RALT;
	if (Event.down) {
		if (RightAlt && !Scripted && (GetKeyState(VK_LCONTROL) & 0x8000) != 0 && (Event.mod & SDL_KMOD_LCTRL) == 0) {
			platform::TKeyEvent Control = Key;
			Control.Key = platform::TKey::Control;
			AltGrControl = true;
			KeyDown(Control, Scripted);
		}
		KeyDown(Key, Scripted);
		return;
	}
	Keys.Up(Key);
	if (RightAlt && AltGrControl) {
		platform::TKeyEvent Control = Key;
		Control.Key = platform::TKey::Control;
		Keys.Up(Control);
		AltGrControl = false;
	}
}

// SDL_EVENT_TEXT_INPUT: the characters of the typed text (WM_CHAR, which an ANSI window got in the code page), each
// converted to the client's Windows-1252 (issue #112; a character it does not have is '?', as Windows' conversion for
// the ANSI window made it on this code page).
void OnText(const char *Text)
{
	if (Text == nullptr)
		return;
	for (const char Byte : text::Utf8ToCp1252(Text))
		OnChar(static_cast<unsigned char>(Byte));
}

// The window's point of a mouse event (window coordinates; negative or past the size while the mouse is captured): in
// fullscreen mode a screen point (ClientToScreen), as the button handlers of CMainFrame passed them.
POINT EventPoint(float X, float Y)
{
	int x = static_cast<int>(std::floor(X));
	int y = static_cast<int>(std::floor(Y));
	if (GetViewMode() == 2)
		ClientToScreen(x, y);
	return POINT{x, y};
}

bool InsideClient(float X, float Y)
{
	int Width = 0;
	int Height = 0;
	SDL_GetWindowSize(Window, &Width, &Height);
	return X >= 0.0F && Y >= 0.0F && X < static_cast<float>(Width) && Y < static_cast<float>(Height);
}

// CMainFrame::OnSetCursor's moment: Windows sends WM_SETCURSOR as the mouse moves or a button goes down over the
// window, when no window has the capture; in the client area (always in fullscreen mode) the client sets its cursor.
// SDL sets the cursor UpdateMouseCursor chose at each WM_SETCURSOR itself; this keeps the original's updates in between
// (a change of the client's cursor or of its Windows-pointer option, read again at the next move).
void OnSetCursor(float X, float Y)
{
	if (LeftButtonPressed || RightButtonPressed)
		return;
	if (GetViewMode() != 2 && !InsideClient(X, Y))
		return;
	UpdateMouseCursor();
}

// The button handlers of CMainFrame: the first button down takes the mouse (SDL_CaptureMouse, SetCapture), the last
// one up releases it. A left double click is LeftButtonDoubleClick; a right double click is a click (OnRButtonDblClk).
void ButtonDown(const POINT &Point, bool Left, bool Double)
{
	if (Left) {
		if (Double)
			LeftButtonDoubleClick(Point.x, Point.y, Keys.Flags());
		else
			LeftButtonDown(Point.x, Point.y, Keys.Flags());
		LeftButtonPressed = true;
		if (!RightButtonPressed)
			SDL_CaptureMouse(true);
	} else {
		RightButtonDown(Point.x, Point.y, Keys.Flags());
		RightButtonPressed = true;
		if (!LeftButtonPressed)
			SDL_CaptureMouse(true);
	}
}

void ButtonUp(const POINT &Point, bool Left)
{
	if (Left) {
		if (LeftButtonPressed) {
			LeftButtonUp(Point.x, Point.y, Keys.Flags());
			LeftButtonPressed = false;
			if (!RightButtonPressed)
				SDL_CaptureMouse(false);
		}
	} else if (RightButtonPressed) {
		RightButtonUp(Point.x, Point.y, Keys.Flags());
		RightButtonPressed = false;
		if (!LeftButtonPressed)
			SDL_CaptureMouse(false);
	}
}

// SDL_EVENT_MOUSE_BUTTON_DOWN and _UP. The point is SDL's mouse position at the button (the last motion's, which
// Windows' WM_MOUSEMOVE keeps where the button message's point is). Whether a press is a double click: Windows' rule
// over SDL's click counts (platform::TClickCounter; every button's presses count, as every button's messages reset
// Windows' double click); a scripted press is the script's (clicks 2 for WM_xBUTTONDBLCLK). The middle and side
// buttons have no handler, as in the original.
void OnButton(const SDL_MouseButtonEvent &Event, bool Scripted)
{
	if (Event.down) {
		const bool Double = Scripted ? Event.clicks >= 2 : Clicks.IsDouble(Event.button, Event.clicks);
		if (Event.button != SDL_BUTTON_LEFT && Event.button != SDL_BUTTON_RIGHT)
			return;
		if (!Scripted)
			OnSetCursor(Event.x, Event.y);
		ButtonDown(EventPoint(Event.x, Event.y), Event.button == SDL_BUTTON_LEFT, Double);
	} else if (Event.button == SDL_BUTTON_LEFT || Event.button == SDL_BUTTON_RIGHT) {
		ButtonUp(EventPoint(Event.x, Event.y), Event.button == SDL_BUTTON_LEFT);
	}
}

// CMainFrame::OnMouseWheel: one MouseWheel call per notch (WM_MOUSEWHEEL's 120; SDL's wheel distance is notches) at
// the client's mouse position. The horizontal wheel has no handler.
void OnWheel(const SDL_MouseWheelEvent &Event)
{
	float Distance = Event.y * 120.0F;
	if (Event.direction == SDL_MOUSEWHEEL_FLIPPED)
		Distance = -Distance;
	const int Notches = Wheel.Notches(static_cast<int>(std::lround(Distance)));
	for (int i = 0; i < std::abs(Notches); i++) {
		// The original's order: a call's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
		const int MouseY = GetMouseY();
		MouseWheel(GetMouseX(), MouseY, Notches > 0 ? 1 : 2);
	}
}

// Every input event of the window (the event watch's and the test mode's scripted ones), once the client has its
// window (InitMainWindow).
void Input(const SDL_Event &Event, bool Scripted)
{
	if (!Created)
		return;
	switch (Event.type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: OnKey(Event.key, Scripted); break;
	case SDL_EVENT_TEXT_INPUT: OnText(Event.text.text); break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: OnButton(Event.button, Scripted); break;
	case SDL_EVENT_MOUSE_WHEEL: OnWheel(Event.wheel); break;
	case SDL_EVENT_MOUSE_MOTION:
		if (!Scripted)
			OnSetCursor(Event.motion.x, Event.motion.y);
		break;
	default: break;
	}
}

// CMainFrame::OnCaptureChanged: another window took the mouse; the held buttons are released at the cursor. SDL has no
// event for a lost capture, so on Windows it is still the window's WM_CAPTURECHANGED (the subclass below), which comes
// also when the client releases the capture itself, after it has cleared its button flags (nothing to do then).
// SDL's capture state is then set free too (SDL_CaptureMouse(false)), so that the next press takes the mouse again.
void OnCaptureChanged()
{
	if (platform::Platform().Input.IgnoreCaptureChange())
		return;
	POINT Point;
	platform::ReadCursorPosition(platform::Platform().Input, Point); // GetCursorPos (issue #119; SDL's, #123)
	if (GetViewMode() == 1) {
		int x = Point.x;
		int y = Point.y;
		ScreenToClient(x, y);
		Point = POINT{x, y};
	}
	if (LeftButtonPressed) {
		LeftButtonUp(Point.x, Point.y, Keys.Flags());
		LeftButtonPressed = false;
	}
	if (RightButtonPressed) {
		RightButtonUp(Point.x, Point.y, Keys.Flags());
		RightButtonPressed = false;
	}
	SDL_CaptureMouse(false);
}

// CMainFrame::OnHostResolved (the socket's message until issue #125): WSAAsyncGetHostByName finished.
void OnHostResolved(LPARAM LParam)
{
	if (WSAGETASYNCERROR(LParam) == 0) {
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
		SetServerError(WSAGETASYNCERROR(LParam));
	}
	ConnectToLoginServer();
}

// The SDL window's subclass, for what SDL3 has no event for: the loss of the mouse capture (WM_CAPTURECHANGED, then
// SDL's own handling of it), the test mode's main timer ticks (WM_TIMER 1: with a script the driver dispatches them),
// the host lookup's message (WM_USER + 1, the socket until issue #125) and the fullscreen window's hit test. The keys,
// the buttons, the wheel and the cursor are SDL's (issue #123).
LRESULT CALLBACK InputProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam, [[maybe_unused]] UINT_PTR Id,
	[[maybe_unused]] DWORD_PTR Data)
{
	switch (Message) {
	case WM_CAPTURECHANGED: OnCaptureChanged(); break;
	case WM_TIMER:
		// CMainFrame::OnTimer: the test mode's ticks (with a script the driver dispatches WM_TIMER 1); the loop's own
		// timer does not use WM_TIMER.
		::DefWindowProcA(Hwnd, Message, WParam, LParam);
		TimerEvent();
		return 0;
	case WM_USER + 1:
		OnHostResolved(LParam);
		return 0;
	case WM_NCHITTEST:
		if (GetViewMode() == 2)
			return HTCLIENT;
		break;
	case WM_NCDESTROY: RemoveWindowSubclass(Hwnd, InputProc, InputSubclassId); break;
	default: break;
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
	// The cursors (OnDestroy destroyed the two of the client's resources; issue #123: all five are SDL's).
	for (SDL_Cursor **Cursor : {&DefaultCursor, &HandCursor, &ArrowCursor, &SizeCursor, &WaitCursor}) {
		SDL_DestroyCursor(*Cursor);
		*Cursor = nullptr;
	}
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
	// Issue #123, the input as Windows gave it to the MFC frame: the click that activates the window is a click
	// (Windows' WM_MOUSEACTIVATE default; SDL would drop it); two presses are a double click within Windows' double-click
	// rectangle (SM_CXDOUBLECLK around the first: SDL's radius is half of it; its time is GetDoubleClickTime already);
	// the system menu's keys (Alt+Space: WM_SYSCHAR, SC_KEYMENU) work, as DefWindowProc gave them; the frame keeps
	// Windows' cursors while the client's is hidden.
	SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
	SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "unaware");
	SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
	const std::string ClickRadius = std::to_string(GetSystemMetrics(SM_CXDOUBLECLK) / 2);
	SDL_SetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS, ClickRadius.c_str());
	SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS, "1");
	SDL_SetHint(SDL_HINT_WINDOW_FRAME_USABLE_WHILE_CURSOR_HIDDEN, "1");
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
	Keys.Reset();
	memset(patella::bridge::Network().PacketHeader.data(), 0, sizeof(patella::bridge::Network().PacketHeader));
	// The cursors (issue #123: SDL's system cursors for IDC_ARROW, IDC_SIZENS and IDC_WAIT, which SDL loads as those on
	// Windows; the client's two from its resources, LoadClientCursor).
	ArrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
	SizeCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
	WaitCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	DefaultCursor = LoadClientCursor(0x84);
	HandCursor = LoadClientCursor(0x89);
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
	// Issue #123: the typed characters as SDL_EVENT_TEXT_INPUT (WM_CHAR, the IME's results, dead keys combined), for as
	// long as the window lives.
	if (!SDL_StartTextInput(Window))
		log::Warn("SDL_StartTextInput failed: {}", SDL_GetError());

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

// MainFrm.cpp's UpdateMouseCursor: the cursor for the client's mouse cursor (Status.cpp), or none when the client
// draws its own (SetCursor(NULL): SDL_HideCursor). Issue #123: SDL's cursors, which SDL sets in the client area (its
// WM_SETCURSOR); a cursor that could not be made is none, as SetCursor of LoadCursor's null cursor.
void UpdateMouseCursor()
{
	SDL_Cursor *Cursor = nullptr;
	if (GetWindowsMousepointer()) {
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
	if (Cursor == nullptr) {
		SDL_HideCursor();
		return;
	}
	SDL_SetCursor(Cursor);
	SDL_ShowCursor();
}

void ScreenToClient(int &X, int &Y)
{
	if (Window == nullptr)
		return;
	int Left = 0;
	int Top = 0;
	SDL_GetWindowPosition(Window, &Left, &Top);
	X -= Left;
	Y -= Top;
}

void ClientToScreen(int &X, int &Y)
{
	if (Window == nullptr)
		return;
	int Left = 0;
	int Top = 0;
	SDL_GetWindowPosition(Window, &Left, &Top);
	X += Left;
	Y += Top;
}

void ScriptedInput(const SDL_Event &Event)
{
	Input(Event, true);
}

} // namespace patella::sdlframe
