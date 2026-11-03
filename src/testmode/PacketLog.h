// PacketLog.h: the packet log of the test mode (issue #22; notes/modernization-plan.md, 4.4;
// notes/testmode.md, "The packet log").
//
// The test build writes every packet it sends, before encryption, and every packet it receives, after
// decryption, as one line of text (packets.log): the same payloads tools/proxy.py decrypts from the
// wire, so that the two can be compared packet for packet (the issue's criterion), and issue #23 can
// compare them with Tibia.exe's recordings. This file is the portable half: the line format and the
// connections (src/Network.cpp and src/Communication.cpp call the Windows side through the hooks of
// src/TestHooks.h). Portable: no platform header.
//
// The file: the header line PacketLogHeader, then one line per packet, fields separated by one space:
//
//	<time> <dir> <kind> <ordinal> <form> <payload> <padding>
//
//	time      milliseconds: the script's virtual time (without a script: since the first packet)
//	dir       c2s (sent by the client) or s2c (received and decrypted by the client)
//	kind      login or game (the connection's first packet: opcode 0x01 or 0x0a; other: anything else)
//	ordinal   the how-manieth connection of that kind (1, 2, ...), as tools/proxy.py's conn_ordinal
//	form      plain: the connection's first packet, not encrypted ([opcode][header][RSA block]), with the
//	          RSA block as it was before RSA encryption ([0][XTEA key][fields][random bytes]), as
//	          tools/proxy.py's "plain" of a first packet; xtea: an XTEA packet, the payload without its
//	          length and padding, as proxy.py's "plain"
//	payload   lower-case hex, - when empty
//	padding   an XTEA packet's padding (lower-case hex, proxy.py's "pad"), - when empty or plain
//
// Lines starting with # are comments (the header).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace patella::testmode {

inline constexpr std::string_view PacketLogHeader = "# patella-packets 1";
// The first packets' opcodes (Communication.cpp SendLogin, SendGameLogin).
inline constexpr std::uint8_t LoginOpcode = 0x01;
inline constexpr std::uint8_t GameOpcode = 0x0a;
// The RSA block's size at the end of a first packet.
inline constexpr std::size_t RsaBlockSize = 128;

enum class TPacketDirection : std::uint8_t {
	ClientToServer,
	ServerToClient,
};

struct TPacketRecord {
	std::uint32_t Time = 0;
	TPacketDirection Direction = TPacketDirection::ClientToServer;
	std::string Kind; // login, game, other
	int Ordinal = 0;
	bool Plain = false; // the unencrypted first packet
	std::vector<std::uint8_t> Payload;
	std::vector<std::uint8_t> Padding;

	friend bool operator==(const TPacketRecord &, const TPacketRecord &) = default;
};

// The line of a record (without the newline).
[[nodiscard]] std::string PacketLogLine(const TPacketRecord &Record);
// A line back into a record; false for a comment, an empty line or a line that does not parse.
bool ParsePacketLogLine(std::string_view Line, TPacketRecord *Record);

// The connections of a run: every unencrypted packet the client sends starts one (its opcode says the
// kind); the packets after it, both ways, belong to it.
class TConnections {
public:
	// A packet the client sends; Plain: unencrypted, a new connection.
	TPacketRecord Sent(std::uint32_t Time, bool Plain, std::span<const std::uint8_t> Payload,
		std::span<const std::uint8_t> Padding);
	// A packet the client received and decrypted.
	TPacketRecord Received(std::uint32_t Time, std::span<const std::uint8_t> Payload,
		std::span<const std::uint8_t> Padding);

private:
	[[nodiscard]] TPacketRecord Make(std::uint32_t Time, TPacketDirection Direction, std::span<const std::uint8_t> Payload,
		std::span<const std::uint8_t> Padding) const;

	std::string Kind = "other";
	int Ordinal = 0;
	int Logins = 0;
	int Games = 0;
	int Others = 0;
};

} // namespace patella::testmode
