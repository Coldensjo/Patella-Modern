// Status.cpp: reconstructed from Tibia.exe 7.72 (0x4e6dc0-0x4ea860).
//
// The functions without line evidence (0x4e6dc0-0x4e7f00, all of them small
// accessors) come first, in binary order; the rest follow the assertion and
// catch line numbers, which are also their binary order. The gap before the
// first assertion (line 91) belongs here because its globals (0x71c458-
// 0x71c64f) are interleaved with those of the asserted functions; only the
// skin table at its start is a separate file (Skins.cpp).
//
// Of all functions here only CheckClipboardText has a try block (TBEGIN ...
// TEND at line 869); the others have assertions only.

#include "Status.h"
#include "Utils.h"
#include "sysinc.h"
#include "Extern/regexp/regexp.h"
#include "Container.h"
#include "Objects.h"
#include "common.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace {

// Data, in .bss order. Status.obj's only .data, LinkPattern (0x5c2560), is
// its copy of Status.h's.
regexp *LinkRegexp;                         // 0x71c458, LinkPattern compiled
char *TaintedClipboardText;                 // 0x71c45c, name from an assertion
void (*LinkWarningEventHandler)();          // 0x71c460, from "linkwarningeventhandler!=NULL"
bool BlinkState;                            // 0x71c464, toggled every 300 ms
int BlinkTime;                              // 0x71c468
int MouseOverTextY;                         // 0x71c46c
int MouseOverTextX;                         // 0x71c470
const char *MouseOverText;                  // 0x71c474
int Rights[RIGHT_ANY];                      // 0x71c478
bool Gamemaster;                            // 0x71c540, sent in the game login packet
int PremiumDays;                            // 0x71c544
int ActiveCharacter;                        // 0x71c548
TCharacter *Characters;                     // 0x71c54c
int AccountCharacters;                      // 0x71c550, name from an assertion
char AccountPassword[30];                   // 0x71c554
char AccountNumberString[12];               // 0x71c574
int AccountNumber;                          // 0x71c580
int Progress;                               // 0x71c584, -1 = none
int ConnectionState;                        // 0x71c588, see SetConnectionState
int TradeInfo;                              // 0x71c58c
int UseZ;                                   // 0x71c590
int UseY;                                   // 0x71c594
int UseX;                                   // 0x71c598
int UseIndex;                               // 0x71c59c
int TradeZ;                                 // 0x71c5a0
int TradeY;                                 // 0x71c5a4
int TradeX;                                 // 0x71c5a8
int TradeIndex;                             // 0x71c5ac
int DragTargetZ;                            // 0x71c5b0
int DragTargetY;                            // 0x71c5b4
int DragTargetX;                            // 0x71c5b8
int DragZ;                                  // 0x71c5bc
int DragY;                                  // 0x71c5c0
int DragX;                                  // 0x71c5c4
int DragIndex;                              // 0x71c5c8
int DragStartY;                             // 0x71c5cc
int DragStartX;                             // 0x71c5d0
int PendingActionNumber;                    // 0x71c5d4
int PendingActionState;                     // 0x71c5d8
int WindowHeight;                           // 0x71c5dc
int WindowWidth;                            // 0x71c5e0
int MouseCursor;                            // 0x71c5e4
int ActionState;                            // 0x71c5e8, from "GetActionState()" in Control.cpp
int ActionNumber;                           // 0x71c5ec
int NumberOfMessages;                       // 0x71c5f0
vector<TMessage> Messages(0, 9, 10);        // 0x71c5f4, initialiser at 0x55a0f0
int MouseX;                                 // 0x71c61c
int MouseY;                                 // 0x71c620
TObject DraggedObject;                      // 0x71c624
TObject TradeObject;                        // 0x71c630, from "TradeObject!=0" in MapWindow.cpp
TObject UseObject;                          // 0x71c63c, "SrcObj" in MapWindow.cpp
vector<TErrorReport> ErrorReports(0, 0, 1); // 0x71c648, initialiser at 0x55a110

} // namespace

// 0x4e6dc0. Inlined into CheckRight. Only the first 32 rights are checked
// (the loop ends at 0x71c4f8), the ones GetRightName has names for; rights
// 32-49 never count.
bool CheckAnyRight()
{
	for (int i = 0; i < 32; i++) {
		if (Rights[i] != 0)
			return true;
	}
	return false;
}

// 0x4e6de0. Inlined into InitStatus.
void ClearRights()
{
	for (int i = 0; i < RIGHT_ANY; i++)
		Rights[i] = 0;
}

// 0x4e6e00
int GetActionNumber()
{
	return ActionNumber;
}

// 0x4e6e10
void IncrementActionNumber()
{
	ActionNumber++;
}

// 0x4e6e20. Values seen: 1 normal, 6 dragging, 7 use with, 8 trade with,
// 9, 10 and 11 (a modal window has the mouse).
void SetActionState(int State)
{
	ActionState = State;
}

// 0x4e6e30
int GetActionState()
{
	return ActionState;
}

// 0x4e6e40
void SetMousePosition(int x, int y)
{
	MouseX = x;
	MouseY = y;
}

// 0x4e6e60
int GetMouseX()
{
	return MouseX;
}

// 0x4e6e70
int GetMouseY()
{
	return MouseY;
}

// 0x4e6e80
void SetMouseCursor(int Cursor)
{
	MouseCursor = Cursor;
}

// 0x4e6e90. The action state overrides the cursor that was set.
int GetMouseCursor()
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
void SetPendingAction(int State, int Number)
{
	PendingActionState = State;
	PendingActionNumber = Number;
}

// 0x4e6ee0
int GetPendingActionState()
{
	return PendingActionState;
}

// 0x4e6ef0
int GetPendingActionNumber()
{
	return PendingActionNumber;
}

// 0x4e6f00
void SetWindowSize(int Width, int Height)
{
	WindowWidth = Width;
	WindowHeight = Height;
}

// 0x4e6f20
int GetWindowWidth()
{
	return WindowWidth;
}

// 0x4e6f30
int GetWindowHeight()
{
	return WindowHeight;
}

// 0x4e6f40. Clears the drag object; SetDragObject sets it afterwards.
void SetDragStart(int x, int y)
{
	DragStartX = x;
	DragStartY = y;
	DraggedObject.Type = 0;
	DraggedObject.Data = 0;
	DraggedObject.Data2 = 0;
}

// 0x4e6f70
void SetDragObject(TObject Obj, int Index, int x, int y, int z)
{
	DraggedObject = Obj;
	DragIndex = Index;
	DragX = x;
	DragY = y;
	DragZ = z;
}

// 0x4e6fc0
void SetDragTarget(int x, int y, int z)
{
	DragTargetX = x;
	DragTargetY = y;
	DragTargetZ = z;
}

// 0x4e6fe0. Starts dragging once the mouse is more than 3 pixels away from
// where the button went down.
void CheckDragStart(int x, int y)
{
	if (ActionState != 6 && DraggedObject.Type != 0
		&& (abs(x - DragStartX) > 3 || abs(y - DragStartY) > 3))
		ActionState = 6;
}

// 0x4e7030
TObject *GetDragObject(int *Index, int *x, int *y, int *z)
{
	*Index = DragIndex;
	*x = DragX;
	*y = DragY;
	*z = DragZ;
	return &DraggedObject;
}

// 0x4e7070
void GetDragTarget(int *x, int *y, int *z)
{
	*x = DragTargetX;
	*y = DragTargetY;
	*z = DragTargetZ;
}

// 0x4e70a0
void SetTradeObject(TObject Obj, int Index, int x, int y, int z)
{
	TradeObject = Obj;
	TradeIndex = Index;
	TradeX = x;
	TradeY = y;
	TradeZ = z;
}

// 0x4e70f0
void SetUseObject(TObject Obj, int Index, int x, int y, int z)
{
	UseObject = Obj;
	UseIndex = Index;
	UseX = x;
	UseY = y;
	UseZ = z;
}

// 0x4e7140. Set together with the trade and use objects; meaning unknown.
void SetTradeInfo(int Info)
{
	TradeInfo = Info;
}

// 0x4e7150
TObject *GetTradeObject(int *Index, int *x, int *y, int *z)
{
	*Index = TradeIndex;
	*x = TradeX;
	*y = TradeY;
	*z = TradeZ;
	return &TradeObject;
}

// 0x4e7190
TObject *GetUseObject(int *Index, int *x, int *y, int *z)
{
	*Index = UseIndex;
	*x = UseX;
	*y = UseY;
	*z = UseZ;
	return &UseObject;
}

// 0x4e71d0
int GetTradeInfo()
{
	return TradeInfo;
}

// 0x4e71e0. 0 offline, 1 resolving the login server name, 2 connecting to a
// login server, 4 talking to the login server, 5 connecting to the game
// server; the game uses higher values (see ReceivePackets in Control.cpp).
void SetConnectionState(int State)
{
	ConnectionState = State;
}

// 0x4e71f0
int GetConnectionState()
{
	return ConnectionState;
}

// 0x4e7200. Download progress in percent, -1 when there is none.
void SetProgress(int Progress)
{
	::Progress = Progress;
}

// 0x4e7210
int GetProgress()
{
	return Progress;
}

// 0x4e7220
int GetAccountNumber()
{
	return AccountNumber;
}

// 0x4e7230
const char *GetAccountNumberString()
{
	return AccountNumberString;
}

// 0x4e7240
const char *GetAccountPassword()
{
	return AccountPassword;
}

// 0x4e7250. Replaces the character list with Number empty entries.
void SetAccountCharacters(int Number)
{
	AccountCharacters = Number;
	if (Characters != nullptr)
		delete[] Characters;
	Characters = nullptr;
	if (Number > 0)
		Characters = new TCharacter[Number];
}

// 0x4e7290
void SetPremiumDays(int Days)
{
	PremiumDays = Days;
}

// 0x4e72a0
int GetAccountCharacters()
{
	return AccountCharacters;
}

// 0x4e72b0
int GetPremiumDays()
{
	return PremiumDays;
}

// 0x4e72c0
int GetActiveCharacter()
{
	return ActiveCharacter;
}

// 0x4e72d0
bool GetGamemaster()
{
	return Gamemaster;
}

// 0x4e72e0. The text is not copied.
void SetMouseOverText(const char *Text)
{
	MouseOverText = Text;
}

// 0x4e72f0
void SetMouseOverTextPosition(int x, int y)
{
	MouseOverTextX = x;
	MouseOverTextY = y;
}

// 0x4e7310
const char *GetMouseOverText()
{
	return MouseOverText;
}

// 0x4e7320
void GetMouseOverTextPosition(int *x, int *y)
{
	*x = MouseOverTextX;
	*y = MouseOverTextY;
}

// 0x4e7340. Called once per main loop iteration.
void UpdateBlinkState()
{
	const int Now = GetMilliseconds();
	if (BlinkTime + 300 < Now) {
		BlinkState = !BlinkState;
		BlinkTime = Now;
	}
}

// 0x4e7370
bool GetBlinkState()
{
	return BlinkState;
}

// 0x4e7380. The result of RemoveFile is dropped.
void DeleteErrorFile()
{
	RemoveFile("error.txt");
}

// 0x4e75d0. Type of the first message, 0 when the queue is empty.
int PeekMessageType()
{
	if (NumberOfMessages == 0)
		return 0;
	return Messages(0).Type;
}

// 0x4e75f0. Removes the first message; all zero when the queue is empty.
// The parameters now belong to the caller.
void FetchMessage(int *Type, INT_PTR *Param1, INT_PTR *Param2)
{
	if (NumberOfMessages != 0) {
		*Type = Messages(0).Type;
		*Param1 = Messages(0).Param1;
		*Param2 = Messages(0).Param2;
		for (int i = 0; i < NumberOfMessages - 1; i++) {
			Messages(i).Type = Messages(i + 1).Type;
			Messages(i).Param1 = Messages(i + 1).Param1;
			Messages(i).Param2 = Messages(i + 1).Param2;
		}
		NumberOfMessages--;
		return;
	}
	*Type = 0;
	*Param1 = 0;
	*Param2 = 0;
}

// 0x4e76e0. Frees the parameters that each message type owns. Type 14's
// Param2 points to two more blocks.
void ClearMessages()
{
	for (int i = 0; i < NumberOfMessages; i++) {
		const int Type = Messages(i).Type;
		void *Param1 = reinterpret_cast<void *>(Messages(i).Param1);
		void *Param2 = reinterpret_cast<void *>(Messages(i).Param2);
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
		}
	}
	NumberOfMessages = 0;
}

// 0x4e7bb0
void ClearErrorReports()
{
	for (int i = ErrorReports.min; i <= ErrorReports.max; i++) {
		TErrorReport &Report = ErrorReports(i);
		memset(Report.Title, 0, sizeof(Report.Title));
		memset(Report.Date, 0, sizeof(Report.Date));
		memset(Report.Text, 0, sizeof(Report.Text));
		memset(Report.Comment, 0, sizeof(Report.Comment));
	}
}

// 0x4e7c20. Starts a report after the last one that has a title. Like the
// functions below it searches down from ErrorReports.max; reports are
// filled by the error.txt reader in Player.cpp.
void AddErrorReport(const char *Title)
{
	int i = ErrorReports.max;
	while (i >= 0 && ErrorReports(i).Title[0] == 0)
		i--;
	strncpy(ErrorReports(i + 1).Title, Title, 64);
}

// 0x4e7c70. The setters below change the last report with a title and do
// nothing when there is none.
void SetErrorReportDate(const char *Date)
{
	int i = ErrorReports.max;
	while (i >= 0 && ErrorReports(i).Title[0] == 0)
		i--;
	if (i >= 0)
		strncpy(ErrorReports(i).Date, Date, 26);
}

// 0x4e7cc0. Appends a line; nothing is added once the text is full.
void AddErrorReportText(const char *Text)
{
	int i = ErrorReports.max;
	while (i >= 0 && ErrorReports(i).Title[0] == 0)
		i--;
	if (i >= 0) {
		int Length = (int)strlen(ErrorReports(i).Text);
		if (Length + 1 < 2048) {
			if (Length != 0) {
				ErrorReports(i).Text[Length] = '\n';
				Length++;
			}
			strncpy(ErrorReports(i).Text + Length, Text, 2048 - Length);
		}
	}
}

// 0x4e7d60
void SetErrorReportComment(const char *Comment)
{
	int i = ErrorReports.max;
	while (i >= 0 && ErrorReports(i).Title[0] == 0)
		i--;
	if (i >= 0) {
		char Prefix[] = "Comment: ";
		const int Length = (int)strlen(Prefix);
		strncpy(ErrorReports(i).Comment, Prefix, Length);
		strncpy(ErrorReports(i).Comment + Length, Comment, 512 - Length);
	}
}

// 0x4e7e10. Reports up to and including the last one with a title.
int GetNumberOfErrorReports()
{
	int i = ErrorReports.max;
	while (true) {
		if (i < 0)
			return 0;
		if (ErrorReports(i).Title[0] != 0)
			break;
		i--;
	}
	return i + 1;
}

// 0x4e7e40. The getters return "" for numbers outside the vector.
const char *GetErrorReportTitle(int Number)
{
	if (Number >= ErrorReports.min && Number <= ErrorReports.max)
		return ErrorReports(Number).Title;
	return "";
}

// 0x4e7e70
const char *GetErrorReportDate(int Number)
{
	if (Number >= ErrorReports.min && Number <= ErrorReports.max)
		return ErrorReports(Number).Date;
	return "";
}

// 0x4e7ea0
const char *GetErrorReportText(int Number)
{
	if (Number >= ErrorReports.min && Number <= ErrorReports.max)
		return ErrorReports(Number).Text;
	return "";
}

// 0x4e7ed0
const char *GetErrorReportComment(int Number)
{
	if (Number >= ErrorReports.min && Number <= ErrorReports.max)
		return ErrorReports(Number).Comment;
	return "";
}

// 0x4e7f00. LinkRegexp and TaintedClipboardText are freed but not cleared,
// and the queued messages are not freed.
void ExitStatus()
{
	ClearErrorReports();
	if (Characters != nullptr)
		delete[] Characters;
	Characters = nullptr;
	if (TaintedClipboardText != nullptr)
		delete[] TaintedClipboardText;
	if (LinkRegexp != nullptr)
		free(LinkRegexp);
}

// 0x4e8580, line 91
void SetRight(int rightid, int Value)
{
	TASSERT1(91, In(rightid,0,RIGHT_ANY-1), rightid);
	Rights[rightid] = Value;
}

// 0x4e86e0, line 100. Bit -1 asks for any bit. The RIGHT_ANY branch cannot
// be reached after the assertion; it is in the binary.
bool CheckRight(int rightid, int Bit)
{
	TASSERT1(100, In(rightid,0,RIGHT_ANY-1), rightid);
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
		TERROR1(165, "unknown right", rightid);
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
		TERROR1(183, "unknown action", actionid);
	}
}

// 0x4e9020, line 197. Appends a message; the queue takes ownership of the
// parameters that ClearMessages frees.
void AddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	TASSERT1(197, In(Type,MESSAGE_NONE,MESSAGE_STARTPATCHER), Type);
	Messages(NumberOfMessages).Type = Type;
	Messages(NumberOfMessages).Param1 = Param1;
	Messages(NumberOfMessages).Param2 = Param2;
	NumberOfMessages++;
}

// 0x4e91d0, line 209. Inserts a message at the front of the queue.
void AddMessageFirst(int Type, INT_PTR Param1, INT_PTR Param2)
{
	TASSERT1(209, In(Type,MESSAGE_NONE,MESSAGE_STARTPATCHER), Type);
	for (int i = NumberOfMessages; i > 0; i--)
		Messages(i) = Messages(i - 1);
	Messages(0).Type = Type;
	Messages(0).Param1 = Param1;
	Messages(0).Param2 = Param2;
	NumberOfMessages++;
}

// 0x4e93d0, lines 621-622
void SetAccount(int Number, const char *Password)
{
	TASSERT(621, Password!=NULL);
	TASSERT1(622, strlen(Password)<30, strlen(Password));
	AccountNumber = Number;
	MsvcSnprintf(AccountNumberString, sizeof(AccountNumberString), "%d", Number);
	AccountNumberString[11] = 0;
	strncpy(AccountPassword, Password, 30);
}

// 0x4e96d0, lines 669-673. Address is the IPv4 address in network order.
void SetCharacter(int Number, const char *Name, const char *World, const uint8 *Address, int Port)
{
	TASSERT1(669, In(Number,0,AccountCharacters-1), Number);
	TASSERT(670, Name!=NULL);
	TASSERT1(671, strlen(Name)<30, strlen(Name));
	TASSERT(672, World!=NULL);
	TASSERT1(673, strlen(World)<30, strlen(World));
	TCharacter *Character = &Characters[Number];
	strcpy(Character->Name, Name);
	strcpy(Character->World, World);
	Character->IP = *reinterpret_cast<const uint32 *>(Address);
	MsvcSnprintf(Character->IPAddress, sizeof(Character->IPAddress), "%d.%d.%d.%d",
		Address[0], Address[1], Address[2], Address[3]);
	Character->IPAddress[15] = 0;
	Character->Port = Port;
}

// 0x4e9c50, line 696
void SetActiveCharacter(int Number)
{
	TASSERT1(696, In(Number,0,AccountCharacters-1), Number);
	ActiveCharacter = Number;
}

// 0x4e9db0, line 713
const char *GetCharacterName(int Number)
{
	TASSERT1(713, In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].Name;
}

// 0x4e9f20, line 722
const char *GetCharacterWorld(int Number)
{
	TASSERT1(722, In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].World;
}

// 0x4ea090, line 740
const char *GetCharacterIPAddress(int Number)
{
	TASSERT1(740, In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].IPAddress;
}

// 0x4ea200, line 749
int GetCharacterPort(int Number)
{
	TASSERT1(749, In(Number,0,AccountCharacters-1), Number);
	return Characters[Number].Port;
}

// 0x4ea370, line 869. Keeps a copy of the text; if its lower-case form
// contains something like a web or IP address, the link warning handler is
// called instead of copying (it can copy later with
// CopyTaintedClipboardText).
void CheckClipboardText(const char *Text, int Length)
{
	TBEGIN
	if (TaintedClipboardText != nullptr)
		delete[] TaintedClipboardText;
	TaintedClipboardText = new char[Length + 1];
	memcpy(TaintedClipboardText, Text, Length);
	TaintedClipboardText[Length] = 0;
	char *Lower = new char[Length + 1];
	memcpy(Lower, TaintedClipboardText, Length + 1);
	LowerString(Lower);
	const int Match = regexec(LinkRegexp, Lower);
	delete[] Lower;
	if (Match == 1) {
		LinkWarningEventHandler();
		return;
	}
	SetClipboardText(TaintedClipboardText);
	TEND(869)
}

// 0x4ea5b0, line 875
void CopyToClipboard(const char *text)
{
	TASSERT(875, text);
	CheckClipboardText(text, (int)strlen(text));
}

// 0x4ea710, line 882
void CopyTaintedClipboardText()
{
	TASSERT(882, TaintedClipboardText);
	SetClipboardText(TaintedClipboardText);
}

// 0x4ea860, line 1051. Not everything is reset: the mouse, window and drag
// positions keep their values.
void InitStatus(bool Gamemaster, void (*linkwarningeventhandler)())
{
	TASSERT(1051, linkwarningeventhandler!=NULL);
	LinkWarningEventHandler = linkwarningeventhandler;
	TaintedClipboardText = nullptr;
	LinkRegexp = nullptr;
	LinkRegexp = regcomp(const_cast<char *>(LinkPattern));
	::Gamemaster = Gamemaster;
	ClearRights();
	NumberOfMessages = 0;
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
	ConnectionState = 0;
	AccountNumber = 0;
	AccountPassword[0] = 0;
	AccountCharacters = 0;
	Characters = nullptr;
	PremiumDays = 0;
	ActiveCharacter = 0;
	ActionState = 0;
	MouseOverText = nullptr;
	PendingActionState = 0;
	PendingActionNumber = 0;
	Progress = -1;
}

#if defined(PATELLA_TEST_MODE)
// The test mode's state dump (issue #22; src/testmode/ClientView.h) reads this file's data; it writes
// nothing. Test build only.
const int *patella::testmode::ViewConnectionState()
{
	return &ConnectionState;
}
#endif
