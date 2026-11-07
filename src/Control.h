// Control.cpp: the client's main control flow. Graphics engine changes,
// redrawing the screen, mouse and keyboard input, the timer, the event queue
// that opens dialogs and starts connections, the network callbacks with the
// packet dispatcher, and start-up and shutdown of all modules.
// Function names are ours; parameter names come from the binary's catch
// strings ("engine", "Force", "x", "y", "Key").
//
// Since Patella-Modern issue #118 the functions are TGUI's members (GUI.h), defined in Control.cpp; these free
// functions of the original's names forward to them for the callers outside TGUI (MainFrm.cpp, Player.cpp, the test
// mode), InitControl's callbacks and the golden tests, which call and fake them by name.

#pragma once

#include "common.h"
#include "platform/Window.h"

#include <array>

void Resize(int Width, int Height);

void CheckSurface(bool Force);
void ChangeEngine(int engine);
void Redraw(bool Force);
void EnterGame();
void LeaveGame();

void MouseMove(int x, int y);
void LeftButtonDown(int x, int y, int Flags);
void LeftButtonUp(int x, int y, int Flags);
void LeftButtonDoubleClick(int x, int y, int Flags);
void RightButtonDown(int x, int y, int Flags);
void RightButtonUp(int x, int y, int Flags);
void MouseWheel(int x, int y, int Direction);
void KeyPress(int Key);
bool CloseRequest();

void HandleEvents();
void LinkCopyWarning();
void ConnectionEstablished();
void ReceivePackets();
void ConnectionClosed();
void TimerEvent();
void ProcessEvents();

void InitWindow(patella::platform::TWindowHandle Window, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode), void (*OpenURL)(const char *URL));
void InitControl(bool Gamemaster);
void ExitControl();

// Not in the original (Patella-Modern issue #77): the last ten packet types received, the newest first (0 for none),
// for the developer log's diagnostics (Player.cpp HandleException; error.txt's "Last Packet Types" until issue #74).
std::array<uint8, 10> GetLastPacketTypes();
