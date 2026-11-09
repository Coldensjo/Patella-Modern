// Communication.cpp: reconstructed from Tibia.exe 7.72 (0x401000, 0x404350-0x417270).
// The client-to-server packets (Send*) and the handlers of the server's
// packets (Receive*), which ReceivePackets in Control.cpp dispatches.
// Functions are in original source order from the assertion and catch line
// numbers; see notes/Communication.md for the details of this file.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none
// apart from the handlers of their own: the encryption failures of SendLogin
// and SendGameLogin (TRSA's const char *) and ReceiveMoveCreature's cylinder
// loop (an error while listing the fields goes into the message).

#include "Communication.h"
#include "BuildConfig.h"
#include "Help.h"
#include "Inventory.h"
#include "Creatures.h"
#include "Map.h"
#include "MapState.h"
#include "Network.h"
#include "Objects.h"
#include "Utils.h"
#include "TRSA.h"
#include "Preferences.h"
#include "Status.h"
#include "Talk.h"
#include "Text.h"
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

// The data is TCommunication's (Communication.h, issue #105), in the original's order with its addresses.

namespace {

// The login server's request to update the client (issue #75): the patch packets end the login with this text in the
// login error dialog.
constexpr std::string_view TextUpdateRefused = "This server wants to update the client";

} // namespace

patella::TCommunication::TCommunication(TNetwork &NetworkRef, TMapState &MapState, TLoginSignatures &SignaturesRef)
	: Network(NetworkRef), ObjectTypes(MapState.ObjectTypes), Map(MapState.Map), Creatures(MapState.Creatures),
	  PlayerState(MapState.PlayerState), Inventory(MapState.Inventory), Signatures(SignaturesRef)
{
}

static_assert(offsetof(TCreature, Direction) == 0x50, "creature direction");
static_assert(offsetof(TCreature, OutfitType) == 0x60, "creature outfit");
static_assert(offsetof(TCreature, PKFlag) == 0x90, "creature skull");

// Not in this file's source: the linker placed VC7.1 STL template code that
// this file instantiates (through std::stringstream in SendLogin and
// SendGameLogin) and two compiler-generated TMyException members in the
// address range. They are listed in names.tsv as kind "stl":
//   0x412a60 stringstream::str, 0x412a80 num_put::do_put(double),
//   0x412be0 num_put::_Ffmt, 0x412c40 num_put::_Fput,
//   0x4131f0 string::append(count, char), 0x4132c0 string::insert(pos, count, char),
//   0x4133e0 string::append(ptr, count), 0x4134f0 string::append(string, pos, count),
//   0x4135e0 num_put::do_put(long double),
//   0x4044f0 TMyException copy constructor, 0x404570 its scalar deleting destructor.
// The same holds for 0x401010-0x404341 before the file's other functions
// (string, stringstream and locale code; names.tsv rows std_<address>).

// 0x401000, the first function of the image (no line evidence). Called by
// InitControl. The original cleared the patch transfer's buffer; since issue #75 (the patcher removed) there is
// nothing left to set up.
void patella::TCommunication::InitCommunication()
{
}

// 0x413780, lines 41-96
void patella::TCommunication::SendLogin(uint32 AccountID, const char *Password)
{
	PATELLA_ASSERT(Password!=nullptr);
	std::array<uint8, 16> Key;
	for (int i = 0; i < 16; i++)
		Key[i] = (uint8)rand();
	Network.SetSymmetricKey(Key.data());
	std::array<uint8, 128> Data;
	for (int i = 0; i < 128; i++)
		Data[i] = (uint8)rand();
	TWriteBuffer Buffer(Data);
	Buffer.PutByte(0);
	Buffer.PutBytes(Key);
	Buffer.PutQuad(AccountID);
	Buffer.PutString(Password);
	// Raw RSA, no padding: the protocol's block is the plain text (a zero byte, the XTEA key, the login data,
	// rand()'s filler) raised to 65537 modulo the key, as the server expects (notes/decisions.md, "#128").
	PATELLA_TEST_RSA_BLOCK(Data)
	try {
		TRSA RSA;
		RSA.SetPublicKey(PATELLA_TEST_RSA_MODULUS(patella::buildconfig::RsaModulus));
		RSA.Encrypt(Data);
	} catch (const char *Error) {
		std::stringstream Text;
		Text << "Asymmetric encryption for login server failed";
		for (int i = 0; i < 128; i++) {
			if (i % 16 == 0)
				Text << std::endl;
			TFixedString<20> Byte;
			Byte.Format("{:03} ", (unsigned)Data[i]);
			Text << Byte.View();
		}
		Text << std::endl;
		throw TMYEXCEPTION(Text.str().c_str(), "", 0, "", 0, Error);
	} catch (...) {
		std::stringstream Text;
		Text << "Asymmetric encryption for login server failed with unknown exception";
		for (int i = 0; i < 128; i++) {
			if (i % 16 == 0)
				Text << std::endl;
			TFixedString<20> Byte;
			Byte.Format("{:03} ", (unsigned)Data[i]);
			Text << Byte.View();
		}
		// No std::endl after the bytes here, unlike the first catch block (the
		// binary goes from the loop to str(), issue #36).
		TERROR(Text.str().c_str());
	}
	Network.BeginCommand(1);
	Network.SendWord(2);
	Network.SendWord(772);
	Network.SendQuad(ObjectTypes.DatSignature);
	Network.SendQuad(Signatures.SpriteSignature());
	Network.SendQuad(Signatures.PictureSignature());
	Network.SendBytes(Data);
	Network.SendCommand(false);
}

// 0x413de0, lines 109-166
void patella::TCommunication::SendGameLogin(uint32 AccountID, const char *Name, const char *Password)
{
	PATELLA_ASSERT(Name!=nullptr);
	PATELLA_ASSERT(Password!=nullptr);
	std::array<uint8, 16> Key;
	for (int i = 0; i < 16; i++)
		Key[i] = (uint8)rand();
	Network.SetSymmetricKey(Key.data());
	std::array<uint8, 128> Data;
	for (int i = 0; i < 128; i++)
		Data[i] = (uint8)rand();
	TWriteBuffer Buffer(Data);
	Buffer.PutByte(0);
	Buffer.PutBytes(Key);
	Buffer.PutByte(Session.GetGamemaster() != 0);
	Buffer.PutQuad(AccountID);
	Buffer.PutString(Name);
	Buffer.PutString(Password);
	// Raw RSA, no padding, as in SendLogin (notes/decisions.md, "#128").
	PATELLA_TEST_RSA_BLOCK(Data)
	try {
		TRSA RSA;
		RSA.SetPublicKey(PATELLA_TEST_RSA_MODULUS(patella::buildconfig::RsaModulus));
		RSA.Encrypt(Data);
	} catch (const char *Error) {
		std::stringstream Text;
		Text << "Asymmetric encryption for game server failed";
		for (int i = 0; i < 128; i++) {
			if (i % 16 == 0)
				Text << std::endl;
			TFixedString<20> Byte;
			Byte.Format("{:03} ", (unsigned)Data[i]);
			Text << Byte.View();
		}
		Text << std::endl;
		throw TMYEXCEPTION(Text.str().c_str(), "", 0, "", 0, Error);
	} catch (...) {
		std::stringstream Text;
		Text << "Asymmetric encryption for game server failed with unknown exception";
		for (int i = 0; i < 128; i++) {
			if (i % 16 == 0)
				Text << std::endl;
			TFixedString<20> Byte;
			Byte.Format("{:03} ", (unsigned)Data[i]);
			Text << Byte.View();
		}
		// No std::endl after the bytes here, unlike the first catch block (the
		// binary goes from the loop to str(), issue #36).
		TERROR(Text.str().c_str());
	}
	Network.BeginCommand(10);
	Network.SendWord(2);
	Network.SendWord(772);
	Network.SendBytes(Data);
	Network.SendCommand(false);
}

// 0x404350, lines 178-178
// The body is in the leading gap, but its Communication.cpp catch identifies source line 178.
void patella::TCommunication::SendLogout()
{
	Network.BeginCommand(0x14);
	Network.SendCommand(true);
}

// 0x404590, lines 190-190
void patella::TCommunication::SendPing()
{
	Network.BeginCommand(0x1e);
	Network.SendCommand(true);
}

// 0x414530, lines 208-208
void patella::TCommunication::PrepareMovement()
{
	if (GetAutoChaseOff()) {
		if (Creatures.GetAttackID() != 0) {
			if (GetChaseMode() != 0) {
				SetChaseMode(0);
				SendFightModes();
			}
		}
	}
	if (Creatures.GetFollowID() != 0) {
		Creatures.SetFollowID(0);
		SendFollow(0);
	}
}

// 0x414700, lines 219-238
// An invalid single-step direction still sends the existing command buffer.
void patella::TCommunication::SendWalk()
{
	PrepareMovement();
	const int Steps = PlayerState.GetNumberOfPlayerSteps();
	PATELLA_ASSERT_VALUES(Steps>0, Steps);
	if (Steps == 1) {
		switch (PlayerState.GetPlayerStep(0)) {
		case 1: Network.BeginCommand(0x66); break;
		case 2: Network.BeginCommand(0x6a); break;
		case 3: Network.BeginCommand(0x65); break;
		case 4: Network.BeginCommand(0x6d); break;
		case 5: Network.BeginCommand(0x68); break;
		case 6: Network.BeginCommand(0x6c); break;
		case 7: Network.BeginCommand(0x67); break;
		case 8: Network.BeginCommand(0x6b); break;
		default: break; // another step sends nothing, as without it (bugprone-switch-missing-default-case, issue #110)
		}
	} else {
		Network.BeginCommand(0x64);
		Network.SendByte(Steps);
		for (int i = 0; i < Steps; i++)
			Network.SendByte(PlayerState.GetPlayerStep(i));
	}
	Network.SendCommand(true);
}

// 0x414a10, lines 251-251
void patella::TCommunication::SendStop()
{
	PrepareMovement();
	Network.BeginCommand(0x69);
	Network.SendCommand(true);
}

// 0x404730, lines 263-263
void patella::TCommunication::SendTurnNorth()
{
	Network.BeginCommand(0x6f);
	Network.SendCommand(true);
}

// 0x4048d0, lines 275-275
void patella::TCommunication::SendTurnEast()
{
	Network.BeginCommand(0x70);
	Network.SendCommand(true);
}

// 0x404a70, lines 287-287
void patella::TCommunication::SendTurnSouth()
{
	Network.BeginCommand(0x71);
	Network.SendCommand(true);
}

// 0x404c10, lines 299-299
void patella::TCommunication::SendTurnWest()
{
	Network.BeginCommand(0x72);
	Network.SendCommand(true);
}

// 0x404db0, lines 325-325
void patella::TCommunication::SendMoveObject(int x, int y, int z, int ObjectType, int Position, int DestinationX, int DestinationY, int DestinationZ, int Count)
{
	if (ObjectTypes.GetObjectFlag(ObjectType, 13)) {
		SetStatusMessage("You cannot move this object.", false);
		return;
	}
	if (x != 0xffff)
		PlayerState.ClearPlayerSteps();
	Network.BeginCommand(0x78);
	Network.SendWord(x);
	Network.SendWord(y);
	Network.SendByte(z);
	Network.SendWord(ObjectType);
	Network.SendByte(Position);
	Network.SendWord(DestinationX);
	Network.SendWord(DestinationY);
	Network.SendByte(DestinationZ);
	Network.SendByte(Count);
	Network.SendCommand(true);
}

// 0x405000, lines 344-344
void patella::TCommunication::SendRequestTrade(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID)
{
	PlayerState.ClearPlayerSteps();
	Network.BeginCommand(0x7d);
	Network.SendWord(x);
	Network.SendWord(y);
	Network.SendByte(z);
	Network.SendWord(ObjectType);
	Network.SendByte(Position);
	Network.SendQuad(CreatureID);
	Network.SendCommand(true);
}

// 0x4051f0, lines 358-358
void patella::TCommunication::SendInspectTrade(int Side, int Position)
{
	Network.BeginCommand(0x7e);
	Network.SendByte(Side);
	Network.SendByte(Position);
	Network.SendCommand(true);
}

// 0x4053b0, lines 370-370
void patella::TCommunication::SendAcceptTrade()
{
	Network.BeginCommand(0x7f);
	Network.SendCommand(true);
}

// 0x405550, lines 382-382
void patella::TCommunication::SendRejectTrade()
{
	Network.BeginCommand(0x80);
	Network.SendCommand(true);
}

// 0x4056f0, lines 401-401
void patella::TCommunication::SendUseObject(int x, int y, int z, int ObjectType, int Position, int Container)
{
	if (x != 0xffff)
		PlayerState.ClearPlayerSteps();
	Network.BeginCommand(0x82);
	Network.SendWord(x);
	Network.SendWord(y);
	Network.SendByte(z);
	Network.SendWord(ObjectType);
	Network.SendByte(Position);
	Network.SendByte(Container);
	Network.SendCommand(true);
}

// 0x4058f0, lines 424-424
void patella::TCommunication::SendUseObjectWith(int x1, int y1, int z1, int ObjectType1, int Position1, int x2, int y2, int z2, int ObjectType2, int Position2)
{
	if (x1 != 0xffff || x2 != 0xffff)
		PlayerState.ClearPlayerSteps();
	Network.BeginCommand(0x83);
	Network.SendWord(x1);
	Network.SendWord(y1);
	Network.SendByte(z1);
	Network.SendWord(ObjectType1);
	Network.SendByte(Position1);
	Network.SendWord(x2);
	Network.SendWord(y2);
	Network.SendByte(z2);
	Network.SendWord(ObjectType2);
	Network.SendByte(Position2);
	Network.SendCommand(true);
}

// 0x405b20, lines 443-443
void patella::TCommunication::SendUseObjectOnCreature(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID)
{
	PlayerState.ClearPlayerSteps();
	Network.BeginCommand(0x84);
	Network.SendWord(x);
	Network.SendWord(y);
	Network.SendByte(z);
	Network.SendWord(ObjectType);
	Network.SendByte(Position);
	Network.SendQuad(CreatureID);
	Network.SendCommand(true);
}

// 0x405d10, lines 460-460
void patella::TCommunication::SendRotateObject(int x, int y, int z, int ObjectType, int Position)
{
	Network.BeginCommand(0x85);
	Network.SendWord(x);
	Network.SendWord(y);
	Network.SendByte(z);
	Network.SendWord(ObjectType);
	Network.SendByte(Position);
	Network.SendCommand(true);
}

// 0x405ef0, lines 473-473
void patella::TCommunication::SendCloseContainer(int Container)
{
	Network.BeginCommand(0x87);
	Network.SendByte(Container);
	Network.SendCommand(true);
}

// 0x4060a0, lines 486-486
void patella::TCommunication::SendContainerUp(int Container)
{
	Network.BeginCommand(0x88);
	Network.SendByte(Container);
	Network.SendCommand(true);
}

// 0x406250, lines 500-500
void patella::TCommunication::SendEditText(uint32 ObjectNumber, const char *Text)
{
	Network.BeginCommand(0x89);
	Network.SendQuad(ObjectNumber);
	Network.SendString(Text);
	Network.SendCommand(true);
}

// 0x406410, lines 515-515
void patella::TCommunication::SendEditHouseText(int Type, uint32 Identifier, const char *Text)
{
	Network.BeginCommand(0x8a);
	Network.SendByte(Type);
	Network.SendQuad(Identifier);
	Network.SendString(Text);
	Network.SendCommand(true);
}

// 0x4065e0, lines 532-532
void patella::TCommunication::SendLook(int x, int y, int z, int ObjectType, int Position)
{
	Network.BeginCommand(0x8c);
	Network.SendWord(x);
	Network.SendWord(y);
	Network.SendByte(z);
	Network.SendWord(ObjectType);
	Network.SendByte(Position);
	Network.SendCommand(true);
}

// 0x4067c0, lines 541-549
void patella::TCommunication::SendTalk(int Mode, const char *Text)
{
	PATELLA_ASSERT_VALUES(Mode==TALK_SAY || Mode==TALK_WHISPER || Mode==TALK_YELL || Mode==TALK_GAMEMASTER_REQUEST || Mode==TALK_PLAYER_ANSWER || Mode==TALK_GAMEMASTER_BROADCAST || Mode==TALK_ANONYMOUS_BROADCAST, Mode);
	PATELLA_ASSERT(Text!=nullptr);
	Network.BeginCommand(0x96);
	Network.SendByte(Mode);
	Network.SendString(Text);
	Network.SendCommand(true);
}

// 0x406b40, lines 557-567
void patella::TCommunication::SendTalk(int Mode, const char *Addressee, const char *Text)
{
	PATELLA_ASSERT_VALUES(Mode==TALK_PRIVATE_MESSAGE || Mode==TALK_GAMEMASTER_ANSWER || Mode==TALK_GAMEMASTER_MESSAGE || Mode==TALK_ANONYMOUS_MESSAGE, Mode);
	PATELLA_ASSERT(Addressee!=nullptr);
	PATELLA_ASSERT(Text!=nullptr);
	Network.BeginCommand(0x96);
	Network.SendByte(Mode);
	Network.SendString(Addressee);
	Network.SendString(Text);
	Network.SendCommand(true);
}

// 0x406f90, lines 575-584
void patella::TCommunication::SendTalk(int Mode, int ChannelID, const char *Text)
{
	PATELLA_ASSERT_VALUES(Mode==TALK_CHANNEL_CALL || Mode==TALK_GAMEMASTER_CHANNELCALL || Mode==TALK_ANONYMOUS_CHANNELCALL, Mode);
	PATELLA_ASSERT(Text!=nullptr);
	Network.BeginCommand(0x96);
	Network.SendByte(Mode);
	Network.SendWord(ChannelID);
	Network.SendString(Text);
	Network.SendCommand(true);
}

// 0x407300, lines 596-596
void patella::TCommunication::SendRequestChannels()
{
	Network.BeginCommand(0x97);
	Network.SendCommand(true);
}

// 0x4074a0, lines 609-609
void patella::TCommunication::SendJoinChannel(int ChannelID)
{
	Network.BeginCommand(0x98);
	Network.SendWord(ChannelID);
	Network.SendCommand(true);
}

// 0x407650, lines 622-622
void patella::TCommunication::SendLeaveChannel(int ChannelID)
{
	Network.BeginCommand(0x99);
	Network.SendWord(ChannelID);
	Network.SendCommand(true);
}

// 0x407800, lines 635-635
void patella::TCommunication::SendOpenPrivateConversation(const char *Name)
{
	Network.BeginCommand(0x9a);
	Network.SendString(Name);
	Network.SendCommand(true);
}

// 0x4079b0, lines 648-648
void patella::TCommunication::SendProcessRuleViolation(const char *Name)
{
	Network.BeginCommand(0x9b);
	Network.SendString(Name);
	Network.SendCommand(true);
}

// 0x407b60, lines 661-661
void patella::TCommunication::SendCloseRuleViolation(const char *Name)
{
	Network.BeginCommand(0x9c);
	Network.SendString(Name);
	Network.SendCommand(true);
}

// 0x407d10, lines 673-673
void patella::TCommunication::SendCancelRuleViolation()
{
	Network.BeginCommand(0x9d);
	Network.SendCommand(true);
}

// 0x407eb0, lines 689-689
void patella::TCommunication::SendFightModes()
{
	Network.BeginCommand(0xa0);
	Network.SendByte(GetAttackMode());
	Network.SendByte(GetChaseMode());
	Network.SendByte(GetSecureMode());
	Network.SendCommand(true);
}

// 0x408080, lines 703-703
void patella::TCommunication::SendAttack(uint32 CreatureID)
{
	if (CreatureID != 0)
		PlayerState.ClearPlayerSteps();
	Network.BeginCommand(0xa1);
	Network.SendQuad(CreatureID);
	Network.SendCommand(true);
}

// 0x408230, lines 717-717
void patella::TCommunication::SendFollow(uint32 CreatureID)
{
	if (CreatureID != 0)
		PlayerState.ClearPlayerSteps();
	Network.BeginCommand(0xa2);
	Network.SendQuad(CreatureID);
	Network.SendCommand(true);
}

// 0x4083e0, lines 729-729
void patella::TCommunication::SendOpenPrivateChannel()
{
	Network.BeginCommand(0xaa);
	Network.SendCommand(true);
}

// 0x408580, lines 742-742
void patella::TCommunication::SendInviteToPrivateChannel(const char *Name)
{
	Network.BeginCommand(0xab);
	Network.SendString(Name);
	Network.SendCommand(true);
}

// 0x408730, lines 755-755
void patella::TCommunication::SendExcludeFromPrivateChannel(const char *Name)
{
	Network.BeginCommand(0xac);
	Network.SendString(Name);
	Network.SendCommand(true);
}

// 0x4088e0, lines 768-768
void patella::TCommunication::SendInviteToParty(uint32 CharacterID)
{
	Network.BeginCommand(0xa3);
	Network.SendQuad(CharacterID);
	Network.SendCommand(true);
}

// 0x408a90, lines 781-781
void patella::TCommunication::SendJoinParty(uint32 CharacterID)
{
	Network.BeginCommand(0xa4);
	Network.SendQuad(CharacterID);
	Network.SendCommand(true);
}

// 0x408c40, lines 794-794
void patella::TCommunication::SendRevokePartyInvitation(uint32 CharacterID)
{
	Network.BeginCommand(0xa5);
	Network.SendQuad(CharacterID);
	Network.SendCommand(true);
}

// 0x408df0, lines 807-807
void patella::TCommunication::SendPassPartyLeadership(uint32 CharacterID)
{
	Network.BeginCommand(0xa6);
	Network.SendQuad(CharacterID);
	Network.SendCommand(true);
}

// 0x408fa0, lines 818-818
void patella::TCommunication::SendLeaveParty()
{
	Network.BeginCommand(0xa7);
	Network.SendCommand(true);
}

// 0x409140, lines 830-830
void patella::TCommunication::SendCancel()
{
	Network.BeginCommand(0xbe);
	Network.SendCommand(true);
}

// 0x4092e0, lines 847-847
void patella::TCommunication::SendRefreshField(int x, int y, int z)
{
	Network.BeginCommand(0xc9);
	Network.SendWord(x);
	Network.SendWord(y);
	Network.SendByte(z);
	Network.SendCommand(true);
	Map.AbsoluteToRelative(&x, &y, &z);
	Map.RequestFieldRefresh(x, y, z);
}

// 0x4094d0, lines 860-860
void patella::TCommunication::SendRefreshContainer(int Container)
{
	Network.BeginCommand(0xca);
	Network.SendByte(Container);
	Network.SendCommand(true);
}

// 0x409680, lines 875-875
// The request flag is set before sending and is not rolled back on an exception.
void patella::TCommunication::SendRequestOutfit()
{
	if (!OutfitRequested) {
		OutfitRequested = true;
		Network.BeginCommand(0xd2);
		Network.SendCommand(true);
	}
}

// 0x409830, lines 883-893
void patella::TCommunication::SendOutfit(int OutfitType, const int *Colors)
{
	PATELLA_ASSERT(Colors!=nullptr);
	Network.BeginCommand(0xd3);
	Network.SendWord(OutfitType);
	Network.SendByte(Colors[0]);
	Network.SendByte(Colors[1]);
	Network.SendByte(Colors[2]);
	Network.SendByte(Colors[3]);
	Network.SendCommand(true);
}

// 0x409b00, lines 902-908
void patella::TCommunication::SendAddBuddy(const char *Name)
{
	PATELLA_ASSERT(Name!=nullptr);
	Network.BeginCommand(0xdc);
	Network.SendString(Name);
	Network.SendCommand(true);
}

// 0x409d90, lines 921-921
void patella::TCommunication::SendRemoveBuddy(uint32 CharacterID)
{
	Network.BeginCommand(0xdd);
	Network.SendQuad(CharacterID);
	Network.SendCommand(true);
}

// 0x409f40, lines 934-934
void patella::TCommunication::SendBugReport(const char *Text)
{
	Network.BeginCommand(0xe6);
	Network.SendString(Text);
	Network.SendCommand(true);
}

// 0x40a0f0, lines 953-953
void patella::TCommunication::SendRuleViolationReport(const char *Name, uint8 Reason, uint8 Action, const char *Comment, uint32 StatementID, bool IPBanishment)
{
	Network.BeginCommand(0xe7);
	Network.SendString(Name);
	Network.SendByte(Reason);
	Network.SendByte(Action);
	Network.SendString(Comment);
	Network.SendQuad(StatementID);
	Network.SendByte(IPBanishment);
	Network.SendCommand(true);
}

// 0x40a2f0, line 969, SendDebugReport (client packet 0xe8: an error report of error.txt) is gone since
// Patella-Modern issue #74 (decision D2).

// 0x40a4c0, lines 994-994
void patella::TCommunication::ReceiveLoginError()
{
	const char *Text = Network.GetString();
	AddMessage(1, reinterpret_cast<std::intptr_t>("Sorry"), reinterpret_cast<std::intptr_t>(Text));
	Network.Disconnect(false);
	Session.SetConnectionState(0);
}

// 0x40a670, lines 1006-1006
void patella::TCommunication::ReceiveLoginMessage()
{
	const char *Text = Network.GetString();
	AddMessage(1, reinterpret_cast<std::intptr_t>("For Your Information"), reinterpret_cast<std::intptr_t>(Text));
}

// 0x40a810, lines 1020-1020
void patella::TCommunication::ReceiveMessageOfTheDay()
{
	const char *Text = Network.GetString();
	if (SetMotD(Text))
		AddMessage(0x28, 0, 0);
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): GetString's text, the caller's (notes/ownership.md)
	delete[] Text;
}

// 0x40a9c0 ReceivePatch (lines 1033-1045, packet 0x1e: a difference patch of a file), 0x40ae70 ReceiveWholeFile
// (1061-1073, 0x1f: a whole file) and 0x40b290 ReceivePatchBlock (1083-1135, 0x20: a block of either) are gone since
// Patella-Modern issue #75 (decision D9): with them a login server could rewrite Tibia.exe (as Tibia.bak), Tibia.dat,
// Tibia.spr, Tibia.pic and Patch.exe, and have Patch.exe run. ReceiveUpdateRequest takes all three: the login ends as
// a login error does, in the same dialog ("Sorry"), with TextUpdateRefused. The rest of what the server sent (the
// patch's header or block, the packets after it in this frame and in the frames already received) is read and
// dropped, so that nothing of it reaches the game server's table once the connection state is 0.
void patella::TCommunication::ReceiveUpdateRequest()
{
	while (Network.GetNextCommand() != -1) {
	}
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	char *Text = new char[TextUpdateRefused.size() + 1];
	memcpy(Text, TextUpdateRefused.data(), TextUpdateRefused.size());
	Text[TextUpdateRefused.size()] = 0;
	AddMessage(1, reinterpret_cast<std::intptr_t>("Sorry"), reinterpret_cast<std::intptr_t>(Text));
	Network.Disconnect(false);
	Session.SetConnectionState(0);
}

// 0x40b8b0, lines 1162-1162
void patella::TCommunication::ReceiveCharacterList()
{
	int Count = 0;
	Count = Network.GetByte();
	Session.SetAccountCharacters(Count);
	for (int i = 0; i < Count; i++) {
		TFixedString<40> Name;
		TFixedString<32> World;
		std::array<uint8, 8> Address;
		Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
		Network.GetString(patella::TSpan<char>(World.Chars).first(30));
		for (int j = 0; j < 4; j++)
			Address[j] = Network.GetByte();
		const int Port = Network.GetWord();
		Session.SetCharacter(i, Name, World, Address.data(), Port);
	}
	const int PremiumDays = Network.GetWord();
	Session.SetPremiumDays(PremiumDays);
	AddMessage(10, 300, 0);
}

// 0x40bac0, CancelPatch (line 1178: the update dialog's Cancel, the transfer dropped and the connection closed), is
// gone with the update dialog since Patella-Modern issue #75.

// 0x40bc80, lines 1192-1192
void patella::TCommunication::ReceiveLoginServerOverloaded()
{
	Network.SetConnectionTimeout(0);
	Network.Disconnect(false);
	Session.SetConnectionState(0);
	Network.SetServerError(ERROR_LOGINSERVER_OVERLOADED);
	Network.ConnectToLoginServer();
}

// 0x40be30, lines 1215-1215
void patella::TCommunication::ReceiveGameLogin()
{
	Session.ClearRights();
	const uint32 CreatureID = Network.GetQuad();
	PlayerState.SetPlayerID(CreatureID);
	const int Beat = Network.GetWord();
	Creatures.SetBeat(Beat);
	const uint8 value = Network.GetByte();
	Session.SetRight(40, value);
	SetActionState(0);
}

// 0x40c000, lines 1232-1232
void patella::TCommunication::ReceiveGamemasterActions()
{
	int i;
	uint8 value = 0;
	for (i = 0; i < 32; i++) {
		value = Network.GetByte();
		Session.SetRight(i, value);
	}
}

// 0x40c1b0, lines 1246-1246
void patella::TCommunication::ReceiveGameError()
{
	const char *Text = Network.GetString();
	AddMessage(1, reinterpret_cast<std::intptr_t>("Sorry"), reinterpret_cast<std::intptr_t>(Text));
	Network.Disconnect(false);
	Session.SetConnectionState(0);
}

// 0x40c360, lines 1258-1258
void patella::TCommunication::ReceiveGameMessage()
{
	const char *Text = Network.GetString();
	AddMessage(1, reinterpret_cast<std::intptr_t>("For Your Information"), reinterpret_cast<std::intptr_t>(Text));
}

// 0x40c500, lines 1271-1271
void patella::TCommunication::ReceiveLoginWait()
{
	const char *Text = Network.GetString();
	const int Delay = Network.GetByte();
	AddMessage(5, reinterpret_cast<std::intptr_t>(Text), Delay);
}

// 0x40c6a0, lines 1282-1282
void patella::TCommunication::ReceivePing()
{
	SendPing();
}

// 0x40c830, lines 1302-1368
// A missing known creature consumes a byte, not a word, for outfit type and skips no skull/shield bytes.
uint32 patella::TCommunication::ReadCreature(int Type, int x, int y, int z)
{
	uint32 CreatureID = 0;
	TCreature *Creature;
	if (Type == 0x61) {
		const uint32 RemoveID = Network.GetQuad();
		CreatureID = Network.GetQuad();
		Creature = Creatures.AddCreature(RemoveID, CreatureID);
		PATELLA_ASSERT_VALUES(Creature!=nullptr, CreatureID);
		Network.GetString(patella::TSpan<char>(Creature->Name.Chars).first(30));
		if (!Creatures.IsPlayer(CreatureID))
			CapitalizeString(Creature->Name);
		Creature->Health = Network.GetByte();
		Creature->Direction = Network.GetByte();
		Creature->OutfitType = Network.GetWord();
		if (Creature->OutfitType == 0) {
			Creature->Colors[0] = Network.GetWord();
		} else {
			Creature->Colors[0] = Network.GetByte();
			Creature->Colors[1] = Network.GetByte();
			Creature->Colors[2] = Network.GetByte();
			Creature->Colors[3] = Network.GetByte();
		}
		Creature->LightStrength = Network.GetByte();
		Creature->LightColor = Network.GetByte();
		Creature->Speed = Network.GetWord();
		const int Skull = Network.GetByte();
		Creature->SetSkull(Skull, false);
		Creatures.NotifySkull(Creature->PKFlag);
		Creature->PARTYFlag = Network.GetByte();
	} else if (Type == 0x62) {
		CreatureID = Network.GetQuad();
		Creature = Creatures.GetCreature(CreatureID);
		if (Creature == nullptr) {
			Network.GetByte();
			Network.GetByte();
			if (Network.GetByte() == 0) {
				Network.GetWord();
				Network.GetByte();
				Network.GetByte();
				Network.GetWord();
			} else {
				Network.GetByte();
				Network.GetByte();
				Network.GetByte();
				Network.GetByte();
				Network.GetByte();
				Network.GetByte();
				Network.GetWord();
			}
		} else {
			Creature->Health = Network.GetByte();
			Creature->Direction = Network.GetByte();
			Creature->OutfitType = Network.GetWord();
			if (Creature->OutfitType == 0) {
				Creature->Colors[0] = Network.GetWord();
			} else {
				Creature->Colors[0] = Network.GetByte();
				Creature->Colors[1] = Network.GetByte();
				Creature->Colors[2] = Network.GetByte();
				Creature->Colors[3] = Network.GetByte();
			}
			Creature->LightStrength = Network.GetByte();
			Creature->LightColor = Network.GetByte();
			Creature->Speed = Network.GetWord();
			const int Skull = Network.GetByte();
			Creature->SetSkull(Skull, false);
			Creatures.NotifySkull(Creature->PKFlag);
			Creature->PARTYFlag = Network.GetByte();
		}
	} else {
		if (Type != 0x63)
			return 0;
		CreatureID = Network.GetQuad();
		Creature = Creatures.GetCreature(CreatureID);
		if (Creature == nullptr)
			Network.GetByte();
		else
			Creature->Direction = Network.GetByte();
	}
	if (Creature != nullptr) {
		Map.RelativeToAbsolute(&x, &y, &z);
		Creature->SetPosition(x, y, z);
		Creature->Visible = true;
	}
	return CreatureID;
}

// 0x414bb0, lines 1401-1416. OldType only the original's catch wrapper read (issue #78).
void patella::TCommunication::ReadField(int rx, int ry, int rz)
{
	uint16 Type = 0;
	[[maybe_unused]] uint16 OldType = 0;
	if (SkipFields < 1) {
		for (;;) {
			OldType = Type;
			Type = Network.GetWord();
			if (Type >= 0xff00) {
				SkipFields = Type - 0xff00;
				break;
			}
			if (Type == 0) {
				std::stringstream Text;
				Text << "client recieved field with type 0 (" << rx << "," << ry << "," << rz << ")";
				TERROR(Text.str().c_str());
			}
			if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
				const uint32 CreatureID = ReadCreature(Type, rx, ry, rz);
				Map.AppendObject(rx, ry, rz, TObject{0x63, (int)CreatureID, 0});
			} else {
				int Data = 0;
				if (ObjectTypes.GetObjectFlag(Type, 5))
					Data = Network.GetByte();
				if (ObjectTypes.GetObjectFlag(Type, 11))
					Data = Network.GetByte();
				if (ObjectTypes.GetObjectFlag(Type, 10))
					Data = Network.GetByte();
				Map.AppendObject(rx, ry, rz, TObject{Type, Data, 0});
			}
		}
	} else {
		SkipFields--;
	}
	if (rz == Map.GetMapLevel())
		Map.UpdateAutomap(rx, ry);
}

// 0x414ff0, lines 1437-1437
void patella::TCommunication::ReadMap(int xmin, int ymin, int xmax, int ymax)
{
	int PlayerX;
	int PlayerY;
	int PlayerZ;
	Map.GetMapPosition(&PlayerX, &PlayerY, &PlayerZ);
	int Floor;
	int EndFloor;
	int Step;
	if (PlayerZ < 8) {
		Floor = 0;
		EndFloor = 8;
		Step = 1;
	} else {
		Floor = 4;
		EndFloor = PlayerZ - 14;
		if (EndFloor < -1)
			EndFloor = -1;
		Step = -1;
	}
	SkipFields = 0;
	for (; Floor != EndFloor; Floor += Step) {
		for (int x = xmin; x <= xmax; x++) {
			for (int y = ymin; y <= ymax; y++)
				ReadField(x, y, Floor);
		}
	}
}

// 0x415200, lines 1460-1460
void patella::TCommunication::ReceiveMap()
{
	uint16 PlayerX = 0;
	uint16 PlayerY = 0;
	PlayerX = Network.GetWord();
	PlayerY = Network.GetWord();
	const int PlayerZ = Network.GetByte();
	Map.SetMapPosition(PlayerX, PlayerY, PlayerZ);
	Map.ClearMap();
	Creatures.HideAllCreatures();
	ReadMap(0, 0, 17, 13);
	SetTalkChanged();
	PlayerState.StopPlayer();
	CheckEventHelpPosition(PlayerX, PlayerY, PlayerZ);
}

// 0x4153f0, lines 1476-1476
void patella::TCommunication::ReceiveMapNorth()
{
	Map.ShiftMap(0, 1);
	ReadMap(0, 0, 17, 0);
	SetTalkChanged();
	int x;
	int y;
	int z;
	Map.GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x4155c0, lines 1492-1492
void patella::TCommunication::ReceiveMapEast()
{
	Map.ShiftMap(-1, 0);
	ReadMap(17, 0, 17, 13);
	SetTalkChanged();
	int x;
	int y;
	int z;
	Map.GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x415790, lines 1508-1508
void patella::TCommunication::ReceiveMapSouth()
{
	Map.ShiftMap(0, -1);
	ReadMap(0, 13, 17, 13);
	SetTalkChanged();
	int x;
	int y;
	int z;
	Map.GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x415960, lines 1524-1524
void patella::TCommunication::ReceiveMapWest()
{
	Map.ShiftMap(1, 0);
	ReadMap(0, 0, 0, 13);
	SetTalkChanged();
	int x;
	int y;
	int z;
	Map.GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x415b30, lines 1546-1546
// ReadField already refreshes visible-floor lighting; this handler deliberately does it again.
void patella::TCommunication::ReceiveField()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = Network.GetWord();
	ry = Network.GetWord();
	rz = Network.GetByte();
	Map.AbsoluteToRelative(&rx, &ry, &rz);
	Map.ClearField(rx, ry, rz, false);
	SkipFields = 0;
	ReadField(rx, ry, rz);
	if (rz == Map.GetMapLevel())
		Map.UpdateAutomap(rx, ry);
}

// 0x40cd10, lines 1575-1575
void patella::TCommunication::ReceiveAddObject()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = Network.GetWord();
	ry = Network.GetWord();
	rz = Network.GetByte();
	Map.AbsoluteToRelative(&rx, &ry, &rz);
	const int Type = Network.GetWord();
	TObject Object;
	if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
		Object.Type = 0x63;
		Object.Data = ReadCreature(Type, rx, ry, rz);
		Object.Data2 = 0;
	} else {
		int Data = 0;
		if (ObjectTypes.GetObjectFlag(Type, 5))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 11))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 10))
			Data = Network.GetByte();
		Object.Type = Type;
		Object.Data = Data;
		Object.Data2 = 0;
	}
	Map.AddObject(rx, ry, rz, Object);
	if (rz == Map.GetMapLevel())
		Map.UpdateAutomap(rx, ry);
}

// 0x40cfa0, lines 1594-1630
void patella::TCommunication::ReceiveChangeObject()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = Network.GetWord();
	ry = Network.GetWord();
	rz = Network.GetByte();
	const int Position = Network.GetByte(); // read before the conversion (0x40cff3)
	Map.AbsoluteToRelative(&rx, &ry, &rz);
	const TObject OldObject = Map.GetMapObject(rx, ry, rz, Position);
	if (OldObject.Type == ObjectTypes.NoObject.Type) {
		PATELLA_ASSERT_VALUES(Map.FieldRefreshRequested(rx,ry,rz), rx, ry);
		const int Type = Network.GetWord();
		if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
			ReadCreature(Type, rx, ry, rz);
			return;
		}
		if (ObjectTypes.GetObjectFlag(Type, 5))
			Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 11))
			Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 10))
			Network.GetByte();
	} else {
		if (OldObject.Type == 0x63) {
			TCreature *Creature = Creatures.GetCreature(OldObject.Data);
			if (Creature != nullptr)
				Creature->Visible = false;
		}
		const int Type = Network.GetWord();
		TObject Object;
		if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
			Object.Type = 0x63;
			Object.Data = ReadCreature(Type, rx, ry, rz);
			Object.Data2 = 0;
		} else {
			int Data = 0;
			if (ObjectTypes.GetObjectFlag(Type, 5))
				Data = Network.GetByte();
			if (ObjectTypes.GetObjectFlag(Type, 11))
				Data = Network.GetByte();
			if (ObjectTypes.GetObjectFlag(Type, 10))
				Data = Network.GetByte();
			Object.Type = Type;
			Object.Data = Data;
			Object.Data2 = 0;
		}
		Map.ChangeObject(rx, ry, rz, Position, Object);
		if (rz == Map.GetMapLevel())
			Map.UpdateAutomap(rx, ry);
	}
}

// 0x40d440, lines 1649-1661
void patella::TCommunication::ReceiveDeleteObject()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = Network.GetWord();
	ry = Network.GetWord();
	rz = Network.GetByte();
	const int Position = Network.GetByte();
	Map.AbsoluteToRelative(&rx, &ry, &rz);
	const TObject Object = Map.GetMapObject(rx, ry, rz, Position);
	if (Object.Type == ObjectTypes.NoObject.Type) {
		PATELLA_ASSERT_VALUES(Map.FieldRefreshRequested(rx,ry,rz), rx, ry);
	} else {
		if (Object.Type == 0x63) {
			TCreature *Creature = Creatures.GetCreature(Object.Data);
			if (Creature != nullptr)
				Creature->Visible = false;
		}
		const bool Refresh = Map.DeleteObject(rx, ry, rz, Position);
		if (rz == Map.GetMapLevel())
			Map.UpdateAutomap(rx, ry);
		Map.RelativeToAbsolute(&rx, &ry, &rz);
		if (Refresh)
			SendRefreshField(rx, ry, rz);
	}
}

// 0x415d40, lines 1879-1895
// Diagnostics may follow creature mutation; a missing creature record still moves the map object.
void patella::TCommunication::ReceiveMoveCreature()
{
	int sx = Network.GetWord();
	int sy = Network.GetWord();
	int sz = Network.GetByte();
	const int Position = Network.GetByte();
	int zx = Network.GetWord();
	int zy = Network.GetWord();
	int zz = Network.GetByte();
	const int dx = zx - sx;
	const int dy = zy - sy;
	const int dz = zz - sz;
	const int abs_sx = sx;
	const int abs_sy = sy;
	const int abs_sz = sz;
	const int abs_zx = zx;
	const int abs_zy = zy;
	const int abs_zz = zz;
	const bool Teleport = dz != 0 || abs(dx) >= 2 || abs(dy) >= 2;
	Map.AbsoluteToRelative(&sx, &sy, &sz);
	Map.AbsoluteToRelative(&zx, &zy, &zz);
	const TObject Object = Map.GetMapObject(sx, sy, sz, Position);
	std::string Error;
	TCreature *Creature = nullptr;
	if (Object.Type == ObjectTypes.NoObject.Type || Object.Type != 0x63) {
		if (Map.FieldRefreshRequested(sx, sy, sz)) {
			Map.RelativeToAbsolute(&zx, &zy, &zz);
			SendRefreshField(zx, zy, zz);
			return;
		}
		std::stringstream Text;
		Text << "MoveCreature has been received for a coordinate where there is no creature anymore [bug0000017]";
		if (Object.Type != ObjectTypes.NoObject.Type) {
			if (Object.Type == 0x61)
				Text << " (UNKNOWNCREATURE, ";
			else if (Object.Type == 0x62)
				Text << " (OUTDATEDCREATURE, ";
			else if (Object.Type == 0x63)
				Text << " (CREATURE, ";
			else
				Text << " (" << Object.Type << ", ";
			Text << Object.Data << ")";
		}
		Error = Text.str();
	} else {
		Creature = Creatures.GetCreature(Object.Data);
		if (Creature != nullptr) {
			if (Teleport) {
				if (Creature->CreatureID == PlayerState.GetPlayerID())
					PlayerState.StopPlayer();
				if (dx > 0)
					Creature->Direction = 1;
				else if (dx < 0)
					Creature->Direction = 3;
				else if (dy < 0)
					Creature->Direction = 0;
				else if (dy > 0)
					Creature->Direction = 2;
				if (Creature->CreatureID != PlayerState.GetPlayerID())
					Creature->StartMovement(0, 0, 0);
			} else {
				const TObject Bank = Map.GetFirstObject(zx, zy, zz);
				if (ObjectTypes.GetObjectFlag(Bank.Type, 0))
					Creature->StartMovement(dx, dy, ObjectTypes.GetObjectWaypoints(Bank.Type));
				else
					Error = "MoveCreature has been received for a coordinate where there is no bank as first object [bug0000484]";
			}
			Creature->RealX += dx;
			Creature->RealY += dy;
			Creature->RealZ += dz;
			Creatures.CheckMonsterHint(Creature);
		}
	}
	if (!Error.empty()) {
		std::stringstream Text;
		Text << " sx: " << sx << " sy: " << sy << " sz: " << sz << std::endl;
		Text << " Position: " << Position << std::endl;
		Text << " zx: " << zx << " zy: " << zy << " zz: " << zz << std::endl;
		Text << " dx: " << dx << " dy: " << dy << " dz: " << dz << std::endl;
		Text << " abs_sx: " << abs_sx << " abs_sy: " << abs_sy << " abs_sz: " << abs_sz << std::endl;
		Text << " abs_zx: " << abs_zx << " abs_zy: " << abs_zy << " abs_zz: " << abs_zz << std::endl;
		Text << " Start Cylinder (1): ";
		try {
			TObject Current = Map.GetFirstObject(sx, sy, sz);
			while (Current.Type != 0) {
				Text << " (" << Current.Type << "," << Current.Data << ")";
				Current = Map.GetNextObject();
			}
			Text << std::endl
				 << " Start Cylinder (2): ";
			for (int i = 0; i < 10; i++) {
				Current = Map.GetMapObject(sx, sy, sz, i);
				if (Current.Type == ObjectTypes.NoObject.Type)
					Text << "(-)";
				else
					Text << " (" << Current.Type << "," << Current.Data << ")";
			}
			Text << std::endl
				 << " Destination Cylinder (1): ";
			Current = Map.GetFirstObject(zx, zy, zz);
			while (Current.Type != 0) {
				Text << " (" << Current.Type << "," << Current.Data << ")";
				Current = Map.GetNextObject();
			}
			Text << std::endl
				 << " Destination Cylinder (2): ";
			for (int i = 0; i < 10; i++) {
				Current = Map.GetMapObject(zx, zy, zz, i);
				if (Current.Type == ObjectTypes.NoObject.Type)
					Text << "(-)";
				else
					Text << " (" << Current.Type << "," << Current.Data << ")";
			}
		} catch (TMyException &Exception) {
			Text << std::endl
				 << "Exception during cylinder looping: " << std::endl
				 << Exception.GetMessage();
		}
		if (Creature != nullptr) {
			Text << std::endl
				 << " ID: " << Creature->CreatureID << " Name: " << Creature->Name.Chars;
			Text << std::endl
				 << " RealX: " << Creature->RealX << " RealY: " << Creature->RealY << " RealZ: " << Creature->RealZ;
			Text << std::endl
				 << " DeltaX: " << Creature->DeltaX << " DeltaY: " << Creature->DeltaY;
		}
		throw TMYEXCEPTION(Error.c_str(), "", 0, "", 0, Text.str().c_str());
	}
	if (Map.DeleteObject(sx, sy, sz, Position)) {
		int x = sx;
		int y = sy;
		int z = sz;
		Map.RelativeToAbsolute(&x, &y, &z);
		SendRefreshField(x, y, z);
	}
	if (sz == Map.GetMapLevel())
		Map.UpdateAutomap(sx, sy);
	Map.AddObject(zx, zy, zz, Object);
	if (zz == Map.GetMapLevel())
		Map.UpdateAutomap(zx, zy);
}

// 0x40d7d0, lines 1922-1922
void patella::TCommunication::ReceiveOpenContainer(int &Container)
{
	Container = Network.GetByte();
	if (Inventory.IsContainerOpen(Container))
		Inventory.CloseContainer(Container);
	const int Type = Network.GetWord();
	TFixedString<40> Name;
	Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
	const int Capacity = Network.GetByte();
	const bool HasParent = Network.GetByte() != 0;
	Inventory.OpenContainer(Container, TObject{Type, 0, 0}, Name, Capacity, HasParent);
	const int Count = Network.GetByte();
	for (int i = 0; i < Count; i++) {
		const int ObjectType = Network.GetWord(); // Type until issue #110 (GCC's -Wshadow in patella_core)
		int Data = 0;
		if (ObjectTypes.GetObjectFlag(ObjectType, 5))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(ObjectType, 11))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(ObjectType, 10))
			Data = Network.GetByte();
		Inventory.AppendContainerObject(Container, TObject{ObjectType, (int)Data, 0});
	}
}

// 0x40da70, lines 1934-1934
void patella::TCommunication::ReceiveCloseContainer(int &Container)
{
	Container = Network.GetByte();
	if (Inventory.IsContainerOpen(Container))
		Inventory.CloseContainer(Container);
}

// 0x40dc20, lines 1953-1953
void patella::TCommunication::ReceiveContainerAddObject()
{
	int Container = 0;
	int Type = 0;
	Container = Network.GetByte();
	Type = Network.GetWord();
	int Data = 0;
	if (ObjectTypes.GetObjectFlag(Type, 5))
		Data = Network.GetByte();
	if (ObjectTypes.GetObjectFlag(Type, 11))
		Data = Network.GetByte();
	if (ObjectTypes.GetObjectFlag(Type, 10))
		Data = Network.GetByte();
	if (Inventory.IsContainerOpen(Container))
		Inventory.InsertContainerObject(Container, TObject{Type, (int)Data, 0});
}

// 0x40de40, lines 1974-1974
void patella::TCommunication::ReceiveContainerChangeObject()
{
	int Container = 0;
	int Type = 0;
	Container = Network.GetByte();
	const int Position = Network.GetByte();
	Type = Network.GetWord();
	int Data = 0;
	if (ObjectTypes.GetObjectFlag(Type, 5))
		Data = Network.GetByte();
	if (ObjectTypes.GetObjectFlag(Type, 11))
		Data = Network.GetByte();
	if (ObjectTypes.GetObjectFlag(Type, 10))
		Data = Network.GetByte();
	if (Inventory.IsContainerOpen(Container))
		Inventory.ChangeContainerObject(Container, Position, TObject{Type, (int)Data, 0});
}

// 0x40e060, lines 1992-1992
void patella::TCommunication::ReceiveContainerDeleteObject()
{
	int Container = 0;
	int Position = 0;
	Container = Network.GetByte();
	Position = Network.GetByte();
	if (Inventory.IsContainerOpen(Container)) {
		if (Inventory.DeleteContainerObject(Container, Position))
			SendRefreshContainer(Container);
	}
}

// 0x40e230, lines 2011-2011
void patella::TCommunication::ReceiveInventoryObject()
{
	int Position = 0;
	int Type = 0;
	Position = Network.GetByte();
	Type = Network.GetWord();
	int Data = 0;
	if (ObjectTypes.GetObjectFlag(Type, 5))
		Data = Network.GetByte();
	if (ObjectTypes.GetObjectFlag(Type, 11))
		Data = Network.GetByte();
	if (ObjectTypes.GetObjectFlag(Type, 10))
		Data = Network.GetByte();
	Inventory.SetBodyObject(Position, TObject{Type, (int)Data, 0});
}

// 0x40e440, lines 2024-2024
void patella::TCommunication::ReceiveDeleteInventoryObject()
{
	int Position = 0;
	Position = Network.GetByte();
	Inventory.DeleteBodyObject(Position);
}

// 0x40e5e0, lines 2046-2046
void patella::TCommunication::ReceiveOwnTrade()
{
	TFixedString<40> Name;
	Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
	Inventory.SetOwnOffer(Name);
	const int Count = Network.GetByte();
	for (int i = 0; i < Count; i++) {
		const int Type = Network.GetWord();
		int Data = 0;
		if (ObjectTypes.GetObjectFlag(Type, 5))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 11))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 10))
			Data = Network.GetByte();
		Inventory.AddOwnOfferObject(TObject{Type, (int)Data, 0});
	}
}

// 0x40e800, lines 2068-2068
void patella::TCommunication::ReceiveCounterTrade()
{
	TFixedString<40> Name;
	Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
	const bool Open = Inventory.SetCounterOffer(Name);
	const int Count = Network.GetByte();
	for (int i = 0; i < Count; i++) {
		const int Type = Network.GetWord();
		int Data = 0;
		if (ObjectTypes.GetObjectFlag(Type, 5))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 11))
			Data = Network.GetByte();
		if (ObjectTypes.GetObjectFlag(Type, 10))
			Data = Network.GetByte();
		if (Open)
			Inventory.AddCounterOfferObject(TObject{Type, (int)Data, 0});
	}
}

// 0x40ea30, lines 2079-2079
void patella::TCommunication::ReceiveCloseTrade()
{
	Inventory.ClearOffers();
}

// 0x40ebc0, lines 2092-2092
void patella::TCommunication::ReceiveWorldLight()
{
	const int Brightness = Network.GetByte();
	const int Color = Network.GetByte();
	Map.SetAmbientLight(Brightness, Color);
}

// 0x40ed60, lines 2111-2111
void patella::TCommunication::ReceiveMagicEffect()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = Network.GetWord();
	ry = Network.GetWord();
	rz = Network.GetByte();
	Map.AbsoluteToRelative(&rx, &ry, &rz);
	const int Effect = Network.GetByte();
	Map.AddGraphicalEffect(rx, ry, rz, Effect);
}

// 0x40ef50, lines 2132-2132
void patella::TCommunication::ReceiveAnimatedText()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = Network.GetWord();
	ry = Network.GetWord();
	rz = Network.GetByte();
	Map.AbsoluteToRelative(&rx, &ry, &rz);
	const int Color = Network.GetByte();
	TFixedString<24> Text;
	Network.GetString(patella::TSpan<char>(Text.Chars).first(10));
	Map.AddTextualEffect(rx, ry, rz, Color, Text);
}

// 0x40f150, lines 2155-2155
void patella::TCommunication::ReceiveDistanceEffect()
{
	int sx = 0;
	int sy = 0;
	int sz = 0;
	sx = Network.GetWord();
	sy = Network.GetWord();
	sz = Network.GetByte();
	Map.AbsoluteToRelative(&sx, &sy, &sz);
	int dx = Network.GetWord();
	int dy = Network.GetWord();
	int dz = Network.GetByte();
	Map.AbsoluteToRelative(&dx, &dy, &dz);
	const int Effect = Network.GetByte();
	Map.AddMissileEffect(sx, sy, sz, dx, dy, dz, Effect);
}

// 0x40f380, lines 2170-2170
void patella::TCommunication::ReceiveCreatureMark()
{
	uint32 CreatureID = 0;
	CreatureID = Network.GetQuad();
	TCreature *Creature = Creatures.GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = Network.GetByte();
		Creature->SetMark(Value);
		return;
	}
	Network.GetByte();
}

// 0x40f550, lines 2185-2185
void patella::TCommunication::ReceiveCreatureHealth()
{
	uint32 CreatureID = 0;
	CreatureID = Network.GetQuad();
	TCreature *Creature = Creatures.GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = Network.GetByte();
		Creature->Health = Value;
		return;
	}
	Network.GetByte();
}

// 0x40f710, lines 2205-2205
void patella::TCommunication::ReceiveCreatureLight()
{
	uint32 CreatureID = 0;
	CreatureID = Network.GetQuad();
	TCreature *Creature = Creatures.GetCreature(CreatureID);
	if (Creature != nullptr) {
		Creature->LightStrength = Network.GetByte();
		Creature->LightColor = Network.GetByte();
		return;
	}
	Network.GetByte();
	Network.GetByte();
}

// 0x40f8e0, lines 2235-2235
void patella::TCommunication::ReceiveCreatureOutfit()
{
	uint32 CreatureID = 0;
	CreatureID = Network.GetQuad();
	TCreature *Creature = Creatures.GetCreature(CreatureID);
	const int Type = Network.GetWord();
	if (Creature == nullptr) {
		if (Type != 0) {
			Network.GetByte();
			Network.GetByte();
			Network.GetByte();
			Network.GetByte();
			return;
		}
		Network.GetWord();
		return;
	}
	Creature->OutfitType = Type;
	if (Type != 0) {
		Creature->Colors[0] = Network.GetByte();
		Creature->Colors[1] = Network.GetByte();
		Creature->Colors[2] = Network.GetByte();
		Creature->Colors[3] = Network.GetByte();
		return;
	}
	Creature->Colors[0] = Network.GetWord();
}

// 0x40fb20, lines 2250-2250
void patella::TCommunication::ReceiveCreatureSpeed()
{
	uint32 CreatureID = 0;
	CreatureID = Network.GetQuad();
	TCreature *Creature = Creatures.GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = Network.GetWord();
		Creature->Speed = Value;
		return;
	}
	Network.GetWord();
}

// 0x40fce0, lines 2267-2267
void patella::TCommunication::ReceiveCreatureSkull()
{
	uint32 CreatureID = 0;
	CreatureID = Network.GetQuad();
	TCreature *Creature = Creatures.GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Skull = Network.GetByte();
		Creature->SetSkull(Skull, true);
		Creatures.NotifySkull(Creature->PKFlag);
		return;
	}
	Network.GetByte();
}

// 0x40fec0, lines 2283-2283
void patella::TCommunication::ReceiveCreatureShield()
{
	uint32 CreatureID = 0;
	CreatureID = Network.GetQuad();
	TCreature *Creature = Creatures.GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = Network.GetByte();
		Creature->PARTYFlag = Value;
		return;
	}
	Network.GetByte();
}

// 0x410080, lines 2303-2303
void patella::TCommunication::ReceiveEditText()
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	uint32 *Info = new uint32[3];
	Info[0] = Network.GetQuad();
	Info[1] = Network.GetWord();
	Info[2] = Network.GetWord();
	char *Text = Network.GetString();
	char *Writer = Network.GetString();
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	char **Strings = new char *[2];
	Strings[0] = Writer;
	Strings[1] = Text;
	AddMessage(14, reinterpret_cast<std::intptr_t>(Info), reinterpret_cast<std::intptr_t>(Strings));
}

// 0x410260, lines 2318-2318
void patella::TCommunication::ReceiveEditHouseText()
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	uint32 *Info = new uint32[2];
	Info[0] = Network.GetByte();
	Info[1] = Network.GetQuad();
	const char *Text = Network.GetString();
	AddMessage(15, reinterpret_cast<std::intptr_t>(Info), reinterpret_cast<std::intptr_t>(Text));
}

// 0x410410, lines 2347-2347
void patella::TCommunication::ReceivePlayerStats()
{
	const int Health = Network.GetWord();
	const int MaxHealth = Network.GetWord();
	const int Capacity = Network.GetWord();
	const uint32 Experience = Network.GetQuad();
	const int Level = Network.GetWord();
	const int LevelPercent = Network.GetByte();
	const int Mana = Network.GetWord();
	const int MaxMana = Network.GetWord();
	const int MagicLevel = Network.GetByte();
	const int MagicLevelPercent = Network.GetByte();
	const int Soul = Network.GetByte();
	PlayerState.SetPlayerHealth(Health, MaxHealth);
	PlayerState.SetPlayerCapacity(Capacity);
	PlayerState.SetPlayerMana(Mana, MaxMana);
	PlayerState.SetPlayerSoul(Soul);
	PlayerState.SetPlayerExperience(Experience, Level, LevelPercent);
	PlayerState.SetPlayerMagicLevel(MagicLevel, MagicLevelPercent);
}

// 0x410660, lines 2363-2363
void patella::TCommunication::ReceivePlayerSkills()
{
	int i;
	for (i = 0; i < 7; i++) {
		const int Level = Network.GetByte();
		const int Percent = Network.GetByte();
		PlayerState.SetPlayerSkill(i, Level, Percent);
	}
}

// 0x410810, lines 2374-2374
void patella::TCommunication::ReceivePlayerIcons()
{
	const int Icons = Network.GetByte();
	PlayerState.SetPlayerState(Icons);
}

// 0x4109b0, lines 2386-2386
void patella::TCommunication::ReceiveCancelAttack()
{
	Creatures.SetAttackID(0);
}

// 0x410b40, lines 2461-2466
void patella::TCommunication::ReceiveTalk()
{
	int Mode = 1;
	int Channel = 0;
	const uint32 StatementID = Network.GetQuad();
	TFixedString<36> Name;
	Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
	Mode = Network.GetByte();
	TFixedString<632> Text;
	int Time = 0;
	int TargetChannel = -1;
	switch (Mode) {
	case 1:
	case 2:
	case 3: {
		const int x = Network.GetWord();
		const int y = Network.GetWord();
		const int z = Network.GetByte();
		Network.GetString(patella::TSpan<char>(Text.Chars).first(512));
		if (IsTalkIgnored(Mode, Name, -1)) {
			SetTalkThrottle();
			return;
		}
		AddTalkbox(Mode, Name, x, y, z, Text);
		break;
	}
	case 4:
	case 9:
	case 11:
		Network.GetString(patella::TSpan<char>(Text.Chars).first(512));
		if (IsTalkIgnored(Mode, Name, -1)) {
			SetTalkThrottle();
			return;
		}
		AddTalkbox(Mode, Name, 0, 0, 0, Text);
		break;
	case 5:
	case 10:
	case 12:
	case 14:
		Channel = Network.GetWord();
		Network.GetString(patella::TSpan<char>(Text.Chars).first(512));
		if (IsTalkIgnored(Mode, Name, -1)) {
			SetTalkThrottle();
			return;
		}
		TargetChannel = Channel;
		break;
	case 6:
		Time = -(int)Network.GetQuad();
		Network.GetString(patella::TSpan<char>(Text.Chars).first(512));
		break;
	case 7:
	case 8:
		Network.GetString(patella::TSpan<char>(Text.Chars).first(512));
		break;
	case 16:
	case 17: {
		const int x = Network.GetWord();
		const int y = Network.GetWord();
		const int z = Network.GetByte();
		Network.GetString(patella::TSpan<char>(Text.Chars).first(512));
		if (!IsTalkIgnored(Mode, Name, -1)) {
			AddTalkbox(Mode, Name, x, y, z, Text);
			return;
		}
		SetTalkThrottle();
		return;
	}
	default:
		TERROR1_REASON("unknown talk mode", Mode, "[bug0000649]");
	}
	AddConsoleText(StatementID, Text, Mode, Name, TargetChannel, Time);
}

// 0x411030, lines 2487-2487
void patella::TCommunication::ReceiveChannelList()
{
	int Count = 0;
	Count = Network.GetByte();
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	int *Channels = new int[Count + 1];
	char *Names = nullptr;
	if (Count != 0)
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
		Names = new char[Count * 30];
	Channels[0] = Count;
	for (int i = 0; i < Count; i++) {
		Channels[i + 1] = Network.GetWord();
		Network.GetString(patella::TSpan<char>(Names + i * 30, 30));
	}
	AddMessage(0x12, reinterpret_cast<std::intptr_t>(Channels), reinterpret_cast<std::intptr_t>(Names));
}

// 0x411230, lines 2502-2502
void patella::TCommunication::ReceiveOpenChannel()
{
	int ChannelID = 0;
	ChannelID = Network.GetWord();
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	char *Name = new char[30];
	Network.GetString(patella::TSpan<char>(Name, 30));
	AddMessage(48, ChannelID, reinterpret_cast<std::intptr_t>(Name));
}

// 0x4113f0, lines 2515-2515
void patella::TCommunication::ReceiveOpenPrivateConversation()
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	char *Name = new char[30];
	Network.GetString(patella::TSpan<char>(Name, 30));
	AddMessage(0x2f, 0, reinterpret_cast<std::intptr_t>(Name));
}

// 0x4115a0, lines 2530-2530
void patella::TCommunication::ReceiveRuleViolationChannel()
{
	int ChannelID = 0;
	ChannelID = Network.GetWord();
	const TTalkBuffer *Channel = CreateTextBuffer(6, 1, nullptr, ChannelID);
	if (Channel == nullptr)
		Channel = GetTextBuffer(6, nullptr, -1);
	AddMessage(0x32, reinterpret_cast<std::intptr_t>(Channel), 0);
}

// 0x411760, lines 2551-2551
void patella::TCommunication::ReceiveRemoveRuleViolation()
{
	TFixedString<44> Name;
	Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
	TTalkBuffer *Channel = GetTextBuffer(6, nullptr, -1);
	if (Channel != nullptr) {
		Channel->cursor = Channel->first;
		TTalkLine *Statement = Channel->first;
		while (Statement != nullptr) {
			TTalkLine *Current = Statement;
			Statement = Channel->GetNext();
			if (CompareStrings(Current->Addressee, Name, -1) == 0) {
				Channel->Remove(Current);
			}
		}
		SetConsoleChanged();
	}
}

// 0x411940, lines 2569-2569
void patella::TCommunication::ReceiveCancelRuleViolation()
{
	TFixedString<44> Name;
	Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
	TTalkBuffer *Channel = GetTextBuffer(8, Name, -1);
	if (Channel != nullptr) {
		TFixedString<444> Text;
		Text.Format("{} has finished the request.", Name);
		AddTextBufferLine(Channel, 0, Text, 18, nullptr, 0);
		Channel->SetWriteEnabled(false);
	}
}

// 0x411b20, lines 2584-2584
void patella::TCommunication::ReceiveLockRuleViolation()
{
	TTalkBuffer *Channel = GetTextBuffer(7, nullptr, -1);
	if (Channel != nullptr) {
		const char *Text = TextRequestClosed;
		AddTextBufferLine(Channel, 0, Text, 18, nullptr, 0);
		Channel->SetWriteEnabled(false);
	}
}

// 0x411ce0, lines 2606-2606
void patella::TCommunication::ReceiveOwnChannel()
{
	int ChannelID = 0;
	ChannelID = Network.GetWord();
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	char *Name = new char[40];
	Network.GetString(patella::TSpan<char>(Name, 40));
	AddMessage(49, ChannelID, reinterpret_cast<std::intptr_t>(Name));
}

// 0x411ea0, lines 2623-2623
void patella::TCommunication::ReceiveCloseChannel()
{
	int ChannelID = 0;
	ChannelID = Network.GetWord();
	TTalkBuffer *Channel = GetTextBuffer(5, nullptr, ChannelID);
	if (Channel != nullptr) {
		const char *Text = TextChannelClosed;
		AddTextBufferLine(Channel, 0, Text, 18, nullptr, 0);
		Channel->SetWriteEnabled(false);
	}
}

// 0x412060, lines 2634-2641
void patella::TCommunication::ReceiveTextMessage()
{
	int Mode = 0;
	Mode = Network.GetByte();
	PATELLA_ASSERT_VALUES(Mode>=0&&Mode<TALK_MODE_BEYONDLAST, Mode);
	TFixedString<632> Text;
	Network.GetString(patella::TSpan<char>(Text.Chars).first(512));
	if (Mode == 18 || Mode == 19 || Mode == 22)
		AddMessageTalkbox(Mode, Text);
	if (Mode == 23 || Mode == 21 || Mode == 20)
		SetStatusMessage(Text, false);
	if (Mode != 23)
		AddConsoleText(0, Text, Mode, nullptr, -1, 0);
}

// 0x416c40, lines 2672-2672
void patella::TCommunication::ReceiveCancelWalk()
{
	const int Direction = Network.GetByte();
	int x;
	int y;
	int z;
	PlayerState.GetPlayerPosition(&x, &y, &z);
	if (x == LastCancelX && y == LastCancelY && z == LastCancelZ)
		CancelCount++;
	else
		CancelCount = 0;
	LastCancelX = x;
	LastCancelY = y;
	LastCancelZ = z;
	if (CancelCount > 15) {
		PlayerState.ClearPlayerSteps();
		Creatures.SetAttackID(0);
		SendCancel();
		CancelCount = 0;
	}
	if (PlayerState.CancelPlayerWalk(Direction))
		SendWalk();
}

// 0x416e60, lines 2685-2685
// 0x416e90 is this loop's interior basic block, misidentified as a second function.
void patella::TCommunication::ReadFloor(int rz)
{
	// 0x416e90, lines 2685-2685. Interior loop, not a separately callable function.
	for (int rx = 0; rx < 18; rx++) {
		for (int ry = 0; ry < 14; ry++)
			ReadField(rx, ry, rz);
	}
}

// 0x417020, lines 2717-2717
void patella::TCommunication::ReceiveFloorUp()
{
	int x;
	int y;
	int z;
	Map.GetMapPosition(&x, &y, &z);
	z += -1;
	x += 1;
	y += 1;
	Map.SetMapPosition(x, y, z);
	if (z < 8) {
		if (z == 7) {
			Map.ShiftMapLevel(-3);
			SkipFields = 0;
			for (int rz = 2; rz < 8; rz++)
				ReadFloor(rz);
		}
	} else {
		Map.ShiftMapLevel(-1);
		SkipFields = 0;
		ReadFloor(4);
	}
	SetTalkChanged();
	PlayerState.StopPlayer();
	CheckEventHelpPosition(x, y, z);
	for (int rx = 0; rx < 18; rx++) {
		for (int ry = 0; ry < 14; ry++)
			Map.UpdateAutomap(rx, ry);
	}
}

// 0x417270, lines 2749-2749
void patella::TCommunication::ReceiveFloorDown()
{
	int x;
	int y;
	int z;
	Map.GetMapPosition(&x, &y, &z);
	z += 1;
	x += -1;
	y += -1;
	Map.SetMapPosition(x, y, z);
	if (z < 9) {
		if (z == 8) {
			Map.ShiftMapLevel(3);
			SkipFields = 0;
			for (int rz = 2; rz >= 0; rz--)
				ReadFloor(rz);
		}
	} else {
		Map.ShiftMapLevel(1);
		SkipFields = 0;
		if (z < 14)
			ReadFloor(0);
	}
	SetTalkChanged();
	PlayerState.StopPlayer();
	CheckEventHelpPosition(x, y, z);
	for (int rx = 0; rx < 18; rx++) {
		for (int ry = 0; ry < 14; ry++)
			Map.UpdateAutomap(rx, ry);
	}
}

// 0x412350, lines 2768-2768
void patella::TCommunication::ReceiveOutfit()
{
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): a block of the message queue (AddMessage), freed by its reader (notes/ownership.md)
	int *Outfit = new int[7];
	Outfit[0] = Network.GetWord();
	Outfit[1] = Network.GetByte();
	Outfit[2] = Network.GetByte();
	Outfit[3] = Network.GetByte();
	Outfit[4] = Network.GetByte();
	Outfit[5] = Network.GetWord();
	Outfit[6] = Network.GetWord();
	AddMessage(16, reinterpret_cast<std::intptr_t>(Outfit), 0);
}

// 0x412540, lines 2785-2785
// Only the byte value 1 means online, not every nonzero value.
void patella::TCommunication::ReceiveBuddy()
{
	uint32 CharacterID = 0;
	CharacterID = Network.GetQuad();
	TFixedString<44> Name;
	Network.GetString(patella::TSpan<char>(Name.Chars).first(30));
	const int State = Network.GetByte();
	Creatures.AddBuddy(CharacterID, Name, State == 1);
	const char *Copy = MsvcStrdup(Name);
	AddMessage(25, reinterpret_cast<std::intptr_t>(Copy), 0);
}

// 0x412720, lines 2798-2798
void patella::TCommunication::ReceiveBuddyLogin()
{
	uint32 CharacterID = 0;
	CharacterID = Network.GetQuad();
	Creatures.SetBuddyState(CharacterID, true);
}

// 0x4128c0, lines 2811-2811
void patella::TCommunication::ReceiveBuddyLogout()
{
	uint32 CharacterID = 0;
	CharacterID = Network.GetQuad();
	Creatures.SetBuddyState(CharacterID, false);
}

// Empty; the linker kept one copy of the identical empty functions (0x496160,
// GUI.obj). ExitControl calls it where Communication.cpp's exit belongs in the reverse
// order of InitControl (name ours, low confidence).
void patella::TCommunication::ExitCommunication()
{
}

// The original's functions (issue #105): each calls its member on the one TClient's TCommunication.

void InitCommunication()
{
	patella::TCommunication::InitCommunication();
}

void ExitCommunication()
{
	patella::TCommunication::ExitCommunication();
}

void SendLogin(uint32 AccountID, const char *Password)
{
	patella::bridge::Communication().SendLogin(AccountID, Password);
}

void SendGameLogin(uint32 AccountID, const char *Name, const char *Password)
{
	patella::bridge::Communication().SendGameLogin(AccountID, Name, Password);
}

void SendLogout()
{
	patella::bridge::Communication().SendLogout();
}

void SendPing()
{
	patella::bridge::Communication().SendPing();
}

void PrepareMovement()
{
	patella::bridge::Communication().PrepareMovement();
}

void SendWalk()
{
	patella::bridge::Communication().SendWalk();
}

void SendStop()
{
	patella::bridge::Communication().SendStop();
}

void SendTurnNorth()
{
	patella::bridge::Communication().SendTurnNorth();
}

void SendTurnEast()
{
	patella::bridge::Communication().SendTurnEast();
}

void SendTurnSouth()
{
	patella::bridge::Communication().SendTurnSouth();
}

void SendTurnWest()
{
	patella::bridge::Communication().SendTurnWest();
}

void SendMoveObject(int x, int y, int z, int ObjectType, int Position, int DestinationX, int DestinationY, int DestinationZ, int Count)
{
	patella::bridge::Communication().SendMoveObject(x, y, z, ObjectType, Position, DestinationX, DestinationY, DestinationZ, Count);
}

void SendRequestTrade(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID)
{
	patella::bridge::Communication().SendRequestTrade(x, y, z, ObjectType, Position, CreatureID);
}

void SendInspectTrade(int Side, int Position)
{
	patella::bridge::Communication().SendInspectTrade(Side, Position);
}

void SendAcceptTrade()
{
	patella::bridge::Communication().SendAcceptTrade();
}

void SendRejectTrade()
{
	patella::bridge::Communication().SendRejectTrade();
}

void SendUseObject(int x, int y, int z, int ObjectType, int Position, int Container)
{
	patella::bridge::Communication().SendUseObject(x, y, z, ObjectType, Position, Container);
}

void SendUseObjectWith(int x1, int y1, int z1, int ObjectType1, int Position1, int x2, int y2, int z2, int ObjectType2, int Position2)
{
	patella::bridge::Communication().SendUseObjectWith(x1, y1, z1, ObjectType1, Position1, x2, y2, z2, ObjectType2, Position2);
}

void SendUseObjectOnCreature(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID)
{
	patella::bridge::Communication().SendUseObjectOnCreature(x, y, z, ObjectType, Position, CreatureID);
}

void SendRotateObject(int x, int y, int z, int ObjectType, int Position)
{
	patella::bridge::Communication().SendRotateObject(x, y, z, ObjectType, Position);
}

void SendCloseContainer(int Container)
{
	patella::bridge::Communication().SendCloseContainer(Container);
}

void SendContainerUp(int Container)
{
	patella::bridge::Communication().SendContainerUp(Container);
}

void SendEditText(uint32 ObjectNumber, const char *Text)
{
	patella::bridge::Communication().SendEditText(ObjectNumber, Text);
}

void SendEditHouseText(int Type, uint32 Identifier, const char *Text)
{
	patella::bridge::Communication().SendEditHouseText(Type, Identifier, Text);
}

void SendLook(int x, int y, int z, int ObjectType, int Position)
{
	patella::bridge::Communication().SendLook(x, y, z, ObjectType, Position);
}

void SendTalk(int Mode, const char *Text)
{
	patella::bridge::Communication().SendTalk(Mode, Text);
}

void SendTalk(int Mode, const char *Addressee, const char *Text)
{
	patella::bridge::Communication().SendTalk(Mode, Addressee, Text);
}

void SendTalk(int Mode, int ChannelID, const char *Text)
{
	patella::bridge::Communication().SendTalk(Mode, ChannelID, Text);
}

void SendRequestChannels()
{
	patella::bridge::Communication().SendRequestChannels();
}

void SendJoinChannel(int ChannelID)
{
	patella::bridge::Communication().SendJoinChannel(ChannelID);
}

void SendLeaveChannel(int ChannelID)
{
	patella::bridge::Communication().SendLeaveChannel(ChannelID);
}

void SendOpenPrivateConversation(const char *Name)
{
	patella::bridge::Communication().SendOpenPrivateConversation(Name);
}

void SendProcessRuleViolation(const char *Name)
{
	patella::bridge::Communication().SendProcessRuleViolation(Name);
}

void SendCloseRuleViolation(const char *Name)
{
	patella::bridge::Communication().SendCloseRuleViolation(Name);
}

void SendCancelRuleViolation()
{
	patella::bridge::Communication().SendCancelRuleViolation();
}

void SendFightModes()
{
	patella::bridge::Communication().SendFightModes();
}

void SendAttack(uint32 CreatureID)
{
	patella::bridge::Communication().SendAttack(CreatureID);
}

void SendFollow(uint32 CreatureID)
{
	patella::bridge::Communication().SendFollow(CreatureID);
}

void SendOpenPrivateChannel()
{
	patella::bridge::Communication().SendOpenPrivateChannel();
}

void SendInviteToPrivateChannel(const char *Name)
{
	patella::bridge::Communication().SendInviteToPrivateChannel(Name);
}

void SendExcludeFromPrivateChannel(const char *Name)
{
	patella::bridge::Communication().SendExcludeFromPrivateChannel(Name);
}

void SendInviteToParty(uint32 CharacterID)
{
	patella::bridge::Communication().SendInviteToParty(CharacterID);
}

void SendJoinParty(uint32 CharacterID)
{
	patella::bridge::Communication().SendJoinParty(CharacterID);
}

void SendRevokePartyInvitation(uint32 CharacterID)
{
	patella::bridge::Communication().SendRevokePartyInvitation(CharacterID);
}

void SendPassPartyLeadership(uint32 CharacterID)
{
	patella::bridge::Communication().SendPassPartyLeadership(CharacterID);
}

void SendLeaveParty()
{
	patella::bridge::Communication().SendLeaveParty();
}

void SendCancel()
{
	patella::bridge::Communication().SendCancel();
}

void SendRefreshField(int x, int y, int z)
{
	patella::bridge::Communication().SendRefreshField(x, y, z);
}

void SendRefreshContainer(int Container)
{
	patella::bridge::Communication().SendRefreshContainer(Container);
}

void SendRequestOutfit()
{
	patella::bridge::Communication().SendRequestOutfit();
}

void SendOutfit(int OutfitType, const int *Colors)
{
	patella::bridge::Communication().SendOutfit(OutfitType, Colors);
}

void SendAddBuddy(const char *Name)
{
	patella::bridge::Communication().SendAddBuddy(Name);
}

void SendRemoveBuddy(uint32 CharacterID)
{
	patella::bridge::Communication().SendRemoveBuddy(CharacterID);
}

void SendBugReport(const char *Text)
{
	patella::bridge::Communication().SendBugReport(Text);
}

void SendRuleViolationReport(const char *Name, uint8 Reason, uint8 Action, const char *Comment, uint32 StatementID, bool IPBanishment)
{
	patella::bridge::Communication().SendRuleViolationReport(Name, Reason, Action, Comment, StatementID, IPBanishment);
}

void ReceiveLoginError()
{
	patella::bridge::Communication().ReceiveLoginError();
}

void ReceiveLoginMessage()
{
	patella::bridge::Communication().ReceiveLoginMessage();
}

void ReceiveMessageOfTheDay()
{
	patella::bridge::Communication().ReceiveMessageOfTheDay();
}

void ReceiveUpdateRequest()
{
	patella::bridge::Communication().ReceiveUpdateRequest();
}

void ReceiveCharacterList()
{
	patella::bridge::Communication().ReceiveCharacterList();
}

void ReceiveLoginServerOverloaded()
{
	patella::bridge::Communication().ReceiveLoginServerOverloaded();
}

void ReceiveGameLogin()
{
	patella::bridge::Communication().ReceiveGameLogin();
}

void ReceiveGamemasterActions()
{
	patella::bridge::Communication().ReceiveGamemasterActions();
}

void ReceiveGameError()
{
	patella::bridge::Communication().ReceiveGameError();
}

void ReceiveGameMessage()
{
	patella::bridge::Communication().ReceiveGameMessage();
}

void ReceiveLoginWait()
{
	patella::bridge::Communication().ReceiveLoginWait();
}

void ReceivePing()
{
	patella::bridge::Communication().ReceivePing();
}

uint32 ReadCreature(int Type, int x, int y, int z)
{
	return patella::bridge::Communication().ReadCreature(Type, x, y, z);
}

void ReadField(int rx, int ry, int rz)
{
	patella::bridge::Communication().ReadField(rx, ry, rz);
}

void ReadMap(int xmin, int ymin, int xmax, int ymax)
{
	patella::bridge::Communication().ReadMap(xmin, ymin, xmax, ymax);
}

void ReceiveMap()
{
	patella::bridge::Communication().ReceiveMap();
}

void ReceiveMapNorth()
{
	patella::bridge::Communication().ReceiveMapNorth();
}

void ReceiveMapEast()
{
	patella::bridge::Communication().ReceiveMapEast();
}

void ReceiveMapSouth()
{
	patella::bridge::Communication().ReceiveMapSouth();
}

void ReceiveMapWest()
{
	patella::bridge::Communication().ReceiveMapWest();
}

void ReceiveField()
{
	patella::bridge::Communication().ReceiveField();
}

void ReceiveAddObject()
{
	patella::bridge::Communication().ReceiveAddObject();
}

void ReceiveChangeObject()
{
	patella::bridge::Communication().ReceiveChangeObject();
}

void ReceiveDeleteObject()
{
	patella::bridge::Communication().ReceiveDeleteObject();
}

void ReceiveMoveCreature()
{
	patella::bridge::Communication().ReceiveMoveCreature();
}

void ReceiveOpenContainer(int &Container)
{
	patella::bridge::Communication().ReceiveOpenContainer(Container);
}

void ReceiveCloseContainer(int &Container)
{
	patella::bridge::Communication().ReceiveCloseContainer(Container);
}

void ReceiveContainerAddObject()
{
	patella::bridge::Communication().ReceiveContainerAddObject();
}

void ReceiveContainerChangeObject()
{
	patella::bridge::Communication().ReceiveContainerChangeObject();
}

void ReceiveContainerDeleteObject()
{
	patella::bridge::Communication().ReceiveContainerDeleteObject();
}

void ReceiveInventoryObject()
{
	patella::bridge::Communication().ReceiveInventoryObject();
}

void ReceiveDeleteInventoryObject()
{
	patella::bridge::Communication().ReceiveDeleteInventoryObject();
}

void ReceiveOwnTrade()
{
	patella::bridge::Communication().ReceiveOwnTrade();
}

void ReceiveCounterTrade()
{
	patella::bridge::Communication().ReceiveCounterTrade();
}

void ReceiveCloseTrade()
{
	patella::bridge::Communication().ReceiveCloseTrade();
}

void ReceiveWorldLight()
{
	patella::bridge::Communication().ReceiveWorldLight();
}

void ReceiveMagicEffect()
{
	patella::bridge::Communication().ReceiveMagicEffect();
}

void ReceiveAnimatedText()
{
	patella::bridge::Communication().ReceiveAnimatedText();
}

void ReceiveDistanceEffect()
{
	patella::bridge::Communication().ReceiveDistanceEffect();
}

void ReceiveCreatureMark()
{
	patella::bridge::Communication().ReceiveCreatureMark();
}

void ReceiveCreatureHealth()
{
	patella::bridge::Communication().ReceiveCreatureHealth();
}

void ReceiveCreatureLight()
{
	patella::bridge::Communication().ReceiveCreatureLight();
}

void ReceiveCreatureOutfit()
{
	patella::bridge::Communication().ReceiveCreatureOutfit();
}

void ReceiveCreatureSpeed()
{
	patella::bridge::Communication().ReceiveCreatureSpeed();
}

void ReceiveCreatureSkull()
{
	patella::bridge::Communication().ReceiveCreatureSkull();
}

void ReceiveCreatureShield()
{
	patella::bridge::Communication().ReceiveCreatureShield();
}

void ReceiveEditText()
{
	patella::bridge::Communication().ReceiveEditText();
}

void ReceiveEditHouseText()
{
	patella::bridge::Communication().ReceiveEditHouseText();
}

void ReceivePlayerStats()
{
	patella::bridge::Communication().ReceivePlayerStats();
}

void ReceivePlayerSkills()
{
	patella::bridge::Communication().ReceivePlayerSkills();
}

void ReceivePlayerIcons()
{
	patella::bridge::Communication().ReceivePlayerIcons();
}

void ReceiveCancelAttack()
{
	patella::bridge::Communication().ReceiveCancelAttack();
}

void ReceiveTalk()
{
	patella::bridge::Communication().ReceiveTalk();
}

void ReceiveChannelList()
{
	patella::bridge::Communication().ReceiveChannelList();
}

void ReceiveOpenChannel()
{
	patella::bridge::Communication().ReceiveOpenChannel();
}

void ReceiveOpenPrivateConversation()
{
	patella::bridge::Communication().ReceiveOpenPrivateConversation();
}

void ReceiveRuleViolationChannel()
{
	patella::bridge::Communication().ReceiveRuleViolationChannel();
}

void ReceiveRemoveRuleViolation()
{
	patella::bridge::Communication().ReceiveRemoveRuleViolation();
}

void ReceiveCancelRuleViolation()
{
	patella::bridge::Communication().ReceiveCancelRuleViolation();
}

void ReceiveLockRuleViolation()
{
	patella::bridge::Communication().ReceiveLockRuleViolation();
}

void ReceiveOwnChannel()
{
	patella::bridge::Communication().ReceiveOwnChannel();
}

void ReceiveCloseChannel()
{
	patella::bridge::Communication().ReceiveCloseChannel();
}

void ReceiveTextMessage()
{
	patella::bridge::Communication().ReceiveTextMessage();
}

void ReceiveCancelWalk()
{
	patella::bridge::Communication().ReceiveCancelWalk();
}

void ReadFloor(int rz)
{
	patella::bridge::Communication().ReadFloor(rz);
}

void ReceiveFloorUp()
{
	patella::bridge::Communication().ReceiveFloorUp();
}

void ReceiveFloorDown()
{
	patella::bridge::Communication().ReceiveFloorDown();
}

void ReceiveOutfit()
{
	patella::bridge::Communication().ReceiveOutfit();
}

void ReceiveBuddy()
{
	patella::bridge::Communication().ReceiveBuddy();
}

void ReceiveBuddyLogin()
{
	patella::bridge::Communication().ReceiveBuddyLogin();
}

void ReceiveBuddyLogout()
{
	patella::bridge::Communication().ReceiveBuddyLogout();
}
