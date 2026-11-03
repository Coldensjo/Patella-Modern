// SurfaceDX.h: the DirectX 5 engine (engine 0), drawing through DirectDraw.
//
// Despite the engine's name the code uses the DirectX 6 interfaces:
// IDirectDraw4 (QueryInterface with IID_IDirectDraw4), IDirectDrawSurface4
// (DDSURFACEDESC2, 0x7c bytes), and IDirect3D3 / IDirect3DDevice3 /
// IDirect3DViewport3 for the shading of light rectangles (DrawPrimitive with
// D3DFVF_TLVERTEX). The class name TSurfaceDX is ours (after the file name);
// the member names Surface, Rect, DirectDraw and Direct3D are the binary's
// (assertions), the others are ours.

#pragma once

#include "Surface.h"
#include "graphics.h"
// The includer defines DIRECT3D_VERSION 0x0700 before its first include (d3d.h's default, which
// real MFC's d3d9types.h would change to 0x0900; see SurfaceDX.cpp).
#if defined(DIRECT3D_VERSION) && DIRECT3D_VERSION != 0x0700
#error define DIRECT3D_VERSION 0x0700 before the first include
#endif
#define D3D_OVERLOADS
#include <ddraw.h>
#include <d3d.h>

// The vtable is at 0x56f038. No constructor is in the binary: the factory
// CreateSurfaceDX allocates 0x250 bytes, runs TSurface's constructor and
// sets the vtable, so the constructor is inline and initialises nothing;
// InitSurfaces sets the members. The members are public so that tests can
// put fake COM objects into Surface[] and DirectDraw.
//
// Several slots have the code of another object, which the linker kept for
// identical copies: DrawFrameClipped and DrawFrame (SurfaceGL.cpp's 0x4fa2c0,
// 0x4fa110), ClipRect and ResetFrameTimer (surfaceDX9.cpp's 0x4f0410,
// 0x4f04d0), SurfaceRecreated (surfaceDX9.cpp's 0x4f13e0), GetCacheSurfaces,
// IsUnavailable and GetFPS (SurfaceGL.cpp's 0x4fb5e0, 0x4fb5f0, 0x4f9d00);
// TestEngine is a "return true" folded into the STL's
// codecvt::do_always_noconv (0x4d7d40). They are defined in SurfaceDX.cpp.
class TSurfaceDX : public TSurface {
public:
	// User-provided on purpose (issue #34): "= default" here would make TSurfaceDX() and TSurfaceDX{}
	// zero the 0x250 bytes before the constructors run (value-initialization of a class whose default
	// constructor is not user-provided).
	TSurfaceDX() {}         // NOLINT(modernize-use-equals-default)
	~TSurfaceDX() override; // 0x4eae90 (deleting 0x4eca60)

	void SetPixel(int Surface, int x, int y, TRGBColor Color) override;                                  // 0x4eb160
	void FillRectangleClipped(int Surface, TRect Rect, TRGBColor Color, TRect Clip) override;            // 0x4eb3f0
	void FillRectangle(int Surface, TRect Rect, TRGBColor Color) override;                               // 0x4eb2f0
	void DrawHorizontalLine(int Surface, int x, int y, int Width, TRGBColor Color) override;             // 0x4eb540
	void DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor Color) override;              // 0x4eb630
	void DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color, TRect Clip) override; // 0x4fa2c0
	void DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color) override;                    // 0x4fa110
	void WriteImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data) override;           // 0x4eb720
	void WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data,
		uint8 *Mask, TRGBColor *Colors) override;                                                            // 0x4eb900
	void BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect) override;                    // 0x4ee630
	void Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey) override;    // 0x4ee410
	void BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip) override; // 0x4ebe60
	void BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRect Clip) override;                                                                       // 0x4ebd20
	void StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source) override; // 0x4ebc30
	void StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight) override; // 0x4ed520
	void StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight, TRect Clip) override;                                           // 0x4edc70
	void ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors) override;  // 0x4ebf80
	void SetSourceColorKey(int Surface, TRGBColor ColorKey) override;                        // 0x4eafb0
	bool ClipRect(TRect Clip, RECT *Destination, RECT *Source) override;                     // 0x4f0410
	void ResetFrameTimer() override;                                                         // 0x4f04d0
	void WaitForNextFrame() override;                                                        // 0x4eb040
	void AttachWindow() override;                                                            // 0x4ed1a0
	void UpdateWindowRect() override;                                                        // 0x4eb100
	void InitDisplay(int VMode, TFullscreenMode *GMode) override;                            // 0x4ee830
	void ExitDisplay() override;                                                             // 0x4ec540
	void CreateSurfaces() override;                                                          // 0x4eed40
	void ResetDisplay(int VMode, TFullscreenMode *GMode) override;                           // 0x4ec600
	void RestoreSurfaces(int ViewMode, TFullscreenMode *Mode) override;                      // 0x4ec690
	void FinishFrame() override;                                                             // 0x4ec220
	bool Init(HWND *Window, int ViewMode, TFullscreenMode *Resolution, bool Force) override; // 0x4ec700
	int GetFullscreenModes(TFullscreenMode **Modes) override;                                // 0x4ec980
	int GetCacheSurfaces() override;                                                         // 0x4fb5e0
	void GetSurfaceRect(int Surface, TRect *Rect) override;                                  // 0x4efb70
	bool SurfaceRecreated(int Surface) override;                                             // 0x4f13e0
	bool IsUnavailable() override;                                                           // 0x4fb5f0
	bool CanShadeRectangles() override;                                                      // 0x4ec9d0
	int GetSurfaceMemory(int Index) override;                                                // 0x4eae80
	void SetFramerateLimit(int Framerate) override;                                          // 0x4eb000
	double GetFPS() override;                                                                // 0x4f9d00
	void InitSurfaces() override;                                                            // 0x4ecfd0
	void ExitSurfaces() override;                                                            // 0x4ec9e0
	bool TestEngine() override;                                                              // 0x4d7d40

	// Not virtual.
	uint32 GetPixelColor(IDirectDrawSurface4 *DDSurface, TRGBColor Color); // 0x4eaeb0
	bool CreateOffscreenSurface(IDirectDrawSurface4 **Result, int Width, int Height,
		bool VideoMemoryOnly); // 0x4ec3a0
	bool Create3DSurface(IDirectDrawSurface4 **Result, int Width, int Height,
		bool VideoMemoryOnly); // 0x4ec470
	void ShadeRectangle(int Surface, TRect Rect, TRGBColor TopLeft, TRGBColor TopRight,
		TRGBColor BottomLeft, TRGBColor BottomRight); // 0x4eca80
	bool TestShading(int Surface);                    // 0x4ecdd0

	bool Recreated;                                   // +0x70, set by CreateSurfaces, cleared by Init
	IDirectDraw4 *DirectDraw;                         // +0x74
	RECT WindowRect;                                  // +0x78, the client area in screen coordinates (windowed)
	IDirectDrawSurface4 *Surface[NUMBER_OF_SURFACES]; // +0x88
	RECT Rect[NUMBER_OF_SURFACES];                    // +0xe0, the size of each surface
	IDirect3D3 *Direct3D;                             // +0x240, NULL when the shading does not work
	IDirect3DDevice3 *Device;                         // +0x244
	IDirect3DViewport3 *Viewport;                     // +0x248
	int Data24C;                                      // +0x24c
};

TSurface *CreateSurfaceDX(); // 0x4eca00
