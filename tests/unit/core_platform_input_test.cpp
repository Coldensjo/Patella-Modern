// The platform interfaces of part 2 (src/platform/Clipboard.h, Input.h, Window.h; issue #119): the process's own
// clipboard of the test build's scripted runs (TMemoryClipboard), and the interfaces' value types.

#include <doctest/doctest.h>

#include "platform/Clipboard.h"
#include "platform/Input.h"
#include "platform/Window.h"

#include <optional>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <string>
#include <type_traits>

namespace {

using patella::platform::IClipboard;
using patella::platform::TMemoryClipboard;

} // namespace

TEST_CASE("Memory clipboard: empty until the first SetText")
{
	TMemoryClipboard Clipboard;
	IClipboard &Interface = Clipboard;
	CHECK_FALSE(Interface.HasText());
	const std::optional<std::string> Text = Interface.Text();
	REQUIRE(Text.has_value());
	CHECK(Text->empty());
}

TEST_CASE("Memory clipboard: the text set, replaced by the next, byte for byte")
{
	TMemoryClipboard Clipboard;
	IClipboard &Interface = Clipboard;
	CHECK(Interface.SetText("J\xf6rg\r\nline"));
	CHECK(Interface.HasText());
	CHECK(Interface.Text().value_or("-") == "J\xf6rg\r\nline");
	CHECK(Interface.SetText(""));
	CHECK(Interface.HasText()); // an empty text is still text, as Windows' CF_UNICODETEXT of ""
	CHECK(Interface.Text().value_or("-").empty());
	CHECK(Interface.SetText("next"));
	CHECK(Interface.Text().value_or("-") == "next");
}

TEST_CASE("Platform value types: no platform type, the defaults")
{
	static_assert(std::is_same_v<patella::platform::TWindowHandle, void *>);
	const patella::platform::TPoint Point;
	CHECK(Point.x == 0);
	CHECK(Point.y == 0);
	const patella::platform::TDisplayMode Mode;
	CHECK(Mode.Width == 0);
	CHECK(Mode.Height == 0);
	CHECK(Mode.Depth == 0);
	CHECK(patella::platform::TMessageBoxIcon::Error != patella::platform::TMessageBoxIcon::Warning);
}
