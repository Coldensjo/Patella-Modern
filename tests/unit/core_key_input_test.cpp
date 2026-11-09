// The client's keyboard and mouse rules over SDL3's events (src/platform/KeyInput.h, issue #123): CMainFrame's
// TranslateKey, KeyDown, KeyUp and OnChar, the control characters of TranslateMessage, Windows' double clicks from
// SDL's click counts, the wheel's notches; and the cursor resources' decoder (src/platform/CursorImage.h).

#include <doctest/doctest.h>

#include "platform/CursorImage.h"
#include "platform/WindowIcon.h"
#include "platform/KeyInput.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using patella::platform::DecodeIconImage;
using patella::platform::DecodeMonochromeCursor;
using patella::platform::KeyCharacter;
using patella::platform::KeyFlagAlt;
using patella::platform::KeyFlagControl;
using patella::platform::KeyFlagShift;
using patella::platform::TClickCounter;
using patella::platform::TKey;
using patella::platform::TKeyAction;
using patella::platform::TKeyEvent;
using patella::platform::TKeyFlags;
using patella::platform::TranslateKey;
using patella::platform::TWheel;

TKeyEvent Key(TKey Which)
{
	TKeyEvent Event;
	Event.Key = Which;
	return Event;
}

TKeyEvent Letter(char Value, bool Control = false, bool Alt = false)
{
	TKeyEvent Event;
	Event.Letter = Value;
	Event.Control = Control;
	Event.Alt = Alt;
	return Event;
}

} // namespace

TEST_CASE("Keys: TranslateKey's table")
{
	CHECK(TranslateKey(Key(TKey::Shift), 0) == KeyFlagShift);
	CHECK(TranslateKey(Key(TKey::Control), 0) == KeyFlagControl);
	CHECK(TranslateKey(Key(TKey::Alt), 0) == KeyFlagAlt);
	CHECK(TranslateKey(Key(TKey::PageUp), 0) == 0x114);
	CHECK(TranslateKey(Key(TKey::PageDown), 0) == 0x115);
	CHECK(TranslateKey(Key(TKey::End), 0) == 0x113);
	CHECK(TranslateKey(Key(TKey::Home), 0) == 0x112);
	CHECK(TranslateKey(Key(TKey::Left), 0) == 0x10d);
	CHECK(TranslateKey(Key(TKey::Up), 0) == 0x10c);
	CHECK(TranslateKey(Key(TKey::Right), 0) == 0x10e);
	CHECK(TranslateKey(Key(TKey::Down), 0) == 0x10f);
	CHECK(TranslateKey(Key(TKey::Insert), 0) == 0x110);
	CHECK(TranslateKey(Key(TKey::Delete), 0) == 0x111);
	CHECK(TranslateKey(Key(TKey::F1), 0) == 0x100);
	CHECK(TranslateKey(Key(TKey::F4), 0) == 0x103);
	CHECK(TranslateKey(Key(TKey::F12), 0) == 0x10b);
	CHECK(TranslateKey(Key(TKey::Other), 0) == 0);
	CHECK(TranslateKey(Key(TKey::Escape), 0) == 0);
	CHECK(TranslateKey(Letter('a'), 0) == 0);
}

TEST_CASE("Keys: the keypad navigates with NumLock off, or on with Shift")
{
	TKeyEvent Pad = Key(TKey::PadPageUp);
	CHECK(TranslateKey(Pad, 0) == 0x119);
	Pad.NumLock = true;
	CHECK(TranslateKey(Pad, 0) == 0);
	Pad.Shift = true;
	CHECK(TranslateKey(Pad, 0) == 0x119);
	CHECK(TranslateKey(Key(TKey::PadPageDown), 0) == 0x117);
	CHECK(TranslateKey(Key(TKey::PadEnd), 0) == 0x116);
	CHECK(TranslateKey(Key(TKey::PadHome), 0) == 0x118);
	CHECK(TranslateKey(Key(TKey::PadLeft), 0) == 0x10d);
	CHECK(TranslateKey(Key(TKey::PadUp), 0) == 0x10c);
	CHECK(TranslateKey(Key(TKey::PadRight), 0) == 0x10e);
	CHECK(TranslateKey(Key(TKey::PadDown), 0) == 0x10f);
	CHECK(TranslateKey(Key(TKey::PadInsert), 0) == 0x110);
	CHECK(TranslateKey(Key(TKey::PadDelete), 0) == 0x111);
}

TEST_CASE("Keys: Return is a key only with Alt in the client's flags")
{
	CHECK(TranslateKey(Key(TKey::Return), 0) == 0);
	CHECK(TranslateKey(Key(TKey::Return), KeyFlagAlt) == 13);
	CHECK(TranslateKey(Key(TKey::PadEnter), KeyFlagAlt | KeyFlagShift) == 13);
}

TEST_CASE("Keys: the control characters TranslateMessage makes")
{
	CHECK(KeyCharacter(Key(TKey::Return)) == 13);
	CHECK(KeyCharacter(Key(TKey::PadEnter)) == 13);
	CHECK(KeyCharacter(Key(TKey::Escape)) == 27);
	CHECK(KeyCharacter(Key(TKey::Tab)) == 9);
	CHECK(KeyCharacter(Key(TKey::Backspace)) == 8);
	CHECK(KeyCharacter(Key(TKey::Left)) == -1);
	CHECK(KeyCharacter(Letter('a')) == -1); // its character is text
	CHECK(KeyCharacter(Letter('a', true)) == 1);
	CHECK(KeyCharacter(Letter('l', true)) == 12);
	CHECK(KeyCharacter(Letter('z', true)) == 26);
	TKeyEvent ControlReturn = Key(TKey::Return);
	ControlReturn.Control = true;
	CHECK(KeyCharacter(ControlReturn) == 10);
	TKeyEvent ControlBack = Key(TKey::Backspace);
	ControlBack.Control = true;
	CHECK(KeyCharacter(ControlBack) == 127);
	CHECK(KeyCharacter(Letter('a', true, true)) == -1); // AltGr: text
	TKeyEvent AltReturn = Key(TKey::Return);
	AltReturn.Alt = true;
	CHECK(KeyCharacter(AltReturn) == -1); // WM_SYSCHAR
	TKeyEvent ShiftTab = Key(TKey::Tab);
	ShiftTab.Shift = true;
	CHECK(KeyCharacter(ShiftTab) == 9);
}

TEST_CASE("Keys: KeyDown and KeyUp keep the modifier flags and press with them")
{
	TKeyFlags Flags;
	TKeyAction Action = Flags.Down(Key(TKey::Shift));
	CHECK(Action.Kind == TKeyAction::TKind::Press);
	CHECK(Action.Code == (KeyFlagShift | KeyFlagShift));
	CHECK(Flags.Flags() == KeyFlagShift);
	Action = Flags.Down(Key(TKey::Shift)); // a repeat presses again
	CHECK(Action.Code == KeyFlagShift);
	Action = Flags.Down(Key(TKey::Control));
	CHECK(Action.Code == (KeyFlagShift | KeyFlagControl));
	Action = Flags.Down(Key(TKey::Up));
	CHECK(Action.Kind == TKeyAction::TKind::Press);
	CHECK(Action.Code == (KeyFlagShift | KeyFlagControl | 0x10c));
	CHECK(Flags.Down(Letter('a')).Kind == TKeyAction::TKind::None);
	Flags.Up(Key(TKey::Shift));
	CHECK(Flags.Flags() == KeyFlagControl);
	Flags.Up(Key(TKey::Up));
	CHECK(Flags.Flags() == KeyFlagControl);
	Flags.Reset();
	CHECK(Flags.Flags() == 0);
}

TEST_CASE("Keys: Alt+F4 closes, F4 alone is a key")
{
	TKeyFlags Flags;
	TKeyAction Action = Flags.Down(Key(TKey::F4));
	CHECK(Action.Kind == TKeyAction::TKind::Press);
	CHECK(Action.Code == 0x103);
	static_cast<void>(Flags.Down(Key(TKey::Alt)));
	CHECK(Flags.Down(Key(TKey::F4)).Kind == TKeyAction::TKind::Close);
	Action = Flags.Down(Key(TKey::Return));
	CHECK(Action.Code == (KeyFlagAlt | 13));
}

TEST_CASE("Keys: OnChar's characters")
{
	TKeyFlags Flags;
	CHECK(Flags.Character('a') == 'a');
	CHECK(Flags.Character(13) == 13);
	CHECK(Flags.Character(0xe4) == 0xe4);
	CHECK(Flags.Character(256) == -1);
	static_cast<void>(Flags.Down(Key(TKey::Control)));
	CHECK(Flags.Character(1) == (KeyFlagControl | 'a'));
	CHECK(Flags.Character(10) == (KeyFlagControl | 'j'));
	CHECK(Flags.Character(26) == (KeyFlagControl | 'z'));
	CHECK(Flags.Character(0) == -1);
	CHECK(Flags.Character(27) == -1);
	CHECK(Flags.Character(' ') == -1);
	static_cast<void>(Flags.Down(Key(TKey::Alt)));
	CHECK(Flags.Character('@') == (KeyFlagControl | KeyFlagAlt | '@'));
}

TEST_CASE("Clicks: Windows' double clicks from SDL's click counts")
{
	TClickCounter Counter;
	CHECK_FALSE(Counter.IsDouble(1, 1));
	CHECK(Counter.IsDouble(1, 2));
	CHECK_FALSE(Counter.IsDouble(1, 3)); // after a double click a press is a press
	CHECK(Counter.IsDouble(1, 4));
	CHECK_FALSE(Counter.IsDouble(1, 1)); // SDL started again (time or distance)
	// Another button in between: Windows starts again, SDL counts on.
	CHECK_FALSE(Counter.IsDouble(3, 1));
	CHECK_FALSE(Counter.IsDouble(1, 2));
	CHECK(Counter.IsDouble(1, 3));
	CHECK_FALSE(Counter.IsDouble(2, 1)); // the middle button counts too
	CHECK_FALSE(Counter.IsDouble(1, 4));
	CHECK(Counter.IsDouble(1, 5));
	CHECK_FALSE(Counter.IsDouble(3, 1));
	CHECK(Counter.IsDouble(3, 2));
}

TEST_CASE("Wheel: notches of 120, the rest kept with its sign")
{
	TWheel Wheel;
	CHECK(Wheel.Notches(120) == 1);
	CHECK(Wheel.Notches(-240) == -2);
	CHECK(Wheel.Notches(30) == 0);
	CHECK(Wheel.Notches(30) == 0);
	CHECK(Wheel.Notches(60) == 1);
	CHECK(Wheel.Notches(-90) == 0);
	CHECK(Wheel.Notches(-30) == -1);
	CHECK(Wheel.Notches(-100) == 0);
	CHECK(Wheel.Notches(100) == 0); // -100 + 100
}

namespace {

// An RT_CURSOR resource: the hot spot, a 1-bit DIB of Width x Height (twice the height in the header), a palette of
// black and white, the XOR rows and the AND rows, bottom-up, each padded to 4 bytes. Pixels top-down: 'B' black,
// 'W' white, '.' transparent, 'I' inverted.
std::vector<std::uint8_t> CursorResource(int Width, int Height, int HotX, int HotY, const char *const *Rows)
{
	std::vector<std::uint8_t> Bytes;
	auto Put = [&Bytes](std::uint32_t Value, int Size) {
		for (int i = 0; i < Size; i++)
			Bytes.push_back(static_cast<std::uint8_t>(Value >> (8 * i)));
	};
	Put(static_cast<std::uint32_t>(HotX), 2);
	Put(static_cast<std::uint32_t>(HotY), 2);
	Put(40, 4);
	Put(static_cast<std::uint32_t>(Width), 4);
	Put(static_cast<std::uint32_t>(Height * 2), 4);
	Put(1, 2);
	Put(1, 2);
	Put(0, 4);
	Put(0, 4);
	Put(0, 4);
	Put(0, 4);
	Put(2, 4);
	Put(0, 4);
	Put(0x00000000, 4); // black
	Put(0x00ffffff, 4); // white
	const std::size_t Stride = ((static_cast<std::size_t>(Width) + 31) / 32) * 4;
	for (int Plane = 0; Plane < 2; Plane++) { // XOR, then AND
		for (int y = Height - 1; y >= 0; y--) {
			std::vector<std::uint8_t> Row(Stride, 0);
			for (int x = 0; x < Width; x++) {
				const char Pixel = Rows[y][x];
				const bool Xor = Pixel == 'W' || Pixel == 'I';
				const bool And = Pixel == '.' || Pixel == 'I';
				if (Plane == 0 ? Xor : And)
					Row[static_cast<std::size_t>(x) / 8] |= static_cast<std::uint8_t>(0x80U >> (x % 8));
			}
			Bytes.insert(Bytes.end(), Row.begin(), Row.end());
		}
	}
	return Bytes;
}

} // namespace

TEST_CASE("Cursor: a monochrome cursor resource as SDL_CreateCursor's bitmaps")
{
	const std::array<const char *, 2> Rows = {"BW.I......", "IIBB......"};
	const std::vector<std::uint8_t> Resource = CursorResource(10, 2, 3, 1, Rows.data());
	const auto Cursor = DecodeMonochromeCursor(Resource);
	REQUIRE(Cursor.has_value());
	CHECK(Cursor->Width == 10);
	CHECK(Cursor->Height == 2);
	CHECK(Cursor->HotX == 3);
	CHECK(Cursor->HotY == 1);
	REQUIRE(Cursor->Data.size() == 4); // two rows of two bytes, top-down
	REQUIRE(Cursor->Mask.size() == 4);
	// Row 0 "BW.I": black data 1 mask 1, white data 0 mask 1, transparent 0 0, inverted data 1 mask 0.
	CHECK(Cursor->Data[0] == 0x90);
	CHECK(Cursor->Mask[0] == 0xc0);
	CHECK(Cursor->Data[1] == 0x00);
	CHECK(Cursor->Mask[1] == 0x00);
	// Row 1 "IIBB".
	CHECK(Cursor->Data[2] == 0xf0);
	CHECK(Cursor->Mask[2] == 0x30);
}

TEST_CASE("Cursor: what is not a 1-bit cursor is refused")
{
	const std::array<const char *, 1> Rows = {"B"};
	std::vector<std::uint8_t> Resource = CursorResource(1, 1, 0, 0, Rows.data());
	CHECK(DecodeMonochromeCursor(Resource).has_value());
	std::vector<std::uint8_t> Short(Resource.begin(), Resource.end() - 1);
	CHECK_FALSE(DecodeMonochromeCursor(Short).has_value());
	std::vector<std::uint8_t> Colour = Resource;
	Colour[4 + 14] = 4; // 4 bits a pixel
	CHECK_FALSE(DecodeMonochromeCursor(Colour).has_value());
	CHECK_FALSE(DecodeMonochromeCursor(std::vector<std::uint8_t>(10, 0)).has_value());
}

namespace {

// A made-up RT_ICON resource of Width x 2 pixels, 4 bits a pixel: Rows[0] is the top row, each character a palette index
// ('0'-'3') or '.' for transparent. Palette: 0 black, 1 red, 2 green, 3 blue (RGBQUAD: blue, green, red, reserved).
std::vector<std::uint8_t> IconResource(int Width, const std::array<const char *, 2> &Rows)
{
	std::vector<std::uint8_t> Bytes(40, 0);
	const auto Put32 = [&Bytes](std::size_t Offset, std::uint32_t Value) {
		for (std::size_t i = 0; i < 4; i++)
			Bytes[Offset + i] = static_cast<std::uint8_t>(Value >> (8 * i));
	};
	Put32(0, 40);
	Put32(4, static_cast<std::uint32_t>(Width));
	Put32(8, 4); // twice the height
	Bytes[12] = 1;
	Bytes[14] = 4;
	Put32(32, 4);
	for (const std::uint32_t Colour : {0x00000000U, 0x00ff0000U, 0x0000ff00U, 0x000000ffU}) {
		Bytes.push_back(static_cast<std::uint8_t>(Colour));
		Bytes.push_back(static_cast<std::uint8_t>(Colour >> 8));
		Bytes.push_back(static_cast<std::uint8_t>(Colour >> 16));
		Bytes.push_back(0);
	}
	const auto Stride = static_cast<std::size_t>((Width * 4 + 31) / 32) * 4;
	const auto AndStride = static_cast<std::size_t>((Width + 31) / 32) * 4;
	std::vector<std::uint8_t> Colours(Stride * 2, 0);
	std::vector<std::uint8_t> Ands(AndStride * 2, 0);
	for (std::size_t y = 0; y < 2; y++) {
		const std::size_t Row = 1 - y; // bottom-up
		for (std::size_t x = 0; x < static_cast<std::size_t>(Width); x++) {
			const char Pixel = Rows.at(y)[x];
			if (Pixel == '.')
				Ands[Row * AndStride + x / 8] |= static_cast<std::uint8_t>(0x80U >> (x % 8));
			else
				Colours[Row * Stride + x / 2] |= static_cast<std::uint8_t>((Pixel - '0') << (x % 2 == 0 ? 4 : 0));
		}
	}
	Bytes.insert(Bytes.end(), Colours.begin(), Colours.end());
	Bytes.insert(Bytes.end(), Ands.begin(), Ands.end());
	return Bytes;
}

} // namespace

TEST_CASE("Icon: an RT_ICON resource as ARGB rows, top-down, the AND bitmap's pixels transparent")
{
	const auto Icon = DecodeIconImage(IconResource(3, {"01.", ".23"}));
	REQUIRE(Icon.has_value());
	CHECK(Icon->Width == 3);
	CHECK(Icon->Height == 2);
	REQUIRE(Icon->Pixels.size() == 6);
	CHECK(Icon->Pixels[0] == 0xff000000U);
	CHECK(Icon->Pixels[1] == 0xffff0000U);
	CHECK(Icon->Pixels[2] == 0x00000000U);
	CHECK(Icon->Pixels[3] == 0x00000000U);
	CHECK(Icon->Pixels[4] == 0xff00ff00U);
	CHECK(Icon->Pixels[5] == 0xff0000ffU);
}

TEST_CASE("Icon: what is not a palette icon is refused")
{
	const std::vector<std::uint8_t> Resource = IconResource(3, {"000", "000"});
	CHECK(DecodeIconImage(Resource).has_value());
	const std::vector<std::uint8_t> Short(Resource.begin(), Resource.end() - 1);
	CHECK_FALSE(DecodeIconImage(Short).has_value());
	std::vector<std::uint8_t> Deep = Resource;
	Deep[14] = 32;
	CHECK_FALSE(DecodeIconImage(Deep).has_value());
	std::vector<std::uint8_t> Compressed = Resource;
	Compressed[16] = 1;
	CHECK_FALSE(DecodeIconImage(Compressed).has_value());
	std::vector<std::uint8_t> OddHeight = Resource;
	OddHeight[8] = 3;
	CHECK_FALSE(DecodeIconImage(OddHeight).has_value());
	CHECK_FALSE(DecodeIconImage(std::vector<std::uint8_t>(10, 0)).has_value());
}
