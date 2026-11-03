// The golden corpus's recorder and replayer (golden.h has the format).

// fopen and getenv as everywhere else (src/common.h).
#define _CRT_SECURE_NO_WARNINGS

#include "golden.h"

#include <filesystem>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace golden {

namespace {

TMode CurrentMode = TMode::Off;

// --- SHA-256 (FIPS 180-4), for the fingerprint of Tibia.exe ----------------------------------------

struct TSha256 {
	uint32_t State[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
		0x5be0cd19};
	uint8_t Block[64] = {};
	size_t Used = 0;
	uint64_t Length = 0;

	static uint32_t Rotate(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

	void Compress()
	{
		static const uint32_t K[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
			0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
			0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
			0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
			0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb,
			0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624,
			0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
			0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb,
			0xbef9a3f7, 0xc67178f2};
		uint32_t W[64];
		for (int i = 0; i < 16; i++)
			W[i] = (uint32_t)Block[4 * i] << 24 | (uint32_t)Block[4 * i + 1] << 16 | (uint32_t)Block[4 * i + 2] << 8
				| Block[4 * i + 3];
		for (int i = 16; i < 64; i++) {
			uint32_t s0 = Rotate(W[i - 15], 7) ^ Rotate(W[i - 15], 18) ^ (W[i - 15] >> 3);
			uint32_t s1 = Rotate(W[i - 2], 17) ^ Rotate(W[i - 2], 19) ^ (W[i - 2] >> 10);
			W[i] = W[i - 16] + s0 + W[i - 7] + s1;
		}
		uint32_t a = State[0], b = State[1], c = State[2], d = State[3], e = State[4], f = State[5], g = State[6],
				 h = State[7];
		for (int i = 0; i < 64; i++) {
			uint32_t t1 = h + (Rotate(e, 6) ^ Rotate(e, 11) ^ Rotate(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + W[i];
			uint32_t t2 = (Rotate(a, 2) ^ Rotate(a, 13) ^ Rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
			h = g;
			g = f;
			f = e;
			e = d + t1;
			d = c;
			c = b;
			b = a;
			a = t1 + t2;
		}
		State[0] += a;
		State[1] += b;
		State[2] += c;
		State[3] += d;
		State[4] += e;
		State[5] += f;
		State[6] += g;
		State[7] += h;
	}

	void Add(const uint8_t *Data, size_t Size)
	{
		for (size_t i = 0; i < Size; i++) {
			Block[Used++] = Data[i];
			if (Used == 64) {
				Compress();
				Used = 0;
			}
		}
		Length += Size;
	}

	void Finish(uint8_t Digest[32])
	{
		uint64_t Bits = Length * 8;
		uint8_t Pad = 0x80;
		Add(&Pad, 1);
		Pad = 0;
		while (Used != 56)
			Add(&Pad, 1);
		uint8_t Tail[8];
		for (int i = 0; i < 8; i++)
			Tail[i] = (uint8_t)(Bits >> (56 - 8 * i));
		Add(Tail, 8);
		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 4; j++)
				Digest[4 * i + j] = (uint8_t)(State[i] >> (24 - 8 * j));
	}
};

std::string Hex(const uint8_t *Data, size_t Size)
{
	static const char Digits[] = "0123456789abcdef";
	std::string Out;
	for (size_t i = 0; i < Size; i++) {
		Out += Digits[Data[i] >> 4];
		Out += Digits[Data[i] & 15];
	}
	return Out;
}

bool FromHex(const std::string &Text, uint8_t *Data, size_t Size)
{
	if (Text.size() != 2 * Size)
		return false;
	for (size_t i = 0; i < 2 * Size; i++) {
		char c = Text[i];
		int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10
																	  : -1;
		if (v < 0)
			return false;
		Data[i / 2] = (uint8_t)(i % 2 == 0 ? v << 4 : Data[i / 2] | v);
	}
	return true;
}

enum : uint8_t { TAG_NAME = 1,
	TAG_VALUE = 2,
	TAG_END = 3 };

// --- The recorder ----------------------------------------------------------------------------------

struct TRecorder {
	std::filesystem::path Directory; // <corpus>/<executable>
	std::string Executable;
	std::string TibiaSha256;
	uint8_t TibiaDigest[32] = {};
	std::string Manifest;
	// The test that runs.
	std::string Test;
	uint32_t Seed = 0;
	int ChecksBefore = 0;
	FILE *File = nullptr;
	std::map<std::string, uint32_t> Names;
	long long Records = 0;
};
TRecorder *Recorder;

void Fail(const std::string &Message)
{
	fprintf(stderr, "golden: %s\n", Message.c_str());
	printf("golden: %s\n", Message.c_str());
	fflush(stdout);
	exit(2);
}

void PutVarint(FILE *File, uint64_t Value)
{
	do {
		uint8_t Byte = (uint8_t)(Value & 0x7f);
		Value >>= 7;
		if (Value != 0)
			Byte |= 0x80;
		fputc(Byte, File);
	} while (Value != 0);
}
void PutU32(FILE *File, uint32_t Value)
{
	for (int i = 0; i < 4; i++)
		fputc((int)((Value >> (8 * i)) & 0xff), File);
}
void PutU64(FILE *File, uint64_t Value)
{
	for (int i = 0; i < 8; i++)
		fputc((int)((Value >> (8 * i)) & 0xff), File);
}

void OpenTestFile()
{
	std::filesystem::path Path = Recorder->Directory / (Recorder->Test + ".bin");
	Recorder->File = fopen(Path.string().c_str(), "wb");
	if (Recorder->File == nullptr)
		Fail("cannot write " + Path.string());
	static char Buffer[1 << 16];
	setvbuf(Recorder->File, Buffer, _IOFBF, sizeof(Buffer));
	fwrite("PGLD", 1, 4, Recorder->File);
	PutU32(Recorder->File, GOLDEN_FORMAT);
	fwrite(Recorder->TibiaDigest, 1, 32, Recorder->File);
	PutU32(Recorder->File, Recorder->Seed);
	uint16_t Length = (uint16_t)Recorder->Test.size();
	fputc(Length & 0xff, Recorder->File);
	fputc(Length >> 8, Recorder->File);
	fwrite(Recorder->Test.data(), 1, Length, Recorder->File);
}

void Record(const char *Name, char Role, char Type, size_t ElementSize, size_t Count, const void *Data)
{
	if (Recorder->Test.empty())
		Fail(std::string("record \"") + Name + "\" outside a test");
	if (Recorder->File == nullptr)
		OpenTestFile();
	FILE *File = Recorder->File;
	auto Found = Recorder->Names.find(Name);
	uint32_t Id;
	if (Found == Recorder->Names.end()) {
		Id = (uint32_t)Recorder->Names.size();
		Recorder->Names[Name] = Id;
		size_t Length = strlen(Name);
		fputc(TAG_NAME, File);
		PutVarint(File, Id);
		PutVarint(File, Length);
		fwrite(Name, 1, Length, File);
	} else
		Id = Found->second;
	fputc(TAG_VALUE, File);
	PutVarint(File, Id);
	fputc(Role, File);
	fputc(Type, File);
	fputc((int)ElementSize, File);
	PutVarint(File, Count);
	if (Count != 0)
		fwrite(Data, ElementSize, Count, File);
	Recorder->Records++;
}

// --- The replayer ----------------------------------------------------------------------------------

struct TReplay {
	std::string Path;
	std::vector<uint8_t> Data;
	size_t Position = 0;
	std::vector<std::string> Names;
	long long Records = 0;
};
TReplay *Replay;

[[noreturn]] void Stale(const std::string &Message)
{
	TStale Error;
	Error.Message = (Replay != nullptr ? Replay->Path + ": " : std::string()) + Message;
	throw Error;
}

uint8_t GetByte()
{
	if (Replay->Position >= Replay->Data.size())
		Stale("the file ends early");
	return Replay->Data[Replay->Position++];
}
uint64_t GetVarint()
{
	uint64_t Value = 0;
	for (int Shift = 0; Shift < 64; Shift += 7) {
		uint8_t Byte = GetByte();
		Value |= (uint64_t)(Byte & 0x7f) << Shift;
		if ((Byte & 0x80) == 0)
			return Value;
	}
	Stale("a bad number");
}
uint64_t GetFixed(int Bytes)
{
	uint64_t Value = 0;
	for (int i = 0; i < Bytes; i++)
		Value |= (uint64_t)GetByte() << (8 * i);
	return Value;
}
const uint8_t *GetBlock(size_t Size)
{
	if (Replay->Data.size() - Replay->Position < Size)
		Stale("the file ends early");
	const uint8_t *Block = Replay->Data.data() + Replay->Position;
	Replay->Position += Size;
	return Block;
}

// The next tag that is not a name definition.
uint8_t NextTag()
{
	for (;;) {
		uint8_t Tag = GetByte();
		if (Tag != TAG_NAME)
			return Tag;
		uint64_t Id = GetVarint();
		uint64_t Length = GetVarint();
		const uint8_t *Name = GetBlock((size_t)Length);
		if (Id != Replay->Names.size())
			Stale("a name out of order");
		Replay->Names.push_back(std::string((const char *)Name, (size_t)Length));
	}
}

const char *RoleName(char Role) { return Role == 'I' ? "input" : "output"; }

void Play(const char *Name, char Role, char Type, size_t ElementSize, size_t Count, void *Data)
{
	long long Index = Replay->Records;
	char Where[64];
	snprintf(Where, sizeof(Where), " (record %lld)", Index);
	uint8_t Tag = NextTag();
	if (Tag != TAG_VALUE)
		Stale(std::string("the test asks for the ") + RoleName(Role) + " \"" + Name + "\"" + Where
			+ ", the recording has no more values: the test changed since the recording");
	uint64_t Id = GetVarint();
	if (Id >= Replay->Names.size())
		Stale("an undefined name");
	const std::string &Recorded = Replay->Names[(size_t)Id];
	char RecordedRole = (char)GetByte();
	char RecordedType = (char)GetByte();
	size_t RecordedSize = GetByte();
	uint64_t RecordedCount = GetVarint();
	if (Recorded != Name || RecordedRole != Role)
		Stale(std::string("the test asks for the ") + RoleName(Role) + " \"" + Name + "\"" + Where
			+ ", the recording has the " + RoleName(RecordedRole) + " \"" + Recorded
			+ "\": the test changed since the recording");
	bool Text = Type == 'b' && Count == (size_t)-1; // OutputText: any length
	if (RecordedType != Type || RecordedSize != ElementSize || (!Text && RecordedCount != Count))
		Stale(std::string("\"") + Name + "\"" + Where + ": recorded as another type or size");
	const uint8_t *Value = GetBlock((size_t)(RecordedCount * RecordedSize));
	Replay->Records++;
	if (Text) {
		*(std::string *)Data = std::string((const char *)Value, (size_t)RecordedCount);
		return;
	}
	if (Role == 'O') {
		if (Count != 0)
			memcpy(Data, Value, Count * ElementSize);
	} else if (Count != 0 && memcmp(Data, Value, Count * ElementSize) != 0)
		Stale(std::string("the input \"") + Name + "\"" + Where
			+ " is not the recorded one: the test generates other inputs than at the recording");
}

} // namespace

TMode Mode() { return CurrentMode; }

std::string FileSha256(const char *Path)
{
	FILE *File = fopen(Path, "rb");
	if (File == nullptr)
		return std::string();
	TSha256 Hash;
	uint8_t Buffer[1 << 16];
	size_t Read;
	while ((Read = fread(Buffer, 1, sizeof(Buffer), File)) != 0)
		Hash.Add(Buffer, Read);
	fclose(File);
	uint8_t Digest[32];
	Hash.Finish(Digest);
	return Hex(Digest, 32);
}

void StartRecording(const char *Executable, const char *TibiaPath)
{
	const char *Target = getenv("DIFFTEST_RECORD");
	if (Target == nullptr || *Target == 0)
		return;
	if (getenv("DIFFTEST_ONLY") != nullptr || getenv("DIFFTEST_SKIP") != nullptr) {
		printf("golden: DIFFTEST_RECORD ignored: a corpus is recorded from full runs only (DIFFTEST_ONLY or "
			   "DIFFTEST_SKIP is set)\n");
		return;
	}
	Recorder = new TRecorder;
	Recorder->Executable = Executable;
	Recorder->TibiaSha256 = FileSha256(TibiaPath);
	if (Recorder->TibiaSha256.empty())
		Fail(std::string("cannot read ") + TibiaPath);
	FromHex(Recorder->TibiaSha256, Recorder->TibiaDigest, 32);
	if (Recorder->TibiaSha256 != TIBIA_SHA256)
		printf("golden: warning: %s is not Tibia.exe 7.72 (sha256 %s); the runner will not accept this corpus\n",
			TibiaPath, Recorder->TibiaSha256.c_str());
	Recorder->Directory = std::filesystem::path(Target) / Executable;
	std::error_code Error;
	std::filesystem::create_directories(Recorder->Directory, Error);
	if (Error)
		Fail("cannot create " + Recorder->Directory.string() + ": " + Error.message());
	// The earlier recording goes first: a recording that does not finish leaves no manifest.
	for (auto &Entry : std::filesystem::directory_iterator(Recorder->Directory, Error)) {
		std::string Extension = Entry.path().extension().string();
		if (Extension == ".bin" || Entry.path().filename() == "manifest.tsv")
			std::filesystem::remove(Entry.path(), Error);
	}
	char Line[256];
	snprintf(Line, sizeof(Line), "format\t%u\ntibia_sha256\t%s\nexecutable\t%s\n", (unsigned)GOLDEN_FORMAT,
		Recorder->TibiaSha256.c_str(), Executable);
	Recorder->Manifest = Line;
	CurrentMode = TMode::Record;
	printf("golden: recording %s's corpus into %s\n", Executable, Recorder->Directory.string().c_str());
}

void BeginTest(const char *Test, uint32_t Seed, int Checks)
{
	if (Recorder == nullptr)
		return;
	Recorder->Test = Test;
	if (Recorder->Test.compare(0, 4, "Test") == 0)
		Recorder->Test.erase(0, 4);
	Recorder->Seed = Seed;
	Recorder->ChecksBefore = Checks;
	Recorder->Records = 0;
	Recorder->Names.clear();
}

void EndTest(int Checks)
{
	if (Recorder == nullptr || Recorder->Test.empty())
		return;
	int TestChecks = Checks - Recorder->ChecksBefore;
	if (Recorder->File != nullptr) {
		fputc(TAG_END, Recorder->File);
		PutU64(Recorder->File, (uint64_t)TestChecks);
		PutU64(Recorder->File, (uint64_t)Recorder->Records);
		if (fclose(Recorder->File) != 0)
			Fail("cannot write the recording of " + Recorder->Test);
		Recorder->File = nullptr;
	}
	char Line[256];
	snprintf(Line, sizeof(Line), "test\t%s\t%u\t%d\t%lld\n", Recorder->Test.c_str(), (unsigned)Recorder->Seed,
		TestChecks, Recorder->Records);
	Recorder->Manifest += Line;
	Recorder->Test.clear();
}

void FinishRecording(int Checks, int Failures)
{
	if (Recorder == nullptr)
		return;
	char Line[128];
	snprintf(Line, sizeof(Line), "total\t%d\t%d\n", Checks, Failures);
	Recorder->Manifest += Line;
	std::filesystem::path Path = Recorder->Directory / "manifest.tsv";
	FILE *File = fopen(Path.string().c_str(), "wb");
	if (File == nullptr || fwrite(Recorder->Manifest.data(), 1, Recorder->Manifest.size(), File) != Recorder->Manifest.size()
		|| fclose(File) != 0)
		Fail("cannot write " + Path.string());
	printf("golden: %s's corpus recorded (%s)\n", Recorder->Executable.c_str(), Path.string().c_str());
}

bool ReadManifest(const std::string &Directory, const char *Executable, TManifest *Manifest)
{
	std::filesystem::path Path = std::filesystem::path(Directory) / Executable / "manifest.tsv";
	FILE *File = fopen(Path.string().c_str(), "rb");
	if (File == nullptr)
		return false;
	std::string Text;
	char Buffer[4096];
	size_t Read;
	while ((Read = fread(Buffer, 1, sizeof(Buffer), File)) != 0)
		Text.append(Buffer, Read);
	fclose(File);
	*Manifest = TManifest();
	size_t Start = 0;
	while (Start < Text.size()) {
		size_t End = Text.find('\n', Start);
		if (End == std::string::npos)
			End = Text.size();
		size_t Next = End + 1;
		if (End > Start && Text[End - 1] == '\r') // CRLF, from an editor
			End--;
		std::vector<std::string> Fields;
		size_t Field = Start;
		for (size_t i = Start; i <= End; i++)
			if (i == End || Text[i] == '\t') {
				Fields.push_back(Text.substr(Field, i - Field));
				Field = i + 1;
			}
		Start = Next;
		if (Fields.size() == 2 && Fields[0] == "format")
			Manifest->Format = (uint32_t)strtoul(Fields[1].c_str(), nullptr, 10);
		else if (Fields.size() == 2 && Fields[0] == "tibia_sha256")
			Manifest->TibiaSha256 = Fields[1];
		else if (Fields.size() == 2 && Fields[0] == "executable")
			Manifest->Executable = Fields[1];
		else if (Fields.size() == 5 && Fields[0] == "test" && Manifest->NumberOfTests < 64) {
			TTestEntry &Entry = Manifest->Tests[Manifest->NumberOfTests++];
			Entry.Name = Fields[1];
			Entry.Seed = (uint32_t)strtoul(Fields[2].c_str(), nullptr, 10);
			Entry.Checks = strtoll(Fields[3].c_str(), nullptr, 10);
			Entry.Records = strtoll(Fields[4].c_str(), nullptr, 10);
		} else if (Fields.size() == 3 && Fields[0] == "total") {
			Manifest->Checks = strtoll(Fields[1].c_str(), nullptr, 10);
			Manifest->Failures = strtoll(Fields[2].c_str(), nullptr, 10);
		}
	}
	TStale Error;
	if (Manifest->Format != GOLDEN_FORMAT)
		Error.Message = "format " + std::to_string(Manifest->Format) + ", the runner reads format "
			+ std::to_string(GOLDEN_FORMAT);
	else if (Manifest->TibiaSha256 != TIBIA_SHA256)
		Error.Message = "recorded from another Tibia.exe (sha256 " + Manifest->TibiaSha256 + ", expected "
			+ TIBIA_SHA256 + ")";
	else if (Manifest->Executable != Executable || Manifest->Checks < 0)
		Error.Message = "incomplete";
	else if (Manifest->Failures != 0)
		Error.Message = "recorded from a run with " + std::to_string(Manifest->Failures) + " failures";
	if (!Error.Message.empty()) {
		Error.Message = Path.string() + ": " + Error.Message + "; record the corpus again";
		throw Error;
	}
	return true;
}

uint32_t BeginReplay(const std::string &Directory, const char *Executable, const char *Test)
{
	delete Replay;
	Replay = new TReplay;
	Replay->Path = (std::filesystem::path(Directory) / Executable / (std::string(Test) + ".bin")).string();
	FILE *File = fopen(Replay->Path.c_str(), "rb");
	if (File == nullptr)
		Stale("missing; record the corpus again");
	uint8_t Buffer[1 << 16];
	size_t Read;
	while ((Read = fread(Buffer, 1, sizeof(Buffer), File)) != 0)
		Replay->Data.insert(Replay->Data.end(), Buffer, Buffer + Read);
	fclose(File);
	const uint8_t *Magic = GetBlock(4);
	if (memcmp(Magic, "PGLD", 4) != 0)
		Stale("not a corpus file");
	uint32_t Format = (uint32_t)GetFixed(4);
	if (Format != GOLDEN_FORMAT)
		Stale("format " + std::to_string(Format) + ", the runner reads format " + std::to_string(GOLDEN_FORMAT)
			+ "; record the corpus again");
	std::string Digest = Hex(GetBlock(32), 32);
	if (Digest != TIBIA_SHA256)
		Stale("recorded from another Tibia.exe (sha256 " + Digest + "); record the corpus again");
	uint32_t Seed = (uint32_t)GetFixed(4);
	size_t Length = (size_t)GetFixed(2);
	std::string Name((const char *)GetBlock(Length), Length);
	if (Name != Test)
		Stale("the recording of " + Name + ", not of " + Test);
	CurrentMode = TMode::Replay;
	return Seed;
}

void EndReplay(int Checks)
{
	uint8_t Tag = NextTag();
	if (Tag != TAG_END)
		Stale("the test ended before the recording's values did: the test changed since the recording");
	uint64_t RecordedChecks = GetFixed(8);
	uint64_t RecordedRecords = GetFixed(8);
	if (Replay->Position != Replay->Data.size())
		Stale("data after the end");
	if (RecordedRecords != (uint64_t)Replay->Records)
		Stale("the recording has " + std::to_string(RecordedRecords) + " values, the test used "
			+ std::to_string(Replay->Records));
	if (RecordedChecks != (uint64_t)Checks)
		Stale("the oracle made " + std::to_string(RecordedChecks) + " checks, the runner "
			+ std::to_string(Checks));
	CurrentMode = TMode::Off;
	delete Replay;
	Replay = nullptr;
}

void Exchange(const char *Name, char Role, char Type, size_t ElementSize, size_t Count, void *Data)
{
	if (CurrentMode == TMode::Record)
		Record(Name, Role, Type, ElementSize, Count, Data);
	else if (CurrentMode == TMode::Replay)
		Play(Name, Role, Type, ElementSize, Count, Data);
}

std::string OutputText(const char *Name, const char *Text)
{
	if (CurrentMode == TMode::Record)
		Record(Name, 'O', 'b', 1, strlen(Text), Text);
	else if (CurrentMode == TMode::Replay) {
		std::string Recorded;
		Play(Name, 'O', 'b', 1, (size_t)-1, &Recorded);
		return Recorded;
	}
	return Text != nullptr ? std::string(Text) : std::string();
}

} // namespace golden
