// Player.h: the application class (c:\mud\tibiaplayer, so Player.cpp).
//
// The class name CPlayerApp is the original's, MFC's wizard naming for the
// project "tibiaplayer"; the member names are ours. Since Patella-Modern issue
// #127 it derives from nothing: the process's entry is WinMain
// (platform/EntryWin32.cpp), which does what MFC's did and calls InitInstance
// and Run.

#pragma once

#include "common.h"

class CPlayerApp final {
public:
	CPlayerApp();
	BOOL InitInstance();
	int Run();
	void OnIdle();

	void FatalError(const char *Text, const char *Caption);
	void HandleProgramError(PROGRAM_ERROR Error);
	void HandleException(TMyException &Exception);

	// The original's byte after Terminating was RunPatch (Run started patch.exe when the message loop ended), gone with
	// the patcher since Patella-Modern issue #75 (decision D9).
	bool Terminating;             // a fatal error is being shown; later errors return at once
	const char *CommandLine = ""; // WinMain's (CWinApp::m_lpCmdLine until issue #127)
};


extern CPlayerApp theApp; // 0x719dc0, initialiser at 0x55a030

void GetWindowTitle(char *Text);     // "Tibia   "
void GetWindowClassName(char *Text); // "TibiaClient"
