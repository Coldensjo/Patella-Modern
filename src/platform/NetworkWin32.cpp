// NetworkWin32.cpp: the Windows client's network, TPortableSocket (platform/PortableSocket.h) with what the client's
// window needs of it (Patella-Modern issue #125; notes/decisions.md, "#125"). Not part of the original source.
// Patella.exe and PatellaTest.exe compile it; the golden runner has its own NetworkSocket()
// (tests/golden/network/MfcSocket.cpp).
//
// - A finished lookup goes to the window as WSAAsyncGetHostByName's reply did: on the SDL3 path to SdlFrame.cpp's
//   OnHostResolved (HostResolved: no window message), on MFC's (PATELLA_WINDOW=mfc, until issue #127) as WM_USER + 1
//   to CMainFrame's message map (SendMessage from the loop's thread, with WSAMAKEASYNCREPLY's LPARAM).
// - While the socket has a connection or a lookup, a thread timer (SetTimer without a window, USER_TIMER_MINIMUM) polls
//   it from every message loop that dispatches the thread's messages: the main loop's pump, and the modal loops the
//   idle step does not run in (a message box's: FatalError's ShutdownConnection, whose close the original handled in
//   the box's loop; the window's move and size loops), where MFC's socket window got WinSock's messages.

#include "platform/Win32Platform.h"

#include "platform/PortableSocket.h"
#include "platform/Socket.h"

#if defined(PATELLA_SDL3)
#include "SdlFrame.h"
#include "platform/WindowChoice.h"
#endif

namespace patella::platform {

namespace {

UINT_PTR PollTimer = 0;

TPortableSocket &Instance();

void CALLBACK PollOnTimer([[maybe_unused]] HWND Window, [[maybe_unused]] UINT Message, [[maybe_unused]] UINT_PTR Id,
	[[maybe_unused]] DWORD Time)
{
	Instance().Poll();
}

void Busy(bool Now)
{
	if (Now && PollTimer == 0) {
		PollTimer = ::SetTimer(nullptr, 0, USER_TIMER_MINIMUM, PollOnTimer);
	} else if (!Now && PollTimer != 0) {
		::KillTimer(nullptr, PollTimer);
		PollTimer = 0;
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
