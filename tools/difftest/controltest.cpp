// Differential test of Control.cpp (issue #36), in communicationtest.exe.
//
// Every function of Control.cpp runs on both sides (the image's and ours)
// with every callee faked by commworld.cpp, Communication.cpp's functions
// included (logged, ReceiveOpenContainer and ReceiveCloseContainer set the
// container number): the input handlers in every action state with and
// without a modal window (whose handlers may change the action state), the
// timer, the surface checks and engine changes with a scripted surface,
// Redraw with tooltips at every edge and the frame rate display, the network
// callbacks in every connection state, the dispatcher with every type byte in
// both tables, ProcessEvents with queues of every event type (the parameters
// built as their senders build them), start-up and shutdown. Calls are often
// chained (MouseMove then TimerEvent, KeyPress then Redraw), so the file's
// statics carry from one call to the next; the statics are also compared
// directly: the image's at their addresses, ours found by the values the test
// gives them. The error round makes the n-th fake call of each scenario throw
// (a TMyException, a structured exception or a PROGRAM_ERROR) for every n.
//
// CT_TRACE=1 prints every comparison, CT_LOGS=<file> writes every compared
// log, CT_ROUNDS=<n> the number of rounds (default 3000), CT_SEED=<n>.

#include "../../tests/golden/commworld.h"
#include "../../src/Communication.h"
#include "../../src/Control.h"
#include "../../src/MainWindow.h"
#include "../../src/Surface.h"
#include <functional>

typedef void (__cdecl *TVoid)();
typedef void (__cdecl *TIntFn)(int);
typedef void (__cdecl *TInt2Fn)(int, int);
typedef void (__cdecl *TInt3Fn)(int, int, int);
typedef void (__cdecl *TBoolFn)(bool);
typedef bool (__cdecl *TBoolResult)();

// --- Fakes of Communication.cpp's functions -------------------------------------

#define COMM_FAKE(Name) static void F_##Name() { Call(#Name "()"); }
COMM_FAKE(InitCommunication)
COMM_FAKE(SendFightModes)
COMM_FAKE(SendWalk)
COMM_FAKE(ReceiveLoginError)
COMM_FAKE(ReceiveLoginMessage)
COMM_FAKE(ReceiveMessageOfTheDay)
COMM_FAKE(ReceivePatch)
COMM_FAKE(ReceiveWholeFile)
COMM_FAKE(ReceivePatchBlock)
COMM_FAKE(ReceiveCharacterList)
COMM_FAKE(ReceiveLoginServerOverloaded)
COMM_FAKE(ReceiveGameLogin)
COMM_FAKE(ReceiveGamemasterActions)
COMM_FAKE(ReceiveGameError)
COMM_FAKE(ReceiveGameMessage)
COMM_FAKE(ReceiveLoginWait)
COMM_FAKE(ReceivePing)
COMM_FAKE(ReceiveAddObject)
COMM_FAKE(ReceiveChangeObject)
COMM_FAKE(ReceiveDeleteObject)
COMM_FAKE(ReceiveContainerAddObject)
COMM_FAKE(ReceiveContainerChangeObject)
COMM_FAKE(ReceiveContainerDeleteObject)
COMM_FAKE(ReceiveInventoryObject)
COMM_FAKE(ReceiveDeleteInventoryObject)
COMM_FAKE(ReceiveOwnTrade)
COMM_FAKE(ReceiveCounterTrade)
COMM_FAKE(ReceiveCloseTrade)
COMM_FAKE(ReceiveWorldLight)
COMM_FAKE(ReceiveMagicEffect)
COMM_FAKE(ReceiveAnimatedText)
COMM_FAKE(ReceiveDistanceEffect)
COMM_FAKE(ReceiveCreatureMark)
COMM_FAKE(ReceiveCreatureHealth)
COMM_FAKE(ReceiveCreatureLight)
COMM_FAKE(ReceiveCreatureOutfit)
COMM_FAKE(ReceiveCreatureSpeed)
COMM_FAKE(ReceiveCreatureSkull)
COMM_FAKE(ReceiveCreatureShield)
COMM_FAKE(ReceiveEditText)
COMM_FAKE(ReceiveEditHouseText)
COMM_FAKE(ReceivePlayerStats)
COMM_FAKE(ReceivePlayerSkills)
COMM_FAKE(ReceivePlayerIcons)
COMM_FAKE(ReceiveCancelAttack)
COMM_FAKE(ReceiveTalk)
COMM_FAKE(ReceiveChannelList)
COMM_FAKE(ReceiveOpenChannel)
COMM_FAKE(ReceiveOpenPrivateConversation)
COMM_FAKE(ReceiveRuleViolationChannel)
COMM_FAKE(ReceiveRemoveRuleViolation)
COMM_FAKE(ReceiveCancelRuleViolation)
COMM_FAKE(ReceiveLockRuleViolation)
COMM_FAKE(ReceiveOwnChannel)
COMM_FAKE(ReceiveCloseChannel)
COMM_FAKE(ReceiveTextMessage)
COMM_FAKE(ReceiveOutfit)
COMM_FAKE(ReceiveBuddy)
COMM_FAKE(ReceiveBuddyLogin)
COMM_FAKE(ReceiveBuddyLogout)
COMM_FAKE(ReceiveMap)
COMM_FAKE(ReceiveMapNorth)
COMM_FAKE(ReceiveMapEast)
COMM_FAKE(ReceiveMapSouth)
COMM_FAKE(ReceiveMapWest)
COMM_FAKE(ReceiveField)
COMM_FAKE(ReceiveMoveCreature)
COMM_FAKE(ReceiveCancelWalk)
COMM_FAKE(ReceiveFloorUp)
COMM_FAKE(ReceiveFloorDown)
static void F_ReceiveOpenContainer(int &Container)
{
	Container = (int)(Hash(90, Wd().ActionNumber, Ticks[Side]) % 20);
	Call("ReceiveOpenContainer()=%d", Container);
}
static void F_ReceiveCloseContainer(int &Container)
{
	Container = (int)(Hash(91, Wd().ActionNumber, Ticks[Side]) % 20);
	Call("ReceiveCloseContainer()=%d", Container);
}
static void F_SendLogin(uint32 AccountID, const char *Password)
{
	Call("SendLogin(%u,%s)", AccountID, Quote(Password).c_str());
}
static void F_SendGameLogin(uint32 AccountID, const char *Name, const char *Password)
{
	Call("SendGameLogin(%u,%s,%s)", AccountID, Quote(Name).c_str(), Quote(Password).c_str());
}
static void F_SendDebugReport(const char *Assertion, const char *Date, const char *Description, const char *Comment)
{
	Call("SendDebugReport(%s,%s,%s,%s)", Quote(Assertion).c_str(), Quote(Date).c_str(), Quote(Description).c_str(),
		Quote(Comment).c_str());
}

#define CF(Name, Address) {#Name, Address, (void *)&::Name, (void *)F_##Name}
static const TFake CommunicationFakes[] = {
	CF(InitCommunication, 0x401000), CF(SendFightModes, 0x407eb0), CF(SendDebugReport, 0x40a2f0),
	CF(ReceiveLoginError, 0x40a4c0), CF(ReceiveLoginMessage, 0x40a670), CF(ReceiveMessageOfTheDay, 0x40a810),
	CF(ReceivePatch, 0x40a9c0), CF(ReceiveWholeFile, 0x40ae70), CF(ReceivePatchBlock, 0x40b290),
	CF(ReceiveCharacterList, 0x40b8b0), CF(ReceiveLoginServerOverloaded, 0x40bc80), CF(ReceiveGameLogin, 0x40be30),
	CF(ReceiveGamemasterActions, 0x40c000), CF(ReceiveGameError, 0x40c1b0), CF(ReceiveGameMessage, 0x40c360),
	CF(ReceiveLoginWait, 0x40c500), CF(ReceivePing, 0x40c6a0), CF(ReceiveAddObject, 0x40cd10),
	CF(ReceiveChangeObject, 0x40cfa0), CF(ReceiveDeleteObject, 0x40d440), CF(ReceiveOpenContainer, 0x40d7d0),
	CF(ReceiveCloseContainer, 0x40da70), CF(ReceiveContainerAddObject, 0x40dc20),
	CF(ReceiveContainerChangeObject, 0x40de40), CF(ReceiveContainerDeleteObject, 0x40e060),
	CF(ReceiveInventoryObject, 0x40e230), CF(ReceiveDeleteInventoryObject, 0x40e440), CF(ReceiveOwnTrade, 0x40e5e0),
	CF(ReceiveCounterTrade, 0x40e800), CF(ReceiveCloseTrade, 0x40ea30), CF(ReceiveWorldLight, 0x40ebc0),
	CF(ReceiveMagicEffect, 0x40ed60), CF(ReceiveAnimatedText, 0x40ef50), CF(ReceiveDistanceEffect, 0x40f150),
	CF(ReceiveCreatureMark, 0x40f380), CF(ReceiveCreatureHealth, 0x40f550), CF(ReceiveCreatureLight, 0x40f710),
	CF(ReceiveCreatureOutfit, 0x40f8e0), CF(ReceiveCreatureSpeed, 0x40fb20), CF(ReceiveCreatureSkull, 0x40fce0),
	CF(ReceiveCreatureShield, 0x40fec0), CF(ReceiveEditText, 0x410080), CF(ReceiveEditHouseText, 0x410260),
	CF(ReceivePlayerStats, 0x410410), CF(ReceivePlayerSkills, 0x410660), CF(ReceivePlayerIcons, 0x410810),
	CF(ReceiveCancelAttack, 0x4109b0), CF(ReceiveTalk, 0x410b40), CF(ReceiveChannelList, 0x411030),
	CF(ReceiveOpenChannel, 0x411230), CF(ReceiveOpenPrivateConversation, 0x4113f0),
	CF(ReceiveRuleViolationChannel, 0x4115a0), CF(ReceiveRemoveRuleViolation, 0x411760),
	CF(ReceiveCancelRuleViolation, 0x411940), CF(ReceiveLockRuleViolation, 0x411b20), CF(ReceiveOwnChannel, 0x411ce0),
	CF(ReceiveCloseChannel, 0x411ea0), CF(ReceiveTextMessage, 0x412060), CF(ReceiveOutfit, 0x412350),
	CF(ReceiveBuddy, 0x412540), CF(ReceiveBuddyLogin, 0x412720), CF(ReceiveBuddyLogout, 0x4128c0),
	CF(SendLogin, 0x413780), CF(SendGameLogin, 0x413de0), CF(SendWalk, 0x414700), CF(ReceiveMap, 0x415200),
	CF(ReceiveMapNorth, 0x4153f0), CF(ReceiveMapEast, 0x4155c0), CF(ReceiveMapSouth, 0x415790),
	CF(ReceiveMapWest, 0x415960), CF(ReceiveField, 0x415b30), CF(ReceiveMoveCreature, 0x415d40),
	CF(ReceiveCancelWalk, 0x416c40), CF(ReceiveFloorUp, 0x417020), CF(ReceiveFloorDown, 0x417270),
};

// --- The callbacks of InitWindow ---------------------------------------------------

static void TestExitProgram() { Call("ExitProgram()"); }
static void TestCloseProgram() { Call("CloseProgram()"); }
static void TestSetDisplayMode(int Mode) { Call("SetDisplayMode(%d)", Mode); }
static void TestOpenURL(const char *URL) { Call("OpenURL(%s)", Quote(URL).c_str()); }
static const HWND TestWindow = (HWND)(uintptr_t)0x13572468;

// --- Failure kinds -----------------------------------------------------------------

// What the FailAt-th call throws: 0 the side's TMyException, 1 a structured
// exception (catch (...) of both sides), 2 a PROGRAM_ERROR of the side.
static int FailKind;
static uint32 ThrownError;
static void ControlThrow()
{
	if (FailKind == 1) {
		RaiseException(0xe0436f6d, 0, 0, NULL);
	} else if (FailKind == 2) {
		ThrownError = 7 + Ticks[Side] % 11;
		if (Side == IMAGE)
			At<void (__stdcall *)(void *, void *)>(0x511d2b)(&ThrownError, (void *)(uintptr_t)0x57b3c4);
		throw (PROGRAM_ERROR)ThrownError;
	}
	SideThrow();
}

// --- The statics ---------------------------------------------------------------------

// The image's statics (Control.cpp's .data/.bss) and ours, found by values.
struct TControlStatics {
	int *LastPacketType, *ReconnectTimeout, *LastMouseMove;
	HWND *WindowHandle;
	void **ExitCallback, **CloseCallback, **DisplayModeCallback, **URLCallback;
};
static TControlStatics Statics[2];

// Our writable sections.
static void OurWritable(uint8 **Start, uint8 **End)
{
	uint8 *Base = (uint8 *)GetModuleHandleA(NULL);
	IMAGE_NT_HEADERS32 *NT = (IMAGE_NT_HEADERS32 *)(Base + ((IMAGE_DOS_HEADER *)Base)->e_lfanew);
	IMAGE_SECTION_HEADER *S = IMAGE_FIRST_SECTION(NT);
	*Start = NULL;
	*End = NULL;
	for (int i = 0; i < NT->FileHeader.NumberOfSections; i++) {
		if ((S[i].Characteristics & IMAGE_SCN_MEM_WRITE) == 0)
			continue;
		uint8 *A = Base + S[i].VirtualAddress, *B = A + S[i].Misc.VirtualSize;
		if (*Start == NULL || A < *Start)
			*Start = A;
		if (B > *End)
			*End = B;
	}
}

// The one dword of ours that holds Value after Set(Value) and Value2 after Set(Value2).
static void *FindOurStatic(const char *Name, std::function<void(uint32)> Set, uint32 Value, uint32 Value2)
{
	uint8 *Start, *End;
	OurWritable(&Start, &End);
	Set(Value);
	std::vector<uint32 *> Candidates;
	for (uint32 *p = (uint32 *)Start; p + 1 <= (uint32 *)End; p++)
		if (*p == Value && (p < (uint32 *)&World[0] || p >= (uint32 *)&World[2])) // not the worlds' copies
			Candidates.push_back(p);
	Set(Value2);
	uint32 *Found = NULL;
	int Count = 0;
	for (uint32 *p : Candidates) {
		if (*p == Value2) {
			Found = p;
			Count++;
		}
	}
	if (Count != 1) {
		printf("FAIL controltest: our %s not found (%d candidates)\n", Name, Count);
		Failures++;
		return NULL;
	}
	return Found;
}

static void FindStatics()
{
	TControlStatics &I = Statics[IMAGE];
	I.LastPacketType = (int *)(uintptr_t)0x5c0d8c;
	I.ReconnectTimeout = (int *)(uintptr_t)0x5c4520;
	I.LastMouseMove = (int *)(uintptr_t)0x5c4528;
	I.URLCallback = (void **)(uintptr_t)0x5c452c;
	I.DisplayModeCallback = (void **)(uintptr_t)0x5c4530;
	I.CloseCallback = (void **)(uintptr_t)0x5c4534;
	I.ExitCallback = (void **)(uintptr_t)0x5c4538;
	I.WindowHandle = (HWND *)(uintptr_t)0x5c453c;
	TControlStatics &O = Statics[OURS];
	Side = OURS;
	ResetWorlds();
	auto Init = [](uint32 v) {
		InitWindow((HWND)(uintptr_t)v, (void (*)())(uintptr_t)(v + 1), (void (*)())(uintptr_t)(v + 2),
			(void (*)(int))(uintptr_t)(v + 3), (void (*)(const char *))(uintptr_t)(v + 4));
	};
	O.WindowHandle = (HWND *)FindOurStatic("WindowHandle", [&](uint32 v) { Init(v); }, 0x7a5b3c10, 0x7a5b3d20);
	O.ExitCallback = (void **)FindOurStatic("ExitCallback", [&](uint32 v) { Init(v - 1); }, 0x7a5b3c11, 0x7a5b3d21);
	O.CloseCallback = (void **)FindOurStatic("CloseCallback", [&](uint32 v) { Init(v - 2); }, 0x7a5b3c12, 0x7a5b3d22);
	O.DisplayModeCallback = (void **)FindOurStatic("DisplayModeCallback", [&](uint32 v) { Init(v - 3); }, 0x7a5b3c13,
		0x7a5b3d23);
	O.URLCallback = (void **)FindOurStatic("URLCallback", [&](uint32 v) { Init(v - 4); }, 0x7a5b3c14, 0x7a5b3d24);
	O.LastMouseMove = (int *)FindOurStatic("LastMouseMove", [](uint32 v) {
		World[OURS].ActionState = 0;
		World[OURS].ActionNumber = (int)v;
		MouseMove(1, 1);
	}, 0x6b5c4d30, 0x6b5c4d40);
	O.ReconnectTimeout = (int *)FindOurStatic("ReconnectTimeout", [](uint32 v) {
		World[OURS].ConnectionState = 8;
		World[OURS].Milliseconds = (int)v - 5000;
		World[OURS].Events.clear();
		World[OURS].Events.push_back(TQueuedEvent{0x3f, 0, 0});
		World[OURS].Capture = NULL;
		ModalWindow = NULL;
		ProcessEvents();
	}, 0x5c6d7e30, 0x5c6d7e40);
	// LastPacketType: a type the dispatcher knows, read from a frame.
	O.LastPacketType = (int *)FindOurStatic("LastPacketType", [](uint32 v) {
		World[OURS].ConnectionState = 8;
		Frames.clear();
		Frames.push_back(std::vector<uint8>{(uint8)v});
		NextFrame[OURS] = 0;
		SetReceiveData(OURS, NULL, 0);
		ReceivePackets();
	}, 0xd3, 0xd4);
	Log[OURS].clear();
	FreeWorlds();
}

static std::string DumpStatics(int S)
{
	TControlStatics &T = Statics[S];
	if (T.LastPacketType == NULL || T.ReconnectTimeout == NULL || T.LastMouseMove == NULL || T.WindowHandle == NULL
		|| T.ExitCallback == NULL || T.CloseCallback == NULL || T.DisplayModeCallback == NULL || T.URLCallback == NULL)
		return "statics not found";
	int Saved = Side;
	Side = S;
	char Buffer[512];
	sprintf(Buffer, "LastPacketType %d ReconnectTimeout %d LastMouseMove %d WindowHandle %s callbacks %s %s %s %s",
		*T.LastPacketType, *T.ReconnectTimeout, *T.LastMouseMove, NameOf(*T.WindowHandle).c_str(),
		NameOf(*T.ExitCallback).c_str(), NameOf(*T.CloseCallback).c_str(), NameOf(*T.DisplayModeCallback).c_str(),
		NameOf(*T.URLCallback).c_str());
	Side = Saved;
	return Buffer;
}

// OutputControlErrorInformations into the image's std::stringstream
// (constructor 0x403a10, its ostream at +8, str() 0x412a60) and ours.
typedef void (__fastcall *TStreamCtor)(void *This, void *, int Mode, int MostDerived);
typedef void *(__fastcall *TStreamStr)(void *This, void *, void *Result);
static std::string ImageErrorInformations()
{
	static uint8 Stream[0x200];
	memset(Stream, 0, sizeof(Stream));
	At<TStreamCtor>(0x403a10)(Stream, NULL, 3, 1);
	At<void (__cdecl *)(void *)>(0x445900)(Stream + 8);
	uint8 String[0x1c];
	At<TStreamStr>(0x412a60)(Stream, NULL, String);
	std::string Text = ImageString(String);
	FreeImageString(String);
	return Text;
}

static void CompareStatics(const char *What)
{
	std::string A = DumpStatics(OURS), B = DumpStatics(IMAGE);
	CHECK(A == B, "%s: Control.cpp's statics\n    ours:     %s\n    original: %s", What, A.c_str(), B.c_str());
	std::ostringstream Out;
	OutputControlErrorInformations(Out);
	std::string C = Out.str(), D = ImageErrorInformations();
	for (std::string *T : {&C, &D}) // std::endl: "\n" in both
		while (!T->empty() && (T->back() == '\n' || T->back() == '\r'))
			T->pop_back();
	CHECK(C == D, "%s: OutputControlErrorInformations \"%s\", original \"%s\"", What, C.c_str(), D.c_str());
}

// --- Scenarios -----------------------------------------------------------------------

// A modal window on both sides (or none), and the action state the window's
// handlers leave.
static void SetModal(int Kind)
{
	for (int S = 0; S < 2; S++) {
		int Saved = Side;
		Side = S;
		SideModalWindow(S) = Kind != 0 ? (TWindow *)MakeWindow() : NULL;
		Side = Saved;
	}
}

static int Coordinate()
{
	switch (Random() % 6) {
	case 0: return (int)(Random() % 3) - 1;
	case 1: return (int)Random() - 0x40000000;
	default: return (int)(Random() % 1200) - 100;
	}
}

static void ScriptWorld()
{
	ResetWorlds();
	int Modal = Random() % 3 == 0 ? 0 : 1;
	SetModal(Modal);
	int Next = Random() % 3 == 0 ? (int)(Random() % 12) : -1;
	for (int S = 0; S < 2; S++) {
		World[S].NextActionState = Next;
		curEngine = Global<int>(0x71d0c4) = Random() % 5 == 0 ? 3 : (int)(Random() % 3);
	}
	if (Random() % 4 == 0) {
		int State = Random() % 12;
		World[0].ActionState = World[1].ActionState = State;
	}
}

static void Step(int Round)
{
	char What[96];
	int Kind = Random() % 27;
	int x = Coordinate(), y = Coordinate(), Flags = Random() % 4 == 0 ? 0x200 : (int)(Random() % 16);
	int Key = Random() % 5 == 0 ? 0x907 : (Random() % 3 == 0 ? (int)Random() % 0x1000 : (int)(Random() % 256));
	bool Force = Random() % 2 == 0;
	int Engine = Random() % 6 == 0 ? (int)(Random() % 7) - 2 : (int)(Random() % 3);
	sprintf(What, "Control step %d (round %d)", Kind, Round);
	switch (Kind) {
	case 0:
		Both(What, [&] { MouseMove(x, y); TimerEvent(); }, [&] { At<TInt2Fn>(0x4469b0)(x, y); At<TVoid>(0x4489a0)(); });
		break;
	case 1:
		Both(What, [&] { LeftButtonDown(x, y, Flags); }, [&] { At<TInt3Fn>(0x446d10)(x, y, Flags); });
		break;
	case 2:
		Both(What, [&] { LeftButtonUp(x, y, Flags); }, [&] { At<TInt3Fn>(0x447000)(x, y, Flags); });
		break;
	case 3:
		Both(What, [&] { LeftButtonDoubleClick(x, y, Flags); }, [&] { At<TInt3Fn>(0x447390)(x, y, Flags); });
		break;
	case 4:
		Both(What, [&] { RightButtonDown(x, y, Flags); }, [&] { At<TInt3Fn>(0x4476c0)(x, y, Flags); });
		break;
	case 5:
		Both(What, [&] { RightButtonUp(x, y, Flags); }, [&] { At<TInt3Fn>(0x447880)(x, y, Flags); });
		break;
	case 6:
		Both(What, [&] { MouseWheel(x, y, Flags); }, [&] { At<TInt3Fn>(0x447bd0)(x, y, Flags); });
		break;
	case 7:
		Both(What, [&] { KeyPress(Key); Redraw(Force); }, [&] { At<TIntFn>(0x447e60)(Key); At<TBoolFn>(0x449210)(Force); });
		break;
	case 8: {
		bool Result[2] = {false, false};
		Both(What, [&] { Result[0] = CloseRequest(); }, [&] { Result[1] = At<TBoolResult>(0x4480b0)(); });
		CHECK(Result[0] == Result[1], "%s: CloseRequest %d, original %d", What, Result[0], Result[1]);
		break;
	}
	case 9:
		// In fullscreen mode the original keeps the mode's size in registers and
		// its catch block prints the arguments; ours prints the size it assigned
		// to them (a compiler artifact, notes/decisions.md): a failing call in
		// that mode gets the mode's size as its arguments.
		if (FailAt != 0 && World[OURS].ViewMode == 2) {
			x = World[OURS].Fullscreen.Width;
			y = World[OURS].Fullscreen.Height;
		}
		Both(What, [&] { Resize(x, y); }, [&] { At<TInt2Fn>(0x4459b0)(x, y); });
		break;
	case 10:
		Both(What, [&] { CheckSurface(Force); }, [&] { At<TBoolFn>(0x449010)(Force); });
		break;
	case 11:
		Both(What, [&] { ChangeEngine(Engine); }, [&] { At<TIntFn>(0x445c20)(Engine); });
		CHECK(curEngine == Global<int>(0x71d0c4), "%s: curEngine %d, original %d", What, curEngine, Global<int>(0x71d0c4));
		break;
	case 12:
		Both(What, [&] { Redraw(Force); }, [&] { At<TBoolFn>(0x449210)(Force); });
		break;
	case 13:
		Both(What, [&] { SendErrorReports(); }, [&] { At<TVoid>(0x4463f0)(); });
		break;
	case 14:
		Both(What, [&] { EnterGame(); }, [&] { At<TVoid>(0x4465f0)(); });
		break;
	case 15:
		Both(What, [&] { LeaveGame(); }, [&] { At<TVoid>(0x4467d0)(); });
		break;
	case 16:
		Both(What, [&] { LinkCopyWarning(); }, [&] { At<TVoid>(0x448290)(); });
		break;
	case 17: {
		int State = Random() % 3 == 0 ? (int)(Random() % 9) : (Random() % 2 == 0 ? 2 : 5);
		World[0].ConnectionState = World[1].ConnectionState = State;
		Both(What, [&] { ConnectionEstablished(); }, [&] { At<TVoid>(0x448430)(); });
		break;
	}
	case 18: {
		int State = Random() % 3 == 0 ? (int)(Random() % 9) : (Random() % 2 == 0 ? 8 : (Random() % 2 == 0 ? 6 : 3));
		World[0].ConnectionState = World[1].ConnectionState = State;
		// A pending reconnect first (event 0x3f), sometimes.
		bool Pending = Random() % 2 == 0;
		static const int Edges[] = {4999, 5000, 5001, 6000};
		int Delay = Random() % 2 == 0 ? Edges[Random() % 4] : (int)(Random() % 4000);
		if (Pending) { // ProcessEvents handles the event
			SetModal(0);
			World[0].Capture = World[1].Capture = NULL;
		}
		Both(What, [&] {
			if (Pending) {
				Wd().Events.push_back(TQueuedEvent{0x3f, 0, 0});
				ProcessEvents();
				Wd().Milliseconds += Delay;
			}
			ConnectionClosed();
		}, [&] {
			if (Pending) {
				Wd().Events.push_back(TQueuedEvent{0x3f, 0, 0});
				At<TVoid>(0x4498a0)();
				Wd().Milliseconds += Delay;
			}
			At<TVoid>(0x448720)();
		});
		break;
	}
	case 19:
		Both(What, [&] { TimerEvent(); }, [&] { At<TVoid>(0x4489a0)(); });
		break;
	case 20:
		Both(What, [&] { MouseMove(x, y); }, [&] { At<TInt2Fn>(0x4469b0)(x, y); });
		break;
	case 21:
		Both(What, [&] { HandleEvents(); }, [&] { At<TVoid>(0x44a770)(); });
		break;
	case 22:
		Both(What, [&] {
			InitWindow(TestWindow, TestExitProgram, TestCloseProgram, TestSetDisplayMode, TestOpenURL);
		}, [&] {
			At<void (__cdecl *)(HWND, void (*)(), void (*)(), void (*)(int), void (*)(const char *))>(0x448c50)(
				TestWindow, TestExitProgram, TestCloseProgram, TestSetDisplayMode, TestOpenURL);
		});
		break;
	case 26: {
		// Ten timer ticks after a mouse move: the window under the mouse is told (hover).
		int Ticks = 9 + Random() % 3;
		Both(What, [&] {
			MouseMove(x, y);
			for (int i = 0; i < Ticks; i++) {
				TimerEvent();
			}
		}, [&] {
			At<TInt2Fn>(0x4469b0)(x, y);
			for (int i = 0; i < Ticks; i++) {
				At<TVoid>(0x4489a0)();
			}
		});
		break;
	}
	default: {
		// A tooltip at an edge, the cursor, the frame rate.
		int Width = Random() % 3 == 0 ? (int)(Random() % 4000) : (int)(Random() % 200);
		int Height = -1;
		const char *Text = Random() % 4 == 0 ? NULL : "Tooltip";
		if (Text != NULL && Random() % 2 == 0) {
			// The limits' edges: MaxX and MaxY of -1 to 2 (GetTextWidth and
			// GetTextHeight answer hashes of the text, commworld.cpp).
			int Border = SkinProperties.TooltipBorder * 2;
			Width = (int)(HashText(Text) % 2000) + Border + (int)(Random() % 4);
			Height = (int)(HashText(Text) % 40) + Border + (int)(Random() % 4);
		}
		for (int S = 0; S < 2; S++) {
			World[S].MouseOverText = Text;
			World[S].Main->Width = Width;
			if (Height >= 0)
				World[S].Main->Height = Height;
		}
		Both(What, [&] { Redraw(Force); }, [&] { At<TBoolFn>(0x449210)(Force); });
	}
	}
}

// ProcessEvents with a queue: the parameters as their senders build them.
static const char *const Literals[] = {"Caption", "A text.", "", "Rule Violations"};

static char *HeapText(const char *Text)
{
	char *Copy = new char[strlen(Text) + 1];
	strcpy(Copy, Text);
	return Copy;
}

struct TEventPlan {
	int Type;
	uint32 Seed;
};

static TQueuedEvent BuildEvent(const TEventPlan &Plan)
{
	uint32 Saved = GetRandomSeed();
	SetRandomSeed(Plan.Seed);
	TQueuedEvent E = {Plan.Type, 0, 0};
	char Text[64];
	sprintf(Text, "text %u", Random() % 1000);
	switch (Plan.Type) {
	case 0x01: case 0x02: E.Param1 = (INT_PTR)Literals[Random() % 4]; E.Param2 = (INT_PTR)HeapText(Text); break;
	case 0x05: E.Param1 = (INT_PTR)HeapText(Text); E.Param2 = Random() % 100; break;
	case 0x06: case 0x07: {
		INT_PTR *Data = new INT_PTR[4];
		Data[0] = (INT_PTR)Literals[Random() % 4];
		Data[1] = (INT_PTR)Literals[Random() % 4];
		Data[2] = Random() % 2 == 0 ? 0 : (INT_PTR)Wd().Main;
		Data[3] = Random() % 50;
		E.Param1 = (INT_PTR)Data;
		break;
	}
	case 0x09: case 0x0a: case 0x18: E.Param1 = Random() % 1000; break;
	case 0x0b: case 0x0c: E.Param1 = (INT_PTR)Literals[Random() % 4]; break;
	case 0x0e: {
		uint32 *Info = new uint32[3];
		Info[0] = Random();
		Info[1] = Random() % 100;
		Info[2] = Random() % 100;
		INT_PTR *Texts = new INT_PTR[2];
		Texts[0] = (INT_PTR)HeapText(Text);
		Texts[1] = (INT_PTR)HeapText("writer");
		E.Param1 = (INT_PTR)Info;
		E.Param2 = (INT_PTR)Texts;
		break;
	}
	case 0x0f: {
		uint32 *Info = new uint32[2];
		Info[0] = Random() % 3;
		Info[1] = Random();
		E.Param1 = (INT_PTR)Info;
		E.Param2 = (INT_PTR)HeapText(Text);
		break;
	}
	case 0x10: {
		int *Outfit = new int[7];
		for (int i = 0; i < 7; i++)
			Outfit[i] = Random() % 200;
		E.Param1 = (INT_PTR)Outfit;
		break;
	}
	case 0x12: {
		int Count = Random() % 4;
		int *Channels = new int[Count + 1];
		Channels[0] = Count;
		char *Names = Count != 0 ? new char[Count * 30] : NULL;
		for (int i = 0; i < Count; i++) {
			Channels[i + 1] = Random() % 100;
			sprintf(Names + i * 30, "channel %d", i);
		}
		E.Param1 = (INT_PTR)Channels;
		E.Param2 = (INT_PTR)Names;
		break;
	}
	case 0x19: case 0x1a: case 0x33: E.Param1 = (INT_PTR)HeapText(Text); break;
	case 0x1c: {
		E.Param1 = Random() % 3 == 0 ? 0 : (INT_PTR)HeapText(Text);
		if (Random() % 3 != 0) {
			char *Statement = new char[64];
			memset(Statement, 0, 64);
			NameObject(Side, Statement, "Statement");
			E.Param2 = (INT_PTR)Statement;
		}
		break;
	}
	case 0x1d: case 0x1e: case 0x24: case 0x3a: case 0x3b: case 0x3c:
		E.Param1 = (INT_PTR)Wd().TextWindow;
		E.Param2 = Plan.Type == 0x1e ? (INT_PTR)Wd().CurrentConsole : 0;
		break;
	case 0x29: E.Param1 = (INT_PTR)Literals[Random() % 4]; E.Param2 = Random() % 5000; break;
	case 0x2f: E.Param2 = (INT_PTR)HeapText(Text); break;
	case 0x30: case 0x31: E.Param1 = Random() % 100; E.Param2 = (INT_PTR)HeapText(Text); break;
	case 0x32: E.Param1 = (INT_PTR)Wd().Buffers[Random() % WORLD_BUFFERS]; E.Param2 = Random() % 2 == 0 ? 0 : (INT_PTR)HeapText(Text); break;
	case 0x35: case 0x36: case 0x37: case 0x39: E.Param1 = Random() % 100000; break;
	default:
		if (Plan.Type > 0x48 || Plan.Type < 1) {
			E.Param1 = Random() % 10;
			E.Param2 = Random() % 10;
		}
	}
	SetRandomSeed(Saved);
	return E;
}

static int EventCovered[0x4a];

static void EventRound(int Round, int Fail)
{
	char What[96];
	ScriptWorld();
	if (Random() % 2 == 0)
		SetModal(0);
	int Count = 1 + Random() % 4;
	std::vector<TEventPlan> Plans;
	for (int i = 0; i < Count; i++) {
		TEventPlan P;
		P.Type = Random() % 30 == 0 ? (Random() % 2 == 0 ? 0 : 0x49 + (int)(Random() % 100)) : 1 + (int)(Random() % 0x48);
		P.Seed = Random();
		if (P.Type == 0x48 && Random() % 2 == 0)
			P.Type = 0x47;
		Plans.push_back(P);
		EventCovered[P.Type < 0x49 ? P.Type : 0x49]++;
	}
	bool Captured = Random() % 6 == 0;
	for (int S = 0; S < 2; S++) {
		int Saved = Side;
		Side = S;
		World[S].Capture = Captured ? (TComponent *)World[S].Main : NULL;
		for (const TEventPlan &P : Plans)
			World[S].Events.push_back(BuildEvent(P));
		Side = Saved;
	}
	sprintf(What, "ProcessEvents of round %d", Round);
	FailAt = Fail;
	Both(What, [&] { ProcessEvents(); }, [&] { At<TVoid>(0x4498a0)(); });
	FailAt = 0;
}

static void DispatchRound(int Round, int Fail)
{
	char What[96];
	ScriptWorld();
	int State = Random() % 9;
	Frames.clear();
	int FrameCount = 1 + Random() % 3;
	for (int f = 0; f < FrameCount; f++) {
		std::vector<uint8> Frame;
		for (int i = Random() % 5; i >= 0; i--)
			Frame.push_back((uint8)Random());
		Frames.push_back(Frame);
	}
	World[0].ConnectionState = World[1].ConnectionState = State;
	NextFrame[0] = NextFrame[1] = 0;
	SetReceiveData(OURS, NULL, 0);
	SetReceiveData(IMAGE, NULL, 0);
	sprintf(What, "ReceivePackets of round %d", Round);
	FailAt = Fail;
	Both(What, [&] { ReceivePackets(); }, [&] { At<TVoid>(0x44a900)(); });
	FailAt = 0;
}

// Every type byte in both tables, in order.
static void DispatchAll()
{
	for (int Login = 0; Login < 2; Login++) {
		for (int Type = 0; Type < 256; Type++) {
			ScriptWorld();
			Frames.clear();
			Frames.push_back(std::vector<uint8>{(uint8)Type});
			int State = Login ? 3 + Type % 2 : 6 + Type % 3;
			World[0].ConnectionState = World[1].ConnectionState = State;
			NextFrame[0] = NextFrame[1] = 0;
			SetReceiveData(OURS, NULL, 0);
			SetReceiveData(IMAGE, NULL, 0);
			char What[64];
			sprintf(What, "ReceivePackets type 0x%02x state %d", Type, State);
			Both(What, [&] { ReceivePackets(); }, [&] { At<TVoid>(0x44a900)(); });
			CompareStatics(What);
			FreeWorlds();
		}
	}
}

static void InitExitRound(const char *Name, int Fail)
{
	char What[96];
	ScriptWorld();
	bool Gamemaster = Random() % 2 == 0;
	sprintf(What, "InitControl(%d) %s", Gamemaster ? 1 : 0, Name);
	FailAt = Fail;
	Both(What, [&] { InitControl(Gamemaster); }, [&] { At<TBoolFn>(0x44b510)(Gamemaster); });
	sprintf(What, "ExitControl %s", Name);
	Both(What, [&] { ExitControl(); }, [&] { At<TVoid>(0x448e30)(); });
	FailAt = 0;
}

// --- The error round -----------------------------------------------------------------

static void ErrorRound(int Rounds)
{
	TErrorRound Round("Control");
	char Name[64];
	for (int Kind = 0; Kind < 3; Kind++) {
		FailKind = Kind;
		for (int Fail = 1; Fail <= 30; Fail++) {
			sprintf(Name, "failing at call %d (kind %d)", Fail, Kind);
			InitExitRound(Name, Fail);
			FreeWorlds();
		}
		for (int r = 0; r < Rounds; r++) {
			uint32 Seed = Random();
			for (int Fail = 1; Fail <= 12; Fail++) {
				SetRandomSeed(Seed);
				ScriptWorld();
				FailAt = Fail;
				Step(r);
				FailAt = 0;
				FreeWorlds();
				SetRandomSeed(Seed);
				EventRound(r, Fail);
				FreeWorlds();
				SetRandomSeed(Seed);
				DispatchRound(r, Fail);
				FreeWorlds();
			}
		}
	}
	FailKind = 0;
	Round.Finish();
}

void TestControl(const char *ExePath)
{
	int Before = Checks, BeforeFailures = Failures;
	TestName = "Control";
	TraceCalls = getenv("CT_TRACE") != NULL;
	TraceBefore = TraceCalls && atoi(getenv("CT_TRACE")) >= 2;
	LogFile = getenv("CT_LOGS") != NULL ? fopen(getenv("CT_LOGS"), "w") : NULL;
	int Rounds = getenv("CT_ROUNDS") != NULL ? atoi(getenv("CT_ROUNDS")) : 3000;
	if (getenv("CT_SEED") != NULL)
		SetRandomSeed(strtoul(getenv("CT_SEED"), NULL, 10));
	LoadObjectFlags(ExePath);
	BeginWorld();
	InstallFakes(CommunicationFakes);
	FaultHandler = ControlThrow;
	NameFunction("ExitProgram", (uint32)(uintptr_t)TestExitProgram, (void *)TestExitProgram);
	NameFunction("CloseProgram", (uint32)(uintptr_t)TestCloseProgram, (void *)TestCloseProgram);
	NameFunction("SetDisplayMode", (uint32)(uintptr_t)TestSetDisplayMode, (void *)TestSetDisplayMode);
	NameFunction("OpenURL", (uint32)(uintptr_t)TestOpenURL, (void *)TestOpenURL);
	NameFunction("Window", (uint32)(uintptr_t)TestWindow, (void *)TestWindow);
	NameFunction("HideCreature", 0x44c8c0, (void *)&HideCreature);
	NameFunction("LinkCopyWarning", 0x448290, (void *)&LinkCopyWarning);
	NameFunction("ConnectionEstablished", 0x448430, (void *)&ConnectionEstablished);
	NameFunction("ReceivePackets", 0x44a900, (void *)&ReceivePackets);
	NameFunction("ConnectionClosed", 0x448720, (void *)&ConnectionClosed);
	FindStatics();
	// Both sides' statics from the same start.
	ResetWorlds();
	Both("InitWindow", [&] { InitWindow(TestWindow, TestExitProgram, TestCloseProgram, TestSetDisplayMode, TestOpenURL); },
		[&] {
			At<void (__cdecl *)(HWND, void (*)(), void (*)(), void (*)(int), void (*)(const char *))>(0x448c50)(
				TestWindow, TestExitProgram, TestCloseProgram, TestSetDisplayMode, TestOpenURL);
		});
	FreeWorlds();
	ResetWorlds();
	for (int S = 0; S < 2; S++) {
		if (Statics[S].LastPacketType != NULL)
			*Statics[S].LastPacketType = -1;
		if (Statics[S].LastMouseMove != NULL)
			*Statics[S].LastMouseMove = 0;
		if (Statics[S].ReconnectTimeout != NULL)
			*Statics[S].ReconnectTimeout = 0;
	}
	CompareStatics("InitWindow");
	FreeWorlds();
	for (int Round = 0; Round < Rounds; Round++) {
		ScriptWorld();
		Step(Round);
		CompareStatics("step");
		FreeWorlds();
		EventRound(Round, 0);
		CompareStatics("events");
		FreeWorlds();
		if (Round % 4 == 0) {
			DispatchRound(Round, 0);
			CompareStatics("dispatch");
			FreeWorlds();
		}
		if (Round % 50 == 0) {
			InitExitRound("", 0);
			FreeWorlds();
		}
	}
	DispatchAll();
	int Least = 1 << 30;
	for (int Type = 1; Type <= 0x49; Type++)
		if (EventCovered[Type] < Least)
			Least = EventCovered[Type];
	printf("Control: %d checks, %d failures (each event type at least %d times)\n", Checks - Before,
		Failures - BeforeFailures, Least);
	ErrorRound(Rounds / 30);
	FaultHandler = NULL;
	if (LogFile != NULL)
		fclose(LogFile);
	LogFile = NULL;
}
