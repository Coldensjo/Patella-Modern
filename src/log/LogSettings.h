// LogSettings.h: the developer log's settings in Patella.ini (Patella-Modern issue #77; plans D12, D16). Not part of
// the original source.
//
// Two keys of Patella.ini (settings/Settings.h), read once at the start (Player.cpp InitInstance, the x64 client):
//
//	LogLevel=debug          error, warn, info (the default) or debug, without regard to case (log/Log.h)
//	LogFile=patella.log     a file the lines are appended to as well (created where it does not exist; the name in
//	                        UTF-8, issue #117: settings::ClientText); none
//	                        without the key or with an empty value. A relative name is from the working directory,
//	                        Tibia.cfg's and Patella.ini's.
//
// Nothing writes the keys: the user adds them. Without Patella.ini, or without the keys, the log stays at its defaults
// (info, stderr only), so no new file appears. A value that cannot be used is reported at the warn level and the
// default is kept. Part of patella_core (no platform header), tested by tests_core (tests/unit/core_log_settings_test.cpp).

#pragma once

#include "log/Log.h"
#include "platform/FileSystem.h"
#include "settings/Settings.h"

namespace patella::log {

inline constexpr const char *LevelKey = "LogLevel";
inline constexpr const char *FileKey = "LogFile";

// Sets Target's level and file from Settings' keys (above); the other keys are not read. The log file is opened
// through Files (issue #119).
void Configure(TLog &Target, const patella::settings::TSettingsFile &Settings, patella::platform::IFileSystem &Files);

// Reads Patella.ini (settings::FileName) through Files (the platform's IFileSystem: the working folder's) and
// configures Log() with it; nothing without the file.
void LoadSettings(patella::platform::IFileSystem &Files);

} // namespace patella::log
