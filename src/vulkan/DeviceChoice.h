// DeviceChoice.h: the graphics device choice of the Vulkan engine (Patella-Modern issue #31; plan 6.3).
//
// Plan 6.3 turns the advanced graphics options' three engine buttons into a graphics device choice:
// "Automatic", "GPU" (one entry per Vulkan GPU; the button goes on to the next GPU when there are
// several) and "CPU (compatibility)". The choice is Patella.ini's Device (src/settings/Settings.h) and
// PATELLA_VULKAN's value, in the same words: auto, gpu (the best GPU), gpuN or gpu:N (the n-th GPU, from
// 0, in the loader's order: TRenderer's ListGpus), cpu (the CPU driver). No Vulkan here, no platform
// header: tests_render checks it on every preset.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace patella::vulkan {

// The device the engine runs on.
enum class TDeviceKind : int {
	Automatic, // the best GPU with Vulkan 1.3, the CPU driver when there is none
	Gpu,       // a GPU only (GpuIndex: the n-th, -1 the best)
	Cpu,       // the CPU driver only
};
static_assert(sizeof(TDeviceKind) == 4);
static_assert(std::is_same_v<std::underlying_type_t<TDeviceKind>, int>);

struct TDeviceChoice {
	TDeviceKind Kind = TDeviceKind::Automatic;
	int GpuIndex = -1; // only for Gpu
	bool operator==(const TDeviceChoice &) const = default;
};

// The advanced options' buttons, in the skin's three places (the original's DirectX5, DirectX9 and
// OpenGL buttons).
enum class TDeviceButton : int {
	Automatic,
	Gpu,
	Cpu,
};
static_assert(sizeof(TDeviceButton) == 4);
static_assert(std::is_same_v<std::underlying_type_t<TDeviceButton>, int>);

// "auto", "gpu", "gpu1", "gpu:1", "cpu", without regard to ASCII case or the spaces around it; nothing
// for anything else (an empty text included).
std::optional<TDeviceChoice> ParseDeviceChoice(std::string_view Text);

// The words ParseDeviceChoice reads: auto, gpu, gpuN, cpu.
std::string FormatDeviceChoice(const TDeviceChoice &Choice);

// The choice after a click on Button. Automatic and CPU choose themselves. GPU, when the choice is not
// a GPU yet, chooses the best GPU; when it is one and NumberOfGpus is more than 1, the next GPU after the
// chosen one (BestGpu for "the best"), the first again after the last. Without a GPU, or with one GPU
// already chosen, the choice stays.
TDeviceChoice ChoiceAfterButton(const TDeviceChoice &Current, TDeviceButton Button, int NumberOfGpus, int BestGpu);

// The button that shows Choice as selected.
TDeviceButton ButtonOf(const TDeviceChoice &Choice);

} // namespace patella::vulkan
