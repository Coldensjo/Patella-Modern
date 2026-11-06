// The golden runner's Windows functions on Linux (issue #91; linux/windows.h): each calls through an import slot of
// its own, GoldenSlot_<name>, as the Windows runner's calls go through its import table, and golden::FindImport
// (golden_patch.h) finds the slots by name. A slot starts at the function's default: a Linux implementation where
// the code under test or the tests need one (the clocks, the temporary folder, the current directory, the global
// memory of the clipboard functions), else a stop with the function's name (what the Windows runner reaches with
// a fake only; reaching it without one is a failure of the port, not of the code).
//
// The C library's time and fwrite are wrapped at the link (tests/golden/CMakeLists.txt, ld --wrap): their slots
// start at the real functions, __real_time and __real_fwrite. rand is GoldenRand (linux/windows.h says why).

#include <windows.h>

// The C library's rand itself here (linux/windows.h makes the name GoldenRand).
#undef rand

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <ctime>
#include <string>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

[[noreturn]] void Unexpected(const char *Name)
{
	std::printf("golden: the Windows function %s was called without a fake (tests/golden/linux/win32.cpp)\n", Name);
	std::fflush(stdout);
	std::abort();
}

thread_local DWORD LastError = 0;

std::uint64_t Milliseconds()
{
	return static_cast<std::uint64_t>(
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
			.count());
}

// GlobalAlloc's memory: its size before the bytes.
struct TGlobalHeader {
	SIZE_T Size;
	SIZE_T Padding;
};

// The address space as /proc/self/maps lists it: read again when an address is not in the last list (the images,
// which the tests ask about, do not move).
struct TMapping {
	std::uintptr_t Start, End;
	DWORD Protect;
	bool Image;
	std::uintptr_t ImageBase; // the lowest mapping of the same file
};
std::vector<TMapping> &Mappings()
{
	static std::vector<TMapping> List;
	return List;
}
void ReadMappings()
{
	std::vector<TMapping> List;
	std::FILE *File = std::fopen("/proc/self/maps", "r");
	if (File == nullptr)
		Unexpected("VirtualQuery (no /proc/self/maps)");
	char Line[4096];
	std::vector<std::string> Paths;
	while (std::fgets(Line, sizeof(Line), File) != nullptr) {
		unsigned long Start = 0, End = 0, Inode = 0;
		char Permissions[8] = {};
		int Path = 0;
		if (std::sscanf(Line, "%lx-%lx %7s %*s %*s %lu %n", &Start, &End, Permissions, &Inode, &Path) < 4)
			continue;
		std::string Name = Line + Path;
		while (!Name.empty() && (Name.back() == '\n' || Name.back() == ' '))
			Name.pop_back();
		const bool Read = Permissions[0] == 'r', Write = Permissions[1] == 'w', Execute = Permissions[2] == 'x';
		DWORD Protect = PAGE_NOACCESS;
		if (Execute)
			Protect = Write ? PAGE_EXECUTE_READWRITE : Read ? PAGE_EXECUTE_READ
															: PAGE_EXECUTE;
		else if (Read)
			Protect = Write ? PAGE_READWRITE : PAGE_READONLY;
		const bool Image = Inode != 0 && !Name.empty() && Name[0] == '/';
		std::uintptr_t Base = Start;
		for (std::size_t i = 0; i < List.size(); i++)
			if (Image && List[i].Image && Paths[i] == Name) {
				Base = List[i].ImageBase;
				break;
			}
		List.push_back(TMapping{Start, End, Protect, Image, Base});
		Paths.push_back(Name);
	}
	std::fclose(File);
	Mappings() = std::move(List);
}
const TMapping *FindMapping(std::uintptr_t Address, bool Again)
{
	for (const TMapping &Mapping : Mappings())
		if (Address >= Mapping.Start && Address < Mapping.End)
			return &Mapping;
	if (!Again)
		return nullptr;
	ReadMappings();
	return FindMapping(Address, false);
}

} // namespace

// The tests' memory functions (not import slots: no test replaces them).
SIZE_T VirtualQuery(LPCVOID Address, PMEMORY_BASIC_INFORMATION Buffer, SIZE_T Length)
{
	if (Length < sizeof(MEMORY_BASIC_INFORMATION))
		return 0;
	const std::uintptr_t Page = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
	const std::uintptr_t Here = reinterpret_cast<std::uintptr_t>(Address) & ~(Page - 1);
	*Buffer = MEMORY_BASIC_INFORMATION{};
	if (const TMapping *Mapping = FindMapping(Here, true)) {
		Buffer->BaseAddress = reinterpret_cast<PVOID>(Here);
		Buffer->AllocationBase = reinterpret_cast<PVOID>(Mapping->ImageBase);
		Buffer->AllocationProtect = Mapping->Protect;
		Buffer->RegionSize = Mapping->End - Here;
		Buffer->State = MEM_COMMIT;
		Buffer->Protect = Mapping->Protect;
		Buffer->Type = Mapping->Image ? MEM_IMAGE : MEM_PRIVATE;
		return sizeof(MEMORY_BASIC_INFORMATION);
	}
	// A gap: free up to the next mapping; none above it: the end of the address space.
	std::uintptr_t Next = 0;
	for (const TMapping &Mapping : Mappings())
		if (Mapping.Start > Here && (Next == 0 || Mapping.Start < Next))
			Next = Mapping.Start;
	if (Next == 0)
		return 0;
	Buffer->BaseAddress = reinterpret_cast<PVOID>(Here);
	Buffer->RegionSize = Next - Here;
	Buffer->State = MEM_FREE;
	Buffer->Protect = PAGE_NOACCESS;
	return sizeof(MEMORY_BASIC_INFORMATION);
}
BOOL VirtualProtect(LPVOID Address, SIZE_T Size, DWORD NewProtect, PDWORD OldProtect)
{
	const std::uintptr_t Page = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
	const std::uintptr_t Start = reinterpret_cast<std::uintptr_t>(Address) & ~(Page - 1);
	const std::uintptr_t End = (reinterpret_cast<std::uintptr_t>(Address) + Size + Page - 1) & ~(Page - 1);
	const TMapping *Mapping = FindMapping(Start, true);
	if (OldProtect != nullptr)
		*OldProtect = Mapping != nullptr ? Mapping->Protect : PAGE_NOACCESS;
	int Protection = PROT_NONE;
	switch (NewProtect & 0xff) {
	case PAGE_READONLY: Protection = PROT_READ; break;
	case PAGE_READWRITE:
	case PAGE_WRITECOPY: Protection = PROT_READ | PROT_WRITE; break;
	case PAGE_EXECUTE: Protection = PROT_EXEC; break;
	case PAGE_EXECUTE_READ: Protection = PROT_READ | PROT_EXEC; break;
	case PAGE_EXECUTE_READWRITE:
	case PAGE_EXECUTE_WRITECOPY: Protection = PROT_READ | PROT_WRITE | PROT_EXEC; break;
	default: break;
	}
	if (mprotect(reinterpret_cast<void *>(Start), End - Start, Protection) != 0)
		return FALSE;
	Mappings().clear(); // the protections changed: read the list again when asked
	return TRUE;
}
BOOL FlushInstructionCache(HANDLE, LPCVOID, SIZE_T) { return TRUE; } // x86-64 keeps the caches coherent
HANDLE HeapCreate(DWORD, SIZE_T, SIZE_T) { return reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(1)); }
LPVOID HeapAlloc(HANDLE, DWORD, SIZE_T Bytes) { return std::malloc(Bytes == 0 ? 1 : Bytes); }
BOOL HeapFree(HANDLE, DWORD, LPVOID Memory)
{
	std::free(Memory);
	return TRUE;
}
// The thread's stack from pthread_getattr_np: StackLimit its lowest address, StackBase its top.
_TEB *NtCurrentTeb()
{
	thread_local NT_TIB Block{};
	if (Block.StackBase == nullptr) {
		pthread_attr_t Attributes;
		void *Low = nullptr;
		std::size_t Size = 0;
		if (pthread_getattr_np(pthread_self(), &Attributes) != 0)
			Unexpected("NtCurrentTeb (pthread_getattr_np)");
		pthread_attr_getstack(&Attributes, &Low, &Size);
		pthread_attr_destroy(&Attributes);
		Block.StackLimit = Low;
		Block.StackBase = static_cast<char *>(Low) + Size;
	}
	return reinterpret_cast<_TEB *>(&Block);
}

// The defaults of the slots.
namespace golden_linux {

DWORD GetLastError() { return LastError; }
void SetLastError(DWORD Error) { LastError = Error; }
void Sleep(DWORD Duration) { std::this_thread::sleep_for(std::chrono::milliseconds(Duration)); }
DWORD GetTickCount() { return static_cast<DWORD>(Milliseconds()); }
DWORD GetCurrentProcessId() { return static_cast<DWORD>(getpid()); }
DWORD GetCurrentThreadId() { return static_cast<DWORD>(std::hash<std::thread::id>{}(std::this_thread::get_id())); }
HANDLE GetCurrentProcess() { return reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-1)); }
HMODULE GetModuleHandleA(LPCSTR) { Unexpected("GetModuleHandleA"); }
DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD) { Unexpected("GetModuleFileNameA"); }
void *GetProcAddress(HMODULE, LPCSTR) { Unexpected("GetProcAddress"); }
// The TEMP folder with a separator at its end, as Windows' (TEMP, then TMP, then /tmp/).
DWORD GetTempPathA(DWORD Length, LPSTR Buffer)
{
	const char *Folder = std::getenv("TEMP");
	if (Folder == nullptr || *Folder == 0)
		Folder = std::getenv("TMP");
	if (Folder == nullptr || *Folder == 0)
		Folder = "/tmp";
	std::string Path = Folder;
	if (Path.back() != '/')
		Path += '/';
	if (Path.size() + 1 > Length)
		return static_cast<DWORD>(Path.size() + 1);
	std::memcpy(Buffer, Path.c_str(), Path.size() + 1);
	return static_cast<DWORD>(Path.size());
}
BOOL CreateDirectoryA(LPCSTR Path, void *)
{
	if (mkdir(Path, 0777) == 0)
		return TRUE;
	LastError = errno == EEXIST ? ERROR_ALREADY_EXISTS : ERROR_FILE_NOT_FOUND;
	return FALSE;
}
BOOL RemoveDirectoryA(LPCSTR Path) { return rmdir(Path) == 0 ? TRUE : FALSE; }
BOOL DeleteFileA(LPCSTR Path) { return unlink(Path) == 0 ? TRUE : FALSE; }
DWORD GetCurrentDirectoryA(DWORD Length, LPSTR Buffer)
{
	char Path[4096];
	if (getcwd(Path, sizeof(Path)) == nullptr)
		return 0;
	const std::size_t Size = std::strlen(Path);
	if (Size + 1 > Length)
		return static_cast<DWORD>(Size + 1);
	std::memcpy(Buffer, Path, Size + 1);
	return static_cast<DWORD>(Size);
}
BOOL SetCurrentDirectoryA(LPCSTR Path) { return chdir(Path) == 0 ? TRUE : FALSE; }
DWORD GetFileAttributesA(LPCSTR Path)
{
	struct stat Status{};
	if (stat(Path, &Status) != 0)
		return INVALID_FILE_ATTRIBUTES;
	return S_ISDIR(Status.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0x80; // FILE_ATTRIBUTE_NORMAL
}
HGLOBAL GlobalAlloc(UINT Flags, SIZE_T Bytes)
{
	auto *Header = static_cast<TGlobalHeader *>(std::calloc(1, sizeof(TGlobalHeader) + Bytes));
	if (Header == nullptr)
		return nullptr;
	(void)Flags;
	Header->Size = Bytes;
	return Header;
}
HGLOBAL GlobalFree(HGLOBAL Memory)
{
	std::free(Memory);
	return nullptr;
}
LPVOID GlobalLock(HGLOBAL Memory) { return Memory == nullptr ? nullptr : static_cast<TGlobalHeader *>(Memory) + 1; }
BOOL GlobalUnlock(HGLOBAL) { return FALSE; }
SIZE_T GlobalSize(HGLOBAL Memory) { return Memory == nullptr ? 0 : static_cast<TGlobalHeader *>(Memory)->Size; }
// 100-nanosecond intervals since 1601.
void GetSystemTimeAsFileTime(LPFILETIME Time)
{
	const auto Now = std::chrono::system_clock::now().time_since_epoch();
	const std::uint64_t Ticks =
		static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Now).count() / 100) + 116444736000000000ull;
	Time->dwLowDateTime = static_cast<DWORD>(Ticks);
	Time->dwHighDateTime = static_cast<DWORD>(Ticks >> 32);
}
void GetLocalTime(LPSYSTEMTIME) { Unexpected("GetLocalTime"); }
BOOL QueryPerformanceCounter(LONGLONG *Count)
{
	*Count = static_cast<LONGLONG>(std::chrono::steady_clock::now().time_since_epoch().count());
	return TRUE;
}
BOOL QueryPerformanceFrequency(LONGLONG *Frequency)
{
	*Frequency = static_cast<LONGLONG>(std::chrono::steady_clock::period::den / std::chrono::steady_clock::period::num);
	return TRUE;
}
// A structured exception: a C++ exception here (linux/windows.h, TStructuredException).
void RaiseException(DWORD Code, DWORD, DWORD, const ULONG_PTR *) { throw TStructuredException{Code}; }
BOOL CloseHandle(HANDLE) { return TRUE; }
DWORD timeGetTime() { return static_cast<DWORD>(Milliseconds()); }
UINT timeBeginPeriod(UINT) { return 0; }
UINT timeEndPeriod(UINT) { return 0; }
int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT) { Unexpected("MessageBoxA"); }
BOOL PostMessageA(HWND, UINT, WPARAM, LPARAM) { Unexpected("PostMessageA"); }
LRESULT SendMessageA(HWND, UINT, WPARAM, LPARAM) { Unexpected("SendMessageA"); }
BOOL GetClientRect(HWND, LPRECT) { Unexpected("GetClientRect"); }
BOOL GetWindowRect(HWND, LPRECT) { Unexpected("GetWindowRect"); }
BOOL IsIconic(HWND) { Unexpected("IsIconic"); }
BOOL IsZoomed(HWND) { Unexpected("IsZoomed"); }
BOOL IsWindow(HWND) { Unexpected("IsWindow"); }
BOOL KillTimer(HWND, UINT_PTR) { Unexpected("KillTimer"); }
UINT_PTR SetTimer(HWND, UINT_PTR, UINT, TIMERPROC) { Unexpected("SetTimer"); }
HCURSOR SetCursor(HCURSOR) { Unexpected("SetCursor"); }
BOOL GetCursorPos(LPPOINT) { Unexpected("GetCursorPos"); }
BOOL SetCursorPos(int, int) { Unexpected("SetCursorPos"); }
BOOL PtInRect(const RECT *Rect, POINT Point)
{
	return Point.x >= Rect->left && Point.x < Rect->right && Point.y >= Rect->top && Point.y < Rect->bottom;
}
BOOL ClientToScreen(HWND, LPPOINT) { Unexpected("ClientToScreen"); }
BOOL ScreenToClient(HWND, LPPOINT) { Unexpected("ScreenToClient"); }
HWND SetCapture(HWND) { Unexpected("SetCapture"); }
BOOL ReleaseCapture() { Unexpected("ReleaseCapture"); }
HWND GetCapture() { Unexpected("GetCapture"); }
int GetSystemMetrics(int) { Unexpected("GetSystemMetrics"); }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR) { Unexpected("LoadCursorA"); }
HICON LoadIconA(HINSTANCE, LPCSTR) { Unexpected("LoadIconA"); }
BOOL DestroyCursor(HCURSOR) { Unexpected("DestroyCursor"); }
int ShowCursor(BOOL) { Unexpected("ShowCursor"); }
BOOL UpdateWindow(HWND) { Unexpected("UpdateWindow"); }
int GetWindowTextA(HWND, LPSTR, int) { Unexpected("GetWindowTextA"); }
int GetClassNameA(HWND, LPSTR, int) { Unexpected("GetClassNameA"); }
BOOL EnumWindows(WNDENUMPROC, LPARAM) { Unexpected("EnumWindows"); }
BOOL OpenClipboard(HWND) { Unexpected("OpenClipboard"); }
BOOL CloseClipboard() { Unexpected("CloseClipboard"); }
BOOL EmptyClipboard() { Unexpected("EmptyClipboard"); }
HANDLE GetClipboardData(UINT) { Unexpected("GetClipboardData"); }
HANDLE SetClipboardData(UINT, HANDLE) { Unexpected("SetClipboardData"); }
BOOL IsClipboardFormatAvailable(UINT) { Unexpected("IsClipboardFormatAvailable"); }
BOOL EnumDisplaySettingsA(LPCSTR, DWORD, DEVMODEA *) { Unexpected("EnumDisplaySettingsA"); }
LONG ChangeDisplaySettingsA(DEVMODEA *, DWORD) { Unexpected("ChangeDisplaySettingsA"); }
SHORT GetKeyState(int) { Unexpected("GetKeyState"); }
SHORT GetAsyncKeyState(int) { Unexpected("GetAsyncKeyState"); }
HWND GetForegroundWindow() { Unexpected("GetForegroundWindow"); }
BOOL SetForegroundWindow(HWND) { Unexpected("SetForegroundWindow"); }
HINSTANCE ShellExecuteA(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT) { Unexpected("ShellExecuteA"); }
int Rand() { return std::rand(); }

} // namespace golden_linux

// The slots, and the functions that call through them. GOLDEN_SLOT(Name) is the slot of ::Name with its default
// golden_linux::Name; the function (Parameters, Arguments) calls the slot as ::Name's own type.
#define GOLDEN_SLOT(Name) \
	extern "C" { \
	void *GoldenSlot_##Name = reinterpret_cast<void *>(static_cast<decltype(&::Name)>(&golden_linux::Name)); \
	}
#define GOLDEN_FUNCTION(Return, Name, Parameters, Arguments) \
	GOLDEN_SLOT(Name) \
	Return Name Parameters { return reinterpret_cast<decltype(&::Name)>(GoldenSlot_##Name) Arguments; }

GOLDEN_FUNCTION(DWORD, GetLastError, (), ())
GOLDEN_FUNCTION(void, SetLastError, (DWORD Error), (Error))
GOLDEN_FUNCTION(void, Sleep, (DWORD Milliseconds), (Milliseconds))
GOLDEN_FUNCTION(DWORD, GetTickCount, (), ())
GOLDEN_FUNCTION(DWORD, GetCurrentProcessId, (), ())
GOLDEN_FUNCTION(DWORD, GetCurrentThreadId, (), ())
GOLDEN_FUNCTION(HANDLE, GetCurrentProcess, (), ())
GOLDEN_FUNCTION(HMODULE, GetModuleHandleA, (LPCSTR Name), (Name))
GOLDEN_FUNCTION(DWORD, GetModuleFileNameA, (HMODULE Module, LPSTR Name, DWORD Size), (Module, Name, Size))
GOLDEN_FUNCTION(void *, GetProcAddress, (HMODULE Module, LPCSTR Name), (Module, Name))
GOLDEN_FUNCTION(DWORD, GetTempPathA, (DWORD Length, LPSTR Buffer), (Length, Buffer))
GOLDEN_FUNCTION(BOOL, CreateDirectoryA, (LPCSTR Path, void *Security), (Path, Security))
GOLDEN_FUNCTION(BOOL, RemoveDirectoryA, (LPCSTR Path), (Path))
GOLDEN_FUNCTION(BOOL, DeleteFileA, (LPCSTR Path), (Path))
GOLDEN_FUNCTION(DWORD, GetCurrentDirectoryA, (DWORD Length, LPSTR Buffer), (Length, Buffer))
GOLDEN_FUNCTION(BOOL, SetCurrentDirectoryA, (LPCSTR Path), (Path))
GOLDEN_FUNCTION(DWORD, GetFileAttributesA, (LPCSTR Path), (Path))
GOLDEN_FUNCTION(HGLOBAL, GlobalAlloc, (UINT Flags, SIZE_T Bytes), (Flags, Bytes))
GOLDEN_FUNCTION(HGLOBAL, GlobalFree, (HGLOBAL Memory), (Memory))
GOLDEN_FUNCTION(LPVOID, GlobalLock, (HGLOBAL Memory), (Memory))
GOLDEN_FUNCTION(BOOL, GlobalUnlock, (HGLOBAL Memory), (Memory))
GOLDEN_FUNCTION(SIZE_T, GlobalSize, (HGLOBAL Memory), (Memory))
GOLDEN_FUNCTION(void, GetSystemTimeAsFileTime, (LPFILETIME Time), (Time))
GOLDEN_FUNCTION(void, GetLocalTime, (LPSYSTEMTIME Time), (Time))
GOLDEN_FUNCTION(BOOL, QueryPerformanceCounter, (LONGLONG * Count), (Count))
GOLDEN_FUNCTION(BOOL, QueryPerformanceFrequency, (LONGLONG * Frequency), (Frequency))
GOLDEN_SLOT(RaiseException)
void RaiseException(DWORD Code, DWORD Flags, DWORD NumberOfArguments, const ULONG_PTR *Arguments)
{
	reinterpret_cast<decltype(&::RaiseException)>(GoldenSlot_RaiseException)(Code, Flags, NumberOfArguments, Arguments);
	std::abort(); // a fake that returns: a structured exception does not continue
}
GOLDEN_FUNCTION(BOOL, CloseHandle, (HANDLE Object), (Object))
GOLDEN_FUNCTION(DWORD, timeGetTime, (), ())
GOLDEN_FUNCTION(UINT, timeBeginPeriod, (UINT Period), (Period))
GOLDEN_FUNCTION(UINT, timeEndPeriod, (UINT Period), (Period))
GOLDEN_FUNCTION(int, MessageBoxA, (HWND Window, LPCSTR Text, LPCSTR Caption, UINT Type), (Window, Text, Caption, Type))
GOLDEN_FUNCTION(BOOL, PostMessageA, (HWND Window, UINT Message, WPARAM wParam, LPARAM lParam),
	(Window, Message, wParam, lParam))
GOLDEN_FUNCTION(LRESULT, SendMessageA, (HWND Window, UINT Message, WPARAM wParam, LPARAM lParam),
	(Window, Message, wParam, lParam))
GOLDEN_FUNCTION(BOOL, GetClientRect, (HWND Window, LPRECT Rect), (Window, Rect))
GOLDEN_FUNCTION(BOOL, GetWindowRect, (HWND Window, LPRECT Rect), (Window, Rect))
GOLDEN_FUNCTION(BOOL, IsIconic, (HWND Window), (Window))
GOLDEN_FUNCTION(BOOL, IsZoomed, (HWND Window), (Window))
GOLDEN_FUNCTION(BOOL, IsWindow, (HWND Window), (Window))
GOLDEN_FUNCTION(BOOL, KillTimer, (HWND Window, UINT_PTR Id), (Window, Id))
GOLDEN_FUNCTION(UINT_PTR, SetTimer, (HWND Window, UINT_PTR Id, UINT Elapse, TIMERPROC Function),
	(Window, Id, Elapse, Function))
GOLDEN_FUNCTION(HCURSOR, SetCursor, (HCURSOR Cursor), (Cursor))
GOLDEN_FUNCTION(BOOL, GetCursorPos, (LPPOINT Point), (Point))
GOLDEN_FUNCTION(BOOL, SetCursorPos, (int x, int y), (x, y))
GOLDEN_FUNCTION(BOOL, PtInRect, (const RECT *Rect, POINT Point), (Rect, Point))
GOLDEN_FUNCTION(BOOL, ClientToScreen, (HWND Window, LPPOINT Point), (Window, Point))
GOLDEN_FUNCTION(BOOL, ScreenToClient, (HWND Window, LPPOINT Point), (Window, Point))
GOLDEN_FUNCTION(HWND, SetCapture, (HWND Window), (Window))
GOLDEN_FUNCTION(BOOL, ReleaseCapture, (), ())
GOLDEN_FUNCTION(HWND, GetCapture, (), ())
GOLDEN_FUNCTION(int, GetSystemMetrics, (int Index), (Index))
GOLDEN_FUNCTION(HCURSOR, LoadCursorA, (HINSTANCE Instance, LPCSTR Name), (Instance, Name))
GOLDEN_FUNCTION(HICON, LoadIconA, (HINSTANCE Instance, LPCSTR Name), (Instance, Name))
GOLDEN_FUNCTION(BOOL, DestroyCursor, (HCURSOR Cursor), (Cursor))
GOLDEN_FUNCTION(int, ShowCursor, (BOOL Show), (Show))
GOLDEN_FUNCTION(BOOL, UpdateWindow, (HWND Window), (Window))
GOLDEN_FUNCTION(int, GetWindowTextA, (HWND Window, LPSTR Text, int MaxCount), (Window, Text, MaxCount))
GOLDEN_FUNCTION(int, GetClassNameA, (HWND Window, LPSTR Name, int MaxCount), (Window, Name, MaxCount))
GOLDEN_FUNCTION(BOOL, EnumWindows, (WNDENUMPROC Function, LPARAM Parameter), (Function, Parameter))
GOLDEN_FUNCTION(BOOL, OpenClipboard, (HWND Window), (Window))
GOLDEN_FUNCTION(BOOL, CloseClipboard, (), ())
GOLDEN_FUNCTION(BOOL, EmptyClipboard, (), ())
GOLDEN_FUNCTION(HANDLE, GetClipboardData, (UINT Format), (Format))
GOLDEN_FUNCTION(HANDLE, SetClipboardData, (UINT Format, HANDLE Memory), (Format, Memory))
GOLDEN_FUNCTION(BOOL, IsClipboardFormatAvailable, (UINT Format), (Format))
GOLDEN_FUNCTION(BOOL, EnumDisplaySettingsA, (LPCSTR Device, DWORD Index, DEVMODEA *Mode), (Device, Index, Mode))
GOLDEN_FUNCTION(LONG, ChangeDisplaySettingsA, (DEVMODEA * Mode, DWORD Flags), (Mode, Flags))
GOLDEN_FUNCTION(SHORT, GetKeyState, (int Key), (Key))
GOLDEN_FUNCTION(SHORT, GetAsyncKeyState, (int Key), (Key))
GOLDEN_FUNCTION(HWND, GetForegroundWindow, (), ())
GOLDEN_FUNCTION(BOOL, SetForegroundWindow, (HWND Window), (Window))
GOLDEN_FUNCTION(HINSTANCE, ShellExecuteA,
	(HWND Window, LPCSTR Operation, LPCSTR File, LPCSTR Parameters, LPCSTR Directory, INT Show),
	(Window, Operation, File, Parameters, Directory, Show))

// rand: GoldenRand (linux/windows.h), its slot "rand".
extern "C" {
void *GoldenSlot_rand = reinterpret_cast<void *>(static_cast<int (*)()>(&golden_linux::Rand));
}
int GoldenRand() { return reinterpret_cast<int (*)()>(GoldenSlot_rand)(); }

// time and fwrite: the link's wrappers (ld --wrap), their slots "_time64" (the name of the Microsoft CRT's
// time) and "fwrite".
extern "C" {
std::time_t __real_time(std::time_t *Time);
std::size_t __real_fwrite(const void *Buffer, std::size_t Size, std::size_t Count, std::FILE *Stream);
void *GoldenSlot_time = reinterpret_cast<void *>(&__real_time);
void *GoldenSlot_fwrite = reinterpret_cast<void *>(&__real_fwrite);
std::time_t __wrap_time(std::time_t *Time)
{
	return reinterpret_cast<std::time_t (*)(std::time_t *)>(GoldenSlot_time)(Time);
}
std::size_t __wrap_fwrite(const void *Buffer, std::size_t Size, std::size_t Count, std::FILE *Stream)
{
	return reinterpret_cast<std::size_t (*)(const void *, std::size_t, std::size_t, std::FILE *)>(GoldenSlot_fwrite)(
		Buffer, Size, Count, Stream);
}
}

namespace golden {

void **FindLinuxImport(const char *Function); // golden_patch.h

// The import slots by name: those of the Windows runner's imports the tests bind (golden_patch.h FindImport).
void **FindLinuxImport(const char *Function)
{
	struct TImport {
		const char *Name;
		void **Slot;
	};
#define GOLDEN_IMPORT(Name) \
	{ \
		#Name, &GoldenSlot_##Name \
	}
	static const TImport Imports[] = {
		GOLDEN_IMPORT(Sleep),
		GOLDEN_IMPORT(rand),
		GOLDEN_IMPORT(fwrite),
		{"_time64", &GoldenSlot_time},
		GOLDEN_IMPORT(PostMessageA),
		GOLDEN_IMPORT(GetClientRect),
		GOLDEN_IMPORT(GetWindowRect),
		GOLDEN_IMPORT(IsIconic),
		GOLDEN_IMPORT(IsZoomed),
		GOLDEN_IMPORT(KillTimer),
		GOLDEN_IMPORT(SetTimer),
		GOLDEN_IMPORT(SetCursor),
		GOLDEN_IMPORT(GetCursorPos),
		GOLDEN_IMPORT(PtInRect),
		GOLDEN_IMPORT(ClientToScreen),
		GOLDEN_IMPORT(ScreenToClient),
		GOLDEN_IMPORT(SetCapture),
		GOLDEN_IMPORT(ReleaseCapture),
		GOLDEN_IMPORT(GetSystemMetrics),
		GOLDEN_IMPORT(LoadCursorA),
		GOLDEN_IMPORT(DestroyCursor),
		GOLDEN_IMPORT(ShellExecuteA),
		GOLDEN_IMPORT(MessageBoxA),
		GOLDEN_IMPORT(timeGetTime),
		GOLDEN_IMPORT(GetTempPathA),
	};
#undef GOLDEN_IMPORT
	for (const TImport &Import : Imports)
		if (std::strcmp(Import.Name, Function) == 0)
			return Import.Slot;
	return nullptr;
}

} // namespace golden

// The runner's own operator new and delete (issue #91): the window tests redirect them (contentwindows_tests.cpp and
// dialogs_tests.cpp: new to their pattern, delete to their table of blocks; golden::RedirectOwn), which reaches the
// runner's image only; on Windows the runner's are in its image, on Linux they would be libstdc++'s. container_tests.cpp
// defines new[] and delete[], over malloc and free as these.
void *operator new(std::size_t Size)
{
	void *Memory = std::malloc(Size == 0 ? 1 : Size);
	if (Memory == nullptr)
		throw std::bad_alloc();
	return Memory;
}
void operator delete(void *Memory) noexcept
{
	std::free(Memory);
}
void operator delete(void *Memory, std::size_t) noexcept
{
	std::free(Memory);
}
