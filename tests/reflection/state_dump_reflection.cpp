// state_dump_reflection.cpp: issue #114's experiment, outside the build (notes/decisions.md, "#114: reflection for
// the state dump and the serialisers"). Not part of the original source, not compiled by any preset.
//
// The state dump's creature and container entries (src/testmode/StateDump.h TCreatureValue, TContainerValue) written
// three ways, and compared byte for byte:
//	1. the product's hand-written writer (StateDump.cpp WriteCreature, WriteContainer, through StateDumpJson);
//	2. the fallback a generator would give MSVC and Clang: a table of member pointers per structure
//	   (PATELLA_FIELD, C++23), written by one generic function;
//	3. C++26 reflection (P2996, GCC 16 -freflection): the members of the structure, in their order, iterated with an
//	   expansion statement (P1306); the key is the member's name in snake_case (HasParent: has_parent).
// With reflection the program also checks at compile time that the fallback's tables have every member, in the
// structure's order, under the reflected key, and that the keys are StateDump.h's CreatureKeys and ContainerKeys
// (the lists tools/testmode_check.py compares with client_state.py): the sync check a CI job would make.
//
// Run it in the Linux image (from the repository root; GCC 16.2):
//	python -I tools/build.py --env linux-x64-gcc bash -c "g++ -std=c++26 -freflection -O2 -Wall -Wextra -Wpedantic
//	    -Wconversion -Wsign-conversion -Werror -Isrc tests/reflection/state_dump_reflection.cpp
//	    src/testmode/StateDump.cpp -o /tmp/reflection && /tmp/reflection"
// and without reflection (the fallback against the product, as MSVC and Clang would build it): the same without
// -freflection, with -DPATELLA_NO_REFLECTION, with clang++, or with MSVC (`python -I tools/build.py --env windows-x64
// cl /std:c++latest /EHsc /W4 /WX /O2 /Isrc <the two files>`). Prints "same bytes" and exits 0, or the first
// difference and 1. clang-format 22 does not know the expansion statement (`template for`): it puts its brace on a
// line of its own; clang-tidy (Clang 22, no -freflection) never sees the reflection part.

#include "testmode/StateDump.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202603L && !defined(PATELLA_NO_REFLECTION)
#define PATELLA_HAS_REFLECTION 1
#include <meta>
#else
#define PATELLA_HAS_REFLECTION 0
#endif

namespace {

using patella::testmode::TContainerValue;
using patella::testmode::TCreatureValue;
using patella::testmode::TObjectValue;

// StateDump.cpp's TJsonWriter, as far as the entries need it.
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
		Out += patella::testmode::JsonString(Name);
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
		Out += patella::testmode::JsonString(Latin1);
	}
	[[nodiscard]] const std::string &Text() const { return Out; }

private:
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

// --- The value writers: one per member type (shared by the fallback and reflection) ---

template <class T>
	requires std::is_integral_v<T>
void WriteValue(TJsonWriter &Json, T Value)
{
	if constexpr (std::is_signed_v<T>)
		Json.Int(Value);
	else
		Json.UInt(Value);
}

void WriteValue(TJsonWriter &Json, const std::string &Value)
{
	Json.String(Value);
}

template <class T, std::size_t N> void WriteValue(TJsonWriter &Json, const std::array<T, N> &Values)
{
	Json.BeginArray();
	for (const T &Value : Values)
		WriteValue(Json, Value);
	Json.EndArray();
}

// A TObject is an array [Type, Data, Data2], not an object: a format rule of its own, in either path.
void WriteValue(TJsonWriter &Json, const TObjectValue &Object)
{
	Json.BeginArray();
	Json.Int(Object.Type);
	Json.Int(Object.Data);
	Json.Int(Object.Data2);
	Json.EndArray();
}

template <class T> void WriteValue(TJsonWriter &Json, const std::vector<T> &Values)
{
	Json.BeginArray();
	for (const T &Value : Values)
		WriteValue(Json, Value);
	Json.EndArray();
}

// The key of a member: its name in snake_case (Slot: slot, HasParent: has_parent).
constexpr std::string SnakeCase(std::string_view Name)
{
	std::string Key;
	for (const char c : Name) {
		if (c >= 'A' && c <= 'Z') {
			if (!Key.empty())
				Key += '_';
			Key += static_cast<char>(c - 'A' + 'a');
		} else {
			Key += c;
		}
	}
	return Key;
}

// --- 2. The fallback: a table of member pointers per structure ---

template <class T, class TMember> struct TField {
	std::string_view Member; // the member's name, for the sync check
	TMember T::*Pointer;
};

#define PATELLA_FIELD(Type, Member) \
	TField<Type, decltype(Type::Member)> { #Member, &Type::Member }

template <class T> struct TFields;

template <> struct TFields<TCreatureValue> {
	static constexpr auto List = std::tuple{PATELLA_FIELD(TCreatureValue, Slot), PATELLA_FIELD(TCreatureValue, Id),
		PATELLA_FIELD(TCreatureValue, Name), PATELLA_FIELD(TCreatureValue, Pos), PATELLA_FIELD(TCreatureValue, Direction),
		PATELLA_FIELD(TCreatureValue, Outfit), PATELLA_FIELD(TCreatureValue, Light), PATELLA_FIELD(TCreatureValue, Mark),
		PATELLA_FIELD(TCreatureValue, Health), PATELLA_FIELD(TCreatureValue, Speed),
		PATELLA_FIELD(TCreatureValue, Visible), PATELLA_FIELD(TCreatureValue, Skull),
		PATELLA_FIELD(TCreatureValue, Shield)};
};

template <> struct TFields<TContainerValue> {
	static constexpr auto List = std::tuple{PATELLA_FIELD(TContainerValue, Index),
		PATELLA_FIELD(TContainerValue, Object), PATELLA_FIELD(TContainerValue, Name),
		PATELLA_FIELD(TContainerValue, Capacity), PATELLA_FIELD(TContainerValue, HasParent),
		PATELLA_FIELD(TContainerValue, Objects)};
};

template <class T> void WriteByTable(TJsonWriter &Json, const T &Value)
{
	Json.BeginObject();
	std::apply(
		[&](const auto &...Field) {
			((Json.Key(SnakeCase(Field.Member)), WriteValue(Json, Value.*Field.Pointer)), ...);
		},
		TFields<T>::List);
	Json.EndObject();
}

// --- 3. Reflection ---

#if PATELLA_HAS_REFLECTION

template <class T> consteval auto MembersOf()
{
	return std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
}

template <std::meta::info Member> constexpr const char *KeyOf = std::define_static_string(SnakeCase(std::meta::identifier_of(Member)));

template <class T> void WriteReflected(TJsonWriter &Json, const T &Value)
{
	Json.BeginObject();
	template for (constexpr std::meta::info Member : MembersOf<T>())
	{
		Json.Key(KeyOf<Member>);
		WriteValue(Json, Value.[:Member:]);
	}
	Json.EndObject();
}

// The sync check: the fallback's table has every member of T in T's order, and the keys are Keys.
template <class T, std::size_t N> consteval bool InSync(const std::array<std::string_view, N> &Keys)
{
	constexpr auto Members = MembersOf<T>();
	if (Members.size() != std::tuple_size_v<decltype(TFields<T>::List)> || Members.size() != N)
		return false;
	bool Same = true;
	std::size_t i = 0;
	std::apply(
		[&](const auto &...Field) {
			((Same = Same && Field.Member == std::meta::identifier_of(Members[i]) && SnakeCase(Field.Member) == Keys[i],
				 i++),
				...);
		},
		TFields<T>::List);
	return Same;
}

static_assert(InSync<TCreatureValue>(patella::testmode::CreatureKeys),
	"TFields<TCreatureValue> or CreatureKeys differs from TCreatureValue's members");
static_assert(InSync<TContainerValue>(patella::testmode::ContainerKeys),
	"TFields<TContainerValue> or ContainerKeys differs from TContainerValue's members");

#endif

// --- The comparison ---

// The part of Json between "<Key>": and the next top-level key of "state" (Next).
std::string Between(const std::string &Json, std::string_view Key, std::string_view Next)
{
	const std::string Begin = "\"" + std::string(Key) + "\":";
	const std::size_t Start = Json.find(Begin);
	const std::size_t End = Json.find("\"" + std::string(Next) + "\":", Start);
	if (Start == std::string::npos || End == std::string::npos)
		return {};
	return Json.substr(Start + Begin.size(), End - 1 - Start - Begin.size()); // without the comma before Next
}

template <class T> std::string WriteList(const std::vector<T> &Values, void (*Write)(TJsonWriter &, const T &))
{
	TJsonWriter Json;
	Json.BeginArray();
	for (const T &Value : Values)
		Write(Json, Value);
	Json.EndArray();
	return Json.Text();
}

bool Same(const char *What, const std::string &Expected, const std::string &Actual)
{
	if (Expected == Actual)
		return true;
	std::size_t i = 0;
	while (i < Expected.size() && i < Actual.size() && Expected[i] == Actual[i])
		i++;
	std::printf("%s: differs at byte %zu\n  expected %s\n  actual   %s\n", What, i, Expected.c_str(), Actual.c_str());
	return false;
}

} // namespace

int main()
{
	patella::testmode::TStateDump Dump;
	TCreatureValue Player;
	Player.Slot = 0;
	Player.Id = 0x10000001U;
	Player.Name = "Patella";
	Player.Pos = {32369, 32241, 7};
	Player.Direction = 2;
	Player.Outfit = {128, 78, 69, 58, 76};
	Player.Light = {7, 215};
	Player.Health = 100;
	Player.Speed = 220;
	Player.Visible = 1;
	TCreatureValue Monster;
	Monster.Slot = 149;
	Monster.Id = 0xFFFFFFFFU;                              // the largest id: unsigned
	Monster.Name = std::string("R\xe4tte \"\\\n\x7f", 10); // Latin-1, escapes
	Monster.Pos = {-1, 0, 15};
	Monster.Direction = -3;
	Monster.Mark = -2147483647 - 1;
	Monster.Health = -5;
	Monster.Skull = 3;
	Monster.Shield = 5;
	Dump.State.Creatures = {Player, Monster, TCreatureValue{}};
	TContainerValue Backpack;
	Backpack.Index = 3;
	Backpack.Object = {1988, 0, 0};
	Backpack.Name = "backpack";
	Backpack.Capacity = 20;
	Backpack.HasParent = 1;
	Backpack.Objects = {{2148, 100, 0}, {3031, -1, 7}};
	Dump.State.Containers = {Backpack, TContainerValue{}};

	const std::string Product = patella::testmode::StateDumpJson(Dump);
	const std::string Creatures = Between(Product, "creatures", "body");
	const std::string Containers = Between(Product, "containers", "trade");
	bool Ok = !Creatures.empty() && !Containers.empty();
	Ok &= Same("creatures, the table", Creatures, WriteList<TCreatureValue>(Dump.State.Creatures, WriteByTable));
	Ok &= Same("containers, the table", Containers, WriteList<TContainerValue>(Dump.State.Containers, WriteByTable));
#if PATELLA_HAS_REFLECTION
	Ok &= Same("creatures, reflection", Creatures, WriteList<TCreatureValue>(Dump.State.Creatures, WriteReflected));
	Ok &= Same("containers, reflection", Containers, WriteList<TContainerValue>(Dump.State.Containers, WriteReflected));
#endif
	std::printf("%s (%s; %zu + %zu bytes)\n", Ok ? "same bytes" : "DIFFERENT",
		PATELLA_HAS_REFLECTION ? "product, table and reflection" : "product and table", Creatures.size(),
		Containers.size());
	return Ok ? 0 : 1;
}
