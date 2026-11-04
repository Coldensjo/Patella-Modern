// SurfaceGL.h: the OpenGL surface (curEngine 1).
//
// Reconstructed from Tibia.exe 7.72; SurfaceGL.obj is 0x4f46f0-0x4fcb8f
// (notes/decisions.md, "SurfaceGL.cpp"): NVIDIA's glh_extensions.h code
// first, then this file. Names from the binary: the file, glh_extensions.h,
// NUMBER_OF_SURFACES_GL, Type (SurfaceRecreated), FromRect and the surface
// names (assertions); the rest are ours.
//
// Every surface except SCREEN_SURFACE (the window's back buffer) is a texture
// whose size is rounded up to powers of two. Drawing into a texture surface
// goes through a 640x480 pbuffer (WGL_ARB_pbuffer) that shares its textures
// with the window's context: selecting a surface draws its texture into the
// pbuffer, and the part drawn since then (the dirty rectangle) is copied back
// into the texture when another surface is selected. OpenGL is
// opengl32.dll and glu32.dll, imported; the extension entry points come from
// wglGetProcAddress (glh_extensions.h).

#pragma once

#include "Surface.h"
#include "graphics.h"
#include <GL/gl.h>
#include <type_traits>
#include "common.h"

// The surfaces (Textures[] and Rect[] are indexed with them) are Surface.h's
// PRIMARY_SURFACE (no texture), SCREEN_SURFACE (the back buffer, the
// window's client area), TEMP_SURFACE (640x480), STRETCH_SURFACE (64x64),
// CACHE_SURFACE (512x384), AUTOMAP_SURFACE (106x106) and SPRITE_SURFACE
// (6-21: the video cache surfaces, CacheSurfaceWidth x CacheSurfaceHeight).
enum : int {
	NUMBER_OF_SURFACES_GL = 22
};
static_assert(sizeof(decltype(NUMBER_OF_SURFACES_GL)) == 4);
static_assert(std::is_same_v<std::underlying_type_t<decltype(NUMBER_OF_SURFACES_GL)>, int>);

// The texture of one surface (0x20 bytes; the constructor is inline in
// TSurfaceGL's).
struct TTextureGL {
	TTextureGL()
	{
		Texture = 0;
		Width = 0;
		Height = 0;
		TexWidth = 0;
		TexHeight = 0;
		ColorKey = TRGBColor(1, 1, 1);
	}

	GLuint Texture;     // +0x00, the texture name; 0 none
	int Width;          // +0x04, the surface's size
	int Height;         // +0x08
	int TexWidth;       // +0x0c, the texture's size: the next powers of two
	int TexHeight;      // +0x10
	TRGBColor ColorKey; // +0x14, SetSourceColorKey; pixels of this colour become transparent
};

// 0x498 bytes, vtable 0x573250.
class TSurfaceGL : public TSurface {
public:
	TSurfaceGL();           // 0x4fb670
	~TSurfaceGL() override; // 0x4f9860

	void SelectSurface(int Surface) override;                                                            // +0x04, 0x4f9890
	void SetTextureFilter(int Filter) override;                                                          // +0x08, 0x4f9bd0
	void SetPixel(int Surface, int x, int y, TRGBColor Color) override;                                  // +0x0c, 0x4f9dc0
	void FillRectangleClipped(int Surface, TRect Rect, TRGBColor Color, TRect Clip) override;            // +0x10, 0x4f9ef0
	void FillRectangle(int Surface, TRect Rect, TRGBColor Color) override;                               // +0x14, 0x4f9e50
	void DrawHorizontalLine(int Surface, int x, int y, int Width, TRGBColor Color) override;             // +0x18, 0x4f9ff0
	void DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor Color) override;              // +0x1c, 0x4fa080
	void DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color, TRect Clip) override; // +0x20, 0x4fa2c0
	void DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color) override;                    // +0x24, 0x4fa110
	void WriteImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data) override;           // +0x28, 0x4fb7f0
	void WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data, uint8 *Mask,
		TRGBColor *Colors) override;                                                                         // +0x2c, 0x4fbbb0
	void BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect) override;                    // +0x30, 0x4fc6a0
	void Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey) override;    // +0x34, 0x4fc480
	void BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip) override; // +0x38, 0x4fc130
	void BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRect Clip) override;                                                                       // +0x3c, 0x4fab70
	void StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source) override; // +0x40, 0x4fa700
	void StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight) override; // +0x44, 0x4fa510
	void StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight, TRect Clip) override;                                           // +0x48, 0x4fa8b0
	void ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors) override;  // +0x4c, 0x4fabd0
	void SetSourceColorKey(int Surface, TRGBColor ColorKey) override;                        // +0x54, 0x4f9c10
	bool ClipRect(TRect Clip, RECT *Rect, RECT *Source) override;                            // +0x58 (copy 0x4f0410)
	void ResetFrameTimer() override;                                                         // +0x5c (copy 0x4f04d0)
	void WaitForNextFrame() override;                                                        // +0x60, 0x4f9c40
	void UpdateWindowRect() override;                                                        // +0x68, 0x4f9d10
	void InitDisplay(int VMode, TFullscreenMode *GMode) override;                            // +0x6c, 0x4fada0
	void ExitDisplay() override;                                                             // +0x70, 0x4fb030
	void CreateSurfaces() override;                                                          // +0x74, 0x4fb070
	void ResetDisplay(int VMode, TFullscreenMode *GMode) override;                           // +0x78, 0x4fb1f0
	void RestoreSurfaces(int ViewMode, TFullscreenMode *Mode) override;                      // +0x7c, 0x4fb280
	void AddDirtyRect(TRect Rect) override;                                                  // +0x80, 0x4f9b80
	void FinishFrame() override;                                                             // +0x84, 0x4fad20
	bool Init(HWND *Window, int ViewMode, TFullscreenMode *Resolution, bool Force) override; // +0x88, 0x4fb2e0
	int GetFullscreenModes(TFullscreenMode **Modes) override;                                // +0x8c, 0x4fb4e0
	int GetCacheSurfaces() override;                                                         // +0x90, 0x4fb5e0
	void GetSurfaceRect(int Surface, TRect *Rect) override;                                  // +0x94, 0x4fc8a0
	bool SurfaceRecreated(int Type) override;                                                // +0x9c, 0x4fca30
	bool IsUnavailable() override;                                                           // +0xa0, 0x4fb5f0
	bool CanShadeRectangles() override;                                                      // +0xa4 (copy 0x4d7d40)
	int GetSurfaceMemory(int Index) override;                                                // +0xa8, 0x4f9880
	void SetFramerateLimit(int Limit) override;                                              // +0xac (copy 0x4eb000)
	double GetFPS() override;                                                                // +0xb0, 0x4f9d00
	void InitSurfaces() override;                                                            // +0xb4, 0x4fb700
	void ExitSurfaces() override;                                                            // +0xb8, 0x4fb600
	bool TestEngine() override;                                                              // +0xbc (copy 0x4d7d40)

	bool CreateTexture(TTextureGL *Texture, int Width, int Height, bool Empty); // 0x4faef0

	HGLRC Context;                              // +0x70, the window's rendering context
	TTextureGL Textures[NUMBER_OF_SURFACES_GL]; // +0x74
	RECT Rect[NUMBER_OF_SURFACES_GL];           // +0x334, the surfaces' areas (Rect[SCREEN_SURFACE] the client area)
	bool Recreated[3];                          // +0x494, by Type: set by CreateSurfaces, read and cleared by SurfaceRecreated
};

// 0x4f9710. Surface.cpp's engine selection: OpenGL can draw on the window's
// device context with a pbuffer.
bool IsOpenGLWorking(HDC Context);
