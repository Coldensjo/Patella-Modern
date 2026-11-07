// LogSettings.cpp: the developer log's settings in Patella.ini (LogSettings.h; Patella-Modern issue #77).

#include "log/LogSettings.h"

#include "log/Log.h"
#include "platform/FileSystem.h"
#include "settings/Settings.h"

#include <optional>
#include <string>

namespace patella::log {

void Configure(TLog &Target, const patella::settings::TSettingsFile &Settings, patella::platform::IFileSystem &Files)
{
	if (const std::optional<std::string> Name = Settings.Get(LevelKey)) {
		if (const std::optional<TLevel> Level = ParseLevel(*Name))
			Target.SetLevel(*Level);
		else
			Target.Warn("{}: {} \"{}\" is not error, warn, info or debug; {} kept", patella::settings::FileName,
				LevelKey, *Name, LevelName(Target.GetLevel()));
	}
	std::string Opened;
	if (const std::optional<std::string> Value = Settings.Get(FileKey); Value.has_value() && !Value->empty()) {
		// The client's text, as every name TFile opens (issue #117: converted from the file's UTF-8).
		const std::string Path = patella::settings::ClientText(Settings, *Value);
		if (Target.OpenFile(Files, Path))
			Opened = Path;
		else
			Target.Warn("{}: {} \"{}\" cannot be opened for appending; no log file", patella::settings::FileName, FileKey,
				Path);
	}
	Target.Debug("{}: level {}, {}{}", patella::settings::FileName, LevelName(Target.GetLevel()),
		Opened.empty() ? "no log file" : "log file ", Opened);
}

void LoadSettings(patella::platform::IFileSystem &Files)
{
	if (const std::optional<patella::settings::TSettingsFile> Settings = patella::settings::Load(Files, patella::settings::FileName))
		Configure(Log(), *Settings, Files);
}

} // namespace patella::log
