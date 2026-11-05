// Player.cpp: reconstructed from Tibia.exe 7.72 (0x4d7cd0-0x4db29f).
//
// The MFC application: start-up (InitInstance), the message loop (Run),
// idle processing, and the top-level error handling, which ends the client
// with the critical error box. The original also wrote the error to error.txt
// and read it back at the next start for the error report dialog; that
// report is gone (Patella-Modern issue #74, decision D2): the message goes to
// stderr instead.
//
// The range also holds code the linker kept in this object that is not part
// of the source: CWinApp's destructor, AfxGetMainWnd, <fstream>/<sstream>
// and <string> members, the ofstream/ifstream constructors and destructors
// (0x4d9700-0x4d9bbf) and TMyException::GetOrigin. They have names.tsv rows
// of kind stl.
//
// Order: the functions without line evidence first, in binary order
// (message map, constructor, FatalError), then the two text decoders
// (lines 62, 75), the window counter for EnumWindows, the error handlers
// (binary order, before their first user; the error.txt reader that followed
// them, 0x4da140, is gone since issue #74), then
// InitInstance (lines 255-347), Run (383-408) and OnIdle (456). The
// compiler placed OnIdle right after the decoders.

#include "Player.h"
#include "Control.h"
#include "MainFrm.h"
#include "graphics.h"
#include "Network.h"
#include "Preferences.h"
#include "Status.h"
#include "Surface.h"
#include "Text.h"
#include "Utils.h"
#include "sysinc.h"
#include "common.h"
#include "types/FixedString.h"
#include <array>
#include <sstream>
#include <string>
#include <new>
#include <stdexcept>
#include "compat/msvc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>

namespace {

// The window title and class name are stored XOR-encoded. The key of each
// string is in this object's .rdata (0x56dd98, 0x56ddb8); the other half,
// TitleCode and ClassNameCode, is sysinc.cpp's (0x5733a4, 0x5733c4).
const std::array<int, 8> TitleKey = {16, 34, 227, 56, 3, 110, 64, 173};                         // 0x56dd98
const std::array<int, 11> ClassNameKey = {109, 168, 131, 83, 237, 53, 175, 191, 138, 154, 253}; // 0x56ddb8

// Data, in .data/.bss order.
int LastMouseCursor = 1;  // 0x5c2518
int NumberOfTibiaWindows; // 0x719db0, counted by CountTibiaWindows
POINT LastMousePosition;  // 0x719db4

// Not in the original (Patella-Modern issue #74, decision D2): a fatal error's text on stderr, the three parts and a
// line break in one write. A GUI client's stderr is valid only when it is redirected; a failed write is ignored, and
// nothing here throws.
void WriteToStderr(const char *First, const char *Second, const char *Third) noexcept
{
	try {
		std::string Text;
		for (const char *Part : {First, Second, Third})
			if (Part != nullptr)
				Text += Part;
		Text += '\n';
		std::fwrite(Text.data(), 1, Text.size(), stderr);
		std::fflush(stderr);
	} catch (...) {
	}
}

} // namespace

CPlayerApp theApp; // 0x719dc0

// 0x4d7cd0: CPlayerApp has no message handlers of its own.
BEGIN_MESSAGE_MAP(CPlayerApp, CWinApp) // NOLINT(modernize-avoid-c-arrays): MFC's macro declares the message table (issue #60)
END_MESSAGE_MAP()

// 0x4d7cf0
CPlayerApp::CPlayerApp()
	: CWinApp(nullptr)
{
	Terminating = false;
}

// 0x4d7d80. Every step is guarded on its own; the program ends here. this
// is passed but not used. Since issue #74 (D2) the box's caption and text go
// to stderr first, the fork's record of a fatal error.
void CPlayerApp::FatalError(const char *Text, const char *Caption)
{
	WriteToStderr(Caption, ": ", Text);
	try {
		ShutdownConnection();
	} catch (...) {
	}
	try {
		if (GetViewMode() != 1)
			SetDisplayMode(1);
	} catch (...) {
	}
	try {
		AfxGetMainWnd()->ShowWindow(SW_HIDE);
	} catch (...) {
	}
	PATELLA_MESSAGE_BOX(nullptr, Text, Caption, MB_ICONHAND);
	exit(1);
}

// 0x4d9140, line 62. Text needs 9 bytes.
void GetWindowTitle(char *Text)
{
	TASSERT(62, Text!=NULL);
	for (int i = 0; i < 8; i++)
		Text[i] = (char)(TitleCode[i] ^ TitleKey[i]);
	Text[8] = 0;
}

// 0x4d92b0, line 75. Text needs 12 bytes.
void GetWindowClassName(char *Text)
{
	TASSERT(75, Text!=NULL);
	for (int i = 0; i < 11; i++)
		Text[i] = (char)(ClassNameCode[i] ^ ClassNameKey[i]);
	Text[11] = 0;
}

namespace {

// 0x4d9420 (missed by Ghidra). EnumWindows callback: counts the windows
// whose title or class matches the client's (caseless), this one included.
BOOL CALLBACK CountTibiaWindows(HWND Window, [[maybe_unused]] LPARAM Parameter)
{
	TFixedString<32> Title;
	TFixedString<32> WindowText;
	TFixedString<32> ClassName;
	TFixedString<32> WindowClass;
	const int TextLength = GetWindowTextA(Window, WindowText, 30);
	const int ClassLength = GetClassNameA(Window, WindowClass, 30);
	GetWindowTitle(Title);
	GetWindowClassName(ClassName);
	if ((TextLength > 0 && CompareStrings(WindowText, Title, -1) == 0)
		|| (ClassLength > 0 && CompareStrings(WindowClass, ClassName, -1) == 0))
		NumberOfTibiaWindows++;
	return TRUE;
}

} // namespace

// 0x4d9910. Shows the error code with its texts and ends the program. If
// a fatal error is already being shown it only sets Terminating again.
void CPlayerApp::HandleProgramError(PROGRAM_ERROR Error)
{
	if (Terminating) {
		Terminating = true;
		return;
	}
	std::stringstream Message;
	Message << GetErrorText(Error) << " (Error Code " << Error << ")\r\n\r\n"
			<< GetErrorHint(Error);
	FatalError(Message.str().c_str(), "Tibia Error");
}

// 0x4d9bd0. Ends the program with the critical error box. The original
// appended a report to error.txt first (the client version, the exception's
// origin, the time, the Windows version, the engine, the last packets, the
// player's position and the message), which ReadErrorFile read back at the
// next start for the error report dialog and Control.cpp sent to the server
// as packet 0xe8. That report is gone (Patella-Modern issue #74, decision D2):
// the exception's message goes to stderr, then FatalError's box (whose text,
// TextCriticalError, no longer mentions the error file) ends the client.
void CPlayerApp::HandleException(TMyException &Exception)
{
	if (Terminating) {
		Terminating = true;
		return;
	}
	try {
		WriteToStderr("Debug Assertion 7.72 ", Exception.GetMessage().c_str(), "");
	} catch (...) {
	}
	FatalError(TextCriticalError, "Tibia Error - Debug Assertion");
}

// 0x4da3e0, lines 255-347. Registers the window class, parses the command
// line, creates the frame window and the surfaces, and refuses to run twice
// (except for gamemasters). Odd behaviour kept: "host" and "port" are
// recognised but their values are dropped (the test server is always
// test.cipsoft.com:7172), and after an error InitInstance still returns TRUE.
// A PROGRAM_ERROR shows its code, any other error is turned into a
// TMyException for HandleException. The original read error.txt first and
// posted event 0x2a at the end (the error report dialog); both are gone
// since issue #74.
BOOL CPlayerApp::InitInstance()
{
	try {
		try {
			TFixedString<32> ClassName;
			GetWindowClassName(ClassName);
			WNDCLASSA WindowClass = {0};
			WindowClass.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			WindowClass.lpfnWndProc = AfxGetAfxWndProc();
			WindowClass.hInstance = AfxGetInstanceHandle();
			WindowClass.lpszClassName = ClassName;
			WindowClass.hIcon = ::LoadIconA(AfxGetResourceHandle(), MAKEINTRESOURCEA(128));
			AfxRegisterClass(&WindowClass);

			TFixedString<1024> CommandLine;
			strncpy(CommandLine, m_lpCmdLine, sizeof(CommandLine));
			CommandLine[sizeof(CommandLine) - 1] = 0;
			const char *Token = strtok(CommandLine, " \t\n");
			int Expected = 0;
			bool Gamemaster = false;
			SetTestServerAddress("test.cipsoft.com");
			SetTestServerPort(7172);
			while (Token != nullptr) {
				if (MsvcStricmp(Token, "gamemaster") == 0) {
					Gamemaster = true;
				} else if (MsvcStricmp(Token, "host") == 0) {
					Expected = 1;
				} else if (MsvcStricmp(Token, "port") == 0) {
					Expected = 2;
				} else if (MsvcStricmp(Token, "engine") == 0) {
					Expected = 3;
				} else {
					switch (Expected) {
					case 1: // host name: not used
						Expected = 0;
						break;
					case 2: // port: not used
						Expected = 0;
						break;
					case 3:
						CommandLineEngine = atoi(Token);
						Expected = 0;
						break;
					}
				}
				Token = strtok(nullptr, " \t\n");
			}

			CMainFrame *MainFrame;
			MainFrame = new CMainFrame(Gamemaster);
			TFixedString<56> Title;
			GetWindowTitle(Title);
			if (!MainFrame->Create(nullptr, Title, WS_OVERLAPPEDWINDOW, CFrameWnd::rectDefault, nullptr, nullptr, 0, nullptr)) {
				delete MainFrame;
				return FALSE;
			}
			try {
				SurfaceInitInstance(&MainFrame->m_hWnd);
				GetSurface()->InitSurfaces();
				InitVideoCache();
			} catch (...) {
				throw (PROGRAM_ERROR)12; // "Video surface allocation failed."
			}
			if (!Gamemaster) {
				NumberOfTibiaWindows = 0;
				EnumWindows(CountTibiaWindows, 0);
				if (NumberOfTibiaWindows == 0) {
					PATELLA_MESSAGE_BOX(nullptr, "Application is corrupted! Please re-install Tibia.", "Tibia - Error", MB_ICONEXCLAMATION);
					ExitProgram();
					return FALSE;
				}
				if (NumberOfTibiaWindows > 1) {
					PATELLA_MESSAGE_BOX(nullptr, "A Tibia client is already running!", "Tibia - Error", MB_ICONEXCLAMATION);
					ExitProgram();
					return FALSE;
				}
			}
			Redraw(false);
			SendMessageA(MainFrame->m_hWnd, WM_SETICON, ICON_BIG,
				reinterpret_cast<LPARAM>(::LoadIconA(AfxGetResourceHandle(), MAKEINTRESOURCEA(128))));
			m_pMainWnd = MainFrame;
			int x, y, Width, Height;
			bool Maximized;
			GetWindowedMode(&x, &y, &Width, &Height, &Maximized);
			m_pMainWnd->ShowWindow(Maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
			m_pMainWnd->UpdateWindow();
			PATELLA_GET_CURSOR_POS(&LastMousePosition);
			if (GetViewMode() == 1)
				ScreenToClient(m_pMainWnd->GetSafeHwnd(), &LastMousePosition);
			MouseMove(LastMousePosition.x, LastMousePosition.y);
			return TRUE;
		} catch (PROGRAM_ERROR Error) {
			HandleProgramError(Error);
		} catch (TMyException &Exception_) {
			throw TMYEXCEPTION(322, "exception occurred", "", 0, "", 0, Exception_);
		} catch (std::bad_alloc &) {
			TERROR(335, "bad_alloc_exception occurred");
		} catch (std::out_of_range &) {
			TERROR(338, "out_of_range_exception occurred");
		} catch (std::exception &) {
			TERROR(341, "exception occurred");
		} catch (const char *) {
			TERROR(344, "const char* exception occurred");
		} catch (...) {
			TERROR(347, "unknown exception occurred");
		}
	} catch (TMyException &Exception) {
		HandleException(Exception);
	}
	return TRUE;
}

// 0x4dae60, lines 383-408. The same handlers as InitInstance around the
// message loop. Tibia.exe's Run then started patch.exe (_execl) when CloseProgram had set RunPatch: gone with the
// patcher since Patella-Modern issue #75 (decision D9).
int CPlayerApp::Run()
{
	int Result = 0;
	try {
		try {
			Result = CWinApp::Run();
		} catch (PROGRAM_ERROR Error) {
			HandleProgramError(Error);
		} catch (TMyException &Exception_) {
			throw TMYEXCEPTION(383, "exception occurred", "", 0, "", 0, Exception_);
		} catch (std::bad_alloc &) {
			TERROR(396, "bad_alloc_exception occurred");
		} catch (std::out_of_range &) {
			TERROR(399, "out_of_range_exception occurred");
		} catch (std::exception &) {
			TERROR(402, "exception occurred");
		} catch (const char *) {
			TERROR(405, "const char* exception occurred");
		} catch (...) {
			TERROR(408, "unknown exception occurred");
		}
	} catch (TMyException &Exception) {
		HandleException(Exception);
	}
	return Result;
}

// 0x4d94c0, line 456. Forwards mouse moves and cursor changes, then
// handles the event queue (Control.cpp).
BOOL CPlayerApp::OnIdle(LONG lCount)
{
	PATELLA_TEST_IDLE
	CWinApp::OnIdle(lCount);
	POINT Point;
	PATELLA_GET_CURSOR_POS(&Point);
	if (GetViewMode() == 1)
		ScreenToClient(m_pMainWnd->GetSafeHwnd(), &Point);
	if (LastMousePosition.x != Point.x || LastMousePosition.y != Point.y) {
		LastMousePosition.x = Point.x;
		LastMousePosition.y = Point.y;
		MouseMove(Point.x, Point.y);
	}
	const int Cursor = GetMouseCursor();
	if (Cursor != LastMouseCursor) {
		UpdateMouseCursor();
		LastMouseCursor = Cursor;
	}
	ProcessEvents();
	return TRUE;
}

#if defined(PATELLA_TEST_MODE)
// The test build only (src/TestHooks.h, src/testmode/TestModeWin32.cpp).
BOOL CPlayerApp::PreTranslateMessage(MSG *pMsg)
{
	if (patella::testmode::DropInput(pMsg))
		return TRUE;
	return CWinApp::PreTranslateMessage(pMsg);
}
#endif
