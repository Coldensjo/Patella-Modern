// TIndexedVector<T> of src/types/IndexedVector.h (issue #99), CipSoft's growable vector<T> over a std::vector: the
// range min..max and negative indices, growth by indexing (by block, or doubling when block is 0, below and above),
// the init fill (at construction and for every index the range takes in), the entries keeping their values and
// indices through growth, the padding a growth carries (the byte copies of issue #91), the storage's allocator (new
// T[n]'s), and the two assertions. The golden corpus's Container test compares the same object, entry for
// entry, with the original's instances; these tests hold its semantics on every compiler without the corpus.

#include <doctest/doctest.h>

#include "TMyException.h"
#include "types/IndexedVector.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>

namespace {

// An assertion's message names its own line (issue #79), so a message is compared as "<file> N: ...".
std::string WithoutLine(const std::string &Message)
{
	const std::string::size_type Space = Message.find(' ');
	const std::string::size_type Colon = Message.find(": ");
	if (Space == std::string::npos || Colon == std::string::npos || Colon < Space)
		return Message;
	return Message.substr(0, Space + 1) + "N" + Message.substr(Colon);
}

template <class F> std::string MessageOf(F Function)
{
	try {
		Function();
	} catch (const TMyException &Exception) {
		return WithoutLine(Exception.GetMessage());
	}
	return "no exception";
}

// A structure with padding after Tag, trivially copyable: a growth copies it as bytes, padding included (issue #91).
struct TPadded {
	char Tag;
	int Value;
};
static_assert(sizeof(TPadded) > sizeof(char) + sizeof(int), "TPadded has padding");
static_assert(std::is_trivially_copyable_v<TPadded>, "TPadded is copied as bytes");

// Not trivially copyable: a growth copies it with its assignment.
struct TCounted {
	static inline int Constructed = 0;
	static inline int Assigned = 0;
	int Value = 7;
	TCounted() { Constructed++; }
	TCounted(const TCounted &Other) : Value(Other.Value) {}
	TCounted &operator=(const TCounted &Other)
	{
		Value = Other.Value;
		Assigned++;
		return *this;
	}
	~TCounted() = default;
};
static_assert(!std::is_trivially_copyable_v<TCounted>, "TCounted is copied with its assignment");

} // namespace

TEST_CASE("TIndexedVector: the range and negative indices")
{
	TIndexedVector<int> V(-3, 2, 10, 0);
	CHECK(V.min == -3);
	CHECK(V.max == 2);
	CHECK(V.start == -3);
	CHECK(V.space == 6);
	CHECK(V.block == 10);
	CHECK(V.initialized);
	CHECK(V.entry.size() == 6);
	for (int i = -3; i <= 2; i++)
		V(i) = i * 10;
	for (int i = -3; i <= 2; i++)
		CHECK(V(i) == i * 10);
	// Index i is entry[i - start].
	CHECK(&V(-3) == &V.entry[0]);
	CHECK(&V(2) == &V.entry[5]);
	// Inside the storage nothing grows or moves.
	const int *First = V.entry.data();
	V(0) = 1;
	CHECK(V.entry.data() == First);
	CHECK(V.space == 6);
}

TEST_CASE("TIndexedVector: growth by block, above and below")
{
	TIndexedVector<int> V(0, 0, 3, -1);
	V(0) = 100;
	V(4) = 104; // above: 1 + 3 = 4 entries, then 7
	CHECK(V.space == 7);
	CHECK(V.start == 0);
	CHECK(V.min == 0);
	CHECK(V.max == 4);
	CHECK(V(0) == 100);
	CHECK(V(4) == 104);
	V(-2) = 98; // below: 7 + 3 = 10 entries, start -3
	CHECK(V.space == 10);
	CHECK(V.start == -3);
	CHECK(V.min == -2);
	CHECK(V.max == 4);
	CHECK(V(-2) == 98);
	CHECK(V(0) == 100);
	CHECK(V(4) == 104);
	CHECK(V.entry.size() == 10);
	CHECK(&V(-3) == &V.entry[0]);
}

TEST_CASE("TIndexedVector: growth doubles when block is 0")
{
	TIndexedVector<int> V(0, 1, 0, -1);
	CHECK(V(0) == -1);
	V(1) = 1;
	V(5) = 5; // doubles twice: space 2, 4, 8
	CHECK(V.space == 8);
	CHECK(V.max == 5);
	CHECK(V(4) == -1);
	CHECK(V(1) == 1);
	V(-3) = -3; // grows below by the space, 8
	CHECK(V.start == -8);
	CHECK(V.space == 16);
	CHECK(V.min == -3);
	CHECK(V(-1) == -1);
	CHECK(V(1) == 1);
	CHECK(V(5) == 5);
}

TEST_CASE("TIndexedVector: the init fill")
{
	// Every entry starts as init, and every index the range takes in is set to init first, even inside the storage.
	TIndexedVector<int> V(0, 1, 4, 42);
	CHECK(V.entry[0] == 42);
	CHECK(V.entry[1] == 42);
	V(3) = 3; // grows to 6; 2 and 3 are filled, 4 and 5 are outside the range
	CHECK(V.min == 0);
	CHECK(V.max == 3);
	CHECK(V(2) == 42);
	V.entry[5] = 5; // outside the range: taken in later, set to init then
	CHECK(V(5) == 42);
	CHECK(V(4) == 42);
	CHECK(V(3) == 3);

	// Taken in again below: min moves down one index at a time, each entry set to init.
	V(0) = 0;
	TIndexedVector<int> W(5, 5, 2, -7);
	W.entry[0] = 1;
	W(3) = 3;
	CHECK(W.min == 3);
	CHECK(W(4) == -7);
	CHECK(W(5) == 1);

	// Without init the entries outside the range keep what they hold when the range takes them in.
	TIndexedVector<int> X(0, 0, 4);
	CHECK_FALSE(X.initialized);
	X(0) = 1;
	X(1) = 2;
	X.entry[3] = 33;
	CHECK(X(3) == 33);
	CHECK(X.max == 3);
}

TEST_CASE("TIndexedVector: a growth carries the entries' padding")
{
	TIndexedVector<TPadded> V(0, 1, 1);
	unsigned char Bytes[sizeof(TPadded)];
	for (std::size_t k = 0; k < sizeof(Bytes); k++)
		Bytes[k] = static_cast<unsigned char>(0xa0 + k);
	std::memcpy(&V(1), Bytes, sizeof(Bytes));
	V(-1).Tag = 'x'; // below: new storage, entries 0 and 1 swapped into it
	V(2).Tag = 'y';  // above: again
	CHECK(V.start == -1);
	CHECK(V.space == 4);
	CHECK(std::memcmp(&V(1), Bytes, sizeof(Bytes)) == 0);
}

TEST_CASE("TIndexedVector: entries that are not trivially copyable")
{
	TCounted::Constructed = 0;
	TCounted::Assigned = 0;
	TIndexedVector<TCounted> V(0, 2, 2);
	CHECK(TCounted::Constructed == 4); // 3 entries and init, each default-constructed (new T[3], the member)
	V(1).Value = 11;
	V(3).Value = 13; // 5 new entries constructed; 3 entries swapped through a copy, 2 assignments each
	CHECK(TCounted::Constructed == 9);
	CHECK(TCounted::Assigned == 6);
	CHECK(V(1).Value == 11);
	CHECK(V(3).Value == 13);
	CHECK(V(4).Value == 7);
}

TEST_CASE("TIndexedVector: the storage's allocator")
{
	// new T[n]: operator new[], and an entry without arguments default-initialised (the constructor of a class, nothing
	// for an int: the golden corpus's Container test compares such entries' bytes, the pattern of its operator new[];
	// the language gives them no value a test here could read). With arguments a plain construction.
	TArrayNewAllocator<TCounted> Allocator;
	TCounted *Entries = Allocator.allocate(2);
	REQUIRE(Entries != nullptr);
	TCounted::Constructed = 0;
	Allocator.construct(&Entries[0]);
	CHECK(TCounted::Constructed == 1);
	CHECK(Entries[0].Value == 7);
	Allocator.construct(&Entries[1], Entries[0]);
	CHECK(TCounted::Constructed == 1); // the copy constructor
	CHECK(Entries[1].Value == 7);
	Entries[1].~TCounted();
	Entries[0].~TCounted();
	Allocator.deallocate(Entries, 2);
	CHECK(Allocator == TArrayNewAllocator<TCounted>());
	const TArrayNewAllocator<int> Rebound(Allocator);
	CHECK(Rebound == TArrayNewAllocator<int>());
}

TEST_CASE("TIndexedVector: the assertions")
{
	CHECK(MessageOf([] { const TIndexedVector<int> Bad(0, 0, -1); })
		== "IndexedVector.h N: assertion failed (block = -1), reason:\nblock >= 0");
	CHECK(MessageOf([] { const TIndexedVector<int> Empty(1, 0, 10); })
		== "IndexedVector.h N: assertion failed (space = 0), reason:\nspace > 0");
	CHECK(MessageOf([] { const TIndexedVector<int> Bad(0, 0, -1, 5); })
		== "IndexedVector.h N: assertion failed (block = -1), reason:\nblock >= 0");
	CHECK(MessageOf([] { const TIndexedVector<int> Fine(0, 0, 0); }) == "no exception");
}
