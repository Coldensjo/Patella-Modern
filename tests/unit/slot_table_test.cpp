// TSlotTable<T, N> of src/types/SlotTable.h (issue #102), Map.cpp's effect table as a TStaticVector<TEffect, EFFECTS>
// (std::inplace_vector in tests_unit on GCC 16, the stand-in in tests_unit_cxx26_standin and elsewhere): the numbers
// New() gives (the first free entry), a full table (New() gives -1 and changes nothing: the original drops the new
// effect), a freed entry taken again with its old members (the original does not clear it), the free entries beyond
// Size(), Clear, and a run of random operations against the original's table, an array of EFFECTS entries searched
// for the first Status 0 (0x4ba6e0, NewEffect). TEntry stands for TEffect (Map.h, which needs the client's headers):
// the members the table and the tests read. The golden corpus's Map test replays the original's effect operations
// through Map.cpp's public functions; these tests hold the table's semantics on every compiler without the corpus.

#include <doctest/doctest.h>

#include "types/FixedString.h"
#include "types/SlotTable.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

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
