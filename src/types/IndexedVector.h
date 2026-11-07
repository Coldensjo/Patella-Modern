// IndexedVector.h: TIndexedVector<T>, CipSoft's growable vector<T> of Container.h (c:\mud\tibiaplayer\Container.h,
// lines 353-370 and its operator()) over a std::vector. Patella-Modern issue #99 (notes/modernization-plan.md, section
// 7, M7; notes/decisions.md, "TIndexedVector over std::vector"). The semantics are the original's, unchanged:
//
// - The entries are indexed from min to max, the lowest and highest index used so far; entry[0] is index start, and
//   space entries are allocated (start <= min <= max < start + space).
// - Indexing outside start..start + space - 1 grows the storage by block entries at a time (by its current size, so
//   doubling, when block is 0), below or above, until the index is inside; the entries from min to max keep their
//   values and indices, each swapped into the new storage as the original swaps it (SwapIndexedEntries).
// - Indexing below min or above max moves min or max to the index; with initialized set (the four-argument
//   constructor) every entry so taken into the range is set to init first, as is every entry at construction.
// - New storage is allocated as the original's new T[n]: through operator new[] (the golden corpus's Container test
//   fills that memory with its pattern and compares every allocated entry), each entry default-initialised (a type
//   without a constructor keeps the memory's bytes, one with a constructor runs it). std::vector's own allocator
//   value-initialises (zeros an int), so the storage has its own allocator, TArrayNewAllocator.
//
// The members are the original's, in its order and with its names (the Container test compares min ... init, with
// their padding, by the original's offsets; the storage, the original's T *entry, is the last member). Instances of the
// original and their addresses: Container.h's list. The members are initialised with braces: <windows.h>'s min and
// max macros take a parenthesis after the name.

#pragma once

#include "compat/assertion.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

// Allocates as new T[n] does: operator new[], and an entry constructed without arguments is default-initialised, not
// value-initialised. Stateless; every instance equals every other.
template <class T> class TArrayNewAllocator {
public:
	using value_type = T;
	static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "TArrayNewAllocator: no over-aligned types");

	TArrayNewAllocator() noexcept = default;
	template <class U> explicit(false) TArrayNewAllocator(const TArrayNewAllocator<U> & /*Other*/) noexcept {}

	T *allocate(std::size_t Count) { return static_cast<T *>(::operator new[](Count * sizeof(T))); }
	void deallocate(T *Entries, std::size_t /*Count*/) noexcept { ::operator delete[](static_cast<void *>(Entries)); }

	// new T[n]'s initialisation of each entry; with arguments a plain construction. GCC's analyzer (issue #111) takes
	// the null storage of a vector of no entries into the loop that constructs none (-Wanalyzer-null-dereference).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-null-dereference"
#endif
	template <class U> void construct(U *Entry) noexcept(std::is_nothrow_default_constructible_v<U>)
	{
		::new (static_cast<void *>(Entry)) U;
	}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
	template <class U, class... A> void construct(U *Entry, A &&...Arguments)
	{
		::new (static_cast<void *>(Entry)) U(std::forward<A>(Arguments)...);
	}

	friend bool operator==(const TArrayNewAllocator & /*A*/, const TArrayNewAllocator & /*B*/) noexcept { return true; }
};

// Swaps two entries as the original does when its vector<T> grows: through a copy, each entry copied as a block of
// bytes with its padding (VC7.1 copies a structure so, and so does MSVC). The golden corpus compares the entries'
// padding (Patella-Modern issue #91), which GCC and Clang do not copy when they copy a structure member by member; a
// type that is not trivially copyable is copied with its assignment.
template <class T> inline void SwapIndexedEntries(T &A, T &B)
{
	if constexpr (std::is_trivially_copyable_v<T>) {
		unsigned char Help[sizeof(T)];
		// The entries as bytes (a vector of pointers too: bugprone-bitwise-pointer-cast takes the copy for a cast).
		std::memcpy(Help, static_cast<const void *>(&A), sizeof(T));
		std::memcpy(static_cast<void *>(&A), static_cast<const void *>(&B), sizeof(T)); // NOLINT(bugprone-bitwise-pointer-cast): a copy of bytes
		std::memcpy(static_cast<void *>(&B), Help, sizeof(T));
	} else {
		T Help = A;
		A = B;
		B = Help;
	}
}

template <class T> class TIndexedVector {
public:
	using TEntries = std::vector<T, TArrayNewAllocator<T>>;

	// Every entry from Min to Max starts as Init, and so does every entry the range takes in later.
	TIndexedVector(int Min, int Max, int Block, T Init);
	// The entries are default-initialised (new T[n]).
	TIndexedVector(int Min, int Max, int Block);
	TIndexedVector(const TIndexedVector &) = delete;
	TIndexedVector &operator=(const TIndexedVector &) = delete;
	TIndexedVector(TIndexedVector &&) = delete;
	TIndexedVector &operator=(TIndexedVector &&) = delete;
	~TIndexedVector() = default;

	// The entry of index i, growing the storage and the range to it.
	T &operator()(int i);

	int min;          // lowest index used so far
	int max;          // highest index used so far
	int start;        // index of entry[0]
	int space;        // allocated entries, entry.size()
	int block;        // growth step, 0 = double
	bool initialized; // fill new entries with init
	T init;           // (the three-argument constructor leaves it default-initialised, as the original)
	TEntries entry;   // the storage, the last member

private:
	static std::size_t Offset(int Index) { return static_cast<std::size_t>(Index); }
	void Grow(int Below, int Above);
};

// Lines 353-358 (0x453300, 0x425e70, 0x4973f0, 0x4fd6e0).
template <class T>
TIndexedVector<T>::TIndexedVector(int Min, int Max, int Block, T Init)
	: min{Min}, max{Max}, start{Min}, space{Max - Min + 1}, block{Block}, initialized{true}, init{Init}
{
	PATELLA_ASSERT_VALUES(space > 0, space);
	PATELLA_ASSERT_VALUES(block >= 0, block);
	entry = TEntries(Offset(space));
	for (int i = 0; i < space; i++)
		entry[Offset(i)] = init;
}

// Lines 368-370.
template <class T>
TIndexedVector<T>::TIndexedVector(int Min, int Max, int Block)
	: min{Min}, max{Max}, start{Min}, space{Max - Min + 1}, block{Block}, initialized{false}
{
	PATELLA_ASSERT_VALUES(space > 0, space);
	PATELLA_ASSERT_VALUES(block >= 0, block);
	entry = TEntries(Offset(space));
}

// New storage of space + the step entries, Below of them before the old first one; min..max swapped into it.
template <class T> void TIndexedVector<T>::Grow(int Below, int Above)
{
	TEntries Help(Offset(space + Below + Above));
	for (int j = min; j <= max; j++)
		SwapIndexedEntries(Help[Offset(j - start + Below)], entry[Offset(j - start)]);
	entry.swap(Help);
	start -= Below;
	space += Below + Above;
}

template <class T> T &TIndexedVector<T>::operator()(int i)
{
	while (i < start)
		Grow(block == 0 ? space : block, 0);
	while (i >= start + space)
		Grow(0, block == 0 ? space : block);
	while (i < min) {
		min--;
		if (initialized)
			entry[Offset(min - start)] = init;
	}
	while (i > max) {
		max++;
		if (initialized)
			entry[Offset(max - start)] = init;
	}
	return entry[Offset(i - start)];
}
