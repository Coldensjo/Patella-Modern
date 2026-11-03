// Help.h: the help bubble of the help mode and the event helps (hints).
// Names are ours except EVENTHELPS, Handle, Number and Name, which are in
// the assertions and catch strings.

#pragma once

#include "common.h"

inline constexpr int EVENTHELPS = 14; // "In(Number,1,EVENTHELPS)", "In(Hint,1,EVENTHELPS)" (MapWindow.cpp)

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
