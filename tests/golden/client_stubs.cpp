// The runner's stubs (issue #26): the functions that the reconstructed files of golden_client
// (tests/golden/CMakeLists.txt) call in files the runner does not link, and the MFC members of
// src/compat/mfc_shim.h. difftest.exe links every reconstructed file and its tests' fakes; the runner
// links only the files its ported tests compare (since issue #28 also graphics.cpp, GUI.cpp and the window
// files, since issue #30 Communication.cpp and Control.cpp, so the stubs are the files below them: the Vulkan
// engine). None of these is called on the paths the tests run: each stops the runner with its name.

#include "common.h"

#include "Creatures.h"
#include "Objects.h"
#include "SdlFrame.h"
#include "Surface.h"
#include "vulkan/DeviceChoice.h"
#include "vulkan/DeviceSettings.h"

#include <ostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

namespace {

[[noreturn]] void StubCalled(const char *Name)
{
	printf("golden: stub %s called\n", Name);
	fflush(stdout);
	abort();
}

} // namespace

#define STUB() StubCalled(__FUNCTION__)

// The engine (Surface.cpp with PATELLA_VULKAN, as in Patella.exe).
bool IsVulkanInstalled() { STUB(); }
TSurface *CreateSurfaceVulkan(patella::platform::TSurfaceShared &) { STUB(); }
void LoadDeviceChoice() { STUB(); }
patella::vulkan::TDeviceChoice GetDeviceChoice() { STUB(); }
void SetDeviceChoice(const patella::vulkan::TDeviceChoice &) { STUB(); }
int GetNumberOfGpus() { STUB(); }
const std::string &GetCpuDriverManifest() { STUB(); }
const char *GetDeviceName() { STUB(); }
patella::vulkan::TDeviceButton patella::vulkan::ButtonOf(const TDeviceChoice &) { STUB(); }
// Control.cpp's ChangeEngine (the x64 client's, issue #31; Communication never reaches it).
bool SaveDeviceChoice() { STUB(); }
int GetBestGpu() { STUB(); }
patella::vulkan::TDeviceButton DeviceButtonOfEngine(int) { STUB(); }
patella::vulkan::TDeviceChoice patella::vulkan::ChoiceAfterButton(const TDeviceChoice &, TDeviceButton, int, int) { STUB(); }

// The SDL3 window (src/SdlFrame.cpp, issue #122), which Player.cpp's InitInstance, Run, OnIdle and FatalError call on
// the SDL3 path; the tests call none of them.
bool patella::sdlframe::Create(bool, const char *, const char *) { STUB(); }
patella::platform::TWindowHandle patella::sdlframe::Handle() { STUB(); }
void patella::sdlframe::Show(bool) { STUB(); }
void patella::sdlframe::Hide() { STUB(); }
int patella::sdlframe::Run(void (*)(long)) { STUB(); }
void patella::sdlframe::ExitProgram() { STUB(); }
void patella::sdlframe::SetDisplayMode(int) { STUB(); }
void patella::sdlframe::OpenURL(const char *) { STUB(); }
void patella::sdlframe::UpdateMouseCursor() { STUB(); }

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
int CWinThread::ExitInstance() { STUB(); }
int CWinThread::Run() { STUB(); }
const RECT CFrameWnd::rectDefault = {0, 0, 0, 0};
BOOL CFrameWnd::Create(LPCTSTR, LPCTSTR, DWORD, const RECT &, CWnd *, LPCTSTR, DWORD, CCreateContext *) { STUB(); }
BOOL CWnd::ShowWindow(int) { STUB(); }
