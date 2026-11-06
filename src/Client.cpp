// Client.cpp: the one TClient of the transition and the subsystems' bridge accessors (Patella-Modern issue #104;
// Client.h, notes/decisions.md, "#104: TClient and the subsystem objects").

#include "Client.h"

#include "Communication.h"
#include "Creatures.h"
#include "Inventory.h"
#include "Map.h"
#include "MapState.h"
#include "Network.h"
#include "Objects.h"
#include "Preferences.h"
#include "Status.h"
#include "Talk.h"
#include "common.h"

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

// Status.cpp's session and message queue for TNetwork (TNetworkSession, issue #105), until TCommunication and TGUI
// implement it. Stateless, as TTalkCppLists: each member calls Status.cpp's function of its name (from another object
// file than Network.cpp's, so the golden Network test's fakes of those functions are reached as before).
class TStatusCppSession final : public TNetworkSession {
public:
	void SetConnectionState(int State) override { ::SetConnectionState(State); }
	int GetConnectionState() override { return ::GetConnectionState(); }
	int GetActiveCharacter() override { return ::GetActiveCharacter(); }
	const char *GetCharacterIPAddress(int Number) override { return ::GetCharacterIPAddress(Number); }
	int GetCharacterPort(int Number) override { return ::GetCharacterPort(Number); }
	void AddMessage(int Type, INT_PTR Param1, INT_PTR Param2) override { ::AddMessage(Type, Param1, Param2); }
};

// Made on first use, as TClient is (a static initialiser of another file may make TClient first).
TNetworkSession &StatusCppSession()
{
	static TStatusCppSession Session;
	return Session;
}

} // namespace

TClient::TClient() : Network(Preferences, StatusCppSession()), Communication(Network) {}

TClient &bridge::Client()
{
	static TClient Instance;
	return Instance;
}

TPreferences &bridge::Preferences()
{
	return Client().Preferences;
}

TNetwork &bridge::Network()
{
	return Client().Network;
}

TCommunication &bridge::Communication()
{
	return Client().Communication;
}

TSession &bridge::Session()
{
	return Client().Communication.Session;
}

TObjectTypes &bridge::ObjectTypes()
{
	return Client().MapState.ObjectTypes;
}

TMap &bridge::Map()
{
	return Client().MapState.Map;
}

TCreatures &bridge::Creatures()
{
	return Client().MapState.Creatures;
}

TPlayerState &bridge::PlayerState()
{
	return Client().MapState.PlayerState;
}

TInventory &bridge::Inventory()
{
	return Client().MapState.Inventory;
}

TTalkLists &bridge::TalkLists()
{
	static TTalkCppLists Lists;
	return Lists;
}

} // namespace patella
