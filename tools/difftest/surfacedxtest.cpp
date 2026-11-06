// Differential test of SurfaceDX.cpp (TSurfaceDX, the DirectX 5 engine,
// 0x4eae70-0x4efd0f) against the original code.
//
// Both sides run their real SurfaceDX functions on fake COM objects: per side
// a pool of IDirectDraw, IDirectDraw4, IDirectDrawSurface4,
// IDirectDrawClipper, IDirect3D3, IDirect3DDevice3 and IDirect3DViewport3
// objects (the vtables have the slot order of the SDK's ddraw.h and d3d.h).
// The fakes log every call with the logical names of the objects ("S5",
// "Dev0"; never pointers), do what the real ones would to the fake pixels
// (colour fills, copies with colour key and stretching, the shading of the
// render target), and fail or succeed as the script says, which is decided
// before either side runs. After every operation the test compares the logs,
// the 0x250-byte objects (the COM pointers mapped to their logical IDs), the
// shared globals (InitStep, the blit statistics, NumberOfSurfaces, the cache
// surface size) with the image's copies, SurfaceMemory through
// GetSurfaceMemory, the results, and the state and pixels of the fakes.
//
// The imports: the image's IAT entries (DirectDrawCreate 0x55b03c,
// timeGetTime 0x55b59c, Sleep 0x55b1f0, SetRect 0x55b4c0, GetClientRect
// 0x55b514, ClientToScreen 0x55b510, GetSystemMetrics 0x55b4f8) and ours (our
// executable's IAT) point to the same recorders while the test runs. Patched
// with jumps (both sides, restored at the end): operator new and new[] (the
// 0xab pattern), GetSurface (AddFullscreenMode's accessor: the object of the
// side that runs), SetDefaultNoStretching and GetConfineFramerate (logged,
// the scripted frame rate). The vector constructor iterator, the
// D3DTLVERTEX constructor and _ftol of the original run as they are.
//
// Phases: factory (CreateSurfaceDX, both destructors), draw (the drawing
// slots, all blits and stretches, masks, colour keys, GetPixelColor,
// ClipRect, ShadeRectangle(s), TestShading and the accessors on prepared
// objects), timer (SetFramerateLimit, ResetFrameTimer, WaitForNextFrame with
// early, exact, late frames and the second boundaries), modes
// (GetFullscreenModes with accepted and rejected modes, both refresh flag
// placements, empty and failed enumerations), lifecycle (InitSurfaces, Init in
// every mode change, Force, InitDisplay, CreateSurfaces with every Direct3D
// step failing, video memory fallbacks and exhaustion, ResetDisplay,
// RestoreSurfaces, FinishFrame, AttachWindow, UpdateWindowRect,
// CreateOffscreenSurface, Create3DSurface, ExitDisplay, ExitSurfaces) and
// clockwrap (WaitForNextFrame across timeGetTime's wrap, last: it leaves the
// frame counter's second far in the future). The phases script only failures
// the code handles without throwing; the assertions and PROGRAM_ERROR throws
// are TestSurfaceDXErrors' (issue #34, imageeh.h), reported apart.
//
// DX_LOGS=<file> writes every operation's log; DX_TRACE=1 prints each
// operation before it runs and the coverage of each phase to stderr.

#include "difftest.h"
#include "imageeh.h"
#include "../../src/SurfaceDX.h"
#include "../../src/graphics.h"
#include "../../src/Preferences.h"
#include <ddraw.h>
#include <new>
#include <stdarg.h>
#include <map>
#include <string>
#include <vector>

typedef unsigned long long TDXMask;

enum { DX_OURS = 0, DX_ORIGINAL = 1 };

static int DXSide;
static bool DXRunning;
static std::string DXLog[2];
static FILE *DXLogs;
static bool DXTrace;
static std::map<std::string, int> DXCoverage;
static void *DXObjects[2];
static const char *DXPhase = "";
static int DXOperation;

static void DXLogf(const char *Format, ...)
{
	if (!DXRunning)
		return;
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	DXLog[DXSide] += Buffer;
}

static void DXCover(const char *Format, ...)
{
	char Buffer[256];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	DXCoverage[Buffer]++;
}

template <class Value> static Value &DXField(void *Object, int Offset)
{
	return *(Value *)((uint8 *)Object + Offset);
}

template <class Result, class... Args> static Result DXVirtual(void *Object, int Offset, Args... Values)
{
	void *Function = (*(void ***)Object)[Offset / 4];
	return ((Result (__fastcall *)(void *, void *, Args...))Function)(Object, NULL, Values...);
}

// --- The script: what the fakes do, decided before both sides run ----------

struct TDXScript {
	uint32 Clock, ClockStep;          // timeGetTime: Clock + call * ClockStep
	int Framerate;                    // GetConfineFramerate
	DWORD FXCaps;                     // IDirectDraw4::GetCaps
	HRESULT CapsResult;
	RECT Client;                      // GetClientRect (+16 for the second window)
	POINT Origin;                     // ClientToScreen
	int Metrics[2];                   // GetSystemMetrics(SM_CXSCREEN, SM_CYSCREEN)
	DDPIXELFORMAT Format;             // of the display and the created surfaces
	TDXMask VideoFail;                // bit n: the n-th offscreen video memory request fails
	TDXMask SystemFail;               // bit n: the n-th offscreen system memory request fails
	int Fail3D;                       // render target: 1 video memory fails, 2 both fail
	DWORD TempBits;                   // render target's bit count (0: Format's)
	int TempLockFailures;
	HRESULT TempLockError;
	HRESULT QI3D, Device, Viewport, AddViewport, SetViewport, DeviceCaps, CurrentViewport, Begin;
	int RenderStateFail;              // ordinal (1-based) of the SetRenderState that fails
	DWORD SrcBlend, DestBlend;        // the hardware triangle blend caps
	int ShadeMode;                    // DrawPrimitive: 0 draws nothing, 1 fills with vertex 0's colour
	HRESULT CoopTest;                 // TestCooperativeLevel
	int LostCall, OddLostCall;        // IsLost ordinals (1-based) that return SURFACELOST, GENERIC
	int StillDrawing;                 // GetBltStatus and Flip: WASSTILLDRAWING this many times
	HRESULT FlipError;                // then Flip's result
	int ModeCount;
	DDSURFACEDESC2 Modes[16];
	HRESULT EnumResult;
	int FailCall; // the error round (issue #34): the FailCall-th creation call fails (0: none)
};
static TDXScript DXScript;

struct TDXCounters {
	int Time, VideoRequests, SystemRequests, RenderStates, IsLost, BltStatus, Flips;
	int Calls; // DXFailing's
};
static TDXCounters DXCount[2];

// DirectDrawCreate, QueryInterface (IDirectDraw4), CreateClipper,
// SetCooperativeLevel, SetDisplayMode, CreateSurface and GetAttachedSurface
// count their calls; the one DXScript.FailCall names fails.
static bool DXFailing(const char *What)
{
	if (DXScript.FailCall == 0 || ++DXCount[DXSide].Calls != DXScript.FailCall)
		return false;
	DXLogf("%s fails;", What);
	return true;
}

static void DXFormat(DDPIXELFORMAT *Format, int Kind)
{
	memset(Format, 0, sizeof(*Format));
	Format->dwSize = sizeof(DDPIXELFORMAT);
	Format->dwFlags = DDPF_RGB;
	static const DWORD Bits[] = {8, 16, 16, 24, 32};
	Format->dwRGBBitCount = Bits[Kind % 5];
	Format->dwRBitMask = Kind % 5 == 1 ? 0xf800 : Kind % 5 == 2 ? 0x7c00 : 0xff0000;
	Format->dwGBitMask = Kind % 5 == 1 ? 0x07e0 : Kind % 5 == 2 ? 0x03e0 : 0x00ff00;
	Format->dwBBitMask = Kind % 5 == 1 || Kind % 5 == 2 ? 0x001f : 0x0000ff;
}

static void DXDefaultScript(int FormatKind)
{
	memset(&DXScript, 0, sizeof(DXScript));
	DXScript.Clock = 100000;
	DXScript.Framerate = 50;
	DXScript.FXCaps = DDFXCAPS_BLTSHRINKX | DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSHRINKY | DDFXCAPS_BLTSTRETCHY;
	DXScript.Client = {0, 0, 800, 600};
	DXScript.Origin = {30, 40};
	DXScript.Metrics[0] = 1280;
	DXScript.Metrics[1] = 1024;
	DXFormat(&DXScript.Format, FormatKind);
	DXScript.SrcBlend = D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE;
	DXScript.DestBlend = D3DPBLENDCAPS_SRCCOLOR | D3DPBLENDCAPS_ZERO;
	DXScript.ShadeMode = 1;
	DXScript.TempLockError = DDERR_SURFACEBUSY;
}

// --- The fake COM objects -----------------------------------------------------

enum { DX_DD1, DX_DD4, DX_SURFACE, DX_CLIPPER, DX_D3D, DX_DEVICE, DX_VIEWPORT, DX_KINDS };
static const char *const DXKindNames[DX_KINDS] = {"DD1", "DD", "S", "Clip", "D3D", "Dev", "VP"};
static const int DXPoolSize[DX_KINDS] = {12, 12, 96, 24, 12, 12, 12};
static const int DXSlots[DX_KINDS] = {23, 28, 45, 9, 12, 42, 21};
enum { DX_POOL = 96, DX_PITCH_MAX = 64 * 4 + 8, DX_PIXELS = 64 * DX_PITCH_MAX };

struct TDXCom {
	void **Vtable;
	int Kind, ID, Side, Refs, Releases;
	DDPIXELFORMAT Format;
	int Pitch, Width, Height;
	DWORD Caps;
	int LockFailures;
	HRESULT LockError;
	int BlitFailures;
	HRESULT BlitError;
	bool Keyed;
	DWORD KeyFlags, KeyLow, KeyHigh;
	TDXCom *Clipper;   // surface: SetClipper's
	TDXCom *Target;    // device: the render target
	HWND Window;       // clipper: SetHWnd's
	uint8 *Pixels;
};

static TDXCom DXPool[2][DX_KINDS][DX_POOL];
static uint8 DXPixelData[2][DX_POOL][DX_PIXELS];
static void *DXVtables[DX_KINDS][48];
static int DXHigh[2][DX_KINDS];

static void DXResetPools()
{
	memset(DXPool, 0, sizeof(DXPool));
	memset(DXHigh, 0, sizeof(DXHigh));
}

static TDXCom *DXAlloc(int Kind)
{
	int Side = DXSide;
	for (int ItemIndex = 0; ItemIndex < DXPoolSize[Kind]; ItemIndex++) {
		TDXCom *ComObject = &DXPool[Side][Kind][ItemIndex];
		if (ComObject->Refs > 0)
			continue;
		memset(ComObject, 0, sizeof(*ComObject));
		ComObject->Vtable = DXVtables[Kind];
		ComObject->Kind = Kind;
		ComObject->ID = ItemIndex;
		ComObject->Side = Side;
		ComObject->Refs = 1;
		if (Kind == DX_SURFACE) {
			ComObject->Pixels = DXPixelData[Side][ItemIndex];
			memset(ComObject->Pixels, 0x40 + ItemIndex, DX_PIXELS);
		}
		if (ItemIndex + 1 > DXHigh[Side][Kind])
			DXHigh[Side][Kind] = ItemIndex + 1;
		return ComObject;
	}
	CHECK(false, "DX pool of %s exhausted (side %d)", DXKindNames[Kind], Side);
	return NULL;
}

static TDXCom *DXFind(const void *Pointer, int *Side)
{
	if (Pointer == NULL)
		return NULL;
	for (int SideIndex = 0; SideIndex < 2; SideIndex++)
		for (int KindIndex = 0; KindIndex < DX_KINDS; KindIndex++) {
			const uint8 *First = (const uint8 *)&DXPool[SideIndex][KindIndex][0];
			const uint8 *DataPointer = (const uint8 *)Pointer;
			if (DataPointer >= First && DataPointer < (const uint8 *)&DXPool[SideIndex][KindIndex][DX_POOL]
					&& (DataPointer - First) % sizeof(TDXCom) == 0) {
				*Side = SideIndex;
				return (TDXCom *)Pointer;
			}
		}
	return NULL;
}

static std::string DXName(const void *Pointer)
{
	if (Pointer == NULL)
		return "null";
	int Side;
	TDXCom *ComObject = DXFind(Pointer, &Side);
	char Buffer[64];
	if (ComObject == NULL)
		snprintf(Buffer, sizeof(Buffer), "?%p", Pointer);
	else
		snprintf(Buffer, sizeof(Buffer), "%s%s%d", Side != DXSide ? "!" : "", DXKindNames[ComObject->Kind], ComObject->ID);
	return Buffer;
}

// The logical ID of a COM pointer in Side's object (the raw value for others).
static uint32 DXIdOf(const void *Pointer, int Side)
{
	if (Pointer == NULL)
		return 0;
	int Found;
	TDXCom *ComObject = DXFind(Pointer, &Found);
	if (ComObject == NULL || Found != Side)
		return (uint32)(uintptr_t)Pointer;
	return 0x7f000000 | ComObject->Kind << 16 | ComObject->ID;
}

// Where a pointer argument points: into the side's TSurfaceDX or elsewhere.
static std::string DXWhere(const void *Pointer)
{
	char Buffer[32];
	const uint8 *Object = (const uint8 *)DXObjects[DXSide];
	const uint8 *DataPointer = (const uint8 *)Pointer;
	if (Pointer == NULL)
		return "null";
	if (Object != NULL && DataPointer >= Object && DataPointer < Object + 0x250)
		snprintf(Buffer, sizeof(Buffer), "+0x%x", (unsigned)(DataPointer - Object));
	else
		snprintf(Buffer, sizeof(Buffer), "ext");
	return Buffer;
}

static std::string DXRectText(const RECT *Rect)
{
	if (Rect == NULL)
		return "null";
	char Buffer[64];
	snprintf(Buffer, sizeof(Buffer), "%ld,%ld,%ld,%ld", Rect->left, Rect->top, Rect->right, Rect->bottom);
	return Buffer;
}

static ULONG DXDoRelease(TDXCom *ComObject)
{
	ComObject->Releases++;
	if (ComObject->Refs <= 0) {
		DXLogf("ReleaseDead;");
		return 0;
	}
	ComObject->Refs--;
	if (ComObject->Refs == 0 && ComObject->Clipper != NULL && ComObject->Clipper->Refs > 0)
		ComObject->Clipper->Refs--;
	return ComObject->Refs;
}

static ULONG WINAPI DXRelease(TDXCom *ComObject)
{
	DXLogf("Release %s(%d);", DXName(ComObject).c_str(), ComObject->Refs - 1);
	return DXDoRelease(ComObject);
}

// --- Pixels -------------------------------------------------------------------

static int DXBytes(const TDXCom *StateObject)
{
	int Bytes = StateObject->Format.dwRGBBitCount / 8;
	return Bytes < 1 ? 1 : Bytes > 4 ? 4 : Bytes;
}

static uint8 *DXPixel(TDXCom *StateObject, int PixelX, int PixelY)
{
	if (PixelX < 0 || PixelY < 0 || PixelX >= 64 || PixelY >= 64)
		return NULL;
	return StateObject->Pixels + PixelY * StateObject->Pitch + PixelX * DXBytes(StateObject);
}

static uint32 DXReadPixel(TDXCom *StateObject, uint8 *DataPointer)
{
	uint32 Value = 0;
	for (int ItemIndex = 0; ItemIndex < DXBytes(StateObject); ItemIndex++)
		Value |= (uint32)DataPointer[ItemIndex] << (8 * ItemIndex);
	return Value;
}

static void DXWritePixel(TDXCom *StateObject, uint8 *DataPointer, uint32 Value)
{
	for (int ItemIndex = 0; ItemIndex < DXBytes(StateObject); ItemIndex++)
		DataPointer[ItemIndex] = (uint8)(Value >> (8 * ItemIndex));
}

// The test's own conversion of an RGB colour to a surface's format.
static uint32 DXColorPixel(TDXCom *StateObject, int RedValue, int GlobalValue, int ImageValue)
{
	if (StateObject->Format.dwRGBBitCount < 16)
		return 0;
	DWORD Masks[3] = {StateObject->Format.dwRBitMask, StateObject->Format.dwGBitMask, StateObject->Format.dwBBitMask};
	int Values[3] = {RedValue, GlobalValue, ImageValue};
	uint32 Pixel = 0;
	for (int ComponentIndex = 0; ComponentIndex < 3; ComponentIndex++) {
		DWORD Mask = Masks[ComponentIndex];
		int Shift = 0, Bits = 0;
		while (Mask != 0 && (Mask & 1) == 0) {
			Mask >>= 1;
			Shift++;
		}
		while ((Mask & 1) != 0) {
			Mask >>= 1;
			Bits++;
		}
		Pixel |= (uint32)(Values[ComponentIndex] >> (8 - Bits)) << Shift;
	}
	return Pixel;
}

static void DXFill(TDXCom *StateObject, RECT Area, uint32 Value)
{
	for (int PixelY = Area.top < 0 ? 0 : Area.top; PixelY < Area.bottom && PixelY < 64; PixelY++)
		for (int PixelX = Area.left < 0 ? 0 : Area.left; PixelX < Area.right && PixelX < 64; PixelX++)
			DXWritePixel(StateObject, DXPixel(StateObject, PixelX, PixelY), Value);
}

// --- IDirectDrawSurface4 ---------------------------------------------------------

static HRESULT WINAPI DXBlt(TDXCom *StateObject, RECT *To, TDXCom *From, RECT *Source, DWORD Flags, DDBLTFX *Effects)
{
	DXLogf("Blt %s<-%s,%lx,%s,%s", DXName(StateObject).c_str(), DXName(From).c_str(), Flags, DXRectText(To).c_str(),
		DXRectText(Source).c_str());
	if (Effects != NULL)
		DXLogf(",FX%lu,%lx", Effects->dwSize, Effects->dwFillColor);
	DXLogf(";");
	if (StateObject->BlitFailures > 0) {
		StateObject->BlitFailures--;
		return StateObject->BlitError;
	}
	RECT TargetRect = To != NULL ? *To : RECT{0, 0, 64, 64};
	if ((Flags & DDBLT_COLORFILL) != 0) {
		DXFill(StateObject, TargetRect, Effects->dwFillColor);
	} else if (From != NULL) {
		RECT SourceRect = Source != NULL ? *Source : RECT{0, 0, 64, 64};
		int ToWidth = TargetRect.right - TargetRect.left, ToHeight = TargetRect.bottom - TargetRect.top;
		int FromWidth = SourceRect.right - SourceRect.left, FromHeight = SourceRect.bottom - SourceRect.top;
		if (ToWidth <= 0 || ToHeight <= 0 || FromWidth <= 0 || FromHeight <= 0)
			return DD_OK;
		for (int PixelY = TargetRect.top < 0 ? 0 : TargetRect.top; PixelY < TargetRect.bottom && PixelY < 64; PixelY++)
			for (int PixelX = TargetRect.left < 0 ? 0 : TargetRect.left; PixelX < TargetRect.right && PixelX < 64; PixelX++) {
				int FromX = SourceRect.left + (int)((long long)(PixelX - TargetRect.left) * FromWidth / ToWidth);
				int FromY = SourceRect.top + (int)((long long)(PixelY - TargetRect.top) * FromHeight / ToHeight);
				uint8 *DataPointer = DXPixel(From, FromX, FromY);
				if (DataPointer == NULL)
					continue;
				uint32 Value = DXReadPixel(From, DataPointer);
				if ((Flags & DDBLT_KEYSRC) != 0 && From->Keyed && Value >= From->KeyLow && Value <= From->KeyHigh)
					continue;
				DXWritePixel(StateObject, DXPixel(StateObject, PixelX, PixelY), Value);
			}
	}
	return DD_OK;
}

static HRESULT WINAPI DXFlip(TDXCom *StateObject, TDXCom *Target, DWORD Flags)
{
	DXLogf("Flip %s,%s,%lx;", DXName(StateObject).c_str(), DXName(Target).c_str(), Flags);
	if (DXCount[DXSide].Flips++ < DXScript.StillDrawing)
		return DDERR_WASSTILLDRAWING;
	return DXScript.FlipError;
}

static HRESULT WINAPI DXGetAttachedSurface(TDXCom *StateObject, DDSCAPS2 *Caps, TDXCom **Result)
{
	// Only dwCaps: the binary leaves the rest of its DDSCAPS2 uninitialised too
	// (0x4ef694 sets [esp+0x114] alone).
	DXLogf("GetAttachedSurface %s,%lx,%s;", DXName(StateObject).c_str(), Caps->dwCaps, DXWhere(Result).c_str());
	if (DXFailing("GetAttachedSurface")) {
		*Result = NULL;
		return DDERR_NOTFOUND;
	}
	TDXCom *Back = DXAlloc(DX_SURFACE);
	if (Back == NULL)
		return DDERR_OUTOFMEMORY;
	Back->Format = StateObject->Format;
	Back->Pitch = 64 * DXBytes(Back) + 8;
	Back->Width = StateObject->Width;
	Back->Height = StateObject->Height;
	Back->Caps = Caps->dwCaps;
	*Result = Back;
	DXLogf("=%s;", DXName(Back).c_str());
	return DD_OK;
}

static HRESULT WINAPI DXGetBltStatus(TDXCom *StateObject, DWORD Flags)
{
	DXLogf("GetBltStatus %s,%lx;", DXName(StateObject).c_str(), Flags);
	if (DXCount[DXSide].BltStatus++ < DXScript.StillDrawing)
		return DDERR_WASSTILLDRAWING;
	return DD_OK;
}

static HRESULT WINAPI DXGetPixelFormat(TDXCom *StateObject, DDPIXELFORMAT *Format)
{
	DXLogf("PF %s,%lu;", DXName(StateObject).c_str(), Format->dwSize);
	*Format = StateObject->Format;
	return DD_OK;
}

static HRESULT WINAPI DXGetSurfaceDesc(TDXCom *StateObject, DDSURFACEDESC2 *Desc)
{
	DXLogf("Desc %s,%lu;", DXName(StateObject).c_str(), Desc->dwSize);
	Desc->ddpfPixelFormat = StateObject->Format;
	Desc->ddsCaps.dwCaps = StateObject->Caps;
	return DD_OK;
}

static HRESULT WINAPI DXIsLost(TDXCom *StateObject)
{
	int Call = ++DXCount[DXSide].IsLost;
	DXLogf("IsLost %s;", DXName(StateObject).c_str());
	if (Call == DXScript.LostCall)
		return DDERR_SURFACELOST;
	if (Call == DXScript.OddLostCall)
		return DDERR_GENERIC;
	return DD_OK;
}

static HRESULT WINAPI DXLock(TDXCom *StateObject, RECT *Rect, DDSURFACEDESC2 *Desc, DWORD Flags, HANDLE Event)
{
	DXLogf("Lock %s,%s,%lx,%lu,%s;", DXName(StateObject).c_str(), DXRectText(Rect).c_str(), Flags, Desc->dwSize,
		Event == NULL ? "null" : "event");
	if (StateObject->LockFailures > 0) {
		StateObject->LockFailures--;
		return StateObject->LockError;
	}
	Desc->lPitch = StateObject->Pitch;
	Desc->lpSurface = StateObject->Pixels;
	Desc->ddpfPixelFormat = StateObject->Format;
	return DD_OK;
}

static HRESULT WINAPI DXSetClipper(TDXCom *StateObject, TDXCom *Clipper)
{
	DXLogf("SetClipper %s,%s;", DXName(StateObject).c_str(), DXName(Clipper).c_str());
	if (StateObject->Clipper != NULL && StateObject->Clipper->Refs > 0)
		StateObject->Clipper->Refs--;
	StateObject->Clipper = Clipper;
	if (Clipper != NULL)
		Clipper->Refs++;
	return DD_OK;
}

static HRESULT WINAPI DXSetColorKey(TDXCom *StateObject, DWORD Flags, DDCOLORKEY *Key)
{
	DXLogf("Key %s,%lx,%lx,%lx;", DXName(StateObject).c_str(), Flags, Key->dwColorSpaceLowValue,
		Key->dwColorSpaceHighValue);
	StateObject->Keyed = true;
	StateObject->KeyFlags = Flags;
	StateObject->KeyLow = Key->dwColorSpaceLowValue;
	StateObject->KeyHigh = Key->dwColorSpaceHighValue;
	return DD_OK;
}

static HRESULT WINAPI DXUnlock(TDXCom *StateObject, RECT *Rect)
{
	DXLogf("Unlock %s,%s;", DXName(StateObject).c_str(), DXRectText(Rect).c_str());
	return DD_OK;
}

// --- IDirectDrawClipper ------------------------------------------------------------

static HRESULT WINAPI DXSetHWnd(TDXCom *ComObject, DWORD Flags, HWND Window)
{
	DXLogf("SetHWnd %s,%lx,%x;", DXName(ComObject).c_str(), Flags, (unsigned)(uintptr_t)Window);
	ComObject->Window = Window;
	return DD_OK;
}

// --- IDirectDraw and IDirectDraw4 ----------------------------------------------------

static const char *DXGuidName(const GUID *Guid)
{
	if (*Guid == IID_IDirectDraw4)
		return "IDirectDraw4";
	if (*Guid == IID_IDirect3D3)
		return "IDirect3D3";
	if (*Guid == IID_IDirect3DHALDevice)
		return "HAL";
	return "other";
}

static HRESULT WINAPI DXQueryInterface1(TDXCom *ComObject, const GUID *Guid, void **Result)
{
	DXLogf("QI %s,%s,%s;", DXName(ComObject).c_str(), DXGuidName(Guid), DXWhere(Result).c_str());
	if (*Guid != IID_IDirectDraw4 || DXFailing("QI")) {
		*Result = NULL;
		return E_NOINTERFACE;
	}
	*Result = DXAlloc(DX_DD4);
	DXLogf("=%s;", DXName(*Result).c_str());
	return DD_OK;
}

static HRESULT WINAPI DXQueryInterface4(TDXCom *ComObject, const GUID *Guid, void **Result)
{
	DXLogf("QI %s,%s,%s;", DXName(ComObject).c_str(), DXGuidName(Guid), DXWhere(Result).c_str());
	if (*Guid != IID_IDirect3D3 || DXScript.QI3D != DD_OK) {
		*Result = NULL;
		return *Guid != IID_IDirect3D3 ? E_NOINTERFACE : DXScript.QI3D;
	}
	*Result = DXAlloc(DX_D3D);
	DXLogf("=%s;", DXName(*Result).c_str());
	return DD_OK;
}

static HRESULT WINAPI DXCreateClipper(TDXCom *ComObject, DWORD Flags, TDXCom **Result, IUnknown *Outer)
{
	DXLogf("CreateClipper %s,%lx,%s;", DXName(ComObject).c_str(), Flags, Outer == NULL ? "null" : "outer");
	if (DXFailing("CreateClipper")) {
		*Result = NULL;
		return DDERR_GENERIC;
	}
	*Result = DXAlloc(DX_CLIPPER);
	DXLogf("=%s;", DXName(*Result).c_str());
	return DD_OK;
}

static HRESULT WINAPI DXCreateSurface(TDXCom *ComObject, DDSURFACEDESC2 *Desc, TDXCom **Result, IUnknown *Outer)
{
	DWORD Caps = Desc->ddsCaps.dwCaps;
	DXLogf("CreateSurface %s,%lu,%lx,%lx,%lx,%lu,%lu,%lu,%s,%s;", DXName(ComObject).c_str(), Desc->dwSize, Desc->dwFlags,
		Caps, Desc->ddsCaps.dwCaps2, Desc->dwWidth, Desc->dwHeight, Desc->dwBackBufferCount,
		DXWhere(Result).c_str(), Outer == NULL ? "null" : "outer");
	bool Fail = false;
	if (DXFailing("CreateSurface")) {
		*Result = NULL;
		return DDERR_OUTOFMEMORY;
	}
	if ((Caps & DDSCAPS_PRIMARYSURFACE) != 0) {
	} else if ((Caps & DDSCAPS_3DDEVICE) != 0) {
		Fail = (Caps & DDSCAPS_VIDEOMEMORY) != 0 ? DXScript.Fail3D >= 1 : DXScript.Fail3D >= 2;
	} else if ((Caps & DDSCAPS_VIDEOMEMORY) != 0) {
		int Call = DXCount[DXSide].VideoRequests++;
		Fail = Call < 64 && (DXScript.VideoFail >> Call & 1) != 0;
	} else {
		int Call = DXCount[DXSide].SystemRequests++;
		Fail = Call < 64 && (DXScript.SystemFail >> Call & 1) != 0;
	}
	if (Fail) {
		DXLogf("=fail;");
		return DDERR_OUTOFVIDEOMEMORY;
	}
	TDXCom *StateObject = DXAlloc(DX_SURFACE);
	if (StateObject == NULL)
		return DDERR_OUTOFMEMORY;
	StateObject->Format = DXScript.Format;
	if ((Caps & DDSCAPS_3DDEVICE) != 0) {
		if (DXScript.TempBits != 0)
			StateObject->Format.dwRGBBitCount = DXScript.TempBits;
		StateObject->LockFailures = DXScript.TempLockFailures;
		StateObject->LockError = DXScript.TempLockError;
	}
	StateObject->Pitch = 64 * DXBytes(StateObject) + 8;
	StateObject->Width = (Desc->dwFlags & DDSD_WIDTH) != 0 ? Desc->dwWidth : 0;
	StateObject->Height = (Desc->dwFlags & DDSD_HEIGHT) != 0 ? Desc->dwHeight : 0;
	StateObject->Caps = Caps;
	*Result = StateObject;
	DXLogf("=%s;", DXName(StateObject).c_str());
	return DD_OK;
}

static HRESULT WINAPI DXEnumDisplayModes(TDXCom *ComObject, DWORD Flags, DDSURFACEDESC2 *Desc, void *Context,
	LPDDENUMMODESCALLBACK2 Callback)
{
	DXLogf("EnumDisplayModes %s,%lx,%s;", DXName(ComObject).c_str(), Flags, Desc == NULL ? "null" : "desc");
	for (int ItemIndex = 0; ItemIndex < DXScript.ModeCount; ItemIndex++) {
		DDSURFACEDESC2 Mode = DXScript.Modes[ItemIndex];
		HRESULT Result = Callback(&Mode, Context);
		DXLogf("cb%d=%ld;", ItemIndex, (long)Result);
		if (Result == DDENUMRET_CANCEL)
			break;
	}
	return DXScript.EnumResult;
}

static HRESULT WINAPI DXGetCaps(TDXCom *ComObject, DDCAPS *Hardware, DDCAPS *Software)
{
	DXLogf("GetCaps %s,%lu,%lu;", DXName(ComObject).c_str(), Hardware->dwSize, Software->dwSize);
	if (DXScript.CapsResult == DD_OK)
		Hardware->dwFXCaps = DXScript.FXCaps;
	return DXScript.CapsResult;
}

static HRESULT WINAPI DXGetDisplayMode(TDXCom *ComObject, DDSURFACEDESC2 *Desc)
{
	DXLogf("GetDisplayMode %s,%lu;", DXName(ComObject).c_str(), Desc->dwSize);
	Desc->ddpfPixelFormat = DXScript.Format;
	return DD_OK;
}

static HRESULT WINAPI DXSetCooperativeLevel(TDXCom *ComObject, HWND Window, DWORD Flags)
{
	DXLogf("SetCooperativeLevel %s,%x,%lx;", DXName(ComObject).c_str(), (unsigned)(uintptr_t)Window, Flags);
	return DXFailing("SetCooperativeLevel") ? DDERR_GENERIC : DD_OK;
}

static HRESULT WINAPI DXSetDisplayMode(TDXCom *ComObject, DWORD Width, DWORD Height, DWORD Bits, DWORD Refresh,
	DWORD Flags)
{
	DXLogf("SetDisplayMode %s,%lu,%lu,%lu,%lu,%lx;", DXName(ComObject).c_str(), Width, Height, Bits, Refresh, Flags);
	return DXFailing("SetDisplayMode") ? DDERR_GENERIC : DD_OK;
}

static HRESULT WINAPI DXTestCooperativeLevel(TDXCom *ComObject)
{
	DXLogf("TestCooperativeLevel %s;", DXName(ComObject).c_str());
	return DXScript.CoopTest;
}

// --- IDirect3D3, IDirect3DDevice3, IDirect3DViewport3 ----------------------------------

static HRESULT WINAPI DXCreateViewport(TDXCom *ComObject, TDXCom **Result, IUnknown *Outer)
{
	DXLogf("CreateViewport %s,%s,%s;", DXName(ComObject).c_str(), DXWhere(Result).c_str(), Outer == NULL ? "null" : "outer");
	if (DXScript.Viewport != D3D_OK)
		return DXScript.Viewport;
	*Result = DXAlloc(DX_VIEWPORT);
	DXLogf("=%s;", DXName(*Result).c_str());
	return D3D_OK;
}

static HRESULT WINAPI DXCreateDevice(TDXCom *ComObject, const GUID *Guid, TDXCom *Target, TDXCom **Result,
	IUnknown *Outer)
{
	DXLogf("CreateDevice %s,%s,%s,%s,%s;", DXName(ComObject).c_str(), DXGuidName(Guid), DXName(Target).c_str(),
		DXWhere(Result).c_str(), Outer == NULL ? "null" : "outer");
	if (DXScript.Device != D3D_OK)
		return DXScript.Device;
	TDXCom *Device = DXAlloc(DX_DEVICE);
	if (Device == NULL)
		return DDERR_OUTOFMEMORY;
	Device->Target = Target;
	*Result = Device;
	DXLogf("=%s;", DXName(Device).c_str());
	return D3D_OK;
}

static HRESULT WINAPI DXDeviceCaps(TDXCom *ComObject, D3DDEVICEDESC *Hardware, D3DDEVICEDESC *Software)
{
	DXLogf("DeviceCaps %s,%lu,%lu;", DXName(ComObject).c_str(), Hardware->dwSize, Software->dwSize);
	if (DXScript.DeviceCaps == D3D_OK) {
		Hardware->dpcTriCaps.dwSrcBlendCaps = DXScript.SrcBlend;
		Hardware->dpcTriCaps.dwDestBlendCaps = DXScript.DestBlend;
	}
	return DXScript.DeviceCaps;
}

static HRESULT WINAPI DXAddViewport(TDXCom *ComObject, TDXCom *Viewport)
{
	DXLogf("AddViewport %s,%s;", DXName(ComObject).c_str(), DXName(Viewport).c_str());
	return DXScript.AddViewport;
}

static HRESULT WINAPI DXBeginScene(TDXCom *ComObject)
{
	DXLogf("BeginScene %s;", DXName(ComObject).c_str());
	return DXScript.Begin;
}

static HRESULT WINAPI DXEndScene(TDXCom *ComObject)
{
	DXLogf("EndScene %s;", DXName(ComObject).c_str());
	return D3D_OK;
}

static HRESULT WINAPI DXSetCurrentViewport(TDXCom *ComObject, TDXCom *Viewport)
{
	DXLogf("SetCurrentViewport %s,%s;", DXName(ComObject).c_str(), DXName(Viewport).c_str());
	return DXScript.CurrentViewport;
}

static HRESULT WINAPI DXSetRenderState(TDXCom *ComObject, D3DRENDERSTATETYPE State, DWORD Value)
{
	int Call = ++DXCount[DXSide].RenderStates;
	DXLogf("SetRenderState %s,%d,%lu;", DXName(ComObject).c_str(), (int)State, Value);
	return Call == DXScript.RenderStateFail ? D3DERR_INVALIDCURRENTVIEWPORT : D3D_OK;
}

static HRESULT WINAPI DXDrawPrimitive(TDXCom *ComObject, D3DPRIMITIVETYPE Type, DWORD VertexType, void *Vertices,
	DWORD Count, DWORD Flags)
{
	DXLogf("DrawPrimitive %s,%d,%lx,%lu,%lx", DXName(ComObject).c_str(), (int)Type, VertexType, Count, Flags);
	const uint32 *Words = (const uint32 *)Vertices;
	for (DWORD ItemIndex = 0; ItemIndex < Count * 8; ItemIndex++)
		DXLogf("%s%08x", ItemIndex % 8 == 0 ? ";v" : ",", Words[ItemIndex]);
	DXLogf(";");
	if (DXScript.ShadeMode == 1 && Count == 4 && ComObject->Target != NULL) {
		const D3DTLVERTEX *VertexData = (const D3DTLVERTEX *)Vertices;
		RECT Area = {(LONG)VertexData[0].sx, (LONG)VertexData[0].sy, (LONG)VertexData[3].sx, (LONG)VertexData[3].sy};
		D3DCOLOR Color = VertexData[0].color;
		DXFill(ComObject->Target, Area, DXColorPixel(ComObject->Target, (Color >> 16) & 255, (Color >> 8) & 255, Color & 255));
	}
	return D3D_OK;
}

static HRESULT WINAPI DXSetViewport(TDXCom *ComObject, D3DVIEWPORT *Data)
{
	DXLogf("SetViewport %s", DXName(ComObject).c_str());
	const uint32 *Words = (const uint32 *)Data;
	for (size_t ItemIndex = 0; ItemIndex < sizeof(D3DVIEWPORT) / 4; ItemIndex++)
		DXLogf(",%08x", Words[ItemIndex]);
	DXLogf(";");
	return DXScript.SetViewport;
}

static void DXInitVtables()
{
	memset(DXVtables, 0, sizeof(DXVtables));
	for (int KindIndex = 0; KindIndex < DX_KINDS; KindIndex++)
		DXVtables[KindIndex][2] = (void *)DXRelease;
	DXVtables[DX_DD1][0] = (void *)DXQueryInterface1;
	void **DD = DXVtables[DX_DD4];
	DD[0] = (void *)DXQueryInterface4;
	DD[4] = (void *)DXCreateClipper;
	DD[6] = (void *)DXCreateSurface;
	DD[8] = (void *)DXEnumDisplayModes;
	DD[11] = (void *)DXGetCaps;
	DD[12] = (void *)DXGetDisplayMode;
	DD[20] = (void *)DXSetCooperativeLevel;
	DD[21] = (void *)DXSetDisplayMode;
	DD[26] = (void *)DXTestCooperativeLevel;
	void **StateObject = DXVtables[DX_SURFACE];
	StateObject[5] = (void *)DXBlt;
	StateObject[11] = (void *)DXFlip;
	StateObject[12] = (void *)DXGetAttachedSurface;
	StateObject[13] = (void *)DXGetBltStatus;
	StateObject[21] = (void *)DXGetPixelFormat;
	StateObject[22] = (void *)DXGetSurfaceDesc;
	StateObject[24] = (void *)DXIsLost;
	StateObject[25] = (void *)DXLock;
	StateObject[28] = (void *)DXSetClipper;
	StateObject[29] = (void *)DXSetColorKey;
	StateObject[32] = (void *)DXUnlock;
	DXVtables[DX_CLIPPER][8] = (void *)DXSetHWnd;
	DXVtables[DX_D3D][6] = (void *)DXCreateViewport;
	DXVtables[DX_D3D][8] = (void *)DXCreateDevice;
	void **Device = DXVtables[DX_DEVICE];
	Device[3] = (void *)DXDeviceCaps;
	Device[5] = (void *)DXAddViewport;
	Device[9] = (void *)DXBeginScene;
	Device[10] = (void *)DXEndScene;
	Device[12] = (void *)DXSetCurrentViewport;
	Device[22] = (void *)DXSetRenderState;
	Device[28] = (void *)DXDrawPrimitive;
	DXVtables[DX_VIEWPORT][5] = (void *)DXSetViewport;
}

// --- Imports and the other functions both sides call -------------------------------

static HRESULT WINAPI DXDirectDrawCreate(GUID *Guid, TDXCom **Result, IUnknown *Outer)
{
	DXLogf("DirectDrawCreate %s,%s,%s;", Guid == NULL ? "null" : "guid", DXWhere(Result).c_str(),
		Outer == NULL ? "null" : "outer");
	if (DXFailing("DirectDrawCreate")) {
		*Result = NULL;
		return DDERR_GENERIC;
	}
	*Result = DXAlloc(DX_DD1);
	DXLogf("=%s;", DXName(*Result).c_str());
	return DD_OK;
}

static DWORD WINAPI DXTimeGetTime()
{
	uint32 Value = DXScript.Clock + DXCount[DXSide].Time++ * DXScript.ClockStep;
	DXLogf("Time%u;", Value);
	return Value;
}

static void WINAPI DXSleep(DWORD Delay)
{
	DXLogf("Sleep%lu;", Delay);
}

static BOOL WINAPI DXGetClientRect(HWND Window, RECT *Rect)
{
	DXLogf("GetClientRect %x,%s;", (unsigned)(uintptr_t)Window, DXWhere(Rect).c_str());
	*Rect = DXScript.Client;
	if (Window == (HWND)0x5678) {
		Rect->right += 16;
		Rect->bottom += 16;
	}
	return TRUE;
}

static BOOL WINAPI DXClientToScreen(HWND Window, POINT *Point)
{
	DXLogf("ClientToScreen %x,%s,%ld,%ld;", (unsigned)(uintptr_t)Window, DXWhere(Point).c_str(), Point->x, Point->y);
	Point->x += DXScript.Origin.x;
	Point->y += DXScript.Origin.y;
	return TRUE;
}

static int WINAPI DXGetSystemMetrics(int Index)
{
	DXLogf("GetSystemMetrics %d;", Index);
	return Index == SM_CXSCREEN ? DXScript.Metrics[0] : Index == SM_CYSCREEN ? DXScript.Metrics[1] : -1;
}

static BOOL WINAPI DXSetRect(RECT *Rect, int Left, int Top, int Right, int Bottom)
{
	DXLogf("SetRect %s,%d,%d,%d,%d;", DXWhere(Rect).c_str(), Left, Top, Right, Bottom);
	Rect->left = Left;
	Rect->top = Top;
	Rect->right = Right;
	Rect->bottom = Bottom;
	return TRUE;
}

static void __cdecl DXSetDefaultNoStretching(bool Value)
{
	DXLogf("DefaultNoStretching%d;", Value ? 1 : 0);
}

static int __cdecl DXGetConfineFramerate()
{
	DXLogf("ConfineFramerate;");
	return DXScript.Framerate;
}

static TSurface *__cdecl DXGetSurfaceFake()
{
	DXLogf("GetSurface;");
	return (TSurface *)DXObjects[DXSide];
}

static void *__cdecl DXPatternNew(size_t Size)
{
	void *Memory = malloc(Size == 0 ? 1 : Size);
	memset(Memory, 0xab, Size);
	return Memory;
}

// --- Patches (all restored at the end) ----------------------------------------------

struct TDXSavedBytes {
	uint8 *Code;
	uint8 Bytes[5];
};
static std::vector<TDXSavedBytes> DXSaved;

static void DXRedirect(uint32 Address, void *Target)
{
	TDXSavedBytes Saved;
	Saved.Code = (uint8 *)(uintptr_t)Address;
	DWORD Protection;
	VirtualProtect(Saved.Code, 5, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(Saved.Bytes, Saved.Code, 5);
	DXSaved.push_back(Saved);
	PatchJump(Address, Target);
	VirtualProtect(Saved.Code, 5, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Saved.Code, 5);
}

struct TDXSavedSlot {
	void **Slot;
	void *Value;
};
static std::vector<TDXSavedSlot> DXSavedSlots;

static void DXSetSlot(void **Slot, void *Value)
{
	DXSavedSlots.push_back({Slot, *Slot});
	DWORD Protection;
	VirtualProtect(Slot, sizeof(void *), PAGE_READWRITE, &Protection);
	*Slot = Value;
	VirtualProtect(Slot, sizeof(void *), Protection, &Protection);
}

// Our executable's IAT entry of an imported function.
static void **DXFindImport(const char *Dll, const char *Name)
{
	uint8 *Base = (uint8 *)GetModuleHandleA(NULL);
	IMAGE_NT_HEADERS *Headers = (IMAGE_NT_HEADERS *)(Base + ((IMAGE_DOS_HEADER *)Base)->e_lfanew);
	IMAGE_DATA_DIRECTORY &Directory = Headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	for (IMAGE_IMPORT_DESCRIPTOR *Descriptor = (IMAGE_IMPORT_DESCRIPTOR *)(Base + Directory.VirtualAddress); Descriptor->Name != 0; Descriptor++) {
		if (_stricmp((const char *)(Base + Descriptor->Name), Dll) != 0 || Descriptor->OriginalFirstThunk == 0)
			continue;
		IMAGE_THUNK_DATA *Names = (IMAGE_THUNK_DATA *)(Base + Descriptor->OriginalFirstThunk);
		IMAGE_THUNK_DATA *Slots = (IMAGE_THUNK_DATA *)(Base + Descriptor->FirstThunk);
		for (; Names->u1.AddressOfData != 0; Names++, Slots++) {
			if (IMAGE_SNAP_BY_ORDINAL(Names->u1.Ordinal))
				continue;
			IMAGE_IMPORT_BY_NAME *Import = (IMAGE_IMPORT_BY_NAME *)(Base + Names->u1.AddressOfData);
			if (strcmp((const char *)Import->Name, Name) == 0)
				return (void **)&Slots->u1.Function;
		}
	}
	return NULL;
}

static void DXPatchAll()
{
	struct {
		const char *Dll, *Name;
		uint32 Image;
		void *Fake;
	} Imports[] = {
		{"ddraw.dll", "DirectDrawCreate", 0x55b03c, (void *)DXDirectDrawCreate},
		{"WINMM.dll", "timeGetTime", 0x55b59c, (void *)DXTimeGetTime},
		{"KERNEL32.dll", "Sleep", 0x55b1f0, (void *)DXSleep},
		{"USER32.dll", "SetRect", 0x55b4c0, (void *)DXSetRect},
		{"USER32.dll", "GetClientRect", 0x55b514, (void *)DXGetClientRect},
		{"USER32.dll", "ClientToScreen", 0x55b510, (void *)DXClientToScreen},
		{"USER32.dll", "GetSystemMetrics", 0x55b4f8, (void *)DXGetSystemMetrics},
	};
	for (auto &Import : Imports) {
		void **Ours = DXFindImport(Import.Dll, Import.Name);
		CHECK(Ours != NULL, "DX: our import %s!%s not found", Import.Dll, Import.Name);
		if (Ours != NULL)
			DXSetSlot(Ours, Import.Fake);
		DXSetSlot((void **)(uintptr_t)Import.Image, Import.Fake);
	}
	struct {
		uint32 Image;
		void *Ours, *Fake;
	} Functions[] = {
		{0x5237e7, (void *)(void *(__cdecl *)(size_t))operator new, (void *)DXPatternNew},
		{0x523817, (void *)(void *(__cdecl *)(size_t))operator new[], (void *)DXPatternNew},
		{0x4eaa90, (void *)GetSurface, (void *)DXGetSurfaceFake},
		{0x4db360, (void *)SetDefaultNoStretching, (void *)DXSetDefaultNoStretching},
		{0x4db410, (void *)GetConfineFramerate, (void *)DXGetConfineFramerate},
	};
	for (auto &SourceRect : Functions) {
		DXRedirect(SourceRect.Image, SourceRect.Fake);
		DXRedirect((uint32)(uintptr_t)SourceRect.Ours, SourceRect.Fake);
	}
}

static void DXRestoreAll()
{
	for (size_t Index = DXSaved.size(); Index-- > 0;) {
		DWORD Protection;
		VirtualProtect(DXSaved[Index].Code, 5, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(DXSaved[Index].Code, DXSaved[Index].Bytes, 5);
		VirtualProtect(DXSaved[Index].Code, 5, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), DXSaved[Index].Code, 5);
	}
	DXSaved.clear();
	for (size_t Index = DXSavedSlots.size(); Index-- > 0;) {
		DWORD Protection;
		VirtualProtect(DXSavedSlots[Index].Slot, sizeof(void *), PAGE_READWRITE, &Protection);
		*DXSavedSlots[Index].Slot = DXSavedSlots[Index].Value;
		VirtualProtect(DXSavedSlots[Index].Slot, sizeof(void *), Protection, &Protection);
	}
	DXSavedSlots.clear();
}

// --- The original's non-virtual functions -------------------------------------------

typedef uint32 (__fastcall *TDXGetPixelColorFn)(void *, void *, IDirectDrawSurface4 *, TRGBColor);
typedef bool (__fastcall *TDXCreateFn)(void *, void *, IDirectDrawSurface4 **, int, int, bool);
typedef void (__fastcall *TDXShadeRectangleFn)(void *, void *, int, TRect, TRGBColor, TRGBColor, TRGBColor,
	TRGBColor);
typedef bool (__fastcall *TDXTestShadingFn)(void *, void *, int);

static uint32 DXGetPixelColor(int Side, IDirectDrawSurface4 *Surface, TRGBColor Color)
{
	if (Side == DX_OURS)
		return ((TSurfaceDX *)DXObjects[0])->GetPixelColor(Surface, Color);
	return At<TDXGetPixelColorFn>(0x4eaeb0)(DXObjects[1], NULL, Surface, Color);
}

static bool DXCreateOffscreen(int Side, bool ThreeD, IDirectDrawSurface4 **Result, int Width, int Height,
	bool VideoOnly)
{
	TSurfaceDX *Ours = (TSurfaceDX *)DXObjects[0];
	if (Side == DX_OURS)
		return ThreeD ? Ours->Create3DSurface(Result, Width, Height, VideoOnly)
			: Ours->CreateOffscreenSurface(Result, Width, Height, VideoOnly);
	return At<TDXCreateFn>(ThreeD ? 0x4ec470 : 0x4ec3a0)(DXObjects[1], NULL, Result, Width, Height, VideoOnly);
}

static void DXShadeRectangle(int Side, int Surface, TRect Rect, const TRGBColor *ComObject)
{
	if (Side == DX_OURS)
		((TSurfaceDX *)DXObjects[0])->ShadeRectangle(Surface, Rect, ComObject[0], ComObject[1], ComObject[2], ComObject[3]);
	else
		At<TDXShadeRectangleFn>(0x4eca80)(DXObjects[1], NULL, Surface, Rect, ComObject[0], ComObject[1], ComObject[2], ComObject[3]);
}

static bool DXTestShading(int Side, int Surface)
{
	if (Side == DX_OURS)
		return ((TSurfaceDX *)DXObjects[0])->TestShading(Surface);
	return At<TDXTestShadingFn>(0x4ecdd0)(DXObjects[1], NULL, Surface);
}

// --- Comparison ------------------------------------------------------------------------

struct TDXGlobal {
	const char *Name;
	int *Ours;
	uint32 Image;
};
static const TDXGlobal DXGlobals[] = {
	{"InitStep", &InitStep, 0x71d0bc}, {"BlitCount", &BlitCount, 0x71d0d0},
	{"BlitPixels", &BlitPixels, 0x71d0d4}, {"Data71D0D8", &Data71D0D8, 0x71d0d8},
	{"ScreenWidth", &ScreenWidth, 0x71d0dc}, {"ScreenHeight", &ScreenHeight, 0x71d0e0},
	{"ScreenDepth", &ScreenDepth, 0x71d0e4}, {"LastBlitCount", &LastBlitCount, 0x71d0e8},
	{"LastBlitPixels", &LastBlitPixels, 0x71d0ec}, {"NumberOfSurfaces", &NumberOfSurfaces, 0x71d0f0},
	{"Data71D0F4", &Data71D0F4, 0x71d0f4}, {"CacheSurfaceWidth", &CacheSurfaceWidth, 0x5c2568},
	{"CacheSurfaceHeight", &CacheSurfaceHeight, 0x5c256c},
};

static void DXSetGlobal(int *Ours, uint32 Image, int Value)
{
	*Ours = Value;
	Global<int>(Image) = Value;
}

static void DXNormalize(uint8 *State, int Side)
{
	memset(State, 0, 4);
	int Offsets[26];
	int Count = 0;
	Offsets[Count++] = 0x74;
	for (int ItemIndex = 0; ItemIndex < NUMBER_OF_SURFACES; ItemIndex++)
		Offsets[Count++] = 0x88 + ItemIndex * 4;
	Offsets[Count++] = 0x240;
	Offsets[Count++] = 0x244;
	Offsets[Count++] = 0x248;
	for (int ItemIndex = 0; ItemIndex < Count; ItemIndex++) {
		void *&Pointer = *(void **)(State + Offsets[ItemIndex]);
		Pointer = (void *)(uintptr_t)DXIdOf(Pointer, Side);
	}
}

static void DXCompareAll(const char *Name)
{
	CHECK(DXLog[0] == DXLog[1], "DX %s %d %s: log\n ours     %s\n original %s", DXPhase, DXOperation, Name,
		DXLog[0].c_str(), DXLog[1].c_str());
	if (DXLogs != NULL)
		fprintf(DXLogs, "%s %d %s: %s\n", DXPhase, DXOperation, Name, DXLog[0].c_str());
	if (DXObjects[0] != NULL && DXObjects[1] != NULL) {
		uint8 State[2][0x250];
		for (int Side = 0; Side < 2; Side++) {
			memcpy(State[Side], DXObjects[Side], 0x250);
			DXNormalize(State[Side], Side);
		}
		if (memcmp(State[0], State[1], 0x250) != 0) {
			std::string Text;
			for (int Offset = 0; Offset < 0x250 && Text.size() < 400; Offset += 4) {
				uint32 OurValue = *(uint32 *)(State[0] + Offset), ImageValue = *(uint32 *)(State[1] + Offset);
				if (OurValue != ImageValue) {
					char Buffer[64];
					snprintf(Buffer, sizeof(Buffer), " +0x%x %08x/%08x", Offset, OurValue, ImageValue);
					Text += Buffer;
				}
			}
			CHECK(false, "DX %s %d %s: object state (ours/original)%s", DXPhase, DXOperation, Name, Text.c_str());
		} else {
			Checks++;
		}
	}
	// SurfaceMemory through the functions themselves (not the vtables: the
	// destructor leaves TSurface's, whose slot is _purecall).
	for (int ItemIndex = 0; ItemIndex < NUMBER_OF_SURFACES; ItemIndex++) {
		int OurValue = ((TSurfaceDX *)NULL)->TSurfaceDX::GetSurfaceMemory(ItemIndex);
		int ImageValue = At<int (__fastcall *)(void *, void *, int)>(0x4eae80)(NULL, NULL, ItemIndex);
		CHECK(OurValue == ImageValue, "DX %s %d %s: SurfaceMemory[%d] %d, original %d", DXPhase, DXOperation, Name, ItemIndex, OurValue, ImageValue);
	}
	for (auto &GlobalValue : DXGlobals)
		CHECK(*GlobalValue.Ours == Global<int>(GlobalValue.Image), "DX %s %d %s: %s %d, original %d", DXPhase, DXOperation, Name,
			GlobalValue.Name, *GlobalValue.Ours, Global<int>(GlobalValue.Image));
	for (int KindIndex = 0; KindIndex < DX_KINDS; KindIndex++) {
		int High = DXHigh[0][KindIndex] > DXHigh[1][KindIndex] ? DXHigh[0][KindIndex] : DXHigh[1][KindIndex];
		for (int ItemIndex = 0; ItemIndex < High; ItemIndex++) {
			TDXCom &OurValue = DXPool[0][KindIndex][ItemIndex], &ImageValue = DXPool[1][KindIndex][ItemIndex];
			bool Same = OurValue.Refs == ImageValue.Refs && OurValue.Releases == ImageValue.Releases && OurValue.Keyed == ImageValue.Keyed
				&& OurValue.KeyFlags == ImageValue.KeyFlags && OurValue.KeyLow == ImageValue.KeyLow && OurValue.KeyHigh == ImageValue.KeyHigh && OurValue.Caps == ImageValue.Caps
				&& OurValue.Width == ImageValue.Width && OurValue.Height == ImageValue.Height && OurValue.LockFailures == ImageValue.LockFailures
				&& OurValue.BlitFailures == ImageValue.BlitFailures && OurValue.Window == ImageValue.Window
				&& memcmp(&OurValue.Format, &ImageValue.Format, sizeof(OurValue.Format)) == 0
				&& (OurValue.Clipper == NULL ? -1 : OurValue.Clipper->ID) == (ImageValue.Clipper == NULL ? -1 : ImageValue.Clipper->ID)
				&& (OurValue.Target == NULL ? -1 : OurValue.Target->ID) == (ImageValue.Target == NULL ? -1 : ImageValue.Target->ID);
			CHECK(Same, "DX %s %d %s: fake %s%d state: refs %d/%d releases %d/%d key %d,%lx/%d,%lx", DXPhase,
				DXOperation, Name, DXKindNames[KindIndex], ItemIndex, OurValue.Refs, ImageValue.Refs, OurValue.Releases, ImageValue.Releases, OurValue.Keyed, OurValue.KeyLow,
				ImageValue.Keyed, ImageValue.KeyLow);
			if (KindIndex == DX_SURFACE)
				CHECK(memcmp(DXPixelData[0][ItemIndex], DXPixelData[1][ItemIndex], DX_PIXELS) == 0, "DX %s %d %s: surface S%d pixels",
					DXPhase, DXOperation, Name, ItemIndex);
		}
	}
}

template <class Body> static void DXRun(const char *Name, Body Run)
{
	DXOperation++;
	DXCover("op %s", Name);
	if (DXTrace) {
		fprintf(stderr, "DX %s %d %s\n", DXPhase, DXOperation, Name);
		fflush(stderr);
	}
	for (int Side = 0; Side < 2; Side++) {
		DXSide = Side;
		DXLog[Side].clear();
		memset(&DXCount[Side], 0, sizeof(TDXCounters));
		DXRunning = true;
		Run(Side, (TSurface *)DXObjects[Side]);
		DXRunning = false;
	}
	DXSide = DX_OURS;
	DXCompareAll(Name);
}

static TDXCom *DXSurfaceOf(int Side, int Index)
{
	int Found;
	void *Pointer = DXField<void *>(DXObjects[Side], 0x88 + Index * 4);
	return DXFind(Pointer, &Found);
}

static TSurfaceDX *DXOurs()
{
	return (TSurfaceDX *)DXObjects[DX_OURS];
}

// --- Object creation and destruction -----------------------------------------------

static void DXCreateObjects()
{
	DXRun("CreateSurfaceDX", [&](int Side, TSurface *) {
		DXObjects[Side] = Side == DX_OURS ? (void *)CreateSurfaceDX() : At<void *(__cdecl *)()>(0x4eca00)();
	});
}

static void DXDestroyObjects()
{
	int Value = 1 + Random() % 20;
	DXSetGlobal(&NumberOfSurfaces, 0x71d0f0, Value);
	DXRun("delete", [&](int Side, TSurface *Object) {
		if (Side == DX_OURS)
			delete Object;
		else
			DXVirtual<void *>(Object, 0, 1);
		DXObjects[Side] = NULL;
	});
	DXObjects[0] = DXObjects[1] = NULL;
	CHECK(NumberOfSurfaces == 0, "DX delete: NumberOfSurfaces %d", NumberOfSurfaces);
}

static void DXPhaseFactory()
{
	DXPhase = "factory";
	for (int Round = 0; Round < 8; Round++) {
		DXCreateObjects();
		if (Round % 2 == 0) {
			DXDestroyObjects();
			continue;
		}
		int Value = 1 + Random() % 20;
		DXSetGlobal(&NumberOfSurfaces, 0x71d0f0, Value);
		DXRun("~TSurfaceDX", [&](int Side, TSurface *Object) {
			if (Side == DX_OURS)
				((TSurfaceDX *)Object)->~TSurfaceDX();
			else
				At<void (__fastcall *)(void *, void *)>(0x4eae90)(Object, NULL);
		});
		CHECK(NumberOfSurfaces == 0, "DX ~TSurfaceDX: NumberOfSurfaces %d", NumberOfSurfaces);
		free(DXObjects[0]);
		free(DXObjects[1]);
		DXObjects[0] = DXObjects[1] = NULL;
	}
}

// --- Drawing on prepared objects ----------------------------------------------------

static TRect DXRandomRect(int Low, int Span, int MaxSize)
{
	// Issue #55: the arguments right to left, as the oracle's.
	const int Height = (int)(Random() % MaxSize) - 2, Width = (int)(Random() % MaxSize) - 2;
	const int Y = Low + (int)(Random() % Span), X = Low + (int)(Random() % Span);
	return TRect(X, Y, Width, Height);
}

static TRGBColor DXRandomColor()
{
	const int B = Random() % 256, G = Random() % 256, R = Random() % 256; // issue #55: right to left
	return TRGBColor(R, G, B);
}

static void DXPrepareDrawObjects(int Round)
{
	for (int Side = 0; Side < 2; Side++) {
		DXSide = Side;
		void *Object = DXObjects[Side];
		memset((uint8 *)Object + 4, 0, 0x250 - 4);
		DXField<int>(Object, 0x10) = WINDOWED_MODE;
		DXField<HWND>(Object, 8) = (HWND)0x1234;
		for (int Index = 0; Index < NUMBER_OF_SURFACES; Index++) {
			TDXCom *StateObject = DXAlloc(DX_SURFACE);
			DXFormat(&StateObject->Format, Round);
			StateObject->Pitch = 64 * DXBytes(StateObject) + 8;
			StateObject->Width = StateObject->Height = 64;
			memset(StateObject->Pixels, 0x5a, DX_PIXELS);
			DXField<void *>(Object, 0x88 + Index * 4) = StateObject;
			DXField<RECT>(Object, 0xe0 + Index * sizeof(RECT)) = {0, 0, 64, 64};
		}
		TDXCom *DD = DXAlloc(DX_DD4);
		DXField<void *>(Object, 0x74) = DD;
		DXField<RECT>(Object, 0x78) = {30, 40, 94, 104};
		if (Round % 4 != 3) {
			TDXCom *D3D = DXAlloc(DX_D3D);
			TDXCom *Device = DXAlloc(DX_DEVICE);
			Device->Target = &DXPool[Side][DX_SURFACE][TEMP_SURFACE];
			TDXCom *Viewport = DXAlloc(DX_VIEWPORT);
			DXField<void *>(Object, 0x240) = D3D;
			DXField<void *>(Object, 0x244) = Device;
			DXField<void *>(Object, 0x248) = Viewport;
		}
	}
	DXSide = DX_OURS;
}

static void DXPhaseDraw()
{
	DXPhase = "draw";
	for (int Round = 0; Round < 50; Round++) {
		DXResetPools();
		DXDefaultScript(Round);
		DXCreateObjects();
		DXPrepareDrawObjects(Round);
		DXRun("prepared", [&](int, TSurface *) {});
		for (int Operation = 0; Operation < 200; Operation++) {
			int Kind = Operation < 32 ? Operation : Random() % 32;
			int Surface = 2 + Random() % 4;
			int Source = 6 + Random() % 4;
			int PosX = 4 + Random() % 12, PosY = 4 + Random() % 12;
			int Width = 1 + Random() % 8, Height = 1 + Random() % 8;
			int FromWidth = 1 + Random() % 12, FromHeight = 1 + Random() % 12;
			int ToWidth = 1 + Random() % 12, ToHeight = 1 + Random() % 12;
			const int Third = 3 * (Random() % 3); // issue #55: the left operand first
			int StretchTo = 1 + Third + (Random() % 2); // 1, 2?, 4, 5, 7, 8
			if (StretchTo == 2 || StretchTo == 3)
				StretchTo = 6;
			int LineWidth = 1 + Random() % 3;
			TRect Rect(PosX, PosY, Width, Height);
			TRect Clip = Random() % 3 == 0 ? DXRandomRect(0, 24, 30) : TRect(8, 8, 12, 12);
			TRGBColor Color = DXRandomColor(), Key = DXRandomColor(), Highlight = DXRandomColor();
			uint8 Data[8 * 8 * 3], Mask[8 * 8 * 3];
			TRGBColor Colors[12];
			for (int ItemIndex = 0; ItemIndex < 12; ItemIndex++)
				Colors[ItemIndex] = DXRandomColor();
			if (Operation % 3 == 0) {
				Colors[0] = TRGBColor(80, 100, 120);
				Colors[1] = TRGBColor(0, 255, 0);
			}
			const uint8 Markers[6][3] = {{255, 255, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {12, 34, 56}, {0, 0, 0}};
			for (int Pixel = 0; Pixel < 64; Pixel++) {
				for (int Channel = 0; Channel < 3; Channel++)
					Data[Pixel * 3 + Channel] = (uint8)Random();
				if (Random() % 3 == 0) {
					Data[Pixel * 3] = (uint8)Colors[1].R;
					Data[Pixel * 3 + 1] = (uint8)Colors[1].G;
					Data[Pixel * 3 + 2] = (uint8)Colors[1].B;
				}
				memcpy(Mask + Pixel * 3, Markers[Random() % 6], 3);
			}
			TRect Rects[3];
			for (int ItemIndex = 0; ItemIndex < 3; ItemIndex++)
				Rects[ItemIndex] = DXRandomRect(0, 40, 24);
			int Count = Random() % 4;
			int LockFailures = Operation % 7 == 0 ? 1 + Random() % 2 : 0;
			HRESULT LockError = Random() % 2 == 0 ? DDERR_SURFACEBUSY : E_FAIL;
			int BlitFailures = Operation % 9 == 0 ? 1 + Random() % 2 : 0;
			HRESULT BlitError = Random() % 2 == 0 ? DDERR_SURFACEBUSY : E_FAIL;
			DXScript.Begin = Random() % 6 == 0 ? D3DERR_SCENE_IN_SCENE : D3D_OK;
			DXScript.ShadeMode = Random() % 3 == 0 ? 0 : 1;
			int ViewMode = Random() % 3;
			int Index = Random() % NUMBER_OF_SURFACES;
			int Value = (int)(Random() % 300) - 20;
			RECT ClipTo = {(LONG)(Random() % 60) - 20, (LONG)(Random() % 60) - 20, 0, 0};
			ClipTo.right = ClipTo.left + (LONG)(Random() % 45) - 5;
			ClipTo.bottom = ClipTo.top + (LONG)(Random() % 45) - 5;
			RECT ClipFrom = {(LONG)(Random() % 30), (LONG)(Random() % 30), (LONG)(Random() % 60), (LONG)(Random() % 60)};
			int Accessor = Random() % 12;
			if (Kind >= 20 && Kind <= 22 && Random() % 4 != 0)
				Surface = TEMP_SURFACE;
			for (int Side = 0; Side < 2; Side++) {
				TDXCom *Target = &DXPool[Side][DX_SURFACE][Kind >= 15 && Kind <= 18 ? StretchTo : Surface];
				Target->LockFailures = LockFailures;
				Target->LockError = LockError;
				Target->BlitFailures = BlitFailures;
				Target->BlitError = BlitError;
				if (Kind == 28)
					DXField<int>(DXObjects[Side], 0x10) = ViewMode;
			}
			TRect Output[2];
			uint32 Result[2] = {0, 0};
			RECT Clipped[2][2];
			double FPS[2] = {0, 0};
			static const char *const Names[32] = {"SetPixel", "WriteImage", "WriteMaskedImage",
				"WriteMaskedImage(no mask)", "FillRectangle", "FillRectangleClipped", "DrawHorizontalLine",
				"DrawVerticalLine", "DrawFrame", "DrawFrameClipped", "Blit", "BlitClipped", "BlitRect",
				"BlitRectClipped", "StretchBlitRect", "StretchBlitUnclipped(same)", "StretchBlitUnclipped(scaled)",
				"StretchBlit(same)", "StretchBlit(scaled)", "GetSurfaceRect", "ShadeRectangles", "ShadeRectangle",
				"TestShading", "GetPixelColor", "SetColorKey", "SetSourceColorKey", "ClipRect", "accessor",
				"UpdateWindowRect", "AttachWindow", "BlitColorKeyed", "FillFrame"};
			DXRun(Names[Kind], [&](int Side, TSurface *Object) {
				switch (Kind) {
				case 0: Object->SetPixel(Surface, PosX, PosY, Color); break;
				case 1: Object->WriteImage(Surface, PosX, PosY, Width, Height, Data); break;
				case 2: Object->WriteMaskedImage(Surface, PosX, PosY, Width, Height, Data, Mask, Colors); break;
				case 3: Object->WriteMaskedImage(Surface, PosX, PosY, Width, Height, Data, NULL, Colors); break;
				case 4: Object->FillRectangle(Surface, Rect, Color); break;
				case 5: Object->FillRectangleClipped(Surface, Rect, Color, Clip); break;
				case 6: Object->DrawHorizontalLine(Surface, PosX, PosY, Width, Color); break;
				case 7: Object->DrawVerticalLine(Surface, PosX, PosY, Height, Color); break;
				case 8: Object->DrawFrame(Surface, Rect, LineWidth, Color); break;
				case 9: Object->DrawFrameClipped(Surface, Rect, LineWidth, Color, Clip); break;
				case 10: Object->Blit(Surface, PosX, PosY, Source, TRect(1, 2, Width, Height), Key); break;
				case 11: Object->BlitClipped(Surface, PosX, PosY, Source, TRect(1, 2, Width, Height), Key, Clip); break;
				case 12: Object->BlitRect(Surface, PosX, PosY, Source, TRect(1, 2, Width, Height)); break;
				case 13: Object->BlitRectClipped(Surface, PosX, PosY, Source, TRect(3, 1, Width, Height), Clip); break;
				case 14: Object->StretchBlitRect(Surface, Rect, Source, TRect(1, 2, FromWidth, FromHeight)); break;
				case 15:
					Object->StretchBlitUnclipped(StretchTo, TRect(PosX, PosY, FromWidth, FromHeight), Source,
						TRect(2, 3, FromWidth, FromHeight), Key, Highlight);
					break;
				case 16:
					Object->StretchBlitUnclipped(StretchTo, TRect(PosX, PosY, ToWidth, ToHeight == FromHeight
						&& ToWidth == FromWidth ? ToHeight + 1 : ToHeight), Source, TRect(2, 3, FromWidth, FromHeight),
						Key, Highlight);
					break;
				case 17:
					Object->StretchBlit(StretchTo, TRect(PosX, PosY, FromWidth, FromHeight), Source,
						TRect(2, 3, FromWidth, FromHeight), Key, Highlight, Clip);
					break;
				case 18:
					Object->StretchBlit(StretchTo, TRect(PosX, PosY, ToWidth, ToHeight == FromHeight
						&& ToWidth == FromWidth ? ToHeight + 1 : ToHeight), Source, TRect(2, 3, FromWidth, FromHeight),
						Key, Highlight, Clip);
					break;
				case 19: Object->GetSurfaceRect(Index, &Output[Side]); break;
				case 20: Object->ShadeRectangles(Surface, Count, Rects, Colors); break;
				case 21: DXShadeRectangle(Side, Surface, Rects[0], Colors); break;
				case 22: Result[Side] = DXTestShading(Side, Surface); break;
				case 23:
					Result[Side] = DXGetPixelColor(Side, (IDirectDrawSurface4 *)DXField<void *>(Object,
						0x88 + Source * 4), Color);
					break;
				case 24: Object->SetColorKey(Source, Key); break;
				case 25: Object->SetSourceColorKey(Index, Key); break;
				case 26:
					Clipped[Side][0] = ClipTo;
					Clipped[Side][1] = ClipFrom;
					Result[Side] = Object->ClipRect(Clip, &Clipped[Side][0], &Clipped[Side][1]);
					break;
				case 27:
					switch (Accessor) {
					case 0: Result[Side] = Object->CanShadeRectangles(); break;
					case 1: Result[Side] = Object->IsUnavailable(); break;
					case 2: FPS[Side] = Object->GetFPS(); break;
					case 3: Result[Side] = Object->GetCacheSurfaces(); break;
					case 4: Result[Side] = Object->SurfaceRecreated(Index); break;
					case 5: Result[Side] = Object->GetSurfaceMemory(Index); break;
					case 6: Result[Side] = Object->TestEngine(); break;
					case 7: Object->SelectSurface(Value); break;
					case 8: Object->SetTextureFilter(Value); break;
					case 9: Object->AddDirtyRect(TRect(Value, 1, 2, 3)); break;
					case 10: Object->SetSurfaceRect(Value, 1, 2, 3, 4); break;
					case 11: Object->SetFramerateLimit(Value); break;
					}
					break;
				case 28: Object->UpdateWindowRect(); break;
				case 29: Object->AttachWindow(); break;
				case 30:
					Object->SetColorKey(Source, Key);
					Object->BlitClipped(Surface, PosX - 6, PosY - 6, Source, TRect(0, 0, Width + 8, Height + 8), Key,
						Clip);
					break;
				case 31: Object->DrawFrame(Surface, TRect(PosX, PosY, LineWidth, Height + 4), LineWidth, Color); break;
				}
			});
			if (Kind == 19)
				CHECK(memcmp(&Output[0], &Output[1], sizeof(TRect)) == 0, "DX GetSurfaceRect(%d)", Index);
			if (Kind == 22)
				DXCover("TestShading %s", Result[0] ? "true" : "false");
			if (Kind == 26) {
				CHECK(memcmp(Clipped[0], Clipped[1], sizeof(Clipped[0])) == 0, "DX ClipRect rectangles");
				DXCover("ClipRect %s", Result[0] ? "true" : "false");
			}
			CHECK(Result[0] == Result[1], "DX %s: result %u, original %u", Names[Kind], Result[0], Result[1]);
			CHECK(memcmp(&FPS[0], &FPS[1], sizeof(double)) == 0, "DX GetFPS");
			if (Kind == 23 || Kind == 21)
				DXCover("%s %lu bits", Names[Kind], DXPool[0][DX_SURFACE][Source].Format.dwRGBBitCount);
		}
		DXDestroyObjects();
	}
}

// --- The frame timer ------------------------------------------------------------------

// The test's copy of WaitForNextFrame's second (LastSecond), to aim at the
// boundaries of the FPS count.
static double DXLastSecond;

static void DXWait(uint32 Clock)
{
	DXScript.Clock = Clock;
	DXScript.ClockStep = 0;
	DXRun("WaitForNextFrame", [&](int, TSurface *Object) { Object->WaitForNextFrame(); });
	if ((double)Clock - DXLastSecond > 1000.0) {
		DXLastSecond = Clock;
		DXCover("FPS counted");
	}
	double NextFrame = DXField<double>(DXObjects[0], 0x50);
	DXCover(NextFrame > (double)Clock ? "frame early" : "frame late");
}

static void DXSetNextFrame(double Value)
{
	DXField<double>(DXObjects[0], 0x50) = Value;
	DXField<double>(DXObjects[1], 0x50) = Value;
}

static void DXPhaseTimer()
{
	DXPhase = "timer";
	DXResetPools();
	DXDefaultScript(1);
	DXCreateObjects();
	DXLastSecond = 0.0;
	uint32 Clock = 60000;
	static const int Rates[] = {-5, 0, 1, 9, 10, 11, 25, 60, 100, 199, 200, 201, 0xffff, 100000};
	static const int Steps[] = {0, 1, 2, 5, 15, 16, 17, 33, 100, 499, 500, 999, 1000, 1001, 1500};
	static const double Deltas[] = {-1000.5, -5.0, -1.5, -1.0, -0.5, 0.0, 0.25, 0.5, 1.0, 3.75, 100.0, 2000.0};
	DXScript.Clock = Clock;
	DXRun("ResetFrameTimer", [&](int, TSurface *Object) { Object->ResetFrameTimer(); });
	for (int Op = 0; Op < 3000; Op++) {
		int Kind = Random() % 8;
		if (Kind == 0) {
			int Rate = Rates[Random() % (sizeof(Rates) / sizeof(Rates[0]))];
			DXRun("SetFramerateLimit", [&](int, TSurface *Object) { Object->SetFramerateLimit(Rate); });
			DXCover("SetFramerateLimit %s", Rate < 10 || Rate > 200 ? "none" : "limit");
		} else if (Kind == 1 && Random() % 4 == 0) {
			Clock += Steps[Random() % (sizeof(Steps) / sizeof(Steps[0]))];
			DXScript.Clock = Clock;
			DXRun("ResetFrameTimer", [&](int, TSurface *Object) { Object->ResetFrameTimer(); });
		} else if (Kind == 2) {
			// The second boundary: exactly 1000 ms, 1 ms less or more.
			double Boundary = DXLastSecond + 1000.0 + (double)((int)(Random() % 3) - 1);
			if (Boundary >= (double)Clock)
				Clock = (uint32)Boundary;
			DXWait(Clock);
		} else if (Kind == 3) {
			DXSetNextFrame((double)Clock + Deltas[Random() % (sizeof(Deltas) / sizeof(Deltas[0]))]);
			DXWait(Clock);
		} else if (Kind == 4) {
			double FPS[2];
			DXRun("GetFPS", [&](int Side, TSurface *Object) { FPS[Side] = Object->GetFPS(); });
			CHECK(memcmp(&FPS[0], &FPS[1], sizeof(double)) == 0, "DX GetFPS %g, original %g", FPS[0], FPS[1]);
		} else {
			Clock += Random() % 2 == 0 ? Steps[Random() % (sizeof(Steps) / sizeof(Steps[0]))] : Random() % 3000;
			DXWait(Clock);
		}
	}
	DXDestroyObjects();
}

// Last: timeGetTime wraps after 49.7 days; the frame counter's second stays
// at the time before the wrap afterwards.
static void DXPhaseClockWrap()
{
	DXPhase = "clockwrap";
	DXResetPools();
	DXDefaultScript(1);
	DXCreateObjects();
	static const int Rates[] = {10, 30, 200, 0};
	for (int Round = 0; Round < 8; Round++) {
		int Rate = Rates[Round % 4];
		DXRun("SetFramerateLimit", [&](int, TSurface *Object) { Object->SetFramerateLimit(Rate); });
		uint32 Clock = 0xffffffffu - (uint32)(Random() % 3000);
		DXScript.Clock = Clock;
		DXRun("ResetFrameTimer", [&](int, TSurface *Object) { Object->ResetFrameTimer(); Object->SetFramerateLimit(Rate); });
		for (int Op = 0; Op < 60; Op++) {
			int Kind = Random() % 4;
			if (Kind == 0)
				DXSetNextFrame((double)0xffffffffu - (double)(Random() % 3));
			else if (Kind == 1)
				DXSetNextFrame((double)Clock + (double)(Random() % 200));
			Clock += Random() % 2 == 0 ? Random() % 40 : Random() % 1500;
			DXWait(Clock);
			if (Clock < 5000)
				DXCover("clock wrapped");
		}
	}
	DXDestroyObjects();
}

// --- Fullscreen modes --------------------------------------------------------------------

static void DXMode(DDSURFACEDESC2 *Desc, DWORD Flags, DWORD Width, DWORD Height, DWORD FormatFlags, DWORD Bits,
	DWORD Refresh)
{
	memset(Desc, 0, sizeof(*Desc));
	Desc->dwSize = sizeof(DDSURFACEDESC2);
	Desc->dwFlags = Flags;
	Desc->dwWidth = Width;
	Desc->dwHeight = Height;
	Desc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	Desc->ddpfPixelFormat.dwFlags = FormatFlags;
	Desc->ddpfPixelFormat.dwRGBBitCount = Bits;
	Desc->dwRefreshRate = Refresh;
}

static void DXRandomModes(int Round)
{
	const DWORD Base = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	DXScript.EnumResult = DD_OK;
	if (Round == 0) {
		// The refresh rate's flag in dwFlags only: the binary tests the pixel
		// format's flags (Desc+0x4c), so the frequency is 0.
		DXScript.ModeCount = 3;
		DXMode(&DXScript.Modes[0], Base | DDSD_REFRESHRATE, 800, 600, DDPF_RGB, 16, 75);
		DXMode(&DXScript.Modes[1], Base, 1024, 768, DDPF_RGB | DDSD_REFRESHRATE, 32, 60);
		DXMode(&DXScript.Modes[2], Base | DDSD_REFRESHRATE, 640, 480, DDPF_RGB | DDSD_REFRESHRATE, 16, 85);
		return;
	}
	if (Round == 1) {
		DXScript.ModeCount = 0;
		return;
	}
	if (Round == 2) {
		DXScript.ModeCount = 2;
		DXMode(&DXScript.Modes[0], Base, 640, 480, DDPF_RGB, 16, 0);
		DXMode(&DXScript.Modes[1], Base, 800, 600, DDPF_RGB, 32, 0);
		DXScript.EnumResult = DDERR_GENERIC;
		return;
	}
	static const DWORD Widths[] = {320, 639, 640, 641, 800, 1024, 1280};
	static const DWORD Heights[] = {200, 479, 480, 481, 600, 768, 1024};
	static const DWORD Bits[] = {4, 8, 15, 16, 24, 32};
	static const DWORD Refreshes[] = {0, 60, 72, 75, 85};
	DXScript.ModeCount = Random() % 14;
	for (int ItemIndex = 0; ItemIndex < DXScript.ModeCount; ItemIndex++) {
		DWORD Flags = Random() % 4 != 0 ? Base : Random() & (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT);
		if (Random() % 2 == 0)
			Flags |= DDSD_REFRESHRATE;
		DWORD FormatFlags = Random() % 5 != 0 ? DDPF_RGB : Random() % 2 == 0 ? 0 : DDPF_PALETTEINDEXED8;
		if (Random() % 2 == 0)
			FormatFlags |= DDSD_REFRESHRATE;
		// Issue #55: the arguments right to left, as the oracle's.
		const DWORD Refresh = Refreshes[Random() % 5], Bit = Bits[Random() % 6], Height = Heights[Random() % 7],
					Width = Widths[Random() % 7];
		DXMode(&DXScript.Modes[ItemIndex], Flags, Width, Height, FormatFlags, Bit, Refresh);
	}
	if (Random() % 5 == 0)
		DXScript.EnumResult = Random() % 2 == 0 ? DDERR_GENERIC : DDERR_INVALIDPARAMS;
}

static void DXGetModes(int Round)
{
	DXRandomModes(Round);
	int Stale = Random() % 3 == 0 ? (int)(Random() % 5) : 0;
	DXField<int>(DXObjects[0], 0x38) = Stale;
	DXField<int>(DXObjects[1], 0x38) = Stale;
	TFullscreenMode *Modes[2] = {(TFullscreenMode *)(uintptr_t)0xdeadbeef, (TFullscreenMode *)(uintptr_t)0xdeadbeef};
	int Count[2];
	DXRun("GetFullscreenModes", [&](int Side, TSurface *Object) { Count[Side] = Object->GetFullscreenModes(&Modes[Side]); });
	CHECK(Count[0] == Count[1], "DX GetFullscreenModes count %d, original %d", Count[0], Count[1]);
	CHECK((Modes[0] == NULL) == (Modes[1] == NULL), "DX GetFullscreenModes list %p, original %p", Modes[0], Modes[1]);
	if (Modes[0] != NULL && Modes[1] != NULL) {
		int Entries = DXField<int>(DXObjects[0], 0x38);
		for (int ItemIndex = 0; ItemIndex < Entries; ItemIndex++)
			CHECK(memcmp(&Modes[0][ItemIndex], &Modes[1][ItemIndex], sizeof(TFullscreenMode)) == 0,
				"DX GetFullscreenModes mode %d: %dx%dx%d@%d, original %dx%dx%d@%d", ItemIndex, Modes[0][ItemIndex].Width,
				Modes[0][ItemIndex].Height, Modes[0][ItemIndex].Depth, Modes[0][ItemIndex].Frequency, Modes[1][ItemIndex].Width, Modes[1][ItemIndex].Height,
				Modes[1][ItemIndex].Depth, Modes[1][ItemIndex].Frequency);
	}
	if (Round == 0) {
		// The expected values, independent of the original.
		CHECK(Count[0] == 3 && Modes[0] != NULL, "DX GetFullscreenModes fixed list: count %d", Count[0]);
		if (Count[0] == 3 && Modes[0] != NULL) {
			CHECK(Modes[0][0].Width == 800 && Modes[0][0].Frequency == 0,
				"DX mode with DDSD_REFRESHRATE in dwFlags only: frequency %d, expected 0", Modes[0][0].Frequency);
			CHECK(Modes[0][1].Width == 1024 && Modes[0][1].Frequency == 60,
				"DX mode with the flag in the pixel format only: frequency %d, expected 60", Modes[0][1].Frequency);
			CHECK(Modes[0][2].Frequency == 85, "DX mode with both flags: frequency %d, expected 85",
				Modes[0][2].Frequency);
		}
	}
	DXCover("modes %s, %d accepted of %d", DXScript.EnumResult == DD_OK ? "enumerated" : "failed",
		Count[0], DXScript.ModeCount);
	for (int Side = 0; Side < 2; Side++)
		if (Modes[Side] != NULL && Modes[Side] != (TFullscreenMode *)(uintptr_t)0xdeadbeef)
			free(Modes[Side]);
}

// --- The display lifecycle -----------------------------------------------------------------

static TDXMask DXRandomMask()
{
	switch (Random() % 5) {
	case 0:
	case 1:
		return 0;
	case 2: {
		TDXMask Mask = 0;
		for (int Bit = 0; Bit < 40; Bit++)
			if (Random() % 6 == 0)
				Mask |= (TDXMask)1 << Bit;
		return Mask;
	}
	default:
		// Video memory runs out at the k-th request.
		return ~(TDXMask)0 << (Random() % 23);
	}
}

// The script for CreateSurfaces: one of the Direct3D steps fails (or none),
// and the video memory runs out somewhere. Nothing that throws.
static int DXRandomCreation()
{
	TDXScript &StateObject = DXScript;
	StateObject.VideoFail = DXRandomMask();
	StateObject.SystemFail = 0;
	StateObject.Fail3D = Random() % 6 == 0 ? 1 : 0;
	StateObject.QI3D = StateObject.Device = StateObject.Viewport = StateObject.AddViewport = StateObject.SetViewport = StateObject.DeviceCaps = StateObject.CurrentViewport = D3D_OK;
	StateObject.Begin = D3D_OK;
	StateObject.RenderStateFail = 0;
	StateObject.SrcBlend = D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE;
	StateObject.DestBlend = D3DPBLENDCAPS_SRCCOLOR;
	StateObject.ShadeMode = 1;
	StateObject.TempBits = 0;
	StateObject.TempLockFailures = 0;
	StateObject.TempLockError = DDERR_SURFACEBUSY;
	int Stage = Random() % 2 == 0 ? 0 : 1 + Random() % 19;
	switch (Stage) {
	case 1: StateObject.QI3D = E_NOINTERFACE; break;
	case 2: StateObject.Fail3D = 2; break;
	case 3: StateObject.Device = DDERR_GENERIC; break;
	case 4: StateObject.Viewport = DDERR_GENERIC; break;
	case 5: StateObject.AddViewport = DDERR_GENERIC; break;
	case 6: StateObject.SetViewport = DDERR_INVALIDPARAMS; break;
	case 7: StateObject.DeviceCaps = DDERR_GENERIC; break;
	case 8: StateObject.SrcBlend = D3DPBLENDCAPS_ONE; break;
	case 9: StateObject.DestBlend = D3DPBLENDCAPS_ZERO; break;
	case 10: StateObject.CurrentViewport = DDERR_GENERIC; break;
	case 11: case 12: case 13: StateObject.RenderStateFail = Stage - 10; break;
	case 14: StateObject.ShadeMode = 0; break;
	case 15: StateObject.TempLockFailures = 1; StateObject.TempLockError = E_FAIL; break;
	case 16: StateObject.TempLockFailures = 2; StateObject.TempLockError = DDERR_SURFACEBUSY; break;
	case 17: StateObject.TempBits = 8; break;
	case 18: StateObject.Begin = D3DERR_SCENE_IN_SCENE; break;
	case 19: StateObject.Fail3D = 1; break;
	}
	DXFormat(&StateObject.Format, 1 + Random() % 4);
	StateObject.Metrics[0] = 1024 + 256 * (Random() % 2);
	StateObject.Metrics[1] = 768 + 256 * (Random() % 2);
	StateObject.Client = {0, 0, (LONG)(320 + Random() % 800), (LONG)(200 + Random() % 600)};
	StateObject.Origin = {(LONG)(Random() % 200) - 50, (LONG)(Random() % 200)};
	StateObject.CoopTest = Random() % 6 == 0 ? DDERR_EXCLUSIVEMODEALREADYSET : Random() % 8 == 0 ? DDERR_NOEXCLUSIVEMODE : DD_OK;
	StateObject.LostCall = Random() % 5 == 0 ? 1 + Random() % 22 : 0;
	StateObject.OddLostCall = Random() % 5 == 0 ? 1 + Random() % 22 : 0;
	StateObject.Clock += Random() % 100;
	return Stage;
}

// Recorded only when the operation created the surfaces (GetDisplayMode is
// CreateSurfaces' first call).
static void DXCoverCreation(const char *Name, int Stage)
{
	TSurfaceDX *Ours = DXOurs();
	if (DXScript.LostCall != 0 && DXCount[0].IsLost >= DXScript.LostCall)
		DXCover("%s: lost surface, created again", Name);
	if (DXLog[0].find("GetDisplayMode") == std::string::npos)
		return;
	DXCover("created: stage %2d, InitStep %2d, Direct3D %s", Stage, InitStep, Ours->Direct3D != NULL ? "kept" : "none");
	int System = 0;
	for (int ItemIndex = 0; ItemIndex < NUMBER_OF_SURFACES; ItemIndex++)
		System += Ours->GetSurfaceMemory(ItemIndex) == 2;
	DXCover("created: %2d cache surfaces, %s", Ours->CacheSurfaces, System != 0 ? "system memory fallback" : "all video");
}

static const TFullscreenMode DXResolutions[] = {
	{640, 480, 16, 60}, {800, 600, 16, 75}, {800, 600, 32, 60}, {1024, 768, 32, 85},
};

static void DXPhaseLifecycle()
{
	DXPhase = "lifecycle";
	for (int Round = 0; Round < 70; Round++) {
		DXResetPools();
		DXDefaultScript(1 + Round % 4);
		DXSetGlobal(&CacheSurfaceWidth, 0x5c2568, 256);
		DXSetGlobal(&CacheSurfaceHeight, 0x5c256c, 256);
		DXCreateObjects();
		bool Initialised = false;
		for (int Op = 0; Op < 26; Op++) {
			TSurfaceDX *Ours = DXOurs();
			bool HasDD = Initialised && Ours->DirectDraw != NULL;
			bool Created = HasDD && Ours->Surface[PRIMARY_SURFACE] != NULL;
			int Choice = !HasDD ? 0 : Op < 2 ? 1 : 1 + Random() % 22;
			TFullscreenMode Resolution = DXResolutions[Random() % 4];
			Resolution.Frequency = 50 + Random() % 40;
			if (Ours->ViewMode == FULLSCREEN_MODE && Random() % 3 == 0) {
				Resolution.Width = Ours->Width;
				Resolution.Height = Ours->Height;
				Resolution.Depth = Ours->Depth;
			}
			HWND Window = Random() % 3 == 0 ? (HWND)0x5678 : (HWND)0x1234;
			int Mode = 1 + Random() % 2;
			switch (Choice) {
			case 0: {
				// InitSurfaces: the stretch capabilities, the configured frame rate.
				DXScript.CapsResult = Random() % 5 == 0 ? DDERR_GENERIC : DD_OK;
				DXScript.FXCaps = 0;
				static const DWORD Bits[] = {DDFXCAPS_BLTSHRINKX, DDFXCAPS_BLTSTRETCHX, DDFXCAPS_BLTSHRINKY,
					DDFXCAPS_BLTSTRETCHY};
				for (int ItemIndex = 0; ItemIndex < 4; ItemIndex++)
					if (Random() % 5 != 0)
						DXScript.FXCaps |= Bits[ItemIndex];
				if (Random() % 3 == 0)
					DXScript.FXCaps |= DDFXCAPS_BLTARITHSTRETCHY | DDFXCAPS_BLTMIRRORLEFTRIGHT;
				static const int Rates[] = {0, 5, 10, 30, 50, 200, 201, 1000};
				DXScript.Framerate = Rates[Random() % 8];
				DXScript.Clock += 1000 + Random() % 1000;
				DXRun("InitSurfaces", [&](int, TSurface *Object) { Object->InitSurfaces(); });
				DXCover("InitSurfaces: CanStretch %d, caps %s", Ours->CanStretch ? 1 : 0,
					DXScript.CapsResult == DD_OK ? "ok" : "failed");
				Initialised = true;
				break;
			}
			case 1: case 2: case 3: case 4: case 5: {
				// Init, mostly without Force; ViewMode 0 does nothing but restore.
				int ViewMode = Random() % 10 == 0 ? 0 : Mode;
				bool Force = ViewMode != 0 && Random() % 5 == 0;
				int Stage = DXRandomCreation();
				if (ViewMode == 0)
					DXScript.LostCall = 0;
				int Before = Ours->ViewMode;
				bool Result[2];
				DXRun("Init", [&](int Side, TSurface *Object) {
					Result[Side] = Object->Init(&Window, ViewMode, &Resolution, Force);
				});
				CHECK(Result[0] == Result[1], "DX Init result");
				DXCover("Init %d->%d%s%s%s", Before, ViewMode, Force ? " force" : "",
					DXScript.CoopTest == DDERR_EXCLUSIVEMODEALREADYSET ? " unavailable" : "",
					DXScript.LostCall != 0 ? " lost?" : "");
				DXCoverCreation("Init", Stage);
				break;
			}
			case 6: case 7: {
				// FinishFrame: the primary still drawing, busy or failing.
				DXScript.StillDrawing = Random() % 4;
				static const HRESULT Errors[] = {DD_OK, DDERR_SURFACELOST, DDERR_GENERIC};
				DXScript.FlipError = Errors[Random() % 3];
				DXScript.Clock += Random() % 100;
				int Failures = Random() % 3;
				HRESULT Error = Random() % 2 == 0 ? DDERR_SURFACEBUSY : DDERR_GENERIC;
				if (Created)
					for (int Side = 0; Side < 2; Side++) {
						TDXCom *Primary = DXSurfaceOf(Side, PRIMARY_SURFACE);
						Primary->BlitFailures = Failures;
						Primary->BlitError = Error;
					}
				int Blits = Random() % 50;
				DXSetGlobal(&BlitCount, 0x71d0d0, Blits);
				DXSetGlobal(&BlitPixels, 0x71d0d4, Blits * 37);
				if (!Created && Ours->ViewMode != 0)
					break;
				DXRun("FinishFrame", [&](int, TSurface *Object) { Object->FinishFrame(); });
				DXCover("FinishFrame mode %d", Ours->ViewMode);
				if (Created)
					for (int Side = 0; Side < 2; Side++)
						DXSurfaceOf(Side, PRIMARY_SURFACE)->BlitFailures = 0;
				break;
			}
			case 8:
				DXGetModes(3 + Random() % 1000);
				break;
			case 9: {
				int Stage = DXRandomCreation();
				DXRun("RestoreSurfaces", [&](int, TSurface *Object) { Object->RestoreSurfaces(Mode, &Resolution); });
				DXCover("RestoreSurfaces %s%s", DXScript.CoopTest == DDERR_EXCLUSIVEMODEALREADYSET ? "unavailable"
					: "available", DXScript.LostCall != 0 && Created ? " lost?" : "");
				DXCoverCreation("RestoreSurfaces", Stage);
				break;
			}
			case 10:
				DXRun("ExitDisplay", [&](int, TSurface *Object) { Object->ExitDisplay(); });
				break;
			case 11:
				if (Random() % 3 != 0)
					break;
				DXRun("ExitSurfaces", [&](int, TSurface *Object) { Object->ExitSurfaces(); });
				if (Random() % 2 == 0)
					DXRun("ExitSurfaces again", [&](int, TSurface *Object) { Object->ExitSurfaces(); });
				Initialised = false;
				break;
			case 12: case 13: {
				if (Ours->ViewMode != 0) {
					DXRun("ExitDisplay", [&](int, TSurface *Object) { Object->ExitDisplay(); });
				}
				int Stage = DXRandomCreation();
				// InitSurfaces cleared the window; ResetDisplay and InitDisplay need one.
				DXField<HWND>(DXObjects[0], 8) = Window;
				DXField<HWND>(DXObjects[1], 8) = Window;
				if (Choice == 12) {
					DXRun("ResetDisplay", [&](int, TSurface *Object) { Object->ResetDisplay(Mode, &Resolution); });
					DXCoverCreation("ResetDisplay", Stage);
				} else {
					TFullscreenMode *GMode = Mode == WINDOWED_MODE && Random() % 2 == 0 ? NULL : &Resolution;
					DXRun("InitDisplay", [&](int, TSurface *Object) { Object->InitDisplay(Mode, GMode); });
					DXRun("CreateSurfaces", [&](int, TSurface *Object) { Object->CreateSurfaces(); });
					DXCoverCreation("CreateSurfaces", Stage);
				}
				break;
			}
			case 14:
				DXRun("UpdateWindowRect", [&](int, TSurface *Object) { Object->UpdateWindowRect(); });
				DXCover("UpdateWindowRect mode %d", Ours->ViewMode);
				break;
			case 15:
				if (!Created)
					break;
				DXRun("AttachWindow", [&](int, TSurface *Object) { Object->AttachWindow(); });
				break;
			case 16: {
				// CreateOffscreenSurface and Create3DSurface on their own.
				bool ThreeD = Random() % 2 == 0;
				bool VideoOnly = Random() % 2 == 0;
				int Width = 1 + Random() % 700, Height = 1 + Random() % 500;
				DXScript.VideoFail = Random() % 2;
				DXScript.SystemFail = Random() % 2;
				DXScript.Fail3D = Random() % 3;
				int Number = Random() % 21;
				DXSetGlobal(&NumberOfSurfaces, 0x71d0f0, Number);
				IDirectDrawSurface4 *Result[2] = {(IDirectDrawSurface4 *)(uintptr_t)0x1111,
					(IDirectDrawSurface4 *)(uintptr_t)0x1111};
				bool Made[2];
				DXRun(ThreeD ? "Create3DSurface" : "CreateOffscreenSurface", [&](int Side, TSurface *) {
					Made[Side] = DXCreateOffscreen(Side, ThreeD, &Result[Side], Width, Height, VideoOnly);
				});
				CHECK(Made[0] == Made[1] && DXIdOf(Result[0], 0) == DXIdOf(Result[1], 1),
					"DX %s: %d %s, original %d %s", ThreeD ? "Create3DSurface" : "CreateOffscreenSurface", Made[0],
					DXName(Result[0]).c_str(), Made[1], DXName(Result[1]).c_str());
				DXCover("%s %s, video only %d", ThreeD ? "Create3DSurface" : "CreateOffscreenSurface",
					!Made[0] ? "failed" : Ours->GetSurfaceMemory(Number) == 1 ? "video" : "system", VideoOnly ? 1 : 0);
				for (int Side = 0; Side < 2; Side++) {
					int Found;
					TDXCom *ComObject = DXFind(Result[Side], &Found);
					if (ComObject != NULL)
						ComObject->Refs = 0;
				}
				break;
			}
			case 17: {
				// Drawing on the created surfaces.
				if (!Created)
					break;
				int Sprite = Ours->CacheSurfaces > 0 ? SPRITE_SURFACE + Random() % Ours->CacheSurfaces : CACHE_SURFACE;
				TRGBColor Key = DXRandomColor(), Color = DXRandomColor();
				TRect Rect = DXRandomRect(-4, 50, 30);
				int Width = 1 + Random() % 30, Height = 1 + Random() % 30;
				int ToWidth = 1 + Random() % 64, ToHeight = 1 + Random() % 64;
				TRect Clip = DXRandomRect(0, 40, 50);
				int Which = Random() % 4;
				DXRun("draw", [&](int, TSurface *Object) {
					switch (Which) {
					case 0: Object->FillRectangle(SCREEN_SURFACE, Rect, Color); break;
					case 1:
						Object->SetColorKey(Sprite, Key);
						Object->Blit(SCREEN_SURFACE, Rect.x, Rect.y, Sprite, TRect(2, 3, Width, Height), Key);
						break;
					case 2:
						Object->StretchBlit(SCREEN_SURFACE, TRect(Rect.x, Rect.y, ToWidth, ToHeight), Sprite,
							TRect(1, 1, Width, Height), Key, Color, Clip);
						break;
					case 3:
						Object->StretchBlitUnclipped(AUTOMAP_SURFACE, TRect(Rect.x, Rect.y, ToWidth, ToHeight), Sprite,
							TRect(1, 1, Width, Height), Key, Color);
						break;
					}
				});
				break;
			}
			case 18: {
				if (!Created)
					break;
				TRect Rects[3];
				TRGBColor Colors[12];
				for (int ItemIndex = 0; ItemIndex < 3; ItemIndex++)
					Rects[ItemIndex] = DXRandomRect(0, 40, 30);
				for (int ItemIndex = 0; ItemIndex < 12; ItemIndex++)
					Colors[ItemIndex] = DXRandomColor();
				int Count = 1 + Random() % 3;
				DXScript.Begin = Random() % 5 == 0 ? D3DERR_SCENE_IN_SCENE : D3D_OK;
				DXScript.ShadeMode = Random() % 2;
				bool Shading[2];
				DXRun("ShadeRectangles+TestShading", [&](int Side, TSurface *Object) {
					Object->ShadeRectangles(TEMP_SURFACE, Count, Rects, Colors);
					Shading[Side] = DXTestShading(Side, TEMP_SURFACE);
				});
				CHECK(Shading[0] == Shading[1], "DX TestShading on the created surfaces");
				DXCover("live shading %s, %s", Ours->Direct3D != NULL ? "Direct3D" : "none", Shading[0] ? "true" : "false");
				break;
			}
			case 19: {
				int Index = Random() % NUMBER_OF_SURFACES;
				TRect Rect[2];
				uint32 Values[2][6];
				double FPS[2];
				DXRun("accessors", [&](int Side, TSurface *Object) {
					Object->GetSurfaceRect(Index, &Rect[Side]);
					Values[Side][0] = Object->CanShadeRectangles();
					Values[Side][1] = Object->IsUnavailable();
					Values[Side][2] = Object->GetCacheSurfaces();
					Values[Side][3] = Object->SurfaceRecreated(Index);
					Values[Side][4] = Object->GetSurfaceMemory(Index);
					Values[Side][5] = Object->TestEngine();
					FPS[Side] = Object->GetFPS();
				});
				CHECK(memcmp(&Rect[0], &Rect[1], sizeof(TRect)) == 0 && memcmp(Values[0], Values[1], sizeof(Values[0])) == 0
					&& memcmp(&FPS[0], &FPS[1], sizeof(double)) == 0, "DX accessors after the lifecycle");
				DXCover("accessors: unavailable %u, recreated %u", Values[0][1], Values[0][3]);
				break;
			}
			case 20: {
				DXScript.Clock += Random() % 40;
				DXRun("WaitForNextFrame", [&](int, TSurface *Object) { Object->WaitForNextFrame(); });
				break;
			}
			case 21: {
				// GetPixelColor on a created surface, any format.
				if (!Created)
					break;
				int Index = Random() % (SPRITE_SURFACE + Ours->CacheSurfaces);
				TRGBColor Color = DXRandomColor();
				uint32 Pixel[2];
				DXRun("GetPixelColor", [&](int Side, TSurface *Object) {
					Pixel[Side] = DXGetPixelColor(Side, (IDirectDrawSurface4 *)DXField<void *>(Object, 0x88 + Index * 4),
						Color);
				});
				CHECK(Pixel[0] == Pixel[1], "DX GetPixelColor %x, original %x", Pixel[0], Pixel[1]);
				break;
			}
			case 22:
				DXGetModes(3 + Random() % 1000);
				break;
			}
		}
		if (Round % 2 == 0 && Initialised)
			DXRun("ExitSurfaces", [&](int, TSurface *Object) { Object->ExitSurfaces(); });
		DXDestroyObjects();
	}
}

static void DXPrintCoverage(FILE *File)
{
	for (auto &Entry : DXCoverage)
		fprintf(File, "  %6d %s\n", Entry.second, Entry.first.c_str());
}

static void DXEndPhase()
{
	if (DXTrace) {
		fprintf(stderr, "SurfaceDX phase %s done: %d operations so far, %d checks, %d failures; coverage:\n", DXPhase,
			DXOperation, Checks, Failures);
		DXPrintCoverage(stderr);
		fflush(stderr);
	}
}

// --- Error paths (issue #34) ---------------------------------------------------------------

// DXRun with each side in OurThrow/ImageThrow: what they threw, then the
// usual comparison of logs and objects.
template <class Body> static void DXThrown(TErrorRound &R, const char *Name, Body Run)
{
	TThrown Thrown[2];
	DXRun(Name, [&](int Side, TSurface *Object) {
		if (Side == DX_OURS)
			Thrown[Side] = OurThrow([&] { Run(Side, Object); });
		else
			Thrown[Side] = ImageThrow([&] { Run(Side, Object); });
	});
	R.Compare(Name, Thrown[DX_OURS], Thrown[DX_ORIGINAL]);
}

// PROGRAM_ERROR 7-18: one creation call fails, the desktop has too few bits,
// all offscreen memory is gone; the assertions of AttachWindow, the blits,
// InitDisplay, CreateSurfaces and GetSurfaceRect.
static void TestSurfaceDXErrors()
{
	TErrorRound R("SurfaceDX");
	DXPhase = "errors";
	for (int Round = 0; Round < 160; Round++) {
		DXResetPools();
		DXDefaultScript(1 + Round % 4);
		DXSetGlobal(&CacheSurfaceWidth, 0x5c2568, 256);
		DXSetGlobal(&CacheSurfaceHeight, 0x5c256c, 256);
		DXCreateObjects();
		int Case = Round % 4;
		DXScript.FailCall = Case == 0 ? 1 + (int)(Random() % 2) : 0; // DirectDrawCreate, QueryInterface
		DXThrown(R, "InitSurfaces", [&](int, TSurface *Object) { Object->InitSurfaces(); });
		if (Case != 0) {
			// Case 1: each counted call of Init in turn fails; case 2: the
			// desktop depth, or the n-th offscreen request in both memories.
			int Sweep = Round / 4;
			DXScript.FailCall = Case == 1 ? 1 + Sweep % 20 : 0;
			if (Case == 2) {
				if (Sweep % 3 == 0)
					DXScript.Format.dwRGBBitCount = 1 + Random() % 15;
				else {
					DXScript.VideoFail = (TDXMask)1 << (Sweep % 26);
					DXScript.SystemFail = ~(TDXMask)0;
					if (Sweep % 3 == 1) { // no Direct3D: TEMP_SURFACE is the first offscreen request
						DXScript.QI3D = E_NOINTERFACE;
						DXScript.VideoFail = (TDXMask)1 << ((Sweep / 3) % 4);
					}
				}
			}
			TFullscreenMode Resolution = DXResolutions[Random() % 4];
			Resolution.Frequency = 60;
			HWND Window = (HWND)0x1234;
			int Mode = Case == 1 ? 1 + (Sweep / 20) % 2 : 1 + (int)(Random() % 2);
			DXThrown(R, "Init", [&](int, TSurface *Object) { Object->Init(&Window, Mode, &Resolution, true); });
			DXScript.FailCall = 0;
			DXScript.VideoFail = DXScript.SystemFail = 0;
			DXDefaultScript(1 + Round % 4);
			TSurfaceDX *Ours = DXOurs();
			if (Case == 3 && Ours->DirectDraw != NULL && Ours->Surface[PRIMARY_SURFACE] != NULL) {
				// Issue #55: the arguments right to left, as the oracle's.
				const int SomeHeight = 1 + (int)(Random() % 30), SomeWidth = 1 + (int)(Random() % 30);
				TRect Some(1, 1, SomeWidth, SomeHeight);
				const int EmptyHeight = 1 + (int)(Random() % 5), EmptyWidth = (int)(Random() % 2) - 1;
				TRect Empty(1, 1, EmptyWidth, EmptyHeight);
				if (Random() % 2 == 0)
					std::swap(Empty.width, Empty.height);
				TRGBColor Key(1, 2, 3);
				int Good = (int)(Random() % NUMBER_OF_SURFACES);
				int Bad = Random() % 2 == 0 ? -1 - (int)(Random() % 5) : NUMBER_OF_SURFACES + (int)(Random() % 5);
				DXThrown(R, "Blit", [&](int, TSurface *Object) { Object->Blit(SCREEN_SURFACE, 1, 1, Good, Empty, Key); });
				DXThrown(R, "BlitRect", [&](int, TSurface *Object) { Object->BlitRect(SCREEN_SURFACE, 1, 1, Good, Empty); });
				DXThrown(R, "GetSurfaceRect", [&](int, TSurface *Object) { TRect Out; Object->GetSurfaceRect(Bad, &Out); });
				for (int k = 0; k < 6; k++) {
					int From = k == 0 ? TEMP_SURFACE : k == 1 ? STRETCH_SURFACE : Good;
					int To = k == 2 ? TEMP_SURFACE : k == 3 ? STRETCH_SURFACE : SCREEN_SURFACE;
					if (From == TEMP_SURFACE || From == STRETCH_SURFACE)
						To = SCREEN_SURFACE;
					if (k >= 4 && (Good == TEMP_SURFACE || Good == STRETCH_SURFACE))
						From = SPRITE_SURFACE;
					// Issue #55: the arguments right to left, as the oracle's.
					const int HugeHeight = 1 + (int)(Random() % 20), HugeWidth = 2000 + (int)(Random() % 3000);
					TRect Huge(0, 0, HugeWidth, HugeHeight);
					if (Random() % 2 == 0)
						std::swap(Huge.width, Huge.height);
					TRect FromRect = k == 4 ? Huge : Some;
					TRect ToRect = k == 5 ? Huge : TRect(FromRect.x, FromRect.y, FromRect.width + 1, FromRect.height);
					DXThrown(R, "StretchBlitUnclipped", [&](int, TSurface *Object) {
						Object->StretchBlitUnclipped(To, ToRect, From, FromRect, Key, Key);
					});
					DXThrown(R, "StretchBlit", [&](int, TSurface *Object) {
						Object->StretchBlit(To, ToRect, From, FromRect, Key, Key, TRect(0, 0, 800, 600));
					});
				}
				// The state checks: members set on both objects for the call
				// (a stand-in pointer where a check needs one that is not set).
				static const int Order[6] = {PRIMARY_SURFACE, SCREEN_SURFACE, TEMP_SURFACE, CACHE_SURFACE,
					AUTOMAP_SURFACE, SPRITE_SURFACE};
				for (int Check = 0; Check < 16; Check++) {
					static uint8 Saved[2][sizeof(TSurfaceDX)]; // raw copies: no constructor or destructor
					for (int Side = 0; Side < 2; Side++) {
						TSurfaceDX *D = (TSurfaceDX *)DXObjects[Side];
						memcpy((void *)&Saved[Side], (void *)D, sizeof(TSurfaceDX));
						void *StandIn = D->DirectDraw;
						if (Check == 0 || Check == 4 || Check == 7)
							D->DirectDraw = NULL;
						if (Check == 1 || Check == 5)
							D->hWindow = NULL;
						if (Check == 6)
							D->Surface[PRIMARY_SURFACE] = NULL;
						if (Check == 8 && D->Direct3D == NULL)
							D->Direct3D = (IDirect3D3 *)StandIn;
						if (Check >= 9) {
							D->Direct3D = NULL;
							for (int k = 0; k < 6; k++) {
								if (k < Check - 9)
									D->Surface[Order[k]] = NULL;
								else if (k == Check - 9 && D->Surface[Order[k]] == NULL)
									D->Surface[Order[k]] = (IDirectDrawSurface4 *)StandIn;
							}
							if (Check == 15)
								D->ViewMode = 7;
						}
					}
					if (Check <= 3) {
						int VMode = Check == 2 ? 7 : Check == 3 ? FULLSCREEN_MODE : WINDOWED_MODE;
						TFullscreenMode *GMode = Check == 3 ? NULL : &Resolution;
						DXThrown(R, "InitDisplay", [&](int, TSurface *Object) { Object->InitDisplay(VMode, GMode); });
					} else if (Check <= 6) {
						DXThrown(R, "AttachWindow", [&](int, TSurface *Object) { Object->AttachWindow(); });
					} else {
						DXThrown(R, "CreateSurfaces", [&](int, TSurface *Object) { Object->CreateSurfaces(); });
					}
					for (int Side = 0; Side < 2; Side++)
						memcpy(DXObjects[Side], (void *)&Saved[Side], sizeof(TSurfaceDX));
				}
				// AttachWindow's clipper failing (PROGRAM_ERROR 9).
				DXScript.FailCall = 1;
				DXThrown(R, "AttachWindow clipper", [&](int, TSurface *Object) { Object->AttachWindow(); });
				DXScript.FailCall = 0;
			}
		}
		DXThrown(R, "ExitSurfaces", [&](int, TSurface *Object) { Object->ExitSurfaces(); });
		DXDestroyObjects();
	}
	DXScript.FailCall = 0;
	R.Finish();
}

void TestSurfaceDX()
{
	int Before = Checks, BeforeFailures = Failures;
	PatchImage();
	if (getenv("DX_LOGS") != NULL)
		DXLogs = fopen(getenv("DX_LOGS"), "w");
	DXTrace = getenv("DX_TRACE") != NULL;
	DXInitVtables();
	DXPatchAll();
	for (auto &GlobalValue : DXGlobals)
		Global<int>(GlobalValue.Image) = *GlobalValue.Ours;
	DXPhaseFactory();
	DXEndPhase();
	DXPhaseDraw();
	DXEndPhase();
	DXPhaseTimer();
	DXEndPhase();
	DXPhase = "modes";
	DXResetPools();
	DXDefaultScript(1);
	DXCreateObjects();
	DXRun("InitSurfaces", [&](int, TSurface *Object) { Object->InitSurfaces(); });
	for (int Round = 0; Round < 300; Round++)
		DXGetModes(Round);
	DXRun("ExitSurfaces", [&](int, TSurface *Object) { Object->ExitSurfaces(); });
	DXDestroyObjects();
	DXEndPhase();
	DXPhaseLifecycle();
	DXEndPhase();
	DXPhaseClockWrap();
	DXEndPhase();
	// The error round runs with the fakes; its operations and coverage
	// points are not counted in the lines below, its checks apart.
	int ErrorChecks = Checks, Operations = DXOperation;
	auto Coverage = DXCoverage;
	TestSurfaceDXErrors();
	ErrorChecks = Checks - ErrorChecks;
	DXOperation = Operations;
	DXCoverage = Coverage;
	DXRestoreAll();
	if (DXLogs != NULL) {
		fclose(DXLogs);
		DXLogs = NULL;
	}
	int Kinds = 0;
	for (auto &Entry : DXCoverage)
		if (Entry.first.compare(0, 3, "op ") == 0)
			Kinds++;
	printf("SurfaceDX: %d operations of %d kinds, %d coverage points\n", DXOperation, Kinds, (int)DXCoverage.size());
	printf("SurfaceDX: %d checks, %d failures\n", Checks - Before - ErrorChecks, Failures - BeforeFailures);
}
