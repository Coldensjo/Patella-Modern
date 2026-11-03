// patella::TStaticVector of src/compat/cxx26.h: the same tests for std::inplace_vector (GCC 16) and
// the stand-in (MSVC, and GCC with PATELLA_CXX26_STANDIN), so the stand-in is checked against the
// real container's results and exceptions.

#include <doctest/doctest.h>

#include "compat/cxx26.h"

#include <compare>
#include <cstddef>
#include <iterator>
#include <memory>
#include <new>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using patella::TStaticVector;

namespace {

template <class T, std::size_t N>
std::vector<T> Contents(const TStaticVector<T, N> &Vector)
{
	return std::vector<T>(Vector.begin(), Vector.end());
}

using Ints = std::vector<int>;

// Counts the live objects, to check that every element constructed is destroyed exactly once.
struct TTracked {
	static inline int Live = 0;
	int Value;

	TTracked(int Init = 0) : Value(Init) { ++Live; }
	TTracked(const TTracked &Other) : Value(Other.Value) { ++Live; }
	TTracked(TTracked &&Other) noexcept : Value(Other.Value) { ++Live; }
	TTracked &operator=(const TTracked &) = default;
	TTracked &operator=(TTracked &&) noexcept = default;
	~TTracked() { --Live; }
	friend bool operator==(const TTracked &, const TTracked &) = default;
};

// Ordered by < only: <=> of the container synthesises a weak ordering from it.
struct TLessOnly {
	int Value;
	[[maybe_unused]] friend bool operator==(const TLessOnly &, const TLessOnly &) = default;
	friend bool operator<(const TLessOnly &Left, const TLessOnly &Right) { return Left.Value < Right.Value; }
};

// Throws from its copy constructor when told to.
struct TThrowing {
	static inline int CopiesLeft = 1000;
	static inline int Live = 0;
	int Value;

	TThrowing(int Init) : Value(Init) { ++Live; }
	TThrowing(const TThrowing &Other) : Value(Other.Value)
	{
		if (CopiesLeft-- <= 0)
			throw std::runtime_error("copy");
		++Live;
	}
	~TThrowing() { --Live; }
};

constexpr int ConstexprUse()
{
	TStaticVector<int, 8> Vector{3, 1, 2};
	Vector.push_back(5);
	Vector.insert(Vector.begin(), 9);
	Vector.erase(Vector.begin() + 1);
	TStaticVector<int, 8> Copy = Vector;
	Copy.pop_back();
	int Sum = 0;
	for (int Value : Copy)
		Sum = Sum * 10 + Value;
	return Sum + static_cast<int>(Vector.size()) * 10000;
}

} // namespace

TEST_CASE("cxx26.h selects the expected inplace_vector path")
{
#ifdef PATELLA_EXPECT_CXX26
	CHECK(PATELLA_HAS_INPLACE_VECTOR == PATELLA_EXPECT_CXX26);
#endif
#ifdef PATELLA_CXX26_STANDIN
	CHECK(PATELLA_HAS_INPLACE_VECTOR == 0);
#endif
	CHECK((PATELLA_HAS_INPLACE_VECTOR == 0 || PATELLA_HAS_INPLACE_VECTOR == 1));
}

TEST_CASE("TStaticVector: types and triviality")
{
	using V = TStaticVector<int, 4>;
	static_assert(std::is_same_v<V::value_type, int>);
	static_assert(std::is_same_v<V::size_type, std::size_t>);
	static_assert(std::is_same_v<V::difference_type, std::ptrdiff_t>);
	static_assert(std::is_same_v<V::reference, int &>);
	static_assert(std::is_same_v<V::const_reference, const int &>);
	static_assert(std::random_access_iterator<V::iterator>);
	static_assert(std::contiguous_iterator<V::iterator>);
	static_assert(std::ranges::contiguous_range<V>);
	static_assert(std::is_trivially_copyable_v<V>);
	static_assert(std::is_trivially_destructible_v<V>);
	static_assert(std::is_nothrow_default_constructible_v<V>);
	static_assert(!std::is_trivially_copyable_v<TStaticVector<std::string, 4>>);
	static_assert(!std::is_trivially_destructible_v<TStaticVector<std::string, 4>>);
	static_assert(std::is_nothrow_move_constructible_v<TStaticVector<std::string, 4>>);
	static_assert(V::capacity() == 4 && V::max_size() == 4);
	static_assert(TStaticVector<int, 0>::capacity() == 0);
	CHECK(V().empty());
}

TEST_CASE("TStaticVector: constant evaluation")
{
	static_assert(ConstexprUse() == 40912);
	CHECK(ConstexprUse() == 40912);
}

TEST_CASE("TStaticVector: construction")
{
	CHECK(Contents(TStaticVector<int, 4>(3)) == Ints{0, 0, 0});
	CHECK(Contents(TStaticVector<int, 4>(2, 7)) == Ints{7, 7});
	CHECK(Contents(TStaticVector<int, 4>{1, 2, 3}) == Ints{1, 2, 3});
	const Ints Source{4, 5, 6};
	CHECK(Contents(TStaticVector<int, 4>(Source.begin(), Source.end())) == Source);
	CHECK(Contents(TStaticVector<int, 4>(std::from_range, Source)) == Source);
	CHECK(Contents(TStaticVector<int, 4>(std::from_range, std::views::iota(1, 4))) == Ints{1, 2, 3});

	// Input iterators (the distance is not known beforehand), and an input range.
	std::istringstream Stream("8 9 10");
	CHECK(Contents(TStaticVector<int, 4>(std::istream_iterator<int>(Stream), std::istream_iterator<int>())) == Ints{8, 9, 10});
	std::istringstream Stream2("11 12");
	CHECK(Contents(TStaticVector<int, 4>(std::from_range, std::views::istream<int>(Stream2))) == Ints{11, 12});

	// Beyond the capacity: std::bad_alloc.
	using Two = TStaticVector<int, 2>;
	CHECK_THROWS_AS(Two(3), std::bad_alloc);
	CHECK_THROWS_AS(Two(3, 1), std::bad_alloc);
	CHECK_THROWS_AS((Two{1, 2, 3}), std::bad_alloc);
	CHECK_THROWS_AS(Two(Source.begin(), Source.end()), std::bad_alloc);
	std::istringstream Stream3("1 2 3");
	CHECK_THROWS_AS(Two(std::istream_iterator<int>(Stream3), std::istream_iterator<int>()), std::bad_alloc);
	CHECK_NOTHROW(Two(2));
}

TEST_CASE("TStaticVector: copy, move and element lifetimes")
{
	TTracked::Live = 0;
	{
		TStaticVector<TTracked, 4> Vector{1, 2, 3};
		CHECK(TTracked::Live == 3);
		TStaticVector<TTracked, 4> Copy = Vector;
		CHECK(TTracked::Live == 6);
		CHECK(Copy == Vector);
		TStaticVector<TTracked, 4> Moved = std::move(Copy);
		CHECK(Moved == Vector);
		TStaticVector<TTracked, 4> Assigned{9};
		Assigned = Vector;
		CHECK(Assigned == Vector);
		Assigned = TStaticVector<TTracked, 4>{7};
		CHECK(Assigned.size() == 1);
		CHECK(Assigned[0].Value == 7);
		Assigned = {4, 5, 6, 7};
		CHECK(Assigned.size() == 4);
		Assigned.pop_back();
		Assigned.erase(Assigned.begin());
		CHECK(Assigned.size() == 2);
	}
	CHECK(TTracked::Live == 0);

	// A copy that throws part way destroys what it had constructed.
	TThrowing::Live = 0;
	TThrowing::CopiesLeft = 1000;
	{
		using TThrowings = TStaticVector<TThrowing, 4>;
		TThrowings Vector;
		Vector.emplace_back(1);
		Vector.emplace_back(2);
		Vector.emplace_back(3);
		TThrowing::CopiesLeft = 2;
		CHECK_THROWS_AS(TThrowings(Vector.begin(), Vector.end()), std::runtime_error);
		CHECK(TThrowing::Live == 3);
		TThrowing::CopiesLeft = 1000;
	}
	CHECK(TThrowing::Live == 0);

	// Move-only elements.
	TStaticVector<std::unique_ptr<int>, 3> Pointers;
	Pointers.push_back(std::make_unique<int>(1));
	Pointers.emplace_back(std::make_unique<int>(3));
	Pointers.insert(Pointers.begin() + 1, std::make_unique<int>(2));
	CHECK(*Pointers[0] == 1);
	CHECK(*Pointers[1] == 2);
	CHECK(*Pointers[2] == 3);
	TStaticVector<std::unique_ptr<int>, 3> Taken = std::move(Pointers);
	CHECK(*Taken.back() == 3);
	Taken.erase(Taken.begin());
	CHECK(*Taken.front() == 2);
}

TEST_CASE("TStaticVector: element access")
{
	TStaticVector<int, 4> Vector{1, 2, 3};
	const auto &Const = Vector;
	CHECK(Vector[1] == 2);
	CHECK(Const[2] == 3);
	CHECK(Vector.at(0) == 1);
	CHECK(Const.at(2) == 3);
	CHECK_THROWS_AS((void)Vector.at(3), std::out_of_range);
	CHECK_THROWS_AS((void)Const.at(4), std::out_of_range);
	CHECK(Vector.front() == 1);
	CHECK(Const.back() == 3);
	CHECK(Vector.data() == &Vector[0]);
	CHECK(*Const.data() == 1);
	CHECK(Ints(Vector.rbegin(), Vector.rend()) == Ints{3, 2, 1});
	CHECK(Ints(Const.crbegin(), Const.crend()) == Ints{3, 2, 1});
	CHECK(Vector.cend() - Vector.cbegin() == 3);
	Vector.front() = 10;
	Vector.back() = 30;
	CHECK(Contents(Vector) == Ints{10, 2, 30});
}

TEST_CASE("TStaticVector: appending")
{
	TStaticVector<int, 3> Vector;
	int *First = &Vector.push_back(1);
	CHECK(First == Vector.data());
	const int Two = 2;
	Vector.push_back(Two);
	CHECK(Vector.emplace_back(3) == 3);
	CHECK_THROWS_AS(Vector.push_back(4), std::bad_alloc);
	CHECK_THROWS_AS(Vector.emplace_back(4), std::bad_alloc);
	CHECK(Contents(Vector) == Ints{1, 2, 3});

	// try_: nothing when full; usable as `if (auto Element = ...)`, `*Element` and `Element->`.
	TStaticVector<std::string, 2> Strings;
	if (auto Element = Strings.try_push_back("a"))
		CHECK(*Element == "a");
	else
		FAIL("try_push_back failed below the capacity");
	auto Second = Strings.try_emplace_back(3, 'b');
	REQUIRE(static_cast<bool>(Second));
	CHECK(Second->size() == 3);
	const std::string Third = "c";
	CHECK(!Strings.try_push_back(Third));
	CHECK(!Strings.try_emplace_back("d"));
	CHECK(Strings.size() == 2);

	TStaticVector<int, 2> Unchecked;
	Unchecked.unchecked_push_back(5);
	CHECK(Unchecked.unchecked_emplace_back(6) == 6);
	CHECK(Contents(Unchecked) == Ints{5, 6});

	TStaticVector<int, 5> Ranges{1};
	Ranges.append_range(Ints{2, 3});
	Ranges.append_range(std::views::iota(4, 6));
	CHECK(Contents(Ranges) == Ints{1, 2, 3, 4, 5});
	CHECK_THROWS_AS(Ranges.append_range(Ints{6}), std::bad_alloc);
	CHECK(Ranges.size() == 5);

	// A sized range beyond the capacity changes nothing; an input range fills it, then throws.
	TStaticVector<int, 3> Partial{1};
	CHECK_THROWS_AS(Partial.append_range(Ints{2, 3, 4}), std::bad_alloc);
	CHECK(Contents(Partial) == Ints{1});
	std::istringstream Stream("2 3 4");
	CHECK_THROWS_AS(Partial.append_range(std::views::istream<int>(Stream)), std::bad_alloc);
	CHECK(Contents(Partial) == Ints{1, 2, 3});
}

TEST_CASE("TStaticVector: inserting")
{
	TStaticVector<int, 8> Vector{1, 5};
	auto Position = Vector.insert(Vector.begin() + 1, 3);
	CHECK(Position == Vector.begin() + 1);
	CHECK(Contents(Vector) == Ints{1, 3, 5});
	const int Zero = 0;
	CHECK(*Vector.insert(Vector.begin(), Zero) == 0);
	CHECK(*Vector.emplace(Vector.end(), 6) == 6);
	CHECK(Contents(Vector) == Ints{0, 1, 3, 5, 6});
	Position = Vector.insert(Vector.begin() + 2, 2, 2);
	CHECK(Position - Vector.begin() == 2);
	CHECK(Contents(Vector) == Ints{0, 1, 2, 2, 3, 5, 6});
	CHECK_THROWS_AS(Vector.insert(Vector.begin(), 2, 9), std::bad_alloc);
	CHECK(Contents(Vector) == Ints{0, 1, 2, 2, 3, 5, 6});
	Position = Vector.insert(Vector.begin() + 5, {4});
	CHECK(*Position == 4);
	CHECK(Contents(Vector) == Ints{0, 1, 2, 2, 3, 4, 5, 6});
	CHECK_THROWS_AS(Vector.insert(Vector.begin(), 9), std::bad_alloc);
	CHECK_THROWS_AS(Vector.emplace(Vector.begin(), 9), std::bad_alloc);
	CHECK(Vector.size() == 8);

	TStaticVector<int, 6> Ranges{1, 6};
	const Ints Middle{2, 3};
	auto RangePosition = Ranges.insert(Ranges.begin() + 1, Middle.begin(), Middle.end());
	CHECK(RangePosition == Ranges.begin() + 1);
	RangePosition = Ranges.insert_range(Ranges.begin() + 3, std::views::iota(4, 6));
	CHECK(*RangePosition == 4);
	CHECK(Contents(Ranges) == Ints{1, 2, 3, 4, 5, 6});
	CHECK_THROWS_AS(Ranges.insert_range(Ranges.begin(), Ints{0}), std::bad_alloc);
	CHECK(Contents(Ranges) == Ints{1, 2, 3, 4, 5, 6});
	RangePosition = Ranges.insert(Ranges.begin(), Middle.begin(), Middle.begin());
	CHECK(RangePosition == Ranges.begin());

	TStaticVector<int, 4> FromInput{1, 4};
	std::istringstream Stream("2 3");
	auto InputPosition = FromInput.insert(FromInput.begin() + 1, std::istream_iterator<int>(Stream), std::istream_iterator<int>());
	CHECK(InputPosition == FromInput.begin() + 1);
	CHECK(Contents(FromInput) == Ints{1, 2, 3, 4});
}

TEST_CASE("TStaticVector: erasing")
{
	TStaticVector<int, 8> Vector{0, 1, 2, 3, 4, 5};
	auto Position = Vector.erase(Vector.begin() + 1);
	CHECK(*Position == 2);
	CHECK(Contents(Vector) == Ints{0, 2, 3, 4, 5});
	Position = Vector.erase(Vector.begin() + 1, Vector.begin() + 3);
	CHECK(*Position == 4);
	CHECK(Contents(Vector) == Ints{0, 4, 5});
	Position = Vector.erase(Vector.begin(), Vector.begin());
	CHECK(Position == Vector.begin());
	Position = Vector.erase(Vector.end() - 1);
	CHECK(Position == Vector.end());
	Vector.pop_back();
	CHECK(Contents(Vector) == Ints{0});
	Vector.clear();
	CHECK(Vector.empty());

	// erase and erase_if as std's, found by argument-dependent lookup on both paths.
	TStaticVector<int, 8> Values{1, 2, 3, 2, 5, 2};
	CHECK(erase(Values, 2) == 3);
	CHECK(Contents(Values) == Ints{1, 3, 5});
	CHECK(erase_if(Values, [](int Value) { return Value > 2; }) == 2);
	CHECK(Contents(Values) == Ints{1});
}

TEST_CASE("TStaticVector: assign, resize, reserve")
{
	TStaticVector<int, 4> Vector{1, 2, 3};
	Vector.assign(2, 9);
	CHECK(Contents(Vector) == Ints{9, 9});
	Vector.assign(4, 8);
	CHECK(Contents(Vector) == Ints{8, 8, 8, 8});
	const Ints Source{1, 2, 3};
	Vector.assign(Source.begin(), Source.end());
	CHECK(Contents(Vector) == Source);
	Vector.assign({5});
	CHECK(Contents(Vector) == Ints{5});
	Vector.assign_range(std::views::iota(1, 5));
	CHECK(Contents(Vector) == Ints{1, 2, 3, 4});
	std::istringstream Stream("7 8");
	Vector.assign(std::istream_iterator<int>(Stream), std::istream_iterator<int>());
	CHECK(Contents(Vector) == Ints{7, 8});
	CHECK_THROWS_AS(Vector.assign(5, 1), std::bad_alloc);
	CHECK_THROWS_AS(Vector.assign_range(Ints{1, 2, 3, 4, 5}), std::bad_alloc);
	CHECK(Contents(Vector) == Ints{7, 8});
	CHECK_THROWS_AS((Vector = {1, 2, 3, 4, 5}), std::bad_alloc);

	Vector.resize(4);
	CHECK(Contents(Vector) == Ints{7, 8, 0, 0});
	Vector.resize(1);
	CHECK(Contents(Vector) == Ints{7});
	Vector.resize(3, 6);
	CHECK(Contents(Vector) == Ints{7, 6, 6});
	CHECK_THROWS_AS(Vector.resize(5), std::bad_alloc);
	CHECK_THROWS_AS(Vector.resize(5, 1), std::bad_alloc);
	CHECK(Contents(Vector) == Ints{7, 6, 6});

	CHECK_NOTHROW(Vector.reserve(4));
	CHECK_THROWS_AS(Vector.reserve(5), std::bad_alloc);
	Vector.shrink_to_fit();
	CHECK(Vector.capacity() == 4);
}

TEST_CASE("TStaticVector: swap")
{
	TTracked::Live = 0;
	{
		TStaticVector<TTracked, 4> Small{1};
		TStaticVector<TTracked, 4> Big{2, 3, 4};
		Small.swap(Big);
		CHECK(Small.size() == 3);
		CHECK(Small[0].Value == 2);
		CHECK(Small[2].Value == 4);
		CHECK(Big.size() == 1);
		CHECK(Big[0].Value == 1);
		swap(Small, Big);
		CHECK(Small.size() == 1);
		CHECK(Big[1].Value == 3);
		CHECK(TTracked::Live == 4);
	}
	CHECK(TTracked::Live == 0);
}

TEST_CASE("TStaticVector: comparison")
{
	using V = TStaticVector<int, 4>;
	CHECK(V{1, 2} == V{1, 2});
	CHECK(V{1, 2} != V{1, 3});
	CHECK(V{1, 2} < V{1, 3});
	CHECK(V{1, 2} < V{1, 2, 0});
	CHECK(V{2} > V{1, 9});
	CHECK(V{} <= V{});
	static_assert(std::is_same_v<decltype(V{} <=> V{}), std::strong_ordering>);
	CHECK((V{1} <=> V{1, 0}) == std::strong_ordering::less);

	using L = TStaticVector<TLessOnly, 4>;
	static_assert(std::is_same_v<decltype(L{} <=> L{}), std::weak_ordering>);
	CHECK((L{{1}, {2}} <=> L{{1}, {3}}) == std::weak_ordering::less);
	CHECK((L{{2}} <=> L{{2}}) == std::weak_ordering::equivalent);
}

TEST_CASE("TStaticVector: capacity 0")
{
	TStaticVector<int, 0> Vector;
	CHECK(Vector.empty());
	CHECK(Vector.size() == 0);
	CHECK(Vector.begin() == Vector.end());
	CHECK_THROWS_AS(Vector.push_back(1), std::bad_alloc);
	CHECK(!Vector.try_push_back(1));
	CHECK_THROWS_AS(Vector.resize(1), std::bad_alloc);
	CHECK_NOTHROW(Vector.resize(0));
	CHECK(Vector == TStaticVector<int, 0>{});
}
