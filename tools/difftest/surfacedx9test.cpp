// surfaceDX9.cpp against the original (0x4efd10-0x4f46ef).
//
// Nothing reaches Windows, d3d9.dll or ddraw.dll: the Windows functions the
// surface calls (LoadLibraryA, GetProcAddress, FreeLibrary, SetRect,
// GetClientRect, ClientToScreen, GetSystemMetrics, timeGetTime, Sleep and
// DirectDrawCreate) are fakes on both sides. The image calls them through its
// import table, whose slots get the fakes' addresses; ours call them through
// difftest.exe's import table, whose slots are patched while the test runs
// (ddraw.lib is linked for DirectDrawCreate). LoadLibraryA and GetProcAddress pass
// every other name to the real functions.
//
// The Direct3D 9 and DirectDraw 4 objects are fakes too: plain objects with
// vtables in the COM method order, one set per side (created by the fake
// Direct3DCreate9, CreateDevice, CreateTexture, ...). They log every call with
// its arguments (objects by kind and number), keep pixel memory for textures,
// surfaces and vertex buffers (LockRect/Lock hand it out, ColorFill,
// UpdateSurface and GetRenderTargetData work on it) and log a hash of the
// vertices each DrawPrimitive draws. Each round creates a surface on both
// sides (the image's factory 0x4f1430 and CreateSurfaceDX9), runs
// InitSurfaces and Init, then random operations through the vtable (the
// image's object is called through its own vtable); after each, the logs, a
// dump of both objects (every member word, fake objects by number), the
// contents of every fake object and Surface.cpp's frame statistics are
// compared. The error paths (issue #34, TestSurfaceDX9Errors after the
// counted checks): one creation call fails, the desktop depth or d3d9.dll,
// and the drawing functions' assertions, compared through imageeh.h.
//
// DIFFTEST_ONLY=SurfaceDX9 runs it alone (DX9_TRACE=1 traces, DX9_LOGS=<file>
// writes every compared log).

#include "difftest.h"
#include "imageeh.h"
#include "../../src/surfaceDX9.h"
#include "../../src/GUI.h"
#include "../../src/Preferences.h"
#include <string>
#include <vector>
#include <utility>
#include <stdarg.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

#pragma warning(disable: 4100)


static_assert(sizeof(TSurfaceDX9) == 0x2c0, "TSurfaceDX9 size");
static_assert(offsetof(TSurfaceDX9, Ready) == 0x70, "Ready");
static_assert(offsetof(TSurfaceDX9, Rect) == 0x74, "Rect");
static_assert(offsetof(TSurfaceDX9, WindowRect) == 0x1d4, "WindowRect");
static_assert(offsetof(TSurfaceDX9, Direct3D) == 0x1f4, "Direct3D");
static_assert(offsetof(TSurfaceDX9, Surface9) == 0x200, "Surface9");
static_assert(offsetof(TSurfaceDX9, RenderTarget) == 0x258, "RenderTarget");
static_assert(offsetof(TSurfaceDX9, ScreenTexture) == 0x2bc, "ScreenTexture");

enum { IMAGE = 0, OURS = 1 };
static int Side;
static std::string Log[2];
static bool Trace;
static FILE *LogFile;

static void Logf(const char *Format, ...)
{
	char Buffer[512];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

// --- The world the fakes answer from ---------------------------------------------------------

struct TDX9Mode {
	int Width, Height, Depth, Frequency;
	DWORD Flags, PixelFlags;
};

struct TDX9World {
	uint32 Clock;
	uint32 ClockStep;
	HRESULT Cooperative;
	DWORD FXCaps;
	HRESULT CapsResult;
	int DesktopDepth;
	HRESULT EnumResult;
	int NumberOfModes;
	TDX9Mode Modes[8];
	RECT Client;
	POINT Origin;
	int ScreenWidth, ScreenHeight;
	int Confine;
	HRESULT AlphaResult;
	int FailCall; // the error round (issue #34): the FailCall-th creation call fails (0: none)
	int Calls;
};
static TDX9World World;
// DirectDrawCreate, QueryInterface, Direct3DCreate9, CreateDevice and
// CreateTexture count their calls; the one World.FailCall names fails.
static bool Failing(const char *What)
{
	if (World.FailCall == 0 || ++World.Calls != World.FailCall)
		return false;
	Logf("%s fails;", What);
	return true;
}

// --- Fake COM objects ------------------------------------------------------------------------

struct TDX9Fake {
	void **Vtbl;
	char Kind;           // 'D' Direct3D, 'V' device, 'T' texture, 'S' surface, 'B' vertex buffer, 'O' DirectDraw, '7' DirectDraw 7
	int Id;
	int Refs;
	int Width, Height, Pitch;
	std::vector<uint8> Memory; // pixels (with margins) or vertices
	uint8 *Bits;
	TDX9Fake *Level;     // a texture's level 0
	D3DPRESENT_PARAMETERS Parameters;
	TDX9Fake *Stream;
	int Stride;
	TDX9Fake *Owner;     // the texture whose memory a level surface shares
	bool Dirty;          // the memory may have changed since Hashed was computed
	uint32 Hashed;
};
static std::vector<TDX9Fake *> Fakes[2];
static const int MARGIN = 8; // rows of spare memory above and below a surface

static TDX9Fake *NewFake(char Kind, void **Vtbl)
{
	TDX9Fake *F = new TDX9Fake();
	F->Vtbl = Vtbl;
	F->Kind = Kind;
	F->Id = (int)Fakes[Side].size();
	F->Refs = 1;
	Fakes[Side].push_back(F);
	return F;
}

static void SetPixels(TDX9Fake *F, int Width, int Height)
{
	F->Width = Width;
	F->Height = Height;
	F->Pitch = Width * 4 + 32;
	F->Memory.resize((size_t)(Height + 2 * MARGIN) * F->Pitch);
	for (size_t i = 0; i < F->Memory.size(); i++)
		F->Memory[i] = (uint8)(i * 7 + F->Id * 13);
	F->Bits = &F->Memory[MARGIN * F->Pitch];
	F->Dirty = true;
}

static void Touch(TDX9Fake *F)
{
	if (F->Owner != NULL)
		F = F->Owner;
	F->Dirty = true;
}

static const char *Name(void *Object)
{
	static char Buffer[8][16];
	static int Next;
	char *B = Buffer[Next++ % 8];
	if (Object == NULL)
		return "0";
	TDX9Fake *F = (TDX9Fake *)Object;
	for (TDX9Fake *G : Fakes[Side])
		if (G == F) {
			sprintf(B, "%c%d", F->Kind, F->Id);
			return B;
		}
	sprintf(B, "?%p", Object);
	return B;
}

static const char *RectText(const RECT *R)
{
	static char Buffer[4][48];
	static int Next;
	char *B = Buffer[Next++ % 4];
	if (R == NULL)
		return "null";
	sprintf(B, "(%ld,%ld,%ld,%ld)", R->left, R->top, R->right, R->bottom);
	return B;
}

static uint32 Hash(const uint8 *Data, size_t Size)
{
	uint32 H = 2166136261u;
	size_t i = 0;
	for (; i + 4 <= Size; i += 4)
		H = (H ^ *(const uint32 *)(Data + i)) * 16777619u;
	for (; i < Size; i++)
		H = (H ^ Data[i]) * 16777619u;
	return H;
}

// Traps for the slots no fake implements.
template <int N> static void __stdcall Trap()
{
	printf("FAIL unexpected COM call, slot +0x%x\n", N * 4);
	fflush(stdout);
	abort();
}
template <int... N> static void MakeTraps(void **Table, std::integer_sequence<int, N...>)
{
	void *Traps[] = {(void *)&Trap<N>...};
	for (size_t i = 0; i < sizeof...(N); i++)
		Table[i] = Traps[i];
}
static const int SLOTS = 120;

static ULONG __stdcall FakeAddRef(TDX9Fake *This) { Logf("%s.AddRef;", Name(This)); return ++This->Refs; }
static ULONG __stdcall FakeRelease(TDX9Fake *This) { Logf("%s.Release;", Name(This)); return --This->Refs; }

static void **Direct3DVtbl, **DeviceVtbl, **TextureVtbl, **SurfaceVtbl, **BufferVtbl, **DirectDrawVtbl, **DirectDraw7Vtbl;

static TDX9Fake *NewSurface(int Width, int Height)
{
	TDX9Fake *F = NewFake('S', SurfaceVtbl);
	SetPixels(F, Width, Height);
	return F;
}

// IDirect3D9
static HRESULT __stdcall D3DCreateDevice(TDX9Fake *This, UINT Adapter, D3DDEVTYPE Type, HWND Window, DWORD Flags,
	D3DPRESENT_PARAMETERS *P, TDX9Fake **Device)
{
	if (Failing("CreateDevice")) {
		*Device = NULL;
		return D3DERR_NOTAVAILABLE;
	}
	TDX9Fake *F = NewFake('V', DeviceVtbl);
	F->Parameters = *P;
	*Device = F;
	Logf("%s.CreateDevice(%u,%d,%p,%lx,{%u,%u,%d,%u,%d,%lu,%d,%p,%d,%d,%d,%lx,%u,%x})=%s;", Name(This), Adapter, Type,
		Window, Flags, P->BackBufferWidth, P->BackBufferHeight, P->BackBufferFormat, P->BackBufferCount,
		P->MultiSampleType, P->MultiSampleQuality, P->SwapEffect, P->hDeviceWindow, P->Windowed,
		P->EnableAutoDepthStencil, P->AutoDepthStencilFormat, P->Flags, P->FullScreen_RefreshRateInHz,
		P->PresentationInterval, Name(F));
	return D3D_OK;
}

// IDirect3DDevice9
static HRESULT __stdcall DevTestCooperativeLevel(TDX9Fake *This)
{
	Logf("%s.TestCooperativeLevel;", Name(This));
	return World.Cooperative;
}
static HRESULT __stdcall DevReset(TDX9Fake *This, D3DPRESENT_PARAMETERS *P)
{
	Logf("%s.Reset(%08x);", Name(This), Hash((uint8 *)P, sizeof(*P)));
	return D3D_OK;
}
static HRESULT __stdcall DevPresent(TDX9Fake *This, const RECT *Source, const RECT *Dest, HWND Window, void *Region)
{
	Logf("%s.Present(%s,%s,%p,%p);", Name(This), RectText(Source), RectText(Dest), Window, Region);
	return D3D_OK;
}
static HRESULT __stdcall DevGetBackBuffer(TDX9Fake *This, UINT Chain, UINT Buffer, D3DBACKBUFFER_TYPE Type,
	TDX9Fake **Surface)
{
	TDX9Fake *F = NewSurface(This->Parameters.BackBufferWidth, This->Parameters.BackBufferHeight);
	*Surface = F;
	Logf("%s.GetBackBuffer(%u,%u,%d)=%s;", Name(This), Chain, Buffer, Type, Name(F));
	return D3D_OK;
}
static HRESULT __stdcall DevCreateTexture(TDX9Fake *This, UINT Width, UINT Height, UINT Levels, DWORD Usage,
	D3DFORMAT Format, D3DPOOL Pool, TDX9Fake **Texture, HANDLE *Shared)
{
	if (Failing("CreateTexture")) {
		*Texture = NULL;
		return D3DERR_OUTOFVIDEOMEMORY;
	}
	TDX9Fake *F = NewFake('T', TextureVtbl);
	SetPixels(F, Width, Height);
	*Texture = F;
	Logf("%s.CreateTexture(%u,%u,%u,%lx,%d,%d,%p)=%s;", Name(This), Width, Height, Levels, Usage, Format, Pool,
		Shared, Name(F));
	return D3D_OK;
}
static HRESULT __stdcall DevCreateVertexBuffer(TDX9Fake *This, UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
	TDX9Fake **Buffer, HANDLE *Shared)
{
	TDX9Fake *F = NewFake('B', BufferVtbl);
	F->Memory.assign(Length + 64, 0);
	F->Bits = &F->Memory[0];
	F->Dirty = true;
	*Buffer = F;
	Logf("%s.CreateVertexBuffer(%u,%lx,%lx,%d,%p)=%s;", Name(This), Length, Usage, FVF, Pool, Shared, Name(F));
	return D3D_OK;
}
static void CopyPixels(TDX9Fake *To, int ToX, int ToY, TDX9Fake *From, int FromX, int FromY, int Width, int Height)
{
	for (int y = 0; y < Height; y++)
		for (int x = 0; x < Width; x++) {
			if (FromX + x < 0 || FromX + x >= From->Width || FromY + y < 0 || FromY + y >= From->Height)
				continue;
			if (ToX + x < 0 || ToX + x >= To->Width || ToY + y < 0 || ToY + y >= To->Height)
				continue;
			memcpy(&To->Bits[(ToY + y) * To->Pitch + (ToX + x) * 4],
				&From->Bits[(FromY + y) * From->Pitch + (FromX + x) * 4], 4);
		}
}
static HRESULT __stdcall DevUpdateSurface(TDX9Fake *This, TDX9Fake *From, const RECT *Rect, TDX9Fake *To,
	const POINT *Point)
{
	Logf("%s.UpdateSurface(%s,%s,%s,%p);", Name(This), Name(From), RectText(Rect), Name(To), Point);
	Touch(To);
	RECT R = Rect != NULL ? *Rect : RECT{0, 0, From->Width, From->Height};
	CopyPixels(To, Point ? Point->x : R.left, Point ? Point->y : R.top, From, R.left, R.top, R.right - R.left,
		R.bottom - R.top);
	return D3D_OK;
}
static HRESULT __stdcall DevGetRenderTargetData(TDX9Fake *This, TDX9Fake *From, TDX9Fake *To)
{
	Logf("%s.GetRenderTargetData(%s,%s);", Name(This), Name(From), Name(To));
	Touch(To);
	CopyPixels(To, 0, 0, From, 0, 0, To->Width, To->Height);
	return D3D_OK;
}
static HRESULT __stdcall DevColorFill(TDX9Fake *This, TDX9Fake *Surface, const RECT *Rect, D3DCOLOR Color)
{
	Logf("%s.ColorFill(%s,%s,%08lx);", Name(This), Name(Surface), RectText(Rect), Color);
	if (Surface == NULL)
		return D3D_OK;
	Touch(Surface);
	RECT R = Rect != NULL ? *Rect : RECT{0, 0, Surface->Width, Surface->Height};
	for (int y = R.top; y < R.bottom; y++)
		for (int x = R.left; x < R.right; x++)
			if (x >= 0 && x < Surface->Width && y >= 0 && y < Surface->Height)
				memcpy(&Surface->Bits[y * Surface->Pitch + x * 4], &Color, 4);
	return D3D_OK;
}
static HRESULT __stdcall DevCreateOffscreenPlainSurface(TDX9Fake *This, UINT Width, UINT Height, D3DFORMAT Format,
	D3DPOOL Pool, TDX9Fake **Surface, HANDLE *Shared)
{
	TDX9Fake *F = NewSurface(Width, Height);
	*Surface = F;
	Logf("%s.CreateOffscreenPlainSurface(%u,%u,%d,%d,%p)=%s;", Name(This), Width, Height, Format, Pool, Shared,
		Name(F));
	return D3D_OK;
}
static HRESULT __stdcall DevSetRenderTarget(TDX9Fake *This, DWORD Index, TDX9Fake *Surface)
{
	Logf("%s.SetRenderTarget(%lu,%s);", Name(This), Index, Name(Surface));
	return D3D_OK;
}
static HRESULT __stdcall DevBeginScene(TDX9Fake *This) { Logf("%s.BeginScene;", Name(This)); return D3D_OK; }
static HRESULT __stdcall DevEndScene(TDX9Fake *This) { Logf("%s.EndScene;", Name(This)); return D3D_OK; }
static HRESULT __stdcall DevSetRenderState(TDX9Fake *This, D3DRENDERSTATETYPE State, DWORD Value)
{
	Logf("%s.SetRenderState(%d,%lu);", Name(This), State, Value);
	return State == D3DRS_ALPHABLENDENABLE ? World.AlphaResult : D3D_OK;
}
static HRESULT __stdcall DevSetTexture(TDX9Fake *This, DWORD Stage, TDX9Fake *Texture)
{
	Logf("%s.SetTexture(%lu,%s);", Name(This), Stage, Name(Texture));
	return D3D_OK;
}
static HRESULT __stdcall DevSetTextureStageState(TDX9Fake *This, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	Logf("%s.SetTextureStageState(%lu,%d,%lu);", Name(This), Stage, Type, Value);
	return D3D_OK;
}
static HRESULT __stdcall DevSetSamplerState(TDX9Fake *This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	Logf("%s.SetSamplerState(%lu,%d,%lu);", Name(This), Sampler, Type, Value);
	return D3D_OK;
}
static HRESULT __stdcall DevDrawPrimitive(TDX9Fake *This, D3DPRIMITIVETYPE Type, UINT Start, UINT Count)
{
	uint32 H = 0;
	if (This->Stream != NULL) {
		size_t Size = (size_t)Count * 3 * This->Stride;
		if (Size > This->Stream->Memory.size())
			Size = This->Stream->Memory.size();
		H = Hash(This->Stream->Bits, Size);
	}
	Logf("%s.DrawPrimitive(%d,%u,%u,%s:%08x);", Name(This), Type, Start, Count, Name(This->Stream), H);
	return D3D_OK;
}
static HRESULT __stdcall DevSetFVF(TDX9Fake *This, DWORD FVF) { Logf("%s.SetFVF(%lx);", Name(This), FVF); return D3D_OK; }
static HRESULT __stdcall DevSetStreamSource(TDX9Fake *This, UINT Stream, TDX9Fake *Buffer, UINT Offset, UINT Stride)
{
	Logf("%s.SetStreamSource(%u,%s,%u,%u);", Name(This), Stream, Name(Buffer), Offset, Stride);
	This->Stream = Buffer;
	This->Stride = Stride;
	return D3D_OK;
}

// IDirect3DTexture9 and IDirect3DSurface9
static HRESULT LockPixels(TDX9Fake *F, D3DLOCKED_RECT *Locked, const RECT *Rect, DWORD Flags)
{
	Touch(F);
	Locked->Pitch = F->Pitch;
	Locked->pBits = F->Bits + (Rect != NULL ? Rect->top * F->Pitch + Rect->left * 4 : 0);
	Logf("%s.LockRect(%s,%lx);", Name(F), RectText(Rect), Flags);
	return D3D_OK;
}
static HRESULT __stdcall TexGetSurfaceLevel(TDX9Fake *This, UINT Level, TDX9Fake **Surface)
{
	if (This->Level == NULL) {
		TDX9Fake *F = NewFake('S', SurfaceVtbl);
		F->Width = This->Width;
		F->Height = This->Height;
		F->Pitch = This->Pitch;
		F->Bits = This->Bits; // shares the texture's memory
		F->Owner = This;
		This->Level = F;
	}
	*Surface = This->Level;
	Logf("%s.GetSurfaceLevel(%u)=%s;", Name(This), Level, Name(This->Level));
	return D3D_OK;
}
static HRESULT __stdcall TexLockRect(TDX9Fake *This, UINT Level, D3DLOCKED_RECT *Locked, const RECT *Rect, DWORD Flags)
{
	return LockPixels(This, Locked, Rect, Flags);
}
static HRESULT __stdcall TexUnlockRect(TDX9Fake *This, UINT Level)
{
	Logf("%s.UnlockRect(%u):%08x;", Name(This), Level, Hash(&This->Memory[0], This->Memory.size()));
	return D3D_OK;
}
static HRESULT __stdcall SurfLockRect(TDX9Fake *This, D3DLOCKED_RECT *Locked, const RECT *Rect, DWORD Flags)
{
	return LockPixels(This, Locked, Rect, Flags);
}
static HRESULT __stdcall SurfUnlockRect(TDX9Fake *This)
{
	Logf("%s.UnlockRect;", Name(This));
	return D3D_OK;
}

// IDirect3DVertexBuffer9
static HRESULT __stdcall BufLock(TDX9Fake *This, UINT Offset, UINT Size, void **Data, DWORD Flags)
{
	Touch(This);
	*Data = This->Bits + Offset;
	Logf("%s.Lock(%u,%u,%lx);", Name(This), Offset, Size, Flags);
	return D3D_OK;
}
static HRESULT __stdcall BufUnlock(TDX9Fake *This)
{
	Logf("%s.Unlock;", Name(This));
	return D3D_OK;
}

// IDirectDraw and IDirectDraw7
static HRESULT __stdcall DDQueryInterface(TDX9Fake *This, REFIID Id, void **Object)
{
	if (Failing("QueryInterface")) {
		*Object = NULL;
		return E_NOINTERFACE;
	}
	TDX9Fake *F = NewFake('7', DirectDraw7Vtbl);
	*Object = F;
	Logf("%s.QueryInterface(%08x)=%s;", Name(This), Hash((const uint8 *)&Id, sizeof(IID)), Name(F));
	return DD_OK;
}
static HRESULT __stdcall DD7EnumDisplayModes(TDX9Fake *This, DWORD Flags, DDSURFACEDESC2 *Filter, void *Context,
	LPDDENUMMODESCALLBACK2 Callback)
{
	Logf("%s.EnumDisplayModes(%lx,%p);", Name(This), Flags, Filter);
	for (int i = 0; i < World.NumberOfModes; i++) {
		DDSURFACEDESC2 D;
		memset(&D, 0x5a, sizeof(D));
		const TDX9Mode &M = World.Modes[i];
		D.dwSize = sizeof(D);
		D.dwFlags = M.Flags;
		D.dwWidth = M.Width;
		D.dwHeight = M.Height;
		D.dwRefreshRate = M.Frequency;
		D.ddpfPixelFormat.dwFlags = M.PixelFlags;
		D.ddpfPixelFormat.dwRGBBitCount = M.Depth;
		HRESULT Result = Callback(&D, Context);
		Logf("cb=%ld;", Result);
	}
	return World.EnumResult;
}
static HRESULT __stdcall DD7GetCaps(TDX9Fake *This, DDCAPS *Driver, DDCAPS *Emulation)
{
	Logf("%s.GetCaps(%lu,%lu,%08x,%08x);", Name(This), Driver->dwSize, Emulation->dwSize,
		Hash((uint8 *)Driver, sizeof(DDCAPS)), Hash((uint8 *)Emulation, sizeof(DDCAPS)));
	Driver->dwFXCaps = World.FXCaps;
	return World.CapsResult;
}
static HRESULT __stdcall DD7GetDisplayMode(TDX9Fake *This, DDSURFACEDESC2 *Description)
{
	Logf("%s.GetDisplayMode(%lu);", Name(This), Description->dwSize);
	Description->ddpfPixelFormat.dwRGBBitCount = World.DesktopDepth;
	return DD_OK;
}

static void **MakeVtbl(std::initializer_list<std::pair<int, void *>> Slots)
{
	void **Table = new void *[SLOTS];
	MakeTraps(Table, std::make_integer_sequence<int, SLOTS>());
	Table[1] = (void *)FakeAddRef;
	Table[2] = (void *)FakeRelease;
	for (auto &S : Slots)
		Table[S.first / 4] = S.second;
	return Table;
}

static void MakeVtbls()
{
	Direct3DVtbl = MakeVtbl({{0x40, (void *)D3DCreateDevice}});
	DeviceVtbl = MakeVtbl({{0x0c, (void *)DevTestCooperativeLevel}, {0x40, (void *)DevReset},
		{0x44, (void *)DevPresent}, {0x48, (void *)DevGetBackBuffer}, {0x5c, (void *)DevCreateTexture},
		{0x68, (void *)DevCreateVertexBuffer}, {0x78, (void *)DevUpdateSurface},
		{0x80, (void *)DevGetRenderTargetData}, {0x8c, (void *)DevColorFill},
		{0x90, (void *)DevCreateOffscreenPlainSurface}, {0x94, (void *)DevSetRenderTarget},
		{0xa4, (void *)DevBeginScene}, {0xa8, (void *)DevEndScene}, {0xe4, (void *)DevSetRenderState},
		{0x104, (void *)DevSetTexture}, {0x10c, (void *)DevSetTextureStageState},
		{0x114, (void *)DevSetSamplerState}, {0x144, (void *)DevDrawPrimitive}, {0x164, (void *)DevSetFVF},
		{0x190, (void *)DevSetStreamSource}});
	TextureVtbl = MakeVtbl({{0x48, (void *)TexGetSurfaceLevel}, {0x4c, (void *)TexLockRect},
		{0x50, (void *)TexUnlockRect}});
	SurfaceVtbl = MakeVtbl({{0x34, (void *)SurfLockRect}, {0x38, (void *)SurfUnlockRect}});
	BufferVtbl = MakeVtbl({{0x2c, (void *)BufLock}, {0x30, (void *)BufUnlock}});
	DirectDrawVtbl = MakeVtbl({{0x00, (void *)DDQueryInterface}});
	DirectDraw7Vtbl = MakeVtbl({{0x20, (void *)DD7EnumDisplayModes}, {0x2c, (void *)DD7GetCaps},
		{0x30, (void *)DD7GetDisplayMode}});
}

// --- Fake Windows functions ------------------------------------------------------------------

static HMODULE const FAKE_D3D9 = (HMODULE)(uintptr_t)0x7e5d0000;
static HMODULE (WINAPI *RealLoadLibraryA)(LPCSTR);
static FARPROC (WINAPI *RealGetProcAddress)(HMODULE, LPCSTR);
static BOOL (WINAPI *RealFreeLibrary)(HMODULE);
static bool D3D9Missing;

static IDirect3D9 *WINAPI FakeDirect3DCreate9(UINT Version)
{
	if (Failing("Direct3DCreate9"))
		return NULL;
	TDX9Fake *F = NewFake('D', Direct3DVtbl);
	Logf("Direct3DCreate9(%u)=%s;", Version, Name(F));
	return (IDirect3D9 *)F;
}
static HMODULE WINAPI FakeLoadLibraryA(LPCSTR Name)
{
	if (_stricmp(Name, "d3d9.dll") != 0)
		return RealLoadLibraryA(Name);
	Logf("LoadLibraryA(%s);", Name);
	return D3D9Missing ? NULL : FAKE_D3D9;
}
static FARPROC WINAPI FakeGetProcAddress(HMODULE Module, LPCSTR Name)
{
	if (Module != FAKE_D3D9)
		return RealGetProcAddress(Module, Name);
	Logf("GetProcAddress(%s);", Name);
	return strcmp(Name, "Direct3DCreate9") == 0 ? (FARPROC)FakeDirect3DCreate9 : NULL;
}
static BOOL WINAPI FakeFreeLibrary(HMODULE Module)
{
	if (Module != FAKE_D3D9)
		return RealFreeLibrary(Module);
	Logf("FreeLibrary;");
	return TRUE;
}
static BOOL WINAPI FakeSetRect(RECT *R, int Left, int Top, int Right, int Bottom)
{
	Logf("SetRect(%d,%d,%d,%d);", Left, Top, Right, Bottom);
	R->left = Left;
	R->top = Top;
	R->right = Right;
	R->bottom = Bottom;
	return TRUE;
}
static BOOL WINAPI FakeGetClientRect(HWND Window, RECT *R)
{
	Logf("GetClientRect(%p);", Window);
	*R = World.Client;
	return TRUE;
}
static BOOL WINAPI FakeClientToScreen(HWND Window, POINT *P)
{
	Logf("ClientToScreen(%p,%ld,%ld);", Window, P->x, P->y);
	P->x += World.Origin.x;
	P->y += World.Origin.y;
	return TRUE;
}
static int WINAPI FakeGetSystemMetrics(int Index)
{
	Logf("GetSystemMetrics(%d);", Index);
	return Index == SM_CXSCREEN ? World.ScreenWidth : Index == SM_CYSCREEN ? World.ScreenHeight : 0;
}
static DWORD WINAPI FakeTimeGetTime()
{
	DWORD Now = World.Clock;
	World.Clock += World.ClockStep;
	Logf("timeGetTime=%lu;", Now);
	return Now;
}
static void WINAPI FakeSleep(DWORD Milliseconds)
{
	Logf("Sleep(%lu);", Milliseconds);
}
static HRESULT WINAPI FakeDirectDrawCreate(GUID *Id, IDirectDraw **DirectDraw, IUnknown *Outer)
{
	if (Failing("DirectDrawCreate")) {
		*DirectDraw = NULL;
		return DDERR_GENERIC;
	}
	TDX9Fake *F = NewFake('O', DirectDrawVtbl);
	*DirectDraw = (IDirectDraw *)F;
	Logf("DirectDrawCreate(%p,%p)=%s;", Id, Outer, Name(F));
	return DD_OK;
}

static void __cdecl FakeDrawSkin(int Surface, int x, int y, int Width, int Height, int Skin, int SourceX, int SourceY)
{
	Logf("DrawSkin(%d,%d,%d,%d,%d,%d,%d,%d);", Surface, x, y, Width, Height, Skin, SourceX, SourceY);
}
static void __cdecl FakeSetDefaultNoStretching(bool Value) { Logf("SetDefaultNoStretching(%d);", Value); }
static int __cdecl FakeGetConfineFramerate() { Logf("GetConfineFramerate;"); return World.Confine; }
static TSurface *Objects[2];
static TSurface *__cdecl FakeGetSurface() { return Objects[Side]; }

// --- Patches ---------------------------------------------------------------------------------

struct TDX9Saved {
	uint8 *Address;
	uint8 Bytes[5];
	int Size;
};
static std::vector<TDX9Saved> Saved;

static void Write(void *Address, const void *Data, int Size)
{
	TDX9Saved S;
	S.Address = (uint8 *)Address;
	S.Size = Size;
	DWORD Protection;
	VirtualProtect(Address, Size, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(S.Bytes, Address, Size);
	memcpy(Address, Data, Size);
	VirtualProtect(Address, Size, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Address, Size);
	Saved.push_back(S);
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
	for (size_t i = Saved.size(); i-- > 0;) {
		DWORD Protection;
		VirtualProtect(Saved[i].Address, Saved[i].Size, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(Saved[i].Address, Saved[i].Bytes, Saved[i].Size);
		VirtualProtect(Saved[i].Address, Saved[i].Size, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), Saved[i].Address, Saved[i].Size);
	}
	Saved.clear();
}

// The slot of an imported function in difftest.exe's own import table.
static void **OwnImportSlot(const char *Dll, const char *Function)
{
	uint8 *Base = (uint8 *)GetModuleHandleA(NULL);
	void *Real = (void *)RealGetProcAddress(GetModuleHandleA(Dll), Function);
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
	RealLoadLibraryA = LoadLibraryA;
	RealGetProcAddress = GetProcAddress;
	RealFreeLibrary = FreeLibrary;
	struct { uint32 ImageSlot; const char *Dll; const char *Function; void *Fake; } Table[] = {
		{0x55b200, "kernel32.dll", "LoadLibraryA", (void *)FakeLoadLibraryA},
		{0x55b1fc, "kernel32.dll", "GetProcAddress", (void *)FakeGetProcAddress},
		{0x55b20c, "kernel32.dll", "FreeLibrary", (void *)FakeFreeLibrary},
		{0x55b1f0, "kernel32.dll", "Sleep", (void *)FakeSleep},
		{0x55b4c0, "user32.dll", "SetRect", (void *)FakeSetRect},
		{0x55b514, "user32.dll", "GetClientRect", (void *)FakeGetClientRect},
		{0x55b510, "user32.dll", "ClientToScreen", (void *)FakeClientToScreen},
		{0x55b4f8, "user32.dll", "GetSystemMetrics", (void *)FakeGetSystemMetrics},
		{0x55b59c, "winmm.dll", "timeGetTime", (void *)FakeTimeGetTime},
		{0x55b03c, "ddraw.dll", "DirectDrawCreate", (void *)FakeDirectDrawCreate},
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
	Redirect((void *)(uintptr_t)0x497a00, (void *)FakeDrawSkin);
	Redirect((void *)DrawSkin, (void *)FakeDrawSkin);
	Redirect((void *)(uintptr_t)0x4db360, (void *)FakeSetDefaultNoStretching);
	Redirect((void *)SetDefaultNoStretching, (void *)FakeSetDefaultNoStretching);
	Redirect((void *)(uintptr_t)0x4db410, (void *)FakeGetConfineFramerate);
	Redirect((void *)GetConfineFramerate, (void *)FakeGetConfineFramerate);
	Redirect((void *)(uintptr_t)0x4eaa90, (void *)FakeGetSurface);
	Redirect((void *)GetSurface, (void *)FakeGetSurface);
}

// --- Comparison ------------------------------------------------------------------------------

static std::string Dump(int S)
{
	int Old = Side;
	Side = S;
	std::string Text;
	char Buffer[64];
	uint8 *Object = (uint8 *)Objects[S];
	for (int Offset = 4; Offset < 0x2c0; Offset += 4) {
		uint32 Value = *(uint32 *)(Object + Offset);
		bool Pointer = Offset >= 0x1f4; // every word from Direct3D on is an object
		if (Pointer)
			sprintf(Buffer, "%x:%s ", Offset, Name((void *)(uintptr_t)Value));
		else
			sprintf(Buffer, "%x:%08x ", Offset, Value);
		Text += Buffer;
	}
	for (TDX9Fake *F : Fakes[S]) {
		if (F->Dirty && !F->Memory.empty()) {
			F->Hashed = Hash(&F->Memory[0], F->Memory.size());
			F->Dirty = F->Kind == 'B'; // vertex buffers stay locked
		}
		sprintf(Buffer, "|%c%d r%d %08x", F->Kind, F->Id, F->Refs, F->Hashed);
		Text += Buffer;
	}
	if (S == IMAGE)
		sprintf(Buffer, "|stats %d %d %d %d %d %d %d %d %d %d %d", Global<int>(0x71d0d0), Global<int>(0x71d0d4),
			Global<int>(0x71d0d8), Global<int>(0x71d0dc), Global<int>(0x71d0e0), Global<int>(0x71d0e8),
			Global<int>(0x71d0ec), Global<int>(0x71d0f0), Global<int>(0x5c2568), Global<int>(0x5c256c),
			Global<int>(0x71d0bc));
	else
		sprintf(Buffer, "|stats %d %d %d %d %d %d %d %d %d %d %d", BlitCount, BlitPixels, Data71D0D8,
			ScreenWidth, ScreenHeight, LastBlitCount, LastBlitPixels, NumberOfSurfaces, CacheSurfaceWidth,
			CacheSurfaceHeight, InitStep);
	Text += Buffer;
	Side = Old;
	return Text;
}

static const char *What;
static void Compare()
{
	Checks++;
	if (Log[IMAGE] != Log[OURS]) {
		Failures++;
		if (Failures <= FailuresPrinted) {
			size_t i = 0;
			while (i < Log[IMAGE].size() && i < Log[OURS].size() && Log[IMAGE][i] == Log[OURS][i])
				i++;
			size_t From = i > 120 ? i - 120 : 0;
			printf("FAIL SurfaceDX9 %s: logs differ at %u\n  image: %s\n  ours:  %s\n", What, (unsigned)i,
				Log[IMAGE].substr(From, 300).c_str(), Log[OURS].substr(From, 300).c_str());
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
			size_t From = i > 60 ? i - 60 : 0;
			printf("FAIL SurfaceDX9 %s: objects differ at %u\n  image: %s\n  ours:  %s\n", What, (unsigned)i,
				A.substr(From, 200).c_str(), B.substr(From, 200).c_str());
			for (size_t k = 0; k < Fakes[0].size() && k < Fakes[1].size(); k++) {
				TDX9Fake *F = Fakes[0][k], *G = Fakes[1][k];
				if (F->Memory.size() != G->Memory.size())
					continue;
				for (size_t b = 0; b < F->Memory.size(); b++)
					if (F->Memory[b] != G->Memory[b]) {
						long Offset = (long)b - (long)(F->Bits - &F->Memory[0]);
						printf("  %c%d first differs at byte %ld (row %ld, column %ld): %02x %02x\n", F->Kind, F->Id,
							Offset, F->Pitch ? Offset / F->Pitch : 0, F->Pitch ? Offset % F->Pitch / 4 : 0,
							F->Memory[b], G->Memory[b]);
						break;
					}
			}
		}
	}
	if (LogFile != NULL)
		fprintf(LogFile, "== %s\n%s\n", What, Log[OURS].c_str());
	Log[IMAGE].clear();
	Log[OURS].clear();
}

// Runs one operation on both sides from the same world; the world the image
// leaves is kept as the next one only if ours leaves the same (it does when
// the logs agree).
template <class F> static void Both(const char *Name, F Operation)
{
	What = Name;
	if (Trace)
		printf("  %s\n", Name);
	TDX9World Start = World;
	Side = IMAGE;
	Operation(Objects[IMAGE]);
	World = Start;
	Side = OURS;
	Operation(Objects[OURS]);
	Compare();
}
template <class R, class F> static void BothResult(const char *Name, F Operation)
{
	R Result[2];
	Both(Name, [&](TSurface *S) { Result[Side] = Operation(S); });
	CHECK(memcmp(&Result[0], &Result[1], sizeof(R)) == 0, "SurfaceDX9 %s: results differ", Name);
}

// --- Inputs ----------------------------------------------------------------------------------

static int Between(int Min, int Max) { return Min + (int)(Random() % (uint32)(Max - Min + 1)); }
static bool Chance(int Percent) { return (int)(Random() % 100) < Percent; }
static TRGBColor RandomColor()
{
	static const TRGBColor Common[] = {TRGBColor(0, 0, 0), TRGBColor(255, 0, 255), TRGBColor(255, 255, 255)};
	if (Chance(40))
		return Common[Random() % 3];
	const int B = Between(0, 255), G = Between(0, 255), R = Between(0, 255); // issue #55: right to left
	return TRGBColor(R, G, B);
}
static TRect RandomRect(int Size)
{
	const int Height = Between(0, Size / 2), Width = Between(0, Size / 2), Y = Between(-20, Size), X = Between(-20, Size); // issue #55: the arguments right to left
	return TRect(X, Y, Width, Height);
}
static int AnySurface() { return Between(0, NUMBER_OF_SURFACES_DX9 - 1); }

static void RandomWorld()
{
	World.Clock = Random();
	World.ClockStep = Chance(30) ? 0 : Between(1, 40);
	World.Cooperative = D3D_OK;
	World.FXCaps = Chance(50) ? 0x15400 : Random();
	World.CapsResult = Chance(85) ? DD_OK : DDERR_GENERIC;
	World.DesktopDepth = Chance(50) ? 32 : Between(16, 40);
	World.EnumResult = DD_OK;
	World.NumberOfModes = Between(1, 8);
	for (int i = 0; i < World.NumberOfModes; i++) {
		TDX9Mode &M = World.Modes[i];
		static const int Sizes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {320, 200}, {1280, 1024}};
		int k = Between(0, 4);
		M.Width = Sizes[k][0];
		M.Height = Sizes[k][1];
		M.Depth = Chance(70) ? 32 : Between(8, 24);
		M.Frequency = Between(50, 100);
		M.Flags = Chance(80) ? (DDSD_WIDTH | DDSD_HEIGHT | DDSD_REFRESHRATE | DDSD_PIXELFORMAT) : Random();
		M.PixelFlags = Chance(70) ? DDPF_RGB : Random();
	}
	World.Modes[0].Flags |= DDSD_WIDTH | DDSD_HEIGHT; // at least one usable mode
	World.Modes[0].PixelFlags |= DDPF_RGB;
	World.Modes[0].Depth = 32;
	World.Modes[0].Width = 640;
	World.Modes[0].Height = 480;
	World.Client = RECT{0, 0, Between(100, 1000), Between(100, 800)};
	World.Origin = POINT{Between(0, 300), Between(0, 300)};
	World.ScreenWidth = Between(640, 1600);
	World.ScreenHeight = Between(480, 1200);
	World.Confine = Chance(30) ? Between(0, 300) : Between(10, 200);
	World.AlphaResult = D3D_OK;
}

static void FillPattern(TSurface *S)
{
	uint8 *P = (uint8 *)S;
	for (int Offset = 4; Offset < 0x2c0; Offset++) {
		bool Set = Offset == 0x26 || (Offset >= 0x38 && Offset < 0x44) || (Offset >= 0x1f4 && Offset < 0x1fc)
			|| (Offset >= 0x2b4 && Offset < 0x2c0);
		if (!Set)
			P[Offset] = 0xab;
	}
}

static void ClearFakes()
{
	for (int S = 0; S < 2; S++) {
		for (TDX9Fake *F : Fakes[S])
			delete F;
		Fakes[S].clear();
	}
}

// --- The test --------------------------------------------------------------------------------

static bool DeviceReady()
{
	TSurfaceDX9 *S = (TSurfaceDX9 *)Objects[OURS];
	return S->Device != NULL && S->Ready && S->VertexBuffer != NULL;
}

static void InitBoth(bool Force)
{
	static HWND Windows[] = {(HWND)(uintptr_t)0x10010, (HWND)(uintptr_t)0x20020};
	TSurfaceDX9 *S = (TSurfaceDX9 *)Objects[OURS];
	HWND Window = Windows[Random() % 2];
	int Mode = Chance(50) ? WINDOWED_MODE : FULLSCREEN_MODE;
	TFullscreenMode Resolution;
	static const int Sizes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {333, 222}};
	int k = Between(0, 3);
	Resolution.Width = Sizes[k][0];
	Resolution.Height = Sizes[k][1];
	Resolution.Depth = Chance(70) ? 32 : 16;
	Resolution.Frequency = Between(0, 100);
	if (Chance(20) && S->ViewMode == FULLSCREEN_MODE) {
		Resolution.Width = S->Width;
		Resolution.Height = S->Height;
		Resolution.Depth = S->Depth;
	}
	if (Chance(10))
		World.Cooperative = D3DERR_DEVICENOTRESET;
	else if (Chance(5))
		World.Cooperative = D3DERR_DEVICELOST;
	else
		World.Cooperative = D3D_OK;
	TFullscreenMode Copies[2] = {Resolution, Resolution};
	BothResult<bool>(Force ? "Init force" : "Init", [&](TSurface *T) {
		return T->Init(&Window, Mode, &Copies[Side], Force);
	});
	CHECK(memcmp(&Copies[0], &Copies[1], sizeof(Resolution)) == 0, "SurfaceDX9 Init: resolutions differ");
	World.Cooperative = D3D_OK;
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

static void RandomOperation(int Round)
{
	TSurfaceDX9 *S = (TSurfaceDX9 *)Objects[OURS];
	bool Ready = DeviceReady();
	int Kind = Between(0, Ready ? 40 : 12);
	switch (Kind) {
	case 0:
		InitBoth(Chance(30));
		break;
	case 1: {
		World.Cooperative = Chance(30) ? D3DERR_DEVICENOTRESET : Chance(50) ? D3DERR_DEVICELOST : D3D_OK;
		if (S->Device != NULL)
			Both("RestoreSurfaces", [&](TSurface *T) { T->RestoreSurfaces(0, NULL); });
		World.Cooperative = D3D_OK;
		break;
	}
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
			// a whole-millisecond frame time and a frame 0-2 ms late: the boundaries of the late case
			int Limit = Chance(50) ? 100 : 125;
			Both("SetFramerateLimit", [&](TSurface *T) { T->SetFramerateLimit(Limit); });
			World.ClockStep = 1000 / Limit + Between(-1, 2);
			Both("ResetFrameTimer", [&](TSurface *T) { T->ResetFrameTimer(); });
		} else {
			World.ClockStep = Chance(50) ? Between(0, 5) : Between(0, 1500);
		}
		Both("WaitForNextFrame", [&](TSurface *T) { T->WaitForNextFrame(); });
		break;
	case 5:
		BothResult<double>("GetFPS", [&](TSurface *T) { return T->GetFPS(); });
		BothResult<int>("GetCacheSurfaces", [&](TSurface *T) { return T->GetCacheSurfaces(); });
		BothResult<bool>("IsUnavailable", [&](TSurface *T) { return T->IsUnavailable(); });
		BothResult<bool>("CanShadeRectangles", [&](TSurface *T) { return T->CanShadeRectangles(); });
		BothResult<bool>("SurfaceRecreated", [&](TSurface *T) { return T->SurfaceRecreated(Between(0, 1)); });
		break;
	case 6: {
		int Surface = AnySurface();
		BothResult<int>("GetSurfaceMemory", [&](TSurface *T) { return T->GetSurfaceMemory(Surface); });
		TRect R[2];
		Both("GetSurfaceRect", [&](TSurface *T) { T->GetSurfaceRect(Surface, &R[Side]); });
		CHECK(memcmp(&R[0], &R[1], sizeof(TRect)) == 0, "SurfaceDX9 GetSurfaceRect differs");
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
		CHECK(memcmp(Rects[0], Rects[1], sizeof(Rects[0])) == 0, "SurfaceDX9 ClipRect rectangles differ");
		break;
	}
	case 8: {
		// at least 128: textures re-created with this size must hold the 106x106 writes
		int Surface = Chance(40) ? 0 : Between(SPRITE_SURFACE, NUMBER_OF_SURFACES_DX9 - 1);
		int x = Between(0, 4), y = Between(0, 4), Width = Between(128, 256), Height = Between(128, 256);
		Both("SetSurfaceRect", [&](TSurface *T) { T->SetSurfaceRect(Surface, x, y, Width, Height); });
		break;
	}
	case 9:
		World.Client = RECT{0, 0, Between(100, 1000), Between(100, 800)};
		Both("UpdateWindowRect", [&](TSurface *T) { T->UpdateWindowRect(); });
		break;
	case 10: {
		TFullscreenMode *Modes[2] = {NULL, NULL};
		if (S->DirectDraw == NULL)
			break;
		if (Chance(20))
			World.EnumResult = DDERR_GENERIC;
		BothResult<int>("GetFullscreenModes", [&](TSurface *T) { return T->GetFullscreenModes(&Modes[Side]); });
		int Number = S->NumberOfModes;
		if (Modes[0] != NULL && Modes[1] != NULL)
			CHECK(memcmp(Modes[0], Modes[1], Number * sizeof(TFullscreenMode)) == 0, "SurfaceDX9 modes differ");
		else
			CHECK(Modes[0] == NULL && Modes[1] == NULL, "SurfaceDX9 modes: one side NULL");
		FreeModes(IMAGE, Modes[0]);
		FreeModes(OURS, Modes[1]);
		World.EnumResult = DD_OK;
		break;
	}
	case 11:
		if (Chance(50))
			Both("ExitDisplay", [&](TSurface *T) { T->ExitDisplay(); });
		else if (Chance(50)) {
			Both("ExitSurfaces", [&](TSurface *T) { T->ExitSurfaces(); });
			Both("InitSurfaces", [&](TSurface *T) { T->InitSurfaces(); });
		}
		break;
	case 12: {
		int Surface = Chance(50) ? 0 : AnySurface();
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
		if (Chance(10))
			R.width = 0;
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
		int Surface = Chance(40) ? Between(0, SPRITE_SURFACE - 1) : Between(SPRITE_SURFACE, NUMBER_OF_SURFACES_DX9 - 1);
		int Width = Between(1, 32), Height = Between(1, 32);
		int x = Between(0, 106 - Width - 4), y = Between(0, 106 - Height - 4);
		std::vector<uint8> Data(Width * Height * 3);
		for (size_t i = 0; i < Data.size(); i++)
			Data[i] = Chance(30) ? (i % 3 == 1 ? 0 : 255) : (uint8)Random();
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
		int x = Between(0, 64 - Width), y = Between(0, 64 - Height);
		std::vector<uint8> Data(Width * Height * 3), Mask(Width * Height * 3);
		static const uint8 MaskColors[][3] = {{255, 255, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {0, 0, 0},
			{255, 255, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 1}, {0, 1, 255}};
		TRGBColor Colors[4];
		for (int i = 0; i < 4; i++)
			Colors[i] = RandomColor();
		for (int i = 0; i < Width * Height; i++) {
			const uint8 *M = MaskColors[Random() % 10];
			memcpy(&Mask[i * 3], M, 3);
			if (Chance(30)) {
				Data[i * 3] = (uint8)Colors[1].R;
				Data[i * 3 + 1] = (uint8)Colors[1].G;
				Data[i * 3 + 2] = (uint8)Colors[1].B;
			} else {
				for (int k = 0; k < 3; k++)
					Data[i * 3 + k] = (uint8)Random();
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
		int To = AnySurface(), From = AnySurface(), x = Between(-40, 600), y = Between(-40, 450);
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
		int To = AnySurface(), From = AnySurface(), x = Between(-40, 600), y = Between(-40, 450);
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
		int To = AnySurface(), From = AnySurface();
		const int DHeight = Between(0, 200), DWidth = Between(0, 200), DY = Between(-40, 450), DX = Between(-40, 600); // issue #55: the arguments right to left
		TRect D(DX, DY, DWidth, DHeight);
		const int SourceHeight = Between(1, 200), SourceWidth = Between(1, 200), SourceY = Between(0, 300), SourceX = Between(0, 400); // issue #55: the arguments right to left
		TRect Source(SourceX, SourceY, SourceWidth, SourceHeight);
		Both("StretchBlitRect", [&](TSurface *T) { T->StretchBlitRect(To, D, From, Source); });
		break;
	}
	case 28:
	case 29: {
		int To = AnySurface(), From = AnySurface(), x = Between(-40, 600), y = Between(-40, 450);
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
		int To, From;
		do
			To = AnySurface();
		while (To == TEMP_SURFACE || To == STRETCH_SURFACE);
		do
			From = AnySurface();
		while (From == TEMP_SURFACE || From == STRETCH_SURFACE);
		int x = Between(-40, 600), y = Between(-40, 450);
		int FromX = Between(0, 400), FromY = Between(0, 300), FromWidth = Between(1, 63), FromHeight = Between(1, 63);
		int Width = Chance(25) ? FromWidth : Between(1, 64), Height = Chance(25) ? FromHeight : Between(1, 64);
		TRGBColor Key = RandomColor(), Highlight = RandomColor();
		TRect Clip = RandomRect(500);
		bool Skin = Chance(40);
		S->Flag26 = Skin;
		((TSurfaceDX9 *)Objects[IMAGE])->Flag26 = Skin;
		Both("StretchBlit", [&](TSurface *T) {
			T->StretchBlit(To, TRect(x, y, Width, Height), From, TRect(FromX, FromY, FromWidth, FromHeight), Key,
				Highlight, Clip);
		});
		break;
	}
	case 33:
	case 34: {
		int Surface = Chance(85) ? TEMP_SURFACE : AnySurface();
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
	case 37: {
		// many quads in a row: the full buffer (9600 vertices) is drawn
		int To = AnySurface(), From = Chance(50) ? SCREEN_SURFACE : AnySurface();
		int n = Between(1500, 1700);
		Both("StretchBlitRect x n", [&](TSurface *T) {
			for (int i = 0; i < n; i++)
				T->StretchBlitRect(To, TRect(i % 50, i % 37, 10, 10), From, TRect(i % 13, i % 7, 5, 5));
		});
		break;
	}
	default: {
		int Surface = AnySurface();
		int x = Between(-10, 600), y = Between(-10, 400);
		TRGBColor C = RandomColor();
		Both("SetPixel", [&](TSurface *T) { T->SetPixel(Surface, x, y, C); });
		break;
	}
	}
}

static void RunRound(int Round)
{
	RandomWorld();
	Side = IMAGE;
	Objects[IMAGE] = At<TSurface *(__cdecl *)()>(0x4f1430)();
	Side = OURS;
	Objects[OURS] = CreateSurfaceDX9();
	FillPattern(Objects[IMAGE]);
	FillPattern(Objects[OURS]);
	What = "constructor";
	Compare();
	if (Round % 7 == 3) {
		// TestEngine: the main window is Surface.cpp's (0 on both sides in the test)
		Global<HWND>(0x71d0c0) = NULL;
		BothResult<bool>("TestEngine", [&](TSurface *T) { return T->TestEngine(); });
		D3D9Missing = true;
		BothResult<bool>("IsDirect3D9Installed (missing)", [&](TSurface *T) {
			return Side == IMAGE ? At<bool (__cdecl *)()>(0x4efd10)() : IsDirect3D9Installed();
		});
		D3D9Missing = false;
		BothResult<bool>("IsDirect3D9Installed", [&](TSurface *T) {
			return Side == IMAGE ? At<bool (__cdecl *)()>(0x4efd10)() : IsDirect3D9Installed();
		});
	}
	Both("InitSurfaces", [&](TSurface *T) { T->InitSurfaces(); });
	if (Round % 11 == 5) {
		// a failed state: CreateSurfaces returns early, nothing else is created
		World.AlphaResult = D3DERR_INVALIDCALL;
		InitBoth(false);
		World.AlphaResult = D3D_OK;
	} else {
		InitBoth(Chance(20));
		int Operations = Between(20, 80);
		for (int i = 0; i < Operations; i++)
			RandomOperation(Round);
	}
	Both("ExitSurfaces", [&](TSurface *T) { T->ExitSurfaces(); });
	Side = IMAGE;
	delete Objects[IMAGE]; // the image's scalar deleting destructor 0x4f18b0
	Side = OURS;
	delete Objects[OURS];
	What = "destructor";
	Log[0].clear();
	Log[1].clear();
	CHECK(Global<int>(0x71d0f0) == NumberOfSurfaces, "SurfaceDX9 destructor: NumberOfSurfaces");
	Objects[0] = Objects[1] = NULL;
	ClearFakes();
}

// --- Error paths (issue #34) ---------------------------------------------------------------

// Both sides as Both does, each in ImageThrow/OurThrow: what they threw, then
// the logs and the objects.
template <class F> static void BothThrown(TErrorRound &R, const char *Name, F Operation)
{
	What = Name;
	if (Trace)
		printf("  %s (errors)\n", Name);
	TDX9World Start = World;
	Side = IMAGE;
	TThrown Original = ImageThrow([&] { Operation(Objects[IMAGE]); });
	World = Start;
	Side = OURS;
	TThrown Ours = OurThrow([&] { Operation(Objects[OURS]); });
	R.Compare(Name, Ours, Original);
	Compare();
}

// PROGRAM_ERROR 7, 8, 12 and 14-18: one creation call fails (DirectDrawCreate,
// QueryInterface, Direct3DCreate9, CreateDevice, each CreateTexture), the
// desktop has too few bits, or d3d9.dll is missing; then the assertions of
// the drawing functions with surfaces and rectangles out of range.
static void TestSurfaceDX9Errors()
{
	TErrorRound R("SurfaceDX9");
	for (int Round = 0; Round < 120; Round++) {
		RandomWorld();
		int Case = Round % 4;
		Side = IMAGE;
		Objects[IMAGE] = At<TSurface *(__cdecl *)()>(0x4f1430)();
		Side = OURS;
		Objects[OURS] = CreateSurfaceDX9();
		FillPattern(Objects[IMAGE]);
		FillPattern(Objects[OURS]);
		What = "constructor";
		Compare();
		World.Calls = 0;
		World.FailCall = Case == 0 ? Between(1, 2) : 0; // DirectDrawCreate, QueryInterface
		BothThrown(R, "InitSurfaces", [&](TSurface *T) { T->InitSurfaces(); });
		bool Ready = World.FailCall == 0;
		if (Ready) {
			World.Calls = 0;
			// Every creation call of Init in turn (at random, some were never
			// chosen: the mutation check found the primary render target's 14).
			World.FailCall = Case == 1 ? 1 + (Round / 4) % 26 : 0;
			if (Case == 2) {
				if (Chance(50))
					World.DesktopDepth = Between(1, 15);
				else
					D3D9Missing = true;
			}
			HWND Window = (HWND)(uintptr_t)0x10010;
			int Mode = Chance(50) ? WINDOWED_MODE : FULLSCREEN_MODE;
			TFullscreenMode Copies[2];
			for (int s = 0; s < 2; s++) {
				Copies[s].Width = 640;
				Copies[s].Height = 480;
				Copies[s].Depth = 32;
				Copies[s].Frequency = 60;
			}
			BothThrown(R, "Init", [&](TSurface *T) { T->Init(&Window, Mode, &Copies[Side], false); });
			D3D9Missing = false;
			World.FailCall = 0;
			if (Case == 3 && DeviceReady()) {
				int Bad = Chance(50) ? Between(-5, -1) : Between(NUMBER_OF_SURFACES_DX9, NUMBER_OF_SURFACES_DX9 + 5);
				int Good = Between(0, NUMBER_OF_SURFACES_DX9 - 1);
				const int EmptyHeight = Between(1, 5), EmptyWidth = Between(-3, 0), EmptyY = Between(0, 5), EmptyX = Between(0, 5); // issue #55: the arguments right to left
				TRect Empty(EmptyX, EmptyY, EmptyWidth, EmptyHeight);
				if (Chance(50))
					std::swap(Empty.width, Empty.height);
				const int SomeHeight = Between(1, 50), SomeWidth = Between(1, 50), SomeY = Between(0, 5), SomeX = Between(0, 5); // issue #55: the arguments right to left
				TRect Some(SomeX, SomeY, SomeWidth, SomeHeight);
				const int HugeHeight = Between(1, 50), HugeWidth = Between(2000, 5000); // issue #55: the arguments right to left
				TRect Huge(0, 0, HugeWidth, HugeHeight);
				TRGBColor Key(1, 2, 3);
				int x = Chance(50) ? Between(-5, -1) : Between(0, 40), y = Between(0, 40);
				int w = Chance(50) ? Between(-2, 0) : Between(1, 20), h = Between(1, 20);
				static uint8 Data[64 * 64 * 3], Mask[64 * 64 * 3];
				static TRGBColor Colors[8];
				BothThrown(R, "SetSourceColorKey", [&](TSurface *T) { T->SetSourceColorKey(Bad, Key); });
				BothThrown(R, "GetSurfaceRect", [&](TSurface *T) { TRect Out; T->GetSurfaceRect(Bad, &Out); });
				BothThrown(R, "WriteMaskedImage screen", [&](TSurface *T) {
					T->WriteMaskedImage(SCREEN_SURFACE, 0, 0, 4, 4, Data, Mask, Colors);
				});
				BothThrown(R, "WriteMaskedImage area", [&](TSurface *T) {
					T->WriteMaskedImage(SPRITE_SURFACE, x, y, w, h, Data, Mask, Colors);
				});
				int Stretch = Between(0, 5);
				int From = Stretch == 0 ? TEMP_SURFACE : Stretch == 1 ? STRETCH_SURFACE : Good;
				int To = Stretch == 2 ? TEMP_SURFACE : Stretch == 3 ? STRETCH_SURFACE : SCREEN_SURFACE;
				TRect FromRect = Stretch == 4 ? Huge : Some, ToRect = Stretch == 5 ? Huge : TRect(Some.x, Some.y, Some.width + 1, Some.height);
				BothThrown(R, "StretchBlit", [&](TSurface *T) {
					T->StretchBlit(To, ToRect, From, FromRect, Key, Key, TRect(0, 0, 800, 600));
				});
				BothThrown(R, "Blit", [&](TSurface *T) { T->Blit(SCREEN_SURFACE, 1, 1, Good, Empty, Key); });
				BothThrown(R, "BlitRect", [&](TSurface *T) { T->BlitRect(SCREEN_SURFACE, 1, 1, Good, Empty); });
				// InitDisplay's and CreateSurfaces' state checks: the members they
				// check set on both objects for the call, then put back.
				TSurfaceDX9 *D[2] = {(TSurfaceDX9 *)Objects[0], (TSurfaceDX9 *)Objects[1]};
				for (int Check = 0; Check < 12; Check++) {
					TFullscreenMode Mode640 = Copies[0];
					void *SavedDraw[2], *SavedD3D[2], *SavedSurfaces[2][6];
					HWND SavedWindow[2];
					int SavedView[2];
					static const int Order[6] = {PRIMARY_SURFACE, SCREEN_SURFACE, TEMP_SURFACE, CACHE_SURFACE, AUTOMAP_SURFACE,
						SPRITE_SURFACE};
					for (int s = 0; s < 2; s++) {
						SavedDraw[s] = D[s]->DirectDraw;
						SavedD3D[s] = D[s]->Direct3D;
						SavedWindow[s] = D[s]->hWindow;
						SavedView[s] = D[s]->ViewMode;
						for (int k = 0; k < 6; k++)
							SavedSurfaces[s][k] = D[s]->Surface9[Order[k]];
						if (Check == 0 || Check == 1)
							D[s]->DirectDraw = NULL;
						if (Check == 2)
							D[s]->hWindow = NULL;
						if (Check >= 4) { // CreateSurfaces with Direct3D gone and Check - 5 surfaces gone
							D[s]->Direct3D = Check == 4 ? D[s]->Direct3D : NULL;
							for (int k = 0; k < Check - 5 && k < 6; k++)
								D[s]->Surface9[Order[k]] = NULL;
							if (Check == 6 && D[s]->Surface9[SCREEN_SURFACE] == NULL) // never set: a stand-in
								D[s]->Surface9[SCREEN_SURFACE] = (decltype(D[s]->Surface9[0]))SavedSurfaces[s][0];
							if (Check == 11)
								D[s]->ViewMode = 7;
						}
					}
					if (Check <= 3) {
						int VMode = Check == 3 ? (Chance(50) ? 7 : FULLSCREEN_MODE) : WINDOWED_MODE;
						TFullscreenMode *GMode = Check == 3 && VMode == FULLSCREEN_MODE ? NULL : &Mode640;
						BothThrown(R, "InitDisplay", [&](TSurface *T) { ((TSurfaceDX9 *)T)->InitDisplay(VMode, GMode); });
					}
					if (Check == 1 || Check >= 4)
						BothThrown(R, "CreateSurfaces", [&](TSurface *T) { ((TSurfaceDX9 *)T)->CreateSurfaces(); });
					for (int s = 0; s < 2; s++) {
						D[s]->DirectDraw = (decltype(D[s]->DirectDraw))SavedDraw[s];
						D[s]->Direct3D = (decltype(D[s]->Direct3D))SavedD3D[s];
						D[s]->hWindow = SavedWindow[s];
						D[s]->ViewMode = SavedView[s];
						for (int k = 0; k < 6; k++)
							D[s]->Surface9[Order[k]] = (decltype(D[s]->Surface9[0]))SavedSurfaces[s][k];
					}
					Compare();
				}
			}
		}
		BothThrown(R, "ExitSurfaces", [&](TSurface *T) { T->ExitSurfaces(); });
		Side = IMAGE;
		delete Objects[IMAGE];
		Side = OURS;
		delete Objects[OURS];
		What = "destructor";
		Log[0].clear();
		Log[1].clear();
		Objects[0] = Objects[1] = NULL;
		ClearFakes();
	}
	World.FailCall = 0;
	R.Finish();
}

void TestSurfaceDX9()
{
	PatchImage();
	MakeVtbls();
	PatchWindows();
	Trace = getenv("DX9_TRACE") != NULL;
	const char *LogName = getenv("DX9_LOGS");
	LogFile = LogName != NULL ? fopen(LogName, "w") : NULL;
	int Before = Checks;
	int Rounds = getenv("DX9_ROUNDS") != NULL ? atoi(getenv("DX9_ROUNDS")) : 400;
	for (int Round = 0; Round < Rounds; Round++) {
		if (Trace)
			printf("round %d\n", Round);
		RunRound(Round);
	}
	printf("SurfaceDX9: %d checks\n", Checks - Before);
	TestSurfaceDX9Errors();
	RestoreAll();
	if (LogFile != NULL)
		fclose(LogFile);
}
