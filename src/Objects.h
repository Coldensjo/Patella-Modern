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

// A reference to an object (12 bytes, passed by value). Type is named in
// MapWindow.cpp's "SrcObj.Type!=CREATURE" (CREATURE is 99), Data in
// Dialogs.cpp's "In(DraggedObject.Data,1,100)" (the count); Data2 is ours.
// The default constructor clears the object: Map.cpp's TMapPoint, ten
// TObjects and plain ints, has a compiler-generated constructor (0x4b0b40)
// that zeroes exactly the ten objects. Written with default member
// initializers so that the type stays an aggregate. MapWindow.cpp compares
// objects with numbers ("SrcObj!=0", "Obj!=CREATURE") and prints one as the
// int value of TEND1(Obj): the original converts a TObject to its Type.
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
enum {
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
	uint8 Flags[4];    // +0x24, bit Flag%8 of Flags[Flag/8]
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

extern array<TProperties> *MissileProperties; // 0x719d8c
extern array<TProperties> *EffectProperties;  // 0x719d90
extern array<TProperties> *OutfitProperties;  // 0x719d94
extern array<TProperties> *ObjectProperties;  // 0x719d98, from 100
extern uint32 DatSignature;                   // 0x719d9c, sent at login and when patching
extern TObject NoObject;                      // 0x719da0, never written

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
