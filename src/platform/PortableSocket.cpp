// PortableSocket.cpp: TPortableSocket, the platform's network over Winsock or POSIX sockets (PortableSocket.h;
// Patella-Modern issue #125, notes/decisions.md "#125"). Not part of the original source.

#include "platform/PortableSocket.h"

#include "compat/span.h"
#include "platform/Socket.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <type_traits>
#endif

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif

namespace patella::platform {

#if defined(_WIN32)
// ISocket's numbers are WinSock's (platform/Socket.h, issue #110): the Windows build passes WinSock's on unchanged.
static_assert(socket_error::Interrupted == WSAEINTR && socket_error::AccessDenied == WSAEACCES
	&& socket_error::Fault == WSAEFAULT && socket_error::Invalid == WSAEINVAL && socket_error::TooManySockets == WSAEMFILE
	&& socket_error::WouldBlock == WSAEWOULDBLOCK && socket_error::Already == WSAEALREADY
	&& socket_error::NotSocket == WSAENOTSOCK && socket_error::MessageSize == WSAEMSGSIZE
	&& socket_error::FamilyNotSupported == WSAEAFNOSUPPORT && socket_error::AddressInUse == WSAEADDRINUSE
	&& socket_error::AddressNotAvailable == WSAEADDRNOTAVAIL && socket_error::NetworkDown == WSAENETDOWN
	&& socket_error::NetworkUnreachable == WSAENETUNREACH && socket_error::NetworkReset == WSAENETRESET
	&& socket_error::ConnectionAborted == WSAECONNABORTED && socket_error::ConnectionReset == WSAECONNRESET
	&& socket_error::NoBuffers == WSAENOBUFS && socket_error::AlreadyConnected == WSAEISCONN
	&& socket_error::NotConnected == WSAENOTCONN && socket_error::Shutdown == WSAESHUTDOWN
	&& socket_error::TimedOut == WSAETIMEDOUT && socket_error::ConnectionRefused == WSAECONNREFUSED
	&& socket_error::HostUnreachable == WSAEHOSTUNREACH && socket_error::HostNotFound == WSAHOST_NOT_FOUND
	&& socket_error::TryAgain == WSATRY_AGAIN && socket_error::NoRecovery == WSANO_RECOVERY
	&& socket_error::NoData == WSANO_DATA);
static_assert(SocketFailed == SOCKET_ERROR && InvalidNativeSocket == INVALID_SOCKET);
static_assert(std::is_same_v<TNativeSocket, SOCKET>);
#endif

namespace {

#if defined(_WIN32)


SOCKET Native(TNativeSocket Handle)
{
	return static_cast<SOCKET>(Handle);
}

int SystemError()
{
	return WSAGetLastError();
}

int LookupError(int Code)
{
	return Code; // getaddrinfo's EAI_* are WinSock's numbers (WSAHOST_NOT_FOUND ...)
}

void CloseNative(TNativeSocket Handle)
{
	closesocket(Native(Handle));
}
#else
using TLength = std::size_t;

int Native(TNativeSocket Handle)
{
	return Handle;
}

// errno as WinSock's number (WinSock's own names for the BSD errors: WSAE<name>). An error WinSock has no number for
// is WSAEINVAL.
int WinsockError(int Code)
{
	if (Code == EWOULDBLOCK || Code == EAGAIN || Code == EINPROGRESS)
		return socket_error::WouldBlock;
	switch (Code) {
	case EINTR: return socket_error::Interrupted;
	case EACCES: return socket_error::AccessDenied;
	case EFAULT: return socket_error::Fault;
	case EMFILE:
	case ENFILE: return socket_error::TooManySockets;
	case EALREADY: return socket_error::Already;
	case ENOTSOCK:
	case EBADF: return socket_error::NotSocket;
	case EMSGSIZE: return socket_error::MessageSize;
	case EAFNOSUPPORT: return socket_error::FamilyNotSupported;
	case EADDRINUSE: return socket_error::AddressInUse;
	case EADDRNOTAVAIL: return socket_error::AddressNotAvailable;
	case ENETDOWN: return socket_error::NetworkDown;
	case ENETUNREACH: return socket_error::NetworkUnreachable;
	case ENETRESET: return socket_error::NetworkReset;
	case ECONNABORTED: return socket_error::ConnectionAborted;
	case ECONNRESET: return socket_error::ConnectionReset;
	case ENOBUFS:
	case ENOMEM: return socket_error::NoBuffers;
	case EISCONN: return socket_error::AlreadyConnected;
	case ENOTCONN: return socket_error::NotConnected;
	case EPIPE:
	case ESHUTDOWN: // NOLINT(misc-include-cleaner): <cerrno>'s (glibc's asm-generic/errno.h)
		return socket_error::Shutdown;
	case ETIMEDOUT: return socket_error::TimedOut;
	case ECONNREFUSED: return socket_error::ConnectionRefused;
	case EHOSTDOWN: // NOLINT(misc-include-cleaner): <cerrno>'s (glibc's asm-generic/errno.h)
	case EHOSTUNREACH: return socket_error::HostUnreachable;
	default: return socket_error::Invalid;
	}
}

int SystemError()
{
	return WinsockError(errno);
}

// getaddrinfo's EAI_* as WinSock's numbers (Windows' getaddrinfo returns those).
int LookupError(int Code)
{
	switch (Code) {
	case EAI_NONAME: return socket_error::HostNotFound;
	case EAI_AGAIN: return socket_error::TryAgain;
	case EAI_FAIL: return socket_error::NoRecovery;
	case EAI_MEMORY: return socket_error::NoBuffers;
	case EAI_FAMILY: return socket_error::FamilyNotSupported;
	case EAI_SYSTEM: return WinsockError(errno);
#if defined(EAI_NODATA)
	case EAI_NODATA:
#endif
#if defined(EAI_ADDRFAMILY)
	case EAI_ADDRFAMILY:
#endif
#if defined(EAI_NODATA) || defined(EAI_ADDRFAMILY)
		return socket_error::NoData;
#endif
	default: return socket_error::NoRecovery;
	}
}

void CloseNative(TNativeSocket Handle)
{
	close(Handle);
}
#endif

// A connect's state: -1 while it connects, else 0 or the error it ended with.
int ConnectState(TNativeSocket Handle)
{
#if defined(_WIN32)
	fd_set Write{};
	Write.fd_count = 1;
	Write.fd_array[0] = Native(Handle);
	fd_set Except = Write;
	timeval Now{};
	if (select(0, nullptr, &Write, &Except, &Now) <= 0)
		return -1;
	const bool Failed = Except.fd_count != 0;
#else
	pollfd Entry{};
	Entry.fd = Native(Handle);
	Entry.events = POLLOUT;
	if (poll(&Entry, 1, 0) <= 0 || (Entry.revents & (POLLOUT | POLLERR | POLLHUP)) == 0)
		return -1;
	const bool Failed = (Entry.revents & (POLLERR | POLLHUP)) != 0;
#endif
	int Code = 0;
	socklen_t Size = sizeof(Code);
	// NOLINTNEXTLINE(misc-include-cleaner): SOL_SOCKET and SO_ERROR are <sys/socket.h>'s (glibc's asm-generic/socket.h)
	if (getsockopt(Native(Handle), SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&Code), &Size) != 0)
		return SystemError();
#if !defined(_WIN32)
	if (Code != 0)
		Code = WinsockError(Code);
#endif
	if (Code == 0 && Failed)
		Code = socket_error::ConnectionRefused; // an error state without its code
	return Code;
}

// What a connection has to read: bytes, its end (the peer's shutdown), an error (a reset), or nothing yet.
enum class TReadable : unsigned char { Nothing,
	Bytes,
	End,
	Failed };

TReadable Readable(TNativeSocket Handle, int &Code)
{
	char Byte = 0;
	const auto Count = recv(Native(Handle), &Byte, 1, MSG_PEEK);
	if (Count > 0)
		return TReadable::Bytes;
	if (Count == 0)
		return TReadable::End;
	Code = SystemError();
	if (Code == socket_error::WouldBlock || Code == socket_error::Interrupted)
		return TReadable::Nothing;
	return TReadable::Failed;
}

// getaddrinfo's addresses for Name: the first IPv4 one, else the first IPv6 one (the original's gethostbyname had
// IPv4 only, so a host with both is reached as before). 0 or the WinSock number.
int FindAddress(const char *Name, bool AllowIPv6, sockaddr_storage &Address, socklen_t &Size)
{
	addrinfo Hints{};
	Hints.ai_family = AF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	addrinfo *List = nullptr;
	const int Code = getaddrinfo(Name, nullptr, &Hints, &List);
	if (Code != 0)
		return LookupError(Code);
	const addrinfo *Chosen = nullptr;
	for (const addrinfo *Entry = List; Entry != nullptr && Chosen == nullptr; Entry = Entry->ai_next)
		if (Entry->ai_family == AF_INET && Entry->ai_addrlen <= sizeof(Address))
			Chosen = Entry;
	for (const addrinfo *Entry = List; AllowIPv6 && Entry != nullptr && Chosen == nullptr; Entry = Entry->ai_next)
		if (Entry->ai_family == AF_INET6 && Entry->ai_addrlen <= sizeof(Address))
			Chosen = Entry;
	int Result = socket_error::NoData;
	if (Chosen != nullptr) {
		Address = {};
		std::memcpy(&Address, Chosen->ai_addr, Chosen->ai_addrlen);
		Size = static_cast<socklen_t>(Chosen->ai_addrlen);
		Result = 0;
	}
	freeaddrinfo(List);
	return Result;
}

// The connect's target for a socket of Family: an IPv4 address mapped into IPv6 on an IPv6 socket.
bool Target(int Family, const char *Address, int Port, sockaddr_storage &Out, socklen_t &Size)
{
	sockaddr_storage Found{};
	socklen_t FoundSize = 0;
	if (FindAddress(Address, Family == AF_INET6, Found, FoundSize) != 0)
		return false;
	const auto NetworkPort = htons(static_cast<std::uint16_t>(Port)); // MFC's htons((u_short)nHostPort)
	Out = {};
	if (Found.ss_family == AF_INET6) {
		auto *Six = reinterpret_cast<sockaddr_in6 *>(&Found);
		Six->sin6_port = NetworkPort;
		std::memcpy(&Out, Six, sizeof(sockaddr_in6));
		Size = sizeof(sockaddr_in6);
		return true;
	}
	const auto *Four = reinterpret_cast<const sockaddr_in *>(&Found);
	if (Family == AF_INET) {
		sockaddr_in Plain{};
		Plain.sin_family = AF_INET;
		Plain.sin_port = NetworkPort;
		Plain.sin_addr = Four->sin_addr;
		std::memcpy(&Out, &Plain, sizeof(Plain));
		Size = sizeof(Plain);
		return true;
	}
	sockaddr_in6 Mapped{};
	Mapped.sin6_family = AF_INET6;
	Mapped.sin6_port = NetworkPort;
	std::array<unsigned char, 16> Bytes{};
	Bytes[10] = 0xff;
	Bytes[11] = 0xff;
	std::memcpy(&Bytes[12], &Four->sin_addr, 4);
	std::memcpy(&Mapped.sin6_addr, Bytes.data(), Bytes.size());
	std::memcpy(&Out, &Mapped, sizeof(Mapped));
	Size = sizeof(Mapped);
	return true;
}

// A non-blocking stream socket of Family, not inherited by child processes; InvalidNativeSocket with Code.
TNativeSocket OpenNative(int Family, int &Code)
{
#if defined(_WIN32)
	const SOCKET Handle = WSASocketW(Family, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
	if (Handle == INVALID_SOCKET) {
		Code = SystemError();
		return InvalidNativeSocket;
	}
	u_long NonBlocking = 1;
	if (ioctlsocket(Handle, FIONBIO, &NonBlocking) != 0) {
		Code = SystemError();
		closesocket(Handle);
		return InvalidNativeSocket;
	}
#else
	const int Handle = socket(Family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
	if (Handle < 0) {
		Code = SystemError();
		return InvalidNativeSocket;
	}
#endif
	if (Family == AF_INET6) {
		int Only = 0;
		if (setsockopt(Native(Handle), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char *>(&Only), sizeof(Only))
			!= 0) {
			Code = SystemError();
			CloseNative(Handle);
			return InvalidNativeSocket;
		}
	}
	return Handle;
}

} // namespace

// A lookup: its thread writes Address and Error, then Done; the client's thread reads them after Done.
struct TPortableSocket::TLookup {
	void *Request = nullptr;
	void *Window = nullptr;
	unsigned int Message = 0;
	char *Buffer = nullptr;
	int Length = 0;
	bool Cancelled = false;
	std::string Address;
	int Error = 0;
	std::atomic<bool> Done{false};
	std::jthread Thread; // last: destroyed (joined) first
};

TPortableSocket::TPortableSocket(TResolved ResolvedHook, TBusy OnBusy) : Resolved(ResolvedHook), BusyHook(OnBusy) {}

TPortableSocket::~TPortableSocket()
{
	for (auto &[Id, Connection] : Connections) {
		CloseNative(Connection->Handle);
		Connection->Handle = InvalidNativeSocket;
		Connection->Id = 0;
	}
	Connections.clear();
	Lookups.clear(); // waits for their threads
#if defined(_WIN32)
	if (Started)
		WSACleanup();
#endif
}

int TPortableSocket::Fail(int Code)
{
	Error = Code;
	return SocketFailed;
}

bool TPortableSocket::Startup()
{
#if defined(_WIN32)
	if (!Started) {
		WSADATA Data{};
		if (WSAStartup(MAKEWORD(2, 2), &Data) != 0)
			return false;
		Started = true;
	}
#else
	Started = true;
#endif
	return true;
}

int TPortableSocket::LastError()
{
	return Error;
}

TSocket *TPortableSocket::New(ISocketEvents &Events)
{
	return new TSocket(Events); // NOLINT(cppcoreguidelines-owning-memory): TNetwork's Socket, released by Delete
}

void TPortableSocket::Delete(TSocket *Connection)
{
	if (Connection == nullptr)
		return;
	Close(Connection); // CAsyncSocket's destructor closes an open socket
	delete Connection; // NOLINT(cppcoreguidelines-owning-memory): New's
}

bool TPortableSocket::Create(TSocket *Connection)
{
	if (Connection == nullptr) {
		Fail(socket_error::NotSocket);
		return false;
	}
	if (Connection->Handle != InvalidNativeSocket) {
		Fail(socket_error::Invalid);
		return false;
	}
	int Code = 0;
	int Family = AF_INET6;
	TNativeSocket Handle = OpenNative(Family, Code);
	if (Handle == InvalidNativeSocket) { // a system without IPv6
		Family = AF_INET;
		Handle = OpenNative(Family, Code);
	}
	if (Handle == InvalidNativeSocket) {
		Fail(Code);
		return false;
	}
	Connection->Handle = Handle;
	Connection->Family = Family;
	Connection->Id = NextId++;
	Connection->Connecting = false;
	Connection->Connected = false;
	Connection->Ended = false;
	Connections.emplace(Connection->Id, Connection);
	UpdateBusy();
	return true;
}

// Connection may be null: TNetwork's ConnectSocket without a socket calls it so (MFC's CAsyncSocket::Connect read the
// null object's handle; here WSAENOTSOCK).
bool TPortableSocket::Connect(TSocket *Connection, const char *Address, int Port)
{
	if (Connection == nullptr || Connection->Handle == InvalidNativeSocket) {
		Fail(socket_error::NotSocket);
		return false;
	}
	if (Connection->Connecting || Connection->Connected) {
		Fail(Connection->Connected ? socket_error::AlreadyConnected : socket_error::Already);
		return false;
	}
	sockaddr_storage To{};
	socklen_t Size = 0;
	// MFC's: inet_addr, else gethostbyname (blocking), else WSAEINVAL.
	if (Address == nullptr || !Target(Connection->Family, Address, Port, To, Size)) {
		Fail(socket_error::Invalid);
		return false;
	}
	if (connect(Native(Connection->Handle), reinterpret_cast<const sockaddr *>(&To), Size) == 0) {
		Connection->Connecting = true; // OnConnect(0) at the next Poll, as WinSock's FD_CONNECT
		return true;
	}
	const int Code = SystemError();
	if (Code == socket_error::WouldBlock || Code == socket_error::Interrupted) {
		Connection->Connecting = true;
		Fail(socket_error::WouldBlock);
		return false;
	}
	Fail(Code);
	return false;
}

int TPortableSocket::Send(TSocket *Connection, const void *Data, int Length)
{
	if (Connection == nullptr || Connection->Handle == InvalidNativeSocket)
		return Fail(socket_error::NotSocket);
	if (Length < 0 || (Data == nullptr && Length > 0))
		return Fail(socket_error::Fault);
#if defined(_WIN32)
	const auto Count = send(Native(Connection->Handle), static_cast<const char *>(Data), Length, 0);
#else
	const auto Count = send(Native(Connection->Handle), Data, static_cast<TLength>(Length), MSG_NOSIGNAL);
#endif
	if (Count < 0)
		return Fail(SystemError());
	return static_cast<int>(Count);
}

int TPortableSocket::Receive(TSocket *Connection, void *Buffer, int Length)
{
	if (Connection == nullptr || Connection->Handle == InvalidNativeSocket)
		return Fail(socket_error::NotSocket);
	if (Length < 0 || (Buffer == nullptr && Length > 0))
		return Fail(socket_error::Fault);
#if defined(_WIN32)
	const auto Count = recv(Native(Connection->Handle), static_cast<char *>(Buffer), Length, 0);
#else
	const auto Count = recv(Native(Connection->Handle), Buffer, static_cast<TLength>(Length), 0);
#endif
	if (Count < 0)
		return Fail(SystemError());
	Connection->Taken += static_cast<std::uint64_t>(Count);
	return static_cast<int>(Count);
}

void TPortableSocket::Shutdown(TSocket *Connection, TShutdown How)
{
	if (Connection == nullptr || Connection->Handle == InvalidNativeSocket) {
		Fail(socket_error::NotSocket);
		return;
	}
#if defined(_WIN32)
	const int Which = How == TShutdown::Send ? SD_SEND : SD_BOTH;
#else
	const int Which = How == TShutdown::Send ? SHUT_WR : SHUT_RDWR;
#endif
	if (shutdown(Native(Connection->Handle), Which) != 0)
		Fail(SystemError());
}

void TPortableSocket::Close(TSocket *Connection)
{
	if (Connection == nullptr || Connection->Handle == InvalidNativeSocket)
		return;
	CloseNative(Connection->Handle);
	Connections.erase(Connection->Id);
	Connection->Handle = InvalidNativeSocket;
	Connection->Id = 0;
	Connection->Connecting = false;
	Connection->Connected = false;
	Connection->Ended = false;
	UpdateBusy();
}

void *TPortableSocket::ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer, int Length)
{
	if (Hostname == nullptr || Buffer == nullptr || Length <= 0) {
		Fail(socket_error::Fault);
		return nullptr;
	}
	Buffer[0] = '\0';
	auto Lookup = std::make_unique<TLookup>();
	// The request's handle: a number, never null, never a pointer.
	Lookup->Request = reinterpret_cast<void *>(NextRequest++); // NOLINT(performance-no-int-to-ptr): a handle, as WSAAsyncGetHostByName's
	Lookup->Window = Window;
	Lookup->Message = Message;
	Lookup->Buffer = Buffer;
	Lookup->Length = Length;
	TLookup *Running = Lookup.get();
	try {
		Running->Thread = std::jthread([Running, Name = std::string(Hostname)] {
			sockaddr_storage Address{};
			socklen_t Size = 0;
			int Code = FindAddress(Name.c_str(), true, Address, Size);
			if (Code == 0) {
				std::array<char, INET6_ADDRSTRLEN> Text{};
				const void *Bytes = Address.ss_family == AF_INET6
					? static_cast<const void *>(&reinterpret_cast<const sockaddr_in6 *>(&Address)->sin6_addr)
					: static_cast<const void *>(&reinterpret_cast<const sockaddr_in *>(&Address)->sin_addr);
				if (inet_ntop(Address.ss_family, Bytes, Text.data(), Text.size()) != nullptr)
					Running->Address = Text.data();
				else
					Code = socket_error::NoRecovery;
			}
			Running->Error = Code;
			Running->Done.store(true, std::memory_order_release);
		});
	} catch (const std::system_error &) {
		Fail(socket_error::NoBuffers);
		return nullptr;
	}
	void *Request = Running->Request;
	Lookups.push_back(std::move(Lookup));
	UpdateBusy();
	return Request;
}

void TPortableSocket::CancelResolve(void *Request)
{
	if (Request == nullptr)
		return;
	for (const std::unique_ptr<TLookup> &Lookup : Lookups)
		if (Lookup->Request == Request)
			Lookup->Cancelled = true;
	UpdateBusy();
}

const char *TPortableSocket::ResolvedAddress(const char *Buffer)
{
	return Buffer != nullptr ? Buffer : "";
}

void TPortableSocket::Poll()
{
	// The connections in the order they were made; an event's handler may close or make any of them. The numbers are
	// this call's own, because a handler's modal loop can reach Poll again. The client's few connections fit on the
	// stack, so a turn of the loop does not allocate (issue #155); more than that go in a vector.
	std::array<std::uint64_t, 8> Inline{};
	std::vector<std::uint64_t> Many;
	if (Connections.size() > Inline.size())
		Many.resize(Connections.size());
	const patella::TSpan<std::uint64_t> Ids(Many.empty() ? Inline.data() : Many.data(), Connections.size());
	std::size_t Count = 0;
	for (const auto &Entry : Connections)
		Ids[Count++] = Entry.first;
	std::ranges::sort(Ids.begin(), Ids.end());
	for (const std::uint64_t Id : Ids)
		PollConnection(Id);
	PollLookups();
	UpdateBusy();
}

bool TPortableSocket::Alive(std::uint64_t Id, const TSocket *Connection) const
{
	const auto Found = Connections.find(Id);
	return Found != Connections.end() && Found->second == Connection;
}

void TPortableSocket::PollConnection(std::uint64_t Id)
{
	const auto Found = Connections.find(Id);
	if (Found == Connections.end())
		return;
	TSocket *Connection = Found->second;
	if (Connection->Connecting) {
		const int State = ConnectState(Connection->Handle);
		if (State < 0)
			return;
		Connection->Connecting = false;
		Connection->Connected = State == 0;
		Connection->OnConnect(State);
		if (!Alive(Id, Connection) || !Connection->Connected)
			return;
	}
	if (!Connection->Connected || Connection->Ended)
		return;
	for (int Round = 0; Round < ReadRounds; Round++) {
		int Code = 0;
		switch (Readable(Connection->Handle, Code)) {
		case TReadable::Nothing: return;
		case TReadable::Bytes: {
			const std::uint64_t Before = Connection->Taken;
			Connection->OnReceive(0);
			// Closed by the handler, or nothing taken (its buffer is full): the rest at the next Poll.
			if (!Alive(Id, Connection) || Connection->Taken == Before)
				return;
			break;
		}
		case TReadable::End:
		case TReadable::Failed:
			Connection->Ended = true;
			Connection->OnClose(Code);
			return;
		}
	}
}

void TPortableSocket::PollLookups()
{
	std::vector<std::unique_ptr<TLookup>> Finished;
	for (auto Entry = Lookups.begin(); Entry != Lookups.end();) {
		if ((*Entry)->Done.load(std::memory_order_acquire)) {
			Finished.push_back(std::move(*Entry));
			Entry = Lookups.erase(Entry);
		} else {
			++Entry;
		}
	}
	for (const std::unique_ptr<TLookup> &Lookup : Finished) {
		if (Lookup->Cancelled)
			continue;
		int Code = Lookup->Error;
		if (Code == 0) {
			if (Lookup->Address.size() + 1 > static_cast<std::size_t>(Lookup->Length)) {
				Code = socket_error::NoBuffers; // WinSock's for a buffer too small for the host's entry
			} else {
				std::memcpy(Lookup->Buffer, Lookup->Address.c_str(), Lookup->Address.size() + 1);
			}
		}
		if (Resolved != nullptr)
			Resolved(Lookup->Window, Lookup->Message, Lookup->Request, Code);
	}
}

bool TPortableSocket::Busy() const
{
	return !Connections.empty()
		|| std::ranges::any_of(Lookups, [](const std::unique_ptr<TLookup> &Lookup) { return !Lookup->Cancelled; });
}

void TPortableSocket::UpdateBusy()
{
	const bool Now = Busy();
	if (Now == WasBusy)
		return;
	WasBusy = Now;
	if (BusyHook != nullptr)
		BusyHook(Now);
}

} // namespace patella::platform
