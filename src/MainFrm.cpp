// MainFrm.cpp: reconstructed from Tibia.exe 7.72 (0x4a8200-0x4abec8).
//
// The frame window of the client. Source order follows the catch lines;
// functions without line evidence stay where the binary has them: the
// message map and TranslateKey at the start (the gap after Inventory.cpp;
// OutputErrorInformations, 0x4a8360, which wrote the last packet and the
// player's position to error.txt, is gone since Patella-Modern issue #74),
// ExitProgram after the destructor, AddFrameSize before OnSize, KeyUp and
// the OnKeyDown/OnKeyUp handlers after KeyDown.
// The compiler placed SetDefaultWindowPosition (line 375), OnCreate (273)
// and OnDisplayChange (295) after the key handlers.
//
// In the original every handler was wrapped in a catch wrapper (common.h); since issue #78
// the functions here have none, and the try blocks and catch lines the
// comments name are the original's. In ExitProgram, AddFrameSize and KeyUp
// the binary has the unwind states but no try block: they call only C
// functions (or TranslateKey, which has no exception frame), so the
// compiler removed the handlers.

#include "MainFrm.h"
#include "Control.h"
#include "GUI.h"
#include "MainWindow.h"
#include "Network.h"
#include "Player.h"
#include "Preferences.h"
#include "Status.h"
#include "Surface.h"
#include "Text.h" // IWYU pragma: keep (the object's own copy of the text table, see Text.h)
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>

// Data, in .bss order.
std::array<uint8, 16> PacketHeader; // 0x5d1644, written by Network.cpp
CMainFrame *MainFrame;              // 0x5d1654

namespace {

HCURSOR HandCursor;      // 0x5d1658, cursor resource 0x89 (mouse cursor 6)
HCURSOR SizeCursor;      // 0x5d165c, IDC_SIZENS (mouse cursor 2)
HCURSOR WaitCursor;      // 0x5d1660, IDC_WAIT (mouse cursor 3)
HCURSOR ArrowCursor;     // 0x5d1664, IDC_ARROW (mouse cursor 1)
HCURSOR DefaultCursor;   // 0x5d1668, cursor resource 0x84 (the others)
bool LeftButtonPressed;  // 0x5d166c
bool RightButtonPressed; // 0x5d166d
bool Closing;            // 0x5d166f, CloseRequest is not asked any more

} // namespace

bool DisplayChanged; // 0x5d1670, set by OnDisplayChange; SurfaceGL.cpp's Init reads and clears it

namespace {

// At 0x5d1674, constructed by the static initialiser 0x559f20 as TCallback(NULL, 0) (no atexit) and
// never used, like the window files' (issue #24).
TCallback NoCallback(nullptr, 0);
TFixedString<32> ClassName; // 0x5d167c
int WheelDelta;             // 0x5d169c, the remainder below one notch (120)

} // namespace


// The arrays std::arrays and TFixedStrings (issue #60). CMainFrame derives from MFC's CFrameWnd, whose size is
// the MFC's or the shim's (difftest, the golden tests): KeyStates' place among CMainFrame's own members.
static_assert(sizeof(PacketHeader) == 16 && sizeof(ClassName) == 32, "PacketHeader and ClassName");
#if PATELLA_ORIGINAL_LAYOUT
static_assert(offsetof(CMainFrame, KeyStates) == offsetof(CMainFrame, Placement) + sizeof(WINDOWPLACEMENT)
		&& offsetof(CMainFrame, KeyFlags) == offsetof(CMainFrame, KeyStates) + 0x100,
	"CMainFrame's KeyStates");
#endif
// 0x4a8200. The handlers that share an address in the table (OnKeyDown
// and OnSysKeyDown, OnKeyUp and OnSysKeyUp) have identical code that the
// linker folded.
BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
ON_WM_CREATE()
ON_WM_ACTIVATEAPP()
ON_WM_SETCURSOR()
ON_WM_KEYDOWN()
ON_WM_KEYUP()
ON_WM_SYSKEYDOWN()
ON_WM_SYSKEYUP()
ON_WM_SIZE()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
ON_WM_RBUTTONUP()
ON_WM_RBUTTONDBLCLK()
ON_WM_MOUSEWHEEL()
ON_WM_CLOSE()
ON_WM_DESTROY()
ON_WM_TIMER()
ON_WM_CHAR()
ON_WM_LBUTTONDBLCLK()
ON_WM_GETMINMAXINFO()
ON_WM_NCHITTEST()
ON_WM_CAPTURECHANGED()
ON_MESSAGE(WM_DISPLAYCHANGE, OnDisplayChange)
ON_MESSAGE(WM_USER + 1, OnHostResolved)
END_MESSAGE_MAP()

// 0x4a8210. A virtual key to the client's key code: the modifiers, the
// cursor block (bit 8 of nFlags: the extended keys, not the number pad),
// F1-F12 (0x100-0x10b) and, with Alt held, Return (Alt+Return).
int CMainFrame::TranslateKey(UINT nChar, [[maybe_unused]] UINT nRepCnt, UINT nFlags) const
{
	const bool Extended = (nFlags & 0x100) == 0x100;
	switch (nChar) {
	case VK_SHIFT:
		return KEY_SHIFT;
	case VK_CONTROL:
		return KEY_CONTROL;
	case VK_MENU:
		return KEY_ALT;
	case VK_PRIOR:
		return Extended ? 0x114 : 0x119;
	case VK_NEXT:
		return Extended ? 0x115 : 0x117;
	case VK_END:
		return Extended ? 0x113 : 0x116;
	case VK_HOME:
		return Extended ? 0x112 : 0x118;
	case VK_LEFT:
		return 0x10d;
	case VK_UP:
		return 0x10c;
	case VK_RIGHT:
		return 0x10e;
	case VK_DOWN:
		return 0x10f;
	case VK_INSERT:
		return 0x110;
	case VK_DELETE:
		return 0x111;
	}
	if (nChar >= VK_F1 && nChar <= VK_F12)
		return nChar + 0x90;
	if ((KeyFlags & KEY_ALT) != 0 && nChar == VK_RETURN)
		return VK_RETURN;
	return 0;
}

// 0x4a8520, line 112
CMainFrame::CMainFrame(bool Gamemaster)
{
	MainFrame = this;
	KeyFlags = 0;
	memset(KeyStates.data(), 0, sizeof(KeyStates));
	memset(PacketHeader.data(), 0, sizeof(PacketHeader));
	ArrowCursor = LoadCursorA(nullptr, IDC_ARROW);
	SizeCursor = LoadCursorA(nullptr, IDC_SIZENS);
	WaitCursor = LoadCursorA(nullptr, IDC_WAIT);
	DefaultCursor = LoadCursorA(AfxGetInstanceHandle(), MAKEINTRESOURCEA(0x84));
	HandCursor = LoadCursorA(AfxGetInstanceHandle(), MAKEINTRESOURCEA(0x89));
	SetWindowedMode(-1, -1, 0, 0, false);
	InitControl(Gamemaster);
	SetViewMode(1);
	MouseCursor = GetMouseCursor();
}

// 0x4a8760, line 123. The original's handlers rethrew out of the destructor.
// Unlike TComponent's, this one cannot be noexcept(false): CFrameWnd's
// destructor (MFC) is noexcept and an override may not throw more (C2694). An
// exception from ExitControl ends the program here, where the original's goes
// on to the caller of the destructor (issue #34). The warning C4297 that the
// rethrowing handlers caused went with them (issue #78).
CMainFrame::~CMainFrame()
{
	ExitControl();
}

// 0x4a88f0. Closes the client without asking (Control.cpp's exit callback).
void ExitProgram()
{
	Closing = true;
	PostMessageA(MainFrame->m_hWnd, WM_CLOSE, 0, 0);
}

// 0x4a8940, CloseProgram (line 149: ExitProgram, and CPlayerApp::Run starting patch.exe afterwards), is gone with
// the patcher since Patella-Modern issue #75 (decision D9).

// 0x4a8af0, line 181. View mode 2 is fullscreen: a topmost popup window over
// the whole screen; 1 is a normal window at its old placement.
void SetDisplayMode(int ViewMode)
{
	if (GetViewMode() == 1)
		MainFrame->GetWindowPlacement(&MainFrame->Placement);
	SetViewMode(ViewMode);
	if (ViewMode == 2) {
		MainFrame->ModifyStyle(WS_MAXIMIZE | WS_CAPTION | WS_THICKFRAME, WS_POPUP, 0);
		MainFrame->ModifyStyleEx(WS_EX_APPWINDOW | WS_EX_CLIENTEDGE, 0, 0);
		Redraw(false);
		MainFrame->SetWindowPos(&CWnd::wndTopMost, 0, 0, GetSystemMetrics(SM_CXSCREEN),
			GetSystemMetrics(SM_CYSCREEN), 0);
		if (curEngine == 2)
			Resize(0, 0);
	} else {
		MainFrame->ModifyStyle(WS_POPUP, WS_CAPTION | WS_THICKFRAME, 0);
		MainFrame->ModifyStyleEx(0, WS_EX_APPWINDOW | WS_EX_CLIENTEDGE, 0);
		Redraw(false);
		MainFrame->SetWindowPlacement(&MainFrame->Placement);
		MainFrame->SetWindowPos(&CWnd::wndNoTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
	}
}

// 0x4a8d80, line 193. Leaves fullscreen mode first.
void OpenURL(const char *URL)
{
	if (GetViewMode() == 2)
		SetDisplayMode(1);
	ShellExecuteA(nullptr, nullptr, URL, nullptr, nullptr, SW_SHOWNORMAL);
}

// 0x4a8f30, line 244. The window class registered by InitInstance.
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT &cs)
{
	GetWindowClassName(ClassName);
	cs.lpszClass = ClassName;
	CFrameWnd::PreCreateWindow(cs);
	return TRUE;
}

// 0x4abbc0, line 273. Creates the main game window (MainWindow.cpp), hands
// the callbacks to Control.cpp and restores the window position when it
// fits on the screen.
int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	RECT Client;
	::GetClientRect(m_hWnd, &Client);
	InitMainWindow(Client.right, Client.bottom, ExitProgram, SetDisplayMode, OpenURL);
	InitWindow(m_hWnd, ExitProgram, SetDisplayMode, OpenURL);
	int x, y, Width, Height;
	bool Maximized;
	GetWindowedMode(&x, &y, &Width, &Height, &Maximized);
	if (x >= 0 && y >= 0 && x + Width <= GetSystemMetrics(SM_CXSCREEN)
		&& y + Height <= GetSystemMetrics(SM_CYSCREEN))
		MoveWindow(x, y, Width, Height, TRUE);
	else
		SetDefaultWindowPosition();
	PATELLA_SET_MAIN_TIMER(m_hWnd, 1, 100, nullptr);
	return 0;
}

// 0x4abe40, line 295. A screen smaller than the window in windowed mode
// moves it back to the default position.
LRESULT CMainFrame::OnDisplayChange([[maybe_unused]] WPARAM wParam, LPARAM lParam)
{
	if (GetViewMode() == 1) {
		RECT Rect;
		::GetWindowRect(m_hWnd, &Rect);
		if ((int)LOWORD(lParam) < Rect.right - Rect.left || (int)HIWORD(lParam) < Rect.bottom - Rect.top)
			SetDefaultWindowPosition();
		DisplayChanged = true;
	}
	return 0;
}

// 0x4a90e0, line 335. WSAAsyncGetHostByName finished (Network.cpp gives the
// window to it): connect to the resolved address, or report the error.
LRESULT CMainFrame::OnHostResolved([[maybe_unused]] WPARAM wParam, LPARAM lParam)
{
	if (WSAGETASYNCERROR(lParam) == 0) {
		if (GetConnectionState() != 1) {
			SetConnectionTimeout(0);
			SetConnectionState(0);
			Disconnect(true);
			char *Text = new char[1024];
			GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Text, ERROR_MESSAGE_SIZE), -1, 1);
			AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Text));
			return 0;
		}
		SetConnectionTimeout(0);
		SetServerIPAddress(GetResolvedIPAddress());
		SetConnectionState(2);
	} else {
		SetConnectionTimeout(0);
		SetConnectionState(0);
		SetServerError(WSAGETASYNCERROR(lParam));
	}
	ConnectToLoginServer();
	return 0;
}

// 0x4a9310, line 356. Forgets the held modifiers and leaves fullscreen mode
// whenever the activation changes (both ways).
void CMainFrame::OnActivateApp([[maybe_unused]] BOOL bActive, [[maybe_unused]] DWORD dwThreadID)
{
	KeyFlags = 0;
	if (GetViewMode() == 2)
		SetDisplayMode(1);
}

// 0x4ab9d0, line 375. 600x450 client area, centred.
void CMainFrame::SetDefaultWindowPosition()
{
	int Width = 600;
	int Height = 450;
	AddFrameSize(&Width, &Height);
	const int x = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
	const int y = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
	SetWindowedMode(x, y, Width, Height, false);
	MoveWindow(x, y, Width, Height, TRUE);
}

// 0x4a94b0. From a client size to the window size.
void CMainFrame::AddFrameSize(int *Width, int *Height)
{
	RECT Client, Window;
	::GetClientRect(m_hWnd, &Client);
	::GetWindowRect(m_hWnd, &Window);
	*Width += Client.left - Client.right - Window.left + Window.right;
	*Height += Client.top - Client.bottom - Window.top + Window.bottom;
}

// 0x4a9540, line 414. Keeps the client area at least 550x450.
void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	if (!::IsIconic(m_hWnd) && (cx < 550 || cy < 450)) {
		if (cx < 550)
			cx = 550;
		if (cy < 450)
			cy = 450;
		Resize(cx, cy);
		AddFrameSize(&cx, &cy);
		RECT Rect;
		::GetWindowRect(m_hWnd, &Rect);
		MoveWindow(Rect.left, Rect.top, cx, cy, TRUE);
	} else {
		const int Width = cx;
		const int Height = cy;
		CFrameWnd::OnSize(nType, cx, cy);
		if (!::IsIconic(m_hWnd))
			Resize(Width, Height);
	}
}

// 0x4a9790, line 430. The smallest window: a client area of 550x450.
void CMainFrame::OnGetMinMaxInfo(MINMAXINFO *lpMMI)
{
	Default();
	int Width = 550;
	int Height = 450;
	AddFrameSize(&Width, &Height);
	lpMMI->ptMinTrackSize.y = Height;
	lpMMI->ptMinTrackSize.x = Width;
}

// 0x4a9950, line 441. In fullscreen mode everything is client area.
LRESULT CMainFrame::OnNcHitTest([[maybe_unused]] CPoint point)
{
	if (GetViewMode() == 2)
		return HTCLIENT;
	return Default();
}

// 0x4a9b00, line 454
void CMainFrame::OnTimer([[maybe_unused]] UINT_PTR nIDEvent)
{
	Default();
	TimerEvent();
}

// 0x4a9c90, line 466. Control.cpp may refuse (logout warning in the game).
void CMainFrame::OnClose()
{
	if (!Closing && !CloseRequest())
		return;
	CFrameWnd::OnClose();
}

// 0x4a9e30, line 481
BOOL CMainFrame::DestroyWindow()
{
	if (GetViewMode() == 2)
		SetDisplayMode(1);
	return CWnd::DestroyWindow();
}

// 0x4a9fd0, line 500. Saves the windowed position for Tibia.cfg.
void CMainFrame::OnDestroy()
{
	if (GetViewMode() == 1)
		GetWindowPlacement(&Placement);
	const RECT &Normal = Placement.rcNormalPosition;
	SetWindowedMode(Normal.left, Normal.top, Normal.right - Normal.left, Normal.bottom - Normal.top,
		::IsZoomed(m_hWnd) != 0);
	CFrameWnd::OnDestroy();
	::KillTimer(m_hWnd, 1);
	DestroyCursor(DefaultCursor);
	DestroyCursor(HandCursor);
}

// 0x4aa1d0, line 523. The Windows cursor for the client's mouse cursor
// (Status.cpp), or none when the client draws its own.
void UpdateMouseCursor()
{
	HCURSOR Cursor;
	if (!GetWindowsMousepointer()) {
		Cursor = nullptr;
	} else {
		Cursor = DefaultCursor;
		switch (GetMouseCursor()) {
		case 1:
			Cursor = ArrowCursor;
			break;
		case 2:
			Cursor = SizeCursor;
			break;
		case 3:
			Cursor = WaitCursor;
			break;
		case 4:
		case 5:
			break;
		case 6:
			Cursor = HandCursor;
			break;
		default:
			return;
		}
	}
	SetCursor(Cursor);
}

// 0x4aa3c0, line 550. Outside the client area Windows sets the cursor.
BOOL CMainFrame::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	if (GetViewMode() != 2) {
		RECT Client;
		::GetClientRect(m_hWnd, &Client);
		::ClientToScreen(m_hWnd, reinterpret_cast<POINT *>(&Client.left));
		::ClientToScreen(m_hWnd, reinterpret_cast<POINT *>(&Client.right));
		POINT Cursor;
		PATELLA_GET_CURSOR_POS(&Cursor);
		if (!PtInRect(&Client, Cursor))
			return CFrameWnd::OnSetCursor(pWnd, nHitTest, message);
	}
	UpdateMouseCursor();
	return TRUE;
}

// 0x4aa5d0, line 568. In fullscreen mode the points are screen points.
void CMainFrame::OnLButtonDown([[maybe_unused]] UINT nFlags, CPoint point)
{
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	LeftButtonDown(point.x, point.y, KeyFlags);
	LeftButtonPressed = true;
	if (!RightButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
}

// 0x4aa7b0, line 588
void CMainFrame::OnLButtonUp([[maybe_unused]] UINT nFlags, CPoint point)
{
	Default();
	if (LeftButtonPressed) {
		if (GetViewMode() == 2)
			::ClientToScreen(m_hWnd, &point);
		LeftButtonUp(point.x, point.y, KeyFlags);
		LeftButtonPressed = false;
		if (!RightButtonPressed)
			ReleaseCapture();
	}
}

// 0x4aa990, line 606
void CMainFrame::OnLButtonDblClk([[maybe_unused]] UINT nFlags, CPoint point)
{
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	LeftButtonDoubleClick(point.x, point.y, KeyFlags);
	LeftButtonPressed = true;
	if (!RightButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
}

// 0x4aab70, line 624
void CMainFrame::OnRButtonDown([[maybe_unused]] UINT nFlags, CPoint point)
{
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	RightButtonDown(point.x, point.y, KeyFlags);
	RightButtonPressed = true;
	if (!LeftButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
}

// 0x4aad50, line 644
void CMainFrame::OnRButtonUp([[maybe_unused]] UINT nFlags, CPoint point)
{
	Default();
	if (RightButtonPressed) {
		if (GetViewMode() == 2)
			::ClientToScreen(m_hWnd, &point);
		RightButtonUp(point.x, point.y, KeyFlags);
		RightButtonPressed = false;
		if (!LeftButtonPressed)
			ReleaseCapture();
	}
}

// 0x4aaf30, line 662. The same as a single click.
void CMainFrame::OnRButtonDblClk([[maybe_unused]] UINT nFlags, CPoint point)
{
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	RightButtonDown(point.x, point.y, KeyFlags);
	RightButtonPressed = true;
	if (!LeftButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
}

// 0x4ab110, line 686. Another window took the mouse: releases the held
// buttons at the cursor position. The test build's scripted runs skip it
// (TestHooks.h, Patella-Modern issue #52).
void CMainFrame::OnCaptureChanged([[maybe_unused]] CWnd *pWnd)
{
	PATELLA_TEST_CAPTURE_CHANGED
	POINT Point;
	PATELLA_GET_CURSOR_POS(&Point);
	if (GetViewMode() == 1)
		::ScreenToClient(m_hWnd, &Point);
	if (LeftButtonPressed) {
		LeftButtonUp(Point.x, Point.y, KeyFlags);
		LeftButtonPressed = false;
	}
	if (RightButtonPressed) {
		RightButtonUp(Point.x, Point.y, KeyFlags);
		RightButtonPressed = false;
	}
	Default();
}

// 0x4ab310, line 704. One MouseWheel call per notch (120); direction 1 is
// up, 2 down. The catch block names point.x/point.y although the binary
// passes the mouse position from Status.cpp.
BOOL CMainFrame::OnMouseWheel([[maybe_unused]] UINT nFlags, short zDelta, [[maybe_unused]] CPoint point)
{
	Default();
	const int Delta = WheelDelta + zDelta;
	WheelDelta = Delta % 120;
	for (int i = 0; i < abs(Delta / 120); i++)
		MouseWheel(GetMouseX(), GetMouseY(), Delta > 0 ? 1 : 2);
	return TRUE;
}

// 0x4ab510, line 768. Modifiers are remembered; Alt+F4 closes the window;
// other translated keys go to KeyPress with the modifiers.
void CMainFrame::KeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	const int Key = TranslateKey(nChar, nRepCnt, nFlags);
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

// 0x4ab730
void CMainFrame::KeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	const int Key = TranslateKey(nChar, nRepCnt, nFlags);
	if (Key == KEY_SHIFT)
		KeyFlags &= ~KEY_SHIFT;
	else if (Key == KEY_CONTROL)
		KeyFlags &= ~KEY_CONTROL;
	else if (Key == KEY_ALT)
		KeyFlags &= ~KEY_ALT;
}

// 0x4ab7c0 (with OnSysKeyDown)
void CMainFrame::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	KeyDown(nChar, nRepCnt, nFlags);
}

void CMainFrame::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	KeyDown(nChar, nRepCnt, nFlags);
}

// 0x4ab7d0 (with OnSysKeyUp)
void CMainFrame::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	KeyUp(nChar, nRepCnt, nFlags);
}

void CMainFrame::OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	KeyUp(nChar, nRepCnt, nFlags);
}

// 0x4ab7e0, line 871. Characters up to 255; with Control (and not Alt) the
// control characters 1-26 become the letters 'a'-'z'.
void CMainFrame::OnChar(UINT nChar, [[maybe_unused]] UINT nRepCnt, [[maybe_unused]] UINT nFlags)
{
	Default();
	if ((KeyFlags & KEY_CONTROL) != 0 && (KeyFlags & KEY_ALT) == 0) {
		if (nChar == 0 || nChar > 26)
			return;
		nChar += 0x60;
	} else if (nChar > 255) {
		return;
	}
	KeyPress(KeyFlags | nChar);
}
