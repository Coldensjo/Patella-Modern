// patella::TMdspan, TExtents, TDynamicExtents, TLayoutRight and TLayoutLeft (src/compat/mdspan.h, issue #101):
// std::mdspan in tests_unit where the library has it (MSVC, GCC 16's libstdc++), the stand-in in
// tests_unit_cxx26_standin (PATELLA_MDSPAN_STANDIN) and in tests_unit with a library without it (linux-x64-clang:
// libstdc++ 14). The same tests hold for both, so the stand-in behaves as std::mdspan where the code uses it.

#include <doctest/doctest.h>

#include "compat/mdspan.h"
#include "compat/span.h" // patella::DynamicExtent

#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <type_traits>

namespace {

using patella::DynamicExtent;
using patella::TDynamicExtents;
using patella::TExtents;
using patella::TLayoutLeft;
using patella::TLayoutRight;
using patella::TMdspan;

#if defined(PATELLA_MDSPAN_STANDIN)
static_assert(PATELLA_HAS_MDSPAN == 0, "PATELLA_MDSPAN_STANDIN selects the stand-in");
#elif defined(__cpp_lib_mdspan)
static_assert(PATELLA_HAS_MDSPAN == 1, "a library with std::mdspan gives std::mdspan");
static_assert(std::is_same_v<TMdspan<int, TDynamicExtents<int, 2>>, std::mdspan<int, std::dextents<int, 2>>>);
static_assert(std::is_same_v<TExtents<int, 256, 256>, std::extents<int, 256, 256>>);
static_assert(std::is_same_v<TLayoutLeft, std::layout_left> && std::is_same_v<TLayoutRight, std::layout_right>);
#else
static_assert(PATELLA_HAS_MDSPAN == 0, "a library without std::mdspan gives the stand-in");
#endif

using TStatic = TExtents<int, 3, 4>;
using TMixed = TExtents<int, 2, DynamicExtent, 5>;
using TDynamic3 = TDynamicExtents<int, 3>;

static_assert(TStatic::rank() == 2 && TStatic::rank_dynamic() == 0);
static_assert(TStatic::static_extent(0) == 3 && TStatic::static_extent(1) == 4);
static_assert(TMixed::rank() == 3 && TMixed::rank_dynamic() == 1 && TMixed::static_extent(1) == DynamicExtent);
static_assert(TDynamic3::rank() == 3 && TDynamic3::rank_dynamic() == 3);
static_assert(std::is_same_v<TDynamic3, TExtents<int, DynamicExtent, DynamicExtent, DynamicExtent>>);
static_assert(std::is_same_v<TDynamic3::index_type, int>);
static_assert(std::is_same_v<TMdspan<const int, TStatic>::element_type, const int>
	&& std::is_same_v<TMdspan<const int, TStatic>::value_type, int>);
static_assert(std::is_same_v<TMdspan<int, TStatic>::layout_type, TLayoutRight>);

// The mappings, at compile time: layout_right's last index the fastest, layout_left's first.
static_assert(TLayoutRight::mapping<TStatic>()(0, 1) == 1 && TLayoutRight::mapping<TStatic>()(1, 0) == 4);
static_assert(TLayoutRight::mapping<TStatic>()(2, 3) == 11 && TLayoutRight::mapping<TStatic>().required_span_size() == 12);
static_assert(TLayoutLeft::mapping<TStatic>()(1, 0) == 1 && TLayoutLeft::mapping<TStatic>()(0, 1) == 3);
static_assert(TLayoutLeft::mapping<TStatic>()(2, 3) == 11 && TLayoutLeft::mapping<TStatic>().required_span_size() == 12);

// The automap's layer (src/AutomapLayer.h): [x, y] at x * 256 + y.
using TLayer = TMdspan<std::uint8_t, TExtents<int, 256, 256>>;
static_assert(TLayer::mapping_type()(1, 0) == 256 && TLayer::mapping_type()(0, 1) == 1);
static_assert(TLayer::mapping_type()(255, 255) == 65535 && TLayer::mapping_type().required_span_size() == 65536);

} // namespace

TEST_CASE("TExtents: static and dynamic extents")
{
	const TMixed Mixed(7);
	CHECK(Mixed.extent(0) == 2);
	CHECK(Mixed.extent(1) == 7);
	CHECK(Mixed.extent(2) == 5);
	const TDynamic3 Dynamic(4, 5, 6);
	CHECK(Dynamic.extent(0) == 4);
	CHECK(Dynamic.extent(1) == 5);
	CHECK(Dynamic.extent(2) == 6);
	const TDynamic3 Empty;
	CHECK(Empty.extent(0) == 0);
	CHECK(Empty.extent(2) == 0);
}

TEST_CASE("TMdspan: layout_right, static extents")
{
	std::array<int, 12> Storage{};
	std::iota(Storage.begin(), Storage.end(), 0);
	const TMdspan<int, TStatic> View(Storage.data());
	CHECK(View.data_handle() == Storage.data());
	CHECK(View.size() == 12U);
	CHECK_FALSE(View.empty());
	CHECK(View.extent(0) == 3);
	CHECK(View.extent(1) == 4);
	CHECK(View.stride(0) == 4);
	CHECK(View.stride(1) == 1);
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 4; j++) {
			CHECK(View[i, j] == i * 4 + j);
			CHECK(&View[i, j] == &Storage[static_cast<std::size_t>(i * 4 + j)]);
		}
	}
	View[2, 1] = 99; // a view of non-const elements writes them
	CHECK(Storage[9] == 99);
}

TEST_CASE("TMdspan: layout_left, dynamic extents")
{
	std::array<int, 24> Storage{};
	std::iota(Storage.begin(), Storage.end(), 0);
	const TMdspan<int, TDynamic3, TLayoutLeft> View(Storage.data(), 2, 3, 4);
	CHECK(View.size() == 24U);
	CHECK(View.extent(0) == 2);
	CHECK(View.extent(1) == 3);
	CHECK(View.extent(2) == 4);
	CHECK(View.stride(0) == 1);
	CHECK(View.stride(1) == 2);
	CHECK(View.stride(2) == 6);
	CHECK(View.mapping().required_span_size() == 24);
	for (int z = 0; z < 4; z++) {
		for (int y = 0; y < 3; y++) {
			for (int x = 0; x < 2; x++)
				CHECK(View[x, y, z] == (z * 3 + y) * 2 + x);
		}
	}
	const TMdspan<int, TDynamicExtents<int, 2>, TLayoutLeft> Plane(Storage.data(), 4, 6);
	CHECK(&Plane[3, 5] == &Storage[23]);
	CHECK(Plane[1, 2] == 2 * 4 + 1);
}

TEST_CASE("TMdspan: a const view of the same elements, and an empty one")
{
	std::array<std::uint8_t, 65536> Bytes{};
	const TLayer Layer(Bytes.data());
	Layer[1, 2] = 7;
	CHECK(Bytes[258] == 7);
	const TMdspan<const std::uint8_t, TExtents<int, 256, 256>> Read = Layer; // T to const T
	CHECK(Read.data_handle() == Bytes.data());
	CHECK(Read[1, 2] == 7);
	CHECK(Read.size() == 65536U);

	const TMdspan<int, TDynamicExtents<int, 2>> None(nullptr, 0, 5);
	CHECK(None.empty());
	CHECK(None.size() == 0U);
	CHECK(None.extent(1) == 5);
}
