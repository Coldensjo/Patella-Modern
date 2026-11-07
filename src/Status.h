// Status.cpp: the client's shared state. Account, character list and
// connection state; the message queue that the network code and the GUI use
// to talk to the main loop; mouse and drag state; the player's rights; the
// clipboard link check. (The error reports read from error.txt are gone
// since Patella-Modern issue #74, decision D2.)
//
// Names are ours unless a comment says they come from the binary.
//
// Patella-Modern issue #105 (M7, notes/decisions.md, "#104: TClient and the subsystem objects" and "#105"): the
// session (the connection state, the account, the character list, the rights) is TSession's, which TCommunication
// owns (Communication.h); the free functions of the original's names are the bridge that the callers use until they
// hold the object (bridge::Session(), defined in Client.cpp). Issue #107: the rest of the file's data, the user
// interaction (the action, drag, trade and use state, the mouse, the message queue, the blink, the mouse-over text, the
// clipboard's link check), is TInteraction's, which TGUI owns (GUI.h); bridge::Interaction() the same way.

#pragma once

#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include "Objects.h" // TObject
#include "types/Regexp.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// "In(rightid,0,RIGHT_ANY-1)"; CheckRight treats RIGHT_ANY as "any right".
inline constexpr int RIGHT_ANY = 50;

// "In(Type,MESSAGE_NONE,MESSAGE_STARTPATCHER)". Other values seen: 1 shows
// a message box (Param1 title, Param2 text, which the queue owns), 10 with
// Param1 300 follows a failed game server connection.
inline constexpr int MESSAGE_NONE = 0;
inline constexpr int MESSAGE_STARTPATCHER = 72;

// One queued message (12 bytes). Type is named in the assertions.
struct TMessage {
	int Type;
	std::intptr_t Param1;
	std::intptr_t Param2;
};

// One entry of the character list (0x54 bytes). The 30-byte limits come
// from "strlen(Name)<30" and "strlen(World)<30".
struct TCharacter {
	TFixedString<30> Name;      // +0x00
	TFixedString<30> World;     // +0x1e
	uint32 IP;                  // +0x3c, as received
	TFixedString<16> IPAddress; // +0x40, dotted text
	int Port;                   // +0x50
};
// The original's layout (no pointers: the same on x64), with the names TFixedStrings (issue #57).
static_assert(sizeof(TCharacter) == 0x54 && offsetof(TCharacter, World) == 0x1e && offsetof(TCharacter, IP) == 0x3c
		&& offsetof(TCharacter, IPAddress) == 0x40 && offsetof(TCharacter, Port) == 0x50,
	"TCharacter's layout");

namespace patella {

// Status.cpp's session (issue #105): the globals 0x71c478-0x71c58b of the original are the members, in its order
// with the same initial values (0, as the original's .bss), and the functions that read and write only them the
// members of the same names. InitSession and ExitSession are InitStatus's and ExitStatus's part of them (not functions
// of the original: InitStatus inlined ClearRights).
class TSession {
public:
	TSession() = default;
	TSession(const TSession &) = delete;
	TSession &operator=(const TSession &) = delete;
	TSession(TSession &&) = delete;
	TSession &operator=(TSession &&) = delete;
	~TSession() = default;

	[[nodiscard]] bool CheckAnyRight() const;
	void ClearRights();
	void SetConnectionState(int State);
	[[nodiscard]] int GetConnectionState() const;
	void SetProgress(int Value);
	[[nodiscard]] int GetProgress() const;
	[[nodiscard]] int GetAccountNumber() const;
	[[nodiscard]] const char *GetAccountNumberString() const;
	[[nodiscard]] const char *GetAccountPassword() const;
	void SetAccountCharacters(int Number);
	void SetPremiumDays(int Days);
	[[nodiscard]] int GetAccountCharacters() const;
	[[nodiscard]] int GetPremiumDays() const;
	[[nodiscard]] int GetActiveCharacter() const;
	[[nodiscard]] bool GetGamemaster() const;
	void SetRight(int rightid, int Value);
	[[nodiscard]] bool CheckRight(int rightid, int Bit) const;
	void SetAccount(int Number, const char *Password);
	void SetCharacter(int Number, const char *Name, const char *World, const uint8 *Address, int Port);
	void SetActiveCharacter(int Number);
	[[nodiscard]] const char *GetCharacterName(int Number) const;
	[[nodiscard]] const char *GetCharacterWorld(int Number) const;
	[[nodiscard]] const char *GetCharacterIPAddress(int Number) const;
	[[nodiscard]] int GetCharacterPort(int Number) const;
	void InitSession(bool Value);
	void ExitSession();

	// The test mode's views (ClientView.h's ViewConnectionState and ViewCharacters read through them).
	[[nodiscard]] const int *ViewConnectionState() const { return &ConnectionState; }
	[[nodiscard]] const TCharacter *ViewCharacters() const { return Characters.get(); }
	[[nodiscard]] const int *ViewAccountCharacters() const { return &AccountCharacters; }

private:
	// Data, in the original's .bss order.
	std::array<int, RIGHT_ANY> Rights{};      // 0x71c478
	bool Gamemaster{};                        // 0x71c540, sent in the game login packet
	int PremiumDays{};                        // 0x71c544
	int ActiveCharacter{};                    // 0x71c548
	std::unique_ptr<TCharacter[]> Characters; // 0x71c54c (issue #58)

public:
	int AccountCharacters{}; // 0x71c550, name from an assertion; public: the golden Errors test sets it (TestErrorStatics)

private:
	TFixedString<30> AccountPassword{};     // 0x71c554
	TFixedString<12> AccountNumberString{}; // 0x71c574
	int AccountNumber{};                    // 0x71c580
	int Progress{};                         // 0x71c584, -1 = none
	int ConnectionState{};                  // 0x71c588, see SetConnectionState

	// The arrays std::arrays and TFixedStrings (issue #60).
	static_assert(sizeof(Rights) == RIGHT_ANY * sizeof(int) && sizeof(AccountPassword) == 30
			&& sizeof(AccountNumberString) == 12,
		"Status.cpp's arrays");
};

// Status.cpp's user interaction (issue #107): the globals of 0x71c458-0x71c64f that are not the session's are the
// members, in the original's .bss order with the same initial values (0, as its .bss), and the functions that read and
// write them the members of the same names. TGUI owns it (GUI.h). GetRightName and GetActionName read no data and
// stay free functions.
class TInteraction {
public:
	TInteraction() = default;
	TInteraction(const TInteraction &) = delete;
	TInteraction &operator=(const TInteraction &) = delete;
	TInteraction(TInteraction &&) = delete;
	TInteraction &operator=(TInteraction &&) = delete;
	~TInteraction() = default;

	[[nodiscard]] int GetActionNumber() const;
	void IncrementActionNumber();
	void SetActionState(int State);
	[[nodiscard]] int GetActionState() const;
	void SetMousePosition(int x, int y);
	[[nodiscard]] int GetMouseX() const;
	[[nodiscard]] int GetMouseY() const;
	void SetMouseCursor(int Cursor);
	[[nodiscard]] int GetMouseCursor() const;
	void SetPendingAction(int State, int Number);
	[[nodiscard]] int GetPendingActionState() const;
	[[nodiscard]] int GetPendingActionNumber() const;
	void SetWindowSize(int Width, int Height);
	[[nodiscard]] int GetWindowWidth() const;
	[[nodiscard]] int GetWindowHeight() const;
	void SetDragStart(int x, int y);
	void SetDragObject(TObject Obj, int Index, int x, int y, int z);
	void SetDragTarget(int x, int y, int z);
	void CheckDragStart(int x, int y);
	TObject *GetDragObject(int *Index, int *x, int *y, int *z);
	void GetDragTarget(int *x, int *y, int *z) const;
	void SetTradeObject(TObject Obj, int Index, int x, int y, int z);
	void SetUseObject(TObject Obj, int Index, int x, int y, int z);
	void SetTradeInfo(int Info);
	TObject *GetTradeObject(int *Index, int *x, int *y, int *z);
	TObject *GetUseObject(int *Index, int *x, int *y, int *z);
	[[nodiscard]] int GetTradeInfo() const;
	void SetMouseOverText(const char *Text);
	void SetMouseOverTextPosition(int x, int y);
	[[nodiscard]] const char *GetMouseOverText() const;
	void GetMouseOverTextPosition(int *x, int *y) const;
	void UpdateBlinkState();
	[[nodiscard]] bool GetBlinkState() const;
	int PeekMessageType();
	void FetchMessage(int *Type, std::intptr_t *Param1, std::intptr_t *Param2);
	void ClearMessages();
	void ExitStatus();
	void AddMessage(int Type, std::intptr_t Param1, std::intptr_t Param2);
	void AddMessageFirst(int Type, std::intptr_t Param1, std::intptr_t Param2);
	void CheckClipboardText(TSpan<const char> Characters);
	void CopyToClipboard(const char *text);
	void CopyTaintedClipboardText();
	void InitStatus(bool Gamemaster, void (*linkwarningeventhandler)());

private:
	// Data, in the original's .bss order (0x71c478-0x71c58b, between MouseOverText and TradeInfo, is TSession's).
	TRegexp LinkRegexp;                  // 0x71c458, LinkPattern compiled (issue #129: owned, TRegexp)
	char *TaintedClipboardText{};        // 0x71c45c, name from an assertion
	void (*LinkWarningEventHandler)(){}; // 0x71c460, from "linkwarningeventhandler!=NULL"
	bool BlinkState{};                   // 0x71c464, toggled every 300 ms
	int BlinkTime{};                     // 0x71c468
	int MouseOverTextY{};                // 0x71c46c
	int MouseOverTextX{};                // 0x71c470
	const char *MouseOverText{};         // 0x71c474
	int TradeInfo{};                     // 0x71c58c
	int UseZ{};                          // 0x71c590
	int UseY{};                          // 0x71c594
	int UseX{};                          // 0x71c598
	int UseIndex{};                      // 0x71c59c
	int TradeZ{};                        // 0x71c5a0
	int TradeY{};                        // 0x71c5a4
	int TradeX{};                        // 0x71c5a8
	int TradeIndex{};                    // 0x71c5ac
	int DragTargetZ{};                   // 0x71c5b0
	int DragTargetY{};                   // 0x71c5b4
	int DragTargetX{};                   // 0x71c5b8
	int DragZ{};                         // 0x71c5bc
	int DragY{};                         // 0x71c5c0
	int DragX{};                         // 0x71c5c4
	int DragIndex{};                     // 0x71c5c8
	int DragStartY{};                    // 0x71c5cc
	int DragStartX{};                    // 0x71c5d0
	int PendingActionNumber{};           // 0x71c5d4
	int PendingActionState{};            // 0x71c5d8
	int WindowHeight{};                  // 0x71c5dc
	int WindowWidth{};                   // 0x71c5e0
	int MouseCursor{};                   // 0x71c5e4
	int ActionState{};                   // 0x71c5e8, from "GetActionState()" in Control.cpp
	int ActionNumber{};                  // 0x71c5ec
	// 0x71c5f0 NumberOfMessages and 0x71c5f4 (initialiser 0x55a0f0) the original's vector<TMessage>(0, 9, 10): a
	// queue that appends at its count (AddMessage), inserts at 0 (AddMessageFirst), pops at 0 and reads below the
	// count only, so a std::vector, its size the count (issue #100; notes/decisions.md, "#100").
	std::vector<TMessage> Messages;
	int MouseX{};            // 0x71c61c
	int MouseY{};            // 0x71c620
	TObject DraggedObject{}; // 0x71c624
	TObject TradeObject{};   // 0x71c630, from "TradeObject!=0" in MapWindow.cpp
	TObject UseObject{};     // 0x71c63c, "SrcObj" in MapWindow.cpp
};

// The transition's access path (issue #105; notes/decisions.md, "#104: TClient and the subsystem objects"): the
// session of the one TClient's TCommunication (Client.cpp). It goes when no free function needs it.
namespace bridge {
TSession &Session();
TInteraction &Interaction(); // issue #107: the one TClient's TGUI's
} // namespace bridge

} // namespace patella

// The session's functions (issue #105): each calls TSession's member of the same name on bridge::Session(); the
// others are Status.cpp's own.
bool CheckAnyRight();
void ClearRights();
int GetActionNumber();
void IncrementActionNumber();
// The action state of no action (name from MainWindow.cpp's
// "GetActionState()==ACTIONSTATE_NONE"; the other states are numbers: 1 left
// button down, 2 right button down, 3 both down, 4 and 5 one of both
// released, 7 and 8 set by events 0x2c and 0x2d, 9 help mode, 10 dialog
// open, 11 context menu open).
inline constexpr int ACTIONSTATE_NONE = 0;
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
int PeekMessageType();
void FetchMessage(int *Type, std::intptr_t *Param1, std::intptr_t *Param2);
void ClearMessages();
void ExitStatus();
void SetRight(int rightid, int Value);
bool CheckRight(int rightid, int Bit);
const char *GetRightName(int rightid);
const char *GetActionName(int actionid);
void AddMessage(int Type, std::intptr_t Param1, std::intptr_t Param2);
void AddMessageFirst(int Type, std::intptr_t Param1, std::intptr_t Param2);
void SetAccount(int Number, const char *Password);
void SetCharacter(int Number, const char *Name, const char *World, const uint8 *Address, int Port);
void SetActiveCharacter(int Number);
const char *GetCharacterName(int Number);
const char *GetCharacterWorld(int Number);
const char *GetCharacterIPAddress(int Number);
int GetCharacterPort(int Number);

namespace {

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
// their neighbours do not), the others high. Only Status.cpp reads it (the
// others' copies are unused: [[maybe_unused]] for GCC and Clang, issue #91). The
// name is ours.
[[maybe_unused]] const char *LinkPattern =
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

} // namespace

void CheckClipboardText(patella::TSpan<const char> Characters); // the copied characters, no NUL needed
void CopyToClipboard(const char *text);
void CopyTaintedClipboardText();
void InitStatus(bool Gamemaster, void (*linkwarningeventhandler)());
