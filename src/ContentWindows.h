// ContentWindows.h: the windows of the side bar and of the dialog column
// (c:\mud\tibiaplayer\ContentWindows.h).
//
// MainWindow.cpp fills two TDialogWindow columns (DialogWindow.h) with these
// windows. The side panel holds the side-bar windows, TContentWindows with
// fixed contents: the inventory (identifier 1, a plain TContentWindow with a
// TInventoryView and a TCombatControls, made by CreateInventoryWindow), the
// small inventory (2, TSmallInventoryWindow), the automap (3,
// TAutomapWindow: a TAutomapView, a TCompassRose and buttons), the status
// bars (4, TStatusWindow) and the buttons that open the skill, battle and
// VIP windows (5, TButtonsWindow). The dialog column holds TSubWindows with a
// body (TSubWindowBody): the containers (0x40 plus the container number), the
// skills (6), the battle list (7), the trade (8, TRADE_WINDOW) and the VIP
// list (9). The Create* functions build those windows with their buttons,
// icon and title and size them before MainWindow.cpp hands them to
// Rearrange(16).
//
// Names from the binary: _CheckObject, _RoseSkin, NumberOfSkins, islarge,
// _Container, ScrollbarX, EventComponent, NumberOfLine, Lines, Value,
// Progress, Position, SkinProperties.StatusWindow and PlayerStates (the class
// names TStatusWindow and TPlayerStates), BuddylistWindow, RadioButtonGroup
// (DialogElements.cpp's assertion) and the catch-block parameters (Surface, x,
// y, Width, Height, Event, Direction, Code). The class, member and function
// names are ours; see notes/decisions.md, "ContentWindows.cpp".

#pragma once

#include "common.h" // IWYU pragma: keep (first: it configures the CRT and MFC for the headers after it)
#include "GUI.h"
#include "DialogWindow.h"
#include "Map.h" // TMark
#include "DialogElements.h"
#include "types/FixedString.h"
#include <memory>

class TSkillContextMenu; // ContextMenus.h

// --- The side-bar windows -------------------------------------------------------

// Identifier 5: the buttons that open and close the skill, battle and VIP
// windows of the dialog column (_CheckObject, whose windows they show as
// pressed) and "Logout". Their events go to the main window (14 skills, 15
// battle, 22 VIP, 507 logout).
class TButtonsWindow : public TContentWindow {
public:
	TButtonsWindow(TDialogWindow *_CheckObject, TWindow *Receiver); // 0x426480, lines 292-309
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override; // +0x44, 0x4268c0, line 322
	bool HelpMouseMove(int x, int y) override;     // +0x68, 0x426aa0, line 334

	TDialogWindow *CheckObject;      // +0x44
	TTextToggleButton *SkillsButton; // +0x48
	TTextToggleButton *BattleButton; // +0x4c
	TTextToggleButton *BuddyButton;  // +0x50
	TTextButton *LogoutButton;       // +0x54
};

// The map of the automap window: 106x106 pixels around ViewX/ViewY/ViewZ of
// its parent, one automap point per Zoom/2 pixels.
class TAutomapView final : public TElement {
public:
	// The constructor and the destructor are the implicit ones (deleting
	// destructor 0x429590, folded).
	void Draw(int Surface, int x, int y) override;        // +0x44, 0x4276f0, line 652
	bool LeftButtonUp(int x, int y, int Flags) override;  // +0x54, 0x427e40, line 696
	bool RightButtonUp(int x, int y, int Flags) override; // +0x5c, 0x428080, line 732
	virtual TMark GetMark(int x, int y);                  // +0x84, 0x4282e0, line 784
};

// The compass rose of the automap window: a click on one of its five areas
// (the middle and the four directions) centres or scrolls the map.
class TCompassRose final : public TElement {
public:
	TCompassRose(int _RoseSkin); // 0x428680, line 791
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void Draw(int Surface, int x, int y) override;       // +0x44, 0x428800, line 803
	bool LeftButtonUp(int x, int y, int Flags) override; // +0x54, 0x4289c0, line 825
	void MouseHover(int x, int y) override;              // +0x64, 0x428cb0, line 836

	int RoseSkin; // +0x24
};

// Identifier 3: the automap with its view, the compass rose and the buttons
// for the floor and the zoom. The shown area follows the player (Centred)
// until it is scrolled; Changed asks the view to redraw.
class TAutomapWindow : public TContentWindow {
public:
	TAutomapWindow();                          // 0x4368c0, line 387
	~TAutomapWindow() override;                // +0x00, 0x426c40, line 406 (deleting 0x436e80)
	bool HelpMouseMove(int x, int y) override; // +0x68, 0x426e40, line 418
	void MouseHover(int x, int y) override;    // +0x64, 0x426fe0, line 433
	void Arrange() override;                   // +0x48, 0x427200, line 465
	void HandleEvent(int Event) override;      // +0x70, 0x436ea0, line 523
	void Scroll(int Direction);                // 0x4274c0, line 550

	TAutomapView *View;           // +0x44
	TTextButton *CentreButton;    // +0x48, event 106
	TSkinButton *FloorUpButton;   // +0x4c, event 3
	TSkinButton *FloorDownButton; // +0x50, event 4
	TSkinButton *ZoomInButton;    // +0x54, event 107
	TSkinButton *ZoomOutButton;   // +0x58, event 108
	TCompassRose *CompassRose;    // +0x5c
	bool Changed;                 // +0x60, the view must be redrawn
	bool Centred;                 // +0x61, the view follows the player
	int ViewX;                    // +0x64, the centre of the view
	int ViewY;                    // +0x68
	int ViewZ;                    // +0x6c
	int PlayerX;                  // +0x70, the player's position when the view was scrolled
	int PlayerY;                  // +0x74
	int PlayerZ;                  // +0x78
	int Zoom;                     // +0x7c, 1, 2, 4 or 8 (Preferences.cpp's AutomapZoom)

protected:
	void OnClose() override; // the destructor's zoom and hotkeys (issue #108)
};

// The capacity and the soul points of the inventory (Style3E0, Style428).
class TCapacityLabel final : public TElement {
public:
	// The constructor and the destructor are the implicit ones.
	void Draw(int Surface, int x, int y) override; // +0x44, 0x428e40, line 858
};
class TSoulLabel final : public TElement {
public:
	// The constructor and the destructor are the implicit ones.
	void Draw(int Surface, int x, int y) override; // +0x44, 0x429090, line 875
};

// The icons of the player's states (poisoned, burning, ...) in a row: in the
// inventory (IsLarge, ten icons) or in the small inventory (nine).
class TPlayerStates final : public TElement {
public:
	TPlayerStates(bool islarge); // 0x4292e0, line 901
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void Draw(int Surface, int x, int y) override;   // +0x44, 0x437210, lines 920-925
	void MouseHover(int x, int y) override;          // +0x64, 0x437630, lines 973-976
	[[nodiscard]] int GetNumberOfIcons() const;      // 0x4295b0, line 947
	[[nodiscard]] int GetIconAt(int x, int y) const; // 0x4297a0, line 995
	int GetIcon(int Position);                       // 0x429990, lines 1036-1070

	bool IsLarge; // +0x24
	int MaxIcons; // +0x28, 10 or 9
};

// Identifier 2: the small inventory, a drop area for the body slots (skin
// 99) with the combat controls, the "maximise" button, capacity, soul and
// the states.
class TSmallInventoryWindow final : public TContentWindow {
public:
	TSmallInventoryWindow(); // 0x429eb0, line 1083
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override;       // +0x44, 0x42a0c0, line 1099
	bool LeftButtonUp(int x, int y, int Flags) override; // +0x54, 0x42a300, line 1127
	bool HelpMouseMove(int x, int y) override;           // +0x68, 0x42a590, line 1141
};

// The inventory's ten body slots with the "minimise" button, capacity, soul
// and the states.
class TInventoryView final : public TWindow {
public:
	TInventoryView(TWindow *Receiver); // 0x42a750, line 1170
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override;         // +0x44, 0x42aa90, lines 1181-1209
	bool LeftButtonDown(int x, int y, int Flags) override; // +0x50, 0x437930, line 1229
	bool LeftButtonUp(int x, int y, int Flags) override;   // +0x54, 0x437b30, lines 1251-1277
	bool RightButtonUp(int x, int y, int Flags) override;  // +0x5c, 0x437fe0, line 1329
	bool HelpMouseMove(int x, int y) override;             // +0x68, 0x42b1b0, lines 1339-1344
	int GetSlotAt(int x, int y);                           // 0x42b450, 1-10 or 0
};

// The fight modes (radio buttons), the secure mode, "Stop" and the options
// and help buttons; skin buttons in the small inventory, text buttons in the
// large one (IsLarge).
class TCombatControls final : public TWindow {
public:
	TCombatControls(const TCombatControls &) = delete;
	TCombatControls &operator=(const TCombatControls &) = delete;
	TCombatControls(TWindow *Receiver, bool Large); // 0x42b4d0, line 1425
	// Inline (0x425e00, deleting destructor 0x425e50).
	~TCombatControls() override
	{
		AttackModes.reset();
		ChaseModes.reset();
	}
	void Draw(int Surface, int x, int y) override; // +0x44, 0x42bb70, line 1450
	bool HelpMouseMove(int x, int y) override;     // +0x68, 0x42bd30, lines 1461-1466
	void HandleEvent(int Event) override;          // +0x70, 0x42bff0, line 1502

	bool IsLarge;                                   // +0x2c
	std::unique_ptr<TRadioButtonGroup> AttackModes; // +0x30, offensive, balanced, defensive (issue #58)
	std::unique_ptr<TRadioButtonGroup> ChaseModes;  // +0x34, stand, chase
	TSkinButton *StandButton;                       // +0x38
	TSkinButton *ChaseButton;                       // +0x3c
	TToggleButton *SecureButton;                    // +0x40, State: attacks on unmarked players allowed
};

// Identifier 4: the hit point and mana bars with their values.
class TStatusWindow : public TContentWindow {
public:
	TStatusWindow(); // 0x438310, line 1522
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override; // +0x44, 0x42c1e0, lines 1550-1559
	bool HelpMouseMove(int x, int y) override;     // +0x68, 0x42c830, line 1571
	void Arrange() override;                       // +0x48, 0x42c9d0, line 1598

	int Data44; // +0x44, 100, never read
	int Data48; // +0x48, 100, never read
};

// --- The bodies of the sub-windows ------------------------------------------------

// TSubWindowBody, the content of a TSubWindow, is DialogWindow.h's since issue #118.

// The objects of a container in rows of SkinProperties.Data62C slots, with
// a scroll bar.
class TContainerWindowBody : public TSubWindowBody {
public:
	TContainerWindowBody(int _Container);                           // 0x438500, line 1618
	~TContainerWindowBody() override;                               // +0x00, 0x42cc70, line 1628 (deleting 0x438700)
	void Draw(int Surface, int x, int y) override;                  // +0x44, 0x42ce10, lines 1644-1666
	void SetSize(int Width, int Height) override;                   // +0x34, 0x42d420, lines 1675-1683
	int getMinimumHeight() override;                                // +0x40, 0x42d6f0, line 1693
	int GetMaximumHeight() override;                                // +0xb0, 0x42d890, line 1712
	void Arrange() override;                                        // +0x48, 0x42da20, line 1744
	bool LeftButtonDown(int x, int y, int Flags) override;          // +0x50, 0x438720, line 1764
	bool LeftButtonUp(int x, int y, int Flags) override;            // +0x54, 0x438920, lines 1785-1812
	bool RightButtonUp(int x, int y, int Flags) override;           // +0x5c, 0x438dd0, line 1862
	bool MouseWheel(int x, int y, int Code) override;               // +0x60, 0x42dcb0, line 1875
	bool HelpMouseMove(int x, int y) override;                      // +0x68, 0x42de80, lines 1885-1890
	void HandleEvent(int Event) override;                           // +0x70, 0x42e120, line 1904
	virtual void Update();                                          // +0xb8, 0x439100, line 1917
	[[nodiscard]] int GetSlotAt(int x, int y, bool Occupied) const; // 0x42e2c0, line 1940
	void UpdateUpButton();                                          // 0x42e4e0, line 1960
	// Inline: emitted at the start of the object (0x425a10, 0x425a40).
	// Odd, kept: the rows start at Data630.x (the left edge of the
	// first slot, the same as Data630.y in the game's skin).
	int GetInitialHeight() override
	{
		return (ObjectCount / SkinProperties().Data62C + (ObjectCount % SkinProperties().Data62C > 0))
			* SkinProperties().Data64C
			+ SkinProperties().Data630.x;
	}
	int GetContentHeight() override
	{
		return (Capacity / SkinProperties().Data62C + (Capacity % SkinProperties().Data62C > 0))
			* SkinProperties().Data64C
			+ SkinProperties().Data630.x;
	}

	TScrollBar *ScrollBar; // +0x30
	int Container;         // +0x34
	int ObjectCount;       // +0x38, at the last Arrange
	int Capacity;          // +0x3c
	bool HasParent;        // +0x40, the "up" button is shown

protected:
	void OnClose() override; // the destructor's SendCloseContainer (issue #108)
};

// The player's skills, one per line (Lines, 15), with a progress bar for
// those whose bar is switched on.
class TSkillsWindowBody : public TSubWindowBody {
public:
	TSkillsWindowBody();                                                         // 0x4392a0, line 1980
	~TSkillsWindowBody() override;                                               // +0x00, 0x42e6b0 (deleting 0x4394b0)
	void Draw(int Surface, int x, int y) override;                               // +0x44, 0x4394d0, lines 2003-2063
	bool MouseWheel(int x, int y, int Code) override;                            // +0x60, 0x42e700, line 2076
	bool HelpMouseMove(int x, int y) override;                                   // +0x68, 0x42e8d0, lines 2086-2091
	void MouseHover(int x, int y) override;                                      // +0x64, 0x439c30, line 2133
	bool RightButtonUp(int x, int y, int Flags) override;                        // +0x5c, 0x42eb70, line 2144
	void SetSize(int Width, int Height) override;                                // +0x34, 0x42ed10, lines 2153-2160
	void Arrange() override;                                                     // +0x48, 0x42efd0, line 2173
	int getMinimumHeight() override;                                             // +0x40, 0x42f190, line 2183
	int GetInitialHeight() override;                                             // +0xac, 0x42f330, line 2196
	int GetMaximumHeight() override;                                             // +0xb0, 0x42f500, line 2206
	int GetContentHeight() override;                                             // +0xb4, 0x439ef0, line 2225
	bool GetLine(int NumberOfLine, char *Text, int &Value, int &Progress) const; // 0x42f6a0, lines 2234-2280

	TSkillContextMenu *ContextMenu; // +0x30, while the skill menu (ContextMenus.cpp) is open
	TScrollBar *ScrollBar;          // +0x34
	int Lines;                      // +0x38, 15
};

// The battle list: the opponents (Creatures.cpp) with outfit, name, health
// bar and marks.
class TBattleWindowBody final : public TSubWindowBody {
public:
	TBattleWindowBody(); // 0x43a0c0, line 2300
	// Inline (0x425a70, deleting destructor 0x425dc0); its SetHighlightID(0) is
	// the close since issue #108.
	~TBattleWindowBody() override { CheckClosed("TBattleWindowBody"); }
	void Draw(int Surface, int x, int y) override;         // +0x44, 0x43a2c0, lines 2328-2452
	void SetSize(int Width, int Height) override;          // +0x34, 0x42fc80, lines 2465-2472
	void Arrange() override;                               // +0x48, 0x42ff30, line 2487
	bool LeftButtonDown(int x, int y, int Flags) override; // +0x50, 0x4300e0, line 2498
	bool LeftButtonUp(int x, int y, int Flags) override;   // +0x54, 0x43ae20, lines 2521-2544
	bool RightButtonUp(int x, int y, int Flags) override;  // +0x5c, 0x43b2f0, line 2573
	void MouseHover(int x, int y) override;                // +0x64, 0x43b510, line 2593
	bool MouseWheel(int x, int y, int Code) override;      // +0x60, 0x430280, line 2606
	bool HelpMouseMove(int x, int y) override;             // +0x68, 0x430450, lines 2616-2621
	int getMinimumHeight() override;                       // +0x40, 0x4306f0, line 2631
	int GetInitialHeight() override;                       // +0xac, 0x430890, line 2644
	int GetContentHeight() override;                       // +0xb4, 0x430a50, line 2654
	int GetOpponentAt(int x, int y);                       // 0x430be0, line 2668

	TScrollBar *ScrollBar;          // +0x30
	int NumberOfOpponents;          // +0x34, at the last Draw; -1 at first
	TFixedString<32> MouseOverText; // +0x38, a shortened name in full

protected:
	void OnClose() override; // the destructor's SetHighlightID(0) (issue #108)
};
// The parts of the trade body: the two offers (TTradeOffers), the names
// above them (TTradeNames) and the buttons below (TTradeButtons).
class TTradeOffers final : public TWindow {
public:
	TTradeOffers(); // 0x430dc0, line 2687
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override;                 // +0x44, 0x430fd0, lines 2698-2730
	void SetSize(int Width, int Height) override;                  // +0x34, 0x4317b0, line 2745
	int getMinimumHeight() override;                               // +0x40, 0x4319b0, line 2755
	int GetContentHeight();                                        // 0x431b40, line 2769
	void Arrange() override;                                       // +0x48, 0x431d10, line 2782
	bool LeftButtonUp(int x, int y, int Flags) override;           // +0x54, 0x43b710, line 2801
	bool LeftButtonDown(int x, int y, int Flags) override;         // +0x50, 0x431ec0, line 2812
	bool RightButtonUp(int x, int y, int Flags) override;          // +0x5c, 0x43b900, line 2831
	bool MouseWheel(int x, int y, int Code) override;              // +0x60, 0x432060, line 2844
	void GetOfferAt(int x, int y, int *Side, int *Position) const; // 0x432230, line 2877

	TScrollBar *ScrollBar; // +0x2c
	TSeparator *Separator; // +0x30
};

class TTradeNames final : public TWindow {
public:
	TTradeNames(); // 0x432470, line 2894
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override; // +0x44, 0x432770, lines 2901-2907
	void SetSize(int Width, int Height) override;  // +0x34, 0x432b00, line 2919
	void Arrange() override;                       // +0x48, 0x432d40, line 2930

	TLabel *OwnName;       // +0x2c
	TLabel *CounterName;   // +0x30
	TSeparator *Separator; // +0x34
};

class TTradeButtons final : public TWindow {
public:
	TTradeButtons(); // 0x432f00, line 2947
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override;     // +0x44, 0x433140, lines 2954-2960
	void SetSize(int Width, int Height) override;      // +0x34, 0x4334d0, line 2974
	void SetCancelButton(bool Cancel);                 // 0x433720, line 2988
	void ShowAcceptButton(TComponent *EventComponent); // 0x433940, lines 2995-3005
	void Wait();                                       // 0x433c40, line 3020

	int Data2C;                // +0x2c, not used
	int Data30;                // +0x30, not used
	TTextButton *CancelButton; // +0x34, "Cancel" or "Reject" (Callback)
	TTextButton *AcceptButton; // +0x38, "Accept" (event 1 of the body)
	TLabel *WaitLabel;         // +0x3c
	TSeparator *Separator;     // +0x40
	TCallback Callback;        // +0x44, closes the window (CreateTradeWindow)
};

// The trade: both offers, the names and the buttons. Event 21 (MainWindow.cpp's
// UpdateTrade: the counter offer came) shows "Accept", event 1 accepts.
class TTradeWindowBody final : public TSubWindowBody {
public:
	TTradeWindowBody();           // 0x43baf0, line 3037
	~TTradeWindowBody() override; // +0x00, 0x433e60, line 3048 (deleting 0x43bd20)
	// Inline: emitted at the start of the object (0x4259b0).
	void Draw(int Surface, int x, int y) override
	{
		if (!Minimized)
			TWindow::Draw(Surface, x, y);
	}
	int GetContentHeight() override;              // +0xb4, 0x434000
	void SetSize(int Width, int Height) override; // +0x34, 0x434010, line 3066
	bool HelpMouseMove(int x, int y) override;    // +0x68, 0x4342b0, lines 3076-3081
	int getMinimumHeight() override;              // +0x40, 0x434550, line 3092
	int GetInitialHeight() override;              // +0xac, 0x4346f0, line 3102
	int GetMaximumHeight() override;              // +0xb0, 0x434880, line 3112
	void HandleEvent(int Event) override;         // +0x70, 0x434a20, line 3130

	TTradeButtons *Buttons; // +0x30
	TTradeOffers *Offers;   // +0x34
	TTradeNames *Names;     // +0x38

protected:
	void OnClose() override; // the destructor's SendRejectTrade and ClearOffers (issue #108)
};

// The VIP list (Creatures.cpp's buddies) with icon, name and online state.
class TBuddylistWindowBody final : public TSubWindowBody {
public:
	TBuddylistWindowBody(const TBuddylistWindowBody &) = delete;
	TBuddylistWindowBody &operator=(const TBuddylistWindowBody &) = delete;
	TBuddylistWindowBody(); // 0x43bd40, line 3150
	// Inline (0x425ad0, deleting destructor 0x425de0).
	~TBuddylistWindowBody() override
	{
		if (MouseOverText != nullptr)
			MouseOverText.reset();
		MouseOverText = nullptr;
	}
	void Draw(int Surface, int x, int y) override;                // +0x44, 0x434bf0, lines 3176-3214
	void SetSize(int Width, int Height) override;                 // +0x34, 0x4352e0, lines 3225-3232
	bool RightButtonUp(int x, int y, int Flags) override;         // +0x5c, 0x43bf40, line 3249
	bool LeftButtonDoubleClick(int x, int y, int Flags) override; // +0x58, 0x43c110, line 3267
	bool MouseWheel(int x, int y, int Code) override;             // +0x60, 0x435590, line 3280
	void MouseHover(int x, int y) override;                       // +0x64, 0x43c2e0, line 3312
	bool HelpMouseMove(int x, int y) override;                    // +0x68, 0x435760, lines 3322-3327
	void Arrange() override;                                      // +0x48, 0x435a00, line 3340
	int getMinimumHeight() override;                              // +0x40, 0x435bb0, line 3350
	int GetInitialHeight() override;                              // +0xac, 0x435d50, line 3363
	int GetContentHeight() override;                              // +0xb4, 0x435f20, line 3374
	int GetBuddyAt(int x, int y);                                 // 0x4360c0, line 3389

	TScrollBar *ScrollBar;                 // +0x30
	int NumberOfBuddies;                   // +0x34, at the last Draw; -1 at first
	std::unique_ptr<char[]> MouseOverText; // +0x38, a shortened name in full (new char[36]; issue #58)
};

// --- The creators ------------------------------------------------------------------

// The inventory windows' Receiver (and TButtonsWindow's, TInventoryView's, TCombatControls'), the main window
// (MainWindow.cpp passes this): only their buttons' TCallback receiver, so a TWindow since issue #118 (the original's
// TMainWindow; ContentWindows.cpp then does not include MainWindow.h, which made ContentWindows and MainWindow a
// cycle).

TContentWindow *CreateInventoryWindow(TWindow *Receiver);      // 0x4362b0, line 106
TContentWindow *CreateSmallInventoryWindow(TWindow *Receiver); // 0x436510, line 140
TSubWindow *CreateContainerWindow(int Container);              // 0x43c520, line 170
TSubWindow *CreateSkillsWindow();                              // 0x43c920, line 197
TSubWindow *CreateBattleWindow();                              // 0x43cc90, line 225
TSubWindow *CreateTradeWindow();                               // 0x43d000, line 253
TSubWindow *CreateBuddylistWindow();                           // 0x43d390, line 280
