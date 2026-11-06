// Differential test of Control.cpp (issue #36), in communicationtest.exe (moved here from
// tools/difftest/controltest.cpp, issue #30: a golden-corpus test, golden.h).
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
// log, CT_ROUNDS=<n> the number of rounds (default 3000), CT_SEED=<n> (not with a corpus).
//
// By value (issue #30): the oracle records the original's side of every comparison as Communication does
// (commworld.cpp's CompareRun), the original's statics (their dump; OutputControlErrorInformations' text until #74),
// CloseRequest's result and curEngine. The runner runs our side alone; it compiles Control.cpp into this file
// (as network_tests.cpp compiles Network.cpp), so it reads and sets our statics by name. The comparisons that reach
// ChangeEngine retire with the oracle (the user's decision of November 4, 2026; commworld.h, RetireChangeEngine):
// ChangeEngine is the x86 engine selection in the oracle and the graphics device choice of the x64 client
// (PATELLA_VULKAN, issue #31); after each such part the original's statics and curEngine are set on our side
// (AfterRetired), and the statics' checks right after it retire too.

#include "commworld.h"

#if GOLDEN_ORACLE
#include "Control.h"
#else
// Control.cpp is compiled into this file (its statics are in an unnamed namespace), at its own warning level.
#ifdef _MSC_VER
#pragma warning(push, 3)
#endif
#include "../../src/Control.cpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "golden_patch.h"
#endif
#include "Communication.h"
#include "MainWindow.h"
#include "Status.h"
#include "Surface.h"
#include <functional>

#if GOLDEN_ORACLE
typedef void(__cdecl *TVoid)();
typedef void(__cdecl *TIntFn)(int);
typedef void(__cdecl *TInt2Fn)(int, int);
typedef void(__cdecl *TInt3Fn)(int, int, int);
typedef void(__cdecl *TBoolFn)(bool);
typedef bool(__cdecl *TBoolResult)();
#endif

// --- The original's catch wrappers and the structured exception -------------------

// The error round's structured exception (ControlThrow, kind 1): the code of RaiseException. The original (VC7.1)
// catches it in the catch (...) of its catch wrappers, which made it a TMyException ("unknown exception occurred")
// in the outermost Control.cpp function the test calls. Ours has no wrappers since Patella-Modern issue #78 and
// lets it pass, as the products (/EHsc, whose catch (...) never catches one) always did; this directory compiles
// with /EHa only so that both sides' remaining catch (...) see it (comm/CMakeLists.txt). ControlBoth makes it the
// original's TMyException at the call on our side, so that the comparison still checks that both sides throw
// there; any other exception passes unchanged, so a change of another kind still fails.
constexpr DWORD ControlStructuredException = 0xe0436f6d;

static bool RunUntilStructuredException(void (*Run)(void *), void *Argument)
{
#if defined(__linux__)
	// Linux (issue #91): RaiseException throws a TStructuredException (tests/golden/linux/windows.h).
	try {
		Run(Argument);
		return true;
	} catch (const TStructuredException &Exception) {
		if (Exception.Code != ControlStructuredException)
			throw;
		return false;
	}
#else
	__try {
		Run(Argument);
		return true;
	} __except (GetExceptionCode() == ControlStructuredException ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		return false;
	}
#endif
}

template <class FO, class FI> bool ControlBoth(const char *What, FO Ours, FI Image)
{
	auto Wrapped = [&] {
		if (!RunUntilStructuredException([](void *F) { (*static_cast<FO *>(F))(); }, &Ours))
			throw TMYEXCEPTION("unknown exception occurred", "", 0, "", 0, "");
	};
	return Both(What, Wrapped, Image);
}

// --- Fakes of Communication.cpp's functions -------------------------------------

#define COMM_FAKE(Name) \
	static void F_##Name() { Call(#Name "()"); }
COMM_FAKE(InitCommunication)
COMM_FAKE(SendFightModes)
COMM_FAKE(SendWalk)
COMM_FAKE(ReceiveLoginError)
COMM_FAKE(ReceiveLoginMessage)
COMM_FAKE(ReceiveMessageOfTheDay)
COMM_FAKE(ReceivePatch)
COMM_FAKE(ReceiveWholeFile)
COMM_FAKE(ReceivePatchBlock)
COMM_FAKE(ReceiveUpdateRequest)
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
	CF(InitCommunication, 0x401000),
	CF(SendFightModes, 0x407eb0),
	// The original's only (ours is gone, Patella-Modern issue #74): its code that sends the error reports is not
	// reached any more (events 0x2a and 0x2b, below), the fake stays in case it is.
	{"SendDebugReport", 0x40a2f0, nullptr, (void *)F_SendDebugReport},
	CF(ReceiveLoginError, 0x40a4c0),
	CF(ReceiveLoginMessage, 0x40a670),
	CF(ReceiveMessageOfTheDay, 0x40a810),
	// The original's only (ours are gone, Patella-Modern issue #75): the patch packets are not dispatched to them any
	// more (DispatchRound, DispatchAll: below), the fakes stay in case they are. Ours of the three, ReceiveUpdateRequest,
	// has no original.
	{"ReceivePatch", 0x40a9c0, nullptr, (void *)F_ReceivePatch},
	{"ReceiveWholeFile", 0x40ae70, nullptr, (void *)F_ReceiveWholeFile},
	{"ReceivePatchBlock", 0x40b290, nullptr, (void *)F_ReceivePatchBlock},
	{"ReceiveUpdateRequest", 0, (void *)&::ReceiveUpdateRequest, (void *)F_ReceiveUpdateRequest},
	CF(ReceiveCharacterList, 0x40b8b0),
	CF(ReceiveLoginServerOverloaded, 0x40bc80),
	CF(ReceiveGameLogin, 0x40be30),
	CF(ReceiveGamemasterActions, 0x40c000),
	CF(ReceiveGameError, 0x40c1b0),
	CF(ReceiveGameMessage, 0x40c360),
	CF(ReceiveLoginWait, 0x40c500),
	CF(ReceivePing, 0x40c6a0),
	CF(ReceiveAddObject, 0x40cd10),
	CF(ReceiveChangeObject, 0x40cfa0),
	CF(ReceiveDeleteObject, 0x40d440),
	CF(ReceiveOpenContainer, 0x40d7d0),
	CF(ReceiveCloseContainer, 0x40da70),
	CF(ReceiveContainerAddObject, 0x40dc20),
	CF(ReceiveContainerChangeObject, 0x40de40),
	CF(ReceiveContainerDeleteObject, 0x40e060),
	CF(ReceiveInventoryObject, 0x40e230),
	CF(ReceiveDeleteInventoryObject, 0x40e440),
	CF(ReceiveOwnTrade, 0x40e5e0),
	CF(ReceiveCounterTrade, 0x40e800),
	CF(ReceiveCloseTrade, 0x40ea30),
	CF(ReceiveWorldLight, 0x40ebc0),
	CF(ReceiveMagicEffect, 0x40ed60),
	CF(ReceiveAnimatedText, 0x40ef50),
	CF(ReceiveDistanceEffect, 0x40f150),
	CF(ReceiveCreatureMark, 0x40f380),
	CF(ReceiveCreatureHealth, 0x40f550),
	CF(ReceiveCreatureLight, 0x40f710),
	CF(ReceiveCreatureOutfit, 0x40f8e0),
	CF(ReceiveCreatureSpeed, 0x40fb20),
	CF(ReceiveCreatureSkull, 0x40fce0),
	CF(ReceiveCreatureShield, 0x40fec0),
	CF(ReceiveEditText, 0x410080),
	CF(ReceiveEditHouseText, 0x410260),
	CF(ReceivePlayerStats, 0x410410),
	CF(ReceivePlayerSkills, 0x410660),
	CF(ReceivePlayerIcons, 0x410810),
	CF(ReceiveCancelAttack, 0x4109b0),
	CF(ReceiveTalk, 0x410b40),
	CF(ReceiveChannelList, 0x411030),
	CF(ReceiveOpenChannel, 0x411230),
	CF(ReceiveOpenPrivateConversation, 0x4113f0),
	CF(ReceiveRuleViolationChannel, 0x4115a0),
	CF(ReceiveRemoveRuleViolation, 0x411760),
	CF(ReceiveCancelRuleViolation, 0x411940),
	CF(ReceiveLockRuleViolation, 0x411b20),
	CF(ReceiveOwnChannel, 0x411ce0),
	CF(ReceiveCloseChannel, 0x411ea0),
	CF(ReceiveTextMessage, 0x412060),
	CF(ReceiveOutfit, 0x412350),
	CF(ReceiveBuddy, 0x412540),
	CF(ReceiveBuddyLogin, 0x412720),
	CF(ReceiveBuddyLogout, 0x4128c0),
	CF(SendLogin, 0x413780),
	CF(SendGameLogin, 0x413de0),
	CF(SendWalk, 0x414700),
	CF(ReceiveMap, 0x415200),
	CF(ReceiveMapNorth, 0x4153f0),
	CF(ReceiveMapEast, 0x4155c0),
	CF(ReceiveMapSouth, 0x415790),
	CF(ReceiveMapWest, 0x415960),
	CF(ReceiveField, 0x415b30),
	CF(ReceiveMoveCreature, 0x415d40),
	CF(ReceiveCancelWalk, 0x416c40),
	CF(ReceiveFloorUp, 0x417020),
	CF(ReceiveFloorDown, 0x417270),
};

// --- The callbacks of InitWindow ---------------------------------------------------

static void TestExitProgram() { Call("ExitProgram()"); }
// The original's third callback, CloseProgram (event 0x48), is gone from ours (issue #75): the original still gets it.
static void TestCloseProgram() { Call("CloseProgram()"); }
static void TestSetDisplayMode(int Mode) { Call("SetDisplayMode(%d)", Mode); }
static void TestOpenURL(const char *URL) { Call("OpenURL(%s)", Quote(URL).c_str()); }
static const HWND TestWindow = (HWND)(uintptr_t)0x13572468;

// --- Failure kinds -----------------------------------------------------------------

// What the FailAt-th call throws: 0 the side's TMyException, 1 a structured
// exception (catch (...) of both sides), 2 a PROGRAM_ERROR of the side (ours in a TProgramError, issue #80).
static int FailKind;
static uint32 ThrownError;
static void ControlThrow()
{
	if (FailKind == 1) {
		RaiseException(0xe0436f6d, 0, 0, NULL);
	} else if (FailKind == 2) {
		ThrownError = 7 + Ticks[Side] % 11;
#if GOLDEN_ORACLE
		if (Side == IMAGE)
			At<void(__stdcall *)(void *, void *)>(0x511d2b)(&ThrownError, (void *)(uintptr_t)0x57b3c4);
#endif
		throw TProgramError{static_cast<PROGRAM_ERROR>(ThrownError)};
	}
	SideThrow();
}

// --- The statics ---------------------------------------------------------------------

// The image's statics (Control.cpp's .data/.bss) and ours, found by values. The image's CloseCallback (0x5c4534) is
// gone from ours with the patcher (issue #75) and not compared.
struct TControlStatics {
	int *LastPacketType, *ReconnectTimeout, *LastMouseMove;
	HWND *WindowHandle;
	void **ExitCallback, **DisplayModeCallback, **URLCallback;
};
static TControlStatics Statics[2];

#if GOLDEN_ORACLE
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
#define OUR_STATIC(Name, Set, Value, Value2) FindOurStatic(#Name, Set, Value, Value2)
#else
// The runner: our statics by name (Control.cpp is this file's); the same calls as the oracle's search, which
// leave the same state, and the same proof that the static holds the value.
static void *NamedStatic(const char *Name, void *Static, std::function<void(uint32)> Set, uint32 Value, uint32 Value2)
{
	Set(Value);
	const bool First = *(const uint32 *)Static == Value;
	Set(Value2);
	if (!First || *(const uint32 *)Static != Value2) {
		printf("FAIL controltest: our %s does not hold the values the search sets\n", Name);
		Failures++;
		return nullptr;
	}
	return Static;
}
#define OUR_STATIC(Name, Set, Value, Value2) NamedStatic(#Name, (void *)&Name, Set, Value, Value2)
#endif

#if !GOLDEN_ORACLE
static void SearchChangeEngine(int Engine) { Call("ChangeEngine(%d)", Engine); }
#endif

static void FindStatics()
{
#if GOLDEN_ORACLE
	TControlStatics &I = Statics[IMAGE];
	I.LastPacketType = (int *)(uintptr_t)0x5c0d8c;
	I.ReconnectTimeout = (int *)(uintptr_t)0x5c4520;
	I.LastMouseMove = (int *)(uintptr_t)0x5c4528;
	I.URLCallback = (void **)(uintptr_t)0x5c452c;
	I.DisplayModeCallback = (void **)(uintptr_t)0x5c4530;
	I.ExitCallback = (void **)(uintptr_t)0x5c4538;
	I.WindowHandle = (HWND *)(uintptr_t)0x5c453c;
#endif
	TControlStatics &O = Statics[OURS];
	Side = OURS;
#if !GOLDEN_ORACLE
	// The search's ProcessEvents redraws, and a surface whose Init fails makes CheckSurface call ChangeEngine:
	// in the oracle our x86 one, compared nowhere (it leaves only curEngine, which every scenario sets again
	// before a comparison reads it); here the x64 client's device choice, which the runner does not have. For the
	// search alone it is a stand-in that logs the call (the log is cleared below).
	uint8 ChangeEngineStart[5];
	golden::SaveOwn((const void *)&ChangeEngine, ChangeEngineStart);
	golden::RedirectOwn((void *)&ChangeEngine, (void *)SearchChangeEngine);
#endif
	ResetWorlds();
	auto Init = [](uint32 v) {
		InitWindow((HWND)(uintptr_t)v, (void (*)())(uintptr_t)(v + 1), (void (*)(int))(uintptr_t)(v + 3),
			(void (*)(const char *))(uintptr_t)(v + 4));
	};
	O.WindowHandle = (HWND *)OUR_STATIC(WindowHandle, [&](uint32 v) { Init(v); }, 0x7a5b3c10, 0x7a5b3d20);
	O.ExitCallback = (void **)OUR_STATIC(ExitCallback, [&](uint32 v) { Init(v - 1); }, 0x7a5b3c11, 0x7a5b3d21);
	O.DisplayModeCallback = (void **)OUR_STATIC(DisplayModeCallback, [&](uint32 v) { Init(v - 3); }, 0x7a5b3c13, 0x7a5b3d23);
	O.URLCallback = (void **)OUR_STATIC(URLCallback, [&](uint32 v) { Init(v - 4); }, 0x7a5b3c14, 0x7a5b3d24);
	O.LastMouseMove = (int *)OUR_STATIC(LastMouseMove, [](uint32 v) {
		World[OURS].ActionState = 0;
		World[OURS].ActionNumber = (int)v;
		MouseMove(1, 1); }, 0x6b5c4d30, 0x6b5c4d40);
	O.ReconnectTimeout = (int *)OUR_STATIC(ReconnectTimeout, [](uint32 v) {
		World[OURS].ConnectionState = 8;
		World[OURS].Milliseconds = (int)v - 5000;
		World[OURS].Events.clear();
		World[OURS].Events.push_back(TQueuedEvent{0x3f, 0, 0});
		World[OURS].Capture = NULL;
		ModalWindow = NULL;
		ProcessEvents(); }, 0x5c6d7e30, 0x5c6d7e40);
	// LastPacketType: a type the dispatcher knows, read from a frame.
	O.LastPacketType = (int *)OUR_STATIC(LastPacketType, [](uint32 v) {
		World[OURS].ConnectionState = 8;
		Frames.clear();
		Frames.push_back(std::vector<uint8>{(uint8)v});
		NextFrame[OURS] = 0;
		SetReceiveData(OURS, NULL, 0);
		ReceivePackets(); }, 0xd3, 0xd4);
	Log[OURS].clear();
	FreeWorlds();
#if !GOLDEN_ORACLE
	golden::RestoreOwn((void *)&ChangeEngine, ChangeEngineStart);
#endif
}

static std::string DumpStatics(int S)
{
	TControlStatics &T = Statics[S];
	if (T.LastPacketType == NULL || T.ReconnectTimeout == NULL || T.LastMouseMove == NULL || T.WindowHandle == NULL
		|| T.ExitCallback == NULL || T.DisplayModeCallback == NULL || T.URLCallback == NULL)
		return "statics not found";
	int Saved = Side;
	Side = S;
	char Buffer[512];
	sprintf(Buffer, "LastPacketType %d ReconnectTimeout %d LastMouseMove %d WindowHandle %s callbacks %s %s %s",
		*T.LastPacketType, *T.ReconnectTimeout, *T.LastMouseMove, NameOf(*T.WindowHandle).c_str(),
		NameOf(*T.ExitCallback).c_str(), NameOf(*T.DisplayModeCallback).c_str(), NameOf(*T.URLCallback).c_str());
	Side = Saved;
	return Buffer;
}


static void CompareStatics(const char *What)
{
	// After a retired comparison its statics' checks retire too (the user's decision; commworld.h).
	const bool Retired = LastRetired;
	LastRetired = false;
	if (Retired && golden::RetiredBegin("statics after ChangeEngine", Checks)) {
		golden::RetiredEnd(Checks);
		return;
	}
	std::string A = DumpStatics(OURS);
	std::string B = golden::OutputText("statics", ORIGINAL_OR(DumpStatics(IMAGE), std::string()).c_str());
	CHECK(A == B, "%s: Control.cpp's statics\n    ours:     %s\n    original: %s", What, A.c_str(), B.c_str());
	// Until Patella-Modern issue #74 OutputControlErrorInformations' text (error.txt's last packet types) was compared
	// here too; the function is gone, and its comparison with it.
	if (Retired)
		golden::RetiredEnd(Checks);
}

// What a retired comparison leaves for the next ones (commworld.h AfterRetired): the original's Control.cpp
// statics after it (by value; the oracle's at their addresses, which our statics equal there: the retired checks
// compared them) and curEngine; the runner sets ours. The callbacks and the window handle only ever hold the test's
// values (InitWindow's); their names are compared, not set.
static void ControlAfterRetired()
{
	int Numbers[7] = {};
	uint8 Types[10] = {};
	std::string Callbacks;
#if GOLDEN_ORACLE
	Numbers[0] = Global<int>(0x5c0d8c);                              // LastPacketType
	Numbers[1] = Global<int>(0x5c4510);                              // LastPacketIndex
	Numbers[2] = Global<int>(0x5c4520);                              // ReconnectTimeout
	Numbers[3] = Global<bool>(0x5c4524) ? 1 : 0;                     // Reconnect
	Numbers[4] = Global<int>(0x5c4528);                              // LastMouseMove
	Numbers[5] = Global<bool>(0x5c4554) ? 1 : 0;                     // ShowFPS
	Numbers[6] = Global<int>(0x71d0c4);                              // curEngine
	memcpy(Types, (const void *)(uintptr_t)0x5c4514, sizeof(Types)); // LastPacketTypes
	Callbacks = DumpStatics(IMAGE);
	Callbacks = Callbacks.substr(Callbacks.find(" WindowHandle"));
#endif
	golden::Output("statics after ChangeEngine", Numbers);
	golden::Output("packet types after ChangeEngine", Types);
	Callbacks = golden::OutputText("callbacks after ChangeEngine", Callbacks.c_str());
#if !GOLDEN_ORACLE
	LastPacketType = Numbers[0];
	LastPacketIndex = Numbers[1];
	ReconnectTimeout = Numbers[2];
	Reconnect = Numbers[3] != 0;
	LastMouseMove = Numbers[4];
	ShowFPS = Numbers[5] != 0;
	curEngine = Numbers[6];
	memcpy(LastPacketTypes.data(), Types, sizeof(Types));
	std::string Ours = DumpStatics(OURS);
	Ours = Ours.substr(Ours.find(" WindowHandle"));
	if (Ours != Callbacks) {
		printf("FAIL controltest: after a retired comparison our callbacks are%s, the original's%s\n", Ours.c_str(),
			Callbacks.c_str());
		Failures++;
	}
#endif
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
		curEngine = Random() % 5 == 0 ? 3 : (int)(Random() % 3);
		ORIGINAL(Global<int>(0x71d0c4) = curEngine);
	}
	if (Random() % 4 == 0) {
		int State = Random() % 12;
		World[0].ActionState = World[1].ActionState = State;
	}
}

// MouseMove with its original's catch wrapper's one call: TEND1(483, GetActionState()) read the action state once more
// for its message when an exception other than a PROGRAM_ERROR left the function. Ours has no wrapper since
// Patella-Modern issue #78, so the test makes that call on our side, where the fake logs it as the original's.
static void MouseMoveAsWrapped(int x, int y)
{
	try {
		MouseMove(x, y);
	} catch (const TProgramError &) {
		throw;
	} catch (...) {
		static_cast<void>(GetActionState());
		throw;
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
	// Step 11 calls ChangeEngine itself; a retired comparison's part ends after the step's checks (commworld.h).
	ForceReach = Kind == 11;
	DeferRetiredEnd = true;
	switch (Kind) {
	case 0:
		ControlBoth(What, [&] { MouseMoveAsWrapped(x, y); TimerEvent(); }, ORIGINAL_SIDE(At<TInt2Fn>(0x4469b0)(x, y); At<TVoid>(0x4489a0)()));
		break;
	case 1:
		ControlBoth(What, [&] { LeftButtonDown(x, y, Flags); }, ORIGINAL_SIDE(At<TInt3Fn>(0x446d10)(x, y, Flags)));
		break;
	case 2:
		ControlBoth(What, [&] { LeftButtonUp(x, y, Flags); }, ORIGINAL_SIDE(At<TInt3Fn>(0x447000)(x, y, Flags)));
		break;
	case 3:
		ControlBoth(What, [&] { LeftButtonDoubleClick(x, y, Flags); }, ORIGINAL_SIDE(At<TInt3Fn>(0x447390)(x, y, Flags)));
		break;
	case 4:
		ControlBoth(What, [&] { RightButtonDown(x, y, Flags); }, ORIGINAL_SIDE(At<TInt3Fn>(0x4476c0)(x, y, Flags)));
		break;
	case 5:
		ControlBoth(What, [&] { RightButtonUp(x, y, Flags); }, ORIGINAL_SIDE(At<TInt3Fn>(0x447880)(x, y, Flags)));
		break;
	case 6:
		ControlBoth(What, [&] { MouseWheel(x, y, Flags); }, ORIGINAL_SIDE(At<TInt3Fn>(0x447bd0)(x, y, Flags)));
		break;
	case 7:
		ControlBoth(What, [&] { KeyPress(Key); Redraw(Force); }, ORIGINAL_SIDE(At<TIntFn>(0x447e60)(Key); At<TBoolFn>(0x449210)(Force)));
		break;
	case 8: {
		bool Result[2] = {false, false};
		ControlBoth(What, [&] { Result[0] = CloseRequest(); }, ORIGINAL_SIDE(Result[1] = At<TBoolResult>(0x4480b0)()));
		if (!SkippingRetired()) {
			golden::Output("CloseRequest", Result[1]);
			CHECK(Result[0] == Result[1], "%s: CloseRequest %d, original %d", What, Result[0], Result[1]);
		}
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
		ControlBoth(What, [&] { Resize(x, y); }, ORIGINAL_SIDE(At<TInt2Fn>(0x4459b0)(x, y)));
		break;
	case 10:
		ControlBoth(What, [&] { CheckSurface(Force); }, ORIGINAL_SIDE(At<TBoolFn>(0x449010)(Force)));
		break;
	case 11:
		ControlBoth(What, [&] { ChangeEngine(Engine); }, ORIGINAL_SIDE(At<TIntFn>(0x445c20)(Engine)));
		if (!SkippingRetired()) {
			const int Original = ORIGINAL_VALUE("curEngine", int, Global<int>(0x71d0c4));
			CHECK(curEngine == Original, "%s: curEngine %d, original %d", What, curEngine, Original);
		}
		break;
	case 12:
		ControlBoth(What, [&] { Redraw(Force); }, ORIGINAL_SIDE(At<TBoolFn>(0x449210)(Force)));
		break;
	case 13:
		// SendErrorReports (event 0x2b) is gone (Patella-Modern issue #74), and its comparison with it.
		break;
	case 14:
		ControlBoth(What, [&] { EnterGame(); }, ORIGINAL_SIDE(At<TVoid>(0x4465f0)()));
		break;
	case 15:
		ControlBoth(What, [&] { LeaveGame(); }, ORIGINAL_SIDE(At<TVoid>(0x4467d0)()));
		break;
	case 16:
		ControlBoth(What, [&] { LinkCopyWarning(); }, ORIGINAL_SIDE(At<TVoid>(0x448290)()));
		break;
	case 17: {
		int State = Random() % 3 == 0 ? (int)(Random() % 9) : (Random() % 2 == 0 ? 2 : 5);
		World[0].ConnectionState = World[1].ConnectionState = State;
		ControlBoth(What, [&] { ConnectionEstablished(); }, ORIGINAL_SIDE(At<TVoid>(0x448430)()));
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
		ControlBoth(What, [&] {
			if (Pending) {
				Wd().Events.push_back(TQueuedEvent{0x3f, 0, 0});
				ProcessEvents();
				Wd().Milliseconds += Delay;
			}
			ConnectionClosed(); }, ORIGINAL_SIDE(if (Pending) {
				Wd().Events.push_back(TQueuedEvent{0x3f, 0, 0});
				At<TVoid>(0x4498a0)();
				Wd().Milliseconds += Delay; } At<TVoid>(0x448720)()));
		break;
	}
	case 19:
		ControlBoth(What, [&] { TimerEvent(); }, ORIGINAL_SIDE(At<TVoid>(0x4489a0)()));
		break;
	case 20:
		ControlBoth(What, [&] { MouseMoveAsWrapped(x, y); }, ORIGINAL_SIDE(At<TInt2Fn>(0x4469b0)(x, y)));
		break;
	case 21:
		ControlBoth(What, [&] { HandleEvents(); }, ORIGINAL_SIDE(At<TVoid>(0x44a770)()));
		break;
	case 22:
		ControlBoth(What, [&] { InitWindow(TestWindow, TestExitProgram, TestSetDisplayMode, TestOpenURL); }, ORIGINAL_SIDE(At<void(__cdecl *)(HWND, void (*)(), void (*)(), void (*)(int), void (*)(const char *))>(0x448c50)(TestWindow, TestExitProgram, TestCloseProgram, TestSetDisplayMode, TestOpenURL)));
		break;
	case 26: {
		// Ten timer ticks after a mouse move: the window under the mouse is told (hover).
		int TimerTicks = 9 + Random() % 3;
		ControlBoth(What, [&] {
			MouseMoveAsWrapped(x, y);
			for (int i = 0; i < TimerTicks; i++) {
				TimerEvent();
			} }, ORIGINAL_SIDE(At<TInt2Fn>(0x4469b0)(x, y); for (int i = 0; i < TimerTicks; i++) { At<TVoid>(0x4489a0)(); }));
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
		ControlBoth(What, [&] { Redraw(Force); }, ORIGINAL_SIDE(At<TBoolFn>(0x449210)(Force)));
	}
	}
	ForceReach = false;
	DeferRetiredEnd = false;
	EndRetiredComparison();
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
	case 0x01:
	case 0x02:
		E.Param1 = (INT_PTR)Literals[Random() % 4];
		E.Param2 = (INT_PTR)HeapText(Text);
		break;
	case 0x05:
		E.Param1 = (INT_PTR)HeapText(Text);
		E.Param2 = Random() % 100;
		break;
	case 0x06:
	case 0x07: {
		INT_PTR *Data = new INT_PTR[4];
		Data[0] = (INT_PTR)Literals[Random() % 4];
		Data[1] = (INT_PTR)Literals[Random() % 4];
		Data[2] = Random() % 2 == 0 ? 0 : (INT_PTR)Wd().Main;
		Data[3] = Random() % 50;
		E.Param1 = (INT_PTR)Data;
		break;
	}
	case 0x09:
	case 0x0a:
	case 0x18: E.Param1 = Random() % 1000; break;
	case 0x0b:
	case 0x0c: E.Param1 = (INT_PTR)Literals[Random() % 4]; break;
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
	case 0x19:
	case 0x1a:
	case 0x33: E.Param1 = (INT_PTR)HeapText(Text); break;
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
	case 0x1d:
	case 0x1e:
	case 0x24:
	case 0x3a:
	case 0x3b:
	case 0x3c:
		E.Param1 = (INT_PTR)Wd().TextWindow;
		E.Param2 = Plan.Type == 0x1e ? (INT_PTR)Wd().CurrentConsole : 0;
		break;
	case 0x29:
		E.Param1 = (INT_PTR)Literals[Random() % 4];
		E.Param2 = Random() % 5000;
		break;
	case 0x2f: E.Param2 = (INT_PTR)HeapText(Text); break;
	case 0x30:
	case 0x31:
		E.Param1 = Random() % 100;
		E.Param2 = (INT_PTR)HeapText(Text);
		break;
	case 0x32:
		E.Param1 = (INT_PTR)Wd().Buffers[Random() % WORLD_BUFFERS];
		E.Param2 = Random() % 2 == 0 ? 0 : (INT_PTR)HeapText(Text);
		break;
	case 0x35:
	case 0x36:
	case 0x37:
	case 0x39: E.Param1 = Random() % 100000; break;
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
		// Events 0x2a and 0x2b (the error report dialog, the error reports sent) are gone (Patella-Modern issue
		// #74): ours handles them as it handles a type it does not know. Each becomes such a type for both sides
		// (0x49, 0x4a), so the original's code for them is not run; the random numbers stay as they were.
		if (P.Type == 0x2a || P.Type == 0x2b)
			P.Type = 0x49 + (P.Type - 0x2a);
		// The patcher's events are gone too (issue #75): 0x29 (the update dialog), 0x40 (wait for the login
		// connection's end), 0x41-0x44 (InstallPatch), 0x45-0x47 (the data files reloaded) and 0x48 (CloseCallback,
		// then patch.exe). In the same way each becomes a type neither side knows: 0x4b, and 0x4c-0x54.
		if (P.Type == 0x29)
			P.Type = 0x4b;
		else if (P.Type >= 0x40 && P.Type <= 0x48)
			P.Type = 0x4c + (P.Type - 0x40);
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
	ControlBoth(What, [&] { ProcessEvents(); }, ORIGINAL_SIDE(At<TVoid>(0x4498a0)()));
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
		// The login table's patch packets (0x1e, 0x1f, 0x20) go to ours' ReceiveUpdateRequest since issue #75, to the
		// original's patcher in the image: in the login states each becomes a type neither table knows (0x21-0x23),
		// as ProcessEvents' removed events do. The fakes read nothing, so every byte of a frame is a type.
		if (State == 3 || State == 4)
			for (uint8 &Byte : Frame)
				if (Byte >= 0x1e && Byte <= 0x20)
					Byte = (uint8)(0x21 + (Byte - 0x1e));
		Frames.push_back(Frame);
	}
	World[0].ConnectionState = World[1].ConnectionState = State;
	NextFrame[0] = NextFrame[1] = 0;
	SetReceiveData(OURS, NULL, 0);
	SetReceiveData(IMAGE, NULL, 0);
	sprintf(What, "ReceivePackets of round %d", Round);
	FailAt = Fail;
	ControlBoth(What, [&] { ReceivePackets(); }, ORIGINAL_SIDE(At<TVoid>(0x44a900)()));
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
			// The patch packets of the login table: ours ends the login, the original patches (issue #75); not
			// compared.
			if (!Login || Type < 0x1e || Type > 0x20) {
				ControlBoth(What, [&] { ReceivePackets(); }, ORIGINAL_SIDE(At<TVoid>(0x44a900)()));
				CompareStatics(What);
			}
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
	ControlBoth(What, [&] { InitControl(Gamemaster); }, ORIGINAL_SIDE(At<TBoolFn>(0x44b510)(Gamemaster)));
	sprintf(What, "ExitControl %s", Name);
	ControlBoth(What, [&] { ExitControl(); }, ORIGINAL_SIDE(At<TVoid>(0x448e30)()));
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
	RecordComparisons = true; // a ported test: its comparisons go into the corpus
	RetireChangeEngine = true;
	AfterRetired = ControlAfterRetired;
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
	ControlBoth("InitWindow", [&] { InitWindow(TestWindow, TestExitProgram, TestSetDisplayMode, TestOpenURL); }, ORIGINAL_SIDE(At<void(__cdecl *)(HWND, void (*)(), void (*)(), void (*)(int), void (*)(const char *))>(0x448c50)(TestWindow, TestExitProgram, TestCloseProgram, TestSetDisplayMode, TestOpenURL)));
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
		if (Type != 0x29 && Type != 0x2a && Type != 0x2b && (Type < 0x40 || Type > 0x48) && EventCovered[Type] < Least)
			Least = EventCovered[Type];
	printf("Control: %d checks, %d failures (each event type at least %d times)\n", Checks - Before,
		Failures - BeforeFailures, Least);
	ErrorRound(Rounds / 30);
	FaultHandler = NULL;
	EndWorld();
	AfterRetired = nullptr;
	RetireChangeEngine = false;
	RecordComparisons = false;
	if (LogFile != NULL)
		fclose(LogFile);
	LogFile = NULL;
}
