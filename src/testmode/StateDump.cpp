// StateDump.cpp: the JSON form of the test mode's state dump (StateDump.h; issue #22).

#include "StateDump.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace patella::testmode {

namespace {

// Writes JSON without spaces: values, arrays and objects, the commas between them.
class TJsonWriter {
public:
	void BeginObject()
	{
		Separate();
		Out += '{';
		First.push_back(true);
	}
	void EndObject()
	{
		Out += '}';
		First.pop_back();
	}
	void BeginArray()
	{
		Separate();
		Out += '[';
		First.push_back(true);
	}
	void EndArray()
	{
		Out += ']';
		First.pop_back();
	}
	void Key(std::string_view Name)
	{
		Separate();
		Out += JsonString(Name);
		Out += ':';
		AfterKey = true;
	}
	void Int(std::int64_t Value)
	{
		Separate();
		Out += std::to_string(Value);
	}
	void UInt(std::uint64_t Value)
	{
		Separate();
		Out += std::to_string(Value);
	}
	void String(std::string_view Latin1)
	{
		Separate();
		Out += JsonString(Latin1);
	}
	void Null()
	{
		Separate();
		Out += "null";
	}
	[[nodiscard]] const std::string &Text() const { return Out; }

private:
	// The comma before a value that is not the first of its array or object (a key's value has none).
	void Separate()
	{
		if (AfterKey) {
			AfterKey = false;
			return;
		}
		if (First.empty())
			return;
		if (!First.back())
			Out += ',';
		First.back() = false;
	}

	std::string Out;
	std::vector<bool> First;
	bool AfterKey = false;
};

void WriteObject(TJsonWriter &Json, const TObjectValue &Object)
{
	Json.BeginArray();
	Json.Int(Object.Type);
	Json.Int(Object.Data);
	Json.Int(Object.Data2);
	Json.EndArray();
}

void WriteObjects(TJsonWriter &Json, const std::vector<TObjectValue> &Objects)
{
	Json.BeginArray();
	for (const TObjectValue &Object : Objects)
		WriteObject(Json, Object);
	Json.EndArray();
}

template <class TList> void WriteInts(TJsonWriter &Json, const TList &Values)
{
	Json.BeginArray();
	for (auto Value : Values)
		Json.Int(Value);
	Json.EndArray();
}

void WriteCreature(TJsonWriter &Json, const TCreatureValue &Creature)
{
	// CreatureKeys' order.
	Json.BeginObject();
	Json.Key(CreatureKeys[0]);
	Json.Int(Creature.Slot);
	Json.Key(CreatureKeys[1]);
	Json.UInt(Creature.Id);
	Json.Key(CreatureKeys[2]);
	Json.String(Creature.Name);
	Json.Key(CreatureKeys[3]);
	WriteInts(Json, Creature.Pos);
	Json.Key(CreatureKeys[4]);
	Json.Int(Creature.Direction);
	Json.Key(CreatureKeys[5]);
	WriteInts(Json, Creature.Outfit);
	Json.Key(CreatureKeys[6]);
	WriteInts(Json, Creature.Light);
	Json.Key(CreatureKeys[7]);
	Json.Int(Creature.Mark);
	Json.Key(CreatureKeys[8]);
	Json.Int(Creature.Health);
	Json.Key(CreatureKeys[9]);
	Json.Int(Creature.Speed);
	Json.Key(CreatureKeys[10]);
	Json.Int(Creature.Visible);
	Json.Key(CreatureKeys[11]);
	Json.Int(Creature.Skull);
	Json.Key(CreatureKeys[12]);
	Json.Int(Creature.Shield);
	Json.EndObject();
}

void WriteContainer(TJsonWriter &Json, const TContainerValue &Container)
{
	Json.BeginObject();
	Json.Key(ContainerKeys[0]);
	Json.Int(Container.Index);
	Json.Key(ContainerKeys[1]);
	WriteObject(Json, Container.Object);
	Json.Key(ContainerKeys[2]);
	Json.String(Container.Name);
	Json.Key(ContainerKeys[3]);
	Json.Int(Container.Capacity);
	Json.Key(ContainerKeys[4]);
	Json.Int(Container.HasParent);
	Json.Key(ContainerKeys[5]);
	WriteObjects(Json, Container.Objects);
	Json.EndObject();
}

void WriteOffer(TJsonWriter &Json, const TOfferValue &Offer)
{
	Json.BeginObject();
	Json.Key(OfferKeys[0]);
	Json.String(Offer.Name);
	Json.Key(OfferKeys[1]);
	Json.Int(Offer.Count);
	Json.Key(OfferKeys[2]);
	WriteObjects(Json, Offer.Objects);
	Json.EndObject();
}

void WriteState(TJsonWriter &Json, const TClientState &State)
{
	// StateKeys' order.
	Json.BeginObject();
	Json.Key(StateKeys[0]);
	Json.Int(State.Connection);
	Json.Key(StateKeys[1]);
	Json.BeginObject();
	for (std::size_t i = 0; i < PlayerFields.size(); i++) {
		Json.Key(PlayerFields.at(i));
		Json.Int(State.Player.at(i));
	}
	Json.EndObject();
	Json.Key(StateKeys[2]);
	WriteInts(Json, State.Skills);
	Json.Key(StateKeys[3]);
	WriteInts(Json, State.SkillProgress);
	Json.Key(StateKeys[4]);
	WriteInts(Json, State.MapCentre);
	Json.Key(StateKeys[5]);
	Json.BeginArray();
	for (const std::vector<TObjectValue> &Point : State.Map)
		WriteObjects(Json, Point);
	Json.EndArray();
	Json.Key(StateKeys[6]);
	Json.BeginArray();
	for (const TCreatureValue &Creature : State.Creatures)
		WriteCreature(Json, Creature);
	Json.EndArray();
	Json.Key(StateKeys[7]);
	WriteObjects(Json, State.Body);
	Json.Key(StateKeys[8]);
	Json.BeginArray();
	for (const TContainerValue &Container : State.Containers)
		WriteContainer(Json, Container);
	Json.EndArray();
	Json.Key(StateKeys[9]);
	Json.BeginArray();
	for (const TOfferValue &Offer : State.Trade)
		WriteOffer(Json, Offer);
	Json.EndArray();
	Json.Key(StateKeys[10]);
	Json.BeginArray();
	for (const TConsoleLine &Line : State.Console) {
		Json.BeginArray();
		Json.Int(Line.Mode);
		if (Line.HasText)
			Json.String(Line.Text);
		else
			Json.Null();
		Json.EndArray();
	}
	Json.EndArray();
	Json.EndObject();
}

void WriteGui(TJsonWriter &Json, const std::vector<TGuiNode> &Nodes)
{
	Json.BeginArray();
	for (const TGuiNode &Node : Nodes) {
		// GuiKeys' order.
		Json.BeginObject();
		Json.Key(GuiKeys[0]);
		Json.Int(Node.Depth);
		Json.Key(GuiKeys[1]);
		Json.String(Node.Class);
		Json.Key(GuiKeys[2]);
		Json.Int(Node.X);
		Json.Key(GuiKeys[3]);
		Json.Int(Node.Y);
		Json.Key(GuiKeys[4]);
		Json.Int(Node.W);
		Json.Key(GuiKeys[5]);
		Json.Int(Node.H);
		Json.Key(GuiKeys[6]);
		Json.UInt(Node.Ptr);
		Json.Key(GuiKeys[7]);
		Json.UInt(Node.Vtable);
		if (Node.TextKind != TGuiText::None) {
			Json.Key(GuiKeys[8]);
			if (Node.TextKind == TGuiText::Value)
				Json.String(Node.Text);
			else
				Json.Null();
		}
		Json.EndObject();
	}
	Json.EndArray();
}

} // namespace

std::string JsonString(std::string_view Latin1)
{
	static constexpr std::string_view Hex = "0123456789abcdef";
	std::string Out = "\"";
	for (char c : Latin1) {
		auto Byte = static_cast<unsigned char>(c);
		switch (Byte) {
		case '"': Out += "\\\""; break;
		case '\\': Out += "\\\\"; break;
		case '\n': Out += "\\n"; break;
		case '\r': Out += "\\r"; break;
		case '\t': Out += "\\t"; break;
		case '\b': Out += "\\b"; break;
		case '\f': Out += "\\f"; break;
		default:
			if (Byte >= 0x20 && Byte <= 0x7e) {
				Out += c;
			} else {
				Out += "\\u00";
				Out += Hex[Byte >> 4U];
				Out += Hex[Byte & 0xfU];
			}
			break;
		}
	}
	Out += '"';
	return Out;
}

std::string StateDumpJson(const TStateDump &Dump)
{
	TJsonWriter Json;
	Json.BeginObject();
	Json.Key("format");
	Json.String(StateDumpFormat);
	Json.Key("step");
	Json.Int(Dump.Step);
	Json.Key("time");
	Json.UInt(Dump.Time);
	Json.Key("line");
	Json.Int(Dump.Line);
	Json.Key("state");
	WriteState(Json, Dump.State);
	Json.Key("gui");
	WriteGui(Json, Dump.Gui);
	Json.EndObject();
	return Json.Text() + "\n";
}

} // namespace patella::testmode
