// Differential test of Communication.cpp (issue #36), in communicationtest.exe (moved here from
// tools/difftest/communicationtest.cpp, issue #30: a golden-corpus test, golden.h).
//
// Server packets of every type both dispatch tables of ReceivePackets know
// (the login server's and the game server's), valid, truncated, with bytes
// changed or appended, and of unknown types, go through the image's
// ReceivePackets (0x44a900) and ours; seeds are the frames of issue #19's
// recordings (commseeds.h), the rest is generated with the same object flags
// as the fake GetObjectFlag answers (Tibia.dat's). Every function below
// Communication.cpp is a fake of commworld.cpp on both sides, so the
// comparison is every call it makes with its arguments, the packets it
// builds (SendCommand logs SendBuffer), the creature records and text
// buffers it writes, the receive buffer's position and what it throws.
// Every sender is called directly with random and edge arguments, and the
// readers (ReadCreature, ReadField, ReadMap, ReadFloor) directly too. The
// error round makes the n-th fake call of each packet and sender throw, for
// every n, and calls the senders with the arguments their assertions refuse.
//
// CM_TRACE=1 prints every comparison, CM_LOGS=<file> writes every compared
// log, CM_ROUNDS=<n> the number of generated frames (default 6000),
// CM_SEED=<n> changes the inputs (not with a corpus).
//
// By value (issue #30): the oracle records the original's side of every comparison (commworld.cpp's
// CompareRun: what it threw, its log, its creature records, its buffers' and world's dumps), the readers'
// results, the first byte of the image's empty Exit function, and Tibia.dat's flag table (LoadObjectFlags).
// The runner runs our side alone (every image call is inside ORIGINAL or ORIGINAL_SIDE) against the same
// fakes, and compares with the recorded original. The error round's structured exceptions (the failing RSA's)
// are caught by both sides' catch (...): the runner compiles this file, commworld.cpp, Communication.cpp and
// Control.cpp with /EHa, as the oracle does (tests/golden/comm/CMakeLists.txt).
//
// The patcher is gone from ours (Patella-Modern issue #75, decision D9): a login frame with a patch packet (0x1e,
// 0x1f, 0x20) ends the login in ours (ReceiveUpdateRequest) where the original patches, so those frames are not
// compared; their random numbers are still drawn, as they were, so that every other comparison keeps its inputs.
// TestUpdateRequests checks ours alone on such frames.

#include "commworld.h"
#include "commseeds.h"
#if !GOLDEN_ORACLE
#include "golden_patch.h"
#endif

#include "compat/mfc_shim.h"
#include "Communication.h"
#include "Control.h"
#include "MainWindow.h"
#include "Network.h"
#include "Status.h"
#include "TRSA.h"
#include <string.h>

// --- What difftest.exe's other test files provide -------------------------
// (The runner has them: status_tests.cpp's PatchImage, network_tests.cpp's CAsyncSocket and AfxSocketInit,
// mainfrm_tests.cpp's MainFrm.cpp.)

#if GOLDEN_ORACLE
// imageeh.cpp's EnableImageExceptions calls it (tests/golden/status_tests.cpp's in difftest.exe):
// the image's malloc and free are ours.
static void *__cdecl CommImageMalloc(size_t Size) { return malloc(Size); }
static void __cdecl CommImageFree(void *Block) { free(Block); }
void PatchImage()
{
	static bool Done = false;
	if (Done)
		return;
	Done = true;
	PatchJump(0x512d18, (void *)CommImageMalloc);
	PatchJump(0x51165d, (void *)CommImageFree);
}

// Network.cpp's socket is MFC's CAsyncSocket, which the shim only declares:
// nothing in the test opens a socket (Network.cpp's connecting functions are
// faked).
static void SocketTrap(const char *Name)
{
	printf("crash: communicationtest: CAsyncSocket::%s called\n", Name);
	fflush(stdout);
	abort();
}
CAsyncSocket::CAsyncSocket() { m_hSocket = INVALID_SOCKET; }
CAsyncSocket::~CAsyncSocket() {}
BOOL CAsyncSocket::Create(UINT, int, long, LPCTSTR)
{
	SocketTrap("Create");
	return FALSE;
}
BOOL CAsyncSocket::Connect(LPCTSTR, UINT)
{
	SocketTrap("Connect");
	return FALSE;
}
void CAsyncSocket::Close() { SocketTrap("Close"); }
int CAsyncSocket::Receive(void *, int, int)
{
	SocketTrap("Receive");
	return 0;
}
int CAsyncSocket::Send(const void *, int, int)
{
	SocketTrap("Send");
	return 0;
}
void CAsyncSocket::OnReceive(int) {}
void CAsyncSocket::OnSend(int) {}
void CAsyncSocket::OnOutOfBandData(int) {}
void CAsyncSocket::OnAccept(int) {}
void CAsyncSocket::OnConnect(int) {}
void CAsyncSocket::OnClose(int) {}
BOOL AfxSocketInit(WSADATA *)
{
	SocketTrap("AfxSocketInit");
	return FALSE;
}

// MainFrm.cpp's globals (MainFrm.cpp and Player.cpp are not linked: MFC's
// frame window and application classes, which the shim only declares).
std::array<uint8, 16> PacketHeader;
bool DisplayChanged;
#endif

// --- Control.cpp's functions that ReceivePackets calls -----------------------

// Compared by controltest.cpp; here they only log.
static void F_EnterGame() { Call("EnterGame()"); }
static void F_HandleEvents() { Call("HandleEvents()"); }
static const TFake ControlFakes[] = {
	{"EnterGame", 0x4465f0, (void *)&EnterGame, (void *)F_EnterGame},
	{"HandleEvents", 0x44a770, (void *)&HandleEvents, (void *)F_HandleEvents},
};

// --- Values ------------------------------------------------------------------------

static int Pick8()
{
	static const int Edges[] = {0, 1, 2, 3, 0x7f, 0x80, 0xfe, 0xff};
	return Random() % 3 == 0 ? Edges[Random() % 8] : (int)(Random() % 256);
}
static int Pick16()
{
	static const int Edges[] = {0, 1, 0xff, 0x100, 0x7fff, 0x8000, 0xfffe, 0xffff};
	switch (Random() % 4) {
	case 0: return Edges[Random() % 8];
	case 1: return (int)(Random() % 20);
	default: return (int)(Random() % 0x10000);
	}
}
static uint32 PickID()
{
	TWorld &W = World[OURS];
	switch (Random() % 6) {
	case 0: return 0;
	case 1: return Random32();
	case 2: return 0xffffffff;
	default: return W.Creatures[Random() % WORLD_CREATURES].CreatureID;
	}
}
static uint32 Pick32()
{
	static const uint32 Edges[] = {0, 1, 0x7fffffff, 0x80000000, 0xffffffff, 0x10000, 0xffff};
	return Random() % 3 == 0 ? Edges[Random() % 7] : Random32();
}
static std::string PickText(int Long = 0)
{
	int Length;
	switch (Random() % 20) {
	case 0:
	case 1: Length = 0; break;
	case 2: Length = 29 + Random() % 3; break;
	case 3: Length = 30 + Random() % 40; break;
	case 4: Length = Long ? 500 + Random() % 300 : Random() % 60; break;
	case 5: Length = Long ? 1000 + Random() % 1500 : 1; break;
	default: Length = 1 + Random() % 25;
	}
	char *Buffer = new char[Length + 1];
	RandomText(Buffer, Length);
	for (int i = 0; i < Length; i++)
		if (Buffer[i] == 0)
			Buffer[i] = 'x';
	std::string Text(Buffer, Length);
	delete[] Buffer;
	if (Random() % 8 == 0) {
		static const char *const Names[] = {"Alice", "alice", "Bob", "Carol", "Patella Tester", "a b", "Alicia"};
		Text = Names[Random() % 7];
	}
	return Text;
}

// --- Packets -----------------------------------------------------------------------

struct TPacket {
	std::vector<uint8> Data;
	void B(int v) { Data.push_back((uint8)v); }
	void W(int v)
	{
		B(v);
		B(v >> 8);
	}
	void Q(uint32 v)
	{
		W((int)(v & 0xffff));
		W((int)(v >> 16));
	}
	void S(const std::string &Text)
	{
		W((int)Text.size());
		Data.insert(Data.end(), Text.begin(), Text.end());
	}
	void Pos()
	{
		W(Pick16() % 3 == 0 ? Pick16() : 32000 + (int)(Random() % 100));
		W(Pick16() % 3 == 0 ? Pick16() : 31000 + (int)(Random() % 100));
		B(Random() % 4 == 0 ? Pick8() : (int)(Random() % 16));
	}
};

static int PickType()
{
	switch (Random() % 12) {
	case 0: return 0x61 + Random() % 3;
	case 1: return Pick16();
	case 2: return 0;
	default: return 100 + Random() % (MaxObjectType() - 99);
	}
}

// The creature data after a creature type (ReadCreature).
static void GenCreature(TPacket &P, int Type)
{
	if (Type == 0x61) {
		P.Q(PickID());
		P.Q(PickID());
		P.S(PickText());
	} else {
		P.Q(PickID());
		if (Type == 0x63) {
			P.B(Pick8() % 5);
			return;
		}
	}
	P.B(Pick8());
	P.B(Pick8() % 5);
	int Outfit = Random() % 3 == 0 ? 0 : Pick16();
	P.W(Outfit);
	if (Outfit == 0) {
		P.W(PickType());
	} else {
		for (int i = 0; i < 4; i++)
			P.B(Pick8());
	}
	P.B(Pick8());
	P.B(Pick8());
	P.W(Pick16());
	P.B(Pick8() % 7);
	P.B(Pick8() % 6);
}

// An object: its type and the bytes its flags announce (or a creature).
static void GenObject(TPacket &P, bool Creatures)
{
	int Type = PickType();
	if (!Creatures && Type >= 0x61 && Type <= 0x63)
		Type = 100;
	P.W(Type);
	if (Type >= 0x61 && Type <= 0x63) {
		GenCreature(P, Type);
		return;
	}
	if (ObjectFlag(Type, 5))
		P.B(Pick8());
	if (ObjectFlag(Type, 11))
		P.B(Pick8());
	if (ObjectFlag(Type, 10))
		P.B(Pick8());
}

// Map data for Count fields: objects, then 0xff00 + the number of fields to skip.
static void GenFields(TPacket &P, int Count)
{
	int Dense = Random() % 4;
	for (int Field = 0; Field < Count;) {
		int Objects = Random() % (Dense == 0 ? 2 : 5) == 0 ? 1 + Random() % 3 : 0;
		for (int i = 0; i < Objects; i++)
			GenObject(P, true);
		int Skip = Random() % 3 == 0 ? (int)(Random() % 256) : (int)(Random() % 20);
		if (Random() % 40 == 0)
			Skip = Count;
		P.W(0xff00 + Skip);
		Field += 1 + Skip;
	}
}

static int Floors(int z) // ReadMap's floors
{
	if (z < 8)
		return 8;
	int End = z - 14 < -1 ? -1 : z - 14;
	return 4 - End;
}

// The model of the player's floor while frames are generated.
static int ModelZ;

static void GenGame(TPacket &P, int Type)
{
	P.B(Type);
	switch (Type) {
	case 0x0a:
		P.Q(PickID());
		P.W(Pick16());
		P.B(Pick8());
		break;
	case 0x0b:
		for (int i = 0; i < 32; i++) P.B(Random() % 4 == 0 ? Pick8() : (int)(Random() % 2));
		break;
	case 0x14:
	case 0x15: P.S(PickText(1)); break;
	case 0x16:
		P.S(PickText(1));
		P.B(Pick8());
		break;
	case 0x1e: break;
	case 0x64: {
		P.W(32000 + Random() % 100);
		P.W(31000 + Random() % 100);
		int z = Random() % 5 == 0 ? Pick8() % 16 : (int)(Random() % 16);
		P.B(z);
		ModelZ = z;
		GenFields(P, Floors(z) * 18 * 14);
		break;
	}
	case 0x65:
	case 0x67: GenFields(P, Floors(ModelZ) * 18); break;
	case 0x66:
	case 0x68: GenFields(P, Floors(ModelZ) * 14); break;
	case 0x69:
		P.Pos();
		GenFields(P, 1);
		break;
	case 0x6a:
		P.Pos();
		GenObject(P, true);
		break;
	case 0x6b:
		P.Pos();
		P.B(Pick8() % 12);
		GenObject(P, true);
		break;
	case 0x6c:
		P.Pos();
		P.B(Pick8() % 12);
		break;
	case 0x6d: {
		int x = 32000 + Random() % 100, y = 31000 + Random() % 100, z = Random() % 16;
		P.W(x);
		P.W(y);
		P.B(z);
		P.B(Pick8() % 12);
		switch (Random() % 4) {
		case 0: P.Pos(); break;
		case 1:
			P.W(x + (int)(Random() % 5) - 2);
			P.W(y + (int)(Random() % 5) - 2);
			P.B(z + (Random() % 5 == 0 ? 1 : 0));
			break;
		default:
			P.W(x + (int)(Random() % 3) - 1);
			P.W(y + (int)(Random() % 3) - 1);
			P.B(z);
			break;
		}
		break;
	}
	case 0x6e: {
		P.B(Pick8() % 20);
		P.W(PickType());
		P.S(PickText());
		P.B(Pick8());
		P.B(Random() % 4 == 0 ? Pick8() : (int)(Random() % 2));
		int Count = Random() % 4 == 0 ? Pick8() % 40 : (int)(Random() % 8);
		P.B(Count);
		for (int i = 0; i < Count; i++)
			GenObject(P, false);
		break;
	}
	case 0x6f: P.B(Pick8() % 20); break;
	case 0x70:
		P.B(Pick8() % 20);
		GenObject(P, false);
		break;
	case 0x71:
		P.B(Pick8() % 20);
		P.B(Pick8());
		GenObject(P, false);
		break;
	case 0x72:
		P.B(Pick8() % 20);
		P.B(Pick8());
		break;
	case 0x78:
		P.B(Pick8() % 12);
		GenObject(P, false);
		break;
	case 0x79: P.B(Pick8() % 12); break;
	case 0x7d:
	case 0x7e: {
		P.S(PickText());
		int Count = Random() % 4 == 0 ? Pick8() % 40 : (int)(Random() % 8);
		P.B(Count);
		for (int i = 0; i < Count; i++)
			GenObject(P, false);
		break;
	}
	case 0x7f: break;
	case 0x82:
		P.B(Pick8());
		P.B(Pick8());
		break;
	case 0x83:
		P.Pos();
		P.B(Pick8());
		break;
	case 0x84:
		P.Pos();
		P.B(Pick8());
		P.S(PickText());
		break;
	case 0x85:
		P.Pos();
		P.Pos();
		P.B(Pick8());
		break;
	case 0x86:
	case 0x8c:
	case 0x90:
	case 0x91:
		P.Q(PickID());
		P.B(Pick8());
		break;
	case 0x8d:
		P.Q(PickID());
		P.B(Pick8());
		P.B(Pick8());
		break;
	case 0x8e: {
		P.Q(PickID());
		int Outfit = Random() % 3 == 0 ? 0 : Pick16();
		P.W(Outfit);
		if (Outfit == 0)
			P.W(PickType());
		else
			for (int i = 0; i < 4; i++)
				P.B(Pick8());
		break;
	}
	case 0x8f:
		P.Q(PickID());
		P.W(Pick16());
		break;
	case 0x96:
		P.Q(Pick32());
		P.W(Pick16());
		P.W(Pick16());
		P.S(PickText(1));
		P.S(PickText());
		break;
	case 0x97:
		P.B(Pick8());
		P.Q(Pick32());
		P.S(PickText(1));
		break;
	case 0xa0:
		P.W(Pick16());
		P.W(Pick16());
		P.W(Pick16());
		P.Q(Pick32());
		P.W(Pick16());
		P.B(Pick8());
		P.W(Pick16());
		P.W(Pick16());
		P.B(Pick8());
		P.B(Pick8());
		P.B(Pick8());
		break;
	case 0xa1:
		for (int i = 0; i < 14; i++) P.B(Pick8());
		break;
	case 0xa2: P.B(Pick8()); break;
	case 0xa3: break;
	case 0xaa: {
		P.Q(Pick32());
		P.S(PickText());
		int Mode = Random() % 6 == 0 ? Pick8() : (int)(1 + Random() % 17);
		P.B(Mode);
		switch (Mode) {
		case 1:
		case 2:
		case 3:
		case 16:
		case 17: P.Pos(); break;
		case 5:
		case 10:
		case 12:
		case 14: P.W(Pick16()); break;
		case 6: P.Q(Pick32()); break;
		}
		P.S(PickText(1));
		break;
	}
	case 0xab: {
		int Count = Random() % 5 == 0 ? Pick8() : (int)(Random() % 6);
		P.B(Count);
		for (int i = 0; i < Count; i++) {
			P.W(Pick16());
			P.S(PickText());
		}
		break;
	}
	case 0xac:
	case 0xb2:
		P.W(Pick16());
		P.S(PickText());
		break;
	case 0xad:
	case 0xaf:
	case 0xb0: P.S(PickText()); break;
	case 0xae:
	case 0xb3: P.W(Pick16()); break;
	case 0xb1: break;
	case 0xb4:
		P.B(Random() % 4 == 0 ? Pick8() : (int)(Random() % 26));
		P.S(PickText(1));
		break;
	case 0xb5: P.B(Pick8() % 9); break;
	case 0xbe:
		if (ModelZ - 1 == 7)
			GenFields(P, 6 * 18 * 14);
		else if (ModelZ - 1 >= 8)
			GenFields(P, 18 * 14);
		ModelZ--;
		break;
	case 0xbf:
		if (ModelZ + 1 == 8)
			GenFields(P, 3 * 18 * 14);
		else if (ModelZ + 1 > 8 && ModelZ + 1 < 14)
			GenFields(P, 18 * 14);
		ModelZ++;
		break;
	case 0xc8:
		P.W(Pick16());
		for (int i = 0; i < 4; i++) P.B(Pick8());
		P.W(Pick16());
		P.W(Pick16());
		break;
	case 0xd2:
		P.Q(PickID());
		P.S(PickText());
		P.B(Random() % 3 == 0 ? Pick8() : (int)(Random() % 2));
		break;
	case 0xd3:
	case 0xd4: P.Q(PickID()); break;
	default: // an unknown type: random bytes
		for (int i = Random() % 6; i > 0; i--)
			P.B(Pick8());
	}
}

static void GenLogin(TPacket &P, int Type)
{
	P.B(Type);
	switch (Type) {
	case 0x0a:
	case 0x0b:
	case 0x14: P.S(PickText(1)); break;
	case 0x1e:
	case 0x1f:
		P.B(Random() % 4 == 0 ? Pick8() : (int)(Random() % 6));
		P.Q(Random() % 6 == 0 ? 0 : Random() % 3000);
		break;
	case 0x20: {
		int Size = Random() % 4 == 0 ? (int)(Random() % 3500) : (int)(Random() % 200);
		P.W(Size);
		for (int i = 0; i < Size; i++)
			P.B(Random());
		break;
	}
	case 0x28: break;
	case 0x64: {
		int Count = Random() % 5 == 0 ? Pick8() % 40 : (int)(Random() % 5);
		P.B(Count);
		for (int i = 0; i < Count; i++) {
			P.S(PickText());
			P.S(PickText());
			for (int j = 0; j < 4; j++)
				P.B(Pick8());
			P.W(Pick16());
		}
		P.W(Pick16());
		break;
	}
	default:
		for (int i = Random() % 6; i > 0; i--)
			P.B(Pick8());
	}
}

static const int GameTypes[] = {0x0a, 0x0b, 0x14, 0x15, 0x16, 0x1e, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
	0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x78, 0x79, 0x7d, 0x7e, 0x7f, 0x82, 0x83, 0x84, 0x85, 0x86, 0x8c, 0x8d,
	0x8e, 0x8f, 0x90, 0x91, 0x96, 0x97, 0xa0, 0xa1, 0xa2, 0xa3, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2,
	0xb3, 0xb4, 0xb5, 0xbe, 0xbf, 0xc8, 0xd2, 0xd3, 0xd4};
static const int LoginTypes[] = {0x0a, 0x0b, 0x14, 0x1e, 0x1f, 0x20, 0x28, 0x64};
static const int GameTypeCount = sizeof(GameTypes) / sizeof(GameTypes[0]);
// The login server's patch packets (the original's ReceivePatch, ReceiveWholeFile and ReceivePatchBlock; ours ends
// the login: ReceiveUpdateRequest, issue #75).
static bool IsPatchMessage(int Type) { return Type == 0x1e || Type == 0x1f || Type == 0x20; }
static const int LoginTypeCount = sizeof(LoginTypes) / sizeof(LoginTypes[0]);

// Changes a frame: truncates it, changes bytes, appends bytes. A login
// frame gets no byte that could become a patch message (0x1e, 0x1f, 0x20):
// until issue #75 the original's and our ReceivePatch and ReceiveWholeFile
// set the new size before they asserted that no transfer was running, and a
// block after such a failed start wrote past the old buffer on both sides;
// since then such a message ends the login in ours alone. A frame that starts
// a patch keeps its bytes (its size: a huge new[] failed differently in the
// two runtimes).
static int MutationByte(bool Login)
{
	for (;;) {
		int Byte = Pick8();
		if (!Login || (Byte != 0x1e && Byte != 0x1f && Byte != 0x20))
			return Byte;
	}
}

static void Mutate(std::vector<uint8> &Frame, bool Login, bool Patch)
{
	switch (Random() % 8) {
	case 0:
		if (!Frame.empty())
			Frame.resize(Random() % Frame.size());
		break;
	case 1:
		if (!Patch && !Frame.empty()) {
			for (int i = 1 + Random() % 3; i > 0; i--)
				Frame[Random() % Frame.size()] = (uint8)MutationByte(Login);
		}
		break;
	case 2:
		for (int i = 1 + Random() % 4; i > 0; i--)
			Frame.push_back((uint8)MutationByte(Login));
		break;
	}
}

// --- Rounds ------------------------------------------------------------------------

#if GOLDEN_ORACLE
typedef void(__cdecl *TVoid)();
#endif
static int Covered[2][256]; // [login][type]: frames that started with the type

// Runs the frames through both sides' ReceivePackets in a connection state.
static bool RunFrames(const char *What, int ConnectionState)
{
	World[OURS].ConnectionState = World[IMAGE].ConnectionState = ConnectionState;
	NextFrame[0] = NextFrame[1] = 0;
	SetReceiveData(OURS, NULL, 0);
	SetReceiveData(IMAGE, NULL, 0);
	for (auto &F : Frames) {
		if (!F.empty())
			Covered[ConnectionState == 3 || ConnectionState == 4][F[0]]++;
		if (TraceCalls) {
			printf("%s: state %d frame %s\n", What, ConnectionState, Hex(F.data(), (int)F.size()).substr(0, 600).c_str());
			fflush(stdout);
		}
	}
	// Under AddressSanitizer (the preset windows-x64-asan, issue #70) our side fills the stack with the pattern again
	// right before its call, from the same frame: ReceivePackets passes its Container, not yet assigned, to
	// ReceiveOpenContainer and ReceiveCloseContainer, whose TEND prints it when the first GetByte throws (the
	// original's read, issue #30), and ASan's Debug frames (larger, their slots shared between scopes) let what Both
	// calls after its FillStack land on that slot. Without ASan the call stays as it was: the depth of
	// ReceivePackets' frame decides what the slot holds.
#if defined(__SANITIZE_ADDRESS__)
	return Both(
		What,
		[&] {
			FillStack();
			ReceivePackets();
		},
		ORIGINAL_SIDE(At<TVoid>(0x44a900)()));
#else
	return Both(What, [&] { ReceivePackets(); }, ORIGINAL_SIDE(At<TVoid>(0x44a900)()));
#endif
}

static void SetRand(uint32 Seed);

static void TestSeeds()
{
	char What[64];
	int Count = sizeof(SeedFrames) / sizeof(SeedFrames[0]);
	for (int Round = 0; Round < Count * 6; Round++) {
		const TSeedFrame &Seed = SeedFrames[Round % Count];
		ResetWorlds();
		SetRand(Random());
		Frames.clear();
		Frames.push_back(std::vector<uint8>(Seed.Data, Seed.Data + Seed.Size));
		if (Round >= Count)
			Mutate(Frames.back(), Seed.Login, false);
		sprintf(What, "seed frame %d (round %d)", Round % Count, Round);
		int State = Seed.Login ? 3 + Random() % 2 : 6 + Random() % 3;
		RunFrames(What, State);
		// The original's CancelPatch after a login frame is gone with ours (issue #75).
		FreeWorlds();
	}
}

static void TestGenerated(int Rounds)
{
	char What[64];
	for (int Round = 0; Round < Rounds; Round++) {
		ResetWorlds();
		SetRand(Random());
		ModelZ = World[OURS].MapZ;
		bool Login = Random() % 6 == 0;
		Frames.clear();
		int FrameCount = 1 + Random() % 3;
		bool Started = false;      // one patch start per round (see Mutate)
		bool PatchMessage = false; // a patch packet in a login round: not compared (issue #75)
		for (int f = 0; f < FrameCount; f++) {
			TPacket P;
			int Messages = Random() % 3 == 0 ? 1 + Random() % 4 : 1;
			bool Patch = false;
			for (int m = 0; m < Messages; m++) {
				int Type;
				if (Random() % 40 == 0)
					Type = Random() % 256;
				else
					Type = Login ? LoginTypes[Random() % LoginTypeCount] : GameTypes[Random() % GameTypeCount];
				if (Login && (Type == 0x1e || Type == 0x1f)) {
					if (Started)
						Type = 0x0b;
					Started = true;
					Patch = true;
				}
				if (Login && IsPatchMessage(Type))
					PatchMessage = true;
				if (Login)
					GenLogin(P, Type);
				else
					GenGame(P, Type);
			}
			if (Random() % 5 == 0)
				Mutate(P.Data, Login, Patch);
			Frames.push_back(P.Data);
		}
		sprintf(What, "frames of round %d", Round);
		int State = Login ? 3 + Random() % 2 : 6 + Random() % 3;
		// A login round with a patch packet is not compared (the original patches, ours ends the login: issue #75);
		// the original's CancelPatch after a login round is gone with ours.
		if (!PatchMessage)
			RunFrames(What, State);
		FreeWorlds();
	}
}

// Cancelled steps at one position: from the 17th in a row the client stops
// attacking and sends a cancel (ReceiveCancelWalk's counter).
static void TestCancelWalks(int Rounds)
{
	char What[64];
	for (int Round = 0; Round < Rounds; Round++) {
		ResetWorlds();
		SetRand(Random());
		Frames.clear();
		TPacket P;
		int Count = 10 + Random() % 30;
		for (int i = 0; i < Count; i++) {
			P.B(0xb5);
			P.B(Random() % 9);
		}
		Frames.push_back(P.Data);
		sprintf(What, "cancelled steps of round %d", Round);
		RunFrames(What, 8);
		FreeWorlds();
	}
}

// A patch over several frames: the patch, its blocks, sometimes more or less. Not compared since issue #75 (the
// original patches, ours ends the login at the first packet): the frames are still made, so that the later tests keep
// their inputs; TestUpdateRequests runs ours alone on such frames.
static void MakePatchFrames()
{
	TPacket P;
	int Size = Random() % 4 == 0 ? 0 : (int)(Random() % 2000);
	P.B(Random() % 2 == 0 ? 0x1e : 0x1f);
	P.B(Random() % 5 == 0 ? Pick8() % 8 : (int)(Random() % 5));
	P.Q(Size);
	Frames.push_back(P.Data);
	int Left = Size + (Random() % 6 == 0 ? 10 : 0);
	while (Left > 0 && Frames.size() < 20) {
		TPacket B;
		int Block = Random() % 3 == 0 ? Left : 1 + (int)(Random() % (Left < 700 ? Left : 700));
		B.B(0x20);
		B.W(Block);
		for (int i = 0; i < Block; i++)
			B.B(Random());
		Frames.push_back(B.Data);
		Left -= Block;
	}
	if (Random() % 3 == 0)
		Frames.push_back(std::vector<uint8>{0x20, 0x00, 0x00});
	// A second start while a transfer may be running (until issue #75 the assertions of ReceivePatch and
	// ReceiveWholeFile).
	if (Random() % 3 == 0) {
		TPacket Again;
		Again.B(Random() % 2 == 0 ? 0x1e : 0x1f);
		Again.B(Random() % 5);
		Again.Q(Random() % 3000);
		Frames.push_back(Again.Data);
	}
}

static void TestPatches(int Rounds)
{
	for (int Round = 0; Round < Rounds; Round++) {
		ResetWorlds();
		SetRand(Random());
		Frames.clear();
		MakePatchFrames();
		FreeWorlds();
	}
}

// --- The senders -------------------------------------------------------------------

static void SetRand(uint32 Seed)
{
	extern uint32 CommRandState(int, uint32);
	CommRandState(OURS, Seed);
	CommRandState(IMAGE, Seed);
}

#if GOLDEN_ORACLE
typedef void(__cdecl *TSendI)(int);
typedef void(__cdecl *TSendU)(uint32);
typedef void(__cdecl *TSendS)(const char *);
typedef void(__cdecl *TSendIS)(int, const char *);
typedef void(__cdecl *TSendISS)(int, const char *, const char *);
typedef void(__cdecl *TSendIIS)(int, int, const char *);
typedef void(__cdecl *TSendUS)(uint32, const char *);
typedef void(__cdecl *TSendUSS)(uint32, const char *, const char *);
typedef void(__cdecl *TSendIUS)(int, uint32, const char *);
typedef void(__cdecl *TSendII)(int, int);
typedef void(__cdecl *TSendIII)(int, int, int);
typedef void(__cdecl *TSend5)(int, int, int, int, int);
typedef void(__cdecl *TSend6)(int, int, int, int, int, int);
typedef void(__cdecl *TSend6U)(int, int, int, int, int, uint32);
typedef void(__cdecl *TSend9)(int, int, int, int, int, int, int, int, int);
typedef void(__cdecl *TSend10)(int, int, int, int, int, int, int, int, int, int);
typedef void(__cdecl *TSendOutfit)(int, const int *);
typedef void(__cdecl *TSendReport)(const char *, uint8, uint8, const char *, uint32, bool);
#endif

static int Coordinate()
{
	switch (Random() % 5) {
	case 0: return Pick16();
	case 1: return 0xffff;
	case 2: return (int)Pick32();
	default: return 32000 + Random() % 100;
	}
}
static int Small() { return Random() % 4 == 0 ? (int)Pick32() : Pick8(); }

struct TSender {
	const char *Name;
	int Count;
};
static TSender Senders[64];
static int SenderCount;

// One random call of every sender; Valid keeps the assertions' arguments in range.
static void CallSenders(const char *Round, bool Valid)
{
	char What[96];
	int Index = 0;
#define SEND(Fn, OurCall, ImageCall) \
	do { \
		sprintf(What, "%s %s", #Fn, Round); \
		SetRand(Random()); \
		Both(What, [&] { OurCall; }, ORIGINAL_SIDE(ImageCall)); \
		Senders[Index].Name = #Fn; \
		Senders[Index].Count++; \
		Index++; \
	} while (0)
	uint32 a = Pick32();
	// T3 and T4 were SendDebugReport's (issue #74): still drawn, see below.
	std::string T1 = PickText(1), T2 = PickText(1), T3 = PickText(), T4 = PickText();
	const char *S1 = !Valid && Random() % 4 == 0 ? NULL : T1.c_str();
	const char *S2 = !Valid && Random() % 4 == 0 ? NULL : T2.c_str();
	SEND(SendLogin, SendLogin(a, S1), At<TSendUS>(0x413780)(a, S1));
	SEND(SendGameLogin, SendGameLogin(a, S1, S2), At<TSendUSS>(0x413de0)(a, S1, S2));
	SEND(SendLogout, SendLogout(), At<TVoid>(0x404350)());
	SEND(SendPing, SendPing(), At<TVoid>(0x404590)());
	SEND(PrepareMovement, PrepareMovement(), At<TVoid>(0x414530)());
	SEND(SendWalk, SendWalk(), At<TVoid>(0x414700)());
	SEND(SendStop, SendStop(), At<TVoid>(0x414a10)());
	SEND(SendTurnNorth, SendTurnNorth(), At<TVoid>(0x404730)());
	SEND(SendTurnEast, SendTurnEast(), At<TVoid>(0x4048d0)());
	SEND(SendTurnSouth, SendTurnSouth(), At<TVoid>(0x404a70)());
	SEND(SendTurnWest, SendTurnWest(), At<TVoid>(0x404c10)());
	int x = Coordinate(), y = Coordinate(), z = Small(), Type = Random() % 3 == 0 ? (int)Pick32() : PickType();
	int Pos = Small(), x2 = Coordinate(), y2 = Coordinate(), z2 = Small(), Type2 = PickType(), Pos2 = Small(), Count = Small();
	SEND(SendMoveObject, SendMoveObject(x, y, z, Type, Pos, x2, y2, z2, Count), At<TSend9>(0x404db0)(x, y, z, Type, Pos, x2, y2, z2, Count));
	SEND(SendRequestTrade, SendRequestTrade(x, y, z, Type, Pos, a), At<TSend6U>(0x405000)(x, y, z, Type, Pos, a));
	SEND(SendInspectTrade, SendInspectTrade(Pos, Count), At<TSendII>(0x4051f0)(Pos, Count));
	SEND(SendAcceptTrade, SendAcceptTrade(), At<TVoid>(0x4053b0)());
	SEND(SendRejectTrade, SendRejectTrade(), At<TVoid>(0x405550)());
	SEND(SendUseObject, SendUseObject(x, y, z, Type, Pos, Count), At<TSend6>(0x4056f0)(x, y, z, Type, Pos, Count));
	SEND(SendUseObjectWith, SendUseObjectWith(x, y, z, Type, Pos, x2, y2, z2, Type2, Pos2),
		At<TSend10>(0x4058f0)(x, y, z, Type, Pos, x2, y2, z2, Type2, Pos2));
	SEND(SendUseObjectOnCreature, SendUseObjectOnCreature(x, y, z, Type, Pos, a), At<TSend6U>(0x405b20)(x, y, z, Type, Pos, a));
	SEND(SendRotateObject, SendRotateObject(x, y, z, Type, Pos), At<TSend5>(0x405d10)(x, y, z, Type, Pos));
	SEND(SendCloseContainer, SendCloseContainer(Pos), At<TSendI>(0x405ef0)(Pos));
	SEND(SendContainerUp, SendContainerUp(Pos), At<TSendI>(0x4060a0)(Pos));
	SEND(SendEditText, SendEditText(a, S1), At<TSendUS>(0x406250)(a, S1));
	SEND(SendEditHouseText, SendEditHouseText(Pos, a, S1), At<TSendIUS>(0x406410)(Pos, a, S1));
	SEND(SendLook, SendLook(x, y, z, Type, Pos), At<TSend5>(0x4065e0)(x, y, z, Type, Pos));
	static const int Modes1[] = {1, 2, 3, 6, 8, 9, 13}; // the modes each overload's assertion accepts
	static const int Modes2[] = {4, 7, 11, 15};
	static const int Modes3[] = {5, 10, 14};
	int M1 = Valid || Random() % 3 != 0 ? Modes1[Random() % 7] : (int)(Random() % 30) - 2;
	int M2 = Valid || Random() % 3 != 0 ? Modes2[Random() % 4] : (int)(Random() % 30) - 2;
	int M3 = Valid || Random() % 3 != 0 ? Modes3[Random() % 3] : (int)(Random() % 30) - 2;
	int Channel = Random() % 3 == 0 ? (int)Pick32() : Pick16();
	SEND(SendTalk1, SendTalk(M1, S1), At<TSendIS>(0x4067c0)(M1, S1));
	SEND(SendTalk2, SendTalk(M2, S2, S1), At<TSendISS>(0x406b40)(M2, S2, S1));
	SEND(SendTalk3, SendTalk(M3, Channel, S1), At<TSendIIS>(0x406f90)(M3, Channel, S1));
	SEND(SendRequestChannels, SendRequestChannels(), At<TVoid>(0x407300)());
	SEND(SendJoinChannel, SendJoinChannel(Channel), At<TSendI>(0x4074a0)(Channel));
	SEND(SendLeaveChannel, SendLeaveChannel(Channel), At<TSendI>(0x407650)(Channel));
	SEND(SendOpenPrivateConversation, SendOpenPrivateConversation(S1), At<TSendS>(0x407800)(S1));
	SEND(SendProcessRuleViolation, SendProcessRuleViolation(S1), At<TSendS>(0x4079b0)(S1));
	SEND(SendCloseRuleViolation, SendCloseRuleViolation(S1), At<TSendS>(0x407b60)(S1));
	SEND(SendCancelRuleViolation, SendCancelRuleViolation(), At<TVoid>(0x407d10)());
	SEND(SendFightModes, SendFightModes(), At<TVoid>(0x407eb0)());
	SEND(SendAttack, SendAttack(a), At<TSendU>(0x408080)(a));
	SEND(SendFollow, SendFollow(a), At<TSendU>(0x408230)(a));
	SEND(SendOpenPrivateChannel, SendOpenPrivateChannel(), At<TVoid>(0x4083e0)());
	SEND(SendInviteToPrivateChannel, SendInviteToPrivateChannel(S1), At<TSendS>(0x408580)(S1));
	SEND(SendExcludeFromPrivateChannel, SendExcludeFromPrivateChannel(S1), At<TSendS>(0x408730)(S1));
	SEND(SendInviteToParty, SendInviteToParty(a), At<TSendU>(0x4088e0)(a));
	SEND(SendJoinParty, SendJoinParty(a), At<TSendU>(0x408a90)(a));
	SEND(SendRevokePartyInvitation, SendRevokePartyInvitation(a), At<TSendU>(0x408c40)(a));
	SEND(SendPassPartyLeadership, SendPassPartyLeadership(a), At<TSendU>(0x408df0)(a));
	SEND(SendLeaveParty, SendLeaveParty(), At<TVoid>(0x408fa0)());
	SEND(SendCancel, SendCancel(), At<TVoid>(0x409140)());
	SEND(SendRefreshField, SendRefreshField(x, y, z), At<TSendIII>(0x4092e0)(x, y, z));
	SEND(SendRefreshContainer, SendRefreshContainer(Pos), At<TSendI>(0x4094d0)(Pos));
	bool Requested = Random() % 2 == 0;
	OutfitRequested = Requested;
	ORIGINAL(Global<bool>(0x5c4358) = Requested);
	SEND(SendRequestOutfit, SendRequestOutfit(), At<TVoid>(0x409680)());
	int Colors[4] = {Small(), Small(), Small(), Small()};
	const int *C = !Valid && Random() % 4 == 0 ? NULL : Colors;
	SEND(SendOutfit, SendOutfit(Type, C), At<TSendOutfit>(0x409830)(Type, C));
	SEND(SendAddBuddy, SendAddBuddy(S1), At<TSendS>(0x409b00)(S1));
	SEND(SendRemoveBuddy, SendRemoveBuddy(a), At<TSendU>(0x409d90)(a));
	SEND(SendBugReport, SendBugReport(S1), At<TSendS>(0x409f40)(S1));
	uint8 Reason = (uint8)Pick8(), Action = (uint8)Pick8();
	bool IP = Random() % 2 == 0;
	SEND(SendRuleViolationReport, SendRuleViolationReport(S1, Reason, Action, S2, a, IP),
		At<TSendReport>(0x40a0f0)(S1, Reason, Action, S2, a, IP));
	// SendDebugReport (packet 0xe8, the error report) is gone (Patella-Modern issue #74), and its comparison with it.
	// Its arguments' and its SetRand's random numbers are still drawn, as they were, so that the rounds after it
	// keep their inputs.
	if (!Valid)
		Random(); // S3: NULL or T3
	if (!Valid)
		Random(); // S4: NULL or T4
	Random();     // the sender's SetRand
	SenderCount = Index;
#undef SEND
}

// --- The readers -------------------------------------------------------------------

#if GOLDEN_ORACLE
typedef uint32(__cdecl *TReadCreature)(int, int, int, int);
typedef void(__cdecl *TReadField)(int, int, int);
typedef void(__cdecl *TReadMap)(int, int, int, int);
typedef void(__cdecl *TReadFloor)(int);
#endif

static void TestReaders(int Rounds)
{
	char What[64];
	for (int Round = 0; Round < Rounds; Round++) {
		ResetWorlds();
		TPacket P;
		int Kind = Random() % 4;
		int x = Random() % 18, y = Random() % 14, z = Random() % 8;
		int Type = Random() % 5 == 0 ? Pick16() : 0x61 + Random() % 3;
		switch (Kind) {
		case 0: GenCreature(P, Type); break;
		case 1: GenFields(P, 1 + Random() % 3); break;
		case 2: GenFields(P, 18 * 14 * 2); break;
		default: GenFields(P, 18 * 14); break;
		}
		if (Random() % 5 == 0)
			Mutate(P.Data, false, false);
		SetReceiveData(OURS, P.Data.data(), (int)P.Data.size());
		SetReceiveData(IMAGE, P.Data.data(), (int)P.Data.size());
		sprintf(What, "reader %d of round %d", Kind, Round);
		switch (Kind) {
		case 0: {
			uint32 Result[2] = {0, 0};
			Both(What, [&] { Result[0] = ReadCreature(Type, x, y, z); }, ORIGINAL_SIDE(Result[1] = At<TReadCreature>(0x40c830)(Type, x, y, z)));
			golden::Output("ReadCreature", Result[1]);
			CHECK(Result[0] == Result[1], "%s: ReadCreature returns %u, the original %u", What, Result[0], Result[1]);
			break;
		}
		case 1:
			Both(What, [&] { ReadField(x, y, z); }, ORIGINAL_SIDE(At<TReadField>(0x414bb0)(x, y, z)));
			break;
		case 2: {
			int xmin = Random() % 18, ymin = Random() % 14, xmax = xmin + Random() % 3, ymax = ymin + Random() % 3;
			Both(What, [&] { ReadMap(xmin, ymin, xmax, ymax); }, ORIGINAL_SIDE(At<TReadMap>(0x414ff0)(xmin, ymin, xmax, ymax)));
			break;
		}
		default:
			Both(What, [&] { ReadFloor(z); }, ORIGINAL_SIDE(At<TReadFloor>(0x416e60)(z)));
		}
		FreeWorlds();
	}
}

// --- The error round -----------------------------------------------------------------

// TRSA::Encrypt in the error round only: 1 throws a const char * (the image's
// with its ThrowInfo 0x5bb5d0), 2 a structured exception.
static int RsaFailure;
static const char *RsaMessage = "RSA failure of the test";
static void GOLDEN_THISCALL F_Encrypt(GOLDEN_THIS, uint8 *)
{
	Call("TRSA::Encrypt()");
	if (RsaFailure == 2)
		RaiseException(0xe0525341, 0, 0, NULL);
#if GOLDEN_ORACLE
	if (Side == IMAGE)
		At<void(__stdcall *)(void *, void *)>(0x511d2b)(&RsaMessage, (void *)(uintptr_t)0x5bb5d0);
#endif
	throw RsaMessage;
}
static const TFake RsaFakes[] = {
	{"TRSA::Encrypt", 0x508aa0, MemberAddress(&TRSA::Encrypt), (void *)F_Encrypt},
};

// Every fake call of a frame and of a sender fails once (FailAt = 1, 2, ...).
static void ErrorRound()
{
	TErrorRound Round("Communication");
	char What[96];
	for (int t = 0; t < GameTypeCount + LoginTypeCount; t++) {
		bool Login = t >= GameTypeCount;
		int Type = Login ? LoginTypes[t - GameTypeCount] : GameTypes[t];
		for (int Variant = 0; Variant < 3; Variant++) {
			uint32 Seed = Random();
			if (Login && IsPatchMessage(Type)) {
				// Not compared since issue #75 (ours ends the login). Every pass of the loop below starts from Seed
				// and makes the same frame, so one making of it leaves the random numbers as the loop did.
				SetRandomSeed(Seed);
				ResetWorlds();
				SetRand(Random());
				TPacket P;
				GenLogin(P, Type);
				FreeWorlds();
				continue;
			}
			int Total = 0;
			for (int Fail = 0;; Fail++) {
				SetRandomSeed(Seed);
				ResetWorlds();
				SetRand(Random());
				ModelZ = World[OURS].MapZ;
				TPacket P;
				if (Login)
					GenLogin(P, Type);
				else
					GenGame(P, Type);
				Frames.clear();
				Frames.push_back(P.Data);
				FailAt = Fail;
				sprintf(What, "frame type 0x%02x variant %d failing at call %d", Type, Variant, Fail);
				RunFrames(What, Login ? 4 : 8);
				FailAt = 0;
				if (Fail == 0)
					Total = Ticks[OURS];
				// The original's CancelPatch after each (PatchData of a patch that failed half-way) is gone with ours
				// (issue #75).
				FreeWorlds();
				if (Fail >= Total || Fail >= 300)
					break;
			}
		}
	}
	// Every message cut after each of its first 40 bytes: each read of each
	// handler fails once (the TEND blocks with what the handler has read so far).
	for (int t = 0; t < GameTypeCount + LoginTypeCount; t++) {
		bool Login = t >= GameTypeCount;
		int Type = Login ? LoginTypes[t - GameTypeCount] : GameTypes[t];
		uint32 Seed = Random();
		for (int Cut = 1; Cut <= 40; Cut++) {
			SetRandomSeed(Seed);
			ResetWorlds();
			SetRand(Random());
			ModelZ = World[OURS].MapZ;
			TPacket P;
			if (Login)
				GenLogin(P, Type);
			else
				GenGame(P, Type);
			if (Cut >= (int)P.Data.size()) {
				FreeWorlds();
				break;
			}
			P.Data.resize(Cut);
			Frames.clear();
			Frames.push_back(P.Data);
			sprintf(What, "frame type 0x%02x cut after %d bytes", Type, Cut);
			// A cut patch packet is not compared, and the original's CancelPatch after each is gone (issue #75).
			if (!Login || !IsPatchMessage(Type))
				RunFrames(What, Login ? 4 : 8);
			FreeWorlds();
		}
	}
	// The senders, with the arguments their assertions refuse, and failing calls.
	for (int i = 0; i < 300; i++) {
		ResetWorlds();
		sprintf(What, "(invalid arguments %d)", i);
		CallSenders(What, false);
		FreeWorlds();
	}
	for (int Fail = 1; Fail <= 12; Fail++) {
		ResetWorlds();
		FailAt = Fail;
		sprintf(What, "(failing at call %d)", Fail);
		CallSenders(What, true);
		FailAt = 0;
		FreeWorlds();
	}
	// Last (the fakes stay until Finish): TRSA::Encrypt throws a const char *
	// (the login packets' first catch) or a structured exception (catch (...)).
	InstallFakes(RsaFakes);
	for (int Kind = 1; Kind <= 2; Kind++) {
		for (int i = 0; i < 20; i++) {
			ResetWorlds();
			RsaFailure = Kind;
			uint32 Account = Pick32();
			std::string Name = PickText(), Password = PickText();
			SetRand(Random());
			sprintf(What, "SendLogin with a failing RSA (%d, %d)", Kind, i);
			Both(What, [&] { SendLogin(Account, Password.c_str()); }, ORIGINAL_SIDE(At<void(__cdecl *)(uint32, const char *)>(0x413780)(Account, Password.c_str())));
			SetRand(Random());
			sprintf(What, "SendGameLogin with a failing RSA (%d, %d)", Kind, i);
			Both(What, [&] { SendGameLogin(Account, Name.c_str(), Password.c_str()); }, ORIGINAL_SIDE(At<void(__cdecl *)(uint32, const char *, const char *)>(0x413de0)(Account, Name.c_str(), Password.c_str())));
			RsaFailure = 0;
			FreeWorlds();
		}
	}
	Round.Finish();
}

// The patch packets end the login (Patella-Modern issue #75, decision D9): ours alone (the original patches, so there
// is nothing of it to compare with). Login frames as TestPatches makes them (a difference patch or a whole file, its
// blocks, a block of a zero-sized patch, a second start), after login messages of the same frame or not, with login
// messages after the patch packet in its frame, or a block alone; in connection state 3 or 4. Ours must read
// everything the server sent (every frame, the receive buffer at its end), post one event, the login error dialog
// ("Sorry") with the text, close the connection and set the connection state 0, and do nothing else: no other event,
// no packet sent, no Communication.cpp function the original's patcher called (SetProgress, ApplyPatch,
// ApplyFullPatch, InstallPatch, CreateUpdateDialog: neither the fakes nor ours exist for them). Its own random numbers
// (the seed saved and set again), so that the later tests' inputs stay the same.
static void TestUpdateRequests(int Rounds)
{
	const uint32 Saved = GetRandomSeed();
	SetRandomSeed(75);
	static const char *const Refused = "This server wants to update the client";
	int ZeroSized = 0;
	for (int Round = 0; Round < Rounds; Round++) {
		ResetWorlds();
		SetRand(Random());
		Frames.clear();
		const int Before = Random() % 3; // login messages before the patch packet in its frame
		if (Round % 7 == 3) {
			// A block alone (no transfer started).
			TPacket P;
			GenLogin(P, 0x20);
			Frames.push_back(P.Data);
		} else if (Round % 5 == 0) {
			// Issue #67: a zero-sized patch and its block (the original's ReceivePatchBlock divided by the size).
			TPacket P;
			P.B(Round % 2 == 0 ? 0x1e : 0x1f);
			P.B(Random() % 5);
			P.Q(0);
			Frames.push_back(P.Data);
			Frames.push_back(std::vector<uint8>{0x20, 0x00, 0x00});
			ZeroSized++;
		} else {
			MakePatchFrames();
		}
		if (Before > 0) {
			TPacket First;
			for (int i = 0; i < Before; i++)
				GenLogin(First, Random() % 2 == 0 ? 0x0b : 0x14);
			Frames[0].insert(Frames[0].begin(), First.Data.begin(), First.Data.end());
		}
		if (Random() % 3 == 0) {
			TPacket After;
			GenLogin(After, Random() % 2 == 0 ? 0x0b : 0x64);
			Frames[0].insert(Frames[0].end(), After.Data.begin(), After.Data.end());
		}
		const int State = 3 + Random() % 2;
		char What[64];
		sprintf(What, "update request %d", Round);
		World[OURS].ConnectionState = State;
		World[OURS].Events.clear();
		NextFrame[OURS] = 0;
		SetReceiveData(OURS, NULL, 0);
		const int SavedSide = Side;
		Side = OURS;
		Log[OURS].clear();
		bool Threw = false;
		try {
			ReceivePackets();
		} catch (...) {
			Threw = true;
		}
		Side = SavedSide;
		const std::string &L = Log[OURS];
		CHECK(!Threw, "%s: ReceivePackets threw", What);
		CHECK(NextFrame[OURS] == (int)Frames.size() && ReceiveBufferOf(OURS)->Position == ReceiveBufferOf(OURS)->Size,
			"%s: frame %d of %d read, at %d of %d", What, NextFrame[OURS], (int)Frames.size(),
			ReceiveBufferOf(OURS)->Position, ReceiveBufferOf(OURS)->Size);
		const std::vector<TQueuedEvent> &E = World[OURS].Events;
		const int Messages = Before; // ReceiveLoginMessage and ReceiveMessageOfTheDay post one event each, or none
		const bool Posted = !E.empty() && E.back().Type == 1 && E.back().Param1 != 0 && E.back().Param2 != 0
			&& strcmp((const char *)E.back().Param1, "Sorry") == 0 && strcmp((const char *)E.back().Param2, Refused) == 0;
		CHECK(Posted && (int)E.size() <= Messages + 1, "%s: %d events, the last not the login error with the text", What,
			(int)E.size());
		const size_t Message = L.rfind("AddMessage1("), Close = L.find("Disconnect(0)", Message == std::string::npos ? 0 : Message);
		CHECK(Message != std::string::npos && Close != std::string::npos && L.find("SetConnectionState(0)", Close) != std::string::npos,
			"%s: no message, Disconnect and SetConnectionState(0) in that order: %s", What, L.substr(0, 400).c_str());
		CHECK(World[OURS].ConnectionState == 0, "%s: connection state %d", What, World[OURS].ConnectionState);
		CHECK(L.find("SendCommand") == std::string::npos && L.find("AddMessageFirst") == std::string::npos
				&& L.find("SetProgress") == std::string::npos,
			"%s: more than the login's end: %s", What, L.substr(0, 400).c_str());
		FreeWorlds();
	}
	printf("Communication: %d update requests ended the login (%d of a zero-sized patch)\n", Rounds, ZeroSized);
	SetRandomSeed(Saved);
}

void TestCommunication(const char *ExePath)
{
	int Before = Checks, BeforeFailures = Failures;
	TestName = "Communication";
	TraceCalls = getenv("CM_TRACE") != NULL;
	TraceBefore = TraceCalls && atoi(getenv("CM_TRACE")) >= 2;
	LogFile = getenv("CM_LOGS") != NULL ? fopen(getenv("CM_LOGS"), "w") : NULL;
	int Rounds = getenv("CM_ROUNDS") != NULL ? atoi(getenv("CM_ROUNDS")) : 6000;
	if (getenv("CM_SEED") != NULL)
		SetRandomSeed(strtoul(getenv("CM_SEED"), NULL, 10));
	RecordComparisons = true; // a ported test: its comparisons go into the corpus
	LoadObjectFlags(ExePath);
	// ExitCommunication is empty: in the image one copy of five empty Exit
	// functions (0x496160), which the fakes replace on both sides (ExitControl's
	// calls are compared by controltest.cpp). Compared here by its code, a
	// single ret, before the fakes are installed.
	// The runner: on x64 too a single ret (C3; C2 00 00 is ret 0), as nothing of ours is redirected yet.
#if !GOLDEN_ORACLE
	golden::RestoreAllOwn();
#endif
	const uint8 *Ours = (const uint8 *)(void *)&ExitCommunication; // ret or ret 0
	const int Original = ORIGINAL_VALUE("0x496160", int, *(const uint8 *)(uintptr_t)0x496160);
	// Linux with the sanitizers' frame pointers (linux-x64-clang, -fno-omit-frame-pointer; issue #91): the empty
	// function's frame, push rbp; mov rbp, rsp; pop rbp; ret.
	static const uint8 FramedRet[] = {0x55, 0x48, 0x89, 0xe5, 0x5d, 0xc3};
	CHECK((Ours[0] == 0xc3 || (Ours[0] == 0xc2 && Ours[1] == 0 && Ours[2] == 0)
			  || memcmp(Ours, FramedRet, sizeof(FramedRet)) == 0)
			&& Original == 0xc3,
		"ExitCommunication or 0x496160 is not a single ret: %02x, %02x", *(const uint8 *)(void *)&ExitCommunication,
		Original);
	BeginWorld();
	InstallFakes(ControlFakes);
	ResetWorlds();
	Both("InitCommunication", [&] { InitCommunication(); }, ORIGINAL_SIDE(At<TVoid>(0x401000)()));
	FreeWorlds();
	TestSeeds();
	TestGenerated(Rounds);
	TestPatches(Rounds / 20);
	TestCancelWalks(Rounds / 100 + 5);
	TestReaders(Rounds / 4);
	for (int i = 0; i < Rounds / 20; i++) {
		ResetWorlds();
		char What[32];
		sprintf(What, "(round %d)", i);
		CallSenders(What, true);
		FreeWorlds();
	}
	// Before the error round: its Finish undoes the fakes.
	TestUpdateRequests(300);
	int Least = 1 << 30;
	for (int Login = 0; Login < 2; Login++) {
		int Count = Login ? LoginTypeCount : GameTypeCount;
		for (int i = 0; i < Count; i++) {
			int Type = Login ? LoginTypes[i] : GameTypes[i];
			if (Login && IsPatchMessage(Type))
				continue; // not compared since issue #75
			if (Covered[Login][Type] < Least)
				Least = Covered[Login][Type];
		}
	}
	printf("Communication: %d checks, %d failures (each packet type at least %d times, %d senders)\n", Checks - Before,
		Failures - BeforeFailures, Least, SenderCount);
	ErrorRound();
	EndWorld();
	RecordComparisons = false;
	if (LogFile != NULL)
		fclose(LogFile);
	LogFile = NULL;
}
