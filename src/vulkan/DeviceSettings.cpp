// DeviceSettings.cpp: the x64 client's graphics device choice (DeviceSettings.h; issue #31).

#include "vulkan/DeviceSettings.h"

#include "settings/Settings.h"
#include "vulkan/Renderer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdlib>
#include <optional>
#include <vector>

using patella::vulkan::TDeviceButton;
using patella::vulkan::TDeviceChoice;

namespace {

constexpr const char *DeviceKey = "Device";
// Tibia.cfg's line ends (Preferences.cpp writes it in text mode), for a new Patella.ini.
constexpr const char *NewFileNewline = "\r\n";
constexpr const char *NewFileComment = "# Patella-Modern's own settings (Tibia.cfg stays the original client's)";

TDeviceChoice Choice;
std::vector<patella::vulkan::TDeviceInfo> Gpus;
int Best = -1;
std::string CpuDriverManifest;

// An environment variable's value; nothing when it is not set or empty.
std::optional<std::string> Environment(const char *Name)
{
	char *Value = nullptr;
	size_t Length = 0;
	if (_dupenv_s(&Value, &Length, Name) != 0 || Value == nullptr)
		return std::nullopt;
	std::string Result(Value);
	std::free(Value);
	if (Result.empty())
		return std::nullopt;
	return Result;
}

std::string FindCpuDriverManifest()
{
	char Path[MAX_PATH];
	DWORD Length = GetModuleFileNameA(NULL, Path, MAX_PATH);
	if (Length == 0 || Length >= MAX_PATH)
		return {};
	std::string Manifest(Path, Length);
	size_t Slash = Manifest.find_last_of("\\/");
	Manifest = Manifest.substr(0, Slash + 1) + "lvp_icd.x86_64.json";
	if (GetFileAttributesA(Manifest.c_str()) == INVALID_FILE_ATTRIBUTES)
		return {};
	return Manifest;
}

} // namespace

void LoadDeviceChoice()
{
	Choice = TDeviceChoice{};
	if (std::optional<std::string> Override = Environment("PATELLA_VULKAN")) {
		// As before issue #31: a value it does not know (off, dx9 ...) is automatic.
		Choice = patella::vulkan::ParseDeviceChoice(*Override).value_or(TDeviceChoice{});
	} else if (std::optional<patella::settings::TSettingsFile> File = patella::settings::Load(patella::settings::FileName)) {
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
	std::optional<patella::settings::TSettingsFile> File = patella::settings::Load(patella::settings::FileName,
		NewFileNewline);
	if (!File) {
		// A file that is there but cannot be read (or is too large to be a settings file) is left alone.
		if (GetFileAttributesA(patella::settings::FileName) != INVALID_FILE_ATTRIBUTES)
			return false;
		File = patella::settings::TSettingsFile::Parse(std::string(NewFileComment) + NewFileNewline, NewFileNewline);
	}
	if (!File->Set(DeviceKey, patella::vulkan::FormatDeviceChoice(Choice)))
		return false;
	return patella::settings::Save(patella::settings::FileName, *File);
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
