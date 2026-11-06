// The redirects of the DialogElements.cpp fakes (elementfakes.h): the oracle's (x86, the image's and our
// constructors and members) and the runner's (x64, ours).

#include "golden_test.h"

#include "elementfakes.h"

#include <new>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

template <class T> static void *Member(T Function) { return *(void **)&Function; }


// --- The runner (x64): our layout, our constructors -------------------------------------------------

// Each named member of a fake is where our code reads the real member it stands for, each fake as large as
// its real class (our constructors are called on memory of the real class's size).
#define GOLDEN_SAME_OFFSET(Fake, FakeMember, Real, RealMember) \
	static_assert(offsetof(Fake, FakeMember) == offsetof(Real, RealMember), #Fake "::" #FakeMember)
#define GOLDEN_SAME_SIZE(Fake, Real) static_assert(sizeof(Fake) == sizeof(Real), "sizeof(" #Fake ")")
GOLDEN_SAME_SIZE(TFakeResizer, TResizer);
GOLDEN_SAME_SIZE(TFakeSkinButton, TSkinButton);
GOLDEN_SAME_OFFSET(TFakeSkinButton, ButtonUp, TSkinButton, ButtonUp);
GOLDEN_SAME_OFFSET(TFakeSkinButton, ButtonDown, TSkinButton, ButtonDown);
GOLDEN_SAME_OFFSET(TFakeSkinButton, Data2C, TSkinButton, Pressed);
GOLDEN_SAME_OFFSET(TFakeSkinButton, RadioButtonGroup, TSkinButton, RadioButtonGroup);
GOLDEN_SAME_OFFSET(TFakeSkinButton, Tooltip, TSkinButton, Tooltip);
GOLDEN_SAME_OFFSET(TFakeSkinButton, Data40, TSkinButton, Enabled);
static_assert(sizeof(TFakeSkinButton::Data2C) >= sizeof(TCallback), "TFakeSkinButton::Data2C holds a TCallback");
GOLDEN_SAME_SIZE(TFakeToggleButton, TToggleButton);
GOLDEN_SAME_OFFSET(TFakeToggleButton, State, TToggleButton, State);
GOLDEN_SAME_SIZE(TFakeMultiStateButton, TMultiStateButton);
GOLDEN_SAME_OFFSET(TFakeMultiStateButton, State, TMultiStateButton, State);
GOLDEN_SAME_OFFSET(TFakeMultiStateButton, States, TMultiStateButton, States);
GOLDEN_SAME_OFFSET(TFakeMultiStateButton, ButtonUps, TMultiStateButton, ButtonUps);
GOLDEN_SAME_OFFSET(TFakeMultiStateButton, ButtonDowns, TMultiStateButton, ButtonDowns);
GOLDEN_SAME_OFFSET(TFakeMultiStateButton, Data54, TMultiStateButton, Callbacks);
GOLDEN_SAME_OFFSET(TFakeScrollBarRange, Value, TScrollBarRange, Value);
GOLDEN_SAME_SIZE(TFakeScrollBar, TScrollBar);
GOLDEN_SAME_OFFSET(TFakeScrollBar, Range, TScrollBar, Range);
GOLDEN_SAME_SIZE(TFakeEditLine, TEditLine);
GOLDEN_SAME_OFFSET(TFakeEditLine, Text, TEditLine, Text);
GOLDEN_SAME_OFFSET(TFakeEditLine, Data30, TEditLine, MaxLength);
GOLDEN_SAME_OFFSET(TFakeEditLine, Data5C, TEditLine, SelectionStart);
GOLDEN_SAME_OFFSET(TFakeEditLine, Data60, TEditLine, SelectionLength);
GOLDEN_SAME_OFFSET(TFakeEditLine, Data64, TEditLine, Selecting);
GOLDEN_SAME_OFFSET(TFakeEditLine, Data66, TEditLine, ReadOnly);
GOLDEN_SAME_SIZE(TFakeFormattedText, TFormattedText);
GOLDEN_SAME_SIZE(TFakeBorder, TBorder);
GOLDEN_SAME_SIZE(TFakeTextButton, TTextButton);
GOLDEN_SAME_SIZE(TFakeTextToggleButton, TTextToggleButton);
GOLDEN_SAME_OFFSET(TFakeTextToggleButton, State, TTextToggleButton, State);
GOLDEN_SAME_OFFSET(TFakeTextToggleButton, Tooltip1, TTextToggleButton, Tooltip1);
GOLDEN_SAME_OFFSET(TFakeTextToggleButton, Tooltip2, TTextToggleButton, Tooltip2);
GOLDEN_SAME_SIZE(TFakeLabel, TLabel);
GOLDEN_SAME_SIZE(TFakeSkinIcon, TSkinIcon);
GOLDEN_SAME_OFFSET(TFakeSkinIcon, Skin, TSkinIcon, Skin);
GOLDEN_SAME_SIZE(TFakeBlinkingButton, TBlinkingButton);

// Our constructors through their decorated names: MSVC x64's, and on Linux (issue #91) the Itanium ABI's
// (golden_test.h GOLDEN_SYMBOL).
#if defined(_MSC_VER)
extern "C" void OurResizerConstructor();
extern "C" void OurSkinButtonConstructor();
extern "C" void OurToggleButtonConstructor();
extern "C" void OurMultiStateButtonConstructor();
extern "C" void OurScrollBarConstructor();
extern "C" void OurEditLineConstructor();
extern "C" void OurFormattedTextConstructor();
extern "C" void OurBorderConstructor1();
extern "C" void OurBorderConstructor2();
extern "C" void OurTextButtonConstructor1();
extern "C" void OurTextButtonConstructor2();
extern "C" void OurTextToggleButtonConstructor();
extern "C" void OurLabelConstructor();
extern "C" void OurLabelAreaConstructor();
extern "C" void OurSkinIconConstructor();
extern "C" void OurBlinkingButtonConstructor();
#pragma comment(linker, "/alternatename:OurResizerConstructor=??0TResizer@@QEAA@PEAVTComponent@@_N11@Z")
#pragma comment(linker, "/alternatename:OurSkinButtonConstructor=??0TSkinButton@@QEAA@HHHHUTCallback@@PEBD_N@Z")
#pragma comment(linker, "/alternatename:OurToggleButtonConstructor=??0TToggleButton@@QEAA@HHHHUTCallback@@PEBD1_N@Z")
#pragma comment(linker, "/alternatename:OurMultiStateButtonConstructor=??0TMultiStateButton@@QEAA@HHPEBDHHHUTCallback@@ZZ")
#pragma comment(linker, "/alternatename:OurScrollBarConstructor=??0TScrollBar@@QEAA@UTRect@@HHHHH@Z")
#pragma comment(linker, "/alternatename:OurEditLineConstructor=??0TEditLine@@QEAA@UTRect@@H_N@Z")
#pragma comment(linker, "/alternatename:OurFormattedTextConstructor=??0TFormattedText@@QEAA@PEBD@Z")
#pragma comment(linker, "/alternatename:OurBorderConstructor1=??0TBorder@@QEAA@PEAVTComponent@@H@Z")
#pragma comment(linker, "/alternatename:OurBorderConstructor2=??0TBorder@@QEAA@PEAVTComponent@@HHHHHHHHHH@Z")
#pragma comment(linker, "/alternatename:OurTextButtonConstructor1=??0TTextButton@@QEAA@HHPEBDHHUTCallback@@0@Z")
#pragma comment(linker, "/alternatename:OurTextButtonConstructor2=??0TTextButton@@QEAA@HHPEBDUTCallback@@0@Z")
#pragma comment(linker, "/alternatename:OurTextToggleButtonConstructor=??0TTextToggleButton@@QEAA@HHPEBDHHUTCallback@@00_N@Z")
#pragma comment(linker, "/alternatename:OurLabelConstructor=??0TLabel@@QEAA@UTFont@@PEBD@Z")
#pragma comment(linker, "/alternatename:OurLabelAreaConstructor=??0TLabel@@QEAA@HHHHUTFont@@PEBD_N@Z")
#pragma comment(linker, "/alternatename:OurSkinIconConstructor=??0TSkinIcon@@QEAA@H@Z")
#pragma comment(linker, "/alternatename:OurBlinkingButtonConstructor=??0TBlinkingButton@@QEAA@HHHHHHUTCallback@@PEBD@Z")
#else
GOLDEN_SYMBOL(OurResizerConstructor, "_ZN8TResizerC1EP10TComponentbbb");
GOLDEN_SYMBOL(OurSkinButtonConstructor, "_ZN11TSkinButtonC1Eiiii9TCallbackPKcb");
GOLDEN_SYMBOL(OurToggleButtonConstructor, "_ZN13TToggleButtonC1Eiiii9TCallbackPKcS2_b");
GOLDEN_SYMBOL(OurMultiStateButtonConstructor, "_ZN17TMultiStateButtonC1EiiPKciii9TCallbackz");
GOLDEN_SYMBOL(OurScrollBarConstructor, "_ZN10TScrollBarC1E5TRectiiiii");
GOLDEN_SYMBOL(OurEditLineConstructor, "_ZN9TEditLineC1E5TRectib");
GOLDEN_SYMBOL(OurFormattedTextConstructor, "_ZN14TFormattedTextC1EPKc");
GOLDEN_SYMBOL(OurBorderConstructor1, "_ZN7TBorderC1EP10TComponenti");
GOLDEN_SYMBOL(OurBorderConstructor2, "_ZN7TBorderC1EP10TComponentiiiiiiiiii");
GOLDEN_SYMBOL(OurTextButtonConstructor1, "_ZN11TTextButtonC1EiiPKcii9TCallbackS1_");
GOLDEN_SYMBOL(OurTextButtonConstructor2, "_ZN11TTextButtonC1EiiPKc9TCallbackS1_");
GOLDEN_SYMBOL(OurTextToggleButtonConstructor, "_ZN17TTextToggleButtonC1EiiPKcii9TCallbackS1_S1_b");
GOLDEN_SYMBOL(OurLabelConstructor, "_ZN6TLabelC1E5TFontPKc");
GOLDEN_SYMBOL(OurLabelAreaConstructor, "_ZN6TLabelC1Eiiii5TFontPKcb");
GOLDEN_SYMBOL(OurSkinIconConstructor, "_ZN9TSkinIconC1Ei");
GOLDEN_SYMBOL(OurBlinkingButtonConstructor, "_ZN15TBlinkingButtonC1Eiiiiii9TCallbackPKc");
#endif

// The constructors' stand-ins. On x64 a constructor (a member function) takes this as its first argument,
// before the others, as a plain function does (the oracle's need __fastcall and a dummy for x86's ECX):
// each stand-in has the constructor's own parameter types (bool where the constructor has bool), with this
// first, and passes them to the fake as the oracle's stand-ins do.
namespace {

void *NewResizer(void *This, TComponent *ToResize, bool P2, bool P3, bool P4)
{
	return new (This) TFakeResizer(ToResize, P2, P3, P4);
}
void *NewSkinButton(void *This, int x, int y, int Up, int Down, TCallback Callback, const char *Tooltip, bool Param)
{
	return new (This) TFakeSkinButton(x, y, Up, Down, Callback, Tooltip, Param);
}
void *NewToggleButton(void *This, int x, int y, int Up, int Down, TCallback Callback, const char *Tooltip1,
	const char *Tooltip2, bool Param)
{
	return new (This) TFakeToggleButton(x, y, Up, Down, Callback, Tooltip1, Tooltip2, Param);
}
void *NewScrollBar(void *This, TRect Area, int P2, int P3, int P4, int P5, int P6)
{
	return new (This) TFakeScrollBar(Area, P2, P3, P4, P5, P6);
}
void *NewEditLine(void *This, TRect Area, int MaxLength, bool Param)
{
	return new (This) TFakeEditLine(Area, MaxLength, Param);
}
void *NewFormattedText(void *This, const char *Text) { return new (This) TFakeFormattedText(Text); }
void *NewBorder1(void *This, TWindow *W, int P) { return new (This) TFakeBorder(W, P); }
void *NewBorder2(void *This, TWindow *W, int S1, int S2, int S3, int S4, int S5, int S6, int S7, int S8, int S9, int B)
{
	return new (This) TFakeBorder(W, S1, S2, S3, S4, S5, S6, S7, S8, S9, B);
}
void *NewTextButton1(void *This, int x, int y, const char *Text, int S1, int S2, TCallback C, const char *P)
{
	return new (This) TFakeTextButton(x, y, Text, S1, S2, C, P);
}
void *NewTextButton2(void *This, int x, int y, const char *Text, TCallback C, const char *Tooltip)
{
	return new (This) TFakeTextButton(x, y, Text, C, Tooltip);
}
void *NewTextToggleButton(void *This, int x, int y, const char *Text, int S1, int S2, TCallback C, const char *T1,
	const char *T2, bool State)
{
	return new (This) TFakeTextToggleButton(x, y, Text, S1, S2, C, T1, T2, State);
}
void *NewLabel(void *This, TFont Font, const char *Text) { return new (This) TFakeLabel(Font, Text); }
void *NewLabelArea(void *This, int x, int y, int Width, int Height, TFont Font, const char *Text, bool Param)
{
	return new (This) TFakeLabel(x, y, Width, Height, Font, Text, Param);
}
void *NewSkinIcon(void *This, int Skin) { return new (This) TFakeSkinIcon(Skin); }
void *NewBlinkingButton(void *This, int x, int y, int S1, int S2, int S3, int S4, TCallback C, const char *Tooltip)
{
	return new (This) TFakeBlinkingButton(x, y, S1, S2, S3, S4, C, Tooltip);
}

} // namespace

// consolewindow_tests.cpp: the varargs constructor (both sides).
void *NewMultiStateButton(void *This, int x, int y, const char *Tooltip, int States, ...);

void PatchElementFakes(void (*Redirect)(void *Function, void *Target))
{
	struct {
		void *Ours;
		void *Fake;
	} Table[] = {
		{(void *)OurResizerConstructor, (void *)NewResizer},
		{(void *)OurSkinButtonConstructor, (void *)NewSkinButton},
		{(void *)OurToggleButtonConstructor, (void *)NewToggleButton},
		{(void *)OurMultiStateButtonConstructor, (void *)NewMultiStateButton},
		{(void *)OurScrollBarConstructor, (void *)NewScrollBar},
		{(void *)OurEditLineConstructor, (void *)NewEditLine},
		{(void *)OurFormattedTextConstructor, (void *)NewFormattedText},
		{(void *)OurBorderConstructor1, (void *)NewBorder1},
		{(void *)OurBorderConstructor2, (void *)NewBorder2},
		{(void *)OurTextButtonConstructor1, (void *)NewTextButton1},
		{(void *)OurTextButtonConstructor2, (void *)NewTextButton2},
		{(void *)OurTextToggleButtonConstructor, (void *)NewTextToggleButton},
		{(void *)OurLabelConstructor, (void *)NewLabel},
		{(void *)OurLabelAreaConstructor, (void *)NewLabelArea},
		{(void *)OurSkinIconConstructor, (void *)NewSkinIcon},
		{(void *)OurBlinkingButtonConstructor, (void *)NewBlinkingButton},
		{Member(&TEditLine::SetActive), Member(&TFakeEditLine::SetActive)},
		{Member(&TScrollBarRange::SetRange), Member(&TFakeScrollBarRange::SetRange)},
		{Member(&TScrollBarRange::SetValue), Member(&TFakeScrollBarRange::SetValue)},
		{Member(&TSkinButton::SetSkins), Member(&TFakeSkinButton::SetSkins)},
		{Member(&TSkinButton::SetRadioButtonGroup), Member(&TFakeSkinButton::SetRadioButtonGroup)},
		{Member(&TBorder::FitToWindow), Member(&TFakeBorder::FitToWindow)},
	};
	for (auto &T : Table)
		Redirect(T.Ours, T.Fake);
}

// The fakes themselves: dialogwindow_tests.cpp, contextmenus_tests.cpp, consolewindow_tests.cpp,
// contentwindows_tests.cpp, mapwindow_tests.cpp (the blinking button) and mainwindow_tests.cpp (the border, the
// first text button constructor, the label's font constructor), on both sides.
