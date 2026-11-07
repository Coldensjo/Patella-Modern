// SdlFrame.h: the client's main window and event loop on SDL3 (Patella-Modern issue #122, plan M8; notes/decisions.md,
// "#122: the SDL3 window and its event loop"). Not part of the original source.
//
// The SDL3 counterpart of MainFrm.cpp's CMainFrame (the frame window: its creation, placement, size, fullscreen
// switch, activation, closing and destruction) and of CWinApp::Run (the message loop, with the main timer in the loop
// instead of SetTimer). Player.cpp's CPlayerApp uses it when platform/WindowChoice.h's UseSdlWindow() holds (the
// default; PATELLA_WINDOW=mfc keeps MFC's until issue #127). The window is SDL_CreateWindow's; on Windows its HWND is
// the client's window handle (the Vulkan engine's main window, TGUI's and TNetwork's window; the engine's surfaces are
// SDL_Vulkan_CreateSurface's since #126), and its keys, buttons and wheel still come as Win32 window messages (a subclass of the
// SDL window, the handlers of MainFrm.cpp's message map), so that the test mode's scripted messages reach it as they
// reach the MFC frame: SDL3's input is issue #123.
//
// No SDL header here: Player.cpp includes it, and the golden tests compile Player.cpp (their stubs stand in for these
// functions, tests/golden/client_stubs.cpp).

#pragma once

#include "platform/Window.h"

namespace patella::sdlframe {

// CMainFrame's constructor and Create (InitInstance's `new CMainFrame` and `Create`): SDL3 started (the window class
// registered as Title's and ClassName's, the client's own), the client's control initialised (InitControl, which may
// throw as in the constructor), the window made hidden, InitMainWindow and InitWindow called and the window placed as
// OnCreate does. False when SDL3 or its window cannot be made (InitInstance then returns FALSE, as after a failed
// Create).
[[nodiscard]] bool Create(bool Gamemaster, const char *Title, const char *ClassName);
// The window's handle (an HWND on Windows), or null before Create and after the window is gone.
[[nodiscard]] platform::TWindowHandle Handle();
// ShowWindow(SW_SHOWNORMAL or SW_SHOWMAXIMIZED) and UpdateWindow.
void Show(bool Maximized);
// FatalError's ShowWindow(SW_HIDE): nothing without a window.
void Hide();
// The event loop (CWinApp::Run): until the window is closed and destroyed (OnClose, OnDestroy, the frame's
// destructor's ExitControl). Idle is CPlayerApp::OnIdle, called whenever the queue is empty, with MFC's idle count
// (0 after a message, then counting up). Returns the exit code.
int Run(void (*Idle)(long Count));

// CMainFrame::OnHostResolved for this window (issue #125): the name lookup is done, Error 0 or WinSock's number. The
// portable socket's Poll calls it on the SDL3 path (platform/NetworkWin32.cpp), where WSAAsyncGetHostByName posted
// WM_USER + 1.
void HostResolved(int Error);

// MainFrm.cpp's callbacks of Control.cpp and its UpdateMouseCursor, for this window.
void ExitProgram();
void SetDisplayMode(int ViewMode);
void OpenURL(const char *URL);
void UpdateMouseCursor();

} // namespace patella::sdlframe
