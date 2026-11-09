// fuzz_settings: Patella.ini (Patella-Modern issue #132): the input is the file's text. patella_core alone
// (settings/Settings.cpp, settings/PreferenceKeys.cpp, log/LogSettings.cpp), so every preset builds it.
//
// Besides the sanitizers it checks what these files promise (all of it the fork's code, none the original's):
// - TSettingsFile: Parse(Text()) gives the same Text(); a Set that succeeds is what Get then reads, and Remove takes it
//   away again.
// - The options: what ReadPreferences reads from the file, as the client holds it (values.h Held: a text ends at its
//   first NUL, the client's being C strings; a hotkey without a text has no "sent at once"), WritePreferences writes
//   and ReadPreferences reads back the same, with no problem, into a new file and into the file itself
//   (SavePreferences' update of the file it loaded); a second write changes neither.
// - The developer log's keys (log::Configure, LogFile removed first: no file is opened).
// - Where the web links go (settings::TLinks, issue #169): what a button opens is the original's address or one the rule
//   accepts, and a caption fits TTextButton.

#include "fuzz.h"
#include "values.h"

#include "log/Log.h"
#include "Text.h" // IWYU pragma: keep (the addresses of the links)
#include "log/LogSettings.h"
#include "platform/FileSystem.h"
#include "settings/LinkSettings.h"
#include "settings/PreferenceKeys.h"
#include "settings/Settings.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using patella::settings::TPreferenceValues;
using patella::settings::TSettingsFile;

const char *const Target = "fuzz_settings";

// Where two texts differ: the line of each, quoted.
std::string FirstDifference(const std::string &A, const std::string &B)
{
	std::size_t At = 0;
	while (At < A.size() && At < B.size() && A[At] == B[At])
		At++;
	const std::size_t Start = At == 0 ? std::string::npos : A.rfind('\n', At - 1);
	const std::size_t From = Start == std::string::npos ? 0 : Start + 1;
	const auto Line = [From](const std::string &Text) {
		return From > Text.size() ? std::string() : Text.substr(From, Text.find('\n', From) - From);
	};
	return "\"" + Line(A) + "\" became \"" + Line(B) + "\"";
}

// The client writes what it holds (SetPreferenceValues, then GetPreferenceValues at the save): Held(Values).
void CheckRoundTrip(TSettingsFile File, const TPreferenceValues &Read, const char *How)
{
	const TPreferenceValues Values = patella::fuzz::Held(Read);
	patella::settings::WritePreferences(File, Values);
	TPreferenceValues Again;
	const std::vector<std::string> Problems = patella::settings::ReadPreferences(File, Again);
	if (!Problems.empty())
		patella::fuzz::Fail(Target, std::string(How) + ": the written options read with a problem: " + Problems.front());
	if (patella::fuzz::Held(Again) != Values)
		patella::fuzz::Fail(Target, std::string(How) + ": the written options read back differently: " + patella::fuzz::Difference(patella::fuzz::Held(Again), Values));
	// Written once more, the file stays as it is.
	TSettingsFile Twice = File;
	patella::settings::WritePreferences(Twice, patella::fuzz::Held(Again));
	if (Twice.Text() != File.Text())
		patella::fuzz::Fail(Target, std::string(How) + ": a second write changed the file: " + FirstDifference(File.Text(), Twice.Text()));
}

void NoConsole(std::string_view) noexcept {}

void Run(std::string_view Text)
{
	const TSettingsFile File = TSettingsFile::Parse(Text, patella::settings::NewFileNewline);
	const std::string Written = File.Text();
	if (TSettingsFile::Parse(Written, File.Newline()).Text() != Written)
		patella::fuzz::Fail(Target, "Parse(Text()) changed the text");
	static_cast<void>(File.BadLines());
	const std::vector<std::string> Keys = File.Keys();
	for (const std::string &Key : Keys) {
		const std::optional<std::string> Value = File.Get(Key);
		if (!Value)
			patella::fuzz::Fail(Target, "a key of Keys() without a value: " + Key);
		static_cast<void>(patella::settings::ParseFields(*Value, patella::settings::EncodingOf(File)));
		static_cast<void>(patella::settings::ClientText(File, *Value));
	}
	if (!Keys.empty()) {
		TSettingsFile Changed = File;
		const std::string &Key = Keys.front();
		if (Changed.Set(Key, "fuzz value")) {
			if (Changed.Get(Key) != "fuzz value")
				patella::fuzz::Fail(Target, "Get does not read what Set wrote: " + Key);
			if (Changed.Remove(Key) < 1 || Changed.Get(Key).has_value())
				patella::fuzz::Fail(Target, "Remove left the key: " + Key);
		}
	}

	TPreferenceValues Values;
	static_cast<void>(patella::settings::ReadPreferences(File, Values));
	CheckRoundTrip(TSettingsFile(patella::settings::NewFileNewline), Values, "a new file");
	CheckRoundTrip(File, Values, "the file itself");
	TSettingsFile Converted = File;
	patella::settings::ConvertToUtf8(Converted);

	TSettingsFile LogKeys = File;
	LogKeys.Remove(patella::log::FileKey);
	patella::log::TLog Log(NoConsole);
	patella::platform::TSystemFileSystem Files; // LogKeys has no LogFile, so nothing is opened (#119: Configure takes the file system)
	patella::log::Configure(Log, LogKeys, Files);

	patella::settings::TLinks Links;
	Links.Configure(File, Log);
	for (const char *const Address : {TextTibiaURL, TextManualURL, TextFAQURL, TextOptionsURL, TextGraphicsOptionsURL,
			 TextConsoleOptionsURL, TextNetworkOptionsURL, TextAccountURL, TextSignupURL}) {
		const std::string Opened = Links.Target(Address);
		if (Opened != Address && !patella::settings::ValidLinkAddress(Opened))
			patella::fuzz::Fail(Target, "a link opens an address the rule refuses: " + Opened);
		if (std::string_view(Links.HoverText(Address)).size() > patella::settings::MaxLinkHoverText)
			patella::fuzz::Fail(Target, "a hover text is too long");
	}
	if (Links.WebsiteLabel().size() > patella::settings::MaxLinkCaption)
		patella::fuzz::Fail(Target, "a caption TTextButton would throw on: " + Links.WebsiteLabel());
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	// The problems Configure reports go to the process's log: quiet.
	patella::log::Log().SetLevel(patella::log::TLevel::Error);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	try {
		Run(std::string_view(reinterpret_cast<const char *>(Data), Size));
	} catch (const std::exception &Exception) {
		patella::fuzz::Fail(Target, std::string("an exception: ") + Exception.what());
	}
	return 0;
}
