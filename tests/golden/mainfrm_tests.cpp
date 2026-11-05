// MainFrm.cpp against the original (0x4a8200-0x4abec8), issue #37.
//
// CMainFrame is an MFC frame window: the image's is laid out by MFC 7.1
// (CFrameWnd's 0xd0 bytes, m_hWnd at +0x1c), ours by the test build's MFC
// shim, so each side has its own object (the image's built by its
// constructor 0x4a8520 in a buffer of 0x300 bytes, ours by ours, both
// buffers filled with the same pattern first) and the members are compared
// by name (MouseCursor +0xd0, Placement +0xd4, KeyStates +0x100, KeyFlags
// +0x200, m_hWnd +0x1c in the image). MainFrm.cpp is compiled into this file
// (#include below, not linked on its own), so its statics are this file's.
//
// Nothing reaches Windows or MFC. Every function the file calls outside
// itself is a fake that logs into the calling side's log (Side), one fake
// for both sides:
// - user32 and shell32 (PostMessageA, Get{Client,Window}Rect, IsIconic,
//   IsZoomed, Set/KillTimer, SetCursor, GetCursorPos, PtInRect,
//   ClientToScreen, ScreenToClient, SetCapture, ReleaseCapture,
//   GetSystemMetrics, LoadCursorA, DestroyCursor, ShellExecuteA): the image's
//   import slots and difftest.exe's own;
// - MFC: MFC is linked into both clients (7.1 in the image, 14 in
//   Patella.exe), so the comparison is at the level of the calls into MFC
//   (the behaviour of the handlers' MFC parts, CWnd::Default and the
//   CFrameWnd base handlers, is MFC's, not the reconstruction's): the
//   image's CFrameWnd constructor and destructor, CWnd::Default,
//   Get/SetWindowPlacement, ModifyStyle/Ex, SetWindowPos, MoveWindow,
//   FromHandle, DestroyWindow, the CFrameWnd handlers (OnCreate, OnSize,
//   OnClose, OnDestroy, OnSetCursor, PreCreateWindow) and AfxGetModuleState
//   (AfxGetApp and AfxGetInstanceHandle, inline in MFC 7.1) start with a
//   jump to the fakes; ours (the shim's declarations) are defined here;
// - Control.cpp (playertest.cpp's stubs on our side), MainWindow.cpp's
//   InitMainWindow, Preferences.cpp's view and window modes, Status.cpp's
//   mouse cursor and position, the connection state and AddMessage,
//   Network.cpp's functions (Network.cpp has its own test). Until issue #74
//   also Creatures.cpp's GetPlayerID and GetCreature and the image's three
//   ostream members, OutputErrorInformations' calls (the script still draws
//   the player and its position, so that the random inputs stay as they
//   were).
//
// Each round constructs both frames, runs random operations through every
// handler and free function of the file (keys with every modifier and the
// extended flag, characters, the mouse buttons and the wheel in windowed and
// fullscreen mode, capture changes, sizes below and above the minimum,
// minimised, the cursor inside and outside, every mouse cursor, display
// changes, the host lookup's reply with and without an error in every
// connection state, the display mode switch with every engine, closing with
// and without CloseRequest's consent, a packet header or none; until issue
// #74 OutputErrorInformations with them) and destroys them; after each
// operation the logs, the frames' members and the file's statics are
// compared. The error round (TErrorRound) after the counted checks: every
// operation with the n-th fake call throwing an assertion of the calling
// side, for every n (the destructor excepted: ours is noexcept, notes/
// decisions.md, issue #34).
//
// DIFFTEST_ONLY=MainFrm runs it alone (MF_TRACE=1 traces the operations,
// MF_LOGS=<file> writes every compared log, MF_ROUNDS=<n>).
//
// A golden-corpus test (issue #54; tests/golden/golden.h, golden_eh.h): the
// oracle runs both sides as above and records the original's side of every
// comparison by value: its log, its state (the frame's members, the file's
// statics and the world its fakes keep, golden::OutputRuns), in the error
// round what its call threw (ORIGINAL_THROWN) and how many fake calls it made.
// The runner runs our side alone, against the same fakes (our functions
// redirected, user32's and shell32's import slots of the runner bound to
// them, the MFC shim's members defined here), and compares with the recorded
// original. MF_ROUNDS changes the inputs: not with a corpus.

// golden_test.h first (common.h: it sets _CRT_SECURE_NO_WARNINGS before any C runtime header).
#include "golden_test.h"

#include "golden_eh.h"
#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#else
#include "golden_patch.h"
#endif
// MainFrm.cpp is compiled into this file (its statics are in an unnamed namespace), at its own warning
// level (cmake/Warnings.cmake's baseline).
#ifdef _MSC_VER
#pragma warning(push, 3)
#endif
#include "../../src/MainFrm.cpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <new>
#include <string>
#include <vector>
#include <stdarg.h>

#ifdef _MSC_VER
#pragma warning(disable : 4100)
#endif

namespace {

enum { IMAGE = 0,
	OURS = 1 };
int Side;
std::string Log[2];
bool Trace;
FILE *LogFile;
const char *What = "";

void Logf(const char *Format, ...)
{
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

// --- Patches ---------------------------------------------------------------------------------

struct TSaved {
	void *Address;
	size_t Size;
	uint8 Bytes[8];
};
std::vector<TSaved> Saved;
void Write(void *Address, const void *Data, size_t Size)
{
	TSaved S;
	S.Address = Address;
	S.Size = Size;
	memcpy(S.Bytes, Address, Size);
	DWORD Protection;
	VirtualProtect(Address, Size, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(Address, Data, Size);
	VirtualProtect(Address, Size, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Address, Size);
	Saved.push_back(S);
}
void Redirect(void *Function, void *Target)
{
	intptr_t Distance = (intptr_t)Target - ((intptr_t)Function + 5);
	if (Distance != (int32_t)Distance) { // the runner (x64): both in its image, within reach
		printf("FAIL MainFrm: cannot redirect %p to %p: out of a rel32 jump's reach\n", Function, Target);
		exit(1);
	}
	uint8 Jump[5];
	Jump[0] = 0xe9;
	int32_t Offset = (int32_t)Distance;
	memcpy(Jump + 1, &Offset, 4);
	Write(Function, Jump, 5);
}
void RestoreAll()
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
#if GOLDEN_ORACLE
uint32 *ImageImport(const char *Dll, const char *Name)
{
	uint8 *Base = (uint8 *)(uintptr_t)0x400000;
	IMAGE_NT_HEADERS32 *NT = (IMAGE_NT_HEADERS32 *)(Base + ((IMAGE_DOS_HEADER *)Base)->e_lfanew);
	IMAGE_DATA_DIRECTORY &Directory = NT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	for (IMAGE_IMPORT_DESCRIPTOR *D = (IMAGE_IMPORT_DESCRIPTOR *)(Base + Directory.VirtualAddress); D->Name != 0; D++) {
		if (_stricmp((const char *)Base + D->Name, Dll) != 0)
			continue;
		uint32 *Names = (uint32 *)(Base + D->OriginalFirstThunk);
		uint32 *Slots = (uint32 *)(Base + D->FirstThunk);
		for (int i = 0; Names[i] != 0; i++)
			if ((Names[i] & 0x80000000) == 0 && strcmp((const char *)Base + Names[i] + 2, Name) == 0)
				return &Slots[i];
	}
	return NULL;
}
void **OwnImport(const char *Dll, const char *Name)
{
	uint8 *Base = (uint8 *)GetModuleHandleA(NULL);
	HMODULE Module = GetModuleHandleA(Dll);
	void *Real = Module != NULL ? (void *)GetProcAddress(Module, Name) : NULL;
	if (Real == NULL)
		return NULL;
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
#endif
// The image's import slot and ours (the oracle), the runner's (by name, golden_patch.h).
void BindBoth([[maybe_unused]] const char *Dll, const char *Name, void *Fake)
{
#if GOLDEN_ORACLE
	uint32 *Slot = ImageImport(Dll, Name);
	void **Own = OwnImport(Dll, Name);
	if (Slot == NULL || Own == NULL) {
		printf("FAIL MainFrm: no import slot for %s %s (image %p, ours %p)\n", Dll, Name, (void *)Slot, (void *)Own);
		Failures++;
		return;
	}
	Write(Slot, &Fake, 4);
#else
	void **Own = golden::FindImport(Name);
	if (Own == nullptr) {
		printf("FAIL MainFrm: no import slot for %s\n", Name);
		Failures++;
		return;
	}
#endif
	Write(Own, &Fake, sizeof(Fake));
}

// --- The world the fakes answer from ---------------------------------------------------------

// Decided before both sides run.
struct TScript {
	LRESULT Default;
	BOOL CloseRequest;
	int OnCreate;
	BOOL OnSetCursor;
	BOOL DestroyWindow;
	BOOL PreCreate;
	bool Iconic, Zoomed;
	RECT Client;  // GetClientRect's (left/top 0)
	RECT Window;  // GetWindowRect's
	POINT Origin; // the client area's screen position
	POINT Cursor; // GetCursorPos's
	int ScreenWidth, ScreenHeight;
	WINDOWPLACEMENT Placement; // GetWindowPlacement's
	bool HasApp;
	int PlayerID;
	int CreatureX, CreatureY, CreatureZ;
	bool HasCreature;
	char ResolvedIP[24];
};
TScript Script;

struct TSideWorld {
	int ViewMode;
	int WindowedX, WindowedY, WindowedWidth, WindowedHeight;
	bool WindowedMaximized;
	int MouseCursor;
	bool WindowsMousepointer;
	int MouseX, MouseY;
	int ConnectionState;
	int Ticks;
};
TSideWorld World[2];
int FailAt;
bool NoTick;

void Tick()
{
	if (FailAt == 0 || NoTick)
		return;
	if (++World[Side].Ticks == FailAt) {
		Logf("throw;");
		ThrowSideAssertion(Side == IMAGE, -World[Side].Ticks);
	}
}

// The frames: the image's in a buffer of its own, ours by placement new.
alignas(16) uint8 ImageFrame[0x300];
alignas(16) uint8 OurFrame[sizeof(CMainFrame) + 16];
bool Alive;
CMainFrame *Frame() { return (CMainFrame *)OurFrame; }
uint8 *FrameOf(int S) { return S == IMAGE ? ImageFrame : OurFrame; }
HWND FAKE_WINDOW = (HWND)(uintptr_t)0x10ab0;
uint8 ImageApp[0x200];
alignas(16) uint8 OurApp[sizeof(CPlayerApp) + 16];
#if GOLDEN_ORACLE
// The image's AFX_MODULE_STATE as the inline AfxGetApp and AfxGetInstanceHandle read it.
struct TModuleState {
	uint32 Unused;
	void *App;          // +0x04 m_pCurrentWinApp
	HINSTANCE Instance; // +0x08 m_hCurrentInstanceHandle
	uint8 Rest[0x40];
};
TModuleState ModuleState;
#endif
const HINSTANCE FAKE_INSTANCE = (HINSTANCE)(uintptr_t)0x770000;

const char *Who(const void *Window)
{
	if (Window == NULL)
		return "NULL";
	if (Window == (const void *)FrameOf(Side))
		return "frame";
	if (Side == IMAGE ? (uintptr_t)Window == 0x720920 : Window == &CWnd::wndTopMost)
		return "topmost";
	if (Side == IMAGE ? (uintptr_t)Window == 0x720970 : Window == &CWnd::wndNoTopMost)
		return "notopmost";
	return "other";
}
const char *HwndName(HWND Window) { return Window == FAKE_WINDOW ? "hwnd" : Window == NULL ? "NULL"
																						   : "other"; }
// The callbacks the frame hands out, by name.
const char *CallbackName(const void *Function)
{
	struct {
		uint32 Image;
		const void *Ours;
		const char *Name;
	} Names[] = {
		{0x4a88f0, (const void *)ExitProgram, "ExitProgram"},
		{0x4a8940, (const void *)CloseProgram, "CloseProgram"},
		{0x4a8af0, (const void *)SetDisplayMode, "SetDisplayMode"},
		{0x4a8d80, (const void *)OpenURL, "OpenURL"},
		{0x4aa1d0, (const void *)UpdateMouseCursor, "UpdateMouseCursor"},
	};
	for (auto &N : Names)
		if (Side == IMAGE ? (uintptr_t)Function == N.Image : Function == N.Ours)
			return N.Name;
	return Function == NULL ? "NULL" : "other";
}

// --- The fakes: user32 and shell32 -----------------------------------------------------------

// The Windows functions never throw in the error round: they are C
// functions, which /EHsc (the original's build and Patella.exe's) assumes
// cannot throw, so the compiler drops a try block that calls nothing else
// (AddFrameSize's and ExitProgram's TENDs have no handlers in the binary);
// the difftest builds with /EHa, which keeps ours. Every TEND is still
// reached through the MFC and game fakes.
void Win32Tick() {}

BOOL WINAPI FakePostMessageA(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
	Logf("Post(%s,%x,%x,%x);", HwndName(Window), Message, (unsigned)wParam, (unsigned)lParam);
	Win32Tick();
	return TRUE;
}
BOOL WINAPI FakeGetClientRect(HWND Window, RECT *Rect)
{
	Logf("GetClientRect(%s);", HwndName(Window));
	Win32Tick();
	*Rect = Script.Client;
	return TRUE;
}
BOOL WINAPI FakeGetWindowRect(HWND Window, RECT *Rect)
{
	Logf("GetWindowRect(%s);", HwndName(Window));
	Win32Tick();
	*Rect = Script.Window;
	return TRUE;
}
BOOL WINAPI FakeIsIconic(HWND Window)
{
	Logf("IsIconic(%s);", HwndName(Window));
	Win32Tick();
	return Script.Iconic;
}
BOOL WINAPI FakeIsZoomed(HWND Window)
{
	Logf("IsZoomed(%s);", HwndName(Window));
	Win32Tick();
	return Script.Zoomed;
}
BOOL WINAPI FakeKillTimer(HWND Window, UINT_PTR Id)
{
	Logf("KillTimer(%s,%u);", HwndName(Window), (unsigned)Id);
	Win32Tick();
	return TRUE;
}
UINT_PTR WINAPI FakeSetTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function)
{
	// The callback by name (a pointer printed with %p has another width on x64; issue #54).
	Logf("SetTimer(%s,%u,%u,%s);", HwndName(Window), (unsigned)Id, Elapse, CallbackName((const void *)Function));
	Win32Tick();
	return Id;
}
HCURSOR WINAPI FakeSetCursor(HCURSOR Cursor)
{
	Logf("SetCursor(%x);", (unsigned)(uintptr_t)Cursor);
	Win32Tick();
	return NULL;
}
BOOL WINAPI FakeGetCursorPos(POINT *Point)
{
	Logf("GetCursorPos;");
	Win32Tick();
	*Point = Script.Cursor;
	return TRUE;
}
BOOL WINAPI FakePtInRect(const RECT *Rect, POINT Point)
{
	Logf("PtInRect(%d,%d,%d,%d,%d,%d);", (int)Rect->left, (int)Rect->top, (int)Rect->right, (int)Rect->bottom,
		(int)Point.x, (int)Point.y);
	Win32Tick();
	return Point.x >= Rect->left && Point.x < Rect->right && Point.y >= Rect->top && Point.y < Rect->bottom;
}
BOOL WINAPI FakeClientToScreen(HWND Window, POINT *Point)
{
	Logf("ClientToScreen(%s,%d,%d);", HwndName(Window), (int)Point->x, (int)Point->y);
	Win32Tick();
	Point->x += Script.Origin.x;
	Point->y += Script.Origin.y;
	return TRUE;
}
BOOL WINAPI FakeScreenToClient(HWND Window, POINT *Point)
{
	Logf("ScreenToClient(%s,%d,%d);", HwndName(Window), (int)Point->x, (int)Point->y);
	Win32Tick();
	Point->x -= Script.Origin.x;
	Point->y -= Script.Origin.y;
	return TRUE;
}
HWND WINAPI FakeSetCapture(HWND Window)
{
	Logf("SetCapture(%s);", HwndName(Window));
	Win32Tick();
	return (HWND)(uintptr_t)0x20cd0;
}
BOOL WINAPI FakeReleaseCapture()
{
	Logf("ReleaseCapture;");
	Win32Tick();
	return TRUE;
}
int WINAPI FakeGetSystemMetrics(int Index)
{
	Logf("Metrics(%d);", Index);
	Win32Tick();
	return Index == SM_CXSCREEN ? Script.ScreenWidth : Index == SM_CYSCREEN ? Script.ScreenHeight
																			: 7;
}
HCURSOR WINAPI FakeLoadCursorA(HINSTANCE Instance, LPCSTR Name)
{
	Logf("LoadCursor(%x,%x);", (unsigned)(uintptr_t)Instance, (unsigned)(uintptr_t)Name);
	Win32Tick();
	return (HCURSOR)(uintptr_t)((Instance == NULL ? 0x31000000 : 0x32000000) + ((uintptr_t)Name & 0xffff));
}
BOOL WINAPI FakeDestroyCursor(HCURSOR Cursor)
{
	Logf("DestroyCursor(%x);", (unsigned)(uintptr_t)Cursor);
	Win32Tick();
	return TRUE;
}
HINSTANCE WINAPI FakeShellExecuteA(HWND Window, LPCSTR Operation, LPCSTR File, LPCSTR Parameters, LPCSTR Directory,
	INT Show)
{
	Logf("ShellExecute(%s,%s,%s,%s,%s,%d);", HwndName(Window), Operation == NULL ? "NULL" : Operation,
		File == NULL ? "NULL" : File, Parameters == NULL ? "NULL" : Parameters, Directory == NULL ? "NULL" : Directory,
		Show);
	Win32Tick();
	return (HINSTANCE)(uintptr_t)42;
}

// --- The fakes: MFC ------------------------------------------------------------------------------

HWND &WindowOf(void *Object) { return Side == IMAGE ? *(HWND *)((uint8 *)Object + 0x1c) : ((CWnd *)Object)->m_hWnd; }

void FrameConstructed(void *This)
{
	Logf("CFrameWnd(%s);", Who(This));
	WindowOf(This) = NULL;
}
void FrameDestroyed(void *This) { Logf("~CFrameWnd(%s);", Who(This)); }
BOOL GetPlacement(void *This, WINDOWPLACEMENT *Placement)
{
	bool Own = Side == IMAGE ? (uint8 *)Placement == (uint8 *)This + 0xd4
							 : Placement == &((CMainFrame *)This)->Placement;
	Logf("GetWindowPlacement(%s,%s);", Who(This), Own ? "Placement" : "other");
	Tick();
	*Placement = Script.Placement;
	return TRUE;
}
BOOL SetPlacement(void *This, const WINDOWPLACEMENT *Placement)
{
	bool Own = Side == IMAGE ? (const uint8 *)Placement == (uint8 *)This + 0xd4
							 : Placement == &((CMainFrame *)This)->Placement;
	Logf("SetWindowPlacement(%s,%s,", Who(This), Own ? "Placement" : "other");
	for (size_t i = 0; i < sizeof(WINDOWPLACEMENT); i++)
		Logf("%02x", ((const uint8 *)Placement)[i]);
	Logf(");");
	Tick();
	return TRUE;
}
BOOL ModifyStyleFake(void *This, DWORD Remove, DWORD Add, UINT Flags, bool Ex)
{
	Logf("ModifyStyle%s(%s,%lx,%lx,%u);", Ex ? "Ex" : "", Who(This), Remove, Add, Flags);
	Tick();
	return TRUE;
}
BOOL SetPosFake(void *This, const void *After, int x, int y, int cx, int cy, UINT Flags)
{
	Logf("SetWindowPos(%s,%s,%d,%d,%d,%d,%x);", Who(This), Who(After), x, y, cx, cy, Flags);
	Tick();
	return TRUE;
}
void MoveFake(void *This, int x, int y, int Width, int Height, BOOL Repaint)
{
	Logf("MoveWindow(%s,%d,%d,%d,%d,%d);", Who(This), x, y, Width, Height, Repaint);
	Tick();
}
LRESULT DefaultFake(void *This)
{
	Logf("Default(%s);", Who(This));
	Tick();
	return Script.Default;
}
void BaseOnSize(void *This, UINT Type, int cx, int cy)
{
	Logf("CFrameWnd::OnSize(%s,%u,%d,%d);", Who(This), Type, cx, cy);
	Tick();
}
void BaseOnClose(void *This)
{
	Logf("CFrameWnd::OnClose(%s);", Who(This));
	Tick();
}
void BaseOnDestroy(void *This)
{
	Logf("CFrameWnd::OnDestroy(%s);", Who(This));
	Tick();
}
BOOL BaseDestroyWindow(void *This)
{
	Logf("CWnd::DestroyWindow(%s);", Who(This));
	Tick();
	return Script.DestroyWindow;
}
BOOL BaseOnSetCursor(void *This, void *Window, UINT HitTest, UINT Message)
{
	Logf("CFrameWnd::OnSetCursor(%s,%s,%u,%x);", Who(This), Who(Window), HitTest, Message);
	Tick();
	return Script.OnSetCursor;
}
BOOL BasePreCreateWindow(void *This, CREATESTRUCT &cs)
{
	Logf("CFrameWnd::PreCreateWindow(%s,%s,%s);", Who(This), cs.lpszClass == NULL ? "NULL" : cs.lpszClass,
		cs.lpszName == NULL ? "NULL" : cs.lpszName);
	Tick();
	return Script.PreCreate;
}
int BaseOnCreate(void *This, LPCREATESTRUCT Create)
{
	Logf("CFrameWnd::OnCreate(%s,%s);", Who(This), Create != NULL ? "cs" : "NULL");
	Tick();
	return Script.OnCreate;
}
void *FromHandleFake(HWND Window)
{
	Logf("FromHandle(%x);", (unsigned)(uintptr_t)Window);
	Tick();
	return NULL;
}
void ModuleStateUsed()
{
	Logf("ModuleState;");
	Tick();
}

// The image's MFC (the oracle).
#if GOLDEN_ORACLE
void *__fastcall ImageFrameCtor(void *This, void *)
{
	FrameConstructed(This);
	return This;
}
void __fastcall ImageFrameDtor(void *This, void *) { FrameDestroyed(This); }
BOOL __fastcall ImageGetPlacement(void *This, void *, WINDOWPLACEMENT *P) { return GetPlacement(This, P); }
BOOL __fastcall ImageSetPlacement(void *This, void *, const WINDOWPLACEMENT *P) { return SetPlacement(This, P); }
BOOL __fastcall ImageModifyStyle(void *This, void *, DWORD Remove, DWORD Add, UINT Flags)
{
	return ModifyStyleFake(This, Remove, Add, Flags, false);
}
BOOL __fastcall ImageModifyStyleEx(void *This, void *, DWORD Remove, DWORD Add, UINT Flags)
{
	return ModifyStyleFake(This, Remove, Add, Flags, true);
}
BOOL __fastcall ImageSetWindowPos(void *This, void *, const void *After, int x, int y, int cx, int cy, UINT Flags)
{
	return SetPosFake(This, After, x, y, cx, cy, Flags);
}
void __fastcall ImageMoveWindow(void *This, void *, int x, int y, int Width, int Height, BOOL Repaint)
{
	MoveFake(This, x, y, Width, Height, Repaint);
}
LRESULT __fastcall ImageDefault(void *This, void *) { return DefaultFake(This); }
void __fastcall ImageOnSize(void *This, void *, UINT Type, int cx, int cy) { BaseOnSize(This, Type, cx, cy); }
void __fastcall ImageOnClose(void *This, void *) { BaseOnClose(This); }
void __fastcall ImageOnDestroy(void *This, void *) { BaseOnDestroy(This); }
BOOL __fastcall ImageDestroyWindow(void *This, void *) { return BaseDestroyWindow(This); }
BOOL __fastcall ImageOnSetCursor(void *This, void *, void *Window, UINT HitTest, UINT Message)
{
	return BaseOnSetCursor(This, Window, HitTest, Message);
}
BOOL __fastcall ImagePreCreateWindow(void *This, void *, CREATESTRUCT &cs) { return BasePreCreateWindow(This, cs); }
int __fastcall ImageOnCreate(void *This, void *, LPCREATESTRUCT Create) { return BaseOnCreate(This, Create); }
void *__stdcall ImageFromHandle(HWND Window) { return FromHandleFake(Window); }
void *__cdecl ImageModuleState()
{
	ModuleStateUsed();
	ModuleState.App = Script.HasApp ? ImageApp : NULL;
	ModuleState.Instance = FAKE_INSTANCE;
	return &ModuleState;
}
#endif

// --- The fakes: the game files ---------------------------------------------------------------

void __cdecl FakeLeftButtonDown(int x, int y, int Flags)
{
	Logf("LDown(%d,%d,%x);", x, y, Flags);
	Tick();
}
void __cdecl FakeLeftButtonUp(int x, int y, int Flags)
{
	Logf("LUp(%d,%d,%x);", x, y, Flags);
	Tick();
}
void __cdecl FakeLeftButtonDoubleClick(int x, int y, int Flags)
{
	Logf("LDbl(%d,%d,%x);", x, y, Flags);
	Tick();
}
void __cdecl FakeRightButtonDown(int x, int y, int Flags)
{
	Logf("RDown(%d,%d,%x);", x, y, Flags);
	Tick();
}
void __cdecl FakeRightButtonUp(int x, int y, int Flags)
{
	Logf("RUp(%d,%d,%x);", x, y, Flags);
	Tick();
}
void __cdecl FakeMouseWheel(int x, int y, int Direction)
{
	Logf("Wheel(%d,%d,%d);", x, y, Direction);
	Tick();
}
void __cdecl FakeKeyPress(int Key)
{
	Logf("Key(%x);", Key);
	Tick();
}
void __cdecl FakeTimerEvent()
{
	Logf("TimerEvent;");
	Tick();
}
bool __cdecl FakeCloseRequest()
{
	Logf("CloseRequest;");
	Tick();
	return Script.CloseRequest != FALSE;
}
void __cdecl FakeResize(int Width, int Height)
{
	Logf("Resize(%d,%d);", Width, Height);
	Tick();
}
void __cdecl FakeRedraw(bool Force)
{
	Logf("Redraw(%d);", Force ? 1 : 0);
	Tick();
}
void __cdecl FakeInitControl(bool Gamemaster)
{
	Logf("InitControl(%d);", Gamemaster ? 1 : 0);
	Tick();
}
void __cdecl FakeExitControl()
{
	Logf("ExitControl;");
	Tick();
}
void __cdecl FakeInitWindow(HWND Window, void (*A)(), void (*B)(), void (*C)(int), void (*D)(const char *))
{
	Logf("InitWindow(%s,%s,%s,%s,%s);", HwndName(Window), CallbackName((void *)A), CallbackName((void *)B),
		CallbackName((void *)C), CallbackName((void *)D));
	Tick();
}
void __cdecl FakeInitMainWindow(int Width, int Height, void (*A)(), void (*B)(int), void (*C)(const char *))
{
	Logf("InitMainWindow(%d,%d,%s,%s,%s);", Width, Height, CallbackName((void *)A), CallbackName((void *)B),
		CallbackName((void *)C));
	Tick();
}
void __cdecl FakeSetWindowedMode(int x, int y, int Width, int Height, bool Maximized)
{
	Logf("SetWindowedMode(%d,%d,%d,%d,%d);", x, y, Width, Height, Maximized ? 1 : 0);
	Tick();
	TSideWorld &W = World[Side];
	W.WindowedX = x;
	W.WindowedY = y;
	W.WindowedWidth = Width;
	W.WindowedHeight = Height;
	W.WindowedMaximized = Maximized;
}
void __cdecl FakeGetWindowedMode(int *x, int *y, int *Width, int *Height, bool *Maximized)
{
	Logf("GetWindowedMode;");
	Tick();
	TSideWorld &W = World[Side];
	*x = W.WindowedX;
	*y = W.WindowedY;
	*Width = W.WindowedWidth;
	*Height = W.WindowedHeight;
	*Maximized = W.WindowedMaximized;
}
void __cdecl FakeSetViewMode(int Mode)
{
	Logf("SetViewMode(%d);", Mode);
	Tick();
	World[Side].ViewMode = Mode;
}
int __cdecl FakeGetViewMode()
{
	Tick();
	return World[Side].ViewMode;
}
bool __cdecl FakeGetWindowsMousepointer()
{
	Tick();
	return World[Side].WindowsMousepointer;
}
int __cdecl FakeGetMouseCursor()
{
	Tick();
	return World[Side].MouseCursor;
}
int __cdecl FakeGetMouseX()
{
	Tick();
	return World[Side].MouseX;
}
int __cdecl FakeGetMouseY()
{
	Tick();
	return World[Side].MouseY;
}
void __cdecl FakeSetConnectionState(int State)
{
	Logf("Conn%d;", State);
	Tick();
	World[Side].ConnectionState = State;
}
int __cdecl FakeGetConnectionState()
{
	Tick();
	return World[Side].ConnectionState;
}
void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	if (Type == 1)
		Logf("Msg1(%s|%s);", (const char *)Param1, (const char *)Param2);
	else
		Logf("Msg%d(%d,%d);", Type, (int)Param1, (int)Param2);
	if (Type == 1 && Param2 != 0) {
		if (Side == IMAGE)
			free((void *)Param2);
		else
			delete[] (char *)Param2;
	}
	Tick();
}
void __cdecl FakeSetConnectionTimeout(int Timeout)
{
	Logf("SetConnectionTimeout(%d);", Timeout);
	Tick();
}
void __cdecl FakeSetServerError(int Code)
{
	Logf("SetServerError(%d);", Code);
	Tick();
}
void __cdecl FakeConnectToLoginServer()
{
	Logf("ConnectToLoginServer;");
	Tick();
}
const char *__cdecl FakeGetResolvedIPAddress()
{
	Logf("GetResolvedIPAddress;");
	Tick();
	return Script.ResolvedIP;
}
void __cdecl FakeSetServerIPAddress(const char *Address)
{
	Logf("SetServerIPAddress(%s);", Address);
	Tick();
}
void __cdecl FakeDisconnect(bool Force)
{
	Logf("Disconnect(%d);", Force ? 1 : 0);
	Tick();
}
void __cdecl FakeGetErrorMessage(char *Message, int Code, int Type)
{
	Logf("GetErrorMessage(%d,%d);", Code, Type);
	Tick();
	sprintf(Message, "error %d type %d", Code, Type);
}


} // namespace

// Ours: the shim's MFC declarations, defined as the fakes (they were
// playertest.cpp's stubs).
CWinApp *AfxGetApp()
{
	ModuleStateUsed();
	return Script.HasApp ? (CWinApp *)OurApp : NULL;
}
HINSTANCE AfxGetInstanceHandle()
{
	ModuleStateUsed();
	return FAKE_INSTANCE;
}
int CWnd::OnCreate(LPCREATESTRUCT Create) { return BaseOnCreate(this, Create); }
BOOL CWnd::OnSetCursor(CWnd *Window, UINT HitTest, UINT Message) { return BaseOnSetCursor(this, Window, HitTest, Message); }
void CWnd::OnClose() { BaseOnClose(this); }
void CWnd::OnDestroy() { BaseOnDestroy(this); }
void CWnd::OnSize(UINT Type, int cx, int cy) { BaseOnSize(this, Type, cx, cy); }
CFrameWnd::CFrameWnd() { FrameConstructed(this); }
CFrameWnd::~CFrameWnd() { FrameDestroyed(this); }
BOOL CWnd::GetWindowPlacement(WINDOWPLACEMENT *Placement) const { return GetPlacement((void *)this, Placement); }
BOOL CWnd::SetWindowPlacement(const WINDOWPLACEMENT *Placement) { return SetPlacement(this, Placement); }
BOOL CWnd::ModifyStyle(DWORD Remove, DWORD Add, UINT Flags) { return ModifyStyleFake(this, Remove, Add, Flags, false); }
BOOL CWnd::ModifyStyleEx(DWORD Remove, DWORD Add, UINT Flags) { return ModifyStyleFake(this, Remove, Add, Flags, true); }
BOOL CWnd::SetWindowPos(const CWnd *After, int x, int y, int cx, int cy, UINT Flags)
{
	return SetPosFake(this, After, x, y, cx, cy, Flags);
}
LRESULT CWnd::Default() { return DefaultFake(this); }
CWnd *PASCAL CWnd::FromHandle(HWND Window) { return (CWnd *)FromHandleFake(Window); }
const CWnd CWnd::wndTopMost = CWnd();
const CWnd CWnd::wndNoTopMost = CWnd();
BOOL CWnd::DestroyWindow() { return BaseDestroyWindow(this); }
BOOL CWnd::PreCreateWindow(CREATESTRUCT &cs) { return BasePreCreateWindow(this, cs); }
void CWnd::MoveWindow(int x, int y, int Width, int Height, BOOL Repaint) { MoveFake(this, x, y, Width, Height, Repaint); }

namespace {

// The handlers are protected: a subclass makes them reachable.
class TFrameAccess : public CMainFrame {
public:
	using CMainFrame::OnActivateApp;
	using CMainFrame::OnCaptureChanged;
	using CMainFrame::OnChar;
	using CMainFrame::OnClose;
	using CMainFrame::OnCreate;
	using CMainFrame::OnDestroy;
	using CMainFrame::OnDisplayChange;
	using CMainFrame::OnGetMinMaxInfo;
	using CMainFrame::OnHostResolved;
	using CMainFrame::OnKeyDown;
	using CMainFrame::OnKeyUp;
	using CMainFrame::OnLButtonDblClk;
	using CMainFrame::OnLButtonDown;
	using CMainFrame::OnLButtonUp;
	using CMainFrame::OnMouseWheel;
	using CMainFrame::OnNcHitTest;
	using CMainFrame::OnRButtonDblClk;
	using CMainFrame::OnRButtonDown;
	using CMainFrame::OnRButtonUp;
	using CMainFrame::OnSetCursor;
	using CMainFrame::OnSize;
	using CMainFrame::OnSysKeyDown;
	using CMainFrame::OnSysKeyUp;
	using CMainFrame::OnTimer;
};
TFrameAccess *Ours() { return (TFrameAccess *)OurFrame; }

// The image's members, thiscall through __fastcall. The runner has no image: its side never runs there.
#if GOLDEN_ORACLE
template <class R, class... A> R ImageMember(uint32 Address, A... Arguments)
{
	return ((R(__fastcall *)(void *, void *, A...))(uintptr_t)Address)(ImageFrame, NULL, Arguments...);
}
template <class R, class... A> R ImageFree(uint32 Address, A... Arguments)
{
	return ((R(__cdecl *)(A...))(uintptr_t)Address)(Arguments...);
}
#else
[[noreturn]] void NoImage()
{
	printf("golden: MainFrm called the original in the runner\n");
	abort();
}
template <class R, class... A> R ImageMember(uint32, A...) { NoImage(); }
template <class R, class... A> R ImageFree(uint32, A...) { NoImage(); }
#endif

// --- Comparison ------------------------------------------------------------------------------

std::string Hex(const void *Data, int Size)
{
	std::string Text;
	char Buffer[4];
	for (int i = 0; i < Size; i++) {
		snprintf(Buffer, sizeof(Buffer), "%02x", ((const uint8 *)Data)[i]);
		Text += Buffer;
	}
	return Text;
}

struct TStatics {
	uint8 *PacketHeader;
	void **MainFrame;
	HCURSOR *Hand, *Size, *Wait, *Arrow, *Default;
	bool *LeftButtonPressed, *RightButtonPressed, *Closing, *DisplayChanged;
	char *ClassName;
	int *WheelDelta;
};
TStatics Statics[2];
#if !GOLDEN_ORACLE
// The runner: the test sets the original's statics as well (the inputs both sides get); nothing reads them.
struct {
	uint8 PacketHeader[16];
	void *MainFrame;
	HCURSOR Hand, Size, Wait, Arrow, Default;
	bool LeftButtonPressed, RightButtonPressed, Closing, DisplayChanged;
	char ClassName[32];
	int WheelDelta;
} ImageStatics;
#endif
void MakeStatics()
{
	TStatics &I = Statics[IMAGE];
#if GOLDEN_ORACLE
	I.PacketHeader = (uint8 *)0x5d1644;
	I.MainFrame = (void **)0x5d1654;
	I.Hand = (HCURSOR *)0x5d1658;
	I.Size = (HCURSOR *)0x5d165c;
	I.Wait = (HCURSOR *)0x5d1660;
	I.Arrow = (HCURSOR *)0x5d1664;
	I.Default = (HCURSOR *)0x5d1668;
	I.LeftButtonPressed = (bool *)0x5d166c;
	I.RightButtonPressed = (bool *)0x5d166d;
	I.Closing = (bool *)0x5d166f;
	I.DisplayChanged = (bool *)0x5d1670;
	I.ClassName = (char *)0x5d167c;
	I.WheelDelta = (int *)0x5d169c;
#else
	I.PacketHeader = ImageStatics.PacketHeader;
	I.MainFrame = &ImageStatics.MainFrame;
	I.Hand = &ImageStatics.Hand;
	I.Size = &ImageStatics.Size;
	I.Wait = &ImageStatics.Wait;
	I.Arrow = &ImageStatics.Arrow;
	I.Default = &ImageStatics.Default;
	I.LeftButtonPressed = &ImageStatics.LeftButtonPressed;
	I.RightButtonPressed = &ImageStatics.RightButtonPressed;
	I.Closing = &ImageStatics.Closing;
	I.DisplayChanged = &ImageStatics.DisplayChanged;
	I.ClassName = ImageStatics.ClassName;
	I.WheelDelta = &ImageStatics.WheelDelta;
#endif
	TStatics &O = Statics[OURS];
	O.PacketHeader = PacketHeader.data();
	O.MainFrame = (void **)&MainFrame;
	O.Hand = &HandCursor;
	O.Size = &SizeCursor;
	O.Wait = &WaitCursor;
	O.Arrow = &ArrowCursor;
	O.Default = &DefaultCursor;
	O.LeftButtonPressed = &LeftButtonPressed;
	O.RightButtonPressed = &RightButtonPressed;
	O.Closing = &Closing;
	O.DisplayChanged = &DisplayChanged;
	O.ClassName = ClassName;
	O.WheelDelta = &WheelDelta;
}

// One side's state after an operation, by value (the corpus records the original's; the same layout
// on both targets, no padding): the frame's members, the file's statics and the world its fakes keep.
struct TState {
	uint32 Window;
	int32_t MouseCursor;
	int32_t KeyFlags;
	int32_t MainFrame; // 0 NULL, 1 the side's frame, 2 another
	uint32 Cursors[5]; // Hand, Size, Wait, Arrow, Default
	int32_t WheelDelta;
	int32_t ViewMode, WindowedX, WindowedY, WindowedWidth, WindowedHeight, ConnectionState;
	uint8 Placement[sizeof(WINDOWPLACEMENT)];
	uint8 KeyStates[0x100];
	uint8 PacketHeader[16];
	uint8 LeftButtonPressed, RightButtonPressed, Closing, DisplayChanged, WindowedMaximized, RunPatch, Unused[2];
	char ClassName[32];
};
static_assert(sizeof(WINDOWPLACEMENT) == 44 && sizeof(TState) == 420, "TState: the same layout on both targets");

TState Capture(int S)
{
	TState State;
	memset(&State, 0, sizeof(State));
	TStatics &T = Statics[S];
	uint8 *F = FrameOf(S);
	if (S == IMAGE) {
		State.Window = (uint32)(uintptr_t)*(HWND *)(F + 0x1c);
		State.MouseCursor = *(int *)(F + 0xd0);
		memcpy(State.Placement, F + 0xd4, sizeof(State.Placement));
		memcpy(State.KeyStates, F + 0x100, sizeof(State.KeyStates));
		State.KeyFlags = *(int *)(F + 0x200);
		State.RunPatch = ImageApp[0xa1] != 0;
	} else {
		CMainFrame *M = (CMainFrame *)F;
		State.Window = (uint32)(uintptr_t)M->m_hWnd;
		State.MouseCursor = M->MouseCursor;
		memcpy(State.Placement, &M->Placement, sizeof(State.Placement));
		memcpy(State.KeyStates, M->KeyStates.data(), sizeof(State.KeyStates));
		State.KeyFlags = M->KeyFlags;
		State.RunPatch = ((CPlayerApp *)OurApp)->RunPatch;
	}
	memcpy(State.PacketHeader, T.PacketHeader, sizeof(State.PacketHeader));
	State.MainFrame = *T.MainFrame == (void *)F ? 1 : *T.MainFrame == NULL ? 0
																		   : 2;
	HCURSOR *Cursors[5] = {T.Hand, T.Size, T.Wait, T.Arrow, T.Default};
	for (int i = 0; i < 5; i++)
		State.Cursors[i] = (uint32)(uintptr_t)*Cursors[i];
	// The flags as the bytes the bools hold (the dump printed them promoted to int).
	State.LeftButtonPressed = *(const uint8 *)T.LeftButtonPressed;
	State.RightButtonPressed = *(const uint8 *)T.RightButtonPressed;
	State.Closing = *(const uint8 *)T.Closing;
	State.DisplayChanged = *(const uint8 *)T.DisplayChanged;
	memcpy(State.ClassName, T.ClassName, sizeof(State.ClassName));
	State.WheelDelta = *T.WheelDelta;
	TSideWorld &W = World[S];
	State.ViewMode = W.ViewMode;
	State.WindowedX = W.WindowedX;
	State.WindowedY = W.WindowedY;
	State.WindowedWidth = W.WindowedWidth;
	State.WindowedHeight = W.WindowedHeight;
	State.WindowedMaximized = W.WindowedMaximized;
	State.ConnectionState = W.ConnectionState;
	return State;
}

std::string Dump(const TState &State)
{
	char Buffer[1024];
	snprintf(Buffer, sizeof(Buffer),
		"frame %s hwnd %x cursor %d keys %x placement %s keystates %s\n"
		"PacketHeader %s MainFrame %s cursors %x %x %x %x %x buttons %d %d Closing %d DisplayChanged %d ClassName %.32s "
		"WheelDelta %d RunPatch %d\n",
		Alive ? "alive" : "dead", State.Window, State.MouseCursor, State.KeyFlags,
		Hex(State.Placement, sizeof(State.Placement)).c_str(), Hex(State.KeyStates, sizeof(State.KeyStates)).c_str(),
		Hex(State.PacketHeader, 16).c_str(), State.MainFrame == 1 ? "frame" : State.MainFrame == 0 ? "NULL"
																								   : "other",
		State.Cursors[0], State.Cursors[1], State.Cursors[2], State.Cursors[3], State.Cursors[4],
		State.LeftButtonPressed, State.RightButtonPressed, State.Closing, State.DisplayChanged, State.ClassName,
		State.WheelDelta, State.RunPatch);
	std::string Text = Buffer;
	snprintf(Buffer, sizeof(Buffer), "world: view %d windowed %d,%d,%d,%d,%d connection %d\n", State.ViewMode,
		State.WindowedX, State.WindowedY, State.WindowedWidth, State.WindowedHeight, State.WindowedMaximized,
		State.ConnectionState);
	return Text + Buffer;
}

// The original's state as last recorded (golden::OutputRuns records its changes).
TState LastOriginal;

void Compare(const char *Operation)
{
	Checks++;
	// The runner gets the original's log from the corpus.
	Log[IMAGE] = golden::OutputText("log", Log[IMAGE].c_str());
	if (Log[IMAGE] != Log[OURS]) {
		Failures++;
		if (Failures <= FailuresPrinted)
			printf("FAIL MainFrm %s (%s): log\n%s\noriginal\n%s\n", Operation, What, Log[OURS].c_str(), Log[IMAGE].c_str());
	}
	if (LogFile != NULL)
		fprintf(LogFile, "%s: %s\n", Operation, Log[OURS].c_str());
	Log[IMAGE].clear();
	Log[OURS].clear();
#if GOLDEN_ORACLE
	TState Original = Capture(IMAGE);
	golden::OutputRuns("state", &LastOriginal, &Original, sizeof(TState));
#else
	golden::OutputRuns("state", &LastOriginal, nullptr, sizeof(TState));
#endif
	std::string A = Dump(Capture(OURS)), B = Dump(LastOriginal);
	CHECK(A == B, "MainFrm %s (%s): state\n%s\noriginal\n%s", Operation, What, A.c_str(), B.c_str());
}

TErrorRound *Round;

template <class F> void Both(const char *Name, F Call)
{
	What = Name;
	World[0].Ticks = World[1].Ticks = 0;
	if (Trace)
		printf("MainFrm: %s\n", Name);
	if (Round == NULL) {
#if GOLDEN_ORACLE
		Side = IMAGE;
		Call(IMAGE);
#endif
		Side = OURS;
		Call(OURS);
	} else {
		Round->Calling(Name, "ours");
		Side = OURS;
		TThrown O = OurThrow([&] { Call(OURS); });
		Round->Calling(Name, "the original");
		TThrown I = ORIGINAL_THROWN("thrown", Side = IMAGE; Call(IMAGE));
		Round->Compare(Name, O, I);
		// How many fake calls the original made (whether its n-th was reached).
		golden::Output("original ticks", World[IMAGE].Ticks);
	}
	Compare(Name);
}

bool Chance(int Percent) { return (int)(Random() % 100) < Percent; }
int Between(int Low, int High) { return Low + (int)(Random() % (uint32)(High - Low + 1)); }

void RandomScript()
{
	TScript &S = Script;
	S.Default = Chance(50) ? 0 : Between(-5, 20);
	S.CloseRequest = Chance(50);
	S.OnCreate = Chance(85) ? 0 : -1;
	S.OnSetCursor = Chance(50);
	S.DestroyWindow = Chance(80);
	S.PreCreate = Chance(80);
	S.Iconic = Chance(15);
	S.Zoomed = Chance(30);
	int Width = Chance(50) ? Between(300, 1300) : Between(540, 560), Height = Chance(50) ? Between(300, 1000) : Between(440, 460);
	S.Client = {0, 0, Width, Height};
	S.Origin = {Between(-50, 900), Between(-50, 700)};
	int Frame = Between(0, 12), Caption = Between(0, 30);
	S.Window = {S.Origin.x - Frame, S.Origin.y - Frame - Caption, S.Origin.x + Width + Frame, S.Origin.y + Height + Frame};
	S.Cursor = Chance(70) ? POINT{S.Origin.x + Between(-20, Width + 20), S.Origin.y + Between(-20, Height + 20)}
						  : POINT{Between(-100, 2000), Between(-100, 1500)};
	S.ScreenWidth = Chance(80) ? 1024 : Between(640, 2560);
	S.ScreenHeight = Chance(80) ? 768 : Between(480, 1600);
	memset(&S.Placement, 0, sizeof(S.Placement));
	S.Placement.length = sizeof(WINDOWPLACEMENT);
	S.Placement.flags = Random() % 4;
	S.Placement.showCmd = Random() % 10;
	S.Placement.ptMinPosition = {Between(-1, 50), Between(-1, 50)};
	S.Placement.ptMaxPosition = {Between(-1, 50), Between(-1, 50)};
	S.Placement.rcNormalPosition = {Between(-100, 500), Between(-100, 500), Between(0, 1600), Between(0, 1200)};
	S.HasApp = Chance(85);
	S.PlayerID = Chance(60) ? Between(1, 100000) : 0;
	S.HasCreature = Chance(80);
	S.CreatureX = Between(30000, 34000);
	S.CreatureY = Between(30000, 34000);
	S.CreatureZ = Between(0, 15);
	snprintf(S.ResolvedIP, sizeof(S.ResolvedIP), "%u.%u.%u.%u", Random() % 256, Random() % 256, Random() % 256,
		Random() % 256);
}

// The world both sides read (Preferences, Status), set alike.
void RandomWorld()
{
	int View = Chance(45) ? 1 : Chance(80) ? 2
										   : Between(-1, 4);
	bool Pointer = Chance(70);
	int Cursor = Between(-1, 8), x = Between(-10, 800), y = Between(-10, 600), Engine = Between(-1, 3);
	int State = Between(0, 6);
	for (int s = 0; s < 2; s++) {
		World[s].ViewMode = View;
		World[s].WindowsMousepointer = Pointer;
		World[s].MouseCursor = Cursor;
		World[s].MouseX = x;
		World[s].MouseY = y;
		World[s].ConnectionState = State;
	}
	curEngine = Engine;
	ORIGINAL(Global<int>(0x71d0c4) = Engine);
}

UINT RandomKey()
{
	if (Chance(10))
		return Chance(50) ? VK_F4 : VK_RETURN;
	static const UINT Keys[] = {VK_SHIFT, VK_CONTROL, VK_MENU, VK_PRIOR, VK_NEXT, VK_END, VK_HOME, VK_LEFT, VK_UP,
		VK_RIGHT, VK_DOWN, VK_INSERT, VK_DELETE, VK_F1, VK_F4, VK_F12, VK_RETURN, 'A', '1', VK_ESCAPE, VK_TAB, VK_SPACE};
	return Chance(85) ? Keys[Random() % (sizeof(Keys) / sizeof(Keys[0]))] : Random() % 0x120;
}
POINT RandomPoint()
{
	return Chance(80) ? POINT{Between(0, 800), Between(0, 600)} : POINT{Between(-40000, 40000), Between(-40000, 40000)};
}

void Construct()
{
	bool Gamemaster = Chance(20);
	memset(ImageFrame, 0xab, sizeof(ImageFrame));
	memset(OurFrame, 0xab, sizeof(OurFrame));
	Alive = true;
	Both("constructor", [&](int s) {
		if (s == IMAGE)
			ImageMember<void *>(0x4a8520, Gamemaster);
		else
			new (OurFrame) CMainFrame(Gamemaster);
		WindowOf(FrameOf(s)) = FAKE_WINDOW; // as Create would set it
	});
}
void Destroy()
{
	NoTick = true; // ours is noexcept
	Both("destructor", [&](int s) {
		if (s == IMAGE)
			ImageMember<void>(0x4a8760);
		else
			Frame()->~CMainFrame();
	});
	NoTick = false;
	Alive = false;
}

enum { OPERATIONS = 40 };

// One operation, the inputs decided from Random() before both sides run.
void Operation(int Kind)
{
	RandomScript();
	switch (Kind) {
	case 0:
	case 1:
	case 2: {
		UINT Key = RandomKey(), Repeat = Between(0, 3), Flags = (Chance(50) ? 0x100 : 0) | (Random() & 0x2ff & ~0x100);
		static const char *Names[] = {"OnKeyDown", "OnSysKeyDown", "KeyDown"};
		Both(Names[Kind], [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(Kind == 2 ? 0x4ab510 : 0x4ab7c0, Key, Repeat, Flags);
			else if (Kind == 0)
				Ours()->OnKeyDown(Key, Repeat, Flags);
			else if (Kind == 1)
				Ours()->OnSysKeyDown(Key, Repeat, Flags);
			else
				Ours()->KeyDown(Key, Repeat, Flags);
		});
		break;
	}
	case 3:
	case 4:
	case 5: {
		UINT Key = RandomKey(), Repeat = Between(0, 3), Flags = (Chance(50) ? 0x100 : 0) | (Random() & 0x2ff & ~0x100);
		static const char *Names[] = {"OnKeyUp", "OnSysKeyUp", "KeyUp"};
		Both(Names[Kind - 3], [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(Kind == 5 ? 0x4ab730 : 0x4ab7d0, Key, Repeat, Flags);
			else if (Kind == 3)
				Ours()->OnKeyUp(Key, Repeat, Flags);
			else if (Kind == 4)
				Ours()->OnSysKeyUp(Key, Repeat, Flags);
			else
				Ours()->KeyUp(Key, Repeat, Flags);
		});
		break;
	}
	case 6: {
		UINT Char = Chance(30) ? Between(0, 27) : Chance(80) ? Between(28, 255)
															 : Between(256, 0x10000);
		UINT Repeat = 1, Flags = Random() & 0xffff;
		Both("OnChar", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4ab7e0, Char, Repeat, Flags);
			else
				Ours()->OnChar(Char, Repeat, Flags);
		});
		break;
	}
	case 7: {
		static const short Deltas[] = {120, -120, 240, -360, 60, -60, 30, 119, -119, 121, 32767, -32768};
		short Delta = Chance(85) ? Deltas[Random() % 12] : (short)Random();
		UINT Flags = Random() % 32;
		POINT Point = RandomPoint();
		Both("OnMouseWheel", [&](int s) {
			BOOL Result = s == IMAGE ? ImageMember<BOOL>(0x4ab310, Flags, (int)Delta, Point)
									 : Ours()->OnMouseWheel(Flags, Delta, *(CPoint *)&Point);
			Logf("=%d;", Result);
		});
		break;
	}
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13: {
		static const uint32 Image[] = {0x4aa5d0, 0x4aa7b0, 0x4aa990, 0x4aab70, 0x4aad50, 0x4aaf30};
		static const char *Names[] = {"OnLButtonDown", "OnLButtonUp", "OnLButtonDblClk", "OnRButtonDown", "OnRButtonUp",
			"OnRButtonDblClk"};
		UINT Flags = Random() % 64;
		POINT Point = RandomPoint();
		int i = Kind - 8;
		Both(Names[i], [&](int s) {
			if (s == IMAGE) {
				ImageMember<void>(Image[i], Flags, Point);
				return;
			}
			CPoint P = *(CPoint *)&Point;
			switch (i) {
			case 0: Ours()->OnLButtonDown(Flags, P); break;
			case 1: Ours()->OnLButtonUp(Flags, P); break;
			case 2: Ours()->OnLButtonDblClk(Flags, P); break;
			case 3: Ours()->OnRButtonDown(Flags, P); break;
			case 4: Ours()->OnRButtonUp(Flags, P); break;
			case 5: Ours()->OnRButtonDblClk(Flags, P); break;
			}
		});
		break;
	}
	case 14: {
		void *Other = Chance(50) ? NULL : (void *)ImageFrame;
		Both("OnCaptureChanged", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4ab110, Other);
			else
				Ours()->OnCaptureChanged((CWnd *)Other);
		});
		break;
	}
	case 15: {
		UINT Id = Chance(80) ? 1 : Random();
		Both("OnTimer", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4a9b00, Id);
			else
				Ours()->OnTimer(Id);
		});
		break;
	}
	case 16:
		Both("OnClose", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4a9c90);
			else
				Ours()->OnClose();
		});
		break;
	case 17:
		Both("DestroyWindow", [&](int s) {
			BOOL Result = s == IMAGE ? ImageMember<BOOL>(0x4a9e30) : Frame()->DestroyWindow();
			Logf("=%d;", Result);
		});
		break;
	case 18:
		Both("OnDestroy", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4a9fd0);
			else
				Ours()->OnDestroy();
		});
		break;
	case 19: {
		UINT Type = Chance(70) ? SIZE_RESTORED : Random() % 5;
		// The minimum 550x450 and its neighbours often.
		int cx = Chance(30) ? Between(548, 552) : Chance(70) ? Between(500, 1300)
															 : Between(-10, 600);
		int cy = Chance(30) ? Between(448, 452) : Chance(70) ? Between(400, 1000)
															 : Between(-10, 500);
		Both("OnSize", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4a9540, Type, cx, cy);
			else
				Ours()->OnSize(Type, cx, cy);
		});
		break;
	}
	case 20:
		Both("OnGetMinMaxInfo", [&](int s) {
			MINMAXINFO Info;
			memset(&Info, 0x5a, sizeof(Info));
			if (s == IMAGE)
				ImageMember<void>(0x4a9790, &Info);
			else
				Ours()->OnGetMinMaxInfo(&Info);
			Logf("=%s;", Hex(&Info, sizeof(Info)).c_str());
		});
		break;
	case 21: {
		POINT Point = RandomPoint();
		Both("OnNcHitTest", [&](int s) {
			LRESULT Result = s == IMAGE ? ImageMember<LRESULT>(0x4a9950, Point) : Ours()->OnNcHitTest(*(CPoint *)&Point);
			Logf("=%d;", (int)Result);
		});
		break;
	}
	case 22: {
		void *Window = Chance(50) ? NULL : (void *)FrameOf(OURS);
		UINT HitTest = Chance(70) ? HTCLIENT : Random() % 25, Message = Chance(70) ? WM_MOUSEMOVE : Random() % 0x300;
		Both("OnSetCursor", [&](int s) {
			void *W = Window == NULL ? NULL : FrameOf(s);
			BOOL Result = s == IMAGE ? ImageMember<BOOL>(0x4aa3c0, W, HitTest, Message)
									 : Ours()->OnSetCursor((CWnd *)W, HitTest, Message);
			Logf("=%d;", Result);
		});
		break;
	}
	case 23:
		Both("UpdateMouseCursor", [&](int s) {
			if (s == IMAGE)
				ImageFree<void>(0x4aa1d0);
			else
				UpdateMouseCursor();
		});
		break;
	case 24: {
		int Width = Between(-100, 1600), Height = Between(-100, 1200);
		Both("AddFrameSize", [&](int s) {
			int w = Width, h = Height;
			if (s == IMAGE)
				ImageMember<void>(0x4a94b0, &w, &h);
			else
				Frame()->AddFrameSize(&w, &h);
			Logf("=%d,%d;", w, h);
		});
		break;
	}
	case 25:
		Both("SetDefaultWindowPosition", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4ab9d0);
			else
				Frame()->SetDefaultWindowPosition();
		});
		break;
	case 26: {
		// OnCreate reads the windowed mode Tibia.cfg gave: set it alike.
		int x = Chance(70) ? Between(0, 400) : Between(-50, 2000), y = Chance(70) ? Between(0, 300) : Between(-50, 1500);
		int w = Between(100, 1100), h = Between(100, 900);
		bool Max = Chance(30);
		for (int s = 0; s < 2; s++) {
			World[s].WindowedX = x;
			World[s].WindowedY = y;
			World[s].WindowedWidth = w;
			World[s].WindowedHeight = h;
			World[s].WindowedMaximized = Max;
		}
		Both("OnCreate", [&](int s) {
			CREATESTRUCT cs;
			memset(&cs, 0, sizeof(cs));
			int Result = s == IMAGE ? ImageMember<int>(0x4abbc0, &cs) : Ours()->OnCreate(&cs);
			Logf("=%d;", Result);
		});
		break;
	}
	case 27: {
		// The new screen size: random, or the window's size and its neighbours.
		int Width = Script.Window.right - Script.Window.left, Height = Script.Window.bottom - Script.Window.top;
		LPARAM lParam = Chance(40) ? MAKELPARAM(Width + Between(-1, 1), Height + Between(-1, 1))
			: Chance(70)           ? MAKELPARAM(Between(500, 2000), Between(400, 1500))
								   : (LPARAM)Random();
		WPARAM wParam = Random() % 33;
		Both("OnDisplayChange", [&](int s) {
			LRESULT Result = s == IMAGE ? ImageMember<LRESULT>(0x4abe40, wParam, lParam) : Ours()->OnDisplayChange(wParam, lParam);
			Logf("=%d;", (int)Result);
		});
		break;
	}
	case 28: {
		int Error = Chance(60) ? 0 : Chance(50) ? WSAHOST_NOT_FOUND
												: Between(1, 0xffff);
		LPARAM lParam = MAKELPARAM(Between(0, 2000), Error);
		WPARAM wParam = Random() % 0x10000;
		if (Chance(50)) // resolving, as ResolveHostname leaves it
			World[0].ConnectionState = World[1].ConnectionState = 1;
		Both("OnHostResolved", [&](int s) {
			LRESULT Result = s == IMAGE ? ImageMember<LRESULT>(0x4a90e0, wParam, lParam) : Ours()->OnHostResolved(wParam, lParam);
			Logf("=%d;", (int)Result);
		});
		break;
	}
	case 29: {
		BOOL Active = Random() % 3;
		DWORD Thread = Random();
		Both("OnActivateApp", [&](int s) {
			if (s == IMAGE)
				ImageMember<void>(0x4a9310, Active, Thread);
			else
				Ours()->OnActivateApp(Active, Thread);
		});
		break;
	}
	case 30: {
		int Mode = Chance(45) ? 1 : Chance(80) ? 2
											   : Between(-1, 4);
		Both("SetDisplayMode", [&](int s) {
			if (s == IMAGE)
				ImageFree<void>(0x4a8af0, Mode);
			else
				SetDisplayMode(Mode);
		});
		break;
	}
	case 31: {
		static const char *URLs[] = {"http://www.tibia.com", "", "https://example.org/a?b=c", "x"};
		const char *URL = URLs[Random() % 4];
		Both("OpenURL", [&](int s) {
			if (s == IMAGE)
				ImageFree<void>(0x4a8d80, URL);
			else
				OpenURL(URL);
		});
		break;
	}
	case 32:
		Both("ExitProgram", [&](int s) {
			if (s == IMAGE)
				ImageFree<void>(0x4a88f0);
			else
				ExitProgram();
		});
		break;
	case 33:
		ImageApp[0xa1] = 0;
		((CPlayerApp *)OurApp)->RunPatch = false;
		Both("CloseProgram", [&](int s) {
			if (s == IMAGE)
				ImageFree<void>(0x4a8940);
			else
				CloseProgram();
		});
		break;
	case 34: {
		bool Name = Chance(50);
		Both("PreCreateWindow", [&](int s) {
			CREATESTRUCT cs;
			memset(&cs, 0, sizeof(cs));
			cs.lpszName = Name ? "Tibia" : NULL;
			cs.lpszClass = "Before";
			BOOL Result = s == IMAGE ? ImageMember<BOOL>(0x4a8f30, &cs) : Frame()->PreCreateWindow(cs);
			Logf("=%d,%s;", Result, cs.lpszClass == (s == IMAGE ? (const char *)0x5d167c : ClassName) ? "ClassName" : cs.lpszClass);
		});
		break;
	}
	case 35: {
		// A packet header in the statics, or none. Until Patella-Modern issue #74 OutputErrorInformations, which
		// printed it for error.txt, was compared here; it is gone, and its comparison with it. The header's random
		// bytes are still drawn, as they were, so that the operations after it keep their inputs.
		uint8 Header[16];
		memset(Header, 0, 16);
		if (Chance(70))
			for (int i = Random() % 16; i < 16; i++)
				Header[i] = (uint8)(Chance(30) ? 0 : Random());
		for (int s = 0; s < 2; s++)
			memcpy(Statics[s].PacketHeader, Header, 16);
		break;
	}
	case 36: {
		// The world (Preferences.cpp, Status.cpp, the engine).
		RandomWorld();
		break;
	}
	case 37: {
		UINT Key = RandomKey(), Flags = Chance(50) ? 0x100 : 0;
		Both("TranslateKey", [&](int s) {
			int Result = s == IMAGE ? ImageMember<int>(0x4a8210, Key, 1u, Flags) : Frame()->TranslateKey(Key, 1, Flags);
			Logf("=%x;", Result);
		});
		break;
	}
	case 38:
		// The button and closing state (as other messages would leave it).
		{
			bool L = Chance(50), R = Chance(50), C = Chance(20);
			for (int s = 0; s < 2; s++) {
				*Statics[s].LeftButtonPressed = L;
				*Statics[s].RightButtonPressed = R;
				*Statics[s].Closing = C;
			}
		}
		break;
	case 39: {
		// The constructor and destructor again.
		Destroy();
		Construct();
		break;
	}
	}
}

int RandomKind()
{
	static const int Weights[OPERATIONS] = {6, 3, 2, 5, 2, 2, 6, 4, 3, 3, 2, 3, 3, 2, 3, 2, 3, 2, 2, 4, 2, 3, 4, 3, 2, 2,
		3, 3, 4, 3, 4, 2, 2, 2, 2, 3, 4, 2, 4, 1};
	int Total = 0;
	for (int w : Weights)
		Total += w;
	int r = Random() % Total;
	for (int k = 0; k < OPERATIONS; k++) {
		if (r < Weights[k])
			return k;
		r -= Weights[k];
	}
	return 0;
}

void ResetStatics()
{
	for (int s = 0; s < 2; s++) {
		TStatics &T = Statics[s];
		memset(T.PacketHeader, 0, 16);
		*T.MainFrame = NULL;
		*T.Hand = *T.Size = *T.Wait = *T.Arrow = *T.Default = NULL;
		*T.LeftButtonPressed = *T.RightButtonPressed = *T.Closing = *T.DisplayChanged = false;
		memset(T.ClassName, 0, 32);
		*T.WheelDelta = 0;
		World[s].Ticks = 0;
	}
	memset(ImageApp, 0, sizeof(ImageApp));
	memset(OurApp, 0, sizeof(OurApp));
	Log[0].clear();
	Log[1].clear();
}

void StartRound()
{
	ResetStatics();
	RandomWorld();
	Construct();
}

void TestMainFrmErrors()
{
	ORIGINAL(VerifyOurSideAssertion("MainFrm"));
	TErrorRound R("MainFrm");
	Round = &R;
	for (int Kind = 0; Kind <= OPERATIONS; Kind++) {
		if (Kind == 36 || Kind == 38 || Kind == 39)
			continue;
		for (int Trial = 0; Trial < 6; Trial++) {
			uint32 Seed = Random() ^ (Random() << 16);
			int Length = Random() % 10;
			for (int n = 1; n < 200; n++) {
				SetRandomSeed(Seed);
				golden::Input("error seed", Seed);
				FailAt = 0;
				ResetStatics();
				RandomWorld();
				bool Threw;
				if (Kind == OPERATIONS) {
					// The constructor itself.
					memset(ImageFrame, 0xab, sizeof(ImageFrame));
					memset(OurFrame, 0xab, sizeof(OurFrame));
					FailAt = n;
					bool Gamemaster = Chance(50);
					Alive = false;
					Both("constructor", [&](int s) {
						if (s == IMAGE)
							ImageMember<void *>(0x4a8520, Gamemaster);
						else
							new (OurFrame) CMainFrame(Gamemaster);
					});
					Threw = World[0].Ticks >= n || World[1].Ticks >= n;
					FailAt = 0;
					if (!Threw) {
						Alive = true;
						Destroy();
					}
				} else {
					Construct();
					for (int i = 0; i < Length; i++) {
						int k = RandomKind();
						if (k != 39)
							Operation(k);
					}
					FailAt = n;
					Operation(Kind);
					Threw = World[0].Ticks >= n || World[1].Ticks >= n;
					FailAt = 0;
					Destroy();
				}
				if (!Threw)
					break;
			}
		}
	}
	Round = NULL;
	R.Finish();
}

} // namespace

void TestMainFrm()
{
	// Random()'s state is put back at the end: the later tests (MapWindow, MainWindow) get
	// the inputs they had before this test existed.
	uint32 SavedSeed = GetRandomSeed();
	ORIGINAL(PatchImage());
	memset(&LastOriginal, 0, sizeof(LastOriginal));
	Trace = getenv("MF_TRACE") != NULL;
	const char *LogName = getenv("MF_LOGS");
	LogFile = LogName != NULL ? fopen(LogName, "w") : NULL;
	MakeStatics();
	// user32 and shell32 through both import tables.
	struct {
		const char *Dll;
		const char *Name;
		void *Fake;
	} Imports[] = {
		{"USER32.dll", "PostMessageA", (void *)FakePostMessageA},
		{"USER32.dll", "GetClientRect", (void *)FakeGetClientRect},
		{"USER32.dll", "GetWindowRect", (void *)FakeGetWindowRect},
		{"USER32.dll", "IsIconic", (void *)FakeIsIconic},
		{"USER32.dll", "IsZoomed", (void *)FakeIsZoomed},
		{"USER32.dll", "KillTimer", (void *)FakeKillTimer},
		{"USER32.dll", "SetTimer", (void *)FakeSetTimer},
		{"USER32.dll", "SetCursor", (void *)FakeSetCursor},
		{"USER32.dll", "GetCursorPos", (void *)FakeGetCursorPos},
		{"USER32.dll", "PtInRect", (void *)FakePtInRect},
		{"USER32.dll", "ClientToScreen", (void *)FakeClientToScreen},
		{"USER32.dll", "ScreenToClient", (void *)FakeScreenToClient},
		{"USER32.dll", "SetCapture", (void *)FakeSetCapture},
		{"USER32.dll", "ReleaseCapture", (void *)FakeReleaseCapture},
		{"USER32.dll", "GetSystemMetrics", (void *)FakeGetSystemMetrics},
		{"USER32.dll", "LoadCursorA", (void *)FakeLoadCursorA},
		{"USER32.dll", "DestroyCursor", (void *)FakeDestroyCursor},
		{"SHELL32.dll", "ShellExecuteA", (void *)FakeShellExecuteA},
	};
	for (auto &I : Imports)
		BindBoth(I.Dll, I.Name, I.Fake);
#if GOLDEN_ORACLE
	// The image's MFC.
	struct {
		uint32 Address;
		void *Fake;
	} Mfc[] = {
		{0x52c0e6, (void *)ImageFrameCtor},
		{0x52c1ab, (void *)ImageFrameDtor},
		{0x5239d8, (void *)ImageGetPlacement},
		{0x5239ef, (void *)ImageSetPlacement},
		{0x526b61, (void *)ImageModifyStyle},
		{0x526b8b, (void *)ImageModifyStyleEx},
		{0x526c92, (void *)ImageSetWindowPos},
		{0x526bd6, (void *)ImageMoveWindow},
		{0x52bbf9, (void *)ImageOnSize},
		{0x524c2c, (void *)ImageDefault},
		{0x52abbd, (void *)ImageOnClose},
		{0x524f25, (void *)ImageDestroyWindow},
		{0x52be2b, (void *)ImageOnDestroy},
		{0x52a8b3, (void *)ImageOnSetCursor},
		{0x524ccf, (void *)ImageFromHandle},
		{0x52a65e, (void *)ImagePreCreateWindow},
		{0x52be1c, (void *)ImageOnCreate},
		{0x52d008, (void *)ImageModuleState},
	};
	for (auto &M : Mfc)
		Redirect((void *)(uintptr_t)M.Address, M.Fake);
#endif
	// The game files: one fake for both sides (ours of Control.cpp are playertest.cpp's in the oracle and
	// client_stubs.cpp's in the runner, ours of Network.cpp network_tests.cpp's on both sides, issue #30).
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		{0x446d10, (void *)LeftButtonDown, (void *)FakeLeftButtonDown},
		{0x447000, (void *)LeftButtonUp, (void *)FakeLeftButtonUp},
		{0x447390, (void *)LeftButtonDoubleClick, (void *)FakeLeftButtonDoubleClick},
		{0x4476c0, (void *)RightButtonDown, (void *)FakeRightButtonDown},
		{0x447880, (void *)RightButtonUp, (void *)FakeRightButtonUp},
		{0x447bd0, (void *)MouseWheel, (void *)FakeMouseWheel},
		{0x447e60, (void *)KeyPress, (void *)FakeKeyPress},
		{0x4489a0, (void *)TimerEvent, (void *)FakeTimerEvent},
		{0x4480b0, (void *)CloseRequest, (void *)FakeCloseRequest},
		{0x4459b0, (void *)Resize, (void *)FakeResize},
		{0x449210, (void *)Redraw, (void *)FakeRedraw},
		{0x44b510, (void *)InitControl, (void *)FakeInitControl},
		{0x448e30, (void *)ExitControl, (void *)FakeExitControl},
		{0x448c50, (void *)InitWindow, (void *)FakeInitWindow},
		{0x4b05b0, (void *)InitMainWindow, (void *)FakeInitMainWindow},
		{0x4db430, (void *)SetWindowedMode, (void *)FakeSetWindowedMode},
		{0x4db470, (void *)GetWindowedMode, (void *)FakeGetWindowedMode},
		{0x4db2c0, (void *)SetViewMode, (void *)FakeSetViewMode},
		{0x4db3a0, (void *)GetViewMode, (void *)FakeGetViewMode},
		{0x4db3c0, (void *)GetWindowsMousepointer, (void *)FakeGetWindowsMousepointer},
		{0x4e6e90, (void *)GetMouseCursor, (void *)FakeGetMouseCursor},
		{0x4e6e60, (void *)GetMouseX, (void *)FakeGetMouseX},
		{0x4e6e70, (void *)GetMouseY, (void *)FakeGetMouseY},
		{0x4e71e0, (void *)SetConnectionState, (void *)FakeSetConnectionState},
		{0x4e71f0, (void *)GetConnectionState, (void *)FakeGetConnectionState},
		{0x4e9020, (void *)AddMessage, (void *)FakeAddMessage},
		{0x4ca290, (void *)SetConnectionTimeout, (void *)FakeSetConnectionTimeout},
		{0x4ca730, (void *)SetServerError, (void *)FakeSetServerError},
		{0x4cf050, (void *)ConnectToLoginServer, (void *)FakeConnectToLoginServer},
		{0x4cdca0, (void *)GetResolvedIPAddress, (void *)FakeGetResolvedIPAddress},
		{0x4ca9f0, (void *)SetServerIPAddress, (void *)FakeSetServerIPAddress},
		{0x4ce300, (void *)Disconnect, (void *)FakeDisconnect},
		{0x4cd180, (void *)GetErrorMessage, (void *)FakeGetErrorMessage},
	};
	for (auto &T : Table) {
		ORIGINAL(Redirect((void *)(uintptr_t)T.Image, T.Fake));
		Redirect(T.Ours, T.Fake);
	}
	int SavedEngine = curEngine;
	ORIGINAL(int SavedImageEngine = Global<int>(0x71d0c4));

	int Before = Checks;
	int Rounds = getenv("MF_ROUNDS") != NULL ? atoi(getenv("MF_ROUNDS")) : 400;
	for (int r = 0; r < Rounds; r++) {
		if (Trace)
			printf("MainFrm: round %d\n", r);
		golden::Input("round seed", GetRandomSeed());
		StartRound();
		int Count = 10 + Random() % 60;
		for (int i = 0; i < Count; i++)
			Operation(RandomKind());
		Destroy();
	}
	printf("MainFrm: %d checks\n", Checks - Before);
	TestMainFrmErrors();
	ResetStatics();
	curEngine = SavedEngine;
	ORIGINAL(Global<int>(0x71d0c4) = SavedImageEngine);
	RestoreAll();
	SetRandomSeed(SavedSeed);
	if (LogFile != NULL)
		fclose(LogFile);
}
