// GUI.h: the window system's base classes, timers, hotkeys and the skin
// properties (c:\mud\tibiaplayer\GUI.h; GUI.cpp is .\GUI.cpp).
//
// Names from the binary: SkinProperties and the member paths in its
// assertions (PlayerStates.NumberOfIcons, BuddylistWindow.Logo,
// Buttons.Texture, CheckBox.Item, StatusWindow.HitpointsBar/ManaBar),
// DrawingList (and DrawingListLength, skin, srcx, srcy from an error text),
// Parent, FirstChild, Capture, getWidth, getHeight, OffsetX/OffsetY (from
// the parameters _OffsetX/_OffsetY), Width, Height and the parameter names.
// The class names and the other member names are ours; see
// notes/decisions.md "GUI.cpp".

#pragma once

#include "common.h"

// The graphics types TRect, TRGBColor and TFont are graphics.h's; the window
// classes and SkinProperties need them complete.
#include "graphics.h"
// TGUI (below) owns Talk.cpp's, Help.cpp's and Status.cpp's objects (issue #107).
#include "Help.h"
#include "Status.h"
#include "Talk.h"
#include "types/FixedString.h"
#include "types/IndexedVector.h"
#include <array>
#include <memory>

void DrawSkin(int Surface, int x, int y, int width, int height, int skin, int srcx, int srcy); // 0x497a00
void DrawSkinClipped(int Surface, int x, int y, int width, int height, int skin, int ClipX, int ClipY,
	int ClipWidth, int ClipHeight, int skinoffsetx, int skinoffsety); // 0x4a1690

// --- Window classes -------------------------------------------------------

class TComponent;

// A receiver and an event number (8 bytes, passed by value): Send() calls
// Receiver->HandleEvent(Event). Buttons, timers and hotkeys keep one.
struct TCallback {
	TCallback() : Receiver(nullptr), Event(0) {} // inlined (vector<THotkey>, TTimer)
	TCallback(TComponent *Receiver, int Event);  // 0x496130
	void Send() const;                           // 0x4a0f00, line 1931
	TComponent *Receiver;                        // +0x00
	int Event;                                   // +0x04
};

// One entry of a component's drawing list (0x1c bytes): a skin drawn at an
// offset in the component, tiled to width x height when they are not 0. The
// constructors are inlined: new TDrawing[8] zeroes the entries (MapWindow.cpp
// 0x4c1ab4), and the subclasses assign whole entries.
struct TDrawing {
	TDrawing() : skin(0), x(0), y(0), width(0), height(0), srcx(0), srcy(0) {}
	TDrawing(int skin, int x, int y, int width, int height, int srcx, int srcy)
		: skin(skin), x(x), y(y), width(width), height(height), srcx(srcx), srcy(srcy) {}
	int skin;   // +0x00
	int x;      // +0x04
	int y;      // +0x08
	int width;  // +0x0c
	int height; // +0x10
	int srcx;   // +0x14
	int srcy;   // +0x18
};

// The abstract base class of every window element (vtable 0x5644a0). Slots
// with _purecall in the binary are pure; the child functions throw "not
// possible" (only TWindow has children).
class TComponent {
public:
	TComponent(const TComponent &) = delete;
	TComponent &operator=(const TComponent &) = delete;
	// Inlined into the constructors of TWindow and TElement.
	TComponent() : DrawingList(nullptr), DrawingListLength(0), Parent(nullptr), Next(nullptr), OffsetX(0),
				   OffsetY(0), Width(0), Height(0) {}
	virtual ~TComponent() noexcept(false);                                  // +0x00, 0x496040 (deleting 0x496520)
	virtual void InsertChild(TComponent *Child);                            // +0x04, 0x49b450, line 993
	virtual void AppendChild(TComponent *Child);                            // +0x08, 0x49b570, line 1001
	virtual void RemoveChild(TComponent *Child);                            // +0x0c, 0x49b690, line 1009
	virtual void DeleteChild(TComponent *Child);                            // +0x10, 0x49b7b0, line 1017
	virtual void InsertChildAfter(TComponent *Previous, TComponent *Child); // +0x14, 0x49b8d0, line 1025
	virtual void SwapChildren(TComponent *Child1, TComponent *Child2);      // +0x18, 0x49b9f0, line 1041
	virtual bool IsChild(TComponent *Child);                                // +0x1c, 0x49bb10, line 1049
	// GUI.h lines 681 and 682 (0x417de0, 0x417f00, kept in an earlier object).
	virtual TComponent *GetFirstChild() { TERROR("not possible"); }  // +0x20
	virtual TComponent *GetLastChild() { TERROR("not possible"); }   // +0x24
	virtual TComponent *GetNext() { return Next; }                   // +0x28, 0x417680
	virtual TComponent *GetPrevious();                               // +0x2c, 0x49bc30, lines 1057-1066
	virtual TComponent *GetParent() { return Parent; }               // +0x30, 0x417690
	virtual void SetSize(int _Width, int _Height);                   // +0x34, 0x49bee0, lines 1092-1093
	virtual void SetPosition(int _OffsetX, int _OffsetY);            // +0x38, 0x496060
	virtual int getMinimumWidth() { return 0; }                      // +0x3c, 0x417a20
	virtual int getMinimumHeight() { return 0; }                     // +0x40, 0x417a20
	virtual void Draw(int Surface, int x, int y);                    // +0x44, 0x4a1fb0, lines 1118-1163
	virtual void Arrange() {}                                        // +0x48, empty (0x496160); rebuilds the drawing list in subclasses
	virtual bool MouseMove(int x, int y) = 0;                        // +0x4c
	virtual bool LeftButtonDown(int x, int y, int Flags) = 0;        // +0x50
	virtual bool LeftButtonUp(int x, int y, int Flags) = 0;          // +0x54, Flags 0x200: both buttons
	virtual bool LeftButtonDoubleClick(int x, int y, int Flags) = 0; // +0x58
	virtual bool RightButtonUp(int x, int y, int Flags) = 0;         // +0x5c
	virtual bool MouseWheel(int x, int y, int Direction) = 0;        // +0x60, 1 up, 2 down, 6 left button held
	virtual void MouseHover(int x, int y) = 0;                       // +0x64
	virtual bool HelpMouseMove(int x, int y) = 0;                    // +0x68, action state 9
	virtual void KeyPress([[maybe_unused]] int Key) {}               // +0x6c, empty (0x4eac00)
	virtual void HandleEvent([[maybe_unused]] int Event) {}          // +0x70, empty (0x4eac00); see TCallback
	virtual void PositionChanged() {}                                // +0x74, empty (0x496160); the parent moved
	virtual void SetCapture(TComponent *Component) = 0;              // +0x78
	virtual void ReleaseCapture(TComponent *Component) = 0;          // +0x7c
	virtual void StopDragging() = 0;                                 // +0x80

	int getWidth() const { return Width; }   // "Child->getWidth()" (line 1725), inlined
	int getHeight() const { return Height; } // "Child->getHeight()" (line 1726), inlined

	std::unique_ptr<TDrawing[]> DrawingList; // +0x04, allocated by the subclasses (issue #58: the list's only owner)
	int DrawingListLength;                   // +0x08
	TComponent *Parent;                      // +0x0c
	TComponent *Next;                        // +0x10, the next child of Parent
	int OffsetX;                             // +0x14, relative to Parent
	int OffsetY;                             // +0x18
	int Width;                               // +0x1c
	int Height;                              // +0x20
};

// A component without children (vtable 0x5645c8, 0x24 bytes). It takes
// every mouse event; a double click is a second button press. The inline
// slots are kept in earlier objects (ConsoleWindow.obj and the
// Communication/ConsoleWindow gap).
class TElement : public TComponent {
public:
	TElement(); // 0x496100
	// The destructor is the implicit one (deleting destructor 0x496550).
	bool MouseMove([[maybe_unused]] int x, [[maybe_unused]] int y) override { return true; }                                  // +0x4c, 0x4259f0
	bool LeftButtonDown([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return true; } // +0x50, 0x425a00
	bool LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return true; }   // +0x54, 0x425a00
	bool LeftButtonDoubleClick(int x, int y, int Flags) override { return LeftButtonDown(x, y, Flags); }                      // +0x58, 0x41acb0
	bool RightButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return true; }  // +0x5c, 0x425a00
	bool MouseWheel([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Direction) override { return true; } // +0x60, 0x425a00
	void MouseHover([[maybe_unused]] int x, [[maybe_unused]] int y) override {}                                               // +0x64, 0x417a30
	bool HelpMouseMove(int x, int y) override;                                                                                // +0x68, 0x417a40 (inline, below)
	void SetCapture(TComponent *Component) override;                                                                          // +0x78, 0x4a0bc0, line 1893
	void ReleaseCapture(TComponent *Component) override;                                                                      // +0x7c, 0x4a0d60, line 1905
	void StopDragging() override {}                                                                                           // +0x80, empty (0x496160)
};

// A component with children (vtable 0x564528, 0x2c bytes); the base of the
// main window, the dialogs and the context menus. Children are a singly
// linked list through Next; Capture is the child that gets every mouse
// event while it is set (a drag).
class TWindow : public TComponent {
public:
	TWindow(const TWindow &) = delete;
	TWindow &operator=(const TWindow &) = delete;
	TWindow();                                                               // 0x496080
	~TWindow() override;                                                     // +0x00, 0x4a2590 (deleting 0x4a29e0)
	void InsertChild(TComponent *Child) override;                            // +0x04, 0x49c190, lines 1193-1194
	void AppendChild(TComponent *Child) override;                            // +0x08, 0x49c430, lines 1206-1207
	void RemoveChild(TComponent *Child) override;                            // +0x0c, 0x49c890, lines 1239-1260
	void DeleteChild(TComponent *Child) override;                            // +0x10, 0x49c6f0, line 1231
	void InsertChildAfter(TComponent *Previous, TComponent *Child) override; // +0x14, 0x49cd00, lines 1269-1287
	void SwapChildren(TComponent *Child1, TComponent *Child2) override;      // +0x18, 0x49d200, lines 1295-1347
	bool IsChild(TComponent *Child) override;                                // +0x1c, 0x4960b0
	TComponent *GetFirstChild() override { return FirstChild; }              // +0x20, 0x4176a0
	TComponent *GetLastChild() override;                                     // +0x24, 0x4960e0
	void SetPosition(int _OffsetX, int _OffsetY) override;                   // +0x38, 0x49da60, line 1389
	void Draw(int Surface, int x, int y) override;                           // +0x44, 0x4a2600, line 1408
	void Arrange() override;                                                 // +0x48, 0x49dc20, line 1424
	bool MouseMove(int x, int y) override;                                   // +0x4c, 0x49ddc0, line 1448
	bool LeftButtonDown(int x, int y, int Flags) override;                   // +0x50, 0x49dff0, line 1472
	bool LeftButtonUp(int x, int y, int Flags) override;                     // +0x54, 0x49e230, line 1496
	bool LeftButtonDoubleClick(int x, int y, int Flags) override;            // +0x58, 0x49e470, line 1520
	bool RightButtonUp(int x, int y, int Flags) override;                    // +0x5c, 0x49e6b0, line 1544
	bool MouseWheel(int x, int y, int Direction) override;                   // +0x60, 0x49e8f0, line 1568
	void MouseHover(int x, int y) override;                                  // +0x64, 0x49eb30, line 1588
	bool HelpMouseMove(int x, int y) override;                               // +0x68, 0x49ed10, line 1611
	void SetCapture(TComponent *Component) override;                         // +0x78, 0x49ef10, lines 1633-1638
	void ReleaseCapture(TComponent *Component) override;                     // +0x7c, 0x49f1a0, lines 1648-1653
	void StopDragging() override;                                            // +0x80, 0x49f430, line 1668
	virtual TComponent *GetCapture() { return Capture; }                     // +0x84, 0x4176b0
	virtual void SetChildPosition(TComponent *Child, int x, int y);          // +0x88, 0x49fa40, lines 1700-1708
	// The area is a 16-byte block passed by value (every caller builds it
	// with sub esp,0x10 and four stores, issue #27).
	virtual void SetChildArea(TComponent *Child, TRect Area);                   // +0x8c, 0x49f5d0, lines 1679-1690
	virtual void Fit(bool Notify) { FitToChildren(TRect(0, 0, 0, 0), Notify); } // +0x90, 0x4176c0
	// The margins are one 16-byte block passed by value (the callers, Fit and
	// ContextMenus.cpp, build it with sub esp,0x10): a TRect whose x, y,
	// Width and Height are the left, top, right and bottom margins.
	virtual void FitToChildren(TRect Margin, bool Notify); // +0x94, 0x4a0300, lines 1772-1794
	// The area is a 16-byte block passed by value as well (0x4882c0).
	virtual void FitToChild(TComponent *Child, TRect Area, bool Notify); // +0x98, 0x49fe70, lines 1724-1738
	virtual void ClipChildren(bool Strict);                              // +0x9c, 0x4a0920, line 1860

	TComponent *FirstChild; // +0x24
	TComponent *Capture;    // +0x28
};

// --- Timers ---------------------------------------------------------------

// A countdown (vtable 0x5646e8, 0x14 bytes) registered in the timer list
// while it runs. UpdateTimers counts it down and sends its callback on every
// update, the last time when it reaches 0.
class TTimer {
public:
	TTimer(int Time, TCallback Callback); // 0x49b290, line 905; Time in tenths of a second
	virtual ~TTimer();                    // +0x00, inlined into the deleting destructor 0x4a1dc0
	int GetTime() const;                  // 0x496000, tenths of a second left
	bool IsExpired() const;               // 0x496020
	void Update(int elapsed);             // 0x4a1df0, line 947
	int Remaining;                        // +0x04, milliseconds
	bool Active;                          // +0x08
	TCallback Callback;                   // +0x0c
};

void AddTimer(TTimer *Timer);    // 0x49af20, line 851
void RemoveTimer(TTimer *Timer); // 0x49b0d0, line 868
void UpdateTimers();             // 0x4a27c0, line 887

// --- Hotkeys --------------------------------------------------------------

struct THotkey {        // 12 bytes
	int Key;            // +0x00, with the modifier flags (0x200 is dropped by CheckHotkey)
	TCallback Callback; // +0x04
};

void InitHotkeys();                          // 0x496150
void ExitHotkeys();                          // empty, folded into 0x496160
void AddHotkey(int Key, TCallback Callback); // 0x4a10a0, line 1965
void RemoveHotkey(int Key);                  // 0x4a12a0, line 1983
bool CheckHotkey(int Key);                   // 0x4a1480, line 2004

// --- Skin properties ------------------------------------------------------

struct TGUIPoint {
	TGUIPoint() : x(0), y(0) {}
	int x, y;
};

// SkinProperties (0x5ccfc8, 0x1528 bytes): positions, sizes, colours and
// fonts of the window graphics, filled in by InitSkinProperties (0x498160).
// Its constructor (0x496800) and those of the blocks below (the gap
// 0x495480-0x4964ff) are compiler-generated: they construct the members
// that have constructors (TGUIPoint, TRGBColor, TRect, TFont) and leave the
// ints alone. Names from the assertions where there are any; the other
// members are named by their offset. The block boundaries are those of the
// out-of-line block constructors; blocks whose constructor was inlined are
// not visible and their members are direct members here (for example
// those of the inventory, which ContentWindows.cpp reads as arrays). The member types of the
// zero-initialised members (TGUIPoint, TRGBColor or TRect) are guesses from
// the values InitSkinProperties stores.

// GUIStyle+0xd8 (0x5cd0a0); compiler-generated constructor 0x495480. The
// start window's buttons (MainWindow.cpp): the size of their panel, its
// position and the positions of "Enter Game", "Options", "Info", "Exit
// Game" and "Access Account" in it (the constructor zeroes +0x08-+0x47,
// which leaves the types open; MainWindow.cpp reads +0x08-+0x37 as points,
// +0x38-+0x47 is not used).
struct TSkinProperties0D8 {
	int Data00;       // +0x00, panel width
	int Data04;       // +0x04, panel height
	TGUIPoint Data08; // +0x08, panel position (SetChildPosition)
	TGUIPoint Data10; // +0x10, "Enter Game"
	TGUIPoint Data18; // +0x18, "Options"
	TGUIPoint Data20; // +0x20, "Info"
	TGUIPoint Data28; // +0x28, "Exit Game"
	TGUIPoint Data30; // +0x30, "Access Account"
	TRect Data38;     // +0x38, zeroed: a type with a constructor (a guess)
	TFont Data48;     // +0x48
};

// GUIStyle+0x194 (0x5cd15c); compiler-generated constructor 0x4954f0.
struct TSkinProperties194 {
	TRect Data00;     // +0x00
	TGUIPoint Data10; // +0x10
	TGUIPoint Data18; // +0x18
	TGUIPoint Data20; // +0x20
	TRect Data28;     // +0x28, the title's area (TSubWindow::SetTitle, DialogWindow.cpp)
	TFont Data38;     // +0x38
	int Data4C;       // +0x4c
	TRect Data50;     // +0x50, the area passed to FitToChild (0x4882c0)
	TRect Data60;     // +0x60, the area of the first resizer (TSubWindow::SetSize)
	TRect Data70;     // +0x70, the area of the second resizer
	TRGBColor Data80; // +0x80
};

// GUIStyle+0x288 (0x5cd250); compiler-generated constructor 0x496180.
struct TSkinProperties288 {
	int Data00;                       // +0x00
	int Data04;                       // +0x04
	TGUIPoint Data08;                 // +0x08
	std::array<TGUIPoint, 10> Data10; // +0x10, the objects of the ten body slots (ContentWindows.cpp)
	std::array<TRect, 10> Data60;     // +0x60, the ten slot areas
	std::array<int, 10> Data100;      // +0x100, the skins of the empty slots
	TRect Data128;                    // +0x128
	TRect Data138;                    // +0x138
	TRect Data148;                    // +0x148
};

// GUIStyle+0x3e0, +0x428 (0x5cd3a8, 0x5cd3f0); compiler-generated constructor 0x495590.
struct TSkinProperties3E0 {
	TRect Data00; // +0x00
	TFont Data10; // +0x10
	TRect Data24; // +0x24
	TFont Data34; // +0x34
};

// GUIStyle+0x5a0 (0x5cd568); compiler-generated constructor 0x495610.
struct TStatusWindowProperties {
	int Data00;         // +0x00
	TRect Data04;       // +0x04
	TRect HitpointsBar; // +0x14
	TRect ManaBar;      // +0x24
	TRect Data34;       // +0x34
	TRect Data44;       // +0x44
	TFont Data54;       // +0x54
};

// GUIStyle+0x688 (0x5cd650); compiler-generated constructor 0x495680.
struct TSkinProperties688 {
	int Data00;       // +0x00
	TRect Data04;     // +0x04
	TRect Data14;     // +0x14
	int Data24;       // +0x24
	TRect Data28;     // +0x28
	TRect Data38;     // +0x38
	TRGBColor Data48; // +0x48
	TFont Data54;     // +0x54
	TRGBColor Data68; // +0x68
};

// GUIStyle+0x6fc (0x5cd6c4); compiler-generated constructor 0x495700.
struct TSkinProperties6FC {
	int Data00;   // +0x00
	TRect Data04; // +0x04, the names' area in the trade window's body (TTradeWindowBody::SetSize, ContentWindows.cpp)
	TRect Data14; // +0x14
	TRect Data24; // +0x24
	TRect Data34; // +0x34, the area of the names' separator (TTradeNames::SetSize)
	TFont Data44; // +0x44
	TRect Data58; // +0x58, the offers' area in the body
	TRect Data68; // +0x68
	TRect Data78; // +0x78
	TRect Data88; // +0x88
	int Data98;   // +0x98
	int Data9C;   // +0x9c
	int DataA0;   // +0xa0
	int DataA4;   // +0xa4
	TRect DataA8; // +0xa8, the area of the offers' separator (TTradeOffers::SetSize)
	TRect DataB8; // +0xb8, the buttons' area in the body
	TRect DataC8; // +0xc8
	TRect DataD8; // +0xd8
	TRect DataE8; // +0xe8, the area of the buttons' separator (TTradeButtons::SetSize)
	TFont DataF8; // +0xf8
};

// GUIStyle+0x808 (0x5cd7d0); compiler-generated constructor 0x495870.
struct TBuddylistWindowProperties {
	int Data00;       // +0x00
	int Data04;       // +0x04
	TRect Logo;       // +0x08
	TRect Data18;     // +0x18
	TFont Data28;     // +0x28
	TRGBColor Data3C; // +0x3c, an online buddy
	TRGBColor Data48; // +0x48, an offline buddy
	TRGBColor Data54; // +0x54, a buddy who just came online
};

// GUIStyle+0x88c (0x5cd854); compiler-generated constructor 0x4958e0.
struct TButtonsProperties {
	TFont Data00;      // +0x00
	TFont Data14;      // +0x14
	int Data28;        // +0x28
	TGUIPoint Texture; // +0x2c
};

// GUIStyle+0x8c4 (0x5cd88c); compiler-generated constructor 0x495950.
struct TSkinProperties8C4 {
	TFont Data00; // +0x00
	TFont Data14; // +0x14
	TFont Data28; // +0x28
};

// GUIStyle+0x900 (0x5cd8c8); compiler-generated constructor 0x4959e0.
struct TCheckBoxProperties {
	TGUIPoint Item; // +0x00
	TRect Data08;   // +0x08, the text's area (DialogElements.cpp's TCheckBox)
	TFont Data18;   // +0x18
	TFont Data2C;   // +0x2c
};

// GUIStyle+0x944 (0x5cd90c); compiler-generated constructor 0x495a60.
struct TSkinProperties944 {
	TRGBColor Data00; // +0x00
	TRGBColor Data0C; // +0x0c
	TRGBColor Data18; // +0x18
	TFont Data24;     // +0x24
	TFont Data38;     // +0x38
	TRGBColor Data4C; // +0x4c
};

// GUIStyle+0x9a0 (0x5cd968); compiler-generated constructor 0x495af0.
struct TSkinProperties9A0 {
	TRGBColor Data00; // +0x00
	TFont Data0C;     // +0x0c
	int Data20;       // +0x20
	TRGBColor Data24; // +0x24
	TFont Data30;     // +0x30
};

// GUIStyle+0x9e8 (0x5cd9b0); compiler-generated constructor 0x495b70 (it
// zeroes +0x04-+0xa7, so the types are open; these are ConsoleWindow.cpp's).
// The console: its minimum height, the areas of the tab bar, the edit line
// and the text, the button positions, the text window's area in its TConsole,
// the selection colour and the font of the channels other than the default.
struct TSkinProperties9E8 {
	int Data00;       // +0x00, the console's minimum height
	TRect Data04;     // +0x04, the tab bar (SetChildArea)
	TRect Data14;     // +0x14, not used by ConsoleWindow.cpp
	TRect Data24;     // +0x24, the edit line
	TRect Data34;     // +0x34, not used by ConsoleWindow.cpp
	TRect Data44;     // +0x44, the current channel (TConsole)
	TGUIPoint Data54; // +0x54, the button "Move channels to the right"
	TGUIPoint Data5C; // +0x5c, "Move channels to the left"
	TGUIPoint Data64; // +0x64, "Adjust volume"
	TGUIPoint Data6C; // +0x6c, "Close this Channel"
	TGUIPoint Data74; // +0x74, the server messages button
	TGUIPoint Data7C; // +0x7c, the ignore button
	TGUIPoint Data84; // +0x84, "Open new channel"
	TRect Data8C;     // +0x8c, the text window in its TConsole
	TRGBColor Data9C; // +0x9c, the selection
	TFont DataA8;     // +0xa8
};

// GUIStyle+0xaa4 (0x5cda6c); compiler-generated constructor 0x495c60.
struct TSkinPropertiesAA4 {
	int Data00;       // +0x00
	int Data04;       // +0x04
	TGUIPoint Data08; // +0x08
	int Data10;       // +0x10
	TFont Data14;     // +0x14
	TFont Data28;     // +0x28
	TFont Data3C;     // +0x3c
	TFont Data50;     // +0x50
};

// GUIStyle+0xb08 (0x5cdad0); compiler-generated constructor 0x4962c0.
struct TSkinPropertiesB08 {
	int Data00;                      // +0x00
	int Data04;                      // +0x04
	TRect Data08;                    // +0x08, the caption
	TFont Data18;                    // +0x18
	std::array<TGUIPoint, 3> Data2C; // +0x2c, the buttons from the right
	TRect Data44;                    // +0x44, the separator
	TRect Data54;                    // +0x54, the body (FitToChild takes it by value)
	TFont Data64;                    // +0x64
	TFont Data78;                    // +0x78
};

// GUIStyle+0xd3c (0x5cdd04); compiler-generated constructor 0x495d20.
struct TSkinPropertiesD3C {
	int Data00;       // +0x00
	int Data04;       // +0x04
	TRect Data08;     // +0x08
	TRect Data18;     // +0x18
	TRect Data28;     // +0x28
	TGUIPoint Data38; // +0x38
	TGUIPoint Data40; // +0x40
	TRect Data48;     // +0x48
	TRect Data58;     // +0x58
	TRect Data68;     // +0x68
	TRect Data78;     // +0x78
	TRGBColor Data88; // +0x88
	TRect Data94;     // +0x94
	TRect DataA4;     // +0xa4
	TRect DataB4;     // +0xb4
	TGUIPoint DataC4; // +0xc4
	TGUIPoint DataCC; // +0xcc
	TRect DataD4;     // +0xd4
	TRect DataE4;     // +0xe4
	TRect DataF4;     // +0xf4
};

// GUIStyle+0xe68 (0x5cde30); compiler-generated constructor 0x495e50.
struct TSkinPropertiesE68 {
	int Data00;   // +0x00
	int Data04;   // +0x04
	TFont Data08; // +0x08
	TFont Data1C; // +0x1c
	int Data30;   // +0x30
	TRect Data34; // +0x34
	TRect Data44; // +0x44
	TRect Data54; // +0x54
};

// GUIStyle+0xf8c (0x5cdf54); compiler-generated constructor 0x4963a0.
struct TSkinPropertiesF8C {
	int Data00;                       // +0x00
	int Data04;                       // +0x04
	TRect Data08;                     // +0x08
	TRect Data18;                     // +0x18
	TRect Data28;                     // +0x28
	std::array<TGUIPoint, 11> Data38; // +0x38
	TRect Data90;                     // +0x90
	TRect DataA0;                     // +0xa0
	TRect DataB0;                     // +0xb0
};

// GUIStyle+0x1084 (0x5ce04c); compiler-generated constructor 0x496430.
struct TSkinProperties1084 {
	int Data00;       // +0x00
	int Data04;       // +0x04
	TGUIPoint Data08; // +0x08
	TGUIPoint Data10; // +0x10
	TGUIPoint Data18; // +0x18
	TGUIPoint Data20; // +0x20
	TGUIPoint Data28; // +0x28
	TGUIPoint Data30; // +0x30
	TGUIPoint Data38; // +0x38
	TGUIPoint Data40; // +0x40
	TGUIPoint Data48; // +0x48
	TGUIPoint Data50; // +0x50
	TGUIPoint Data58; // +0x58
	TGUIPoint Data60; // +0x60
};

// GUIStyle+0x1114 (0x5ce0dc); compiler-generated constructor 0x495ee0.
struct TSkinProperties1114 {
	int Data00;   // +0x00
	int Data04;   // +0x04
	TRect Data08; // +0x08
	TRect Data18; // +0x18
	TRect Data28; // +0x28
	TRect Data38; // +0x38
	TRect Data48; // +0x48
	TRect Data58; // +0x58
	TRect Data68; // +0x68
	TRect Data78; // +0x78
	TRect Data88; // +0x88
	TRect Data98; // +0x98
	TRect DataA8; // +0xa8
};

// GUIStyle+0x1274 (0x5ce23c); compiler-generated constructor 0x4964a0.
struct TSkinProperties1274 {
	int Data00;       // +0x00
	int Data04;       // +0x04
	TGUIPoint Data08; // +0x08
	TGUIPoint Data10; // +0x10
	TGUIPoint Data18; // +0x18
	TGUIPoint Data20; // +0x20
	TGUIPoint Data28; // +0x28
	TGUIPoint Data30; // +0x30
	TGUIPoint Data38; // +0x38
	TGUIPoint Data40; // +0x40
	TGUIPoint Data48; // +0x48
	TGUIPoint Data50; // +0x50
	TGUIPoint Data58; // +0x58
	TGUIPoint Data60; // +0x60
	TRect Data68;     // +0x68
};

// GUIStyle+0x1394 (0x5ce35c); compiler-generated constructor 0x495fa0.
struct TSkinProperties1394 {
	int Data00;       // +0x00
	int Data04;       // +0x04
	TGUIPoint Data08; // +0x08
	TGUIPoint Data10; // +0x10
	TGUIPoint Data18; // +0x18
	TGUIPoint Data20; // +0x20
	TRect Data28;     // +0x28
	TRect Data38;     // +0x38
	TRect Data48;     // +0x48
	TRect Data58;     // +0x58
	TGUIPoint Data68; // +0x68
	TGUIPoint Data70; // +0x70
};

// GUIStyle+0x470 (0x5cd438), PlayerStates in ContentWindows.cpp's assertion
// ("SkinProperties.PlayerStates.NumberOfIcons"); its constructor is inlined
// into SkinProperties' (the extent of the block is a guess: the members
// TPlayerStates reads). Data00 and Data10 hold the first icon's position in
// the large and the small row, Data24 the distance between the icons.
struct TPlayerStatesProperties {
	TRect Data00;      // +0x00
	TRect Data10;      // +0x10
	int NumberOfIcons; // +0x20
	int Data24;        // +0x24
};

// GUIStyle+0x4ac and +0x4fc (0x5cd474, 0x5cd4c4): the layout of the large and
// the small combat controls (ContentWindows.cpp copies a whole block by
// value): their height and width and the positions of the offensive,
// balanced, defensive, stand, chase, secure, stop, options and help buttons.
// The type and member names are ours.
struct TCombatControlsProperties {
	int Height;                       // +0x00
	int Width;                        // +0x04
	std::array<TGUIPoint, 9> Buttons; // +0x08
};

struct TSkinProperties {
	TGUIPoint CursorHotspot1;                   // +0x000
	TGUIPoint CursorHotspot2;                   // +0x008
	TGUIPoint CursorHotspot3;                   // +0x010
	TGUIPoint CursorHotspot4;                   // +0x018
	TGUIPoint CursorHotspot5;                   // +0x020
	TGUIPoint CursorHotspot6;                   // +0x028
	TFont TooltipFont;                          // +0x030
	int TooltipBorder;                          // +0x044
	TRGBColor TooltipBackground;                // +0x048
	TRGBColor TooltipFrame;                     // +0x054
	TRGBColor HighlightColor;                   // +0x060
	TRGBColor AttackColor;                      // +0x06c
	TRGBColor FollowColor;                      // +0x078
	TRGBColor HighlightedAttackColor;           // +0x084
	TRGBColor HighlightedFollowColor;           // +0x090
	int Data09C;                                // +0x09c, the gap between a menu entry's text and its shortcut (ContextMenus.cpp)
	TRect Data0A0;                              // +0x0a0, a context menu's margins around its entries (FitToChildren, ContextMenus.cpp)
	TFont Data0B0;                              // +0x0b0, the font of the menu entries (number and colour; ContextMenus.cpp)
	int Data0C4;                                // +0x0c4, the height of a menu separator
	TRGBColor Data0C8;                          // +0x0c8, the background of the menu entry under the mouse
	int Data0D4;                                // +0x0d4, the space around a menu entry's text
	TSkinProperties0D8 Style0D8;                // +0x0d8, 0x5c bytes
	int Data134;                                // +0x134, the border of the start window's frame
	int Data138;                                // +0x138, the height of MainWindow.cpp's splitter
	int Data13C;                                // +0x13c
	TRect Data140;                              // +0x140, the area of the map window's hint button (TMapWindow::Arrange)
	int Data150;                                // +0x150, the help bubble's minimum width and height (ContextMenus.cpp)
	int Data154;                                // +0x154
	int Data158;                                // +0x158, its maximum width and height
	int Data15C;                                // +0x15c
	TRect Data160;                              // +0x160, the margins around its text (one 16-byte block passed by value)
	int Data170;                                // +0x170, its sizes are Data174 modulo Data170
	int Data174;                                // +0x174
	int Data178;                                // +0x178, its distance to the help area and the window edges
	int Data17C;                                // +0x17c
	int Data180;                                // +0x180
	int Data184;                                // +0x184, the width of the side bar (MainWindow.cpp)
	TRGBColor Data188;                          // +0x188
	TSkinProperties194 Style194;                // +0x194, 0x8c bytes
	int Data220;                                // +0x220
	TRect Data224;                              // +0x224
	TRect Data234;                              // +0x234
	TRect Data244;                              // +0x244
	TRect Data254;                              // +0x254
	TRect Data264;                              // +0x264
	int Data274;                                // +0x274
	TRect Data278;                              // +0x278
	TSkinProperties288 Style288;                // +0x288, 0x158 bytes
	TSkinProperties3E0 Style3E0;                // +0x3e0, 0x48 bytes
	TSkinProperties3E0 Style428;                // +0x428, 0x48 bytes
	TPlayerStatesProperties PlayerStates;       // +0x470, 0x28 bytes
	TFont Data498;                              // +0x498
	TCombatControlsProperties Style4AC;         // +0x4ac, the large combat controls (ContentWindows.cpp)
	TCombatControlsProperties Style4FC;         // +0x4fc, the small ones
	int Data54C;                                // +0x54c
	TRect Data550;                              // +0x550
	TRect Data560;                              // +0x560
	TRect Data570;                              // +0x570
	TRect Data580;                              // +0x580
	TRect Data590;                              // +0x590
	TStatusWindowProperties StatusWindow;       // +0x5a0, 0x68 bytes
	int Data608;                                // +0x608
	TRect Data60C;                              // +0x60c
	TRect Data61C;                              // +0x61c
	int Data62C;                                // +0x62c
	TGUIPoint Data630;                          // +0x630
	TRect Data638;                              // +0x638
	int Data648;                                // +0x648
	int Data64C;                                // +0x64c
	int Data650;                                // +0x650
	TFont Data654;                              // +0x654
	int Data668;                                // +0x668
	int Data66C;                                // +0x66c
	TRGBColor Data670;                          // +0x670, the bars of the first seven skill lines (ContentWindows.cpp)
	TRGBColor Data67C;                          // +0x67c, the bars of the skills
	TSkinProperties688 Style688;                // +0x688, 0x74 bytes
	TSkinProperties6FC Style6FC;                // +0x6fc, 0x10c bytes
	TBuddylistWindowProperties BuddylistWindow; // +0x808, 0x60 bytes
	int Data868;                                // +0x868
	int Data86C;                                // +0x86c
	int Data870;                                // +0x870
	int Data874;                                // +0x874
	int Data878;                                // +0x878
	int Data87C;                                // +0x87c
	int Data880;                                // +0x880
	int Data884;                                // +0x884
	int Data888;                                // +0x888
	TButtonsProperties Buttons;                 // +0x88c, 0x34 bytes
	int Data8C0;                                // +0x8c0
	TSkinProperties8C4 Style8C4;                // +0x8c4, 0x3c bytes
	TCheckBoxProperties CheckBox;               // +0x900, 0x40 bytes
	int Data940;                                // +0x940
	TSkinProperties944 Style944;                // +0x944, 0x58 bytes
	int Data99C;                                // +0x99c
	TSkinProperties9A0 Style9A0;                // +0x9a0, 0x44 bytes
	int Data9E4;                                // +0x9e4
	TSkinProperties9E8 Style9E8;                // +0x9e8, 0xbc bytes
	TSkinPropertiesAA4 StyleAA4;                // +0xaa4, 0x64 bytes
	TSkinPropertiesB08 StyleB08;                // +0xb08, 0x8c bytes
	int DataB94;                                // +0xb94
	int DataB98;                                // +0xb98
	TRect DataB9C;                              // +0xb9c
	TRect DataBAC;                              // +0xbac, the login dialog's account number
	TRect DataBBC;                              // +0xbbc
	TRect DataBCC;                              // +0xbcc, its password
	TRect DataBDC;                              // +0xbdc, its link
	int DataBEC;                                // +0xbec
	int DataBF0;                                // +0xbf0
	int DataBF4;                                // +0xbf4
	TRect DataBF8;                              // +0xbf8, the character list's label
	TRect DataC08;                              // +0xc08, its list
	TRect DataC18;                              // +0xc18, its account status
	int DataC28;                                // +0xc28
	int DataC2C;                                // +0xc2c
	TRect DataC30;                              // +0xc30, the move dialog's scroll bar
	TRect DataC40;                              // +0xc40, its label
	TGUIPoint DataC50;                          // +0xc50
	int DataC58;                                // +0xc58
	int DataC5C;                                // +0xc5c
	TRect DataC60;                              // +0xc60
	int DataC70;                                // +0xc70
	TRGBColor DataC74;                          // +0xc74
	TGUIPoint DataC80;                          // +0xc80
	std::array<TGUIPoint, 4> DataC88;           // +0xc88, the outfit dialog's part buttons
	TGUIPoint DataCA8;                          // +0xca8
	int DataCB0;                                // +0xcb0
	int DataCB4;                                // +0xcb4
	TGUIPoint DataCB8;                          // +0xcb8
	int DataCC0;                                // +0xcc0
	int DataCC4;                                // +0xcc4
	TRect DataCC8;                              // +0xcc8
	TRGBColor DataCD8;                          // +0xcd8
	TRect DataCE4;                              // +0xce4, the text dialog's description
	TRect DataCF4;                              // +0xcf4, its text
	int DataD04;                                // +0xd04
	int DataD08;                                // +0xd08
	TRect DataD0C;                              // +0xd0c
	TRect DataD1C;                              // +0xd1c, the edit list's description
	TRect DataD2C;                              // +0xd2c, its text
	TSkinPropertiesD3C StyleD3C;                // +0xd3c, 0x104 bytes
	int DataE40;                                // +0xe40
	int DataE44;                                // +0xe44
	TRect DataE48;                              // +0xe48, the bug report's text
	TRect DataE58;                              // +0xe58, its label
	TSkinPropertiesE68 StyleE68;                // +0xe68, 0x64 bytes
	int DataECC;                                // +0xecc
	int DataED0;                                // +0xed0
	TRect DataED4;                              // +0xed4, the new channel dialog's labels and fields
	TRect DataEE4;                              // +0xee4
	TRect DataEF4;                              // +0xef4
	TRect DataF04;                              // +0xf04
	int DataF14;                                // +0xf14
	int DataF18;                                // +0xf18
	TRect DataF1C;                              // +0xf1c, the private channel dialog's name
	TRect DataF2C;                              // +0xf2c, its label
	int DataF3C;                                // +0xf3c
	int DataF40;                                // +0xf40
	TRect DataF44;                              // +0xf44, the report dialog's text
	TRect DataF54;                              // +0xf54, its label
	int DataF64;                                // +0xf64
	int DataF68;                                // +0xf68
	TRect DataF6C;                              // +0xf6c, the VIP dialog's name
	TRect DataF7C;                              // +0xf7c, its label
	TSkinPropertiesF8C StyleF8C;                // +0xf8c, 0xc0 bytes
	int Data104C;                               // +0x104c
	int Data1050;                               // +0x1050
	TRect Data1054;                             // +0x1054, the update dialog's label
	TRect Data1064;                             // +0x1064, zeroed, not used
	TRect Data1074;                             // +0x1074, its progress bar
	TSkinProperties1084 Style1084;              // +0x1084, 0x68 bytes
	int Data10EC;                               // +0x10ec
	int Data10F0;                               // +0x10f0
	TRect Data10F4;                             // +0x10f4, the hints dialog's label
	TRect Data1104;                             // +0x1104, its list
	TSkinProperties1114 Style1114;              // +0x1114, 0xb8 bytes
	int Data11CC;                               // +0x11cc
	int Data11D0;                               // +0x11d0
	TRect Data11D4;                             // +0x11d4, the mark dialog's labels and text
	TRect Data11E4;                             // +0x11e4
	TRect Data11F4;                             // +0x11f4
	std::array<TGUIPoint, 14> Data1204;         // +0x1204
	TSkinProperties1274 Style1274;              // +0x1274, 0x78 bytes
	int Data12EC;                               // +0x12ec
	int Data12F0;                               // +0x12f0
	TRect Data12F4;                             // +0x12f4, the general options' check boxes
	TRect Data1304;                             // +0x1304
	TRect Data1314;                             // +0x1314
	TRect Data1324;                             // +0x1324
	TRect Data1334;                             // +0x1334
	int Data1344;                               // +0x1344
	int Data1348;                               // +0x1348
	TRect Data134C;                             // +0x134c, the graphics options' controls
	TRect Data135C;                             // +0x135c
	TRect Data136C;                             // +0x136c
	TGUIPoint Data137C;                         // +0x137c
	TRect Data1384;                             // +0x1384
	TSkinProperties1394 Style1394;              // +0x1394, 0x78 bytes
	int Data140C;                               // +0x140c
	int Data1410;                               // +0x1410
	TRect Data1414;                             // +0x1414, the console options' check boxes
	TRect Data1424;                             // +0x1424
	TRect Data1434;                             // +0x1434
	TRect Data1444;                             // +0x1444
	TRect Data1454;                             // +0x1454
	int Data1464;                               // +0x1464
	int Data1468;                               // +0x1468
	TRect Data146C;                             // +0x146c, the network options' check box
	int Data147C;                               // +0x147c
	int Data1480;                               // +0x1480
	TRect Data1484;                             // +0x1484, the hotkey dialog's list
	TRect Data1494;                             // +0x1494, its labels and fields
	TRect Data14A4;                             // +0x14a4
	TRect Data14B4;                             // +0x14b4
	TRect Data14C4;                             // +0x14c4
	TRGBColor Data14D4;                         // +0x14d4, the lines of hotkeys sent automatically
	int Data14E0;                               // +0x14e0
	int Data14E4;                               // +0x14e4
	TRect Data14E8;                             // +0x14e8, the error report's labels and fields (set, unread since #74)
	TRect Data14F8;                             // +0x14f8
	TRect Data1508;                             // +0x1508
	TRect Data1518;                             // +0x1518
};

extern TSkinProperties SkinProperties; // 0x5ccfc8

void InitSkinProperties(); // 0x498160

class TMainWindow;

namespace patella {

// The GUI's state (issue #107, M7; notes/decisions.md, "#104: TClient and the subsystem objects" and "#107"), the
// sixth of TClient's subsystems: Talk.cpp's TTalk, Help.cpp's THelp and Status.cpp's TInteraction, and the data of
// GUI.cpp (the timers and hotkeys), ContentWindows.cpp (the two mouse-over texts), Control.cpp (the window, the
// callbacks, the mouse's last move, the frame counter's switch) and MainWindow.cpp (ModalWindow, MainWindow and the
// callbacks), in the original's address order with the same initial values. GUI.cpp's timer and hotkey functions are
// its members of the same names; the free functions forward to them. SkinProperties is still GUI.cpp's global (its
// 2,000 readers in the window files: notes/decisions.md, "#107", "Found, not done").
//
// ModalWindow and MainWindow keep the original's raw ownership: Control.cpp news the dialogs into ModalWindow and
// deletes the one it replaces, Player.cpp and Control.cpp the main window (notes/ownership.md, section 7). They are
// TGUI's members, no longer globals; #108 turns them into owners.
class TGUI {
public:
	TGUI() = default;
	TGUI(const TGUI &) = delete;
	TGUI &operator=(const TGUI &) = delete;
	TGUI(TGUI &&) = delete;
	TGUI &operator=(TGUI &&) = delete;
	~TGUI() = default;

	// GUI.cpp's timers and hotkeys.
	void AddTimer(TTimer *Timer);
	void RemoveTimer(TTimer *Timer);
	void UpdateTimers();
	void InitHotkeys();
	static void ExitHotkeys(); // empty (as TCommunication::ExitCommunication, issue #105)
	void AddHotkey(int Key, TCallback Callback);
	void RemoveHotkey(int Key);
	bool CheckHotkey(int Key);

	// The files' objects.
	TTalk Talk;               // Talk.cpp
	THelp Help;               // Help.cpp
	TInteraction Interaction; // Status.cpp's user interaction

	// Data, in the original's address order; public where another file or a golden test reads it (named).
	TFixedString<100> AutomapMouseOverText{}; // 0x5c4398, ContentWindows.cpp: a mark's text (TAutomapWindow::MouseHover)
	TFixedString<256> SkillMouseOverText{};   // 0x5c4400, ContentWindows.cpp: "You have %d percent to go"
	// Control.cpp's (0x5c4540-0x5c4553 is not used by any code); the golden Control test reads and sets them.
	int LastMouseMove{};                     // 0x5c4528, timer tick of the last mouse move, 0 after a click
	void (*URLCallback)(const char *URL){};  // 0x5c452c
	void (*DisplayModeCallback)(int Mode){}; // 0x5c4530
	void (*ExitCallback)(){};                // 0x5c4538, stored but not used
	HWND WindowHandle{};                     // 0x5c453c
	bool ShowFPS{};                          // 0x5c4554, toggled by key 0x907

private:
	// GUI.cpp's (SkinProperties, 0x5ccfc8, is still its global).
	int NumberOfHotkeys{};                             // 0x5ccf98, used entries of Hotkeys
	TIndexedVector<TTimer *> Timers{0, 0, 2, nullptr}; // 0x5ccfa4, initialiser 0x559e30
	TIndexedVector<THotkey> Hotkeys{0, 50, 10};        // 0x5ce4f0, initialiser 0x559e60
	int LastTime{};                                    // 0x5ce518, UpdateTimers' static
	bool LastTimeSet{};                                // its guard (0x5ce51c)

public:
	// MainWindow.cpp's: the callbacks InitMainWindow stores, the modal window and the main window.
	void (*OpenURLCallback)(const char *URL){}; // 0x5d16a0
	void (*SetDisplayModeCallback)(int Mode){}; // 0x5d16a4
	void (*ExitProgramCallback)(){};            // 0x5d16a8
	TWindow *ModalWindow{};                     // 0x5d16ac, the dialog on top (raw, as the original's; #108)
	TMainWindow *MainWindow{};                  // 0x5d16b0 (raw, as the original's; #108)
};

// The transition's access path (issue #107; notes/decisions.md, "#104: TClient and the subsystem objects"): the one
// TClient's TGUI (Client.cpp). It goes when no free function needs it.
namespace bridge {
TGUI &GUI();
} // namespace bridge

} // namespace patella


inline bool TElement::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(0);
	return false;
}
