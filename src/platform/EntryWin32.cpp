// EntryWin32.cpp: the Windows client's entry point (Patella-Modern issue #127, plan M8; notes/decisions.md, "#127").
// Not part of the original source, where MFC's WinMain (AfxWinMain) was the entry and called CPlayerApp's InitInstance
// and Run. Patella.exe and PatellaTest.exe compile it; the golden tests, which have main, do not. What AfxWinMain did
// that the client relies on: the program's critical errors without Windows' own boxes; the command line
// (m_lpCmdLine); InitInstance, and Run unless InitInstance returned FALSE (the exit code is then CWinApp's ExitInstance's:
// 0 after a start refused with one of InitInstance's boxes).

#include "Player.h"
#include "common.h"

int WINAPI WinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE Previous, _In_ LPSTR CommandLine, _In_ int ShowCommand)
{
	(void)Instance;
	(void)Previous;
	(void)ShowCommand;
	// AfxWinInit's.
	SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
	theApp.CommandLine = CommandLine;
	if (theApp.InitInstance() == FALSE)
		return 0;
	return theApp.Run();
}
