// Status.cpp: the client's shared state. Account, character list and
// connection state; the message queue that the network code and the GUI use
// to talk to the main loop; mouse and drag state; the player's rights;
// error reports read from error.txt; the clipboard link check.
//
// Names are ours unless a comment says they come from the binary.

#pragma once

#include "common.h"
#include "Container.h"
#include "Objects.h" // TObject

// "In(rightid,0,RIGHT_ANY-1)"; CheckRight treats RIGHT_ANY as "any right".
#define RIGHT_ANY 50

// "In(Type,MESSAGE_NONE,MESSAGE_STARTPATCHER)". Other values seen: 1 shows
// a message box (Param1 title, Param2 text, which the queue owns), 10 with
// Param1 300 follows a failed game server connection.
#define MESSAGE_NONE 0
#define MESSAGE_STARTPATCHER 72

// One queued message (12 bytes). Type is named in the assertions.
struct TMessage {
	int Type;
	INT_PTR Param1;
	INT_PTR Param2;
};

// One entry of the character list (0x54 bytes). The 30-byte limits come
// from "strlen(Name)<30" and "strlen(World)<30".
struct TCharacter {
	char Name[30];       // +0x00
	char World[30];      // +0x1e
	uint32 IP;           // +0x3c, as received
	char IPAddress[16];  // +0x40, dotted text
	int Port;            // +0x50
};

// One error report from error.txt (0xa5a bytes, byte aligned). Date is
// 26 bytes, the size of an asctime() string.
struct TErrorReport {
	char Title[64];      // +0x000
	char Date[26];       // +0x040
	char Text[2048];     // +0x05a, lines separated by '\n'
	char Comment[512];   // +0x85a, "Comment: " and the player's text

	TErrorReport()
	{
		memset(Title, 0, sizeof(Title));
		memset(Date, 0, sizeof(Date));
		memset(Text, 0, sizeof(Text));
		memset(Comment, 0, sizeof(Comment));
	}
};

bool CheckAnyRight();
void ClearRights();
int GetActionNumber();
void IncrementActionNumber();
// The action state of no action (name from MainWindow.cpp's
// "GetActionState()==ACTIONSTATE_NONE"; the other states are numbers: 1 left
// button down, 2 right button down, 3 both down, 4 and 5 one of both
// released, 7 and 8 set by events 0x2c and 0x2d, 9 help mode, 10 dialog
// open, 11 context menu open).
#define ACTIONSTATE_NONE 0
void SetActionState(int State);
int GetActionState();
void SetMousePosition(int x, int y);
int GetMouseX();
int GetMouseY();
void SetMouseCursor(int Cursor);
int GetMouseCursor();
void SetPendingAction(int State, int Number);
int GetPendingActionState();
int GetPendingActionNumber();
void SetWindowSize(int Width, int Height);
int GetWindowWidth();
int GetWindowHeight();
void SetDragStart(int x, int y);
void SetDragObject(TObject Obj, int Index, int x, int y, int z);
void SetDragTarget(int x, int y, int z);
void CheckDragStart(int x, int y);
TObject *GetDragObject(int *Index, int *x, int *y, int *z);
void GetDragTarget(int *x, int *y, int *z);
void SetTradeObject(TObject Obj, int Index, int x, int y, int z);
void SetUseObject(TObject Obj, int Index, int x, int y, int z);
void SetTradeInfo(int Info);
TObject *GetTradeObject(int *Index, int *x, int *y, int *z);
TObject *GetUseObject(int *Index, int *x, int *y, int *z);
int GetTradeInfo();
void SetConnectionState(int State);
int GetConnectionState();
void SetProgress(int Progress);
int GetProgress();
int GetAccountNumber();
const char *GetAccountNumberString();
const char *GetAccountPassword();
void SetAccountCharacters(int Number);
void SetPremiumDays(int Days);
int GetAccountCharacters();
int GetPremiumDays();
int GetActiveCharacter();
bool GetGamemaster();
void SetMouseOverText(const char *Text);
void SetMouseOverTextPosition(int x, int y);
const char *GetMouseOverText();
void GetMouseOverTextPosition(int *x, int *y);
void UpdateBlinkState();
bool GetBlinkState();
void DeleteErrorFile();
int PeekMessageType();
void FetchMessage(int *Type, INT_PTR *Param1, INT_PTR *Param2);
void ClearMessages();
void ClearErrorReports();
void AddErrorReport(const char *Title);
void SetErrorReportDate(const char *Date);
void AddErrorReportText(const char *Text);
void SetErrorReportComment(const char *Comment);
int GetNumberOfErrorReports();
const char *GetErrorReportTitle(int Number);
const char *GetErrorReportDate(int Number);
const char *GetErrorReportText(int Number);
const char *GetErrorReportComment(int Number);
void ExitStatus();
void SetRight(int rightid, int Value);
bool CheckRight(int rightid, int Bit);
const char *GetRightName(int rightid);
const char *GetActionName(int actionid);
void AddMessage(int Type, INT_PTR Param1, INT_PTR Param2);
void AddMessageFirst(int Type, INT_PTR Param1, INT_PTR Param2);
void SetAccount(int Number, const char *Password);
void SetCharacter(int Number, const char *Name, const char *World, const uint8 *Address, int Port);
void SetActiveCharacter(int Number);
const char *GetCharacterName(int Number);
const char *GetCharacterWorld(int Number);
const char *GetCharacterIPAddress(int Number);
int GetCharacterPort(int Number);

// The regular expression of a link (a domain or an IPv4 address), which
// CheckClipboardText looks for in copied text; InitStatus compiles it.
// Defined here with internal linkage: every object that includes this
// header has its own copy of the pointer in .data (all point to the text
// 0x55b758), 17 in the image (notes/decisions.md, "LinkPattern (issue
// #33)"):
//
//   0x5c039c Communication.obj   0x5c16f4 graphics.obj
//   0x5c0668 ConsoleWindow.obj   0x5c1704 GUI.obj
//   0x5c08c4 ContentWindows.obj  0x5c1bac MainFrm.obj
//   0x5c0b20 ContextMenus.obj    0x5c1e08 MainWindow.obj
//   0x5c0b30 Control.obj         0x5c22b0 MapWindow.obj
//   0x5c0fdc Creatures.obj       0x5c22c0 Network.obj
//   0x5c122c DialogElements.obj  0x5c22c8 Player.obj
//   0x5c1488 Dialogs.obj         0x5c2560 Status.obj (the one InitStatus reads)
//   0x5c16e4 DialogWindow.obj
//
// MainFrm.obj, MainWindow.obj and Player.obj medium (they use Status.cpp,
// their neighbours do not), the others high. Only Status.cpp reads it. The
// name is ours.
static const char *LinkPattern =
	"(^|[ .,:;/@<>|()\"\n\r%])(([a-z0-9-]+(\\.|\\%2e))+(ac|ad|ae|aero|af|ag|ai|al|am|an|ao|aq|a"
	"r|arpa|as|at|au|aw|az|ba|bb|bd|be|bf|bg|bh|bi|biz|bj|bm|bn|bo|br|bs|bt|bv|bw|by|bz|ca|cc|c"
	"d|cf|cg|ch|ci|ck|cl|cm|cn|co|com|coop|cr|cu|cv|cx|cy|cz|de|dj|dk|dm|do|dz|ec|edu|ee|eg|er|"
	"es|et|eu|fi|fj|fk|fm|fo|fr|ga|gb|gd|ge|gf|gg|gh|gi|gl|gm|gn|gov|gp|gq|gr|gs|gt|gu|gw|gy|hk"
	"|hm|hn|hr|ht|hu|id|ie|il|im|in|info|int|io|iq|ir|is|it|je|jm|jo|jobs|jp|ke|kg|kh|ki|km|kn|"
	"kr|kw|ky|kz|la|lb|lc|li|lk|lr|ls|lt|lu|lv|ly|ma|mc|md|mg|mh|mil|mk|ml|mm|mn|mo|mobi|mp|mq|"
	"mr|ms|mt|mu|museum|mv|mw|mx|my|mz|na|name|nc|ne|net|nf|ng|ni|nl|no|np|nr|nu|nz|om|org|pa|p"
	"e|pf|pg|ph|pk|pl|pm|pn|pr|pro|ps|pt|pw|py|qa|re|ro|ru|rw|sa|sb|sc|sd|se|sg|sh|si|sj|sk|sl|"
	"sm|sn|so|sr|st|su|sv|sy|sz|tc|td|tf|tg|th|tj|tk|tl|tm|tn|to|tp|tr|travel|tt|tv|tw|tz|ua|ug"
	"|uk|um|us|uy|uz|va|vc|ve|vg|vi|vn|vu|wf|ws|ye|yt|yu|za|zm|zw)\\.?|[0-9][0-9]?[0-9]?(\\.|\\"
	"%2e)[0-9][0-9]?[0-9]?(\\.|\\%2e)[0-9][0-9]?[0-9]?(\\.|\\%2e)[0-9][0-9]?[0-9]?)([?#& .,:;/@"
	"<>|()\"\r\n%]|$)";

void CheckClipboardText(const char *Text, int Length);
void CopyToClipboard(const char *text);
void CopyTaintedClipboardText();
void InitStatus(bool Gamemaster, void (*linkwarningeventhandler)());
