// PipeSocketWin32.cpp: the test build's ISocket for the server replay (PipeSocketWin32.h; Patella-Modern issue #120).

#include "testmode/PipeSocketWin32.h"

#include "platform/Platform.h"
#include "platform/Win32Platform.h"
#include "testmode/PipeNetwork.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace patella::testmode {

namespace {

// The events' window message: WParam the connection's id, LParam the event and (bit 8) Failed.
constexpr UINT PipeMessage = WM_USER + 0x120;
constexpr wchar_t WindowClass[] = L"PatellaReplaySockets";

int ErrorCode(TPipeError Error)
{
	switch (Error) {
	case TPipeError::None:
		return 0;
	case TPipeError::WouldBlock:
		return WSAEWOULDBLOCK;
	case TPipeError::NotConnected:
		return WSAENOTCONN;
	case TPipeError::Shutdown:
		return WSAESHUTDOWN;
	case TPipeError::Reset:
		return WSAECONNRESET;
	}
	return WSAEINVAL;
}

} // namespace

TPipeSocketWin32::TPipeSocketWin32() : Pipes([this](int Id, TPipeEvent Event, bool Failed) { Post(Id, Event, Failed); })
{
}

bool TPipeSocketWin32::Start()
{
	if (Window != nullptr)
		return true;
	HINSTANCE Instance = ::GetModuleHandleW(nullptr);
	WNDCLASSEXW Class = {};
	Class.cbSize = sizeof(Class);
	Class.lpfnWndProc = WindowProcedure;
	Class.hInstance = Instance;
	Class.lpszClassName = WindowClass;
	if (::RegisterClassExW(&Class) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		return false;
	Window = ::CreateWindowExW(0, WindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, Instance, nullptr);
	if (Window == nullptr)
		return false;
	::SetWindowLongPtrW(Window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	return true;
}

LRESULT CALLBACK TPipeSocketWin32::WindowProcedure(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	if (Message != PipeMessage)
		return ::DefWindowProcW(Window, Message, WParam, LParam);
	auto *Self = reinterpret_cast<TPipeSocketWin32 *>(::GetWindowLongPtrW(Window, GWLP_USERDATA));
	if (Self != nullptr) {
		const auto Bits = static_cast<std::uintptr_t>(LParam);
		Self->Deliver(static_cast<int>(WParam), static_cast<TPipeEvent>(Bits & 0xffU), (Bits & 0x100U) != 0);
	}
	return 0;
}

void TPipeSocketWin32::Post(int Id, TPipeEvent Event, bool Failed)
{
	const auto Bits = static_cast<std::uintptr_t>(Event) | (Failed ? 0x100U : 0U);
	::PostMessageW(Window, PipeMessage, static_cast<WPARAM>(Id), static_cast<LPARAM>(Bits));
}

void TPipeSocketWin32::Deliver(int Id, TPipeEvent Event, bool Failed)
{
	const auto Found = Sockets.find(Id);
	if (Found == Sockets.end())
		return; // closed since (MFC's socket map has the handle no more)
	platform::TSocket *Connection = Found->second;
	switch (Event) {
	case TPipeEvent::Connect:
		Connection->OnConnect(Failed ? WSAECONNREFUSED : 0);
		break;
	case TPipeEvent::Write:
		Connection->OnSend(0);
		break;
	case TPipeEvent::Read:
		// MFC's CAsyncSocket::DoCallBack: OnReceive only when select finds the socket readable.
		if (Pipes.Readable(Id))
			Connection->OnReceive(0);
		break;
	case TPipeEvent::Close:
		Connection->OnClose(Failed ? WSAECONNRESET : 0);
		break;
	}
}

int TPipeSocketWin32::IdOf(platform::TSocket *Connection)
{
	const auto Found = Ids.find(Connection);
	if (Found == Ids.end()) {
		Error = WSAENOTSOCK;
		return -1;
	}
	return Found->second;
}

int TPipeSocketWin32::Fail(TPipeError Code)
{
	Error = ErrorCode(Code);
	return SOCKET_ERROR;
}

bool TPipeSocketWin32::Startup()
{
	return platform::Win32Socket().Startup();
}

int TPipeSocketWin32::LastError()
{
	return Error;
}

platform::TSocket *TPipeSocketWin32::New(platform::ISocketEvents &Events)
{
	return new platform::TSocket(Events); // NOLINT(cppcoreguidelines-owning-memory): TNetwork's Socket, released by Delete
}

void TPipeSocketWin32::Delete(platform::TSocket *Connection)
{
	if (Connection == nullptr)
		return;
	Close(Connection); // CAsyncSocket's destructor closes an open socket
	delete Connection; // NOLINT(cppcoreguidelines-owning-memory): New's
}

bool TPipeSocketWin32::Create(platform::TSocket *Connection)
{
	if (Connection == nullptr || Ids.contains(Connection)) {
		Error = WSAEINVAL;
		return false;
	}
	const int Id = Pipes.Open();
	Ids.emplace(Connection, Id);
	Sockets.emplace(Id, Connection);
	return true;
}

bool TPipeSocketWin32::Connect(platform::TSocket *Connection, const char *Address, int Port)
{
	const int Id = IdOf(Connection);
	if (Id < 0)
		return false;
	if (!Pipes.Connect(Id, Address != nullptr ? std::string_view(Address) : std::string_view(), Port)) {
		Error = WSAEISCONN;
		return false;
	}
	Error = WSAEWOULDBLOCK; // a non-blocking connect: OnConnect follows
	return false;
}

int TPipeSocketWin32::Send(platform::TSocket *Connection, const void *Data, int Length)
{
	const int Id = IdOf(Connection);
	if (Id < 0)
		return SOCKET_ERROR;
	if (Length < 0) {
		Error = WSAEINVAL;
		return SOCKET_ERROR;
	}
	const TPipeResult Result = Pipes.Send(Id, std::span(static_cast<const std::uint8_t *>(Data), static_cast<std::size_t>(Length)));
	return Result.Count < 0 ? Fail(Result.Error) : Result.Count;
}

int TPipeSocketWin32::Receive(platform::TSocket *Connection, void *Buffer, int Length)
{
	const int Id = IdOf(Connection);
	if (Id < 0)
		return SOCKET_ERROR;
	if (Length < 0) {
		Error = WSAEINVAL;
		return SOCKET_ERROR;
	}
	const TPipeResult Result = Pipes.Receive(Id, std::span(static_cast<std::uint8_t *>(Buffer), static_cast<std::size_t>(Length)));
	return Result.Count < 0 ? Fail(Result.Error) : Result.Count;
}

void TPipeSocketWin32::Shutdown(platform::TSocket *Connection, platform::TShutdown How)
{
	const int Id = IdOf(Connection);
	if (Id >= 0)
		Pipes.Shutdown(Id, How == platform::TShutdown::Both);
}

void TPipeSocketWin32::Close(platform::TSocket *Connection)
{
	const auto Found = Ids.find(Connection);
	if (Found == Ids.end())
		return;
	Pipes.Release(Found->second);
	Sockets.erase(Found->second);
	Ids.erase(Found);
}

void *TPipeSocketWin32::ResolveHostname(void *Owner, unsigned int Message, const char *Hostname, char *Buffer, int Length)
{
	void *Request = platform::Win32Socket().ResolveHostname(Owner, Message, Hostname, Buffer, Length);
	if (Request == nullptr)
		Error = platform::Win32Socket().LastError();
	return Request;
}

void TPipeSocketWin32::CancelResolve(void *Request)
{
	platform::Win32Socket().CancelResolve(Request);
}

const char *TPipeSocketWin32::ResolvedAddress(const char *Buffer)
{
	return platform::Win32Socket().ResolvedAddress(Buffer);
}

} // namespace patella::testmode
