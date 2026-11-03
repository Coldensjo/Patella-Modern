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
// below. The oracle compiles the original's.
#if defined(PATELLA_VULKAN)
#include "Surface.h"
#include "graphics.h"
#include "Preferences.h"
#include "vulkan/DeviceSettings.h"
#include "common.h"
#include "vulkan/DeviceChoice.h"
bool IsVulkanInstalled();
TSurface *CreateSurfaceVulkan();
#else
// SurfaceDX.h's d3d.h must declare the DirectX 6/7 interfaces: real MFC's afxwin.h (common.h)
// includes d3d9types.h, which would set DIRECT3D_VERSION to 0x0900 first (issue #16).
#define DIRECT3D_VERSION 0x0700
#include "Surface.h"
#include "SurfaceDX.h"
#include "SurfaceGL.h"
#include "common.h"
#include "Preferences.h"
#endif

// Data, in .data and .bss order. The shared data the engines use (Surface.h)
// lies around this file's own; 0x71d0f8 and up are SurfaceDX.cpp's.
int CommandLineEngine = -1;    // 0x5c2564, set by Player.cpp from "engine N"
int CacheSurfaceWidth = 256;   // 0x5c2568
int CacheSurfaceHeight = 256;  // 0x5c256c
int InitStep;                  // 0x71d0bc
static HWND SurfaceWindow;     // 0x71d0c0, the main window, from SurfaceInitInstance
int curEngine;                 // 0x71d0c4
static uint8 AvailableEngines; // 0x71d0c8, bit 0 OpenGL works, bit 1 DirectX 9 passed TestEngine
static TSurface *Surface;      // 0x71d0cc
int BlitCount;                 // 0x71d0d0
int BlitPixels;                // 0x71d0d4
int Data71D0D8;                // 0x71d0d8
int ScreenWidth;               // 0x71d0dc
int ScreenHeight;              // 0x71d0e0
int ScreenDepth;               // 0x71d0e4
int LastBlitCount;             // 0x71d0e8
int LastBlitPixels;            // 0x71d0ec
int NumberOfSurfaces;          // 0x71d0f0
int Data71D0F4;                // 0x71d0f4

// 0x4eaa80. Written to error.txt as "Graphic Engine".
int GetCurrentEngine()
{
	return curEngine;
}

#if defined(PATELLA_VULKAN)
// The x64 client: curEngine is always 2, the Vulkan engine (SurfaceInitInstance).
TSurface *GetSurface()
{
	if (curEngine == 2 && Surface == nullptr)
		Surface = CreateSurfaceVulkan();
	return Surface;
}
#else
// 0x4eaa90. Creates the surface of the current engine on first use.
TSurface *GetSurface()
{
	if (curEngine == 1) {
		if (Surface == nullptr)
			Surface = new TSurfaceGL;
	} else if (curEngine == 2) {
		if (Surface == nullptr)
			Surface = CreateSurfaceDX9();
	} else if (curEngine == 0 && Surface == nullptr) {
		Surface = CreateSurfaceDX();
	}
	return Surface;
}
#endif

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
TSurface::~TSurface()
{
}

// 0x4eabe0. A member (Dialogs.cpp calls it on GetSurface()) that reads only the
// global.
uint8 TSurface::GetAvailableEngines()
{
	return AvailableEngines;
}

// 0x4eabf0
void TSurface::SetSurfaceRect(int Surface, int x, int y, int Width, int Height)
{
}

// 0x4eac00: SelectSurface and SetTextureFilter share this copy (identical
// code). Only SurfaceGL.cpp overrides them.
void TSurface::SelectSurface(int Surface)
{
}

void TSurface::SetTextureFilter(int Filter)
{
}

// 0x4eac10. Only SurfaceGL.cpp overrides it.
void TSurface::AddDirtyRect(TRect Rect)
{
}

// The linker kept the copy of this empty body from another object
// (0x496160, folded with other empty functions).
void TSurface::AttachWindow()
{
}

#if defined(PATELLA_VULKAN)
// The x64 client (issue #20): the Vulkan engine is the only engine, so curEngine is 2 whatever Tibia.cfg
// or the command line says, and everything that asks for the engine (graphics.cpp's colour keys, the
// frame rate line, error.txt's "Graphic Engine") sees the original's DirectX 9. Tibia.cfg's Engine keeps
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
		throw (PROGRAM_ERROR)12;
	}
	SetEngine(Engine);
}
#else
// 0x4eac20. Chooses the engine at start-up and stores it (Preferences).
// An "engine N" on the command line (0-2) is taken as it is, without
// detection. Otherwise the engine from Tibia.cfg counts if it is 0-2 and
// available: DirectX 9 when installed and its TestEngine passes, OpenGL when
// an accelerated pixel format exists and works; the first engine found
// available is taken when Tibia.cfg has none, DirectX 5 is the fallback.
// Odd behaviour kept: a failed DirectX 9 test also drops the engine chosen
// in Tibia.cfg, and when ChoosePixelFormat fails the function returns
// before storing the engine.
void SurfaceInitInstance(HWND *Window)
{
	SurfaceWindow = *Window;
	bool Chosen = false;
	if (CommandLineEngine >= 0 && CommandLineEngine < 3) {
		curEngine = CommandLineEngine;
		SetEngine(curEngine);
		return;
	}
	int Engine = curEngine = GetEngine();
	if (Engine >= 0 && Engine < 3)
		Chosen = true;
	try {
		if (IsDirect3D9Installed()) {
			curEngine = 2;
			GetSurface()->hWindow = *Window;
			if (GetSurface()->TestEngine()) {
				if (Surface != nullptr)
					delete Surface;
				AvailableEngines |= 2;
				Surface = nullptr;
				if (!Chosen) {
					curEngine = 2;
					Chosen = true;
				} else {
					curEngine = Engine;
				}
			} else {
				if (Surface != nullptr)
					delete Surface;
				Surface = nullptr;
				Chosen = false;
				curEngine = Engine;
			}
		}
	} catch (...) {
		throw (PROGRAM_ERROR)12;
	}
	HDC Context = GetDC(*Window);
	// clang-format off
	PIXELFORMATDESCRIPTOR Format = {
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DEPTH_DONTCARE,
		PFD_TYPE_RGBA, 32,
	};
	// clang-format on
	// The binary has the unwind states of a try block here but no handler:
	// ChoosePixelFormat and DescribePixelFormat are C functions, which the
	// compiler assumes cannot throw, so it removed the catch (its body is
	// unknown).
	try {
		int Number = ChoosePixelFormat(Context, &Format);
		if (Number == 0)
			return;
		DescribePixelFormat(Context, Number, sizeof(PIXELFORMATDESCRIPTOR), &Format);
	} catch (...) {
		throw (PROGRAM_ERROR)12;
	}
	if ((Format.dwFlags & PFD_GENERIC_ACCELERATED) == 0 && (Format.dwFlags & PFD_GENERIC_FORMAT) == 0
		&& IsOpenGLWorking(Context)) {
		AvailableEngines |= 1;
		if (!Chosen) {
			curEngine = 1;
			Chosen = true;
		}
	}
	if (Chosen && curEngine == 2 && (AvailableEngines & 2) == 0)
		curEngine = 1;
	if (!Chosen || (curEngine == 1 && (AvailableEngines & 1) == 0))
		curEngine = 0;
	SetEngine(curEngine);
}
#endif
