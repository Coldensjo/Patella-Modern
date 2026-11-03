// surfaceDX9.h: the Direct3D 9 surface (curEngine 2).
//
// Reconstructed from Tibia.exe 7.72; surfaceDX9.obj is 0x4efd10-0x4f46ef
// (notes/decisions.md, "surfaceDX9.cpp"). Names from the binary: the file,
// Direct3D, DirectDraw, Surface9, Rect, hWindow, ViewMode, VMode, GMode,
// FromRect, ToRect, NUMBER_OF_SURFACES_DX9 and the surface names
// (assertions); the rest are ours.
//
// d3d9.dll is not in the import table: IsDirect3D9Installed loads it and
// Direct3DCreate9 at run time. DirectDraw 4 (ddraw.dll, imported) is used
// only for the display modes, the stretch capabilities and the desktop
// depth. The interfaces are the Windows SDK's d3d9.h and ddraw.h.

#pragma once

#include "Surface.h"
#include "graphics.h"
#include <d3d9.h>
#include <ddraw.h>

// The surfaces (Surface9[] and Rect[] are indexed with them) are Surface.h's
// PRIMARY_SURFACE (the main render target, Rect[TEMP_SURFACE]'s size),
// SCREEN_SURFACE (the back buffer; its texture is the 106x106 ScreenTexture),
// TEMP_SURFACE (640x480), STRETCH_SURFACE (64x64), CACHE_SURFACE (512x384),
// AUTOMAP_SURFACE (106x106) and SPRITE_SURFACE (6-21: the video cache
// surfaces, CacheSurfaceWidth x CacheSurfaceHeight); the values follow from
// this file's comparisons (ToSurface!=SCREEN_SURFACE compares with 1).
enum {
	NUMBER_OF_SURFACES_DX9 = 22
};

// One vertex of the vertex buffers (D3DFVF_XYZRHW | D3DFVF_DIFFUSE |
// D3DFVF_TEX1, 0x1c bytes).
struct TVertexDX9 {
	float x, y, z, rhw;
	D3DCOLOR Color;
	float u, v;
};

// 0x2c0 bytes, vtable 0x56f480 (after SurfaceDX.cpp's TSurfaceDX).
class TSurfaceDX9 : public TSurface {
public:
	TSurfaceDX9();           // inline, in CreateSurfaceDX9
	~TSurfaceDX9() override; // 0x4f03f0

	void SetPixel(int Surface, int x, int y, TRGBColor Color) override;                                  // +0x0c, 0x4f0630
	void FillRectangleClipped(int Surface, TRect Rect, TRGBColor Color, TRect Clip) override;            // +0x10, 0x4f0770
	void FillRectangle(int Surface, TRect Rect, TRGBColor Color) override;                               // +0x14, 0x4f06b0
	void DrawHorizontalLine(int Surface, int x, int y, int Width, TRGBColor Color) override;             // +0x18, 0x4f0870
	void DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor Color) override;              // +0x1c, 0x4f0920
	void DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color, TRect Clip) override; // +0x20 (copy 0x4fa2c0)
	void DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color) override;                    // +0x24 (copy 0x4fa110)
	void WriteImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data) override;           // +0x28, 0x4f09d0
	void WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data, uint8 *Mask,
		TRGBColor *Colors) override;                                                                         // +0x2c, 0x4f2040
	void BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect) override;                    // +0x30, 0x4f3040
	void Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey) override;    // +0x34, 0x4f2e20
	void BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip) override; // +0x38, 0x4f1b70
	void BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRect Clip) override;                                                                       // +0x3c (copy 0x4fab70)
	void StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source) override; // +0x40, 0x4f1a40
	void StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight) override; // +0x44, 0x4f18d0
	void StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight, TRect Clip) override;                                           // +0x48, 0x4f2620
	void ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors) override;  // +0x4c, 0x4f0e50
	void SetSourceColorKey(int ToSurface, TRGBColor ColorKey) override;                      // +0x54, 0x4f1eb0
	bool ClipRect(TRect Clip, RECT *Rect, RECT *Source) override;                            // +0x58, 0x4f0410
	void ResetFrameTimer() override;                                                         // +0x5c, 0x4f04d0
	void WaitForNextFrame() override;                                                        // +0x60, 0x4f0510
	void UpdateWindowRect() override;                                                        // +0x68, 0x4f05d0
	void InitDisplay(int VMode, TFullscreenMode *GMode) override;                            // +0x6c, 0x4f3240
	void ExitDisplay() override;                                                             // +0x70, 0x4f1040
	void CreateSurfaces() override;                                                          // +0x74, 0x4f3740
	void ResetDisplay(int VMode, TFullscreenMode *GMode) override;                           // +0x78, 0x4f1160
	void RestoreSurfaces(int ViewMode, TFullscreenMode *Resolution) override;                // +0x7c, 0x4f11f0
	void FinishFrame() override;                                                             // +0x84, 0x4f0f80
	bool Init(HWND *Window, int ViewMode, TFullscreenMode *Resolution, bool Force) override; // +0x88, 0x4f1240
	int GetFullscreenModes(TFullscreenMode **Modes) override;                                // +0x8c, 0x4f1320
	int GetCacheSurfaces() override;                                                         // +0x90 (copy 0x4fb5e0)
	void GetSurfaceRect(int Surface, TRect *Rect) override;                                  // +0x94, 0x4f4560
	void SetSurfaceRect(int Surface, int x, int y, int Width, int Height) override;          // +0x98, 0x4f1370
	bool SurfaceRecreated(int Surface) override;                                             // +0x9c, 0x4f13e0
	bool IsUnavailable() override;                                                           // +0xa0 (copy 0x4fb5f0)
	bool CanShadeRectangles() override;                                                      // +0xa4, 0x4f13f0
	int GetSurfaceMemory(int Surface) override;                                              // +0xa8, 0x4f03e0
	void SetFramerateLimit(int Limit) override;                                              // +0xac (copy 0x4eb000)
	double GetFPS() override;                                                                // +0xb0 (copy 0x4f9d00)
	void InitSurfaces() override;                                                            // +0xb4, 0x4f1cd0
	void ExitSurfaces() override;                                                            // +0xb8, 0x4f1400
	bool TestEngine() override;                                                              // +0xbc, 0x4efd40

	void AddShadeQuad(int Left, int Top, int Right, int Bottom, D3DCOLOR TopLeft, D3DCOLOR TopRight,
		D3DCOLOR BottomRight, D3DCOLOR BottomLeft);      // 0x4efe70
	void FlushVertices();                                // 0x4f02a0
	void SetTexture(int Surface);                        // 0x4f0330
	void SetRenderTarget(int Surface);                   // 0x4f0380
	void ApplyColorKey(int Surface, TRGBColor ColorKey); // 0x4f0c60
	void FlushShade();                                   // 0x4f0d50
	void AddQuad(int Left, int Top, int Right, int Bottom, double Left_u, double Right_u, double Top_v,
		double Bottom_v, D3DCOLOR TopLeft, D3DCOLOR TopRight, D3DCOLOR BottomRight, D3DCOLOR BottomLeft); // 0x4f14c0

	bool Ready;                                              // +0x70, set when CreateSurfaces has finished (SurfaceRecreated)
	RECT Rect[NUMBER_OF_SURFACES_DX9];                       // +0x74
	RECT WindowRect;                                         // +0x1d4, the client area on the screen
	RECT SurfaceArea;                                        // +0x1e4, SetSurfaceRect
	IDirect3D9 *Direct3D;                                    // +0x1f4
	IDirect3DDevice9 *Device;                                // +0x1f8
	IDirectDraw4 *DirectDraw;                                // +0x1fc, DirectDraw 4 (IID_IDirectDraw4)
	IDirect3DTexture9 *Surface9[NUMBER_OF_SURFACES_DX9];     // +0x200
	IDirect3DSurface9 *RenderTarget[NUMBER_OF_SURFACES_DX9]; // +0x258, level 0 of Surface9[] (SCREEN_SURFACE: the back buffer)
	IDirect3DSurface9 *CurrentTarget;                        // +0x2b0
	IDirect3DVertexBuffer9 *VertexBuffer;                    // +0x2b4, the quads (Vertices)
	IDirect3DVertexBuffer9 *ShadeBuffer;                     // +0x2b8, the shaded rectangles (ShadeVertices)
	IDirect3DTexture9 *ScreenTexture;                        // +0x2bc, the texture of SCREEN_SURFACE
};

// IsDirect3D9Installed (0x4efd10) and CreateSurfaceDX9 (0x4f1430) are declared in Surface.h.
