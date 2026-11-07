// Help.h: the help bubble of the help mode and the event helps (hints).
// Names are ours except EVENTHELPS, Handle, Number and Name, which are in
// the assertions and catch strings.

#pragma once

#include "types/FixedString.h"
#include <vector>

inline constexpr int EVENTHELPS = 14; // "In(Number,1,EVENTHELPS)", "In(Hint,1,EVENTHELPS)" (MapWindow.cpp)

namespace patella {

// Help.cpp's state (issue #107, M7; notes/decisions.md, "#104: TClient and the subsystem objects" and "#107"): the
// globals 0x5ce520-0x5ced5f of the original are the members, in its .bss order with the same initial values, and the
// file's functions the members of the same names. TGUI owns it (GUI.h).
class THelp {
public:
	THelp() = default;
	THelp(const THelp &) = delete;
	THelp &operator=(const THelp &) = delete;
	THelp(THelp &&) = delete;
	THelp &operator=(THelp &&) = delete;
	~THelp() = default;

	void SetHelpPosition(int x, int y);
	void MoveHelpPosition(int dx, int dy);
	void SetHelpSize(int Width, int Height);
	void SetHelpHandle(int Handle);
	void GetHelpArea(int *x, int *y, int *Width, int *Height) const;
	[[nodiscard]] int GetHelpHandle() const;
	bool GetHelpHandleChanged();
	void InitHelp();
	// Static (issue #107, as TCommunication::ExitCommunication): empty, the golden tests' fake takes its place.
	static void ExitHelp();
	int GetEventHelp();
	int PopEventHelp();
	const char *GetHelpText(int Handle);
	void AddEventHelp(int Number);
	void CheckEventHelpName(const char *Name);
	void CheckEventHelpPosition(int x, int y, int z);
	const char *GetEventHelpTitle(int Number);
	const char *GetEventHelpText(int Number);
	const char *GetCurrentHelpText();

private:
	// Data, in the original's .bss order.
	// 0x5ce520, NumberOfEventHelps: EventHelps.size() (issue #100).
	bool HelpHandleChanged{}; // 0x5ce524
	int HelpHandle{};         // 0x5ce528, what the help bubble explains
	int HelpHeight{};         // 0x5ce52c
	int HelpWidth{};          // 0x5ce530
	int HelpY{};              // 0x5ce534
	int HelpX{};              // 0x5ce538
	// 0x5ce53c (initialiser 0x559e80), the original's vector<int>(0, 9, 10) with NumberOfEventHelps: a queue that
	// appends at its count and pops at 0, read below the count only, so a std::vector (issue #100).
	std::vector<int> EventHelps;
	TFixedString<2048> HelpText{}; // 0x5ce560

	// HelpText a TFixedString (issue #60).
	static_assert(sizeof(HelpText) == 2048, "HelpText");
};

// The transition's access path (issue #107; notes/decisions.md, "#104: TClient and the subsystem objects"): the one
// TClient's THelp (Client.cpp). It goes when no free function needs it.
namespace bridge {
THelp &Help();
} // namespace bridge

} // namespace patella

// Help.cpp's functions (issue #107): each calls THelp's member of the same name on bridge::Help().
void SetHelpPosition(int x, int y);
void MoveHelpPosition(int dx, int dy);
void SetHelpSize(int Width, int Height);
void SetHelpHandle(int Handle);
void GetHelpArea(int *x, int *y, int *Width, int *Height);
int GetHelpHandle();
bool GetHelpHandleChanged();
void InitHelp();
void ExitHelp(); // empty, folded into 0x496160
int GetEventHelp();
int PopEventHelp();
const char *GetHelpText(int Handle);
void AddEventHelp(int Number);
void CheckEventHelpName(const char *Name);
void CheckEventHelpPosition(int x, int y, int z);
const char *GetEventHelpTitle(int Number);
const char *GetEventHelpText(int Number);
const char *GetCurrentHelpText();
