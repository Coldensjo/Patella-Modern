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
namespace patella {

class TObjectTypes;

namespace testmode {
struct TInventoryView;
} // namespace testmode

// Inventory.cpp's data and functions (Patella-Modern issue #106, M7; notes/decisions.md, "#104: TClient and the
// subsystem objects" and "#106"): the body slots, the open containers and the two offers (0x5ced60-0x5d1643), in the
// original's .bss order, every member 0 at the start; the functions are the members of the same names. TMapState owns
// it (MapState.h), last; it reads TObjectTypes' NoObject. OwnOffer and CounterOffer are public: the golden Errors test
// sets their counts (TestErrorStatics); the others private, as the file-local statics were.
class TInventory {
public:
	explicit TInventory(TObjectTypes &ObjectTypes);
	TInventory(const TInventory &) = delete;
	TInventory &operator=(const TInventory &) = delete;
	TInventory(TInventory &&) = delete;
	TInventory &operator=(TInventory &&) = delete;
	~TInventory() = default;

	void ClearBody();
	void ClearOffers();
	int GetOwnOfferCount() const;
	int GetCounterOfferCount() const;
	void SetBodyObject(int Position, TObject Object);
	void DeleteBodyObject(int Position);
	TObject *GetBodyObject(int Position);
	void OpenContainer(int Container, TObject Object, const char *Name, int Capacity, bool HasParent);
	void CloseContainer(int Container);
	bool IsContainerOpen(int Container) const;
	void InsertContainerObject(int Container, TObject Object);
	void AppendContainerObject(int Container, TObject Object);
	void ChangeContainerObject(int Container, int Position, TObject Object);
	bool DeleteContainerObject(int Container, int Position);
	int GetContainerObjectCount(int Container) const;
	TObject GetContainerObject(int Container) const;
	TObject GetContainerContent(int Container, int Position) const;
	char *GetContainerName(int Container);
	int GetContainerCapacity(int Container) const;
	bool GetContainerHasParent(int Container) const;
	int GetFreeContainer() const;
	bool SetOwnOffer(const char *Name);
	bool SetCounterOffer(const char *Name);
	void AddOwnOfferObject(TObject Object);
	void AddCounterOfferObject(TObject Object);
	char *GetOwnOfferName();
	char *GetCounterOfferName();
	TObject GetOwnOfferObject(int Position) const;
	TObject GetCounterOfferObject(int Position) const;
	void InitInventory();
	void ResetInventory();
	static void ExitInventory(); // empty, folded into 0x496160

	// The test mode's view (ClientView.h's ViewInventory; test build only).
	void View(testmode::TInventoryView *Target) const;

private:
	TObjectTypes &ObjectTypes;

	// Data, in .bss order.
	std::array<TObject, BODY_POSITIONS> Body{};           // 0x5ced60
	std::array<TContainer, OPEN_CONTAINERS> Containers{}; // 0x5cedd8

public:
	TOffer OwnOffer{};     // 0x5d0c98; public: the golden Errors test
	TOffer CounterOffer{}; // 0x5d1170 (4 bytes after OwnOffer's end); public: the golden Errors test

private:
	// The arrays std::arrays (issue #60): their sizes.
	static_assert(sizeof(Body) == BODY_POSITIONS * sizeof(TObject)
			&& sizeof(Containers) == OPEN_CONTAINERS * sizeof(TContainer),
		"Body and Containers");
};

// The transition's access path (issue #106; notes/decisions.md, "#104: TClient and the subsystem objects"): the
// inventory of the one TClient (Client.cpp). It goes when no free function needs it.
namespace bridge {
TInventory &Inventory();
} // namespace bridge

} // namespace patella

// The original's functions (the callers' names, and the golden tests'): each calls TInventory's member of the same
// name on bridge::Inventory().
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
