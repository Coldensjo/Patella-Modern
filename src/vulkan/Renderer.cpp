// Renderer.cpp: the Vulkan 1.3 back end of the Vulkan engine (Renderer.h; notes/vulkan.md).

#include "vulkan/Renderer.h"

#include "compat/msvc.h"

#include <volk.h>

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The SPIR-V of src/vulkan/shaders, compiled by glslang in the build (cmake/Vulkan.cmake).
#include "ColorKeyFrag.h"
#include "FillFrag.h"
#include "QuadFrag.h"
#include "QuadVert.h"

#include "types/Cp1252.h"
#include "vulkan/DeviceChoice.h"
#include "vulkan/Quads.h"

namespace patella::vulkan {

namespace {

constexpr VkFormat ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
constexpr int FramesInFlight = 2;
constexpr VkDeviceSize ChunkSize = VkDeviceSize{4} << 20;
// The descriptor sets: three per image (the samplers below).
constexpr uint32_t MaxImages = 64;
constexpr uint32_t Samplers = 3; // nearest, linear (both wrapping, D3DTADDRESS_WRAP), nearest clamped (Edge copies)

void Check(VkResult Result, const char *What)
{
	if (Result != VK_SUCCESS)
		throw TVulkanError(std::string(What) + " failed (VkResult " + std::to_string(static_cast<int>(Result)) + ")");
}

template <typename T>
T Value(vkb::Result<T> &&Result, const char *What)
{
	if (!Result)
		throw TVulkanError(std::string(What) + " failed: " + Result.error().message());
	return std::move(Result.value());
}

// The push constants of every pipeline (the same layout in both stages).
struct TPushConstants {
	float ScaleX, ScaleY;             // 2 / the target's size: pixels to normalised device coordinates
	int32_t OffsetX, OffsetY;         // the colour key pass: the rectangle's corner in the target
	std::array<uint32_t, 4> Key;      // the colour key pass: the key, 0-255 (R, G, B, A)
	std::array<float, 4> Replacement; // the colour key pass: the replacement (R, G, B, A)
};
static_assert(sizeof(TPushConstants) == 48);

struct TBuffer {
	VkBuffer Buffer = VK_NULL_HANDLE;
	VmaAllocation Allocation = nullptr;
	uint8_t *Mapped = nullptr;
	VkDeviceSize Size = 0;
	VkDeviceSize Used = 0;
};

struct TImage {
	VkImage Image = VK_NULL_HANDLE;
	VmaAllocation Allocation = nullptr;
	VkImageView View = VK_NULL_HANDLE;
	int Width = 0;
	int Height = 0;
	std::array<VkDescriptorSet, Samplers> Sets = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
};

struct TFrame {
	VkCommandPool Pool = VK_NULL_HANDLE;
	VkCommandBuffer Commands = VK_NULL_HANDLE;
	VkFence Fence = VK_NULL_HANDLE;
	VkSemaphore Acquired = VK_NULL_HANDLE;
	std::vector<TBuffer> Vertices;
	std::vector<TBuffer> Staging;
	// Issue #134, the frame timings (TRenderer::SetFrameTimestamps): two timestamps around each submission.
	VkQueryPool Timestamps = VK_NULL_HANDLE;
	bool TimestampWritten = false; // this command buffer has its first timestamp
	bool TimestampPending = false; // a submission with both timestamps, its results not read yet
	bool EndsFrame = false;        // and it was the frame's last
	uint64_t FrameTicks = 0;       // the frame's submissions so far (device ticks)
};

uint32_t ToUint(int Value)
{
	return static_cast<uint32_t>(Value < 0 ? 0 : Value);
}

// The loader's driver settings of this process (never the system's): VK_DRIVER_FILES and, for loaders
// before 1.3.207, VK_ICD_FILENAMES. The previous values are restored when the instance and device
// exist.
class TDriverFiles {
public:
	explicit TDriverFiles(const std::string &Manifest)
	{
		for (int i = 0; i < 2; i++) {
			Saved[i] = Get(Names[i]);
			Set(Names[i], Manifest.c_str());
		}
	}
	~TDriverFiles()
	{
		for (int i = 0; i < 2; i++) {
			const std::optional<std::string> &Value = Saved[i];
			Set(Names[i], Value ? Value->c_str() : nullptr);
		}
	}
	TDriverFiles(const TDriverFiles &) = delete;
	TDriverFiles &operator=(const TDriverFiles &) = delete;

private:
	static std::optional<std::string> Get(const char *Name)
	{
		char *Value = nullptr;
		size_t Length = 0;
		if (MsvcDupenv(&Value, &Length, Name) != 0 || Value == nullptr)
			return std::nullopt;
		std::string Result(Value);
		std::free(Value); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): MsvcDupenv's copy
		return Result;
	}
	static void Set(const char *Name, const char *Value)
	{
		// _putenv_s (MsvcPutenv) sets the process environment too (on Windows SetEnvironmentVariable), which
		// the loader reads; an empty value removes the variable.
		MsvcPutenv(Name, Value != nullptr ? Value : "");
	}
	static constexpr std::array<const char *, 2> Names = {"VK_DRIVER_FILES", "VK_ICD_FILENAMES"};
	std::array<std::optional<std::string>, 2> Saved;
};

int TypeRank(VkPhysicalDeviceType Type)
{
	switch (Type) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return 0;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return 1;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return 2;
	case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		return 3;
	default:
		return 4; // the CPU
	}
}

// The devices of one side (the CPU driver's or the system's GPUs) the engine can use: Vulkan 1.3 with
// dynamic rendering and synchronization2, in the loader's order (vk-bootstrap's select_devices).
// Listed: the list a GPU index counts in (ListGpus, issue #31), the same with or without a window, so
// any surface is left out and VK_KHR_swapchain is required instead. Otherwise, with a surface, the
// devices that can present to it.
std::vector<vkb::PhysicalDevice> FindDevices(const vkb::Instance &Instance, VkSurfaceKHR Surface, bool Cpu, bool Listed)
{
	VkPhysicalDeviceVulkan13Features Features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	Features13.dynamicRendering = VK_TRUE;
	Features13.synchronization2 = VK_TRUE;
	vkb::PhysicalDeviceSelector Selector(Instance);
	Selector.set_minimum_version(1, 3).set_required_features_13(Features13).allow_any_gpu_device_type(true);
	if (Listed)
		Selector.defer_surface_initialization().add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	else if (Surface != VK_NULL_HANDLE)
		Selector.set_surface(Surface);
	else
		Selector.defer_surface_initialization();
	auto Found = Selector.select_devices();
	std::vector<vkb::PhysicalDevice> Devices;
	if (Found) {
		for (const vkb::PhysicalDevice &Physical : Found.value()) {
			const bool IsCpu = Physical.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
			if (IsCpu == Cpu)
				Devices.push_back(Physical);
		}
	}
	return Devices;
}

TDeviceInfo InfoOf(const vkb::PhysicalDevice &Physical, bool Cpu)
{
	TDeviceInfo Info;
	Info.Name = patella::text::Utf8ToCp1252(Physical.properties.deviceName); // UTF-8 (Vulkan), shown by the GUI
	Info.Type = Physical.properties.deviceType;
	Info.VendorId = Physical.properties.vendorID;
	Info.DeviceId = Physical.properties.deviceID;
	Info.ApiVersion = Physical.properties.apiVersion;
	Info.DriverVersion = Physical.properties.driverVersion;
	Info.CpuDriver = Cpu;
	return Info;
}

// The window system's surface extension of the platform (the one the client's TDeviceRequest names).
// Every system's surface extension the loader offers is enabled on every instance (issue-31-fix, below): on Linux the
// X11 and the Wayland one of SDL3's video drivers (issue #131); which of them the window's device names is SDL's choice.
#if defined(_WIN32)
constexpr std::array<const char *, 1> PlatformSurfaceExtensions = {"VK_KHR_win32_surface"};
#else
constexpr std::array<const char *, 3> PlatformSurfaceExtensions = {"VK_KHR_xlib_surface", "VK_KHR_xcb_surface",
	"VK_KHR_wayland_surface"};
#endif

// The builder of every instance the back end creates (issue-31-fix). vk-bootstrap loads its instance
// functions (the surface queries of the device selection and the swapchain, the debug messenger's) into a
// table of its own once per process, from the first instance it builds (VkBootstrap.cpp
// init_instance_funcs), and keeps them for every later instance. The loader gives such a function only
// for an instance with its extension enabled, so the first instance decides for the whole process: the
// device list's headless instance, the first at the client's start since issue #31, left the surface
// functions null, and the window's device selection then called a null pointer (get_present_queue_index;
// the start-up crash 0xC0000409). So every instance enables the same instance extensions, whatever it is
// for: VK_KHR_surface with the platform's surface extension, and VK_EXT_debug_utils, each where the loader
// offers it (they cost nothing without a window or a messenger), and the one a request names. The functions
// the loader gives are its trampolines, the same for every instance, so the table stays valid after its
// instance is destroyed (as it did before issue #31: the start-up test's instance was destroyed too).
vkb::InstanceBuilder NewInstanceBuilder(const std::string &SurfaceExtension)
{
	vkb::InstanceBuilder Builder(vkGetInstanceProcAddr);
	Builder.set_app_name("Patella").set_engine_name("Patella").require_api_version(1, 3, 0).set_headless(true);
	auto System = vkb::SystemInfo::get_system_info(vkGetInstanceProcAddr);
	std::vector<const char *> Wanted = {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
	Wanted.insert(Wanted.begin() + 1, PlatformSurfaceExtensions.begin(), PlatformSurfaceExtensions.end());
	for (const char *Extension : Wanted) {
		if (Extension != nullptr && System && System.value().is_extension_available(Extension)
			&& SurfaceExtension != Extension)
			Builder.enable_extension(Extension);
	}
	// A surface the request needs: asked for even when the loader does not list it (the build fails then,
	// as before).
	if (!SurfaceExtension.empty()) {
		if (!System || !System.value().is_extension_available(VK_KHR_SURFACE_EXTENSION_NAME))
			Builder.enable_extension(VK_KHR_SURFACE_EXTENSION_NAME);
		Builder.enable_extension(SurfaceExtension);
	}
	return Builder;
}

// A headless instance just for a device list.
std::vector<TDeviceInfo> ListDevices(bool Cpu)
{
	if (volkInitialize() != VK_SUCCESS)
		throw TVulkanError("no Vulkan loader (vulkan-1.dll)");
	const vkb::InstanceBuilder Builder = NewInstanceBuilder({});
	auto Built = Builder.build();
	std::vector<TDeviceInfo> Result;
	if (!Built)
		return Result; // no driver offers Vulkan 1.3
	const vkb::Instance Instance = Built.value();
	try {
		for (const vkb::PhysicalDevice &Physical : FindDevices(Instance, VK_NULL_HANDLE, Cpu, true))
			Result.push_back(InfoOf(Physical, Cpu));
	} catch (...) {
		vkb::destroy_instance(Instance);
		throw;
	}
	vkb::destroy_instance(Instance);
	return Result;
}

} // namespace

std::vector<TDeviceInfo> ListGpus()
{
	return ListDevices(false);
}

int BestGpu(const std::vector<TDeviceInfo> &Gpus)
{
	int Best = -1;
	for (size_t i = 0; i < Gpus.size(); i++) {
		if (Best < 0 || TypeRank(Gpus[i].Type) < TypeRank(Gpus[static_cast<size_t>(Best)].Type))
			Best = static_cast<int>(i);
	}
	return Best;
}

std::optional<TDeviceInfo> CpuDriverDevice(const std::string &Manifest)
{
	if (Manifest.empty())
		return std::nullopt;
	const TDriverFiles Driver(Manifest);
	std::vector<TDeviceInfo> Devices = ListDevices(true);
	if (Devices.empty())
		return std::nullopt;
	return Devices.front();
}

bool LoaderAvailable()
{
	return volkInitialize() == VK_SUCCESS;
}

struct TRenderer::TState {
	vkb::Instance Instance;
	vkb::Device Device;
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	vkb::Swapchain Swapchain;
	std::vector<VkImage> SwapchainImages;
	std::vector<VkSemaphore> Rendered; // per swapchain image
	bool SwapchainStale = false;

	TDeviceInfo DeviceInfo;
	VkQueue Queue = VK_NULL_HANDLE;
	VmaAllocator Allocator = nullptr;

	VkDescriptorSetLayout SetLayout = VK_NULL_HANDLE;
	VkPipelineLayout Layout = VK_NULL_HANDLE;
	VkPipeline Textured = VK_NULL_HANDLE;
	VkPipeline Fill = VK_NULL_HANDLE;
	VkPipeline ColorKey = VK_NULL_HANDLE;
	std::array<VkSampler, Samplers> SamplerObjects = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;

	std::vector<TImage> Images;
	int KeyScratch = -1; // ReplaceColor's copy of the rectangle
	int Feedback = -1;   // a copy of a target that a draw into it samples

	std::array<TFrame, FramesInFlight> Frames;
	int FrameIndex = 0;
	bool Recording = false;

	// Issue #134: off unless SetFrameTimestamps asks for them; with them off no timing command is recorded.
	bool TimestampsOn = false;
	uint64_t TimestampMask = 0;       // the queue's valid timestamp bits
	double TimestampPeriod = 0.0;     // nanoseconds per device tick
	std::vector<uint64_t> FrameTimes; // nanoseconds of finished frames, not taken yet

	int PassTarget = -1;
	VkPipeline BoundPipeline = VK_NULL_HANDLE;
	VkDescriptorSet BoundSet = VK_NULL_HANDLE;
	VkBuffer BoundVertices = VK_NULL_HANDLE;

	TBuffer ReadBuffer;

	// TRenderer::Draw's working copies of the quads (issue #155): they keep their capacity, so a draw allocates only when
	// its batch is the largest so far. Draw empties and refills both before it reads them.
	std::vector<TVertex> QuadVertices;
	std::vector<uint32_t> QuadSamplers; // the descriptor set per quad

	TFrame &Frame() { return Frames[FrameIndex]; }
	VkCommandBuffer Commands() { return Frames[FrameIndex].Commands; }

	// --- buffers ---

	[[nodiscard]] TBuffer CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage, bool Readable) const
	{
		TBuffer Buffer;
		VkBufferCreateInfo Info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		Info.size = Size;
		Info.usage = Usage;
		VmaAllocationCreateInfo Allocation{};
		Allocation.usage = VMA_MEMORY_USAGE_AUTO;
		Allocation.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
			| (Readable ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
						: VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		VmaAllocationInfo Result{};
		Check(vmaCreateBuffer(Allocator, &Info, &Allocation, &Buffer.Buffer, &Buffer.Allocation, &Result),
			"vmaCreateBuffer");
		Buffer.Mapped = static_cast<uint8_t *>(Result.pMappedData);
		Buffer.Size = Size;
		return Buffer;
	}

	void DestroyBuffer(TBuffer &Buffer) const
	{
		if (Buffer.Buffer != VK_NULL_HANDLE)
			vmaDestroyBuffer(Allocator, Buffer.Buffer, Buffer.Allocation);
		Buffer = TBuffer{};
	}

	// Room for Size bytes (aligned to 16) in one of the frame's buffers of a kind.
	TBuffer &Allocate(std::vector<TBuffer> &Chunks, VkDeviceSize Size, VkBufferUsageFlags Usage, VkDeviceSize &Offset) const
	{
		for (TBuffer &Chunk : Chunks) {
			const VkDeviceSize Start = (Chunk.Used + 15) & ~VkDeviceSize{15};
			if (Start + Size <= Chunk.Size) {
				Offset = Start;
				Chunk.Used = Start + Size;
				return Chunk;
			}
		}
		Chunks.push_back(CreateBuffer(std::max(ChunkSize, Size), Usage, false));
		Offset = 0;
		Chunks.back().Used = Size;
		return Chunks.back();
	}

	// --- synchronisation and passes ---

	// Every earlier write is visible to every later access. The engine is a sequence of small steps
	// on a few images, so one global barrier between them is the simple and safe choice.
	void Barrier()
	{
		VkMemoryBarrier2 Memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
		Memory.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		Memory.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		Memory.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		Memory.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		VkDependencyInfo Dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		Dependency.memoryBarrierCount = 1;
		Dependency.pMemoryBarriers = &Memory;
		vkCmdPipelineBarrier2(Commands(), &Dependency);
	}

	void ImageBarrier(VkImage Image, VkImageLayout From, VkImageLayout To, VkPipelineStageFlags2 SourceStage,
		VkAccessFlags2 SourceAccess, VkPipelineStageFlags2 TargetStage, VkAccessFlags2 TargetAccess)
	{
		VkImageMemoryBarrier2 Barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
		Barrier.srcStageMask = SourceStage;
		Barrier.srcAccessMask = SourceAccess;
		Barrier.dstStageMask = TargetStage;
		Barrier.dstAccessMask = TargetAccess;
		Barrier.oldLayout = From;
		Barrier.newLayout = To;
		Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		Barrier.image = Image;
		Barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		VkDependencyInfo Dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		Dependency.imageMemoryBarrierCount = 1;
		Dependency.pImageMemoryBarriers = &Barrier;
		vkCmdPipelineBarrier2(Commands(), &Dependency);
	}

	void EndPass()
	{
		if (PassTarget >= 0) {
			vkCmdEndRendering(Commands());
			PassTarget = -1;
			Barrier();
		}
	}

	void BeginPass(int Target)
	{
		if (PassTarget == Target)
			return;
		EndPass();
		const TImage &Image = Images[static_cast<size_t>(Target)];
		VkRenderingAttachmentInfo Attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
		Attachment.imageView = Image.View;
		Attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		VkRenderingInfo Rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
		Rendering.renderArea = {{0, 0}, {ToUint(Image.Width), ToUint(Image.Height)}};
		Rendering.layerCount = 1;
		Rendering.colorAttachmentCount = 1;
		Rendering.pColorAttachments = &Attachment;
		vkCmdBeginRendering(Commands(), &Rendering);
		const VkViewport Viewport{0.0f, 0.0f, static_cast<float>(Image.Width), static_cast<float>(Image.Height), 0.0f, 1.0f};
		vkCmdSetViewport(Commands(), 0, 1, &Viewport);
		const VkRect2D Scissor = Rendering.renderArea;
		vkCmdSetScissor(Commands(), 0, 1, &Scissor);
		PassTarget = Target;
		BoundPipeline = VK_NULL_HANDLE;
		BoundSet = VK_NULL_HANDLE;
		BoundVertices = VK_NULL_HANDLE;
	}

	void Bind(VkPipeline Pipeline, const TPushConstants &Constants)
	{
		if (BoundPipeline != Pipeline) {
			vkCmdBindPipeline(Commands(), VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
			BoundPipeline = Pipeline;
		}
		vkCmdPushConstants(Commands(), Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			sizeof(Constants), &Constants);
	}

	void BindSet(VkDescriptorSet Set)
	{
		if (BoundSet != Set) {
			vkCmdBindDescriptorSets(Commands(), VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 0, 1, &Set, 0, nullptr);
			BoundSet = Set;
		}
	}

	TPushConstants Constants(int Target)
	{
		TPushConstants C{};
		C.ScaleX = 2.0f / static_cast<float>(Images[static_cast<size_t>(Target)].Width);
		C.ScaleY = 2.0f / static_cast<float>(Images[static_cast<size_t>(Target)].Height);
		return C;
	}

	// Draws Count vertices into the current pass from the frame's vertex buffers.
	void DrawVertices(const TVertex *Vertices, size_t Count)
	{
		VkDeviceSize Offset = 0;
		const VkDeviceSize Size = Count * sizeof(TVertex);
		const TBuffer &Buffer = Allocate(Frame().Vertices, Size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, Offset);
		std::memcpy(Buffer.Mapped + Offset, Vertices, static_cast<size_t>(Size));
		Check(vmaFlushAllocation(Allocator, Buffer.Allocation, Offset, Size), "vmaFlushAllocation");
		const VkDeviceSize Zero = 0;
		if (BoundVertices != Buffer.Buffer) {
			vkCmdBindVertexBuffers(Commands(), 0, 1, &Buffer.Buffer, &Zero);
			BoundVertices = Buffer.Buffer;
		}
		vkCmdDraw(Commands(), static_cast<uint32_t>(Count), 1, static_cast<uint32_t>(Offset / sizeof(TVertex)), 0);
	}

	// --- frames ---

	void BeginFrame()
	{
		TFrame &F = Frame();
		Check(vkWaitForFences(Device.device, 1, &F.Fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
		if (F.TimestampPending)
			ReadTimestamps(F);
		Check(vkResetFences(Device.device, 1, &F.Fence), "vkResetFences");
		Check(vkResetCommandPool(Device.device, F.Pool, 0), "vkResetCommandPool");
		for (TBuffer &Chunk : F.Vertices)
			Chunk.Used = 0;
		for (TBuffer &Chunk : F.Staging)
			Chunk.Used = 0;
		VkCommandBufferBeginInfo Begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		Begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		Check(vkBeginCommandBuffer(F.Commands, &Begin), "vkBeginCommandBuffer");
		F.TimestampWritten = false;
		if (TimestampsOn && F.Timestamps != VK_NULL_HANDLE) {
			vkCmdResetQueryPool(F.Commands, F.Timestamps, 0, 2);
			vkCmdWriteTimestamp2(F.Commands, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, F.Timestamps, 0);
			F.TimestampWritten = true;
		}
		Recording = true;
		PassTarget = -1;
		BoundPipeline = VK_NULL_HANDLE;
		BoundSet = VK_NULL_HANDLE;
		BoundVertices = VK_NULL_HANDLE;
	}

	// Issue #134: the results of a submission whose fence has signalled; the frame's last one completes a frame.
	void ReadTimestamps(TFrame &F)
	{
		F.TimestampPending = false;
		std::array<uint64_t, 2> Values{};
		if (vkGetQueryPoolResults(Device.device, F.Timestamps, 0, 2, sizeof(Values), Values.data(), sizeof(uint64_t),
				VK_QUERY_RESULT_64_BIT)
			== VK_SUCCESS)
			F.FrameTicks += (Values[1] - Values[0]) & TimestampMask;
		if (F.EndsFrame) {
			F.EndsFrame = false;
			FrameTimes.push_back(static_cast<uint64_t>(static_cast<double>(F.FrameTicks) * TimestampPeriod));
			F.FrameTicks = 0;
		}
	}

	bool EnableTimestamps()
	{
		if (Frames.front().Timestamps == VK_NULL_HANDLE) {
			const uint32_t Family = Value(Device.get_queue_index(vkb::QueueType::graphics), "the graphics queue");
			const uint32_t ValidBits = Device.queue_families[Family].timestampValidBits;
			TimestampPeriod = static_cast<double>(Device.physical_device.properties.limits.timestampPeriod);
			if (ValidBits == 0 || TimestampPeriod <= 0.0)
				return false;
			TimestampMask = ValidBits >= 64 ? ~uint64_t{0} : (uint64_t{1} << ValidBits) - 1;
			VkQueryPoolCreateInfo Info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
			Info.queryType = VK_QUERY_TYPE_TIMESTAMP;
			Info.queryCount = 2;
			for (TFrame &F : Frames)
				Check(vkCreateQueryPool(Device.device, &Info, nullptr, &F.Timestamps), "vkCreateQueryPool");
			// Room for the frames between two takes: taking them in a frame loop allocates nothing.
			FrameTimes.reserve(64);
		}
		TimestampsOn = true;
		return true;
	}

	// Ends and submits the frame's commands (waiting for Wait at the transfer stage, signalling Signal).
	void Submit(VkSemaphore Wait, VkSemaphore Signal)
	{
		EndPass();
		TFrame &F = Frame();
		if (F.TimestampWritten) {
			vkCmdWriteTimestamp2(F.Commands, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, F.Timestamps, 1);
			F.TimestampPending = true;
		}
		Check(vkEndCommandBuffer(F.Commands), "vkEndCommandBuffer");
		Recording = false;
		VkCommandBufferSubmitInfo CommandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
		CommandInfo.commandBuffer = F.Commands;
		VkSemaphoreSubmitInfo WaitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
		WaitInfo.semaphore = Wait;
		WaitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
		VkSemaphoreSubmitInfo SignalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
		SignalInfo.semaphore = Signal;
		SignalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		VkSubmitInfo2 Submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
		Submit.commandBufferInfoCount = 1;
		Submit.pCommandBufferInfos = &CommandInfo;
		Submit.waitSemaphoreInfoCount = Wait != VK_NULL_HANDLE ? 1u : 0u;
		Submit.pWaitSemaphoreInfos = &WaitInfo;
		Submit.signalSemaphoreInfoCount = Signal != VK_NULL_HANDLE ? 1u : 0u;
		Submit.pSignalSemaphoreInfos = &SignalInfo;
		Check(vkQueueSubmit2(Queue, 1, &Submit, F.Fence), "vkQueueSubmit2");
	}

	// Submits, waits, and records on in the same frame.
	void SubmitAndWait()
	{
		Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
		BeginFrame();
	}

	void NextFrame()
	{
		Frame().EndsFrame = Frame().TimestampPending;
		FrameIndex = (FrameIndex + 1) % FramesInFlight;
		BeginFrame();
	}

	// --- images ---

	void WriteSets(TImage &Image)
	{
		for (uint32_t i = 0; i < Samplers; i++) {
			const VkDescriptorImageInfo Info{SamplerObjects[i], Image.View, VK_IMAGE_LAYOUT_GENERAL};
			VkWriteDescriptorSet Write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			Write.dstSet = Image.Sets[i];
			Write.descriptorCount = 1;
			Write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			Write.pImageInfo = &Info;
			vkUpdateDescriptorSets(Device.device, 1, &Write, 0, nullptr);
		}
	}

	void MakeImage(TImage &Image, int Width, int Height)
	{
		if (Width <= 0 || Height <= 0)
			throw TVulkanError("an image of " + std::to_string(Width) + "x" + std::to_string(Height));
		VkImageCreateInfo Info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; // NOLINT(bugprone-invalid-enum-default-initialization): samples is set below
		Info.imageType = VK_IMAGE_TYPE_2D;
		Info.format = ImageFormat;
		Info.extent = {ToUint(Width), ToUint(Height), 1};
		Info.mipLevels = 1;
		Info.arrayLayers = 1;
		Info.samples = VK_SAMPLE_COUNT_1_BIT;
		Info.tiling = VK_IMAGE_TILING_OPTIMAL;
		Info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
			| VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		Info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VmaAllocationCreateInfo Allocation{};
		Allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		Check(vmaCreateImage(Allocator, &Info, &Allocation, &Image.Image, &Image.Allocation, nullptr), "vmaCreateImage");
		VkImageViewCreateInfo View{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		View.image = Image.Image;
		View.viewType = VK_IMAGE_VIEW_TYPE_2D;
		View.format = ImageFormat;
		View.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		Check(vkCreateImageView(Device.device, &View, nullptr, &Image.View), "vkCreateImageView");
		Image.Width = Width;
		Image.Height = Height;
		WriteSets(Image);
		// GENERAL for every use, cleared to 0.
		EndPass();
		ImageBarrier(Image.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, 0,
			VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
		const VkClearColorValue Zero{};
		const VkImageSubresourceRange Range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		vkCmdClearColorImage(Commands(), Image.Image, VK_IMAGE_LAYOUT_GENERAL, &Zero, 1, &Range);
		Barrier();
	}

	void FreeImage(TImage &Image) const
	{
		if (Image.View != VK_NULL_HANDLE)
			vkDestroyImageView(Device.device, Image.View, nullptr);
		if (Image.Image != VK_NULL_HANDLE)
			vmaDestroyImage(Allocator, Image.Image, Image.Allocation);
		Image.View = VK_NULL_HANDLE;
		Image.Image = VK_NULL_HANDLE;
		Image.Allocation = nullptr;
	}

	[[nodiscard]] bool ImageTooSmall(int Id, int Width, int Height) const
	{
		const TImage &Image = Images[static_cast<size_t>(Id)];
		return Image.Width < Width || Image.Height < Height;
	}

	// Images never moves (reserved for MaxImages), so references to its elements stay valid.
	int NewImage(int Width, int Height)
	{
		if (Images.size() >= MaxImages)
			throw TVulkanError("too many images");
		TImage Image;
		const std::array<VkDescriptorSetLayout, Samplers> Layouts = {SetLayout, SetLayout, SetLayout};
		VkDescriptorSetAllocateInfo Info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		Info.descriptorPool = DescriptorPool;
		Info.descriptorSetCount = Samplers;
		Info.pSetLayouts = Layouts.data();
		Check(vkAllocateDescriptorSets(Device.device, &Info, Image.Sets.data()), "vkAllocateDescriptorSets");
		Images.push_back(Image);
		MakeImage(Images.back(), Width, Height);
		return static_cast<int>(Images.size() - 1);
	}

	void Resize(int Id, int Width, int Height)
	{
		TImage &Image = Images[static_cast<size_t>(Id)];
		if (Image.Width == Width && Image.Height == Height)
			return;
		// The old image may still be in use by submitted frames and by this one.
		SubmitAndWait();
		Check(vkDeviceWaitIdle(Device.device), "vkDeviceWaitIdle");
		FreeImage(Image);
		MakeImage(Image, Width, Height);
	}

	// --- pipelines ---

	VkShaderModule Module(const uint32_t *Code, size_t Size) const
	{
		VkShaderModuleCreateInfo Info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		Info.codeSize = Size;
		Info.pCode = Code;
		VkShaderModule Result;
		Check(vkCreateShaderModule(Device.device, &Info, nullptr, &Result), "vkCreateShaderModule");
		return Result;
	}

	VkPipeline Pipeline(VkShaderModule Vertex, VkShaderModule Fragment, bool Blend) const
	{
		std::array<VkPipelineShaderStageCreateInfo, 2> Stages{}; // NOLINT(bugprone-invalid-enum-default-initialization): each stage is set below
		Stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		Stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		Stages[0].module = Vertex;
		Stages[0].pName = "main";
		Stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		Stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		Stages[1].module = Fragment;
		Stages[1].pName = "main";
		const VkVertexInputBindingDescription Binding{0, sizeof(TVertex), VK_VERTEX_INPUT_RATE_VERTEX};
		const std::array<VkVertexInputAttributeDescription, 3> Attributes = {{
			{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TVertex, x)},
			{1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TVertex, u)},
			{2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TVertex, r)},
		}};
		VkPipelineVertexInputStateCreateInfo Input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		Input.vertexBindingDescriptionCount = 1;
		Input.pVertexBindingDescriptions = &Binding;
		Input.vertexAttributeDescriptionCount = 3;
		Input.pVertexAttributeDescriptions = Attributes.data();
		VkPipelineInputAssemblyStateCreateInfo Assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
		Assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		VkPipelineViewportStateCreateInfo Viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		Viewport.viewportCount = 1;
		Viewport.scissorCount = 1;
		VkPipelineRasterizationStateCreateInfo Raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
		Raster.polygonMode = VK_POLYGON_MODE_FILL;
		Raster.cullMode = VK_CULL_MODE_NONE;
		Raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
		Raster.lineWidth = 1.0f;
		VkPipelineMultisampleStateCreateInfo Multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; // NOLINT(bugprone-invalid-enum-default-initialization): rasterizationSamples is set on the next line
		Multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		// surfaceDX9.cpp: D3DRS_SRCBLEND D3DBLEND_SRCALPHA, D3DRS_DESTBLEND D3DBLEND_INVSRCALPHA, for
		// alpha too (no separate alpha blend).
		VkPipelineColorBlendAttachmentState Attachment{};
		Attachment.blendEnable = Blend ? VK_TRUE : VK_FALSE;
		Attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		Attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		Attachment.colorBlendOp = VK_BLEND_OP_ADD;
		Attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		Attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		Attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		Attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo BlendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
		BlendState.attachmentCount = 1;
		BlendState.pAttachments = &Attachment;
		const std::array<VkDynamicState, 2> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo Dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
		Dynamic.dynamicStateCount = 2;
		Dynamic.pDynamicStates = DynamicStates.data();
		const VkFormat Format = ImageFormat;
		VkPipelineRenderingCreateInfo Rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
		Rendering.colorAttachmentCount = 1;
		Rendering.pColorAttachmentFormats = &Format;
		VkGraphicsPipelineCreateInfo Info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
		Info.pNext = &Rendering;
		Info.stageCount = 2;
		Info.pStages = Stages.data();
		Info.pVertexInputState = &Input;
		Info.pInputAssemblyState = &Assembly;
		Info.pViewportState = &Viewport;
		Info.pRasterizationState = &Raster;
		Info.pMultisampleState = &Multisample;
		Info.pColorBlendState = &BlendState;
		Info.pDynamicState = &Dynamic;
		Info.layout = Layout;
		VkPipeline Result;
		Check(vkCreateGraphicsPipelines(Device.device, VK_NULL_HANDLE, 1, &Info, nullptr, &Result),
			"vkCreateGraphicsPipelines");
		return Result;
	}

	void CreatePipelines()
	{
		const VkDescriptorSetLayoutBinding Binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
			VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
		VkDescriptorSetLayoutCreateInfo SetInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		SetInfo.bindingCount = 1;
		SetInfo.pBindings = &Binding;
		Check(vkCreateDescriptorSetLayout(Device.device, &SetInfo, nullptr, &SetLayout), "vkCreateDescriptorSetLayout");
		const VkPushConstantRange Range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TPushConstants)};
		VkPipelineLayoutCreateInfo LayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		LayoutInfo.setLayoutCount = 1;
		LayoutInfo.pSetLayouts = &SetLayout;
		LayoutInfo.pushConstantRangeCount = 1;
		LayoutInfo.pPushConstantRanges = &Range;
		Check(vkCreatePipelineLayout(Device.device, &LayoutInfo, nullptr, &Layout), "vkCreatePipelineLayout");

		const VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Samplers * MaxImages};
		VkDescriptorPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		PoolInfo.maxSets = Samplers * MaxImages;
		PoolInfo.poolSizeCount = 1;
		PoolInfo.pPoolSizes = &PoolSize;
		Check(vkCreateDescriptorPool(Device.device, &PoolInfo, nullptr, &DescriptorPool), "vkCreateDescriptorPool");

		// Direct3D 9's samplers: no mipmaps, D3DTADDRESS_WRAP; the third clamps (Edge copies).
		for (uint32_t i = 0; i < Samplers; i++) {
			VkSamplerCreateInfo Info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
			Info.magFilter = i == 1 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
			Info.minFilter = Info.magFilter;
			Info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			const VkSamplerAddressMode Mode = i == 2 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
			Info.addressModeU = Mode;
			Info.addressModeV = Mode;
			Info.addressModeW = Mode;
			Info.maxLod = 0.0f;
			Check(vkCreateSampler(Device.device, &Info, nullptr, &SamplerObjects[i]), "vkCreateSampler");
		}

		VkShaderModule Vertex = Module(QuadVertSpirv, sizeof(QuadVertSpirv));
		VkShaderModule Quad = Module(QuadFragSpirv, sizeof(QuadFragSpirv));
		VkShaderModule FillFragment = Module(FillFragSpirv, sizeof(FillFragSpirv));
		VkShaderModule Key = Module(ColorKeyFragSpirv, sizeof(ColorKeyFragSpirv));
		Textured = Pipeline(Vertex, Quad, true);
		Fill = Pipeline(Vertex, FillFragment, false);
		ColorKey = Pipeline(Vertex, Key, false);
		vkDestroyShaderModule(Device.device, Vertex, nullptr);
		vkDestroyShaderModule(Device.device, Quad, nullptr);
		vkDestroyShaderModule(Device.device, FillFragment, nullptr);
		vkDestroyShaderModule(Device.device, Key, nullptr);
	}

	// --- the swapchain ---

	void DestroySwapchainSemaphores()
	{
		for (VkSemaphore Semaphore : Rendered)
			vkDestroySemaphore(Device.device, Semaphore, nullptr);
		Rendered.clear();
	}

	// False when the window has no area (minimised).
	bool RecreateSwapchain(uint32_t Width, uint32_t Height)
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device.physical_device.physical_device, Surface, &Capabilities),
			"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		if (Capabilities.currentExtent.width == 0 || Capabilities.currentExtent.height == 0)
			return false;
		Check(vkDeviceWaitIdle(Device.device), "vkDeviceWaitIdle");
		// The window's surface given here: a device chosen by its index was selected without it (FindDevices).
		vkb::SwapchainBuilder Builder(Device, Surface);
		// The original engines show frames without waiting for the display (D3DPRESENT_INTERVAL_IMMEDIATE);
		// MAILBOX and FIFO only where IMMEDIATE is missing. D13's vsync comes with the settings file (D12).
		Builder.set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
			.add_fallback_format({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
			.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
			.add_fallback_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
			.add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
			.set_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			.set_desired_extent(Width, Height)
			.set_old_swapchain(Swapchain);
		const vkb::Swapchain New = Value(Builder.build(), "creating the swapchain");
		if (New.image_format != VK_FORMAT_B8G8R8A8_UNORM && New.image_format != VK_FORMAT_R8G8B8A8_UNORM) {
			vkb::destroy_swapchain(New);
			throw TVulkanError("the window offers no B8G8R8A8_UNORM or R8G8B8A8_UNORM swapchain format");
		}
		if (Swapchain.swapchain != VK_NULL_HANDLE)
			vkb::destroy_swapchain(Swapchain);
		DestroySwapchainSemaphores();
		Swapchain = New;
		SwapchainImages = Value(Swapchain.get_images(), "the swapchain's images");
		for (size_t i = 0; i < SwapchainImages.size(); i++) {
			const VkSemaphoreCreateInfo Info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			VkSemaphore Semaphore;
			Check(vkCreateSemaphore(Device.device, &Info, nullptr, &Semaphore), "vkCreateSemaphore");
			Rendered.push_back(Semaphore);
		}
		SwapchainStale = false;
		return true;
	}

	// --- creation ---

	// The instance and a device of the kind wanted; false when there is none (the instance is gone
	// again then).
	bool Open(const TDeviceRequest &Request, bool Cpu, int GpuIndex)
	{
		vkb::InstanceBuilder Builder = NewInstanceBuilder(Request.SurfaceExtension);
		if (Request.Validation)
			Builder.request_validation_layers(true).use_default_debug_messenger();
		auto Built = Builder.build();
		// A loader with no system driver at all (issue #163: GitHub's runner, measured: failed_create_instance headless,
		// requested_extensions_not_present for a window, the loader listing no surface extension) has no device of a
		// system driver to offer, which is no error: the caller goes on to the CPU driver, as ListDevices' empty list.
		if (!Cpu && !Built
			&& (Built.error() == vkb::InstanceError::failed_create_instance
				|| Built.error() == vkb::InstanceError::requested_extensions_not_present))
			return false;
		Instance = Value(std::move(Built), "creating the Vulkan instance");
		volkLoadInstanceOnly(Instance.instance);
		if (Request.CreateSurface)
			Surface = Request.CreateSurface(Instance.instance);

		// The n-th GPU counts in ListGpus' list (the advanced options' GPU button, issue #31), which does
		// not depend on the window; whether it can present to it is checked below.
		const bool Indexed = !Cpu && GpuIndex >= 0;
		std::vector<vkb::PhysicalDevice> Candidates = FindDevices(Instance, Surface, Cpu, Indexed);
		const vkb::PhysicalDevice *Chosen = nullptr;
		if (!Candidates.empty()) {
			if (Indexed) {
				if (static_cast<size_t>(GpuIndex) < Candidates.size())
					Chosen = &Candidates[static_cast<size_t>(GpuIndex)];
			} else {
				// The best kind first (discrete, integrated, virtual, other), in the loader's order.
				std::stable_sort(Candidates.begin(), Candidates.end(),
					[](const vkb::PhysicalDevice &a, const vkb::PhysicalDevice &b) {
						return TypeRank(a.properties.deviceType) < TypeRank(b.properties.deviceType);
					});
				Chosen = &Candidates.front();
			}
		}
		if (Chosen == nullptr) {
			if (Surface != VK_NULL_HANDLE)
				vkb::destroy_surface(Instance, Surface);
			Surface = VK_NULL_HANDLE;
			vkb::destroy_instance(Instance);
			Instance = vkb::Instance{};
			return false;
		}
		const vkb::DeviceBuilder DeviceBuilder(*Chosen);
		Device = Value(DeviceBuilder.build(), "creating the Vulkan device");
		volkLoadDevice(Device.device);
		const uint32_t Family = Value(Device.get_queue_index(vkb::QueueType::graphics), "the graphics queue");
		Queue = Value(Device.get_queue(vkb::QueueType::graphics), "the graphics queue");
		if (Surface != VK_NULL_HANDLE) {
			VkBool32 Supported = VK_FALSE;
			Check(vkGetPhysicalDeviceSurfaceSupportKHR(Chosen->physical_device, Family, Surface, &Supported),
				"vkGetPhysicalDeviceSurfaceSupportKHR");
			if (!Supported)
				throw TVulkanError("the graphics queue of " + Chosen->name + " cannot present to the window");
		}
		DeviceInfo = InfoOf(*Chosen, Cpu);
		return true;
	}

	void Create(const TDeviceRequest &Request)
	{
		if (volkInitialize() != VK_SUCCESS)
			throw TVulkanError("no Vulkan loader (vulkan-1.dll)");
		bool Opened = false;
		if (Request.Kind != TDeviceKind::Cpu)
			Opened = Open(Request, false, Request.Kind == TDeviceKind::Gpu ? Request.GpuIndex : -1);
		if (!Opened && Request.Kind != TDeviceKind::Gpu) {
			if (Request.CpuDriverManifest.empty())
				throw TVulkanError("no GPU with Vulkan 1.3 and no CPU driver");
			const TDriverFiles Driver(Request.CpuDriverManifest);
			Opened = Open(Request, true, -1);
			if (!Opened)
				throw TVulkanError("the CPU driver " + Request.CpuDriverManifest
					+ " offers no device (an elevated process ignores VK_DRIVER_FILES)");
		}
		if (!Opened)
			throw TVulkanError("no GPU with Vulkan 1.3");

		VmaVulkanFunctions Functions{};
		Functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		Functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
		VmaAllocatorCreateInfo AllocatorInfo{};
		AllocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
		AllocatorInfo.physicalDevice = Device.physical_device.physical_device;
		AllocatorInfo.device = Device.device;
		AllocatorInfo.instance = Instance.instance;
		AllocatorInfo.pVulkanFunctions = &Functions;
		Check(vmaCreateAllocator(&AllocatorInfo, &Allocator), "vmaCreateAllocator");

		CreatePipelines();
		Images.reserve(MaxImages);
		for (TFrame &F : Frames) {
			VkCommandPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
			PoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			PoolInfo.queueFamilyIndex = Value(Device.get_queue_index(vkb::QueueType::graphics), "the graphics queue");
			Check(vkCreateCommandPool(Device.device, &PoolInfo, nullptr, &F.Pool), "vkCreateCommandPool");
			VkCommandBufferAllocateInfo BufferInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			BufferInfo.commandPool = F.Pool;
			BufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			BufferInfo.commandBufferCount = 1;
			Check(vkAllocateCommandBuffers(Device.device, &BufferInfo, &F.Commands), "vkAllocateCommandBuffers");
			VkFenceCreateInfo FenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			Check(vkCreateFence(Device.device, &FenceInfo, nullptr, &F.Fence), "vkCreateFence");
			const VkSemaphoreCreateInfo SemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			Check(vkCreateSemaphore(Device.device, &SemaphoreInfo, nullptr, &F.Acquired), "vkCreateSemaphore");
		}
		FrameIndex = 0;
		BeginFrame();
	}

	void Destroy()
	{
		if (Device.device != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(Device.device);
			if (Recording) {
				vkEndCommandBuffer(Commands());
				Recording = false;
			}
			for (TImage &Image : Images)
				FreeImage(Image);
			Images.clear();
			DestroyBuffer(ReadBuffer);
			for (TFrame &F : Frames) {
				for (TBuffer &Chunk : F.Vertices)
					DestroyBuffer(Chunk);
				for (TBuffer &Chunk : F.Staging)
					DestroyBuffer(Chunk);
				F.Vertices.clear();
				F.Staging.clear();
				if (F.Acquired != VK_NULL_HANDLE)
					vkDestroySemaphore(Device.device, F.Acquired, nullptr);
				if (F.Fence != VK_NULL_HANDLE)
					vkDestroyFence(Device.device, F.Fence, nullptr);
				if (F.Pool != VK_NULL_HANDLE)
					vkDestroyCommandPool(Device.device, F.Pool, nullptr);
				if (F.Timestamps != VK_NULL_HANDLE)
					vkDestroyQueryPool(Device.device, F.Timestamps, nullptr);
				F = TFrame{};
			}
			DestroySwapchainSemaphores();
			if (Swapchain.swapchain != VK_NULL_HANDLE)
				vkb::destroy_swapchain(Swapchain);
			Swapchain = vkb::Swapchain{};
			SwapchainImages.clear();
			for (VkPipeline *P : {&Textured, &Fill, &ColorKey}) {
				if (*P != VK_NULL_HANDLE)
					vkDestroyPipeline(Device.device, *P, nullptr);
				*P = VK_NULL_HANDLE;
			}
			for (VkSampler &S : SamplerObjects) {
				if (S != VK_NULL_HANDLE)
					vkDestroySampler(Device.device, S, nullptr);
				S = VK_NULL_HANDLE;
			}
			if (DescriptorPool != VK_NULL_HANDLE)
				vkDestroyDescriptorPool(Device.device, DescriptorPool, nullptr);
			if (Layout != VK_NULL_HANDLE)
				vkDestroyPipelineLayout(Device.device, Layout, nullptr);
			if (SetLayout != VK_NULL_HANDLE)
				vkDestroyDescriptorSetLayout(Device.device, SetLayout, nullptr);
			DescriptorPool = VK_NULL_HANDLE;
			Layout = VK_NULL_HANDLE;
			SetLayout = VK_NULL_HANDLE;
			if (Allocator != nullptr)
				vmaDestroyAllocator(Allocator);
			Allocator = nullptr;
			vkb::destroy_device(Device);
			Device = vkb::Device{};
		}
		if (Surface != VK_NULL_HANDLE)
			vkb::destroy_surface(Instance, Surface);
		Surface = VK_NULL_HANDLE;
		if (Instance.instance != VK_NULL_HANDLE)
			vkb::destroy_instance(Instance);
		Instance = vkb::Instance{};
		KeyScratch = -1;
		Feedback = -1;
	}
};

TRenderer::TRenderer() = default;

TRenderer::~TRenderer()
{
	Destroy();
}

void TRenderer::Create(const TDeviceRequest &Request)
{
	Destroy();
	State = std::make_unique<TState>();
	try {
		State->Create(Request);
	} catch (...) {
		State->Destroy();
		State.reset();
		throw;
	}
}

void TRenderer::Destroy()
{
	if (State != nullptr) {
		State->Destroy();
		State.reset();
	}
}

bool TRenderer::Created() const
{
	return State != nullptr;
}

const TDeviceInfo &TRenderer::Device() const
{
	return State->DeviceInfo;
}

int TRenderer::CreateImage(int Width, int Height)
{
	return State->NewImage(Width, Height);
}

void TRenderer::ResizeImage(int Image, int Width, int Height)
{
	State->Resize(Image, Width, Height);
}

int TRenderer::ImageWidth(int Image) const
{
	return State->Images[static_cast<size_t>(Image)].Width;
}

int TRenderer::ImageHeight(int Image) const
{
	return State->Images[static_cast<size_t>(Image)].Height;
}

void TRenderer::Fill(int Target, int Left, int Top, int Right, int Bottom, uint32_t Color)
{
	TState &S = *State;
	S.BeginPass(Target);
	S.Bind(S.Fill, S.Constants(Target));
	std::array<TVertex, 6> Vertices = FillVertices(Left, Top, Right, Bottom, Color);
	S.DrawVertices(Vertices.data(), Vertices.size());
}

void TRenderer::Draw(int Target, int Texture, TFilter Filter, const TQuad *Quads, size_t Count)
{
	TState &S = *State;
	if (Count == 0)
		return;
	int Source = Texture;
	if (Texture == Target) {
		// A target cannot be sampled while it is drawn into: the draw reads a copy taken now.
		S.EndPass();
		const int Width = S.Images[static_cast<size_t>(Texture)].Width;
		const int Height = S.Images[static_cast<size_t>(Texture)].Height;
		if (S.Feedback < 0)
			S.Feedback = S.NewImage(Width, Height);
		else
			S.Resize(S.Feedback, Width, Height);
		const TImage &Copy = S.Images[static_cast<size_t>(S.Feedback)];
		VkImageCopy Region{};
		Region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		Region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		Region.extent = {ToUint(Width), ToUint(Height), 1};
		vkCmdCopyImage(S.Commands(), S.Images[static_cast<size_t>(Texture)].Image, VK_IMAGE_LAYOUT_GENERAL, Copy.Image,
			VK_IMAGE_LAYOUT_GENERAL, 1, &Region);
		S.Barrier();
		Source = S.Feedback;
	}
	const TImage &Image = S.Images[static_cast<size_t>(Source)];
	std::vector<TVertex> &Vertices = S.QuadVertices;
	std::vector<uint32_t> &Sampler = S.QuadSamplers;
	Vertices.clear();
	Sampler.clear();
	Vertices.reserve(Count * 6);
	Sampler.reserve(Count);
	for (size_t i = 0; i < Count; i++) {
		TQuadVertices Quad = ConvertQuad(Quads[i], Image.Width, Image.Height, Filter);
		Vertices.insert(Vertices.end(), Quad.Vertices.begin(), Quad.Vertices.end());
		uint32_t Set = 1; // linear
		if (Quad.Clamp)
			Set = 2;
		else if (Quad.Nearest || Filter == TFilter::Point)
			Set = 0;
		Sampler.push_back(Set);
	}
	S.BeginPass(Target);
	S.Bind(S.Textured, S.Constants(Target));
	// Runs of quads with the same sampler, in order.
	size_t First = 0;
	while (First < Count) {
		size_t Last = First + 1;
		while (Last < Count && Sampler[Last] == Sampler[First])
			Last++;
		S.BindSet(Image.Sets[Sampler[First]]);
		S.DrawVertices(&Vertices[First * 6], (Last - First) * 6);
		First = Last;
	}
}

void TRenderer::Upload(int Image, int x, int y, int Width, int Height, const uint32_t *Pixels, size_t Pitch)
{
	TState &S = *State;
	if (Width <= 0 || Height <= 0)
		return;
	S.EndPass();
	const VkDeviceSize Size = static_cast<VkDeviceSize>(Width) * static_cast<VkDeviceSize>(Height) * 4;
	VkDeviceSize Offset = 0;
	const TBuffer &Buffer = S.Allocate(S.Frame().Staging, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, Offset);
	for (int Row = 0; Row < Height; Row++)
		std::memcpy(Buffer.Mapped + Offset + static_cast<size_t>(Row) * static_cast<size_t>(Width) * 4,
			Pixels + static_cast<size_t>(Row) * Pitch, static_cast<size_t>(Width) * 4);
	Check(vmaFlushAllocation(S.Allocator, Buffer.Allocation, Offset, Size), "vmaFlushAllocation");
	VkBufferImageCopy Region{};
	Region.bufferOffset = Offset;
	Region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	Region.imageOffset = {x, y, 0};
	Region.imageExtent = {ToUint(Width), ToUint(Height), 1};
	vkCmdCopyBufferToImage(S.Commands(), Buffer.Buffer, S.Images[static_cast<size_t>(Image)].Image,
		VK_IMAGE_LAYOUT_GENERAL, 1, &Region);
	S.Barrier();
}

void TRenderer::ReplaceColor(int Image, int x, int y, int Width, int Height, uint32_t Key, uint32_t Replacement)
{
	TState &S = *State;
	if (Width <= 0 || Height <= 0)
		return;
	S.EndPass();
	if (S.KeyScratch < 0)
		S.KeyScratch = S.NewImage(Width, Height);
	else if (S.ImageTooSmall(S.KeyScratch, Width, Height))
		S.Resize(S.KeyScratch, std::max(Width, S.Images[static_cast<size_t>(S.KeyScratch)].Width),
			std::max(Height, S.Images[static_cast<size_t>(S.KeyScratch)].Height));
	VkImageCopy Region{};
	Region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	Region.srcOffset = {x, y, 0};
	Region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	Region.extent = {ToUint(Width), ToUint(Height), 1};
	vkCmdCopyImage(S.Commands(), S.Images[static_cast<size_t>(Image)].Image, VK_IMAGE_LAYOUT_GENERAL,
		S.Images[static_cast<size_t>(S.KeyScratch)].Image, VK_IMAGE_LAYOUT_GENERAL, 1, &Region);
	S.Barrier();
	S.BeginPass(Image);
	TPushConstants Constants = S.Constants(Image);
	Constants.OffsetX = x;
	Constants.OffsetY = y;
	Constants.Key[0] = (Key >> 16) & 0xff;
	Constants.Key[1] = (Key >> 8) & 0xff;
	Constants.Key[2] = Key & 0xff;
	Constants.Key[3] = (Key >> 24) & 0xff;
	Constants.Replacement[0] = static_cast<float>((Replacement >> 16) & 0xff) / 255.0f;
	Constants.Replacement[1] = static_cast<float>((Replacement >> 8) & 0xff) / 255.0f;
	Constants.Replacement[2] = static_cast<float>(Replacement & 0xff) / 255.0f;
	Constants.Replacement[3] = static_cast<float>((Replacement >> 24) & 0xff) / 255.0f;
	S.Bind(S.ColorKey, Constants);
	S.BindSet(S.Images[static_cast<size_t>(S.KeyScratch)].Sets[0]);
	std::array<TVertex, 6> Vertices = FillVertices(x, y, x + Width, y + Height, 0);
	S.DrawVertices(Vertices.data(), Vertices.size());
}

void TRenderer::ReadBack(int Image, int x, int y, int Width, int Height, std::vector<uint32_t> &Pixels)
{
	TState &S = *State;
	Pixels.assign(static_cast<size_t>(std::max(Width, 0)) * static_cast<size_t>(std::max(Height, 0)), 0);
	if (Width <= 0 || Height <= 0)
		return;
	S.EndPass();
	const VkDeviceSize Size = static_cast<VkDeviceSize>(Width) * static_cast<VkDeviceSize>(Height) * 4;
	if (S.ReadBuffer.Size < Size) {
		// Only ReadBack uses it, and it waits until the copy is done: nothing uses the old one now.
		S.DestroyBuffer(S.ReadBuffer);
		S.ReadBuffer = S.CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	}
	VkBufferImageCopy Region{};
	Region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	Region.imageOffset = {x, y, 0};
	Region.imageExtent = {ToUint(Width), ToUint(Height), 1};
	vkCmdCopyImageToBuffer(S.Commands(), S.Images[static_cast<size_t>(Image)].Image, VK_IMAGE_LAYOUT_GENERAL,
		S.ReadBuffer.Buffer, 1, &Region);
	VkMemoryBarrier2 Memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
	Memory.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
	Memory.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	Memory.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	Memory.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
	VkDependencyInfo Dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	Dependency.memoryBarrierCount = 1;
	Dependency.pMemoryBarriers = &Memory;
	vkCmdPipelineBarrier2(S.Commands(), &Dependency);
	S.Barrier();
	S.SubmitAndWait();
	Check(vmaInvalidateAllocation(S.Allocator, S.ReadBuffer.Allocation, 0, Size), "vmaInvalidateAllocation");
	std::memcpy(Pixels.data(), S.ReadBuffer.Mapped, static_cast<size_t>(Size));
}

bool TRenderer::Present(int Image, int x, int y, int Width, int Height, uint32_t WindowWidth, uint32_t WindowHeight)
{
	TState &S = *State;
	S.EndPass();
	if (S.Surface == VK_NULL_HANDLE || WindowWidth == 0 || WindowHeight == 0 || Width <= 0 || Height <= 0) {
		S.Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
		S.NextFrame();
		return false;
	}
	if (S.Swapchain.swapchain == VK_NULL_HANDLE || S.SwapchainStale || S.Swapchain.extent.width != WindowWidth
		|| S.Swapchain.extent.height != WindowHeight) {
		if (!S.RecreateSwapchain(WindowWidth, WindowHeight)) {
			S.Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
			S.NextFrame();
			return false;
		}
	}
	uint32_t Index = 0;
	const VkResult Acquired = vkAcquireNextImageKHR(S.Device.device, S.Swapchain.swapchain, UINT64_MAX, S.Frame().Acquired,
		VK_NULL_HANDLE, &Index);
	if (Acquired == VK_ERROR_OUT_OF_DATE_KHR) {
		S.SwapchainStale = true;
		S.Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
		S.NextFrame();
		return false;
	}
	if (Acquired == VK_SUBOPTIMAL_KHR)
		S.SwapchainStale = true;
	else
		Check(Acquired, "vkAcquireNextImageKHR");
	VkImage Target = S.SwapchainImages[Index];
	const TImage &From = S.Images[static_cast<size_t>(Image)];
	S.ImageBarrier(Target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
	const int Right = std::min(x + Width, From.Width);
	const int Bottom = std::min(y + Height, From.Height);
	VkImageBlit Region{};
	Region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	Region.srcOffsets[0] = {x, y, 0};
	Region.srcOffsets[1] = {Right, Bottom, 1};
	Region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	Region.dstOffsets[0] = {0, 0, 0};
	Region.dstOffsets[1] = {static_cast<int32_t>(S.Swapchain.extent.width),
		static_cast<int32_t>(S.Swapchain.extent.height), 1};
	const bool OneToOne = static_cast<uint32_t>(Right - x) == S.Swapchain.extent.width
		&& static_cast<uint32_t>(Bottom - y) == S.Swapchain.extent.height;
	vkCmdBlitImage(S.Commands(), From.Image, VK_IMAGE_LAYOUT_GENERAL, Target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
		&Region, OneToOne ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
	S.ImageBarrier(Target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);
	S.Barrier();
	VkSemaphore Rendered = S.Rendered[Index];
	S.Submit(S.Frame().Acquired, Rendered);
	VkPresentInfoKHR Info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	Info.waitSemaphoreCount = 1;
	Info.pWaitSemaphores = &Rendered;
	Info.swapchainCount = 1;
	Info.pSwapchains = &S.Swapchain.swapchain;
	Info.pImageIndices = &Index;
	const VkResult Presented = vkQueuePresentKHR(S.Queue, &Info);
	if (Presented == VK_ERROR_OUT_OF_DATE_KHR || Presented == VK_SUBOPTIMAL_KHR)
		S.SwapchainStale = true;
	else
		Check(Presented, "vkQueuePresentKHR");
	S.NextFrame();
	return true;
}

void TRenderer::Finish()
{
	TState &S = *State;
	S.SubmitAndWait();
	Check(vkDeviceWaitIdle(S.Device.device), "vkDeviceWaitIdle");
}

bool TRenderer::SetFrameTimestamps(bool On)
{
	TState &S = *State;
	if (!On) {
		S.TimestampsOn = false;
		return true;
	}
	return S.EnableTimestamps();
}

void TRenderer::TakeFrameTimes(std::vector<uint64_t> &Nanoseconds)
{
	TState &S = *State;
	Nanoseconds.insert(Nanoseconds.end(), S.FrameTimes.begin(), S.FrameTimes.end());
	S.FrameTimes.clear();
}

VkPresentModeKHR TRenderer::PresentMode() const
{
	return State->Swapchain.present_mode;
}

} // namespace patella::vulkan
