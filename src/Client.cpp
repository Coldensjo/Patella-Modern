// Client.cpp: the one TClient of the transition and the subsystems' bridge accessors (Patella-Modern issue #104;
// Client.h, notes/decisions.md, "#104: TClient and the subsystem objects").

#include "Client.h"

#include "Communication.h"
#include "Control.h"
#include "GUI.h"
#include "Help.h"
#include "Network.h"
#include "Preferences.h"
#include "Status.h"
#include "Talk.h"
#include "common.h"

namespace patella {

namespace {

// Status.cpp's session and message queue for TNetwork (TNetworkSession, issue #105). TSession and TGUI's
// TInteraction are made after TNetwork, so it stays an adapter (issue #107: a reference to them would point up).
// Stateless: each member calls Status.cpp's function of its name (from another object file than Network.cpp's, so the
// golden Network test's fakes of those functions are reached as before).
class TStatusCppSession final : public TNetworkSession {
public:
	void SetConnectionState(int State) override { ::SetConnectionState(State); }
	int GetConnectionState() override { return ::GetConnectionState(); }
	int GetActiveCharacter() override { return ::GetActiveCharacter(); }
	const char *GetCharacterIPAddress(int Number) override { return ::GetCharacterIPAddress(Number); }
	int GetCharacterPort(int Number) override { return ::GetCharacterPort(Number); }
	void AddMessage(int Type, INT_PTR Param1, INT_PTR Param2) override { ::AddMessage(Type, Param1, Param2); }
};

// Control.cpp's functions for TGUI's windows (TGUIController, issue #107), until Control.cpp's functions are an
// object's. Stateless, as TStatusCppSession: each member calls Control.cpp's function of its name (from another object
// file than the windows', so the golden tests' fakes of Resize and ChangeEngine are reached as before).
class TControlCppController final : public TGUIController {
public:
	void ChangeEngine(int engine) override { ::ChangeEngine(engine); }
	void Resize(int Width, int Height) override { ::Resize(Width, Height); }
};

TGUIController &ControlCppController()
{
	static TControlCppController Controller;
	return Controller;
}

// Made on first use, as TClient is (a static initialiser of another file may make TClient first).
TNetworkSession &StatusCppSession()
{
	static TStatusCppSession Session;
	return Session;
}

} // namespace

TClient::TClient() : Network(Preferences, StatusCppSession()), Communication(Network), GUI(ControlCppController()) {}

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

// TPreferences' TTalkLists is TGUI's TTalk since issue #107 (Client.cpp's adapter over Talk.cpp's functions until
// then).
TTalkLists &bridge::TalkLists()
{
	return Client().GUI.Talk;
}

TGUI &bridge::GUI()
{
	return Client().GUI;
}

TTalk &bridge::Talk()
{
	return Client().GUI.Talk;
}

THelp &bridge::Help()
{
	return Client().GUI.Help;
}

TInteraction &bridge::Interaction()
{
	return Client().GUI.Interaction;
}

} // namespace patella
