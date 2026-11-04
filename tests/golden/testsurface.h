// testsurface.h: the tests' recording surface.
//
// The window and graphics tests draw through a surface object whose slots the
// tests define themselves: graphicstest.cpp defines the constructor and the
// slots graphics.cpp calls, mapwindowtest.cpp those MapWindow.cpp calls, and
// the window tests derive their own recording surfaces from it. The other
// slots are never called (they abort). Until SurfaceGL.cpp was reconstructed
// (issue #11) this was external.h's sketch of TSurfaceGL, with the stubs in
// playertest.cpp; the same members and size (0x498 bytes), now a class of the
// tests only, since difftest.exe links the real TSurfaceGL.

#pragma once

#include "Surface.h"
#include "graphics.h"
#include <stdlib.h>

class TTestSurface : public TSurface {
public:
	TTestSurface(); // graphicstest.cpp

	// graphicstest.cpp
	void WriteImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data);
	void WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data, uint8 *Mask,
		TRGBColor *Colors);
	void Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey);
	void BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRect Clip);
	void StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
		TRGBColor Highlight, TRect Clip);
	void SetSourceColorKey(int Surface, TRGBColor ColorKey);
	int GetCacheSurfaces();
	void GetSurfaceRect(int Surface, TRect *Rect);

	// mapwindowtest.cpp
	void FillRectangle(int Surface, TRect Rect, TRGBColor Color);
	void DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color, TRect Clip);
	void BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect);
	void StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source);
	void ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors);
	bool SurfaceRecreated(int Surface);
	bool CanShadeRectangles();

	// Never called.
	void SetPixel(int, int, int, TRGBColor) { abort(); }
	void FillRectangleClipped(int, TRect, TRGBColor, TRect) { abort(); }
	void DrawHorizontalLine(int, int, int, int, TRGBColor) { abort(); }
	void DrawVerticalLine(int, int, int, int, TRGBColor) { abort(); }
	void DrawFrame(int, TRect, int, TRGBColor) { abort(); }
	void BlitRectClipped(int, int, int, int, TRect, TRect) { abort(); }
	void StretchBlitUnclipped(int, TRect, int, TRect, TRGBColor, TRGBColor) { abort(); }
	bool ClipRect(TRect, RECT *, RECT *) { abort(); }
	void ResetFrameTimer() { abort(); }
	void WaitForNextFrame() { abort(); }
	void UpdateWindowRect() { abort(); }
	void InitDisplay(int, TFullscreenMode *) { abort(); }
	void ExitDisplay() { abort(); }
	void CreateSurfaces() { abort(); }
	void ResetDisplay(int, TFullscreenMode *) { abort(); }
	void RestoreSurfaces(int, TFullscreenMode *) { abort(); }
	void FinishFrame() { abort(); }
	bool Init(HWND *, int, TFullscreenMode *, bool) { abort(); }
	int GetFullscreenModes(TFullscreenMode **) { abort(); }
	bool IsUnavailable() { abort(); }
	int GetSurfaceMemory(int) { abort(); }
	void SetFramerateLimit(int) { abort(); }
	double GetFPS() { abort(); }
	void InitSurfaces() { abort(); }
	void ExitSurfaces() { abort(); }

	uint8 DataGL[0x498 - 0x70];
};
