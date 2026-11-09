// PreferenceKeys.cpp: the options of Tibia.cfg as keys of Patella.ini (PreferenceKeys.h; Patella-Modern issues #45, #117).

#include "settings/PreferenceKeys.h"

#include "settings/Settings.h"
#include "types/Cp1252.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace patella::settings {

namespace {

constexpr std::string_view Blanks = " \t";
constexpr std::string_view HexDigits = "0123456789abcdef";

using TKind = TField::TKind;

char Lower(char c)
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool SameText(std::string_view a, std::string_view b)
{
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return Lower(x) == Lower(y); });
}

std::string_view Trim(std::string_view Text)
{
	const size_t First = Text.find_first_not_of(Blanks);
	if (First == std::string_view::npos)
		return {};
	return Text.substr(First, Text.find_last_not_of(Blanks) - First + 1);
}

int HexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

const char *YesNo(bool Value)
{
	return Value ? "yes" : "no";
}

// The number after Prefix in Key (Hotkey12: 12); nothing when Key is not Prefix and decimal digits.
std::optional<int> IndexOf(std::string_view Key, std::string_view Prefix)
{
	if (Key.size() <= Prefix.size() || Key.size() > Prefix.size() + 4 || !SameText(Key.substr(0, Prefix.size()), Prefix))
		return std::nullopt;
	int Index = 0;
	for (const char c : Key.substr(Prefix.size())) {
		if (c < '0' || c > '9')
			return std::nullopt;
		Index = Index * 10 + (c - '0');
	}
	return Index;
}

std::string Numbers(const std::vector<int> &List)
{
	std::string Result;
	for (const int Number : List) {
		if (!Result.empty())
			Result += ',';
		Result += std::to_string(Number);
	}
	return Result;
}

std::string Texts(const std::vector<std::string> &List)
{
	std::string Result;
	for (const std::string &Text : List) {
		if (!Result.empty())
			Result += ',';
		Result += QuoteText(Text);
	}
	return Result;
}

// The keys Prefix<n> of File with their numbers, in the numbers' order (the first line of a number counts).
std::vector<std::pair<int, std::string>> Indexed(const TSettingsFile &File, std::string_view Prefix)
{
	std::vector<std::pair<int, std::string>> Result;
	for (const std::string &Key : File.Keys()) {
		const std::optional<int> Index = IndexOf(Key, Prefix);
		if (Index && std::none_of(Result.begin(), Result.end(), [&](const auto &Entry) { return Entry.first == *Index; }))
			Result.emplace_back(*Index, Key);
	}
	std::sort(Result.begin(), Result.end());
	return Result;
}

// Removes the Prefix<n> lines whose n is not in Keep (Keep[n] true), and those whose n is not written as the writer writes
// it (Buddy01 for Buddy1, Patella-Modern issue #132: Indexed takes the first line of a number, so such a line before the
// written one would be read in its place, and a renumbered entry lost).
void RemoveIndexed(TSettingsFile &File, std::string_view Prefix, const std::vector<bool> &Keep)
{
	for (const std::string &Key : File.Keys()) {
		const std::optional<int> Index = IndexOf(Key, Prefix);
		if (Index && (static_cast<size_t>(*Index) >= Keep.size() || !Keep[static_cast<size_t>(*Index)] || !SameText(Key, std::string(Prefix) + std::to_string(*Index))))
			File.Remove(Key);
	}
}

void Put(TSettingsFile &File, std::string_view Key, std::string_view Value)
{
	// Set refuses only what a value written here never is (line ends are escaped, a quoted text is not padded).
	[[maybe_unused]] const bool Done = File.Set(Key, Value);
}

class TReader {
public:
	TReader(const TSettingsFile &Source, std::vector<std::string> &Found) : File(Source), Problems(Found), Encoding(EncodingOf(Source)) {}

	// The fields of Key's value when they have the kinds of Pattern ('n' number, 'f' flag, 't' text, '*' any
	// number of the kind after it); nothing without a line, or (reported) when they do not.
	std::optional<std::vector<TField>> Fields(std::string_view Key, std::string_view Pattern)
	{
		const std::optional<std::string> Value = File.Get(Key);
		if (!Value)
			return std::nullopt;
		std::optional<std::vector<TField>> Result = ParseFields(*Value, Encoding);
		if (Result && Matches(*Result, Pattern))
			return Result;
		Problems.push_back(std::string(Key) + "=" + ClientText(File, *Value) + ": not " + Describe(Pattern) + "; kept as it was");
		return std::nullopt;
	}

	void Flag(std::string_view Key, bool &Value)
	{
		if (const auto Result = Fields(Key, "f"))
			Value = (*Result)[0].Flag;
	}

	void Number(std::string_view Key, int &Value)
	{
		if (const auto Result = Fields(Key, "n"))
			Value = (*Result)[0].Number;
	}

	template <size_t N>
	void NumberArray(std::string_view Key, std::array<int, N> &Value)
	{
		if (const auto Result = Fields(Key, std::string(N, 'n'))) {
			for (size_t i = 0; i < N; i++)
				Value[i] = (*Result)[i].Number;
		}
	}

	void NumberList(std::string_view Key, std::vector<int> &Value)
	{
		if (const auto Result = Fields(Key, "*n")) {
			Value.clear();
			for (const TField &Field : *Result)
				Value.push_back(Field.Number);
		}
	}

	void TextList(std::string_view Key, std::vector<std::string> &Value)
	{
		if (const auto Result = Fields(Key, "*t")) {
			Value.clear();
			for (const TField &Field : *Result)
				Value.push_back(Field.Text);
		}
	}

	void Problem(std::string Text) { Problems.push_back(std::move(Text)); }

private:
	static bool Matches(const std::vector<TField> &Fields, std::string_view Pattern)
	{
		if (Pattern.starts_with('*')) {
			const TKind Kind = KindOf(Pattern[1]);
			return std::all_of(Fields.begin(), Fields.end(), [&](const TField &Field) { return Field.Kind == Kind; });
		}
		if (Fields.size() != Pattern.size())
			return false;
		for (size_t i = 0; i < Fields.size(); i++) {
			if (Fields[i].Kind != KindOf(Pattern[i]))
				return false;
		}
		return true;
	}

	static TKind KindOf(char c) { return c == 'f' ? TKind::Flag : c == 't' ? TKind::Text
																		   : TKind::Number; }

	static std::string Describe(std::string_view Pattern)
	{
		const auto Name = [](char c) { return c == 'f' ? "yes or no" : c == 't' ? "a quoted text"
																				: "a number"; };
		if (Pattern.starts_with('*'))
			return std::string("a list of ") + (Pattern[1] == 't' ? "quoted texts" : "numbers");
		std::string Result;
		for (const char c : Pattern)
			Result += std::string(Result.empty() ? "" : ", ") + Name(c);
		return Result;
	}

	const TSettingsFile &File;
	std::vector<std::string> &Problems;
	TEncoding Encoding;
};

// The bytes Windows-1252 leaves undefined: the C1 controls of their value (types/Cp1252.h), escaped as the C0 ones.
bool Undefined(unsigned char Byte)
{
	return Byte == 0x81 || Byte == 0x8d || Byte == 0x8f || Byte == 0x90 || Byte == 0x9d;
}

} // namespace

void ConvertToUtf8(TSettingsFile &File)
{
	if (EncodingOf(File) != TEncoding::Cp1252)
		return;
	constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";
	const std::string Old = File.Text();
	std::string_view Text = Old;
	std::string New;
	if (Text.starts_with(ByteOrderMark)) {
		New = ByteOrderMark;
		Text.remove_prefix(ByteOrderMark.size());
	}
	New += patella::text::Cp1252ToUtf8(Text);
	File = TSettingsFile::Parse(New, File.Newline());
}

std::string QuoteText(std::string_view Text, TEncoding Encoding)
{
	std::string Result = "\"";
	for (const char c : Text) {
		const auto Byte = static_cast<unsigned char>(c);
		switch (c) {
		case '\\': Result += "\\\\"; break;
		case '"': Result += "\\\""; break;
		case '\n': Result += "\\n"; break;
		case '\r': Result += "\\r"; break;
		case '\t': Result += "\\t"; break;
		default:
			if (Byte < 0x20 || Byte == 0x7f || (Encoding == TEncoding::Utf8 && Undefined(Byte))) {
				Result += "\\x";
				Result += HexDigits[Byte >> 4];
				Result += HexDigits[Byte & 15];
			} else if (Encoding == TEncoding::Utf8 && Byte >= 0x80) {
				Result += patella::text::Cp1252ToUtf8(std::string_view(&c, 1));
			} else {
				Result += c;
			}
		}
	}
	return Result + "\"";
}

std::optional<std::vector<TField>> ParseFields(std::string_view Value, TEncoding Encoding)
{
	std::vector<TField> Result;
	Value = Trim(Value);
	if (Value.empty())
		return Result;
	size_t Position = 0;
	for (;;) {
		while (Position < Value.size() && Blanks.find(Value[Position]) != std::string_view::npos)
			Position++;
		TField Field;
		if (Position < Value.size() && Value[Position] == '"') {
			Field.Kind = TKind::Text;
			Position++;
			// The characters since the last escape, in the file's encoding: converted at the next escape and at the
			// end (an escape is the client's byte, never part of the UTF-8).
			std::string Run;
			const auto Flush = [&] {
				Field.Text += Encoding == TEncoding::Utf8 ? patella::text::Utf8ToCp1252(Run) : Run;
				Run.clear();
			};
			for (;;) {
				if (Position >= Value.size())
					return std::nullopt; // no closing quote
				const char c = Value[Position++];
				if (c == '"') {
					Flush();
					break;
				}
				if (c != '\\') {
					Run += c;
					continue;
				}
				Flush();
				if (Position >= Value.size())
					return std::nullopt;
				const char e = Value[Position++];
				switch (e) {
				case '\\': Field.Text += '\\'; break;
				case '"': Field.Text += '"'; break;
				case 'n': Field.Text += '\n'; break;
				case 'r': Field.Text += '\r'; break;
				case 't': Field.Text += '\t'; break;
				case 'x': {
					const int High = Position < Value.size() ? HexValue(Value[Position]) : -1;
					const int Low = Position + 1 < Value.size() ? HexValue(Value[Position + 1]) : -1;
					if (High < 0 || Low < 0 || (High == 0 && Low == 0))
						return std::nullopt; // not two hex digits, or a NUL (the client's texts end there)
					Field.Text += static_cast<char>(High * 16 + Low);
					Position += 2;
					break;
				}
				default: return std::nullopt;
				}
			}
		} else {
			size_t End = Value.find(',', Position);
			if (End == std::string_view::npos)
				End = Value.size();
			const std::string_view Word = Trim(Value.substr(Position, End - Position));
			Position = End;
			if (SameText(Word, "yes") || SameText(Word, "no")) {
				Field.Kind = TKind::Flag;
				Field.Flag = SameText(Word, "yes");
			} else {
				const char *First = Word.data();
				const char *Last = Word.data() + Word.size();
				const std::from_chars_result Parsed = std::from_chars(First, Last, Field.Number);
				if (Word.empty() || Parsed.ec != std::errc() || Parsed.ptr != Last)
					return std::nullopt;
			}
		}
		Result.push_back(std::move(Field));
		while (Position < Value.size() && Blanks.find(Value[Position]) != std::string_view::npos)
			Position++;
		if (Position >= Value.size())
			return Result;
		if (Value[Position] != ',')
			return std::nullopt;
		Position++;
	}
}

bool HasPreferences(const TSettingsFile &File)
{
	return File.Get(PreferencesVersionKey).has_value();
}

void WritePreferences(TSettingsFile &File, const TPreferenceValues &Values)
{
	const auto Number = [](int Value) { return std::to_string(Value); };
	ConvertToUtf8(File);
	Put(File, PreferencesVersionKey, Number(PreferencesVersion));
	Put(File, "TestServer", YesNo(Values.TestServer));
	if (Values.FullscreenMode) {
		const std::array<int, 3> &Mode = *Values.FullscreenMode;
		Put(File, "FullscreenMode", Number(Mode[0]) + "," + Number(Mode[1]) + "," + Number(Mode[2]));
	} else {
		File.Remove("FullscreenMode");
	}
	Put(File, "WindowsMousepointer", YesNo(Values.WindowsMousepointer));
	Put(File, "LightEffects", YesNo(Values.LightEffects));
	Put(File, "TextualEffects", YesNo(Values.TextualEffects));
	Put(File, "CreatureInfo", Number(Values.CreatureInfo));
	Put(File, "NoStretching", YesNo(Values.NoStretching));
	Put(File, "ConfineFramerate", Number(Values.ConfineFramerate));
	std::vector<bool> Hotkeys(PreferenceHotkeys);
	for (int i = 0; i < PreferenceHotkeys; i++) {
		const auto Index = static_cast<size_t>(i);
		if (Values.Hotkeys[Index].empty())
			continue;
		Hotkeys[Index] = true;
		Put(File, "Hotkey" + Number(i), QuoteText(Values.Hotkeys[Index]) + "," + YesNo(Values.HotkeyAutoSend[Index]));
	}
	RemoveIndexed(File, "Hotkey", Hotkeys);
	const std::array<int, 4> &Window = Values.WindowedMode;
	Put(File, "WindowedMode", Number(Window[0]) + "," + Number(Window[1]) + "," + Number(Window[2]) + "," + Number(Window[3]) + "," + YesNo(Values.WindowedMaximized));
	Put(File, "ConsoleHeight", Number(Values.ConsoleHeight));
	Put(File, "AutomapZoom", Number(Values.AutomapZoom));
	std::vector<bool> Windows(Values.ContentWindows.size() + 1);
	for (size_t i = 0; i < Values.ContentWindows.size(); i++) {
		const TContentWindowValue &Entry = Values.ContentWindows[i];
		Windows[i + 1] = true;
		Put(File, "ContentWindow" + std::to_string(i + 1), Number(Entry.ID) + "," + Number(Entry.Size));
	}
	RemoveIndexed(File, "ContentWindow", Windows);
	const std::array<int, 3> &Sort = Values.BuddySortmethods;
	Put(File, "BuddySortmethods", Number(Sort[0]) + "," + Number(Sort[1]) + "," + Number(Sort[2]));
	Put(File, "BuddyHideOffline", YesNo(Values.BuddyHideOffline));
	Put(File, "OpenDialogs", Numbers(Values.OpenDialogs));
	Put(File, "LastMotD", Number(Values.LastMotD));
	Put(File, "InfoMessages", YesNo(Values.InfoMessages));
	Put(File, "EventMessages", YesNo(Values.EventMessages));
	Put(File, "StatusMessages", YesNo(Values.StatusMessages));
	Put(File, "TimeStamps", YesNo(Values.TimeStamps));
	Put(File, "PrivateMessages", YesNo(Values.PrivateMessages));
	Put(File, "OpenChannel", Numbers(Values.OpenChannels));
	Put(File, "ClassicControl", YesNo(Values.ClassicControl));
	Put(File, "AutoChaseOff", YesNo(Values.AutoChaseOff));
	Put(File, "AttackMode", Number(Values.AttackMode));
	Put(File, "ChaseMode", Number(Values.ChaseMode));
	Put(File, "SecureMode", YesNo(Values.SecureMode));
	std::vector<bool> Buddies(Values.Buddies.size() + 1);
	for (size_t i = 0; i < Values.Buddies.size(); i++) {
		const TBuddyValue &Entry = Values.Buddies[i];
		Buddies[i + 1] = true;
		Put(File, "Buddy" + std::to_string(i + 1), Number(Entry.CharacterID) + "," + Number(Entry.Icon) + "," + QuoteText(Entry.Description) + "," + YesNo(Entry.Notify));
	}
	RemoveIndexed(File, "Buddy", Buddies);
	Put(File, "White", Texts(Values.White));
	Put(File, "Ignore", Texts(Values.Ignore));
	Put(File, "KnownHints", Numbers(Values.KnownHints));
	Put(File, "ShowNewHints", YesNo(Values.ShowNewHints));
	Put(File, "LevelBar", YesNo(Values.LevelBar));
	Put(File, "MagLevelBar", YesNo(Values.MagLevelBar));
	std::vector<int> Skills;
	for (int i = 0; i < PreferenceSkills; i++) {
		if (Values.SkillBars[static_cast<size_t>(i)])
			Skills.push_back(i);
	}
	Put(File, "SkillBars", Numbers(Skills));
	Put(File, "Engine", Number(Values.Engine));
}

std::vector<std::string> ReadPreferences(const TSettingsFile &File, TPreferenceValues &Values)
{
	std::vector<std::string> Problems;
	if (const std::optional<std::string> Version = File.Get(PreferencesVersionKey); Version && *Version != "1" && *Version != "2")
		Problems.push_back(std::string(PreferencesVersionKey) + "=" + ClientText(File, *Version) + ": not 1 or 2; read as 2 (UTF-8)");
	TReader Read(File, Problems);
	Read.Flag("TestServer", Values.TestServer);
	if (const auto Fields = Read.Fields("FullscreenMode", "nnn"))
		Values.FullscreenMode = std::array<int, 3>{(*Fields)[0].Number, (*Fields)[1].Number, (*Fields)[2].Number};
	Read.Flag("WindowsMousepointer", Values.WindowsMousepointer);
	Read.Flag("LightEffects", Values.LightEffects);
	Read.Flag("TextualEffects", Values.TextualEffects);
	Read.Number("CreatureInfo", Values.CreatureInfo);
	Read.Flag("NoStretching", Values.NoStretching);
	Read.Number("ConfineFramerate", Values.ConfineFramerate);
	Read.Number("Engine", Values.Engine);
	for (const auto &[Index, Key] : Indexed(File, "Hotkey")) {
		if (Index >= PreferenceHotkeys) {
			Read.Problem(Key + ": no hotkey " + std::to_string(Index) + " (0 to 35); ignored");
			continue;
		}
		const auto Fields = Read.Fields(Key, "tf");
		if (!Fields)
			continue;
		// As the original's reader takes the text its writer wrote: a final line break (sent at once) marks
		// a hotkey sent at once, any other becomes a space (Preferences.cpp ReadTibiaCfg).
		std::string Text = (*Fields)[0].Text + ((*Fields)[1].Flag ? "\n" : "");
		if (Text.size() > PreferenceHotkeyLength)
			Text.resize(PreferenceHotkeyLength);
		bool AutoSend = false;
		if (!Text.empty() && Text.back() == '\n') {
			Text.pop_back();
			AutoSend = true;
		}
		std::replace(Text.begin(), Text.end(), '\n', ' ');
		const auto Slot = static_cast<size_t>(Index);
		Values.Hotkeys[Slot] = std::move(Text);
		Values.HotkeyAutoSend[Slot] = AutoSend;
	}
	if (const auto Fields = Read.Fields("WindowedMode", "nnnnf")) {
		for (size_t i = 0; i < 4; i++)
			Values.WindowedMode[i] = (*Fields)[i].Number;
		Values.WindowedMaximized = (*Fields)[4].Flag;
	}
	Read.Number("ConsoleHeight", Values.ConsoleHeight);
	Read.Number("AutomapZoom", Values.AutomapZoom);
	// The lists of numbered keys are the file's when it has one of their keys (the writer writes them all).
	if (const auto Entries = Indexed(File, "ContentWindow"); !Entries.empty()) {
		Values.ContentWindows.clear();
		for (const auto &Entry : Entries) {
			if (const auto Fields = Read.Fields(Entry.second, "nn"))
				Values.ContentWindows.push_back({(*Fields)[0].Number, (*Fields)[1].Number});
		}
	}
	Read.NumberArray("BuddySortmethods", Values.BuddySortmethods);
	Read.Flag("BuddyHideOffline", Values.BuddyHideOffline);
	std::vector<int> Dialogs = Values.OpenDialogs;
	Read.NumberList("OpenDialogs", Dialogs);
	Values.OpenDialogs.clear();
	for (const int Dialog : Dialogs) {
		// The original's reader adds a dialog once (its duplicate check).
		if (std::find(Values.OpenDialogs.begin(), Values.OpenDialogs.end(), Dialog) == Values.OpenDialogs.end())
			Values.OpenDialogs.push_back(Dialog);
	}
	Read.Number("LastMotD", Values.LastMotD);
	Read.Flag("InfoMessages", Values.InfoMessages);
	Read.Flag("EventMessages", Values.EventMessages);
	Read.Flag("StatusMessages", Values.StatusMessages);
	Read.Flag("TimeStamps", Values.TimeStamps);
	Read.Flag("PrivateMessages", Values.PrivateMessages);
	Read.NumberList("OpenChannel", Values.OpenChannels);
	Read.Flag("ClassicControl", Values.ClassicControl);
	Read.Flag("AutoChaseOff", Values.AutoChaseOff);
	Read.Number("AttackMode", Values.AttackMode);
	Read.Number("ChaseMode", Values.ChaseMode);
	if (Values.ChaseMode == 2)
		Values.ChaseMode = 0; // as the original's reader
	Read.Flag("SecureMode", Values.SecureMode);
	if (const auto Entries = Indexed(File, "Buddy"); !Entries.empty()) {
		Values.Buddies.clear();
		for (const auto &Entry : Entries) {
			if (const auto Fields = Read.Fields(Entry.second, "nntf"))
				Values.Buddies.push_back({(*Fields)[0].Number, (*Fields)[1].Number, (*Fields)[2].Text, (*Fields)[3].Flag});
		}
	}
	Read.TextList("White", Values.White);
	Read.TextList("Ignore", Values.Ignore);
	Read.NumberList("KnownHints", Values.KnownHints);
	Read.Flag("ShowNewHints", Values.ShowNewHints);
	Read.Flag("LevelBar", Values.LevelBar);
	Read.Flag("MagLevelBar", Values.MagLevelBar);
	if (const auto Fields = Read.Fields("SkillBars", "*n")) {
		std::array<bool, PreferenceSkills> Bars{};
		bool Valid = true;
		for (const TField &Field : *Fields)
			Valid &= Field.Number >= 0 && Field.Number < PreferenceSkills;
		if (Valid) {
			for (const TField &Field : *Fields)
				Bars[static_cast<size_t>(Field.Number)] = true;
			Values.SkillBars = Bars;
		} else {
			Read.Problem("SkillBars=" + ClientText(File, File.Get("SkillBars").value_or("")) + ": a skill not 0 to 6; kept as it was");
		}
	}
	return Problems;
}

} // namespace patella::settings
