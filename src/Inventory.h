// Inventory.h: the player's body slots, the open containers and the trade
// offers, as the server last sent them.
//
// Names from the binary (assertions): BODY_POSITIONS, OPEN_CONTAINERS,
// OBJECTS_PER_OFFER, Containers, IsOpen, Count, OwnOffer, CounterOffer, Pos,
// Position, Container, Name, i. The rest are ours.

#pragma once

#include "Objects.h" // TObject
#include "types/FixedString.h"
#include <array>

inline constexpr int BODY_POSITIONS = 10;
inline constexpr int OPEN_CONTAINERS = 16;
inline constexpr int CONTAINER_OBJECTS = 36; // our name
inline constexpr int OBJECTS_PER_OFFER = 100;

// 0x1ec bytes.
struct TContainer {
	bool IsOpen;                                    // +0x00
	TObject Object;                                 // +0x04, the container itself
	TFixedString<30> Name;                          // +0x10
	int Capacity;                                   // +0x30
	bool HasParent;                                 // +0x34, can move up
	int Count;                                      // +0x38
	std::array<TObject, CONTAINER_OBJECTS> Objects; // +0x3c
};

// 0x4d4 bytes. CounterOffer starts 0x4d8 after OwnOffer: nothing refers to the four bytes between them
// (issue #24).
struct TOffer {
	TFixedString<30> Name;                          // +0x00, the trading partner
	int Count;                                      // +0x20
	std::array<TObject, OBJECTS_PER_OFFER> Objects; // +0x24
};
void ClearBody();
void ClearOffers();
int GetOwnOfferCount();
int GetCounterOfferCount();
void SetBodyObject(int Position, TObject Object);
void DeleteBodyObject(int Position);
TObject *GetBodyObject(int Position);
void OpenContainer(int Container, TObject Object, const char *Name, int Capacity, bool HasParent);
void CloseContainer(int Container);
bool IsContainerOpen(int Container);
void InsertContainerObject(int Container, TObject Object);
void AppendContainerObject(int Container, TObject Object);
void ChangeContainerObject(int Container, int Position, TObject Object);
bool DeleteContainerObject(int Container, int Position);
int GetContainerObjectCount(int Container);
TObject GetContainerObject(int Container);
TObject GetContainerContent(int Container, int Position);
char *GetContainerName(int Container);
int GetContainerCapacity(int Container);
bool GetContainerHasParent(int Container);
int GetFreeContainer();
bool SetOwnOffer(const char *Name);
bool SetCounterOffer(const char *Name);
void AddOwnOfferObject(TObject Object);
void AddCounterOfferObject(TObject Object);
char *GetOwnOfferName();
char *GetCounterOfferName();
TObject GetOwnOfferObject(int Position);
TObject GetCounterOfferObject(int Position);
void InitInventory();
void ResetInventory();
void ExitInventory(); // empty, folded into 0x496160
