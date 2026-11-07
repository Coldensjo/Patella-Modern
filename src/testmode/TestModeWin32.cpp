// TestModeWin32.cpp: the test mode of the Windows test build (issue #21; notes/modernization-plan.md,
// 4.4; src/TestHooks.h). Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake); Patella.exe
// contains none of it.
//
//	PatellaTest.exe [--test-script <file>] [--test-log <file>] [--test-out <folder>]
//	                [--test-replay <file>] [--test-display-modes <file>] [--test-control <name>]
//	                [the client's own words]
//
// Without --test-script the test build is the client on the system clock (issue #109: the platform's
// TWin32Clock: timeGetTime, time, Sleep). With a script (src/testmode/InputScript.h):
//
// - the clock is virtual (TVirtualClock, src/testmode/Clock.h): timeGetTime starts at 100000 and the
//   wall clock at 2026-11-02 12:00:00 UTC, as tools/capture/patcap.c's, so srand(timeGetTime()) gets
//   the same seed (and the client the same XTEA keys) in every run; the frame timer does not sleep
//   (a real millisecond, so that the idle loop does not spin);
// - the main frame's 100 ms timer is not created; the driver dispatches its WM_TIMER ticks at every
//   multiple of 100 virtual ms;
// - the driver runs at the start of CPlayerApp::OnIdle, which MFC calls whenever the message queue is
//   empty (and the client's OnIdle always asks for more). Each call either dispatches the script's
//   next due event (one per call, so that the client handles it, and polls a mouse move, before the
//   next), or, when none is due, advances the clock to the next tick or event once the real time
//   for it has come (pace: real milliseconds per 100 virtual ms, default 100, real time);
// - events become the window messages Windows would send (WM_KEYDOWN/UP or WM_SYSKEYDOWN/UP with
//   the scan code in lParam, WM_CHAR, WM_LBUTTONDOWN/UP/DBLCLK, WM_RBUTTON..., WM_MOUSEWHEEL, WM_CLOSE
//   posted for quit), dispatched with DispatchMessageA like the message loop's; the mouse is a
//   virtual cursor that GetCursorPos returns (the client polls it in OnIdle);
// - the keyboard and mouse input from Windows is dropped (CPlayerApp::PreTranslateMessage), so the
//   desktop's own mouse and keyboard do not reach the client during a run;
// - --test-log <file> lists every event dispatched with its virtual time and script line.
//
// Issue #22, the dumps (notes/testmode.md), into the folder of --test-out:
//
// - packets.log: every packet the client sends, before encryption, and every packet it decrypts
//   (src/testmode/PacketLog.h; the hooks in Network.cpp and Communication.cpp); with or without a
//   script;
// - state-<NNN>.json at the script's dump commands: client_state.py's state() and gui(), read in the
//   process (src/testmode/StateDump.h, ClientStateWin32.cpp);
// - frame-<NNN>.png and frame-<NNN>.filtered.png at the script's frame commands: the next frame the
//   Vulkan engine shows and its device-filtered pixels (src/testmode/Png.h), through the engine's frame
//   observer (the only one since issue #97); frames.txt lists them.
//
// Issue #23, the reference recordings (notes/testmode.md, "Reference recordings"):
//
// - --test-replay <file> (src/testmode/ServerReplay.h): the server side of a recording, without a
//   server. The driver listens on the file's ports on 127.0.0.1, takes each connection's XTEA key from
//   the client's RSA block, and sends the recorded server packets and closes when they are due (their
//   virtual time has come and the client packets recorded before them have come). It holds the clock
//   and the script while the network is busy: while the client resolves or connects (connection state
//   1, 2, 5), while a client packet has not reached the replay, while a server packet has not been
//   decrypted by the client, while a server close has not been answered by the client's close (each
//   at most HoldLimit real milliseconds). So every packet comes at the same point of the script in
//   every run, whatever the real time;
// - the scripts' clock manual, advance, cursor and post (src/testmode/InputScript.h): the time course
//   of tools/capture/patcap.c, under which Tibia.exe made the recording;
// - with a script, the client's message boxes (FatalError's and the start's, Player.cpp) are not
//   shown: their caption and text go to messages.txt in --test-out (and the log), and the box counts
//   as answered with OK.
//
// Issue #48: --test-display-modes <file> (src/testmode/DisplayModes.h), with or without a script: the
// client's EnumDisplaySettings(nullptr, Index) (the Graphics Options dialog's list, vulkan/SurfaceVulkan.cpp
// GetFullscreenModes) answers from the file instead of the machine, so a reference session lists the
// modes of the machine it was recorded on. Without it the test build asks Windows, as Patella.exe.
//
// A script with dump or frame needs --test-out. (Until issue #97 --test-sync <name> made the client wait
// at each state dump for testmode_check.py's reading from outside; no tool used it after issue #92.)
//
// Issue #92: --test-control <name> (tools/testmode_client.py), the control channel through which the
// comparison tools drive and read a running test client instead of reading its memory or hooking its
// frames (tools/client_state.py, tools/capture/patcap.c: Tibia.exe's). The tool makes the events
// <name>-command and <name>-done, writes control.txt into --test-out and sets <name>-command; at an idle
// call the driver reads it, does it, writes control-reply.txt ("ok" or "error: <why>") and sets
// <name>-done. A command is:
//
// - probe: probe.json (StateDump.h, "The probe"): the state dump's state and window tree and what the
//   tools' drivers read besides (characters, skin values, components' members, the map's names, the Vulkan
//   engine's device and frames), with or without a script;
// - with a script, any lines of a script (InputScript.h; with clock manual none that moves the clock but
//   advance, and no pace, clock or cursor): they come after the script's last event, at the clock's time,
//   and the command is done once every one was handed out, the idle calls after it have passed and its
//   frame dumps were taken. A tool drives the client so with a script of clock manual and cursor only
//   (patcap's time course, tools/compare_rendering.py) and posts its window messages as post lines.
//
// Without a script the client takes Windows' input as Patella.exe does (tools/compare_traffic.py and
// compare_files.py post their messages to its window) and the channel answers probe only. A command is
// taken only once the one before it is done. With a script the frame observer is installed for the
// channel's frame lines.
//
// Issue #83: PATELLA_TEST_FAIL_PRESENT=1 in the environment, with or without a script, makes every present of
// the Vulkan engine fail as a failing vkQueuePresentKHR does (PresentFails, TestHooks.h), so the first frame's
// ends the client on its fatal path (PROGRAM_ERROR 12, "Tibia Error").
//
// Issue #90: PATELLA_TEST_FAIL_INIT=<n> (1-16) in the environment makes InitControl throw PROGRAM_ERROR 6 before
// its step n (16: after the last), so the client ends on InitControl's fatal path ("Tibia Error", "(Error Code
// 6)") with the steps before it done (InitFails, TestHooks.h).
//
// Issue #96: InitInstance's count of the Tibia windows (CountTibiaWindows, Player.cpp: "A Tibia client is already
// running!" with more than one, "Application is corrupted!" with none) sees only this process's windows
// (EnumOwnWindows, TestHooks.h), so another client on the machine (another worktree's test, a parallel CTest test,
// a Tibia.exe) cannot stop a test client. Patella.exe counts every window, as the original.
//
// A script that does not parse ends the program before the window opens, with exit code 3 and the
// message on stderr, in the log and through OutputDebugString (never a dialog).

#include "common.h"
#include "compat/span.h"

#include "MainFrm.h"
#include "testmode/ClientStateWin32.h"
#include "testmode/Clock.h"
#include "testmode/DisplayModes.h"
#include "testmode/InputScript.h"
#include "testmode/PacketLog.h"
#include "testmode/Png.h"
#include "testmode/ServerReplay.h"
#include "testmode/StateDump.h"
#include "platform/Platform.h"
#include "platform/Win32Platform.h"
#include "types/File.h"

#include <shellapi.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <deque>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(PATELLA_TEST_MODE)
#error "TestModeWin32.cpp belongs to the test build (PATELLA_TEST_MODE)"
#endif

namespace patella::testmode {

namespace {

// Exit code of a script that cannot be read or does not parse.
constexpr UINT ScriptErrorExitCode = 3;

void RealYield()
{
	::Sleep(1);
}

// The modal loop of a message box that is not shown (ShowMessageBox): it ends after this many real
// milliseconds without a message or a replay event, or after BoxMilliseconds.
constexpr long long BoxQuietMilliseconds = 500;
constexpr long long BoxMilliseconds = 5000;

// Idle calls after every input event and network action of a script with a manual clock (issue #23).
constexpr int QuietPasses = 10;

// How long the replay holds the clock for one network event before it gives up waiting (issue #23).
constexpr long long HoldLimit = 10000;

// A connection of the client to the server replay (issue #23).
struct TReplaySocket {
	SOCKET Socket = INVALID_SOCKET;
	std::string Kind;
	int Index = -1; // the recorded connection (TServerReplay), -1: none
	std::array<std::uint8_t, 16> Key{};
	bool HasKey = false;
	std::vector<std::uint8_t> In;  // bytes from the client, not yet a whole frame
	std::vector<std::uint8_t> Out; // bytes to the client, not yet sent
	int Unread = 0;                // frames sent that the client has not decrypted yet
	bool ServerClosing = false;    // shut down for sending; the client's close is awaited
	bool Closed = false;           // closed by the client (or failed)
	int Frames = 0;                // client frames read
};

struct TReplayState {
	bool Active = false;
	TServerReplay Schedule;
	std::vector<std::pair<SOCKET, std::string>> Listeners;
	std::vector<TReplaySocket> Sockets;
	std::array<std::uint8_t, 16> PendingKey{};
	bool HasPendingKey = false;
	std::uint64_t ClientSent = 0; // frames the client sent (the hook)
	std::uint64_t ReplayRead = 0; // frames the replay read
	bool Holding = false;
	bool IgnoreConnecting = false;
	std::chrono::steady_clock::time_point HoldStart;
	std::string HoldReason;
	int Sent = 0;
	int GivenUp = 0;
};

// A frame dump asked for by the script, not yet shown.
struct TPendingFrame {
	int Step = 0;
	int Line = 0;
	std::uint32_t Requested = 0;
};

struct TState {
	TVirtualClock Virtual{RealYield};
	IClock *Clock = &platform::Win32Clock(); // the system's clock (issue #109: the platform's own)
	bool Scripted = false;
	TScheduler Scheduler;
	POINT Cursor = {-1, -1}; // client coordinates
	TFile Log;               // --test-log, text: "\r\n" (never closed: TState lives until the process ends)
	bool Started = false;
	std::chrono::steady_clock::time_point RealStart;
	bool FinishReported = false;

	// Issue #22: the dumps.
	std::filesystem::path Out; // --test-out; empty: no dumps
	TFile Packets;
	TConnections Connections;
	std::array<unsigned char, RsaBlockSize> Rsa{};
	bool HasRsa = false;
	bool PacketClockStarted = false;
	std::chrono::steady_clock::time_point PacketClockStart;
	int StateSteps = 0;
	int FrameSteps = 0;
	bool FramesWanted = false; // the script has frame commands: observe the Vulkan engine
	std::deque<TPendingFrame> PendingFrames;
	std::vector<std::uint8_t> Mask;
	int MaskWidth = 0;
	int MaskHeight = 0;

	// Issue #92: the control channel (--test-control).
	HANDLE ControlCommand = nullptr;
	HANDLE ControlDone = nullptr;
	bool ControlBusy = false; // a command's script lines are being handed out
	int Probes = 0;
	TProbeDevice Device;
	std::uint64_t FramesShown = 0; // frames the observer was asked about
	std::uint64_t Ticks = 0;       // the main timer's ticks of advance

	// Issue #23.
	TReplayState Replay;
	int Messages = 0;
	int Saves = 0;
	int Quiet = 0; // idle calls still to wait (manual clock)

	// Issue #48: --test-display-modes.
	bool FixedDisplayModes = false;
	std::vector<TDisplayMode> DisplayModes;
};

void WriteLog(TState &State, const std::string &Line)
{
	if (!State.Log.IsOpen())
		return;
	State.Log.Write(Line);
	State.Log.PutChar('\n');
	State.Log.Flush();
}

[[noreturn]] void Fail(TState &State, const std::string &Message)
{
	const std::string Line = "testmode: " + Message;
	WriteLog(State, Line);
	std::fputs((Line + "\n").c_str(), stderr);
	std::fflush(stderr);
	::OutputDebugStringA((Line + "\n").c_str());
	::ExitProcess(ScriptErrorExitCode);
}

std::string Narrow(const std::wstring &Text)
{
	const int Length = ::WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), nullptr, 0, nullptr, nullptr);
	std::string Result(static_cast<std::size_t>(Length > 0 ? Length : 0), '\0');
	if (Length > 0)
		::WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), Result.data(), Length, nullptr, nullptr);
	return Result;
}

struct TArguments {
	std::wstring Script;
	std::wstring Log;
	std::wstring Out;
	std::wstring Replay;
	std::wstring DisplayModes;
	std::wstring Control;
};

// The arguments --test-script, --test-log, --test-out, --test-replay, --test-display-modes and --test-control
// of the command line.
TArguments ReadArguments()
{
	TArguments Result;
	int Count = 0;
	LPWSTR *Arguments = ::CommandLineToArgvW(::GetCommandLineW(), &Count);
	if (Arguments == nullptr)
		return Result;
	for (int i = 1; i + 1 < Count; i++) {
		if (wcscmp(Arguments[i], L"--test-script") == 0)
			Result.Script = Arguments[i + 1];
		else if (wcscmp(Arguments[i], L"--test-log") == 0)
			Result.Log = Arguments[i + 1];
		else if (wcscmp(Arguments[i], L"--test-out") == 0)
			Result.Out = Arguments[i + 1];
		else if (wcscmp(Arguments[i], L"--test-replay") == 0)
			Result.Replay = Arguments[i + 1];
		else if (wcscmp(Arguments[i], L"--test-display-modes") == 0)
			Result.DisplayModes = Arguments[i + 1];
		else if (wcscmp(Arguments[i], L"--test-control") == 0)
			Result.Control = Arguments[i + 1];
	}
	::LocalFree(Arguments);
	return Result;
}

// The dumps' folder and the packet log.
void OpenOut(TState &State, const std::wstring &Folder)
{
	State.Out = Folder;
	std::error_code Error;
	std::filesystem::create_directories(State.Out, Error);
	const std::filesystem::path Log = State.Out / L"packets.log";
	if (!State.Packets.Open(Log.c_str(), TFile::TAccess::Write))
		Fail(State, "cannot write " + Narrow(Log.wstring()));
	State.Packets.Write(std::string(PacketLogHeader) + "\n");
	State.Packets.Flush();
}

// The tool's two events of the control channel (--test-control, issue #92).
void OpenControl(TState &State, const std::wstring &Name)
{
	if (State.Out.empty())
		Fail(State, "--test-control needs --test-out <folder>");
	State.ControlCommand = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, (Name + L"-command").c_str());
	State.ControlDone = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, (Name + L"-done").c_str());
	if (State.ControlCommand == nullptr || State.ControlDone == nullptr)
		Fail(State, "--test-control " + Narrow(Name) + ": the events " + Narrow(Name) + "-command and -done do not exist");
	WriteLog(State, "testmode: control channel " + Narrow(Name));
}

// The server replay (--test-replay): the file and the listening sockets.
void OpenReplay(TState &State, const std::wstring &Path)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In)
		Fail(State, "cannot read the replay " + Narrow(Path));
	const std::string Text((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	TReplayParseResult Parsed = ParseReplay(Text);
	if (!Parsed.Ok)
		Fail(State, Narrow(Path) + ":" + std::to_string(Parsed.ErrorLine) + ": " + Parsed.Error);
	if (Parsed.File.Listen.empty())
		Fail(State, Narrow(Path) + ": no listen lines");
	WSADATA Data = {};
	if (::WSAStartup(MAKEWORD(2, 2), &Data) != 0)
		Fail(State, "replay: WSAStartup failed");
	for (const TReplayListen &Listen : Parsed.File.Listen) {
		const SOCKET Socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in Address = {};
		Address.sin_family = AF_INET;
		Address.sin_port = ::htons(static_cast<u_short>(Listen.Port));
		Address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
		u_long NonBlocking = 1;
		if (Socket == INVALID_SOCKET || ::bind(Socket, reinterpret_cast<const sockaddr *>(&Address), sizeof(Address)) != 0
			|| ::listen(Socket, 4) != 0 || ::ioctlsocket(Socket, FIONBIO, &NonBlocking) != 0)
			Fail(State, "replay: cannot listen on 127.0.0.1:" + std::to_string(Listen.Port) + " (error " + std::to_string(::WSAGetLastError()) + ")");
		State.Replay.Listeners.emplace_back(Socket, Listen.Kind);
	}
	std::size_t Connections = Parsed.File.Connections.size();
	State.Replay.Schedule = TServerReplay(std::move(Parsed.File.Connections));
	State.Replay.Active = true;
	WriteLog(State, "testmode: replay " + Narrow(Path) + ", " + std::to_string(Connections) + " connections");
}

// The fixed display modes (--test-display-modes).
void OpenDisplayModes(TState &State, const std::wstring &Path)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In)
		Fail(State, "cannot read the display modes " + Narrow(Path));
	const std::string Text((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	TDisplayModesParseResult Parsed = ParseDisplayModes(Text);
	if (!Parsed.Ok)
		Fail(State, Narrow(Path) + ":" + std::to_string(Parsed.ErrorLine) + ": " + Parsed.Error);
	State.DisplayModes = std::move(Parsed.Modes);
	State.FixedDisplayModes = true;
	WriteLog(State, "testmode: display modes " + Narrow(Path) + ", " + std::to_string(State.DisplayModes.size()) + " modes");
}

void Initialize(TState &State)
{
	const TArguments Arguments = ReadArguments();
	if (!Arguments.Log.empty())
		State.Log.Open(Arguments.Log.c_str(), TFile::TAccess::Write, TFile::TContent::Text); // stays closed where it fails
	if (!Arguments.Out.empty())
		OpenOut(State, Arguments.Out);
	if (!Arguments.Replay.empty())
		OpenReplay(State, Arguments.Replay);
	if (!Arguments.DisplayModes.empty())
		OpenDisplayModes(State, Arguments.DisplayModes);
	if (!Arguments.Control.empty())
		OpenControl(State, Arguments.Control);
	const std::wstring &ScriptPath = Arguments.Script;
	if (ScriptPath.empty())
		return;
	std::ifstream In(ScriptPath, std::ios::binary);
	if (!In)
		Fail(State, "cannot read the script " + Narrow(ScriptPath));
	const std::string Text((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	TParseResult Parsed = ParseScript(Text);
	if (!Parsed.Ok)
		Fail(State, Narrow(ScriptPath) + ":" + std::to_string(Parsed.ErrorLine) + ": " + Parsed.Error);
	WriteLog(State, "testmode: script " + Narrow(ScriptPath) + ", " + std::to_string(Parsed.Script.Events.size()) + " events, pace " + std::to_string(Parsed.Script.RealPerTick) + " ms per 100 ms");
	bool Dumps = false;
	for (const TInputEvent &Event : Parsed.Script.Events) {
		if (Event.Kind == TEventKind::StateDump || Event.Kind == TEventKind::FrameDump || Event.Kind == TEventKind::Save)
			Dumps = true;
		if (Event.Kind == TEventKind::FrameDump)
			State.FramesWanted = true;
	}
	if (Dumps && State.Out.empty())
		Fail(State, Narrow(ScriptPath) + ": the script has dump or frame commands: --test-out <folder> is needed");
	// The control channel's frame lines (issue #92) need the observer, which the engine takes at its start.
	if (State.ControlCommand != nullptr)
		State.FramesWanted = true;
	State.Scheduler = TScheduler(std::move(Parsed.Script));
	State.Clock = &State.Virtual;
	State.Scripted = true;
}

// Never destroyed: the client reads the clocks until the very end of the process (static
// destructors included).
TState &State()
{
	static TState *const Instance = [] {
		auto *Created = new TState;
		Initialize(*Created);
		return Created;
	}();
	return *Instance;
}

HWND MainWindow()
{
	return MainFrame != nullptr ? MainFrame->m_hWnd : nullptr;
}

POINT ScreenCursor(const TState &State, HWND Window)
{
	if (State.Scheduler.FixedCursor())
		return POINT{State.Scheduler.CursorX(), State.Scheduler.CursorY()};
	POINT Point = State.Cursor;
	if (Window != nullptr)
		::ClientToScreen(Window, &Point);
	return Point;
}

void Dispatch(TState &State, HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	MSG Msg = {};
	Msg.hwnd = Window;
	Msg.message = Message;
	Msg.wParam = WParam;
	Msg.lParam = LParam;
	Msg.time = State.Clock->Milliseconds();
	Msg.pt = ScreenCursor(State, Window);
	::DispatchMessageA(&Msg);
}

// lParam of a key message: repeat count 1, the scan code, the extended flag, the context code (Alt
// held) and, for a key going up, the previous state and the transition state.
LPARAM KeyLParam(const TInputEvent &Event, bool Up, bool Alt)
{
	std::uint32_t Value = 1 | ((Event.Scan & 0xffU) << 16);
	if (Event.Extended)
		Value |= 1U << 24;
	if (Alt)
		Value |= 1U << 29;
	if (Up)
		Value |= (1U << 30) | (1U << 31);
	return static_cast<LPARAM>(Value);
}

std::string Numbered(const char *Prefix, int Step, const char *Suffix)
{
	std::array<char, 64> Name{};
	std::snprintf(Name.data(), Name.size(), "%s-%03d%s", Prefix, Step, Suffix);
	return Name.data();
}

bool SaveFile(const std::filesystem::path &Path, std::span<const char> Data)
{
	std::ofstream File(Path, std::ios::binary | std::ios::trunc);
	File.write(Data.data(), static_cast<std::streamsize>(Data.size()));
	return static_cast<bool>(File);
}

// The state dump of a dump command.
void WriteStateDump(TState &State, const TInputEvent &Event)
{
	TStateDump Dump;
	Dump.Step = ++State.StateSteps;
	Dump.Time = State.Virtual.Elapsed();
	Dump.Line = Event.Line;
	Dump.State = ReadClientState();
	Dump.Gui = ReadGui();
	const std::string Name = Numbered("state", Dump.Step, ".json");
	std::string Json = StateDumpJson(Dump);
	if (!SaveFile(State.Out / Name, Json))
		WriteLog(State, "testmode: cannot write " + Name);
}

void Deliver(TState &State, HWND Window, const TInputEvent &Event)
{
	WriteLog(State, Describe(Event));
	RECT Client = {};
	::GetClientRect(Window, &Client);
	const POINT Point = {Event.X.Resolve(Client.right), Event.Y.Resolve(Client.bottom)};
	WPARAM MouseFlags = 0;
	if ((Event.Buttons & ButtonLeft) != 0)
		MouseFlags |= MK_LBUTTON;
	if ((Event.Buttons & ButtonRight) != 0)
		MouseFlags |= MK_RBUTTON;
	if ((Event.Modifiers & ModifierShift) != 0)
		MouseFlags |= MK_SHIFT;
	if ((Event.Modifiers & ModifierControl) != 0)
		MouseFlags |= MK_CONTROL;
	const LPARAM PointParam = MAKELPARAM(Point.x, Point.y);
	const bool Alt = (Event.Modifiers & ModifierAlt) != 0 || Event.Key == VK_MENU;
	switch (Event.Kind) {
	case TEventKind::KeyDown:
	case TEventKind::KeyUp: {
		const bool Up = Event.Kind == TEventKind::KeyUp;
		// With Alt held, and for Alt and F10 themselves, Windows sends the system key messages.
		const bool System = Alt || Event.Key == VK_F10;
		const UINT Message = Up ? (System ? WM_SYSKEYUP : WM_KEYUP) : (System ? WM_SYSKEYDOWN : WM_KEYDOWN);
		Dispatch(State, Window, Message, Event.Key, KeyLParam(Event, Up, Alt));
		break;
	}
	case TEventKind::Char: Dispatch(State, Window, WM_CHAR, Event.Key, KeyLParam(Event, false, Alt)); break;
	case TEventKind::MouseMove: State.Cursor = Point; break;
	case TEventKind::LeftDown:
		State.Cursor = Point;
		Dispatch(State, Window, WM_LBUTTONDOWN, MouseFlags, PointParam);
		break;
	case TEventKind::LeftUp:
		State.Cursor = Point;
		Dispatch(State, Window, WM_LBUTTONUP, MouseFlags, PointParam);
		break;
	case TEventKind::LeftDouble:
		State.Cursor = Point;
		Dispatch(State, Window, WM_LBUTTONDBLCLK, MouseFlags, PointParam);
		break;
	case TEventKind::RightDown:
		State.Cursor = Point;
		Dispatch(State, Window, WM_RBUTTONDOWN, MouseFlags, PointParam);
		break;
	case TEventKind::RightUp:
		State.Cursor = Point;
		Dispatch(State, Window, WM_RBUTTONUP, MouseFlags, PointParam);
		break;
	case TEventKind::RightDouble:
		State.Cursor = Point;
		Dispatch(State, Window, WM_RBUTTONDBLCLK, MouseFlags, PointParam);
		break;
	case TEventKind::Wheel: {
		// WM_MOUSEWHEEL: the flags and the distance (120 per notch) in wParam, the screen point.
		const POINT Screen = ScreenCursor(State, Window);
		auto Distance = static_cast<WORD>(static_cast<short>(Event.Delta * WHEEL_DELTA));
		Dispatch(State, Window, WM_MOUSEWHEEL, MAKEWPARAM(static_cast<WORD>(MouseFlags), Distance),
			MAKELPARAM(Screen.x, Screen.y));
		break;
	}
	case TEventKind::Quit:
		if (!State.PendingFrames.empty())
			WriteLog(State, "testmode: " + std::to_string(State.PendingFrames.size()) + " frame dumps were not taken (no frame of the Vulkan engine shown)");
		::PostMessageA(Window, WM_CLOSE, 0, 0);
		break;
	case TEventKind::StateDump: WriteStateDump(State, Event); break;
	case TEventKind::FrameDump:
		State.PendingFrames.push_back(TPendingFrame{++State.FrameSteps, Event.Line, State.Virtual.Elapsed()});
		break;
	case TEventKind::Advance: {
		// tools/capture/patcap.c Advance: every tick moves the clock and is sent at once, in this one
		// idle call.
		std::uint32_t Step = Event.Ticks > 0 ? Event.Duration / Event.Ticks : 0;
		for (std::uint32_t i = 0; i < Event.Ticks; i++) {
			State.Virtual.AdvanceTo(State.Virtual.Elapsed() + Step);
			::SendMessageA(Window, WM_TIMER, 1, 0);
			State.Ticks++;
		}
		State.Virtual.AdvanceTo(State.Virtual.Elapsed() + (Event.Duration - Step * Event.Ticks));
		break;
	}
	case TEventKind::Post:
		Dispatch(State, Window, Event.Key, static_cast<WPARAM>(Event.WParam), static_cast<LPARAM>(Event.LParam));
		break;
	case TEventKind::Save: {
		const std::string Name = Numbered("saved", ++State.Saves, ("-" + Event.Name).c_str());
		std::error_code Error;
		std::filesystem::copy_file(Event.Name, State.Out / Name, std::filesystem::copy_options::overwrite_existing, Error);
		if (Error)
			WriteLog(State, "testmode: cannot save " + Event.Name + ": " + Error.message());
		break;
	}
	}
}

// --- the control channel (issue #92) ---

std::string ReadText(const std::filesystem::path &Path)
{
	std::ifstream In(Path, std::ios::binary);
	return {std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>()};
}

// Answers the command: control-reply.txt, then <name>-done.
void ControlReply(TState &State, const std::string &Reply)
{
	if (!SaveFile(State.Out / L"control-reply.txt", Reply + "\n"))
		WriteLog(State, "testmode: cannot write control-reply.txt");
	if (Reply != "ok")
		WriteLog(State, "testmode: control: " + Reply);
	::SetEvent(State.ControlDone);
}

void WriteProbe(TState &State)
{
	TProbe Probe;
	Probe.Step = ++State.Probes;
	Probe.Time = State.Scripted ? State.Virtual.Elapsed() : 0;
	ReadProbe(Probe);
	Probe.Device = State.Device;
	Probe.Frames = State.FramesShown;
	Probe.Ticks = State.Ticks;
	if (!SaveFile(State.Out / L"probe.json", ProbeJson(Probe)))
		WriteLog(State, "testmode: cannot write probe.json");
}

// The command's script lines (a script with them alone; with a manual clock parsed as one), appended
// after the last event at Elapsed; empty, or why not.
std::string AppendLines(TState &State, const std::string &Text, std::uint32_t Elapsed)
{
	const bool Manual = State.Scheduler.ManualClock();
	TParseResult Parsed = ParseScript((Manual ? "clock manual\n" : "") + Text);
	if (!Parsed.Ok)
		return "line " + std::to_string(Parsed.ErrorLine - (Manual ? 1 : 0)) + ": " + Parsed.Error;
	if (Parsed.Script.FixedCursor || Parsed.Script.RealPerTick != TScript{}.RealPerTick
		|| Parsed.Script.ManualClock != Manual)
		return "pace, clock and cursor belong to the script the client starts with";
	for (const TInputEvent &Event : Parsed.Script.Events) {
		if (Event.Kind == TEventKind::Save && State.Out.empty())
			return "save needs --test-out";
	}
	State.Scheduler.Append(std::move(Parsed.Script.Events), Elapsed);
	return {};
}

// At an idle call with nothing of the script left to hand out: the command whose lines are done is
// answered, then a new one is taken.
void ControlPoll(TState &State, std::uint32_t Elapsed)
{
	if (State.ControlBusy) {
		if (!State.Scheduler.Finished() || !State.PendingFrames.empty())
			return;
		State.ControlBusy = false;
		ControlReply(State, "ok");
	}
	if (::WaitForSingleObject(State.ControlCommand, 0) != WAIT_OBJECT_0)
		return;
	std::string Text = ReadText(State.Out / L"control.txt");
	while (!Text.empty() && (Text.back() == '\n' || Text.back() == '\r' || Text.back() == ' '))
		Text.pop_back();
	WriteLog(State, "testmode: control: " + (Text.find('\n') == std::string::npos ? Text : Text.substr(0, Text.find('\n')) + " ..."));
	if (Text == "probe") {
		WriteProbe(State);
		ControlReply(State, "ok");
		return;
	}
	if (!State.Scripted) {
		ControlReply(State, "error: without --test-script the channel answers probe only");
		return;
	}
	const std::string Error = AppendLines(State, Text, Elapsed);
	if (!Error.empty()) {
		ControlReply(State, "error: " + Error);
		return;
	}
	State.ControlBusy = true;
}

// --- the server replay (issue #23) ---

std::string ReplayName(const TReplaySocket &Socket)
{
	if (Socket.Index < 0)
		return Socket.Kind + " connection (not recorded)";
	return Socket.Kind + " connection " + std::to_string(Socket.Index + 1);
}

void CloseReplaySocket(TState &State, TReplaySocket &Socket, const char *Why)
{
	if (Socket.Closed)
		return;
	Socket.Closed = true;
	Socket.Unread = 0;
	Socket.Out.clear();
	::closesocket(Socket.Socket);
	Socket.Socket = INVALID_SOCKET;
	State.Replay.Schedule.ClientClosed(Socket.Index);
	WriteLog(State, "testmode: replay: " + ReplayName(Socket) + " closed by the client (" + Why + ")");
}

void FlushReplaySocket(TState &State, TReplaySocket &Socket)
{
	while (!Socket.Out.empty() && !Socket.Closed) {
		const int Count = ::send(Socket.Socket, reinterpret_cast<const char *>(Socket.Out.data()),
			static_cast<int>((std::min)(Socket.Out.size(), static_cast<std::size_t>(0x10000))), 0);
		if (Count == SOCKET_ERROR) {
			if (::WSAGetLastError() != WSAEWOULDBLOCK)
				CloseReplaySocket(State, Socket, "send failed");
			return;
		}
		Socket.Out.erase(Socket.Out.begin(), Socket.Out.begin() + Count);
	}
}

// New connections, the client's bytes and closes, the bytes still to send.
void PumpReplay(TState &State)
{
	TReplayState &Replay = State.Replay;
	for (const auto &[Listener, Kind] : Replay.Listeners) {
		while (true) {
			const SOCKET Client = ::accept(Listener, nullptr, nullptr);
			if (Client == INVALID_SOCKET)
				break;
			u_long NonBlocking = 1;
			::ioctlsocket(Client, FIONBIO, &NonBlocking);
			BOOL NoDelay = TRUE;
			::setsockopt(Client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&NoDelay), sizeof(NoDelay));
			TReplaySocket Socket;
			Socket.Socket = Client;
			Socket.Kind = Kind;
			Socket.Index = Replay.Schedule.Open(Kind);
			Replay.Sockets.push_back(std::move(Socket));
			WriteLog(State, "testmode: replay: " + ReplayName(Replay.Sockets.back()) + " opened at " + std::to_string(State.Virtual.Elapsed()) + " ms");
		}
	}
	for (TReplaySocket &Socket : Replay.Sockets) {
		while (!Socket.Closed) {
			std::array<char, 0x4000> Buffer{};
			const int Count = ::recv(Socket.Socket, Buffer.data(), static_cast<int>(Buffer.size()), 0);
			if (Count == SOCKET_ERROR && ::WSAGetLastError() == WSAEWOULDBLOCK)
				break;
			if (Count <= 0) {
				CloseReplaySocket(State, Socket, Count == 0 ? "end of stream" : "receive failed");
				break;
			}
			Socket.In.insert(Socket.In.end(), Buffer.begin(), Buffer.begin() + Count);
		}
		while (Socket.In.size() >= 2) {
			std::size_t Size = Socket.In[0] | (static_cast<std::size_t>(Socket.In[1]) << 8U);
			if (Socket.In.size() < Size + 2)
				break;
			if (Socket.Frames == 0) {
				if (Replay.HasPendingKey) {
					Socket.Key = Replay.PendingKey;
					Socket.HasKey = true;
					Replay.HasPendingKey = false;
				} else {
					WriteLog(State, "testmode: replay: " + ReplayName(Socket) + ": no RSA block seen for its first packet");
				}
			}
			Socket.In.erase(Socket.In.begin(), Socket.In.begin() + static_cast<std::ptrdiff_t>(Size + 2));
			Socket.Frames++;
			Replay.ReplayRead++;
			Replay.Schedule.ClientPacket(Socket.Index);
		}
		FlushReplaySocket(State, Socket);
	}
}

// Why the clock and the script wait for the network, or empty.
std::string ReplayBusy(TState &State)
{
	TReplayState &Replay = State.Replay;
	const int *Connection = ViewConnectionState();
	const int Value = Connection != nullptr ? *Connection : 0;
	const bool Connecting = Value == 1 || Value == 2 || Value == 5;
	if (!Connecting)
		Replay.IgnoreConnecting = false;
	if (Connecting && !Replay.IgnoreConnecting)
		return "the client connects (connection state " + std::to_string(Value) + ")";
	if (Replay.ClientSent > Replay.ReplayRead)
		return "a client packet is on its way";
	for (const TReplaySocket &Socket : Replay.Sockets) {
		if (Socket.Closed)
			continue;
		if (Socket.Unread > 0 || !Socket.Out.empty())
			return ReplayName(Socket) + ": a server packet is on its way";
		if (Socket.ServerClosing)
			return ReplayName(Socket) + ": the client's close is awaited";
	}
	return {};
}

// True while the network is busy (the driver then does nothing else); after HoldLimit real
// milliseconds of the same wait it gives up on it.
bool ReplayHold(TState &State)
{
	TReplayState &Replay = State.Replay;
	const std::string Reason = ReplayBusy(State);
	auto Now = std::chrono::steady_clock::now();
	if (Reason.empty()) {
		if (Replay.Holding)
			State.RealStart += Now - Replay.HoldStart; // the wait does not count for the pace
		Replay.Holding = false;
		return false;
	}
	if (!Replay.Holding || Reason != Replay.HoldReason) {
		if (Replay.Holding)
			State.RealStart += Now - Replay.HoldStart;
		Replay.Holding = true;
		Replay.HoldStart = Now;
		Replay.HoldReason = Reason;
		return true;
	}
	if (std::chrono::duration_cast<std::chrono::milliseconds>(Now - Replay.HoldStart).count() < HoldLimit)
		return true;
	Replay.GivenUp++;
	WriteLog(State, "testmode: replay: gave up waiting after " + std::to_string(HoldLimit) + " ms at " + std::to_string(State.Virtual.Elapsed()) + " ms: " + Reason);
	Replay.IgnoreConnecting = true;
	Replay.ClientSent = Replay.ReplayRead;
	for (TReplaySocket &Socket : Replay.Sockets) {
		Socket.Unread = 0;
		if (Socket.ServerClosing && !Socket.Closed)
			CloseReplaySocket(State, Socket, "no answer to the server's close");
	}
	State.RealStart += Now - Replay.HoldStart;
	Replay.Holding = false;
	return false;
}

// Sends the next server packet or close that is due; true when it did.
bool ReplayDeliver(TState &State, std::uint32_t Elapsed)
{
	TReplayState &Replay = State.Replay;
	for (TReplaySocket &Socket : Replay.Sockets) {
		if (Socket.Closed || Socket.ServerClosing || Socket.Index < 0)
			continue;
		const TReplayEvent *Event = Replay.Schedule.Due(Socket.Index, Elapsed);
		if (Event == nullptr)
			continue;
		if (Event->Kind == TReplayKind::ServerClose) {
			WriteLog(State, std::to_string(Elapsed) + " ms, replay line " + std::to_string(Event->Line) + ": " + ReplayName(Socket) + " closed by the server");
			::shutdown(Socket.Socket, SD_SEND);
			Socket.ServerClosing = true;
			Replay.Schedule.Take(Socket.Index);
			return true;
		}
		if (!Socket.HasKey)
			WriteLog(State, "testmode: replay: " + ReplayName(Socket) + ": no XTEA key, the packet goes out with a zero key");
		std::vector<std::uint8_t> Frame = EncryptFrame(Event->Payload, Socket.Key);
		WriteLog(State, std::to_string(Elapsed) + " ms, replay line " + std::to_string(Event->Line) + ": " + ReplayName(Socket) + " server packet, " + std::to_string(Event->Payload.size()) + " bytes");
		Socket.Out.insert(Socket.Out.end(), Frame.begin(), Frame.end());
		Socket.Unread++;
		Replay.Sent++;
		Replay.Schedule.Take(Socket.Index);
		FlushReplaySocket(State, Socket);
		return true;
	}
	return false;
}

// The next time a server packet or close of an open connection is due (an automatic clock stops
// there); false: none.
bool ReplayNextTime(TState &State, std::uint32_t *Time)
{
	bool Found = false;
	for (const TReplaySocket &Socket : State.Replay.Sockets) {
		std::uint32_t Next = 0;
		if (Socket.Closed || Socket.ServerClosing || !State.Replay.Schedule.NextTime(Socket.Index, &Next))
			continue;
		if (!Found || Next < *Time)
			*Time = Next;
		Found = true;
	}
	return Found;
}

} // namespace

BOOL GetCursorPosition(LPPOINT Point)
{
	const TState &Current = State();
	if (!Current.Scripted)
		return ::GetCursorPos(Point);
	if (Current.Scheduler.FixedCursor()) {
		*Point = POINT{Current.Scheduler.CursorX(), Current.Scheduler.CursorY()};
		return TRUE;
	}
	*Point = ScreenCursor(Current, MainWindow());
	return TRUE;
}

UINT_PTR SetMainTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function)
{
	TState &Current = State();
	if (!Current.Scripted)
		return ::SetTimer(Window, Id, Elapse, Function);
	WriteLog(Current, "testmode: the main timer's ticks come from the driver");
	return Id;
}

bool DropInput(const MSG *Message)
{
	const TState &Current = State();
	if (!Current.Scripted)
		return false;
	const UINT Kind = Message->message;
	if ((Kind >= WM_KEYFIRST && Kind <= WM_KEYLAST) || (Kind >= WM_MOUSEFIRST && Kind <= WM_MOUSELAST))
		return true;
	return Kind == WM_TIMER && Message->wParam == 1 && Message->hwnd == MainWindow();
}

// Issue #52: the main window's WM_CAPTURECHANGED (MainFrm.cpp OnCaptureChanged). With a script the
// buttons are pressed and released only by the script's events, dispatched to the main window; the
// window loses the Windows capture only through the desktop (another window activated, the user's own
// clicks: a scripted run of the reference compare lost a click that way), which, as DropInput's input,
// must not reach the client. The client's own ReleaseCapture comes after it has cleared its button flags,
// when the handler does nothing.
bool IgnoreCaptureChange()
{
	return State().Scripted;
}

// Issue #48: the display modes of the Graphics Options dialog (vulkan/SurfaceVulkan.cpp
// GetFullscreenModes). With --test-display-modes the Index-th mode of the file, with the fields
// EnumDisplaySettings sets; the current and registry settings (ENUM_CURRENT_SETTINGS, the desktop's mode)
// and another device stay the machine's.
BOOL EnumDisplayMode(LPCSTR Device, DWORD Index, DEVMODEA *Mode)
{
	const TState &Current = State();
	if (!Current.FixedDisplayModes || Device != nullptr || Index == ENUM_CURRENT_SETTINGS || Index == ENUM_REGISTRY_SETTINGS)
		return ::EnumDisplaySettingsA(Device, Index, Mode);
	TDisplayMode Found;
	const bool Listed = EnumerateDisplayMode(&Current.DisplayModes, Index, &Found,
		[](std::uint32_t, TDisplayMode *) { return false; });
	if (!Listed)
		return FALSE;
	Mode->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;
	Mode->dmBitsPerPel = Found.Depth;
	Mode->dmPelsWidth = Found.Width;
	Mode->dmPelsHeight = Found.Height;
	Mode->dmDisplayFlags = 0;
	Mode->dmDisplayFrequency = 60;
	return TRUE;
}

// Issue #83: PATELLA_TEST_FAIL_PRESENT=1 makes every present of the Vulkan engine fail (TestHooks.h), read once.
bool PresentFails()
{
	static const bool Fails = [] {
		std::array<char, 8> Value{};
		std::size_t Length = 0;
		return getenv_s(&Length, Value.data(), Value.size(), "PATELLA_TEST_FAIL_PRESENT") == 0 && Length == 2 && Value[0] == '1';
	}();
	return Fails;
}

// Issue #90: PATELLA_TEST_FAIL_INIT=<n> makes InitControl fail before its step n (TestHooks.h), read once.
bool InitFails(int Step)
{
	static const int FailingStep = [] {
		std::array<char, 8> Value{};
		std::size_t Length = 0;
		if (getenv_s(&Length, Value.data(), Value.size(), "PATELLA_TEST_FAIL_INIT") != 0 || Length < 2)
			return 0;
		int Parsed = 0;
		const char *const Last = Value.data() + (Length - 1); // Length counts the terminating zero
		const auto [End, Error] = std::from_chars(Value.data(), Last, Parsed);
		return Error == std::errc{} && End == Last ? Parsed : 0;
	}();
	return FailingStep != 0 && Step == FailingStep;
}

namespace {

// Issue #96: EnumOwnWindows's callback and its parameter, and the filter that hands it only this process's windows.
struct TOwnWindows {
	WNDENUMPROC Function;
	LPARAM Parameter;
};

BOOL CALLBACK EnumOwnWindow(HWND Window, LPARAM Parameter)
{
	const auto *const Own = reinterpret_cast<const TOwnWindows *>(Parameter);
	DWORD Process = 0;
	GetWindowThreadProcessId(Window, &Process);
	return Process == GetCurrentProcessId() ? Own->Function(Window, Own->Parameter) : TRUE;
}

} // namespace

BOOL EnumOwnWindows(WNDENUMPROC Function, LPARAM Parameter)
{
	const TOwnWindows Own{Function, Parameter};
	return EnumWindows(EnumOwnWindow, reinterpret_cast<LPARAM>(&Own));
}

void Idle()
{
	TState &Current = State();
	if (!Current.Scripted) {
		// Issue #92: without a script the control channel answers probe only, at any idle call.
		if (Current.ControlCommand != nullptr && MainWindow() != nullptr)
			ControlPoll(Current, 0);
		return;
	}
	HWND Window = MainWindow();
	if (Window == nullptr)
		return;
	auto Now = std::chrono::steady_clock::now();
	if (!Current.Started) {
		Current.Started = true;
		Current.RealStart = Now;
		WriteLog(Current, "testmode: the driver starts");
	}
	std::uint32_t Elapsed = Current.Virtual.Elapsed();
	// Issue #23: the network first. While it is busy nothing else happens; a due server packet or
	// close is an idle call of its own, as an input event is.
	if (Current.Replay.Active) {
		PumpReplay(Current);
		if (ReplayHold(Current))
			return;
	}
	// With a manual clock the client gets QuietPasses idle calls (each with its ProcessEvents) after
	// every input event and network action before the next one, as Tibia.exe gets many between the
	// record step's messages: the dialogs the last one opens are there before the next one comes.
	if (Current.Quiet > 0) {
		Current.Quiet--;
		return;
	}
	if (Current.Replay.Active && ReplayDeliver(Current, Elapsed)) {
		Current.Quiet = Current.Scheduler.ManualClock() ? QuietPasses : 0;
		return;
	}
	// A state dump takes no idle call of its own: with or without dumps the client gets the same events in
	// the same idle calls. (A frame dump waits for the next frame, so it is an event in its turn.)
	const TInputEvent *Event = Current.Scheduler.TakeDue(Elapsed);
	while (Event != nullptr && (Event->Kind == TEventKind::StateDump || Event->Kind == TEventKind::Save)) {
		Deliver(Current, Window, *Event);
		Event = Current.Scheduler.TakeDue(Elapsed);
	}
	if (Event != nullptr) {
		Deliver(Current, Window, *Event);
		Current.Quiet = Current.Scheduler.ManualClock() ? QuietPasses : 0;
		return;
	}
	if (Current.ControlCommand != nullptr) {
		// Issue #92: the control channel's next command once the script and the last command are done.
		if (Current.Scheduler.Finished()) {
			ControlPoll(Current, Elapsed);
			if (!Current.Scheduler.Finished())
				return; // the command's lines, from the next idle call on
		}
	} else if (Current.Scheduler.Finished() && !Current.FinishReported) {
		Current.FinishReported = true;
		WriteLog(Current, "testmode: the script ends at " + std::to_string(Elapsed) + " ms; the clock goes on");
	}
	if (Current.Scheduler.ManualClock())
		return;
	std::uint32_t Stop = Current.Scheduler.NextStop(Elapsed);
	std::uint32_t ReplayTime = 0;
	if (Current.Replay.Active && ReplayNextTime(Current, &ReplayTime) && ReplayTime > Elapsed && ReplayTime < Stop)
		Stop = ReplayTime;
	// Now again: a state dump's wait for the checker moved RealStart.
	Now = std::chrono::steady_clock::now();
	auto Real = std::chrono::duration_cast<std::chrono::milliseconds>(Now - Current.RealStart).count();
	if (Real < 0 || static_cast<std::uint64_t>(Real) < Current.Scheduler.RealDeadline(Stop))
		return;
	Current.Virtual.AdvanceTo(Stop);
	if (TScheduler::IsTick(Stop))
		Dispatch(Current, Window, WM_TIMER, 1, 0);
}

// --- the packet log ---

namespace {

// A packet's time: the virtual clock with a script, else milliseconds since the first packet.
std::uint32_t PacketTime(TState &State)
{
	if (State.Scripted)
		return State.Virtual.Elapsed();
	auto Now = std::chrono::steady_clock::now();
	if (!State.PacketClockStarted) {
		State.PacketClockStarted = true;
		State.PacketClockStart = Now;
	}
	return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Now - State.PacketClockStart).count());
}

void WritePacket(TState &State, const TPacketRecord &Record)
{
	State.Packets.Write(PacketLogLine(Record) + "\n");
	State.Packets.Flush();
}

} // namespace

void RsaBlock(patella::TSpan<const unsigned char, 128> Block)
{
	TState &Current = State();
	if (Current.Replay.Active) {
		// [0][the XTEA key][fields][random bytes]: the key of the connection whose first packet follows.
		std::copy_n(Block.begin() + 1, Current.Replay.PendingKey.size(), Current.Replay.PendingKey.begin());
		Current.Replay.HasPendingKey = true;
	}
	if (Current.Packets == nullptr)
		return;
	std::copy_n(Block.begin(), RsaBlockSize, Current.Rsa.begin());
	Current.HasRsa = true;
}

void PacketSent(patella::TSpan<const unsigned char> Data, int Size, int Padded)
{
	TState &Current = State();
	if (Current.Replay.Active)
		Current.Replay.ClientSent++;
	if (Current.Packets == nullptr || Size < 0)
		return;
	// SendData: [u16][u16 Size][payload][padding, up to Padded + 2 bytes].
	const std::span<const unsigned char> Payload = Data.subspan(4, static_cast<std::size_t>(Size));
	if (Padded == 0) {
		std::vector<unsigned char> Plain(Payload.begin(), Payload.end());
		if (Current.HasRsa && Plain.size() >= RsaBlockSize)
			std::copy(Current.Rsa.begin(), Current.Rsa.end(), Plain.end() - static_cast<std::ptrdiff_t>(RsaBlockSize));
		Current.HasRsa = false;
		WritePacket(Current, Current.Connections.Sent(PacketTime(Current), true, Plain, {}));
		return;
	}
	std::size_t Padding = Padded >= Size + 2 ? static_cast<std::size_t>(Padded - Size - 2) : 0;
	const std::span<const unsigned char> Pad = Data.subspan(4 + static_cast<std::size_t>(Size), Padding);
	WritePacket(Current, Current.Connections.Sent(PacketTime(Current), false, Payload, Pad));
}

void PacketReceived(patella::TSpan<const unsigned char> Data, int PacketSize)
{
	TState &Current = State();
	if (Current.Replay.Active) {
		// The client reads only its newest connection.
		for (auto Socket = Current.Replay.Sockets.rbegin(); Socket != Current.Replay.Sockets.rend(); ++Socket) {
			if (Socket->Unread > 0) {
				Socket->Unread--;
				break;
			}
		}
	}
	if (Current.Packets == nullptr || PacketSize < 2)
		return;
	// ReceiveData: [u16 PacketSize][u16 length][payload][padding, up to PacketSize + 2 bytes].
	const int Length = (std::min)(Data[2] | (Data[3] << 8), PacketSize - 2);
	const std::span<const unsigned char> Payload = Data.subspan(4, static_cast<std::size_t>(Length));
	const std::span<const unsigned char> Padding =
		Data.subspan(4 + static_cast<std::size_t>(Length), static_cast<std::size_t>(PacketSize - 2 - Length));
	WritePacket(Current, Current.Connections.Received(PacketTime(Current), Payload, Padding));
}

// --- the frame dump: the Vulkan engine's frame observer (src/vulkan/SurfaceVulkan.cpp) ---

namespace {

int PATELLA_CDECL FrameWanted(int /*Width*/, int /*Height*/)
{
	TState &Current = State();
	Current.FramesShown++;
	return Current.PendingFrames.empty() ? 0 : 1;
}

// Issue #92: the device the Vulkan engine runs on, for the control channel's probe.
void PATELLA_CDECL DeviceSeen(const char *Name, int Type, int CpuDriver)
{
	TState &Current = State();
	Current.Device.Known = true;
	Current.Device.Name = Name != nullptr ? Name : "";
	Current.Device.Type = Type;
	Current.Device.CpuDriver = CpuDriver;
	WriteLog(Current, "testmode: the Vulkan engine's device: " + Current.Device.Name);
}

void PATELLA_CDECL FrameMask(const unsigned char *Mask, int Width, int Height)
{
	TState &Current = State();
	Current.Mask.assign(Mask, Mask + static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height));
	Current.MaskWidth = Width;
	Current.MaskHeight = Height;
}

std::span<const char> Bytes(const std::vector<std::uint8_t> &Data)
{
	return {reinterpret_cast<const char *>(Data.data()), Data.size()};
}

void PATELLA_CDECL FramePixels(const void *Bits, int Pitch, int Width, int Height)
{
	TState &Current = State();
	if (Current.PendingFrames.empty())
		return;
	const TPendingFrame Frame = Current.PendingFrames.front();
	Current.PendingFrames.pop_front();
	std::vector<std::uint8_t> Rgb = BgrxToRgb(static_cast<const std::uint8_t *>(Bits), static_cast<std::size_t>(Pitch), Width, Height);
	const std::string Name = Numbered("frame", Frame.Step, ".png");
	bool Ok = SaveFile(Current.Out / Name, Bytes(EncodePngRgb(Width, Height, Rgb)));
	long long Filtered = -1;
	if (Current.MaskWidth == Width && Current.MaskHeight == Height && !Current.Mask.empty()) {
		Filtered = std::count_if(Current.Mask.begin(), Current.Mask.end(), [](std::uint8_t Value) { return Value != 0; });
		Ok &= SaveFile(Current.Out / Numbered("frame", Frame.Step, ".filtered.png"),
			Bytes(EncodePngRgb(Width, Height, MaskToRgb(Current.Mask))));
	}
	Current.Mask.clear();
	Current.MaskWidth = 0;
	Current.MaskHeight = 0;
	// frames.txt: <step> <requested ms> <script line> <shown ms> <width> <height> <filtered pixels, -1: no mask>
	TFile Index;
	if (Index.Open((Current.Out / L"frames.txt").c_str(), TFile::TAccess::Append)) {
		Index.Print("{} {} {} {} {} {} {}\n", Frame.Step, Frame.Requested, Frame.Line, Current.Virtual.Elapsed(), Width,
			Height, Filtered);
		Index.Close();
	}
	if (!Ok)
		WriteLog(Current, "testmode: cannot write " + Name);
}

} // namespace

int ShowMessageBox(HWND Window, const char *Text, const char *Caption, UINT Type)
{
	TState &Current = State();
	if (!Current.Scripted)
		return ::MessageBoxA(Window, Text, Caption, Type);
	// A test run never opens a modal dialog: the box's caption and text are written down instead, and
	// it counts as answered with OK.
	const std::string Caption8 = Caption != nullptr ? Caption : "";
	const std::string Text8 = Text != nullptr ? Text : "";
	WriteLog(Current, "testmode: message box \"" + Caption8 + "\": " + Text8);
	if (!Current.Out.empty()) {
		TFile File;
		if (File.Open((Current.Out / L"messages.txt").c_str(), TFile::TAccess::Append)) {
			const std::string Line = "{\"step\":" + std::to_string(++Current.Messages) + ",\"time\":" + std::to_string(Current.Virtual.Elapsed())
				+ ",\"caption\":" + JsonString(Caption8) + ",\"text\":" + JsonString(Text8) + ",\"type\":" + std::to_string(Type) + "}\n";
			File.Write(Line);
			File.Close();
		}
	}
	// While a real box is open its modal loop dispatches the thread's messages (FatalError's
	// ShutdownConnection: the server's close comes, the client leaves the game and saves Tibia.cfg).
	// The same here, until the queue and the replay have been quiet for BoxQuietMilliseconds (at most
	// BoxMilliseconds).
	auto Start = std::chrono::steady_clock::now();
	auto LastActivity = Start;
	while (true) {
		auto Now = std::chrono::steady_clock::now();
		if (Now - LastActivity > std::chrono::milliseconds(BoxQuietMilliseconds) || Now - Start > std::chrono::milliseconds(BoxMilliseconds))
			break;
		bool Activity = false;
		if (Current.Replay.Active) {
			std::size_t Closed = 0;
			for (const TReplaySocket &Socket : Current.Replay.Sockets)
				Closed += Socket.Closed ? 1 : 0;
			PumpReplay(Current);
			for (const TReplaySocket &Socket : Current.Replay.Sockets)
				Closed -= Socket.Closed ? 1 : 0;
			Activity = Closed != 0;
		}
		MSG Message = {};
		while (::PeekMessageA(&Message, nullptr, 0, 0, PM_REMOVE)) {
			if (Message.message == WM_QUIT) {
				::PostQuitMessage(static_cast<int>(Message.wParam));
				return IDOK;
			}
			if (DropInput(&Message))
				continue;
			::TranslateMessage(&Message);
			::DispatchMessageA(&Message);
			Activity = true;
		}
		if (Activity)
			LastActivity = std::chrono::steady_clock::now();
		::Sleep(1);
	}
	return IDOK;
}

void FrameObservers(int(PATELLA_CDECL **Frame)(int, int), void(PATELLA_CDECL **Pixels)(const void *, int, int, int),
	void(PATELLA_CDECL **Mask)(const unsigned char *, int, int), void(PATELLA_CDECL **Device)(const char *, int, int))
{
	const TState &Current = State();
	if (Current.ControlCommand != nullptr)
		*Device = DeviceSeen;
	if (!Current.FramesWanted)
		return;
	*Frame = FrameWanted;
	*Pixels = FramePixels;
	*Mask = FrameMask;
}

} // namespace patella::testmode

// The test build's platform (issue #109; platform/Platform.h): the clock is the test mode's (the virtual clock with a
// script, the system's without one, as Initialize chose it), the files and the network Win32's (the comparisons
// read the files the client writes in its folder; the server replay is a server on the loopback ports).
patella::platform::TPlatform &patella::platform::Platform()
{
	static TPlatform Instance{*patella::testmode::State().Clock, SystemFileSystem(), Win32Socket()};
	return Instance;
}
