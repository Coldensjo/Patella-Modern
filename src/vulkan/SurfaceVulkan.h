// SurfaceVulkan.h: the Vulkan engine's surface on the client's window (issue #19; notes/vulkan.md; plan 6.2).
//
// TSurfaceVulkan does what surfaceDX9.cpp's TSurfaceDX9 does, call for call, the reference engine of
// the frames (plan D7): the same surfaces and sizes, the same batching of quads (drawn when the
// texture or the target changes, when 9,600 vertices are queued, and at the end of the frame), the
// same colour fills, uploads, colour key and light, and the same statistics. Instead of Direct3D 9 it
// drives the Vulkan back end (Renderer.h); the display modes come from the platform's list (SDL3's since issue #152), in the order
// and with the filter DirectDraw 4 gave surfaceDX9.cpp (issue #20). Windows x64 client only
// (cmake/Vulkan.cmake).
//
// The engine is the x64 client's only engine, curEngine 2 (DirectX 9 in the original and in the oracle;
// Surface.cpp; notes/vulkan.md, "Choosing the engine"). Its device is the graphics device choice of
// DeviceSettings.h (issue #31): the advanced options' buttons, Patella.ini's Device, PATELLA_VULKAN. Its window system
// (the client area, the screen, the surfaces) is SDL3's, vulkan/WindowSystem.h (issue #126).

#pragma once

#include "Surface.h"
#include "compat/msvc.h"
#include "graphics.h"
#include "vulkan/FilterMask.h"
#include "vulkan/Renderer.h"
#include <array>
#include <memory>
#include <vector>
#include <type_traits>
#include "common.h"
#include "vulkan/Quads.h"
#include <cstddef>
#include <cstdint>

enum : int { // NOLINT(cppcoreguidelines-use-enum-class): an int constant, as in platform/Surface.h
	NUMBER_OF_SURFACES_VULKAN = 22
};
static_assert(sizeof(decltype(NUMBER_OF_SURFACES_VULKAN)) == 4);
static_assert(std::is_same_v<std::underlying_type_t<decltype(NUMBER_OF_SURFACES_VULKAN)>, int>);

// The functions of the frame observer, the test build's frame dump (src/TestHooks.h, PATELLA_TEST_FRAME_OBSERVERS;
// none in Patella.exe, which looks up no patcap.dll since issue #97): told about the device, asked before each
// frame is shown whether it wants the frame's pixels.
using TVulkanDeviceObserver = void(PATELLA_CDECL *)(const char *Name, int Type, int CpuDriver);
using TVulkanFrameObserver = int(PATELLA_CDECL *)(int Width, int Height);
using TVulkanPixelsObserver = void(PATELLA_CDECL *)(const void *Bits, int Pitch, int Width, int Height);
// Before the pixels: a byte per pixel of the picture, 1 where its value comes from the device's filter or
// interpolation (the bilinear stretches, the light; notes/vulkan.md, "Pixels"), else 0.
using TVulkanMaskObserver = void(PATELLA_CDECL *)(const unsigned char *Mask, int Width, int Height);

class TSurfaceVulkan final : public TSurface {
public:
	// Shared: the data the engine shares with the client (platform/Surface.h; TGraphicsEngine, issue #138).
	explicit TSurfaceVulkan(patella::platform::TSurfaceShared &Shared);
	~TSurfaceVulkan() override;

	void SetPixel(int Surface, int x, int y, TRGBColor Color) override;
	void FillRectangleClipped(int Surface, TRect Rect, TRGBColor Color, TRect Clip) override;
	void FillRectangle(int Surface, TRect Rect, TRGBColor Color) override;
	void DrawHorizontalLine(int Surface, int x, int y, int Width, TRGBColor Color) override;
	void DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor Color) override;
	void DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color, TRect Clip) override;
	void DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color) override;
	void WriteImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data) override;
	void WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data, uint8 *Mask,
		TRGBColor *Colors) override;
	void BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect) override;
	void Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey) override;
	void BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip) override;
	void BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRect Clip) override;
	void StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source) override;
	void StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight) override;
	void StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight, TRect Clip) override;
	void ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors) override;
	void SetSourceColorKey(int ToSurface, TRGBColor ColorKey) override;
	bool ClipRect(TRect Clip, patella::platform::TEdgeRect *Rect, patella::platform::TEdgeRect *Source) override;
	void ResetFrameTimer() override;
	void WaitForNextFrame() override;
	void UpdateWindowRect() override;
	void InitDisplay(int VMode, TFullscreenMode *GMode) override;
	void ExitDisplay() override;
	void CreateSurfaces() override;
	void ResetDisplay(int VMode, TFullscreenMode *GMode) override;
	void RestoreSurfaces(int ViewMode, TFullscreenMode *Resolution) override;
	void FinishFrame() override;
	bool Init(patella::platform::TWindowHandle *Window, int ViewMode, TFullscreenMode *Resolution, bool Force) override;
	int GetFullscreenModes(TFullscreenMode **Modes) override;
	int GetCacheSurfaces() override;
	void GetSurfaceRect(int Surface, TRect *Rect) override;
	void SetSurfaceRect(int Surface, int x, int y, int Width, int Height) override;
	bool SurfaceRecreated(int Surface) override;
	bool IsUnavailable() override;
	bool CanShadeRectangles() override;
	int GetSurfaceMemory(int Surface) override;
	void SetFramerateLimit(int Limit) override;
	double GetFPS() override;
	void InitSurfaces() override;
	void ExitSurfaces() override;
	bool TestEngine() override;

	// The read-back of the test mode (issue #22's frame dump): the pixels of SCREEN_SURFACE's
	// rectangle as the next FinishFrame would show them (D3DCOLOR, rows top-down). False without a
	// device.
	bool ReadScreen(std::vector<uint32_t> &Pixels, int &Width, int &Height);

	// The device's name ("" without one; DeviceSettings.h GetDeviceName).
	[[nodiscard]] const char *DeviceName() const;

private:
	using TEdgeRect = patella::platform::TEdgeRect; // the rectangles (RECT's edges; RECTs until issue #126)

	void AddShadeQuad(int Left, int Top, int Right, int Bottom, uint32 TopLeft, uint32 TopRight, uint32 BottomRight,
		uint32 BottomLeft);
	void FlushVertices();
	void SetTexture(int Surface);
	void SetRenderTarget(int Surface);
	void ApplyColorKey(int Surface, TRGBColor ColorKey);
	void FlushShade();
	void AddQuad(int Left, int Top, int Right, int Bottom, double Left_u, double Right_u, double Top_v,
		double Bottom_v, uint32 TopLeft, uint32 TopRight, uint32 BottomRight, uint32 BottomLeft);
	void ColorFill(int Image, const patella::platform::TEdgeRect *Rect, uint32 Color);
	// The main window as Win32's, for the devices' own windows (PresentWindow.h, issue #52; hWindow, a TWindowHandle
	// since issue #138).
	[[nodiscard]] HWND MainWindow() const { return static_cast<HWND>(hWindow); }
	int TextureWidth(int Surface);
	int TextureHeight(int Surface);
	patella::vulkan::TDeviceRequest Request(patella::platform::TWindowHandle Window);
	bool Masked(int Target);
	void Track(int Target, int Texture, const patella::vulkan::TQuad *Quads, size_t Count);
	void Fail();

	bool Ready;                                            // CreateSurfaces has finished (SurfaceRecreated)
	std::array<TEdgeRect, NUMBER_OF_SURFACES_VULKAN> Rect; // the surfaces' rectangles, as surfaceDX9.cpp's
	TEdgeRect WindowRect;                                  // the client area (from 0, 0 since issue #126: only its size)
	TEdgeRect SurfaceArea;                                 // SetSurfaceRect
	bool Initialized;                                      // between InitSurfaces and ExitSurfaces (surfaceDX9.cpp: DirectDraw exists)
	std::unique_ptr<patella::vulkan::TRenderer> Renderer;  // surfaceDX9.cpp's Direct3D and Device
	std::array<int, NUMBER_OF_SURFACES_VULKAN> Image;      // the renderer's images (SCREEN_SURFACE: the canvas)
	int ScreenTexture;                                     // the 106x106 texture of SCREEN_SURFACE
	int WhiteTexture;                                      // Direct3D 9's stage without a texture: opaque white
	int DeviceTarget;                                      // the image the device draws quads into
	int DeviceTexture;                                     // the image the device samples (-1 none)
	int CurrentTarget;                                     // surfaceDX9.cpp's CurrentTarget: ColorFill's image
	patella::vulkan::TFilter Filter;                       // the sampler state (D3DSAMP_MAGFILTER/MINFILTER)
	std::vector<patella::vulkan::TQuad> Shade;             // ShadeVertices
	bool DisplayModeSet;                                   // fullscreen changed the display mode
	TVulkanDeviceObserver DeviceObserver;
	TVulkanFrameObserver FrameObserver;
	TVulkanPixelsObserver PixelsObserver;
	TVulkanMaskObserver MaskObserver;
	patella::vulkan::TFilterMasks Masks; // the device-filtered pixels, while an observer watches
};

// Surface.cpp's engine selection.
bool IsVulkanInstalled();
TSurface *CreateSurfaceVulkan(patella::platform::TSurfaceShared &Shared);
