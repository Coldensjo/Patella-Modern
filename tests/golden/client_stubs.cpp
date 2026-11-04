// The runner's stubs (issue #26): the functions that the reconstructed files of golden_client
// (tests/golden/CMakeLists.txt) call in files the runner does not link, and the MFC members of
// src/compat/mfc_shim.h. difftest.exe links every reconstructed file and its tests' fakes; the runner
// links only the files its ported tests compare (since issue #28 also graphics.cpp, GUI.cpp and the window
// files, so the stubs are the files below the windows: Communication.cpp's Send functions, Control.cpp, the
// Vulkan engine). None of these is called on the paths the tests run: each stops the runner with its name.

#include "common.h"

#include "Communication.h"
#include "Control.h"
#include "Creatures.h"
#include "Objects.h"
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
void ChangeEngine(int) { STUB(); }

// Communication.cpp: the Send functions the window files call (issue #28: GUI.cpp, graphics.cpp and the
// window files are in golden_client; the window tests redirect what they call to their fakes).
void CancelPatch() { STUB(); }
void SendLogout() { STUB(); }
void SendWalk() { STUB(); }
void SendStop() { STUB(); }
void SendTurnNorth() { STUB(); }
void SendTurnEast() { STUB(); }
void SendTurnSouth() { STUB(); }
void SendTurnWest() { STUB(); }
void SendMoveObject(int, int, int, int, int, int, int, int, int) { STUB(); }
void SendRequestTrade(int, int, int, int, int, uint32) { STUB(); }
void SendInspectTrade(int, int) { STUB(); }
void SendAcceptTrade() { STUB(); }
void SendRejectTrade() { STUB(); }
void SendUseObject(int, int, int, int, int, int) { STUB(); }
void SendUseObjectWith(int, int, int, int, int, int, int, int, int, int) { STUB(); }
void SendUseObjectOnCreature(int, int, int, int, int, uint32) { STUB(); }
void SendRotateObject(int, int, int, int, int) { STUB(); }
void SendCloseContainer(int) { STUB(); }
void SendContainerUp(int) { STUB(); }
void SendEditText(uint32, const char *) { STUB(); }
void SendEditHouseText(int, uint32, const char *) { STUB(); }
void SendLook(int, int, int, int, int) { STUB(); }
void SendTalk(int, const char *) { STUB(); }
void SendTalk(int, const char *, const char *) { STUB(); }
void SendTalk(int, int, const char *) { STUB(); }
void SendRequestChannels() { STUB(); }
void SendJoinChannel(int) { STUB(); }
void SendLeaveChannel(int) { STUB(); }
void SendOpenPrivateConversation(const char *) { STUB(); }
void SendProcessRuleViolation(const char *) { STUB(); }
void SendCloseRuleViolation(const char *) { STUB(); }
void SendCancelRuleViolation() { STUB(); }
void SendFightModes() { STUB(); }
void SendAttack(uint32) { STUB(); }
void SendFollow(uint32) { STUB(); }
void SendOpenPrivateChannel() { STUB(); }
void SendInviteToPrivateChannel(const char *) { STUB(); }
void SendExcludeFromPrivateChannel(const char *) { STUB(); }
void SendInviteToParty(uint32) { STUB(); }
void SendJoinParty(uint32) { STUB(); }
void SendRevokePartyInvitation(uint32) { STUB(); }
void SendPassPartyLeadership(uint32) { STUB(); }
void SendLeaveParty() { STUB(); }
void SendCancel() { STUB(); }
void SendRequestOutfit() { STUB(); }
void SendOutfit(int, const int *) { STUB(); }
void SendAddBuddy(const char *) { STUB(); }
void SendRemoveBuddy(uint32) { STUB(); }
void SendBugReport(const char *) { STUB(); }
void SendRuleViolationReport(const char *, uint8, uint8, const char *, uint32, bool) { STUB(); }

// The engine (Surface.cpp with PATELLA_VULKAN, as in Patella.exe).
bool IsVulkanInstalled() { STUB(); }
TSurface *CreateSurfaceVulkan() { STUB(); }
void LoadDeviceChoice() { STUB(); }
patella::vulkan::TDeviceChoice GetDeviceChoice() { STUB(); }
void SetDeviceChoice(const patella::vulkan::TDeviceChoice &) { STUB(); }
int GetNumberOfGpus() { STUB(); }
const std::string &GetCpuDriverManifest() { STUB(); }
const char *GetDeviceName() { STUB(); }
patella::vulkan::TDeviceButton patella::vulkan::ButtonOf(const TDeviceChoice &) { STUB(); }

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
