// TestModeWin32.cpp: the test mode of the Windows test build (issue #21; notes/modernization-plan.md,
// 4.4; src/TestHooks.h). Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake); Patella.exe and
// the oracle-x86 build contain none of it.
//
//	PatellaTest.exe [--test-script <file>] [--test-log <file>] [--test-out <folder>] [--test-sync <name>]
//	                [the client's own words]
//
// Without --test-script the test build is the client on the system clock (TSystemClock: timeGetTime,
// _time64, Sleep), through the same hooks. With a script (src/testmode/InputScript.h):
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
//   observer (the one patcap.dll otherwise is); frames.txt lists them.
//
// --test-sync <name> (tools/testmode_check.py): after writing a state dump the client sets the event
// <name>-ready and waits for <name>-continue (both made by the checker), so that client_state.py reads
// the same moment from outside; the wait does not count for the pace (the virtual clock does not see
// it). A script with dump or frame needs --test-out.
//
// A script that does not parse ends the program before the window opens, with exit code 3 and the
// message on stderr, in the log and through OutputDebugString (never a dialog).

#include "common.h"

#include "MainFrm.h"
#include "testmode/ClientStateWin32.h"
#include "testmode/Clock.h"
#include "testmode/InputScript.h"
#include "testmode/PacketLog.h"
#include "testmode/Png.h"
#include "testmode/StateDump.h"

#include <mmsystem.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if !defined(PATELLA_TEST_MODE)
#error "TestModeWin32.cpp belongs to the test build (PATELLA_TEST_MODE)"
#endif

namespace patella::testmode {

namespace {

// Exit code of a script that cannot be read or does not parse.
constexpr UINT ScriptErrorExitCode = 3;

class TSystemClock final : public IClock {
public:
	[[nodiscard]] std::uint32_t Milliseconds() override { return ::timeGetTime(); }
	[[nodiscard]] std::int64_t WallSeconds() override { return _time64(nullptr); }
	void FrameWait(std::uint32_t Duration) override { ::Sleep(Duration); }
};

void RealYield()
{
	::Sleep(1);
}

// How long a state dump waits for the checker's <name>-continue.
constexpr DWORD SyncTimeout = 300000;

// A frame dump asked for by the script, not yet shown.
struct TPendingFrame {
	int Step = 0;
	int Line = 0;
	std::uint32_t Requested = 0;
};

struct TState {
	TSystemClock System;
	TVirtualClock Virtual{RealYield};
	IClock *Clock = &System;
	bool Scripted = false;
	TScheduler Scheduler;
	POINT Cursor = {-1, -1}; // client coordinates
	std::FILE *Log = nullptr;
	bool Started = false;
	std::chrono::steady_clock::time_point RealStart;
	bool FinishReported = false;

	// Issue #22: the dumps.
	std::filesystem::path Out; // --test-out; empty: no dumps
	std::FILE *Packets = nullptr;
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
	HANDLE SyncReady = nullptr;
	HANDLE SyncContinue = nullptr;
};

void WriteLog(TState &State, const std::string &Line)
{
	if (State.Log == nullptr)
		return;
	std::fputs(Line.c_str(), State.Log);
	std::fputc('\n', State.Log);
	std::fflush(State.Log);
}

[[noreturn]] void Fail(TState &State, const std::string &Message)
{
	std::string Line = "testmode: " + Message;
	WriteLog(State, Line);
	std::fputs((Line + "\n").c_str(), stderr);
	std::fflush(stderr);
	::OutputDebugStringA((Line + "\n").c_str());
	::ExitProcess(ScriptErrorExitCode);
}

std::string Narrow(const std::wstring &Text)
{
	int Length = ::WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), nullptr, 0, nullptr, nullptr);
	std::string Result(static_cast<std::size_t>(Length > 0 ? Length : 0), '\0');
	if (Length > 0)
		::WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), static_cast<int>(Text.size()), Result.data(), Length, nullptr, nullptr);
	return Result;
}

struct TArguments {
	std::wstring Script;
	std::wstring Log;
	std::wstring Out;
	std::wstring Sync;
};

// The arguments --test-script, --test-log, --test-out and --test-sync of the command line.
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
		else if (wcscmp(Arguments[i], L"--test-sync") == 0)
			Result.Sync = Arguments[i + 1];
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
	std::filesystem::path Log = State.Out / L"packets.log";
	if (_wfopen_s(&State.Packets, Log.c_str(), L"wb") != 0 || State.Packets == nullptr)
		Fail(State, "cannot write " + Narrow(Log.wstring()));
	std::fputs((std::string(PacketLogHeader) + "\n").c_str(), State.Packets);
	std::fflush(State.Packets);
}

// The checker's two events (--test-sync).
void OpenSync(TState &State, const std::wstring &Name)
{
	State.SyncReady = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, (Name + L"-ready").c_str());
	State.SyncContinue = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, (Name + L"-continue").c_str());
	if (State.SyncReady == nullptr || State.SyncContinue == nullptr)
		Fail(State, "--test-sync " + Narrow(Name) + ": the events " + Narrow(Name) + "-ready and -continue do not exist");
}

void Initialize(TState &State)
{
	TArguments Arguments = ReadArguments();
	if (!Arguments.Log.empty()) {
		if (_wfopen_s(&State.Log, Arguments.Log.c_str(), L"w") != 0)
			State.Log = nullptr;
	}
	if (!Arguments.Out.empty())
		OpenOut(State, Arguments.Out);
	if (!Arguments.Sync.empty())
		OpenSync(State, Arguments.Sync);
	const std::wstring &ScriptPath = Arguments.Script;
	if (ScriptPath.empty())
		return;
	std::ifstream In(ScriptPath, std::ios::binary);
	if (!In)
		Fail(State, "cannot read the script " + Narrow(ScriptPath));
	std::string Text((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	TParseResult Parsed = ParseScript(Text);
	if (!Parsed.Ok)
		Fail(State, Narrow(ScriptPath) + ":" + std::to_string(Parsed.ErrorLine) + ": " + Parsed.Error);
	WriteLog(State, "testmode: script " + Narrow(ScriptPath) + ", " + std::to_string(Parsed.Script.Events.size()) + " events, pace " + std::to_string(Parsed.Script.RealPerTick) + " ms per 100 ms");
	bool Dumps = false;
	for (const TInputEvent &Event : Parsed.Script.Events) {
		if (Event.Kind == TEventKind::StateDump || Event.Kind == TEventKind::FrameDump)
			Dumps = true;
		if (Event.Kind == TEventKind::FrameDump)
			State.FramesWanted = true;
	}
	if (Dumps && State.Out.empty())
		Fail(State, Narrow(ScriptPath) + ": the script has dump or frame commands: --test-out <folder> is needed");
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

// The state dump of a dump command; with --test-sync the checker reads the same moment, and the wait
// does not count for the pace.
void WriteStateDump(TState &State, const TInputEvent &Event)
{
	TStateDump Dump;
	Dump.Step = ++State.StateSteps;
	Dump.Time = State.Virtual.Elapsed();
	Dump.Line = Event.Line;
	Dump.State = ReadClientState();
	Dump.Gui = ReadGui();
	std::string Name = Numbered("state", Dump.Step, ".json");
	std::string Json = StateDumpJson(Dump);
	if (!SaveFile(State.Out / Name, Json))
		WriteLog(State, "testmode: cannot write " + Name);
	if (State.SyncReady == nullptr)
		return;
	auto Before = std::chrono::steady_clock::now();
	::SetEvent(State.SyncReady);
	if (::WaitForSingleObject(State.SyncContinue, SyncTimeout) != WAIT_OBJECT_0)
		WriteLog(State, "testmode: " + Name + ": no answer from the checker (--test-sync)");
	State.RealStart += std::chrono::steady_clock::now() - Before;
}

void Deliver(TState &State, HWND Window, const TInputEvent &Event)
{
	WriteLog(State, Describe(Event));
	RECT Client = {};
	::GetClientRect(Window, &Client);
	POINT Point = {Event.X.Resolve(Client.right), Event.Y.Resolve(Client.bottom)};
	WPARAM MouseFlags = 0;
	if ((Event.Buttons & ButtonLeft) != 0)
		MouseFlags |= MK_LBUTTON;
	if ((Event.Buttons & ButtonRight) != 0)
		MouseFlags |= MK_RBUTTON;
	if ((Event.Modifiers & ModifierShift) != 0)
		MouseFlags |= MK_SHIFT;
	if ((Event.Modifiers & ModifierControl) != 0)
		MouseFlags |= MK_CONTROL;
	LPARAM PointParam = MAKELPARAM(Point.x, Point.y);
	bool Alt = (Event.Modifiers & ModifierAlt) != 0 || Event.Key == VK_MENU;
	switch (Event.Kind) {
	case TEventKind::KeyDown:
	case TEventKind::KeyUp: {
		bool Up = Event.Kind == TEventKind::KeyUp;
		// With Alt held, and for Alt and F10 themselves, Windows sends the system key messages.
		bool System = Alt || Event.Key == VK_F10;
		UINT Message = Up ? (System ? WM_SYSKEYUP : WM_KEYUP) : (System ? WM_SYSKEYDOWN : WM_KEYDOWN);
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
		POINT Screen = ScreenCursor(State, Window);
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
	}
}

} // namespace

DWORD TimeGetTime()
{
	return State().Clock->Milliseconds();
}

__time32_t WallTime32(__time32_t *Result)
{
	auto Now = static_cast<__time32_t>(State().Clock->WallSeconds());
	if (Result != nullptr)
		*Result = Now;
	return Now;
}

time_t WallTime(time_t *Result)
{
	auto Now = static_cast<time_t>(State().Clock->WallSeconds());
	if (Result != nullptr)
		*Result = Now;
	return Now;
}

void FrameSleep(DWORD Milliseconds)
{
	State().Clock->FrameWait(Milliseconds);
}

BOOL GetCursorPosition(LPPOINT Point)
{
	TState &Current = State();
	if (!Current.Scripted)
		return ::GetCursorPos(Point);
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
	TState &Current = State();
	if (!Current.Scripted)
		return false;
	UINT Kind = Message->message;
	if ((Kind >= WM_KEYFIRST && Kind <= WM_KEYLAST) || (Kind >= WM_MOUSEFIRST && Kind <= WM_MOUSELAST))
		return true;
	return Kind == WM_TIMER && Message->wParam == 1 && Message->hwnd == MainWindow();
}

void Idle()
{
	TState &Current = State();
	if (!Current.Scripted)
		return;
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
	// A state dump takes no idle call of its own: with or without dumps the client gets the same events in
	// the same idle calls. (A frame dump waits for the next frame, so it is an event in its turn.)
	const TInputEvent *Event = Current.Scheduler.TakeDue(Elapsed);
	while (Event != nullptr && Event->Kind == TEventKind::StateDump) {
		Deliver(Current, Window, *Event);
		Event = Current.Scheduler.TakeDue(Elapsed);
	}
	if (Event != nullptr) {
		Deliver(Current, Window, *Event);
		return;
	}
	if (Current.Scheduler.Finished() && !Current.FinishReported) {
		Current.FinishReported = true;
		WriteLog(Current, "testmode: the script ends at " + std::to_string(Elapsed) + " ms; the clock goes on");
	}
	std::uint32_t Stop = Current.Scheduler.NextStop(Elapsed);
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
	std::fputs((PacketLogLine(Record) + "\n").c_str(), State.Packets);
	std::fflush(State.Packets);
}

} // namespace

void RsaBlock(const unsigned char *Block)
{
	TState &Current = State();
	if (Current.Packets == nullptr)
		return;
	std::copy_n(Block, RsaBlockSize, Current.Rsa.begin());
	Current.HasRsa = true;
}

void PacketSent(const unsigned char *Data, int Size, int Padded)
{
	TState &Current = State();
	if (Current.Packets == nullptr || Size < 0)
		return;
	// SendData: [u16][u16 Size][payload][padding, up to Padded + 2 bytes].
	std::span<const unsigned char> Payload(Data + 4, static_cast<std::size_t>(Size));
	if (Padded == 0) {
		std::vector<unsigned char> Plain(Payload.begin(), Payload.end());
		if (Current.HasRsa && Plain.size() >= RsaBlockSize)
			std::copy(Current.Rsa.begin(), Current.Rsa.end(), Plain.end() - static_cast<std::ptrdiff_t>(RsaBlockSize));
		Current.HasRsa = false;
		WritePacket(Current, Current.Connections.Sent(PacketTime(Current), true, Plain, {}));
		return;
	}
	std::size_t Padding = Padded >= Size + 2 ? static_cast<std::size_t>(Padded - Size - 2) : 0;
	std::span<const unsigned char> Pad(Data + 4 + Size, Padding);
	WritePacket(Current, Current.Connections.Sent(PacketTime(Current), false, Payload, Pad));
}

void PacketReceived(const unsigned char *Data, int PacketSize)
{
	TState &Current = State();
	if (Current.Packets == nullptr || PacketSize < 2)
		return;
	// ReceiveData: [u16 PacketSize][u16 length][payload][padding, up to PacketSize + 2 bytes].
	int Length = (std::min)(Data[2] | (Data[3] << 8), PacketSize - 2);
	std::span<const unsigned char> Payload(Data + 4, static_cast<std::size_t>(Length));
	std::span<const unsigned char> Padding(Data + 4 + Length, static_cast<std::size_t>(PacketSize - 2 - Length));
	WritePacket(Current, Current.Connections.Received(PacketTime(Current), Payload, Padding));
}

// --- the frame dump: the Vulkan engine's frame observer (src/vulkan/SurfaceVulkan.cpp) ---

namespace {

int __cdecl FrameWanted(int /*Width*/, int /*Height*/)
{
	return State().PendingFrames.empty() ? 0 : 1;
}

void __cdecl FrameMask(const unsigned char *Mask, int Width, int Height)
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

void __cdecl FramePixels(const void *Bits, int Pitch, int Width, int Height)
{
	TState &Current = State();
	if (Current.PendingFrames.empty())
		return;
	TPendingFrame Frame = Current.PendingFrames.front();
	Current.PendingFrames.pop_front();
	std::vector<std::uint8_t> Rgb = BgrxToRgb(static_cast<const std::uint8_t *>(Bits), static_cast<std::size_t>(Pitch), Width, Height);
	std::string Name = Numbered("frame", Frame.Step, ".png");
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
	std::FILE *Index = nullptr;
	if (_wfopen_s(&Index, (Current.Out / L"frames.txt").c_str(), L"ab") == 0 && Index != nullptr) {
		std::fprintf(Index, "%d %u %d %u %d %d %lld\n", Frame.Step, Frame.Requested, Frame.Line, Current.Virtual.Elapsed(),
			Width, Height, Filtered);
		std::fclose(Index);
	}
	if (!Ok)
		WriteLog(Current, "testmode: cannot write " + Name);
}

} // namespace

void FrameObservers(HMODULE Patcap, int(__cdecl **Frame)(int, int), void(__cdecl **Pixels)(const void *, int, int, int),
	void(__cdecl **Mask)(const unsigned char *, int, int))
{
	TState &Current = State();
	if (Patcap != nullptr || !Current.FramesWanted)
		return;
	*Frame = FrameWanted;
	*Pixels = FramePixels;
	*Mask = FrameMask;
}

} // namespace patella::testmode
