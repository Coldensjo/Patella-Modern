// SurfaceDX.cpp: reconstructed from Tibia.exe 7.72 (0x4eae70-0x4efd0f).
//
// The DirectX 5 engine: TSurfaceDX draws on DirectDraw surfaces (pixels,
// fills, lines, images, blits with and without colour key, stretching
// through two work surfaces), shades the light rectangles with Direct3D
// when the hardware can, limits the frame rate, and creates, restores and
// releases the display and its surfaces. The file name is the binary's
// (".\SurfaceDX.cpp" in the assertions). Functions with line evidence are in
// line order; the others (they come first in the binary) in binary order.
//
// 0x4eae70 is d3dtypes.h's inline D3DTLVERTEX::D3DTLVERTEX() (D3D_OVERLOADS),
// the constructor ShadeRectangles' and ShadeRectangle's vertex arrays run.

// SurfaceDX.h's d3d.h must declare the DirectX 6/7 interfaces: real MFC's afxwin.h (common.h)
// includes d3d9types.h, which would set DIRECT3D_VERSION to 0x0900 first (issue #16).
#define DIRECT3D_VERSION 0x0700
#include "SurfaceDX.h"
#include "Preferences.h"
#include <mmsystem.h>
#include "Surface.h"
#include "common.h"
#include "graphics.h"
#include <cstddef>
#include <cstring>

// The image's assertions with a bug number append it to the expression text.
#define TASSERTB1(Line, Expr, V1, Bug) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, "", 0, #Expr " " Bug)

// The factory allocates 0x250 bytes.
#if PATELLA_ORIGINAL_LAYOUT
static_assert(sizeof(TSurfaceDX) == 0x250, "TSurfaceDX's size");
#endif

// Data, in .data and .bss order.
static const GUID *DeviceGUID = &IID_IDirect3DHALDevice; // 0x5c2570, the Direct3D device created
static int SurfaceMemory[NUMBER_OF_SURFACES];            // 0x71d0f8, per created surface: 1 video memory, 2 system memory

// 0x4eae80. SurfaceMemory is filled in the order of creation (by
// NumberOfSurfaces), but ExitDisplay clears it by surface index.
int TSurfaceDX::GetSurfaceMemory(int Index)
{
	return SurfaceMemory[Index];
}

// 0x4eae90. Releases nothing (ExitSurfaces does).
TSurfaceDX::~TSurfaceDX()
{
	NumberOfSurfaces = 0;
}

// 0x4eaeb0. The colour in DDSurface's pixel format; 0 for palette formats
// (less than 16 bits). Odd behaviour kept: a mask of 0 loops forever.
uint32 TSurfaceDX::GetPixelColor(IDirectDrawSurface4 *DDSurface, TRGBColor Color)
{
	DDPIXELFORMAT Format;
	memset(&Format, 0, sizeof(Format));
	Format.dwSize = sizeof(Format);
	DDSurface->GetPixelFormat(&Format);
	if (Format.dwRGBBitCount < 16)
		return 0;
	uint32 Mask = Format.dwRBitMask;
	int RedShift = 0;
	while ((Mask & 1) == 0) {
		Mask >>= 1;
		RedShift++;
	}
	int RedBits = 0;
	while ((Mask & 1) != 0) {
		Mask >>= 1;
		RedBits++;
	}
	Mask = Format.dwGBitMask;
	int GreenShift = 0;
	while ((Mask & 1) == 0) {
		Mask >>= 1;
		GreenShift++;
	}
	int GreenBits = 0;
	while ((Mask & 1) != 0) {
		Mask >>= 1;
		GreenBits++;
	}
	Mask = Format.dwBBitMask;
	int BlueShift = 0;
	while ((Mask & 1) == 0) {
		Mask >>= 1;
		BlueShift++;
	}
	int BlueBits = 0;
	while ((Mask & 1) != 0) {
		Mask >>= 1;
		BlueBits++;
	}
	return (Color.B >> (8 - BlueBits)) << BlueShift
		| (Color.G >> (8 - GreenBits)) << GreenShift
		| (Color.R >> (8 - RedBits)) << RedShift;
}

// 0x4eafb0
void TSurfaceDX::SetSourceColorKey(int Surface, TRGBColor ColorKey)
{
	DDCOLORKEY Key;
	Key.dwColorSpaceLowValue = GetPixelColor(this->Surface[Surface], ColorKey);
	Key.dwColorSpaceHighValue = Key.dwColorSpaceLowValue;
	this->Surface[Surface]->SetColorKey(DDCKEY_SRCBLT, &Key);
}

// 0x4eb000. 10 to 200 frames per second; anything else is no limit.
void TSurfaceDX::SetFramerateLimit(int Framerate)
{
	if (Framerate < 10 || Framerate > 200)
		this->Framerate = 0xffff;
	else
		this->Framerate = Framerate;
	FrameTime = 1000.0 / this->Framerate;
}

// 0x4eb040. Counts the frames of each second into FPS and sleeps until the
// next frame is due; a frame late by more than a millisecond moves the
// schedule (no catching up). The statics are this file's (surfaceDX9.cpp's
// copy has its own).
void TSurfaceDX::WaitForNextFrame()
{
	static uint32 Frames;     // 0x71d150
	static double LastSecond; // 0x71d158
	const double Now = PATELLA_TIME_GET_TIME();
	Frames++;
	if (Now - LastSecond > 1000.0) {
		FPS = (double)(Frames * 1000) / (Now - LastSecond);
		Frames = 0;
		LastSecond = Now;
	}
	NextFrame += FrameTime;
	if (NextFrame > Now)
		PATELLA_FRAME_SLEEP((DWORD)(NextFrame - Now));
	else if (Now - NextFrame > 1.0)
		NextFrame = Now - 1.0;
}

// 0x4eb100. In windowed mode: the client area as the size of the primary
// and screen surfaces and, in screen coordinates, as WindowRect.
void TSurfaceDX::UpdateWindowRect()
{
	if (ViewMode == WINDOWED_MODE) {
		GetClientRect(hWindow, &Rect[PRIMARY_SURFACE]);
		GetClientRect(hWindow, &Rect[SCREEN_SURFACE]);
		GetClientRect(hWindow, &WindowRect);
		ClientToScreen(hWindow, reinterpret_cast<POINT *>(&WindowRect.left));
		ClientToScreen(hWindow, reinterpret_cast<POINT *>(&WindowRect.right));
	}
}

// 0x4eb160. The surface is locked with the size of a DDSURFACEDESC (0x6c) in
// a zeroed DDSURFACEDESC2, as in every Lock of this file. Odd behaviour kept:
// a failed Lock (other than busy) returns at once.
void TSurfaceDX::SetPixel(int Surface, int x, int y, TRGBColor Color)
{
	DDSURFACEDESC2 Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.dwSize = sizeof(DDSURFACEDESC);
	HRESULT Result;
	while ((Result = this->Surface[Surface]->Lock(nullptr, &Desc,
				DDLOCK_WAIT | DDLOCK_NOSYSLOCK, nullptr))
		!= DD_OK) {
		if (Result != DDERR_SURFACEBUSY)
			return;
		Sleep(0);
	}
	DDPIXELFORMAT Format;
	memset(&Format, 0, sizeof(Format));
	Format.dwSize = sizeof(Format);
	this->Surface[Surface]->GetPixelFormat(&Format);
	if (Format.dwRGBBitCount < 16) {
		this->Surface[Surface]->Unlock(nullptr);
		return;
	}
	const int BytesPerPixel = Format.dwRGBBitCount >> 3;
	const uint32 Pixel = GetPixelColor(this->Surface[Surface], Color);
	uint8 Bytes[4];
	Bytes[3] = (uint8)Pixel;
	Bytes[2] = (uint8)(Pixel >> 8);
	Bytes[1] = (uint8)(Pixel >> 16);
	Bytes[0] = (uint8)(Pixel >> 24);
	// Written through below (*Destination++ = ...); clang-tidy 22 takes the pointee for const (issue #39).
	uint8 *Destination = static_cast<uint8 *>(Desc.lpSurface) + y * Desc.lPitch + x * BytesPerPixel; // NOLINT(misc-const-correctness)
	for (int LoopIndex = 3; LoopIndex >= 4 - BytesPerPixel; LoopIndex--)
		*Destination++ = Bytes[LoopIndex];
	this->Surface[Surface]->Unlock(nullptr);
}

// 0x4eb2f0. Every blit is retried while the surface is busy and counted in
// BlitCount and BlitPixels (with the unclipped size).
void TSurfaceDX::FillRectangle(int Surface, TRect Rect, TRGBColor Color)
{
	RECT Area = {Rect.x, Rect.y, Rect.x + Rect.width, Rect.y + Rect.height};
	DDBLTFX Effects;
	Effects.dwSize = sizeof(DDBLTFX);
	Effects.dwFillColor = GetPixelColor(this->Surface[Surface], Color);
	HRESULT Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
		&Effects);
	while (Result == DDERR_SURFACEBUSY) {
		Sleep(0);
		Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
			&Effects);
	}
	BlitCount++;
	BlitPixels += Rect.width * Rect.height;
}

// 0x4eb3f0. Source only receives ClipRect's corrections (it is not set).
void TSurfaceDX::FillRectangleClipped(int Surface, TRect Rect, TRGBColor Color, TRect Clip)
{
	RECT Area = {Rect.x, Rect.y, Rect.x + Rect.width, Rect.y + Rect.height};
	RECT Source;
	if (!ClipRect(Clip, &Area, &Source))
		return;
	DDBLTFX Effects;
	Effects.dwSize = sizeof(DDBLTFX);
	Effects.dwFillColor = GetPixelColor(this->Surface[Surface], Color);
	HRESULT Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
		&Effects);
	while (Result == DDERR_SURFACEBUSY) {
		Sleep(0);
		Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
			&Effects);
	}
	BlitCount++;
	BlitPixels += Rect.width * Rect.height;
}

// 0x4eb540
void TSurfaceDX::DrawHorizontalLine(int Surface, int x, int y, int Width, TRGBColor Color)
{
	RECT Area = {x, y, x + Width, y + 1};
	DDBLTFX Effects;
	Effects.dwSize = sizeof(DDBLTFX);
	Effects.dwFillColor = GetPixelColor(this->Surface[Surface], Color);
	HRESULT Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
		&Effects);
	while (Result == DDERR_SURFACEBUSY) {
		Sleep(0);
		Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
			&Effects);
	}
	BlitCount++;
	BlitPixels += Width;
}

// 0x4eb630
void TSurfaceDX::DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor Color)
{
	RECT Area = {x, y, x + 1, y + Height};
	DDBLTFX Effects;
	Effects.dwSize = sizeof(DDBLTFX);
	Effects.dwFillColor = GetPixelColor(this->Surface[Surface], Color);
	HRESULT Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
		&Effects);
	while (Result == DDERR_SURFACEBUSY) {
		Sleep(0);
		Result = this->Surface[Surface]->Blt(&Area, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT,
			&Effects);
	}
	BlitCount++;
	BlitPixels += Height;
}

// 0x4eb720. Data has three bytes (R, G, B) per pixel. Odd behaviour kept: a
// palette surface (less than 16 bits) stays locked.
void TSurfaceDX::WriteImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data)
{
	DDSURFACEDESC2 Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.dwSize = sizeof(DDSURFACEDESC);
	HRESULT Result;
	while ((Result = Surface[ToSurface]->Lock(nullptr, &Desc,
				DDLOCK_WAIT | DDLOCK_NOSYSLOCK, nullptr))
		!= DD_OK) {
		if (Result != DDERR_SURFACEBUSY)
			return;
		Sleep(0);
	}
	uint8 *Pixels = static_cast<uint8 *>(Desc.lpSurface);
	DDPIXELFORMAT Format;
	memset(&Format, 0, sizeof(Format));
	Format.dwSize = sizeof(Format);
	Surface[ToSurface]->GetPixelFormat(&Format);
	if (Format.dwRGBBitCount < 16)
		return;
	const int BytesPerPixel = Format.dwRGBBitCount >> 3;
	for (int Row = 0; Row < Height; Row++) {
		int Offset = (y + Row) * Desc.lPitch + x * BytesPerPixel;
		for (int Column = 0; Column < Width; Column++) {
			const uint32 Pixel = GetPixelColor(Surface[ToSurface], TRGBColor(Data[0], Data[1], Data[2]));
			Data += 3;
			uint8 Bytes[4];
			Bytes[3] = (uint8)Pixel;
			Bytes[2] = (uint8)(Pixel >> 8);
			Bytes[1] = (uint8)(Pixel >> 16);
			Bytes[0] = (uint8)(Pixel >> 24);
			for (int LoopIndex = 3; LoopIndex >= 4 - BytesPerPixel; LoopIndex--)
				Pixels[Offset++] = Bytes[LoopIndex];
		}
	}
	Surface[ToSurface]->Unlock(nullptr);
}

// 0x4eb900. The outfit colouring: with a mask, the mask's yellow, red, green
// and blue pixels take Colors[0-3] (head, body, legs, feet) shaded by the
// image pixel's brightness; without a mask every pixel but those of
// Colors[1] takes Colors[0]. Odd behaviour kept: a palette surface stays
// locked.
void TSurfaceDX::WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data,
	uint8 *Mask, TRGBColor *Colors)
{
	DDSURFACEDESC2 Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.dwSize = sizeof(DDSURFACEDESC);
	HRESULT Result;
	while ((Result = Surface[ToSurface]->Lock(nullptr, &Desc,
				DDLOCK_WAIT | DDLOCK_NOSYSLOCK, nullptr))
		!= DD_OK) {
		if (Result != DDERR_SURFACEBUSY)
			return;
		Sleep(0);
	}
	uint8 *Pixels = static_cast<uint8 *>(Desc.lpSurface);
	DDPIXELFORMAT Format;
	memset(&Format, 0, sizeof(Format));
	Format.dwSize = sizeof(Format);
	Surface[ToSurface]->GetPixelFormat(&Format);
	if (Format.dwRGBBitCount < 16)
		return;
	const int BytesPerPixel = Format.dwRGBBitCount >> 3;
	for (int Row = 0; Row < Height; Row++) {
		int Offset = (y + Row) * Desc.lPitch + x * BytesPerPixel;
		for (int Column = 0; Column < Width; Column++) {
			int Red = Data[0];
			int Green = Data[1];
			int Blue = Data[2];
			Data += 3;
			int Index = -1;
			if (Mask != nullptr) {
				const int MaskR = Mask[0];
				const int MaskG = Mask[1];
				const int MaskB = Mask[2];
				Mask += 3;
				if (MaskR == 255 && MaskG == 255 && MaskB == 0)
					Index = 0;
				else if (MaskR == 255 && MaskG == 0 && MaskB == 0)
					Index = 1;
				else if (MaskR == 0 && MaskG == 255 && MaskB == 0)
					Index = 2;
				else if (MaskR == 0 && MaskG == 0 && MaskB == 255)
					Index = 3;
			} else if (Red != Colors[1].R || Green != Colors[1].G || Blue != Colors[1].B) {
				Index = 0;
			}
			if (Index != -1) {
				const int Gray = (Red * 30 + Green * 59 + Blue * 11) / 100;
				Red = Colors[Index].R * Gray / 256;
				Green = Colors[Index].G * Gray / 256;
				Blue = Colors[Index].B * Gray / 256;
			}
			const uint32 Pixel = GetPixelColor(Surface[ToSurface], TRGBColor(Red, Green, Blue));
			uint8 Bytes[4];
			Bytes[3] = (uint8)Pixel;
			Bytes[2] = (uint8)(Pixel >> 8);
			Bytes[1] = (uint8)(Pixel >> 16);
			Bytes[0] = (uint8)(Pixel >> 24);
			for (int LoopIndex = 3; LoopIndex >= 4 - BytesPerPixel; LoopIndex--)
				Pixels[Offset++] = Bytes[LoopIndex];
		}
	}
	Surface[ToSurface]->Unlock(nullptr);
}

// 0x4ebc30. Not clipped, no colour key; DirectDraw stretches when the
// rectangles differ in size.
void TSurfaceDX::StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source)
{
	RECT To = {Destination.x, Destination.y, Destination.x + Destination.width,
		Destination.y + Destination.height};
	RECT From = {Source.x, Source.y, Source.x + Source.width, Source.y + Source.height};
	HRESULT Result = Surface[ToSurface]->Blt(&To, Surface[FromSurface], &From, DDBLT_WAIT, nullptr);
	while (Result == DDERR_SURFACEBUSY) {
		Sleep(0);
		Result = Surface[ToSurface]->Blt(&To, Surface[FromSurface], &From, DDBLT_WAIT, nullptr);
	}
	BlitCount++;
	BlitPixels += Destination.width * Destination.height;
}

// 0x4ebd20
void TSurfaceDX::BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
	TRect Clip)
{
	RECT To = {x, y, x + FromRect.width, y + FromRect.height};
	RECT From = {FromRect.x, FromRect.y, FromRect.x + FromRect.width, FromRect.y + FromRect.height};
	if (!ClipRect(Clip, &To, &From))
		return;
	SetSourceColorKey(FromSurface, ColorKey);
	HRESULT Result = Surface[ToSurface]->Blt(&To, Surface[FromSurface], &From,
		DDBLT_WAIT | DDBLT_KEYSRC, nullptr);
	while (Result == DDERR_SURFACEBUSY) {
		Sleep(0);
		Result = Surface[ToSurface]->Blt(&To, Surface[FromSurface], &From, DDBLT_WAIT | DDBLT_KEYSRC,
			nullptr);
	}
	BlitCount++;
	BlitPixels += FromRect.width * FromRect.height;
}

// 0x4ebe60
void TSurfaceDX::BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip)
{
	RECT To = {x, y, x + FromRect.width, y + FromRect.height};
	RECT From = {FromRect.x, FromRect.y, FromRect.x + FromRect.width, FromRect.y + FromRect.height};
	if (!ClipRect(Clip, &To, &From))
		return;
	HRESULT Result = Surface[ToSurface]->Blt(&To, Surface[FromSurface], &From, DDBLT_WAIT, nullptr);
	while (Result == DDERR_SURFACEBUSY) {
		Sleep(0);
		Result = Surface[ToSurface]->Blt(&To, Surface[FromSurface], &From, DDBLT_WAIT, nullptr);
	}
	BlitCount++;
	BlitPixels += FromRect.width * FromRect.height;
}

// 0x4ebf80. Only on the Direct3D render target (TEMP_SURFACE): each
// rectangle becomes a triangle strip with the colours of its corners, which
// the blend states set in CreateSurfaces multiply with the surface. Nothing
// is drawn when BeginScene fails.
void TSurfaceDX::ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors)
{
	if (Surface != TEMP_SURFACE || Direct3D == nullptr)
		return;
	D3DTLVERTEX Vertices[4];
	if (Device->BeginScene() != D3D_OK)
		return;
	for (int LoopIndex = 0; LoopIndex < Count; LoopIndex++) {
		const TRect *Area = &Rects[LoopIndex];
		const TRGBColor *CornerColors = &Colors[LoopIndex * 4];
		Vertices[0] = D3DTLVERTEX(D3DVECTOR((float)Area->x, (float)Area->y, 0.0f), 1.0f,
			RGB_MAKE(CornerColors[0].R, CornerColors[0].G, CornerColors[0].B), 0xff000000, 0.0f, 0.0f);
		Vertices[1] = D3DTLVERTEX(D3DVECTOR((float)(Area->x + Area->width), (float)Area->y, 0.0f), 1.0f,
			RGB_MAKE(CornerColors[1].R, CornerColors[1].G, CornerColors[1].B), 0xff000000, 1.0f, 0.0f);
		Vertices[2] = D3DTLVERTEX(D3DVECTOR((float)Area->x, (float)(Area->y + Area->height), 0.0f), 1.0f,
			RGB_MAKE(CornerColors[2].R, CornerColors[2].G, CornerColors[2].B), 0xff000000, 0.0f, 1.0f);
		Vertices[3] = D3DTLVERTEX(D3DVECTOR((float)(Area->x + Area->width), (float)(Area->y + Area->height), 0.0f),
			1.0f, RGB_MAKE(CornerColors[3].R, CornerColors[3].G, CornerColors[3].B), 0xff000000, 1.0f, 1.0f);
		Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, Vertices, 4, 0);
	}
	Device->EndScene();
}

// 0x4ec220. Shows the screen surface: blitted into the window (windowed) or
// flipped (fullscreen); the statistics of the frame are kept and reset.
// Desc is not zeroed before GetSurfaceDesc (only its size set).
void TSurfaceDX::FinishFrame()
{
	if (ViewMode == 0)
		return;
	WaitForNextFrame();
	ScreenWidth = Rect[SCREEN_SURFACE].right;
	ScreenHeight = Rect[SCREEN_SURFACE].bottom;
	DDSURFACEDESC2 Desc;
	Desc.dwSize = sizeof(Desc);
	Surface[SCREEN_SURFACE]->GetSurfaceDesc(&Desc);
	ScreenDepth = Desc.ddpfPixelFormat.dwRGBBitCount;
	LastBlitCount = BlitCount;
	LastBlitPixels = BlitPixels;
	BlitCount = 0;
	BlitPixels = 0;
	if (ViewMode == WINDOWED_MODE) {
		while (Surface[PRIMARY_SURFACE]->GetBltStatus(DDGBS_ISBLTDONE) == DDERR_WASSTILLDRAWING)
			Sleep(0);
		HRESULT Result = Surface[PRIMARY_SURFACE]->Blt(&WindowRect, Surface[SCREEN_SURFACE],
			&Rect[SCREEN_SURFACE], DDBLT_WAIT, nullptr);
		while (Result == DDERR_SURFACEBUSY) {
			Sleep(0);
			Result = Surface[PRIMARY_SURFACE]->Blt(&WindowRect, Surface[SCREEN_SURFACE],
				&Rect[SCREEN_SURFACE], DDBLT_WAIT, nullptr);
		}
		BlitCount++;
		BlitPixels += (WindowRect.bottom - WindowRect.top) * (WindowRect.right - WindowRect.left);
	} else {
		while (Surface[PRIMARY_SURFACE]->Flip(nullptr, 0) == DDERR_WASSTILLDRAWING)
			Sleep(0);
	}
}

// 0x4ec3a0. An offscreen surface in video memory, else (unless
// VideoMemoryOnly) in system memory; the memory is recorded in SurfaceMemory.
bool TSurfaceDX::CreateOffscreenSurface(IDirectDrawSurface4 **Result, int Width, int Height,
	bool VideoMemoryOnly) const
{
	DDSURFACEDESC2 Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.dwSize = sizeof(Desc);
	Desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	Desc.dwHeight = Height;
	Desc.dwWidth = Width;
	Desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	if (DirectDraw->CreateSurface(&Desc, Result, nullptr) == DD_OK) {
		SurfaceMemory[NumberOfSurfaces] = 1;
		NumberOfSurfaces++;
		return true;
	}
	if (VideoMemoryOnly)
		return false;
	Desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	if (DirectDraw->CreateSurface(&Desc, Result, nullptr) != DD_OK)
		return false;
	SurfaceMemory[NumberOfSurfaces] = 2;
	NumberOfSurfaces++;
	return true;
}

// 0x4ec470. The same for a Direct3D render target.
bool TSurfaceDX::Create3DSurface(IDirectDrawSurface4 **Result, int Width, int Height,
	bool VideoMemoryOnly) const
{
	DDSURFACEDESC2 Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.dwSize = sizeof(Desc);
	Desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	Desc.dwHeight = Height;
	Desc.dwWidth = Width;
	Desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY;
	if (DirectDraw->CreateSurface(&Desc, Result, nullptr) == DD_OK) {
		SurfaceMemory[NumberOfSurfaces] = 1;
		NumberOfSurfaces++;
		return true;
	}
	if (VideoMemoryOnly)
		return false;
	Desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
	if (DirectDraw->CreateSurface(&Desc, Result, nullptr) != DD_OK)
		return false;
	SurfaceMemory[NumberOfSurfaces] = 2;
	NumberOfSurfaces++;
	return true;
}

// 0x4ec540. Releases the surfaces and Direct3D and leaves fullscreen mode;
// DirectDraw itself stays (ExitSurfaces releases it). The screen surface is
// released first (in fullscreen mode it is the primary's back buffer).
void TSurfaceDX::ExitDisplay()
{
	if (DirectDraw == nullptr)
		return;
	if (ViewMode == FULLSCREEN_MODE)
		DirectDraw->SetCooperativeLevel(hWindow, DDSCL_NORMAL);
	if (Surface[SCREEN_SURFACE] != nullptr) {
		Surface[SCREEN_SURFACE]->Release();
		Surface[SCREEN_SURFACE] = nullptr;
	}
	for (int LoopIndex = 0; LoopIndex < NUMBER_OF_SURFACES; LoopIndex++) {
		if (Surface[LoopIndex] != nullptr) {
			Surface[LoopIndex]->Release();
			Surface[LoopIndex] = nullptr;
			SurfaceMemory[LoopIndex] = 0;
		}
	}
	CacheSurfaces = 0;
	if (Viewport != nullptr) {
		Viewport->Release();
		Viewport = nullptr;
	}
	if (Device != nullptr) {
		Device->Release();
		Device = nullptr;
	}
	if (Direct3D != nullptr) {
		Direct3D->Release();
		Direct3D = nullptr;
	}
	ViewMode = 0;
}

// 0x4ec600. A failure releases what was made before the error goes on.
void TSurfaceDX::ResetDisplay(int VMode, TFullscreenMode *GMode)
{
	try {
		InitDisplay(VMode, GMode);
		CreateSurfaces();
	} catch (PROGRAM_ERROR Error) {
		try {
			ExitDisplay();
		} catch (...) {
		}
		throw Error;
	}
}

// 0x4ec690. Sets Unavailable while another application has the exclusive
// mode; creates everything again when a surface was lost.
void TSurfaceDX::RestoreSurfaces(int ViewMode, TFullscreenMode *Mode)
{
	if (DirectDraw->TestCooperativeLevel() == DDERR_EXCLUSIVEMODEALREADYSET) {
		Unavailable = true;
		return;
	}
	Unavailable = false;
	for (int LoopIndex = 0; LoopIndex < NUMBER_OF_SURFACES; LoopIndex++) {
		if (Surface[LoopIndex] != nullptr && Surface[LoopIndex]->IsLost() == DDERR_SURFACELOST) {
			ExitDisplay();
			ResetDisplay(ViewMode, Mode);
			return;
		}
	}
}

// 0x4ec700. Changes the display only when needed: windowed mode keeps its
// surfaces when only the window changed; fullscreen mode is set again only
// for another resolution (the frequency is not compared). Always true.
bool TSurfaceDX::Init(HWND *Window, int ViewMode, TFullscreenMode *Resolution, bool Force)
{
	Recreated = false;
	if (Force) {
		ExitDisplay();
		hWindow = *Window;
		ResetDisplay(ViewMode, Resolution);
	} else if (ViewMode == WINDOWED_MODE) {
		switch (this->ViewMode) {
		case 0:
			hWindow = *Window;
			ResetDisplay(WINDOWED_MODE, nullptr);
			break;
		case WINDOWED_MODE:
			if (hWindow != *Window) {
				hWindow = *Window;
				AttachWindow();
			}
			UpdateWindowRect();
			break;
		case FULLSCREEN_MODE:
			ExitDisplay();
			hWindow = *Window;
			ResetDisplay(WINDOWED_MODE, nullptr);
			break;
		}
	} else if (ViewMode == FULLSCREEN_MODE) {
		if (this->ViewMode != FULLSCREEN_MODE || Resolution->Width != Width
			|| Resolution->Height != Height || Resolution->Depth != Depth) {
			ExitDisplay();
			hWindow = *Window;
			ResetDisplay(FULLSCREEN_MODE, Resolution);
		}
	}
	RestoreSurfaces(ViewMode, Resolution);
	return true;
}

// 0x4ec810. EnumDisplayModes' callback: appends the modes of at least
// 640x480 with 16 bits to the list (Context), counted in the surface
// object's NumberOfModes (through GetSurface). The list grows by one entry
// per mode. Odd behavior kept: the refresh-rate flag is tested in the
// pixel format's flags, not the descriptor's flags (0x4ec869).
static HRESULT WINAPI AddFullscreenMode(DDSURFACEDESC2 *Desc, void *Context)
{
	TFullscreenMode **Modes = static_cast<TFullscreenMode **>(Context);
	if (*Modes == nullptr)
		GetSurface()->NumberOfModes = 0;
	const int Width = (Desc->dwFlags & DDSD_WIDTH) ? Desc->dwWidth : 0;
	const int Height = (Desc->dwFlags & DDSD_HEIGHT) ? Desc->dwHeight : 0;
	const int Depth = (Desc->ddpfPixelFormat.dwFlags & DDPF_RGB) ? Desc->ddpfPixelFormat.dwRGBBitCount : 0;
	const int Frequency = (Desc->ddpfPixelFormat.dwFlags & DDSD_REFRESHRATE) ? Desc->dwRefreshRate : 0;
	if (Width >= 640 && Height >= 480 && Depth >= 16) {
		TFullscreenMode *List = new TFullscreenMode[GetSurface()->NumberOfModes + 1];
		if (*Modes != nullptr) {
			for (int LoopIndex = 0; LoopIndex < GetSurface()->NumberOfModes; LoopIndex++)
				List[LoopIndex] = (*Modes)[LoopIndex];
			delete[] *Modes;
		}
		List[GetSurface()->NumberOfModes].Width = Width;
		List[GetSurface()->NumberOfModes].Height = Height;
		List[GetSurface()->NumberOfModes].Depth = Depth;
		List[GetSurface()->NumberOfModes].Frequency = Frequency;
		GetSurface()->NumberOfModes++;
		*Modes = List;
	}
	return DDENUMRET_OK;
}

// 0x4ec980
int TSurfaceDX::GetFullscreenModes(TFullscreenMode **Modes)
{
	*Modes = nullptr;
	if (DirectDraw->EnumDisplayModes(0, nullptr, Modes, AddFullscreenMode) == DD_OK)
		return NumberOfModes;
	if (*Modes != nullptr)
		delete[] *Modes;
	*Modes = nullptr;
	return 0;
}

// 0x4ec9d0
bool TSurfaceDX::CanShadeRectangles()
{
	return Direct3D != nullptr;
}

// 0x4ec9e0
void TSurfaceDX::ExitSurfaces()
{
	ExitDisplay();
	if (DirectDraw != nullptr) {
		DirectDraw->Release();
		DirectDraw = nullptr;
	}
}

// 0x4eca00. What GetSurface creates for engine 0.
TSurface *CreateSurfaceDX()
{
	return new TSurfaceDX;
}

// 0x4eca80. ShadeRectangles for one rectangle with the four colours as
// arguments, converted with D3DRGB. Only TestShading uses it. The binary
// multiplies by 1/255 (0x56f11c) in double precision; the casts keep that.
void TSurfaceDX::ShadeRectangle(int Surface, TRect Rect, TRGBColor TopLeft, TRGBColor TopRight,
	TRGBColor BottomLeft, TRGBColor BottomRight) const
{
	if (Surface != TEMP_SURFACE || Direct3D == nullptr)
		return;
	D3DTLVERTEX Vertices[4];
	if (Device->BeginScene() != D3D_OK)
		return;
	const double Scale = 1.0f / 255.0f;
	Vertices[0] = D3DTLVERTEX(D3DVECTOR((float)Rect.x, (float)Rect.y, 0.0f), 1.0f,
		D3DRGB(TopLeft.R * Scale, TopLeft.G * Scale, TopLeft.B * Scale), 0xff000000, 0.0f, 0.0f);
	Vertices[1] = D3DTLVERTEX(D3DVECTOR((float)(Rect.x + Rect.width), (float)Rect.y, 0.0f), 1.0f,
		D3DRGB(TopRight.R * Scale, TopRight.G * Scale, TopRight.B * Scale), 0xff000000, 1.0f, 0.0f);
	Vertices[2] = D3DTLVERTEX(D3DVECTOR((float)Rect.x, (float)(Rect.y + Rect.height), 0.0f), 1.0f,
		D3DRGB(BottomLeft.R * Scale, BottomLeft.G * Scale, BottomLeft.B * Scale), 0xff000000,
		0.0f, 1.0f);
	Vertices[3] = D3DTLVERTEX(D3DVECTOR((float)(Rect.x + Rect.width), (float)(Rect.y + Rect.height),
								  0.0f),
		1.0f, D3DRGB(BottomRight.R * Scale, BottomRight.G * Scale, BottomRight.B * Scale),
		0xff000000, 1.0f, 1.0f);
	Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, Vertices, 4, 0);
	Device->EndScene();
}

// 0x4ecdd0. Whether the shading works: a white square shaded with black must
// be black at (16,16). The Lock and pixel format checks are those of
// SetPixel.
bool TSurfaceDX::TestShading(int Surface)
{
	if (Surface != TEMP_SURFACE || Direct3D == nullptr)
		return false;
	FillRectangle(Surface, TRect(0, 0, 32, 32), TRGBColor(255, 255, 255));
	ShadeRectangle(Surface, TRect(0, 0, 32, 32), TRGBColor(0, 0, 0), TRGBColor(0, 0, 0),
		TRGBColor(0, 0, 0), TRGBColor(0, 0, 0));
	DDSURFACEDESC2 Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.dwSize = sizeof(DDSURFACEDESC);
	HRESULT Result;
	while ((Result = this->Surface[Surface]->Lock(nullptr, &Desc,
				DDLOCK_WAIT | DDLOCK_NOSYSLOCK, nullptr))
		!= DD_OK) {
		if (Result != DDERR_SURFACEBUSY)
			return false;
		Sleep(0);
	}
	const uint8 *Pixels = static_cast<uint8 *>(Desc.lpSurface);
	DDPIXELFORMAT Format;
	memset(&Format, 0, sizeof(Format));
	Format.dwSize = sizeof(Format);
	this->Surface[Surface]->GetPixelFormat(&Format);
	if (Format.dwRGBBitCount < 16) {
		this->Surface[Surface]->Unlock(nullptr);
		return false;
	}
	const int BytesPerPixel = Format.dwRGBBitCount >> 3;
	const uint8 *Pixel = Pixels + 16 * (Desc.lPitch + BytesPerPixel);
	for (int LoopIndex = 3; LoopIndex >= 4 - BytesPerPixel; LoopIndex--) {
		if (*Pixel++ != 0) {
			this->Surface[Surface]->Unlock(nullptr);
			return false;
		}
	}
	this->Surface[Surface]->Unlock(nullptr);
	return true;
}

// 0x4ecfd0. Creates DirectDraw (PROGRAM_ERROR 7 when it fails) and sets the
// surface sizes: the work surfaces and 16 video cache surfaces of 320x320
// (CacheSurfaceWidth/Height, shared with the other engines). The hardware
// stretches when it can shrink and stretch both ways; otherwise "no
// stretching" becomes the default. Odd behaviour kept: the IDirectDraw
// interface from DirectDrawCreate is never released.
void TSurfaceDX::InitSurfaces()
{
	hWindow = nullptr;
	ViewMode = 0;
	DirectDraw = nullptr;
	Direct3D = nullptr;
	Device = nullptr;
	Viewport = nullptr;
	CacheSurfaceWidth = 320;
	CacheSurfaceHeight = 320;
	for (int LoopIndex = 0; LoopIndex < NUMBER_OF_SURFACES; LoopIndex++) {
		Surface[LoopIndex] = nullptr;
		SurfaceMemory[LoopIndex] = 0;
	}
	SetRect(&Rect[TEMP_SURFACE], 0, 0, 640, 480);
	SetRect(&Rect[STRETCH_SURFACE], 0, 0, 64, 64);
	SetRect(&Rect[CACHE_SURFACE], 0, 0, 512, 384);
	SetRect(&Rect[AUTOMAP_SURFACE], 0, 0, 106, 106);
	for (int LoopIndex = SPRITE_SURFACE; LoopIndex < NUMBER_OF_SURFACES; LoopIndex++) {
		Rect[LoopIndex].left = 0;
		Rect[LoopIndex].top = 0;
		Rect[LoopIndex].right = CacheSurfaceWidth;
		Rect[LoopIndex].bottom = CacheSurfaceHeight;
	}
	IDirectDraw *DirectDraw1;
	if (DirectDrawCreate(nullptr, &DirectDraw1, nullptr) != DD_OK)
		throw (PROGRAM_ERROR)7;
	if (DirectDraw1->QueryInterface(IID_IDirectDraw4, reinterpret_cast<void **>(&DirectDraw)) != DD_OK)
		throw (PROGRAM_ERROR)7;
	DDCAPS HardwareCaps;
	memset(&HardwareCaps, 0, sizeof(HardwareCaps));
	HardwareCaps.dwSize = sizeof(DDCAPS);
	DDCAPS SoftwareCaps;
	memset(&SoftwareCaps, 0, sizeof(SoftwareCaps));
	SoftwareCaps.dwSize = sizeof(DDCAPS);
	if (DirectDraw->GetCaps(&HardwareCaps, &SoftwareCaps) == DD_OK) {
		CanStretch = (HardwareCaps.dwFXCaps & DDFXCAPS_BLTSHRINKX) != 0
			&& (HardwareCaps.dwFXCaps & DDFXCAPS_BLTSTRETCHX) != 0
			&& (HardwareCaps.dwFXCaps & DDFXCAPS_BLTSHRINKY) != 0
			&& (HardwareCaps.dwFXCaps & DDFXCAPS_BLTSTRETCHY) != 0;
	} else {
		CanStretch = false;
	}
	SetDefaultNoStretching(!CanStretch);
	ResetFrameTimer();
	SetFramerateLimit(GetConfineFramerate());
}

// 0x4ed1a0, lines 267-269. Clips the primary surface to the window
// (PROGRAM_ERROR 9 when no clipper can be made).
void TSurfaceDX::AttachWindow()
{
	TASSERT(267, DirectDraw!=NULL);
	TASSERT(268, hWindow!=NULL);
	TASSERT(269, Surface[PRIMARY_SURFACE]!=NULL);
	IDirectDrawClipper *Clipper;
	if (DirectDraw->CreateClipper(0, &Clipper, nullptr) != DD_OK)
		throw (PROGRAM_ERROR)9;
	Clipper->SetHWnd(0, hWindow);
	Surface[PRIMARY_SURFACE]->SetClipper(Clipper);
	Clipper->Release();
}

// 0x4ed520, lines 614-619. Stretches through the two work surfaces: the
// source goes with its colour key onto TEMP_SURFACE filled with Highlight,
// is stretched onto STRETCH_SURFACE and goes from there to the destination
// with Highlight as colour key. A plain Blit when the sizes are equal.
void TSurfaceDX::StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect,
	TRGBColor ColorKey, TRGBColor Highlight)
{
	if (ToRect.width == FromRect.width && ToRect.height == FromRect.height) {
		Blit(ToSurface, ToRect.x, ToRect.y, FromSurface, FromRect, ColorKey);
		return;
	}
	TASSERT(614, FromSurface!=TEMP_SURFACE);
	TASSERT(615, FromSurface!=STRETCH_SURFACE);
	TASSERT(616, ToSurface!=TEMP_SURFACE);
	TASSERT(617, ToSurface!=STRETCH_SURFACE);
	TASSERT(618, FromRect.width+1<=Rect[TEMP_SURFACE].right && FromRect.height+1<=Rect[TEMP_SURFACE].bottom);
	TASSERT(619, ToRect.width<=Rect[STRETCH_SURFACE].right && ToRect.height<=Rect[STRETCH_SURFACE].bottom);
	FillRectangle(TEMP_SURFACE, TRect(0, 0, FromRect.width + 1, FromRect.height + 1), Highlight);
	Blit(TEMP_SURFACE, 0, 0, FromSurface, FromRect, ColorKey);
	StretchBlitRect(STRETCH_SURFACE, TRect(0, 0, ToRect.width, ToRect.height), TEMP_SURFACE,
		TRect(0, 0, FromRect.width, FromRect.height));
	Blit(ToSurface, ToRect.x, ToRect.y, STRETCH_SURFACE, TRect(0, 0, ToRect.width, ToRect.height), Highlight);
}

// 0x4edc70, lines 678-683. The same with clipping in the last blit.
void TSurfaceDX::StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
	TRGBColor Highlight, TRect Clip)
{
	if (ToRect.width == FromRect.width && ToRect.height == FromRect.height) {
		BlitClipped(ToSurface, ToRect.x, ToRect.y, FromSurface, FromRect, ColorKey, Clip);
		return;
	}
	TASSERT(678, FromSurface!=TEMP_SURFACE);
	TASSERT(679, FromSurface!=STRETCH_SURFACE);
	TASSERT(680, ToSurface!=TEMP_SURFACE);
	TASSERT(681, ToSurface!=STRETCH_SURFACE);
	TASSERT(682, FromRect.width+1<=Rect[TEMP_SURFACE].right && FromRect.height+1<=Rect[TEMP_SURFACE].bottom);
	TASSERT(683, ToRect.width<=Rect[STRETCH_SURFACE].right && ToRect.height<=Rect[STRETCH_SURFACE].bottom);
	FillRectangle(TEMP_SURFACE, TRect(0, 0, FromRect.width + 1, FromRect.height + 1), Highlight);
	Blit(TEMP_SURFACE, 0, 0, FromSurface, FromRect, ColorKey);
	StretchBlitRect(STRETCH_SURFACE, TRect(0, 0, ToRect.width, ToRect.height), TEMP_SURFACE,
		TRect(0, 0, FromRect.width, FromRect.height));
	BlitClipped(ToSurface, ToRect.x, ToRect.y, STRETCH_SURFACE, TRect(0, 0, ToRect.width, ToRect.height), Highlight,
		Clip);
}

// 0x4ee410, line 811. Clipped to the destination surface.
void TSurfaceDX::Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey)
{
	TASSERT(811, FromRect.width>0 && FromRect.height>0);
	TRect Clip;
	GetSurfaceRect(ToSurface, &Clip);
	BlitClipped(ToSurface, x, y, FromSurface, FromRect, ColorKey, Clip);
}

// 0x4ee630, line 824
void TSurfaceDX::BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect)
{
	TASSERT(824, FromRect.width>0 && FromRect.height>0);
	TRect Clip;
	GetSurfaceRect(ToSurface, &Clip);
	BlitRectClipped(ToSurface, x, y, FromSurface, FromRect, Clip);
}

// 0x4ee830, lines 972-975. Sets the cooperative level and, in fullscreen
// mode, the display mode (PROGRAM_ERROR 10 and 11 when they fail).
void TSurfaceDX::InitDisplay(int VMode, TFullscreenMode *GMode)
{
	TASSERT(972, DirectDraw!=NULL);
	TASSERT(973, hWindow!=NULL);
	TASSERT(974, VMode==WINDOWED_MODE || VMode==FULLSCREEN_MODE);
	TASSERT(975, VMode==WINDOWED_MODE || GMode!=NULL);
	if (VMode == WINDOWED_MODE) {
		if (DirectDraw->SetCooperativeLevel(hWindow, DDSCL_NORMAL) != DD_OK)
			throw (PROGRAM_ERROR)10;
		GetClientRect(hWindow, &Rect[PRIMARY_SURFACE]);
		GetClientRect(hWindow, &Rect[SCREEN_SURFACE]);
		GetClientRect(hWindow, &WindowRect);
		ClientToScreen(hWindow, reinterpret_cast<POINT *>(&WindowRect.left));
		ClientToScreen(hWindow, reinterpret_cast<POINT *>(&WindowRect.right));
	} else {
		Width = GMode->Width;
		Height = GMode->Height;
		Depth = GMode->Depth;
		if (DirectDraw->SetCooperativeLevel(hWindow, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) != DD_OK)
			throw (PROGRAM_ERROR)10;
		if (DirectDraw->SetDisplayMode(Width, Height, Depth, 0, 0) != DD_OK)
			throw (PROGRAM_ERROR)11;
		SetRect(&Rect[PRIMARY_SURFACE], 0, 0, Width, Height);
		SetRect(&Rect[SCREEN_SURFACE], 0, 0, Width, Height);
		SetRect(&WindowRect, 0, 0, Width, Height);
	}
	ViewMode = VMode;
}

// 0x4eed40, lines 1106-1114. Creates the primary and screen surfaces (a
// window-sized back buffer of the whole screen, or a flipping chain), then
// tries Direct3D on TEMP_SURFACE: every step counts InitStep up (0 when all
// passed, so the step that failed stays visible), and a failure gives up
// the shading (only Direct3D is released; the device, the viewport and the
// render target stay, and the next TEMP_SURFACE replaces the latter).
// Then the work surfaces and the cache surfaces: the first 8 anywhere, the
// others only in video memory while it lasts. PROGRAM_ERROR 8 for a
// palette display, 12-18 when a surface cannot be made. Odd behaviour
// kept: NumberOfSurfaces is decremented even when the render target was
// not created; GetDisplayMode's Desc is not zeroed.
void TSurfaceDX::CreateSurfaces()
{
	TASSERT(1106, DirectDraw!=NULL);
	TASSERT(1107, Direct3D==NULL);
	TASSERT(1108, Surface[PRIMARY_SURFACE]==NULL);
	TASSERT(1109, Surface[SCREEN_SURFACE]==NULL);
	TASSERT(1110, Surface[TEMP_SURFACE]==NULL);
	TASSERT(1111, Surface[CACHE_SURFACE]==NULL);
	TASSERT(1112, Surface[AUTOMAP_SURFACE]==NULL);
	TASSERT(1113, Surface[SPRITE_SURFACE]==NULL);
	TASSERT(1114, ViewMode==WINDOWED_MODE || ViewMode==FULLSCREEN_MODE);
	NumberOfSurfaces = 0;
	DDSURFACEDESC2 Desc;
	Desc.dwSize = sizeof(Desc);
	DirectDraw->GetDisplayMode(&Desc);
	if (Desc.ddpfPixelFormat.dwRGBBitCount < 16)
		throw (PROGRAM_ERROR)8;
	if (ViewMode == WINDOWED_MODE) {
		memset(&Desc, 0, sizeof(Desc));
		Desc.dwSize = sizeof(Desc);
		Desc.dwFlags = DDSD_CAPS;
		Desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		if (DirectDraw->CreateSurface(&Desc, &Surface[PRIMARY_SURFACE], nullptr) != DD_OK)
			throw (PROGRAM_ERROR)12;
		AttachWindow();
		SurfaceMemory[PRIMARY_SURFACE] = 1;
		NumberOfSurfaces = 1;
		if (!CreateOffscreenSurface(&Surface[SCREEN_SURFACE], GetSystemMetrics(SM_CXSCREEN),
				GetSystemMetrics(SM_CYSCREEN), false))
			throw (PROGRAM_ERROR)13;
	} else if (ViewMode == FULLSCREEN_MODE) {
		memset(&Desc, 0, sizeof(Desc));
		Desc.dwSize = sizeof(Desc);
		Desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		Desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
		Desc.dwBackBufferCount = 1;
		if (DirectDraw->CreateSurface(&Desc, &Surface[PRIMARY_SURFACE], nullptr) != DD_OK)
			throw (PROGRAM_ERROR)12;
		SurfaceMemory[PRIMARY_SURFACE] = 1;
		DDSCAPS2 Caps;
		Caps.dwCaps = DDSCAPS_BACKBUFFER;
		if (Surface[PRIMARY_SURFACE]->GetAttachedSurface(&Caps, &Surface[SCREEN_SURFACE]) != DD_OK)
			throw (PROGRAM_ERROR)13;
		SurfaceMemory[SCREEN_SURFACE] = 1;
	}
	NumberOfSurfaces = 2;
	D3DVIEWPORT ViewportData;
	memset(&ViewportData, 0, sizeof(ViewportData));
	ViewportData.dwSize = sizeof(ViewportData);
	ViewportData.dwWidth = Rect[TEMP_SURFACE].right;
	ViewportData.dwHeight = Rect[TEMP_SURFACE].bottom;
	ViewportData.dvScaleX = Rect[TEMP_SURFACE].right * 0.5f;
	ViewportData.dvScaleY = Rect[TEMP_SURFACE].bottom * 0.5f;
	ViewportData.dvMaxX = 1.0f;
	ViewportData.dvMaxY = 1.0f;
	if (DirectDraw->QueryInterface(IID_IDirect3D3, reinterpret_cast<void **>(&Direct3D)) != DD_OK)
		Direct3D = nullptr;
	InitStep = 2;
	if (Direct3D != nullptr) {
		D3DDEVICEDESC HardwareDesc;
		memset(&HardwareDesc, 0, sizeof(HardwareDesc));
		D3DDEVICEDESC SoftwareDesc;
		memset(&SoftwareDesc, 0, sizeof(SoftwareDesc));
		HardwareDesc.dwSize = sizeof(D3DDEVICEDESC);
		SoftwareDesc.dwSize = sizeof(D3DDEVICEDESC);
		if (Create3DSurface(&Surface[TEMP_SURFACE], Rect[TEMP_SURFACE].right,
				Rect[TEMP_SURFACE].bottom, false)
			&& ++InitStep
			&& Direct3D->CreateDevice(*DeviceGUID, Surface[TEMP_SURFACE], &Device, nullptr) == D3D_OK
			&& ++InitStep
			&& Direct3D->CreateViewport(&Viewport, nullptr) == D3D_OK && ++InitStep
			&& Device->AddViewport(Viewport) == D3D_OK && ++InitStep
			&& Viewport->SetViewport(&ViewportData) == D3D_OK && ++InitStep
			&& Device->GetCaps(&HardwareDesc, &SoftwareDesc) == D3D_OK && ++InitStep
			&& (HardwareDesc.dpcTriCaps.dwSrcBlendCaps & D3DPBLENDCAPS_ZERO) != 0 && ++InitStep
			&& (HardwareDesc.dpcTriCaps.dwDestBlendCaps & D3DPBLENDCAPS_SRCCOLOR) != 0 && ++InitStep
			&& Device->SetCurrentViewport(Viewport) == D3D_OK && ++InitStep
			&& Device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE) == D3D_OK && ++InitStep
			&& Device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO) == D3D_OK && ++InitStep
			&& Device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCCOLOR) == D3D_OK
			&& ++InitStep
			&& TestShading(TEMP_SURFACE) && ++InitStep) {
			InitStep = 0;
		} else {
			NumberOfSurfaces--;
			Direct3D->Release();
			Direct3D = nullptr;
		}
	}
	if (Direct3D == nullptr
		&& !CreateOffscreenSurface(&Surface[TEMP_SURFACE], Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom,
			false))
		throw (PROGRAM_ERROR)14;
	if (!CreateOffscreenSurface(&Surface[STRETCH_SURFACE], Rect[STRETCH_SURFACE].right,
			Rect[STRETCH_SURFACE].bottom, false))
		throw (PROGRAM_ERROR)15;
	if (!CreateOffscreenSurface(&Surface[CACHE_SURFACE], Rect[CACHE_SURFACE].right,
			Rect[CACHE_SURFACE].bottom, false))
		throw (PROGRAM_ERROR)16;
	if (!CreateOffscreenSurface(&Surface[AUTOMAP_SURFACE], Rect[AUTOMAP_SURFACE].right,
			Rect[AUTOMAP_SURFACE].bottom, false))
		throw (PROGRAM_ERROR)17;
	for (int LoopIndex = 0; LoopIndex < NUMBER_OF_SURFACES - SPRITE_SURFACE; LoopIndex++) {
		if (LoopIndex < 8) {
			if (!CreateOffscreenSurface(&Surface[SPRITE_SURFACE + LoopIndex], Rect[SPRITE_SURFACE + LoopIndex].right,
					Rect[SPRITE_SURFACE + LoopIndex].bottom, false))
				throw (PROGRAM_ERROR)18;
		} else if (!CreateOffscreenSurface(&Surface[SPRITE_SURFACE + LoopIndex], Rect[SPRITE_SURFACE + LoopIndex].right,
					   Rect[SPRITE_SURFACE + LoopIndex].bottom, true)) {
			break;
		}
		CacheSurfaces++;
	}
	Unavailable = false;
	Recreated = true;
}

// 0x4efb70, line 1411
void TSurfaceDX::GetSurfaceRect(int Surface, TRect *Rect)
{
	TASSERTB1(1411, In(Surface,0,NUMBER_OF_SURFACES-1), Surface, "[bug0000290]");
	Rect->x = this->Rect[Surface].left;
	Rect->y = this->Rect[Surface].top;
	Rect->width = this->Rect[Surface].right - this->Rect[Surface].left;
	Rect->height = this->Rect[Surface].bottom - this->Rect[Surface].top;
}

// The slots whose code the linker took from another object (identical
// copies, see SurfaceDX.h); their own copies are not in the image.

// The binary's copy is SurfaceGL.cpp's 0x4fa2c0. Frames narrower or lower
// than two lines are filled.
void TSurfaceDX::DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color, TRect Clip)
{
	if (Rect.width < 2 * LineWidth || Rect.height < 2 * LineWidth) {
		FillRectangleClipped(Surface, Rect, Color, Clip);
		return;
	}
	FillRectangleClipped(Surface, TRect(Rect.x, Rect.y, Rect.width - LineWidth, LineWidth), Color,
		Clip);
	FillRectangleClipped(Surface,
		TRect(Rect.x + LineWidth, Rect.y + Rect.height - LineWidth,
			Rect.width - LineWidth, LineWidth),
		Color, Clip);
	FillRectangleClipped(Surface,
		TRect(Rect.x, Rect.y + LineWidth, LineWidth,
			Rect.height - LineWidth),
		Color, Clip);
	FillRectangleClipped(Surface,
		TRect(Rect.x + Rect.width - LineWidth, Rect.y, LineWidth,
			Rect.height - LineWidth),
		Color, Clip);
}

// SurfaceGL.cpp's 0x4fa110
void TSurfaceDX::DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color)
{
	if (Rect.width < 2 * LineWidth || Rect.height < 2 * LineWidth) {
		FillRectangle(Surface, Rect, Color);
		return;
	}
	FillRectangle(Surface, TRect(Rect.x, Rect.y, Rect.width - LineWidth, LineWidth), Color);
	FillRectangle(Surface,
		TRect(Rect.x + LineWidth, Rect.y + Rect.height - LineWidth,
			Rect.width - LineWidth, LineWidth),
		Color);
	FillRectangle(Surface, TRect(Rect.x, Rect.y + LineWidth, LineWidth, Rect.height - LineWidth),
		Color);
	FillRectangle(Surface,
		TRect(Rect.x + Rect.width - LineWidth, Rect.y, LineWidth,
			Rect.height - LineWidth),
		Color);
}

// surfaceDX9.cpp's 0x4f0410
bool TSurfaceDX::ClipRect(TRect Clip, RECT *Destination, RECT *Source)
{
	const int Right = Clip.x + Clip.width;
	const int Bottom = Clip.y + Clip.height;
	if (Destination->left >= Right || Destination->top >= Bottom || Destination->right <= Clip.x
		|| Destination->bottom <= Clip.y)
		return false;
	if (Destination->left < Clip.x) {
		Source->left += Clip.x - Destination->left;
		Destination->left = Clip.x;
	}
	if (Destination->top < Clip.y) {
		Source->top += Clip.y - Destination->top;
		Destination->top = Clip.y;
	}
	if (Destination->right > Right) {
		Source->right += Right - Destination->right;
		Destination->right = Right;
	}
	if (Destination->bottom > Bottom) {
		Source->bottom += Bottom - Destination->bottom;
		Destination->bottom = Bottom;
	}
	return true;
}

// surfaceDX9.cpp's 0x4f04d0. FrameTime is 1000.0 / 0xffff.
void TSurfaceDX::ResetFrameTimer()
{
	const double Now = PATELLA_TIME_GET_TIME();
	StartTime = Now;
	Framerate = 0xffff;
	NextFrame = Now;
	FPS = 0.0;
	FrameTime = 1000.0 / 0xffff;
}

// surfaceDX9.cpp's 0x4f13e0
bool TSurfaceDX::SurfaceRecreated([[maybe_unused]] int Surface)
{
	return Recreated;
}

// SurfaceGL.cpp's 0x4fb5e0
int TSurfaceDX::GetCacheSurfaces()
{
	return CacheSurfaces;
}

// SurfaceGL.cpp's 0x4fb5f0
bool TSurfaceDX::IsUnavailable()
{
	return Unavailable;
}

// SurfaceGL.cpp's 0x4f9d00
double TSurfaceDX::GetFPS()
{
	return FPS;
}

// 0x4d7d40 (folded with the STL's codecvt::do_always_noconv): unlike
// TSurface's, it does not take the main window.
bool TSurfaceDX::TestEngine()
{
	return true;
}
