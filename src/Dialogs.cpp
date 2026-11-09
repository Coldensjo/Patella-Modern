// Dialogs.cpp: the dialogs of the client (c:\mud\tibiaplayer\Dialogs.cpp);
// see Dialogs.h.
//
// Order: the functions follow the source's line numbers: the creators
// (lines 48-617), the frame TDialog (650-891) and the dialog bodies. The
// binary moves the creators, the constructors that call functions defined
// further down (TDialog's, the outfit's, the edit list's, the ignore
// list's, the hotkeys', the error report's, the rule violation's) and the
// rule violation's HandleEvent to the end of the object, Creatures.cpp's
// pattern. The error report dialog (CreateErrorReportDialog, line 631, and
// TErrorReportDialog, lines 4053-4084) is gone since Patella-Modern issue
// #74 (decision D2). The header code at the start of the object (0x46b790-0x46c0cf
// with Container.h's vector<TIgnoreEntry>) is in Dialogs.h and Container.h.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none.

#include "Dialogs.h"
#include "vulkan/DeviceSettings.h"
#include "vulkan/DeviceChoice.h"

namespace {

// The x64 client's graphics device choice in the advanced options (issue #31): the line above the
// buttons before a device exists, and the buttons' help texts.
const char *TextSelectDevice = "Select graphics device:";
const char *TextUseAutomaticDevice = "Use the best graphics device: a GPU, else the CPU";
const char *TextUseGpuDevice = "Use the graphics card (GPU); with several, each click takes the next";
const char *TextUseCpuDevice = "CPU (compatibility): draw without the graphics card";

} // namespace

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Communication.h"
#include "Creatures.h"
#include "Help.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Text.h"
#include "Utils.h"
#include "DialogElements.h"
#include "GUI.h"
#include "BuildConfig.h"
#include "Skins.h"
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include "graphics.h"
#include <cstddef>
#include <memory>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries; the original's varargs (TMultiStateButton's constructor, _snprintf)
// NOLINTBEGIN(cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg)


namespace {

// Data. Dialogs.obj's only static is NoCallback (static initialiser
// 0x559e10). Its copy of the text table (0x5c123c-0x5c1484, then its
// regular expression pointer 0x5c1488 and its copy of DefaultFontColor
// 0x5c148c) is Text.h's (Dialogs.h includes it).
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): the binary's global; TCallback's constructor (GUI.cpp) stores two values
TCallback NoCallback(nullptr, 0); // 0x5cc420

} // namespace

// --- Creators ----------------------------------------------------------------------

// 0x47f8a0, line 48. A message; OK and Escape close it.
TDialog *CreateMessageDialog(const char *Caption, const char *Text)
{
	TLabel *Label = new TLabel(0, 0, SkinProperties().StyleB08.Data64, Text);
	TDialog *Dialog = new TDialog(Caption, Label);
	Dialog->SetOkButton("Ok", TCallback(Dialog, 1));
	Dialog->SetCancelButton(nullptr, TCallback(Dialog, 1));
	return Dialog;
}

// 0x47faf0, line 67. A message with formatting tokens.
TDialog *CreateFormattedMessageDialog(const char *Caption, const char *Text)
{
	TFormattedText *FormattedText = new TFormattedText(Text);
	FormattedText->SetSize(SkinProperties().Data17C, SkinProperties().Data180);
	FormattedText->FitToChildren(TRect(0, 0, 0, 0), false);
	TDialog *Dialog = new TDialog(Caption, FormattedText);
	Dialog->SetOkButton("Ok", TCallback(Dialog, 1));
	Dialog->SetCancelButton(nullptr, TCallback(Dialog, 1));
	return Dialog;
}

// 0x47fd30, line 84. "Connecting": Abort (and Escape) send 29 to the
// receiver (the main window); the next queued message closes it.
TDialog *CreateConnectingDialog(const char *Caption, const char *Text, TComponent *Receiver)
{
	TWaitLabel *Label = new TWaitLabel(Text, true, false);
	TDialog *Dialog = new TDialog(Caption, Label);
	Dialog->SetOkButton("Abort", TCallback(Receiver, 29));
	Dialog->SetCancelButton(nullptr, TCallback(Receiver, 29));
	Dialog->CancelCallback = TCallback(Receiver, 29);
	return Dialog;
}

// 0x47ff60, line 102. "Please wait": Abort clears the message queue; a
// timeout (tenths of a second) closes the dialog.
TDialog *CreateWaitDialog(const char *Caption, const char *Text, int Timeout)
{
	TWaitLabel *Label = new TWaitLabel(Text, false, true);
	TDialog *Dialog = new TDialog(Caption, Label);
	Dialog->SetOkButton("Abort", TCallback(Label, 29));
	Dialog->SetCancelButton(nullptr, TCallback(Label, 29));
	Dialog->CancelCallback = TCallback(Label, 29);
	if (Timeout > 0)
		Label->SetTimer(Timeout, TCallback(Label, 31));
	return Dialog;
}

// 0x4801c0, line 119. The reconnect countdown (timeout in seconds).
TDialog *CreateReconnectDialog(const char *Caption, const char *Text, int timeout)
{
	TReconnectWindow *Window = new TReconnectWindow(Text, timeout);
	TDialog *Dialog = new TDialog(Caption, Window);
	Dialog->SetOkButton("Abort", TCallback(Window, 29));
	Dialog->SetCancelButton(nullptr, TCallback(Window, 29));
	Dialog->CancelCallback = TCallback(Window, 29);
	return Dialog;
}

// 0x4803f0, line 134. A yes/no question whose answers go to the receiver.
TDialog *CreateQuestionDialog(const char *Caption, const char *Text, TComponent *Receiver, int YesEvent,
	int NoEvent)
{
	TQuestionLabel *Label = new TQuestionLabel(SkinProperties().StyleB08.Data64, Text, TCallback(Receiver, YesEvent),
		TCallback(Receiver, NoEvent));
	TDialog *Dialog = new TDialog(Caption, Label);
	Dialog->SetOkButton("Yes", TCallback(Receiver, YesEvent));
	Dialog->SetCancelButton("No", TCallback(Receiver, NoEvent));
	return Dialog;
}

// 0x480640, line 149. Logout (506), Cancel (2) and Exit (7) go to the
// receiver.
TDialog *CreateLogoutDialog(const char *Caption, const char *Text, TComponent *Receiver)
{
	TLogoutLabel *Label = new TLogoutLabel(SkinProperties().StyleB08.Data64, Text, TCallback(Receiver, 506),
		TCallback(Receiver, 7));
	TDialog *Dialog = new TDialog(Caption, Label);
	Dialog->SetOkButton("Logout", TCallback(Receiver, 506));
	Dialog->SetCancelButton("Cancel", TCallback(Receiver, 2));
	Dialog->SetExtraButton("Exit", TCallback(Receiver, 7));
	return Dialog;
}

// BuildConfig.h's limit for a caption is what TTextButton's text holds.
static_assert(sizeof(TTextButton::Text) == patella::buildconfig::MaxCaption + 1, "TTextButton::Text and MaxCaption");

// 0x4808b0, line 177. "Info": the client version and a link to the web
// site, in a frame of skins.
TDialog *CreateInfoDialog(void (*OpenURL)(const char *URL))
{
	TWindow *Window = new TWindow();
	Window->SetSize(SkinProperties().StyleE68.Data00, SkinProperties().StyleE68.Data04);
	TLabel *Label = new TLabel(0, 0, SkinProperties().StyleE68.Data08, TextClientVersion);
	TLink *Link = new TLink(TextOfficialWebsite, patella::buildconfig::WebsiteCaption, TextTibiaURL, OpenURL,
		SkinProperties().StyleE68.Data1C);
	TSeparator *Separator = new TSeparator(true);
	Window->InsertChild(Label);
	Window->InsertChild(Separator);
	Window->InsertChild(Link);
	Window->SetChildArea(Label, SkinProperties().StyleE68.Data34);
	Window->SetChildArea(Separator, SkinProperties().StyleE68.Data54);
	Window->SetChildArea(Link, SkinProperties().StyleE68.Data44);
	TBorder *Border = new TBorder(Window, 0, 9, 0, 11, 10, 0, 8, 0, SkinProperties().StyleE68.Data30);
	Border->FitToWindow();
	TDialog *Dialog = new TDialog("Info", Border);
	Dialog->SetOkButton("Ok", TCallback(Dialog, 1));
	Dialog->SetCancelButton(nullptr, TCallback(Dialog, 1));
	return Dialog;
}

// 0x480cd0, line 200. Only for a dragged pile of 1-100 objects; otherwise
// it ends the drag and makes no dialog.
TDialog *CreateMoveObjectsDialog()
{
	int Unused;
	const TObject *Obj = GetDragObject(&Unused, &Unused, &Unused, &Unused);
	if (*Obj != 0 && Obj->Data > 0 && Obj->Data < 101) {
		TMoveObjectsDialog *Body = new TMoveObjectsDialog();
		TDialog *Dialog = new TDialog("Move Objects", Body);
		Dialog->SetOkButton("Ok", TCallback(Body, 1));
		Dialog->SetCancelButton("Cancel", TCallback(Body, 2));
		return Dialog;
	}
	SetActionState(0);
	return nullptr;
}

// 0x480f40, line 214
TDialog *CreateLoginDialog(void (*OpenURL)(const char *URL))
{
	TLoginDialog *Body = new TLoginDialog(OpenURL);
	TDialog *Dialog = new TDialog("Enter Game", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetCancelButton("Cancel", TCallback(Body, 2));
	return Dialog;
}

// 0x481160, line 230. Timeout in tenths of a second, 0 for none.
TDialog *CreateCharacterListDialog(int Timeout)
{
	TCharacterListDialog *Body = new TCharacterListDialog();
	TDialog *Dialog = new TDialog("Select Character", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	if (Timeout > 0)
		Body->SetTimeout(Timeout);
	return Dialog;
}

// 0x481370, line 280. ObjectData holds the server's object number, the
// object type and the maximum length. The text can be edited when the
// object is writable, or writable once and still empty; Return does not
// close either form (OkCallback is cleared).
TDialog *CreateTextDialog(const int *ObjectData, const char *Author, const char *Text)
{
	const int ObjectNumber = ObjectData[0];
	const int Type = ObjectData[1];
	const int MaximumLength = ObjectData[2];
	TFixedString<500> Description;
	Description[0] = 0;
	TTextDialog *Body;
	TDialog *Dialog;
	if (GetObjectFlag(Type, WRITE) || (GetObjectFlag(Type, WRITEONCE) && strlen(Text) == 0)) {
		if (strlen(Text) == 0)
			TStringBuffer(Description, 255).Format("{}\n{}", TextItIsCurrentlyEmpty, TextYouCanEnterNewText);
		else if (strlen(Author) != 0)
			MsvcSnprintf(Description, 255, "%s\n%s.\n%s", TextYouReadWrittenBy, Author, TextYouCanEnterNewText);
		else
			TStringBuffer(Description, 255).Format("{}\n{}", TextYouReadTheFollowing, TextYouCanEnterNewText);
		TObject Obj;
		Obj.Type = Type;
		Body = new TTextDialog(MaximumLength, Text, ObjectNumber, Obj, Description);
		Dialog = new TDialog("Edit Text", Body);
		Dialog->SetOkButton("Ok", TCallback(Body, 1));
	} else {
		if (strlen(Text) == 0)
			TStringBuffer(Description, 255).Assign(TextItIsEmpty);
		else if (strlen(Author) != 0)
			MsvcSnprintf(Description, 255, "%s\n%s.", TextYouReadWrittenBy, Author);
		else
			TStringBuffer(Description, 255).Assign(TextYouReadTheFollowing);
		TObject Obj;
		Obj.Type = Type;
		Body = new TTextDialog(MaximumLength, Text, ObjectNumber, Obj, Description);
		Dialog = new TDialog("Show Text", Body);
		Dialog->SetOkButton("Ok", TCallback(Body, 2));
		Dialog->SetCancelButton(nullptr, TCallback(Body, 2));
	}
	Dialog->OkCallback = NoCallback;
	return Dialog;
}

// 0x4817c0, line 294. HouseData holds the list's type and identifier.
TDialog *CreateEditListDialog(int *HouseData, const char *Text)
{
	TEditListDialog *Body = new TEditListDialog(2000, Text, HouseData[0], HouseData[1], TextEnterOneNamePerLine);
	TDialog *Dialog = new TDialog("Edit List", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->OkCallback = NoCallback;
	return Dialog;
}

// 0x4819f0, line 307
TDialog *CreateOutfitDialog(int *Data)
{
	TOutfitDialog *Body = new TOutfitDialog(Data);
	TDialog *Dialog = new TDialog("Select Outfit", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	return Dialog;
}

// 0x481bf0, line 321
TDialog *CreateBugReportDialog()
{
	TBugReportDialog *Body = new TBugReportDialog();
	TDialog *Dialog = new TDialog("Reporting", Body);
	Dialog->SetOkButton("Send", TCallback(Body, 1));
	Dialog->OkCallback = NoCallback;
	return Dialog;
}

// 0x481e00, line 334
TDialog *CreateNewChannelDialog(int NumberOfChannels, int *ChannelIDs, const char (*ChannelNames)[30]) // NOLINT(modernize-avoid-c-arrays): Dialogs.h
{
	TNewChannelDialog *Body = new TNewChannelDialog(NumberOfChannels, ChannelIDs, ChannelNames);
	TDialog *Dialog = new TDialog("New Channel", Body);
	Dialog->SetOkButton("Open", TCallback(Body, 1));
	return Dialog;
}

// 0x482010, line 347
TDialog *CreateInviteDialog()
{
	TPrivateChannelDialog *Body = new TPrivateChannelDialog(false);
	TDialog *Dialog = new TDialog("Invite player to private chat channel", Body);
	Dialog->SetOkButton("Add", TCallback(Body, 1));
	return Dialog;
}

// 0x482210, line 360. The OK button says "Add" here too.
TDialog *CreateExcludeDialog()
{
	TPrivateChannelDialog *Body = new TPrivateChannelDialog(true);
	TDialog *Dialog = new TDialog("Exclude player from private chat channel", Body);
	Dialog->SetOkButton("Add", TCallback(Body, 1));
	return Dialog;
}

// 0x482410, line 375. OK sends 602.
TDialog *CreateIgnoreListDialog()
{
	TIgnoreListDialog *Body = new TIgnoreListDialog();
	TDialog *Dialog = new TDialog("Edit Ignore List and White List", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 602));
	Dialog->OkCallback = NoCallback;
	Dialog->SetCancelButton("Cancel", TCallback(Body, 2));
	return Dialog;
}

// 0x482640, line 407. Only one open request at a time (a rule violation
// channel buffer exists while one is open).
TDialog *CreateReportDialog()
{
	if (GetTextBuffer(7, nullptr, -1) != nullptr) {
		SetStatusMessage(TextRequestAlreadySubmitted, false);
		return nullptr;
	}
	TReportDialog *Body = new TReportDialog();
	TDialog *Dialog = new TDialog("Report Rule Violation", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	return Dialog;
}

// 0x482870, line 420
TDialog *CreateAddBuddyDialog()
{
	TAddBuddyDialog *Body = new TAddBuddyDialog();
	TDialog *Dialog = new TDialog("Add to VIP list", Body);
	Dialog->SetOkButton("Add", TCallback(Body, 1));
	return Dialog;
}

// 0x482a70, line 434
TDialog *CreateEditBuddyDialog(int CharacterID)
{
	TEditBuddyDialog *Body = new TEditBuddyDialog(CharacterID);
	TDialog *Dialog = new TDialog("Change player in VIP list", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->OkCallback = NoCallback;
	return Dialog;
}

// 0x482c80, CreateUpdateDialog (line 448), is gone with the update dialog since Patella-Modern issue #75 (decision
// D9).

// 0x482ea0, line 462. OK goes to the frame (which closes the dialog).
TDialog *CreateHelpDialog(void (*OpenURL)(const char *URL))
{
	THelpDialog *Body = new THelpDialog(OpenURL);
	TDialog *Dialog = new TDialog("Help", Body);
	Dialog->SetOkButton("Ok", TCallback(Dialog, 1));
	Dialog->SetCancelButton(nullptr, TCallback(Dialog, 1));
	return Dialog;
}

// 0x4830c0, line 476. Cancel goes to the frame.
TDialog *CreateHintsDialog()
{
	THintsDialog *Body = new THintsDialog();
	TDialog *Dialog = new TDialog("Hints for Tibia", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetCancelButton("Cancel", TCallback(Dialog, 2));
	return Dialog;
}

// 0x485290, line 490. Name and Statement may be NULL.
TDialog *CreateRuleViolationDialog(const char *Name, TTalkLine *Statement)
{
	TRuleViolationDialog *Body = new TRuleViolationDialog(Name, Statement);
	TDialog *Dialog = new TDialog("Rule Violation", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetCancelButton("Cancel", TCallback(Body, 2));
	return Dialog;
}

// 0x4832e0, line 504. The body deletes the coordinate and the mark.
TDialog *CreateEditMarkDialog(TCoordinate *Coordinate, TMark *Mark)
{
	TEditMarkDialog *Body = new TEditMarkDialog(Coordinate, Mark);
	TDialog *Dialog = new TDialog("Edit Mark", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetCancelButton("Cancel", TCallback(Body, 2));
	return Dialog;
}

// 0x483500, line 533. Escape is OK here (both save the options).
TDialog *CreateOptionsDialog()
{
	TOptionsDialog *Body = new TOptionsDialog();
	TDialog *Dialog = new TDialog("Options", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetCancelButton(nullptr, TCallback(Body, 1));
	Dialog->CancelCallback = TCallback(Body, 1);
	return Dialog;
}

// 0x483730, line 547
TDialog *CreateGeneralOptionsDialog(void (*OpenURL)(const char *URL))
{
	TGeneralOptionsDialog *Body = new TGeneralOptionsDialog(OpenURL);
	TDialog *Dialog = new TDialog("General Options", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetExtraButton("Help", TCallback(Body, 701));
	return Dialog;
}

// 0x483950, line 561
TDialog *CreateGraphicsOptionsDialog(void (*OpenURL)(const char *URL), void (*SetDisplayMode)(int Mode),
	TWindow *Parent)
{
	TGraphicsOptionsDialog *Body = new TGraphicsOptionsDialog(OpenURL, SetDisplayMode, Parent);
	TDialog *Dialog = new TDialog("Graphics Options", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetExtraButton("Help", TCallback(Body, 701));
	return Dialog;
}

// 0x483b80, line 576
TDialog *CreateAdvancedGraphicsDialog(void (*OpenURL)(const char *URL), TComponent *Receiver)
{
	TAdvancedGraphicsDialog *Body = new TAdvancedGraphicsDialog(OpenURL, Receiver);
	TDialog *Dialog = new TDialog("Advanced Graphics Options", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetCancelButton("Cancel", TCallback(Body, 2));
	Dialog->SetExtraButton("Help", TCallback(Body, 701));
	return Dialog;
}

// 0x483dc0, line 590
TDialog *CreateConsoleOptionsDialog(void (*OpenURL)(const char *URL))
{
	TConsoleOptionsDialog *Body = new TConsoleOptionsDialog(OpenURL);
	TDialog *Dialog = new TDialog("Console Options", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetExtraButton("Help", TCallback(Body, 701));
	return Dialog;
}

// 0x483fe0, line 604
TDialog *CreateNetworkOptionsDialog(void (*OpenURL)(const char *URL))
{
	TNetworkOptionsDialog *Body = new TNetworkOptionsDialog(OpenURL);
	TDialog *Dialog = new TDialog("Network Options", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	Dialog->SetExtraButton("Help", TCallback(Body, 701));
	return Dialog;
}

// 0x484200, line 617
TDialog *CreateHotkeyDialog(void (*OpenURL)(const char *URL))
{
	THotkeyDialog *Body = new THotkeyDialog(OpenURL);
	TDialog *Dialog = new TDialog("Hotkey Options", Body);
	Dialog->SetOkButton("Ok", TCallback(Body, 1));
	return Dialog;
}

// --- TDialog ----------------------------------------------------------------------

// 0x47c970, lines 650-677. The assertions come after the base class and the
// two callbacks, before the original's try block. The frame has nine skins (43-51, Arrange);
// the caption is copied, the buttons are OK and Cancel until the creator
// changes them.
TDialog::TDialog(const char *_Caption, TComponent *_Dialog)
{
	PATELLA_ASSERT(_Caption!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(_Caption)<100, strlen(_Caption));
	PATELLA_ASSERT(_Dialog!=nullptr);
	DrawingListLength = 9;
	DrawingList.reset(new TDrawing[9]);
	Caption.Assign(_Caption);
	Dragging = false;
	OkCallback = NoCallback;
	CancelCallback = NoCallback;
	OkButton = nullptr;
	CancelButton = nullptr;
	ExtraButton = nullptr;
	Dialog = _Dialog;
	CaptionLabel = new TLabel(0, 0, SkinProperties().StyleB08.Data18, Caption);
	Separator = new TSeparator(true);
	InsertChild(Dialog);
	InsertChild(CaptionLabel);
	InsertChild(Separator);
	FitToChild(Dialog, SkinProperties().StyleB08.Data54, true);
	SetOkButton("Ok", TCallback(this, 1));
	SetCancelButton("Cancel", TCallback(this, 2));
}

// 0x46c3e0, lines 687-700
void TDialog::SetOkButton(const char *Text, TCallback Callback)
{
	if (Text == nullptr) {
		PATELLA_ASSERT(OkButton!=nullptr);
		DeleteChild(OkButton);
		OkButton = nullptr;
		OkCallback = NoCallback;
	} else {
		if (OkButton != nullptr)
			DeleteChild(OkButton);
		OkButton = new TTextButton(0, 0, Text, Callback, nullptr);
		if (Callback.Event == NoCallback.Event && Callback.Receiver == NoCallback.Receiver)
			Callback = TCallback(this, 1);
		OkCallback = Callback;
		InsertChild(OkButton);
	}
	Arrange();
}

// 0x46c710, lines 710-723. Without a button Escape still sends (this, 2).
void TDialog::SetCancelButton(const char *Text, TCallback Callback)
{
	if (Text == nullptr) {
		PATELLA_ASSERT(CancelButton!=nullptr);
		DeleteChild(CancelButton);
		CancelButton = nullptr;
		CancelCallback = TCallback(this, 2);
	} else {
		if (CancelButton != nullptr)
			DeleteChild(CancelButton);
		CancelButton = new TTextButton(0, 0, Text, Callback, nullptr);
		if (Callback.Event == NoCallback.Event && Callback.Receiver == NoCallback.Receiver)
			Callback = TCallback(this, 2);
		CancelCallback = Callback;
		InsertChild(CancelButton);
	}
	Arrange();
}

// 0x46ca40, lines 734-744
void TDialog::SetExtraButton(const char *Text, TCallback Callback)
{
	if (Text == nullptr) {
		PATELLA_ASSERT(ExtraButton!=nullptr);
		DeleteChild(ExtraButton);
		ExtraButton = nullptr;
	} else {
		if (ExtraButton != nullptr)
			DeleteChild(ExtraButton);
		ExtraButton = new TTextButton(0, 0, Text, Callback, nullptr);
		InsertChild(ExtraButton);
	}
	Arrange();
}

// 0x46cd20, line 812. The buttons take the positions from the right in the
// order Cancel, OK, extra. The bottom edge starts at Skins[45].height (the
// corner's height where its width would fit; kept).
void TDialog::Arrange()
{
	SetChildArea(Dialog, SkinProperties().StyleB08.Data54);
	SetChildArea(CaptionLabel, SkinProperties().StyleB08.Data08);
	SetChildArea(Separator, SkinProperties().StyleB08.Data44);
	int Button = 0;
	if (CancelButton != nullptr) {
		SetChildPosition(CancelButton, SkinProperties().StyleB08.Data2C[Button].x,
			SkinProperties().StyleB08.Data2C[Button].y);
		Button++;
	}
	if (OkButton != nullptr) {
		SetChildPosition(OkButton, SkinProperties().StyleB08.Data2C[Button].x,
			SkinProperties().StyleB08.Data2C[Button].y);
		Button++;
	}
	if (ExtraButton != nullptr) {
		SetChildPosition(ExtraButton, SkinProperties().StyleB08.Data2C[Button].x,
			SkinProperties().StyleB08.Data2C[Button].y);
	}
	DrawingList[0] = TDrawing(43, 0, 0, 0, 0, 0, 0);
	DrawingList[1] = TDrawing(49, Skins()[43].width, 0, Width - Skins()[44].width - Skins()[43].width,
		Skins()[49].height, 0, 0);
	DrawingList[2] = TDrawing(44, Width - Skins()[44].width, 0, 0, 0, 0, 0);
	DrawingList[3] = TDrawing(47, 0, Skins()[43].height, Skins()[47].width,
		Height - Skins()[45].height - Skins()[43].height, 0, 0);
	DrawingList[4] = TDrawing(48, Width - Skins()[48].width, Skins()[49].height, Skins()[48].width,
		Height - Skins()[46].height - Skins()[44].height, 0, 0);
	DrawingList[5] = TDrawing(45, 0, Height - Skins()[45].height, 0, 0, 0, 0);
	DrawingList[6] = TDrawing(50, Skins()[45].height, Height - Skins()[50].height,
		Width - Skins()[46].width - Skins()[45].width, Skins()[50].height, 0, 0);
	DrawingList[7] = TDrawing(46, Width - Skins()[46].width, Height - Skins()[46].height, 0, 0, 0, 0);
	DrawingList[8] = TDrawing(51, Skins()[47].width, Skins()[49].height, Width - Skins()[48].width - Skins()[47].width,
		Height - Skins()[50].height - Skins()[49].height, 0, 0);
}

// 0x46d220, line 831
void TDialog::KeyPress(int Key)
{
	if (Key == 13) {
		if (OkButton != nullptr
			&& !(OkCallback.Event == NoCallback.Event && OkCallback.Receiver == NoCallback.Receiver)) {
			OkCallback.Send();
			return;
		}
	} else if (Key == 27) {
		CancelCallback.Send();
		return;
	}
	Dialog->KeyPress(Key);
}

// 0x46d410, line 848. A drag keeps the dialog inside the game window. The new
// position is a value of its own (ebx, edi; the parameters' stack slots are
// never written): the original's catch wrapper prints the mouse position.
bool TDialog::MouseMove(int x, int y)
{
	if (!Dragging)
		return TWindow::MouseMove(x, y);
	int NewX = OffsetX - DragX + x;
	int NewY = OffsetY - DragY + y;
	Limit(&NewX, 0, GetWindowWidth() - Width);
	Limit(&NewY, 0, GetWindowHeight() - Height);
	SetPosition(NewX, NewY);
	return true;
}

// 0x46d660, line 867. A press on the caption line that no child takes
// starts a drag.
bool TDialog::LeftButtonDown(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonDown(x, y, Flags) && In(x, 0, Width - 1) && In(y, 0, Skins()[49].height - 1)) {
		Dragging = true;
		DragX = x;
		DragY = y;
	}
	return true;
}

// 0x46d830, line 880
bool TDialog::LeftButtonUp(int x, int y, int Flags)
{
	if (!Dragging)
		return TWindow::LeftButtonUp(x, y, Flags);
	Dragging = false;
	return true;
}

// 0x46d9f0, line 891. Events 1 and 2 (the default callbacks) close the
// dialog, and so does every other.
void TDialog::HandleEvent([[maybe_unused]] int Event)
{
	SetActionState(0);
}

// --- TQuestionLabel, TLogoutLabel ----------------------------------------------

// 0x46db80, line 915
void TQuestionLabel::KeyPress(int Key)
{
	if (Key == 13 || Key == 'y' || Key == 'Y')
		YesCallback.Send();
	if (Key == 27 || Key == 'n' || Key == 'N')
		NoAnswerCallback.Send();
}

// 0x46dd40, line 952
void TLogoutLabel::KeyPress(int Key)
{
	if (Key == 13 || Key == 'l' || Key == 'L') {
		LogoutCallback.Send();
		SetActionState(0);
	}
	if (Key == 'e' || Key == 'E')
		ExitCallback.Send();
	if (Key == 27 || Key == 'c' || Key == 'C')
		SetActionState(0);
}

// --- TWaitLabel ---------------------------------------------------------------------

// 0x46df20, line 985
void TWaitLabel::SetTimer(int Time, TCallback Callback)
{
	Timer.reset();
	Timer.reset(new TTimer(Time, Callback));
}

// 0x46e0e0, line 1000. The timer's ticks wait until it runs out.
void TWaitLabel::HandleEvent(int Event)
{
	if (Event == 31) {
		if (!Timer->IsExpired())
			return;
	} else if (Event == 29 && ClearOnAbort) {
		ClearMessages();
	}
	SetActionState(0);
}

// 0x46e2b0, line 1011
void TWaitLabel::Draw(int Surface, int x, int y)
{
	TLabel::Draw(Surface, x, y);
	if (CloseOnMessage && PeekMessageType() != 0)
		HandleEvent(29);
}

// --- TReconnectWindow ----------------------------------------------------------

// 0x46e470, line 1052. The width is the widest of the text, the bar and the
// time label with room for four digits (each compared twice, as with a max
// macro).
TReconnectWindow::TReconnectWindow(const char *Text, int timeout)
{
	Timeout = timeout * 10;
	Timer.reset(new TTimer(Timeout, TCallback(this, 31)));
	SetTime(Timeout + 10);
	TLabel *TextLabel = new TLabel(SkinProperties().StyleB08.Data64, Text);
	ProgressBar = new TProgressBar();
	TimeLabel = new TLabel(SkinProperties().StyleB08.Data64, this->Text);
	int FullWidth = TextLabel->Width > ProgressBar->getMinimumWidth() ? TextLabel->Width
																	  : ProgressBar->getMinimumWidth();
	FullWidth = FullWidth > TimeLabel->Width + GetTextWidth(SkinProperties().StyleB08.Data64.FontNumber, "8888")
		? FullWidth
		: TimeLabel->Width + GetTextWidth(SkinProperties().StyleB08.Data64.FontNumber, "8888");
	const int TextHeight = TextLabel->Height;
	InsertChild(TextLabel);
	const int BarY = TextHeight + SkinProperties().DataBEC;
	InsertChild(ProgressBar);
	const int BarHeight = ProgressBar->getMinimumHeight();
	const int TimeY = BarY + ProgressBar->getMinimumHeight() + SkinProperties().DataBEC;
	InsertChild(TimeLabel);
	const int TimeHeight = TimeLabel->Height;
	TComponent::SetSize(FullWidth, TimeY + TimeHeight);
	SetChildArea(TextLabel, TRect(0, 0, FullWidth, TextHeight));
	SetChildArea(ProgressBar, TRect(0, BarY, FullWidth, BarHeight));
	SetChildArea(TimeLabel, TRect(0, TimeY, FullWidth, TimeHeight));
	ProgressBar->SetMaximum(Timeout);
	ProgressBar->SetCurrent(0);
	Arrange();
}

// 0x46e8b0, line 1073
void TReconnectWindow::Draw(int Surface, int x, int y)
{
	TWindow::Draw(Surface, x, y);
	if (PeekMessageType() != 0)
		SetActionState(0);
}

// 0x46ea60, line 1122. When the time is up it logs out of a running game
// (states 7 and 8) or connects to the game world again; Abort (29) gives
// up and shows the character list again.
void TReconnectWindow::HandleEvent(int Event)
{
	if (Event == 31) {
		if (!Timer->IsExpired()) {
			const int Time = Timer->GetTime();
			ProgressBar->SetCurrent(Timeout - Time);
			SetTime(Time + 10);
			TimeLabel->SetText(Text, false);
		} else if (GetConnectionState() == 8 || GetConnectionState() == 7) {
			AddMessage(0x3f, 0, 0);
			SendLogout();
		} else {
			AddMessage(0x3e, 0, 0);
			AddMessage(4, 0, 0);
		}
	} else {
		if (Event == 29) {
			AddMessage(9, 20, 0);
			AddMessage(10, 300, 0);
		}
		SetActionState(0);
	}
}

// --- TLoginDialog -------------------------------------------------------------------

// 0x46ecf0, line 1162
TLoginDialog::TLoginDialog(void (*OpenURL)(const char *URL))
{
	TComponent::SetSize(SkinProperties().DataB94, SkinProperties().DataB98);
	FocusGroup.reset(new TFocusGroup());
	Account = new TPasswordEditLine(8);
	Account->SetFocusGroup(FocusGroup.get());
	Password = new TPasswordEditLine(29);
	Password->SetFocusGroup(FocusGroup.get());
	TLink *Link = new TLink(TextNoAccountYet, "Create Account", TextSignupURL, OpenURL, SkinProperties().StyleB08.Data78);
	InsertChild(Account);
	SetChildArea(Account, SkinProperties().DataBAC);
	InsertChild(Password);
	SetChildArea(Password, SkinProperties().DataBCC);
	InsertChild(Link);
	SetChildArea(Link, SkinProperties().DataBDC);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data64, TextAccountNumber);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().DataB9C);
	Label = new TLabel(SkinProperties().StyleB08.Data64, TextPassword);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().DataBBC);
	FocusGroup->SetActive(Account);
	Arrange();
}

// 0x46f160, lines 1183-1187
void TLoginDialog::KeyPress(int Key)
{
	if (Key == 13) {
		HandleEvent(1);
	} else if (Key == 27) {
		HandleEvent(2);
	} else {
		TEditLine *FocusEdit = FocusGroup->Active;
		PATELLA_ASSERT(FocusEdit!=nullptr);
		FocusEdit->KeyPress(Key);
	}
}

// 0x46f3e0, line 1203. OK (1) logs in with the account and password.
void TLoginDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		SetAccount(atoi(Account->Text.get()), Password->Text.get()); // NOLINT(bugprone-unchecked-string-to-number-conversion): the original's; the field takes digits only
		AddMessage(0x3d, 0, 0);
		AddMessage(3, 0, 0);
	}
	SetActionState(0);
}

// --- TCharacterListDialog ------------------------------------------------------

// 0x46f5c0, line 1262. The selection is the character played last while
// still in the game, else the first.
TCharacterListDialog::TCharacterListDialog()
{
	Timer = nullptr;
	TComponent::SetSize(SkinProperties().DataBF0, SkinProperties().DataBF4);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextSelectCharacter);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().DataBF8);
	ListBox = new TListBox(NoCallback, TCallback(this, 1));
	InsertChild(ListBox);
	SetChildArea(ListBox, SkinProperties().DataC08);
	const int Characters = GetAccountCharacters();
	for (int i = 0; i < Characters; i++) {
		TFixedString<372> Line;
		Line[0] = 0;
		Line.Format("{} ({})", GetCharacterName(i), GetCharacterWorld(i));
		ListBox->AddLine(Line);
	}
	if (GetConnectionState() == 8 && GetActiveCharacter() < Characters)
		ListBox->SetSelection(GetActiveCharacter());
	else
		ListBox->SetSelection(0);
	TFixedString<256> Status;
	const int PremiumDays = GetPremiumDays();
	if (PremiumDays == 0) {
		Status.Format("<font=4><red={}><green={}><blue={}>Account Status:\nFree Account",
			SkinProperties().StyleB08.Data78.Color.R, SkinProperties().StyleB08.Data78.Color.G,
			SkinProperties().StyleB08.Data78.Color.B);
	} else if (PremiumDays == 0xffff) {
		Status.Format("<font=4><red={}><green={}><blue={}>Account Status:\nGratis Premium Account\n",
			SkinProperties().StyleB08.Data78.Color.R, SkinProperties().StyleB08.Data78.Color.G,
			SkinProperties().StyleB08.Data78.Color.B);
	} else if (PremiumDays < 14) {
		Status.Format(
			"<font=4><red={}><green={}><blue={}>Account Status:\nPremium Account (<red={}><green={}><blue={}>{} day{} left<red={}><green={}><blue={}>)",
			SkinProperties().StyleB08.Data78.Color.R, SkinProperties().StyleB08.Data78.Color.G,
			SkinProperties().StyleB08.Data78.Color.B, 248, 96, 96, PremiumDays, PremiumDays < 2 ? "" : "s",
			SkinProperties().StyleB08.Data78.Color.R, SkinProperties().StyleB08.Data78.Color.G,
			SkinProperties().StyleB08.Data78.Color.B);
	} else {
		Status.Format("<font=4><red={}><green={}><blue={}>Account Status:\nPremium Account ({} day{} left)",
			SkinProperties().StyleB08.Data78.Color.R, SkinProperties().StyleB08.Data78.Color.G,
			SkinProperties().StyleB08.Data78.Color.B, PremiumDays, PremiumDays < 2 ? "" : "s");
	}
	TFormattedText *StatusText = new TFormattedText(Status);
	InsertChild(StatusText);
	SetChildArea(StatusText, SkinProperties().DataC18);
	Arrange();
}

// 0x46fa10, line 1274. Every click starts the timeout again.
bool TCharacterListDialog::LeftButtonDown(int x, int y, int Flags)
{
	SetTimeout(Seconds);
	return TWindow::LeftButtonDown(x, y, Flags);
}

// 0x46fbc0, line 1286
bool TCharacterListDialog::RightButtonUp(int x, int y, int Flags)
{
	SetTimeout(Seconds);
	return TWindow::RightButtonUp(x, y, Flags);
}

// 0x46fd70, line 1298
bool TCharacterListDialog::MouseWheel(int x, int y, int Code)
{
	SetTimeout(Seconds);
	return TWindow::MouseWheel(x, y, Code);
}

// 0x46ff20, line 1309
void TCharacterListDialog::PositionChanged()
{
	SetTimeout(Seconds);
}

// 0x4700b0, line 1335. Another key stops the timer before the list gets it;
// SetTimeout starts it again.
void TCharacterListDialog::KeyPress(int Key)
{
	if (Key == 13) {
		HandleEvent(1);
	} else if (Key == 27) {
		HandleEvent(2);
	} else {
		Timer.reset();
		Timer = nullptr;
		ListBox->KeyPress(Key);
	}
	SetTimeout(Seconds);
}

// 0x470280, line 1361. OK (1) or a double click enters the game with the
// selected character (logging out of a running game first).
void TCharacterListDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const int Selection = ListBox->GetSelection();
		if (Selection >= 0) {
			SetActiveCharacter(Selection);
			if (GetConnectionState() == 8 || GetConnectionState() == 7) {
				AddMessage(0x3f, 0, 0);
				SendLogout();
			} else {
				AddMessage(0x3e, 0, 0);
				AddMessage(4, 0, 0);
			}
		}
	} else if (Event == 31 && !Timer->IsExpired()) {
		return;
	}
	SetActionState(0);
}

// 0x470490, line 1374
void TCharacterListDialog::SetTimeout(int seconds)
{
	Timer.reset();
	Timer.reset(new TTimer(seconds, TCallback(this, 31)));
	Seconds = seconds;
}

// --- TMoveObjectsDialog ---------------------------------------------------------

// 0x470670, lines 1392-1406
TMoveObjectsDialog::TMoveObjectsDialog()
{
	LastDigit = 0;
	DraggedObject = *GetDragObject(&SourceIndex, &SourceX, &SourceY, &SourceZ);
	GetDragTarget(&DestinationX, &DestinationY, &DestinationZ);
	PATELLA_ASSERT(DraggedObject!=0);
	PATELLA_ASSERT_VALUES(In(DraggedObject.Data,1,100), DraggedObject.Data, DraggedObject);
	TComponent::SetSize(SkinProperties().DataC28, SkinProperties().DataC2C);
	TLabel *Label = new TLabel(0, 0, SkinProperties().StyleB08.Data78, TextSetNumberOfItems);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().DataC40);
	ScrollBar = new THorizontalScrollBar(1, DraggedObject.Data, DraggedObject.Data, 11, 1, NoCallback);
	InsertChild(ScrollBar);
	SetChildArea(ScrollBar, SkinProperties().DataC30);
	Arrange();
}

// 0x470b60, line 1423. The pile is drawn with the chosen count; the
// cumulative pattern is computed and not used.
void TMoveObjectsDialog::Draw(int Surface, int x, int y)
{
	DraggedObject.Data = ScrollBar->Range->Value;
	int PatternX = 1;
	int PatternY = 1;
	if (GetObjectFlag(DraggedObject, CUMULATIVE))
		GetCumulativePattern(DraggedObject, &PatternX, &PatternY);
	DrawObject(Surface, SkinProperties().DataC50.x + x, SkinProperties().DataC50.y + y, 32, DraggedObject,
		TRGBColor(0, 0, 0), TRect(SkinProperties().DataC50.x + x, SkinProperties().DataC50.y + y, 32, 32),
		SkinProperties().Data498, true);
	TWindow::Draw(Surface, x, y);
}

// 0x470de0, line 1459. Digits type the count (the last two digits);
// PageUp/PageDown change it by 10, Home/End and the arrow keys as they do
// in a list.
void TMoveObjectsDialog::KeyPress(int Key)
{
	if (Key == 13) {
		HandleEvent(1);
	} else if (Key == 27) {
		HandleEvent(2);
	} else if (Key >= '0' && Key <= '9') {
		ScrollBar->Range->SetValue(LastDigit * 10 + Key - '0');
		LastDigit = Key - '0';
	} else if (Key == 0x114) {
		ScrollBar->Range->SetValue(ScrollBar->Range->Value + 10);
	} else if (Key == 0x115) {
		ScrollBar->Range->SetValue(ScrollBar->Range->Value - 10);
	} else if (Key == 0x112) {
		ScrollBar->Range->SetValue(ScrollBar->Range->Max);
	} else if (Key == 0x113) {
		ScrollBar->Range->SetValue(1);
	} else if (Key == 0x10c) {
		ScrollBar->Range->SetValue(ScrollBar->Range->Value + 1);
	} else if (Key == 0x10f) {
		ScrollBar->Range->SetValue(ScrollBar->Range->Value - 1);
	} else {
		ScrollBar->KeyPress(Key);
	}
}

// 0x471080, line 1475
void TMoveObjectsDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const int Count = ScrollBar->Range->Value;
		if (Count > 0)
			SendMoveObject(SourceX, SourceY, SourceZ, DraggedObject.Type, SourceIndex, DestinationX, DestinationY,
				DestinationZ, Count);
	}
	SetActionState(0);
}

// --- TOutfitDialog ------------------------------------------------------------------

namespace {

// The colour of an outfit colour button: GetOutfitColor runs inside the
// new-expression (after the allocation), so the binary has it in the
// argument list (inlined; the name is ours).
inline TRGBColor OutfitColor(int Color)
{
	int R, G, B;
	GetOutfitColor(Color, &R, &G, &B);
	return TRGBColor(R, G, B);
}

} // namespace

// 0x47cf70, lines 1488-1542. The colour buttons are laid out in rows of
// 19 from SkinProperties.DataCA8; the head is the first part shown.
TOutfitDialog::TOutfitDialog(const int *Data)
{
	PATELLA_ASSERT(Data!=nullptr);
	Outfit = Data[0];
	FirstOutfit = Data[5];
	LastOutfit = Data[6];
	Limit(&Outfit, FirstOutfit, LastOutfit);
	Colors[0] = Data[1];
	Colors[1] = Data[2];
	Colors[2] = Data[3];
	Colors[3] = Data[4];
	TComponent::SetSize(SkinProperties().DataC58, SkinProperties().DataC5C);
	OutfitIcon = new TOutfitIcon(Outfit, 2, Colors.data(), &SkinProperties().DataC74);
	TBorder *Border = new TBorder(OutfitIcon, 0, 9, 0, 11, 10, 0, 8, 0, SkinProperties().DataC70);
	InsertChild(Border);
	SetChildArea(Border, SkinProperties().DataC60);
	TTextButton *NextButton = new TTextButton(0, 0, "Next", TCallback(this, 6), nullptr);
	InsertChild(NextButton);
	SetChildPosition(NextButton, SkinProperties().DataC80.x, SkinProperties().DataC80.y);
	PartGroup.reset(new TRadioButtonGroup);
	for (int i = 0; i < 4; i++) {
		const char *PartName = "";
		switch (i) {
		case 0:
			PartName = "Head";
			break;
		case 1:
			PartName = "Body";
			break;
		case 2:
			PartName = "Legs";
			break;
		case 3:
			PartName = "Feet";
			break;
		default:
			break;
		}
		PartButtons[i] = new TTextButton(70, i * 25, PartName, TCallback(this, i + 1000), nullptr);
		InsertChild(PartButtons[i]);
		SetChildPosition(PartButtons[i], SkinProperties().DataC88[i].x, SkinProperties().DataC88[i].y);
		PartButtons[i]->SetRadioButtonGroup(PartGroup.get());
	}
	ColorGroup.reset(new TRadioButtonGroup);
	for (int i = 0; i < 133; i++) {
		ColorButtons[i] = new TColorButton(0, 0, OutfitColor(i), 22, 23, TCallback(this, i + 2000), nullptr);
		ColorButtons[i]->SetRadioButtonGroup(ColorGroup.get());
		InsertChild(ColorButtons[i]);
		SetChildPosition(ColorButtons[i], SkinProperties().DataCA8.x, SkinProperties().DataCA8.y);
		ColorButtons[i]->SetPosition(i % 19 * SkinProperties().DataCB0 + ColorButtons[i]->OffsetX,
			i / 19 * SkinProperties().DataCB4 + ColorButtons[i]->OffsetY);
	}
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextChooseOutfit);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().DataCB8.x, SkinProperties().DataCB8.y);
	PartGroup->Selected = PartButtons[0];
	HandleEvent(1000);
}

// 0x471250, line 1586. A part button selects the colour button of its
// colour; the loop runs over 216 colours and reads past the 133 buttons
// (into the members after them) when a part has a higher colour (kept).
void TOutfitDialog::HandleEvent(int Event)
{
	if (Event >= 1000 && Event <= 1003) {
		Part = Event - 1000;
		for (int i = 0; i < 216; i++) {
			if (Colors[Part] == i)
				ColorGroup->Selected = ColorButtons[i];
		}
	} else if (Event >= 2000 && Event <= 2215) {
		Colors[Part] = Event - 2000;
		OutfitIcon->SetColor(Part + 1, Colors[Part]);
		OutfitIcon->Arrange();
	} else if (Event == 6) {
		Outfit++;
		if (Outfit > LastOutfit)
			Outfit = FirstOutfit;
		OutfitIcon->SetOutfit(Outfit);
		OutfitIcon->Arrange();
	} else if (Event == 1) {
		SendOutfit(Outfit, Colors.data());
		SetActionState(0);
	} else if (Event == 2) {
		SetActionState(0);
	}
}

// --- TTextDialog ----------------------------------------------------------------

// 0x471500, lines 1702-1727. A read-only text keeps its scroll position
// (SetText's second argument).
TTextDialog::TTextDialog(int MaximumLength, const char *Text, uint32 _ObjectNumber, TObject _Obj,
	const char *Description)
{
	PATELLA_ASSERT(Text!=nullptr);
	ObjectNumber = _ObjectNumber;
	Obj = _Obj;
	TComponent::SetSize(SkinProperties().DataCC0, SkinProperties().DataCC4);
	const bool ReadOnly = !GetObjectFlag(Obj, WRITE) && !(GetObjectFlag(Obj, WRITEONCE) && strlen(Text) == 0);
	EditText = new TEditText(MaximumLength, ReadOnly);
	InsertChild(EditText);
	SetChildArea(EditText, SkinProperties().DataCF4);
	EditText->SetActive(true);
	EditText->SetText(Text, ReadOnly);
	ObjectIcon = new TObjectIcon(Obj, &SkinProperties().DataCD8, false);
	InsertChild(ObjectIcon);
	SetChildArea(ObjectIcon, SkinProperties().DataCC8);
	if (Description != nullptr) {
		TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, Description);
		InsertChild(Label);
		SetChildArea(Label, SkinProperties().DataCE4);
	}
	Arrange();
}

// 0x4719b0, line 1739
void TTextDialog::HandleEvent(int Event)
{
	if (Event == 1)
		SendEditText(ObjectNumber, EditText->Text.get());
	SetActionState(0);
}

// --- TEditListDialog ----------------------------------------------------------

// 0x47d660, lines 1752-1775
TEditListDialog::TEditListDialog(int MaxLength, const char *Text, int _Type, uint32 _Identifier,
	const char *Description)
{
	PATELLA_ASSERT(Text!=nullptr);
	PATELLA_ASSERT_VALUES((int)strlen(Text)<MaxLength, strlen(Text), MaxLength);
	Type = _Type;
	Identifier = _Identifier;
	TComponent::SetSize(SkinProperties().DataD04, SkinProperties().DataD08);
	EditText = new TEditText(MaxLength - 1, false);
	InsertChild(EditText);
	SetChildArea(EditText, SkinProperties().DataD2C);
	EditText->SetActive(true);
	EditText->SetText(Text, false);
	DrawingListLength = 1;
	DrawingList.reset(new TDrawing[1]);
	if (Description != nullptr) {
		TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, Description);
		InsertChild(Label);
		SetChildArea(Label, SkinProperties().DataD1C);
	}
	Arrange();
}

// 0x471b60. The original's catch wrapper's states without a try block; the children keep their
// areas (TWindow::Arrange is not called).
void TEditListDialog::Arrange()
{
	DrawingList[0] = TDrawing(217, SkinProperties().DataD0C.x, SkinProperties().DataD0C.y, Skins()[217].width,
		Skins()[217].height, 0, 0);
}

// 0x471be0, line 1800
void TEditListDialog::HandleEvent(int Event)
{
	if (Event == 1)
		SendEditHouseText(Type, Identifier, EditText->Text.get());
	SetActionState(0);
}

// --- TIgnoreListDialog --------------------------------------------------------

// 0x47db60, lines 1875-1975. Talk.cpp's lists come as one text with a name
// on every line; the names are sorted, each into an entry and a line.
TIgnoreListDialog::TIgnoreListDialog()
	: IgnoreEntries(0, 9, 10), WhiteEntries(0, 9, 10)
{
	TComponent::SetSize(SkinProperties().StyleD3C.Data00, SkinProperties().StyleD3C.Data04);
	IgnoreListCheckBox = new TCheckBox("Activate Ignore List");
	InsertChild(IgnoreListCheckBox);
	SetChildArea(IgnoreListCheckBox, SkinProperties().StyleD3C.Data08);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextCharactersToIgnore);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().StyleD3C.Data18);
	IgnoreList = new TFontListBox(TCallback(this, 11), NoCallback);
	IgnoreList->SetEditable(true);
	InsertChild(IgnoreList);
	SetChildArea(IgnoreList, SkinProperties().StyleD3C.Data28);
	AddIgnoreButton = new TTextButton(SkinProperties().StyleD3C.Data38.x, SkinProperties().StyleD3C.Data38.y, "Add",
		14, 15, TCallback(this, 709), nullptr);
	InsertChild(AddIgnoreButton);
	DeleteIgnoreButton = new TTextButton(SkinProperties().StyleD3C.Data40.x, SkinProperties().StyleD3C.Data40.y,
		"Delete", 14, 15, TCallback(this, 710), nullptr);
	InsertChild(DeleteIgnoreButton);
	IgnorePermanently = new TCheckBox("Ignore Permanently");
	InsertChild(IgnorePermanently);
	SetChildArea(IgnorePermanently, SkinProperties().StyleD3C.Data48);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextGlobalIgnoreSettings);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().StyleD3C.Data58);
	IgnoreYelling = new TCheckBox("Ignore Yelling");
	InsertChild(IgnoreYelling);
	SetChildArea(IgnoreYelling, SkinProperties().StyleD3C.Data68);
	IgnorePrivate = new TCheckBox("Ignore Private Messages");
	InsertChild(IgnorePrivate);
	SetChildArea(IgnorePrivate, SkinProperties().StyleD3C.Data78);
	IgnoreListCheckBox->SetChecked(GetIgnoreListEnabled());
	IgnoreYelling->SetChecked(IsModeIgnored(3));
	IgnorePrivate->SetChecked(IsModeIgnored(4));
	char *Text = GetIgnoreListText();
	if (Text != nullptr) {
		const int Length = (int)strlen(Text);
		int AllocationNumber = 0;
		for (int i = 0; i < Length; i++) {
			if (Text[i] == '\n')
				AllocationNumber++;
		}
		char **Names = new char *[AllocationNumber];
		memset(static_cast<void *>(Names), 0, AllocationNumber * sizeof(char *));
		int Number = 0;
		bool NewLine = true;
		for (int i = 0; i < Length; i++) {
			if (Text[i] == '\n') {
				Text[i] = 0;
				NewLine = true;
			} else if (NewLine) {
				PATELLA_ASSERT_VALUES(Number < AllocationNumber, Number, AllocationNumber);
				Names[Number] = Text + i;
				Number++;
				NewLine = false;
			}
		}
		qsort(static_cast<void *>(Names), Number, sizeof(char *), CompareNames);
		for (int i = 0; i < Number; i++) {
			CopyString(patella::TSpan<char>(IgnoreEntries(i).Name.Chars).first(29), Names[i]);
			IgnoreEntries(i).Permanent = IsPermanentIgnore(Names[i]);
			TFont Font;
			if (IgnoreEntries(i).Permanent)
				Font.Color = SkinProperties().StyleD3C.Data88;
			IgnoreList->AddLine(Names[i], Font);
		}
		delete[] Names;
		if (Number > 0)
			IgnoreList->SetSelection(0);
	}
	if (Text != nullptr)
		delete[] Text;
	WhiteListCheckBox = new TCheckBox("Activate White List");
	InsertChild(WhiteListCheckBox);
	SetChildArea(WhiteListCheckBox, SkinProperties().StyleD3C.Data94);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextCharactersToAllow);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().StyleD3C.DataA4);
	WhiteList = new TFontListBox(TCallback(this, 11), NoCallback);
	WhiteList->SetEditable(true);
	InsertChild(WhiteList);
	SetChildArea(WhiteList, SkinProperties().StyleD3C.DataB4);
	AddWhiteButton = new TTextButton(SkinProperties().StyleD3C.DataC4.x, SkinProperties().StyleD3C.DataC4.y, "Add",
		14, 15, TCallback(this, 712), nullptr);
	InsertChild(AddWhiteButton);
	DeleteWhiteButton = new TTextButton(SkinProperties().StyleD3C.DataCC.x, SkinProperties().StyleD3C.DataCC.y,
		"Delete", 14, 15, TCallback(this, 713), nullptr);
	InsertChild(DeleteWhiteButton);
	AllowPermanently = new TCheckBox("Allow Permanently");
	InsertChild(AllowPermanently);
	SetChildArea(AllowPermanently, SkinProperties().StyleD3C.DataD4);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextGlobalAllowSettings);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().StyleD3C.DataE4);
	AllowBuddies = new TCheckBox("Allow VIPs to message you");
	InsertChild(AllowBuddies);
	SetChildArea(AllowBuddies, SkinProperties().StyleD3C.DataF4);
	WhiteListCheckBox->SetChecked(GetIgnoreEnabled());
	AllowBuddies->SetChecked(GetBuddyIgnore());
	Text = GetBuddyIgnoreText();
	if (Text != nullptr) {
		const int Length = (int)strlen(Text);
		int AllocationNumber = 0;
		for (int i = 0; i < Length; i++) {
			if (Text[i] == '\n')
				AllocationNumber++;
		}
		char **Names = new char *[AllocationNumber];
		memset(static_cast<void *>(Names), 0, AllocationNumber * sizeof(char *));
		int Number = 0;
		bool NewLine = true;
		for (int i = 0; i < Length; i++) {
			if (Text[i] == '\n') {
				Text[i] = 0;
				NewLine = true;
			} else if (NewLine) {
				PATELLA_ASSERT_VALUES(Number < AllocationNumber, Number, AllocationNumber);
				Names[Number] = Text + i;
				Number++;
				NewLine = false;
			}
		}
		qsort(static_cast<void *>(Names), Number, sizeof(char *), CompareNames);
		for (int i = 0; i < Number; i++) {
			CopyString(patella::TSpan<char>(WhiteEntries(i).Name.Chars).first(29), Names[i]);
			WhiteEntries(i).Permanent = IsPermanentBuddyIgnore(Names[i]);
			TFont Font;
			if (WhiteEntries(i).Permanent)
				Font.Color = SkinProperties().StyleD3C.Data88;
			WhiteList->AddLine(Names[i], Font);
		}
		delete[] Names;
		if (Number > 0)
			WhiteList->SetSelection(0);
	}
	if (Text != nullptr)
		delete[] Text;
	Arrange();
	HandleEvent(11);
}

// 0x471d90, line 1987
void TIgnoreListDialog::Arrange()
{
	TWindow::Arrange();
}

// 0x471f20, line 2171. OK (602) replaces Talk.cpp's lists with the lines of
// the list boxes (except empty lines and "new entry") and sets the global
// switches; OK then closes the dialog like every event not handled below.
// Add (709, 712) keeps the line being edited in its entry and appends
// "new entry" for editing; Delete (710, 713) removes the selected entry and
// rebuilds the list from the entries (the loops stop one short of the
// vector's last index, so they never see the entry moved last). A
// selection (11) shows the selected entries' permanent flags; it computes
// each entry once more without using it (kept).
void TIgnoreListDialog::HandleEvent(int Event)
{
	if (Event == 602) {
		IgnoreList->StopEdit();
		ClearIgnoreList();
		for (int i = 0; i < IgnoreList->GetNumberOfLines(); i++) {
			TFont Font;
			const char *Name = IgnoreList->GetLine(i, &Font);
			if (strlen(Name) != 0 && strcmp(Name, "new entry") != 0)
				IgnoreName(Name, IgnoreEntries(i).Permanent);
		}
		SetIgnoreListEnabled(IgnoreListCheckBox->IsChecked());
		if (IgnoreYelling->IsChecked())
			IgnoreMode(3);
		else
			UnignoreMode(3);
		if (IgnorePrivate->IsChecked())
			IgnoreMode(4);
		else
			UnignoreMode(4);
		WhiteList->StopEdit();
		ClearBuddyIgnore();
		for (int i = 0; i < WhiteList->GetNumberOfLines(); i++) {
			TFont Font;
			const char *Name = WhiteList->GetLine(i, &Font);
			if (strlen(Name) != 0 && strcmp(Name, "new entry") != 0)
				AddBuddyIgnore(Name, WhiteEntries(i).Permanent);
		}
		SetIgnoreEnabled(WhiteListCheckBox->IsChecked());
		SetBuddyIgnore(AllowBuddies->IsChecked());
	}
	if (Event == 709) {
		WhiteList->StopEdit();
		IgnoreList->StopEdit();
		const int Selection = IgnoreList->GetSelection();
		if (Selection > -1) {
			TFont Font;
			const char *Name = IgnoreList->GetLine(Selection, &Font);
			IgnoreEntries(Selection).AssignName(Name);
			IgnoreEntries(Selection).Permanent = IgnorePermanently->IsChecked();
		}
		const TFont Font;
		TIgnoreEntry Entry;
		CopyString(Entry.Name.Chars, "new entry");
		IgnoreEntries(IgnoreList->GetNumberOfLines()) = Entry;
		IgnoreList->AddLine("new entry", Font);
		IgnoreList->SetSelection(IgnoreList->GetNumberOfLines() - 1);
		IgnorePermanently->SetChecked(false);
		IgnoreList->StartEdit();
	} else if (Event == 712) {
		IgnoreList->StopEdit();
		WhiteList->StopEdit();
		const int Selection = WhiteList->GetSelection();
		if (Selection > -1) {
			TFont Font;
			const char *Name = WhiteList->GetLine(Selection, &Font);
			WhiteEntries(Selection).AssignName(Name);
			WhiteEntries(Selection).Permanent = AllowPermanently->IsChecked();
		}
		const TFont Font;
		TIgnoreEntry Entry;
		CopyString(Entry.Name.Chars, "new entry");
		WhiteEntries(WhiteList->GetNumberOfLines()) = Entry;
		WhiteList->AddLine("new entry", Font);
		WhiteList->SetSelection(WhiteList->GetNumberOfLines() - 1);
		AllowPermanently->SetChecked(false);
		WhiteList->StartEdit();
	} else if (Event == 710) {
		IgnoreList->StopEdit();
		const int Selection = IgnoreList->GetSelection();
		if (Selection > -1) {
			TFont Font;
			const char *Name = IgnoreList->GetLine(Selection, &Font);
			IgnoreEntries(Selection).AssignName(Name);
			IgnoreEntries(Selection).Permanent = IgnorePermanently->IsChecked();
			IgnoreList->Clear();
			int i;
			for (i = 0; i < IgnoreEntries.max - IgnoreEntries.min; i++) {
				if (i >= Selection)
					IgnoreEntries(i) = IgnoreEntries(i + 1);
			}
			IgnoreEntries(i).Name[0] = 0;
			for (i = 0; i < IgnoreEntries.max - IgnoreEntries.min; i++) {
				if (IgnoreEntries(i).Name.Length() != 0) {
					TFont LineFont;
					if (IgnoreEntries(i).Permanent)
						LineFont.Color = SkinProperties().StyleD3C.Data88;
					IgnoreList->AddLine(IgnoreEntries(i).NameText(), LineFont);
				}
			}
		}
		IgnoreList->SetSelection(-1);
	} else if (Event == 713) {
		WhiteList->StopEdit();
		const int Selection = WhiteList->GetSelection();
		if (Selection > -1) {
			TFont Font;
			const char *Name = WhiteList->GetLine(Selection, &Font);
			WhiteEntries(Selection).AssignName(Name);
			WhiteEntries(Selection).Permanent = AllowPermanently->IsChecked();
			WhiteList->Clear();
			int i;
			for (i = 0; i < WhiteEntries.max - WhiteEntries.min; i++) {
				if (i >= Selection)
					WhiteEntries(i) = WhiteEntries(i + 1);
			}
			WhiteEntries(i).Name[0] = 0;
			for (i = 0; i < WhiteEntries.max - WhiteEntries.min; i++) {
				if (WhiteEntries(i).Name.Length() != 0) {
					TFont LineFont;
					if (WhiteEntries(i).Permanent)
						LineFont.Color = SkinProperties().StyleD3C.Data88;
					WhiteList->AddLine(WhiteEntries(i).NameText(), LineFont);
				}
			}
		}
		WhiteList->SetSelection(-1);
	} else if (Event == 11) {
		int Selection = IgnoreList->GetSelection();
		if (Selection != -1) {
			IgnoreEntries(Selection);
			IgnorePermanently->SetChecked(IgnoreEntries(Selection).Permanent);
		}
		Selection = WhiteList->GetSelection();
		if (Selection != -1) {
			WhiteEntries(Selection);
			AllowPermanently->SetChecked(WhiteEntries(Selection).Permanent);
		}
	} else {
		SetActionState(0);
	}
}

// 0x472970, line 2205. A click that changes a permanent check box recolours
// the selected line and stores the flag in its entry.
bool TIgnoreListDialog::LeftButtonUp(int x, int y, int Flags)
{
	const bool Result = TWindow::LeftButtonUp(x, y, Flags);
	int Selection = IgnoreList->GetSelection();
	if (Result && Selection > -1 && IgnoreEntries(Selection).Permanent != IgnorePermanently->IsChecked()) {
		TFont Font;
		IgnoreList->StopEdit();
		TFixedString<30> Name;
		CopyString(Name.Chars, IgnoreList->GetLine(Selection, &Font));
		TFont LineFont;
		if (IgnorePermanently->IsChecked())
			LineFont.Color = SkinProperties().StyleD3C.Data88;
		IgnoreList->SetLine(Selection, Name, LineFont);
		TIgnoreEntry Entry;
		Entry.Permanent = IgnorePermanently->IsChecked();
		CopyString(Entry.Name.Chars, Name);
		IgnoreEntries(Selection) = Entry;
	}
	Selection = WhiteList->GetSelection();
	if (Result && Selection > -1 && WhiteEntries(Selection).Permanent != AllowPermanently->IsChecked()) {
		TFont Font;
		WhiteList->StopEdit();
		TFixedString<30> Name;
		CopyString(Name.Chars, WhiteList->GetLine(Selection, &Font));
		TFont LineFont;
		if (AllowPermanently->IsChecked())
			LineFont.Color = SkinProperties().StyleD3C.Data88;
		WhiteList->SetLine(Selection, Name, LineFont);
		TIgnoreEntry Entry;
		Entry.Permanent = AllowPermanently->IsChecked();
		CopyString(Entry.Name.Chars, Name);
		WhiteEntries(Selection) = Entry;
	}
	return Result;
}

// 0x472d80, line 2216
void TIgnoreListDialog::KeyPress(int Key)
{
	IgnoreList->KeyPress(Key);
	WhiteList->KeyPress(Key);
}

// --- TBugReportDialog ------------------------------------------------------------

// 0x472f20, line 2240
TBugReportDialog::TBugReportDialog()
{
	TComponent::SetSize(SkinProperties().DataE40, SkinProperties().DataE44);
	EditText = new TEditText(1024, false);
	InsertChild(EditText);
	SetChildArea(EditText, SkinProperties().DataE48);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data64, TextReportBugsOnly);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().DataE58);
	EditText->SetActive(true);
	Arrange();
}

// 0x4731e0, line 2252
void TBugReportDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const char *Text = EditText->Text.get();
		if (strlen(Text) != 0)
			SendBugReport(Text);
	}
	SetActionState(0);
}

// --- TNewChannelDialog -----------------------------------------------------------

// 0x4733a0, lines 2265-2297
TNewChannelDialog::TNewChannelDialog(int _NumberOfChannels, const int *_ChannelIDs, const char (*ChannelNames)[30]) // NOLINT(modernize-avoid-c-arrays): Dialogs.h
{
	PATELLA_ASSERT(_ChannelIDs!=nullptr);
	PATELLA_ASSERT_VALUES(_NumberOfChannels>0, _NumberOfChannels);
	TComponent::SetSize(SkinProperties().DataECC, SkinProperties().DataED0);
	ListBox = new TListBox(NoCallback, TCallback(this, 301));
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextSelectChannel);
	InsertChild(ListBox);
	InsertChild(Label);
	SetChildArea(ListBox, SkinProperties().DataEE4);
	SetChildArea(Label, SkinProperties().DataED4);
	NameEdit = new TEditLine(29, false);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextOpenPrivateChannel);
	InsertChild(NameEdit);
	InsertChild(Label);
	SetChildArea(NameEdit, SkinProperties().DataF04);
	SetChildArea(Label, SkinProperties().DataEF4);
	NumberOfChannels = _NumberOfChannels;
	ChannelIDs.reset(new int[_NumberOfChannels]);
	for (int i = 0; i < NumberOfChannels; i++) {
		ChannelIDs[i] = _ChannelIDs[i];
		ListBox->AddLine(ChannelNames[i]);
	}
	NameEdit->SetActive(true);
	Arrange();
}

// 0x473980, line 2321. Return sends 301 (join the selected channel)
// without a name and 302 (nothing but closing) with one; the frame's OK
// button takes Return first.
void TNewChannelDialog::KeyPress(int Key)
{
	if (Key == 13) {
		if (strlen(NameEdit->Text.get()) != 0)
			HandleEvent(302);
		else
			HandleEvent(301);
	} else if (Key == 27) {
		HandleEvent(2);
	} else {
		NameEdit->KeyPress(Key);
	}
}

// 0x473b80, lines 2333-2349. OK (1) with a name opens a private
// conversation (not with oneself), without one it joins the selected
// channel like a double click (301).
void TNewChannelDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const char *Name = NameEdit->Text.get();
		if (strlen(Name) != 0) {
			TCreature *Creature = GetCreature(GetPlayerID());
			PATELLA_ASSERT(Creature!=nullptr);
			if (CompareStrings(Creature->Name, Name, -1) == 0)
				SetStatusMessage(TextNoPrivateChannelWithYourself, false);
			else
				SendOpenPrivateConversation(Name);
		} else {
			Event = 301;
		}
	}
	if (Event == 301) {
		const int Selection = ListBox->GetSelection();
		if (Selection != -1) {
			const int ChannelID = ChannelIDs[Selection];
			if (ChannelID == 0xffff)
				SendOpenPrivateChannel();
			else
				SendJoinChannel(ChannelID);
		}
	}
	SetActionState(0);
}

// --- TPrivateChannelDialog -------------------------------------------------------

// 0x473e60, line 2378
TPrivateChannelDialog::TPrivateChannelDialog(bool _Exclude)
{
	Exclude = _Exclude;
	TComponent::SetSize(SkinProperties().DataF14, SkinProperties().DataF18);
	NameEdit = new TEditLine(29, false);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextEnterCharacterName);
	InsertChild(NameEdit);
	InsertChild(Label);
	SetChildArea(NameEdit, SkinProperties().DataF1C);
	SetChildArea(Label, SkinProperties().DataF2C);
	NameEdit->SetActive(true);
	Arrange();
}

// 0x474110, line 2392
void TPrivateChannelDialog::KeyPress(int Key)
{
	if (Key == 13)
		HandleEvent(1);
	else if (Key == 27)
		HandleEvent(2);
	else
		NameEdit->KeyPress(Key);
}

// 0x4742e0, line 2406
void TPrivateChannelDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const char *Name = NameEdit->Text.get();
		if (strlen(Name) != 0) {
			if (!Exclude)
				SendInviteToPrivateChannel(Name);
			else
				SendExcludeFromPrivateChannel(Name);
		}
	}
	SetActionState(0);
}

// --- TReportDialog ----------------------------------------------------------------

// 0x4744b0, line 2551
TReportDialog::TReportDialog()
{
	TComponent::SetSize(SkinProperties().DataF3C, SkinProperties().DataF40);
	TextEdit = new TEditLine(255, false);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextStateRuleViolation);
	InsertChild(TextEdit);
	InsertChild(Label);
	SetChildArea(TextEdit, SkinProperties().DataF44);
	SetChildArea(Label, SkinProperties().DataF54);
	TextEdit->SetActive(true);
	Arrange();
}

// 0x474760, line 2565
void TReportDialog::KeyPress(int Key)
{
	if (Key == 13)
		HandleEvent(1);
	else if (Key == 27)
		HandleEvent(2);
	else
		TextEdit->KeyPress(Key);
}

// 0x474930, line 2580. The request goes to Control.cpp's event queue as a
// copy (event 0x33).
void TReportDialog::HandleEvent(int Event)
{
	if (Event == 1 && strlen(TextEdit->Text.get()) != 0) {
		const std::size_t Size = strlen(TextEdit->Text.get()) + 1;
		char *Text = new char[Size];
		TStringBuffer(Text, Size).Assign(TextEdit->Text.get());
		AddMessage(0x33, reinterpret_cast<std::intptr_t>(Text), 0);
	}
	SetActionState(0);
}

// --- TAddBuddyDialog ------------------------------------------------------------

// 0x474b20, line 2607
TAddBuddyDialog::TAddBuddyDialog()
{
	TComponent::SetSize(SkinProperties().DataF64, SkinProperties().DataF68);
	NameEdit = new TEditLine(29, false);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextEnterCharacterName2);
	InsertChild(NameEdit);
	InsertChild(Label);
	SetChildArea(NameEdit, SkinProperties().DataF6C);
	SetChildArea(Label, SkinProperties().DataF7C);
	NameEdit->SetActive(true);
	Arrange();
}

// 0x474dd0, line 2621
void TAddBuddyDialog::KeyPress(int Key)
{
	if (Key == 13)
		HandleEvent(1);
	else if (Key == 27)
		HandleEvent(2);
	else
		NameEdit->KeyPress(Key);
}

// 0x474fa0, line 2635
void TAddBuddyDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const char *Name = NameEdit->Text.get();
		if (strlen(Name) != 0)
			SendAddBuddy(Name);
	}
	SetActionState(0);
}

// --- TEditBuddyDialog -----------------------------------------------------------

// 0x475160, lines 2652-2696. The icons are skins 218-227 after "none".
TEditBuddyDialog::TEditBuddyDialog(int _CharacterID)
{
	CharacterID = _CharacterID;
	const int Position = GetBuddyPosition(_CharacterID);
	PATELLA_ASSERT_VALUES(Position >= 0, Position, CharacterID);
	TComponent::SetSize(SkinProperties().StyleF8C.Data00, SkinProperties().StyleF8C.Data04);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data64, GetBuddyListName(Position));
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().StyleF8C.Data08);
	TSeparator *Separator = new TSeparator(true);
	InsertChild(Separator);
	SetChildArea(Separator, SkinProperties().StyleF8C.Data18);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextSelectType);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().StyleF8C.Data28);
	IconGroup.reset(new TRadioButtonGroup);
	for (int i = 0; i < 11; i++) {
		int Icon;
		if (i == 0)
			Icon = 0;
		else
			Icon = i + 217;
		IconButtons[i] = new TIconButton(0, 0, Icon, 24, 25, NoCallback, nullptr);
		InsertChild(IconButtons[i]);
		IconButtons[i]->SetRadioButtonGroup(IconGroup.get());
		if (GetBuddyListIcon(Position) == i)
			IconGroup->Selected = IconButtons[i];
		SetChildPosition(IconButtons[i], SkinProperties().StyleF8C.Data38[i].x, SkinProperties().StyleF8C.Data38[i].y);
	}
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextEnterShortDescription);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().StyleF8C.Data90);
	Description = new TEditText(127, false);
	InsertChild(Description);
	SetChildArea(Description, SkinProperties().StyleF8C.DataA0);
	if (GetBuddyListDescription(Position) != nullptr)
		Description->SetText(GetBuddyListDescription(Position), false);
	Notify = new TCheckBox("Notify at login");
	InsertChild(Notify);
	SetChildArea(Notify, SkinProperties().StyleF8C.DataB0);
	Notify->SetChecked(GetBuddyListNotify(Position));
	Description->SetActive(true);
	Arrange();
}

// 0x475820, line 2720. Without a selected icon the icon is 0.
void TEditBuddyDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		int Icon = 0;
		for (int i = 0; i < 11; i++) {
			if (IconGroup->Selected == IconButtons[i]) {
				Icon = i;
				break;
			}
		}
		EditBuddy(CharacterID, Icon, Description->Text.get(), Notify->IsChecked());
	}
	SetActionState(0);
}

// --- TUpdateDialog -----------------------------------------------------------------

// TUpdateDialog ("Updating ... Please wait": the progress of a patch from the login server; constructor 0x4759f0, line
// 2761, Draw 0x475d00, line 2793, HandleEvent 0x475f00, line 2806) is gone with the patcher since Patella-Modern issue
// #75 (decision D9).

// --- THelpDialog -------------------------------------------------------------------

// 0x4760a0, line 2869. No Arrange at the end.
THelpDialog::THelpDialog(void (*_OpenURL)(const char *URL))
{
	OpenURL = _OpenURL;
	TComponent::SetSize(SkinProperties().Style1084.Data00, SkinProperties().Style1084.Data04);
	TTextButton *Button = new TTextButton(0, 0, "Client Help", 18, 19, TCallback(this, 702), nullptr);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextClientInfo);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1084.Data08.x, SkinProperties().Style1084.Data08.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1084.Data38.x, SkinProperties().Style1084.Data38.y);
	Button = new TTextButton(0, 0, "Hints", 18, 19, TCallback(this, 703), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextShowPreviousHints);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1084.Data10.x, SkinProperties().Style1084.Data10.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1084.Data40.x, SkinProperties().Style1084.Data40.y);
	Button = new TTextButton(0, 0, "Tutor Help", 18, 19, TCallback(this, 704), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextAskTutor);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1084.Data18.x, SkinProperties().Style1084.Data18.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1084.Data48.x, SkinProperties().Style1084.Data48.y);
	Button = new TTextButton(0, 0, "Rule Violation", 18, 19, TCallback(this, 705), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextReportRuleViolations);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1084.Data20.x, SkinProperties().Style1084.Data20.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1084.Data50.x, SkinProperties().Style1084.Data50.y);
	Button = new TTextButton(0, 0, "Manual", 18, 19, TCallback(this, 706), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextReadManual);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1084.Data28.x, SkinProperties().Style1084.Data28.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1084.Data58.x, SkinProperties().Style1084.Data58.y);
	Button = new TTextButton(0, 0, "FAQ", 18, 19, TCallback(this, 707), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextReadFAQ);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1084.Data30.x, SkinProperties().Style1084.Data30.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1084.Data60.x, SkinProperties().Style1084.Data60.y);
}

// 0x476770, line 2882
void THelpDialog::KeyPress(int Key)
{
	if (Key == 13)
		HandleEvent(1);
	else if (Key == 27)
		HandleEvent(2);
}

// 0x476930, lines 2914-2918. Client Help (702) closes first and then asks
// for the client info; Hints (703) opens the hints dialog (0x1f) after
// closing the help menu (0x20).
void THelpDialog::HandleEvent(int Event)
{
	switch (Event) {
	case 1:
	case 2:
		SetActionState(0);
		break;
	case 702:
		SetActionState(0);
		AddMessage(0x2e, 0, 0);
		break;
	case 703:
		AddMessage(0x20, 0, 0);
		AddMessage(0x1f, 0, 0);
		SetActionState(0);
		break;
	case 704:
		SendJoinChannel(7);
		SetActionState(0);
		break;
	case 705:
		AddMessage(0x16, 0, 0);
		SetActionState(0);
		break;
	case 706:
		OpenURL(TextManualURL);
		SetActionState(0);
		break;
	case 707:
		OpenURL(TextFAQURL);
		SetActionState(0);
		break;
	default:
		TERROR1("unknown event", Event);
	}
}

// --- THintsDialog ------------------------------------------------------------------

// 0x476cd0, line 2945. The hints 1-14 the player has seen.
THintsDialog::THintsDialog()
{
	TComponent::SetSize(SkinProperties().Data10EC, SkinProperties().Data10F0);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextAvailableHints);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Data10F4);
	ListBox = new TListBox(NoCallback, TCallback(this, 1));
	InsertChild(ListBox);
	SetChildArea(ListBox, SkinProperties().Data1104);
	for (int i = 1; i < 15; i++) {
		if (IsKnownHint(i))
			ListBox->AddLine(GetEventHelpTitle(i));
	}
}

// 0x476fb0, line 2959
void THintsDialog::KeyPress(int Key)
{
	if (Key == 13)
		HandleEvent(1);
	else if (Key == 27)
		HandleEvent(2);
	else
		ListBox->KeyPress(Key);
}

// 0x477180, lines 2974-2983. The selected hint is shown again: the help
// menu (0x20) and the hint (2) are put first in the event queue.
void THintsDialog::HandleEvent(int Event)
{
	if (Event == 1 && ListBox->GetSelection() != -1) {
		const char *Title = ListBox->GetLine(ListBox->GetSelection());
		int Number = -1;
		for (int i = 1; i < 15; i++) {
			if (CompareStrings(Title, GetEventHelpTitle(i), -1) == 0) {
				Number = i;
				break;
			}
		}
		PATELLA_ASSERT(Number!=-1);
		const char *HelpText = GetEventHelpText(Number);
		const std::size_t Size = strlen(HelpText) + 1;
		char *Text = new char[Size];
		TStringBuffer(Text, Size).Assign(HelpText);
		AddMessageFirst(0x20, 0, 0);
		AddMessageFirst(2, reinterpret_cast<std::intptr_t>(GetEventHelpTitle(Number)), reinterpret_cast<std::intptr_t>(Text));
	}
	SetActionState(0);
}

// --- TRuleViolationDialog --------------------------------------------------------

// 0x484620, line 3056. A given name (or the statement's author) cannot be
// changed; the statement line is read-only and as long as its text.
TRuleViolationDialog::TRuleViolationDialog(const char *Name, TTalkLine *Statement)
{
	for (int i = 0; i < 50; i++)
		Reasons[i] = -1;
	StatementID = Statement != nullptr ? Statement->StatementID : 0;
	TComponent::SetSize(SkinProperties().Style1114.Data00, SkinProperties().Style1114.Data04);
	FocusGroup.reset(new TFocusGroup());
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, "Name");
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Style1114.Data08);
	NameEdit = new TEditLine(29, Name != nullptr || Statement != nullptr);
	if (Name != nullptr)
		NameEdit->SetText(Name);
	if (Statement != nullptr)
		NameEdit->SetText(Statement->Addressee);
	NameEdit->SetFocusGroup(FocusGroup.get());
	InsertChild(NameEdit);
	SetChildArea(NameEdit, SkinProperties().Style1114.Data18);
	Label = new TLabel(SkinProperties().StyleB08.Data64, "Reason");
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Style1114.Data28);
	ReasonList = new TListBox(TCallback(this, 21), NoCallback);
	InsertChild(ReasonList);
	SetChildArea(ReasonList, SkinProperties().Style1114.Data38);
	Label = new TLabel(SkinProperties().StyleB08.Data64, "Action");
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Style1114.Data48);
	ActionList = new TListBox(NoCallback, NoCallback);
	InsertChild(ActionList);
	SetChildArea(ActionList, SkinProperties().Style1114.Data58);
	Label = new TLabel(SkinProperties().StyleB08.Data64, "Comment");
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Style1114.Data68);
	CommentEdit = new TEditLine(100, false);
	CommentEdit->SetFocusGroup(FocusGroup.get());
	InsertChild(CommentEdit);
	SetChildArea(CommentEdit, SkinProperties().Style1114.Data78);
	Label = new TLabel(SkinProperties().StyleB08.Data64, "Statement");
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Style1114.Data88);
	StatementEdit = new TEditLine(Statement != nullptr ? (int)strlen(Statement->Text) : 0, true);
	if (Statement != nullptr)
		StatementEdit->SetText(Statement->Text);
	StatementEdit->SetFocusGroup(FocusGroup.get());
	InsertChild(StatementEdit);
	SetChildArea(StatementEdit, SkinProperties().Style1114.Data98);
	IPBanishment = new TCheckBox("IP Address Banishment");
	IPBanishment->SetChecked(false);
	IPBanishment->SetEnabled(false);
	InsertChild(IPBanishment);
	SetChildArea(IPBanishment, SkinProperties().Style1114.DataA8);
	int Number = 0;
	for (int i = 0; i < 32; i++) {
		if (CheckRight(i, -1)) {
			ReasonList->AddLine(GetRightName(i));
			Reasons[Number] = i;
			Number++;
		}
	}
	UpdateActions();
	FocusGroup->SetActive(NameEdit->ReadOnly ? CommentEdit : NameEdit);
	Arrange();
}

// 0x4774d0, line 3080
void TRuleViolationDialog::KeyPress(int Key)
{
	if (Key == 13) {
		HandleEvent(1);
	} else if (Key == 27) {
		HandleEvent(2);
	} else if (FocusGroup->Active != nullptr) {
		FocusGroup->Active->KeyPress(Key);
	}
}

// 0x484e70, lines 3124-3128. Every missing part gets a status message; the
// dialog closes in any case. A disabled statement line sends statement 0.
void TRuleViolationDialog::HandleEvent(int Event)
{
	switch (Event) {
	case 1: {
		bool Valid = true;
		const int Reason = GetReason();
		const int Action = GetAction();
		if (strlen(NameEdit->Text.get()) == 0) {
			SetStatusMessage(TextNoPlayerName, false);
			Valid = false;
		}
		if (Reason == -1) {
			SetStatusMessage(TextNoReason, false);
			Valid = false;
		}
		if (Action == -1) {
			SetStatusMessage(TextNoAction, false);
			Valid = false;
		}
		if (strlen(CommentEdit->Text.get()) == 0) {
			SetStatusMessage(TextNoComment, false);
			Valid = false;
		}
		if (Action == 6) {
			if (strlen(StatementEdit->Text.get()) == 0) {
				SetStatusMessage(TextNoStatement, false);
				Valid = false;
			} else if (StatementID == 0) {
				SetStatusMessage(TextCannotReportStatement, false);
				Valid = false;
			}
		}
		if (Valid) {
			const uint32 ID = StatementEdit->IsDisabled() ? 0 : StatementID;
			SendRuleViolationReport(NameEdit->Text.get(), Reason, Action, CommentEdit->Text.get(), ID,
				IPBanishment->IsChecked());
		}
		break;
	}
	case 2:
		break;
	case 21:
		UpdateActions();
		return;
	default:
		TERROR1("unknown event", Event);
	}
	SetActionState(0);
}

// 0x47eae0, line 3158. The actions the reason allows; the statement line
// is enabled for reasons with action 0, 2 or 6 among the statement reasons
// 9-27 and 29.
void TRuleViolationDialog::UpdateActions()
{
	const int Reason = GetReason();
	if (Reason != -1) {
		ActionList->Clear();
		for (int i = 0; i < 7; i++) {
			if (CheckRight(Reason, i))
				ActionList->AddLine(GetActionName(i));
		}
		const bool IPBanishmentAllowed = CheckRight(Reason, 7);
		IPBanishment->SetChecked(false);
		IPBanishment->SetEnabled(IPBanishmentAllowed);
		StatementEdit->SetDisabled(!(CheckRight(Reason, 0) || CheckRight(Reason, 2) || CheckRight(Reason, 6))
			|| ((Reason < 9 || Reason > 27) && Reason != 29));
	}
}

// 0x4776b0, line 3171
int TRuleViolationDialog::GetReason()
{
	const int Selection = ReasonList->GetSelection();
	if (Selection == -1)
		return -1;
	return Reasons[Selection];
}

// 0x477860, line 3191. The action of the selected line, by its name.
int TRuleViolationDialog::GetAction() const
{
	const int Selection = ActionList->GetSelection();
	if (Selection == -1)
		return -1;
	for (int i = 0; i < 7; i++) {
		if (strcmp(ActionList->GetLine(Selection), GetActionName(i)) == 0)
			return i;
	}
	return -1;
}

// --- TEditMarkDialog ------------------------------------------------------------

// 0x477c80, lines 3207-3242. The symbols are skins 74-87.
TEditMarkDialog::TEditMarkDialog(TCoordinate *_Coordinate, TMark *_Mark)
{
	Coordinate.x = 0;
	Coordinate.y = 0;
	Coordinate.z = 0;
	TComponent::SetSize(SkinProperties().Data11CC, SkinProperties().Data11D0);
	PATELLA_ASSERT(_Coordinate != nullptr);
	Coordinate = *_Coordinate;
	if (_Mark != nullptr)
		Mark = *_Mark;
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextEnterShortDescription2);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Data11D4);
	TextEdit = new TEditLine(100, false);
	if (Mark.Text != nullptr)
		TextEdit->SetText(Mark.Text);
	InsertChild(TextEdit);
	SetChildArea(TextEdit, SkinProperties().Data11E4);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextSelectType2);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Data11F4);
	TypeGroup.reset(new TRadioButtonGroup);
	for (int i = 0; i < 14; i++) {
		TypeButtons[i] = new TIconButton(0, 0, i + 74, 24, 25, NoCallback, nullptr);
		InsertChild(TypeButtons[i]);
		TypeButtons[i]->SetRadioButtonGroup(TypeGroup.get());
		if (Mark.Type == i)
			TypeGroup->Selected = TypeButtons[i];
		SetChildPosition(TypeButtons[i], SkinProperties().Data1204[i].x, SkinProperties().Data1204[i].y);
	}
	TextEdit->SetActive(true);
	Arrange();
	delete _Coordinate;
	if (_Mark != nullptr)
		delete _Mark;
}

// 0x478200, line 3268
void TEditMarkDialog::KeyPress(int Key)
{
	if (Key == 13)
		HandleEvent(1);
	else if (Key == 27)
		HandleEvent(2);
	else
		TextEdit->KeyPress(Key);
}

// 0x4783d0, lines 3294-3298. An edited mark is deleted and added again at
// its own position.
void TEditMarkDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		CopyString(Mark.Text.Chars, TextEdit->Text.get());
		int Type = 0;
		for (int i = 0; i < 14; i++) {
			if (TypeGroup->Selected == TypeButtons[i]) {
				Type = i;
				break;
			}
		}
		if (Mark.x != 0 && Mark.y != 0) {
			DeleteAutomapMark(Mark, Coordinate.z);
			Coordinate.x = Mark.x;
			Coordinate.y = Mark.y;
		}
		AddAutomapMark(Coordinate, Mark.Text, Type);
	} else if (Event != 2) {
		TERROR1("unknown event", Event);
	}
	SetActionState(0);
}

// --- TOptionsDialog -----------------------------------------------------------------

// 0x4786d0, line 3366. The labels of Network and Motd use the label
// constructor with a position.
TOptionsDialog::TOptionsDialog()
{
	TComponent::SetSize(SkinProperties().Style1274.Data00, SkinProperties().Style1274.Data04);
	TTextButton *Button = new TTextButton(0, 0, "General", 18, 19, TCallback(this, 404), nullptr);
	TLabel *Label = new TLabel(SkinProperties().StyleB08.Data78, TextChangeGeneralOptions);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1274.Data08.x, SkinProperties().Style1274.Data08.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1274.Data38.x, SkinProperties().Style1274.Data38.y);
	Button = new TTextButton(0, 0, "Graphics", 18, 19, TCallback(this, 401), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextChangeGraphicsOptions);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1274.Data10.x, SkinProperties().Style1274.Data10.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1274.Data40.x, SkinProperties().Style1274.Data40.y);
	Button = new TTextButton(0, 0, "Console", 18, 19, TCallback(this, 402), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextCustomiseConsole);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1274.Data18.x, SkinProperties().Style1274.Data18.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1274.Data48.x, SkinProperties().Style1274.Data48.y);
	Button = new TTextButton(0, 0, "Network", 18, 19, TCallback(this, 400), nullptr);
	Label = new TLabel(0, 0, SkinProperties().StyleB08.Data78, TextChangeNetworkSettings);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1274.Data20.x, SkinProperties().Style1274.Data20.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1274.Data50.x, SkinProperties().Style1274.Data50.y);
	Button = new TTextButton(0, 0, "Hotkeys", 18, 19, TCallback(this, 403), nullptr);
	Label = new TLabel(SkinProperties().StyleB08.Data78, TextEditHotkeys);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1274.Data28.x, SkinProperties().Style1274.Data28.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1274.Data58.x, SkinProperties().Style1274.Data58.y);
	Button = new TTextButton(0, 0, "Motd", 18, 19, TCallback(this, 406), nullptr);
	Label = new TLabel(0, 0, SkinProperties().StyleB08.Data78, TextShowMotD);
	InsertChild(Button);
	SetChildPosition(Button, SkinProperties().Style1274.Data30.x, SkinProperties().Style1274.Data30.y);
	InsertChild(Label);
	SetChildPosition(Label, SkinProperties().Style1274.Data60.x, SkinProperties().Style1274.Data60.y);
	TSeparator *Separator = new TSeparator(true);
	InsertChild(Separator);
	SetChildArea(Separator, SkinProperties().Style1274.Data68);
	Arrange();
}

// 0x478e50, line 3380
void TOptionsDialog::KeyPress(int Key)
{
	if (Key == 13)
		HandleEvent(1);
	else if (Key == 27)
		HandleEvent(2);
}

// 0x479010, lines 3422-3426. Closing saves the options; each button opens
// its dialog and this one again after it (0x21).
void TOptionsDialog::HandleEvent(int Event)
{
	switch (Event) {
	case 1:
	case 2:
		SavePreferences();
		break;
	case 400:
		AddMessage(0x22, 0, 0);
		AddMessage(0x21, 0, 0);
		break;
	case 401:
		AddMessage(0x23, 0, 0);
		AddMessage(0x21, 0, 0);
		break;
	case 402:
		AddMessage(0x25, 0, 0);
		AddMessage(0x21, 0, 0);
		break;
	case 403:
		AddMessage(0x26, 0, 0);
		AddMessage(0x21, 0, 0);
		break;
	case 404:
		AddMessage(0x27, 0, 0);
		AddMessage(0x21, 0, 0);
		break;
	case 406:
		AddMessage(0x28, 0, 0);
		AddMessage(0x21, 0, 0);
		break;
	default:
		TERROR1("unknown event", Event);
	}
	SetActionState(0);
}

// --- TGeneralOptionsDialog ------------------------------------------------------

// 0x479310, line 3472
TGeneralOptionsDialog::TGeneralOptionsDialog(void (*_OpenURL)(const char *URL))
{
	OpenURL = _OpenURL;
	TComponent::SetSize(SkinProperties().Data12EC, SkinProperties().Data12F0);
	ClassicControl = new TCheckBox("Tibia Classic Control");
	AutoChaseOff = new TCheckBox("Auto Chase Off");
	ShowHints = new TCheckBox("Show Hints");
	CreatureNames = new TCheckBox("Show Names of Creatures");
	TextualEffects = new TCheckBox("Show Textual Effects");
	InsertChild(ClassicControl);
	InsertChild(AutoChaseOff);
	InsertChild(ShowHints);
	InsertChild(CreatureNames);
	InsertChild(TextualEffects);
	SetChildArea(ClassicControl, SkinProperties().Data12F4);
	SetChildArea(AutoChaseOff, SkinProperties().Data1304);
	SetChildArea(ShowHints, SkinProperties().Data1314);
	SetChildArea(CreatureNames, SkinProperties().Data1334);
	SetChildArea(TextualEffects, SkinProperties().Data1324);
	ClassicControl->SetChecked(GetClassicControl());
	AutoChaseOff->SetChecked(GetAutoChaseOff());
	ShowHints->SetChecked(GetShowNewHints());
	CreatureNames->SetChecked(GetCreatureInfo() != 0);
	TextualEffects->SetChecked(GetTextualEffects());
	Arrange();
}

// 0x479720, lines 3495-3498. The creature names are creature info 2 (names
// and bars) or 0. Help (701) keeps the dialog open.
void TGeneralOptionsDialog::HandleEvent(int Event)
{
	switch (Event) {
	case 1:
		SetClassicControl(ClassicControl->IsChecked());
		SetAutoChaseOff(AutoChaseOff->IsChecked());
		SetShowNewHints(ShowHints->IsChecked());
		SetCreatureInfo(CreatureNames->IsChecked() ? 2 : 0);
		SetTextualEffects(TextualEffects->IsChecked());
		SetActionState(0);
		break;
	case 2:
		SetActionState(0);
		break;
	case 701:
		OpenURL(TextOptionsURL);
		break;
	default:
		TERROR1("unknown event", Event);
	}
}

// --- TGraphicsOptionsDialog ------------------------------------------------------

// 0x479a30, line 3549. The selection is the configured fullscreen mode (by
// size and depth), else the first.
TGraphicsOptionsDialog::TGraphicsOptionsDialog(void (*_OpenURL)(const char *URL), void (*_SetDisplayMode)(int Mode),
	TWindow *_Parent)
{
	OpenURL = _OpenURL;
	SetDisplayMode = _SetDisplayMode;
	Parent = _Parent;
	Fullscreen = new TCheckBox("Fullscreen");
	ResolutionList = new TListBox(NoCallback, NoCallback);
	AdvancedButton = new TTextButton(0, 0, "Advanced", 18, 19, TCallback(this, 407), nullptr);
	TLabel *ResolutionLabel = new TLabel(0, 0, SkinProperties().StyleB08.Data78, TextAvailableResolutions);
	TLabel *AdvancedLabel = new TLabel(0, 0, SkinProperties().StyleB08.Data78, TextAdvancedSettings);
	InsertChild(Fullscreen);
	InsertChild(ResolutionList);
	InsertChild(ResolutionLabel);
	InsertChild(AdvancedLabel);
	InsertChild(AdvancedButton);
	TComponent::SetSize(SkinProperties().Data1344, SkinProperties().Data1348);
	SetChildArea(Fullscreen, SkinProperties().Data134C);
	SetChildArea(ResolutionList, SkinProperties().Data135C);
	SetChildArea(ResolutionLabel, SkinProperties().Data136C);
	SetChildArea(AdvancedLabel, SkinProperties().Data1384);
	SetChildPosition(AdvancedButton, SkinProperties().Data137C.x, SkinProperties().Data137C.y);
	Fullscreen->SetChecked(GetViewMode() == 2);
	// GetFullscreenModes stores its new[] list (or NULL) through the pointer it is given; Modes takes it over (issue
	// #58). Every engine writes the pointer before anything else.
	TFullscreenMode *List = nullptr;
	NumberOfModes = GetSurface()->GetFullscreenModes(&List);
	Modes.reset(List);
	const TFullscreenMode *Current = GetFullscreenMode();
	for (int i = 0; i < NumberOfModes; i++) {
		TFixedString<16> Frequency;
		Frequency.Format(", {} Hz", Modes[i].Frequency);
		TFixedString<244> Line;
		Line.Format("{}*{}, {} Bits{}", Modes[i].Width, Modes[i].Height, Modes[i].Depth,
			Modes[i].Frequency != 0 ? Frequency.Chars : "");
		ResolutionList->AddLine(Line);
		if (i == 0
			|| (Current->Width == Modes[i].Width && Current->Height == Modes[i].Height
				&& Current->Depth == Modes[i].Depth))
			ResolutionList->SetSelection(i);
	}
	Arrange();
}

// 0x479f50, line 3573. Only the arrow keys up and down reach the list.
void TGraphicsOptionsDialog::KeyPress(int Key)
{
	switch (Key) {
	case 13:
		HandleEvent(1);
		break;
	case 27:
		HandleEvent(2);
		break;
	case 0x10c:
		ResolutionList->KeyPress(0x10c);
		break;
	case 0x10f:
		ResolutionList->KeyPress(0x10f);
		break;
	default:
		break;
	}
}

// 0x47a150, line 3598. OK stores the selected mode and switches the
// display mode when the fullscreen setting changed (to fullscreen only with
// a selected mode); the OpenGL engine (2) is resized for fullscreen.
// Advanced (407) closes and opens the advanced options and this dialog
// again; Help (701) keeps it open.
void TGraphicsOptionsDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const int ViewMode = GetViewMode();
		const bool FullscreenChecked = Fullscreen->IsChecked();
		if (ResolutionList->GetSelection() != -1)
			SetFullscreenMode(&Modes[ResolutionList->GetSelection()]);
		if ((ViewMode == 2) != FullscreenChecked
			&& (!Fullscreen->IsChecked() || ResolutionList->GetSelection() != -1))
			SetDisplayMode(Fullscreen->IsChecked() + 1);
		if (patella::bridge::GraphicsEngine().curEngine == 2 && Fullscreen->IsChecked())
			patella::bridge::GUI().Resize(0, 0); // Control.cpp's (issues #107, #118)
	} else if (Event == 407) {
		AddMessageFirst(0x23, 0, 0);
		AddMessageFirst(0x24, reinterpret_cast<std::intptr_t>(Parent), 0);
	} else if (Event == 701) {
		OpenURL(TextGraphicsOptionsURL);
		return;
	}
	SetActionState(0);
}

// --- TAdvancedGraphicsDialog ------------------------------------------------------

// 0x47a3d0, line 3689. Fullscreen allows no engine switch; otherwise the
// engines the surface reports are enabled and the current one selected
// (curEngine 0 DirectX 5, 1 OpenGL, 2 DirectX 9).
TAdvancedGraphicsDialog::TAdvancedGraphicsDialog(void (*_OpenURL)(const char *URL), TComponent *_Receiver)
{
	OpenURL = _OpenURL;
	Receiver = _Receiver;
	EngineGroup.reset(new TRadioButtonGroup);
	LightEffects = new TCheckBox("Show Light Effects");
	NoStretching = new TCheckBox("Don't stretch/shrink the Map");
	WindowsMousepointer = new TCheckBox("Windows Mouse Pointer");
	Framerate = new THorizontalScrollBar(10, 201, 10, 11, 1, TCallback(this, 30));
	// The x64 client (issue #31): the graphics device choice in the engine buttons' places, with the
	// original's events (MainWindow.cpp: 800, 801, 802; Control.cpp ChangeEngine). The buttons are 58
	// pixels wide (skin 16), which "CPU (compatibility)" does not fit: its help text says it. The line
	// above them is as wide as the dialog; a longer device name is shortened, the whole one is its help.
	DeviceLabelName[0] = 0;
	DeviceLabel = new TLabel(SkinProperties().Style1394.Data08.x, SkinProperties().Style1394.Data08.y,
		SkinProperties().Style1394.Data00 - SkinProperties().Style1394.Data08.x,
		GetFontLineHeight(SkinProperties().StyleB08.Data78.FontNumber), SkinProperties().StyleB08.Data78,
		TextSelectDevice, true);
	TLabel *Label = DeviceLabel;
	DirectX5Button = new TTextButton(SkinProperties().Style1394.Data10.x, SkinProperties().Style1394.Data10.y,
		"Automatic", 16, 17, TCallback(_Receiver, 800), TextUseAutomaticDevice);
	DirectX5Button->SetRadioButtonGroup(EngineGroup.get());
	DirectX9Button = new TTextButton(SkinProperties().Style1394.Data18.x, SkinProperties().Style1394.Data18.y,
		"GPU", 16, 17, TCallback(_Receiver, 801), TextUseGpuDevice);
	DirectX9Button->SetRadioButtonGroup(EngineGroup.get());
	OpenGLButton = new TTextButton(SkinProperties().Style1394.Data20.x, SkinProperties().Style1394.Data20.y,
		"CPU", 16, 17, TCallback(_Receiver, 802), TextUseCpuDevice);
	OpenGLButton->SetRadioButtonGroup(EngineGroup.get());
	InsertChild(Label);
	InsertChild(DirectX5Button);
	InsertChild(DirectX9Button);
	InsertChild(OpenGLButton);
	InsertChild(LightEffects);
	InsertChild(NoStretching);
	InsertChild(WindowsMousepointer);
	InsertChild(Framerate);
	TComponent::SetSize(SkinProperties().Style1394.Data00, SkinProperties().Style1394.Data04);
	SetChildArea(LightEffects, SkinProperties().Style1394.Data28);
	SetChildArea(NoStretching, SkinProperties().Style1394.Data38);
	SetChildArea(WindowsMousepointer, SkinProperties().Style1394.Data48);
	SetChildArea(Framerate, SkinProperties().Style1394.Data58);
	LightEffects->SetChecked(GetLightEffects());
	if (!GetSurface()->CanShadeRectangles())
		LightEffects->SetEnabled(false);
	NoStretching->SetChecked(GetNoStretching());
	WindowsMousepointer->SetChecked(GetWindowsMousepointer());
	int FramerateLimit = GetConfineFramerate();
	if (FramerateLimit < 10)
		FramerateLimit = 201;
	Framerate->Range->SetValue(FramerateLimit);
	if (GetViewMode() == 2) {
		DirectX5Button->SetEnabled(false);
		DirectX9Button->SetEnabled(false);
		OpenGLButton->SetEnabled(false);
	} else {
		// GPU without a GPU, CPU without the CPU driver next to the executable (vulkan/DeviceSettings.h).
		if (GetNumberOfGpus() == 0)
			DirectX9Button->SetEnabled(false);
		if (GetCpuDriverManifest().empty())
			OpenGLButton->SetEnabled(false);
		switch (patella::bridge::GraphicsEngine().curEngine) {
		case 0:
			EngineGroup->Selected = DirectX5Button;
			break;
		case 1:
			EngineGroup->Selected = OpenGLButton;
			break;
		case 2:
			EngineGroup->Selected = DirectX9Button;
			break;
		default:
			break;
		}
	}
	ShowDevice();
	Arrange();
}

// The x64 client (issue #31): the device choice's button selected, after the original's selection of
// the current engine's (curEngine is always 2 there), and the line above the buttons naming the device
// in use, when that changed.
void TAdvancedGraphicsDialog::ShowDevice()
{
	switch (patella::vulkan::ButtonOf(GetDeviceChoice())) {
	case patella::vulkan::TDeviceButton::Automatic:
		EngineGroup->Selected = DirectX5Button;
		break;
	case patella::vulkan::TDeviceButton::Gpu:
		EngineGroup->Selected = DirectX9Button;
		break;
	case patella::vulkan::TDeviceButton::Cpu:
		EngineGroup->Selected = OpenGLButton;
		break;
	}
	const char *Name = GetDeviceName();
	if (strncmp(Name, DeviceLabelName, sizeof(DeviceLabelName) - 1) == 0)
		return;
	snprintf(DeviceLabelName, sizeof(DeviceLabelName), "%s", Name);
	TFixedString<sizeof(DeviceLabelName) + 16> Text;
	if (DeviceLabelName[0] != 0)
		snprintf(Text, sizeof(Text), "Device: %s", DeviceLabelName.Chars);
	else
		snprintf(Text, sizeof(Text), "%s", TextSelectDevice);
	DeviceLabel->SetText(Text, true);
}

// 0x47aa30, line 3723. Draws the frame rate texts over the children and
// shows the current engine as selected.
void TAdvancedGraphicsDialog::Draw(int Surface, int x, int y)
{
	TWindow::Draw(Surface, x, y);
	TFixedString<612> Text;
	if (Framerate->Range->Value < 201)
		Text.Format("Adjust framerate limit: {}", Framerate->Range->Value);
	else
		Text.Assign("Adjust framerate limit: max");
	PrintFormattedText(Surface, SkinProperties().Style1394.Data68.x + x, SkinProperties().Style1394.Data68.y + y,
		SkinProperties().StyleB08.Data78, Text);
	const double FPS = GetSurface()->GetFPS();
	Text.Format("Current framerate: {:2.2f}", FPS);
	PrintFormattedText(Surface, SkinProperties().Style1394.Data70.x + x, SkinProperties().Style1394.Data70.y + y,
		SkinProperties().StyleB08.Data78, Text);
	switch (patella::bridge::GraphicsEngine().curEngine) {
	case 0:
		EngineGroup->Selected = DirectX5Button;
		break;
	case 1:
		EngineGroup->Selected = OpenGLButton;
		break;
	case 2:
		EngineGroup->Selected = DirectX9Button;
		break;
	default:
		break;
	}
	ShowDevice();
}

// 0x47ad10, line 3751. The frame rate bar (30) sets the limit while it
// moves; Cancel (2) restores the configured one. Turning on "don't stretch"
// makes the receiver arrange the game window again (event 9).
void TAdvancedGraphicsDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		const bool Rearrange = !GetNoStretching() && NoStretching->IsChecked();
		SetNoStretching(NoStretching->IsChecked());
		SetWindowsMousepointer(WindowsMousepointer->IsChecked());
		SetLightEffects(LightEffects->IsChecked());
		SetConfineFramerate(Framerate->Range->Value);
		GetSurface()->SetFramerateLimit(GetConfineFramerate());
		if (Rearrange)
			Receiver->HandleEvent(9);
	} else if (Event == 701) {
		OpenURL(TextAdvancedGraphicsURL);
		return;
	} else if (Event == 30) {
		GetSurface()->SetFramerateLimit(Framerate->Range->Value);
		return;
	} else if (Event == 2) {
		GetSurface()->SetFramerateLimit(GetConfineFramerate());
	}
	SetActionState(0);
}

// --- TConsoleOptionsDialog --------------------------------------------------------

// 0x47afb0, line 3797
TConsoleOptionsDialog::TConsoleOptionsDialog(void (*_OpenURL)(const char *URL))
{
	OpenURL = _OpenURL;
	TComponent::SetSize(SkinProperties().Data140C, SkinProperties().Data1410);
	InfoMessages = new TCheckBox("Show Info Messages in Console");
	EventMessages = new TCheckBox("Show Event Messages in Console");
	StatusMessages = new TCheckBox("Show Status Messages in Console");
	TimeStamps = new TCheckBox("Show Timestamp in Console");
	PrivateMessages = new TCheckBox("Show Private Messages in Game Window");
	InsertChild(InfoMessages);
	InsertChild(EventMessages);
	InsertChild(StatusMessages);
	InsertChild(TimeStamps);
	InsertChild(PrivateMessages);
	SetChildArea(InfoMessages, SkinProperties().Data1414);
	SetChildArea(EventMessages, SkinProperties().Data1424);
	SetChildArea(StatusMessages, SkinProperties().Data1434);
	SetChildArea(TimeStamps, SkinProperties().Data1444);
	SetChildArea(PrivateMessages, SkinProperties().Data1454);
	InfoMessages->SetChecked(GetInfoMessages());
	EventMessages->SetChecked(GetEventMessages());
	StatusMessages->SetChecked(GetStatusMessages());
	TimeStamps->SetChecked(GetTimeStamps());
	PrivateMessages->SetChecked(GetPrivateMessages());
	Arrange();
}

// 0x47b3c0, line 3819
void TConsoleOptionsDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		SetInfoMessages(InfoMessages->IsChecked());
		SetEventMessages(EventMessages->IsChecked());
		SetStatusMessages(StatusMessages->IsChecked());
		SetTimeStamps(TimeStamps->IsChecked());
		SetPrivateMessages(PrivateMessages->IsChecked());
		SetConsoleChanged();
	} else if (Event == 701) {
		OpenURL(TextConsoleOptionsURL);
		return;
	}
	SetActionState(0);
}

// --- TNetworkOptionsDialog ---------------------------------------------------------

// 0x47b5e0, line 3843
TNetworkOptionsDialog::TNetworkOptionsDialog(void (*_OpenURL)(const char *URL))
{
	OpenURL = _OpenURL;
	TComponent::SetSize(SkinProperties().Data1464, SkinProperties().Data1468);
	TestServerLogin = new TCheckBox("Test Server Login");
	InsertChild(TestServerLogin);
	SetChildArea(TestServerLogin, SkinProperties().Data146C);
	TestServerLogin->SetChecked(GetTestServerLogin());
	Arrange();
}

// 0x47b800, line 3860
void TNetworkOptionsDialog::HandleEvent(int Event)
{
	if (Event == 1) {
		SetTestServerLogin(TestServerLogin->IsChecked());
	} else if (Event == 701) {
		OpenURL(TextNetworkOptionsURL);
		return;
	}
	SetActionState(0);
}

// --- THotkeyDialog -------------------------------------------------------------------

// 0x47edf0, line 3906. Every hotkey gets a line (271 characters at most)
// that UpdateLine fills.
THotkeyDialog::THotkeyDialog(void (*_OpenURL)(const char *URL))
{
	OpenURL = _OpenURL;
	TComponent::SetSize(SkinProperties().Data147C, SkinProperties().Data1480);
	TLabel *Label = new TLabel(0, 0, SkinProperties().StyleB08.Data78, TextAvailableHotkeys);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Data1494);
	Label = new TLabel(0, 0, SkinProperties().StyleB08.Data78, TextEditHotkeyText);
	InsertChild(Label);
	SetChildArea(Label, SkinProperties().Data14B4);
	ListBox = new TFontListBox(TCallback(this, 11), NoCallback);
	InsertChild(ListBox);
	SetChildArea(ListBox, SkinProperties().Data1484);
	for (int i = 0; i < HOTKEYS; i++) {
		Texts[i].Assign(GetHotkey(i, &AutoSend[i]));
		ListBox->AddLine(271);
		UpdateLine(i);
	}
	TextEdit = new TEditLine(255, false);
	InsertChild(TextEdit);
	SetChildArea(TextEdit, SkinProperties().Data14A4);
	AutoSendCheckBox = new TCheckBox("Send automatically");
	InsertChild(AutoSendCheckBox);
	SetChildArea(AutoSendCheckBox, SkinProperties().Data14C4);
	Selected = -1;
	Arrange();
}

// 0x47b9c0, lines 3914-3933. Hotkeys sent automatically are drawn in their
// own colour.
void THotkeyDialog::UpdateLine(int Number)
{
	PATELLA_ASSERT_VALUES(In(Number,0,HOTKEYS-1), Number);
	TFixedString<732> Line;
	if (Number < 12)
		Line.Format("F{}{}: {}", Number + 1, "", Texts[Number]);
	if (Number >= 12 && Number <= 23)
		Line.Format("Shift+F{}{}: {}", Number - 11, "", Texts[Number]);
	if (Number >= 24)
		Line.Format("Control+F{}{}: {}", Number - 23, "", Texts[Number]);
	TFont Font = SkinProperties().Style9A0.Data0C;
	if (AutoSend[Number])
		Font.Color = SkinProperties().Data14D4;
	ListBox->SetLine(Number, Line, Font);
}

// 0x47bd40, line 3958. Every other key edits the selected hotkey's text; an
// empty text cannot be sent automatically.
void THotkeyDialog::KeyPress(int Key)
{
	switch (Key) {
	case 13:
		HandleEvent(1);
		break;
	case 27:
		HandleEvent(2);
		break;
	case 0x10c:
		ListBox->KeyPress(0x10c);
		break;
	case 0x10f:
		ListBox->KeyPress(0x10f);
		break;
	default:
		if (Selected != -1) {
			TextEdit->KeyPress(Key);
			Texts[Selected].Assign(TextEdit->Text.get());
			AutoSendCheckBox->SetEnabled(strlen(Texts[Selected]) != 0);
			if (strlen(Texts[Selected]) == 0) {
				AutoSend[Selected] = false;
				AutoSendCheckBox->SetChecked(false);
			}
			UpdateLine(Selected);
		}
		break;
	}
}

// 0x47c010, lines 3993-3996. OK stores the hotkeys and saves the options
// unless the options dialog comes next (it saves them itself). A selection
// (11) stores the check box of the hotkey left and edits the new one; no
// selection (-1) reads before the texts (kept).
void THotkeyDialog::HandleEvent(int Event)
{
	switch (Event) {
	case 1:
		if (Selected != -1)
			AutoSend[Selected] = AutoSendCheckBox->IsChecked();
		for (int i = 0; i < HOTKEYS; i++)
			SetHotkey(i, Texts[i], AutoSend[i]);
		SetActionState(0);
		if (PeekMessageType() != 0x21)
			SavePreferences();
		break;
	case 2:
		SetActionState(0);
		break;
	case 11:
		if (Selected != -1)
			AutoSend[Selected] = AutoSendCheckBox->IsChecked();
		Selected = ListBox->GetSelection();
		TextEdit->SetText(Texts[Selected]);
		TextEdit->SetActive(true);
		AutoSendCheckBox->SetChecked(AutoSend[Selected]);
		AutoSendCheckBox->SetEnabled(strlen(Texts[Selected]) != 0);
		if (strlen(Texts[Selected]) == 0)
			AutoSendCheckBox->SetChecked(false);
		break;
	default:
		TERROR1("unknown event", Event);
	}
}

// 0x47c400, line 4010. A click may have changed the check box.
bool THotkeyDialog::LeftButtonUp(int x, int y, int Flags)
{
	const bool Result = TWindow::LeftButtonUp(x, y, Flags);
	if (Selected != -1) {
		AutoSend[Selected] = AutoSendCheckBox->IsChecked();
		UpdateLine(Selected);
	}
	return Result;
}
// NOLINTEND(cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg)
