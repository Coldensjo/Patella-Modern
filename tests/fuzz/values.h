// values.h: the options as the client can hold them, for the round trips of fuzz_settings and fuzz_tibia_cfg
// (Patella-Modern issue #132).

#pragma once

#include "settings/PreferenceKeys.h"

#include <cstddef>
#include <string>
#include <vector>

namespace patella::fuzz {

// Values with what neither the client nor a file it writes can keep taken away: a text ends at its first NUL (the
// client's texts are C strings), and a hotkey without a text is not written (Tibia.cfg's writer skips it as well), so
// its "sent at once" flag is not kept.
inline patella::settings::TPreferenceValues Held(patella::settings::TPreferenceValues Values)
{
	const auto Cut = [](std::string &Text) {
		if (const std::size_t Nul = Text.find('\0'); Nul != std::string::npos)
			Text.resize(Nul);
	};
	for (std::size_t i = 0; i < Values.Hotkeys.size(); i++) {
		Cut(Values.Hotkeys[i]);
		if (Values.Hotkeys[i].empty())
			Values.HotkeyAutoSend[i] = false;
	}
	for (patella::settings::TBuddyValue &Buddy : Values.Buddies)
		Cut(Buddy.Description);
	for (std::string &Name : Values.White)
		Cut(Name);
	for (std::string &Name : Values.Ignore)
		Cut(Name);
	return Values;
}

// The names of the options that differ ("" when none), for the findings' reports.
inline std::string Difference(const patella::settings::TPreferenceValues &A, const patella::settings::TPreferenceValues &B)
{
	std::string Names;
	const auto Compare = [&Names](bool Same, const char *Name) {
		if (!Same)
			Names += Names.empty() ? Name : std::string(", ") + Name;
	};
#define PATELLA_FUZZ_COMPARE(Member) Compare(A.Member == B.Member, #Member)
	PATELLA_FUZZ_COMPARE(TestServer);
	PATELLA_FUZZ_COMPARE(FullscreenMode);
	PATELLA_FUZZ_COMPARE(WindowsMousepointer);
	PATELLA_FUZZ_COMPARE(LightEffects);
	PATELLA_FUZZ_COMPARE(TextualEffects);
	PATELLA_FUZZ_COMPARE(CreatureInfo);
	PATELLA_FUZZ_COMPARE(NoStretching);
	PATELLA_FUZZ_COMPARE(ConfineFramerate);
	PATELLA_FUZZ_COMPARE(Engine);
	PATELLA_FUZZ_COMPARE(Hotkeys);
	PATELLA_FUZZ_COMPARE(HotkeyAutoSend);
	PATELLA_FUZZ_COMPARE(WindowedMode);
	PATELLA_FUZZ_COMPARE(WindowedMaximized);
	PATELLA_FUZZ_COMPARE(ConsoleHeight);
	PATELLA_FUZZ_COMPARE(AutomapZoom);
	PATELLA_FUZZ_COMPARE(ContentWindows);
	PATELLA_FUZZ_COMPARE(BuddySortmethods);
	PATELLA_FUZZ_COMPARE(BuddyHideOffline);
	PATELLA_FUZZ_COMPARE(OpenDialogs);
	PATELLA_FUZZ_COMPARE(LastMotD);
	PATELLA_FUZZ_COMPARE(InfoMessages);
	PATELLA_FUZZ_COMPARE(EventMessages);
	PATELLA_FUZZ_COMPARE(StatusMessages);
	PATELLA_FUZZ_COMPARE(TimeStamps);
	PATELLA_FUZZ_COMPARE(PrivateMessages);
	PATELLA_FUZZ_COMPARE(OpenChannels);
	PATELLA_FUZZ_COMPARE(ClassicControl);
	PATELLA_FUZZ_COMPARE(AutoChaseOff);
	PATELLA_FUZZ_COMPARE(AttackMode);
	PATELLA_FUZZ_COMPARE(ChaseMode);
	PATELLA_FUZZ_COMPARE(SecureMode);
	PATELLA_FUZZ_COMPARE(Buddies);
	const auto List = [](const std::vector<std::string> &Entries) {
		std::string Text = "[";
		for (const std::string &Name : Entries)
			Text += "\"" + Name + "\" ";
		return Text + "]";
	};
	Compare(A.White == B.White, ("White " + List(A.White) + " and " + List(B.White)).c_str());
	Compare(A.Ignore == B.Ignore, ("Ignore " + List(A.Ignore) + " and " + List(B.Ignore)).c_str());
	PATELLA_FUZZ_COMPARE(KnownHints);
	PATELLA_FUZZ_COMPARE(ShowNewHints);
	PATELLA_FUZZ_COMPARE(LevelBar);
	PATELLA_FUZZ_COMPARE(MagLevelBar);
	PATELLA_FUZZ_COMPARE(SkillBars);
#undef PATELLA_FUZZ_COMPARE
	return Names;
}

} // namespace patella::fuzz
