// difftest.exe's Inventory test (moved here from tools/difftest/inventorytest.cpp, issue #26):
// Inventory.cpp against the original, as a golden-corpus test (golden.h).
//
// Random sequences of operations on both sides, generated so that no
// assertion fails; after each operation the observable state (body slots,
// containers, offers) is compared through the getters of both sides. The
// original's objects (TObject, three ints) are recorded by value.

#include "golden_test.h"

#include "Inventory.h"
#include "Objects.h"

#include <string.h>


namespace {

static_assert(sizeof(TObject) == 12, "TObject: three ints on both sides");

bool Same(TObject a, TObject b)
{
	return a.Type == b.Type && a.Data == b.Data && a.Data2 == b.Data2;
}

// An object of the original, by value.
TObject Original(const char *Name, [[maybe_unused]] TObject Object)
{
	int Fields[3] = {};
	ORIGINAL(Fields[0] = Object.Type; Fields[1] = Object.Data; Fields[2] = Object.Data2);
	golden::Output(Name, Fields);
	TObject Result;
	Result.Type = Fields[0];
	Result.Data = Fields[1];
	Result.Data2 = Fields[2];
	return Result;
}

TObject RandomObject()
{
	TObject Object = {(int)(Random() % 4000), (int)(Random() % 100), (int)(Random() % 3)};
	return Object;
}

void CompareState(const char *What)
{
	for (int Position = 1; Position <= BODY_POSITIONS; Position++) {
		TObject Body = Original("body", ORIGINAL_OR(*At<GetBodyFn>(0x4a4880)(Position), TObject()));
		CHECK(Same(*GetBodyObject(Position), Body), "body %d after %s", Position, What);
	}
	for (int c = 0; c < OPEN_CONTAINERS; c++) {
		bool Open = IsContainerOpen(c);
		bool OriginalOpen = ORIGINAL_VALUE("container open", bool, At<BoolIntFn>(0x4a5220)(c));
		CHECK(Open == OriginalOpen, "container %d open after %s", c, What);
		if (!Open)
			continue;
		int Count = GetContainerObjectCount(c);
		int OriginalCount = ORIGINAL_VALUE("container count", int, At<IntIntFn>(0x4a5bd0)(c));
		CHECK(Count == OriginalCount, "container %d count after %s", c, What);
		TObject Container = Original("container object", ORIGINAL_OR(At<GetContainerFn>(0x4a5e80)(c), TObject()));
		CHECK(Same(GetContainerObject(c), Container), "container %d object", c);
		golden::TText Name = ORIGINAL_TEXT("container name", At<NameFn>(0x4a64e0)(c));
		CHECK(strcmp(GetContainerName(c), Name.c_str()) == 0, "container %d name '%s' '%s'", c, GetContainerName(c),
			Name.c_str());
		int Capacity = ORIGINAL_VALUE("container capacity", int, At<IntIntFn>(0x4a6790)(c));
		CHECK(GetContainerCapacity(c) == Capacity, "container %d capacity", c);
		bool Parent = ORIGINAL_VALUE("container parent", bool, At<BoolIntFn>(0x4a6a40)(c));
		CHECK(GetContainerHasParent(c) == Parent, "container %d parent", c);
		for (int p = 0; p < Count; p++) {
			TObject Content = Original("container content", ORIGINAL_OR(At<GetContentFn>(0x4a6150)(c, p), TObject()));
			CHECK(Same(GetContainerContent(c, p), Content), "container %d slot %d after %s", c, p, What);
		}
	}
	int Free = ORIGINAL_VALUE("GetFreeContainer", int, At<IntFn>(0x4a6cf0)());
	CHECK(GetFreeContainer() == Free, "GetFreeContainer after %s", What);
	int Own = GetOwnOfferCount(), Counter = GetCounterOfferCount();
	int OriginalOwn = ORIGINAL_VALUE("own offer count", int, At<IntFn>(0x4a4530)());
	int OriginalCounter = ORIGINAL_VALUE("counter offer count", int, At<IntFn>(0x4a4540)());
	CHECK(Own == OriginalOwn && Counter == OriginalCounter, "offer counts after %s", What);
	if (Own > 0) {
		golden::TText Name = ORIGINAL_TEXT("own offer name", At<OfferNameFn>(0x4a7760)());
		CHECK(strcmp(GetOwnOfferName(), Name.c_str()) == 0, "own offer name");
		for (int p = 0; p < Own; p++) {
			TObject Object = Original("own offer", ORIGINAL_OR(At<OfferObjectFn>(0x4a7a00)(p), TObject()));
			CHECK(Same(GetOwnOfferObject(p), Object), "own offer %d", p);
		}
	}
	if (Counter > 0) {
		golden::TText Name = ORIGINAL_TEXT("counter offer name", At<OfferNameFn>(0x4a78b0)());
		CHECK(strcmp(GetCounterOfferName(), Name.c_str()) == 0, "counter offer name");
		for (int p = 0; p < Counter; p++) {
			TObject Object = Original("counter offer", ORIGINAL_OR(At<OfferObjectFn>(0x4a7b90)(p), TObject()));
			CHECK(Same(GetCounterOfferObject(p), Object), "counter offer %d", p);
		}
	}
}

} // namespace

void TestInventory()
{
	ORIGINAL(PatchImage());
	InitInventory();
	ORIGINAL(At<VoidFn>(0x4a7d20)());
	CompareState("InitInventory");
	for (int Step = 0; Step < 20000; Step++) {
		int c = (int)(Random() % OPEN_CONTAINERS);
		bool Open = IsContainerOpen(c);
		const char *What = "";
		switch (Random() % 14) {
		case 0: {
			int Position = 1 + (int)(Random() % BODY_POSITIONS);
			TObject Object = RandomObject();
			SetBodyObject(Position, Object);
			ORIGINAL(At<SetBodyFn>(0x4a4550)(Position, Object));
			What = "SetBodyObject";
			break;
		}
		case 1: {
			int Position = 1 + (int)(Random() % BODY_POSITIONS);
			DeleteBodyObject(Position);
			ORIGINAL(At<void(__cdecl *)(int)>(0x4a46d0)(Position));
			What = "DeleteBodyObject";
			break;
		}
		case 2:
			if (!Open) {
				char Name[30];
				RandomText(Name, (int)(Random() % 30));
				TObject Object = RandomObject();
				int Capacity = (int)(Random() % 40);
				bool Parent = (Random() & 1) != 0;
				OpenContainer(c, Object, Name, Capacity, Parent);
				ORIGINAL(At<OpenFn>(0x4a4b00)(c, Object, Name, Capacity, Parent));
				What = "OpenContainer";
			} else if (Random() % 3 == 0) {
				CloseContainer(c);
				ORIGINAL(At<void(__cdecl *)(int)>(0x4a4f70)(c));
				What = "CloseContainer";
			}
			break;
		case 3:
		case 4:
			if (Open) {
				TObject Object = RandomObject();
				AppendContainerObject(c, Object);
				ORIGINAL(At<ContainerObjectFn>(0x4a5380)(c, Object));
				What = "AppendContainerObject";
			}
			break;
		case 5:
			if (Open) {
				TObject Object = RandomObject();
				InsertContainerObject(c, Object);
				ORIGINAL(At<ContainerObjectFn>(0x4a7f00)(c, Object));
				What = "InsertContainerObject";
			}
			break;
		case 6:
			if (Open) {
				int Position = (int)(Random() % 40) - 2;
				TObject Object = RandomObject();
				ChangeContainerObject(c, Position, Object);
				ORIGINAL(At<ChangeFn>(0x4a5630)(c, Position, Object));
				What = "ChangeContainerObject";
			}
			break;
		case 7:
			if (Open) {
				int Position = (int)(Random() % 40) - 2;
				bool A = DeleteContainerObject(c, Position);
				bool B = ORIGINAL_VALUE("DeleteContainerObject", bool, At<DeleteFn>(0x4a58e0)(c, Position));
				CHECK(A == B, "DeleteContainerObject(%d, %d) %d %d", c, Position, A, B);
				What = "DeleteContainerObject";
			}
			break;
		case 8:
			if (GetOwnOfferCount() == 0) {
				char Name[30];
				RandomText(Name, (int)(Random() % 30));
				bool A = SetOwnOffer(Name);
				bool B = ORIGINAL_VALUE("SetOwnOffer", bool, At<SetOfferFn>(0x4a6eb0)(Name));
				CHECK(A == B, "SetOwnOffer");
				What = "SetOwnOffer";
			}
			break;
		case 9: {
			char Name[30];
			RandomText(Name, (int)(Random() % 30));
			bool A = SetCounterOffer(Name);
			bool B = ORIGINAL_VALUE("SetCounterOffer", bool, At<SetOfferFn>(0x4a7230)(Name));
			CHECK(A == B, "SetCounterOffer %d %d", A, B);
			What = "SetCounterOffer";
			break;
		}
		case 10:
			if (GetOwnOfferCount() < OBJECTS_PER_OFFER) {
				TObject Object = RandomObject();
				AddOwnOfferObject(Object);
				ORIGINAL(At<AddOfferFn>(0x4a7460)(Object));
				What = "AddOwnOfferObject";
			}
			break;
		case 11:
			if (GetCounterOfferCount() < OBJECTS_PER_OFFER) {
				TObject Object = RandomObject();
				AddCounterOfferObject(Object);
				ORIGINAL(At<AddOfferFn>(0x4a75e0)(Object));
				What = "AddCounterOfferObject";
			}
			break;
		case 12:
			if (Random() % 20 == 0) {
				ClearOffers();
				ORIGINAL(At<VoidFn>(0x4a4520)());
				What = "ClearOffers";
			}
			break;
		default:
			if (Random() % 50 == 0) {
				if (Random() & 1) {
					ResetInventory();
					ORIGINAL(At<VoidFn>(0x4a7d70)());
				} else {
					ClearBody();
					ORIGINAL(At<VoidFn>(0x4a4460)());
				}
				What = "reset";
			}
			break;
		}
		if (Step % 20 == 0)
			CompareState(What);
	}
	CompareState("the end");
}
