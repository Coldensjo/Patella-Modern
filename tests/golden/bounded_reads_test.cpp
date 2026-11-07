// bounded_reads (Patella-Modern issues #72 and #86): the reads of Tibia.exe that ran past their data, bounded as
// deliberate differences (notes/decisions.md, "#72 and #86"):
//
// - #72: TNetwork::PrepareNextPacket with a frame whose inner length runs past it (or a frame of size 0, which has
//   no inner length): the frame is dropped and a TMyException thrown, so no read of ReceiveBuffer leaves the frame.
//   Tibia.exe set the read buffer's size from the length as it was and read on, past ReceiveData into SendData.
// - #86: an ignore entry's name of 30 characters and a buddy's description of 128 have no NUL of their own (the next
//   byte is Permanent, Notify); TIgnoreEntry::NameText, TBuddy::DescriptionText and TPreferences::GetBuddyDescription
//   read at most 30 and 128 characters. Tibia.exe's readers read on into the next byte and past it.
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
#include "Network.h"
#include "Preferences.h"
#include "TMyException.h"
#include "TXTEA.h"
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
#include <system_error>

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

TEST_CASE("#72: a frame whose inner length runs past it is dropped and throws")
{
	for (const int Inner : {7, 8, 100, 0x3ff0, 0xffff}) {
		CAPTURE(Inner);
		TNetworkTest T;
		const std::string First = Frame("\x01\x02\x03\x04\x05", Inner);
		T.Put(First + Frame("\x09", 1));
		{
			// Tibia.exe read on past the frame here (into the next frame, and past ReceiveData into SendData).
			const auto Poison = T.PoisonAfter(static_cast<int>(First.size()));
			CHECK_THROWS_AS((void)T.Network->GetNextCommand(), TMyException);
			CHECK(T.Network->ReceiveRead == 10);
			CHECK(T.Network->ReceiveBuffer.Position == T.Network->ReceiveBuffer.Size);
			CHECK(T.Network->ReceiveBuffer.Size <= 10);
			CHECK_THROWS_AS((void)T.Network->GetByte(), TMyException);
		}
		// The next frame is read as usual.
		CHECK(T.Network->GetNextCommand() == 9);
		CHECK(T.Network->GetNextCommand() == -1);
	}
}

TEST_CASE("#72: a frame of size 0 has no inner length: it is dropped and throws")
{
	TNetworkTest T;
	T.Put(Frame("", 0, true) + Frame("\x07", 1));
	CHECK(Received == 1);
	{
		// Tibia.exe read the inner length from the next two bytes (the next frame's size here) and on from there.
		const auto Poison = T.PoisonAfter(2);
		CHECK_THROWS_AS((void)T.Network->GetNextCommand(), TMyException);
		CHECK(T.Network->ReceiveRead == 2);
		CHECK(T.Network->ReceiveBuffer.Position == T.Network->ReceiveBuffer.Size);
	}
	CHECK(T.Network->GetNextCommand() == 7);
	CHECK(T.Network->GetNextCommand() == -1);
}

TEST_CASE("#72: a frame that fills ReceiveData with a length past it reads nothing past ReceiveData")
{
	TNetworkTest T;
	// The largest frame ReceiveData holds: 0x4000 - 2 bytes after the size, a multiple of 8 below that.
	const std::string Payload(0x3ff0 - 2 - 8, '\x03');
	const std::string Data = Frame(Payload, 0xffff);
	REQUIRE(Data.size() <= 0x4000);
	T.Put(Data);
	CHECK(Received == 1);
	const auto Poison = T.PoisonAfter(static_cast<int>(Data.size()));
	CHECK_THROWS_AS((void)T.Network->GetNextCommand(), TMyException);
	CHECK(T.Network->ReceiveBuffer.Size <= static_cast<int>(Data.size()));
	CHECK_THROWS_AS((void)T.Network->GetByte(), TMyException);
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
