// SurfaceGL.cpp and its glh_extensions.h code against the original (0x4f46f0-0x4fcb8f).
//
// Nothing reaches Windows, opengl32.dll or glu32.dll: every function the
// surface imports is a fake on both sides. The image calls them through its
// import table, whose slots get the fakes' addresses. Ours call the user32,
// gdi32, kernel32 and winmm functions through difftest.exe's import table,
// whose slots are patched while the test runs; difftest.exe does not link
// opengl32.lib or glu32.lib at all: the import symbols of the OpenGL
// functions (__imp__glBegin@4, ...) are this file's GLSlot_ variables
// (/alternatename), which hold the fakes, and gluOrtho2D is defined here.
// The extension entry points come from the fake wglGetProcAddress: the
// WGL_ARB_pbuffer/pixel_format functions and wglGetExtensionsStringARB are
// fakes, every other name gets a fixed address (or NULL, decided from the
// name), so both sides' glh_genext.h pointers can be compared.
//
// The fakes log every call with its arguments (pixels as hashes; floats and
// doubles as bits) and answer from one world (display modes, results, handle
// and texture name counters, the current context, extension strings), which
// both sides start from. Each round constructs both surfaces in memory filled
// with the same pattern (the image's constructor 0x4fb670 and ours), runs
// InitSurfaces and Init, then random operations through the vtable (the
// image's object through its own vtable) and the non-virtual members, and
// ExitSurfaces and the destructors (or the scalar deleting destructors).
// After each operation the logs, every word of both objects, the files'
// statics (ours are located once by sentinel values: they are static), the
// shared statistics, the surface memory list, MainFrm.cpp's display change
// flag, glh_extensions.h's strings and all 549 extension pointers are
// compared. Some rounds test glh_init_extensions/glh_shutdown_extensions on
// random extension strings and requests, and IsOpenGLWorking (0x4f9710)
// failing at each step. The error paths (assertions and PROGRAM_ERRORs, which
// need a failing pbuffer in CreateSurfaces) are not run: the image cannot
// throw in the child; nor are glh's out-of-memory exits.
//
// DIFFTEST_ONLY=SurfaceGL runs it alone (GL_TRACE=1 traces, GL_LOGS=<file>
// writes every compared log, GL_ROUNDS=<n>). It has its own random numbers,
// so the tests after it see the same sequence as before it existed.

#include "difftest.h"
#include "imageeh.h"
#include "../../src/SurfaceGL.h"
#include "../../src/Extern/glh/glh_genext.h"
#include "../../src/GUI.h"
#include "../../src/MainFrm.h"
#include "../../src/Preferences.h"
#include "surfacegltest.h"
#include <string>
#include <vector>
#include <new>
#include <stdarg.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")

#pragma warning(disable: 4100)

// glh_extensions.h's functions (declared there only with GLH_EXT_SINGLE_FILE).
int glh_init_extensions(const char *origReqExts);
void glh_shutdown_extensions();

static_assert(sizeof(TSurfaceGL) == 0x498, "TSurfaceGL size");
static_assert(sizeof(TTextureGL) == 0x20, "TTextureGL size");
static_assert(offsetof(TSurfaceGL, DeviceContext) == 0x0c, "DeviceContext");
static_assert(offsetof(TSurfaceGL, ScreenRect) == 0x28, "ScreenRect");
static_assert(offsetof(TSurfaceGL, Context) == 0x70, "Context");
static_assert(offsetof(TSurfaceGL, Textures) == 0x74, "Textures");
static_assert(offsetof(TSurfaceGL, Rect) == 0x334, "Rect");
static_assert(offsetof(TSurfaceGL, Recreated) == 0x494, "Recreated");

enum { IMAGE = 0, OURS = 1 };
static int Side;
static std::string Log[2];
static bool Trace;
static FILE *LogFile;
static uint32 GLSeed = 0x5e11;

static uint32 Rnd()
{
	GLSeed = GLSeed * 1103515245 + 12345;
	return GLSeed >> 8;
}
static int Between(int Min, int Max) { return Min + (int)(Rnd() % (uint32)(Max - Min + 1)); }
static bool Chance(int Percent) { return (int)(Rnd() % 100) < Percent; }

static void Logf(const char *Format, ...)
{
	char Buffer[512];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

static uint32 Hash(const void *Data, size_t Size)
{
	const uint8 *P = (const uint8 *)Data;
	uint32 H = 2166136261u;
	for (size_t i = 0; i < Size; i++)
		H = (H ^ P[i]) * 16777619u;
	return H;
}
static uint32 FloatBits(float Value) { uint32 B; memcpy(&B, &Value, 4); return B; }
static unsigned long long DoubleBits(double Value) { unsigned long long B; memcpy(&B, &Value, 8); return B; }

// --- The world the fakes answer from ---------------------------------------------------------

struct TGLWorld {
	uint32 Clock, ClockStep;
	uint32 NextHandle;
	uint32 NextTexture;
	uint32 CurrentDC, CurrentContext;
	int PixelFormat;
	BOOL SetPixelResult, ShareResult;
	bool CreateContextFails;
	LONG ChangeResult, ChangeBackResult;
	int Desktop[4];        // width, height, depth, frequency
	int NumberOfModes;
	int Modes[12][4];
	RECT Client;
	POINT Origin;
	int Confine;
	int ProcNullPercent;   // the other extension names: NULL by a hash of the name
	uint32 ProcSeed;
	int MissingWGL;        // bit i: WGLNames[i] is not found
	int PbufferFormat;
	UINT PbufferCount;
	bool PbufferFails, PbufferDCFails;
	uint32 TexSeed;
	int Near[3];           // glGetTexImage's pixels are often near this colour
	char Version[16];
	char Extensions[4096];
	char WglExtensions[1024];
};
static TGLWorld World;

// --- Fake functions --------------------------------------------------------------------------

static HDC H(uint32 Value) { return (HDC)(uintptr_t)Value; }
static uint32 U(const void *Handle) { return (uint32)(uintptr_t)Handle; }

static void WINAPI FakeNeverCalled()
{
	printf("FAIL SurfaceGL: an extension entry point that is never called was called\n");
	fflush(stdout);
	abort();
}

static BOOL WINAPI FakeChoosePixelFormatARB(HDC DC, const int *IAttributes, const FLOAT *FAttributes, UINT Max,
	int *Formats, UINT *Count)
{
	Logf("ChoosePixelFormatARB %x", U(DC));
	for (const int *A = IAttributes; A[0] != 0; A += 2) // pairs, ended by a 0 key
		Logf(" %x=%x", A[0], A[1]);
	Logf(" f%d %u;", FAttributes != NULL, Max);
	*Formats = World.PbufferFormat;
	*Count = World.PbufferCount;
	return TRUE;
}
static HPBUFFERARB WINAPI FakeCreatePbufferARB(HDC DC, int Format, int Width, int Height, const int *Attributes)
{
	Logf("CreatePbufferARB %x %d %d %d %d;", U(DC), Format, Width, Height, Attributes != NULL);
	return World.PbufferFails ? NULL : (HPBUFFERARB)(uintptr_t)World.NextHandle++;
}
static HDC WINAPI FakeGetPbufferDCARB(HPBUFFERARB Pbuffer)
{
	Logf("GetPbufferDCARB %x;", U(Pbuffer));
	return World.PbufferDCFails ? NULL : H(World.NextHandle++);
}
static int WINAPI FakeReleasePbufferDCARB(HPBUFFERARB Pbuffer, HDC DC)
{
	Logf("ReleasePbufferDCARB %x %x;", U(Pbuffer), U(DC));
	return 1;
}
static BOOL WINAPI FakeDestroyPbufferARB(HPBUFFERARB Pbuffer)
{
	Logf("DestroyPbufferARB %x;", U(Pbuffer));
	return TRUE;
}
static const char *WINAPI FakeGetExtensionsStringARB(HDC DC)
{
	Logf("GetExtensionsStringARB %x;", U(DC));
	return World.WglExtensions;
}

static const char *const WGLNames[] = {"wglCreatePbufferARB", "wglGetPbufferDCARB", "wglReleasePbufferDCARB",
	"wglDestroyPbufferARB", "wglQueryPbufferARB", "wglGetPixelFormatAttribivARB", "wglGetPixelFormatAttribfvARB",
	"wglChoosePixelFormatARB", "wglGetExtensionsStringARB"};
static void *const WGLFakes[] = {(void *)FakeCreatePbufferARB, (void *)FakeGetPbufferDCARB,
	(void *)FakeReleasePbufferDCARB, (void *)FakeDestroyPbufferARB, (void *)FakeNeverCalled, (void *)FakeNeverCalled,
	(void *)FakeNeverCalled, (void *)FakeChoosePixelFormatARB, (void *)FakeGetExtensionsStringARB};

static PROC WINAPI FakeWglGetProcAddress(LPCSTR Name)
{
	Logf("GetProcAddress %s;", Name);
	for (int i = 0; i < 9; i++)
		if (strcmp(Name, WGLNames[i]) == 0)
			return (World.MissingWGL >> i & 1) ? NULL : (PROC)WGLFakes[i];
	uint32 Value = Hash(Name, strlen(Name));
	if ((int)((Value ^ World.ProcSeed) % 100) < World.ProcNullPercent)
		return NULL;
	return (PROC)(uintptr_t)(0x60000000 | (Value & 0x0ffffff0));
}
static HDC WINAPI FakeWglGetCurrentDC() { Logf("GetCurrentDC;"); return H(World.CurrentDC); }
static BOOL WINAPI FakeWglDeleteContext(HGLRC Context) { Logf("DeleteContext %x;", U(Context)); return TRUE; }
static BOOL WINAPI FakeWglMakeCurrent(HDC DC, HGLRC Context)
{
	Logf("MakeCurrent %x %x;", U(DC), U(Context));
	World.CurrentDC = U(DC);
	World.CurrentContext = U(Context);
	return TRUE;
}
static HGLRC WINAPI FakeWglGetCurrentContext() { Logf("GetCurrentContext;"); return (HGLRC)(uintptr_t)World.CurrentContext; }
static BOOL WINAPI FakeWglShareLists(HGLRC A, HGLRC B) { Logf("ShareLists %x %x;", U(A), U(B)); return World.ShareResult; }
static HGLRC WINAPI FakeWglCreateContext(HDC DC)
{
	Logf("CreateContext %x;", U(DC));
	return World.CreateContextFails ? NULL : (HGLRC)(uintptr_t)World.NextHandle++;
}
static void WINAPI FakeGlReadBuffer(unsigned Mode) { Logf("ReadBuffer %x;", Mode); }
static void WINAPI FakeGlDrawBuffer(unsigned Mode) { Logf("DrawBuffer %x;", Mode); }
static void WINAPI FakeGlEnd() { Logf("End;"); }
static void WINAPI FakeGlVertex2i(int x, int y) { Logf("V %d %d;", x, y); }
static void WINAPI FakeGlTexCoord2f(float s, float t) { Logf("T2f %08x %08x;", FloatBits(s), FloatBits(t)); }
static void WINAPI FakeGlColor3ub(uint8 R, uint8 G, uint8 B) { Logf("C %u %u %u;", R, G, B); }
static void WINAPI FakeGlBegin(unsigned Mode) { Logf("Begin %u;", Mode); }
static void WINAPI FakeGlEnable(unsigned Cap) { Logf("Enable %x;", Cap); }
static void WINAPI FakeGlAlphaFunc(unsigned Func, float Ref) { Logf("AlphaFunc %x %08x;", Func, FloatBits(Ref)); }
static void WINAPI FakeGlViewport(int x, int y, int Width, int Height) { Logf("Viewport %d %d %d %d;", x, y, Width, Height); }
static void WINAPI FakeGlLoadIdentity() { Logf("LoadIdentity;"); }
static void WINAPI FakeGlMatrixMode(unsigned Mode) { Logf("MatrixMode %x;", Mode); }
static void WINAPI FakeGlCopyTexSubImage2D(unsigned Target, int Level, int xOffset, int yOffset, int x, int y,
	int Width, int Height)
{
	Logf("CopyTexSubImage2D %x %d %d %d %d %d %d %d;", Target, Level, xOffset, yOffset, x, y, Width, Height);
}
static void WINAPI FakeGlBindTexture(unsigned Target, unsigned Texture) { Logf("BindTexture %x %u;", Target, Texture); }
static void WINAPI FakeGlTexParameteri(unsigned Target, unsigned Name, int Value)
{
	Logf("TexParameteri %x %x %x;", Target, Name, Value);
}
static void WINAPI FakeGlDisable(unsigned Cap) { Logf("Disable %x;", Cap); }
static void WINAPI FakeGlRecti(int x1, int y1, int x2, int y2) { Logf("Recti %d %d %d %d;", x1, y1, x2, y2); }
static void WINAPI FakeGlTexCoord2d(double s, double t) { Logf("T2d %llx %llx;", DoubleBits(s), DoubleBits(t)); }
static void WINAPI FakeGlTexSubImage2D(unsigned Target, int Level, int x, int y, int Width, int Height,
	unsigned Format, unsigned Type, const void *Pixels)
{
	Logf("TexSubImage2D %x %d %d %d %d %d %x %x %08x;", Target, Level, x, y, Width, Height, Format, Type,
		Hash(Pixels, (size_t)Width * Height * 4));
}
static void WINAPI FakeGlGetTexImage(unsigned Target, int Level, unsigned Format, unsigned Type, void *Pixels)
{
	Logf("GetTexImage %x %d %x %x;", Target, Level, Format, Type);
	uint8 *P = (uint8 *)Pixels;
	uint32 S = World.TexSeed;
	for (int i = 0; i < 64 * 64; i++) {
		S = S * 1103515245 + 12345;
		int Kind = (S >> 8) % 4;
		for (int k = 0; k < 4; k++) {
			S = S * 1103515245 + 12345;
			int Value = (int)(S >> 12) & 0xff;
			if (k < 3 && Kind != 0)
				Value = World.Near[k] + (int)((S >> 9) % 9) - 4;
			P[i * 4 + k] = (uint8)Value;
		}
	}
	World.TexSeed = S;
}
static void WINAPI FakeGlBlendFunc(unsigned Source, unsigned Destination) { Logf("BlendFunc %x %x;", Source, Destination); }
static void WINAPI FakeGlTexImage2D(unsigned Target, int Level, int Internal, int Width, int Height, int Border,
	unsigned Format, unsigned Type, const void *Pixels)
{
	// the pixels are an uncleared buffer
	Logf("TexImage2D %x %d %x %d %d %d %x %x %d;", Target, Level, Internal, Width, Height, Border, Format, Type,
		Pixels != NULL);
}
static void WINAPI FakeGlGenTextures(int Number, unsigned *Textures)
{
	for (int i = 0; i < Number; i++)
		Textures[i] = World.NextTexture++;
	Logf("GenTextures %d %u;", Number, Textures[0]);
}
static void WINAPI FakeGlDeleteTextures(int Number, const unsigned *Textures) { Logf("DeleteTextures %d %u;", Number, Textures[0]); }
static const uint8 *WINAPI FakeGlGetString(unsigned Name)
{
	Logf("GetString %x;", Name);
	if (Name == GL_VERSION)
		return (const uint8 *)World.Version;
	if (Name == GL_EXTENSIONS)
		return (const uint8 *)World.Extensions;
	return (const uint8 *)"";
}
static void FakeOrtho2D(double Left, double Right, double Bottom, double Top)
{
	Logf("Ortho2D %llx %llx %llx %llx;", DoubleBits(Left), DoubleBits(Right), DoubleBits(Bottom), DoubleBits(Top));
}
static void WINAPI ImageOrtho2D(double Left, double Right, double Bottom, double Top) { FakeOrtho2D(Left, Right, Bottom, Top); }

// Ours: glu32's functions are not dllimport, so gluOrtho2D is defined here.
extern "C" void WINAPI gluOrtho2D(double Left, double Right, double Bottom, double Top)
{
	FakeOrtho2D(Left, Right, Bottom, Top);
}

// Ours: the import slots of the OpenGL functions.
extern "C" {
void *GLSlot_wglGetProcAddress = (void *)FakeWglGetProcAddress;
void *GLSlot_wglGetCurrentDC = (void *)FakeWglGetCurrentDC;
void *GLSlot_wglDeleteContext = (void *)FakeWglDeleteContext;
void *GLSlot_wglMakeCurrent = (void *)FakeWglMakeCurrent;
void *GLSlot_wglGetCurrentContext = (void *)FakeWglGetCurrentContext;
void *GLSlot_wglShareLists = (void *)FakeWglShareLists;
void *GLSlot_wglCreateContext = (void *)FakeWglCreateContext;
void *GLSlot_glReadBuffer = (void *)FakeGlReadBuffer;
void *GLSlot_glDrawBuffer = (void *)FakeGlDrawBuffer;
void *GLSlot_glEnd = (void *)FakeGlEnd;
void *GLSlot_glVertex2i = (void *)FakeGlVertex2i;
void *GLSlot_glTexCoord2f = (void *)FakeGlTexCoord2f;
void *GLSlot_glColor3ub = (void *)FakeGlColor3ub;
void *GLSlot_glBegin = (void *)FakeGlBegin;
void *GLSlot_glEnable = (void *)FakeGlEnable;
void *GLSlot_glAlphaFunc = (void *)FakeGlAlphaFunc;
void *GLSlot_glViewport = (void *)FakeGlViewport;
void *GLSlot_glLoadIdentity = (void *)FakeGlLoadIdentity;
void *GLSlot_glMatrixMode = (void *)FakeGlMatrixMode;
void *GLSlot_glCopyTexSubImage2D = (void *)FakeGlCopyTexSubImage2D;
void *GLSlot_glBindTexture = (void *)FakeGlBindTexture;
void *GLSlot_glTexParameteri = (void *)FakeGlTexParameteri;
void *GLSlot_glDisable = (void *)FakeGlDisable;
void *GLSlot_glRecti = (void *)FakeGlRecti;
void *GLSlot_glTexCoord2d = (void *)FakeGlTexCoord2d;
void *GLSlot_glTexSubImage2D = (void *)FakeGlTexSubImage2D;
void *GLSlot_glGetTexImage = (void *)FakeGlGetTexImage;
void *GLSlot_glBlendFunc = (void *)FakeGlBlendFunc;
void *GLSlot_glTexImage2D = (void *)FakeGlTexImage2D;
void *GLSlot_glGenTextures = (void *)FakeGlGenTextures;
void *GLSlot_glDeleteTextures = (void *)FakeGlDeleteTextures;
void *GLSlot_glGetString = (void *)FakeGlGetString;
}
#pragma comment(linker, "/alternatename:__imp__wglGetProcAddress@4=_GLSlot_wglGetProcAddress")
#pragma comment(linker, "/alternatename:__imp__wglGetCurrentDC@0=_GLSlot_wglGetCurrentDC")
#pragma comment(linker, "/alternatename:__imp__wglDeleteContext@4=_GLSlot_wglDeleteContext")
#pragma comment(linker, "/alternatename:__imp__wglMakeCurrent@8=_GLSlot_wglMakeCurrent")
#pragma comment(linker, "/alternatename:__imp__wglGetCurrentContext@0=_GLSlot_wglGetCurrentContext")
#pragma comment(linker, "/alternatename:__imp__wglShareLists@8=_GLSlot_wglShareLists")
#pragma comment(linker, "/alternatename:__imp__wglCreateContext@4=_GLSlot_wglCreateContext")
#pragma comment(linker, "/alternatename:__imp__glReadBuffer@4=_GLSlot_glReadBuffer")
#pragma comment(linker, "/alternatename:__imp__glDrawBuffer@4=_GLSlot_glDrawBuffer")
#pragma comment(linker, "/alternatename:__imp__glEnd@0=_GLSlot_glEnd")
#pragma comment(linker, "/alternatename:__imp__glVertex2i@8=_GLSlot_glVertex2i")
#pragma comment(linker, "/alternatename:__imp__glTexCoord2f@8=_GLSlot_glTexCoord2f")
#pragma comment(linker, "/alternatename:__imp__glColor3ub@12=_GLSlot_glColor3ub")
#pragma comment(linker, "/alternatename:__imp__glBegin@4=_GLSlot_glBegin")
#pragma comment(linker, "/alternatename:__imp__glEnable@4=_GLSlot_glEnable")
#pragma comment(linker, "/alternatename:__imp__glAlphaFunc@8=_GLSlot_glAlphaFunc")
#pragma comment(linker, "/alternatename:__imp__glViewport@16=_GLSlot_glViewport")
#pragma comment(linker, "/alternatename:__imp__glLoadIdentity@0=_GLSlot_glLoadIdentity")
#pragma comment(linker, "/alternatename:__imp__glMatrixMode@4=_GLSlot_glMatrixMode")
#pragma comment(linker, "/alternatename:__imp__glCopyTexSubImage2D@32=_GLSlot_glCopyTexSubImage2D")
#pragma comment(linker, "/alternatename:__imp__glBindTexture@8=_GLSlot_glBindTexture")
#pragma comment(linker, "/alternatename:__imp__glTexParameteri@12=_GLSlot_glTexParameteri")
#pragma comment(linker, "/alternatename:__imp__glDisable@4=_GLSlot_glDisable")
#pragma comment(linker, "/alternatename:__imp__glRecti@16=_GLSlot_glRecti")
#pragma comment(linker, "/alternatename:__imp__glTexCoord2d@16=_GLSlot_glTexCoord2d")
#pragma comment(linker, "/alternatename:__imp__glTexSubImage2D@36=_GLSlot_glTexSubImage2D")
#pragma comment(linker, "/alternatename:__imp__glGetTexImage@20=_GLSlot_glGetTexImage")
#pragma comment(linker, "/alternatename:__imp__glBlendFunc@8=_GLSlot_glBlendFunc")
#pragma comment(linker, "/alternatename:__imp__glTexImage2D@36=_GLSlot_glTexImage2D")
#pragma comment(linker, "/alternatename:__imp__glGenTextures@8=_GLSlot_glGenTextures")
#pragma comment(linker, "/alternatename:__imp__glDeleteTextures@8=_GLSlot_glDeleteTextures")
#pragma comment(linker, "/alternatename:__imp__glGetString@4=_GLSlot_glGetString")

static int WINAPI FakeChoosePixelFormat(HDC DC, const PIXELFORMATDESCRIPTOR *Format)
{
	Logf("ChoosePixelFormat %x", U(DC));
	const uint8 *P = (const uint8 *)Format;
	for (int i = 0; i < (int)sizeof(PIXELFORMATDESCRIPTOR); i++)
		Logf(" %02x", P[i]);
	Logf(";");
	return World.PixelFormat;
}
static BOOL WINAPI FakeSetPixelFormat(HDC DC, int Number, const PIXELFORMATDESCRIPTOR *Format)
{
	Logf("SetPixelFormat %x %d %08x;", U(DC), Number, Hash(Format, sizeof(PIXELFORMATDESCRIPTOR)));
	return World.SetPixelResult;
}
static BOOL WINAPI FakeSwapBuffers(HDC DC) { Logf("SwapBuffers %x;", U(DC)); return TRUE; }
static int WINAPI FakeReleaseDC(HWND Window, HDC DC) { Logf("ReleaseDC %x %x;", U(Window), U(DC)); return 1; }
static LONG WINAPI FakeChangeDisplaySettingsA(DEVMODEA *Mode, DWORD Flags)
{
	Logf("ChangeDisplaySettings %08x %lu;", Mode != NULL ? Hash(Mode, sizeof(DEVMODEA)) : 0, Flags);
	return Mode != NULL ? World.ChangeResult : World.ChangeBackResult;
}
static BOOL WINAPI FakeEnumDisplaySettingsA(LPCSTR Device, DWORD Number, DEVMODEA *Mode)
{
	Logf("EnumDisplaySettings %d %lx;", Device != NULL, Number);
	const int *M;
	if (Number == ENUM_CURRENT_SETTINGS)
		M = World.Desktop;
	else if ((int)Number < World.NumberOfModes)
		M = World.Modes[Number];
	else
		return FALSE;
	Mode->dmPelsWidth = M[0];
	Mode->dmPelsHeight = M[1];
	Mode->dmBitsPerPel = M[2];
	Mode->dmDisplayFrequency = M[3];
	return TRUE;
}
static BOOL WINAPI FakeSetRect(RECT *R, int Left, int Top, int Right, int Bottom)
{
	Logf("SetRect %d %d %d %d;", Left, Top, Right, Bottom);
	R->left = Left;
	R->top = Top;
	R->right = Right;
	R->bottom = Bottom;
	return TRUE;
}
static HDC WINAPI FakeGetDC(HWND Window) { Logf("GetDC %x;", U(Window)); return H(World.NextHandle++); }
static BOOL WINAPI FakeGetClientRect(HWND Window, RECT *R)
{
	Logf("GetClientRect %x;", U(Window));
	*R = World.Client;
	return TRUE;
}
static BOOL WINAPI FakeClientToScreen(HWND Window, POINT *P)
{
	Logf("ClientToScreen %x %ld %ld;", U(Window), P->x, P->y);
	P->x += World.Origin.x;
	P->y += World.Origin.y;
	return TRUE;
}
static void WINAPI FakeSleep(DWORD Milliseconds) { Logf("Sleep %lu;", Milliseconds); }
static void WINAPI FakeOutputDebugStringA(LPCSTR Text) { Logf("OutputDebugString %s;", Text); }
static DWORD WINAPI FakeTimeGetTime()
{
	DWORD Now = World.Clock;
	World.Clock += World.ClockStep;
	Logf("timeGetTime %lu;", Now);
	return Now;
}

static void __cdecl FakeDrawSkin(int Surface, int x, int y, int Width, int Height, int Skin, int SourceX, int SourceY)
{
	Logf("DrawSkin %d %d %d %d %d %d %d %d;", Surface, x, y, Width, Height, Skin, SourceX, SourceY);
}
static void __cdecl FakeSetDefaultNoStretching(bool Value) { Logf("SetDefaultNoStretching %d;", Value); }
static int __cdecl FakeGetConfineFramerate() { Logf("GetConfineFramerate;"); return World.Confine; }
static void *__cdecl ImageRealloc(void *Block, size_t Size) { return realloc(Block, Size); }
static char *__cdecl ImageStrdup(const char *Text) { return _strdup(Text); }

// --- Patches ---------------------------------------------------------------------------------

struct TGLSaved {
	uint8 *Address;
	uint8 Bytes[5];
	int Size;
};
static std::vector<TGLSaved> GLSaved;

static void Write(void *Address, const void *Data, int Size)
{
	TGLSaved S;
	S.Address = (uint8 *)Address;
	S.Size = Size;
	DWORD Protection;
	VirtualProtect(Address, Size, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(S.Bytes, Address, Size);
	memcpy(Address, Data, Size);
	VirtualProtect(Address, Size, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Address, Size);
	GLSaved.push_back(S);
}
static void Redirect(void *Function, void *Target)
{
	uint8 Jump[5];
	Jump[0] = 0xe9;
	*(int32_t *)(Jump + 1) = (int32_t)((uintptr_t)Target - ((uintptr_t)Function + 5));
	Write(Function, Jump, 5);
}
static void RestoreAll()
{
	for (size_t i = GLSaved.size(); i-- > 0;) {
		DWORD Protection;
		VirtualProtect(GLSaved[i].Address, GLSaved[i].Size, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(GLSaved[i].Address, GLSaved[i].Bytes, GLSaved[i].Size);
		VirtualProtect(GLSaved[i].Address, GLSaved[i].Size, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), GLSaved[i].Address, GLSaved[i].Size);
	}
	GLSaved.clear();
}

// The slot of an imported function in difftest.exe's own import table.
static void **OwnImportSlot(const char *Dll, const char *Function)
{
	uint8 *Base = (uint8 *)GetModuleHandleA(NULL);
	void *Real = (void *)GetProcAddress(GetModuleHandleA(Dll), Function);
	IMAGE_NT_HEADERS32 *NT = (IMAGE_NT_HEADERS32 *)(Base + ((IMAGE_DOS_HEADER *)Base)->e_lfanew);
	IMAGE_DATA_DIRECTORY Directory = NT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	for (IMAGE_IMPORT_DESCRIPTOR *D = (IMAGE_IMPORT_DESCRIPTOR *)(Base + Directory.VirtualAddress); D->Name != 0; D++) {
		void **Slot = (void **)(Base + D->FirstThunk);
		for (; *Slot != NULL; Slot++)
			if (*Slot == Real)
				return Slot;
	}
	return NULL;
}

static void PatchWindows()
{
	struct { uint32 ImageSlot; const char *Dll; const char *Function; void *Fake; } Table[] = {
		{0x55b0c4, "gdi32.dll", "SwapBuffers", (void *)FakeSwapBuffers},
		{0x55b0c8, "gdi32.dll", "SetPixelFormat", (void *)FakeSetPixelFormat},
		{0x55b0cc, "gdi32.dll", "ChoosePixelFormat", (void *)FakeChoosePixelFormat},
		{0x55b4b4, "user32.dll", "ReleaseDC", (void *)FakeReleaseDC},
		{0x55b4b8, "user32.dll", "ChangeDisplaySettingsA", (void *)FakeChangeDisplaySettingsA},
		{0x55b4bc, "user32.dll", "EnumDisplaySettingsA", (void *)FakeEnumDisplaySettingsA},
		{0x55b4c0, "user32.dll", "SetRect", (void *)FakeSetRect},
		{0x55b4c4, "user32.dll", "GetDC", (void *)FakeGetDC},
		{0x55b510, "user32.dll", "ClientToScreen", (void *)FakeClientToScreen},
		{0x55b514, "user32.dll", "GetClientRect", (void *)FakeGetClientRect},
		{0x55b1f0, "kernel32.dll", "Sleep", (void *)FakeSleep},
		{0x55b208, "kernel32.dll", "OutputDebugStringA", (void *)FakeOutputDebugStringA},
		{0x55b59c, "winmm.dll", "timeGetTime", (void *)FakeTimeGetTime},
	};
	for (auto &T : Table) {
		Write((void *)(uintptr_t)T.ImageSlot, &T.Fake, 4);
		void **Slot = OwnImportSlot(T.Dll, T.Function);
		if (Slot == NULL) {
			printf("FAIL no import slot for %s in difftest.exe\n", T.Function);
			Failures++;
			continue;
		}
		Write(Slot, &T.Fake, 4);
	}
	// The image's OpenGL and GLU import slots (ours are the GLSlot_ variables).
	struct { uint32 ImageSlot; void *Fake; } GL[] = {
		{0x55b0dc, (void *)ImageOrtho2D},
		{0x55b2e4, (void *)FakeWglGetProcAddress}, {0x55b2e8, (void *)FakeWglGetCurrentDC},
		{0x55b2ec, (void *)FakeWglDeleteContext}, {0x55b2f0, (void *)FakeWglMakeCurrent},
		{0x55b2f4, (void *)FakeWglGetCurrentContext}, {0x55b2f8, (void *)FakeWglShareLists},
		{0x55b2fc, (void *)FakeWglCreateContext}, {0x55b300, (void *)FakeGlReadBuffer},
		{0x55b304, (void *)FakeGlDrawBuffer}, {0x55b308, (void *)FakeGlEnd}, {0x55b30c, (void *)FakeGlVertex2i},
		{0x55b310, (void *)FakeGlTexCoord2f}, {0x55b314, (void *)FakeGlColor3ub}, {0x55b318, (void *)FakeGlBegin},
		{0x55b31c, (void *)FakeGlEnable}, {0x55b320, (void *)FakeGlAlphaFunc}, {0x55b324, (void *)FakeGlViewport},
		{0x55b328, (void *)FakeGlLoadIdentity}, {0x55b32c, (void *)FakeGlMatrixMode},
		{0x55b330, (void *)FakeGlCopyTexSubImage2D}, {0x55b334, (void *)FakeGlBindTexture},
		{0x55b338, (void *)FakeGlTexParameteri}, {0x55b33c, (void *)FakeGlDisable}, {0x55b340, (void *)FakeGlRecti},
		{0x55b344, (void *)FakeGlTexCoord2d}, {0x55b348, (void *)FakeGlTexSubImage2D},
		{0x55b34c, (void *)FakeGlGetTexImage}, {0x55b350, (void *)FakeGlBlendFunc},
		{0x55b354, (void *)FakeGlTexImage2D}, {0x55b358, (void *)FakeGlGenTextures},
		{0x55b35c, (void *)FakeGlDeleteTextures}, {0x55b360, (void *)FakeGlGetString},
	};
	for (auto &T : GL)
		Write((void *)(uintptr_t)T.ImageSlot, &T.Fake, 4);
	Redirect((void *)(uintptr_t)0x497a00, (void *)FakeDrawSkin);
	Redirect((void *)DrawSkin, (void *)FakeDrawSkin);
	Redirect((void *)(uintptr_t)0x4db360, (void *)FakeSetDefaultNoStretching);
	Redirect((void *)SetDefaultNoStretching, (void *)FakeSetDefaultNoStretching);
	Redirect((void *)(uintptr_t)0x4db410, (void *)FakeGetConfineFramerate);
	Redirect((void *)GetConfineFramerate, (void *)FakeGetConfineFramerate);
	// the image's CRT functions glh_init_extensions uses besides malloc and free (PatchImage)
	Redirect((void *)(uintptr_t)0x513aa3, (void *)ImageRealloc);
	Redirect((void *)(uintptr_t)0x51491c, (void *)ImageStrdup);
}

// --- Our statics ------------------------------------------------------------------------------

// SurfaceGL.cpp's statics and glh_extensions.h's strings have no names outside
// their file: they are found once in difftest.exe's writable sections by
// sentinel values the test makes them take (or, for the two strings, in the
// code of glh_shutdown_extensions).
enum {
	ST_FILTER, ST_SURFACE, ST_DIRTY_LEFT, ST_DIRTY_TOP, ST_DIRTY_RIGHT, ST_DIRTY_BOTTOM, ST_PBUFFER, ST_PBUFFER_DC,
	ST_PBUFFER_CONTEXT, ST_MAIN_CONTEXT, ST_MAIN_DC, ST_UNSUPPORTED, ST_SYSEXTS, NUMBER_OF_STATICS
};
static const uint32 ImageStatic[NUMBER_OF_STATICS] = {0x5c257c, 0x5c2580, 0x71d290, 0x71d294, 0x71d298, 0x71d29c,
	0x71d2a0, 0x71d2a4, 0x71d2a8, 0x71d2b4, 0x71d2b8, 0x71dbac, 0x71dbb0};
static const char *const StaticName[NUMBER_OF_STATICS] = {"CurrentFilter", "CurrentSurface", "DirtyRect.left",
	"DirtyRect.top", "DirtyRect.right", "DirtyRect.bottom", "Pbuffer", "PbufferDC", "PbufferContext", "MainContext",
	"MainDC", "unsupportedExts", "sysExts"};
static uint32 *OurStatic[NUMBER_OF_STATICS];

struct TGLRange {
	uint8 *From, *To;
};
static std::vector<TGLRange> WritableSections()
{
	std::vector<TGLRange> Ranges;
	uint8 *Base = (uint8 *)GetModuleHandleA(NULL);
	IMAGE_NT_HEADERS32 *NT = (IMAGE_NT_HEADERS32 *)(Base + ((IMAGE_DOS_HEADER *)Base)->e_lfanew);
	IMAGE_SECTION_HEADER *S = IMAGE_FIRST_SECTION(NT);
	for (int i = 0; i < NT->FileHeader.NumberOfSections; i++)
		if (S[i].Characteristics & IMAGE_SCN_MEM_WRITE)
			Ranges.push_back({Base + S[i].VirtualAddress, Base + S[i].VirtualAddress + S[i].Misc.VirtualSize});
	return Ranges;
}
static bool Excluded(uint8 *P)
{
	return (P >= (uint8 *)&World && P < (uint8 *)(&World + 1)) || (P >= (uint8 *)OurStatic && P < (uint8 *)(OurStatic + NUMBER_OF_STATICS));
}
static uint32 *FindWord(uint32 Value, const char *What)
{
	uint32 *Found = NULL;
	int Count = 0;
	for (TGLRange &R : WritableSections())
		for (uint8 *P = R.From; P + 4 <= R.To; P += 4)
			if (*(uint32 *)P == Value && !Excluded(P)) {
				Found = (uint32 *)P;
				Count++;
			}
	CHECK(Count == 1, "SurfaceGL: our %s found %d times", What, Count);
	return Count == 1 ? Found : NULL;
}
static std::vector<uint32> Snapshot()
{
	std::vector<uint32> Words;
	for (TGLRange &R : WritableSections())
		for (uint8 *P = R.From; P + 4 <= R.To; P += 4)
			Words.push_back(*(uint32 *)P);
	return Words;
}
static uint32 *FindChange(const std::vector<uint32> &Before, uint32 From, uint32 To, const char *What)
{
	uint32 *Found = NULL;
	int Count = 0;
	size_t i = 0;
	for (TGLRange &R : WritableSections())
		for (uint8 *P = R.From; P + 4 <= R.To; P += 4, i++)
			if (Before[i] == From && *(uint32 *)P == To && !Excluded(P)) {
				Found = (uint32 *)P;
				Count++;
			}
	CHECK(Count == 1, "SurfaceGL: our %s found %d times", What, Count);
	return Count == 1 ? Found : NULL;
}
// The first two distinct addresses in our writable sections that the code of
// glh_shutdown_extensions refers to: unsupportedExts, then sysExts.
static void FindGlhStrings()
{
	uint8 *Code = (uint8 *)glh_shutdown_extensions;
	if (Code[0] == 0xe9)
		Code += 5 + *(int32_t *)(Code + 1);
	std::vector<TGLRange> Ranges = WritableSections();
	uint32 *Found[2] = {NULL, NULL};
	// Only the absolute memory operands of the instructions such code uses
	// (mov eax/moffs, mov/cmp/test/push/mov imm with a [disp32] ModRM): any
	// four bytes taken as an address matched a misaligned window once the
	// sections grew (issue #34).
	for (int i = 0; i < 96 && Found[1] == NULL; i++) {
		int Operand = 0;
		if (Code[i] == 0xa1 || Code[i] == 0xa3)
			Operand = i + 1;
		else if ((Code[i] == 0x8b || Code[i] == 0x89 || Code[i] == 0x39 || Code[i] == 0x3b || Code[i] == 0x85
			|| Code[i] == 0x83 || Code[i] == 0xc7 || Code[i] == 0xff) && (Code[i + 1] & 0xc7) == 0x05)
			Operand = i + 2;
		if (Operand == 0)
			continue;
		uint8 *Value = *(uint8 **)(Code + Operand);
		for (TGLRange &R : Ranges)
			if (Value >= R.From && Value + 4 <= R.To && ((uintptr_t)Value & 3) == 0) {
				if (Found[0] == NULL)
					Found[0] = (uint32 *)Value;
				else if ((uint32 *)Value != Found[0])
					Found[1] = (uint32 *)Value;
			}
	}
	CHECK(Found[0] != NULL && Found[1] != NULL, "SurfaceGL: glh strings not found in glh_shutdown_extensions");
	OurStatic[ST_UNSUPPORTED] = Found[0];
	OurStatic[ST_SYSEXTS] = Found[1];
}

// --- Comparison ------------------------------------------------------------------------------

static TSurface *Objects[2];

static std::string Text(const char *Value)
{
	return Value == NULL ? std::string("(null)") : std::string(Value);
}

static std::string Dump(int S)
{
	int Old = Side;
	Side = S;
	std::string Out;
	char Buffer[96];
	uint8 *Object = (uint8 *)Objects[S];
	if (Object != NULL)
		for (int Offset = 4; Offset < 0x498; Offset += 4) {
			sprintf(Buffer, "%x:%x ", Offset, *(uint32 *)(Object + Offset));
			Out += Buffer;
		}
	for (int i = 0; i < NUMBER_OF_STATICS; i++) {
		if (OurStatic[i] == NULL) {
			Out += "|" + std::string(StaticName[i]) + "=?"; // not located yet
		} else if (i == ST_UNSUPPORTED || i == ST_SYSEXTS) {
			const char *Value = S == IMAGE ? Global<const char *>(ImageStatic[i])
				: OurStatic[i] != NULL ? *(const char **)OurStatic[i] : "?";
			Out += "|" + std::string(StaticName[i]) + "=" + Text(Value);
		} else {
			uint32 Value = S == IMAGE ? Global<uint32>(ImageStatic[i]) : OurStatic[i] != NULL ? *OurStatic[i] : 0xdeadbeef;
			sprintf(Buffer, "|%s=%x", StaticName[i], Value);
			Out += Buffer;
		}
	}
	if (S == IMAGE)
		sprintf(Buffer, "|stats %d %d %d %d %d %d %d %d %d %d %d %d %d", Global<int>(0x71d0d0), Global<int>(0x71d0d4),
			Global<int>(0x71d0d8), Global<int>(0x71d0dc), Global<int>(0x71d0e0), Global<int>(0x71d0e4),
			Global<int>(0x71d0e8), Global<int>(0x71d0ec), Global<int>(0x71d0f0), Global<int>(0x71d0f4),
			Global<int>(0x5c2568), Global<int>(0x5c256c), Global<uint8>(0x5d1670));
	else
		sprintf(Buffer, "|stats %d %d %d %d %d %d %d %d %d %d %d %d %d", BlitCount, BlitPixels, Data71D0D8, ScreenWidth,
			ScreenHeight, ScreenDepth, LastBlitCount, LastBlitPixels, NumberOfSurfaces, Data71D0F4, CacheSurfaceWidth,
			CacheSurfaceHeight, (uint8)DisplayChanged);
	Out += Buffer;
	if (Objects[S] != NULL) {
		Out += "|memory";
		for (int i = 0; i < NUMBER_OF_SURFACES_GL; i++) {
			sprintf(Buffer, " %d", Objects[S]->GetSurfaceMemory(i));
			Out += Buffer;
		}
	}
	Side = Old;
	return Out;
}

static const char *What;
static int ExtPointerChecks;
static void Compare()
{
	Checks++;
	if (Log[IMAGE] != Log[OURS]) {
		Failures++;
		if (Failures <= FailuresPrinted) {
			size_t i = 0;
			while (i < Log[IMAGE].size() && i < Log[OURS].size() && Log[IMAGE][i] == Log[OURS][i])
				i++;
			size_t From = i > 160 ? i - 160 : 0;
			printf("FAIL SurfaceGL %s: logs differ at %u\n  image: %s\n  ours:  %s\n", What, (unsigned)i,
				Log[IMAGE].substr(From, 320).c_str(), Log[OURS].substr(From, 320).c_str());
		}
	}
	std::string A = Dump(IMAGE), B = Dump(OURS);
	Checks++;
	if (A != B) {
		Failures++;
		if (Failures <= FailuresPrinted) {
			size_t i = 0;
			while (i < A.size() && i < B.size() && A[i] == B[i])
				i++;
			size_t From = i > 80 ? i - 80 : 0;
			printf("FAIL SurfaceGL %s: state differs at %u\n  image: %s\n  ours:  %s\n", What, (unsigned)i,
				A.substr(From, 240).c_str(), B.substr(From, 240).c_str());
		}
	}
	for (const TGLExtPointer &P : GLExtPointers) {
		ExtPointerChecks++;
		if (Global<void *>(P.Image) != *(void **)P.Ours) {
			Checks++;
			Failures++;
			if (Failures <= FailuresPrinted)
				printf("FAIL SurfaceGL %s: extension pointer %s differs: %p %p\n", What, P.Name, Global<void *>(P.Image),
					*(void **)P.Ours);
			break;
		}
	}
	if (LogFile != NULL)
		fprintf(LogFile, "== %s\n%s\n", What, Log[OURS].c_str());
	Log[IMAGE].clear();
	Log[OURS].clear();
}

// Runs one operation on both sides from the same world.
template <class F> static void Both(const char *Name, F Operation)
{
	What = Name;
	if (Trace)
		printf("  %s\n", Name);
	TGLWorld Start = World;
	Side = IMAGE;
	Operation(Objects[IMAGE]);
	TGLWorld AfterImage = World;
	World = Start;
	Side = OURS;
	Operation(Objects[OURS]);
	Compare();
	Checks++;
	if (memcmp(&AfterImage, &World, sizeof(World)) != 0) {
		Failures++;
		if (Failures <= FailuresPrinted)
			printf("FAIL SurfaceGL %s: the worlds the sides leave differ\n", Name);
	}
}
template <class R, class F> static void BothResult(const char *Name, F Operation)
{
	R Result[2];
	Both(Name, [&](TSurface *S) { Result[Side] = Operation(S); });
	CHECK(memcmp(&Result[0], &Result[1], sizeof(R)) == 0, "SurfaceGL %s: results differ", Name);
}

// The image's functions that are not virtual.
template <class R, class... A> static R ImageMember(uint32 Address, void *This, A... Args)
{
	return ((R (__fastcall *)(void *, void *, A...))(uintptr_t)Address)(This, NULL, Args...);
}
static int GlhInit(const char *Request)
{
	return Side == IMAGE ? At<int (__cdecl *)(const char *)>(0x4f9260)(Request) : glh_init_extensions(Request);
}
static void GlhShutdown()
{
	if (Side == IMAGE)
		At<void (__cdecl *)()>(0x4f9510)();
	else
		glh_shutdown_extensions();
}

// --- Inputs ----------------------------------------------------------------------------------

static TRGBColor RandomColor()
{
	static const TRGBColor Common[] = {TRGBColor(0, 0, 0), TRGBColor(255, 0, 255), TRGBColor(255, 255, 255)};
	if (Chance(40))
		return Common[Rnd() % 3];
	const int B = Between(0, 255), G = Between(0, 255), R = Between(0, 255); // issue #55: right to left
	return TRGBColor(R, G, B);
}
static TRect RandomRect(int Size)
{
	const int Height = Between(-5, Size / 2), Width = Between(-5, Size / 2), Y = Between(-20, Size), X = Between(-20, Size); // issue #55: the arguments right to left
	return TRect(X, Y, Width, Height);
}
static int AnySurface() { return Between(0, NUMBER_OF_SURFACES_GL - 1); }
static int SourceSurface() { return Between(2, NUMBER_OF_SURFACES_GL - 1); } // has a texture (and is not SCREEN_SURFACE)

static void Append(char *Text, size_t Size, const char *Word)
{
	if (strlen(Text) + strlen(Word) + 2 < Size) {
		strcat(Text, Word);
		strcat(Text, " ");
	}
}

// Extension strings: every name with the given chance, the two pbuffer
// extensions when Pbuffer is set, junk sometimes.
static void RandomExtensions(int Percent, bool Pbuffer)
{
	World.Extensions[0] = 0;
	World.WglExtensions[0] = 0;
	for (const char *Name : GLExtensionNames) {
		bool Wgl = strncmp(Name, "WGL_", 4) == 0;
		if (Pbuffer && (strcmp(Name, "WGL_ARB_pbuffer") == 0 || strcmp(Name, "WGL_ARB_pixel_format") == 0))
			Append(World.WglExtensions, sizeof(World.WglExtensions), Name);
		else if (Chance(Percent))
			Append(Wgl ? World.WglExtensions : World.Extensions, Wgl ? sizeof(World.WglExtensions)
				: sizeof(World.Extensions), Name);
		if (Chance(3))
			Append(World.Extensions, sizeof(World.Extensions), "GL_FAKE_extension");
	}
	static const char *const Versions[] = {"1.0", "1.1.0", "1.2", "1.2.1 NVIDIA", "1.3", "1.4.0", "1.5.2", "2.0",
		"2.1.1", "", "0.9", "1.12"};
	strcpy(World.Version, Versions[Rnd() % 12]);
}

static void RandomWorld()
{
	memset(&World, 0, sizeof(World));
	World.Clock = Rnd();
	World.ClockStep = Chance(30) ? 0 : Between(1, 40);
	World.NextHandle = 0x10000 + (Rnd() & 0xff00);
	World.NextTexture = 1 + Between(0, 50);
	World.PixelFormat = Between(1, 20);
	World.SetPixelResult = TRUE;
	World.ShareResult = TRUE;
	World.ChangeResult = DISP_CHANGE_SUCCESSFUL;
	World.ChangeBackResult = DISP_CHANGE_SUCCESSFUL;
	static const int Sizes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {320, 200}, {1280, 1024}, {549, 400}, {550, 400}};
	int Depth = Chance(70) ? 32 : 16;
	World.Desktop[0] = 1280;
	World.Desktop[1] = 1024;
	World.Desktop[2] = Depth;
	World.Desktop[3] = 60;
	World.NumberOfModes = Between(1, 12);
	for (int i = 0; i < World.NumberOfModes; i++) {
		int k = Between(0, 6);
		World.Modes[i][0] = Sizes[k][0];
		World.Modes[i][1] = Sizes[k][1];
		World.Modes[i][2] = Chance(60) ? Depth : Between(8, 32);
		World.Modes[i][3] = Between(50, 100);
	}
	// at least one mode GetFullscreenModes takes (InitDisplay reads before an empty list)
	int k = Between(0, World.NumberOfModes - 1);
	World.Modes[k][0] = Chance(50) ? 640 : 1024;
	World.Modes[k][1] = World.Modes[k][0] == 640 ? 480 : 768;
	World.Modes[k][2] = Depth;
	World.Client = RECT{0, 0, Between(100, 1000), Between(100, 800)};
	World.Origin = POINT{Between(0, 300), Between(0, 300)};
	World.Confine = Chance(30) ? Between(0, 300) : Between(10, 200);
	World.ProcNullPercent = Chance(70) ? 0 : Between(1, 30);
	World.ProcSeed = Rnd();
	World.PbufferFormat = Between(1, 40);
	World.PbufferCount = Between(1, 3);
	World.TexSeed = Rnd();
	RandomExtensions(Between(0, 100), true);
}

static void FillPattern(void *Memory)
{
	uint8 *P = (uint8 *)Memory;
	for (int Offset = 0; Offset < 0x498; Offset++)
		P[Offset] = (uint8)(0xab ^ (Offset * 7));
}

// --- The test --------------------------------------------------------------------------------

static void CreateObjects()
{
	void *Memory[2] = {malloc(0x498), malloc(0x498)};
	FillPattern(Memory[0]);
	FillPattern(Memory[1]);
	Side = IMAGE;
	Objects[IMAGE] = (TSurface *)ImageMember<void *>(0x4fb670, Memory[0]);
	Side = OURS;
	Objects[OURS] = new (Memory[1]) TSurfaceGL;
	What = "constructor";
	Compare();
}

static void DestroyObjects()
{
	bool Deleting = Chance(50);
	What = Deleting ? "scalar deleting destructor" : "destructor";
	Side = IMAGE;
	if (Deleting)
		ImageMember<void *>((*(uint32 **)Objects[IMAGE])[0], Objects[IMAGE], 1);
	else
		ImageMember<void>(0x4f9860, Objects[IMAGE]);
	Side = OURS;
	if (Deleting)
		delete Objects[OURS];
	else
		((TSurfaceGL *)Objects[OURS])->~TSurfaceGL();
	if (!Deleting) {
		free(Objects[IMAGE]);
		free(Objects[OURS]);
	}
	Objects[IMAGE] = Objects[OURS] = NULL;
	Compare();
}

static void InitBoth(bool Force)
{
	static HWND Windows[] = {(HWND)(uintptr_t)0x10010, (HWND)(uintptr_t)0x20020};
	TSurfaceGL *S = (TSurfaceGL *)Objects[OURS];
	HWND Window = Windows[Rnd() % 2];
	int Mode = Chance(50) ? WINDOWED_MODE : FULLSCREEN_MODE;
	if (Chance(20) && S->ViewMode != 0)
		Mode = S->ViewMode;
	TFullscreenMode Resolution;
	static const int Sizes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {333, 222}};
	int k = Between(0, 3);
	Resolution.Width = Sizes[k][0];
	Resolution.Height = Sizes[k][1];
	Resolution.Depth = Chance(70) ? 32 : Between(8, 300);
	Resolution.Frequency = Between(0, 100);
	World.ChangeResult = Chance(15) ? DISP_CHANGE_FAILED : DISP_CHANGE_SUCCESSFUL;
	World.ChangeBackResult = Chance(15) ? DISP_CHANGE_FAILED : DISP_CHANGE_SUCCESSFUL;
	if (Chance(10))
		World.PixelFormat = 0;
	else if (Chance(10))
		World.SetPixelResult = FALSE;
	else if (Chance(10))
		World.MissingWGL = 1 << 4; // wglQueryPbufferARB, the last of its block: the pbuffer still works

	bool Changed = Chance(30);
	DisplayChanged = Changed;
	Global<bool>(0x5d1670) = Changed;
	TFullscreenMode Copies[2] = {Resolution, Resolution};
	BothResult<bool>(Force ? "Init force" : "Init", [&](TSurface *T) {
		return T->Init(&Window, Mode, &Copies[Side], Force);
	});
	CHECK(memcmp(&Copies[0], &Copies[1], sizeof(Resolution)) == 0, "SurfaceGL Init: resolutions differ");
	World.PixelFormat = Between(1, 20);
	World.SetPixelResult = TRUE;
	World.MissingWGL = 0;
	World.ChangeResult = DISP_CHANGE_SUCCESSFUL;
	World.ChangeBackResult = DISP_CHANGE_SUCCESSFUL;
}

static void FreeModes(int S, TFullscreenMode *Modes)
{
	if (Modes == NULL)
		return;
	if (S == IMAGE)
		At<void (__cdecl *)(void *)>(0x523812)(Modes);
	else
		delete[] Modes;
}

static int OperationCounts[48];

static void RandomOperation()
{
	TSurfaceGL *S = (TSurfaceGL *)Objects[OURS];
	bool Ready = S->Context != NULL;
	int Kind = Between(0, Ready ? 45 : 12);
	OperationCounts[Kind]++;
	switch (Kind) {
	case 0:
		InitBoth(Chance(30));
		break;
	case 1:
		if (Ready)
			Both("RestoreSurfaces", [&](TSurface *T) { T->RestoreSurfaces(Between(0, 2), NULL); });
		break;
	case 2: {
		int Limit = Chance(30) ? Between(-5, 300) : Between(10, 200);
		Both("SetFramerateLimit", [&](TSurface *T) { T->SetFramerateLimit(Limit); });
		break;
	}
	case 3:
		Both("ResetFrameTimer", [&](TSurface *T) { T->ResetFrameTimer(); });
		break;
	case 4:
		if (Chance(40)) {
			int Limit = Chance(50) ? 100 : 125;
			Both("SetFramerateLimit", [&](TSurface *T) { T->SetFramerateLimit(Limit); });
			World.ClockStep = 1000 / Limit + Between(-1, 2);
			Both("ResetFrameTimer", [&](TSurface *T) { T->ResetFrameTimer(); });
		} else {
			World.ClockStep = Chance(50) ? Between(0, 5) : Between(0, 1500);
		}
		Both("WaitForNextFrame", [&](TSurface *T) { T->WaitForNextFrame(); });
		break;
	case 5: {
		int Type = Between(0, 2);
		BothResult<double>("GetFPS", [&](TSurface *T) { return T->GetFPS(); });
		BothResult<int>("GetCacheSurfaces", [&](TSurface *T) { return T->GetCacheSurfaces(); });
		BothResult<bool>("IsUnavailable", [&](TSurface *T) { return T->IsUnavailable(); });
		BothResult<bool>("CanShadeRectangles", [&](TSurface *T) { return T->CanShadeRectangles(); });
		BothResult<bool>("TestEngine", [&](TSurface *T) { return T->TestEngine(); });
		BothResult<bool>("SurfaceRecreated", [&](TSurface *T) { return T->SurfaceRecreated(Type); });
		break;
	}
	case 6: {
		int Surface = AnySurface();
		TRect R[2];
		Both("GetSurfaceRect", [&](TSurface *T) { T->GetSurfaceRect(Surface, &R[Side]); });
		CHECK(memcmp(&R[0], &R[1], sizeof(TRect)) == 0, "SurfaceGL GetSurfaceRect differs");
		Both("SetSurfaceRect", [&](TSurface *T) { T->SetSurfaceRect(Surface, 1, 2, 3, 4); });
		break;
	}
	case 7: {
		TRect Clip = RandomRect(300);
		RECT A = {Between(-50, 300), Between(-50, 300), 0, 0};
		A.right = A.left + Between(-10, 200);
		A.bottom = A.top + Between(-10, 200);
		RECT B = {Between(0, 100), Between(0, 100), Between(0, 300), Between(0, 300)};
		RECT Rects[2][2] = {{A, B}, {A, B}};
		BothResult<bool>("ClipRect", [&](TSurface *T) { return T->ClipRect(Clip, &Rects[Side][0], &Rects[Side][1]); });
		CHECK(memcmp(Rects[0], Rects[1], sizeof(Rects[0])) == 0, "SurfaceGL ClipRect rectangles differ");
		break;
	}
	case 8: {
		int Filter = Chance(40) ? GL_LINEAR : Chance(60) ? GL_NEAREST : (int)Rnd();
		Both("SetTextureFilter", [&](TSurface *T) { T->SetTextureFilter(Filter); });
		break;
	}
	case 9:
		// a client area of width 0 makes SelectSurface(SCREEN_SURFACE) ask for it again
		World.Client = RECT{0, 0, Chance(20) ? 0 : Between(1, 1000), Between(0, 800)};
		Both("UpdateWindowRect", [&](TSurface *T) { T->UpdateWindowRect(); });
		World.Client = RECT{0, 0, Between(1, 1000), Between(1, 800)};
		if (Chance(50) && Ready)
			Both("SelectSurface", [&](TSurface *T) { T->SelectSurface(SCREEN_SURFACE); });
		break;
	case 10: {
		TFullscreenMode *Modes[2] = {NULL, NULL};
		int Number[2] = {0, 0};
		Both("GetFullscreenModes", [&](TSurface *T) { Number[Side] = T->GetFullscreenModes(&Modes[Side]); });
		CHECK(Number[0] == Number[1], "SurfaceGL GetFullscreenModes: counts differ");
		if (Number[0] == Number[1])
			CHECK(memcmp(Modes[0], Modes[1], Number[0] * sizeof(TFullscreenMode)) == 0, "SurfaceGL modes differ");
		FreeModes(IMAGE, Modes[0]);
		FreeModes(OURS, Modes[1]);
		break;
	}
	case 11:
		if (Chance(40))
			Both("ExitDisplay", [&](TSurface *T) { T->ExitDisplay(); });
		else if (Chance(50)) {
			Both("ExitSurfaces", [&](TSurface *T) { T->ExitSurfaces(); });
			Both("InitSurfaces", [&](TSurface *T) { T->InitSurfaces(); });
		} else {
			int Mode = Chance(50) ? WINDOWED_MODE : FULLSCREEN_MODE;
			TFullscreenMode GMode = {Chance(50) ? 640 : Between(300, 1300), Chance(50) ? 480 : Between(200, 1100),
				Chance(70) ? 32 : 16, Between(0, 90)};
			TFullscreenMode Copies[2] = {GMode, GMode};
			World.ChangeResult = Chance(20) ? DISP_CHANGE_FAILED : DISP_CHANGE_SUCCESSFUL;
			World.ChangeBackResult = Chance(20) ? DISP_CHANGE_FAILED : DISP_CHANGE_SUCCESSFUL;
			if (Chance(50))
				Both("InitDisplay", [&](TSurface *T) { T->InitDisplay(Mode, &Copies[Side]); });
			else
				Both("ResetDisplay", [&](TSurface *T) { T->ResetDisplay(Mode, &Copies[Side]); });
			CHECK(memcmp(&Copies[0], &Copies[1], sizeof(GMode)) == 0, "SurfaceGL InitDisplay: modes differ");
			World.ChangeResult = DISP_CHANGE_SUCCESSFUL;
			World.ChangeBackResult = DISP_CHANGE_SUCCESSFUL;
		}
		break;
	case 12: {
		int Surface = AnySurface();
		TRGBColor Key = RandomColor();
		Both("SetSourceColorKey", [&](TSurface *T) { T->SetSourceColorKey(Surface, Key); });
		break;
	}
	case 13: {
		int Surface = AnySurface(), x = Between(-5, 700), y = Between(-5, 500);
		TRGBColor C = RandomColor();
		Both("SetPixel", [&](TSurface *T) { T->SetPixel(Surface, x, y, C); });
		break;
	}
	case 14: {
		int Surface = AnySurface();
		TRect R = RandomRect(600);
		TRGBColor C = RandomColor();
		Both("FillRectangle", [&](TSurface *T) { T->FillRectangle(Surface, R, C); });
		break;
	}
	case 15: {
		int Surface = AnySurface();
		TRect R = RandomRect(600), Clip = RandomRect(600);
		TRGBColor C = RandomColor();
		Both("FillRectangleClipped", [&](TSurface *T) { T->FillRectangleClipped(Surface, R, C, Clip); });
		break;
	}
	case 16: {
		int Surface = AnySurface(), x = Between(-5, 600), y = Between(-5, 400), L = Between(-2, 200);
		TRGBColor C = RandomColor();
		if (Chance(50))
			Both("DrawHorizontalLine", [&](TSurface *T) { T->DrawHorizontalLine(Surface, x, y, L, C); });
		else
			Both("DrawVerticalLine", [&](TSurface *T) { T->DrawVerticalLine(Surface, x, y, L, C); });
		break;
	}
	case 17: {
		int Surface = AnySurface(), Line = Between(1, 6);
		TRect R = RandomRect(500), Clip = RandomRect(500);
		TRGBColor C = RandomColor();
		if (Chance(50))
			Both("DrawFrame", [&](TSurface *T) { T->DrawFrame(Surface, R, Line, C); });
		else
			Both("DrawFrameClipped", [&](TSurface *T) { T->DrawFrameClipped(Surface, R, Line, C, Clip); });
		break;
	}
	case 18:
	case 19: {
		int Surface;
		do
			Surface = AnySurface();
		while (Surface == SCREEN_SURFACE);
		int Width = Between(1, 40), Height = Between(1, 40);
		int x = Between(0, 200), y = Between(0, 200);
		TRGBColor Key = S->Textures[Surface].ColorKey;
		std::vector<uint8> Data(Width * Height * 3);
		for (int i = 0; i < Width * Height; i++) {
			bool IsKey = Chance(30);
			Data[i * 3] = IsKey ? (uint8)Key.R : (uint8)Rnd();
			Data[i * 3 + 1] = IsKey ? (uint8)Key.G : (uint8)Rnd();
			Data[i * 3 + 2] = IsKey && !Chance(10) ? (uint8)Key.B : (uint8)Rnd();
		}
		Both("WriteImage", [&](TSurface *T) { T->WriteImage(Surface, x, y, Width, Height, &Data[0]); });
		break;
	}
	case 20:
	case 21: {
		int Surface;
		do
			Surface = AnySurface();
		while (Surface == SCREEN_SURFACE);
		int Width = Chance(70) ? 32 : Between(1, 32), Height = Chance(70) ? 32 : Between(1, 32);
		int x = Between(0, 64), y = Between(0, 64);
		std::vector<uint8> Data(Width * Height * 3), Mask(Width * Height * 3);
		static const uint8 MaskColors[][3] = {{255, 255, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {0, 0, 0},
			{255, 255, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 1}, {0, 1, 255}, {255, 1, 0}, {1, 255, 0}};
		TRGBColor Colors[4];
		for (int i = 0; i < 4; i++)
			Colors[i] = RandomColor();
		TRGBColor Key = S->Textures[Surface].ColorKey;
		for (int i = 0; i < Width * Height; i++) {
			memcpy(&Mask[i * 3], MaskColors[Rnd() % 12], 3);
			int Kind = Between(0, 9);
			for (int k = 0; k < 3; k++) {
				int Value = (int)(Rnd() & 0xff);
				if (Kind < 3)
					Value = k == 0 ? Colors[1].R : k == 1 ? Colors[1].G : Colors[1].B;
				else if (Kind < 5)
					Value = k == 0 ? Key.R : k == 1 ? Key.G : Key.B;
				Data[i * 3 + k] = (uint8)Value;
			}
		}
		bool UseMask = Chance(50);
		Both("WriteMaskedImage", [&](TSurface *T) {
			T->WriteMaskedImage(Surface, x, y, Width, Height, &Data[0], UseMask ? &Mask[0] : NULL, Colors);
		});
		break;
	}
	case 22:
	case 23: {
		int To = AnySurface(), From = SourceSurface(), x = Between(-40, 600), y = Between(-40, 450);
		const int SourceHeight = Between(1, 120), SourceWidth = Between(1, 120), SourceY = Between(0, 300), SourceX = Between(0, 400); // issue #55: the arguments right to left
		TRect Source(SourceX, SourceY, SourceWidth, SourceHeight);
		TRect Clip = RandomRect(500);
		if (Chance(50))
			Both("BlitRect", [&](TSurface *T) { T->BlitRect(To, x, y, From, Source); });
		else
			Both("BlitRectClipped", [&](TSurface *T) {
				T->BlitRectClipped(To, x, y, From, Source, Clip);
			});
		break;
	}
	case 24:
	case 25: {
		int To = AnySurface(), From = SourceSurface(), x = Between(-40, 600), y = Between(-40, 450);
		int FromX = Between(0, 400), FromY = Between(0, 300), Width = Between(1, 120), Height = Between(1, 120);
		TRGBColor Key = RandomColor();
		TRect Clip = RandomRect(500);
		if (Chance(50))
			Both("Blit", [&](TSurface *T) { T->Blit(To, x, y, From, TRect(FromX, FromY, Width, Height), Key); });
		else
			Both("BlitClipped", [&](TSurface *T) {
				T->BlitClipped(To, x, y, From, TRect(FromX, FromY, Width, Height), Key, Clip);
			});
		break;
	}
	case 26:
	case 27: {
		int To = AnySurface(), From = SourceSurface();
		const int DHeight = Between(0, 200), DWidth = Between(0, 200), DY = Between(-40, 450), DX = Between(-40, 600); // issue #55: the arguments right to left
		TRect D(DX, DY, DWidth, DHeight);
		const int SourceHeight = Between(1, 200), SourceWidth = Between(1, 200), SourceY = Between(0, 300), SourceX = Between(0, 400); // issue #55: the arguments right to left
		TRect Source(SourceX, SourceY, SourceWidth, SourceHeight);
		Both("StretchBlitRect", [&](TSurface *T) { T->StretchBlitRect(To, D, From, Source); });
		break;
	}
	case 28:
	case 29: {
		int To = AnySurface(), From = SourceSurface(), x = Between(-40, 600), y = Between(-40, 450);
		int FromX = Between(0, 400), FromY = Between(0, 300), FromWidth = Between(1, 64), FromHeight = Between(1, 64);
		int Width = Chance(30) ? FromWidth : Between(1, 128), Height = Chance(30) ? FromHeight : Between(1, 128);
		TRGBColor Key = RandomColor(), Highlight = RandomColor();
		Both("StretchBlitUnclipped", [&](TSurface *T) {
			T->StretchBlitUnclipped(To, TRect(x, y, Width, Height), From, TRect(FromX, FromY, FromWidth, FromHeight), Key,
				Highlight);
		});
		break;
	}
	case 30:
	case 31:
	case 32: {
		int To = AnySurface(), From = SourceSurface(), x = Between(-40, 600), y = Between(-40, 450);
		int FromX = Between(0, 400), FromY = Between(0, 300), FromWidth = Between(1, 63), FromHeight = Between(1, 63);
		int Width = Chance(25) ? FromWidth : Between(1, 64), Height = Chance(25) ? FromHeight : Between(1, 64);
		TRGBColor Key = RandomColor(), Highlight = RandomColor();
		TRect Clip = RandomRect(500);
		bool Skin = Chance(40);
		S->Flag26 = Skin;
		((TSurfaceGL *)Objects[IMAGE])->Flag26 = Skin;
		World.Near[0] = Highlight.R;
		World.Near[1] = Highlight.G;
		World.Near[2] = Highlight.B;
		Both("StretchBlit", [&](TSurface *T) {
			T->StretchBlit(To, TRect(x, y, Width, Height), From, TRect(FromX, FromY, FromWidth, FromHeight), Key,
				Highlight, Clip);
		});
		break;
	}
	case 33:
	case 34: {
		int Surface = Chance(70) ? TEMP_SURFACE : AnySurface();
		int Count = Chance(10) ? 0 : Between(1, 40);
		std::vector<TRect> Rects(Count + 1);
		std::vector<TRGBColor> Colors(4 * Count + 4);
		for (int i = 0; i < Count; i++) {
			const int Height = Between(0, 64), Width = Between(0, 64), Y = Between(-10, 400), X = Between(-10, 500); // issue #55: the arguments right to left
			Rects[i] = TRect(X, Y, Width, Height);
		}
		for (size_t i = 0; i < Colors.size(); i++)
			Colors[i] = RandomColor();
		Both("ShadeRectangles", [&](TSurface *T) { T->ShadeRectangles(Surface, Count, &Rects[0], &Colors[0]); });
		break;
	}
	case 35:
	case 36:
		World.ClockStep = Between(0, 30);
		Both("FinishFrame", [&](TSurface *T) { T->FinishFrame(); });
		break;
	case 37:
	case 38: {
		int Surface = AnySurface();
		Both("SelectSurface", [&](TSurface *T) { T->SelectSurface(Surface); });
		break;
	}
	case 39:
	case 40: {
		TRect R = RandomRect(700);
		Both("AddDirtyRect", [&](TSurface *T) { T->AddDirtyRect(R); });
		break;
	}
	case 41: {
		int Surface = Between(2, NUMBER_OF_SURFACES_GL - 1), Width = Between(0, 700), Height = Between(0, 700);
		bool Empty = Chance(50);
		if (NumberOfSurfaces >= NUMBER_OF_SURFACES_GL)
			break; // SurfaceCreated[NumberOfSurfaces] would be written past its end

		BothResult<bool>("CreateTexture", [&](TSurface *T) {
			TTextureGL *Texture = &((TSurfaceGL *)T)->Textures[Surface];
			return Side == IMAGE ? ImageMember<bool>(0x4faef0, T, Texture, Width, Height, Empty)
				: ((TSurfaceGL *)T)->CreateTexture(Texture, Width, Height, Empty);
		});
		break;
	}
	case 42: {
		int Surface = Between(0, NUMBER_OF_SURFACES_GL - 1);
		BothResult<int>("GetSurfaceMemory", [&](TSurface *T) { return T->GetSurfaceMemory(Surface); });
		break;
	}
	case 43: {
		// many small blits into one texture surface, then another surface
		int To = Between(2, NUMBER_OF_SURFACES_GL - 1), From = SourceSurface(), n = Between(5, 40);
		Both("BlitRect x n", [&](TSurface *T) {
			for (int i = 0; i < n; i++)
				T->BlitRect(To, i % 50 - 5, i % 37 - 3, From, TRect(i % 13, i % 7, 1 + i % 9, 1 + i % 5));
			T->SelectSurface(SCREEN_SURFACE);
		});
		break;
	}
	default: {
		int Surface = AnySurface(), x = Between(0, 9);
		TRGBColor C = RandomColor();
		Both("FillRectangle", [&](TSurface *T) { T->FillRectangle(Surface, TRect(x, 0, 3, 3), C); });
		break;
	}
	}
}

// glh_init_extensions on random strings and requests.
static void GlhRound()
{
	World.ProcNullPercent = Chance(50) ? 0 : Between(1, 60);
	World.ProcSeed = Rnd();
	World.MissingWGL = Chance(70) ? 0 : (int)(Rnd() & 0x1ff);
	{
		const bool Pbuffer = Chance(50); // issue #55: the arguments right to left
		RandomExtensions(Between(0, 100), Pbuffer);
	}
	Both("glh_shutdown_extensions", [&](TSurface *) { GlhShutdown(); });
	int Requests = Between(1, 6);
	for (int r = 0; r < Requests; r++) {
		char Request[4096] = "";
		static const char *const Blanks[] = {" ", "  ", "\t", "\n", " \t "};
		if (Chance(30))
			strcat(Request, Blanks[Rnd() % 5]);
		if (Chance(40)) // the core versions, answered from the version string
			strcat(Request, "GL_VERSION_1_2 GL_VERSION_1_3\tGL_VERSION_1_4 GL_VERSION_1_5 ");
		int Count = Chance(10) ? 0 : Between(1, 40);
		for (int i = 0; i < Count; i++) {
			const char *Name = Chance(8) ? "GL_NOT_KNOWN" : GLExtensionNames[Rnd() % (sizeof(GLExtensionNames) / sizeof(GLExtensionNames[0]))];
			if (strlen(Request) + strlen(Name) + 4 < sizeof(Request)) {
				strcat(Request, Name);
				if (i + 1 < Count || Chance(50))
					strcat(Request, Blanks[Rnd() % 5]);
			}
		}
		bool Null = Chance(5);
		BothResult<int>("glh_init_extensions", [&](TSurface *) { return GlhInit(Null ? NULL : Request); });
		if (Chance(20)) {
			const bool Pbuffer = Chance(50); // issue #55: the arguments right to left
			RandomExtensions(Between(0, 100), Pbuffer); // used only after the next shutdown
		}
		if (Chance(25))
			Both("glh_shutdown_extensions", [&](TSurface *) { GlhShutdown(); });
	}
	Both("glh_shutdown_extensions", [&](TSurface *) { GlhShutdown(); });
}

// IsOpenGLWorking, failing at a random step.
static void OpenGLRound()
{
	int Step = Between(0, 12);
	World.PixelFormat = Step == 1 ? 0 : Between(1, 20);
	World.SetPixelResult = Step == 2 ? FALSE : TRUE;
	World.MissingWGL = Step == 3 ? 1 << Between(0, 8) : 0;
	RandomExtensions(Between(0, 100), Step != 4);
	World.PbufferCount = Step == 5 ? 0 : Between(1, 3);
	World.PbufferFails = Step == 6;
	World.PbufferDCFails = Step == 7;
	World.CreateContextFails = Step == 8;
	World.ShareResult = Step == 9 ? FALSE : TRUE;
	Both("glh_shutdown_extensions", [&](TSurface *) { GlhShutdown(); });
	HDC DC = H(World.NextHandle++);
	BothResult<bool>("IsOpenGLWorking", [&](TSurface *) {
		return Side == IMAGE ? At<bool (__cdecl *)(HDC)>(0x4f9710)(DC) : IsOpenGLWorking(DC);
	});
	Both("glh_shutdown_extensions", [&](TSurface *) { GlhShutdown(); });
	World.PixelFormat = Between(1, 20);
	World.SetPixelResult = TRUE;
	World.MissingWGL = 0;
	World.PbufferCount = Between(1, 3);
	World.PbufferFails = World.PbufferDCFails = World.CreateContextFails = false;
	World.ShareResult = TRUE;
	RandomExtensions(Between(0, 100), true);
}

// Round 0: finds our statics by the values the operations give them.
static void LocateStatics()
{
	FindGlhStrings();
	RandomWorld();
	World.NextHandle = 0x7a5e3000;
	World.ChangeResult = World.ChangeBackResult = DISP_CHANGE_SUCCESSFUL;
	World.PixelFormat = 1;
	CreateObjects();
	Both("InitSurfaces", [&](TSurface *T) { T->InitSurfaces(); });
	HWND Window = (HWND)(uintptr_t)0x10010;
	TFullscreenMode Resolution = {640, 480, 32, 60};
	DisplayChanged = false;
	Global<bool>(0x5d1670) = false;
	BothResult<bool>("Init (locate)", [&](TSurface *T) { return T->Init(&Window, WINDOWED_MODE, &Resolution, false); });
	OurStatic[ST_MAIN_DC] = FindWord(0x7a5e3000, "MainDC");
	OurStatic[ST_MAIN_CONTEXT] = FindWord(0x7a5e3001, "MainContext");
	OurStatic[ST_PBUFFER] = FindWord(0x7a5e3002, "Pbuffer");
	OurStatic[ST_PBUFFER_DC] = FindWord(0x7a5e3003, "PbufferDC");
	OurStatic[ST_PBUFFER_CONTEXT] = FindWord(0x7a5e3004, "PbufferContext");
	Both("SetTextureFilter (locate)", [&](TSurface *T) { T->SetTextureFilter(0x7a5e4001); });
	OurStatic[ST_FILTER] = FindWord(0x7a5e4001, "CurrentFilter");
	for (int s = 0; s < 2; s++)
		((TSurfaceGL *)Objects[s])->Rect[9] = RECT{0x7a5e5001, 0x7a5e5002, 0x7a5e5003, 0x7a5e5004};
	std::vector<uint32> Before = Snapshot();
	Both("SelectSurface (locate)", [&](TSurface *T) { T->SelectSurface(9); });
	OurStatic[ST_SURFACE] = FindChange(Before, SCREEN_SURFACE, 9, "CurrentSurface");
	OurStatic[ST_DIRTY_LEFT] = FindWord(0x7a5e5003, "DirtyRect.left");
	OurStatic[ST_DIRTY_TOP] = FindWord(0x7a5e5004, "DirtyRect.top");
	OurStatic[ST_DIRTY_RIGHT] = FindWord(0x7a5e5001, "DirtyRect.right");
	OurStatic[ST_DIRTY_BOTTOM] = FindWord(0x7a5e5002, "DirtyRect.bottom");
	for (int s = 0; s < 2; s++)
		((TSurfaceGL *)Objects[s])->Rect[9] = RECT{0, 0, 256, 256};
	What = "locate";
	Compare();
	Both("ExitSurfaces", [&](TSurface *T) { T->ExitSurfaces(); });
	DestroyObjects();
}

static void RunRound(int Round)
{
	RandomWorld();
	if (Round % 5 == 1) {
		GlhRound();
		return;
	}
	if (Round % 5 == 3)
		OpenGLRound();
	CreateObjects();
	Both("InitSurfaces", [&](TSurface *T) { T->InitSurfaces(); });
	InitBoth(Chance(20));
	int Operations = Between(20, 80);
	for (int i = 0; i < Operations; i++)
		RandomOperation();
	Both("ExitSurfaces", [&](TSurface *T) { T->ExitSurfaces(); });
	DestroyObjects();
}

// --- Error paths (issue #34) ---------------------------------------------------------------

// Both sides as Both does, each in ImageThrow/OurThrow: what they threw, then
// the logs, the objects and the worlds.
template <class F> static void BothThrown(TErrorRound &R, const char *Name, F Operation)
{
	TThrown Thrown[2];
	Both(Name, [&](TSurface *T) {
		if (Side == IMAGE)
			Thrown[IMAGE] = ImageThrow([&] { Operation(T); });
		else
			Thrown[OURS] = OurThrow([&] { Operation(T); });
	});
	R.Compare(Name, Thrown[OURS], Thrown[IMAGE]);
}

// PROGRAM_ERROR 12 (no pbuffer: CreateSurfaces through Init, and through
// RestoreSurfaces' catch) and the drawing functions' assertions. The test's
// own random numbers (Rnd) are used after its counted rounds.
static void TestSurfaceGLErrors()
{
	TErrorRound R("SurfaceGL");
	for (int Round = 0; Round < 80; Round++) {
		RandomWorld();
		World.ChangeResult = World.ChangeBackResult = DISP_CHANGE_SUCCESSFUL;
		World.PixelFormat = Between(1, 20);
		CreateObjects();
		Both("InitSurfaces", [&](TSurface *T) { T->InitSurfaces(); });
		HWND Window = (HWND)(uintptr_t)0x10010;
		int Mode = Chance(50) ? WINDOWED_MODE : FULLSCREEN_MODE;
		TFullscreenMode Copies[2];
		for (int s = 0; s < 2; s++) {
			Copies[s].Width = 640;
			Copies[s].Height = 480;
			Copies[s].Depth = 32;
			Copies[s].Frequency = 60;
		}
		int Case = Round % 3;
		if (Case == 0) {
			if (Chance(50))
				World.PbufferFails = true;
			else
				World.PbufferCount = 0;
		}
		BothThrown(R, "Init", [&](TSurface *T) { T->Init(&Window, Mode, &Copies[Side], true); });
		World.PbufferFails = false;
		World.PbufferCount = Between(1, 3);
		if (Case == 1) {
			World.PbufferFails = true;
			BothThrown(R, "RestoreSurfaces", [&](TSurface *T) { T->RestoreSurfaces(Mode, &Copies[Side]); });
			World.PbufferFails = false;
		}
		if (Case == 2) {
			static uint8 Data[64 * 64 * 4], Mask[64 * 64 * 4];
			static TRGBColor Colors[8];
			TRGBColor Key(1, 2, 3);
			int Bad = Chance(50) ? Between(-5, -1) : Between(NUMBER_OF_SURFACES_GL, NUMBER_OF_SURFACES_GL + 5);
			int Good = Between(0, NUMBER_OF_SURFACES_GL - 1);
			int x = Chance(50) ? Between(-5, -1) : Between(0, 40), y = Chance(30) ? -1 : Between(0, 40);
			int w = Chance(50) ? Between(-2, 0) : Between(1, 20), h = Chance(30) ? 0 : Between(1, 20);
			const int EmptyHeight = Between(1, 5), EmptyWidth = Between(-3, 0), EmptyY = Between(0, 5), EmptyX = Between(0, 5); // issue #55: the arguments right to left
			TRect Empty(EmptyX, EmptyY, EmptyWidth, EmptyHeight);
			if (Chance(50))
				std::swap(Empty.width, Empty.height);
			const int SomeHeight = Between(1, 30), SomeWidth = Between(1, 30), SomeY = Between(0, 5), SomeX = Between(0, 5); // issue #55: the arguments right to left
			TRect Some(SomeX, SomeY, SomeWidth, SomeHeight);
			BothThrown(R, "WriteImage screen", [&](TSurface *T) { T->WriteImage(SCREEN_SURFACE, 0, 0, 4, 4, Data); });
			BothThrown(R, "WriteImage area", [&](TSurface *T) { T->WriteImage(SPRITE_SURFACE, x, y, w, h, Data); });
			BothThrown(R, "WriteMaskedImage screen", [&](TSurface *T) {
				T->WriteMaskedImage(SCREEN_SURFACE, 0, 0, 4, 4, Data, Mask, Colors);
			});
			BothThrown(R, "WriteMaskedImage area", [&](TSurface *T) {
				T->WriteMaskedImage(SPRITE_SURFACE, x, y, w, h, Data, Mask, Colors);
			});
			BothThrown(R, "BlitRectClipped screen", [&](TSurface *T) {
				T->BlitRectClipped(Good, 1, 1, SCREEN_SURFACE, Some, TRect(0, 0, 800, 600));
			});
			BothThrown(R, "Blit", [&](TSurface *T) { T->Blit(SCREEN_SURFACE, 1, 1, Good, Empty, Key); });
			BothThrown(R, "BlitRect", [&](TSurface *T) { T->BlitRect(SCREEN_SURFACE, 1, 1, Good, Empty); });
			BothThrown(R, "GetSurfaceRect", [&](TSurface *T) { TRect Out; T->GetSurfaceRect(Bad, &Out); });
			int Type = Chance(50) ? Between(-3, -1) : Between(3, 6);
			BothThrown(R, "SurfaceRecreated", [&](TSurface *T) { T->SurfaceRecreated(Type); });
		}
		Both("ExitSurfaces", [&](TSurface *T) { T->ExitSurfaces(); });
		DestroyObjects();
	}
	R.Finish();
}

void TestSurfaceGL()
{
	PatchImage();
	PatchWindows();
	Trace = getenv("GL_TRACE") != NULL;
	const char *LogName = getenv("GL_LOGS");
	LogFile = LogName != NULL ? fopen(LogName, "w") : NULL;
	int Before = Checks;
	// the statistics the other engines' tests leave
	BlitCount = BlitPixels = Data71D0D8 = ScreenWidth = ScreenHeight = ScreenDepth = 0;
	LastBlitCount = LastBlitPixels = NumberOfSurfaces = Data71D0F4 = 0;
	for (uint32 a = 0x71d0d0; a <= 0x71d0f4; a += 4)
		Global<int>(a) = 0;
	CacheSurfaceWidth = CacheSurfaceHeight = Global<int>(0x5c2568) = Global<int>(0x5c256c) = 256;
	LocateStatics();
	int Rounds = getenv("GL_ROUNDS") != NULL ? atoi(getenv("GL_ROUNDS")) : 300;
	for (int Round = 0; Round < Rounds; Round++) {
		if (Trace)
			printf("round %d\n", Round);
		RunRound(Round);
	}
	int Least = 1 << 30;
	for (int k = 0; k <= 45; k++)
		Least = (std::min)(Least, OperationCounts[k]);
	printf("SurfaceGL: %d checks, %d failures (each operation at least %d times, %d extension pointer comparisons)\n",
		Checks - Before, Failures, Least, ExtPointerChecks);
	TestSurfaceGLErrors();
	RestoreAll();
	if (LogFile != NULL)
		fclose(LogFile);
}
