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
#include "Help.h"
#include "Inventory.h"
#include "Creatures.h"
#include "graphics.h"
#include "Map.h"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string_view>

namespace {

int SkipFields; // 0x5c4340
// 0x5c4344-0x5c4357, the patch transfer's state (PatchData, PatchWholeFile, PatchDataCount, PatchDataSize,
// PatchFileNumber) is gone with the patcher since Patella-Modern issue #75 (decision D9).

// The login server's request to update the client (issue #75): the patch packets end the login with this text in the
// login error dialog.
constexpr std::string_view TextUpdateRefused = "This server wants to update the client";

} // namespace

bool OutfitRequested; // 0x5c4358

namespace {

int CancelCount; // 0x5c4378
int LastCancelZ; // 0x5c437c
int LastCancelY; // 0x5c4380
int LastCancelX; // 0x5c4384

} // namespace

static_assert(offsetof(TCreature, Direction) == 0x50, "creature direction");
static_assert(offsetof(TCreature, OutfitType) == 0x60, "creature outfit");
static_assert(offsetof(TCreature, PKFlag) == 0x90, "creature skull");
#if PATELLA_ORIGINAL_LAYOUT
static_assert(offsetof(TTalkBuffer, first) == 0x10, "channel first statement");
static_assert(offsetof(TTalkBuffer, cursor) == 0x18, "channel cursor");
static_assert(offsetof(TTalkLine, Addressee) == 0x14, "statement addressee");
#endif

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
void InitCommunication()
{
}

// 0x413780, lines 41-96
void SendLogin(uint32 AccountID, const char *Password)
{
	TASSERT(41, Password!=NULL);
	std::array<uint8, 16> Key;
	for (int i = 0; i < 16; i++)
		Key[i] = (uint8)rand();
	SetSymmetricKey(Key.data());
	std::array<uint8, 128> Data;
	for (int i = 0; i < 128; i++)
		Data[i] = (uint8)rand();
	TWriteBuffer Buffer(Data);
	Buffer.PutByte(0);
	Buffer.PutBytes(Key);
	Buffer.PutQuad(AccountID);
	Buffer.PutString(Password);
	PATELLA_TEST_RSA_BLOCK(Data)
	try {
		TRSA RSA;
		RSA.SetPublicKey("124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284902061793");
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
		throw TMYEXCEPTION(73, Text.str().c_str(), "", 0, "", 0, Error);
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
		TERROR(84, Text.str().c_str());
	}
	BeginCommand(1);
	SendWord(2);
	SendWord(772);
	SendQuad(DatSignature);
	SendQuad(SpriteSignature);
	SendQuad(PictureSignature);
	SendBytes(Data);
	SendCommand(false);
}

// 0x413de0, lines 109-166
void SendGameLogin(uint32 AccountID, const char *Name, const char *Password)
{
	TASSERT(109, Name!=NULL);
	TASSERT(110, Password!=NULL);
	std::array<uint8, 16> Key;
	for (int i = 0; i < 16; i++)
		Key[i] = (uint8)rand();
	SetSymmetricKey(Key.data());
	std::array<uint8, 128> Data;
	for (int i = 0; i < 128; i++)
		Data[i] = (uint8)rand();
	TWriteBuffer Buffer(Data);
	Buffer.PutByte(0);
	Buffer.PutBytes(Key);
	Buffer.PutByte(GetGamemaster() != 0);
	Buffer.PutQuad(AccountID);
	Buffer.PutString(Name);
	Buffer.PutString(Password);
	PATELLA_TEST_RSA_BLOCK(Data)
	try {
		TRSA RSA;
		RSA.SetPublicKey("124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284902061793");
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
		throw TMYEXCEPTION(146, Text.str().c_str(), "", 0, "", 0, Error);
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
		TERROR(157, Text.str().c_str());
	}
	BeginCommand(10);
	SendWord(2);
	SendWord(772);
	SendBytes(Data);
	SendCommand(false);
}

// 0x404350, lines 178-178
// The body is in the leading gap, but its Communication.cpp catch identifies source line 178.
void SendLogout()
{
	BeginCommand(0x14);
	SendCommand(true);
}

// 0x404590, lines 190-190
void SendPing()
{
	BeginCommand(0x1e);
	SendCommand(true);
}

// 0x414530, lines 208-208
void PrepareMovement()
{
	if (GetAutoChaseOff()) {
		if (GetAttackID() != 0) {
			if (GetChaseMode() != 0) {
				SetChaseMode(0);
				SendFightModes();
			}
		}
	}
	if (GetFollowID() != 0) {
		SetFollowID(0);
		SendFollow(0);
	}
}

// 0x414700, lines 219-238
// An invalid single-step direction still sends the existing command buffer.
void SendWalk()
{
	PrepareMovement();
	const int Steps = GetNumberOfPlayerSteps();
	TASSERT1(219, Steps>0, Steps);
	if (Steps == 1) {
		switch (GetPlayerStep(0)) {
		case 1: BeginCommand(0x66); break;
		case 2: BeginCommand(0x6a); break;
		case 3: BeginCommand(0x65); break;
		case 4: BeginCommand(0x6d); break;
		case 5: BeginCommand(0x68); break;
		case 6: BeginCommand(0x6c); break;
		case 7: BeginCommand(0x67); break;
		case 8: BeginCommand(0x6b); break;
		}
	} else {
		BeginCommand(0x64);
		SendByte(Steps);
		for (int i = 0; i < Steps; i++)
			SendByte(GetPlayerStep(i));
	}
	SendCommand(true);
}

// 0x414a10, lines 251-251
void SendStop()
{
	PrepareMovement();
	BeginCommand(0x69);
	SendCommand(true);
}

// 0x404730, lines 263-263
void SendTurnNorth()
{
	BeginCommand(0x6f);
	SendCommand(true);
}

// 0x4048d0, lines 275-275
void SendTurnEast()
{
	BeginCommand(0x70);
	SendCommand(true);
}

// 0x404a70, lines 287-287
void SendTurnSouth()
{
	BeginCommand(0x71);
	SendCommand(true);
}

// 0x404c10, lines 299-299
void SendTurnWest()
{
	BeginCommand(0x72);
	SendCommand(true);
}

// 0x404db0, lines 325-325
void SendMoveObject(int x, int y, int z, int ObjectType, int Position, int DestinationX, int DestinationY, int DestinationZ, int Count)
{
	if (GetObjectFlag(ObjectType, 13)) {
		SetStatusMessage("You cannot move this object.", false);
		return;
	}
	if (x != 0xffff)
		ClearPlayerSteps();
	BeginCommand(0x78);
	SendWord(x);
	SendWord(y);
	SendByte(z);
	SendWord(ObjectType);
	SendByte(Position);
	SendWord(DestinationX);
	SendWord(DestinationY);
	SendByte(DestinationZ);
	SendByte(Count);
	SendCommand(true);
}

// 0x405000, lines 344-344
void SendRequestTrade(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID)
{
	ClearPlayerSteps();
	BeginCommand(0x7d);
	SendWord(x);
	SendWord(y);
	SendByte(z);
	SendWord(ObjectType);
	SendByte(Position);
	SendQuad(CreatureID);
	SendCommand(true);
}

// 0x4051f0, lines 358-358
void SendInspectTrade(int Side, int Position)
{
	BeginCommand(0x7e);
	SendByte(Side);
	SendByte(Position);
	SendCommand(true);
}

// 0x4053b0, lines 370-370
void SendAcceptTrade()
{
	BeginCommand(0x7f);
	SendCommand(true);
}

// 0x405550, lines 382-382
void SendRejectTrade()
{
	BeginCommand(0x80);
	SendCommand(true);
}

// 0x4056f0, lines 401-401
void SendUseObject(int x, int y, int z, int ObjectType, int Position, int Container)
{
	if (x != 0xffff)
		ClearPlayerSteps();
	BeginCommand(0x82);
	SendWord(x);
	SendWord(y);
	SendByte(z);
	SendWord(ObjectType);
	SendByte(Position);
	SendByte(Container);
	SendCommand(true);
}

// 0x4058f0, lines 424-424
void SendUseObjectWith(int x1, int y1, int z1, int ObjectType1, int Position1, int x2, int y2, int z2, int ObjectType2, int Position2)
{
	if (x1 != 0xffff || x2 != 0xffff)
		ClearPlayerSteps();
	BeginCommand(0x83);
	SendWord(x1);
	SendWord(y1);
	SendByte(z1);
	SendWord(ObjectType1);
	SendByte(Position1);
	SendWord(x2);
	SendWord(y2);
	SendByte(z2);
	SendWord(ObjectType2);
	SendByte(Position2);
	SendCommand(true);
}

// 0x405b20, lines 443-443
void SendUseObjectOnCreature(int x, int y, int z, int ObjectType, int Position, uint32 CreatureID)
{
	ClearPlayerSteps();
	BeginCommand(0x84);
	SendWord(x);
	SendWord(y);
	SendByte(z);
	SendWord(ObjectType);
	SendByte(Position);
	SendQuad(CreatureID);
	SendCommand(true);
}

// 0x405d10, lines 460-460
void SendRotateObject(int x, int y, int z, int ObjectType, int Position)
{
	BeginCommand(0x85);
	SendWord(x);
	SendWord(y);
	SendByte(z);
	SendWord(ObjectType);
	SendByte(Position);
	SendCommand(true);
}

// 0x405ef0, lines 473-473
void SendCloseContainer(int Container)
{
	BeginCommand(0x87);
	SendByte(Container);
	SendCommand(true);
}

// 0x4060a0, lines 486-486
void SendContainerUp(int Container)
{
	BeginCommand(0x88);
	SendByte(Container);
	SendCommand(true);
}

// 0x406250, lines 500-500
void SendEditText(uint32 ObjectNumber, const char *Text)
{
	BeginCommand(0x89);
	SendQuad(ObjectNumber);
	SendString(Text);
	SendCommand(true);
}

// 0x406410, lines 515-515
void SendEditHouseText(int Type, uint32 Identifier, const char *Text)
{
	BeginCommand(0x8a);
	SendByte(Type);
	SendQuad(Identifier);
	SendString(Text);
	SendCommand(true);
}

// 0x4065e0, lines 532-532
void SendLook(int x, int y, int z, int ObjectType, int Position)
{
	BeginCommand(0x8c);
	SendWord(x);
	SendWord(y);
	SendByte(z);
	SendWord(ObjectType);
	SendByte(Position);
	SendCommand(true);
}

// 0x4067c0, lines 541-549
void SendTalk(int Mode, const char *Text)
{
	TASSERT1(541, Mode==TALK_SAY || Mode==TALK_WHISPER || Mode==TALK_YELL || Mode==TALK_GAMEMASTER_REQUEST || Mode==TALK_PLAYER_ANSWER || Mode==TALK_GAMEMASTER_BROADCAST || Mode==TALK_ANONYMOUS_BROADCAST, Mode);
	TASSERT(542, Text!=NULL);
	BeginCommand(0x96);
	SendByte(Mode);
	SendString(Text);
	SendCommand(true);
}

// 0x406b40, lines 557-567
void SendTalk(int Mode, const char *Addressee, const char *Text)
{
	TASSERT1(557, Mode==TALK_PRIVATE_MESSAGE || Mode==TALK_GAMEMASTER_ANSWER || Mode==TALK_GAMEMASTER_MESSAGE || Mode==TALK_ANONYMOUS_MESSAGE, Mode);
	TASSERT(558, Addressee!=NULL);
	TASSERT(559, Text!=NULL);
	BeginCommand(0x96);
	SendByte(Mode);
	SendString(Addressee);
	SendString(Text);
	SendCommand(true);
}

// 0x406f90, lines 575-584
void SendTalk(int Mode, int ChannelID, const char *Text)
{
	TASSERT1(575, Mode==TALK_CHANNEL_CALL || Mode==TALK_GAMEMASTER_CHANNELCALL || Mode==TALK_ANONYMOUS_CHANNELCALL, Mode);
	TASSERT(576, Text!=NULL);
	BeginCommand(0x96);
	SendByte(Mode);
	SendWord(ChannelID);
	SendString(Text);
	SendCommand(true);
}

// 0x407300, lines 596-596
void SendRequestChannels()
{
	BeginCommand(0x97);
	SendCommand(true);
}

// 0x4074a0, lines 609-609
void SendJoinChannel(int ChannelID)
{
	BeginCommand(0x98);
	SendWord(ChannelID);
	SendCommand(true);
}

// 0x407650, lines 622-622
void SendLeaveChannel(int ChannelID)
{
	BeginCommand(0x99);
	SendWord(ChannelID);
	SendCommand(true);
}

// 0x407800, lines 635-635
void SendOpenPrivateConversation(const char *Name)
{
	BeginCommand(0x9a);
	SendString(Name);
	SendCommand(true);
}

// 0x4079b0, lines 648-648
void SendProcessRuleViolation(const char *Name)
{
	BeginCommand(0x9b);
	SendString(Name);
	SendCommand(true);
}

// 0x407b60, lines 661-661
void SendCloseRuleViolation(const char *Name)
{
	BeginCommand(0x9c);
	SendString(Name);
	SendCommand(true);
}

// 0x407d10, lines 673-673
void SendCancelRuleViolation()
{
	BeginCommand(0x9d);
	SendCommand(true);
}

// 0x407eb0, lines 689-689
void SendFightModes()
{
	BeginCommand(0xa0);
	SendByte(GetAttackMode());
	SendByte(GetChaseMode());
	SendByte(GetSecureMode());
	SendCommand(true);
}

// 0x408080, lines 703-703
void SendAttack(uint32 CreatureID)
{
	if (CreatureID != 0)
		ClearPlayerSteps();
	BeginCommand(0xa1);
	SendQuad(CreatureID);
	SendCommand(true);
}

// 0x408230, lines 717-717
void SendFollow(uint32 CreatureID)
{
	if (CreatureID != 0)
		ClearPlayerSteps();
	BeginCommand(0xa2);
	SendQuad(CreatureID);
	SendCommand(true);
}

// 0x4083e0, lines 729-729
void SendOpenPrivateChannel()
{
	BeginCommand(0xaa);
	SendCommand(true);
}

// 0x408580, lines 742-742
void SendInviteToPrivateChannel(const char *Name)
{
	BeginCommand(0xab);
	SendString(Name);
	SendCommand(true);
}

// 0x408730, lines 755-755
void SendExcludeFromPrivateChannel(const char *Name)
{
	BeginCommand(0xac);
	SendString(Name);
	SendCommand(true);
}

// 0x4088e0, lines 768-768
void SendInviteToParty(uint32 CharacterID)
{
	BeginCommand(0xa3);
	SendQuad(CharacterID);
	SendCommand(true);
}

// 0x408a90, lines 781-781
void SendJoinParty(uint32 CharacterID)
{
	BeginCommand(0xa4);
	SendQuad(CharacterID);
	SendCommand(true);
}

// 0x408c40, lines 794-794
void SendRevokePartyInvitation(uint32 CharacterID)
{
	BeginCommand(0xa5);
	SendQuad(CharacterID);
	SendCommand(true);
}

// 0x408df0, lines 807-807
void SendPassPartyLeadership(uint32 CharacterID)
{
	BeginCommand(0xa6);
	SendQuad(CharacterID);
	SendCommand(true);
}

// 0x408fa0, lines 818-818
void SendLeaveParty()
{
	BeginCommand(0xa7);
	SendCommand(true);
}

// 0x409140, lines 830-830
void SendCancel()
{
	BeginCommand(0xbe);
	SendCommand(true);
}

// 0x4092e0, lines 847-847
void SendRefreshField(int x, int y, int z)
{
	BeginCommand(0xc9);
	SendWord(x);
	SendWord(y);
	SendByte(z);
	SendCommand(true);
	AbsoluteToRelative(&x, &y, &z);
	RequestFieldRefresh(x, y, z);
}

// 0x4094d0, lines 860-860
void SendRefreshContainer(int Container)
{
	BeginCommand(0xca);
	SendByte(Container);
	SendCommand(true);
}

// 0x409680, lines 875-875
// The request flag is set before sending and is not rolled back on an exception.
void SendRequestOutfit()
{
	if (!OutfitRequested) {
		OutfitRequested = true;
		BeginCommand(0xd2);
		SendCommand(true);
	}
}

// 0x409830, lines 883-893
void SendOutfit(int OutfitType, const int *Colors)
{
	TASSERT(883, Colors!=NULL);
	BeginCommand(0xd3);
	SendWord(OutfitType);
	SendByte(Colors[0]);
	SendByte(Colors[1]);
	SendByte(Colors[2]);
	SendByte(Colors[3]);
	SendCommand(true);
}

// 0x409b00, lines 902-908
void SendAddBuddy(const char *Name)
{
	TASSERT(902, Name!=NULL);
	BeginCommand(0xdc);
	SendString(Name);
	SendCommand(true);
}

// 0x409d90, lines 921-921
void SendRemoveBuddy(uint32 CharacterID)
{
	BeginCommand(0xdd);
	SendQuad(CharacterID);
	SendCommand(true);
}

// 0x409f40, lines 934-934
void SendBugReport(const char *Text)
{
	BeginCommand(0xe6);
	SendString(Text);
	SendCommand(true);
}

// 0x40a0f0, lines 953-953
void SendRuleViolationReport(const char *Name, uint8 Reason, uint8 Action, const char *Comment, uint32 StatementID, bool IPBanishment)
{
	BeginCommand(0xe7);
	SendString(Name);
	SendByte(Reason);
	SendByte(Action);
	SendString(Comment);
	SendQuad(StatementID);
	SendByte(IPBanishment);
	SendCommand(true);
}

// 0x40a2f0, line 969, SendDebugReport (client packet 0xe8: an error report of error.txt) is gone since
// Patella-Modern issue #74 (decision D2).

// 0x40a4c0, lines 994-994. Out of line (PATELLA_NOINLINE): the golden
// corpus's Control test replaces it, and ReceivePackets would inline it (issue #78).
PATELLA_NOINLINE void ReceiveLoginError()
{
	const char *Text = GetString();
	AddMessage(1, reinterpret_cast<INT_PTR>("Sorry"), reinterpret_cast<INT_PTR>(Text));
	Disconnect(false);
	SetConnectionState(0);
}

// 0x40a670, lines 1006-1006. Out of line (PATELLA_NOINLINE): the golden
// corpus's Control test replaces it, and ReceivePackets would inline it (issue #78).
PATELLA_NOINLINE void ReceiveLoginMessage()
{
	const char *Text = GetString();
	AddMessage(1, reinterpret_cast<INT_PTR>("For Your Information"), reinterpret_cast<INT_PTR>(Text));
}

// 0x40a810, lines 1020-1020
void ReceiveMessageOfTheDay()
{
	const char *Text = GetString();
	if (SetMotD(Text))
		AddMessage(0x28, 0, 0);
	delete[] Text;
}

// 0x40a9c0 ReceivePatch (lines 1033-1045, packet 0x1e: a difference patch of a file), 0x40ae70 ReceiveWholeFile
// (1061-1073, 0x1f: a whole file) and 0x40b290 ReceivePatchBlock (1083-1135, 0x20: a block of either) are gone since
// Patella-Modern issue #75 (decision D9): with them a login server could rewrite Tibia.exe (as Tibia.bak), Tibia.dat,
// Tibia.spr, Tibia.pic and Patch.exe, and have Patch.exe run. ReceiveUpdateRequest takes all three: the login ends as
// a login error does, in the same dialog ("Sorry"), with TextUpdateRefused. The rest of what the server sent (the
// patch's header or block, the packets after it in this frame and in the frames already received) is read and
// dropped, so that nothing of it reaches the game server's table once the connection state is 0.
void ReceiveUpdateRequest()
{
	while (GetNextCommand() != -1) {
	}
	char *Text = new char[TextUpdateRefused.size() + 1];
	memcpy(Text, TextUpdateRefused.data(), TextUpdateRefused.size());
	Text[TextUpdateRefused.size()] = 0;
	AddMessage(1, reinterpret_cast<INT_PTR>("Sorry"), reinterpret_cast<INT_PTR>(Text));
	Disconnect(false);
	SetConnectionState(0);
}

// 0x40b8b0, lines 1162-1162
void ReceiveCharacterList()
{
	int Count = 0;
	Count = GetByte();
	SetAccountCharacters(Count);
	for (int i = 0; i < Count; i++) {
		TFixedString<40> Name;
		TFixedString<32> World;
		std::array<uint8, 8> Address;
		GetString(patella::TSpan<char>(Name.Chars).first(30));
		GetString(patella::TSpan<char>(World.Chars).first(30));
		for (int j = 0; j < 4; j++)
			Address[j] = GetByte();
		const int Port = GetWord();
		SetCharacter(i, Name, World, Address.data(), Port);
	}
	const int PremiumDays = GetWord();
	SetPremiumDays(PremiumDays);
	AddMessage(10, 300, 0);
}

// 0x40bac0, CancelPatch (line 1178: the update dialog's Cancel, the transfer dropped and the connection closed), is
// gone with the update dialog since Patella-Modern issue #75.

// 0x40bc80, lines 1192-1192
void ReceiveLoginServerOverloaded()
{
	SetConnectionTimeout(0);
	Disconnect(false);
	SetConnectionState(0);
	SetServerError(ERROR_LOGINSERVER_OVERLOADED);
	ConnectToLoginServer();
}

// 0x40be30, lines 1215-1215
void ReceiveGameLogin()
{
	ClearRights();
	const uint32 CreatureID = GetQuad();
	SetPlayerID(CreatureID);
	const int Beat = GetWord();
	SetBeat(Beat);
	const uint8 value = GetByte();
	SetRight(40, value);
	SetActionState(0);
}

// 0x40c000, lines 1232-1232
void ReceiveGamemasterActions()
{
	int i;
	uint8 value = 0;
	for (i = 0; i < 32; i++) {
		value = GetByte();
		SetRight(i, value);
	}
}

// 0x40c1b0, lines 1246-1246
void ReceiveGameError()
{
	const char *Text = GetString();
	AddMessage(1, reinterpret_cast<INT_PTR>("Sorry"), reinterpret_cast<INT_PTR>(Text));
	Disconnect(false);
	SetConnectionState(0);
}

// 0x40c360, lines 1258-1258
void ReceiveGameMessage()
{
	const char *Text = GetString();
	AddMessage(1, reinterpret_cast<INT_PTR>("For Your Information"), reinterpret_cast<INT_PTR>(Text));
}

// 0x40c500, lines 1271-1271
void ReceiveLoginWait()
{
	const char *Text = GetString();
	const int Delay = GetByte();
	AddMessage(5, reinterpret_cast<INT_PTR>(Text), Delay);
}

// 0x40c6a0, lines 1282-1282
void ReceivePing()
{
	SendPing();
}

// 0x40c830, lines 1302-1368
// A missing known creature consumes a byte, not a word, for outfit type and skips no skull/shield bytes.
uint32 ReadCreature(int Type, int x, int y, int z)
{
	uint32 CreatureID = 0;
	TCreature *Creature;
	if (Type == 0x61) {
		const uint32 RemoveID = GetQuad();
		CreatureID = GetQuad();
		Creature = AddCreature(RemoveID, CreatureID);
		TASSERT1(1302, Creature!=NULL, CreatureID);
		GetString(patella::TSpan<char>(Creature->Name.Chars).first(30));
		if (!IsPlayer(CreatureID))
			CapitalizeString(Creature->Name);
		Creature->Health = GetByte();
		Creature->Direction = GetByte();
		Creature->OutfitType = GetWord();
		if (Creature->OutfitType == 0) {
			Creature->Colors[0] = GetWord();
		} else {
			Creature->Colors[0] = GetByte();
			Creature->Colors[1] = GetByte();
			Creature->Colors[2] = GetByte();
			Creature->Colors[3] = GetByte();
		}
		Creature->LightStrength = GetByte();
		Creature->LightColor = GetByte();
		Creature->Speed = GetWord();
		const int Skull = GetByte();
		Creature->SetSkull(Skull, false);
		NotifySkull(Creature->PKFlag);
		Creature->PARTYFlag = GetByte();
	} else if (Type == 0x62) {
		CreatureID = GetQuad();
		Creature = GetCreature(CreatureID);
		if (Creature == nullptr) {
			GetByte();
			GetByte();
			if (GetByte() == 0) {
				GetWord();
				GetByte();
				GetByte();
				GetWord();
			} else {
				GetByte();
				GetByte();
				GetByte();
				GetByte();
				GetByte();
				GetByte();
				GetWord();
			}
		} else {
			Creature->Health = GetByte();
			Creature->Direction = GetByte();
			Creature->OutfitType = GetWord();
			if (Creature->OutfitType == 0) {
				Creature->Colors[0] = GetWord();
			} else {
				Creature->Colors[0] = GetByte();
				Creature->Colors[1] = GetByte();
				Creature->Colors[2] = GetByte();
				Creature->Colors[3] = GetByte();
			}
			Creature->LightStrength = GetByte();
			Creature->LightColor = GetByte();
			Creature->Speed = GetWord();
			const int Skull = GetByte();
			Creature->SetSkull(Skull, false);
			NotifySkull(Creature->PKFlag);
			Creature->PARTYFlag = GetByte();
		}
	} else {
		if (Type != 0x63)
			return 0;
		CreatureID = GetQuad();
		Creature = GetCreature(CreatureID);
		if (Creature == nullptr)
			GetByte();
		else
			Creature->Direction = GetByte();
	}
	if (Creature != nullptr) {
		RelativeToAbsolute(&x, &y, &z);
		Creature->SetPosition(x, y, z);
		Creature->Visible = true;
	}
	return CreatureID;
}

// 0x414bb0, lines 1401-1416. OldType only the original's catch wrapper read (issue #78).
void ReadField(int rx, int ry, int rz)
{
	uint16 Type = 0;
	[[maybe_unused]] uint16 OldType = 0;
	if (SkipFields < 1) {
		for (;;) {
			OldType = Type;
			Type = GetWord();
			if (Type >= 0xff00) {
				SkipFields = Type - 0xff00;
				break;
			}
			if (Type == 0) {
				std::stringstream Text;
				Text << "client recieved field with type 0 (" << rx << "," << ry << "," << rz << ")";
				TERROR(1401, Text.str().c_str());
			}
			if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
				const uint32 CreatureID = ReadCreature(Type, rx, ry, rz);
				AppendObject(rx, ry, rz, TObject{0x63, (int)CreatureID, 0});
			} else {
				int Data = 0;
				if (GetObjectFlag(Type, 5))
					Data = GetByte();
				if (GetObjectFlag(Type, 11))
					Data = GetByte();
				if (GetObjectFlag(Type, 10))
					Data = GetByte();
				AppendObject(rx, ry, rz, TObject{Type, Data, 0});
			}
		}
	} else {
		SkipFields--;
	}
	if (rz == GetMapLevel())
		UpdateAutomap(rx, ry);
}

// 0x414ff0, lines 1437-1437
void ReadMap(int xmin, int ymin, int xmax, int ymax)
{
	int PlayerX;
	int PlayerY;
	int PlayerZ;
	GetMapPosition(&PlayerX, &PlayerY, &PlayerZ);
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
void ReceiveMap()
{
	uint16 PlayerX = 0;
	uint16 PlayerY = 0;
	PlayerX = GetWord();
	PlayerY = GetWord();
	const int PlayerZ = GetByte();
	SetMapPosition(PlayerX, PlayerY, PlayerZ);
	ClearMap();
	HideAllCreatures();
	ReadMap(0, 0, 17, 13);
	SetTalkChanged();
	StopPlayer();
	CheckEventHelpPosition(PlayerX, PlayerY, PlayerZ);
}

// 0x4153f0, lines 1476-1476
void ReceiveMapNorth()
{
	ShiftMap(0, 1);
	ReadMap(0, 0, 17, 0);
	SetTalkChanged();
	int x;
	int y;
	int z;
	GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x4155c0, lines 1492-1492
void ReceiveMapEast()
{
	ShiftMap(-1, 0);
	ReadMap(17, 0, 17, 13);
	SetTalkChanged();
	int x;
	int y;
	int z;
	GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x415790, lines 1508-1508
void ReceiveMapSouth()
{
	ShiftMap(0, -1);
	ReadMap(0, 13, 17, 13);
	SetTalkChanged();
	int x;
	int y;
	int z;
	GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x415960, lines 1524-1524
void ReceiveMapWest()
{
	ShiftMap(1, 0);
	ReadMap(0, 0, 0, 13);
	SetTalkChanged();
	int x;
	int y;
	int z;
	GetMapPosition(&x, &y, &z);
	CheckEventHelpPosition(x, y, z);
}

// 0x415b30, lines 1546-1546
// ReadField already refreshes visible-floor lighting; this handler deliberately does it again.
void ReceiveField()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = GetWord();
	ry = GetWord();
	rz = GetByte();
	AbsoluteToRelative(&rx, &ry, &rz);
	ClearField(rx, ry, rz, false);
	SkipFields = 0;
	ReadField(rx, ry, rz);
	if (rz == GetMapLevel())
		UpdateAutomap(rx, ry);
}

// 0x40cd10, lines 1575-1575
void ReceiveAddObject()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = GetWord();
	ry = GetWord();
	rz = GetByte();
	AbsoluteToRelative(&rx, &ry, &rz);
	const int Type = GetWord();
	TObject Object;
	if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
		Object.Type = 0x63;
		Object.Data = ReadCreature(Type, rx, ry, rz);
		Object.Data2 = 0;
	} else {
		int Data = 0;
		if (GetObjectFlag(Type, 5))
			Data = GetByte();
		if (GetObjectFlag(Type, 11))
			Data = GetByte();
		if (GetObjectFlag(Type, 10))
			Data = GetByte();
		Object.Type = Type;
		Object.Data = Data;
		Object.Data2 = 0;
	}
	AddObject(rx, ry, rz, Object);
	if (rz == GetMapLevel())
		UpdateAutomap(rx, ry);
}

// 0x40cfa0, lines 1594-1630
void ReceiveChangeObject()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = GetWord();
	ry = GetWord();
	rz = GetByte();
	const int Position = GetByte(); // read before the conversion (0x40cff3)
	AbsoluteToRelative(&rx, &ry, &rz);
	const TObject OldObject = GetMapObject(rx, ry, rz, Position);
	if (OldObject.Type == NoObject.Type) {
		TASSERT2(1594, FieldRefreshRequested(rx,ry,rz), rx, ry);
		const int Type = GetWord();
		if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
			ReadCreature(Type, rx, ry, rz);
			return;
		}
		if (GetObjectFlag(Type, 5))
			GetByte();
		if (GetObjectFlag(Type, 11))
			GetByte();
		if (GetObjectFlag(Type, 10))
			GetByte();
	} else {
		if (OldObject.Type == 0x63) {
			TCreature *Creature = GetCreature(OldObject.Data);
			if (Creature != nullptr)
				Creature->Visible = false;
		}
		const int Type = GetWord();
		TObject Object;
		if (Type == 0x61 || Type == 0x62 || Type == 0x63) {
			Object.Type = 0x63;
			Object.Data = ReadCreature(Type, rx, ry, rz);
			Object.Data2 = 0;
		} else {
			int Data = 0;
			if (GetObjectFlag(Type, 5))
				Data = GetByte();
			if (GetObjectFlag(Type, 11))
				Data = GetByte();
			if (GetObjectFlag(Type, 10))
				Data = GetByte();
			Object.Type = Type;
			Object.Data = Data;
			Object.Data2 = 0;
		}
		ChangeObject(rx, ry, rz, Position, Object);
		if (rz == GetMapLevel())
			UpdateAutomap(rx, ry);
	}
}

// 0x40d440, lines 1649-1661
void ReceiveDeleteObject()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = GetWord();
	ry = GetWord();
	rz = GetByte();
	const int Position = GetByte();
	AbsoluteToRelative(&rx, &ry, &rz);
	const TObject Object = GetMapObject(rx, ry, rz, Position);
	if (Object.Type == NoObject.Type) {
		TASSERT2(1649, FieldRefreshRequested(rx,ry,rz), rx, ry);
	} else {
		if (Object.Type == 0x63) {
			TCreature *Creature = GetCreature(Object.Data);
			if (Creature != nullptr)
				Creature->Visible = false;
		}
		const bool Refresh = DeleteObject(rx, ry, rz, Position);
		if (rz == GetMapLevel())
			UpdateAutomap(rx, ry);
		RelativeToAbsolute(&rx, &ry, &rz);
		if (Refresh)
			SendRefreshField(rx, ry, rz);
	}
}

// 0x415d40, lines 1879-1895
// Diagnostics may follow creature mutation; a missing creature record still moves the map object.
void ReceiveMoveCreature()
{
	int sx = GetWord();
	int sy = GetWord();
	int sz = GetByte();
	const int Position = GetByte();
	int zx = GetWord();
	int zy = GetWord();
	int zz = GetByte();
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
	AbsoluteToRelative(&sx, &sy, &sz);
	AbsoluteToRelative(&zx, &zy, &zz);
	const TObject Object = GetMapObject(sx, sy, sz, Position);
	std::string Error;
	TCreature *Creature = nullptr;
	if (Object.Type == NoObject.Type || Object.Type != 0x63) {
		if (FieldRefreshRequested(sx, sy, sz)) {
			RelativeToAbsolute(&zx, &zy, &zz);
			SendRefreshField(zx, zy, zz);
			return;
		}
		std::stringstream Text;
		Text << "MoveCreature has been received for a coordinate where there is no creature anymore [bug0000017]";
		if (Object.Type != NoObject.Type) {
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
		Creature = GetCreature(Object.Data);
		if (Creature != nullptr) {
			if (Teleport) {
				if (Creature->CreatureID == GetPlayerID())
					StopPlayer();
				if (dx > 0)
					Creature->Direction = 1;
				else if (dx < 0)
					Creature->Direction = 3;
				else if (dy < 0)
					Creature->Direction = 0;
				else if (dy > 0)
					Creature->Direction = 2;
				if (Creature->CreatureID != GetPlayerID())
					Creature->StartMovement(0, 0, 0);
			} else {
				const TObject Bank = GetFirstObject(zx, zy, zz);
				if (GetObjectFlag(Bank.Type, 0))
					Creature->StartMovement(dx, dy, GetObjectWaypoints(Bank.Type));
				else
					Error = "MoveCreature has been received for a coordinate where there is no bank as first object [bug0000484]";
			}
			Creature->RealX += dx;
			Creature->RealY += dy;
			Creature->RealZ += dz;
			CheckMonsterHint(Creature);
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
			TObject Current = GetFirstObject(sx, sy, sz);
			while (Current.Type != 0) {
				Text << " (" << Current.Type << "," << Current.Data << ")";
				Current = GetNextObject();
			}
			Text << std::endl
				 << " Start Cylinder (2): ";
			for (int i = 0; i < 10; i++) {
				Current = GetMapObject(sx, sy, sz, i);
				if (Current.Type == NoObject.Type)
					Text << "(-)";
				else
					Text << " (" << Current.Type << "," << Current.Data << ")";
			}
			Text << std::endl
				 << " Destination Cylinder (1): ";
			Current = GetFirstObject(zx, zy, zz);
			while (Current.Type != 0) {
				Text << " (" << Current.Type << "," << Current.Data << ")";
				Current = GetNextObject();
			}
			Text << std::endl
				 << " Destination Cylinder (2): ";
			for (int i = 0; i < 10; i++) {
				Current = GetMapObject(zx, zy, zz, i);
				if (Current.Type == NoObject.Type)
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
		throw TMYEXCEPTION(1879, Error.c_str(), "", 0, "", 0, Text.str().c_str());
	}
	if (DeleteObject(sx, sy, sz, Position)) {
		int x = sx;
		int y = sy;
		int z = sz;
		RelativeToAbsolute(&x, &y, &z);
		SendRefreshField(x, y, z);
	}
	if (sz == GetMapLevel())
		UpdateAutomap(sx, sy);
	AddObject(zx, zy, zz, Object);
	if (zz == GetMapLevel())
		UpdateAutomap(zx, zy);
}

// 0x40d7d0, lines 1922-1922
void ReceiveOpenContainer(int &Container)
{
	Container = GetByte();
	if (IsContainerOpen(Container))
		CloseContainer(Container);
	const int Type = GetWord();
	TFixedString<40> Name;
	GetString(patella::TSpan<char>(Name.Chars).first(30));
	const int Capacity = GetByte();
	const bool HasParent = GetByte() != 0;
	OpenContainer(Container, TObject{Type, 0, 0}, Name, Capacity, HasParent);
	const int Count = GetByte();
	for (int i = 0; i < Count; i++) {
		const int Type = GetWord();
		int Data = 0;
		if (GetObjectFlag(Type, 5))
			Data = GetByte();
		if (GetObjectFlag(Type, 11))
			Data = GetByte();
		if (GetObjectFlag(Type, 10))
			Data = GetByte();
		AppendContainerObject(Container, TObject{Type, (int)Data, 0});
	}
}

// 0x40da70, lines 1934-1934
void ReceiveCloseContainer(int &Container)
{
	Container = GetByte();
	if (IsContainerOpen(Container))
		CloseContainer(Container);
}

// 0x40dc20, lines 1953-1953
void ReceiveContainerAddObject()
{
	int Container = 0;
	int Type = 0;
	Container = GetByte();
	Type = GetWord();
	int Data = 0;
	if (GetObjectFlag(Type, 5))
		Data = GetByte();
	if (GetObjectFlag(Type, 11))
		Data = GetByte();
	if (GetObjectFlag(Type, 10))
		Data = GetByte();
	if (IsContainerOpen(Container))
		InsertContainerObject(Container, TObject{Type, (int)Data, 0});
}

// 0x40de40, lines 1974-1974
void ReceiveContainerChangeObject()
{
	int Container = 0;
	int Type = 0;
	Container = GetByte();
	const int Position = GetByte();
	Type = GetWord();
	int Data = 0;
	if (GetObjectFlag(Type, 5))
		Data = GetByte();
	if (GetObjectFlag(Type, 11))
		Data = GetByte();
	if (GetObjectFlag(Type, 10))
		Data = GetByte();
	if (IsContainerOpen(Container))
		ChangeContainerObject(Container, Position, TObject{Type, (int)Data, 0});
}

// 0x40e060, lines 1992-1992
void ReceiveContainerDeleteObject()
{
	int Container = 0;
	int Position = 0;
	Container = GetByte();
	Position = GetByte();
	if (IsContainerOpen(Container)) {
		if (DeleteContainerObject(Container, Position))
			SendRefreshContainer(Container);
	}
}

// 0x40e230, lines 2011-2011
void ReceiveInventoryObject()
{
	int Position = 0;
	int Type = 0;
	Position = GetByte();
	Type = GetWord();
	int Data = 0;
	if (GetObjectFlag(Type, 5))
		Data = GetByte();
	if (GetObjectFlag(Type, 11))
		Data = GetByte();
	if (GetObjectFlag(Type, 10))
		Data = GetByte();
	SetBodyObject(Position, TObject{Type, (int)Data, 0});
}

// 0x40e440, lines 2024-2024
void ReceiveDeleteInventoryObject()
{
	int Position = 0;
	Position = GetByte();
	DeleteBodyObject(Position);
}

// 0x40e5e0, lines 2046-2046
void ReceiveOwnTrade()
{
	TFixedString<40> Name;
	GetString(patella::TSpan<char>(Name.Chars).first(30));
	SetOwnOffer(Name);
	const int Count = GetByte();
	for (int i = 0; i < Count; i++) {
		const int Type = GetWord();
		int Data = 0;
		if (GetObjectFlag(Type, 5))
			Data = GetByte();
		if (GetObjectFlag(Type, 11))
			Data = GetByte();
		if (GetObjectFlag(Type, 10))
			Data = GetByte();
		AddOwnOfferObject(TObject{Type, (int)Data, 0});
	}
}

// 0x40e800, lines 2068-2068
void ReceiveCounterTrade()
{
	TFixedString<40> Name;
	GetString(patella::TSpan<char>(Name.Chars).first(30));
	const bool Open = SetCounterOffer(Name);
	const int Count = GetByte();
	for (int i = 0; i < Count; i++) {
		const int Type = GetWord();
		int Data = 0;
		if (GetObjectFlag(Type, 5))
			Data = GetByte();
		if (GetObjectFlag(Type, 11))
			Data = GetByte();
		if (GetObjectFlag(Type, 10))
			Data = GetByte();
		if (Open)
			AddCounterOfferObject(TObject{Type, (int)Data, 0});
	}
}

// 0x40ea30, lines 2079-2079
void ReceiveCloseTrade()
{
	ClearOffers();
}

// 0x40ebc0, lines 2092-2092
void ReceiveWorldLight()
{
	const int Brightness = GetByte();
	const int Color = GetByte();
	SetAmbientLight(Brightness, Color);
}

// 0x40ed60, lines 2111-2111
void ReceiveMagicEffect()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = GetWord();
	ry = GetWord();
	rz = GetByte();
	AbsoluteToRelative(&rx, &ry, &rz);
	const int Effect = GetByte();
	AddGraphicalEffect(rx, ry, rz, Effect);
}

// 0x40ef50, lines 2132-2132
void ReceiveAnimatedText()
{
	int rx = 0;
	int ry = 0;
	int rz = 0;
	rx = GetWord();
	ry = GetWord();
	rz = GetByte();
	AbsoluteToRelative(&rx, &ry, &rz);
	const int Color = GetByte();
	TFixedString<24> Text;
	GetString(patella::TSpan<char>(Text.Chars).first(10));
	AddTextualEffect(rx, ry, rz, Color, Text);
}

// 0x40f150, lines 2155-2155
void ReceiveDistanceEffect()
{
	int sx = 0;
	int sy = 0;
	int sz = 0;
	sx = GetWord();
	sy = GetWord();
	sz = GetByte();
	AbsoluteToRelative(&sx, &sy, &sz);
	int dx = GetWord();
	int dy = GetWord();
	int dz = GetByte();
	AbsoluteToRelative(&dx, &dy, &dz);
	const int Effect = GetByte();
	AddMissileEffect(sx, sy, sz, dx, dy, dz, Effect);
}

// 0x40f380, lines 2170-2170
void ReceiveCreatureMark()
{
	uint32 CreatureID = 0;
	CreatureID = GetQuad();
	TCreature *Creature = GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = GetByte();
		Creature->SetMark(Value);
		return;
	}
	GetByte();
}

// 0x40f550, lines 2185-2185
void ReceiveCreatureHealth()
{
	uint32 CreatureID = 0;
	CreatureID = GetQuad();
	TCreature *Creature = GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = GetByte();
		Creature->Health = Value;
		return;
	}
	GetByte();
}

// 0x40f710, lines 2205-2205
void ReceiveCreatureLight()
{
	uint32 CreatureID = 0;
	CreatureID = GetQuad();
	TCreature *Creature = GetCreature(CreatureID);
	if (Creature != nullptr) {
		Creature->LightStrength = GetByte();
		Creature->LightColor = GetByte();
		return;
	}
	GetByte();
	GetByte();
}

// 0x40f8e0, lines 2235-2235
void ReceiveCreatureOutfit()
{
	uint32 CreatureID = 0;
	CreatureID = GetQuad();
	TCreature *Creature = GetCreature(CreatureID);
	const int Type = GetWord();
	if (Creature == nullptr) {
		if (Type != 0) {
			GetByte();
			GetByte();
			GetByte();
			GetByte();
			return;
		}
		GetWord();
		return;
	}
	Creature->OutfitType = Type;
	if (Type != 0) {
		Creature->Colors[0] = GetByte();
		Creature->Colors[1] = GetByte();
		Creature->Colors[2] = GetByte();
		Creature->Colors[3] = GetByte();
		return;
	}
	Creature->Colors[0] = GetWord();
}

// 0x40fb20, lines 2250-2250
void ReceiveCreatureSpeed()
{
	uint32 CreatureID = 0;
	CreatureID = GetQuad();
	TCreature *Creature = GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = GetWord();
		Creature->Speed = Value;
		return;
	}
	GetWord();
}

// 0x40fce0, lines 2267-2267
void ReceiveCreatureSkull()
{
	uint32 CreatureID = 0;
	CreatureID = GetQuad();
	TCreature *Creature = GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Skull = GetByte();
		Creature->SetSkull(Skull, true);
		NotifySkull(Creature->PKFlag);
		return;
	}
	GetByte();
}

// 0x40fec0, lines 2283-2283
void ReceiveCreatureShield()
{
	uint32 CreatureID = 0;
	CreatureID = GetQuad();
	TCreature *Creature = GetCreature(CreatureID);
	if (Creature != nullptr) {
		const int Value = GetByte();
		Creature->PARTYFlag = Value;
		return;
	}
	GetByte();
}

// 0x410080, lines 2303-2303
void ReceiveEditText()
{
	uint32 *Info = new uint32[3];
	Info[0] = GetQuad();
	Info[1] = GetWord();
	Info[2] = GetWord();
	char *Text = GetString();
	char *Writer = GetString();
	char **Strings = new char *[2];
	Strings[0] = Writer;
	Strings[1] = Text;
	AddMessage(14, reinterpret_cast<INT_PTR>(Info), reinterpret_cast<INT_PTR>(Strings));
}

// 0x410260, lines 2318-2318
void ReceiveEditHouseText()
{
	uint32 *Info = new uint32[2];
	Info[0] = GetByte();
	Info[1] = GetQuad();
	const char *Text = GetString();
	AddMessage(15, reinterpret_cast<INT_PTR>(Info), reinterpret_cast<INT_PTR>(Text));
}

// 0x410410, lines 2347-2347
void ReceivePlayerStats()
{
	const int Health = GetWord();
	const int MaxHealth = GetWord();
	const int Capacity = GetWord();
	const uint32 Experience = GetQuad();
	const int Level = GetWord();
	const int LevelPercent = GetByte();
	const int Mana = GetWord();
	const int MaxMana = GetWord();
	const int MagicLevel = GetByte();
	const int MagicLevelPercent = GetByte();
	const int Soul = GetByte();
	SetPlayerHealth(Health, MaxHealth);
	SetPlayerCapacity(Capacity);
	SetPlayerMana(Mana, MaxMana);
	SetPlayerSoul(Soul);
	SetPlayerExperience(Experience, Level, LevelPercent);
	SetPlayerMagicLevel(MagicLevel, MagicLevelPercent);
}

// 0x410660, lines 2363-2363
void ReceivePlayerSkills()
{
	int i;
	for (i = 0; i < 7; i++) {
		const int Level = GetByte();
		const int Percent = GetByte();
		SetPlayerSkill(i, Level, Percent);
	}
}

// 0x410810, lines 2374-2374
void ReceivePlayerIcons()
{
	const int Icons = GetByte();
	SetPlayerState(Icons);
}

// 0x4109b0, lines 2386-2386
void ReceiveCancelAttack()
{
	SetAttackID(0);
}

// 0x410b40, lines 2461-2466
void ReceiveTalk()
{
	int Mode = 1;
	int Channel = 0;
	const uint32 StatementID = GetQuad();
	TFixedString<36> Name;
	GetString(patella::TSpan<char>(Name.Chars).first(30));
	Mode = GetByte();
	TFixedString<632> Text;
	int Time = 0;
	int TargetChannel = -1;
	switch (Mode) {
	case 1:
	case 2:
	case 3: {
		const int x = GetWord();
		const int y = GetWord();
		const int z = GetByte();
		GetString(patella::TSpan<char>(Text.Chars).first(512));
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
		GetString(patella::TSpan<char>(Text.Chars).first(512));
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
		Channel = GetWord();
		GetString(patella::TSpan<char>(Text.Chars).first(512));
		if (IsTalkIgnored(Mode, Name, -1)) {
			SetTalkThrottle();
			return;
		}
		TargetChannel = Channel;
		break;
	case 6:
		Time = -(int)GetQuad();
		GetString(patella::TSpan<char>(Text.Chars).first(512));
		break;
	case 7:
	case 8:
		GetString(patella::TSpan<char>(Text.Chars).first(512));
		break;
	case 16:
	case 17: {
		const int x = GetWord();
		const int y = GetWord();
		const int z = GetByte();
		GetString(patella::TSpan<char>(Text.Chars).first(512));
		if (!IsTalkIgnored(Mode, Name, -1)) {
			AddTalkbox(Mode, Name, x, y, z, Text);
			return;
		}
		SetTalkThrottle();
		return;
	}
	default:
		TERROR1_REASON(2461, "unknown talk mode", Mode, "[bug0000649]");
	}
	AddConsoleText(StatementID, Text, Mode, Name, TargetChannel, Time);
}

// 0x411030, lines 2487-2487
void ReceiveChannelList()
{
	int Count = 0;
	Count = GetByte();
	int *Channels = new int[Count + 1];
	char *Names = nullptr;
	if (Count != 0)
		Names = new char[Count * 30];
	Channels[0] = Count;
	for (int i = 0; i < Count; i++) {
		Channels[i + 1] = GetWord();
		GetString(patella::TSpan<char>(Names + i * 30, 30));
	}
	AddMessage(0x12, reinterpret_cast<INT_PTR>(Channels), reinterpret_cast<INT_PTR>(Names));
}

// 0x411230, lines 2502-2502
void ReceiveOpenChannel()
{
	int ChannelID = 0;
	ChannelID = GetWord();
	char *Name = new char[30];
	GetString(patella::TSpan<char>(Name, 30));
	AddMessage(48, ChannelID, reinterpret_cast<INT_PTR>(Name));
}

// 0x4113f0, lines 2515-2515
void ReceiveOpenPrivateConversation()
{
	char *Name = new char[30];
	GetString(patella::TSpan<char>(Name, 30));
	AddMessage(0x2f, 0, reinterpret_cast<INT_PTR>(Name));
}

// 0x4115a0, lines 2530-2530
void ReceiveRuleViolationChannel()
{
	int ChannelID = 0;
	ChannelID = GetWord();
	const TTalkBuffer *Channel = CreateTextBuffer(6, 1, nullptr, ChannelID);
	if (Channel == nullptr)
		Channel = GetTextBuffer(6, nullptr, -1);
	AddMessage(0x32, reinterpret_cast<INT_PTR>(Channel), 0);
}

// 0x411760, lines 2551-2551
void ReceiveRemoveRuleViolation()
{
	TFixedString<44> Name;
	GetString(patella::TSpan<char>(Name.Chars).first(30));
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
void ReceiveCancelRuleViolation()
{
	TFixedString<44> Name;
	GetString(patella::TSpan<char>(Name.Chars).first(30));
	TTalkBuffer *Channel = GetTextBuffer(8, Name, -1);
	if (Channel != nullptr) {
		TFixedString<444> Text;
		Text.Format("{} has finished the request.", Name);
		AddTextBufferLine(Channel, 0, Text, 18, nullptr, 0);
		Channel->SetWriteEnabled(false);
	}
}

// 0x411b20, lines 2584-2584
void ReceiveLockRuleViolation()
{
	TTalkBuffer *Channel = GetTextBuffer(7, nullptr, -1);
	if (Channel != nullptr) {
		const char *Text = TextRequestClosed;
		AddTextBufferLine(Channel, 0, Text, 18, nullptr, 0);
		Channel->SetWriteEnabled(false);
	}
}

// 0x411ce0, lines 2606-2606
void ReceiveOwnChannel()
{
	int ChannelID = 0;
	ChannelID = GetWord();
	char *Name = new char[40];
	GetString(patella::TSpan<char>(Name, 40));
	AddMessage(49, ChannelID, reinterpret_cast<INT_PTR>(Name));
}

// 0x411ea0, lines 2623-2623
void ReceiveCloseChannel()
{
	int ChannelID = 0;
	ChannelID = GetWord();
	TTalkBuffer *Channel = GetTextBuffer(5, nullptr, ChannelID);
	if (Channel != nullptr) {
		const char *Text = TextChannelClosed;
		AddTextBufferLine(Channel, 0, Text, 18, nullptr, 0);
		Channel->SetWriteEnabled(false);
	}
}

// 0x412060, lines 2634-2641
void ReceiveTextMessage()
{
	int Mode = 0;
	Mode = GetByte();
	TASSERT1(2634, Mode>=0&&Mode<TALK_MODE_BEYONDLAST, Mode);
	TFixedString<632> Text;
	GetString(patella::TSpan<char>(Text.Chars).first(512));
	if (Mode == 18 || Mode == 19 || Mode == 22)
		AddMessageTalkbox(Mode, Text);
	if (Mode == 23 || Mode == 21 || Mode == 20)
		SetStatusMessage(Text, false);
	if (Mode != 23)
		AddConsoleText(0, Text, Mode, nullptr, -1, 0);
}

// 0x416c40, lines 2672-2672
void ReceiveCancelWalk()
{
	const int Direction = GetByte();
	int x;
	int y;
	int z;
	GetPlayerPosition(&x, &y, &z);
	if (x == LastCancelX && y == LastCancelY && z == LastCancelZ)
		CancelCount++;
	else
		CancelCount = 0;
	LastCancelX = x;
	LastCancelY = y;
	LastCancelZ = z;
	if (CancelCount > 15) {
		ClearPlayerSteps();
		SetAttackID(0);
		SendCancel();
		CancelCount = 0;
	}
	if (CancelPlayerWalk(Direction))
		SendWalk();
}

// 0x416e60, lines 2685-2685
// 0x416e90 is this loop's interior basic block, misidentified as a second function.
void ReadFloor(int rz)
{
	// 0x416e90, lines 2685-2685. Interior loop, not a separately callable function.
	for (int rx = 0; rx < 18; rx++) {
		for (int ry = 0; ry < 14; ry++)
			ReadField(rx, ry, rz);
	}
}

// 0x417020, lines 2717-2717
void ReceiveFloorUp()
{
	int x;
	int y;
	int z;
	GetMapPosition(&x, &y, &z);
	z += -1;
	x += 1;
	y += 1;
	SetMapPosition(x, y, z);
	if (z < 8) {
		if (z == 7) {
			ShiftMapLevel(-3);
			SkipFields = 0;
			for (int rz = 2; rz < 8; rz++)
				ReadFloor(rz);
		}
	} else {
		ShiftMapLevel(-1);
		SkipFields = 0;
		ReadFloor(4);
	}
	SetTalkChanged();
	StopPlayer();
	CheckEventHelpPosition(x, y, z);
	for (int rx = 0; rx < 18; rx++) {
		for (int ry = 0; ry < 14; ry++)
			UpdateAutomap(rx, ry);
	}
}

// 0x417270, lines 2749-2749
void ReceiveFloorDown()
{
	int x;
	int y;
	int z;
	GetMapPosition(&x, &y, &z);
	z += 1;
	x += -1;
	y += -1;
	SetMapPosition(x, y, z);
	if (z < 9) {
		if (z == 8) {
			ShiftMapLevel(3);
			SkipFields = 0;
			for (int rz = 2; rz >= 0; rz--)
				ReadFloor(rz);
		}
	} else {
		ShiftMapLevel(1);
		SkipFields = 0;
		if (z < 14)
			ReadFloor(0);
	}
	SetTalkChanged();
	StopPlayer();
	CheckEventHelpPosition(x, y, z);
	for (int rx = 0; rx < 18; rx++) {
		for (int ry = 0; ry < 14; ry++)
			UpdateAutomap(rx, ry);
	}
}

// 0x412350, lines 2768-2768
void ReceiveOutfit()
{
	int *Outfit = new int[7];
	Outfit[0] = GetWord();
	Outfit[1] = GetByte();
	Outfit[2] = GetByte();
	Outfit[3] = GetByte();
	Outfit[4] = GetByte();
	Outfit[5] = GetWord();
	Outfit[6] = GetWord();
	AddMessage(16, reinterpret_cast<INT_PTR>(Outfit), 0);
}

// 0x412540, lines 2785-2785
// Only the byte value 1 means online, not every nonzero value.
void ReceiveBuddy()
{
	uint32 CharacterID = 0;
	CharacterID = GetQuad();
	TFixedString<44> Name;
	GetString(patella::TSpan<char>(Name.Chars).first(30));
	const int State = GetByte();
	AddBuddy(CharacterID, Name, State == 1);
	const char *Copy = MsvcStrdup(Name);
	AddMessage(25, reinterpret_cast<INT_PTR>(Copy), 0);
}

// 0x412720, lines 2798-2798
void ReceiveBuddyLogin()
{
	uint32 CharacterID = 0;
	CharacterID = GetQuad();
	SetBuddyState(CharacterID, true);
}

// 0x4128c0, lines 2811-2811
void ReceiveBuddyLogout()
{
	uint32 CharacterID = 0;
	CharacterID = GetQuad();
	SetBuddyState(CharacterID, false);
}

// Empty; the linker kept one copy of the identical empty functions (0x496160,
// GUI.obj). ExitControl calls it where Communication.cpp's exit belongs in the reverse
// order of InitControl (name ours, low confidence).
void ExitCommunication()
{
}
