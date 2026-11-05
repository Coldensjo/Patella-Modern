// The link stubs of Player.cpp and MainFrm.cpp in difftest.exe. The Player test itself (Player.cpp,
// Surface.cpp and MainFrm.cpp's TranslateKey against the original) is a golden-corpus test since
// issue #26: tests/golden/player_tests.cpp.
//
// Only the two text decoders can run in the child; everything else in
// Player.cpp needs MFC, the window system or the original's iostreams.
// Player.cpp is linked against the stubs below, which are never called.

#include "difftest.h"
#include "../../src/Player.h"
#include "../../src/MainFrm.h"
#include "../../src/Surface.h"
#include "../../src/sysinc.h" // TitleCode, ClassNameCode (defined below for difftest.exe)

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

// --- Link stubs for Player.cpp (MFC shim declarations and callees that are
// not part of the test build). None of them is called by the tests.

// Each stub has a body of its own (the function's name): the linker folds
// identical functions, and tests/golden/mainfrm_tests.cpp redirects the
// Control.cpp stubs one by one. The MFC members MainFrm.cpp calls are its fakes,
// Network.cpp is compiled into tests/golden/network_tests.cpp (issue #37; issue #30).
[[noreturn]] static void StubCalled(const char *Name)
{
	printf("stub %s called\n", Name);
	abort();
}
#define STUB { StubCalled(__FUNCTION__); }
// For MainFrm.cpp.
bool CloseRequest() STUB
void ExitControl() STUB
void InitControl(bool) STUB
void InitWindow(HWND, void (*)(), void (*)(int), void (*)(const char *)) STUB
void KeyPress(int) STUB
void LeftButtonDoubleClick(int, int, int) STUB
void LeftButtonDown(int, int, int) STUB
void LeftButtonUp(int, int, int) STUB
void MouseWheel(int, int, int) STUB
void Resize(int, int) STUB
void RightButtonDown(int, int, int) STUB
void RightButtonUp(int, int, int) STUB
void TimerEvent() STUB
BOOL CWnd::ShowWindow(int) STUB
const RECT CFrameWnd::rectDefault = {0, 0, 0, 0};
BOOL CFrameWnd::Create(LPCTSTR, LPCTSTR, DWORD, const RECT &, CWnd *, LPCTSTR, DWORD, CCreateContext *) STUB
BOOL CWinThread::InitInstance() STUB
int CWinThread::Run() STUB
BOOL CWinThread::OnIdle(LONG) STUB
CWinApp::CWinApp(LPCTSTR) { m_pMainWnd = NULL; m_lpCmdLine = NULL; }
CWnd *AfxGetMainWnd() STUB
BOOL AfxRegisterClass(WNDCLASS *) STUB
HINSTANCE AfxGetResourceHandle() STUB
WNDPROC AfxGetAfxWndProc() STUB





// Not folded into one function: errortest.cpp redirects each to its own fake.
const char *GetErrorText(int) { printf("GetErrorText stub\n"); abort(); }
const char *GetErrorHint(int) { printf("GetErrorHint stub\n"); abort(); }

// sysinc.cpp's halves of the encoded window title and class name (sysinc.cpp is not
// linked into difftest.exe).
const std::array<int, 8> TitleCode = {68, 75, 129, 81, 98, 78, 96, 141};
const std::array<int, 11> ClassNameCode = {57, 193, 225, 58, 140, 118, 195, 214, 239, 244, 137};
void Redraw(bool) STUB
void MouseMove(int, int) STUB
void ProcessEvents() STUB
// Control.cpp's, for Player.cpp's developer log diagnostics (issue #77; at the debug level, never in the tests).
std::array<uint8, 10> GetLastPacketTypes() STUB
