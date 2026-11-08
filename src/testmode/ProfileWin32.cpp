// ProfileWin32.cpp: the test build's profile windows (Patella-Modern issue #134; ProfileWin32.h).
//
// One window at a time, opened and closed by the control channel between two frames. Per frame (TRenderer::Present
// to the next one, the hooks PATELLA_TEST_PROFILE_PRESENT_*): the performance counter at Present's entry, Present's
// own duration and, in an "allocs" window, the frame's heap calls; in a "gpu" window the Vulkan engine's timestamp
// queries (TRenderer::SetFrameTimestamps). The file is written when the window closes: nothing of it runs during
// the frames, and in a "time" window neither the heap hooks nor the timestamps are on.
//
// The heap calls are counted as patcap.dll counts Tibia.exe's: at the executable's own imports of HeapAlloc,
// HeapReAlloc and HeapFree (IAT patched at the first "allocs" window, passing through always). So the same calls are
// counted in both clients: malloc and operator new end there in a static CRT; what system DLLs and drivers allocate
// (their own imports) is in neither count.
//
// The file (tools/profiling.py reads it; tools/capture/patcap.c writes the same form for Tibia.exe):
//	# patella-profile 1
//	# freq <the performance counter's ticks per second>
//	# dropped <frames beyond the window's room>
//	F <counter at Present's entry> <Present's ticks> <allocations> <frees> <bytes asked for> <of them up to 1016 bytes>
//	G <nanoseconds of a frame's device time>

#include "common.h"
#include "testmode/ProfileWin32.h"
#include "vulkan/Renderer.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace {

struct TSample {
	std::uint64_t Start;
	std::uint32_t Present;
	std::uint32_t Allocations;
	std::uint32_t Frees;
	std::uint32_t Small;
	std::uint64_t Bytes;
};

constexpr std::size_t MaxSamples = 400000; // 40 s at 10,000 frames per second
constexpr std::size_t MaxGpuTimes = MaxSamples;
constexpr SIZE_T SmallAllocation = 1016; // the small-block heap's limit in the original's CRT

using THeapAlloc = LPVOID(WINAPI *)(HANDLE, DWORD, SIZE_T);
using THeapReAlloc = LPVOID(WINAPI *)(HANDLE, DWORD, LPVOID, SIZE_T);
using THeapFree = BOOL(WINAPI *)(HANDLE, DWORD, LPVOID);

THeapAlloc RealHeapAlloc = nullptr;
THeapReAlloc RealHeapReAlloc = nullptr;
THeapFree RealHeapFree = nullptr;

std::atomic<bool> Counting{false};
std::atomic<std::uint64_t> Allocations{0};
std::atomic<std::uint64_t> Frees{0};
std::atomic<std::uint64_t> Bytes{0};
std::atomic<std::uint64_t> Small{0};

LPVOID WINAPI HookHeapAlloc(HANDLE Heap, DWORD Flags, SIZE_T Size)
{
	if (Counting.load(std::memory_order_relaxed)) {
		Allocations.fetch_add(1, std::memory_order_relaxed);
		Bytes.fetch_add(Size, std::memory_order_relaxed);
		if (Size <= SmallAllocation)
			Small.fetch_add(1, std::memory_order_relaxed);
	}
	return RealHeapAlloc(Heap, Flags, Size);
}

LPVOID WINAPI HookHeapReAlloc(HANDLE Heap, DWORD Flags, LPVOID Memory, SIZE_T Size)
{
	if (Counting.load(std::memory_order_relaxed)) {
		Allocations.fetch_add(1, std::memory_order_relaxed);
		Bytes.fetch_add(Size, std::memory_order_relaxed);
	}
	return RealHeapReAlloc(Heap, Flags, Memory, Size);
}

BOOL WINAPI HookHeapFree(HANDLE Heap, DWORD Flags, LPVOID Memory)
{
	if (Counting.load(std::memory_order_relaxed))
		Frees.fetch_add(1, std::memory_order_relaxed);
	return RealHeapFree(Heap, Flags, Memory);
}

// The executable's imports of the three heap functions, patched once; false when one of them is missing.
bool PatchHeapImports()
{
	static bool Patched = false;
	if (Patched)
		return true;
	auto *Base = reinterpret_cast<BYTE *>(GetModuleHandleW(nullptr));
	const auto *Dos = reinterpret_cast<IMAGE_DOS_HEADER *>(Base);
	const auto *Nt = reinterpret_cast<IMAGE_NT_HEADERS *>(Base + Dos->e_lfanew);
	const IMAGE_DATA_DIRECTORY &Directory = Nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	int Found = 0;
	for (auto *Import = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(Base + Directory.VirtualAddress); Import->Name != 0;
		Import++) {
		if (_stricmp(reinterpret_cast<const char *>(Base + Import->Name), "kernel32.dll") != 0)
			continue;
		auto *Names = reinterpret_cast<IMAGE_THUNK_DATA *>(Base + Import->OriginalFirstThunk);
		auto *Slots = reinterpret_cast<IMAGE_THUNK_DATA *>(Base + Import->FirstThunk);
		for (; Names->u1.AddressOfData != 0; Names++, Slots++) {
			if (IMAGE_SNAP_BY_ORDINAL(Names->u1.Ordinal))
				continue;
			const char *Name = reinterpret_cast<const char *>(
				reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(Base + Names->u1.AddressOfData)->Name);
			void *Hook = nullptr;
			if (strcmp(Name, "HeapAlloc") == 0) {
				RealHeapAlloc = reinterpret_cast<THeapAlloc>(Slots->u1.Function);
				Hook = reinterpret_cast<void *>(&HookHeapAlloc);
			} else if (strcmp(Name, "HeapReAlloc") == 0) {
				RealHeapReAlloc = reinterpret_cast<THeapReAlloc>(Slots->u1.Function);
				Hook = reinterpret_cast<void *>(&HookHeapReAlloc);
			} else if (strcmp(Name, "HeapFree") == 0) {
				RealHeapFree = reinterpret_cast<THeapFree>(Slots->u1.Function);
				Hook = reinterpret_cast<void *>(&HookHeapFree);
			}
			if (Hook == nullptr)
				continue;
			DWORD Old = 0;
			if (!VirtualProtect(&Slots->u1.Function, sizeof(Slots->u1.Function), PAGE_READWRITE, &Old))
				return false;
			Slots->u1.Function = reinterpret_cast<ULONGLONG>(Hook);
			VirtualProtect(&Slots->u1.Function, sizeof(Slots->u1.Function), Old, &Old);
			Found++;
		}
	}
	Patched = Found == 3;
	return Patched;
}

struct TWindow {
	bool Open = false;
	bool Gpu = false;
	bool TimestampsOn = false;
	std::string Path;
	std::vector<TSample> Samples;
	std::vector<std::uint64_t> GpuTimes;
	std::uint64_t Dropped = 0;
	std::uint64_t Frequency = 0;
	// At Present's entry.
	LARGE_INTEGER PresentStart{};
	std::uint64_t LastAllocations = 0;
	std::uint64_t LastFrees = 0;
	std::uint64_t LastBytes = 0;
	std::uint64_t LastSmall = 0;
};

TWindow &Window()
{
	static TWindow Instance;
	return Instance;
}

std::string Start(const std::string &Kind, const std::string &Path)
{
	TWindow &W = Window();
	if (W.Open)
		return "a profile window is open";
	const bool Allocs = Kind == "allocs";
	if (Kind != "time" && !Allocs && Kind != "gpu")
		return "the kinds are time, allocs and gpu";
	if (Allocs && !PatchHeapImports())
		return "the executable does not import HeapAlloc, HeapReAlloc and HeapFree from KERNEL32";
	W.Path = Path;
	W.Gpu = Kind == "gpu";
	W.Samples.clear();
	W.Samples.reserve(MaxSamples);
	W.GpuTimes.clear();
	if (W.Gpu)
		W.GpuTimes.reserve(MaxGpuTimes);
	W.Dropped = 0;
	LARGE_INTEGER Frequency{};
	QueryPerformanceFrequency(&Frequency);
	W.Frequency = static_cast<std::uint64_t>(Frequency.QuadPart);
	W.LastAllocations = Allocations.load(std::memory_order_relaxed);
	W.LastFrees = Frees.load(std::memory_order_relaxed);
	W.LastBytes = Bytes.load(std::memory_order_relaxed);
	W.LastSmall = Small.load(std::memory_order_relaxed);
	W.Open = true;
	Counting.store(Allocs, std::memory_order_relaxed);
	return {};
}

std::string Stop()
{
	TWindow &W = Window();
	if (!W.Open)
		return "no profile window is open";
	W.Open = false;
	Counting.store(false, std::memory_order_relaxed);
	std::ofstream File(W.Path, std::ios::out | std::ios::trunc);
	File << "# patella-profile 1\n# freq " << W.Frequency << "\n# dropped " << W.Dropped << '\n';
	for (const TSample &S : W.Samples)
		File << "F " << S.Start << ' ' << S.Present << ' ' << S.Allocations << ' ' << S.Frees << ' ' << S.Bytes << ' '
			 << S.Small << '\n';
	for (const std::uint64_t Nanoseconds : W.GpuTimes)
		File << "G " << Nanoseconds << '\n';
	File.close();
	const bool Failed = !File;
	W.Samples = {};
	W.GpuTimes = {};
	return Failed ? "cannot write " + W.Path : std::string{};
}

} // namespace

namespace patella::testmode {

std::string ProfileCommand(const std::string &Text)
{
	if (Text == "profile stop")
		return Stop();
	// "profile start <kind> <file>"; the file is the rest of the line (it may hold spaces).
	const std::string Prefix = "profile start ";
	if (!Text.starts_with(Prefix))
		return "profile: start <kind> <file>, or stop";
	const std::size_t Space = Text.find(' ', Prefix.size());
	if (Space == std::string::npos || Space + 1 >= Text.size())
		return "profile: start <kind> <file>";
	return Start(Text.substr(Prefix.size(), Space - Prefix.size()), Text.substr(Space + 1));
}

void ProfilePresentBegin(patella::vulkan::TRenderer *Renderer)
{
	TWindow &W = Window();
	if (Renderer != nullptr && W.TimestampsOn != (W.Open && W.Gpu)) {
		W.TimestampsOn = W.Open && W.Gpu && Renderer->SetFrameTimestamps(true);
		if (!W.TimestampsOn)
			Renderer->SetFrameTimestamps(false);
	}
	if (W.Open)
		QueryPerformanceCounter(&W.PresentStart);
}

void ProfilePresentEnd(patella::vulkan::TRenderer *Renderer)
{
	TWindow &W = Window();
	if (!W.Open)
		return;
	LARGE_INTEGER End{};
	QueryPerformanceCounter(&End);
	const std::uint64_t Allocated = Allocations.load(std::memory_order_relaxed);
	const std::uint64_t Freed = Frees.load(std::memory_order_relaxed);
	const std::uint64_t Asked = Bytes.load(std::memory_order_relaxed);
	const std::uint64_t Smalls = Small.load(std::memory_order_relaxed);
	if (W.Samples.size() < MaxSamples) {
		W.Samples.push_back({static_cast<std::uint64_t>(W.PresentStart.QuadPart),
			static_cast<std::uint32_t>(End.QuadPart - W.PresentStart.QuadPart), static_cast<std::uint32_t>(Allocated - W.LastAllocations),
			static_cast<std::uint32_t>(Freed - W.LastFrees), static_cast<std::uint32_t>(Smalls - W.LastSmall), Asked - W.LastBytes});
	} else {
		W.Dropped++;
	}
	W.LastAllocations = Allocated;
	W.LastFrees = Freed;
	W.LastBytes = Asked;
	W.LastSmall = Smalls;
	if (W.TimestampsOn && Renderer != nullptr && W.GpuTimes.size() < MaxGpuTimes)
		Renderer->TakeFrameTimes(W.GpuTimes);
}

} // namespace patella::testmode
