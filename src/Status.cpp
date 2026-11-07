// Status.cpp: reconstructed from Tibia.exe 7.72 (0x4e6dc0-0x4ea860).
//
// The functions without line evidence (0x4e6dc0-0x4e7f00, all of them small
// accessors) come first, in binary order (the error report list's,
// 0x4e7380 and 0x4e7bb0-0x4e7ed0, and its vector at 0x71c648 are gone since
// Patella-Modern issue #74, decision D2); the rest follow the assertion and
// catch line numbers, which are also their binary order. The gap before the
// first assertion (line 91) belongs here because its globals (0x71c458-
// 0x71c64f) are interleaved with those of the asserted functions; only the
// skin table at its start is a separate file (Skins.cpp).
//
// Of all functions here only CheckClipboardText had a try block (its catch
// wrapper at line 869, gone since issue #78); the others have assertions only.

#include "Status.h"
#include "Utils.h"
#include "sysinc.h"
#include "types/Regexp.h"
#include "Objects.h"
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

// Characters owns its array as a std::unique_ptr<TCharacter[]> (issue #58): a pointer, as the original's.
static_assert(sizeof(std::unique_ptr<TCharacter[]>) == sizeof(TCharacter *)
		&& alignof(std::unique_ptr<TCharacter[]>) == alignof(TCharacter *),
	"std::unique_ptr<TCharacter[]> is a pointer");

// Status.obj's only .data, LinkPattern (0x5c2560), is its copy of Status.h's. The .bss (0x71c458-0x71c64f) is
// TInteraction's (the user interaction, issue #107) and TSession's (the session, 0x71c478-0x71c58b, issue #105), both
// in Status.h.


// 0x4e6dc0. Inlined into CheckRight. Only the first 32 rights are checked
// (the loop ends at 0x71c4f8), the ones GetRightName has names for; rights
// 32-49 never count.
bool patella::TSession::CheckAnyRight() const
{
	for (int i = 0; i < 32; i++) {
		if (Rights[i] != 0)
			return true;
	}
	return false;
}

// 0x4e6de0. Inlined into InitStatus.
void patella::TSession::ClearRights()
{
	for (int i = 0; i < RIGHT_ANY; i++)
		Rights[i] = 0;
}

// 0x4e6e00
int patella::TInteraction::GetActionNumber() const
{
	return ActionNumber;
}

// 0x4e6e10
void patella::TInteraction::IncrementActionNumber()
{
	ActionNumber++;
}

// 0x4e6e20. Values seen: 1 normal, 6 dragging, 7 use with, 8 trade with,
// 9, 10 and 11 (a modal window has the mouse).
void patella::TInteraction::SetActionState(int State)
{
	ActionState = State;
}

// 0x4e6e30
int patella::TInteraction::GetActionState() const
{
	return ActionState;
}

// 0x4e6e40
void patella::TInteraction::SetMousePosition(int x, int y)
{
	MouseX = x;
	MouseY = y;
}

// 0x4e6e60
int patella::TInteraction::GetMouseX() const
{
	return MouseX;
}

// 0x4e6e70
int patella::TInteraction::GetMouseY() const
{
	return MouseY;
}

// 0x4e6e80
void patella::TInteraction::SetMouseCursor(int Cursor)
{
	MouseCursor = Cursor;
}

// 0x4e6e90. The action state overrides the cursor that was set.
int patella::TInteraction::GetMouseCursor() const
{
	if (ActionState == 9)
		return 6;
	if (ActionState == 6)
		return 5;
	if (ActionState == 7 || ActionState == 8)
		return 4;
	return MouseCursor;
}

// 0x4e6ec0
void patella::TInteraction::SetPendingAction(int State, int Number)
{
	PendingActionState = State;
	PendingActionNumber = Number;
}

// 0x4e6ee0
int patella::TInteraction::GetPendingActionState() const
{
	return PendingActionState;
}

// 0x4e6ef0
int patella::TInteraction::GetPendingActionNumber() const
{
	return PendingActionNumber;
}

// 0x4e6f00
void patella::TInteraction::SetWindowSize(int Width, int Height)
{
	WindowWidth = Width;
	WindowHeight = Height;
}

// 0x4e6f20
int patella::TInteraction::GetWindowWidth() const
{
	return WindowWidth;
}

// 0x4e6f30
int patella::TInteraction::GetWindowHeight() const
{
	return WindowHeight;
}

// 0x4e6f40. Clears the drag object; SetDragObject sets it afterwards.
void patella::TInteraction::SetDragStart(int x, int y)
{
	DragStartX = x;
	DragStartY = y;
	DraggedObject.Type = 0;
	DraggedObject.Data = 0;
	DraggedObject.Data2 = 0;
}

// 0x4e6f70
void patella::TInteraction::SetDragObject(TObject Obj, int Index, int x, int y, int z)
{
	DraggedObject = Obj;
	DragIndex = Index;
	DragX = x;
	DragY = y;
	DragZ = z;
}

// 0x4e6fc0
void patella::TInteraction::SetDragTarget(int x, int y, int z)
{
	DragTargetX = x;
	DragTargetY = y;
	DragTargetZ = z;
}

// 0x4e6fe0. Starts dragging once the mouse is more than 3 pixels away from
// where the button went down.
void patella::TInteraction::CheckDragStart(int x, int y)
{
	if (ActionState != 6 && DraggedObject.Type != 0
		&& (abs(x - DragStartX) > 3 || abs(y - DragStartY) > 3))
		ActionState = 6;
}

// 0x4e7030
TObject *patella::TInteraction::GetDragObject(int *Index, int *x, int *y, int *z)
{
	*Index = DragIndex;
	*x = DragX;
	*y = DragY;
	*z = DragZ;
	return &DraggedObject;
}

// 0x4e7070
void patella::TInteraction::GetDragTarget(int *x, int *y, int *z) const
{
	*x = DragTargetX;
	*y = DragTargetY;
	*z = DragTargetZ;
}

// 0x4e70a0
void patella::TInteraction::SetTradeObject(TObject Obj, int Index, int x, int y, int z)
{
	TradeObject = Obj;
	TradeIndex = Index;
	TradeX = x;
	TradeY = y;
	TradeZ = z;
}

// 0x4e70f0
void patella::TInteraction::SetUseObject(TObject Obj, int Index, int x, int y, int z)
{
	UseObject = Obj;
	UseIndex = Index;
	UseX = x;
	UseY = y;
	UseZ = z;
}

// 0x4e7140. Set together with the trade and use objects; meaning unknown.
void patella::TInteraction::SetTradeInfo(int Info)
{
	TradeInfo = Info;
}

// 0x4e7150
TObject *patella::TInteraction::GetTradeObject(int *Index, int *x, int *y, int *z)
{
	*Index = TradeIndex;
	*x = TradeX;
	*y = TradeY;
	*z = TradeZ;
	return &TradeObject;
}

// 0x4e7190
TObject *patella::TInteraction::GetUseObject(int *Index, int *x, int *y, int *z)
{
	*Index = UseIndex;
	*x = UseX;
	*y = UseY;
	*z = UseZ;
	return &UseObject;
}

// 0x4e71d0
int patella::TInteraction::GetTradeInfo() const
{
	return TradeInfo;
}

// 0x4e71e0. 0 offline, 1 resolving the login server name, 2 connecting to a
// login server, 4 talking to the login server, 5 connecting to the game
// server; the game uses higher values (see ReceivePackets in Control.cpp).
void patella::TSession::SetConnectionState(int State)
{
	ConnectionState = State;
}

// 0x4e71f0
int patella::TSession::GetConnectionState() const
{
	return ConnectionState;
}

// 0x4e7200. Download progress in percent, -1 when there is none.
void patella::TSession::SetProgress(int Value)
{
	Progress = Value;
}

// 0x4e7210
int patella::TSession::GetProgress() const
{
	return Progress;
}

// 0x4e7220
int patella::TSession::GetAccountNumber() const
{
	return AccountNumber;
}

// 0x4e7230
const char *patella::TSession::GetAccountNumberString() const
{
	return AccountNumberString;
}

// 0x4e7240
const char *patella::TSession::GetAccountPassword() const
{
	return AccountPassword;
}

// 0x4e7250. Replaces the character list with Number empty entries.
void patella::TSession::SetAccountCharacters(int Number)
{
	AccountCharacters = Number;
	if (Characters != nullptr)
		Characters.reset();
	Characters = nullptr;
	if (Number > 0)
		Characters.reset(new TCharacter[Number]);
}

// 0x4e7290
void patella::TSession::SetPremiumDays(int Days)
{
	PremiumDays = Days;
}

// 0x4e72a0
int patella::TSession::GetAccountCharacters() const
{
	return AccountCharacters;
}

// 0x4e72b0
int patella::TSession::GetPremiumDays() const
{
	return PremiumDays;
}

// 0x4e72c0
int patella::TSession::GetActiveCharacter() const
{
	return ActiveCharacter;
}

// 0x4e72d0
bool patella::TSession::GetGamemaster() const
{
	return Gamemaster;
}

// 0x4e72e0. The text is not copied.
void patella::TInteraction::SetMouseOverText(const char *Text)
{
	MouseOverText = Text;
}

// 0x4e72f0
void patella::TInteraction::SetMouseOverTextPosition(int x, int y)
{
	MouseOverTextX = x;
	MouseOverTextY = y;
}

// 0x4e7310
const char *patella::TInteraction::GetMouseOverText() const
{
	return MouseOverText;
}

// 0x4e7320
void patella::TInteraction::GetMouseOverTextPosition(int *x, int *y) const
{
	*x = MouseOverTextX;
	*y = MouseOverTextY;
}

// 0x4e7340. Called once per main loop iteration.
void patella::TInteraction::UpdateBlinkState()
{
	const int Now = GetMilliseconds();
	if (BlinkTime + 300 < Now) {
		BlinkState = !BlinkState;
		BlinkTime = Now;
	}
}

// 0x4e7370
bool patella::TInteraction::GetBlinkState() const
{
	return BlinkState;
}

// 0x4e75d0. Type of the first message, 0 when the queue is empty.
int patella::TInteraction::PeekMessageType()
{
	if (Messages.empty())
		return 0;
	return Messages.front().Type;
}

// 0x4e75f0. Removes the first message; all zero when the queue is empty.
// The parameters now belong to the caller.
void patella::TInteraction::FetchMessage(int *Type, std::intptr_t *Param1, std::intptr_t *Param2)
{
	if (!Messages.empty()) {
		*Type = Messages.front().Type;
		*Param1 = Messages.front().Param1;
		*Param2 = Messages.front().Param2;
		Messages.erase(Messages.begin());
		return;
	}
	*Type = 0;
	*Param1 = 0;
	*Param2 = 0;
}

// 0x4e76e0. Frees the parameters that each message type owns. Type 14's
// Param2 points to two more blocks.
void patella::TInteraction::ClearMessages()
{
	for (const TMessage &Message : Messages) {
		const int Type = Message.Type;
		void *Param1 = reinterpret_cast<void *>(Message.Param1);
		void *Param2 = reinterpret_cast<void *>(Message.Param2);
		switch (Type) {
		case 1:
		case 2:
		case 47:
		case 48:
		case 49:
		case 50:
			delete[] static_cast<char *>(Param2);
			break;
		case 5:
		case 6:
		case 7:
		case 16:
		case 25:
		case 26:
		case 51:
			delete[] static_cast<char *>(Param1);
			break;
		case 14:
			delete[] static_cast<char *>(Param1);
			delete[] (static_cast<char **>(Param2))[0];
			delete[] (static_cast<char **>(Param2))[1];
			delete[] static_cast<char *>(Param2);
			break;
		case 15:
			delete[] static_cast<char *>(Param1);
			delete[] static_cast<char *>(Param2);
			break;
		case 18:
			delete[] static_cast<char *>(Param1);
			if (Param2 != nullptr)
				delete[] static_cast<char *>(Param2);
			break;
		case 28:
			if (Param1 != nullptr)
				delete[] static_cast<char *>(Param1);
			if (Param2 != nullptr)
				delete[] static_cast<char *>(Param2);
			break;
		default:
			break;
		}
	}
	Messages.clear();
}

// 0x4e7f00. TaintedClipboardText is freed but not cleared, and the queued
// messages are not freed. The original cleared the error reports first (gone
// since issue #74). LinkRegexp is freed and, since issue #129 (TRegexp),
// cleared too: the original left it dangling until InitStatus.
void patella::TInteraction::ExitStatus()
{
	patella::bridge::Session().ExitSession(); // the character list (issue #105)
	if (TaintedClipboardText != nullptr)
		delete[] TaintedClipboardText;
	LinkRegexp.Reset();
}

// 0x4e8580, line 91
void patella::TSession::SetRight(int rightid, int Value)
{
	PATELLA_ASSERT_VALUES(In(rightid,0,RIGHT_ANY-1), rightid);
	Rights[rightid] = Value;
}

// 0x4e86e0, line 100. Bit -1 asks for any bit. The RIGHT_ANY branch cannot
// be reached after the assertion; it is in the binary.
bool patella::TSession::CheckRight(int rightid, int Bit) const
{
	PATELLA_ASSERT_VALUES(In(rightid,0,RIGHT_ANY-1), rightid);
	if (rightid == RIGHT_ANY)
		return CheckAnyRight();
	if (Bit == -1)
		return Rights[rightid] != 0;
	return (Rights[rightid] & (1 << Bit)) != 0;
}

// 0x4e88c0, line 165. Reasons of the gamemaster report dialog.
const char *GetRightName(int rightid)
{
	switch (rightid) {
	case 0: return "1a) Offensive name";
	case 1: return "1b) Name containing part of sentence";
	case 2: return "1b) Name with nonsensical letter combination";
	case 3: return "1b) Invalid name format";
	case 4: return "1c) Name not describing person";
	case 5: return "1c) Name of celebrity";
	case 6: return "1c) Name referring to country";
	case 7: return "1c) Name to fake player identity";
	case 8: return "1c) Name to fake official position";
	case 9: return "2a) Offensive statement";
	case 10: return "2b) Spamming";
	case 11: return "2c) Advertisement not related to game";
	case 12: return "2c) Real money advertisement";
	case 13: return "2d) Non-English public statement";
	case 14: return "2d) Off-topic public statement";
	case 15: return "2e) Inciting rule violation";
	case 16: return "3a) Bug abuse";
	case 17: return "3b) Game weakness abuse";
	case 18: return "3c) Macro use";
	case 19: return "3d) Using unofficial software to play";
	case 20: return "3e) Hacking";
	case 21: return "3f) Multi-clienting";
	case 22: return "3g) Account trading";
	case 23: return "3g) Account sharing";
	case 24: return "4a) Threatening gamemaster";
	case 25: return "4b) Pretending to have official position";
	case 26: return "4b) Pretending to have influence on gamemaster";
	case 27: return "4c) False report to gamemaster";
	case 28: return "Excessive unjustified player killing";
	case 29: return "Destructive behaviour";
	case 30: return "Spoiling auction";
	case 31: return "Invalid payment";
	default:
		TERROR1("unknown right", rightid);
	}
}

// 0x4e8de0, line 183
const char *GetActionName(int actionid)
{
	switch (actionid) {
	case 0: return "Notation";
	case 1: return "Name Report";
	case 2: return "Banishment";
	case 3: return "Name Report + Banishment";
	case 4: return "Banishment + Final Warning";
	case 5: return "Name Report + Banishment + Final Warning";
	case 6: return "Statement Report";
	case 7: return "IP Banishment";
	default:
		TERROR1("unknown action", actionid);
	}
}

// 0x4e9020, line 197. Appends a message; the queue takes ownership of the
// parameters that ClearMessages frees.
void patella::TInteraction::AddMessage(int Type, std::intptr_t Param1, std::intptr_t Param2)
{
	PATELLA_ASSERT_VALUES(In(Type,MESSAGE_NONE,MESSAGE_STARTPATCHER), Type);
	Messages.push_back({Type, Param1, Param2});
}

// 0x4e91d0, line 209. Inserts a message at the front of the queue.
void patella::TInteraction::AddMessageFirst(int Type, std::intptr_t Param1, std::intptr_t Param2)
{
	PATELLA_ASSERT_VALUES(In(Type,MESSAGE_NONE,MESSAGE_STARTPATCHER), Type);
	Messages.insert(Messages.begin(), {Type, Param1, Param2});
}

// 0x4e93d0, lines 621-622
void patella::TSession::SetAccount(int Number, const char *Password)
{
	PATELLA_ASSERT(Password!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Password)<30, strlen(Password));
	AccountNumber = Number;
	TStringBuffer(AccountNumberString, sizeof(AccountNumberString)).Format("{}", Number); // at most 11 characters
	AccountNumberString[11] = 0;
	// The original's copy, up to the buffer's 30 bytes (the assertion keeps the password shorter; GCC's
	// -Wstringop-truncation sees the bound only, issue #111).
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wstringop-truncation\"")
	strncpy(AccountPassword, Password, 30);
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")
}

// 0x4e96d0, lines 669-673. Address is the IPv4 address in network order.
void patella::TSession::SetCharacter(int Number, const char *Name, const char *World, const uint8 *Address, int Port)
{
	PATELLA_ASSERT_VALUES(In(Number,0,AccountCharacters-1), Number);
	PATELLA_ASSERT(Name!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Name)<30, strlen(Name));
	PATELLA_ASSERT(World!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(World)<30, strlen(World));
	TCharacter *Character = &Characters[Number];
	Character->Name.Assign(Name);
	Character->World.Assign(World);
	Character->IP = *reinterpret_cast<const uint32 *>(Address);
	TStringBuffer(Character->IPAddress, sizeof(Character->IPAddress))
		.Format("{}.{}.{}.{}", Address[0], Address[1], Address[2], Address[3]); // at most 15 characters
	Character->IPAddress[15] = 0;
	Character->Port = Port;
}

// 0x4e9c50, line 696
void patella::TSession::SetActiveCharacter(int Number)
{
	PATELLA_ASSERT_VALUES(In(Number,0,AccountCharacters-1), Number);
	ActiveCharacter = Number;
}

// 0x4e9db0, line 713
const char *patella::TSession::GetCharacterName(int Number) const
{
	PATELLA_ASSERT_VALUES(In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].Name;
}

// 0x4e9f20, line 722
const char *patella::TSession::GetCharacterWorld(int Number) const
{
	PATELLA_ASSERT_VALUES(In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].World;
}

// 0x4ea090, line 740
const char *patella::TSession::GetCharacterIPAddress(int Number) const
{
	PATELLA_ASSERT_VALUES(In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].IPAddress;
}

// 0x4ea200, line 749
int patella::TSession::GetCharacterPort(int Number) const
{
	PATELLA_ASSERT_VALUES(In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].Port;
}

// 0x4ea370, line 869. Keeps a copy of the text; if its lower-case form
// contains something like a web or IP address, the link warning handler is
// called instead of copying (it can copy later with
// CopyTaintedClipboardText). Text and Length are the span's pointer and its size as an int (issue #69).
void patella::TInteraction::CheckClipboardText(patella::TSpan<const char> Characters)
{
	const char *const Text = Characters.data();
	const int Length = static_cast<int>(Characters.size());
	if (TaintedClipboardText != nullptr)
		delete[] TaintedClipboardText;
	TaintedClipboardText = new char[Length + 1];
	memcpy(TaintedClipboardText, Text, Length);
	TaintedClipboardText[Length] = 0;
	char *Lower = new char[Length + 1];
	memcpy(Lower, TaintedClipboardText, Length + 1);
	LowerString(Lower);
	const int Match = LinkRegexp.Execute(Lower);
	delete[] Lower;
	if (Match == 1) {
		LinkWarningEventHandler();
		return;
	}
	SetClipboardText(TaintedClipboardText);
}

// 0x4ea5b0, line 875
void patella::TInteraction::CopyToClipboard(const char *text)
{
	PATELLA_ASSERT(text);
	CheckClipboardText(patella::TSpan<const char>(text, strlen(text)));
}

// 0x4ea710, line 882
void patella::TInteraction::CopyTaintedClipboardText()
{
	PATELLA_ASSERT(TaintedClipboardText);
	SetClipboardText(TaintedClipboardText);
}

// 0x4ea860, line 1051. Not everything is reset: the mouse, window and drag
// positions keep their values.
void patella::TInteraction::InitStatus(bool Gamemaster, void (*linkwarningeventhandler)())
{
	PATELLA_ASSERT(linkwarningeventhandler!=nullptr);
	LinkWarningEventHandler = linkwarningeventhandler;
	TaintedClipboardText = nullptr;
	// The original set LinkRegexp to NULL first, so a second InitStatus without ExitStatus leaked the program;
	// TRegexp's assignment frees it (issue #129).
	LinkRegexp = patella::TRegexp(LinkPattern);
	patella::bridge::Session().InitSession(Gamemaster); // the session's part (issue #105), in its order below
	Messages.clear();                                   // the parameters not freed, as the original
	ActionNumber = 0;
	DraggedObject.Type = 0;
	DraggedObject.Data = 0;
	DraggedObject.Data2 = 0;
	TradeObject.Type = 0;
	TradeObject.Data = 0;
	TradeObject.Data2 = 0;
	UseObject.Type = 0;
	UseObject.Data = 0;
	UseObject.Data2 = 0;
	TradeInfo = 0;
	ActionState = 0;
	MouseOverText = nullptr;
	PendingActionState = 0;
	PendingActionNumber = 0;
}

// InitStatus's part (0x4ea860, issue #105): the session's members in the order InitStatus wrote them (Gamemaster and
// the rights first, ClearRights inlined there; the rest after its other data).
void patella::TSession::InitSession(bool Value)
{
	Gamemaster = Value;
	ClearRights();
	ConnectionState = 0;
	AccountNumber = 0;
	AccountPassword[0] = 0;
	AccountCharacters = 0;
	Characters = nullptr;
	PremiumDays = 0;
	ActiveCharacter = 0;
	Progress = -1;
}

// ExitStatus's part (0x4e7f00, issue #105): the character list, freed first.
void patella::TSession::ExitSession()
{
	if (Characters != nullptr)
		Characters.reset();
	Characters = nullptr;
}

#if defined(PATELLA_TEST_MODE)
// The test mode's state dump (issue #22; src/testmode/ClientView.h) reads this file's data; it writes
// nothing. Test build only.
const int *patella::testmode::ViewConnectionState()
{
	return patella::bridge::Session().ViewConnectionState();
}

// The character list (issue #92: the control channel's probe, tools/client_state.py's characters()).
void patella::testmode::ViewCharacters(TCharactersView *View)
{
	const patella::TSession &Session = patella::bridge::Session();
	View->Characters = Session.ViewCharacters();
	View->Count = Session.ViewAccountCharacters();
}
#endif

// The session's functions of the original (issue #105): each calls its member on the one TClient's TSession.

bool CheckAnyRight()
{
	return patella::bridge::Session().CheckAnyRight();
}

void ClearRights()
{
	patella::bridge::Session().ClearRights();
}

void SetConnectionState(int State)
{
	patella::bridge::Session().SetConnectionState(State);
}

int GetConnectionState()
{
	return patella::bridge::Session().GetConnectionState();
}

void SetProgress(int Progress)
{
	patella::bridge::Session().SetProgress(Progress);
}

int GetProgress()
{
	return patella::bridge::Session().GetProgress();
}

int GetAccountNumber()
{
	return patella::bridge::Session().GetAccountNumber();
}

const char *GetAccountNumberString()
{
	return patella::bridge::Session().GetAccountNumberString();
}

const char *GetAccountPassword()
{
	return patella::bridge::Session().GetAccountPassword();
}

void SetAccountCharacters(int Number)
{
	patella::bridge::Session().SetAccountCharacters(Number);
}

void SetPremiumDays(int Days)
{
	patella::bridge::Session().SetPremiumDays(Days);
}

int GetAccountCharacters()
{
	return patella::bridge::Session().GetAccountCharacters();
}

int GetPremiumDays()
{
	return patella::bridge::Session().GetPremiumDays();
}

int GetActiveCharacter()
{
	return patella::bridge::Session().GetActiveCharacter();
}

bool GetGamemaster()
{
	return patella::bridge::Session().GetGamemaster();
}

void SetRight(int rightid, int Value)
{
	patella::bridge::Session().SetRight(rightid, Value);
}

bool CheckRight(int rightid, int Bit)
{
	return patella::bridge::Session().CheckRight(rightid, Bit);
}

void SetAccount(int Number, const char *Password)
{
	patella::bridge::Session().SetAccount(Number, Password);
}

void SetCharacter(int Number, const char *Name, const char *World, const uint8 *Address, int Port)
{
	patella::bridge::Session().SetCharacter(Number, Name, World, Address, Port);
}

void SetActiveCharacter(int Number)
{
	patella::bridge::Session().SetActiveCharacter(Number);
}

const char *GetCharacterName(int Number)
{
	return patella::bridge::Session().GetCharacterName(Number);
}

const char *GetCharacterWorld(int Number)
{
	return patella::bridge::Session().GetCharacterWorld(Number);
}

const char *GetCharacterIPAddress(int Number)
{
	return patella::bridge::Session().GetCharacterIPAddress(Number);
}

int GetCharacterPort(int Number)
{
	return patella::bridge::Session().GetCharacterPort(Number);
}

// The functions of the original (issue #107): each calls its member on the one TClient's TInteraction.

int GetActionNumber()
{
	return patella::bridge::Interaction().GetActionNumber();
}

void IncrementActionNumber()
{
	patella::bridge::Interaction().IncrementActionNumber();
}

void SetActionState(int State)
{
	patella::bridge::Interaction().SetActionState(State);
}

int GetActionState()
{
	return patella::bridge::Interaction().GetActionState();
}

void SetMousePosition(int x, int y)
{
	patella::bridge::Interaction().SetMousePosition(x, y);
}

int GetMouseX()
{
	return patella::bridge::Interaction().GetMouseX();
}

int GetMouseY()
{
	return patella::bridge::Interaction().GetMouseY();
}

void SetMouseCursor(int Cursor)
{
	patella::bridge::Interaction().SetMouseCursor(Cursor);
}

int GetMouseCursor()
{
	return patella::bridge::Interaction().GetMouseCursor();
}

void SetPendingAction(int State, int Number)
{
	patella::bridge::Interaction().SetPendingAction(State, Number);
}

int GetPendingActionState()
{
	return patella::bridge::Interaction().GetPendingActionState();
}

int GetPendingActionNumber()
{
	return patella::bridge::Interaction().GetPendingActionNumber();
}

void SetWindowSize(int Width, int Height)
{
	patella::bridge::Interaction().SetWindowSize(Width, Height);
}

int GetWindowWidth()
{
	return patella::bridge::Interaction().GetWindowWidth();
}

int GetWindowHeight()
{
	return patella::bridge::Interaction().GetWindowHeight();
}

void SetDragStart(int x, int y)
{
	patella::bridge::Interaction().SetDragStart(x, y);
}

void SetDragObject(TObject Obj, int Index, int x, int y, int z)
{
	patella::bridge::Interaction().SetDragObject(Obj, Index, x, y, z);
}

void SetDragTarget(int x, int y, int z)
{
	patella::bridge::Interaction().SetDragTarget(x, y, z);
}

void CheckDragStart(int x, int y)
{
	patella::bridge::Interaction().CheckDragStart(x, y);
}

TObject *GetDragObject(int *Index, int *x, int *y, int *z)
{
	return patella::bridge::Interaction().GetDragObject(Index, x, y, z);
}

void GetDragTarget(int *x, int *y, int *z)
{
	patella::bridge::Interaction().GetDragTarget(x, y, z);
}

void SetTradeObject(TObject Obj, int Index, int x, int y, int z)
{
	patella::bridge::Interaction().SetTradeObject(Obj, Index, x, y, z);
}

void SetUseObject(TObject Obj, int Index, int x, int y, int z)
{
	patella::bridge::Interaction().SetUseObject(Obj, Index, x, y, z);
}

void SetTradeInfo(int Info)
{
	patella::bridge::Interaction().SetTradeInfo(Info);
}

TObject *GetTradeObject(int *Index, int *x, int *y, int *z)
{
	return patella::bridge::Interaction().GetTradeObject(Index, x, y, z);
}

TObject *GetUseObject(int *Index, int *x, int *y, int *z)
{
	return patella::bridge::Interaction().GetUseObject(Index, x, y, z);
}

int GetTradeInfo()
{
	return patella::bridge::Interaction().GetTradeInfo();
}

void SetMouseOverText(const char *Text)
{
	patella::bridge::Interaction().SetMouseOverText(Text);
}

void SetMouseOverTextPosition(int x, int y)
{
	patella::bridge::Interaction().SetMouseOverTextPosition(x, y);
}

const char *GetMouseOverText()
{
	return patella::bridge::Interaction().GetMouseOverText();
}

void GetMouseOverTextPosition(int *x, int *y)
{
	patella::bridge::Interaction().GetMouseOverTextPosition(x, y);
}

void UpdateBlinkState()
{
	patella::bridge::Interaction().UpdateBlinkState();
}

bool GetBlinkState()
{
	return patella::bridge::Interaction().GetBlinkState();
}

int PeekMessageType()
{
	return patella::bridge::Interaction().PeekMessageType();
}

void FetchMessage(int *Type, std::intptr_t *Param1, std::intptr_t *Param2)
{
	patella::bridge::Interaction().FetchMessage(Type, Param1, Param2);
}

void ClearMessages()
{
	patella::bridge::Interaction().ClearMessages();
}

void ExitStatus()
{
	patella::bridge::Interaction().ExitStatus();
}

void AddMessage(int Type, std::intptr_t Param1, std::intptr_t Param2)
{
	patella::bridge::Interaction().AddMessage(Type, Param1, Param2);
}

void AddMessageFirst(int Type, std::intptr_t Param1, std::intptr_t Param2)
{
	patella::bridge::Interaction().AddMessageFirst(Type, Param1, Param2);
}

void CheckClipboardText(patella::TSpan<const char> Characters)
{
	patella::bridge::Interaction().CheckClipboardText(Characters);
}

void CopyToClipboard(const char *text)
{
	patella::bridge::Interaction().CopyToClipboard(text);
}

void CopyTaintedClipboardText()
{
	patella::bridge::Interaction().CopyTaintedClipboardText();
}

void InitStatus(bool Gamemaster, void (*linkwarningeventhandler)())
{
	patella::bridge::Interaction().InitStatus(Gamemaster, linkwarningeventhandler);
}
// NOLINTEND(cppcoreguidelines-owning-memory)
