// ConsoleWindow.h: the console below the map (c:\mud\tibiaplayer\ConsoleWindow.h,
// lines 47-54 of it are TLineFormat's constructor).
//
// TConsoleWindow (vtable 0x55e1a0, 0x6c bytes) is MainWindow.cpp's Console: a
// tab bar (TTabBar, vtable 0x55dcd8, 0x40 bytes) with one tab (TTab, vtable
// 0x55e098, 0x58 bytes) per open channel, the view of the current channel
// (TConsole, vtable 0x55dfc0, 0x38 bytes: a TTextWindow and a scroll bar),
// the edit line and the buttons. A TTextWindow (vtable 0x55dd80, 0x94 bytes)
// shows the lines of a Talk.cpp text buffer, broken into TLineFormats (0x28
// bytes) that fit its width, with a selection. Tabs that do not fit into the
// bar are moved into two hidden windows of the bar (LeftTabs, RightTabs), whose
// arrow buttons scroll them back in.
//
// Names from the binary: TLineFormat's LineText and its constructor's
// parameters (_TextLine, _LineText, _LineLength, _OriginalOffset, _Indent),
// getFirstLineNumber, getLastLineNumber, _CurrentLineNumber, InvisibleLines
// (an error message), getTextBuffer, isDefault, getCaption, CurrentConsole,
// CurrentTab, DefaultTab, NewTab, Input, TextBuffer, _Caption, _TextBuffer,
// _Active, Tab, Container ("failed to get composite"), Search, Question,
// Addressee, ChannelID, TalkMode, MessageSeparator and the catch-block
// parameters (Surface, x, y, Width, Height, Code, Event, Key, LineNumber,
// Active, RecvMode, SendMode). The class names and the other names are ours;
// see notes/decisions.md, "ConsoleWindow.cpp".

#pragma once

#include "common.h"
#include "types/IndexedVector.h"
#include "GUI.h"
#include "Talk.h"
#include "graphics.h"
#include <memory>

class TEditLine;         // DialogElements.h
class TScrollBar;        // DialogElements.h
class TSkinButton;       // DialogElements.h
class TToggleButton;     // DialogElements.h
class TMultiStateButton; // DialogElements.h

// One line of a text window: the part of a talk line that fits into the
// window's width. Indent is 0 for the first line of a talk line and 10 for
// the following ones (0 for the modes 18-22); LineEnd is set on the last
// part and where the text has a line break (the selection copies a "\n"
// there).
struct TLineFormat {
	// Inline in Container.h's vector constructor (0x417aa0) and in
	// FormatLines: the colour is cleared by its own constructor first.
	TLineFormat()
		: LineText(nullptr), LineLength(0), OriginalOffset(0), Indent(0), FontNumber(4), TextLine(nullptr),
		  LineEnd(true)
	{
		Color = DefaultFontColor;
	}
	// 0x418020, lines 47-54 (out of line, no try block).
	TLineFormat(TTalkLine *_TextLine, const char *_LineText, int _LineLength, int _OriginalOffset, int _Indent,
		int _FontNumber, TRGBColor _Color, bool _LineEnd)
	{
		PATELLA_ASSERT(_TextLine!=nullptr);
		PATELLA_ASSERT(_LineText!=nullptr);
		PATELLA_ASSERT_VALUES(_LineLength>0, _LineLength);
		PATELLA_ASSERT_VALUES(_Indent>=0, _Indent);
		PATELLA_ASSERT_VALUES(_OriginalOffset>=0, _OriginalOffset);
		TextLine = _TextLine;
		Color = _Color;
		LineText = _LineText;
		Indent = _Indent;
		LineLength = _LineLength;
		OriginalOffset = _OriginalOffset;
		LineEnd = _LineEnd;
		FontNumber = _FontNumber;
	}

	const char *LineText; // +0x00, points into the talk line's text
	int LineLength;       // +0x04
	int OriginalOffset;   // +0x08, offset of LineText in the talk line's text
	int Indent;           // +0x0c
	TRGBColor Color;      // +0x10
	int FontNumber;       // +0x1c
	TTalkLine *TextLine;  // +0x20
	bool LineEnd;         // +0x24
};

// The text of a channel (ContextMenus.cpp's _TextWindow). Lines are numbered
// from getFirstLineNumber() (Lines.min, always 0) to getLastLineNumber();
// CurrentLineNumber is the bottom line shown (the scroll bar's value). A
// selection runs from (SelectionStartLine, SelectionStartOffset) to
// (SelectionEndLine, SelectionEndOffset) in either order; -1 is none.
class TTextWindow final : public TElement {
public:
	TTextWindow(const TTextWindow &) = delete;
	TTextWindow &operator=(const TTextWindow &) = delete;
	TTextWindow(); // 0x418520
	// Inline (0x4179f0, deleting destructor 0x417a80); Lines frees its entries.
	~TTextWindow() override
	{
		if (SelectedText != nullptr)
			SelectedText.reset();
	}
	void Draw(int Surface, int x, int y) override;                // +0x44, 0x420550, lines 84-119
	void Arrange() override;                                      // +0x48, 0x420b80, line 148
	bool MouseMove(int x, int y) override;                        // +0x4c, 0x4185b0, line 165
	bool LeftButtonDown(int x, int y, int Flags) override;        // +0x50, 0x418930, line 195
	bool LeftButtonUp(int x, int y, int Flags) override;          // +0x54, 0x418790, line 179
	bool LeftButtonDoubleClick(int x, int y, int Flags) override; // +0x58, 0x420d70, line 218
	bool RightButtonUp(int x, int y, int Flags) override;         // +0x5c, 0x420f90, line 240
	void HandleEvent(int Event) override;                         // +0x70, 0x421190, lines 262-265
	// Inline (0x417a50): the end of a selection drag.
	void ReleaseCapture(TComponent *Component) override
	{
		Selecting = false;
		TElement::ReleaseCapture(Component);
	}
	virtual void FormatLines();                                                  // +0x84, 0x418ca0, line 379
	virtual bool GetPosition(int x, int y, int *Line, int *Offset, int *Length); // +0x88, 0x419fc0, line 674

	void SetTextBuffer(TTalkBuffer *TextBuffer);       // 0x418b10, line 277
	TTalkLine *GetTalkLine(int LineNumber);            // 0x419030, line 411
	TLineFormat *GetLineFormat(int LineNumber);        // 0x419200, line 425
	void SetCurrentLineNumber(int _CurrentLineNumber); // 0x4193d0, line 432
	bool GetSelectionStart(int *Line, int *Offset);    // 0x419560, line 481
	bool GetSelectionEnd(int *Line, int *Offset);      // 0x4197a0, line 510
	bool SelectAll();                                  // 0x4199e0, line 526
	const char *GetSelectedText();                     // 0x419bd0, line 626
	[[nodiscard]] int getFirstLineNumber() const { return Lines.min; }
	[[nodiscard]] int getLastLineNumber() const { return LastLineNumber; }
	// Inline (out-of-line copy 0x417590; inlined into KeyPress): a selection
	// of at least one character exists.
	[[nodiscard]] bool HasSelection() const
	{
		if (TextBuffer == nullptr)
			return false;
		if (!TextBuffer->Complete) {
			TextBuffer->LinesChanged = false;
			return false;
		}
		return SelectionStartLine != -1 && SelectionStartOffset != -1 && SelectionEndLine != -1
			&& SelectionEndOffset != -1
			&& (SelectionStartLine != SelectionEndLine || SelectionStartOffset != SelectionEndOffset);
	}

	TTalkBuffer *TextBuffer;              // +0x24
	TIndexedVector<TLineFormat> Lines;    // +0x28 (entry +0x68)
	int CurrentLineNumber;                // +0x6c
	int InvisibleLines;                   // +0x70, lines above the window when the last one is at the bottom
	int LastLineNumber;                   // +0x74
	int SelectionStartLine;               // +0x78
	int SelectionStartOffset;             // +0x7c
	int SelectionEndLine;                 // +0x80
	int SelectionEndOffset;               // +0x84
	bool Selecting;                       // +0x88, the left button is down
	std::unique_ptr<char[]> SelectedText; // +0x8c, GetSelectedText's result (issue #58)
	bool AtEnd;                           // +0x90, the last line is shown
};

// The view of the current channel: the text window with a scroll bar on its
// right (ScrollToEnd moves it to the last line at the next Arrange).
class TConsole final : public TWindow {
public:
	TConsole(); // 0x41a280, line 695
	// The destructor is the implicit one (deleting destructor 0x4adc40, folded).
	int getMinimumHeight() override;                  // +0x40, 0x41a4d0
	void Draw(int Surface, int x, int y) override;    // +0x44, 0x41a4f0, line 706
	void Arrange() override;                          // +0x48, 0x421450, lines 753-794
	bool MouseWheel(int x, int y, int Code) override; // +0x60, 0x41a6a0, line 717
	[[nodiscard]] TTalkBuffer *getTextBuffer() const { return TextWindow->TextBuffer; }

	TTextWindow *TextWindow; // +0x2c
	TScrollBar *ScrollBar;   // +0x30
	bool ScrollToEnd;        // +0x34
};

// The tab of a channel. Caption is the trimmed and, where it is too wide,
// shortened title; FullCaption the trimmed one. A tab that is not active
// lights up when its buffer gets new lines (Unread), blinking for 10 actions.
class TTab final : public TElement {
public:
	TTab(const TTab &) = delete;
	TTab &operator=(const TTab &) = delete;
	TTab(const char *_Caption, TTalkBuffer *_TextBuffer, TCallback Callback); // 0x41a870, lines 806-831
	~TTab() override;                                                         // +0x00, 0x41acd0, lines 849-866 (deleting 0x421b10)
	// Inline (0x417700, folded with TTabBar's): the tab height.
	int getMinimumHeight() override { return SkinProperties().StyleAA4.Data04; } // +0x40
	void Draw(int Surface, int x, int y) override;                               // +0x44, 0x41b890, lines 981-996
	void Arrange() override;                                                     // +0x48, 0x41b6c0, line 974
	bool MouseMove(int x, int y) override;                                       // +0x4c, 0x41b190, line 898
	bool LeftButtonDown(int x, int y, int Flags) override;                       // +0x50, 0x424200, line 916
	bool LeftButtonUp(int x, int y, int Flags) override;                         // +0x54, 0x41b370, line 931
	bool RightButtonUp(int x, int y, int Flags) override;                        // +0x5c, 0x4243d0, line 945
	void MouseHover(int x, int y) override;                                      // +0x64, 0x41b520, line 956
	// Inline (0x41acc0): the end of a drag.
	void ReleaseCapture(TComponent *Component) override
	{
		Dragging = false;
		TElement::ReleaseCapture(Component);
	}
	void SetActive(bool _Active); // 0x41afe0, line 880
	[[nodiscard]] const char *getCaption() const { return Shortened ? FullCaption.get() : Caption.get(); }
	[[nodiscard]] TTalkBuffer *getTextBuffer() const { return TextBuffer; }

	TTalkBuffer *TextBuffer;             // +0x24 (with the console's filter list: not one owner, notes/ownership.md 7)
	int LineCount;                       // +0x28, the buffer's lines when the tab last looked
	std::unique_ptr<char[]> Caption;     // +0x2c (issue #58)
	std::unique_ptr<char[]> FullCaption; // +0x30
	int Dragging;                        // +0x34
	int DragX;                           // +0x38
	int DragY;                           // +0x3c, set and never read
	int DragRight;                       // +0x40, set and never read
	bool Active;                         // +0x44
	bool Unread;                         // +0x45
	int BlinkEnd;                        // +0x48, action number; 0 = not blinking
	TCallback Callback;                  // +0x4c, sent when the tab is pressed
	bool Shortened;                      // +0x54

protected:
	void OnClose() override; // the destructor's sends and DeleteTextBuffer (issue #108)
};

// The tab bar: its own tabs and the two hidden windows with the tabs that do
// not fit. The child functions work on whichever of the three holds the tab
// (GetContainer); Moved is the tab being dragged, drawn last.
class TTabBar final : public TWindow {
public:
	TTabBar(const TTabBar &) = delete;
	TTabBar &operator=(const TTabBar &) = delete;
	TTabBar(); // 0x421b30, line 1016
	// Inline (0x417610, deleting destructor 0x417a60). The original's two
	// deletes, in its order (OnClose closes the two windows' tabs first).
	~TTabBar() override
	{
		LeftTabs.reset();
		RightTabs.reset();
	}
	void InsertChild(TComponent *Child) override;                            // +0x04, 0x41c2a0, line 1098
	void AppendChild(TComponent *Child) override;                            // +0x08, 0x41c450, line 1111
	void RemoveChild(TComponent *Child) override;                            // +0x0c, 0x422250, line 1177
	void DeleteChild(TComponent *Child) override;                            // +0x10, 0x422090, line 1125
	void InsertChildAfter(TComponent *Previous, TComponent *Child) override; // +0x14, 0x41c600, line 1139
	void SwapChildren(TComponent *Child1, TComponent *Child2) override;      // +0x18, 0x41c7b0, line 1163
	bool IsChild(TComponent *Child) override;                                // +0x1c, 0x41c950, line 1191
	void SetSize(int Width, int Height) override;                            // +0x34, 0x41bf40, line 1055
	// Inline (0x417700, folded with TTab's).
	int getMinimumHeight() override { return SkinProperties().StyleAA4.Data04; } // +0x40
	void Draw(int Surface, int x, int y) override;                               // +0x44, 0x41bd40, line 1043
	void Arrange() override;                                                     // +0x48, 0x41c0f0, line 1068
	bool HelpMouseMove(int x, int y) override;                                   // +0x68, 0x41cda0, lines 1215-1220
	void HandleEvent(int Event) override;                                        // +0x70, 0x41d040, line 1258
	virtual void Rearrange(TTab *_Moved);                                        // +0xa0, 0x41d810, lines 1372-1427

	void SetCurrentTab(TTab *Tab);            // 0x421d10, lines 1075-1085
	TWindow *GetContainer(TComponent *Child); // 0x41cb20, lines 1203-1205
	TTab *FindTab(TTalkBuffer *TextBuffer);   // 0x41d270, line 1290
	void ShowTab(TTab *Tab);                  // 0x41d450, line 1309
	void StoreOpenChannels();                 // 0x41d620, line 1344

	TTab *Moved;                        // +0x2c
	TTab *CurrentTab;                   // +0x30
	std::unique_ptr<TWindow> LeftTabs;  // +0x34 (issue #108: the bar is their only owner, they are not its children)
	std::unique_ptr<TWindow> RightTabs; // +0x38
	bool Busy;                          // +0x3c, Rearrange is running
	bool Changed;                       // +0x3d, the console updates its buttons when it is set

protected:
	void OnClose() override; // the two windows' tabs, then the bar's own (issue #108)
};

// The console. TalkMode is the volume of the default channel (1 say,
// 2 whisper, 3 yell), back to 1 after every message; History keeps the sent
// lines (HistoryPosition -1 is the edit line itself).
class TConsoleWindow : public TWindow {
public:
	TConsoleWindow(const TConsoleWindow &) = delete;
	TConsoleWindow &operator=(const TConsoleWindow &) = delete;
	TConsoleWindow();                             // 0x422410, line 1516
	~TConsoleWindow() override;                   // +0x00, 0x41dbe0, line 1543 (deleting 0x422aa0)
	void SetSize(int Width, int Height) override; // +0x34, 0x41de80, line 1571
	// Inline (0x41de70).
	int getMinimumHeight() override { return SkinProperties().Style9E8.Data00; } // +0x40
	void Draw(int Surface, int x, int y) override;                               // +0x44, 0x422ac0, lines 1583-1601
	void Arrange() override;                                                     // +0x48, 0x41e1e0, line 1647
	bool LeftButtonDown(int x, int y, int Flags) override;                       // +0x50, 0x41e5e0, line 1659
	bool HelpMouseMove(int x, int y) override;                                   // +0x68, 0x41e7a0, line 1673
	void KeyPress(int Key) override;                                             // +0x6c, 0x424580, lines 1718-1773
	void HandleEvent(int Event) override;                                        // +0x70, 0x424b10, lines 1856-1859

	void SetActive(bool Active) const;                                     // 0x41e960, lines 1869-1875
	void UpdateButtons();                                                  // 0x41ed00, lines 1888-1938
	void OpenDefaultChannel(TTalkBuffer *TextBuffer, const char *Caption); // 0x422e70, line 1953
	TTalkBuffer *OpenChannel(int RecvMode, int SendMode, const char *Name, int Channel,
		const char *Caption);                                                                    // 0x425070, lines 1965-1969
	void AddChannel(TTalkBuffer *TextBuffer, const char *Caption);                               // 0x423060, lines 1977-1998
	void CloseChannel();                                                                         // 0x4234b0, lines 2009-2020
	void ReportRuleViolation(const char *Question);                                              // 0x425330, lines 2033-2051
	[[nodiscard]] bool GetServerMessages() const;                                                // 0x41f1c0, lines 2060-2061
	void SetServerMessages(bool Show);                                                           // 0x41f470, lines 2071-2077
	[[nodiscard]] TTalkBuffer *GetTextBuffer() const;                                            // 0x41f7f0, line 2085
	void SendInput();                                                                            // 0x423830, lines 2145-2184
	void AddToHistory();                                                                         // 0x41f940, line 2207
	void ParseInput(char **Text, char **Addressee, int *TalkMode, char *MessageSeparator) const; // 0x41fb60, lines 2258-2304
	[[nodiscard]] bool SaveChannel() const;                                                      // 0x420090, lines 2316-2335

	TTabBar *TabBar;          // +0x2c
	int Data30;               // +0x30, not used
	TConsole *CurrentConsole; // +0x34
	TTab *DefaultTab;         // +0x38
	TEditLine *Input;         // +0x3c
	int SwitchTime;           // +0x40, new channels become current only from this action number on
	int HistoryPosition;      // +0x44
	// The input history, a plain TTalkBuffer (+0x48, issue #81). TTalkBuffer's destructor is not virtual (the
	// original's; Talk.cpp's DeleteTextBuffer destroys the derived buffers through it), so the deleter calls it
	// as a delete would, by its qualified name, which Clang's -Wdelete-non-abstract-non-virtual-dtor does not take for
	// a virtual call (issue #111).
	struct THistoryDelete {
		void operator()(TTalkBuffer *Buffer) const noexcept
		{
			Buffer->TTalkBuffer::~TTalkBuffer();
			::operator delete(Buffer);
		}
	};
	std::unique_ptr<TTalkBuffer, THistoryDelete> History;
	// The buttons shown by need (UpdateButtons) are owned by the child list while they are children and by the
	// member while they are not, so they stay raw (notes/ownership.md, section 7; issue #108).
	TMultiStateButton *VolumeButton;     // +0x4c, events 206-208
	TSkinButton *CloseButton;            // +0x50, event 202
	TToggleButton *ServerMessagesButton; // +0x54, event 203
	TSkinButton *IgnoreButton;           // +0x58, event 204
	TSkinButton *NewChannelButton;       // +0x5c, event 205
	TSkinButton *MoveRightButton;        // +0x60, the tab bar's event 5
	TSkinButton *MoveLeftButton;         // +0x64, the tab bar's event 6
	int TalkMode;                        // +0x68

protected:
	void OnClose() override; // the destructor's preferences and hotkeys, the buttons that are not children (issue #108)
};
