// DeviceChoice.cpp: the graphics device choice (DeviceChoice.h; Patella-Modern issue #31).

#include "vulkan/DeviceChoice.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace patella::vulkan {

namespace {

std::string Normalized(std::string_view Text)
{
	const size_t First = Text.find_first_not_of(" \t\r\n");
	if (First == std::string_view::npos)
		return {};
	const size_t Last = Text.find_last_not_of(" \t\r\n");
	std::string Result(Text.substr(First, Last - First + 1));
	for (char &c : Result) {
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	}
	return Result;
}

} // namespace

std::optional<TDeviceChoice> ParseDeviceChoice(std::string_view Text)
{
	const std::string Value = Normalized(Text);
	if (Value == "auto")
		return TDeviceChoice{TDeviceKind::Automatic, -1};
	if (Value == "cpu")
		return TDeviceChoice{TDeviceKind::Cpu, -1};
	if (!Value.starts_with("gpu"))
		return std::nullopt;
	std::string_view Number = std::string_view(Value).substr(3);
	if (Number.starts_with(':'))
		Number.remove_prefix(1);
	else if (Number.empty())
		return TDeviceChoice{TDeviceKind::Gpu, -1};
	// A number of up to four digits (no GPU list is longer).
	if (Number.empty() || Number.size() > 4)
		return std::nullopt;
	int Index = 0;
	for (const char c : Number) {
		if (c < '0' || c > '9')
			return std::nullopt;
		Index = Index * 10 + (c - '0');
	}
	return TDeviceChoice{TDeviceKind::Gpu, Index};
}

std::string FormatDeviceChoice(const TDeviceChoice &Choice)
{
	switch (Choice.Kind) {
	case TDeviceKind::Gpu:
		return Choice.GpuIndex >= 0 ? "gpu" + std::to_string(Choice.GpuIndex) : "gpu";
	case TDeviceKind::Cpu:
		return "cpu";
	case TDeviceKind::Automatic:
		break;
	}
	return "auto";
}

TDeviceChoice ChoiceAfterButton(const TDeviceChoice &Current, TDeviceButton Button, int NumberOfGpus, int BestGpu)
{
	switch (Button) {
	case TDeviceButton::Automatic:
		return TDeviceChoice{TDeviceKind::Automatic, -1};
	case TDeviceButton::Cpu:
		return TDeviceChoice{TDeviceKind::Cpu, -1};
	case TDeviceButton::Gpu:
		break;
	}
	if (NumberOfGpus <= 0)
		return Current;
	if (Current.Kind != TDeviceKind::Gpu)
		return TDeviceChoice{TDeviceKind::Gpu, -1};
	if (NumberOfGpus == 1)
		return Current;
	int Chosen = Current.GpuIndex >= 0 ? Current.GpuIndex : BestGpu;
	if (Chosen < 0 || Chosen >= NumberOfGpus)
		Chosen = 0;
	return TDeviceChoice{TDeviceKind::Gpu, (Chosen + 1) % NumberOfGpus};
}

TDeviceButton ButtonOf(const TDeviceChoice &Choice)
{
	switch (Choice.Kind) {
	case TDeviceKind::Gpu:
		return TDeviceButton::Gpu;
	case TDeviceKind::Cpu:
		return TDeviceButton::Cpu;
	case TDeviceKind::Automatic:
		break;
	}
	return TDeviceButton::Automatic;
}

} // namespace patella::vulkan
