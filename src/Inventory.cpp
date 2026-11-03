// Inventory.cpp: reconstructed from Tibia.exe 7.72 (0x4a4460-0x4a81ff).
//
// The body slots, open containers and trade offers. The functions without
// line evidence come where the binary has them: four at the start (the gap
// after Help.cpp, 0x4a4460-0x4a4546, which uses only this file's data) and
// InitInventory before ResetInventory (line 400). The others are in line
// order; the compiler placed InsertContainerObject (lines 139-140) last.
//
// Most functions assert before doing anything and have no try block; the
// ones that call other functions of this file have TBEGIN/TEND.

#include "Inventory.h"
#include "Objects.h"
#include "Utils.h"
#include "common.h"
#include <cstddef>
#include <cstring>

// Data, in .bss order.
static TObject Body[BODY_POSITIONS];           // 0x5ced60
static TContainer Containers[OPEN_CONTAINERS]; // 0x5cedd8
static TOffer OwnOffer;                        // 0x5d0c98
static TOffer CounterOffer;                    // 0x5d1170 (4 bytes after OwnOffer's end)

// 0x4a4460
void ClearBody()
{
	for (int i = 0; i < BODY_POSITIONS; i++)
		Body[i] = NoObject;
}

// 0x4a4520
void ClearOffers()
{
	OwnOffer.Count = 0;
	CounterOffer.Count = 0;
}

// 0x4a4530
int GetOwnOfferCount()
{
	return OwnOffer.Count;
}

// 0x4a4540
int GetCounterOfferCount()
{
	return CounterOffer.Count;
}

// 0x4a4550, line 55. Positions count from 1.
void SetBodyObject(int Position, TObject Object)
{
	int Pos = Position - 1;
	TASSERT1(55, In(Pos,0,BODY_POSITIONS-1), Pos);
	Body[Pos] = Object;
}

// 0x4a46d0, line 67
void DeleteBodyObject(int Position)
{
	TBEGIN
	SetBodyObject(Position, NoObject);
	TEND1(67, Position)
}

// 0x4a4880, lines 77-80
TObject *GetBodyObject(int Position)
{
	TBEGIN
	int Pos = Position - 1;
	TASSERT1(77, In(Pos,0,BODY_POSITIONS-1), Pos);
	return &Body[Pos];
	TEND1(80, Position)
}

// 0x4a4b00, lines 102-105. The name is stored capitalised.
void OpenContainer(int Container, TObject Object, const char *Name, int Capacity, bool HasParent)
{
	TASSERT1(102, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT(103, !Containers[Container].IsOpen);
	TASSERT(104, Name!=NULL);
	TASSERT1(105, strlen(Name)<30, strlen(Name));
	Containers[Container].IsOpen = true;
	Containers[Container].Object = Object;
	strcpy(Containers[Container].Name, Name);
	CapitalizeString(Containers[Container].Name);
	Containers[Container].HasParent = HasParent;
	Containers[Container].Count = 0;
	Containers[Container].Capacity = Capacity;
}

// 0x4a4f70, lines 120-121
void CloseContainer(int Container)
{
	TASSERT1(120, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT(121, Containers[Container].IsOpen);
	Containers[Container].IsOpen = false;
}

// 0x4a5220, line 130
bool IsContainerOpen(int Container)
{
	TASSERT1(130, In(Container,0,OPEN_CONTAINERS-1), Container);
	return Containers[Container].IsOpen;
}

// 0x4a7f00, lines 139-140. Puts the object in front; a full container
// loses its last object first.
void InsertContainerObject(int Container, TObject Object)
{
	TASSERT1(139, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT(140, Containers[Container].IsOpen);
	if (Containers[Container].Count > CONTAINER_OBJECTS - 1)
		DeleteContainerObject(Container, Containers[Container].Count - 1);
	for (int i = Containers[Container].Count; i > 0; i--)
		Containers[Container].Objects[i] = Containers[Container].Objects[i - 1];
	Containers[Container].Objects[0] = Object;
	Containers[Container].Count++;
}

// 0x4a5380, lines 153-154. Ignored when the container is full.
void AppendContainerObject(int Container, TObject Object)
{
	TASSERT1(153, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(154, Containers[Container].IsOpen, Container);
	if (Containers[Container].Count < CONTAINER_OBJECTS) {
		Containers[Container].Objects[Containers[Container].Count] = Object;
		Containers[Container].Count++;
	}
}

// 0x4a5630, lines 165-166. Ignored for a position outside the contents.
void ChangeContainerObject(int Container, int Position, TObject Object)
{
	TASSERT1(165, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(166, Containers[Container].IsOpen, Container);
	if (In(Position, 0, Containers[Container].Count - 1))
		Containers[Container].Objects[Position] = Object;
}

// 0x4a58e0, lines 177-178. True when the container was full, so the
// server can send the object that moves up into view.
bool DeleteContainerObject(int Container, int Position)
{
	TASSERT1(177, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(178, Containers[Container].IsOpen, Container);
	if (!In(Position, 0, Containers[Container].Count - 1))
		return false;
	for (int i = Position; i < Containers[Container].Count - 1; i++)
		Containers[Container].Objects[i] = Containers[Container].Objects[i + 1];
	Containers[Container].Count--;
	return Containers[Container].Count == CONTAINER_OBJECTS - 1;
}

// 0x4a5bd0, lines 191-192
int GetContainerObjectCount(int Container)
{
	TASSERT1(191, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(192, Containers[Container].IsOpen, Container);
	return Containers[Container].Count;
}

// 0x4a5e80, lines 201-202. The container itself.
TObject GetContainerObject(int Container)
{
	TASSERT1(201, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(202, Containers[Container].IsOpen, Container);
	return Containers[Container].Object;
}

// 0x4a6150, lines 211-213
TObject GetContainerContent(int Container, int Position)
{
	TASSERT1(211, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(212, Containers[Container].IsOpen, Container);
	TASSERT2(213, In(Position,0,Containers[Container].Count-1), Position, Containers[Container].Count-1);
	return Containers[Container].Objects[Position];
}

// 0x4a64e0, lines 222-223
char *GetContainerName(int Container)
{
	TASSERT1(222, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(223, Containers[Container].IsOpen, Container);
	return Containers[Container].Name;
}

// 0x4a6790, lines 232-233
int GetContainerCapacity(int Container)
{
	TASSERT1(232, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(233, Containers[Container].IsOpen, Container);
	return Containers[Container].Capacity;
}

// 0x4a6a40, lines 242-243
bool GetContainerHasParent(int Container)
{
	TASSERT1(242, In(Container,0,OPEN_CONTAINERS-1), Container);
	TASSERT1(243, Containers[Container].IsOpen, Container);
	return Containers[Container].HasParent;
}

// 0x4a6cf0, line 258. The first closed container; 15 when all are open.
int GetFreeContainer()
{
	int i = 0;
	TBEGIN
	for (i = 0; i < OPEN_CONTAINERS; i++) {
		if (!IsContainerOpen(i))
			return i;
	}
	return OPEN_CONTAINERS - 1;
	TEND1(258, i)
}

// 0x4a6eb0, lines 281-283
bool SetOwnOffer(const char *Name)
{
	TASSERT1(281, OwnOffer.Count==0, OwnOffer.Count);
	TASSERT(282, Name!=NULL);
	TASSERT1(283, strlen(Name)<30, strlen(Name));
	strcpy(OwnOffer.Name, Name);
	return true;
}

// 0x4a7230, lines 296-297. False while a counter offer is shown.
bool SetCounterOffer(const char *Name)
{
	if (CounterOffer.Count != 0)
		return false;
	TASSERT(296, Name!=NULL);
	TASSERT1(297, strlen(Name)<30, strlen(Name));
	strcpy(CounterOffer.Name, Name);
	return true;
}

// 0x4a7460, line 307
void AddOwnOfferObject(TObject Object)
{
	TASSERT1(307, OwnOffer.Count<OBJECTS_PER_OFFER, OwnOffer.Count);
	OwnOffer.Objects[OwnOffer.Count] = Object;
	OwnOffer.Count++;
}

// 0x4a75e0, line 317
void AddCounterOfferObject(TObject Object)
{
	TASSERT1(317, CounterOffer.Count<OBJECTS_PER_OFFER, CounterOffer.Count);
	CounterOffer.Objects[CounterOffer.Count] = Object;
	CounterOffer.Count++;
}

// 0x4a7760, line 327
char *GetOwnOfferName()
{
	TASSERT1(327, OwnOffer.Count>0, OwnOffer.Count);
	return OwnOffer.Name;
}

// 0x4a78b0, line 336
char *GetCounterOfferName()
{
	TASSERT1(336, CounterOffer.Count>0, CounterOffer.Count);
	return CounterOffer.Name;
}

// 0x4a7a00, line 361
TObject GetOwnOfferObject(int Position)
{
	TASSERT2(361, In(Position,0,OwnOffer.Count-1), Position, OwnOffer.Count-1);
	return OwnOffer.Objects[Position];
}

// 0x4a7b90, line 370
TObject GetCounterOfferObject(int Position)
{
	TASSERT2(370, In(Position,0,CounterOffer.Count-1), Position, CounterOffer.Count-1);
	return CounterOffer.Objects[Position];
}

// 0x4a7d20
void InitInventory()
{
	ClearBody();
	for (int i = 0; i < OPEN_CONTAINERS; i++)
		Containers[i].IsOpen = false;
	OwnOffer.Count = 0;
	CounterOffer.Count = 0;
}

// 0x4a7d70, line 400
void ResetInventory()
{
	TBEGIN
	InitInventory();
	TEND(400)
}

// Empty; the linker kept one copy of the identical empty functions (0x496160,
// GUI.obj). ExitControl calls it where Inventory.cpp's exit belongs in the reverse
// order of InitControl (name ours, low confidence).
void ExitInventory()
{
}

#if defined(PATELLA_TEST_MODE)
// The test mode's state dump (issue #22; src/testmode/ClientView.h) reads this file's data; it writes
// nothing. Test build only.
void patella::testmode::ViewInventory(TInventoryView *View)
{
	View->Body = Body;
	View->Containers = Containers;
	View->OwnOffer = &OwnOffer;
	View->CounterOffer = &CounterOffer;
}
#endif
