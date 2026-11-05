// Map.cpp: the map points and their objects, the effects, the automap and
// the path finder (c:\mud\tibiaplayer\Map.cpp).
//
// Functions in source order (assertion and catch lines). The functions of the
// MainWindow/Map gap (0x4b08a0-0x4b0b10: the map position and the ambient
// light) have no line evidence and come first, where the binary has them.
//
// The try blocks and catch lines the comments name are the original's (its
// TBEGIN/TEND); since issue #78 the functions here have none.
//
// Container.h instances of this object: array<TMapPoint> (0x4b0bc0, 0x4b5900),
// matrix3d<int> (0x4b0d70, 0x4b5b40), matrix<TPathNode> (0x4b1100, 0x4b5e90,
// 0x4b0b20); vector<TMark> is used through the copies kept from an earlier
// object (0x426170, 0x425b20). Compiler-generated: TMapPoint's constructor
// 0x4b0b40, TAutomapBlock's destructor 0x4b0b30; the inline constructors of
// TPathNode (0x4b0880) and TAutomapBlock (0x4b1410) are in Map.h.
//
// Names: see Map.h and notes/decisions.md ("Map.cpp").

#include "Map.h"
#include "Objects.h"
#include "Script.h"
#include "Talk.h"
#include "Utils.h"
#include "sysinc.h"
#include "Container.h"
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sstream>

// Assertions whose expression text carries a bug number.
#define TASSERTB(Line, Expr, Bug) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", "", 0, "", 0, #Expr " " Bug)
#define TASSERTB1(Line, Expr, V1, Bug) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, "", 0, #Expr " " Bug)
#define TASSERTB2(Line, Expr, V1, V2, Bug) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, #V2, V2, #Expr " " Bug)

constexpr int AUTOMAP_BLOCKS = 10;

namespace {

void (*CreatureCallback)(int CreatureID);                                        // 0x5d16c0, called for every creature ClearField removes
int AmbienteActualBrightness;                                                    // 0x5d16c4, follows AmbienteBrightness step by step
int AmbienteBrightness;                                                          // 0x5d16c8, -1: take the next value at once
bool AutomapChanged;                                                             // 0x5d16cc
int LeastRecentBlock;                                                            // 0x5d16d0, tail of the block cache's LRU list
int MostRecentBlock;                                                             // 0x5d16d4, head
int EffectIteratorCount;                                                         // 0x5d16d8
int EffectIteratorField;                                                         // 0x5d16dc
int ObjectIteratorCount;                                                         // 0x5d16e0
int ObjectIteratorField;                                                         // 0x5d16e4
int MapZ;                                                                        // 0x5d16e8, world position of the map's centre
int MapY;                                                                        // 0x5d16ec
int MapX;                                                                        // 0x5d16f0
std::array<TEffect, EFFECTS> Effects;                                            // 0x5d16f8
array<TMapPoint> Fields(0, MAPSIZE_X * MAPSIZE_Y * MAPSIZE_Z - 1);               // 0x5d4c18, initialiser at 0x559f40
matrix3d<int> FieldNumber(0, MAPSIZE_X - 1, 0, MAPSIZE_Y - 1, 0, MAPSIZE_Z - 1); // 0x5d4c24, 0x559f60
std::array<TAutomapBlock, AUTOMAP_BLOCKS> AutomapBlocks;                         // 0x5d4c40, initialiser at 0x559f90
TRGBColor AmbienteColor;                                                         // 0x7152d0
TRGBColor AmbienteActualColor;                                                   // 0x7152dc

} // namespace

// The arrays std::arrays and TMark::Text a TFixedString (issue #60). TMapPoint and TMark have no pointers: the same
// on x64.
static_assert(sizeof(TMapPoint) == 0xac && offsetof(TMapPoint, Objects) == 0x04 && offsetof(TMapPoint, Order) == 0x7c
		&& sizeof(TMapPoint::Objects) == 0x78 && sizeof(TMapPoint::Order) == 0x28,
	"TMapPoint's layout");
static_assert(sizeof(TMark) == 0x70 && offsetof(TMark, Text) == 0x0c && sizeof(TMark::Text) == 100, "TMark's layout");
#if PATELLA_ORIGINAL_LAYOUT
static_assert(sizeof(TAutomapBlock) == 0x200a8 && offsetof(TAutomapBlock, Color) == 0x14
		&& offsetof(TAutomapBlock, Speed) == 0x10014 && sizeof(TAutomapBlock::Color) == 0x10000
		&& sizeof(TAutomapBlock::Speed) == 0x10000,
	"TAutomapBlock's layout");
#endif
static_assert(sizeof(Effects) == EFFECTS * sizeof(TEffect) && sizeof(AutomapBlocks) == AUTOMAP_BLOCKS * sizeof(TAutomapBlock),
	"Effects and AutomapBlocks");

// --- Position (the MainWindow/Map gap) ------------------------------------

// 0x4b08a0
void SetMapPosition(int x, int y, int z)
{
	MapX = x;
	MapY = y;
	MapZ = z;
}

// 0x4b08c0
void GetMapPosition(int *x, int *y, int *z)
{
	*x = MapX;
	*y = MapY;
	*z = MapZ;
}

// 0x4b08f0. The map level (rz) of the centre's floor: above ground all eight
// floors are on the map, below ground two above and two below.
int GetMapLevel()
{
	if (MapZ <= 7)
		return 7 - MapZ;
	return 2;
}

// 0x4b0910. Map coordinates to world coordinates; every floor above the
// centre's is shifted by one field to the north-west.
void RelativeToAbsolute(int *x, int *y, int *z)
{
	const int dz = *z - GetMapLevel();
	*x += MapX - 8 + dz;
	*y += MapY - 6 + dz;
	*z = MapZ - dz;
}

// 0x4b0960
void AbsoluteToRelative(int *x, int *y, int *z)
{
	const int dz = MapZ - *z;
	*x += 8 - MapX - dz;
	*y += 6 - MapY - dz;
	*z = GetMapLevel() + dz;
}

// 0x4b09d0
bool GetAutomapChanged()
{
	const bool Result = AutomapChanged;
	AutomapChanged = false;
	return Result;
}

// 0x4b09e0. Color is a palette index. After SetAmbientLight(-1, ...) the next
// call takes effect at once instead of fading.
void SetAmbientLight(int Brightness, int Color)
{
	const bool Immediately = AmbienteBrightness == -1;
	AmbienteBrightness = Brightness;
	AmbienteColor = TRGBColor(Color);
	if (Immediately) {
		AmbienteActualBrightness = Brightness;
		AmbienteActualColor = AmbienteColor;
	}
}

// 0x4b0a70. One step of the fade toward the set light.
void UpdateAmbientLight()
{
	if (AmbienteActualBrightness < AmbienteBrightness)
		AmbienteActualBrightness++;
	if (AmbienteActualBrightness > AmbienteBrightness)
		AmbienteActualBrightness--;
	if (AmbienteActualColor.R < AmbienteColor.R)
		AmbienteActualColor.R++;
	if (AmbienteActualColor.R > AmbienteColor.R)
		AmbienteActualColor.R--;
	if (AmbienteActualColor.G < AmbienteColor.G)
		AmbienteActualColor.G++;
	if (AmbienteActualColor.G > AmbienteColor.G)
		AmbienteActualColor.G--;
	if (AmbienteActualColor.B < AmbienteColor.B)
		AmbienteActualColor.B++;
	if (AmbienteActualColor.B > AmbienteColor.B)
		AmbienteActualColor.B--;
}

// 0x4b0af0
TRGBColor GetAmbientColor()
{
	return AmbienteActualColor;
}

// --- Objects ---------------------------------------------------------------

// 0x4b6140, lines 169-171. Starts an iteration over the objects of a point in
// drawing order; NoObject (Type 0) ends it.
TObject GetFirstObject(int rx, int ry, int rz)
{
	TASSERTB1(169, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(170, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(171, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	ObjectIteratorField = FieldNumber(rx, ry, rz);
	ObjectIteratorCount = 0;
	const TMapPoint &Point = Fields(ObjectIteratorField);
	if (Point.ObjectCount == 0)
		return NoObject;
	return Point.Objects[Point.Order[0]];
}

// 0x4b6510
TObject GetNextObject()
{
	ObjectIteratorCount++;
	const TMapPoint &Point = Fields(ObjectIteratorField);
	if (ObjectIteratorCount >= Point.ObjectCount)
		return NoObject;
	return Point.Objects[Point.Order[ObjectIteratorCount]];
}

// 0x4b6580, lines 197-202. The object at a stack position (0 is the bottom).
TObject GetMapObject(int rx, int ry, int rz, int Position)
{
	TASSERTB1(197, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(198, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(199, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	const TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	TASSERT1(202, In(Position,0,OBJECTS_PER_POINT-1), Position);
	if (Position < Point.ObjectCount)
		return Point.Objects[Position];
	return NoObject;
}

// 0x4b6a00, lines 213-228. The lowest object that is no ground, border,
// bottom or top object and no creature, or the one below it when it cannot
// be moved.
TObject GetTopMoveObject(int rx, int ry, int rz, int &Position)
{
	TASSERTB1(213, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(214, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(215, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	const TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	Position = 0;
	while (Position < Point.ObjectCount - 1) {
		const TObject &Obj = Point.Objects[Position];
		if (!GetObjectFlag(Obj.Type, BANK) && !GetObjectFlag(Obj.Type, CLIP) && !GetObjectFlag(Obj.Type, BOTTOM)
			&& !GetObjectFlag(Obj.Type, TOP) && Obj.Type != CREATURE)
			break;
		Position++;
	}
	if (Position > 0 && GetObjectFlag(Point.Objects[Position].Type, UNMOVE))
		Position--;
	return Point.Objects[Position];
}

// 0x4b6ee0, lines 237-250. The lowest object that is no ground, border,
// bottom or top object.
TObject GetTopLookObject(int rx, int ry, int rz, int &Position)
{
	TASSERTB1(237, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(238, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(239, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	const TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	Position = 0;
	while (Position < Point.ObjectCount - 1) {
		const TObject &Obj = Point.Objects[Position];
		if (!GetObjectFlag(Obj.Type, BANK) && !GetObjectFlag(Obj.Type, CLIP) && !GetObjectFlag(Obj.Type, BOTTOM)
			&& !GetObjectFlag(Obj.Type, TOP))
			break;
		Position++;
	}
	return Point.Objects[Position];
}

// 0x4b73a0, lines 259-275. The first object with FORCEUSE, else the lowest
// object that is no ground, border, bottom or top object and no creature,
// stepping down over creatures and liquid pools.
TObject GetTopUseObject(int rx, int ry, int rz, int &Position)
{
	TASSERTB1(259, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(260, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(261, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	const TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	Position = 0;
	while (Position < Point.ObjectCount - 1) {
		const TObject &Obj = Point.Objects[Position];
		if (GetObjectFlag(Obj.Type, FORCEUSE))
			return Obj;
		if (!GetObjectFlag(Obj.Type, BANK) && !GetObjectFlag(Obj.Type, CLIP) && !GetObjectFlag(Obj.Type, BOTTOM)
			&& !GetObjectFlag(Obj.Type, TOP) && Obj.Type != CREATURE)
			break;
		Position++;
	}
	while (Position > 0
		&& (Point.Objects[Position].Type == CREATURE || GetObjectFlag(Point.Objects[Position].Type, LIQUIDPOOL)))
		Position--;
	return Point.Objects[Position];
}

// 0x4b78d0, lines 284-299. Like GetTopUseObject, but creatures count and only
// one liquid pool is stepped over.
TObject GetTopMultiUseObject(int rx, int ry, int rz, int &Position)
{
	TASSERTB1(284, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(285, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(286, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	const TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	Position = 0;
	while (Position < Point.ObjectCount - 1) {
		const TObject &Obj = Point.Objects[Position];
		if (GetObjectFlag(Obj.Type, FORCEUSE))
			return Obj;
		if (!GetObjectFlag(Obj.Type, BANK) && !GetObjectFlag(Obj.Type, CLIP) && !GetObjectFlag(Obj.Type, BOTTOM)
			&& !GetObjectFlag(Obj.Type, TOP))
			break;
		Position++;
	}
	if (Position > 0 && GetObjectFlag(Point.Objects[Position].Type, LIQUIDPOOL))
		Position--;
	return Point.Objects[Position];
}

namespace {

// 0x4b7de0, lines 314-341. Computes the drawing order (the ground, border and
// bottom objects from the bottom, then the rest from the top) and the side
// of a hanging object from the hook it hangs on.
void SortObjects(int rx, int ry, int rz)
{
	TASSERTB1(314, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(315, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(316, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	int n = 0;
	for (int i = 0; i < Point.ObjectCount; i++) {
		if (!GetObjectFlag(Point.Objects[i].Type, BANK) && !GetObjectFlag(Point.Objects[i].Type, CLIP)
			&& !GetObjectFlag(Point.Objects[i].Type, BOTTOM))
			break;
		Point.Order[n++] = i;
	}
	for (int i = Point.ObjectCount - 1; i >= 0 && n != Point.ObjectCount; i--)
		Point.Order[n++] = i;
	TObject *Hanging = nullptr;
	const TObject *Hook = nullptr;
	for (int i = Point.ObjectCount - 1; i >= 0; i--) {
		TObject &Obj = Point.Objects[i];
		if (GetObjectFlag(Obj.Type, HANG))
			Hanging = &Obj;
		if (GetObjectFlag(Obj.Type, HOOKSOUTH) || GetObjectFlag(Obj.Type, HOOKEAST))
			Hook = &Obj;
	}
	if (Hanging != nullptr && Hook != nullptr)
		Hanging->Data2 = GetObjectFlag(Hook->Type, HOOKSOUTH) ? HOOKSOUTH : HOOKEAST;
}

// 0x4b8310, lines 351-366. Inserts an object at a stack position; on a full
// point the top object falls off. The try block starts only after the
// assertions.
void InsertObject(int rx, int ry, int rz, int Position, TObject Obj)
{
	TASSERTB1(351, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(352, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(353, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	TASSERT1(355, In(Position,0,OBJECTS_PER_POINT-1), Position);
	TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	TASSERT1(357, Position<=Point.ObjectCount, Position);
	int i = Point.ObjectCount;
	if (i > OBJECTS_PER_POINT - 1)
		i = OBJECTS_PER_POINT - 1;
	for (; i > Position; i--)
		Point.Objects[i] = Point.Objects[i - 1];
	Point.Objects[Position] = Obj;
	if (Point.ObjectCount < OBJECTS_PER_POINT)
		Point.ObjectCount++;
	SortObjects(rx, ry, rz);
}

} // namespace

// 0x4b88f0, lines 376-401. Inserts an object where it belongs: ground
// objects first, then borders, bottom objects, top objects, and new
// creatures above the creatures, other objects below them. Nothing happens
// on a full point.
void AddObject(int rx, int ry, int rz, TObject Obj)
{
	TASSERTB1(376, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(377, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(378, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	const TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	int Position;
	for (Position = 0; Position < Point.ObjectCount; Position++) {
		const TObject &Here = Point.Objects[Position];
		if (GetObjectFlag(Obj.Type, BANK) && !GetObjectFlag(Here.Type, BANK))
			break;
		if (GetObjectFlag(Obj.Type, BANK) == GetObjectFlag(Here.Type, BANK)) {
			if (GetObjectFlag(Obj.Type, CLIP) && !GetObjectFlag(Here.Type, CLIP))
				break;
			if (GetObjectFlag(Obj.Type, CLIP) == GetObjectFlag(Here.Type, CLIP)) {
				if (GetObjectFlag(Obj.Type, BOTTOM) && !GetObjectFlag(Here.Type, BOTTOM))
					break;
				if (GetObjectFlag(Obj.Type, BOTTOM) == GetObjectFlag(Here.Type, BOTTOM)) {
					if (GetObjectFlag(Obj.Type, TOP) && !GetObjectFlag(Here.Type, TOP))
						break;
					if (GetObjectFlag(Obj.Type, TOP) == GetObjectFlag(Here.Type, TOP)
						&& (Obj.Type == CREATURE || Here.Type != CREATURE))
						break;
				}
			}
		}
	}
	if (Position < OBJECTS_PER_POINT)
		InsertObject(rx, ry, rz, Position, Obj);
}

// 0x4b8ec0, lines 410-431. Puts an object on top of the stack.
void AppendObject(int rx, int ry, int rz, TObject Obj)
{
	TASSERTB1(410, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(411, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(412, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	const TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	if (!In(Point.ObjectCount, 0, OBJECTS_PER_POINT - 1)) {
		std::stringstream s;
		s << "ObjectCount: " << Point.ObjectCount << " rx: " << rx << " ry: " << ry << " rz: " << rz;
		throw TMYEXCEPTION(426, "too many objects on map point", "", 0, "", 0, s.str());
	}
	InsertObject(rx, ry, rz, Point.ObjectCount, Obj);
}

// 0x4b9420, lines 441-454. True when the point was full, so that the objects
// that fell off it have to be requested again.
bool DeleteObject(int rx, int ry, int rz, int Position)
{
	TASSERTB1(441, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(442, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(443, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	TASSERT1(447, In(Position,0,Point.ObjectCount-1), Position);
	for (; Position < Point.ObjectCount - 1; Position++)
		Point.Objects[Position] = Point.Objects[Position + 1];
	Point.ObjectCount--;
	SortObjects(rx, ry, rz);
	return Point.ObjectCount == OBJECTS_PER_POINT - 1;
}

// 0x4b9940, lines 463-473
void ChangeObject(int rx, int ry, int rz, int Position, TObject Obj)
{
	TASSERTB1(463, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(464, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(465, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	TASSERT1(469, In(Position,0,Point.ObjectCount-1), Position);
	Point.Objects[Position] = Obj;
	SortObjects(rx, ry, rz);
}

// --- Effects -----------------------------------------------------------------

namespace {

// 0x4b9e30, lines 508-516. The n-th effect of the iterated point, -1 after
// the last.
int GetEffectIteratorNumber(int n)
{
	int EffectNumber = Fields(EffectIteratorField).EffectNumber;
	TASSERTB2(508, In(EffectNumber,-1,EFFECTS-1), EffectNumber, EffectIteratorField, "[bug0000913]");
	while (EffectNumber != -1 && n != 0) {
		EffectNumber = Effects[EffectNumber].Next;
		n--;
	}
	return EffectNumber;
}

} // namespace

// 0x4ba100, lines 524-534
int GetFirstEffect(int rx, int ry, int rz)
{
	TASSERTB1(524, In(rx,0,MAPSIZE_X-1), rx, "[bug0000013]");
	TASSERTB1(525, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000013]");
	TASSERTB1(526, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000013]");
	EffectIteratorField = FieldNumber(rx, ry, rz);
	EffectIteratorCount = 0;
	return GetEffectIteratorNumber(0);
}

// 0x4ba540, line 548
int GetNextEffect()
{
	EffectIteratorCount++;
	return GetEffectIteratorNumber(EffectIteratorCount);
}

// 0x4b1440, line 556. Like the getters below, without a try block.
int GetEffectStatus(int EffectNumber)
{
	TASSERT1(556, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	return Effects[EffectNumber].Status;
}

// 0x4b15a0, lines 565-566
int GetGraphicalEffectType(int EffectNumber)
{
	TASSERT1(565, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(566, Effects[EffectNumber].Status==1, Effects[EffectNumber].Status);
	return Effects[EffectNumber].Type;
}

// 0x4b1860, lines 575-576
int GetGraphicalEffectPhase(int EffectNumber)
{
	TASSERT1(575, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(576, Effects[EffectNumber].Status==1, Effects[EffectNumber].Status);
	return Effects[EffectNumber].Phase;
}

// 0x4b1b20, lines 585-586
int GetTextualEffectColor(int EffectNumber)
{
	TASSERT1(585, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(586, Effects[EffectNumber].Status==2, Effects[EffectNumber].Status);
	return Effects[EffectNumber].Type;
}

// 0x4b1de0, lines 595-596
char *GetTextualEffectText(int EffectNumber)
{
	TASSERT1(595, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(596, Effects[EffectNumber].Status==2, Effects[EffectNumber].Status);
	return Effects[EffectNumber].Text;
}

// 0x4b20a0, lines 605-606
int GetTextualEffectPhase(int EffectNumber)
{
	TASSERT1(605, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(606, Effects[EffectNumber].Status==2, Effects[EffectNumber].Status);
	return Effects[EffectNumber].TextPhase;
}

// 0x4b2360, lines 615-616
int GetMissileEffectType(int EffectNumber)
{
	TASSERT1(615, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(616, Effects[EffectNumber].Status==3, Effects[EffectNumber].Status);
	return Effects[EffectNumber].Type;
}

// 0x4b2620, lines 625-626
int GetMissileEffectDirection(int EffectNumber)
{
	TASSERT1(625, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(626, Effects[EffectNumber].Status==3, Effects[EffectNumber].Status);
	return Effects[EffectNumber].Direction;
}

// 0x4b28e0, lines 635-636. Pixels from the field the missile is linked to.
int GetMissileEffectX(int EffectNumber)
{
	TASSERT1(635, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(636, Effects[EffectNumber].Status==3, Effects[EffectNumber].Status);
	const TEffect &Effect = Effects[EffectNumber];
	return (Effect.DestX - Effect.x) * 32 + Effect.OffsetX;
}

// 0x4b2bb0, lines 645-646
int GetMissileEffectY(int EffectNumber)
{
	TASSERT1(645, In(EffectNumber,0,EFFECTS-1), EffectNumber);
	TASSERT1(646, Effects[EffectNumber].Status==3, Effects[EffectNumber].Status);
	const TEffect &Effect = Effects[EffectNumber];
	return (Effect.DestY - Effect.y) * 32 + Effect.OffsetY;
}

namespace {

// 0x4ba6e0, lines 678-688. Takes a free entry (-1 if there is none) and puts
// it first on the point; coordinates off the map mean point 0,0,0.
int NewEffect(int Status, int rx, int ry, int rz)
{
	int Number = -1;
	for (int i = 0; i < EFFECTS; i++) {
		if (Effects[i].Status == 0) {
			Number = i;
			break;
		}
	}
	if (Number == -1)
		return -1;
	if (!In(rx, 0, MAPSIZE_X - 1) || !In(ry, 0, MAPSIZE_Y - 1) || !In(rz, 0, MAPSIZE_Z - 1)) {
		rx = 0;
		ry = 0;
		rz = 0;
	}
	TEffect &Effect = Effects[Number];
	Effect.Status = Status;
	Effect.Field = FieldNumber(rx, ry, rz);
	TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	TASSERTB1(678, In(Point.EffectNumber,-1,EFFECTS-1), Point.EffectNumber, "[bug0000913]");
	Effect.Next = Point.EffectNumber;
	Effect.Previous = -1;
	if (Point.EffectNumber != -1) {
		TASSERTB1(682, In(Number,-1,EFFECTS-1), Number, "[bug0000913]");
		Effects[Point.EffectNumber].Previous = Number;
	}
	Point.EffectNumber = Number;
	return Number;
}

} // namespace

// 0x4bab40, line 703
void AddGraphicalEffect(int rx, int ry, int rz, int Type)
{
	const int Number = NewEffect(1, rx, ry, rz);
	if (Number != -1) {
		Effects[Number].Type = Type;
		Effects[Number].Phase = 1;
	}
}

// 0x4bad00, lines 712-721
void AddTextualEffect(int rx, int ry, int rz, int Color, const char *Text)
{
	TASSERT(712, Text!=NULL);
	TASSERT1(713, strlen(Text)<10, strlen(Text));
	const int Number = NewEffect(2, rx, ry, rz);
	if (Number != -1) {
		Effects[Number].Type = Color;
		Effects[Number].Text.Assign(Text);
		Effects[Number].TextPhase = 0;
	}
}

// 0x4bb0c0, lines 730-768. Only missiles on the centre's floor are shown.
// The flight takes 150 ms times the fourth root of the squared distance,
// truncated; the direction is one of eight sectors of 45 degrees (tan 22.5
// and tan 67.5 as 106/256 and 618/256).
void AddMissileEffect(int sx, int sy, int sz, int dx, int dy, int dz, int Type)
{
	TASSERT1(730, Type>0, Type);
	if (sz != GetMapLevel())
		return;
	if (dz != GetMapLevel())
		return;
	const int Number = NewEffect(3, sx, sy, sz);
	if (Number == -1)
		return;
	TEffect &Effect = Effects[Number];
	Effect.Type = Type;
	if (dx == sx) {
		Effect.Direction = dy <= sy ? 0 : 4;
	} else if (dx > sx) {
		const int South = (dy - sy) * 256;
		if (South > (dx - sx) * 618)
			Effect.Direction = 4;
		else if (South > (dx - sx) * 106)
			Effect.Direction = 3;
		else if (South > (sx - dx) * 106)
			Effect.Direction = 2;
		else
			Effect.Direction = South > (sx - dx) * 618;
	} else {
		const int South = (dy - sy) * 256;
		if (South > (sx - dx) * 618)
			Effect.Direction = 4;
		else if (South > (sx - dx) * 106)
			Effect.Direction = 5;
		else if (South > (dx - sx) * 106)
			Effect.Direction = 6;
		else
			Effect.Direction = South <= (dx - sx) * 618 ? 0 : 7;
	}
	const int DeltaX = sx - dx;
	const int DeltaY = sy - dy;
	const int Steps = (int)sqrt(sqrt((double)DeltaX * DeltaX + DeltaY * DeltaY));
	Effect.OffsetX = DeltaX * 32;
	Effect.OffsetY = DeltaY * 32;
	Effect.EndTime = GetMilliseconds() + Steps * 150;
	Effect.DistanceX = abs(DeltaX) * 32;
	Effect.DistanceY = abs(DeltaY) * 32;
	Effect.Duration = Steps * 150;
	RelativeToAbsolute(&sx, &sy, &sz);
	RelativeToAbsolute(&dx, &dy, &dz);
	Effect.DestX = dx;
	Effect.DestY = dy;
	Effect.DestZ = dz;
	Effect.x = sx;
	Effect.y = sy;
}

namespace {

// 0x4bb520, lines 776-789. No try block.
void DeleteEffect(int Number)
{
	TASSERT1(776, In(Number,0,EFFECTS-1), Number);
	TEffect &Effect = Effects[Number];
	TASSERT(778, Effect.Status!=0);
	Effect.Status = 0;
	if (Fields(Effect.Field).EffectNumber == Number) {
		TASSERTB1(781, In(Effect.Next,-1,EFFECTS-1), Effect.Next, "[bug0000913]");
		Fields(Effect.Field).EffectNumber = Effect.Next;
	}
	if (Effect.Previous != -1) {
		TASSERTB1(785, In(Effect.Next,-1,EFFECTS-1), Effect.Next, "[bug0000913]");
		Effects[Effect.Previous].Next = Effect.Next;
	}
	if (Effect.Next != -1) {
		TASSERTB1(789, In(Effect.Previous,-1,EFFECTS-1), Effect.Previous, "[bug0000913]");
		Effects[Effect.Next].Previous = Effect.Previous;
	}
}

} // namespace

// 0x4bba70, lines 807-817. One animation step of the graphical effects and
// texts; a text lasts ten steps.
void ProcessEffects()
{
	int i = 0;
	for (i = 0; i < EFFECTS; i++) {
		TEffect &Effect = Effects[i];
		switch (Effect.Status) {
		case 1:
			Effect.Phase++;
			TASSERT1(807, Effect.Type > 0, Effect.Type);
			if (Effect.Phase > GetEffectPhases(Effect.Type))
				DeleteEffect(i);
			break;
		case 2:
			Effect.TextPhase++;
			if (Effect.TextPhase > 10)
				DeleteEffect(i);
			break;
		}
	}
}

// 0x4bbd40, lines 841-878. Moves the missiles toward their destinations and
// links them to the field they are over; a missile that has arrived is
// deleted. The offsets shrink by single pixels until they fit the time left.
void ProcessMissileEffects()
{
	int i = 0;
	const int Now = GetMilliseconds();
	for (i = 0; i < EFFECTS; i++) {
		TEffect &Effect = Effects[i];
		if (Effect.Status != 3)
			continue;
		if (Effect.OffsetX != 0) {
			while (Effect.OffsetX > 0 && (Effect.EndTime - Now) * Effect.DistanceX < Effect.Duration * Effect.OffsetX)
				Effect.OffsetX--;
			while (Effect.OffsetX < 0 && (Effect.EndTime - Now) * Effect.DistanceX < -(Effect.Duration * Effect.OffsetX))
				Effect.OffsetX++;
		}
		if (Effect.OffsetY != 0) {
			while (Effect.OffsetY > 0 && (Effect.EndTime - Now) * Effect.DistanceY < Effect.Duration * Effect.OffsetY)
				Effect.OffsetY--;
			while (Effect.OffsetY < 0 && (Effect.EndTime - Now) * Effect.DistanceY < -(Effect.Duration * Effect.OffsetY))
				Effect.OffsetY++;
		}
		if (Effect.OffsetX != 0 || Effect.OffsetY != 0 || Now <= Effect.EndTime) {
			int x = (Effect.DestX * 32 + 31 + Effect.OffsetX) / 32;
			int y = (Effect.DestY * 32 + 31 + Effect.OffsetY) / 32;
			int z = Effect.DestZ;
			if (x != Effect.x || y != Effect.y) {
				if (Fields(Effect.Field).EffectNumber == i) {
					TASSERTB1(850, In(Effect.Next,-1,EFFECTS-1), Effect.Next, "[bug0000913]");
					Fields(Effect.Field).EffectNumber = Effect.Next;
				}
				if (Effect.Previous != -1) {
					TASSERTB1(854, In(Effect.Next,-1,EFFECTS-1), Effect.Next, "[bug0000913]");
					Effects[Effect.Previous].Next = Effect.Next;
				}
				if (Effect.Next != -1) {
					TASSERTB1(858, In(Effect.Previous,-1,EFFECTS-1), Effect.Previous, "[bug0000913]");
					Effects[Effect.Next].Previous = Effect.Previous;
				}
				Effect.x = x;
				Effect.y = y;
				AbsoluteToRelative(&x, &y, &z);
				if (!In(x, 0, MAPSIZE_X - 1) || !In(y, 0, MAPSIZE_Y - 1) || !In(z, 0, MAPSIZE_Z - 1)) {
					x = 0;
					y = 0;
					z = 0;
				}
				TMapPoint &Point = Fields(FieldNumber(x, y, z));
				Effect.Field = FieldNumber(x, y, z);
				TASSERTB1(867, In(Point.EffectNumber,-1,EFFECTS-1), Point.EffectNumber, "[bug0000913]");
				Effect.Next = Point.EffectNumber;
				Effect.Previous = -1;
				if (Point.EffectNumber != -1) {
					TASSERTB1(871, In(i,-1,EFFECTS-1), i, "[bug0000913]");
					Effects[Point.EffectNumber].Previous = i;
				}
				Point.EffectNumber = i;
			}
		} else {
			TASSERT(841, Effect.Status!=0);
			DeleteEffect(i);
		}
	}
}

// --- Map points ----------------------------------------------------------------

// 0x4bc720, lines 891-913. Empties a point; its creatures go to the
// callback. The binary sets i to -1 after the object loop, so an error while
// deleting the effects reports i = -1.
void ClearField(int rx, int ry, int rz, bool DeleteEffects)
{
	TASSERTB1(891, In(rx,0,MAPSIZE_X-1), rx, "[bug0000913]");
	TASSERTB1(892, In(ry,0,MAPSIZE_Y-1), ry, "[bug0000913]");
	TASSERTB1(893, In(rz,0,MAPSIZE_Z-1), rz, "[bug0000913]");
	int i = 0;
	TMapPoint &Point = Fields(FieldNumber(rx, ry, rz));
	for (i = 0; i < Point.ObjectCount; i++) {
		if (Point.Objects[i].Type == CREATURE)
			CreatureCallback(Point.Objects[i].Data);
	}
	Point.ObjectCount = 0;
	Point.RefreshRequested = false;
	i = -1;
	if (DeleteEffects) {
		while (Point.EffectNumber != -1) {
			TASSERTB(908, Effects[Point.EffectNumber].Status!=0, "[bug0000611]");
			DeleteEffect(Point.EffectNumber);
		}
	}
}

// 0x4bccb0. Empties every point and deletes every effect; no try block.
void ClearMap()
{
	for (int i = 0; i < MAPSIZE_X * MAPSIZE_Y * MAPSIZE_Z; i++) {
		Fields(i).ObjectCount = 0;
		Fields(i).EffectNumber = -1;
		Fields(i).RefreshRequested = false;
	}
	for (int i = 0; i < EFFECTS; i++)
		Effects[i].Status = 0;
}

namespace {

// 0x4bcd20, line 945. The points move one field west (the centre moved
// east); the column that comes in on the east is cleared. Ghidra splits
// this and the five functions below at their loops (0x4bcd50 ...).
void ShiftMapWest()
{
	for (int z = 0; z < MAPSIZE_Z; z++) {
		for (int y = 0; y < MAPSIZE_Y; y++) {
			const int Help = FieldNumber(0, y, z);
			for (int x = 0; x < MAPSIZE_X - 1; x++)
				FieldNumber(x, y, z) = FieldNumber(x + 1, y, z);
			FieldNumber(MAPSIZE_X - 1, y, z) = Help;
			ClearField(MAPSIZE_X - 1, y, z, true);
		}
	}
}

// 0x4bcf30, line 964
void ShiftMapEast()
{
	for (int z = 0; z < MAPSIZE_Z; z++) {
		for (int y = 0; y < MAPSIZE_Y; y++) {
			const int Help = FieldNumber(MAPSIZE_X - 1, y, z);
			for (int x = MAPSIZE_X - 1; x > 0; x--)
				FieldNumber(x, y, z) = FieldNumber(x - 1, y, z);
			FieldNumber(0, y, z) = Help;
			ClearField(0, y, z, true);
		}
	}
}

// 0x4bd140, line 983
void ShiftMapNorth()
{
	for (int z = 0; z < MAPSIZE_Z; z++) {
		for (int x = 0; x < MAPSIZE_X; x++) {
			const int Help = FieldNumber(x, 0, z);
			for (int y = 0; y < MAPSIZE_Y - 1; y++)
				FieldNumber(x, y, z) = FieldNumber(x, y + 1, z);
			FieldNumber(x, MAPSIZE_Y - 1, z) = Help;
			ClearField(x, MAPSIZE_Y - 1, z, true);
		}
	}
}

// 0x4bd350, line 1002
void ShiftMapSouth()
{
	for (int z = 0; z < MAPSIZE_Z; z++) {
		for (int x = 0; x < MAPSIZE_X; x++) {
			const int Help = FieldNumber(x, MAPSIZE_Y - 1, z);
			for (int y = MAPSIZE_Y - 1; y > 0; y--)
				FieldNumber(x, y, z) = FieldNumber(x, y - 1, z);
			FieldNumber(x, 0, z) = Help;
			ClearField(x, 0, z, true);
		}
	}
}

} // namespace

// 0x4bd560, line 1018. dx, dy: how far the points move (the centre moves the
// other way).
void ShiftMap(int dx, int dy)
{
	MapX -= dx;
	MapY -= dy;
	for (; dx > 0; dx--)
		ShiftMapEast();
	for (; dx < 0; dx++)
		ShiftMapWest();
	for (; dy > 0; dy--)
		ShiftMapSouth();
	for (; dy < 0; dy++)
		ShiftMapNorth();
}

namespace {

// 0x4bd750, line 1037. The points move one level up; level 0 is cleared.
void ShiftMapUp()
{
	for (int x = 0; x < MAPSIZE_X; x++) {
		for (int y = 0; y < MAPSIZE_Y; y++) {
			const int Help = FieldNumber(x, y, MAPSIZE_Z - 1);
			for (int z = MAPSIZE_Z - 1; z > 0; z--)
				FieldNumber(x, y, z) = FieldNumber(x, y, z - 1);
			FieldNumber(x, y, 0) = Help;
			ClearField(x, y, 0, true);
		}
	}
}

// 0x4bd960, line 1056
void ShiftMapDown()
{
	for (int x = 0; x < MAPSIZE_X; x++) {
		for (int y = 0; y < MAPSIZE_Y; y++) {
			const int Help = FieldNumber(x, y, 0);
			for (int z = 0; z < MAPSIZE_Z - 1; z++)
				FieldNumber(x, y, z) = FieldNumber(x, y, z + 1);
			FieldNumber(x, y, MAPSIZE_Z - 1) = Help;
			ClearField(x, y, MAPSIZE_Z - 1, true);
		}
	}
}

} // namespace

// 0x4bdb70, line 1074. After moving up, the levels 5-7 are cleared as well
// (the binary decides this on dz before the loops count it down).
void ShiftMapLevel(int dz)
{
	const bool Up = dz > 0;
	for (; dz > 0; dz--)
		ShiftMapUp();
	for (; dz < 0; dz++)
		ShiftMapDown();
	if (Up) {
		for (int z = 5; z < MAPSIZE_Z; z++)
			for (int x = 0; x < MAPSIZE_X; x++)
				for (int y = 0; y < MAPSIZE_Y; y++)
					ClearField(x, y, z, true);
	}
}

// 0x4bdd70, line 1085
void RequestFieldRefresh(int rx, int ry, int rz)
{
	Fields(FieldNumber(rx, ry, rz)).RefreshRequested = true;
}

// 0x4bdf30, line 1104. The number of objects with HEIGHT.
int GetFieldHeight(int rx, int ry, int rz)
{
	int Height = 0;
	TObject Obj = GetFirstObject(rx, ry, rz);
	while (Obj.Type != 0) {
		if (GetObjectFlag(Obj.Type, HEIGHT))
			Height++;
		Obj = GetNextObject();
	}
	return Height;
}

// 0x4be130, line 1127. False if an object blocks the point. With CheckHeight,
// a point without ground above a pile of more than two HEIGHT objects, or any
// point while the centre stands on such a pile, is passable.
bool IsFieldPassable(int rx, int ry, int rz, bool CheckHeight)
{
	if (CheckHeight) {
		if (rz > 0 && rx < MAPSIZE_X - 1 && ry < MAPSIZE_Y - 1) {
			const TObject Bank = GetFirstObject(rx, ry, rz);
			if ((Bank.Type == 0 || !GetObjectFlag(Bank.Type, BANK)) && GetFieldHeight(rx + 1, ry + 1, rz - 1) > 2)
				return true;
		}
		if (GetFieldHeight(8, 6, GetMapLevel()) > 2)
			return true;
	}
	TObject Obj = GetFirstObject(rx, ry, rz);
	while (Obj.Type != 0) {
		if (GetObjectFlag(Obj.Type, UNPASS))
			return false;
		Obj = GetNextObject();
	}
	return true;
}

// 0x4be390, line 1141. False if an object blocks throwing.
bool IsFieldThrowable(int rx, int ry, int rz)
{
	TObject Obj = GetFirstObject(rx, ry, rz);
	while (Obj.Type != 0) {
		if (GetObjectFlag(Obj.Type, UNTHROW))
			return false;
		Obj = GetNextObject();
	}
	return true;
}

// 0x4be570, line 1153
bool IsFieldTranslucent(int rx, int ry, int rz)
{
	TObject Obj = GetFirstObject(rx, ry, rz);
	while (Obj.Type != 0) {
		if (GetObjectFlag(Obj.Type, TRANSLUCENT))
			return true;
		Obj = GetNextObject();
	}
	return false;
}

// 0x4be750, line 1164
bool FieldRefreshRequested(int rx, int ry, int rz)
{
	return Fields(FieldNumber(rx, ry, rz)).RefreshRequested;
}

namespace {

// 0x4be910, line 1191. The automap colour and speed of a point: the ground's
// speed (at most 250), the last AUTOMAP colour, 255 for an empty or blocked
// point.
void GetFieldAutomapInfo(int rx, int ry, int rz, uint8 *Color, uint8 *Speed)
{
	*Color = 0;
	*Speed = 250;
	bool Blocked = false;
	TObject Obj = GetFirstObject(rx, ry, rz);
	if (Obj.Type == 0)
		Blocked = true;
	while (Obj.Type != 0) {
		if (GetObjectFlag(Obj.Type, BANK)) {
			int Waypoints = GetObjectWaypoints(Obj.Type);
			if (Waypoints > 250)
				Waypoints = 250;
			*Speed = (uint8)Waypoints;
		}
		if (GetObjectFlag(Obj.Type, AUTOMAP))
			*Color = (uint8)GetObjectAutomapColor(Obj.Type);
		if (GetObjectFlag(Obj.Type, AVOID) || GetObjectFlag(Obj.Type, UNPASS))
			Blocked = true;
		Obj = GetNextObject();
	}
	if (Blocked)
		*Speed = 255;
}

// --- The automap -------------------------------------------------------------

// 0x4b2e80, lines 1199-1304. Loads a block into cache entry Position. A
// missing file leaves the block empty (speed 250 everywhere); a damaged one
// empties it again and reports that in the default console. The inner
// message says "colors" where it reads the colours and speeds, "flags" for
// the marks.
void LoadAutomapBlock(int Position, int bx, int by, int bz)
{
	TASSERT1(1199, In(bx,0,999), bx);
	TASSERT1(1200, In(by,0,999), by);
	TASSERT1(1201, In(bz,0, 99), bz);
	TAutomapBlock &Block = AutomapBlocks[Position];
	Block.bx = bx;
	Block.by = by;
	Block.bz = bz;
	memset(Block.Color.data(), 0, sizeof(Block.Color));
	memset(Block.Speed.data(), 250, sizeof(Block.Speed));
	Block.Changed = false;
	for (int i = Block.Marks.min; i <= Block.Marks.max; i++)
		Block.Marks(i) = TMark();
	Block.MarkCount = 0;
	TFixedString<16> FileName;
	FileName.Format("{:03}{:03}{:02}.map", bx, by, bz);
	TReadBinaryFile File;
	bool Opened = false;
	try {
		File.open(FileName);
		Opened = true;
	} catch (TMyException &) {
	}
	if (Opened) {
		try {
			try {
				File.readBytes(patella::TSpan<uint8>(&Block.Color[0][0], sizeof(Block.Color)));
				File.readBytes(patella::TSpan<uint8>(&Block.Speed[0][0], sizeof(Block.Speed)));
			} catch (TMyException &Exception) {
				throw TMYEXCEPTION(1244, "failed to write colors", "", 0, "", 0, Exception);
			}
			try {
				if (File.eof())
					Block.MarkCount = 0;
				else
					Block.MarkCount = File.readQuad();
				for (int i = 0; i < Block.MarkCount; i++) {
					const int x = File.readQuad();
					Block.Marks(i).x = x;
					const int y = File.readQuad();
					Block.Marks(i).y = y;
					const int Type = File.readQuad();
					Block.Marks(i).Type = Type;
					const char *Text = File.readString();
					if (Text != nullptr) {
						CopyString(Block.Marks(i).Text.Chars, Text);
						delete[] Text;
					}
				}
			} catch (TMyException &Exception) {
				throw TMYEXCEPTION(1268, "failed to load flags", "", 0, "", 0, Exception);
			}
			File.close();
		} catch (TMyException &) {
			File.close();
			Block.bx = bx;
			Block.by = by;
			Block.bz = bz;
			memset(Block.Color.data(), 0, sizeof(Block.Color));
			memset(Block.Speed.data(), 250, sizeof(Block.Speed));
			Block.Changed = false;
			for (int i = Block.Marks.min; i <= Block.Marks.max; i++)
				Block.Marks(i) = TMark();
			Block.MarkCount = 0;
			if (DefaultTextBuffer != nullptr) {
				TFixedString<256> Message;
				Message.Format("Map file '{}' was corrupted!", FileName);
				AddTextBufferLine(DefaultTextBuffer, 0, Message, 20, nullptr, 0);
				SetConsoleChanged();
			}
		}
	}
}

// 0x4b36e0, lines 1317-1411. Writes a changed block back; marks at 0,0 are
// deleted ones and are skipped. A file that cannot be opened or written is
// reported in the default console; the block counts as saved either way.
void SaveAutomapBlock(int Position)
{
	TAutomapBlock &Block = AutomapBlocks[Position];
	if (Block.bx == -1 || !Block.Changed)
		return;
	TASSERT1(1317, In(Block.bx,0,999), Block.bx);
	TASSERT1(1318, In(Block.by,0,999), Block.by);
	TASSERT1(1319, In(Block.bz,0, 99), Block.bz);
	TFixedString<16> FileName;
	FileName.Format("{:03}{:03}{:02}.map", Block.bx, Block.by, Block.bz);
	TWriteBinaryFile File;
	bool Opened = false;
	try {
		File.open(FileName);
		Opened = true;
	} catch (TMyException &) {
	}
	if (Opened) {
		try {
			try {
				File.writeBytes(patella::TSpan<const uint8>(&Block.Color[0][0], sizeof(Block.Color)));
				File.writeBytes(patella::TSpan<const uint8>(&Block.Speed[0][0], sizeof(Block.Speed)));
			} catch (TMyException &Exception) {
				throw TMYEXCEPTION(1346, "failed to write colors", "", 0, "", 0, Exception);
			}
			int Count = 0;
			for (int j = 0; j < Block.MarkCount; j++) {
				if (Block.Marks(j).x != 0 || Block.Marks(j).y != 0)
					Count++;
			}
			try {
				File.writeQuad(Count);
			} catch (TMyException &Exception) {
				throw TMYEXCEPTION(1361, "failed to write flag count", "", 0, "", 0, Exception);
			}
			int i = 0;
			try {
				for (i = 0; i < Block.MarkCount; i++) {
					if (Block.Marks(i).x != 0 || Block.Marks(i).y != 0) {
						File.writeQuad(Block.Marks(i).x);
						File.writeQuad(Block.Marks(i).y);
						File.writeQuad(Block.Marks(i).Type);
						File.writeString(Block.Marks(i).Text);
					}
				}
			} catch (TMyException &Exception) {
				throw TMYEXCEPTION(1376, "failed to write flag", "i", i, "", 0, Exception);
			}
			File.close();
		} catch (TMyException &) {
			File.close();
			if (DefaultTextBuffer != nullptr) {
				TFixedString<256> Message;
				Message.Format("Error while saving file '{}'!", FileName);
				AddTextBufferLine(DefaultTextBuffer, 0, Message, 20, nullptr, 0);
				SetConsoleChanged();
			}
		}
	} else if (DefaultTextBuffer != nullptr) {
		TFixedString<256> Message;
		Message.Format("Could not save file '{}'!", FileName);
		AddTextBufferLine(DefaultTextBuffer, 0, Message, 20, nullptr, 0);
		SetConsoleChanged();
	}
	Block.Changed = false;
}

// 0x4b3e80, lines 1420-1446. The cached block, loaded into the least recently
// used entry if necessary, becomes the most recently used one. (The binary
// keeps Position in bx's stack slot while the entry is replaced, the only
// part that can fail; the catch block reads it from there.)
TAutomapBlock *GetAutomapBlock(int bx, int by, int bz)
{
	TASSERT1(1420, bx>=0, bx);
	TASSERT1(1421, by>=0, by);
	TASSERT1(1422, bz>=0, bz);
	int Position = MostRecentBlock;
	while (Position != -1) {
		const TAutomapBlock &Block = AutomapBlocks[Position];
		if (Block.bx == bx && Block.by == by && Block.bz == bz)
			break;
		Position = Block.Next;
	}
	if (Position == -1) {
		Position = LeastRecentBlock;
		SaveAutomapBlock(Position);
		LoadAutomapBlock(Position, bx, by, bz);
	}
	if (Position != MostRecentBlock) {
		TAutomapBlock &Block = AutomapBlocks[Position];
		if (Block.Next != -1)
			AutomapBlocks[Block.Next].Previous = Block.Previous;
		if (Block.Previous != -1)
			AutomapBlocks[Block.Previous].Next = Block.Next;
		if (Position == LeastRecentBlock)
			LeastRecentBlock = Block.Previous;
		Block.Next = MostRecentBlock;
		Block.Previous = -1;
		AutomapBlocks[MostRecentBlock].Previous = Position;
		MostRecentBlock = Position;
	}
	return &AutomapBlocks[Position];
}

} // namespace

// 0x4beb90, line 1465. Records a point of the centre's level in the automap.
void UpdateAutomap(int rx, int ry)
{
	int x = rx;
	int y = ry;
	int z = GetMapLevel();
	RelativeToAbsolute(&x, &y, &z);
	TAutomapBlock *Block = GetAutomapBlock(x / 256, y / 256, z);
	GetFieldAutomapInfo(rx, ry, GetMapLevel(), &Block->Color[x % 256][y % 256], &Block->Speed[x % 256][y % 256]);
	Block->Changed = true;
	AutomapChanged = true;
}

// 0x4b4370, lines 1473-1479. Black for negative coordinates.
TRGBColor GetAutomapColor(int x, int y, int z)
{
	TASSERT1(1473, In(z,0,15), z);
	if (x >= 0 && y >= 0) {
		const TAutomapBlock *Block = GetAutomapBlock(x / 256, y / 256, z);
		return TRGBColor(Block->Color[x % 256][y % 256]);
	}
	return TRGBColor(0, 0, 0);
}

// 0x4b4680, lines 1485-1493. Copies the marks of a block; x and y are block
// coordinates here, despite their names.
void GetAutomapMarks(int x, int y, int z, vector<TMark> *Marks, int *Count)
{
	TASSERT1(1485, In(z,0,15), z);
	if (x >= 0 && y >= 0) {
		TAutomapBlock *Block = GetAutomapBlock(x, y, z);
		*Count = Block->MarkCount;
		for (int i = 0; i < Block->MarkCount; i++)
			(*Marks)(i) = Block->Marks(i);
	}
}

namespace {

// 0x4b4960, lines 1500-1506. Negative coordinates are blocked.
bool IsAutomapBlocked(int x, int y, int z)
{
	TASSERT1(1500, In(z,0,15), z);
	if (x >= 0 && y >= 0) {
		const TAutomapBlock *Block = GetAutomapBlock(x / 256, y / 256, z);
		return Block->Speed[x % 256][y % 256] == 255;
	}
	return true;
}

// 0x4b4c60, lines 1514-1520
int GetAutomapSpeed(int x, int y, int z)
{
	TASSERT1(1514, In(z,0,15), z);
	if (x >= 0 && y >= 0) {
		const TAutomapBlock *Block = GetAutomapBlock(x / 256, y / 256, z);
		return Block->Speed[x % 256][y % 256];
	}
	return 1;
}

} // namespace

// 0x4b4f60, lines 1538-1557
void AddAutomapMark(TCoordinate &Coordinate, const char *Text, int Type)
{
	TASSERT(1538, Text!=NULL);
	TASSERT1(1539, Coordinate.x>0, Coordinate.x);
	TASSERT1(1540, Coordinate.y>0, Coordinate.y);
	TAutomapBlock *Block = GetAutomapBlock(Coordinate.x / 256, Coordinate.y / 256, Coordinate.z);
	TMark Mark;
	Mark.x = Coordinate.x;
	Mark.y = Coordinate.y;
	Mark.Type = Type;
	CopyString(Mark.Text.Chars, Text);
	Block->Marks(Block->MarkCount) = Mark;
	Block->MarkCount++;
	Block->Changed = true;
	AutomapChanged = true;
}

// 0x4b5410, line 1578. Deletes the marks at the position of Deleted by moving
// them to 0,0; only its x and y are read. Its caller, ContextMenus.cpp's mark
// menu, passes the mark it was opened for.
void DeleteAutomapMark(TMark &Deleted, int z)
{
	int i = 0;
	TAutomapBlock *Block = GetAutomapBlock(Deleted.x / 256, Deleted.y / 256, z);
	for (i = 0; i < Block->MarkCount; i++) {
		TMark &Mark = Block->Marks(i);
		if (Mark.x == Deleted.x && Mark.y == Deleted.y) {
			Mark.x = 0;
			Mark.y = 0;
			Block->Changed = true;
			AutomapChanged = true;
		}
	}
}

// --- Paths ----------------------------------------------------------------------

// 0x4bee00, line 1738. Finds a path of at most ten steps over the automap of
// the start's floor, searching from the destination (Dijkstra with an
// estimate, MinSpeed per step, sorted open list). Directions: 1 east, then
// counter-clockwise to 8 south-east. Returns -1 if the destination is above,
// -2 below, -3 more than 110 fields away, -4 without a path, 0 for no step,
// 1 for a complete path and 2 for one cut off after ten steps or at the
// edge of the visible map. Without MustReach a blocked destination ends the
// path before it. Errors get the parameters added; there is only this one
// handler.
int FindPath(int StartX, int StartY, int StartZ, int DestX, int DestY, int DestZ, bool MustReach,
	int *Steps, int *Directions, bool ForceDiagonal)
{
	try {
		if (DestZ < StartZ)
			return -1;
		if (DestZ > StartZ)
			return -2;
		const int DistanceX = abs(DestX - StartX);
		if (DistanceX > 110)
			return -3;
		const int DistanceY = abs(DestY - StartY);
		if (DistanceY > 110)
			return -3;
		if (DestX == StartX && DestY == StartY)
			return 0;
		if (DistanceY + DistanceX == 1) {
			if (DestX == StartX + 1 && DestY == StartY)
				Directions[0] = 1;
			if (DestX == StartX && DestY == StartY - 1)
				Directions[0] = 3;
			if (DestX == StartX - 1 && DestY == StartY)
				Directions[0] = 5;
			if (DestX == StartX && DestY == StartY + 1)
				Directions[0] = 7;
			*Steps = 1;
			return 1;
		}
		if (ForceDiagonal && DistanceX == 1 && DistanceY == 1) {
			if (DestX == StartX + 1 && DestY == StartY - 1)
				Directions[0] = 2;
			if (DestX == StartX - 1 && DestY == StartY - 1)
				Directions[0] = 4;
			if (DestX == StartX - 1 && DestY == StartY + 1)
				Directions[0] = 6;
			if (DestX == StartX + 1 && DestY == StartY + 1)
				Directions[0] = 8;
			*Steps = 1;
			return 1;
		}
		matrix<TPathNode> Nodes(-111, 111, -111, 111);
		int MinSpeed = 0x7fffffff;
		for (int x = -110; x <= 110; x++) {
			for (int y = -110; y <= 110; y++) {
				Nodes(x, y).Blocked = IsAutomapBlocked(StartX + x, StartY + y, StartZ);
				Nodes(x, y).Speed = GetAutomapSpeed(x + StartX, y + StartY, StartZ);
				if (Nodes(x, y).Speed <= MinSpeed)
					MinSpeed = Nodes(x, y).Speed;
				Nodes(x, y).x = x;
				Nodes(x, y).y = y;
				Nodes(x, y).Predecessor = nullptr;
				Nodes(x, y).Next = nullptr;
				Nodes(x, y).Cost = 0x7fffffff;
				Nodes(x, y).Estimate = 0x7fffffff;
			}
		}
		for (int x = -111; x <= 111; x++) {
			Nodes(x, -111).Blocked = true;
			Nodes(x, 111).Blocked = true;
		}
		for (int y = -110; y <= 110; y++) {
			Nodes(-111, y).Blocked = true;
			Nodes(111, y).Blocked = true;
		}
		Nodes(DestX - StartX, DestY - StartY).Cost = 0;
		TPathNode *Open = &Nodes(DestX - StartX, DestY - StartY);
		while (Open != nullptr) {
			TPathNode *Current = Open;
			Open = Current->Next;
			const int Cost = Current->Speed + Current->Cost;
			if (Cost >= Nodes(0, 0).Cost)
				break;
			for (int ox = -1; ox <= 1; ox++) {
				for (int oy = -1; oy <= 1; oy++) {
					if (ox == 0 && oy == 0)
						continue;
					TPathNode *Node = &Nodes(Current->x + ox, Current->y + oy);
					int NewCost = Cost;
					if (abs(oy) + abs(ox) != 1)
						NewCost = Cost + Current->Speed * 2;
					if (Node->Cost <= NewCost)
						continue;
					Node->Cost = NewCost;
					Node->Predecessor = Current;
					if ((Node->x == 0 && Node->y == 0) || Node->Blocked)
						continue;
					// Already in the open list: take it out. A node that is
					// not found is cut out after the last node anyway.
					if (Node->Estimate != 0x7fffffff) {
						if (Open == Node) {
							Open = Node->Next;
						} else {
							TPathNode *Help = Open;
							while (Help->Next != nullptr && Help->Next != Node)
								Help = Help->Next;
							Help->Next = Node->Next;
						}
					}
					Node->Estimate = (abs(Node->y) + abs(Node->x) - 1) * MinSpeed + Node->Speed + NewCost;
					if (Open == nullptr || Node->Estimate <= Open->Estimate) {
						Node->Next = Open;
						Open = Node;
					} else {
						TPathNode *Help = Open;
						while (Help->Next != nullptr && Help->Next->Estimate < Node->Estimate)
							Help = Help->Next;
						Node->Next = Help->Next;
						Help->Next = Node;
					}
				}
			}
		}
		if (Nodes(0, 0).Cost == 0x7fffffff)
			return -4;
		int Count = -1;
		int LastX = 0;
		int LastY = 0;
		const TPathNode *Node = &Nodes(0, 0);
		while (Node != nullptr) {
			if (!MustReach && Node->x == DestX - StartX && Node->y == DestY - StartY
				&& IsAutomapBlocked(DestX, DestY, DestZ)) {
				Node = nullptr;
				break;
			}
			if (Count >= 0) {
				if (Node->x == LastX + 1 && Node->y == LastY)
					Directions[Count] = 1;
				if (Node->x == LastX + 1 && Node->y == LastY - 1)
					Directions[Count] = 2;
				if (Node->x == LastX && Node->y == LastY - 1)
					Directions[Count] = 3;
				if (Node->x == LastX - 1 && Node->y == LastY - 1)
					Directions[Count] = 4;
				if (Node->x == LastX - 1 && Node->y == LastY)
					Directions[Count] = 5;
				if (Node->x == LastX - 1 && Node->y == LastY + 1)
					Directions[Count] = 6;
				if (Node->x == LastX && Node->y == LastY + 1)
					Directions[Count] = 7;
				if (Node->x == LastX + 1 && Node->y == LastY + 1)
					Directions[Count] = 8;
			}
			LastX = Node->x;
			LastY = Node->y;
			Node = Node->Predecessor;
			Count++;
			if (Count >= 10 || Node == nullptr || abs(Node->x) >= 9 || abs(Node->y) >= 7)
				break;
		}
		*Steps = Count;
		if (Count == 0)
			return 0;
		if (Node == nullptr)
			return 1;
		return 2;
	} catch (TMyException &Exception) {
		std::stringstream Text;
		Text << "Something failed" << " StartX: " << StartX << " StartY: " << StartY << " StartZ: " << StartZ
			 << " DestX: " << DestX << " DestY: " << DestY << " DestZ: " << DestZ
			 << " MustReach: " << (MustReach ? "true" : "false")
			 << " ForceDiagonal: " << (ForceDiagonal ? "true" : "false") << ")";
		throw TMYEXCEPTION(1738, Text.str().c_str(), "", 0, "", 0, Exception);
	}
}

// --- Initialisation --------------------------------------------------------------

// 0x4b5610, line 1784. No try block.
int GetAmbientBrightness()
{
	TASSERT1(1784, AmbienteActualBrightness>=0, AmbienteActualBrightness);
	return AmbienteActualBrightness;
}

// 0x4bf620, line 1830. Every map position gets its own point; the automap
// cache is empty. Callback receives the creatures ClearField removes.
void InitMap(void (*Callback)(int CreatureID))
{
	for (int x = 0; x < MAPSIZE_X; x++)
		for (int y = 0; y < MAPSIZE_Y; y++)
			for (int z = 0; z < MAPSIZE_Z; z++)
				FieldNumber(x, y, z) = x + (z * MAPSIZE_Y + y) * MAPSIZE_X;
	ClearMap();
	MapX = 0;
	MapY = 0;
	MapZ = 0;
	ObjectIteratorField = -1;
	for (int i = 0; i < AUTOMAP_BLOCKS; i++) {
		AutomapBlocks[i].Previous = i - 1;
		AutomapBlocks[i].bx = -1;
		AutomapBlocks[i].Next = i + 1;
	}
	AutomapBlocks[0].Previous = -1;
	AutomapBlocks[AUTOMAP_BLOCKS - 1].Next = -1;
	MostRecentBlock = 0;
	LeastRecentBlock = AUTOMAP_BLOCKS - 1;
	AutomapChanged = true;
	CreatureCallback = Callback;
}

// 0x4b5760, line 1842. Saves the cached automap blocks.
void ExitMap()
{
	int i = 0;
	for (i = 0; i < AUTOMAP_BLOCKS; i++)
		SaveAutomapBlock(i);
}

#if defined(PATELLA_TEST_MODE)
// The test mode's state dump (issue #22; src/testmode/ClientView.h) reads this file's data; it writes
// nothing. Test build only.
void patella::testmode::ViewMap(TMapView *View)
{
	View->MapX = &MapX;
	View->MapY = &MapY;
	View->MapZ = &MapZ;
	View->Fields = &Fields;
}
#endif
