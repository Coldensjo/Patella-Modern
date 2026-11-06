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
// T: an implicit-lifetime aggregate with an int Status, value-initialised by T{} (TEffect).

#pragma once

#include "compat/cxx26.h"
#include <cstddef>

template <class T, std::size_t N>
class TSlotTable {
public:
	// The entries taken since the last Clear (free ones among them): every entry from Size() on is free.
	[[nodiscard]] int Size() const noexcept { return static_cast<int>(Entries.size()); }

	// Entry Number (0 to N - 1); beyond Size() a free one.
	[[nodiscard]] const T &Get(int Number) const noexcept
	{
		if (Number < Size())
			return Entries[static_cast<std::size_t>(Number)];
		return FreeEntry;
	}

	// Entry Number below Size(), to change it.
	[[nodiscard]] T &operator[](int Number) noexcept { return Entries[static_cast<std::size_t>(Number)]; }

	// The number of the first free entry, left free (Status 0) for the caller to fill; -1 when all N are taken.
	[[nodiscard]] int New() noexcept
	{
		for (int i = 0; i < Size(); i++) {
			if (Entries[static_cast<std::size_t>(i)].Status == 0)
				return i;
		}
		if (Entries.try_emplace_back())
			return Size() - 1;
		return -1;
	}

	// Every entry free.
	void Clear() noexcept { Entries.clear(); }

private:
	patella::TStaticVector<T, N> Entries;
	static constexpr T FreeEntry{};
};
