// DialogWindow.h: the columns of windows on the right of the game window.
//
// TDialogWindow (vtable 0x563f80, 0x4c bytes) holds windows one above the
// other: MainWindow.cpp makes two, SidePanel for the inventory and the other
// side-bar windows and DialogWindow for the containers and the skill, battle,
// VIP and trade windows. A child can be dragged to another place in the
// column; Rearrange keeps the children in the column's height with one of
// six strategies. The free space below the last child is filled with skins.
//
// TContentWindow (vtable 0x563e38, 0x44 bytes) is the base of every window
// in a column: an identifier, an optional tiled background (skin 7) and the
// dragging. ContentWindows.cpp's side-bar windows derive from it directly.
// TSubWindow (vtable 0x563ed8, 0x70 bytes) is a window with a body (the
// content, made by ContentWindows.cpp), a frame of eight skins, a title, an
// icon, the buttons "up", "minimize" and "close" and optionally two
// resizers (DialogElements.cpp).
//
// Names from the binary: TDialogWindow (MainWindow.cpp's
// "DialogWindow!=NULL" and the file name), hasContentWindow (which gives the
// class name TContentWindow and GetContentWindow), getBody, Body, _Body,
// _Identifier, IsMinimized, Search, Work, DeltaHeight, Strategy and the
// parameter names in the catch blocks (Surface, Identifier, x, y, Width,
// Height, Event). The other names are ours (notes/decisions.md,
// "DialogWindow.cpp").

#pragma once

#include "GUI.h"
#include "graphics.h"

class TSubWindowBody; // ContentWindows.h
class TResizer;       // DialogElements.h
class TSkinButton;    // DialogElements.h

// A window in a TDialogWindow. Identifier: 1 and 2 the two inventory
// windows, 3 the automap, 4 the status bars, 5 the buttons, 6 skills, 7 battle, TRADE_WINDOW
// (8, MainWindow.h), 9 VIP, 0x40 plus the number of a container.
class TContentWindow : public TWindow {
public:
	TContentWindow(int _Identifier, bool _Background); // 0x4854c0
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override;         // +0x44, 0x487210, lines 681-692
	void Arrange() override;                               // +0x48, 0x4875a0, line 703
	bool MouseMove(int x, int y) override;                 // +0x4c, 0x4878d0, line 734
	bool LeftButtonDown(int x, int y, int Flags) override; // +0x50, 0x487ad0, line 751
	bool LeftButtonUp(int x, int y, int Flags) override;   // +0x54, 0x487ca0, line 768
	void ReleaseCapture(TComponent *Component) override;   // +0x7c, 0x487740, line 714

	int Identifier;  // +0x2c
	int BackgroundY; // +0x30, the background skin's row at the top edge
	bool Background; // +0x34, draw the tiled skin 7 behind the children
	bool Dragging;   // +0x35
	int DragX;       // +0x38, where the drag started (window coordinates)
	int DragY;       // +0x3c
	int DragBottom;  // +0x40, from the drag start to the bottom edge; set, never read
};

// A content window with a body and a frame (MainWindow.cpp's containers and
// the skill, battle, VIP and trade windows).
class TSubWindow : public TContentWindow {
public:
	TSubWindow(const TSubWindow &) = delete;
	TSubWindow &operator=(const TSubWindow &) = delete;
	TSubWindow(int _Identifier, TSubWindowBody *_Body, bool _Resizable, bool _Background); // 0x487e80, lines 791-804
	~TSubWindow() override;                                                                // +0x00, 0x485500 (deleting 0x485790)
	void SetSize(int Width, int Height) override;                                          // +0x34, 0x4881f0, line 861
	int getMinimumHeight() override;                                                       // +0x40, 0x485690
	void Arrange() override;                                                               // +0x48, 0x488510, lines 891-928
	bool MouseMove(int x, int y) override;                                                 // +0x4c, 0x488c20, line 972
	bool LeftButtonDown(int x, int y, int Flags) override;                                 // +0x50, 0x4889c0, line 950
	bool LeftButtonDoubleClick(int x, int y, int Flags) override;                          // +0x58, 0x488e80, line 986
	void HandleEvent(int Event) override;                                                  // +0x70, 0x489030, line 1002
	virtual void Minimize();                                                               // +0xa0, 0x489220, lines 1009-1020
	virtual void Maximize();                                                               // +0xa4, 0x489500, lines 1027-1037
	void ShowUpButton();                                                                   // 0x4897d0, line 1049
	void HideUpButton();                                                                   // 0x489970, line 1061
	void SetUpButton(TComponent *Button);                                                  // 0x489b10, line 1075
	void SetCloseButton(TComponent *Button);                                               // 0x489cf0, line 1088
	void SetMinimizeButton(TSkinButton *Button);                                           // 0x489ed0, line 1101
	void SetIcon(TComponent *_Icon);                                                       // 0x48a0b0, line 1114
	void SetTitle(TComponent *_Title);                                                     // 0x48a2a0, line 1127
	static int GetFrameHeight();                                                           // 0x485670, the title bar and the bottom edge
	// Inline ("Window->getBody()!=NULL", MainWindow.cpp).
	TSubWindowBody *getBody() const { return Body; }

	TSubWindowBody *Body;        // +0x44
	TResizer *Resizer1;          // +0x48, area Style194.Data60
	TResizer *Resizer2;          // +0x4c, area Style194.Data70
	TComponent *UpButton;        // +0x50, a container's "up" button, shown while it has a parent
	TComponent *CloseButton;     // +0x54, event 10
	TSkinButton *MinimizeButton; // +0x58, event 9
	TComponent *Icon;            // +0x5c
	TComponent *Title;           // +0x60
	bool IsMinimized;            // +0x64
	int Data68;                  // +0x68, cleared by the constructor, not used here
	int RestoreHeight;           // +0x6c, the height before Minimize
};

// A column of content windows.
class TDialogWindow : public TWindow {
public:
	TDialogWindow(); // 0x4856c0
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void InsertChild(TComponent *Child) override;                            // +0x04, 0x485d70, line 181
	void AppendChild(TComponent *Child) override;                            // +0x08, 0x485f10, line 193
	void DeleteChild(TComponent *Child) override;                            // +0x10, 0x4860b0, line 205
	void InsertChildAfter(TComponent *Previous, TComponent *Child) override; // +0x14, 0x486260, line 217
	void SetSize(int Width, int Height) override;                            // +0x34, 0x4857b0, line 63
	int getMinimumWidth() override;                                          // +0x3c, 0x4854b0
	int getMinimumHeight() override;                                         // +0x40, 0x485970, line 86
	void Draw(int Surface, int x, int y) override;                           // +0x44, 0x485b10, line 109
	void Arrange() override;                                                 // +0x48, 0x48a490, line 169
	// Strategy: 1 a child is dragged (Work, NULL when the drag ends), 2 a
	// child grew, 4 a child (Work) shrank, 8 the column's height changed, 16
	// a new child (Work) is appended, 32 a child was removed.
	virtual void Rearrange(int Strategy, TComponent *Work); // +0xa0, 0x48aea0, lines 271-297
	TContentWindow *GetContentWindow(int Identifier);       // 0x486400, line 233
	void StoreOpenDialogs();                                // 0x4865b0, line 247
	// Inline ("!DialogWindow->hasContentWindow(TRADE_WINDOW)", MainWindow.cpp).
	bool hasContentWindow(int Identifier) { return GetContentWindow(Identifier) != nullptr; }
	void PositionChildren(bool SkipDragged); // 0x486760, line 317
	void FitChildren();                      // 0x486930, line 353
	bool Shrink();                           // 0x48a990, lines 377-394
	void DragChild(TComponent *Work);        // 0x486b30, lines 419-429
	void MakeRoom(TComponent *Work);         // 0x486e20, line 477
	void Grow(TComponent *Work);             // 0x48acb0, line 505
	int GetFreeHeight();                     // 0x487060, line 524

	TRect Area;            // +0x2c, where the children go (x, y, width, height)
	TComponent *Dragged;   // +0x3c
	bool Busy;             // +0x40, Rearrange is running
	int FreeHeight;        // +0x44, GetFreeHeight's last result while UpdateFreeHeight
	bool UpdateFreeHeight; // +0x48
};
