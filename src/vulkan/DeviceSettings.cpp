// DeviceSettings.cpp: the x64 client's graphics device choice (DeviceSettings.h; issue #31).

#include "vulkan/DeviceSettings.h"

#include "platform/Platform.h"
#include "settings/Settings.h"
#include "vulkan/Renderer.h"
#include "vulkan/DeviceChoice.h"
#include "compat/msvc.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <array>
#include <windows.h>
#else
#include <filesystem>
#include <system_error>
#endif

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

using patella::vulkan::TDeviceButton;
using patella::vulkan::TDeviceChoice;

namespace {

constexpr const char *DeviceKey = "Device";

TDeviceChoice Choice;
std::vector<patella::vulkan::TDeviceInfo> Gpus;
int Best = -1;
std::string CpuDriverManifest;

// An environment variable's value; nothing when it is not set or empty.
std::optional<std::string> Environment(const char *Name)
{
	char *Value = nullptr;
	size_t Length = 0;
	if (MsvcDupenv(&Value, &Length, Name) != 0 || Value == nullptr)
		return std::nullopt;
	std::string Result(Value);
	std::free(Value); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): MsvcDupenv's copy
	if (Result.empty())
		return std::nullopt;
	return Result;
}

#if !defined(_WIN32)
// Linux (issue #131): Mesa's lavapipe as the system has it (the distribution's mesa-vulkan-drivers package; Windows has
// its copy next to the executable, cmake/Vulkan.cmake). Empty where it is not installed.
std::string FindCpuDriverManifest()
{
	for (const char *Folder : {"/usr/share/vulkan/icd.d", "/etc/vulkan/icd.d", "/usr/local/share/vulkan/icd.d"}) {
		for (const char *Name : {"lvp_icd.json", "lvp_icd.x86_64.json"}) {
			const std::filesystem::path Manifest = std::filesystem::path(Folder) / Name;
			std::error_code Error;
			if (std::filesystem::is_regular_file(Manifest, Error))
				return Manifest.string();
		}
	}
	return {};
}
#else
std::string FindCpuDriverManifest()
{
	std::array<char, MAX_PATH> Path;
	const DWORD Length = GetModuleFileNameA(nullptr, Path.data(), MAX_PATH);
	if (Length == 0 || Length >= MAX_PATH)
		return {};
	std::string Manifest(Path.data(), Length);
	const size_t Slash = Manifest.find_last_of("\\/");
	Manifest = Manifest.substr(0, Slash + 1) + "lvp_icd.x86_64.json";
	if (GetFileAttributesA(Manifest.c_str()) == INVALID_FILE_ATTRIBUTES)
		return {};
	return Manifest;
}
#endif

} // namespace

void LoadDeviceChoice()
{
	Choice = TDeviceChoice{};
	if (std::optional<std::string> Override = Environment("PATELLA_VULKAN")) {
		// As before issue #31: a value it does not know (off, dx9 ...) is automatic.
		Choice = patella::vulkan::ParseDeviceChoice(*Override).value_or(TDeviceChoice{});
	} else if (std::optional<patella::settings::TSettingsFile> File = patella::settings::Load(patella::platform::Platform().FileSystem, patella::settings::FileName)) {
		if (std::optional<std::string> Value = File->Get(DeviceKey))
			Choice = patella::vulkan::ParseDeviceChoice(*Value).value_or(TDeviceChoice{});
	}
	CpuDriverManifest = FindCpuDriverManifest();
	Gpus.clear();
	try {
		if (patella::vulkan::LoaderAvailable())
			Gpus = patella::vulkan::ListGpus();
	} catch (const patella::vulkan::TVulkanError &) {
		Gpus.clear();
	}
	Best = patella::vulkan::BestGpu(Gpus);
}

TDeviceChoice GetDeviceChoice()
{
	return Choice;
}

void SetDeviceChoice(const TDeviceChoice &NewChoice)
{
	Choice = NewChoice;
}

bool SaveDeviceChoice()
{
	// A file that is there but cannot be read (or is too large to be a settings file) is left alone.
	std::optional<patella::settings::TSettingsFile> File = patella::settings::LoadForUpdate(patella::platform::Platform().FileSystem, patella::settings::FileName);
	if (!File || !File->Set(DeviceKey, patella::vulkan::FormatDeviceChoice(Choice)))
		return false;
	return patella::settings::Save(patella::platform::Platform().FileSystem, patella::settings::FileName, *File);
}

int GetNumberOfGpus()
{
	return static_cast<int>(Gpus.size());
}

int GetBestGpu()
{
	return Best;
}

const char *GetGpuName(int Index)
{
	if (Index < 0 || Index >= GetNumberOfGpus())
		return "";
	return Gpus[static_cast<size_t>(Index)].Name.c_str();
}

const std::string &GetCpuDriverManifest()
{
	return CpuDriverManifest;
}

TDeviceButton DeviceButtonOfEngine(int Engine)
{
	switch (Engine) {
	case 1:
		return TDeviceButton::Cpu;
	case 2:
		return TDeviceButton::Gpu;
	default:
		return TDeviceButton::Automatic;
	}
}
