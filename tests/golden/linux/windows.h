// The golden runner's stand-in for <windows.h> on Linux (issue #91): the types, constants and functions of the
// Windows API that the client's code under test (tests/golden/CMakeLists.txt's golden_client, golden_sysinc,
// Network.cpp, Communication.cpp, Control.cpp) and the ported tests use, compiled as on Windows. Test code only: no product target sees this directory.
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

#include "compat/win32_types.h"

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
