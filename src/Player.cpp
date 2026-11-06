// Player.cpp: reconstructed from Tibia.exe 7.72 (0x4d7cd0-0x4db29f).
//
// The MFC application: start-up (InitInstance), the message loop (Run),
// idle processing, and the top-level error handling, which ends the client
// with the critical error box. The original also wrote the error to error.txt
// and read it back at the next start for the error report dialog; that
// report is gone (Patella-Modern issue #74, decision D2): the message goes to
// the developer log instead (issue #77, D16: log/Log.h; stderr, and a log file
// when Patella.ini names one), with the report's diagnostics at the debug level.
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
#include "Creatures.h"
#include "MainFrm.h"
#include "MainWindow.h"
#include "graphics.h"
#include "Network.h"
#include "Preferences.h"
#include "Status.h"
#include "Surface.h"
#include "Text.h"
#include "Utils.h"
#include "sysinc.h"
#include "common.h"
#include "log/Log.h"
#include "types/FixedString.h"
#if defined(PATELLA_VULKAN)
#include "log/LogSettings.h"
#endif
#include <array>
#include <sstream>
#include <string>
#include <new>
#include <stdexcept>
#include "compat/msvc.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>

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

// Not in the original (Patella-Modern issue #77, D16): bytes as error.txt listed them until issue #74, " 016 000 ...",
// each as three digits.
template <std::size_t N>
std::string ByteList(const std::array<uint8, N> &Bytes)
{
	std::string Text;
	for (const uint8 Byte : Bytes) {
		TFixedString<8> Help;
		Help.Format(" {:03}", static_cast<unsigned int>(Byte));
		Text += Help.View();
	}
	return Text;
}

// Not in the original (Patella-Modern issue #77, D16): at the debug level, what HandleException's report in error.txt
// held besides the message until issue #74: the engine, the last ten packet types (the newest first), the first bytes
// of the last packet when there was one, and the player's position in the game. A part that fails is left out.
void LogDiagnostics() noexcept
{
	if (!patella::log::Enabled(patella::log::TLevel::Debug))
		return;
	try {
		patella::log::Debug("Graphic engine: {}", GetCurrentEngine());
	} catch (...) { // NOLINT(bugprone-empty-catch): this part left out
	}
	try {
		patella::log::Debug("Last packet types:{}", ByteList(GetLastPacketTypes()));
	} catch (...) { // NOLINT(bugprone-empty-catch): this part left out
	}
	try {
		bool Any = false;
		for (const uint8 Byte : PacketHeader)
			Any = Any || Byte != 0;
		if (Any)
			patella::log::Debug("Last packet:{}", ByteList(PacketHeader));
	} catch (...) { // NOLINT(bugprone-empty-catch): this part left out
	}
	try {
		if (GetPlayerID() != 0) {
			const TCreature *Player = GetCreature(GetPlayerID());
			if (Player != nullptr)
				patella::log::Debug("Player position: [{},{},{}]", Player->RealX, Player->RealY, Player->RealZ);
		}
	} catch (...) { // NOLINT(bugprone-empty-catch): this part left out
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
// to stderr first, the fork's record of a fatal error; since issue #77 (D16)
// through the developer log, at the error level. Debug builds then write the
// stack (std::stacktrace, where the standard library has it) and stop in an
// attached debugger; without a debugger they go on as Release builds do.
void CPlayerApp::FatalError(const char *Text, const char *Caption)
{
	patella::log::Error("{}: {}", Caption != nullptr ? Caption : "", Text != nullptr ? Text : "");
#if !defined(NDEBUG)
	patella::log::WriteStacktrace(patella::log::Log());
	patella::log::BreakIfDebugged();
#endif
	try {
		ShutdownConnection();
	} catch (...) {
	}
	try {
		// Patella-Modern issue #90: after a failed InitControl (CMainFrame's constructor) there is no main
		// window and, on x64, no surface (GetSurface makes one only once SurfaceInitInstance has run), and
		// MainFrame points to the freed frame: SetDisplayMode's calls would fault. Tibia.exe makes them (under
		// cdb its CWnd::ModifyStyle faults on the freed frame, 0xfeeefeee), and its catch (...) (MSVC 7.1: an
		// access violation as well) goes on to the box; under /EHsc it does not, so the step is skipped.
		if (GetViewMode() != 1 && MainWindow != nullptr && GetSurface() != nullptr)
			SetDisplayMode(1);
	} catch (...) {
	}
	try {
		// Patella-Modern issue #83: before InitInstance sets m_pMainWnd (a fatal error in its first Redraw: a
		// failing first present) there is no main window. Tibia.exe calls CWnd::ShowWindow on null there too, and
		// its catch (...) (MSVC 7.1: an access violation as well) goes on to the box; under /EHsc it does not.
		CWnd *Window = AfxGetMainWnd();
		if (Window != nullptr)
			Window->ShowWindow(SW_HIDE);
	} catch (...) {
	}
	PATELLA_MESSAGE_BOX(nullptr, Text, Caption, MB_ICONHAND);
	exit(1);
}

// 0x4d9140, line 62. Text needs 9 bytes.
void GetWindowTitle(char *Text)
{
	PATELLA_ASSERT(Text!=NULL);
	for (int i = 0; i < 8; i++)
		Text[i] = (char)(TitleCode[i] ^ TitleKey[i]);
	Text[8] = 0;
}

// 0x4d92b0, line 75. Text needs 12 bytes.
void GetWindowClassName(char *Text)
{
	PATELLA_ASSERT(Text!=NULL);
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
	const int Code = static_cast<int>(Error); // an enum class since issue #80
	std::stringstream Message;
	Message << GetErrorText(Code) << " (Error Code " << Code << ")\r\n\r\n"
			<< GetErrorHint(Code);
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
// Since issue #77 (D16) the message goes through the developer log at the
// error level, and the report's other lines at the debug level (LogDiagnostics).
void CPlayerApp::HandleException(TMyException &Exception)
{
	if (Terminating) {
		Terminating = true;
		return;
	}
	try {
		patella::log::Error("Debug Assertion 7.72 {}", Exception.GetMessage());
	} catch (...) { // NOLINT(bugprone-empty-catch): GetMessage's std::bad_alloc; the box still ends the client
	}
	LogDiagnostics();
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
#if defined(PATELLA_VULKAN)
	// Not in the original (Patella-Modern issue #77, D16): the developer log's level and file from Patella.ini.
	patella::log::LoadSettings();
#endif
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
				throw TProgramError{PROGRAM_ERROR::SURFACE}; // "Video surface allocation failed."
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
		} catch (const TProgramError &Error) {
			HandleProgramError(Error.Code);
		} catch (TMyException &Exception_) {
			throw TMYEXCEPTION("exception occurred", "", 0, "", 0, Exception_);
		} catch (std::bad_alloc &) {
			TERROR("bad_alloc_exception occurred");
		} catch (std::out_of_range &) {
			TERROR("out_of_range_exception occurred");
		} catch (std::exception &) {
			TERROR("exception occurred");
		} catch (const char *) {
			TERROR("const char* exception occurred");
		} catch (...) {
			TERROR("unknown exception occurred");
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
		} catch (const TProgramError &Error) {
			HandleProgramError(Error.Code);
		} catch (TMyException &Exception_) {
			throw TMYEXCEPTION("exception occurred", "", 0, "", 0, Exception_);
		} catch (std::bad_alloc &) {
			TERROR("bad_alloc_exception occurred");
		} catch (std::out_of_range &) {
			TERROR("out_of_range_exception occurred");
		} catch (std::exception &) {
			TERROR("exception occurred");
		} catch (const char *) {
			TERROR("const char* exception occurred");
		} catch (...) {
			TERROR("unknown exception occurred");
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
