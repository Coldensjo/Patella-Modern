// WindowSystem.h: what the Vulkan engine asks of the window system (Patella-Modern issue #126, plan M8; notes/vulkan.md,
// "The window system"; notes/decisions.md, "#126"). Not part of the original source. No platform header: the window is
// a TWindowHandle (platform/Window.h), the surface Vulkan's.
//
// TSurfaceVulkan (SurfaceVulkan.cpp) reads the main window's client area and the screen's size, and creates each
// device's surface, through the window system of the client's window:
//
//	SdlWindowSystem()    the SDL3 window's (WindowSystemSdl.cpp): SDL_GetWindowSizeInPixels, the primary
//	                     display's bounds, SDL_Vulkan_GetInstanceExtensions and SDL_Vulkan_CreateSurface
//
// The surface goes on the device's own window (PresentWindow.h, issue #52: a child window per device, never the main
// window), which TPresentWindows makes and destroys.

#pragma once

#include "platform/Window.h"

#include <string>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan_core.h>

namespace patella::vulkan {

// A size in pixels.
struct TWindowSize {
	int Width = 0;
	int Height = 0;
};

class IWindowSystem {
public:
	IWindowSystem() = default;
	IWindowSystem(const IWindowSystem &) = delete;
	IWindowSystem &operator=(const IWindowSystem &) = delete;
	IWindowSystem(IWindowSystem &&) = delete;
	IWindowSystem &operator=(IWindowSystem &&) = delete;

	// The client area of Window (the main window) as GetClientRect gives it: 0 x 0 while the window is minimised or
	// unknown.
	[[nodiscard]] virtual TWindowSize ClientSize(platform::TWindowHandle Window) = 0;
	// The primary display's size (GetSystemMetrics' SM_CXSCREEN and SM_CYSCREEN): the windowed canvas's least size.
	[[nodiscard]] virtual TWindowSize ScreenSize() = 0;
	// The instance extension a surface of this window system needs (TDeviceRequest::SurfaceExtension; the loader's
	// VK_KHR_surface comes with it, Renderer.cpp). Empty when it cannot be known (CreateSurface then fails).
	[[nodiscard]] virtual std::string SurfaceExtension() = 0;
	// A surface on Window (a device's window, PresentWindow.h) for Instance, created with SurfaceExtension enabled.
	// Throws TVulkanError (Renderer.h) when it cannot be made.
	[[nodiscard]] virtual VkSurfaceKHR CreateSurface(VkInstance Instance, platform::TWindowHandle Window) = 0;

protected:
	~IWindowSystem() = default;
};

// The SDL3 window's (SDL's video subsystem started: SdlFrame.cpp's Create, tests_vulkan's main).
[[nodiscard]] IWindowSystem &SdlWindowSystem();

} // namespace patella::vulkan
