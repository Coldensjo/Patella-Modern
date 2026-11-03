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
//	PATELLA_TIME32(Time)               _time32, the wall clock: Talk.cpp, Creatures.cpp, ContentWindows.cpp,
//	                                   ConsoleWindow.cpp
//	PATELLA_TIME(Time)                 time, the wall clock: Player.cpp (error.txt)
//	PATELLA_FRAME_SLEEP(Milliseconds)  the frame timer's Sleep (WaitForNextFrame of the three engines)
//	PATELLA_GET_CURSOR_POS(Point)      GetCursorPos: the mouse, polled in OnIdle (Player.cpp, MainFrm.cpp)
//	PATELLA_SET_MAIN_TIMER(...)        the main frame's 100 ms timer (MainFrm.cpp OnCreate); with a script
//	                                   the driver makes its ticks
//	PATELLA_TEST_IDLE                  the driver's step, at the start of CPlayerApp::OnIdle (a statement
//	                                   without a semicolon, like TBEGIN; empty outside the test build)
//
// With a script, CPlayerApp::PreTranslateMessage (test build only) drops the keyboard and mouse input
// that comes from Windows, so the desktop's own mouse and keyboard cannot reach the client.

#pragma once

#if defined(PATELLA_TEST_MODE)

#include <ctime>

namespace patella::testmode {
DWORD TimeGetTime();
__time32_t WallTime32(__time32_t *Result);
time_t WallTime(time_t *Result);
void FrameSleep(DWORD Milliseconds);
BOOL GetCursorPosition(LPPOINT Point);
UINT_PTR SetMainTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function);
void Idle();
// True: the message is input from Windows that the driver's script replaces (dropped).
bool DropInput(const MSG *Message);
} // namespace patella::testmode

#define PATELLA_TIME_GET_TIME() patella::testmode::TimeGetTime()
#define PATELLA_TIME32(Time) patella::testmode::WallTime32(Time)
#define PATELLA_TIME(Time) patella::testmode::WallTime(Time)
#define PATELLA_FRAME_SLEEP(Milliseconds) patella::testmode::FrameSleep(Milliseconds)
#define PATELLA_GET_CURSOR_POS(Point) patella::testmode::GetCursorPosition(Point)
#define PATELLA_SET_MAIN_TIMER(Window, Id, Elapse, Function) patella::testmode::SetMainTimer(Window, Id, Elapse, Function)
#define PATELLA_TEST_IDLE patella::testmode::Idle();

#else

#define PATELLA_TIME_GET_TIME() timeGetTime()
#define PATELLA_TIME32(Time) _time32(Time)
#define PATELLA_TIME(Time) time(Time)
#define PATELLA_FRAME_SLEEP(Milliseconds) Sleep(Milliseconds)
#define PATELLA_GET_CURSOR_POS(Point) GetCursorPos(Point)
#define PATELLA_SET_MAIN_TIMER(Window, Id, Elapse, Function) ::SetTimer(Window, Id, Elapse, Function)
// No statement at all: an empty one is a nop in an unoptimised MSVC build (Debug), which would move
// the code after it.
#define PATELLA_TEST_IDLE

#endif
