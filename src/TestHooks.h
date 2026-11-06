// TestHooks.h: the places where the test mode reaches into the client (issue #21;
// notes/modernization-plan.md, 4.4). Included by common.h.
//
// In the product and oracle builds every hook is the system call it replaces, word for word, so they
// compile to the same code and contain nothing of the test mode. The test build (PatellaTest.exe,
// cmake/WindowsX64.cmake) defines PATELLA_TEST_MODE: the hooks go to src/testmode/TestModeWin32.cpp,
// which sends the clocks through one IClock (src/testmode/Clock.h) and, with a script
// (--test-script <file>), drives the client from src/testmode/InputScript.h:
//
//	PATELLA_TIME_GET_TIME()            timeGetTime: sysinc.cpp (GetMilliseconds, srand), the frame timers
//	PATELLA_TIME(Time)                 time, the wall clock in 64 bits: Talk.cpp,
//	                                   Creatures.cpp, ContentWindows.cpp, ConsoleWindow.cpp (issue #62: no
//	                                   _time32 any more; the 4-byte fields convert through types/Time32.h)
//	PATELLA_FRAME_SLEEP(Milliseconds)  the frame timer's Sleep (WaitForNextFrame of the three engines)
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
//	PATELLA_TEST_FRAME_OBSERVERS(Patcap, Frame, Pixels, Mask)
//	                                   vulkan/SurfaceVulkan.cpp InitDisplay: without patcap.dll, the test
//	                                   mode's frame dump becomes the engine's frame observer
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
#include <ctime>

#include "compat/msvc.h"
#include "compat/span.h"
#include "testmode/ClientView.h" // IWYU pragma: export

namespace patella::testmode {
DWORD TimeGetTime();
time_t WallTime(time_t *Result);
void FrameSleep(DWORD Milliseconds);
BOOL GetCursorPosition(LPPOINT Point);
UINT_PTR SetMainTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function);
void Idle();
// True: the message is input from Windows that the driver's script replaces (dropped).
bool DropInput(const MSG *Message);
// The buffers as spans (issue #60): the RSA block, and the whole of SendData and ReceiveData.
void RsaBlock(patella::TSpan<const unsigned char, 128> Block);
void PacketSent(patella::TSpan<const unsigned char> Data, int Size, int Padded);
void PacketReceived(patella::TSpan<const unsigned char> Data, int PacketSize);
void FrameObservers(HMODULE Patcap, int(PATELLA_CDECL **Frame)(int, int), void(PATELLA_CDECL **Pixels)(const void *, int, int, int),
	void(PATELLA_CDECL **Mask)(const unsigned char *, int, int));
int ShowMessageBox(HWND Window, const char *Text, const char *Caption, UINT Type);
// True: a WM_CAPTURECHANGED that the script's input does not know of (with a script: every one).
bool IgnoreCaptureChange();
// EnumDisplaySettingsA, or the --test-display-modes list (Device null, Index a mode's number).
BOOL EnumDisplayMode(LPCSTR Device, DWORD Index, DEVMODEA *Mode);
// True: PATELLA_TEST_FAIL_PRESENT is 1 (issue #83), every present fails.
bool PresentFails();
} // namespace patella::testmode

#define PATELLA_TIME_GET_TIME() patella::testmode::TimeGetTime()
#define PATELLA_TIME(Time) patella::testmode::WallTime(Time)
#define PATELLA_FRAME_SLEEP(Milliseconds) patella::testmode::FrameSleep(Milliseconds)
#define PATELLA_GET_CURSOR_POS(Point) patella::testmode::GetCursorPosition(Point)
#define PATELLA_SET_MAIN_TIMER(Window, Id, Elapse, Function) patella::testmode::SetMainTimer(Window, Id, Elapse, Function)
#define PATELLA_TEST_IDLE patella::testmode::Idle();
#define PATELLA_TEST_RSA_BLOCK(Block) patella::testmode::RsaBlock(Block);
#define PATELLA_TEST_PACKET_SENT(Data, Size, Padded) patella::testmode::PacketSent(Data, Size, Padded);
#define PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize) patella::testmode::PacketReceived(Data, PacketSize);
#define PATELLA_TEST_FRAME_OBSERVERS(Patcap, Frame, Pixels, Mask) patella::testmode::FrameObservers(Patcap, &(Frame), &(Pixels), &(Mask));
#define PATELLA_MESSAGE_BOX(Window, Text, Caption, Type) patella::testmode::ShowMessageBox(Window, Text, Caption, Type)
#define PATELLA_TEST_CAPTURE_CHANGED \
	if (patella::testmode::IgnoreCaptureChange()) { \
		Default(); \
		return; \
	}
#define PATELLA_ENUM_DISPLAY_SETTINGS(Device, Index, Mode) patella::testmode::EnumDisplayMode(Device, Index, Mode)
#define PATELLA_TEST_PRESENT_FAILURE(Failure) \
	if (patella::testmode::PresentFails()) { Failure; }

#else

#define PATELLA_TIME_GET_TIME() timeGetTime()
#define PATELLA_TIME(Time) time(Time)
#define PATELLA_FRAME_SLEEP(Milliseconds) Sleep(Milliseconds)
#define PATELLA_GET_CURSOR_POS(Point) GetCursorPos(Point)
#define PATELLA_SET_MAIN_TIMER(Window, Id, Elapse, Function) ::SetTimer(Window, Id, Elapse, Function)
// No statement at all: an empty one is a nop in an unoptimised MSVC build (Debug), which would move
// the code after it.
#define PATELLA_TEST_IDLE
#define PATELLA_TEST_RSA_BLOCK(Block)
#define PATELLA_TEST_PACKET_SENT(Data, Size, Padded)
#define PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize)
#define PATELLA_TEST_FRAME_OBSERVERS(Patcap, Frame, Pixels, Mask)
#define PATELLA_MESSAGE_BOX(Window, Text, Caption, Type) MessageBoxA(Window, Text, Caption, Type)
#define PATELLA_TEST_CAPTURE_CHANGED
#define PATELLA_ENUM_DISPLAY_SETTINGS(Device, Index, Mode) EnumDisplaySettingsA(Device, Index, Mode)
#define PATELLA_TEST_PRESENT_FAILURE(Failure)

#endif
