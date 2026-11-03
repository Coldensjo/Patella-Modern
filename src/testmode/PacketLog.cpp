// PacketLog.cpp: the line format and the connections of the test mode's packet log (PacketLog.h;
// issue #22).

#include "PacketLog.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace patella::testmode {

namespace {

constexpr std::string_view HexDigits = "0123456789abcdef";

std::string Hex(const std::vector<std::uint8_t> &Bytes)
{
	if (Bytes.empty())
		return "-";
	std::string Out;
	Out.reserve(Bytes.size() * 2);
	for (std::uint8_t Byte : Bytes) {
		Out += HexDigits[Byte >> 4U];
		Out += HexDigits[Byte & 0xfU];
	}
	return Out;
}

int HexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

bool ParseHex(std::string_view Text, std::vector<std::uint8_t> *Bytes)
{
	Bytes->clear();
	if (Text == "-")
		return true;
	if (Text.empty() || Text.size() % 2 != 0)
		return false;
	for (std::size_t i = 0; i < Text.size(); i += 2) {
		const int High = HexValue(Text[i]);
		const int Low = HexValue(Text[i + 1]);
		if (High < 0 || Low < 0)
			return false;
		Bytes->push_back(static_cast<std::uint8_t>((High << 4) | Low));
	}
	return true;
}

template <class T> bool ParseNumber(std::string_view Text, T *Value)
{
	auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), *Value);
	return Error == std::errc() && End == Text.data() + Text.size() && !Text.empty();
}

} // namespace

std::string PacketLogLine(const TPacketRecord &Record)
{
	std::string Line = std::to_string(Record.Time);
	Line += Record.Direction == TPacketDirection::ClientToServer ? " c2s " : " s2c ";
	Line += Record.Kind + " " + std::to_string(Record.Ordinal);
	Line += Record.Plain ? " plain " : " xtea ";
	Line += Hex(Record.Payload) + " " + Hex(Record.Padding);
	return Line;
}

bool ParsePacketLogLine(std::string_view Line, TPacketRecord *Record)
{
	if (Line.ends_with('\r'))
		Line.remove_suffix(1);
	if (Line.empty() || Line.front() == '#')
		return false;
	std::vector<std::string_view> Fields;
	while (!Line.empty()) {
		std::size_t Space = Line.find(' ');
		Fields.push_back(Line.substr(0, Space));
		Line = Space == std::string_view::npos ? std::string_view() : Line.substr(Space + 1);
	}
	if (Fields.size() != 7)
		return false;
	TPacketRecord Parsed;
	if (!ParseNumber(Fields[0], &Parsed.Time))
		return false;
	if (Fields[1] == "c2s")
		Parsed.Direction = TPacketDirection::ClientToServer;
	else if (Fields[1] == "s2c")
		Parsed.Direction = TPacketDirection::ServerToClient;
	else
		return false;
	if (Fields[2] != "login" && Fields[2] != "game" && Fields[2] != "other")
		return false;
	Parsed.Kind = std::string(Fields[2]);
	if (!ParseNumber(Fields[3], &Parsed.Ordinal) || Parsed.Ordinal < 0)
		return false;
	if (Fields[4] == "plain")
		Parsed.Plain = true;
	else if (Fields[4] != "xtea")
		return false;
	if (!ParseHex(Fields[5], &Parsed.Payload) || !ParseHex(Fields[6], &Parsed.Padding))
		return false;
	*Record = std::move(Parsed);
	return true;
}

TPacketRecord TConnections::Make(std::uint32_t Time, TPacketDirection Direction, std::span<const std::uint8_t> Payload,
	std::span<const std::uint8_t> Padding) const
{
	TPacketRecord Record;
	Record.Time = Time;
	Record.Direction = Direction;
	Record.Kind = Kind;
	Record.Ordinal = Ordinal;
	Record.Payload.assign(Payload.begin(), Payload.end());
	Record.Padding.assign(Padding.begin(), Padding.end());
	return Record;
}

TPacketRecord TConnections::Sent(std::uint32_t Time, bool Plain, std::span<const std::uint8_t> Payload,
	std::span<const std::uint8_t> Padding)
{
	if (Plain) {
		std::uint8_t Opcode = Payload.empty() ? 0 : Payload[0];
		if (Opcode == LoginOpcode) {
			Kind = "login";
			Ordinal = ++Logins;
		} else if (Opcode == GameOpcode) {
			Kind = "game";
			Ordinal = ++Games;
		} else {
			Kind = "other";
			Ordinal = ++Others;
		}
	}
	TPacketRecord Record = Make(Time, TPacketDirection::ClientToServer, Payload, Plain ? std::span<const std::uint8_t>() : Padding);
	Record.Plain = Plain;
	return Record;
}

TPacketRecord TConnections::Received(std::uint32_t Time, std::span<const std::uint8_t> Payload,
	std::span<const std::uint8_t> Padding)
{
	return Make(Time, TPacketDirection::ServerToClient, Payload, Padding);
}

} // namespace patella::testmode
