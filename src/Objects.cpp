// Objects.cpp: reconstructed from Tibia.exe 7.72 (0x4cfbb0-0x4d7cc9).
//
// The properties of objects, outfits, effects and missiles from Tibia.dat,
// one array<TProperties> each, and their accessors. Functions in line order.
// Where the binary order differs: the entry loader (line 939) sits right
// after GetEntrySpriteNumber, and ExitObjects (line 1023) before
// ReloadObjects (line 1012).
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none.
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
#include <memory>

// The tables as std::unique_ptr (issue #58): each a pointer, as the original's.
static_assert(sizeof(std::unique_ptr<array<TProperties>>) == sizeof(array<TProperties> *)
		&& alignof(std::unique_ptr<array<TProperties>>) == alignof(array<TProperties> *),
	"std::unique_ptr<array<TProperties>> is a pointer");

// The data (0x719d8c-0x719dab) is TObjectTypes' (Objects.h, issue #106).

namespace {

// Inlined everywhere: "GetEntryFlag(Entry,BANK)". Flag%8 and Flag/8 are
// signed divisions in the binary.
inline bool GetEntryFlag(TProperties &Entry, int Flag)
{
	return (FlagByte(Entry, Flag) & (1 << (Flag % 8))) != 0;
}

// --- Entry accessors -------------------------------------------------------

// 0x4cfbb0, lines 67-70
int GetEntryWaypoints(TProperties &Entry)
{
	PATELLA_ASSERT_VALUES(GetEntryFlag(Entry,BANK), *reinterpret_cast<const int*>(Entry.Flags));
	return Entry.Waypoints;
}

// 0x4cfe20, lines 91-94
int GetEntryBrightness(TProperties &Entry)
{
	PATELLA_ASSERT(GetEntryFlag(Entry,LIGHT));
	return Entry.Brightness;
}

// 0x4d0090, lines 103-106
int GetEntryLightColor(TProperties &Entry)
{
	PATELLA_ASSERT(GetEntryFlag(Entry,LIGHT));
	return Entry.LightColor;
}

// 0x4d0300, lines 115-118
int GetEntryShiftX(TProperties &Entry)
{
	PATELLA_ASSERT(GetEntryFlag(Entry,SHIFT));
	return Entry.ShiftX;
}

// 0x4d0570, lines 127-130
int GetEntryShiftY(TProperties &Entry)
{
	PATELLA_ASSERT(GetEntryFlag(Entry,SHIFT));
	return Entry.ShiftY;
}

// 0x4d07e0, lines 139-142
int GetEntryElevation(TProperties &Entry)
{
	PATELLA_ASSERT(GetEntryFlag(Entry,HEIGHT));
	return Entry.Elevation;
}

// 0x4d0a50, lines 151-154
int GetEntryAutomapColor(TProperties &Entry)
{
	PATELLA_ASSERT(GetEntryFlag(Entry,AUTOMAP));
	return Entry.AutomapColor;
}

// 0x4d0cc0, lines 163-166
int GetEntryLensHelp(TProperties &Entry)
{
	PATELLA_ASSERT(GetEntryFlag(Entry,LENSHELP));
	return Entry.LensHelp;
}

// 0x4d0f30, lines 238-244. No try block. The binary inlines In() for the
// first five checks and calls its out-of-line copy (0x4174d0) for the last
// two.
int GetEntrySpriteNumber(TProperties &Entry, int X, int Y, int Layer, int PatternX, int PatternY,
	int PatternZ, int Phase)
{
	PATELLA_ASSERT_VALUES(In(X,1,Entry.Width), X, Entry.Width);
	PATELLA_ASSERT_VALUES(In(Y,1,Entry.Height), Y, Entry.Height);
	PATELLA_ASSERT_VALUES(In(Layer,1,Entry.Layers), Layer, Entry.Layers);
	PATELLA_ASSERT_VALUES(In(PatternX,1,Entry.PatternWidth), PatternX, Entry.PatternWidth);
	PATELLA_ASSERT_VALUES(In(PatternY,1,Entry.PatternHeight), PatternY, Entry.PatternHeight);
	PATELLA_ASSERT_VALUES(In(PatternZ,1,Entry.PatternDepth), PatternZ, Entry.PatternDepth);
	PATELLA_ASSERT_VALUES(In(Phase,1,Entry.Phases), Phase, Entry.Phases);
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
bool patella::TObjectTypes::GetObjectFlag(int Type, int Flag) const
{
	if (Type == CREATURE)
		return Flag == UNPASS;
	PATELLA_ASSERT_VALUES(In(Type,ObjectProperties->low(),ObjectProperties->high()), Type, ObjectProperties->high());
	return GetEntryFlag((*ObjectProperties)(Type), Flag);
}

// 0x4d1f30, line 281
int patella::TObjectTypes::GetObjectWaypoints(int Type) const
{
	return GetEntryWaypoints((*ObjectProperties)(Type));
}

// 0x4d20d0, line 303
int patella::TObjectTypes::GetObjectBrightness(int Type) const
{
	return GetEntryBrightness((*ObjectProperties)(Type));
}

// 0x4d2270, line 314
int patella::TObjectTypes::GetObjectLightColor(int Type) const
{
	return GetEntryLightColor((*ObjectProperties)(Type));
}

// 0x4d2410, line 325
int patella::TObjectTypes::GetObjectShiftX(int Type) const
{
	return GetEntryShiftX((*ObjectProperties)(Type));
}

// 0x4d25b0, line 336
int patella::TObjectTypes::GetObjectShiftY(int Type) const
{
	return GetEntryShiftY((*ObjectProperties)(Type));
}

// 0x4d2750, line 347
int patella::TObjectTypes::GetObjectElevation(int Type) const
{
	return GetEntryElevation((*ObjectProperties)(Type));
}

// 0x4d28f0, line 358
int patella::TObjectTypes::GetObjectAutomapColor(int Type) const
{
	return GetEntryAutomapColor((*ObjectProperties)(Type));
}

// 0x4d2a90, line 369
int patella::TObjectTypes::GetObjectLensHelp(int Type) const
{
	return GetEntryLensHelp((*ObjectProperties)(Type));
}

// 0x4d2c30, line 380
int patella::TObjectTypes::GetObjectWidth(int Type) const
{
	return (*ObjectProperties)(Type).Width;
}

// 0x4d2dd0, line 391
int patella::TObjectTypes::GetObjectHeight(int Type) const
{
	return (*ObjectProperties)(Type).Height;
}

// 0x4d2f70, line 402
int patella::TObjectTypes::GetObjectExactSize(int Type) const
{
	return (*ObjectProperties)(Type).ExactSize;
}

// 0x4d3110, line 413
int patella::TObjectTypes::GetObjectLayers(int Type) const
{
	return (*ObjectProperties)(Type).Layers;
}

// 0x4d32b0, line 424
int patella::TObjectTypes::GetObjectPatternWidth(int Type) const
{
	return (*ObjectProperties)(Type).PatternWidth;
}

// 0x4d3450, line 435
int patella::TObjectTypes::GetObjectPatternHeight(int Type) const
{
	return (*ObjectProperties)(Type).PatternHeight;
}

// 0x4d35f0, line 446
int patella::TObjectTypes::GetObjectPatternDepth(int Type) const
{
	return (*ObjectProperties)(Type).PatternDepth;
}

// 0x4d3790, line 457
int patella::TObjectTypes::GetObjectPhases(int Type) const
{
	return (*ObjectProperties)(Type).Phases;
}

// 0x4d3930, line 468
int patella::TObjectTypes::GetObjectSpriteNumber(int Type, int X, int Y, int Layer, int PatternX, int PatternY, int PatternZ,
	int Phase) const
{
	return GetEntrySpriteNumber((*ObjectProperties)(Type), X, Y, Layer, PatternX, PatternY, PatternZ,
		Phase);
}

// --- Outfits -----------------------------------------------------------------

// 0x4d3af0, line 484. Unlike GetObjectFlag, no range check.
bool patella::TObjectTypes::GetOutfitFlag(int Number, int Flag) const
{
	return GetEntryFlag((*OutfitProperties)(Number), Flag);
}

// 0x4d3cc0, line 495
int patella::TObjectTypes::GetOutfitBrightness(int Number) const
{
	return GetEntryBrightness((*OutfitProperties)(Number));
}

// 0x4d3e60, line 506
int patella::TObjectTypes::GetOutfitLightColor(int Number) const
{
	return GetEntryLightColor((*OutfitProperties)(Number));
}

// 0x4d4000, line 517
int patella::TObjectTypes::GetOutfitShiftX(int Number) const
{
	return GetEntryShiftX((*OutfitProperties)(Number));
}

// 0x4d41a0, line 528
int patella::TObjectTypes::GetOutfitShiftY(int Number) const
{
	return GetEntryShiftY((*OutfitProperties)(Number));
}

// 0x4d4340, line 539
int patella::TObjectTypes::GetOutfitWidth(int Number) const
{
	return (*OutfitProperties)(Number).Width;
}

// 0x4d44e0, line 550. Comes before the height, unlike the object accessors.
int patella::TObjectTypes::GetOutfitExactSize(int Number) const
{
	return (*OutfitProperties)(Number).ExactSize;
}

// 0x4d4680, line 561
int patella::TObjectTypes::GetOutfitHeight(int Number) const
{
	return (*OutfitProperties)(Number).Height;
}

// 0x4d4820, line 572
int patella::TObjectTypes::GetOutfitLayers(int Number) const
{
	return (*OutfitProperties)(Number).Layers;
}

// 0x4d49c0, line 583
int patella::TObjectTypes::GetOutfitPhases(int Number) const
{
	return (*OutfitProperties)(Number).Phases;
}

// 0x4d4b60, line 594
int patella::TObjectTypes::GetOutfitSpriteNumber(int Number, int X, int Y, int Layer, int Direction, int Phase) const
{
	return GetEntrySpriteNumber((*OutfitProperties)(Number), X, Y, Layer, Direction + 1, 1, 1, Phase);
}

// --- Effects -----------------------------------------------------------------

// 0x4d4d20, line 610
bool patella::TObjectTypes::GetEffectFlag(int Number, int Flag) const
{
	return GetEntryFlag((*EffectProperties)(Number), Flag);
}

// 0x4d4ef0, line 621
int patella::TObjectTypes::GetEffectBrightness(int Number) const
{
	return GetEntryBrightness((*EffectProperties)(Number));
}

// 0x4d5090, line 632
int patella::TObjectTypes::GetEffectLightColor(int Number) const
{
	return GetEntryLightColor((*EffectProperties)(Number));
}

// 0x4d5230, line 643
int patella::TObjectTypes::GetEffectWidth(int Number) const
{
	return (*EffectProperties)(Number).Width;
}

// 0x4d53d0, line 654
int patella::TObjectTypes::GetEffectHeight(int Number) const
{
	return (*EffectProperties)(Number).Height;
}

// 0x4d5570, line 665
int patella::TObjectTypes::GetEffectLayers(int Number) const
{
	return (*EffectProperties)(Number).Layers;
}

// 0x4d5710, line 676
int patella::TObjectTypes::GetEffectPhases(int Number) const
{
	return (*EffectProperties)(Number).Phases;
}

// 0x4d58b0, line 687
int patella::TObjectTypes::GetEffectSpriteNumber(int Number, int X, int Y, int Layer, int Phase) const
{
	return GetEntrySpriteNumber((*EffectProperties)(Number), X, Y, Layer, 1, 1, 1, Phase);
}

// --- Missiles ----------------------------------------------------------------

// 0x4d5a70, line 703
bool patella::TObjectTypes::GetMissileFlag(int Number, int Flag) const
{
	return GetEntryFlag((*MissileProperties)(Number), Flag);
}

// 0x4d5c40, line 714
int patella::TObjectTypes::GetMissileBrightness(int Number) const
{
	return GetEntryBrightness((*MissileProperties)(Number));
}

// 0x4d5de0, line 725
int patella::TObjectTypes::GetMissileLightColor(int Number) const
{
	return GetEntryLightColor((*MissileProperties)(Number));
}

// 0x4d5f80, line 736
int patella::TObjectTypes::GetMissileWidth(int Number) const
{
	return (*MissileProperties)(Number).Width;
}

// 0x4d6120, line 747
int patella::TObjectTypes::GetMissileHeight(int Number) const
{
	return (*MissileProperties)(Number).Height;
}

// 0x4d62c0, line 758
int patella::TObjectTypes::GetMissileLayers(int Number) const
{
	return (*MissileProperties)(Number).Layers;
}

// 0x4d6460, lines 766-780. The assertion comes before the try block. The
// 3x3 pattern holds the eight directions around its centre, clockwise from
// north (PatternX 2, PatternY 1). The default case cannot be reached; the
// binary returns 0 there.
int patella::TObjectTypes::GetMissileSpriteNumber(int Number, int X, int Y, int Layer, int Direction) const
{
	PATELLA_ASSERT_VALUES(In(Direction,0,7), Direction);
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
	default: // the 0 below, as without it (bugprone-switch-missing-default-case, issue #110)
		break;
	}
	return 0;
}

// --- Patterns ----------------------------------------------------------------

// 0x4d6800, lines 796-810. Obj.Data is the count: 1, 2, 3, 4, then up to
// 9, 24, 49 and more in the second row. Both results wrap around the
// object's pattern size.
// A pattern size of 0 in Tibia.dat throws a TMyException (PATELLA_DIVISOR, issue #67).
void patella::TObjectTypes::GetCumulativePattern(TObject &Obj, int *PatternX, int *PatternY) const
{
	PATELLA_ASSERT(GetObjectFlag(Obj,CUMULATIVE));
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
	*PatternX = *PatternX % PATELLA_DIVISOR(GetObjectPatternWidth(Obj.Type)) + 1;
	*PatternY = *PatternY % PATELLA_DIVISOR(GetObjectPatternHeight(Obj.Type)) + 1;
}

// 0x4d6b20, lines 821-826. Obj.Data is the liquid: four columns per row.
// A pattern size of 0 in Tibia.dat throws a TMyException (PATELLA_DIVISOR, issue #67).
void patella::TObjectTypes::GetLiquidPattern(TObject &Obj, int *PatternX, int *PatternY) const
{
	PATELLA_ASSERT(GetObjectFlag(Obj,LIQUIDPOOL) || GetObjectFlag(Obj,LIQUIDCONTAINER));
	*PatternX = Obj.Data % 4 % PATELLA_DIVISOR(GetObjectPatternWidth(Obj.Type)) + 1;
	*PatternY = Obj.Data / 4 % PATELLA_DIVISOR(GetObjectPatternHeight(Obj.Type)) + 1;
}

// 0x4d6df0, lines 837-853. Obj.Data2 holds the wall flag the object hangs
// on (HOOKSOUTH or HOOKEAST); the second and third columns are used only
// when the object has them.
void patella::TObjectTypes::GetHangPattern(TObject &Obj, int *PatternX, int *PatternY) const
{
	PATELLA_ASSERT(GetObjectFlag(Obj,HANG));
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
void patella::TObjectTypes::ReleaseObjects()
{
	if (ObjectProperties != nullptr) {
		for (int i = ObjectProperties->low(); i <= ObjectProperties->high(); i++)
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): LoadProperties' sprite table
			delete[] (*ObjectProperties)(i).Sprites;
		ObjectProperties.reset();
		ObjectProperties = nullptr;
	}
	if (OutfitProperties != nullptr) {
		for (int i = OutfitProperties->low(); i <= OutfitProperties->high(); i++)
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): LoadProperties' sprite table
			delete[] (*OutfitProperties)(i).Sprites;
		OutfitProperties.reset();
		OutfitProperties = nullptr;
	}
	if (EffectProperties != nullptr) {
		for (int i = EffectProperties->low(); i <= EffectProperties->high(); i++)
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): LoadProperties' sprite table
			delete[] (*EffectProperties)(i).Sprites;
		EffectProperties.reset();
		EffectProperties = nullptr;
	}
	if (MissileProperties != nullptr) {
		for (int i = MissileProperties->low(); i <= MissileProperties->high(); i++)
			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): LoadProperties' sprite table
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
		FlagByte(*Entry, Flag) |= (uint8)(1 << (Flag % 8));
		switch (Flag) {
		case BANK:
			Entry->Waypoints = File->readWord();
			break;
		case WRITE: // and WRITEONCE: the same read (two identical cases until issue #110, bugprone-branch-clone)
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
		default: // nothing, as without it (bugprone-switch-missing-default-case, issue #110)
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
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): the entry's sprite table, deleted by ReleaseObjects (notes/ownership.md)
	Entry->Sprites = new uint16[Count];
	for (int i = 0; i < Count; i++)
		Entry->Sprites[i] = File->readWord();
}

} // namespace

// 0x4d7380, lines 948-978. The assertion comes before the try block.
// Header: signature, then the highest object type (types start at 100) and
// the numbers of outfits, effects and missiles. The file is closed twice:
// by close() and by the destructor.
void patella::TObjectTypes::LoadObjects(const char *FileName)
{
	PATELLA_ASSERT(FileName!=nullptr);
	ReleaseObjects();
	TReadBinaryFile File;
	File.open(FileName);
	DatSignature = File.readQuad();
	const int Objects = File.readWord();
	const int Outfits = File.readWord();
	const int Effects = File.readWord();
	const int Missiles = File.readWord();
	// Patella-Modern issue #139, item 1, a deliberate difference (notes/decisions.md, "#139"): every entry's sprite
	// table starts empty, each table's as soon as it is allocated, so ReleaseObjects, after a file that ends before
	// the last entry (or a count of 0, which the next table's constructor refuses), deletes only the tables
	// LoadProperties allocated. The binary left the entries' Sprites indeterminate and deleted them.
	const auto ClearSprites = [](array<TProperties> &Table) {
		for (int i = Table.low(); i <= Table.high(); i++)
			Table(i).Sprites = nullptr;
	};
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): unique_ptr::reset, a template, is not among its owners
	ObjectProperties.reset(new array<TProperties>(100, Objects));
	ClearSprites(*ObjectProperties);
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): unique_ptr::reset, a template, is not among its owners
	OutfitProperties.reset(new array<TProperties>(1, Outfits));
	ClearSprites(*OutfitProperties);
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): unique_ptr::reset, a template, is not among its owners
	EffectProperties.reset(new array<TProperties>(1, Effects));
	ClearSprites(*EffectProperties);
	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): unique_ptr::reset, a template, is not among its owners
	MissileProperties.reset(new array<TProperties>(1, Missiles));
	ClearSprites(*MissileProperties);
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

// 0x4d77f0, line 1000. An exception while loading becomes PROGRAM_ERROR 1
// (name of the value unknown); InitControl then shuts down. The original
// catches a TMyException; its LoadObjects' catch wrapper made every other
// exception one (LoadProperties' new[] of a size from Tibia.dat can throw
// std::bad_array_new_length or std::bad_alloc). That wrapper is gone (issue
// #78), so the boundary catches everything (LoadObjects throws no
// PROGRAM_ERROR): the outcome is the original's.
void patella::TObjectTypes::InitObjects()
{
	// Cleared, not freed, as the original does (issue #58: release, not reset).
	static_cast<void>(ObjectProperties.release());  // NOLINT(bugprone-unused-return-value): the leak is the original's
	static_cast<void>(OutfitProperties.release());  // NOLINT(bugprone-unused-return-value): the leak is the original's
	static_cast<void>(EffectProperties.release());  // NOLINT(bugprone-unused-return-value): the leak is the original's
	static_cast<void>(MissileProperties.release()); // NOLINT(bugprone-unused-return-value): the leak is the original's
	try {
		LoadObjects("Tibia.dat");
	} catch (...) {
		throw TProgramError{PROGRAM_ERROR::OBJECTS_FILE};
	}
}

// 0x4d7b40, line 1012. After Tibia.dat was patched (event 0x45).
void patella::TObjectTypes::ReloadObjects()
{
	ExitObjects();
	InitObjects();
}

// 0x4d79b0, line 1023
void patella::TObjectTypes::ExitObjects()
{
	ReleaseObjects();
}

// The original's functions (issue #106): each calls its member on the one TClient's TObjectTypes.

bool GetObjectFlag(int Type, int Flag)
{
	return patella::bridge::ObjectTypes().GetObjectFlag(Type, Flag);
}

int GetObjectWaypoints(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectWaypoints(Type);
}

int GetObjectBrightness(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectBrightness(Type);
}

int GetObjectLightColor(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectLightColor(Type);
}

int GetObjectShiftX(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectShiftX(Type);
}

int GetObjectShiftY(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectShiftY(Type);
}

int GetObjectElevation(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectElevation(Type);
}

int GetObjectAutomapColor(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectAutomapColor(Type);
}

int GetObjectLensHelp(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectLensHelp(Type);
}

int GetObjectWidth(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectWidth(Type);
}

int GetObjectHeight(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectHeight(Type);
}

int GetObjectExactSize(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectExactSize(Type);
}

int GetObjectLayers(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectLayers(Type);
}

int GetObjectPatternWidth(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectPatternWidth(Type);
}

int GetObjectPatternHeight(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectPatternHeight(Type);
}

int GetObjectPatternDepth(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectPatternDepth(Type);
}

int GetObjectPhases(int Type)
{
	return patella::bridge::ObjectTypes().GetObjectPhases(Type);
}

int GetObjectSpriteNumber(int Type, int X, int Y, int Layer, int PatternX, int PatternY, int PatternZ, int Phase)
{
	return patella::bridge::ObjectTypes().GetObjectSpriteNumber(Type, X, Y, Layer, PatternX, PatternY, PatternZ, Phase);
}

bool GetOutfitFlag(int Number, int Flag)
{
	return patella::bridge::ObjectTypes().GetOutfitFlag(Number, Flag);
}

int GetOutfitBrightness(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitBrightness(Number);
}

int GetOutfitLightColor(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitLightColor(Number);
}

int GetOutfitShiftX(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitShiftX(Number);
}

int GetOutfitShiftY(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitShiftY(Number);
}

int GetOutfitWidth(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitWidth(Number);
}

int GetOutfitExactSize(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitExactSize(Number);
}

int GetOutfitHeight(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitHeight(Number);
}

int GetOutfitLayers(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitLayers(Number);
}

int GetOutfitPhases(int Number)
{
	return patella::bridge::ObjectTypes().GetOutfitPhases(Number);
}

int GetOutfitSpriteNumber(int Number, int X, int Y, int Layer, int Direction, int Phase)
{
	return patella::bridge::ObjectTypes().GetOutfitSpriteNumber(Number, X, Y, Layer, Direction, Phase);
}

bool GetEffectFlag(int Number, int Flag)
{
	return patella::bridge::ObjectTypes().GetEffectFlag(Number, Flag);
}

int GetEffectBrightness(int Number)
{
	return patella::bridge::ObjectTypes().GetEffectBrightness(Number);
}

int GetEffectLightColor(int Number)
{
	return patella::bridge::ObjectTypes().GetEffectLightColor(Number);
}

int GetEffectWidth(int Number)
{
	return patella::bridge::ObjectTypes().GetEffectWidth(Number);
}

int GetEffectHeight(int Number)
{
	return patella::bridge::ObjectTypes().GetEffectHeight(Number);
}

int GetEffectLayers(int Number)
{
	return patella::bridge::ObjectTypes().GetEffectLayers(Number);
}

int GetEffectPhases(int Number)
{
	return patella::bridge::ObjectTypes().GetEffectPhases(Number);
}

int GetEffectSpriteNumber(int Number, int X, int Y, int Layer, int Phase)
{
	return patella::bridge::ObjectTypes().GetEffectSpriteNumber(Number, X, Y, Layer, Phase);
}

bool GetMissileFlag(int Number, int Flag)
{
	return patella::bridge::ObjectTypes().GetMissileFlag(Number, Flag);
}

int GetMissileBrightness(int Number)
{
	return patella::bridge::ObjectTypes().GetMissileBrightness(Number);
}

int GetMissileLightColor(int Number)
{
	return patella::bridge::ObjectTypes().GetMissileLightColor(Number);
}

int GetMissileWidth(int Number)
{
	return patella::bridge::ObjectTypes().GetMissileWidth(Number);
}

int GetMissileHeight(int Number)
{
	return patella::bridge::ObjectTypes().GetMissileHeight(Number);
}

int GetMissileLayers(int Number)
{
	return patella::bridge::ObjectTypes().GetMissileLayers(Number);
}

int GetMissileSpriteNumber(int Number, int X, int Y, int Layer, int Direction)
{
	return patella::bridge::ObjectTypes().GetMissileSpriteNumber(Number, X, Y, Layer, Direction);
}

void GetCumulativePattern(TObject &Obj, int *PatternX, int *PatternY)
{
	patella::bridge::ObjectTypes().GetCumulativePattern(Obj, PatternX, PatternY);
}

void GetLiquidPattern(TObject &Obj, int *PatternX, int *PatternY)
{
	patella::bridge::ObjectTypes().GetLiquidPattern(Obj, PatternX, PatternY);
}

void GetHangPattern(TObject &Obj, int *PatternX, int *PatternY)
{
	patella::bridge::ObjectTypes().GetHangPattern(Obj, PatternX, PatternY);
}

void ReleaseObjects()
{
	patella::bridge::ObjectTypes().ReleaseObjects();
}

void LoadObjects(const char *FileName)
{
	patella::bridge::ObjectTypes().LoadObjects(FileName);
}

void InitObjects()
{
	patella::bridge::ObjectTypes().InitObjects();
}

void ReloadObjects()
{
	patella::bridge::ObjectTypes().ReloadObjects();
}

void ExitObjects()
{
	patella::bridge::ObjectTypes().ExitObjects();
}
