// MainFrm.cpp: reconstructed from Tibia.exe 7.72 (0x4a8200-0x4abec8).
//
// The frame window of the client. Source order follows the catch lines;
// functions without line evidence stay where the binary has them: the
// message map, TranslateKey and OutputErrorInformations at the start (the
// gap after Inventory.cpp), ExitProgram after the destructor, AddFrameSize
// before OnSize, KeyUp and the OnKeyDown/OnKeyUp handlers after KeyDown.
// The compiler placed SetDefaultWindowPosition (line 375), OnCreate (273)
// and OnDisplayChange (295) after the key handlers.
//
// Every handler is wrapped in TBEGIN/TEND. In ExitProgram, AddFrameSize and
// KeyUp the binary has the unwind states but no try block: they call only C
// functions (or TranslateKey, which has no exception frame), so the
// compiler removed the handlers.

#include "MainFrm.h"
#include "Control.h"
#include "Creatures.h"
#include "GUI.h"
#include "MainWindow.h"
#include "Network.h"
#include "Player.h"
#include "Preferences.h"
#include "Status.h"
#include "Surface.h"
#include "Text.h"
#include <ostream>

// Data, in .bss order.
uint8 PacketHeader[16];         // 0x5d1644, written by Network.cpp
CMainFrame *MainFrame;          // 0x5d1654
static HCURSOR HandCursor;      // 0x5d1658, cursor resource 0x89 (mouse cursor 6)
static HCURSOR SizeCursor;      // 0x5d165c, IDC_SIZENS (mouse cursor 2)
static HCURSOR WaitCursor;      // 0x5d1660, IDC_WAIT (mouse cursor 3)
static HCURSOR ArrowCursor;     // 0x5d1664, IDC_ARROW (mouse cursor 1)
static HCURSOR DefaultCursor;   // 0x5d1668, cursor resource 0x84 (the others)
static bool LeftButtonPressed;  // 0x5d166c
static bool RightButtonPressed; // 0x5d166d
static bool Closing;            // 0x5d166f, CloseRequest is not asked any more
bool DisplayChanged;            // 0x5d1670, set by OnDisplayChange; SurfaceGL.cpp's Init reads and clears it
// At 0x5d1674, constructed by the static initialiser 0x559f20 as TCallback(NULL, 0) (no atexit) and
// never used, like the window files' (issue #24).
static TCallback NoCallback(NULL, 0);
static char ClassName[32]; // 0x5d167c
static int WheelDelta;     // 0x5d169c, the remainder below one notch (120)

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
int CMainFrame::TranslateKey(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool Extended = (nFlags & 0x100) == 0x100;
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

// 0x4a8360. For error.txt (Player.cpp's HandleException): the start of the
// last packet, if any, and the player's position.
void CMainFrame::OutputErrorInformations(std::ostream &Out)
{
	int i;
	for (i = 0; i < 16; i++) {
		if (PacketHeader[i] != 0)
			break;
	}
	if (i < 16) {
		Out << "Last Packet:";
		for (i = 0; i < 16; i++) {
			char Help[8];
			sprintf(Help, " %03u", (unsigned int)PacketHeader[i]);
			Out << Help;
		}
		Out << std::endl;
	}
	if (GetPlayerID() != 0) {
		try {
			TCreature *Player = GetCreature(GetPlayerID());
			if (Player != NULL)
				Out << "Player Position: [" << Player->RealX << "," << Player->RealY << "," << Player->RealZ << "]"
					<< std::endl;
		} catch (...) {
		}
	}
}

// 0x4a8520, line 112
CMainFrame::CMainFrame(bool Gamemaster)
{
	TBEGIN
	MainFrame = this;
	KeyFlags = 0;
	memset(KeyStates, 0, sizeof(KeyStates));
	memset(PacketHeader, 0, sizeof(PacketHeader));
	ArrowCursor = LoadCursorA(NULL, IDC_ARROW);
	SizeCursor = LoadCursorA(NULL, IDC_SIZENS);
	WaitCursor = LoadCursorA(NULL, IDC_WAIT);
	DefaultCursor = LoadCursorA(AfxGetInstanceHandle(), MAKEINTRESOURCEA(0x84));
	HandCursor = LoadCursorA(AfxGetInstanceHandle(), MAKEINTRESOURCEA(0x89));
	SetWindowedMode(-1, -1, 0, 0, false);
	InitControl(Gamemaster);
	SetViewMode(1);
	MouseCursor = GetMouseCursor();
	TEND(112)
}

// 0x4a8760, line 123. The handlers rethrow out of the destructor; modern
// compilers make destructors noexcept and warn about that. Unlike
// TComponent's, this one cannot be noexcept(false): CFrameWnd's destructor
// (MFC) is noexcept and an override may not throw more (C2694). An exception
// from ExitControl ends the program here, where the original's goes on to
// the caller of the destructor (issue #34).
#pragma warning(push)
#pragma warning(disable : 4297)
CMainFrame::~CMainFrame()
{
	TBEGIN
	ExitControl();
	TEND(123)
}
#pragma warning(pop)

// 0x4a88f0. Closes the client without asking (Control.cpp's exit callback).
void ExitProgram()
{
	TBEGIN
	Closing = true;
	PostMessageA(MainFrame->m_hWnd, WM_CLOSE, 0, 0);
	TEND(135)
}

// 0x4a8940, line 149. Like ExitProgram, but Run starts patch.exe afterwards.
void CloseProgram()
{
	TBEGIN
	CPlayerApp *App = (CPlayerApp *)AfxGetApp();
	if (App != NULL)
		App->RunPatch = true;
	Closing = true;
	PostMessageA(MainFrame->m_hWnd, WM_CLOSE, 0, 0);
	TEND(149)
}

// 0x4a8af0, line 181. View mode 2 is fullscreen: a topmost popup window over
// the whole screen; 1 is a normal window at its old placement.
void SetDisplayMode(int ViewMode)
{
	TBEGIN
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
	TEND1(181, ViewMode)
}

// 0x4a8d80, line 193. Leaves fullscreen mode first.
void OpenURL(const char *URL)
{
	TBEGIN
	if (GetViewMode() == 2)
		SetDisplayMode(1);
	ShellExecuteA(NULL, NULL, URL, NULL, NULL, SW_SHOWNORMAL);
	TEND(193)
}

// 0x4a8f30, line 244. The window class registered by InitInstance.
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT &cs)
{
	TBEGIN
	GetWindowClassName(ClassName);
	cs.lpszClass = ClassName;
	CFrameWnd::PreCreateWindow(cs);
	return TRUE;
	TEND(244)
}

// 0x4abbc0, line 273. Creates the main game window (MainWindow.cpp), hands
// the callbacks to Control.cpp and restores the window position when it
// fits on the screen.
int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	TBEGIN
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	RECT Client;
	::GetClientRect(m_hWnd, &Client);
	InitMainWindow(Client.right, Client.bottom, ExitProgram, SetDisplayMode, OpenURL);
	InitWindow(m_hWnd, ExitProgram, CloseProgram, SetDisplayMode, OpenURL);
	int x, y, Width, Height;
	bool Maximized;
	GetWindowedMode(&x, &y, &Width, &Height, &Maximized);
	if (x >= 0 && y >= 0 && x + Width <= GetSystemMetrics(SM_CXSCREEN)
		&& y + Height <= GetSystemMetrics(SM_CYSCREEN))
		MoveWindow(x, y, Width, Height, TRUE);
	else
		SetDefaultWindowPosition();
	PATELLA_SET_MAIN_TIMER(m_hWnd, 1, 100, NULL);
	return 0;
	TEND(273)
}

// 0x4abe40, line 295. A screen smaller than the window in windowed mode
// moves it back to the default position.
LRESULT CMainFrame::OnDisplayChange(WPARAM wParam, LPARAM lParam)
{
	TBEGIN
	if (GetViewMode() == 1) {
		RECT Rect;
		::GetWindowRect(m_hWnd, &Rect);
		if ((int)LOWORD(lParam) < Rect.right - Rect.left || (int)HIWORD(lParam) < Rect.bottom - Rect.top)
			SetDefaultWindowPosition();
		DisplayChanged = true;
	}
	return 0;
	TEND(295)
}

// 0x4a90e0, line 335. WSAAsyncGetHostByName finished (Network.cpp gives the
// window to it): connect to the resolved address, or report the error.
LRESULT CMainFrame::OnHostResolved(WPARAM wParam, LPARAM lParam)
{
	TBEGIN
	if (WSAGETASYNCERROR(lParam) == 0) {
		if (GetConnectionState() != 1) {
			SetConnectionTimeout(0);
			SetConnectionState(0);
			Disconnect(true);
			char *Text = new char[1024];
			GetErrorMessage(Text, -1, 1);
			AddMessage(1, (INT_PTR) "Connection failed", (INT_PTR)Text);
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
	TEND(335)
}

// 0x4a9310, line 356. Forgets the held modifiers and leaves fullscreen mode
// whenever the activation changes (both ways).
void CMainFrame::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
	TBEGIN
	KeyFlags = 0;
	if (GetViewMode() == 2)
		SetDisplayMode(1);
	TEND(356)
}

// 0x4ab9d0, line 375. 600x450 client area, centred.
void CMainFrame::SetDefaultWindowPosition()
{
	TBEGIN
	int Width = 600;
	int Height = 450;
	AddFrameSize(&Width, &Height);
	int x = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
	int y = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
	SetWindowedMode(x, y, Width, Height, false);
	MoveWindow(x, y, Width, Height, TRUE);
	TEND(375)
}

// 0x4a94b0. From a client size to the window size.
void CMainFrame::AddFrameSize(int *Width, int *Height)
{
	TBEGIN
	RECT Client, Window;
	::GetClientRect(m_hWnd, &Client);
	::GetWindowRect(m_hWnd, &Window);
	*Width += Client.left - Client.right - Window.left + Window.right;
	*Height += Client.top - Client.bottom - Window.top + Window.bottom;
	TEND(388)
}

// 0x4a9540, line 414. Keeps the client area at least 550x450.
void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	TBEGIN
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
		int Width = cx;
		int Height = cy;
		CFrameWnd::OnSize(nType, cx, cy);
		if (!::IsIconic(m_hWnd))
			Resize(Width, Height);
	}
	TEND2(414, cx, cy)
}

// 0x4a9790, line 430. The smallest window: a client area of 550x450.
void CMainFrame::OnGetMinMaxInfo(MINMAXINFO *lpMMI)
{
	TBEGIN
	Default();
	int Width = 550;
	int Height = 450;
	AddFrameSize(&Width, &Height);
	lpMMI->ptMinTrackSize.y = Height;
	lpMMI->ptMinTrackSize.x = Width;
	TEND(430)
}

// 0x4a9950, line 441. In fullscreen mode everything is client area.
LRESULT CMainFrame::OnNcHitTest(CPoint point)
{
	TBEGIN
	if (GetViewMode() == 2)
		return HTCLIENT;
	return Default();
	TEND(441)
}

// 0x4a9b00, line 454
void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	TBEGIN
	Default();
	TimerEvent();
	TEND1(454, nIDEvent)
}

// 0x4a9c90, line 466. Control.cpp may refuse (logout warning in the game).
void CMainFrame::OnClose()
{
	TBEGIN
	if (!Closing && !CloseRequest())
		return;
	CFrameWnd::OnClose();
	TEND(466)
}

// 0x4a9e30, line 481
BOOL CMainFrame::DestroyWindow()
{
	TBEGIN
	if (GetViewMode() == 2)
		SetDisplayMode(1);
	return CWnd::DestroyWindow();
	TEND(481)
}

// 0x4a9fd0, line 500. Saves the windowed position for Tibia.cfg.
void CMainFrame::OnDestroy()
{
	TBEGIN
	if (GetViewMode() == 1)
		GetWindowPlacement(&Placement);
	RECT &Normal = Placement.rcNormalPosition;
	SetWindowedMode(Normal.left, Normal.top, Normal.right - Normal.left, Normal.bottom - Normal.top,
		::IsZoomed(m_hWnd) != 0);
	CFrameWnd::OnDestroy();
	::KillTimer(m_hWnd, 1);
	DestroyCursor(DefaultCursor);
	DestroyCursor(HandCursor);
	TEND(500)
}

// 0x4aa1d0, line 523. The Windows cursor for the client's mouse cursor
// (Status.cpp), or none when the client draws its own.
void UpdateMouseCursor()
{
	TBEGIN
	HCURSOR Cursor;
	if (!GetWindowsMousepointer()) {
		Cursor = NULL;
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
	TEND(523)
}

// 0x4aa3c0, line 550. Outside the client area Windows sets the cursor.
BOOL CMainFrame::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	TBEGIN
	if (GetViewMode() != 2) {
		RECT Client;
		::GetClientRect(m_hWnd, &Client);
		::ClientToScreen(m_hWnd, (POINT *)&Client.left);
		::ClientToScreen(m_hWnd, (POINT *)&Client.right);
		POINT Cursor;
		PATELLA_GET_CURSOR_POS(&Cursor);
		if (!PtInRect(&Client, Cursor))
			return CFrameWnd::OnSetCursor(pWnd, nHitTest, message);
	}
	UpdateMouseCursor();
	return TRUE;
	TEND(550)
}

// 0x4aa5d0, line 568. In fullscreen mode the points are screen points.
void CMainFrame::OnLButtonDown(UINT nFlags, CPoint point)
{
	TBEGIN
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	LeftButtonDown(point.x, point.y, KeyFlags);
	LeftButtonPressed = true;
	if (!RightButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
	TEND2(568, point.x, point.y)
}

// 0x4aa7b0, line 588
void CMainFrame::OnLButtonUp(UINT nFlags, CPoint point)
{
	TBEGIN
	Default();
	if (LeftButtonPressed) {
		if (GetViewMode() == 2)
			::ClientToScreen(m_hWnd, &point);
		LeftButtonUp(point.x, point.y, KeyFlags);
		LeftButtonPressed = false;
		if (!RightButtonPressed)
			ReleaseCapture();
	}
	TEND2(588, point.x, point.y)
}

// 0x4aa990, line 606
void CMainFrame::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	TBEGIN
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	LeftButtonDoubleClick(point.x, point.y, KeyFlags);
	LeftButtonPressed = true;
	if (!RightButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
	TEND2(606, point.x, point.y)
}

// 0x4aab70, line 624
void CMainFrame::OnRButtonDown(UINT nFlags, CPoint point)
{
	TBEGIN
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	RightButtonDown(point.x, point.y, KeyFlags);
	RightButtonPressed = true;
	if (!LeftButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
	TEND2(624, point.x, point.y)
}

// 0x4aad50, line 644
void CMainFrame::OnRButtonUp(UINT nFlags, CPoint point)
{
	TBEGIN
	Default();
	if (RightButtonPressed) {
		if (GetViewMode() == 2)
			::ClientToScreen(m_hWnd, &point);
		RightButtonUp(point.x, point.y, KeyFlags);
		RightButtonPressed = false;
		if (!LeftButtonPressed)
			ReleaseCapture();
	}
	TEND2(644, point.x, point.y)
}

// 0x4aaf30, line 662. The same as a single click.
void CMainFrame::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	TBEGIN
	Default();
	if (GetViewMode() == 2)
		::ClientToScreen(m_hWnd, &point);
	RightButtonDown(point.x, point.y, KeyFlags);
	RightButtonPressed = true;
	if (!LeftButtonPressed)
		CWnd::FromHandle(::SetCapture(m_hWnd));
	TEND2(662, point.x, point.y)
}

// 0x4ab110, line 686. Another window took the mouse: releases the held
// buttons at the cursor position.
void CMainFrame::OnCaptureChanged(CWnd *pWnd)
{
	TBEGIN
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
	TEND(686)
}

// 0x4ab310, line 704. One MouseWheel call per notch (120); direction 1 is
// up, 2 down. The catch block names point.x/point.y although the binary
// passes the mouse position from Status.cpp.
BOOL CMainFrame::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
	TBEGIN
	Default();
	int Delta = WheelDelta + zDelta;
	WheelDelta = Delta % 120;
	for (int i = 0; i < abs(Delta / 120); i++)
		MouseWheel(GetMouseX(), GetMouseY(), Delta > 0 ? 1 : 2);
	return TRUE;
	TEND2(704, point.x, point.y)
}

// 0x4ab510, line 768. Modifiers are remembered; Alt+F4 closes the window;
// other translated keys go to KeyPress with the modifiers.
void CMainFrame::KeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	TBEGIN
	int Key = TranslateKey(nChar, nRepCnt, nFlags);
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
	TEND2(768, nChar, nFlags)
}

// 0x4ab730
void CMainFrame::KeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	TBEGIN
	int Key = TranslateKey(nChar, nRepCnt, nFlags);
	if (Key == KEY_SHIFT)
		KeyFlags &= ~KEY_SHIFT;
	else if (Key == KEY_CONTROL)
		KeyFlags &= ~KEY_CONTROL;
	else if (Key == KEY_ALT)
		KeyFlags &= ~KEY_ALT;
	TEND(795)
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
void CMainFrame::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	TBEGIN
	Default();
	if ((KeyFlags & KEY_CONTROL) != 0 && (KeyFlags & KEY_ALT) == 0) {
		if (nChar == 0 || nChar > 26)
			return;
		nChar += 0x60;
	} else if (nChar > 255) {
		return;
	}
	KeyPress(KeyFlags | nChar);
	TEND2(871, nChar, nFlags)
}
