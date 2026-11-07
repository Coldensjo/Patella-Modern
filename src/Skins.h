// Skins.cpp: rectangles of the GUI graphics (file name inferred), and SkinProperties, the window graphics' positions,
// sizes, colours and fonts (GUI.cpp's global 0x5ccfc8, which InitSkinProperties fills).
//
// TSkins (Patella-Modern issue #116, M7; notes/decisions.md, "#104: TClient and the subsystem objects" and "#116"):
// Skins.cpp's data and SkinProperties, a part of TGraphics (graphics.h), declared here and defined in Skins.cpp, as
// TSession is Status.cpp's inside TCommunication. SkinProperties' types moved here from GUI.h with it. Portable
// (patella_core): the bridge accessor and the original's free functions are the client's (graphics.cpp), since
// patella_core has no TClient.

#pragma once

#include "types/GraphicsTypes.h"
#include <array>
#include <memory>

// Number of skin entries; the terminator of the table in InitSkins.
inline constexpr int LASTSKIN = 262;

// One rectangle in the GUI picture. width and height are named in GUI.cpp's
// assertions; x and y are ours.
struct TSkin {
	int x;      // +0x0
	int y;      // +0x4
	int width;  // +0x8
	int height; // +0xc
};

// --- Skin properties (GUI.h's until issue #116) ---------------------------

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

namespace patella {

// Skins.cpp's data and GUI.cpp's SkinProperties (issue #116), TGraphics' member Skins. Public, as the globals were:
// the window files read them (through the accessors below), the golden tests set them (gui_tests.cpp constructs
// SkinProperties in place and compares it byte for byte; the window tests pass Skins to the original's side).
class TSkins {
public:
	TSkins() = default;
	TSkins(const TSkins &) = delete;
	TSkins &operator=(const TSkins &) = delete;
	TSkins(TSkins &&) = delete;
	TSkins &operator=(TSkins &&) = delete;
	~TSkins() = default;

	void InitSkins(); // 0x4e3cb0
	void ExitSkins(); // 0x4e6da0

	// Data, in .bss order (Skins.cpp's), then GUI.cpp's.
	int NumberOfSkins{};              // 0x71c450
	std::unique_ptr<TSkin[]> Skins;   // 0x71c454 (issue #58: the table's only owner, a pointer as the original's)
	TSkinProperties SkinProperties{}; // 0x5ccfc8, GUI.cpp's; filled by InitSkinProperties
};

// The transition's access path (notes/decisions.md, "#104: TClient and the subsystem objects"): the one TClient's
// TSkins (Client.cpp). It goes when no free function needs it.
namespace bridge {
TSkins &Skins();
} // namespace bridge

} // namespace patella

// The original's globals' names for their about 2,400 readers in the window files (issue #116): SkinProperties() and
// Skins() are the one TClient's, so a use reads `SkinProperties().TooltipBorder` where it read
// `SkinProperties.TooltipBorder`. Not the design's `bridge::Skins().SkinProperties` at each use: the same object,
// without rewriting every expression of the windows (notes/decisions.md, "#116").
inline TSkinProperties &SkinProperties()
{
	return patella::bridge::Skins().SkinProperties;
}

inline std::unique_ptr<TSkin[]> &Skins()
{
	return patella::bridge::Skins().Skins;
}

// The original's free functions (graphics.cpp's since issue #116: they call the bridge, which patella_core does not
// have), forwarding to TSkins' members.
void InitSkins();
void ExitSkins();
