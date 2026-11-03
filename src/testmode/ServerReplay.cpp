// ServerReplay.cpp: the server replay's file format and schedule (ServerReplay.h; issue #23).

#include "ServerReplay.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace patella::testmode {

namespace {

constexpr std::uint32_t XteaDelta = 0x9e3779b9;
constexpr int XteaRounds = 32;

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
	if (Text.size() % 2 != 0)
		return false;
	Bytes->reserve(Text.size() / 2);
	for (std::size_t i = 0; i < Text.size(); i += 2) {
		int High = HexValue(Text[i]);
		int Low = HexValue(Text[i + 1]);
		if (High < 0 || Low < 0)
			return false;
		Bytes->push_back(static_cast<std::uint8_t>((High << 4) | Low));
	}
	return true;
}

bool ParseNumber(std::string_view Text, std::uint32_t Max, std::uint32_t *Value)
{
	std::uint32_t Parsed = 0;
	auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Parsed);
	if (Error != std::errc() || End != Text.data() + Text.size() || Text.empty() || Parsed > Max)
		return false;
	*Value = Parsed;
	return true;
}

std::vector<std::string_view> Fields(std::string_view Line)
{
	std::vector<std::string_view> Out;
	std::size_t i = 0;
	while (i < Line.size()) {
		while (i < Line.size() && (Line[i] == ' ' || Line[i] == '\r' || Line[i] == '\t'))
			i++;
		std::size_t Start = i;
		while (i < Line.size() && Line[i] != ' ' && Line[i] != '\r' && Line[i] != '\t')
			i++;
		if (i > Start)
			Out.push_back(Line.substr(Start, i - Start));
	}
	return Out;
}

std::uint32_t Word(const std::array<std::uint8_t, 16> &Key, std::size_t Index)
{
	return static_cast<std::uint32_t>(Key.at(Index * 4)) | (static_cast<std::uint32_t>(Key.at(Index * 4 + 1)) << 8U)
		| (static_cast<std::uint32_t>(Key.at(Index * 4 + 2)) << 16U) | (static_cast<std::uint32_t>(Key.at(Index * 4 + 3)) << 24U);
}

std::uint32_t Load(std::span<const std::uint8_t> Data, std::size_t Offset)
{
	return static_cast<std::uint32_t>(Data[Offset]) | (static_cast<std::uint32_t>(Data[Offset + 1]) << 8U)
		| (static_cast<std::uint32_t>(Data[Offset + 2]) << 16U) | (static_cast<std::uint32_t>(Data[Offset + 3]) << 24U);
}

void Store(std::span<std::uint8_t> Data, std::size_t Offset, std::uint32_t Value)
{
	for (std::size_t i = 0; i < 4; i++)
		Data[Offset + i] = static_cast<std::uint8_t>(Value >> (8U * i));
}

} // namespace

TReplayParseResult ParseReplay(std::string_view Text)
{
	TReplayParseResult Result;
	int LineNumber = 0;
	bool HeaderSeen = false;
	auto Fail = [&Result, &LineNumber](std::string Message) {
		Result.Ok = false;
		Result.ErrorLine = LineNumber;
		Result.Error = std::move(Message);
		return Result;
	};
	while (!Text.empty()) {
		LineNumber++;
		std::size_t End = Text.find('\n');
		std::string_view Line = Text.substr(0, End);
		Text = End == std::string_view::npos ? std::string_view() : Text.substr(End + 1);
		if (!Line.empty() && Line.back() == '\r')
			Line.remove_suffix(1);
		if (Line.starts_with("#")) {
			if (LineNumber == 1 && Line == ReplayHeader)
				HeaderSeen = true;
			continue;
		}
		std::vector<std::string_view> Words = Fields(Line);
		if (Words.empty())
			continue;
		if (!HeaderSeen)
			return Fail("the file does not start with \"" + std::string(ReplayHeader) + "\"");
		if (Words[0] == "listen") {
			std::uint32_t Port = 0;
			if (Words.size() != 3 || (Words[1] != "login" && Words[1] != "game") || !ParseNumber(Words[2], 65535, &Port) || Port == 0)
				return Fail("listen takes login or game and a port");
			Result.File.Listen.push_back(TReplayListen{std::string(Words[1]), static_cast<int>(Port)});
			continue;
		}
		if (Words.size() < 3 || (Words[0] != "login" && Words[0] != "game"))
			return Fail("a line starts with listen, login or game");
		std::uint32_t Ordinal = 0;
		if (!ParseNumber(Words[1], 1000000, &Ordinal) || Ordinal == 0)
			return Fail("not a connection ordinal: " + std::string(Words[1]));
		TReplayEvent Event;
		Event.Line = LineNumber;
		if (Words[2] == "c2s") {
			if (Words.size() != 3)
				return Fail("c2s takes nothing more");
			Event.Kind = TReplayKind::ClientPacket;
		} else if (Words[2] == "s2c") {
			if (Words.size() != 5 || !ParseNumber(Words[3], UINT32_MAX, &Event.Time) || !ParseHex(Words[4], &Event.Payload))
				return Fail("s2c takes a time and a hex payload");
			Event.Kind = TReplayKind::ServerPacket;
		} else if (Words[2] == "close") {
			if (Words.size() == 5 && Words[3] == "server") {
				if (!ParseNumber(Words[4], UINT32_MAX, &Event.Time))
					return Fail("close server takes a time");
				Event.Kind = TReplayKind::ServerClose;
			} else if (Words.size() == 4 && Words[3] == "client") {
				Event.Kind = TReplayKind::ClientClose;
			} else {
				return Fail("close takes server <time> or client");
			}
		} else {
			return Fail("unknown entry: " + std::string(Words[2]));
		}
		auto Found = std::find_if(Result.File.Connections.begin(), Result.File.Connections.end(),
			[&](const TReplayConnection &Connection) { return Connection.Kind == Words[0] && Connection.Ordinal == static_cast<int>(Ordinal); });
		if (Found == Result.File.Connections.end()) {
			if (Event.Kind != TReplayKind::ClientPacket)
				return Fail("a connection starts with the client's packet (c2s)");
			Result.File.Connections.push_back(TReplayConnection{std::string(Words[0]), static_cast<int>(Ordinal), {}});
			Found = Result.File.Connections.end() - 1;
		}
		Found->Events.push_back(std::move(Event));
	}
	if (!HeaderSeen)
		return Fail("the file does not start with \"" + std::string(ReplayHeader) + "\"");
	Result.Ok = true;
	return Result;
}

TServerReplay::TServerReplay(std::vector<TReplayConnection> Connections) :
	Recorded(std::move(Connections)), Next(Recorded.size(), 0), Waiting(Recorded.size(), 0),
	ClosedByClient(Recorded.size(), false), Opened(Recorded.size(), 0)
{
}

int TServerReplay::Open(std::string_view Kind)
{
	int Count = 0;
	for (std::size_t i = 0; i < Recorded.size(); i++) {
		if (Recorded[i].Kind == Kind && Opened[i] != 0)
			Count++;
	}
	for (std::size_t i = 0; i < Recorded.size(); i++) {
		if (Recorded[i].Kind == Kind && Recorded[i].Ordinal == Count + 1) {
			Opened[i] = 1;
			return static_cast<int>(i);
		}
	}
	return -1;
}

void TServerReplay::Settle(int Index)
{
	auto i = static_cast<std::size_t>(Index);
	const std::vector<TReplayEvent> &Events = Recorded[i].Events;
	while (Next[i] < Events.size()) {
		TReplayKind Kind = Events[Next[i]].Kind;
		if (Kind == TReplayKind::ClientPacket && Waiting[i] > 0) {
			Waiting[i]--;
			Next[i]++;
		} else if (Kind == TReplayKind::ClientClose && ClosedByClient[i]) {
			Next[i]++;
		} else if (ClosedByClient[i] && (Kind == TReplayKind::ServerPacket || Kind == TReplayKind::ServerClose)) {
			// The client is gone: nothing more can be sent.
			Next[i] = Events.size();
		} else {
			break;
		}
	}
	if (Next[i] >= Events.size() && Waiting[i] > 0) {
		Extra += Waiting[i];
		Waiting[i] = 0;
	}
}

void TServerReplay::ClientPacket(int Index)
{
	if (Index < 0 || static_cast<std::size_t>(Index) >= Recorded.size()) {
		Extra++;
		return;
	}
	Waiting[static_cast<std::size_t>(Index)]++;
	Settle(Index);
}

void TServerReplay::ClientClosed(int Index)
{
	if (Index < 0 || static_cast<std::size_t>(Index) >= Recorded.size())
		return;
	ClosedByClient[static_cast<std::size_t>(Index)] = true;
	Settle(Index);
}

bool TServerReplay::NextTime(int Index, std::uint32_t *Time)
{
	if (Index < 0 || static_cast<std::size_t>(Index) >= Recorded.size())
		return false;
	Settle(Index);
	auto i = static_cast<std::size_t>(Index);
	if (Next[i] >= Recorded[i].Events.size())
		return false;
	const TReplayEvent &Event = Recorded[i].Events[Next[i]];
	if (Event.Kind != TReplayKind::ServerPacket && Event.Kind != TReplayKind::ServerClose)
		return false;
	*Time = Event.Time;
	return true;
}

const TReplayEvent *TServerReplay::Due(int Index, std::uint32_t Elapsed)
{
	std::uint32_t Time = 0;
	if (!NextTime(Index, &Time) || Time > Elapsed)
		return nullptr;
	auto i = static_cast<std::size_t>(Index);
	return &Recorded[i].Events[Next[i]];
}

void TServerReplay::Take(int Index)
{
	if (Index < 0 || static_cast<std::size_t>(Index) >= Recorded.size())
		return;
	auto i = static_cast<std::size_t>(Index);
	if (Next[i] < Recorded[i].Events.size())
		Next[i]++;
	Settle(Index);
}

bool TServerReplay::Finished(int Index) const
{
	if (Index < 0 || static_cast<std::size_t>(Index) >= Recorded.size())
		return true;
	auto i = static_cast<std::size_t>(Index);
	return Next[i] >= Recorded[i].Events.size();
}

void XteaEncrypt(std::span<std::uint8_t> Data, const std::array<std::uint8_t, 16> &Key)
{
	for (std::size_t Offset = 0; Offset + 8 <= Data.size(); Offset += 8) {
		std::uint32_t V0 = Load(Data, Offset);
		std::uint32_t V1 = Load(Data, Offset + 4);
		std::uint32_t Sum = 0;
		for (int Round = 0; Round < XteaRounds; Round++) {
			V0 += (((V1 << 4U) ^ (V1 >> 5U)) + V1) ^ (Sum + Word(Key, Sum & 3U));
			Sum += XteaDelta;
			V1 += (((V0 << 4U) ^ (V0 >> 5U)) + V0) ^ (Sum + Word(Key, (Sum >> 11U) & 3U));
		}
		Store(Data, Offset, V0);
		Store(Data, Offset + 4, V1);
	}
}

void XteaDecrypt(std::span<std::uint8_t> Data, const std::array<std::uint8_t, 16> &Key)
{
	for (std::size_t Offset = 0; Offset + 8 <= Data.size(); Offset += 8) {
		std::uint32_t V0 = Load(Data, Offset);
		std::uint32_t V1 = Load(Data, Offset + 4);
		std::uint32_t Sum = XteaDelta * static_cast<std::uint32_t>(XteaRounds);
		for (int Round = 0; Round < XteaRounds; Round++) {
			V1 -= (((V0 << 4U) ^ (V0 >> 5U)) + V0) ^ (Sum + Word(Key, (Sum >> 11U) & 3U));
			Sum -= XteaDelta;
			V0 -= (((V1 << 4U) ^ (V1 >> 5U)) + V1) ^ (Sum + Word(Key, Sum & 3U));
		}
		Store(Data, Offset, V0);
		Store(Data, Offset + 4, V1);
	}
}

std::vector<std::uint8_t> EncryptFrame(std::span<const std::uint8_t> Payload, const std::array<std::uint8_t, 16> &Key)
{
	std::size_t Inner = Payload.size() + 2;
	std::size_t Padded = (Inner + 7) / 8 * 8;
	std::vector<std::uint8_t> Frame(Padded + 2, 0);
	Frame[0] = static_cast<std::uint8_t>(Padded & 0xffU);
	Frame[1] = static_cast<std::uint8_t>((Padded >> 8U) & 0xffU);
	Frame[2] = static_cast<std::uint8_t>(Payload.size() & 0xffU);
	Frame[3] = static_cast<std::uint8_t>((Payload.size() >> 8U) & 0xffU);
	std::copy(Payload.begin(), Payload.end(), Frame.begin() + 4);
	XteaEncrypt(std::span<std::uint8_t>(Frame).subspan(2), Key);
	return Frame;
}

} // namespace patella::testmode
