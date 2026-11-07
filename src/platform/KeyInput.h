// KeyInput.h: the client's keyboard and mouse rules, apart from the platform's events (Patella-Modern issue #123,
// plan M8; notes/decisions.md, "#123: input over SDL3"). Not part of the original source. Portable: no platform header.
//
// CMainFrame's input handlers (MainFrm.cpp) read Windows' virtual keys and messages: TranslateKey's table of the keys
// the client tells apart, the modifier flags KeyDown and KeyUp keep, OnChar's characters, the double clicks Windows
// makes (CS_DBLCLKS) and the wheel's notches. The SDL3 window (SdlFrame.cpp) describes each key as a TKeyEvent and
// leaves the rules to these classes, so the client gets the calls the original's handlers made. On Windows its keys,
// characters and buttons are still the window's messages (SDL's events differ from them: notes/decisions.md, "#123"),
// so TranslateKey, TKeyFlags and TWheel are used there; KeyCharacter and TClickCounter are for a platform where SDL's
// events (scan codes, printable UTF-8 text, click counts) are the only source:
//
//	TKeyEvent       a key: which of the keys the client tells apart (TKey), its letter in the active layout, the
//	                platform's NumLock, Shift, Control and Alt state at the key
//	TranslateKey    CMainFrame::TranslateKey: the client's key code of a key, or 0
//	KeyCharacter    the character Windows' TranslateMessage makes of a key that the text input does not deliver: the
//	                control characters (Return 13, Escape 27, Tab 9, Back 8; with Control the letters 1-26, Return 10,
//	                Back 127), or -1
//	TKeyFlags       CMainFrame::KeyFlags with KeyDown, KeyUp and OnChar: what each key and character gives the client
//	TClickCounter   the double clicks Windows makes from SDL's click counts
//	TWheel          CMainFrame::OnMouseWheel's notches (120 per notch, the rest kept for the next turn)

#pragma once

#include <cstdint>

namespace patella::platform {

// The client's modifier flags (MainFrm.h's KEY_SHIFT, KEY_CONTROL, KEY_ALT; SdlFrame.cpp checks they are the same).
inline constexpr int KeyFlagShift = 0x200;
inline constexpr int KeyFlagControl = 0x400;
inline constexpr int KeyFlagAlt = 0x800;

// The keys the client tells apart: the virtual keys of CMainFrame::TranslateKey and the keys TranslateMessage makes
// control characters of. The keypad's navigation keys are their own keys (Windows gives them the navigation keys'
// virtual keys without the extended flag while NumLock is off, the digits' while it is on). Every other key is Other.
enum class TKey : std::uint8_t {
	Other,
	Shift,
	Control,
	Alt,
	PageUp,
	PageDown,
	End,
	Home,
	Left,
	Up,
	Right,
	Down,
	Insert,
	Delete,
	PadPageUp,   // keypad 9
	PadPageDown, // keypad 3
	PadEnd,      // keypad 1
	PadHome,     // keypad 7
	PadLeft,     // keypad 4
	PadUp,       // keypad 8
	PadRight,    // keypad 6
	PadDown,     // keypad 2
	PadInsert,   // keypad 0
	PadDelete,   // keypad decimal point
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	Return,
	PadEnter,
	Escape,
	Tab,
	Backspace,
};

struct TKeyEvent {
	TKey Key = TKey::Other;
	// The key's letter in the active layout, 'a'-'z' (Windows' virtual keys A-Z, which are Latin letters in every
	// layout), or 0.
	char Letter = 0;
	// The platform's state at the key (Windows' keyboard state, which TranslateMessage reads).
	bool NumLock = false;
	bool Shift = false;
	bool Control = false;
	bool Alt = false;
};

// A keypad key acts as its navigation key where Windows gives it the navigation key's virtual key: NumLock off, or
// on with Shift held (Windows then also sends a Shift up and down around it, which SDL does not pass on for the left
// Shift: notes/decisions.md, "#123").
[[nodiscard]] constexpr bool PadNavigates(const TKeyEvent &Key) noexcept
{
	return !Key.NumLock || Key.Shift;
}

// CMainFrame::TranslateKey: Shift, Control and Alt give their flags, the navigation keys and F1-F12 their codes
// (the keypad's 0x116-0x119 where the dedicated keys have 0x112-0x115), Return with Alt held (in the client's flags)
// 13; every other key 0.
[[nodiscard]] constexpr int TranslateKey(const TKeyEvent &Key, int KeyFlags) noexcept
{
	const bool Pad = PadNavigates(Key);
	switch (Key.Key) {
	case TKey::Shift: return KeyFlagShift;
	case TKey::Control: return KeyFlagControl;
	case TKey::Alt: return KeyFlagAlt;
	case TKey::PageUp: return 0x114;
	case TKey::PageDown: return 0x115;
	case TKey::End: return 0x113;
	case TKey::Home: return 0x112;
	case TKey::Left: return 0x10d;
	case TKey::Up: return 0x10c;
	case TKey::Right: return 0x10e;
	case TKey::Down: return 0x10f;
	case TKey::Insert: return 0x110;
	case TKey::Delete: return 0x111;
	case TKey::PadPageUp: return Pad ? 0x119 : 0;
	case TKey::PadPageDown: return Pad ? 0x117 : 0;
	case TKey::PadEnd: return Pad ? 0x116 : 0;
	case TKey::PadHome: return Pad ? 0x118 : 0;
	case TKey::PadLeft: return Pad ? 0x10d : 0;
	case TKey::PadUp: return Pad ? 0x10c : 0;
	case TKey::PadRight: return Pad ? 0x10e : 0;
	case TKey::PadDown: return Pad ? 0x10f : 0;
	case TKey::PadInsert: return Pad ? 0x110 : 0;
	case TKey::PadDelete: return Pad ? 0x111 : 0;
	case TKey::Return:
	case TKey::PadEnter: return (KeyFlags & KeyFlagAlt) != 0 ? 13 : 0; // VK_RETURN
	default: break;
	}
	if (Key.Key >= TKey::F1 && Key.Key <= TKey::F12)
		return 0x100 + (static_cast<int>(Key.Key) - static_cast<int>(TKey::F1)); // VK_F1 (0x70) + 0x90
	return 0;
}

// The control character TranslateMessage makes of a key (WM_CHAR), which SDL's text input leaves out (it delivers
// printable text only): without Control and Alt Return (either) 13, Escape 27, Tab 9, Back 8; with Control and without
// Alt a letter 1-26, Return 10, Back 127. With Alt none (WM_SYSCHAR, which the client does not handle; Control with
// Alt is AltGr, whose characters are text). -1 where the key makes no control character.
[[nodiscard]] constexpr int KeyCharacter(const TKeyEvent &Key) noexcept
{
	if (Key.Alt)
		return -1;
	if (!Key.Control) {
		switch (Key.Key) {
		case TKey::Return:
		case TKey::PadEnter: return 13;
		case TKey::Escape: return 27;
		case TKey::Tab: return 9;
		case TKey::Backspace: return 8;
		default: return -1;
		}
	}
	if (Key.Letter >= 'a' && Key.Letter <= 'z')
		return Key.Letter - 'a' + 1;
	switch (Key.Key) {
	case TKey::Return:
	case TKey::PadEnter: return 10;
	case TKey::Backspace: return 127;
	default: return -1;
	}
}

// What a key down asks of the client: nothing, KeyPress(Code), or the close (Alt+F4: CMainFrame::OnClose).
struct TKeyAction {
	enum class TKind : std::uint8_t { None,
		Press,
		Close };
	TKind Kind = TKind::None;
	int Code = 0;
};

// CMainFrame::KeyFlags with KeyDown, KeyUp, OnChar and OnActivateApp's reset.
class TKeyFlags {
public:
	[[nodiscard]] int Flags() const noexcept { return Value; }
	// OnActivateApp: the held modifiers forgotten.
	void Reset() noexcept { Value = 0; }

	// CMainFrame::KeyDown: a modifier is remembered and pressed with its flag, Alt+F4 closes, F4 without Alt and the
	// other translated keys are pressed with the flags, the rest is nothing.
	[[nodiscard]] TKeyAction Down(const TKeyEvent &Key) noexcept
	{
		const int Code = TranslateKey(Key, Value);
		if (Code == KeyFlagShift || Code == KeyFlagControl || Code == KeyFlagAlt) {
			Value |= Code;
		} else if (Code == 0x103) {
			if ((Value & KeyFlagAlt) != 0)
				return TKeyAction{TKeyAction::TKind::Close, 0};
		} else if (Code < 1) {
			return TKeyAction{};
		}
		return TKeyAction{TKeyAction::TKind::Press, Value | Code};
	}

	// CMainFrame::KeyUp: a modifier's flag cleared.
	void Up(const TKeyEvent &Key) noexcept
	{
		const int Code = TranslateKey(Key, Value);
		if (Code == KeyFlagShift || Code == KeyFlagControl || Code == KeyFlagAlt)
			Value &= ~Code;
	}

	// CMainFrame::OnChar: the code to press for a character of the client's code page (0-255; a larger value is
	// dropped), or -1. With Control (and not Alt) only 1-26, as 'a'-'z'.
	[[nodiscard]] int Character(unsigned int Char) const noexcept
	{
		if ((Value & KeyFlagControl) != 0 && (Value & KeyFlagAlt) == 0) {
			if (Char == 0 || Char > 26)
				return -1;
			Char += 0x60;
		} else if (Char > 255) {
			return -1;
		}
		return Value | static_cast<int>(Char);
	}

private:
	int Value = 0;
};

// The double clicks of the client's window (CS_DBLCLKS): Windows sends a button's double click message for a press
// that comes within the double-click time and distance of the press before it, of the same button, when that press
// was not a double click itself (so presses alternate: down, double click, down, ...). SDL counts the presses of each
// button within the time and distance (1, 2, 3, ...) and does not reset a button's count when another button is
// pressed in between, as Windows does. IsDouble takes SDL's count of a press and gives Windows' answer.
class TClickCounter {
public:
	// Buttons are SDL's numbers (1 left, 2 middle, 3 right, 4, 5 the side buttons); others count as one button.
	[[nodiscard]] bool IsDouble(int Button, int Clicks) noexcept
	{
		const int Index = Button >= 1 && Button <= 5 ? Button - 1 : 5;
		if (Clicks <= 1 || Index != LastButton)
			Base[Index] = Clicks - 1;
		LastButton = Index;
		const int Count = Clicks - Base[Index];
		return Count >= 2 && Count % 2 == 0;
	}

private:
	int Base[6] = {};
	int LastButton = -1;
};

// CMainFrame::OnMouseWheel's notches: a turn's distance (120 a notch; high-resolution wheels send less) is added to
// the rest of the last turns; the result is the signed number of whole notches (positive up), and the rest (with the
// same sign, C++'s %) is kept.
class TWheel {
public:
	[[nodiscard]] int Notches(int Distance) noexcept
	{
		const int Delta = Rest + Distance;
		Rest = Delta % 120;
		return Delta / 120;
	}

private:
	int Rest = 0;
};

} // namespace patella::platform
