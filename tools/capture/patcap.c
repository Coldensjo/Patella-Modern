/* patcap.dll: frame capture and virtual time for the rendering comparison (issue #20).

Loaded into Tibia.exe as an ordinary dependency: the run folders' copies
of the client get one more import descriptor (patcap.dll, PatcapLoaded; tools/compare_rendering.py
add_import), so the loader loads it and runs DllMain before the client's entry point (and its
static initialisers). DllMain patches the main module's import address table, so the DLL sees
exactly the calls the client makes:

- timeGetTime: a virtual clock. Its first call (sysinc.cpp's StartTime initialiser) returns
  BASE_TIME; afterwards the clock stands still and moves only by ADVANCE commands. Wall-clock
  time (GetSystemTimeAsFileTime, and GetSystemTimePreciseAsFileTime fetched by GetProcAddress)
  is FILETIME_BASE plus the same virtual time. srand(timeGetTime()) gets BASE_TIME too.
- SetTimer/KillTimer: the main frame's 100 ms timer (id 1, MainFrm.cpp OnCreate) is not
  created; its ticks (WM_TIMER, TimerEvent, ActionNumber) are sent by ADVANCE commands.
  Other timers pass through.
- GetCursorPos: a fixed point outside the client window (the client polls it in OnIdle).
- PeekMessageA: the main thread's idle loop calls it between messages; commands from the
  controller are carried out there (never inside a frame).
- The frame: just before the client shows a frame, the composed picture is read back:
  DirectX 5 (SurfaceDX.cpp): IDirectDrawSurface4::Blt into the primary surface (windowed: the
  source surface and rectangle) or ::Flip (fullscreen: the back buffer);
  DirectX 9 (surfaceDX9.cpp): IDirect3DDevice9::Present (the back buffer through
  GetRenderTargetData, the source rectangle);
  OpenGL (SurfaceGL.cpp): SwapBuffers (glReadPixels of the back buffer, the client area).
  The vtable entries are patched when the client creates the objects (DirectDrawCreate,
  IDirectDraw4::CreateSurface, GetProcAddress "Direct3DCreate9", IDirect3D9::CreateDevice).
- Direct3D 9's course (Patella-Modern issue #98): the calls of Direct3DCreate9, CreateDevice (its
  result), TestCooperativeLevel (its last result) and Present (its first and last time, its last
  result, the failures) are counted in the shared memory, and the rare events (the load, the device,
  the first frame, a change of a result, every capture) are written to patcap.log in the client's
  folder with the milliseconds since the DLL was loaded: the controller names the cause when a capture
  gets no frame (no device, a lost device, no Present at all).

The controller (tools/compare_rendering.py) talks to it through a named file mapping
"Local\PatellaCapture-<pid>" (layout: struct Shared). Nothing here depends on which client it
is in. It has the client's bitness (cmake/Capture.cmake): the patcap target of the capture-x86 preset,
32-bit, for Tibia.exe (tools/compare_rendering.py and the reference recordings' record step,
tools/reference_record.py); until Patella-Modern issue #94 also for the 32-bit oracle's Patella.exe, which left the
fork with the oracle.

Tibia.exe's only (Patella-Modern issue #92): the tools read the x64 client through its
test mode (PatellaTest.exe --test-control: the frame dump, the probe; tools/testmode_client.py), never
by hooking it, so this file no longer builds for x64 (issue #18 had built a 64-bit one). Its Vulkan
exports (PatcapVulkanDevice, PatcapVulkanFrame, PatcapVulkanMask, PatcapVulkanPixels, issue #19) served
Patella.exe's lookup of the DLL, which issue #97 removed; they left with issue #98, and with them the
shared memory's Vulkan fields (version 4).

Profile windows (Patella-Modern issue #134, tools/profiling.py, version 5): the commands CMD_PROFILE_START and
CMD_PROFILE_STOP open and close a window between two frames; per Direct3D 9 Present the performance counter and
Present's own duration (a time window), and with Arg0 PROFILE_ALLOCS the client's own heap calls since the last
Present, counted at the main module's imports of HeapAlloc, HeapReAlloc and HeapFree, as PatellaTest.exe counts
its own (src/testmode/ProfileWin32.cpp, the file's form too). The heap hooks are a call through while no window
is open; what the system DLLs and drivers allocate (their own imports) is in no count. */

#if defined(_WIN64)
#error "patcap.dll is Tibia.exe's only (Patella-Modern issues #92, #94): the x64 client is read through its test mode"
#endif

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define CINTERFACE
#define INITGUID
#include <windows.h>
#include <mmsystem.h>
#include <ddraw.h>
#include <d3d9.h>
#include <GL/gl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define BASE_TIME 100000u           /* the virtual timeGetTime of the first call */
#define FILETIME_BASE 0x01dd6ac1afb3a000ull  /* 2026-11-02 12:00:00 UTC */
#define CURSOR_X 1020
#define CURSOR_Y 760
#define MAIN_TIMER_ID 1
#define MAIN_TIMER_INTERVAL 100

#define CMD_ADVANCE 1               /* arg0 milliseconds, arg1 ticks */
#define CMD_CAPTURE 2               /* arg0 frames to let pass first; path */
#define CMD_REALTIME 3              /* arg0 1: the clock and the timer run in real time, 0: virtual again */
#define CMD_PROFILE_START 4         /* arg0 PROFILE_*; path: the file CMD_PROFILE_STOP writes (issue #134) */
#define CMD_PROFILE_STOP 5

#define PROFILE_ALLOCS 1

#define API_DDRAW_BLT 1
#define API_DDRAW_FLIP 2
#define API_D3D9 3
#define API_OPENGL 4

#pragma pack(push, 4)
typedef struct {
	LONG Magic;            /* 0 'PCAP' */
	LONG Version;          /* 4 */
	volatile LONG CmdSeq;  /* 8: the controller increments it after filling the command */
	volatile LONG DoneSeq; /* 12: set to CmdSeq when the command is done */
	LONG Cmd;              /* 16 */
	LONG Arg0;             /* 20 */
	LONG Arg1;             /* 24 */
	LONG Result;           /* 28: 0 done, else an error number (Message says more) */
	LONG Frames;           /* 32: frames shown so far */
	LONG Api;              /* 36: the last API that showed a frame (API_*) */
	LONG Now;              /* 40: virtual timeGetTime - BASE_TIME */
	LONG Ticks;            /* 44: WM_TIMER ticks sent */
	LONG TimerWindow;      /* 48 */
	LONG TimerActive;      /* 52: the client's main timer is set (not killed) */
	LONG Width;            /* 56: the last capture */
	LONG Height;           /* 60 */
	LONG SourceBits;       /* 64: bits per pixel of the captured surface */
	LONG Hooks;            /* 68: which IAT entries were found (bit per hook) */
	char Path[520];        /* 72 */
	char Message[256];     /* 592 */
	LONG ApiFrames[5];     /* 848: frames per API */
	LONG Realtime;         /* 868 */
	LONG FirstCall;        /* 872: the virtual clock has started */
	/* Version 4 (Patella-Modern issue #98; versions 2 and 3 had the Vulkan engine's fields here):
	   Direct3D 9's course. Times are milliseconds since the DLL was loaded (GetTickCount), -1 never. */
	LONG Direct3DCreates;  /* 876: calls of Direct3DCreate9 (fetched through GetProcAddress) */
	LONG DeviceCreates;    /* 880: calls of IDirect3D9::CreateDevice (counted when it is called) */
	LONG DeviceResult;     /* 884: the last CreateDevice's HRESULT (1: none yet) */
	LONG DeviceTime;       /* 888: the last CreateDevice's time */
	LONG DeviceWidth;      /* 892: its back buffer as asked for (windowed: 0, the window's) */
	LONG DeviceHeight;     /* 896 */
	LONG DeviceWindow;     /* 900: its focus window */
	LONG CooperativeLevel; /* 904: the last TestCooperativeLevel's HRESULT (1: none yet) */
	LONG LostChecks;       /* 908: TestCooperativeLevel results other than D3D_OK */
	LONG PresentResult;    /* 912: the last Present's HRESULT (1: none yet) */
	LONG PresentFailures;  /* 916: Present results other than D3D_OK */
	LONG FirstPresentTime; /* 920 */
	LONG LastPresentTime;  /* 924 */
	LONG LoadTick;         /* 928: GetTickCount when the DLL was loaded */
	LONG DeviceInside;     /* 932: 1 while a CreateDevice call has not returned */
	LONG DeviceCallTime;   /* 936: when the last CreateDevice was called */
} Shared;
#pragma pack(pop)

static Shared *S;
static HANDLE Mapping;
static HMODULE Self;

/* --- the event log (patcap.log in the client's folder, issue #98) ------------------------- */

static LONG Elapsed(void)
{
	return (LONG)(GetTickCount() - (DWORD)S->LoadTick);
}

/* One line: the milliseconds since the load, the text. Only rare events: it opens the file each time. */
static void Log(const char *Format, ...)
{
	char Line[512];
	int Length, More;
	va_list Arguments;
	HANDLE File;
	DWORD Written;
	Length = sprintf_s(Line, sizeof(Line), "%8ld ", (long)Elapsed());
	if (Length <= 0)
		return;
	va_start(Arguments, Format);
	More = vsprintf_s(Line + Length, sizeof(Line) - Length - 2, Format, Arguments);
	va_end(Arguments);
	if (More < 0)
		return;
	Length += More;
	Line[Length++] = '\r';
	Line[Length++] = '\n';
	File = CreateFileA("patcap.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (File == INVALID_HANDLE_VALUE)
		return;
	WriteFile(File, Line, (DWORD)Length, &Written, NULL);
	CloseHandle(File);
}

/* --- the virtual clock ------------------------------------------------------------------- */

static DWORD VirtualNow = BASE_TIME;
static DWORD RealBase;              /* realtime mode: timeGetTime at its start */
static DWORD VirtualAtRealBase;

static DWORD WINAPI HookTimeGetTime(void)
{
	if (S->Realtime)
		VirtualNow = VirtualAtRealBase + (timeGetTime() - RealBase);
	S->FirstCall = 1;
	S->Now = (LONG)(VirtualNow - BASE_TIME);
	return VirtualNow;
}

static void VirtualFileTime(LPFILETIME Time)
{
	ULONGLONG Value = FILETIME_BASE + (ULONGLONG)(HookTimeGetTime() - BASE_TIME) * 10000u;
	Time->dwLowDateTime = (DWORD)Value;
	Time->dwHighDateTime = (DWORD)(Value >> 32);
}

static void WINAPI HookGetSystemTimeAsFileTime(LPFILETIME Time)
{
	VirtualFileTime(Time);
}

static void WINAPI HookGetSystemTimePreciseAsFileTime(LPFILETIME Time)
{
	VirtualFileTime(Time);
}

static BOOL WINAPI HookGetCursorPos(LPPOINT Point)
{
	if (Point == NULL)
		return FALSE;
	Point->x = CURSOR_X;
	Point->y = CURSOR_Y;
	return TRUE;
}

/* --- the main timer ---------------------------------------------------------------------- */

static HWND TimerWindow;
static BOOL TimerActive;
static BOOL TimerReal;              /* realtime mode: the real timer runs */

static UINT_PTR WINAPI HookSetTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Proc)
{
	if (Window != NULL && Id == MAIN_TIMER_ID && Elapse == MAIN_TIMER_INTERVAL && Proc == NULL) {
		TimerWindow = Window;
		TimerActive = TRUE;
		S->TimerWindow = (LONG)(LONG_PTR)Window;
		S->TimerActive = 1;
		if (S->Realtime) {
			TimerReal = TRUE;
			return SetTimer(Window, Id, Elapse, Proc);
		}
		return Id;
	}
	return SetTimer(Window, Id, Elapse, Proc);
}

static BOOL WINAPI HookKillTimer(HWND Window, UINT_PTR Id)
{
	if (Window != NULL && Window == TimerWindow && Id == MAIN_TIMER_ID) {
		TimerActive = FALSE;
		S->TimerActive = 0;
		if (TimerReal) {
			TimerReal = FALSE;
			return KillTimer(Window, Id);
		}
		return TRUE;
	}
	return KillTimer(Window, Id);
}

/* --- frames ------------------------------------------------------------------------------ */

static BOOL CapturePending;
static LONG CaptureSkip;

/* --- profile windows (issue #134) --------------------------------------------------------- */

#define PROFILE_MAX_SAMPLES 400000  /* 40 s at 10,000 frames per second */
#define PROFILE_SMALL 1016          /* the small-block heap's limit in the original's CRT */

typedef struct {
	ULONGLONG Start;
	DWORD Present;
	LONG Allocations;
	LONG Frees;
	LONG Small;
	ULONGLONG Bytes;
} TProfileSample;

static volatile LONG ProfileCounting;
static volatile LONG ProfileAllocations, ProfileFrees, ProfileSmall;
static volatile LONGLONG ProfileBytes;
static TProfileSample *ProfileSamples;
static LONG ProfileCount, ProfileDropped;
static BOOL ProfileOpen;
static char ProfilePath[520];
static LARGE_INTEGER ProfileFrequency, ProfilePresentStart;
static LONG ProfileLastAllocations, ProfileLastFrees, ProfileLastSmall;
static LONGLONG ProfileLastBytes;

/* HeapAlloc etc. below are this DLL's own imports (PatchImports patches only the client's). */

static LPVOID WINAPI HookHeapAlloc(HANDLE Heap, DWORD Flags, SIZE_T Size)
{
	if (ProfileCounting) {
		InterlockedIncrement(&ProfileAllocations);
		InterlockedExchangeAdd64(&ProfileBytes, (LONGLONG)Size);
		if (Size <= PROFILE_SMALL)
			InterlockedIncrement(&ProfileSmall);
	}
	return HeapAlloc(Heap, Flags, Size);
}

static LPVOID WINAPI HookHeapReAlloc(HANDLE Heap, DWORD Flags, LPVOID Memory, SIZE_T Size)
{
	if (ProfileCounting) {
		InterlockedIncrement(&ProfileAllocations);
		InterlockedExchangeAdd64(&ProfileBytes, (LONGLONG)Size);
	}
	return HeapReAlloc(Heap, Flags, Memory, Size);
}

static BOOL WINAPI HookHeapFree(HANDLE Heap, DWORD Flags, LPVOID Memory)
{
	if (ProfileCounting)
		InterlockedIncrement(&ProfileFrees);
	return HeapFree(Heap, Flags, Memory);
}

static void ProfilePresentBegin(void)
{
	if (ProfileOpen)
		QueryPerformanceCounter(&ProfilePresentStart);
}

static void ProfilePresentEnd(void)
{
	LARGE_INTEGER End;
	LONG Allocations, Frees, Small;
	LONGLONG Bytes;
	if (!ProfileOpen)
		return;
	QueryPerformanceCounter(&End);
	Allocations = ProfileAllocations;
	Frees = ProfileFrees;
	Small = ProfileSmall;
	Bytes = InterlockedCompareExchange64(&ProfileBytes, 0, 0);
	if (ProfileCount < PROFILE_MAX_SAMPLES) {
		TProfileSample *Sample = &ProfileSamples[ProfileCount++];
		Sample->Start = (ULONGLONG)ProfilePresentStart.QuadPart;
		Sample->Present = (DWORD)(End.QuadPart - ProfilePresentStart.QuadPart);
		Sample->Allocations = Allocations - ProfileLastAllocations;
		Sample->Frees = Frees - ProfileLastFrees;
		Sample->Small = Small - ProfileLastSmall;
		Sample->Bytes = (ULONGLONG)(Bytes - ProfileLastBytes);
	} else {
		ProfileDropped++;
	}
	ProfileLastAllocations = Allocations;
	ProfileLastFrees = Frees;
	ProfileLastSmall = Small;
	ProfileLastBytes = Bytes;
}
static char CapturePath[520];

static void Finish(LONG Result, const char *Message)
{
	S->Result = Result;
	lstrcpynA(S->Message, Message ? Message : "", sizeof(S->Message));
	InterlockedExchange(&S->DoneSeq, S->CmdSeq);
}

/* Writes Width x Height pixels (rows top-down, Pitch bytes apart, in the given format) as a
   24-bit BMP. Format: 32 X8R8G8B8, 565 R5G6B5, 555 X1R5G5B5, 0x80e1 GL BGRA rows bottom-up. */
static BOOL WriteBitmap(const char *Path, const BYTE *Bits, int Pitch, int Width, int Height, int Format)
{
	int Row = (Width * 3 + 3) & ~3;
	BYTE *Line;
	BITMAPFILEHEADER File;
	BITMAPINFOHEADER Info;
	FILE *Out;
	int x, y;
	if (fopen_s(&Out, Path, "wb") != 0)
		return FALSE;
	memset(&File, 0, sizeof(File));
	memset(&Info, 0, sizeof(Info));
	File.bfType = 0x4d42;
	File.bfOffBits = sizeof(File) + sizeof(Info);
	File.bfSize = File.bfOffBits + Row * Height;
	Info.biSize = sizeof(Info);
	Info.biWidth = Width;
	Info.biHeight = Height;
	Info.biPlanes = 1;
	Info.biBitCount = 24;
	Info.biSizeImage = Row * Height;
	fwrite(&File, sizeof(File), 1, Out);
	fwrite(&Info, sizeof(Info), 1, Out);
	Line = (BYTE *)calloc(Row, 1);
	for (y = Height - 1; y >= 0; y--) {   /* BMP rows bottom-up */
		const BYTE *Source = Format == 0x80e1 ? Bits + (Height - 1 - y) * Pitch : Bits + y * Pitch;
		for (x = 0; x < Width; x++) {
			BYTE r, g, b;
			if (Format == 32 || Format == 0x80e1) {
				b = Source[x * 4];
				g = Source[x * 4 + 1];
				r = Source[x * 4 + 2];
			} else {
				WORD v = ((const WORD *)Source)[x];
				if (Format == 565) {
					r = (BYTE)((v >> 11) & 31); g = (BYTE)((v >> 5) & 63); b = (BYTE)(v & 31);
					r = (BYTE)((r << 3) | (r >> 2)); g = (BYTE)((g << 2) | (g >> 4)); b = (BYTE)((b << 3) | (b >> 2));
				} else {
					r = (BYTE)((v >> 10) & 31); g = (BYTE)((v >> 5) & 31); b = (BYTE)(v & 31);
					r = (BYTE)((r << 3) | (r >> 2)); g = (BYTE)((g << 3) | (g >> 2)); b = (BYTE)((b << 3) | (b >> 2));
				}
			}
			Line[x * 3] = b;
			Line[x * 3 + 1] = g;
			Line[x * 3 + 2] = r;
		}
		fwrite(Line, Row, 1, Out);
	}
	free(Line);
	return fclose(Out) == 0;
}

static void ProfileStart(LONG Flags, const char *Path)
{
	if (ProfileOpen) {
		Finish(4, "a profile window is open");
		return;
	}
	if (ProfileSamples == NULL) {
		ProfileSamples = (TProfileSample *)VirtualAlloc(NULL, PROFILE_MAX_SAMPLES * sizeof(TProfileSample),
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (ProfileSamples == NULL) {
			Finish(5, "no memory for the samples");
			return;
		}
	}
	lstrcpynA(ProfilePath, Path, sizeof(ProfilePath));
	QueryPerformanceFrequency(&ProfileFrequency);
	ProfileCount = 0;
	ProfileDropped = 0;
	ProfileLastAllocations = ProfileAllocations;
	ProfileLastFrees = ProfileFrees;
	ProfileLastSmall = ProfileSmall;
	ProfileLastBytes = InterlockedCompareExchange64(&ProfileBytes, 0, 0);
	ProfileOpen = TRUE;
	InterlockedExchange(&ProfileCounting, (Flags & PROFILE_ALLOCS) != 0);
	Finish(0, "");
}

static void ProfileStop(void)
{
	FILE *Out;
	LONG i;
	if (!ProfileOpen) {
		Finish(6, "no profile window is open");
		return;
	}
	ProfileOpen = FALSE;
	InterlockedExchange(&ProfileCounting, 0);
	if (fopen_s(&Out, ProfilePath, "w") != 0) {
		Finish(7, "cannot write the profile file");
		return;
	}
	fprintf(Out, "# patella-profile 1\n# freq %llu\n# dropped %ld\n", (unsigned long long)ProfileFrequency.QuadPart,
		(long)ProfileDropped);
	for (i = 0; i < ProfileCount; i++) {
		const TProfileSample *Sample = &ProfileSamples[i];
		fprintf(Out, "F %llu %lu %ld %ld %llu %ld\n", (unsigned long long)Sample->Start, (unsigned long)Sample->Present,
			(long)Sample->Allocations, (long)Sample->Frees, (unsigned long long)Sample->Bytes, (long)Sample->Small);
	}
	Finish(fclose(Out) == 0 ? 0 : 7, "");
}

/* Called before each frame is shown; TRUE when this frame is to be captured. */
static BOOL FrameShown(int Api)
{
	if (S->Frames == 0)
		Log("first frame (API %d)", Api);
	S->Frames++;
	S->Api = Api;
	S->ApiFrames[Api]++;
	if (!CapturePending)
		return FALSE;
	if (CaptureSkip > 0) {
		CaptureSkip--;
		return FALSE;
	}
	CapturePending = FALSE;
	return TRUE;
}

static void Captured(BOOL Ok, int Width, int Height, int Bits, const char *Error)
{
	S->Width = Width;
	S->Height = Height;
	S->SourceBits = Bits;
	if (Ok)
		Log("capture done: %dx%d, %d bits, frame %ld", Width, Height, Bits, (long)S->Frames);
	else
		Log("capture failed: %s", Error);
	Finish(Ok ? 0 : 2, Ok ? "" : Error);
}

static int FormatOfMasks(DWORD Bits, DWORD Red, DWORD Green, DWORD Blue)
{
	if (Bits == 32 && Red == 0xff0000 && Green == 0xff00 && Blue == 0xff)
		return 32;
	if (Bits == 16 && Red == 0xf800 && Green == 0x7e0 && Blue == 0x1f)
		return 565;
	if (Bits == 16 && Red == 0x7c00 && Green == 0x3e0 && Blue == 0x1f)
		return 555;
	return 0;
}

static void CaptureDirectDraw(IDirectDrawSurface4 *Surface, const RECT *Area)
{
	DDSURFACEDESC2 Desc;
	RECT Whole;
	HRESULT Result;
	int Format;
	memset(&Desc, 0, sizeof(Desc));
	Desc.dwSize = sizeof(Desc);
	if (Area == NULL) {
		if (IDirectDrawSurface4_GetSurfaceDesc(Surface, &Desc) != DD_OK) {
			Captured(FALSE, 0, 0, 0, "GetSurfaceDesc failed");
			return;
		}
		SetRect(&Whole, 0, 0, Desc.dwWidth, Desc.dwHeight);
		Area = &Whole;
		memset(&Desc, 0, sizeof(Desc));
		Desc.dwSize = sizeof(Desc);
	}
	Result = IDirectDrawSurface4_Lock(Surface, (LPRECT)Area, &Desc, DDLOCK_WAIT | DDLOCK_READONLY | DDLOCK_SURFACEMEMORYPTR, NULL);
	if (Result != DD_OK) {
		char Text[64];
		sprintf_s(Text, sizeof(Text), "Lock failed: 0x%08lx", (unsigned long)Result);
		Captured(FALSE, 0, 0, 0, Text);
		return;
	}
	Format = FormatOfMasks(Desc.ddpfPixelFormat.dwRGBBitCount, Desc.ddpfPixelFormat.dwRBitMask,
		Desc.ddpfPixelFormat.dwGBitMask, Desc.ddpfPixelFormat.dwBBitMask);
	if (Format == 0) {
		IDirectDrawSurface4_Unlock(Surface, (LPRECT)Area);
		Captured(FALSE, 0, 0, Desc.ddpfPixelFormat.dwRGBBitCount, "unknown pixel format");
		return;
	}
	{
		int Width = Area->right - Area->left, Height = Area->bottom - Area->top;
		BOOL Ok = WriteBitmap(CapturePath, (const BYTE *)Desc.lpSurface, Desc.lPitch, Width, Height, Format);
		IDirectDrawSurface4_Unlock(Surface, (LPRECT)Area);
		Captured(Ok, Width, Height, Desc.ddpfPixelFormat.dwRGBBitCount, "cannot write the file");
	}
}

/* DirectDraw: vtables patched once each (the surfaces share one in practice). */
typedef HRESULT (STDMETHODCALLTYPE *TBlt)(IDirectDrawSurface4 *, LPRECT, IDirectDrawSurface4 *, LPRECT, DWORD, LPDDBLTFX);
typedef HRESULT (STDMETHODCALLTYPE *TFlip)(IDirectDrawSurface4 *, IDirectDrawSurface4 *, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *TCreateSurface)(IDirectDraw4 *, LPDDSURFACEDESC2, IDirectDrawSurface4 **, IUnknown *);
typedef HRESULT (WINAPI *TDirectDrawCreate)(GUID *, LPDIRECTDRAW *, IUnknown *);

#define MAX_VTABLES 8
typedef struct { void **Vtable; void *Original; } TPatch;
static TPatch BltPatches[MAX_VTABLES], FlipPatches[MAX_VTABLES], CreateSurfacePatches[MAX_VTABLES];
static TPatch PresentPatches[MAX_VTABLES], CreateDevicePatches[MAX_VTABLES], CooperativeLevelPatches[MAX_VTABLES];

static void *Original(TPatch *Patches, void *Object)
{
	void **Vtable = *(void ***)Object;
	int i;
	for (i = 0; i < MAX_VTABLES; i++)
		if (Patches[i].Vtable == Vtable)
			return Patches[i].Original;
	return NULL;
}

static void PatchVtable(TPatch *Patches, void *Object, int Index, void *Hook)
{
	void **Vtable = *(void ***)Object;
	DWORD Old;
	int i;
	for (i = 0; i < MAX_VTABLES; i++)
		if (Patches[i].Vtable == Vtable) {
			if (Vtable[Index] == Hook)
				return;
			/* Issue #161: seen in Windows Sandbox, a start's second Direct3DCreate9 hands out an object whose vtable,
			   at the same place, has its original slot again (not a reload of d3d9.dll: pinning it changed nothing;
			   tools/compare_rendering.py takes such a start for another). The entry is stale: the hook goes in again. */
			Log("the vtable at 0x%lx is unpatched again: patching it again",
				(unsigned long)(ULONG_PTR)Vtable);
			break;
		}
	if (i == MAX_VTABLES)
		for (i = 0; i < MAX_VTABLES; i++)
			if (Patches[i].Vtable == NULL)
				break;
	if (i == MAX_VTABLES || Vtable[Index] == Hook)
		return;
	Patches[i].Original = Vtable[Index];
	if (VirtualProtect(&Vtable[Index], sizeof(void *), PAGE_READWRITE, &Old)) {
		Vtable[Index] = Hook;
		VirtualProtect(&Vtable[Index], sizeof(void *), Old, &Old);
		Patches[i].Vtable = Vtable;
	}
}

static BOOL IsPrimary(IDirectDrawSurface4 *Surface)
{
	DDSCAPS2 Caps;
	memset(&Caps, 0, sizeof(Caps));
	return IDirectDrawSurface4_GetCaps(Surface, &Caps) == DD_OK && (Caps.dwCaps & DDSCAPS_PRIMARYSURFACE) != 0;
}

static HRESULT STDMETHODCALLTYPE HookBlt(IDirectDrawSurface4 *This, LPRECT To, IDirectDrawSurface4 *From,
	LPRECT FromRect, DWORD Flags, LPDDBLTFX Fx)
{
	TBlt Real = (TBlt)Original(BltPatches, This);
	if (From != NULL && (Flags & DDBLT_COLORFILL) == 0 && IsPrimary(This) && FrameShown(API_DDRAW_BLT))
		CaptureDirectDraw(From, FromRect);
	return Real(This, To, From, FromRect, Flags, Fx);
}

static HRESULT STDMETHODCALLTYPE HookFlip(IDirectDrawSurface4 *This, IDirectDrawSurface4 *Target, DWORD Flags)
{
	TFlip Real = (TFlip)Original(FlipPatches, This);
	if (FrameShown(API_DDRAW_FLIP)) {
		DDSCAPS2 Caps;
		IDirectDrawSurface4 *Back = NULL;
		memset(&Caps, 0, sizeof(Caps));
		Caps.dwCaps = DDSCAPS_BACKBUFFER;
		if (IDirectDrawSurface4_GetAttachedSurface(This, &Caps, &Back) == DD_OK && Back != NULL) {
			CaptureDirectDraw(Back, NULL);
			IDirectDrawSurface4_Release(Back);
		} else {
			Captured(FALSE, 0, 0, 0, "no back buffer");
		}
	}
	return Real(This, Target, Flags);
}

static void PatchSurface(IDirectDrawSurface4 *Surface)
{
	PatchVtable(BltPatches, Surface, 5, (void *)HookBlt);      /* IDirectDrawSurface4::Blt */
	PatchVtable(FlipPatches, Surface, 11, (void *)HookFlip);   /* ::Flip */
}

static HRESULT STDMETHODCALLTYPE HookCreateSurface(IDirectDraw4 *This, LPDDSURFACEDESC2 Desc,
	IDirectDrawSurface4 **Surface, IUnknown *Outer)
{
	TCreateSurface Real = (TCreateSurface)Original(CreateSurfacePatches, This);
	HRESULT Result = Real(This, Desc, Surface, Outer);
	if (Result == DD_OK && Surface != NULL && *Surface != NULL)
		PatchSurface(*Surface);
	return Result;
}

static HRESULT WINAPI HookDirectDrawCreate(GUID *Guid, LPDIRECTDRAW *DirectDraw, IUnknown *Outer)
{
	HRESULT Result = DirectDrawCreate(Guid, DirectDraw, Outer);
	if (Result == DD_OK && DirectDraw != NULL && *DirectDraw != NULL) {
		IDirectDraw4 *DirectDraw4 = NULL;
		if (IDirectDraw_QueryInterface(*DirectDraw, &IID_IDirectDraw4, (void **)&DirectDraw4) == DD_OK) {
			PatchVtable(CreateSurfacePatches, DirectDraw4, 6, (void *)HookCreateSurface);  /* IDirectDraw4::CreateSurface */
			IDirectDraw4_Release(DirectDraw4);
		}
	}
	return Result;
}

/* Direct3D 9 */
typedef HRESULT (STDMETHODCALLTYPE *TPresent)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT (STDMETHODCALLTYPE *TTestCooperativeLevel)(IDirect3DDevice9 *);
typedef HRESULT (STDMETHODCALLTYPE *TCreateDevice)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD,
	D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
typedef IDirect3D9 *(WINAPI *TDirect3DCreate9)(UINT);
static TDirect3DCreate9 RealDirect3DCreate9;

static void CaptureDirect3D9(IDirect3DDevice9 *Device, const RECT *Area)
{
	IDirect3DSurface9 *Back = NULL, *Copy = NULL;
	D3DSURFACE_DESC Desc;
	D3DLOCKED_RECT Locked = {0, NULL};
	RECT Whole;
	int Format;
	HRESULT Result = IDirect3DDevice9_GetBackBuffer(Device, 0, 0, D3DBACKBUFFER_TYPE_MONO, &Back);
	if (Result != D3D_OK || Back == NULL) {
		Captured(FALSE, 0, 0, 0, "GetBackBuffer failed");
		return;
	}
	IDirect3DSurface9_GetDesc(Back, &Desc);
	Format = Desc.Format == D3DFMT_X8R8G8B8 || Desc.Format == D3DFMT_A8R8G8B8 ? 32
		: Desc.Format == D3DFMT_R5G6B5 ? 565 : Desc.Format == D3DFMT_X1R5G5B5 ? 555 : 0;
	if (Format == 0) {
		IDirect3DSurface9_Release(Back);
		Captured(FALSE, 0, 0, 0, "unknown back buffer format");
		return;
	}
	if (Area == NULL) {
		SetRect(&Whole, 0, 0, Desc.Width, Desc.Height);
		Area = &Whole;
	}
	Result = IDirect3DDevice9_CreateOffscreenPlainSurface(Device, Desc.Width, Desc.Height, Desc.Format,
		D3DPOOL_SYSTEMMEM, &Copy, NULL);
	if (Result == D3D_OK)
		Result = IDirect3DDevice9_GetRenderTargetData(Device, Back, Copy);
	if (Result == D3D_OK)
		Result = IDirect3DSurface9_LockRect(Copy, &Locked, Area, D3DLOCK_READONLY);
	if (Result != D3D_OK) {
		char Text[64];
		sprintf_s(Text, sizeof(Text), "read-back failed: 0x%08lx", (unsigned long)Result);
		if (Copy != NULL)
			IDirect3DSurface9_Release(Copy);
		IDirect3DSurface9_Release(Back);
		Captured(FALSE, 0, 0, 0, Text);
		return;
	}
	{
		int Width = Area->right - Area->left, Height = Area->bottom - Area->top;
		BOOL Ok = WriteBitmap(CapturePath, (const BYTE *)Locked.pBits, Locked.Pitch, Width, Height, Format);
		IDirect3DSurface9_UnlockRect(Copy);
		IDirect3DSurface9_Release(Copy);
		IDirect3DSurface9_Release(Back);
		Captured(Ok, Width, Height, Format == 32 ? 32 : 16, "cannot write the file");
	}
}

static HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9 *This, const RECT *From, const RECT *To, HWND Window,
	const RGNDATA *Dirty)
{
	TPresent Real = (TPresent)Original(PresentPatches, This);
	HRESULT Result;
	LONG Now = Elapsed();
	if (S->FirstPresentTime < 0)
		S->FirstPresentTime = Now;
	S->LastPresentTime = Now;
	if (FrameShown(API_D3D9))
		CaptureDirect3D9(This, From);
	ProfilePresentBegin();
	Result = Real(This, From, To, Window, Dirty);
	ProfilePresentEnd();
	if (Result != D3D_OK)
		S->PresentFailures++;
	if (Result != S->PresentResult)
		Log("Present: 0x%08lx (frame %ld)", (unsigned long)Result, (long)S->Frames);
	S->PresentResult = Result;
	return Result;
}

/* The client asks it before every frame (surfaceDX9.cpp RestoreSurfaces): a lost device shows here. */
static HRESULT STDMETHODCALLTYPE HookTestCooperativeLevel(IDirect3DDevice9 *This)
{
	TTestCooperativeLevel Real = (TTestCooperativeLevel)Original(CooperativeLevelPatches, This);
	HRESULT Result = Real(This);
	if (Result != D3D_OK)
		S->LostChecks++;
	if (Result != S->CooperativeLevel)
		Log("TestCooperativeLevel: 0x%08lx (frame %ld)", (unsigned long)Result, (long)S->Frames);
	S->CooperativeLevel = Result;
	return Result;
}

static HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirect3D9 *This, UINT Adapter, D3DDEVTYPE Type, HWND Window,
	DWORD Flags, D3DPRESENT_PARAMETERS *Parameters, IDirect3DDevice9 **Device)
{
	TCreateDevice Real = (TCreateDevice)Original(CreateDevicePatches, This);
	HRESULT Result;
	S->DeviceCreates++;
	S->DeviceInside = 1;
	S->DeviceCallTime = Elapsed();
	Log("CreateDevice called (call %ld)", (long)S->DeviceCreates);
	Result = Real(This, Adapter, Type, Window, Flags, Parameters, Device);
	S->DeviceInside = 0;
	S->DeviceResult = Result;
	S->DeviceTime = Elapsed();
	S->DeviceWindow = (LONG)(LONG_PTR)Window;
	S->DeviceWidth = Parameters != NULL ? (LONG)Parameters->BackBufferWidth : -1;
	S->DeviceHeight = Parameters != NULL ? (LONG)Parameters->BackBufferHeight : -1;
	Log("CreateDevice(adapter %u, type %d, window 0x%lx, flags 0x%lx, windowed %d, %ldx%ld): 0x%08lx", Adapter,
		(int)Type, (unsigned long)(ULONG_PTR)Window, (unsigned long)Flags, Parameters != NULL ? Parameters->Windowed : -1,
		(long)S->DeviceWidth, (long)S->DeviceHeight, (unsigned long)Result);
	if (Result == D3D_OK && Device != NULL && *Device != NULL) {
		PatchVtable(PresentPatches, *Device, 17, (void *)HookPresent);  /* IDirect3DDevice9::Present */
		PatchVtable(CooperativeLevelPatches, *Device, 3, (void *)HookTestCooperativeLevel);  /* ::TestCooperativeLevel */
	}
	return Result;
}

static IDirect3D9 *WINAPI HookDirect3DCreate9(UINT Version)
{
	IDirect3D9 *Direct3D = RealDirect3DCreate9(Version);
	S->Direct3DCreates++;
	Log("Direct3DCreate9(%u): %s", Version, Direct3D != NULL ? "an object" : "NULL");
	if (Direct3D != NULL)
		PatchVtable(CreateDevicePatches, Direct3D, 16, (void *)HookCreateDevice);  /* IDirect3D9::CreateDevice */
	return Direct3D;
}

/* OpenGL: the context last made current with each DC (SurfaceGL.cpp switches to its pbuffer). */
#define MAX_CONTEXTS 8
static struct { HDC DC; HGLRC Context; } Contexts[MAX_CONTEXTS];

static BOOL WINAPI HookWglMakeCurrent(HDC DC, HGLRC Context)
{
	int i;
	if (DC != NULL && Context != NULL) {
		for (i = 0; i < MAX_CONTEXTS; i++)
			if (Contexts[i].DC == DC || Contexts[i].DC == NULL) {
				Contexts[i].DC = DC;
				Contexts[i].Context = Context;
				break;
			}
	}
	return wglMakeCurrent(DC, Context);
}

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80e1
#endif

static void CaptureOpenGL(HDC DC)
{
	HGLRC Context = NULL, OldContext = wglGetCurrentContext();
	HDC OldDC = wglGetCurrentDC();
	HWND Window = WindowFromDC(DC);
	RECT Client;
	GLint ReadBuffer, Alignment, RowLength, SkipRows, SkipPixels;
	BYTE *Pixels;
	int i, Width, Height;
	BOOL Ok;
	for (i = 0; i < MAX_CONTEXTS; i++)
		if (Contexts[i].DC == DC)
			Context = Contexts[i].Context;
	if (Context == NULL || Window == NULL || !GetClientRect(Window, &Client)) {
		Captured(FALSE, 0, 0, 0, "no OpenGL context for the DC");
		return;
	}
	Width = Client.right;
	Height = Client.bottom;
	if (OldContext != Context)
		wglMakeCurrent(DC, Context);
	glGetIntegerv(GL_READ_BUFFER, &ReadBuffer);
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &RowLength);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &SkipRows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &SkipPixels);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	Pixels = (BYTE *)malloc(Width * Height * 4);
	glReadPixels(0, 0, Width, Height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, Pixels);
	Ok = glGetError() == GL_NO_ERROR;
	glReadBuffer(ReadBuffer);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, RowLength);
	glPixelStorei(GL_PACK_SKIP_ROWS, SkipRows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, SkipPixels);
	if (OldContext != Context)
		wglMakeCurrent(OldDC, OldContext);
	if (Ok)
		Ok = WriteBitmap(CapturePath, Pixels, Width * 4, Width, Height, GL_BGRA_EXT);
	free(Pixels);
	Captured(Ok, Width, Height, 32, "glReadPixels or the file failed");
}

static BOOL WINAPI HookSwapBuffers(HDC DC)
{
	if (FrameShown(API_OPENGL))
		CaptureOpenGL(DC);
	return SwapBuffers(DC);
}

/* --- commands ---------------------------------------------------------------------------- */

static void Advance(LONG Milliseconds, LONG Ticks)
{
	LONG i, Step = Ticks > 0 ? Milliseconds / Ticks : 0;
	if (S->Realtime) {
		Finish(3, "ADVANCE in realtime mode");
		return;
	}
	for (i = 0; i < Ticks; i++) {
		VirtualNow += Step;
		if (TimerActive && TimerWindow != NULL) {
			SendMessageA(TimerWindow, WM_TIMER, MAIN_TIMER_ID, 0);
			S->Ticks++;
		}
	}
	VirtualNow += Milliseconds - Step * Ticks;
	S->Now = (LONG)(VirtualNow - BASE_TIME);
	Finish(TimerActive || Ticks == 0 ? 0 : 1, TimerActive || Ticks == 0 ? "" : "the main timer is not set");
}

static void SetRealtime(BOOL On)
{
	if (On && !S->Realtime) {
		RealBase = timeGetTime();
		VirtualAtRealBase = VirtualNow;
		S->Realtime = 1;
		if (TimerActive && TimerWindow != NULL && !TimerReal) {
			SetTimer(TimerWindow, MAIN_TIMER_ID, MAIN_TIMER_INTERVAL, NULL);
			TimerReal = TRUE;
		}
	} else if (!On && S->Realtime) {
		HookTimeGetTime();
		S->Realtime = 0;
		if (TimerReal) {
			KillTimer(TimerWindow, MAIN_TIMER_ID);
			TimerReal = FALSE;
		}
	}
	Finish(0, "");
}

static void Commands(void)
{
	if (S->CmdSeq == S->DoneSeq || CapturePending)
		return;
	switch (S->Cmd) {
	case CMD_ADVANCE:
		Advance(S->Arg0, S->Arg1);
		break;
	case CMD_CAPTURE:
		Log("capture asked for: skip %ld, frame %ld, Present %ld calls (the last 0x%08lx at %ld)", (long)S->Arg0,
			(long)S->Frames, (long)S->ApiFrames[API_D3D9], (unsigned long)S->PresentResult, (long)S->LastPresentTime);
		lstrcpynA(CapturePath, S->Path, sizeof(CapturePath));
		CaptureSkip = S->Arg0;
		CapturePending = TRUE;
		break;
	case CMD_REALTIME:
		SetRealtime(S->Arg0 != 0);
		break;
	case CMD_PROFILE_START:
		ProfileStart(S->Arg0, S->Path);
		break;
	case CMD_PROFILE_STOP:
		ProfileStop();
		break;
	default:
		Finish(9, "unknown command");
	}
}

static BOOL WINAPI HookPeekMessageA(LPMSG Msg, HWND Window, UINT First, UINT Last, UINT Remove)
{
	Commands();
	return PeekMessageA(Msg, Window, First, Last, Remove);
}

static FARPROC WINAPI HookGetProcAddress(HMODULE Module, LPCSTR Name)
{
	FARPROC Result = GetProcAddress(Module, Name);
	if (Result != NULL && ((ULONG_PTR)Name >> 16) != 0) {
		if (strcmp(Name, "Direct3DCreate9") == 0) {
			RealDirect3DCreate9 = (TDirect3DCreate9)Result;
			return (FARPROC)HookDirect3DCreate9;
		}
		if (strcmp(Name, "GetSystemTimePreciseAsFileTime") == 0)
			return (FARPROC)HookGetSystemTimePreciseAsFileTime;
	}
	return Result;
}

/* --- import address table ---------------------------------------------------------------- */

typedef struct { const char *Dll, *Name; void *Hook; } THook;
static const THook Hooks[] = {
	{"winmm.dll", "timeGetTime", (void *)HookTimeGetTime},
	{"kernel32.dll", "GetSystemTimeAsFileTime", (void *)HookGetSystemTimeAsFileTime},
	{"kernel32.dll", "GetSystemTimePreciseAsFileTime", (void *)HookGetSystemTimePreciseAsFileTime},
	{"kernel32.dll", "GetProcAddress", (void *)HookGetProcAddress},
	{"user32.dll", "SetTimer", (void *)HookSetTimer},
	{"user32.dll", "KillTimer", (void *)HookKillTimer},
	{"user32.dll", "GetCursorPos", (void *)HookGetCursorPos},
	{"user32.dll", "PeekMessageA", (void *)HookPeekMessageA},
	{"gdi32.dll", "SwapBuffers", (void *)HookSwapBuffers},
	{"opengl32.dll", "wglMakeCurrent", (void *)HookWglMakeCurrent},
	{"ddraw.dll", "DirectDrawCreate", (void *)HookDirectDrawCreate},
	{"kernel32.dll", "HeapAlloc", (void *)HookHeapAlloc},
	{"kernel32.dll", "HeapReAlloc", (void *)HookHeapReAlloc},
	{"kernel32.dll", "HeapFree", (void *)HookHeapFree},
};

static int PatchImports(void)
{
	BYTE *Base = (BYTE *)GetModuleHandleA(NULL);
	IMAGE_NT_HEADERS *Nt = (IMAGE_NT_HEADERS *)(Base + ((IMAGE_DOS_HEADER *)Base)->e_lfanew);
	IMAGE_DATA_DIRECTORY *Dir = &Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	IMAGE_IMPORT_DESCRIPTOR *Import = (IMAGE_IMPORT_DESCRIPTOR *)(Base + Dir->VirtualAddress);
	int Found = 0;
	for (; Import->Name != 0; Import++) {
		const char *Dll = (const char *)(Base + Import->Name);
		IMAGE_THUNK_DATA *Names = (IMAGE_THUNK_DATA *)(Base + (Import->OriginalFirstThunk ? Import->OriginalFirstThunk : Import->FirstThunk));
		IMAGE_THUNK_DATA *Slots = (IMAGE_THUNK_DATA *)(Base + Import->FirstThunk);
		for (; Names->u1.AddressOfData != 0; Names++, Slots++) {
			const char *Name;
			int i;
			if (IMAGE_SNAP_BY_ORDINAL(Names->u1.Ordinal))
				continue;
			Name = (const char *)((IMAGE_IMPORT_BY_NAME *)(Base + Names->u1.AddressOfData))->Name;
			for (i = 0; i < (int)(sizeof(Hooks) / sizeof(Hooks[0])); i++) {
				if (_stricmp(Dll, Hooks[i].Dll) == 0 && strcmp(Name, Hooks[i].Name) == 0) {
					DWORD Old;
					if (VirtualProtect(&Slots->u1.Function, sizeof(void *), PAGE_READWRITE, &Old)) {
						Slots->u1.Function = (ULONG_PTR)Hooks[i].Hook;
						VirtualProtect(&Slots->u1.Function, sizeof(void *), Old, &Old);
						Found |= 1 << i;
					}
				}
			}
		}
	}
	return Found;
}

/* The function the clients' copies import (by name); never called. */
__declspec(dllexport) int __cdecl PatcapLoaded(void)
{
	return S != NULL;
}

BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, LPVOID Reserved)
{
	(void)Reserved;
	if (Reason == DLL_PROCESS_ATTACH) {
		char Name[64];
		Self = Instance;
		DisableThreadLibraryCalls(Instance);
		sprintf_s(Name, sizeof(Name), "Local\\PatellaCapture-%lu", GetCurrentProcessId());
		Mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, Name);
		if (Mapping == NULL)
			return FALSE;
		S = (Shared *)MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
		if (S == NULL)
			return FALSE;
		S->Magic = 'PCAP';
		S->Version = 5;
		S->LoadTick = (LONG)GetTickCount();
		S->DeviceResult = 1;
		S->CooperativeLevel = 1;
		S->PresentResult = 1;
		S->FirstPresentTime = -1;
		S->LastPresentTime = -1;
		S->DeviceTime = -1;
		S->DeviceCallTime = -1;
		S->Hooks = PatchImports();
		Log("loaded in PID %lu; hooks 0x%x", GetCurrentProcessId(), (unsigned)S->Hooks);
	}
	return TRUE;
}
