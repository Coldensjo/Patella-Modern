// DeviceChoice (src/vulkan/DeviceChoice.cpp, issue #31): Patella.ini's and PATELLA_VULKAN's device words
// and what the advanced options' buttons choose.

#include <doctest/doctest.h>

#include "vulkan/DeviceChoice.h"

using patella::vulkan::ButtonOf;
using patella::vulkan::ChoiceAfterButton;
using patella::vulkan::FormatDeviceChoice;
using patella::vulkan::ParseDeviceChoice;
using patella::vulkan::TDeviceButton;
using patella::vulkan::TDeviceChoice;
using patella::vulkan::TDeviceKind;

namespace {

const TDeviceChoice Automatic{TDeviceKind::Automatic, -1};
const TDeviceChoice Cpu{TDeviceKind::Cpu, -1};

TDeviceChoice Gpu(int Index = -1)
{
	return TDeviceChoice{TDeviceKind::Gpu, Index};
}

} // namespace

TEST_CASE("DeviceChoice: the words")
{
	CHECK(ParseDeviceChoice("auto") == Automatic);
	CHECK(ParseDeviceChoice(" AUTO \r") == Automatic);
	CHECK(ParseDeviceChoice("cpu") == Cpu);
	CHECK(ParseDeviceChoice("Cpu") == Cpu);
	CHECK(ParseDeviceChoice("gpu") == Gpu());
	CHECK(ParseDeviceChoice("gpu0") == Gpu(0));
	CHECK(ParseDeviceChoice("GPU1") == Gpu(1));
	CHECK(ParseDeviceChoice("gpu:2") == Gpu(2));
	CHECK(ParseDeviceChoice("gpu12") == Gpu(12));
	CHECK(ParseDeviceChoice("gpu9999") == Gpu(9999));
	for (const char *Bad : {"", " ", "off", "dx9", "0", "gpu:", "gpu-1", "gpu1x", "gpu 1", "gpu99999", "cpu0", "autox"})
		CHECK_FALSE(ParseDeviceChoice(Bad).has_value());
	CHECK(FormatDeviceChoice(Automatic) == "auto");
	CHECK(FormatDeviceChoice(Cpu) == "cpu");
	CHECK(FormatDeviceChoice(Gpu()) == "gpu");
	CHECK(FormatDeviceChoice(Gpu(3)) == "gpu3");
	for (const TDeviceChoice &Choice : {Automatic, Cpu, Gpu(), Gpu(0), Gpu(7)})
		CHECK(ParseDeviceChoice(FormatDeviceChoice(Choice)) == Choice);
}

TEST_CASE("DeviceChoice: the buttons")
{
	SUBCASE("Automatic and CPU choose themselves")
	{
		for (const TDeviceChoice &Current : {Automatic, Cpu, Gpu(), Gpu(1)}) {
			CHECK(ChoiceAfterButton(Current, TDeviceButton::Automatic, 2, 0) == Automatic);
			CHECK(ChoiceAfterButton(Current, TDeviceButton::Cpu, 2, 0) == Cpu);
			CHECK(ChoiceAfterButton(Current, TDeviceButton::Cpu, 0, -1) == Cpu);
		}
	}
	SUBCASE("GPU: the best one first")
	{
		CHECK(ChoiceAfterButton(Automatic, TDeviceButton::Gpu, 1, 0) == Gpu());
		CHECK(ChoiceAfterButton(Cpu, TDeviceButton::Gpu, 3, 1) == Gpu());
	}
	SUBCASE("GPU without a GPU or with one: nothing changes")
	{
		CHECK(ChoiceAfterButton(Automatic, TDeviceButton::Gpu, 0, -1) == Automatic);
		CHECK(ChoiceAfterButton(Cpu, TDeviceButton::Gpu, 0, -1) == Cpu);
		CHECK(ChoiceAfterButton(Gpu(), TDeviceButton::Gpu, 1, 0) == Gpu());
		CHECK(ChoiceAfterButton(Gpu(0), TDeviceButton::Gpu, 1, 0) == Gpu(0));
	}
	SUBCASE("GPU with several: the next one, round")
	{
		// The best is the second in the loader's order (a laptop's discrete GPU after the integrated).
		TDeviceChoice Choice = ChoiceAfterButton(Automatic, TDeviceButton::Gpu, 3, 1);
		CHECK(Choice == Gpu());
		Choice = ChoiceAfterButton(Choice, TDeviceButton::Gpu, 3, 1);
		CHECK(Choice == Gpu(2));
		Choice = ChoiceAfterButton(Choice, TDeviceButton::Gpu, 3, 1);
		CHECK(Choice == Gpu(0));
		Choice = ChoiceAfterButton(Choice, TDeviceButton::Gpu, 3, 1);
		CHECK(Choice == Gpu(1));
		// A stored index beyond the list starts again at the first.
		CHECK(ChoiceAfterButton(Gpu(5), TDeviceButton::Gpu, 2, 0) == Gpu(1));
	}
	CHECK(ButtonOf(Automatic) == TDeviceButton::Automatic);
	CHECK(ButtonOf(Gpu()) == TDeviceButton::Gpu);
	CHECK(ButtonOf(Gpu(2)) == TDeviceButton::Gpu);
	CHECK(ButtonOf(Cpu) == TDeviceButton::Cpu);
}
