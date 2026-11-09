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
// Issue #92: ReadProbe adds what the comparison tools' drivers read from tools/client_state.py besides
// state() and gui() (StateDump.h, "The probe"), for the control channel of TestModeWin32.cpp.
//
// Reading changes nothing in the client: it calls no function of the client, only reads its data.

#include "common.h"

#include "Container.h"
#include "ContentWindows.h"
#include "ContextMenus.h"
#include "Creatures.h"
#include "DialogElements.h"
#include "DialogWindow.h"
#include "Dialogs.h"
#include "GUI.h"
#include "Inventory.h"
#include "MainWindow.h" // IWYU pragma: keep (TMainWindow to TComponent *: the conversion needs the whole class)
#include "Map.h"
#include "MapPoint.h"
#include "MapWindow.h"
#include "Objects.h"
#include "Preferences.h"
#include "Status.h"
#include "Talk.h"
#include "graphics.h"
#include "testmode/ClientStateWin32.h"
#include "testmode/StateDump.h"

#if !defined(_WIN32)
#include <cxxabi.h>
#include <cstdlib>
#include <memory>
#endif

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#if defined(_WIN32)
#include <string_view>
#endif
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
	return {Data, strnlen(Data, Limit)}; // NOLINT(misc-include-cleaner): POSIX and the CRT declare it in <string.h>, which <cstring> gives; the C header itself is not to be included
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
	Out.Objects = Values(Source.Objects.data(), Out.Count);
	return Out;
}

[[maybe_unused]] std::string Hex(std::uintptr_t Value)
{
	std::array<char, 24> Buffer{};
	std::snprintf(Buffer.data(), Buffer.size(), "%llx", static_cast<unsigned long long>(Value)); // NOLINT(cppcoreguidelines-pro-type-vararg): a number into a fixed buffer
	return Buffer.data();
}

std::string ClassName(const TComponent *Component, std::uintptr_t Vtable)
{
#if defined(_WIN32)
	const std::string_view Raw = typeid(*Component).raw_name();
	if (Raw.starts_with(".?AV") && Raw.ends_with("@@"))
		return std::string(Raw.substr(4, Raw.size() - 6));
	auto Base = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
	return "vt_" + Hex(Vtable - Base + 0x400000);
#else
	// Issue #131: the Itanium ABI's name of the class (GCC, Clang), which abi::__cxa_demangle turns into the name
	// the Windows tools see in MSVC's decorated one.
	static_cast<void>(Vtable);
	int Status = 0;
	const std::unique_ptr<char, void (*)(void *)> Demangled(abi::__cxa_demangle(typeid(*Component).name(), nullptr, nullptr, &Status), std::free);
	return Status == 0 && Demangled != nullptr ? std::string(Demangled.get()) : std::string(typeid(*Component).name());
#endif
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
			const char *Full = static_cast<const TLabel *>(Component)->FullText.get();
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

// --- the probe (issue #92) ---

std::int64_t Address(const void *Pointer)
{
	return static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(Pointer));
}

std::uint64_t Key(const void *Pointer)
{
	return reinterpret_cast<std::uintptr_t>(Pointer);
}

void SkinInt(std::vector<TProbeSkin> &Skin, const char *Path, int Value)
{
	Skin.push_back(TProbeSkin{Path, false, {Value}});
}

template <std::size_t N> void SkinRects(std::vector<TProbeSkin> &Skin, const char *Path, const std::array<TRect, N> &Rects)
{
	TProbeSkin Entry{Path, true, {}};
	for (const TRect &Rect : Rects)
		Entry.Values.insert(Entry.Values.end(), {Rect.x, Rect.y, Rect.width, Rect.height});
	Skin.push_back(std::move(Entry));
}

void SkinRect(std::vector<TProbeSkin> &Skin, const char *Path, const TRect &Rect)
{
	SkinRects(Skin, Path, std::array<TRect, 1>{Rect});
}

// The members tools/compare_files.py, compare_rendering.py and session_client.py follow from a component
// (client_state.py's LAYOUT_FIELDS names), by its class.
void Members(TProbeComponent &Out, const TComponent *Component)
{
	auto Add = [&Out](const std::string &Path, std::int64_t Value) { Out.Members.push_back(TProbeMember{Path, Value}); };
	Add("TComponent.Parent", Address(Component->Parent));
	if (const auto *Window = dynamic_cast<const TSubWindow *>(Component))
		Add("TSubWindow.CloseButton", Address(Window->CloseButton));
	if (const auto *Buttons = dynamic_cast<const TTradeButtons *>(Component))
		Add("TTradeButtons.CancelButton", Address(Buttons->CancelButton));
	if (const auto *Automap = dynamic_cast<const TAutomapWindow *>(Component)) {
		Add("TAutomapWindow.ZoomInButton", Address(Automap->ZoomInButton));
		Add("TAutomapWindow.ZoomOutButton", Address(Automap->ZoomOutButton));
	}
	if (const auto *Box = dynamic_cast<const TCheckBox *>(Component))
		Add("TCheckBox.Checked", Byte(Box->Checked));
	if (const auto *Bar = dynamic_cast<const THorizontalScrollBar *>(Component))
		Add("THorizontalScrollBar.LeftButton", Address(Bar->LeftButton));
	if (const auto *Dialog = dynamic_cast<const TGeneralOptionsDialog *>(Component)) {
		Add("TGeneralOptionsDialog.TextualEffects", Address(Dialog->TextualEffects));
		Add("TGeneralOptionsDialog.CreatureNames", Address(Dialog->CreatureNames));
		Add("TGeneralOptionsDialog.ShowHints", Address(Dialog->ShowHints));
		Add("TGeneralOptionsDialog.ClassicControl", Address(Dialog->ClassicControl));
		Add("TGeneralOptionsDialog.AutoChaseOff", Address(Dialog->AutoChaseOff));
	}
	if (const auto *Dialog = dynamic_cast<const TConsoleOptionsDialog *>(Component)) {
		Add("TConsoleOptionsDialog.InfoMessages", Address(Dialog->InfoMessages));
		Add("TConsoleOptionsDialog.EventMessages", Address(Dialog->EventMessages));
		Add("TConsoleOptionsDialog.StatusMessages", Address(Dialog->StatusMessages));
		Add("TConsoleOptionsDialog.TimeStamps", Address(Dialog->TimeStamps));
		Add("TConsoleOptionsDialog.PrivateMessages", Address(Dialog->PrivateMessages));
	}
	if (const auto *Dialog = dynamic_cast<const TNetworkOptionsDialog *>(Component))
		Add("TNetworkOptionsDialog.TestServerLogin", Address(Dialog->TestServerLogin));
	if (const auto *Dialog = dynamic_cast<const TGraphicsOptionsDialog *>(Component))
		Add("TGraphicsOptionsDialog.Fullscreen", Address(Dialog->Fullscreen));
	if (const auto *Dialog = dynamic_cast<const TAdvancedGraphicsDialog *>(Component)) {
		Add("TAdvancedGraphicsDialog.LightEffects", Address(Dialog->LightEffects));
		Add("TAdvancedGraphicsDialog.NoStretching", Address(Dialog->NoStretching));
		Add("TAdvancedGraphicsDialog.WindowsMousepointer", Address(Dialog->WindowsMousepointer));
		Add("TAdvancedGraphicsDialog.Framerate", Address(Dialog->Framerate));
		Add("TAdvancedGraphicsDialog.OpenGLButton", Address(Dialog->OpenGLButton));
	}
	if (const auto *Dialog = dynamic_cast<const THotkeyDialog *>(Component)) {
		Add("THotkeyDialog.AutoSendCheckBox", Address(Dialog->AutoSendCheckBox));
		Add("THotkeyDialog.Selected", Dialog->Selected);
	}
	if (const auto *Dialog = dynamic_cast<const TEditMarkDialog *>(Component)) {
		for (std::size_t i = 0; i < Dialog->TypeButtons.size(); i++)
			Add("TEditMarkDialog.TypeButtons[" + std::to_string(i) + "]", Address(Dialog->TypeButtons.at(i)));
	}
}

// Every component of the window tree, in gui()'s order (Walk's): its members and a map window's names.
void ProbeWalk(TProbe &Probe, const TComponent *Component)
{
	int Count = 0;
	while (Component != nullptr && Count < Siblings) {
		TProbeComponent Entry;
		Entry.Ptr = Key(Component);
		Members(Entry, Component);
		Probe.Components.push_back(std::move(Entry));
		if (const auto *Map = dynamic_cast<const TMapWindow *>(Component)) {
			TProbeNames Names;
			Names.Ptr = Key(Component);
			const int Used = Clamp(Map->NumberOfNames, static_cast<int>(Map->Names.size()));
			for (int i = 0; i < Used; i++) {
				const TCreatureName &Name = Map->Names.at(static_cast<std::size_t>(i));
				Names.Names.push_back(TProbeName{Name.CreatureID, Name.x, Name.y});
			}
			Probe.MapNames.push_back(std::move(Names));
		}
		if (const auto *Window = dynamic_cast<const TWindow *>(Component))
			ProbeWalk(Probe, Window->FirstChild);
		Component = Component->Next;
		Count++;
	}
}

} // namespace

TClientState ReadClientState()
{
	const patella::TTalk &Talk = patella::bridge::Talk();
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
			State.Map.push_back(Values(Point.Objects.data(), PointObjectCount(Point)));
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
		Container.Objects = Values(Source.Objects.data(), Clamp(Source.Count, CONTAINER_OBJECTS));
		State.Containers.push_back(std::move(Container));
	}
	State.Trade = {Offer(*Inventory.OwnOffer), Offer(*Inventory.CounterOffer)};

	if (Talk.DefaultTextBuffer != nullptr) {
		for (const TTalkLine *Line = Talk.DefaultTextBuffer->first; Line != nullptr && State.Console.size() < ConsoleLines;
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

void ReadProbe(TProbe &Probe)
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	Probe.State = ReadClientState();
	Probe.Gui = ReadGui();

	// client_state.py's characters(): the names of the list (none while it has no array).
	TCharactersView Characters;
	ViewCharacters(&Characters);
	if (Characters.Characters != nullptr && Characters.Count != nullptr) {
		for (int i = 0; i < *Characters.Count; i++) {
			const TCharacter &Character = Characters.Characters[i];
			Probe.Characters.push_back(Text(Character.Name, sizeof(Character.Name)));
		}
	}

	// The skin values the drivers place their clicks and masks by (client_state.py skin_rects, skin_int).
	const TSkinProperties &Skin = SkinProperties();
	SkinRects(Probe.Skin, "Style288.Data60", Skin.Style288.Data60);
	SkinInt(Probe.Skin, "Data62C", Skin.Data62C);
	SkinInt(Probe.Skin, "Data630.x", Skin.Data630.x);
	SkinInt(Probe.Skin, "Data630.y", Skin.Data630.y);
	SkinInt(Probe.Skin, "Data638.width", Skin.Data638.width);
	SkinInt(Probe.Skin, "Data638.height", Skin.Data638.height);
	SkinInt(Probe.Skin, "Data648", Skin.Data648);
	SkinInt(Probe.Skin, "Data64C", Skin.Data64C);
	SkinRect(Probe.Skin, "Style6FC.Data68", Skin.Style6FC.Data68);
	SkinRect(Probe.Skin, "Style6FC.Data78", Skin.Style6FC.Data78);
	SkinInt(Probe.Skin, "Style6FC.Data9C", Skin.Style6FC.Data9C);
	SkinInt(Probe.Skin, "Style6FC.DataA0", Skin.Style6FC.DataA0);
	SkinInt(Probe.Skin, "Style6FC.DataA4", Skin.Style6FC.DataA4);
	SkinRect(Probe.Skin, "Data60C", Skin.Data60C);
	SkinRect(Probe.Skin, "Data61C", Skin.Data61C);
	SkinInt(Probe.Skin, "Style1394.Data00", Skin.Style1394.Data00);
	SkinInt(Probe.Skin, "Style1394.Data08.x", Skin.Style1394.Data08.x);
	SkinInt(Probe.Skin, "Style1394.Data08.y", Skin.Style1394.Data08.y);
	SkinInt(Probe.Skin, "Style1394.Data10.y", Skin.Style1394.Data10.y);
	SkinInt(Probe.Skin, "Style1394.Data70.x", Skin.Style1394.Data70.x);
	SkinInt(Probe.Skin, "Style1394.Data70.y", Skin.Style1394.Data70.y);

	if (GUI.MainWindow != nullptr)
		ProbeWalk(Probe, GUI.MainWindow.get());
	if (GUI.ModalWindow != nullptr)
		ProbeWalk(Probe, GUI.ModalWindow);
}

std::vector<TGuiNode> ReadGui()
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	std::vector<TGuiNode> Nodes;
	if (GUI.MainWindow != nullptr)
		Walk(Nodes, GUI.MainWindow.get(), 0, 0, 0);
	if (GUI.ModalWindow != nullptr) {
		TGuiNode Separator;
		Separator.Class = "(modal)";
		Nodes.push_back(Separator);
		Walk(Nodes, GUI.ModalWindow, 1, 0, 0);
	}
	return Nodes;
}

} // namespace patella::testmode
