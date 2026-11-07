// fuzz_packets: the server's packets (Patella-Modern issue #132): Control.cpp's ReceivePackets, the dispatcher, and
// every Receive* of Communication.cpp (TCommunication) behind it, in the golden Communication test's world
// (tests/golden/commworld.h): their code runs for real, every function they call in the other files is that world's
// fake (the map, the creatures, the windows, the events), and Network.cpp's readers read the test's receive buffer,
// one frame at a time, an allocation of the frame's own size (so a read past it is an AddressSanitizer report).
// The world has no Tibia.dat: the fake object flags are hashes of the type (commworld.cpp ObjectFlag), synthetic.
//
// The input: [connection state][u16 size][frame] ...: the first byte picks the state ReceivePackets starts in, 3 or 4
// (the login server's table) or 6, 7 or 8 (the game server's); then the frames, each one or more commands (the
// packets' payloads, without their inner length: as PrepareNextPacket leaves them). An unknown command, a read past a
// frame's end, an assertion: a TMyException, which ends the client (HandleException), and the input here.

// getenv: the CRT's own (MSVC deprecates it), before the C runtime's headers.
#define _CRT_SECURE_NO_WARNINGS
#include "client.h"

#include "commworld.h"

#include "Control.h"
#include "GUI.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <vector>

namespace {

const char *const Target = "fuzz_packets";
bool Trace = false; // PATELLA_FUZZ_TRACE set: each input's calls (the world's log) on stdout

// Control.cpp's functions ReceivePackets calls (communication_tests.cpp's ControlFakes): they only log.
void FakeEnterGame()
{
	Call("EnterGame()");
}
void FakeHandleEvents()
{
	Call("HandleEvents()");
}
const TFake ControlFakes[] = {
	{"EnterGame", 0x4465f0, MemberAddress(&patella::TGUI::EnterGame), reinterpret_cast<void *>(FakeEnterGame)},
	{"HandleEvents", 0x44a770, MemberAddress(&patella::TGUI::HandleEvents), reinterpret_cast<void *>(FakeHandleEvents)},
};

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	Trace = std::getenv("PATELLA_FUZZ_TRACE") != nullptr;
	BeginWorld();
	InstallFakes(ControlFakes);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	static const int States[] = {3, 4, 6, 7, 8};
	patella::fuzz::TInput Input(Data, Size);
	const int State = States[Input.Byte() % std::size(States)];
	Frames.clear();
	while (!Input.Empty()) {
		const std::string Frame = Input.Bytes(Input.Word());
		Frames.emplace_back(Frame.begin(), Frame.end());
	}
	// The same world for every input.
	SetRandomSeed(132);
	ResetWorlds();
	Side = OURS;
	World[OURS].ConnectionState = State;
	NextFrame[OURS] = 0;
	SetReceiveData(OURS, nullptr, 0);
	patella::fuzz::Guard(Target, [] { ReceivePackets(); });
	if (Trace)
		std::printf("%s: state %d, %zu frames, %d read: %s\n", Target, State, Frames.size(), NextFrame[OURS],
			Log[OURS].substr(0, 2000).c_str());
	FreeWorlds();
	Log[OURS].clear();
	Log[IMAGE].clear();
	return 0;
}
