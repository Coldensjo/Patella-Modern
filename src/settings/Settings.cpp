// Settings.cpp: Patella.ini (Settings.h; Patella-Modern issues #31, #45, plan D12).

#include "settings/Settings.h"

#include "compat/msvc.h"
#include "types/Cp1252.h"

#include "platform/FileSystem.h"
#include "types/File.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <filesystem>
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

namespace {

enum class TRead : unsigned char { Read,
	Missing,
	Failed };

// The file's bytes into Text: Missing where it is not there (the open's ENOENT), Failed where it cannot be opened or
// read, or is larger than MaxFileSize.
TRead ReadFile(patella::platform::IFileSystem &Files, const std::string &Path, std::string &Text)
{
	TFile File;
	errno = 0;
	if (!Files.Open(File, Path.c_str(), TFile::TAccess::Read, TFile::TContent::Binary))
		return errno == ENOENT ? TRead::Missing : TRead::Failed;
	std::array<char, 4096> Buffer;
	while (true) {
		const std::size_t Count = File.Read(Buffer.data(), 1, Buffer.size());
		Text.append(Buffer.data(), Count);
		if (Text.size() > MaxFileSize)
			return TRead::Failed;
		if (Count < Buffer.size())
			break;
	}
	return File.Failed() ? TRead::Failed : TRead::Read;
}

} // namespace

std::optional<TSettingsFile> Load(patella::platform::IFileSystem &Files, const std::string &Path, std::string_view Newline)
{
	std::string Text;
	if (ReadFile(Files, Path, Text) != TRead::Read)
		return std::nullopt;
	return TSettingsFile::Parse(Text, Newline);
}

std::optional<TSettingsFile> LoadForUpdate(patella::platform::IFileSystem &Files, const std::string &Path)
{
	std::string Text;
	switch (ReadFile(Files, Path, Text)) {
	case TRead::Read:
		return TSettingsFile::Parse(Text, NewFileNewline);
	case TRead::Missing:
		return TSettingsFile::Parse(std::string(NewFileComment) + NewFileNewline, NewFileNewline);
	case TRead::Failed:
		break;
	}
	return std::nullopt;
}

bool Save(patella::platform::IFileSystem &Files, const std::string &Path, const TSettingsFile &Settings)
{
	TFile File;
	if (!Files.Open(File, Path.c_str(), TFile::TAccess::Write, TFile::TContent::Binary))
		return false;
	const bool Written = File.Write(Settings.Text());
	return File.Close() == 0 && Written;
}

// GCC's analyzer (-fanalyzer, issue #130) follows a std::filesystem::path copy whose string throws length_error into
// "use of uninitialized value" after the throw: a false positive of its exception paths through libstdc++.
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-use-of-uninitialized-value\"")

namespace {

// Tibia.dat in Folder (FindNoCase's spelling where CaseInsensitive).
bool HasData(const std::filesystem::path &Folder, bool CaseInsensitive)
{
	if (Folder.empty())
		return false;
	try {
		const std::filesystem::path DataFile = CaseInsensitive ? patella::platform::FindNoCase(Folder, "Tibia.dat")
															   : Folder / "Tibia.dat";
		std::error_code Error;
		return std::filesystem::is_regular_file(DataFile, Error);
	} catch (...) { // std::bad_alloc of the path
		return false;
	}
}

// The folder that DataFolder of Folder's Patella.ini names (relative to Folder), where it has one.
std::optional<std::filesystem::path> ConfiguredData(const std::filesystem::path &Folder, bool CaseInsensitive)
{
	if (Folder.empty())
		return std::nullopt;
	patella::platform::TDataFileSystem Files(patella::platform::TFileLocations{Folder, {}, CaseInsensitive});
	const std::optional<TSettingsFile> File = Load(Files, FileName);
	if (!File)
		return std::nullopt;
	const std::optional<std::string> Value = File->Get(DataFolderKey);
	if (!Value || Value->empty())
		return std::nullopt;
	// The file's text: UTF-8, or the client's Windows-1252 in a file of version 1 (EncodingOf).
	const std::string Utf8 = EncodingOf(*File) == TEncoding::Cp1252 ? patella::text::Cp1252ToUtf8(*Value) : *Value;
	return Folder / std::filesystem::path(std::u8string(Utf8.begin(), Utf8.end()));
}

std::filesystem::path Absolute(const std::filesystem::path &Working, const std::filesystem::path &Folder)
{
	return (Folder.is_absolute() ? Folder : Working / Folder).lexically_normal();
}

bool SameFolder(const std::filesystem::path &Left, const std::filesystem::path &Right)
{
	std::error_code Error;
	const bool Same = std::filesystem::equivalent(Left, Right, Error);
	return !Error ? Same : Left.lexically_normal() == Right.lexically_normal();
}

} // namespace

TLocationChoice ChooseLocations(const TLocationInputs &Inputs)
{
	TLocationChoice Choice;
	Choice.Locations.CaseInsensitive = Inputs.CaseInsensitive;
	const std::filesystem::path Working = Inputs.Working.lexically_normal();
	std::filesystem::path Data = Working;
	std::filesystem::path IniFolder; // step 4's
	if (Inputs.DataOption) {
		Choice.Source = TDataSource::Option;
		Data = Absolute(Working, *Inputs.DataOption);
	} else if (HasData(Working, Inputs.CaseInsensitive)) {
		Choice.Source = TDataSource::Working;
	} else if (!Inputs.Executable.empty() && HasData(Absolute(Working, Inputs.Executable), Inputs.CaseInsensitive)) {
		Choice.Source = TDataSource::Executable;
		Data = Absolute(Working, Inputs.Executable);
	} else {
		std::vector<std::filesystem::path> Candidates;
		if (Inputs.Xdg)
			Candidates.push_back(Inputs.XdgFolder);
		else
			Candidates = {Working, Inputs.Executable};
		for (const std::filesystem::path &Candidate : Candidates) {
			if (Candidate.empty())
				continue;
			const std::filesystem::path Folder = Absolute(Working, Candidate);
			const std::optional<std::filesystem::path> Configured = ConfiguredData(Folder, Inputs.CaseInsensitive);
			if (Configured && HasData(Absolute(Working, *Configured), Inputs.CaseInsensitive)) {
				Choice.Source = TDataSource::Settings;
				Data = Absolute(Working, *Configured);
				IniFolder = Folder;
				break;
			}
		}
	}
	std::filesystem::path Settings = Data;
	if (Inputs.Xdg && !Inputs.XdgFolder.empty())
		Settings = Absolute(Working, Inputs.XdgFolder);
	else if (!IniFolder.empty())
		Settings = IniFolder;
	if (!SameFolder(Data, Working))
		Choice.Locations.Data = Data;
	if (!SameFolder(Settings, Data))
		Choice.Locations.Settings = Settings;
	Choice.DataFolder = std::move(Data);
	Choice.SettingsFolder = std::move(Settings);
	return Choice;
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")

std::filesystem::path XdgSettingsFolder(const char *XdgConfigHome, const char *Home)
{
	if (XdgConfigHome != nullptr && *XdgConfigHome != 0) {
		const std::filesystem::path Folder(XdgConfigHome);
		if (Folder.is_absolute())
			return Folder / "patella";
	}
	if (Home != nullptr && *Home != 0)
		return std::filesystem::path(Home) / ".config" / "patella";
	return {};
}

const char *SourceName(TDataSource Source)
{
	switch (Source) {
	case TDataSource::Option:
		return "--data";
	case TDataSource::Working:
		return "the working folder";
	case TDataSource::Executable:
		return "the executable's folder";
	case TDataSource::Settings:
		return "DataFolder of Patella.ini";
	case TDataSource::Default:
		break;
	}
	return "the working folder (no Tibia.dat found)";
}

} // namespace patella::settings
