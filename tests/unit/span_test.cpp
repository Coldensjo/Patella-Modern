// patella::TSpan (src/compat/span.h, issue #59): std::span in tests_unit, the C++17 stand-in of the oracle-x86 build
// in tests_unit_cxx26_standin (PATELLA_SPAN_STANDIN). The same tests hold for both, so the stand-in behaves as
// std::span where the code uses it.

#include <doctest/doctest.h>

#include "compat/span.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if PATELLA_HAS_SPAN
#include <span>
#endif

namespace {

using patella::DynamicExtent;
using patella::TSpan;

#if defined(PATELLA_SPAN_STANDIN)
static_assert(PATELLA_HAS_SPAN == 0, "PATELLA_SPAN_STANDIN selects the stand-in");
#else
// Every product preset's library has std::span (C++20).
static_assert(PATELLA_HAS_SPAN == 1, "the product presets have std::span");
static_assert(std::is_same_v<TSpan<std::uint8_t>, std::span<std::uint8_t>>);
static_assert(std::is_same_v<TSpan<const std::uint8_t, 16>, std::span<const std::uint8_t, 16>>);
static_assert(DynamicExtent == std::dynamic_extent);
#endif

static_assert(DynamicExtent == static_cast<std::size_t>(-1));
static_assert(TSpan<int>::extent == DynamicExtent && TSpan<int, 8>::extent == 8);
static_assert(std::is_same_v<TSpan<const int>::element_type, const int> && std::is_same_v<TSpan<const int>::value_type, int>);

// The layout: a pointer for a fixed extent, a pointer and a size for the dynamic one.
static_assert(patella::IsSpanLayout<std::uint8_t, DynamicExtent> && patella::IsSpanLayout<char, DynamicExtent>);
static_assert(patella::IsSpanLayout<std::uint8_t, 8> && patella::IsSpanLayout<const std::uint8_t, 16>);
static_assert(sizeof(TSpan<std::uint8_t, 128>) == sizeof(void *));

// Which conversions are implicit, explicit or absent, as for std::span.
static_assert(std::is_convertible_v<TSpan<int>, TSpan<const int>>);
static_assert(!std::is_constructible_v<TSpan<int>, TSpan<const int>>);
static_assert(std::is_convertible_v<TSpan<int, 4>, TSpan<int>>);
static_assert(std::is_convertible_v<TSpan<int, 4>, TSpan<const int, 4>>);
static_assert(!std::is_convertible_v<TSpan<int>, TSpan<int, 4>> && std::is_constructible_v<TSpan<int, 4>, TSpan<int>>);
static_assert(!std::is_constructible_v<TSpan<int, 4>, TSpan<int, 5>>);
static_assert(std::is_convertible_v<int (&)[4], TSpan<int>> && std::is_convertible_v<int (&)[4], TSpan<int, 4>>);
static_assert(std::is_convertible_v<int (&)[4], TSpan<const int, 4>>);
static_assert(!std::is_constructible_v<TSpan<int, 4>, int (&)[5]>);
static_assert(!std::is_constructible_v<TSpan<int>, const int (&)[4]>);
static_assert(std::is_convertible_v<std::array<int, 4> &, TSpan<int, 4>>);
static_assert(std::is_convertible_v<const std::array<int, 4> &, TSpan<const int>>);
static_assert(!std::is_constructible_v<TSpan<int>, const std::array<int, 4> &>);
static_assert(std::is_constructible_v<TSpan<int>, int *, std::size_t> && std::is_convertible_v<int *, int *>);
static_assert(std::is_constructible_v<TSpan<int, 4>, int *, std::size_t>);
static_assert(std::is_default_constructible_v<TSpan<int>> && std::is_default_constructible_v<TSpan<int, 0>>);
static_assert(!std::is_default_constructible_v<TSpan<int, 4>>);
static_assert(std::is_trivially_copyable_v<TSpan<int>> && std::is_trivially_copyable_v<TSpan<int, 4>>);

constexpr int Sum(TSpan<const int> Values)
{
	int Result = 0;
	for (const int Value : Values)
		Result += Value;
	return Result;
}

constexpr int Numbers[] = {1, 2, 3, 4, 5};
static_assert(Sum(Numbers) == 15);
static_assert(Sum(TSpan<const int>(Numbers).subspan(1, 3)) == 9);
static_assert(Sum(TSpan<const int>(Numbers).last<2>()) == 9);
static_assert(TSpan<const int>(Numbers).first<3>().back() == 3);

} // namespace

TEST_CASE("TSpan: empty")
{
	const TSpan<int> Empty;
	CHECK(Empty.data() == nullptr);
	CHECK(Empty.size() == 0);
	CHECK(Empty.empty());
	CHECK(Empty.begin() == Empty.end());
	const TSpan<int, 0> Zero;
	CHECK(Zero.data() == nullptr);
	CHECK(Zero.empty());
}

TEST_CASE("TSpan: from a pointer and a count, an array, a std::array")
{
	int Values[6] = {10, 11, 12, 13, 14, 15};
	const TSpan<int> FromPointer(&Values[1], 3);
	CHECK(FromPointer.data() == &Values[1]);
	CHECK(FromPointer.size() == 3);
	CHECK(FromPointer.size_bytes() == 3 * sizeof(int));
	CHECK(!FromPointer.empty());
	CHECK(FromPointer[0] == 11);
	CHECK(FromPointer.front() == 11);
	CHECK(FromPointer.back() == 13);

	const TSpan<int> FromArray(Values);
	CHECK(FromArray.data() == Values);
	CHECK(FromArray.size() == 6);
	const TSpan<int, 6> Fixed(Values);
	CHECK(Fixed.size() == 6);
	CHECK(Fixed.data() == Values);

	std::array<int, 3> Standard = {7, 8, 9};
	const TSpan<int, 3> FromStandard(Standard);
	CHECK(FromStandard.data() == Standard.data());
	const std::array<int, 3> &ConstStandard = Standard;
	const TSpan<const int> FromConstStandard(ConstStandard);
	CHECK(FromConstStandard.size() == 3);
	CHECK(FromConstStandard[2] == 9);

	// Writes go through to the elements.
	FromPointer[1] = 42;
	CHECK(Values[2] == 42);
	int Count = 0;
	for (int &Value : FromArray) {
		Value += 1;
		Count++;
	}
	CHECK(Count == 6);
	CHECK(Values[0] == 11);
	CHECK(Values[2] == 43);
}

TEST_CASE("TSpan: conversions keep the pointer and the size")
{
	int Values[4] = {1, 2, 3, 4};
	const TSpan<int, 4> Fixed(Values);
	const TSpan<int> Dynamic = Fixed;
	CHECK(Dynamic.data() == Values);
	CHECK(Dynamic.size() == 4);
	const TSpan<const int> Const = Dynamic;
	CHECK(Const.data() == Values);
	CHECK(Const.size() == 4);
	const TSpan<const int, 4> FixedAgain(Const);
	CHECK(FixedAgain.data() == Values);
	CHECK(FixedAgain.size() == 4);
}

TEST_CASE("TSpan: first, last, subspan")
{
	unsigned char Bytes[16];
	for (int i = 0; i < 16; i++)
		Bytes[i] = static_cast<unsigned char>(i);
	const TSpan<unsigned char> All(Bytes);

	const TSpan<unsigned char> First = All.first(5);
	CHECK(First.data() == Bytes);
	CHECK(First.size() == 5);
	const TSpan<unsigned char, 8> FirstFixed = All.first<8>();
	CHECK(FirstFixed.data() == Bytes);
	CHECK(FirstFixed.size() == 8);

	const TSpan<unsigned char> Last = All.last(3);
	CHECK(Last.data() == &Bytes[13]);
	CHECK(Last.size() == 3);
	const TSpan<unsigned char, 2> LastFixed = All.last<2>();
	CHECK(LastFixed[0] == 14);

	const TSpan<unsigned char> Rest = All.subspan(10);
	CHECK(Rest.data() == &Bytes[10]);
	CHECK(Rest.size() == 6);
	const TSpan<unsigned char> Middle = All.subspan(4, 8);
	CHECK(Middle.data() == &Bytes[4]);
	CHECK(Middle.size() == 8);
	CHECK(All.subspan(16).empty());
	CHECK(All.first(0).empty());

	// A fixed span's parts: the subspan of a block, as TXTEA's callers take the packets' blocks.
	const TSpan<unsigned char, 8> Block = All.subspan(8).first<8>();
	CHECK(Block.data() == &Bytes[8]);
	CHECK(Block.size() == 8);
	CHECK(FirstFixed.subspan(2, 3).size() == 3);
	CHECK(FirstFixed.first(4).data() == Bytes);
}

TEST_CASE("TSpan: a pointer and a count beyond INT_MAX are kept as they are")
{
	// Utils.cpp reads the size back as the original's int (issue #59); the span itself only stores it.
	unsigned char Byte = 0;
	const TSpan<unsigned char> Huge(&Byte, static_cast<std::size_t>(-1));
	CHECK(Huge.data() == &Byte);
	CHECK(Huge.size() == static_cast<std::size_t>(-1));
	CHECK(static_cast<int>(Huge.size()) == -1);
	const TSpan<unsigned char> Null(static_cast<unsigned char *>(nullptr), 4);
	CHECK(Null.data() == nullptr);
	CHECK(Null.size() == 4);
	const TSpan<unsigned char, 8> NullFixed(static_cast<unsigned char *>(nullptr), 8);
	CHECK(NullFixed.data() == nullptr);
}
