// Win32Platform.cpp: the Win32 implementations of the platform's interfaces (Patella-Modern issue #109;
// platform/Win32Platform.h). Not part of the original source: each function is the call the client made itself
// before #109 (sysinc.cpp's timeGetTime, the wall clock's time, the frame timer's Sleep, Network.cpp's CAsyncSocket
// and WinSock calls), with the same arguments.

#include "platform/Win32Platform.h"

#include "common.h"
#include "platform/Platform.h"

#include <mmsystem.h>

#include <cstdint>
#include <ctime>

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace patella::platform {

std::uint32_t TWin32Clock::Milliseconds()
{
	return timeGetTime();
}

std::int64_t TWin32Clock::WallSeconds()
{
	return std::time(nullptr);
}

void TWin32Clock::FrameWait(std::uint32_t Duration)
{
	Sleep(Duration);
}

bool TWin32Socket::Startup()
{
	return AfxSocketInit(nullptr) != FALSE;
}

int TWin32Socket::LastError()
{
	return WSAGetLastError();
}

TSocket *TWin32Socket::New(ISocketEvents &Events)
{
	return new TSocket(Events); // NOLINT(cppcoreguidelines-owning-memory): TNetwork's Socket, released by Delete
}

void TWin32Socket::Delete(TSocket *Connection)
{
	delete Connection; // NOLINT(cppcoreguidelines-owning-memory): New's
}

bool TWin32Socket::Create(TSocket *Connection)
{
	return Connection->Create(0, SOCK_STREAM, FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE, nullptr)
		!= FALSE;
}

// Connection may be null: TNetwork's ConnectSocket without a socket calls it so, as the original called
// CAsyncSocket::Connect (the golden Network test's rounds reach it; tests/golden/network/CMakeLists.txt).
bool TWin32Socket::Connect(TSocket *Connection, const char *Address, int Port)
{
	return Connection->Connect(Address, Port) != FALSE;
}

int TWin32Socket::Send(TSocket *Connection, const void *Data, int Length)
{
	return Connection->Send(Data, Length, 0);
}

int TWin32Socket::Receive(TSocket *Connection, void *Buffer, int Length)
{
	return Connection->Receive(Buffer, Length, 0);
}

void TWin32Socket::Shutdown(TSocket *Connection, TShutdown How)
{
	shutdown(Connection->m_hSocket, How == TShutdown::Send ? SD_SEND : SD_BOTH);
}

void TWin32Socket::Close(TSocket *Connection)
{
	Connection->Close();
}

void *TWin32Socket::ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer, int Length)
{
	return WSAAsyncGetHostByName(static_cast<HWND>(Window), Message, Hostname, Buffer, Length);
}

void TWin32Socket::CancelResolve(void *Request)
{
	WSACancelAsyncRequest(Request);
}

const char *TWin32Socket::ResolvedAddress(const char *Buffer)
{
	return inet_ntoa(*reinterpret_cast<const in_addr *>(reinterpret_cast<const HOSTENT *>(Buffer)->h_addr_list[0]));
}

// Stateless, with trivial destructors: constant-initialised, so they are there before any static initialiser and
// after every static destructor.
namespace {
constinit TWin32Clock Win32ClockInstance;
constinit TSystemFileSystem SystemFileSystemInstance;
constinit TWin32Socket Win32SocketInstance;
} // namespace

TWin32Clock &Win32Clock()
{
	return Win32ClockInstance;
}

TSystemFileSystem &SystemFileSystem()
{
	return SystemFileSystemInstance;
}

TWin32Socket &Win32Socket()
{
	return Win32SocketInstance;
}

#if !defined(PATELLA_TEST_MODE)
// Patella.exe's and the golden runner's platform (Platform.h); the test build's is TestModeWin32.cpp's.
TPlatform &Platform()
{
	static TPlatform Instance{Win32ClockInstance, SystemFileSystemInstance, Win32SocketInstance};
	return Instance;
}
#endif

} // namespace patella::platform
