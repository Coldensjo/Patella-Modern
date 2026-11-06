// TMapPoint's stack of objects (src/MapPoint.h, issue #102): its objects a TStaticVector<TObject, OBJECTS_PER_POINT>
// (std::inplace_vector with GCC 16), with the original's behaviour on a full point, one test case each: InsertObject's
// shift capped at index 9 (the top object falls off), AddObject's drop (AddPosition gives OBJECTS_PER_POINT), AppendObject's
// throw (CheckPointRoom), DeleteObject's true when the point was full; and the original's Objects[0] of an empty point
// (Vacated). The last case runs random operations against the original's point, ObjectCount and an array of ten objects
// changed as 0x4b8310 (InsertObject), 0x4b9420 (DeleteObject) and 0x4bc720 (ClearField) change them. The golden corpus's
// Map test replays the same operations against the original through Map.cpp's public functions.

#include <doctest/doctest.h>

#include "MapPoint.h"
#include "TMyException.h"

#include <array>
#include <cstdint>
#include <string>

namespace {

constexpr int GROUND = 1; // a type with BANK in the tests' flags

bool TestFlag(int Type, int Flag)
{
	return Type == GROUND && Flag == BANK;
}

TObject Item(int Type, int Data = 0)
{
	TObject Obj;
	Obj.Type = Type;
	Obj.Data = Data;
	return Obj;
}

TMapPoint FullPoint()
{
	TMapPoint Point{};
	for (int i = 0; i < OBJECTS_PER_POINT; i++)
		InsertPointObject(Point, i, Item(100 + i));
	return Point;
}

bool Same(const TObject &A, const TObject &B)
{
	return A.Type == B.Type && A.Data == B.Data && A.Data2 == B.Data2;
}

// The original's point: ObjectCount and the array, changed as the binary changes them.
struct TOriginalPoint {
	int ObjectCount = 0;
	std::array<TObject, OBJECTS_PER_POINT> Objects{};

	void Insert(int Position, const TObject &Obj)
	{
		int i = ObjectCount;
		if (i > OBJECTS_PER_POINT - 1)
			i = OBJECTS_PER_POINT - 1;
		for (; i > Position; i--)
			Objects[static_cast<std::size_t>(i)] = Objects[static_cast<std::size_t>(i - 1)];
		Objects[static_cast<std::size_t>(Position)] = Obj;
		if (ObjectCount < OBJECTS_PER_POINT)
			ObjectCount++;
	}

	bool Delete(int Position)
	{
		for (; Position < ObjectCount - 1; Position++)
			Objects[static_cast<std::size_t>(Position)] = Objects[static_cast<std::size_t>(Position + 1)];
		ObjectCount--;
		return ObjectCount == OBJECTS_PER_POINT - 1;
	}
};

} // namespace

TEST_CASE("TMapPoint: an insert into a full point drops the top object (InsertObject's cap at index 9)")
{
	TMapPoint Point = FullPoint();
	InsertPointObject(Point, 3, Item(7));
	REQUIRE(PointObjectCount(Point) == OBJECTS_PER_POINT);
	const int Expected[OBJECTS_PER_POINT] = {100, 101, 102, 7, 103, 104, 105, 106, 107, 108}; // 109 fell off
	for (int i = 0; i < OBJECTS_PER_POINT; i++)
		CHECK(PointObject(Point, i).Type == Expected[i]);

	InsertPointObject(Point, OBJECTS_PER_POINT - 1, Item(8)); // at the top: it replaces the top object
	CHECK(PointObjectCount(Point) == OBJECTS_PER_POINT);
	CHECK(PointObject(Point, 8).Type == 107);
	CHECK(PointObject(Point, 9).Type == 8);

	TMapPoint Small{};
	InsertPointObject(Small, 0, Item(5));
	InsertPointObject(Small, 0, Item(6));
	CHECK(PointObjectCount(Small) == 2);
	CHECK(PointObject(Small, 0).Type == 6);
	CHECK(PointObject(Small, 1).Type == 5);
}

TEST_CASE("TMapPoint: AddObject drops an object that belongs above every object of a full point")
{
	TMapPoint Grounds{};
	for (int i = 0; i < OBJECTS_PER_POINT; i++)
		InsertPointObject(Grounds, i, Item(GROUND));
	CHECK(AddPosition(Grounds, Item(200), TestFlag) == OBJECTS_PER_POINT); // dropped: AddObject inserts below 10 only
	CHECK(AddPosition(Grounds, Item(GROUND), TestFlag) == 0);              // a ground goes in at the bottom: the top falls off
	CHECK(AddPosition(Grounds, Item(CREATURE), TestFlag) == OBJECTS_PER_POINT);

	// Not full (ground, creature, item): a new item goes above the creatures, a new creature at the first creature, a
	// new ground at the first ground.
	TMapPoint Point{};
	InsertPointObject(Point, 0, Item(GROUND));
	InsertPointObject(Point, 1, Item(CREATURE));
	InsertPointObject(Point, 2, Item(200));
	CHECK(AddPosition(Point, Item(201), TestFlag) == 2);
	CHECK(AddPosition(Point, Item(CREATURE), TestFlag) == 1);
	CHECK(AddPosition(Point, Item(GROUND), TestFlag) == 0);

	// A full point of items: a new item goes in at the bottom of them (and InsertPointObject drops the top object).
	TMapPoint Full = FullPoint();
	CHECK(AddPosition(Full, Item(300), TestFlag) == 0);
}

TEST_CASE("TMapPoint: AppendObject throws on a full point")
{
	TMapPoint Point = FullPoint();
	try {
		CheckPointRoom(Point, 1, 2, 3);
		FAIL("no exception");
	} catch (const TMyException &Exception) {
		const std::string Message = Exception.GetMessage();
		CHECK(Message.find("too many objects on map point") != std::string::npos);
		CHECK(Message.find("ObjectCount: 10 rx: 1 ry: 2 rz: 3") != std::string::npos);
	}
	CHECK(DeletePointObject(Point, 0));
	CHECK_NOTHROW(CheckPointRoom(Point, 1, 2, 3));
	TMapPoint Empty{};
	CHECK_NOTHROW(CheckPointRoom(Empty, 0, 0, 0));
}

TEST_CASE("TMapPoint: DeleteObject's result is true when the point was full")
{
	TMapPoint Point = FullPoint();
	CHECK(DeletePointObject(Point, 4));
	CHECK(PointObjectCount(Point) == OBJECTS_PER_POINT - 1);
	CHECK(PointObject(Point, 4).Type == 105);
	CHECK_FALSE(DeletePointObject(Point, 0));
	CHECK(PointObjectCount(Point) == OBJECTS_PER_POINT - 2);
	CHECK(PointObject(Point, 0).Type == 101);
}

TEST_CASE("TMapPoint: an empty point's Objects[0] is the last bottom object, as the original's array kept it")
{
	TMapPoint Point{};
	CHECK(Same(PointObject(Point, 0), TObject())); // the cleared objects of new TMapPoint[n]
	InsertPointObject(Point, 0, Item(5, 1));
	InsertPointObject(Point, 1, Item(6, 2));
	CHECK_FALSE(DeletePointObject(Point, 0));
	CHECK_FALSE(DeletePointObject(Point, 0));
	CHECK(PointObjectCount(Point) == 0);
	CHECK(Same(PointObject(Point, 0), Item(6, 2)));

	InsertPointObject(Point, 0, Item(7, 3));
	InsertPointObject(Point, 0, Item(8, 4));
	ClearPointObjects(Point);
	CHECK(PointObjectCount(Point) == 0);
	CHECK(Same(PointObject(Point, 0), Item(8, 4)));
	ClearPointObjects(Point); // already empty: unchanged
	CHECK(Same(PointObject(Point, 0), Item(8, 4)));
}

TEST_CASE("TMapPoint: random operations give the original's objects, count, results and Objects[0]")
{
	TMapPoint Point{};
	TOriginalPoint Original;
	std::uint32_t Seed = 1022;
	const auto Random = [&Seed](std::uint32_t Range) {
		Seed = Seed * 1103515245U + 12345U;
		return static_cast<int>((Seed >> 8) % Range);
	};
	int Full = 0;
	for (int Step = 0; Step < 20000; Step++) {
		const int Operation = Random(100);
		const int Count = Original.ObjectCount;
		if (Operation < 60) {
			const int Position = Random(static_cast<std::uint32_t>((Count < OBJECTS_PER_POINT ? Count : OBJECTS_PER_POINT - 1) + 1));
			const TObject Obj = Item(1 + Random(500), Step);
			Full += Count == OBJECTS_PER_POINT ? 1 : 0;
			InsertPointObject(Point, Position, Obj);
			Original.Insert(Position, Obj);
		} else if (Operation < 98) {
			if (Count > 0) {
				const int Position = Random(static_cast<std::uint32_t>(Count));
				REQUIRE(DeletePointObject(Point, Position) == Original.Delete(Position));
			}
		} else {
			ClearPointObjects(Point);
			Original.ObjectCount = 0;
		}
		REQUIRE(PointObjectCount(Point) == Original.ObjectCount);
		bool SameObjects = true;
		for (int i = 0; i < Original.ObjectCount; i++)
			SameObjects = SameObjects && Same(PointObject(Point, i), Original.Objects[static_cast<std::size_t>(i)]);
		REQUIRE(SameObjects);
		REQUIRE(Same(PointObject(Point, 0), Original.Objects[0])); // also on an empty point (GetTop*Object)
	}
	CHECK(Full > 0); // the run met full points
}
