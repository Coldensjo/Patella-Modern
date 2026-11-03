// ServerReplay.h: the server side of a reference recording, replayed inside the test build (issue #23;
// notes/modernization-plan.md, 4.4; notes/testmode.md, "The server replay").
//
// The reference recordings (tools/reference.py) hold the packets the server sent to Tibia.exe in a
// reference run, each with the virtual time at which it was sent, and the order of the server's and
// the client's packets on every connection. PatellaTest.exe --test-replay <file> plays the server side
// of that recording to the client without a server: TestModeWin32.cpp listens on the recording's ports
// on 127.0.0.1, takes the XTEA key of each connection from the client's RSA block (the hook before the
// RSA encryption), encrypts the recorded payloads (EncryptFrame) and sends them as this schedule says.
// This file is the portable half: the file format and the schedule. Portable: no platform header.
//
// The file, one line per entry, fields separated by one space; # starts a comment line:
//
//	# patella-replay 1
//	listen <kind> <port>                       the port the client connects to for login or game
//	<kind> <ordinal> c2s                       the client sends a packet here (the replay waits for it)
//	<kind> <ordinal> s2c <time> <payload>      the server's packet (lower-case hex payload, - when empty),
//	                                           due at <time> virtual milliseconds
//	<kind> <ordinal> close server <time>       the server closes the connection, due at <time>
//	<kind> <ordinal> close client              the client closes it here
//
// kind is login or game, ordinal the how-manieth connection of that kind (from 1), as tools/proxy.py's
// conn_ordinal; each connection's entries are in their recorded order and start with its first c2s
// (the login packet). The schedule of one connection is the proxy's replay (tools/proxy.py
// replay_connection): a server packet or close is due once the client packets recorded before it have
// come and its time has come; a client packet that comes while the recording still has server packets
// before its place waits there (it counts when the schedule reaches it).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace patella::testmode {

inline constexpr std::string_view ReplayHeader = "# patella-replay 1";

enum class TReplayKind : std::uint8_t {
	ClientPacket,
	ServerPacket, // Time, Payload
	ServerClose,  // Time
	ClientClose,
};

struct TReplayEvent {
	TReplayKind Kind = TReplayKind::ClientPacket;
	std::uint32_t Time = 0; // virtual milliseconds after the start
	std::vector<std::uint8_t> Payload;
	int Line = 0; // the replay file's line

	friend bool operator==(const TReplayEvent &, const TReplayEvent &) = default;
};

struct TReplayConnection {
	std::string Kind; // login or game
	int Ordinal = 0;
	std::vector<TReplayEvent> Events;
};

struct TReplayListen {
	std::string Kind;
	int Port = 0;
};

struct TReplayFile {
	std::vector<TReplayListen> Listen;
	std::vector<TReplayConnection> Connections; // in the order of their first lines
};

struct TReplayParseResult {
	bool Ok = false;
	TReplayFile File;
	int ErrorLine = 0;
	std::string Error;
};

[[nodiscard]] TReplayParseResult ParseReplay(std::string_view Text);

// The schedule of the replay: which recorded connection the client's connections are and what is due
// on each.
class TServerReplay {
public:
	TServerReplay() = default;
	explicit TServerReplay(std::vector<TReplayConnection> Recorded);

	// The client opened a connection of Kind: the index of the recorded connection it is (its
	// ordinal-th of that kind), or -1 when the recording has none.
	int Open(std::string_view Kind);
	// The client sent a packet on connection Index (a recorded c2s is passed when the schedule gets
	// there).
	void ClientPacket(int Index);
	// The client closed connection Index.
	void ClientClosed(int Index);
	// The server packet or close due on connection Index at Elapsed, or nullptr.
	[[nodiscard]] const TReplayEvent *Due(int Index, std::uint32_t Elapsed);
	// The time of the next server packet or close of connection Index once its client packets have
	// come (false: none, or it waits for the client).
	[[nodiscard]] bool NextTime(int Index, std::uint32_t *Time);
	// The event Due returned is done.
	void Take(int Index);
	// Every event of connection Index done.
	[[nodiscard]] bool Finished(int Index) const;
	// Client packets that came where the recording had none (after its end, or after a close).
	[[nodiscard]] int Unexpected() const { return Extra; }
	[[nodiscard]] const TReplayConnection &Connection(int Index) const { return Recorded.at(static_cast<std::size_t>(Index)); }
	[[nodiscard]] std::size_t Position(int Index) const { return Next.at(static_cast<std::size_t>(Index)); }

private:
	// Passes the recorded client packets that have come.
	void Settle(int Index);

	std::vector<TReplayConnection> Recorded;
	std::vector<std::size_t> Next;   // per connection: the next event
	std::vector<int> Waiting;        // per connection: client packets that came, not yet passed
	std::vector<bool> ClosedByClient; // per connection
	std::vector<int> Opened;         // per connection: opened by the client
	int Extra = 0;
};

// A server packet as it goes over the wire (src/Network.cpp PrepareNextPacket; tools/proxy.py
// encrypt_frame): [u16 size][XTEA([u16 length][payload][zero padding to 8 bytes])].
[[nodiscard]] std::vector<std::uint8_t> EncryptFrame(std::span<const std::uint8_t> Payload, const std::array<std::uint8_t, 16> &Key);
// XTEA (src/TXTEA.cpp; tools/proxy.py): Data's 8-byte blocks in place, 32 rounds, little-endian words.
void XteaEncrypt(std::span<std::uint8_t> Data, const std::array<std::uint8_t, 16> &Key);
void XteaDecrypt(std::span<std::uint8_t> Data, const std::array<std::uint8_t, 16> &Key);

} // namespace patella::testmode
