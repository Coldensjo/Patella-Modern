// span.h: C++20's std::span for the parameters that took a pointer and a length (the packet buffers, TXTEA and TRSA;
// Patella-Modern issue #59), and a C++17 stand-in, written for the 32-bit oracle's /std:c++17 (which left the fork
// with issue #94) and kept for tests/unit's stand-in build. Not part of the original source.
//
// After this header PATELLA_HAS_SPAN is 1 where the library has std::span (every product preset: MSVC
// /std:c++latest, GCC 16, Clang 23) and 0 elsewhere (a C++17 library) or with PATELLA_SPAN_STANDIN, which
// tests/unit's stand-in build defines for a whole target (never for one file: the two are different types), so that
// the stand-in runs the tests std::span passes, on the same compiler.
//
// patella::TSpan<T, Extent> is std::span<T, Extent> where PATELLA_HAS_SPAN is 1, and the stand-in otherwise, with the
// part of std::span's interface the code uses and the same meaning:
//	- construction: empty (a dynamic extent or 0), from a pointer and a count (explicit for a fixed extent), from an
//	  array U[N] and a std::array<U, N> (N equal to a fixed extent), from a TSpan of another extent or a convertible
//	  element type (U to const U; a dynamic extent to a fixed one only explicitly);
//	- data, size, size_bytes, empty, operator[], front, back, begin and end, first and last (a count, or a fixed
//	  one as the template argument), subspan(Offset, Count);
//	- patella::DynamicExtent is std::dynamic_extent.
// What it does not have: construction from other ranges and from two iterators, the deduction guides (the code spells
// the type), rbegin and rend, subspan<Offset, Count>, as_bytes. Its iterators are pointers. Like std::span in a library
// without hardening, it checks no precondition (an index or a count past the end, a count that differs from a fixed
// extent): a violated precondition is undefined in both.
//
// The size is std::span's in MSVC's, libstdc++'s and libc++'s library: a pointer for a fixed extent, a pointer and a
// std::size_t for the dynamic one, with a pointer's alignment, trivially copyable (asserted below on both paths). A
// TSpan is a parameter or a local only, never a member of a frozen structure (plan, section 1.2).

#pragma once

#include <cstddef>
#include <type_traits>
#include <version> // IWYU pragma: keep (the feature-test macro of the #if line)

#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L && !defined(PATELLA_SPAN_STANDIN)
#define PATELLA_HAS_SPAN 1
#include <span> // IWYU pragma: export
#else
#define PATELLA_HAS_SPAN 0
#include <array>
#endif

namespace patella {

#if PATELLA_HAS_SPAN

inline constexpr std::size_t DynamicExtent = std::dynamic_extent;

template <typename T, std::size_t Extent = std::dynamic_extent>
using TSpan = std::span<T, Extent>;

#else

inline constexpr std::size_t DynamicExtent = static_cast<std::size_t>(-1);

namespace detail {

// The extent: nothing for a fixed one (an empty base), the count for the dynamic one.
template <std::size_t Extent>
class TSpanExtent {
public:
	constexpr TSpanExtent() noexcept = default;
	constexpr explicit TSpanExtent(std::size_t /*Count*/) noexcept {}
	[[nodiscard]] static constexpr std::size_t ExtentCount() noexcept { return Extent; }
};

template <>
class TSpanExtent<DynamicExtent> {
public:
	constexpr TSpanExtent() noexcept = default;
	constexpr explicit TSpanExtent(std::size_t Size) noexcept : Stored(Size) {}
	[[nodiscard]] constexpr std::size_t ExtentCount() const noexcept { return Stored; }

private:
	std::size_t Stored = 0;
};

} // namespace detail

template <typename T, std::size_t Extent = DynamicExtent>
class TSpan : private detail::TSpanExtent<Extent> {
	using TExtent = detail::TSpanExtent<Extent>;

	// std::span's rule for a convertible element type: U (*)[] converts to T (*)[] (U to const U, not a derived class).
	template <typename U>
	static constexpr bool IsCompatible = std::is_convertible_v<U (*)[], T (*)[]>;
	// An extent N may become Extent.
	template <std::size_t N>
	static constexpr bool FitsExtent = Extent == DynamicExtent || N == DynamicExtent || N == Extent;

public:
	using element_type = T;
	using value_type = std::remove_cv_t<T>;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using pointer = T *;
	using const_pointer = const T *;
	using reference = T &;
	using const_reference = const T &;
	using iterator = T *;

	static constexpr std::size_t extent = Extent;

	template <std::size_t E = Extent, std::enable_if_t<E == DynamicExtent || E == 0, int> = 0>
	constexpr TSpan() noexcept
	{
	}

	template <std::size_t E = Extent, std::enable_if_t<E == DynamicExtent, int> = 0>
	constexpr TSpan(T *First, std::size_t Count) noexcept : TExtent(Count), Pointer(First)
	{
	}
	template <std::size_t E = Extent, std::enable_if_t<E != DynamicExtent, int> = 0>
	constexpr explicit TSpan(T *First, std::size_t Count) noexcept : TExtent(Count), Pointer(First)
	{
	}

	template <typename U, std::size_t N,
		std::enable_if_t<(Extent == DynamicExtent || Extent == N) && IsCompatible<U>, int> = 0>
	constexpr TSpan(U (&Array)[N]) noexcept : TExtent(N), Pointer(Array) // NOLINT(google-explicit-constructor)
	{
	}
	template <typename U, std::size_t N,
		std::enable_if_t<(Extent == DynamicExtent || Extent == N) && IsCompatible<U>, int> = 0>
	constexpr TSpan(std::array<U, N> &Array) noexcept // NOLINT(google-explicit-constructor)
		: TExtent(N), Pointer(Array.data())
	{
	}
	template <typename U, std::size_t N,
		std::enable_if_t<(Extent == DynamicExtent || Extent == N) && IsCompatible<const U>, int> = 0>
	constexpr TSpan(const std::array<U, N> &Array) noexcept // NOLINT(google-explicit-constructor)
		: TExtent(N), Pointer(Array.data())
	{
	}

	template <typename U, std::size_t N,
		std::enable_if_t<FitsExtent<N> && IsCompatible<U> && !(Extent != DynamicExtent && N == DynamicExtent), int> = 0>
	constexpr TSpan(const TSpan<U, N> &Other) noexcept // NOLINT(google-explicit-constructor)
		: TExtent(Other.size()), Pointer(Other.data())
	{
	}
	template <typename U, std::size_t N,
		std::enable_if_t<FitsExtent<N> && IsCompatible<U> && Extent != DynamicExtent && N == DynamicExtent, int> = 0>
	constexpr explicit TSpan(const TSpan<U, N> &Other) noexcept : TExtent(Other.size()), Pointer(Other.data())
	{
	}

	[[nodiscard]] constexpr T *data() const noexcept { return Pointer; }
	[[nodiscard]] constexpr std::size_t size() const noexcept { return this->ExtentCount(); }
	[[nodiscard]] constexpr std::size_t size_bytes() const noexcept { return size() * sizeof(T); }
	[[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

	constexpr T &operator[](std::size_t Index) const noexcept { return Pointer[Index]; }
	[[nodiscard]] constexpr T &front() const noexcept { return Pointer[0]; }
	[[nodiscard]] constexpr T &back() const noexcept { return Pointer[size() - 1]; }
	[[nodiscard]] constexpr T *begin() const noexcept { return Pointer; }
	[[nodiscard]] constexpr T *end() const noexcept { return Pointer + size(); }

	template <std::size_t Length>
	[[nodiscard]] constexpr TSpan<T, Length> first() const noexcept
	{
		return TSpan<T, Length>(Pointer, Length);
	}
	[[nodiscard]] constexpr TSpan<T> first(std::size_t Count) const noexcept { return TSpan<T>(Pointer, Count); }
	template <std::size_t Length>
	[[nodiscard]] constexpr TSpan<T, Length> last() const noexcept
	{
		return TSpan<T, Length>(Pointer + (size() - Length), Length);
	}
	[[nodiscard]] constexpr TSpan<T> last(std::size_t Count) const noexcept
	{
		return TSpan<T>(Pointer + (size() - Count), Count);
	}
	[[nodiscard]] constexpr TSpan<T> subspan(std::size_t Offset, std::size_t Count = DynamicExtent) const noexcept
	{
		return TSpan<T>(Pointer + Offset, Count == DynamicExtent ? size() - Offset : Count);
	}

private:
	T *Pointer = nullptr;
};

#endif

// The layout on both paths (a parameter's, never a frozen member's; the oracle build checks the stand-in's).
template <typename T, std::size_t Extent>
inline constexpr bool IsSpanLayout = sizeof(TSpan<T, Extent>)
		== (Extent == DynamicExtent ? sizeof(T *) + sizeof(std::size_t) : sizeof(T *))
	&& alignof(TSpan<T, Extent>) == alignof(T *) && std::is_trivially_copyable_v<TSpan<T, Extent>>;
static_assert(IsSpanLayout<unsigned char, DynamicExtent> && IsSpanLayout<const unsigned char, DynamicExtent>
	&& IsSpanLayout<char, DynamicExtent> && IsSpanLayout<unsigned char, 8> && IsSpanLayout<const unsigned char, 16>
	&& IsSpanLayout<unsigned char, 128>);

} // namespace patella
