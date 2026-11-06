// Player.h: the MFC application class (c:\mud\tibiaplayer, so Player.cpp).
//
// The class name CPlayerApp follows MFC's wizard naming for the project
// "tibiaplayer"; the member names are ours. Layout: CWinApp (0xa0 bytes),
// then the two flags (the second is padding since issue #75).

#pragma once

#include "common.h"

class CPlayerApp : public CWinApp {
public:
	CPlayerApp();
	BOOL InitInstance() override;
	int Run() override;
	BOOL OnIdle(LONG lCount) override;
#if defined(PATELLA_TEST_MODE)
	// The test build only (src/TestHooks.h): drops the input from Windows while a script drives.
	BOOL PreTranslateMessage(MSG *pMsg) override;
#endif

	void FatalError(const char *Text, const char *Caption);
	void HandleProgramError(PROGRAM_ERROR Error);
	void HandleException(TMyException &Exception);

	bool Terminating; // +0xa0, a fatal error is being shown; later errors return at once
	// +0xa1 was RunPatch (Run started patch.exe when the message loop ended), gone with the patcher since
	// Patella-Modern issue #75 (decision D9): padding now, the size is the original's.

	DECLARE_MESSAGE_MAP()
};


extern CPlayerApp theApp; // 0x719dc0, initialiser at 0x55a030

void GetWindowTitle(char *Text);     // "Tibia   "
void GetWindowClassName(char *Text); // "TibiaClient"
