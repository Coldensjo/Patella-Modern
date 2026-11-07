// MfcSocket.cpp: the golden runner's ISocket, MFC's CAsyncSocket and WinSock (MfcSocket.h; Patella-Modern issue #125,
// until then src/platform/Win32Platform.cpp's TWin32Socket). Each function is the call Network.cpp made itself before
// issue #109, with the same arguments; the runner's Platform() (src/platform/Win32Platform.cpp) gives it to TNetwork.

#include "MfcSocket.h"

#include "common.h"
#include "platform/Platform.h"
#include "platform/Win32Platform.h"

namespace patella::platform {

// ISocket's numbers are WinSock's (platform/Socket.h, issue #110): TMfcSocket passes WinSock's on unchanged.
static_assert(socket_error::Invalid == WSAEINVAL && socket_error::WouldBlock == WSAEWOULDBLOCK
	&& socket_error::MessageSize == WSAEMSGSIZE && socket_error::AddressNotAvailable == WSAEADDRNOTAVAIL
	&& socket_error::NetworkDown == WSAENETDOWN && socket_error::NetworkUnreachable == WSAENETUNREACH
	&& socket_error::TimedOut == WSAETIMEDOUT && socket_error::ConnectionRefused == WSAECONNREFUSED
	&& socket_error::HostUnreachable == WSAEHOSTUNREACH && socket_error::HostNotFound == WSAHOST_NOT_FOUND
	&& socket_error::TryAgain == WSATRY_AGAIN && socket_error::NoData == WSANO_DATA);
static_assert(SocketFailed == SOCKET_ERROR && HostEntrySize == MAXGETHOSTSTRUCT);

bool TMfcSocket::Startup()
{
	return AfxSocketInit(nullptr) != FALSE;
}

int TMfcSocket::LastError()
{
	return WSAGetLastError();
}

TSocket *TMfcSocket::New(ISocketEvents &Events)
{
	return new TSocket(Events); // NOLINT(cppcoreguidelines-owning-memory): TNetwork's Socket, released by Delete
}

void TMfcSocket::Delete(TSocket *Connection)
{
	delete Connection; // NOLINT(cppcoreguidelines-owning-memory): New's
}

bool TMfcSocket::Create(TSocket *Connection)
{
	return Connection->Create(0, SOCK_STREAM, FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE, nullptr)
		!= FALSE;
}

// Connection may be null: TNetwork's ConnectSocket without a socket calls it so, as the original called
// CAsyncSocket::Connect (the golden Network test's rounds reach it; tests/golden/network/CMakeLists.txt).
bool TMfcSocket::Connect(TSocket *Connection, const char *Address, int Port)
{
	return Connection->Connect(Address, Port) != FALSE;
}

int TMfcSocket::Send(TSocket *Connection, const void *Data, int Length)
{
	return Connection->Send(Data, Length, 0);
}

int TMfcSocket::Receive(TSocket *Connection, void *Buffer, int Length)
{
	return Connection->Receive(Buffer, Length, 0);
}

void TMfcSocket::Shutdown(TSocket *Connection, TShutdown How)
{
	shutdown(Connection->m_hSocket, How == TShutdown::Send ? SD_SEND : SD_BOTH);
}

void TMfcSocket::Close(TSocket *Connection)
{
	Connection->Close();
}

void *TMfcSocket::ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer, int Length)
{
	return WSAAsyncGetHostByName(static_cast<HWND>(Window), Message, Hostname, Buffer, Length);
}

void TMfcSocket::CancelResolve(void *Request)
{
	WSACancelAsyncRequest(Request);
}

const char *TMfcSocket::ResolvedAddress(const char *Buffer)
{
	return inet_ntoa(*reinterpret_cast<const in_addr *>(reinterpret_cast<const HOSTENT *>(Buffer)->h_addr_list[0]));
}

namespace {
constinit TMfcSocket MfcSocketInstance;
} // namespace

// The runner's network (Win32Platform.h): constant-initialised, as the platform's other objects.
ISocket &NetworkSocket()
{
	return MfcSocketInstance;
}

} // namespace patella::platform
