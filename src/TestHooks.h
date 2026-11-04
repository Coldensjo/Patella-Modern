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
//	PATELLA_TIME32(Time)               _time32 (MsvcTime32, compat/msvc.h), the wall clock: Talk.cpp, Creatures.cpp, ContentWindows.cpp,
//	                                   ConsoleWindow.cpp
//	PATELLA_TIME(Time)                 time, the wall clock: Player.cpp (error.txt)
//	PATELLA_FRAME_SLEEP(Milliseconds)  the frame timer's Sleep (WaitForNextFrame of the three engines)
//	PATELLA_GET_CURSOR_POS(Point)      GetCursorPos: the mouse, polled in OnIdle (Player.cpp, MainFrm.cpp)
//	PATELLA_SET_MAIN_TIMER(...)        the main frame's 100 ms timer (MainFrm.cpp OnCreate); with a script
//	                                   the driver makes its ticks
//	PATELLA_TEST_IDLE                  the driver's step, at the start of CPlayerApp::OnIdle (a statement
//	                                   without a semicolon, like TBEGIN; empty outside the test build)
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
// The state dump reads the client's data through the View functions at the end of Creatures.cpp,
// Inventory.cpp, Map.cpp and Status.cpp (src/testmode/ClientView.h; test build only).
//
// With a script, CPlayerApp::PreTranslateMessage (test build only) drops the keyboard and mouse input
// that comes from Windows, so the desktop's own mouse and keyboard cannot reach the client.

#pragma once

#if defined(PATELLA_TEST_MODE)

#include <ctime>

#include "testmode/ClientView.h" // IWYU pragma: export

namespace patella::testmode {
DWORD TimeGetTime();
Time32 WallTime32(Time32 *Result);
time_t WallTime(time_t *Result);
void FrameSleep(DWORD Milliseconds);
BOOL GetCursorPosition(LPPOINT Point);
UINT_PTR SetMainTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function);
void Idle();
// True: the message is input from Windows that the driver's script replaces (dropped).
bool DropInput(const MSG *Message);
void RsaBlock(const unsigned char *Block);
void PacketSent(const unsigned char *Data, int Size, int Padded);
void PacketReceived(const unsigned char *Data, int PacketSize);
void FrameObservers(HMODULE Patcap, int(PATELLA_CDECL **Frame)(int, int), void(PATELLA_CDECL **Pixels)(const void *, int, int, int),
	void(PATELLA_CDECL **Mask)(const unsigned char *, int, int));
int ShowMessageBox(HWND Window, const char *Text, const char *Caption, UINT Type);
// True: a WM_CAPTURECHANGED that the script's input does not know of (with a script: every one).
bool IgnoreCaptureChange();
} // namespace patella::testmode

#define PATELLA_TIME_GET_TIME() patella::testmode::TimeGetTime()
#define PATELLA_TIME32(Time) patella::testmode::WallTime32(Time)
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

#else

#define PATELLA_TIME_GET_TIME() timeGetTime()
#define PATELLA_TIME32(Time) MsvcTime32(Time)
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

#endif
