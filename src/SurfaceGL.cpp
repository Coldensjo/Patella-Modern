// SurfaceGL.cpp: reconstructed from Tibia.exe 7.72 (0x4f9550-0x4fcb8f, after
// the glh_extensions.h code 0x4f46f0-0x4f954f of the same object).
//
// The OpenGL surface. SCREEN_SURFACE is the window's back buffer; the other
// surfaces are textures. Drawing into a texture surface selects it
// (SelectSurface): the 640x480 pbuffer's context becomes current, the
// texture is drawn into the pbuffer, and every drawing call adds its area to
// the dirty rectangle, which is copied back into the texture
// (glCopyTexSubImage2D, the whole texture size from the origin) when another
// surface is selected. Rectangles, lines and pixels are untextured
// primitives, blits textured quads.
//
// Only the functions with assertions have line numbers (767-1555); they are
// in binary order, so the file is in binary order. Six vtable slots have no
// code here: the linker kept an identical copy from another object
// (surfaceDX9.obj's ClipRect and ResetFrameTimer, SurfaceDX.obj's
// SetFramerateLimit, and the return-true body 0x4d7d40 for TestEngine and
// CanShadeRectangles); they are at the end. The other way round, the DirectX
// engines' vtables use six of this file's functions (GetFPS, DrawFrame,
// DrawFrameClipped, BlitClipped, GetCacheSurfaces, IsUnavailable).

#include "common.h"
#define GLH_EXT_SINGLE_FILE
#include "Extern/glh/glh_extensions.h"
#include "Extern/glh/glh_genext.h"
#include "SurfaceGL.h"
#include "GUI.h"
#include "MainFrm.h"
#include "Preferences.h"
#include "Surface.h"
#include "graphics.h"
#include <GL/glu.h>
#include <mmsystem.h>
#include <cstring>

// Not in the Windows SDK's OpenGL 1.1 headers (glext.h, wglext.h).
#define GL_CLAMP_TO_EDGE 0x812F
#define WGL_DRAW_TO_PBUFFER_ARB 0x202D
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_ACCELERATION_ARB 0x2003
#define WGL_FULL_ACCELERATION_ARB 0x2027
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_ALPHA_BITS_ARB 0x201B

// Data, in address order (.data, then .bss; the glh_extensions.h pointers
// lie between 0x71d318 and 0x71dbb0).
static int CurrentFilter = GL_LINEAR;             // 0x5c257c, the textures' filter (SetTextureFilter)
static int CurrentSurface = SCREEN_SURFACE;       // 0x5c2580, the selected surface
static RECT DirtyRect;                            // 0x71d290, drawn into CurrentSurface since it was selected
static HPBUFFERARB Pbuffer;                       // 0x71d2a0
static HDC PbufferDC;                             // 0x71d2a4
static HGLRC PbufferContext;                      // 0x71d2a8
static int PbufferWidth;                          // 0x71d2ac
static int PbufferHeight;                         // 0x71d2b0
static HGLRC MainContext;                         // 0x71d2b4, the window's context
static HDC MainDC;                                // 0x71d2b8, the window's device context
static int SurfaceCreated[NUMBER_OF_SURFACES_GL]; // 0x71d2c0, indexed by NumberOfSurfaces
static uint32 FrameCount;                         // 0x71dbb4
static double LastFPSTime;                        // 0x71dbb8

// 0x4f9550. Odd behaviour kept: the imported functions are checked for NULL
// before they are called.
static void ReleasePbuffer()
{
	if (Pbuffer != nullptr) {
		if (wglGetCurrentContext != nullptr && wglGetCurrentContext() == PbufferContext)
			wglMakeCurrent(MainDC, MainContext);
		if (wglDeleteContext != nullptr)
			wglDeleteContext(PbufferContext);
		if (wglReleasePbufferDCARB != nullptr)
			wglReleasePbufferDCARB(Pbuffer, PbufferDC);
		if (wglDestroyPbufferARB != nullptr)
			wglDestroyPbufferARB(Pbuffer);
		Pbuffer = nullptr;
	}
}

// 0x4f95d0. A 640x480 pbuffer with an accelerated 24-bit pixel format and
// alpha, whose context shares the window context's textures. Odd behaviour
// kept: after wglCreateContext the pbuffer is checked again instead of the
// new context.
static bool CreatePbuffer()
{
	Pbuffer = nullptr;
	PbufferWidth = 640;
	PbufferHeight = 480;
	// clang-format off
	int Attributes[] = {
		WGL_DRAW_TO_PBUFFER_ARB, 1,
		WGL_SUPPORT_OPENGL_ARB, 1,
		WGL_DOUBLE_BUFFER_ARB, 0,
		WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB, 24,
		WGL_ALPHA_BITS_ARB, 8,
		0,
	};
	// clang-format on
	int Format;
	UINT Count = 0;
	if (wglChoosePixelFormatARB != nullptr) {
		wglChoosePixelFormatARB(MainDC, Attributes, nullptr, 1, &Format, &Count);
		if (Count != 0 && wglCreatePbufferARB != nullptr) {
			Pbuffer = wglCreatePbufferARB(MainDC, Format, PbufferWidth, PbufferHeight, nullptr);
			if (Pbuffer != nullptr && wglGetPbufferDCARB != nullptr) {
				PbufferDC = wglGetPbufferDCARB(Pbuffer);
				if (PbufferDC != nullptr && wglCreateContext != nullptr) {
					PbufferContext = wglCreateContext(PbufferDC);
					if (Pbuffer != nullptr && wglShareLists != nullptr)
						return wglShareLists(MainContext, PbufferContext) != FALSE;
				}
			}
		}
	}
	return false;
}

// 0x4f9710. Sets a double-buffered RGBA pixel format on the context, needs
// the pbuffer extensions and creates and releases a pbuffer. Odd behaviour
// kept: the paths that return early leave the context current (and the
// extension strings allocated).
bool IsOpenGLWorking(HDC Context)
{
	bool Works = false;
	MainDC = Context;
	// clang-format off
	PIXELFORMATDESCRIPTOR Format = {
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DEPTH_DONTCARE,
		PFD_TYPE_RGBA, 32,
	};
	// clang-format on
	int Number = ChoosePixelFormat(Context, &Format);
	if (Number == 0 || !SetPixelFormat(MainDC, Number, &Format))
		return false;
	MainContext = wglCreateContext(MainDC);
	wglMakeCurrent(MainDC, MainContext);
	if (!glh_init_extensions("WGL_ARB_pbuffer WGL_ARB_pixel_format ")) {
		OutputDebugStringA("Necessary OpenGL extensions were not supported");
		return false;
	}
	try {
		if (CreatePbuffer()) {
			ReleasePbuffer();
			Works = true;
		}
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(MainContext);
		MainDC = nullptr;
		glh_shutdown_extensions();
		return Works;
	} catch (...) {
		return false;
	}
}

// 0x4f9860
TSurfaceGL::~TSurfaceGL()
{
	NumberOfSurfaces = 0;
}

// 0x4f9880
int TSurfaceGL::GetSurfaceMemory(int Index)
{
	return SurfaceCreated[Index];
}

// 0x4f9890. Makes Surface the drawing target. The surface selected before
// gets what was drawn into the pbuffer since it was selected (unless it is
// SCREEN_SURFACE or nothing was drawn); a texture surface is then drawn into
// the pbuffer (whose projection grows with the texture), SCREEN_SURFACE gets
// the window's context and projection. The dirty rectangle starts inverted
// (empty). Each change counts in Data71D0D8.
void TSurfaceGL::SelectSurface(int Surface)
{
	if (CurrentSurface == Surface)
		return;
	Data71D0D8++;
	if (CurrentSurface != SCREEN_SURFACE && DirtyRect.left < DirtyRect.right && DirtyRect.top < DirtyRect.bottom) {
		glBindTexture(GL_TEXTURE_2D, Textures[CurrentSurface].Texture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, Textures[CurrentSurface].TexWidth,
			Textures[CurrentSurface].TexHeight);
	}
	if (Surface == SCREEN_SURFACE) {
		wglMakeCurrent(MainDC, MainContext);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		if (Rect[SCREEN_SURFACE].right == 0)
			GetClientRect(hWindow, &Rect[SCREEN_SURFACE]);
		glViewport(0, 0, Rect[SCREEN_SURFACE].right, Rect[SCREEN_SURFACE].bottom);
		gluOrtho2D(0.0, Rect[SCREEN_SURFACE].right, Rect[SCREEN_SURFACE].bottom, 0.0);
		glDrawBuffer(GL_BACK);
		glReadBuffer(GL_BACK);
	} else {
		if (wglGetCurrentContext() != PbufferContext)
			wglMakeCurrent(PbufferDC, PbufferContext);
		TTextureGL *Texture = &Textures[Surface];
		if (Texture->TexWidth > Textures[CurrentSurface].TexWidth
			|| Texture->TexHeight > Textures[CurrentSurface].TexHeight) {
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glViewport(0, 0, Texture->TexWidth, Texture->TexHeight);
			gluOrtho2D(0.0, Texture->TexWidth, 0.0, Texture->TexHeight);
		}
		glAlphaFunc(GL_GREATER, 0.0f);
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, Texture->Texture);
		glBegin(GL_QUADS);
		glColor3ub(255, 255, 255);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2i(0, 0);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2i(0, Texture->TexHeight);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2i(Texture->TexWidth, Texture->TexHeight);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2i(Texture->TexWidth, 0);
		glEnd();
	}
	DirtyRect.top = Rect[Surface].bottom;
	DirtyRect.left = Rect[Surface].right;
	DirtyRect.bottom = Rect[Surface].top;
	DirtyRect.right = Rect[Surface].left;
	CurrentSurface = Surface;
}

// 0x4f9b80
void TSurfaceGL::AddDirtyRect(TRect Rect)
{
	if (Rect.x < DirtyRect.left)
		DirtyRect.left = Rect.x;
	if (Rect.y < DirtyRect.top)
		DirtyRect.top = Rect.y;
	if (Rect.x + Rect.width > DirtyRect.right)
		DirtyRect.right = Rect.x + Rect.width;
	if (Rect.y + Rect.height > DirtyRect.bottom)
		DirtyRect.bottom = Rect.y + Rect.height;
}

// 0x4f9bd0. The filter of the bound texture, GL_LINEAR or GL_NEAREST (only
// when it differs from the last one set).
void TSurfaceGL::SetTextureFilter(int Filter)
{
	if (Filter != CurrentFilter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Filter);
		CurrentFilter = Filter;
	}
}

// 0x4f9c10
void TSurfaceGL::SetSourceColorKey(int Surface, TRGBColor ColorKey)
{
	Textures[Surface].ColorKey = ColorKey;
}

// 0x4f9c40 (the same code as surfaceDX9.cpp's, with this file's counters).
// Measures the frame rate once a second and sleeps until the next frame is
// due; a frame more than a millisecond late moves the schedule.
void TSurfaceGL::WaitForNextFrame()
{
	double Now = PATELLA_TIME_GET_TIME();
	FrameCount++;
	if (Now - LastFPSTime > 1000.0) {
		FPS = (FrameCount * 1000) / (Now - LastFPSTime);
		FrameCount = 0;
		LastFPSTime = Now;
	}
	NextFrame += FrameTime;
	if (NextFrame > Now)
		PATELLA_FRAME_SLEEP((int)(NextFrame - Now));
	else if (Now - NextFrame > 1.0)
		NextFrame = Now - 1.0;
}

// 0x4f9d00 (the DirectX engines' vtables use it too)
double TSurfaceGL::GetFPS()
{
	return FPS;
}

// 0x4f9d10
void TSurfaceGL::UpdateWindowRect()
{
	GetClientRect(hWindow, &Rect[PRIMARY_SURFACE]);
	GetClientRect(hWindow, &Rect[SCREEN_SURFACE]);
	GetClientRect(hWindow, &ScreenRect);
	ClientToScreen(hWindow, (POINT *)&ScreenRect.left);
	ClientToScreen(hWindow, (POINT *)&ScreenRect.right);
	if (Context != nullptr) {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glViewport(0, 0, Rect[SCREEN_SURFACE].right, Rect[SCREEN_SURFACE].bottom);
		gluOrtho2D(0.0, Rect[SCREEN_SURFACE].right, Rect[SCREEN_SURFACE].bottom, 0.0);
	}
}

// 0x4f9dc0
void TSurfaceGL::SetPixel(int Surface, int x, int y, TRGBColor Color)
{
	SelectSurface(Surface);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_POINTS);
	glColor3ub(Color.R, Color.G, Color.B);
	glVertex2i(x, y);
	glEnd();
	AddDirtyRect(TRect(x, y, 1, 1));
	BlitPixels++;
}

// 0x4f9e50
void TSurfaceGL::FillRectangle(int Surface, TRect Rect, TRGBColor Color)
{
	SelectSurface(Surface);
	glDisable(GL_TEXTURE_2D);
	glColor3ub(Color.R, Color.G, Color.B);
	glRecti(Rect.x, Rect.y, Rect.x + Rect.width, Rect.y + Rect.height);
	BlitCount++;
	BlitPixels += Rect.width * Rect.height;
	AddDirtyRect(Rect);
}

// 0x4f9ef0. Odd behaviour kept: ClipRect's second rectangle is an
// uninitialised local; the statistics and the dirty rectangle use the
// unclipped rectangle.
void TSurfaceGL::FillRectangleClipped(int Surface, TRect Rect, TRGBColor Color, TRect Clip)
{
	SelectSurface(Surface);
	glDisable(GL_TEXTURE_2D);
	RECT Destination;
	Destination.left = Rect.x;
	Destination.top = Rect.y;
	Destination.right = Rect.x + Rect.width;
	Destination.bottom = Rect.y + Rect.height;
	RECT Unused;
	if (ClipRect(Clip, &Destination, &Unused)) {
		glColor3ub(Color.R, Color.G, Color.B);
		glRecti(Destination.left, Destination.top, Destination.right, Destination.bottom);
		BlitPixels += Rect.width * Rect.height;
		BlitCount++;
		AddDirtyRect(Rect);
	}
}

// 0x4f9ff0
void TSurfaceGL::DrawHorizontalLine(int Surface, int x, int y, int Width, TRGBColor Color)
{
	SelectSurface(Surface);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_LINES);
	glColor3ub(Color.R, Color.G, Color.B);
	glVertex2i(x, y);
	glVertex2i(x + Width, y);
	glEnd();
	AddDirtyRect(TRect(x, y, Width, 1));
}

// 0x4fa080
void TSurfaceGL::DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor Color)
{
	SelectSurface(Surface);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_LINES);
	glColor3ub(Color.R, Color.G, Color.B);
	glVertex2i(x, y);
	glVertex2i(x, y + Height);
	glEnd();
	AddDirtyRect(TRect(x, y, 1, Height));
}

// 0x4fa110 (the DirectX engines' vtables use it too). A frame thinner than
// twice the line is filled.
void TSurfaceGL::DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color)
{
	if (Rect.width < 2 * LineWidth || Rect.height < 2 * LineWidth) {
		FillRectangle(Surface, Rect, Color);
		return;
	}
	FillRectangle(Surface, TRect(Rect.x, Rect.y, Rect.width - LineWidth, LineWidth), Color);
	FillRectangle(Surface,
		TRect(Rect.x + LineWidth, Rect.y + Rect.height - LineWidth, Rect.width - LineWidth,
			LineWidth),
		Color);
	FillRectangle(Surface, TRect(Rect.x, Rect.y + LineWidth, LineWidth, Rect.height - LineWidth), Color);
	FillRectangle(Surface, TRect(Rect.x + Rect.width - LineWidth, Rect.y, LineWidth, Rect.height - LineWidth), Color);
}

// 0x4fa2c0 (the DirectX engines' vtables use it too)
void TSurfaceGL::DrawFrameClipped(int Surface, TRect Rect, int LineWidth, TRGBColor Color, TRect Clip)
{
	if (Rect.width < 2 * LineWidth || Rect.height < 2 * LineWidth) {
		FillRectangleClipped(Surface, Rect, Color, Clip);
		return;
	}
	FillRectangleClipped(Surface, TRect(Rect.x, Rect.y, Rect.width - LineWidth, LineWidth), Color, Clip);
	FillRectangleClipped(Surface,
		TRect(Rect.x + LineWidth, Rect.y + Rect.height - LineWidth, Rect.width - LineWidth,
			LineWidth),
		Color, Clip);
	FillRectangleClipped(Surface, TRect(Rect.x, Rect.y + LineWidth, LineWidth, Rect.height - LineWidth), Color, Clip);
	FillRectangleClipped(Surface, TRect(Rect.x + Rect.width - LineWidth, Rect.y, LineWidth, Rect.height - LineWidth),
		Color, Clip);
}

// 0x4fa510. A blit of the same size goes to Blit; otherwise the source is
// drawn scaled with linear filtering. Odd behaviour kept: the scaled path
// ignores both colours.
void TSurfaceGL::StretchBlitUnclipped(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect,
	TRGBColor ColorKey, [[maybe_unused]] TRGBColor Highlight)
{
	if (ToRect.width == FromRect.width && ToRect.height == FromRect.height) {
		Blit(ToSurface, ToRect.x, ToRect.y, FromSurface, FromRect, ColorKey);
		return;
	}
	TTextureGL *Texture = &Textures[FromSurface];
	float Left = (float)((double)FromRect.x / Texture->TexWidth);
	float Right = (float)((double)(FromRect.x + FromRect.width) / Texture->TexWidth);
	float Top = (float)((double)FromRect.y / Texture->TexHeight);
	float Bottom = (float)((double)(FromRect.y + FromRect.height) / Texture->TexHeight);
	SelectSurface(ToSurface);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, Texture->Texture);
	SetTextureFilter(GL_LINEAR);
	glBegin(GL_QUADS);
	glColor3ub(255, 255, 255);
	glTexCoord2f(Left, Top);
	glVertex2i(ToRect.x, ToRect.y);
	glTexCoord2f(Left, Bottom);
	glVertex2i(ToRect.x, ToRect.y + ToRect.height);
	glTexCoord2f(Right, Bottom);
	glVertex2i(ToRect.x + ToRect.width, ToRect.y + ToRect.height);
	glTexCoord2f(Right, Top);
	glVertex2i(ToRect.x + ToRect.width, ToRect.y);
	glEnd();
	BlitCount++;
	BlitPixels += ToRect.height * ToRect.width;
	AddDirtyRect(ToRect);
}

// 0x4fa700. Scaled, with linear filtering.
void TSurfaceGL::StretchBlitRect(int ToSurface, TRect Destination, int FromSurface, TRect Source)
{
	TTextureGL *Texture = &Textures[FromSurface];
	double Left = (double)Source.x / Texture->TexWidth;
	double Right = (double)(Source.x + Source.width) / Texture->TexWidth;
	double Top = (double)Source.y / Texture->TexHeight;
	double Bottom = (double)(Source.y + Source.height) / Texture->TexHeight;
	SelectSurface(ToSurface);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, Texture->Texture);
	SetTextureFilter(GL_LINEAR);
	glBegin(GL_QUADS);
	glColor3ub(255, 255, 255);
	glTexCoord2d(Left, Top);
	glVertex2i(Destination.x, Destination.y);
	glTexCoord2d(Left, Bottom);
	glVertex2i(Destination.x, Destination.y + Destination.height);
	glTexCoord2d(Right, Bottom);
	glVertex2i(Destination.x + Destination.width, Destination.y + Destination.height);
	glTexCoord2d(Right, Top);
	glVertex2i(Destination.x + Destination.width, Destination.y);
	glEnd();
	BlitCount++;
	BlitPixels += Destination.width * Destination.height;
	AddDirtyRect(Destination);
}

// 0x4fa8b0. A scaled blit goes through TEMP_SURFACE (the source with a
// one-pixel border of Highlight, or skin 185 while Flag26 is set) and
// STRETCH_SURFACE, whose pixels within 2 of Highlight in every channel become
// transparent (read back with glGetTexImage; the whole 64x64 texture);
// Highlight is the colour key of the result. Odd behaviour kept: the readback
// selects CACHE_SURFACE first.
void TSurfaceGL::StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
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
		SelectSurface(CACHE_SURFACE);
		uint8 *Pixels = new uint8[64 * 64 * 4];
		glBindTexture(GL_TEXTURE_2D, Textures[STRETCH_SURFACE].Texture);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);
		uint8 *Pixel = Pixels;
		for (int i = 0; i < 64 * 64; i++) {
			if (Pixel[0] >= Highlight.R - 2 && Pixel[0] <= Highlight.R + 2
				&& Pixel[1] >= Highlight.G - 2 && Pixel[1] <= Highlight.G + 2
				&& Pixel[2] >= Highlight.B - 2 && Pixel[2] <= Highlight.B + 2)
				Pixel[3] = 0;
			Pixel += 4;
		}
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);
		delete Pixels;
	}
	BlitClipped(ToSurface, ToRect.x, ToRect.y, STRETCH_SURFACE, TRect(0, 0, ToRect.width, ToRect.height), Highlight,
		Clip);
}

// 0x4fab70 (the DirectX engines' vtables use it too). The colour key is not
// used.
void TSurfaceGL::BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, [[maybe_unused]] TRGBColor ColorKey,
	TRect Clip)
{
	BlitRectClipped(ToSurface, x, y, FromSurface, FromRect, Clip);
}

// 0x4fabd0. Multiplies Surface with Count quads whose corners have the
// colours Colors[4*i] (top left), +1 (top right), +2 (bottom left) and +3
// (bottom right).
void TSurfaceGL::ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors)
{
	SelectSurface(Surface);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	for (int i = 0; i < Count; i++) {
		TRect *R = &Rects[i];
		TRGBColor *C = &Colors[4 * i];
		glColor3ub(C[0].R, C[0].G, C[0].B);
		glVertex2i(R->x, R->y);
		glColor3ub(C[2].R, C[2].G, C[2].B);
		glVertex2i(R->x, R->y + R->height);
		glColor3ub(C[3].R, C[3].G, C[3].B);
		glVertex2i(R->x + R->width, R->y + R->height);
		glColor3ub(C[1].R, C[1].G, C[1].B);
		glVertex2i(R->x + R->width, R->y);
		AddDirtyRect(*R);
	}
	glEnd();
	glDisable(GL_BLEND);
}

// 0x4fad20
void TSurfaceGL::FinishFrame()
{
	if (ViewMode != 0) {
		WaitForNextFrame();
		ScreenWidth = Rect[SCREEN_SURFACE].right;
		ScreenHeight = Rect[SCREEN_SURFACE].bottom;
		ScreenDepth = Depth;
		LastBlitCount = BlitCount;
		LastBlitPixels = BlitPixels;
		Data71D0F4 = Data71D0D8;
		BlitCount = 0;
		BlitPixels = 0;
		Data71D0D8 = 0;
		SwapBuffers(DeviceContext);
	}
}

// 0x4fada0. Windowed mode restores the desktop's display settings and takes
// their size; full-screen mode switches to GMode's size if the display
// lists it with the desktop's depth, otherwise to the last mode listed
// (written back into GMode, without its frequency). Odd behaviour kept: a
// failed switch back to the desktop still sets the view mode without the
// size, a failed full-screen switch changes nothing, and with no modes at
// all it reads before the array.
void TSurfaceGL::InitDisplay(int VMode, TFullscreenMode *GMode)
{
	DEVMODEA Mode;
	memset(&Mode, 0, sizeof(Mode));
	Mode.dmSize = sizeof(DEVMODEA);
	if (VMode == WINDOWED_MODE) {
		if (ChangeDisplaySettingsA(nullptr, 0) == DISP_CHANGE_SUCCESSFUL) {
			EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &Mode);
			Height = Mode.dmPelsHeight;
			Width = Mode.dmPelsWidth;
			Depth = Mode.dmBitsPerPel;
			ViewMode = VMode;
			return;
		}
	} else {
		Mode.dmPelsHeight = GMode->Height;
		Mode.dmBitsPerPel = GMode->Depth;
		Mode.dmPelsWidth = GMode->Width;
		Mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		TFullscreenMode *Modes = nullptr;
		int Number = GetFullscreenModes(&Modes);
		int i;
		for (i = 0; i < Number; i++) {
			if (GMode->Width == Modes[i].Width && GMode->Height == Modes[i].Height)
				break;
		}
		if (i >= Number) {
			Mode.dmBitsPerPel = Modes[Number - 1].Depth;
			Mode.dmPelsWidth = Modes[Number - 1].Width;
			Mode.dmPelsHeight = Modes[Number - 1].Height;
			GMode->Depth = Modes[Number - 1].Depth;
			GMode->Width = Modes[Number - 1].Width;
			GMode->Height = Modes[Number - 1].Height;
		}
		if (Modes != nullptr)
			delete[] Modes;
		if (ChangeDisplaySettingsA(&Mode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			return;
		Width = GMode->Width;
		Height = GMode->Height;
		Depth = GMode->Depth;
	}
	ViewMode = VMode;
}

// 0x4faef0. A texture of the next powers of two in size, nearest filtering,
// clamped, with undefined contents (a buffer that is not cleared); with Empty
// only the texture name. Counts in SurfaceCreated and NumberOfSurfaces; always
// true.
bool TSurfaceGL::CreateTexture(TTextureGL *Texture, int Width, int Height, bool Empty)
{
	int Size = 1;
	while (Size < Width)
		Size *= 2;
	Texture->TexWidth = Size;
	Size = 1;
	while (Size < Height)
		Size *= 2;
	Texture->TexHeight = Size;
	Texture->Width = Width;
	Texture->Height = Height;
	if (Empty) {
		glEnable(GL_TEXTURE_2D);
		glGenTextures(1, &Texture->Texture);
		glBindTexture(GL_TEXTURE_2D, Texture->Texture);
	} else {
		glEnable(GL_TEXTURE_2D);
		glGenTextures(1, &Texture->Texture);
		glBindTexture(GL_TEXTURE_2D, Texture->Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		uint8 *Data = new uint8[Texture->TexWidth * Texture->TexHeight * 4];
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Texture->TexWidth, Texture->TexHeight, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, Data);
		delete Data;
	}
	SurfaceCreated[NumberOfSurfaces] = 1;
	NumberOfSurfaces++;
	return true;
}

// 0x4fb030. The display mode is not restored here.
void TSurfaceGL::ExitDisplay()
{
	for (int i = 0; i < NUMBER_OF_SURFACES_GL; i++) {
		if (Textures[i].Texture != 0) {
			glDeleteTextures(1, &Textures[i].Texture);
			Textures[i].Texture = 0;
		}
	}
	ReleasePbuffer();
	CacheSurfaces = 0;
	ViewMode = 0;
}

// 0x4fb070. The textures of TEMP_SURFACE to the last video cache surface and
// the pbuffer. PROGRAM_ERRORs: 15-17 for STRETCH_SURFACE, CACHE_SURFACE and
// AUTOMAP_SURFACE, 18 for a video cache surface, 12 without pbuffer (none can
// happen: CreateTexture always succeeds). Odd behaviour kept: TEMP_SURFACE's
// result is not checked; the video cache loop has a second way out for
// textures from the 25th on, which cannot happen with 16.
void TSurfaceGL::CreateSurfaces()
{
	NumberOfSurfaces = 0;
	CreateTexture(&Textures[TEMP_SURFACE], Rect[TEMP_SURFACE].right, Rect[TEMP_SURFACE].bottom, false);
	if (!CreateTexture(&Textures[STRETCH_SURFACE], Rect[STRETCH_SURFACE].right, Rect[STRETCH_SURFACE].bottom, false))
		throw (PROGRAM_ERROR)15;
	if (!CreateTexture(&Textures[CACHE_SURFACE], Rect[CACHE_SURFACE].right, Rect[CACHE_SURFACE].bottom, false))
		throw (PROGRAM_ERROR)16;
	if (!CreateTexture(&Textures[AUTOMAP_SURFACE], Rect[AUTOMAP_SURFACE].right, Rect[AUTOMAP_SURFACE].bottom, false))
		throw (PROGRAM_ERROR)17;
	for (int i = 0; i < 16; i++) {
		int Surface = SPRITE_SURFACE + i;
		if (i < 24) {
			if (!CreateTexture(&Textures[Surface], Rect[Surface].right, Rect[Surface].bottom, false))
				throw (PROGRAM_ERROR)18;
		} else if (!CreateTexture(&Textures[Surface], Rect[Surface].right, Rect[Surface].bottom, false)) {
			break;
		}
		CacheSurfaces++;
	}
	if (!CreatePbuffer())
		throw (PROGRAM_ERROR)12;
	Recreated[0] = true;
	Recreated[1] = true;
	Recreated[2] = true;
}

// 0x4fb1f0
void TSurfaceGL::ResetDisplay(int VMode, TFullscreenMode *GMode)
{
	try {
		InitDisplay(VMode, GMode);
	} catch (...) {
		try {
			ExitDisplay();
		} catch (...) {
			throw (PROGRAM_ERROR)12;
		}
		throw (PROGRAM_ERROR)12;
	}
}

// 0x4fb280. The arguments are not used.
void TSurfaceGL::RestoreSurfaces([[maybe_unused]] int ViewMode, [[maybe_unused]] TFullscreenMode *Mode)
{
	Unavailable = false;
	try {
		CreateSurfaces();
	} catch (...) {
		throw (PROGRAM_ERROR)12;
	}
}

// 0x4fb2e0. Creates the window's context (a pixel format with Resolution's
// depth, also in windowed mode) after the display mode and then the
// surfaces; nothing is done if a context exists for the same view mode and
// Force is not set. A display change since the last call (MainFrm.cpp) is
// reported by SurfaceRecreated(2). Odd behaviour kept: missing extensions are
// only reported to the debugger.
bool TSurfaceGL::Init(HWND *Window, int ViewMode, TFullscreenMode *Resolution, bool Force)
{
	Recreated[0] = false;
	Recreated[1] = false;
	Recreated[2] = false;
	UpdateWindowRect();
	if (DisplayChanged) {
		DisplayChanged = false;
		Recreated[2] = true;
	}
	if (Context != nullptr && !Force && ViewMode == this->ViewMode)
		return true;
	ExitDisplay();
	if (Context != nullptr && DeviceContext != nullptr)
		wglMakeCurrent(DeviceContext, nullptr);
	if (Context != nullptr)
		wglDeleteContext(Context);
	if (hWindow != nullptr && DeviceContext != nullptr)
		ReleaseDC(hWindow, DeviceContext);
	ResetDisplay(ViewMode, Resolution);
	hWindow = *Window;
	DeviceContext = GetDC(hWindow);
	// clang-format off
	PIXELFORMATDESCRIPTOR Format = {
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DEPTH_DONTCARE,
		PFD_TYPE_RGBA, (BYTE)Resolution->Depth,
	};
	// clang-format on
	int Number = ChoosePixelFormat(DeviceContext, &Format);
	if (Number == 0 || !SetPixelFormat(DeviceContext, Number, &Format))
		return false;
	Context = wglCreateContext(DeviceContext);
	wglMakeCurrent(DeviceContext, Context);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glAlphaFunc(GL_GREATER, 0.0f);
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	MainDC = DeviceContext;
	MainContext = Context;
	if (glh_init_extensions("WGL_ARB_pbuffer WGL_ARB_pixel_format ") != TRUE)
		OutputDebugStringA("Necessary extensions were not supported");
	RestoreSurfaces(ViewMode, Resolution);
	return true;
}

// 0x4fb4e0. The display modes of the desktop's depth from 550 pixels wide
// (a new[] array, the caller deletes it). Odd behaviour kept: the array has
// room for every mode, the count is not stored in NumberOfModes, and the
// description's size is never set.
int TSurfaceGL::GetFullscreenModes(TFullscreenMode **Modes)
{
	DEVMODEA Mode;
	int Number = 0;
	int Count = 0;
	EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &Mode);
	DWORD DesktopDepth = Mode.dmBitsPerPel;
	while (EnumDisplaySettingsA(nullptr, Count, &Mode))
		Count++;
	*Modes = new TFullscreenMode[Count];
	for (int i = 0; EnumDisplaySettingsA(nullptr, i, &Mode); i++) {
		if (Mode.dmBitsPerPel == DesktopDepth && Mode.dmPelsWidth >= 550) {
			(*Modes)[Number].Width = Mode.dmPelsWidth;
			(*Modes)[Number].Height = Mode.dmPelsHeight;
			(*Modes)[Number].Depth = Mode.dmBitsPerPel;
			(*Modes)[Number].Frequency = Mode.dmDisplayFrequency;
			Number++;
		}
	}
	return Number;
}

// 0x4fb5e0 (the DirectX engines' vtables use it too)
int TSurfaceGL::GetCacheSurfaces()
{
	return CacheSurfaces;
}

// 0x4fb5f0 (the DirectX engines' vtables use it too)
bool TSurfaceGL::IsUnavailable()
{
	return Unavailable;
}

// 0x4fb600. The extension strings are freed with glh_shutdown_extensions'
// inlined code.
void TSurfaceGL::ExitSurfaces()
{
	ExitDisplay();
	if (Context != nullptr && DeviceContext != nullptr && hWindow != nullptr) {
		wglMakeCurrent(DeviceContext, nullptr);
		wglDeleteContext(Context);
	}
	glh_shutdown_extensions();
}

// 0x4fb670. Surface.cpp's GetSurface creates it with new.
TSurfaceGL::TSurfaceGL()
{
}

// 0x4fb700. Rect[PRIMARY_SURFACE] is not set.
void TSurfaceGL::InitSurfaces()
{
	hWindow = nullptr;
	ViewMode = 0;
	Context = nullptr;
	DeviceContext = nullptr;
	CacheSurfaceWidth = 256;
	CacheSurfaceHeight = 256;
	SetRect(&Rect[SCREEN_SURFACE], 0, 0, 0, 0);
	SetRect(&Rect[TEMP_SURFACE], 0, 0, 640, 480);
	SetRect(&Rect[STRETCH_SURFACE], 0, 0, 64, 64);
	SetRect(&Rect[CACHE_SURFACE], 0, 0, 512, 384);
	SetRect(&Rect[AUTOMAP_SURFACE], 0, 0, 106, 106);
	for (int i = SPRITE_SURFACE; i < NUMBER_OF_SURFACES_GL; i++) {
		Rect[i].left = 0;
		Rect[i].top = 0;
		Rect[i].right = CacheSurfaceWidth;
		Rect[i].bottom = CacheSurfaceHeight;
	}
	for (int i = 0; i < NUMBER_OF_SURFACES_GL; i++)
		Textures[i].Texture = 0;
	SetDefaultNoStretching(false);
	ResetFrameTimer();
	SetFramerateLimit(GetConfineFramerate());
}

// 0x4fb7f0, lines 767-768. RGB data into a surface's texture at (x, y); the
// pixels of the surface's colour key become transparent. AUTOMAP_SURFACE is
// written without selecting it.
void TSurfaceGL::WriteImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data)
{
	TASSERT(767, ToSurface!=SCREEN_SURFACE);
	TASSERT(768, x>=0 && y>=0 && Width>0 && Height>0);
	if (ToSurface != AUTOMAP_SURFACE)
		SelectSurface(ToSurface);
	int Size = Width * Height;
	uint8 *Image = new uint8[Size * 4];
	uint8 *Pixel = Image;
	for (int i = 0; i < Size; i++) {
		Pixel[0] = Data[0];
		Pixel[1] = Data[1];
		Pixel[2] = Data[2];
		TRGBColor *Key = &Textures[ToSurface].ColorKey;
		if (Data[0] == Key->R && Data[1] == Key->G && Data[2] == Key->B)
			Pixel[3] = 0;
		else
			Pixel[3] = 0xff;
		Data += 3;
		Pixel += 4;
	}
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, Textures[ToSurface].Texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, Image);
	delete Image;
}

// 0x4fbbb0, lines 802-803. A sprite with a colour mask: with a mask, the
// pixels whose mask is yellow, red, green or blue become the grey value of
// the pixel tinted with Colors[0-3]; without one, every pixel that is not
// Colors[1] is tinted with Colors[0]. The other pixels are copied, opaque
// unless they are the surface's colour key.
void TSurfaceGL::WriteMaskedImage(int ToSurface, int x, int y, int Width, int Height, uint8 *Data,
	uint8 *Mask, TRGBColor *Colors)
{
	TASSERT(802, ToSurface!=SCREEN_SURFACE);
	TASSERT(803, x>=0 && y>=0 && Width>0 && Height>0);
	SelectSurface(ToSurface);
	int Size = Width * Height;
	uint8 *Image = new uint8[Size * 4];
	uint8 *Pixel = Image;
	for (int i = 0; i < Size; i++) {
		uint8 *From = &Data[i * 3];
		int Index;
		if (Mask != nullptr) {
			uint8 *M = &Mask[i * 3];
			if (M[0] == 0xff) {
				if (M[1] == 0xff) {
					if (M[2] != 0)
						goto Copy;
					Index = 0;
				} else {
					if (M[1] != 0 || M[2] != 0)
						goto Copy;
					Index = 1;
				}
			} else {
				if (M[0] != 0)
					goto Copy;
				if (M[1] == 0xff) {
					if (M[2] != 0)
						goto Copy;
					Index = 2;
				} else {
					if (M[1] != 0 || M[2] != 0xff)
						goto Copy;
					Index = 3;
				}
			}
		} else {
			if (From[0] == Colors[1].R && From[1] == Colors[1].G && From[2] == Colors[1].B)
				goto Copy;
			Index = 0;
		}
		{
			int Grey = (From[1] * 59 + From[0] * 30 + From[2] * 11) / 100;
			TRGBColor *C = &Colors[Index];
			Pixel[0] = (uint8)(C->R * Grey / 256);
			Pixel[1] = (uint8)(C->G * Grey / 256);
			Pixel[2] = (uint8)(C->B * Grey / 256);
			Pixel[3] = 0xff;
		}
		goto Next;
	Copy:
		Pixel[0] = From[0];
		Pixel[1] = From[1];
		Pixel[2] = From[2];
		Pixel[3] = TRGBColor(From[0], From[1], From[2]) == Textures[ToSurface].ColorKey ? 0 : 0xff;
	Next:
		Pixel += 4;
	}
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, Textures[ToSurface].Texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, Image);
	delete Image;
}

// 0x4fc130, line 1023. A copy with nearest filtering, clipped. The statistics
// count the unclipped size.
void TSurfaceGL::BlitRectClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRect Clip)
{
	TASSERT(1023, FromSurface != SCREEN_SURFACE);
	RECT Destination = {x, y, x + FromRect.width, y + FromRect.height};
	RECT Source = {FromRect.x, FromRect.y, FromRect.x + FromRect.width, FromRect.y + FromRect.height};
	if (!ClipRect(Clip, &Destination, &Source))
		return;
	TTextureGL *Texture = &Textures[FromSurface];
	double Left = (double)Source.left / Texture->TexWidth;
	double Right = (double)Source.right / Texture->TexWidth;
	double Top = (double)Source.top / Texture->TexHeight;
	double Bottom = (double)Source.bottom / Texture->TexHeight;
	SelectSurface(ToSurface);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, Texture->Texture);
	SetTextureFilter(GL_NEAREST);
	glBegin(GL_QUADS);
	glColor3ub(255, 255, 255);
	glTexCoord2d(Left, Top);
	glVertex2i(Destination.left, Destination.top);
	glTexCoord2d(Left, Bottom);
	glVertex2i(Destination.left, Destination.bottom);
	glTexCoord2d(Right, Bottom);
	glVertex2i(Destination.right, Destination.bottom);
	glTexCoord2d(Right, Top);
	glVertex2i(Destination.right, Destination.top);
	glEnd();
	BlitCount++;
	BlitPixels += FromRect.height * FromRect.width;
	AddDirtyRect(TRect(Destination.left, Destination.top, Destination.right - Destination.left,
		Destination.bottom - Destination.top));
}

// 0x4fc480, line 1073. Clipped to the target surface.
void TSurfaceGL::Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey)
{
	TASSERT2(1073, FromRect.width>0 && FromRect.height>0, FromRect.width, FromRect.height);
	TRect Clip;
	GetSurfaceRect(ToSurface, &Clip);
	BlitClipped(ToSurface, x, y, FromSurface, FromRect, ColorKey, Clip);
}

// 0x4fc6a0, line 1086
void TSurfaceGL::BlitRect(int ToSurface, int x, int y, int FromSurface, TRect FromRect)
{
	TASSERT2(1086, FromRect.width>0 && FromRect.height>0, FromRect.width, FromRect.height);
	TRect Clip;
	GetSurfaceRect(ToSurface, &Clip);
	BlitRectClipped(ToSurface, x, y, FromSurface, FromRect, Clip);
}

// 0x4fc8a0, line 1540
void TSurfaceGL::GetSurfaceRect(int Surface, TRect *Rect)
{
	TASSERT(1540, In(Surface,0,NUMBER_OF_SURFACES_GL-1));
	Rect->x = this->Rect[Surface].left;
	Rect->y = this->Rect[Surface].top;
	Rect->width = this->Rect[Surface].right - this->Rect[Surface].left;
	Rect->height = this->Rect[Surface].bottom - this->Rect[Surface].top;
}

// 0x4fca30, line 1555. Type 0-2 (Init clears all three, CreateSurfaces sets
// them; 2 is also set by a display change); reading clears it.
bool TSurfaceGL::SurfaceRecreated(int Type)
{
	TASSERT(1555, Type>=0 && Type<3);
	bool Result = Recreated[Type];
	Recreated[Type] = false;
	return Result;
}

// The slots whose code the linker took from another object (identical
// copies): surfaceDX9.obj's 0x4f0410 and 0x4f04d0, SurfaceDX.obj's 0x4eb000
// and the return-true body 0x4d7d40.

// +0x58 (0x4f0410). Clips Rect to Clip and moves the edges of Source by as
// much; false when nothing is left.
bool TSurfaceGL::ClipRect(TRect Clip, RECT *Rect, RECT *Source)
{
	int ClipRight = Clip.x + Clip.width;
	int ClipBottom = Clip.y + Clip.height;
	if (Rect->left < ClipRight && Rect->top < ClipBottom && Rect->right > Clip.x && Rect->bottom > Clip.y) {
		if (Rect->left < Clip.x) {
			Source->left += Clip.x - Rect->left;
			Rect->left = Clip.x;
		}
		if (Rect->top < Clip.y) {
			Source->top += Clip.y - Rect->top;
			Rect->top = Clip.y;
		}
		if (Rect->right > ClipRight) {
			Source->right += Clip.width - Rect->right + Clip.x;
			Rect->right = ClipRight;
		}
		if (Rect->bottom > ClipBottom) {
			Source->bottom += Clip.height - Rect->bottom + Clip.y;
			Rect->bottom = ClipBottom;
		}
		return true;
	}
	return false;
}

// +0x5c (0x4f04d0)
void TSurfaceGL::ResetFrameTimer()
{
	double Now = PATELLA_TIME_GET_TIME();
	StartTime = Now;
	Framerate = 0xffff;
	NextFrame = Now;
	FPS = 0.0;
	FrameTime = 1000.0 / 0xffff;
}

// +0xa4 (0x4d7d40)
bool TSurfaceGL::CanShadeRectangles()
{
	return true;
}

// +0xac (0x4eb000). Limits from 10 to 200 frames per second; others mean no
// limit (0xffff).
void TSurfaceGL::SetFramerateLimit(int Limit)
{
	if (Limit < 10 || Limit >= 201)
		Framerate = 0xffff;
	else
		Framerate = Limit;
	FrameTime = 1000.0 / Framerate;
}

// +0xbc (0x4d7d40). Unlike TSurface's, it does not take the main window.
bool TSurfaceGL::TestEngine()
{
	return true;
}
