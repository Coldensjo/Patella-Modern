// Client.cpp: the one TClient of the transition and the subsystems' bridge accessors (Patella-Modern issue #104;
// Client.h, notes/decisions.md, "#104: TClient and the subsystem objects").

#include "Client.h"

#include "Preferences.h"
#include "Talk.h"

namespace patella {

namespace {

// Talk.cpp's lists for TPreferences (TTalkLists), until Talk's state is an object of its own (TGUI, issue #107),
// which then implements TTalkLists itself. Stateless: each member calls Talk.cpp's function of its name (in another
// object file than Talk.cpp's, so a golden test's fake of that function is reached as before).
class TTalkCppLists final : public TTalkLists {
public:
	void ClearIgnoreList() override { ::ClearIgnoreList(); }
	char *GetBuddyIgnoreText() override { return ::GetBuddyIgnoreText(); }
	void AddBuddyIgnore(const char *Addressee, bool Permanent) override { ::AddBuddyIgnore(Addressee, Permanent); }
	bool IsPermanentBuddyIgnore(const char *Addressee) override { return ::IsPermanentBuddyIgnore(Addressee); }
	char *GetIgnoreListText() override { return ::GetIgnoreListText(); }
	void IgnoreName(const char *Addressee, bool Permanent) override { ::IgnoreName(Addressee, Permanent); }
	bool IsPermanentIgnore(const char *Addressee) override { return ::IsPermanentIgnore(Addressee); }
};

} // namespace

TClient &bridge::Client()
{
	static TClient Instance;
	return Instance;
}

TPreferences &bridge::Preferences()
{
	return Client().Preferences;
}

TTalkLists &bridge::TalkLists()
{
	static TTalkCppLists Lists;
	return Lists;
}

} // namespace patella
