// driver.cpp: the fuzz targets' main without libFuzzer (Patella-Modern issue #132; fuzz.h).
//
//	fuzz_<target> [--mutations=N] [--seed=N] [--max-len=N] [--seconds=N] <file or folder> ...
//
// Runs LLVMFuzzerTestOneInput on every file given (a folder: its files, in name order), then on N mutations of them
// (default 0): each a copy of an input picked at random with a few of the changes below (bits, bytes, the edges of
// 8- and 16-bit values, insertions, removals, repeats and splices), at most --max-len bytes (default: 4096 or the
// largest input). The pseudo-random sequence is --seed's (default 1), so a run is the same on every platform;
// --seconds (default 0: none) ends the mutations early. A finding aborts (the sanitizers' reports, the harness's
// Fail); the exit code is 0 otherwise, 2 for a usage error or no input. This is the CTest smoke run of every preset;
// the fuzzing itself is libFuzzer's (PATELLA_FUZZ=ON). When a run aborts (a Fail, an assertion of the C library), the
// input goes to fuzz-crash-<program>.bin in the working directory the driver started in, as libFuzzer's crash- file.

#include "fuzz.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

using TBytes = std::vector<std::uint8_t>;

class TRandom {
public:
	explicit TRandom(std::uint64_t Seed) : State(Seed * 0x9e3779b97f4a7c15ULL + 1) {}
	std::uint64_t Next()
	{
		State ^= State << 13;
		State ^= State >> 7;
		State ^= State << 17;
		return State;
	}
	std::size_t Below(std::size_t Limit) { return Limit == 0 ? 0 : static_cast<std::size_t>(Next() % Limit); }

private:
	std::uint64_t State;
};

bool ReadInput(const fs::path &Path, std::vector<TBytes> &Inputs)
{
	std::ifstream File(Path, std::ios::binary);
	if (!File)
		return false;
	Inputs.emplace_back(std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>());
	return true;
}

void Mutate(TBytes &Data, const std::vector<TBytes> &Inputs, TRandom &Random, std::size_t MaxLength)
{
	static const std::uint8_t Edges8[] = {0x00, 0x01, 0x07, 0x08, 0x7f, 0x80, 0xfe, 0xff};
	static const std::uint16_t Edges16[] = {0x0000, 0x0001, 0x0002, 0x0007, 0x0008, 0x00ff, 0x0100, 0x3ff0, 0x4000,
		0x7fff, 0x8000, 0xfffe, 0xffff};
	const std::size_t Changes = 1 + Random.Below(4);
	for (std::size_t c = 0; c < Changes; c++) {
		const std::size_t Size = Data.size();
		switch (Random.Below(9)) {
		case 0: // a bit
			if (Size != 0)
				Data[Random.Below(Size)] ^= static_cast<std::uint8_t>(1U << Random.Below(8));
			break;
		case 1: // a byte
			if (Size != 0)
				Data[Random.Below(Size)] = static_cast<std::uint8_t>(Random.Next());
			break;
		case 2: // an 8-bit edge
			if (Size != 0)
				Data[Random.Below(Size)] = Edges8[Random.Below(std::size(Edges8))];
			break;
		case 3: // a 16-bit edge, little-endian
			if (Size >= 2) {
				const std::size_t At = Random.Below(Size - 1);
				const std::uint16_t Value = Edges16[Random.Below(std::size(Edges16))];
				Data[At] = static_cast<std::uint8_t>(Value & 0xff);
				Data[At + 1] = static_cast<std::uint8_t>(Value >> 8);
			}
			break;
		case 4: // insert bytes
		{
			const std::size_t Count = 1 + Random.Below(8);
			const std::size_t At = Random.Below(Size + 1);
			TBytes Inserted(Count);
			for (std::uint8_t &Byte : Inserted)
				Byte = static_cast<std::uint8_t>(Random.Next());
			Data.insert(Data.begin() + static_cast<std::ptrdiff_t>(At), Inserted.begin(), Inserted.end());
			break;
		}
		case 5: // remove bytes
			if (Size != 0) {
				const std::size_t At = Random.Below(Size);
				const std::size_t Count = 1 + Random.Below(std::min<std::size_t>(Size - At, 16));
				Data.erase(Data.begin() + static_cast<std::ptrdiff_t>(At),
					Data.begin() + static_cast<std::ptrdiff_t>(At + Count));
			}
			break;
		case 6: // repeat a piece
			if (Size != 0) {
				const std::size_t At = Random.Below(Size);
				const std::size_t Count = 1 + Random.Below(std::min<std::size_t>(Size - At, 64));
				const TBytes Piece(Data.begin() + static_cast<std::ptrdiff_t>(At),
					Data.begin() + static_cast<std::ptrdiff_t>(At + Count));
				const std::size_t To = Random.Below(Size + 1);
				Data.insert(Data.begin() + static_cast<std::ptrdiff_t>(To), Piece.begin(), Piece.end());
			}
			break;
		case 7: // cut the end
			if (Size != 0)
				Data.resize(Random.Below(Size));
			break;
		default: // splice: the start of this one, the rest of another
		{
			const TBytes &Other = Inputs[Random.Below(Inputs.size())];
			const std::size_t Cut = Random.Below(Size + 1);
			const std::size_t From = Random.Below(Other.size() + 1);
			Data.resize(Cut);
			Data.insert(Data.end(), Other.begin() + static_cast<std::ptrdiff_t>(From), Other.end());
			break;
		}
		}
	}
	if (Data.size() > MaxLength)
		Data.resize(MaxLength);
}

const TBytes *Current = nullptr; // the input that runs
std::string CrashFile;           // where an aborted run's input goes

extern "C" void SaveCrash(int)
{
	if (Current != nullptr && !CrashFile.empty()) {
		if (std::FILE *File = std::fopen(CrashFile.c_str(), "wb")) {
			if (!Current->empty())
				std::fwrite(Current->data(), 1, Current->size(), File);
			std::fclose(File);
			std::fprintf(stderr, "driver: the input is in %s\n", CrashFile.c_str());
		}
	}
	std::signal(SIGABRT, SIG_DFL);
}

void Run(const TBytes &Data)
{
	// A copy of its own size: under AddressSanitizer a read past the input is a report, as with libFuzzer.
	std::vector<std::uint8_t> Copy(Data);
	Current = &Data;
	LLVMFuzzerTestOneInput(Copy.empty() ? nullptr : Copy.data(), Copy.size());
	Current = nullptr;
}

bool Option(const char *Argument, const char *Name, unsigned long long &Value)
{
	const std::size_t Length = std::strlen(Name);
	if (std::strncmp(Argument, Name, Length) != 0)
		return false;
	Value = std::strtoull(Argument + Length, nullptr, 10);
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	unsigned long long Mutations = 0;
	unsigned long long Seed = 1;
	unsigned long long MaxLength = 0;
	unsigned long long Seconds = 0;
	std::vector<fs::path> Paths;
	for (int i = 1; i < argc; i++) {
		const char *Argument = argv[i];
		if (Option(Argument, "--mutations=", Mutations) || Option(Argument, "--seed=", Seed)
			|| Option(Argument, "--max-len=", MaxLength) || Option(Argument, "--seconds=", Seconds))
			continue;
		if (Argument[0] == '-') {
			std::fprintf(stderr, "%s: unknown option %s\n", argv[0], Argument);
			return 2;
		}
		// Absolute: a target's LLVMFuzzerInitialize may change the working directory (EnterWorkFolder).
		std::error_code Error;
		Paths.push_back(fs::absolute(Argument, Error));
	}
	std::error_code Error;
	CrashFile = (fs::current_path(Error) / ("fuzz-crash-" + fs::path(argv[0]).stem().string() + ".bin")).string();
	std::signal(SIGABRT, SaveCrash);
	LLVMFuzzerInitialize(&argc, &argv);
	std::vector<TBytes> Inputs;
	for (const fs::path &Path : Paths) {
		if (fs::is_directory(Path, Error)) {
			std::vector<fs::path> Files;
			for (const fs::directory_entry &Entry : fs::directory_iterator(Path, Error))
				if (Entry.is_regular_file(Error))
					Files.push_back(Entry.path());
			std::sort(Files.begin(), Files.end());
			for (const fs::path &File : Files)
				ReadInput(File, Inputs);
		} else if (!ReadInput(Path, Inputs)) {
			std::fprintf(stderr, "%s: cannot read %s\n", argv[0], Path.string().c_str());
			return 2;
		}
	}
	if (Inputs.empty()) {
		std::fprintf(stderr, "%s: no input (a file or a folder of seeds)\n", argv[0]);
		return 2;
	}
	std::size_t Largest = 4096;
	for (const TBytes &Input : Inputs)
		Largest = std::max(Largest, Input.size());
	const std::size_t Limit = MaxLength != 0 ? static_cast<std::size_t>(MaxLength) : Largest;
	for (const TBytes &Input : Inputs)
		Run(Input);
	TRandom Random(Seed);
	const auto Start = std::chrono::steady_clock::now();
	unsigned long long Done = 0;
	for (; Done < Mutations; Done++) {
		if (Seconds != 0 && Done % 64 == 0
			&& std::chrono::steady_clock::now() - Start > std::chrono::seconds(static_cast<long long>(Seconds)))
			break;
		TBytes Data = Inputs[Random.Below(Inputs.size())];
		Mutate(Data, Inputs, Random, Limit);
		Run(Data);
	}
	std::printf("%s: %zu inputs and %llu mutations (seed %llu), no finding\n", fs::path(argv[0]).filename().string().c_str(),
		Inputs.size(), Done, Seed);
	return 0;
}
