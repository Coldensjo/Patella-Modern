// SdlFrame.h: the client's main window and event loop on SDL3 (Patella-Modern issue #122, plan M8; notes/decisions.md,
// "#122: the SDL3 window and its event loop"). Not part of the original source.
//
// The client's main window: the SDL3 counterpart of the original's CMainFrame (the frame window: its creation,
// placement, size, fullscreen switch, activation, closing and destruction) and of CWinApp::Run (the message loop, with
// the main timer in the loop instead of SetTimer). Player.cpp's CPlayerApp runs on it (since issue #127 the only frame:
// the original's handlers stay in tests/golden/mfc for the golden corpus). The window is SDL_CreateWindow's; on Windows its HWND is
// the client's window handle (the Vulkan engine's main window, TGUI's and TNetwork's window; the engine's surfaces are
// SDL_Vulkan_CreateSurface's since #126). Its input (issue #123): the wheel, motion and IME text events, the mouse
// capture (SDL_CaptureMouse), the cursors and the window's coordinates are SDL3's; on Windows the keys, the characters
// and the buttons stay the window's messages, because SDL's events differ from them (notes/decisions.md, "#123").
// Either way platform/KeyInput.h has the client's rules. The test mode's scripted wheel becomes the same SDL event
// (testmode/SdlInputWin32.cpp, through ScriptedInput); its other messages are dispatched to the window.
//
// No SDL header here: Player.cpp includes it, and the golden tests compile Player.cpp (their stubs stand in for these
// functions, tests/golden/client_stubs.cpp).

#pragma once

#include "common.h"
#include "platform/Window.h"

union SDL_Event; // SDL3's (SDL_events.h)

namespace patella::sdlframe {

// CMainFrame's constructor and Create (InitInstance's `new CMainFrame` and `Create`): SDL3 started (the window class
// registered as Title's and ClassName's, the client's own), the client's control initialised (InitControl, which may
// throw as in the constructor), the window made hidden, InitMainWindow and InitWindow called and the window placed as
// OnCreate does. False when SDL3 or its window cannot be made (InitInstance then returns FALSE, as after a failed
// Create).
[[nodiscard]] bool Create(bool Gamemaster, const char *Title, const char *ClassName);
// The window's handle (an HWND on Windows), or null before Create and after the window is gone.
[[nodiscard]] platform::TWindowHandle Handle();
// ShowWindow(SW_SHOWNORMAL or SW_SHOWMAXIMIZED) and UpdateWindow; nothing shown in a headless test client (#145).
void Show(bool Maximized);
// FatalError's ShowWindow(SW_HIDE): nothing without a window.
void Hide();
// The event loop (CWinApp::Run): until the window is closed and destroyed (OnClose, OnDestroy, the frame's
// destructor's ExitControl). Idle is CPlayerApp::OnIdle, called whenever the queue is empty. Returns the exit code.
int Run(void (*Idle)());

// CMainFrame::OnHostResolved for this window (issue #125): the name lookup is done, Error 0 or WinSock's number. The
// portable socket's Poll calls it on the SDL3 path (platform/NetworkWin32.cpp), where WSAAsyncGetHostByName posted
// WM_USER + 1.
void HostResolved(int Error);

// MainFrm.cpp's callbacks of Control.cpp and its UpdateMouseCursor, for this window.
void ExitProgram();
void SetDisplayMode(int ViewMode);
void OpenURL(const char *URL);
void UpdateMouseCursor();

// Issue #123: the window's coordinates. ScreenToClient and ClientToScreen with the client area's place on the screen as
// SDL knows it (SDL_GetWindowPosition, the client area's top left corner); nothing without a window.
void ScreenToClient(int &X, int &Y);
void ClientToScreen(int &X, int &Y);
// The test mode's scripted input (testmode/SdlInputWin32.cpp): an SDL event made from a script's window message (on
// Windows the wheel), handled as the desktop's events are, except that a key makes no character (the script's WM_CHAR
// is its character) and a button's click count is the message's (1, or 2 for a double click).
void ScriptedInput(const SDL_Event &Event);

#if !defined(_WIN32)
// Issue #131: the Linux path of the test mode's window messages (testmode/SdlInputWin32.cpp): what the window procedure
// does on Windows with a key, character, button, timer or close message. And the window's client area, GetClientRect's.
void WindowMessage(UINT Message, WPARAM WParam, LPARAM LParam);
void ClientSize(int &Width, int &Height);
#endif

} // namespace patella::sdlframe
