// Renderer.h: the Vulkan 1.3 back end of the Vulkan engine (issue #19; notes/vulkan.md; plan 6.2).
//
// A small 2D compositor without any window system: images (the engine's surfaces), colour fills,
// textured quads in Direct3D 9's pixel rules (Quads.h), uploads, the colour key pass, read-back,
// and the presentation of an image in a swapchain when a surface is given. The engine
// (SurfaceVulkan.cpp, Windows) maps TSurface's calls onto it; tests_vulkan drives it headless.
//
// Libraries (cmake/Vulkan.cmake): Vulkan-Headers, volk (the loader's functions, no import library),
// vk-bootstrap (instance, device and swapchain), VMA (memory). Vulkan 1.3 core only: dynamic
// rendering, synchronization2. Every image is B8G8R8A8_UNORM in the GENERAL layout; a pixel in memory
// is a D3DCOLOR (0xAARRGGBB, little-endian).

#pragma once

#include "vulkan/DeviceChoice.h"
#include "vulkan/Quads.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstddef>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan_core.h>

namespace patella::vulkan {

// Every failure of the back end (no loader, no device, a Vulkan call that failed).
class TVulkanError : public std::runtime_error {
public:
	explicit TVulkanError(const std::string &Message) : std::runtime_error(Message) {}
};

// The device the engine runs on (plan 6.3: "Automatic", a GPU, "CPU (compatibility)"; TDeviceKind is
// DeviceChoice.h's).
struct TDeviceRequest {
	TDeviceKind Kind = TDeviceKind::Automatic;
	int GpuIndex = -1; // the n-th of ListGpus, -1 the best
	// The CPU driver's manifest (lvp_icd.x86_64.json next to the executable). The loader is told
	// about it through its driver setting VK_DRIVER_FILES in this process only, and only when the CPU
	// driver is wanted; empty: no CPU driver.
	std::string CpuDriverManifest;
	// The window system's instance extension (VK_KHR_win32_surface; empty: headless).
	std::string SurfaceExtension;
	// Creates the window's surface on the instance (empty function: headless).
	std::function<VkSurfaceKHR(VkInstance)> CreateSurface;
	bool Validation = false;
};

struct TDeviceInfo {
	std::string Name; // the driver's UTF-8 name in the client's code page (issue #112: the options show it)
	VkPhysicalDeviceType Type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
	uint32_t VendorId = 0;
	uint32_t DeviceId = 0;
	uint32_t ApiVersion = 0;
	uint32_t DriverVersion = 0;
	bool CpuDriver = false; // the shipped CPU driver
};

// True when the Vulkan loader can be loaded (vulkan-1.dll, libvulkan.so.1).
bool LoaderAvailable();

// The GPUs the engine can use (Vulkan 1.3 with dynamic rendering and synchronization2, and
// VK_KHR_swapchain), in the loader's order: the list TDeviceRequest::GpuIndex and the advanced options'
// GPU button count in (issue #31). Headless: an instance of its own, without a window, gone again on
// return; the system's drivers only (no CPU driver). Throws TVulkanError without a loader. Its instance
// enables the same instance extensions as every other (Renderer.cpp NewInstanceBuilder: vk-bootstrap
// keeps the functions of the first instance of the process).
std::vector<TDeviceInfo> ListGpus();

// The index in ListGpus' list of the GPU "the best GPU" means (TDeviceRequest::GpuIndex -1): the first
// of the best kind (discrete, integrated, virtual, other); -1 for an empty list.
int BestGpu(const std::vector<TDeviceInfo> &Gpus);

// The CPU driver's device from its manifest (TDeviceRequest::CpuDriverManifest), headless as ListGpus;
// nothing when it offers none. Throws TVulkanError without a loader.
std::optional<TDeviceInfo> CpuDriverDevice(const std::string &Manifest);

class TRenderer {
public:
	TRenderer();
	~TRenderer();
	TRenderer(const TRenderer &) = delete;
	TRenderer &operator=(const TRenderer &) = delete;

	// Creates the instance, picks the device (TDeviceRequest), the queue, the memory allocator, the
	// pipelines and the first frame. Throws TVulkanError.
	void Create(const TDeviceRequest &Request);
	void Destroy();
	[[nodiscard]] bool Created() const;
	[[nodiscard]] const TDeviceInfo &Device() const;

	// An image of Width x Height, cleared to 0 (transparent black). Ids from 0 on.
	int CreateImage(int Width, int Height);
	// A new size; the contents are cleared.
	void ResizeImage(int Image, int Width, int Height);
	[[nodiscard]] int ImageWidth(int Image) const;
	[[nodiscard]] int ImageHeight(int Image) const;

	// Direct3D 9's ColorFill: the rectangle set to Color (D3DCOLOR_XRGB: alpha 1), no blending.
	void Fill(int Target, int Left, int Top, int Right, int Bottom, uint32_t Color);
	// Quads of Texture into Target with surfaceDX9.cpp's blending (source alpha) and Filter.
	void Draw(int Target, int Texture, TFilter Filter, const TQuad *Quads, size_t Count);
	// Width x Height pixels (D3DCOLOR, rows Pitch pixels apart) into Image at (x, y).
	void Upload(int Image, int x, int y, int Width, int Height, const uint32_t *Pixels, size_t Pitch);
	// Every pixel of the rectangle that is exactly Key (alpha included) becomes Replacement.
	void ReplaceColor(int Image, int x, int y, int Width, int Height, uint32_t Key, uint32_t Replacement);
	// The pixels of the rectangle (rows top-down, Width apart) after everything recorded so far; waits
	// for the device.
	void ReadBack(int Image, int x, int y, int Width, int Height, std::vector<uint32_t> &Pixels);

	// Ends the frame: with a surface, the rectangle of Image is shown in the window (WindowWidth x
	// WindowHeight; nothing is shown while that is 0, as for a minimised window); without one the
	// frame's work is only submitted. Returns true when a picture was shown.
	bool Present(int Image, int x, int y, int Width, int Height, uint32_t WindowWidth, uint32_t WindowHeight);
	// Submits what is recorded and waits until the device is idle.
	void Finish();

	// The present mode of the swapchain (0 before the first Present).
	[[nodiscard]] VkPresentModeKHR PresentMode() const;

	// Issue #134, the profiling tool (tools/profiling.py; the test build's only caller): with On every command
	// buffer of a frame gets two timestamp queries (from the next frame on), and TakeFrameTimes hands out the
	// device time of each finished frame, the sum over its submissions, in nanoseconds, a frame or two after
	// it was submitted. False: the device has no timestamps. Off by default: no query pool, no timing commands.
	bool SetFrameTimestamps(bool On);
	void TakeFrameTimes(std::vector<uint64_t> &Nanoseconds);

private:
	struct TState;
	std::unique_ptr<TState> State;
};

} // namespace patella::vulkan
