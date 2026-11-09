// WindowSystemSdl.cpp: the Vulkan engine's window system on the SDL3 window (WindowSystem.h; Patella-Modern issue
// #126, plan M8). Not part of the original source.
//
// The main window is SdlFrame.cpp's SDL_Window, which the client knows by its native handle (TWindowHandle, the HWND
// on Windows: TGUI's, TNetwork's and the surface's window since issue #122); the window system finds the SDL_Window of
// a handle among SDL's windows. Each device's surface is SDL_Vulkan_CreateSurface's, on the device's own window
// (PresentWindow.h, issue #52). SDL3 has no child windows of its own, so that window is a native child window which
// SDL wraps for the call (SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, with Vulkan); the wrapper goes again as soon as
// the surface exists. The surface belongs to the native window, which TPresentWindows keeps for the device's life; SDL's
// window procedure stays off the device windows, which must keep letting every mouse message through to the main
// window and paint nothing (the wrapper's events, its creation, size and destruction, name its own window id, which
// SdlFrame.cpp's event watch ignores).

#include "vulkan/WindowSystem.h"

#include "platform/Window.h"
#include "vulkan/Renderer.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#if defined(_WIN32)
#include <SDL3/SDL_properties.h>
#endif
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <string>
#include <string_view>

namespace patella::vulkan {

namespace {

// The native handle of an SDL window (the HWND on Windows).
void *NativeHandle(SDL_Window *Window)
{
#if defined(_WIN32)
	return SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
	return Window; // Linux: the client's handle is the SDL_Window (SdlFrame.cpp, issue #131)
#endif
}

class TSdlWindowSystem final : public IWindowSystem {
public:
	TWindowSize ClientSize(platform::TWindowHandle Window) override
	{
		SDL_Window *const Found = Find(Window);
		if (Found == nullptr || (SDL_GetWindowFlags(Found) & SDL_WINDOW_MINIMIZED) != 0)
			return {};
		TWindowSize Size;
		if (!SDL_GetWindowSizeInPixels(Found, &Size.Width, &Size.Height))
			return {};
		return Size;
	}

	TWindowSize ScreenSize() override
	{
		SDL_Rect Bounds{};
		if (SDL_WasInit(SDL_INIT_VIDEO) == 0 || !SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &Bounds))
			return {};
		return {Bounds.w, Bounds.h};
	}

	std::string SurfaceExtension() override
	{
		if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
			return {};
		Uint32 Count = 0;
		const char *const *Names = SDL_Vulkan_GetInstanceExtensions(&Count);
		for (Uint32 i = 0; Names != nullptr && i < Count; i++) {
			if (std::string_view(Names[i]) != VK_KHR_SURFACE_EXTENSION_NAME)
				return Names[i];
		}
		return {};
	}

	VkSurfaceKHR CreateSurface(VkInstance Instance, platform::TWindowHandle Window) override
	{
		if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
			throw TVulkanError("SDL_Vulkan_CreateSurface: SDL's video subsystem is not started");
#if !defined(_WIN32)
		// Issue #131: SDL3 has no child windows here: the surface is the window's own (PresentWindow.h), the main window's,
		// or the hidden one of the device test (TTestWindow).
		if (Window == nullptr || Find(Window) == nullptr)
			throw TVulkanError("SDL_Vulkan_CreateSurface: not a window of this process");
		VkSurfaceKHR Own = VK_NULL_HANDLE;
		if (!SDL_Vulkan_CreateSurface(Find(Window), Instance, nullptr, &Own) || Own == VK_NULL_HANDLE)
			throw TVulkanError(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
		return Own;
#else
		// Never one of SDL's own windows (the main window): a second SDL window on its handle would take over its data.
		if (Window == nullptr || Find(Window) != nullptr)
			throw TVulkanError("SDL_Vulkan_CreateSurface: not a device's window");
		const SDL_PropertiesID Properties = SDL_CreateProperties();
		SDL_SetPointerProperty(Properties, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, Window);
		SDL_SetBooleanProperty(Properties, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
		SDL_Window *const Wrapper = SDL_CreateWindowWithProperties(Properties);
		SDL_DestroyProperties(Properties);
		if (Wrapper == nullptr)
			throw TVulkanError(std::string("SDL_CreateWindowWithProperties failed: ") + SDL_GetError());
		VkSurfaceKHR Surface = VK_NULL_HANDLE;
		const bool Created = SDL_Vulkan_CreateSurface(Wrapper, Instance, nullptr, &Surface);
		const std::string Error = Created ? std::string() : std::string(SDL_GetError());
		SDL_DestroyWindow(Wrapper);
		if (!Created || Surface == VK_NULL_HANDLE)
			throw TVulkanError("SDL_Vulkan_CreateSurface failed: " + Error);
		return Surface;
#endif
	}

private:
	// The SDL window whose native handle is Window (the last one found is kept: the main window, asked every frame).
	SDL_Window *Find(platform::TWindowHandle Window)
	{
		if (Window == nullptr || SDL_WasInit(SDL_INIT_VIDEO) == 0)
			return nullptr;
		if (Last != nullptr && SDL_GetWindowID(Last) == LastId && NativeHandle(Last) == Window)
			return Last;
		Last = nullptr;
		LastId = 0;
		int Count = 0;
		SDL_Window **Windows = SDL_GetWindows(&Count);
		for (int i = 0; Windows != nullptr && i < Count; i++) {
			if (NativeHandle(Windows[i]) == Window) {
				Last = Windows[i];
				LastId = SDL_GetWindowID(Last);
				break;
			}
		}
		SDL_free(static_cast<void *>(Windows)); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): SDL_GetWindows' array
		return Last;
	}

	SDL_Window *Last = nullptr;
	SDL_WindowID LastId = 0;
};

} // namespace

IWindowSystem &SdlWindowSystem()
{
	static TSdlWindowSystem System;
	return System;
}

} // namespace patella::vulkan
