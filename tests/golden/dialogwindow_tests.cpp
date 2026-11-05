// difftest.exe's DialogWindow test (moved here from tools/difftest/dialogwindowtest.cpp, issue #28):
// DialogWindow.cpp against the original, as a golden-corpus test (golden.h).
//
// Two columns are built twice, from the image's classes (constructors
// 0x4856c0, 0x4854c0 and 0x487e80) and from ours: column 0 holds sub-windows
// (TSubWindow, like MainWindow.cpp's DialogWindow), column 1 plain content
// windows (TContentWindow, like its SidePanel). The parts from files that are
// not reconstructed are the same objects of this file on both sides: the
// bodies, the buttons, the icon and the title (test classes with logging and
// deterministic answers), the resizers (TResizer's constructor, also behind
// the image's 0x453be0) and TSkinButton::SetSkins (0x458b20). DrawSkin, the
// surface's FillRectangle, SetContentWindowSize and AddOpenDialog are logging
// fakes on both sides; TWindow and TComponent are the image's for the
// original objects and ours for ours.
//
// Both sides get the same random operations: new windows (Rearrange 16 or
// the child functions), deletions, sizes of the columns and the windows,
// drawing, mouse presses, moves and releases through the columns (dragging),
// double clicks, minimize and maximize, the close event, the five setters,
// the "up" button, every Rearrange strategy, StoreOpenDialogs and the
// queries. After each operation the logs, the results and a dump of both
// trees (positions, sizes, drawing lists, captures and every member of the
// three classes) are compared. Failing assertions are not exercised: the
// operations keep to what the game does (windows wider than their title,
// bodies at least one pixel high, no structural change while dragging).
// The error paths have a round of their own (issue #34, TestDialogWindowErrors,
// "DialogWindow errors: ..."): the bodies and elements throw on demand, the
// assertions get bad arguments.
//
// By value (plan 4.3): the original's side exists in the oracle only (every use of it is inside ORIGINAL or
// #if GOLDEN_ORACLE); the runner has our columns alone. Per comparison the corpus holds the original's log
// (text), its dump (text, by its changes since the last one, golden::OutputTextChanges), the results of its
// calls and in the error round what each call threw (GOLDEN_ROUND_COMPARE). The operands are chosen from
// our side, as before. The fakes of the DialogElements.cpp classes are elementfakes.h's, with our x64
// layout in the runner; our functions go to the fakes through golden::RedirectOwn (restored at the end).

#include "golden_test.h"

#include "elementfakes.h"
#include "golden_eh.h"
#include "testsurface.h"
#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#else
#include "golden_patch.h"
#endif

#include "ContentWindows.h"
#include "DialogWindow.h"
#include "GUI.h"
#include "Preferences.h"
#include "Skins.h"
#include "Surface.h"
#include "graphics.h"

#include <map>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#if GOLDEN_ORACLE
static_assert(sizeof(TContentWindow) == 0x44, "TContentWindow is 0x44 bytes");
static_assert(sizeof(TSubWindow) == 0x70, "TSubWindow is 0x70 bytes");
static_assert(sizeof(TDialogWindow) == 0x4c, "TDialogWindow is 0x4c bytes");
static_assert(offsetof(TContentWindow, Identifier) == 0x2c, "TContentWindow::Identifier at +0x2c");
static_assert(offsetof(TContentWindow, Dragging) == 0x35, "TContentWindow::Dragging at +0x35");
static_assert(offsetof(TContentWindow, DragBottom) == 0x40, "TContentWindow::DragBottom at +0x40");
static_assert(offsetof(TSubWindow, Body) == 0x44, "TSubWindow::Body at +0x44");
static_assert(offsetof(TSubWindow, IsMinimized) == 0x64, "TSubWindow::IsMinimized at +0x64");
static_assert(offsetof(TSubWindow, RestoreHeight) == 0x6c, "TSubWindow::RestoreHeight at +0x6c");
static_assert(offsetof(TDialogWindow, Area) == 0x2c, "TDialogWindow::Area at +0x2c");
static_assert(offsetof(TDialogWindow, Dragged) == 0x3c, "TDialogWindow::Dragged at +0x3c");
static_assert(offsetof(TDialogWindow, UpdateFreeHeight) == 0x48, "TDialogWindow::UpdateFreeHeight at +0x48");
static_assert(sizeof(TResizer) == 0x38, "TResizer is 0x38 bytes");
#endif

// --- Shared fakes (mainwindowtest.cpp uses them as well) ----------------------------

TDialogFakes DialogFakes;

// The container body's UpdateUpButton (the image's 0x42e4e0 and ours; elementfakes.h).
#if GOLDEN_ORACLE
void __fastcall FakeUpdateUpButton(void *Body, void *)
#else
void FakeUpdateUpButton(void *Body)
#endif
{
	if (DialogFakes.Body != NULL)
		DialogFakes.Body((TSubWindowBody *)Body, "Up");
}

TFakeResizer::TFakeResizer(TComponent *_ToResize, bool Param2, bool Param3, bool Param4)
{
	memset(Data24, 0, sizeof(Data24));
	if (DialogFakes.NewResizer != NULL)
		DialogFakes.NewResizer((TResizer *)this, _ToResize, Param2, Param3, Param4);
}

void TFakeSkinButton::SetSkins(int _ButtonUp, int _ButtonDown)
{
	if (DialogFakes.SetSkins != NULL)
		DialogFakes.SetSkins((TSkinButton *)this, _ButtonUp, _ButtonDown);
}

// The test's own definitions: in a namespace of their own (mainwindowtest.cpp, linked into the same
// difftest.exe, defines a struct TInfo of another layout at global scope).
namespace DialogWindowTest {

// --- Logs and names ----------------------------------------------------------------

enum { OURS = 0,
	ORIGINAL = 1 };
// The sides that have objects: both in the oracle, ours in the runner.
static const int SIDES = GOLDEN_ORACLE ? 2 : 1;
static int Side;
static std::string Log[2];

static void Logf(const char *Format, ...)
{
	char Buffer[512];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

static void *VtableOf(const void *Object) { return *(void **)Object; }
static void SetVtable(void *Object, void *Vtable) { *(void **)Object = Vtable; }
template <class T> static void *Member(T Function) { return *(void **)&Function; }

// Every object of the test has a kind and a serial number per side; both
// sides make their objects in the same order, so the numbers match.
struct TInfo {
	std::string Kind;
	int Serial;
};
static std::map<const void *, TInfo> Info[2];
static int Serials[2];

static void Register(const void *Object, const char *Kind)
{
	TInfo &I = Info[Side][Object];
	I.Kind = Kind;
	I.Serial = Serials[Side]++;
}

static std::string NameOf(int s, const void *Object)
{
	if (Object == NULL)
		return "0";
	auto I = Info[s].find(Object);
	if (I == Info[s].end())
		return "?";
	char Buffer[64];
	sprintf(Buffer, "%s#%d", I->second.Kind.c_str(), I->second.Serial);
	return Buffer;
}
static std::string Name(const void *Object) { return NameOf(Side, Object); }

// The classes by vtable, per side.
enum { CLASS_OTHER,
	CLASS_COLUMN,
	CLASS_CONTENT,
	CLASS_SUB };
static std::map<void *, int> ClassOf[2];
static int Class(int s, const void *Object)
{
	auto C = ClassOf[s].find(VtableOf(Object));
	return C == ClassOf[s].end() ? CLASS_OTHER : C->second;
}

// --- Test classes of the parts --------------------------------------------------------

static int Answer(int a, int b, int c) { return (a * 7 + b * 3 + c) % 4 == 0; }

// The error round's throwing test objects: a method whose bit is set throws
// an assertion of the side that calls it (Side), with -Bit as its value.
static int ThrowMask;
static void MaybeThrow(int Bit)
{
	if ((ThrowMask & Bit) != 0)
		ThrowSideAssertion(Side == ORIGINAL, -Bit);
}

// The test body reports the calls of TSubWindow's Minimize and Maximize (the
// body's +0xa0, +0xa4).
class TTestBody : public TSubWindowBody {
public:
	int MinHeight;
	void Minimize()
	{
		if (DialogFakes.Body != NULL) DialogFakes.Body(this, "A0");
		MaybeThrow(32);
	}
	void Maximize()
	{
		if (DialogFakes.Body != NULL) DialogFakes.Body(this, "A4");
		MaybeThrow(32);
	}
	int GetInitialHeight()
	{
		MaybeThrow(16);
		return MinHeight;
	}
	int GetContentHeight()
	{
		MaybeThrow(16);
		return MinHeight;
	}
	int getMinimumHeight()
	{
		MaybeThrow(16);
		return MinHeight;
	}
	void Draw(int Surface, int x, int y)
	{
		Logf("DBody%s,%d,%d,%d;", Name(this).c_str(), Surface, x, y);
		MaybeThrow(1);
	}
	bool LeftButtonDown(int x, int y, int Flags)
	{
		Logf("PBody%s,%d,%d,%d;", Name(this).c_str(), x, y, Flags);
		MaybeThrow(2);
		return Answer(x, y, MinHeight) != 0;
	}
	bool MouseMove(int x, int y)
	{
		Logf("MBody%s,%d,%d;", Name(this).c_str(), x, y);
		MaybeThrow(4);
		return false;
	}
	bool LeftButtonDoubleClick(int x, int y, int)
	{
		Logf("WBody%s,%d,%d;", Name(this).c_str(), x, y);
		MaybeThrow(8);
		return Answer(y, x, 1) != 0;
	}
	void SetPosition(int x, int y)
	{
		MaybeThrow(64);
		TSubWindowBody::SetPosition(x, y);
	}
	void SetSize(int _Width, int _Height)
	{
		MaybeThrow(64);
		TSubWindowBody::SetSize(_Width, _Height);
	}
	void Arrange()
	{
		MaybeThrow(128);
		TSubWindowBody::Arrange();
	}
};

class TTestElement : public TElement {
public:
	void Draw(int Surface, int x, int y)
	{
		Logf("DElem%s,%d,%d,%d;", Name(this).c_str(), Surface, x, y);
		MaybeThrow(1);
	}
	bool LeftButtonDown(int x, int y, int)
	{
		Logf("PElem%s,%d,%d;", Name(this).c_str(), x, y);
		MaybeThrow(2);
		return Answer(x, y, 2) != 0;
	}
	bool LeftButtonDoubleClick(int x, int y, int)
	{
		Logf("WElem%s,%d,%d;", Name(this).c_str(), x, y);
		MaybeThrow(8);
		return Answer(x, y, 3) != 0;
	}
	void SetPosition(int x, int y)
	{
		MaybeThrow(64);
		TElement::SetPosition(x, y);
	}
	void SetSize(int _Width, int _Height)
	{
		MaybeThrow(64);
		TElement::SetSize(_Width, _Height);
	}
	void Arrange()
	{
		MaybeThrow(128);
		TElement::Arrange();
	}
	bool LeftButtonUp(int x, int y, int Flags)
	{
		MaybeThrow(2);
		return TElement::LeftButtonUp(x, y, Flags);
	}
};

class TTestSkinButton : public TFakeSkinButton {
public:
	TTestSkinButton() : TFakeSkinButton(0, 0, 0, 0, TCallback(), NULL, 0) {}
	void Draw(int Surface, int x, int y) { Logf("DSkin%s,%d,%d,%d;", Name(this).c_str(), Surface, x, y); }
	bool LeftButtonDown(int x, int y, int)
	{
		Logf("PSkin%s,%d,%d;", Name(this).c_str(), x, y);
		return Answer(x, y, 1) != 0;
	}
};

static void *FakeResizerVtable;
class TDWTestResizer : public TFakeResizer {
public:
	TDWTestResizer() : TFakeResizer(NULL, false, false, false) {}
	void Draw(int Surface, int x, int y) { Logf("DRes%s,%d,%d,%d;", Name(this).c_str(), Surface, x, y); }
	bool LeftButtonDown(int x, int y, int)
	{
		Logf("PRes%s,%d,%d;", Name(this).c_str(), x, y);
		return Answer(x, y, Data24[0]) != 0;
	}
	bool MouseMove(int x, int y)
	{
		Logf("MRes%s,%d,%d;", Name(this).c_str(), x, y);
		return Answer(y, x, Data24[0]) != 0;
	}
};

static void HookBody(TSubWindowBody *Body, const char *Call) { Logf("Body%s,%s;", Call, Name(Body).c_str()); }
static bool HookBodyAccepts(TSubWindowBody *) { return true; }
static void HookNewResizer(TResizer *Resizer, TComponent *ToResize, bool Param2, bool Param3, bool Param4)
{
	Register(Resizer, "Res");
	Logf("NRes%s,%s,%d,%d,%d;", Name(Resizer).c_str(), Name(ToResize).c_str(), Param2, Param3, Param4);
	((TFakeResizer *)Resizer)->Data24[0] = (uint8)(Param4 ? 1 : 0);
	if (FakeResizerVtable != NULL)
		SetVtable(Resizer, FakeResizerVtable);
}
static void HookSetSkins(TSkinButton *Button, int ButtonUp, int ButtonDown)
{
	Logf("Skins%s,%d,%d;", Name(Button).c_str(), ButtonUp, ButtonDown);
}


// --- Fakes of the other callees ---------------------------------------------------------

static void __cdecl FakeDrawSkin(int Surface, int x, int y, int width, int height, int skin, int srcx, int srcy)
{
	Logf("Skin%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, width, height, skin, srcx, srcy);
}
static void __cdecl FakeDrawPicturePart(int Surface, int x, int y, int PictureNumber, int srcx, int srcy, int Width,
	int Height)
{
	Logf("Part%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, PictureNumber, srcx, srcy, Width, Height);
}
// Not a thrower: ~TSubWindow calls it, and our destructors are noexcept
// (C++11; the original's were not), so a throw there ends the child.
static void __cdecl FakeSetContentWindowSize(int ID, int Size) { Logf("CWSize%d,%d;", ID, Size); }
static void __cdecl FakeAddOpenDialog(int Identifier)
{
	Logf("Open%d;", Identifier);
	MaybeThrow(256);
}

class TDialogSurface : public TTestSurface {
public:
	void FillRectangle(int Surface, TRect R, TRGBColor C)
	{
		Logf("Fill%d,%d,%d,%d,%d,%d,%d,%d;", Surface, R.x, R.y, R.width, R.height, C.R, C.G, C.B);
	}
};
static TDialogSurface *DialogSurface;
static TSurface *__cdecl FakeGetSurface() { return DialogSurface; }

// Patches that are undone at the end (later tests use these functions).
struct TSaved {
	uint8 *Code;
	uint8 Bytes[5];
};
static std::vector<TSaved> SavedCode;

// The oracle patches the image and difftest.exe's code with PatchJump; the runner its own code with
// golden::RedirectOwn (golden_patch.h), saved and restored the same way.
static void Redirect(void *Function, void *Target)
{
	TSaved S;
	S.Code = (uint8 *)Function;
#if GOLDEN_ORACLE
	DWORD Protection;
	VirtualProtect(Function, 5, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(S.Bytes, Function, 5);
	PatchJump((uint32)(uintptr_t)Function, Target);
	VirtualProtect(Function, 5, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Function, 5);
#else
	golden::SaveOwn(Function, S.Bytes);
	golden::RedirectOwn(Function, Target);
#endif
	SavedCode.push_back(S);
}

static void RestoreAll()
{
	for (size_t i = SavedCode.size(); i-- > 0;) {
#if GOLDEN_ORACLE
		DWORD Protection;
		VirtualProtect(SavedCode[i].Code, 5, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(SavedCode[i].Code, SavedCode[i].Bytes, 5);
		VirtualProtect(SavedCode[i].Code, 5, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), SavedCode[i].Code, 5);
#else
		golden::RestoreOwn(SavedCode[i].Code, SavedCode[i].Bytes);
#endif
	}
	SavedCode.clear();
}

static void PatchCallees()
{
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		{0x497a00, (void *)DrawSkin, (void *)FakeDrawSkin},
		{0x490c60, (void *)DrawPicturePart, (void *)FakeDrawPicturePart},
		{0x4eaa90, (void *)GetSurface, (void *)FakeGetSurface},
		{0x4dbcd0, (void *)SetContentWindowSize, (void *)FakeSetContentWindowSize},
		{0x4dc950, (void *)AddOpenDialog, (void *)FakeAddOpenDialog},
	};
	for (auto &T : Table) {
		ORIGINAL(Redirect((void *)(uintptr_t)T.Image, T.Fake));
		Redirect(T.Ours, T.Fake);
	}
	// DialogElements.cpp: both sides build the fakes (elementfakes.cpp).
	PatchElementFakes(Redirect);
	ORIGINAL(Redirect((void *)(uintptr_t)0x42e4e0, (void *)FakeUpdateUpButton));
	Redirect(Member(&TContainerWindowBody::UpdateUpButton), (void *)FakeUpdateUpButton);
}

// --- The objects ----------------------------------------------------------------------

#if GOLDEN_ORACLE
typedef void *(__fastcall *ColumnCtorFn)(void *, void *);
typedef void *(__fastcall *ContentCtorFn)(void *, void *, int, bool);
typedef void *(__fastcall *SubCtorFn)(void *, void *, int, TSubWindowBody *, bool, bool);
typedef TContentWindow *(__fastcall *FindFn)(void *, void *, int);
typedef int(__fastcall *GetFn)(void *, void *);
typedef void(__fastcall *VoidFn)(void *, void *);
typedef void(__fastcall *SetFn)(void *, void *, TComponent *);
typedef bool(__fastcall *BoolFn)(void *, void *);
typedef void(__fastcall *PositionFn)(void *, void *, bool);
#endif

static TDialogWindow *Column[2][2]; // [side][column]

static TDialogWindow *NewColumn(int s)
{
	TDialogWindow *C;
	if (s == OURS) {
		C = new TDialogWindow;
	} else {
#if GOLDEN_ORACLE
		void *Memory = operator new(sizeof(TDialogWindow));
		C = (TDialogWindow *)At<ColumnCtorFn>(0x4856c0)(Memory, NULL);
#else
		abort(); // the runner has no original
#endif
	}
	Register(C, "Col");
	return C;
}

static TContentWindow *NewContent(int s, int Identifier, bool Background)
{
	TContentWindow *W;
	if (s == OURS) {
		W = new TContentWindow(Identifier, Background);
	} else {
#if GOLDEN_ORACLE
		void *Memory = operator new(sizeof(TContentWindow));
		W = (TContentWindow *)At<ContentCtorFn>(0x4854c0)(Memory, NULL, Identifier, Background);
#else
		abort();
#endif
	}
	Register(W, "CW");
	return W;
}

static TTestBody *NewBody(int MinHeight)
{
	TTestBody *B = new TTestBody;
	B->MinHeight = MinHeight;
	Register(B, "Body");
	return B;
}

static TSubWindow *NewSub(int s, int Identifier, TTestBody *Body, bool Resizable, bool Background)
{
	TSubWindow *W;
	void *Memory = operator new(sizeof(TSubWindow));
	Register(Memory, "Sub"); // before the constructor: the resizers name it
#if GOLDEN_ORACLE
	if (s == OURS)
		W = new (Memory) TSubWindow(Identifier, Body, Resizable, Background);
	else
		W = (TSubWindow *)At<SubCtorFn>(0x487e80)(Memory, NULL, Identifier, Body, Resizable, Background);
#else
	(void)s; // ours: the runner has no original
	W = new (Memory) TSubWindow(Identifier, Body, Resizable, Background);
#endif
	return W;
}

// Runs one call on both sides: F(s) with Side set. An error of our side (an
// assertion the original would fail as well, or one it would not) is
// reported and ends the test.
static int Kind;
template <class F> static void Both(F Call)
{
	Side = OURS;
	try {
		Call(OURS);
	} catch (TMyException &E) {
		printf("FAIL DialogWindow (kind %d): our side raised %s\n", Kind, E.GetMessage().c_str());
		exit(1);
	}
	ORIGINAL(Side = ORIGINAL; Call(ORIGINAL));
}

// --- Dumps ------------------------------------------------------------------------------

static void DumpComponent(int s, std::string &Out, TComponent *C, int Depth, const char *Role);

static void DumpPart(int s, std::string &Out, TSubWindow *W, TComponent *Part, int Depth, const char *Role)
{
	if (Part != NULL && Part->Parent != W)
		DumpComponent(s, Out, Part, Depth, Role);
}

static void DumpComponent(int s, std::string &Out, TComponent *C, int Depth, const char *Role)
{
	char Buffer[512];
	sprintf(Buffer, "%*s%s%s c%d %d,%d %dx%d", Depth * 2, "", Role, NameOf(s, C).c_str(), Class(s, C), C->OffsetX,
		C->OffsetY, C->Width, C->Height);
	Out += Buffer;
	if (C->DrawingListLength > 0 && C->DrawingList != NULL) {
		sprintf(Buffer, " DL%d", C->DrawingListLength);
		Out += Buffer;
		for (int i = 0; i < C->DrawingListLength; i++) {
			TDrawing &D = C->DrawingList[i];
			sprintf(Buffer, " %d/%d/%d/%d/%d/%d/%d", D.skin, D.x, D.y, D.width, D.height, D.srcx, D.srcy);
			Out += Buffer;
		}
	}
	int K = Class(s, C);
	if (K == CLASS_CONTENT || K == CLASS_SUB) {
		TContentWindow *W = (TContentWindow *)C;
		sprintf(Buffer, " id %d bg %d,%d drag %d %d,%d,%d", W->Identifier, W->Background ? 1 : 0, W->BackgroundY,
			W->Dragging ? 1 : 0, W->DragX, W->DragY, W->DragBottom);
		Out += Buffer;
	}
	if (K == CLASS_SUB) {
		TSubWindow *W = (TSubWindow *)C;
		sprintf(Buffer, " min %d %d,%d", W->IsMinimized ? 1 : 0, W->Data68, W->RestoreHeight);
		Out += Buffer;
		Out += " parts " + NameOf(s, W->Body) + "," + NameOf(s, W->Resizer1) + "," + NameOf(s, W->Resizer2) + ","
			+ NameOf(s, W->UpButton) + "," + NameOf(s, W->CloseButton) + "," + NameOf(s, W->MinimizeButton) + ","
			+ NameOf(s, W->Icon) + "," + NameOf(s, W->Title);
	}
	if (K == CLASS_COLUMN) {
		TDialogWindow *D = (TDialogWindow *)C;
		sprintf(Buffer, " area %d,%d,%d,%d busy %d free %d,%d", D->Area.x, D->Area.y, D->Area.width, D->Area.height,
			D->Busy ? 1 : 0, D->FreeHeight, D->UpdateFreeHeight ? 1 : 0);
		Out += Buffer;
		Out += " dragged " + NameOf(s, D->Dragged);
	}
	Out += " parent " + NameOf(s, C->Parent) + "\n";
	if (K == CLASS_COLUMN || K == CLASS_CONTENT || K == CLASS_SUB) {
		TWindow *Win = (TWindow *)C;
		if (Win->Capture != NULL)
			Out += std::string(Depth * 2 + 2, ' ') + "capture " + NameOf(s, Win->Capture) + "\n";
		for (TComponent *Child = Win->FirstChild; Child != NULL; Child = Child->Next)
			DumpComponent(s, Out, Child, Depth + 1, "");
	}
	if (K == CLASS_SUB) {
		// The parts that are not children at the moment.
		TSubWindow *W = (TSubWindow *)C;
		DumpPart(s, Out, W, W->Body, Depth + 1, "-body ");
		DumpPart(s, Out, W, W->Resizer1, Depth + 1, "-res1 ");
		DumpPart(s, Out, W, W->Resizer2, Depth + 1, "-res2 ");
		DumpPart(s, Out, W, W->UpButton, Depth + 1, "-up ");
	}
}

static std::string Dump(int s)
{
	std::string Out;
	for (int c = 0; c < 2; c++)
		DumpComponent(s, Out, Column[s][c], 0, "");
	return Out;
}

// The original's log of a comparison: the oracle's, recorded; the runner's from the corpus.
static void OutputOriginalLog()
{
	std::string Text = golden::OutputText("log", Log[ORIGINAL].data(), Log[ORIGINAL].size());
	ORIGINAL_OR((void)0, Log[ORIGINAL] = Text);
}

// The original's dump: the oracle's, recorded by its changes since the last one; the runner's from the
// corpus.
static std::string LastDump;
static std::string OriginalDump()
{
#if GOLDEN_ORACLE
	std::string Text = Dump(ORIGINAL);
#else
	std::string Text;
#endif
	return golden::OutputTextChanges("dump", &LastDump, Text);
}

static void Compare(const char *What)
{
	OutputOriginalLog();
	Checks++;
	if (Log[OURS] != Log[ORIGINAL]) {
		Failures++;
		if (Failures <= FailuresPrinted) {
			printf("FAIL DialogWindow %s (kind %d): logs differ\n  ours:     %.800s\n  original: %.800s\n", What, Kind,
				Log[OURS].c_str(), Log[ORIGINAL].c_str());
		}
	}
	std::string A = Dump(OURS), B = OriginalDump();
	Checks++;
	if (A != B) {
		Failures++;
		if (Failures <= FailuresPrinted)
			printf("FAIL DialogWindow %s (kind %d): windows differ\n--- ours\n%s--- original\n%s", What, Kind, A.c_str(),
				B.c_str());
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

// --- Choosing operands (from our side; the other side's object has the same position) ----

static std::vector<TComponent *> ChildrenOf(TWindow *W)
{
	std::vector<TComponent *> List;
	for (TComponent *Child = W->FirstChild; Child != NULL; Child = Child->Next)
		List.push_back(Child);
	return List;
}

// The window at the same place of the other side's column.
static TComponent *Counterpart(int s, int c, int Index)
{
	std::vector<TComponent *> List = ChildrenOf(Column[s][c]);
	return Index >= 0 && Index < (int)List.size() ? List[Index] : NULL;
}

static int RandomIndex(int c)
{
	int Count = (int)ChildrenOf(Column[OURS][c]).size();
	return Count == 0 ? -1 : (int)(Random() % Count);
}

static bool Captured() { return Column[OURS][0]->Capture != NULL || Column[OURS][1]->Capture != NULL; }

// No window may be drawn above or left of the surface (TContentWindow::Draw's
// assertions and TComponent::Draw's "dialog origin outside main window"): a
// dragged window taller than its column goes above it.
static bool CanDraw(int c, int x, int y)
{
	for (TComponent *Child = Column[OURS][c]->FirstChild; Child != NULL; Child = Child->Next) {
		if (x + Child->OffsetX < 0 || y + Child->OffsetY < 0)
			return false;
	}
	return true;
}

static int FrameHeight() { return Skins[133].height + Skins[132].height; }

static int SerialIdentifier = 0;
static int NewIdentifier(int c)
{
	static const int Sub[] = {6, 7, 8, 9};
	if (c == 1)
		return 1 + Random() % 5;
	if (Random() % 2 == 0)
		return Sub[Random() % 4];
	return 0x40 + (SerialIdentifier++ % 16);
}

// A new sub-window, sized like ContentWindows.cpp's creators do, with its
// buttons, icon and title on most.
static TSubWindow *MakeSub(int s, int Identifier, int MinHeight, bool Resizable, bool Background, int Height, int Parts)
{
	TTestBody *Body = NewBody(MinHeight);
	TSubWindow *W = NewSub(s, Identifier, Body, Resizable, Background);
	W->SetSize(Column[s][0]->Width, Height);
	TElement *Close = (Parts & 1) ? new TTestElement : NULL;
	TTestSkinButton *Minimize = (Parts & 2) ? new TTestSkinButton : NULL;
	TElement *Up = (Parts & 4) ? new TTestElement : NULL;
	TElement *Icon = (Parts & 8) ? new TTestElement : NULL;
	TElement *Title = (Parts & 16) ? new TTestElement : NULL;
	if (Close != NULL) {
		Register(Close, "Close");
		if (s == OURS)
			W->SetCloseButton(Close);
		else
			ORIGINAL(At<SetFn>(0x489cf0)(W, NULL, Close));
	}
	if (Minimize != NULL) {
		Register(Minimize, "Min");
		if (s == OURS)
			W->SetMinimizeButton((TSkinButton *)Minimize);
		else
			ORIGINAL(At<SetFn>(0x489ed0)(W, NULL, Minimize));
	}
	if (Up != NULL) {
		Register(Up, "Up");
		if (s == OURS)
			W->SetUpButton(Up);
		else
			ORIGINAL(At<SetFn>(0x489b10)(W, NULL, Up));
	}
	if (Icon != NULL) {
		Register(Icon, "Icon");
		if (s == OURS)
			W->SetIcon(Icon);
		else
			ORIGINAL(At<SetFn>(0x48a0b0)(W, NULL, Icon));
	}
	if (Title != NULL) {
		Register(Title, "Title");
		if (s == OURS)
			W->SetTitle(Title);
		else
			ORIGINAL(At<SetFn>(0x48a2a0)(W, NULL, Title));
	}
	return W;
}

// The image's skin properties get the same values as ours (the oracle only). Each constructor's arguments
// are drawn in the order the x86 oracle evaluated them, right to left (#55: the order of the calls in one
// expression is unspecified; x64 MSVC may take another).
static void RandomSkinProperties()
{
	TSkinProperties &M = SkinProperties;
	TSkinProperties &O = ORIGINAL_OR(Global<TSkinProperties>(0x5ccfc8), M); // the runner: ours twice
	M.Data184 = O.Data184 = 140 + Random() % 80;
	{
		int Blue = Random() % 256, Green = Random() % 256, Red = Random() % 256;
		M.Data188 = O.Data188 = TRGBColor(Red, Green, Blue);
	}
	TSkinProperties194 &A = M.Style194, &B = O.Style194;
	{
		int Height = 8 + Random() % 8, Width = 8 + Random() % 8, y = Random() % 4, x = Random() % 8;
		A.Data00 = B.Data00 = TRect(x, y, Width, Height);
	}
	A.Data10.x = B.Data10.x = -(int)(10 + Random() % 8);
	A.Data10.y = B.Data10.y = Random() % 4;
	A.Data18.x = B.Data18.x = -(int)(22 + Random() % 8);
	A.Data18.y = B.Data18.y = Random() % 4;
	A.Data20.x = B.Data20.x = -(int)(34 + Random() % 8);
	A.Data20.y = B.Data20.y = Random() % 4;
	A.Data28.x = B.Data28.x = 18 + Random() % 6;
	A.Data28.y = B.Data28.y = Random() % 6;
	A.Data28.width = B.Data28.width = -(int)(36 + Random() % 8);
	A.Data28.height = B.Data28.height = 8 + Random() % 6;
	// A minimized window is as high as its frame (as in the game): lower, Shrink
	// fails its assertion; higher, it loops for ever, in both versions.
	A.Data4C = B.Data4C = Skins[133].height + Skins[132].height;
	// The body needs a height of at least 1 in the 19-pixel frame.
	{
		int Height = -(int)(2 + Random() % 3), Width = -(int)(2 + Random() % 4), y = 13 + Random() % 3,
			x = 2 + Random() % 4;
		A.Data50 = B.Data50 = TRect(x, y, Width, Height);
	}
	A.Data60.x = B.Data60.x = Random() % 6;
	A.Data60.y = B.Data60.y = -(int)(14 + Random() % 4);
	A.Data60.width = B.Data60.width = 10 + Random() % 6;
	A.Data60.height = B.Data60.height = 10 + Random() % 6;
	A.Data70.x = B.Data70.x = 0;
	A.Data70.y = B.Data70.y = -(int)(3 + Random() % 3);
	A.Data70.width = B.Data70.width = 0;
	A.Data70.height = B.Data70.height = 0;
}

// --- Operations ---------------------------------------------------------------------------

static int KindCount[24];

static void Operation()
{
	Kind = Random() % 24;
	KindCount[Kind]++;
	char What[64];
	sprintf(What, "operation %d", Kind);
	bool Busy = Captured();
	switch (Kind) {
	case 0:
	case 1: {
		// A new sub-window, appended by Rearrange(16) like MainWindow.cpp does.
		if (Busy)
			return;
		int Identifier = NewIdentifier(0), MinHeight = 1 + Random() % 60;
		bool Resizable = Random() % 2 != 0, Background = Random() % 2 != 0;
		int Height = FrameHeight() + MinHeight + Random() % 80, Parts = Random() % 32;
		Both([&](int s) {
			TSubWindow *W = MakeSub(s, Identifier, MinHeight, Resizable, Background, Height, Parts);
			Column[s][0]->Rearrange(16, W);
		});
		break;
	}
	case 2: {
		// A new window through the child functions, then Rearrange(32).
		if (Busy)
			return;
		int c = Random() % 2, How = Random() % 3, Index = RandomIndex(c);
		if (How == 2 && Index < 0)
			How = 0;
		int Identifier = NewIdentifier(c), MinHeight = 1 + Random() % 40;
		bool Resizable = Random() % 2 != 0, Background = Random() % 2 != 0;
		int Height = c == 0 ? FrameHeight() + MinHeight + Random() % 60 : 10 + Random() % 120, Parts = Random() % 32;
		Both([&](int s) {
			TComponent *W;
			if (c == 0) {
				W = MakeSub(s, Identifier, MinHeight, Resizable, Background, Height, Parts);
			} else {
				W = NewContent(s, Identifier, Background);
				W->SetSize(Column[s][1]->Width, Height);
			}
			if (How == 0)
				Column[s][c]->InsertChild(W);
			else if (How == 1)
				Column[s][c]->AppendChild(W);
			else
				Column[s][c]->InsertChildAfter(Counterpart(s, c, Index), W);
			Column[s][c]->Rearrange(32, NULL);
		});
		break;
	}
	case 3: {
		if (Busy)
			return;
		int c = Random() % 2, Index = RandomIndex(c);
		if (Index < 0)
			return;
		Both([&](int s) { Column[s][c]->DeleteChild(Counterpart(s, c, Index)); });
		break;
	}
	case 4: {
		// The column's size (strategy 8 deletes the windows that do not fit).
		if (Busy)
			return;
		int c = Random() % 2, Width = 70 + Random() % 180, Height = Random() % 3 == 0 ? (int)(Random() % 120) : 100 + (int)(Random() % 600);
		Both([&](int s) { Column[s][c]->SetSize(Width, Height); });
		break;
	}
	case 5: {
		int c = Random() % 2, x = Random() % 30, y = Random() % 30;
		if (!CanDraw(c, x, y))
			return;
		int Surface = Random() % 3;
		Both([&](int s) { Column[s][c]->Draw(Surface, x, y); });
		break;
	}
	case 6:
	case 7:
	case 8: {
		// Mouse events through a column: press, move, release.
		int c = Random() % 2, Action = Kind - 6;
		TDialogWindow *O = Column[OURS][c];
		if (Action == 0 && Busy)
			return;
		int x = (int)(Random() % (O->Width + 20)) - 10, y = (int)(Random() % (O->Height + 40)) - 20;
		bool Results[2];
		Both([&](int s) {
			TDialogWindow *D = Column[s][c];
			if (Action == 0)
				Results[s] = D->LeftButtonDown(x, y, 0);
			else if (Action == 1)
				Results[s] = D->MouseMove(x, y);
			else
				Results[s] = D->LeftButtonUp(x, y, 0);
		});
		golden::Output("mouse result", Results[ORIGINAL]);
		CHECK(Results[OURS] == Results[ORIGINAL], "DialogWindow mouse %d result %d, original %d", Action,
			Results[OURS], Results[ORIGINAL]);
		break;
	}
	case 9: {
		// A double click (minimizes or restores the sub-window under it).
		if (Busy)
			return;
		TDialogWindow *O = Column[OURS][0];
		int x = Random() % (O->Width + 1), y = Random() % (O->Height + 1);
		bool Results[2];
		Both([&](int s) { Results[s] = Column[s][0]->LeftButtonDoubleClick(x, y, 0); });
		golden::Output("double click result", Results[ORIGINAL]);
		CHECK(Results[OURS] == Results[ORIGINAL], "DialogWindow double click result %d, original %d", Results[OURS],
			Results[ORIGINAL]);
		break;
	}
	case 10: {
		// Events 9 (minimize, maximize) and 10 (close) and others.
		if (Busy)
			return;
		int Index = RandomIndex(0);
		if (Index < 0)
			return;
		static const int Events[] = {9, 9, 10, 0, 8, 11};
		int Event = Events[Random() % 6];
		Both([&](int s) { Counterpart(s, 0, Index)->HandleEvent(Event); });
		break;
	}
	case 11: {
		// Minimize or Maximize directly, as the state allows.
		if (Busy)
			return;
		int Index = RandomIndex(0);
		if (Index < 0)
			return;
		bool Minimized = ((TSubWindow *)Counterpart(OURS, 0, Index))->IsMinimized;
		Both([&](int s) {
			TSubWindow *W = (TSubWindow *)Counterpart(s, 0, Index);
			if (Minimized)
				W->Maximize();
			else
				W->Minimize();
		});
		break;
	}
	case 12: {
		// A sub-window's size (strategies 2, 4 and 32, then 8).
		if (Busy)
			return;
		int Index = RandomIndex(0);
		if (Index < 0)
			return;
		int Height = Random() % 400;
		Both([&](int s) {
			TComponent *W = Counterpart(s, 0, Index);
			W->SetSize(W->Width, Height);
		});
		break;
	}
	case 13: {
		// A plain content window's size, then Rearrange 2 or 4 like a side-bar window.
		if (Busy)
			return;
		int Index = RandomIndex(1);
		if (Index < 0)
			return;
		int Height = Random() % 200;
		bool Grew = Height > Counterpart(OURS, 1, Index)->Height;
		Both([&](int s) {
			TComponent *W = Counterpart(s, 1, Index);
			W->SetSize(W->Width, Height);
			Column[s][1]->Rearrange(Grew ? 2 : 32, W);
		});
		break;
	}
	case 14: {
		// The setters replace a part.
		if (Busy)
			return;
		int Index = RandomIndex(0);
		if (Index < 0)
			return;
		int Which = Random() % 5;
		Both([&](int s) {
			TSubWindow *W = (TSubWindow *)Counterpart(s, 0, Index);
			ORIGINAL(static const uint32 Setters[] = {0x489b10, 0x489cf0, 0x489ed0, 0x48a0b0, 0x48a2a0});
			static const char *Kinds[] = {"Up", "Close", "Min", "Icon", "Title"};
			TComponent *Part = Which == 2 ? (TComponent *)new TTestSkinButton : (TComponent *)new TTestElement;
			Register(Part, Kinds[Which]);
			if (s == ORIGINAL) {
				ORIGINAL(At<SetFn>(Setters[Which])(W, NULL, Part));
			} else if (Which == 0) {
				W->SetUpButton(Part);
			} else if (Which == 1) {
				W->SetCloseButton(Part);
			} else if (Which == 2) {
				W->SetMinimizeButton((TSkinButton *)Part);
			} else if (Which == 3) {
				W->SetIcon(Part);
			} else {
				W->SetTitle(Part);
			}
		});
		break;
	}
	case 15: {
		// The "up" button comes and goes (the container body's 0x42e4e0 does this).
		if (Busy)
			return;
		int Index = RandomIndex(0);
		if (Index < 0)
			return;
		bool Show = Random() % 2 != 0;
		Both([&](int s) {
			TSubWindow *W = (TSubWindow *)Counterpart(s, 0, Index);
			if (s == OURS) {
				if (Show)
					W->ShowUpButton();
				else
					W->HideUpButton();
			} else {
				ORIGINAL(At<VoidFn>(Show ? 0x4897d0 : 0x489970)(W, NULL));
			}
		});
		break;
	}
	case 16: {
		// Rearrange with a strategy that keeps the windows.
		if (Busy)
			return;
		int c = Random() % 2, Index = RandomIndex(c);
		static const int Strategies[] = {1, 2, 4, 8, 32};
		int Strategy = Strategies[Random() % 5];
		if ((Strategy == 4 || Strategy == 1) && Index < 0)
			return;
		if (c == 1 && Strategy == 4)
			Strategy = 32;
		Both([&](int s) {
			TComponent *Work = Index >= 0 && Strategy != 8 && Strategy != 32 ? Counterpart(s, c, Index) : NULL;
			Column[s][c]->Rearrange(Strategy, Work);
			if (Strategy == 1)
				Column[s][c]->Rearrange(1, NULL);
		});
		break;
	}
	case 17: {
		// The queries.
		int c = Random() % 2, Identifier = NewIdentifier(c);
		int Ids[2], Free[2], MinH[2], MinW[2], Frame[2];
		Both([&](int s) {
			TDialogWindow *D = Column[s][c];
			TContentWindow *W;
			if (s == OURS) {
				W = D->GetContentWindow(Identifier);
				Free[s] = D->GetFreeHeight();
				Frame[s] = TSubWindow::GetFrameHeight();
			} else {
#if GOLDEN_ORACLE
				W = At<FindFn>(0x486400)(D, NULL, Identifier);
				Free[s] = At<GetFn>(0x487060)(D, NULL);
				Frame[s] = At<int(__cdecl *)()>(0x485670)();
#else
				abort();
#endif
			}
			Ids[s] = W == NULL ? -1 : Info[s][W].Serial;
			MinH[s] = D->getMinimumHeight();
			MinW[s] = D->getMinimumWidth();
		});
		golden::Output("query id", Ids[ORIGINAL]);
		golden::Output("query free height", Free[ORIGINAL]);
		golden::Output("query minimum height", MinH[ORIGINAL]);
		golden::Output("query minimum width", MinW[ORIGINAL]);
		golden::Output("query frame height", Frame[ORIGINAL]);
		CHECK(Ids[OURS] == Ids[ORIGINAL] && Free[OURS] == Free[ORIGINAL] && MinH[OURS] == MinH[ORIGINAL]
				&& MinW[OURS] == MinW[ORIGINAL] && Frame[OURS] == Frame[ORIGINAL],
			"DialogWindow queries: %d %d %d %d %d, original %d %d %d %d %d", Ids[OURS], Free[OURS], MinH[OURS],
			MinW[OURS], Frame[OURS], Ids[ORIGINAL], Free[ORIGINAL], MinH[ORIGINAL], MinW[ORIGINAL], Frame[ORIGINAL]);
		for (TComponent *Child = Column[OURS][0]->FirstChild; Child != NULL; Child = Child->Next) {
			int Index = 0;
			for (TComponent *C = Column[OURS][0]->FirstChild; C != Child; C = C->Next)
				Index++;
			int A = Child->getMinimumHeight();
			int B = ORIGINAL_VALUE("sub minimum height", int, Counterpart(ORIGINAL, 0, Index)->getMinimumHeight());
			CHECK(A == B, "TSubWindow::getMinimumHeight %d, original %d", A, B);
		}
		break;
	}
	case 18: {
		int c = Random() % 2;
		Both([&](int s) {
			if (s == OURS)
				Column[s][c]->StoreOpenDialogs();
			else
				ORIGINAL(At<VoidFn>(0x4865b0)(Column[s][c], NULL));
		});
		break;
	}
	case 19: {
		// Arrange of the column or of one of its windows.
		int c = Random() % 2, Index = Random() % 2 == 0 ? -1 : RandomIndex(c);
		Both([&](int s) {
			if (Index < 0)
				Column[s][c]->Arrange();
			else
				Counterpart(s, c, Index)->Arrange();
		});
		break;
	}
	case 20: {
		// A whole drag: press on a window's frame, moves, release.
		if (Busy)
			return;
		int c = Random() % 2, Index = RandomIndex(c);
		if (Index < 0)
			return;
		TComponent *W = Counterpart(OURS, c, Index);
		int x = W->OffsetX + Random() % (W->Width > 0 ? W->Width : 1), y = W->OffsetY + Random() % 4;
		int Moves = 1 + Random() % 4;
		int dy[4];
		for (int i = 0; i < 4; i++)
			dy[i] = (int)(Random() % 160) - 80;
		Both([&](int s) {
			TDialogWindow *D = Column[s][c];
			D->LeftButtonDown(x, y, 0);
			int yy = y;
			for (int i = 0; i < Moves; i++) {
				yy += dy[i];
				D->MouseMove(x, yy);
			}
		});
		Compare("drag moves");
		if (Column[OURS][c]->Capture != NULL) {
			int c2 = c;
			int Surface = 1;
			if (CanDraw(c2, 5, 5))
				Both([&](int s) { Column[s][c2]->Draw(Surface, 5, 5); });
			Compare("drag draw");
		}
		Both([&](int s) { Column[s][c]->LeftButtonUp(x, y, 0); });
		break;
	}
	case 21: {
		// MouseMove without a capture (the resizers get it first).
		if (Busy)
			return;
		TDialogWindow *O = Column[OURS][0];
		int x = Random() % (O->Width + 1), y = Random() % (O->Height + 1);
		bool Results[2];
		Both([&](int s) { Results[s] = Column[s][0]->MouseMove(x, y); });
		golden::Output("move result", Results[ORIGINAL]);
		CHECK(Results[OURS] == Results[ORIGINAL], "DialogWindow move result %d, original %d", Results[OURS],
			Results[ORIGINAL]);
		break;
	}
	case 22: {
		RandomSkinProperties();
		int c = Random() % 2;
		Both([&](int s) { Column[s][c]->Rearrange(32, NULL); });
		break;
	}
	case 23: {
		// Releases that come without a press (and a release of a stale capture).
		int c = Random() % 2;
		int x = Random() % 100, y = Random() % 300;
		Both([&](int s) { Column[s][c]->LeftButtonUp(x, y, 0); });
		break;
	}
	}
	Compare(What);
}

// --- Error paths (issue #34) ----------------------------------------------------------

// One call on both sides (Side set for the fakes and the throwing test
// objects), the exceptions compared, then the logs and both trees. A
// Rearrange that threw leaves Busy set (both sides), a drag that threw its
// Dragged window: cleared for the next call (a later deletion would leave
// Dragged dangling, and the next strategy 1 would call through freed memory).
template <class F> static void ErrorCall(TErrorRound &R, const char *What, F Call)
{
	GOLDEN_ROUND_COMPARE(R, What, (Side = OURS, Call(OURS)), (Side = ORIGINAL, Call(ORIGINAL)));
	for (int s = 0; s < SIDES; s++)
		for (int c = 0; c < 2; c++) {
			Column[s][c]->Busy = false;
			Column[s][c]->UpdateFreeHeight = true;
			if (Column[s][c]->Capture == NULL)
				Column[s][c]->Dragged = NULL;
		}
	Compare(What);
}

// Every TASSERT/TERROR of DialogWindow.cpp that the columns can reach, and
// the TEND blocks: the bodies, elements and fakes throw (ThrowMask, one kind
// of method per batch), the column functions, the sub-window functions and
// the setters run on both sides, then the assertions get bad arguments.
static void TestDialogWindowErrors()
{
	TErrorRound R("DialogWindow");
	static const int Masks[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 511};
	char What[96];
	for (int Round = 0; Round < 30; Round++) {
		RandomSkinProperties();
		int Width = 70 + Random() % 180, Height[2] = {100 + (int)(Random() % 600), 100 + (int)(Random() % 400)};
		Both([&](int s) {
			for (int c = 0; c < 2; c++) {
				Column[s][c] = NewColumn(s);
				Column[s][c]->SetSize(Width, Height[c]);
			}
		});
		ThrowMask = 0;
		for (int i = 0; i < 40; i++)
			Operation();
		for (int c = 0; c < 2; c++)
			if (Column[OURS][c]->Capture != NULL)
				Both([&](int s) { Column[s][c]->LeftButtonUp(0, 0, 0); });
		Compare("errors: setup");
		ThrowMask = Masks[Round % 10];
		int Mask = ThrowMask;
		for (int c = 0; c < 2; c++) {
			TDialogWindow *O = Column[OURS][c];
			int x = (int)(Random() % (O->Width + 20)) - 10, y = (int)(Random() % (O->Height + 40)) - 20;
			int Dx = Random() % 30, Dy = Random() % 30;
			snprintf(What, sizeof(What), "column %d mask %d", c, Mask);
			if (CanDraw(c, Dx, Dy))
				ErrorCall(R, "TDialogWindow::Draw", [&](int s) { Column[s][c]->Draw(1, Dx, Dy); });
			ErrorCall(R, "TDialogWindow::LeftButtonDown", [&](int s) { Column[s][c]->LeftButtonDown(x, y, 0); });
			ErrorCall(R, "TDialogWindow::MouseMove", [&](int s) { Column[s][c]->MouseMove(x + 3, y + 7); });
			ErrorCall(R, "TDialogWindow::LeftButtonUp", [&](int s) { Column[s][c]->LeftButtonUp(x + 3, y + 7, 0); });
			ThrowMask = 0;
			if (Column[OURS][c]->Capture != NULL)
				Both([&](int s) { Column[s][c]->LeftButtonUp(0, 0, 0); });
			Compare("errors: release");
			ThrowMask = Mask;
			ErrorCall(R, "TDialogWindow::LeftButtonDoubleClick",
				[&](int s) { Column[s][c]->LeftButtonDoubleClick(x, y, 0); });
			ErrorCall(R, "TDialogWindow::Arrange", [&](int s) { Column[s][c]->Arrange(); });
			ErrorCall(R, "TDialogWindow::getMinimumHeight", [&](int s) { Column[s][c]->getMinimumHeight(); });
			int Wd = 70 + Random() % 180, Ht = Random() % 3 == 0 ? (int)(Random() % 120) : 100 + (int)(Random() % 600);
			ErrorCall(R, "TDialogWindow::SetSize", [&](int s) { Column[s][c]->SetSize(Wd, Ht); });
			static const int Strategies[] = {1, 2, 4, 8, 16, 32};
			int Index = RandomIndex(c), Strategy = Strategies[Random() % 6];
			if (Index >= 0 && Strategy != 16)
				ErrorCall(R, "TDialogWindow::Rearrange", [&](int s) { Column[s][c]->Rearrange(Strategy, Counterpart(s, c, Index)); });
			ErrorCall(R, "TDialogWindow::StoreOpenDialogs", [&](int s) {
				if (s == OURS) Column[s][c]->StoreOpenDialogs(); else ORIGINAL(At<VoidFn>(0x4865b0)(Column[s][c], NULL)); });
			ErrorCall(R, "TDialogWindow::GetFreeHeight", [&](int s) {
				if (s == OURS) Column[s][c]->GetFreeHeight(); else ORIGINAL(At<GetFn>(0x487060)(Column[s][c], NULL)); });
			ErrorCall(R, "TDialogWindow::PositionChildren", [&](int s) {
				if (s == OURS) Column[s][c]->PositionChildren(false); else ORIGINAL(At<PositionFn>(0x486760)(Column[s][c], NULL, false)); });
			ErrorCall(R, "TDialogWindow::FitChildren", [&](int s) {
				if (s == OURS) Column[s][c]->FitChildren(); else ORIGINAL(At<VoidFn>(0x486930)(Column[s][c], NULL)); });
			ErrorCall(R, "TDialogWindow::Shrink", [&](int s) {
				if (s == OURS) Column[s][c]->Shrink(); else ORIGINAL(At<BoolFn>(0x48a990)(Column[s][c], NULL)); });
			Index = RandomIndex(c);
			if (Index >= 0) {
				ErrorCall(R, "TDialogWindow::Grow", [&](int s) {
					TComponent *W = Counterpart(s, c, Index);
					if (s == OURS) Column[s][c]->Grow(W); else ORIGINAL(At<SetFn>(0x48acb0)(Column[s][c], NULL, W)); });
				Index = RandomIndex(c);
				ErrorCall(R, "TDialogWindow::DeleteChild", [&](int s) { Column[s][c]->DeleteChild(Counterpart(s, c, Index)); });
			}
			// The child functions with a new window (they arrange the column).
			int Identifier = NewIdentifier(c), MinHeight = 1 + Random() % 40, How = Random() % 3;
			bool Resizable = Random() % 2 != 0, Background = Random() % 2 != 0;
			int NewHeight = c == 0 ? FrameHeight() + MinHeight + Random() % 60 : 10 + Random() % 120, Parts = Random() % 32;
			Index = RandomIndex(c);
			if (How == 2 && Index < 0)
				How = 0;
			ThrowMask = 0;
			TComponent *New[2];
			Both([&](int s) {
				if (c == 0) {
					New[s] = MakeSub(s, Identifier, MinHeight, Resizable, Background, NewHeight, Parts);
				} else {
					New[s] = NewContent(s, Identifier, Background);
					New[s]->SetSize(Column[s][1]->Width, NewHeight);
				}
			});
			Log[OURS].clear();
			Log[ORIGINAL].clear();
			ThrowMask = Mask;
			ErrorCall(R, "TDialogWindow child functions", [&](int s) {
				if (How == 0)
					Column[s][c]->InsertChild(New[s]);
				else if (How == 1)
					Column[s][c]->AppendChild(New[s]);
				else
					Column[s][c]->InsertChildAfter(Counterpart(s, c, Index), New[s]);
			});
		}
		// The sub-windows of column 0: their own functions and the setters.
		std::vector<TComponent *> Subs = ChildrenOf(Column[OURS][0]);
		for (int i = 0; i < (int)Subs.size() && i < 3; i++) {
			if (Column[OURS][0]->Capture != NULL)
				break;
			auto Sub = [&](int s) { return (TSubWindow *)Counterpart(s, 0, i); };
			// The original's answer is recorded (in the oracle each call of Gone evaluates it unless ours is gone).
			auto Gone = [&]() {
				return Sub(OURS) == NULL || ORIGINAL_VALUE("gone", bool, Sub(ORIGINAL) == NULL);
			};
			if (Gone())
				break;
			int x = Random() % (Sub(OURS)->Width + 1), y = Random() % (Sub(OURS)->Height + 1);
			ErrorCall(R, "TSubWindow::Arrange", [&](int s) { Sub(s)->Arrange(); });
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::SetSize", [&](int s) { Sub(s)->SetSize(Sub(s)->Width, Sub(s)->Height + 3); });
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::MouseMove", [&](int s) { Sub(s)->MouseMove(x, y); });
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::LeftButtonDown", [&](int s) { Sub(s)->LeftButtonDown(x, y, 0); });
			ThrowMask = 0;
			if (Gone())
				break;
			if (Column[OURS][0]->Capture != NULL || Sub(OURS)->Capture != NULL)
				Both([&](int s) { Sub(s)->LeftButtonUp(x, y, 0); Column[s][0]->LeftButtonUp(0, 0, 0); });
			Compare("errors: release");
			ThrowMask = Mask;
			if (Column[OURS][0]->Capture != NULL)
				break;
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::LeftButtonDoubleClick", [&](int s) { Sub(s)->LeftButtonDoubleClick(x, y, 0); });
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::HandleEvent", [&](int s) { Sub(s)->HandleEvent(9); });
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::Minimize/Maximize", [&](int s) {
				if (Sub(s)->IsMinimized) Sub(s)->Maximize(); else Sub(s)->Minimize(); });
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::ShowUpButton", [&](int s) {
				if (s == OURS) Sub(s)->ShowUpButton(); else ORIGINAL(At<VoidFn>(0x4897d0)(Sub(s), NULL)); });
			if (Gone())
				break;
			ErrorCall(R, "TSubWindow::HideUpButton", [&](int s) {
				if (s == OURS) Sub(s)->HideUpButton(); else ORIGINAL(At<VoidFn>(0x489970)(Sub(s), NULL)); });
			// The setters with a new part (the old one goes).
			if (Gone())
				break;
			int Which = Random() % 5;
			ORIGINAL(static const uint32 Setters[] = {0x489b10, 0x489cf0, 0x489ed0, 0x48a0b0, 0x48a2a0});
			ThrowMask = 0;
			TComponent *Part = Which == 2 ? (TComponent *)new TTestSkinButton : (TComponent *)new TTestElement;
			Side = OURS;
			Register(Part, "Part");
			TComponent *Part2 = nullptr; // the original's part (the oracle only)
#if GOLDEN_ORACLE
			Part2 = Which == 2 ? (TComponent *)new TTestSkinButton : (TComponent *)new TTestElement;
			Side = ORIGINAL;
			Register(Part2, "Part");
#endif
			Part->SetSize(5, 5);
			ORIGINAL(Part2->SetSize(5, 5));
			ThrowMask = Mask;
			ErrorCall(R, "TSubWindow setter", [&](int s) {
				TComponent *P = s == OURS ? Part : Part2;
				if (s == ORIGINAL)
					ORIGINAL(At<SetFn>(Setters[Which])(Sub(s), NULL, P));
				else if (Which == 0)
					Sub(s)->SetUpButton(P);
				else if (Which == 1)
					Sub(s)->SetCloseButton(P);
				else if (Which == 2)
					Sub(s)->SetMinimizeButton((TSkinButton *)P);
				else if (Which == 3)
					Sub(s)->SetIcon(P);
				else
					Sub(s)->SetTitle(P);
			});
		}
		ThrowMask = 0;
		// The assertions with bad arguments.
		for (int c = 0; c < 2; c++) {
			static const int Bad[] = {4, 16, 0, 3, 5, 64, 999, -1};
			int Strategy = Bad[Random() % 8];
			snprintf(What, sizeof(What), "TDialogWindow::Rearrange(%d, NULL)", Strategy);
			if (Column[OURS][c]->Capture == NULL)
				ErrorCall(R, What, [&](int s) { Column[s][c]->Rearrange(Strategy, NULL); });
			// DragChild of a window that is not a child.
			TTestElement *Stranger = new TTestElement;
			for (Side = 0; Side < SIDES; Side++)
				Register(Stranger, "Stranger");
			ErrorCall(R, "TDialogWindow::DragChild stranger", [&](int s) {
				if (s == OURS) Column[s][c]->DragChild(Stranger); else ORIGINAL(At<SetFn>(0x486b30)(Column[s][c], NULL, Stranger)); });
			delete Stranger;
			// Shrink with a child below its minimum height and no room.
			std::vector<TComponent *> Children = ChildrenOf(Column[OURS][c]);
			if (!Children.empty()) {
				int Index = (int)Children.size() - 1;
				int SavedHeight[2], SavedColumn[2];
				for (int s = 0; s < SIDES; s++) {
					TComponent *W = Counterpart(s, c, Index);
					SavedHeight[s] = W->Height;
					SavedColumn[s] = Column[s][c]->Height;
					W->Height = 0;
					Column[s][c]->Height = 1;
				}
				ErrorCall(R, "TDialogWindow::Shrink below the minimum", [&](int s) {
					if (s == OURS) Column[s][c]->Shrink(); else ORIGINAL(At<BoolFn>(0x48a990)(Column[s][c], NULL)); });
				for (int s = 0; s < SIDES; s++) {
					Counterpart(s, c, Index)->Height = SavedHeight[s];
					Column[s][c]->Height = SavedColumn[s];
				}
			}
			// TContentWindow::Draw left of or above the surface.
			Children = ChildrenOf(Column[OURS][c]);
			if (!Children.empty()) {
				int Index = (int)(Random() % Children.size()), Neg = -1 - (int)(Random() % 30), Which = Random() % 2;
				ErrorCall(R, "TContentWindow::Draw outside", [&](int s) {
					TComponent *W = Counterpart(s, c, Index);
					if (Which == 0) W->Draw(1, Neg - W->OffsetX, 0); else W->Draw(1, 0, Neg - W->OffsetY); });
			}
		}
		// The sub-window's assertions: no body, Minimize twice, Maximize unminimized.
		Subs = ChildrenOf(Column[OURS][0]);
		if (!Subs.empty() && Column[OURS][0]->Capture == NULL) {
			int i = (int)(Random() % Subs.size());
			auto Sub = [&](int s) { return (TSubWindow *)Counterpart(s, 0, i); };
			TSubWindowBody *Saved[2] = {Sub(OURS)->Body, ORIGINAL_OR(Sub(ORIGINAL)->Body, nullptr)};
			ErrorCall(R, "TSubWindow::Arrange without a body", [&](int s) { Sub(s)->Body = NULL; Sub(s)->Arrange(); });
			for (int s = 0; s < SIDES; s++)
				Sub(s)->Body = Saved[s];
			// The state set directly (calling the other one first would
			// rearrange the column): each assertion comes before the try block.
			for (int Minimized = 0; Minimized < 2; Minimized++) {
				bool SavedMinimized[2] = {Sub(OURS)->IsMinimized, ORIGINAL_OR(Sub(ORIGINAL)->IsMinimized, false)};
				ErrorCall(R, Minimized ? "TSubWindow::Minimize minimized" : "TSubWindow::Maximize not minimized",
					[&](int s) {
						Sub(s)->IsMinimized = Minimized != 0;
						if (Minimized) Sub(s)->Minimize();
						else Sub(s)->Maximize();
					});
				for (int s = 0; s < SIDES; s++)
					Sub(s)->IsMinimized = SavedMinimized[s];
			}
		}
		int Identifier = 0x40 + (int)(Random() % 16);
		GOLDEN_ROUND_COMPARE(R, "TSubWindow without a body", delete new TSubWindow(Identifier, NULL, false, false),
			At<SubCtorFn>(0x487e80)(operator new(sizeof(TSubWindow)), NULL, Identifier, NULL, false, false));
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		// The columns go, as in the test.
		for (int c = 0; c < 2; c++)
			if (Column[OURS][c]->Capture != NULL)
				Both([&](int s) { Column[s][c]->LeftButtonUp(0, 0, 0); });
		Both([&](int s) {
			for (int c = 0; c < 2; c++) {
				delete Column[s][c];
				Column[s][c] = NULL;
			}
		});
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		Info[OURS].clear();
		Info[ORIGINAL].clear();
		Serials[OURS] = Serials[ORIGINAL] = 0;
	}
	R.Finish();
}

} // namespace DialogWindowTest

#if !GOLDEN_ORACLE
// The runner's operator delete: while the test runs, the memory it frees stays as it was until the end of the
// test. Our TDialogWindow::SetSize (DialogWindow.cpp, as the original's 0x4857d0) sizes each child in a loop
// that goes on through the child's Next; a sub-window's SetSize can rearrange the column and delete children,
// the loop then reads a deleted child. The x86 oracle's heap leaves a freed object's members as they were, and
// so does the x64 Release runner's, mostly; the Debug CRT fills freed memory. Keeping it makes the runner read
// what the oracle read, in Release and Debug alike.
// The list is malloc's own (a vector's reallocation would call this operator delete).
namespace {
bool KeepFreed = false;
void **Freed = nullptr;
size_t NumberOfFreed = 0, FreedCapacity = 0;
void Free(void *Memory)
{
	if (!KeepFreed || Memory == nullptr) {
		free(Memory);
		return;
	}
	if (NumberOfFreed == FreedCapacity) {
		FreedCapacity = FreedCapacity == 0 ? 4096 : 2 * FreedCapacity;
		void **List = (void **)realloc(Freed, FreedCapacity * sizeof(void *));
		if (List == nullptr) {
			printf("golden: out of memory\n");
			exit(1);
		}
		Freed = List;
	}
	Freed[NumberOfFreed++] = Memory;
}
void FreeKept()
{
	KeepFreed = false;
	for (size_t i = 0; i < NumberOfFreed; i++)
		free(Freed[i]);
	free(Freed);
	Freed = nullptr;
	NumberOfFreed = FreedCapacity = 0;
}
} // namespace
void operator delete(void *Memory) noexcept { Free(Memory); }
void operator delete(void *Memory, size_t) noexcept { Free(Memory); }
#endif

void TestDialogWindow()
{
	using namespace DialogWindowTest;
	int Before = Checks, BeforeFailures = Failures;
#if !GOLDEN_ORACLE
	KeepFreed = true;
#endif
	ORIGINAL(PatchImage());
	InitSkins();
	ORIGINAL(Global<int>(0x71c450) = NumberOfSkins; Global<TSkin *>(0x71c454) = Skins.get());
	InitSkinProperties();
	ORIGINAL(At<void(__cdecl *)()>(0x498160)());
	LastDump.clear();
	DialogSurface = new TDialogSurface;
	PatchCallees();
	DialogFakes.Body = HookBody;
	DialogFakes.BodyAccepts = HookBodyAccepts;
	DialogFakes.NewResizer = HookNewResizer;
	DialogFakes.SetSkins = HookSetSkins;

	// The vtables: the image's classes and ours (from instances), the fake resizer.
	{
		TDWTestResizer *R = new TDWTestResizer;
		FakeResizerVtable = VtableOf(R);
		delete R;
		TDialogWindow *D = new TDialogWindow;
		TContentWindow *C = new TContentWindow(1, false);
		TTestBody *B = new TTestBody;
		B->MinHeight = 5;
		TSubWindow *W = new TSubWindow(6, B, false, false);
		ClassOf[OURS][VtableOf(D)] = CLASS_COLUMN;
		ClassOf[OURS][VtableOf(C)] = CLASS_CONTENT;
		ClassOf[OURS][VtableOf(W)] = CLASS_SUB;
		delete D;
		delete C;
		delete W;
		ORIGINAL(ClassOf[ORIGINAL][(void *)(uintptr_t)0x563f80] = CLASS_COLUMN;
			ClassOf[ORIGINAL][(void *)(uintptr_t)0x563e38] = CLASS_CONTENT;
			ClassOf[ORIGINAL][(void *)(uintptr_t)0x563ed8] = CLASS_SUB);
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();
	Info[OURS].clear();
	Info[ORIGINAL].clear();
	Serials[OURS] = Serials[ORIGINAL] = 0;

	for (int Round = 0; Round < 8; Round++) {
		RandomSkinProperties();
		int Width = 70 + Random() % 180, Height[2] = {100 + (int)(Random() % 600), 100 + (int)(Random() % 400)};
		Both([&](int s) {
			for (int c = 0; c < 2; c++) {
				Column[s][c] = NewColumn(s);
				Column[s][c]->SetSize(Width, Height[c]);
			}
		});
		Compare("columns");
		for (int i = 0; i < 1500; i++)
			Operation();
		// A capture left by the last drag goes with a release.
		for (int c = 0; c < 2; c++) {
			if (Column[OURS][c]->Capture != NULL)
				Both([&](int s) { Column[s][c]->LeftButtonUp(0, 0, 0); });
		}
		Compare("release");
		// The columns go, with their windows (the sub-windows store their sizes).
		Both([&](int s) {
			for (int c = 0; c < 2; c++) {
				delete Column[s][c];
				Column[s][c] = NULL;
			}
		});
		OutputOriginalLog();
		Checks++;
		if (Log[OURS] != Log[ORIGINAL]) {
			Failures++;
			if (Failures <= FailuresPrinted)
				printf("FAIL DialogWindow destructors: logs differ\n  ours:     %.800s\n  original: %.800s\n",
					Log[OURS].c_str(), Log[ORIGINAL].c_str());
		}
		Log[OURS].clear();
		Log[ORIGINAL].clear();
		Info[OURS].clear();
		Info[ORIGINAL].clear();
		Serials[OURS] = Serials[ORIGINAL] = 0;
	}

	int RoundChecks = Checks, SavedKinds[24];
	memcpy(SavedKinds, KindCount, sizeof(SavedKinds));
	TestDialogWindowErrors();
	memcpy(KindCount, SavedKinds, sizeof(SavedKinds));
	RoundChecks = Checks - RoundChecks;
	DialogFakes = TDialogFakes();
	RestoreAll();
	delete DialogSurface;
	ExitSkins();
	ORIGINAL(Global<TSkin *>(0x71c454) = NULL);
#if !GOLDEN_ORACLE
	FreeKept();
#endif
	int Least = KindCount[0];
	for (int i = 1; i < 24; i++)
		Least = KindCount[i] < Least ? KindCount[i] : Least;
	printf("DialogWindow: %d checks, %d failures (each of the 24 operations at least %d times)\n",
		Checks - Before - RoundChecks, Failures - BeforeFailures, Least);
}
