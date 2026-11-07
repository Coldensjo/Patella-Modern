// DialogElements.h: the controls of the dialogs and windows
// (c:\mud\tibiaplayer\DialogElements.h; DialogElements.cpp is .\DialogElements.cpp).
//
// Separators, resizers, frames (TBorder), texts (TLabel, TFormattedText),
// links, icons, buttons of every kind, check boxes, edit lines (single line,
// password, multi-line) with their focus groups, scroll bars of both
// directions, list boxes and the progress bar.
//
// Names from the binary: SkinProperties.Buttons.Texture and CheckBox.Item,
// NumberOfSkins, getMinimumWidth/getMinimumHeight, getWidth/getHeight, Text,
// Cursor, MaxLength, FocusGroup, MemberList, Member, Active, Next, FieldWidth,
// Lines, RadioButtonGroup, ToResize, TokenType/TokenData, maximum/current, and
// the parameters (_ToResize, _Window, _Text, ButtonText, _Destination,
// _Outfit, _Direction, Mask, ColorData, _ButtonUp, _ButtonDown, _MaxLength,
// _FocusGroup, Position, insertlength, start, length, _Min, _Max, _Value,
// _ButtonRange, Number, Key, Code, Event, Surface, x, y, Width, Height).
// TCheckBox is named after SkinProperties.CheckBox. The other class, member
// and function names are ours; see notes/decisions.md, "DialogElements.cpp".
//
// Several header functions have out-of-line copies at the start of
// DialogElements.obj (0x4524e0-0x452cb0 and the destructors up to 0x4532ff),
// some only called by Dialogs.cpp.

#pragma once

#include "common.h"
#include "types/FixedString.h"
#include "types/IndexedVector.h"
#include "GUI.h"
#include "graphics.h"
#include "Skins.h"   // Skins
#include "Status.h"  // SetMouseOverText
#include "Objects.h" // TObject
#include <array>
#include <cstring>
#include <memory>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

// --- Separator, resizer and frame -----------------------------------------------

// A line between groups (vtable 0x5610b0, 0x28 bytes): skins 9 and 8 one above
// the other across the width, or 11 and 10 side by side down the height.
class TSeparator : public TElement {
public:
	// Inline (out-of-line copy 0x452cb0).
	TSeparator(bool _Horizontal)
	{
		Horizontal = _Horizontal;
		DrawingListLength = 2;
		DrawingList.reset(new TDrawing[2]);
	}
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void SetSize(int Width, int Height) override; // +0x34, 0x453930, line 42
	void Arrange() override;                      // +0x48, 0x453ad0

	bool Horizontal; // +0x24
};

// The resize handle of a window (vtable 0x5612b0, 0x38 bytes): dragging it
// changes ToResize's width and/or height by the mouse movement.
class TResizer : public TElement {
public:
	TResizer(TComponent *_ToResize, bool _ResizeWidth, bool _ResizeHeight, bool _Invisible); // 0x453be0, line 74
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void SetSize(int Width, int Height) override;          // +0x34, 0x453dc0, line 94
	void Arrange() override;                               // +0x48, 0x453f60
	bool MouseMove(int x, int y) override;                 // +0x4c, 0x453fe0, lines 113-122
	bool LeftButtonDown(int x, int y, int Flags) override; // +0x50, 0x4542a0, lines 129-137
	bool LeftButtonUp(int x, int y, int Flags) override;   // +0x54, 0x454530, lines 144-154
	void ReleaseCapture(TComponent *Component) override    // +0x7c, 0x453db0
	{
		Dragging = false;
		TElement::ReleaseCapture(Component);
	}

	TComponent *ToResize; // +0x24
	bool Dragging;        // +0x28
	int StartX;           // +0x2c, where the drag started
	int StartY;           // +0x30
	bool ResizeWidth;     // +0x34
	bool ResizeHeight;    // +0x35
	bool Invisible;       // +0x36, no skin
};

// A frame of nine skins (corners, edges and an optional background) around one
// window (vtable 0x561358, 0x68 bytes).
class TBorder : public TWindow {
public:
	TBorder(TComponent *_Window, int _Border); // 0x4547f0, lines 166-184, the skins 26-33
	TBorder(TComponent *_Window, int _TopLeft, int _Top, int _TopRight, int _Left, int _Right, int _BottomLeft,
		int _Bottom, int _BottomRight, int _Border); // 0x454b10, lines 192-210
	TBorder(TComponent *_Window, int _TopLeft, int _Top, int _TopRight, int _Left, int _Right, int _BottomLeft,
		int _Bottom, int _BottomRight, int _Background, int _Border); // 0x454e30, lines 217-235
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void SetSize(int Width, int Height) override;                                                         // +0x34, 0x455150, lines 242-251
	int getMinimumWidth() override { return Skins()[Right].width + Skins()[Left].width + 2 * Border; }    // +0x3c, 0x4525a0
	int getMinimumHeight() override { return Skins()[Bottom].height + Skins()[Top].height + 2 * Border; } // +0x40, 0x452570
	void Arrange() override;                                                                              // +0x48, 0x4556b0
	void FitToWindow();                                                                                   // 0x455500, line 262

	TComponent *Window; // +0x2c, a window or an element (the outfit dialog frames its icon)
	int TopLeft;        // +0x30, skins
	int Top;            // +0x34
	int TopRight;       // +0x38
	int Left;           // +0x3c
	int Right;          // +0x40
	int BottomLeft;     // +0x44
	int Bottom;         // +0x48
	int BottomRight;    // +0x4c
	int Background;     // +0x50, 0 for none
	TRect WindowArea;   // +0x54, Window's area, set by Arrange
	int Border;         // +0x64, space between the frame and the window
};

// --- Texts ------------------------------------------------------------------------

// A text of one or more lines (vtable 0x560bb8, 0x4c bytes). The constructors
// without an area size the label to the text. Mouse events go through it.
class TLabel : public TElement {
public:
	TLabel(const TLabel &) = delete;
	TLabel &operator=(const TLabel &) = delete;
	TLabel(int x, int y, int Width, int Height, TFont _Font, const char *_Text, bool Shorten); // 0x465510, lines 337-348
	TLabel(int x, int y, TFont _Font, const char *_Text);                                      // 0x465810, lines 355-369
	TLabel(TFont _Font, const char *_Text);                                                    // 0x465b40, lines 376-389
	// Inline (out-of-line copy 0x4525d0, deleting destructor 0x452d50).
	~TLabel() override
	{
		if (Text != nullptr)
			Text.reset();
		if (FullText != nullptr)
			FullText.reset();
		if (Line != nullptr)
			Line.reset();
	}
	void Draw(int Surface, int x, int y) override;                                                                             // +0x44, 0x455a50, line 413
	bool MouseMove([[maybe_unused]] int x, [[maybe_unused]] int y) override { return false; }                                  // +0x4c, 0x452620
	bool LeftButtonDown([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; } // +0x50, 0x457700 (folded)
	bool LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }   // +0x54, 0x457700
	bool RightButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }  // +0x5c, 0x457700
	void MouseHover(int x, int y) override;                                                                                    // +0x64, 0x455ca0, line 423
	virtual void SetText(const char *_Text, bool Shorten);                                                                     // +0x84, 0x455e40, lines 431-472

	std::unique_ptr<char[]> Text;     // +0x24, the lines, each terminated (issue #58)
	std::unique_ptr<char[]> FullText; // +0x28, the text as given (the tooltip of a shortened text; issue #58)
	TFont Font;                       // +0x2c
	int Lines;                        // +0x40
	std::unique_ptr<char *[]> Line;   // +0x44, the start of each line in Text (owns the table, not the lines; #58)
	bool Shortened;                   // +0x48, a line was cut to the width
};

// A text with formatting tokens (<br>, <p>, <hr>, <vspace=n>, <hspace=n>,
// <indent=n>, colours ...), laid out as one TLabel per piece (vtable 0x560c40,
// 0x38 bytes); the help bubble's text. A width or height below 1 means "as
// large as the text".
class TFormattedText : public TWindow {
public:
	TFormattedText(const TFormattedText &) = delete;
	TFormattedText &operator=(const TFormattedText &) = delete;
	TFormattedText(const char *_Text); // 0x465e60, line 508
	// Inline (out-of-line copy 0x452630, deleting destructor 0x452d70).
	~TFormattedText() override
	{
		if (Text != nullptr)
			Text.reset();
		if (Line != nullptr)
			Line.reset();
	}
	void SetSize(int Width, int Height) override;                   // +0x34, 0x456200, line 531
	void Arrange() override;                                        // +0x48, 0x466010, lines 684-715
	virtual void SetText(const char *_Text, bool Rearrange);        // +0xa0, 0x456810, lines 723-734
	int GetToken(const char *Text, int &TokenType, int &TokenData); // 0x4563d0, line 577

	std::unique_ptr<char[]> Text; // +0x2c (issue #58)
	std::unique_ptr<char[]> Line; // +0x30, the piece being laid out, a buffer as large as Text (issue #58)
	bool AutoWidth;               // +0x34
	bool AutoHeight;              // +0x35
};

class TTextButton;

// A text with a button that opens Destination with OpenURL (vtable 0x560ce8,
// 0x3c bytes): the dialogs' links to the web site.
class TLink final : public TWindow {
public:
	TLink(const TLink &) = delete;
	TLink &operator=(const TLink &) = delete;
	TLink(const char *_Text, const char *ButtonText, const char *_Destination, void (*_OpenURL)(const char *URL),
		TFont Font); // 0x46b030, lines 785-796
	// Inline (out-of-line copy 0x452670, deleting destructor 0x452d90).
	~TLink() override { Destination.reset(); }
	void SetSize(int Width, int Height) override; // +0x34, 0x456df0, line 838
	int getMinimumWidth() override;               // +0x3c, 0x4526a0 (inline, below)
	int getMinimumHeight() override;              // +0x40, 0x452690 (inline, below)
	void Arrange() override;                      // +0x48, 0x456b00, lines 812-824
	void HandleEvent(int Event) override;         // +0x70, 0x456fa0, line 848

	void (*OpenURL)(const char *URL);    // +0x2c
	std::unique_ptr<char[]> Destination; // +0x30 (issue #58)
	TTextButton *Button;                 // +0x34, event 1
	TLabel *Label;                       // +0x38
};

// --- Icons ------------------------------------------------------------------------

// An outfit with its four colours (vtable 0x561970, 0x48 bytes).
class TOutfitIcon final : public TElement {
public:
	TOutfitIcon(int _Outfit, int _Direction, int *ColorData, const TRGBColor *_Highlight); // 0x4666a0, line 899
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void Draw(int Surface, int x, int y) override;                                                                             // +0x44, 0x457140, lines 908-911
	bool MouseMove([[maybe_unused]] int x, [[maybe_unused]] int y) override { return false; }                                  // +0x4c, 0x452620
	bool LeftButtonDown([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; } // +0x50, 0x457700
	bool LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }   // +0x54, 0x457700
	bool RightButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }  // +0x5c, 0x457700
	// Inline (out-of-line copy 0x4526b0, called by Dialogs.cpp).
	void SetOutfit(int _Outfit) { Outfit = _Outfit; }
	void SetColor(int Mask, int Color);   // 0x457440, line 934
	void SetColors(const int *ColorData); // 0x4575a0, line 942

	int Outfit;                // +0x24
	std::array<int, 4> Colors; // +0x28, head, body, legs, feet
	int Direction;             // +0x38, kept, not drawn
	TRGBColor Highlight;       // +0x3c
};

// One skin, sized to it (vtable 0x5614f8, 0x28 bytes).
class TSkinIcon : public TElement {
public:
	TSkinIcon(int _Skin); // 0x457710, line 1002
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void Draw(int Surface, int x, int y) override;                                                                             // +0x44, 0x4578c0, lines 1009-1014
	bool MouseMove([[maybe_unused]] int x, [[maybe_unused]] int y) override { return false; }                                  // +0x4c, 0x452620
	bool LeftButtonDown([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; } // +0x50, 0x457700
	bool LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }   // +0x54, 0x457700
	bool RightButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }  // +0x5c, 0x457700
	bool MouseWheel([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Direction) override { return false; } // +0x60, 0x457700

	int Skin; // +0x24
};

// An object (vtable 0x560d88, 0x50 bytes), optionally in a frame (skin 102).
class TObjectIcon final : public TElement {
public:
	// Inline (out-of-line copy 0x4526c0).
	TObjectIcon(TObject _Obj, const TRGBColor *_Highlight, bool _Framed);
	// The destructor is the implicit one (deleting destructor 0x429590).
	void Draw(int Surface, int x, int y) override;                                                                             // +0x44, 0x457c40, lines 1052-1063
	bool MouseMove([[maybe_unused]] int x, [[maybe_unused]] int y) override { return false; }                                  // +0x4c, 0x452620
	bool LeftButtonDown([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; } // +0x50, 0x457700
	bool LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }   // +0x54, 0x457700
	bool RightButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override { return false; }  // +0x5c, 0x457700

	TRect Unused24;      // +0x24, cleared, never read
	bool Framed;         // +0x34
	TRGBColor Highlight; // +0x38
	TObject Obj;         // +0x44
};

// --- Buttons ----------------------------------------------------------------------

struct TRadioButtonGroup;

// A button drawn with two skins (released, pressed) that sends Callback when
// it is released over itself (vtable 0x5619f8, 0x44 bytes). Repeat sends
// Callback again on every held-button tick (MouseWheel code 6). A button of a
// radio group shows as pressed while it is the group's selection.
class TSkinButton : public TElement {
public:
	TSkinButton(int x, int y, int _ButtonUp, int _ButtonDown, TCallback _Callback, const char *_Tooltip,
		bool _Repeat); // 0x466860, line 1113
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void Draw(int Surface, int x, int y) override;                                                          // +0x44, 0x458150, lines 1122-1127
	bool MouseMove(int x, int y) override;                                                                  // +0x4c, 0x4584e0
	bool LeftButtonDown(int x, int y, int Flags) override;                                                  // +0x50, 0x458540, line 1155
	bool LeftButtonUp(int x, int y, int Flags) override;                                                    // +0x54, 0x458710, line 1174
	bool MouseWheel(int x, int y, int Code) override;                                                       // +0x60, 0x4588f0, line 1192
	void MouseHover([[maybe_unused]] int x, [[maybe_unused]] int y) override { SetMouseOverText(Tooltip); } // +0x64, 0x452780
	void ReleaseCapture(TComponent *Component) override                                                     // +0x7c, 0x452770
	{
		Pressed = false;
		TElement::ReleaseCapture(Component);
	}
	virtual bool IsPressed();                           // +0x84, 0x458ab0
	void SetSkins(int _ButtonUp, int _ButtonDown);      // 0x458b20, lines 1228-1229
	void SetRadioButtonGroup(TRadioButtonGroup *Group); // 0x458de0, line 1239
	// Inline (out-of-line copy 0x452730, called by Dialogs.cpp).
	void SetEnabled(bool _Enabled) { Enabled = _Enabled; }

	int ButtonUp;                        // +0x24
	int ButtonDown;                      // +0x28
	bool Pressed;                        // +0x2c, the mouse went down on the button
	bool MouseInside;                    // +0x2d
	bool Repeat;                         // +0x2e
	TCallback Callback;                  // +0x30
	TRadioButtonGroup *RadioButtonGroup; // +0x38
	const char *Tooltip;                 // +0x3c
	bool Enabled;                        // +0x40
};

// The buttons of a radio group (0x28 bytes; the type name is ours). The
// constructor and the destructor are inline (ContentWindows.cpp's
// TCombatControls; the out-of-line vector constructor is 0x425e70); Selected
// is left uninitialised.
struct TRadioButtonGroup {
	TRadioButtonGroup() : Buttons(0, 0, 10, nullptr) { Count = 0; }
	TSkinButton *Selected;                 // +0x00
	TIndexedVector<TSkinButton *> Buttons; // +0x04
	int Count;                             // +0x24
};

// A button on a coloured background (vtable 0x561a80, 0x50 bytes).
class TColorButton final : public TSkinButton {
public:
	TColorButton(int x, int y, TRGBColor _Color, int _ButtonUp, int _ButtonDown, TCallback _Callback,
		const char *_Tooltip);                     // 0x466a60
	void Draw(int Surface, int x, int y) override; // +0x44, 0x458f50, line 1274

	TRGBColor Color; // +0x44
};

// A button with an icon skin on it (vtable 0x561b08, 0x48 bytes).
class TIconButton final : public TSkinButton {
public:
	TIconButton(int x, int y, int _Icon, int _ButtonUp, int _ButtonDown, TCallback _Callback,
		const char *_Tooltip);                     // 0x466ad0
	void Draw(int Surface, int x, int y) override; // +0x44, 0x459130, lines 1305-1310

	int Icon; // +0x44
};

// A button that alternates between two pairs of skins (vtable 0x561b90, 0x5c
// bytes): MapWindow.cpp's hint button.
class TBlinkingButton : public TSkinButton {
public:
	TBlinkingButton(int x, int y, int _ButtonUp1, int _ButtonDown1, int _ButtonUp2, int _ButtonDown2,
		TCallback _Callback, const char *_Tooltip); // 0x466b20
	void Draw(int Surface, int x, int y) override;  // +0x44, 0x4594e0, line 1363

	int NextChange;  // +0x44, GetMilliseconds() time
	int Phase;       // +0x48, 1 or 2
	int ButtonUp1;   // +0x4c
	int ButtonUp2;   // +0x50
	int ButtonDown1; // +0x54
	int ButtonDown2; // +0x58
};

// A button with a text (vtable 0x561c40, 0x78 bytes).
class TTextButton : public TSkinButton {
public:
	TTextButton(int x, int y, const char *_Text, int _ButtonUp, int _ButtonDown, TCallback _Callback,
		const char *_Tooltip);                                                               // 0x466bc0, lines 1378-1379
	TTextButton(int x, int y, const char *_Text, TCallback _Callback, const char *_Tooltip); // 0x466ed0, lines 1390-1391
	void Draw(int Surface, int x, int y) override;                                           // +0x44, 0x4596d0, line 1410
	virtual void SetText(const char *_Text) { Text.Assign(_Text); }                          // +0x88, 0x466eb0

	TFixedString<50> Text; // +0x44
};
inline TObjectIcon::TObjectIcon(TObject _Obj, const TRGBColor *_Highlight, bool _Framed)
{
	Obj = _Obj;
	Framed = _Framed;
	Highlight = *_Highlight;
}

inline int TLink::getMinimumWidth()
{
	return Button->Width;
}

inline int TLink::getMinimumHeight()
{
	return Button->Height;
}

// A button that stays pressed while State is set; a click toggles it
// (vtable 0x561cd0, 0x50 bytes). The tooltips belong to the two states.
class TToggleButton : public TSkinButton {
public:
	TToggleButton(int x, int y, int _ButtonUp, int _ButtonDown, TCallback _Callback, const char *_Tooltip1,
		const char *_Tooltip2, bool _State);                                                                                    // 0x4671e0
	bool LeftButtonUp(int x, int y, int Flags) override;                                                                        // +0x54, 0x459940, line 1454
	void MouseHover([[maybe_unused]] int x, [[maybe_unused]] int y) override { SetMouseOverText(State ? Tooltip2 : Tooltip1); } // +0x64, 0x4671b0
	bool IsPressed() override;                                                                                                  // +0x84, 0x459b00

	bool State;           // +0x44
	const char *Tooltip1; // +0x48
	const char *Tooltip2; // +0x4c
};

// A text button with a state (vtable 0x561d58, 0x84 bytes): ContentWindows.cpp's
// TButtonsWindow sets State while it draws.
class TTextToggleButton : public TTextButton {
public:
	TTextToggleButton(int x, int y, const char *_Text, int _ButtonUp, int _ButtonDown, TCallback _Callback,
		const char *_Tooltip1, const char *_Tooltip2, bool _State);                                                             // 0x467230
	bool LeftButtonUp(int x, int y, int Flags) override;                                                                        // +0x54, 0x459b70, line 1511
	void MouseHover([[maybe_unused]] int x, [[maybe_unused]] int y) override { SetMouseOverText(State ? Tooltip2 : Tooltip1); } // +0x64, 0x467290
	bool IsPressed() override;                                                                                                  // +0x84, 0x459d30

	bool State;           // +0x78
	const char *Tooltip1; // +0x7c
	const char *Tooltip2; // +0x80
};

// A button that steps through States states, each with its skins and callback
// (vtable 0x560e10, 0x58 bytes): the console's volume. The constructor takes
// the first state's skins and callback, then those of the other states.
class TMultiStateButton : public TSkinButton {
public:
	TMultiStateButton(const TMultiStateButton &) = delete;
	TMultiStateButton &operator=(const TMultiStateButton &) = delete;
	TMultiStateButton(int x, int y, const char *_Tooltip, int _States, int _ButtonUp, int _ButtonDown,
		TCallback _Callback, ...); // 0x4672d0
	// Inline (out-of-line copy 0x452740, deleting destructor 0x452db0).
	~TMultiStateButton() override
	{
		ButtonUps.reset();
		ButtonDowns.reset();
		Callbacks.reset();
	}
	bool LeftButtonUp(int x, int y, int Flags) override; // +0x54, 0x459da0, line 1609
	// Inline (out-of-line copy 0x452790): State is set past the last state,
	// which wraps to state 0.
	void Reset()
	{
		State = States + 1;
		if (State >= States)
			State = 0;
		ButtonUp = ButtonUps[State];
		ButtonDown = ButtonDowns[State];
	}

	int State;                              // +0x44
	int States;                             // +0x48
	std::unique_ptr<int[]> ButtonUps;       // +0x4c (issue #58)
	std::unique_ptr<int[]> ButtonDowns;     // +0x50
	std::unique_ptr<TCallback[]> Callbacks; // +0x54
};

// A check box with a text (vtable 0x561de8, 0x34 bytes).
class TCheckBox final : public TWindow {
public:
	TCheckBox(const char *_Text); // 0x467440, lines 1686-1699
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void Draw(int Surface, int x, int y) override;                                                         // +0x44, 0x459f90, lines 1711-1716
	void Arrange() override;                                                                               // +0x48, 0x45a360
	bool LeftButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags) override // +0x54, 0x4527c0
	{
		if (Enabled)
			Checked = !Checked;
		return true;
	}
	virtual void SetChecked(bool _Checked) { Checked = _Checked; } // +0xa0, 0x467420
	virtual bool IsChecked() { return Checked; }                   // +0xa4, 0x467430
	void SetEnabled(bool _Enabled);                                // 0x4678d0, line 1768

	TLabel *Label; // +0x2c
	bool Checked;  // +0x30
	bool Enabled;  // +0x31
};

// --- Edit lines -------------------------------------------------------------------

class TFocusGroup;

// A line of text input (vtable 0x560e98, 0x68 bytes; "_Edit" in ContextMenus.cpp),
// the console's Input. HandleEvent takes 16 copy, 17 cut, 18 paste, 19 delete
// and 20 select all. A read-only line (ReadOnly) allows only moving, copying
// and selecting; a disabled one (Disabled) shows no text and takes no input.
class TEditLine : public TWindow {
public:
	TEditLine(const TEditLine &) = delete;
	TEditLine &operator=(const TEditLine &) = delete;
	TEditLine(TRect Area, int _MaxLength, bool _ReadOnly); // 0x467b50, lines 1878-1900
	TEditLine(int _MaxLength, bool _ReadOnly);             // 0x45a4c0, lines 1908-1927
	// Inline (out-of-line copy 0x4527e0, deleting destructor 0x452dd0).
	~TEditLine() override { Text.reset(); }
	void SetSize(int Width, int Height) override;                 // +0x34, 0x45ac40, line 1974
	void Draw(int Surface, int x, int y) override;                // +0x44, 0x45a800, line 1963
	void Arrange() override;                                      // +0x48, 0x45ade0
	bool MouseMove(int x, int y) override;                        // +0x4c, 0x45af40, line 2014
	bool LeftButtonDown(int x, int y, int Flags) override;        // +0x50, 0x467ec0, line 2037
	bool LeftButtonUp(int x, int y, int Flags) override;          // +0x54, 0x45b130, line 2058
	bool LeftButtonDoubleClick(int x, int y, int Flags) override; // +0x58, 0x45b2e0, line 2078
	bool RightButtonUp(int x, int y, int Flags) override;         // +0x5c, 0x45b5b0, line 2093
	void KeyPress(int Key) override;                              // +0x6c, 0x4680c0, line 2156
	void HandleEvent(int Event) override;                         // +0x70, 0x45b790, lines 2206-2209
	void ReleaseCapture(TComponent *Component) override           // +0x7c, 0x452800
	{
		Selecting = false;
		TWindow::ReleaseCapture(Component);
	}
	virtual void SetText(const char *_Text);                                    // +0xa0, 0x45bcc0, lines 2243-2252
	virtual void Clear();                                                       // +0xa4, 0x45c980, line 2342
	virtual void DeleteText(int start, int length);                             // +0xa8, 0x45c760, line 2327
	virtual void DeleteChar(int Position) { DeleteText(Position, 1); }          // +0xac, 0x452810
	virtual int InsertText(int Position, const char *String, int insertlength); // +0xb0, 0x45bf70, lines 2263-2294
	virtual void InsertChar(int Position, char c);                              // +0xb4, 0x45c490, lines 2302-2309
	virtual void SetDisabled(bool _Disabled) { Disabled = _Disabled; }          // +0xb8, 0x452830
	virtual bool IsDisabled() { return Disabled; }                              // +0xbc, 0x452840
	virtual void SetFont(TFont _Font) { Font = _Font; }                         // +0xc0, 0x452850
	virtual int GetPosition(int x, int y);                                      // +0xc4, 0x4528e0 (inline, below)
	void SetActive(bool _Active);                                               // 0x45bc80
	void SetFocusGroup(TFocusGroup *_FocusGroup);                               // 0x468470, lines 2230-2235
	// Inline (out-of-line copy 0x417520 in ConsoleWindow.obj).
	[[nodiscard]] bool HasSelection() const { return SelectionStart != -1 && SelectionLength != 0; }
	// Inline (out-of-line copies 0x4524e0, 0x452530, 0x452880).
	void SetCursor(int Position)
	{
		Cursor = Position;
		Limit(&Cursor, 0, (int)strlen(Text.get()));
	}
	void LimitCursor() { Limit(&Cursor, 0, (int)strlen(Text.get())); }
	void SetSelection(int Position)
	{
		Limit(&Position, 0, (int)strlen(Text.get()));
		SelectionLength = 0;
		SelectionStart = Position;
	}

	std::unique_ptr<char[]> Text; // +0x2c (issue #81)
	int MaxLength;                // +0x30
	int Cursor;                   // +0x34
	bool SingleLine;              // +0x38, a paste stops at the first line break
	TFocusGroup *FocusGroup;      // +0x3c
	TFont Font;                   // +0x40
	int FirstVisible;             // +0x54, the first character shown
	bool Active;                  // +0x58, shows the cursor
	int SelectionStart;           // +0x5c, -1 without a selection
	int SelectionLength;          // +0x60, negative when selected to the left
	bool Selecting;               // +0x64, a mouse drag
	bool Disabled;                // +0x65
	bool ReadOnly;                // +0x66
};

// The character under (x, y): the first visible one left of the text, the
// end right of or below it.
inline int TEditLine::GetPosition(int x, int y)
{
	x -= SkinProperties().Data99C;
	y -= SkinProperties().Data99C;
	if (x < 0 || y < 0)
		return FirstVisible;
	if (x > Width - 2 * SkinProperties().Data99C || y > Height - 2 * SkinProperties().Data99C)
		return (int)strlen(Text.get());
	const int Position = GetFittingTextLength(Font.FontNumber, Text.get() + FirstVisible, x) + FirstVisible;
	if (Position > (int)strlen(Text.get()))
		return (int)strlen(Text.get());
	return Position;
}

// An edit line that shows stars (vtable 0x560f60, 0x6c bytes). It only pastes.
class TPasswordEditLine final : public TEditLine {
public:
	TPasswordEditLine(const TPasswordEditLine &) = delete;
	TPasswordEditLine &operator=(const TPasswordEditLine &) = delete;
	TPasswordEditLine(int _MaxLength); // 0x45cb50
	// Inline (out-of-line copy 0x452980, deleting destructor 0x452df0).
	~TPasswordEditLine() override { Stars.reset(); }
	void Draw(int Surface, int x, int y) override; // +0x44, 0x45cbb0, line 2420
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): the original's call of the base's base (the image's)
	bool MouseMove(int x, int y) override { return TWindow::MouseMove(x, y); } // +0x4c, 0x4529c0, no selecting
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): the original's call of the base's base (the image's)
	bool RightButtonUp(int x, int y, int Flags) override { return TWindow::RightButtonUp(x, y, Flags); } // +0x5c, 0x4529b0, no menu
	void HandleEvent(int Event) override;                                                                // +0x70, 0x45cda0, line 2430

	std::unique_ptr<char[]> Stars; // +0x68 (issue #81)
};

// A multi-line edit field with a scroll bar (vtable 0x562040, 0x9c bytes).
// LineStart holds the first character of every line (Lines entries, then the
// text's end).
class TScrollBar;
class TEditText final : public TEditLine {
public:
	TEditText(int MaxLength, bool _ReadOnly); // 0x46b570, line 2466
	// The destructor is the implicit one (0x46b540, deleting destructor 0x46b520).
	int getMinimumWidth() override;                   // +0x3c, 0x46b500 (inline, below)
	int getMinimumHeight() override;                  // +0x40, 0x46b510 (inline, below)
	void Draw(int Surface, int x, int y) override;    // +0x44, 0x45cf30, line 2503
	void Arrange() override;                          // +0x48, 0x4686f0, line 2536
	bool MouseWheel(int x, int y, int Code) override; // +0x60, 0x45d400, line 2549
	void KeyPress(int Key) override;                  // +0x6c, 0x468a60, line 2703
	int GetPosition(int x, int y) override;           // +0xc4, 0x45d770, line 2762
	// NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method): the original's virtual over TEditLine's plain one
	virtual void SetActive(bool _Active);                       // +0xc8, 0x45d5d0, line 2728
	virtual void SetText(const char *_Text, bool KeepPosition); // +0xcc, 0x469120, line 2711
	using TEditLine::SetText;                                   // TEditLine's SetText(Text) too (GCC's -Woverloaded-virtual, issue #111)
	void FormatText();                                          // 0x45da90, lines 2770-2795

	TIndexedVector<int> LineStart; // +0x68
	TScrollBar *ScrollBar;         // +0x88
	int Lines;                     // +0x8c
	int FieldWidth;                // +0x90
	int FieldHeight;               // +0x94
	int VisibleLines;              // +0x98
};

// The edit lines of a dialog between which Tab and Shift+Tab move the input
// (0x28 bytes).
class TFocusGroup {
public:
	TFocusGroup();                     // 0x45ddf0
	void SetActive(TEditLine *Member); // 0x45de20, line 2828
	void AddMember(TEditLine *Member); // 0x45dfc0, lines 2835-2836
	void Next();                       // 0x45e290, lines 2846-2860
	void Previous();                   // 0x45e630, lines 2868-2882

	TEditLine *Active;                      // +0x00
	TIndexedVector<TEditLine *> MemberList; // +0x04
	int Members;                            // +0x24
};

// --- Scroll bars ------------------------------------------------------------------

// The track and slider of a horizontal scroll bar (vtable 0x5617e0, 0x54
// bytes); Callback is sent on every change of Value.
class THorizontalScrollBarRange final : public TElement {
public:
	THorizontalScrollBarRange(int _Min, int _Max, int _Value, int _Visible, TCallback _Callback); // 0x45e9e0, lines 2925-2944
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void SetSize(int Width, int Height) override;                        // +0x34, 0x45eff0, line 2986
	int getMinimumWidth() override { return SkinProperties().Data87C; }  // +0x3c, 0x45ee00
	int getMinimumHeight() override { return SkinProperties().Data880; } // +0x40, 0x45edf0
	void Arrange() override;                                             // +0x48, 0x45f1a0
	bool MouseMove(int x, int y) override;                               // +0x4c, 0x45f3d0, line 3039
	bool LeftButtonDown(int x, int y, int Flags) override;               // +0x50, 0x45f5c0, line 3057
	bool LeftButtonUp(int x, int y, int Flags) override;                 // +0x54, 0x45f790, line 3081
	bool MouseWheel(int x, int y, int Code) override;                    // +0x60, 0x45f9d0, line 3113
	void KeyPress(int Key) override;                                     // +0x6c, 0x45fca0, line 3131
	void ReleaseCapture(TComponent *Component) override                  // +0x7c, 0x45ede0
	{
		Dragging = false;
		TElement::ReleaseCapture(Component);
	}
	void SetValue(int _Value); // 0x45ee10, line 2974

	int ButtonRange;    // +0x24, the step of a button or wheel
	int Min;            // +0x28
	int Max;            // +0x2c
	int Value;          // +0x30
	int Visible;        // +0x34, the part shown at once (the slider's share)
	bool Dragging;      // +0x38, the slider
	bool Pressed;       // +0x39
	int DragOffset;     // +0x3c
	int SliderWidth;    // +0x40
	int SliderPosition; // +0x44
	bool PageScroll;    // +0x48, a click beside the slider moves by Visible-1
	bool Enabled;       // +0x49, Min!=Max
	TCallback Callback; // +0x4c
};

// A horizontal scroll bar: two buttons and the range (vtable 0x561ed0, 0x38
// bytes). The height is always SkinProperties.Data880.
class THorizontalScrollBar final : public TWindow {
public:
	THorizontalScrollBar(int Min, int Max, int _Value, int _Visible, int _ButtonRange, TCallback _Callback); // 0x4692d0, lines 3158-3169
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void SetSize(int Width, int Height) override;                        // +0x34, 0x45fe90, line 3188
	int getMinimumWidth() override { return SkinProperties().Data884; }  // +0x3c, 0x4692c0
	int getMinimumHeight() override { return SkinProperties().Data880; } // +0x40, 0x45edf0
	bool MouseWheel(int x, int y, int Code) override;                    // +0x60, 0x4600c0, line 3200
	void KeyPress(int Key) override;                                     // +0x6c, 0x4731d0 (inline, below)
	void HandleEvent(int Event) override;                                // +0x70, 0x460280, line 3213

	THorizontalScrollBarRange *Range; // +0x2c
	TSkinButton *LeftButton;          // +0x30, event 5
	TSkinButton *RightButton;         // +0x34, event 6
};

inline void THorizontalScrollBar::KeyPress(int Key)
{
	Range->KeyPress(Key);
}

// The track and slider of a vertical scroll bar (vtable 0x561868, 0x4c bytes).
class TScrollBarRange : public TElement {
public:
	TScrollBarRange(int x, int y, int Width, int Height, int _Min, int _Max, int _Value, int _Visible); // 0x469670, lines 3227-3248
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	void SetSize(int Width, int Height) override;                        // +0x34, 0x4608e0, line 3315
	int getMinimumWidth() override { return SkinProperties().Data870; }  // +0x3c, 0x469e60
	int getMinimumHeight() override { return SkinProperties().Data86C; } // +0x40, 0x460440
	void Arrange() override;                                             // +0x48, 0x460a90
	bool MouseMove(int x, int y) override;                               // +0x4c, 0x460cc0, line 3367
	bool LeftButtonDown(int x, int y, int Flags) override;               // +0x50, 0x460eb0, line 3385
	bool LeftButtonUp(int x, int y, int Flags) override;                 // +0x54, 0x461080, line 3408
	bool MouseWheel(int x, int y, int Code) override;                    // +0x60, 0x4612c0, line 3438
	void KeyPress(int Key) override;                                     // +0x6c, 0x461580, line 3455
	void ReleaseCapture(TComponent *Component) override                  // +0x7c, 0x460430
	{
		Dragging = false;
		TElement::ReleaseCapture(Component);
	}
	void SetRange(int _Min, int _Max, int _Value, int _Visible); // 0x460450, lines 3281-3291
	void SetValue(int _Value);                                   // 0x460710, line 3303

	int ButtonRange;    // +0x24
	int Min;            // +0x28
	int Max;            // +0x2c
	int Value;          // +0x30
	int Visible;        // +0x34
	bool Pressed;       // +0x38
	bool Dragging;      // +0x39
	int DragOffset;     // +0x3c
	int SliderHeight;   // +0x40
	int SliderPosition; // +0x44
	bool PageScroll;    // +0x48
	bool Enabled;       // +0x49
};

// A vertical scroll bar: two buttons and the range (vtable 0x561f70, 0x38
// bytes). The width is always SkinProperties.Data878.
class TScrollBar : public TWindow {
public:
	TScrollBar(TRect Area, int Min, int Max, int _Value, int _Visible, int _ButtonRange); // 0x469a80, lines 3462-3475
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	void SetSize(int Width, int Height) override;                        // +0x34, 0x461770, line 3509
	int getMinimumWidth() override { return SkinProperties().Data870; }  // +0x3c, 0x469e60
	int getMinimumHeight() override { return SkinProperties().Data874; } // +0x40, 0x469e50
	bool MouseWheel(int x, int y, int Code) override;                    // +0x60, 0x4619a0, line 3521
	void KeyPress(int Key) override { Range->KeyPress(Key); }            // +0x6c, 0x4731d0 (folded)
	void HandleEvent(int Event) override;                                // +0x70, 0x461b60, line 3534

	TScrollBarRange *Range;  // +0x2c
	TSkinButton *UpButton;   // +0x30, event 3
	TSkinButton *DownButton; // +0x34, event 4
};

inline int TEditText::getMinimumWidth()
{
	return ScrollBar->getMinimumWidth();
}

inline int TEditText::getMinimumHeight()
{
	return ScrollBar->getMinimumHeight();
}

// --- List boxes -------------------------------------------------------------------

// A list of text lines with a selection and a scroll bar (vtable 0x561138,
// 0x7c bytes). SelectCallback goes out when a line is clicked or selected by
// key, ActionCallback on a double click. An editable list edits the selected
// line in EditLine (a double click starts, Return or another line ends).
class TListBox : public TWindow {
public:
	TListBox(const TListBox &) = delete;
	TListBox &operator=(const TListBox &) = delete;
	TListBox(TCallback _SelectCallback, TCallback _ActionCallback); // 0x469e70, line 3596
	// Inline (out-of-line copy 0x452e10, deleting destructor 0x453230).
	~TListBox() override
	{
		for (int i = 0; i < Lines; i++)
			delete[] LineText(i);
		if (!Editing && EditLine != nullptr)
			delete EditLine;
	}
	void SetSize(int Width, int Height) override;                 // +0x34, 0x4620e0, line 3646
	void Draw(int Surface, int x, int y) override;                // +0x44, 0x461d10, line 3635
	void Arrange() override;                                      // +0x48, 0x46a120, line 3676
	bool LeftButtonDown(int x, int y, int Flags) override;        // +0x50, 0x46a460, line 3696
	bool LeftButtonDoubleClick(int x, int y, int Flags) override; // +0x58, 0x46a640, line 3720
	bool MouseWheel(int x, int y, int Code) override;             // +0x60, 0x462290, line 3747
	void MouseHover(int x, int y) override;                       // +0x64, 0x46a830, line 3734
	void KeyPress(int _Key) override;                             // +0x6c, 0x46aa30, line 3791
	virtual void StartEdit();                                     // +0xa0, 0x463060, line 3957
	virtual void StopEdit();                                      // +0xa4, 0x463280, line 3971
	void AddLine(const char *Text);                               // 0x462460, lines 3799-3806
	void SetLine(int Number, const char *Text);                   // 0x462730, lines 3831-3839
	char *GetLine(int Number);                                    // 0x462b20, line 3847
	void SetSelection(int Number);                                // 0x46ad10, lines 3891-3896
	void Clear();                                                 // 0x462ca0, line 3921
	[[nodiscard]] int GetLineAt(int x, int y) const;              // 0x462e70, line 3938
	// Inline (out-of-line copies 0x4529d0 and 0x4529f0, called by Dialogs.cpp).
	[[nodiscard]] int GetSelection() const
	{
		if (Selected < 0 || Selected > Lines - 1)
			return -1;
		return Selected;
	}
	void SetEditable(bool _Editable) { Editable = _Editable; }
	// Inline (Dialogs.cpp; the out-of-line copy is folded with Talk.h's
	// TChannelTalkBuffer::GetChannelID 0x507060).
	[[nodiscard]] int GetNumberOfLines() const { return Lines; }

	int Lines;                       // +0x2c
	int TextWidth;                   // +0x30
	TIndexedVector<char *> LineText; // +0x34
	int Selected;                    // +0x54, -1 for none
	TCallback SelectCallback;        // +0x58
	TCallback ActionCallback;        // +0x60
	TScrollBar *ScrollBar;           // +0x68
	int VisibleLines;                // +0x6c
	bool Editable;                   // +0x70
	bool Editing;                    // +0x71
	TEditLine *EditLine;             // +0x74
	int EditScroll;                  // +0x78, the scroll position when the edit started
};

// A line of a TFontListBox (0x18 bytes; the type name is ours). The
// constructor is implicit (0x452c70, TFont's defaults; Text is left as it is).
struct TListBoxLine {
	char *Text; // +0x00
	TFont Font; // +0x04
};

// A list box whose lines have their own fonts (vtable 0x5611e0, 0xb0 bytes).
// It keeps its lines in FontLines; the base's LineText stays empty.
class TFontListBox final : public TListBox {
public:
	TFontListBox(const TFontListBox &) = delete;
	TFontListBox &operator=(const TFontListBox &) = delete;
	TFontListBox(TCallback _SelectCallback, TCallback _ActionCallback); // 0x46afc0
	// Inline (out-of-line copy 0x453250, deleting destructor 0x4532e0).
	~TFontListBox() override
	{
		for (int i = 0; i < Lines; i++)
			delete[] FontLines(i).Text;
	}
	void Draw(int Surface, int x, int y) override;          // +0x44, 0x463430, line 4029
	void MouseHover(int x, int y) override;                 // +0x64, 0x464670, line 4133
	void StartEdit() override;                              // +0xa0, 0x464870, line 4163
	void StopEdit() override;                               // +0xa4, 0x464af0, line 4183
	void AddLine(const char *Text, TFont Font);             // 0x463820, lines 4051-4059
	void AddLine(int MaxLength);                            // 0x463b10, lines 4067-4074
	void SetLine(int Number, const char *Text, TFont Font); // 0x463dc0, lines 4082-4091
	// NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method): the original's, called on a TFontListBox only
	void Clear();                           // 0x4641e0, line 4105
	char *GetLine(int Number, TFont *Font); // 0x4643a0, lines 4113-4118

	TIndexedVector<TListBoxLine> FontLines; // +0x7c
};

// --- Progress bar -----------------------------------------------------------------

// A bar filled to current/maximum (vtable 0x561028, 0x2c bytes).
class TProgressBar final : public TElement {
public:
	// Inline (out-of-line copy 0x452a00).
	TProgressBar()
	{
		DrawingListLength = 6;
		DrawingList.reset(new TDrawing[6]);
		Current = 0;
		Maximum = 100;
	}
	// The destructor is the implicit one (deleting destructor 0x429590, folded).
	int getMinimumWidth() override { return Skins()[256].width + Skins()[258].width + Skins()[257].width; } // +0x3c, 0x452a90
	int getMinimumHeight() override { return Skins()[256].height; }                                         // +0x40, 0x452ab0
	void Arrange() override;                                                                                // +0x48, 0x464d00
	void SetMaximum(int maximum);                                                                           // 0x464f40, lines 4271-4276
	void SetCurrent(int current);                                                                           // 0x4651c0, line 4289
	void SetComplete();                                                                                     // 0x465380, line 4301

	int Maximum; // +0x24
	int Current; // +0x28
};
// NOLINTEND(cppcoreguidelines-owning-memory)
