// NetworkLinux.cpp: the Linux client's network, TPortableSocket (platform/PortableSocket.h; Patella-Modern issue #131,
// plan M8), the counterpart of NetworkWin32.cpp. There is no message loop to poll a socket from: the loop's idle step does
// (Player.cpp OnIdle, ISocket::Poll), so the Busy callback has nothing to do. A finished lookup goes to SdlFrame.cpp's
// OnHostResolved, as on Windows.

#include "SdlFrame.h"

#include "platform/PortableSocket.h"
#include "platform/Process.h"
#include "platform/Socket.h"

namespace patella::platform {

namespace {

void Resolved([[maybe_unused]] void *Window, [[maybe_unused]] unsigned int Message, [[maybe_unused]] void *Request, int Error)
{
	sdlframe::HostResolved(Error);
}

void Busy([[maybe_unused]] bool Now)
{
}

} // namespace

ISocket &NetworkSocket()
{
	// Made on first use and never destroyed (NetworkWin32.cpp's): a lookup still running at the exit is not waited for.
	static TPortableSocket &Socket = *new TPortableSocket(Resolved, Busy); // NOLINT(cppcoreguidelines-owning-memory)
	return Socket;
}

} // namespace patella::platform
