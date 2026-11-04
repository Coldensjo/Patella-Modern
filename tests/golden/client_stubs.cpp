// The runner's stubs (issue #26): the functions that the reconstructed files of golden_client
// (tests/golden/CMakeLists.txt) call in files the runner does not link, and the MFC members of
// src/compat/mfc_shim.h. difftest.exe links every reconstructed file and its tests' fakes; the runner
// links only the files its ported tests compare. None of these is called on the paths the tests run:
// each stops the runner with its name.

#include "common.h"

#include "Control.h"
#include "Creatures.h"
#include "GUI.h"
#include "MainWindow.h"
#include "Network.h"
#include "Objects.h"
#include "Surface.h"
#include "graphics.h"
#include "vulkan/DeviceSettings.h"

#include <ostream>
#include <stdio.h>
#include <stdlib.h>

namespace {

[[noreturn]] void StubCalled(const char *Name)
{
	printf("golden: stub %s called\n", Name);
	fflush(stdout);
	abort();
}

} // namespace

#define STUB() StubCalled(__FUNCTION__)

// Control.cpp.
bool CloseRequest() { STUB(); }
void ExitControl() { STUB(); }
void InitControl(bool) { STUB(); }
void InitWindow(HWND, void (*)(), void (*)(), void (*)(int), void (*)(const char *)) { STUB(); }
void KeyPress(int) { STUB(); }
void LeftButtonDoubleClick(int, int, int) { STUB(); }
void LeftButtonDown(int, int, int) { STUB(); }
void LeftButtonUp(int, int, int) { STUB(); }
void MouseMove(int, int) { STUB(); }
void MouseWheel(int, int, int) { STUB(); }
void OutputControlErrorInformations(std::ostream &) { STUB(); }
void ProcessEvents() { STUB(); }
void Redraw(bool) { STUB(); }
void Resize(int, int) { STUB(); }
void RightButtonDown(int, int, int) { STUB(); }
void RightButtonUp(int, int, int) { STUB(); }
void TimerEvent() { STUB(); }

// Network.cpp.
void ConnectToLoginServer() { STUB(); }
void Disconnect(bool) { STUB(); }
void GetErrorMessage(char *, int, int) { STUB(); }
const char *GetResolvedIPAddress() { STUB(); }
void SetConnectionTimeout(int) { STUB(); }
void SetServerError(int) { STUB(); }
void SetServerIPAddress(const char *) { STUB(); }
void SetTestServerAddress(const char *) { STUB(); }
void SetTestServerPort(int) { STUB(); }
void ShutdownConnection() { STUB(); }

// GUI.cpp, MainWindow.cpp, graphics.cpp.
// MainFrm.cpp's NoCallback is constructed at start-up: GUI.cpp's constructor (0x496130), which only
// stores its arguments.
TCallback::TCallback(TComponent *Receiver, int Event)
{
	this->Receiver = Receiver;
	this->Event = Event;
}
void InitMainWindow(int, int, void (*)(), void (*)(int), void (*)(const char *)) { STUB(); }
void InitVideoCache() { STUB(); }

// The engine (Surface.cpp with PATELLA_VULKAN, as in Patella.exe).
bool IsVulkanInstalled() { STUB(); }
TSurface *CreateSurfaceVulkan() { STUB(); }
void LoadDeviceChoice() { STUB(); }
patella::vulkan::TDeviceChoice GetDeviceChoice() { STUB(); }
void SetDeviceChoice(const patella::vulkan::TDeviceChoice &) { STUB(); }

// src/compat/mfc_shim.h's MFC members (those MainFrm.cpp calls are mainfrm_tests.cpp's fakes, issue #54).
// Player.cpp's theApp is constructed at start-up (as in difftest.exe's playertest.cpp).
CWinApp::CWinApp(LPCTSTR)
{
	m_pMainWnd = nullptr;
	m_lpCmdLine = nullptr;
}
CWnd *AfxGetMainWnd() { STUB(); }
HINSTANCE AfxGetResourceHandle() { STUB(); }
WNDPROC AfxGetAfxWndProc() { STUB(); }
BOOL AfxRegisterClass(WNDCLASS *) { STUB(); }
BOOL CWinThread::InitInstance() { STUB(); }
BOOL CWinThread::OnIdle(LONG) { STUB(); }
int CWinThread::Run() { STUB(); }
const RECT CFrameWnd::rectDefault = {0, 0, 0, 0};
BOOL CFrameWnd::Create(LPCTSTR, LPCTSTR, DWORD, const RECT &, CWnd *, LPCTSTR, DWORD, CCreateContext *) { STUB(); }
BOOL CWnd::ShowWindow(int) { STUB(); }
