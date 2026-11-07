// Settings.cpp: Patella.ini (Settings.h; Patella-Modern issues #31, #45, plan D12).

#include "settings/Settings.h"

#include "types/Cp1252.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace patella::settings {

namespace {

constexpr std::string_view Blanks = " \t\r";
constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";

std::string_view Trim(std::string_view Text)
{
	const size_t First = Text.find_first_not_of(Blanks);
	if (First == std::string_view::npos)
		return {};
	const size_t Last = Text.find_last_not_of(Blanks);
	return Text.substr(First, Last - First + 1);
}

char Lower(char c)
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool SameKey(std::string_view a, std::string_view b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); i++) {
		if (Lower(a[i]) != Lower(b[i]))
			return false;
	}
	return true;
}

bool HasLineEnd(std::string_view Text)
{
	return Text.find_first_of(std::string_view("\r\n\0", 3)) != std::string_view::npos;
}

bool Padded(std::string_view Text)
{
	return !Text.empty() && (Text.front() == ' ' || Text.front() == '\t' || Text.back() == ' ' || Text.back() == '\t');
}

} // namespace

TSettingsFile::TSettingsFile(std::string_view Newline) : LineEnd(Newline) {}

TSettingsFile::TLine TSettingsFile::ParseLine(std::string Text, bool First)
{
	TLine Line;
	std::string_view Content = Text;
	if (First && Content.starts_with(ByteOrderMark))
		Content.remove_prefix(ByteOrderMark.size());
	const std::string_view Trimmed = Trim(Content);
	if (!Trimmed.empty() && Trimmed.front() != '#' && Trimmed.front() != ';') {
		const size_t Equals = Trimmed.find('=');
		const std::string_view Key = Equals == std::string_view::npos ? std::string_view() : Trim(Trimmed.substr(0, Equals));
		if (Key.empty())
			Line.Bad = true;
		else {
			Line.Key = std::string(Key);
			Line.Value = std::string(Trim(Trimmed.substr(Equals + 1)));
		}
	}
	Line.Text = std::move(Text);
	return Line;
}

TSettingsFile TSettingsFile::Parse(std::string_view Text, std::string_view Newline)
{
	const size_t FirstEnd = Text.find('\n');
	const bool Crlf = FirstEnd != std::string_view::npos && FirstEnd > 0 && Text[FirstEnd - 1] == '\r';
	TSettingsFile Result(FirstEnd == std::string_view::npos ? Newline : Crlf ? "\r\n"
																			 : "\n");
	Result.FinalNewline = Text.empty() || Text.back() == '\n';
	size_t Start = 0;
	while (Start < Text.size()) {
		size_t End = Text.find('\n', Start);
		const bool Ended = End != std::string_view::npos;
		if (!Ended)
			End = Text.size();
		std::string_view Line = Text.substr(Start, End - Start);
		// With CRLF line ends the CR belongs to the end; a lone CR stays in the line (and is a blank), and so does a CR
		// at the end of a last line without a line end (Patella-Modern issue #132: it went, so Text() lost it).
		if (Crlf && Ended && Line.ends_with('\r'))
			Line.remove_suffix(1);
		Result.Lines.push_back(ParseLine(std::string(Line), Result.Lines.empty()));
		Start = End + 1;
	}
	return Result;
}

std::optional<std::string> TSettingsFile::Get(std::string_view Key) const
{
	for (const TLine &Line : Lines) {
		if (!Line.Key.empty() && SameKey(Line.Key, Key))
			return Line.Value;
	}
	return std::nullopt;
}

bool TSettingsFile::Set(std::string_view Key, std::string_view Value)
{
	// A key that starts with a byte order mark (Patella-Modern issue #132): on the first line the next parse would take
	// the mark as the file's, and the key would be another.
	if (Key.empty() || Key.find('=') != std::string_view::npos || Key.front() == '#' || Key.front() == ';'
		|| Key.starts_with(ByteOrderMark) || HasLineEnd(Key) || Padded(Key) || HasLineEnd(Value) || Padded(Value))
		return false;
	const std::string Text = std::string(Key) + "=" + std::string(Value);
	for (size_t i = 0; i < Lines.size(); i++) {
		TLine &Line = Lines[i];
		if (Line.Key.empty() || !SameKey(Line.Key, Key))
			continue;
		const bool Bom = i == 0 && Line.Text.starts_with(ByteOrderMark);
		Line = ParseLine((Bom ? std::string(ByteOrderMark) : std::string()) + Text, i == 0);
		return true;
	}
	Lines.push_back(ParseLine(Text, Lines.empty()));
	FinalNewline = true;
	return true;
}

int TSettingsFile::Remove(std::string_view Key)
{
	int Removed = 0;
	for (size_t i = Lines.size(); i-- > 0;) {
		if (!Lines[i].Key.empty() && SameKey(Lines[i].Key, Key)) {
			Lines.erase(Lines.begin() + static_cast<std::ptrdiff_t>(i));
			Removed++;
		}
	}
	return Removed;
}

std::vector<std::string> TSettingsFile::Keys() const
{
	std::vector<std::string> Result;
	for (const TLine &Line : Lines) {
		if (!Line.Key.empty())
			Result.push_back(Line.Key);
	}
	return Result;
}

std::string TSettingsFile::Text() const
{
	std::string Result;
	for (size_t i = 0; i < Lines.size(); i++) {
		if (i > 0)
			Result += LineEnd;
		Result += Lines[i].Text;
	}
	if (!Lines.empty() && FinalNewline)
		Result += LineEnd;
	return Result;
}

std::vector<int> TSettingsFile::BadLines() const
{
	std::vector<int> Result;
	for (size_t i = 0; i < Lines.size(); i++) {
		if (Lines[i].Bad)
			Result.push_back(static_cast<int>(i + 1));
	}
	return Result;
}

TEncoding EncodingOf(const TSettingsFile &File)
{
	return File.Get(VersionKey) == "1" ? TEncoding::Cp1252 : TEncoding::Utf8;
}

std::string ClientText(const TSettingsFile &File, std::string_view Value)
{
	return EncodingOf(File) == TEncoding::Cp1252 ? std::string(Value) : patella::text::Utf8ToCp1252(Value);
}

std::optional<TSettingsFile> Load(const std::string &Path, std::string_view Newline)
{
	std::ifstream File(Path, std::ios::binary);
	if (!File)
		return std::nullopt;
	std::string Text;
	std::array<char, 4096> Buffer;
	while (File.read(Buffer.data(), Buffer.size()) || File.gcount() > 0) {
		Text.append(Buffer.data(), static_cast<size_t>(File.gcount()));
		if (Text.size() > MaxFileSize)
			return std::nullopt;
	}
	if (File.bad())
		return std::nullopt;
	return TSettingsFile::Parse(Text, Newline);
}

std::optional<TSettingsFile> LoadForUpdate(const std::string &Path)
{
	if (std::optional<TSettingsFile> File = Load(Path, NewFileNewline))
		return File;
	std::error_code Error;
	if (std::filesystem::exists(Path, Error) || Error)
		return std::nullopt;
	return TSettingsFile::Parse(std::string(NewFileComment) + NewFileNewline, NewFileNewline);
}

bool Save(const std::string &Path, const TSettingsFile &Settings)
{
	std::ofstream File(Path, std::ios::binary | std::ios::trunc);
	if (!File)
		return false;
	std::string Text = Settings.Text();
	File.write(Text.data(), static_cast<std::streamsize>(Text.size()));
	File.close();
	return !File.fail();
}

} // namespace patella::settings
