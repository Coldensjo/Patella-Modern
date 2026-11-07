// MainWindow.h: the main game window, the window that fills the client
// area and holds every other window except the modal one.
//
// TMainWindow (vtable 0x56c498, 0x4c bytes) has two modes. Out of the game
// it holds the start window (TStartWindow, vtable 0x56c5c0, 0x30 bytes): the
// background picture and a frame with the buttons "Enter Game", "Access
// Account", "Options", "Info" and "Exit Game". In the game it holds the map
// frame (MapWindow.h) on the left above the console, a splitter between the
// two (TSplitter, vtable 0x56c660, 0x30 bytes) that moves the console's top
// edge, and on the right the side bar (a frame around the panel of the
// side-bar windows) above the dialog window that holds the containers and
// the skill, battle, VIP and trade windows. The keys and hotkeys of the game
// window are handled here (walking, turning and the Ctrl shortcuts).
//
// Names from the binary: MainWindow and ModalWindow (assertions in
// Control.cpp), DialogWindow ("DialogWindow!=NULL"), getBody, OldWindow,
// TRADE_WINDOW, ACTIONSTATE_NONE and the parameter names in the catch blocks
// (Width, Height, Surface, Key, Event, ConsoleHeight, dx, dy, Container,
// Identifier, x, y). The class names and the other names are ours
// (notes/decisions.md, "MainWindow.cpp").

#pragma once

#include "GUI.h"

class TMapFrame;      // MapWindow.h
class TBorder;        // DialogElements.h
class TDialogWindow;  // DialogWindow.h
class TConsoleWindow; // ConsoleWindow.h
class TStartWindow;
class TSplitter;

// TRADE_WINDOW is DialogWindow.h's since issue #118 (ContentWindows.cpp no longer includes this header).

class TMainWindow : public TWindow {
public:
	TMainWindow(int Width, int Height);               // 0x4b03c0, line 85
	~TMainWindow() override;                          // +0x00, 0x4ac010 (deleting 0x4ac0d0)
	void Draw(int Surface, int x, int y) override;    // +0x44, 0x4ac0f0, line 127
	void SetSize(int Width, int Height) override;     // +0x34, 0x4ac340, line 177
	void KeyPress(int Key) override;                  // +0x6c, 0x4aea50, line 203
	void HandleEvent(int Event) override;             // +0x70, 0x4aed10, lines 308-339
	void ChangeMode(bool InGame);                     // 0x4af5e0, line 463
	void ResizeConsole(int ConsoleHeight) const;      // 0x4ac610, line 475
	[[nodiscard]] int GetConsoleWindowHeight() const; // 0x4ac7c0
	void Walk(int dx, int dy, bool Diagonal);         // 0x4ac7f0, line 503
	void OpenContainer(int Container) const;          // 0x4ac9e0, lines 511-539
	void CloseContainer(int Container) const;         // 0x4acdb0, line 552
	void OpenTrade() const;                           // 0x4afec0, lines 560-565
	void UpdateTrade() const;                         // 0x4acf60, lines 576-579
	void CloseTrade() const;                          // 0x4b0230, line 590
	void OpenContentWindow(int Identifier) const;     // 0x4ad1e0, lines 606-610
	void CloseContentWindow(int Identifier) const;    // 0x4ad4a0, line 621
	void SetConsoleActive(bool Active) const;         // 0x4ad650, line 632

	// The children; all NULL in the mode that does not use them.
	TMapFrame *MapFrame;         // +0x2c
	TBorder *SideBar;            // +0x30, the frame around SidePanel
	TDialogWindow *DialogWindow; // +0x34, containers, skills, battle, VIP and trade
	TDialogWindow *SidePanel;    // +0x38, inventory and the other side-bar windows
	TConsoleWindow *Console;     // +0x3c
	TSplitter *Splitter;         // +0x40
	TStartWindow *StartWindow;   // +0x44
	bool InGame;                 // +0x48

protected:
	void OnClose() override; // the destructor's preferences (issue #108)
};

// The window behind the start dialogs (vtable 0x56c5c0, 0x30 bytes).
class TStartWindow final : public TWindow {
public:
	TStartWindow(); // 0x4ad7f0, line 671
	// The destructor is the implicit one; its deleting destructor 0x4adc40
	// (a jmp to ~TWindow) is shared with TMapFrame's.
	void Draw(int Surface, int x, int y) override; // +0x44, 0x4adc60, line 691
	void SetSize(int Width, int Height) override;  // +0x34, 0x4ade70, line 703
	void HandleEvent(int Event) override;          // +0x70, 0x4ae030, line 731

	TBorder *Frame; // +0x2c, around the buttons
};

// The bar between the map and the console (vtable 0x56c660, 0x30 bytes):
// dragging it changes the console's height.
class TSplitter final : public TElement {
public:
	TSplitter(); // 0x4ae240, line 749
	// The destructor is the implicit one (its deleting destructor is folded
	// into 0x429590).
	void Arrange() override;                               // +0x48, 0x4ae430
	bool MouseMove(int x, int y) override;                 // +0x4c, 0x4ae530, line 780
	bool LeftButtonDown(int x, int y, int Flags) override; // +0x50, 0x4ae700, line 795
	bool LeftButtonUp(int x, int y, int Flags) override;   // +0x54, 0x4ae8b0, line 807
	// +0x7c, 0x4ae420 (inline: no exception frame, a jmp to the base).
	void ReleaseCapture(TComponent *Component) override
	{
		Dragging = false;
		TElement::ReleaseCapture(Component);
	}

	bool Dragging; // +0x24
	int DragX;     // +0x28, where the button was pressed
	int DragY;     // +0x2c
};

// ModalWindow (0x5d16ac) and MainWindow (0x5d16b0) are TGUI's members (GUI.h, issue #107):
// patella::bridge::GUI().ModalWindow and .MainWindow.

void InitMainWindow(int Width, int Height, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode),
	void (*OpenURL)(const char *URL)); // 0x4b05b0, lines 822-828
void ExitMainWindow();                 // 0x4ac090
