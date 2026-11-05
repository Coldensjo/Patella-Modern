// Objects.cpp: reconstructed from Tibia.exe 7.72 (0x4cfbb0-0x4d7cc9).
//
// The properties of objects, outfits, effects and missiles from Tibia.dat,
// one array<TProperties> each, and their accessors. Functions in line order.
// Where the binary order differs: the entry loader (line 939) sits right
// after GetEntrySpriteNumber, and ExitObjects (line 1023) before
// ReloadObjects (line 1012).
//
// The try blocks and catch lines the comments name are the original's (its
// TBEGIN/TEND); since issue #78 the functions here have none.
//
// The array<TProperties> constructor (0x4cfa30) and operator() (0x4d1a10)
// are Container.h code. The gap after this file (0x4d7cd0-0x4d913e) is
// Player.obj: the message map of the application class, its constructor and
// destructor, MFC and STL code that Player.cpp uses (file-ranges.tsv).
//
// The source has unreferenced functions the linker dropped: the line gaps
// 70-91 and 281-303 fit an accessor of MaxLength (+0x2c) at each level, and
// lines 166-238 had room for more.

#include "Objects.h"
#include "Script.h"
#include "Container.h"
#include "common.h"
#include <cstddef>
#include <memory>

// The tables as std::unique_ptr (issue #58): each a pointer, as the original's.
static_assert(sizeof(std::unique_ptr<array<TProperties>>) == sizeof(array<TProperties> *)
		&& alignof(std::unique_ptr<array<TProperties>>) == alignof(array<TProperties> *),
	"std::unique_ptr<array<TProperties>> is a pointer");

// Data, in .bss order.
std::unique_ptr<array<TProperties>> MissileProperties; // 0x719d8c
std::unique_ptr<array<TProperties>> EffectProperties;  // 0x719d90
std::unique_ptr<array<TProperties>> OutfitProperties;  // 0x719d94
std::unique_ptr<array<TProperties>> ObjectProperties;  // 0x719d98
uint32 DatSignature;                                   // 0x719d9c
TObject NoObject;                                      // 0x719da0, owner inferred from the address

namespace {

// Inlined everywhere: "GetEntryFlag(Entry,BANK)". Flag%8 and Flag/8 are
// signed divisions in the binary.
inline bool GetEntryFlag(TProperties &Entry, int Flag)
{
	return (Entry.Flags[Flag / 8] & (1 << (Flag % 8))) != 0;
}

// --- Entry accessors -------------------------------------------------------

// 0x4cfbb0, lines 67-70
int GetEntryWaypoints(TProperties &Entry)
{
	TASSERT1(67, GetEntryFlag(Entry,BANK), *(int*)Entry.Flags);
	return Entry.Waypoints;
}

// 0x4cfe20, lines 91-94
int GetEntryBrightness(TProperties &Entry)
{
	TASSERT(91, GetEntryFlag(Entry,LIGHT));
	return Entry.Brightness;
}

// 0x4d0090, lines 103-106
int GetEntryLightColor(TProperties &Entry)
{
	TASSERT(103, GetEntryFlag(Entry,LIGHT));
	return Entry.LightColor;
}

// 0x4d0300, lines 115-118
int GetEntryShiftX(TProperties &Entry)
{
	TASSERT(115, GetEntryFlag(Entry,SHIFT));
	return Entry.ShiftX;
}

// 0x4d0570, lines 127-130
int GetEntryShiftY(TProperties &Entry)
{
	TASSERT(127, GetEntryFlag(Entry,SHIFT));
	return Entry.ShiftY;
}

// 0x4d07e0, lines 139-142
int GetEntryElevation(TProperties &Entry)
{
	TASSERT(139, GetEntryFlag(Entry,HEIGHT));
	return Entry.Elevation;
}

// 0x4d0a50, lines 151-154
int GetEntryAutomapColor(TProperties &Entry)
{
	TASSERT(151, GetEntryFlag(Entry,AUTOMAP));
	return Entry.AutomapColor;
}

// 0x4d0cc0, lines 163-166
int GetEntryLensHelp(TProperties &Entry)
{
	TASSERT(163, GetEntryFlag(Entry,LENSHELP));
	return Entry.LensHelp;
}

// 0x4d0f30, lines 238-244. No try block. The binary inlines In() for the
// first five checks and calls its out-of-line copy (0x4174d0) for the last
// two.
int GetEntrySpriteNumber(TProperties &Entry, int X, int Y, int Layer, int PatternX, int PatternY,
	int PatternZ, int Phase)
{
	TASSERT2(238, In(X,1,Entry.Width), X, Entry.Width);
	TASSERT2(239, In(Y,1,Entry.Height), Y, Entry.Height);
	TASSERT2(240, In(Layer,1,Entry.Layers), Layer, Entry.Layers);
	TASSERT2(241, In(PatternX,1,Entry.PatternWidth), PatternX, Entry.PatternWidth);
	TASSERT2(242, In(PatternY,1,Entry.PatternHeight), PatternY, Entry.PatternHeight);
	TASSERT2(243, In(PatternZ,1,Entry.PatternDepth), PatternZ, Entry.PatternDepth);
	TASSERT2(244, In(Phase,1,Entry.Phases), Phase, Entry.Phases);
	return Entry.Sprites[((((((Phase - 1) * Entry.PatternDepth + PatternZ - 1) * Entry.PatternHeight
								+ PatternY - 1)
								   * Entry.PatternWidth
							   + PatternX - 1)
								  * Entry.Layers
							  + Layer - 1)
								 * Entry.Height
							 + Y - 1)
			* Entry.Width
		+ X - 1];
}

} // namespace

// --- Object types ------------------------------------------------------------

// 0x4d1c40, lines 267-270
bool GetObjectFlag(int Type, int Flag)
{
	if (Type == CREATURE)
		return Flag == UNPASS;
	TASSERT2(267, In(Type,ObjectProperties->low(),ObjectProperties->high()), Type, ObjectProperties->high());
	return GetEntryFlag((*ObjectProperties)(Type), Flag);
}

// 0x4d1f30, line 281
int GetObjectWaypoints(int Type)
{
	return GetEntryWaypoints((*ObjectProperties)(Type));
}

// 0x4d20d0, line 303
int GetObjectBrightness(int Type)
{
	return GetEntryBrightness((*ObjectProperties)(Type));
}

// 0x4d2270, line 314
int GetObjectLightColor(int Type)
{
	return GetEntryLightColor((*ObjectProperties)(Type));
}

// 0x4d2410, line 325
int GetObjectShiftX(int Type)
{
	return GetEntryShiftX((*ObjectProperties)(Type));
}

// 0x4d25b0, line 336
int GetObjectShiftY(int Type)
{
	return GetEntryShiftY((*ObjectProperties)(Type));
}

// 0x4d2750, line 347
int GetObjectElevation(int Type)
{
	return GetEntryElevation((*ObjectProperties)(Type));
}

// 0x4d28f0, line 358
int GetObjectAutomapColor(int Type)
{
	return GetEntryAutomapColor((*ObjectProperties)(Type));
}

// 0x4d2a90, line 369
int GetObjectLensHelp(int Type)
{
	return GetEntryLensHelp((*ObjectProperties)(Type));
}

// 0x4d2c30, line 380
int GetObjectWidth(int Type)
{
	return (*ObjectProperties)(Type).Width;
}

// 0x4d2dd0, line 391
int GetObjectHeight(int Type)
{
	return (*ObjectProperties)(Type).Height;
}

// 0x4d2f70, line 402
int GetObjectExactSize(int Type)
{
	return (*ObjectProperties)(Type).ExactSize;
}

// 0x4d3110, line 413
int GetObjectLayers(int Type)
{
	return (*ObjectProperties)(Type).Layers;
}

// 0x4d32b0, line 424
int GetObjectPatternWidth(int Type)
{
	return (*ObjectProperties)(Type).PatternWidth;
}

// 0x4d3450, line 435
int GetObjectPatternHeight(int Type)
{
	return (*ObjectProperties)(Type).PatternHeight;
}

// 0x4d35f0, line 446
int GetObjectPatternDepth(int Type)
{
	return (*ObjectProperties)(Type).PatternDepth;
}

// 0x4d3790, line 457
int GetObjectPhases(int Type)
{
	return (*ObjectProperties)(Type).Phases;
}

// 0x4d3930, line 468
int GetObjectSpriteNumber(int Type, int X, int Y, int Layer, int PatternX, int PatternY, int PatternZ,
	int Phase)
{
	return GetEntrySpriteNumber((*ObjectProperties)(Type), X, Y, Layer, PatternX, PatternY, PatternZ,
		Phase);
}

// --- Outfits -----------------------------------------------------------------

// 0x4d3af0, line 484. Unlike GetObjectFlag, no range check.
bool GetOutfitFlag(int Number, int Flag)
{
	return GetEntryFlag((*OutfitProperties)(Number), Flag);
}

// 0x4d3cc0, line 495
int GetOutfitBrightness(int Number)
{
	return GetEntryBrightness((*OutfitProperties)(Number));
}

// 0x4d3e60, line 506
int GetOutfitLightColor(int Number)
{
	return GetEntryLightColor((*OutfitProperties)(Number));
}

// 0x4d4000, line 517
int GetOutfitShiftX(int Number)
{
	return GetEntryShiftX((*OutfitProperties)(Number));
}

// 0x4d41a0, line 528
int GetOutfitShiftY(int Number)
{
	return GetEntryShiftY((*OutfitProperties)(Number));
}

// 0x4d4340, line 539
int GetOutfitWidth(int Number)
{
	return (*OutfitProperties)(Number).Width;
}

// 0x4d44e0, line 550. Comes before the height, unlike the object accessors.
int GetOutfitExactSize(int Number)
{
	return (*OutfitProperties)(Number).ExactSize;
}

// 0x4d4680, line 561
int GetOutfitHeight(int Number)
{
	return (*OutfitProperties)(Number).Height;
}

// 0x4d4820, line 572
int GetOutfitLayers(int Number)
{
	return (*OutfitProperties)(Number).Layers;
}

// 0x4d49c0, line 583
int GetOutfitPhases(int Number)
{
	return (*OutfitProperties)(Number).Phases;
}

// 0x4d4b60, line 594
int GetOutfitSpriteNumber(int Number, int X, int Y, int Layer, int Direction, int Phase)
{
	return GetEntrySpriteNumber((*OutfitProperties)(Number), X, Y, Layer, Direction + 1, 1, 1, Phase);
}

// --- Effects -----------------------------------------------------------------

// 0x4d4d20, line 610
bool GetEffectFlag(int Number, int Flag)
{
	return GetEntryFlag((*EffectProperties)(Number), Flag);
}

// 0x4d4ef0, line 621
int GetEffectBrightness(int Number)
{
	return GetEntryBrightness((*EffectProperties)(Number));
}

// 0x4d5090, line 632
int GetEffectLightColor(int Number)
{
	return GetEntryLightColor((*EffectProperties)(Number));
}

// 0x4d5230, line 643
int GetEffectWidth(int Number)
{
	return (*EffectProperties)(Number).Width;
}

// 0x4d53d0, line 654
int GetEffectHeight(int Number)
{
	return (*EffectProperties)(Number).Height;
}

// 0x4d5570, line 665
int GetEffectLayers(int Number)
{
	return (*EffectProperties)(Number).Layers;
}

// 0x4d5710, line 676
int GetEffectPhases(int Number)
{
	return (*EffectProperties)(Number).Phases;
}

// 0x4d58b0, line 687
int GetEffectSpriteNumber(int Number, int X, int Y, int Layer, int Phase)
{
	return GetEntrySpriteNumber((*EffectProperties)(Number), X, Y, Layer, 1, 1, 1, Phase);
}

// --- Missiles ----------------------------------------------------------------

// 0x4d5a70, line 703
bool GetMissileFlag(int Number, int Flag)
{
	return GetEntryFlag((*MissileProperties)(Number), Flag);
}

// 0x4d5c40, line 714
int GetMissileBrightness(int Number)
{
	return GetEntryBrightness((*MissileProperties)(Number));
}

// 0x4d5de0, line 725
int GetMissileLightColor(int Number)
{
	return GetEntryLightColor((*MissileProperties)(Number));
}

// 0x4d5f80, line 736
int GetMissileWidth(int Number)
{
	return (*MissileProperties)(Number).Width;
}

// 0x4d6120, line 747
int GetMissileHeight(int Number)
{
	return (*MissileProperties)(Number).Height;
}

// 0x4d62c0, line 758
int GetMissileLayers(int Number)
{
	return (*MissileProperties)(Number).Layers;
}

// 0x4d6460, lines 766-780. The assertion comes before the try block. The
// 3x3 pattern holds the eight directions around its centre, clockwise from
// north (PatternX 2, PatternY 1). The default case cannot be reached; the
// binary returns 0 there.
int GetMissileSpriteNumber(int Number, int X, int Y, int Layer, int Direction)
{
	TASSERT1(766, In(Direction,0,7), Direction);
	switch (Direction) {
	case 0:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 2, 1, 1, 1);
	case 1:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 3, 1, 1, 1);
	case 2:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 3, 2, 1, 1);
	case 3:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 3, 3, 1, 1);
	case 4:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 2, 3, 1, 1);
	case 5:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 1, 3, 1, 1);
	case 6:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 1, 2, 1, 1);
	case 7:
		return GetEntrySpriteNumber((*MissileProperties)(Number), X, Y, Layer, 1, 1, 1, 1);
	}
	return 0;
}

// --- Patterns ----------------------------------------------------------------

// 0x4d6800, lines 796-810. Obj.Data is the count: 1, 2, 3, 4, then up to
// 9, 24, 49 and more in the second row. Both results wrap around the
// object's pattern size.
void GetCumulativePattern(TObject &Obj, int *PatternX, int *PatternY)
{
	TASSERT(796, GetObjectFlag(Obj,CUMULATIVE));
	const int Count = Obj.Data;
	if (Count < 2) {
		*PatternX = 0;
		*PatternY = 0;
	} else if (Count == 2) {
		*PatternX = 1;
		*PatternY = 0;
	} else if (Count == 3) {
		*PatternX = 2;
		*PatternY = 0;
	} else if (Count == 4) {
		*PatternX = 3;
		*PatternY = 0;
	} else {
		if (Count < 10)
			*PatternX = 0;
		else if (Count < 25)
			*PatternX = 1;
		else if (Count < 50)
			*PatternX = 2;
		else
			*PatternX = 3;
		*PatternY = 1;
	}
	*PatternX = *PatternX % GetObjectPatternWidth(Obj.Type) + 1;
	*PatternY = *PatternY % GetObjectPatternHeight(Obj.Type) + 1;
}

// 0x4d6b20, lines 821-826. Obj.Data is the liquid: four columns per row.
void GetLiquidPattern(TObject &Obj, int *PatternX, int *PatternY)
{
	TASSERT(821, GetObjectFlag(Obj,LIQUIDPOOL) || GetObjectFlag(Obj,LIQUIDCONTAINER));
	*PatternX = Obj.Data % 4 % GetObjectPatternWidth(Obj.Type) + 1;
	*PatternY = Obj.Data / 4 % GetObjectPatternHeight(Obj.Type) + 1;
}

// 0x4d6df0, lines 837-853. Obj.Data2 holds the wall flag the object hangs
// on (HOOKSOUTH or HOOKEAST); the second and third columns are used only
// when the object has them.
void GetHangPattern(TObject &Obj, int *PatternX, int *PatternY)
{
	TASSERT(837, GetObjectFlag(Obj,HANG));
	if (Obj.Data2 == HOOKSOUTH)
		*PatternX = (GetObjectPatternWidth(Obj.Type) >= 2) ? 2 : 1;
	else if (Obj.Data2 == HOOKEAST)
		*PatternX = (GetObjectPatternWidth(Obj.Type) >= 3) ? 3 : 1;
	else
		*PatternX = 1;
	*PatternY = 1;
}

// --- Loading -------------------------------------------------------------------

// 0x4d70c0, line 888. Frees the four arrays and their sprite tables.
void ReleaseObjects()
{
	if (ObjectProperties != nullptr) {
		for (int i = ObjectProperties->low(); i <= ObjectProperties->high(); i++)
			delete[] (*ObjectProperties)(i).Sprites;
		ObjectProperties.reset();
		ObjectProperties = nullptr;
	}
	if (OutfitProperties != nullptr) {
		for (int i = OutfitProperties->low(); i <= OutfitProperties->high(); i++)
			delete[] (*OutfitProperties)(i).Sprites;
		OutfitProperties.reset();
		OutfitProperties = nullptr;
	}
	if (EffectProperties != nullptr) {
		for (int i = EffectProperties->low(); i <= EffectProperties->high(); i++)
			delete[] (*EffectProperties)(i).Sprites;
		EffectProperties.reset();
		EffectProperties = nullptr;
	}
	if (MissileProperties != nullptr) {
		for (int i = MissileProperties->low(); i <= MissileProperties->high(); i++)
			delete[] (*MissileProperties)(i).Sprites;
		MissileProperties.reset();
		MissileProperties = nullptr;
	}
}

namespace {

// 0x4d1650, line 939. Reads one entry: flags with their values up to 0xff,
// then the sizes and the sprite numbers. Unknown flags are only recorded;
// a flag above 31 (not in Tibia.dat) sets a bit in the fields after Flags,
// up to offset 0x43, as in the binary. The binary stores Waypoints when the
// BANK case jumps back into the zeroing code, which ends with that store.
void LoadProperties(TReadBinaryFile *File, TProperties *Entry)
{
	for (int i = 0; i < 4; i++)
		Entry->Flags[i] = 0;
	Entry->Waypoints = 0;
	Entry->MaxLength = 0;
	Entry->Brightness = 0;
	Entry->LightColor = 0;
	Entry->ShiftX = 0;
	Entry->ShiftY = 0;
	Entry->Elevation = 0;
	Entry->AutomapColor = 0;
	Entry->LensHelp = 0;
	while (true) {
		const int Flag = File->readByte();
		if (Flag == 0xff)
			break;
		Entry->Flags[Flag / 8] |= (uint8)(1 << (Flag % 8));
		switch (Flag) {
		case BANK:
			Entry->Waypoints = File->readWord();
			break;
		case WRITE:
			Entry->MaxLength = File->readWord();
			break;
		case WRITEONCE:
			Entry->MaxLength = File->readWord();
			break;
		case LIGHT:
			Entry->Brightness = File->readWord();
			Entry->LightColor = File->readWord();
			break;
		case SHIFT:
			Entry->ShiftX = File->readWord();
			Entry->ShiftY = File->readWord();
			break;
		case HEIGHT:
			Entry->Elevation = File->readWord();
			break;
		case AUTOMAP:
			Entry->AutomapColor = File->readWord();
			break;
		case LENSHELP:
			Entry->LensHelp = File->readWord();
			break;
		}
	}
	Entry->Width = File->readByte();
	Entry->Height = File->readByte();
	if (Entry->Width > 1 || Entry->Height > 1)
		Entry->ExactSize = File->readByte();
	else
		Entry->ExactSize = 32;
	Entry->Layers = File->readByte();
	Entry->PatternWidth = File->readByte();
	Entry->PatternHeight = File->readByte();
	Entry->PatternDepth = File->readByte();
	Entry->Phases = File->readByte();
	const int Count = Entry->Width * Entry->Height * Entry->Layers * Entry->PatternWidth * Entry->PatternHeight
		* Entry->PatternDepth * Entry->Phases;
	Entry->Sprites = new uint16[Count];
	for (int i = 0; i < Count; i++)
		Entry->Sprites[i] = File->readWord();
}

} // namespace

// 0x4d7380, lines 948-978. The assertion comes before the try block.
// Header: signature, then the highest object type (types start at 100) and
// the numbers of outfits, effects and missiles. The file is closed twice:
// by close() and by the destructor.
void LoadObjects(const char *FileName)
{
	TASSERT(948, FileName!=NULL);
	ReleaseObjects();
	TReadBinaryFile File;
	File.open(FileName);
	DatSignature = File.readQuad();
	const int Objects = File.readWord();
	const int Outfits = File.readWord();
	const int Effects = File.readWord();
	const int Missiles = File.readWord();
	ObjectProperties.reset(new array<TProperties>(100, Objects));
	OutfitProperties.reset(new array<TProperties>(1, Outfits));
	EffectProperties.reset(new array<TProperties>(1, Effects));
	MissileProperties.reset(new array<TProperties>(1, Missiles));
	for (int i = 100; i <= Objects; i++)
		LoadProperties(&File, &(*ObjectProperties)(i));
	for (int i = 1; i <= Outfits; i++)
		LoadProperties(&File, &(*OutfitProperties)(i));
	for (int i = 1; i <= Effects; i++)
		LoadProperties(&File, &(*EffectProperties)(i));
	for (int i = 1; i <= Missiles; i++)
		LoadProperties(&File, &(*MissileProperties)(i));
	File.close();
}

// 0x4d77f0, line 1000. A TMyException while loading becomes PROGRAM_ERROR 1
// (name of the value unknown); InitControl then shuts down. The binary's
// handler has a catch object, so the original named the unused reference;
// it is left unnamed here to avoid warning C4101.
void InitObjects()
{
	// Cleared, not freed, as the original does (issue #58: release, not reset).
	static_cast<void>(ObjectProperties.release());  // NOLINT(bugprone-unused-return-value): the leak is the original's
	static_cast<void>(OutfitProperties.release());  // NOLINT(bugprone-unused-return-value): the leak is the original's
	static_cast<void>(EffectProperties.release());  // NOLINT(bugprone-unused-return-value): the leak is the original's
	static_cast<void>(MissileProperties.release()); // NOLINT(bugprone-unused-return-value): the leak is the original's
	try {
		LoadObjects("Tibia.dat");
	} catch (TMyException &) {
		throw (PROGRAM_ERROR)1;
	}
}

// 0x4d7b40, line 1012. After Tibia.dat was patched (event 0x45).
void ReloadObjects()
{
	ExitObjects();
	InitObjects();
}

// 0x4d79b0, line 1023
void ExitObjects()
{
	ReleaseObjects();
}
