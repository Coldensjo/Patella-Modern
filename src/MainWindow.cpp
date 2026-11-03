// MainWindow.cpp: reconstructed from Tibia.exe 7.72 (0x4ac010-0x4b07c6).
//
// The main game window (TMainWindow), the start window shown before the
// game (TStartWindow) and the splitter above the console (TSplitter); see
// MainWindow.h. The functions follow in line order; those without line
// evidence go where the binary puts them (the destructor and ExitMainWindow
// open the object, GetConsoleWindowHeight and TSplitter::Arrange sit between
// their neighbours by line). The binary keeps a second run of functions after
// the splitter (KeyPress, HandleEvent, ChangeMode, OpenTrade, CloseTrade,
// the constructor and InitMainWindow); their lines put them in between.
//
// In the game the window is laid out in two columns: the map frame, the
// splitter and the console on the left, the side bar (SidePanel in a frame)
// and the dialog window on the right, SkinProperties.Data184 wide. The
// heights of the map and the console follow the console height of the
// preferences and the minimum sizes; the splitter changes the console's
// height. Leaving a mode (ChangeMode, the destructor) stores the open
// sub-windows of both columns in the preferences; entering the game opens
// them again.

#include "MainWindow.h"
#include "ContentWindows.h"
#include "Communication.h"
#include "ConsoleWindow.h"
#include "Control.h"
#include "Creatures.h"
#include "DialogElements.h"
#include "Inventory.h"
#include "MapWindow.h"
#include "Network.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Text.h"
#include "graphics.h"
#include "DialogWindow.h"
#include "GUI.h"
#include "common.h"

#include <cstdlib>

// Data, in .bss order.
static void (*OpenURLCallback)(const char *URL); // 0x5d16a0
static void (*SetDisplayModeCallback)(int Mode); // 0x5d16a4
static void (*ExitProgramCallback)();            // 0x5d16a8
TWindow *ModalWindow;                            // 0x5d16ac
TMainWindow *MainWindow;                         // 0x5d16b0
// At 0x5d16b4, constructed by the static initialiser 0x559f30 as
// TCallback(NULL, 0) and never used (MapWindow.cpp has one too).
static TCallback NoCallback(nullptr, 0);

// --- TMainWindow -----------------------------------------------------------

// 0x4b03c0, line 85. The size is set before the children exist (TWindow's
// SetSize, not the own one); ChangeMode creates them. Ctrl+F and Alt+Enter
// switch between windowed and fullscreen mode.
TMainWindow::TMainWindow(int Width, int Height)
{
	TBEGIN
	TWindow::SetSize(Width, Height);
	ChangeMode(false);
	AddHotkey(0x446, TCallback(this, 0x1f8));
	AddHotkey(0x80d, TCallback(this, 0x1f8));
	TEND2(85, Width, Height)
}

// 0x4ac010. No exception frame of its own; ChangeMode starts with the same
// code.
TMainWindow::~TMainWindow()
{
	if (FirstChild != nullptr && InGame) {
		ClearOpenDialogs();
		if (SidePanel != nullptr)
			SidePanel->StoreOpenDialogs();
		if (DialogWindow != nullptr)
			DialogWindow->StoreOpenDialogs();
	}
}

// 0x4ac090. Called by Control.cpp's ExitControl.
void ExitMainWindow()
{
	delete MainWindow;
	MainWindow = nullptr;
	if (ModalWindow != nullptr) {
		delete ModalWindow;
		SetActionState(0);
	}
	ModalWindow = nullptr;
}

// 0x4ac0f0, line 127. Out of the game an error while drawing the start
// window becomes PROGRAM_ERROR 12 (the code is shared with the surface
// errors of Control.cpp, Player.cpp and Surface.cpp). In the game the
// children are drawn in a fixed order, the map frame after the right column.
void TMainWindow::Draw(int Surface, int x, int y)
{
	TBEGIN
	if (!InGame) {
		try {
			TWindow::Draw(Surface, x, y);
		} catch (...) {
			throw (PROGRAM_ERROR)12;
		}
		return;
	}
	Console->Draw(Surface, Console->OffsetX + x, Console->OffsetY + y);
	SideBar->Draw(Surface, SideBar->OffsetX + x, SideBar->OffsetY + y);
	DialogWindow->Draw(Surface, DialogWindow->OffsetX + x, DialogWindow->OffsetY + y);
	MapFrame->Draw(Surface, MapFrame->OffsetX + x, MapFrame->OffsetY + y);
	Splitter->Draw(Surface, Splitter->OffsetX + x, Splitter->OffsetY + y);
	TEND1(127, Surface)
}

// 0x4ac340, line 177. The map gets the height the console leaves, at least
// its minimum height and at most what the console's minimum height leaves.
void TMainWindow::SetSize(int Width, int Height)
{
	TBEGIN
	TWindow::SetSize(Width, Height);
	SetWindowSize(Width, Height);
	if (InGame) {
		int SplitterHeight = Splitter->Height;
		int MaxMapHeight = MapFrame->getMinimumHeight();
		int Rest = Height - Console->getMinimumHeight() - SplitterHeight;
		if (Rest > MaxMapHeight)
			MaxMapHeight = Rest;
		int MinMapHeight = MapFrame->getMinimumHeight();
		int MapHeight = Height - Console->Height - Splitter->Height;
		if (MapHeight < MinMapHeight)
			MapHeight = MinMapHeight;
		if (MapHeight > MaxMapHeight)
			MapHeight = MaxMapHeight;
		int MapWidth = Width - SkinProperties.Data184;
		MapFrame->SetPosition(0, 0);
		MapFrame->SetSize(MapWidth, MapHeight);
		MapHeight = MapFrame->Height;
		Splitter->SetSize(MapWidth, SkinProperties.Data138);
		Splitter->SetPosition(0, MapHeight);
		Splitter->Arrange();
		Console->SetSize(MapWidth, Height - SkinProperties.Data138 - MapHeight);
		Console->SetPosition(0, Splitter->Height + MapHeight);
		Console->Arrange();
		SideBar->SetPosition(MapWidth, 0);
		DialogWindow->SetSize(SkinProperties.Data184, Height - SideBar->Height);
		DialogWindow->SetPosition(MapWidth, SideBar->Height);
	} else {
		StartWindow->SetSize(Width, Height);
		StartWindow->SetPosition(0, 0);
	}
	Arrange();
	TEND2(177, Width, Height)
}

// 0x4aea50, line 203. Only in connection state 8 (in the game): the arrow
// keys and the keypad walk (the diagonals with Diagonal set), Ctrl with an
// arrow key turns, every other key goes to the console.
void TMainWindow::KeyPress(int Key)
{
	TBEGIN
	if (GetConnectionState() != 8)
		return;
	switch (Key) {
	case 0x10c: Walk(0, -1, false); break;
	case 0x10d: Walk(-1, 0, false); break;
	case 0x10e: Walk(1, 0, false); break;
	case 0x10f: Walk(0, 1, false); break;
	case 0x116: Walk(-1, 1, true); break;
	case 0x117: Walk(1, 1, true); break;
	case 0x118: Walk(-1, -1, true); break;
	case 0x119: Walk(1, -1, true); break;
	case 0x50c: SendTurnNorth(); break;
	case 0x50d: SendTurnWest(); break;
	case 0x50e: SendTurnEast(); break;
	case 0x50f: SendTurnSouth(); break;
	default:
		Console->KeyPress(Key);
		break;
	}
	TEND1(203, Key)
}

// 0x4aed10, lines 308-339. The events of the hotkeys, the buttons of the
// side-bar windows and the menus. 0x17 and 0x18 swap the two inventory
// windows; 0xe, 0xf and 0x16 open or close the skill, battle and VIP
// windows; 0x1d gives up a connection; 0x1f8 switches between windowed and
// fullscreen mode (a fullscreen mode below 640x480 is raised to it first).
void TMainWindow::HandleEvent(int Event)
{
	TBEGIN
	switch (Event) {
	case 0x21:
		CopyTaintedClipboardText();
		[[fallthrough]];
	case 2:
		SetActionState(0);
		break;
	case 7:
		ExitProgramCallback();
		break;
	case 8:
		AddMessage(0x21, 0, 0);
		break;
	case 9:
		SetSize(Width, Height);
		break;
	case 0xe:
		if (DialogWindow->GetContentWindow(6) != nullptr)
			CloseContentWindow(6);
		else
			OpenContentWindow(6);
		break;
	case 0xf:
		if (DialogWindow->GetContentWindow(7) != nullptr)
			CloseContentWindow(7);
		else
			OpenContentWindow(7);
		break;
	case 0x16:
		if (DialogWindow->GetContentWindow(9) != nullptr)
			CloseContentWindow(9);
		else
			OpenContentWindow(9);
		break;
	case 0x17: {
		TContentWindow *OldWindow = SidePanel->GetContentWindow(1);
		TASSERT(308, OldWindow!=NULL);
		SidePanel->InsertChildAfter(OldWindow, CreateSmallInventoryWindow(this));
		SidePanel->DeleteChild(OldWindow);
		SidePanel->Rearrange(32, nullptr);
		SideBar->SetSize(SkinProperties.Data184, SidePanel->getMinimumHeight() + SideBar->getMinimumHeight());
		SetSize(Width, Height);
		break;
	}
	case 0x18: {
		TContentWindow *OldWindow = SidePanel->GetContentWindow(2);
		TASSERT(317, OldWindow!=NULL);
		SidePanel->InsertChildAfter(OldWindow, CreateInventoryWindow(this));
		SidePanel->DeleteChild(OldWindow);
		SidePanel->Rearrange(32, nullptr);
		SideBar->SetSize(SkinProperties.Data184, SidePanel->getMinimumHeight() + SideBar->getMinimumHeight());
		SetSize(Width, Height);
		break;
	}
	case 0x1d:
		Disconnect(false);
		if (GetConnectionState() == 5 || GetConnectionState() == 6) {
			AddMessage(9, 20, 0);
			AddMessage(10, 300, 0);
		}
		SetConnectionState(0);
		SetActionState(0);
		break;
	case 0x20:
		ClearPlayerSteps();
		SetAttackID(0);
		SendCancel();
		break;
	case 0x1f8:
		if (GetFullscreenMode()->Width != 0) {
			TFullscreenMode Mode = *GetFullscreenMode();
			if (Mode.Width < 640 || Mode.Height < 480) {
				Mode.Width = 640;
				Mode.Height = 480;
				SetFullscreenMode(&Mode);
				SetSize(640, 480);
			}
			SetDisplayModeCallback(GetViewMode() == 1 ? 2 : 1);
		}
		break;
	case 0x1f9:
		if (!CheckRight(40, -1)) {
			SetStatusMessage(TextNoBugReportRight, false);
		} else if (GetConnectionState() == 8 && ModalWindow == nullptr) {
			AddMessage(0x11, 0, 0);
		}
		break;
	case 0x1fa:
		SetActionState(0);
		SendLogout();
		break;
	case 0x1fb: {
		// A question with an answer event; message 6 deletes the block.
		INT_PTR *Data = new INT_PTR[4];
		Data[0] = (INT_PTR) "Warning";
		Data[1] = (INT_PTR) "Are you sure you want to leave Tibia?";
		Data[2] = (INT_PTR)this;
		Data[3] = 0x1fa;
		AddMessage(6, (INT_PTR)Data, 0);
		break;
	}
	case 0x1fd:
		AddMessage(10, 300, 0);
		break;
	case 0x2bd:
		AddMessage(0x1f, 0, 0);
		break;
	case 0x2be:
		SetActionState(0);
		AddMessage(0x2e, 0, 0);
		break;
	case 0x2c0:
		SendJoinChannel(7);
		break;
	case 0x2c1:
		AddMessage(0x16, 0, 0);
		break;
	case 0x2c4:
		if (CheckAnyRight())
			AddMessage(0x1c, 0, 0);
		break;
	case 800:
		ChangeEngine(0);
		break;
	case 801:
		ChangeEngine(2);
		break;
	case 802:
		ChangeEngine(1);
		break;
	}
	TEND1(339, Event)
}

// 0x4af5e0, line 463. Rebuilds the window for the game (InGame) or for the
// start window. The open sub-windows are stored in the preferences first and
// read back when the game starts: the side-bar windows 1-5 (missing ones are
// added: 5, the inventory, 4 and 3; InsertChild puts each on top) and the
// skill, battle and VIP windows (not the trade window). The member pointers
// are not initialised before the first call: only FirstChild guards them.
// SideBar is not cleared with the others: out of the game it points to the
// deleted frame (never used there).
void TMainWindow::ChangeMode(bool InGame)
{
	TBEGIN
	if (FirstChild != nullptr && this->InGame) {
		ClearOpenDialogs();
		if (SidePanel != nullptr)
			SidePanel->StoreOpenDialogs();
		if (DialogWindow != nullptr)
			DialogWindow->StoreOpenDialogs();
	}
	while (FirstChild != nullptr)
		DeleteChild(FirstChild);
	MapFrame = nullptr;
	Console = nullptr;
	DialogWindow = nullptr;
	SidePanel = nullptr;
	Splitter = nullptr;
	StartWindow = nullptr;
	this->InGame = InGame;
	if (InGame) {
		MapFrame = new TMapFrame;
		Splitter = new TSplitter;
		Console = new TConsoleWindow;
		SidePanel = new TDialogWindow;
		DialogWindow = new TDialogWindow;
		int *Dialogs;
		int Count = GetOpenDialogs(&Dialogs);
		bool HaveAutomap = false;
		bool HaveStatus = false;
		bool HaveInventory = false;
		bool HaveButtons = false;
		for (int i = Count - 1; i >= 0; i--) {
			switch (Dialogs[i]) {
			case 1:
				SidePanel->InsertChild(CreateInventoryWindow(this));
				HaveInventory = true;
				break;
			case 2:
				SidePanel->InsertChild(CreateSmallInventoryWindow(this));
				HaveInventory = true;
				break;
			case 3:
				SidePanel->InsertChild(new TAutomapWindow);
				HaveAutomap = true;
				break;
			case 4:
				SidePanel->InsertChild(new TStatusWindow);
				HaveStatus = true;
				break;
			case 5:
				SidePanel->InsertChild(new TButtonsWindow(DialogWindow, this));
				HaveButtons = true;
				break;
			}
		}
		if (!HaveButtons)
			SidePanel->InsertChild(new TButtonsWindow(DialogWindow, this));
		if (!HaveInventory)
			SidePanel->InsertChild(CreateInventoryWindow(this));
		if (!HaveStatus)
			SidePanel->InsertChild(new TStatusWindow);
		if (!HaveAutomap)
			SidePanel->InsertChild(new TAutomapWindow);
		SideBar = new TBorder(SidePanel, 0);
		InsertChild(MapFrame);
		InsertChild(Splitter);
		InsertChild(Console);
		InsertChild(SideBar);
		InsertChild(DialogWindow);
		// The console keeps its stored height unless the map would get
		// smaller than its minimum height; then it gets its own minimum.
		int ConsoleHeight = Console->getMinimumHeight();
		int StoredHeight = GetConsoleHeight();
		if (StoredHeight > ConsoleHeight)
			ConsoleHeight = StoredHeight;
		int MapWidth = Width - SkinProperties.Data184;
		int MapHeight = Height - ConsoleHeight - SkinProperties.Data138;
		if (MapHeight < MapFrame->getMinimumHeight()) {
			ConsoleHeight = Console->getMinimumHeight();
			MapWidth = Width - SkinProperties.Data184;
			MapHeight = Height - ConsoleHeight - SkinProperties.Data138;
		}
		MapFrame->SetPosition(0, 0);
		MapFrame->SetSize(MapWidth, MapHeight);
		Splitter->SetSize(MapWidth, SkinProperties.Data138);
		Splitter->SetPosition(0, MapHeight);
		Splitter->Arrange();
		Console->SetSize(MapWidth, ConsoleHeight);
		Console->SetPosition(0, MapHeight + SkinProperties.Data138);
		Console->OpenDefaultChannel(DefaultTextBuffer, "Default");
		Console->Arrange();
		SideBar->SetPosition(MapWidth, 0);
		SideBar->SetSize(SkinProperties.Data184, SidePanel->getMinimumHeight() + SideBar->getMinimumHeight());
		DialogWindow->SetSize(SkinProperties.Data184, Height - SideBar->Height);
		DialogWindow->SetPosition(MapWidth, SideBar->Height);
		for (int i = 0; i < Count; i++) {
			int Identifier = Dialogs[i];
			if (Identifier >= 6 && (Identifier <= 7 || Identifier == 9))
				OpenContentWindow(Identifier);
		}
		if (Count > 0) {
			free(Dialogs);
			Dialogs = nullptr;
		}
		// Ctrl+L and Ctrl+Q log out, Ctrl+G asks for another character,
		// Ctrl+S, Ctrl+B and Ctrl+P toggle the skill, battle and VIP windows,
		// Ctrl+Z reports a bug, Ctrl+R a rule violation, Ctrl+Y opens the
		// gamemaster dialog, Ctrl+H the hints, Ctrl+T the channel 7 and
		// Escape cancels the attack and the walk.
		AddHotkey(0x44c, TCallback(this, 0x1fa));
		AddHotkey(0x451, TCallback(this, 0x1fa));
		AddHotkey(0x447, TCallback(this, 0x1fd));
		AddHotkey(0x453, TCallback(this, 0xe));
		AddHotkey(0x442, TCallback(this, 0xf));
		AddHotkey(0x450, TCallback(this, 0x16));
		AddHotkey(0x45a, TCallback(this, 0x1f9));
		AddHotkey(0x452, TCallback(this, 0x2c1));
		AddHotkey(0x459, TCallback(this, 0x2c4));
		AddHotkey(0x448, TCallback(this, 0x2be));
		AddHotkey(0x454, TCallback(this, 0x2c0));
		AddHotkey(0x1b, TCallback(this, 0x20));
	} else {
		StartWindow = new TStartWindow;
		InsertChild(StartWindow);
		RemoveHotkey(0x44c);
		RemoveHotkey(0x451);
		RemoveHotkey(0x447);
		RemoveHotkey(0x453);
		RemoveHotkey(0x442);
		RemoveHotkey(0x450);
		RemoveHotkey(0x45a);
		RemoveHotkey(0x452);
		RemoveHotkey(0x459);
		RemoveHotkey(0x448);
		RemoveHotkey(0x454);
		RemoveHotkey(0x1b);
	}
	SetSize(Width, Height);
	SetConsoleActive(ModalWindow == nullptr);
	TEND(463)
}

// 0x4ac610, line 475. The console's new height, at least its minimum; the
// caller lays out the window again.
void TMainWindow::ResizeConsole(int ConsoleHeight)
{
	TBEGIN
	// One call of getMinimumHeight (0x4ac642; issue #34).
	int MinimumHeight = Console->getMinimumHeight();
	if (ConsoleHeight < MinimumHeight)
		ConsoleHeight = MinimumHeight;
	Console->SetSize(Console->Width, ConsoleHeight);
	TEND1(475, ConsoleHeight)
}

// 0x4ac7c0. The try block is gone (nothing in it can throw); the function
// shares its exception tables with TSplitter::Arrange.
int TMainWindow::GetConsoleWindowHeight()
{
	TBEGIN
	return Console->Height;
	TEND(484)
}

// 0x4ac7f0, line 503. SetPlayerDestination returns 1 when the step can be
// walked and 2 when the player has to stop.
void TMainWindow::Walk(int dx, int dy, bool Diagonal)
{
	TBEGIN
	int x, y, z;
	GetPlayerPosition(&x, &y, &z);
	switch (SetPlayerDestination(x + dx, y + dy, z, true, Diagonal)) {
	case 1:
		SendWalk();
		break;
	case 2:
		SendStop();
		break;
	}
	TEND2(503, dx, dy)
}

// 0x4ac9e0, lines 511-539. The assertion comes before the try block. An
// open container window is filled again: the body is updated (scrolled to
// the top, its counts, icon, title and "up" button; ContentWindows.cpp) and,
// while it is minimized, the window gets the name and the object of the
// container and the "up" state of a container that has a parent here.
void TMainWindow::OpenContainer(int Container)
{
	TASSERT(511, DialogWindow!=NULL);
	TBEGIN
	TSubWindow *Window = (TSubWindow *)DialogWindow->GetContentWindow(Container + 0x40);
	if (Window == nullptr) {
		DialogWindow->Rearrange(16, CreateContainerWindow(Container));
		return;
	}
	Window->Arrange();
	TContainerWindowBody *Body = (TContainerWindowBody *)Window->Body;
	Body->Update();
	if (Body->IsMinimized()) {
		Window->SetTitle(new TLabel(SkinProperties.Style194.Data38, GetContainerName(Container)));
		Window->SetIcon(new TObjectIcon(GetContainerObject(Container), &SkinProperties.Style194.Data80, false));
		if (GetContainerHasParent(Container))
			Window->ShowUpButton();
		else
			Window->HideUpButton();
	}
	TEND1(539, Container)
}

// 0x4acdb0, line 552
void TMainWindow::CloseContainer(int Container)
{
	TBEGIN
	if (DialogWindow != nullptr) {
		TContentWindow *Window = DialogWindow->GetContentWindow(Container + 0x40);
		if (Window != nullptr)
			DialogWindow->DeleteChild(Window);
	}
	TEND1(552, Container)
}

// 0x4afec0, lines 560-565. Both assertions come before the try block.
void TMainWindow::OpenTrade()
{
	TASSERT(560, DialogWindow!=NULL);
	TASSERT(561, !DialogWindow->hasContentWindow(TRADE_WINDOW));
	TBEGIN
	OpenContentWindow(TRADE_WINDOW);
	TEND(565)
}

// 0x4acf60, lines 576-579. The trade window's body gets event 0x15 (the
// counter offer arrived).
void TMainWindow::UpdateTrade()
{
	TBEGIN
	TSubWindow *Window = (TSubWindow *)DialogWindow->GetContentWindow(TRADE_WINDOW);
	if (Window != nullptr) {
		TASSERT(576, Window->getBody()!=NULL);
		Window->getBody()->HandleEvent(0x15);
	}
	TEND(579)
}

// 0x4b0230, line 590
void TMainWindow::CloseTrade()
{
	TBEGIN
	CloseContentWindow(TRADE_WINDOW);
	TEND(590)
}

// 0x4ad1e0, lines 606-610. The jump table 0x4ad48c has the four cases.
void TMainWindow::OpenContentWindow(int Identifier)
{
	TBEGIN
	if (DialogWindow->GetContentWindow(Identifier) == nullptr) {
		TWindow *Window;
		switch (Identifier) {
		case 6:
			Window = CreateSkillsWindow();
			break;
		case 7:
			Window = CreateBattleWindow();
			break;
		case TRADE_WINDOW:
			Window = CreateTradeWindow();
			break;
		case 9:
			Window = CreateBuddylistWindow();
			break;
		default:
			TERROR1(606, "unknown identifier", Identifier);
		}
		DialogWindow->Rearrange(16, Window);
	}
	TEND1(610, Identifier)
}

// 0x4ad4a0, line 621
void TMainWindow::CloseContentWindow(int Identifier)
{
	TBEGIN
	TContentWindow *Window = DialogWindow->GetContentWindow(Identifier);
	if (Window != nullptr)
		DialogWindow->DeleteChild(Window);
	TEND1(621, Identifier)
}

// 0x4ad650, line 632. The console takes the keyboard while no modal window
// is open (Control.cpp passes ModalWindow==NULL).
void TMainWindow::SetConsoleActive(bool Active)
{
	TBEGIN
	if (Console != nullptr)
		Console->SetActive(Active);
	TEND(632)
}

// --- TStartWindow ----------------------------------------------------------

// 0x4ad7f0, line 671. The buttons sit on a plain window that the frame
// (skins 0x34-0x3c) surrounds.
TStartWindow::TStartWindow()
{
	TBEGIN
	TWindow *Panel = new TWindow;
	Panel->SetSize(SkinProperties.Style0D8.Data00, SkinProperties.Style0D8.Data04);
	TComponent *Button = new TTextButton(0, 0, "Enter Game", 0x12, 0x13, TCallback(this, 0x1f5), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties.Style0D8.Data10.x, SkinProperties.Style0D8.Data10.y);
	Button = new TTextButton(0, 0, "Options", 0x12, 0x13, TCallback(this, 0x1f7), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties.Style0D8.Data18.x, SkinProperties.Style0D8.Data18.y);
	Button = new TTextButton(0, 0, "Info", 0x12, 0x13, TCallback(this, 0x1f6), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties.Style0D8.Data20.x, SkinProperties.Style0D8.Data20.y);
	Button = new TTextButton(0, 0, "Exit Game", 0x12, 0x13, TCallback(this, 7), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties.Style0D8.Data28.x, SkinProperties.Style0D8.Data28.y);
	Button = new TTextButton(0, 0, "Access Account", 0x12, 0x13, TCallback(this, 0x1fc), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties.Style0D8.Data30.x, SkinProperties.Style0D8.Data30.y);
	Frame = new TBorder(Panel, 0x34, 0x3a, 0x35, 0x38, 0x39, 0x36, 0x3b, 0x37, 0x3c, SkinProperties.Data134);
	InsertChild(Frame);
	Frame->FitToWindow();
	TEND(671)
}

// 0x4adc60, line 691. Picture 1, the start screen, goes to the 640x480
// surface 2 and is stretched over the window.
void TStartWindow::Draw(int Surface, int x, int y)
{
	TBEGIN
	DrawPicture(2, 0, 0, 1);
	GetSurface()->StretchBlitRect(Surface, TRect(x, y, Width, Height), 2, TRect(0, 0, 640, 480));
	TWindow::Draw(Surface, x, y);
	TEND1(691, Surface)
}

// 0x4ade70, line 703
void TStartWindow::SetSize(int Width, int Height)
{
	TBEGIN
	TWindow::SetSize(Width, Height);
	SetChildPosition(Frame, SkinProperties.Style0D8.Data08.x, SkinProperties.Style0D8.Data08.y);
	TEND2(703, Width, Height)
}

// 0x4ae030, line 731. The buttons work only while there is no connection.
void TStartWindow::HandleEvent(int Event)
{
	TBEGIN
	if (GetConnectionState() != 0)
		return;
	switch (Event) {
	case 0x1f5:
		AddMessage(8, 0, 0);
		break;
	case 0x1f6:
		AddMessage(0x1b, 0, 0);
		break;
	case 0x1f7:
		AddMessage(0x21, 0, 0);
		break;
	case 7:
		ExitProgramCallback();
		break;
	case 0x1fc:
		OpenURLCallback(TextAccountURL);
		break;
	}
	TEND1(731, Event)
}

// --- TSplitter -------------------------------------------------------------

// 0x4ae240, line 749
TSplitter::TSplitter()
{
	TBEGIN
	Dragging = false;
	DrawingListLength = 3;
	DrawingList = new TDrawing[3];
	for (int i = 0; i < DrawingListLength; i++)
		DrawingList[i].skin = 0;
	TEND(749)
}

// 0x4ae430. Skins 8 (top), 7 (middle, stretched) and 9 (bottom). As in
// GetConsoleWindowHeight the try block is gone.
void TSplitter::Arrange()
{
	TBEGIN
	DrawingList[0] = TDrawing(8, 0, 0, Width, Skins[8].height, 0, 0);
	DrawingList[1] = TDrawing(7, 0, Skins[8].height, Width, Height - Skins[9].height - Skins[8].height, 0, 0);
	DrawingList[2] = TDrawing(9, 0, Height - Skins[9].height, Width, Skins[9].height, 0, 0);
	TEND(762)
}

// 0x4ae530, line 780. Mouse cursor 2 is the vertical size arrow. While
// dragging, the console follows the mouse; the global MainWindow is used,
// not the parent. x has no name in the catch block.
bool TSplitter::MouseMove([[maybe_unused]] int x, int y)
{
	TBEGIN
	SetMouseCursor(2);
	if (Dragging) {
		MainWindow->ResizeConsole(DragY - y + MainWindow->GetConsoleWindowHeight());
		MainWindow->SetSize(MainWindow->Width, MainWindow->Height);
	}
	return true;
	TEND1(780, y)
}

// 0x4ae700, line 795
bool TSplitter::LeftButtonDown(int x, int y, [[maybe_unused]] int Flags)
{
	TBEGIN
	SetCapture(nullptr);
	DragX = x;
	Dragging = true;
	DragY = y;
	return true;
	TEND2(795, x, y)
}

// 0x4ae8b0, line 807. ReleaseCapture clears Dragging.
bool TSplitter::LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags)
{
	TBEGIN
	if (Dragging)
		ReleaseCapture(nullptr);
	return true;
	TEND(807)
}

// --- The main window -------------------------------------------------------

// 0x4b05b0, lines 822-828. Called by CMainFrame::OnCreate.
void InitMainWindow(int Width, int Height, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode),
	void (*OpenURL)(const char *URL))
{
	TBEGIN
	MainWindow = new TMainWindow(Width, Height);
	TASSERT1(822, GetActionState()==ACTIONSTATE_NONE, GetActionState());
	ExitProgramCallback = ExitProgram;
	ModalWindow = nullptr;
	SetDisplayModeCallback = SetDisplayMode;
	OpenURLCallback = OpenURL;
	TEND2(828, Width, Height)
}
