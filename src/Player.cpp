// Player.cpp: reconstructed from Tibia.exe 7.72 (0x4d7cd0-0x4db29f).
//
// The application: start-up (InitInstance), the message loop (Run),
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
#include "GUI.h"
#include "graphics.h"
#include "Network.h"
#include "Preferences.h"
#include "SdlFrame.h"
#include "Status.h"
#include "Surface.h"
#include "Text.h"
#include "Utils.h"
#include "sysinc.h"
#include "common.h"
#include "log/Log.h"
#include "types/FixedString.h"
#include "log/LogSettings.h"
#include "platform/Platform.h"
#include "platform/Process.h"
#include <array>
#include <sstream>
#include <string>
#include "compat/msvc.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>

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
// Not in the original (Patella-Modern issue #127): OnIdle's local. Where Windows cannot read the cursor (another desktop:
// the lock screen, UAC's secure desktop; GetCursorPos fails) the point stays what it last was; the original read an
// uninitialised local there, in practice the stack slot's previous value.
POINT LastCursorPosition;

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
		const auto &PacketHeader = patella::bridge::Network().PacketHeader;
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

// Issue #123: ScreenToClient on the SDL3 window (its client area's place as SDL knows it).
void SdlScreenToClient(POINT &Point)
{
	int x = Point.x;
	int y = Point.y;
	patella::sdlframe::ScreenToClient(x, y);
	Point.x = x;
	Point.y = y;
}

} // namespace

CPlayerApp theApp; // 0x719dc0 // NOLINT(bugprone-throwing-static-initialization): the application object, constructed before main; a failure there ends the program

// 0x4d7cf0
CPlayerApp::CPlayerApp()
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
	} catch (...) { // NOLINT(bugprone-empty-catch): every step of the fatal path is guarded on its own, and the box still comes
	}
	try {
		// Patella-Modern issue #90: after a failed InitControl (CMainFrame's constructor) there is no main
		// window and, on x64, no surface (GetSurface makes one only once SurfaceInitInstance has run), and
		// MainFrame points to the freed frame: SetDisplayMode's calls would fault. Tibia.exe makes them (under
		// cdb its CWnd::ModifyStyle faults on the freed frame, 0xfeeefeee), and its catch (...) (MSVC 7.1: an
		// access violation as well) goes on to the box; under /EHsc it does not, so the step is skipped.
		if (GetViewMode() != 1 && patella::bridge::GUI().MainWindow != nullptr && GetSurface() != nullptr)
			patella::sdlframe::SetDisplayMode(1); // issue #122: the SDL3 window's
	} catch (...) {                               // NOLINT(bugprone-empty-catch): as above
	}
	try {
		// Patella-Modern issue #83: before InitInstance sets m_pMainWnd (a fatal error in its first Redraw: a
		// failing first present) there is no main window. Tibia.exe calls CWnd::ShowWindow on null there too, and
		// its catch (...) (MSVC 7.1: an access violation as well) goes on to the box; under /EHsc it does not.
		patella::sdlframe::Hide(); // issue #122: the SDL3 window's ShowWindow(SW_HIDE)
	} catch (...) {                // NOLINT(bugprone-empty-catch): as above
	}
	// MessageBoxA with MB_ICONHAND (issue #119: the platform's IWindow).
	patella::platform::Platform().Window.ShowMessageBox(nullptr, Text, Caption, patella::platform::TMessageBoxIcon::Error);
	// Issue #108: the main window stays as the original's global did, allocated and not closed. TGUI owns it, and
	// TClient's static destruction after exit(1) would destroy it and close its children (packets, preferences).
	patella::bridge::GUI().AbandonWindows();
	exit(1);
}

// 0x4d9140, line 62. Text needs 9 bytes.
void GetWindowTitle(char *Text)
{
	PATELLA_ASSERT(Text != nullptr);
	for (int i = 0; i < 8; i++)
		Text[i] = (char)(TitleCode[i] ^ TitleKey[i]);
	Text[8] = 0;
}

// 0x4d92b0, line 75. Text needs 12 bytes.
void GetWindowClassName(char *Text)
{
	PATELLA_ASSERT(Text != nullptr);
	for (int i = 0; i < 11; i++)
		Text[i] = (char)(ClassNameCode[i] ^ ClassNameKey[i]);
	Text[11] = 0;
}

namespace {

// 0x4d9420 (missed by Ghidra). EnumWindows callback: counts the windows
// whose title or class matches the client's (caseless), this one included.
// Since Patella-Modern issue #119 the platform's IWindow::VisitWindows calls it
// with the window's title and class (TWin32Window: the original's
// GetWindowTextA and GetClassNameA with 30, platform/Win32Platform.cpp).
void CountTibiaWindows(const char *WindowText, int TextLength, const char *WindowClass, int ClassLength)
{
	TFixedString<32> Title;
	TFixedString<32> ClassName;
	GetWindowTitle(Title);
	GetWindowClassName(ClassName);
	if ((TextLength > 0 && CompareStrings(WindowText, Title, -1) == 0)
		|| (ClassLength > 0 && CompareStrings(WindowClass, ClassName, -1) == 0))
		NumberOfTibiaWindows++;
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

namespace {

// Not in the original (Patella-Modern issue #122): InitInstance's steps after the command line on the SDL3 window
// (SdlFrame.h), in the same order as the original's: the frame (CMainFrame's constructor and Create), the surfaces, the
// count of the Tibia windows and its two boxes, the first Redraw, the window shown, the first mouse position. Its
// exceptions go to InitInstance's handlers. The icon is the window class's (SDL_RegisterApp with resource 128).
BOOL InitSdlWindow(bool Gamemaster, const char *ClassName)
{
	TFixedString<56> Title;
	GetWindowTitle(Title);
	if (!patella::sdlframe::Create(Gamemaster, Title, ClassName))
		return FALSE;
	try {
		SurfaceInitInstance(patella::sdlframe::Handle());
		GetSurface()->InitSurfaces();
		InitVideoCache();
	} catch (...) {
		throw TProgramError{PROGRAM_ERROR::SURFACE}; // "Video surface allocation failed."
	}
	if (!Gamemaster) {
		NumberOfTibiaWindows = 0;
		patella::platform::Platform().Window.VisitWindows(CountTibiaWindows); // EnumWindows (issue #119)
		if (NumberOfTibiaWindows == 0) {
			patella::platform::Platform().Window.ShowMessageBox(nullptr, "Application is corrupted! Please re-install Tibia.",
				"Tibia - Error", patella::platform::TMessageBoxIcon::Warning); // MB_ICONEXCLAMATION
			patella::sdlframe::ExitProgram();
			return FALSE;
		}
		if (NumberOfTibiaWindows > 1) {
			patella::platform::Platform().Window.ShowMessageBox(nullptr, "A Tibia client is already running!", "Tibia - Error",
				patella::platform::TMessageBoxIcon::Warning); // MB_ICONEXCLAMATION
			patella::sdlframe::ExitProgram();
			return FALSE;
		}
	}
	Redraw(false);
	int x, y, Width, Height;
	bool Maximized;
	GetWindowedMode(&x, &y, &Width, &Height, &Maximized);
	patella::sdlframe::Show(Maximized);
	patella::platform::ReadCursorPosition(patella::platform::Platform().Input, LastMousePosition); // GetCursorPos
	if (GetViewMode() == 1)
		SdlScreenToClient(LastMousePosition); // issue #123: the SDL window's coordinates
	MouseMove(LastMousePosition.x, LastMousePosition.y);
	return TRUE;
}

} // namespace

// 0x4da3e0, lines 255-347. Registers the window class, parses the command
// line, creates the frame window and the surfaces, and refuses to run twice
// (except for gamemasters). Odd behaviour kept: "host" and "port" are
// recognised but their values are dropped (the test server is always
// test.cipsoft.com:7172), and after an error InitInstance still returns TRUE.
// A PROGRAM_ERROR shows its code, any other error is turned into a
// TMyException for HandleException (patella::ThrowTopLevelError, TMyException.h:
// the original's handlers). The original read error.txt first and
// posted event 0x2a at the end (the error report dialog); both are gone
// since issue #74.
BOOL CPlayerApp::InitInstance()
{
	// Not in the original (Patella-Modern issue #130, D14): the data folder and Patella.ini's folder (the working
	// folder where it has Tibia.dat, as the original's files were).
	patella::platform::ConfigureDataFiles();
	// Not in the original (Patella-Modern issue #77, D16): the developer log's level and file from Patella.ini.
	patella::log::LoadSettings(patella::platform::Platform().FileSystem); // issue #119: through the platform's files
	patella::platform::LogDataFiles();
#if defined(PATELLA_PLACEHOLDER_RESOURCES)
	// Issue #127: the build-without-Tibia.exe's neutral pictures (cmake/WindowsX64.cmake); never a release's.
	patella::log::Warn("this build has placeholder cursors and icon, not the original's (hosted CI's build)");
#endif
	try {
		try {
			TFixedString<32> ClassName; // the SDL3 window's class is SDL_RegisterApp's (SdlFrame.cpp), of this name
			GetWindowClassName(ClassName);

			TFixedString<1024> Arguments;
			strncpy(Arguments, CommandLine, sizeof(Arguments) - 1); // the last byte is set next (GCC's -Wstringop-truncation, issue #131)
			Arguments[sizeof(Arguments) - 1] = 0;
			const char *Token = strtok(Arguments, " \t\n");
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
					case 2: // port: not used
						Expected = 0;
						break;
					case 3:
						patella::bridge::GraphicsEngine().CommandLineEngine = atoi(Token); // NOLINT(bugprone-unchecked-string-to-number-conversion): the original's; a bad number counts as 0
						Expected = 0;
						break;
					default:
						break;
					}
				}
				Token = strtok(nullptr, " \t\n");
			}

			return InitSdlWindow(Gamemaster, ClassName);
		} catch (const TProgramError &Error) {
			HandleProgramError(Error.Code);
		} catch (...) {
			patella::ThrowTopLevelError(PATELLA_SOURCE_LOCATION());
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
	const int Result = 0;
	try {
		try {
			// Issue #122: the SDL3 window's loop (SdlFrame.cpp), whose end is the end of the program (exit code 0, as
			// CWinApp::ExitInstance's result was after MFC's own loop had stopped).
			patella::sdlframe::Run([] { theApp.OnIdle(); });
		} catch (const TProgramError &Error) {
			HandleProgramError(Error.Code);
		} catch (...) {
			patella::ThrowTopLevelError(PATELLA_SOURCE_LOCATION());
		}
	} catch (TMyException &Exception) {
		HandleException(Exception);
	}
	return Result;
}

// 0x4d94c0, line 456. Forwards mouse moves and cursor changes, then
// handles the event queue (Control.cpp). The original's returned TRUE (MFC's
// idle count argument and CWinApp::OnIdle's own housekeeping went with MFC, issue #127).
void CPlayerApp::OnIdle()
{
	// Issue #125: the socket's events and lookups, where the message loop had just dispatched WinSock's messages.
	patella::platform::Platform().Socket.Poll();
	patella::platform::Platform().Input.BeginIdle();                                                // issue #119: the test mode's driver step (TestHooks.h until then)
	patella::platform::ReadCursorPosition(patella::platform::Platform().Input, LastCursorPosition); // GetCursorPos (issue #119)
	POINT Point = LastCursorPosition;
	if (GetViewMode() == 1)
		SdlScreenToClient(Point); // issue #123: the SDL window's coordinates
	if (LastMousePosition.x != Point.x || LastMousePosition.y != Point.y) {
		LastMousePosition.x = Point.x;
		LastMousePosition.y = Point.y;
		MouseMove(Point.x, Point.y);
	}
	const int Cursor = GetMouseCursor();
	if (Cursor != LastMouseCursor) {
		patella::sdlframe::UpdateMouseCursor(); // issue #122: the SDL3 window's
		LastMouseCursor = Cursor;
	}
	ProcessEvents();
}
