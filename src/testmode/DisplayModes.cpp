// DisplayModes.cpp: the test build's fixed list of display modes (issue #48; DisplayModes.h).

#include "testmode/DisplayModes.h"

#include <charconv>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace patella::testmode {

namespace {

// Larger than any display's side or depth.
constexpr std::uint32_t MaxSide = 65535;
constexpr std::uint32_t MaxDepth = 64;

bool ParseNumber(std::string_view Text, std::uint32_t Max, std::uint32_t *Value)
{
	std::uint32_t Parsed = 0;
	auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Parsed);
	if (Error != std::errc() || End != Text.data() + Text.size() || Text.empty() || Parsed == 0 || Parsed > Max)
		return false;
	*Value = Parsed;
	return true;
}

std::vector<std::string_view> Fields(std::string_view Line)
{
	std::vector<std::string_view> Out;
	std::size_t i = 0;
	while (i < Line.size()) {
		while (i < Line.size() && (Line[i] == ' ' || Line[i] == '\t'))
			i++;
		const std::size_t Start = i;
		while (i < Line.size() && Line[i] != ' ' && Line[i] != '\t')
			i++;
		if (i > Start)
			Out.push_back(Line.substr(Start, i - Start));
	}
	return Out;
}

} // namespace

TDisplayModesParseResult ParseDisplayModes(std::string_view Text)
{
	TDisplayModesParseResult Result;
	int LineNumber = 0;
	bool HeaderSeen = false;
	const auto Fail = [&Result, &LineNumber](std::string Message) {
		Result.Ok = false;
		Result.Modes.clear();
		Result.ErrorLine = LineNumber;
		Result.Error = std::move(Message);
		return Result;
	};
	while (!Text.empty()) {
		LineNumber++;
		const std::size_t End = Text.find('\n');
		std::string_view Line = Text.substr(0, End);
		Text = End == std::string_view::npos ? std::string_view() : Text.substr(End + 1);
		if (!Line.empty() && Line.back() == '\r')
			Line.remove_suffix(1);
		if (Line.starts_with("#")) {
			if (LineNumber == 1 && Line == DisplayModesHeader)
				HeaderSeen = true;
			continue;
		}
		const std::vector<std::string_view> Words = Fields(Line);
		if (Words.empty())
			continue;
		if (!HeaderSeen)
			return Fail("the file does not start with \"" + std::string(DisplayModesHeader) + "\"");
		TDisplayMode Mode;
		if (Words.size() != 3 || !ParseNumber(Words[0], MaxSide, &Mode.Width) || !ParseNumber(Words[1], MaxSide, &Mode.Height)
			|| !ParseNumber(Words[2], MaxDepth, &Mode.Depth))
			return Fail("expected <width> <height> <bits per pixel>");
		Result.Modes.push_back(Mode);
	}
	LineNumber = 0;
	if (!HeaderSeen)
		return Fail("the file does not start with \"" + std::string(DisplayModesHeader) + "\"");
	if (Result.Modes.empty())
		return Fail("no display modes");
	Result.Ok = true;
	return Result;
}

} // namespace patella::testmode
