// Map.h: the client's map. The visible part of the world is a cube of
// MAPSIZE_X x MAPSIZE_Y x MAPSIZE_Z map points with up to OBJECTS_PER_POINT
// objects each; graphical effects, animated texts and missiles live in a
// table of EFFECTS entries linked to the points; the automap keeps the colours
// and speeds of the explored world in blocks of 256x256 fields that are cached
// in memory and stored in files "<bx><by><bz>.map".
//
// Coordinates: rx, ry, rz are map (relative) coordinates, x, y, z world
// (absolute) ones. rz grows upwards; the player's floor is rz 7-z above
// ground and rz 2 below it (GetMapLevel).
//
// Names from the binary: MAPSIZE_X/Y/Z, OBJECTS_PER_POINT, EFFECTS, the
// variables Point, Effect, Effects, Block, Coordinate, EffectNumber,
// EffectIteratorField, EffectIteratorCount and AmbienteActualBrightness, the
// fields ObjectCount, EffectNumber, Status, Type, Next, Previous, bx, by, bz,
// x, y, the parameter names in the assertions and catch blocks, and FindPath's
// StartX ... ForceDiagonal (its error message). Everything else is ours
// (notes/decisions.md, "Map.cpp").

#pragma once

#include "common.h"
#include "types/FixedString.h"
#include "Container.h"
#include "graphics.h" // TRGBColor
#include "Objects.h"  // TObject
#include <array>
#include <cstddef>

inline constexpr int MAPSIZE_X = 18;
inline constexpr int MAPSIZE_Y = 14;
inline constexpr int MAPSIZE_Z = 8;
inline constexpr int OBJECTS_PER_POINT = 10;
inline constexpr int EFFECTS = 200;

// A map point (0xac bytes); "Point" in the assertions.
struct TMapPoint {
	int ObjectCount;                                // +0x00
	std::array<TObject, OBJECTS_PER_POINT> Objects; // +0x04, bottom to top
	std::array<int, OBJECTS_PER_POINT> Order;       // +0x7c, drawing order (SortObjects)
	int EffectNumber;                               // +0xa4, first effect on the point, -1 none
	bool RefreshRequested;                          // +0xa8
};

// An effect (0x44 bytes). Status 0 is a free entry, 1 a graphical effect, 2 an
// animated text, 3 a missile. The three kinds share the space after Type.
struct TEffect {
	int Status; // +0x00
	int Type;   // +0x04, the colour of a text
	union {
		int Phase;                 // +0x08, graphical effect
		struct {                   // animated text
			TFixedString<12> Text; // +0x08, at most 9 characters
			int TextPhase;         // +0x14
		};
		struct {           // missile; world coordinates
			int Direction; // +0x08, 0 north, clockwise, 4 south
			int DestX;     // +0x0c
			int DestY;     // +0x10
			int DestZ;     // +0x14
			int x;         // +0x18, the field it is linked to
			int y;         // +0x1c
			int OffsetX;   // +0x20, pixels from the destination
			int OffsetY;   // +0x24
			int EndTime;   // +0x28
			int DistanceX; // +0x2c, abs(start - destination) * 32
			int DistanceY; // +0x30
			int Duration;  // +0x34, milliseconds
		};
	};
	int Field;    // +0x38, the map point (index into the point table)
	int Previous; // +0x3c, effects of the same point
	int Next;     // +0x40
};
// The original's layout (no pointers: the same on x64), with Text a TFixedString (issue #57).
static_assert(sizeof(TEffect) == 0x44 && offsetof(TEffect, Text) == 0x08 && offsetof(TEffect, TextPhase) == 0x14
		&& offsetof(TEffect, Field) == 0x38,
	"TEffect's layout");

// An automap mark (0x70 bytes). The constructor (inlined in the vector<TMark>
// constructor 0x426170 and in LoadAutomapBlock) clears it.
struct TMark {
	TMark()
	{
		x = 0;
		y = 0;
		Type = 0;
		Text[0] = 0;
	}

	int x;                  // +0x00, world coordinates; 0,0 is a deleted mark
	int y;                  // +0x04
	int Type;               // +0x08, the mark symbol
	TFixedString<100> Text; // +0x0c
};

// A world position, as passed to AddAutomapMark ("Coordinate.x").
struct TCoordinate {
	int x;
	int y;
	int z;
};

// One automap block: 256x256 fields of one floor (0x200a8 bytes). The
// inline constructor is 0x4b1410, the compiler-generated destructor (which
// frees Marks) 0x4b0b30.
struct TAutomapBlock {
	TAutomapBlock() : Marks(0, 0, 10) { MarkCount = 0; }

	int bx;                                        // +0x00, -1 for an unused block
	int by;                                        // +0x04
	int bz;                                        // +0x08
	int Previous;                                  // +0x0c, the cache's LRU list, toward the most recent
	int Next;                                      // +0x10, toward the least recent
	std::array<std::array<uint8, 256>, 256> Color; // +0x14, [x][y], palette colour
	std::array<std::array<uint8, 256>, 256> Speed; // +0x10014, [x][y], ground speed, 250 unknown, 255 blocked
	bool Changed;                                  // +0x20014
	vector<TMark> Marks;                           // +0x20018
	int MarkCount;                                 // +0x200a4
};

// A node of FindPath's search (0x20 bytes); constructor 0x4b0880.
struct TPathNode {
	TPathNode()
	{
		Cost = -1;
		Estimate = -1;
		Predecessor = nullptr;
		Next = nullptr;
	}

	int x;                  // +0x00, relative to the start
	int y;                  // +0x04
	bool Blocked;           // +0x08
	int Cost;               // +0x0c, from the destination
	int Speed;              // +0x10
	int Estimate;           // +0x14, cost plus the estimate to the start
	TPathNode *Predecessor; // +0x18, toward the destination
	TPathNode *Next;        // +0x1c, the open list, sorted by Estimate
};

// Position and coordinates (the gap before Map.cpp's first assertion).
void SetMapPosition(int x, int y, int z);
void GetMapPosition(int *x, int *y, int *z);
int GetMapLevel();
void RelativeToAbsolute(int *x, int *y, int *z);
void AbsoluteToRelative(int *x, int *y, int *z);

// Ambient light.
void SetAmbientLight(int Brightness, int Color);
void UpdateAmbientLight();
TRGBColor GetAmbientColor();
int GetAmbientBrightness();

// Objects.
TObject GetFirstObject(int rx, int ry, int rz);
TObject GetNextObject();
TObject GetMapObject(int rx, int ry, int rz, int Position);
TObject GetTopMoveObject(int rx, int ry, int rz, int &Position);
TObject GetTopLookObject(int rx, int ry, int rz, int &Position);
TObject GetTopUseObject(int rx, int ry, int rz, int &Position);
TObject GetTopMultiUseObject(int rx, int ry, int rz, int &Position);
void AddObject(int rx, int ry, int rz, TObject Obj);
void AppendObject(int rx, int ry, int rz, TObject Obj);
bool DeleteObject(int rx, int ry, int rz, int Position);
void ChangeObject(int rx, int ry, int rz, int Position, TObject Obj);

// Effects.
int GetFirstEffect(int rx, int ry, int rz);
int GetNextEffect();
int GetEffectStatus(int EffectNumber);
int GetGraphicalEffectType(int EffectNumber);
int GetGraphicalEffectPhase(int EffectNumber);
int GetTextualEffectColor(int EffectNumber);
char *GetTextualEffectText(int EffectNumber);
int GetTextualEffectPhase(int EffectNumber);
int GetMissileEffectType(int EffectNumber);
int GetMissileEffectDirection(int EffectNumber);
int GetMissileEffectX(int EffectNumber);
int GetMissileEffectY(int EffectNumber);
void AddGraphicalEffect(int rx, int ry, int rz, int Type);
void AddTextualEffect(int rx, int ry, int rz, int Color, const char *Text);
void AddMissileEffect(int sx, int sy, int sz, int dx, int dy, int dz, int Type);
void ProcessEffects();
void ProcessMissileEffects();

// Map points and scrolling.
void ClearField(int rx, int ry, int rz, bool DeleteEffects);
void ClearMap();
void ShiftMap(int dx, int dy);
void ShiftMapLevel(int dz);
void RequestFieldRefresh(int rx, int ry, int rz);
int GetFieldHeight(int rx, int ry, int rz);
bool IsFieldPassable(int rx, int ry, int rz, bool CheckHeight);
bool IsFieldThrowable(int rx, int ry, int rz);
bool IsFieldTranslucent(int rx, int ry, int rz);
bool FieldRefreshRequested(int rx, int ry, int rz);

// The automap. The block cache and the file functions are static.
bool GetAutomapChanged();
void UpdateAutomap(int rx, int ry);
TRGBColor GetAutomapColor(int x, int y, int z);
void GetAutomapMarks(int x, int y, int z, vector<TMark> *Marks, int *Count);
void AddAutomapMark(TCoordinate &Coordinate, const char *Text, int Type);
void DeleteAutomapMark(TMark &Deleted, int z);

// Paths, initialisation.
int FindPath(int StartX, int StartY, int StartZ, int DestX, int DestY, int DestZ, bool MustReach,
	int *Steps, int *Directions, bool ForceDiagonal);
void InitMap(void (*Callback)(int CreatureID));
void ExitMap();
