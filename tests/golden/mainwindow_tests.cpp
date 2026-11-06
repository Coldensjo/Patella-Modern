// difftest.exe's MainWindow test (moved here from tools/difftest/mainwindowtest.cpp, issue #27): MainWindow.cpp
// against the original, as a golden-corpus test (golden.h).
//
// The windows MainWindow.cpp creates from files that are not reconstructed
// yet (the console, the frames, buttons, labels, the side-bar windows and
// the content windows' bodies) are fakes of this file on both sides: the
// image's constructors and members jump to the same code as ours, which
// logs every call per side (Side) and builds the same objects. The dialog
// windows (DialogWindow.cpp) are real on both sides: the image's for the
// image's main window, ours for ours; the ContentWindows.cpp creators make
// the image's or our TSubWindow and TContentWindow around a shared fake body. The other
// callees (Preferences, Status, Talk, Creatures, Inventory, GUI hotkeys,
// graphics, the surface, Communication and Network) are fakes as well, for
// the image and for our code (PatchOwn). The map frame is real on both sides
// while the window is built (the image's TMapFrame and ours); right after
// that its vtable is replaced by a logging fake, so that drawing does not
// reach the map. TWindow, TSplitter and TStartWindow are the image's and ours.
//
// Both main windows (InitMainWindow, 0x4b05b0) get the same random
// operations: mode changes, sizes, drawing, keys, events, the console
// splitter, containers, trade and content windows, the start window's
// events, ExitMainWindow. After each one the logs and a dump of both window
// trees (classes, positions, sizes, drawing lists, captures, the members of
// TMainWindow, TSplitter, TStartWindow and the dialog windows and the fakes'
// arguments) are compared. Failing assertions are not exercised.
//
// By value (plan 4.3): the original's side exists in the oracle only (every use of it is inside ORIGINAL or
// #if GOLDEN_ORACLE); the runner has our main windows alone. The dump was text already (classes, geometry,
// drawing lists, the members by name), so per comparison the corpus holds the original's log (text), its dump
// (text, by its changes since the last one, golden::OutputTextChanges), the state its fakes keep (the action
// and connection states, the open dialogs, the fullscreen mode), the results of its calls, whether it had a
// trade window, how many fake calls its call made (the error round draws the failing call from them) and what
// it threw. The fakes of this file (the console, the side-bar windows, the border, the first text button
// constructor, the label's font constructor) have our x64 layout in the runner, as elementfakes.h's: each as
// large as its real class; our constructors are redirected to them through their x64 decorated names. The
// runner redirects our functions to the fakes with golden::RedirectOwn, for good, as the oracle does.

#include "golden_test.h"

#include "elementfakes.h"
#include "golden_eh.h"
#include "testsurface.h"
#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#else
#include "golden_patch.h"
#endif

#include "Communication.h"
#include "ConsoleWindow.h"
#include "ContentWindows.h"
#include "Control.h"
#include "Creatures.h"
#include "DialogWindow.h"
#include "GUI.h"
#include "Inventory.h"
#include "MainWindow.h"
#include "MapWindow.h"
#include "Network.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "graphics.h"

#include <map>
#include <new>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#if GOLDEN_ORACLE
static_assert(sizeof(TMainWindow) == 0x4c, "TMainWindow is 0x4c bytes");
static_assert(sizeof(TStartWindow) == 0x30, "TStartWindow is 0x30 bytes");
static_assert(sizeof(TSplitter) == 0x30, "TSplitter is 0x30 bytes");
static_assert(offsetof(TMainWindow, MapFrame) == 0x2c, "TMainWindow::MapFrame at +0x2c");
static_assert(offsetof(TMainWindow, InGame) == 0x48, "TMainWindow::InGame at +0x48");
static_assert(offsetof(TSplitter, DragY) == 0x2c, "TSplitter::DragY at +0x2c");
static_assert(sizeof(TConsoleWindow) == 0x6c, "TConsoleWindow is 0x6c bytes");
static_assert(sizeof(TBorder) == 0x68, "TBorder is 0x68 bytes");
static_assert(sizeof(TTextButton) == 0x78, "TTextButton is 0x78 bytes");
static_assert(sizeof(TLabel) == 0x4c, "TLabel is 0x4c bytes");
static_assert(sizeof(TObjectIcon) == 0x50, "TObjectIcon is 0x50 bytes");
static_assert(sizeof(TAutomapWindow) == 0x80 && sizeof(TStatusWindow) == 0x4c && sizeof(TButtonsWindow) == 0x58,
	"side-bar window sizes");
#endif

// The test's own definitions: in a namespace of their own (the other window tests, linked into the same
// executable, define classes of the same names at global scope). The fakes' members (elementfakes.h's
// classes) are global; they see the namespace through the using-directive below.
namespace MainWindowTest {

enum { OURS = 0,
	ORIGINAL = 1 };
// The sides that have objects: both in the oracle, ours in the runner.
static const int SIDES = GOLDEN_ORACLE ? 2 : 1;
static int Side;
static bool Quiet;
static bool Running; // the shared fakes (TTextButton, TLabel, TObjectIcon) register only while this test runs
static std::string Log[2];

// The error round (issue #34): the FailAt-th logged call of a fake throws an
// assertion of the calling side (0: none); Ticks counts one side's calls. The
// fakes that destructors call (ours are noexcept) do not count (TNoTick).
static int FailAt, Ticks;
static void Tick()
{
	Ticks++;
	if (FailAt > 0 && --FailAt == 0)
		ThrowSideAssertion(Side == ORIGINAL, -Ticks);
}
struct TNoTick {
	int Saved;
	TNoTick() : Saved(FailAt) { FailAt = 0; }
	~TNoTick() { FailAt = Saved; }
};

static void Logf(const char *Format, ...)
{
	if (Quiet)
		return;
	Tick();
	char Buffer[512];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

// Our functions to the fakes, for good (the oracle patches difftest.exe's code with PatchJump, the runner its
// own with golden::RedirectOwn).
static void PatchOwn(void *Function, void *Target)
{
#if GOLDEN_ORACLE
	DWORD Protection;
	VirtualProtect(Function, 5, PAGE_EXECUTE_READWRITE, &Protection);
	PatchJump((uint32)(uintptr_t)Function, Target);
	VirtualProtect(Function, 5, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Function, 5);
#else
	golden::RedirectOwn(Function, Target);
#endif
}

template <class T> static void *Member(T Function) { return *(void **)&Function; }
static void *VtableOf(const void *Object) { return *(void **)Object; }
static void SetVtable(void *Object, void *Vtable) { *(void **)Object = Vtable; }

#if GOLDEN_ORACLE
static TMainWindow *&OriginalMainWindow = Global<TMainWindow *>(0x5d16b0);
static TWindow *&OriginalModalWindow = Global<TWindow *>(0x5d16ac);
#else
// The runner has no original: these are set as the oracle sets the image's globals, never read for the
// original's side (which runs in the oracle only).
static TMainWindow *OriginalMainWindow;
static TWindow *OriginalModalWindow;
#endif
static TMainWindow *Main(int s) { return s == OURS ? MainWindow : OriginalMainWindow; }

// --- Objects: names and serial numbers -----------------------------------------

// What the fakes know about the objects they made, per side.
struct TInfo {
	std::string Kind;
	int Serial;
	int Identifier;     // sub-windows
	std::string Detail; // constructor arguments
};
static std::map<const void *, TInfo> Info[2];
static int Serials[2];
static std::map<void *, std::string> VtableName[2];
static std::map<std::string, bool> IsWindowKind;

static void NameVtable(int s, void *Vtable, const char *Name, bool Window)
{
	VtableName[s][Vtable] = Name;
	IsWindowKind[Name] = Window;
}

static TInfo &Register(const void *Object, const char *Kind, int Identifier = 0)
{
	TInfo &I = Info[Side][Object];
	I.Kind = Kind;
	I.Serial = Serials[Side]++;
	I.Identifier = Identifier;
	I.Detail.clear();
	return I;
}

static std::string NameOf(int s, const void *Object)
{
	if (Object == NULL)
		return "0";
	if (Object == Main(s))
		return "Main";
	auto V = VtableName[s].find(VtableOf(Object));
	std::string Name = V == VtableName[s].end() ? "?" : V->second;
	auto I = Info[s].find(Object);
	if (I != Info[s].end() && I->second.Kind == Name) {
		char Buffer[32];
		snprintf(Buffer, sizeof(Buffer), "#%d/%d", I->second.Serial, I->second.Identifier);
		Name += Buffer;
	}
	return Name;
}
static std::string Name(const void *Object) { return NameOf(Side, Object); }

// --- The fake classes ----------------------------------------------------------

// The fakes' own overrides. Each fake constructor sets the vtable of its
// test subclass at the end (same layout, more overrides).
static int ConsoleMinHeight, BodyMinHeight, BorderMinHeight, FrameMinHeight;

// The console (ConsoleWindow.cpp) is a fake of the same size on both sides:
// the image's constructor and ours (found through its decorated name) build
// it, the image's and our SetActive and OpenDefaultChannel log.
class TFakeConsole : public TWindow {
public:
	TFakeConsole();
	int getMinimumHeight()
	{
		Tick();
		return ConsoleMinHeight;
	}
	void Draw(int Surface, int x, int y) { Logf("DCons%d,%d,%d;", Surface, x, y); }
	void Arrange() { Logf("ACons;"); }
	void KeyPress(int Key) { Logf("KCons%d;", Key); }
	uint8 Data2C[sizeof(TConsoleWindow) - sizeof(TWindow)]; // +0x2c on x86
};
static_assert(sizeof(TFakeConsole) == sizeof(TConsoleWindow), "the fake console has the console's size");
class TMWTestBorder : public TFakeBorder {
public:
	TMWTestBorder(TWindow *Window) : TFakeBorder(Window, 0) {}
	int getMinimumHeight()
	{
		Tick();
		return BorderMinHeight;
	}
	void SetPosition(int x, int y)
	{
		Tick();
		TFakeBorder::SetPosition(x, y);
	}
	void Draw(int Surface, int x, int y) { Logf("D%s,%d,%d,%d;", Name(this).c_str(), Surface, x, y); }
};
class TMWTestButton : public TFakeTextButton {
public:
	TMWTestButton() : TFakeTextButton(0, 0, "", 0, 0, TCallback(), NULL) {}
	void Draw(int Surface, int x, int y) { Logf("D%s,%d,%d,%d;", Name(this).c_str(), Surface, x, y); }
};
// A body needs a height (TSubWindow fits it into its frame). Update is its
// first new virtual (+0xb8, the container body's, which OpenContainer calls).
static void HookBody(TSubWindowBody *Body, const char *Call);
static bool BodyAccepts;
class TFakeBody : public TSubWindowBody {
public:
	virtual void Update() { HookBody(this, "B8"); }
	void Minimize() { HookBody(this, "A0"); }
	void Maximize() { HookBody(this, "A4"); }
	bool IsMinimized()
	{
		HookBody(this, "A8");
		return BodyAccepts;
	}
	int GetInitialHeight()
	{
		Tick();
		return BodyMinHeight;
	}
	int GetContentHeight()
	{
		Tick();
		return BodyMinHeight;
	}
	int getMinimumHeight()
	{
		Tick();
		return BodyMinHeight;
	}
	void SetSize(int W, int H)
	{
		Tick();
		TSubWindowBody::SetSize(W, H);
	}
	void SetPosition(int x, int y)
	{
		Tick();
		TSubWindowBody::SetPosition(x, y);
	}
	void HandleEvent(int Event) { Logf("HBody%s,%d;", Name(this).c_str(), Event); }
	void Draw(int Surface, int x, int y) { Logf("D%s,%d,%d,%d;", Name(this).c_str(), Surface, x, y); }
};
// The map frame after construction (vtable only; TMapFrame's layout).
class TFakeMapFrame : public TWindow {
public:
	int getMinimumHeight()
	{
		Tick();
		return FrameMinHeight;
	}
	void SetSize(int W, int H)
	{
		Logf("SFrame%d,%d;", W, H);
		TWindow::SetSize(W, H - 3);
	}
	void Draw(int Surface, int x, int y) { Logf("DFrame%d,%d,%d;", Surface, x, y); }
	void Arrange() { Logf("AFrame;"); }
#if !GOLDEN_ORACLE
	// The objects with this vtable are our TMapFrames (ChangeMode's new TMapFrame, larger than this class): the
	// deleting destructor frees them without a size, so ASan's sized delete sees no mismatch (issue #65; until then
	// dialogwindow_tests.cpp's operator delete, the runner's for every test, took no size).
	static void operator delete(void *Memory) { ::operator delete(Memory); }
#endif
};

static void *FakeConsoleVtable, *FakeBorderVtable, *FakeButtonVtable, *FakeMapFrameVtable;

TFakeConsole::TFakeConsole()
{
	Register(this, "Console");
	Logf("NCons;");
	memset(Data2C, 0, sizeof(Data2C));
}
// The constructor's and the members' stand-ins (GOLDEN_THISCALL: this in ECX on x86, first on x64).
static void *GOLDEN_THISCALL FakeConsoleNew(GOLDEN_THIS_NAMED(void *This)) { return new (This) TFakeConsole; }
static void GOLDEN_THISCALL FakeConsoleActive(GOLDEN_THIS, bool Active) { Logf("CActive%d;", Active ? 1 : 0); }
// The text buffer (the test's 0x12340 on both sides) as the x86 oracle's %p printed it: eight hex digits.
static void GOLDEN_THISCALL FakeConsoleOpen(GOLDEN_THIS, TTalkBuffer *TextBuffer, const char *Caption)
{
	Logf("COpen%08X,%s;", (unsigned)(uintptr_t)TextBuffer, Caption);
}

} // namespace MainWindowTest
using namespace MainWindowTest;

// Our TConsoleWindow::TConsoleWindow() (a constructor has no address in C++), through its decorated name (x86
// in the oracle, x64 in the runner; on Linux, issue #91, the Itanium ABI's, golden_test.h GOLDEN_SYMBOL).
#if defined(_MSC_VER)
extern "C" void OurConsoleConstructor();
#endif
#if GOLDEN_ORACLE
#pragma comment(linker, "/alternatename:_OurConsoleConstructor=??0TConsoleWindow@@QAE@XZ")
#elif defined(_MSC_VER)
#pragma comment(linker, "/alternatename:OurConsoleConstructor=??0TConsoleWindow@@QEAA@XZ")
#else
GOLDEN_SYMBOL(OurConsoleConstructor, "_ZN14TConsoleWindowC1Ev");
#endif

namespace MainWindowTest {

// The open dialogs of the preferences, per side (ClearOpenDialogs,
// GetOpenDialogs and AddOpenDialog).
static std::vector<int> OpenDialogs[2];

// The test bodies and dialogwindowtest.cpp's FakeUpdateUpButton report here.
static void HookBody(TSubWindowBody *Body, const char *Call) { Logf("B%s%s;", Call, Name(Body).c_str()); }
static bool HookBodyAccepts(TSubWindowBody *) { return BodyAccepts; }
static void HookSetSkins(TSkinButton *Button, int ButtonUp, int ButtonDown)
{
	Logf("Skins%s,%d,%d;", Name(Button).c_str(), ButtonUp, ButtonDown);
}

#if GOLDEN_ORACLE
typedef void *(__fastcall *ContentCtorFn)(void *, void *, int, bool);
typedef void *(__fastcall *SubCtorFn)(void *, void *, int, TSubWindowBody *, bool, bool);
typedef TContentWindow *(__fastcall *FindFn)(void *, void *, int);
#endif

// A content window as the ContentWindows.cpp creators return them: the
// image's or our class (per side), sized to the side bar. The inventory
// windows are plain content windows, the others sub-windows with a body.
static TWindow *NewSubWindow(int Identifier)
{
	int Height = 40 + (Identifier & 31);
	if (Identifier <= 2) {
		TContentWindow *Window;
		if (Side == OURS) {
			Window = new TContentWindow(Identifier, true);
		} else {
#if GOLDEN_ORACLE
			void *Memory = operator new(sizeof(TContentWindow));
			Window = (TContentWindow *)At<ContentCtorFn>(0x4854c0)(Memory, NULL, Identifier, true);
#else
			abort(); // (the original's side: the oracle only)
#endif
		}
		Register(Window, "CW", Identifier);
		Window->SetSize(SkinProperties.Data184, Height);
		Logf("NCW%s;", NameOf(Side, Window).c_str());
		return Window;
	}
	TFakeBody *Body = new TFakeBody;
	Register(Body, "Body", Identifier);
	TSubWindow *Window;
	void *Memory = operator new(sizeof(TSubWindow));
	if (Side == OURS)
		Window = new (Memory) TSubWindow(Identifier, Body, false, Identifier % 2 != 0);
	else {
#if GOLDEN_ORACLE
		Window = (TSubWindow *)At<SubCtorFn>(0x487e80)(Memory, NULL, Identifier, Body, false, Identifier % 2 != 0);
#else
		abort(); // (the original's side: the oracle only)
#endif
	}
	Register(Window, "Sub", Identifier);
	Window->SetSize(SkinProperties.Data184, Height);
	Logf("NSub%s;", NameOf(Side, Window).c_str());
	return Window;
}
// The creators (the image's and ours jump here).
static TWindow *FakeCreateInventoryWindow(TMainWindow *Receiver)
{
	Logf("NInv1%s;", Name(Receiver).c_str());
	return NewSubWindow(1);
}
static TWindow *FakeCreateSmallInventoryWindow(TMainWindow *Receiver)
{
	Logf("NInv2%s;", Name(Receiver).c_str());
	return NewSubWindow(2);
}
static TWindow *FakeCreateContainerWindow(int Container)
{
	Logf("NCont%d;", Container);
	return NewSubWindow(0x40 + Container);
}
static TWindow *FakeCreateSkillsWindow() { return NewSubWindow(6); }
static TWindow *FakeCreateBattleWindow() { return NewSubWindow(7); }
static TWindow *FakeCreateTradeWindow() { return NewSubWindow(8); }
static TWindow *FakeCreateBuddylistWindow() { return NewSubWindow(9); }
// The side-bar windows the main window makes itself (TAutomapWindow,
// TStatusWindow, TButtonsWindow): content windows of the same sizes; the
// image's constructors and ours (through their decorated names) build them.
class TMainSide3 : public TContentWindow {
public:
	TMainSide3() : TContentWindow(3, false)
	{
		Register(this, "Side3", 3);
		Logf("N%s;", Name(this).c_str());
		memset(Data44, 0, sizeof(Data44));
	}
	uint8 Data44[sizeof(TAutomapWindow) - sizeof(TContentWindow)]; // +0x44 on x86
};
class TMainSide4 : public TContentWindow {
public:
	TMainSide4() : TContentWindow(4, false)
	{
		Register(this, "Side4", 4);
		Logf("N%s;", Name(this).c_str());
		memset(Data44, 0, sizeof(Data44));
	}
	uint8 Data44[sizeof(TStatusWindow) - sizeof(TContentWindow)]; // +0x44 on x86
};
class TMainSide5 : public TContentWindow {
public:
	TMainSide5(TDialogWindow *_CheckObject, TMainWindow *Receiver) : TContentWindow(5, false)
	{
		Register(this, "Side5", 5);
		Logf("N%s,%s,%s;", Name(this).c_str(), Name(_CheckObject).c_str(), Name(Receiver).c_str());
		memset(Data44, 0, sizeof(Data44));
	}
	uint8 Data44[sizeof(TButtonsWindow) - sizeof(TContentWindow)]; // +0x44 on x86
};
static_assert(sizeof(TMainSide3) == sizeof(TAutomapWindow) && sizeof(TMainSide4) == sizeof(TStatusWindow)
		&& sizeof(TMainSide5) == sizeof(TButtonsWindow),
	"the side-bar fakes have the real sizes");

} // namespace MainWindowTest
using namespace MainWindowTest;

#if defined(_MSC_VER)
extern "C" void OurAutomapConstructor();
extern "C" void OurStatusConstructor();
extern "C" void OurButtonsConstructor();
#endif
#if GOLDEN_ORACLE
#pragma comment(linker, "/alternatename:_OurAutomapConstructor=??0TAutomapWindow@@QAE@XZ")
#pragma comment(linker, "/alternatename:_OurStatusConstructor=??0TStatusWindow@@QAE@XZ")
#pragma comment(linker, "/alternatename:_OurButtonsConstructor=??0TButtonsWindow@@QAE@PAVTDialogWindow@@PAVTMainWindow@@@Z")
#elif defined(_MSC_VER)
#pragma comment(linker, "/alternatename:OurAutomapConstructor=??0TAutomapWindow@@QEAA@XZ")
#pragma comment(linker, "/alternatename:OurStatusConstructor=??0TStatusWindow@@QEAA@XZ")
#pragma comment(linker, "/alternatename:OurButtonsConstructor=??0TButtonsWindow@@QEAA@PEAVTDialogWindow@@PEAVTMainWindow@@@Z")
#else
GOLDEN_SYMBOL(OurAutomapConstructor, "_ZN14TAutomapWindowC1Ev");
GOLDEN_SYMBOL(OurStatusConstructor, "_ZN13TStatusWindowC1Ev");
GOLDEN_SYMBOL(OurButtonsConstructor, "_ZN14TButtonsWindowC1EP13TDialogWindowP11TMainWindow");
#endif

// elementfakes.h's fakes this test introduced (the border, the first text button constructor, the label's
// font constructor), on both sides.

TFakeBorder::TFakeBorder(TWindow *_Window, int Param)
{
	TInfo &I = Register(this, "Border");
	char Buffer[128];
	snprintf(Buffer, sizeof(Buffer), "%s,%d", Name(_Window).c_str(), Param);
	I.Detail = Buffer;
	Logf("NBorder%s;", Buffer);
	InsertChild(_Window);
	if (FakeBorderVtable != NULL)
		SetVtable(this, FakeBorderVtable);
}
TFakeBorder::TFakeBorder(TWindow *_Window, int Skin1, int Skin2, int Skin3, int Skin4, int Skin5, int Skin6, int Skin7,
	int Skin8, int Skin9, int Border)
{
	TInfo &I = Register(this, "Border");
	char Buffer[160];
	snprintf(Buffer, sizeof(Buffer), "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", Name(_Window).c_str(), Skin1, Skin2, Skin3, Skin4, Skin5,
		Skin6, Skin7, Skin8, Skin9, Border);
	I.Detail = Buffer;
	Logf("NBorder%s;", Buffer);
	InsertChild(_Window);
	if (FakeBorderVtable != NULL)
		SetVtable(this, FakeBorderVtable);
}
void TFakeBorder::FitToWindow() { Logf("Fit%s;", Name(this).c_str()); }

TFakeTextButton::TFakeTextButton(int x, int y, const char *_Text, int Skin1, int Skin2, TCallback Callback,
	const char *Tooltip)
{
	int Param = (int)(intptr_t)Tooltip;
	ReportElement(this, "TextButton", "%d,%d,%s,%d,%d,@%p/%d,%d", x, y, _Text, Skin1, Skin2, (void *)Callback.Receiver,
		Callback.Event, Param);
	if (!Running)
		return;
	TInfo &I = Register(this, "Button");
	char Buffer[160];
	snprintf(Buffer, sizeof(Buffer), "%d,%d,%s,%d,%d,%s,%d,%d", x, y, _Text, Skin1, Skin2, Name(Callback.Receiver).c_str(),
		Callback.Event, Param);
	I.Detail = Buffer;
	Logf("NButton%s;", Buffer);
	SetSize(80, 20);
	if (FakeButtonVtable != NULL)
		SetVtable(this, FakeButtonVtable);
}
TFakeLabel::TFakeLabel(TFont Font, const char *_Text)
{
	ReportElement(this, "Label", "%d,%d,%d,%d,%d,%s", Font.FontNumber, Font.Color.R, Font.Color.G, Font.Color.B,
		Font.Alignment, _Text);
	if (!Running)
		return;
	TInfo &I = Register(this, "Label");
	char Buffer[160];
	snprintf(Buffer, sizeof(Buffer), "%d,%d,%d,%d,%d,%s", Font.FontNumber, Font.Color.R, Font.Color.G, Font.Color.B, Font.Alignment,
		_Text);
	I.Detail = Buffer;
	Logf("NLabel%s;", Buffer);
}

namespace MainWindowTest {

// TObjectIcon's constructor is inline in DialogElements.h: the icons are real
// on both sides; DrawObject is faked.
static void __cdecl FakeDrawObject(int Surface, int x, int y, int Size, TObject Obj, TRGBColor Highlight, TRect Clip,
	TFont /* Font */, bool ShowCount)
{
	Logf("DObj%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Size, Obj.Type, Obj.Data, Obj.Data2,
		Highlight.R, Highlight.G, Highlight.B, Clip.x, Clip.y, Clip.width, Clip.height, ShowCount ? 1 : 0);
}

// Both sides' DialogElements.cpp constructors and members build and are the
// fakes (elementfakes.cpp), patched for good.
static void PatchElement(void *Function, void *Target) { PatchOwn(Function, Target); }
// The side-bar constructors' stand-ins (the image's and ours; GOLDEN_THISCALL).
static void *GOLDEN_THISCALL ImageSide3(GOLDEN_THIS_NAMED(void *This)) { return new (This) TMainSide3; }
static void *GOLDEN_THISCALL ImageSide4(GOLDEN_THIS_NAMED(void *This)) { return new (This) TMainSide4; }
static void *GOLDEN_THISCALL ImageSide5(GOLDEN_THIS_NAMED(void *This), TDialogWindow *D, TMainWindow *M)
{
	return new (This) TMainSide5(D, M);
}

// --- Fakes of the other callees -------------------------------------------------

// Not part of the test build (Communication.cpp's, Control.cpp's): in the oracle our side's definitions are
// these fakes (below; the image's jump to them), in the runner client_stubs.cpp's stubs are redirected to
// them.
static void FakeSendTurnNorth() { Logf("TN;"); }
static void FakeSendTurnEast() { Logf("TE;"); }
static void FakeSendTurnSouth() { Logf("TS;"); }
static void FakeSendTurnWest() { Logf("TW;"); }
static void FakeSendCancel() { Logf("Cancel;"); }
static void FakeSendLogout() { Logf("Logout;"); }
static void FakeSendJoinChannel(int ChannelID) { Logf("Join%d;", ChannelID); }
static void FakeChangeEngine(int engine) { Logf("Engine%d;", engine); }

} // namespace MainWindowTest
using namespace MainWindowTest;

#if GOLDEN_ORACLE
void SendTurnNorth() { FakeSendTurnNorth(); }
void SendTurnEast() { FakeSendTurnEast(); }
void SendTurnSouth() { FakeSendTurnSouth(); }
void SendTurnWest() { FakeSendTurnWest(); }
void SendCancel() { FakeSendCancel(); }
void SendLogout() { FakeSendLogout(); }
void SendJoinChannel(int ChannelID) { FakeSendJoinChannel(ChannelID); }
void ChangeEngine(int engine) { FakeChangeEngine(engine); }
#endif

namespace MainWindowTest {

static void __cdecl FakeSendWalk() { Logf("Walk;"); }
static void __cdecl FakeSendStop() { Logf("Stop;"); }
static void __cdecl FakeDisconnect(bool force) { Logf("Disc%d;", force ? 1 : 0); }

static int ConnectionState[2], ActionState[2], ConsoleHeightPref, ViewMode;
static bool Right, AnyRight;
static TFullscreenMode Fullscreen[2];
static int PlayerX = 1000, PlayerY = 1000, PlayerZ = 7, DestinationResult;

// ~TMainWindow calls ClearOpenDialogs and AddOpenDialog, ~TSubWindow
// SetContentWindowSize: they do not count for FailAt.
static void __cdecl FakeClearOpenDialogs()
{
	TNoTick NoTick;
	Logf("ClrDlg;");
	OpenDialogs[Side].clear();
}
static void __cdecl FakeAddOpenDialog(int Identifier)
{
	TNoTick NoTick;
	Logf("AddDlg%d;", Identifier);
	OpenDialogs[Side].push_back(Identifier);
}
static void __cdecl FakeSetContentWindowSize(int ID, int Size)
{
	TNoTick NoTick;
	Logf("CWSize%d,%d;", ID, Size);
}
static int __cdecl FakeGetOpenDialogs(int **List)
{
	int Count = (int)OpenDialogs[Side].size();
	Logf("GetDlg%d;", Count);
	*List = Count > 0 ? (int *)malloc(Count * sizeof(int)) : NULL;
	for (int i = 0; i < Count; i++)
		(*List)[i] = OpenDialogs[Side][i];
	return Count;
}
static int __cdecl FakeGetConsoleHeight()
{
	Logf("GetCH;");
	return ConsoleHeightPref;
}
static TFullscreenMode *__cdecl FakeGetFullscreenMode()
{
	Logf("GetFS;");
	return &Fullscreen[Side];
}
static void __cdecl FakeSetFullscreenMode(const TFullscreenMode *Mode)
{
	Logf("SetFS%d,%d,%d,%d;", Mode->Width, Mode->Height, Mode->Depth, Mode->Frequency);
	Fullscreen[Side] = *Mode;
}
static int __cdecl FakeGetViewMode() { return ViewMode; }
static void __cdecl FakeSetActionState(int State)
{
	Logf("Act%d;", State);
	ActionState[Side] = State;
}
static int __cdecl FakeGetActionState() { return ActionState[Side]; }
static int __cdecl FakeGetConnectionState() { return ConnectionState[Side]; }
static void __cdecl FakeSetConnectionState(int State)
{
	Logf("Conn%d;", State);
	ConnectionState[Side] = State;
}
static void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	if (Type == 6) {
		INT_PTR *Data = (INT_PTR *)Param1;
		Logf("Msg6,%s,%s,%s,%d,%d;", (const char *)Data[0], (const char *)Data[1], Name((void *)Data[2]).c_str(),
			(int)Data[3], (int)Param2);
		delete[] Data;
		return;
	}
	Logf("Msg%d,%d,%d;", Type, (int)Param1, (int)Param2);
}
static bool __cdecl FakeCheckRight(int rightid, int Bit)
{
	Logf("Right%d,%d;", rightid, Bit);
	return Right;
}
static bool __cdecl FakeCheckAnyRight()
{
	Logf("AnyRight;");
	return AnyRight;
}
static void __cdecl FakeSetMouseCursor(int Cursor) { Logf("Cursor%d;", Cursor); }
static void __cdecl FakeSetWindowSize(int Width, int Height) { Logf("WinSize%d,%d;", Width, Height); }
static void __cdecl FakeCopyTainted() { Logf("Clip;"); }
static void __cdecl FakeSetStatusMessage(const char *Message, bool Permanent)
{
	Logf("Status%s,%d;", Message, Permanent ? 1 : 0);
}
static void __cdecl FakeSetTalkChanged() { Logf("TC;"); }
static bool __cdecl FakeGetNoStretching() { return false; }
static void __cdecl FakeGetPlayerPosition(int *x, int *y, int *z)
{
	*x = PlayerX;
	*y = PlayerY;
	*z = PlayerZ;
}
static int __cdecl FakeSetPlayerDestination(int x, int y, int z, bool Exact, bool Diagonal)
{
	Logf("Dest%d,%d,%d,%d,%d;", x, y, z, Exact ? 1 : 0, Diagonal ? 1 : 0);
	return DestinationResult;
}
static void __cdecl FakeClearPlayerSteps() { Logf("ClrSteps;"); }
static void __cdecl FakeSetAttackID(uint32 CreatureID) { Logf("Attack%u;", CreatureID); }
static char ContainerNames[16][16];
static char *__cdecl FakeGetContainerName(int Container) { return ContainerNames[Container & 15]; }
static TObject __cdecl FakeGetContainerObject(int Container)
{
	TObject Obj;
	Obj.Type = 100 + Container;
	Obj.Data = Container * 3;
	Obj.Data2 = 0;
	return Obj;
}
static bool __cdecl FakeGetContainerHasParent(int Container) { return Container % 3 == 1; }
static void __cdecl FakeAddHotkey(int Key, TCallback Callback)
{
	Logf("AddKey%d,%s,%d;", Key, Name(Callback.Receiver).c_str(), Callback.Event);
}
static void __cdecl FakeRemoveHotkey(int Key)
{
	TNoTick NoTick;
	Logf("RemKey%d;", Key);
}
static void __cdecl FakeDrawPicture(int Surface, int x, int y, int PictureNumber)
{
	Logf("Pic%d,%d,%d,%d;", Surface, x, y, PictureNumber);
}
static void __cdecl FakeDrawSkin(int Surface, int x, int y, int width, int height, int skin, int srcx, int srcy)
{
	Logf("Skin%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, width, height, skin, srcx, srcy);
}
static void __cdecl FakeDrawPicturePart(int Surface, int x, int y, int PictureNumber, int srcx, int srcy, int Width,
	int Height)
{
	Logf("Part%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, PictureNumber, srcx, srcy, Width, Height);
}

// The surface: StretchBlitRect and FillRectangle (a dragged content window's
// place) are logged, the rest never called.
class TMainSurface : public TTestSurface {
public:
	void FillRectangle(int Surface, TRect R, TRGBColor C)
	{
		Logf("Fill%d,%d,%d,%d,%d,%d,%d,%d;", Surface, R.x, R.y, R.width, R.height, C.R, C.G, C.B);
	}
	void StretchBlitRect(int ToSurface, TRect D, int FromSurface, TRect S)
	{
		Logf("Stretch%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", ToSurface, D.x, D.y, D.width, D.height, FromSurface, S.x, S.y,
			S.width, S.height);
	}
};
static TMainSurface *Surface;
static TSurface *__cdecl FakeGetSurface() { return Surface; }

// The frame callbacks of InitMainWindow.
static void FakeExitProgram() { Logf("Exit;"); }
static void FakeSetDisplayMode(int Mode) { Logf("Display%d;", Mode); }
static void FakeOpenURL(const char *URL) { Logf("URL%s;", URL); }

static void PatchCallees()
{
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		{0x4db550, (void *)ClearOpenDialogs, (void *)FakeClearOpenDialogs},
		{0x4dbd90, (void *)GetOpenDialogs, (void *)FakeGetOpenDialogs},
		{0x4dc950, (void *)AddOpenDialog, (void *)FakeAddOpenDialog},
		{0x4dbcd0, (void *)SetContentWindowSize, (void *)FakeSetContentWindowSize},
		{0x4db4c0, (void *)(int (*)())GetConsoleHeight, (void *)FakeGetConsoleHeight},
		{0x4db3b0, (void *)GetFullscreenMode, (void *)FakeGetFullscreenMode},
		{0x4db2d0, (void *)SetFullscreenMode, (void *)FakeSetFullscreenMode},
		{0x4db3a0, (void *)GetViewMode, (void *)FakeGetViewMode},
		{0x4db400, (void *)GetNoStretching, (void *)FakeGetNoStretching},
		{0x4e6e20, (void *)SetActionState, (void *)FakeSetActionState},
		{0x4e6e30, (void *)GetActionState, (void *)FakeGetActionState},
		{0x4e71f0, (void *)GetConnectionState, (void *)FakeGetConnectionState},
		{0x4e71e0, (void *)SetConnectionState, (void *)FakeSetConnectionState},
		{0x4e9020, (void *)AddMessage, (void *)FakeAddMessage},
		{0x4e86e0, (void *)CheckRight, (void *)FakeCheckRight},
		{0x4e6dc0, (void *)CheckAnyRight, (void *)FakeCheckAnyRight},
		{0x4e6e80, (void *)SetMouseCursor, (void *)FakeSetMouseCursor},
		{0x4e6f00, (void *)SetWindowSize, (void *)FakeSetWindowSize},
		{0x4ea710, (void *)CopyTaintedClipboardText, (void *)FakeCopyTainted},
		{0x4ff500, (void *)SetStatusMessage, (void *)FakeSetStatusMessage},
		{0x4fd4c0, (void *)SetTalkChanged, (void *)FakeSetTalkChanged},
		{0x44b880, (void *)GetPlayerPosition, (void *)FakeGetPlayerPosition},
		{0x451400, (void *)SetPlayerDestination, (void *)FakeSetPlayerDestination},
		{0x44b8c0, (void *)ClearPlayerSteps, (void *)FakeClearPlayerSteps},
		{0x44bb00, (void *)SetAttackID, (void *)FakeSetAttackID},
		{0x4a64e0, (void *)GetContainerName, (void *)FakeGetContainerName},
		{0x4a5e80, (void *)GetContainerObject, (void *)FakeGetContainerObject},
		{0x4a6a40, (void *)GetContainerHasParent, (void *)FakeGetContainerHasParent},
		{0x4a10a0, (void *)AddHotkey, (void *)FakeAddHotkey},
		{0x4a12a0, (void *)RemoveHotkey, (void *)FakeRemoveHotkey},
		{0x490920, (void *)DrawPicture, (void *)FakeDrawPicture},
		{0x490c60, (void *)DrawPicturePart, (void *)FakeDrawPicturePart},
		{0x497a00, (void *)DrawSkin, (void *)FakeDrawSkin},
		{0x4eaa90, (void *)GetSurface, (void *)FakeGetSurface},
		{0x414700, (void *)SendWalk, (void *)FakeSendWalk},
		{0x414a10, (void *)SendStop, (void *)FakeSendStop},
		{0x4ce300, (void *)Disconnect, (void *)FakeDisconnect},
		{0x494ca0, (void *)DrawObject, (void *)FakeDrawObject},
	};
	for (auto &T : Table) {
		ORIGINAL(PatchJump(T.Image, T.Fake));
		PatchOwn(T.Ours, T.Fake);
	}
	// The functions difftest.exe does not link: the image's jump to our definitions above (the oracle); ours,
	// client_stubs.cpp's in the runner, to the fakes.
	static const struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Undefined[] = {
		{0x404730, (void *)SendTurnNorth, (void *)FakeSendTurnNorth},
		{0x4048d0, (void *)SendTurnEast, (void *)FakeSendTurnEast},
		{0x404a70, (void *)SendTurnSouth, (void *)FakeSendTurnSouth},
		{0x404c10, (void *)SendTurnWest, (void *)FakeSendTurnWest},
		{0x409140, (void *)SendCancel, (void *)FakeSendCancel},
		{0x404350, (void *)SendLogout, (void *)FakeSendLogout},
		{0x4074a0, (void *)SendJoinChannel, (void *)FakeSendJoinChannel},
		{0x445c20, (void *)ChangeEngine, (void *)FakeChangeEngine},
	};
	for (auto &U : Undefined) {
#if GOLDEN_ORACLE
		PatchJump(U.Image, U.Ours);
		(void)U.Fake;
#else
		PatchOwn(U.Ours, U.Fake);
#endif
	}
	// Image functions whose fakes are the stand-ins above (ours: the constructors and members below).
#if GOLDEN_ORACLE
	static const struct {
		uint32 Image;
		void *Fake;
	} Images[] = {
		{0x422410, (void *)FakeConsoleNew},
		{0x41e960, (void *)FakeConsoleActive},
		{0x422e70, (void *)FakeConsoleOpen},
		{0x42e4e0, (void *)FakeUpdateUpButton},
		{0x4368c0, (void *)ImageSide3},
		{0x438310, (void *)ImageSide4},
		{0x426480, (void *)ImageSide5},
	};
	for (auto &I : Images)
		PatchJump(I.Image, I.Fake);
#endif
	// ContentWindows.cpp: the creators and the side-bar constructors, the image's and ours.
	static const struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Content[] = {
		{0x4362b0, (void *)CreateInventoryWindow, (void *)FakeCreateInventoryWindow},
		{0x436510, (void *)CreateSmallInventoryWindow, (void *)FakeCreateSmallInventoryWindow},
		{0x43c520, (void *)CreateContainerWindow, (void *)FakeCreateContainerWindow},
		{0x43c920, (void *)CreateSkillsWindow, (void *)FakeCreateSkillsWindow},
		{0x43cc90, (void *)CreateBattleWindow, (void *)FakeCreateBattleWindow},
		{0x43d000, (void *)CreateTradeWindow, (void *)FakeCreateTradeWindow},
		{0x43d390, (void *)CreateBuddylistWindow, (void *)FakeCreateBuddylistWindow},
		{0, (void *)OurAutomapConstructor, (void *)ImageSide3},
		{0, (void *)OurStatusConstructor, (void *)ImageSide4},
		{0, (void *)OurButtonsConstructor, (void *)ImageSide5},
		{0, Member(&TContainerWindowBody::UpdateUpButton), (void *)FakeUpdateUpButton},
	};
	for (auto &C : Content) {
		if (C.Image != 0)
			ORIGINAL(PatchJump(C.Image, C.Fake));
		PatchOwn(C.Ours, C.Fake);
	}
	PatchElementFakes(PatchElement);
	PatchOwn((void *)OurConsoleConstructor, (void *)FakeConsoleNew);
	PatchOwn(Member(&TConsoleWindow::SetActive), (void *)FakeConsoleActive);
	PatchOwn(Member(&TConsoleWindow::OpenDefaultChannel), (void *)FakeConsoleOpen);
}

// The vtables of the fakes, from instances made quietly.
static void FindFakeVtables()
{
	Quiet = true;
	TFakeConsole *Console = new TFakeConsole;
	FakeConsoleVtable = VtableOf(Console);
	TMWTestBorder *Border = new TMWTestBorder(new TWindow);
	FakeBorderVtable = VtableOf(Border);
	TMWTestButton *Button = new TMWTestButton;
	FakeButtonVtable = VtableOf(Button);
	TFakeMapFrame *Frame = new TFakeMapFrame;
	FakeMapFrameVtable = VtableOf(Frame);
	Quiet = false;
	delete Console;
	delete Border;
	delete Button;
	delete Frame;
}

// --- The two main windows ------------------------------------------------------

#if GOLDEN_ORACLE
typedef void(__fastcall *MemberVoid)(void *, void *);
typedef void(__fastcall *MemberInt)(void *, void *, int);
typedef int(__fastcall *MemberGet)(void *, void *);
typedef void(__fastcall *MemberWalk)(void *, void *, int, int, int);
typedef void(__cdecl *InitFn)(int, int, void (*)(), void (*)(int), void (*)(const char *));
#endif

static void NameOurVtables()
{
	TMainWindow *M = MainWindow;
	NameVtable(OURS, VtableOf(M), "Main", true);
	if (M->StartWindow != NULL)
		NameVtable(OURS, VtableOf(M->StartWindow), "Start", true);
	if (M->Splitter != NULL)
		NameVtable(OURS, VtableOf(M->Splitter), "Splitter", false);
}

// After a mode change the map frames get the logging vtable.
static void SwapFrames()
{
	for (int s = 0; s < SIDES; s++) {
		TMainWindow *M = Main(s);

		if (M != NULL && M->InGame && M->MapFrame != NULL && VtableOf(M->MapFrame) != FakeMapFrameVtable) {
			// The map view goes (its destructor removes its hotkeys on both
			// sides): no mouse event or drawing reaches the map.
			int Saved = Side;
			Side = s;
			TWindow *Frame = (TWindow *)M->MapFrame;
			if (Frame->FirstChild != NULL)
				Frame->DeleteChild(Frame->FirstChild);
			SetVtable(M->MapFrame, FakeMapFrameVtable);
			Side = Saved;
		}
	}
}

static void DumpComponent(int s, std::string &Out, TComponent *C, int Depth)
{
	char Buffer[256];
	std::string N = NameOf(s, C);
	sprintf(Buffer, "%*s%s %d,%d %dx%d", Depth * 2, "", N.c_str(), C->OffsetX, C->OffsetY, C->Width, C->Height);
	Out += Buffer;
	// An entry of a freed object at the same address is not this one's.
	auto I = Info[s].find(C);
	if (I != Info[s].end() && !I->second.Detail.empty() && N.compare(0, I->second.Kind.size(), I->second.Kind) == 0)
		Out += " [" + I->second.Detail + "]";
	if (C->DrawingListLength > 0 && C->DrawingList != NULL) {
		sprintf(Buffer, " DL%d", C->DrawingListLength);
		Out += Buffer;
		for (int i = 0; i < C->DrawingListLength; i++) {
			TDrawing &D = C->DrawingList[i];
			sprintf(Buffer, " %d/%d/%d/%d/%d/%d/%d", D.skin, D.x, D.y, D.width, D.height, D.srcx, D.srcy);
			Out += Buffer;
		}
	}
	if (N == "Splitter") {
		TSplitter *S = (TSplitter *)C;
		sprintf(Buffer, " drag %d %d,%d", S->Dragging ? 1 : 0, S->DragX, S->DragY);
		Out += Buffer;
	}
	if (N.compare(0, 5, "Start") == 0)
		Out += " frame " + NameOf(s, ((TStartWindow *)C)->Frame);
	std::string Kind0 = N.substr(0, N.find('#'));
	if (Kind0 == "CW" || Kind0 == "Sub" || Kind0.compare(0, 4, "Side") == 0) {
		TContentWindow *W = (TContentWindow *)C;
		sprintf(Buffer, " id %d bg %d,%d drag %d %d,%d,%d", W->Identifier, W->Background ? 1 : 0, W->BackgroundY,
			W->Dragging ? 1 : 0, W->DragX, W->DragY, W->DragBottom);
		Out += Buffer;
	}
	if (Kind0 == "Sub") {
		TSubWindow *W = (TSubWindow *)C;
		sprintf(Buffer, " min %d %d body ", W->IsMinimized ? 1 : 0, W->RestoreHeight);
		Out += Buffer + NameOf(s, W->Body);
	}
	if (Kind0 == "Dialog") {
		TDialogWindow *D = (TDialogWindow *)C;
		sprintf(Buffer, " area %d,%d,%d,%d busy %d free %d,%d dragged ", D->Area.x, D->Area.y, D->Area.width,
			D->Area.height, D->Busy ? 1 : 0, D->FreeHeight, D->UpdateFreeHeight ? 1 : 0);
		Out += Buffer + NameOf(s, D->Dragged);
	}
	Out += " parent " + NameOf(s, C->Parent) + "\n";
	std::string Base = N.substr(0, N.find('#'));
	auto W = IsWindowKind.find(Base);
	bool Window = W != IsWindowKind.end() ? W->second : false;
	if (Window) {
		TWindow *Win = (TWindow *)C;
		if (Win->Capture != NULL)
			Out += std::string(Depth * 2 + 2, ' ') + "capture " + NameOf(s, Win->Capture) + "\n";
		for (TComponent *Child = Win->FirstChild; Child != NULL; Child = Child->Next)
			DumpComponent(s, Out, Child, Depth + 1);
	}
}

static std::string Dump(int s)
{
	std::string Out;
	TMainWindow *M = Main(s);
	if (M == NULL)
		return "no main window\n";
	char Buffer[128];
	sprintf(Buffer, "InGame %d\n", M->InGame ? 1 : 0);
	Out += Buffer;
	if (M->InGame) {
		Out += "MapFrame " + NameOf(s, M->MapFrame) + " SideBar " + NameOf(s, M->SideBar) + " Dialog "
			+ NameOf(s, M->DialogWindow) + " Side " + NameOf(s, M->SidePanel) + " Console " + NameOf(s, M->Console)
			+ " Splitter " + NameOf(s, M->Splitter) + " Start " + NameOf(s, M->StartWindow) + "\n";
	} else {
		Out += "MapFrame " + NameOf(s, M->MapFrame) + " Dialog " + NameOf(s, M->DialogWindow) + " Side "
			+ NameOf(s, M->SidePanel) + " Console " + NameOf(s, M->Console) + " Splitter " + NameOf(s, M->Splitter)
			+ " Start " + NameOf(s, M->StartWindow) + "\n";
	}
	DumpComponent(s, Out, M, 0);
	return Out;
}

static int Kind;
// The error round: a call that threw may leave members of the main window
// pointing to deleted windows; the trees are not dumped then, and the round
// goes on with new main windows.
static TErrorRound *MainRound;
static bool Threw;

// The original's log, dump and the state its fakes keep: the oracle's, recorded; the runner's from the
// corpus.
static std::string LastDump;
static void OutputOriginal(std::string *Dumped)
{
	std::string Text = golden::OutputText("log", Log[ORIGINAL].data(), Log[ORIGINAL].size());
	ORIGINAL_OR((void)0, Log[ORIGINAL] = Text);
	if (Dumped != NULL) {
#if GOLDEN_ORACLE
		std::string Dump0 = Dump(ORIGINAL);
#else
		std::string Dump0;
#endif
		*Dumped = golden::OutputTextChanges("dump", &LastDump, Dump0);
	}
	golden::Output("action state", ActionState[ORIGINAL]);
	golden::Output("connection state", ConnectionState[ORIGINAL]);
	uint32 Dialogs = (uint32)OpenDialogs[ORIGINAL].size();
	golden::Output("open dialogs", Dialogs);
	OpenDialogs[ORIGINAL].resize(Dialogs);
	if (Dialogs != 0)
		golden::OutputArray("open dialogs", OpenDialogs[ORIGINAL].data(), Dialogs);
	int Mode[4] = {Fullscreen[ORIGINAL].Width, Fullscreen[ORIGINAL].Height, Fullscreen[ORIGINAL].Depth,
		Fullscreen[ORIGINAL].Frequency};
	golden::Output("fullscreen", Mode);
	Fullscreen[ORIGINAL].Width = Mode[0];
	Fullscreen[ORIGINAL].Height = Mode[1];
	Fullscreen[ORIGINAL].Depth = Mode[2];
	Fullscreen[ORIGINAL].Frequency = Mode[3];
}

static void Compare(const char *What)
{
	if (!Threw)
		SwapFrames();
	std::string B;
	OutputOriginal(Threw ? NULL : &B);
	Checks++;
	if (Log[OURS] != Log[ORIGINAL]) {
		Failures++;
		if (Failures <= FailuresPrinted) {
			printf("FAIL MainWindow %s (kind %d): logs differ\n  ours:     %.600s\n  original: %.600s\n", What, Kind,
				Log[OURS].c_str(), Log[ORIGINAL].c_str());
		}
	}
	std::string A = Threw ? "" : Dump(OURS);
	Checks++;
	if (A != B) {
		Failures++;
		if (Failures <= FailuresPrinted)
			printf("FAIL MainWindow %s (kind %d): windows differ\n--- ours\n%s--- original\n%s", What, Kind,
				A.c_str(), B.c_str());
	}
	CHECK(ActionState[OURS] == ActionState[ORIGINAL] && ConnectionState[OURS] == ConnectionState[ORIGINAL]
			&& OpenDialogs[OURS] == OpenDialogs[ORIGINAL]
			&& memcmp(&Fullscreen[OURS], &Fullscreen[ORIGINAL], sizeof(TFullscreenMode)) == 0,
		"MainWindow %s (kind %d): state differs", What, Kind);
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

// The most fake calls one call of each operation made (Kind 24: InitMainWindow,
// 25 and 26: the start window's Draw and SetSize); the error round's calls
// fail only in the operation of the round (TargetKind).
static int MaxTicks[27], TargetKind = -1;

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

// Runs one call on both sides: F(s) with Side set. In the error round the
// call fails at one of the fake calls the operation made before, and what
// both sides threw is compared.
template <class F> static void Both(F Call)
{
	if (MainRound == NULL) {
		Side = OURS;
		Ticks = 0;
		Call(OURS);
		// (Side is the original's afterwards on both sides: what our code logs between the calls goes where
		// the oracle's goes.)
		Side = ORIGINAL;
		ORIGINAL(Ticks = 0; Call(ORIGINAL));
		Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls
		MaxTicks[Kind] = Ticks > MaxTicks[Kind] ? Ticks : MaxTicks[Kind];
		return;
	}
	int Fail = MaxTicks[Kind] > 0 && (TargetKind < 0 || Kind == TargetKind) ? FailIndex(MaxTicks[Kind] + 1) : 0;
	char What[64];
	sprintf(What, "operation %d (fail at %d)", Kind, Fail);
	MainRound->Calling(What, "ours");
	TThrown A = OurThrow([&] { Side = OURS; Ticks = 0; FailAt = Fail; Call(OURS); });
	MainRound->Calling(What, "the original");
	TThrown B = ORIGINAL_THROWN("thrown", Side = ORIGINAL; Ticks = 0; FailAt = Fail; Call(ORIGINAL));
	Side = ORIGINAL;
	Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls
	FailAt = 0;
	MainRound->Compare(What, A, B);
	if (A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING)
		Threw = true;
	else
		MaxTicks[Kind] = Ticks > MaxTicks[Kind] ? Ticks : MaxTicks[Kind];
}

static TContentWindow *Find(int s, TDialogWindow *Dialog, int Identifier)
{
	if (s == OURS)
		return Dialog->GetContentWindow(Identifier);
#if GOLDEN_ORACLE
	return At<FindFn>(0x486400)(Dialog, NULL, Identifier);
#else
	abort(); // (the original's side: the oracle only)
#endif
}

static void RandomSkinProperties()
{
	int Width = 150 + Random() % 60, Splitter = Skins[9].height + Random() % 6, Border = Random() % 12;
	SkinProperties.Data184 = Width;
	SkinProperties.Data138 = Splitter;
	SkinProperties.Data134 = Border;
	SkinProperties.Style0D8.Data00 = 60 + Random() % 60;
	SkinProperties.Style0D8.Data04 = 100 + Random() % 80;
	int x = (int)(Random() % 120) - 60, y = (int)(Random() % 400) - 300;
	SkinProperties.Style0D8.Data08.x = x;
	SkinProperties.Style0D8.Data08.y = y;
	TGUIPoint *Ours = &SkinProperties.Style0D8.Data10;
	for (int i = 0; i < 5; i++) {
		Ours[i].x = Random() % 20;
		Ours[i].y = Random() % 140;
	}
	SkinProperties.Style194.Data38.FontNumber = Random() % 5;
	// TRGBColor(Random() % 256, Random() % 256, 7) and TRGBColor(Random() % 256, 3, Random() % 256) with their
	// calls in the x86 oracle's order (issue #55: the constructor's arguments right to left).
	int G = (int)(Random() % 256);
	int R = (int)(Random() % 256);
	SkinProperties.Style194.Data38.Color = TRGBColor(R, G, 7);
	int B = (int)(Random() % 256);
	R = (int)(Random() % 256);
	SkinProperties.Style194.Data80 = TRGBColor(R, 3, B);
#if GOLDEN_ORACLE
	// The image's skin properties get the same.
	TSkinProperties &O = Global<TSkinProperties>(0x5ccfc8);
	O.Data184 = SkinProperties.Data184;
	O.Data138 = SkinProperties.Data138;
	O.Data134 = SkinProperties.Data134;
	O.Style0D8.Data00 = SkinProperties.Style0D8.Data00;
	O.Style0D8.Data04 = SkinProperties.Style0D8.Data04;
	O.Style0D8.Data08.x = SkinProperties.Style0D8.Data08.x;
	O.Style0D8.Data08.y = SkinProperties.Style0D8.Data08.y;
	TGUIPoint *Theirs = &O.Style0D8.Data10;
	for (int i = 0; i < 5; i++) {
		Theirs[i].x = Ours[i].x;
		Theirs[i].y = Ours[i].y;
	}
	O.Style194.Data38.FontNumber = SkinProperties.Style194.Data38.FontNumber;
	O.Style194.Data38.Color = SkinProperties.Style194.Data38.Color;
	O.Style194.Data80 = SkinProperties.Style194.Data80;
#endif
}

static void RandomWorld()
{
	ConsoleMinHeight = 30 + Random() % 60;
	BodyMinHeight = 1 + Random() % 40;
	BorderMinHeight = Random() % 30;
	FrameMinHeight = 150 + Random() % 150;
	ConsoleHeightPref = Random() % 3 == 0 ? (int)(Random() % 40) : 60 + (int)(Random() % 400);
	ViewMode = Random() % 3;
	Right = Random() % 2 != 0;
	AnyRight = Random() % 2 != 0;
	BodyAccepts = Random() % 3 != 0;
	DestinationResult = Random() % 4;
	PlayerX = 900 + Random() % 200;
	PlayerY = 900 + Random() % 200;
	PlayerZ = Random() % 16;
	for (int i = 0; i < 16; i++)
		RandomText(ContainerNames[i], 1 + Random() % 14);
}

static void RandomOpenDialogs()
{
	std::vector<int> List;
	int Count = Random() % 8;
	for (int i = 0; i < Count; i++)
		List.push_back(1 + Random() % 9);
	OpenDialogs[OURS] = OpenDialogs[ORIGINAL] = List;
}

static void RandomSize(int &Width, int &Height)
{
	Width = 480 + Random() % 800;
	Height = 300 + Random() % 700;
}

static const int Keys[] = {0x10c, 0x10d, 0x10e, 0x10f, 0x110, 0x115, 0x116, 0x117, 0x118, 0x119, 0x50c, 0x50d,
	0x50e, 0x50f, 0x510, 0x50b, 0x41, 0x1b, 0x10b, 0x11a};
static const int AnyEvents[] = {2, 7, 8, 9, 0x1d, 0x20, 0x21, 0x1f8, 0x1f9, 0x1fa, 0x1fb, 0x1fc, 0x1fd, 0x2bd, 0x2be,
	0x2bf, 0x2c0, 0x2c1, 0x2c2, 0x2c4, 799, 800, 801, 802, 803, 1, 3, 0x1f7, 0x1f5, 600};
static const int GameEvents[] = {0xe, 0xf, 0x16};
static const int StartEvents[] = {0x1f5, 0x1f6, 0x1f7, 7, 0x1fc, 0x1f8, 8, 0, 0x1fd};
static int KindCount[24];

static void Operation()
{
	TMainWindow *O = Main(OURS);
	bool InGame = O->InGame;
	Kind = Random() % 24;
	KindCount[Kind]++;
	if (getenv("MAINWINDOW_TRACE") != NULL) {
		printf("kind %d ingame %d\n", Kind, InGame ? 1 : 0);
		fflush(stdout);
	}
	char What[64];
	sprintf(What, "operation %d", Kind);
	switch (Kind) {
	case 0:
	case 1: {
		// The preferences may change between two games.
		if (Random() % 2 == 0)
			RandomOpenDialogs();
		if (Random() % 3 == 0)
			RandomWorld();
		bool Game = Random() % 3 != 0;
		Both([&](int s) {
			if (s == OURS)
				MainWindow->ChangeMode(Game);
			else
				ORIGINAL(At<MemberInt>(0x4af5e0)(OriginalMainWindow, NULL, Game));
		});
		// The new splitters' drag position is left uninitialised on both sides.
		for (int s = 0; s < SIDES && Game && !Threw; s++)
			Main(s)->Splitter->DragX = Main(s)->Splitter->DragY = 0;
		break;
	}
	case 2: {
		int Width, Height;
		RandomSize(Width, Height);
		Both([&](int s) { Main(s)->SetSize(Width, Height); });
		break;
	}
	case 3: {
		int Target = Random() % 3, x = Random() % 40, y = Random() % 40;
		Both([&](int s) { Main(s)->Draw(Target, x, y); });
		break;
	}
	case 4: {
		int Key = Keys[Random() % (sizeof(Keys) / sizeof(Keys[0]))];
		int State = Random() % 3 == 0 ? (int)(Random() % 10) : 8;
		if (!InGame && State == 8)
			State = Random() % 8;
		ConnectionState[OURS] = ConnectionState[ORIGINAL] = State;
		Both([&](int s) { Main(s)->KeyPress(Key); });
		break;
	}
	case 5:
	case 6: {
		int Event = AnyEvents[Random() % (sizeof(AnyEvents) / sizeof(AnyEvents[0]))];
		ConnectionState[OURS] = ConnectionState[ORIGINAL] = Random() % 2 == 0 ? 8 : Random() % 9;
		Fullscreen[OURS].Width = Fullscreen[ORIGINAL].Width = Random() % 4 == 0 ? 0 : 400 + Random() % 800;
		Fullscreen[OURS].Height = Fullscreen[ORIGINAL].Height = 300 + Random() % 600;
		Fullscreen[OURS].Depth = Fullscreen[ORIGINAL].Depth = 16;
		Fullscreen[OURS].Frequency = Fullscreen[ORIGINAL].Frequency = 60 + Random() % 30;
		TWindow *Modal = Random() % 3 == 0 ? (TWindow *)(uintptr_t)0x1000 : NULL;
		ModalWindow = OriginalModalWindow = Modal;
		Both([&](int s) { Main(s)->HandleEvent(Event); });
		ModalWindow = OriginalModalWindow = NULL;
		break;
	}
	case 7: {
		if (!InGame)
			return;
		int Event = GameEvents[Random() % 3];
		Both([&](int s) { Main(s)->HandleEvent(Event); });
		break;
	}
	case 8: {
		// The inventory windows swap (the assertion holds).
		if (!InGame)
			return;
		int Event = Find(OURS, O->SidePanel, 1) != NULL ? 0x17 : 0x18;
		Both([&](int s) { Main(s)->HandleEvent(Event); });
		break;
	}
	case 9: {
		if (!InGame)
			return;
		int Height = Random() % 500;
		Both([&](int s) {
			if (s == OURS)
				MainWindow->ResizeConsole(Height);
			else
				ORIGINAL(At<MemberInt>(0x4ac610)(OriginalMainWindow, NULL, Height));
		});
		if (Threw)
			break;
		int A = MainWindow->GetConsoleWindowHeight();
		int B = ORIGINAL_VALUE("console height", int, At<MemberGet>(0x4ac7c0)(OriginalMainWindow, NULL));
		CHECK(A == B, "GetConsoleWindowHeight %d, original %d", A, B);
		break;
	}
	case 10: {
		int dx = (int)(Random() % 3) - 1, dy = (int)(Random() % 3) - 1;
		bool Diagonal = Random() % 2 != 0;
		DestinationResult = Random() % 4;
		Both([&](int s) {
			if (s == OURS)
				MainWindow->Walk(dx, dy, Diagonal);
			else
				ORIGINAL(At<MemberWalk>(0x4ac7f0)(OriginalMainWindow, NULL, dx, dy, Diagonal));
		});
		break;
	}
	case 11: {
		if (!InGame)
			return;
		int Container = Random() % 16;
		BodyAccepts = Random() % 3 != 0;
		Both([&](int s) {
			if (s == OURS)
				MainWindow->OpenContainer(Container);
			else
				ORIGINAL(At<MemberInt>(0x4ac9e0)(OriginalMainWindow, NULL, Container));
		});
		break;
	}
	case 12: {
		int Container = Random() % 16;
		Both([&](int s) {
			if (s == OURS)
				MainWindow->CloseContainer(Container);
			else
				ORIGINAL(At<MemberInt>(0x4acdb0)(OriginalMainWindow, NULL, Container));
		});
		break;
	}
	case 13: {
		if (!InGame)
			return;
		bool Open = Find(OURS, O->DialogWindow, TRADE_WINDOW) != NULL;
		int What2 = Random() % 3;
		if (What2 == 0 && Open)
			What2 = 1;
		Both([&](int s) {
			if (What2 == 0) {
				if (s == OURS)
					MainWindow->OpenTrade();
				else
					ORIGINAL(At<MemberVoid>(0x4afec0)(OriginalMainWindow, NULL));
			} else if (What2 == 1) {
				if (s == OURS)
					MainWindow->UpdateTrade();
				else
					ORIGINAL(At<MemberVoid>(0x4acf60)(OriginalMainWindow, NULL));
			} else {
				if (s == OURS)
					MainWindow->CloseTrade();
				else
					ORIGINAL(At<MemberVoid>(0x4b0230)(OriginalMainWindow, NULL));
			}
		});
		break;
	}
	case 14: {
		if (!InGame)
			return;
		static const int Identifiers[] = {6, 7, 8, 9};
		int Identifier = Identifiers[Random() % 4];
		bool Open = Random() % 2 != 0;
		Both([&](int s) {
			if (Open) {
				if (s == OURS)
					MainWindow->OpenContentWindow(Identifier);
				else
					ORIGINAL(At<MemberInt>(0x4ad1e0)(OriginalMainWindow, NULL, Identifier));
			} else {
				if (s == OURS)
					MainWindow->CloseContentWindow(Identifier);
				else
					ORIGINAL(At<MemberInt>(0x4ad4a0)(OriginalMainWindow, NULL, Identifier));
			}
		});
		break;
	}
	case 15: {
		bool Active = Random() % 2 != 0;
		Both([&](int s) {
			if (s == OURS)
				MainWindow->SetConsoleActive(Active);
			else
				ORIGINAL(At<MemberInt>(0x4ad650)(OriginalMainWindow, NULL, Active));
		});
		break;
	}
	case 16:
	case 17:
	case 18: {
		// The splitter: press, move, release (directly or through the main window).
		if (!InGame)
			return;
		int x = Random() % 400, y = (int)(Random() % 40) - 20;
		int Action = Kind - 16;
		// A second press would fail TWindow::SetCapture's assertion.
		if (Action == 0 && O->Capture != NULL)
			return;
		Both([&](int s) {
			TComponent *Splitter = Main(s)->Splitter;
			if (Action == 0)
				Splitter->LeftButtonDown(x, y, 0);
			else if (Action == 1)
				Splitter->MouseMove(x, y);
			else
				Splitter->LeftButtonUp(x, y, 0);
		});
		break;
	}
	case 19: {
		// Mouse events through the main window (TWindow's routing to the children).
		int x = Random() % 1200, y = Random() % 900, Which = Random() % 3;
		if (Which == 0 && O->Capture != NULL)
			return;
		Both([&](int s) {
			TMainWindow *M = Main(s);
			if (Which == 0)
				M->LeftButtonDown(x, y, 0);
			else if (Which == 1)
				M->MouseMove(x, y);
			else
				M->LeftButtonUp(x, y, 0);
		});
		break;
	}
	case 20: {
		if (InGame)
			return;
		int Event = StartEvents[Random() % (sizeof(StartEvents) / sizeof(StartEvents[0]))];
		ConnectionState[OURS] = ConnectionState[ORIGINAL] = Random() % 2 == 0 ? 0 : Random() % 9;
		Both([&](int s) { Main(s)->StartWindow->HandleEvent(Event); });
		break;
	}
	case 21: {
		// The buttons' callbacks reach the start window.
		if (InGame)
			return;
		static const int ButtonEvents[] = {0x1f5, 0x1f7, 0x1f6, 7, 0x1fc};
		ConnectionState[OURS] = ConnectionState[ORIGINAL] = 0;
		int Event = ButtonEvents[Random() % 5];
		Both([&](int s) { Main(s)->StartWindow->HandleEvent(Event); });
		break;
	}
	case 22: {
		int Width, Height;
		RandomSize(Width, Height);
		RandomSkinProperties();
		Both([&](int s) { Main(s)->SetSize(Width, Height); });
		break;
	}
	case 23: {
		// The main window is made again.
		int Width, Height;
		RandomSize(Width, Height);
		bool WithModal = Random() % 2 != 0;
		ActionState[OURS] = ActionState[ORIGINAL] = 0;
		// The original's check after its ExitMainWindow: not reached (-1, the call threw before it), failed (0)
		// or passed (1); recorded, and checked on both sides after the calls.
		int OriginalLeft = -1;
		Both([&](int s) {
			if (WithModal) {
				TWindow *Modal = new TWindow;
				if (s == OURS)
					ModalWindow = Modal;
				else
					OriginalModalWindow = Modal;
			}
			if (s == OURS) {
				ExitMainWindow();
				CHECK(MainWindow == NULL && ModalWindow == NULL, "ExitMainWindow leaves a window");
				InitMainWindow(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL);
			} else {
#if GOLDEN_ORACLE
				At<void(__cdecl *)()>(0x4ac090)();
				OriginalLeft = OriginalMainWindow == NULL && OriginalModalWindow == NULL ? 1 : 0;
				At<InitFn>(0x4b05b0)(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL);
#endif
			}
		});
		OriginalLeft = ORIGINAL_VALUE("left", int, OriginalLeft);
		if (OriginalLeft >= 0)
			CHECK(OriginalLeft == 1, "0x4ac090 leaves a window");
		if (!Threw)
			NameOurVtables();
		break;
	}
	}
	Compare(What);
}

// --- Error paths (issue #34) ---------------------------------------------------

// New main windows for the error round (the old ones stay: after a throw
// their members may point to deleted windows).
static void NewMainWindows(int Fail)
{
	MainWindow = NULL;
	OriginalMainWindow = NULL;
	ModalWindow = OriginalModalWindow = NULL;
	Info[OURS].clear();
	Info[ORIGINAL].clear();
	Serials[OURS] = Serials[ORIGINAL] = 0;
	Log[OURS].clear();
	Log[ORIGINAL].clear();
	RandomWorld();
	RandomSkinProperties();
	ActionState[OURS] = ActionState[ORIGINAL] = 0;
	int Width, Height;
	RandomSize(Width, Height);
	Kind = 24;
	Threw = false;
	TErrorRound *Saved = MainRound;
	if (Fail == 0)
		MainRound = NULL;
	Both([&](int s) {
		if (s == OURS)
			InitMainWindow(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL);
		else
			ORIGINAL(At<InitFn>(0x4b05b0)(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL));
	});
	MainRound = Saved;
	if (!Threw)
		NameOurVtables();
	Compare("errors: InitMainWindow");
}

// One call on both sides, failing at the Fail-th fake call (0: none), what
// they threw compared.
template <class F> static void MainCall(TErrorRound &R, const char *What, F Call, int Fail = 0)
{
	R.Calling(What, "ours");
	TThrown A = OurThrow([&] { Side = OURS; Ticks = 0; FailAt = Fail; Call(OURS); });
	R.Calling(What, "the original");
	TThrown B = ORIGINAL_THROWN("thrown", Side = ORIGINAL; Ticks = 0; FailAt = Fail; Call(ORIGINAL));
	Side = ORIGINAL;
	Ticks = ORIGINAL_VALUE("ticks", int, Ticks); // the original's fake calls
	FailAt = 0;
	R.Compare(What, A, B);
	if (A.Kind != TThrown::NOTHING || B.Kind != TThrown::NOTHING)
		Threw = true;
	Compare(What);
}

static void ToGame(bool Game)
{
	Kind = 0;
	TErrorRound *Saved = MainRound;
	MainRound = NULL;
	Both([&](int s) {
		if (s == OURS)
			MainWindow->ChangeMode(Game);
		else
			ORIGINAL(At<MemberInt>(0x4af5e0)(OriginalMainWindow, NULL, Game));
	});
	MainRound = Saved;
	for (int s = 0; s < SIDES && Game; s++)
		Main(s)->Splitter->DragX = Main(s)->Splitter->DragY = 0;
	Compare("errors: ChangeMode");
}

// Every PATELLA_ASSERT/TERROR of MainWindow.cpp and the TEND blocks: the test's
// operations on new main windows, each failing at one of the fake calls it
// made in the counted test, until one throws; then the assertions.
static void TestMainWindowErrors()
{
	TErrorRound R("MainWindow");
	MainRound = &R;
	for (int Round = 0; Round < 405; Round++) {
		TargetKind = Round % 27;
		NewMainWindows(TargetKind == 24 ? 1 : 0);
		if (Threw)
			continue;
		for (int i = 0; i < 40 && !Threw; i++) {
			Operation();
			// The start window's own functions (TMainWindow::Draw turns its
			// errors into PROGRAM_ERROR 12).
			if (Threw || Main(OURS)->InGame || (TargetKind != 25 && TargetKind != 26))
				continue;
			int Width, Height;
			RandomSize(Width, Height);
			Kind = TargetKind;
			Both([&](int s) {
				if (Kind == 25)
					Main(s)->StartWindow->Draw(1, 0, 0);
				else
					Main(s)->StartWindow->SetSize(Width, Height);
			});
			Compare("errors: start window");
		}
	}
	TargetKind = -1;
	// The assertions, on new windows each.
	for (int Round = 0; Round < 12; Round++) {
		NewMainWindows(0);
		// InitMainWindow in an action state: the new window is made before the
		// assertion, and the round goes on with it.
		int State = 1 + Random() % 9;
		ActionState[OURS] = ActionState[ORIGINAL] = State;
		int Width, Height;
		RandomSize(Width, Height);
		MainCall(R, "InitMainWindow in an action state", [&](int s) {
			if (s == OURS)
				InitMainWindow(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL);
			else
				ORIGINAL(At<InitFn>(0x4b05b0)(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL));
		});
		ActionState[OURS] = ActionState[ORIGINAL] = 0;
		Threw = false;
		NameOurVtables();
		// Out of the game there is no dialog window.
		int Container = Random() % 16;
		MainCall(R, "TMainWindow::OpenContainer without a dialog window", [&](int s) {
			if (s == OURS) MainWindow->OpenContainer(Container);
			else ORIGINAL(At<MemberInt>(0x4ac9e0)(OriginalMainWindow, NULL, Container)); });
		MainCall(R, "TMainWindow::OpenTrade without a dialog window", [&](int s) {
			if (s == OURS) MainWindow->OpenTrade(); else ORIGINAL(At<MemberVoid>(0x4afec0)(OriginalMainWindow, NULL)); });
		Threw = false;
		ToGame(true);
		// The trade window twice, then without a body.
		MainCall(R, "TMainWindow::OpenTrade", [&](int s) {
			if (s == OURS) MainWindow->OpenTrade(); else ORIGINAL(At<MemberVoid>(0x4afec0)(OriginalMainWindow, NULL)); });
		MainCall(R, "TMainWindow::OpenTrade again", [&](int s) {
			if (s == OURS) MainWindow->OpenTrade(); else ORIGINAL(At<MemberVoid>(0x4afec0)(OriginalMainWindow, NULL)); });
		Threw = false;
		TSubWindow *Trade[2] = {NULL, NULL};
		for (int s = 0; s < SIDES; s++)
			Trade[s] = (TSubWindow *)Find(s, Main(s)->DialogWindow, TRADE_WINDOW);
		bool OriginalTrade = ORIGINAL_VALUE("trade window", bool, Trade[ORIGINAL] != NULL);
		if (Trade[OURS] != NULL && OriginalTrade) {
			TSubWindowBody *Body[2] = {Trade[OURS]->Body, NULL};
			Trade[OURS]->Body = NULL;
			ORIGINAL(Body[ORIGINAL] = Trade[ORIGINAL]->Body; Trade[ORIGINAL]->Body = NULL);
			MainCall(R, "TMainWindow::UpdateTrade without a body", [&](int s) {
				if (s == OURS) MainWindow->UpdateTrade(); else ORIGINAL(At<MemberVoid>(0x4acf60)(OriginalMainWindow, NULL)); });
			Trade[OURS]->Body = Body[OURS];
			ORIGINAL(Trade[ORIGINAL]->Body = Body[ORIGINAL]);
			Threw = false;
		}
		// Identifiers OpenContentWindow does not know.
		int Identifier = Random() % 2 ? 1 + (int)(Random() % 5) : 10 + (int)(Random() % 50);
		MainCall(R, "TMainWindow::OpenContentWindow unknown", [&](int s) {
			if (s == OURS) MainWindow->OpenContentWindow(Identifier);
			else ORIGINAL(At<MemberInt>(0x4ad1e0)(OriginalMainWindow, NULL, Identifier)); });
		Threw = false; // (the error comes before any change)
		// The splitter pressed while the main window has a capture, released
		// while dragging without one.
		for (int s = 0; s < SIDES; s++)
			Main(s)->Capture = Main(s)->Console;
		MainCall(R, "TSplitter::LeftButtonDown with a capture", [&](int s) { Main(s)->Splitter->LeftButtonDown(5, 6, 0); });
		for (int s = 0; s < SIDES; s++) {
			Main(s)->Capture = NULL;
			Main(s)->Splitter->Dragging = true;
		}
		MainCall(R, "TSplitter::LeftButtonUp without the capture", [&](int s) { Main(s)->Splitter->LeftButtonUp(5, 6, 0); });
		for (int s = 0; s < SIDES; s++)
			Main(s)->Splitter->Dragging = false;
		Threw = false;
		// The inventory windows' swap without the window it replaces (half of
		// the time after a swap).
		if (Round % 2 == 0) {
			int Swap = Find(OURS, MainWindow->SidePanel, 1) != NULL ? 0x17 : 0x18;
			MainCall(R, "TMainWindow::HandleEvent swap", [&](int s) { Main(s)->HandleEvent(Swap); });
			if (Threw)
				continue;
		}
		int Event = Find(OURS, MainWindow->SidePanel, 1) != NULL ? 0x18 : 0x17;
		MainCall(R, "TMainWindow::HandleEvent swap without the window", [&](int s) { Main(s)->HandleEvent(Event); });
		Threw = false; // (the assertion comes before any change)
		// The splitter's MouseMove: its first fake call (the cursor).
		MainCall(R, "TSplitter::MouseMove", [&](int s) { Main(s)->Splitter->MouseMove(5, 6); }, 1);
		Threw = false; // (nothing changed before)
		// Opening and closing: a container, the trade window, a content window;
		// the closing call fails at one of the fake calls it makes (the
		// skills and battle windows stay: the column's rearrangement asks their
		// bodies).
		for (int Id = 6; Id <= 7; Id++) {
			if (Find(OURS, MainWindow->DialogWindow, Id) == NULL)
				MainCall(R, "errors: open", [&](int s) {
					if (s == OURS) MainWindow->OpenContentWindow(Id);
					else ORIGINAL(At<MemberInt>(0x4ad1e0)(OriginalMainWindow, NULL, Id)); });
		}
		for (int What = 0; What < 3 && !Threw; What++) {
			int Box = Random() % 16, Content = 9;
			auto Open = [&](int s) {
				if (What == 0) {
					if (s == OURS) MainWindow->OpenContainer(Box);
					else ORIGINAL(At<MemberInt>(0x4ac9e0)(OriginalMainWindow, NULL, Box));
				} else if (What == 1) {
					if (Find(s, Main(s)->DialogWindow, TRADE_WINDOW) == NULL) {
						if (s == OURS) MainWindow->OpenTrade();
						else ORIGINAL(At<MemberVoid>(0x4afec0)(OriginalMainWindow, NULL));
					}
				} else {
					if (s == OURS) MainWindow->OpenContentWindow(Content);
					else ORIGINAL(At<MemberInt>(0x4ad1e0)(OriginalMainWindow, NULL, Content));
				}
			};
			auto Close = [&](int s) {
				if (What == 0) {
					if (s == OURS) MainWindow->CloseContainer(Box);
					else ORIGINAL(At<MemberInt>(0x4acdb0)(OriginalMainWindow, NULL, Box));
				} else if (What == 1) {
					if (s == OURS) MainWindow->CloseTrade();
					else ORIGINAL(At<MemberVoid>(0x4b0230)(OriginalMainWindow, NULL));
				} else {
					if (s == OURS) MainWindow->CloseContentWindow(Content);
					else ORIGINAL(At<MemberInt>(0x4ad4a0)(OriginalMainWindow, NULL, Content));
				}
			};
			static const char *const Names[] = {"TMainWindow::CloseContainer", "TMainWindow::CloseTrade",
				"TMainWindow::CloseContentWindow"};
			MainCall(R, "errors: open", Open);
			MainCall(R, Names[What], Close);
			int Count = Ticks;
			if (Threw || Count == 0)
				continue;
			MainCall(R, "errors: open", Open);
			if (!Threw)
				MainCall(R, Names[What], Close, 1 + (int)(Random() % Count));
		}
		if (Threw)
			continue;
	}
	MainWindow = NULL;
	OriginalMainWindow = NULL;
	ModalWindow = OriginalModalWindow = NULL;
	Threw = false;
	MainRound = NULL;
	R.Finish();
}

} // namespace MainWindowTest
using namespace MainWindowTest;

void TestMainWindow()
{
	int Before = Checks, BeforeFailures = Failures;
	Running = true;
	ORIGINAL(PatchImage());
	InitSkins();
	ORIGINAL(Global<int>(0x71c450) = NumberOfSkins; Global<TSkin *>(0x71c454) = Skins.get());
	InitSkinProperties();
	ORIGINAL(At<void(__cdecl *)()>(0x498160)());
	TTalkBuffer *SavedBuffer = DefaultTextBuffer;
#if GOLDEN_ORACLE
	TTalkBuffer *&OriginalBuffer = Global<TTalkBuffer *>(0x71ef68);
	TTalkBuffer *SavedOriginalBuffer = OriginalBuffer;
	OriginalBuffer = (TTalkBuffer *)(uintptr_t)0x12340;
#endif
	DefaultTextBuffer = (TTalkBuffer *)(uintptr_t)0x12340;
	Surface = new TMainSurface;
	FindFakeVtables();
	PatchCallees();
	DialogFakes.Body = HookBody;
	DialogFakes.BodyAccepts = HookBodyAccepts;
	DialogFakes.SetSkins = HookSetSkins;

	// The image's classes, and ours that can be made directly.
#if GOLDEN_ORACLE
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x56c498, "Main", true);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x56c5c0, "Start", true);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x56c660, "Splitter", false);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x564528, "Window", true);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x56d0b8, "RealFrame", true);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x56cf38, "MapView", true);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x563f80, "Dialog", true);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x563ed8, "Sub", true);
	NameVtable(ORIGINAL, (void *)(uintptr_t)0x563e38, "CW", true);
#endif
	{
		TWindow W;
		NameVtable(OURS, VtableOf(&W), "Window", true);
	}
	struct {
		void *Vtable;
		const char *Name;
		bool Window;
	} Fakes[] = {
		{FakeConsoleVtable, "Console", true},
		{FakeMapFrameVtable, "Frame", true},
		{FakeBorderVtable, "Border", true},
		{FakeButtonVtable, "Button", false},
	};
	for (auto &F : Fakes) {
		NameVtable(OURS, F.Vtable, F.Name, F.Window);
		NameVtable(ORIGINAL, F.Vtable, F.Name, F.Window);
	}
	Quiet = true;
	{
		// Vtables of the fakes that cannot be made without a constructor call.
		TFakeBody *Body = new TFakeBody;
		TSubWindow *Sub = new TSubWindow(6, Body, false, false);
		TContentWindow *Content = new TContentWindow(1, false);
		TDialogWindow *Dialog = new TDialogWindow;
		TMainSide3 *S3 = new TMainSide3;
		TMainSide4 *S4 = new TMainSide4;
		TMainSide5 *S5 = new TMainSide5(NULL, NULL);
		TFont Font;
		TFakeLabel *Label = new TFakeLabel(Font, "");
		TObject Obj;
		TRGBColor Color;
		TObjectIcon *Icon = new TObjectIcon(Obj, &Color, false);
		struct {
			void *Object;
			const char *Name;
			bool Window;
		} List[] = {
			{Body, "Body", true},
			{S3, "Side3", true},
			{S4, "Side4", true},
			{S5, "Side5", true},
			{Label, "Label", false},
			{Icon, "Icon", false},
		};
		for (auto &L : List) {
			NameVtable(OURS, VtableOf(L.Object), L.Name, L.Window);
			NameVtable(ORIGINAL, VtableOf(L.Object), L.Name, L.Window);
		}
		// The real icons of the image (TObjectIcon's constructor is inline).
		ORIGINAL(NameVtable(ORIGINAL, (void *)(uintptr_t)0x560d88, "Icon", false));
		NameVtable(OURS, VtableOf(Sub), "Sub", true);
		NameVtable(OURS, VtableOf(Content), "CW", true);
		NameVtable(OURS, VtableOf(Dialog), "Dialog", true);
		delete Sub;
		delete Content;
		delete Dialog;
		delete S3;
		delete S4;
		delete S5;
		delete Label;
		delete Icon;
		// Our TMainWindow, TStartWindow, TSplitter, TMapFrame and TMapWindow,
		// from a main window made quietly.
		RandomWorld();
		Side = OURS;
		TMainWindow *Tmp = new TMainWindow(800, 600);
		NameVtable(OURS, VtableOf(Tmp), "Main", true);
		NameVtable(OURS, VtableOf(Tmp->StartWindow), "Start", true);
		Tmp->ChangeMode(true);
		NameVtable(OURS, VtableOf(Tmp->Splitter), "Splitter", false);
		NameVtable(OURS, VtableOf(Tmp->MapFrame), "RealFrame", true);
		NameVtable(OURS, VtableOf(((TWindow *)Tmp->MapFrame)->FirstChild), "MapView", true);
		delete Tmp;
		OpenDialogs[OURS].clear();
	}
	Quiet = false;
	Info[OURS].clear();
	Info[ORIGINAL].clear();
	Serials[OURS] = Serials[ORIGINAL] = 0;
	Log[OURS].clear();
	Log[ORIGINAL].clear();

	RandomWorld();
	RandomSkinProperties();
	ActionState[OURS] = ActionState[ORIGINAL] = 0;
	ModalWindow = OriginalModalWindow = NULL;
	int Width, Height;
	RandomSize(Width, Height);
	Both([&](int s) {
		if (s == OURS)
			InitMainWindow(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL);
		else
			ORIGINAL(At<InitFn>(0x4b05b0)(Width, Height, FakeExitProgram, FakeSetDisplayMode, FakeOpenURL));
	});
	NameOurVtables();
	Compare("InitMainWindow");
	bool Trace = getenv("MAINWINDOW_TRACE") != NULL;
	for (int i = 0; i < 6000; i++) {
		if (Trace) {
			printf("op %d\n", i);
			fflush(stdout);
		}
		Operation();
	}
	Both([&](int s) {
		if (s == OURS)
			ExitMainWindow();
		else
			ORIGINAL(At<void(__cdecl *)()>(0x4ac090)());
	});
	Compare("ExitMainWindow");
	// The error round (issue #34): its checks are reported apart.
	int RoundChecks = Checks, SavedKinds[24];
	memcpy(SavedKinds, KindCount, sizeof(SavedKinds));
	TestMainWindowErrors();
	memcpy(KindCount, SavedKinds, sizeof(SavedKinds));
	RoundChecks = Checks - RoundChecks;

	DefaultTextBuffer = SavedBuffer;
	ORIGINAL(OriginalBuffer = SavedOriginalBuffer);
	ExitSkins();
	ORIGINAL(Global<TSkin *>(0x71c454) = NULL);
	int Least = KindCount[0];
	for (int i = 1; i < 24; i++)
		Least = KindCount[i] < Least ? KindCount[i] : Least;
	printf("MainWindow: %d checks, %d failures (each of the 24 operations at least %d times)\n",
		Checks - Before - RoundChecks, Failures - BeforeFailures, Least);
}
