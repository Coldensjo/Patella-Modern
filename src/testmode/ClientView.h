// ClientView.h: where the test mode's state dump finds the client's data (issue #22; StateDump.h).
// Test build only (PATELLA_TEST_MODE; included by src/TestHooks.h).
//
// The data client_state.py reads are mostly file-local (static) in the client's sources. Each of
// those files ends with a View function, compiled only into the test build, that hands out const
// pointers to its data; src/testmode/ClientStateWin32.cpp reads through them and never writes.

#pragma once

#include <cstdint>

class TCreature;
struct TCharacter;
struct TContainer;
struct TMapPoint;
struct TObject;
struct TOffer;
template <class T> class array;

namespace patella::testmode {

// Creatures.cpp
struct TCreaturesView {
	const int *PlayerState = nullptr;
	const int *PlayerSkillProgress = nullptr; // [SKILLS]
	const int *PlayerSkillValue = nullptr;    // [SKILLS]
	const std::uint32_t *FollowID = nullptr;
	const std::uint32_t *AttackID = nullptr;
	const int *PlayerCapacity = nullptr;
	const int *PlayerSoul = nullptr;
	const int *PlayerMaxMana = nullptr;
	const int *PlayerMana = nullptr;
	const int *PlayerMagicLevelProgress = nullptr;
	const int *PlayerLevelProgress = nullptr;
	const int *PlayerMagicLevel = nullptr;
	const int *PlayerLevel = nullptr;
	const int *PlayerExperience = nullptr;
	const int *PlayerMaxHealth = nullptr;
	const int *PlayerHealth = nullptr;
	const std::uint32_t *PlayerID = nullptr;
	const TCreature *Creatures = nullptr; // [CREATURES]
};

// Inventory.cpp
struct TInventoryView {
	const TObject *Body = nullptr;          // [BODY_POSITIONS]
	const TContainer *Containers = nullptr; // [OPEN_CONTAINERS]
	const TOffer *OwnOffer = nullptr;
	const TOffer *CounterOffer = nullptr;
};

// Map.cpp
struct TMapView {
	const int *MapX = nullptr;
	const int *MapY = nullptr;
	const int *MapZ = nullptr;
	const array<TMapPoint> *Fields = nullptr;
};

void ViewCreatures(TCreaturesView *View);
void ViewInventory(TInventoryView *View);
void ViewMap(TMapView *View);
// Status.cpp
const int *ViewConnectionState();
// Status.cpp's character list (issue #92)
struct TCharactersView {
	const TCharacter *Characters = nullptr; // [*Count], or none
	const int *Count = nullptr;
};
void ViewCharacters(TCharactersView *View);

} // namespace patella::testmode
