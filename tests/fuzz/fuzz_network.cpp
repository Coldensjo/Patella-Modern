// fuzz_network: TNetwork's frames (Network.cpp; Patella-Modern issue #132): OnReceive, PrepareNextPacket,
// GetNextCommand and the readers, on a TNetwork of its own with a fake session and sockets, as bounded_reads drives it
// (tests/golden/bounded_reads_test.cpp, #72).
//
// The input is the server's stream with the frames' contents in the clear: [u16 frame size][the frame's bytes] ...,
// as on the wire; the harness encrypts every whole 8-byte block of a frame with the session key (XTEA, as the server
// does) and leaves a frame's last bytes that are no whole block as they are, so a frame whose size is no multiple of 8
// still reaches PrepareNextPacket's check. Inner lengths, sizes 0, frames past ReceiveData: all the fuzzer's.
// The first byte of the input picks how many bytes each Receive returns (1 to 0x4000), so frames arrive split.
//
// The receive handler reads each packet as a caller would: after GetNextCommand, the command's low bits pick the
// readers (bytes, words, quads, the strings, a block of bytes) until the packet ends. A read past a packet's end throws
// a TMyException: the client ends there (HandleException); the harness catches it and reads on, as bounded_reads
// shows the buffer stays consistent, so later frames are fuzzed too (after 32 throws the input's rest is not read).
// Since issue #139 the frames the client cannot use (#72's inner length past the frame or a frame of size 0, a size
// that is no multiple of 8, a frame larger than ReceiveData) are dropped and logged, and the connection goes on: the
// harness's property is that every byte of the stream is taken (none waits forever, #139 item 6), as long as the
// handler read every packet. Since issue #142 an empty packet (inner length 0) no longer ends the reading with its
// frame left in ReceiveData, so the property holds for every input (until #142 the harness excluded those), and
// GetNextCommand's -1 leaves no frame in ReceiveData.

#include "client.h"

#include "Network.h"
#include "Preferences.h"
#include "TXTEA.h"
#include "log/Log.h"
#include "platform/Socket.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>

namespace {

const char *const Target = "fuzz_network";

class TFakeSession final : public patella::TNetworkSession {
public:
	void SetConnectionState(int Value) override { State = Value; }
	int GetConnectionState() override { return State; }
	int GetActiveCharacter() override { return 0; }
	const char *GetCharacterIPAddress(int) override { return "127.0.0.1"; }
	int GetCharacterPort(int) override { return 7172; }
	void AddMessage(int, std::intptr_t, std::intptr_t) override {}
	int State = 8;
};

// No connection; Receive gives Incoming's bytes, at most Chunk at a time.
class TFakeSockets final : public patella::platform::ISocket {
public:
	bool Startup() override { return true; }
	int LastError() override { return 0; }
	patella::platform::TSocket *New(patella::platform::ISocketEvents &) override { return nullptr; }
	void Delete(patella::platform::TSocket *) override {}
	bool Create(patella::platform::TSocket *) override { return false; }
	bool Connect(patella::platform::TSocket *, const char *, int) override { return false; }
	int Send(patella::platform::TSocket *, const void *, int Length) override { return Length; }
	int Receive(patella::platform::TSocket *, void *Buffer, int Length) override
	{
		std::size_t Count = Incoming.size() - Taken;
		if (Count > Chunk)
			Count = Chunk;
		if (Count > static_cast<std::size_t>(Length))
			Count = static_cast<std::size_t>(Length);
		if (Count != 0)
			std::memcpy(Buffer, Incoming.data() + Taken, Count);
		Taken += Count;
		return static_cast<int>(Count);
	}
	void Shutdown(patella::platform::TSocket *, patella::platform::TShutdown) override {}
	void Close(patella::platform::TSocket *) override {}
	void *ResolveHostname(void *, unsigned int, const char *, char *, int) override { return nullptr; }
	void CancelResolve(void *) override {}
	const char *ResolvedAddress(const char *) override { return ""; }
	std::string Incoming;
	std::size_t Taken = 0;
	std::size_t Chunk = 0x4000;
};

const uint8 Key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

patella::TNetwork *Current = nullptr;
std::size_t Reads = 0; // the reads left for this input: a bound on the handler's work
int Throws = 0;        // the throws left for this input (each costs as much as many reads)

// One packet's commands, read as their low bits say.
void ReadCommand(patella::TNetwork &N, int Command)
{
	switch (Command % 7) {
	case 0: static_cast<void>(N.GetByte()); break;
	case 1: static_cast<void>(N.GetWord()); break;
	case 2: static_cast<void>(N.GetQuad()); break;
	case 3: {
		// A text that does not fit throws before anything is allocated (issue #139, item 4; the original leaked it).
		char *Text = N.GetString();
		delete[] Text;
		break;
	}
	case 4: {
		char Text[30];
		N.GetString(patella::TSpan<char>(Text, sizeof(Text)));
		break;
	}
	case 5: {
		uint8 Bytes[5];
		N.GetBytes(patella::TSpan<uint8>(Bytes, sizeof(Bytes)));
		break;
	}
	default: // the rest of the packet, byte by byte
		while (N.ReceiveBuffer.Position < N.ReceiveBuffer.Size && Reads != 0) {
			Reads--;
			static_cast<void>(N.GetByte());
		}
		break;
	}
}

void ReceiveHandler()
{
	patella::TNetwork &N = *Current;
	while (Reads != 0 && Throws > 0) {
		Reads--;
		bool Ended = false;
		bool Threw = true;
		patella::fuzz::Guard(Target, [&] {
			const int Command = N.GetNextCommand();
			Threw = false;
			if (Command == -1) {
				Ended = true;
				return;
			}
			Threw = true;
			ReadCommand(N, Command);
			Threw = false;
		});
		if (Ended) {
			// Every complete frame read and the last one moved out of ReceiveData (issue #142: an empty packet's
			// too, which Tibia.exe left there with the frames behind it waiting).
			if (N.ReceiveRead != 0 || N.ReceivePacketEnd != 0)
				patella::fuzz::Fail(Target, "GetNextCommand returned -1 with a frame left in ReceiveData");
			return;
		}
		Throws -= Threw ? 1 : 0;
	}
}

void OtherHandler() {}

// The wire's bytes of the input (above).
std::string Stream(patella::fuzz::TInput &Input)
{
	TXTEA Cipher;
	Cipher.SetKey(TXTEA::TKey(Key, 16));
	std::string Result;
	while (!Input.Empty()) {
		const std::uint16_t Size = Input.Word();
		std::string Frame = Input.Bytes(Size);
		for (std::size_t i = 0; i + 8 <= Frame.size(); i += 8)
			Cipher.EncryptBlock(TXTEA::TBlock(reinterpret_cast<uint8 *>(&Frame[i]), 8));
		Result += static_cast<char>(Size & 0xff);
		Result += static_cast<char>(Size >> 8);
		Result += Frame;
	}
	return Result;
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	// The dropped frames' warn lines (issue #139) would be most of the run's output.
	patella::log::Log().SetLevel(patella::log::TLevel::Error);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	static const std::size_t Chunks[] = {0x4000, 1, 2, 7, 64, 1000};
	patella::fuzz::TInput Input(Data, Size);
	TFakeSession Session;
	TFakeSockets Sockets;
	Sockets.Chunk = Chunks[Input.Byte() % std::size(Chunks)];
	Sockets.Incoming = Stream(Input);
	// The receive buffer is 0x4000 bytes inside TNetwork: an object of its own, as the client's (TClient's member).
	auto Network = std::make_unique<patella::TNetwork>(patella::bridge::Preferences(), Session, Sockets);
	Current = Network.get();
	Reads = 4 * Size + 64;
	Throws = 32;
	Network->InitNetwork(OtherHandler, ReceiveHandler, OtherHandler);
	Network->SetSymmetricKey(Key);
	auto &Socket = *reinterpret_cast<patella::platform::TSocket *>(&Sockets);
	for (;;) {
		const std::size_t Before = Sockets.Taken;
		patella::fuzz::Guard(Target, [&] { Network->OnReceive(Socket, 0); });
		// Everything taken, or nothing more taken (ReceiveData full: only when the handler stopped early, below).
		if (Sockets.Taken == Before)
			break;
	}
	// Issue #139, item 6: a frame larger than ReceiveData no longer stalls the stream, nor (issue #142) an empty
	// packet. While the handler read every packet it was given, there is always room: the frames are taken to the end.
	if (Reads != 0 && Throws > 0 && Sockets.Taken != Sockets.Incoming.size())
		patella::fuzz::Fail(Target, "the stream stalled: " + std::to_string(Sockets.Incoming.size() - Sockets.Taken) + " bytes never taken");
	Network->ExitNetwork();
	Current = nullptr;
	return 0;
}
