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

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "vulkan/Renderer.h"

#include <volk.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vulkan/vulkan_win32.h>

#include "vulkan/PresentWindow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
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
	HINSTANCE Module = GetModuleHandleA(nullptr);
	WNDCLASSA Class = {};
	Class.lpfnWndProc = DefWindowProcA;
	Class.hInstance = Module;
	Class.lpszClassName = "PatellaTestsVulkanHidden";
	RegisterClassA(&Class);
	HWND Window = CreateWindowExA(0, Class.lpszClassName, "tests_vulkan", WS_OVERLAPPEDWINDOW, 0, 0, 160, 120, nullptr,
		nullptr, Module, nullptr);
	REQUIRE(Window != nullptr);
	CHECK_FALSE(IsWindowVisible(Window));
	for (TDevice &Device : Devices) {
		CAPTURE(Device.Label);
		pv::TDeviceRequest Request = Device.Request;
		Request.SurfaceExtension = "VK_KHR_win32_surface";
		Request.CreateSurface = [Window, Module](VkInstance Instance) {
			auto Create = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
				vkGetInstanceProcAddr(Instance, "vkCreateWin32SurfaceKHR"));
			VkWin32SurfaceCreateInfoKHR Info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
			Info.hinstance = Module;
			Info.hwnd = Window;
			VkSurfaceKHR Surface = VK_NULL_HANDLE;
			if (Create == nullptr || Create(Instance, &Info, nullptr, &Surface) != VK_SUCCESS)
				throw pv::TVulkanError("vkCreateWin32SurfaceKHR failed");
			return Surface;
		};
		pv::TRenderer Renderer;
		REQUIRE_NOTHROW(Renderer.Create(Request));
		std::printf("%s: a device for a hidden window: %s\n", Device.Label.c_str(), Renderer.Device().Name.c_str());
		// The swapchain (vk-bootstrap's surface format and present mode queries) on the first frame.
		int Image = Renderer.CreateImage(160, 120);
		Renderer.Fill(Image, 0, 0, 160, 120, 0xff204060u);
		RECT Client;
		GetClientRect(Window, &Client);
		CHECK_NOTHROW(Renderer.Present(Image, 0, 0, 160, 120, static_cast<uint32_t>(Client.right),
			static_cast<uint32_t>(Client.bottom)));
		Renderer.Finish();
	}
	CHECK_FALSE(IsWindowVisible(Window));
	DestroyWindow(Window);
	UnregisterClassA(Class.lpszClassName, Module);
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

	auto OnWindow = [Module](pv::TDeviceRequest Request, HWND Window) {
		Request.SurfaceExtension = "VK_KHR_win32_surface";
		Request.CreateSurface = [Window, Module](VkInstance Instance) {
			auto Create = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
				vkGetInstanceProcAddr(Instance, "vkCreateWin32SurfaceKHR"));
			VkWin32SurfaceCreateInfoKHR Info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
			Info.hinstance = Module;
			Info.hwnd = Window;
			VkSurfaceKHR Surface = VK_NULL_HANDLE;
			if (Create == nullptr || Create(Instance, &Info, nullptr, &Surface) != VK_SUCCESS)
				throw pv::TVulkanError("vkCreateWin32SurfaceKHR failed");
			return Surface;
		};
		return Request;
	};
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
		REQUIRE_NOTHROW(Engine->Create(OnWindow(From.Request, Window)));
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
			REQUIRE_NOTHROW(Tested.Create(OnWindow(To.Request, Test.Handle())));
		}
		Engine = std::make_unique<pv::TRenderer>();
		Window = Windows.Open(Parent);
		REQUIRE(Window != nullptr);
		REQUIRE_NOTHROW(Engine->Create(OnWindow(To.Request, Window)));
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
	HINSTANCE Module = GetModuleHandleA(nullptr);
	WNDCLASSA Class = {};
	Class.lpfnWndProc = DefWindowProcA;
	Class.hInstance = Module;
	Class.lpszClassName = "PatellaTestsVulkanSwitches";
	RegisterClassA(&Class);
	HWND Parent = CreateWindowExA(0, Class.lpszClassName, "tests_vulkan", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr,
		nullptr, Module, nullptr);
	REQUIRE(Parent != nullptr);

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
	auto OnWindow = [&Surfaces, Module](pv::TDeviceRequest Request, HWND Window) {
		Request.SurfaceExtension = "VK_KHR_win32_surface";
		Request.CreateSurface = [Window, Module, &Surfaces](VkInstance Instance) {
			Surfaces.push_back(Window);
			auto Create = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
				vkGetInstanceProcAddr(Instance, "vkCreateWin32SurfaceKHR"));
			VkWin32SurfaceCreateInfoKHR Info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
			Info.hinstance = Module;
			Info.hwnd = Window;
			VkSurfaceKHR Surface = VK_NULL_HANDLE;
			if (Create == nullptr || Create(Instance, &Info, nullptr, &Surface) != VK_SUCCESS)
				throw pv::TVulkanError("vkCreateWin32SurfaceKHR failed");
			return Surface;
		};
		return Request;
	};

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
			REQUIRE_NOTHROW(Tested.Create(OnWindow(Choices[Choice], TestWindow)));
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
		REQUIRE_NOTHROW(Engine->Create(OnWindow(Choices[Choice], Window)));
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
	DestroyWindow(Parent);
	UnregisterClassA(Class.lpszClassName, Module);
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
	doctest::Context Context(argc, argv);
	return Context.run();
}
