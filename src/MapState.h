// MapState.h: TMapState, the game state as five objects (Patella-Modern issue #106, plan M7; notes/decisions.md,
// "#104: TClient and the subsystem objects" and "#106"). Not part of the original source.
//
// TClient owns it (Client.h), after TNetwork and before TCommunication, whose packet handlers write it. Its five
// objects are made in this order, each given the earlier ones it reads (references, never owners):
//	- TObjectTypes (Objects.cpp): Tibia.dat's tables, read once, then read-only.
//	- TMap (Map.cpp): the map points, the effects, the automap, the ambient light; reads the object types.
//	- TCreatures (Creatures.cpp): the creature table, the battle list, the targets, the VIP list; asks the player's
//	  state through TCreaturesPlayer, which TMapState implements (TPlayerState is made after it).
//	- TPlayerState (Creatures.cpp): the player's statistics, skills and walk; reads the object types, the map and
//	  the creature table.
//	- TInventory (Inventory.cpp): the body, the containers, the trade offers; reads the object types.

#pragma once

#include "Creatures.h"
#include "Inventory.h"
#include "Map.h"
#include "Objects.h"
#include "common.h"

namespace patella {

class TMapState final : private TCreaturesPlayer {
public:
	// Console and GUI: the calls up into TGUI (issue #118), which TClient passes (Client.cpp's adapters).
	TMapState(TMapConsole &Console, TCreaturesGUI &GUI)
		: Map(ObjectTypes, Console), Creatures(*this, GUI), PlayerState(ObjectTypes, Map, Creatures, GUI), Inventory(ObjectTypes)
	{
	}
	TMapState(const TMapState &) = delete;
	TMapState &operator=(const TMapState &) = delete;
	TMapState(TMapState &&) = delete;
	TMapState &operator=(TMapState &&) = delete;
	~TMapState() = default;

	// The objects, in the order they are made.
	TObjectTypes ObjectTypes;
	TMap Map;
	TCreatures Creatures;
	TPlayerState PlayerState;
	TInventory Inventory;

private:
	// TCreaturesPlayer: TCreatures' calls of the player's state. GetPlayerID calls TPlayerState's member since issue
	// #118 (the free function until then): the golden tests that fake the free one (the Communication world, the window
	// tests) fake the member too.
	uint32 GetPlayerID() override { return PlayerState.GetPlayerID(); }
	bool ContinuePlayerWalk() override { return PlayerState.ContinuePlayerWalk(); }
	void ResetPlayer() override { PlayerState.ResetPlayer(); }
};

} // namespace patella
