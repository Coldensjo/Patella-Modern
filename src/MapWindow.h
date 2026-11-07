// MapWindow.h: the game window's map view and the frame around it.
//
// TMapWindow (vtable 0x56cf38, 0x92d8 bytes) draws the visible part of the
// map: the levels from the bottom up, field by field, with objects,
// creatures, effects and the light of the light sources, then the creature
// names and health bars, the talkboxes, the status message and the animated
// texts. The 32x32 fields are composed in a cache of 16x12 tiles on surface 4
// (TCacheField) and blitted to surface 2, the 480x352 map surface; the map
// surface is copied (or stretched) to the screen. TMapFrame (vtable
// 0x56d0b8, 0x40 bytes) is the window MainWindow.cpp creates: it holds the
// map view, keeps it at an aspect ratio of 15:11 and draws the border.
//
// Names from the binary: MAP_WIDTH and MAP_HEIGHT ("getWidth()<=MAP_WIDTH*32"),
// the creature fields PKFlag and PARTYFlag, Obj, SrcObj, TradeObject, Hint,
// Brightness, Player, Talkbox, Creatures[i], EffectNumber and the parameter
// names in the catch blocks (Width, Height, x, y, Event, Surface, posx, posy,
// FieldX, FieldY, SpriteNumber). The class names and everything else are
// ours (notes/decisions.md, "MapWindow.cpp").

#pragma once

#include "common.h"
#include "GUI.h"
#include "Map.h" // MAPSIZE_X, MAPSIZE_Y, EFFECTS, GetAmbientColor
#include "Creatures.h"
#include "Objects.h"
#include "graphics.h"
#include <array>

// The map surface in fields ("getWidth()<=MAP_WIDTH*32"): 480x352 pixels.
inline constexpr int MAP_WIDTH = 15;
inline constexpr int MAP_HEIGHT = 11;

// A creature whose name and health bar are drawn above it (12 bytes).
struct TCreatureName {
	uint32 CreatureID; // +0x00
	int x;             // +0x04, centre of the creature on the map surface
	int y;             // +0x08, its top
};

// An animated text to be drawn after the map (0x18 bytes).
struct TTextualEffectPosition {
	int EffectNumber; // +0x00
	int x;            // +0x04, centre on the map surface
	int y;            // +0x08
	int FieldX;       // +0x0c, the field it is on
	int FieldY;       // +0x10
	int FieldZ;       // +0x14
};

// A 32x32 tile of the field cache on surface 4 (0x8c bytes): the sprites that
// make up a field, each with its offset. Sprite 0 is the empty tile of
// picture 7. Sprites counts the sprites of the current frame; while they are
// the same as the first CachedSprites of the tile, nothing is drawn again.
struct TCacheField {
	int x;                       // +0x00, position on surface 4
	int y;                       // +0x04
	int CachedSprites;           // +0x08, sprites in the tile
	std::array<int, 10> Sprite;  // +0x0c
	std::array<int, 10> SpriteX; // +0x34
	std::array<int, 10> SpriteY; // +0x5c
	int Sprites;                 // +0x84
	bool Copied;                 // +0x88, blitted to the map surface in this frame
};

class TMapWindow : public TWindow {
public:
	TMapWindow(); // 0x4bfa00, line 130
	// 0x4bf860 (inline, out-of-line copy before the constructor; deleting
	// destructor 0x4bf9e0). Its two RemoveHotkey calls are the close since
	// issue #108.
	~TMapWindow() override { CheckClosed("TMapWindow"); }
	void SetSize(int Width, int Height) override;          // +0x34, 0x4bfc30, line 152
	int getMinimumWidth() override { return 240; }         // +0x3c, 0x4bf8d0
	int getMinimumHeight() override { return 176; }        // +0x40, 0x4bf8e0
	void Draw(int Surface, int x, int y) override;         // +0x44, 0x4c7580, lines 456-834
	void Arrange() override;                               // +0x48, 0x4bfde0, line 164
	bool LeftButtonDown(int x, int y, int Flags) override; // +0x50, 0x4c24d0, line 196
	bool LeftButtonUp(int x, int y, int Flags) override;   // +0x54, 0x4c2710, lines 219-272
	bool RightButtonUp(int x, int y, int Flags) override;  // +0x5c, 0x4c2e50, lines 335-368
	bool HelpMouseMove(int x, int y) override;             // +0x68, 0x4c3520, lines 417-432
	void HandleEvent(int Event) override;                  // +0x70, 0x4bffa0, lines 382-388

	void DrawField(int posx, int posy, int FieldX, int FieldY, int FieldZ, int Mode); // 0x4c5520, lines 874-1062
	void DrawObject(int posx, int posy, int Level, TObject Obj, int x, int y, int z); // 0x4c4af0, lines 1071-1094
	void DrawCreature(int posx, int posy, int Level, TCreature *Creature);            // 0x4c4f80, lines 1106-1129
	void DrawLightMask(int posx, int posy, int Brightness);                           // 0x4c02c0, lines 1138-1144
	void DrawFieldSprite(int x, int y, int Level, bool Animated, int SpriteNumber, int MaskNumber,
		TRGBColor *Colors);                                                          // 0x4c46a0, line 1180
	void CacheSprite(int FieldX, int FieldY, int SpriteNumber, int dx, int dy);      // 0x4c41e0, line 1231
	void FlushField(int FieldX, int FieldY);                                         // 0x4c3980, line 1256
	TCacheField *GetCacheField(int FieldX, int FieldY);                              // 0x4c0590, lines 1264-1265
	void AddLightSource(int posx, int posy, int z, int Brightness, TRGBColor Color); // 0x4c3c80, lines 1338-1347
	void ArrangeTalkboxes();                                                         // 0x4c0880, lines 1405-1470
	void GetField(int x, int y, int *FieldX, int *FieldY, int *FieldZ);              // 0x4c12e0, lines 1480-1499
	uint32 GetCreatureAt(int x, int y);                                              // 0x4c16b0, line 1545

	// 0x4bf8f0 (inline; inlined once in DrawFieldSprite). A sprite at Level
	// shows on a field unless a ground above Level covers it.
	bool IsFieldVisible(int FieldX, int FieldY, int Level)
	{
		if (!In(FieldX, 0, MAPSIZE_X - 1) || !In(FieldY, 0, MAPSIZE_Y - 1))
			return false;
		return CoverLevel[FieldX][FieldY] <= Level;
	}

	// 0x4bf930 (inline): the ambient colour at a brightness of 0-255.
	void SetLightColor(int FieldX, int FieldY, int Brightness)
	{
		if (Brightness < 0)
			Brightness = 0;
		else if (Brightness > 255)
			Brightness = 255;
		const TRGBColor Color = GetAmbientColor();
		Light[FieldX][FieldY].R = Color.R * Brightness / 255;
		Light[FieldX][FieldY].G = Color.G * Brightness / 255;
		Light[FieldX][FieldY].B = Color.B * Brightness / 255;
	}

	int NumberOfNames;                                             // +0x002c
	std::array<TCreatureName, CREATURES> Names;                    // +0x0030
	int NumberOfTextualEffects;                                    // +0x0738
	std::array<TTextualEffectPosition, EFFECTS> TextualEffects;    // +0x073c
	std::array<std::array<TRGBColor, MAPSIZE_Y>, MAPSIZE_X> Light; // +0x19fc, the light on each field
	int PositionX;                                                 // +0x25cc, GetMapPosition
	int PositionY;                                                 // +0x25d0
	int PositionZ;                                                 // +0x25d4
	int MaxLevel;                                                  // +0x25d8, the highest level drawn
	std::array<std::array<int, MAPSIZE_Y>, MAPSIZE_X> CoverLevel;  // +0x25dc, the highest level with a full ground, 0 none
	int ScrollX;                                                   // +0x29cc, the player's walk offset + 32
	int ScrollY;                                                   // +0x29d0
	std::array<std::array<TCacheField, 12>, 16> Cache;             // +0x29d4
	TComponent *HintButton;                                        // +0x92d4, "Receive hint"

protected:
	// The destructor's work until issue #108 (0x4bf860), then the children.
	void OnClose() override
	{
		try {
			RemoveHotkey(0x44e);
			RemoveHotkey(0x457);
		} catch (...) {
			CloseWhileUnwinding([this] { TWindow::OnClose(); });
			throw;
		}
		TWindow::OnClose();
	}
};

class TMapFrame : public TWindow {
public:
	TMapFrame(); // 0x4c1a40, line 1564
	// The destructor is the implicit one (deleting destructor 0x4adc40 in
	// MainWindow.obj).
	void SetSize(int Width, int Height) override; // +0x34, 0x4c1c40, line 1584
	int getMinimumWidth() override;               // +0x3c, 0x4c1ea0, line 1595
	int getMinimumHeight() override;              // +0x40, 0x4c2040, line 1606
	void Arrange() override;                      // +0x48, 0x4c21e0

	TMapWindow *MapWindow; // +0x2c
	int ViewY;             // +0x30, position of the map view
	int ViewX;             // +0x34
	int ViewWidth;         // +0x38
	int ViewHeight;        // +0x3c
};
