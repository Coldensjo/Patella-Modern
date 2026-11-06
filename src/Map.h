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
#include "types/IndexedVector.h"
#include "types/SlotTable.h"
#include "Container.h"
#include "graphics.h" // TRGBColor
#include "MapPoint.h" // TMapPoint, OBJECTS_PER_POINT (issue #102)
#include "Objects.h"  // TObject
#include <array>
#include <cstddef>
#include <iterator> // std::default_sentinel_t, TMapEffects

inline constexpr int MAPSIZE_X = 18;
inline constexpr int MAPSIZE_Y = 14;
inline constexpr int MAPSIZE_Z = 8;
inline constexpr int EFFECTS = 200;

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
	int Field;          // +0x38, the map point (index into the point table, not an effect)
	TEffectId Previous; // +0x3c, effects of the same point; None for the first
	TEffectId Next;     // +0x40, None for the last
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
	TIndexedVector<TMark> Marks;                   // +0x20018
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

inline constexpr int AUTOMAP_BLOCKS = 10;

// The effect table (0x5d16f8): EFFECTS entries, numbered by TEffectId (issue #103; notes/decisions.md, "#102").
using TEffectTable = TSlotTable<TEffect, EFFECTS, TEffectId>;

// The effects of one map point, first to last, by their Next links (issue #103): TEffectTable::Chain from the point's
// EffectNumber (TMap::PointEffects), the range Map.cpp's loops walk. A loop's body may delete the effect it is given
// (ClearField does): DeleteEffect keeps the deleted entry's Next, which the step reads.
using TPointEffects = TEffectTable::TChain;

namespace patella {

class TObjectTypes;

namespace testmode {
struct TMapView;
} // namespace testmode

// Map.cpp's data and functions (Patella-Modern issue #106, M7; notes/decisions.md, "#104: TClient and the subsystem
// objects" and "#106"): the map points and their position, the effect table and the two iterators, the automap blocks
// and their cache list, the ambient light (0x5d16c0-0x5d4c4b, 0x7152d0-0x7152e7), private as the file-local statics
// were, with the same initial values (Fields and FieldNumber as their initialisers 0x559f40 and 0x559f60 made them,
// the others 0). The functions are the members of the same names, the file-local ones private. TMapState owns it
// (MapState.h), after TObjectTypes, which it reads.
class TMap {
public:
	explicit TMap(TObjectTypes &ObjectTypes);
	TMap(const TMap &) = delete;
	TMap &operator=(const TMap &) = delete;
	TMap(TMap &&) = delete;
	TMap &operator=(TMap &&) = delete;
	~TMap() = default;

	// Position and coordinates (the gap before Map.cpp's first assertion).
	void SetMapPosition(int x, int y, int z);
	void GetMapPosition(int *x, int *y, int *z) const;
	int GetMapLevel() const;
	void RelativeToAbsolute(int *x, int *y, int *z) const;
	void AbsoluteToRelative(int *x, int *y, int *z) const;

	// Ambient light.
	void SetAmbientLight(int Brightness, int Color);
	void UpdateAmbientLight();
	TRGBColor GetAmbientColor() const;
	int GetAmbientBrightness() const;

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
	TEffectId GetFirstEffect(int rx, int ry, int rz);
	TEffectId GetNextEffect();
	int GetEffectStatus(TEffectId EffectNumber) const;
	int GetGraphicalEffectType(TEffectId EffectNumber) const;
	int GetGraphicalEffectPhase(TEffectId EffectNumber) const;
	int GetTextualEffectColor(TEffectId EffectNumber) const;
	char *GetTextualEffectText(TEffectId EffectNumber);
	int GetTextualEffectPhase(TEffectId EffectNumber) const;
	int GetMissileEffectType(TEffectId EffectNumber) const;
	int GetMissileEffectDirection(TEffectId EffectNumber) const;
	int GetMissileEffectX(TEffectId EffectNumber) const;
	int GetMissileEffectY(TEffectId EffectNumber) const;
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
	void GetAutomapMarks(int x, int y, int z, TIndexedVector<TMark> *Marks, int *Count);
	void AddAutomapMark(TCoordinate &Coordinate, const char *Text, int Type);
	void DeleteAutomapMark(TMark &Deleted, int z);

	// Paths, initialisation.
	int FindPath(int StartX, int StartY, int StartZ, int DestX, int DestY, int DestZ, bool MustReach, int *Steps,
		int *Directions, bool ForceDiagonal);
	void InitMap(void (*Callback)(int CreatureID));
	void ExitMap();

	// The test mode's view (ClientView.h's ViewMap; test build only).
	void View(testmode::TMapView *Target) const;

private:
	void SortObjects(int rx, int ry, int rz);
	void InsertObject(int rx, int ry, int rz, int Position, TObject Obj);
	[[nodiscard]] TPointEffects PointEffects(const TMapPoint &Point) const noexcept { return Effects.Chain(Point.EffectNumber); }
	TEffectId GetEffectIteratorNumber(int n);
	TEffectId NewEffect(int Status, int rx, int ry, int rz);
	void DeleteEffect(TEffectId Number);
	void ShiftMapWest();
	void ShiftMapEast();
	void ShiftMapNorth();
	void ShiftMapSouth();
	void ShiftMapUp();
	void ShiftMapDown();
	void GetFieldAutomapInfo(int rx, int ry, int rz, uint8 *Color, uint8 *Speed);
	void LoadAutomapBlock(int Position, int bx, int by, int bz);
	void SaveAutomapBlock(int Position);
	TAutomapBlock *GetAutomapBlock(int bx, int by, int bz);
	bool IsAutomapBlocked(int x, int y, int z);
	int GetAutomapSpeed(int x, int y, int z);

	TObjectTypes &ObjectTypes;

	// Data, in the original's .data/.bss order.
	void (*CreatureCallback)(int CreatureID){};                                      // 0x5d16c0, called for every creature ClearField removes
	int AmbienteActualBrightness{};                                                  // 0x5d16c4, follows AmbienteBrightness step by step
	int AmbienteBrightness{};                                                        // 0x5d16c8, -1: take the next value at once
	bool AutomapChanged{};                                                           // 0x5d16cc
	int LeastRecentBlock{};                                                          // 0x5d16d0, tail of the block cache's LRU list
	int MostRecentBlock{};                                                           // 0x5d16d4, head
	int EffectIteratorCount{};                                                       // 0x5d16d8
	int EffectIteratorField{};                                                       // 0x5d16dc
	int ObjectIteratorCount{};                                                       // 0x5d16e0
	int ObjectIteratorField{};                                                       // 0x5d16e4
	int MapZ{};                                                                      // 0x5d16e8, world position of the map's centre
	int MapY{};                                                                      // 0x5d16ec
	int MapX{};                                                                      // 0x5d16f0
	TEffectTable Effects;                                                            // 0x5d16f8, an array of EFFECTS until issue #102
	array<TMapPoint> Fields{0, MAPSIZE_X * MAPSIZE_Y * MAPSIZE_Z - 1};               // 0x5d4c18, initialiser at 0x559f40
	matrix3d<int> FieldNumber{0, MAPSIZE_X - 1, 0, MAPSIZE_Y - 1, 0, MAPSIZE_Z - 1}; // 0x5d4c24, 0x559f60
	std::array<TAutomapBlock, AUTOMAP_BLOCKS> AutomapBlocks;                         // 0x5d4c40, initialiser at 0x559f90
	TRGBColor AmbienteColor;                                                         // 0x7152d0
	TRGBColor AmbienteActualColor;                                                   // 0x7152dc

	static_assert(sizeof(AutomapBlocks) == AUTOMAP_BLOCKS * sizeof(TAutomapBlock), "AutomapBlocks");
};

// The transition's access path (issue #106; notes/decisions.md, "#104: TClient and the subsystem objects"): the map
// of the one TClient (Client.cpp). It goes when no free function needs it.
namespace bridge {
TMap &Map();
} // namespace bridge

} // namespace patella

// The original's functions (the callers' names, and the golden tests'): each calls TMap's member of the same name on
// bridge::Map().
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

// The effects of map point rx, ry, rz by GetFirstEffect and GetNextEffect, as the original's numbers (issue #103):
// TMapWindow::DrawField's effect loop. begin() calls GetFirstEffect, each step GetNextEffect, in the original loop's
// order; the range ends at the first -1, and gives any other number as it comes (the loop checks the range itself).
class TMapEffects {
public:
	class TIterator {
	public:
		using value_type = int;
		using difference_type = std::ptrdiff_t;

		TIterator() = default;
		explicit TIterator(int Number) noexcept : Number(Number) {}

		[[nodiscard]] int operator*() const noexcept { return Number; }
		TIterator &operator++()
		{
			Number = GetNextEffect();
			return *this;
		}
		TIterator operator++(int)
		{
			TIterator Before = *this;
			++*this;
			return Before;
		}
		[[nodiscard]] bool operator==(std::default_sentinel_t /*End*/) const noexcept { return Number == -1; }

	private:
		int Number = -1;
	};

	TMapEffects(int rx, int ry, int rz) noexcept : rx(rx), ry(ry), rz(rz) {}

	[[nodiscard]] TIterator begin() const { return TIterator{GetFirstEffect(rx, ry, rz)}; }
	[[nodiscard]] static std::default_sentinel_t end() noexcept { return std::default_sentinel; }

private:
	int rx;
	int ry;
	int rz;
};

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
void GetAutomapMarks(int x, int y, int z, TIndexedVector<TMark> *Marks, int *Count);
void AddAutomapMark(TCoordinate &Coordinate, const char *Text, int Type);
void DeleteAutomapMark(TMark &Deleted, int z);

// Paths, initialisation.
int FindPath(int StartX, int StartY, int StartZ, int DestX, int DestY, int DestZ, bool MustReach,
	int *Steps, int *Directions, bool ForceDiagonal);
void InitMap(void (*Callback)(int CreatureID));
void ExitMap();
