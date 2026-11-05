// Skins.cpp: rectangles of the GUI graphics (file name inferred).

#pragma once

#include <memory>

// Number of skin entries; the terminator of the table in InitSkins.
inline constexpr int LASTSKIN = 262;

// One rectangle in the GUI picture. width and height are named in GUI.cpp's
// assertions; x and y are ours.
struct TSkin {
	int x;      // +0x0
	int y;      // +0x4
	int width;  // +0x8
	int height; // +0xc
};

extern int NumberOfSkins;
extern std::unique_ptr<TSkin[]> Skins; // issue #58: the table's only owner, a pointer as the original's

void InitSkins();
void ExitSkins();
