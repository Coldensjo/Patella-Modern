// Creatures.cpp: reconstructed from Tibia.exe 7.72 (0x44b740-0x4524df).
//
// The creature table, the player's statistics and walk, the battle list and
// the VIP list. Functions in line order. Where the binary order differs: the
// compiler emits a function only after the bodies of the functions it calls,
// so the ones that call a function defined further down (lines 278-592,
// InitCreatures at line 1366 and SetPosition at line 108) come after
// ResetCreatures (line 1414) in the binary. The functions without exception
// frame (0x44b740-0x44bc6f) have no line evidence; they come first in the
// binary, before the line-142 function, and keep that place and order here
// (notes/decisions.md).
//
// The leading gap (0x44b740-0x44bc6f) is this file: every function uses
// Creatures data (0x5c4560-0x5cc417) or a TCreature, and Control.cpp's last
// global is at 0x5c4554. The trailing gap (0x4524e0-0x45399a) is
// DialogElements.obj (file-ranges.tsv).

#include "Creatures.h"
#include "GUI.h"
#include "Help.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Status.h"
#include "Talk.h"
#include "Text.h" // IWYU pragma: keep (the object's own copy of the text table, see Text.h)
#include "sysinc.h"
#include "common.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

// Assertions whose text carries a bug number after the expression.
#define TASSERT1_BUG(Line, Expr, Bug, V1) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, "", 0, #Expr " [" #Bug "]")
#define TASSERT2_BUG(Line, Expr, Bug, V1, V2) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, #V2, V2, #Expr " [" #Bug "]")

// Data, in .bss order. 0x5c456c and 0x5c68ac are not used by any code.
static int BuddySortMethods[3];         // 0x5c4560, 0 name, 1 icon, 2 online; most significant first
static TBuddyEntry Buddies[BUDDIES];    // 0x5c4570
static int NumberOfOnlineBuddies;       // 0x5c67d0
static int NumberOfBuddies;             // 0x5c67d4
static int PlayerState;                 // 0x5c67d8, the state icons, one bit each
static int PlayerSkillProgress[SKILLS]; // 0x5c67dc
static int PlayerSkillValue[SKILLS];    // 0x5c67f8
static uint32 HighlightID;              // 0x5c6814
static uint32 FollowID;                 // 0x5c6818
static uint32 AttackID;                 // 0x5c681c
static int PlayerCapacity;              // 0x5c6820
static int PlayerSoul;                  // 0x5c6824
static int PlayerMaxMana;               // 0x5c6828
static int PlayerMana;                  // 0x5c682c
static int PlayerMagicLevelProgress;    // 0x5c6830
static int PlayerLevelProgress;         // 0x5c6834
static int PlayerMagicLevel;            // 0x5c6838
static int PlayerLevel;                 // 0x5c683c
static int PlayerExperience;            // 0x5c6840
static int PlayerMaxHealth;             // 0x5c6844
static int PlayerHealth;                // 0x5c6848
static uint32 PlayerID;                 // 0x5c684c
static bool NewPlayerDestination;       // 0x5c6850, a destination came while the old path was walked
static int CurrentPlayerStep;           // 0x5c6854
static int NumberOfPlayerSteps;         // 0x5c6858
static int PlayerSteps[10];             // 0x5c685c, 1 east, 2 north-east, ..., 8 south-east; the path finder stops at 10
static bool DestinationDiagonal;        // 0x5c6884, passed on to the path finder
static bool DestinationExact;           // 0x5c6885, passed on to the path finder
static int DestinationZ;                // 0x5c6888
static int DestinationY;                // 0x5c688c
static int DestinationX;                // 0x5c6890, 0: no destination
static int PlayerStepDirection;         // 0x5c6894, Direction of the step not confirmed yet
static int PlayerStepDY;                // 0x5c6898, the step sent but not confirmed by the server
static int PlayerStepDX;                // 0x5c689c
static int Beat;                        // 0x5c68a0, step durations are rounded up to a multiple
static int LastOpponent;                // 0x5c68a4, battle list ends (slot numbers, -1: empty)
static int FirstOpponent;               // 0x5c68a8
static TCreature Creatures[CREATURES];  // 0x5c68b0
// At 0x5cc418, constructed by the static initialiser 0x559e00 as
// TCallback(NULL, 0) and never used (the other window files have one too);
// the end of Creatures.obj's .bss, before Dialogs.obj's NoCallback 0x5cc420.
static TCallback NoCallback(nullptr, 0);

static_assert(sizeof(TCreature) == 0x9c, "TCreature is 0x9c bytes");
static_assert(sizeof(TBuddyEntry) == 0x2c, "TBuddyEntry is 0x2c bytes");

// --- Functions without exception frame (no line evidence) ------------------

// 0x44b740. Health in percent.
TRGBColor TCreature::GetHealthColor()
{
	TRGBColor Color;
	if (In(Health, 0, 3)) {
		Color.R = 0x60;
		Color.G = 0;
		Color.B = 0;
	} else if (In(Health, 4, 9)) {
		Color.R = 0xc0;
		Color.G = 0;
		Color.B = 0;
	} else if (In(Health, 10, 29)) {
		Color.R = 0xc0;
		Color.G = 0x30;
		Color.B = 0x30;
	} else if (In(Health, 30, 59)) {
		Color.R = 0xc0;
		Color.G = 0xc0;
		Color.B = 0;
	} else if (In(Health, 60, 94)) {
		Color.R = 0x60;
		Color.G = 0xc0;
		Color.B = 0x60;
	} else {
		Color.R = 0;
		Color.G = 0xc0;
		Color.B = 0;
	}
	return Color;
}

// 0x44b800
bool TCreature::IsMarked()
{
	return GetMilliseconds() <= MarkTime;
}

// 0x44b820. With Delay a removed skull is kept and UpdateSkulls takes it
// away four seconds later.
void TCreature::SetSkull(int Skull, bool Delay)
{
	if (Delay && this->PKFlag != 0 && Skull == 0) {
		SkullTime = PATELLA_TIME32(nullptr);
		return;
	}
	SkullTime = 0;
	this->PKFlag = Skull;
}

// 0x44b870
int GetNumberOfPlayerSteps()
{
	return NumberOfPlayerSteps;
}

// 0x44b880. The map centre (GetMapPosition) plus the step that the server
// has not confirmed yet.
void GetPlayerPosition(int *x, int *y, int *z)
{
	GetMapPosition(x, y, z);
	*x += PlayerStepDX;
	*y += PlayerStepDY;
}

// 0x44b8c0
void ClearPlayerSteps()
{
	DestinationX = 0;
	NumberOfPlayerSteps = 0;
	CurrentPlayerStep = 0;
	NewPlayerDestination = false;
}

// 0x44b8e0
void HideAllCreatures()
{
	for (int i = 0; i < CREATURES; i++)
		Creatures[i].Visible = false;
}

// 0x44b900
void SetBeat(int Beat)
{
	::Beat = Beat;
}

// 0x44b910
bool IsPlayer(uint32 CreatureID)
{
	return CreatureID < 0x40000000;
}

// 0x44b920
void SetPlayerID(uint32 CreatureID)
{
	PlayerID = CreatureID;
}

// 0x44b930
void SetPlayerHealth(int Health, int MaxHealth)
{
	PlayerHealth = Health;
	PlayerMaxHealth = MaxHealth;
}

// 0x44b950
void SetPlayerMagicLevel(int Level, int Progress)
{
	PlayerMagicLevel = Level;
	PlayerMagicLevelProgress = Progress;
}

// 0x44b970
void SetPlayerMana(int Mana, int MaxMana)
{
	PlayerMana = Mana;
	PlayerMaxMana = MaxMana;
}

// 0x44b990
void SetPlayerSoul(int Soul)
{
	PlayerSoul = Soul;
}

// 0x44b9a0
void SetPlayerCapacity(int Capacity)
{
	PlayerCapacity = Capacity;
}

// 0x44b9b0
void SetPlayerState(int State)
{
	PlayerState = State;
}

// 0x44b9c0
uint32 GetPlayerID()
{
	return PlayerID;
}

// 0x44b9d0
int GetPlayerHealth()
{
	return PlayerHealth;
}

// 0x44b9e0
int GetPlayerMaxHealth()
{
	return PlayerMaxHealth;
}

// 0x44b9f0
int GetPlayerExperience()
{
	return PlayerExperience;
}

// 0x44ba00
int GetPlayerLevel()
{
	return PlayerLevel;
}

// 0x44ba10
int GetPlayerLevelProgress()
{
	return PlayerLevelProgress;
}

// 0x44ba20
int GetPlayerMana()
{
	return PlayerMana;
}

// 0x44ba30
int GetPlayerMaxMana()
{
	return PlayerMaxMana;
}

// 0x44ba40
int GetPlayerSoul()
{
	return PlayerSoul;
}

// 0x44ba50
int GetPlayerMagicLevel()
{
	return PlayerMagicLevel;
}

// 0x44ba60
int GetPlayerMagicLevelProgress()
{
	return PlayerMagicLevelProgress;
}

// 0x44ba70
int GetPlayerCapacity()
{
	return PlayerCapacity;
}

// 0x44ba80. The state bits in the order of the protocol's icons.
bool IsPlayerPoisoned()
{
	return (PlayerState & 1) != 0;
}

// 0x44ba90
bool IsPlayerBurning()
{
	return (PlayerState >> 1 & 1) != 0;
}

// 0x44baa0
bool IsPlayerElectrified()
{
	return (PlayerState >> 2 & 1) != 0;
}

// 0x44bab0
bool IsPlayerDrunk()
{
	return (PlayerState >> 3 & 1) != 0;
}

// 0x44bac0
bool IsPlayerManaShielded()
{
	return (PlayerState >> 4 & 1) != 0;
}

// 0x44bad0
bool IsPlayerParalyzed()
{
	return (PlayerState >> 5 & 1) != 0;
}

// 0x44bae0
bool IsPlayerHasted()
{
	return (PlayerState >> 6 & 1) != 0;
}

// 0x44baf0
bool IsPlayerInFight()
{
	return (PlayerState >> 7 & 1) != 0;
}

// 0x44bb00
void SetAttackID(uint32 CreatureID)
{
	AttackID = CreatureID;
	FollowID = 0;
}

// 0x44bb20
void SetFollowID(uint32 CreatureID)
{
	FollowID = CreatureID;
	AttackID = 0;
}

// 0x44bb40
void SetHighlightID(uint32 CreatureID)
{
	HighlightID = CreatureID;
}

// 0x44bb50
uint32 GetAttackID()
{
	return AttackID;
}

// 0x44bb60
uint32 GetFollowID()
{
	return FollowID;
}

// 0x44bb70
uint32 GetHighlightID()
{
	return HighlightID;
}

// 0x44bb80. Inlined into SetBuddyState and RemoveBuddy.
int GetBuddyPosition(uint32 CharacterID)
{
	for (int i = 0; i < NumberOfBuddies; i++) {
		if (Buddies[i].CharacterID == CharacterID)
			return i;
	}
	return -1;
}

// 0x44bbb0
bool IsBuddy(const char *Name)
{
	for (int i = 0; i < NumberOfBuddies; i++) {
		if (_stricmp(Name, Buddies[i].Name) == 0)
			return true;
	}
	return false;
}

// --- Creatures -------------------------------------------------------------

// 0x452320, line 108
void TCreature::SetPosition(int x, int y, int z)
{
	TBEGIN
	if (RealX != x || RealY != y || RealZ != z) {
		RealX = x;
		RealY = y;
		RealZ = z;
		DeltaX = 0;
		DeltaY = 0;
		Moving = false;
		CheckMonsterHint(this);
	}
	TEND1(108, CreatureID)
}

// 0x44bbf0. For the player the step not confirmed yet is already drawn.
int TCreature::GetDeltaX()
{
	if (CreatureID == PlayerID)
		return PlayerStepDX * 32 + DeltaX;
	return DeltaX;
}

// 0x44bc10
int TCreature::GetDeltaY()
{
	if (CreatureID == PlayerID)
		return PlayerStepDY * 32 + DeltaY;
	return DeltaY;
}

// 0x44bc30
int TCreature::GetDrawDirection()
{
	if (CreatureID == PlayerID && (PlayerStepDX != 0 || PlayerStepDY != 0))
		return PlayerStepDirection;
	if (Moving)
		return MoveDirection;
	return Direction;
}

// 0x44bc70, line 142. Starts the animation of a step by (dx,dy) onto a
// ground with the given waypoints. For the player a step that DoPlayerStep
// predicted only clears the prediction (the animation runs already); any
// other step counts as the next one of the walk. A diagonal step blocks the
// next step for three times the unrounded duration.
void TCreature::StartMovement(int dx, int dy, int Waypoints)
{
	TBEGIN
	if (dx > 0)
		Direction = 1;
	else if (dx < 0)
		Direction = 3;
	else if (dy < 0)
		Direction = 0;
	else if (dy > 0)
		Direction = 2;
	if (Waypoints != 0 && CreatureID == PlayerID) {
		const bool Predicted = PlayerStepDX == dx && PlayerStepDY == dy;
		PlayerStepDX = 0;
		PlayerStepDY = 0;
		if (Predicted)
			return;
		CurrentPlayerStep++;
	}
	MoveDirection = Direction;
	int Duration;
	if (Speed > 0)
		Duration = Waypoints * 1000 / Speed;
	else
		Duration = 1000;
	Duration = (Beat - 1 + Duration) / Beat * Beat;
	DeltaX = -dx * 32;
	DeltaY = -dy * 32;
	StepX = abs(dx) * 32;
	StepY = abs(dy) * 32;
	StepDuration = Duration;
	StepEnd = GetMilliseconds() + Duration;
	if (dx != 0 && dy != 0) {
		if (Speed > 0)
			Duration = Waypoints * 3000 / Speed;
		else
			Duration = 1000;
	}
	NextStep = GetMilliseconds() + Duration;
	Moving = true;
	TEND1(142, CreatureID)
}

// 0x44bf20, line 154
void TCreature::SetMark(int Mark)
{
	TBEGIN
	this->Mark = Mark;
	MarkTime = GetMilliseconds() + 1000;
	TEND1(154, CreatureID)
}

// 0x44c0d0, lines 163-169. Phase 1 stands still; walking runs through
// phases 2 to AvailablePhases, one every 8 pixels.
int TCreature::GetAnimationPhase(int AvailablePhases)
{
	TASSERT1(163, AvailablePhases>=1, AvailablePhases);
	TBEGIN
	if (AvailablePhases == 1)
		return 1;
	if (DeltaX == 0 && DeltaY == 0 && GetMilliseconds() > StepEnd)
		return 1;
	int Distance = abs(DeltaX);
	if (Distance <= abs(DeltaY))
		Distance = abs(DeltaY);
	return Distance / 8 % (AvailablePhases - 1) + 2;
	TEND2(169, CreatureID, AvailablePhases)
}

// --- The player's walk -----------------------------------------------------

// 0x44c3c0, line 259. No try block.
int GetPlayerStep(int Step)
{
	TASSERT2(259, In(Step,0,NumberOfPlayerSteps-1), Step, NumberOfPlayerSteps-1);
	return PlayerSteps[Step];
}

// 0x450c80, lines 278-312. Starts the current step of the walk at once,
// before the server confirms it, if the field is free and not more than one
// level higher. The map functions take screen coordinates; the player
// stands at (8,6).
void DoPlayerStep()
{
	TASSERT1(278, CurrentPlayerStep<NumberOfPlayerSteps, CurrentPlayerStep);
	TBEGIN
	if (NewPlayerDestination || PlayerHealth == 0)
		return;
	TCreature *Player = GetCreature(GetPlayerID());
	if (Player == nullptr)
		return;
	if (PlayerStepDX != 0 || PlayerStepDY != 0) {
		Player->Moving = true;
		return;
	}
	int dx = 0;
	int dy = 0;
	switch (PlayerSteps[CurrentPlayerStep]) {
		// clang-format off
	case 1: dx = 1; dy = 0; break;
	case 2: dx = 1; dy = -1; break;
	case 3: dx = 0; dy = -1; break;
	case 4: dx = -1; dy = -1; break;
	case 5: dx = -1; dy = 0; break;
	case 6: dx = -1; dy = 1; break;
	case 7: dx = 0; dy = 1; break;
	case 8: dx = 1; dy = 1; break;
		// clang-format on
	}
	const TObject Bank = GetFirstObject(dx + 8, dy + 6, GetMapLevel());
	if (Bank.Type == 0 || !GetObjectFlag(Bank.Type, BANK))
		return;
	if (!IsFieldPassable(dx + 8, dy + 6, GetMapLevel(), false))
		return;
	const int Height = GetFieldHeight(8, 6, GetMapLevel());
	if (GetFieldHeight(dx + 8, dy + 6, GetMapLevel()) > Height + 1)
		return;
	Player->StartMovement(dx, dy, GetObjectWaypoints(Bank.Type));
	PlayerStepDX = dx;
	PlayerStepDY = dy;
	if (dx > 0)
		PlayerStepDirection = 1;
	else if (dx < 0)
		PlayerStepDirection = 3;
	else if (dy < 0)
		PlayerStepDirection = 0;
	else if (dy > 0)
		PlayerStepDirection = 2;
	TEND(312)
}

// 0x4510f0, line 347. Computes the path to the destination and starts its
// first step; 1 if there is a path. A result of 2 from the path finder
// (FindPath) keeps the destination.
int FindPlayerPath()
{
	TBEGIN
	NumberOfPlayerSteps = 0;
	NewPlayerDestination = false;
	if (DestinationX == 0)
		return 0;
	if (abs(PlayerStepDY) + abs(PlayerStepDX) > 1) {
		TCreature *Player = GetCreature(GetPlayerID());
		if (Player != nullptr)
			Player->Moving = true;
		return 0;
	}
	int x, y, z;
	GetMapPosition(&x, &y, &z);
	x += PlayerStepDX;
	y += PlayerStepDY;
	const int Result = FindPath(x, y, z, DestinationX, DestinationY, DestinationZ, DestinationExact,
		&NumberOfPlayerSteps, PlayerSteps, DestinationDiagonal);
	if (Result != 2)
		DestinationX = 0;
	if (Result > 0) {
		CurrentPlayerStep = 0;
		DoPlayerStep();
		return 1;
	}
	switch (Result) {
	case -4:
		SetStatusMessage("There is no way.", false);
		return 0;
	case -3:
		SetStatusMessage("Destination is out of range.", false);
		return 0;
	case -2:
		SetStatusMessage("First go downstairs.", false);
		return 0;
	case -1:
		SetStatusMessage("First go upstairs.", false);
		return 0;
	}
	return 0;
	TEND(347)
}

// 0x451400, line 383. 0 if nothing is to be sent, otherwise the result of
// FindPlayerPath, or 2 when the new path waits for the current step.
// The second comparison adds the unconfirmed step twice (GetPlayerPosition
// has added it already); kept from the binary.
int SetPlayerDestination(int x, int y, int z, bool Exact, bool Diagonal)
{
	TBEGIN
	int PlayerX, PlayerY, PlayerZ;
	GetPlayerPosition(&PlayerX, &PlayerY, &PlayerZ);
	if (x == DestinationX && y == DestinationY && z == DestinationZ)
		return 0;
	if (x == PlayerX + PlayerStepDX && y == PlayerY + PlayerStepDY && z == PlayerZ)
		return 0;
	DestinationX = x;
	DestinationY = y;
	DestinationZ = z;
	DestinationExact = Exact;
	DestinationDiagonal = Diagonal;
	AbsoluteToRelative(&x, &y, &z);
	if (In(x, 1, 15) && In(y, 1, 11) && z == GetMapLevel() && (x != 8 || y != 6)) {
		if (!IsFieldPassable(x, y, z, true)) {
			SetStatusMessage("Sorry, not possible.", false);
			DestinationX = 0;
			return 0;
		}
	}
	if (!NewPlayerDestination) {
		if (NumberOfPlayerSteps == 0)
			return FindPlayerPath();
		if (CurrentPlayerStep < NumberOfPlayerSteps) {
			NewPlayerDestination = true;
			return 2;
		}
	}
	return 0;
	TEND(383)
}

// 0x4516e0, line 413
void StopPlayer()
{
	TBEGIN
	DestinationX = 0;
	NumberOfPlayerSteps = 0;
	CurrentPlayerStep = 0;
	NewPlayerDestination = false;
	PlayerStepDX = 0;
	PlayerStepDY = 0;
	TCreature *Player = GetCreature(GetPlayerID());
	if (Player != nullptr) {
		Player->MoveDirection = Player->Direction;
		Player->Moving = false;
		Player->DeltaX = 0;
		Player->DeltaY = 0;
	}
	TEND(413)
}

// 0x4518b0, line 435. The server refused a step: turn to NewDirection and
// plan the path again; true if a new walk is to be sent.
bool CancelPlayerWalk(int NewDirection)
{
	TBEGIN
	NumberOfPlayerSteps = 0;
	TCreature *Player = GetCreature(GetPlayerID());
	if (Player == nullptr) {
		PlayerStepDX = 0;
		PlayerStepDY = 0;
	} else {
		Player->Direction = NewDirection;
		if (PlayerStepDX != 0 || PlayerStepDY != 0 || !Player->Moving) {
			Player->StartMovement(0, 0, -1);
			return FindPlayerPath() == 1;
		}
	}
	return false;
	TEND2(435, NewDirection, GetPlayerID())
}

// --- The creature table ----------------------------------------------------

// 0x44c530, line 450. Takes the slot of RemoveID (a free slot when RemoveID
// is 0); without one the last slot is overwritten. A reused slot keeps its
// name, position and battle list links. The binary has eleven EH states
// for a try block after the assertion but no try block: nothing in it can
// throw, so the compiler dropped it with its catch blocks.
TCreature *AddCreature(uint32 RemoveID, uint32 NewCreatureID)
{
	TASSERT(450, NewCreatureID!=0);
	TBEGIN
	int Index = -1;
	for (int i = 0; i < CREATURES; i++) {
		if (Creatures[i].CreatureID == RemoveID) {
			Index = i;
			break;
		}
	}
	if (Index == -1)
		Index = CREATURES - 1;
	TCreature *Creature = &Creatures[Index];
	if (Creature->CreatureID == 0)
		Creature->NextOpponent = NOT_AN_OPPONENT;
	Creature->PKFlag = 0;
	Creature->PARTYFlag = 0;
	Creature->SkullTime = 0;
	Creature->CreatureID = NewCreatureID;
	return Creature;
	TEND(470)
}

// 0x44c6e0, line 476. No try block.
TCreature *GetCreature(uint32 CreatureID)
{
	TASSERT(476, CreatureID!=0);
	for (int i = 0; i < CREATURES; i++) {
		if (Creatures[i].CreatureID == CreatureID)
			return &Creatures[i];
	}
	return nullptr;
}

// 0x44c860. Removes the skulls that SetSkull kept. Like AddCreature the
// binary has the EH states of a try block it dropped.
void UpdateSkulls()
{
	TBEGIN
	const __time32_t Now = PATELLA_TIME32(nullptr);
	for (int i = 0; i < CREATURES; i++) {
		TCreature *Creature = &Creatures[i];
		if (Creature->SkullTime != 0 && (Now < Creature->SkullTime || Now > Creature->SkullTime + 4)) {
			Creature->SkullTime = 0;
			Creature->PKFlag = 0;
		}
	}
	TEND(504)
}

// 0x44c8c0, line 517
void HideCreature(int CreatureID)
{
	TBEGIN
	TCreature *Creature = GetCreature(CreatureID);
	if (Creature != nullptr)
		Creature->Visible = false;
	TEND1(517, CreatureID)
}

// 0x451ab0, line 549. Advances the walk animations; when the player's step
// is over the next one starts. True if a new walk is to be sent.
bool MoveCreatures()
{
	TBEGIN
	bool Result = false;
	const int Now = GetMilliseconds();
	for (int i = 0; i < CREATURES; i++) {
		TCreature *Creature = &Creatures[i];
		if (!Creature->Moving)
			continue;
		if (Creature->DeltaX != 0) {
			while (Creature->DeltaX > 0
				&& Creature->StepDuration * Creature->DeltaX > (Creature->StepEnd - Now) * Creature->StepX)
				Creature->DeltaX--;
			while (Creature->DeltaX < 0
				&& -(Creature->StepDuration * Creature->DeltaX) > (Creature->StepEnd - Now) * Creature->StepX)
				Creature->DeltaX++;
		}
		if (Creature->DeltaY != 0) {
			while (Creature->DeltaY > 0
				&& Creature->StepDuration * Creature->DeltaY > (Creature->StepEnd - Now) * Creature->StepY)
				Creature->DeltaY--;
			while (Creature->DeltaY < 0
				&& -(Creature->StepDuration * Creature->DeltaY) > (Creature->StepEnd - Now) * Creature->StepY)
				Creature->DeltaY++;
		}
		if (Creature->DeltaX == 0 && Creature->DeltaY == 0 && Now > Creature->NextStep) {
			Creature->Moving = false;
			if (Creature->CreatureID == PlayerID) {
				if (CurrentPlayerStep < NumberOfPlayerSteps)
					DoPlayerStep();
				else if (FindPlayerPath() == 1)
					Result = true;
			}
		}
	}
	return Result;
	TEND(549)
}

// 0x451db0, lines 573-592. Help hints (CheckEventHelpName) for the monsters next
// to the player. Without a player creature a monster in the battle list
// fails IsOpponent's assertion.
void CheckMonsterHint(TCreature *Creature)
{
	TASSERT(573, Creature!=NULL);
	TBEGIN
	if (!Creature->Visible)
		return;
	const TCreature *Player = GetCreature(GetPlayerID());
	if (Player != nullptr && Creature->CreatureID == Player->CreatureID) {
		const int Number = GetNumberOfOpponents();
		for (int i = 0; i < Number; i++) {
			TCreature *Opponent = GetOpponent(i);
			TASSERT(585, Opponent!=NULL);
			if (!IsPlayer(Opponent->CreatureID) && abs(Opponent->RealX - Player->RealX) <= 2
				&& abs(Opponent->RealY - Player->RealY) <= 2)
				CheckEventHelpName(Opponent->Name);
		}
	} else if (!IsPlayer(Creature->CreatureID) && IsOpponent(Creature)
		&& abs(Creature->RealX - Player->RealX) <= 2 && abs(Creature->RealY - Player->RealY) <= 2) {
		CheckEventHelpName(Creature->Name);
	}
	TEND(592)
}

// 0x44ca60, line 604. Communication.cpp passes the skull of every creature
// it reads.
void NotifySkull(int Flag)
{
	TBEGIN
	if (Flag != 0)
		AddEventHelp(13);
	TEND1(604, Flag)
}

// --- Experience and skills -------------------------------------------------

// 0x44cc00, lines 634-644. The help hints for the first experience and the
// levels 2, 8 and 20; none while the level is unknown (0).
void SetPlayerExperience(int Experience, int Level, int Progress)
{
	TASSERT1_BUG(634, In(Progress,-1,100), bug0000038, Progress);
	TBEGIN
	if (PlayerLevel != 0 && PlayerExperience == 0 && Experience > 0)
		AddEventHelp(7);
	if (PlayerLevel != 0 && PlayerLevel < 2 && Level >= 2)
		AddEventHelp(8);
	if (PlayerLevel != 0 && PlayerLevel < 8 && Level >= 8)
		AddEventHelp(10);
	if (PlayerLevel != 0 && PlayerLevel < 20 && Level >= 20)
		AddEventHelp(14);
	PlayerExperience = Experience;
	PlayerLevel = Level;
	PlayerLevelProgress = Progress;
	TEND(644)
}

// 0x44cf10, lines 687-693. SkillNumber is not checked.
void SetPlayerSkill(int SkillNumber, int Value, int Progress)
{
	TASSERT1(687, In(Progress,0,100), Progress);
	TBEGIN
	if (PlayerSkillValue[SkillNumber] != 0 && PlayerSkillValue[SkillNumber] < Value)
		AddEventHelp(9);
	PlayerSkillProgress[SkillNumber] = Progress;
	PlayerSkillValue[SkillNumber] = Value;
	TEND2(693, SkillNumber, Value)
}

// 0x44d1c0, line 804. No try block.
int GetPlayerSkillValue(int SkillNumber)
{
	TASSERT1(804, In(SkillNumber,0,SKILLS-1), SkillNumber);
	return PlayerSkillValue[SkillNumber];
}

// 0x44d320, line 813. No try block.
int GetPlayerSkillProgress(int SkillNumber)
{
	TASSERT1(813, In(SkillNumber,0,SKILLS-1), SkillNumber);
	return PlayerSkillProgress[SkillNumber];
}

// 0x44d480, line 822. No try block.
const char *GetSkillName(int SkillNumber)
{
	TASSERT1(822, In(SkillNumber,0,SKILLS-1), SkillNumber);
	switch (SkillNumber) {
	case 0: return "Fist Fighting";
	case 1: return "Club Fighting";
	case 2: return "Sword Fighting";
	case 3: return "Axe Fighting";
	case 4: return "Distance Fighting";
	case 5: return "Shielding";
	case 6: return "Fishing";
	default: return "";
	}
}

// 0x44d6d0. No check for a missing player creature.
bool HasPlayerSkull()
{
	return GetCreature(GetPlayerID())->PKFlag != 0;
}

// 0x44d6f0. Party flag 1, 3 or 4; ContextMenus.cpp offers "Leave Party"
// then.
bool IsPlayerInParty()
{
	const int Shield = GetCreature(GetPlayerID())->PARTYFlag;
	return Shield == 4 || Shield == 3 || Shield == 1;
}

// 0x44d720
int GetPlayerPartyFlag()
{
	return GetCreature(GetPlayerID())->PARTYFlag;
}

// --- The battle list -------------------------------------------------------

// 0x44d740, lines 987-994. No try block. A visible, living creature on the
// player's level within the screen; not monsters that are invisible
// (outfit 0 showing object 0).
bool IsOpponent(TCreature *Creature)
{
	TASSERT(987, Creature!=NULL);
	if (Creature->CreatureID == 0 || Creature->CreatureID == GetPlayerID() || !Creature->Visible)
		return false;
	if (!IsPlayer(Creature->CreatureID) && Creature->OutfitType == 0 && Creature->Colors[0] == 0)
		return false;
	if (Creature->Health < 1)
		return false;
	const TCreature *Player = GetCreature(GetPlayerID());
	TASSERT(994, Player!=NULL);
	if (Creature->RealZ != Player->RealZ || abs(Creature->RealX - Player->RealX) > 7)
		return false;
	return abs(Creature->RealY - Player->RealY) <= 5;
}

// 0x44d9f0, line 1029. Brings the battle list up to date: new opponents are
// appended, the others taken out. Returns the number of opponents.
int GetNumberOfOpponents()
{
	TBEGIN
	int Number = 0;
	for (int i = 0; i < CREATURES; i++) {
		TCreature *Creature = &Creatures[i];
		if (Creature->CreatureID == 0)
			continue;
		if (IsOpponent(Creature)) {
			if (Creature->NextOpponent == NOT_AN_OPPONENT) {
				Creature->NextOpponent = -1;
				Creature->PrevOpponent = LastOpponent;
				if (LastOpponent != -1)
					Creatures[LastOpponent].NextOpponent = i;
				LastOpponent = i;
				if (FirstOpponent == -1)
					FirstOpponent = i;
			}
			Number++;
		} else if (Creature->NextOpponent != NOT_AN_OPPONENT) {
			if (Creature->NextOpponent != -1)
				Creatures[Creature->NextOpponent].PrevOpponent = Creature->PrevOpponent;
			if (Creature->PrevOpponent != -1)
				Creatures[Creature->PrevOpponent].NextOpponent = Creature->NextOpponent;
			if (FirstOpponent == i)
				FirstOpponent = Creature->NextOpponent;
			if (LastOpponent == i)
				LastOpponent = Creature->PrevOpponent;
			Creature->NextOpponent = NOT_AN_OPPONENT;
		}
	}
	return Number;
	TEND(1029)
}

// 0x44dc70, line 1037. No try block. Number counts from 0 in list order.
TCreature *GetOpponent(int Number)
{
	TASSERT1(1037, Number>=0, Number);
	int i = FirstOpponent;
	while (i != -1 && Number > 0) {
		i = Creatures[i].NextOpponent;
		Number--;
	}
	if (i == -1)
		return nullptr;
	return &Creatures[i];
}

// --- The VIP list ----------------------------------------------------------

// 0x44de20, lines 1069-1071. No try block. True if buddy i may stand
// before buddy j (also when they are equal).
bool CompareBuddies(int i, int j)
{
	TASSERT1(1069, i!=j, i);
	TASSERT2(1070, In(i,0,NumberOfBuddies-1), i, NumberOfBuddies-1);
	TASSERT2(1071, In(j,0,NumberOfBuddies-1), j, NumberOfBuddies-1);
	for (int k = 0; k < 3; k++) {
		switch (BuddySortMethods[k]) {
		case 0: {
			const int Result = strcmp(Buddies[i].Name, Buddies[j].Name);
			if (Result < 0)
				return true;
			if (Result > 0)
				return false;
			break;
		}
		case 1:
			if (Buddies[i].Icon < Buddies[j].Icon)
				return true;
			if (Buddies[i].Icon > Buddies[j].Icon)
				return false;
			break;
		case 2:
			if (Buddies[i].Online && !Buddies[j].Online)
				return true;
			if (!Buddies[i].Online && Buddies[j].Online)
				return false;
			break;
		}
	}
	return true;
}

// 0x44e260, lines 1097-1099. No try block.
void SwapBuddies(int i, int j)
{
	TASSERT1(1097, i!=j, i);
	TASSERT2(1098, In(i,0,NumberOfBuddies-1), i, NumberOfBuddies-1);
	TASSERT2(1099, In(j,0,NumberOfBuddies-1), j, NumberOfBuddies-1);
	const TBuddyEntry Help = Buddies[i];
	Buddies[i] = Buddies[j];
	Buddies[j] = Help;
}

// 0x44e600, line 1116
void SortBuddies()
{
	TBEGIN
	for (int i = 0; i < NumberOfBuddies - 1; i++) {
		for (int j = i + 1; j < NumberOfBuddies; j++) {
			if (!CompareBuddies(i, j))
				SwapBuddies(i, j);
		}
	}
	TEND(1116)
}

// 0x44e7d0, lines 1124-1135. Moves buddy i to its place. CompareBuddies is
// also true for equal buddies, so i first moves up past every equal one.
void SortBuddy(int i)
{
	TASSERT2(1124, In(i,0,NumberOfBuddies-1), i, NumberOfBuddies-1);
	TBEGIN
	while (i > 0 && CompareBuddies(i, i - 1)) {
		SwapBuddies(i, i - 1);
		i--;
	}
	while (i < NumberOfBuddies - 1 && !CompareBuddies(i, i + 1)) {
		SwapBuddies(i, i + 1);
		i++;
	}
	TEND1(1135, i)
}

// 0x44eab0, lines 1144-1170. Adds the buddy or updates the entry. An
// entry that was online and is updated as offline stays counted as online.
void AddBuddy(uint32 CharacterID, const char *Name, bool Online)
{
	TASSERT(1144, Name!=NULL);
	TASSERT1(1145, strlen(Name)<30, strlen(Name));
	TBEGIN
	int Position = GetBuddyPosition(CharacterID);
	if (Position == -1) {
		TASSERT1(1151, NumberOfBuddies<BUDDIES, NumberOfBuddies);
		Position = NumberOfBuddies++;
		if (Online)
			NumberOfOnlineBuddies++;
	} else {
		TASSERT1(1156, NumberOfBuddies<=BUDDIES, NumberOfBuddies);
		if (Online && !Buddies[Position].Online)
			NumberOfOnlineBuddies++;
	}
	TBuddyEntry *Buddy = &Buddies[Position];
	Buddy->CharacterID = CharacterID;
	strcpy(Buddy->Name, Name);
	Buddy->Online = Online;
	Buddy->LoginAction = 0;
	Buddy->Icon = GetBuddyIcon(CharacterID);
	SortBuddy(Position);
	TEND1(1170, CharacterID)
}

// 0x44f000, line 1192
void SetBuddyState(uint32 CharacterID, bool Online)
{
	TBEGIN
	const int Position = GetBuddyPosition(CharacterID);
	if (Position == -1)
		return;
	TBuddyEntry *Buddy = &Buddies[Position];
	if (Buddy->Online)
		NumberOfOnlineBuddies--;
	Buddy->Online = Online;
	if (Online)
		NumberOfOnlineBuddies++;
	Buddy->LoginAction = GetActionNumber();
	if (Online && GetBuddyNotify(CharacterID)) {
		char Text[256];
		sprintf(Text, "%s has logged in.", Buddy->Name);
		SetStatusMessage(Text, false);
	}
	SortBuddy(Position);
	TEND1(1192, CharacterID)
}

// 0x44f230, lines 1200-1210. Stores the buddy's data in the preferences
// (an empty entry deletes it there) and resorts the list.
void EditBuddy(int CharacterID, int Icon, const char *Description, bool Notify)
{
	TASSERT(1200, CharacterID!=0);
	TASSERT(1201, Description!=NULL);
	TBEGIN
	if (Icon == 0 && Description[0] == 0 && !Notify)
		DeleteBuddy(CharacterID);
	else
		SetBuddy(CharacterID, Icon, Description, Notify);
	const int Position = GetBuddyPosition(CharacterID);
	if (Position != -1) {
		Buddies[Position].Icon = Icon;
		SortBuddy(Position);
	}
	TEND1(1210, CharacterID)
}

// 0x44f5e0, line 1227. Message 0x1a (Status.cpp) gets a copy of the name.
void RemoveBuddy(uint32 CharacterID)
{
	TBEGIN
	const int Position = GetBuddyPosition(CharacterID);
	if (Position == -1)
		return;
	AddMessage(0x1a, (INT_PTR)_strdup(Buddies[Position].Name), 0);
	if (Buddies[Position].Online)
		NumberOfOnlineBuddies--;
	for (int i = Position; i < NumberOfBuddies - 1; i++)
		Buddies[i] = Buddies[i + 1];
	NumberOfBuddies--;
	DeleteBuddy(CharacterID);
	TEND1(1227, CharacterID)
}

// 0x44f810, line 1247. Method becomes the most significant sort key. With
// offline buddies hidden the online state is made the first key at once.
void SetBuddySortMethod(int Method)
{
	TBEGIN
	if (Method == BuddySortMethods[0])
		return;
	if (Method == BuddySortMethods[2])
		BuddySortMethods[2] = BuddySortMethods[1];
	BuddySortMethods[1] = BuddySortMethods[0];
	BuddySortMethods[0] = Method;
	if (!GetBuddyHideOffline() || Method == 2)
		SortBuddies();
	else
		SetBuddySortMethod(2);
	int Methods[3];
	for (int i = 0; i < 3; i++)
		Methods[i] = BuddySortMethods[i];
	SetBuddySortmethods(Methods);
	TEND1(1247, Method)
}

// 0x44fa00, line 1259
void HideOfflineBuddies(bool Hide)
{
	TBEGIN
	SetBuddyHideOffline(Hide);
	if (Hide)
		SetBuddySortMethod(2);
	TEND(1259)
}

// 0x44fba0, line 1280
int GetNumberOfBuddies()
{
	TBEGIN
	if (GetBuddyHideOffline())
		return NumberOfOnlineBuddies;
	return NumberOfBuddies;
	TEND(1280)
}

// 0x44fd40, line 1288. No try block.
uint32 GetBuddyListID(int Position)
{
	TASSERT2(1288, In(Position,0,NumberOfBuddies-1), Position, NumberOfBuddies-1);
	return Buddies[Position].CharacterID;
}

// 0x44feb0, line 1297. No try block.
const char *GetBuddyListName(int Position)
{
	TASSERT2(1297, In(Position,0,NumberOfBuddies-1), Position, NumberOfBuddies-1);
	return Buddies[Position].Name;
}

// 0x450020, line 1306. No try block.
bool GetBuddyListOnline(int Position)
{
	TASSERT2(1306, In(Position,0,NumberOfBuddies-1), Position, NumberOfBuddies-1);
	return Buddies[Position].Online;
}

// 0x450190, line 1316. No try block. Online and logged in less than ten
// actions ago.
bool GetBuddyListNew(int Position)
{
	TASSERT2(1316, In(Position,0,NumberOfBuddies-1), Position, NumberOfBuddies-1);
	if (Buddies[Position].Online && GetActionNumber() - 10 < Buddies[Position].LoginAction)
		return true;
	return false;
}

// 0x450330, line 1325. No try block.
int GetBuddyListIcon(int Position)
{
	TASSERT2_BUG(1325, In(Position,0,NumberOfBuddies-1), bug0000289, Position, NumberOfBuddies-1);
	return Buddies[Position].Icon;
}

// 0x4504a0, lines 1334-1338
const char *GetBuddyListDescription(int Position)
{
	TASSERT2(1334, In(Position,0,NumberOfBuddies-1), Position, NumberOfBuddies-1);
	TBEGIN
	return GetBuddyDescription(Buddies[Position].CharacterID);
	TEND1(1338, Position)
}

// 0x450740, lines 1346-1350
bool GetBuddyListNotify(int Position)
{
	TASSERT2(1346, In(Position,0,NumberOfBuddies-1), Position, NumberOfBuddies-1);
	TBEGIN
	return GetBuddyNotify(Buddies[Position].CharacterID);
	TEND1(1350, Position)
}

// --- Initialisation --------------------------------------------------------

// 0x452190, line 1366. Called by InitControl.
void InitCreatures()
{
	TBEGIN
	ResetCreatures();
	TEND(1366)
}

// 0x4509e0, line 1414. Called by InitCreatures and EnterGame. The beat,
// soul, highlighted creature, step direction and the destination's y and z
// stay; the sort methods come from the preferences when they are valid.
void ResetCreatures()
{
	TBEGIN
	memset(Creatures, 0, sizeof(Creatures));
	for (int i = 0; i < CREATURES; i++)
		Creatures[i].CreatureID = 0;
	FirstOpponent = -1;
	LastOpponent = -1;
	PlayerStepDX = 0;
	PlayerStepDY = 0;
	DestinationX = 0;
	NumberOfPlayerSteps = 0;
	CurrentPlayerStep = 0;
	NewPlayerDestination = false;
	PlayerID = 0;
	PlayerHealth = 0;
	PlayerMaxHealth = 0;
	PlayerExperience = 0;
	PlayerLevel = 0;
	PlayerLevelProgress = 0;
	PlayerMana = 0;
	PlayerMaxMana = 0;
	PlayerMagicLevel = 0;
	PlayerMagicLevelProgress = 0;
	PlayerCapacity = 0;
	AttackID = 0;
	FollowID = 0;
	PlayerState = 0;
	for (int i = 0; i < SKILLS; i++) {
		PlayerSkillValue[i] = 0;
		PlayerSkillProgress[i] = 0;
	}
	NumberOfBuddies = 0;
	NumberOfOnlineBuddies = 0;
	BuddySortMethods[0] = 2;
	BuddySortMethods[1] = 1;
	BuddySortMethods[2] = 0;
	int Methods[3];
	GetBuddySortmethods(Methods);
	for (int i = 0; i < 3; i++) {
		if (In(Methods[i], 0, 2))
			BuddySortMethods[i] = Methods[i];
	}
	TEND(1414)
}

// Empty; the linker kept one copy of the identical empty functions (0x496160,
// GUI.obj). ExitControl calls it where Creatures.cpp's exit belongs in the reverse
// order of InitControl (name ours, low confidence).
void ExitCreatures()
{
}

#if defined(PATELLA_TEST_MODE)
// The test mode's state dump (issue #22; src/testmode/ClientView.h) reads this file's data; it writes
// nothing. Test build only.
void patella::testmode::ViewCreatures(TCreaturesView *View)
{
	View->PlayerState = &PlayerState;
	View->PlayerSkillProgress = PlayerSkillProgress;
	View->PlayerSkillValue = PlayerSkillValue;
	View->FollowID = &FollowID;
	View->AttackID = &AttackID;
	View->PlayerCapacity = &PlayerCapacity;
	View->PlayerSoul = &PlayerSoul;
	View->PlayerMaxMana = &PlayerMaxMana;
	View->PlayerMana = &PlayerMana;
	View->PlayerMagicLevelProgress = &PlayerMagicLevelProgress;
	View->PlayerLevelProgress = &PlayerLevelProgress;
	View->PlayerMagicLevel = &PlayerMagicLevel;
	View->PlayerLevel = &PlayerLevel;
	View->PlayerExperience = &PlayerExperience;
	View->PlayerMaxHealth = &PlayerMaxHealth;
	View->PlayerHealth = &PlayerHealth;
	View->PlayerID = &PlayerID;
	View->Creatures = Creatures;
}
#endif
