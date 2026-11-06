// MapPoint.h: a map point's stack of objects (Patella-Modern issue #102, plan M7's fixed-capacity lists): TMapPoint
// and the operations on its stack that Map.cpp's InsertObject, AddObject, AppendObject, DeleteObject, ClearField and
// the GetTop*Object functions make of it. Not part of the original source; TMapPoint, ObjectCount and EffectNumber are
// the original's names (Map.h before #102).
//
// The original's point (0xac bytes) held ObjectCount and an array of OBJECTS_PER_POINT objects; here the objects are a
// TStaticVector<TObject, OBJECTS_PER_POINT> (compat/cxx26.h: std::inplace_vector where the library has it), its size
// the count. What happens when the stack is full is the original's (notes/decisions.md, "#102b"):
//	- InsertObject (0x4b8310) shifts the objects from Position up by one, but from index 9 at most: on a full point the
//	  top object falls off and the count stays OBJECTS_PER_POINT (InsertPointObject).
//	- AddObject (0x4b88f0) inserts only when its position is below OBJECTS_PER_POINT: an object that belongs above
//	  every object of a full point is dropped (AddPosition gives OBJECTS_PER_POINT then).
//	- AppendObject (0x4b8ec0) throws "too many objects on map point" on a full point (CheckPointRoom).
//	- DeleteObject (0x4b9420) returns true when the point was full (DeletePointObject).
// The original's array kept the objects beyond the count. One of them is read: the GetTop*Object functions return
// Objects[0] of an empty point, the last object that was at the bottom when the point became empty (or the cleared
// object of the start). Vacated holds it (PointObject), set where the point becomes empty: the last DeletePointObject
// and ClearPointObjects.

#pragma once

#include "common.h"
#include "Objects.h" // TObject, the flags
#include "compat/cxx26.h"
#include <array>
#include <cstddef>
#include <sstream>

inline constexpr int OBJECTS_PER_POINT = 10;

// An entry of the effect table (Map.h), as the map points and the entries' Previous and Next links hold it (issue
// #103; ints until then). Its value is the original's effect number, the entry's index (0 to EFFECTS - 1), which the
// golden corpus records; None is the original's -1: no effect (the end of a point's list, a full table).
enum class TEffectId : int { None = -1 };

// The original's effect number of an id, and the id of a number (the original's functions take and give numbers).
[[nodiscard]] constexpr int EffectNumberOf(TEffectId Id) noexcept
{
	return static_cast<int>(Id);
}
[[nodiscard]] constexpr TEffectId EffectIdOf(int Number) noexcept
{
	return static_cast<TEffectId>(Number);
}

// A map point; "Point" in the assertions.
struct TMapPoint {
	patella::TStaticVector<TObject, OBJECTS_PER_POINT> Objects; // bottom to top; the original's ObjectCount is its size
	std::array<int, OBJECTS_PER_POINT> Order;                   // drawing order (SortObjects)
	TEffectId EffectNumber;                                     // first effect on the point, None if there is none
	bool RefreshRequested;
	TObject Vacated; // the original's Objects[0] while the point is empty (above)
};

// The number of objects, the original's ObjectCount.
[[nodiscard]] inline int PointObjectCount(const TMapPoint &Point) noexcept
{
	return static_cast<int>(Point.Objects.size());
}

// The original's Objects[Position]: an object of the stack, or Vacated for position 0 of an empty point.
[[nodiscard]] inline const TObject &PointObject(const TMapPoint &Point, int Position) noexcept
{
	if (Position < PointObjectCount(Point))
		return Point.Objects[static_cast<std::size_t>(Position)];
	return Point.Vacated;
}

// InsertObject's stack operation: Obj at Position (0 to the count); on a full point the top object falls off.
inline void InsertPointObject(TMapPoint &Point, int Position, const TObject &Obj)
{
	if (PointObjectCount(Point) >= OBJECTS_PER_POINT)
		Point.Objects.pop_back();
	Point.Objects.insert(Point.Objects.begin() + Position, Obj);
}

// DeleteObject's stack operation: removes the object at Position (below the count); true when the point was full.
inline bool DeletePointObject(TMapPoint &Point, int Position)
{
	const bool WasFull = PointObjectCount(Point) == OBJECTS_PER_POINT;
	if (PointObjectCount(Point) == 1)
		Point.Vacated = Point.Objects[0];
	Point.Objects.erase(Point.Objects.begin() + Position);
	return WasFull;
}

// ClearField's and ClearMap's: no object left.
inline void ClearPointObjects(TMapPoint &Point) noexcept
{
	if (!Point.Objects.empty())
		Point.Vacated = Point.Objects[0];
	Point.Objects.clear();
}

// AddObject's position for Obj: ground objects first, then borders, bottom objects, top objects, and new creatures
// above the creatures, other objects below them. OBJECTS_PER_POINT on a full point when Obj belongs on top: AddObject
// then drops it. GetFlag is GetObjectFlag (a parameter, so that the tests need no Tibia.dat).
template <class FlagFunction>
[[nodiscard]] int AddPosition(const TMapPoint &Point, const TObject &Obj, FlagFunction GetFlag)
{
	int Position;
	for (Position = 0; Position < PointObjectCount(Point); Position++) {
		const TObject &Here = Point.Objects[static_cast<std::size_t>(Position)];
		if (GetFlag(Obj.Type, BANK) && !GetFlag(Here.Type, BANK))
			break;
		if (GetFlag(Obj.Type, BANK) == GetFlag(Here.Type, BANK)) {
			if (GetFlag(Obj.Type, CLIP) && !GetFlag(Here.Type, CLIP))
				break;
			if (GetFlag(Obj.Type, CLIP) == GetFlag(Here.Type, CLIP)) {
				if (GetFlag(Obj.Type, BOTTOM) && !GetFlag(Here.Type, BOTTOM))
					break;
				if (GetFlag(Obj.Type, BOTTOM) == GetFlag(Here.Type, BOTTOM)) {
					if (GetFlag(Obj.Type, TOP) && !GetFlag(Here.Type, TOP))
						break;
					if (GetFlag(Obj.Type, TOP) == GetFlag(Here.Type, TOP)
						&& (Obj.Type == CREATURE || Here.Type != CREATURE))
						break;
				}
			}
		}
	}
	return Position;
}

// AppendObject's check: a full point throws "too many objects on map point".
inline void CheckPointRoom(const TMapPoint &Point, int rx, int ry, int rz)
{
	if (!In(PointObjectCount(Point), 0, OBJECTS_PER_POINT - 1)) {
		std::stringstream s;
		s << "ObjectCount: " << PointObjectCount(Point) << " rx: " << rx << " ry: " << ry << " rz: " << rz;
		throw TMYEXCEPTION("too many objects on map point", "", 0, "", 0, s.str());
	}
}
