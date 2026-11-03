// Vma.cpp: the implementation of the Vulkan Memory Allocator (VMA, a header-only library; cmake/Vulkan.cmake).
// It gets its Vulkan functions from the allocator's creation (Renderer.cpp passes volk's
// vkGetInstanceProcAddr and vkGetDeviceProcAddr), not from prototypes: volk replaces those.
// Compiled with the other libraries' sources, without the product's warning settings.

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include <volk.h>

#include <vk_mem_alloc.h>
