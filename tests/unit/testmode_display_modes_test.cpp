// Issue #48: the test build's fixed display modes (src/testmode/DisplayModes.h): the file
// tools/reference.py writes, and the choice of the test mode's IWindow::DisplayMode (TestModeWin32.cpp
// TTestWindow, issue #119; PATELLA_ENUM_DISPLAY_SETTINGS until then): with a list the client gets that list whatever the machine reports, without one the
// machine's, as in Patella.exe.

#include <doctest/doctest.h>

#include "testmode/DisplayModes.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace patella::testmode;

namespace {

// The list of a reference session as tools/reference.py writes it (display_modes_text), the start of
// FORMAT1_DISPLAY_MODES.
const char *const SessionFile = "# patella-display-modes 1\r\n640 480 32\r\n720 480 32\r\n720 576 32\r\n800 600 32\r\n"
								"1024 768 32\r\n1152 864 32\r\n1280 720 32\r\n1280 768 32\r\n";

// A made-up machine: the modes of issue #48's display, a 1176x664 mode among them, a 16-bit mode, one
// below 640x480 and every mode twice (two refresh rates), as EnumDisplaySettings lists them.
const std::vector<TDisplayMode> &MachineModes()
{
	static const std::vector<TDisplayMode> Modes = {{320, 200, 32}, {640, 480, 32}, {640, 480, 32}, {720, 480, 32}, {720, 576, 32},
		{800, 600, 16}, {800, 600, 32}, {1024, 768, 32}, {1152, 864, 32}, {1176, 664, 32}, {1176, 664, 32}, {1280, 720, 32},
		{1280, 768, 32}, {2560, 1600, 32}};
	return Modes;
}

struct TMachine {
	int Asked = 0;
	bool operator()(std::uint32_t Index, TDisplayMode *Mode)
	{
		Asked++;
		if (Index >= MachineModes().size())
			return false;
		*Mode = MachineModes()[Index];
		return true;
	}
};

// TSurfaceVulkan::GetFullscreenModes' loop (vulkan/SurfaceVulkan.cpp): every mode until the first
// false, at least 640x480 and 16 bits, each size and depth once.
std::vector<TDisplayMode> DialogList(const std::vector<TDisplayMode> *Fixed, TMachine &Machine)
{
	std::vector<TDisplayMode> Found;
	TDisplayMode Mode;
	for (std::uint32_t Index = 0; EnumerateDisplayMode(Fixed, Index, &Mode, Machine); Index++) {
		if (Mode.Width < 640 || Mode.Height < 480 || Mode.Depth < 16)
			continue;
		bool Known = false;
		for (const TDisplayMode &Other : Found)
			Known = Known || Other == Mode;
		if (!Known)
			Found.push_back(Mode);
	}
	return Found;
}

} // namespace

TEST_CASE("DisplayModes: the file")
{
	const TDisplayModesParseResult Parsed = ParseDisplayModes(SessionFile);
	REQUIRE_MESSAGE(Parsed.Ok, Parsed.ErrorLine, ": ", Parsed.Error);
	REQUIRE(Parsed.Modes.size() == 8);
	CHECK(Parsed.Modes.front() == TDisplayMode{640, 480, 32});
	CHECK(Parsed.Modes[6] == TDisplayMode{1280, 720, 32});
	CHECK(Parsed.Modes.back() == TDisplayMode{1280, 768, 32});

	const TDisplayModesParseResult Comments = ParseDisplayModes("# patella-display-modes 1\n# a comment\n\n  800\t600 16  \n");
	REQUIRE(Comments.Ok);
	CHECK(Comments.Modes == std::vector<TDisplayMode>{{800, 600, 16}});
}

TEST_CASE("DisplayModes: files that do not parse")
{
	struct TCase {
		const char *Text;
		int Line;
	};
	const TCase Cases[] = {
		{"", 0},                                           // no header
		{"640 480 32\n", 1},                               // no header
		{"# patella-display-modes 2\n640 480 32\n", 2},    // another version
		{"# patella-display-modes 1\n", 0},                // no mode
		{"# patella-display-modes 1\n640 480\n", 2},       // a field missing
		{"# patella-display-modes 1\n640 480 32 60\n", 2}, // a field too many
		{"# patella-display-modes 1\n640x480 32\n", 2},
		{"# patella-display-modes 1\n640 480 0\n", 2},
		{"# patella-display-modes 1\n640 480 128\n", 2},
		{"# patella-display-modes 1\n0 480 32\n", 2},
		{"# patella-display-modes 1\n640 -480 32\n", 2},
		{"# patella-display-modes 1\n800 600 32\n99999 480 32\n", 3},
	};
	for (const TCase &Case : Cases) {
		CAPTURE(std::string(Case.Text));
		const TDisplayModesParseResult Parsed = ParseDisplayModes(Case.Text);
		CHECK_FALSE(Parsed.Ok);
		CHECK(Parsed.ErrorLine == Case.Line);
		CHECK(Parsed.Modes.empty());
		CHECK_FALSE(Parsed.Error.empty());
	}
}

TEST_CASE("DisplayModes: with a fixed list the dialog lists it, whatever the machine reports")
{
	const TDisplayModesParseResult Parsed = ParseDisplayModes(SessionFile);
	REQUIRE(Parsed.Ok);
	TMachine Machine;
	const std::vector<TDisplayMode> Listed = DialogList(&Parsed.Modes, Machine);
	CHECK(Listed == Parsed.Modes);
	CHECK(Machine.Asked == 0);
	TDisplayMode Mode{1, 2, 3};
	CHECK_FALSE(EnumerateDisplayMode(&Parsed.Modes, static_cast<std::uint32_t>(Parsed.Modes.size()), &Mode, Machine));
	CHECK(Mode == TDisplayMode{1, 2, 3});
	CHECK(Machine.Asked == 0);
}

TEST_CASE("DisplayModes: without a list the dialog lists the machine's modes (Patella.exe's path)")
{
	TMachine Machine;
	const std::vector<TDisplayMode> Listed = DialogList(nullptr, Machine);
	const std::vector<TDisplayMode> Expected = {{640, 480, 32}, {720, 480, 32}, {720, 576, 32}, {800, 600, 16}, {800, 600, 32},
		{1024, 768, 32}, {1152, 864, 32}, {1176, 664, 32}, {1280, 720, 32}, {1280, 768, 32}, {2560, 1600, 32}};
	CHECK(Listed == Expected);
	CHECK(Machine.Asked == static_cast<int>(MachineModes().size()) + 1);
}
