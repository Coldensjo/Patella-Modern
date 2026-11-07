// SurfaceVulkan.cpp: the Vulkan engine's surface on the MFC window (SurfaceVulkan.h; notes/vulkan.md).
//
// Every function follows its counterpart in surfaceDX9.cpp (the comments name it), with the Direct3D 9
// calls replaced: ColorFill by TRenderer::Fill, the dynamic vertex buffer by TRenderer::Draw, LockRect by
// TRenderer::Upload, ApplyColorKey's read-back by TRenderer::ReplaceColor, Present by TRenderer::Present.
// The state surfaceDX9.cpp keeps in file statics (the current target and texture numbers, the colour
// keys, the queued vertices, the frame counter) is kept the same way, so it outlives the surface
// object as there. Not new code paths for the error reports: no PATELLA_ASSERT (an index out of range is
// ignored), and the Vulkan back end's failures become PROGRAM_ERROR 12, as Direct3D's device failures.

#include "common.h"
#include "vulkan/SurfaceVulkan.h"
#include "vulkan/DeviceSettings.h"
#include "Preferences.h"
#include "Surface.h"
#include "graphics.h"
#include "platform/Platform.h"
#include "vulkan/DeviceChoice.h"
#include "vulkan/PresentWindow.h"
#include "vulkan/Quads.h"
#include "vulkan/Renderer.h"
#include <array>

#include <volk.h>

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// GUI.cpp (GUI.h is not warning-free at the full level; the engine needs only this).
void DrawSkin(int Surface, int x, int y, int width, int height, int skin, int srcx, int srcy);

using patella::vulkan::TDeviceKind;
using patella::vulkan::TDeviceRequest;
using patella::vulkan::TFilter;
using patella::vulkan::TQuad;
using patella::vulkan::TRenderer;
using patella::vulkan::TVulkanError;

namespace {

// surfaceDX9.cpp's file data.
int CurrentTargetNumber = -1;
std::array<uint32, NUMBER_OF_SURFACES_VULKAN> SurfaceKeys; // surfaceDX9.cpp ColorKey; 0 when the surface has none
std::vector<TQuad> Vertices;                               // the queued quads (6 vertices each)
std::array<int, NUMBER_OF_SURFACES_VULKAN> SurfaceCreated;
int CurrentTexture;
uint32 FrameCount;
double LastFPSTime;

// The windows the devices present in (PresentWindow.h; issue #52): one per device, a child of the main
// window. They outlive the surface object (a device switch deletes it), so the window of the device
// before a switch keeps its last frame until the next device has shown one.
patella::vulkan::TPresentWindows PresentWindows;

const int VertexLimit = 9600; // surfaceDX9.cpp's vertex buffer

uint32 Xrgb(int R, int G, int B)
{
	return 0xff000000u | (uint32)(R & 0xff) << 16 | (uint32)(G & 0xff) << 8 | (uint32)(B & 0xff);
}

} // namespace

// --- the engine choice (Surface.cpp) ---

bool IsVulkanInstalled()
{
	return patella::vulkan::LoaderAvailable();
}

TSurface *CreateSurfaceVulkan(patella::platform::TSurfaceShared &Shared)
{
	return new TSurfaceVulkan(Shared);
}

// The name of the device the engine runs on (DeviceSettings.h; the advanced options show it).
const char *GetDeviceName()
{
	const TSurfaceVulkan *Engine = static_cast<TSurfaceVulkan *>(GetSurface());
	return Engine != nullptr ? Engine->DeviceName() : "";
}

const char *TSurfaceVulkan::DeviceName() const
{
	return Renderer != nullptr && Renderer->Created() ? Renderer->Device().Name.c_str() : "";
}

// The device request: the graphics device choice (DeviceSettings.h: the advanced options' buttons,
// Patella.ini, PATELLA_VULKAN; issue #31), the CPU driver next to the executable, the window the device
// presents in (Window; nullptr: headless).
TDeviceRequest TSurfaceVulkan::Request(HWND Window)
{
	TDeviceRequest Result;
	const patella::vulkan::TDeviceChoice Choice = GetDeviceChoice();
	Result.Kind = Choice.Kind;
	Result.GpuIndex = Choice.Kind == TDeviceKind::Gpu ? Choice.GpuIndex : -1;
	Result.CpuDriverManifest = GetCpuDriverManifest();
	std::array<char, 8> Validation = {};
	size_t ValidationLength = 0;
	Result.Validation = getenv_s(&ValidationLength, Validation.data(), Validation.size(), "PATELLA_VULKAN_VALIDATION") == 0
		&& ValidationLength > 0 && Validation[0] == '1';
	if (Window != nullptr) {
		Result.SurfaceExtension = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
		Result.CreateSurface = [Window](VkInstance Instance) {
			PFN_vkCreateWin32SurfaceKHR Create = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(Instance, "vkCreateWin32SurfaceKHR"));
			VkWin32SurfaceCreateInfoKHR Info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
			Info.hinstance = GetModuleHandleA(nullptr);
			Info.hwnd = Window;
			VkSurfaceKHR Surface = VK_NULL_HANDLE;
			if (Create == nullptr || Create(Instance, &Info, nullptr, &Surface) != VK_SUCCESS)
				throw TVulkanError("vkCreateWin32SurfaceKHR failed");
			return Surface;
		};
	}
	return Result;
}

void TSurfaceVulkan::Fail()
{
	throw TProgramError{PROGRAM_ERROR::SURFACE};
}

// surfaceDX9.cpp TestEngine: a device for the window, released again. Its surface is on a hidden child
// window of its own (PresentWindow.h, issue #52), so the test leaves nothing bound to the main window or
// to a window that shows frames.
bool TSurfaceVulkan::TestEngine()
{
	TSurface::TestEngine();
	if (!IsVulkanInstalled())
		return false;
	try {
		const patella::vulkan::TTestWindow Window(MainWindow());
		if (hWindow != nullptr && Window.Handle() == nullptr)
			return false;
		TRenderer Test;
		Test.Create(Request(Window.Handle()));
	} catch (const TVulkanError &) {
		return false;
	}
	return true;
}

// --- surfaceDX9.cpp's helpers ---

// AddShadeQuad: the texture coordinates are relative to TEMP_SURFACE.
void TSurfaceVulkan::AddShadeQuad(int Left, int Top, int Right, int Bottom, uint32 TopLeft, uint32 TopRight,
	uint32 BottomRight, uint32 BottomLeft)
{
	const double Left_u = (Left + 0.5) / Rect[TEMP_SURFACE].right;
	const double Right_u = (Right + 0.5) / Rect[TEMP_SURFACE].right;
	const double Top_v = (Top + 0.5) / Rect[TEMP_SURFACE].bottom;
	const double Bottom_v = (Bottom + 0.5) / Rect[TEMP_SURFACE].bottom;
	Shade.push_back({Left, Top, Right, Bottom, (float)Left_u, (float)Right_u, (float)Top_v, (float)Bottom_v, TopLeft,
		TopRight, BottomRight, BottomLeft});
}

// --- the device-filtered pixels (only while a frame observer watches) ---
//
// compare_rendering.py compares exactly, except the pixels whose values come from the device's filter
// or interpolation (the user's decision of November 3, 2026: up to 2 per channel there). TFilterMasks
// (FilterMask.h) follows the engine's fills, uploads and quads to know them.

// True when the masks are kept; makes Target's mask exist with the image's size.
bool TSurfaceVulkan::Masked(int Target)
{
	if (FrameObserver == nullptr || Renderer == nullptr || Target < 0)
		return false;
	Masks.Ensure(Target, Renderer->ImageWidth(Target), Renderer->ImageHeight(Target));
	return true;
}

void TSurfaceVulkan::Track(int Target, int Texture, const TQuad *Quads, size_t Count)
{
	if (Masked(Target) && Masked(Texture))
		Masks.Draw(Target, Texture, Filter, Quads, Count);
}

// FlushVertices: the queued quads into the device's target with its texture and sampler.
void TSurfaceVulkan::FlushVertices()
{
	if (!Vertices.empty()) {
		if (Renderer != nullptr && DeviceTarget >= 0) {
			Track(DeviceTarget, DeviceTexture >= 0 ? DeviceTexture : WhiteTexture, Vertices.data(), Vertices.size());
			Renderer->Draw(DeviceTarget, DeviceTexture >= 0 ? DeviceTexture : WhiteTexture, Filter, Vertices.data(),
				Vertices.size());
		}
		Vertices.clear();
	}
}

// SetTexture: SCREEN_SURFACE's texture is ScreenTexture.
void TSurfaceVulkan::SetTexture(int Surface)
{
	if (CurrentTexture != Surface) {
		FlushVertices();
		DeviceTexture = Surface == SCREEN_SURFACE ? ScreenTexture : Image[Surface];
		CurrentTexture = Surface;
	}
}

void TSurfaceVulkan::SetRenderTarget(int Surface)
{
	if (CurrentTargetNumber != Surface) {
		FlushVertices();
		DeviceTarget = Image[Surface];
		CurrentTargetNumber = Surface;
	}
	CurrentTarget = Image[Surface];
}

int TSurfaceVulkan::GetSurfaceMemory(int Surface)
{
	return SurfaceCreated[Surface];
}

// The device goes before its window is retired (PresentWindow.h).
TSurfaceVulkan::~TSurfaceVulkan()
{
	Shared.NumberOfSurfaces = 0;
	Renderer.reset();
	PresentWindows.Retire();
}

// The same code in all engines.
bool TSurfaceVulkan::ClipRect(TRect Clip, patella::platform::TEdgeRect *Area, patella::platform::TEdgeRect *Source)
{
	const int ClipRight = Clip.x + Clip.width;
	const int ClipBottom = Clip.y + Clip.height;
	if (Area->left < ClipRight && Area->top < ClipBottom && Area->right > Clip.x && Area->bottom > Clip.y) {
		if (Area->left < Clip.x) {
			Source->left += Clip.x - Area->left;
			Area->left = Clip.x;
		}
		if (Area->top < Clip.y) {
			Source->top += Clip.y - Area->top;
			Area->top = Clip.y;
		}
		if (Area->right > ClipRight) {
			Source->right += Clip.width - Area->right + Clip.x;
			Area->right = ClipRight;
		}
		if (Area->bottom > ClipBottom) {
			Source->bottom += Clip.height - Area->bottom + Clip.y;
			Area->bottom = ClipBottom;
		}
		return true;
	}
	return false;
}

void TSurfaceVulkan::ResetFrameTimer()
{
	const double Now = patella::platform::Platform().Clock.Milliseconds();
	StartTime = Now;
	Framerate = 0xffff;
	NextFrame = Now;
	FPS = 0.0;
	FrameTime = 1000.0 / 0xffff;
}

void TSurfaceVulkan::WaitForNextFrame()
{
	const double Now = patella::platform::Platform().Clock.Milliseconds();
	FrameCount++;
	if (Now - LastFPSTime > 1000.0) {
		FPS = (FrameCount * 1000) / (Now - LastFPSTime);
		FrameCount = 0;
		LastFPSTime = Now;
	}
	NextFrame += FrameTime;
	if (NextFrame > Now)
		patella::platform::Platform().Clock.FrameWait(static_cast<std::uint32_t>((int)(NextFrame - Now)));
	else if (Now - NextFrame > 1.0)
		NextFrame = Now - 1.0;
}

// UpdateWindowRect. The canvas (SCREEN_SURFACE, surfaceDX9.cpp's back buffer of the screen's size)
// grows when the client area is larger than it (a window over several monitors).
void TSurfaceVulkan::UpdateWindowRect()
{
	if (ViewMode == WINDOWED_MODE) {
		GetClientRect(MainWindow(), &Rect[PRIMARY_SURFACE]);
		GetClientRect(MainWindow(), &Rect[SCREEN_SURFACE]);
		GetClientRect(MainWindow(), &WindowRect);
		ClientToScreen(MainWindow(), reinterpret_cast<POINT *>(&WindowRect.left));
		ClientToScreen(MainWindow(), reinterpret_cast<POINT *>(&WindowRect.right));
		if (Renderer != nullptr && Image[SCREEN_SURFACE] >= 0) {
			const int Canvas = Image[SCREEN_SURFACE];
			const int CanvasWidth = Renderer->ImageWidth(Canvas);
			const int CanvasHeight = Renderer->ImageHeight(Canvas);
			if (Rect[SCREEN_SURFACE].right > CanvasWidth || Rect[SCREEN_SURFACE].bottom > CanvasHeight) {
				FlushVertices();
				Renderer->ResizeImage(Canvas,
					Rect[SCREEN_SURFACE].right > CanvasWidth ? Rect[SCREEN_SURFACE].right : CanvasWidth,
					Rect[SCREEN_SURFACE].bottom > CanvasHeight ? Rect[SCREEN_SURFACE].bottom : CanvasHeight);
			}
		}
	}
}

// Direct3D 9's ColorFill: a rectangle that does not lie inside the surface is refused (D3DERR_INVALIDCALL).
void TSurfaceVulkan::ColorFill(int Target, const patella::platform::TEdgeRect *R, uint32 Color)
{
	if (Renderer == nullptr || Target < 0)
		return;
	if (R->left < 0 || R->top < 0 || R->left >= R->right || R->top >= R->bottom
		|| R->right > Renderer->ImageWidth(Target) || R->bottom > Renderer->ImageHeight(Target))
		return;
	if (Masked(Target))
		Masks.Fill(Target, R->left, R->top, R->right, R->bottom);
	Renderer->Fill(Target, R->left, R->top, R->right, R->bottom, Color);
}

void TSurfaceVulkan::SetPixel(int Surface, int x, int y, TRGBColor Color)
{
	const patella::platform::TEdgeRect R = {x, y, x + 1, y + 1};
	SetRenderTarget(Surface);
	FlushVertices();
	ColorFill(CurrentTarget, &R, Xrgb(Color.R, Color.G, Color.B));
}

void TSurfaceVulkan::FillRectangle(int Surface, TRect Area, TRGBColor Color)
{
	const patella::platform::TEdgeRect R = {Area.x, Area.y, Area.x + Area.width, Area.y + Area.height};
	if (R.top != R.bottom && R.left != R.right) {
		SetRenderTarget(Surface);
		FlushVertices();
		ColorFill(CurrentTarget, &R, Xrgb(Color.R, Color.G, Color.B));
		Shared.BlitCount++;
		Shared.BlitPixels += Area.height * Area.width;
	}
}

// FillRectangleClipped: the pixel count uses the size before clipping.
void TSurfaceVulkan::FillRectangleClipped(int Surface, TRect Area, TRGBColor Color, TRect Clip)
{
	patella::platform::TEdgeRect R = {Area.x, Area.y, Area.x + Area.width, Area.y + Area.height};
	if (R.top != R.bottom && R.left != R.right) {
		const int Pixels = Area.height * Area.width;
		patella::platform::TEdgeRect Unused = R;
		if (ClipRect(Clip, &R, &Unused)) {
			SetRenderTarget(Surface);
			FlushVertices();
			ColorFill(CurrentTarget, &R, Xrgb(Color.R, Color.G, Color.B));
			Shared.BlitCount++;
			Shared.BlitPixels += Pixels;
		}
	}
}

void TSurfaceVulkan::DrawHorizontalLine(int Surface, int x, int y, int Length, TRGBColor Color)
{
	const patella::platform::TEdgeRect R = {x, y, x + Length, y + 1};
	if (R.top != R.bottom && R.left != R.right) {
		SetRenderTarget(Surface);
		FlushVertices();
		ColorFill(CurrentTarget, &R, Xrgb(Color.R, Color.G, Color.B));
		Shared.BlitCount++;
		Shared.BlitPixels += Length;
	}
}

void TSurfaceVulkan::DrawVerticalLine(int Surface, int x, int y, int Length, TRGBColor Color)
{
	const patella::platform::TEdgeRect R = {x, y, x + 1, y + Length};
	if (R.top != R.bottom && R.left != R.right) {
		SetRenderTarget(Surface);
		FlushVertices();
		ColorFill(CurrentTarget, &R, Xrgb(Color.R, Color.G, Color.B));
		Shared.BlitCount++;
		Shared.BlitPixels += Length;
	}
}

// WriteImage: RGB data into a cache surface's texture; the pixels of the surface's colour key become
// transparent. Below SPRITE_SURFACE the data goes into ScreenTexture at (x, y) and the whole 106x106
// texture is blitted to AUTOMAP_SURFACE with point filtering, whatever ToSurface is.
void TSurfaceVulkan::WriteImage(int ToSurface, int x, int y, int PartWidth, int PartHeight, uint8 *Data)
{
	if (ToSurface < 0 || ToSurface >= NUMBER_OF_SURFACES_VULKAN || PartWidth <= 0 || PartHeight <= 0)
		return;
	std::vector<uint32_t> Pixels((size_t)PartWidth * (size_t)PartHeight);
	for (int i = 0; i < PartHeight; i++) {
		for (int j = 0; j < PartWidth; j++) {
			uint32 Color = Xrgb(Data[0], Data[1], Data[2]);
			if (Color == SurfaceKeys[ToSurface])
				Color = (uint32)Data[0] << 16 | (uint32)Data[1] << 8 | Data[2];
			Data += 3;
			Pixels[(size_t)i * (size_t)PartWidth + (size_t)j] = Color;
		}
	}
	if (ToSurface < SPRITE_SURFACE) {
		if (Renderer != nullptr) {
			if (Masked(ScreenTexture))
				Masks.Upload(ScreenTexture, x, y, PartWidth, PartHeight, Pixels.data());
			Renderer->Upload(ScreenTexture, x, y, PartWidth, PartHeight, Pixels.data(), (size_t)PartWidth);
		}
		Filter = TFilter::Point;
		BlitRect(AUTOMAP_SURFACE, 0, 0, SCREEN_SURFACE, TRect(0, 0, 106, 106));
		FlushVertices();
		Filter = TFilter::Linear;
		return;
	}
	if (Renderer != nullptr && Image[ToSurface] >= 0) {
		if (Masked(Image[ToSurface]))
			Masks.Upload(Image[ToSurface], x, y, PartWidth, PartHeight, Pixels.data());
		Renderer->Upload(Image[ToSurface], x, y, PartWidth, PartHeight, Pixels.data(), (size_t)PartWidth);
	}
}

// ApplyColorKey: the pixels of ColorKey in the top left 64x64 of Surface become transparent white
// (0x00ffffff), written back to STRETCH_SURFACE from Rect[Surface]'s corner.
void TSurfaceVulkan::ApplyColorKey(int Surface, TRGBColor Key)
{
	if (Renderer == nullptr || Image[Surface] < 0)
		return;
	Renderer->ReplaceColor(Image[Surface], 0, 0, 64, 64, Xrgb(Key.R, Key.G, Key.B), 0x00ffffffu);
}

// FlushShade: copies TEMP_SURFACE to PRIMARY_SURFACE and draws the shaded rectangles into TEMP_SURFACE.
void TSurfaceVulkan::FlushShade()
{
	StretchBlitRect(PRIMARY_SURFACE, TRect(0, 0, Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom), TEMP_SURFACE,
		TRect(0, 0, Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom));
	SetTexture(PRIMARY_SURFACE);
	SetRenderTarget(TEMP_SURFACE);
	if (Renderer != nullptr && DeviceTarget >= 0) {
		Track(DeviceTarget, DeviceTexture >= 0 ? DeviceTexture : WhiteTexture, Shade.data(), Shade.size());
		Renderer->Draw(DeviceTarget, DeviceTexture >= 0 ? DeviceTexture : WhiteTexture, Filter, Shade.data(),
			Shade.size());
	}
	Shade.clear();
}

// ShadeRectangles: MapWindow.cpp's light, only on TEMP_SURFACE. The corner colours are top left, top
// right, bottom left, bottom right.
void TSurfaceVulkan::ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors)
{
	if (Surface == TEMP_SURFACE && Renderer != nullptr) {
		Shade.clear();
		for (int i = 0; i < Count; i++) {
			const TRect *R = &Rects[i];
			const TRGBColor *C = &Colors[i * 4];
			AddShadeQuad(R->x, R->y, R->x + R->width, R->y + R->height, Xrgb(C[0].R, C[0].G, C[0].B),
				Xrgb(C[1].R, C[1].G, C[1].B), Xrgb(C[3].R, C[3].G, C[3].B), Xrgb(C[2].R, C[2].G, C[2].B));
		}
		if (!Shade.empty())
			FlushShade();
	}
}

// FinishFrame. A frame observer (the test build's frame dump) sees the frame before it is shown.
void TSurfaceVulkan::FinishFrame()
{
	if (ViewMode != 0) {
		WaitForNextFrame();
		Shared.ScreenWidth = Rect[SCREEN_SURFACE].right;
		Shared.ScreenHeight = Rect[SCREEN_SURFACE].bottom;
		Shared.LastBlitPixels = Shared.BlitPixels;
		Shared.LastBlitCount = Shared.BlitCount;
		Shared.Data71D0D8 = 0;
		Shared.BlitCount = 0;
		Shared.BlitPixels = 0;
		FlushVertices();
		if (Renderer != nullptr && Image[SCREEN_SURFACE] >= 0) {
			const int x = Rect[SCREEN_SURFACE].left;
			const int y = Rect[SCREEN_SURFACE].top;
			const int ShownWidth = Rect[SCREEN_SURFACE].right - x;
			const int ShownHeight = Rect[SCREEN_SURFACE].bottom - y;
			try {
				if (FrameObserver != nullptr && FrameObserver(ShownWidth, ShownHeight) && PixelsObserver != nullptr) {
					if (MaskObserver != nullptr && Masked(Image[SCREEN_SURFACE])) {
						// The picture's device-filtered pixels: 1, the others 0.
						std::vector<uint8> Shown((size_t)ShownWidth * (size_t)ShownHeight);
						for (int i = 0; i < ShownHeight; i++)
							for (int j = 0; j < ShownWidth; j++)
								Shown[(size_t)i * (size_t)ShownWidth + (size_t)j] = (uint8)(Masks.At(Image[SCREEN_SURFACE], x + j, y + i) & patella::vulkan::TFilterMasks::Filtered);
						MaskObserver(Shown.data(), ShownWidth, ShownHeight);
					}
					std::vector<uint32_t> Pixels;
					Renderer->ReadBack(Image[SCREEN_SURFACE], x, y, ShownWidth, ShownHeight, Pixels);
					PixelsObserver(Pixels.data(), ShownWidth * 4, ShownWidth, ShownHeight);
				}
				// The device's window covers the main window's client area, which the surface had before
				// issue #52; once it shows a frame, the window of the device before it goes.
				PresentWindows.Fit();
				PATELLA_TEST_PRESENT_FAILURE(throw TVulkanError("vkQueuePresentKHR failed (VkResult -5)"))
				if (Renderer->Present(Image[SCREEN_SURFACE], x, y, ShownWidth, ShownHeight,
						(uint32_t)(WindowRect.right - WindowRect.left), (uint32_t)(WindowRect.bottom - WindowRect.top)))
					PresentWindows.Presented();
			} catch (const TVulkanError &) {
				Fail();
			}
		}
		Shared.BlitCount++;
		Shared.BlitPixels += (WindowRect.bottom - WindowRect.top) * (WindowRect.right - WindowRect.left);
	}
}

bool TSurfaceVulkan::ReadScreen(std::vector<uint32_t> &Pixels, int &ShownWidth, int &ShownHeight)
{
	if (Renderer == nullptr || Image[SCREEN_SURFACE] < 0)
		return false;
	FlushVertices();
	const int x = Rect[SCREEN_SURFACE].left;
	const int y = Rect[SCREEN_SURFACE].top;
	ShownWidth = Rect[SCREEN_SURFACE].right - x;
	ShownHeight = Rect[SCREEN_SURFACE].bottom - y;
	Renderer->ReadBack(Image[SCREEN_SURFACE], x, y, ShownWidth, ShownHeight, Pixels);
	return true;
}

// ExitDisplay: releases the device and its surfaces (only after InitSurfaces, as surfaceDX9.cpp's only
// while DirectDraw exists); a fullscreen display mode goes back to the desktop's, as when Direct3D 9
// releases a fullscreen device.
void TSurfaceVulkan::ExitDisplay()
{
	if (!Initialized)
		return;
	Vertices.clear();
	Shade.clear();
	Masks.Clear();
	Renderer.reset();
	PresentWindows.Retire();
	for (int i = 0; i < NUMBER_OF_SURFACES_VULKAN; i++) {
		// surfaceDX9.cpp clears SurfaceCreated[i] where Surface9[i] exists, which SCREEN_SURFACE's never does.
		if (Image[i] >= 0) {
			Image[i] = -1;
			if (i != SCREEN_SURFACE)
				SurfaceCreated[i] = 0;
		}
	}
	ScreenTexture = -1;
	WhiteTexture = -1;
	DeviceTarget = -1;
	DeviceTexture = -1;
	CurrentTarget = -1;
	CacheSurfaces = 0;
	if (DisplayModeSet) {
		ChangeDisplaySettingsA(nullptr, 0);
		DisplayModeSet = false;
	}
	ViewMode = 0;
}

void TSurfaceVulkan::ResetDisplay(int VMode, TFullscreenMode *GMode)
{
	try {
		InitDisplay(VMode, GMode);
		CreateSurfaces();
	} catch (const TProgramError &Error) {
		try {
			ExitDisplay();
		} catch (...) {
		}
		throw Error;
	}
}

// RestoreSurfaces: Vulkan has no lost device to reset (surfaceDX9.cpp's TestCooperativeLevel).
void TSurfaceVulkan::RestoreSurfaces(int, TFullscreenMode *)
{
	Unavailable = false;
}

// Init: switches to VMode (with the resolution Resolution in fullscreen mode) only when something
// changes, or always with Force.
bool TSurfaceVulkan::Init(patella::platform::TWindowHandle *Window, int VMode, TFullscreenMode *Resolution, bool Force)
{
	Ready = false;
	if (Force) {
		ExitDisplay();
		hWindow = *Window;
		ResetDisplay(VMode, Resolution);
	} else if (VMode == WINDOWED_MODE) {
		switch (ViewMode) {
		case FULLSCREEN_MODE:
			ExitDisplay();
			[[fallthrough]];
		case 0:
			hWindow = *Window;
			ResetDisplay(WINDOWED_MODE, nullptr);
			break;
		case WINDOWED_MODE:
			if (hWindow != *Window)
				hWindow = *Window;
			UpdateWindowRect();
			break;
		}
	} else if (VMode == FULLSCREEN_MODE) {
		if (ViewMode != FULLSCREEN_MODE || Resolution->Width != Width || Resolution->Height != Height
			|| Resolution->Depth != Depth) {
			ExitDisplay();
			hWindow = *Window;
			ResetDisplay(FULLSCREEN_MODE, Resolution);
		}
	}
	RestoreSurfaces(VMode, Resolution);
	return true;
}

// The display modes as surfaceDX9.cpp's DirectDraw 4 EnumDisplayModes(0) and its callback (SurfaceDX.obj's
// copy 0x4ec810) gave them (issue #20): the display's modes in the order EnumDisplaySettings lists them,
// each size and depth once (without DDEDM_REFRESHRATES DirectDraw folds the refresh rates), at least
// 640x480 and 16 bits. Frequency is 0: DirectDraw then reports none, and the callback tested the refresh
// rate's flag in the pixel format's flags, where it is never set. On the machine of issue #20 both lists
// are the same 24 modes in the same order (notes/vulkan.md). The test build can fix the list
// (issue #48). Since issue #119 the modes are the platform's IWindow::DisplayMode (platform/Window.h; Win32's:
// EnumDisplaySettingsA, TWin32Window).
int TSurfaceVulkan::GetFullscreenModes(TFullscreenMode **Modes)
{
	*Modes = nullptr;
	if (!Initialized)
		return 0;
	std::vector<TFullscreenMode> Found;
	patella::platform::TDisplayMode Mode;
	for (unsigned int Index = 0; patella::platform::Platform().Window.DisplayMode(Index, Mode); Index++) {
		TFullscreenMode Listed;
		Listed.Width = Mode.Width;
		Listed.Height = Mode.Height;
		Listed.Depth = Mode.Depth;
		Listed.Frequency = 0;
		if (Listed.Width < 640 || Listed.Height < 480 || Listed.Depth < 16)
			continue;
		bool Known = false;
		for (const TFullscreenMode &Other : Found)
			Known = Known || (Other.Width == Listed.Width && Other.Height == Listed.Height && Other.Depth == Listed.Depth);
		if (!Known)
			Found.push_back(Listed);
	}
	NumberOfModes = (int)Found.size();
	if (Found.empty())
		return 0;
	*Modes = new TFullscreenMode[Found.size()];
	for (size_t i = 0; i < Found.size(); i++)
		(*Modes)[i] = Found[i];
	return NumberOfModes;
}

// SetSurfaceRect: SurfaceArea.left is set only for surface 0, its other members always.
void TSurfaceVulkan::SetSurfaceRect(int Surface, int x, int y, int PartWidth, int PartHeight)
{
	if (Surface != 0) {
		Rect[Surface].left = x;
		Rect[Surface].top = y;
		Rect[Surface].right = x + PartWidth;
		Rect[Surface].bottom = y + PartHeight;
	} else
		SurfaceArea.left = x;
	SurfaceArea.top = y;
	SurfaceArea.right = x + PartWidth;
	SurfaceArea.bottom = y + PartHeight;
}

bool TSurfaceVulkan::SurfaceRecreated(int)
{
	return Ready;
}

bool TSurfaceVulkan::CanShadeRectangles()
{
	return Renderer != nullptr;
}

void TSurfaceVulkan::ExitSurfaces()
{
	ExitDisplay();
	Initialized = false;
}

TSurfaceVulkan::TSurfaceVulkan(patella::platform::TSurfaceShared &Shared) : TSurface(Shared)
{
	Ready = false;
	memset(Rect.data(), 0, sizeof(Rect));
	memset(&WindowRect, 0, sizeof(WindowRect));
	memset(&SurfaceArea, 0, sizeof(SurfaceArea));
	Initialized = false;
	for (int i = 0; i < NUMBER_OF_SURFACES_VULKAN; i++)
		Image[i] = -1;
	ScreenTexture = -1;
	WhiteTexture = -1;
	DeviceTarget = -1;
	DeviceTexture = -1;
	CurrentTarget = -1;
	Filter = TFilter::Linear;
	DisplayModeSet = false;
	DeviceObserver = nullptr;
	FrameObserver = nullptr;
	PixelsObserver = nullptr;
	MaskObserver = nullptr;
}

// AddQuad: six vertices of a textured quad; a full buffer is drawn.
void TSurfaceVulkan::AddQuad(int Left, int Top, int Right, int Bottom, double Left_u, double Right_u, double Top_v,
	double Bottom_v, uint32 TopLeft, uint32 TopRight, uint32 BottomRight, uint32 BottomLeft)
{
	Vertices.push_back({Left, Top, Right, Bottom, (float)Left_u, (float)Right_u, (float)Top_v, (float)Bottom_v,
		TopLeft, TopRight, BottomRight, BottomLeft});
	if ((int)Vertices.size() * 6 == VertexLimit)
		FlushVertices();
}

// StretchBlitUnclipped: an unscaled one is a Blit; the colours are not used for a scaled one.
void TSurfaceVulkan::StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect,
	TRGBColor ColorKey, TRGBColor)
{
	if (ToRect.width == FromRect.width && ToRect.height == FromRect.height) {
		Blit(ToSurface, ToRect.x, ToRect.y, FromSurface, FromRect, ColorKey);
		return;
	}
	const int Right = ToRect.x + ToRect.width;
	const int Bottom = ToRect.y + ToRect.height;
	const int FromRight = FromRect.x + FromRect.width;
	const int FromBottom = FromRect.y + FromRect.height;
	SetRenderTarget(ToSurface);
	SetTexture(FromSurface);
	AddQuad(ToRect.x, ToRect.y, Right, Bottom, (FromRect.x + 0.5) / Rect[FromSurface].right,
		(FromRight + 0.5) / Rect[FromSurface].right, (FromRect.y + 0.5) / Rect[FromSurface].bottom,
		(FromBottom + 0.5) / Rect[FromSurface].bottom, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
}

void TSurfaceVulkan::StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source)
{
	SetRenderTarget(ToSurface);
	SetTexture(FromSurface);
	AddQuad(Destination.x, Destination.y, Destination.x + Destination.width, Destination.y + Destination.height,
		(Source.x + 0.5) / Rect[FromSurface].right, (Source.x + Source.width + 0.5) / Rect[FromSurface].right,
		(Source.y + 0.5) / Rect[FromSurface].bottom, (Source.y + Source.height + 0.5) / Rect[FromSurface].bottom,
		0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
	Shared.BlitCount++;
	Shared.BlitPixels += Destination.height * Destination.width;
}

// BlitRectClipped: ScreenTexture (SCREEN_SURFACE) is always mapped as a whole.
void TSurfaceVulkan::BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip)
{
	patella::platform::TEdgeRect To = {x, y, x + FromRect.width, y + FromRect.height};
	patella::platform::TEdgeRect From = {FromRect.x, FromRect.y, FromRect.x + FromRect.width, FromRect.y + FromRect.height};
	if (ClipRect(Clip, &To, &From)) {
		SetRenderTarget(ToSurface);
		SetTexture(FromSurface);
		double Left_u, Right_u, Top_v, Bottom_v;
		if (FromSurface == SCREEN_SURFACE) {
			Left_u = 0.0;
			Right_u = 1.0;
			Top_v = 0.0;
			Bottom_v = 1.0;
		} else {
			Left_u = (From.left + 0.5) / Rect[FromSurface].right;
			Right_u = (From.right + 0.5) / Rect[FromSurface].right;
			Top_v = (From.top + 0.5) / Rect[FromSurface].bottom;
			Bottom_v = (From.bottom + 0.5) / Rect[FromSurface].bottom;
		}
		AddQuad(To.left, To.top, To.right, To.bottom, Left_u, Right_u, Top_v, Bottom_v, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff);
		Shared.BlitCount++;
		Shared.BlitPixels += FromRect.height * FromRect.width;
	}
}

// InitSurfaces: the start state. surfaceDX9.cpp creates DirectDraw 4 here for the modes and the stretch
// capabilities; the modes come from EnumDisplaySettings now (GetFullscreenModes), and Vulkan stretches on
// every device, so CanStretch is set (surfaceDX9.cpp: DirectDraw's four stretch and shrink flags, which
// the machine of issue #20 reports too).
void TSurfaceVulkan::InitSurfaces()
{
	hWindow = nullptr;
	ViewMode = 0;
	Initialized = false;
	Renderer.reset();
	PresentWindows.Retire();
	Shared.CacheSurfaceWidth = 512;
	Shared.CacheSurfaceHeight = 512;
	CurrentTarget = -1;
	for (int i = 0; i < NUMBER_OF_SURFACES_VULKAN; i++) {
		Image[i] = -1;
		SurfaceCreated[i] = 0;
		SurfaceKeys[i] = 0;
	}
	SetRect(&Rect[TEMP_SURFACE], 0, 0, 640, 480);
	SetRect(&Rect[STRETCH_SURFACE], 0, 0, 64, 64);
	SetRect(&Rect[CACHE_SURFACE], 0, 0, 512, 384);
	SetRect(&Rect[AUTOMAP_SURFACE], 0, 0, 106, 106);
	for (int i = SPRITE_SURFACE; i < NUMBER_OF_SURFACES_VULKAN; i++) {
		Rect[i].left = 0;
		Rect[i].top = 0;
		Rect[i].right = Shared.CacheSurfaceWidth;
		Rect[i].bottom = Shared.CacheSurfaceHeight;
	}
	Initialized = true;
	CanStretch = true;
	SetDefaultNoStretching(CanStretch == false);
	ResetFrameTimer();
	SetFramerateLimit(GetConfineFramerate());
}

void TSurfaceVulkan::SetSourceColorKey(int ToSurface, TRGBColor Key)
{
	if (ToSurface < 0 || ToSurface >= NUMBER_OF_SURFACES_VULKAN)
		return;
	SurfaceKeys[ToSurface] = Xrgb(Key.R, Key.G, Key.B);
}

// WriteMaskedImage: a 32x32 sprite with a colour mask; see surfaceDX9.cpp.
void TSurfaceVulkan::WriteMaskedImage(int ToSurface, int x, int y, int PartWidth, int PartHeight, uint8 *Data,
	uint8 *Mask, TRGBColor *Colors)
{
	if (ToSurface < 0 || ToSurface >= NUMBER_OF_SURFACES_VULKAN || PartWidth <= 0 || PartHeight <= 0)
		return;
	const int Size = PartWidth * PartHeight;
	std::vector<uint32_t> Pixels((size_t)Size);
	for (int i = 0; i < Size; i++) {
		const uint8 *From = &Data[i * 3];
		int Index = -1; // the colour of the tint, or -1: the pixel is copied
		uint8 R = 0, G = 0, B = 0, A = 0;
		if (Mask != nullptr) {
			const uint8 *M = &Mask[i * 3];
			if (M[0] == 0xff) {
				if (M[1] == 0xff) {
					if (M[2] == 0)
						Index = 0;
				} else if (M[1] == 0 && M[2] == 0) {
					Index = 1;
				}
			} else if (M[0] == 0) {
				if (M[1] == 0xff) {
					if (M[2] == 0)
						Index = 2;
				} else if (M[1] == 0 && M[2] == 0xff) {
					Index = 3;
				}
			}
		} else if (From[0] != Colors[1].R || From[1] != Colors[1].G || From[2] != Colors[1].B) {
			Index = 0;
		}
		if (Index >= 0) {
			const int Grey = (From[1] * 59 + From[0] * 30 + From[2] * 11) / 100;
			const TRGBColor *C = &Colors[Index];
			R = (uint8)(C->R * Grey / 256);
			G = (uint8)(C->G * Grey / 256);
			B = (uint8)(C->B * Grey / 256);
			A = 0xff;
		} else {
			R = From[0];
			G = From[1];
			B = From[2];
			A = Xrgb(From[0], From[1], From[2]) != SurfaceKeys[ToSurface] ? 0xff : 0;
		}
		Pixels[(size_t)i] = (uint32)A << 24 | (uint32)R << 16 | (uint32)G << 8 | B;
	}
	if (Renderer != nullptr && Image[ToSurface] >= 0) {
		if (Masked(Image[ToSurface]))
			Masks.Upload(Image[ToSurface], x, y, PartWidth, PartHeight, Pixels.data());
		Renderer->Upload(Image[ToSurface], x, y, PartWidth, PartHeight, Pixels.data(), (size_t)PartWidth);
	}
}

// StretchBlit: a scaled blit goes through TEMP_SURFACE (the source with a one-pixel border of
// Highlight, or skin 185 while Flag26 is set) and STRETCH_SURFACE; Highlight is the colour key of the
// result.
void TSurfaceVulkan::StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
	TRGBColor Highlight, TRect Clip)
{
	if (ToRect.width == FromRect.width && ToRect.height == FromRect.height) {
		BlitClipped(ToSurface, ToRect.x, ToRect.y, FromSurface, FromRect, ColorKey, Clip);
		return;
	}
	if (Flag26)
		DrawSkin(TEMP_SURFACE, 0, 0, FromRect.width + 1, FromRect.height + 1, 185, 0, 0);
	else
		FillRectangle(TEMP_SURFACE, TRect(0, 0, FromRect.width + 1, FromRect.height + 1), Highlight);
	Blit(TEMP_SURFACE, 0, 0, FromSurface, FromRect, ColorKey);
	StretchBlitRect(STRETCH_SURFACE, TRect(0, 0, ToRect.width, ToRect.height), TEMP_SURFACE,
		TRect(0, 0, FromRect.width, FromRect.height));
	if (!Flag26) {
		FlushVertices();
		ApplyColorKey(STRETCH_SURFACE, Highlight);
	}
	BlitClipped(ToSurface, ToRect.x, ToRect.y, STRETCH_SURFACE, TRect(0, 0, ToRect.width, ToRect.height), Highlight,
		Clip);
}

// Blit: clipped to the target surface.
void TSurfaceVulkan::Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey)
{
	TRect Clip;
	GetSurfaceRect(ToSurface, &Clip);
	BlitClipped(ToSurface, x, y, FromSurface, FromRect, ColorKey, Clip);
}

void TSurfaceVulkan::BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect)
{
	TRect Clip;
	GetSurfaceRect(ToSurface, &Clip);
	BlitRectClipped(ToSurface, x, y, FromSurface, FromRect, Clip);
}

// InitDisplay: fullscreen mode takes GMode's size if GetFullscreenModes lists it, otherwise the last mode
// listed (written back into GMode, without its frequency).
void TSurfaceVulkan::InitDisplay(int VMode, TFullscreenMode *GMode)
{
	if (VMode == WINDOWED_MODE) {
		GetClientRect(MainWindow(), &Rect[PRIMARY_SURFACE]);
		GetClientRect(MainWindow(), &Rect[SCREEN_SURFACE]);
		GetClientRect(MainWindow(), &WindowRect);
		ClientToScreen(MainWindow(), reinterpret_cast<POINT *>(&WindowRect.left));
		ClientToScreen(MainWindow(), reinterpret_cast<POINT *>(&WindowRect.right));
	} else {
		TFullscreenMode *Modes;
		const int Number = GetFullscreenModes(&Modes);
		int i;
		for (i = 0; i < Number; i++) {
			if (GMode->Width == Modes[i].Width && GMode->Height == Modes[i].Height)
				break;
		}
		if (i == Number && Number > 0) {
			GMode->Width = Modes[Number - 1].Width;
			GMode->Height = Modes[Number - 1].Height;
			GMode->Depth = Modes[Number - 1].Depth;
		}
		if (Modes != nullptr)
			delete[] Modes;
		Width = GMode->Width;
		Height = GMode->Height;
		Depth = GMode->Depth;
		SetRect(&Rect[PRIMARY_SURFACE], 0, 0, Width, Height);
		SetRect(&Rect[SCREEN_SURFACE], 0, 0, Width, Height);
		SetRect(&WindowRect, 0, 0, Width, Height);
	}
	ViewMode = VMode;
}

// CreateSurfaces: the device for ViewMode and every surface, in surfaceDX9.cpp's order (SurfaceCreated
// and NumberOfSurfaces count them the same way). PROGRAM_ERRORs: 8 below 16 bits per pixel on the
// desktop, 12 without a Vulkan device.
void TSurfaceVulkan::CreateSurfaces()
{
	Shared.NumberOfSurfaces = 0;
	// surfaceDX9.cpp: DirectDraw's GetDisplayMode, the desktop's depth (0 before InitSurfaces). Since issue #138 the
	// platform's IWindow::DesktopMode (platform/Window.h; Win32's: EnumDisplaySettingsA with ENUM_CURRENT_SETTINGS).
	patella::platform::TDisplayMode Desktop;
	if (!Initialized || !patella::platform::Platform().Window.DesktopMode(Desktop))
		Desktop.Depth = 0;
	if (Desktop.Depth < 16)
		throw TProgramError{PROGRAM_ERROR::COLOR_DEPTH};
	if (ViewMode == FULLSCREEN_MODE) {
		// Direct3D 9's fullscreen device sets the display mode; the window is already a popup over the
		// whole screen (MainFrm.cpp SetDisplayMode).
		DEVMODEA Mode;
		memset(&Mode, 0, sizeof(Mode));
		Mode.dmSize = sizeof(DEVMODEA);
		Mode.dmPelsWidth = Width;
		Mode.dmPelsHeight = Height;
		Mode.dmBitsPerPel = Depth;
		Mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		if (ChangeDisplaySettingsA(&Mode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
			Shared.InitStep = 1;
			throw TProgramError{PROGRAM_ERROR::SURFACE};
		}
		DisplayModeSet = true;
	}
	PATELLA_TEST_FRAME_OBSERVERS(FrameObserver, PixelsObserver, MaskObserver, DeviceObserver)
	try {
		// A window of its own for the new device (PresentWindow.h, issue #52).
		HWND Window = nullptr;
		if (hWindow != nullptr) {
			Window = PresentWindows.Open(MainWindow());
			if (Window == nullptr)
				throw TVulkanError("the device's window could not be created");
		}
		Renderer = std::make_unique<TRenderer>();
		Renderer->Create(Request(Window));
		int CanvasWidth, CanvasHeight;
		if (ViewMode == WINDOWED_MODE) {
			CanvasWidth = GetSystemMetrics(SM_CXSCREEN);
			CanvasHeight = GetSystemMetrics(SM_CYSCREEN);
			if (CanvasWidth < Rect[SCREEN_SURFACE].right)
				CanvasWidth = Rect[SCREEN_SURFACE].right;
			if (CanvasHeight < Rect[SCREEN_SURFACE].bottom)
				CanvasHeight = Rect[SCREEN_SURFACE].bottom;
		} else {
			CanvasWidth = Width;
			CanvasHeight = Height;
		}
		Image[SCREEN_SURFACE] = Renderer->CreateImage(CanvasWidth, CanvasHeight);
		WhiteTexture = Renderer->CreateImage(1, 1);
		if (Masked(WhiteTexture))
			Masks.Fill(WhiteTexture, 0, 0, 1, 1);
		Renderer->Fill(WhiteTexture, 0, 0, 1, 1, 0xffffffffu);

		Image[PRIMARY_SURFACE] = Renderer->CreateImage(Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom);
		SurfaceCreated[Shared.NumberOfSurfaces++] = 1;
		if (Masked(Image[PRIMARY_SURFACE]))
			Masks.Fill(Image[PRIMARY_SURFACE], 0, 0, Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom);
		Renderer->Fill(Image[PRIMARY_SURFACE], 0, 0, Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom, 0xffff0000u);

		ScreenTexture = Renderer->CreateImage(106, 106);
		SurfaceCreated[Shared.NumberOfSurfaces++] = 1;

		Image[TEMP_SURFACE] = Renderer->CreateImage(Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom);
		SurfaceCreated[Shared.NumberOfSurfaces++] = 1;
		Image[STRETCH_SURFACE] = Renderer->CreateImage(Rect[STRETCH_SURFACE].right, Rect[STRETCH_SURFACE].bottom);
		SurfaceCreated[Shared.NumberOfSurfaces++] = 1;
		Image[CACHE_SURFACE] = Renderer->CreateImage(Rect[CACHE_SURFACE].right, Rect[CACHE_SURFACE].bottom);
		SurfaceCreated[Shared.NumberOfSurfaces++] = 1;
		Image[AUTOMAP_SURFACE] = Renderer->CreateImage(Rect[AUTOMAP_SURFACE].right, Rect[AUTOMAP_SURFACE].bottom);
		SurfaceCreated[Shared.NumberOfSurfaces++] = 1;
		for (int i = 0; i < 16; i++) {
			const int Surface = SPRITE_SURFACE + i;
			Image[Surface] = Renderer->CreateImage(Rect[Surface].right, Rect[Surface].bottom);
			SurfaceCreated[Shared.NumberOfSurfaces++] = 1;
			CacheSurfaces++;
		}
	} catch (const TVulkanError &) {
		Renderer.reset();
		PresentWindows.Retire();
		Shared.InitStep = 1;
		throw TProgramError{PROGRAM_ERROR::SURFACE};
	}
	// A new device draws into the back buffer and has no texture; the numbers above stay as they were
	// (surfaceDX9.cpp's statics outlive the device).
	DeviceTarget = Image[SCREEN_SURFACE];
	DeviceTexture = -1;
	Filter = TFilter::Linear;
	if (DeviceObserver != nullptr)
		DeviceObserver(Renderer->Device().Name.c_str(), (int)Renderer->Device().Type, Renderer->Device().CpuDriver);
	Unavailable = false;
	Ready = true;
}

void TSurfaceVulkan::GetSurfaceRect(int Surface, TRect *Result)
{
	if (Surface < 0 || Surface >= NUMBER_OF_SURFACES_VULKAN) {
		*Result = TRect();
		return;
	}
	Result->x = Rect[Surface].left;
	Result->y = Rect[Surface].top;
	Result->width = Rect[Surface].right - Rect[Surface].left;
	Result->height = Rect[Surface].bottom - Rect[Surface].top;
}

// A frame thinner than twice the line is filled.
void TSurfaceVulkan::DrawFrameClipped(int Surface, TRect Area, int LineWidth, TRGBColor Color, TRect Clip)
{
	if (Area.width < 2 * LineWidth || Area.height < 2 * LineWidth) {
		FillRectangleClipped(Surface, Area, Color, Clip);
		return;
	}
	FillRectangleClipped(Surface, TRect(Area.x, Area.y, Area.width - LineWidth, LineWidth), Color, Clip);
	FillRectangleClipped(Surface,
		TRect(Area.x + LineWidth, Area.y + Area.height - LineWidth, Area.width - LineWidth, LineWidth), Color, Clip);
	FillRectangleClipped(Surface, TRect(Area.x, Area.y + LineWidth, LineWidth, Area.height - LineWidth), Color, Clip);
	FillRectangleClipped(Surface, TRect(Area.x + Area.width - LineWidth, Area.y, LineWidth, Area.height - LineWidth),
		Color, Clip);
}

void TSurfaceVulkan::DrawFrame(int Surface, TRect Area, int LineWidth, TRGBColor Color)
{
	if (Area.width < 2 * LineWidth || Area.height < 2 * LineWidth) {
		FillRectangle(Surface, Area, Color);
		return;
	}
	FillRectangle(Surface, TRect(Area.x, Area.y, Area.width - LineWidth, LineWidth), Color);
	FillRectangle(Surface, TRect(Area.x + LineWidth, Area.y + Area.height - LineWidth, Area.width - LineWidth, LineWidth),
		Color);
	FillRectangle(Surface, TRect(Area.x, Area.y + LineWidth, LineWidth, Area.height - LineWidth), Color);
	FillRectangle(Surface, TRect(Area.x + Area.width - LineWidth, Area.y, LineWidth, Area.height - LineWidth), Color);
}

// BlitClipped: the colour key is not used.
void TSurfaceVulkan::BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor,
	TRect Clip)
{
	BlitRectClipped(ToSurface, x, y, FromSurface, FromRect, Clip);
}

int TSurfaceVulkan::GetCacheSurfaces()
{
	return CacheSurfaces;
}

bool TSurfaceVulkan::IsUnavailable()
{
	return Unavailable;
}

// Limits from 10 to 200 frames per second; others mean no limit (0xffff).
void TSurfaceVulkan::SetFramerateLimit(int Limit)
{
	if (Limit < 10 || Limit >= 201)
		Framerate = 0xffff;
	else
		Framerate = Limit;
	FrameTime = 1000.0 / Framerate;
}

double TSurfaceVulkan::GetFPS()
{
	return FPS;
}
