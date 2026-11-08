// DataLocationWin32.cpp: where the Windows client's files are (Patella-Modern issue #130, plan M8, D14; notes/decisions.md,
// "#130"). Not part of the original source. Patella.exe and PatellaTest.exe compile it; the golden runner never calls it
// (its files stay the working folder's: tests/golden/client_stubs.cpp).
//
// The inputs of settings::ChooseLocations from the process: the working folder (std::filesystem::current_path), the
// executable's folder (GetModuleFileNameW), the command line's --data <folder> (CommandLineToArgvW: a folder with
// spaces in quotes, any characters). Windows' file system finds names without regard to case itself, so the lookup's
// own (FindNoCase) is off, and the XDG option is Linux's.

#include "platform/Win32Platform.h"

#include "log/Log.h"
#include "platform/FileSystem.h"
#include "settings/Settings.h"
#include "types/Cp1252.h"

#include <shellapi.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace patella::platform {

namespace {

// The choice ConfigureDataFiles made, for LogDataFiles.
settings::TLocationChoice &Chosen()
{
	static settings::TLocationChoice &Choice = *new settings::TLocationChoice; // NOLINT(cppcoreguidelines-owning-memory): never destroyed
	return Choice;
}

std::filesystem::path ExecutableFolder()
{
	std::wstring Name(MAX_PATH, L'\0');
	while (true) {
		const DWORD Length = ::GetModuleFileNameW(nullptr, Name.data(), static_cast<DWORD>(Name.size()));
		if (Length == 0)
			return {};
		if (Length < Name.size()) {
			Name.resize(Length);
			return std::filesystem::path(Name).parent_path();
		}
		if (Name.size() >= 32768)
			return {};
		Name.resize(Name.size() * 2);
	}
}

std::vector<std::wstring> Arguments()
{
	std::vector<std::wstring> Result;
	int Count = 0;
	LPWSTR *List = ::CommandLineToArgvW(::GetCommandLineW(), &Count);
	if (List == nullptr)
		return Result;
	for (int i = 1; i < Count; i++)
		Result.emplace_back(List[i]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): CommandLineToArgvW's array of Count
	::LocalFree(static_cast<HLOCAL>(List));
	return Result;
}

std::string Shown(const std::filesystem::path &Folder)
{
	return text::WideToCp1252(Folder.native());
}

} // namespace

void ConfigureDataFiles()
{
	try {
		settings::TLocationInputs Inputs;
		std::error_code Error;
		Inputs.Working = std::filesystem::current_path(Error);
		if (Error)
			return; // the working folder's files, as before
		Inputs.Executable = ExecutableFolder();
		Inputs.DataOption = settings::DataOption(Arguments());
		Chosen() = settings::ChooseLocations(Inputs);
		SystemFileSystem().SetLocations(Chosen().Locations);
	} catch (...) { // std::bad_alloc: the working folder's files, as before
		Chosen() = {};
	}
}

void LogDataFiles()
{
	const settings::TLocationChoice &Choice = Chosen();
	if (Choice.DataFolder.empty())
		return;
	if (Choice.Source == settings::TDataSource::Working) {
		log::Debug("data files: {} ({})", Shown(Choice.DataFolder), settings::SourceName(Choice.Source));
		return;
	}
	log::Info("data files: {} ({}); {}: {}", Shown(Choice.DataFolder), settings::SourceName(Choice.Source),
		settings::FileName, Shown(Choice.SettingsFolder));
	std::error_code Error;
	if (Choice.Source == settings::TDataSource::Option && !std::filesystem::exists(Choice.DataFolder / "Tibia.dat", Error))
		log::Warn("--data {}: no Tibia.dat there", Shown(Choice.DataFolder));
}

} // namespace patella::platform
