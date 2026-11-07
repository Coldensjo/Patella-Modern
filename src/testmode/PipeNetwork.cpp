// PipeNetwork.cpp: the server replay's network inside the process (PipeNetwork.h; Patella-Modern issue #120).

#include "PipeNetwork.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace patella::testmode {

namespace {

// Moves up to Buffer's size from the front of Queue; the count.
int Take(std::deque<std::uint8_t> &Queue, std::span<std::uint8_t> Buffer)
{
	const std::size_t Count = (std::min)(Queue.size(), Buffer.size());
	std::copy_n(Queue.begin(), Count, Buffer.begin());
	Queue.erase(Queue.begin(), Queue.begin() + static_cast<std::ptrdiff_t>(Count));
	return static_cast<int>(Count);
}

TPipeResult Failure(TPipeError Error)
{
	return TPipeResult{-1, Error};
}

} // namespace

void TPipeNetwork::Listen(std::string Address, int Port, std::string Kind)
{
	Listeners.push_back(TListener{std::move(Address), Port, std::move(Kind)});
}

TPipeNetwork::TConnection *TPipeNetwork::Find(int Id)
{
	if (Id < 0 || static_cast<std::size_t>(Id) >= Connections.size())
		return nullptr;
	return &Connections[static_cast<std::size_t>(Id)];
}

const TPipeNetwork::TConnection *TPipeNetwork::Find(int Id) const
{
	if (Id < 0 || static_cast<std::size_t>(Id) >= Connections.size())
		return nullptr;
	return &Connections[static_cast<std::size_t>(Id)];
}

void TPipeNetwork::Post(int Id, TPipeEvent Event, bool Failed) const
{
	if (Notify)
		Notify(Id, Event, Failed);
}

void TPipeNetwork::PostRead(int Id, TConnection &Connection) const
{
	if (Connection.ReadPosted || Connection.ClientReleased)
		return;
	Connection.ReadPosted = true;
	Post(Id, TPipeEvent::Read, false);
}

void TPipeNetwork::PostClose(int Id, TConnection &Connection, bool Reset) const
{
	if (Connection.ClosePosted || Connection.ClientReleased || !Connection.Connected)
		return;
	Connection.ClosePosted = true;
	Post(Id, TPipeEvent::Close, Reset);
}

int TPipeNetwork::Open()
{
	Connections.emplace_back();
	return static_cast<int>(Connections.size() - 1);
}

bool TPipeNetwork::Connect(int Id, std::string_view Address, int Port)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || Connection->ClientReleased || Connection->Connecting)
		return false;
	Connection->Connecting = true;
	const auto Listener = std::find_if(Listeners.begin(), Listeners.end(),
		[&](const TListener &Each) { return Each.Address == Address && Each.Port == Port; });
	if (Listener == Listeners.end()) {
		Connection->Refused = true;
		Post(Id, TPipeEvent::Connect, true);
		return true;
	}
	Connection->Connected = true;
	Connection->Kind = Listener->Kind;
	Pending.push_back(Id);
	Post(Id, TPipeEvent::Connect, false);
	Post(Id, TPipeEvent::Write, false);
	return true;
}

TPipeResult TPipeNetwork::Send(int Id, std::span<const std::uint8_t> Data)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || Connection->ClientReleased || !Connection->Connected)
		return Failure(TPipeError::NotConnected);
	if (Connection->ClientSendShut)
		return Failure(TPipeError::Shutdown);
	if (!Connection->ServerClosed)
		Connection->ToServer.insert(Connection->ToServer.end(), Data.begin(), Data.end());
	return TPipeResult{static_cast<int>(Data.size()), TPipeError::None};
}

TPipeResult TPipeNetwork::Receive(int Id, std::span<std::uint8_t> Buffer)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || Connection->ClientReleased || !Connection->Connected)
		return Failure(TPipeError::NotConnected);
	if (Connection->ClientReceiveShut)
		return Failure(TPipeError::Shutdown);
	// A receive re-enables Read (WinSock's FD_READ), whatever it finds.
	Connection->ReadPosted = false;
	if (!Connection->ToClient.empty()) {
		const int Count = Take(Connection->ToClient, Buffer);
		if (!Connection->ToClient.empty())
			PostRead(Id, *Connection);
		return TPipeResult{Count, TPipeError::None};
	}
	if (Connection->ServerReset)
		return Failure(TPipeError::Reset);
	if (Connection->ServerSendShut || Connection->ServerClosed)
		return TPipeResult{0, TPipeError::None};
	return Failure(TPipeError::WouldBlock);
}

void TPipeNetwork::Shutdown(int Id, bool Both)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || Connection->ClientReleased || !Connection->Connected)
		return;
	Connection->ClientSendShut = true;
	if (Both)
		Connection->ClientReceiveShut = true;
}

void TPipeNetwork::Release(int Id)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || Connection->ClientReleased)
		return;
	Connection->ClientReleased = true;
	if (Connection->Connected && !Connection->ToClient.empty())
		Connection->ClientReset = true;
	Connection->ToClient.clear();
}

bool TPipeNetwork::Readable(int Id) const
{
	const TConnection *Connection = Find(Id);
	if (Connection == nullptr || Connection->ClientReleased || !Connection->Connected)
		return false;
	return !Connection->ToClient.empty() || Connection->ServerSendShut || Connection->ServerClosed
		|| Connection->ClientReceiveShut;
}

bool TPipeNetwork::Live(int Id) const
{
	const TConnection *Connection = Find(Id);
	return Connection != nullptr && !Connection->ClientReleased;
}

std::optional<TPipeNetwork::TAccepted> TPipeNetwork::Accept()
{
	if (Pending.empty())
		return std::nullopt;
	const int Id = Pending.front();
	Pending.pop_front();
	TConnection &Connection = Connections[static_cast<std::size_t>(Id)];
	Connection.Accepted = true;
	return TAccepted{Id, Connection.Kind};
}

TPipeResult TPipeNetwork::ServerReceive(int Id, std::span<std::uint8_t> Buffer)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || !Connection->Accepted || Connection->ServerClosed)
		return Failure(TPipeError::NotConnected);
	if (!Connection->ToServer.empty())
		return TPipeResult{Take(Connection->ToServer, Buffer), TPipeError::None};
	if (Connection->ClientReset)
		return Failure(TPipeError::Reset);
	if (Connection->ClientSendShut || Connection->ClientReleased)
		return TPipeResult{0, TPipeError::None};
	return Failure(TPipeError::WouldBlock);
}

TPipeResult TPipeNetwork::ServerSend(int Id, std::span<const std::uint8_t> Data)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || !Connection->Accepted || Connection->ServerClosed)
		return Failure(TPipeError::NotConnected);
	if (Connection->ServerSendShut)
		return Failure(TPipeError::Shutdown);
	if (Connection->ClientReleased)
		return Failure(TPipeError::Reset);
	Connection->ToClient.insert(Connection->ToClient.end(), Data.begin(), Data.end());
	if (!Data.empty())
		PostRead(Id, *Connection);
	return TPipeResult{static_cast<int>(Data.size()), TPipeError::None};
}

void TPipeNetwork::ServerShutdown(int Id)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || !Connection->Accepted || Connection->ServerClosed || Connection->ServerSendShut)
		return;
	Connection->ServerSendShut = true;
	PostClose(Id, *Connection, false);
}

void TPipeNetwork::ServerClose(int Id)
{
	TConnection *Connection = Find(Id);
	if (Connection == nullptr || !Connection->Accepted || Connection->ServerClosed)
		return;
	Connection->ServerClosed = true;
	Connection->ServerReset = !Connection->ToServer.empty();
	Connection->ToServer.clear();
	PostClose(Id, *Connection, Connection->ServerReset);
}

} // namespace patella::testmode
