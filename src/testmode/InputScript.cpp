// InputScript.cpp: the parser and scheduler of the test mode's input script (InputScript.h;
// issue #21).

#include "InputScript.h"

#include "Clock.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace patella::testmode {

namespace {

// Virtual times stay below this (11.5 days), far from the wrap of timeGetTime.
constexpr std::uint64_t MaxTime = 1000000000;
// Coordinates and wheel notches stay within this.
constexpr std::int64_t MaxCoordinate = 100000;

// Virtual-key codes (WinUser.h).
constexpr std::uint32_t VkBack = 0x08;
constexpr std::uint32_t VkTab = 0x09;
constexpr std::uint32_t VkReturn = 0x0d;
constexpr std::uint32_t VkShift = 0x10;
constexpr std::uint32_t VkControl = 0x11;
constexpr std::uint32_t VkMenu = 0x12;
constexpr std::uint32_t VkEscape = 0x1b;
constexpr std::uint32_t VkSpace = 0x20;

struct TNamedKey {
	std::string_view Name;
	TKeyInfo Info;
};

// clang-format off
constexpr std::array NamedKeys = {
	TNamedKey{"Back", {VkBack, 0x0e, false}},
	TNamedKey{"Backspace", {VkBack, 0x0e, false}},
	TNamedKey{"Tab", {VkTab, 0x0f, false}},
	TNamedKey{"Return", {VkReturn, 0x1c, false}},
	TNamedKey{"Enter", {VkReturn, 0x1c, false}},
	TNamedKey{"Shift", {VkShift, 0x2a, false}},
	TNamedKey{"Ctrl", {VkControl, 0x1d, false}},
	TNamedKey{"Control", {VkControl, 0x1d, false}},
	TNamedKey{"Alt", {VkMenu, 0x38, false}},
	TNamedKey{"Escape", {VkEscape, 0x01, false}},
	TNamedKey{"Space", {VkSpace, 0x39, false}},
	TNamedKey{"PageUp", {0x21, 0x49, true}},
	TNamedKey{"PageDown", {0x22, 0x51, true}},
	TNamedKey{"End", {0x23, 0x4f, true}},
	TNamedKey{"Home", {0x24, 0x47, true}},
	TNamedKey{"Left", {0x25, 0x4b, true}},
	TNamedKey{"Up", {0x26, 0x48, true}},
	TNamedKey{"Right", {0x27, 0x4d, true}},
	TNamedKey{"Down", {0x28, 0x50, true}},
	TNamedKey{"Insert", {0x2d, 0x52, true}},
	TNamedKey{"Delete", {0x2e, 0x53, true}},
	// The keypad's keys with Num Lock off: the same codes, not extended (MainFrm.cpp TranslateKey).
	TNamedKey{"NumPageUp", {0x21, 0x49, false}},
	TNamedKey{"NumPageDown", {0x22, 0x51, false}},
	TNamedKey{"NumEnd", {0x23, 0x4f, false}},
	TNamedKey{"NumHome", {0x24, 0x47, false}},
};

// Set-1 scan codes of A-Z and of 0-9.
constexpr std::array<std::uint8_t, 26> LetterScan = {
	0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
	0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
constexpr std::array<std::uint8_t, 10> DigitScan = {0x0b, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a};
// F1-F12.
constexpr std::array<std::uint8_t, 12> FunctionScan = {
	0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58};
// clang-format on
constexpr std::string_view ShiftedDigits = ")!@#$%^&*(";

char Lower(char c)
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool SameCaseless(std::string_view a, std::string_view b)
{
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return Lower(x) == Lower(y); });
}

bool IsSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

std::uint32_t ModifierOf(std::uint32_t Key)
{
	switch (Key) {
	case VkShift: return ModifierShift;
	case VkControl: return ModifierControl;
	case VkMenu: return ModifierAlt;
	default: return 0;
	}
}

// The words of a line up to a comment; a word starting with '"' runs to the closing quote.
struct TWord {
	std::string_view Text;
	std::size_t Offset = 0;
};

class TParser {
public:
	TParseResult Run(std::string_view Text);

private:
	bool Fail(std::string Message);
	bool Line(std::string_view Text);
	bool Split(std::string_view Text, std::vector<TWord> *Words);
	bool Number(std::string_view Word, std::int64_t Min, std::int64_t Max, std::int64_t *Value);
	bool Coordinate(std::string_view Word, char Anchor, TCoordinate *Value);
	bool Combo(std::string_view Word, std::vector<TKeyInfo> *Held, TKeyInfo *Key);
	bool Mods(const std::vector<TWord> &Words, std::size_t Index, std::vector<TKeyInfo> *Held);
	bool String(std::string_view Word, std::string *Value);
	bool Expect(const std::vector<TWord> &Words, std::size_t Min, std::size_t Max);

	void Add(TEventKind Kind);
	void AddKey(TEventKind Kind, const TKeyInfo &Key);
	void AddChar(std::uint32_t Code, std::uint32_t Scan, bool Extended);
	void AddMouse(TEventKind Kind);
	// Presses the modifiers that are not held yet; returns them, to be released by Release.
	std::vector<TKeyInfo> Press(const std::vector<TKeyInfo> &Wanted);
	void Release(const std::vector<TKeyInfo> &Pressed);

	TParseResult Result;
	int LineNumber = 0;
	std::uint64_t Time = 0;
	std::uint32_t Modifiers = 0;
	std::uint32_t Buttons = 0;
	TCoordinate X, Y;
};

bool TParser::Fail(std::string Message)
{
	Result.Ok = false;
	Result.ErrorLine = LineNumber;
	Result.Error = std::move(Message);
	return false;
}

TParseResult TParser::Run(std::string_view Text)
{
	if (Text.starts_with("\xef\xbb\xbf"))
		Text.remove_prefix(3);
	Result.Ok = true;
	while (!Text.empty()) {
		LineNumber++;
		std::size_t End = Text.find('\n');
		std::string_view Current = Text.substr(0, End);
		Text = End == std::string_view::npos ? std::string_view() : Text.substr(End + 1);
		if (!Line(Current))
			break;
	}
	return std::move(Result);
}

bool TParser::Split(std::string_view Text, std::vector<TWord> *Words)
{
	std::size_t i = 0;
	while (i < Text.size()) {
		if (IsSpace(Text[i])) {
			i++;
			continue;
		}
		if (Text[i] == '#')
			break;
		std::size_t Start = i;
		if (Text[i] == '"') {
			i++;
			while (i < Text.size() && Text[i] != '"') {
				if (Text[i] == '\\' && i + 1 < Text.size())
					i++;
				i++;
			}
			if (i >= Text.size())
				return Fail("the string has no closing quote");
			i++;
		} else {
			while (i < Text.size() && !IsSpace(Text[i]) && Text[i] != '#')
				i++;
		}
		Words->push_back(TWord{Text.substr(Start, i - Start), Start});
	}
	return true;
}

bool TParser::Expect(const std::vector<TWord> &Words, std::size_t Min, std::size_t Max)
{
	std::size_t Arguments = Words.size() - 1;
	if (Arguments < Min || Arguments > Max) {
		std::string Message(Words[0].Text);
		if (Min == Max)
			Message += " takes " + std::to_string(Min) + " argument" + (Min == 1 ? "" : "s");
		else
			Message += " takes " + std::to_string(Min) + " to " + std::to_string(Max) + " arguments";
		return Fail(Message + ", not " + std::to_string(Arguments));
	}
	return true;
}

bool TParser::Number(std::string_view Word, std::int64_t Min, std::int64_t Max, std::int64_t *Value)
{
	std::int64_t Parsed = 0;
	const char *First = Word.data();
	const char *Last = Word.data() + Word.size();
	auto [End, Error] = std::from_chars(First, Last, Parsed);
	if (Error != std::errc() || End != Last || Word.empty())
		return Fail("not a number: " + std::string(Word));
	if (Parsed < Min || Parsed > Max)
		return Fail(std::string(Word) + " is not between " + std::to_string(Min) + " and " + std::to_string(Max));
	*Value = Parsed;
	return true;
}

bool TParser::Coordinate(std::string_view Word, char Anchor, TCoordinate *Value)
{
	TCoordinate Parsed;
	if (Word.size() >= 2 && Lower(Word[0]) == Lower(Anchor) && Word[1] == '-') {
		Parsed.FromEnd = true;
		Word.remove_prefix(2);
	}
	std::int64_t Pixels = 0;
	if (!Number(Word, -MaxCoordinate, MaxCoordinate, &Pixels))
		return false;
	Parsed.Value = static_cast<int>(Pixels);
	*Value = Parsed;
	return true;
}

bool TParser::Combo(std::string_view Word, std::vector<TKeyInfo> *Held, TKeyInfo *Key)
{
	while (true) {
		std::size_t Plus = Word.find('+');
		std::string_view Name = Word.substr(0, Plus);
		TKeyInfo Info;
		if (!FindKey(Name, &Info))
			return Fail("unknown key: " + std::string(Name));
		if (Plus == std::string_view::npos) {
			*Key = Info;
			return true;
		}
		if (ModifierOf(Info.Key) == 0)
			return Fail("not a modifier (Shift, Ctrl, Alt): " + std::string(Name));
		Held->push_back(Info);
		Word.remove_prefix(Plus + 1);
	}
}

bool TParser::Mods(const std::vector<TWord> &Words, std::size_t Index, std::vector<TKeyInfo> *Held)
{
	if (Index >= Words.size())
		return true;
	// Modifiers only: Shift, Ctrl+Shift.
	std::string_view Word = Words[Index].Text;
	while (true) {
		std::size_t Plus = Word.find('+');
		std::string_view Name = Word.substr(0, Plus);
		TKeyInfo Info;
		if (!FindKey(Name, &Info) || ModifierOf(Info.Key) == 0)
			return Fail("not a modifier (Shift, Ctrl, Alt): " + std::string(Name));
		Held->push_back(Info);
		if (Plus == std::string_view::npos)
			return true;
		Word.remove_prefix(Plus + 1);
	}
}

bool TParser::String(std::string_view Word, std::string *Value)
{
	if (Word.size() < 2 || Word.front() != '"' || Word.back() != '"')
		return Fail("text takes a string in double quotes");
	Word = Word.substr(1, Word.size() - 2);
	std::string Latin1;
	std::size_t i = 0;
	while (i < Word.size()) {
		auto Byte = static_cast<unsigned char>(Word[i]);
		if (Byte == '\\') {
			if (i + 1 >= Word.size())
				return Fail("the string ends with a backslash");
			char Escaped = Word[i + 1];
			if (Escaped == '"' || Escaped == '\\') {
				Latin1 += Escaped;
				i += 2;
				continue;
			}
			if (Escaped != 'x')
				return Fail("unknown escape: \\" + std::string(1, Escaped));
			// \xHH: the character HH itself.
			unsigned int Code = 0;
			const char *Digits = Word.data() + i + 2;
			const char *DigitsEnd = Word.data() + std::min(i + 4, Word.size());
			auto [End, Error] = std::from_chars(Digits, DigitsEnd, Code, 16);
			if (Error != std::errc() || End != Digits + 2)
				return Fail("\\x takes two hexadecimal digits");
			Latin1 += static_cast<char>(Code);
			i += 4;
		} else if (Byte < 0x80) {
			Latin1 += static_cast<char>(Byte);
			i++;
		} else if ((Byte & 0xe0U) == 0xc0U && i + 1 < Word.size()
			&& (static_cast<unsigned char>(Word[i + 1]) & 0xc0U) == 0x80U) {
			unsigned int Code = ((Byte & 0x1fU) << 6U) | (static_cast<unsigned char>(Word[i + 1]) & 0x3fU);
			if (Code < 0x80 || Code > 0xff)
				return Fail("text holds a character that is not Latin-1");
			Latin1 += static_cast<char>(Code);
			i += 2;
		} else {
			return Fail("text holds a character that is not Latin-1 (or not UTF-8)");
		}
	}
	*Value = std::move(Latin1);
	return true;
}

void TParser::Add(TEventKind Kind)
{
	TInputEvent Event;
	Event.Time = static_cast<std::uint32_t>(Time);
	Event.Kind = Kind;
	Event.Modifiers = Modifiers;
	Event.Buttons = Buttons;
	Event.X = X;
	Event.Y = Y;
	Event.Line = LineNumber;
	Result.Script.Events.push_back(Event);
}

void TParser::AddKey(TEventKind Kind, const TKeyInfo &Key)
{
	std::uint32_t Modifier = ModifierOf(Key.Key);
	if (Kind == TEventKind::KeyDown)
		Modifiers |= Modifier;
	else
		Modifiers &= ~Modifier;
	Add(Kind);
	TInputEvent &Event = Result.Script.Events.back();
	Event.Key = Key.Key;
	Event.Scan = Key.Scan;
	Event.Extended = Key.Extended;
}

void TParser::AddChar(std::uint32_t Code, std::uint32_t Scan, bool Extended)
{
	Add(TEventKind::Char);
	TInputEvent &Event = Result.Script.Events.back();
	Event.Key = Code;
	Event.Scan = Scan;
	Event.Extended = Extended;
}

void TParser::AddMouse(TEventKind Kind)
{
	switch (Kind) {
	case TEventKind::LeftDown:
	case TEventKind::LeftDouble: Buttons |= ButtonLeft; break;
	case TEventKind::LeftUp: Buttons &= ~ButtonLeft; break;
	case TEventKind::RightDown:
	case TEventKind::RightDouble: Buttons |= ButtonRight; break;
	case TEventKind::RightUp: Buttons &= ~ButtonRight; break;
	default: break;
	}
	Add(Kind);
}

std::vector<TKeyInfo> TParser::Press(const std::vector<TKeyInfo> &Wanted)
{
	std::vector<TKeyInfo> Pressed;
	for (const TKeyInfo &Key : Wanted) {
		if ((Modifiers & ModifierOf(Key.Key)) != 0)
			continue;
		AddKey(TEventKind::KeyDown, Key);
		Pressed.push_back(Key);
	}
	return Pressed;
}

void TParser::Release(const std::vector<TKeyInfo> &Pressed)
{
	for (auto Key = Pressed.rbegin(); Key != Pressed.rend(); ++Key)
		AddKey(TEventKind::KeyUp, *Key);
}

bool TParser::Line(std::string_view Text)
{
	std::vector<TWord> Words;
	if (!Split(Text, &Words))
		return false;
	if (Words.empty())
		return true;
	std::string Command(Words[0].Text);
	std::transform(Command.begin(), Command.end(), Command.begin(), Lower);

	if (Command == "pace") {
		std::int64_t Value = 0;
		if (!Expect(Words, 1, 1) || !Number(Words[1].Text, 0, 100000, &Value))
			return false;
		if (!Result.Script.Events.empty())
			return Fail("pace comes before the first event");
		Result.Script.RealPerTick = static_cast<std::uint32_t>(Value);
		return true;
	}
	if (Command == "wait" || Command == "at") {
		std::int64_t Value = 0;
		if (!Expect(Words, 1, 1) || !Number(Words[1].Text, 0, static_cast<std::int64_t>(MaxTime), &Value))
			return false;
		std::uint64_t Target = Command == "wait" ? Time + static_cast<std::uint64_t>(Value) : static_cast<std::uint64_t>(Value);
		if (Target < Time)
			return Fail("at " + std::to_string(Value) + " is earlier than the script's time " + std::to_string(Time));
		if (Target > MaxTime)
			return Fail("the script's time passes " + std::to_string(MaxTime) + " ms");
		Time = Target;
		return true;
	}
	if (Command == "key") {
		std::vector<TKeyInfo> Wanted;
		TKeyInfo Key;
		if (!Expect(Words, 1, 1) || !Combo(Words[1].Text, &Wanted, &Key))
			return false;
		std::vector<TKeyInfo> Pressed = Press(Wanted);
		AddKey(TEventKind::KeyDown, Key);
		int Character = KeyCharacter(Key.Key, Modifiers);
		if (Character >= 0)
			AddChar(static_cast<std::uint32_t>(Character), Key.Scan, Key.Extended);
		AddKey(TEventKind::KeyUp, Key);
		Release(Pressed);
		return true;
	}
	if (Command == "keydown" || Command == "keyup") {
		TKeyInfo Key;
		if (!Expect(Words, 1, 1))
			return false;
		if (!FindKey(Words[1].Text, &Key))
			return Fail("unknown key: " + std::string(Words[1].Text));
		AddKey(Command == "keydown" ? TEventKind::KeyDown : TEventKind::KeyUp, Key);
		return true;
	}
	if (Command == "char") {
		std::int64_t Value = 0;
		if (!Expect(Words, 1, 1) || !Number(Words[1].Text, 0, 255, &Value))
			return false;
		AddChar(static_cast<std::uint32_t>(Value), 0, false);
		return true;
	}
	if (Command == "text") {
		std::string Value;
		if (!Expect(Words, 1, 1) || !String(Words[1].Text, &Value))
			return false;
		for (char c : Value)
			AddChar(static_cast<unsigned char>(c), 0, false);
		return true;
	}
	if (Command == "move") {
		if (!Expect(Words, 2, 2) || !Coordinate(Words[1].Text, 'R', &X) || !Coordinate(Words[2].Text, 'B', &Y))
			return false;
		AddMouse(TEventKind::MouseMove);
		return true;
	}

	struct TButtonCommand {
		std::string_view Name;
		std::array<TEventKind, 4> Kinds;
		std::size_t Count;
	};
	// clang-format off
	static constexpr std::array ButtonCommands = {
		TButtonCommand{"click", {TEventKind::LeftDown, TEventKind::LeftUp}, 2},
		TButtonCommand{"rclick", {TEventKind::RightDown, TEventKind::RightUp}, 2},
		TButtonCommand{"dclick", {TEventKind::LeftDown, TEventKind::LeftUp, TEventKind::LeftDouble, TEventKind::LeftUp}, 4},
		TButtonCommand{"rdclick", {TEventKind::RightDown, TEventKind::RightUp, TEventKind::RightDouble, TEventKind::RightUp}, 4},
		TButtonCommand{"ldown", {TEventKind::LeftDown}, 1},
		TButtonCommand{"lup", {TEventKind::LeftUp}, 1},
		TButtonCommand{"rdown", {TEventKind::RightDown}, 1},
		TButtonCommand{"rup", {TEventKind::RightUp}, 1},
	};
	// clang-format on
	for (const TButtonCommand &Button : ButtonCommands) {
		if (Command != Button.Name)
			continue;
		std::vector<TKeyInfo> Wanted;
		if (!Expect(Words, 2, 3) || !Coordinate(Words[1].Text, 'R', &X) || !Coordinate(Words[2].Text, 'B', &Y)
			|| !Mods(Words, 3, &Wanted))
			return false;
		std::vector<TKeyInfo> Pressed = Press(Wanted);
		AddMouse(TEventKind::MouseMove);
		for (std::size_t i = 0; i < Button.Count; i++)
			AddMouse(Button.Kinds.at(i));
		Release(Pressed);
		return true;
	}

	if (Command == "wheel") {
		std::int64_t Value = 0;
		if (!Expect(Words, 1, 1) || !Number(Words[1].Text, -200, 200, &Value))
			return false;
		if (Value == 0)
			return Fail("wheel takes a number of notches other than 0");
		Add(TEventKind::Wheel);
		Result.Script.Events.back().Delta = static_cast<int>(Value);
		return true;
	}
	if (Command == "quit") {
		if (!Expect(Words, 0, 0))
			return false;
		Add(TEventKind::Quit);
		return true;
	}
	if (Command == "dump" || Command == "frame") {
		if (!Expect(Words, 0, 0))
			return false;
		Add(Command == "dump" ? TEventKind::StateDump : TEventKind::FrameDump);
		return true;
	}
	return Fail("unknown command: " + std::string(Words[0].Text));
}

} // namespace

bool FindKey(std::string_view Name, TKeyInfo *Info)
{
	if (Name.size() == 1) {
		char c = Lower(Name[0]);
		if (c >= 'a' && c <= 'z') {
			auto Index = static_cast<std::size_t>(c - 'a');
			*Info = TKeyInfo{static_cast<std::uint32_t>('A' + Index), LetterScan.at(Index), false};
			return true;
		}
		if (c >= '0' && c <= '9') {
			auto Index = static_cast<std::size_t>(c - '0');
			*Info = TKeyInfo{static_cast<std::uint32_t>('0' + Index), DigitScan.at(Index), false};
			return true;
		}
	}
	if (Name.size() >= 2 && Lower(Name[0]) == 'f') {
		std::uint32_t Number = 0;
		auto [End, Error] = std::from_chars(Name.data() + 1, Name.data() + Name.size(), Number);
		if (Error == std::errc() && End == Name.data() + Name.size() && Number >= 1 && Number <= 12) {
			*Info = TKeyInfo{0x70 + Number - 1, FunctionScan.at(Number - 1), false};
			return true;
		}
	}
	for (const TNamedKey &Key : NamedKeys) {
		if (SameCaseless(Name, Key.Name)) {
			*Info = Key.Info;
			return true;
		}
	}
	return false;
}

int KeyCharacter(std::uint32_t Key, std::uint32_t Modifiers)
{
	if ((Modifiers & ModifierAlt) != 0)
		return -1;
	bool Shift = (Modifiers & ModifierShift) != 0;
	bool Control = (Modifiers & ModifierControl) != 0;
	if (Key >= 'A' && Key <= 'Z') {
		if (Control)
			return static_cast<int>(Key - 'A' + 1);
		return static_cast<int>(Shift ? Key : Key - 'A' + 'a');
	}
	if (Key >= '0' && Key <= '9') {
		if (Control)
			return -1;
		return Shift ? ShiftedDigits.at(Key - '0') : static_cast<int>(Key);
	}
	switch (Key) {
	case VkSpace: return ' ';
	case VkReturn: return Control ? 10 : 13;
	case VkEscape: return 27;
	case VkTab: return Control ? -1 : 9;
	case VkBack: return Control ? 127 : 8;
	default: return -1;
	}
}

std::string Describe(const TInputEvent &Event)
{
	auto Hex = [](std::uint32_t Value) {
		std::array<char, 8> Digits{};
		auto [End, Error] = std::to_chars(Digits.data(), Digits.data() + Digits.size(), Value, 16);
		std::string Text = Error == std::errc() ? std::string(Digits.data(), End) : std::string("?");
		return (Text.size() < 2 ? "0x0" : "0x") + Text;
	};
	auto Point = [&Event] {
		return std::string(Event.X.FromEnd ? "R-" : "") + std::to_string(Event.X.Value) + " "
			+ (Event.Y.FromEnd ? "B-" : "") + std::to_string(Event.Y.Value);
	};
	std::string Text = std::to_string(Event.Time) + " ms, line " + std::to_string(Event.Line) + ": ";
	switch (Event.Kind) {
	case TEventKind::KeyDown:
	case TEventKind::KeyUp:
		Text += Event.Kind == TEventKind::KeyDown ? "key down " : "key up ";
		Text += Hex(Event.Key) + " scan " + Hex(Event.Scan) + (Event.Extended ? " extended" : "");
		break;
	case TEventKind::Char: Text += "char " + std::to_string(Event.Key); break;
	case TEventKind::MouseMove: Text += "move " + Point(); break;
	case TEventKind::LeftDown: Text += "left down " + Point(); break;
	case TEventKind::LeftUp: Text += "left up " + Point(); break;
	case TEventKind::LeftDouble: Text += "left double " + Point(); break;
	case TEventKind::RightDown: Text += "right down " + Point(); break;
	case TEventKind::RightUp: Text += "right up " + Point(); break;
	case TEventKind::RightDouble: Text += "right double " + Point(); break;
	case TEventKind::Wheel: Text += "wheel " + std::to_string(Event.Delta); break;
	case TEventKind::Quit: Text += "quit"; break;
	case TEventKind::StateDump: Text += "dump"; break;
	case TEventKind::FrameDump: Text += "frame"; break;
	}
	if (Event.Modifiers != 0)
		Text += " modifiers " + std::to_string(Event.Modifiers);
	if (Event.Buttons != 0)
		Text += " buttons " + std::to_string(Event.Buttons);
	return Text;
}

TParseResult ParseScript(std::string_view Text)
{
	TParser Parser;
	return Parser.Run(Text);
}

const TInputEvent *TScheduler::TakeDue(std::uint32_t Elapsed)
{
	if (Finished() || Script.Events[Next].Time > Elapsed)
		return nullptr;
	return &Script.Events[Next++];
}

std::uint32_t TScheduler::NextStop(std::uint32_t Elapsed) const
{
	std::uint64_t Tick = (static_cast<std::uint64_t>(Elapsed) / TickMilliseconds + 1) * TickMilliseconds;
	std::uint64_t Stop = Tick;
	if (!Finished())
		Stop = std::min<std::uint64_t>(Stop, std::max(Script.Events[Next].Time, Elapsed));
	return static_cast<std::uint32_t>(std::min<std::uint64_t>(Stop, UINT32_MAX));
}

std::uint64_t TScheduler::RealDeadline(std::uint32_t Elapsed) const
{
	return static_cast<std::uint64_t>(Elapsed) * Script.RealPerTick / TickMilliseconds;
}

bool TScheduler::IsTick(std::uint32_t Elapsed)
{
	return Elapsed != 0 && Elapsed % TickMilliseconds == 0;
}

} // namespace patella::testmode
