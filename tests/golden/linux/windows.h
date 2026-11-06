// The golden runner's stand-in for <windows.h> on Linux (issue #91): the types, constants and functions of the
// Windows API that the client's code under test (tests/golden/CMakeLists.txt's golden_client, golden_sysinc,
// Network.cpp, Communication.cpp, Control.cpp, MainFrm.cpp) and the ported tests use, compiled against
// src/compat/mfc_shim.h as on Windows. Test code only: no product target sees this directory.
//
// The types have the Windows x64 sizes (LONG and DWORD 4 bytes, where Linux's long has 8; the handles and the
// *_PTR types 8), the constants the Windows SDK's values (the tests record some of them, the virtual keys and the
// messages). The functions are linux/win32.cpp's: each calls through an import slot of its own, as the Windows
// runner's calls go through its import table, so that golden::FindImport and SetImport (golden_patch.h) bind a
// test's fake to it. A function the tests never reach without a fake stops the runner with its name.
//
// The C library's rand is the stand-ins' too (GoldenRand, below): glibc declares rand noexcept (__THROW), and the
// Network test's fake rand throws in its error round, which would end the program in a caller that relies on
// noexcept. time and fwrite are wrapped at the link instead (ld --wrap, tests/golden/CMakeLists.txt).

#pragma once

#if defined(_WIN32)
#error "tests/golden/linux is the runner's Linux stand-in for the Windows headers"
#endif

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits.h>
#include <linux/limits.h>

// The client's own PATH_MAX (graphics.h, the original's 1024) is not the Linux headers' macro.
#undef PATH_MAX

// --- The compiler's spellings ---

#define __cdecl
#define __stdcall
#define __fastcall
#define WINAPI
#define CALLBACK
#define APIENTRY
#define PASCAL
#define FAR
#define NEAR
#define CONST const
#define VOID void
// __declspec(noinline) of the tests (commworld.h FillStack); no other __declspec is used.
#define __declspec(Specifier) GOLDEN_DECLSPEC_##Specifier
#define GOLDEN_DECLSPEC_noinline __attribute__((noinline))

// --- The types (Windows x64 sizes) ---

using BOOL = int;
using BOOLEAN = unsigned char;
using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using UINT = unsigned int;
using INT = int;
using SHORT = short;
using USHORT = unsigned short;
using CHAR = char;
using UCHAR = unsigned char;
using TCHAR = char;
using WCHAR = char16_t;
using LONGLONG = std::int64_t;
using ULONGLONG = std::uint64_t;
using DWORD64 = std::uint64_t;
using INT_PTR = std::intptr_t;
using UINT_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using ULONG_PTR = std::uintptr_t;
using DWORD_PTR = ULONG_PTR;
using SIZE_T = std::size_t;
using WPARAM = UINT_PTR;
using LPARAM = LONG_PTR;
using LRESULT = LONG_PTR;
using ATOM = WORD;
using COLORREF = DWORD;
using HRESULT = LONG;
using LPVOID = void *;
using LPCVOID = const void *;
using PVOID = void *;
using LPSTR = char *;
using LPCSTR = const char *;
using LPTSTR = char *;
using LPCTSTR = const char *;
using LPWSTR = WCHAR *;
using LPCWSTR = const WCHAR *;
using LPBYTE = BYTE *;
using LPDWORD = DWORD *;
using LPLONG = LONG *;
using PLONG = LONG *;
using LPINT = int *;
using LPBOOL = BOOL *;

#define DECLARE_HANDLE(Name) \
	struct Name##__ { \
		int unused; \
	}; \
	using Name = Name##__ *
using HANDLE = void *;
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HMONITOR);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HFONT);
using HMODULE = HINSTANCE;
using HCURSOR = HICON;
using HGLOBAL = HANDLE;
using HLOCAL = HANDLE;
using HGDIOBJ = void *;

#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL nullptr
#endif
#define MAX_PATH 260
#define INFINITE 0xffffffffu

#define LOWORD(l) (static_cast<WORD>(static_cast<DWORD_PTR>(l) & 0xffff))
#define HIWORD(l) (static_cast<WORD>((static_cast<DWORD_PTR>(l) >> 16) & 0xffff))
#define LOBYTE(w) (static_cast<BYTE>(static_cast<DWORD_PTR>(w) & 0xff))
#define HIBYTE(w) (static_cast<BYTE>((static_cast<DWORD_PTR>(w) >> 8) & 0xff))
#define MAKEWORD(a, b) (static_cast<WORD>((static_cast<BYTE>(a)) | (static_cast<WORD>(static_cast<BYTE>(b)) << 8)))
#define MAKELONG(a, b) (static_cast<LONG>((static_cast<WORD>(a)) | (static_cast<DWORD>(static_cast<WORD>(b)) << 16)))
#define MAKELPARAM(l, h) (static_cast<LPARAM>(static_cast<DWORD>(MAKELONG(l, h))))
#define MAKEWPARAM(l, h) (static_cast<WPARAM>(static_cast<DWORD>(MAKELONG(l, h))))
#define MAKELRESULT(l, h) (static_cast<LRESULT>(static_cast<DWORD>(MAKELONG(l, h))))
#define RGB(r, g, b) (static_cast<COLORREF>((static_cast<BYTE>(r) | (static_cast<WORD>(static_cast<BYTE>(g)) << 8)) | (static_cast<DWORD>(static_cast<BYTE>(b)) << 16)))
#define MAKEINTRESOURCEA(i) (reinterpret_cast<LPSTR>(static_cast<ULONG_PTR>(static_cast<WORD>(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEA

typedef struct tagRECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT, *PRECT, *LPRECT;
using LPCRECT = const RECT *;
typedef struct tagPOINT {
	LONG x;
	LONG y;
} POINT, *PPOINT, *LPPOINT;
typedef struct tagSIZE {
	LONG cx;
	LONG cy;
} SIZE, *PSIZE, *LPSIZE;
typedef struct tagMSG {
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
} MSG, *PMSG, *LPMSG;
typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME {
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

using WNDPROC = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);
using TIMERPROC = void (*)(HWND, UINT, UINT_PTR, DWORD);
typedef struct tagWNDCLASSA {
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCSTR lpszMenuName;
	LPCSTR lpszClassName;
} WNDCLASSA, WNDCLASS, *LPWNDCLASS;
typedef struct tagCREATESTRUCTA {
	LPVOID lpCreateParams;
	HINSTANCE hInstance;
	HMENU hMenu;
	HWND hwndParent;
	int cy;
	int cx;
	int y;
	int x;
	LONG style;
	LPCSTR lpszName;
	LPCSTR lpszClass;
	DWORD dwExStyle;
} CREATESTRUCTA, CREATESTRUCT, *LPCREATESTRUCT;
typedef struct tagWINDOWPLACEMENT {
	UINT length;
	UINT flags;
	UINT showCmd;
	POINT ptMinPosition;
	POINT ptMaxPosition;
	RECT rcNormalPosition;
} WINDOWPLACEMENT, *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;
typedef struct tagMINMAXINFO {
	POINT ptReserved;
	POINT ptMaxSize;
	POINT ptMaxPosition;
	POINT ptMinTrackSize;
	POINT ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;
// DEVMODEA with the members the client uses at their Windows offsets (220 bytes, dmSize 156 + no driver data).
typedef struct _devicemodeA {
	BYTE dmDeviceName[32];
	WORD dmSpecVersion;
	WORD dmDriverVersion;
	WORD dmSize;
	WORD dmDriverExtra;
	DWORD dmFields;
	union {
		struct {
			short dmOrientation;
			short dmPaperSize;
			short dmPaperLength;
			short dmPaperWidth;
			short dmScale;
			short dmCopies;
			short dmDefaultSource;
			short dmPrintQuality;
		} Printer;
		struct {
			POINT dmPosition;
			DWORD dmDisplayOrientation;
			DWORD dmDisplayFixedOutput;
		} Display;
	} Union1;
	short dmColor;
	short dmDuplex;
	short dmYResolution;
	short dmTTOption;
	short dmCollate;
	BYTE dmFormName[32];
	WORD dmLogPixels;
	DWORD dmBitsPerPel;
	DWORD dmPelsWidth;
	DWORD dmPelsHeight;
	DWORD dmDisplayFlags;
	DWORD dmDisplayFrequency;
	DWORD dmICMMethod;
	DWORD dmICMIntent;
	DWORD dmMediaType;
	DWORD dmDitherType;
	DWORD dmReserved1;
	DWORD dmReserved2;
	DWORD dmPanningWidth;
	DWORD dmPanningHeight;
} DEVMODEA, DEVMODE, *LPDEVMODEA, *LPDEVMODE;

// --- The constants (the Windows SDK's values) ---

// Messages.
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000f
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ACTIVATEAPP 0x001c
#define WM_SETCURSOR 0x0020
#define WM_GETMINMAXINFO 0x0024
#define WM_DISPLAYCHANGE 0x007e
#define WM_SETICON 0x0080
#define WM_NCHITTEST 0x0084
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEWHEEL 0x020a
#define WM_CAPTURECHANGED 0x0215
#define WM_USER 0x0400
#define WHEEL_DELTA 120
#define ICON_SMALL 0
#define ICON_BIG 1

// Virtual keys.
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0d
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2d
#define VK_DELETE 0x2e
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7a
#define VK_F12 0x7b
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010

// Message boxes, windows, cursors, metrics.
#define MB_OK 0x00000000u
#define MB_OKCANCEL 0x00000001u
#define MB_YESNO 0x00000004u
#define MB_ICONHAND 0x00000010u
#define MB_ICONERROR MB_ICONHAND
#define MB_ICONSTOP MB_ICONHAND
#define MB_ICONQUESTION 0x00000020u
#define MB_ICONEXCLAMATION 0x00000030u
#define MB_ICONWARNING MB_ICONEXCLAMATION
#define MB_ICONASTERISK 0x00000040u
#define MB_ICONINFORMATION MB_ICONASTERISK
#define MB_TASKMODAL 0x00002000u
#define MB_SETFOREGROUND 0x00010000u
#define MB_TOPMOST 0x00040000u
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9
#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define HTCLIENT 1
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_CLIENTEDGE 0x00000200L
#define WS_EX_APPWINDOW 0x00040000L
#define WS_EX_TOPMOST 0x00000008L
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define IDC_SIZENS MAKEINTRESOURCE(32645)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define CF_TEXT 1
#define CF_UNICODETEXT 13
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
#define GMEM_DDESHARE 0x2000
#define GHND (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#define CS_OWNDC 0x0020
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

// Display modes.
#define ENUM_CURRENT_SETTINGS (static_cast<DWORD>(-1))
#define DM_BITSPERPEL 0x00040000L
#define DM_PELSWIDTH 0x00080000L
#define DM_PELSHEIGHT 0x00100000L
#define DM_DISPLAYFREQUENCY 0x00400000L
#define CDS_FULLSCREEN 0x00000004
#define DISP_CHANGE_SUCCESSFUL 0

// Structured exceptions (linux/win32.cpp's RaiseException throws them as TStructuredException).
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define EXCEPTION_NONCONTINUABLE 0x1
#define EXCEPTION_INT_DIVIDE_BY_ZERO 0xC0000094u
#define EXCEPTION_ACCESS_VIOLATION 0xC0000005u

// Files.
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)))
#define INVALID_FILE_ATTRIBUTES (static_cast<DWORD>(-1))
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define ERROR_SUCCESS 0L
#define NO_ERROR 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_ALREADY_EXISTS 183L

// Memory (the tests' code patches and their maps of the address space: linux/win32.cpp over mprotect and
// /proc/self/maps).
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD 0x100
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_FREE 0x10000
#define MEM_PRIVATE 0x20000
#define MEM_MAPPED 0x40000
#define MEM_IMAGE 0x1000000
typedef struct _MEMORY_BASIC_INFORMATION {
	PVOID BaseAddress;
	PVOID AllocationBase;
	DWORD AllocationProtect;
	WORD PartitionId;
	SIZE_T RegionSize;
	DWORD State;
	DWORD Protect;
	DWORD Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;
using PDWORD = DWORD *;
// The thread's stack (NtCurrentTeb()->StackBase, its top, and StackLimit, its lowest address).
typedef struct _NT_TIB {
	void *ExceptionList;
	PVOID StackBase;
	PVOID StackLimit;
} NT_TIB;
struct _TEB;
_TEB *NtCurrentTeb();
SIZE_T VirtualQuery(LPCVOID Address, PMEMORY_BASIC_INFORMATION Buffer, SIZE_T Length);
BOOL VirtualProtect(LPVOID Address, SIZE_T Size, DWORD NewProtect, PDWORD OldProtect);
BOOL FlushInstructionCache(HANDLE Process, LPCVOID Address, SIZE_T Size);
// A heap of its own: malloc's.
HANDLE HeapCreate(DWORD Options, SIZE_T InitialSize, SIZE_T MaximumSize);
LPVOID HeapAlloc(HANDLE Heap, DWORD Flags, SIZE_T Bytes);
BOOL HeapFree(HANDLE Heap, DWORD Flags, LPVOID Memory);
// The Microsoft CRT's heap check (<malloc.h>): glibc checks its heap itself.
#define _HEAPEMPTY (-1)
#define _HEAPOK (-2)
inline int _heapchk() { return _HEAPOK; }

// --- The functions (linux/win32.cpp) ---

// kernel32
DWORD GetLastError();
void SetLastError(DWORD Error);
void Sleep(DWORD Milliseconds);
DWORD GetTickCount();
DWORD GetCurrentProcessId();
DWORD GetCurrentThreadId();
HANDLE GetCurrentProcess();
HMODULE GetModuleHandleA(LPCSTR Name);
DWORD GetModuleFileNameA(HMODULE Module, LPSTR Name, DWORD Size);
void *GetProcAddress(HMODULE Module, LPCSTR Name);
DWORD GetTempPathA(DWORD Length, LPSTR Buffer);
BOOL CreateDirectoryA(LPCSTR Path, void *Security);
BOOL RemoveDirectoryA(LPCSTR Path);
BOOL DeleteFileA(LPCSTR Path);
DWORD GetCurrentDirectoryA(DWORD Length, LPSTR Buffer);
BOOL SetCurrentDirectoryA(LPCSTR Path);
DWORD GetFileAttributesA(LPCSTR Path);
HGLOBAL GlobalAlloc(UINT Flags, SIZE_T Bytes);
HGLOBAL GlobalFree(HGLOBAL Memory);
LPVOID GlobalLock(HGLOBAL Memory);
BOOL GlobalUnlock(HGLOBAL Memory);
SIZE_T GlobalSize(HGLOBAL Memory);
void GetSystemTimeAsFileTime(LPFILETIME Time);
void GetLocalTime(LPSYSTEMTIME Time);
BOOL QueryPerformanceCounter(LONGLONG *Count);
BOOL QueryPerformanceFrequency(LONGLONG *Frequency);
[[noreturn]] void RaiseException(DWORD Code, DWORD Flags, DWORD NumberOfArguments, const ULONG_PTR *Arguments);
BOOL CloseHandle(HANDLE Object);
// winmm
DWORD timeGetTime();
UINT timeBeginPeriod(UINT Period);
UINT timeEndPeriod(UINT Period);
// user32
int MessageBoxA(HWND Window, LPCSTR Text, LPCSTR Caption, UINT Type);
BOOL PostMessageA(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageA(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam);
BOOL GetClientRect(HWND Window, LPRECT Rect);
BOOL GetWindowRect(HWND Window, LPRECT Rect);
BOOL IsIconic(HWND Window);
BOOL IsZoomed(HWND Window);
BOOL IsWindow(HWND Window);
BOOL KillTimer(HWND Window, UINT_PTR Id);
UINT_PTR SetTimer(HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function);
HCURSOR SetCursor(HCURSOR Cursor);
BOOL GetCursorPos(LPPOINT Point);
BOOL SetCursorPos(int x, int y);
BOOL PtInRect(const RECT *Rect, POINT Point);
BOOL ClientToScreen(HWND Window, LPPOINT Point);
BOOL ScreenToClient(HWND Window, LPPOINT Point);
HWND SetCapture(HWND Window);
BOOL ReleaseCapture();
HWND GetCapture();
int GetSystemMetrics(int Index);
HCURSOR LoadCursorA(HINSTANCE Instance, LPCSTR Name);
HICON LoadIconA(HINSTANCE Instance, LPCSTR Name);
BOOL DestroyCursor(HCURSOR Cursor);
int ShowCursor(BOOL Show);
BOOL UpdateWindow(HWND Window);
int GetWindowTextA(HWND Window, LPSTR Text, int MaxCount);
int GetClassNameA(HWND Window, LPSTR Name, int MaxCount);
using WNDENUMPROC = BOOL (*)(HWND, LPARAM);
BOOL EnumWindows(WNDENUMPROC Function, LPARAM Parameter);
BOOL OpenClipboard(HWND Window);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE GetClipboardData(UINT Format);
HANDLE SetClipboardData(UINT Format, HANDLE Memory);
BOOL IsClipboardFormatAvailable(UINT Format);
BOOL EnumDisplaySettingsA(LPCSTR Device, DWORD Index, DEVMODEA *Mode);
LONG ChangeDisplaySettingsA(DEVMODEA *Mode, DWORD Flags);
SHORT GetKeyState(int Key);
SHORT GetAsyncKeyState(int Key);
HWND GetForegroundWindow();
BOOL SetForegroundWindow(HWND Window);
// shell32
HINSTANCE ShellExecuteA(HWND Window, LPCSTR Operation, LPCSTR File, LPCSTR Parameters, LPCSTR Directory, INT Show);

#define MessageBox MessageBoxA
#define PostMessage PostMessageA
#define SendMessage SendMessageA
#define LoadCursor LoadCursorA
#define LoadIcon LoadIconA
#define GetTempPath GetTempPathA
#define CreateDirectory CreateDirectoryA
#define RemoveDirectory RemoveDirectoryA
#define DeleteFile DeleteFileA
#define GetModuleHandle GetModuleHandleA
#define GetModuleFileName GetModuleFileNameA
#define GetWindowText GetWindowTextA
#define GetClassName GetClassNameA
#define EnumDisplaySettings EnumDisplaySettingsA
#define ChangeDisplaySettings ChangeDisplaySettingsA
#define ShellExecute ShellExecuteA

// A structured exception of RaiseException (and the tests' __try/__except, in their Linux forms): a C++
// exception here, which catch (...) catches, as MSVC's /EHa catch (...) catches a structured exception.
struct TStructuredException {
	DWORD Code;
};

// The C library's rand through an import slot of its own (the comment at the top).
int GoldenRand();
#define rand GoldenRand
namespace std {
using ::GoldenRand;
}

// --- The Microsoft CRT's spellings the ported tests use (its <stdio.h>, <string.h>, <stdlib.h>, <time.h>) ---

// compat/msvc.h's portable functions with the Microsoft semantics (compat/msvc_portable.h).
#include "compat/msvc.h"

#define _snprintf MsvcSnprintf
#define _vsnprintf MsvcVsnprintf
#define _stricmp MsvcStricmp
#define _strdup MsvcStrdup
#define _putenv_s MsvcPutenv
// The client's clock is time() (the platform's TWin32Clock, issue #109), 64 bits as the Microsoft CRT's _time64.
using __time64_t = std::time_t;
static_assert(sizeof(__time64_t) == 8, "time_t has 64 bits");
inline __time64_t _time64(__time64_t *Time) { return std::time(Time); }
// The original's 32-bit time (the tests' fixed clocks; types/Time32.h converts the client's).
using __time32_t = std::int32_t;
inline void _tzset() { tzset(); }

// GetVersionExA's structure (errors_tests.cpp's fake; the client calls it no more, issue #74).
typedef struct _OSVERSIONINFOA {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
} OSVERSIONINFOA, *LPOSVERSIONINFOA;
