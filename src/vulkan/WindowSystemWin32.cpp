// WindowSystemWin32.cpp: the Vulkan engine's window system on the MFC window, until issue #127 (WindowSystem.h;
// Patella-Modern issue #126). Not part of the original source.
//
// PATELLA_WINDOW=mfc keeps MFC's frame window (platform/WindowChoice.h), on which SDL3 is not started, so the engine
// asks Win32 directly there, as it did on every path before issue #126: GetClientRect for the client area,
// GetSystemMetrics for the screen, VK_KHR_win32_surface's vkCreateWin32SurfaceKHR for the device's window
// (PresentWindow.h). Goes with MFC (issue #127).

#include "vulkan/WindowSystem.h"

#include "vulkan/Renderer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <volk.h>
#include <vulkan/vulkan_win32.h>

#include <string>

namespace patella::vulkan {

namespace {

class TWin32WindowSystem final : public IWindowSystem {
public:
	TWindowSize ClientSize(platform::TWindowHandle Window) override
	{
		RECT Client = {};
		if (Window == nullptr || GetClientRect(static_cast<HWND>(Window), &Client) == 0)
			return {};
		return {Client.right, Client.bottom};
	}

	TWindowSize ScreenSize() override { return {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)}; }

	std::string SurfaceExtension() override { return VK_KHR_WIN32_SURFACE_EXTENSION_NAME; }

	VkSurfaceKHR CreateSurface(VkInstance Instance, platform::TWindowHandle Window) override
	{
		auto Create = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(Instance, "vkCreateWin32SurfaceKHR"));
		VkWin32SurfaceCreateInfoKHR Info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
		Info.hinstance = GetModuleHandleA(nullptr);
		Info.hwnd = static_cast<HWND>(Window);
		VkSurfaceKHR Surface = VK_NULL_HANDLE;
		if (Create == nullptr || Create(Instance, &Info, nullptr, &Surface) != VK_SUCCESS)
			throw TVulkanError("vkCreateWin32SurfaceKHR failed");
		return Surface;
	}
};

} // namespace

IWindowSystem &Win32WindowSystem()
{
	static TWin32WindowSystem System;
	return System;
}

} // namespace patella::vulkan
