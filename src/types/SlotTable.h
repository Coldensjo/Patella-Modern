// SlotTable.h: TSlotTable<T, N>, a table of at most N entries whose numbers stay theirs, an entry with Status 0 being
// free (Patella-Modern issue #102, plan M7's fixed-capacity lists). Not part of the original source. Its one user is
// Map.cpp's effect table (TSlotTable<TEffect, EFFECTS>).
//
// The original's effect table is an array of EFFECTS entries (0x5d16f8); an entry's number is its index, which the
// map points and the entries' links hold, and NewEffect (0x4ba6e0) takes the first entry with Status 0, or none
// (-1) when all are taken, which drops the new effect. TSlotTable keeps those numbers: its entries are a
// TStaticVector<T, N> (compat/cxx26.h: std::inplace_vector where the library has it) that only grows, by one entry
// when no entry below its size is free, and is emptied by Clear only. So the entries from Size() to N are the ones
// the original had free and had not taken since its last clear (or the start), and New() gives the number the
// original's search gave. An entry beyond Size() reads as a free one, value-initialised (Get). A freed entry keeps
// its other members until it is taken again, as the original's did.
//
// T: an implicit-lifetime aggregate with an int Status, value-initialised by T{} (TEffect). Key: the entries' numbers,
// int or an enumeration over int (MapPoint.h's TEffectId, issue #103), its value the number; New() gives Key(-1) when full.
// Chain(First): the entries linked from First by their Next members (a Key, Key(-1) for the last), as a range of
// numbers (issue #103: a map point's effects). Its step reads the Next of the entry it leaves after the loop's body, so
// the body may free that entry if freeing keeps its Next (Map.cpp's DeleteEffect does).

#pragma once

#include "compat/cxx26.h"
#include <cstddef>
#include <iterator> // std::default_sentinel_t

template <class T, std::size_t N, class Key = int>
class TSlotTable {
public:
	// The entries taken since the last Clear (free ones among them): every entry from Size() on is free.
	[[nodiscard]] int Size() const noexcept { return static_cast<int>(Entries.size()); }

	// Entry Number (0 to N - 1); beyond Size() a free one.
	[[nodiscard]] const T &Get(Key Number) const noexcept
	{
		if (static_cast<int>(Number) < Size())
			return Entries[static_cast<std::size_t>(Number)];
		return FreeEntry;
	}

	// Entry Number below Size(), to change it.
	[[nodiscard]] T &operator[](Key Number) noexcept { return Entries[static_cast<std::size_t>(Number)]; }

	// The number of the first free entry, left free (Status 0) for the caller to fill; -1 when all N are taken.
	[[nodiscard]] Key New() noexcept
	{
		for (int i = 0; i < Size(); i++) {
			if (Entries[static_cast<std::size_t>(i)].Status == 0)
				return static_cast<Key>(i);
		}
		if (Entries.try_emplace_back())
			return static_cast<Key>(Size() - 1);
		return static_cast<Key>(-1);
	}

	// Every entry free.
	void Clear() noexcept { Entries.clear(); }

	// The numbers of the entries linked from First by Next (above).
	class TChain {
	public:
		class TIterator {
		public:
			using value_type = Key;
			using difference_type = std::ptrdiff_t;

			TIterator() = default;
			TIterator(const TSlotTable &Of, Key At) noexcept : Table(&Of), Number(At) {}

			[[nodiscard]] Key operator*() const noexcept { return Number; }
			TIterator &operator++() noexcept
			{
				Number = Table->Get(Number).Next;
				return *this;
			}
			TIterator operator++(int) noexcept
			{
				TIterator Before = *this;
				++*this;
				return Before;
			}
			[[nodiscard]] bool operator==(std::default_sentinel_t /*End*/) const noexcept
			{
				return Number == static_cast<Key>(-1);
			}

		private:
			const TSlotTable *Table = nullptr;
			Key Number = static_cast<Key>(-1);
		};

		TChain(const TSlotTable &Of, Key From) noexcept : Table(Of), First(From) {}

		[[nodiscard]] TIterator begin() const noexcept { return {Table, First}; }
		[[nodiscard]] static std::default_sentinel_t end() noexcept { return std::default_sentinel; }

	private:
		const TSlotTable &Table;
		Key First;
	};

	[[nodiscard]] TChain Chain(Key First) const noexcept { return {*this, First}; }

private:
	patella::TStaticVector<T, N> Entries;
	// NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization): a free entry is all zeros, the original's (TEffect's links too)
	static constexpr T FreeEntry{};
};
