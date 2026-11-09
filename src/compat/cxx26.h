// cxx26.h: the C++26 features Patella-Modern uses, each the real feature where the compiler and its
// library have it and a C++23 stand-in elsewhere, selected by the feature's test macro
// (notes/modernization-plan.md, sections 1.1 and 8; issue #9). Not part of the original source.
//
// The code reaches a C++26 feature only through this header. Today GCC 16 has both features below
// (with -std=c++26, which cmake/Warnings.cmake gives GCC and Clang); MSVC 19.51 and Clang 23 (with
// GCC 16's libstdc++, below) have neither and get the stand-ins. When a compiler catches up, its
// stand-in goes and the name stays as it is.
//
// PATELLA_CXX26_STANDIN, defined for a whole target (never for one file: the two paths are not
// interchangeable inside one program), selects every stand-in also where the real feature exists;
// tests/unit builds the unit tests once that way, so the stand-ins are checked against the real
// features on the same compiler. After this header:
//	PATELLA_HAS_CONTRACTS        1: CONTRACT_ASSERT is contract_assert; 0: the stand-in
//	PATELLA_HAS_INPLACE_VECTOR   1: TStaticVector is std::inplace_vector; 0: the stand-in
//
// - CONTRACT_ASSERT(Predicate);  a contract assertion with the evaluation semantic "enforce" (GCC's
//   default, the only one the project uses): the predicate is evaluated; when it is false, or when
//   it throws, a message goes to stderr and the program ends through std::terminate. Usable in
//   constexpr functions: a violation during constant evaluation is a compile error. The stand-in
//   prints the first line of GCC's message ("contract violation in function F at FILE:LINE:
//   PREDICATE"), and the second with the mode the standard names (GCC 16.2 says predicate_false also
//   when the predicate threw). What only the real feature does: local variables are const inside
//   the predicate, other semantics (-fcontract-evaluation-semantic) and a replaceable violation
//   handler. GCC 16 keeps its default handler in libstdc++exp, which patella_target_language links.
// - patella::TStaticVector<T, N>: std::inplace_vector, a vector with the capacity N inside the
//   object (no allocation). The stand-in has the interface of libstdc++'s (__cpp_lib_inplace_vector
//   202603) with the same results and exceptions (std::bad_alloc beyond the capacity,
//   std::out_of_range from at()), the triviality of the copy, move and destructor for trivial T, and
//   constexpr use for trivial T. It differs in what portable code does not depend on: the iterators
//   are pointers; try_emplace_back and try_push_back return T * (nullptr when full) where the real one
//   returns std::optional<T &> (use only `if (auto Element = V.try_push_back(X))`, `*Element` and
//   `Element->`); N == 0 is not an empty type; the preconditions of operator[], front, back,
//   pop_back, the unchecked_ functions and the positions are CONTRACT_ASSERTs; sizeof differs (never
//   in a frozen layout, plan 1.2).

#pragma once

#include <version> // IWYU pragma: keep (the feature-test macros of the #if lines)

#if defined(__cpp_contracts) && __cpp_contracts >= 202502L && !defined(PATELLA_CXX26_STANDIN)
#define PATELLA_HAS_CONTRACTS 1
#else
#define PATELLA_HAS_CONTRACTS 0
#endif

// Not Clang with libstdc++ (linux-x64-clang since issue #121: Clang 23 with GCC 16's library, which says
// 202603): libstdc++ 16's inplace_vector is GCC's. Under Clang 23 its _M_init names is_trivial_v, deprecated in
// C++26, in a template the code instantiates (-Wdeprecated-declarations, an error under -Werror), and its constexpr
// copy is not a constant expression ("subobject '_M_elems' is not initialized"). Clang keeps the stand-in.
#if defined(__clang__) && defined(__GLIBCXX__)
#define PATELLA_HAS_INPLACE_VECTOR 0
#elif defined(__cpp_lib_inplace_vector) && __cpp_lib_inplace_vector >= 202603L && !defined(PATELLA_CXX26_STANDIN)
#define PATELLA_HAS_INPLACE_VECTOR 1
#else
#define PATELLA_HAS_INPLACE_VECTOR 0
#endif

// ---------------------------------------------------------------------------------------------------
// Contract assertions

#if PATELLA_HAS_CONTRACTS

#define CONTRACT_ASSERT(...) contract_assert(__VA_ARGS__)

#else

#include <cstdio>
#include <exception>
#include <print>
#include <source_location>

namespace patella::cxx26 {

// The violation of a CONTRACT_ASSERT: GCC's message, then std::terminate (inside the handler of the
// exception when the predicate threw, as GCC does).
// NOLINTNEXTLINE(bugprone-exception-escape): an exception of std::print ends in std::terminate, as the violation does
[[noreturn]] inline void ContractViolation(const char *Predicate, bool Threw, const std::source_location &Location) noexcept
{
	std::print(stderr,
		"contract violation in function {} at {}:{}: {}\n"
		"[assertion_kind: assert, semantic: enforce, mode: {}, terminating: yes]\n",
		Location.function_name(), Location.file_name(), Location.line(), Predicate,
		Threw ? "evaluation_exception" : "predicate_false");
	std::fflush(stderr);
	std::terminate();
}

} // namespace patella::cxx26

// static_cast<bool> as the predicate's contextual conversion; a top-level comma is an error, as in
// contract_assert.
#define CONTRACT_ASSERT(...) \
	do { \
		try { \
			if (!static_cast<bool>(__VA_ARGS__)) \
				::patella::cxx26::ContractViolation(#__VA_ARGS__, false, std::source_location::current()); \
		} catch (...) { \
			::patella::cxx26::ContractViolation(#__VA_ARGS__, true, std::source_location::current()); \
		} \
	} while (false)

#endif

// ---------------------------------------------------------------------------------------------------
// TStaticVector

#if PATELLA_HAS_INPLACE_VECTOR

#include <cstddef>
#include <inplace_vector>

namespace patella {

template <class T, std::size_t N>
using TStaticVector = std::inplace_vector<T, N>;

} // namespace patella

#else

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace patella {

namespace cxx26 {

// An iterator of the (legacy) input category, as the standard containers' iterator-pair members take.
template <class Iterator>
concept InputIterator = requires { typename std::iterator_traits<Iterator>::iterator_category; }
	&& std::derived_from<typename std::iterator_traits<Iterator>::iterator_category, std::input_iterator_tag>;

// A range whose elements convert to T (the standard's container-compatible-range).
template <class Range, class T>
concept CompatibleRange = std::ranges::input_range<Range> && std::convertible_to<std::ranges::range_reference_t<Range>, T>;

// An iterator of the (legacy) forward category.
template <class Iterator>
concept ForwardIterator = InputIterator<Iterator>
	&& std::derived_from<typename std::iterator_traits<Iterator>::iterator_category, std::forward_iterator_tag>;

// Whether the number of elements between First and Last is known before they are read.
template <class Iterator, class Sentinel>
concept KnownDistance = std::sized_sentinel_for<Sentinel, Iterator> || std::forward_iterator<Iterator>
	|| (std::same_as<Iterator, Sentinel> && ForwardIterator<Iterator>);

} // namespace cxx26

template <class T, std::size_t N>
class TStaticVector {
public:
	using value_type = T;
	using pointer = T *;
	using const_pointer = const T *;
	using reference = T &;
	using const_reference = const T &;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using iterator = T *;
	using const_iterator = const T *;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// Construction, copy and destruction. The constructors that add elements delegate to the default
	// one, so the destructor cleans up when an element's constructor throws.
	constexpr TStaticVector() noexcept { Init(); }

	constexpr explicit TStaticVector(size_type Count) : TStaticVector()
	{
		Reserve(Count);
		while (Length < Count)
			ConstructBack();
	}

	constexpr TStaticVector(size_type Count, const T &Value) : TStaticVector()
	{
		Reserve(Count);
		while (Length < Count)
			ConstructBack(Value);
	}

	template <cxx26::InputIterator Iterator>
	constexpr TStaticVector(Iterator First, Iterator Last) : TStaticVector()
	{
		AppendFrom(First, Last);
	}

	template <cxx26::CompatibleRange<T> Range>
	constexpr TStaticVector(std::from_range_t, Range &&Elements) : TStaticVector()
	{
		AppendFrom(std::ranges::begin(Elements), std::ranges::end(Elements));
	}

	constexpr TStaticVector(std::initializer_list<T> Elements) : TStaticVector()
	{
		AppendFrom(Elements.begin(), Elements.end());
	}

	TStaticVector(const TStaticVector &)
		requires std::is_trivially_copy_constructible_v<T>
	= default;

	constexpr TStaticVector(const TStaticVector &Other) noexcept(std::is_nothrow_copy_constructible_v<T>) : TStaticVector()
	{
		AppendFrom(Other.begin(), Other.end());
	}

	TStaticVector(TStaticVector &&)
		requires std::is_trivially_move_constructible_v<T>
	= default;

	constexpr TStaticVector(TStaticVector &&Other) noexcept(std::is_nothrow_move_constructible_v<T>) : TStaticVector()
	{
		AppendFrom(std::make_move_iterator(Other.begin()), std::make_move_iterator(Other.end()));
	}

	~TStaticVector()
		requires std::is_trivially_destructible_v<T>
	= default;

	constexpr ~TStaticVector() { clear(); }

	TStaticVector &operator=(const TStaticVector &)
		requires std::is_trivially_copy_assignable_v<T> && std::is_trivially_copy_constructible_v<T>
		&& std::is_trivially_destructible_v<T>
	= default;

	constexpr TStaticVector &operator=(const TStaticVector &Other) noexcept(
		std::is_nothrow_copy_assignable_v<T> && std::is_nothrow_copy_constructible_v<T>)
	{
		if (std::addressof(Other) != this)
			AssignFrom(Other.begin(), Other.end());
		return *this;
	}

	TStaticVector &operator=(TStaticVector &&)
		requires std::is_trivially_move_assignable_v<T> && std::is_trivially_move_constructible_v<T>
		&& std::is_trivially_destructible_v<T>
	= default;

	constexpr TStaticVector &operator=(TStaticVector &&Other) noexcept(
		std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_constructible_v<T>)
	{
		if (std::addressof(Other) != this)
			AssignFrom(std::make_move_iterator(Other.begin()), std::make_move_iterator(Other.end()));
		return *this;
	}

	constexpr TStaticVector &operator=(std::initializer_list<T> Elements)
	{
		AssignFrom(Elements.begin(), Elements.end());
		return *this;
	}

	template <cxx26::InputIterator Iterator>
	constexpr void assign(Iterator First, Iterator Last)
	{
		AssignFrom(First, Last);
	}

	template <cxx26::CompatibleRange<T> Range>
	constexpr void assign_range(Range &&Elements)
	{
		AssignFrom(std::ranges::begin(Elements), std::ranges::end(Elements));
	}

	constexpr void assign(size_type Count, const T &Value)
	{
		Reserve(Count);
		size_type Index = 0;
		for (; Index < Length && Index < Count; ++Index)
			Slots[Index] = Value;
		DestroyFrom(Index);
		while (Length < Count)
			ConstructBack(Value);
	}

	constexpr void assign(std::initializer_list<T> Elements) { AssignFrom(Elements.begin(), Elements.end()); }

	// Iterators.
	[[nodiscard]] constexpr iterator begin() noexcept { return data(); }
	[[nodiscard]] constexpr const_iterator begin() const noexcept { return data(); }
	[[nodiscard]] constexpr iterator end() noexcept { return data() + Length; }
	[[nodiscard]] constexpr const_iterator end() const noexcept { return data() + Length; }
	[[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	[[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	[[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
	[[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	[[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
	[[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }
	[[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
	[[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

	// Size and capacity. The capacity never changes: reserve only checks it.
	[[nodiscard]] constexpr bool empty() const noexcept { return Length == 0; }
	[[nodiscard]] constexpr size_type size() const noexcept { return Length; }
	[[nodiscard]] static constexpr size_type max_size() noexcept { return N; }
	[[nodiscard]] static constexpr size_type capacity() noexcept { return N; }

	constexpr void resize(size_type Count)
	{
		Reserve(Count);
		DestroyFrom(Count);
		while (Length < Count)
			ConstructBack();
	}

	constexpr void resize(size_type Count, const T &Value)
	{
		Reserve(Count);
		DestroyFrom(Count);
		while (Length < Count)
			ConstructBack(Value);
	}

	static constexpr void reserve(size_type Count) { Reserve(Count); }
	static constexpr void shrink_to_fit() noexcept {}

	// Element access.
	[[nodiscard]] constexpr reference operator[](size_type Index)
	{
		CONTRACT_ASSERT(Index < Length);
		return Slots[Index];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type Index) const
	{
		CONTRACT_ASSERT(Index < Length);
		return Slots[Index];
	}

	[[nodiscard]] constexpr reference at(size_type Index)
	{
		if (Index >= Length)
			throw std::out_of_range("TStaticVector::at: index out of range");
		return Slots[Index];
	}

	[[nodiscard]] constexpr const_reference at(size_type Index) const
	{
		if (Index >= Length)
			throw std::out_of_range("TStaticVector::at: index out of range");
		return Slots[Index];
	}

	[[nodiscard]] constexpr reference front()
	{
		CONTRACT_ASSERT(Length > 0);
		return Slots[0];
	}

	[[nodiscard]] constexpr const_reference front() const
	{
		CONTRACT_ASSERT(Length > 0);
		return Slots[0];
	}

	[[nodiscard]] constexpr reference back()
	{
		CONTRACT_ASSERT(Length > 0);
		return Slots[Length - 1];
	}

	[[nodiscard]] constexpr const_reference back() const
	{
		CONTRACT_ASSERT(Length > 0);
		return Slots[Length - 1];
	}

	[[nodiscard]] constexpr T *data() noexcept { return Slots; }
	[[nodiscard]] constexpr const T *data() const noexcept { return Slots; }

	// Modifiers. Beyond the capacity they throw std::bad_alloc; the try_ functions return nullptr
	// instead, the unchecked_ ones require the room.
	template <class... Arguments>
	constexpr T &emplace_back(Arguments &&...Values)
	{
		if (Length >= N)
			throw std::bad_alloc();
		return ConstructBack(std::forward<Arguments>(Values)...);
	}

	constexpr T &push_back(const T &Value) { return emplace_back(Value); }
	constexpr T &push_back(T &&Value) { return emplace_back(std::move(Value)); }

	template <cxx26::CompatibleRange<T> Range>
	constexpr void append_range(Range &&Elements)
	{
		AppendFrom(std::ranges::begin(Elements), std::ranges::end(Elements));
	}

	constexpr void pop_back()
	{
		CONTRACT_ASSERT(Length > 0);
		DestroyFrom(Length - 1);
	}

	template <class... Arguments>
	constexpr T *try_emplace_back(Arguments &&...Values)
	{
		if (Length >= N)
			return nullptr;
		return std::addressof(ConstructBack(std::forward<Arguments>(Values)...));
	}

	constexpr T *try_push_back(const T &Value) { return try_emplace_back(Value); }
	constexpr T *try_push_back(T &&Value) { return try_emplace_back(std::move(Value)); }

	template <class... Arguments>
	constexpr T &unchecked_emplace_back(Arguments &&...Values)
	{
		CONTRACT_ASSERT(Length < N);
		return ConstructBack(std::forward<Arguments>(Values)...);
	}

	constexpr T &unchecked_push_back(const T &Value) { return unchecked_emplace_back(Value); }
	constexpr T &unchecked_push_back(T &&Value) { return unchecked_emplace_back(std::move(Value)); }

	// Insertion before Position: the new elements are constructed at the end and rotated into place.
	template <class... Arguments>
	constexpr iterator emplace(const_iterator Position, Arguments &&...Values)
	{
		const size_type Before = Offset(Position);
		if (Length >= N)
			throw std::bad_alloc();
		ConstructBack(std::forward<Arguments>(Values)...);
		std::rotate(begin() + Before, end() - 1, end());
		return begin() + Before;
	}

	constexpr iterator insert(const_iterator Position, const T &Value) { return emplace(Position, Value); }
	constexpr iterator insert(const_iterator Position, T &&Value) { return emplace(Position, std::move(Value)); }

	constexpr iterator insert(const_iterator Position, size_type Count, const T &Value)
	{
		const size_type Before = Offset(Position);
		if (Count > N - Length)
			throw std::bad_alloc();
		const size_type OldLength = Length;
		for (size_type Index = 0; Index < Count; ++Index)
			ConstructBack(Value);
		std::rotate(begin() + Before, begin() + OldLength, end());
		return begin() + Before;
	}

	template <cxx26::InputIterator Iterator>
	constexpr iterator insert(const_iterator Position, Iterator First, Iterator Last)
	{
		return InsertFrom(Position, First, Last);
	}

	template <cxx26::CompatibleRange<T> Range>
	constexpr iterator insert_range(const_iterator Position, Range &&Elements)
	{
		return InsertFrom(Position, std::ranges::begin(Elements), std::ranges::end(Elements));
	}

	constexpr iterator insert(const_iterator Position, std::initializer_list<T> Elements)
	{
		return InsertFrom(Position, Elements.begin(), Elements.end());
	}

	constexpr iterator erase(const_iterator Position)
	{
		const size_type Index = Offset(Position);
		CONTRACT_ASSERT(Index < Length);
		std::move(begin() + Index + 1, end(), begin() + Index);
		pop_back();
		return begin() + Index;
	}

	constexpr iterator erase(const_iterator First, const_iterator Last)
	{
		const size_type Index = Offset(First);
		CONTRACT_ASSERT(First <= Last && Last <= cend());
		const auto Count = static_cast<size_type>(Last - First);
		std::move(begin() + Index + Count, end(), begin() + Index);
		DestroyFrom(Length - Count);
		return begin() + Index;
	}

	constexpr void swap(TStaticVector &Other) noexcept(
		std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<T>)
	{
		TStaticVector *Smaller = Other.Length < Length ? std::addressof(Other) : this;
		TStaticVector *Bigger = Other.Length < Length ? this : std::addressof(Other);
		const size_type Common = Smaller->Length;
		while (Smaller->Length < Bigger->Length)
			Smaller->ConstructBack(std::move_if_noexcept(Bigger->Slots[Smaller->Length]));
		Bigger->DestroyFrom(Common);
		using std::swap;
		for (size_type Index = 0; Index < Common; ++Index)
			swap(Slots[Index], Other.Slots[Index]);
	}

	constexpr void clear() noexcept { DestroyFrom(0); }

	friend constexpr bool operator==(const TStaticVector &Left, const TStaticVector &Right)
	{
		return std::equal(Left.begin(), Left.end(), Right.begin(), Right.end());
	}

	friend constexpr auto operator<=>(const TStaticVector &Left, const TStaticVector &Right)
		requires requires(const T &Value) { { Value < Value } -> std::convertible_to<bool>; }
	{
		return std::lexicographical_compare_three_way(Left.begin(), Left.end(), Right.begin(), Right.end(),
			[](const T &A, const T &B) { return SynthThreeWay(A, B); });
	}

	friend constexpr void swap(TStaticVector &Left, TStaticVector &Right) noexcept(noexcept(Left.swap(Right)))
	{
		Left.swap(Right);
	}

private:
	// The elements: [0, Length) alive, the rest raw storage. The union keeps the array's members
	// from being constructed or destroyed with the object. N == 0 still has one (never used) slot.
	union {
		T Slots[N == 0 ? 1 : N];
	};
	size_type Length = 0;

	// During constant evaluation the slots of a trivial T are given values, which makes the array
	// the union's active member, so that the elements can be constructed and read there.
	constexpr void Init() noexcept
	{
		if consteval {
			if constexpr (std::is_trivially_default_constructible_v<T> && std::is_trivially_copyable_v<T>) {
				for (size_type Index = 0; Index < N; ++Index)
					Slots[Index] = T();
			}
		}
	}

	static constexpr void Reserve(size_type Count)
	{
		if (Count > N)
			throw std::bad_alloc();
	}

	constexpr size_type Offset(const_iterator Position) const
	{
		CONTRACT_ASSERT(cbegin() <= Position && Position <= cend());
		return static_cast<size_type>(Position - cbegin());
	}

	template <class... Arguments>
	constexpr T &ConstructBack(Arguments &&...Values)
	{
		T *Element = std::construct_at(data() + Length, std::forward<Arguments>(Values)...);
		++Length;
		return *Element;
	}

	// Destroys the elements from Count on. During constant evaluation the slot of a trivial T gets
	// a value again (as in Init): Clang does not copy an array with a destroyed element (the trivial
	// copy constructor of a TStaticVector<int, N>; issue #7).
	constexpr void DestroyFrom(size_type Count) noexcept
	{
		while (Length > Count) {
			--Length;
			std::destroy_at(data() + Length);
			if consteval {
				if constexpr (std::is_trivially_default_constructible_v<T> && std::is_trivially_copyable_v<T>)
					std::construct_at(data() + Length);
			}
		}
	}

	// Appends [First, Last). With a known distance the capacity is checked first and nothing changes
	// when it is too small; otherwise the elements are appended until it is full.
	template <class Iterator, class Sentinel>
	constexpr void AppendFrom(Iterator First, Sentinel Last)
	{
		if constexpr (cxx26::KnownDistance<Iterator, Sentinel>) {
			if (Distance(First, Last) > N - Length)
				throw std::bad_alloc();
			for (; First != Last; ++First)
				ConstructBack(*First);
		} else {
			for (; First != Last; ++First)
				emplace_back(*First);
		}
	}

	// Assigns [First, Last): the existing elements are assigned, the rest constructed or destroyed.
	template <class Iterator, class Sentinel>
	constexpr void AssignFrom(Iterator First, Sentinel Last)
	{
		if constexpr (cxx26::KnownDistance<Iterator, Sentinel>)
			Reserve(Distance(First, Last));
		size_type Index = 0;
		for (; Index < Length && First != Last; ++Index, ++First)
			Slots[Index] = *First;
		DestroyFrom(Index);
		AppendFrom(std::move(First), std::move(Last));
	}

	template <class Iterator, class Sentinel>
	constexpr iterator InsertFrom(const_iterator Position, Iterator First, Sentinel Last)
	{
		const size_type Before = Offset(Position);
		const size_type OldLength = Length;
		AppendFrom(std::move(First), std::move(Last));
		std::rotate(begin() + Before, begin() + OldLength, end());
		return begin() + Before;
	}

	template <class Iterator, class Sentinel>
	static constexpr size_type Distance(const Iterator &First, const Sentinel &Last)
	{
		if constexpr (std::sized_sentinel_for<Sentinel, Iterator> || std::forward_iterator<Iterator>)
			return static_cast<size_type>(std::ranges::distance(First, Last));
		else
			return static_cast<size_type>(std::distance(First, Last));
	}

	// The standard's synth-three-way: <=> where T has it, otherwise from <.
	static constexpr auto SynthThreeWay(const T &A, const T &B)
	{
		if constexpr (std::three_way_comparable<T>) {
			return A <=> B;
		} else {
			if (A < B)
				return std::weak_ordering::less;
			if (B < A)
				return std::weak_ordering::greater;
			return std::weak_ordering::equivalent;
		}
	}
};

// std::erase and std::erase_if for TStaticVector: remove the matching elements and return how many.
template <class T, std::size_t N, class U = T>
constexpr typename TStaticVector<T, N>::size_type erase(TStaticVector<T, N> &Vector, const U &Value)
{
	const auto Kept = std::remove(Vector.begin(), Vector.end(), Value);
	const auto Count = static_cast<typename TStaticVector<T, N>::size_type>(Vector.end() - Kept);
	Vector.erase(Kept, Vector.end());
	return Count;
}

template <class T, std::size_t N, class Predicate>
constexpr typename TStaticVector<T, N>::size_type erase_if(TStaticVector<T, N> &Vector, Predicate Matches)
{
	const auto Kept = std::remove_if(Vector.begin(), Vector.end(), Matches);
	const auto Count = static_cast<typename TStaticVector<T, N>::size_type>(Vector.end() - Kept);
	Vector.erase(Kept, Vector.end());
	return Count;
}

} // namespace patella

#endif
