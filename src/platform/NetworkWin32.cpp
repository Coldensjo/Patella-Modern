// NetworkWin32.cpp: the Windows client's network, TPortableSocket (platform/PortableSocket.h) with what the client's
// window needs of it (Patella-Modern issue #125; notes/decisions.md, "#125"). Not part of the original source.
// Patella.exe and PatellaTest.exe compile it; the golden runner has its own NetworkSocket()
// (tests/golden/network/MfcSocket.cpp).
//
// - A finished lookup goes to the window as WSAAsyncGetHostByName's reply did: on the SDL3 path to SdlFrame.cpp's
//   OnHostResolved (HostResolved: no window message), on MFC's (PATELLA_WINDOW=mfc, until issue #127) as WM_USER + 1
//   to CMainFrame's message map (SendMessage from the loop's thread, with WSAMAKEASYNCREPLY's LPARAM).
// - While the socket has a connection or a lookup, a timer (USER_TIMER_MINIMUM) of a message-only window polls it from
//   every message loop that dispatches the thread's messages: the main loop's pump, and the modal loops the idle step
//   does not run in (a message box's: FatalError's ShutdownConnection, whose close the original handled in the box's
//   loop; the window's move and size loops), where MFC's socket window got WinSock's messages. A window's, as MFC's
//   socket window: an exception of an event's handler leaves DispatchMessage, where a TIMERPROC's is swallowed.

#include "platform/Win32Platform.h"

#include "platform/PortableSocket.h"
#include "platform/Socket.h"

#if defined(PATELLA_SDL3)
#include "SdlFrame.h"
#include "platform/WindowChoice.h"
#endif

namespace patella::platform {

namespace {

// The polling timer's message-only window (made at the first connection or lookup) and whether its timer runs.
HWND PollWindow = nullptr;
bool PollTimer = false;
constexpr UINT_PTR PollTimerId = 1;
constexpr wchar_t PollWindowClass[] = L"PatellaSocketPoll";

TPortableSocket &Instance();

// A window procedure, not a TIMERPROC: Windows swallows an exception thrown out of a TIMERPROC
// (UOI_TIMERPROC_EXCEPTION_SUPPRESSION), and a packet handler's TMyException (TERROR: an unknown packet type) must reach
// the loop's handlers as it did out of MFC's socket window (DispatchMessage passes it on).
LRESULT CALLBACK PollWindowProcedure(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	if (Message == WM_TIMER && WParam == PollTimerId) {
		Instance().Poll();
		return 0;
	}
	return ::DefWindowProcW(Window, Message, WParam, LParam);
}

bool MakePollWindow()
{
	if (PollWindow != nullptr)
		return true;
	HINSTANCE Module = ::GetModuleHandleW(nullptr);
	WNDCLASSEXW Class = {};
	Class.cbSize = sizeof(Class);
	Class.lpfnWndProc = PollWindowProcedure;
	Class.hInstance = Module;
	Class.lpszClassName = PollWindowClass;
	if (::RegisterClassExW(&Class) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		return false;
	PollWindow = ::CreateWindowExW(0, PollWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, Module, nullptr);
	return PollWindow != nullptr;
}

void Busy(bool Now)
{
	if (Now && !PollTimer && MakePollWindow()) {
		PollTimer = ::SetTimer(PollWindow, PollTimerId, USER_TIMER_MINIMUM, nullptr) != 0;
	} else if (!Now && PollTimer) {
		::KillTimer(PollWindow, PollTimerId);
		PollTimer = false;
	}
}

void Resolved(void *Window, unsigned int Message, void *Request, int Error)
{
#if defined(PATELLA_SDL3)
	if (UseSdlWindow()) {
		sdlframe::HostResolved(Error);
		return;
	}
#endif
	if (Window != nullptr)
		::SendMessageA(static_cast<HWND>(Window), Message, reinterpret_cast<WPARAM>(Request),
			MAKELPARAM(0, static_cast<WORD>(Error))); // WSAMAKEASYNCREPLY(0, Error)
}

TPortableSocket &Instance()
{
	// Made on first use and never destroyed, as the other platform objects: a lookup still running at the exit is not
	// waited for.
	static TPortableSocket &Socket = *new TPortableSocket(Resolved, Busy); // NOLINT(cppcoreguidelines-owning-memory)
	return Socket;
}

} // namespace

ISocket &NetworkSocket()
{
	return Instance();
}

} // namespace patella::platform
