// TestModeWin32.cpp: the test mode of the Windows test build (issue #21; notes/modernization-plan.md,
// 4.4; src/TestHooks.h). Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake); Patella.exe and
// the oracle-x86 build contain none of it.
//
//	PatellaTest.exe [--test-script <file>] [--test-log <file>] [the client's own words]
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
// A script that does not parse ends the program before the window opens, with exit code 3 and the
// message on stderr, in the log and through OutputDebugString (never a dialog).

#include "common.h"

#include "MainFrm.h"
#include "testmode/Clock.h"
#include "testmode/InputScript.h"

#include <mmsystem.h>
#include <shellapi.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

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

// The arguments --test-script and --test-log of the command line.
void ReadArguments(std::wstring *Script, std::wstring *Log)
{
	int Count = 0;
	LPWSTR *Arguments = ::CommandLineToArgvW(::GetCommandLineW(), &Count);
	if (Arguments == nullptr)
		return;
	for (int i = 1; i + 1 < Count; i++) {
		if (wcscmp(Arguments[i], L"--test-script") == 0)
			*Script = Arguments[i + 1];
		else if (wcscmp(Arguments[i], L"--test-log") == 0)
			*Log = Arguments[i + 1];
	}
	::LocalFree(Arguments);
}

void Initialize(TState &State)
{
	std::wstring ScriptPath, LogPath;
	ReadArguments(&ScriptPath, &LogPath);
	if (!LogPath.empty()) {
		if (_wfopen_s(&State.Log, LogPath.c_str(), L"w") != 0)
			State.Log = nullptr;
	}
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
	case TEventKind::Quit: ::PostMessageA(Window, WM_CLOSE, 0, 0); break;
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
	if (const TInputEvent *Event = Current.Scheduler.TakeDue(Elapsed)) {
		Deliver(Current, Window, *Event);
		return;
	}
	if (Current.Scheduler.Finished() && !Current.FinishReported) {
		Current.FinishReported = true;
		WriteLog(Current, "testmode: the script ends at " + std::to_string(Elapsed) + " ms; the clock goes on");
	}
	std::uint32_t Stop = Current.Scheduler.NextStop(Elapsed);
	auto Real = std::chrono::duration_cast<std::chrono::milliseconds>(Now - Current.RealStart).count();
	if (static_cast<std::uint64_t>(Real) < Current.Scheduler.RealDeadline(Stop))
		return;
	Current.Virtual.AdvanceTo(Stop);
	if (TScheduler::IsTick(Stop))
		Dispatch(Current, Window, WM_TIMER, 1, 0);
}

} // namespace patella::testmode
