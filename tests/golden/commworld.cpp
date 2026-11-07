// The shared fake world of communication_tests.cpp and tools/difftest/controltest.cpp
// (issue #36; a golden-corpus world since issue #30); see commworld.h.

#include "commworld.h"
#include "golden_patch.h"

#include "Communication.h"
#include "ConsoleWindow.h"
#include "ContextMenus.h"
#include "Control.h"
#include "Dialogs.h"
#include "Help.h"
#include "Inventory.h"
#include "MainWindow.h"
#include "Map.h"
#include "Network.h"
#include "Preferences.h"
#include "Script.h"
#include "Skins.h"
#include "Status.h"
#include "TRSA.h"
#include "graphics.h"
#include "sysinc.h"
#include <map>
#include <new>
#include <stdarg.h>
#include <malloc.h>

#ifdef _MSC_VER
#pragma warning(disable : 4100 4702) // the fakes' unused parameters; the surface traps' returns after abort()
#endif

int Side = OURS;
const char *TestName = "Communication";
std::string Log[2];
int FailAt = 0;
int Ticks[2];
uint32 Salt = 1;
TWorld World[2];
bool TraceCalls = false;
bool TraceBefore = false;
bool LiveLog = false;
FILE *LogFile = NULL;
bool RecordComparisons = false;
bool RetireChangeEngine = false;
bool ForceReach = false;
bool DeferRetiredEnd = false;
bool RetiredPending = false;
bool LastRetired = false;
void (*AfterRetired)() = nullptr;

// --- Logs ------------------------------------------------------------------------

static void VLogf(const char *Format, va_list Args)
{
	char Buffer[2048];
	int n = _vsnprintf(Buffer, sizeof(Buffer) - 1, Format, Args);
	if (n < 0)
		n = sizeof(Buffer) - 1;
	Buffer[n] = 0;
	Log[Side] += Buffer;
	Log[Side] += ';';
	if (LiveLog) {
		printf("  %s %s\n", Side == IMAGE ? "original" : "ours", Buffer);
		fflush(stdout);
	}
}

void Logf(const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	VLogf(Format, Args);
	va_end(Args);
}

void Call(const char *Format, ...)
{
	Tick();
	va_list Args;
	va_start(Args, Format);
	VLogf(Format, Args);
	va_end(Args);
}

std::string Quote(const char *Text)
{
	if (Text == NULL)
		return "NULL";
	std::string Result = "\"";
	for (const unsigned char *p = (const unsigned char *)Text; *p != 0; p++) {
		if (Result.size() > 700) {
			Result += "...";
			break;
		}
		if (*p < 32 || *p >= 127 || *p == '"' || *p == '\\' || *p == ';') {
			char Hexa[8];
			sprintf(Hexa, "\\x%02x", *p);
			Result += Hexa;
		} else {
			Result += (char)*p;
		}
	}
	return Result + "\"";
}

std::string Hex(const uint8 *Data, int Size)
{
	std::string Result;
	char Byte[4];
	for (int i = 0; i < Size; i++) {
		sprintf(Byte, "%02x", Data[i]);
		Result += Byte;
	}
	return Result;
}

// --- Failure injection ---------------------------------------------------------------


// The side's own assertion: TReadBuffer::GetByte on an empty buffer
// (Utils.cpp line 299), the image's 0x50a4b0 or ours.
void SideThrow()
{
	static uint8 Data[4];
	if (Side == IMAGE) {
		printf("golden: communication: the original's side called in the runner\n");
		abort();
	} else {
		TReadBuffer Empty(patella::TSpan<uint8>(Data).first(0));
		Empty.GetByte();
	}
}

void (*FaultHandler)();
// Set while a getter runs whose value an assertion of Control.cpp prints
// (GetActionState, GetActiveCharacter, GetAccountCharacters): such a call
// throws the side's TMyException even when FaultHandler would raise a
// structured exception. Raised there, during the original's construction of
// its assertion message, a structured exception left the original's frame
// without reaching its catch (...) (MouseMove, line 461, state 5 of its try
// block), while ours caught it; notes/decisions.md.
static bool AssertionGetter;

void Tick()
{
	bool Getter = AssertionGetter;
	AssertionGetter = false;
	Ticks[Side]++;
	if (FailAt != 0 && Ticks[Side] == FailAt) {
		Log[Side] += "THROW;";
		if (FaultHandler != NULL && !Getter)
			FaultHandler();
		else
			SideThrow();
	}
}

// --- Names ---------------------------------------------------------------------------

static std::map<const void *, std::string> Names[2];

void NameObject(int S, const void *Object, const std::string &Name) { Names[S][Object] = Name; }

static std::vector<std::pair<std::string, std::pair<uint32, const void *>>> FunctionNames;
void NameFunction(const char *Name, uint32 Image, const void *Ours)
{
	FunctionNames.push_back(std::make_pair(std::string(Name), std::make_pair(Image, Ours)));
}

std::string NameOf(const void *Object)
{
	if (Object == NULL)
		return "NULL";
	auto i = Names[Side].find(Object);
	return i != Names[Side].end() ? i->second : std::string("?");
}

// --- Hashes --------------------------------------------------------------------------

uint32 Hash(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e)
{
	uint32 h = Salt * 0x9e3779b1u;
	uint32 v[5] = {a, b, c, d, e};
	for (uint32 x : v) {
		h ^= x + 0x7f4a7c15u + (h << 6) + (h >> 2);
		h *= 0x85ebca6bu;
		h ^= h >> 13;
	}
	return h;
}

uint32 HashText(const char *Text)
{
	uint32 h = 2166136261u;
	if (Text == NULL)
		return 7;
	for (; *Text != 0; Text++)
		h = (h ^ (uint8)*Text) * 16777619u;
	return h;
}

// --- Text buffers ----------------------------------------------------------------------

bool TCommBuffer::SetWriteEnabled(bool Value)
{
	Call("%s.SetWriteEnabled(%d)", NameOf(this).c_str(), Value ? 1 : 0);
	return Value;
}

void TCommBuffer::Remove(TTalkLine *line)
{
	Call("%s.Remove(%s)", NameOf(this).c_str(), NameOf(line).c_str());
	// Unlinked, not freed (the world frees its lines).
	if (line->Previous != NULL)
		line->Previous->Next = line->Next;
	else
		first = line->Next;
	if (line->Next != NULL)
		line->Next->Previous = line->Previous;
	else
		last = line->Previous;
}

void TCommBuffer::SetSelected(bool Value)
{
	Call("%s.SetSelected(%d)", NameOf(this).c_str(), Value ? 1 : 0);
	Selected = Value;
}

// --- Windows ---------------------------------------------------------------------------

// On every target: TCtlMainWindow's members where our TMainWindow has them, and a TCtlWindow fits the memory
// our code allocates for each window a faked constructor builds in place (ours: our layout, x64 in the runner).
static_assert(offsetof(TCtlMainWindow, MapFrame) == offsetof(TMainWindow, MapFrame)
		&& offsetof(TCtlMainWindow, Console) == offsetof(TMainWindow, Console)
		&& offsetof(TCtlMainWindow, StartWindow) == offsetof(TMainWindow, StartWindow)
		&& offsetof(TCtlMainWindow, InGame) == offsetof(TMainWindow, InGame),
	"TCtlMainWindow's members are TMainWindow's");
static_assert(sizeof(TCtlWindow) <= sizeof(THelpBubble) && sizeof(TCtlWindow) <= sizeof(TObjectContextMenu)
		&& sizeof(TCtlWindow) <= sizeof(TCreatureContextMenu) && sizeof(TCtlWindow) <= sizeof(TBuddyContextMenu)
		&& sizeof(TCtlWindow) <= sizeof(TTextContextMenu) && sizeof(TCtlWindow) <= sizeof(TChannelContextMenu)
		&& sizeof(TCtlWindow) <= sizeof(TRequestContextMenu) && sizeof(TCtlWindow) <= sizeof(TEditContextMenu)
		&& sizeof(TCtlWindow) <= sizeof(TMarkContextMenu) && sizeof(TCtlWindow) <= sizeof(TSkillContextMenu),
	"a TCtlWindow fits each window built in place");
// The console's members Control.cpp reads, in memory of this size (the oracle's as before).
static constexpr size_t ConsoleSize = 0x100; // x64 (the oracle's x86 console was 0x80, until issue #94)
static_assert(offsetof(TConsoleWindow, CurrentConsole) + sizeof(void *) <= ConsoleSize
		&& offsetof(TConsole, TextWindow) + sizeof(void *) <= ConsoleSize,
	"the console's members fit");

static void ApplyNextActionState()
{
	if (Wd().NextActionState != -1) {
		Wd().ActionState = Wd().NextActionState;
		Wd().NextActionState = -1;
	}
}

static std::vector<void *> DeletedWindows;
void TCtlWindow::operator delete(void *Memory) { DeletedWindows.push_back(Memory); }

TCtlWindow::~TCtlWindow()
{
	std::string Name = NameOf(this);
	Logf("delete %s", Name.c_str());
	// A pointer left to it (the original keeps ModalWindow until its end) shows as deleted.
	NameObject(Side, this, "deleted " + Name);
}
void TCtlWindow::SetSize(int _Width, int _Height)
{
	Call("%s.SetSize(%d,%d)", NameOf(this).c_str(), _Width, _Height);
	Width = _Width;
	Height = _Height;
}
void TCtlWindow::SetPosition(int _OffsetX, int _OffsetY)
{
	Call("%s.SetPosition(%d,%d)", NameOf(this).c_str(), _OffsetX, _OffsetY);
	OffsetX = _OffsetX;
	OffsetY = _OffsetY;
}
void TCtlWindow::Draw(int Surface, int x, int y)
{
	Call("%s.Draw(%d,%d,%d)", NameOf(this).c_str(), Surface, x, y);
}
bool TCtlWindow::MouseMove(int x, int y)
{
	Call("%s.MouseMove(%d,%d)", NameOf(this).c_str(), x, y);
	ApplyNextActionState();
	return (Hash(1, x, y) & 1) != 0;
}
bool TCtlWindow::LeftButtonDown(int x, int y, int Flags)
{
	Call("%s.LeftButtonDown(%d,%d,%d)", NameOf(this).c_str(), x, y, Flags);
	ApplyNextActionState();
	return (Hash(2, x, y) & 1) != 0;
}
bool TCtlWindow::LeftButtonUp(int x, int y, int Flags)
{
	Call("%s.LeftButtonUp(%d,%d,%d)", NameOf(this).c_str(), x, y, Flags);
	ApplyNextActionState();
	return (Hash(3, x, y) & 1) != 0;
}
bool TCtlWindow::LeftButtonDoubleClick(int x, int y, int Flags)
{
	Call("%s.LeftButtonDoubleClick(%d,%d,%d)", NameOf(this).c_str(), x, y, Flags);
	ApplyNextActionState();
	return (Hash(4, x, y) & 1) != 0;
}
bool TCtlWindow::RightButtonUp(int x, int y, int Flags)
{
	Call("%s.RightButtonUp(%d,%d,%d)", NameOf(this).c_str(), x, y, Flags);
	ApplyNextActionState();
	return (Hash(5, x, y) & 1) != 0;
}
bool TCtlWindow::MouseWheel(int x, int y, int Direction)
{
	Call("%s.MouseWheel(%d,%d,%d)", NameOf(this).c_str(), x, y, Direction);
	return (Hash(6, x, y) & 1) != 0;
}
void TCtlWindow::MouseHover(int x, int y)
{
	Call("%s.MouseHover(%d,%d)", NameOf(this).c_str(), x, y);
	if (Hash(10, x, y) % 2 == 0)
		Wd().MouseOverText = "Hover text"; // a window with a tooltip under the mouse
}
bool TCtlWindow::HelpMouseMove(int x, int y)
{
	Call("%s.HelpMouseMove(%d,%d)", NameOf(this).c_str(), x, y);
	return (Hash(7, x, y) & 1) != 0;
}
void TCtlWindow::KeyPress(int Key)
{
	Call("%s.KeyPress(%d)", NameOf(this).c_str(), Key);
	ApplyNextActionState();
}
void TCtlWindow::StopDragging()
{
	Call("%s.StopDragging()", NameOf(this).c_str());
}
TComponent *TCtlWindow::GetCapture()
{
	Call("%s.GetCapture()=%s", NameOf(this).c_str(), NameOf(Wd().Capture).c_str());
	return Wd().Capture;
}

// A new window of the current side, named by its creation number.
static TCtlWindow *NewWindow(void *Memory = NULL)
{
	TCtlWindow *Window = Memory != NULL ? new (Memory) TCtlWindow() : new TCtlWindow();
	char Name[16];
	sprintf(Name, "W%d", Wd().WindowCount++);
	NameObject(Side, Window, Name);
	Window->Width = 100 + (int)(Hash(8, Wd().WindowCount) % 300);
	Window->Height = 50 + (int)(Hash(9, Wd().WindowCount) % 200);
	return Window;
}

TCtlWindow *MakeWindow() { return NewWindow(); }

// --- The surface ---------------------------------------------------------------------

static void SurfaceTrap(const char *Name)
{
	printf("crash: communicationtest: TCtlSurface::%s called\n", Name);
	fflush(stdout);
	abort();
}

class TCtlSurface : public TSurface {
public:
	bool InitResult, Recreated, Unavailable, TestResult;
	void SetPixel(int, int, int, TRGBColor) override { SurfaceTrap("SetPixel"); }
	void FillRectangleClipped(int, TRect, TRGBColor, TRect) override { SurfaceTrap("FillRectangleClipped"); }
	void FillRectangle(int Surface, TRect Rect, TRGBColor Color) override
	{
		Call("Surface.FillRectangle(%d,%d,%d,%d,%d,%d,%d,%d)", Surface, Rect.x, Rect.y, Rect.width, Rect.height,
			Color.R, Color.G, Color.B);
	}
	void DrawHorizontalLine(int, int, int, int, TRGBColor) override { SurfaceTrap("DrawHorizontalLine"); }
	void DrawVerticalLine(int, int, int, int, TRGBColor) override { SurfaceTrap("DrawVerticalLine"); }
	void DrawFrameClipped(int, TRect, int, TRGBColor, TRect) override { SurfaceTrap("DrawFrameClipped"); }
	void DrawFrame(int Surface, TRect Rect, int LineWidth, TRGBColor Color) override
	{
		Call("Surface.DrawFrame(%d,%d,%d,%d,%d,%d,%d,%d,%d)", Surface, Rect.x, Rect.y, Rect.width, Rect.height,
			LineWidth, Color.R, Color.G, Color.B);
	}
	void WriteImage(int, int, int, int, int, uint8 *) override { SurfaceTrap("WriteImage"); }
	void WriteMaskedImage(int, int, int, int, int, uint8 *, uint8 *, TRGBColor *) override { SurfaceTrap("WriteMaskedImage"); }
	void BlitRect(int, int, int, int, TRect) override { SurfaceTrap("BlitRect"); }
	void Blit(int, int, int, int, TRect, TRGBColor) override { SurfaceTrap("Blit"); }
	void BlitRectClipped(int, int, int, int, TRect, TRect) override { SurfaceTrap("BlitRectClipped"); }
	void BlitClipped(int, int, int, int, TRect, TRGBColor, TRect) override { SurfaceTrap("BlitClipped"); }
	void StretchBlitRect(int, TRect, int, TRect) override { SurfaceTrap("StretchBlitRect"); }
	void StretchBlitUnclipped(int, TRect, int, TRect, TRGBColor, TRGBColor) override { SurfaceTrap("StretchBlitUnclipped"); }
	void StretchBlit(int, TRect, int, TRect, TRGBColor, TRGBColor, TRect) override { SurfaceTrap("StretchBlit"); }
	void ShadeRectangles(int, int, TRect *, TRGBColor *) override { SurfaceTrap("ShadeRectangles"); }
	void SetSourceColorKey(int, TRGBColor) override { SurfaceTrap("SetSourceColorKey"); }
	bool ClipRect(TRect, RECT *, RECT *) override
	{
		SurfaceTrap("ClipRect");
		return false;
	}
	void ResetFrameTimer() override { SurfaceTrap("ResetFrameTimer"); }
	void WaitForNextFrame() override { SurfaceTrap("WaitForNextFrame"); }
	void UpdateWindowRect() override { SurfaceTrap("UpdateWindowRect"); }
	void InitDisplay(int, TFullscreenMode *) override { SurfaceTrap("InitDisplay"); }
	void ExitDisplay() override { SurfaceTrap("ExitDisplay"); }
	void CreateSurfaces() override { SurfaceTrap("CreateSurfaces"); }
	void ResetDisplay(int, TFullscreenMode *) override { SurfaceTrap("ResetDisplay"); }
	void RestoreSurfaces(int, TFullscreenMode *) override { SurfaceTrap("RestoreSurfaces"); }
	void FinishFrame() override { Call("Surface.FinishFrame()"); }
	bool Init(HWND *Window, int Mode, TFullscreenMode *Resolution, bool Force) override
	{
		Call("Surface.Init(%s,%d,%s,%d)=%d", Window != NULL ? NameOf(*Window).c_str() : "NULL", Mode,
			NameOf(Resolution).c_str(), Force ? 1 : 0, InitResult ? 1 : 0);
		return InitResult;
	}
	int GetFullscreenModes(TFullscreenMode **) override
	{
		SurfaceTrap("GetFullscreenModes");
		return 0;
	}
	int GetCacheSurfaces() override
	{
		SurfaceTrap("GetCacheSurfaces");
		return 0;
	}
	void GetSurfaceRect(int, TRect *) override { SurfaceTrap("GetSurfaceRect"); }
	bool SurfaceRecreated(int Surface) override
	{
		Call("Surface.SurfaceRecreated(%d)=%d", Surface, Recreated ? 1 : 0);
		return Recreated;
	}
	bool IsUnavailable() override
	{
		Call("Surface.IsUnavailable()=%d", Unavailable ? 1 : 0);
		return Unavailable;
	}
	bool CanShadeRectangles() override
	{
		SurfaceTrap("CanShadeRectangles");
		return false;
	}
	int GetSurfaceMemory(int) override
	{
		SurfaceTrap("GetSurfaceMemory");
		return 0;
	}
	void SetFramerateLimit(int) override { SurfaceTrap("SetFramerateLimit"); }
	double GetFPS() override
	{
		Call("Surface.GetFPS()");
		return 37.25 + (Salt % 100);
	}
	void InitSurfaces() override { Call("Surface.InitSurfaces()"); }
	void ExitSurfaces() override { Call("Surface.ExitSurfaces()"); }
	bool TestEngine() override
	{
		Call("Surface.TestEngine()=%d", TestResult ? 1 : 0);
		return TestResult;
	}
};

// --- Worlds ----------------------------------------------------------------------------

[[maybe_unused]] static const int WorldLines = WORLD_BUFFERS * WORLD_LINES;

static void BuildWorld(int S, uint32 Seed)
{
	int Saved = Side;
	Side = S;
	SetRandomSeed(Seed);
	TWorld &W = World[S];
	Names[S].clear();
	memset(W.Creatures, 0xab, sizeof(W.Creatures));
	for (int i = 0; i < WORLD_CREATURES; i++) {
		TCreature &C = W.Creatures[i];
		if (i == 0)
			C.CreatureID = 0x10000000 + Random() % 8;
		else {
			const uint32 Base = Random() % 3 == 0 ? 0x10000000 : 0x40000000; // issue #55: the left operand first
			C.CreatureID = Base + Random() % 8;
		}
		if (Random() % 6 == 0)
			C.CreatureID = 0;
		sprintf(C.Name, "Creature %d", i);
		char Name[16];
		sprintf(Name, "C%d", i);
		NameObject(S, &C, Name);
	}
	W.PlayerID = W.Creatures[0].CreatureID;
	W.AttackID = Random() % 3 == 0 ? 0 : W.Creatures[Random() % WORLD_CREATURES].CreatureID;
	W.FollowID = Random() % 3 == 0 ? 0 : W.Creatures[Random() % WORLD_CREATURES].CreatureID;
	W.PlayerSteps = Random() % 4 == 0 ? Random() % 9 : 1 + Random() % 2;
	for (int i = 0; i < 8; i++)
		W.Steps[i] = Random() % 10;
	W.PlayerX = 32000 + Random() % 100;
	W.PlayerY = 31000 + Random() % 100;
	W.PlayerZ = Random() % 16;
	W.MapX = W.PlayerX;
	W.MapY = W.PlayerY;
	W.MapZ = W.PlayerZ;
	W.MapLevel = Random() % 8;
	W.CylinderIndex = -1;
	W.CylinderX = W.CylinderY = W.CylinderZ = 0;
	W.OpenContainers = Random32();
	for (int b = 0; b < WORLD_BUFFERS; b++) {
		TCommBuffer *Buffer = new TCommBuffer();
		W.Buffers[b] = Buffer;
		char Name[16];
		sprintf(Name, "Buffer%d", b);
		NameObject(S, Buffer, Name);
		int Count = Random() % (WORLD_LINES + 1);
		TTalkLine *Previous = NULL;
		for (int l = 0; l < WORLD_LINES; l++) {
			TTalkLine *Line = (TTalkLine *)calloc(1, sizeof(TTalkLine));
			W.Lines[b][l] = Line;
			sprintf(Name, "Line%d.%d", b, l);
			NameObject(S, Line, Name);
			static const char *const Addressees[] = {"Alice", "alice", "Bob", "Carol", "", "Alicia"};
			strcpy(Line->Addressee, Addressees[Random() % 6]);
			Line->StatementID = l;
			if (l < Count) {
				Line->Previous = Previous;
				if (Previous != NULL)
					Previous->Next = Line;
				else
					Buffer->first = Line;
				Buffer->last = Line;
				Previous = Line;
			}
		}
		Buffer->cursor = Count > 0 && Random() % 2 == 0 ? Buffer->first : NULL;
	}
	W.ActionState = Random() % 12;
	W.ConnectionState = Random() % 9;
	W.ActionNumber = 1000 + Random() % 50;
	W.MouseX = (int)(Random() % 900) - 50;
	W.MouseY = (int)(Random() % 700) - 50;
	W.MouseCursor = Random() % 8;
	W.PendingState = Random() % 3 == 0 ? 6 : Random() % 8;
	W.PendingNumber = Random() % 3 == 0 ? 0 : W.ActionNumber - Random() % 12;
	W.AccountCharacters = Random() % 5;
	W.ActiveCharacter = Random() % 6 - 1;
	static const char *const OverTexts[] = {NULL, "Tooltip", "A long tooltip text that is wider than the screen"};
	W.MouseOverText = OverTexts[Random() % 3];
	W.MouseOverX = (int)(Random() % 900) - 50;
	W.MouseOverY = (int)(Random() % 700) - 50;
	W.ErrorReports = Random() % 4;
	W.Events.clear();
	W.ViewMode = Random() % 3;
	W.Engine = Random() % 3;
	W.ClassicControl = Random() % 2 != 0;
	W.WindowsMousepointer = Random() % 2 != 0;
	W.AutoChaseOff = Random() % 2 != 0;
	W.AttackMode = Random() % 3;
	W.ChaseMode = Random() % 2;
	W.SecureMode = Random() % 2;
	W.Fullscreen.Width = 640 + Random() % 1000;
	W.Fullscreen.Height = 480 + Random() % 800;
	W.Fullscreen.Depth = 16;
	W.Fullscreen.Frequency = 60;
	NameObject(S, &W.Fullscreen, "Fullscreen");
	W.HelpHandle = Random() % 3;
	W.HelpChanged = Random() % 2 != 0;
	W.WindowCount = 0;
	W.Main = new TCtlMainWindow();
	NameObject(S, W.Main, "Main");
	W.Main->Width = 640 + Random() % 400;
	W.Main->Height = 480 + Random() % 300;
	W.Main->MapFrame = W.Main->SideBar = W.Main->DialogWindow = W.Main->SidePanel = NULL;
	W.Main->Splitter = W.Main->StartWindow = NULL;
	W.Main->InGame = false;
	// The console (TConsoleWindow, 0x6c bytes in the oracle) and its current console
	// (CurrentConsole +0x34, TextWindow +0x2c there): only these members are read.
	W.Console = calloc(1, ConsoleSize);
	W.CurrentConsole = calloc(1, ConsoleSize);
	W.TextWindow = calloc(1, ConsoleSize);
	*(void **)((uint8 *)W.Console + offsetof(TConsoleWindow, CurrentConsole)) = W.CurrentConsole;
	*(void **)((uint8 *)W.CurrentConsole + offsetof(TConsole, TextWindow)) = W.TextWindow;
	W.Main->Console = W.Console;
	NameObject(S, W.Console, "Console");
	NameObject(S, W.CurrentConsole, "CurrentConsole");
	NameObject(S, W.TextWindow, "TextWindow");
	W.Surface = new TCtlSurface();
	NameObject(S, W.Surface, "Surface");
	W.Surface->InitResult = Random() % 4 != 0;
	W.Surface->Recreated = Random() % 3 == 0;
	W.Surface->Unavailable = Random() % 6 == 0;
	W.Surface->TestResult = Random() % 3 != 0;
	W.Capture = Random() % 4 == 0 ? (TComponent *)W.Main : NULL;
	W.NextActionState = -1;
	W.Milliseconds = 100000 + Random() % 20000;
	for (auto &F : FunctionNames)
		NameObject(S, S == IMAGE ? (const void *)(uintptr_t)F.second.first : F.second.second, F.first);
	Side = Saved;
}

void ResetWorlds()
{
	uint32 Seed = Random();
	Salt = Random() | 1;
	BuildWorld(OURS, Seed);
	BuildWorld(IMAGE, Seed);
	for (int S = 0; S < 2; S++) {
		SetSideMainWindow(S, World[S].Main);
		SideModalWindow(S) = NULL;
		SetReceiveData(S, NULL, 0);
	}
	SetRandomSeed(Seed);
	Random();
}

void FreeEvent(const TQueuedEvent &E)
{
	switch (E.Type) {
	case 0x01:
	case 0x02:
	case 0x2f:
	case 0x30:
	case 0x31:
	case 0x32:
		delete[] (char *)E.Param2;
		break;
	case 0x05:
	case 0x10:
	case 0x19:
	case 0x1a:
	case 0x33:
		delete[] (char *)E.Param1;
		break;
	case 0x06:
	case 0x07:
		delete[] (INT_PTR *)E.Param1;
		break;
	case 0x0e: {
		INT_PTR *Texts = (INT_PTR *)E.Param2;
		delete[] (char *)E.Param1;
		if (Texts != NULL) {
			delete[] (char *)Texts[0];
			delete[] (char *)Texts[1];
			delete[] Texts;
		}
		break;
	}
	case 0x0f:
	case 0x12:
	case 0x1c:
		delete[] (char *)E.Param1;
		delete[] (char *)E.Param2;
		break;
	}
}

static void FreeWorld(int S)
{
	TWorld &W = World[S];
	if (W.Main == NULL)
		return;
	for (const TQueuedEvent &E : W.Events)
		FreeEvent(E);
	W.Events.clear();
	for (int b = 0; b < WORLD_BUFFERS; b++) {
		W.Buffers[b]->first = W.Buffers[b]->last = W.Buffers[b]->cursor = NULL;
		delete W.Buffers[b];
		W.Buffers[b] = NULL;
		for (int l = 0; l < WORLD_LINES; l++)
			free(W.Lines[b][l]);
	}
	int Saved = Side;
	Side = S;
	DeleteComponent(W.Main);
	Side = Saved;
	W.Main = NULL;
	free(W.Console);
	free(W.CurrentConsole);
	free(W.TextWindow);
	delete W.Surface;
	W.Surface = NULL;
}

void FreeWorlds()
{
	FreeWorld(OURS);
	FreeWorld(IMAGE);
	for (void *Memory : DeletedWindows)
		free(Memory);
	DeletedWindows.clear();
	for (int S = 0; S < 2; S++) {
		SetSideMainWindow(S, NULL);
		SideModalWindow(S) = NULL;
	}
}

static std::string Text(INT_PTR Value) { return Quote((const char *)Value); }

void DescribeEvent(std::string &Out, const TQueuedEvent &E)
{
	char Buffer[256];
	sprintf(Buffer, "%d(", E.Type);
	Out += Buffer;
	INT_PTR P1 = E.Param1, P2 = E.Param2;
	switch (E.Type) {
	case 0x01:
	case 0x02:
		Out += Text(P1) + "," + Text(P2);
		break;
	case 0x05:
	case 0x29:
		sprintf(Buffer, ",%d", (int)P2);
		Out += Text(P1) + Buffer;
		break;
	case 0x06:
	case 0x07: {
		INT_PTR *Data = (INT_PTR *)P1;
		sprintf(Buffer, ",%d", (int)Data[3]);
		Out += Text(Data[0]) + "," + Text(Data[1]) + "," + NameOf((void *)Data[2]) + Buffer;
		break;
	}
	case 0x0b:
	case 0x0c:
	case 0x19:
	case 0x1a:
	case 0x33:
		Out += Text(P1);
		break;
	case 0x0e: {
		uint32 *Info = (uint32 *)P1;
		INT_PTR *Texts = (INT_PTR *)P2;
		sprintf(Buffer, "%u,%u,%u,", Info[0], Info[1], Info[2]);
		Out += Buffer + Text(Texts[0]) + "," + Text(Texts[1]);
		break;
	}
	case 0x0f: {
		uint32 *Info = (uint32 *)P1;
		sprintf(Buffer, "%u,%u,", Info[0], Info[1]);
		Out += Buffer + Text(P2);
		break;
	}
	case 0x10: {
		int *Outfit = (int *)P1;
		sprintf(Buffer, "%d,%d,%d,%d,%d,%d,%d", Outfit[0], Outfit[1], Outfit[2], Outfit[3], Outfit[4], Outfit[5],
			Outfit[6]);
		Out += Buffer;
		break;
	}
	case 0x12: {
		int *Channels = (int *)P1;
		const char *ChannelNames = (const char *)P2;
		sprintf(Buffer, "%d", Channels[0]);
		Out += Buffer;
		for (int i = 0; i < Channels[0] && i < 300; i++) {
			sprintf(Buffer, ",%d:", Channels[i + 1]);
			Out += Buffer + Quote(ChannelNames + i * 30);
		}
		if (ChannelNames == NULL)
			Out += ",NULL";
		break;
	}
	case 0x1c:
		Out += Text(P1) + "," + NameOf((void *)P2);
		break;
	case 0x2f:
		sprintf(Buffer, "%d,", (int)P1);
		Out += Buffer + Text(P2);
		break;
	case 0x30:
	case 0x31:
		sprintf(Buffer, "%d,", (int)P1);
		Out += Buffer + Text(P2);
		break;
	case 0x32:
		Out += NameOf((void *)P1) + "," + (P2 != 0 ? Text(P2) : std::string("0"));
		break;
	case 0x1d:
	case 0x1e:
	case 0x24:
	case 0x3a:
	case 0x3b:
	case 0x3c:
		Out += NameOf((void *)P1) + "," + NameOf((void *)P2);
		break;
	default:
		sprintf(Buffer, "%d,%d", (int)P1, (int)P2);
		Out += Buffer;
	}
	Out += ")";
}

// --- Receive and send buffers ------------------------------------------------------------

// Our ReceiveBuffer and SendBuffer are statics of Network.cpp. In the oracle their
// addresses are the "mov ecx, imm32" before the first call in our GetByte
// (TReadBuffer::GetByte) and BeginCommand (TWriteBuffer::PutWord); the runner
// compiles Network.cpp into network_tests.cpp, which gives them by name.
static TBuffer *OurReceiveBuffer, *OurSendBuffer;

// network_tests.cpp: Network.cpp's buffers, back in the state of the program's start.
TBuffer *GoldenNetworkReceiveBuffer();
TBuffer *GoldenNetworkSendBuffer();
void GoldenNetworkResetBuffers();
// The original's buffers have no data in the runner; the test still sets them as the oracle does.
static TBuffer ImageReceiveBuffer, ImageSendBuffer;
TBuffer *ReceiveBufferOf(int S) { return S == IMAGE ? &ImageReceiveBuffer : OurReceiveBuffer; }
TBuffer *SendBufferOf(int S) { return S == IMAGE ? &ImageSendBuffer : OurSendBuffer; }

static uint8 *ReceiveCopy[2];
void SetReceiveData(int S, const uint8 *Data, int Size)
{
	free(ReceiveCopy[S]);
	ReceiveCopy[S] = (uint8 *)malloc(Size > 0 ? Size : 1);
	if (Size > 0)
		memcpy(ReceiveCopy[S], Data, Size);
	TBuffer *B = ReceiveBufferOf(S);
	B->Data = ReceiveCopy[S];
	B->Size = Size;
	B->Position = 0;
}

// --- Object flags --------------------------------------------------------------------------

static std::vector<uint32> FlagTable; // per type from 100: the 32 flags
static std::vector<int> WaypointTable;
static int FirstType = 100;

// The oracle loads Tibia.dat with our Objects.cpp; the tables go into the corpus by value (the runner
// needs neither Tibia.dat nor Objects.cpp's state).
void LoadObjectFlags(const char *ExePath)
{
	if (!FlagTable.empty())
		return;
	static_cast<void>(ExePath);
	if (!RecordComparisons)
		return;
	int Sizes[2] = {FirstType, (int)FlagTable.size()};
	golden::Output("object flags", Sizes);
	FirstType = Sizes[0];
	FlagTable.resize((size_t)Sizes[1]);
	WaypointTable.resize((size_t)Sizes[1]);
	if (Sizes[1] > 0) {
		golden::OutputArray("flag table", FlagTable.data(), FlagTable.size());
		golden::OutputArray("waypoint table", WaypointTable.data(), WaypointTable.size());
	}
}

int MaxObjectType() { return FlagTable.empty() ? 3000 : FirstType + (int)FlagTable.size() - 1; }

bool ObjectFlag(int Type, int Flag)
{
	if (Type >= FirstType && Type - FirstType < (int)FlagTable.size() && Flag >= 0 && Flag < 32)
		return (FlagTable[Type - FirstType] >> Flag & 1) != 0;
	return Hash(50, Type, Flag) % 3 == 0;
}

int ObjectWaypoints(int Type)
{
	if (Type >= FirstType && Type - FirstType < (int)WaypointTable.size())
		return WaypointTable[Type - FirstType];
	return (int)(Hash(51, Type) % 300);
}

// --- Frames ------------------------------------------------------------------------------

std::vector<std::vector<uint8>> Frames;
int NextFrame[2];

// --- The fakes ---------------------------------------------------------------------------

// Network.cpp's GetNextCommand: the next byte of the current frame, else the
// next frame (as PrepareNextPacket does), -1 when there is none.
static int F_GetNextCommand()
{
	TBuffer *B = ReceiveBufferOf(Side);
	if (B->Position == B->Size) {
		if (NextFrame[Side] >= (int)Frames.size()) {
			Call("GetNextCommand()=-1");
			return -1;
		}
		const std::vector<uint8> &Frame = Frames[NextFrame[Side]++];
		SetReceiveData(Side, Frame.data(), (int)Frame.size());
		if (B->Position == B->Size) {
			Call("GetNextCommand()=-1");
			return -1;
		}
	}
	int Type = B->Data[B->Position++];
	Call("GetNextCommand()=%d", Type);
	return Type;
}

static std::string Obj(const TObject &O)
{
	char Buffer[48];
	sprintf(Buffer, "{%d,%d,%d}", O.Type, O.Data, O.Data2);
	return Buffer;
}

// Network.cpp
static void F_SetConnectionTimeout(int Timeout) { Call("SetConnectionTimeout(%d)", Timeout); }
static void F_SetServerError(int Error) { Call("SetServerError(%d)", Error); }
static void F_SetSymmetricKey(const uint8 *Key) { Call("SetSymmetricKey(%s)", Hex(Key, 16).c_str()); }
static void F_Disconnect(bool Force) { Call("Disconnect(%d)", Force ? 1 : 0); }
static void F_ConnectToLoginServer() { Call("ConnectToLoginServer()"); }
static void F_ConnectToGameServer() { Call("ConnectToGameServer()"); }
static void F_SendCommand(bool Encrypt)
{
	TBuffer *B = SendBufferOf(Side);
	Call("SendCommand(%d,%s)", Encrypt ? 1 : 0, Hex(B->Data, B->Position).c_str());
}
static void F_SetNetworkWindow(HWND Window) { Call("SetNetworkWindow(%s)", NameOf(Window).c_str()); }
static void F_GetErrorMessage(char *Message, int ErrorCode, int ErrorType)
{
	Call("GetErrorMessage(%d,%d)", ErrorCode, ErrorType);
	sprintf(Message, "Error %d of kind %d.", ErrorCode, ErrorType);
}
static void F_InitNetwork(void (*ConnectHandler)(), void (*ReceiveHandler)(), void (*CloseHandler)())
{
	Call("InitNetwork(%s,%s,%s)", NameOf((void *)ConnectHandler).c_str(), NameOf((void *)ReceiveHandler).c_str(),
		NameOf((void *)CloseHandler).c_str());
}
static void F_ExitNetwork() { Call("ExitNetwork()"); }
static void F_CheckConnectionTimeout() { Call("CheckConnectionTimeout()"); }

// Map.cpp
static void F_SetMapPosition(int x, int y, int z)
{
	Call("SetMapPosition(%d,%d,%d)", x, y, z);
	Wd().MapX = x;
	Wd().MapY = y;
	Wd().MapZ = z;
}
static void F_GetMapPosition(int *x, int *y, int *z)
{
	Call("GetMapPosition()=%d,%d,%d", Wd().MapX, Wd().MapY, Wd().MapZ);
	*x = Wd().MapX;
	*y = Wd().MapY;
	*z = Wd().MapZ;
}
static int F_GetMapLevel()
{
	Call("GetMapLevel()=%d", Wd().MapLevel);
	return Wd().MapLevel;
}
static void F_RelativeToAbsolute(int *x, int *y, int *z)
{
	Call("RelativeToAbsolute(%d,%d,%d)", *x, *y, *z);
	*x += Wd().MapX - 8;
	*y += Wd().MapY - 6;
	*z = Wd().MapZ + *z - (Wd().MapZ <= 7 ? 7 : 2);
}
static void F_AbsoluteToRelative(int *x, int *y, int *z)
{
	Call("AbsoluteToRelative(%d,%d,%d)", *x, *y, *z);
	*x -= Wd().MapX - 8;
	*y -= Wd().MapY - 6;
	*z = *z - Wd().MapZ + (Wd().MapZ <= 7 ? 7 : 2);
}
static void F_SetAmbientLight(int Brightness, int Color) { Call("SetAmbientLight(%d,%d)", Brightness, Color); }
static TObject CylinderObject(int x, int y, int z, int i)
{
	TObject O;
	uint32 h = Hash(20, x, y, z, i);
	if (i >= (int)(Hash(21, x, y, z) % 5)) {
		O.Type = 0;
		return O;
	}
	O.Type = h % 7 == 0 ? 0x63 : 100 + (int)(h % 3000);
	O.Data = O.Type == 0x63 ? (int)Wd().Creatures[(h >> 8) % WORLD_CREATURES].CreatureID : (int)((h >> 12) % 100);
	return O;
}
static TObject F_GetFirstObject(int rx, int ry, int rz)
{
	Wd().CylinderIndex = 0;
	Wd().CylinderX = rx;
	Wd().CylinderY = ry;
	Wd().CylinderZ = rz;
	TObject O = CylinderObject(rx, ry, rz, 0);
	Call("GetFirstObject(%d,%d,%d)=%s", rx, ry, rz, Obj(O).c_str());
	return O;
}
static TObject F_GetNextObject()
{
	TWorld &W = Wd();
	W.CylinderIndex++;
	TObject O = CylinderObject(W.CylinderX, W.CylinderY, W.CylinderZ, W.CylinderIndex);
	Call("GetNextObject()=%s", Obj(O).c_str());
	return O;
}
static TObject F_GetMapObject(int rx, int ry, int rz, int Position)
{
	TObject O;
	uint32 h = Hash(22, rx, ry, rz, Position);
	switch (h % 8) {
	case 0:
	case 1:
		O = patella::bridge::ObjectTypes().NoObject;
		break;
	case 2:
	case 3:
	case 4:
		O.Type = 0x63;
		O.Data = (int)Wd().Creatures[(h >> 8) % WORLD_CREATURES].CreatureID;
		break;
	case 5:
		O.Type = 0x61 + (int)((h >> 8) % 2);
		O.Data = (int)(h >> 12) % 50;
		break;
	default:
		O.Type = 100 + (int)((h >> 8) % 3000);
		O.Data = (int)(h >> 20) % 100;
	}
	Call("GetMapObject(%d,%d,%d,%d)=%s", rx, ry, rz, Position, Obj(O).c_str());
	return O;
}
static void F_AddObject(int rx, int ry, int rz, TObject O) { Call("AddObject(%d,%d,%d,%s)", rx, ry, rz, Obj(O).c_str()); }
static void F_AppendObject(int rx, int ry, int rz, TObject O)
{
	Call("AppendObject(%d,%d,%d,%s)", rx, ry, rz, Obj(O).c_str());
}
static bool F_DeleteObject(int rx, int ry, int rz, int Position)
{
	bool Result = Hash(23, rx, ry, rz, Position) % 3 == 0;
	Call("DeleteObject(%d,%d,%d,%d)=%d", rx, ry, rz, Position, Result ? 1 : 0);
	return Result;
}
static void F_ChangeObject(int rx, int ry, int rz, int Position, TObject O)
{
	Call("ChangeObject(%d,%d,%d,%d,%s)", rx, ry, rz, Position, Obj(O).c_str());
}
static void F_AddGraphicalEffect(int rx, int ry, int rz, int Type)
{
	Call("AddGraphicalEffect(%d,%d,%d,%d)", rx, ry, rz, Type);
}
static void F_AddTextualEffect(int rx, int ry, int rz, int Color, const char *Text)
{
	Call("AddTextualEffect(%d,%d,%d,%d,%s)", rx, ry, rz, Color, Quote(Text).c_str());
}
static void F_AddMissileEffect(int sx, int sy, int sz, int dx, int dy, int dz, int Type)
{
	Call("AddMissileEffect(%d,%d,%d,%d,%d,%d,%d)", sx, sy, sz, dx, dy, dz, Type);
}
static void F_ClearField(int rx, int ry, int rz, bool DeleteEffects)
{
	Call("ClearField(%d,%d,%d,%d)", rx, ry, rz, DeleteEffects ? 1 : 0);
}
static void F_ClearMap() { Call("ClearMap()"); }
static void F_ShiftMap(int dx, int dy) { Call("ShiftMap(%d,%d)", dx, dy); }
static void F_ShiftMapLevel(int dz) { Call("ShiftMapLevel(%d)", dz); }
static void F_RequestFieldRefresh(int rx, int ry, int rz) { Call("RequestFieldRefresh(%d,%d,%d)", rx, ry, rz); }
static bool F_FieldRefreshRequested(int rx, int ry, int rz)
{
	bool Result = Hash(24, rx, ry, rz) % 2 == 0;
	Call("FieldRefreshRequested(%d,%d,%d)=%d", rx, ry, rz, Result ? 1 : 0);
	return Result;
}
static void F_UpdateAutomap(int rx, int ry) { Call("UpdateAutomap(%d,%d)", rx, ry); }
static void F_UpdateAmbientLight() { Call("UpdateAmbientLight()"); }
static void F_ExitMap() { Call("ExitMap()"); }
static void F_ProcessEffects() { Call("ProcessEffects()"); }
static void F_InitMap(void (*Callback)(int)) { Call("InitMap(%s)", NameOf((void *)Callback).c_str()); }

// Creatures.cpp
static void GOLDEN_THISCALL F_SetSkull(GOLDEN_THIS_NAMED(TCreature *This), int Skull, bool Delay)
{
	Call("%s.SetSkull(%d,%d)", NameOf(This).c_str(), Skull, Delay ? 1 : 0);
	This->PKFlag = Skull ^ 0x10;
}
static void GOLDEN_THISCALL F_SetMark(GOLDEN_THIS_NAMED(TCreature *This), int Mark)
{
	Call("%s.SetMark(%d)", NameOf(This).c_str(), Mark);
	This->Mark = Mark;
}
static void GOLDEN_THISCALL F_CreatureSetPosition(GOLDEN_THIS_NAMED(TCreature *This), int x, int y, int z)
{
	Call("%s.SetPosition(%d,%d,%d)", NameOf(This).c_str(), x, y, z);
	This->RealX = x;
	This->RealY = y;
	This->RealZ = z;
}
static void GOLDEN_THISCALL F_StartMovement(GOLDEN_THIS_NAMED(TCreature *This), int dx, int dy, int Waypoints)
{
	Call("%s.StartMovement(%d,%d,%d)", NameOf(This).c_str(), dx, dy, Waypoints);
}
static int F_GetNumberOfPlayerSteps()
{
	Call("GetNumberOfPlayerSteps()=%d", Wd().PlayerSteps);
	return Wd().PlayerSteps;
}
static void F_GetPlayerPosition(int *x, int *y, int *z)
{
	Call("GetPlayerPosition()=%d,%d,%d", Wd().PlayerX, Wd().PlayerY, Wd().PlayerZ);
	*x = Wd().PlayerX;
	*y = Wd().PlayerY;
	*z = Wd().PlayerZ;
}
static void F_ClearPlayerSteps() { Call("ClearPlayerSteps()"); }
static void F_HideAllCreatures() { Call("HideAllCreatures()"); }
static void F_SetBeat(int Beat) { Call("SetBeat(%d)", Beat); }
static bool F_IsPlayer(uint32 CreatureID)
{
	bool Result = CreatureID < 0x40000000;
	Call("IsPlayer(%u)=%d", CreatureID, Result ? 1 : 0);
	return Result;
}
static void F_SetPlayerID(uint32 CreatureID)
{
	Call("SetPlayerID(%u)", CreatureID);
	Wd().PlayerID = CreatureID;
}
static void F_SetPlayerHealth(int Health, int MaxHealth) { Call("SetPlayerHealth(%d,%d)", Health, MaxHealth); }
static void F_SetPlayerMagicLevel(int Level, int Progress) { Call("SetPlayerMagicLevel(%d,%d)", Level, Progress); }
static void F_SetPlayerMana(int Mana, int MaxMana) { Call("SetPlayerMana(%d,%d)", Mana, MaxMana); }
static void F_SetPlayerSoul(int Soul) { Call("SetPlayerSoul(%d)", Soul); }
static void F_SetPlayerCapacity(int Capacity) { Call("SetPlayerCapacity(%d)", Capacity); }
static void F_SetPlayerState(int State) { Call("SetPlayerState(%d)", State); }
static uint32 F_GetPlayerID()
{
	Call("GetPlayerID()=%u", Wd().PlayerID);
	return Wd().PlayerID;
}
static void F_SetAttackID(uint32 CreatureID)
{
	Call("SetAttackID(%u)", CreatureID);
	Wd().AttackID = CreatureID;
}
static void F_SetFollowID(uint32 CreatureID)
{
	Call("SetFollowID(%u)", CreatureID);
	Wd().FollowID = CreatureID;
}
static uint32 F_GetAttackID()
{
	Call("GetAttackID()=%u", Wd().AttackID);
	return Wd().AttackID;
}
static uint32 F_GetFollowID()
{
	Call("GetFollowID()=%u", Wd().FollowID);
	return Wd().FollowID;
}
static int F_GetPlayerStep(int Step)
{
	int Result = Wd().Steps[Step & 7];
	Call("GetPlayerStep(%d)=%d", Step, Result);
	return Result;
}
static TCreature *F_AddCreature(uint32 RemoveID, uint32 NewCreatureID)
{
	uint32 h = Hash(30, RemoveID, NewCreatureID);
	TCreature *C = NULL;
	if (h % 9 != 0) {
		C = &Wd().Creatures[(h >> 4) % WORLD_CREATURES];
		C->CreatureID = NewCreatureID;
	}
	Call("AddCreature(%u,%u)=%s", RemoveID, NewCreatureID, NameOf(C).c_str());
	return C;
}
static TCreature *F_GetCreature(uint32 CreatureID)
{
	TCreature *C = NULL;
	if (CreatureID != 0) {
		for (int i = 0; i < WORLD_CREATURES; i++) {
			if (Wd().Creatures[i].CreatureID == CreatureID) {
				C = &Wd().Creatures[i];
				break;
			}
		}
	}
	Call("GetCreature(%u)=%s", CreatureID, NameOf(C).c_str());
	return C;
}
static void F_NotifySkull(int Flag) { Call("NotifySkull(%d)", Flag); }
static void F_SetPlayerExperience(int Experience, int Level, int Progress)
{
	Call("SetPlayerExperience(%d,%d,%d)", Experience, Level, Progress);
}
static void F_SetPlayerSkill(int SkillNumber, int Value, int Progress)
{
	Call("SetPlayerSkill(%d,%d,%d)", SkillNumber, Value, Progress);
}
static void F_AddBuddy(uint32 CharacterID, const char *Name, bool Online)
{
	Call("AddBuddy(%u,%s,%d)", CharacterID, Quote(Name).c_str(), Online ? 1 : 0);
}
static void F_SetBuddyState(uint32 CharacterID, bool Online) { Call("SetBuddyState(%u,%d)", CharacterID, Online ? 1 : 0); }
static void F_StopPlayer() { Call("StopPlayer()"); }
static bool F_CancelPlayerWalk(int NewDirection)
{
	bool Result = Hash(31, NewDirection) % 2 == 0;
	Call("CancelPlayerWalk(%d)=%d", NewDirection, Result ? 1 : 0);
	return Result;
}
static void F_CheckMonsterHint(TCreature *Creature) { Call("CheckMonsterHint(%s)", NameOf(Creature).c_str()); }
static void F_UpdateSkulls() { Call("UpdateSkulls()"); }
static void F_ResetCreatures() { Call("ResetCreatures()"); }
static bool F_MoveCreatures()
{
	bool Result = Hash(32, Wd().ActionNumber) % 2 == 0;
	Call("MoveCreatures()=%d", Result ? 1 : 0);
	return Result;
}
static void F_InitCreatures() { Call("InitCreatures()"); }

// Help.cpp
static void F_CheckEventHelpPosition(int x, int y, int z) { Call("CheckEventHelpPosition(%d,%d,%d)", x, y, z); }
static void F_SetHelpPosition(int x, int y) { Call("SetHelpPosition(%d,%d)", x, y); }
static void F_SetHelpSize(int Width, int Height) { Call("SetHelpSize(%d,%d)", Width, Height); }
static void F_SetHelpHandle(int Handle)
{
	Call("SetHelpHandle(%d)", Handle);
	Wd().HelpHandle = Handle;
}
static int F_GetHelpHandle()
{
	Call("GetHelpHandle()=%d", Wd().HelpHandle);
	return Wd().HelpHandle;
}
static bool F_GetHelpHandleChanged()
{
	Call("GetHelpHandleChanged()=%d", Wd().HelpChanged ? 1 : 0);
	return Wd().HelpChanged;
}
static void F_InitHelp() { Call("InitHelp()"); }
static void F_AddEventHelp(int Number) { Call("AddEventHelp(%d)", Number); }
static const char *F_GetCurrentHelpText()
{
	Call("GetCurrentHelpText()");
	return "The current help text.";
}

// Inventory.cpp
static void F_ClearOffers() { Call("ClearOffers()"); }
static void F_SetBodyObject(int Position, TObject O) { Call("SetBodyObject(%d,%s)", Position, Obj(O).c_str()); }
static void F_DeleteBodyObject(int Position) { Call("DeleteBodyObject(%d)", Position); }
static void F_OpenContainer(int Container, TObject O, const char *Name, int Capacity, bool HasParent)
{
	Call("OpenContainer(%d,%s,%s,%d,%d)", Container, Obj(O).c_str(), Quote(Name).c_str(), Capacity, HasParent ? 1 : 0);
	Wd().OpenContainers |= 1u << (Container & 31);
}
static void F_CloseContainer(int Container)
{
	Call("CloseContainer(%d)", Container);
	Wd().OpenContainers &= ~(1u << (Container & 31));
}
static bool F_IsContainerOpen(int Container)
{
	bool Result = (Wd().OpenContainers >> (Container & 31) & 1) != 0;
	Call("IsContainerOpen(%d)=%d", Container, Result ? 1 : 0);
	return Result;
}
static void F_AppendContainerObject(int Container, TObject O)
{
	Call("AppendContainerObject(%d,%s)", Container, Obj(O).c_str());
}
static void F_ChangeContainerObject(int Container, int Position, TObject O)
{
	Call("ChangeContainerObject(%d,%d,%s)", Container, Position, Obj(O).c_str());
}
static bool F_DeleteContainerObject(int Container, int Position)
{
	bool Result = Hash(40, Container, Position) % 3 == 0;
	Call("DeleteContainerObject(%d,%d)=%d", Container, Position, Result ? 1 : 0);
	return Result;
}
static bool F_SetOwnOffer(const char *Name)
{
	bool Result = HashText(Name) % 2 == 0;
	Call("SetOwnOffer(%s)=%d", Quote(Name).c_str(), Result ? 1 : 0);
	return Result;
}
static bool F_SetCounterOffer(const char *Name)
{
	bool Result = (HashText(Name) ^ Salt) % 3 != 0;
	Call("SetCounterOffer(%s)=%d", Quote(Name).c_str(), Result ? 1 : 0);
	return Result;
}
static void F_AddOwnOfferObject(TObject O) { Call("AddOwnOfferObject(%s)", Obj(O).c_str()); }
static void F_AddCounterOfferObject(TObject O) { Call("AddCounterOfferObject(%s)", Obj(O).c_str()); }
static void F_InsertContainerObject(int Container, TObject O)
{
	Call("InsertContainerObject(%d,%s)", Container, Obj(O).c_str());
}
static void F_InitInventory() { Call("InitInventory()"); }
static void F_ResetInventory() { Call("ResetInventory()"); }

// Objects.cpp
static bool F_GetObjectFlag(int Type, int Flag)
{
	bool Result = ObjectFlag(Type, Flag);
	Call("GetObjectFlag(%d,%d)=%d", Type, Flag, Result ? 1 : 0);
	return Result;
}
static int F_GetObjectWaypoints(int Type)
{
	int Result = ObjectWaypoints(Type);
	Call("GetObjectWaypoints(%d)=%d", Type, Result);
	return Result;
}
static void F_InitObjects() { Call("InitObjects()"); }
static void F_ExitObjects() { Call("ExitObjects()"); }
static void F_ReloadObjects() { Call("ReloadObjects()"); }

// Preferences.cpp
static bool F_GetAutoChaseOff()
{
	Call("GetAutoChaseOff()=%d", Wd().AutoChaseOff ? 1 : 0);
	return Wd().AutoChaseOff;
}
static void F_SetChaseMode(int Value)
{
	Call("SetChaseMode(%d)", Value);
	Wd().ChaseMode = Value;
}
static int F_GetAttackMode()
{
	Call("GetAttackMode()=%d", Wd().AttackMode);
	return Wd().AttackMode;
}
static int F_GetChaseMode()
{
	Call("GetChaseMode()=%d", Wd().ChaseMode);
	return Wd().ChaseMode;
}
static bool F_GetSecureMode()
{
	Call("GetSecureMode()=%d", Wd().SecureMode);
	return Wd().SecureMode != 0;
}
static bool F_SetMotD(const char *Text)
{
	bool Result = HashText(Text) % 2 == 0;
	Call("SetMotD(%s)=%d", Quote(Text).c_str(), Result ? 1 : 0);
	return Result;
}
static void F_SetEngine(int Value) { Call("SetEngine(%d)", Value); }
static int F_GetViewMode()
{
	Call("GetViewMode()=%d", Wd().ViewMode);
	return Wd().ViewMode;
}
static TFullscreenMode *F_GetFullscreenMode()
{
	Call("GetFullscreenMode()");
	return &Wd().Fullscreen;
}
static bool F_GetWindowsMousepointer()
{
	Call("GetWindowsMousepointer()=%d", Wd().WindowsMousepointer ? 1 : 0);
	return Wd().WindowsMousepointer;
}
static const char *F_GetMotD()
{
	Call("GetMotD()");
	return "The message of the day.";
}
static bool F_GetClassicControl()
{
	Call("GetClassicControl()=%d", Wd().ClassicControl ? 1 : 0);
	return Wd().ClassicControl;
}
static void F_InitPreferences() { Call("InitPreferences()"); }
static void F_SavePreferences() { Call("SavePreferences()"); }
static void F_ExitPreferences() { Call("ExitPreferences()"); }

// Script.cpp
static uint32 Checksum(const uint8 *Data, int Size)
{
	uint32 h = 2166136261u;
	for (int i = 0; i < Size; i++)
		h = (h ^ Data[i]) * 16777619u;
	return h;
}
static bool F_ApplyPatch(const char *FileName, const uint8 *Patch, int Size, uint32 Signature)
{
	bool Result = (Checksum(Patch, Size) ^ Salt) % 3 != 0;
	Call("ApplyPatch(%s,%08x,%d,%08x)=%d", Quote(FileName).c_str(), Checksum(Patch, Size), Size, Signature, Result ? 1 : 0);
	return Result;
}
static bool F_ApplyFullPatch(const char *FileName, const uint8 *Data, int Size)
{
	bool Result = (Checksum(Data, Size) ^ Salt) % 3 != 0;
	Call("ApplyFullPatch(%s,%08x,%d)=%d", Quote(FileName).c_str(), Checksum(Data, Size), Size, Result ? 1 : 0);
	return Result;
}
static void F_InstallPatch(const char *FileName) { Call("InstallPatch(%s)", Quote(FileName).c_str()); }

// Status.cpp
static void F_ClearRights() { Call("ClearRights()"); }
static void F_SetActionState(int State)
{
	Call("SetActionState(%d)", State);
	Wd().ActionState = State;
}
static void F_SetConnectionState(int State)
{
	Call("SetConnectionState(%d)", State);
	Wd().ConnectionState = State;
}
static void F_SetProgress(int Progress) { Call("SetProgress(%d)", Progress); }
static void F_SetAccountCharacters(int Number) { Call("SetAccountCharacters(%d)", Number); }
static void F_SetPremiumDays(int Days) { Call("SetPremiumDays(%d)", Days); }
static bool F_GetGamemaster()
{
	bool Result = Salt % 3 == 0;
	Call("GetGamemaster()=%d", Result ? 1 : 0);
	return Result;
}
static void F_ClearMessages()
{
	Call("ClearMessages()");
	for (const TQueuedEvent &E : Wd().Events)
		FreeEvent(E);
	Wd().Events.clear();
}
static void F_SetRight(int RightID, int Value) { Call("SetRight(%d,%d)", RightID, Value); }
static void F_AddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	// Events 0x2a and 0x2b (the error report dialog at the start, the error reports sent when the game starts) are
	// gone from ours (Patella-Modern issue #74): the original's posts of them (ReceivePackets' game start, 0x0a) are
	// neither a call nor an event of the world, so both sides' logs and queues stay comparable.
	if (Type == 0x2a || Type == 0x2b)
		return;
	TQueuedEvent E = {Type, Param1, Param2};
	std::string Text = "AddMessage";
	DescribeEvent(Text, E);
	Call("%s", Text.c_str());
	Wd().Events.push_back(E);
}
static void F_AddMessageFirst(int Type, INT_PTR Param1, INT_PTR Param2)
{
	TQueuedEvent E = {Type, Param1, Param2};
	std::string Text = "AddMessageFirst";
	DescribeEvent(Text, E);
	Call("%s", Text.c_str());
	Wd().Events.insert(Wd().Events.begin(), E);
}
static void F_SetCharacter(int Number, const char *Name, const char *GameWorld, const uint8 *Address, int Port)
{
	Call("SetCharacter(%d,%s,%s,%s,%d)", Number, Quote(Name).c_str(), Quote(GameWorld).c_str(), Hex(Address, 4).c_str(), Port);
}
static int F_GetActionNumber()
{
	Call("GetActionNumber()=%d", Wd().ActionNumber);
	return Wd().ActionNumber;
}
static void F_IncrementActionNumber()
{
	Call("IncrementActionNumber()");
	Wd().ActionNumber++;
}
static int F_GetActionState()
{
	AssertionGetter = true;
	Call("GetActionState()=%d", Wd().ActionState);
	AssertionGetter = false;
	return Wd().ActionState;
}
static void F_SetMousePosition(int x, int y)
{
	Call("SetMousePosition(%d,%d)", x, y);
	Wd().MouseX = x;
	Wd().MouseY = y;
}
static int F_GetMouseX()
{
	Call("GetMouseX()=%d", Wd().MouseX);
	return Wd().MouseX;
}
static int F_GetMouseY()
{
	Call("GetMouseY()=%d", Wd().MouseY);
	return Wd().MouseY;
}
static void F_SetMouseCursor(int Cursor)
{
	Call("SetMouseCursor(%d)", Cursor);
	Wd().MouseCursor = Cursor;
}
static int F_GetMouseCursor()
{
	Call("GetMouseCursor()=%d", Wd().MouseCursor);
	return Wd().MouseCursor;
}
static void F_SetPendingAction(int State, int Number)
{
	Call("SetPendingAction(%d,%d)", State, Number);
	Wd().PendingState = State;
	Wd().PendingNumber = Number;
}
static int F_GetPendingActionState()
{
	Call("GetPendingActionState()=%d", Wd().PendingState);
	return Wd().PendingState;
}
static int F_GetPendingActionNumber()
{
	Call("GetPendingActionNumber()=%d", Wd().PendingNumber);
	return Wd().PendingNumber;
}
static int F_GetWindowWidth()
{
	Call("GetWindowWidth()=%d", Wd().Main->Width);
	return Wd().Main->Width;
}
static int F_GetWindowHeight()
{
	Call("GetWindowHeight()=%d", Wd().Main->Height);
	return Wd().Main->Height;
}
static void F_SetDragStart(int x, int y) { Call("SetDragStart(%d,%d)", x, y); }
static void F_CheckDragStart(int x, int y) { Call("CheckDragStart(%d,%d)", x, y); }
static int F_GetConnectionState()
{
	Call("GetConnectionState()=%d", Wd().ConnectionState);
	return Wd().ConnectionState;
}
static int F_GetAccountNumber()
{
	Call("GetAccountNumber()");
	return 1234567 + (int)(Salt % 1000);
}
static const char *F_GetAccountPassword()
{
	Call("GetAccountPassword()");
	return Salt % 5 == 0 ? "" : "a secret";
}
static int F_GetAccountCharacters()
{
	AssertionGetter = true;
	Call("GetAccountCharacters()=%d", Wd().AccountCharacters);
	AssertionGetter = false;
	return Wd().AccountCharacters;
}
static int F_GetActiveCharacter()
{
	AssertionGetter = true;
	Call("GetActiveCharacter()=%d", Wd().ActiveCharacter);
	AssertionGetter = false;
	return Wd().ActiveCharacter;
}
static void F_SetMouseOverText(const char *Text)
{
	Call("SetMouseOverText(%s)", Quote(Text).c_str());
	Wd().MouseOverText = Text;
}
static void F_SetMouseOverTextPosition(int x, int y) { Call("SetMouseOverTextPosition(%d,%d)", x, y); }
static const char *F_GetMouseOverText()
{
	Call("GetMouseOverText()=%s", Quote(Wd().MouseOverText).c_str());
	return Wd().MouseOverText;
}
static void F_GetMouseOverTextPosition(int *x, int *y)
{
	Call("GetMouseOverTextPosition()=%d,%d", Wd().MouseOverX, Wd().MouseOverY);
	*x = Wd().MouseOverX;
	*y = Wd().MouseOverY;
}
static void F_UpdateBlinkState() { Call("UpdateBlinkState()"); }
static int F_PeekMessageType()
{
	int Type = Wd().Events.empty() ? 0 : Wd().Events.front().Type;
	Call("PeekMessageType()=%d", Type);
	return Type;
}
static void F_FetchMessage(int *Type, INT_PTR *Param1, INT_PTR *Param2)
{
	if (Wd().Events.empty()) {
		Call("FetchMessage()=empty");
		*Type = 0;
		*Param1 = *Param2 = 0;
		return;
	}
	TQueuedEvent E = Wd().Events.front();
	Wd().Events.erase(Wd().Events.begin());
	std::string Text = "FetchMessage()=";
	DescribeEvent(Text, E);
	Call("%s", Text.c_str());
	*Type = E.Type;
	*Param1 = E.Param1;
	*Param2 = E.Param2;
}
static void F_ClearErrorReports() { Call("ClearErrorReports()"); }
static int F_GetNumberOfErrorReports()
{
	Call("GetNumberOfErrorReports()=%d", Wd().ErrorReports);
	return Wd().ErrorReports;
}
static const char *ReportText(const char *What, int Number)
{
	static char Texts[2][4][64];
	char *T = Texts[Side][(Number & 3)];
	sprintf(T, "%s %d", What, Number);
	return T;
}
static const char *F_GetErrorReportTitle(int Number)
{
	Call("GetErrorReportTitle(%d)", Number);
	return ReportText("Title", Number);
}
static const char *F_GetErrorReportDate(int Number)
{
	Call("GetErrorReportDate(%d)", Number);
	return ReportText("Date", Number);
}
static const char *F_GetErrorReportText(int Number)
{
	Call("GetErrorReportText(%d)", Number);
	return ReportText("Text", Number);
}
static const char *F_GetErrorReportComment(int Number)
{
	Call("GetErrorReportComment(%d)", Number);
	return ReportText("Comment", Number);
}
static void F_ExitStatus() { Call("ExitStatus()"); }
static const char *F_GetCharacterName(int Number)
{
	Call("GetCharacterName(%d)", Number);
	return ReportText("Character", Number);
}
static void F_InitStatus(bool Gamemaster, void (*Handler)())
{
	Call("InitStatus(%d,%s)", Gamemaster ? 1 : 0, NameOf((void *)Handler).c_str());
}

// Talk.cpp
static void F_SetTalkChanged() { Call("SetTalkChanged()"); }
static void F_SetTalkThrottle() { Call("SetTalkThrottle()"); }
static void F_SetConsoleChanged() { Call("SetConsoleChanged()"); }
static void F_SetStatusMessage(const char *Message, bool Permanent)
{
	Call("SetStatusMessage(%s,%d)", Quote(Message).c_str(), Permanent ? 1 : 0);
}
static bool F_IsTalkIgnored(int Mode, const char *Name, int Channel)
{
	bool Result = (HashText(Name) ^ Hash(60, Mode, Channel)) % 4 == 0;
	Call("IsTalkIgnored(%d,%s,%d)=%d", Mode, Quote(Name).c_str(), Channel, Result ? 1 : 0);
	return Result;
}
static TTalkBuffer *F_GetTextBuffer(int Mode, const char *Name, int Channel)
{
	uint32 h = HashText(Name) ^ Hash(61, Mode, Channel);
	TTalkBuffer *Result = h % 5 == 0 ? NULL : Wd().Buffers[(h >> 4) % WORLD_BUFFERS];
	Call("GetTextBuffer(%d,%s,%d)=%s", Mode, Quote(Name).c_str(), Channel, NameOf(Result).c_str());
	return Result;
}
static void F_AddTalkbox(int Mode, const char *Speaker, int x, int y, int z, const char *Text)
{
	Call("AddTalkbox(%d,%s,%d,%d,%d,%s)", Mode, Quote(Speaker).c_str(), x, y, z, Quote(Text).c_str());
}
static void F_AddMessageTalkbox(int Mode, const char *Text) { Call("AddMessageTalkbox(%d,%s)", Mode, Quote(Text).c_str()); }
static void F_AddTextBufferLine(TTalkBuffer *Buffer, uint32 StatementID, const char *Text, int TalkMode,
	const char *Name, int Time)
{
	Call("AddTextBufferLine(%s,%u,%s,%d,%s,%d)", NameOf(Buffer).c_str(), StatementID, Quote(Text).c_str(), TalkMode,
		Quote(Name).c_str(), Time);
}
static void F_AddConsoleText(uint32 StatementID, const char *Text, int TalkMode, const char *Name, int Channel, int Time)
{
	Call("AddConsoleText(%u,%s,%d,%s,%d,%d)", StatementID, Quote(Text).c_str(), TalkMode, Quote(Name).c_str(), Channel,
		Time);
}
static TTalkBuffer *F_CreateTextBuffer(int ReceiveTalkMode, int SendTalkMode, const char *Name, int Channel)
{
	uint32 h = Hash(62, ReceiveTalkMode, SendTalkMode, Channel) ^ HashText(Name);
	TTalkBuffer *Result = h % 3 == 0 ? NULL : Wd().Buffers[(h >> 4) % WORLD_BUFFERS];
	Call("CreateTextBuffer(%d,%d,%s,%d)=%s", ReceiveTalkMode, SendTalkMode, Quote(Name).c_str(), Channel,
		NameOf(Result).c_str());
	return Result;
}
static void F_UpdateTalkboxes() { Call("UpdateTalkboxes()"); }
static void F_UpdateStatusMessage() { Call("UpdateStatusMessage()"); }
static void F_AddTemporaryIgnore(const char *Addressee) { Call("AddTemporaryIgnore(%s)", Quote(Addressee).c_str()); }
static void F_RemoveTemporaryIgnore(const char *Addressee) { Call("RemoveTemporaryIgnore(%s)", Quote(Addressee).c_str()); }
static void F_ResetTalk() { Call("ResetTalk()"); }
static void F_InitTalk() { Call("InitTalk()"); }
static void F_ShowPermanentIgnoreWarning() { Call("ShowPermanentIgnoreWarning()"); }
static void F_ExitTalk() { Call("ExitTalk()"); }

// graphics.cpp
static void F_ExitGraphics() { Call("ExitGraphics()"); }
static void F_DrawPicturePart(int Surface, int x, int y, int PictureNumber, int SourceX, int SourceY, int Width,
	int Height)
{
	Call("DrawPicturePart(%d,%d,%d,%d,%d,%d,%d,%d)", Surface, x, y, PictureNumber, SourceX, SourceY, Width, Height);
}
static int F_GetTextHeight(int FontNumber, const char *Text)
{
	int Result = (int)(HashText(Text) % 40);
	Call("GetTextHeight(%d,%s)=%d", FontNumber, Quote(Text).c_str(), Result);
	return Result;
}
static void F_ResetVideoCache() { Call("ResetVideoCache()"); }
static void F_ReloadSprites() { Call("ReloadSprites()"); }
static void F_PrintText(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text, int Alignment)
{
	Call("PrintText(%d,%d,%d,%d,%d,%d,%d,%s,%d)", Surface, x, y, FontNumber, Color.R, Color.G, Color.B,
		Quote(Text).c_str(), Alignment);
}
static void F_PrintFormattedText(int Surface, int x, int y, TFont Format, const char *Text)
{
	Call("PrintFormattedText(%d,%d,%d,%d,%d,%d,%d,%d,%s)", Surface, x, y, Format.FontNumber, Format.Color.R,
		Format.Color.G, Format.Color.B, Format.Alignment, Quote(Text).c_str());
}
static int F_GetTextWidth(int FontNumber, const char *Text)
{
	int Result = (int)(HashText(Text) % 2000);
	Call("GetTextWidth(%d,%s)=%d", FontNumber, Quote(Text).c_str(), Result);
	return Result;
}
static void F_ReloadPictures() { Call("ReloadPictures()"); }
static void F_InitGraphics(int MemoryCacheBytes) { Call("InitGraphics(%d)", MemoryCacheBytes); }

// GUI.cpp; 0x496160 is the one copy of five empty Exit functions.
static void F_InitHotkeys() { Call("InitHotkeys()"); }
static void F_EmptyExit() { Call("EmptyExit()"); }
static void F_InitSkinProperties() { Call("InitSkinProperties()"); }
static bool F_CheckHotkey(int Key)
{
	bool Result = Hash(70, Key) % 2 == 0;
	Call("CheckHotkey(%d)=%d", Key, Result ? 1 : 0);
	return Result;
}
static void F_UpdateTimers() { Call("UpdateTimers()"); }

// MainWindow.cpp
static void F_ExitMainWindow() { Call("ExitMainWindow()"); }
static void GOLDEN_THISCALL F_MainOpenContainer(GOLDEN_THIS_NAMED(TMainWindow *This), int Container)
{
	Call("%s.OpenContainer(%d)", NameOf(This).c_str(), Container);
}
static void GOLDEN_THISCALL F_MainCloseContainer(GOLDEN_THIS_NAMED(TMainWindow *This), int Container)
{
	Call("%s.CloseContainer(%d)", NameOf(This).c_str(), Container);
}
static void GOLDEN_THISCALL F_MainUpdateTrade(GOLDEN_THIS_NAMED(TMainWindow *This)) { Call("%s.UpdateTrade()", NameOf(This).c_str()); }
static void GOLDEN_THISCALL F_MainSetConsoleActive(GOLDEN_THIS_NAMED(TMainWindow *This), bool Active)
{
	Call("%s.SetConsoleActive(%d)", NameOf(This).c_str(), Active ? 1 : 0);
}
static void GOLDEN_THISCALL F_MainChangeMode(GOLDEN_THIS_NAMED(TMainWindow *This), bool InGame)
{
	Call("%s.ChangeMode(%d)", NameOf(This).c_str(), InGame ? 1 : 0);
}
static void GOLDEN_THISCALL F_MainOpenTrade(GOLDEN_THIS_NAMED(TMainWindow *This)) { Call("%s.OpenTrade()", NameOf(This).c_str()); }
static void GOLDEN_THISCALL F_MainCloseTrade(GOLDEN_THIS_NAMED(TMainWindow *This)) { Call("%s.CloseTrade()", NameOf(This).c_str()); }

// ConsoleWindow.cpp
static void GOLDEN_THISCALL F_AddChannel(GOLDEN_THIS_NAMED(TConsoleWindow *This), TTalkBuffer *Buffer, const char *Caption)
{
	Call("%s.AddChannel(%s,%s)", NameOf(This).c_str(), NameOf(Buffer).c_str(), Quote(Caption).c_str());
}
static TTalkBuffer *GOLDEN_THISCALL F_OpenChannel(GOLDEN_THIS_NAMED(TConsoleWindow *This), int RecvMode, int SendMode, const char *Name,
	int Channel, const char *Caption)
{
	TTalkBuffer *Result = Wd().Buffers[Hash(80, RecvMode, Channel) % WORLD_BUFFERS];
	Call("%s.OpenChannel(%d,%d,%s,%d,%s)=%s", NameOf(This).c_str(), RecvMode, SendMode, Quote(Name).c_str(), Channel,
		Quote(Caption).c_str(), NameOf(Result).c_str());
	return Result;
}
static void GOLDEN_THISCALL F_ReportRuleViolation(GOLDEN_THIS_NAMED(TConsoleWindow *This), const char *Question)
{
	Call("%s.ReportRuleViolation(%s)", NameOf(This).c_str(), Quote(Question).c_str());
}

// ContextMenus.cpp: the constructors build a TCtlWindow in the memory the
// caller allocated (at least 0x30 bytes).
static void *GOLDEN_THISCALL F_NewHelpBubble(GOLDEN_THIS_NAMED(void *This), const char *HelpText)
{
	TCtlWindow *W = NewWindow(This);
	Call("new THelpBubble(%s)=%s", Quote(HelpText).c_str(), NameOf(W).c_str());
	return This;
}
static void GOLDEN_THISCALL F_HelpBubblePlace(GOLDEN_THIS_NAMED(void *This), TWindow *Window)
{
	Call("%s.Place(%s)", NameOf(This).c_str(), NameOf(Window).c_str());
}
static void *GOLDEN_THISCALL F_NewObjectMenu(GOLDEN_THIS_NAMED(void *This), int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TObjectContextMenu(%d,%d,%d,%d)=%s", x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewCreatureMenu(GOLDEN_THIS_NAMED(void *This), uint32 ID, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TCreatureContextMenu(%u,%d,%d,%d,%d)=%s", ID, x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewBuddyMenu(GOLDEN_THIS_NAMED(void *This), uint32 ID, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TBuddyContextMenu(%u,%d,%d,%d,%d)=%s", ID, x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewTextMenu(GOLDEN_THIS_NAMED(void *This), void *TextWindow, int Line, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TTextContextMenu(%s,%d,%d,%d,%d,%d)=%s", NameOf(TextWindow).c_str(), Line, x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewChannelMenu(GOLDEN_THIS_NAMED(void *This), void *Console, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TChannelContextMenu(%s,%d,%d,%d,%d)=%s", NameOf(Console).c_str(), x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewRequestMenu(GOLDEN_THIS_NAMED(void *This), void *Console, int Line, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TRequestContextMenu(%s,%d,%d,%d,%d,%d)=%s", NameOf(Console).c_str(), Line, x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewEditMenu(GOLDEN_THIS_NAMED(void *This), void *Edit, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TEditContextMenu(%s,%d,%d,%d,%d)=%s", NameOf(Edit).c_str(), x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewMarkMenu(GOLDEN_THIS_NAMED(void *This), void *Data, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TMarkContextMenu(%s,%d,%d,%d,%d)=%s", NameOf(Data).c_str(), x, y, PW, PH, NameOf(W).c_str());
	return This;
}
static void *GOLDEN_THISCALL F_NewSkillMenu(GOLDEN_THIS_NAMED(void *This), void *Skills, int x, int y, int PW, int PH)
{
	TCtlWindow *W = NewWindow(This);
	Call("new TSkillContextMenu(%s,%d,%d,%d,%d)=%s", NameOf(Skills).c_str(), x, y, PW, PH, NameOf(W).c_str());
	return This;
}
#if defined(_MSC_VER)
extern "C" void OurHelpBubbleConstructor();
extern "C" void OurObjectMenuConstructor();
extern "C" void OurCreatureMenuConstructor();
extern "C" void OurBuddyMenuConstructor();
extern "C" void OurTextMenuConstructor();
extern "C" void OurChannelMenuConstructor();
extern "C" void OurRequestMenuConstructor();
extern "C" void OurEditMenuConstructor();
extern "C" void OurMarkMenuConstructor();
extern "C" void OurSkillMenuConstructor();
#endif
// Our constructors through their decorated names: x86 (the oracle)...
#if defined(_MSC_VER)
// x64 (the runner, MSVC only): the same constructors' x64 decorated names (as elementfakes.cpp's, issue #28).
#pragma comment(linker, "/alternatename:OurHelpBubbleConstructor=??0THelpBubble@@QEAA@PEBD@Z")
#pragma comment(linker, "/alternatename:OurObjectMenuConstructor=??0TObjectContextMenu@@QEAA@HHHH@Z")
#pragma comment(linker, "/alternatename:OurCreatureMenuConstructor=??0TCreatureContextMenu@@QEAA@IHHHH@Z")
#pragma comment(linker, "/alternatename:OurBuddyMenuConstructor=??0TBuddyContextMenu@@QEAA@IHHHH@Z")
#pragma comment(linker, "/alternatename:OurTextMenuConstructor=??0TTextContextMenu@@QEAA@PEAVTTextWindow@@HHHHH@Z")
#pragma comment(linker, "/alternatename:OurChannelMenuConstructor=??0TChannelContextMenu@@QEAA@PEAVTConsoleWindow@@HHHH@Z")
#pragma comment(linker, "/alternatename:OurRequestMenuConstructor=??0TRequestContextMenu@@QEAA@PEAVTConsoleWindow@@HHHHH@Z")
#pragma comment(linker, "/alternatename:OurEditMenuConstructor=??0TEditContextMenu@@QEAA@PEAVTEditLine@@HHHH@Z")
#pragma comment(linker, "/alternatename:OurMarkMenuConstructor=??0TMarkContextMenu@@QEAA@PEAUTMarkMenuData@@HHHH@Z")
#pragma comment(linker, "/alternatename:OurSkillMenuConstructor=??0TSkillContextMenu@@QEAA@PEAVTSkillsWindowBody@@HHHH@Z")
#else
// Linux (issue #91): the Itanium ABI's names (golden_test.h GOLDEN_SYMBOL).
GOLDEN_SYMBOL(OurHelpBubbleConstructor, "_ZN11THelpBubbleC1EPKc");
GOLDEN_SYMBOL(OurObjectMenuConstructor, "_ZN18TObjectContextMenuC1Eiiii");
GOLDEN_SYMBOL(OurCreatureMenuConstructor, "_ZN20TCreatureContextMenuC1Ejiiii");
GOLDEN_SYMBOL(OurBuddyMenuConstructor, "_ZN17TBuddyContextMenuC1Ejiiii");
GOLDEN_SYMBOL(OurTextMenuConstructor, "_ZN16TTextContextMenuC1EP11TTextWindowiiiii");
GOLDEN_SYMBOL(OurChannelMenuConstructor, "_ZN19TChannelContextMenuC1EP14TConsoleWindowiiii");
GOLDEN_SYMBOL(OurRequestMenuConstructor, "_ZN19TRequestContextMenuC1EP14TConsoleWindowiiiii");
GOLDEN_SYMBOL(OurEditMenuConstructor, "_ZN16TEditContextMenuC1EP9TEditLineiiii");
GOLDEN_SYMBOL(OurMarkMenuConstructor, "_ZN16TMarkContextMenuC1EP13TMarkMenuDataiiii");
GOLDEN_SYMBOL(OurSkillMenuConstructor, "_ZN17TSkillContextMenuC1EP17TSkillsWindowBodyiiii");
#endif

// Dialogs.cpp: every creator returns a new window of the side.
#define DIALOG_RESULT(Text) \
	TCtlWindow *W = NewWindow(); \
	Call("%s=%s", (Text).c_str(), NameOf(W).c_str()); \
	return (TDialog *)(TWindow *)W
static std::string Fmt(const char *Format, ...)
{
	char Buffer[2048];
	va_list Args;
	va_start(Args, Format);
	_vsnprintf(Buffer, sizeof(Buffer) - 1, Format, Args);
	va_end(Args);
	Buffer[sizeof(Buffer) - 1] = 0;
	return Buffer;
}
static std::string Fn(void *Function) { return NameOf(Function); }
static TDialog *F_CreateMessageDialog(const char *Caption, const char *Text)
{
	DIALOG_RESULT(Fmt("CreateMessageDialog(%s,%s)", Quote(Caption).c_str(), Quote(Text).c_str()));
}
static TDialog *F_CreateFormattedMessageDialog(const char *Caption, const char *Text)
{
	DIALOG_RESULT(Fmt("CreateFormattedMessageDialog(%s,%s)", Quote(Caption).c_str(), Quote(Text).c_str()));
}
static TDialog *F_CreateConnectingDialog(const char *Caption, const char *Text, TComponent *Receiver)
{
	DIALOG_RESULT(Fmt("CreateConnectingDialog(%s,%s,%s)", Quote(Caption).c_str(), Quote(Text).c_str(), NameOf(Receiver).c_str()));
}
static TDialog *F_CreateWaitDialog(const char *Caption, const char *Text, int Timeout)
{
	DIALOG_RESULT(Fmt("CreateWaitDialog(%s,%s,%d)", Quote(Caption).c_str(), Quote(Text).c_str(), Timeout));
}
static TDialog *F_CreateReconnectDialog(const char *Caption, const char *Text, int Timeout)
{
	DIALOG_RESULT(Fmt("CreateReconnectDialog(%s,%s,%d)", Quote(Caption).c_str(), Quote(Text).c_str(), Timeout));
}
static TDialog *F_CreateQuestionDialog(const char *Caption, const char *Text, TComponent *Receiver, int YesEvent, int NoEvent)
{
	DIALOG_RESULT(Fmt("CreateQuestionDialog(%s,%s,%s,%d,%d)", Quote(Caption).c_str(), Quote(Text).c_str(),
		NameOf(Receiver).c_str(), YesEvent, NoEvent));
}
static TDialog *F_CreateLogoutDialog(const char *Caption, const char *Text, TComponent *Receiver)
{
	DIALOG_RESULT(Fmt("CreateLogoutDialog(%s,%s,%s)", Quote(Caption).c_str(), Quote(Text).c_str(), NameOf(Receiver).c_str()));
}
static TDialog *F_CreateInfoDialog(void (*OpenURL)(const char *)) { DIALOG_RESULT(Fmt("CreateInfoDialog(%s)", Fn((void *)OpenURL).c_str())); }
static TDialog *F_CreateMoveObjectsDialog() { DIALOG_RESULT(Fmt("CreateMoveObjectsDialog()")); }
static TDialog *F_CreateLoginDialog(void (*OpenURL)(const char *)) { DIALOG_RESULT(Fmt("CreateLoginDialog(%s)", Fn((void *)OpenURL).c_str())); }
static TDialog *F_CreateCharacterListDialog(int Timeout) { DIALOG_RESULT(Fmt("CreateCharacterListDialog(%d)", Timeout)); }
static TDialog *F_CreateTextDialog(const int *ObjectData, const char *Author, const char *Text)
{
	DIALOG_RESULT(Fmt("CreateTextDialog(%d,%d,%d,%s,%s)", ObjectData[0], ObjectData[1], ObjectData[2], Quote(Author).c_str(),
		Quote(Text).c_str()));
}
static TDialog *F_CreateEditListDialog(int *HouseData, const char *Text)
{
	DIALOG_RESULT(Fmt("CreateEditListDialog(%d,%d,%s)", HouseData[0], HouseData[1], Quote(Text).c_str()));
}
static TDialog *F_CreateOutfitDialog(int *Data)
{
	DIALOG_RESULT(Fmt("CreateOutfitDialog(%d,%d,%d,%d,%d,%d,%d)", Data[0], Data[1], Data[2], Data[3], Data[4], Data[5], Data[6]));
}
static TDialog *F_CreateBugReportDialog() { DIALOG_RESULT(Fmt("CreateBugReportDialog()")); }
static TDialog *F_CreateNewChannelDialog(int NumberOfChannels, int *ChannelIDs, const char (*ChannelNames)[30])
{
	std::string T = Fmt("CreateNewChannelDialog(%d", NumberOfChannels);
	for (int i = 0; i < NumberOfChannels && i < 300; i++)
		T += Fmt(",%d:%s", ChannelIDs[i], Quote(ChannelNames[i]).c_str());
	if (ChannelNames == NULL)
		T += ",NULL";
	T += ")";
	DIALOG_RESULT(T);
}
static TDialog *F_CreateInviteDialog() { DIALOG_RESULT(Fmt("CreateInviteDialog()")); }
static TDialog *F_CreateExcludeDialog() { DIALOG_RESULT(Fmt("CreateExcludeDialog()")); }
static TDialog *F_CreateIgnoreListDialog() { DIALOG_RESULT(Fmt("CreateIgnoreListDialog()")); }
static TDialog *F_CreateReportDialog() { DIALOG_RESULT(Fmt("CreateReportDialog()")); }
static TDialog *F_CreateAddBuddyDialog() { DIALOG_RESULT(Fmt("CreateAddBuddyDialog()")); }
static TDialog *F_CreateEditBuddyDialog(int CharacterID) { DIALOG_RESULT(Fmt("CreateEditBuddyDialog(%d)", CharacterID)); }
static TDialog *F_CreateUpdateDialog(const char *FileName, int DataSize)
{
	DIALOG_RESULT(Fmt("CreateUpdateDialog(%s,%d)", Quote(FileName).c_str(), DataSize));
}
static TDialog *F_CreateHelpDialog(void (*OpenURL)(const char *)) { DIALOG_RESULT(Fmt("CreateHelpDialog(%s)", Fn((void *)OpenURL).c_str())); }
static TDialog *F_CreateHintsDialog() { DIALOG_RESULT(Fmt("CreateHintsDialog()")); }
static TDialog *F_CreateEditMarkDialog(TCoordinate *Coordinate, TMark *Mark)
{
	DIALOG_RESULT(Fmt("CreateEditMarkDialog(%s,%s)", NameOf(Coordinate).c_str(), NameOf(Mark).c_str()));
}
static TDialog *F_CreateOptionsDialog() { DIALOG_RESULT(Fmt("CreateOptionsDialog()")); }
static TDialog *F_CreateGeneralOptionsDialog(void (*OpenURL)(const char *))
{
	DIALOG_RESULT(Fmt("CreateGeneralOptionsDialog(%s)", Fn((void *)OpenURL).c_str()));
}
static TDialog *F_CreateGraphicsOptionsDialog(void (*OpenURL)(const char *), void (*SetDisplayMode)(int), TWindow *Parent)
{
	DIALOG_RESULT(Fmt("CreateGraphicsOptionsDialog(%s,%s,%s)", Fn((void *)OpenURL).c_str(), Fn((void *)SetDisplayMode).c_str(),
		NameOf(Parent).c_str()));
}
static TDialog *F_CreateAdvancedGraphicsDialog(void (*OpenURL)(const char *), TComponent *Receiver)
{
	DIALOG_RESULT(Fmt("CreateAdvancedGraphicsDialog(%s,%s)", Fn((void *)OpenURL).c_str(), NameOf(Receiver).c_str()));
}
static TDialog *F_CreateConsoleOptionsDialog(void (*OpenURL)(const char *))
{
	DIALOG_RESULT(Fmt("CreateConsoleOptionsDialog(%s)", Fn((void *)OpenURL).c_str()));
}
static TDialog *F_CreateNetworkOptionsDialog(void (*OpenURL)(const char *))
{
	DIALOG_RESULT(Fmt("CreateNetworkOptionsDialog(%s)", Fn((void *)OpenURL).c_str()));
}
static TDialog *F_CreateHotkeyDialog(void (*OpenURL)(const char *)) { DIALOG_RESULT(Fmt("CreateHotkeyDialog(%s)", Fn((void *)OpenURL).c_str())); }
static TDialog *F_CreateErrorReportDialog() { DIALOG_RESULT(Fmt("CreateErrorReportDialog()")); }
static TDialog *F_CreateRuleViolationDialog(const char *Name, TTalkLine *Statement)
{
	DIALOG_RESULT(Fmt("CreateRuleViolationDialog(%s,%s)", Quote(Name).c_str(), NameOf(Statement).c_str()));
}

// Skins.cpp, Surface.cpp, sysinc.cpp
static void F_InitSkins() { Call("InitSkins()"); }
static void F_ExitSkins() { Call("ExitSkins()"); }
static TSurface *F_GetSurface()
{
	Call("GetSurface()");
	return Wd().Surface;
}
static void F_SurfaceExitInstance() { Call("SurfaceExitInstance()"); }
static int F_GetMilliseconds()
{
	Call("GetMilliseconds()=%d", Wd().Milliseconds);
	return Wd().Milliseconds;
}
static bool F_FileExists(const char *FileName)
{
	bool Result = (HashText(FileName) ^ Salt) % 2 == 0;
	Call("FileExists(%s)=%d", Quote(FileName).c_str(), Result ? 1 : 0);
	return Result;
}
static bool F_RemoveFile(const char *FileName)
{
	Call("RemoveFile(%s)", Quote(FileName).c_str());
	return true;
}
static void F_InitSysinc() { Call("InitSysinc()"); }
static void F_ExitSysinc() { Call("ExitSysinc()"); }

// The C library of both sides: rand from one sequence per side (the same),
// the image's formatting functions as ours.
static uint32 RandState[2];
uint32 CommRandState(int S, uint32 Seed)
{
	RandState[S] = Seed;
	return Seed;
}
uint32 CommRandOf(int S) { return RandState[S]; }
static int __cdecl F_Rand()
{
	RandState[Side] = RandState[Side] * 214013 + 2531011;
	return (RandState[Side] >> 16) & 0x7fff;
}
static int __cdecl F_ImageSprintf(char *Buffer, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vsprintf(Buffer, Format, Args);
	va_end(Args);
	return Result;
}
static int __cdecl F_ImageSnprintf(char *Buffer, size_t Count, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = _vsnprintf(Buffer, Count, Format, Args);
	va_end(Args);
	return Result;
}
static char *__cdecl F_ImageStrdup(const char *Text)
{
	char *Copy = (char *)malloc(strlen(Text) + 1);
	strcpy(Copy, Text);
	return Copy;
}
static void WINAPI F_Sleep(DWORD Milliseconds)
{
	Call("Sleep(%u)", Milliseconds);
}

#define F(Name, Address) {#Name, Address, (void *)&::Name, (void *)F_##Name}
#define FM(Name, Address, Member, Fake) {Name, Address, MemberAddress(Member), (void *)Fake}
#define FO(Name, Address, Ours, Fake) {Name, Address, (void *)Ours, (void *)Fake}

// Since issue #105 Communication.cpp's members reach Network.cpp's and Status.cpp's session functions through their
// TNetwork and TSession, and since issue #106 the game state through TMapState's five objects, member to member: a free
// function's fake also takes the place of the member of its name, through this shim. The shim is a member function
// itself, which does not use its this: it has the members' calling convention (MSVC's x64 passes a returned
// structure's address after this, a free function's before the arguments; GetFirstObject returns a TObject). The free
// functions (which Control.cpp and the other files call) keep their fakes. A static member is faked as a function.
template <auto Fake> struct TMemberFake;
template <class R, class... A, R (*Fake)(A...)> struct TMemberFake<Fake> {
	R Call(A... Arguments) { return Fake(Arguments...); }
};
#define FMF(Class, Name) {#Class "::" #Name, 0, MemberAddress(&patella::Class::Name), MemberAddress(&TMemberFake<F_##Name>::Call)}

const TFake WorldFakes[] = {
	// Network.cpp
	F(SetConnectionTimeout, 0x4ca290),
	F(SetServerError, 0x4ca730),
	F(SetSymmetricKey, 0x4ce210),
	F(Disconnect, 0x4ce300),
	F(ConnectToLoginServer, 0x4cf050),
	F(ConnectToGameServer, 0x4cf390),
	F(SendCommand, 0x4cbe00),
	F(GetNextCommand, 0x4cc550),
	F(SetNetworkWindow, 0x4ca2b0),
	F(GetErrorMessage, 0x4cd180),
	F(InitNetwork, 0x4ce930),
	F(ExitNetwork, 0x4cec70),
	F(CheckConnectionTimeout, 0x4cf850),
	FMF(TNetwork, SetConnectionTimeout),
	FMF(TNetwork, SetServerError),
	FMF(TNetwork, SetSymmetricKey),
	FMF(TNetwork, Disconnect),
	FMF(TNetwork, ConnectToLoginServer),
	FMF(TNetwork, ConnectToGameServer),
	FMF(TNetwork, SendCommand),
	FMF(TNetwork, GetNextCommand),
	FMF(TNetwork, SetNetworkWindow),
	FMF(TNetwork, GetErrorMessage),
	FMF(TNetwork, InitNetwork),
	FMF(TNetwork, ExitNetwork),
	FMF(TNetwork, CheckConnectionTimeout),
	// Map.cpp
	F(SetMapPosition, 0x4b08a0),
	F(GetMapPosition, 0x4b08c0),
	F(GetMapLevel, 0x4b08f0),
	F(RelativeToAbsolute, 0x4b0910),
	F(AbsoluteToRelative, 0x4b0960),
	F(SetAmbientLight, 0x4b09e0),
	F(GetFirstObject, 0x4b6140),
	F(GetNextObject, 0x4b6510),
	F(GetMapObject, 0x4b6580),
	F(AddObject, 0x4b88f0),
	F(AppendObject, 0x4b8ec0),
	FO("DeleteObject", 0x4b9420, (bool (*)(int, int, int, int))&DeleteObject, F_DeleteObject),
	F(ChangeObject, 0x4b9940),
	F(AddGraphicalEffect, 0x4bab40),
	F(AddTextualEffect, 0x4bad00),
	F(AddMissileEffect, 0x4bb0c0),
	F(ClearField, 0x4bc720),
	F(ClearMap, 0x4bccb0),
	F(ShiftMap, 0x4bd560),
	F(ShiftMapLevel, 0x4bdb70),
	F(RequestFieldRefresh, 0x4bdd70),
	F(FieldRefreshRequested, 0x4be750),
	F(UpdateAutomap, 0x4beb90),
	F(UpdateAmbientLight, 0x4b0a70),
	F(ExitMap, 0x4b5760),
	F(ProcessEffects, 0x4bba70),
	F(InitMap, 0x4bf620),
	// Creatures.cpp
	FM("TCreature::SetSkull", 0x44b820, &TCreature::SetSkull, F_SetSkull),
	FM("TCreature::SetMark", 0x44bf20, &TCreature::SetMark, F_SetMark),
	FM("TCreature::SetPosition", 0x452320, &TCreature::SetPosition, F_CreatureSetPosition),
	FM("TCreature::StartMovement", 0x44bc70, &TCreature::StartMovement, F_StartMovement),
	F(GetNumberOfPlayerSteps, 0x44b870),
	F(GetPlayerPosition, 0x44b880),
	F(ClearPlayerSteps, 0x44b8c0),
	F(HideAllCreatures, 0x44b8e0),
	F(SetBeat, 0x44b900),
	F(IsPlayer, 0x44b910),
	F(SetPlayerID, 0x44b920),
	F(SetPlayerHealth, 0x44b930),
	F(SetPlayerMagicLevel, 0x44b950),
	F(SetPlayerMana, 0x44b970),
	F(SetPlayerSoul, 0x44b990),
	F(SetPlayerCapacity, 0x44b9a0),
	F(SetPlayerState, 0x44b9b0),
	F(GetPlayerID, 0x44b9c0),
	F(SetAttackID, 0x44bb00),
	F(SetFollowID, 0x44bb20),
	F(GetAttackID, 0x44bb50),
	F(GetFollowID, 0x44bb60),
	F(GetPlayerStep, 0x44c3c0),
	F(AddCreature, 0x44c530),
	F(GetCreature, 0x44c6e0),
	F(NotifySkull, 0x44ca60),
	F(SetPlayerExperience, 0x44cc00),
	F(SetPlayerSkill, 0x44cf10),
	F(AddBuddy, 0x44eab0),
	F(SetBuddyState, 0x44f000),
	F(StopPlayer, 0x4516e0),
	F(CancelPlayerWalk, 0x4518b0),
	F(CheckMonsterHint, 0x451db0),
	F(UpdateSkulls, 0x44c860),
	F(ResetCreatures, 0x4509e0),
	F(MoveCreatures, 0x451ab0),
	F(InitCreatures, 0x452190),
	// Help.cpp
	F(CheckEventHelpPosition, 0x4a3690),
	F(SetHelpPosition, 0x4a2a00),
	F(SetHelpSize, 0x4a2a50),
	F(SetHelpHandle, 0x4a2a70),
	F(GetHelpHandle, 0x4a2ac0),
	F(GetHelpHandleChanged, 0x4a2ad0),
	F(InitHelp, 0x4a2ae0),
	F(AddEventHelp, 0x4a32e0),
	F(GetCurrentHelpText, 0x4a4440),
	// Inventory.cpp
	F(ClearOffers, 0x4a4520),
	F(SetBodyObject, 0x4a4550),
	F(DeleteBodyObject, 0x4a46d0),
	F(OpenContainer, 0x4a4b00),
	F(CloseContainer, 0x4a4f70),
	F(IsContainerOpen, 0x4a5220),
	F(AppendContainerObject, 0x4a5380),
	F(ChangeContainerObject, 0x4a5630),
	F(DeleteContainerObject, 0x4a58e0),
	F(SetOwnOffer, 0x4a6eb0),
	F(SetCounterOffer, 0x4a7230),
	F(AddOwnOfferObject, 0x4a7460),
	F(AddCounterOfferObject, 0x4a75e0),
	F(InsertContainerObject, 0x4a7f00),
	F(InitInventory, 0x4a7d20),
	F(ResetInventory, 0x4a7d70),
	// Objects.cpp
	FO("GetObjectFlag", 0x4d1c40, (bool (*)(int, int))&GetObjectFlag, F_GetObjectFlag),
	F(GetObjectWaypoints, 0x4d1f30),
	F(InitObjects, 0x4d77f0),
	F(ExitObjects, 0x4d79b0),
	F(ReloadObjects, 0x4d7b40),
	// TMapState's members of the faked functions (issue #106): TCommunication calls them.
	FMF(TMap, SetMapPosition),
	FMF(TMap, GetMapPosition),
	FMF(TMap, GetMapLevel),
	FMF(TMap, RelativeToAbsolute),
	FMF(TMap, AbsoluteToRelative),
	FMF(TMap, SetAmbientLight),
	FMF(TMap, GetFirstObject),
	FMF(TMap, GetNextObject),
	FMF(TMap, GetMapObject),
	FMF(TMap, AddObject),
	FMF(TMap, AppendObject),
	FMF(TMap, ChangeObject),
	FMF(TMap, AddGraphicalEffect),
	FMF(TMap, AddTextualEffect),
	FMF(TMap, AddMissileEffect),
	FMF(TMap, ClearField),
	FMF(TMap, ClearMap),
	FMF(TMap, ShiftMap),
	FMF(TMap, ShiftMapLevel),
	FMF(TMap, RequestFieldRefresh),
	FMF(TMap, FieldRefreshRequested),
	FMF(TMap, UpdateAutomap),
	FMF(TMap, UpdateAmbientLight),
	FMF(TMap, ExitMap),
	FMF(TMap, ProcessEffects),
	FMF(TMap, InitMap),
	FMF(TPlayerState, GetNumberOfPlayerSteps),
	FMF(TPlayerState, GetPlayerPosition),
	FMF(TPlayerState, ClearPlayerSteps),
	FMF(TCreatures, HideAllCreatures),
	FMF(TCreatures, SetBeat),
	{"TCreatures::IsPlayer", 0, (void *)&patella::TCreatures::IsPlayer, (void *)F_IsPlayer},
	FMF(TPlayerState, SetPlayerID),
	FMF(TPlayerState, SetPlayerHealth),
	FMF(TPlayerState, SetPlayerMagicLevel),
	FMF(TPlayerState, SetPlayerMana),
	FMF(TPlayerState, SetPlayerSoul),
	FMF(TPlayerState, SetPlayerCapacity),
	FMF(TPlayerState, SetPlayerState),
	FMF(TPlayerState, GetPlayerID),
	FMF(TCreatures, SetAttackID),
	FMF(TCreatures, SetFollowID),
	FMF(TCreatures, GetAttackID),
	FMF(TCreatures, GetFollowID),
	FMF(TPlayerState, GetPlayerStep),
	FMF(TCreatures, AddCreature),
	FMF(TCreatures, GetCreature),
	FMF(TCreatures, NotifySkull), // a member since issue #118 (static before: it calls TCreaturesGUI)
	FMF(TPlayerState, SetPlayerExperience),
	FMF(TPlayerState, SetPlayerSkill),
	FMF(TCreatures, AddBuddy),
	FMF(TCreatures, SetBuddyState),
	FMF(TPlayerState, StopPlayer),
	FMF(TPlayerState, CancelPlayerWalk),
	FMF(TCreatures, CheckMonsterHint),
	FMF(TCreatures, UpdateSkulls),
	FMF(TCreatures, ResetCreatures),
	FMF(TCreatures, MoveCreatures),
	FMF(TCreatures, InitCreatures),
	FMF(TInventory, ClearOffers),
	FMF(TInventory, SetBodyObject),
	FMF(TInventory, DeleteBodyObject),
	FMF(TInventory, OpenContainer),
	FMF(TInventory, CloseContainer),
	FMF(TInventory, IsContainerOpen),
	FMF(TInventory, AppendContainerObject),
	FMF(TInventory, ChangeContainerObject),
	FMF(TInventory, DeleteContainerObject),
	FMF(TInventory, SetOwnOffer),
	FMF(TInventory, SetCounterOffer),
	FMF(TInventory, AddOwnOfferObject),
	FMF(TInventory, AddCounterOfferObject),
	FMF(TInventory, InsertContainerObject),
	FMF(TInventory, InitInventory),
	FMF(TInventory, ResetInventory),
	FMF(TObjectTypes, GetObjectWaypoints),
	FMF(TObjectTypes, InitObjects),
	FMF(TObjectTypes, ExitObjects),
	FMF(TObjectTypes, ReloadObjects),
	FMF(TMap, DeleteObject),
	{"TObjectTypes::GetObjectFlag", 0, MemberAddress(static_cast<bool (patella::TObjectTypes::*)(int, int) const>(&patella::TObjectTypes::GetObjectFlag)), MemberAddress(&TMemberFake<F_GetObjectFlag>::Call)},
	// Preferences.cpp
	F(GetAutoChaseOff, 0x4db650),
	F(SetChaseMode, 0x4db670),
	F(GetAttackMode, 0x4db690),
	F(GetChaseMode, 0x4db6a0),
	F(GetSecureMode, 0x4db6b0),
	F(SetMotD, 0x4dcac0),
	F(SetEngine, 0x4db390),
	F(GetViewMode, 0x4db3a0),
	F(GetFullscreenMode, 0x4db3b0),
	F(GetWindowsMousepointer, 0x4db3c0),
	F(GetMotD, 0x4db560),
	F(GetClassicControl, 0x4db640),
	F(InitPreferences, 0x4ddc00),
	F(SavePreferences, 0x4de890),
	F(ExitPreferences, 0x4df520),
	// Script.cpp
	// The original's only (ours are gone with the patcher, Patella-Modern issue #75): the patch packets and the
	// patcher's events are not compared any more, the fakes stay in case the image's code is reached.
	{"ApplyPatch", 0x4e3100, nullptr, (void *)F_ApplyPatch},
	{"ApplyFullPatch", 0x4e3500, nullptr, (void *)F_ApplyFullPatch},
	{"InstallPatch", 0x4e3610, nullptr, (void *)F_InstallPatch},
	// Status.cpp
	F(ClearRights, 0x4e6de0),
	F(SetActionState, 0x4e6e20),
	F(SetConnectionState, 0x4e71e0),
	F(SetProgress, 0x4e7200),
	F(SetAccountCharacters, 0x4e7250),
	F(SetPremiumDays, 0x4e7290),
	F(GetGamemaster, 0x4e72d0),
	F(ClearMessages, 0x4e76e0),
	F(SetRight, 0x4e8580),
	F(AddMessage, 0x4e9020),
	F(AddMessageFirst, 0x4e91d0),
	F(SetCharacter, 0x4e96d0),
	F(GetActionNumber, 0x4e6e00),
	F(IncrementActionNumber, 0x4e6e10),
	F(GetActionState, 0x4e6e30),
	F(SetMousePosition, 0x4e6e40),
	F(GetMouseX, 0x4e6e60),
	F(GetMouseY, 0x4e6e70),
	F(SetMouseCursor, 0x4e6e80),
	F(GetMouseCursor, 0x4e6e90),
	F(SetPendingAction, 0x4e6ec0),
	F(GetPendingActionState, 0x4e6ee0),
	F(GetPendingActionNumber, 0x4e6ef0),
	F(GetWindowWidth, 0x4e6f20),
	F(GetWindowHeight, 0x4e6f30),
	F(SetDragStart, 0x4e6f40),
	F(CheckDragStart, 0x4e6fe0),
	F(GetConnectionState, 0x4e71f0),
	F(GetAccountNumber, 0x4e7220),
	F(GetAccountPassword, 0x4e7240),
	F(GetAccountCharacters, 0x4e72a0),
	F(GetActiveCharacter, 0x4e72c0),
	F(SetMouseOverText, 0x4e72e0),
	F(SetMouseOverTextPosition, 0x4e72f0),
	F(GetMouseOverText, 0x4e7310),
	F(GetMouseOverTextPosition, 0x4e7320),
	F(UpdateBlinkState, 0x4e7340),
	F(PeekMessageType, 0x4e75d0),
	F(FetchMessage, 0x4e75f0),
	// The error report list is the original's only (ours is gone, Patella-Modern issue #74): its code that reads it
	// (events 0x2a and 0x2b) is not reached any more, the fakes stay in case it is.
	FO("ClearErrorReports", 0x4e7bb0, nullptr, F_ClearErrorReports),
	FO("GetNumberOfErrorReports", 0x4e7e10, nullptr, F_GetNumberOfErrorReports),
	FO("GetErrorReportTitle", 0x4e7e40, nullptr, F_GetErrorReportTitle),
	FO("GetErrorReportDate", 0x4e7e70, nullptr, F_GetErrorReportDate),
	FO("GetErrorReportText", 0x4e7ea0, nullptr, F_GetErrorReportText),
	FO("GetErrorReportComment", 0x4e7ed0, nullptr, F_GetErrorReportComment),
	F(ExitStatus, 0x4e7f00),
	F(GetCharacterName, 0x4e9db0),
	FMF(TSession, ClearRights),
	FMF(TSession, SetConnectionState),
	FMF(TSession, SetProgress),
	FMF(TSession, SetAccountCharacters),
	FMF(TSession, SetPremiumDays),
	FMF(TSession, GetGamemaster),
	FMF(TSession, SetRight),
	FMF(TSession, SetCharacter),
	FMF(TSession, GetConnectionState),
	FMF(TSession, GetAccountNumber),
	FMF(TSession, GetAccountPassword),
	FMF(TSession, GetAccountCharacters),
	FMF(TSession, GetActiveCharacter),
	FMF(TSession, GetCharacterName),
	F(InitStatus, 0x4ea860),
	// Talk.cpp
	F(SetTalkChanged, 0x4fd4c0),
	F(SetTalkThrottle, 0x4fd540),
	F(SetConsoleChanged, 0x4fd590),
	F(SetStatusMessage, 0x4ff500),
	F(IsTalkIgnored, 0x500060),
	FO("GetTextBuffer", 0x5025c0, (TTalkBuffer * (*)(int, const char *, int)) & GetTextBuffer, F_GetTextBuffer),
	F(AddTalkbox, 0x5052e0),
	F(AddMessageTalkbox, 0x505ac0),
	F(AddTextBufferLine, 0x505f10),
	F(AddConsoleText, 0x5061f0),
	F(CreateTextBuffer, 0x507bc0),
	F(UpdateTalkboxes, 0x4ff130),
	F(UpdateStatusMessage, 0x4ff7d0),
	F(AddTemporaryIgnore, 0x500c60),
	F(RemoveTemporaryIgnore, 0x500e40),
	F(ResetTalk, 0x504d70),
	F(InitTalk, 0x504f30),
	F(ShowPermanentIgnoreWarning, 0x5076e0),
	F(ExitTalk, 0x507a00),
	// graphics.cpp
	F(ExitGraphics, 0x48b4a0),
	F(DrawPicturePart, 0x490c60),
	F(GetTextHeight, 0x491880),
	F(ResetVideoCache, 0x4922c0),
	F(ReloadSprites, 0x492490),
	F(PrintText, 0x493af0),
	F(PrintFormattedText, 0x493f60),
	F(GetTextWidth, 0x494570),
	F(ReloadPictures, 0x494900),
	F(InitGraphics, 0x494af0),
	// GUI.cpp and the four other empty Exit functions folded into 0x496160
	F(InitHotkeys, 0x496150),
	FO("ExitHotkeys", 0x496160, &ExitHotkeys, F_EmptyExit),
	FO("ExitCommunication", 0, &ExitCommunication, F_EmptyExit),
	FO("ExitHelp", 0, &ExitHelp, F_EmptyExit),
	FO("ExitCreatures", 0, &ExitCreatures, F_EmptyExit),
	FO("ExitInventory", 0, &ExitInventory, F_EmptyExit),
	F(InitSkinProperties, 0x498160),
	F(CheckHotkey, 0x4a1480),
	F(UpdateTimers, 0x4a27c0),
	// MainWindow.cpp
	F(ExitMainWindow, 0x4ac090),
	FM("TMainWindow::OpenContainer", 0x4ac9e0, &TMainWindow::OpenContainer, F_MainOpenContainer),
	FM("TMainWindow::CloseContainer", 0x4acdb0, &TMainWindow::CloseContainer, F_MainCloseContainer),
	FM("TMainWindow::UpdateTrade", 0x4acf60, &TMainWindow::UpdateTrade, F_MainUpdateTrade),
	FM("TMainWindow::SetConsoleActive", 0x4ad650, &TMainWindow::SetConsoleActive, F_MainSetConsoleActive),
	FM("TMainWindow::ChangeMode", 0x4af5e0, &TMainWindow::ChangeMode, F_MainChangeMode),
	FM("TMainWindow::OpenTrade", 0x4afec0, &TMainWindow::OpenTrade, F_MainOpenTrade),
	FM("TMainWindow::CloseTrade", 0x4b0230, &TMainWindow::CloseTrade, F_MainCloseTrade),
	// ConsoleWindow.cpp
	FM("TConsoleWindow::AddChannel", 0x423060, &TConsoleWindow::AddChannel, F_AddChannel),
	FM("TConsoleWindow::OpenChannel", 0x425070, &TConsoleWindow::OpenChannel, F_OpenChannel),
	FM("TConsoleWindow::ReportRuleViolation", 0x425330, &TConsoleWindow::ReportRuleViolation, F_ReportRuleViolation),
	// ContextMenus.cpp
	FO("THelpBubble::THelpBubble", 0x441d60, OurHelpBubbleConstructor, F_NewHelpBubble),
	FM("THelpBubble::Place", 0x442110, &THelpBubble::Place, F_HelpBubblePlace),
	FO("TObjectContextMenu", 0x443130, OurObjectMenuConstructor, F_NewObjectMenu),
	FO("TCreatureContextMenu", 0x443960, OurCreatureMenuConstructor, F_NewCreatureMenu),
	FO("TBuddyContextMenu", 0x443e20, OurBuddyMenuConstructor, F_NewBuddyMenu),
	FO("TTextContextMenu", 0x444160, OurTextMenuConstructor, F_NewTextMenu),
	FO("TRequestContextMenu", 0x444720, OurRequestMenuConstructor, F_NewRequestMenu),
	FO("TChannelContextMenu", 0x444a70, OurChannelMenuConstructor, F_NewChannelMenu),
	FO("TEditContextMenu", 0x444eb0, OurEditMenuConstructor, F_NewEditMenu),
	FO("TMarkContextMenu", 0x4451e0, OurMarkMenuConstructor, F_NewMarkMenu),
	FO("TSkillContextMenu", 0x4454f0, OurSkillMenuConstructor, F_NewSkillMenu),
	// Dialogs.cpp
	F(CreateMessageDialog, 0x47f8a0),
	F(CreateFormattedMessageDialog, 0x47faf0),
	F(CreateConnectingDialog, 0x47fd30),
	F(CreateWaitDialog, 0x47ff60),
	F(CreateReconnectDialog, 0x4801c0),
	F(CreateQuestionDialog, 0x4803f0),
	F(CreateLogoutDialog, 0x480640),
	F(CreateInfoDialog, 0x4808b0),
	F(CreateMoveObjectsDialog, 0x480cd0),
	F(CreateLoginDialog, 0x480f40),
	F(CreateCharacterListDialog, 0x481160),
	F(CreateTextDialog, 0x481370),
	F(CreateEditListDialog, 0x4817c0),
	F(CreateOutfitDialog, 0x4819f0),
	F(CreateBugReportDialog, 0x481bf0),
	F(CreateNewChannelDialog, 0x481e00),
	F(CreateInviteDialog, 0x482010),
	F(CreateExcludeDialog, 0x482210),
	F(CreateIgnoreListDialog, 0x482410),
	F(CreateReportDialog, 0x482640),
	F(CreateAddBuddyDialog, 0x482870),
	F(CreateEditBuddyDialog, 0x482a70),
	{"CreateUpdateDialog", 0x482c80, nullptr, (void *)F_CreateUpdateDialog}, // the original's only (issue #75)
	F(CreateHelpDialog, 0x482ea0),
	F(CreateHintsDialog, 0x4830c0),
	F(CreateEditMarkDialog, 0x4832e0),
	F(CreateOptionsDialog, 0x483500),
	F(CreateGeneralOptionsDialog, 0x483730),
	F(CreateGraphicsOptionsDialog, 0x483950),
	F(CreateAdvancedGraphicsDialog, 0x483b80),
	F(CreateConsoleOptionsDialog, 0x483dc0),
	F(CreateNetworkOptionsDialog, 0x483fe0),
	F(CreateHotkeyDialog, 0x484200),
	FO("CreateErrorReportDialog", 0x484400, nullptr, F_CreateErrorReportDialog), // the original's only (#74)
	F(CreateRuleViolationDialog, 0x485290),
	// Skins.cpp, Surface.cpp, sysinc.cpp
	F(InitSkins, 0x4e3cb0),
	F(ExitSkins, 0x4e6da0),
	F(GetSurface, 0x4eaa90),
	F(SurfaceExitInstance, 0x4eab60),
	F(GetMilliseconds, 0x4fcb90),
	F(FileExists, 0x4fcba0),
	F(RemoveFile, 0x4fcbc0),
	F(InitSysinc, 0x4fcd00),
	F(ExitSysinc, 0x4fcd30),
	// The C library
	FO("rand", 0x511f33, ORIGINAL_OR(&rand, nullptr), F_Rand), // the runner: its import slot (BeginWorld)
	FO("sprintf", 0x511d6b, NULL, F_ImageSprintf),
	FO("_snprintf", 0x51233d, NULL, F_ImageSnprintf),
	FO("_strdup", 0x51491c, NULL, F_ImageStrdup),
};
const int WorldFakeCount = sizeof(WorldFakes) / sizeof(WorldFakes[0]);

// Our functions must have room for the jump: functions start 16-byte aligned
// (/Gy), so a short one is followed by padding. One that starts on a 16-byte
// boundary has the 16 bytes up to the next function's start: room enough. The
// scan for a ret is for the others: it reads bytes, not instructions, and took
// the operand of `mov eax,[0x2066c378]` for a ret once the data moved (issue #57).
static void CheckRoom(const TFake &Fake, void *Code)
{
	uint8 *p = (uint8 *)Code;
	if (((uintptr_t)Code & 15) == 0)
		return;
	for (int i = 0; i < 5; i++) {
		if (p[i] == 0xc3 || p[i] == 0xc2) {
			for (int j = i + (p[i] == 0xc3 ? 1 : 3); j < 5; j++) {
				if (p[j] != 0xcc && p[j] != 0x90) {
					printf("FAIL communicationtest: our %s is too short for a jump\n", Fake.Name);
					Failures++;
					return;
				}
			}
			return;
		}
	}
}

void InstallFakes(const TFake *Fakes, int Count)
{
	for (int i = 0; i < Count; i++) {
		const TFake &Fake = Fakes[i];
		// The runner is linked without incremental-link thunks: the address is the code.
		if (Fake.Ours != nullptr) {
			CheckRoom(Fake, Fake.Ours);
			golden::RedirectOwn(Fake.Ours, Fake.Fake);
		}
	}
}

// The original's MainWindow and ModalWindow: the test still sets them as the oracle does.
static TCtlMainWindow *ImageMainWindow;
static TWindow *ImageModalWindow;
TCtlMainWindow *SideMainWindow(int S) { return S == IMAGE ? ImageMainWindow : (TCtlMainWindow *)patella::bridge::GUI().MainWindow.get(); }
// TGUI's MainWindow is an owner since issue #108: the test's window goes in without the old one being freed (the
// test frees its windows itself, FreeWorld), as the raw assignment before.
void SetSideMainWindow(int S, TCtlMainWindow *Window)
{
	if (S == IMAGE) {
		ImageMainWindow = Window;
		return;
	}
	(void)patella::bridge::GUI().MainWindow.release();
	patella::bridge::GUI().MainWindow.reset((TMainWindow *)(TWindow *)Window);
}
TWindow *&SideModalWindow(int S) { return S == IMAGE ? ImageModalWindow : patella::bridge::GUI().ModalWindow; }

// An import slot of the runner bound to a fake (golden_patch.h; EndWorld unbinds it).
static void BindOwnImport(const char *Name, void *Fake)
{
	void **Slot = golden::FindImport(Name);
	if (Slot == nullptr) {
		printf("FAIL communication: the runner does not import %s\n", Name);
		Failures++;
		return;
	}
	golden::SetImport(Slot, Fake);
}

void BeginWorld()
{
	// communicationtest.exe runs the test with nothing of ours redirected: the earlier tests' redirections of
	// our code and import slots are undone.
	golden::RestoreAllOwn();
	golden::RestoreAllImports();
	static bool Constructed = false;
	if (!Constructed) {
		Constructed = true;
		// The skins and SkinProperties Redraw reads (cursor hotspots, the
		// tooltip): the real initialisers, before the fakes replace them.
		InitSkins();
		InitSkinProperties();
		OurReceiveBuffer = GoldenNetworkReceiveBuffer();
		OurSendBuffer = GoldenNetworkSendBuffer();
		GoldenNetworkResetBuffers();
		if (OurSendBuffer->Size != 0x800 || OurReceiveBuffer->Size != 0) {
			printf("FAIL communication: Network.cpp's buffers are not in their start state\n");
			Failures++;
		}
	}
	InstallFakes(WorldFakes, WorldFakeCount);
	BindOwnImport("Sleep", (void *)F_Sleep);
	BindOwnImport("rand", (void *)F_Rand);
	// The same signatures and the same empty object as the original's.
	patella::bridge::ObjectTypes().DatSignature = 0x4495fa2b;
	SpriteSignature = 0x449a1c41;
	PictureSignature = 0x43fc1d0c;
	patella::bridge::ObjectTypes().NoObject = TObject();
}

void EndWorld()
{
	golden::RestoreAllOwn();
	golden::RestoreAllImports();
}

// --- Comparison ------------------------------------------------------------------------

__declspec(noinline) void FillStack()
{
	[[maybe_unused]] volatile uint32 Area[8192];
	for (int i = 0; i < 8192; i++)
		Area[i] = 0x5a5a5a5a;
}

static void PrintLogDifference(const char *What)
{
	const std::string &A = Log[OURS], &B = Log[IMAGE];
	size_t i = 0;
	while (i < A.size() && i < B.size() && A[i] == B[i])
		i++;
	size_t Start = i > 300 ? A.rfind(';', i - 300) : 0;
	if (Start == std::string::npos)
		Start = 0;
	if (Failures <= FailuresPrinted) {
		printf("  %s: the logs differ at %u\n    ours:     ...%s\n    original: ...%s\n", What, (unsigned)i,
			A.substr(Start, i - Start + 400).c_str(), B.substr(Start, i - Start + 400).c_str());
	}
}

static std::string DumpCreature(const TCreature &C)
{
	char Buffer[512];
	sprintf(Buffer, "%u %s pos %d,%d,%d dir %d outfit %d %d,%d,%d,%d light %d,%d mark %d health %d speed %d visible %d "
					"skull %d party %d",
		C.CreatureID, Quote(C.Name).c_str(), C.RealX, C.RealY, C.RealZ, C.Direction, C.OutfitType,
		C.Colors[0], C.Colors[1], C.Colors[2], C.Colors[3], C.LightStrength, C.LightColor, C.Mark, C.Health, C.Speed,
		*(const uint8 *)&C.Visible != 0 ? 1 : 0, C.PKFlag, C.PARTYFlag); // its byte: a bool of the test's memory pattern (issue #91)
	return Buffer;
}

static std::string DumpBuffer(int S, TCommBuffer *B)
{
	if (B == NULL)
		return "no buffer";
	int Saved = Side;
	Side = S;
	std::string Text = NameOf(B) + ": first " + NameOf(B->first) + " last " + NameOf(B->last) + " cursor " + NameOf(B->cursor) + " lines";
	for (TTalkLine *L = B->first; L != NULL; L = L->Next)
		Text += " " + NameOf(L);
	Text += B->Selected ? " selected" : "";
	Side = Saved;
	return Text;
}

static std::string DumpWorld(int S)
{
	int Saved = Side;
	Side = S;
	TWorld &W = World[S];
	char Buffer[1024];
	sprintf(Buffer, "player %u attack %u follow %u map %d,%d,%d actionstate %d connection %d actionnumber %d mouse %d,%d "
					"cursor %d pending %d,%d helphandle %d chase %d open %08x overtext %s",
		W.PlayerID, W.AttackID, W.FollowID, W.MapX, W.MapY, W.MapZ, W.ActionState, W.ConnectionState, W.ActionNumber,
		W.MouseX, W.MouseY, W.MouseCursor, W.PendingState, W.PendingNumber, W.HelpHandle, W.ChaseMode, W.OpenContainers,
		Quote(W.MouseOverText).c_str());
	std::string Text = Buffer;
	Text += " events";
	for (const TQueuedEvent &E : W.Events) {
		Text += " ";
		DescribeEvent(Text, E);
	}
	Text += " modal " + NameOf(SideModalWindow(S));
	Text += " main " + NameOf(SideMainWindow(S));
	TBuffer *R = ReceiveBufferOf(S);
	sprintf(Buffer, " receive %d/%d", R->Position, R->Size);
	Text += Buffer;
	Side = Saved;
	return Text;
}

// The original's side of the comparisons by value (golden.h): each text by its changes since the last
// comparison, the creature records by their changed bytes. Empty at the start on both sides.
static std::string LastLog, LastBuffer[WORLD_BUFFERS], LastWorld;
static uint8 LastCreatures[sizeof(TCreature) * WORLD_CREATURES];
static void ResetTextChanges()
{
	LastLog.clear();
	for (std::string &B : LastBuffer)
		B.clear();
	LastWorld.clear();
	memset(LastCreatures, 0, sizeof(LastCreatures));
}
static_assert(sizeof(TCreature) == 0x9c, "TCreature: the same 0x9c bytes on both targets (its records are compared by bytes)");

// --- Control's retired comparisons (commworld.h) ----------------------------------------------------

bool ReachesChangeEngine()
{
	if (ForceReach)
		return true;
	for (const std::string &L : Log) {
		for (size_t p = L.find("Surface.Init("); p != std::string::npos; p = L.find("Surface.Init(", p + 1)) {
			size_t End = L.find(';', p);
			if (End != std::string::npos && End >= 2 && L.compare(End - 2, 2, "=0") == 0)
				return true;
		}
	}
	return false;
}

bool RetiredComparison(bool Reached)
{
	if (!RecordComparisons || !RetireChangeEngine)
		return false;
	uint8 Flag = Reached ? 1 : 0;
	golden::Output("reaches ChangeEngine", Flag);
	return Flag != 0;
}

bool CompareOrRetire(const char *What, const TThrown &O, const TThrown &I, bool Retired)
{
	if (!Retired)
		return CompareRun(What, O, I);
	RetiredPending = true;
	if (!golden::RetiredBegin("ChangeEngine", Checks))
		CompareRun(What, O, I); // the oracle: the part's checks and values
	if (!DeferRetiredEnd)
		EndRetiredComparison();
	LastRetired = true;
	return true;
}

bool SkippingRetired() { return RetiredPending && golden::Mode() == golden::TMode::Replay; }

static void ResetTextChanges();
uint32 CommRandState(int S, uint32 Seed);
uint32 CommRandOf(int S);

void EndRetiredComparison()
{
	if (!RetiredPending)
		return;
	RetiredPending = false;
	golden::RetiredEnd(Checks);
	// What the part changed that later comparisons read: the oracle's values; the runner takes them.
	int Counters[2] = {ThrownCompared, ThrownThrew};
	golden::Output("error round counters", Counters);
	ThrownCompared = Counters[0];
	ThrownThrew = Counters[1];
	uint32 State[2] = {GetRandomSeed(), CommRandOf(IMAGE)};
	golden::Output("random states", State);
	if (golden::Mode() == golden::TMode::Replay) {
		SetRandomSeed(State[0]);
		CommRandState(OURS, State[1]);
	}
	if (AfterRetired != nullptr)
		AfterRetired();
	// The texts recorded by their changes start again from empty on both sides (the runner skipped the part's).
	ResetTextChanges();
}

bool CompareRun(const char *What, const TThrown &O, TThrown I)
{
	int Failures0 = Failures;
	static bool HeapCheck = getenv("CM_HEAPCHK") != NULL;
	if (HeapCheck) {
		int Heap = _heapchk();
		if (Heap != _HEAPOK && Heap != _HEAPEMPTY) {
			printf("communicationtest: the heap is corrupted after %s (_heapchk() %d)\n", What, Heap);
			fflush(stdout);
			abort();
		}
	}
	// The original's side: the oracle's, recorded; the runner's, from the corpus. The creature records go into
	// the original's world, so that they are compared as in the oracle; the dumps of its text buffers and of
	// its world are texts.
	std::string BufferDump[WORLD_BUFFERS], WorldDump;
	if (RecordComparisons) {
		golden::OutputThrown("thrown", &I);
		Log[IMAGE] = golden::OutputTextChanges("log", &LastLog, Log[IMAGE]);
		golden::OutputRuns("creatures", LastCreatures, World[IMAGE].Creatures, sizeof(LastCreatures));
		memcpy(static_cast<void *>(World[IMAGE].Creatures), LastCreatures, sizeof(LastCreatures));
		for (int b = 0; b < WORLD_BUFFERS; b++) {
			static const char *const Records[WORLD_BUFFERS] = {"buffer 0", "buffer 1", "buffer 2", "buffer 3"};
			BufferDump[b] = golden::OutputTextChanges(Records[b], &LastBuffer[b],
				ORIGINAL_OR(DumpBuffer(IMAGE, World[IMAGE].Buffers[b]), std::string()));
		}
		WorldDump = golden::OutputTextChanges("world", &LastWorld, ORIGINAL_OR(DumpWorld(IMAGE), std::string()));
	} else {
	}
	CompareThrown(TestName, What, O, I);
	Checks++;
	if (Log[OURS] != Log[IMAGE]) {
		Failures++;
		if (Failures <= FailuresPrinted)
			printf("FAIL %s: the calls differ\n", What);
		PrintLogDifference(What);
	}
	for (int i = 0; i < WORLD_CREATURES; i++) {
		std::string A = DumpCreature(World[OURS].Creatures[i]), B = DumpCreature(World[IMAGE].Creatures[i]);
		CHECK(A == B && memcmp(&World[OURS].Creatures[i], &World[IMAGE].Creatures[i], sizeof(TCreature)) == 0,
			"%s: creature %d: %s, original %s", What, i, A.c_str(), B.c_str());
	}
	for (int b = 0; b < WORLD_BUFFERS; b++) {
		std::string A = DumpBuffer(OURS, World[OURS].Buffers[b]);
		const std::string &B = BufferDump[b];
		CHECK(A == B, "%s: %s, original %s", What, A.c_str(), B.c_str());
	}
	std::string A = DumpWorld(OURS);
	const std::string &B = WorldDump;
	CHECK(A == B, "%s: world\n    ours:     %s\n    original: %s\n    calls: %s", What, A.c_str(), B.c_str(),
		Log[OURS].substr(0, 3000).c_str());
	if (TraceCalls)
		printf("%s: %s | %s\n", What, O.Describe().c_str(), Log[OURS].substr(0, 300).c_str());
	if (LogFile != NULL)
		fprintf(LogFile, "%s: %s\n%s\n", What, O.Describe().c_str(), Log[OURS].c_str());
	return Failures == Failures0;
}
