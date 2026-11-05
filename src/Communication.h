// Communication.cpp: Tibia 7.72 protocol senders and receive handlers.
// Parameter names and assertion identifiers follow the binary; remaining names are inferred.

#pragma once

#include "common.h"
// Nothing of Talk.h is used here, but without it the objects that include this header declare its names
// later, and MSVC, which orders an object's globals by the names its translation unit declares (issue #43),
// emits Communication.obj's .bss in another order (OutfitRequested after the ATL statics): the images' .data
// moves (measured on x64, issue #49).
#include "Talk.h" // IWYU pragma: keep

extern bool OutfitRequested; // 0x5c4358; also reset by Control.cpp

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
void ReceivePatch();
void ReceiveWholeFile();
void ReceivePatchBlock();
void ReceiveCharacterList();
void CancelPatch();
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
