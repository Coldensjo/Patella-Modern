// StateDump.h: the state dump of the test mode (issue #22; notes/modernization-plan.md, 4.4;
// notes/testmode.md, "The state dump").
//
// At a script's `dump` command the test build writes the client's state as JSON: exactly what
// tools/client_state.py's state() and gui() read from the client's memory from outside, field for
// field and with the same names, so that the two can be compared value for value (the issue's
// criterion) and issue #23 can compare the test client's dumps with Tibia.exe's readings. This file
// is the portable half: the data (filled by the Windows side, src/testmode/ClientStateWin32.cpp) and
// its JSON form. Portable: no platform header.
//
// A dump file (one per dump, state-<NNN>.json, one line):
//
//	{"format":"patella-state 1","step":N,"time":T,"line":L,"state":{...},"gui":[...]}
//
// step: the dump's number (1, 2, ...); time: the virtual milliseconds of the script; line: the
// script line of the dump command. "state" is client_state.py's state(), "gui" its gui(); the keys
// below (StateKeys and the others) are those of client_state.py, tools/testmode_check.py compares the
// two lists statically. Integers are JSON numbers; texts are the client's Latin-1 bytes as JSON
// strings (outside printable ASCII as \u00XX, as Python's json module writes them).

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace patella::testmode {

// The format name of a dump file ("format").
inline constexpr std::string_view StateDumpFormat = "patella-state 1";

// The keys of "state", in client_state.py's order.
inline constexpr std::array<std::string_view, 11> StateKeys = {"connection", "player", "skills", "skill_progress",
	"map_centre", "map", "creatures", "body", "containers", "trade", "console"};
// The keys of "state"."player": the client's globals of the same names (Creatures.cpp).
inline constexpr std::array<std::string_view, 15> PlayerFields = {"PlayerID", "PlayerHealth", "PlayerMaxHealth",
	"PlayerExperience", "PlayerLevel", "PlayerLevelProgress", "PlayerMagicLevel", "PlayerMagicLevelProgress",
	"PlayerMana", "PlayerMaxMana", "PlayerSoul", "PlayerCapacity", "PlayerState", "AttackID", "FollowID"};
// The keys of a creature (a used slot of Creatures, Creatures.cpp).
inline constexpr std::array<std::string_view, 13> CreatureKeys = {"slot", "id", "name", "pos", "direction", "outfit",
	"light", "mark", "health", "speed", "visible", "skull", "shield"};
// The keys of an open container (Inventory.cpp's Containers).
inline constexpr std::array<std::string_view, 6> ContainerKeys = {"index", "object", "name", "capacity", "has_parent",
	"objects"};
// The keys of a trade offer (OwnOffer, CounterOffer).
inline constexpr std::array<std::string_view, 3> OfferKeys = {"name", "count", "objects"};
// The keys of a node of "gui"; "text" only for TLabel, TContextMenuEntry and TTextButton.
inline constexpr std::array<std::string_view, 9> GuiKeys = {"depth", "class", "x", "y", "w", "h", "ptr", "vtable",
	"text"};

// A TObject: [Type, Data, Data2].
struct TObjectValue {
	std::int32_t Type = 0;
	std::int32_t Data = 0;
	std::int32_t Data2 = 0;
	friend bool operator==(const TObjectValue &, const TObjectValue &) = default;
};

struct TCreatureValue {
	int Slot = 0;
	std::uint32_t Id = 0;
	std::string Name; // Latin-1
	std::array<std::int32_t, 3> Pos{};
	std::int32_t Direction = 0;
	std::array<std::int32_t, 5> Outfit{}; // OutfitType, Colors[4]
	std::array<std::int32_t, 2> Light{};  // LightStrength, LightColor
	std::int32_t Mark = 0;
	std::int32_t Health = 0;
	std::int32_t Speed = 0;
	int Visible = 0; // the bool's byte
	std::int32_t Skull = 0;
	std::int32_t Shield = 0;
};

struct TContainerValue {
	int Index = 0;
	TObjectValue Object;
	std::string Name;
	std::int32_t Capacity = 0;
	int HasParent = 0; // the bool's byte
	std::vector<TObjectValue> Objects;
};

struct TOfferValue {
	std::string Name;
	std::int32_t Count = 0; // clamped to 0..OBJECTS_PER_OFFER, as client_state.py does
	std::vector<TObjectValue> Objects;
};

struct TConsoleLine {
	std::int32_t Mode = 0;
	bool HasText = false; // false: null (no text)
	std::string Text;
};

struct TClientState {
	std::int32_t Connection = 0;
	std::array<std::int32_t, PlayerFields.size()> Player{}; // in PlayerFields' order
	std::vector<std::int32_t> Skills;
	std::vector<std::int32_t> SkillProgress;
	std::array<std::int32_t, 3> MapCentre{};
	std::vector<std::vector<TObjectValue>> Map; // every map point's objects
	std::vector<TCreatureValue> Creatures;
	std::vector<TObjectValue> Body;
	std::vector<TContainerValue> Containers;
	std::array<TOfferValue, 2> Trade; // OwnOffer, CounterOffer
	std::vector<TConsoleLine> Console;
};

enum class TGuiText : std::uint8_t {
	None,  // no "text" key
	Null,  // "text": null
	Value, // "text": Text
};

struct TGuiNode {
	int Depth = 0;
	std::string Class;
	std::int64_t X = 0;
	std::int64_t Y = 0;
	std::int32_t W = 0;
	std::int32_t H = 0;
	std::uint64_t Ptr = 0;    // the component's address (this run only)
	std::uint64_t Vtable = 0; // its vtable's address (this run only)
	TGuiText TextKind = TGuiText::None;
	std::string Text;
};

struct TStateDump {
	int Step = 0;
	std::uint32_t Time = 0;
	int Line = 0;
	TClientState State;
	std::vector<TGuiNode> Gui;
};

// --- The probe (issue #92) ---
//
// The control channel's `probe` (TestModeWin32.cpp, --test-control): what the comparison tools read from a
// running test client instead of its memory. The state and the window tree of a state dump, plus what
// their session drivers read from tools/client_state.py besides state() and gui() (the places they click
// at): the character list, the skin values (SkinProperties), the members of the components they follow
// (a dialog's check box, a window's close button, ...: ProbeMembers), the names on a map window, and the
// Vulkan engine's device and frames. One line of JSON, probe.json in --test-out:
//
//	{"format":"patella-probe 1","step":N,"time":T,"state":{...},"gui":[...],"characters":[...],
//	 "skin":{"Style288.Data60":[[x,y,w,h],...],"Data62C":v,...},"members":{"<ptr>":{"<path>":v,...}},
//	 "names":{"<ptr>":[[id,x,y],...]},"device":null|{"name":...,"type":n,"cpu_driver":0|1},
//	 "frames":n,"ticks":n}
//
// "<ptr>" is a component's "ptr" of "gui" as a decimal string; a member's path and a skin value's are
// client_state.py's LAYOUT_FIELDS names (Type.Member, TSkinProperties' members without the type), a
// pointer member's value a "ptr" (0: none), a bool's its byte. step: the probe's number; time: the
// virtual milliseconds (0 without a script); frames: the frames the Vulkan engine showed to the test
// mode's observer; ticks: the main timer's ticks the driver sent (advance).

// The format name of a probe file.
inline constexpr std::string_view ProbeFormat = "patella-probe 1";

struct TProbeMember {
	std::string Path;
	std::int64_t Value = 0;
};

// The members of one component.
struct TProbeComponent {
	std::uint64_t Ptr = 0;
	std::vector<TProbeMember> Members;
};

// A name on the map window (TMapWindow.Names: TCreatureName).
struct TProbeName {
	std::uint32_t CreatureId = 0;
	std::int32_t X = 0;
	std::int32_t Y = 0;
};

struct TProbeNames {
	std::uint64_t Ptr = 0; // the map window
	std::vector<TProbeName> Names;
};

// A skin value: an int, or (Rects) TRects as x, y, width, height each.
struct TProbeSkin {
	std::string Path;
	bool Rects = false;
	std::vector<std::int32_t> Values;
};

// The Vulkan engine's device as the engine reported it (the frame observer's device call).
struct TProbeDevice {
	bool Known = false;
	std::string Name;
	int Type = 0; // VkPhysicalDeviceType
	int CpuDriver = 0;
};

struct TProbe {
	int Step = 0;
	std::uint32_t Time = 0;
	TClientState State;
	std::vector<TGuiNode> Gui;
	std::vector<std::string> Characters; // Latin-1
	std::vector<TProbeSkin> Skin;
	std::vector<TProbeComponent> Components;
	std::vector<TProbeNames> MapNames;
	TProbeDevice Device;
	std::uint64_t Frames = 0;
	std::uint64_t Ticks = 0;
};

// A Latin-1 text as a JSON string (with the quotes), as Python's json.dumps writes it: \" \\ \n \r \t
// \b \f, every other byte outside 0x20-0x7e as \u00XX.
[[nodiscard]] std::string JsonString(std::string_view Latin1);
// The dump file's text (one line, with a newline at the end).
[[nodiscard]] std::string StateDumpJson(const TStateDump &Dump);
// The probe file's text (one line, with a newline at the end; issue #92).
[[nodiscard]] std::string ProbeJson(const TProbe &Probe);

} // namespace patella::testmode
