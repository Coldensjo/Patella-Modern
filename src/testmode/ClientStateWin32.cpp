// ClientStateWin32.cpp: the test mode's state dump reads the client (issue #22; StateDump.h,
// ClientView.h). Only PatellaTest.exe compiles it (cmake/WindowsX64.cmake).
//
// Every value is read as tools/client_state.py reads it from outside, so that the dump and that
// reading of the same moment are equal (tools/testmode_check.py compares them):
//
// - state(): the integers as 4-byte values, signed (client_state.py's var, field_i32) but a creature's
//   ID (field_u32); the bools as their byte (visible, has_parent); a char[N] up to its first NUL within
//   N bytes; a C string up to its first NUL within 512 bytes (the console) or 256 (a label);
//   containers' and offers' counts clamped to their arrays, as client_state.py clamps them.
// - gui(): the components from MainWindow and from ModalWindow (after a "(modal)" node), siblings
//   through Next (500 at most per list), a window's children through FirstChild; the class name from
//   the RTTI (MSVC's decorated name .?AV<name>@@ without its ends, else vt_<vtable - base + 0x400000>);
//   a component is a window when it is a TWindow (client_state.py: its GetFirstChild is TWindow's,
//   the only override of TComponent's); "ptr" and "vtable" are the addresses in this process.
//
// Reading changes nothing in the client: it calls no function of the client, only reads its data.

#include "common.h"

#include "Container.h"
#include "ContextMenus.h"
#include "Creatures.h"
#include "DialogElements.h"
#include "GUI.h"
#include "Inventory.h"
#include "MainWindow.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Talk.h"
#include "testmode/ClientStateWin32.h"
#include "testmode/StateDump.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <typeinfo> // IWYU pragma: keep (typeid)
#include <utility>
#include <vector>

#if !defined(PATELLA_TEST_MODE)
#error "ClientStateWin32.cpp belongs to the test build (PATELLA_TEST_MODE)"
#endif

namespace patella::testmode {

namespace {

// client_state.py: MAP_POINTS, the console's limit of lines, the C strings' limits, the siblings' limit.
constexpr int MapPoints = MAPSIZE_X * MAPSIZE_Y * MAPSIZE_Z;
constexpr int ConsoleLines = 2000;
constexpr std::size_t ConsoleText = 512;
constexpr std::size_t LabelText = 256;
constexpr int Siblings = 500;

// A char[N] or a C string: up to its first NUL within Limit bytes.
std::string Text(const char *Data, std::size_t Limit)
{
	return {Data, strnlen(Data, Limit)};
}

int Byte(const bool &Value)
{
	unsigned char Raw = 0;
	std::memcpy(&Raw, &Value, 1);
	return Raw;
}

std::int32_t Signed(std::uint32_t Value)
{
	return static_cast<std::int32_t>(Value);
}

TObjectValue Value(const TObject &Object)
{
	return {Object.Type, Object.Data, Object.Data2};
}

std::vector<TObjectValue> Values(const TObject *Objects, int Count)
{
	std::vector<TObjectValue> Out;
	for (int i = 0; i < Count; i++)
		Out.push_back(Value(Objects[i]));
	return Out;
}

int Clamp(int Count, int Limit)
{
	return Count < 0 ? 0 : (Count > Limit ? Limit : Count);
}

TOfferValue Offer(const TOffer &Source)
{
	TOfferValue Out;
	Out.Name = Text(Source.Name, sizeof(Source.Name));
	Out.Count = Clamp(Source.Count, OBJECTS_PER_OFFER);
	Out.Objects = Values(Source.Objects, Out.Count);
	return Out;
}

std::string Hex(std::uintptr_t Value)
{
	std::array<char, 24> Buffer{};
	std::snprintf(Buffer.data(), Buffer.size(), "%llx", static_cast<unsigned long long>(Value));
	return Buffer.data();
}

std::string ClassName(const TComponent *Component, std::uintptr_t Vtable)
{
	std::string_view Raw = typeid(*Component).raw_name();
	if (Raw.starts_with(".?AV") && Raw.ends_with("@@"))
		return std::string(Raw.substr(4, Raw.size() - 6));
	auto Base = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
	return "vt_" + Hex(Vtable - Base + 0x400000);
}

void Walk(std::vector<TGuiNode> &Nodes, TComponent *Component, int Depth, std::int64_t X, std::int64_t Y)
{
	int Count = 0;
	while (Component != nullptr && Count < Siblings) {
		std::uintptr_t Vtable = 0;
		std::memcpy(&Vtable, static_cast<const void *>(Component), sizeof(Vtable));
		TGuiNode Node;
		Node.Depth = Depth;
		Node.Class = ClassName(Component, Vtable);
		Node.X = X + Component->OffsetX;
		Node.Y = Y + Component->OffsetY;
		Node.W = Component->Width;
		Node.H = Component->Height;
		Node.Ptr = reinterpret_cast<std::uintptr_t>(Component);
		Node.Vtable = Vtable;
		if (Node.Class == "TLabel") {
			const char *Full = static_cast<const TLabel *>(Component)->FullText;
			Node.TextKind = Full != nullptr ? TGuiText::Value : TGuiText::Null;
			if (Full != nullptr)
				Node.Text = Text(Full, LabelText);
		} else if (Node.Class == "TContextMenuEntry") {
			const auto *Entry = static_cast<const TContextMenuEntry *>(Component);
			Node.TextKind = TGuiText::Value;
			Node.Text = Text(Entry->Text, sizeof(Entry->Text));
		} else if (Node.Class == "TTextButton") {
			const auto *Button = static_cast<const TTextButton *>(Component);
			Node.TextKind = TGuiText::Value;
			Node.Text = Text(Button->Text, sizeof(Button->Text));
		}
		std::int64_t ChildX = Node.X;
		std::int64_t ChildY = Node.Y;
		Nodes.push_back(std::move(Node));
		if (const auto *Window = dynamic_cast<const TWindow *>(Component))
			Walk(Nodes, Window->FirstChild, Depth + 1, ChildX, ChildY);
		Component = Component->Next;
		Count++;
	}
}

} // namespace

TClientState ReadClientState()
{
	TClientState State;
	State.Connection = *ViewConnectionState();

	TCreaturesView Creatures;
	ViewCreatures(&Creatures);
	State.Player = {Signed(*Creatures.PlayerID), *Creatures.PlayerHealth, *Creatures.PlayerMaxHealth,
		*Creatures.PlayerExperience, *Creatures.PlayerLevel, *Creatures.PlayerLevelProgress, *Creatures.PlayerMagicLevel,
		*Creatures.PlayerMagicLevelProgress, *Creatures.PlayerMana, *Creatures.PlayerMaxMana, *Creatures.PlayerSoul,
		*Creatures.PlayerCapacity, *Creatures.PlayerState, Signed(*Creatures.AttackID), Signed(*Creatures.FollowID)};
	State.Skills.assign(Creatures.PlayerSkillValue, Creatures.PlayerSkillValue + SKILLS);
	State.SkillProgress.assign(Creatures.PlayerSkillProgress, Creatures.PlayerSkillProgress + SKILLS);

	TMapView Map;
	ViewMap(&Map);
	State.MapCentre = {*Map.MapX, *Map.MapY, *Map.MapZ};
	if (Map.Fields->d == MapPoints && Map.Fields->entry != nullptr) {
		for (int i = 0; i < MapPoints; i++) {
			const TMapPoint &Point = Map.Fields->entry[i];
			State.Map.push_back(Values(Point.Objects, Clamp(Point.ObjectCount, OBJECTS_PER_POINT)));
		}
	}

	for (int i = 0; i < CREATURES; i++) {
		const TCreature &Source = Creatures.Creatures[i];
		if (Source.CreatureID == 0)
			continue;
		TCreatureValue Creature;
		Creature.Slot = i;
		Creature.Id = Source.CreatureID;
		Creature.Name = Text(Source.Name, sizeof(Source.Name));
		Creature.Pos = {Source.RealX, Source.RealY, Source.RealZ};
		Creature.Direction = Source.Direction;
		Creature.Outfit = {Source.OutfitType, Source.Colors[0], Source.Colors[1], Source.Colors[2], Source.Colors[3]};
		Creature.Light = {Source.LightStrength, Source.LightColor};
		Creature.Mark = Source.Mark;
		Creature.Health = Source.Health;
		Creature.Speed = Source.Speed;
		Creature.Visible = Byte(Source.Visible);
		Creature.Skull = Source.PKFlag;
		Creature.Shield = Source.PARTYFlag;
		State.Creatures.push_back(std::move(Creature));
	}

	TInventoryView Inventory;
	ViewInventory(&Inventory);
	State.Body = Values(Inventory.Body, BODY_POSITIONS);
	for (int i = 0; i < OPEN_CONTAINERS; i++) {
		const TContainer &Source = Inventory.Containers[i];
		if (Byte(Source.IsOpen) == 0)
			continue;
		TContainerValue Container;
		Container.Index = i;
		Container.Object = Value(Source.Object);
		Container.Name = Text(Source.Name, sizeof(Source.Name));
		Container.Capacity = Source.Capacity;
		Container.HasParent = Byte(Source.HasParent);
		Container.Objects = Values(Source.Objects, Clamp(Source.Count, CONTAINER_OBJECTS));
		State.Containers.push_back(std::move(Container));
	}
	State.Trade = {Offer(*Inventory.OwnOffer), Offer(*Inventory.CounterOffer)};

	if (DefaultTextBuffer != nullptr) {
		for (const TTalkLine *Line = DefaultTextBuffer->first; Line != nullptr && State.Console.size() < ConsoleLines;
			Line = Line->Next) {
			TConsoleLine Entry;
			Entry.Mode = Line->Mode;
			Entry.HasText = Line->WithoutTime != nullptr;
			if (Entry.HasText)
				Entry.Text = Text(Line->WithoutTime, ConsoleText);
			State.Console.push_back(std::move(Entry));
		}
	}
	return State;
}

std::vector<TGuiNode> ReadGui()
{
	std::vector<TGuiNode> Nodes;
	if (MainWindow != nullptr)
		Walk(Nodes, MainWindow, 0, 0, 0);
	if (ModalWindow != nullptr) {
		TGuiNode Separator;
		Separator.Class = "(modal)";
		Nodes.push_back(Separator);
		Walk(Nodes, ModalWindow, 1, 0, 0);
	}
	return Nodes;
}

} // namespace patella::testmode
