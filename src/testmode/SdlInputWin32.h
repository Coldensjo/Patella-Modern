// SdlInputWin32.h: the test mode's scripted window messages as the SDL3 window's input (Patella-Modern issue #123,
// plan M8; notes/testmode.md, "The two window paths"; notes/decisions.md, "#123: input over SDL3"). Not part of the
// original source. Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake).
//
// The driver's events and the reference recordings' `post` lines are Win32 window messages (the MFC frame gets them
// with DispatchMessageA). On the SDL3 window, whose input is SDL's events since issue #123, each input message becomes
// the SDL event SDL makes of the same input, handed to the window as a scripted event (sdlframe::ScriptedInput):
//
//	WM_KEYDOWN, WM_SYSKEYDOWN    SDL_EVENT_KEY_DOWN: the scan code of the key's position (the virtual key with the
//	WM_KEYUP, WM_SYSKEYUP        extended flag: the keypad's keys without it; NumLock on for the keypad's digits only),
//	                             repeat from lParam's previous-state bit; SDL_EVENT_KEY_UP
//	WM_CHAR                      SDL_EVENT_TEXT_INPUT: the byte (the client's Windows-1252) as UTF-8, control
//	                             characters too (the script's own characters: a scripted key makes none)
//	WM_LBUTTONDOWN/UP/DBLCLK,    SDL_EVENT_MOUSE_BUTTON_DOWN/UP at lParam's client point, 1 click (2 for a double
//	WM_RBUTTON...                click)
//	WM_MOUSEWHEEL                SDL_EVENT_MOUSE_WHEEL: wParam's distance in notches (120 each)
//	WM_MOUSEMOVE                 nothing: the frame had no handler, and a dispatched move makes no WM_SETCURSOR
//
// Every other message (WM_TIMER, WM_CLOSE, WM_SYSCHAR ...) is not input here: the caller dispatches it as before.

#pragma once

#include "common.h"

namespace patella::testmode {

// True where Message was input and went to the SDL3 window as its SDL event; false for any other message.
bool DeliverToSdlWindow(UINT Message, WPARAM WParam, LPARAM LParam);

} // namespace patella::testmode
