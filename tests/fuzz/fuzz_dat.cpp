// fuzz_dat: Tibia.dat's loader (Objects.cpp TObjectTypes::LoadObjects and LoadProperties; Patella-Modern issue #132),
// on a TObjectTypes of its own: the input is the file. The header (signature, the highest object type, the numbers of
// outfits, effects and missiles), then per type its flags with their values up to 0xff, its sizes and its sprite
// numbers. A file that ends early is a TMyException (InitObjects makes it PROGRAM_ERROR 1). The sprite table's size is
// the product of seven bytes of the file, so its allocation can fail (std::bad_alloc, the same outcome) or be large:
// run libFuzzer with -rss_limit_mb and -malloc_limit_mb above 4096 (tools/fuzz.py does), or such inputs stop it. Under
// AddressSanitizer the files with a table it cannot allocate are left out (below).
// After a load the harness reads the loaded types' properties through the getters the client uses.
//
// A load that fails leaves the tables' entries it did not reach with their Sprites pointer never set (array's new T[d],
// as the original's), and ReleaseObjects (the next load's start, ExitObjects) deletes them: the original's, filed as
// a finding of #132 (notes/decisions.md). The harness reads the file as LoadProperties does (Expected) and frees the
// entries that were reached itself, the others' pointers cleared, before ReleaseObjects.

#include "client.h"

#include "Objects.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace {

const char *const Target = "fuzz_dat";

patella::TObjectTypes *Types = nullptr;

// How far LoadObjects gets in the file: the entries it filled, in its order (objects, outfits, effects, missiles),
// and whether the one after them has its sprite table (the reads failed after its allocation).
struct TExpected {
	int Filled = 0;
	bool PartialTable = false;
	bool Unallocatable = false; // a sprite table of a negative or more than 2^30 entries is reached
};

TExpected Expected(const std::uint8_t *Data, std::size_t Size)
{
	patella::fuzz::TInput Input(Data, Size);
	TExpected Result;
	if (Input.Left() < 12)
		return Result;
	static_cast<void>(Input.Bytes(4));
	const int Objects = Input.Word();
	const int Outfits = Input.Word();
	const int Effects = Input.Word();
	const int Missiles = Input.Word();
	// A table of no entries is array's assertion, before any entry is read.
	if (Objects < 100 || Outfits < 1 || Effects < 1 || Missiles < 1)
		return Result;
	const long long Entries = static_cast<long long>(Objects - 99) + Outfits + Effects + Missiles;
	for (long long Entry = 0; Entry < Entries; Entry++) {
		for (;;) {
			if (Input.Empty())
				return Result;
			const int Flag = Input.Byte();
			if (Flag == 0xff)
				break;
			int Words = 0;
			switch (Flag) {
			case BANK:
			case WRITE:
			case WRITEONCE:
			case HEIGHT:
			case AUTOMAP:
			case LENSHELP: Words = 1; break;
			case LIGHT:
			case SHIFT: Words = 2; break;
			default: break;
			}
			if (Input.Left() < static_cast<std::size_t>(2 * Words))
				return Result;
			static_cast<void>(Input.Bytes(static_cast<std::size_t>(2 * Words)));
		}
		if (Input.Left() < 2)
			return Result;
		const int Width = Input.Byte();
		const int Height = Input.Byte();
		const int Sizes = Width > 1 || Height > 1 ? 6 : 5;
		if (Input.Left() < static_cast<std::size_t>(Sizes))
			return Result;
		if (Sizes == 6)
			static_cast<void>(Input.Byte());
		unsigned int Count = static_cast<unsigned int>(Width) * static_cast<unsigned int>(Height);
		for (int i = 0; i < 5; i++)
			Count *= Input.Byte();
		// LoadProperties' int product (wrapped, -fwrapv); a negative one throws std::bad_array_new_length.
		const int Signed = static_cast<int>(Count);
		if (Signed < 0 || Signed > (1 << 30)) {
			Result.Unallocatable = true;
			return Result;
		}
		if (Input.Left() < 2 * static_cast<std::size_t>(Signed)) {
			Result.PartialTable = true;
			return Result;
		}
		static_cast<void>(Input.Bytes(2 * static_cast<std::size_t>(Signed)));
		Result.Filled++;
	}
	return Result;
}

// The tables as LoadObjects left them after a failure: the entries not reached get a null sprite table.
void ClearUnreached(const TExpected &Reached)
{
	int Entry = 0;
	const auto Clear = [&](std::unique_ptr<array<TProperties>> &Table) {
		if (Table == nullptr)
			return;
		for (int i = Table->low(); i <= Table->high(); i++, Entry++) {
			if (Entry > Reached.Filled || (Entry == Reached.Filled && !Reached.PartialTable))
				(*Table)(i).Sprites = nullptr;
		}
	};
	Clear(Types->ObjectProperties);
	Clear(Types->OutfitProperties);
	Clear(Types->EffectProperties);
	Clear(Types->MissileProperties);
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
	patella::fuzz::EnterWorkFolder("dat");
	static patella::TObjectTypes Instance;
	Types = &Instance;
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size)
{
	// Under AddressSanitizer a new[] of a negative size (std::bad_array_new_length) or of more than its limit (std::bad_alloc)
	// is a report, not the exception the client gets (InitObjects: PROGRAM_ERROR 1): such a file is not loaded there.
	if constexpr (PATELLA_FUZZ_ASAN) {
		if (Expected(Data, Size).Unallocatable)
			return 0;
	}
	patella::fuzz::WriteFile("Tibia.dat", Data, Size);
	bool Loaded = false;
	patella::fuzz::Guard(Target, [&] {
		Types->LoadObjects("Tibia.dat");
		Loaded = true;
	});
	if (Loaded) {
		patella::fuzz::Guard(Target, [] {
			const int Last = Types->ObjectProperties->high();
			for (int Type = 100; Type <= Last && Type < 100 + 256; Type++) {
				for (int Flag = 0; Flag < 32; Flag++)
					static_cast<void>(Types->GetObjectFlag(Type, Flag));
				static_cast<void>(Types->GetObjectWidth(Type));
				static_cast<void>(Types->GetObjectHeight(Type));
			}
		});
	} else {
		ClearUnreached(Expected(Data, Size));
	}
	Types->ReleaseObjects();
	return 0;
}
