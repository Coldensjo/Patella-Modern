// tests_core_link (issue #111, plan M7's exit): patella_core is the whole client but the platform layer. This program
// links the library's TClient (Client.cpp, which makes every subsystem: the network, the protocol, the game state, the
// graphics, the preferences and the GUI with its windows and dialogs) and gives it nothing but the platform layer's
// entry points, below. A symbol of the client that the library lacks fails this target's link on every preset: the
// build is the check, and the program is not run (sysinc.cpp's StartTime reads the platform's clock before main, as
// the original's static initialiser did, and these entry points are no platform).
//
// The entry points are the platform layer's (cmake/Core.cmake): platform::Platform() (src/platform, Win32Platform.cpp
// in the client), GetSurface and SurfaceExitInstance (Surface.cpp, the engine selection) and the graphics device
// choice (vulkan/DeviceSettings.h, the Vulkan engine's; vulkan/DeviceChoice.cpp is linked as it is). Each stops the
// program if it is called.

#include "Client.h"
#include "Surface.h"
#include "platform/Platform.h"
#include "vulkan/DeviceChoice.h"
#include "vulkan/DeviceSettings.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

[[noreturn]] void NotThePlatform(const char *Name)
{
	std::fprintf(stderr, "tests_core_link: the platform's %s called\n", Name);
	std::abort();
}

} // namespace

patella::platform::TPlatform &patella::platform::Platform()
{
	NotThePlatform("Platform");
}

TSurface *GetSurface()
{
	NotThePlatform("GetSurface");
}

void SurfaceExitInstance()
{
	NotThePlatform("SurfaceExitInstance");
}

patella::vulkan::TDeviceChoice GetDeviceChoice()
{
	NotThePlatform("GetDeviceChoice");
}

void SetDeviceChoice([[maybe_unused]] const patella::vulkan::TDeviceChoice &Choice)
{
	NotThePlatform("SetDeviceChoice");
}

bool SaveDeviceChoice()
{
	NotThePlatform("SaveDeviceChoice");
}

int GetNumberOfGpus()
{
	NotThePlatform("GetNumberOfGpus");
}

int GetBestGpu()
{
	NotThePlatform("GetBestGpu");
}

const std::string &GetCpuDriverManifest()
{
	NotThePlatform("GetCpuDriverManifest");
}

patella::vulkan::TDeviceButton DeviceButtonOfEngine([[maybe_unused]] int Engine)
{
	NotThePlatform("DeviceButtonOfEngine");
}

const char *GetDeviceName()
{
	NotThePlatform("GetDeviceName");
}

int main(int argc, [[maybe_unused]] char **argv)
{
	// The accessors' addresses, through a volatile so the linker keeps Client.cpp's object and everything it needs.
	using TAccessor = patella::TClient &(*)();
	volatile TAccessor Accessor = &patella::bridge::Client;
	// Makes the call reachable for the linker; the program is not run (above).
	if (argc > 100)
		Accessor();
	std::puts("tests_core_link: patella_core links with the platform layer's entry points only");
	return EXIT_SUCCESS;
}
