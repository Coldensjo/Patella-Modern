// Shared world of communication_tests.cpp and tools/difftest/controltest.cpp (issue #36; moved here from
// tools/difftest with Communication's port to the golden corpus, issue #30).
//
// Communication.cpp and Control.cpp run for real on both sides: the image's
// functions and ours. Every function they call is replaced, on both sides,
// by one fake (commworld.cpp): the image's by a jump at its address, ours by
// a jump at the start of our function (imageeh.cpp's PatchImageJump and
// PatchOwnCode, undone by DisableImageExceptions). A fake logs its call into
// the log of the side that runs (Side) and answers from that side's world,
// which the test sets up the same way for both sides before the calls. The
// comparison is then the two logs, what each side threw, and the dumps of
// both worlds (creature records, text buffers, windows, the receive and send
// buffers, the event queues, the file statics of both files).
//
// Exceptions: the whole test runs with the image's exceptions enabled
// (imageeh.cpp); a fake can make the n-th call of a side throw that side's
// own TMyException (FailAt), so the TEND blocks of every function are reached.
//
// The golden corpus (issue #30; golden.h): the oracle (communicationtest.exe) runs both sides as above and,
// while a ported test records its comparisons (RecordComparisons: Communication; Control is not ported), writes
// the original's side of every comparison by value: what it threw, its log, its creature records (bytes: TCreature
// has the same layout on both targets), its text buffers' and its world's dumps (texts, by their changes since
// the last comparison). The runner (golden_runner, x64) has our side alone: the image's world is built as in the
// oracle but no original code runs on it; Both runs our call, takes the original's side from the corpus and
// compares as the oracle does. Our functions go to the fakes through golden::RedirectOwn (golden_patch.h), our
// rand() and Sleep() through the runner's import slots; the fakes that stand for member functions are
// GOLDEN_THISCALL; the windows the fakes build in place have our x64 layout (each no larger than the class it
// stands for).

#pragma once

#include "golden_test.h"

#include "golden_eh.h"
#if GOLDEN_ORACLE
#include "../../tools/difftest/imageeh.h"
#endif

#include "Creatures.h"
#include "GUI.h"
#include "Objects.h"
#include "Surface.h"
#include "Talk.h"
#include "Utils.h"

#include <cstddef>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

enum { OURS = 0,
	IMAGE = 1 };
extern int Side; // the side whose code runs

// --- The logs ----------------------------------------------------------------

extern std::string Log[2];
void Logf(const char *Format, ...);  // appends to Log[Side]
void Call(const char *Format, ...);  // Tick(), then Logf (every fake that stands for a call)
std::string Quote(const char *Text); // "text" with escapes, or NULL
std::string Hex(const uint8 *Data, int Size);

// Failure injection: the FailAt-th Tick() of a side throws that side's own
// TMyException (SideThrow). 0: never.
extern int FailAt;
extern int Ticks[2];
void Tick();
void SideThrow();
// What the FailAt-th call throws instead of SideThrow (controltest.cpp: other kinds).
extern void (*FaultHandler)();

// Names of the objects of the worlds in the logs (pointers differ between
// the sides): "W3", "Buffer1", "Line2", ...
void NameObject(int Side, const void *Object, const std::string &Name);
std::string NameOf(const void *Object); // of the current side; "?" if unknown, "NULL"

// --- Scripted answers ----------------------------------------------------------

// The answers of the fakes are hashes of their arguments and of the round's
// salt (both sides get the same answer for the same call).
extern uint32 Salt;
uint32 Hash(uint32 a, uint32 b = 0, uint32 c = 0, uint32 d = 0, uint32 e = 0);
uint32 HashText(const char *Text);

// --- The world of one side -----------------------------------------------------

class TCtlWindow;
TCtlWindow *MakeWindow(); // a new window of the current side ("W<n>")

// A text buffer of the world: our TTalkBuffer with the two virtuals
// Communication.cpp calls logged.
class TCommBuffer : public TTalkBuffer {
public:
	TCommBuffer() : TTalkBuffer(0) {}
	bool SetWriteEnabled(bool Value) override;
	void Remove(TTalkLine *line) override;
	void SetSelected(bool Value) override;
};

// A window of the world (dialogs, context menus, the help bubble, ModalWindow):
// every virtual that Control.cpp calls is logged. No larger than the smallest
// window that a faked constructor builds in place (the image's THelpBubble,
// 0x30 bytes, in the oracle; on x64 our context menus and help bubble).
class TCtlWindow : public TWindow {
public:
	TCtlWindow() {}
	~TCtlWindow() override;
	// Deleted windows are freed only when the world is (FreeWorlds): no new
	// window gets the address of one deleted in the same round, so a pointer
	// the code keeps to a deleted window (ModalWindow) is named the same on
	// both sides.
	static void operator delete(void *Memory);
	void SetSize(int _Width, int _Height) override;
	void SetPosition(int _OffsetX, int _OffsetY) override;
	void Draw(int Surface, int x, int y) override;
	bool MouseMove(int x, int y) override;
	bool LeftButtonDown(int x, int y, int Flags) override;
	bool LeftButtonUp(int x, int y, int Flags) override;
	bool LeftButtonDoubleClick(int x, int y, int Flags) override;
	bool RightButtonUp(int x, int y, int Flags) override;
	bool MouseWheel(int x, int y, int Direction) override;
	void MouseHover(int x, int y) override;
	bool HelpMouseMove(int x, int y) override;
	void KeyPress(int Key) override;
	void StopDragging() override;
	TComponent *GetCapture() override;
};

// The main window: TMainWindow's members up to InGame (+0x48 in the oracle; the
// same offsets as our TMainWindow's on every target, commworld.cpp).
class TCtlMainWindow : public TCtlWindow {
public:
	void *MapFrame, *SideBar, *DialogWindow, *SidePanel; // +0x2c
	void *Console;                                       // +0x3c
	void *Splitter, *StartWindow;                        // +0x40
	bool InGame;                                         // +0x48
};

// The surface GetSurface returns.
class TCtlSurface;

const int WORLD_CREATURES = 6;
const int WORLD_BUFFERS = 4;
const int WORLD_LINES = 5;

struct TQueuedEvent {
	int Type;
	INT_PTR Param1, Param2;
};

struct TWorld {
	// Creatures.cpp
	TCreature Creatures[WORLD_CREATURES];
	uint32 PlayerID, AttackID, FollowID;
	int PlayerSteps, Steps[8];
	int PlayerX, PlayerY, PlayerZ;
	// Map.cpp
	int MapX, MapY, MapZ, MapLevel;
	int CylinderIndex, CylinderX, CylinderY, CylinderZ;
	// Inventory.cpp
	uint32 OpenContainers;
	// Talk.cpp
	TCommBuffer *Buffers[WORLD_BUFFERS];
	TTalkLine *Lines[WORLD_BUFFERS][WORLD_LINES];
	// Status.cpp
	int ActionState, ConnectionState, ActionNumber;
	int MouseX, MouseY, MouseCursor;
	int PendingState, PendingNumber;
	int ActiveCharacter, AccountCharacters;
	const char *MouseOverText;
	int MouseOverX, MouseOverY;
	int ErrorReports;
	std::vector<TQueuedEvent> Events;
	// Preferences.cpp
	int ViewMode, Engine;
	bool ClassicControl, WindowsMousepointer, AutoChaseOff;
	int AttackMode, ChaseMode, SecureMode;
	TFullscreenMode Fullscreen;
	// Help.cpp
	int HelpHandle;
	bool HelpChanged;
	// Windows
	TCtlMainWindow *Main;
	void *Console, *CurrentConsole, *TextWindow;
	TCtlSurface *Surface;
	int WindowCount;
	TComponent *Capture; // what the windows' GetCapture returns
	int NextActionState; // set by a window's mouse and key handlers when not -1
	int Milliseconds;
};
extern TWorld World[2];
inline TWorld &Wd() { return World[Side]; }

// Builds both worlds the same way from Random() (decide before the calls).
void ResetWorlds();
void FreeWorlds();
// Compares both worlds; What names the call. Returns false on a difference.
bool CompareWorlds(const char *What);
void DescribeEvent(std::string &Out, const TQueuedEvent &E); // the event's parameters as text
void FreeEvent(const TQueuedEvent &E);                       // deletes the parameters it owns

// --- Patching ------------------------------------------------------------------

struct TFake {
	const char *Name;
	uint32 Image; // the image's function
	void *Ours;   // ours
	void *Fake;
};
void InstallFakes(const TFake *Fakes, int Count);
template <int N> void InstallFakes(const TFake (&Fakes)[N]) { InstallFakes(Fakes, N); }
// The address of a member function (non-virtual, single inheritance).
template <class T> void *MemberAddress(T Member)
{
	union {
		T m;
		void *p;
	} u;
	u.m = Member;
	return u.p;
}

// Every fake of the functions Communication.cpp and Control.cpp call from the
// other files (Network.cpp's reading and building functions stay real: the
// test sets their buffers; SendCommand is faked and logs the packet).
extern const TFake WorldFakes[];
extern const int WorldFakeCount;
// Control.cpp's MainWindow and ModalWindow, per side.
TCtlMainWindow *&SideMainWindow(int Side);
TWindow *&SideModalWindow(int Side);

// The receive buffer of a side (Network.cpp's ReceiveBuffer: the image's
// 0x719d68, ours Network.cpp's static) and the send buffer.
TBuffer *ReceiveBufferOf(int Side);
TBuffer *SendBufferOf(int Side);
void SetReceiveData(int Side, const uint8 *Data, int Size);

// Installs the image's and our world for a test: enables the image's
// exceptions, constructs the image's statics the code needs, installs
// WorldFakes and the CRT fakes (rand, sprintf, _snprintf, _strdup, Sleep).
void BeginWorld();
// The runner: undoes every redirection of our code and every import slot it bound, as the oracle's
// DisableImageExceptions does at the end of the test's error round.
void EndWorld();

// The comparisons of a ported test go into the corpus (golden.h): the original's side by value. Set by
// Communication (issue #30); Control's are not recorded (not ported).
extern bool RecordComparisons;

// Runs Ours and Image (our code, then the image's), with the logs cleared,
// and compares what they threw, the logs and the worlds. What names the call
// in failures. Returns true when everything matched. In the runner Image is
// nullptr (ORIGINAL_SIDE): the original's side comes from the corpus.
template <class FO, class FI> bool Both(const char *What, FO Ours, FI Image);
bool CompareRun(const char *What, const TThrown &O, TThrown I);

// Control's comparisons that reach Control.cpp's ChangeEngine retire with the oracle (issue #30, the user's
// decision of November 4, 2026): ChangeEngine is the x86 engine selection in the oracle and the graphics device
// choice in the x64 client (PATELLA_VULKAN, issue #31). While RetireChangeEngine is set, the oracle records per
// comparison, after both sides ran, whether it reached ChangeEngine (Step 11's call, ForceReach, or a
// "Surface.Init(...)=0" in a log: CheckSurface then calls ChangeEngine(0)); the runner reads that first and, for
// a reached one, neither runs our call nor compares: the comparison's checks are a retired part
// (golden::RetiredBegin, "ChangeEngine"). After the part both sides pass the state the next comparisons read
// through golden::Output (EndRetiredComparison: the counters of the error round, the rand() state, Random()'s
// seed, and the test's own state through AfterRetired). A Step defers the part's end to after its own checks
// on the comparison's results (DeferRetiredEnd); CompareStatics retires its checks after a retired comparison.
extern bool RetireChangeEngine;
extern bool ForceReach;        // the comparison calls ChangeEngine directly (Step 11)
extern bool DeferRetiredEnd;   // a Step ends the retired part itself
extern bool RetiredPending;    // a retired part has begun and not ended yet
extern bool LastRetired;       // the last comparison was retired (CompareStatics retires its checks too)
extern void (*AfterRetired)(); // the test's state after a retired part, through golden::Output
// Whether the comparison is retired: the oracle records Reached (only while RetireChangeEngine is set); the
// runner reads it before it runs our side.
bool RetiredComparison(bool Reached);
bool ReachesChangeEngine(); // the oracle, after both sides ran
// The comparison's checks: CompareRun, or a retired part around it.
bool CompareOrRetire(const char *What, const TThrown &O, const TThrown &I, bool Retired);
void EndRetiredComparison();
// True in the runner while a retired part's checks are skipped (a Step's checks after its comparison).
bool SkippingRetired();
// Fills the stack below the caller with one pattern before each side's call:
// a local the code under test reads before it writes it (a catch block's
// value of a variable not assigned yet) reads the same on both sides.
void FillStack();
extern const char *TestName; // "Communication" or "Control", for the failure reports
// Tibia.dat's object flags and waypoints (loaded by LoadObjectFlags with our
// Objects.cpp before the fakes are installed; in the corpus by value); hashes for other types.
void LoadObjectFlags(const char *ExePath);
bool ObjectFlag(int Type, int Flag);
int ObjectWaypoints(int Type);
int MaxObjectType();
// The server packets ReceivePackets reads (the fake GetNextCommand): each a
// frame, one or more messages; NextFrame[Side] is the next to read.
extern std::vector<std::vector<uint8>> Frames;
extern int NextFrame[2];
// Our functions that the fakes print by name (callbacks): both sides' address.
void NameFunction(const char *Name, uint32 Image, const void *Ours);
extern bool TraceCalls;  // CM_TRACE=1 / CT_TRACE=1
extern bool LiveLog;     // CM_LIVE=<text>: the calls of a comparison whose name contains it, as they happen
extern bool TraceBefore; // CM_TRACE=2 / CT_TRACE=2: each call is named before it runs
extern FILE *LogFile;    // CM_LOGS=<file> / CT_LOGS=<file>: every compared log

// The original's call of a comparison: a lambda in the oracle, nullptr in the runner (no original).
#if GOLDEN_ORACLE
#define ORIGINAL_SIDE(...) [&] { __VA_ARGS__; }
#else
#define ORIGINAL_SIDE(...) nullptr
#endif

template <class FO, class FI> bool Both(const char *What, FO Ours, FI Image)
{
	Log[0].clear();
	Log[1].clear();
	Ticks[0] = Ticks[1] = 0;
	LiveLog = getenv("CM_LIVE") != nullptr && strstr(What, getenv("CM_LIVE")) != nullptr;
	if (TraceBefore) {
		printf("calling %s\n", What);
		fflush(stdout);
	}
	LastRetired = false;
	// The runner knows from the corpus whether the comparison is retired, before it runs our side.
	bool Retired = GOLDEN_ORACLE ? false : RetiredComparison(false);
	Side = OURS;
	TThrown O;
	if (!Retired) {
		FillStack();
		O = OurThrow(Ours);
	}
	TThrown I;
#if GOLDEN_ORACLE
	Side = IMAGE;
	FillStack();
	I = ImageThrow(Image);
	Retired = RetiredComparison(ReachesChangeEngine());
#else
	static_cast<void>(Image);
#endif
	Side = OURS;
	return CompareOrRetire(What, O, I, Retired);
}
