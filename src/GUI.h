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
// SkinProperties (and its types) and the skin table, which the windows read: Skins.h's since issue #116.
#include "Skins.h" // IWYU pragma: export
#include "Status.h"
#include "Talk.h"
#include "platform/Window.h"
#include "types/Callback.h" // IWYU pragma: export (TCallback, GUI.h's until issue #110)
#include "types/FixedString.h"
#include <array>
#include <flat_map>
#include <memory>
#include <vector>

void DrawSkin(int Surface, int x, int y, int width, int height, int skin, int srcx, int srcy); // 0x497a00
void DrawSkinClipped(int Surface, int x, int y, int width, int height, int skin, int ClipX, int ClipY,
	int ClipWidth, int ClipHeight, int skinoffsetx, int skinoffsety); // 0x4a1690

// --- Window classes -------------------------------------------------------

class TComponent;

// TCallback: types/Callback.h (issue #110), exported above.

// One entry of a component's drawing list (0x1c bytes): a skin drawn at an
// offset in the component, tiled to width x height when they are not 0. The
// constructors are inlined: new TDrawing[8] zeroes the entries (MapWindow.cpp
// 0x4c1ab4), and the subclasses assign whole entries.
struct TDrawing {
	TDrawing() : skin(0), x(0), y(0), width(0), height(0), srcx(0), srcy(0) {}
	TDrawing(int _skin, int _x, int _y, int _width, int _height, int _srcx, int _srcy)
		: skin(_skin), x(_x), y(_y), width(_width), height(_height), srcx(_srcx), srcy(_srcy) {}
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
	virtual ~TComponent();                                                  // +0x00, 0x496040 (deleting 0x496520); noexcept (issue #108)
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

	// The work of the original's destructors that can fail or acts outside the component (issue #108): the Send
	// functions, the preferences, the hotkeys, the assertions, DeleteTextBuffer. Close does it, once, in the order the
	// original's destructors did (a class's own part first, then its bases', a window's children in the order of its
	// list, an owned part that is not a child where the original's destructor deleted it); the destructors free the
	// memory only and are noexcept. Each delete of a component closes it first (DeleteComponent, below; TWindow's
	// DeleteChild). An exception of a close passes to the caller as the original's destructor's did, and the component
	// is left allocated and marked closed (the original had freed it): see notes/ownership.md, section 7.
	void Close();

	[[nodiscard]] int getWidth() const { return Width; }   // "Child->getWidth()" (line 1725), inlined
	[[nodiscard]] int getHeight() const { return Height; } // "Child->getHeight()" (line 1726), inlined

	std::unique_ptr<TDrawing[]> DrawingList; // +0x04, allocated by the subclasses (issue #58: the list's only owner)
	int DrawingListLength;                   // +0x08

private:
	// Close has run (issue #108; not in the original's layout). Here, in the padding after DrawingListLength on x64,
	// so that no class's size or member offsets change (at the end the Itanium ABI would put derived members into its
	// tail padding, which the golden tests' fake elements do not mirror).
	bool Closed = false;

public:
	TComponent *Parent; // +0x0c
	TComponent *Next;   // +0x10, the next child of Parent
	int OffsetX;        // +0x14, relative to Parent
	int OffsetY;        // +0x18
	int Width;          // +0x1c
	int Height;         // +0x20

protected:
	// A class's own part of the close (issue #108), overridden by the classes whose destructors did such work; each
	// override ends with its base's, as a destructor ends with its base's. Not one of the original's virtual functions.
	virtual void OnClose() {}
	// The rest of a close after an exception of a class's own part: the original's destructor went on with its bases'
	// destructors while the exception passed, and a second exception there ended the program (std::terminate), which
	// noexcept keeps.
	template <class F> static void CloseWhileUnwinding(F &&Rest) noexcept { Rest(); }
	// A component of a class with a close of its own that is destroyed without it (a delete without Close: a missed
	// close, its work not done) writes a line to the developer log (error level).
	void CheckClosed(const char *Class) const noexcept;
};

// Closes the component and deletes it (issue #108): the original's `delete Component`. Nothing for NULL. When the
// close throws, the component is not deleted and the exception passes on.
void DeleteComponent(TComponent *Component);

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

protected:
	void OnClose() override; // the children, first to last (~TWindow's order; issue #108)
};

// --- Timers ---------------------------------------------------------------

// A countdown (vtable 0x5646e8, 0x14 bytes) registered in the timer list
// while it runs. UpdateTimers counts it down and sends its callback on every
// update, the last time when it reaches 0.
class TTimer {
public:
	TTimer(int Time, TCallback Callback); // 0x49b290, line 905; Time in tenths of a second
	virtual ~TTimer();                    // +0x00, inlined into the deleting destructor 0x4a1dc0
	[[nodiscard]] int GetTime() const;    // 0x496000, tenths of a second left
	[[nodiscard]] bool IsExpired() const; // 0x496020
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

// SkinProperties and its types are Skins.h's since issue #116 (TGraphics' TSkins); InitSkinProperties, which fills it,
// stays GUI.cpp's.

void InitSkinProperties(); // 0x498160

class TMainWindow;

namespace patella {

class TNetwork;       // Network.h
class TCommunication; // Communication.h

// The GUI's state (issue #107, M7; notes/decisions.md, "#104: TClient and the subsystem objects" and "#107"), the
// sixth of TClient's subsystems: Talk.cpp's TTalk, Help.cpp's THelp and Status.cpp's TInteraction, and the data of
// GUI.cpp (the timers and hotkeys), ContentWindows.cpp (the two mouse-over texts), Control.cpp (the window, the
// callbacks, the mouse's last move, the frame counter's switch) and MainWindow.cpp (ModalWindow, MainWindow and the
// callbacks), in the original's address order with the same initial values. GUI.cpp's timer and hotkey functions,
// Control.cpp's and MainWindow.cpp's functions (issue #118) are its members of the same names; the free functions
// forward to them. SkinProperties is TGraphics' since issue #116 (with Skins.cpp's table, TSkins: Skins.h).
//
// Control.cpp, the controller above the windows, is TGUI's since issue #118: the windows' calls into it (the engine
// buttons of the main window's options, MainWindow.cpp, and the graphics options' fullscreen switch, Dialogs.cpp) are
// calls of TGUI's members, and TGUI takes the earlier subsystems Control.cpp reads (TNetwork's reconnect, TCommunication's
// packet ring and outfit request) by constructor reference.
//
// MainWindow owns the main window (issue #108; raw until then): InitMainWindow makes it, ExitMainWindow closes and
// frees it, at the original's new and delete; CPlayerApp::FatalError abandons it before exit(1) (AbandonWindows), so
// TClient's static destruction never destroys it. ModalWindow keeps the original's raw pointer (below): Control.cpp
// makes each dialog into it and closes and deletes the one it ends (DeleteComponent). notes/decisions.md, "#108".
class TGUI {
public:
	TGUI(TNetwork &Network, TCommunication &Communication); // MainWindow.cpp (TMainWindow is complete there)
	TGUI(const TGUI &) = delete;
	TGUI &operator=(const TGUI &) = delete;
	TGUI(TGUI &&) = delete;
	TGUI &operator=(TGUI &&) = delete;
	~TGUI();

	// GUI.cpp's timers and hotkeys.
	void AddTimer(TTimer *Timer);
	void RemoveTimer(TTimer *Timer);
	void UpdateTimers();
	void InitHotkeys();
	static void ExitHotkeys(); // empty (as TCommunication::ExitCommunication, issue #105)
	void AddHotkey(int Key, TCallback Callback);
	void RemoveHotkey(int Key);
	bool CheckHotkey(int Key);

	// Control.cpp's (issue #118; Control.h has the free functions of the same names, which forward here). Their calls
	// into the other files (GUI.cpp's and MainWindow.cpp's included) are those files' free functions, which the golden
	// Control and Communication tests fake (commworld.cpp's WorldFakes); their calls of each other are member calls.
	void Resize(int Width, int Height);
	void CheckSurface(bool Force);
	static void ChangeEngine(int engine);
	void Redraw(bool Force);
	void EnterGame() const;
	void LeaveGame();
	void MouseMove(int x, int y);
	void LeftButtonDown(int x, int y, int Flags);
	void LeftButtonUp(int x, int y, int Flags);
	void LeftButtonDoubleClick(int x, int y, int Flags);
	void RightButtonDown(int x, int y, int Flags);
	void RightButtonUp(int x, int y, int Flags);
	void MouseWheel(int x, int y, int Direction);
	void KeyPress(int Key);
	[[nodiscard]] bool CloseRequest() const;
	void HandleEvents();
	static void LinkCopyWarning();
	void ConnectionEstablished();
	void ReceivePackets();
	void ConnectionClosed();
	void TimerEvent() const;
	void ProcessEvents();
	void InitWindow(platform::TWindowHandle Window, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode), void (*OpenURL)(const char *URL));
	static void InitControl(bool Gamemaster);
	static void ExitControl();
	[[nodiscard]] std::array<uint8, 10> GetLastPacketTypes() const;

	// MainWindow.cpp's (issue #118; MainWindow.h has the free functions).
	void InitMainWindow(int Width, int Height, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode),
		void (*OpenURL)(const char *URL));
	void ExitMainWindow();

	// The earlier subsystems Control.cpp reads (issue #118).
	TNetwork &Network;
	TCommunication &Communication;

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
	platform::TWindowHandle WindowHandle{};  // 0x5c453c
	bool ShowFPS{};                          // 0x5c4554, toggled by key 0x907

private:
	// GUI.cpp's (SkinProperties, 0x5ccfc8, is TGraphics' since issue #116: Skins.h).
	// 0x5ccf98 NumberOfHotkeys, the used entries of Hotkeys: Hotkeys.size() (issue #100).
	// 0x5ccfa4 (initialiser 0x559e30), the original's vector<TTimer *>(0, 0, 2, NULL): AddTimer takes the first
	// NULL entry from 0 or appends, RemoveTimer sets the entry to NULL, UpdateTimers goes through the entries in
	// order. Its min stays 0 and max + 1 is the size, so a std::vector (issue #100; notes/decisions.md, "#100").
	std::vector<TTimer *> Timers;
	// 0x5ce4f0 (initialiser 0x559e60), the original's vector<THotkey>(0, 50, 10) with NumberOfHotkeys: a lookup by
	// key, each key at most once (AddHotkey replaces its callback), the order never read (RemoveHotkey moves the
	// last entry into the gap). A std::flat_map from the key to the callback (issue #100).
	std::flat_map<int, TCallback> Hotkeys;
	int LastTime{};     // 0x5ce518, UpdateTimers' static
	bool LastTimeSet{}; // its guard (0x5ce51c)

public:
	// MainWindow.cpp's: the callbacks InitMainWindow stores, the modal window and the main window.
	void (*OpenURLCallback)(const char *URL){}; // 0x5d16a0
	void (*SetDisplayModeCallback)(int Mode){}; // 0x5d16a4
	void (*ExitProgramCallback)(){};            // 0x5d16a8
	// The dialog on top. Raw (one owner, but not a std::unique_ptr; issue #108): MouseMove's help bubble leaves it
	// pointing to the bubble it deleted when the new bubble or its Place throws (issue #36), which the golden Control
	// test compares ("deleted ..."), and an owner cannot hold a freed window. Never destroyed at exit (a raw pointer).
	TWindow *ModalWindow{};                  // 0x5d16ac
	std::unique_ptr<TMainWindow> MainWindow; // 0x5d16b0 (issue #108: its owner)

	// Issue #108: leaves the main window allocated, neither closed nor freed, and MainWindow empty: what the
	// original's global was at exit(1). Called by CPlayerApp::FatalError before it, so that TClient's static
	// destruction does not run the window's destructors (nor, through ~TWindow, close its children: the leave-channel
	// and close-container packets and the preferences the original never sent or wrote on a fatal error).
	void AbandonWindows() noexcept;
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
