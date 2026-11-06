// Surface.cpp: reconstructed from Tibia.exe 7.72 (0x4eaa80-0x4eae6f).
//
// The first part of the Status/SurfaceDX gap: the current graphics engine,
// the surface object of that engine, the engine selection at start-up, and
// the base class TSurface, and the data the engines share. The file name is
// inferred (see Surface.h). No function has line evidence, so they are in
// binary order. The DirectDraw surface code after 0x4eae70 is SurfaceDX.cpp's.

// The Windows x64 client (PATELLA_VULKAN, cmake/WindowsX64.cmake) has one engine since Patella-Modern
// issue #20: the Vulkan engine (src/vulkan/SurfaceVulkan.h) as engine 2, without SurfaceDX.cpp,
// SurfaceGL.cpp and surfaceDX9.cpp; GetSurface and SurfaceInitInstance have their own versions for it
// below. The original's versions and the three engines left the fork with the 32-bit oracle (issue #94).
#include "Surface.h"
#include "graphics.h"
#include "Preferences.h"
#include "vulkan/DeviceSettings.h"
#include "common.h"
#include "vulkan/DeviceChoice.h"
bool IsVulkanInstalled();
TSurface *CreateSurfaceVulkan();

// Data, in .data and .bss order. The shared data the engines use (Surface.h)
// lies around this file's own; 0x71d0f8 and up are SurfaceDX.cpp's.
int CommandLineEngine = -1;   // 0x5c2564, set by Player.cpp from "engine N"
int CacheSurfaceWidth = 256;  // 0x5c2568
int CacheSurfaceHeight = 256; // 0x5c256c
int InitStep;                 // 0x71d0bc

namespace {

HWND SurfaceWindow; // 0x71d0c0, the main window, from SurfaceInitInstance

} // namespace

int curEngine; // 0x71d0c4

namespace {

uint8 AvailableEngines; // 0x71d0c8, bit 0 OpenGL works, bit 1 DirectX 9 passed TestEngine
TSurface *Surface;      // 0x71d0cc

} // namespace

int BlitCount;        // 0x71d0d0
int BlitPixels;       // 0x71d0d4
int Data71D0D8;       // 0x71d0d8
int ScreenWidth;      // 0x71d0dc
int ScreenHeight;     // 0x71d0e0
int ScreenDepth;      // 0x71d0e4
int LastBlitCount;    // 0x71d0e8
int LastBlitPixels;   // 0x71d0ec
int NumberOfSurfaces; // 0x71d0f0
int Data71D0F4;       // 0x71d0f4

// 0x4eaa80. Was written to error.txt as "Graphic Engine"; no caller in the client since issue #74.
int GetCurrentEngine()
{
	return curEngine;
}

// The x64 client: curEngine is always 2, the Vulkan engine (SurfaceInitInstance).
TSurface *GetSurface()
{
	if (curEngine == 2 && Surface == nullptr)
		Surface = CreateSurfaceVulkan();
	return Surface;
}

// 0x4eab60
void SurfaceExitInstance()
{
	if (Surface != nullptr)
		delete Surface;
	Surface = nullptr;
}

// 0x4eab80. The default test only takes the main window.
bool TSurface::TestEngine()
{
	hWindow = SurfaceWindow;
	return true;
}

// 0x4eab90
TSurface::TSurface()
{
	NumberOfModes = 0;
	Data3C = 0;
	CacheSurfaces = 0;
	Flag26 = false;
}

// 0x4eabb0
void TSurface::SetColorKey(int Surface, TRGBColor ColorKey)
{
	SetSourceColorKey(Surface, ColorKey);
}

// 0x4eabd0
TSurface::~TSurface() = default;

// 0x4eabe0. A member (Dialogs.cpp calls it on GetSurface()) that reads only the
// global.
uint8 TSurface::GetAvailableEngines()
{
	return AvailableEngines;
}

// 0x4eabf0
void TSurface::SetSurfaceRect([[maybe_unused]] int Surface, [[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Width, [[maybe_unused]] int Height)
{
}

// 0x4eac00: SelectSurface and SetTextureFilter share this copy (identical
// code). Only SurfaceGL.cpp overrides them.
void TSurface::SelectSurface([[maybe_unused]] int Surface)
{
}

void TSurface::SetTextureFilter([[maybe_unused]] int Filter)
{
}

// 0x4eac10. Only SurfaceGL.cpp overrides it.
void TSurface::AddDirtyRect([[maybe_unused]] TRect Rect)
{
}

// The linker kept the copy of this empty body from another object
// (0x496160, folded with other empty functions).
void TSurface::AttachWindow()
{
}

// The x64 client (issue #20): the Vulkan engine is the only engine, so curEngine is 2 whatever Tibia.cfg
// or the command line says, and everything that asks for the engine (graphics.cpp's colour keys, the
// frame rate line, GetCurrentEngine) sees the original's DirectX 9. Tibia.cfg's Engine keeps
// the original's number for Tibia.exe (plan 6.3): the command line's "engine N" (0-2) and a number 0-2 from
// Tibia.cfg are stored as they are, as the original stores them when the engine is available, and no
// number (-1) or one out of range becomes 2, the original's choice on a machine with Direct3D 9. As in
// the original, the command line skips the test. The engine's device is the graphics device choice
// (issue #31; vulkan/DeviceSettings.h: PATELLA_VULKAN, else Patella.ini, else automatic), read first;
// the advanced options' buttons choose it (Control.cpp ChangeEngine). When the chosen device fails the
// test (a GPU that is gone, a CPU driver that is missing), the engine runs on the automatic choice, as
// the original falls back to another engine; Patella.ini keeps the choice. AvailableEngines is set as
// the original's on a machine with Direct3D 9 and OpenGL when the test passes. Without a Vulkan device
// there is no engine to fall back to: curEngine stays 2 and the engine's InitSurfaces or CreateSurfaces
// fails with PROGRAM_ERROR 12, as the original's Direct3D 9 device would.
void SurfaceInitInstance(HWND *Window)
{
	SurfaceWindow = *Window;
	curEngine = 2;
	LoadDeviceChoice();
	if (CommandLineEngine >= 0 && CommandLineEngine < 3) {
		SetEngine(CommandLineEngine);
		return;
	}
	int Engine = GetEngine();
	if (Engine < 0 || Engine >= 3)
		Engine = 2;
	try {
		if (IsVulkanInstalled()) {
			GetSurface()->hWindow = *Window;
			bool Works = GetSurface()->TestEngine();
			if (!Works && GetDeviceChoice().Kind != patella::vulkan::TDeviceKind::Automatic) {
				SetDeviceChoice(patella::vulkan::TDeviceChoice{});
				Works = GetSurface()->TestEngine();
			}
			if (Works)
				AvailableEngines |= 3;
			if (Surface != nullptr)
				delete Surface;
			Surface = nullptr;
		}
	} catch (...) {
		throw TProgramError{PROGRAM_ERROR::SURFACE};
	}
	SetEngine(Engine);
}
