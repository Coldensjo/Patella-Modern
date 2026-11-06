// The golden corpus's replayer (golden.h has the format). The recorder was the 32-bit oracle's, until
// Patella-Modern issue #94: the corpus is frozen (issue #93).

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

enum : uint8_t { TAG_NAME = 1,
	TAG_VALUE = 2,
	TAG_END = 3 };

// --- The replayer ----------------------------------------------------------------------------------

struct TReplay {
	std::string Path;
	std::vector<uint8_t> Data;
	size_t Position = 0;
	std::vector<std::string> Names;
	long long Records = 0;
	long long Retired = 0; // the checks of the test's retired parts (RetiredBegin)
};
TReplay *Replay;
long long TotalRetired = 0;

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

namespace {

// The lines of Text, each split at its tabs (a CR before the LF dropped: CRLF from an editor).
std::vector<std::vector<std::string>> TabLines(const std::string &Text)
{
	std::vector<std::vector<std::string>> Lines;
	size_t Start = 0;
	while (Start < Text.size()) {
		size_t End = Text.find('\n', Start);
		if (End == std::string::npos)
			End = Text.size();
		const size_t Next = End + 1;
		if (End > Start && Text[End - 1] == '\r')
			End--;
		std::vector<std::string> Fields;
		size_t Field = Start;
		for (size_t i = Start; i <= End; i++)
			if (i == End || Text[i] == '\t') {
				Fields.push_back(Text.substr(Field, i - Field));
				Field = i + 1;
			}
		Lines.push_back(Fields);
		Start = Next;
	}
	return Lines;
}

bool ReadText(const std::filesystem::path &Path, std::string *Text)
{
	FILE *File = fopen(Path.string().c_str(), "rb");
	if (File == nullptr)
		return false;
	char Buffer[4096];
	size_t Read;
	while ((Read = fread(Buffer, 1, sizeof(Buffer), File)) != 0)
		Text->append(Buffer, Read);
	fclose(File);
	return true;
}

// A decimal number of the manifest, or -1.
long long ManifestNumber(const std::string &Text)
{
	if (Text.empty() || Text.find_first_not_of("0123456789") != std::string::npos)
		return -1;
	return strtoll(Text.c_str(), nullptr, 10);
}

} // namespace

void VerifyDataManifest(
	const std::string &Directory, const char *Kind, TDataManifest *Manifest, std::vector<std::string> *Problems)
{
	*Manifest = TDataManifest();
	const std::filesystem::path Root(Directory);
	std::string Text;
	if (!ReadText(Root / DATA_MANIFEST, &Text)) {
		Problems->push_back(std::string("there is no ") + DATA_MANIFEST);
		return;
	}
	Manifest->Found = true;
	struct TListed {
		long long Size;
		std::string Sha256;
		bool Seen = false;
	};
	std::map<std::string, TListed> Listed;
	long long ListedBytes = 0;
	int Number = 0;
	bool Format = false;
	for (const std::vector<std::string> &Fields : TabLines(Text)) {
		Number++;
		if (!Fields.empty() && !Fields[0].empty() && Fields[0][0] == '#')
			continue;
		const std::string &Key = Fields[0];
		if (Number == 1 && !(Fields.size() == 2 && Key == "format" && Fields[1] == DATA_MANIFEST_FORMAT)) {
			Problems->push_back(std::string(DATA_MANIFEST) + "'s first line is not \"format\t" + DATA_MANIFEST_FORMAT
				+ "\": another format, which this runner does not read");
			return;
		}
		if (Fields.size() == 2 && Key == "format")
			Format = true;
		else if (Fields.size() == 2 && Key == "kind")
			Manifest->Kind = Fields[1];
		else if (Fields.size() == 2 && Key == "commit")
			Manifest->Commit = Fields[1];
		else if (Fields.size() == 2 && Key == "clean")
			Manifest->Clean = Fields[1];
		else if (Fields.size() == 2 && Key == "date")
			Manifest->Date = Fields[1];
		else if (Fields.size() == 2 && Key == "time_zone")
			Manifest->TimeZone = Fields[1];
		else if (Fields.size() == 2 && Key == "tibia_sha256")
			Manifest->TibiaSha256 = Fields[1];
		else if ((Fields.size() == 3 && (Key == "tool" || Key == "manifest")) || (Fields.size() == 4 && Key == "program")) {
		} else if (Fields.size() == 3 && Key == "files") {
			Manifest->Files = ManifestNumber(Fields[1]);
			Manifest->Bytes = ManifestNumber(Fields[2]);
		} else if (Fields.size() == 4 && Key == "file" && ManifestNumber(Fields[1]) >= 0) {
			if (!Listed.emplace(Fields[3], TListed{ManifestNumber(Fields[1]), Fields[2]}).second)
				Problems->push_back(Fields[3] + " is listed twice");
			ListedBytes += ManifestNumber(Fields[1]);
		} else
			Problems->push_back(std::string(DATA_MANIFEST) + ", line " + std::to_string(Number) + ": not a line of the format");
	}
	if (!Format)
		Problems->push_back(std::string(DATA_MANIFEST) + " has no format line");
	if (Manifest->Kind != Kind)
		Problems->push_back(std::string("the manifest is of kind \"") + Manifest->Kind + "\", not \"" + Kind + "\"");
	if (Manifest->TimeZone.empty())
		Problems->push_back("the manifest names no time zone");
	if (Manifest->Commit.empty() || Manifest->Date.empty())
		Problems->push_back("the manifest has no commit or no date");
	if (Manifest->TibiaSha256 != TIBIA_SHA256)
		Problems->push_back("the manifest's Tibia.exe is not " + std::string(TIBIA_SHA256));
	if (Manifest->Files != (long long)Listed.size() || Manifest->Bytes != ListedBytes)
		Problems->push_back("the manifest lists " + std::to_string(Listed.size()) + " files of " + std::to_string(ListedBytes)
			+ " bytes, its files line says " + std::to_string(Manifest->Files) + " of " + std::to_string(Manifest->Bytes));
	// Every file in the directory: listed, with the listed size and SHA-256.
	std::error_code Error;
	std::vector<std::string> Unlisted;
	for (std::filesystem::recursive_directory_iterator Entry(Root, Error), End; !Error && Entry != End;
		Entry.increment(Error)) {
		std::error_code FileError;
		if (!Entry->is_regular_file(FileError))
			continue;
		const std::string Relative = Entry->path().lexically_relative(Root).generic_string();
		if (Relative == DATA_MANIFEST)
			continue;
		auto Found = Listed.find(Relative);
		if (Found == Listed.end()) {
			Unlisted.push_back(Relative);
			continue;
		}
		Found->second.Seen = true;
		const long long Size = (long long)Entry->file_size(FileError);
		if (Size != Found->second.Size)
			Problems->push_back(Relative + " has " + std::to_string(Size) + " bytes, the manifest " + std::to_string(Found->second.Size));
		else if (FileSha256(Entry->path().string().c_str()) != Found->second.Sha256)
			Problems->push_back(Relative + " has another SHA-256 than the manifest's");
	}
	if (Error)
		Problems->push_back("cannot list " + Directory + ": " + Error.message());
	for (const auto &[Path, File] : Listed)
		if (!File.Seen)
			Problems->push_back(Path + " is missing");
	for (const std::string &Path : Unlisted)
		Problems->push_back(Path + " is not in the manifest");
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
		Error.Message = Path.string() + ": " + Error.Message + "; the corpus is frozen: regenerated only from the tag layout-frozen-final";
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
		Stale("missing; the corpus is frozen: regenerated only from the tag layout-frozen-final");
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
			+ "; the corpus is frozen: regenerated only from the tag layout-frozen-final");
	std::string Digest = Hex(GetBlock(32), 32);
	if (Digest != TIBIA_SHA256)
		Stale("recorded from another Tibia.exe (sha256 " + Digest + "); the corpus is frozen: regenerated only from the tag layout-frozen-final");
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
	if (RecordedChecks != (uint64_t)(Checks + Replay->Retired))
		Stale("the oracle made " + std::to_string(RecordedChecks) + " checks, the runner "
			+ std::to_string(Checks) + " and " + std::to_string(Replay->Retired) + " retired");
	CurrentMode = TMode::Off;
	delete Replay;
	Replay = nullptr;
}

bool RetiredBegin(const char *Name, int Checks)
{
	(void)Checks; // the recorder's (the part's checks at its start; the oracle's, until issue #94)
	InputBytes("retired", Name, strlen(Name));
	if (CurrentMode != TMode::Replay)
		return false;
	// The part's values, up to its "retired end": skipped by their sizes (a value's name is checked, not
	// its contents).
	long long Skipped = 0;
	for (;;) {
		char Where[64];
		snprintf(Where, sizeof(Where), " (record %lld)", Replay->Records);
		if (NextTag() != TAG_VALUE)
			Stale(std::string("the retired part \"") + Name + "\"" + Where + " has no end: the test changed since the recording");
		uint64_t Id = GetVarint();
		if (Id >= Replay->Names.size())
			Stale("an undefined name");
		const std::string &Recorded = Replay->Names[(size_t)Id];
		char Role = (char)GetByte();
		char Type = (char)GetByte();
		size_t Size = GetByte();
		uint64_t Count = GetVarint();
		const uint8_t *Value = GetBlock((size_t)(Count * Size));
		Replay->Records++;
		if (Recorded == "retired")
			Stale(std::string("a retired part inside the retired part \"") + Name + "\"" + Where);
		if (Recorded != "retired end") {
			Skipped++;
			continue;
		}
		uint64_t End[2];
		if (Role != 'O' || Type != 'i' || Size != sizeof(End[0]) || Count != 2)
			Stale(std::string("\"retired end\"") + Where + ": recorded as another type or size");
		memcpy(End, Value, sizeof(End));
		if (End[1] != (uint64_t)Skipped)
			Stale(std::string("the retired part \"") + Name + "\"" + Where + " has " + std::to_string(Skipped)
				+ " values, its end says " + std::to_string(End[1]));
		Replay->Retired += (long long)End[0];
		TotalRetired += (long long)End[0];
		return true;
	}
}

void RetiredEnd(int Checks)
{
	(void)Checks; // the runner read the end in RetiredBegin (the recorder wrote it here until issue #94)
}

long long RetiredChecks() { return TotalRetired; }

void Exchange(const char *Name, char Role, char Type, size_t ElementSize, size_t Count, void *Data)
{
	if (CurrentMode == TMode::Replay)
		Play(Name, Role, Type, ElementSize, Count, Data);
}

std::string OutputText(const char *Name, const char *Text)
{
	if (CurrentMode == TMode::Replay) {
		std::string Recorded;
		Play(Name, 'O', 'b', 1, (size_t)-1, &Recorded);
		return Recorded;
	}
	return Text != nullptr ? std::string(Text) : std::string();
}

std::string OutputText(const char *Name, const char *Text, size_t Length)
{
	if (CurrentMode == TMode::Replay) {
		std::string Recorded;
		Play(Name, 'O', 'b', 1, (size_t)-1, &Recorded);
		return Recorded;
	}
	return Text != nullptr ? std::string(Text, Length) : std::string();
}

void OutputChanges(const char *Name, void *Last, const void *Value, size_t Size)
{
	uint8_t *Previous = (uint8_t *)Last;
	uint32_t Span[2] = {0, 0};          // the first changed byte, the number of bytes from there to the last
	if (CurrentMode != TMode::Replay) { // without a corpus (TMode::Off): computed here
		const uint8_t *Current = (const uint8_t *)Value;
		size_t First = 0, End = Size;
		while (First < Size && Previous[First] == Current[First])
			First++;
		while (End > First && Previous[End - 1] == Current[End - 1])
			End--;
		Span[0] = (uint32_t)First;
		Span[1] = (uint32_t)(End - First);
	}
	Output(Name, Span);
	if (Span[0] > Size || Span[1] > Size - Span[0])
		throw TStale{std::string("\"") + Name + "\": a change outside the buffer"};
	std::string Bytes = OutputText(Name, Value != nullptr ? (const char *)Value + Span[0] : nullptr, Span[1]);
	if (CurrentMode == TMode::Replay && Bytes.size() != Span[1])
		throw TStale{std::string("\"") + Name + "\": the changed bytes are not the recorded ones"};
	if (Span[1] != 0)
		memcpy(Previous + Span[0], Bytes.data(), Span[1]);
}

void OutputRuns(const char *Name, void *Last, const void *Value, size_t Size)
{
	uint8_t *Previous = (uint8_t *)Last;
	std::vector<uint32_t> Runs; // offset, length
	std::string Bytes;
	if (CurrentMode != TMode::Replay) { // without a corpus (TMode::Off): computed here
		const uint8_t *Current = (const uint8_t *)Value;
		size_t i = 0;
		while (i < Size) {
			if (Previous[i] == Current[i]) {
				i++;
				continue;
			}
			size_t End = i + 1;
			for (size_t j = i + 1; j < Size && j - End < 16; j++)
				if (Previous[j] != Current[j])
					End = j + 1;
			Runs.push_back((uint32_t)i);
			Runs.push_back((uint32_t)(End - i));
			Bytes.append((const char *)Current + i, End - i);
			i = End;
		}
	}
	uint32_t Count = (uint32_t)(Runs.size() / 2);
	Output(Name, Count);
	if (Count == 0)
		return;
	Runs.resize((size_t)Count * 2);
	OutputArray(Name, Runs.data(), Runs.size());
	size_t Total = 0;
	for (uint32_t r = 0; r < Count; r++) {
		if (Runs[2 * r] > Size || Runs[2 * r + 1] > Size - Runs[2 * r])
			throw TStale{std::string("\"") + Name + "\": a change outside the buffer"};
		Total += Runs[2 * r + 1];
	}
	Bytes = OutputText(Name, Value != nullptr ? Bytes.data() : nullptr, Bytes.size());
	if (Bytes.size() != Total)
		throw TStale{std::string("\"") + Name + "\": the changed bytes are not the recorded ones"};
	size_t Position = 0;
	for (uint32_t r = 0; r < Count; r++) {
		memcpy(Previous + Runs[2 * r], Bytes.data() + Position, Runs[2 * r + 1]);
		Position += Runs[2 * r + 1];
	}
}

std::string OutputTextChanges(const char *Name, std::string *Last, const std::string &Text)
{
	uint32_t Span[3] = {0, 0, 0};       // the bytes kept from Last's start, from its end, the new text's length
	if (CurrentMode != TMode::Replay) { // without a corpus (TMode::Off): computed here
		size_t Shorter = Text.size() < Last->size() ? Text.size() : Last->size(), Start = 0, End = 0;
		while (Start < Shorter && Text[Start] == (*Last)[Start])
			Start++;
		while (End < Shorter - Start && Text[Text.size() - 1 - End] == (*Last)[Last->size() - 1 - End])
			End++;
		Span[0] = (uint32_t)Start;
		Span[1] = (uint32_t)End;
		Span[2] = (uint32_t)Text.size();
	}
	Output(Name, Span);
	if (Span[0] > Last->size() || Span[1] > Last->size() - Span[0] || Span[0] + Span[1] > Span[2])
		throw TStale{std::string("\"") + Name + "\": a change outside the text"};
	size_t Middle = Span[2] - Span[0] - Span[1];
	std::string Bytes = OutputText(Name, CurrentMode != TMode::Replay ? Text.data() + Span[0] : nullptr, Middle);
	if (Bytes.size() != Middle)
		throw TStale{std::string("\"") + Name + "\": the changed bytes are not the recorded ones"};
	*Last = Last->substr(0, Span[0]) + Bytes + Last->substr(Last->size() - Span[1]);
	return *Last;
}

std::string DataFile(const char *Name, const char *ExePath)
{
	std::string Path, Contents;
	uint8_t Found = 0;
	if (CurrentMode != TMode::Replay) { // without a corpus (TMode::Off): computed here
		Path = ExePath != nullptr ? ExePath : "";
		size_t Slash = Path.find_last_of("\\/");
		Path = (Slash == std::string::npos ? std::string() : Path.substr(0, Slash + 1)) + Name;
		if (FILE *File = fopen(Path.c_str(), "rb")) {
			Found = 1;
			fclose(File);
		}
	}
	Output(Name, Found);
	if (Found == 0)
		return std::string();
	if (CurrentMode == TMode::Off)
		return Path;
	Contents = OutputText(Name, Contents.data(), Contents.size());
	std::error_code Error;
	std::filesystem::path Directory = std::filesystem::temp_directory_path(Error) / "golden-data";
	std::filesystem::create_directories(Directory, Error);
	Path = (Directory / Name).string();
	FILE *File = fopen(Path.c_str(), "wb");
	if (File == nullptr || fwrite(Contents.data(), 1, Contents.size(), File) != Contents.size() || fclose(File) != 0)
		throw TStale{"cannot write " + Path};
	return Path;
}

TText RecordedText(const char *Name, const char *Text, size_t MaxLength)
{
	TText Result;
	uint8_t Null = Text == nullptr;
	Output(Name, Null);
	Result.Null = Null != 0;
	if (!Result.Null) {
		size_t Length = 0;
		if (Text != nullptr)
			while (Length < MaxLength && Text[Length] != 0)
				Length++;
		Result.Text = OutputText(Name, Text, Length);
	}
	return Result;
}

} // namespace golden
