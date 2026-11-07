// Client.cpp: the one TClient of the transition and the subsystems' bridge accessors (Patella-Modern issue #104;
// Client.h, notes/decisions.md, "#104: TClient and the subsystem objects").

#include "Client.h"

#include "Communication.h"
#include "Creatures.h"
#include "GUI.h"
#include "Help.h"
#include "Inventory.h"
#include "Map.h"
#include "MapState.h"
#include "Network.h"
#include "Objects.h"
#include "Preferences.h"
#include "Status.h"
#include "Skins.h"
#include "Surface.h"
#include "Talk.h"
#include "graphics.h"
#include "common.h"
#include "platform/Platform.h"
#include <cstdint>

namespace patella {

namespace {

// Status.cpp's session and message queue for TNetwork (TNetworkSession, issue #105). TSession (TCommunication's) and
// TGUI's TInteraction are made after TNetwork, so it stays an adapter (issue #107: a reference to them would point up).
// Issue #118 kept it rather than an interface TClient wires into TNetwork after construction: that would be a pointer
// TNetwork holds unset between its construction and TClient's (a two-phase start the golden runner, the unit tests
// and the test mode would each have to reach), and the golden Network test fakes Status.cpp's free functions, which
// this adapter calls. It goes with the bridge (#111) or with #109's ISocket events (notes/decisions.md, "#118").
// Stateless: each member calls Status.cpp's function of its name (from another object file than Network.cpp's, so the
// golden Network test's fakes of those functions are reached as before).
class TStatusCppSession final : public TNetworkSession {
public:
	void SetConnectionState(int State) override { ::SetConnectionState(State); }
	int GetConnectionState() override { return ::GetConnectionState(); }
	int GetActiveCharacter() override { return ::GetActiveCharacter(); }
	const char *GetCharacterIPAddress(int Number) override { return ::GetCharacterIPAddress(Number); }
	int GetCharacterPort(int Number) override { return ::GetCharacterPort(Number); }
	void AddMessage(int Type, std::intptr_t Param1, std::intptr_t Param2) override { ::AddMessage(Type, Param1, Param2); }
};

// Made on first use, as TClient is (a static initialiser of another file may make TClient first).
TNetworkSession &StatusCppSession()
{
	static TStatusCppSession Session;
	return Session;
}

// Talk.cpp's default console for TMap (TMapConsole, issue #118) and Talk.cpp's, Help.cpp's and Status.cpp's functions
// for TCreatures and TPlayerState (TCreaturesGUI): TGUI's, made after TMapState, so adapters as TStatusCppSession is.
// Stateless: each member calls the free functions of the original's code (from another object file than Map.cpp's and
// Creatures.cpp's, so the golden tests' fakes of those functions are reached as before).
class TTalkCppConsole final : public TMapConsole {
public:
	bool HasDefaultConsole() override { return bridge::Talk().DefaultTextBuffer != nullptr; }
	void AddDefaultConsoleLine(const char *Message) override
	{
		::AddTextBufferLine(bridge::Talk().DefaultTextBuffer, 0, Message, 20, nullptr, 0);
		::SetConsoleChanged();
	}
};

class TGUICppCreatures final : public TCreaturesGUI {
public:
	void SetStatusMessage(const char *Message, bool Permanent) override { ::SetStatusMessage(Message, Permanent); }
	void AddEventHelp(int Number) override { ::AddEventHelp(Number); }
	void CheckEventHelpName(const char *Name) override { ::CheckEventHelpName(Name); }
	void AddMessage(int Type, std::intptr_t Param1, std::intptr_t Param2) override { ::AddMessage(Type, Param1, Param2); }
	int GetActionNumber() override { return ::GetActionNumber(); }
};

// TGraphics' signatures for TCommunication's login packet (TLoginSignatures, issue #110): TGraphics is made after
// TCommunication, so an adapter as the ones above. Stateless: it reads the one TClient's TGraphics.
class TGraphicsSignatures final : public TLoginSignatures {
public:
	uint32 SpriteSignature() override { return bridge::Graphics().SpriteSignature; }
	uint32 PictureSignature() override { return bridge::Graphics().PictureSignature; }
};

TLoginSignatures &GraphicsSignatures()
{
	static TGraphicsSignatures Signatures;
	return Signatures;
}

TMapConsole &TalkCppConsole()
{
	static TTalkCppConsole Console;
	return Console;
}

TCreaturesGUI &GUICppCreatures()
{
	static TGUICppCreatures Adapter;
	return Adapter;
}

} // namespace

TClient::TClient(platform::TPlatform &Platform)
	: Preferences(Platform.FileSystem), Network(Preferences, StatusCppSession(), Platform.Socket),
	  MapState(TalkCppConsole(), GUICppCreatures()), Communication(Network, MapState, GraphicsSignatures()), GUI(Network, Communication)
{
}

TClient &bridge::Client()
{
	static TClient Instance(platform::Platform());
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

TGraphics &bridge::Graphics()
{
	return Client().Graphics;
}

TGraphicsEngine &bridge::GraphicsEngine()
{
	return Client().Graphics.Engine;
}

TSkins &bridge::Skins()
{
	return Client().Graphics.Skins;
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
