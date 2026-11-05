// LogSettings.cpp: the developer log's settings in Patella.ini (LogSettings.h; Patella-Modern issue #77).

#include "log/LogSettings.h"

#include "log/Log.h"
#include "settings/Settings.h"

#include <optional>
#include <string>

namespace patella::log {

void Configure(TLog &Target, const patella::settings::TSettingsFile &Settings)
{
	if (const std::optional<std::string> Name = Settings.Get(LevelKey)) {
		if (const std::optional<TLevel> Level = ParseLevel(*Name))
			Target.SetLevel(*Level);
		else
			Target.Warn("{}: {} \"{}\" is not error, warn, info or debug; {} kept", patella::settings::FileName,
				LevelKey, *Name, LevelName(Target.GetLevel()));
	}
	std::string Opened;
	if (const std::optional<std::string> Path = Settings.Get(FileKey); Path.has_value() && !Path->empty()) {
		if (Target.OpenFile(*Path))
			Opened = *Path;
		else
			Target.Warn("{}: {} \"{}\" cannot be opened for appending; no log file", patella::settings::FileName, FileKey,
				*Path);
	}
	Target.Debug("{}: level {}, {}{}", patella::settings::FileName, LevelName(Target.GetLevel()),
		Opened.empty() ? "no log file" : "log file ", Opened);
}

void LoadSettings()
{
	if (const std::optional<patella::settings::TSettingsFile> Settings = patella::settings::Load(patella::settings::FileName))
		Configure(Log(), *Settings);
}

} // namespace patella::log
