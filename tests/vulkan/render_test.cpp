// tests_vulkan: the Vulkan back end headless (tests/vulkan/CMakeLists.txt; notes/vulkan.md, "Checks").
//
// Each test draws with TRenderer into images that no window shows, reads them back and compares them
// with Model, a plain C++ model of what surfaceDX9.cpp's Direct3D 9 calls compute per pixel (pixel
// centres on integers, texture coordinates interpolated from the corners, point or bilinear sampling
// with wrapping, the texture times the vertex colour, source-alpha blending, ColorFill). Copies, fills
// and the colour key must be exact; a filtered stretch and the light may differ by 1 per channel (the
// hardware's filter weights and rounding). Every test runs on the CPU driver next to the executable
// and on the machine's best GPU, if it has one. One more lists the graphics device choice's devices
// (issue #31).
// Device switches (issue #52) and the handles they leave (issue #53) are checked at the end.
// The devices for a window get their surfaces through the engine's window systems (src/vulkan/WindowSystem.h, issue
// #126): SDL3's (SDL_Vulkan_CreateSurface on the device's window, the client's only path since issue #127). SDL's video
// subsystem is started in main, as the client starts it.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "vulkan/Renderer.h"

#include <volk.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <psapi.h>
#include <tlhelp32.h>
#include <winternl.h>

#include "vulkan/PresentWindow.h"
#include "vulkan/WindowSystem.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pv = patella::vulkan;

namespace {

struct TDevice {
	std::string Label;
	pv::TDeviceRequest Request;
};

std::vector<TDevice> Devices;

std::string CpuManifest()
{
	char Path[MAX_PATH];
	DWORD Length = GetModuleFileNameA(nullptr, Path, MAX_PATH);
	std::string Result(Path, Length);
	return Result.substr(0, Result.find_last_of("\\/") + 1) + "lvp_icd.x86_64.json";
}

// Request for a device that presents in Window (a device's window, PresentWindow.h), its surface made by System (the
// engine's window system, WindowSystem.h); every window a surface is made on goes to Surfaces when it is given.
pv::TDeviceRequest OnWindow(
	pv::TDeviceRequest Request, HWND Window, pv::IWindowSystem &System, std::vector<HWND> *Surfaces = nullptr)
{
	Request.SurfaceExtension = System.SurfaceExtension();
	Request.CreateSurface = [Window, &System, Surfaces](VkInstance Instance) {
		if (Surfaces != nullptr)
			Surfaces->push_back(Window);
		return System.CreateSurface(Instance, Window);
	};
	return Request;
}

// A hidden SDL3 window, as the client's main window on its default path (SdlFrame.cpp: SDL_CreateWindow, hidden and
// resizable), with its HWND; destroyed with the object.
class TSdlMainWindow {
public:
	TSdlMainWindow(int Width, int Height)
		: Window(SDL_CreateWindow("tests_vulkan", Width, Height, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE))
	{
		if (Window != nullptr)
			Handle = static_cast<HWND>(SDL_GetPointerProperty(
				SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
	}
	~TSdlMainWindow()
	{
		if (Window != nullptr)
			SDL_DestroyWindow(Window);
		SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
	}
	TSdlMainWindow(const TSdlMainWindow &) = delete;
	TSdlMainWindow &operator=(const TSdlMainWindow &) = delete;
	TSdlMainWindow(TSdlMainWindow &&) = delete;
	TSdlMainWindow &operator=(TSdlMainWindow &&) = delete;

	SDL_Window *Window = nullptr;
	HWND Handle = nullptr;
};

// The number of SDL's windows.
int SdlWindows()
{
	int Count = 0;
	SDL_free(static_cast<void *>(SDL_GetWindows(&Count)));
	return Count;
}

// A deterministic pseudo-random sequence (the pictures are the same in every run).
struct TRandom {
	uint32_t State;
	uint32_t Next()
	{
		State = State * 1664525u + 1013904223u;
		return State >> 8;
	}
};

uint32_t Channel(uint32_t Color, int Index) // 0 B, 1 G, 2 R, 3 A
{
	return (Color >> (Index * 8)) & 0xff;
}

// An image of the model: D3DCOLOR pixels, rows top-down.
struct TPicture {
	int Width = 0;
	int Height = 0;
	std::vector<uint32_t> Pixels;
	TPicture() = default;
	TPicture(int w, int h, uint32_t Value = 0) : Width(w), Height(h), Pixels(static_cast<size_t>(w) * static_cast<size_t>(h), Value) {}
	uint32_t &At(int x, int y) { return Pixels[static_cast<size_t>(y) * static_cast<size_t>(Width) + static_cast<size_t>(x)]; }
	uint32_t At(int x, int y) const
	{
		return Pixels[static_cast<size_t>(y) * static_cast<size_t>(Width) + static_cast<size_t>(x)];
	}
};

TPicture RandomPicture(int Width, int Height, uint32_t Seed, bool KeyedAlpha)
{
	TPicture Picture(Width, Height);
	TRandom Random{Seed};
	for (uint32_t &Pixel : Picture.Pixels) {
		uint32_t Color = Random.Next() & 0xffffff;
		bool Transparent = KeyedAlpha && (Random.Next() % 4) == 0;
		Pixel = Color | (Transparent ? 0u : 0xff000000u);
	}
	return Picture;
}

struct TSample {
	double c[4]; // B, G, R, A in 0-1
};

int Wrap(int Value, int Size)
{
	int Result = Value % Size;
	return Result < 0 ? Result + Size : Result;
}

TSample Fetch(const TPicture &Texture, int x, int y)
{
	uint32_t Pixel = Texture.At(Wrap(x, Texture.Width), Wrap(y, Texture.Height));
	TSample Result;
	for (int i = 0; i < 4; i++)
		Result.c[i] = Channel(Pixel, i) / 255.0;
	return Result;
}

// Direct3D 9's samplers at the texture coordinate (u, v): point reads floor(u * size), linear blends
// the four texels around (u * size - 0.5).
TSample Sample(const TPicture &Texture, double u, double v, pv::TFilter Filter)
{
	// The hardware snaps the coordinate to 1/256 of a texel first (Quads.cpp), so a coordinate that
	// floating point puts a hair below a texel edge reads the texel after it.
	if (Filter == pv::TFilter::Point)
		return Fetch(Texture, static_cast<int>(std::floor(std::round(u * Texture.Width * 256.0) / 256.0)),
			static_cast<int>(std::floor(std::round(v * Texture.Height * 256.0) / 256.0)));
	double s = u * Texture.Width - 0.5;
	double t = v * Texture.Height - 0.5;
	int x0 = static_cast<int>(std::floor(s));
	int y0 = static_cast<int>(std::floor(t));
	double fx = s - x0;
	double fy = t - y0;
	TSample a = Fetch(Texture, x0, y0), b = Fetch(Texture, x0 + 1, y0), c = Fetch(Texture, x0, y0 + 1),
			d = Fetch(Texture, x0 + 1, y0 + 1);
	TSample Result;
	for (int i = 0; i < 4; i++)
		Result.c[i] = (a.c[i] * (1 - fx) + b.c[i] * fx) * (1 - fy) + (c.c[i] * (1 - fx) + d.c[i] * fx) * fy;
	return Result;
}

uint32_t Pack(const double *c)
{
	uint32_t Result = 0;
	for (int i = 0; i < 4; i++) {
		double Value = std::clamp(c[i], 0.0, 1.0) * 255.0;
		Result |= static_cast<uint32_t>(std::lround(Value)) << (i * 8);
	}
	return Result;
}

// surfaceDX9.cpp's AddQuad as Direct3D 9 rasterises it: the pixels (x, y) with Left <= x < Right and
// Top <= y < Bottom; the first triangle (top left, top right, bottom right) above the diagonal, the
// second below; the attributes interpolated at the pixel centre (x, y) from the triangle's corners.
void ModelDraw(TPicture &Target, const TPicture &Texture, pv::TFilter Filter, const pv::TQuad &Q)
{
	double Width = Q.Right - Q.Left;
	double Height = Q.Bottom - Q.Top;
	for (int y = std::max(Q.Top, 0); y < std::min(Q.Bottom, Target.Height); y++) {
		for (int x = std::max(Q.Left, 0); x < std::min(Q.Right, Target.Width); x++) {
			double fx = (x - Q.Left) / Width;
			double fy = (y - Q.Top) / Height;
			double u = Q.LeftU + fx * (static_cast<double>(Q.RightU) - Q.LeftU);
			double v = Q.TopV + fy * (static_cast<double>(Q.BottomV) - Q.TopV);
			double Color[4];
			for (int i = 0; i < 4; i++) {
				double tl = Channel(Q.TopLeft, i) / 255.0, tr = Channel(Q.TopRight, i) / 255.0;
				double br = Channel(Q.BottomRight, i) / 255.0, bl = Channel(Q.BottomLeft, i) / 255.0;
				if (fx >= fy) // (top left, top right, bottom right)
					Color[i] = tl + fx * (tr - tl) + fy * (br - tr);
				else // (bottom right, bottom left, top left)
					Color[i] = tl + fy * (bl - tl) + fx * (br - bl);
			}
			TSample Texel = Sample(Texture, u, v, Filter);
			double Source[4];
			for (int i = 0; i < 4; i++)
				Source[i] = Texel.c[i] * Color[i];
			double Alpha = Source[3];
			if (Alpha == 0.0)
				continue;
			uint32_t Old = Target.At(x, y);
			double Result[4];
			for (int i = 0; i < 4; i++)
				Result[i] = Source[i] * Alpha + Channel(Old, i) / 255.0 * (1.0 - Alpha);
			Target.At(x, y) = Pack(Result);
		}
	}
}

void ModelFill(TPicture &Target, int Left, int Top, int Right, int Bottom, uint32_t Color)
{
	for (int y = Top; y < Bottom; y++)
		for (int x = Left; x < Right; x++)
			Target.At(x, y) = Color | 0xff000000u;
}

// A quad of a blit as surfaceDX9.cpp's BlitRectClipped and StretchBlitRect compute it.
pv::TQuad Blit(int x, int y, int Width, int Height, int FromX, int FromY, int FromWidth, int FromHeight,
	const TPicture &Texture)
{
	pv::TQuad Q{};
	Q.Left = x;
	Q.Top = y;
	Q.Right = x + Width;
	Q.Bottom = y + Height;
	Q.LeftU = static_cast<float>((FromX + 0.5) / Texture.Width);
	Q.RightU = static_cast<float>((FromX + FromWidth + 0.5) / Texture.Width);
	Q.TopV = static_cast<float>((FromY + 0.5) / Texture.Height);
	Q.BottomV = static_cast<float>((FromY + FromHeight + 0.5) / Texture.Height);
	Q.TopLeft = Q.TopRight = Q.BottomRight = Q.BottomLeft = 0xffffffffu;
	return Q;
}

struct TDifference {
	int Pixels = 0;  // pixels with a difference
	int Largest = 0; // the largest difference of a channel
};

TDifference Compare(const TPicture &Expected, const std::vector<uint32_t> &Actual)
{
	TDifference Result;
	for (size_t i = 0; i < Expected.Pixels.size(); i++) {
		int Largest = 0;
		for (int c = 0; c < 4; c++)
			Largest = std::max(Largest, std::abs(static_cast<int>(Channel(Expected.Pixels[i], c)) - static_cast<int>(Channel(Actual[i], c))));
		if (Largest > 0)
			Result.Pixels++;
		Result.Largest = std::max(Result.Largest, Largest);
	}
	return Result;
}

int Image(pv::TRenderer &Renderer, const TPicture &Picture)
{
	int Id = Renderer.CreateImage(Picture.Width, Picture.Height);
	Renderer.Upload(Id, 0, 0, Picture.Width, Picture.Height, Picture.Pixels.data(), static_cast<size_t>(Picture.Width));
	return Id;
}

std::vector<uint32_t> Read(pv::TRenderer &Renderer, int Id)
{
	std::vector<uint32_t> Pixels;
	Renderer.ReadBack(Id, 0, 0, Renderer.ImageWidth(Id), Renderer.ImageHeight(Id), Pixels);
	return Pixels;
}

void Report(const std::string &Device, const char *What, const TDifference &Difference)
{
	std::printf("%s: %s: %d pixels differ, by at most %d\n", Device.c_str(), What, Difference.Pixels,
		Difference.Largest);
}

} // namespace

TEST_CASE("the device")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		const pv::TDeviceInfo &Info = Renderer.Device();
		std::printf("%s: %s (type %d, Vulkan %u.%u.%u)\n", Device.Label.c_str(), Info.Name.c_str(),
			static_cast<int>(Info.Type), VK_API_VERSION_MAJOR(Info.ApiVersion), VK_API_VERSION_MINOR(Info.ApiVersion),
			VK_API_VERSION_PATCH(Info.ApiVersion));
		CHECK(VK_API_VERSION_MINOR(Info.ApiVersion) + VK_API_VERSION_MAJOR(Info.ApiVersion) * 100 >= 103);
		if (Device.Request.Kind == pv::TDeviceKind::Cpu) {
			CHECK(Info.CpuDriver);
			CHECK(Info.Type == VK_PHYSICAL_DEVICE_TYPE_CPU);
		} else {
			CHECK_FALSE(Info.CpuDriver);
			CHECK(Info.Type != VK_PHYSICAL_DEVICE_TYPE_CPU);
		}
		// A new image is cleared to 0.
		int Id = Renderer.CreateImage(13, 7);
		std::vector<uint32_t> Pixels = Read(Renderer, Id);
		CHECK(std::count(Pixels.begin(), Pixels.end(), 0u) == 13 * 7);
	}
}

// The graphics device choice's lists (issue #31): the GPUs the advanced options' GPU button goes through
// (ListGpus, the order GpuIndex counts in), the one "the best GPU" and "Automatic" mean, and the CPU
// driver's device; each listed GPU opens as its index, headless.
TEST_CASE("the device choice: GPUs, the best one, the CPU driver")
{
	std::vector<pv::TDeviceInfo> Gpus = pv::ListGpus();
	int Best = pv::BestGpu(Gpus);
	std::printf("device choice: %zu GPU(s), the best %d\n", Gpus.size(), Best);
	for (size_t i = 0; i < Gpus.size(); i++) {
		std::printf("device choice: gpu%zu %s (type %d)\n", i, Gpus[i].Name.c_str(), static_cast<int>(Gpus[i].Type));
		CHECK(Gpus[i].Type != VK_PHYSICAL_DEVICE_TYPE_CPU);
		CHECK_FALSE(Gpus[i].CpuDriver);
	}
	bool HasGpu = std::any_of(Devices.begin(), Devices.end(),
		[](const TDevice &Device) { return Device.Request.Kind == pv::TDeviceKind::Gpu; });
	CHECK(HasGpu == !Gpus.empty());
	CHECK((Best >= 0) == !Gpus.empty());

	std::optional<pv::TDeviceInfo> Cpu = pv::CpuDriverDevice(CpuManifest());
	REQUIRE(Cpu.has_value());
	std::printf("device choice: cpu %s\n", Cpu->Name.c_str());
	CHECK(Cpu->CpuDriver);
	CHECK(Cpu->Type == VK_PHYSICAL_DEVICE_TYPE_CPU);
	CHECK(Cpu->Name.find("llvmpipe") != std::string::npos);
	CHECK_FALSE(pv::CpuDriverDevice("").has_value());
	CHECK_FALSE(pv::CpuDriverDevice(CpuManifest() + ".missing").has_value());

	// Automatic: the best GPU, else the CPU driver.
	{
		pv::TDeviceRequest Request;
		Request.Kind = pv::TDeviceKind::Automatic;
		Request.CpuDriverManifest = CpuManifest();
		pv::TRenderer Renderer;
		Renderer.Create(Request);
		std::printf("device choice: auto opens %s\n", Renderer.Device().Name.c_str());
		if (Gpus.empty())
			CHECK(Renderer.Device().CpuDriver);
		else
			CHECK(Renderer.Device().Name == Gpus[static_cast<size_t>(Best)].Name);
	}
	// gpu (the best) and every gpuN.
	for (int Index = Gpus.empty() ? 0 : -1; Index < static_cast<int>(Gpus.size()); Index++) {
		CAPTURE(Index);
		pv::TDeviceRequest Request;
		Request.Kind = pv::TDeviceKind::Gpu;
		Request.GpuIndex = Index;
		pv::TRenderer Renderer;
		Renderer.Create(Request);
		size_t Expected = static_cast<size_t>(Index >= 0 ? Index : Best);
		CHECK(Renderer.Device().Name == Gpus[Expected].Name);
		CHECK_FALSE(Renderer.Device().CpuDriver);
	}
	// A GPU index beyond the list opens nothing (the client then falls back to Automatic).
	{
		pv::TDeviceRequest Request;
		Request.Kind = pv::TDeviceKind::Gpu;
		Request.GpuIndex = static_cast<int>(Gpus.size());
		pv::TRenderer Renderer;
		CHECK_THROWS_AS(Renderer.Create(Request), pv::TVulkanError);
		CHECK_FALSE(Renderer.Created());
	}
}

// A machine with a Vulkan loader and no system driver at all (issue #163: GitHub's runner). VK_DRIVER_FILES naming a file
// that does not exist gives any machine the same state: the loader finds no driver, listing no surface extension, until
// the engine names the CPU driver's manifest for its own instance.
TEST_CASE("the automatic choice without any system driver opens the CPU driver (issue #163)")
{
	class TNoSystemDriver {
	public:
		TNoSystemDriver()
		{
			for (size_t i = 0; i < Names.size(); i++) {
				char *Value = nullptr;
				size_t Length = 0;
				if (_dupenv_s(&Value, &Length, Names[i]) == 0 && Value != nullptr) {
					Saved[i] = Value;
					std::free(Value);
				}
				_putenv_s(Names[i], (CpuManifest() + ".missing").c_str());
			}
		}
		~TNoSystemDriver()
		{
			for (size_t i = 0; i < Names.size(); i++)
				_putenv_s(Names[i], Saved[i].c_str()); // an empty value removes the variable
		}
		TNoSystemDriver(const TNoSystemDriver &) = delete;
		TNoSystemDriver &operator=(const TNoSystemDriver &) = delete;

	private:
		const std::array<const char *, 2> Names = {"VK_DRIVER_FILES", "VK_ICD_FILENAMES"};
		std::array<std::string, 2> Saved;
	};
	const TNoSystemDriver NoSystemDriver;
	CHECK(pv::ListGpus().empty());

	pv::TDeviceRequest Automatic;
	Automatic.Kind = pv::TDeviceKind::Automatic;
	Automatic.CpuDriverManifest = CpuManifest();
	// Headless (the system drivers' instance fails: failed_create_instance) ...
	{
		pv::TRenderer Renderer;
		REQUIRE_NOTHROW(Renderer.Create(Automatic));
		CHECK(Renderer.Device().CpuDriver);
	}
	// ... for a window (it fails sooner: requested_extensions_not_present) ...
	{
		const TSdlMainWindow Main(160, 120);
		REQUIRE(Main.Handle != nullptr);
		pv::TPresentWindows Windows;
		HWND Window = Windows.Open(Main.Handle);
		REQUIRE(Window != nullptr);
		{
			pv::TRenderer Renderer;
			REQUIRE_NOTHROW(Renderer.Create(OnWindow(Automatic, Window, pv::SdlWindowSystem())));
			CHECK(Renderer.Device().CpuDriver);
		}
		Windows.Close();
	}
	// ... and a GPU asked for by name still opens nothing (the client then falls back to Automatic).
	{
		pv::TDeviceRequest Gpu;
		Gpu.Kind = pv::TDeviceKind::Gpu;
		pv::TRenderer Renderer;
		CHECK_THROWS_AS(Renderer.Create(Gpu), pv::TVulkanError);
		CHECK_FALSE(Renderer.Created());
	}
}

// The client's start-up order (issue #31's crash, fixed in issue-31-fix): headless instances first (the
// device list, and every test above), then a device for a window, whose selection asks whether the
// device can present to the window's surface. vk-bootstrap loads its instance functions once, from the
// first instance the process creates; when that one had no surface extension, the surface functions were
// null and the selection crashed (an access violation in get_present_queue_index). The window is never
// shown (no WS_VISIBLE, no ShowWindow) and is destroyed at the end; nothing appears on the desktop.
TEST_CASE("a device for a window after headless instances (the start-up order)")
{
	REQUIRE_NOTHROW(pv::ListGpus());
	REQUIRE_NOTHROW(pv::CpuDriverDevice(CpuManifest()));
	// A device and its first frame on a device window of Parent, its surface through System.
	auto OnEveryDevice = [](HWND Parent, pv::IWindowSystem &System, const char *Path) {
		pv::TPresentWindows Windows;
		HWND Window = Windows.Open(Parent);
		REQUIRE(Window != nullptr);
		for (TDevice &Device : Devices) {
			CAPTURE(Device.Label);
			CAPTURE(Path);
			pv::TRenderer Renderer;
			REQUIRE_NOTHROW(Renderer.Create(OnWindow(Device.Request, Window, System)));
			std::printf("%s: a device for a hidden window (%s): %s\n", Device.Label.c_str(), Path,
				Renderer.Device().Name.c_str());
			// The swapchain (vk-bootstrap's surface format and present mode queries) on the first frame.
			const int Image = Renderer.CreateImage(160, 120);
			Renderer.Fill(Image, 0, 0, 160, 120, 0xff204060u);
			const pv::TWindowSize Client = System.ClientSize(Parent);
			CHECK(Client.Width > 0);
			CHECK(Client.Height > 0);
			CHECK_NOTHROW(Renderer.Present(
				Image, 0, 0, 160, 120, static_cast<uint32_t>(Client.Width), static_cast<uint32_t>(Client.Height)));
			Renderer.Finish();
		}
		Windows.Close();
	};

	// The SDL3 window: the main window SDL's, the surface SDL_Vulkan_CreateSurface's.
	{
		const TSdlMainWindow Main(160, 120);
		REQUIRE(Main.Handle != nullptr);
		CHECK_FALSE(IsWindowVisible(Main.Handle));
		OnEveryDevice(Main.Handle, pv::SdlWindowSystem(), "SDL3");
		CHECK_FALSE(IsWindowVisible(Main.Handle));
	}
}

// The SDL3 window system (issue #126): the client area and the screen as Win32 gives them (the process is not DPI
// aware, as the client), the surface made on a device's window through a wrapper SDL window that is gone again with
// the call (SDL's window procedure and data off the device's window), never on one of SDL's own windows.
TEST_CASE("the SDL3 window system: sizes, and SDL_Vulkan_CreateSurface on a device's window (issue #126)")
{
	pv::IWindowSystem &System = pv::SdlWindowSystem();
	const TSdlMainWindow Main(320, 240);
	REQUIRE(Main.Handle != nullptr);
	RECT Client = {};
	GetClientRect(Main.Handle, &Client);
	CHECK(System.ClientSize(Main.Handle).Width == Client.right);
	CHECK(System.ClientSize(Main.Handle).Height == Client.bottom);
	SetWindowPos(Main.Handle, nullptr, 0, 0, 410, 330, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	GetClientRect(Main.Handle, &Client);
	CHECK(System.ClientSize(Main.Handle).Width == Client.right);
	CHECK(System.ClientSize(Main.Handle).Height == Client.bottom);
	CHECK(System.ClientSize(nullptr).Width == 0);
	CHECK(System.ClientSize(nullptr).Height == 0);
	CHECK(System.ScreenSize().Width == GetSystemMetrics(SM_CXSCREEN));
	CHECK(System.ScreenSize().Height == GetSystemMetrics(SM_CYSCREEN));
	CHECK(System.SurfaceExtension() == "VK_KHR_win32_surface");

	pv::TPresentWindows Windows;
	HWND Window = Windows.Open(Main.Handle);
	REQUIRE(Window != nullptr);
	CHECK(System.ClientSize(Window).Width == 0); // not one of SDL's windows
	const LONG_PTR Procedure = GetWindowLongPtrA(Window, GWLP_WNDPROC);
	const int Before = SdlWindows();
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		REQUIRE_NOTHROW(Renderer.Create(OnWindow(Device.Request, Window, System)));
		CHECK(SdlWindows() == Before);
		CHECK(GetWindowLongPtrA(Window, GWLP_WNDPROC) == Procedure);
		CHECK(GetPropA(Window, "SDL_WindowData") == nullptr);
		CHECK(SendMessageA(Window, WM_NCHITTEST, 0, MAKELPARAM(10, 10)) == HTTRANSPARENT);
		const int Image = Renderer.CreateImage(64, 64);
		Renderer.Fill(Image, 0, 0, 64, 64, 0xff102030u);
		const pv::TWindowSize Size = System.ClientSize(Main.Handle);
		CHECK_NOTHROW(Renderer.Present(
			Image, 0, 0, 64, 64, static_cast<uint32_t>(Size.Width), static_cast<uint32_t>(Size.Height)));
		Renderer.Finish();
		// SDL's own window is refused.
		pv::TRenderer Refused;
		CHECK_THROWS_AS(Refused.Create(OnWindow(Device.Request, Main.Handle, System)), pv::TVulkanError);
		CHECK_FALSE(Refused.Created());
	}
	Windows.Close();
	CHECK(SdlWindows() == Before);
}

TEST_CASE("colour fills are exact (ColorFill)")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		TPicture Expected(64, 48);
		int Id = Renderer.CreateImage(64, 48);
		struct {
			int l, t, r, b;
			uint32_t c;
		} Fills[] = {{0, 0, 64, 48, 0x102030}, {5, 6, 20, 30, 0x00aabbcc}, {63, 47, 64, 48, 0x010203}, {10, 10, 11, 40, 0xfefdfc},
			{0, 20, 64, 21, 0x808080}};
		for (auto &F : Fills) {
			Renderer.Fill(Id, F.l, F.t, F.r, F.b, F.c);
			ModelFill(Expected, F.l, F.t, F.r, F.b, F.c);
		}
		TDifference Difference = Compare(Expected, Read(Renderer, Id));
		Report(Device.Label, "fills", Difference);
		CHECK(Difference.Pixels == 0);
	}
}

TEST_CASE("unscaled copies with alpha 0 texels are exact (BlitRectClipped)")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		TPicture Sprites = RandomPicture(512, 512, 1, true);
		TPicture Expected = RandomPicture(640, 480, 2, false);
		int Texture = Image(Renderer, Sprites);
		int Target = Image(Renderer, Expected);
		std::vector<pv::TQuad> Quads;
		TRandom Random{3};
		for (int i = 0; i < 400; i++) {
			int Width = 1 + static_cast<int>(Random.Next() % 64);
			int Height = 1 + static_cast<int>(Random.Next() % 64);
			int FromX = static_cast<int>(Random.Next() % static_cast<uint32_t>(512 - Width));
			int FromY = static_cast<int>(Random.Next() % static_cast<uint32_t>(512 - Height));
			int x = static_cast<int>(Random.Next() % static_cast<uint32_t>(640 - Width));
			int y = static_cast<int>(Random.Next() % static_cast<uint32_t>(480 - Height));
			Quads.push_back(Blit(x, y, Width, Height, FromX, FromY, Width, Height, Sprites));
		}
		Renderer.Draw(Target, Texture, pv::TFilter::Linear, Quads.data(), Quads.size());
		for (const pv::TQuad &Q : Quads)
			ModelDraw(Expected, Sprites, pv::TFilter::Linear, Q);
		TDifference Difference = Compare(Expected, Read(Renderer, Target));
		Report(Device.Label, "400 copies of 512x512 texture quads", Difference);
		CHECK(Difference.Pixels == 0);
	}
}

// Issue #155: Draw reuses its working copies of the quads. A batch smaller than the one before draws only its own quads.
TEST_CASE("a small batch after a large one draws only its own quads")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		TPicture Sprites = RandomPicture(512, 512, 4, true);
		TPicture Expected = RandomPicture(640, 480, 5, false);
		const int Texture = Image(Renderer, Sprites);
		const int Target = Image(Renderer, Expected);
		TRandom Random{6};
		for (const size_t Count : {size_t{400}, size_t{1}, size_t{3}, size_t{0}, size_t{50}, size_t{2}}) {
			std::vector<pv::TQuad> Quads;
			for (size_t i = 0; i < Count; i++) {
				const int Width = 1 + static_cast<int>(Random.Next() % 64);
				const int Height = 1 + static_cast<int>(Random.Next() % 64);
				const int FromX = static_cast<int>(Random.Next() % static_cast<uint32_t>(512 - Width));
				const int FromY = static_cast<int>(Random.Next() % static_cast<uint32_t>(512 - Height));
				const int x = static_cast<int>(Random.Next() % static_cast<uint32_t>(640 - Width));
				const int y = static_cast<int>(Random.Next() % static_cast<uint32_t>(480 - Height));
				Quads.push_back(Blit(x, y, Width, Height, FromX, FromY, Width, Height, Sprites));
			}
			Renderer.Draw(Target, Texture, pv::TFilter::Linear, Quads.data(), Quads.size());
			for (const pv::TQuad &Q : Quads)
				ModelDraw(Expected, Sprites, pv::TFilter::Linear, Q);
			TDifference Difference = Compare(Expected, Read(Renderer, Target));
			Report(Device.Label, "batches of different sizes in turn", Difference);
			CHECK(Difference.Pixels == 0);
		}
	}
}

TEST_CASE("the automap's whole-texture copy (point sampling, u 0 to 1)")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		// Each texel holds its own coordinates (x in red, y in green), so the read-back tells which
		// texel every pixel shows. The samples fall on the texel edges: Tibia.exe's DirectX 9 frames
		// (NVIDIA, issue #19's live comparison) show texel k - 1 at pixel k; at pixel 0 the coordinate is
		// the corner's own, texel 0. The engine gives every device that rule (an Edge copy, Quads.cpp).
		TPicture Source(106, 106);
		for (int y = 0; y < 106; y++)
			for (int x = 0; x < 106; x++)
				Source.At(x, y) = 0xff000000u | static_cast<uint32_t>(x) << 16 | static_cast<uint32_t>(y) << 8;
		int Texture = Image(Renderer, Source);
		pv::TQuad Q{0, 0, 106, 106, 0.0f, 1.0f, 0.0f, 1.0f, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
		int Target = Renderer.CreateImage(106, 106);
		Renderer.Draw(Target, Texture, pv::TFilter::Point, &Q, 1);
		std::vector<uint32_t> Pixels = Read(Renderer, Target);
		int Wrong = 0;
		for (int y = 0; y < 106; y++)
			for (int x = 0; x < 106; x++)
				if (Pixels[static_cast<size_t>(y) * 106 + static_cast<size_t>(x)]
					!= Source.At(std::max(x - 1, 0), std::max(y - 1, 0)))
					Wrong++;
		auto Texel = [&](int x, int y) {
			uint32_t P = Pixels[static_cast<size_t>(y) * 106 + static_cast<size_t>(x)];
			return std::to_string((P >> 16) & 0xff) + "," + std::to_string((P >> 8) & 0xff);
		};
		std::printf("%s: the automap upload: %d of 106x106 pixels not texel (max(x-1,0), max(y-1,0)); pixel (0,0) shows %s, (1,1) %s, "
					"(105,105) %s\n",
			Device.Label.c_str(), Wrong, Texel(0, 0).c_str(), Texel(1, 1).c_str(), Texel(105, 105).c_str());
		CHECK(Wrong == 0);
		TDifference Difference;
		// The same quad with linear filtering reads between the texels (half of each, the first column
		// wrapping to the last): within 1 of the model.
		int Linear = Renderer.CreateImage(106, 106);
		Renderer.Draw(Linear, Texture, pv::TFilter::Linear, &Q, 1);
		TPicture Blended(106, 106);
		ModelDraw(Blended, Source, pv::TFilter::Linear, Q);
		Difference = Compare(Blended, Read(Renderer, Linear));
		Report(Device.Label, "the same, linear", Difference);
		CHECK(Difference.Largest <= 1);
	}
}

TEST_CASE("stretched copies through bilinear filtering (StretchBlitRect)")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		TPicture Temp = RandomPicture(640, 480, 5, false);
		int Texture = Image(Renderer, Temp);
		int Target = Renderer.CreateImage(64, 64);
		TPicture Expected(64, 64);
		pv::TQuad Quads[] = {Blit(0, 0, 48, 48, 0, 0, 32, 32, Temp), Blit(48, 0, 16, 20, 40, 3, 32, 32, Temp),
			Blit(0, 48, 64, 16, 100, 100, 7, 3, Temp)};
		Renderer.Draw(Target, Texture, pv::TFilter::Linear, Quads, 3);
		for (const pv::TQuad &Q : Quads)
			ModelDraw(Expected, Temp, pv::TFilter::Linear, Q);
		TDifference Difference = Compare(Expected, Read(Renderer, Target));
		Report(Device.Label, "stretches", Difference);
		CHECK(Difference.Largest <= 1);
	}
}

TEST_CASE("the colour key pass is exact (ApplyColorKey)")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		const uint32_t Key = 0xff00ff00u;
		TPicture Picture = RandomPicture(80, 70, 6, false);
		TRandom Random{7};
		for (uint32_t &Pixel : Picture.Pixels) {
			uint32_t r = Random.Next() % 5;
			if (r == 0)
				Pixel = Key; // replaced
			else if (r == 1)
				Pixel = Key & 0x00ffffffu; // the key's colour with alpha 0: kept
		}
		int Id = Image(Renderer, Picture);
		Renderer.ReplaceColor(Id, 0, 0, 64, 64, Key, 0x00ffffffu);
		TPicture Expected = Picture;
		for (int y = 0; y < 64; y++)
			for (int x = 0; x < 64; x++)
				if (Expected.At(x, y) == Key)
					Expected.At(x, y) = 0x00ffffffu;
		TDifference Difference = Compare(Expected, Read(Renderer, Id));
		Report(Device.Label, "the colour key", Difference);
		CHECK(Difference.Pixels == 0);
		// At an offset, twice (the scratch image is reused).
		Renderer.ReplaceColor(Id, 10, 5, 70, 65, Key, 0x12345678u);
		for (int y = 5; y < 70; y++)
			for (int x = 10; x < 80; x++)
				if (Expected.At(x, y) == Key)
					Expected.At(x, y) = 0x12345678u;
		Difference = Compare(Expected, Read(Renderer, Id));
		CHECK(Difference.Pixels == 0);
	}
}

TEST_CASE("the light: shaded rectangles over a copy (ShadeRectangles)")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		TPicture Temp = RandomPicture(640, 480, 8, false);
		int TempId = Image(Renderer, Temp);
		int Primary = Renderer.CreateImage(640, 480);
		// FlushShade: TEMP_SURFACE to PRIMARY_SURFACE, then the rectangles into TEMP_SURFACE.
		pv::TQuad Copy = Blit(0, 0, 640, 480, 0, 0, 640, 480, Temp);
		Renderer.Draw(Primary, TempId, pv::TFilter::Linear, &Copy, 1);
		TPicture PrimaryModel(640, 480);
		ModelDraw(PrimaryModel, Temp, pv::TFilter::Linear, Copy);
		CHECK(Compare(PrimaryModel, Read(Renderer, Primary)).Pixels == 0);
		std::vector<pv::TQuad> Shade;
		TRandom Random{9};
		for (int y = 0; y < 480; y += 32) {
			for (int x = 0; x < 640; x += 32) {
				pv::TQuad Q = Blit(x, y, 32, 32, x, y, 32, 32, Temp);
				Q.TopLeft = 0xff000000u | (Random.Next() & 0xffffff);
				Q.TopRight = 0xff000000u | (Random.Next() & 0xffffff);
				Q.BottomRight = 0xff000000u | (Random.Next() & 0xffffff);
				Q.BottomLeft = 0xff000000u | (Random.Next() & 0xffffff);
				Shade.push_back(Q);
			}
		}
		Renderer.Draw(TempId, Primary, pv::TFilter::Linear, Shade.data(), Shade.size());
		TPicture Expected = Temp;
		for (const pv::TQuad &Q : Shade)
			ModelDraw(Expected, PrimaryModel, pv::TFilter::Linear, Q);
		TDifference Difference = Compare(Expected, Read(Renderer, TempId));
		Report(Device.Label, "the light", Difference);
		CHECK(Difference.Largest <= 1);
	}
}

TEST_CASE("a draw that samples its own target reads it as it was")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		TPicture Picture = RandomPicture(128, 96, 10, false);
		int Id = Image(Renderer, Picture);
		pv::TQuad Quads[] = {Blit(20, 20, 40, 30, 0, 0, 40, 30, Picture), Blit(30, 25, 40, 30, 10, 5, 40, 30, Picture)};
		Renderer.Draw(Id, Id, pv::TFilter::Linear, Quads, 2);
		TPicture Expected = Picture;
		for (const pv::TQuad &Q : Quads)
			ModelDraw(Expected, Picture, pv::TFilter::Linear, Q);
		CHECK(Compare(Expected, Read(Renderer, Id)).Pixels == 0);
	}
}

TEST_CASE("frames, large uploads and a resized image")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		int Id = Renderer.CreateImage(1200, 1000);
		for (int Frame = 0; Frame < 6; Frame++) {
			// 4.8 MB: more than one staging buffer.
			TPicture Picture = RandomPicture(1200, 1000, 20 + static_cast<uint32_t>(Frame), true);
			Renderer.Upload(Id, 0, 0, 1200, 1000, Picture.Pixels.data(), 1200);
			Renderer.Fill(Id, Frame, Frame, Frame + 10, Frame + 10, 0x445566);
			ModelFill(Picture, Frame, Frame, Frame + 10, Frame + 10, 0x445566);
			CHECK_FALSE(Renderer.Present(Id, 0, 0, 1200, 1000, 1200, 1000)); // headless: nothing shown
			CHECK(Compare(Picture, Read(Renderer, Id)).Pixels == 0);
		}
		Renderer.ResizeImage(Id, 50, 40);
		CHECK(Renderer.ImageWidth(Id) == 50);
		std::vector<uint32_t> Pixels = Read(Renderer, Id);
		CHECK(Pixels.size() == 50u * 40u);
		CHECK(std::count(Pixels.begin(), Pixels.end(), 0u) == 50 * 40);
		Renderer.Finish();
	}
}

TEST_CASE("frame timestamps: none by default, each frame's device time when asked for (issue #134)")
{
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TRenderer Renderer;
		Renderer.Create(Device.Request);
		const int Id = Renderer.CreateImage(64, 64);
		const auto Frames = [&](int Count) {
			for (int Frame = 0; Frame < Count; Frame++) {
				Renderer.Fill(Id, 0, 0, 64, 64, 0x102030u + static_cast<uint32_t>(Frame));
				CHECK_FALSE(Renderer.Present(Id, 0, 0, 64, 64, 64, 64));
			}
		};
		std::vector<uint64_t> Times;
		Frames(4);
		Renderer.TakeFrameTimes(Times);
		CHECK(Times.empty());
		if (!Renderer.SetFrameTimestamps(true)) {
			MESSAGE(Device.Label << ": no timestamps");
			continue;
		}
		Frames(8);
		Renderer.Finish();
		Renderer.TakeFrameTimes(Times);
		// The first frame began before they were on and the last ones may still be in flight.
		CHECK(Times.size() >= 5u);
		for (const uint64_t Nanoseconds : Times)
			CHECK(Nanoseconds < 10'000'000'000u);
		CHECK(Renderer.SetFrameTimestamps(false));
		Frames(4); // what was in flight
		Times.clear();
		Renderer.TakeFrameTimes(Times);
		Times.clear();
		Frames(3);
		Renderer.TakeFrameTimes(Times);
		CHECK(Times.empty());
	}
}

namespace {

int ChildWindows(HWND Parent)
{
	int Count = 0;
	EnumChildWindows(
		Parent,
		[](HWND, LPARAM Data) -> BOOL {
			++*reinterpret_cast<int *>(Data);
			return TRUE;
		},
		reinterpret_cast<LPARAM>(&Count));
	return Count;
}

// The main window of the input check below: as CMainFrame (MainFrm.cpp), a left button takes the mouse
// (SetCapture) and gives it back on its release; a loss of the capture in between is what releases the
// client's held buttons early (OnCaptureChanged), so it is counted.
struct TInputLog {
	int Downs = 0;
	int Ups = 0;
	int CaptureChanges = 0;
	bool Held = false;
};
TInputLog InputLog;

LRESULT CALLBACK InputWindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	switch (Message) {
	case WM_LBUTTONDOWN:
		InputLog.Downs++;
		InputLog.Held = true;
		SetCapture(Window);
		return 0;
	case WM_LBUTTONUP:
		InputLog.Ups++;
		if (InputLog.Held) {
			InputLog.Held = false;
			ReleaseCapture();
		}
		return 0;
	case WM_CAPTURECHANGED:
		if (InputLog.Held)
			InputLog.CaptureChanges++;
		return 0;
	default:
		return DefWindowProcA(Window, Message, WParam, LParam);
	}
}

// The test mode's Dispatch (src/testmode/TestModeWin32.cpp): the message as the message loop would hand it
// to the main window.
void DispatchToWindow(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	MSG Msg = {};
	Msg.hwnd = Window;
	Msg.message = Message;
	Msg.wParam = WParam;
	Msg.lParam = LParam;
	DispatchMessageA(&Msg);
}

} // namespace

// Issue #71: while Windows runs its modal size loop the client draws no frame and the device's window keeps
// its size (Fit comes with the next frame), so the part of the main window a larger window uncovers is the
// main window's own, whose class has no background brush. Its erase (WM_ERASEBKGND, which the resize's
// CS_HREDRAW/CS_VREDRAW invalidation brings) must fill that part black and leave the device windows' part
// alone, from the first Open to Close or the main window's end. No device is needed: the windows are never
// shown, and the erase is sent with a memory DC of the client area's size, white before each erase.
TEST_CASE("the main window's area the device windows do not cover is filled black (issue #71)")
{
	HINSTANCE Module = GetModuleHandleA(nullptr);
	WNDCLASSA Class = {};
	Class.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW; // the main window's (Player.cpp InitInstance)
	Class.lpfnWndProc = DefWindowProcA;
	Class.hInstance = Module;
	Class.lpszClassName = "PatellaTestsVulkanErase";
	RegisterClassA(&Class);
	auto NewParent = [&] {
		return CreateWindowExA(0, Class.lpszClassName, "tests_vulkan", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr,
			nullptr, Module, nullptr);
	};
	constexpr int Width = 640;
	constexpr int Height = 480;
	BITMAPINFO Info = {};
	Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
	Info.bmiHeader.biWidth = Width;
	Info.bmiHeader.biHeight = -Height;
	Info.bmiHeader.biPlanes = 1;
	Info.bmiHeader.biBitCount = 32;
	Info.bmiHeader.biCompression = BI_RGB;
	void *Bits = nullptr;
	HDC Context = CreateCompatibleDC(nullptr);
	HBITMAP Bitmap = CreateDIBSection(Context, &Info, DIB_RGB_COLORS, &Bits, nullptr, 0);
	REQUIRE(Bitmap != nullptr);
	HGDIOBJ Old = SelectObject(Context, Bitmap);
	auto *Pixels = static_cast<uint32_t *>(Bits);
	// The erase on a white picture: its result, and the pixels inside Covered (client coordinates) and
	// outside it, up to the client area's size, that are not what they should be (white, black).
	struct TErase {
		LRESULT Result = 0;
		int WrongInside = 0;
		int WrongOutside = 0;
	};
	auto Erase = [&](HWND Parent, RECT Covered, uint32_t Outside) {
		GdiFlush();
		std::fill(Pixels, Pixels + Width * Height, 0x00ffffffu);
		TErase Result;
		Result.Result = SendMessageA(Parent, WM_ERASEBKGND, reinterpret_cast<WPARAM>(Context), 0);
		GdiFlush();
		RECT Client = {};
		GetClientRect(Parent, &Client);
		for (int y = 0; y < Client.bottom && y < Height; y++)
			for (int x = 0; x < Client.right && x < Width; x++) {
				const uint32_t Pixel = Pixels[y * Width + x] & 0x00ffffffu;
				const POINT Point = {x, y};
				if (PtInRect(&Covered, Point))
					Result.WrongInside += Pixel != 0x00ffffffu ? 1 : 0;
				else
					Result.WrongOutside += Pixel != Outside ? 1 : 0;
			}
		return Result;
	};
	auto ChildRect = [](HWND Parent, HWND Window) {
		RECT Rect = {};
		GetWindowRect(Window, &Rect);
		MapWindowPoints(nullptr, Parent, reinterpret_cast<POINT *>(&Rect), 2);
		return Rect;
	};
	const RECT None = {0, 0, 0, 0};

	HWND Parent = NewParent();
	REQUIRE(Parent != nullptr);
	// Before any device window the main window is its own: no subclass, the class's erase (no brush: nothing).
	pv::TPresentWindows Windows;
	CHECK(Windows.Subclassed() == nullptr);
	const TErase Before = Erase(Parent, None, 0x00ffffffu);
	CHECK(Before.Result == 0);
	CHECK(Before.WrongOutside == 0);

	// A device window over the client area.
	HWND Window = Windows.Open(Parent);
	REQUIRE(Window != nullptr);
	CHECK(Windows.Subclassed() == Parent);
	RECT Client = {};
	GetClientRect(Parent, &Client);
	const RECT First = ChildRect(Parent, Window);
	CHECK(First.left == 0);
	CHECK(First.top == 0);
	CHECK(First.right == Client.right);
	CHECK(First.bottom == Client.bottom);
	const TErase Covered = Erase(Parent, First, 0);
	CHECK(Covered.Result == 1);
	CHECK(Covered.WrongInside == 0);
	CHECK(Covered.WrongOutside == 0);

	// The size loop: the main window grows in steps, no frame comes, the device window keeps its size, and
	// each erase fills what the growth uncovered, and nothing of the device window's part.
	int Steps = 0;
	for (int Step = 1; Step <= 8; Step++) {
		SetWindowPos(Parent, nullptr, 0, 0, 320 + Step * 30, 240 + Step * 20, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		const RECT Now = ChildRect(Parent, Window);
		CHECK(Now.right == First.right);
		CHECK(Now.bottom == First.bottom);
		const TErase Grown = Erase(Parent, Now, 0);
		CHECK(Grown.Result == 1);
		CHECK(Grown.WrongInside == 0);
		CHECK(Grown.WrongOutside == 0);
		Steps++;
	}
	GetClientRect(Parent, &Client);
	CHECK(Client.right > First.right);
	CHECK(Client.bottom > First.bottom);
	// The mouse released, the next frame: Fit gives the device window the whole client area again.
	Windows.Fit();
	const RECT Fitted = ChildRect(Parent, Window);
	CHECK(Fitted.right == Client.right);
	CHECK(Fitted.bottom == Client.bottom);
	CHECK(Erase(Parent, Fitted, 0).WrongInside == 0);

	// A retired window that showed a frame still covers its part; the new device's window above it.
	Windows.Presented();
	Windows.Retire();
	CHECK(Windows.Retired() == Window);
	SetWindowPos(Parent, nullptr, 0, 0, 600, 450, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	const RECT RetiredPart = ChildRect(Parent, Window);
	const TErase Retired = Erase(Parent, RetiredPart, 0);
	CHECK(Retired.WrongInside == 0);
	CHECK(Retired.WrongOutside == 0);
	HWND Next = Windows.Open(Parent);
	REQUIRE(Next != nullptr);
	CHECK(Windows.Subclassed() == Parent);
	CHECK(Erase(Parent, ChildRect(Parent, Next), 0).WrongOutside == 0);
	// Close: the main window is its own again.
	Windows.Close();
	CHECK(Windows.Subclassed() == nullptr);
	const TErase Closed = Erase(Parent, None, 0x00ffffffu);
	CHECK(Closed.Result == 0);
	CHECK(Closed.WrongOutside == 0);

	// The main window's end while subclassed takes the subclass (and the device windows) with it.
	Window = Windows.Open(Parent);
	REQUIRE(Window != nullptr);
	CHECK(Windows.Subclassed() == Parent);
	DestroyWindow(Parent);
	CHECK(Windows.Subclassed() == nullptr);
	CHECK_FALSE(IsWindow(Window));
	Windows.Close();
	CHECK(Windows.Current() == nullptr);

	// Another main window afterwards (the same object, as SurfaceVulkan.cpp's static).
	Parent = NewParent();
	REQUIRE(Parent != nullptr);
	Window = Windows.Open(Parent);
	REQUIRE(Window != nullptr);
	CHECK(Windows.Subclassed() == Parent);
	CHECK(Erase(Parent, ChildRect(Parent, Window), 0).WrongOutside == 0);
	// No device window at all (a device that never showed a frame is gone, CreateSurfaces failed): the whole
	// client area is black.
	Windows.Retire();
	CHECK(Windows.Current() == nullptr);
	CHECK(Windows.Retired() == nullptr);
	CHECK(Windows.Subclassed() == Parent);
	const TErase Empty = Erase(Parent, None, 0);
	CHECK(Empty.Result == 1);
	CHECK(Empty.WrongOutside == 0);
	Windows.Close();
	CHECK_FALSE(IsWindowVisible(Parent));
	DestroyWindow(Parent);
	std::printf("main window erase: %d size steps, the uncovered part black, the device windows' part untouched\n",
		Steps);

	SelectObject(Context, Old);
	DeleteObject(Bitmap);
	DeleteDC(Context);
	UnregisterClassA(Class.lpszClassName, Module);
}

// Issue #52, the input: the scripted clicks of the test mode go to the main window (DispatchMessageA, the
// test mode's Dispatch) and real ones through the window's hit test, which the device's window passes on
// (HTTRANSPARENT). Between a button's press and its release, everything the engine does with its windows
// (a new device's window, frames shown on the CPU driver and on a GPU, the window fitted after the main
// window changed its size, a device switch with the old window destroyed after the new one's first frame,
// the device test's hidden window) must leave the main window the mouse: no WM_CAPTURECHANGED, the capture
// still the main window's when the release comes, and every press and release delivered. The windows are
// never shown.
TEST_CASE("scripted clicks reach the main window while device windows come and go (issue #52)")
{
	HINSTANCE Module = GetModuleHandleA(nullptr);
	WNDCLASSA Class = {};
	Class.lpfnWndProc = InputWindowProc;
	Class.hInstance = Module;
	Class.lpszClassName = "PatellaTestsVulkanInput";
	RegisterClassA(&Class);
	HWND Parent = CreateWindowExA(0, Class.lpszClassName, "tests_vulkan", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr,
		nullptr, Module, nullptr);
	REQUIRE(Parent != nullptr);
	InputLog = TInputLog{};

	// The surfaces SDL3's (issue #126): SDL wraps each device's window for the call; the clicks still reach the main
	// window.
	pv::IWindowSystem &System = pv::SdlWindowSystem();
	auto Frames = [Parent](pv::TPresentWindows &Windows, pv::TRenderer &Engine, int Canvas, int Count) {
		for (int Frame = 0; Frame < Count; Frame++) {
			Windows.Fit();
			RECT Client = {};
			GetClientRect(Parent, &Client);
			Engine.Fill(Canvas, 0, 0, 64, 64, 0xff204060u + static_cast<uint32_t>(Frame));
			if (Engine.Present(Canvas, 0, 0, Client.right, Client.bottom, static_cast<uint32_t>(Client.right),
					static_cast<uint32_t>(Client.bottom)))
				Windows.Presented();
		}
	};

	pv::TPresentWindows Windows;
	int Clicks = 0;
	for (size_t First = 0; First < Devices.size(); First++) {
		const TDevice &From = Devices[First];
		const TDevice &To = Devices[(First + 1) % Devices.size()];
		CAPTURE(From.Label);
		CAPTURE(To.Label);
		// Press: the main window takes the mouse.
		DispatchToWindow(Parent, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(40, 30));
		REQUIRE(GetCapture() == Parent);
		// A device and its window, frames, the main window resized.
		auto Engine = std::make_unique<pv::TRenderer>();
		HWND Window = Windows.Open(Parent);
		REQUIRE(Window != nullptr);
		REQUIRE_NOTHROW(Engine->Create(OnWindow(From.Request, Window, System)));
		int Canvas = Engine->CreateImage(400, 300);
		Frames(Windows, *Engine, Canvas, 3);
		SetWindowPos(Parent, nullptr, 0, 0, 300 + static_cast<int>(First) * 10, 230, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		Frames(Windows, *Engine, Canvas, 2);
		CHECK(GetCapture() == Parent);
		// A device switch while the button is held: the device goes, its window is retired, the next
		// device is tested on a hidden window, then gets its own window, whose first frame destroys the old.
		Engine.reset();
		Windows.Retire();
		{
			const pv::TTestWindow Test(Parent);
			pv::TRenderer Tested;
			REQUIRE_NOTHROW(Tested.Create(OnWindow(To.Request, Test.Handle(), System)));
		}
		Engine = std::make_unique<pv::TRenderer>();
		Window = Windows.Open(Parent);
		REQUIRE(Window != nullptr);
		REQUIRE_NOTHROW(Engine->Create(OnWindow(To.Request, Window, System)));
		Canvas = Engine->CreateImage(400, 300);
		Frames(Windows, *Engine, Canvas, 3);
		CHECK(Windows.Retired() == nullptr);
		// Release: still the main window's mouse, and the release arrives.
		CHECK(GetCapture() == Parent);
		DispatchToWindow(Parent, WM_LBUTTONUP, 0, MAKELPARAM(40, 30));
		Clicks++;
		Engine.reset();
		Windows.Retire();
	}
	Windows.Close();
	CHECK(InputLog.Downs == Clicks);
	CHECK(InputLog.Ups == Clicks);
	CHECK(InputLog.CaptureChanges == 0);
	CHECK(GetCapture() == nullptr);
	std::printf("device windows and input: %d clicks, %d capture changes while held\n", Clicks,
		InputLog.CaptureChanges);
	CHECK_FALSE(IsWindowVisible(Parent));
	DestroyWindow(Parent);
	UnregisterClassA(Class.lpszClassName, Module);
}

// Issue #52: quick device switches (Automatic, GPU, GPU again, CPU, in any order) froze the client's
// picture. After a switch away from the AMD Radeon iGPU, whose swapchain presents cross-adapter on this
// machine, the desktop compositor kept showing the Radeon's last frame over the window for good, while
// every later device presented into the same window unseen. Nothing of it is left in the process (device,
// swapchain, surface and instance are gone, in that order); a hidden window is not composed, so this test
// cannot show the stale picture itself. It runs the client's switch sequence (SurfaceVulkan.cpp:
// ExitDisplay, TestEngine, CreateSurfaces, FinishFrame) through PresentWindow.h on every device of the
// machine, many times and in a fixed pseudo-random order, including switches requested before the new
// device has shown a frame, and checks what makes the stale picture impossible: no device ever presents
// in the main window, every device presents in a window no earlier device presented in, the device test
// uses a hidden window of its own that is gone afterwards, and an old device's window is gone as soon as
// the next device has shown a frame (until then it keeps the old picture). The windows are never shown.
TEST_CASE("quick device switches: one window per device, an old device's window goes (issue #52)")
{
	// The client's default path (issue #126): the main window SDL3's, the surfaces SDL_Vulkan_CreateSurface's.
	const TSdlMainWindow Main(320, 240);
	HWND Parent = Main.Handle;
	REQUIRE(Parent != nullptr);
	pv::IWindowSystem &System = pv::SdlWindowSystem();

	// The client's choices: Automatic, every listed GPU, the CPU driver.
	std::vector<pv::TDeviceRequest> Choices;
	pv::TDeviceRequest Automatic;
	Automatic.Kind = pv::TDeviceKind::Automatic;
	Automatic.CpuDriverManifest = CpuManifest();
	Choices.push_back(Automatic);
	const size_t NumberOfGpus = pv::ListGpus().size();
	for (size_t i = 0; i < NumberOfGpus; i++) {
		pv::TDeviceRequest Gpu = Automatic;
		Gpu.Kind = pv::TDeviceKind::Gpu;
		Gpu.GpuIndex = static_cast<int>(i);
		Choices.push_back(Gpu);
	}
	pv::TDeviceRequest Cpu = Automatic;
	Cpu.Kind = pv::TDeviceKind::Cpu;
	Choices.push_back(Cpu);

	std::vector<HWND> Surfaces; // every window a surface was created on, in order

	pv::TPresentWindows Windows;
	std::unique_ptr<pv::TRenderer> Engine;
	std::vector<HWND> Presented; // the windows that showed a frame, in order
	TRandom Random{52};
	int Switches = 0, Shown = 0, Pending = 0;
	std::vector<int> Opened(Choices.size(), 0);
	for (int Step = 0; Step < 48; Step++) {
		const size_t Choice = Random.Next() % Choices.size();
		CAPTURE(Step);
		CAPTURE(Choice);
		// ChangeEngine: ExitSurfaces (the device goes, then its window is retired) ...
		Engine.reset();
		Windows.Retire();
		CHECK(Windows.Current() == nullptr);
		// ... the old device's window stays only when it showed a frame, and only until the next one does.
		if (Windows.Retired() != nullptr) {
			CHECK(IsWindow(Windows.Retired()));
			CHECK(Windows.Retired() == Presented.back());
		}
		// ... TestEngine on a hidden window of its own, gone afterwards.
		HWND TestWindow = nullptr;
		{
			const pv::TTestWindow Test(Parent);
			TestWindow = Test.Handle();
			REQUIRE(TestWindow != nullptr);
			CHECK(GetParent(TestWindow) == Parent);
			CHECK_FALSE((GetWindowLongA(TestWindow, GWL_STYLE) & WS_VISIBLE) != 0);
			pv::TRenderer Tested;
			REQUIRE_NOTHROW(Tested.Create(OnWindow(Choices[Choice], TestWindow, System, &Surfaces)));
		}
		CHECK_FALSE(IsWindow(TestWindow));
		Switches++;
		// Another click before the next frame: the next switch comes before any device for this one.
		if (Random.Next() % 3 == 0) {
			Pending++;
			continue;
		}
		// CheckSurface -> CreateSurfaces: a new window, above the old one, then the device.
		HWND Window = Windows.Open(Parent);
		REQUIRE(Window != nullptr);
		CHECK(Window != Parent);
		CHECK(GetParent(Window) == Parent);
		CHECK(Window != Windows.Retired());
		CHECK(std::find(Presented.begin(), Presented.end(), Window) == Presented.end());
		CHECK(SendMessageA(Window, WM_NCHITTEST, 0, MAKELPARAM(10, 10)) == HTTRANSPARENT);
		CHECK(ChildWindows(Parent) <= 2);
		Engine = std::make_unique<pv::TRenderer>();
		REQUIRE_NOTHROW(Engine->Create(OnWindow(Choices[Choice], Window, System, &Surfaces)));
		Opened[Choice]++;
		const int Canvas = Engine->CreateImage(400, 300);
		// Frames (FinishFrame): the window takes the main window's client area, then the picture is shown;
		// the main window changes its size between some of them.
		const int Frames = 1 + static_cast<int>(Random.Next() % 3);
		for (int Frame = 0; Frame < Frames; Frame++) {
			if (Random.Next() % 4 == 0)
				SetWindowPos(Parent, nullptr, 0, 0, 280 + Frame * 20, 220 + Frame * 10, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			Windows.Fit();
			RECT Client = {}, Inside = {};
			GetClientRect(Parent, &Client);
			GetClientRect(Window, &Inside);
			CHECK(Inside.right == Client.right);
			CHECK(Inside.bottom == Client.bottom);
			CHECK(System.ClientSize(Parent).Width == Client.right);
			CHECK(System.ClientSize(Parent).Height == Client.bottom);
			Engine->Fill(Canvas, 0, 0, 400, 300, 0xff000000u | (Random.Next() & 0xffffff));
			bool Showed = false;
			REQUIRE_NOTHROW(Showed = Engine->Present(Canvas, 0, 0, Client.right, Client.bottom,
								static_cast<uint32_t>(Client.right), static_cast<uint32_t>(Client.bottom)));
			CHECK(Showed);
			if (Showed) {
				Windows.Presented();
				if (Presented.empty() || Presented.back() != Window)
					Presented.push_back(Window);
				Shown++;
				// The old device's window is gone with the new device's first frame.
				CHECK(Windows.Retired() == nullptr);
				CHECK(ChildWindows(Parent) == 1);
			}
		}
	}
	Engine.reset();
	Windows.Retire();
	Windows.Close();
	CHECK(ChildWindows(Parent) == 0);
	for (HWND Window : Presented)
		CHECK_FALSE(IsWindow(Window));
	// No device ever had a surface on the main window.
	CHECK(std::find(Surfaces.begin(), Surfaces.end(), Parent) == Surfaces.end());
	for (size_t i = 0; i < Choices.size(); i++)
		std::printf("device switches: choice %zu opened %d times\n", i, Opened[i]);
	std::printf("device switches: %d switches, %d before a device existed, %d frames shown, %zu windows\n", Switches,
		Pending, Shown, Presented.size());
	CHECK_FALSE(IsWindowVisible(Parent));
}

namespace {

// The process's own handles (issue #53): GetProcessHandleCount, and through ntdll's list of the system's
// handles (NtQuerySystemInformation, SystemExtendedHandleInformation) each handle of this process with
// its object type; for a thread handle also whether its thread has ended, where it started and which
// thread it is.
struct THandleEntry {
	void *Object;
	ULONG_PTR Process;
	ULONG_PTR Handle;
	ULONG Access;
	USHORT BackTrace;
	USHORT Type;
	ULONG Attributes;
	ULONG Reserved;
};

struct TThreadHandle {
	ULONG_PTR Handle;
	bool Ended;
	uintptr_t Start; // where the thread started
	DWORD Thread;    // the live thread's id
};

struct TCensus {
	DWORD Total = 0;
	std::vector<std::pair<std::string, int>> Types; // object type, handles, by name
	std::vector<TThreadHandle> Threads;
	[[nodiscard]] int Of(const std::string &Type) const
	{
		for (const auto &Entry : Types)
			if (Entry.first == Type)
				return Entry.second;
		return 0;
	}
	[[nodiscard]] int Ended() const
	{
		return static_cast<int>(
			std::count_if(Threads.begin(), Threads.end(), [](const TThreadHandle &Thread) { return Thread.Ended; }));
	}
};

using TNtQuerySystemInformation = LONG(WINAPI *)(ULONG, void *, ULONG, ULONG *);
using TNtQueryObject = LONG(WINAPI *)(HANDLE, ULONG, void *, ULONG, ULONG *);
using TNtQueryInformationThread = LONG(WINAPI *)(HANDLE, ULONG, void *, ULONG, ULONG *);

template <typename T>
T NtdllFunction(const char *Name)
{
	return reinterpret_cast<T>(GetProcAddress(GetModuleHandleA("ntdll.dll"), Name));
}

TCensus TakeCensus()
{
	TCensus Census;
	GetProcessHandleCount(GetCurrentProcess(), &Census.Total);
	const auto QuerySystem = NtdllFunction<TNtQuerySystemInformation>("NtQuerySystemInformation");
	const auto QueryObject = NtdllFunction<TNtQueryObject>("NtQueryObject");
	const auto QueryThread = NtdllFunction<TNtQueryInformationThread>("NtQueryInformationThread");
	REQUIRE(QuerySystem != nullptr);
	REQUIRE(QueryObject != nullptr);
	REQUIRE(QueryThread != nullptr);
	constexpr ULONG SystemExtendedHandleInformation = 64;
	constexpr ULONG ObjectTypeInformation = 2;
	constexpr ULONG ThreadQuerySetWin32StartAddress = 9;
	constexpr LONG InfoLengthMismatch = static_cast<LONG>(0xC0000004);
	std::vector<unsigned char> Buffer(static_cast<size_t>(1) << 20);
	ULONG Needed = 0;
	LONG Status = 0;
	while ((Status = QuerySystem(SystemExtendedHandleInformation, Buffer.data(), static_cast<ULONG>(Buffer.size()), &Needed))
		== InfoLengthMismatch)
		Buffer.resize(std::max<size_t>(Buffer.size() * 2, static_cast<size_t>(Needed) + 65536));
	REQUIRE(Status >= 0);
	const size_t Count = *reinterpret_cast<const ULONG_PTR *>(Buffer.data());
	const auto *Entries = reinterpret_cast<const THandleEntry *>(Buffer.data() + 2 * sizeof(ULONG_PTR));
	const ULONG_PTR Self = GetCurrentProcessId();
	std::vector<std::pair<USHORT, std::string>> Names; // type index, name
	for (size_t i = 0; i < Count; i++) {
		const THandleEntry &Entry = Entries[i];
		if (Entry.Process != Self)
			continue;
		auto *const Handle = reinterpret_cast<HANDLE>(Entry.Handle);
		auto Known = std::find_if(Names.begin(), Names.end(), [&Entry](const auto &Name) { return Name.first == Entry.Type; });
		if (Known == Names.end()) {
			alignas(8) unsigned char Info[1024];
			std::string Name = "type " + std::to_string(Entry.Type);
			if (QueryObject(Handle, ObjectTypeInformation, Info, sizeof Info, nullptr) >= 0) {
				const auto *Text = reinterpret_cast<const UNICODE_STRING *>(Info);
				Name.clear();
				for (USHORT c = 0; c < Text->Length / 2; c++)
					Name += static_cast<char>(Text->Buffer[c]);
			}
			Known = Names.emplace(Names.end(), Entry.Type, Name);
		}
		const std::string &TypeName = Known->second;
		auto Type = std::find_if(
			Census.Types.begin(), Census.Types.end(), [&TypeName](const auto &Counted) { return Counted.first == TypeName; });
		if (Type == Census.Types.end())
			Census.Types.emplace_back(TypeName, 1);
		else
			Type->second++;
		if (TypeName == "Thread") {
			TThreadHandle Thread{Entry.Handle, false, 0, 0};
			QueryThread(Handle, ThreadQuerySetWin32StartAddress, &Thread.Start, sizeof Thread.Start, nullptr);
			DWORD Code = 0;
			if (GetExitCodeThread(Handle, &Code) && Code != STILL_ACTIVE)
				Thread.Ended = true;
			else
				Thread.Thread = GetThreadId(Handle);
			Census.Threads.push_back(Thread);
		}
	}
	std::sort(Census.Types.begin(), Census.Types.end());
	return Census;
}

std::string Describe(const TCensus &Census)
{
	std::string Text = std::to_string(Census.Total) + " handles, " + std::to_string(Census.Threads.size())
		+ " of threads (" + std::to_string(Census.Ended()) + " ended):";
	for (const auto &Entry : Census.Types)
		Text += " " + Entry.first + " " + std::to_string(Entry.second);
	return Text;
}

// The handles of live threads, of this thread (Own) or of the others.
int LiveThreadHandles(const TCensus &Census, bool Own)
{
	const DWORD Self = GetCurrentThreadId();
	return static_cast<int>(std::count_if(Census.Threads.begin(), Census.Threads.end(),
		[Own, Self](const TThreadHandle &Thread) { return !Thread.Ended && (Thread.Thread == Self) == Own; }));
}

} // namespace

// Issue #53: switching to and from the CPU driver (and between it and every GPU, as the advanced options'
// buttons do) leaves no handle of ours behind. What the drivers leave is known and bounded, and counted
// apart (notes/vulkan.md, "Handles the drivers leave (issue #53)"):
// - lavapipe (Mesa 26.2.5) never joins its rasterizer threads on Windows (lp_rast.c, lp_rast_destroy), so
//   each device leaves the handles of that many ended threads, all started in vulkan_lvp.dll: the same
//   number for every device, at most LP_MAX_THREADS (32; one per logical processor, or LP_NUM_THREADS);
// - lavapipe's thrd_current (c11/impl/threads_win32.c) duplicates a handle of the calling thread, closed
//   only when that thread ends, which it never sees once the driver is unloaded: at most one per instance;
// - NVIDIA's driver leaves 3 events per device (the Radeon's and lavapipe's none).
// Everything else must stay flat within Tolerance (justified at its definition) over Rounds rounds, so a
// leak of ours of one handle per device or more fails. Headless: no window, no dialog.
TEST_CASE("switching to and from the CPU driver leaves no handles of ours (issue #53)")
{
	// A round has at least one device (one CPU driver device and two per GPU: 5 on this machine).
	constexpr int Rounds = 5;
	// System objects that come and go on their own while the test runs: the census showed the process's
	// ALPC ports changing by 2 with no device in between, and a driver holding an event while its device
	// exists. Below Rounds: a leak of one handle per device exceeds it even without a GPU.
	constexpr int Tolerance = 4;
	static_assert(Tolerance < Rounds);
	constexpr int MaxRasterizerThreads = 32; // Mesa's LP_MAX_THREADS (lp_limits.h)
	constexpr int NvidiaEventsPerDevice = 3;
	constexpr uint32_t Nvidia = 0x10de;

	std::vector<pv::TDeviceRequest> Gpus;
	const size_t NumberOfGpus = pv::ListGpus().size();
	for (size_t i = 0; i < NumberOfGpus; i++) {
		pv::TDeviceRequest Gpu;
		Gpu.Kind = pv::TDeviceKind::Gpu;
		Gpu.GpuIndex = static_cast<int>(i);
		Gpus.push_back(Gpu);
	}
	pv::TDeviceRequest Cpu;
	Cpu.Kind = pv::TDeviceKind::Cpu;
	Cpu.CpuDriverManifest = CpuManifest();

	std::vector<std::pair<uintptr_t, uintptr_t>> LavapipeImages; // where vulkan_lvp.dll was, each time
	int CpuDevices = 0, NvidiaDevices = 0;
	// One device as the client uses it: a frame drawn and read back, then the device destroyed.
	auto Use = [&](const pv::TDeviceRequest &Request) {
		pv::TRenderer Renderer;
		REQUIRE_NOTHROW(Renderer.Create(Request));
		if (Request.Kind == pv::TDeviceKind::Cpu) {
			const HMODULE Lavapipe = GetModuleHandleA("vulkan_lvp.dll");
			REQUIRE(Lavapipe != nullptr);
			MODULEINFO Info = {};
			REQUIRE(GetModuleInformation(GetCurrentProcess(), Lavapipe, &Info, sizeof Info));
			const auto Base = reinterpret_cast<uintptr_t>(Info.lpBaseOfDll);
			LavapipeImages.emplace_back(Base, Base + Info.SizeOfImage);
			CpuDevices++;
		} else if (Renderer.Device().VendorId == Nvidia) {
			NvidiaDevices++;
		}
		const int Image = Renderer.CreateImage(64, 64);
		Renderer.Fill(Image, 0, 0, 64, 64, 0xff204060u);
		CHECK(Read(Renderer, Image)[0] == 0xff204060u);
		Renderer.Finish();
	};
	// Under MSVC's AddressSanitizer (the preset windows-x64-asan, issue #70) every thread starts in the ASan
	// runtime's own start routine (its CreateThread interceptor), which then calls the driver's: such a thread
	// counts as lavapipe's here, the only threads the rounds start (none started elsewhere without ASan).
#if defined(__SANITIZE_ADDRESS__)
	if (const HMODULE Asan = GetModuleHandleA("clang_rt.asan_dynamic-x86_64.dll"); Asan != nullptr) {
		MODULEINFO Info = {};
		REQUIRE(GetModuleInformation(GetCurrentProcess(), Asan, &Info, sizeof Info));
		const auto Base = reinterpret_cast<uintptr_t>(Info.lpBaseOfDll);
		LavapipeImages.emplace_back(Base, Base + Info.SizeOfImage);
	}
#endif
	const auto InLavapipe = [&LavapipeImages](const TThreadHandle &Thread) {
		return std::any_of(LavapipeImages.begin(), LavapipeImages.end(),
			[&Thread](const auto &Image) { return Thread.Start >= Image.first && Thread.Start < Image.second; });
	};
	// A census once the CPU driver's threads have ended (issue #87). lavapipe's lp_rast_destroy waits on
	// Windows for each rasterizer thread's last semaphore post, not for the thread's end (lp_rast.c,
	// lines 1334-1350), so right after a device is destroyed some of its threads may still be ending: their
	// handles are of live threads, and a census taken then counts them in the next device's share (21 and 27
	// for 24 under ASan, whose thread exit is slower). Waits, at most SettleTimeout in all, for the handles
	// that appeared since Earlier, are of live threads and started in vulkan_lvp.dll (or in the ASan
	// runtime); a thread that does not end in time stays live, and the checks below then count it.
	constexpr DWORD SettleTimeout = 30000; // ms
	int StillEnding = 0;                   // threads a census found still ending, for the log
	const auto Settled = [&](const TCensus &Earlier) {
		const ULONGLONG Deadline = GetTickCount64() + SettleTimeout;
		for (;;) {
			TCensus Census = TakeCensus();
			std::vector<HANDLE> Ending;
			for (const TThreadHandle &Thread : Census.Threads) {
				if (Thread.Ended || Thread.Thread == GetCurrentThreadId() || !InLavapipe(Thread))
					continue;
				// The same handle of the same live thread (a handle value can be reused).
				const bool Old =
					std::any_of(Earlier.Threads.begin(), Earlier.Threads.end(), [&Thread](const TThreadHandle &Before) {
						return !Before.Ended && Before.Thread == Thread.Thread && Before.Handle == Thread.Handle;
					});
				if (!Old)
					Ending.push_back(reinterpret_cast<HANDLE>(Thread.Handle));
			}
			if (Ending.empty() || GetTickCount64() >= Deadline)
				return Census;
			StillEnding += static_cast<int>(Ending.size());
			for (HANDLE Thread : Ending) {
				const ULONGLONG Left = Deadline > GetTickCount64() ? Deadline - GetTickCount64() : 0;
				WaitForSingleObject(Thread, static_cast<DWORD>(Left));
			}
		}
	};
	// A round: the CPU driver, then each GPU and back to the CPU driver; the handles of ended threads
	// each CPU driver device added.
	std::vector<int> EndedPerDevice;
	auto Round = [&]() {
		TCensus Last = TakeCensus();
		Use(Cpu);
		TCensus Next = Settled(Last);
		EndedPerDevice.push_back(Next.Ended() - Last.Ended());
		Last = std::move(Next);
		for (const pv::TDeviceRequest &Gpu : Gpus) {
			Use(Gpu);
			Use(Cpu);
			Next = Settled(Last);
			EndedPerDevice.push_back(Next.Ended() - Last.Ended());
			Last = std::move(Next);
		}
	};

	// One round first: what a process creates once (the loader's, the drivers' and the system's objects).
	Round();
	CHECK(GetModuleHandleA("vulkan_lvp.dll") == nullptr); // the loader unloads the driver with its instance
	const TCensus Before = TakeCensus();
	const int CpuBefore = CpuDevices, NvidiaBefore = NvidiaDevices;
	std::printf("handle census before: %s\n", Describe(Before).c_str());
	for (int r = 0; r < Rounds; r++)
		Round();
	const TCensus After = TakeCensus();
	std::printf("handle census after %d rounds: %s\n", Rounds, Describe(After).c_str());
	const int CpuCreated = CpuDevices - CpuBefore, NvidiaCreated = NvidiaDevices - NvidiaBefore;

	// lavapipe's rasterizer threads: every new handle of an ended thread is of a thread vulkan_lvp.dll
	// started, the same number for every device, at most LP_MAX_THREADS.
	int Rasterizers = 0, Elsewhere = 0;
	for (const TThreadHandle &Thread : After.Threads) {
		if (!Thread.Ended)
			continue;
		const bool Old = std::any_of(Before.Threads.begin(), Before.Threads.end(),
			[&Thread](const TThreadHandle &Earlier) { return Earlier.Ended && Earlier.Handle == Thread.Handle; });
		if (Old)
			continue;
		if (InLavapipe(Thread))
			Rasterizers++;
		else
			Elsewhere++;
	}
	const int PerDevice = EndedPerDevice.front();
	std::printf("handle census: %d CPU driver devices left %d handles of ended threads (%d each), %d started elsewhere; "
				"%d found still ending\n",
		CpuCreated, Rasterizers, PerDevice, Elsewhere, StillEnding);
	CHECK(Elsewhere == 0);
	CHECK(PerDevice <= MaxRasterizerThreads);
	for (const int Count : EndedPerDevice)
		CHECK(Count == PerDevice);
	CHECK(Rasterizers == PerDevice * CpuCreated);

	// lavapipe's thrd_current: handles of this thread, at most one per instance; none of other live threads.
	const int OwnThread = LiveThreadHandles(After, true) - LiveThreadHandles(Before, true);
	const int OtherThreads = LiveThreadHandles(After, false) - LiveThreadHandles(Before, false);
	std::printf("handle census: %d handles of this thread more, %d of other live threads\n", OwnThread, OtherThreads);
	CHECK(OwnThread >= 0);
	CHECK(OwnThread <= CpuCreated);
	CHECK(OtherThreads <= Tolerance);

	// NVIDIA's events; every other kind of handle flat.
	const int Events = After.Of("Event") - Before.Of("Event");
	std::printf("handle census: %d events more after %d NVIDIA devices\n", Events, NvidiaCreated);
	CHECK(Events <= NvidiaEventsPerDevice * NvidiaCreated + Tolerance);
	int Others = 0;
	for (const auto &Entry : After.Types)
		if (Entry.first != "Thread" && Entry.first != "Event")
			Others += Entry.second - Before.Of(Entry.first);
	for (const auto &Entry : Before.Types)
		if (Entry.first != "Thread" && Entry.first != "Event" && After.Of(Entry.first) == 0)
			Others -= Entry.second;
	std::printf("handle census: %d handles of other kinds more\n", Others);
	CHECK(Others <= Tolerance);
	const long Ours = static_cast<long>(After.Total) - static_cast<long>(Before.Total) - Rasterizers - OwnThread
		- std::min(Events, NvidiaEventsPerDevice * NvidiaCreated);
	std::printf("handle census: %ld handles more than the drivers' (tolerance %d)\n", Ours, Tolerance);
	CHECK(Ours <= Tolerance);
}

int main(int argc, char **argv)
{
	if (!pv::LoaderAvailable()) {
		std::printf("tests_vulkan: no Vulkan loader (vulkan-1.dll); skipped\n");
		return 77;
	}
	TDevice Cpu;
	Cpu.Label = "CPU driver";
	Cpu.Request.Kind = pv::TDeviceKind::Cpu;
	Cpu.Request.CpuDriverManifest = CpuManifest();
	Devices.push_back(Cpu);
	TDevice Gpu;
	Gpu.Label = "GPU";
	Gpu.Request.Kind = pv::TDeviceKind::Gpu;
	try {
		pv::TRenderer Probe;
		Probe.Create(Gpu.Request);
		Gpu.Label = "GPU (" + Probe.Device().Name + ")";
		Devices.push_back(Gpu);
	} catch (const pv::TVulkanError &Error) {
		std::printf("tests_vulkan: no GPU with Vulkan 1.3 (%s); the CPU driver only\n", Error.what());
	}
	// SDL3's video subsystem, as the client starts it (SdlFrame.cpp's Create: not DPI aware, as the original).
	SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "unaware");
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::printf("tests_vulkan: SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	doctest::Context Context(argc, argv);
	const int Result = Context.run();
	SDL_Quit();
	return Result;
}
