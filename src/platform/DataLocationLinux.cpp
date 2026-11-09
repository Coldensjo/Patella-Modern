// DataLocationLinux.cpp: where the Linux client's files are (Patella-Modern issue #131; notes/decisions.md, "#131"; the
// rules are settings::ChooseLocations', issue #130). The counterpart of DataLocationWin32.cpp: the inputs from the
// process, with Linux's own: the executable's folder from /proc/self/exe, the original names found without regard to
// ASCII case (platform::FindNoCase, the data files of a Windows install on an ext4 disk), and the XDG option, the
// command line's word --xdg-config (Patella.ini in $XDG_CONFIG_HOME/patella, else $HOME/.config/patella).

#include "platform/FileSystem.h"
#include "platform/Process.h"

#include "log/Log.h"
#include "settings/Settings.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

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
	std::error_code Error;
	const std::filesystem::path Self = std::filesystem::read_symlink("/proc/self/exe", Error);
	if (Error)
		return {};
	return Self.parent_path();
}

bool WantsXdg()
{
	for (const std::string &Argument : Arguments())
		if (Argument == "--xdg-config")
			return true;
	return false;
}

const char *Environment(const char *Name)
{
	return std::getenv(Name); // NOLINT(concurrency-mt-unsafe): read at the start, before any thread
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
		Inputs.Xdg = WantsXdg();
		Inputs.XdgFolder = settings::XdgSettingsFolder(Environment("XDG_CONFIG_HOME"), Environment("HOME"));
		Inputs.CaseInsensitive = true;
		Chosen() = settings::ChooseLocations(Inputs);
		SystemFileSystem().SetLocations(Chosen().Locations);
		// The XDG folder may not exist yet: Patella.ini is saved there.
		if (Inputs.Xdg && !Inputs.XdgFolder.empty())
			std::filesystem::create_directories(Inputs.XdgFolder, Error);
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
		log::Debug("data files: {} ({})", Choice.DataFolder.string(), settings::SourceName(Choice.Source));
		return;
	}
	log::Info("data files: {} ({}); {}: {}", Choice.DataFolder.string(), settings::SourceName(Choice.Source),
		settings::FileName, Choice.SettingsFolder.string());
	std::error_code Error;
	if (Choice.Source == settings::TDataSource::Option && !std::filesystem::exists(Choice.DataFolder / "Tibia.dat", Error))
		log::Warn("--data {}: no Tibia.dat there", Choice.DataFolder.string());
}

} // namespace patella::platform
