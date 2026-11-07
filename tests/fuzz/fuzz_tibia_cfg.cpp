// fuzz_tibia_cfg: the import of Tibia.cfg (Preferences.cpp; Patella-Modern issues #45, #132): the input is Tibia.cfg.
// InitPreferences with no Patella.ini imports it with the original's reader (TPreferences::ReadTibiaCfg, Script.cpp's
// TReadScriptFile: a file it cannot read gives the defaults) and writes Patella.ini at once; ExitPreferences saves it
// again. The rest is the fork's (#45), and a difference there a finding of ours:
// - Patella.ini read back (settings::ReadPreferences) gives the options the client held after the import, up to what
//   it cannot hold (values.h Held).
// - The start after it: InitPreferences reads that Patella.ini, and the client holds the same options again; a third
//   start holds what the second did. Not compared with the import: OpenDialogs (SetPreferenceValues keeps
//   ReadTibiaCfg's duplicate check, which reads the vector's entries as they are, those of the first load in this
//   process included, and the client loads once per process), White and Ignore (a name longer than the list keeps is
//   kept cut, so the import can hold one twice; saved, both are the same cut name, which the next load adds once:
//   Tibia.exe's next start does the same).

#include "client.h"
#include "values.h"

#include "Preferences.h"
#include "Talk.h"
#include "log/Log.h"
#include "settings/PreferenceKeys.h"
#include "settings/Settings.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

const char *const Target = "fuzz_tibia_cfg";

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	patella::fuzz::EnterWorkFolder("tibia-cfg");
	// InitPreferences reports the import at the info level: quiet.
	patella::log::Log().SetLevel(patella::log::TLevel::Error);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	using patella::fuzz::Held;
	// SetDefaultPreferences clears the ignore list, not the white list (Talk.cpp's BuddyExemptionList): in one process
	// it would grow from input to input (the client loads once). Cleared here, as a new process has it.
	ClearBuddyIgnore();
	patella::fuzz::RemoveFile(patella::settings::FileName);
	patella::fuzz::WriteFile("Tibia.cfg", Data, Size);
	patella::settings::TPreferenceValues Imported;
	bool Loaded = false;
	patella::fuzz::Guard(Target, [&] {
		InitPreferences();
		Imported = Held(GetPreferenceValues());
		ExitPreferences();
		Loaded = true;
	});
	if (!Loaded)
		return 0;
	const std::optional<patella::settings::TSettingsFile> File = patella::settings::Load(patella::settings::FileName);
	if (!File)
		patella::fuzz::Fail(Target, "the import wrote no Patella.ini");
	patella::settings::TPreferenceValues Read = Imported;
	const std::vector<std::string> Problems = patella::settings::ReadPreferences(*File, Read);
	if (!Problems.empty())
		patella::fuzz::Fail(Target, "the Patella.ini of the import read with a problem: " + Problems.front());
	if (Held(Read) != Imported)
		patella::fuzz::Fail(Target, "the Patella.ini of the import reads other options: " + patella::fuzz::Difference(Held(Read), Imported));
	// The next starts read Patella.ini (Tibia.cfg stays, and is not read again), each with the white list of a new process.
	const auto Start = [] {
		ClearBuddyIgnore();
		patella::settings::TPreferenceValues Values;
		patella::fuzz::Guard(Target, [&] {
			InitPreferences();
			Values = Held(GetPreferenceValues());
			ExitPreferences();
		});
		return Values;
	};
	patella::settings::TPreferenceValues Second = Start();
	const patella::settings::TPreferenceValues Third = Start();
	if (Third != Second)
		patella::fuzz::Fail(Target, "the third start differs from the second: " + patella::fuzz::Difference(Third, Second));
	// Not compared with the import (above): OpenDialogs, White, Ignore.
	Second.OpenDialogs = Imported.OpenDialogs;
	Second.White = Imported.White;
	Second.Ignore = Imported.Ignore;
	if (Second != Imported)
		patella::fuzz::Fail(Target, "the options of the next start differ from the imported ones: " + patella::fuzz::Difference(Second, Imported));
	return 0;
}
