// TestHooks.h: the places where the test mode reaches into the client (issue #21;
// notes/modernization-plan.md, 4.4). Included by common.h.
//
// In the product and oracle builds every hook is the system call it replaces, word for word, so they
// compile to the same code and contain nothing of the test mode. The test build (PatellaTest.exe,
// cmake/WindowsX64.cmake) defines PATELLA_TEST_MODE: the hooks go to src/testmode/TestModeWin32.cpp,
// which, with a script (--test-script <file>), drives the client from src/testmode/InputScript.h.
//
// Issue #109 (notes/decisions.md, "#109: the platform interfaces"): the clocks are no hooks any more. timeGetTime,
// time and the frame timer's Sleep (PATELLA_TIME_GET_TIME, PATELLA_TIME and PATELLA_FRAME_SLEEP until then) are the
// platform's IClock (src/platform/Platform.h), whose test implementation is the test mode's clock (TestModeWin32.cpp's
// Platform()). The hooks below are the services of the interfaces still to come (IInput: the cursor, the timer, the
// idle step, the dropped input, the capture; IWindow: the message boxes, the windows' count, the display modes) and
// the test mode's observers and failures, which are not a platform's service (the packets, the frames, the present
// and InitControl failures):
//
//	PATELLA_GET_CURSOR_POS(Point)      GetCursorPos: the mouse, polled in OnIdle (Player.cpp, MainFrm.cpp)
//	PATELLA_SET_MAIN_TIMER(...)        the main frame's 100 ms timer (MainFrm.cpp OnCreate); with a script
//	                                   the driver makes its ticks
//	PATELLA_TEST_IDLE                  the driver's step, at the start of CPlayerApp::OnIdle (a statement
//	                                   without a semicolon; empty outside the test build)
//
// Issue #22 (the packet log and the frame dump; notes/testmode.md), statements without a semicolon too,
// nothing at all outside the test build:
//
//	PATELLA_TEST_RSA_BLOCK(Block)      Communication.cpp SendLogin, SendGameLogin: the RSA block before
//	                                   it is encrypted (the packet log writes the first packets with it)
//	PATELLA_TEST_PACKET_SENT(Data, Size, Padded)
//	                                   Network.cpp SendCommand: SendData with the packet complete, before
//	                                   XTEA encryption (Padded: its length with the padding; 0: not
//	                                   encrypted)
//	PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize)
//	                                   Network.cpp PrepareNextPacket: ReceiveData with the next packet
//	                                   decrypted
//	PATELLA_TEST_FRAME_OBSERVERS(Frame, Pixels, Mask, Device)
//	                                   vulkan/SurfaceVulkan.cpp InitDisplay: the test mode's frame dump
//	                                   becomes the engine's frame observer (and, issue #92, its device
//	                                   observer: the control channel's probe names the device); the only
//	                                   observers since issue #97 (no patcap.dll lookup)
//
// Issue #23 (the reference recordings; notes/testmode.md):
//
//	PATELLA_MESSAGE_BOX(Window, Text, Caption, Type)
//	                                   MessageBoxA: Player.cpp (FatalError, the start's two boxes); with a
//	                                   script the box is not shown (written to messages.txt, answered OK)
//
// Issue #52 (a statement without a semicolon, nothing at all outside the test build):
//
//	PATELLA_TEST_CAPTURE_CHANGED       MainFrm.cpp OnCaptureChanged, before its body: with a script the
//	                                   window's loss of the mouse capture is desktop input (another
//	                                   window activated, the user's own clicks) and does not release the
//	                                   script's held buttons; the client's own releases come after its
//	                                   button flags are cleared, so the body would do nothing for them
//
// Issue #48:
//
//	PATELLA_ENUM_DISPLAY_SETTINGS(Device, Index, Mode)
//	                                   EnumDisplaySettingsA: vulkan/SurfaceVulkan.cpp GetFullscreenModes (the
//	                                   Graphics Options dialog's list); with --test-display-modes <file> the
//	                                   modes come from the file, not from the machine (src/testmode/DisplayModes.h)
//
// Issue #83 (a statement without a semicolon, nothing at all outside the test build):
//
//	PATELLA_TEST_PRESENT_FAILURE(Failure)
//	                                   vulkan/SurfaceVulkan.cpp, before TRenderer::Present: with
//	                                   PATELLA_TEST_FAIL_PRESENT=1 in the environment the statement Failure
//	                                   (a TVulkanError thrown, as a failing vkQueuePresentKHR's) runs in place
//	                                   of every present, so the first one fails (the CPU driver's present on a
//	                                   hidden desktop, issue #56)
//
// Issue #90 (a statement without a semicolon, nothing at all outside the test build):
//
//	PATELLA_TEST_INIT_FAILURE(Step)    Control.cpp InitControl, before each of its 15 steps (Step 1-15) and
//	                                   after the last (16): with PATELLA_TEST_FAIL_INIT=<Step> in the
//	                                   environment InitControl throws PROGRAM_ERROR 6 (InitNetwork's "Cannot
//	                                   initialize Windows Sockets.") there, as a failing step does, so its
//	                                   catch runs ExitControl with the steps before it done
//
// Issue #96:
//
//	PATELLA_ENUM_WINDOWS(Function, Parameter)
//	                                   EnumWindows: Player.cpp InitInstance, the count of the Tibia windows
//	                                   (CountTibiaWindows; "A Tibia client is already running!" with more than
//	                                   one); the test build hands the callback only its own process's windows,
//	                                   so another client on the machine does not stop a test client
//
// The state dump reads the client's data through the View functions at the end of Creatures.cpp,
// Inventory.cpp, Map.cpp and Status.cpp (src/testmode/ClientView.h; test build only).
//
// With a script, CPlayerApp::PreTranslateMessage (test build only) drops the keyboard and mouse input
// that comes from Windows, so the desktop's own mouse and keyboard cannot reach the client.

#pragma once

#if defined(PATELLA_TEST_MODE)

// The Windows types of the declarations: MFC, as common.h includes it (before anything of Windows, which
// MFC requires; the test build has real MFC, never the shim). PATELLA_CDECL: compat/msvc.h.
// In the client's sources common.h has included both already (issue #49).
#include <afxwin.h>

#include "compat/msvc.h"
#include "compat/span.h"
#include "testmode/ClientView.h" // IWYU pragma: export

namespace patella::testmode {
BOOL GetCursorPosition(LPPOINT Point);
UINT_PTR SetMainTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function);
void Idle();
// True: the message is input from Windows that the driver's script replaces (dropped).
bool DropInput(const MSG *Message);
// The buffers as spans (issue #60): the RSA block, and the whole of SendData and ReceiveData.
void RsaBlock(patella::TSpan<const unsigned char, 128> Block);
void PacketSent(patella::TSpan<const unsigned char> Data, int Size, int Padded);
void PacketReceived(patella::TSpan<const unsigned char> Data, int PacketSize);
void FrameObservers(int(PATELLA_CDECL **Frame)(int, int), void(PATELLA_CDECL **Pixels)(const void *, int, int, int),
	void(PATELLA_CDECL **Mask)(const unsigned char *, int, int), void(PATELLA_CDECL **Device)(const char *, int, int));
int ShowMessageBox(HWND Window, const char *Text, const char *Caption, UINT Type);
// True: a WM_CAPTURECHANGED that the script's input does not know of (with a script: every one).
bool IgnoreCaptureChange();
// EnumDisplaySettingsA, or the --test-display-modes list (Device null, Index a mode's number).
BOOL EnumDisplayMode(LPCSTR Device, DWORD Index, DEVMODEA *Mode);
// True: PATELLA_TEST_FAIL_PRESENT is 1 (issue #83), every present fails.
bool PresentFails();
// True: PATELLA_TEST_FAIL_INIT is Step (issue #90), InitControl fails there.
bool InitFails(int Step);
// EnumWindows over this process's top-level windows only (issue #96).
BOOL EnumOwnWindows(WNDENUMPROC Function, LPARAM Parameter);
} // namespace patella::testmode

#define PATELLA_GET_CURSOR_POS(Point) patella::testmode::GetCursorPosition(Point)
#define PATELLA_SET_MAIN_TIMER(Window, Id, Elapse, Function) patella::testmode::SetMainTimer(Window, Id, Elapse, Function)
#define PATELLA_TEST_IDLE patella::testmode::Idle();
#define PATELLA_TEST_RSA_BLOCK(Block) patella::testmode::RsaBlock(Block);
#define PATELLA_TEST_PACKET_SENT(Data, Size, Padded) patella::testmode::PacketSent(Data, Size, Padded);
#define PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize) patella::testmode::PacketReceived(Data, PacketSize);
#define PATELLA_TEST_FRAME_OBSERVERS(Frame, Pixels, Mask, Device) \
	patella::testmode::FrameObservers(&(Frame), &(Pixels), &(Mask), &(Device));
#define PATELLA_MESSAGE_BOX(Window, Text, Caption, Type) patella::testmode::ShowMessageBox(Window, Text, Caption, Type)
#define PATELLA_TEST_CAPTURE_CHANGED \
	if (patella::testmode::IgnoreCaptureChange()) { \
		Default(); \
		return; \
	}
#define PATELLA_ENUM_DISPLAY_SETTINGS(Device, Index, Mode) patella::testmode::EnumDisplayMode(Device, Index, Mode)
#define PATELLA_TEST_PRESENT_FAILURE(Failure) \
	if (patella::testmode::PresentFails()) { Failure; }
#define PATELLA_TEST_INIT_FAILURE(Step) \
	if (patella::testmode::InitFails(Step)) { throw TProgramError{PROGRAM_ERROR::SOCKET_INIT}; }
#define PATELLA_ENUM_WINDOWS(Function, Parameter) patella::testmode::EnumOwnWindows(Function, Parameter)

#else

#define PATELLA_GET_CURSOR_POS(Point) GetCursorPos(Point)
#define PATELLA_SET_MAIN_TIMER(Window, Id, Elapse, Function) ::SetTimer(Window, Id, Elapse, Function)
// No statement at all: an empty one is a nop in an unoptimised MSVC build (Debug), which would move
// the code after it.
#define PATELLA_TEST_IDLE
#define PATELLA_TEST_RSA_BLOCK(Block)
#define PATELLA_TEST_PACKET_SENT(Data, Size, Padded)
#define PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize)
#define PATELLA_TEST_FRAME_OBSERVERS(Frame, Pixels, Mask, Device)
#define PATELLA_MESSAGE_BOX(Window, Text, Caption, Type) MessageBoxA(Window, Text, Caption, Type)
#define PATELLA_TEST_CAPTURE_CHANGED
#define PATELLA_ENUM_DISPLAY_SETTINGS(Device, Index, Mode) EnumDisplaySettingsA(Device, Index, Mode)
#define PATELLA_TEST_PRESENT_FAILURE(Failure)
#define PATELLA_TEST_INIT_FAILURE(Step)
#define PATELLA_ENUM_WINDOWS(Function, Parameter) EnumWindows(Function, Parameter)

#endif
