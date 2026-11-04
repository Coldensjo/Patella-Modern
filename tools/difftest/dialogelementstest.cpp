// Differential tests of DialogElements.cpp against the original code.
//
// Every round builds one kind of control on both sides, the original from the
// image's constructor (at its address, in pattern-filled memory) and ours with
// new, as the child of a root window (the image's TWindow for the original,
// ours for ours): separators, resizers, the three frames, the three labels,
// the formatted text, links, the three icons, the nine buttons, check boxes,
// the edit lines (both constructors and the password line, three of them in a
// focus group), the multi-line field, both scroll bars and both ranges, both
// list boxes and the progress bar. Both get the same random operations: sizes,
// Arrange, drawing, mouse moves, presses with releases, double and right
// clicks, the wheel (codes 1, 2 and the held button 6), hovering, keys, events
// and the members of each class (texts, colours, skins, radio groups, states,
// the editing members, ranges and values, lines and selections, the
// progress, the header's inline members against their out-of-line copies).
// After each operation the logs of every callee with an effect and a dump of
// the control (geometry, drawing list, every member, the children) are
// compared; at the end the roots are deleted.
//
// TComponent, TElement, TWindow and TCallback are the image's for the original
// objects and ours for ours. The text metrics and drawing (graphics.cpp), the
// surface, the mouse-over text, cursor, action state and number, messages,
// the clipboard and the clock are fakes for both sides; the callbacks go to a
// receiver per side that logs its events, the links' OpenURL is a logging
// function. The image's operator new and ours fill memory with the same
// pattern. Assertions are not exercised: the inputs keep to what the game
// does (square icons, frames and links no smaller than their parts, valid
// lines and values, Tab only in an active focus group).
// DIFFTEST_ONLY=DialogElements runs it alone; DE_TRACE=1 traces rounds and
// operations, DE_LOGS=<file> writes every compared log and dump.

#include "difftest.h"
#include "imageeh.h"
#include "../../src/DialogElements.h"
#include "../../src/GUI.h"
#include "../../src/Skins.h"
#include "../../src/Status.h"
#include "../../src/Surface.h"
#include "../../tests/golden/testsurface.h"
#include "../../src/Utils.h"
#include "../../src/graphics.h"
#include "../../src/sysinc.h"
#include <map>
#include <new>
#include <stdarg.h>
#include <string>
#include <vector>

static_assert(sizeof(TSeparator) == 0x28 && sizeof(TResizer) == 0x38 && sizeof(TBorder) == 0x68
	&& sizeof(TLabel) == 0x4c && sizeof(TFormattedText) == 0x38 && sizeof(TLink) == 0x3c
	&& sizeof(TOutfitIcon) == 0x48 && sizeof(TSkinIcon) == 0x28 && sizeof(TObjectIcon) == 0x50, "sizes 1");
static_assert(sizeof(TSkinButton) == 0x44 && sizeof(TColorButton) == 0x50 && sizeof(TIconButton) == 0x48
	&& sizeof(TBlinkingButton) == 0x5c && sizeof(TTextButton) == 0x78 && sizeof(TToggleButton) == 0x50
	&& sizeof(TTextToggleButton) == 0x84 && sizeof(TMultiStateButton) == 0x58 && sizeof(TCheckBox) == 0x34, "sizes 2");
static_assert(sizeof(TEditLine) == 0x68 && sizeof(TPasswordEditLine) == 0x6c && sizeof(TEditText) == 0x9c
	&& sizeof(TFocusGroup) == 0x28 && sizeof(THorizontalScrollBarRange) == 0x54 && sizeof(THorizontalScrollBar) == 0x38
	&& sizeof(TScrollBarRange) == 0x4c && sizeof(TScrollBar) == 0x38 && sizeof(TListBox) == 0x7c
	&& sizeof(TFontListBox) == 0xb0 && sizeof(TProgressBar) == 0x2c && sizeof(TListBoxLine) == 0x18, "sizes 3");
static_assert(offsetof(TEditLine, ReadOnly) == 0x66 && offsetof(TEditText, VisibleLines) == 0x98
	&& offsetof(TListBox, EditScroll) == 0x78 && offsetof(TFontListBox, FontLines) == 0x7c
	&& offsetof(THorizontalScrollBarRange, Callback) == 0x4c && offsetof(TSkinButton, Enabled) == 0x40, "layouts");

// --- Logs and names --------------------------------------------------------------------

enum { OURS = 0, ORIGINAL = 1 };
static int Side;
static bool Testing;
static std::string Log[2];
static FILE *LogFile;

// The error round (issue #34): the FailAt-th call of a fake (a logged call or
// a getter) throws an assertion of the calling side (0: none); Ticks counts
// one side's calls.
static int FailAt, Ticks;
static void Tick()
{
	Ticks++;
	if (FailAt > 0 && --FailAt == 0)
		ThrowSideAssertion(Side == ORIGINAL, -Ticks);
}

static void Logf(const char *Format, ...)
{
	if (!Testing)
		return;
	Tick();
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}
static void Add(std::string &Out, const char *Format, ...)
{
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Out += Buffer;
}
static void *VtableOf(const void *Object) { return *(void **)Object; }
template <class T> static void *Member(T Function) { return *(void **)&Function; }

// Image calls of members (thiscall through a fastcall pointer).
template <class R, class... A> static R CallI(uint32 Address, const void *This, A... Args)
{
	return ((R (__fastcall *)(const void *, void *, A...))(uintptr_t)Address)(This, NULL, Args...);
}

static std::map<const void *, std::string> Roles[2];
static std::map<void *, std::string> Kinds[2];
static void *OurElementVtable, *OurWindowVtable; // (the error round's image-side objects)
static std::string KindOf(int s, const void *Object)
{
	auto K = Kinds[s].find(VtableOf(Object));
	return K == Kinds[s].end() ? "?" : K->second;
}
static std::string Name(int s, const void *Object)
{
	if (Object == NULL)
		return "0";
	auto R = Roles[s].find(Object);
	if (R != Roles[s].end())
		return R->second;
	return KindOf(s, Object);
}
static std::string Name(const void *Object) { return Name(Side, Object); }
static std::string Str(const char *Text) { return Text == NULL ? "(null)" : std::string("'") + Text + "'"; }
// A tooltip: the toggle buttons pass their own unset member to the base class.
static std::string Tip(const char *Text)
{
	char Buffer[16];
	if ((uintptr_t)Text == 0xabababab) {
		sprintf(Buffer, "%p", (void *)Text);
		return Buffer;
	}
	return Str(Text);
}

// --- The world and the fakes ---------------------------------------------------------------

static struct {
	int ActionState;
	int ActionNumber;
	bool ClipboardEmpty;
	const char *Clipboard;
} World;

// Text metrics: characters 4-8 pixels wide by their code and the font.
static int CharWidth(int Font, char c) { return 4 + ((unsigned char)c + Font) % 5; }
static int __cdecl FakeGetTextPartWidth(int Font, const char *Text, int Length)
{
	Tick();
	int Width = 0;
	for (int i = 0; i < Length && Text[i] != 0; i++)
		Width += CharWidth(Font, Text[i]);
	return Width;
}
static int __cdecl FakeGetTextWidth(int Font, const char *Text) { Tick(); return FakeGetTextPartWidth(Font, Text, 1 << 30); }
static int __cdecl FakeGetFittingTextLength(int Font, const char *Text, int Width)
{
	Tick();
	int Length = 0, Used = 0;
	while (Text[Length] != 0 && Used + CharWidth(Font, Text[Length]) <= Width)
		Used += CharWidth(Font, Text[Length++]);
	return Length;
}
static int __cdecl FakeGetFontHeight(int Font) { Tick(); return 8 + (Font & 3); }
static int __cdecl FakeGetFontLineHeight(int Font) { Tick(); return 11 + (Font & 3); }
static void __cdecl FakeGetTextPosition(int *TextX, int *TextY, int x, int y, int Width, int Height, TFont *Format)
{
	Tick();
	*TextX = x + Width / 3 + Format->Alignment;
	*TextY = y + Height / 4 + Format->FontNumber;
}
static void __cdecl FakePrintTextClipped(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Alignment, TRect Clip)
{
	Logf("PrintC%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d;", Surface, x, y, FontNumber, Color.R, Color.G, Color.B, Text,
		Alignment, Clip.x, Clip.y, Clip.width, Clip.height);
}
static void __cdecl FakePrintFormattedText(int Surface, int x, int y, TFont F, const char *Text)
{
	Logf("PrintF%d,%d,%d,%d,%d,%d,%d,%d,%s;", Surface, x, y, F.FontNumber, F.Color.R, F.Color.G, F.Color.B, F.Alignment,
		Text);
}
static void __cdecl FakePrintFormattedTextPart(int Surface, int x, int y, TFont F, const char *Text, int Length)
{
	std::string Part(Text, Length > 0 ? Length : 0);
	Logf("PrintP%d,%d,%d,%d,%d,%d,%d,%d,%s,%d;", Surface, x, y, F.FontNumber, F.Color.R, F.Color.G, F.Color.B,
		F.Alignment, Part.c_str(), Length);
}
static void __cdecl FakeDrawObject(int Surface, int x, int y, int Size, TObject Obj, TRGBColor H, TRect Clip, TFont F,
	bool ShowCount)
{
	Logf("Obj%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Size, Obj.Type, Obj.Data, Obj.Data2,
		H.R, H.G, H.B, Clip.x, Clip.y, Clip.width, Clip.height, F.FontNumber, ShowCount ? 1 : 0);
}
static void __cdecl FakeDrawOutfit(int Surface, int x, int y, int Size, int Outfit, int *Colors, TRGBColor H, TRect Clip,
	TFont F)
{
	Logf("Outfit%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Size, Outfit, Colors[0],
		Colors[1], Colors[2], Colors[3], H.R, H.G, H.B, Clip.x, Clip.y, Clip.width, Clip.height, F.FontNumber, F.Alignment);
}
static void __cdecl FakeDrawPicturePart(int Surface, int x, int y, int Picture, int SourceX, int SourceY, int Width,
	int Height)
{
	Logf("Pic%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Picture, SourceX, SourceY, Width, Height);
}
static void __cdecl FakeSetMouseOverText(const char *Text) { Logf("Over%s;", Text == NULL ? "-" : Text); }
static void __cdecl FakeSetMouseCursor(int Cursor) { Logf("Cursor%d;", Cursor); }
static int __cdecl FakeGetActionState() { Tick(); return World.ActionState; }
static int __cdecl FakeGetActionNumber() { Tick(); return World.ActionNumber; }
static void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	Logf("Msg%d,%s,%d;", Type, Name((void *)Param1).c_str(), (int)Param2);
}
static void __cdecl FakeCheckClipboardText(const char *Text, int Length)
{
	Logf("Clip%s,%d;", std::string(Text, Length > 0 ? Length : 0).c_str(), Length);
}
static bool __cdecl FakeIsClipboardEmpty() { Tick(); return World.ClipboardEmpty; }
static char *__cdecl FakeGetClipboardText() { Tick(); return (char *)World.Clipboard; }
// sysinc.cpp is not linked: our side's definition.
char *GetClipboardText() { return FakeGetClipboardText(); }

// The surface: the two calls DialogElements.cpp makes.
class TDESurface : public TTestSurface {
public:
	void FillRectangle(int Surface, TRect R, TRGBColor C)
	{
		Logf("Fill%d,%d,%d,%d,%d,%d,%d,%d;", Surface, R.x, R.y, R.width, R.height, C.R, C.G, C.B);
	}
	void DrawVerticalLine(int Surface, int x, int y, int Height, TRGBColor C)
	{
		Logf("VLine%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Height, C.R, C.G, C.B);
	}
};
static TDESurface *Surface;
static TSurface *__cdecl FakeGetSurface() { Tick(); return Surface; }

static void __cdecl FakeOpenURL(const char *URL) { Logf("URL%s;", URL); }

static int __cdecl ImageSprintf(char *Buffer, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vsprintf(Buffer, Format, Args);
	va_end(Args);
	return Result;
}

// New memory of both sides gets the same byte pattern.
static void *__cdecl PatternNew(size_t Size)
{
	void *Memory = malloc(Size == 0 ? 1 : Size);
	memset(Memory, 0xab, Size);
	return Memory;
}

// The receivers of the callbacks and the targets of resizers and frames.
class TDEReceiver : public TElement {
public:
	void HandleEvent(int Event) { Logf("Event%s,%d;", Name(this).c_str(), Event); }
};
class TDETarget : public TWindow {
public:
	void SetSize(int _Width, int _Height)
	{
		Logf("Size%s,%d,%d;", Name(this).c_str(), _Width, _Height);
		TComponent::SetSize(_Width < 0 ? 0 : _Width, _Height < 0 ? 0 : _Height);
	}
	void SetPosition(int x, int y)
	{
		Logf("Pos%s,%d,%d;", Name(this).c_str(), x, y);
		TWindow::SetPosition(x, y);
	}
	void Arrange() { Logf("Arr%s;", Name(this).c_str()); }
};

// --- Patches -----------------------------------------------------------------------------

struct TSaved {
	uint8 *Code;
	uint8 Bytes[5];
};
static std::vector<TSaved> SavedCode;
static void Redirect(void *Function, void *Target)
{
	TSaved S;
	S.Code = (uint8 *)Function;
	DWORD Protection;
	VirtualProtect(Function, 5, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(S.Bytes, Function, 5);
	PatchJump((uint32)(uintptr_t)Function, Target);
	VirtualProtect(Function, 5, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Function, 5);
	SavedCode.push_back(S);
}
static void RestoreAll()
{
	for (size_t i = SavedCode.size(); i-- > 0;) {
		DWORD Protection;
		VirtualProtect(SavedCode[i].Code, 5, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(SavedCode[i].Code, SavedCode[i].Bytes, 5);
		VirtualProtect(SavedCode[i].Code, 5, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), SavedCode[i].Code, 5);
	}
	SavedCode.clear();
}

static void PatchCallees()
{
	struct { uint32 Image; void *Ours; void *Fake; } Table[] = {
		{0x491b80, (void *)GetTextPartWidth, (void *)FakeGetTextPartWidth},
		{0x494570, (void *)GetTextWidth, (void *)FakeGetTextWidth},
		{0x491f50, (void *)GetFittingTextLength, (void *)FakeGetFittingTextLength},
		{0x491710, (void *)GetFontHeight, (void *)FakeGetFontHeight},
		{0x48b430, (void *)GetFontLineHeight, (void *)FakeGetFontLineHeight},
		{0x492260, (void *)GetTextPosition, (void *)FakeGetTextPosition},
		{0x4934e0, (void *)PrintTextClipped, (void *)FakePrintTextClipped},
		{0x493f60, (void *)PrintFormattedText, (void *)FakePrintFormattedText},
		{0x4943b0, (void *)PrintFormattedTextPart, (void *)FakePrintFormattedTextPart},
		{0x494ca0, (void *)DrawObject, (void *)FakeDrawObject},
		{0x495080, (void *)DrawOutfit, (void *)FakeDrawOutfit},
		{0x490c60, (void *)DrawPicturePart, (void *)FakeDrawPicturePart},
		{0x4eaa90, (void *)GetSurface, (void *)FakeGetSurface},
		{0x4e72e0, (void *)SetMouseOverText, (void *)FakeSetMouseOverText},
		{0x4e6e80, (void *)SetMouseCursor, (void *)FakeSetMouseCursor},
		{0x4e6e30, (void *)GetActionState, (void *)FakeGetActionState},
		{0x4e6e00, (void *)GetActionNumber, (void *)FakeGetActionNumber},
		{0x4e9020, (void *)AddMessage, (void *)FakeAddMessage},
		{0x4ea370, (void *)CheckClipboardText, (void *)FakeCheckClipboardText},
		// Ours of IsClipboardEmpty is contextmenustest.cpp's, of GetClipboardText this file's.
		{0x4fcbe0, (void *)IsClipboardEmpty, (void *)FakeIsClipboardEmpty},
		{0x4fcff0, NULL, (void *)FakeGetClipboardText},
		{0x511d6b, NULL, (void *)ImageSprintf},
		{0x5237e7, (void *)(void *(__cdecl *)(size_t))operator new, (void *)PatternNew},
		{0x523817, (void *)(void *(__cdecl *)(size_t))operator new[], (void *)PatternNew},
	};
	for (auto &T : Table) {
		Redirect((void *)(uintptr_t)T.Image, T.Fake);
		if (T.Ours != NULL)
			Redirect(T.Ours, T.Fake);
	}
}

// --- The kinds of controls -----------------------------------------------------------------

enum {
	K_SEPARATOR, K_RESIZER, K_BORDER, K_LABEL, K_FORMATTED, K_LINK, K_OUTFIT, K_SKINICON, K_OBJECTICON,
	K_SKINBUTTON, K_COLORBUTTON, K_ICONBUTTON, K_BLINKING, K_TEXTBUTTON, K_TOGGLE, K_TEXTTOGGLE, K_MULTI,
	K_CHECKBOX, K_EDIT, K_EDITTEXT, K_HSCROLL, K_VSCROLL, K_HRANGE, K_VRANGE, K_LIST, K_FONTLIST, K_PROGRESS,
	KINDS
};
static const char *const KindNames[KINDS] = {"separator", "resizer", "border", "label", "formatted text", "link",
	"outfit icon", "skin icon", "object icon", "skin button", "colour button", "icon button", "blinking button",
	"text button", "toggle button", "text toggle button", "multi-state button", "check box", "edit lines",
	"edit text", "horizontal scroll bar", "scroll bar", "horizontal range", "range", "list box", "font list box",
	"progress bar"};
static const struct { uint32 Vtable; const char *Kind; } ImageVtables[] = {
	{0x560bb8, "Label"}, {0x560c40, "FormattedText"}, {0x560ce8, "Link"}, {0x560d88, "ObjectIcon"},
	{0x560e10, "MultiState"}, {0x560e98, "EditLine"}, {0x560f60, "Password"}, {0x561028, "Progress"},
	{0x5610b0, "Separator"}, {0x561138, "ListBox"}, {0x5611e0, "FontListBox"}, {0x5612b0, "Resizer"},
	{0x561358, "Border"}, {0x5614f8, "SkinIcon"}, {0x5617e0, "HRange"}, {0x561868, "VRange"},
	{0x561970, "OutfitIcon"}, {0x5619f8, "SkinButton"}, {0x561a80, "ColorButton"}, {0x561b08, "IconButton"},
	{0x561b90, "Blinking"}, {0x561c40, "TextButton"}, {0x561cd0, "Toggle"}, {0x561d58, "TextToggle"},
	{0x561de8, "CheckBox"}, {0x561ed0, "HScroll"}, {0x561f70, "VScroll"}, {0x562040, "EditText"},
	{0x564528, "Window"},
};

static int Kind;
static TWindow *Root[2];
static TComponent *Control[2];
static TComponent *Extra[2][2];        // the other edit lines of the focus group
static TFocusGroup *Group[2];
static TRadioButtonGroup *Radio[2];
static TDEReceiver *Receiver[2];
static TDETarget *Target[2];

static const char *const Texts[] = {
	"Hello", "Hello world, this is a longer text", "two\nlines", "a\nb\nc", "", "x",
	"Tibia rules the world of words and spaces", "<b>bold</b> and <n>normal", "<font=2>font two<br=5>break",
	"<color=100>col <hspace=10> spaced <indent=20> indented\nnext line", "<skin=12>icon text", "<skin=999>bad skin",
	"<red=200><green=10><blue=30>rgb <h>head", "bad <x token", "<font=x", "<b>", "word-word.word,word;word",
	"Line one\nLine two is longer than the first\n\nfour", "<color=101>pink <color=17>blue",
	"averyveryverylongwordwithoutanyspacesinsideitatall",
};
static const char *RandomText() { return Texts[Random() % (sizeof(Texts) / sizeof(Texts[0]))]; }
static const char *const Tooltips[] = {"Tip one", "Tip two", "Third tip", NULL};
static const char *RandomTooltip() { return Tooltips[Random() % 4]; }
static TFont RandomFont()
{
	TFont F;
	F.FontNumber = Random() % 5;
	F.Color = TRGBColor(Random() % 256, Random() % 256, Random() % 256);
	F.Alignment = Random() % 3;
	return F;
}
static int RandomSkin() { return Random() % 250; }

// --- Building ---------------------------------------------------------------------------

typedef void *(__fastcall *CtorFn)(void *, void *);
static void *ImageWindow() { return At<CtorFn>(0x496080)(PatternNew(sizeof(TWindow)), NULL); }

struct TArguments {
	int Variant;
	int x, y, a, b, c, d, e, f, g, h, i;
	int Skins[9];
	bool Flag1, Flag2, Flag3;
	const char *Text, *Text2, *Tooltip1, *Tooltip2;
	TFont Font;
	TRGBColor Color;
	int Colors[4];
	TObject Obj;
};

static TArguments RandomArguments()
{
	TArguments A;
	A.Variant = Random() % 3;
	A.x = Random() % 30;
	A.y = Random() % 30;
	A.a = Random() % 40;
	A.b = A.a + 1 + Random() % 40;
	A.c = A.a + Random() % (A.b - A.a + 1);
	A.d = Random() % 30;
	A.e = 1 + Random() % 10;
	A.f = 20 + Random() % 40;
	A.g = Random() % 3;
	A.h = Random() % 10;
	A.i = Random() % 4;
	for (int i = 0; i < 9; i++)
		A.Skins[i] = 26 + Random() % 20;
	// The frames get the game's skin sets (the side bar's, the start window's).
	static const int Frames[2][9] = {{26, 32, 27, 30, 31, 28, 33, 29, 7}, {0x34, 0x3a, 0x35, 0x38, 0x39, 0x36, 0x3b, 0x37, 0x3c}};
	if (Kind == K_BORDER) {
		int Set = Random() % 2;
		for (int i = 0; i < 9; i++)
			A.Skins[i] = Frames[Set][i];
	}
	A.Flag1 = Random() % 2 != 0;
	A.Flag2 = Random() % 2 != 0;
	A.Flag3 = Random() % 2 != 0;
	A.Text = RandomText();
	A.Text2 = Random() % 2 ? "Button" : "Open";
	A.Tooltip1 = RandomTooltip();
	A.Tooltip2 = RandomTooltip();
	A.Font = RandomFont();
	A.Color = TRGBColor(Random() % 256, Random() % 256, Random() % 256);
	for (int i = 0; i < 4; i++)
		A.Colors[i] = Random() % 133;
	A.Obj.Type = 100 + Random() % 3000;
	A.Obj.Data = Random() % 100;
	A.Obj.Data2 = Random() % 3;
	return A;
}

static const char *ShortText(const char *Text) { return strlen(Text) < 50 ? Text : "Long"; }
// A check box always has a text (its label must have a width).
static const char *BoxText(const char *Text) { return Text[0] == 0 || Text[0] == '<' ? "Check" : ShortText(Text); }

static TComponent *Build(int s, const TArguments &A)
{
	TCallback CB(Receiver[s], 1 + A.h);
	TCallback CB2(Receiver[s], 100 + A.h);
	bool O = s == OURS;
	switch (Kind) {
	case K_SEPARATOR:
		return O ? (TComponent *)new TSeparator(A.Flag1)
			: (TComponent *)CallI<void *>(0x452cb0, PatternNew(0x28), A.Flag1);
	case K_RESIZER:
		return O ? (TComponent *)new TResizer(Target[s], A.Flag1, A.Flag2, A.Flag3)
			: (TComponent *)CallI<void *>(0x453be0, PatternNew(0x38), (TComponent *)Target[s], A.Flag1, A.Flag2, A.Flag3);
	case K_BORDER:
		if (A.Variant == 0)
			return O ? (TComponent *)new TBorder(Target[s], A.g)
				: (TComponent *)CallI<void *>(0x4547f0, PatternNew(0x68), (TWindow *)Target[s], A.g);
		if (A.Variant == 1)
			return O ? (TComponent *)new TBorder(Target[s], A.Skins[0], A.Skins[1], A.Skins[2], A.Skins[3], A.Skins[4],
				A.Skins[5], A.Skins[6], A.Skins[7], A.g)
				: (TComponent *)CallI<void *>(0x454b10, PatternNew(0x68), (TWindow *)Target[s], A.Skins[0], A.Skins[1],
					A.Skins[2], A.Skins[3], A.Skins[4], A.Skins[5], A.Skins[6], A.Skins[7], A.g);
		return O ? (TComponent *)new TBorder(Target[s], A.Skins[0], A.Skins[1], A.Skins[2], A.Skins[3], A.Skins[4],
			A.Skins[5], A.Skins[6], A.Skins[7], A.Flag1 ? A.Skins[8] : 0, A.g)
			: (TComponent *)CallI<void *>(0x454e30, PatternNew(0x68), (TWindow *)Target[s], A.Skins[0], A.Skins[1],
				A.Skins[2], A.Skins[3], A.Skins[4], A.Skins[5], A.Skins[6], A.Skins[7], A.Flag1 ? A.Skins[8] : 0, A.g);
	case K_LABEL:
		if (A.Variant == 0)
			return O ? (TComponent *)new TLabel(A.x, A.y, 30 + A.f, 10 + A.d, A.Font, A.Text, A.Flag1)
				: (TComponent *)CallI<void *>(0x465510, PatternNew(0x4c), A.x, A.y, 30 + A.f, 10 + A.d, A.Font, A.Text,
					A.Flag1);
		if (A.Variant == 1)
			return O ? (TComponent *)new TLabel(A.x, A.y, A.Font, A.Text)
				: (TComponent *)CallI<void *>(0x465810, PatternNew(0x4c), A.x, A.y, A.Font, A.Text);
		return O ? (TComponent *)new TLabel(A.Font, A.Text)
			: (TComponent *)CallI<void *>(0x465b40, PatternNew(0x4c), A.Font, A.Text);
	case K_FORMATTED:
		return O ? (TComponent *)new TFormattedText(A.Text)
			: (TComponent *)CallI<void *>(0x465e60, PatternNew(0x38), A.Text);
	case K_LINK:
		return O ? (TComponent *)new TLink(A.Text, A.Text2, "http://www.tibia.com/", FakeOpenURL, A.Font)
			: (TComponent *)CallI<void *>(0x46b030, PatternNew(0x3c), A.Text, A.Text2, "http://www.tibia.com/",
				(void (*)(const char *))FakeOpenURL, A.Font);
	case K_OUTFIT:
		return O ? (TComponent *)new TOutfitIcon(A.a, A.i, (int *)A.Colors, &A.Color)
			: (TComponent *)CallI<void *>(0x4666a0, PatternNew(0x48), A.a, A.i, (int *)A.Colors, &A.Color);
	case K_SKINICON:
		return O ? (TComponent *)new TSkinIcon(A.Skins[0])
			: (TComponent *)CallI<void *>(0x457710, PatternNew(0x28), A.Skins[0]);
	case K_OBJECTICON:
		return O ? (TComponent *)new TObjectIcon(A.Obj, &A.Color, A.Flag1)
			: (TComponent *)CallI<void *>(0x4526c0, PatternNew(0x50), A.Obj, &A.Color, A.Flag1);
	case K_SKINBUTTON:
		return O ? (TComponent *)new TSkinButton(A.x, A.y, A.Skins[0], A.Skins[1], CB, A.Tooltip1, A.Flag1)
			: (TComponent *)CallI<void *>(0x466860, PatternNew(0x44), A.x, A.y, A.Skins[0], A.Skins[1], CB, A.Tooltip1,
				A.Flag1);
	case K_COLORBUTTON:
		return O ? (TComponent *)new TColorButton(A.x, A.y, A.Color, A.Skins[0], A.Skins[1], CB, A.Tooltip1)
			: (TComponent *)CallI<void *>(0x466a60, PatternNew(0x50), A.x, A.y, A.Color, A.Skins[0], A.Skins[1], CB,
				A.Tooltip1);
	case K_ICONBUTTON:
		return O ? (TComponent *)new TIconButton(A.x, A.y, A.Skins[2], A.Skins[0], A.Skins[1], CB, A.Tooltip1)
			: (TComponent *)CallI<void *>(0x466ad0, PatternNew(0x48), A.x, A.y, A.Skins[2], A.Skins[0], A.Skins[1], CB,
				A.Tooltip1);
	case K_BLINKING:
		return O ? (TComponent *)new TBlinkingButton(A.x, A.y, A.Skins[0], A.Skins[1], A.Skins[2], A.Skins[3], CB,
			A.Tooltip1)
			: (TComponent *)CallI<void *>(0x466b20, PatternNew(0x5c), A.x, A.y, A.Skins[0], A.Skins[1], A.Skins[2],
				A.Skins[3], CB, A.Tooltip1);
	case K_TEXTBUTTON:
		if (A.Variant == 0)
			return O ? (TComponent *)new TTextButton(A.x, A.y, ShortText(A.Text), A.Skins[0], A.Skins[1], CB, A.Tooltip1)
				: (TComponent *)CallI<void *>(0x466bc0, PatternNew(0x78), A.x, A.y, ShortText(A.Text), A.Skins[0],
					A.Skins[1], CB, A.Tooltip1);
		return O ? (TComponent *)new TTextButton(A.x, A.y, ShortText(A.Text), CB, A.Tooltip1)
			: (TComponent *)CallI<void *>(0x466ed0, PatternNew(0x78), A.x, A.y, ShortText(A.Text), CB, A.Tooltip1);
	case K_TOGGLE:
		return O ? (TComponent *)new TToggleButton(A.x, A.y, A.Skins[0], A.Skins[1], CB, A.Tooltip1, A.Tooltip2, A.Flag1)
			: (TComponent *)CallI<void *>(0x4671e0, PatternNew(0x50), A.x, A.y, A.Skins[0], A.Skins[1], CB, A.Tooltip1,
				A.Tooltip2, A.Flag1);
	case K_TEXTTOGGLE:
		return O ? (TComponent *)new TTextToggleButton(A.x, A.y, ShortText(A.Text), A.Skins[0], A.Skins[1], CB,
			A.Tooltip1, A.Tooltip2, A.Flag1)
			: (TComponent *)CallI<void *>(0x467230, PatternNew(0x84), A.x, A.y, ShortText(A.Text), A.Skins[0], A.Skins[1],
				CB, A.Tooltip1, A.Tooltip2, A.Flag1);
	case K_MULTI: {
		TCallback CB3(Receiver[s], 200 + A.h);
		if (O)
			return new TMultiStateButton(A.x, A.y, A.Tooltip1, 3, A.Skins[0], A.Skins[1], CB, A.Skins[2], A.Skins[3], CB2,
				A.Skins[4], A.Skins[5], CB3);
		typedef void *(__cdecl *MultiFn)(void *, int, int, const char *, int, int, int, TCallback, ...);
		return (TComponent *)At<MultiFn>(0x4672d0)(PatternNew(0x58), A.x, A.y, A.Tooltip1, 3, A.Skins[0], A.Skins[1], CB,
			A.Skins[2], A.Skins[3], CB2, A.Skins[4], A.Skins[5], CB3);
	}
	case K_CHECKBOX:
		return O ? (TComponent *)new TCheckBox(BoxText(A.Text))
			: (TComponent *)CallI<void *>(0x467440, PatternNew(0x34), BoxText(A.Text));
	case K_EDIT:
		if (A.Variant == 0)
			return O ? (TComponent *)new TEditLine(TRect(A.x, A.y, 60 + A.f, 16), A.f, A.Flag1)
				: (TComponent *)CallI<void *>(0x467b50, PatternNew(0x68), TRect(A.x, A.y, 60 + A.f, 16), A.f, A.Flag1);
		if (A.Variant == 1)
			return O ? (TComponent *)new TEditLine(A.f, A.Flag1)
				: (TComponent *)CallI<void *>(0x45a4c0, PatternNew(0x68), A.f, A.Flag1);
		return O ? (TComponent *)new TPasswordEditLine(A.f)
			: (TComponent *)CallI<void *>(0x45cb50, PatternNew(0x6c), A.f);
	case K_EDITTEXT:
		return O ? (TComponent *)new TEditText(40 + A.f * 2, A.Flag1)
			: (TComponent *)CallI<void *>(0x46b570, PatternNew(0x9c), 40 + A.f * 2, A.Flag1);
	case K_HSCROLL:
		return O ? (TComponent *)new THorizontalScrollBar(A.a, A.b, A.c, A.d, A.e, CB)
			: (TComponent *)CallI<void *>(0x4692d0, PatternNew(0x38), A.a, A.b, A.c, A.d, A.e, CB);
	case K_VSCROLL:
		return O ? (TComponent *)new TScrollBar(TRect(A.x, A.y, 12, 40 + A.f), A.a, A.b, A.c, A.d, A.e)
			: (TComponent *)CallI<void *>(0x469a80, PatternNew(0x38), TRect(A.x, A.y, 12, 40 + A.f), A.a, A.b, A.c, A.d,
				A.e);
	case K_HRANGE:
		return O ? (TComponent *)new THorizontalScrollBarRange(A.a, A.b, A.c, A.d, CB)
			: (TComponent *)CallI<void *>(0x45e9e0, PatternNew(0x54), A.a, A.b, A.c, A.d, CB);
	case K_VRANGE:
		return O ? (TComponent *)new TScrollBarRange(A.x, A.y, 12, 30 + A.f, A.a, A.b, A.c, A.d)
			: (TComponent *)CallI<void *>(0x469670, PatternNew(0x4c), A.x, A.y, 12, 30 + A.f, A.a, A.b, A.c, A.d);
	case K_LIST:
		return O ? (TComponent *)new TListBox(CB, CB2)
			: (TComponent *)CallI<void *>(0x469e70, PatternNew(0x7c), CB, CB2);
	case K_FONTLIST:
		return O ? (TComponent *)new TFontListBox(CB, CB2)
			: (TComponent *)CallI<void *>(0x46afc0, PatternNew(0xb0), CB, CB2);
	case K_PROGRESS:
		return O ? (TComponent *)new TProgressBar : (TComponent *)CallI<void *>(0x452a00, PatternNew(0x2c));
	}
	return NULL;
}

// --- Dumps -------------------------------------------------------------------------------

template <class T> static bool Entry(vector<T> &V, int i, T *Out)
{
	if (i < V.min || i > V.max)
		return false;
	*Out = V.entry[i - V.start];
	return true;
}
static void DumpFont(std::string &Out, const TFont &F)
{
	Add(Out, " font %d,%d,%d,%d,%d", F.FontNumber, F.Color.R, F.Color.G, F.Color.B, F.Alignment);
}
static void DumpCallback(std::string &Out, int s, const TCallback &C)
{
	Add(Out, " cb %s/%d", Name(s, C.Receiver).c_str(), C.Event);
}
static void DumpButton(std::string &Out, int s, TSkinButton *B)
{
	Add(Out, " skins %d,%d pressed %d inside %d repeat %d group %d tip %s enabled %d", B->ButtonUp, B->ButtonDown,
		B->Pressed, B->MouseInside, B->Repeat, B->RadioButtonGroup != NULL ? 1 : 0, Tip(B->Tooltip).c_str(), B->Enabled);
	DumpCallback(Out, s, B->Callback);
}
static void DumpEdit(std::string &Out, int s, TEditLine *E)
{
	Add(Out, " text %s max %d cursor %d single %d group %d first %d active %d sel %d,%d selecting %d dis %d ro %d",
		Str(E->Text).c_str(), E->MaxLength, E->Cursor, E->SingleLine, E->FocusGroup != NULL ? 1 : 0, E->FirstVisible,
		E->Active, E->SelectionStart, E->SelectionLength, E->Selecting, E->Disabled, E->ReadOnly);
	DumpFont(Out, E->Font);
}
static void DumpRange(std::string &Out, int Step, int Min, int Max, int Value, int Visible, bool Dragging, bool Pressed,
	int DragOffset, int SliderSize, int SliderPosition, bool PageScroll, bool Enabled)
{
	Add(Out, " step %d range %d,%d value %d visible %d drag %d pressed %d off %d slider %d at %d page %d enabled %d", Step,
		Min, Max, Value, Visible, Dragging, Pressed, DragOffset, SliderSize, SliderPosition, PageScroll, Enabled);
}

static void Dump(std::string &Out, int s, TComponent *C, int Depth);
static void DumpChildren(std::string &Out, int s, TWindow *W, int Depth)
{
	Add(Out, " cap %s", Name(s, W->Capture).c_str());
	for (TComponent *Child = W->FirstChild; Child != NULL; Child = Child->Next)
		Dump(Out, s, Child, Depth + 1);
}

static void Dump(std::string &Out, int s, TComponent *C, int Depth)
{
	Out += "\n" + std::string(Depth * 2, ' ');
	std::string K = KindOf(s, C);
	Add(Out, "%s %d,%d %dx%d parent %s", Name(s, C).c_str(), C->OffsetX, C->OffsetY, C->Width, C->Height,
		Name(s, C->Parent).c_str());
	for (int i = 0; i < C->DrawingListLength && C->DrawingList != NULL; i++) {
		TDrawing &D = C->DrawingList[i];
		Add(Out, " {%d,%d,%d,%d,%d,%d,%d}", D.skin, D.x, D.y, D.width, D.height, D.srcx, D.srcy);
	}
	if (K == "Separator") {
		Add(Out, " horizontal %d", ((TSeparator *)C)->Horizontal);
	} else if (K == "Resizer") {
		TResizer *R = (TResizer *)C;
		Add(Out, " to %s drag %d %d,%d resize %d,%d invisible %d", Name(s, R->ToResize).c_str(), R->Dragging, R->StartX,
			R->StartY, R->ResizeWidth, R->ResizeHeight, R->Invisible);
	} else if (K == "Border") {
		TBorder *B = (TBorder *)C;
		Add(Out, " window %s skins %d,%d,%d,%d,%d,%d,%d,%d,%d area %d,%d,%d,%d border %d", Name(s, B->Window).c_str(),
			B->TopLeft, B->Top, B->TopRight, B->Left, B->Right, B->BottomLeft, B->Bottom, B->BottomRight, B->Background,
			B->WindowArea.x, B->WindowArea.y, B->WindowArea.width, B->WindowArea.height, B->Border);
	} else if (K == "Label") {
		TLabel *L = (TLabel *)C;
		Add(Out, " full %s lines %d short %d", Str(L->FullText).c_str(), L->Lines, L->Shortened);
		for (int i = 0; i < L->Lines; i++)
			Add(Out, " [%d]%s", (int)(L->Line[i] - L->Text), Str(L->Line[i]).c_str());
		DumpFont(Out, L->Font);
	} else if (K == "FormattedText") {
		TFormattedText *F = (TFormattedText *)C;
		Add(Out, " text %s auto %d,%d", Str(F->Text).c_str(), F->AutoWidth, F->AutoHeight);
	} else if (K == "Link") {
		TLink *L = (TLink *)C;
		Add(Out, " dest %s url %d button %s label %s", Str(L->Destination).c_str(), L->OpenURL == FakeOpenURL ? 1 : 0,
			Name(s, L->Button).c_str(), Name(s, L->Label).c_str());
	} else if (K == "OutfitIcon") {
		TOutfitIcon *O = (TOutfitIcon *)C;
		Add(Out, " outfit %d colors %d,%d,%d,%d dir %d hl %d,%d,%d", O->Outfit, O->Colors[0], O->Colors[1], O->Colors[2],
			O->Colors[3], O->Direction, O->Highlight.R, O->Highlight.G, O->Highlight.B);
	} else if (K == "SkinIcon") {
		Add(Out, " skin %d", ((TSkinIcon *)C)->Skin);
	} else if (K == "ObjectIcon") {
		TObjectIcon *O = (TObjectIcon *)C;
		Add(Out, " unused %d,%d,%d,%d framed %d hl %d,%d,%d obj %d,%d,%d", O->Unused24.x, O->Unused24.y,
			O->Unused24.width, O->Unused24.height, O->Framed, O->Highlight.R, O->Highlight.G, O->Highlight.B, O->Obj.Type,
			O->Obj.Data, O->Obj.Data2);
	} else if (K == "SkinButton" || K == "ColorButton" || K == "IconButton" || K == "Blinking" || K == "TextButton"
		|| K == "Toggle" || K == "TextToggle" || K == "MultiState") {
		DumpButton(Out, s, (TSkinButton *)C);
		if (K == "ColorButton") {
			TColorButton *B = (TColorButton *)C;
			Add(Out, " color %d,%d,%d", B->Color.R, B->Color.G, B->Color.B);
		} else if (K == "IconButton") {
			Add(Out, " icon %d", ((TIconButton *)C)->Icon);
		} else if (K == "Blinking") {
			TBlinkingButton *B = (TBlinkingButton *)C;
			Add(Out, " next %d phase %d %d,%d,%d,%d", B->NextChange, B->Phase, B->ButtonUp1, B->ButtonUp2, B->ButtonDown1,
				B->ButtonDown2);
		} else if (K == "TextButton" || K == "TextToggle") {
			Add(Out, " text %s", Str(((TTextButton *)C)->Text).c_str());
			if (K == "TextToggle") {
				TTextToggleButton *B = (TTextToggleButton *)C;
				Add(Out, " state %d tips %s,%s", B->State, Str(B->Tooltip1).c_str(), Str(B->Tooltip2).c_str());
			}
		} else if (K == "Toggle") {
			TToggleButton *B = (TToggleButton *)C;
			Add(Out, " state %d tips %s,%s", B->State, Str(B->Tooltip1).c_str(), Str(B->Tooltip2).c_str());
		} else if (K == "MultiState") {
			TMultiStateButton *B = (TMultiStateButton *)C;
			Add(Out, " state %d of %d", B->State, B->States);
			for (int i = 0; i < B->States; i++) {
				Add(Out, " %d,%d", B->ButtonUps[i], B->ButtonDowns[i]);
				DumpCallback(Out, s, B->Callbacks[i]);
			}
		}
	} else if (K == "CheckBox") {
		TCheckBox *B = (TCheckBox *)C;
		Add(Out, " label %s checked %d enabled %d", Name(s, B->Label).c_str(), B->Checked, B->Enabled);
	} else if (K == "EditLine" || K == "Password" || K == "EditText") {
		DumpEdit(Out, s, (TEditLine *)C);
		if (K == "Password") {
			Add(Out, " stars %s", Str(((TPasswordEditLine *)C)->Stars).c_str());
		} else if (K == "EditText") {
			TEditText *E = (TEditText *)C;
			Add(Out, " lines %d field %d,%d visible %d starts", E->Lines, E->FieldWidth, E->FieldHeight,
				E->VisibleLines);
			for (int i = 0; i <= E->Lines && i < 200; i++) {
				int Start;
				if (Entry(E->LineStart, i, &Start))
					Add(Out, " %d", Start);
			}
		}
	} else if (K == "HRange") {
		THorizontalScrollBarRange *R = (THorizontalScrollBarRange *)C;
		DumpRange(Out, R->ButtonRange, R->Min, R->Max, R->Value, R->Visible, R->Dragging, R->Pressed, R->DragOffset,
			R->SliderWidth, R->SliderPosition, R->PageScroll, R->Enabled);
		DumpCallback(Out, s, R->Callback);
	} else if (K == "VRange") {
		TScrollBarRange *R = (TScrollBarRange *)C;
		DumpRange(Out, R->ButtonRange, R->Min, R->Max, R->Value, R->Visible, R->Dragging, R->Pressed, R->DragOffset,
			R->SliderHeight, R->SliderPosition, R->PageScroll, R->Enabled);
	} else if (K == "ListBox" || K == "FontListBox") {
		TListBox *L = (TListBox *)C;
		Add(Out, " lines %d width %d sel %d visible %d editable %d editing %d scroll %d", L->Lines, L->TextWidth,
			L->Selected, L->VisibleLines, L->Editable, L->Editing, L->EditScroll);
		DumpCallback(Out, s, L->SelectCallback);
		DumpCallback(Out, s, L->ActionCallback);
		for (int i = 0; i < L->Lines; i++) {
			if (K == "ListBox") {
				char *Text;
				if (Entry(L->LineText, i, &Text))
					Add(Out, " [%s]", Str(Text).c_str());
			} else {
				TListBoxLine Line;
				if (Entry(((TFontListBox *)C)->FontLines, i, &Line)) {
					Add(Out, " [%s", Str(Line.Text).c_str());
					DumpFont(Out, Line.Font);
					Out += "]";
				}
			}
		}
		Add(Out, " edit");
		DumpEdit(Out, s, L->EditLine);
	} else if (K == "Progress") {
		TProgressBar *P = (TProgressBar *)C;
		Add(Out, " max %d current %d", P->Maximum, P->Current);
	}
	bool Window = K == "Window" || K == "Border" || K == "FormattedText" || K == "Link" || K == "CheckBox"
		|| K == "EditLine" || K == "Password" || K == "EditText" || K == "HScroll" || K == "VScroll" || K == "ListBox"
		|| K == "FontListBox" || K == "Target";
	if (Window)
		DumpChildren(Out, s, (TWindow *)C, Depth);
}

static std::string DumpAll(int s)
{
	std::string Out;
	Dump(Out, s, Root[s], 0);
	if (Kind == K_RESIZER)
		Dump(Out, s, Target[s], 0);
	if (Group[s] != NULL) {
		Add(Out, "\ngroup active %s members %d:", Name(s, Group[s]->Active).c_str(), Group[s]->Members);
		for (int i = 0; i < Group[s]->Members; i++) {
			TEditLine *Member;
			if (Entry(Group[s]->MemberList, i, &Member))
				Add(Out, " %s", Name(s, Member).c_str());
		}
	}
	if (Radio[s] != NULL)
		Add(Out, "\nradio %s count %d", Name(s, Radio[s]->Selected).c_str(), Radio[s]->Count);
	return Out;
}

static int OperationCount[64];
static const char *OperationName;

// The error round: the action whose calls fail (CurrentAction: the operation,
// 22 the constructors, 23 the first SetSize, 24 the destructors), the most
// fake calls and the last count one call of each action made for each kind
// of control, whether a call threw (the controls are then abandoned: members
// may point to deleted parts, so only the logs are compared).
enum { ACTION_CONSTRUCT = 22, ACTION_FIRSTSIZE = 23, ACTION_DESTRUCT = 24, ACTIONS = 25 };
static TErrorRound *ElementsRound;
static int CurrentAction, TargetAction = -1, MaxTicks[KINDS][ACTIONS], LastTicks[KINDS][ACTIONS];
static bool Threw;

static void Compare()
{
	std::string D[2];
	if (!Threw) {
		D[OURS] = DumpAll(OURS);
		D[ORIGINAL] = DumpAll(ORIGINAL);
	}
	if (LogFile != NULL)
		fprintf(LogFile, "== %s %s\n%s\n%s\n", KindNames[Kind], OperationName, Log[OURS].c_str(), D[OURS].c_str());
	CHECK(Log[OURS] == Log[ORIGINAL], "DialogElements %s (%s): logs differ\n  ours: %s\n  orig: %s", OperationName,
		KindNames[Kind], Log[OURS].c_str(), Log[ORIGINAL].c_str());
	CHECK(D[OURS] == D[ORIGINAL], "DialogElements %s (%s): controls differ\n  ours: %s\n  orig: %s", OperationName,
		KindNames[Kind], D[OURS].c_str(), D[ORIGINAL].c_str());
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

static int FailIndex(int Count)
{
	int Choice = Random() % 3, Range = 1 << (Random() % 12);
	Range = Range < Count ? Range : Count;
	if (Choice == 0)
		return 1 + (int)(Random() % Count);
	if (Choice == 1)
		return 1 + (int)(Random() % Range);
	return Count - (int)(Random() % Range);
}

// Runs F for our side and then for the image's. In the error round the calls
// of the round's action fail at one of the fake calls such a call made before
// (for the same kind of control), and what both sides threw is compared;
// after a throw nothing runs.
template <class F> static void Both(F Function)
{
	if (ElementsRound != NULL) {
		if (Threw)
			return;
		int Count = Random() % 2 == 0 ? MaxTicks[Kind][CurrentAction] : LastTicks[Kind][CurrentAction];
		int Fail = Count > 0 && CurrentAction == TargetAction ? FailIndex(Count + 1) : 0;
		char What[96];
		sprintf(What, "%s %s (fail at %d)", KindNames[Kind], OperationName, Fail);
		ElementsRound->Calling(What, "ours");
		TThrown A = OurThrow([&] { Side = OURS; Ticks = 0; FailAt = Fail; Function(OURS); });
		ElementsRound->Calling(What, "the original");
		TThrown B = ImageThrow([&] { Side = ORIGINAL; Ticks = 0; FailAt = Fail; Function(ORIGINAL); });
		FailAt = 0;
		Side = OURS;
		ElementsRound->Compare(What, A, B);
		if (A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING) {
			Threw = true;
		} else {
			LastTicks[Kind][CurrentAction] = Ticks;
			if (Ticks > MaxTicks[Kind][CurrentAction])
				MaxTicks[Kind][CurrentAction] = Ticks;
		}
		return;
	}
	for (int s = 0; s < 2; s++) {
		Side = s;
		Ticks = 0;
		if (s == OURS) {
			try {
				Function(s);
			} catch (TMyException &E) {
				printf("DialogElements: our side threw: %s (%s)\n", E.GetMessage().c_str(), E.GetOrigin().c_str());
				fflush(stdout);
				abort();
			}
		} else {
			Function(s);
		}
	}
	// (the counts of the error round's failures)
	if (Ticks > MaxTicks[Kind][CurrentAction])
		MaxTicks[Kind][CurrentAction] = Ticks;
	Side = OURS;
}

// --- Operations ---------------------------------------------------------------------------

static bool IsEditKind() { return Kind == K_EDIT || Kind == K_EDITTEXT; }
static bool IsButtonKind() { return Kind >= K_SKINBUTTON && Kind <= K_MULTI; }
static bool IsListKind() { return Kind == K_LIST || Kind == K_FONTLIST; }

// A valid size for the kind (the assertions and the endless loops of narrow
// fields stay out of reach).
static void RandomSize(int *Width, int *Height)
{
	TComponent *C = Control[OURS];
	*Width = 30 + Random() % 120;
	*Height = 20 + Random() % 50;
	switch (Kind) {
	case K_OUTFIT: case K_OBJECTICON:
		*Height = *Width = 10 + Random() % 40;
		break;
	case K_PROGRESS:
		*Width = ((TProgressBar *)C)->getMinimumWidth() + Random() % 100;
		*Height = ((TProgressBar *)C)->getMinimumHeight() + Random() % 10;
		break;
	case K_BORDER:
		*Width = ((TBorder *)C)->getMinimumWidth() + 1 + Random() % 80;
		*Height = ((TBorder *)C)->getMinimumHeight() + 1 + Random() % 80;
		break;
	case K_LINK:
		*Width = ((TLink *)C)->Button->Width + 1 + Random() % 150;
		break;
	case K_FORMATTED:
		*Width = Random() % 3 == 0 ? 0 : 300 + Random() % 100;
		break;
	case K_EDIT: case K_EDITTEXT:
		*Width = 40 + Random() % 120;
		*Height = 20 + Random() % 80;
		break;
	case K_HRANGE: case K_HSCROLL:
		*Width = 60 + Random() % 120;
		break;
	case K_VRANGE: case K_VSCROLL: case K_LIST: case K_FONTLIST:
		*Width = 40 + Random() % 100;
		*Height = 60 + Random() % 120;
		break;
	}
}

static int Key()
{
	static const int Keys[] = {'a', 'Z', '0', ' ', '.', '~', 8, 9, 0x209, 0x0d, 0x10c, 0x10d, 0x10e, 0x10f, 0x110, 0x111,
		0x112, 0x113, 0x114, 0x115, 0x30d, 0x50d, 0x310, 0x476, 0x510, 0x463, 0x311, 0x478, 0x511, 0x461, 0x441, 0x841,
		0x461 | 0x800, 0x20c, 0x40f, 'q', 'M', 'w'};
	return Keys[Random() % (sizeof(Keys) / sizeof(Keys[0]))];
}

static TEditLine *Edit(int s) { return (TEditLine *)Control[s]; }
static int EditLength() { return (int)strlen(Edit(OURS)->Text); }

// The members of each class (the non-virtual ones at their image addresses).
static void Special(int r1, int r2, int r3)
{
	int Pick = r1 % 8;
	const char *Text = RandomText();
	TFont Font = RandomFont();
	switch (Kind) {
	case K_BORDER:
		OperationName = "FitToWindow";
		Both([&](int s) {
			if (s == OURS)
				((TBorder *)Control[s])->FitToWindow();
			else
				CallI<void>(0x455500, Control[s]);
		});
		break;
	case K_LABEL: {
		bool Shorten = r2 % 2 != 0;
		OperationName = "Label::SetText";
		Both([&](int s) { ((TLabel *)Control[s])->SetText(Text, Shorten); });
		break;
	}
	case K_FORMATTED: {
		bool Rearrange = r2 % 2 != 0;
		OperationName = "FormattedText::SetText";
		Both([&](int s) { ((TFormattedText *)Control[s])->SetText(Text, Rearrange); });
		break;
	}
	case K_OUTFIT: {
		int Mask = 1 + r2 % 4, Color = r3 % 133;
		int Colors[4] = {r2 % 50, r3 % 60, (r2 >> 8) % 70, (r3 >> 8) % 80};
		OperationName = "OutfitIcon";
		Both([&](int s) {
			TOutfitIcon *O = (TOutfitIcon *)Control[s];
			if (Pick < 3) {
				if (s == OURS)
					O->SetColor(Mask, Color);
				else
					CallI<void>(0x457440, O, Mask, Color);
			} else if (Pick < 6) {
				if (s == OURS)
					O->SetColors(Colors);
				else
					CallI<void>(0x4575a0, O, (int *)Colors);
			} else {
				if (s == OURS)
					O->SetOutfit(Color);
				else
					CallI<void>(0x4526b0, O, Color);
			}
		});
		break;
	}
	case K_SKINBUTTON: case K_COLORBUTTON: case K_ICONBUTTON: case K_BLINKING: case K_TEXTBUTTON: case K_TOGGLE:
	case K_TEXTTOGGLE: case K_MULTI: {
		int Up = 26 + r2 % 30, Down = 26 + r3 % 30;
		bool Flag = r3 % 3 != 0;
		bool Join = ((TSkinButton *)Control[OURS])->RadioButtonGroup == NULL;
		int Choice = Pick;
		if (Choice == 1 && !Join)
			Choice = 2;
		OperationName = "Button";
		Both([&](int s) {
			TSkinButton *B = (TSkinButton *)Control[s];
			switch (Choice) {
			case 0:
				if (s == OURS)
					B->SetSkins(Up, Down);
				else
					CallI<void>(0x458b20, B, Up, Down);
				break;
			case 1:
				if (s == OURS)
					B->SetRadioButtonGroup(Radio[s]);
				else
					CallI<void>(0x458de0, B, Radio[s]);
				break;
			case 2:
				Radio[s]->Selected = Flag ? B : NULL;
				break;
			case 3:
				if (s == OURS)
					B->SetEnabled(Flag);
				else
					CallI<void>(0x452730, B, Flag);
				break;
			case 4:
				Logf("Pressed%d;", B->IsPressed() ? 1 : 0);
				break;
			case 5:
				if (Kind == K_TEXTBUTTON || Kind == K_TEXTTOGGLE)
					((TTextButton *)B)->SetText(ShortText(Text));
				break;
			case 6:
				if (Kind == K_MULTI) {
					if (s == OURS)
						((TMultiStateButton *)B)->Reset();
					else
						CallI<void>(0x452790, B);
				}
				break;
			case 7:
				if (Kind == K_TOGGLE)
					((TToggleButton *)B)->State = Flag;
				else if (Kind == K_TEXTTOGGLE)
					((TTextToggleButton *)B)->State = Flag;
				break;
			}
		});
		break;
	}
	case K_CHECKBOX: {
		bool Flag = r2 % 2 != 0;
		OperationName = "CheckBox";
		Both([&](int s) {
			TCheckBox *B = (TCheckBox *)Control[s];
			if (Pick < 3)
				B->SetChecked(Flag);
			else if (Pick < 5)
				Logf("Checked%d;", B->IsChecked() ? 1 : 0);
			else if (s == OURS)
				B->SetEnabled(Flag);
			else
				CallI<void>(0x4678d0, B, Flag);
		});
		break;
	}
	case K_EDIT: case K_EDITTEXT: {
		int Length = EditLength();
		int Position = Length == 0 ? 0 : r2 % (Length + 1);
		int Cursor = Edit(OURS)->Cursor;
		int Count = r3 % 6;
		int Choice = r1 % 19;
		bool Flag = r3 % 2 != 0;
		int x = r2 % 120, y = r3 % 60;
		char c = "abc d.\n"[r3 % 7];
		TFocusGroup *G = Group[OURS];
		if ((Choice == 15 || Choice == 16) && (G == NULL || G->Active == NULL))
			Choice = 14;
		if (Choice == 18 && (Kind != K_EDITTEXT || ((TEditText *)Edit(OURS))->FieldWidth <= 0))
			Choice = 17;
		OperationName = "Edit";
		Both([&](int s) {
			TEditLine *E = Edit(s);
			switch (Choice) {
			case 0: E->SetText(Text); break;
			case 1: E->Clear(); break;
			case 2: E->DeleteText(Position, Count); break;
			case 3: E->DeleteChar(Position); break;
			case 4: Logf("Inserted%d;", E->InsertText(Cursor, Text, strlen(Text))); break;
			case 5: E->InsertChar(Cursor, c); break;
			case 6: E->SetDisabled(Flag); break;
			case 7: Logf("Disabled%d;", E->IsDisabled() ? 1 : 0); break;
			case 8: E->SetFont(Font); break;
			case 9: Logf("Position%d;", E->GetPosition(x, y)); break;
			case 10:
				if (Kind == K_EDITTEXT)
					((TEditText *)E)->SetActive(Flag);
				else if (s == OURS)
					E->SetActive(Flag);
				else
					CallI<void>(0x45bc80, E, Flag);
				break;
			case 11:
				if (s == OURS)
					E->SetCursor(Position + Count - 2);
				else
					CallI<void>(0x4524e0, E, Position + Count - 2);
				break;
			case 12:
				if (s == OURS)
					E->SetSelection(Position + Count - 2);
				else
					CallI<void>(0x452880, E, Position + Count - 2);
				break;
			case 13:
				if (s == OURS)
					Logf("Has%d;", E->HasSelection() ? 1 : 0);
				else
					Logf("Has%d;", CallI<bool>(0x417520, E) ? 1 : 0);
				E->SelectionStart = Flag ? -1 : Position;
				E->SelectionLength = Count - 2;
				if (E->SelectionStart + E->SelectionLength < 0 || E->SelectionStart + E->SelectionLength > Length)
					E->SelectionLength = 0;
				break;
			case 14:
				if (s == OURS)
					E->LimitCursor();
				else
					CallI<void>(0x452530, E);
				break;
			case 15:
				if (s == OURS)
					Group[s]->Next();
				else
					CallI<void>(0x45e290, Group[s]);
				break;
			case 16:
				if (s == OURS)
					Group[s]->Previous();
				else
					CallI<void>(0x45e630, Group[s]);
				break;
			case 17:
				if (Kind == K_EDITTEXT)
					((TEditText *)E)->SetText(Text, Flag);
				else if (Group[s] != NULL && s == OURS)
					Group[s]->SetActive(E);
				else if (Group[s] != NULL)
					CallI<void>(0x45de20, Group[s], E);
				break;
			case 18:
				if (s == OURS)
					((TEditText *)E)->FormatText();
				else
					CallI<void>(0x45da90, E);
				break;
			}
		});
		break;
	}
	case K_HSCROLL: case K_HRANGE: {
		int Value = (int)(r2 % 100) - 20;
		OperationName = "HRange::SetValue";
		Both([&](int s) {
			THorizontalScrollBarRange *R = Kind == K_HRANGE ? (THorizontalScrollBarRange *)Control[s]
				: ((THorizontalScrollBar *)Control[s])->Range;
			if (s == OURS)
				R->SetValue(Value);
			else
				CallI<void>(0x45ee10, R, Value);
		});
		break;
	}
	case K_VSCROLL: case K_VRANGE: {
		int Value = (int)(r2 % 100) - 20, Min = r3 % 30, Max = Min + (r3 >> 8) % 40, Visible = r1 % 20;
		OperationName = "Range";
		Both([&](int s) {
			TScrollBarRange *R = Kind == K_VRANGE ? (TScrollBarRange *)Control[s] : ((TScrollBar *)Control[s])->Range;
			if (Pick < 4) {
				if (s == OURS)
					R->SetValue(Value);
				else
					CallI<void>(0x460710, R, Value);
			} else if (s == OURS) {
				R->SetRange(Min, Max, Value, Visible);
			} else {
				CallI<void>(0x460450, R, Min, Max, Value, Visible);
			}
		});
		break;
	}
	case K_LIST: case K_FONTLIST: {
		TListBox *L0 = (TListBox *)Control[OURS];
		int Lines = L0->Lines;
		int Number = Lines == 0 ? 0 : r2 % Lines;
		int Choice = r1 % 12;
		bool Flag = r3 % 2 != 0;
		int y = (int)(r3 % 200) - 10;
		if ((Choice == 2 || Choice == 3) && Lines == 0)
			Choice = 0;
		if (Choice == 7 && L0->GetSelection() == -1)
			Choice = 0;
		if (Choice == 0 && Lines > 30)
			Choice = 5;
		OperationName = "List";
		Both([&](int s) {
			TListBox *L = (TListBox *)Control[s];
			bool F = Kind == K_FONTLIST;
			switch (Choice) {
			case 0: case 1:
				if (!F) {
					if (s == OURS)
						L->AddLine(Text);
					else
						CallI<void>(0x462460, L, Text);
				} else if (Choice == 0 || !Flag) {
					if (s == OURS)
						((TFontListBox *)L)->AddLine(Text, Font);
					else
						CallI<void>(0x463820, L, Text, Font);
				} else {
					if (s == OURS)
						((TFontListBox *)L)->AddLine(20);
					else
						CallI<void>(0x463b10, L, 20);
				}
				break;
			case 2:
				if (!F) {
					if (s == OURS)
						L->SetLine(Number, Text);
					else
						CallI<void>(0x462730, L, Number, Text);
				} else if (s == OURS) {
					((TFontListBox *)L)->SetLine(Number, Text, Font);
				} else {
					CallI<void>(0x463dc0, L, Number, Text, Font);
				}
				break;
			case 3:
				if (!F) {
					Logf("Line%s;", s == OURS ? L->GetLine(Number) : CallI<char *>(0x462b20, L, Number));
				} else {
					TFont Got;
					char *T = s == OURS ? ((TFontListBox *)L)->GetLine(Number, &Got)
						: CallI<char *>(0x4643a0, L, Number, &Got);
					Logf("Line%s,%d,%d;", T, Got.FontNumber, Got.Color.R);
				}
				break;
			case 4:
				if (s == OURS)
					L->SetSelection(Flag || Lines == 0 ? -1 : Number);
				else
					CallI<void>(0x46ad10, L, Flag || Lines == 0 ? -1 : Number);
				break;
			case 5:
				if (!F) {
					if (s == OURS)
						L->Clear();
					else
						CallI<void>(0x462ca0, L);
				} else if (s == OURS) {
					((TFontListBox *)L)->Clear();
				} else {
					CallI<void>(0x4641e0, L);
				}
				break;
			case 6:
				Logf("At%d;", s == OURS ? L->GetLineAt(5, y) : CallI<int>(0x462e70, L, 5, y));
				break;
			case 7:
				L->StartEdit();
				break;
			case 8:
				L->StopEdit();
				break;
			case 9:
				Logf("Sel%d;", s == OURS ? L->GetSelection() : CallI<int>(0x4529d0, L));
				break;
			case 10: case 11:
				if (s == OURS)
					L->SetEditable(Flag);
				else
					CallI<void>(0x4529f0, L, Flag);
				break;
			}
		});
		break;
	}
	case K_PROGRESS: {
		int Value = (int)(r2 % 300) - 50, Maximum = 1 + r3 % 200;
		if (r3 % 3 == 0 && Control[OURS]->Width > 0) {
			Maximum = Control[OURS]->Width;
			Value = Skins[259].width + (int)(r2 % 3) - 1;
			Pick = r2 % 2 == 0 ? 3 : 0;
		}
		OperationName = "Progress";
		Both([&](int s) {
			TProgressBar *P = (TProgressBar *)Control[s];
			if (Pick < 3) {
				if (s == OURS)
					P->SetCurrent(Value);
				else
					CallI<void>(0x4651c0, P, Value);
			} else if (Pick < 5) {
				if (s == OURS)
					P->SetMaximum(Maximum);
				else
					CallI<void>(0x464f40, P, Maximum);
			} else if (s == OURS) {
				P->SetComplete();
			} else {
				CallI<void>(0x465380, P);
			}
		});
		break;
	}
	default:
		OperationName = "Arrange";
		Both([&](int s) { Control[s]->Arrange(); });
		break;
	}
}

// Forced >= 0: that operation (the error round), with the same random numbers.
static void Operation(int Forced = -1)
{
	TComponent *C0 = Control[OURS];
	int Op = Random() % 22;
	if (Forced >= 0)
		Op = Forced;
	int x = (int)(Random() % (C0->Width + 12)) - 5, y = (int)(Random() % (C0->Height + 12)) - 5;
	int x2 = (int)(Random() % (C0->Width + 12)) - 5, y2 = (int)(Random() % (C0->Height + 12)) - 5;
	int r1 = Random(), r2 = Random(), r3 = Random();
	World.ActionNumber += 1 + Random() % 3;
	World.ActionState = Random() % 5 == 0 ? 10 + Random() % 2 : 0;
	World.ClipboardEmpty = Random() % 4 == 0;
	static const char *const Clips[] = {"pasted", "two\nlines", "a much longer pasted text for the field", NULL};
	World.Clipboard = Clips[Random() % 4];
	SetTestClock(r3 % 1000 + 1000 * (World.ActionNumber % 5));
	OperationCount[Op]++;
	CurrentAction = Op;
	switch (Op) {
	case 0: {
		int w, h;
		RandomSize(&w, &h);
		OperationName = "SetSize";
		Both([&](int s) { Control[s]->SetSize(w, h); });
		break;
	}
	case 1:
		OperationName = "Arrange";
		Both([&](int s) { Control[s]->Arrange(); });
		break;
	case 2: {
		int Surface = 1 + r1 % 3, dx = r2 % 50, dy = r3 % 50;
		OperationName = "Draw";
		Both([&](int s) { Control[s]->Draw(Surface, dx, dy); });
		break;
	}
	case 3:
		OperationName = "MouseMove";
		Both([&](int s) { Logf("R%d;", Control[s]->MouseMove(x, y) ? 1 : 0); });
		break;
	case 4: case 5: {
		// A press, maybe a drag and held-button ticks, a release.
		bool Drag = r1 % 2 != 0, Hold = r2 % 3 == 0;
		int Flags = r3 % 4 == 0 ? 0x200 : 0;
		OperationName = "Click";
		Both([&](int s) {
			Logf("D%d;", Control[s]->LeftButtonDown(x, y, Flags) ? 1 : 0);
			if (Drag)
				Logf("M%d;", Control[s]->MouseMove(x2, y2) ? 1 : 0);
			if (Hold) {
				Logf("H%d;", Control[s]->MouseWheel(x2, y2, 6) ? 1 : 0);
				Logf("H%d;", Control[s]->MouseWheel(x2, y2, 6) ? 1 : 0);
			}
			Logf("U%d;", Control[s]->LeftButtonUp(Drag ? x2 : x, Drag ? y2 : y, Flags) ? 1 : 0);
		});
		break;
	}
	case 6:
		OperationName = "DoubleClick";
		Both([&](int s) {
			Logf("D%d;", Control[s]->LeftButtonDoubleClick(x, y, 0) ? 1 : 0);
			Logf("U%d;", Control[s]->LeftButtonUp(x, y, 0) ? 1 : 0);
		});
		break;
	case 7:
		OperationName = "RightButtonUp";
		Both([&](int s) { Logf("R%d;", Control[s]->RightButtonUp(x, y, 0) ? 1 : 0); });
		break;
	case 8: {
		int Code = 1 + r1 % 2;
		OperationName = "MouseWheel";
		Both([&](int s) { Logf("R%d;", Control[s]->MouseWheel(x, y, Code) ? 1 : 0); });
		break;
	}
	case 9:
		OperationName = "MouseHover";
		Both([&](int s) { Control[s]->MouseHover(x, y); });
		break;
	case 10: case 11: {
		int K = Key();
		if ((K == 9 || K == 0x209) && (Group[OURS] == NULL || Group[OURS]->Active == NULL))
			K = 'k';
		int Before = IsEditKind() ? r1 % 4 : 3;
		if (Before == 1 && r3 % 3 == 0)
			K = 8;
		bool Draw = IsEditKind() && r2 % 2 != 0;
		OperationName = "KeyPress";
		Both([&](int s) {
			if (Before == 0)
				Control[s]->HandleEvent(20);
			else if (Before == 1) {
				if (Edit(s)->Text[0] == 0)
					Edit(s)->SetText("one two three");
				Logf("D%d;", Control[s]->LeftButtonDoubleClick(x, y, 0) ? 1 : 0);
				Logf("U%d;", Control[s]->LeftButtonUp(x, y, 0) ? 1 : 0);
			}
			Control[s]->KeyPress(K);
			if (Draw)
				Control[s]->Draw(1, 0, 0);
		});
		break;
	}
	case 12: {
		int Event = r1 % 11;
		if (IsEditKind())
			Event = 16 + r1 % 5;
		else if (Kind == K_HSCROLL || Kind == K_VSCROLL)
			Event = 3 + r1 % 4;
		OperationName = "HandleEvent";
		Both([&](int s) { Control[s]->HandleEvent(Event); });
		break;
	}
	default:
		Special(r1, r2, r3);
		break;
	}
	Compare();
}

// --- The test ----------------------------------------------------------------------------

static const char *const ProbeNames[KINDS] = {"Separator", "Resizer", "Border", "Label", "FormattedText", "Link",
	"OutfitIcon", "SkinIcon", "ObjectIcon", "SkinButton", "ColorButton", "IconButton", "Blinking", "TextButton",
	"Toggle", "TextToggle", "MultiState", "CheckBox", "EditLine", "EditText", "HScroll", "VScroll", "HRange", "VRange",
	"ListBox", "FontListBox", "Progress"};

// Our vtables, from objects made quietly.
static void FindVtables()
{
	for (size_t i = 0; i < sizeof(ImageVtables) / sizeof(ImageVtables[0]); i++)
		Kinds[ORIGINAL][(void *)(uintptr_t)ImageVtables[i].Vtable] = ImageVtables[i].Kind;
	Side = OURS;
	Receiver[OURS] = new TDEReceiver;
	TWindow *W = new TWindow;
	Kinds[OURS][VtableOf(W)] = "Window";
	OurWindowVtable = VtableOf(W);
	delete W;
	TElement *E = new TElement;
	OurElementVtable = VtableOf(E);
	delete E;
	for (int s = 0; s < 2; s++)
		Kinds[s][VtableOf(Receiver[OURS])] = "Receiver";
	for (int k = 0; k < KINDS; k++) {
		for (int Variant = 0; Variant < 3; Variant++) {
			Kind = k;
			TArguments A = RandomArguments();
			A.Variant = Variant;
			Target[OURS] = new TDETarget;
			if (k == 0 && Variant == 0) {
				for (int s = 0; s < 2; s++)
					Kinds[s][VtableOf(Target[OURS])] = "Target";
			}
			TComponent *C = Build(OURS, A);
			Kinds[OURS][VtableOf(C)] = k == K_EDIT && Variant == 2 ? "Password" : ProbeNames[k];
			delete C;
			if (k != K_BORDER)
				delete Target[OURS];
		}
	}
	delete Receiver[OURS];
}

// The image side's receiver and target in the error round: the image's
// TElement and TWindow code for what they do not override (imageeh.h,
// ImageSideVtable), named like ours.
static void ImageSide(int s, void *Object, bool Window)
{
	if (ElementsRound == NULL || s != ORIGINAL)
		return;
	void *Ours = VtableOf(Object);
	void *Hybrid = ImageSideVtable(Ours, Window ? OurWindowVtable : OurElementVtable, Window ? 0x564528 : 0x5645c8);
	Kinds[ORIGINAL][Hybrid] = Kinds[ORIGINAL][Ours];
	*(void **)Object = Hybrid;
}

static void Construct(const TArguments &A)
{
	Both([&](int s) {
		Receiver[s] = new TDEReceiver;
		ImageSide(s, Receiver[s], false);
		Roles[s][Receiver[s]] = "Recv";
		Target[s] = new TDETarget;
		ImageSide(s, Target[s], true);
		Roles[s][Target[s]] = "Target";
		Target[s]->TComponent::SetSize(100, 80);
		Root[s] = s == OURS ? new TWindow : (TWindow *)ImageWindow();
		Roles[s][Root[s]] = "Root";
		Root[s]->SetSize(500, 500);
		Radio[s] = new TRadioButtonGroup;
		Radio[s]->Selected = NULL;
		Group[s] = NULL;
		Control[s] = Build(s, A);
		Roles[s][Control[s]] = "C";
		Root[s]->InsertChild(Control[s]);
		if (Kind == K_EDIT && A.Flag2) {
			Group[s] = s == OURS ? new TFocusGroup : (TFocusGroup *)At<CtorFn>(0x45ddf0)(PatternNew(0x28), NULL);
			for (int i = 0; i < 2; i++) {
				TRect Area(0, 20 + 20 * i, 80, 16);
				Extra[s][i] = s == OURS ? (TComponent *)new TEditLine(Area, 20, false)
					: (TComponent *)CallI<void *>(0x467b50, PatternNew(0x68), Area, 20, false);
				Roles[s][Extra[s][i]] = i == 0 ? "X1" : "X2";
				Root[s]->InsertChild(Extra[s][i]);
			}
			TEditLine *Members[3] = {(TEditLine *)Control[s], (TEditLine *)Extra[s][0], (TEditLine *)Extra[s][1]};
			for (int i = 0; i < 3; i++) {
				if (s == OURS)
					Members[i]->SetFocusGroup(Group[s]);
				else
					CallI<void>(0x468470, Members[i], Group[s]);
			}
		}
	});
}

static void Destruct()
{
	Both([&](int s) {
		delete Root[s];
		if (Kind != K_BORDER)
			delete Target[s];
		delete Receiver[s];
		delete Radio[s];
		delete Group[s];
		Group[s] = NULL;
	});
}

// --- The error round (issue #34) ---------------------------------------------------------

// New controls of the kind (random arguments) and their first size; false
// when a call threw.
static bool NewControls(TArguments &A, bool WithGroup = false)
{
	A = RandomArguments();
	if (WithGroup)
		A.Flag2 = true; // (the edit lines' focus group)
	Testing = true;
	Threw = false;
	OperationName = "constructor";
	CurrentAction = ACTION_CONSTRUCT;
	Construct(A);
	Compare();
	int Width, Height;
	if (!Threw)
		RandomSize(&Width, &Height);
	OperationName = "first SetSize";
	CurrentAction = ACTION_FIRSTSIZE;
	Both([&](int s) { Control[s]->SetSize(Width, Height); });
	Compare();
	return !Threw;
}

// The end of a round: the controls deleted, or abandoned after a throw.
static void EndControls()
{
	if (!Threw) {
		OperationName = "destructor";
		CurrentAction = ACTION_DESTRUCT;
		Destruct();
		CHECK(Log[OURS] == Log[ORIGINAL], "DialogElements errors destructor (%s): logs differ\n  ours: %s\n  orig: %s",
			KindNames[Kind], Log[OURS].c_str(), Log[ORIGINAL].c_str());
	}
	for (int s = 0; s < 2; s++) {
		Root[s] = NULL;
		Control[s] = NULL;
		Group[s] = NULL;
		Roles[s].clear();
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();
	Testing = false;
	Threw = false;
}

// One call on both sides (failing at the Fail-th fake call, 0: none), what
// they threw compared, then the logs (and the controls unless a call threw).
template <class F> static void ElementCall(const char *What, F Call, int Fail = 0)
{
	OperationName = What;
	ElementsRound->Calling(What, "ours");
	TThrown A = OurThrow([&] { Side = OURS; Ticks = 0; FailAt = Fail; Call(OURS); });
	ElementsRound->Calling(What, "the original");
	TThrown B = ImageThrow([&] { Side = ORIGINAL; Ticks = 0; FailAt = Fail; Call(ORIGINAL); });
	FailAt = 0;
	Side = OURS;
	ElementsRound->Compare(What, A, B);
	Threw = Threw || A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING;
	Compare();
}

static void TestDialogElementsAssertions();

// Every TASSERT/TERROR of DialogElements.cpp and the TEND blocks: the test's
// operations on new controls, the calls of one action per round failing at
// one of the fake calls it made before, until one throws; then the
// assertions with direct calls.
static void TestDialogElementsErrors()
{
	TErrorRound R("DialogElements");
	ElementsRound = &R;
	// Each kind with each action failing (half of the operations are the
	// action's own), forty times.
	for (int Round = 0; Round < 40 * KINDS * ACTIONS; Round++) {
		Kind = Round % KINDS;
		TargetAction = Round / KINDS % ACTIONS;
		TArguments A;
		if (NewControls(A)) {
			int Operations = 5 + Random() % 30;
			for (int i = 0; i < Operations && !Threw; i++) {
				bool Own = Random() % 2 != 0;
				Operation(Own && TargetAction < 22 ? TargetAction : -1);
			}
		}
		EndControls();
	}
	TargetAction = -1;
	TestDialogElementsAssertions();
	ElementsRound = NULL;
	R.Finish();
}

// A direct call on new controls of the kind (with the edit lines' focus
// group), compared; the controls are deleted or abandoned after it.
template <class F> static void Direct(int k, const char *What, F Call, bool WithGroup = false)
{
	Kind = k;
	TArguments A;
	if (NewControls(A, WithGroup))
		ElementCall(What, Call);
	EndControls();
}

// A call on new controls of the kind, first without a failure (its fake
// calls counted), then failing at each of them in turn, each on new controls.
template <class F> static void Sweep(int k, const char *What, F Call, int Limit = 60)
{
	int Count = 0;
	for (int Fail = 0; Fail <= Count && Fail <= Limit; Fail++) {
		Kind = k;
		TArguments A;
		if (NewControls(A)) {
			ElementCall(What, Call, Fail);
			if (Fail == 0)
				Count = Threw ? 0 : Ticks;
		}
		EndControls();
	}
}

static const char *const LongButtonText = "a button text of fifty characters and a few more!!";

// The assertions with direct calls: constructors with arguments the game
// does not give (made beside new controls of a kind), the members of each
// kind outside their ranges.
static void TestDialogElementsAssertions()
{
	static char Filler[64];
	memset(Filler, 'x', sizeof(Filler) - 1);
	for (int Round = 0; Round < 4; Round++) {
		TFont F = RandomFont();
		int r = Round;
		// Constructors.
		Direct(K_SEPARATOR, "TResizer::TResizer NULL", [&](int s) {
			if (s == OURS) new TResizer(NULL, true, r % 2 != 0, false);
			else CallI<void *>(0x453be0, PatternNew(0x38), (TComponent *)NULL, true, r % 2 != 0, false); });
		Direct(K_SEPARATOR, "TBorder::TBorder NULL", [&](int s) {
			if (s == OURS) new TBorder((TWindow *)NULL, r);
			else CallI<void *>(0x4547f0, PatternNew(0x68), (TWindow *)NULL, r); });
		Direct(K_SEPARATOR, "TBorder::TBorder 8 skins NULL", [&](int s) {
			if (s == OURS) new TBorder((TWindow *)NULL, 26, 32, 27, 30, 31, 28, 33, 29, r);
			else CallI<void *>(0x454b10, PatternNew(0x68), (TWindow *)NULL, 26, 32, 27, 30, 31, 28, 33, 29, r); });
		Direct(K_SEPARATOR, "TBorder::TBorder 9 skins NULL", [&](int s) {
			if (s == OURS) new TBorder((TWindow *)NULL, 26, 32, 27, 30, 31, 28, 33, 29, 7, r);
			else CallI<void *>(0x454e30, PatternNew(0x68), (TWindow *)NULL, 26, 32, 27, 30, 31, 28, 33, 29, 7, r); });
		Direct(K_SEPARATOR, "TLabel::TLabel NULL", [&](int s) {
			const char *T = NULL;
			if (r % 3 == 0) {
				if (s == OURS) new TLabel(1, 2, 40, 12, F, T, true);
				else CallI<void *>(0x465510, PatternNew(0x4c), 1, 2, 40, 12, F, T, true);
			} else if (r % 3 == 1) {
				if (s == OURS) new TLabel(1, 2, F, T);
				else CallI<void *>(0x465810, PatternNew(0x4c), 1, 2, F, T);
			} else {
				if (s == OURS) new TLabel(F, T);
				else CallI<void *>(0x465b40, PatternNew(0x4c), F, T);
			} });
		Direct(K_SEPARATOR, "TLink::TLink NULL", [&](int s) {
			const char *Text = r % 3 == 1 ? NULL : "Text", *Button = r % 3 == 2 ? NULL : "Open";
			const char *Destination = r % 3 == 0 ? NULL : "http://www.tibia.com/";
			if (s == OURS) new TLink(Text, Button, Destination, FakeOpenURL, F);
			else CallI<void *>(0x46b030, PatternNew(0x3c), Text, Button, Destination, (void (*)(const char *))FakeOpenURL, F); });
		Direct(K_SEPARATOR, "TTextButton::TTextButton", [&](int s) {
			const char *T = r % 2 == 0 ? NULL : LongButtonText;
			TCallback CB(Receiver[s], 1);
			if (r < 2) {
				if (s == OURS) new TTextButton(1, 2, T, 30, 31, CB, "Tip");
				else CallI<void *>(0x466bc0, PatternNew(0x78), 1, 2, T, 30, 31, CB, "Tip");
			} else {
				if (s == OURS) new TTextButton(1, 2, T, CB, "Tip");
				else CallI<void *>(0x466ed0, PatternNew(0x78), 1, 2, T, CB, "Tip");
			} });
		Direct(K_SEPARATOR, "TCheckBox::TCheckBox", [&](int s) {
			const char *T = r % 2 == 0 ? NULL : LongButtonText;
			if (s == OURS) new TCheckBox(T);
			else CallI<void *>(0x467440, PatternNew(0x34), T); });
		Direct(K_SEPARATOR, "TEditLine::TEditLine negative", [&](int s) {
			if (r % 2 == 0) {
				if (s == OURS) new TEditLine(TRect(1, 2, 60, 16), -1 - r, false);
				else CallI<void *>(0x467b50, PatternNew(0x68), TRect(1, 2, 60, 16), -1 - r, false);
			} else {
				if (s == OURS) new TEditLine(-1 - r, true);
				else CallI<void *>(0x45a4c0, PatternNew(0x68), -1 - r, true);
			} });
		// The ranges: minimum above maximum, value outside, no buttons.
		int Min = r % 2 == 0 ? 10 : 0, Max = r % 2 == 0 ? 5 : 10, Value = r % 2 == 0 ? 7 : 11 + r;
		Direct(K_SEPARATOR, "THorizontalScrollBarRange::THorizontalScrollBarRange", [&](int s) {
			TCallback CB(Receiver[s], 1);
			if (s == OURS) new THorizontalScrollBarRange(Min, Max, Value, 2, CB);
			else CallI<void *>(0x45e9e0, PatternNew(0x54), Min, Max, Value, 2, CB); });
		Direct(K_SEPARATOR, "TScrollBarRange::TScrollBarRange", [&](int s) {
			if (s == OURS) new TScrollBarRange(0, 0, 12, 40, Min, Max, Value, 2);
			else CallI<void *>(0x469670, PatternNew(0x4c), 0, 0, 12, 40, Min, Max, Value, 2); });
		Direct(K_SEPARATOR, "THorizontalScrollBar::THorizontalScrollBar no buttons", [&](int s) {
			TCallback CB(Receiver[s], 1);
			if (s == OURS) new THorizontalScrollBar(0, 10, 5, 3, -r, CB);
			else CallI<void *>(0x4692d0, PatternNew(0x38), 0, 10, 5, 3, -r, CB); });
		Direct(K_SEPARATOR, "TScrollBar::TScrollBar no buttons", [&](int s) {
			if (s == OURS) new TScrollBar(TRect(1, 2, 12, 60), 0, 10, 5, 3, -r);
			else CallI<void *>(0x469a80, PatternNew(0x38), TRect(1, 2, 12, 60), 0, 10, 5, 3, -r); });

		// The resizer without its window.
		for (int What = 0; What < 3; What++) {
			Direct(K_RESIZER, "TResizer without a window", [&](int s) {
				((TResizer *)Control[s])->ToResize = NULL;
				if (What == 0) Control[s]->MouseMove(1, 1);
				else if (What == 1) Control[s]->LeftButtonDown(1, 1, 0);
				else Control[s]->LeftButtonUp(1, 1, 0); });
		}
		// Sizes below the minimum, a link without a width, icons that are not square.
		Direct(K_BORDER, "TBorder::SetSize below the minimum", [&](int s) {
			TBorder *B = (TBorder *)Control[OURS];
			int w = B->getMinimumWidth(), h = B->getMinimumHeight();
			if (r % 2 == 0) Control[s]->SetSize(w - 1 - r, h + 5);
			else Control[s]->SetSize(w + 5, h - 1 - r); });
		Direct(K_LINK, "TLink::Arrange without a width", [&](int s) {
			Control[s]->Width = -r;
			Control[s]->Arrange(); });
		Direct(K_OUTFIT, "TOutfitIcon::Draw not square", [&](int s) {
			Control[s]->Width = 20;
			Control[s]->Height = 21 + r;
			Control[s]->Draw(1, 0, 0); });
		Direct(K_OBJECTICON, "TObjectIcon::Draw not square", [&](int s) {
			Control[s]->Width = 20 + r;
			Control[s]->Height = 19;
			Control[s]->Draw(1, 0, 0); });
		// Drawing at negative places.
		static const int DrawKinds[] = {K_SKINICON, K_OBJECTICON, K_SKINBUTTON, K_COLORBUTTON, K_ICONBUTTON, K_BLINKING,
			K_TEXTBUTTON, K_TOGGLE, K_TEXTTOGGLE, K_MULTI, K_CHECKBOX};
		for (int k : DrawKinds) {
			int dx = k == K_ICONBUTTON ? SkinProperties.Buttons.Texture.x : k == K_CHECKBOX ? SkinProperties.CheckBox.Item.x : 0;
			int dy = k == K_ICONBUTTON ? SkinProperties.Buttons.Texture.y : k == K_CHECKBOX ? SkinProperties.CheckBox.Item.y : 0;
			int x = r % 2 == 0 ? -dx - 1 - r : 0, y = r % 2 == 0 ? 0 : -dy - 1 - r;
			Direct(k, "Draw at a negative place", [&](int s) {
				if (k == K_OBJECTICON)
					((TObjectIcon *)Control[s])->Framed = true; // (the assertions are the frame's)
				Control[s]->Draw(1, x, y); });
		}
		// The outfit's colours, the buttons' skins and radio groups.
		Direct(K_OUTFIT, "TOutfitIcon::SetColor mask", [&](int s) {
			int Mask = r % 2 == 0 ? -r : 5 + r;
			if (s == OURS) ((TOutfitIcon *)Control[s])->SetColor(Mask, 3);
			else CallI<void>(0x457440, Control[s], Mask, 3); });
		Direct(K_OUTFIT, "TOutfitIcon::SetColors NULL", [&](int s) {
			if (s == OURS) ((TOutfitIcon *)Control[s])->SetColors(NULL);
			else CallI<void>(0x4575a0, Control[s], (int *)NULL); });
		static const int ButtonKinds[] = {K_SKINBUTTON, K_COLORBUTTON, K_ICONBUTTON, K_BLINKING, K_TEXTBUTTON, K_TOGGLE,
			K_TEXTTOGGLE, K_MULTI};
		for (int k : ButtonKinds) {
			int Up = r % 2 == 0 ? NumberOfSkins + r : 30, Down = r % 2 == 0 ? 30 : NumberOfSkins + r;
			Direct(k, "TSkinButton::SetSkins", [&](int s) {
				if (s == OURS) ((TSkinButton *)Control[s])->SetSkins(Up, Down);
				else CallI<void>(0x458b20, Control[s], Up, Down); });
			if (r == 0) {
				Direct(k, "TSkinButton::SetRadioButtonGroup twice", [&](int s) {
					for (int i = 0; i < 2; i++) {
						if (s == OURS) ((TSkinButton *)Control[s])->SetRadioButtonGroup(Radio[s]);
						else CallI<void>(0x458de0, Control[s], Radio[s]);
					} });
			}
		}
		// The edit lines: unknown events, texts and positions outside.
		Direct(K_EDIT, "TEditLine::HandleEvent unknown", [&](int s) { Control[s]->HandleEvent(r % 2 == 0 ? 15 : 21 + r); });
		Direct(K_EDITTEXT, "TEditText::HandleEvent unknown", [&](int s) { Control[s]->HandleEvent(r % 2 == 0 ? 15 : 21 + r); });
		Direct(K_EDIT, "TEditLine::SetFocusGroup NULL", [&](int s) {
			if (s == OURS) Edit(s)->SetFocusGroup(NULL);
			else CallI<void>(0x468470, Control[s], (TFocusGroup *)NULL); });
		Direct(K_EDIT, "TEditLine::SetText NULL", [&](int s) { Edit(s)->SetText(NULL); });
		Direct(K_LABEL, "TLabel::SetText NULL", [&](int s) { ((TLabel *)Control[s])->SetText(NULL, r % 2 != 0); });
		Direct(K_FORMATTED, "TFormattedText::SetText NULL", [&](int s) {
			((TFormattedText *)Control[s])->SetText(NULL, r % 2 != 0); });
		Direct(K_EDITTEXT, "TEditText::SetText NULL", [&](int s) { ((TEditText *)Control[s])->SetText(NULL, r % 2 != 0); });
		Direct(r % 2 == 0 ? K_EDIT : K_EDITTEXT, "TEditLine::InsertText after the end", [&](int s) {
			Edit(s)->SetText("abc");
			Logf("Inserted%d;", Edit(s)->InsertText(4 + r, "x", 1)); });
		Direct(r % 2 == 0 ? K_EDIT : K_EDITTEXT, "TEditLine::InsertChar after the end", [&](int s) {
			Edit(s)->SetText("abc");
			Edit(s)->InsertChar(4 + r, 'x'); });
		Direct(r % 2 == 0 ? K_EDIT : K_EDITTEXT, "TEditLine::InsertText too long", [&](int s) {
			// The cursor after the position: the rest is counted twice.
			TEditLine *E = Edit(s);
			E->SetText("abcdefghij");
			E->Cursor = 10;
			int Length = E->MaxLength - 10 - r;
			Logf("Inserted%d;", Length >= 0 ? E->InsertText(0, Filler, Length) : 0); });
		Direct(K_EDITTEXT, "TEditText::FormatText without a width", [&](int s) {
			((TEditText *)Control[s])->FieldWidth = -r;
			if (s == OURS) ((TEditText *)Control[s])->FormatText();
			else CallI<void>(0x45da90, Control[s]); });
		// The focus group: members twice or NULL, Next and Previous without an
		// active member or with one that is not in the group.
		Direct(K_EDIT, "TFocusGroup::AddMember twice", [&](int s) {
			TEditLine *M = r % 2 == 0 ? (TEditLine *)Control[s] : NULL;
			if (Group[s] == NULL) return;
			if (s == OURS) Group[s]->AddMember(M);
			else CallI<void>(0x45dfc0, Group[s], M); }, true);
		for (int Previous = 0; Previous < 2; Previous++) {
			Direct(K_EDIT, Previous ? "TFocusGroup::Previous" : "TFocusGroup::Next", [&](int s) {
				if (Group[s] == NULL) return;
				Group[s]->Active = r % 2 == 0 ? NULL : (TEditLine *)Root[s];
				if (s == OURS) {
					if (Previous) Group[s]->Previous();
					else Group[s]->Next();
				} else {
					CallI<void>(Previous ? 0x45e630 : 0x45e290, Group[s]);
				} }, true);
		}
		// Ranges, lines and the progress outside.
		Direct(r % 2 == 0 ? K_VRANGE : K_VSCROLL, "TScrollBarRange::SetRange", [&](int s) {
			TScrollBarRange *R = Kind == K_VRANGE ? (TScrollBarRange *)Control[s] : ((TScrollBar *)Control[s])->Range;
			if (s == OURS) R->SetRange(10 + r, 5, 7, 2);
			else CallI<void>(0x460450, R, 10 + r, 5, 7, 2); });
		for (int What = 0; What < 5; What++) {
			Direct(K_LIST, "TListBox lines", [&](int s) {
				TListBox *L = (TListBox *)Control[s];
				int Lines = L->Lines, Bad = r % 2 == 0 ? Lines + r : -2 - r;
				switch (What) {
				case 0: if (s == OURS) L->AddLine(NULL); else CallI<void>(0x462460, L, (const char *)NULL); break;
				case 1: if (s == OURS) L->SetLine(0, NULL); else CallI<void>(0x462730, L, 0, (const char *)NULL); break;
				case 2: if (s == OURS) L->SetLine(Bad, "x"); else CallI<void>(0x462730, L, Bad, "x"); break;
				case 3: Logf("Line%s;", s == OURS ? L->GetLine(Bad) : CallI<char *>(0x462b20, L, Bad)); break;
				case 4:
					// (an empty list takes every selection as none)
					if (s == OURS) L->AddLine("a"); else CallI<void>(0x462460, L, "a");
					if (s == OURS) L->SetSelection(Bad + 1); else CallI<void>(0x46ad10, L, Bad + 1);
					break;
				} });
			Direct(K_FONTLIST, "TFontListBox lines", [&](int s) {
				TFontListBox *L = (TFontListBox *)Control[s];
				int Lines = L->Lines, Bad = r % 2 == 0 ? Lines + r : -2 - r;
				TFont Got;
				switch (What) {
				case 0: if (s == OURS) L->AddLine(NULL, F); else CallI<void>(0x463820, L, (const char *)NULL, F); break;
				case 1: if (s == OURS) L->AddLine(-1 - r); else CallI<void>(0x463b10, L, -1 - r); break;
				case 2: if (s == OURS) L->SetLine(0, NULL, F); else CallI<void>(0x463dc0, L, 0, (const char *)NULL, F); break;
				case 3: if (s == OURS) L->SetLine(Bad, "x", F); else CallI<void>(0x463dc0, L, Bad, "x", F); break;
				case 4: Logf("Line%s;", s == OURS ? L->GetLine(Bad, &Got) : CallI<char *>(0x4643a0, L, Bad, &Got)); break;
				} });
		}
		Direct(K_PROGRESS, "TProgressBar::SetMaximum", [&](int s) {
			if (s == OURS) ((TProgressBar *)Control[s])->SetMaximum(-r);
			else CallI<void>(0x464f40, Control[s], -r); });

		// Negative sizes (TComponent::SetSize's assertion under each SetSize).
		for (int k = 0; k < KINDS; k++)
			Direct(k, "SetSize negative", [&](int s) { Control[s]->SetSize(r % 2 == 0 ? -1 - r : 30, r % 2 == 0 ? 20 : -1 - r); });
		// Presses while the parent captures another component; a range's
		// release whose parent captures another one.
		static const int PressKinds[] = {K_RESIZER, K_SKINBUTTON, K_COLORBUTTON, K_ICONBUTTON, K_BLINKING, K_TEXTBUTTON,
			K_TOGGLE, K_TEXTTOGGLE, K_MULTI, K_HRANGE, K_VRANGE};
		for (int k : PressKinds) {
			Direct(k, "LeftButtonDown with a capture", [&](int s) {
				Root[s]->Capture = Receiver[s];
				Logf("D%d;", Control[s]->LeftButtonDown(2, 2, 0) ? 1 : 0); });
		}
		Direct(r % 2 == 0 ? K_VRANGE : K_HRANGE, "LeftButtonUp with another capture", [&](int s) {
			if (Kind == K_VRANGE) ((TScrollBarRange *)Control[s])->Pressed = true;
			else ((THorizontalScrollBarRange *)Control[s])->Pressed = true;
			Root[s]->Capture = Receiver[s];
			Logf("U%d;", Control[s]->LeftButtonUp(2, 2, 0) ? 1 : 0); });
		Direct(K_EDIT, "TEditLine::LeftButtonUp selecting with a capture", [&](int s) {
			Edit(s)->Selecting = true;
			((TWindow *)Control[s])->Capture = Receiver[s];
			Logf("U%d;", Control[s]->LeftButtonUp(2, 2, 0) ? 1 : 0); });
		Direct(K_EDIT, "TEditLine::SetFocusGroup again", [&](int s) {
			if (Group[s] == NULL) return;
			if (s == OURS) Edit(s)->SetFocusGroup(Group[s]);
			else CallI<void>(0x468470, Control[s], Group[s]); }, true);
		// Constructors whose callees assert: a frame around a window that has a
		// parent, a formatted text without a text, an outfit without colours, a
		// button with a skin outside, an edit line and a range of negative width,
		// scroll bars with a minimum above the maximum.
		Direct(K_SEPARATOR, "TBorder::TBorder window with a parent", [&](int s) {
			TWindow *W = (TWindow *)Control[s];
			if (r % 3 == 0) {
				if (s == OURS) new TBorder(W, r);
				else CallI<void *>(0x4547f0, PatternNew(0x68), W, r);
			} else if (r % 3 == 1) {
				if (s == OURS) new TBorder(W, 26, 32, 27, 30, 31, 28, 33, 29, r);
				else CallI<void *>(0x454b10, PatternNew(0x68), W, 26, 32, 27, 30, 31, 28, 33, 29, r);
			} else {
				if (s == OURS) new TBorder(W, 26, 32, 27, 30, 31, 28, 33, 29, 7, r);
				else CallI<void *>(0x454e30, PatternNew(0x68), W, 26, 32, 27, 30, 31, 28, 33, 29, 7, r);
			} });
		Direct(K_SEPARATOR, "TFormattedText::TFormattedText NULL", [&](int s) {
			if (s == OURS) new TFormattedText(NULL);
			else CallI<void *>(0x465e60, PatternNew(0x38), (const char *)NULL); });
		Direct(K_SEPARATOR, "TOutfitIcon::TOutfitIcon without colours", [&](int s) {
			TRGBColor C(1, 2, 3);
			if (s == OURS) new TOutfitIcon(130, r, NULL, &C);
			else CallI<void *>(0x4666a0, PatternNew(0x48), 130, r, (int *)NULL, &C); });
		Direct(K_SEPARATOR, "TSkinButton::TSkinButton skin outside", [&](int s) {
			TCallback CB(Receiver[s], 1);
			if (s == OURS) new TSkinButton(1, 2, 30, NumberOfSkins + r, CB, "Tip", false);
			else CallI<void *>(0x466860, PatternNew(0x44), 1, 2, 30, NumberOfSkins + r, CB, "Tip", false); });
		Direct(K_SEPARATOR, "TEditLine::TEditLine negative width", [&](int s) {
			TRect Area(1, 2, r % 2 == 0 ? -1 - r : 60, r % 2 == 0 ? 16 : -1 - r);
			if (s == OURS) new TEditLine(Area, 20, false);
			else CallI<void *>(0x467b50, PatternNew(0x68), Area, 20, false); });
		Direct(K_SEPARATOR, "TScrollBarRange::TScrollBarRange negative width", [&](int s) {
			if (s == OURS) new TScrollBarRange(0, 0, -1 - r, 40, 0, 10, 5, 2);
			else CallI<void *>(0x469670, PatternNew(0x4c), 0, 0, -1 - r, 40, 0, 10, 5, 2); });
		Direct(K_SEPARATOR, "THorizontalScrollBar::THorizontalScrollBar range", [&](int s) {
			TCallback CB(Receiver[s], 1);
			if (s == OURS) new THorizontalScrollBar(Min, Max, Value, 3, 1, CB);
			else CallI<void *>(0x4692d0, PatternNew(0x38), Min, Max, Value, 3, 1, CB); });
		Direct(K_SEPARATOR, "TScrollBar::TScrollBar range", [&](int s) {
			if (s == OURS) new TScrollBar(TRect(1, 2, 12, 60), Min, Max, Value, 3, 1);
			else CallI<void *>(0x469a80, PatternNew(0x38), TRect(1, 2, 12, 60), Min, Max, Value, 3, 1); });
	}
	// A label's constructor and a check box's SetEnabled, failing at each of
	// their fake calls.
	for (int Round = 0; Round < 3; Round++) {
		TFont F = RandomFont();
		const char *Text = Round == 0 ? "Hello world, this is a longer text" : Round == 1 ? "two\nlines" : "x";
		Sweep(K_SEPARATOR, "TLabel::TLabel", [&](int s) {
			if (s == OURS) new TLabel(1, 2, 40, 12, F, Text, Round != 1);
			else CallI<void *>(0x465510, PatternNew(0x4c), 1, 2, 40, 12, F, Text, Round != 1); });
		Sweep(K_CHECKBOX, "TCheckBox::SetEnabled", [&](int s) {
			if (s == OURS) ((TCheckBox *)Control[s])->SetEnabled(Round != 1);
			else CallI<void>(0x4678d0, Control[s], Round != 1); });
	}
}

void TestDialogElements()
{
	int Before = Checks, BeforeFailures = Failures;
	bool Trace = getenv("DE_TRACE") != NULL;
	if (Trace)
		setvbuf(stdout, NULL, _IONBF, 0);
	if (getenv("DE_LOGS") != NULL)
		LogFile = fopen(getenv("DE_LOGS"), "w");
	PatchImage();
	InitSkins();
	Global<int>(0x71c450) = NumberOfSkins;
	Global<TSkin *>(0x71c454) = Skins;
	InitSkinProperties();
	At<void (__cdecl *)()>(0x498160)();
	Surface = new TDESurface;
	PatchCallees();
	FindVtables();

	int KindCount[KINDS] = {};
	for (int Round = 0; Round < 1600; Round++) {
		Kind = Random() % KINDS;
		KindCount[Kind]++;
		if (Trace)
			fprintf(stderr, "round %d kind %s\n", Round, KindNames[Kind]);
		TArguments A = RandomArguments();
		Testing = true;
		CurrentAction = ACTION_CONSTRUCT;
		Construct(A);
		OperationName = "constructor";
		Compare();
		int Width, Height;
		RandomSize(&Width, &Height);
		OperationName = "first SetSize";
		CurrentAction = ACTION_FIRSTSIZE;
		Both([&](int s) { Control[s]->SetSize(Width, Height); });
		Compare();
		int Operations = 20 + Random() % 40;
		for (int i = 0; i < Operations; i++) {
			Operation();
			if (Trace)
				fprintf(stderr, "  %s\n", OperationName);
		}
		CurrentAction = ACTION_DESTRUCT;
		Destruct();
		OperationName = "destructor";
		CHECK(Log[OURS] == Log[ORIGINAL], "DialogElements destructor (%s): logs differ\n  ours: %s\n  orig: %s",
			KindNames[Kind], Log[OURS].c_str(), Log[ORIGINAL].c_str());
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		Roles[OURS].clear();
		Roles[ORIGINAL].clear();
		Testing = false;
	}

	// The error round (issue #34): its checks are reported apart.
	int RoundChecks = Checks, SavedCounts[64];
	memcpy(SavedCounts, OperationCount, sizeof(SavedCounts));
	TestDialogElementsErrors();
	memcpy(OperationCount, SavedCounts, sizeof(SavedCounts));
	RoundChecks = Checks - RoundChecks;

	RestoreAll();
	if (LogFile != NULL)
		fclose(LogFile);
	int MinKind = 1 << 30, MinOperation = 1 << 30;
	for (int k = 0; k < KINDS; k++)
		MinKind = KindCount[k] < MinKind ? KindCount[k] : MinKind;
	for (int i = 0; i < 22; i++)
		MinOperation = OperationCount[i] < MinOperation ? OperationCount[i] : MinOperation;
	printf("DialogElements: %d checks, %d failures (each kind at least %d times, each operation at least %d times)\n",
		Checks - Before - RoundChecks, Failures - BeforeFailures, MinKind, MinOperation);
}
