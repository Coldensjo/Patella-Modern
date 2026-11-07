// bounded_reads (Patella-Modern issues #72, #86, #139 and #142): the reads and writes of Tibia.exe that ran past their data,
// and its other failures on hostile input, bounded as deliberate differences (notes/decisions.md, "#72 and #86" and
// "#139"):
//
// - #72: TNetwork::PrepareNextPacket with a frame whose inner length runs past it (or a frame of size 0, which has
//   no inner length): the frame is dropped (a warn line in the developer log; until #139 a TMyException), so no read
//   of ReceiveBuffer leaves the frame, and the next frame is read. Tibia.exe set the read buffer's size from the
//   length as it was and read on, past ReceiveData into SendData.
// - #86: an ignore entry's name of 30 characters and a buddy's description of 128 have no NUL of their own (the next
//   byte is Permanent, Notify); TIgnoreEntry::NameText, TBuddy::DescriptionText and TPreferences::GetBuddyDescription
//   read at most 30 and 128 characters. Tibia.exe's readers read on into the next byte and past it.
// - #139: a failed Tibia.dat or Tibia.pic load frees only what it allocated (items 1, 2), a sprite's or tile's runs
//   stay inside the 32 x 32 buffer (3), GetString throws before it allocates (4), a .map file's negative mark count is
//   a corrupted file (5), a frame larger than ReceiveData (6) or of a size that is no multiple of 8 (7) is dropped as
//   #72's frames are.
// - #142: an empty packet (inner length 0) is read past to the next frame, and its frame leaves ReceiveData at once;
//   Tibia.exe stopped at it with the frames behind it waiting, and a frame that fit only without it never completed.
//
// Under AddressSanitizer (windows-x64-asan, linux-x64-clang) the bytes past the data are poisoned (the frame's end to
// the end of ReceiveData and SendData, which lie inside TNetwork, have no redzone of their own), or the entry is an
// allocation of its own size, so a read past them is an ASan report: with the original's reads put back, every case
// below is one (notes/decisions.md has the proof).
//
// No corpus and no image: the executable links the golden runner's objects (the client's code and the stubs it needs)
// with this file in place of runner.cpp, as preferences_file does (preferences_file_test.cpp).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
// common.h's, before doctest's C runtime headers (Dialogs.h's _snprintf); common.h comes after doctest, whose
// std::min and std::max the Windows headers' macros would break.
#define _CRT_SECURE_NO_WARNINGS
#include <doctest/doctest.h>

#include "common.h"
#include "Dialogs.h"
#include "Map.h"
#include "Network.h"
#include "Objects.h"
#include "Preferences.h"
#include "TMyException.h"
#include "TXTEA.h"
#include "Utils.h"
#include "graphics.h"
#include "log/Log.h"
#include "platform/Socket.h"
#include "settings/Settings.h"
#include "platform/Platform.h"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(__SANITIZE_ADDRESS__)
#define BOUNDED_READS_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define BOUNDED_READS_ASAN 1
#endif
#endif
#if defined(BOUNDED_READS_ASAN)
#include <sanitizer/asan_interface.h>
#endif

int Failures = 0;
int Checks = 0;
extern long long GoldenArrayNews; // container_tests.cpp's operator new[] (#139, item 4)
int FailuresPrinted = 20;
namespace {
uint32 Seed = 12345;
} // namespace
uint32 Random()
{
	Seed = Seed * 1103515245 + 12345;
	return Seed >> 8;
}
uint32 GetRandomSeed() { return Seed; }
void SetRandomSeed(uint32 Value) { Seed = Value; }
void RandomText(char *Text, int Length)
{
	for (int i = 0; i < Length; i++)
		Text[i] = 'a';
	Text[Length] = 0;
}

namespace {

// Poisons [Begin, Begin + Size) under ASan for the object's lifetime; nothing without ASan.
class TPoisoned {
public:
	TPoisoned(const void *Begin, std::size_t Size) : Start(Begin), Length(Size)
	{
#if defined(BOUNDED_READS_ASAN)
		ASAN_POISON_MEMORY_REGION(Start, Length);
#endif
	}
	~TPoisoned()
	{
#if defined(BOUNDED_READS_ASAN)
		ASAN_UNPOISON_MEMORY_REGION(Start, Length);
#endif
	}
	TPoisoned(const TPoisoned &) = delete;
	TPoisoned &operator=(const TPoisoned &) = delete;

private:
	const void *Start;
	std::size_t Length;
};

// --- #72 ---------------------------------------------------------------------------------------

class TFakeSession final : public patella::TNetworkSession {
public:
	void SetConnectionState(int Value) override { State = Value; }
	int GetConnectionState() override { return State; }
	int GetActiveCharacter() override { return 0; }
	const char *GetCharacterIPAddress(int) override { return "127.0.0.1"; }
	int GetCharacterPort(int) override { return 7172; }
	void AddMessage(int, INT_PTR, INT_PTR) override {}
	int State = 0;
};

// The network of one test: no connection; Receive gives Incoming's bytes.
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
		const int Count = static_cast<int>(Incoming.size()) < Length ? static_cast<int>(Incoming.size()) : Length;
		std::memcpy(Buffer, Incoming.data(), static_cast<std::size_t>(Count));
		Incoming.erase(0, static_cast<std::size_t>(Count));
		return Count;
	}
	void Shutdown(patella::platform::TSocket *, patella::platform::TShutdown) override {}
	void Close(patella::platform::TSocket *) override {}
	void *ResolveHostname(void *, unsigned int, const char *, char *, int) override { return nullptr; }
	void CancelResolve(void *) override {}
	const char *ResolvedAddress(const char *) override { return ""; }
	std::string Incoming;
};

const uint8 Key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

// A server frame: [u16 size][XTEA([u16 Inner][Payload][padding to 8])]; Size 0: a frame of size 0.
std::string Frame(const std::string &Payload, int Inner, bool Empty = false)
{
	if (Empty)
		return std::string(2, '\0');
	std::string Plain(2, '\0');
	Plain[0] = static_cast<char>(Inner & 0xff);
	Plain[1] = static_cast<char>((Inner >> 8) & 0xff);
	Plain += Payload;
	while (Plain.size() % 8 != 0)
		Plain += '\x5a';
	TXTEA Cipher;
	Cipher.SetKey(TXTEA::TKey(Key, 16));
	for (std::size_t i = 0; i < Plain.size(); i += 8)
		Cipher.EncryptBlock(TXTEA::TBlock(reinterpret_cast<uint8 *>(&Plain[i]), 8));
	std::string Result(2, '\0');
	Result[0] = static_cast<char>(Plain.size() & 0xff);
	Result[1] = static_cast<char>(Plain.size() >> 8);
	return Result + Plain;
}

int Received = 0;
void ReceiveHandler() { Received++; }
void OtherHandler() {}

// A TNetwork with its key, the frames in ReceiveData as OnReceive leaves them, and the bytes after the first frame
// (ReceiveData's rest and SendData) poisoned under ASan while the test reads the first frame.
struct TNetworkTest {
	TNetworkTest()
	{
		Network->InitNetwork(OtherHandler, ReceiveHandler, OtherHandler);
		Network->SetSymmetricKey(Key);
	}
	void Put(const std::string &Data)
	{
		Sockets.Incoming = Data;
		Received = 0;
		Network->OnReceive(*reinterpret_cast<patella::platform::TSocket *>(&Sockets), 0);
	}
	// What the first frame may be read of: its size and its bytes; the rest of ReceiveData and SendData poisoned.
	std::unique_ptr<TPoisoned> PoisonAfter(int FrameEnd)
	{
		auto &N = *Network;
		const auto *Begin = N.ReceiveData.data() + FrameEnd;
		const auto *End = N.SendData.data() + N.SendData.size();
		return std::make_unique<TPoisoned>(Begin, static_cast<std::size_t>(End - Begin));
	}
	TFakeSession Session;
	TFakeSockets Sockets;
	std::unique_ptr<patella::TNetwork> Network =
		std::make_unique<patella::TNetwork>(patella::bridge::Preferences(), Session, Sockets);
};

// Reads the packet to its end: GetByte until Position reaches Size, then one more, which throws.
int ReadToEnd(patella::TNetwork &N)
{
	int Count = 0;
	while (N.ReceiveBuffer.Position < N.ReceiveBuffer.Size) {
		(void)N.GetByte();
		Count++;
	}
	CHECK_THROWS_AS((void)N.GetByte(), TMyException);
	return Count;
}

} // namespace

TEST_CASE("#72: a frame whose inner length fits is read as before, its padding included")
{
	TNetworkTest T;
	// 5 bytes of payload in a frame of 8: inner length 6 reads the one byte of padding too, as Tibia.exe did.
	const std::string First = Frame("\x01\x02\x03\x04\x05", 6);
	REQUIRE(First.size() == 10);
	T.Put(First + Frame("\x09", 1));
	CHECK(Received == 1);
	{
		const auto Poison = T.PoisonAfter(static_cast<int>(First.size()));
		CHECK(T.Network->GetNextCommand() == 1);
		CHECK(T.Network->ReceiveBuffer.Size == 10);
		CHECK(T.Network->ReceiveRead == 10);
		CHECK(ReadToEnd(*T.Network) == 5);
	}
	CHECK(T.Network->GetNextCommand() == 9);
	CHECK(T.Network->GetNextCommand() == -1);
}

namespace {

// The developer log's lines while a test runs (the warn lines of the dropped frames, #139).
std::string LogLines;
void CaptureLog(std::string_view Line) noexcept
{
	try {
		LogLines += Line;
	} catch (...) { // NOLINT(bugprone-empty-catch): std::bad_alloc; the line is dropped
	}
}
class TLogCapture {
public:
	TLogCapture()
	{
		LogLines.clear();
		patella::log::Log().SetConsole(CaptureLog);
	}
	~TLogCapture() { patella::log::Log().SetConsole(patella::log::WriteToStderr); }
	TLogCapture(const TLogCapture &) = delete;
	TLogCapture &operator=(const TLogCapture &) = delete;
};

} // namespace

TEST_CASE("#72, #139: a frame whose inner length runs past it is dropped and logged, nothing past it read")
{
	for (const int Inner : {7, 8, 100, 0x3ff0, 0xffff}) {
		CAPTURE(Inner);
		TNetworkTest T;
		const TLogCapture Log;
		const std::string First = Frame("\x01\x02\x03\x04\x05", Inner);
		T.Put(First);
		{
			// Tibia.exe read on past the frame here (into the next frame, and past ReceiveData into SendData).
			const auto Poison = T.PoisonAfter(static_cast<int>(First.size()));
			CHECK(T.Network->GetNextCommand() == -1);
			// Dropped, and already moved out of ReceiveData by the search for the next frame.
			CHECK(T.Network->ReceiveDataSize == 0);
			CHECK(T.Network->ReceivePacketEnd == 0);
			CHECK(T.Network->ReceiveBuffer.Position == T.Network->ReceiveBuffer.Size);
			CHECK(T.Network->ReceiveBuffer.Size <= 10);
			CHECK_THROWS_AS((void)T.Network->GetByte(), TMyException);
		}
		CHECK(T.Network->DroppedFrames == 1);
		CHECK(T.Network->LastDrop == patella::TFrameDrop::Length);
		CHECK(LogLines == "[warn] server frame dropped: its length " + std::to_string(Inner) + " runs past its size 8\n");
		// The connection goes on: the next frame is read as usual.
		T.Put(Frame("\x09", 1));
		CHECK(T.Network->GetNextCommand() == 9);
		CHECK(T.Network->GetNextCommand() == -1);
	}
}

TEST_CASE("#72, #139: the frame after a dropped one is read in the same call")
{
	TNetworkTest T;
	const TLogCapture Log;
	T.Put(Frame("\x01\x02\x03\x04\x05", 100) + Frame("\x09", 1) + Frame("", 0, true) + Frame("\x07", 1));
	CHECK(Received == 1);
	CHECK(T.Network->GetNextCommand() == 9);
	CHECK(T.Network->DroppedFrames == 1);
	CHECK(T.Network->GetNextCommand() == 7);
	CHECK(T.Network->DroppedFrames == 2);
	CHECK(T.Network->GetNextCommand() == -1);
	CHECK(LogLines == "[warn] server frame dropped: its length 100 runs past its size 8\n"
					  "[warn] server frame dropped: it has no length (size 0)\n");
}

TEST_CASE("#72, #139: a frame of size 0 has no inner length: it is dropped and logged")
{
	TNetworkTest T;
	const TLogCapture Log;
	T.Put(Frame("", 0, true));
	CHECK(Received == 1);
	{
		// Tibia.exe read the inner length from the next two bytes and on from there.
		const auto Poison = T.PoisonAfter(2);
		CHECK(T.Network->GetNextCommand() == -1);
		CHECK(T.Network->ReceiveDataSize == 0);
		CHECK(T.Network->ReceivePacketEnd == 0);
		CHECK(T.Network->ReceiveBuffer.Position == T.Network->ReceiveBuffer.Size);
	}
	CHECK(T.Network->LastDrop == patella::TFrameDrop::Length);
	CHECK(LogLines == "[warn] server frame dropped: it has no length (size 0)\n");
	T.Put(Frame("\x07", 1));
	CHECK(T.Network->GetNextCommand() == 7);
	CHECK(T.Network->GetNextCommand() == -1);
}

TEST_CASE("#72, #139: a frame that fills ReceiveData with a length past it reads nothing past ReceiveData")
{
	TNetworkTest T;
	// The largest frame ReceiveData holds: 0x4000 - 2 bytes after the size, a multiple of 8 below that.
	const std::string Payload(0x3ff0 - 2 - 8, '\x03');
	const std::string Data = Frame(Payload, 0xffff);
	REQUIRE(Data.size() <= 0x4000);
	T.Put(Data);
	CHECK(Received == 1);
	{
		const auto Poison = T.PoisonAfter(static_cast<int>(Data.size()));
		CHECK(T.Network->GetNextCommand() == -1);
		CHECK(T.Network->ReceiveBuffer.Size <= static_cast<int>(Data.size()));
		CHECK_THROWS_AS((void)T.Network->GetByte(), TMyException);
	}
	CHECK(T.Network->DroppedFrames == 1);
}

TEST_CASE("#139, item 7: a frame whose size is no multiple of 8 is dropped and logged, the next one read")
{
	for (const int Size : {1, 7, 9, 13}) {
		CAPTURE(Size);
		TNetworkTest T;
		const TLogCapture Log;
		std::string Bad(2, '\0');
		Bad[0] = static_cast<char>(Size);
		Bad += std::string(static_cast<std::size_t>(Size), '\x11');
		T.Put(Bad + Frame("\x09", 1));
		CHECK(Received == 1);
		// Tibia.exe threw "packet size does not fit to symmetric encryption" here, and at every later call.
		CHECK(T.Network->GetNextCommand() == 9);
		CHECK(T.Network->DroppedFrames == 1);
		CHECK(T.Network->LastDrop == patella::TFrameDrop::Size);
		CHECK(T.Network->GetNextCommand() == -1);
		CHECK(LogLines == "[warn] server frame dropped: its size " + std::to_string(Size) + " is no multiple of 8\n");
		T.Put(Frame("\x0a", 1));
		CHECK(T.Network->GetNextCommand() == 10);
	}
}

TEST_CASE("#139, item 6: a frame larger than ReceiveData is dropped as it comes, the frames around it read")
{
	for (const int Chunk : {1, 7, 1000, 0x4000}) {
		CAPTURE(Chunk);
		TNetworkTest T;
		const TLogCapture Log;
		const int Size = 0x5000; // more than ReceiveData's 0x4000 bytes
		std::string Large(2, '\0');
		Large[0] = static_cast<char>(Size & 0xff);
		Large[1] = static_cast<char>(Size >> 8);
		Large += std::string(Size, '\x22');
		std::string Incoming = Frame("\x08", 1) + Large + Frame("\x09", 1);
		std::string Commands;
		// The receive handler of the client reads every complete packet; Receive gives Chunk bytes at a time.
		while (!Incoming.empty()) {
			const std::size_t Count = Incoming.size() < static_cast<std::size_t>(Chunk) ? Incoming.size() : Chunk;
			T.Put(Incoming.substr(0, Count));
			REQUIRE(T.Sockets.Incoming.empty()); // Tibia.exe: ReceiveData full, Receive got no room, forever
			Incoming.erase(0, Count);
			for (int Command = T.Network->GetNextCommand(); Command != -1; Command = T.Network->GetNextCommand())
				Commands += static_cast<char>('0' + Command);
		}
		CHECK(Commands == "89");
		CHECK(T.Network->DroppedFrames == 1);
		CHECK(T.Network->LastDrop == patella::TFrameDrop::TooLarge);
		CHECK(T.Network->ReceiveSkip == 0);
		CHECK(LogLines == "[warn] server frame dropped: its size 20480 does not fit the receive buffer of 16384 bytes\n");
	}
}

TEST_CASE("#139, item 6: the largest frame ReceiveData holds is still read")
{
	TNetworkTest T;
	// 0x3ff8 bytes after the size (the largest multiple of 8 that fits with it): 0x3ffa bytes in ReceiveData.
	const std::string Data = Frame(std::string(0x3ff6, '\x05'), 0x3ff6);
	REQUIRE(Data.size() == 0x3ffa);
	T.Put(Data);
	CHECK(Received == 1);
	CHECK(T.Network->GetNextCommand() == 5);
	CHECK(T.Network->DroppedFrames == 0);
}

TEST_CASE("#142: an empty packet is read past: the frame after it is read in the same call, nothing dropped or logged")
{
	TNetworkTest T;
	const TLogCapture Log;
	const std::string Empty = Frame("", 0);
	REQUIRE(Empty.size() == 10);
	T.Put(Empty + Frame("\x09", 1) + Empty + Empty + Frame("\x07", 1));
	CHECK(Received == 1);
	// Tibia.exe returned -1 here, with the four frames behind the empty packet waiting.
	CHECK(T.Network->GetNextCommand() == 9);
	CHECK(T.Network->EmptyPackets == 1);
	CHECK(T.Network->GetNextCommand() == 7);
	CHECK(T.Network->EmptyPackets == 3);
	CHECK(T.Network->GetNextCommand() == -1);
	CHECK(T.Network->DroppedFrames == 0);
	CHECK(T.Network->LastDrop == patella::TFrameDrop::None);
	CHECK(LogLines.empty());
}

TEST_CASE("#142: an empty packet's frame leaves ReceiveData in the call that reads it")
{
	TNetworkTest T;
	T.Put(Frame("", 0));
	CHECK(Received == 1);
	{
		const auto Poison = T.PoisonAfter(10);
		CHECK(T.Network->GetNextCommand() == -1);
	}
	// Tibia.exe left it there (ReceiveRead 10) until the next PrepareNextPacket.
	CHECK(T.Network->ReceiveDataSize == 0);
	CHECK(T.Network->ReceivePacketEnd == 0);
	CHECK(T.Network->ReceiveRead == 0);
	CHECK(T.Network->ReceiveBuffer.Position == T.Network->ReceiveBuffer.Size);
	CHECK(T.Network->EmptyPackets == 1);
	T.Put(Frame("\x08", 1));
	CHECK(T.Network->GetNextCommand() == 8);
	CHECK(T.Network->GetNextCommand() == -1);
}

TEST_CASE("#142: a frame that fits ReceiveData only without the empty packet before it is read (the stall is gone)")
{
	// fuzz_network's case of #139: an empty packet, then a frame of 0x3ff8 bytes (0x3ffa with its size); together
	// 0x4004 bytes, more than ReceiveData. Tibia.exe kept the empty packet's frame in ReceiveData, the large frame
	// never completed, and no later byte came through.
	for (const int Chunk : {1, 2, 7, 0x4000}) {
		CAPTURE(Chunk);
		TNetworkTest T;
		std::string Incoming = Frame("", 0) + Frame(std::string(0x3ff6, '\x05'), 0x3ff6) + Frame("\x09", 1);
		std::string Commands;
		while (!Incoming.empty()) {
			const std::size_t Count = Incoming.size() < static_cast<std::size_t>(Chunk) ? Incoming.size() : Chunk;
			T.Put(Incoming.substr(0, Count));
			const std::size_t Taken = Count - T.Sockets.Incoming.size();
			REQUIRE(Taken > 0);
			Incoming.erase(0, Taken);
			for (int Command = T.Network->GetNextCommand(); Command != -1; Command = T.Network->GetNextCommand()) {
				Commands += static_cast<char>('0' + Command);
				while (T.Network->ReceiveBuffer.Position < T.Network->ReceiveBuffer.Size)
					(void)T.Network->GetByte();
			}
		}
		CHECK(Commands == "59");
		CHECK(T.Network->EmptyPackets == 1);
		CHECK(T.Network->DroppedFrames == 0);
	}
}

// --- #86 ---------------------------------------------------------------------------------------

namespace {

const char *const Thirty = "abcdefghijklmnopqrstuvwxyz0123";
static_assert(sizeof("abcdefghijklmnopqrstuvwxyz0123") == 31);

std::string Characters(std::size_t Count)
{
	std::string Text;
	for (std::size_t i = 0; i < Count; i++)
		Text += static_cast<char>('A' + i % 26);
	return Text;
}

} // namespace

TEST_CASE("#86: an ignore entry's name of 30 characters is read as its 30 characters")
{
	// An allocation of the entry's own 31 bytes: under ASan a read past Permanent is a report.
	auto Entry = std::make_unique<TIgnoreEntry>();
	Entry->AssignName(Thirty);
	Entry->Permanent = true;
	CHECK(std::string(Entry->NameText()) == Thirty);
	CHECK(Entry->Name.Length() == 30);
	Entry->Permanent = false;
	CHECK(std::string(Entry->NameText()) == Thirty);
	// The entries as the dialog's vector holds them: Tibia.exe's line was the name, "\x01" and "new entry".
	auto Entries = std::make_unique<TIgnoreEntry[]>(2);
	Entries[0].AssignName(Thirty);
	Entries[0].Permanent = true;
	Entries[1].AssignName("new entry");
	CHECK(std::string(Entries[0].NameText()) == Thirty);
	CHECK(std::string(Entries[1].NameText()) == "new entry");
	Entry->AssignName("Zed");
	CHECK(std::string(Entry->NameText()) == "Zed");
}

TEST_CASE("#86: a buddy's description of 128 characters is read as its 128 characters")
{
	// An allocation of the entry's own 0x8c bytes, its padding not 0: under ASan a read past it is a report.
	auto Buddy = std::make_unique<TBuddy>();
	Buddy->AssignDescriptionRead(Characters(131).c_str());
	Buddy->Notify = true;
	auto *Bytes = reinterpret_cast<unsigned char *>(Buddy.get());
	std::memset(Bytes + offsetof(TBuddy, Notify) + 1, 'x', sizeof(TBuddy) - offsetof(TBuddy, Notify) - 1);
	CHECK(std::string(Buddy->DescriptionText()) == Characters(128));
	Buddy->AssignDescriptionRead("short");
	CHECK(std::string(Buddy->DescriptionText()) == "short");
}

namespace {

namespace fs = std::filesystem;

// The test's own empty folder as the working directory, the old one back afterwards.
class TFolder {
public:
	explicit TFolder(const char *Name)
	{
		Old = fs::current_path();
		Path = fs::temp_directory_path() / Name;
		std::error_code Error;
		fs::remove_all(Path, Error);
		fs::create_directories(Path);
		fs::current_path(Path);
	}
	~TFolder()
	{
		std::error_code Error;
		fs::current_path(Old, Error);
		fs::remove_all(Path, Error);
	}
	TFolder(const TFolder &) = delete;
	TFolder &operator=(const TFolder &) = delete;
	fs::path Path;

private:
	fs::path Old;
};

std::string ReadFile(const fs::path &Path)
{
	std::ifstream File(Path, std::ios::binary);
	return {std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("#86: GetBuddyDescription, Patella.ini and Tibia.cfg's writer take 128 characters of a description of 128")
{
	TFolder Folder("patella-bounded-reads");
	const std::string Long = Characters(128);
	{
		std::ofstream File(Folder.Path / "Tibia.cfg", std::ios::binary);
		File << "Buddy = (11,1,\"" << Long << "\",yes)\r\n"
			 << "Buddy = (12,2,\"" << Characters(131) << "\",yes)\r\n"
			 << "Buddy = (13,3,\"next\",no)\r\n";
	}
	InitPreferences();
	// Tibia.exe's GetBuddyDescription(11) was the 128 characters, "\x01" (Notify) and the padding's bytes after it.
	CHECK(std::string(GetBuddyDescription(11)) == Long);
	CHECK(std::string(GetBuddyDescription(12)) == Long);
	CHECK(std::string(GetBuddyDescription(13)) == "next");
	CHECK(GetBuddyNotify(11));
	const patella::settings::TPreferenceValues Values = GetPreferenceValues();
	REQUIRE(Values.Buddies.size() == 3);
	CHECK(Values.Buddies[0].Description == Long);
	CHECK(Values.Buddies[1].Description == Long);
	const std::optional<patella::settings::TSettingsFile> Ini = patella::settings::Load(patella::platform::Platform().FileSystem, patella::settings::FileName);
	REQUIRE(Ini.has_value());
	CHECK(Ini->Get("Buddy1") == "11,1,\"" + Long + "\",yes");
	CHECK(Ini->Get("Buddy2") == "12,2,\"" + Long + "\",yes");
	// The original's writer (the golden corpus's reference since #45) writes the same 128.
	WriteTibiaCfg();
	const std::string Written = ReadFile(Folder.Path / "Tibia.cfg");
	CHECK(Written.find("Buddy = (11,1,\"" + Long + "\",yes)") != std::string::npos);
	CHECK(Written.find("Buddy = (12,2,\"" + Long + "\",yes)") != std::string::npos);
	ExitPreferences();
}

// --- #139, items 1 to 5 ------------------------------------------------------------------------

namespace {

// Little-endian file bytes, as the loaders read them.
void PutWord(std::string &File, int Value)
{
	File += static_cast<char>(Value & 0xff);
	File += static_cast<char>((Value >> 8) & 0xff);
}
void PutQuad(std::string &File, int Value)
{
	PutWord(File, Value & 0xffff);
	PutWord(File, (Value >> 16) & 0xffff);
}
void WriteFile(const fs::path &Path, const std::string &Bytes)
{
	std::ofstream File(Path, std::ios::binary);
	File.write(Bytes.data(), static_cast<std::streamsize>(Bytes.size()));
}

// One Tibia.dat entry of a single sprite: no flags, 1 x 1, one layer, pattern and phase.
std::string DatEntry()
{
	std::string Entry(1, '\xff');
	for (int i = 0; i < 7; i++)
		Entry += '\x01';
	PutWord(Entry, 1);
	return Entry;
}

} // namespace

TEST_CASE("#139, item 1: a Tibia.dat that ends early leaves the entries it did not reach without a sprite table")
{
	TFolder Folder("patella-bounded-reads");
	patella::TObjectTypes Types;
	// Objects 100 to 102, one outfit, effect and missile; the file ends after the first object.
	std::string Dat(4, '\0');
	PutWord(Dat, 102);
	PutWord(Dat, 1);
	PutWord(Dat, 1);
	PutWord(Dat, 1);
	Dat += DatEntry();
	WriteFile(Folder.Path / "Tibia.dat", Dat);
	CHECK_THROWS_AS(Types.LoadObjects("Tibia.dat"), TMyException);
	REQUIRE(Types.ObjectProperties != nullptr);
	CHECK((*Types.ObjectProperties)(100).Sprites != nullptr);
	CHECK((*Types.ObjectProperties)(101).Sprites == nullptr);
	CHECK((*Types.ObjectProperties)(102).Sprites == nullptr);
	CHECK((*Types.OutfitProperties)(1).Sprites == nullptr);
	CHECK((*Types.EffectProperties)(1).Sprites == nullptr);
	CHECK((*Types.MissileProperties)(1).Sprites == nullptr);
	// The original's ReleaseObjects deleted the indeterminate pointers here (ASan: a free of 0xbebebebebebebebe).
	Types.ReleaseObjects();
	CHECK(Types.ObjectProperties == nullptr);
	// A count of 0 outfits: the outfits' table refuses it after the objects' table was allocated.
	std::string NoOutfits(4, '\0');
	PutWord(NoOutfits, 101);
	PutWord(NoOutfits, 0);
	PutWord(NoOutfits, 1);
	PutWord(NoOutfits, 1);
	WriteFile(Folder.Path / "Tibia.dat", NoOutfits);
	CHECK_THROWS_AS(Types.LoadObjects("Tibia.dat"), TMyException);
	REQUIRE(Types.ObjectProperties != nullptr);
	CHECK((*Types.ObjectProperties)(100).Sprites == nullptr);
	CHECK((*Types.ObjectProperties)(101).Sprites == nullptr);
	Types.ExitObjects();
}

TEST_CASE("#139, item 2: a Tibia.pic that ends early leaves the pictures it did not reach without offsets")
{
	TFolder Folder("patella-bounded-reads");
	patella::TGraphics Graphics;
	// Three pictures; the file ends after the first one's table.
	std::string Pic(4, '\0');
	PutWord(Pic, 3);
	Pic += std::string("\x01\x01\x00\x00\x00", 5);
	PutQuad(Pic, 0);
	Pic += '\x01'; // the second picture's TilesX, then the end
	WriteFile(Folder.Path / "Tibia.pic", Pic);
	bool Thrown = false;
	try {
		Graphics.LoadPictures("Tibia.pic");
	} catch (const TProgramError &Error) {
		Thrown = Error.Code == PROGRAM_ERROR::PICTURES_FILE;
	}
	CHECK(Thrown);
	REQUIRE(Graphics.Pictures != nullptr);
	REQUIRE(Graphics.NumberOfPictures == 3);
	CHECK(Graphics.Pictures[0].Offsets != nullptr);
	CHECK(Graphics.Pictures[1].Offsets == nullptr);
	CHECK(Graphics.Pictures[2].Offsets == nullptr);
	// The original's ExitGraphics deleted the indeterminate pointers here.
	Graphics.ExitGraphics();
	CHECK(Graphics.Pictures == nullptr);
}

namespace {

// A sprite's or tile's runs: [u16 size]([u16 skip][u16 count][count RGB triples])...
std::string Runs(const std::vector<std::pair<int, int>> &SkipCount)
{
	std::string Body;
	for (const auto &[Skip, Count] : SkipCount) {
		PutWord(Body, Skip);
		PutWord(Body, Count);
		for (int i = 0; i < Count; i++)
			Body += std::string("\x0a\x0b\x0c", 3);
	}
	std::string Result;
	PutWord(Result, static_cast<int>(Body.size()));
	return Result + Body;
}

// The outcome of a decode into a buffer of exactly 32 x 32 RGB triples (an allocation of its own size: under ASan a
// write past it is a report).
template <class TDecode> bool DataError(TDecode Decode, PROGRAM_ERROR Expected)
{
	try {
		Decode();
	} catch (const TProgramError &Error) {
		return Error.Code == Expected;
	}
	return false;
}

} // namespace

TEST_CASE("#139, item 3: a sprite or tile whose runs pass the 1024th pixel is a data error, nothing written past")
{
	TFolder Folder("patella-bounded-reads");
	patella::TGraphics Graphics;
	// Sprites: 1 passes the 1024th pixel (skip 1000, 30 pixels), 2 ends exactly at it (skip 1020, 4 pixels), 3 starts
	// past it (skip 1024, 1 pixel).
	const std::string Sprite1 = std::string(3, '\0') + Runs({{1000, 30}});
	const std::string Sprite2 = std::string(3, '\0') + Runs({{1020, 4}});
	const std::string Sprite3 = std::string(3, '\0') + Runs({{1024, 1}});
	std::string Spr(4, '\0');
	PutWord(Spr, 3);
	const int First = 4 + 2 + 3 * 4;
	PutQuad(Spr, First);
	PutQuad(Spr, First + static_cast<int>(Sprite1.size()));
	PutQuad(Spr, First + static_cast<int>(Sprite1.size() + Sprite2.size()));
	Spr += Sprite1 + Sprite2 + Sprite3;
	WriteFile(Folder.Path / "Tibia.spr", Spr);
	Graphics.LoadSprites("Tibia.spr");
	auto Buffer = std::make_unique<uint8[]>(32 * 32 * 3);
	CHECK(DataError([&] { Graphics.LoadSprite(1, Buffer.get()); }, PROGRAM_ERROR::SPRITES_DATA));
	// The pixels before the 1024th are written as the original wrote them.
	CHECK(Buffer[1023 * 3] == 0x0a);
	CHECK(DataError([&] { Graphics.LoadSprite(3, Buffer.get()); }, PROGRAM_ERROR::SPRITES_DATA));
	Graphics.LoadSprite(2, Buffer.get());
	CHECK(Buffer[1019 * 3] == 0);
	CHECK(Buffer[1020 * 3] == 0x0a);
	CHECK(Buffer[1023 * 3 + 2] == 0x0c);
	// A picture of one tile that passes the 1024th pixel, and one of a tile that ends at it.
	std::string Pic(4, '\0');
	PutWord(Pic, 2);
	const int Tiles = 4 + 2 + 2 * (5 + 4);
	const std::string Tile1 = Runs({{0, 1}, {1022, 2}});
	const std::string Tile2 = Runs({{1023, 1}});
	Pic += std::string("\x01\x01\x00\x00\x00", 5);
	PutQuad(Pic, Tiles);
	Pic += std::string("\x01\x01\x00\x00\x00", 5);
	PutQuad(Pic, Tiles + static_cast<int>(Tile1.size()));
	Pic += Tile1 + Tile2;
	WriteFile(Folder.Path / "Tibia.pic", Pic);
	Graphics.LoadPictures("Tibia.pic");
	CHECK(DataError([&] { Graphics.LoadPictureTile(1, 0, 0, Buffer.get()); }, PROGRAM_ERROR::PICTURES_DATA));
	Graphics.LoadPictureTile(2, 0, 0, Buffer.get());
	CHECK(Buffer[1023 * 3] == 0x0a);
	Graphics.ExitGraphics();
}

TEST_CASE("#139, item 4: GetString of a text that does not fit throws before it allocates")
{
	// [u16 5]"ab": the text's 5 bytes do not fit in the packet's 4.
	uint8 Packet[] = {5, 0, 'a', 'b'};
	TReadBuffer Buffer(patella::TSpan<uint8>(Packet, sizeof(Packet)));
	const long long Before = GoldenArrayNews;
	CHECK_THROWS_AS((void)Buffer.GetString(), TMyException);
	// The original allocated the 6 bytes first and lost them at the throw.
	CHECK(GoldenArrayNews == Before);
	uint8 Fits[] = {2, 0, 'a', 'b'};
	TReadBuffer Good(patella::TSpan<uint8>(Fits, sizeof(Fits)));
	char *Text = Good.GetString();
	CHECK(GoldenArrayNews == Before + 1);
	CHECK(std::string(Text) == "ab");
	delete[] Text;
}

namespace {

class TMapConsoleLines final : public patella::TMapConsole {
public:
	bool HasDefaultConsole() override { return true; }
	void AddDefaultConsoleLine(const char *Message) override { Lines.emplace_back(Message); }
	std::vector<std::string> Lines;
};

void NoCreature(int) {}

} // namespace

TEST_CASE("#139, item 5: a .map file with a negative mark count is a corrupted file")
{
	TFolder Folder("patella-bounded-reads");
	for (const int Count : {-1, -5, -2000000000}) {
		CAPTURE(Count);
		std::string File(2 * 256 * 256, '\x07');
		PutQuad(File, Count);
		WriteFile(Folder.Path / "00000000.map", File);
		patella::TObjectTypes Types;
		TMapConsoleLines Console;
		auto Map = std::make_unique<patella::TMap>(Types, Console); // the block cache: too large for the stack
		patella::TMap &Automap = *Map;
		Automap.InitMap(NoCreature);
		std::vector<TMark> Marks;
		int Marked = -1;
		Automap.GetAutomapMarks(0, 0, 0, &Marks, &Marked);
		CHECK(Marked == 0);
		REQUIRE(Console.Lines.size() == 1);
		CHECK(Console.Lines[0] == "Map file '00000000.map' was corrupted!");
		// A mark goes to index 0 (the original's went to the negative index, growing the marks down to it).
		TCoordinate Coordinate{1, 1, 0};
		Automap.AddAutomapMark(Coordinate, "here", 1);
		Automap.GetAutomapMarks(0, 0, 0, &Marks, &Marked);
		CHECK(Marked == 1);
		REQUIRE(!Marks.empty());
		CHECK(std::string(static_cast<const char *>(Marks[0].Text)) == "here");
	}
}
