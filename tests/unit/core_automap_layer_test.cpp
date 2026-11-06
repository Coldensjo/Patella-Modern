// The automap's layer (src/AutomapLayer.h, issue #101): Map.h's TAutomapBlock keeps its colours and speeds as two
// TAutomapLayer, 256 x 256 bytes in the automap file's order (x * 256 + y), indexed through an mdspan view.

#include <doctest/doctest.h>

#include "AutomapLayer.h"

#include <cstddef>
#include <type_traits>

static_assert(sizeof(TAutomapLayer) == 256 * 256, "a layer is its bytes");
static_assert(std::is_same_v<TAutomapLayer::TView::layout_type, patella::TLayoutRight>);
static_assert(TAutomapLayer::TView::static_extent(0) == 256 && TAutomapLayer::TView::static_extent(1) == 256);

TEST_CASE("TAutomapLayer: the view's [x, y] is the file's byte x * 256 + y")
{
	TAutomapLayer Layer{};
	Layer.Bytes.fill(250);
	const TAutomapLayer::TView View = Layer.view();
	CHECK(View.data_handle() == Layer.Bytes.data());
	CHECK(View.size() == Layer.Bytes.size());
	for (int x = 0; x < 256; x += 51) {
		for (int y = 0; y < 256; y += 17) {
			View[x, y] = static_cast<uint8>(x ^ y);
			CHECK(Layer.Bytes[static_cast<std::size_t>(x) * 256 + static_cast<std::size_t>(y)] == static_cast<uint8>(x ^ y));
		}
	}
	View[255, 255] = 255;
	CHECK(Layer.Bytes[65535] == 255);
	CHECK(Layer.Bytes[1] == 250);

	const TAutomapLayer &Read = Layer;
	const TAutomapLayer::TConstView ReadView = Read.view();
	CHECK(ReadView[255, 255] == 255);
	CHECK(ReadView[0, 17] == 17);
	CHECK(&ReadView[51, 0] == &Layer.Bytes[51 * 256]);
}
