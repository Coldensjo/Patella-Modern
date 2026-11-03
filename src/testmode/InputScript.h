// InputScript.h: the scripted input of the test mode (issue #21; notes/modernization-plan.md, 4.4).
//
// A script is text, one command per line, at a virtual time that starts at 0 (the start of the
// virtual clock, Clock.h) and moves only with wait and at. The parser expands every command into
// primitive events (a key going down or up, a character, a mouse move, a button, the wheel) at that
// time; TScheduler hands them out one at a time and says where the virtual clock goes next. The
// test build's driver (TestModeWin32.cpp) turns each event into the window message the client
// would get from Windows and dispatches it to the main window. Portable: no platform header; the
// key codes are Windows' virtual-key codes and set-1 scan codes as numbers, which the platform layer
// of M8 maps to its own.
//
//	# comment                  (also after a command)
//	pace <ms>                  real milliseconds per 100 virtual ms (default 100: real time; 0: as
//	                           fast as the client runs); only before the first event
//	wait <ms>                  the following commands happen <ms> later
//	at <ms>                    the following commands happen at <ms> (not earlier than now)
//	key <combo>                e.g. Return, Up, Ctrl+L, Shift+Ctrl+Down: the modifiers down, the key
//	                           down, its character (as TranslateMessage makes it for a US layout), the
//	                           key up, the modifiers up
//	keydown <key>, keyup <key> one key; a held Shift, Ctrl or Alt counts for the later commands
//	char <code>                one character (0-255)
//	text "<string>"            one character per character (Latin-1; the file is UTF-8; \" \\ \xHH)
//	move <x> <y>               the mouse to a point of the client area
//	click <x> <y> [<mods>]     move, left button down, up; <mods> e.g. Shift, Ctrl, Shift+Ctrl are
//	                           held around it
//	rclick <x> <y> [<mods>]    the same with the right button
//	dclick, rdclick            a double click: down, up, double click, up
//	ldown, lup, rdown, rup <x> <y> [<mods>]   one button event (and the move to the point)
//	wheel <notches>            the wheel at the mouse; positive turns it up, negative down
//	quit                       closes the main window (WM_CLOSE)
//	dump                       the state dump (issue #22, StateDump.h): the client's state and window
//	                           tree as JSON, after every earlier event was handled
//	frame                      the frame dump (issue #22, Png.h): the next frame the Vulkan engine shows,
//	                           and its device-filtered pixels
//
// A coordinate is a number of pixels from the left or top edge of the client area, or R-<n> / B-<n>
// for <n> pixels from its right or bottom edge (resolved when the event is delivered).
//
// Every event after the script's last one: none; the clock goes on, the client runs on until it
// exits. dump and frame are events of their own (TEventKind::StateDump, FrameDump): the driver
// handles a dump in the idle call of the next event, a frame in an idle call of its own.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace patella::testmode {

// Modifier keys held (TInputEvent::Modifiers).
inline constexpr std::uint32_t ModifierShift = 1;
inline constexpr std::uint32_t ModifierControl = 2;
inline constexpr std::uint32_t ModifierAlt = 4;
// Mouse buttons held (TInputEvent::Buttons).
inline constexpr std::uint32_t ButtonLeft = 1;
inline constexpr std::uint32_t ButtonRight = 2;

enum class TEventKind : std::uint8_t {
	KeyDown,     // Key, Scan, Extended
	KeyUp,       // Key, Scan, Extended
	Char,        // Key: the character; Scan, Extended: the key it came from (0 for text and char)
	MouseMove,   // X, Y
	LeftDown,    // X, Y
	LeftUp,      // X, Y
	LeftDouble,  // X, Y
	RightDown,   // X, Y
	RightUp,     // X, Y
	RightDouble, // X, Y
	Wheel,       // Delta: notches, positive up
	Quit,
	StateDump, // issue #22: the state dump
	FrameDump, // issue #22: the next frame shown
};

// A coordinate of the client area: Value pixels from the left or top edge, or (FromEnd) from the
// right or bottom edge.
struct TCoordinate {
	int Value = 0;
	bool FromEnd = false;

	// The pixel in a client area Size pixels wide (or high).
	[[nodiscard]] int Resolve(int Size) const { return FromEnd ? Size - Value : Value; }
	friend bool operator==(const TCoordinate &, const TCoordinate &) = default;
};

struct TInputEvent {
	std::uint32_t Time = 0; // virtual milliseconds after the start
	TEventKind Kind = TEventKind::Quit;
	std::uint32_t Key = 0;       // virtual-key code, or the character of a Char event
	std::uint32_t Scan = 0;      // set-1 scan code of the key
	bool Extended = false;       // an extended key (the arrows and the block above them)
	std::uint32_t Modifiers = 0; // ModifierShift/Control/Alt held at this event (after a KeyDown of one)
	std::uint32_t Buttons = 0;   // ButtonLeft/Right held at this event (after a button went down)
	TCoordinate X, Y;
	int Delta = 0;
	int Line = 0; // the script line it comes from

	friend bool operator==(const TInputEvent &, const TInputEvent &) = default;
};

struct TKeyInfo {
	std::uint32_t Key = 0; // virtual-key code
	std::uint32_t Scan = 0;
	bool Extended = false;
};

// The key named Name (caseless: A-Z, 0-9, F1-F12, Up, Down, Left, Right, Home, End, PageUp,
// PageDown, Insert, Delete, NumHome, NumEnd, NumPageUp, NumPageDown, Return or Enter, Escape, Tab,
// Back or Backspace, Space, Shift, Ctrl or Control, Alt); false when there is none.
bool FindKey(std::string_view Name, TKeyInfo *Info);
// The character the key makes with Modifiers held, as TranslateMessage makes it for a US layout:
// letters, digits and their shifted symbols, Space, Return 13, Escape 27, Tab 9, Back 8; with Ctrl
// (no Alt) the letters make 1-26, Return 10, Back 127, Escape 27; with Alt none. -1: none.
[[nodiscard]] int KeyCharacter(std::uint32_t Key, std::uint32_t Modifiers);
// One line of the driver's log for Event.
[[nodiscard]] std::string Describe(const TInputEvent &Event);

struct TScript {
	std::vector<TInputEvent> Events; // in time order, then script order
	std::uint32_t RealPerTick = 100; // pace: real milliseconds per TickMilliseconds of virtual time
};

struct TParseResult {
	bool Ok = false;
	TScript Script;
	int ErrorLine = 0;
	std::string Error;
};

// Parses a script (the text of the file).
[[nodiscard]] TParseResult ParseScript(std::string_view Text);

// Hands out the events of a script at the virtual time and says where the clock goes next.
class TScheduler {
public:
	TScheduler() = default;
	explicit TScheduler(TScript Source) : Script(std::move(Source)) {}

	// The next event due at Elapsed (virtual milliseconds since the start), one at a time in script
	// order; nullptr when none is due. The driver delivers one event per idle call, so that the
	// client handles each (a mouse move is polled in OnIdle) before the next.
	const TInputEvent *TakeDue(std::uint32_t Elapsed);
	// Where the virtual clock goes from Elapsed when no event is due: the next tick of the main
	// timer (a multiple of TickMilliseconds) or the next event, whichever comes first.
	[[nodiscard]] std::uint32_t NextStop(std::uint32_t Elapsed) const;
	// The real milliseconds after the driver's start at which the clock may reach Elapsed.
	[[nodiscard]] std::uint64_t RealDeadline(std::uint32_t Elapsed) const;
	// Every event handed out.
	[[nodiscard]] bool Finished() const { return Next >= Script.Events.size(); }
	[[nodiscard]] std::size_t Remaining() const { return Script.Events.size() - Next; }

	// A tick of the main timer falls on Elapsed.
	[[nodiscard]] static bool IsTick(std::uint32_t Elapsed);

private:
	TScript Script;
	std::size_t Next = 0;
};

} // namespace patella::testmode
