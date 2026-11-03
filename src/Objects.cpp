// Objects.cpp: reconstructed from Tibia.exe 7.72 (0x4cfbb0-0x4d7cc9).
//
// The properties of objects, outfits, effects and missiles from Tibia.dat,
// one array<TProperties> each, and their accessors. Functions in line order.
// Where the binary order differs: the entry loader (line 939) sits right
// after GetEntrySpriteNumber, and ExitObjects (line 1023) before
// ReloadObjects (line 1012).
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

// Data, in .bss order.
array<TProperties> *MissileProperties; // 0x719d8c
array<TProperties> *EffectProperties;  // 0x719d90
array<TProperties> *OutfitProperties;  // 0x719d94
array<TProperties> *ObjectProperties;  // 0x719d98
uint32 DatSignature;                   // 0x719d9c
TObject NoObject;                      // 0x719da0, owner inferred from the address

// Inlined everywhere: "GetEntryFlag(Entry,BANK)". Flag%8 and Flag/8 are
// signed divisions in the binary.
static inline bool GetEntryFlag(TProperties &Entry, int Flag)
{
	return (Entry.Flags[Flag / 8] & (1 << (Flag % 8))) != 0;
}

// --- Entry accessors -------------------------------------------------------

// 0x4cfbb0, lines 67-70
static int GetEntryWaypoints(TProperties &Entry)
{
	TBEGIN
	TASSERT1(67, GetEntryFlag(Entry,BANK), *(int*)Entry.Flags);
	return Entry.Waypoints;
	TEND(70)
}

// 0x4cfe20, lines 91-94
static int GetEntryBrightness(TProperties &Entry)
{
	TBEGIN
	TASSERT(91, GetEntryFlag(Entry,LIGHT));
	return Entry.Brightness;
	TEND(94)
}

// 0x4d0090, lines 103-106
static int GetEntryLightColor(TProperties &Entry)
{
	TBEGIN
	TASSERT(103, GetEntryFlag(Entry,LIGHT));
	return Entry.LightColor;
	TEND(106)
}

// 0x4d0300, lines 115-118
static int GetEntryShiftX(TProperties &Entry)
{
	TBEGIN
	TASSERT(115, GetEntryFlag(Entry,SHIFT));
	return Entry.ShiftX;
	TEND(118)
}

// 0x4d0570, lines 127-130
static int GetEntryShiftY(TProperties &Entry)
{
	TBEGIN
	TASSERT(127, GetEntryFlag(Entry,SHIFT));
	return Entry.ShiftY;
	TEND(130)
}

// 0x4d07e0, lines 139-142
static int GetEntryElevation(TProperties &Entry)
{
	TBEGIN
	TASSERT(139, GetEntryFlag(Entry,HEIGHT));
	return Entry.Elevation;
	TEND(142)
}

// 0x4d0a50, lines 151-154
static int GetEntryAutomapColor(TProperties &Entry)
{
	TBEGIN
	TASSERT(151, GetEntryFlag(Entry,AUTOMAP));
	return Entry.AutomapColor;
	TEND(154)
}

// 0x4d0cc0, lines 163-166
static int GetEntryLensHelp(TProperties &Entry)
{
	TBEGIN
	TASSERT(163, GetEntryFlag(Entry,LENSHELP));
	return Entry.LensHelp;
	TEND(166)
}

// 0x4d0f30, lines 238-244. No try block. The binary inlines In() for the
// first five checks and calls its out-of-line copy (0x4174d0) for the last
// two.
static int GetEntrySpriteNumber(TProperties &Entry, int X, int Y, int Layer, int PatternX, int PatternY,
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

// --- Object types ------------------------------------------------------------

// 0x4d1c40, lines 267-270
bool GetObjectFlag(int Type, int Flag)
{
	TBEGIN
	if (Type == CREATURE)
		return Flag == UNPASS;
	TASSERT2(267, In(Type,ObjectProperties->low(),ObjectProperties->high()), Type, ObjectProperties->high());
	return GetEntryFlag((*ObjectProperties)(Type), Flag);
	TEND2(270, Type, Flag)
}

// 0x4d1f30, line 281
int GetObjectWaypoints(int Type)
{
	TBEGIN
	return GetEntryWaypoints((*ObjectProperties)(Type));
	TEND1(281, Type)
}

// 0x4d20d0, line 303
int GetObjectBrightness(int Type)
{
	TBEGIN
	return GetEntryBrightness((*ObjectProperties)(Type));
	TEND1(303, Type)
}

// 0x4d2270, line 314
int GetObjectLightColor(int Type)
{
	TBEGIN
	return GetEntryLightColor((*ObjectProperties)(Type));
	TEND1(314, Type)
}

// 0x4d2410, line 325
int GetObjectShiftX(int Type)
{
	TBEGIN
	return GetEntryShiftX((*ObjectProperties)(Type));
	TEND1(325, Type)
}

// 0x4d25b0, line 336
int GetObjectShiftY(int Type)
{
	TBEGIN
	return GetEntryShiftY((*ObjectProperties)(Type));
	TEND1(336, Type)
}

// 0x4d2750, line 347
int GetObjectElevation(int Type)
{
	TBEGIN
	return GetEntryElevation((*ObjectProperties)(Type));
	TEND1(347, Type)
}

// 0x4d28f0, line 358
int GetObjectAutomapColor(int Type)
{
	TBEGIN
	return GetEntryAutomapColor((*ObjectProperties)(Type));
	TEND1(358, Type)
}

// 0x4d2a90, line 369
int GetObjectLensHelp(int Type)
{
	TBEGIN
	return GetEntryLensHelp((*ObjectProperties)(Type));
	TEND1(369, Type)
}

// 0x4d2c30, line 380
int GetObjectWidth(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).Width;
	TEND1(380, Type)
}

// 0x4d2dd0, line 391
int GetObjectHeight(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).Height;
	TEND1(391, Type)
}

// 0x4d2f70, line 402
int GetObjectExactSize(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).ExactSize;
	TEND1(402, Type)
}

// 0x4d3110, line 413
int GetObjectLayers(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).Layers;
	TEND1(413, Type)
}

// 0x4d32b0, line 424
int GetObjectPatternWidth(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).PatternWidth;
	TEND1(424, Type)
}

// 0x4d3450, line 435
int GetObjectPatternHeight(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).PatternHeight;
	TEND1(435, Type)
}

// 0x4d35f0, line 446
int GetObjectPatternDepth(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).PatternDepth;
	TEND1(446, Type)
}

// 0x4d3790, line 457
int GetObjectPhases(int Type)
{
	TBEGIN
	return (*ObjectProperties)(Type).Phases;
	TEND1(457, Type)
}

// 0x4d3930, line 468
int GetObjectSpriteNumber(int Type, int X, int Y, int Layer, int PatternX, int PatternY, int PatternZ,
	int Phase)
{
	TBEGIN
	return GetEntrySpriteNumber((*ObjectProperties)(Type), X, Y, Layer, PatternX, PatternY, PatternZ,
		Phase);
	TEND1(468, Type)
}

// --- Outfits -----------------------------------------------------------------

// 0x4d3af0, line 484. Unlike GetObjectFlag, no range check.
bool GetOutfitFlag(int Number, int Flag)
{
	TBEGIN
	return GetEntryFlag((*OutfitProperties)(Number), Flag);
	TEND2(484, Number, Flag)
}

// 0x4d3cc0, line 495
int GetOutfitBrightness(int Number)
{
	TBEGIN
	return GetEntryBrightness((*OutfitProperties)(Number));
	TEND1(495, Number)
}

// 0x4d3e60, line 506
int GetOutfitLightColor(int Number)
{
	TBEGIN
	return GetEntryLightColor((*OutfitProperties)(Number));
	TEND1(506, Number)
}

// 0x4d4000, line 517
int GetOutfitShiftX(int Number)
{
	TBEGIN
	return GetEntryShiftX((*OutfitProperties)(Number));
	TEND1(517, Number)
}

// 0x4d41a0, line 528
int GetOutfitShiftY(int Number)
{
	TBEGIN
	return GetEntryShiftY((*OutfitProperties)(Number));
	TEND1(528, Number)
}

// 0x4d4340, line 539
int GetOutfitWidth(int Number)
{
	TBEGIN
	return (*OutfitProperties)(Number).Width;
	TEND1(539, Number)
}

// 0x4d44e0, line 550. Comes before the height, unlike the object accessors.
int GetOutfitExactSize(int Number)
{
	TBEGIN
	return (*OutfitProperties)(Number).ExactSize;
	TEND1(550, Number)
}

// 0x4d4680, line 561
int GetOutfitHeight(int Number)
{
	TBEGIN
	return (*OutfitProperties)(Number).Height;
	TEND1(561, Number)
}

// 0x4d4820, line 572
int GetOutfitLayers(int Number)
{
	TBEGIN
	return (*OutfitProperties)(Number).Layers;
	TEND1(572, Number)
}

// 0x4d49c0, line 583
int GetOutfitPhases(int Number)
{
	TBEGIN
	return (*OutfitProperties)(Number).Phases;
	TEND1(583, Number)
}

// 0x4d4b60, line 594
int GetOutfitSpriteNumber(int Number, int X, int Y, int Layer, int Direction, int Phase)
{
	TBEGIN
	return GetEntrySpriteNumber((*OutfitProperties)(Number), X, Y, Layer, Direction + 1, 1, 1, Phase);
	TEND1(594, Number)
}

// --- Effects -----------------------------------------------------------------

// 0x4d4d20, line 610
bool GetEffectFlag(int Number, int Flag)
{
	TBEGIN
	return GetEntryFlag((*EffectProperties)(Number), Flag);
	TEND2(610, Number, Flag)
}

// 0x4d4ef0, line 621
int GetEffectBrightness(int Number)
{
	TBEGIN
	return GetEntryBrightness((*EffectProperties)(Number));
	TEND1(621, Number)
}

// 0x4d5090, line 632
int GetEffectLightColor(int Number)
{
	TBEGIN
	return GetEntryLightColor((*EffectProperties)(Number));
	TEND1(632, Number)
}

// 0x4d5230, line 643
int GetEffectWidth(int Number)
{
	TBEGIN
	return (*EffectProperties)(Number).Width;
	TEND1(643, Number)
}

// 0x4d53d0, line 654
int GetEffectHeight(int Number)
{
	TBEGIN
	return (*EffectProperties)(Number).Height;
	TEND1(654, Number)
}

// 0x4d5570, line 665
int GetEffectLayers(int Number)
{
	TBEGIN
	return (*EffectProperties)(Number).Layers;
	TEND1(665, Number)
}

// 0x4d5710, line 676
int GetEffectPhases(int Number)
{
	TBEGIN
	return (*EffectProperties)(Number).Phases;
	TEND1(676, Number)
}

// 0x4d58b0, line 687
int GetEffectSpriteNumber(int Number, int X, int Y, int Layer, int Phase)
{
	TBEGIN
	return GetEntrySpriteNumber((*EffectProperties)(Number), X, Y, Layer, 1, 1, 1, Phase);
	TEND1(687, Number)
}

// --- Missiles ----------------------------------------------------------------

// 0x4d5a70, line 703
bool GetMissileFlag(int Number, int Flag)
{
	TBEGIN
	return GetEntryFlag((*MissileProperties)(Number), Flag);
	TEND2(703, Number, Flag)
}

// 0x4d5c40, line 714
int GetMissileBrightness(int Number)
{
	TBEGIN
	return GetEntryBrightness((*MissileProperties)(Number));
	TEND1(714, Number)
}

// 0x4d5de0, line 725
int GetMissileLightColor(int Number)
{
	TBEGIN
	return GetEntryLightColor((*MissileProperties)(Number));
	TEND1(725, Number)
}

// 0x4d5f80, line 736
int GetMissileWidth(int Number)
{
	TBEGIN
	return (*MissileProperties)(Number).Width;
	TEND1(736, Number)
}

// 0x4d6120, line 747
int GetMissileHeight(int Number)
{
	TBEGIN
	return (*MissileProperties)(Number).Height;
	TEND1(747, Number)
}

// 0x4d62c0, line 758
int GetMissileLayers(int Number)
{
	TBEGIN
	return (*MissileProperties)(Number).Layers;
	TEND1(758, Number)
}

// 0x4d6460, lines 766-780. The assertion comes before the try block. The
// 3x3 pattern holds the eight directions around its centre, clockwise from
// north (PatternX 2, PatternY 1). The default case cannot be reached; the
// binary returns 0 there.
int GetMissileSpriteNumber(int Number, int X, int Y, int Layer, int Direction)
{
	TASSERT1(766, In(Direction,0,7), Direction);
	TBEGIN
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
	TEND2(780, Number, Direction)
}

// --- Patterns ----------------------------------------------------------------

// 0x4d6800, lines 796-810. Obj.Data is the count: 1, 2, 3, 4, then up to
// 9, 24, 49 and more in the second row. Both results wrap around the
// object's pattern size.
void GetCumulativePattern(TObject &Obj, int *PatternX, int *PatternY)
{
	TBEGIN
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
	TEND(810)
}

// 0x4d6b20, lines 821-826. Obj.Data is the liquid: four columns per row.
void GetLiquidPattern(TObject &Obj, int *PatternX, int *PatternY)
{
	TBEGIN
	TASSERT(821, GetObjectFlag(Obj,LIQUIDPOOL) || GetObjectFlag(Obj,LIQUIDCONTAINER));
	*PatternX = Obj.Data % 4 % GetObjectPatternWidth(Obj.Type) + 1;
	*PatternY = Obj.Data / 4 % GetObjectPatternHeight(Obj.Type) + 1;
	TEND(826)
}

// 0x4d6df0, lines 837-853. Obj.Data2 holds the wall flag the object hangs
// on (HOOKSOUTH or HOOKEAST); the second and third columns are used only
// when the object has them.
void GetHangPattern(TObject &Obj, int *PatternX, int *PatternY)
{
	TBEGIN
	TASSERT(837, GetObjectFlag(Obj,HANG));
	if (Obj.Data2 == HOOKSOUTH)
		*PatternX = (GetObjectPatternWidth(Obj.Type) >= 2) ? 2 : 1;
	else if (Obj.Data2 == HOOKEAST)
		*PatternX = (GetObjectPatternWidth(Obj.Type) >= 3) ? 3 : 1;
	else
		*PatternX = 1;
	*PatternY = 1;
	TEND(853)
}

// --- Loading -------------------------------------------------------------------

// 0x4d70c0, line 888. Frees the four arrays and their sprite tables.
void ReleaseObjects()
{
	TBEGIN
	if (ObjectProperties != nullptr) {
		for (int i = ObjectProperties->low(); i <= ObjectProperties->high(); i++)
			delete[] (*ObjectProperties)(i).Sprites;
		delete ObjectProperties;
		ObjectProperties = nullptr;
	}
	if (OutfitProperties != nullptr) {
		for (int i = OutfitProperties->low(); i <= OutfitProperties->high(); i++)
			delete[] (*OutfitProperties)(i).Sprites;
		delete OutfitProperties;
		OutfitProperties = nullptr;
	}
	if (EffectProperties != nullptr) {
		for (int i = EffectProperties->low(); i <= EffectProperties->high(); i++)
			delete[] (*EffectProperties)(i).Sprites;
		delete EffectProperties;
		EffectProperties = nullptr;
	}
	if (MissileProperties != nullptr) {
		for (int i = MissileProperties->low(); i <= MissileProperties->high(); i++)
			delete[] (*MissileProperties)(i).Sprites;
		delete MissileProperties;
		MissileProperties = nullptr;
	}
	TEND(888)
}

// 0x4d1650, line 939. Reads one entry: flags with their values up to 0xff,
// then the sizes and the sprite numbers. Unknown flags are only recorded;
// a flag above 31 (not in Tibia.dat) sets a bit in the fields after Flags,
// up to offset 0x43, as in the binary. The binary stores Waypoints when the
// BANK case jumps back into the zeroing code, which ends with that store.
static void LoadProperties(TReadBinaryFile *File, TProperties *Entry)
{
	TBEGIN
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
	TEND(939)
}

// 0x4d7380, lines 948-978. The assertion comes before the try block.
// Header: signature, then the highest object type (types start at 100) and
// the numbers of outfits, effects and missiles. The file is closed twice:
// by close() and by the destructor.
void LoadObjects(const char *FileName)
{
	TASSERT(948, FileName!=NULL);
	TBEGIN
	ReleaseObjects();
	TReadBinaryFile File;
	File.open(FileName);
	DatSignature = File.readQuad();
	const int Objects = File.readWord();
	const int Outfits = File.readWord();
	const int Effects = File.readWord();
	const int Missiles = File.readWord();
	ObjectProperties = new array<TProperties>(100, Objects);
	OutfitProperties = new array<TProperties>(1, Outfits);
	EffectProperties = new array<TProperties>(1, Effects);
	MissileProperties = new array<TProperties>(1, Missiles);
	for (int i = 100; i <= Objects; i++)
		LoadProperties(&File, &(*ObjectProperties)(i));
	for (int i = 1; i <= Outfits; i++)
		LoadProperties(&File, &(*OutfitProperties)(i));
	for (int i = 1; i <= Effects; i++)
		LoadProperties(&File, &(*EffectProperties)(i));
	for (int i = 1; i <= Missiles; i++)
		LoadProperties(&File, &(*MissileProperties)(i));
	File.close();
	TEND(978)
}

// 0x4d77f0, line 1000. A TMyException while loading becomes PROGRAM_ERROR 1
// (name of the value unknown); InitControl then shuts down. The binary's
// handler has a catch object, so the original named the unused reference;
// it is left unnamed here to avoid warning C4101.
void InitObjects()
{
	TBEGIN
	ObjectProperties = nullptr;
	OutfitProperties = nullptr;
	EffectProperties = nullptr;
	MissileProperties = nullptr;
	try {
		LoadObjects("Tibia.dat");
	} catch (TMyException &) {
		throw (PROGRAM_ERROR)1;
	}
	TEND(1000)
}

// 0x4d7b40, line 1012. After Tibia.dat was patched (event 0x45).
void ReloadObjects()
{
	TBEGIN
	ExitObjects();
	InitObjects();
	TEND(1012)
}

// 0x4d79b0, line 1023
void ExitObjects()
{
	TBEGIN
	ReleaseObjects();
	TEND(1023)
}
