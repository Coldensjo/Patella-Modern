// TSlotTable<T, N> of src/types/SlotTable.h (issue #102), Map.cpp's effect table as a TStaticVector<TEffect, EFFECTS>
// (std::inplace_vector in tests_unit on GCC 16, the stand-in in tests_unit_cxx26_standin and elsewhere): the numbers
// New() gives (the first free entry), a full table (New() gives -1 and changes nothing: the original drops the new
// effect), a freed entry taken again with its old members (the original does not clear it), the free entries beyond
// Size(), Clear, and a run of random operations against the original's table, an array of EFFECTS entries searched
// for the first Status 0 (0x4ba6e0, NewEffect). TEntry stands for TEffect (Map.h, which needs the client's headers):
// the members the table and the tests read. The golden corpus's Map test replays the original's effect operations
// through Map.cpp's public functions; these tests hold the table's semantics on every compiler without the corpus.
// Issue #103: a table numbered by an enumeration (TEffectId's kind) and Chain, the range of a map point's effects,
// against the original's loops over the Next links, with the original's way of deleting while walking (ClearField).

#include <doctest/doctest.h>

#include "types/FixedString.h"
#include "types/SlotTable.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int EFFECTS = 200; // Map.h's

struct TEntry {
	int Status;
	int Type;
	TFixedString<12> Text;
	int Field;
	int Next;
};

using TEffectTable = TSlotTable<TEntry, EFFECTS>;

// MapPoint.h's TEffectId and a TEffect's links, for the typed table.
enum class TId : int { None = -1 };

struct TLinkedEntry {
	int Status;
	TId Previous;
	TId Next;
};

using TLinkedTable = TSlotTable<TLinkedEntry, EFFECTS, TId>;

// Map.cpp's NewEffect: a new entry first on the list that Head starts.
TId Push(TLinkedTable &Table, TId &Head)
{
	const TId Number = Table.New();
	if (Number == TId::None)
		return TId::None;
	Table[Number].Status = 1;
	Table[Number].Next = Head;
	Table[Number].Previous = TId::None;
	if (Head != TId::None)
		Table[Head].Previous = Number;
	Head = Number;
	return Number;
}

// Map.cpp's DeleteEffect: frees the entry and unlinks it, keeping its own links.
void Delete(TLinkedTable &Table, TId &Head, TId Number)
{
	TLinkedEntry &Entry = Table[Number];
	Entry.Status = 0;
	if (Head == Number)
		Head = Entry.Next;
	if (Entry.Previous != TId::None)
		Table[Entry.Previous].Next = Entry.Next;
	if (Entry.Next != TId::None)
		Table[Entry.Next].Previous = Entry.Previous;
}

// The original's loop: the numbers from Head by Next.
std::vector<int> Walk(const TLinkedTable &Table, TId Head)
{
	std::vector<int> Numbers;
	for (TId Number = Head; Number != TId::None; Number = Table.Get(Number).Next)
		Numbers.push_back(static_cast<int>(Number));
	return Numbers;
}

// Takes the next entry as Map.cpp's NewEffect does: the number, then its Status.
int Take(TEffectTable &Table, int Status)
{
	const int Number = Table.New();
	if (Number != -1)
		Table[Number].Status = Status;
	return Number;
}

// The original's table and search.
struct TOriginalTable {
	std::array<TEntry, EFFECTS> Entries{};

	int New() const
	{
		for (int i = 0; i < EFFECTS; i++) {
			if (Entries[static_cast<std::size_t>(i)].Status == 0)
				return i;
		}
		return -1;
	}
};

} // namespace

TEST_CASE("TSlotTable: an empty table")
{
	TEffectTable Table;
	CHECK(Table.Size() == 0);
	for (int Number = 0; Number < EFFECTS; Number++)
		CHECK(Table.Get(Number).Status == 0);
	CHECK(Table.New() == 0);
	CHECK(Table.Size() == 1);
	CHECK(Table.Get(0).Status == 0); // still free until the caller fills it
	CHECK(Table.New() == 0);         // so the same entry again
	CHECK(Table.Size() == 1);
}

TEST_CASE("TSlotTable: a full table drops the new effect")
{
	TEffectTable Table;
	for (int Number = 0; Number < EFFECTS; Number++) {
		REQUIRE(Take(Table, 1 + Number % 3) == Number);
		Table[Number].Type = 1000 + Number;
	}
	CHECK(Table.Size() == EFFECTS);
	CHECK(Table.New() == -1);
	CHECK(Take(Table, 1) == -1);
	CHECK(Table.Size() == EFFECTS);
	for (int Number = 0; Number < EFFECTS; Number++) {
		CHECK(Table.Get(Number).Status == 1 + Number % 3);
		CHECK(Table.Get(Number).Type == 1000 + Number);
	}
}

TEST_CASE("TSlotTable: freed entries are taken again, lowest first, with their old fields")
{
	TEffectTable Table;
	for (int Number = 0; Number < EFFECTS; Number++) {
		REQUIRE(Take(Table, 2) == Number);
		Table[Number].Type = Number;
		Table[Number].Text.Assign("abc");
	}
	Table[57].Status = 0;
	Table[3].Status = 0;
	CHECK(Take(Table, 1) == 3);
	CHECK(Table.Get(3).Type == 3); // not cleared: the caller sets what it reads (Map.cpp)
	CHECK(std::strcmp(Table.Get(3).Text, "abc") == 0);
	CHECK(Take(Table, 3) == 57);
	CHECK(Take(Table, 1) == -1);
	CHECK(Table.Size() == EFFECTS);
}

TEST_CASE("TSlotTable: entries beyond Size() are free; the size never shrinks but by Clear")
{
	TEffectTable Table;
	for (int Number = 0; Number < 10; Number++)
		REQUIRE(Take(Table, 1) == Number);
	Table[9].Status = 0;
	Table[4].Status = 0;
	CHECK(Table.Size() == 10);
	CHECK(Table.Get(9).Status == 0);
	CHECK(Table.Get(150).Status == 0);
	CHECK(Table.Get(EFFECTS - 1).Status == 0);
	CHECK(Take(Table, 1) == 4);
	CHECK(Take(Table, 1) == 9);
	CHECK(Take(Table, 1) == 10);
	CHECK(Table.Size() == 11);

	Table[2].Type = 77;
	Table.Clear();
	CHECK(Table.Size() == 0);
	CHECK(Table.Get(2).Status == 0);
	CHECK(Table.New() == 0);
	const TEntry &Entry = Table.Get(0);
	CHECK(Entry.Status == 0);
	CHECK(Entry.Type == 0);
	CHECK(Entry.Field == 0);
	CHECK(Entry.Next == 0);
}

TEST_CASE("TSlotTable: random operations give the original's numbers and statuses")
{
	TEffectTable Table;
	TOriginalTable Original;
	std::uint32_t Seed = 102;
	const auto Random = [&Seed](std::uint32_t Range) {
		Seed = Seed * 1103515245U + 12345U;
		return static_cast<int>((Seed >> 8) % Range);
	};
	int Full = 0;
	for (int Step = 0; Step < 20000; Step++) {
		const int Operation = Random(1000);
		if (Operation < 600) {
			const int Status = 1 + Random(3);
			const int Ours = Take(Table, Status);
			const int Theirs = Original.New();
			if (Theirs != -1)
				Original.Entries[static_cast<std::size_t>(Theirs)].Status = Status;
			REQUIRE(Ours == Theirs);
			Full += Ours == -1 ? 1 : 0;
		} else if (Operation < 999) {
			const int Number = Random(EFFECTS);
			if (Table.Get(Number).Status != 0)
				Table[Number].Status = 0;
			Original.Entries[static_cast<std::size_t>(Number)].Status = 0;
		} else {
			Table.Clear();
			for (TEntry &Entry : Original.Entries)
				Entry.Status = 0;
		}
		bool Same = true;
		for (int Number = 0; Number < EFFECTS; Number++)
			Same = Same && Table.Get(Number).Status == Original.Entries[static_cast<std::size_t>(Number)].Status;
		REQUIRE(Same);
	}
	CHECK(Full > 0); // the run met a full table
}

TEST_CASE("TSlotTable: numbered by an enumeration, a full table gives Key(-1)")
{
	TLinkedTable Table;
	TId Head = TId::None;
	for (int Number = 0; Number < EFFECTS; Number++)
		REQUIRE(Push(Table, Head) == static_cast<TId>(Number));
	CHECK(Table.New() == TId::None);
	CHECK(Push(Table, Head) == TId::None);
	CHECK(Head == static_cast<TId>(EFFECTS - 1));
	CHECK(Table.Get(static_cast<TId>(7)).Status == 1);
}

TEST_CASE("TSlotTable: Chain gives a list's numbers as the original's loop does")
{
	TLinkedTable Table;
	CHECK(Table.Chain(TId::None).begin() == Table.Chain(TId::None).end());

	std::array<TId, 4> Heads{TId::None, TId::None, TId::None, TId::None};
	std::uint32_t Seed = 103;
	const auto Random = [&Seed](std::uint32_t Range) {
		Seed = Seed * 1103515245U + 12345U;
		return static_cast<int>((Seed >> 8) % Range);
	};
	int Cleared = 0;
	for (int Step = 0; Step < 5000; Step++) {
		TId &Head = Heads[static_cast<std::size_t>(Random(4))];
		const int Operation = Random(100);
		if (Operation < 60) {
			static_cast<void>(Push(Table, Head));
		} else if (Operation < 95) {
			// The n-th of the list, as GetEffectIteratorNumber finds it, deleted.
			const std::vector<int> Numbers = Walk(Table, Head);
			if (!Numbers.empty()) {
				int n = Random(static_cast<std::uint32_t>(Numbers.size()));
				TId Found = TId::None;
				for (const TId Number : Table.Chain(Head)) {
					if (n == 0) {
						Found = Number;
						break;
					}
					n--;
				}
				REQUIRE(Found != TId::None);
				Delete(Table, Head, Found);
			}
		} else {
			// ClearField: every effect of the list deleted while the range walks it, in the original's order.
			const std::vector<int> Expected = Walk(Table, Head);
			std::vector<int> Deleted;
			for (const TId Number : Table.Chain(Head)) {
				REQUIRE(Table.Get(Number).Status != 0);
				REQUIRE(Number == Head); // the original deletes the list's first until there is none
				Deleted.push_back(static_cast<int>(Number));
				Delete(Table, Head, Number);
			}
			CHECK(Deleted == Expected);
			CHECK(Head == TId::None);
			Cleared += Expected.empty() ? 0 : 1;
		}
		std::vector<int> Ranged;
		for (const TId Number : Table.Chain(Head))
			Ranged.push_back(static_cast<int>(Number));
		REQUIRE(Ranged == Walk(Table, Head));
	}
	CHECK(Cleared > 0);
}
