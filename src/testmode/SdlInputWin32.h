// SdlInputWin32.h: the test mode's scripted window messages as the SDL3 window's input (Patella-Modern issue #123,
// plan M8; notes/testmode.md, "The two window paths"; notes/decisions.md, "#123: input over SDL3"). Not part of the
// original source. Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake).
//
// The driver's events and the reference recordings' `post` lines are Win32 window messages (the MFC frame gets them
// with DispatchMessageA). On the SDL3 window, whose wheel is SDL's events since issue #123, those messages become the
// SDL event SDL makes of the same input, handed to the window as a scripted event (sdlframe::ScriptedInput):
//
//	WM_MOUSEWHEEL    SDL_EVENT_MOUSE_WHEEL: wParam's distance in notches (120 each)
//	WM_MOUSEMOVE     nothing: the frame had no handler, and a dispatched move makes no WM_SETCURSOR
//
// The key messages, WM_CHAR and the button messages stay window messages on Windows (SdlFrame.cpp's OnKeyMessage,
// OnWindowChar and OnButtonMessage: SDL's key, text and button events differ from them), so they are dispatched, as is
// every other message (WM_TIMER, WM_CLOSE ...).

#pragma once

#include "common.h"

namespace patella::testmode {

// True where Message was input and went to the SDL3 window as its SDL event; false for any other message.
bool DeliverToSdlWindow(UINT Message, WPARAM WParam, LPARAM LParam);

} // namespace patella::testmode
