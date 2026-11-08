// Surface.h: TSurface, the platform's drawing surface (Patella-Modern issue #138, plan M7; notes/decisions.md,
// "#138: TSurface as a platform interface"). Portable: no platform header.
//
// TSurface is the original's base class of the graphics engines (Surface.cpp, vtable 0x56ef78): the client draws
// through it (graphics.cpp, the window files, Control.cpp) and the engine implements it. It moved here from
// src/Surface.h with its slots, their order and their default bodies (still Surface.cpp's code, at its addresses),
// so that the engine is a platform implementation behind it:
//
//	TSurfaceVulkan   the Vulkan engine (vulkan/SurfaceVulkan.h): Patella.exe's and PatellaTest.exe's; its surfaces are
//	                 SDL_Vulkan_CreateSurface's since M8's #126 (vulkan/WindowSystem.h)
//	the golden tests' recording surfaces (tests/golden: testsurface.h, commworld.cpp's)
//
// No Win32 type in it: the window is a TWindowHandle (platform/Window.h), ClipRect's rectangles and ScreenRect a
// TEdgeRect (RECT's four edges), SurfaceGL.cpp's device context a pointer. What the engine shares with the client
// (Surface.cpp's data: the blit statistics, the screen, the cache surfaces' size, the main window, the available
// engines) is a TSurfaceShared that the surface's creator passes to its constructor (TGraphicsEngine, src/Surface.h),
// instead of the client's accessor (bridge::GraphicsEngine(), until issue #138).
//
// The names from the binary: hWindow, ViewMode, the surface and mode constants (SurfaceDX.cpp's and surfaceDX9.cpp's
// assertions), TestEngine, InitSurfaces and ExitSurfaces (ChangeEngine's error messages); the rest are ours.

#pragma once

#include "platform/Window.h"
#include "types/GraphicsTypes.h"
#include <cstdint>
#include <type_traits>

struct TFullscreenMode;
class TSurface;

// View modes (TSurface::ViewMode, Init); 0 is none (not initialised). The int constants of the int members and
// parameters, unscoped as the other unnamed ones (notes/decisions.md, "The other ten stay unscoped").
enum : int { // NOLINT(cppcoreguidelines-use-enum-class): int constants, above
	WINDOWED_MODE = 1,
	FULLSCREEN_MODE = 2,
};
static_assert(sizeof(decltype(WINDOWED_MODE)) == 4);
static_assert(std::is_same_v<std::underlying_type_t<decltype(WINDOWED_MODE)>, int>);

// The surfaces of an engine, by index (the int Surface/ToSurface/FromSurface
// arguments of the drawing slots). SPRITE_SURFACE is the first of the video
// cache surfaces (graphics.cpp's Item.Surface + 6). The names are from the
// assertions of SurfaceDX.cpp and surfaceDX9.cpp ("Surface9[SPRITE_SURFACE]");
// NUMBER_OF_SURFACES is SurfaceDX.cpp's count (surfaceDX9.cpp has its own
// NUMBER_OF_SURFACES_DX9). Unscoped int constants, as the view modes'.
enum : int { // NOLINT(cppcoreguidelines-use-enum-class): int constants, above
	PRIMARY_SURFACE = 0,
	SCREEN_SURFACE = 1,
	TEMP_SURFACE = 2,
	STRETCH_SURFACE = 3,
	CACHE_SURFACE = 4,
	AUTOMAP_SURFACE = 5,
	SPRITE_SURFACE = 6,
	NUMBER_OF_SURFACES = 22,
};
static_assert(sizeof(decltype(PRIMARY_SURFACE)) == 4);
static_assert(std::is_same_v<std::underlying_type_t<decltype(PRIMARY_SURFACE)>, int>);

namespace patella::platform {

// A rectangle by its edges, in pixels: Win32's RECT (left, top inside; right, bottom outside), the same members in
// the same order, for ClipRect and ScreenRect (TRect is a position and a size).
struct TEdgeRect {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
};

// The data the engines share with the client: Surface.cpp's (TGraphicsEngine's since issue #116, which derives from
// this since issue #138). The engine writes the statistics, the screen's size, the surfaces counted, the
// initialisation step and the cache surfaces' size; the client reads them (the frame rate line, the dumps, the golden
// tests). The main window and the available engines are Surface.cpp's file-local values: TGraphicsEngine sets them
// (SurfaceInitInstance), TSurface reads them (TestEngine, GetAvailableEngines). The ints in the original's .data and
// .bss order; the pointer first and the byte last, so the object has no padding but its end and TGraphicsEngine keeps
// its size (80 bytes on x64).
class TSurfaceShared {
public:
	TSurfaceShared(const TSurfaceShared &) = delete;
	TSurfaceShared &operator=(const TSurfaceShared &) = delete;
	TSurfaceShared(TSurfaceShared &&) = delete;
	TSurfaceShared &operator=(TSurfaceShared &&) = delete;

protected:
	TSurfaceShared() = default;
	// Never destroyed through this base (TGraphicsEngine is TGraphics' member).
	~TSurfaceShared() = default;

	friend class ::TSurface;
	TWindowHandle SurfaceWindow{}; // 0x71d0c0, the main window, from SurfaceInitInstance

public:
	// .data
	int CacheSurfaceWidth = 256;  // 0x5c2568, the size of a video cache surface (256 at start)
	int CacheSurfaceHeight = 256; // 0x5c256c
	// .bss. The statistics count the blits of a frame (the engines').
	int InitStep{};         // 0x71d0bc, the step reached in Direct3D's initialisation; 0 done
	int BlitCount{};        // 0x71d0d0
	int BlitPixels{};       // 0x71d0d4
	int Data71D0D8{};       // 0x71d0d8, cleared by surfaceDX9.cpp and SurfaceGL.cpp
	int ScreenWidth{};      // 0x71d0dc, of the last frame
	int ScreenHeight{};     // 0x71d0e0
	int ScreenDepth{};      // 0x71d0e4
	int LastBlitCount{};    // 0x71d0e8, BlitCount of the last frame
	int LastBlitPixels{};   // 0x71d0ec
	int NumberOfSurfaces{}; // 0x71d0f0, the surfaces created (SurfaceDX.cpp's SurfaceMemory entries)
	int Data71D0F4{};       // 0x71d0f4, SurfaceGL.cpp's

protected:
	std::uint8_t AvailableEngines{}; // 0x71d0c8, bit 0 OpenGL works, bit 1 DirectX 9 passed TestEngine
};

} // namespace patella::platform

// The vtable is at 0x56ef78. Slots with _purecall in the binary are pure;
// the others have the default bodies in Surface.cpp. Slot names and
// parameters come from the calls (graphics.cpp and the window files) and
// from SurfaceDX.cpp's implementations; ToSurface, FromSurface, VMode, GMode
// and ViewMode are the binary's (assertions), the other names are ours.
// Surfaces 6 and up are the video cache surfaces.
class TSurface {
public:
	// Shared: the data the engines share with the client (above), the creator's; it outlives the surface.
	explicit TSurface(patella::platform::TSurfaceShared &Shared);
	TSurface(const TSurface &) = delete;
	TSurface &operator=(const TSurface &) = delete;
	TSurface(TSurface &&) = delete;
	TSurface &operator=(TSurface &&) = delete;
	virtual ~TSurface();                                                                         // +0x00
	virtual void SelectSurface(int Surface);                                                     // +0x04, empty; SurfaceGL.cpp's drawing target
	virtual void SetTextureFilter(int Filter);                                                   // +0x08, empty; SurfaceGL.cpp's GL_LINEAR/GL_NEAREST
	virtual void SetPixel(int Surface, int x, int y, TRGBColor Color) = 0;                       // +0x0c, ContentWindows.cpp's automap cross
	virtual void FillRectangleClipped(int Surface, TRect Rect, TRGBColor Color, TRect Clip) = 0; // +0x10, ContentWindows.cpp's bars
	virtual void FillRectangle(int Surface, TRect Rect, TRGBColor Color) = 0;                    // +0x14
	virtual void DrawHorizontalLine(int Surface, int x, int y, int Width, TRGBColor Color) = 0;  // +0x18, ContentWindows.cpp's separator line
	virtual void DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor Color) = 0;   // +0x1c, DialogElements.cpp's edit line cursor
	virtual void DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color,
		TRect Clip) = 0;                                                                                 // +0x20
	virtual void DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color) = 0;                 // +0x24
	virtual void WriteImage(int ToSurface, int x, int y, int Width, int Height, std::uint8_t *Data) = 0; // +0x28
	virtual void WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, std::uint8_t *Data,
		std::uint8_t *Mask, TRGBColor *Colors) = 0; // +0x2c
	// The blits take the source part as a TRect FromRect (and StretchBlit the
	// target as ToRect): the assertions name FromRect.width/height and
	// ToRect.width/height, and every caller builds the 16-byte block on the
	// stack (issue #29).
	virtual void BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect) = 0; // +0x30
	virtual void Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect,
		TRGBColor ColorKey) = 0; // +0x34
	// +0x38: BlitRect's worker, a copy without colour key, clipped.
	virtual void BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect,
		TRect Clip) = 0;
	virtual void BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect,
		TRGBColor ColorKey, TRect Clip) = 0; // +0x3c
	virtual void StretchBlitRect(int ToSurface, TRect Destination, int FromSurface,
		TRect Source) = 0; // +0x40
	// +0x44: StretchBlit without the clipping rectangle.
	virtual void StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect,
		TRGBColor ColorKey, TRGBColor Highlight) = 0;
	virtual void StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect,
		TRGBColor ColorKey, TRGBColor Highlight, TRect Clip) = 0; // +0x48
	// +0x4c: shades Count rectangles with the colours of their four corners
	// (top left, top right, bottom left, bottom right); MapWindow.cpp's light.
	virtual void ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors) = 0;
	virtual void SetColorKey(int Surface, TRGBColor ColorKey);           // +0x50, calls +0x54; graphics.cpp sets a cache surface's colour key
	virtual void SetSourceColorKey(int Surface, TRGBColor ColorKey) = 0; // +0x54
	// +0x58: clips Destination to Clip and moves the edges of Source by as
	// much; false when nothing is left.
	virtual bool ClipRect(TRect Clip, patella::platform::TEdgeRect *Destination, patella::platform::TEdgeRect *Source) = 0;
	virtual void ResetFrameTimer() = 0;                                    // +0x5c
	virtual void WaitForNextFrame() = 0;                                   // +0x60, the frame limiter and the FPS count
	virtual void AttachWindow();                                           // +0x64, empty; the window handle changed
	virtual void UpdateWindowRect() = 0;                                   // +0x68
	virtual void InitDisplay(int VMode, TFullscreenMode *GMode) = 0;       // +0x6c
	virtual void ExitDisplay() = 0;                                        // +0x70
	virtual void CreateSurfaces() = 0;                                     // +0x74
	virtual void ResetDisplay(int VMode, TFullscreenMode *GMode) = 0;      // +0x78
	virtual void RestoreSurfaces(int ViewMode, TFullscreenMode *Mode) = 0; // +0x7c
	virtual void AddDirtyRect(TRect Rect);                                 // +0x80, empty; SurfaceGL.cpp's area drawn into the selected surface
	virtual void FinishFrame() = 0;                                        // +0x84
	// +0x88: Window is the main window (TGUI's WindowHandle), read during the call.
	virtual bool Init(patella::platform::TWindowHandle *Window, int ViewMode, TFullscreenMode *Resolution, bool Force) = 0;
	virtual int GetFullscreenModes(TFullscreenMode **Modes) = 0;                   // +0x8c, a new[] list; the number of modes
	virtual int GetCacheSurfaces() = 0;                                            // +0x90, the number of video cache surfaces
	virtual void GetSurfaceRect(int Surface, TRect *Rect) = 0;                     // +0x94
	virtual void SetSurfaceRect(int Surface, int x, int y, int Width, int Height); // +0x98, empty here (surfaceDX9.cpp sets Rect[Surface])
	// +0x9c: the surfaces were created again since the last Init (their
	// contents are gone); MapWindow.cpp forgets its tile cache then.
	virtual bool SurfaceRecreated(int Surface) = 0;
	virtual bool IsUnavailable() = 0;                  // +0xa0
	virtual bool CanShadeRectangles() = 0;             // +0xa4, MapWindow.cpp draws the light with +0x4c only when true
	virtual int GetSurfaceMemory(int Index) = 0;       // +0xa8, 1 video memory, 2 system memory
	virtual void SetFramerateLimit(int Framerate) = 0; // +0xac
	virtual double GetFPS() = 0;                       // +0xb0
	virtual void InitSurfaces() = 0;                   // +0xb4
	virtual void ExitSurfaces() = 0;                   // +0xb8
	virtual bool TestEngine();                         // +0xbc
	std::uint8_t GetAvailableEngines();                // 0x4eabe0, the engines found at start-up (bit 0 OpenGL, bit 1 DirectX 9)

	// The members the engines share: the engines' identical copies of
	// GetCacheSurfaces, IsUnavailable, GetFPS and the frame timer use the same
	// offsets (SurfaceDX.cpp's vtable has SurfaceGL.cpp's and
	// surfaceDX9.cpp's copies). The doubles align the class to 8 bytes, so
	// +0x04 is padding after the vtable pointer.
	patella::platform::TWindowHandle hWindow; // +0x08, the main window (TestEngine, SurfaceInitInstance)
	void *DeviceContext;                      // +0x0c, SurfaceGL.cpp's device context of the window (an HDC)
	int ViewMode;                             // +0x10, 0 none, WINDOWED_MODE, FULLSCREEN_MODE
	int Width;                                // +0x14, the fullscreen mode
	int Height;                               // +0x18
	int Depth;                                // +0x1c
	int Data20;                               // +0x20
	bool Unavailable;                         // +0x24, no exclusive mode (another application has it)
	bool CanStretch;                          // +0x25, the hardware stretches blits
	bool Flag26;                              // +0x26, cleared by the constructor
	std::uint8_t Data27;                      // +0x27
	patella::platform::TEdgeRect ScreenRect;  // +0x28, SurfaceGL.cpp's client area in screen coordinates
	int NumberOfModes;                        // +0x38, cleared by the constructor; GetFullscreenModes' count
	int Data3C;                               // +0x3c, cleared by the constructor
	int CacheSurfaces;                        // +0x40, cleared by the constructor
	int Data44;                               // +0x44
	double StartTime;                         // +0x48, timeGetTime at ResetFrameTimer
	double NextFrame;                         // +0x50, the time the next frame is due
	double FrameTime;                         // +0x58, 1000 / Framerate
	double FPS;                               // +0x60, frames per second, counted each second
	int Framerate;                            // +0x68, the frame limit; 0xffff is none
	int Data6C;                               // +0x6c

protected:
	// The engine's shared data (issue #138: the client's bridge::GraphicsEngine() until then). After the original's
	// members, so their order is the original's.
	patella::platform::TSurfaceShared &Shared;
};
