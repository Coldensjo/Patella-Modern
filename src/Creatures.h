// Creatures.h: the creatures known to the client, the player's own data
// (statistics, skills, attack/follow targets, the steps of the current walk)
// and the VIP list ("buddies").
//
// Names from the binary: GetCreature, GetPlayerID and GetPlayerPartyFlag
// (assertion texts in ContextMenus.cpp and ConsoleWindow.cpp), CreatureID
// (catch blocks of the TCreature members), NumberOfPlayerSteps,
// CurrentPlayerStep, NumberOfBuddies, BUDDIES, SKILLS, Opponent, and the
// parameter names in the assertions and catch blocks (Step, NewCreatureID,
// AvailablePhases, Flag, Progress, SkillNumber, Value, Number, Position,
// CharacterID, Name, Description, Method, NewDirection, i, j). The rest is
// ours (notes/decisions.md, "Creatures.cpp").

#pragma once

#include "common.h"
#include "types/FixedString.h"
#include "types/Time32.h"
#include "graphics.h"    // TRGBColor
#include "Preferences.h" // SKILLS

#include <array>
#include <cstdint>

inline constexpr int CREATURES = 150; // slots in the creature table (loops up to 0x96)
inline constexpr int BUDDIES = 200;   // "NumberOfBuddies<BUDDIES"

// TCreature::NextOpponent of a creature that is not in the battle list.
inline constexpr int NOT_AN_OPPONENT = 0x7fffffff;

// One entry of the creature table (0x9c bytes).
class TCreature {
public:
	TRGBColor GetHealthColor() const;                  // 0x44b740
	bool IsMarked() const;                             // 0x44b800
	void SetSkull(int Skull, bool Delay);              // 0x44b820
	void SetPosition(int x, int y, int z);             // 0x452320, line 108
	int GetDeltaX() const;                             // 0x44bbf0
	int GetDeltaY() const;                             // 0x44bc10
	int GetDrawDirection() const;                      // 0x44bc30
	void StartMovement(int dx, int dy, int Waypoints); // 0x44bc70, line 142
	void SetMark(int Mark);                            // 0x44bf20, line 154
	int GetAnimationPhase(int AvailablePhases) const;  // 0x44c0d0, lines 163-169
	// Ours (issue #62): the skull SetSkull kept (Delay) is still shown at Now, the wall clock's 64-bit time:
	// SkullTime is set, and Now is from it to four seconds after it. SkullTime holds the original's 4 bytes of the
	// time (StoreTime32, types/Time32.h); the comparison is in 64 bits (LoadTime32), the original's 32-bit one up
	// to 2038.
	[[nodiscard]] bool SkullKept(std::int64_t Now) const
	{
		if (SkullTime == 0)
			return false;
		const std::int64_t Removed = LoadTime32(SkullTime, Now);
		return Now >= Removed && Now <= Removed + 4;
	}

	uint32 CreatureID;         // +0x00, 0 marks a free slot; players have IDs below 0x40000000
	TFixedString<32> Name;     // +0x04
	int RealX;                 // +0x24, map position
	int RealY;                 // +0x28
	int RealZ;                 // +0x2c
	int DeltaX;                // +0x30, pixel offset of the walk animation, counts down to 0
	int DeltaY;                // +0x34
	int StepEnd;               // +0x38, GetMilliseconds() time the step animation ends
	int NextStep;              // +0x3c, time the next step may start (three steps' time after a diagonal one)
	int StepX;                 // +0x40, abs(dx)*32
	int StepY;                 // +0x44, abs(dy)*32
	int StepDuration;          // +0x48, milliseconds, a multiple of Beat
	bool Moving;               // +0x4c
	int Direction;             // +0x50, 0 north, 1 east, 2 south, 3 west
	int MoveDirection;         // +0x54, Direction when the current step started
	int PrevOpponent;          // +0x58, battle list links (slot numbers, -1 at the ends)
	int NextOpponent;          // +0x5c, NOT_AN_OPPONENT when not in the list
	int OutfitType;            // +0x60
	std::array<int, 4> Colors; // +0x64, head, body, legs, feet; Colors[0] is the object type when OutfitType is 0
	int LightStrength;         // +0x74
	int LightColor;            // +0x78
	int Mark;                  // +0x7c, colour of the square around the creature
	int MarkTime;              // +0x80, shown up to this GetMilliseconds() time
	int Health;                // +0x84, percent
	int Speed;                 // +0x88
	bool Visible;              // +0x8c
	int PKFlag;                // +0x90, the skull; name from "Creature->PKFlag" (MapWindow.cpp)
	int PARTYFlag;             // +0x94, the party shield; "Creature->PARTYFlag" (MapWindow.cpp)
	Time32 SkullTime;          // +0x98, time a skull was removed (StoreTime32, 0: none); it stays four seconds longer
};

// A VIP list entry (0x2c bytes). Preferences.h has the stored data
// (icon, description, notification) as TBuddy.
struct TBuddyEntry {
	uint32 CharacterID;    // +0x00
	TFixedString<30> Name; // +0x04, "strlen(Name)<30"
	bool Online;           // +0x22
	int LoginAction;       // +0x24, GetActionNumber() at the last change of Online
	int Icon;              // +0x28
};

namespace patella {

class TMap;
class TObjectTypes;

namespace testmode {
struct TCreaturesView;
} // namespace testmode

// What TCreatures asks of the player's state (Patella-Modern issue #106; rule 5 of notes/decisions.md, "#104"):
// TPlayerState is made after TCreatures (it reads the creature table), so TCreatures reaches it only through this
// interface, which TMapState implements (MapState.h). GetPlayerID is the original's (IsOpponent, CheckMonsterHint and
// MoveCreatures call it); ContinuePlayerWalk is MoveCreatures' step of the walk once the player's step animation is
// over, ResetPlayer ResetCreatures' writes of the player's state (both ours: the original's code of those two
// functions, moved into TPlayerState).
class TCreaturesPlayer {
public:
	TCreaturesPlayer() = default;
	TCreaturesPlayer(const TCreaturesPlayer &) = delete;
	TCreaturesPlayer &operator=(const TCreaturesPlayer &) = delete;
	TCreaturesPlayer(TCreaturesPlayer &&) = delete;
	TCreaturesPlayer &operator=(TCreaturesPlayer &&) = delete;
	virtual uint32 GetPlayerID() = 0;
	virtual bool ContinuePlayerWalk() = 0;
	virtual void ResetPlayer() = 0;

protected:
	~TCreaturesPlayer() = default;
};

// Creatures.cpp's creature table, battle list and VIP list (Patella-Modern issue #106, M7; notes/decisions.md, "#104:
// TClient and the subsystem objects" and "#106"): Creatures, FirstOpponent, LastOpponent, Beat, the attack, follow and
// highlight targets and the buddy window's list, in the original's .bss order (0x5c4560-0x5c67d7, 0x5c6814-0x5c681f,
// 0x5c68a0-0x5cc417), every member 0 at the start; the functions that use them are the members of the same names.
// NumberOfBuddies is public (the golden Errors test sets it, TestErrorStatics), the others private as the file-local
// statics were; TCreature's StartMovement reads Beat (a friend). TMapState owns it (MapState.h), after TMap.
class TCreatures {
public:
	explicit TCreatures(TCreaturesPlayer &PlayerState);
	TCreatures(const TCreatures &) = delete;
	TCreatures &operator=(const TCreatures &) = delete;
	TCreatures(TCreatures &&) = delete;
	TCreatures &operator=(TCreatures &&) = delete;
	~TCreatures() = default;

	// The beat, the battle and follow targets and the buddy list's lookups.
	void HideAllCreatures();                        // 0x44b8e0
	void SetBeat(int Value);                        // 0x44b900
	static bool IsPlayer(uint32 CreatureID);        // 0x44b910
	void SetAttackID(uint32 CreatureID);            // 0x44bb00, clears the follow target
	void SetFollowID(uint32 CreatureID);            // 0x44bb20, clears the attack target
	void SetHighlightID(uint32 CreatureID);         // 0x44bb40, battle list entry under the mouse
	uint32 GetAttackID() const;                     // 0x44bb50
	uint32 GetFollowID() const;                     // 0x44bb60
	uint32 GetHighlightID() const;                  // 0x44bb70
	int GetBuddyPosition(uint32 CharacterID) const; // 0x44bb80, position or -1
	bool IsBuddy(const char *Name) const;           // 0x44bbb0

	// The creature table.
	TCreature *AddCreature(uint32 RemoveID, uint32 NewCreatureID); // 0x44c530, line 450
	TCreature *GetCreature(uint32 CreatureID);                     // 0x44c6e0, line 476
	void UpdateSkulls();                                           // 0x44c860
	void HideCreature(int CreatureID);                             // 0x44c8c0, line 517; a callback for Map.cpp
	bool MoveCreatures();                                          // 0x451ab0, line 549
	void CheckMonsterHint(TCreature *Creature);                    // 0x451db0, lines 573-592
	static void NotifySkull(int Flag);                             // 0x44ca60, line 604

	// The battle list.
	bool IsOpponent(TCreature *Creature); // 0x44d740, lines 987-994
	int GetNumberOfOpponents();           // 0x44d9f0, line 1029; also updates the list
	TCreature *GetOpponent(int Number);   // 0x44dc70, line 1037

	// The VIP list.
	bool CompareBuddies(int i, int j) const;                                         // 0x44de20, lines 1069-1071
	void SwapBuddies(int i, int j);                                                  // 0x44e260, lines 1097-1099
	void SortBuddies();                                                              // 0x44e600, line 1116
	void SortBuddy(int i);                                                           // 0x44e7d0, lines 1124-1135
	void AddBuddy(uint32 CharacterID, const char *Name, bool Online);                // 0x44eab0, lines 1144-1170
	void SetBuddyState(uint32 CharacterID, bool Online);                             // 0x44f000, line 1192
	void EditBuddy(int CharacterID, int Icon, const char *Description, bool Notify); // 0x44f230, lines 1200-1210
	void RemoveBuddy(uint32 CharacterID);                                            // 0x44f5e0, line 1227
	void SetBuddySortMethod(int Method);                                             // 0x44f810, line 1247
	void HideOfflineBuddies(bool Hide);                                              // 0x44fa00, line 1259
	int GetNumberOfBuddies() const;                                                  // 0x44fba0, line 1280; only the online ones when they are hidden
	uint32 GetBuddyListID(int Position) const;                                       // 0x44fd40, line 1288
	const char *GetBuddyListName(int Position) const;                                // 0x44feb0, line 1297
	bool GetBuddyListOnline(int Position) const;                                     // 0x450020, line 1306
	bool GetBuddyListNew(int Position) const;                                        // 0x450190, line 1316
	int GetBuddyListIcon(int Position) const;                                        // 0x450330, line 1325
	const char *GetBuddyListDescription(int Position) const;                         // 0x4504a0, lines 1334-1338
	bool GetBuddyListNotify(int Position) const;                                     // 0x450740, lines 1346-1350

	void InitCreatures();        // 0x452190, line 1366
	static void ExitCreatures(); // empty, folded into 0x496160
	void ResetCreatures();       // 0x4509e0, line 1414

	// The test mode's view (ClientView.h's ViewCreatures, with TPlayerState's; test build only).
	void View(testmode::TCreaturesView *Target) const;

private:
	friend class ::TCreature;

	TCreaturesPlayer &PlayerState;

	// Data, in .bss order. 0x5c456c and 0x5c68ac are not used by any code.
	std::array<int, 3> BuddySortMethods{};      // 0x5c4560, 0 name, 1 icon, 2 online; most significant first
	std::array<TBuddyEntry, BUDDIES> Buddies{}; // 0x5c4570
	int NumberOfOnlineBuddies{};                // 0x5c67d0

public:
	int NumberOfBuddies{}; // 0x5c67d4; public: the golden Errors test

private:
	uint32 HighlightID{};                         // 0x5c6814
	uint32 FollowID{};                            // 0x5c6818
	uint32 AttackID{};                            // 0x5c681c
	int Beat{};                                   // 0x5c68a0, step durations are rounded up to a multiple
	int LastOpponent{};                           // 0x5c68a4, battle list ends (slot numbers, -1: empty)
	int FirstOpponent{};                          // 0x5c68a8
	std::array<TCreature, CREATURES> Creatures{}; // 0x5c68b0

	// The arrays std::arrays (issue #60).
	static_assert(sizeof(BuddySortMethods) == 3 * sizeof(int) && sizeof(Buddies) == BUDDIES * sizeof(TBuddyEntry)
			&& sizeof(Creatures) == CREATURES * sizeof(TCreature),
		"TCreatures' arrays");
};

// Creatures.cpp's player state (Patella-Modern issue #106; notes/decisions.md, "#104" and "#106"): PlayerID, the
// statistics and skills, the state icons and the walk (the steps, the destination, the step not confirmed yet), in the
// original's .bss order (0x5c67d8-0x5c6813, 0x5c6820-0x5c689f), every member 0 at the start; the functions that use
// them are the members of the same names. NumberOfPlayerSteps is public (the golden Errors test sets it), the others
// private; TCreature's members read and write the step not confirmed yet (a friend). TMapState owns it, after
// TCreatures, whose creature table it reads; it reads the map and the object types too.
class TPlayerState {
public:
	TPlayerState(TObjectTypes &ObjectTypes, TMap &Map, TCreatures &Creatures);
	TPlayerState(const TPlayerState &) = delete;
	TPlayerState &operator=(const TPlayerState &) = delete;
	TPlayerState(TPlayerState &&) = delete;
	TPlayerState &operator=(TPlayerState &&) = delete;
	~TPlayerState() = default;

	// The player's creature, steps and statistics.
	int GetNumberOfPlayerSteps() const;                                       // 0x44b870
	void GetPlayerPosition(int *x, int *y, int *z) const;                     // 0x44b880, including the step not confirmed yet
	void ClearPlayerSteps();                                                  // 0x44b8c0
	void SetPlayerID(uint32 CreatureID);                                      // 0x44b920
	void SetPlayerHealth(int Health, int MaxHealth);                          // 0x44b930
	void SetPlayerMagicLevel(int Level, int Progress);                        // 0x44b950
	void SetPlayerMana(int Mana, int MaxMana);                                // 0x44b970
	void SetPlayerSoul(int Soul);                                             // 0x44b990
	void SetPlayerCapacity(int Capacity);                                     // 0x44b9a0
	void SetPlayerState(int State);                                           // 0x44b9b0
	uint32 GetPlayerID() const;                                               // 0x44b9c0
	int GetPlayerHealth() const;                                              // 0x44b9d0
	int GetPlayerMaxHealth() const;                                           // 0x44b9e0
	int GetPlayerExperience() const;                                          // 0x44b9f0
	int GetPlayerLevel() const;                                               // 0x44ba00
	int GetPlayerLevelProgress() const;                                       // 0x44ba10
	int GetPlayerMana() const;                                                // 0x44ba20
	int GetPlayerMaxMana() const;                                             // 0x44ba30
	int GetPlayerSoul() const;                                                // 0x44ba40
	int GetPlayerMagicLevel() const;                                          // 0x44ba50
	int GetPlayerMagicLevelProgress() const;                                  // 0x44ba60
	int GetPlayerCapacity() const;                                            // 0x44ba70
	bool IsPlayerPoisoned() const;                                            // 0x44ba80, state bit 0
	bool IsPlayerBurning() const;                                             // 0x44ba90, bit 1
	bool IsPlayerElectrified() const;                                         // 0x44baa0, bit 2
	bool IsPlayerDrunk() const;                                               // 0x44bab0, bit 3
	bool IsPlayerManaShielded() const;                                        // 0x44bac0, bit 4
	bool IsPlayerParalyzed() const;                                           // 0x44bad0, bit 5
	bool IsPlayerHasted() const;                                              // 0x44bae0, bit 6
	bool IsPlayerInFight() const;                                             // 0x44baf0, bit 7
	int GetPlayerStep(int Step) const;                                        // 0x44c3c0, line 259
	void DoPlayerStep();                                                      // 0x450c80, lines 278-312
	int FindPlayerPath();                                                     // 0x4510f0, line 347
	int SetPlayerDestination(int x, int y, int z, bool Exact, bool Diagonal); // 0x451400, line 383
	void StopPlayer();                                                        // 0x4516e0, line 413
	bool CancelPlayerWalk(int NewDirection);                                  // 0x4518b0, line 435
	void SetPlayerExperience(int Experience, int Level, int Progress);        // 0x44cc00, lines 634-644
	void SetPlayerSkill(int SkillNumber, int Value, int Progress);            // 0x44cf10, lines 687-693
	int GetPlayerSkillValue(int SkillNumber) const;                           // 0x44d1c0, line 804
	int GetPlayerSkillProgress(int SkillNumber) const;                        // 0x44d320, line 813
	static const char *GetSkillName(int SkillNumber);                         // 0x44d480, line 822
	bool HasPlayerSkull() const;                                              // 0x44d6d0
	bool IsPlayerInParty() const;                                             // 0x44d6f0
	int GetPlayerPartyFlag() const;                                           // 0x44d720

	// Ours (TCreaturesPlayer): MoveCreatures' continuation of the walk (true if a new walk is to be sent) and
	// ResetCreatures' writes of the player's state, the original's code of those two functions.
	bool ContinuePlayerWalk();
	void ResetPlayer();

	// The test mode's view (ClientView.h's ViewCreatures, with TCreatures'; test build only).
	void View(testmode::TCreaturesView *Target) const;

private:
	friend class ::TCreature;

	TObjectTypes &ObjectTypes;
	TMap &Map;
	TCreatures &Creatures;

	// Data, in .bss order.
	int PlayerState{};                             // 0x5c67d8, the state icons, one bit each
	std::array<int, SKILLS> PlayerSkillProgress{}; // 0x5c67dc
	std::array<int, SKILLS> PlayerSkillValue{};    // 0x5c67f8
	int PlayerCapacity{};                          // 0x5c6820
	int PlayerSoul{};                              // 0x5c6824
	int PlayerMaxMana{};                           // 0x5c6828
	int PlayerMana{};                              // 0x5c682c
	int PlayerMagicLevelProgress{};                // 0x5c6830
	int PlayerLevelProgress{};                     // 0x5c6834
	int PlayerMagicLevel{};                        // 0x5c6838
	int PlayerLevel{};                             // 0x5c683c
	int PlayerExperience{};                        // 0x5c6840
	int PlayerMaxHealth{};                         // 0x5c6844
	int PlayerHealth{};                            // 0x5c6848
	uint32 PlayerID{};                             // 0x5c684c
	bool NewPlayerDestination{};                   // 0x5c6850, a destination came while the old path was walked
	int CurrentPlayerStep{};                       // 0x5c6854

public:
	int NumberOfPlayerSteps{}; // 0x5c6858; public: the golden Errors test

private:
	std::array<int, 10> PlayerSteps{}; // 0x5c685c, 1 east, 2 north-east, ..., 8 south-east; the path finder stops at 10
	bool DestinationDiagonal{};        // 0x5c6884, passed on to the path finder
	bool DestinationExact{};           // 0x5c6885, passed on to the path finder
	int DestinationZ{};                // 0x5c6888
	int DestinationY{};                // 0x5c688c
	int DestinationX{};                // 0x5c6890, 0: no destination
	int PlayerStepDirection{};         // 0x5c6894, Direction of the step not confirmed yet
	int PlayerStepDY{};                // 0x5c6898, the step sent but not confirmed by the server
	int PlayerStepDX{};                // 0x5c689c

	// The arrays std::arrays (issue #60).
	static_assert(sizeof(PlayerSkillProgress) == SKILLS * sizeof(int) && sizeof(PlayerSkillValue) == SKILLS * sizeof(int)
			&& sizeof(PlayerSteps) == 10 * sizeof(int),
		"TPlayerState's arrays");
};

// The transition's access path (issue #106; notes/decisions.md, "#104: TClient and the subsystem objects"): the
// creatures and the player's state of the one TClient (Client.cpp). They go when no free function needs them.
namespace bridge {
TCreatures &Creatures();
TPlayerState &PlayerState();
} // namespace bridge

} // namespace patella

// The original's functions (the callers' names, and the golden tests'): each calls the member of the same name of
// TCreatures on bridge::Creatures() or of TPlayerState on bridge::PlayerState().
// The player's creature, steps and statistics.
int GetNumberOfPlayerSteps();                                             // 0x44b870
void GetPlayerPosition(int *x, int *y, int *z);                           // 0x44b880, including the step not confirmed yet
void ClearPlayerSteps();                                                  // 0x44b8c0
void HideAllCreatures();                                                  // 0x44b8e0
void SetBeat(int Beat);                                                   // 0x44b900
bool IsPlayer(uint32 CreatureID);                                         // 0x44b910
void SetPlayerID(uint32 CreatureID);                                      // 0x44b920
void SetPlayerHealth(int Health, int MaxHealth);                          // 0x44b930
void SetPlayerMagicLevel(int Level, int Progress);                        // 0x44b950
void SetPlayerMana(int Mana, int MaxMana);                                // 0x44b970
void SetPlayerSoul(int Soul);                                             // 0x44b990
void SetPlayerCapacity(int Capacity);                                     // 0x44b9a0
void SetPlayerState(int State);                                           // 0x44b9b0
uint32 GetPlayerID();                                                     // 0x44b9c0
int GetPlayerHealth();                                                    // 0x44b9d0
int GetPlayerMaxHealth();                                                 // 0x44b9e0
int GetPlayerExperience();                                                // 0x44b9f0
int GetPlayerLevel();                                                     // 0x44ba00
int GetPlayerLevelProgress();                                             // 0x44ba10
int GetPlayerMana();                                                      // 0x44ba20
int GetPlayerMaxMana();                                                   // 0x44ba30
int GetPlayerSoul();                                                      // 0x44ba40
int GetPlayerMagicLevel();                                                // 0x44ba50
int GetPlayerMagicLevelProgress();                                        // 0x44ba60
int GetPlayerCapacity();                                                  // 0x44ba70
bool IsPlayerPoisoned();                                                  // 0x44ba80, state bit 0
bool IsPlayerBurning();                                                   // 0x44ba90, bit 1
bool IsPlayerElectrified();                                               // 0x44baa0, bit 2
bool IsPlayerDrunk();                                                     // 0x44bab0, bit 3
bool IsPlayerManaShielded();                                              // 0x44bac0, bit 4
bool IsPlayerParalyzed();                                                 // 0x44bad0, bit 5
bool IsPlayerHasted();                                                    // 0x44bae0, bit 6
bool IsPlayerInFight();                                                   // 0x44baf0, bit 7
void SetAttackID(uint32 CreatureID);                                      // 0x44bb00, clears the follow target
void SetFollowID(uint32 CreatureID);                                      // 0x44bb20, clears the attack target
void SetHighlightID(uint32 CreatureID);                                   // 0x44bb40, battle list entry under the mouse
uint32 GetAttackID();                                                     // 0x44bb50
uint32 GetFollowID();                                                     // 0x44bb60
uint32 GetHighlightID();                                                  // 0x44bb70
int GetBuddyPosition(uint32 CharacterID);                                 // 0x44bb80, position or -1
bool IsBuddy(const char *Name);                                           // 0x44bbb0
int GetPlayerStep(int Step);                                              // 0x44c3c0, line 259
void DoPlayerStep();                                                      // 0x450c80, lines 278-312
int FindPlayerPath();                                                     // 0x4510f0, line 347
int SetPlayerDestination(int x, int y, int z, bool Exact, bool Diagonal); // 0x451400, line 383
void StopPlayer();                                                        // 0x4516e0, line 413
bool CancelPlayerWalk(int NewDirection);                                  // 0x4518b0, line 435

// The creature table.
TCreature *AddCreature(uint32 RemoveID, uint32 NewCreatureID); // 0x44c530, line 450
TCreature *GetCreature(uint32 CreatureID);                     // 0x44c6e0, line 476
void UpdateSkulls();                                           // 0x44c860
void HideCreature(int CreatureID);                             // 0x44c8c0, line 517; a callback for Map.cpp
bool MoveCreatures();                                          // 0x451ab0, line 549
void CheckMonsterHint(TCreature *Creature);                    // 0x451db0, lines 573-592
void NotifySkull(int Flag);                                    // 0x44ca60, line 604

// Experience and skills.
void SetPlayerExperience(int Experience, int Level, int Progress); // 0x44cc00, lines 634-644
void SetPlayerSkill(int SkillNumber, int Value, int Progress);     // 0x44cf10, lines 687-693
int GetPlayerSkillValue(int SkillNumber);                          // 0x44d1c0, line 804
int GetPlayerSkillProgress(int SkillNumber);                       // 0x44d320, line 813
const char *GetSkillName(int SkillNumber);                         // 0x44d480, line 822
bool HasPlayerSkull();                                             // 0x44d6d0
bool IsPlayerInParty();                                            // 0x44d6f0
int GetPlayerPartyFlag();                                          // 0x44d720

// The battle list.
bool IsOpponent(TCreature *Creature); // 0x44d740, lines 987-994
int GetNumberOfOpponents();           // 0x44d9f0, line 1029; also updates the list
TCreature *GetOpponent(int Number);   // 0x44dc70, line 1037

// The VIP list.
bool CompareBuddies(int i, int j);                                               // 0x44de20, lines 1069-1071
void SwapBuddies(int i, int j);                                                  // 0x44e260, lines 1097-1099
void SortBuddies();                                                              // 0x44e600, line 1116
void SortBuddy(int i);                                                           // 0x44e7d0, lines 1124-1135
void AddBuddy(uint32 CharacterID, const char *Name, bool Online);                // 0x44eab0, lines 1144-1170
void SetBuddyState(uint32 CharacterID, bool Online);                             // 0x44f000, line 1192
void EditBuddy(int CharacterID, int Icon, const char *Description, bool Notify); // 0x44f230, lines 1200-1210
void RemoveBuddy(uint32 CharacterID);                                            // 0x44f5e0, line 1227
void SetBuddySortMethod(int Method);                                             // 0x44f810, line 1247
void HideOfflineBuddies(bool Hide);                                              // 0x44fa00, line 1259
int GetNumberOfBuddies();                                                        // 0x44fba0, line 1280; only the online ones when they are hidden
uint32 GetBuddyListID(int Position);                                             // 0x44fd40, line 1288
const char *GetBuddyListName(int Position);                                      // 0x44feb0, line 1297
bool GetBuddyListOnline(int Position);                                           // 0x450020, line 1306
bool GetBuddyListNew(int Position);                                              // 0x450190, line 1316
int GetBuddyListIcon(int Position);                                              // 0x450330, line 1325
const char *GetBuddyListDescription(int Position);                               // 0x4504a0, lines 1334-1338
bool GetBuddyListNotify(int Position);                                           // 0x450740, lines 1346-1350

void InitCreatures();  // 0x452190, line 1366
void ExitCreatures();  // empty, folded into 0x496160
void ResetCreatures(); // 0x4509e0, line 1414
