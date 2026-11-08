// TestModeWin32.cpp: the test mode of the Windows test build (issue #21; notes/modernization-plan.md,
// 4.4; src/TestHooks.h). Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake); Patella.exe
// contains none of it.
//
//	PatellaTest.exe [--test-script <file>] [--test-log <file>] [--test-out <folder>]
//	                [--test-replay <file>] [--test-display-modes <file>] [--test-control <name>]
//	                [--test-headless] [the client's own words]
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
//   posted for quit), dispatched with DispatchMessageA like the message loop's (on the SDL3 window,
//   whose input is SDL's events since issue #123, the input messages become those SDL events:
//   testmode/SdlInputWin32.h); the mouse is a virtual cursor that GetCursorPos returns (the client
//   polls it in OnIdle);
// - the keyboard and mouse input from Windows is dropped (CPlayerApp::PreTranslateMessage; on the
//   SDL3 window also SDL's events, IInput::IgnoreDesktopInput), so the desktop's own mouse and
//   keyboard do not reach the client during a run;
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
//   server. The driver listens on the file's ports on 127.0.0.1 inside the process (issue #120: the client's
//   socket is TPipeSocketWin32, src/testmode/PipeSocketWin32.h, no port is opened), takes each connection's XTEA
//   key from the client's RSA block, and sends the recorded server packets and closes when they are due (their
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
// modes of the machine it was recorded on. Without it the test build asks the machine, as Patella.exe.
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
// (TTestWindow::VisitWindows, issue #119), so another client on the machine (another worktree's test, a parallel CTest test,
// a Tibia.exe) cannot stop a test client. Patella.exe counts every window, as the original.
//
// Issue #145: --test-headless (with a script; exit code 3 without one) runs the client without a desktop
// (TTestWindow::Headless, platform/Window.h): its main window is made but never shown, so it neither appears nor
// takes the focus, and the desktop's input cannot reach it (the script is its only input, as with any script); the
// Vulkan engine renders without a surface and presents nothing (the frame dump reads the canvas back as always);
// fullscreen mode leaves the display's mode alone. Several headless clients may run at once (the replay opens no
// port, issue #120; the count of the Tibia windows sees only the process's own, issue #96).
//
// A script that does not parse ends the program before the window opens, with exit code 3 and the
// message on stderr, in the log and through OutputDebugString (never a dialog).

#include "common.h"
#include "compat/span.h"

#include "SdlFrame.h"
#include "testmode/ClientStateWin32.h"
#include "testmode/Clock.h"
#include "testmode/DisplayModes.h"
#include "testmode/InputScript.h"
#include "testmode/PacketLog.h"
#include "testmode/PipeNetwork.h"
#include "testmode/PipeSocketWin32.h"
#include "testmode/SdlInputWin32.h"
#include "testmode/Png.h"
#include "testmode/ServerReplay.h"
#include "testmode/StateDump.h"
#include "platform/Platform.h"
#include "platform/Process.h"
#include "types/File.h"
#include "types/FixedString.h"

#if defined(_WIN32)
#include <shellapi.h>
#else
#include "platform/SdlPlatform.h"

#include <filesystem>
#include <thread>
#endif

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
#include <memory>
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
#if defined(_WIN32)
	::Sleep(1);
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
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
	int Pipe = -1; // the connection of TPipeNetwork (issue #120)
	std::string Kind;
	int Index = -1; // the recorded connection (TServerReplay), -1: none
	std::array<std::uint8_t, 16> Key{};
	bool HasKey = false;
	std::vector<std::uint8_t> In; // bytes from the client, not yet a whole frame
	int Unread = 0;               // frames sent that the client has not decrypted yet
	bool ServerClosing = false;   // shut down for sending; the client's close is awaited
	bool Closed = false;          // closed by the client (or failed)
	int Frames = 0;               // client frames read
};

struct TReplayState {
	bool Active = false;
	TServerReplay Schedule;
	std::unique_ptr<TPipeSocketWin32> Pipes; // the client's socket and the replay's network (issue #120)
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
	IClock *Clock = &platform::SystemClock(); // the system's clock (issue #109: the platform's own)
	bool Scripted = false;
	bool Headless = false; // --test-headless (issue #145)
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
	bool ControlOn = false;
#if defined(_WIN32)
	HANDLE ControlCommand = nullptr;
	HANDLE ControlDone = nullptr;
#endif
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
#if defined(_WIN32)
	::OutputDebugStringA((Line + "\n").c_str());
	::ExitProcess(ScriptErrorExitCode);
#else
	std::_Exit(static_cast<int>(ScriptErrorExitCode));
#endif
}

std::string Narrow(const std::filesystem::path &Path)
{
#if defined(_WIN32)
	const std::wstring &Text = Path.native();
	const int Length = ::WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), nullptr, 0, nullptr, nullptr);
	std::string Result(static_cast<std::size_t>(Length > 0 ? Length : 0), '\0');
	if (Length > 0)
		::WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), Result.data(), Length, nullptr, nullptr);
	return Result;
#else
	return Path.string();
#endif
}

struct TArguments {
	std::filesystem::path Script;
	std::filesystem::path Log;
	std::filesystem::path Out;
	std::filesystem::path Replay;
	std::filesystem::path DisplayModes;
	std::filesystem::path Control;
	bool Headless = false;
};

// The arguments --test-script, --test-log, --test-out, --test-replay, --test-display-modes and --test-control
// of the command line, and the word --test-headless.
TArguments ReadArguments()
{
	TArguments Result;
	std::vector<std::filesystem::path> Words;
#if defined(_WIN32)
	int Count = 0;
	LPWSTR *Arguments = ::CommandLineToArgvW(::GetCommandLineW(), &Count);
	if (Arguments == nullptr)
		return Result;
	for (int i = 1; i < Count; i++)
		Words.emplace_back(Arguments[i]);
	::LocalFree(Arguments);
#else
	for (const std::string &Word : platform::Arguments())
		Words.emplace_back(Word);
#endif
	const auto Is = [](const std::filesystem::path &Word, const char *Name) { return Word.native() == std::filesystem::path(Name).native(); };
	for (const std::filesystem::path &Word : Words) {
		if (Is(Word, "--test-headless"))
			Result.Headless = true;
	}
	for (std::size_t i = 0; i + 1 < Words.size(); i++) {
		if (Is(Words[i], "--test-script"))
			Result.Script = Words[i + 1];
		else if (Is(Words[i], "--test-log"))
			Result.Log = Words[i + 1];
		else if (Is(Words[i], "--test-out"))
			Result.Out = Words[i + 1];
		else if (Is(Words[i], "--test-replay"))
			Result.Replay = Words[i + 1];
		else if (Is(Words[i], "--test-display-modes"))
			Result.DisplayModes = Words[i + 1];
		else if (Is(Words[i], "--test-control"))
			Result.Control = Words[i + 1];
	}
	return Result;
}

// The dumps' folder and the packet log.
void OpenOut(TState &State, const std::filesystem::path &Folder)
{
	State.Out = Folder;
	std::error_code Error;
	std::filesystem::create_directories(State.Out, Error);
	const std::filesystem::path Log = State.Out / L"packets.log";
	if (!State.Packets.OpenSystemName(Log.c_str(), TFile::TAccess::Write))
		Fail(State, "cannot write " + Narrow(Log.wstring()));
	State.Packets.Write(std::string(PacketLogHeader) + "\n");
	State.Packets.Flush();
}

// The tool's two events of the control channel (--test-control, issue #92).
void OpenControl(TState &State, const std::filesystem::path &Name)
{
	if (State.Out.empty())
		Fail(State, "--test-control needs --test-out <folder>");
#if defined(_WIN32)
	State.ControlCommand = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, (Name.native() + L"-command").c_str());
	State.ControlDone = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, (Name.native() + L"-done").c_str());
	if (State.ControlCommand == nullptr || State.ControlDone == nullptr)
		Fail(State, "--test-control " + Narrow(Name) + ": the events " + Narrow(Name) + "-command and -done do not exist");
#endif
	State.ControlOn = true;
	WriteLog(State, "testmode: control channel " + Narrow(Name));
}

// The server replay (--test-replay): the file and its listeners, inside the process (issue #120).
void OpenReplay(TState &State, const std::filesystem::path &Path)
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
	State.Replay.Pipes = std::make_unique<TPipeSocketWin32>();
	if (!State.Replay.Pipes->Start())
		Fail(State, "replay: cannot make the socket events' window");
	for (const TReplayListen &Listen : Parsed.File.Listen) {
		State.Replay.Pipes->Network().Listen("127.0.0.1", Listen.Port, Listen.Kind);
		WriteLog(State, "testmode: replay: " + Listen.Kind + " listens on 127.0.0.1:" + std::to_string(Listen.Port) + " in the process (no port)");
	}
	std::size_t Connections = Parsed.File.Connections.size();
	State.Replay.Schedule = TServerReplay(std::move(Parsed.File.Connections));
	State.Replay.Active = true;
	WriteLog(State, "testmode: replay " + Narrow(Path) + ", " + std::to_string(Connections) + " connections");
}

// The fixed display modes (--test-display-modes).
void OpenDisplayModes(TState &State, const std::filesystem::path &Path)
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
		State.Log.OpenSystemName(Arguments.Log.c_str(), TFile::TAccess::Write, TFile::TContent::Text); // stays closed where it fails
	if (!Arguments.Out.empty())
		OpenOut(State, Arguments.Out);
	if (!Arguments.Replay.empty())
		OpenReplay(State, Arguments.Replay);
	if (!Arguments.DisplayModes.empty())
		OpenDisplayModes(State, Arguments.DisplayModes);
	if (!Arguments.Control.empty())
		OpenControl(State, Arguments.Control);
	const std::filesystem::path &ScriptPath = Arguments.Script;
	if (Arguments.Headless && ScriptPath.empty())
		Fail(State, "--test-headless needs --test-script <file>: without a script the client takes the desktop's input");
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
	if (State.ControlOn)
		State.FramesWanted = true;
	State.Scheduler = TScheduler(std::move(Parsed.Script));
	State.Clock = &State.Virtual;
	State.Scripted = true;
	State.Headless = Arguments.Headless;
	if (State.Headless)
		WriteLog(State, "testmode: headless (no window shown, no Vulkan surface)");
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

// The main window: the SDL3 window's (issue #122).
HWND MainWindow()
{
	return static_cast<HWND>(patella::sdlframe::Handle());
}

POINT ScreenCursor(const TState &State, HWND Window)
{
	if (State.Scheduler.FixedCursor())
		return POINT{State.Scheduler.CursorX(), State.Scheduler.CursorY()};
	POINT Point = State.Cursor;
	if (Window != nullptr) {
		// Issue #123: the SDL3 window's coordinates, as the client converts the cursor back (sdlframe::ScreenToClient).
		int x = Point.x;
		int y = Point.y;
		patella::sdlframe::ClientToScreen(x, y);
		Point = POINT{x, y};
	}
	return Point;
}

void Dispatch(TState &State, HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	// Issue #123: the SDL3 window's input is SDL's events; an input message goes to it as the SDL event it stands for
	// (testmode/SdlInputWin32.h). Every other message is dispatched.
	if (patella::testmode::DeliverToSdlWindow(Message, WParam, LParam))
		return;
#if defined(_WIN32)
	MSG Msg = {};
	Msg.hwnd = Window;
	Msg.message = Message;
	Msg.wParam = WParam;
	Msg.lParam = LParam;
	Msg.time = State.Clock->Milliseconds();
	Msg.pt = ScreenCursor(State, Window);
	::DispatchMessageA(&Msg);
#else
	static_cast<void>(State);
	static_cast<void>(Window);
#endif
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
#if defined(_WIN32)
	::GetClientRect(Window, &Client);
#else
	int ClientWidth = 0;
	int ClientHeight = 0;
	patella::sdlframe::ClientSize(ClientWidth, ClientHeight);
	Client.right = ClientWidth;
	Client.bottom = ClientHeight;
#endif
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
#if defined(_WIN32)
		::PostMessageA(Window, WM_CLOSE, 0, 0);
#else
		Dispatch(State, Window, WM_CLOSE, 0, 0);
#endif
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
#if defined(_WIN32)
			::SendMessageA(Window, WM_TIMER, 1, 0);
#else
			Dispatch(State, Window, WM_TIMER, 1, 0);
#endif
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
#if defined(_WIN32)
	::SetEvent(State.ControlDone);
#else
	// Issue #131: the channel's event on Linux, a file in --test-out (tools/testmode_client.py).
	if (!SaveFile(State.Out / "control-done", std::string_view{"\n"}))
		WriteLog(State, "testmode: cannot write control-done");
#endif
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
#if defined(_WIN32)
	if (::WaitForSingleObject(State.ControlCommand, 0) != WAIT_OBJECT_0)
		return;
#else
	// Issue #131: the channel's event on Linux, a file in --test-out (tools/testmode_client.py).
	std::error_code ControlError;
	if (!std::filesystem::remove(State.Out / "control-command", ControlError))
		return;
#endif
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
	State.Replay.Pipes->Network().ServerClose(Socket.Pipe);
	State.Replay.Schedule.ClientClosed(Socket.Index);
	WriteLog(State, "testmode: replay: " + ReplayName(Socket) + " closed by the client (" + Why + ")");
}

// New connections, the client's bytes and closes.
void PumpReplay(TState &State)
{
	TReplayState &Replay = State.Replay;
	TPipeNetwork &Network = Replay.Pipes->Network();
	for (auto Accepted = Network.Accept(); Accepted.has_value(); Accepted = Network.Accept()) {
		TReplaySocket Socket;
		Socket.Pipe = Accepted->Id;
		Socket.Kind = Accepted->Kind;
		Socket.Index = Replay.Schedule.Open(Socket.Kind);
		Replay.Sockets.push_back(std::move(Socket));
		WriteLog(State, "testmode: replay: " + ReplayName(Replay.Sockets.back()) + " opened at " + std::to_string(State.Virtual.Elapsed()) + " ms");
	}
	for (TReplaySocket &Socket : Replay.Sockets) {
		while (!Socket.Closed) {
			std::array<std::uint8_t, 0x4000> Buffer{};
			const TPipeResult Received = Network.ServerReceive(Socket.Pipe, Buffer);
			if (Received.Count < 0 && Received.Error == TPipeError::WouldBlock)
				break;
			if (Received.Count <= 0) {
				CloseReplaySocket(State, Socket, Received.Count == 0 ? "end of stream" : "receive failed");
				break;
			}
			Socket.In.insert(Socket.In.end(), Buffer.begin(), Buffer.begin() + Received.Count);
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
		if (Socket.Unread > 0)
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
			Replay.Pipes->Network().ServerShutdown(Socket.Pipe);
			Socket.ServerClosing = true;
			Replay.Schedule.Take(Socket.Index);
			return true;
		}
		if (!Socket.HasKey)
			WriteLog(State, "testmode: replay: " + ReplayName(Socket) + ": no XTEA key, the packet goes out with a zero key");
		std::vector<std::uint8_t> Frame = EncryptFrame(Event->Payload, Socket.Key);
		WriteLog(State, std::to_string(Elapsed) + " ms, replay line " + std::to_string(Event->Line) + ": " + ReplayName(Socket) + " server packet, " + std::to_string(Event->Payload.size()) + " bytes");
		Socket.Unread++;
		Replay.Sent++;
		Replay.Schedule.Take(Socket.Index);
		if (Replay.Pipes->Network().ServerSend(Socket.Pipe, Frame).Count < 0)
			CloseReplaySocket(State, Socket, "send failed");
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

namespace {

// The cursor (IInput::CursorPosition): with a script the script's (fixed, or the virtual cursor on the main window),
// else the desktop's.
BOOL GetCursorPosition(LPPOINT Point)
{
	const TState &Current = State();
	if (!Current.Scripted) {
		// The window path's (issue #123: SDL's global mouse position on the SDL3 window, GetCursorPos on MFC's).
		patella::platform::TPoint Position;
		if (!patella::platform::WindowInput().CursorPosition(Position))
			return FALSE;
		*Point = POINT{Position.x, Position.y};
		return TRUE;
	}
	if (Current.Scheduler.FixedCursor()) {
		*Point = POINT{Current.Scheduler.CursorX(), Current.Scheduler.CursorY()};
		return TRUE;
	}
	*Point = ScreenCursor(Current, MainWindow());
	return TRUE;
}

// The main frame's timer (IInput::StartTimer): with a script none, the driver makes its ticks (Idle).
void SetMainTimer(HWND Window, unsigned int Id, unsigned int Elapse)
{
	TState &Current = State();
	if (!Current.Scripted) {
		patella::platform::WindowInput().StartTimer(Window, Id, Elapse); // issue #122: the SDL3 loop's, or SetTimer
		return;
	}
	WriteLog(Current, "testmode: the main timer's ticks come from the driver");
}

} // namespace

#if defined(_WIN32)
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
#endif

// Issue #52: the main window's WM_CAPTURECHANGED (MainFrm.cpp OnCaptureChanged). With a script the
// buttons are pressed and released only by the script's events, dispatched to the main window; the
// window loses the Windows capture only through the desktop (another window activated, the user's own
// clicks: a scripted run of the reference compare lost a click that way), which, as DropInput's input,
// must not reach the client. The client's own ReleaseCapture comes after it has cleared its button flags,
// when the handler does nothing. Since issue #119 TTestInput::IgnoreCaptureChange (below).

// Issue #48: the display modes of the Graphics Options dialog (vulkan/SurfaceVulkan.cpp
// GetFullscreenModes). With --test-display-modes the Index-th mode of the file; without it the machine's
// (Win32's EnumDisplaySettingsA; since issue #124 through the window path's IWindow, Win32's on both). Since issue #119
// TTestWindow::DisplayMode (below). The desktop's own mode (ENUM_CURRENT_SETTINGS, the Vulkan engine's CreateSurfaces)
// stays the machine's: TTestWindow::DesktopMode is the window path's (issue #138; #124).

// Issue #83: PATELLA_TEST_FAIL_PRESENT=1 makes every present of the Vulkan engine fail (TestHooks.h), read once.
bool PresentFails()
{
	static const bool Fails = [] {
		const char *const Value = std::getenv("PATELLA_TEST_FAIL_PRESENT"); // NOLINT(concurrency-mt-unsafe): read once, at the first present
		return Value != nullptr && std::string_view(Value) == "1";
	}();
	return Fails;
}

// Issue #90: PATELLA_TEST_FAIL_INIT=<n> makes InitControl fail before its step n (TestHooks.h), read once.
bool InitFails(int Step)
{
	static const int FailingStep = [] {
		const char *const Value = std::getenv("PATELLA_TEST_FAIL_INIT"); // NOLINT(concurrency-mt-unsafe): read once, at the first step
		if (Value == nullptr)
			return 0;
		const std::string_view Text(Value);
		int Parsed = 0;
		const auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Parsed); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): the end of the text
		return Error == std::errc{} && End == Text.data() + Text.size() ? Parsed : 0;              // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): the end of the text
	}();
	return FailingStep != 0 && Step == FailingStep;
}

namespace {

// Issue #96: the windows the count of the Tibia windows sees (TTestWindow::VisitWindows): this process's only, each
// with its title and class as Win32's TWin32Window reads them (GetWindowTextA, GetClassNameA with 30).
#if defined(_WIN32)
BOOL CALLBACK VisitOwnWindow(HWND Window, LPARAM Parameter)
{
	DWORD Process = 0;
	GetWindowThreadProcessId(Window, &Process);
	if (Process != GetCurrentProcessId())
		return TRUE;
	const auto Visit = reinterpret_cast<patella::platform::TWindowVisitor>(Parameter);
	TFixedString<32> WindowText;
	TFixedString<32> WindowClass;
	const int TextLength = GetWindowTextA(Window, WindowText, 30);
	const int ClassLength = GetClassNameA(Window, WindowClass, 30);
	Visit(WindowText, TextLength, WindowClass, ClassLength);
	return TRUE;
}
#endif

// The driver's step (IInput::BeginIdle, the start of CPlayerApp::OnIdle).
void Idle()
{
	TState &Current = State();
	if (!Current.Scripted) {
		// Issue #92: without a script the control channel answers probe only, at any idle call.
		if (Current.ControlOn && MainWindow() != nullptr)
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
	if (Current.ControlOn) {
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

} // namespace

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
	if (Index.OpenSystemName((Current.Out / L"frames.txt").c_str(), TFile::TAccess::Append)) {
		Index.Print("{} {} {} {} {} {} {}\n", Frame.Step, Frame.Requested, Frame.Line, Current.Virtual.Elapsed(), Width,
			Height, Filtered);
		Index.Close();
	}
	if (!Ok)
		WriteLog(Current, "testmode: cannot write " + Name);
}

// A message box (IWindow::ShowMessageBox) with a script: not shown. Without one TTestWindow shows the window path's
// (issue #124).
int ShowMessageBox([[maybe_unused]] HWND Window, const char *Text, const char *Caption, UINT Type)
{
	TState &Current = State();
	// A test run never opens a modal dialog: the box's caption and text are written down instead, and
	// it counts as answered with OK.
	const std::string Caption8 = Caption != nullptr ? Caption : "";
	const std::string Text8 = Text != nullptr ? Text : "";
	WriteLog(Current, "testmode: message box \"" + Caption8 + "\": " + Text8);
	if (!Current.Out.empty()) {
		TFile File;
		if (File.OpenSystemName((Current.Out / L"messages.txt").c_str(), TFile::TAccess::Append)) {
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
#if defined(_WIN32)
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
#else
		// Issue #131: what the box's modal loop did for the sockets (the connection's close): the socket's events.
		patella::platform::Platform().Socket.Poll();
#endif
		if (Activity)
			LastActivity = std::chrono::steady_clock::now();
		RealYield();
	}
	return IDOK;
}

} // namespace

void FrameObservers(int(PATELLA_CDECL **Frame)(int, int), void(PATELLA_CDECL **Pixels)(const void *, int, int, int),
	void(PATELLA_CDECL **Mask)(const unsigned char *, int, int), void(PATELLA_CDECL **Device)(const char *, int, int))
{
	const TState &Current = State();
	if (Current.ControlOn)
		*Device = DeviceSeen;
	if (!Current.FramesWanted)
		return;
	*Frame = FrameWanted;
	*Pixels = FramePixels;
	*Mask = FrameMask;
}

namespace {

// The test mode's IInput (platform/Input.h; issue #119): the hooks PATELLA_GET_CURSOR_POS, PATELLA_SET_MAIN_TIMER,
// PATELLA_TEST_IDLE and PATELLA_TEST_CAPTURE_CHANGED until then.
class TTestInput final : public patella::platform::IInput {
public:
	bool CursorPosition(patella::platform::TPoint &Point) override
	{
		POINT Cursor;
		if (GetCursorPosition(&Cursor) == FALSE)
			return false;
		Point.x = Cursor.x;
		Point.y = Cursor.y;
		return true;
	}
	void StartTimer(patella::platform::TWindowHandle Window, unsigned int Id, unsigned int Milliseconds) override
	{
		SetMainTimer(static_cast<HWND>(Window), Id, Milliseconds);
	}
	// KillTimer, with a script too (of a timer never set: nothing), as before #119; issue #122: the window path's.
	void StopTimer(patella::platform::TWindowHandle Window, unsigned int Id) override
	{
		patella::platform::WindowInput().StopTimer(Window, Id);
	}
	void BeginIdle() override { Idle(); }
	// Issue #52: with a script every WM_CAPTURECHANGED is the desktop's (above).
	[[nodiscard]] bool IgnoreCaptureChange() override { return State().Scripted; }
	// Issue #123: with a script the desktop's SDL events are dropped, as DropInput drops Windows' input messages.
	[[nodiscard]] bool IgnoreDesktopInput() override { return State().Scripted; }
};

// The test mode's IWindow (platform/Window.h; issue #119): the hooks PATELLA_MESSAGE_BOX, PATELLA_ENUM_WINDOWS and
// PATELLA_ENUM_DISPLAY_SETTINGS until then.
class TTestWindow final : public patella::platform::IWindow {
public:
	void ShowMessageBox(patella::platform::TWindowHandle Owner, const char *Text, const char *Caption,
		patella::platform::TMessageBoxIcon Icon) override
	{
		// Without a script the window path's box (issue #124: SDL3's on the SDL3 window, MessageBoxA on MFC's).
		if (!State().Scripted) {
			patella::platform::WindowDesktop().ShowMessageBox(Owner, Text, Caption, Icon);
			return;
		}
		// messages.txt's type: MessageBoxA's flags, as the hook passed them.
		const UINT Type = Icon == patella::platform::TMessageBoxIcon::Error ? MB_ICONHAND : MB_ICONEXCLAMATION;
		patella::testmode::ShowMessageBox(static_cast<HWND>(Owner), Text, Caption, Type);
	}
	void VisitWindows(patella::platform::TWindowVisitor Visit) override
	{
#if defined(_WIN32)
		EnumWindows(VisitOwnWindow, reinterpret_cast<LPARAM>(Visit));
#else
		patella::platform::SdlWindow().VisitOwnWindows(Visit); // issue #131: the process's own, SDL's list
#endif
	}
	[[nodiscard]] bool DisplayMode(unsigned int Index, patella::platform::TDisplayMode &Mode) override
	{
		const TState &Current = State();
		if (!Current.FixedDisplayModes)
			return patella::platform::WindowDesktop().DisplayMode(Index, Mode);
		TDisplayMode Found;
		if (!EnumerateDisplayMode(&Current.DisplayModes, Index, &Found, [](std::uint32_t, TDisplayMode *) { return false; }))
			return false;
		Mode.Width = static_cast<int>(Found.Width);
		Mode.Height = static_cast<int>(Found.Height);
		Mode.Depth = static_cast<int>(Found.Depth);
		return true;
	}
	// The machine's, with or without --test-display-modes (issue #138: the Vulkan engine's own call until then).
	[[nodiscard]] bool DesktopMode(patella::platform::TDisplayMode &Mode) override
	{
		return patella::platform::WindowDesktop().DesktopMode(Mode);
	}
	// The machine's display (issue #124: the Vulkan engine's ChangeDisplaySettingsA until then); headless the engine
	// never asks (SurfaceVulkan.cpp CreateSurfaces).
	bool SetFullscreenMode(patella::platform::TWindowHandle Window, const patella::platform::TDisplayMode *Mode) override
	{
		return patella::platform::WindowDesktop().SetFullscreenMode(Window, Mode);
	}
	// Issue #145: --test-headless.
	[[nodiscard]] bool Headless() override { return State().Headless; }
};

constinit TTestInput TestInput;
constinit TTestWindow TestWindow;

} // namespace

} // namespace patella::testmode

// The test build's platform (issue #109; platform/Platform.h): the clock is the test mode's (the virtual clock with a
// script, the system's without one, as Initialize chose it), the files Win32's (the comparisons read the files the
// client writes in its folder), the network the server replay's inside the process with --test-replay (issue #120:
// TPipeSocketWin32, no port), else the portable socket's (TPortableSocket, issue #125: a live run against the test
// server). Issue #119: the clipboard the process's own with a script (TMemoryClipboard: the desktop's clipboard
// neither reaches a scripted run nor is changed by it), else the window's (issue #124: SDL3's); the input and the
// windows the test mode's (above).
patella::platform::TPlatform &patella::platform::Platform()
{
	const patella::testmode::TState &Current = patella::testmode::State();
	static TMemoryClipboard ScriptClipboard;
	static TPlatform Instance{*Current.Clock, SystemFileSystem(),
		Current.Replay.Active ? static_cast<ISocket &>(*Current.Replay.Pipes) : static_cast<ISocket &>(NetworkSocket()),
		Current.Scripted ? static_cast<IClipboard &>(ScriptClipboard) : WindowClipboard(),
		patella::testmode::TestInput, patella::testmode::TestWindow};
	return Instance;
}
