// Dialogs.h: the dialogs of the client (c:\mud\tibiaplayer\Dialogs.h;
// Dialogs.cpp is .\Dialogs.cpp).
//
// Every dialog is a TDialog (the frame: caption, separator, the OK, Cancel
// and extra buttons, dragging) around a body window that builds the
// dialog's controls and handles its keys and events. The creators make the
// body and the frame and return the frame as the new modal window
// (Control.cpp's ProcessEvents); a body closes the dialog with
// SetActionState(0), which makes ProcessEvents delete the modal window.
//
// Names from the binary: the members OkButton, CancelButton, ExtraButton
// (assertions), FocusEdit (a local, assertion), DraggedObject (with
// TObject's member Data), the parameters _Caption, _Dialog, timeout, Event,
// Key, Surface, seconds, MaximumLength, _ObjectNumber, _NumberOfChannels,
// _ChannelIDs, _CharacterID, CharacterID, DataSize, Timeout, Number, x, y,
// Code, Text, MaxLength, Data, _Coordinate and the locals Position,
// Creature, Number and AllocationNumber. The class, function and other member names are ours; see
// notes/decisions.md, "Dialogs.cpp".
//
// The header's inline constructors and destructors have out-of-line copies
// at the start of Dialogs.obj (0x46b790-0x46c0cf, with Container.h's
// vector<TIgnoreEntry> 0x46bb60-0x46bdef and 0x46c0d0).

#pragma once

#include "common.h"
#include "types/FixedString.h"
#include "types/IndexedVector.h"
#include "GUI.h"
#include "graphics.h"
#include "DialogElements.h"
#include "Map.h"         // TCoordinate, TMark
#include "Preferences.h" // TFullscreenMode, HOTKEYS
#include "Text.h"        // TReconnectBody::SetTime reads TextTryingToReconnect
#include <array>
#include <cstddef>
#include <memory>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-pro-type-vararg off for the file:
// the original's varargs (TMultiStateButton's constructor, _snprintf)
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

class TDialog;
struct TTalkLine;

// --- Creators (lines 48-631) ---------------------------------------------------------

// Each makes a dialog and returns it as the new modal window (Control.cpp's
// ProcessEvents); the Move Objects and Report dialogs return NULL when they
// cannot be opened.
TDialog *CreateMessageDialog(const char *Caption, const char *Text);                          // 0x47f8a0, line 48
TDialog *CreateFormattedMessageDialog(const char *Caption, const char *Text);                 // 0x47faf0, line 67
TDialog *CreateConnectingDialog(const char *Caption, const char *Text, TComponent *Receiver); // 0x47fd30, line 84
TDialog *CreateWaitDialog(const char *Caption, const char *Text, int Timeout);                // 0x47ff60, line 102
TDialog *CreateReconnectDialog(const char *Caption, const char *Text, int timeout);           // 0x4801c0, line 119
TDialog *CreateQuestionDialog(const char *Caption, const char *Text, TComponent *Receiver, int YesEvent,
	int NoEvent);                                                                                       // 0x4803f0, line 134
TDialog *CreateLogoutDialog(const char *Caption, const char *Text, TComponent *Receiver);               // 0x480640, line 149
TDialog *CreateInfoDialog(void (*OpenURL)(const char *URL));                                            // 0x4808b0, line 177
TDialog *CreateMoveObjectsDialog();                                                                     // 0x480cd0, line 200
TDialog *CreateLoginDialog(void (*OpenURL)(const char *URL));                                           // 0x480f40, line 214
TDialog *CreateCharacterListDialog(int Timeout);                                                        // 0x481160, line 230
TDialog *CreateTextDialog(const int *ObjectData, const char *Author, const char *Text);                 // 0x481370, line 280
TDialog *CreateEditListDialog(int *HouseData, const char *Text);                                        // 0x4817c0, line 294
TDialog *CreateOutfitDialog(int *Data);                                                                 // 0x4819f0, line 307
TDialog *CreateBugReportDialog();                                                                       // 0x481bf0, line 321
TDialog *CreateNewChannelDialog(int NumberOfChannels, int *ChannelIDs, const char (*ChannelNames)[30]); // 0x481e00, line 334; NOLINT(modernize-avoid-c-arrays): the tests fake it by address with this signature (issue #60)
TDialog *CreateInviteDialog();                                                                          // 0x482010, line 347
TDialog *CreateExcludeDialog();                                                                         // 0x482210, line 360
TDialog *CreateIgnoreListDialog();                                                                      // 0x482410, line 375
TDialog *CreateReportDialog();                                                                          // 0x482640, line 407
TDialog *CreateAddBuddyDialog();                                                                        // 0x482870, line 420
TDialog *CreateEditBuddyDialog(int CharacterID);                                                        // 0x482a70, line 434
TDialog *CreateHelpDialog(void (*OpenURL)(const char *URL));                                            // 0x482ea0, line 462
TDialog *CreateHintsDialog();                                                                           // 0x4830c0, line 476
TDialog *CreateRuleViolationDialog(const char *Name, TTalkLine *Statement);                             // 0x485290, line 490
TDialog *CreateEditMarkDialog(TCoordinate *Coordinate, TMark *Mark);                                    // 0x4832e0, line 504
TDialog *CreateOptionsDialog();                                                                         // 0x483500, line 533
TDialog *CreateGeneralOptionsDialog(void (*OpenURL)(const char *URL));                                  // 0x483730, line 547
TDialog *CreateGraphicsOptionsDialog(void (*OpenURL)(const char *URL), void (*SetDisplayMode)(int Mode),
	TWindow *Parent);                                                                          // 0x483950, line 561
TDialog *CreateAdvancedGraphicsDialog(void (*OpenURL)(const char *URL), TComponent *Receiver); // 0x483b80, line 576
TDialog *CreateConsoleOptionsDialog(void (*OpenURL)(const char *URL));                         // 0x483dc0, line 590
TDialog *CreateNetworkOptionsDialog(void (*OpenURL)(const char *URL));                         // 0x483fe0, line 604
TDialog *CreateHotkeyDialog(void (*OpenURL)(const char *URL));                                 // 0x484200, line 617

// --- The frame ------------------------------------------------------------------

// The frame of every dialog (vtable 0x563778, 0xc4 bytes): a caption over a
// separator, the body Dialog and up to three buttons in the bottom right
// corner (Cancel, OK, the extra button, from the right). Return sends
// OkCallback while there is an OK button with a callback, Escape sends
// CancelCallback; every other key goes to the body. The caption line drags
// the dialog inside the game window.
class TDialog : public TWindow {
public:
	TDialog(const char *_Caption, TComponent *_Dialog); // 0x47c970, lines 650-677
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	int getMinimumWidth() override { return SkinProperties().StyleB08.Data00; }  // +0x3c, 0x47cf50
	int getMinimumHeight() override { return SkinProperties().StyleB08.Data04; } // +0x40, 0x47cf60
	void Arrange() override;                                                     // +0x48, 0x46cd20, line 812
	bool MouseMove(int x, int y) override;                                       // +0x4c, 0x46d410, line 848
	bool LeftButtonDown(int x, int y, int Flags) override;                       // +0x50, 0x46d660, line 867
	bool LeftButtonUp(int x, int y, int Flags) override;                         // +0x54, 0x46d830, line 880
	void KeyPress(int Key) override;                                             // +0x6c, 0x46d220, line 831
	void HandleEvent(int Event) override;                                        // +0x70, 0x46d9f0, line 891
	// A NULL text removes the button. The OK button's callback NoCallback
	// becomes (this, 1) for Return; the buttons themselves keep the callback
	// given.
	void SetOkButton(const char *Text, TCallback Callback);     // 0x46c3e0, lines 687-700
	void SetCancelButton(const char *Text, TCallback Callback); // 0x46c710, lines 710-723
	void SetExtraButton(const char *Text, TCallback Callback);  // 0x46ca40, lines 734-744

	TComponent *Dialog;        // +0x2c, the body (a window or a label)
	TTextButton *OkButton;     // +0x30
	TTextButton *CancelButton; // +0x34
	TTextButton *ExtraButton;  // +0x38
	TLabel *CaptionLabel;      // +0x3c
	TSeparator *Separator;     // +0x40
	bool Dragging;             // +0x44
	int DragX;                 // +0x48, where the drag started
	int DragY;                 // +0x4c
	TFixedString<100> Caption; // +0x50
	TCallback OkCallback;      // +0xb4, Return
	TCallback CancelCallback;  // +0xbc, Escape
};

// --- Question labels -------------------------------------------------------------

// A question whose keys answer it (vtable 0x562110, 0x5c bytes): Return, y
// and Y send YesCallback, Escape, n and N send NoAnswerCallback.
class TQuestionLabel final : public TLabel {
public:
	// Inline (out-of-line copy 0x46b790).
	TQuestionLabel(TFont &_Font, const char *_Text, TCallback _YesCallback, TCallback _NoAnswerCallback)
		: TLabel(_Font, _Text)
	{
		YesCallback = _YesCallback;
		NoAnswerCallback = _NoAnswerCallback;
	}
	// The destructor is TLabel's (deleting destructor 0x46be00).
	void KeyPress(int Key) override; // +0x6c, 0x46db80, line 915

	TCallback YesCallback;      // +0x4c
	TCallback NoAnswerCallback; // +0x54
};

// The logout question (vtable 0x562198, 0x5c bytes): Return, l and L send
// LogoutCallback and close the dialog, e and E send ExitCallback, Escape, c
// and C close it.
class TLogoutLabel final : public TLabel {
public:
	// Inline (out-of-line copy 0x46b840).
	TLogoutLabel(TFont &_Font, const char *_Text, TCallback _LogoutCallback, TCallback _ExitCallback)
		: TLabel(_Font, _Text)
	{
		LogoutCallback = _LogoutCallback;
		ExitCallback = _ExitCallback;
	}
	// The destructor is TLabel's (deleting destructor 0x46be00, shared).
	void KeyPress(int Key) override; // +0x6c, 0x46dd40, line 952

	TCallback LogoutCallback; // +0x4c
	TCallback ExitCallback;   // +0x54
};

// A message to wait on (vtable 0x562220, 0x54 bytes): the "Connecting"
// dialogs. Event 31 is its timer, 29 the Abort button; both close the
// dialog (an Abort first clears the message queue when ClearOnAbort is
// set). With CloseOnMessage, the next queued message closes it.
class TWaitLabel final : public TLabel {
public:
	TWaitLabel(const TWaitLabel &) = delete;
	TWaitLabel &operator=(const TWaitLabel &) = delete;
	// Inline (out-of-line copy 0x46b8f0).
	TWaitLabel(const char *_Text, bool _CloseOnMessage, bool _ClearOnAbort)
		: TLabel(SkinProperties().StyleB08.Data64, _Text)
	{
		ClearOnAbort = _ClearOnAbort;
		Timer = nullptr;
		CloseOnMessage = _CloseOnMessage;
	}
	// Inline (out-of-line copy 0x46b950, deleting destructor 0x46be20).
	~TWaitLabel() override { Timer.reset(); }
	void Draw(int Surface, int x, int y) override;       // +0x44, 0x46e2b0, line 1011
	void HandleEvent(int Event) override;                // +0x70, 0x46e0e0, line 1000
	virtual void SetTimer(int Time, TCallback Callback); // +0x88, 0x46df20, line 985

	std::unique_ptr<TTimer> Timer; // +0x4c (issue #58)
	bool CloseOnMessage;           // +0x50
	bool ClearOnAbort;             // +0x51
};

// --- Connecting -----------------------------------------------------------------

// The reconnect countdown (vtable 0x5622b0, 0x23c bytes): a text, a progress
// bar and the time left. When the timer runs out it connects again.
class TReconnectWindow final : public TWindow {
public:
	TReconnectWindow(const TReconnectWindow &) = delete;
	TReconnectWindow &operator=(const TReconnectWindow &) = delete;
	TReconnectWindow(const char *Text, int timeout); // 0x46e470, line 1052
	// Inline (out-of-line copy 0x46b9b0, deleting destructor 0x46be40).
	~TReconnectWindow() override { Timer.reset(); }
	void Draw(int Surface, int x, int y) override; // +0x44, 0x46e8b0, line 1073
	void HandleEvent(int Event) override;          // +0x70, 0x46ea60, line 1122
	// Inline (out-of-line copy 0x46ba10): Time in tenths of a second.
	void SetTime(int Time)
	{
		if (Time > 600) {
			const int Minutes = Time / 600 + 1;
			MsvcSnprintf(Text, sizeof(Text), TextTryingToReconnect, Minutes, Minutes < 2 ? "minute" : "minutes");
		} else {
			MsvcSnprintf(Text, sizeof(Text), TextTryingToReconnect, Time / 10,
				Time / 10 < 2 ? "second" : "seconds");
		}
	}

	int Timeout;                   // +0x2c, tenths of a second
	TFixedString<512> Text;        // +0x30, the time left
	std::unique_ptr<TTimer> Timer; // +0x230, event 31 (issue #58)
	TProgressBar *ProgressBar;     // +0x234
	TLabel *TimeLabel;             // +0x238
};

// --- Login and character list ------------------------------------------------

// "Enter Game" (vtable 0x5627d8, 0x38 bytes): account number and password,
// both shown as stars, and a link to the account creation.
class TLoginDialog final : public TWindow {
public:
	TLoginDialog(const TLoginDialog &) = delete;
	TLoginDialog &operator=(const TLoginDialog &) = delete;
	TLoginDialog(void (*OpenURL)(const char *URL)); // 0x46ecf0, line 1162
	// Inline (out-of-line copy 0x46bf90, deleting destructor 0x46c090).
	~TLoginDialog() override { FocusGroup.reset(); }
	void KeyPress(int Key) override;      // +0x6c, 0x46f160, lines 1183-1187
	void HandleEvent(int Event) override; // +0x70, 0x46f3e0, line 1203

	TPasswordEditLine *Account;              // +0x2c
	TPasswordEditLine *Password;             // +0x30
	std::unique_ptr<TFocusGroup> FocusGroup; // +0x34 (issue #58)
};

// "Select Character" (vtable 0x562370, 0x38 bytes): the account's characters
// and its premium state. With a timeout the selection is confirmed when the
// timer runs out; every key and click starts it again.
class TCharacterListDialog final : public TWindow {
public:
	TCharacterListDialog(const TCharacterListDialog &) = delete;
	TCharacterListDialog &operator=(const TCharacterListDialog &) = delete;
	TCharacterListDialog(); // 0x46f5c0, line 1262
	// Inline (out-of-line copy 0x46baa0, deleting destructor 0x46be60).
	~TCharacterListDialog() override
	{
		Timer.reset();
		Timer = nullptr;
	}
	bool LeftButtonDown(int x, int y, int Flags) override; // +0x50, 0x46fa10, line 1274
	bool RightButtonUp(int x, int y, int Flags) override;  // +0x5c, 0x46fbc0, line 1286
	bool MouseWheel(int x, int y, int Code) override;      // +0x60, 0x46fd70, line 1298
	void KeyPress(int Key) override;                       // +0x6c, 0x4700b0, line 1335
	void HandleEvent(int Event) override;                  // +0x70, 0x470280, line 1361
	void PositionChanged() override;                       // +0x74, 0x46ff20, line 1309
	virtual void SetTimeout(int seconds);                  // +0xa0, 0x470490, line 1374

	TListBox *ListBox;             // +0x2c
	std::unique_ptr<TTimer> Timer; // +0x30, event 31 (issue #58)
	int Seconds;                   // +0x34
};

// --- Moving objects -------------------------------------------------------------

// "Move Objects" (vtable 0x562b68, 0x60 bytes): the number of a dragged pile
// to move, chosen with a scroll bar or typed.
class TMoveObjectsDialog final : public TWindow {
public:
	TMoveObjectsDialog(); // 0x470670, lines 1392-1406
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override; // +0x44, 0x470b60, line 1423
	void KeyPress(int Key) override;               // +0x6c, 0x470de0, line 1459
	void HandleEvent(int Event) override;          // +0x70, 0x471080, line 1475

	TObject DraggedObject;           // +0x2c
	int SourceIndex;                 // +0x38, GetDragObject's Index
	int SourceX;                     // +0x3c
	int SourceY;                     // +0x40
	int SourceZ;                     // +0x44
	int DestinationX;                // +0x48
	int DestinationY;                // +0x4c
	int DestinationZ;                // +0x50
	THorizontalScrollBar *ScrollBar; // +0x54
	int Unused58;                    // +0x58, never used
	int LastDigit;                   // +0x5c, the digit typed before
};

// --- Outfit -------------------------------------------------------------------

// "Select Outfit" (vtable 0x562878, 0x27c bytes): the outfit in a frame, a
// Next button, the four parts (head, body, legs, feet) as a radio group and
// the 133 colours as another. Data holds the outfit, its four colours and
// the first and last outfit the player may choose.
class TOutfitDialog final : public TWindow {
public:
	TOutfitDialog(const TOutfitDialog &) = delete;
	TOutfitDialog &operator=(const TOutfitDialog &) = delete;
	TOutfitDialog(const int *Data); // 0x47cf70, lines 1488-1542
	// Inline (out-of-line copy 0x46bfc0, deleting destructor 0x46c0b0).
	~TOutfitDialog() override
	{
		ColorGroup.reset();
		PartGroup.reset();
	}
	void HandleEvent(int Event) override; // +0x70, 0x471250, line 1586

	TOutfitIcon *OutfitIcon;                       // +0x2c
	std::array<TTextButton *, 4> PartButtons;      // +0x30, events 1000-1003
	std::unique_ptr<TRadioButtonGroup> PartGroup;  // +0x40 (issue #58)
	std::array<TColorButton *, 133> ColorButtons;  // +0x44, events 2000-2132
	std::unique_ptr<TRadioButtonGroup> ColorGroup; // +0x258 (issue #58)
	std::array<int, 4> Colors;                     // +0x25c
	int Outfit;                                    // +0x26c
	int FirstOutfit;                               // +0x270
	int LastOutfit;                                // +0x274
	int Part;                                      // +0x278, the part whose colour the buttons show
};

// --- Texts --------------------------------------------------------------------

// "Show Text" and "Edit Text" (vtable 0x562c28, 0x44 bytes): the text of a
// readable or writable object, with its icon and a line about who wrote it.
// The text can be edited when the object is writable, or writable once and
// still empty.
class TTextDialog final : public TWindow {
public:
	TTextDialog(int MaximumLength, const char *Text, uint32 _ObjectNumber, TObject _Obj,
		const char *Description); // 0x471500, lines 1702-1727
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void KeyPress(int Key) override { EditText->KeyPress(Key); } // +0x6c, 0x4719a0
	void HandleEvent(int Event) override;                        // +0x70, 0x4719b0, line 1739

	uint32 ObjectNumber;     // +0x2c, the server's number for SendEditText
	TObject Obj;             // +0x30
	TObjectIcon *ObjectIcon; // +0x3c
	TEditText *EditText;     // +0x40
};

// "Edit List" (vtable 0x563868, 0x38 bytes): a house list, one name per line.
class TEditListDialog final : public TWindow {
public:
	TEditListDialog(int MaxLength, const char *Text, int _Type, uint32 _Identifier,
		const char *Description); // 0x47d660, lines 1752-1775
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Arrange() override;                                     // +0x48, 0x471b60
	void KeyPress(int Key) override { EditText->KeyPress(Key); } // +0x6c, 0x47db50
	void HandleEvent(int Event) override;                        // +0x70, 0x471be0, line 1800

	int Type;            // +0x2c, for SendEditHouseText
	uint32 Identifier;   // +0x30
	TEditText *EditText; // +0x34
};

// --- Ignore list --------------------------------------------------------------

// A name of the ignore or white list with its permanent flag (31 bytes,
// Container.h's vector<TIgnoreEntry> 0x46bb60, 0x46bb70, 0x46c0d0; the type
// name is ours).
struct TIgnoreEntry {
	// Inline: the vector's constructor and operator() clear every new entry
	// (and init) this way.
	TIgnoreEntry()
	{
		Name[0] = 0;
		Permanent = false;
	}
	// strcpy(Name, Text) as the original wrote it (issue #85): the text and its NUL within the entry's 31 bytes.
	// A name of 30 characters, the most the lists' edit line takes, is kept whole, and its NUL lands in
	// Permanent, which every caller sets next (Name.Assign kept 29 characters, #57).
	void AssignName(const char *Text) noexcept
	{
		TStringBuffer(reinterpret_cast<char *>(this), sizeof(TIgnoreEntry)).Assign(Text);
	}
	// The name as a C string, at most its 30 characters (issue #86, a deliberate difference): one of 30 has no NUL
	// of its own (its 31st byte is Permanent), and Tibia.exe read on past it, so with Permanent set the rebuilt
	// list's line had the byte 1 and the next entry's name after it.
	[[nodiscard]] TFixedString<31> NameText() const noexcept
	{
		TFixedString<31> Text;
		Text.Assign(Name.View());
		return Text;
	}

	TFixedString<30> Name; // +0x00
	bool Permanent;        // +0x1e
};
// The original's layout (no pointers: the same on x64), with Name a TFixedString (issue #57).
static_assert(sizeof(TIgnoreEntry) == 0x1f && offsetof(TIgnoreEntry, Name) == 0 && offsetof(TIgnoreEntry, Permanent) == 0x1e,
	"TIgnoreEntry's layout");

// "Edit Ignore List and White List" (vtable 0x5639d8, 0xd0 bytes): both lists
// as editable font list boxes (permanent entries in their own colour), the
// Add and Delete buttons and the global settings. The entries are kept in
// two vectors next to the lists; OK (602) writes everything back to Talk.cpp.
class TIgnoreListDialog final : public TWindow {
public:
	TIgnoreListDialog(); // 0x47db60, lines 1875-1975
	// The destructor is the implicit one (0x47eab0, deleting destructor 0x47ea90).
	void Arrange() override;                             // +0x48, 0x471d90, line 1987
	bool LeftButtonUp(int x, int y, int Flags) override; // +0x54, 0x472970, line 2205
	void KeyPress(int Key) override;                     // +0x6c, 0x472d80, line 2216
	void HandleEvent(int Event) override;                // +0x70, 0x471f20, line 2171

	TCheckBox *IgnoreListCheckBox;              // +0x2c, "Activate Ignore List"
	TFontListBox *IgnoreList;                   // +0x30, event 11
	TTextButton *AddIgnoreButton;               // +0x34, event 709
	TTextButton *DeleteIgnoreButton;            // +0x38, event 710
	TCheckBox *IgnorePermanently;               // +0x3c
	TCheckBox *IgnoreYelling;                   // +0x40
	TCheckBox *IgnorePrivate;                   // +0x44
	TCheckBox *WhiteListCheckBox;               // +0x48, "Activate White List"
	TFontListBox *WhiteList;                    // +0x4c, event 11
	TTextButton *AddWhiteButton;                // +0x50, event 712
	TTextButton *DeleteWhiteButton;             // +0x54, event 713
	TCheckBox *AllowPermanently;                // +0x58
	TCheckBox *AllowBuddies;                    // +0x5c, "Allow VIPs to message you"
	TIndexedVector<TIgnoreEntry> IgnoreEntries; // +0x60
	TIndexedVector<TIgnoreEntry> WhiteEntries;  // +0x98
};

// The qsort order of the names (out-of-line copy 0x46bb00, an inline
// function: its address is taken).
inline int CompareNames(const void *Name1, const void *Name2)
{
	return MsvcStricmp(*static_cast<const char *const *>(Name1), *static_cast<const char *const *>(Name2));
}

// --- Bug report and channels ----------------------------------------------------

// "Reporting" (vtable 0x562cd8, 0x30 bytes): a bug report.
class TBugReportDialog final : public TWindow {
public:
	TBugReportDialog(); // 0x472f20, line 2240
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	// Inline (folded with the scroll bars' 0x4731d0).
	void KeyPress(int Key) override { EditText->KeyPress(Key); } // +0x6c, 0x4731d0
	void HandleEvent(int Event) override;                        // +0x70, 0x4731e0, line 2252

	TEditText *EditText; // +0x2c
};

// "New Channel" (vtable 0x562418, 0x44 bytes): the channels the server
// offers (ID 0xffff opens an own private channel) and a name for a private
// conversation.
class TNewChannelDialog final : public TWindow {
public:
	TNewChannelDialog(const TNewChannelDialog &) = delete;
	TNewChannelDialog &operator=(const TNewChannelDialog &) = delete;
	TNewChannelDialog(int _NumberOfChannels, const int *_ChannelIDs, const char (*ChannelNames)[30]); // 0x4733a0, lines 2265-2297; NOLINT(modernize-avoid-c-arrays): as CreateNewChannelDialog (issue #60)
	// Inline (out-of-line copy 0x46bb20, deleting destructor 0x46be80).
	~TNewChannelDialog() override { ChannelIDs.reset(); }
	void KeyPress(int Key) override;      // +0x6c, 0x473980, line 2321
	void HandleEvent(int Event) override; // +0x70, 0x473b80, lines 2333-2349

	TListBox *ListBox;                 // +0x2c, double click 301
	TEditLine *NameEdit;               // +0x30
	int Unused34;                      // +0x34, never used
	int Unused38;                      // +0x38, never used
	int NumberOfChannels;              // +0x3c
	std::unique_ptr<int[]> ChannelIDs; // +0x40 (issue #58)
};

// "Invite player to private chat channel" and "Exclude player from private
// chat channel" (vtable 0x562db8, 0x34 bytes).
class TPrivateChannelDialog final : public TWindow {
public:
	TPrivateChannelDialog(bool _Exclude); // 0x473e60, line 2378
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void KeyPress(int Key) override;      // +0x6c, 0x474110, line 2392
	void HandleEvent(int Event) override; // +0x70, 0x4742e0, line 2406

	bool Exclude;        // +0x2c
	TEditLine *NameEdit; // +0x30
};

// "Report Rule Violation" (vtable 0x562e58, 0x30 bytes): a player's request
// to the gamemasters, one sentence.
class TReportDialog final : public TWindow {
public:
	TReportDialog(); // 0x4744b0, line 2551
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void KeyPress(int Key) override;      // +0x6c, 0x474760, line 2565
	void HandleEvent(int Event) override; // +0x70, 0x474930, line 2580

	TEditLine *TextEdit; // +0x2c
};

// --- VIP list -----------------------------------------------------------------

// "Add to VIP list" (vtable 0x562ef8, 0x30 bytes).
class TAddBuddyDialog final : public TWindow {
public:
	TAddBuddyDialog(); // 0x474b20, line 2607
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void KeyPress(int Key) override;      // +0x6c, 0x474dd0, line 2621
	void HandleEvent(int Event) override; // +0x70, 0x474fa0, line 2635

	TEditLine *NameEdit; // +0x2c
};

// "Change player in VIP list" (vtable 0x562558, 0x68 bytes): the icon, the
// description and the login notification of a VIP.
class TEditBuddyDialog final : public TWindow {
public:
	TEditBuddyDialog(const TEditBuddyDialog &) = delete;
	TEditBuddyDialog &operator=(const TEditBuddyDialog &) = delete;
	TEditBuddyDialog(int _CharacterID); // 0x475160, lines 2652-2696
	// Inline (out-of-line copy 0x46bec0, deleting destructor 0x46c010).
	~TEditBuddyDialog() override { IconGroup.reset(); }
	// Inline (out-of-line copy 0x46bef0).
	void KeyPress(int Key) override { Description->KeyPress(Key); } // +0x6c, 0x46bef0
	void HandleEvent(int Event) override;                           // +0x70, 0x475820, line 2720

	int CharacterID;                              // +0x2c
	std::unique_ptr<TRadioButtonGroup> IconGroup; // +0x30 (issue #58)
	TCheckBox *Notify;                            // +0x34
	TEditText *Description;                       // +0x38
	std::array<TIconButton *, 11> IconButtons;    // +0x3c
};

// --- Updates, help and hints ----------------------------------------------------

// TUpdateDialog ("Updating ... Please wait", vtable 0x562ff0, 0x44 bytes) is gone with the patcher since
// Patella-Modern issue #75.

// "Help" (vtable 0x5630c0, 0x30 bytes): six buttons with their texts.
class THelpDialog final : public TWindow {
public:
	THelpDialog(void (*_OpenURL)(const char *URL)); // 0x4760a0, line 2869
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void KeyPress(int Key) override;      // +0x6c, 0x476770, line 2882
	void HandleEvent(int Event) override; // +0x70, 0x476930, lines 2914-2918

	void (*OpenURL)(const char *URL); // +0x2c
};

// "Hints for Tibia" (vtable 0x563160, 0x30 bytes): the hints shown so far.
class THintsDialog final : public TWindow {
public:
	THintsDialog(); // 0x476cd0, line 2945
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void KeyPress(int Key) override;      // +0x6c, 0x476fb0, line 2959
	void HandleEvent(int Event) override; // +0x70, 0x477180, lines 2974-2983

	TListBox *ListBox; // +0x2c, double click 1
};

// --- Rule violations (gamemasters) ----------------------------------------------

// "Rule Violation" (vtable 0x5625f8, 0x114 bytes): a gamemaster's report of a
// player, optionally about a statement: the reasons are the rights the
// player has (Reasons maps the list's lines to them), the actions those the
// selected reason allows.
class TRuleViolationDialog final : public TWindow {
public:
	TRuleViolationDialog(const TRuleViolationDialog &) = delete;
	TRuleViolationDialog &operator=(const TRuleViolationDialog &) = delete;
	TRuleViolationDialog(const char *Name, TTalkLine *Statement); // 0x484620, line 3056
	// Inline (out-of-line copy 0x46bf00, deleting destructor 0x46c030).
	~TRuleViolationDialog() override { FocusGroup.reset(); }
	void KeyPress(int Key) override;      // +0x6c, 0x4774d0, line 3080
	void HandleEvent(int Event) override; // +0x70, 0x484e70, lines 3124-3128
	void UpdateActions();                 // 0x47eae0, line 3158
	int GetReason();                      // 0x4776b0, line 3171
	[[nodiscard]] int GetAction() const;  // 0x477860, line 3191

	std::unique_ptr<TFocusGroup> FocusGroup; // +0x2c (issue #58)
	TEditLine *NameEdit;                     // +0x30
	TListBox *ReasonList;                    // +0x34, event 21
	TListBox *ActionList;                    // +0x38
	TEditLine *CommentEdit;                  // +0x3c
	TEditLine *StatementEdit;                // +0x40
	TCheckBox *IPBanishment;                 // +0x44
	uint32 StatementID;                      // +0x48
	std::array<int, 50> Reasons;             // +0x4c, the right of each reason line, -1 after the last
};

// --- Automap marks ---------------------------------------------------------------

// "Edit Mark" (vtable 0x562698, 0xe8 bytes): the text and the symbol of an
// automap mark. It takes the coordinate and the mark (if any) and deletes
// them.
class TEditMarkDialog final : public TWindow {
public:
	TEditMarkDialog(const TEditMarkDialog &) = delete;
	TEditMarkDialog &operator=(const TEditMarkDialog &) = delete;
	TEditMarkDialog(TCoordinate *_Coordinate, TMark *_Mark); // 0x477c80, lines 3207-3242
	// Inline (out-of-line copy 0x46bf30, deleting destructor 0x46c050).
	~TEditMarkDialog() override { TypeGroup.reset(); }
	void KeyPress(int Key) override;      // +0x6c, 0x478200, line 3268
	void HandleEvent(int Event) override; // +0x70, 0x4783d0, lines 3294-3298

	TCoordinate Coordinate;                       // +0x2c
	TMark Mark;                                   // +0x38
	TEditLine *TextEdit;                          // +0xa8
	std::unique_ptr<TRadioButtonGroup> TypeGroup; // +0xac (issue #58)
	std::array<TIconButton *, 14> TypeButtons;    // +0xb0
};

// --- Options -----------------------------------------------------------------------

// "Options" (vtable 0x563258, 0x2c bytes): buttons that open the option
// dialogs (each reopens this one when it closes) and the message of the day.
class TOptionsDialog final : public TWindow {
public:
	TOptionsDialog(); // 0x4786d0, line 3366
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void KeyPress(int Key) override;      // +0x6c, 0x478e50, line 3380
	void HandleEvent(int Event) override; // +0x70, 0x479010, lines 3422-3426
};

// "General Options" (vtable 0x563360, 0x44 bytes).
class TGeneralOptionsDialog final : public TWindow {
public:
	TGeneralOptionsDialog(void (*_OpenURL)(const char *URL)); // 0x479310, line 3472
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void HandleEvent(int Event) override; // +0x70, 0x479720, lines 3495-3498

	TCheckBox *TextualEffects;        // +0x2c
	TCheckBox *CreatureNames;         // +0x30
	TCheckBox *ShowHints;             // +0x34
	TCheckBox *ClassicControl;        // +0x38
	TCheckBox *AutoChaseOff;          // +0x3c
	void (*OpenURL)(const char *URL); // +0x40, the Help button (701)
};

// "Graphics Options" (vtable 0x5624b8, 0x4c bytes): fullscreen and its
// resolution; Advanced (407) opens the advanced options.
class TGraphicsOptionsDialog final : public TWindow {
public:
	TGraphicsOptionsDialog(const TGraphicsOptionsDialog &) = delete;
	TGraphicsOptionsDialog &operator=(const TGraphicsOptionsDialog &) = delete;
	TGraphicsOptionsDialog(void (*_OpenURL)(const char *URL), void (*_SetDisplayMode)(int Mode),
		TWindow *_Parent); // 0x479a30, line 3549
	// Inline (out-of-line copy 0x46bb40, deleting destructor 0x46bea0).
	~TGraphicsOptionsDialog() override { Modes.reset(); }
	void KeyPress(int Key) override;      // +0x6c, 0x479f50, line 3573
	void HandleEvent(int Event) override; // +0x70, 0x47a150, line 3598

	TCheckBox *Fullscreen;                    // +0x2c
	TListBox *ResolutionList;                 // +0x30
	int NumberOfModes;                        // +0x34
	std::unique_ptr<TFullscreenMode[]> Modes; // +0x38, the surface's list (issue #58)
	void (*SetDisplayMode)(int Mode);         // +0x3c, 1 windowed, 2 fullscreen
	TWindow *Parent;                          // +0x40, passed on to the advanced options
	TTextButton *AdvancedButton;              // +0x44
	void (*OpenURL)(const char *URL);         // +0x48
};

// "Advanced Graphics Options" (vtable 0x562738, 0x54 bytes): light,
// stretching, the mouse pointer, the frame rate limit and the engine. The
// engine buttons send 800-802 to Receiver (the main window).
class TAdvancedGraphicsDialog final : public TWindow {
public:
	TAdvancedGraphicsDialog(const TAdvancedGraphicsDialog &) = delete;
	TAdvancedGraphicsDialog &operator=(const TAdvancedGraphicsDialog &) = delete;
	TAdvancedGraphicsDialog(void (*_OpenURL)(const char *URL), TComponent *_Receiver); // 0x47a3d0, line 3689
	// Inline (out-of-line copy 0x46bf60, deleting destructor 0x46c070).
	~TAdvancedGraphicsDialog() override { EngineGroup.reset(); }
	void Draw(int Surface, int x, int y) override; // +0x44, 0x47aa30, line 3723
	void HandleEvent(int Event) override;          // +0x70, 0x47ad10, line 3751

	TComponent *Receiver;                           // +0x2c
	TCheckBox *LightEffects;                        // +0x30
	TCheckBox *NoStretching;                        // +0x34
	TCheckBox *WindowsMousepointer;                 // +0x38
	THorizontalScrollBar *Framerate;                // +0x3c, 10-201 (201 is no limit), event 30
	void (*OpenURL)(const char *URL);               // +0x40
	TTextButton *DirectX5Button;                    // +0x44
	TTextButton *DirectX9Button;                    // +0x48
	TTextButton *OpenGLButton;                      // +0x4c
	std::unique_ptr<TRadioButtonGroup> EngineGroup; // +0x50 (issue #58)
	// The x64 client (Patella-Modern issue #31, plan 6.3): the three buttons choose the graphics device
	// ("Automatic", "GPU", "CPU" in the places of DirectX5Button, DirectX9Button, OpenGLButton), and the
	// line above them names the device in use. Since issue #111 without PATELLA_VULKAN's guard (the oracle,
	// whose layout kept the original's, left with #94), so patella_core compiles it too.
	void ShowDevice();                 // the selected button and the device's name
	TLabel *DeviceLabel;               // the line above the buttons (the original's "Select graphics engine:")
	TFixedString<128> DeviceLabelName; // the device the line names
};

// "Console Options" (vtable 0x563598, 0x44 bytes).
class TConsoleOptionsDialog final : public TWindow {
public:
	TConsoleOptionsDialog(void (*_OpenURL)(const char *URL)); // 0x47afb0, line 3797
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void HandleEvent(int Event) override; // +0x70, 0x47b3c0, line 3819

	TCheckBox *InfoMessages;          // +0x2c
	TCheckBox *EventMessages;         // +0x30
	TCheckBox *StatusMessages;        // +0x34
	TCheckBox *TimeStamps;            // +0x38
	TCheckBox *PrivateMessages;       // +0x3c
	void (*OpenURL)(const char *URL); // +0x40
};

// "Network Options" (vtable 0x563650, 0x34 bytes).
class TNetworkOptionsDialog final : public TWindow {
public:
	TNetworkOptionsDialog(void (*_OpenURL)(const char *URL)); // 0x47b5e0, line 3843
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void HandleEvent(int Event) override; // +0x70, 0x47b800, line 3860

	TCheckBox *TestServerLogin;       // +0x2c
	void (*OpenURL)(const char *URL); // +0x30
};

// "Hotkey Options" (vtable 0x563a90, 0x2464 bytes): the texts of the 36
// hotkeys (F1-F12 with Shift and Control), edited one at a time.
class THotkeyDialog final : public TWindow {
public:
	THotkeyDialog(void (*_OpenURL)(const char *URL)); // 0x47edf0, line 3906
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	bool LeftButtonUp(int x, int y, int Flags) override; // +0x54, 0x47c400, line 4010
	void KeyPress(int Key) override;                     // +0x6c, 0x47bd40, line 3958
	void HandleEvent(int Event) override;                // +0x70, 0x47c010, lines 3993-3996
	void UpdateLine(int Number);                         // 0x47b9c0, lines 3914-3933

	std::array<TFixedString<256>, HOTKEYS> Texts; // +0x2c
	std::array<bool, HOTKEYS> AutoSend;           // +0x242c
	TFontListBox *ListBox;                        // +0x2450, event 11
	TEditLine *TextEdit;                          // +0x2454
	TCheckBox *AutoSendCheckBox;                  // +0x2458
	int Selected;                                 // +0x245c, -1 for none
	void (*OpenURL)(const char *URL);             // +0x2460, kept, not used
};
// NOLINTEND(cppcoreguidelines-pro-type-vararg)
