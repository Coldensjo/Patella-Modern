// Inventory.cpp: reconstructed from Tibia.exe 7.72 (0x4a4460-0x4a81ff).
//
// The body slots, open containers and trade offers. The functions without
// line evidence come where the binary has them: four at the start (the gap
// after Help.cpp, 0x4a4460-0x4a4546, which uses only this file's data) and
// InitInventory before ResetInventory (line 400). The others are in line
// order; the compiler placed InsertContainerObject (lines 139-140) last.
//
// Most functions assert before doing anything and have no try block; the
// original's catch wrappers (common.h) around the calls to other functions of this file
// are gone (issue #78).

#include "Inventory.h"
#include "Objects.h"
#include "Utils.h"
#include "common.h"
#include "types/FixedString.h"
#include <array>
#include <cstring>


// The data (0x5ced60-0x5d1643) is TInventory's (Inventory.h, issue #106).

patella::TInventory::TInventory(TObjectTypes &ObjectTypesRef) : ObjectTypes(ObjectTypesRef) {}

// 0x4a4460
void patella::TInventory::ClearBody()
{
	for (int i = 0; i < BODY_POSITIONS; i++)
		Body[i] = ObjectTypes.NoObject;
}

// 0x4a4520
void patella::TInventory::ClearOffers()
{
	OwnOffer.Count = 0;
	CounterOffer.Count = 0;
}

// 0x4a4530
int patella::TInventory::GetOwnOfferCount() const
{
	return OwnOffer.Count;
}

// 0x4a4540
int patella::TInventory::GetCounterOfferCount() const
{
	return CounterOffer.Count;
}

// 0x4a4550, line 55. Positions count from 1.
void patella::TInventory::SetBodyObject(int Position, TObject Object)
{
	const int Pos = Position - 1;
	PATELLA_ASSERT_VALUES(In(Pos,0,BODY_POSITIONS-1), Pos);
	Body[Pos] = Object;
}

// 0x4a46d0, line 67
void patella::TInventory::DeleteBodyObject(int Position)
{
	SetBodyObject(Position, ObjectTypes.NoObject);
}

// 0x4a4880, lines 77-80
TObject *patella::TInventory::GetBodyObject(int Position)
{
	const int Pos = Position - 1;
	PATELLA_ASSERT_VALUES(In(Pos,0,BODY_POSITIONS-1), Pos);
	return &Body[Pos];
}

// 0x4a4b00, lines 102-105. The name is stored capitalised.
void patella::TInventory::OpenContainer(int Container, TObject Object, const char *Name, int Capacity, bool HasParent)
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT(!Containers[Container].IsOpen);
	PATELLA_ASSERT(Name!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Name)<30, strlen(Name));
	Containers[Container].IsOpen = true;
	Containers[Container].Object = Object;
	Containers[Container].Name.Assign(Name);
	CapitalizeString(Containers[Container].Name);
	Containers[Container].HasParent = HasParent;
	Containers[Container].Count = 0;
	Containers[Container].Capacity = Capacity;
}

// 0x4a4f70, lines 120-121
void patella::TInventory::CloseContainer(int Container)
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT(Containers[Container].IsOpen);
	Containers[Container].IsOpen = false;
}

// 0x4a5220, line 130
bool patella::TInventory::IsContainerOpen(int Container) const
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	return Containers[Container].IsOpen;
}

// 0x4a7f00, lines 139-140. Puts the object in front; a full container
// loses its last object first.
void patella::TInventory::InsertContainerObject(int Container, TObject Object)
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT(Containers[Container].IsOpen);
	if (Containers[Container].Count > CONTAINER_OBJECTS - 1)
		DeleteContainerObject(Container, Containers[Container].Count - 1);
	for (int i = Containers[Container].Count; i > 0; i--)
		Containers[Container].Objects[i] = Containers[Container].Objects[i - 1];
	Containers[Container].Objects[0] = Object;
	Containers[Container].Count++;
}

// 0x4a5380, lines 153-154. Ignored when the container is full.
void patella::TInventory::AppendContainerObject(int Container, TObject Object)
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	if (Containers[Container].Count < CONTAINER_OBJECTS) {
		Containers[Container].Objects[Containers[Container].Count] = Object;
		Containers[Container].Count++;
	}
}

// 0x4a5630, lines 165-166. Ignored for a position outside the contents.
void patella::TInventory::ChangeContainerObject(int Container, int Position, TObject Object)
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	if (In(Position, 0, Containers[Container].Count - 1))
		Containers[Container].Objects[Position] = Object;
}

// 0x4a58e0, lines 177-178. True when the container was full, so the
// server can send the object that moves up into view.
bool patella::TInventory::DeleteContainerObject(int Container, int Position)
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	if (!In(Position, 0, Containers[Container].Count - 1))
		return false;
	for (int i = Position; i < Containers[Container].Count - 1; i++)
		Containers[Container].Objects[i] = Containers[Container].Objects[i + 1];
	Containers[Container].Count--;
	return Containers[Container].Count == CONTAINER_OBJECTS - 1;
}

// 0x4a5bd0, lines 191-192
int patella::TInventory::GetContainerObjectCount(int Container) const
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	return Containers[Container].Count;
}

// 0x4a5e80, lines 201-202. The container itself.
TObject patella::TInventory::GetContainerObject(int Container) const
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	return Containers[Container].Object;
}

// 0x4a6150, lines 211-213
TObject patella::TInventory::GetContainerContent(int Container, int Position) const
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	PATELLA_ASSERT_VALUES(In(Position,0,Containers[Container].Count-1), Position, Containers[Container].Count-1);
	return Containers[Container].Objects[Position];
}

// 0x4a64e0, lines 222-223
char *patella::TInventory::GetContainerName(int Container)
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	return Containers[Container].Name;
}

// 0x4a6790, lines 232-233
int patella::TInventory::GetContainerCapacity(int Container) const
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	return Containers[Container].Capacity;
}

// 0x4a6a40, lines 242-243
bool patella::TInventory::GetContainerHasParent(int Container) const
{
	PATELLA_ASSERT_VALUES(In(Container,0,OPEN_CONTAINERS-1), Container);
	PATELLA_ASSERT_VALUES(Containers[Container].IsOpen, Container);
	return Containers[Container].HasParent;
}

// 0x4a6cf0, line 258. The first closed container; 15 when all are open.
int patella::TInventory::GetFreeContainer() const
{
	int i = 0;
	for (i = 0; i < OPEN_CONTAINERS; i++) {
		if (!IsContainerOpen(i))
			return i;
	}
	return OPEN_CONTAINERS - 1;
}

// 0x4a6eb0, lines 281-283
bool patella::TInventory::SetOwnOffer(const char *Name)
{
	PATELLA_ASSERT_VALUES(OwnOffer.Count==0, OwnOffer.Count);
	PATELLA_ASSERT(Name!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Name)<30, strlen(Name));
	OwnOffer.Name.Assign(Name);
	return true;
}

// 0x4a7230, lines 296-297. False while a counter offer is shown.
bool patella::TInventory::SetCounterOffer(const char *Name)
{
	if (CounterOffer.Count != 0)
		return false;
	PATELLA_ASSERT(Name!=nullptr);
	PATELLA_ASSERT_VALUES(strlen(Name)<30, strlen(Name));
	CounterOffer.Name.Assign(Name);
	return true;
}

// 0x4a7460, line 307
void patella::TInventory::AddOwnOfferObject(TObject Object)
{
	PATELLA_ASSERT_VALUES(OwnOffer.Count<OBJECTS_PER_OFFER, OwnOffer.Count);
	OwnOffer.Objects[OwnOffer.Count] = Object;
	OwnOffer.Count++;
}

// 0x4a75e0, line 317
void patella::TInventory::AddCounterOfferObject(TObject Object)
{
	PATELLA_ASSERT_VALUES(CounterOffer.Count<OBJECTS_PER_OFFER, CounterOffer.Count);
	CounterOffer.Objects[CounterOffer.Count] = Object;
	CounterOffer.Count++;
}

// 0x4a7760, line 327
char *patella::TInventory::GetOwnOfferName()
{
	PATELLA_ASSERT_VALUES(OwnOffer.Count>0, OwnOffer.Count);
	return OwnOffer.Name;
}

// 0x4a78b0, line 336
char *patella::TInventory::GetCounterOfferName()
{
	PATELLA_ASSERT_VALUES(CounterOffer.Count>0, CounterOffer.Count);
	return CounterOffer.Name;
}

// 0x4a7a00, line 361
TObject patella::TInventory::GetOwnOfferObject(int Position) const
{
	PATELLA_ASSERT_VALUES(In(Position,0,OwnOffer.Count-1), Position, OwnOffer.Count-1);
	return OwnOffer.Objects[Position];
}

// 0x4a7b90, line 370
TObject patella::TInventory::GetCounterOfferObject(int Position) const
{
	PATELLA_ASSERT_VALUES(In(Position,0,CounterOffer.Count-1), Position, CounterOffer.Count-1);
	return CounterOffer.Objects[Position];
}

// 0x4a7d20
void patella::TInventory::InitInventory()
{
	ClearBody();
	for (int i = 0; i < OPEN_CONTAINERS; i++)
		Containers[i].IsOpen = false;
	OwnOffer.Count = 0;
	CounterOffer.Count = 0;
}

// 0x4a7d70, line 400
void patella::TInventory::ResetInventory()
{
	InitInventory();
}

// Empty; the linker kept one copy of the identical empty functions (0x496160,
// GUI.obj). ExitControl calls it where Inventory.cpp's exit belongs in the reverse
// order of InitControl (name ours, low confidence).
void patella::TInventory::ExitInventory()
{
}

#if defined(PATELLA_TEST_MODE)
// The test mode's state dump (issue #22; src/testmode/ClientView.h) reads this file's data; it writes
// nothing. Test build only.
void patella::TInventory::View(testmode::TInventoryView *Target) const
{
	Target->Body = Body.data();
	Target->Containers = Containers.data();
	Target->OwnOffer = &OwnOffer;
	Target->CounterOffer = &CounterOffer;
}

void patella::testmode::ViewInventory(TInventoryView *View)
{
	patella::bridge::Inventory().View(View);
}
#endif

// The original's functions (issue #106): each calls its member on the one TClient's TInventory.

void ClearBody()
{
	patella::bridge::Inventory().ClearBody();
}

void ClearOffers()
{
	patella::bridge::Inventory().ClearOffers();
}

int GetOwnOfferCount()
{
	return patella::bridge::Inventory().GetOwnOfferCount();
}

int GetCounterOfferCount()
{
	return patella::bridge::Inventory().GetCounterOfferCount();
}

void SetBodyObject(int Position, TObject Object)
{
	patella::bridge::Inventory().SetBodyObject(Position, Object);
}

void DeleteBodyObject(int Position)
{
	patella::bridge::Inventory().DeleteBodyObject(Position);
}

TObject *GetBodyObject(int Position)
{
	return patella::bridge::Inventory().GetBodyObject(Position);
}

void OpenContainer(int Container, TObject Object, const char *Name, int Capacity, bool HasParent)
{
	patella::bridge::Inventory().OpenContainer(Container, Object, Name, Capacity, HasParent);
}

void CloseContainer(int Container)
{
	patella::bridge::Inventory().CloseContainer(Container);
}

bool IsContainerOpen(int Container)
{
	return patella::bridge::Inventory().IsContainerOpen(Container);
}

void InsertContainerObject(int Container, TObject Object)
{
	patella::bridge::Inventory().InsertContainerObject(Container, Object);
}

void AppendContainerObject(int Container, TObject Object)
{
	patella::bridge::Inventory().AppendContainerObject(Container, Object);
}

void ChangeContainerObject(int Container, int Position, TObject Object)
{
	patella::bridge::Inventory().ChangeContainerObject(Container, Position, Object);
}

bool DeleteContainerObject(int Container, int Position)
{
	return patella::bridge::Inventory().DeleteContainerObject(Container, Position);
}

int GetContainerObjectCount(int Container)
{
	return patella::bridge::Inventory().GetContainerObjectCount(Container);
}

TObject GetContainerObject(int Container)
{
	return patella::bridge::Inventory().GetContainerObject(Container);
}

TObject GetContainerContent(int Container, int Position)
{
	return patella::bridge::Inventory().GetContainerContent(Container, Position);
}

char *GetContainerName(int Container)
{
	return patella::bridge::Inventory().GetContainerName(Container);
}

int GetContainerCapacity(int Container)
{
	return patella::bridge::Inventory().GetContainerCapacity(Container);
}

bool GetContainerHasParent(int Container)
{
	return patella::bridge::Inventory().GetContainerHasParent(Container);
}

int GetFreeContainer()
{
	return patella::bridge::Inventory().GetFreeContainer();
}

bool SetOwnOffer(const char *Name)
{
	return patella::bridge::Inventory().SetOwnOffer(Name);
}

bool SetCounterOffer(const char *Name)
{
	return patella::bridge::Inventory().SetCounterOffer(Name);
}

void AddOwnOfferObject(TObject Object)
{
	patella::bridge::Inventory().AddOwnOfferObject(Object);
}

void AddCounterOfferObject(TObject Object)
{
	patella::bridge::Inventory().AddCounterOfferObject(Object);
}

char *GetOwnOfferName()
{
	return patella::bridge::Inventory().GetOwnOfferName();
}

char *GetCounterOfferName()
{
	return patella::bridge::Inventory().GetCounterOfferName();
}

TObject GetOwnOfferObject(int Position)
{
	return patella::bridge::Inventory().GetOwnOfferObject(Position);
}

TObject GetCounterOfferObject(int Position)
{
	return patella::bridge::Inventory().GetCounterOfferObject(Position);
}

void InitInventory()
{
	patella::bridge::Inventory().InitInventory();
}

void ResetInventory()
{
	patella::bridge::Inventory().ResetInventory();
}

void ExitInventory()
{
	patella::bridge::Inventory().ExitInventory();
}
