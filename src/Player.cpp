// Player.cpp: reconstructed from Tibia.exe 7.72 (0x4d7cd0-0x4db29f).
//
// The MFC application: start-up (InitInstance), the message loop (Run),
// idle processing, and the top-level error handling, which writes error.txt
// and reads it back at the next start for the error report dialog.
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
// and the error.txt reader (binary order, before their first user), then
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
#include "compat/crt71.h"
#include "common.h"
#include <fstream>
#include <sstream>
#include <string>
#include <new>
#include <stdexcept>
#include <ctime>
#include <process.h>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <ios>
#include <ostream>

// The window title and class name are stored XOR-encoded. The key of each
// string is in this object's .rdata (0x56dd98, 0x56ddb8); the other half,
// TitleCode and ClassNameCode, is sysinc.cpp's (0x5733a4, 0x5733c4).
static const int TitleKey[8] = {16, 34, 227, 56, 3, 110, 64, 173};                         // 0x56dd98
static const int ClassNameKey[11] = {109, 168, 131, 83, 237, 53, 175, 191, 138, 154, 253}; // 0x56ddb8

// Data, in .data/.bss order.
static int LastMouseCursor = 1;  // 0x5c2518
static int NumberOfTibiaWindows; // 0x719db0, counted by CountTibiaWindows
static POINT LastMousePosition;  // 0x719db4
CPlayerApp theApp;               // 0x719dc0

// 0x4d7cd0: CPlayerApp has no message handlers of its own.
BEGIN_MESSAGE_MAP(CPlayerApp, CWinApp)
END_MESSAGE_MAP()

// 0x4d7cf0
CPlayerApp::CPlayerApp()
	: CWinApp(nullptr)
{
	Terminating = false;
	RunPatch = false;
}

// 0x4d7d80. Every step is guarded on its own; the program ends here. this
// is passed but not used.
void CPlayerApp::FatalError(const char *Text, const char *Caption)
{
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

// 0x4d9420 (missed by Ghidra). EnumWindows callback: counts the windows
// whose title or class matches the client's (caseless), this one included.
static BOOL CALLBACK CountTibiaWindows(HWND Window, LPARAM Parameter)
{
	char Title[32];
	char WindowText[32];
	char ClassName[32];
	char WindowClass[32];
	int TextLength = GetWindowTextA(Window, WindowText, 30);
	int ClassLength = GetClassNameA(Window, WindowClass, 30);
	GetWindowTitle(Title);
	GetWindowClassName(ClassName);
	if ((TextLength > 0 && CompareStrings(WindowText, Title, -1) == 0)
		|| (ClassLength > 0 && CompareStrings(WindowClass, ClassName, -1) == 0))
		NumberOfTibiaWindows++;
	return TRUE;
}

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

// 0x4d9bd0. Appends a report to error.txt and ends the program; the report
// is read back by ReadErrorFile at the next start. Each part of the report
// is guarded on its own. Odd behaviour kept: the service pack text is
// copied into a 30-byte buffer without a terminator when it has 30
// characters or more, and the dashes at the end have no line break.
void CPlayerApp::HandleException(TMyException &Exception)
{
	if (Terminating) {
		Terminating = true;
		return;
	}
	std::ofstream Out("error.txt", std::ios::out | std::ios::app);
	if (Out.is_open()) {
		Out << std::endl
			<< "+---------------------------------------------------------------" << std::endl
			<< "Debug Assertion " << "7.72" << " " << Exception.GetOrigin();
		time_t Now = PATELLA_TIME(nullptr);
		Out << std::endl
			<< CRT71Time(ctime(&Now)); // the day as MSVC 7.1 writes it
		OSVERSIONINFOA Version;
		memset(&Version, 0, sizeof(Version));
		Version.dwOSVersionInfoSize = sizeof(Version);
		try {
#pragma warning(suppress : 4996) // deprecated in newer SDKs
			if (!GetVersionExA(&Version)) {
				Out << "Windows Version: (error)" << std::endl;
			} else {
				Out << "Windows Version: " << Version.dwMajorVersion << "." << Version.dwMinorVersion
					<< " build " << Version.dwBuildNumber << " on " << Version.dwPlatformId;
				if (strlen(Version.szCSDVersion) != 0) {
					// Characters other than letters, digits, ' ', '.', '-' and
					// '_' become '*'.
					char ServicePack[30] = {0};
					try {
						unsigned int Length = (unsigned int)strlen(Version.szCSDVersion);
						if (Length > 30)
							Length = 30;
						for (unsigned int i = 0; i < Length; i++) {
							char c = Version.szCSDVersion[i];
							if (IsDigit(c) || IsAlpha(c) || c == ' ' || c == '.' || c == '-' || c == '_')
								ServicePack[i] = c;
							else
								ServicePack[i] = '*';
						}
						Out << " with " << ServicePack;
					} catch (...) {
					}
				}
				Out << std::endl;
			}
		} catch (...) {
			Out << "Windows Version: (exception)" << std::endl;
		}
		try {
			Out << "Graphic Engine: " << GetCurrentEngine() << std::endl;
		} catch (...) {
			Out << "Graphic Engine: (exception)" << std::endl;
		}
		try {
			OutputControlErrorInformations(Out);
		} catch (...) {
			Out << "OutputControlErrorInformations: (exception)" << std::endl;
		}
		if (m_pMainWnd != nullptr) {
			try {
				((CMainFrame *)m_pMainWnd)->OutputErrorInformations(Out);
			} catch (...) {
				Out << "OutputErrorInformations: (exception)" << std::endl;
			}
		}
		Out << Exception.GetMessage() << std::endl
			<< "----------------------------------------------------------------";
		Out.close();
	}
	FatalError(TextCriticalError, "Tibia Error - Debug Assertion");
}

// 0x4da140. Reads the reports in error.txt into the error report list
// (Status.cpp): a line starting with '+' begins a report, the next line is
// its title, the one after its date, the rest its text up to a line
// starting with '-'. A line that is exactly "Player Position: [" or
// "Windows Version: " (cut off when the file was written) gets
// " (truncated input line)"; a read error replaces the line.
void CPlayerApp::ReadErrorFile()
{
	if (!FileExists("error.txt"))
		return;
	std::ifstream In("error.txt", std::ios::in);
	if (In.is_open()) {
		int State = 0;
		std::string Line;
		while (!In.eof() && !In.bad()) {
			std::getline(In, Line);
			if (In.bad())
				Line = " (bad input file)";
			if (Line.length() == 0)
				continue;
			if (Line.compare(0, Line.length(), "Player Position: [", 18) == 0)
				Line += " (truncated input line)";
			if (Line.compare(0, Line.length(), "Windows Version: ", 17) == 0)
				Line += " (truncated input line)";
			if (Line.c_str()[0] == '-') {
				State = 0;
			} else if (Line.c_str()[0] == '+') {
				State = 1;
			} else if (State == 1) {
				AddErrorReport(Line.c_str());
				State = 2;
			} else if (State == 2) {
				SetErrorReportDate(Line.c_str());
				State = 3;
			} else if (State == 3) {
				AddErrorReportText(Line.c_str());
			}
		}
		In.close();
	}
}

// 0x4da3e0, lines 255-347. Reads error.txt, registers the window class,
// parses the command line, creates the frame window and the surfaces, and
// refuses to run twice (except for gamemasters). Odd behaviour kept: "host"
// and "port" are recognised but their values are dropped (the test server
// is always test.cipsoft.com:7172), and after an error InitInstance still
// returns TRUE. A PROGRAM_ERROR shows its code, any other error is turned
// into a TMyException and written to error.txt.
BOOL CPlayerApp::InitInstance()
{
	try {
		try {
			ReadErrorFile();
			char ClassName[32];
			GetWindowClassName(ClassName);
			WNDCLASSA WindowClass = {0};
			WindowClass.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			WindowClass.lpfnWndProc = AfxGetAfxWndProc();
			WindowClass.hInstance = AfxGetInstanceHandle();
			WindowClass.lpszClassName = ClassName;
			WindowClass.hIcon = ::LoadIconA(AfxGetResourceHandle(), MAKEINTRESOURCEA(128));
			AfxRegisterClass(&WindowClass);

			char CommandLine[1024];
			strncpy(CommandLine, m_lpCmdLine, sizeof(CommandLine));
			CommandLine[sizeof(CommandLine) - 1] = 0;
			char *Token = strtok(CommandLine, " \t\n");
			int Expected = 0;
			bool Gamemaster = false;
			SetTestServerAddress("test.cipsoft.com");
			SetTestServerPort(7172);
			while (Token != nullptr) {
				if (_stricmp(Token, "gamemaster") == 0) {
					Gamemaster = true;
				} else if (_stricmp(Token, "host") == 0) {
					Expected = 1;
				} else if (_stricmp(Token, "port") == 0) {
					Expected = 2;
				} else if (_stricmp(Token, "engine") == 0) {
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
			TBEGIN
			MainFrame = new CMainFrame(Gamemaster);
			TEND(255)
			char Title[56];
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
			TBEGIN
			Redraw(false);
			TEND(296)
			SendMessageA(MainFrame->m_hWnd, WM_SETICON, ICON_BIG,
				(LPARAM)::LoadIconA(AfxGetResourceHandle(), MAKEINTRESOURCEA(128)));
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
			AddMessage(0x2a, 0, 0);
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
// message loop. patch.exe replaces the client process when RunPatch is set.
int CPlayerApp::Run()
{
	int Result = 0;
	try {
		try {
			Result = CWinApp::Run();
			if (RunPatch)
				_execl("patch.exe", "patch.exe", nullptr);
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
	TBEGIN
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
	int Cursor = GetMouseCursor();
	if (Cursor != LastMouseCursor) {
		UpdateMouseCursor();
		LastMouseCursor = Cursor;
	}
	ProcessEvents();
	return TRUE;
	TEND(456)
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
