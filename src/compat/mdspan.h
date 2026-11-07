// mdspan.h: C++23's std::mdspan for the client's multi-dimensional storage (Container.h's matrix and matrix3d, the
// automap's colour and speed layers; Patella-Modern issue #101), and a stand-in where the library lacks it. Not part
// of the original source.
//
// After this header PATELLA_HAS_MDSPAN is 1 where the library has std::mdspan (MSVC /std:c++latest: 202207; GCC 16's
// libstdc++, also mingw-w64's and linux-x64-clang's since issue #121: 202406) and 0 with a library without it (none of
// the presets since #121; linux-x64-clang's Debian libstdc++ 14 until then) or with PATELLA_MDSPAN_STANDIN, which
// tests/unit's stand-in build defines for a whole target (never for one file: the two are different types), so that the
// stand-in runs the tests std::mdspan passes, on the same compiler.
//
// The names are std's where PATELLA_HAS_MDSPAN is 1, and the stand-ins otherwise, with the part of the interface the
// code uses and the same meaning:
//	- patella::TExtents<IndexType, Extents...> is std::extents (a static extent or patella::DynamicExtent each): rank,
//	  rank_dynamic, static_extent, extent; empty (the dynamic extents 0), from the dynamic extents (explicit);
//	- patella::TDynamicExtents<IndexType, Rank> is std::dextents;
//	- patella::TLayoutRight and TLayoutLeft are std::layout_right (the last index the fastest, C's arrays) and
//	  std::layout_left (the first the fastest, Fortran's); their mapping<Extents>: extents, operator() (the offset of
//	  the indices), required_span_size, stride;
//	- patella::TMdspan<T, Extents, Layout = TLayoutRight> is std::mdspan with the default accessor: from a pointer and
//	  the dynamic extents (explicit), or a pointer and an extents; from a TMdspan of a convertible element type (T to
//	  const T); operator[] with rank() indices (C++23's multidimensional subscript), data_handle, mapping, extents,
//	  extent, size, empty, stride, rank, rank_dynamic, static_extent.
// What it does not have: extents from all rank() extents or from a std::array or std::span, the other layouts
// (layout_stride) and accessors, operator[] with a std::array or std::span of indices, the deduction guides (the code
// spells the type), submdspan, the comparisons of extents and mappings, is_unique and its relatives. Like std::mdspan
// in a library without hardening, it checks no precondition (an index past an extent): a violated precondition is
// undefined in both. The containers check their indices before they reach a view (Container.h).
//
// A TMdspan is a view: a pointer and the dynamic extents. The code makes one from the storage that owns the elements
// (a std::unique_ptr<T[]>, a std::array) when it indexes it, and never keeps one in a structure.

#pragma once

#include <cstddef>
#include <version> // IWYU pragma: keep (the feature-test macro of the #if line)

#if defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L && !defined(PATELLA_MDSPAN_STANDIN)
#define PATELLA_HAS_MDSPAN 1
#include <mdspan> // IWYU pragma: export
#else
#define PATELLA_HAS_MDSPAN 0
#include "compat/span.h" // patella::DynamicExtent (std::dynamic_extent)

#include <array>
#include <type_traits>
#endif

namespace patella {

#if PATELLA_HAS_MDSPAN

template <typename IndexType, std::size_t... Extents>
using TExtents = std::extents<IndexType, Extents...>;

template <typename IndexType, std::size_t Rank>
using TDynamicExtents = std::dextents<IndexType, Rank>;

using TLayoutRight = std::layout_right;
using TLayoutLeft = std::layout_left;

template <typename T, typename Extents, typename Layout = TLayoutRight>
using TMdspan = std::mdspan<T, Extents, Layout>;

#else

template <typename IndexType, std::size_t... Extents>
class TExtents {
	static_assert(std::is_integral_v<IndexType> && !std::is_same_v<IndexType, bool>, "TExtents: an integer index type");

public:
	using index_type = IndexType;
	using size_type = std::make_unsigned_t<IndexType>;
	using rank_type = std::size_t;

	[[nodiscard]] static constexpr rank_type rank() noexcept { return sizeof...(Extents); }
	[[nodiscard]] static constexpr rank_type rank_dynamic() noexcept
	{
		return ((Extents == DynamicExtent ? rank_type{1} : rank_type{0}) + ... + rank_type{0});
	}
	[[nodiscard]] static constexpr std::size_t static_extent(rank_type Rank) noexcept { return Static[Rank]; }

	constexpr TExtents() noexcept = default;

	template <typename... OtherIndexTypes>
		requires(sizeof...(OtherIndexTypes) == rank_dynamic() && sizeof...(OtherIndexTypes) > 0
			&& (std::is_convertible_v<OtherIndexTypes, IndexType> && ...))
	constexpr explicit TExtents(OtherIndexTypes... DynamicExtents) noexcept
		: Dynamic{static_cast<IndexType>(DynamicExtents)...}
	{
	}

	[[nodiscard]] constexpr index_type extent(rank_type Rank) const noexcept
	{
		if (Static[Rank] != DynamicExtent)
			return static_cast<index_type>(Static[Rank]);
		rank_type Position = 0;
		for (rank_type r = 0; r < Rank; r++) {
			if (Static[r] == DynamicExtent)
				Position++;
		}
		return Dynamic[Position];
	}

private:
	static constexpr std::array<std::size_t, sizeof...(Extents)> Static{Extents...};
	std::array<IndexType, rank_dynamic()> Dynamic{};
};

namespace detail {

// TExtents of Rank dynamic extents.
template <typename IndexType, std::size_t Rank, std::size_t... Extents>
struct TDynamicExtentsOf : TDynamicExtentsOf<IndexType, Rank - 1, DynamicExtent, Extents...> {};

template <typename IndexType, std::size_t... Extents>
struct TDynamicExtentsOf<IndexType, 0, Extents...> {
	using type = TExtents<IndexType, Extents...>;
};

} // namespace detail

template <typename IndexType, std::size_t Rank>
using TDynamicExtents = typename detail::TDynamicExtentsOf<IndexType, Rank>::type;

namespace detail {

// The product of the extents [First, Last), in the index type.
template <typename Extents>
[[nodiscard]] constexpr typename Extents::index_type ExtentProduct(const Extents &Value, std::size_t First,
	std::size_t Last) noexcept
{
	typename Extents::index_type Product = 1;
	for (std::size_t r = First; r < Last; r++)
		Product = static_cast<typename Extents::index_type>(Product * Value.extent(r));
	return Product;
}

} // namespace detail

// The last index is the fastest: ((i0 * e1 + i1) * e2 + i2)...
struct TLayoutRight {
	template <typename Extents>
	class mapping {
	public:
		using extents_type = Extents;
		using index_type = typename Extents::index_type;
		using size_type = typename Extents::size_type;
		using rank_type = typename Extents::rank_type;
		using layout_type = TLayoutRight;

		constexpr mapping() noexcept = default;
		constexpr mapping(const Extents &Value) noexcept : Stored(Value) {} // NOLINT(google-explicit-constructor): std's

		[[nodiscard]] constexpr const extents_type &extents() const noexcept { return Stored; }
		[[nodiscard]] constexpr index_type required_span_size() const noexcept
		{
			return detail::ExtentProduct(Stored, 0, Extents::rank());
		}
		template <typename... Indices>
			requires(sizeof...(Indices) == Extents::rank())
		[[nodiscard]] constexpr index_type operator()(Indices... Index) const noexcept
		{
			const std::array<index_type, Extents::rank()> Values{static_cast<index_type>(Index)...};
			index_type Offset = 0;
			for (rank_type r = 0; r < Extents::rank(); r++)
				Offset = static_cast<index_type>(Offset * Stored.extent(r) + Values[r]);
			return Offset;
		}
		[[nodiscard]] constexpr index_type stride(rank_type Rank) const noexcept
		{
			return detail::ExtentProduct(Stored, Rank + 1, Extents::rank());
		}

	private:
		Extents Stored{};
	};
};

// The first index is the fastest: i0 + e0 * (i1 + e1 * (i2...)).
struct TLayoutLeft {
	template <typename Extents>
	class mapping {
	public:
		using extents_type = Extents;
		using index_type = typename Extents::index_type;
		using size_type = typename Extents::size_type;
		using rank_type = typename Extents::rank_type;
		using layout_type = TLayoutLeft;

		constexpr mapping() noexcept = default;
		constexpr mapping(const Extents &Value) noexcept : Stored(Value) {} // NOLINT(google-explicit-constructor): std's

		[[nodiscard]] constexpr const extents_type &extents() const noexcept { return Stored; }
		[[nodiscard]] constexpr index_type required_span_size() const noexcept
		{
			return detail::ExtentProduct(Stored, 0, Extents::rank());
		}
		template <typename... Indices>
			requires(sizeof...(Indices) == Extents::rank())
		[[nodiscard]] constexpr index_type operator()(Indices... Index) const noexcept
		{
			const std::array<index_type, Extents::rank()> Values{static_cast<index_type>(Index)...};
			index_type Offset = 0;
			for (rank_type r = Extents::rank(); r > 0; r--)
				Offset = static_cast<index_type>(Offset * Stored.extent(r - 1) + Values[r - 1]);
			return Offset;
		}
		[[nodiscard]] constexpr index_type stride(rank_type Rank) const noexcept
		{
			return detail::ExtentProduct(Stored, 0, Rank);
		}

	private:
		Extents Stored{};
	};
};

template <typename T, typename Extents, typename Layout = TLayoutRight>
class TMdspan {
public:
	using extents_type = Extents;
	using layout_type = Layout;
	using mapping_type = typename Layout::template mapping<Extents>;
	using element_type = T;
	using value_type = std::remove_cv_t<T>;
	using index_type = typename Extents::index_type;
	using size_type = typename Extents::size_type;
	using rank_type = typename Extents::rank_type;
	using data_handle_type = T *;
	using reference = T &;

	[[nodiscard]] static constexpr rank_type rank() noexcept { return Extents::rank(); }
	[[nodiscard]] static constexpr rank_type rank_dynamic() noexcept { return Extents::rank_dynamic(); }
	[[nodiscard]] static constexpr std::size_t static_extent(rank_type Rank) noexcept
	{
		return Extents::static_extent(Rank);
	}

	constexpr TMdspan() noexcept = default;

	template <typename... OtherIndexTypes>
		requires(sizeof...(OtherIndexTypes) == Extents::rank_dynamic()
					&& (std::is_convertible_v<OtherIndexTypes, index_type> && ...))
	constexpr explicit TMdspan(data_handle_type Data, OtherIndexTypes... DynamicExtents) noexcept
		: Pointer(Data), Map(Extents(static_cast<index_type>(DynamicExtents)...))
	{
	}

	constexpr TMdspan(data_handle_type Data, const Extents &Value) noexcept : Pointer(Data), Map(Value) {}

	// T to const T: as std::mdspan's converting constructor, implicit for a convertible element type.
	template <typename U>
		requires(!std::is_same_v<U, T> && std::is_convertible_v<U (*)[], T (*)[]>)
	constexpr TMdspan(const TMdspan<U, Extents, Layout> &Other) noexcept // NOLINT(google-explicit-constructor): std's
		: Pointer(Other.data_handle()), Map(Other.mapping())
	{
	}

	template <typename... Indices>
		requires(sizeof...(Indices) == Extents::rank() && (std::is_convertible_v<Indices, index_type> && ...))
	[[nodiscard]] constexpr reference operator[](Indices... Index) const noexcept
	{
		return Pointer[Map(static_cast<index_type>(Index)...)];
	}

	[[nodiscard]] constexpr data_handle_type data_handle() const noexcept { return Pointer; }
	[[nodiscard]] constexpr const mapping_type &mapping() const noexcept { return Map; }
	[[nodiscard]] constexpr const extents_type &extents() const noexcept { return Map.extents(); }
	[[nodiscard]] constexpr index_type extent(rank_type Rank) const noexcept { return Map.extents().extent(Rank); }
	[[nodiscard]] constexpr size_type size() const noexcept
	{
		return static_cast<size_type>(detail::ExtentProduct(Map.extents(), 0, Extents::rank()));
	}
	[[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }
	[[nodiscard]] constexpr index_type stride(rank_type Rank) const noexcept { return Map.stride(Rank); }

private:
	data_handle_type Pointer = nullptr;
	mapping_type Map{};
};

#endif

} // namespace patella
