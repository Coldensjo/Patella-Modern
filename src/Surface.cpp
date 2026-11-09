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
#include "types/GraphicsTypes.h"
#include "Preferences.h"
#include "vulkan/DeviceSettings.h"
#include "common.h"
#include "vulkan/DeviceChoice.h"
#include "platform/Window.h"
bool IsVulkanInstalled();
TSurface *CreateSurfaceVulkan(patella::platform::TSurfaceShared &Shared);

// Data: TGraphicsEngine's members since issue #116 (Surface.h), in the original's .data and .bss order. The shared
// data the engines use lies around this file's own; 0x71d0f8 and up are SurfaceDX.cpp's.

// 0x4eaa80. Was written to error.txt as "Graphic Engine"; no caller in the client since issue #74.
int patella::TGraphicsEngine::GetCurrentEngine() const
{
	return curEngine;
}

// The x64 client: curEngine is always 2, the Vulkan engine (SurfaceInitInstance).
TSurface *patella::TGraphicsEngine::GetSurface()
{
	if (curEngine == 2 && Surface == nullptr)
		Surface = CreateSurfaceVulkan(*this);
	return Surface;
}

// 0x4eab60
void patella::TGraphicsEngine::SurfaceExitInstance()
{
	if (Surface != nullptr)
		delete Surface;
	Surface = nullptr;
}

// 0x4eab80. The default test only takes the main window (the engine's shared data since issue #138).
bool TSurface::TestEngine()
{
	hWindow = Shared.SurfaceWindow;
	return true;
}

// 0x4eab90. Shared: issue #138's (platform/Surface.h).
TSurface::TSurface(patella::platform::TSurfaceShared &Shared) : Shared(Shared)
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
// global (TGraphicsEngine's since issue #116, the engine's shared data since issue #138).
uint8 TSurface::GetAvailableEngines() // NOLINT(readability-make-member-function-const): the golden corpus's table takes it as a pointer to this non-const member
{
	return Shared.AvailableEngines;
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
void patella::TGraphicsEngine::SurfaceInitInstance(platform::TWindowHandle Window)
{
	SurfaceWindow = Window;
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
			GetSurface()->hWindow = Window;
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

// The original's free functions (issue #116): each calls its member on the one TClient's TGraphicsEngine. The other
// files call them, and the golden tests fake GetSurface and SurfaceExitInstance by these symbols.
int GetCurrentEngine()
{
	return patella::bridge::GraphicsEngine().GetCurrentEngine();
}

TSurface *GetSurface()
{
	return patella::bridge::GraphicsEngine().GetSurface();
}

void SurfaceExitInstance()
{
	patella::bridge::GraphicsEngine().SurfaceExitInstance();
}

void SurfaceInitInstance(patella::platform::TWindowHandle Window)
{
	patella::bridge::GraphicsEngine().SurfaceInitInstance(Window);
}
