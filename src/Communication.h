// Communication.cpp: Tibia 7.72 protocol senders and receive handlers.
// Parameter names and assertion identifiers follow the binary; remaining names are inferred.
//
// Patella-Modern issue #105 (M7, notes/decisions.md, "#104: TClient and the subsystem objects" and "#105"): the data
// is TCommunication's, which TClient owns (Client.h); the free functions of the original's names are the bridge that
// the callers use until they hold the object (bridge::Communication(), defined in Client.cpp).

#pragma once

#include "common.h"
// Nothing of Talk.h is used here, but without it the objects that include this header declare its names
// later, and MSVC, which orders an object's globals by the names its translation unit declares (issue #43),
// emits Communication.obj's .bss in another order (OutfitRequested after the ATL statics): the images' .data
// moves (measured on x64, issue #49).
#include "Talk.h" // IWYU pragma: keep
#include "Status.h"
#include <array>

namespace patella {

class TCreatures;
class TInventory;
class TMap;
class TMapState;
class TNetwork;
class TObjectTypes;
class TPlayerState;

// The signatures of Tibia.spr and Tibia.pic that the login packet sends (SendLogin; issue #110): TGraphics' data
// (graphics.h), and TGraphics is made after TCommunication, so an interface this header declares (notes/decisions.md,
// "#104", rule 5), which Client.cpp's adapter implements by reading the one TClient's TGraphics. Communication.cpp
// does not include graphics.h (whose engine part is the platform's surface): patella_core compiles it.
class TLoginSignatures {
public:
	TLoginSignatures() = default;
	TLoginSignatures(const TLoginSignatures &) = delete;
	TLoginSignatures &operator=(const TLoginSignatures &) = delete;
	TLoginSignatures(TLoginSignatures &&) = delete;
	TLoginSignatures &operator=(TLoginSignatures &&) = delete;

	virtual uint32 SpriteSignature() = 0;
	virtual uint32 PictureSignature() = 0;

protected:
	~TLoginSignatures() = default;
};

// Communication.cpp's data and functions (issue #105): the globals of the original (0x5c4340-0x5c4387), with
// Control.cpp's LastPacketType, LastPacketIndex and LastPacketTypes (0x5c0d8c, 0x5c4510-0x5c451d), are the members,
// with the same initial values, and the functions the members of the same names; Status.cpp's session is its TSession
// (Status.h). It sends and reads through its TNetwork; the packet handlers write the game state through TMapState's
// five objects (issue #106) and call the windows through those files' functions until TGUI (#107) is an object.
class TCommunication {
public:
	TCommunication(TNetwork &NetworkRef, TMapState &MapState, TLoginSignatures &SignaturesRef);
	TCommunication(const TCommunication &) = delete;
	TCommunication &operator=(const TCommunication &) = delete;
	TCommunication(TCommunication &&) = delete;
	TCommunication &operator=(TCommunication &&) = delete;
	~TCommunication() = default;

	// Empty, so static: no this (Clang's UBSan would check it, and the golden Communication test reads
	// ExitCommunication's code as the original's single ret).
	static void InitCommunication();
	static void ExitCommunication(); // empty, folded into 0x496160
	void SendLogin(uint32 AccountID, const char *Password);
	void SendGameLogin(uint32 AccountID, const char *Name, const char *Password);
	void SendLogout();
	void SendPing();
	void PrepareMovement();
	void SendWalk();
	void SendStop();
	void SendTurnNorth();
	void SendTurnEast();
	void SendTurnSouth();
	void SendTurnWest();
	void SendMoveObject(int x, int y, int z, int ObjectType, int Position, int DestinationX, int DestinationY, int DestinationZ, int Count);
	void SendRequestTrade(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID);
	void SendInspectTrade(int Side, int Position);
	void SendAcceptTrade();
	void SendRejectTrade();
	void SendUseObject(int x, int y, int z, int ObjectType, int Position, int Container);
	void SendUseObjectWith(int x1, int y1, int z1, int ObjectType1, int Position1, int x2, int y2, int z2, int ObjectType2, int Position2);
	void SendUseObjectOnCreature(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID);
	void SendRotateObject(int x, int y, int z, int ObjectType, int Position);
	void SendCloseContainer(int Container);
	void SendContainerUp(int Container);
	void SendEditText(uint32 ObjectNumber, const char *Text);
	void SendEditHouseText(int Type, uint32 Identifier, const char *Text);
	void SendLook(int x, int y, int z, int ObjectType, int Position);
	void SendTalk(int Mode, const char *Text);
	void SendTalk(int Mode, const char *Addressee, const char *Text);
	void SendTalk(int Mode, int ChannelID, const char *Text);
	void SendRequestChannels();
	void SendJoinChannel(int ChannelID);
	void SendLeaveChannel(int ChannelID);
	void SendOpenPrivateConversation(const char *Name);
	void SendProcessRuleViolation(const char *Name);
	void SendCloseRuleViolation(const char *Name);
	void SendCancelRuleViolation();
	void SendFightModes();
	void SendAttack(uint32 CreatureID);
	void SendFollow(uint32 CreatureID);
	void SendOpenPrivateChannel();
	void SendInviteToPrivateChannel(const char *Name);
	void SendExcludeFromPrivateChannel(const char *Name);
	void SendInviteToParty(uint32 CharacterID);
	void SendJoinParty(uint32 CharacterID);
	void SendRevokePartyInvitation(uint32 CharacterID);
	void SendPassPartyLeadership(uint32 CharacterID);
	void SendLeaveParty();
	void SendCancel();
	void SendRefreshField(int x, int y, int z);
	void SendRefreshContainer(int Container);
	void SendRequestOutfit();
	void SendOutfit(int OutfitType, const int *Colors);
	void SendAddBuddy(const char *Name);
	void SendRemoveBuddy(uint32 CharacterID);
	void SendBugReport(const char *Text);
	void SendRuleViolationReport(const char *Name, uint8 Reason, uint8 Action, const char *Comment, uint32 StatementID, bool IPBanishment);
	void ReceiveLoginError();
	void ReceiveLoginMessage();
	void ReceiveMessageOfTheDay();
	void ReceiveUpdateRequest(); // packets 0x1e, 0x1f and 0x20 (the patcher removed: issue #75)
	void ReceiveCharacterList();
	void ReceiveLoginServerOverloaded();
	void ReceiveGameLogin();
	void ReceiveGamemasterActions();
	void ReceiveGameError();
	void ReceiveGameMessage();
	void ReceiveLoginWait();
	void ReceivePing();
	uint32 ReadCreature(int Type, int x, int y, int z);
	void ReadField(int rx, int ry, int rz);
	void ReadMap(int xmin, int ymin, int xmax, int ymax);
	void ReceiveMap();
	void ReceiveMapNorth();
	void ReceiveMapEast();
	void ReceiveMapSouth();
	void ReceiveMapWest();
	void ReceiveField();
	void ReceiveAddObject();
	void ReceiveChangeObject();
	void ReceiveDeleteObject();
	void ReceiveMoveCreature();
	void ReceiveOpenContainer(int &Container);
	void ReceiveCloseContainer(int &Container);
	void ReceiveContainerAddObject();
	void ReceiveContainerChangeObject();
	void ReceiveContainerDeleteObject();
	void ReceiveInventoryObject();
	void ReceiveDeleteInventoryObject();
	void ReceiveOwnTrade();
	void ReceiveCounterTrade();
	void ReceiveCloseTrade();
	void ReceiveWorldLight();
	void ReceiveMagicEffect();
	void ReceiveAnimatedText();
	void ReceiveDistanceEffect();
	void ReceiveCreatureMark();
	void ReceiveCreatureHealth();
	void ReceiveCreatureLight();
	void ReceiveCreatureOutfit();
	void ReceiveCreatureSpeed();
	void ReceiveCreatureSkull();
	void ReceiveCreatureShield();
	void ReceiveEditText();
	void ReceiveEditHouseText();
	void ReceivePlayerStats();
	void ReceivePlayerSkills();
	void ReceivePlayerIcons();
	void ReceiveCancelAttack();
	void ReceiveTalk();
	void ReceiveChannelList();
	void ReceiveOpenChannel();
	void ReceiveOpenPrivateConversation();
	void ReceiveRuleViolationChannel();
	void ReceiveRemoveRuleViolation();
	void ReceiveCancelRuleViolation();
	void ReceiveLockRuleViolation();
	void ReceiveOwnChannel();
	void ReceiveCloseChannel();
	void ReceiveTextMessage();
	void ReceiveCancelWalk();
	void ReadFloor(int rz);
	void ReceiveFloorUp();
	void ReceiveFloorDown();
	void ReceiveOutfit();
	void ReceiveBuddy();
	void ReceiveBuddyLogin();
	void ReceiveBuddyLogout();

	// Data, in the original's .data/.bss order. Public: Control.cpp writes OutfitRequested and the packet types
	// (ReceivePackets, InitWindow), the golden Communication and Control tests set them; Status.cpp's functions reach
	// Session through bridge::Session().
	int LastPacketType = -1;                 // 0x5c0d8c, Control.cpp's: only written (ReceivePackets)
	bool OutfitRequested{};                  // 0x5c4358; also reset by Control.cpp
	int LastPacketIndex{};                   // 0x5c4510, Control.cpp's: next slot in LastPacketTypes
	std::array<uint8, 10> LastPacketTypes{}; // 0x5c4514, Control.cpp's: the last ten packet types, a ring
	TSession Session;                        // 0x71c478-0x71c58b, Status.cpp's

private:
	int SkipFields{}; // 0x5c4340
	// 0x5c4344-0x5c4357, the patch transfer's state (PatchData, PatchWholeFile, PatchDataCount, PatchDataSize,
	// PatchFileNumber) is gone with the patcher since Patella-Modern issue #75 (decision D9).
	int CancelCount{}; // 0x5c4378
	int LastCancelZ{}; // 0x5c437c
	int LastCancelY{}; // 0x5c4380
	int LastCancelX{}; // 0x5c4384

	TNetwork &Network;
	// TMapState's objects (issue #106).
	TObjectTypes &ObjectTypes;
	TMap &Map;
	TCreatures &Creatures;
	TPlayerState &PlayerState;
	TInventory &Inventory;
	TLoginSignatures &Signatures; // issue #110

	// LastPacketTypes a std::array (issue #60).
	static_assert(sizeof(LastPacketTypes) == 10, "LastPacketTypes");
};

// The transition's access path (issue #105; notes/decisions.md, "#104: TClient and the subsystem objects"): the
// communication of the one TClient (Client.cpp). It goes when no free function needs it.
namespace bridge {
TCommunication &Communication();
} // namespace bridge

} // namespace patella

// The original's functions (the callers' names, and the golden tests'): each calls TCommunication's member of the
// same name on bridge::Communication().
void InitCommunication();
void ExitCommunication(); // empty, folded into 0x496160
void SendLogin(uint32 AccountID, const char *Password);
void SendGameLogin(uint32 AccountID, const char *Name, const char *Password);
void SendLogout();
void SendPing();
void PrepareMovement();
void SendWalk();
void SendStop();
void SendTurnNorth();
void SendTurnEast();
void SendTurnSouth();
void SendTurnWest();
void SendMoveObject(int x, int y, int z, int ObjectType, int Position, int DestinationX, int DestinationY, int DestinationZ, int Count);
void SendRequestTrade(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID);
void SendInspectTrade(int Side, int Position);
void SendAcceptTrade();
void SendRejectTrade();
void SendUseObject(int x, int y, int z, int ObjectType, int Position, int Container);
void SendUseObjectWith(int x1, int y1, int z1, int ObjectType1, int Position1, int x2, int y2, int z2, int ObjectType2, int Position2);
void SendUseObjectOnCreature(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID);
void SendRotateObject(int x, int y, int z, int ObjectType, int Position);
void SendCloseContainer(int Container);
void SendContainerUp(int Container);
void SendEditText(uint32 ObjectNumber, const char *Text);
void SendEditHouseText(int Type, uint32 Identifier, const char *Text);
void SendLook(int x, int y, int z, int ObjectType, int Position);
void SendTalk(int Mode, const char *Text);
void SendTalk(int Mode, const char *Addressee, const char *Text);
void SendTalk(int Mode, int ChannelID, const char *Text);
void SendRequestChannels();
void SendJoinChannel(int ChannelID);
void SendLeaveChannel(int ChannelID);
void SendOpenPrivateConversation(const char *Name);
void SendProcessRuleViolation(const char *Name);
void SendCloseRuleViolation(const char *Name);
void SendCancelRuleViolation();
void SendFightModes();
void SendAttack(uint32 CreatureID);
void SendFollow(uint32 CreatureID);
void SendOpenPrivateChannel();
void SendInviteToPrivateChannel(const char *Name);
void SendExcludeFromPrivateChannel(const char *Name);
void SendInviteToParty(uint32 CharacterID);
void SendJoinParty(uint32 CharacterID);
void SendRevokePartyInvitation(uint32 CharacterID);
void SendPassPartyLeadership(uint32 CharacterID);
void SendLeaveParty();
void SendCancel();
void SendRefreshField(int x, int y, int z);
void SendRefreshContainer(int Container);
void SendRequestOutfit();
void SendOutfit(int OutfitType, const int *Colors);
void SendAddBuddy(const char *Name);
void SendRemoveBuddy(uint32 CharacterID);
void SendBugReport(const char *Text);
void SendRuleViolationReport(const char *Name, uint8 Reason, uint8 Action, const char *Comment, uint32 StatementID, bool IPBanishment);
void ReceiveLoginError();
void ReceiveLoginMessage();
void ReceiveMessageOfTheDay();
void ReceiveUpdateRequest(); // packets 0x1e, 0x1f and 0x20 (the patcher removed: issue #75)
void ReceiveCharacterList();
void ReceiveLoginServerOverloaded();
void ReceiveGameLogin();
void ReceiveGamemasterActions();
void ReceiveGameError();
void ReceiveGameMessage();
void ReceiveLoginWait();
void ReceivePing();
uint32 ReadCreature(int Type, int x, int y, int z);
void ReadField(int rx, int ry, int rz);
void ReadMap(int xmin, int ymin, int xmax, int ymax);
void ReceiveMap();
void ReceiveMapNorth();
void ReceiveMapEast();
void ReceiveMapSouth();
void ReceiveMapWest();
void ReceiveField();
void ReceiveAddObject();
void ReceiveChangeObject();
void ReceiveDeleteObject();
void ReceiveMoveCreature();
void ReceiveOpenContainer(int &Container);
void ReceiveCloseContainer(int &Container);
void ReceiveContainerAddObject();
void ReceiveContainerChangeObject();
void ReceiveContainerDeleteObject();
void ReceiveInventoryObject();
void ReceiveDeleteInventoryObject();
void ReceiveOwnTrade();
void ReceiveCounterTrade();
void ReceiveCloseTrade();
void ReceiveWorldLight();
void ReceiveMagicEffect();
void ReceiveAnimatedText();
void ReceiveDistanceEffect();
void ReceiveCreatureMark();
void ReceiveCreatureHealth();
void ReceiveCreatureLight();
void ReceiveCreatureOutfit();
void ReceiveCreatureSpeed();
void ReceiveCreatureSkull();
void ReceiveCreatureShield();
void ReceiveEditText();
void ReceiveEditHouseText();
void ReceivePlayerStats();
void ReceivePlayerSkills();
void ReceivePlayerIcons();
void ReceiveCancelAttack();
void ReceiveTalk();
void ReceiveChannelList();
void ReceiveOpenChannel();
void ReceiveOpenPrivateConversation();
void ReceiveRuleViolationChannel();
void ReceiveRemoveRuleViolation();
void ReceiveCancelRuleViolation();
void ReceiveLockRuleViolation();
void ReceiveOwnChannel();
void ReceiveCloseChannel();
void ReceiveTextMessage();
void ReceiveCancelWalk();
void ReadFloor(int rz);
void ReceiveFloorUp();
void ReceiveFloorDown();
void ReceiveOutfit();
void ReceiveBuddy();
void ReceiveBuddyLogin();
void ReceiveBuddyLogout();
