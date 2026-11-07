// fuzz_map: the automap files (Map.cpp's LoadAutomapBlock and SaveAutomapBlock; Patella-Modern issue #132), on a TMap
// of its own: the input is the block file 00000000.map (block 0, 0, floor 0), which GetAutomapMarks and
// GetAutomapColor load through the block cache.
//
// The file is 256 x 256 colour bytes, as many speed bytes, then the marks (a count, then x, y, type and a text each;
// notes/written-files.md). The input's first byte: bit 0 set, the rest of the input is the file as it is; clear, the
// harness puts the two layers (128 KiB, a pattern) before the rest, so the fuzzer's bytes are the marks. Bit 1: a mark
// is added and the block saved (ExitMap, SaveAutomapBlock), and the file it wrote loaded again by a new cache.
// A damaged file is the original's "Map file '...' was corrupted!" in the default console (here a fake), no error.

#include "client.h"

#include "Map.h"
#include "Objects.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

const char *const Target = "fuzz_map";

class TConsole final : public patella::TMapConsole {
public:
	bool HasDefaultConsole() override { return true; }
	void AddDefaultConsoleLine(const char *) override { Lines++; }
	int Lines = 0;
};

void NoCreature(int) {}

std::unique_ptr<patella::TObjectTypes> ObjectTypes;
std::unique_ptr<TConsole> Console;
std::unique_ptr<patella::TMap> Map;

// Loads block 0, 0, 0 through the cache (InitMap empties it) and reads its marks and colours.
void Load()
{
	Map->InitMap(NoCreature);
	std::vector<TMark> Marks; // #100: GetAutomapMarks fills a std::vector
	int Count = 0;
	Map->GetAutomapMarks(0, 0, 0, &Marks, &Count);
	static_cast<void>(Map->GetAutomapColor(0, 0, 0));
	static_cast<void>(Map->GetAutomapColor(255, 255, 0));
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	patella::fuzz::EnterWorkFolder("map");
	ObjectTypes = std::make_unique<patella::TObjectTypes>();
	Console = std::make_unique<TConsole>();
	Map = std::make_unique<patella::TMap>(*ObjectTypes, *Console);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	patella::fuzz::TInput Input(Data, Size);
	const std::uint8_t Mode = Input.Byte();
	std::string File;
	if ((Mode & 1) == 0) {
		File.resize(2 * 256 * 256);
		for (std::size_t i = 0; i < File.size(); i++)
			File[i] = static_cast<char>(i * 31 >> 8);
	}
	File += Input.Rest();
	patella::fuzz::WriteFile("00000000.map", File);
	patella::fuzz::Guard(Target, Load);
	// The block's mark count as the file has it (LoadAutomapBlock keeps a negative one: it reads no mark then). A mark
	// added to a block of a negative count goes to that index: TIndexedVector grows down to it, 10 entries at a time
	// (the original's, filed by #132), which for a count far below 0 takes as long as it needs memory. Not added there.
	int Count = 0;
	if (File.size() >= 2 * 256 * 256 + 4)
		std::memcpy(&Count, File.data() + 2 * 256 * 256, sizeof(Count));
	if ((Mode & 2) != 0 && Count >= -64) {
		patella::fuzz::Guard(Target, [] {
			TCoordinate Coordinate{1, 1, 0};
			Map->AddAutomapMark(Coordinate, "fuzz", 1);
			Map->ExitMap();
		});
		patella::fuzz::Guard(Target, Load);
	}
	return 0;
}
