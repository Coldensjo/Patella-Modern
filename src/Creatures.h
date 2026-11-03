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
#include "Preferences.h" // SKILLS ("In(SkillNumber,0,SKILLS-1)")
#include "graphics.h"    // TRGBColor
#include "Objects.h"     // TObject

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

	uint32 CreatureID;    // +0x00, 0 marks a free slot; players have IDs below 0x40000000
	char Name[32];        // +0x04
	int RealX;            // +0x24, map position
	int RealY;            // +0x28
	int RealZ;            // +0x2c
	int DeltaX;           // +0x30, pixel offset of the walk animation, counts down to 0
	int DeltaY;           // +0x34
	int StepEnd;          // +0x38, GetMilliseconds() time the step animation ends
	int NextStep;         // +0x3c, time the next step may start (three steps' time after a diagonal one)
	int StepX;            // +0x40, abs(dx)*32
	int StepY;            // +0x44, abs(dy)*32
	int StepDuration;     // +0x48, milliseconds, a multiple of Beat
	bool Moving;          // +0x4c
	int Direction;        // +0x50, 0 north, 1 east, 2 south, 3 west
	int MoveDirection;    // +0x54, Direction when the current step started
	int PrevOpponent;     // +0x58, battle list links (slot numbers, -1 at the ends)
	int NextOpponent;     // +0x5c, NOT_AN_OPPONENT when not in the list
	int OutfitType;       // +0x60
	int Colors[4];        // +0x64, head, body, legs, feet; Colors[0] is the object type when OutfitType is 0
	int LightStrength;    // +0x74
	int LightColor;       // +0x78
	int Mark;             // +0x7c, colour of the square around the creature
	int MarkTime;         // +0x80, shown up to this GetMilliseconds() time
	int Health;           // +0x84, percent
	int Speed;            // +0x88
	bool Visible;         // +0x8c
	int PKFlag;           // +0x90, the skull; name from "Creature->PKFlag" (MapWindow.cpp)
	int PARTYFlag;        // +0x94, the party shield; "Creature->PARTYFlag" (MapWindow.cpp)
	__time32_t SkullTime; // +0x98, time a skull was removed; it stays four seconds longer
};

// A VIP list entry (0x2c bytes). Preferences.h has the stored data
// (icon, description, notification) as TBuddy.
struct TBuddyEntry {
	uint32 CharacterID; // +0x00
	char Name[30];      // +0x04, "strlen(Name)<30"
	bool Online;        // +0x22
	int LoginAction;    // +0x24, GetActionNumber() at the last change of Online
	int Icon;           // +0x28
};

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
