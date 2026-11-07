// ContextMenus.h: the help bubble and the context menus, modal windows that
// Control.cpp creates (the help bubble in action state 9, help mode).
//
// THelpBubble (vtable 0x55f8b8, 0x30 bytes) shows a help text next to the
// window it explains (Help.cpp's help area), on the side where it fits. A
// TContextMenu (vtable 0x55f730, 0x34 bytes) is a column of entries
// (TContextMenuEntry, vtable 0x55f6a8, 0x74 bytes: a text, an optional
// shortcut on the right and a callback) with separators between groups. Its
// subclasses fill it in their constructors and do the work of an entry in
// HandleEvent: the menus of a map object (TObjectContextMenu), of a creature
// in the battle list (TCreatureContextMenu), of a VIP (TBuddyContextMenu), of
// a console line (TTextContextMenu), of a line in the rule violation channel
// (TRequestContextMenu), of the console channel (TChannelContextMenu), of an
// edit line (TEditContextMenu), of an automap point (TMarkContextMenu) and of
// the skill window (TSkillContextMenu). Every click or Escape ends the
// action state, and Control.cpp deletes the menu.
//
// Names from the binary: the members LookedObject,
// LookedObjectX, UsedObjectX, UsedObjectY, LastEntry and the parameter names
// in the assertions and catch blocks (HelpText, Text, Shortcut, Event,
// Surface, Width, Height, x, y, Key, CreatureID, _CharacterID, _TextWindow,
// _LineNumber, _ConsoleWindow, _Edit, _Data, _SkillsWindow, TextBuffer). The
// class names (those of Control.cpp's earlier sketches) and the other names
// are ours; see notes/decisions.md, "ContextMenus.cpp".

#pragma once

#include "common.h"
#include "GUI.h"
#include "Map.h"     // TCoordinate, TMark
#include "Status.h"  // SetActionState
#include "Objects.h" // TObject
#include "types/FixedString.h"

class TConsoleWindow;    // ConsoleWindow.h
class TTextWindow;       // ConsoleWindow.h
class TEditLine;         // DialogElements.h
class TFormattedText;    // DialogElements.h
class TSkillsWindowBody; // ContentWindows.h
class TAutomapWindow;    // ContentWindows.h

// The help bubble: a formatted text in a frame of skins 52-60.
class THelpBubble : public TWindow {
public:
	THelpBubble(const char *HelpText); // 0x441d60, lines 106-121
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void SetSize(int Width, int Height) override; // +0x34, 0x43d730, line 142
	// Inline: emitted with the vtable after the constructor (0x4420f0, 0x442100).
	int getMinimumWidth() override { return SkinProperties().Data150; }    // +0x3c
	int getMinimumHeight() override { return SkinProperties().Data154; }   // +0x40
	void Arrange() override;                                               // +0x48, 0x43d9b0
	void Place(TWindow *Window);                                           // 0x442110, lines 257-272
	bool FitsIn(int &x, int &y, int &Width, int &Height, TWindow *Window); // 0x43dca0, lines 289-293

	TFormattedText *Text; // +0x2c
};

// One entry of a context menu.
class TContextMenuEntry final : public TElement {
public:
	TContextMenuEntry(const char *_Text, const char *_Shortcut, TCallback _Callback); // 0x43dfe0, line 316
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void Draw(int Surface, int x, int y) override;       // +0x44, 0x43e210, line 338
	bool LeftButtonUp(int x, int y, int Flags) override; // +0x54, 0x43e4d0, line 349
	void SetLast();                                      // 0x43e660, line 361

	TFixedString<50> Text;     // +0x24
	TFixedString<20> Shortcut; // +0x56
	TCallback Callback;        // +0x6c
};

// The base class of the menus.
class TContextMenu : public TWindow {
public:
	TContextMenu(); // 0x43e820, line 381
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Arrange() override;                              // +0x48, 0x43ea00
	bool LeftButtonUp(int x, int y, int Flags) override;  // +0x54, 0x43ecf0, line 437
	bool RightButtonUp(int x, int y, int Flags) override; // +0x5c, 0x43eea0, line 449
	void KeyPress(int Key) override;                      // +0x6c, 0x43f030, line 460
	// Inline: emitted with the vtable after the constructor (0x43e9f0). The
	// subclasses override it; an entry of the base class only closes the menu.
	void HandleEvent([[maybe_unused]] int Event) override { SetActionState(0); } // +0x70
	void Show(int x, int y, int ParentWidth, int ParentHeight);                  // 0x442de0, lines 486-498
	void AddEntry(int Event, const char *Text, const char *Shortcut);            // 0x442910, lines 506-521
	void AddSeparator();                                                         // 0x43f1d0, line 542

	int EntryWidth; // +0x2c, the widest entry
	bool Separator; // +0x30, the next entry starts a new group
	bool Empty;     // +0x31, no entry yet
};

// The menu of a map or inventory object: the looked-at object and the
// object to use (Status.cpp's GetTradeObject, GetUseObject) and the creature
// on the field (GetTradeInfo).
class TObjectContextMenu : public TContextMenu {
public:
	TObjectContextMenu(int x, int y, int ParentWidth, int ParentHeight); // 0x443130, lines 560-670
	void HandleEvent(int Event) override;                                // +0x70, 0x43f3a0, lines 712-780

	TObject LookedObject;  // +0x34
	int LookedObjectIndex; // +0x40, the position on the field or in the container
	int LookedObjectX;     // +0x44, 0xffff in a container or the inventory
	int LookedObjectY;     // +0x48, then 64 plus the container number
	int LookedObjectZ;     // +0x4c
	TObject UsedObject;    // +0x50
	int UsedObjectIndex;   // +0x5c
	int UsedObjectX;       // +0x60
	int UsedObjectY;       // +0x64
	int UsedObjectZ;       // +0x68
	uint32 CreatureID;     // +0x6c, 0 without a creature
};

// The menu of a creature in the battle list.
class TCreatureContextMenu : public TContextMenu {
public:
	TCreatureContextMenu(uint32 CreatureID, int x, int y, int ParentWidth, int ParentHeight); // 0x443960, lines 845-862
	void HandleEvent(int Event) override;                                                     // +0x70, 0x43fed0, lines 934-937

	uint32 CreatureID; // +0x34
};

// The menu of the VIP list, on a VIP or on the free space.
class TBuddyContextMenu : public TContextMenu {
public:
	TBuddyContextMenu(uint32 _CharacterID, int x, int y, int ParentWidth, int ParentHeight); // 0x443e20, line 987
	void HandleEvent(int Event) override;                                                    // +0x70, 0x440460, lines 1040-1043

	uint32 CharacterID; // +0x34
};

// The menu of a line in a console channel.
class TTextContextMenu : public TContextMenu {
public:
	TTextContextMenu(TTextWindow *_TextWindow, int _LineNumber, int x, int y, int ParentWidth,
		int ParentHeight);                // 0x444160, lines 1056-1120
	void HandleEvent(int Event) override; // +0x70, 0x4408f0, line 1173

	TTextWindow *TextWindow; // +0x34
	int LineNumber;          // +0x38
};

// The menu of a request in the rule violation channel.
class TRequestContextMenu : public TContextMenu {
public:
	TRequestContextMenu(TConsoleWindow *_ConsoleWindow, int _LineNumber, int x, int y, int ParentWidth,
		int ParentHeight);                // 0x444720, lines 1181-1203
	void HandleEvent(int Event) override; // +0x70, 0x440c60, lines 1221-1241

	TConsoleWindow *ConsoleWindow; // +0x34
	int LineNumber;                // +0x38
};

// The menu of the console's current channel (its tab).
class TChannelContextMenu : public TContextMenu {
public:
	TChannelContextMenu(TConsoleWindow *_ConsoleWindow, int x, int y, int ParentWidth, int ParentHeight); // 0x444a70, lines 1248-1275
	void HandleEvent(int Event) override;                                                                 // +0x70, 0x440fe0, lines 1307-1312

	TConsoleWindow *ConsoleWindow; // +0x34
	bool ShowServerMessages;       // +0x38, the value "Show/Hide server messages" sets
};

// The menu of an edit line.
class TEditContextMenu : public TContextMenu {
public:
	TEditContextMenu(TEditLine *_Edit, int x, int y, int ParentWidth, int ParentHeight); // 0x444eb0, lines 1324-1349
	void HandleEvent(int Event) override;                                                // +0x70, 0x441370, lines 1363-1366

	TEditLine *Edit; // +0x34
};

// The point of the automap a mark menu is opened for (0x80 bytes). The
// automap view (ContentWindows.cpp, TAutomapView::RightButtonUp) makes it with new and the
// menu's constructor deletes it; its constructor (inlined there and in the
// menu's) clears the coordinate and the mark.
struct TMarkMenuData {
	TMarkMenuData()
	{
		Coordinate.x = 0;
		Coordinate.y = 0;
		Coordinate.z = 0;
	}

	TCoordinate Coordinate; // +0x00, the field
	TMark Mark;             // +0x0c, the mark there; x and y 0 without one
	TAutomapWindow *Window; // +0x7c, the automap window (not used here)
};

// The menu of a point of the automap.
class TMarkContextMenu : public TContextMenu {
public:
	TMarkContextMenu(TMarkMenuData *_Data, int x, int y, int ParentWidth, int ParentHeight); // 0x4451e0, lines 1379-1397
	void HandleEvent(int Event) override;                                                    // +0x70, 0x441660, lines 1426-1429

	TMarkMenuData Data; // +0x34
};

// The menu of the skill window: which bars it shows.
class TSkillContextMenu : public TContextMenu {
public:
	TSkillContextMenu(TSkillsWindowBody *_SkillsWindow, int x, int y, int ParentWidth, int ParentHeight); // 0x4454f0, lines 1442-1465
	~TSkillContextMenu() override;                                                                        // +0x00, 0x43d6f0 (deleting 0x43d710)
	void HandleEvent(int Event) override;                                                                 // +0x70, 0x441a00, lines 1525-1530

	TSkillsWindowBody *SkillsWindow; // +0x34, its ContextMenu points back
};

// The skill window's side of the link between it and its menu (issue #118), defined in ContentWindows.cpp where
// TSkillsWindowBody is complete, so that ContextMenus.cpp does not include ContentWindows.h (the content windows make
// the menus, the menus called back into them: the two modules were a cycle). The menu's constructor and destructor
// set and clear the window's ContextMenu with SetSkillsWindowMenu (the original's stores at +0x30), its HandleEvent
// lays the window out again with ArrangeSkillsWindow (the window's virtual Arrange, as the original's call).
void SetSkillsWindowMenu(TSkillsWindowBody *Window, TSkillContextMenu *Menu) noexcept;
void ArrangeSkillsWindow(TSkillsWindowBody *Window);
