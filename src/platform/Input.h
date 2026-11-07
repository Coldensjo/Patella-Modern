// Input.h: IInput, the platform's input outside the window's messages (Patella-Modern issue #119, plan M7;
// notes/decisions.md, "#119: the platform interfaces, part 2"). Not part of the original source. Portable: no
// platform header.
//
// The keys, the buttons and the wheel come to the client as its window's messages (MainFrm.cpp's handlers) or, on the
// SDL3 window (issue #123), as SDL's events (SdlFrame.cpp). What it reads or starts besides goes through the process's
// IInput (platform/Platform.h): the mouse position, polled
// (Player.cpp InitInstance and OnIdle, MainFrm.cpp OnSetCursor and OnCaptureChanged); the main frame's 100 ms
// timer (MainFrm.cpp OnCreate, OnDestroy), whose ticks are input to the client (ActionNumber's); the idle step
// before the client reads them (CPlayerApp::OnIdle); and whether a loss of the mouse capture is the client's input
// (MainFrm.cpp OnCaptureChanged); and, on the SDL3 window, whether the desktop's keyboard and mouse events are the
// client's input (issue #123: the test build drops them with a script, as its message filter drops the messages).
//
//	TWin32Input   GetCursorPos, SetTimer, KillTimer (platform/Win32Platform.h); the idle step does nothing and every
//	              capture change and desktop event is input: Patella.exe's and the golden runner's (TSdlLoopInput, the
//	              SDL3 window's: the loop's timer, GetCursorPos until #127, issues #122, #123)
//	TTestInput    the test mode's (testmode/TestModeWin32.cpp): with a script the virtual cursor, the driver's ticks
//	              instead of the timer, the driver's step at each idle call, and the capture changes and the desktop's
//	              events dropped (issues #52, #123); without one the window path's (the control channel's probe at the
//	              idle step, issue #92)

#pragma once

#include "platform/Window.h"

namespace patella::platform {

// A point on the screen, in pixels.
struct TPoint {
	int x = 0;
	int y = 0;
};

class IInput {
public:
	IInput() = default;
	IInput(const IInput &) = delete;
	IInput &operator=(const IInput &) = delete;
	IInput(IInput &&) = delete;
	IInput &operator=(IInput &&) = delete;

	// The mouse cursor's position on the screen into Point (GetCursorPos): false, Point unchanged, where it cannot
	// be read.
	virtual bool CursorPosition(TPoint &Point) = 0;
	// The window's timer Id: a tick every Milliseconds (SetTimer; WM_TIMER with Id), until StopTimer (KillTimer).
	virtual void StartTimer(TWindowHandle Window, unsigned int Id, unsigned int Milliseconds) = 0;
	virtual void StopTimer(TWindowHandle Window, unsigned int Id) = 0;
	// The start of an idle call (CPlayerApp::OnIdle), before the client polls the cursor and handles its events.
	virtual void BeginIdle() = 0;
	// True where the window's loss of the mouse capture (WM_CAPTURECHANGED) is not the client's input, so that it
	// does not release the held buttons.
	[[nodiscard]] virtual bool IgnoreCaptureChange() = 0;
	// True where the desktop's keyboard and mouse events (SDL3's, issue #123) are not the client's input: the SDL3
	// window then hands them to nobody, as the test build's message filter (DropInput) drops Windows' input messages.
	[[nodiscard]] virtual bool IgnoreDesktopInput() = 0;

protected:
	// Never destroyed through the interface (Clock.h's IClock).
	~IInput() = default;
};

} // namespace patella::platform
