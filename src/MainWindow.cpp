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
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none
// apart from Draw's, which turns an error of the start window into
// PROGRAM_ERROR 12.

#include "MainWindow.h"
#include "ContentWindows.h"
#include "Communication.h"
#include "ConsoleWindow.h"
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

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory off for the file:
// the original's malloc'd entries and texts, freed by their lists; the original's raw owners (notes/ownership.md):
// the window tree owns its children, the message queue its blocks, the lists their entries
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

// The .bss before NoCallback (OpenURLCallback 0x5d16a0, SetDisplayModeCallback 0x5d16a4, ExitProgramCallback 0x5d16a8,
// ModalWindow 0x5d16ac, MainWindow 0x5d16b0) is TGUI's (GUI.h, issue #107).

namespace {

// At 0x5d16b4, constructed by the static initialiser 0x559f30 as
// TCallback(NULL, 0) and never used (MapWindow.cpp has one too).
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): the binary's global; TCallback's constructor (GUI.cpp) stores two values
TCallback NoCallback(nullptr, 0);

} // namespace

// --- TMainWindow -----------------------------------------------------------

// 0x4b03c0, line 85. The size is set before the children exist (TWindow's
// SetSize, not the own one); ChangeMode creates them. Ctrl+F and Alt+Enter
// switch between windowed and fullscreen mode.
TMainWindow::TMainWindow(int Width, int Height)
{
	TWindow::SetSize(Width, Height);
	ChangeMode(false);
	AddHotkey(0x446, TCallback(this, 0x1f8));
	AddHotkey(0x80d, TCallback(this, 0x1f8));
}

// 0x4ac010. Its work is the close since issue #108.
TMainWindow::~TMainWindow()
{
	CheckClosed("TMainWindow");
}

// The destructor's work until issue #108 (0x4ac010; no exception frame of its
// own; ChangeMode starts with the same code): the open sub-windows go into the
// preferences; then the children.
void TMainWindow::OnClose()
{
	try {
		if (FirstChild != nullptr && InGame) {
			ClearOpenDialogs();
			if (SidePanel != nullptr)
				SidePanel->StoreOpenDialogs();
			if (DialogWindow != nullptr)
				DialogWindow->StoreOpenDialogs();
		}
	} catch (...) {
		CloseWhileUnwinding([this] { TWindow::OnClose(); });
		throw;
	}
	TWindow::OnClose();
}

// TGUI's constructor and destructor (GUI.h, issue #108): here, where TMainWindow is complete for its owner.
patella::TGUI::TGUI(TNetwork &Network, TCommunication &Communication) : Network(Network), Communication(Communication)
{
}

patella::TGUI::~TGUI() = default;

void patella::TGUI::AbandonWindows() noexcept
{
	(void)MainWindow.release(); // NOLINT(bugprone-unused-return-value): left allocated on purpose (GUI.h)
}

// 0x4ac090. Called by Control.cpp's ExitControl. Each window is closed and then
// freed where the original deleted it (issue #108); a close that throws leaves
// its window allocated (the main window in its owner).
void patella::TGUI::ExitMainWindow()
{
	if (MainWindow != nullptr)
		MainWindow->Close();
	MainWindow.reset();
	if (ModalWindow != nullptr) {
		DeleteComponent(ModalWindow);
		SetActionState(0);
	}
	ModalWindow = nullptr;
}

// The free function of the original's name (issue #118): Control.cpp's ExitControl and the golden MainWindow test call
// it, the golden Control and Communication tests fake it (commworld.cpp).
void ExitMainWindow()
{
	patella::bridge::GUI().ExitMainWindow();
}

// 0x4ac0f0, line 127. Out of the game an error while drawing the start
// window becomes PROGRAM_ERROR 12 (the code is shared with the surface
// errors of Control.cpp, Player.cpp and Surface.cpp). In the game the
// children are drawn in a fixed order, the map frame after the right column.
void TMainWindow::Draw(int Surface, int x, int y)
{
	if (!InGame) {
		try {
			TWindow::Draw(Surface, x, y);
		} catch (...) {
			throw TProgramError{PROGRAM_ERROR::SURFACE};
		}
		return;
	}
	Console->Draw(Surface, Console->OffsetX + x, Console->OffsetY + y);
	SideBar->Draw(Surface, SideBar->OffsetX + x, SideBar->OffsetY + y);
	DialogWindow->Draw(Surface, DialogWindow->OffsetX + x, DialogWindow->OffsetY + y);
	MapFrame->Draw(Surface, MapFrame->OffsetX + x, MapFrame->OffsetY + y);
	Splitter->Draw(Surface, Splitter->OffsetX + x, Splitter->OffsetY + y);
}

// 0x4ac340, line 177. The map gets the height the console leaves, at least
// its minimum height and at most what the console's minimum height leaves.
void TMainWindow::SetSize(int Width, int Height)
{
	TWindow::SetSize(Width, Height);
	SetWindowSize(Width, Height);
	if (InGame) {
		const int SplitterHeight = Splitter->Height;
		int MaxMapHeight = MapFrame->getMinimumHeight();
		const int Rest = Height - Console->getMinimumHeight() - SplitterHeight;
		if (Rest > MaxMapHeight)
			MaxMapHeight = Rest;
		const int MinMapHeight = MapFrame->getMinimumHeight();
		int MapHeight = Height - Console->Height - Splitter->Height;
		if (MapHeight < MinMapHeight)
			MapHeight = MinMapHeight;
		if (MapHeight > MaxMapHeight)
			MapHeight = MaxMapHeight;
		const int MapWidth = Width - SkinProperties().Data184;
		MapFrame->SetPosition(0, 0);
		MapFrame->SetSize(MapWidth, MapHeight);
		MapHeight = MapFrame->Height;
		Splitter->SetSize(MapWidth, SkinProperties().Data138);
		Splitter->SetPosition(0, MapHeight);
		Splitter->Arrange();
		Console->SetSize(MapWidth, Height - SkinProperties().Data138 - MapHeight);
		Console->SetPosition(0, Splitter->Height + MapHeight);
		Console->Arrange();
		SideBar->SetPosition(MapWidth, 0);
		DialogWindow->SetSize(SkinProperties().Data184, Height - SideBar->Height);
		DialogWindow->SetPosition(MapWidth, SideBar->Height);
	} else {
		StartWindow->SetSize(Width, Height);
		StartWindow->SetPosition(0, 0);
	}
	Arrange();
}

// 0x4aea50, line 203. Only in connection state 8 (in the game): the arrow
// keys and the keypad walk (the diagonals with Diagonal set), Ctrl with an
// arrow key turns, every other key goes to the console.
void TMainWindow::KeyPress(int Key)
{
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
}

// 0x4aed10, lines 308-339. The events of the hotkeys, the buttons of the
// side-bar windows and the menus. 0x17 and 0x18 swap the two inventory
// windows; 0xe, 0xf and 0x16 open or close the skill, battle and VIP
// windows; 0x1d gives up a connection; 0x1f8 switches between windowed and
// fullscreen mode (a fullscreen mode below 640x480 is raised to it first).
// GCC's analyzer (issue #111): the message queue owns the block it gets as an integer (AddMessage; the message's handler deletes it).
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-malloc-leak\"")
void TMainWindow::HandleEvent(int Event)
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	switch (Event) {
	case 0x21:
		CopyTaintedClipboardText();
		[[fallthrough]];
	case 2:
		SetActionState(0);
		break;
	case 7:
		GUI.ExitProgramCallback();
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
		PATELLA_ASSERT(OldWindow!=nullptr);
		SidePanel->InsertChildAfter(OldWindow, CreateSmallInventoryWindow(this));
		SidePanel->DeleteChild(OldWindow);
		SidePanel->Rearrange(32, nullptr);
		SideBar->SetSize(SkinProperties().Data184, SidePanel->getMinimumHeight() + SideBar->getMinimumHeight());
		SetSize(Width, Height);
		break;
	}
	case 0x18: {
		TContentWindow *OldWindow = SidePanel->GetContentWindow(2);
		PATELLA_ASSERT(OldWindow!=nullptr);
		SidePanel->InsertChildAfter(OldWindow, CreateInventoryWindow(this));
		SidePanel->DeleteChild(OldWindow);
		SidePanel->Rearrange(32, nullptr);
		SideBar->SetSize(SkinProperties().Data184, SidePanel->getMinimumHeight() + SideBar->getMinimumHeight());
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
			GUI.SetDisplayModeCallback(GetViewMode() == 1 ? 2 : 1);
		}
		break;
	case 0x1f9:
		if (!CheckRight(40, -1)) {
			SetStatusMessage(TextNoBugReportRight, false);
		} else if (GetConnectionState() == 8 && GUI.ModalWindow == nullptr) {
			AddMessage(0x11, 0, 0);
		}
		break;
	case 0x1fa:
		SetActionState(0);
		SendLogout();
		break;
	case 0x1fb: {
		// A question with an answer event; message 6 deletes the block.
		std::intptr_t *Data = new std::intptr_t[4];
		Data[0] = reinterpret_cast<std::intptr_t>("Warning");
		Data[1] = reinterpret_cast<std::intptr_t>("Are you sure you want to leave Tibia?");
		Data[2] = reinterpret_cast<std::intptr_t>(this);
		Data[3] = 0x1fa;
		AddMessage(6, reinterpret_cast<std::intptr_t>(Data), 0);
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
		patella::TGUI::ChangeEngine(0); // Control.cpp's (issue #118)
		break;
	case 801:
		patella::TGUI::ChangeEngine(2); // Control.cpp's (issue #118)
		break;
	case 802:
		patella::TGUI::ChangeEngine(1); // Control.cpp's (issue #118)
		break;
	default:
		break;
	}
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")

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
		const int Count = GetOpenDialogs(&Dialogs);
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
			default:
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
		const int StoredHeight = GetConsoleHeight();
		if (StoredHeight > ConsoleHeight)
			ConsoleHeight = StoredHeight;
		int MapWidth = Width - SkinProperties().Data184;
		int MapHeight = Height - ConsoleHeight - SkinProperties().Data138;
		if (MapHeight < MapFrame->getMinimumHeight()) {
			ConsoleHeight = Console->getMinimumHeight();
			MapWidth = Width - SkinProperties().Data184;
			MapHeight = Height - ConsoleHeight - SkinProperties().Data138;
		}
		MapFrame->SetPosition(0, 0);
		MapFrame->SetSize(MapWidth, MapHeight);
		Splitter->SetSize(MapWidth, SkinProperties().Data138);
		Splitter->SetPosition(0, MapHeight);
		Splitter->Arrange();
		Console->SetSize(MapWidth, ConsoleHeight);
		Console->SetPosition(0, MapHeight + SkinProperties().Data138);
		Console->OpenDefaultChannel(patella::bridge::Talk().DefaultTextBuffer, "Default");
		Console->Arrange();
		SideBar->SetPosition(MapWidth, 0);
		SideBar->SetSize(SkinProperties().Data184, SidePanel->getMinimumHeight() + SideBar->getMinimumHeight());
		DialogWindow->SetSize(SkinProperties().Data184, Height - SideBar->Height);
		DialogWindow->SetPosition(MapWidth, SideBar->Height);
		for (int i = 0; i < Count; i++) {
			const int Identifier = Dialogs[i];
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
	SetConsoleActive(patella::bridge::GUI().ModalWindow == nullptr);
}

// 0x4ac610, line 475. The console's new height, at least its minimum; the
// caller lays out the window again.
void TMainWindow::ResizeConsole(int ConsoleHeight) const
{
	// One call of getMinimumHeight (0x4ac642; issue #34).
	const int MinimumHeight = Console->getMinimumHeight();
	if (ConsoleHeight < MinimumHeight)
		ConsoleHeight = MinimumHeight;
	Console->SetSize(Console->Width, ConsoleHeight);
}

// 0x4ac7c0. The try block is gone (nothing in it can throw); the function
// shares its exception tables with TSplitter::Arrange.
int TMainWindow::GetConsoleWindowHeight() const
{
	return Console->Height;
}

// 0x4ac7f0, line 503. SetPlayerDestination returns 1 when the step can be
// walked and 2 when the player has to stop.
void TMainWindow::Walk(int dx, int dy, bool Diagonal)
{
	int x, y, z;
	GetPlayerPosition(&x, &y, &z);
	switch (SetPlayerDestination(x + dx, y + dy, z, true, Diagonal)) {
	case 1:
		SendWalk();
		break;
	case 2:
		SendStop();
		break;
	default:
		break;
	}
}

// 0x4ac9e0, lines 511-539. The assertion comes before the try block. An
// open container window is filled again: the body is updated (scrolled to
// the top, its counts, icon, title and "up" button; ContentWindows.cpp) and,
// while it is minimized, the window gets the name and the object of the
// container and the "up" state of a container that has a parent here.
// GCC's analyzer (Debug, issue #111) takes SkinProperties()' styles, set by InitSkinProperties, as uninitialized, and
// the title and icon SetTitle and SetIcon take as leaks (the window owns them).
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-use-of-uninitialized-value\"")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-malloc-leak\"")
void TMainWindow::OpenContainer(int Container) const
{
	PATELLA_ASSERT(DialogWindow!=nullptr);
	TSubWindow *Window = static_cast<TSubWindow *>(DialogWindow->GetContentWindow(Container + 0x40));
	if (Window == nullptr) {
		DialogWindow->Rearrange(16, CreateContainerWindow(Container));
		return;
	}
	Window->Arrange();
	TContainerWindowBody *Body = static_cast<TContainerWindowBody *>(Window->Body);
	Body->Update();
	if (Body->IsMinimized()) {
		Window->SetTitle(new TLabel(SkinProperties().Style194.Data38, GetContainerName(Container)));
		Window->SetIcon(new TObjectIcon(GetContainerObject(Container), &SkinProperties().Style194.Data80, false));
		if (GetContainerHasParent(Container))
			Window->ShowUpButton();
		else
			Window->HideUpButton();
	}
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")

// 0x4acdb0, line 552
void TMainWindow::CloseContainer(int Container) const
{
	if (DialogWindow != nullptr) {
		TContentWindow *Window = DialogWindow->GetContentWindow(Container + 0x40);
		if (Window != nullptr)
			DialogWindow->DeleteChild(Window);
	}
}

// 0x4afec0, lines 560-565. Both assertions come before the try block.
void TMainWindow::OpenTrade() const
{
	PATELLA_ASSERT(DialogWindow!=nullptr);
	PATELLA_ASSERT(!DialogWindow->hasContentWindow(TRADE_WINDOW));
	OpenContentWindow(TRADE_WINDOW);
}

// 0x4acf60, lines 576-579. The trade window's body gets event 0x15 (the
// counter offer arrived).
void TMainWindow::UpdateTrade() const
{
	const TSubWindow *Window = static_cast<TSubWindow *>(DialogWindow->GetContentWindow(TRADE_WINDOW));
	if (Window != nullptr) {
		PATELLA_ASSERT(Window->getBody()!=nullptr);
		Window->getBody()->HandleEvent(0x15);
	}
}

// 0x4b0230, line 590
void TMainWindow::CloseTrade() const
{
	CloseContentWindow(TRADE_WINDOW);
}

// 0x4ad1e0, lines 606-610. The jump table 0x4ad48c has the four cases.
void TMainWindow::OpenContentWindow(int Identifier) const
{
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
			TERROR1("unknown identifier", Identifier);
		}
		DialogWindow->Rearrange(16, Window);
	}
}

// 0x4ad4a0, line 621
void TMainWindow::CloseContentWindow(int Identifier) const
{
	TContentWindow *Window = DialogWindow->GetContentWindow(Identifier);
	if (Window != nullptr)
		DialogWindow->DeleteChild(Window);
}

// 0x4ad650, line 632. The console takes the keyboard while no modal window
// is open (Control.cpp passes ModalWindow==NULL).
void TMainWindow::SetConsoleActive(bool Active) const
{
	if (Console != nullptr)
		Console->SetActive(Active);
}

// --- TStartWindow ----------------------------------------------------------

// 0x4ad7f0, line 671. The buttons sit on a plain window that the frame
// (skins 0x34-0x3c) surrounds.
TStartWindow::TStartWindow()
{
	TWindow *Panel = new TWindow;
	Panel->SetSize(SkinProperties().Style0D8.Data00, SkinProperties().Style0D8.Data04);
	TComponent *Button = new TTextButton(0, 0, "Enter Game", 0x12, 0x13, TCallback(this, 0x1f5), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties().Style0D8.Data10.x, SkinProperties().Style0D8.Data10.y);
	Button = new TTextButton(0, 0, "Options", 0x12, 0x13, TCallback(this, 0x1f7), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties().Style0D8.Data18.x, SkinProperties().Style0D8.Data18.y);
	Button = new TTextButton(0, 0, "Info", 0x12, 0x13, TCallback(this, 0x1f6), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties().Style0D8.Data20.x, SkinProperties().Style0D8.Data20.y);
	Button = new TTextButton(0, 0, "Exit Game", 0x12, 0x13, TCallback(this, 7), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties().Style0D8.Data28.x, SkinProperties().Style0D8.Data28.y);
	Button = new TTextButton(0, 0, "Access Account", 0x12, 0x13, TCallback(this, 0x1fc), nullptr);
	Panel->InsertChild(Button);
	Panel->SetChildPosition(Button, SkinProperties().Style0D8.Data30.x, SkinProperties().Style0D8.Data30.y);
	Frame = new TBorder(Panel, 0x34, 0x3a, 0x35, 0x38, 0x39, 0x36, 0x3b, 0x37, 0x3c, SkinProperties().Data134);
	InsertChild(Frame);
	Frame->FitToWindow();
}

// 0x4adc60, line 691. Picture 1, the start screen, goes to the 640x480
// surface 2 and is stretched over the window.
void TStartWindow::Draw(int Surface, int x, int y)
{
	DrawPicture(2, 0, 0, 1);
	GetSurface()->StretchBlitRect(Surface, TRect(x, y, Width, Height), 2, TRect(0, 0, 640, 480));
	TWindow::Draw(Surface, x, y);
}

// 0x4ade70, line 703
void TStartWindow::SetSize(int Width, int Height)
{
	TWindow::SetSize(Width, Height);
	SetChildPosition(Frame, SkinProperties().Style0D8.Data08.x, SkinProperties().Style0D8.Data08.y);
}

// 0x4ae030, line 731. The buttons work only while there is no connection.
void TStartWindow::HandleEvent(int Event)
{
	const patella::TGUI &GUI = patella::bridge::GUI();
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
		GUI.ExitProgramCallback();
		break;
	case 0x1fc:
		GUI.OpenURLCallback(TextAccountURL);
		break;
	default:
		break;
	}
}

// --- TSplitter -------------------------------------------------------------

// 0x4ae240, line 749
TSplitter::TSplitter()
{
	Dragging = false;
	DrawingListLength = 3;
	DrawingList.reset(new TDrawing[3]);
	for (int i = 0; i < DrawingListLength; i++)
		DrawingList[i].skin = 0;
}

// 0x4ae430. Skins 8 (top), 7 (middle, stretched) and 9 (bottom). As in
// GetConsoleWindowHeight the try block is gone.
void TSplitter::Arrange()
{
	DrawingList[0] = TDrawing(8, 0, 0, Width, Skins()[8].height, 0, 0);
	DrawingList[1] = TDrawing(7, 0, Skins()[8].height, Width, Height - Skins()[9].height - Skins()[8].height, 0, 0);
	DrawingList[2] = TDrawing(9, 0, Height - Skins()[9].height, Width, Skins()[9].height, 0, 0);
}

// 0x4ae530, line 780. Mouse cursor 2 is the vertical size arrow. While
// dragging, the console follows the mouse; the global MainWindow is used,
// not the parent. x has no name in the catch block.
bool TSplitter::MouseMove([[maybe_unused]] int x, int y)
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	SetMouseCursor(2);
	if (Dragging) {
		GUI.MainWindow->ResizeConsole(DragY - y + GUI.MainWindow->GetConsoleWindowHeight());
		GUI.MainWindow->SetSize(GUI.MainWindow->Width, GUI.MainWindow->Height);
	}
	return true;
}

// 0x4ae700, line 795
bool TSplitter::LeftButtonDown(int x, int y, [[maybe_unused]] int Flags)
{
	SetCapture(nullptr);
	DragX = x;
	Dragging = true;
	DragY = y;
	return true;
}

// 0x4ae8b0, line 807. ReleaseCapture clears Dragging.
bool TSplitter::LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags)
{
	if (Dragging)
		ReleaseCapture(nullptr);
	return true;
}

// --- The main window -------------------------------------------------------

// 0x4b05b0, lines 822-828. Called by CMainFrame::OnCreate.
void patella::TGUI::InitMainWindow(int Width, int Height, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode),
	void (*OpenURL)(const char *URL))
{
	std::unique_ptr<TMainWindow> Made(new TMainWindow(Width, Height));
	(void)MainWindow.release(); // NOLINT(bugprone-unused-return-value): the original stored over it (set only once)
	MainWindow = std::move(Made);
	PATELLA_ASSERT_VALUES(GetActionState()==ACTIONSTATE_NONE, GetActionState());
	ExitProgramCallback = ExitProgram;
	ModalWindow = nullptr;
	SetDisplayModeCallback = SetDisplayMode;
	OpenURLCallback = OpenURL;
}

// The free function of the original's name (issue #118): src/SdlFrame.cpp and the golden MainWindow test call it.
void InitMainWindow(int Width, int Height, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode),
	void (*OpenURL)(const char *URL))
{
	patella::bridge::GUI().InitMainWindow(Width, Height, ExitProgram, SetDisplayMode, OpenURL);
}
// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
