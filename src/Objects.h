// Objects.h: the object, outfit, effect and missile properties read from
// Tibia.dat, and their accessors.
//
// Names from the binary: ObjectProperties, the Entry fields in the
// assertions (Flags, Width, Height, Layers, PatternWidth, PatternHeight,
// PatternDepth, Phases), the flags BANK, CUMULATIVE, LIQUIDCONTAINER,
// LIQUIDPOOL, HANG, LIGHT, SHIFT, HEIGHT, AUTOMAP and LENSHELP, CREATURE
// (MapWindow.cpp), and the parameter names Entry, Type, Number, Flag,
// Direction, Obj, X, Y, Layer, PatternX/Y/Z, Phase and FileName. The rest is
// ours (notes/decisions.md, "Objects.cpp").

#pragma once

#include "common.h"
#include "Container.h"
#include <cstddef>
#include <memory>
#include <type_traits>

// A reference to an object (12 bytes, passed by value). Type is named in
// MapWindow.cpp's "SrcObj.Type!=CREATURE" (CREATURE is 99), Data in
// Dialogs.cpp's "In(DraggedObject.Data,1,100)" (the count); Data2 is ours.
// The default constructor clears the object: Map.cpp's TMapPoint, ten
// TObjects and plain ints, has a compiler-generated constructor (0x4b0b40)
// that zeroes exactly the ten objects. Written with default member
// initializers so that the type stays an aggregate. MapWindow.cpp compares
// objects with numbers ("SrcObj!=0", "Obj!=CREATURE") and prints one as the
// int value of Obj in its catch wrapper: the original converts a TObject to its Type.
struct TObject {
	int Type = 0;
	int Data = 0;
	int Data2 = 0;
	operator int() const { return Type; }
};

// Object type of a creature on a map field ("SrcObj.Type!=CREATURE").
inline constexpr int CREATURE = 99;

// Tibia.dat flags: the bit numbers in TProperties::Flags. The names of
// 1-20 follow CipSoft's game server (medium confidence); 22, 23, 26 and 27
// are ours (low).
enum : int {  // NOLINT(cppcoreguidelines-use-enum-class): int constants ("The other ten stay unscoped"), the flags' bit numbers
	BANK = 0, // ground; reads Waypoints
	CLIP = 1,
	BOTTOM = 2,
	TOP = 3,
	CONTAINER = 4,
	CUMULATIVE = 5, // stackable; the count chooses the pattern
	FORCEUSE = 6,
	MULTIUSE = 7,
	WRITE = 8,     // reads MaxLength
	WRITEONCE = 9, // reads MaxLength
	LIQUIDCONTAINER = 10,
	LIQUIDPOOL = 11,
	UNPASS = 12, // also true for creatures
	UNMOVE = 13,
	UNTHROW = 14,
	AVOID = 15,
	TAKE = 16,
	HANG = 17,
	HOOKSOUTH = 18,
	HOOKEAST = 19,
	ROTATE = 20,
	LIGHT = 21, // reads Brightness and LightColor
	DONTHIDE = 22,
	TRANSLUCENT = 23,
	SHIFT = 24,  // reads ShiftX and ShiftY
	HEIGHT = 25, // reads Elevation
	LYINGOBJECT = 26,
	ANIMATEALWAYS = 27,
	AUTOMAP = 28,  // reads AutomapColor
	LENSHELP = 29, // reads LensHelp
	FULLBANK = 30, // ours: a ground that covers the whole field; MapWindow.cpp hides the levels below
};
static_assert(sizeof(decltype(BANK)) == 4);
static_assert(std::is_same_v<std::underlying_type_t<decltype(BANK)>, int>);

// One Tibia.dat entry (0x4c bytes); the same layout serves objects, outfits,
// effects and missiles.
struct TProperties {
	int Width;         // +0x00
	int Height;        // +0x04
	int ExactSize;     // +0x08, 32 unless Width or Height is above 1
	int Layers;        // +0x0c
	int PatternWidth;  // +0x10
	int PatternHeight; // +0x14
	int PatternDepth;  // +0x18
	int Phases;        // +0x1c
	uint16 *Sprites;   // +0x20, Width*Height*Layers*PatternWidth*PatternHeight*PatternDepth*Phases
	uint8 Flags[4];    // +0x24, bit Flag%8 of Flags[Flag/8]; NOLINT(modernize-avoid-c-arrays): the assertion of line 67 reads *(int*)Entry.Flags, frozen until M5 (issue #60)
	int Waypoints;     // +0x28, BANK
	int MaxLength;     // +0x2c, WRITE and WRITEONCE
	int Brightness;    // +0x30, LIGHT
	int LightColor;    // +0x34, LIGHT
	int ShiftX;        // +0x38, SHIFT
	int ShiftY;        // +0x3c, SHIFT
	int Elevation;     // +0x40, HEIGHT
	int AutomapColor;  // +0x44, AUTOMAP
	int LensHelp;      // +0x48, LENSHELP
};

// The byte of Flag's bit, Flags[Flag / 8], for every flag up to 0xff as the original indexes it: a flag above 31 is a
// bit of the members after Flags (inside TProperties, up to +0x43; on x64 +0x47). Through the entry's bytes
// (Patella-Modern issue #91): Flags indexed past its four elements is undefined behaviour, which UBSan's bounds check
// reports.
inline uint8 &FlagByte(TProperties &Entry, int Flag)
{
	static_assert(offsetof(TProperties, Flags) + 0xff / 8 < sizeof(TProperties), "every flag's byte is in TProperties");
	return reinterpret_cast<uint8 *>(&Entry)[offsetof(TProperties, Flags) + static_cast<std::size_t>(Flag / 8)];
}
inline uint8 FlagByte(const TProperties &Entry, int Flag)
{
	return reinterpret_cast<const uint8 *>(&Entry)[offsetof(TProperties, Flags) + static_cast<std::size_t>(Flag / 8)];
}

namespace patella {

// Objects.cpp's data and functions (Patella-Modern issue #106, M7; notes/decisions.md, "#104: TClient and the
// subsystem objects" and "#106"): Tibia.dat's four tables, read once, then read-only, and the functions of the same
// names as members. TMapState owns it (MapState.h), first: TMap, TCreatures and TInventory read it. The data
// (0x719d8c-0x719dab, .bss: every member starts empty) is public: the golden Objects, Divisions, Errors, Graphics, Map
// and MapWindow tests read the tables, the Objects test NoObject and DatSignature, the Communication world sets both;
// Communication.cpp sends DatSignature and compares with NoObject, Inventory.cpp and Map.cpp return NoObject.
class TObjectTypes {
public:
	TObjectTypes() = default;
	TObjectTypes(const TObjectTypes &) = delete;
	TObjectTypes &operator=(const TObjectTypes &) = delete;
	TObjectTypes(TObjectTypes &&) = delete;
	TObjectTypes &operator=(TObjectTypes &&) = delete;
	~TObjectTypes() = default;

	// Object types. Type CREATURE has only UNPASS.
	[[nodiscard]] bool GetObjectFlag(int Type, int Flag) const;
	[[nodiscard]] bool GetObjectFlag(const TObject &Obj, int Flag) const { return GetObjectFlag(Obj.Type, Flag); }
	[[nodiscard]] int GetObjectWaypoints(int Type) const;
	[[nodiscard]] int GetObjectBrightness(int Type) const;
	[[nodiscard]] int GetObjectLightColor(int Type) const;
	[[nodiscard]] int GetObjectShiftX(int Type) const;
	[[nodiscard]] int GetObjectShiftY(int Type) const;
	[[nodiscard]] int GetObjectElevation(int Type) const;
	[[nodiscard]] int GetObjectAutomapColor(int Type) const;
	[[nodiscard]] int GetObjectLensHelp(int Type) const;
	[[nodiscard]] int GetObjectWidth(int Type) const;
	[[nodiscard]] int GetObjectHeight(int Type) const;
	[[nodiscard]] int GetObjectExactSize(int Type) const;
	[[nodiscard]] int GetObjectLayers(int Type) const;
	[[nodiscard]] int GetObjectPatternWidth(int Type) const;
	[[nodiscard]] int GetObjectPatternHeight(int Type) const;
	[[nodiscard]] int GetObjectPatternDepth(int Type) const;
	[[nodiscard]] int GetObjectPhases(int Type) const;
	[[nodiscard]] int GetObjectSpriteNumber(int Type, int X, int Y, int Layer, int PatternX, int PatternY, int PatternZ,
		int Phase) const;

	// Outfits; Direction 0-3 selects the pattern column.
	[[nodiscard]] bool GetOutfitFlag(int Number, int Flag) const;
	[[nodiscard]] int GetOutfitBrightness(int Number) const;
	[[nodiscard]] int GetOutfitLightColor(int Number) const;
	[[nodiscard]] int GetOutfitShiftX(int Number) const;
	[[nodiscard]] int GetOutfitShiftY(int Number) const;
	[[nodiscard]] int GetOutfitWidth(int Number) const;
	[[nodiscard]] int GetOutfitExactSize(int Number) const;
	[[nodiscard]] int GetOutfitHeight(int Number) const;
	[[nodiscard]] int GetOutfitLayers(int Number) const;
	[[nodiscard]] int GetOutfitPhases(int Number) const;
	[[nodiscard]] int GetOutfitSpriteNumber(int Number, int X, int Y, int Layer, int Direction, int Phase) const;

	// Effects.
	[[nodiscard]] bool GetEffectFlag(int Number, int Flag) const;
	[[nodiscard]] int GetEffectBrightness(int Number) const;
	[[nodiscard]] int GetEffectLightColor(int Number) const;
	[[nodiscard]] int GetEffectWidth(int Number) const;
	[[nodiscard]] int GetEffectHeight(int Number) const;
	[[nodiscard]] int GetEffectLayers(int Number) const;
	[[nodiscard]] int GetEffectPhases(int Number) const;
	[[nodiscard]] int GetEffectSpriteNumber(int Number, int X, int Y, int Layer, int Phase) const;

	// Missiles; Direction 0-7 clockwise from north.
	[[nodiscard]] bool GetMissileFlag(int Number, int Flag) const;
	[[nodiscard]] int GetMissileBrightness(int Number) const;
	[[nodiscard]] int GetMissileLightColor(int Number) const;
	[[nodiscard]] int GetMissileWidth(int Number) const;
	[[nodiscard]] int GetMissileHeight(int Number) const;
	[[nodiscard]] int GetMissileLayers(int Number) const;
	[[nodiscard]] int GetMissileSpriteNumber(int Number, int X, int Y, int Layer, int Direction) const;

	// The pattern (1-based) that shows an object's count, liquid or wall side.
	void GetCumulativePattern(TObject &Obj, int *PatternX, int *PatternY) const;
	void GetLiquidPattern(TObject &Obj, int *PatternX, int *PatternY) const;
	void GetHangPattern(TObject &Obj, int *PatternX, int *PatternY) const;

	void ReleaseObjects();
	void LoadObjects(const char *FileName);
	void InitObjects();
	void ReloadObjects();
	void ExitObjects();

	// Data, in .bss order. The four tables own their arrays (std::unique_ptr since issue #58; the entries' Sprites
	// stay raw, freed by ReleaseObjects).
	std::unique_ptr<array<TProperties>> MissileProperties; // 0x719d8c
	std::unique_ptr<array<TProperties>> EffectProperties;  // 0x719d90
	std::unique_ptr<array<TProperties>> OutfitProperties;  // 0x719d94
	std::unique_ptr<array<TProperties>> ObjectProperties;  // 0x719d98, from 100
	uint32 DatSignature{};                                 // 0x719d9c, sent at login and when patching
	TObject NoObject;                                      // 0x719da0, never written; owner inferred from the address
};

// The transition's access path (issue #106; notes/decisions.md, "#104: TClient and the subsystem objects"): the object
// types of the one TClient (Client.cpp). It goes when no free function needs it.
namespace bridge {
TObjectTypes &ObjectTypes();
} // namespace bridge

} // namespace patella

// The original's functions (the callers' names, and the golden tests'): each calls TObjectTypes' member of the same
// name on bridge::ObjectTypes().
// Object types. Type CREATURE has only UNPASS.
bool GetObjectFlag(int Type, int Flag);
inline bool GetObjectFlag(TObject &Obj, int Flag) { return GetObjectFlag(Obj.Type, Flag); }
int GetObjectWaypoints(int Type);
int GetObjectBrightness(int Type);
int GetObjectLightColor(int Type);
int GetObjectShiftX(int Type);
int GetObjectShiftY(int Type);
int GetObjectElevation(int Type);
int GetObjectAutomapColor(int Type);
int GetObjectLensHelp(int Type);
int GetObjectWidth(int Type);
int GetObjectHeight(int Type);
int GetObjectExactSize(int Type);
int GetObjectLayers(int Type);
int GetObjectPatternWidth(int Type);
int GetObjectPatternHeight(int Type);
int GetObjectPatternDepth(int Type);
int GetObjectPhases(int Type);
int GetObjectSpriteNumber(int Type, int X, int Y, int Layer, int PatternX, int PatternY, int PatternZ,
	int Phase);

// Outfits; Direction 0-3 selects the pattern column.
bool GetOutfitFlag(int Number, int Flag);
int GetOutfitBrightness(int Number);
int GetOutfitLightColor(int Number);
int GetOutfitShiftX(int Number);
int GetOutfitShiftY(int Number);
int GetOutfitWidth(int Number);
int GetOutfitExactSize(int Number);
int GetOutfitHeight(int Number);
int GetOutfitLayers(int Number);
int GetOutfitPhases(int Number);
int GetOutfitSpriteNumber(int Number, int X, int Y, int Layer, int Direction, int Phase);

// Effects.
bool GetEffectFlag(int Number, int Flag);
int GetEffectBrightness(int Number);
int GetEffectLightColor(int Number);
int GetEffectWidth(int Number);
int GetEffectHeight(int Number);
int GetEffectLayers(int Number);
int GetEffectPhases(int Number);
int GetEffectSpriteNumber(int Number, int X, int Y, int Layer, int Phase);

// Missiles; Direction 0-7 clockwise from north.
bool GetMissileFlag(int Number, int Flag);
int GetMissileBrightness(int Number);
int GetMissileLightColor(int Number);
int GetMissileWidth(int Number);
int GetMissileHeight(int Number);
int GetMissileLayers(int Number);
int GetMissileSpriteNumber(int Number, int X, int Y, int Layer, int Direction);

// The pattern (1-based) that shows an object's count, liquid or wall side.
void GetCumulativePattern(TObject &Obj, int *PatternX, int *PatternY);
void GetLiquidPattern(TObject &Obj, int *PatternX, int *PatternY);
void GetHangPattern(TObject &Obj, int *PatternX, int *PatternY);

void ReleaseObjects();
void LoadObjects(const char *FileName);
void InitObjects();
void ReloadObjects();
void ExitObjects();
