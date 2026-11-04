// graphics.cpp: reconstructed from Tibia.exe 7.72 (0x48b380-0x49547f).
//
// Sprites (Tibia.spr), pictures (Tibia.pic), the fonts cut from pictures, the
// memory and video caches of 32x32 tiles, and the functions that draw
// sprites, objects, outfits, pictures and text through the engine's surface
// (Surface.h). Functions in line order. Those without line evidence stay
// where the binary has them: the three of the DialogWindow/graphics gap
// (0x48b430-0x48b549) first, the Find functions next to the other cache
// functions, GetTextPosition after GetFittingTextLength. The compiler placed
// InitFont and InitFonts (lines 348-403) after ReloadSprites, GetTextWidth
// (1373) before ReloadPictures, DrawObject and DrawOutfit (1100, 1137) at
// the end.
//
// Also in this object: the out-of-line copies of TRGBColor's operator== and
// operator!= (0x48b380, 0x48b3b0; inline members of TRGBColor in
// graphics.h) and TVideoCacheItem's constructor (0x48b3f0, below), which the
// binary emits before the first function.
//
// Names: see graphics.h and notes/decisions.md ("graphics.cpp").

#include "graphics.h"
#include "Objects.h"
#include "Script.h"
#include "Status.h"
#include "Surface.h"
#include "Utils.h"
#include "common.h"
#include "types/FixedString.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

// Assertions whose expression text carries a bug number (as in Map.cpp).
#define TASSERTB1(Line, Expr, V1, Bug) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, "", 0, #Expr " " Bug)

// Data, in .bss order. 0x5cc75c is not used.
std::array<int, HASH_TABLE_SIZE> MemoryHashTable; // 0x5cc430
std::array<int, HASH_TABLE_SIZE> VideoHashTable;  // 0x5cc5c0
int NumberOfFonts;                                // 0x5cc750
int NumberOfPictures;                             // 0x5cc754
uint32 PictureSignature;                          // 0x5cc758
TFixedString<PATH_MAX> PictureFileName;           // 0x5cc760
int NumberOfSprites;                              // 0x5ccb60
uint32 SpriteSignature;                           // 0x5ccb64
TFixedString<PATH_MAX> SpriteFileName;            // 0x5ccb68
int LeastRecentMemoryItem;                        // 0x5ccf68, tail of the memory cache's LRU list
int MostRecentMemoryItem;                         // 0x5ccf6c, head
int MemoryCacheSize;                              // 0x5ccf70
uint8 *MemoryCacheData;                           // 0x5ccf74, MemoryCacheSize tiles of 32*32*3 bytes
int LeastRecentVideoItem;                         // 0x5ccf78
int MostRecentVideoItem;                          // 0x5ccf7c
int VideoCacheSize;                               // 0x5ccf80
TVideoCacheItem *VideoCache;                      // 0x5ccf84
TMemoryCacheItem *MemoryCache;                    // 0x5ccf88
TSprite *Sprites;                                 // 0x5ccf8c
TPicture *Pictures;                               // 0x5ccf90
TFontInfo *Fonts;                                 // 0x5ccf94

// The arrays std::arrays (issue #60): the original's layouts (no pointers: the same on x64).
static_assert(sizeof(TFontInfo) == 0x71c && offsetof(TFontInfo, Widths) == 0x010 && offsetof(TFontInfo, Heights) == 0x390
		&& sizeof(TFontInfo::Widths) == 0x380,
	"TFontInfo's layout");
static_assert(sizeof(TVideoCacheItem) == 0x5c && offsetof(TVideoCacheItem, Colors) == 0x18 && sizeof(TVideoCacheItem::Colors) == 0x30,
	"TVideoCacheItem's layout");
static_assert(sizeof(MemoryHashTable) == HASH_TABLE_SIZE * sizeof(int) && sizeof(VideoHashTable) == HASH_TABLE_SIZE * sizeof(int),
	"the hash tables");

// 0x48b3f0, passed to the eh vector constructor iterator by InitVideoCache.
// It clears the four colours, then the font colour, which overlaps them
// (the binary stores both).
TVideoCacheItem::TVideoCacheItem()
{
	for (int i = 0; i < 4; i++)
		Colors[i] = TRGBColor(0, 0, 0);
	Color = TRGBColor(0, 0, 0);
}

// --- The DialogWindow/graphics gap -------------------------------------------

// 0x48b430. The line distance of a font. Numbers below 0 or above
// NumberOfFonts give font 3's; 0 is not caught and reads before the table
// (sic).
int GetFontLineHeight(int FontNumber)
{
	if (FontNumber < 0 || FontNumber > NumberOfFonts)
		FontNumber = 3;
	return Fonts[FontNumber - 1].LineHeight;
}

// 0x48b460. Empties every memory cache item without touching the LRU list.
void ClearMemoryCache()
{
	for (int i = 0; i < MemoryCacheSize; i++)
		MemoryCache[i].Type = EMPTY_ITEM;
	for (int i = 0; i < HASH_TABLE_SIZE; i++)
		MemoryHashTable[i] = -1;
}

// 0x48b4a0. Called by ExitControl.
void ExitGraphics()
{
	delete[] VideoCache;
	VideoCache = nullptr;
	delete[] MemoryCacheData;
	MemoryCacheData = nullptr;
	delete[] MemoryCache;
	MemoryCache = nullptr;
	delete[] Fonts;
	Fonts = nullptr;
	for (int i = 0; i < NumberOfPictures; i++)
		delete[] Pictures[i].Offsets;
	delete[] Pictures;
	Pictures = nullptr;
	delete[] Sprites;
	Sprites = nullptr;
}

// --- Files ---------------------------------------------------------------------

// 0x48b550, lines 130-148. Tibia.spr: signature, number of sprites, one
// offset per sprite. A file error becomes PROGRAM_ERROR 2. The file is
// closed twice: by close() and by the destructor.
void LoadSprites(const char *FileName)
{
	TASSERT(130, FileName!=NULL);
	TASSERT(131, strlen(FileName)<PATH_MAX);
	TBEGIN
	SpriteFileName.Assign(FileName);
	try {
		TReadBinaryFile File;
		File.open(FileName);
		SpriteSignature = File.readQuad();
		NumberOfSprites = File.readWord();
		Sprites = new TSprite[NumberOfSprites];
		for (int i = 0; i < NumberOfSprites; i++)
			Sprites[i].Offset = File.readQuad();
		File.close();
	} catch (TMyException &) {
		throw (PROGRAM_ERROR)2;
	}
	TEND(148)
}

// 0x48b9a0, lines 156-212. Decodes a sprite into 32*32 RGB triples: the
// colour key, then runs of transparent and coloured pixels. Opens Tibia.spr
// for every sprite. An empty sprite (offset 0) gets the colour key 0,0,0. A
// file error becomes PROGRAM_ERROR 4. Malformed runs write past the buffer.
void LoadSprite(int SpriteNumber, uint8 *Buffer)
{
	TASSERT2(156, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	TASSERT(157, Buffer!=NULL);
	TBEGIN
	TSprite &Sprite = Sprites[SpriteNumber - 1];
	if (Sprite.Offset == 0) {
		Sprite.ColorKey.R = 0;
		Sprite.ColorKey.G = 0;
		Sprite.ColorKey.B = 0;
		for (int i = 0; i < 32 * 32; i++) {
			Buffer[i * 3] = 0;
			Buffer[i * 3 + 1] = 0;
			Buffer[i * 3 + 2] = 0;
		}
	} else {
		try {
			TReadBinaryFile File;
			File.open(SpriteFileName);
			File.seek(Sprite.Offset);
			Sprite.ColorKey.R = File.readByte();
			Sprite.ColorKey.G = File.readByte();
			Sprite.ColorKey.B = File.readByte();
			for (int i = 0; i < 32 * 32; i++) {
				Buffer[i * 3] = (uint8)Sprite.ColorKey.R;
				Buffer[i * 3 + 1] = (uint8)Sprite.ColorKey.G;
				Buffer[i * 3 + 2] = (uint8)Sprite.ColorKey.B;
			}
			const int Size = File.readWord();
			int Read = 0;
			int Pixel = 0;
			while (Read < Size) {
				Pixel += File.readWord();
				const int Count = File.readWord();
				Read += 4;
				for (int i = 0; i < Count; i++) {
					Buffer[Pixel * 3] = File.readByte();
					Buffer[Pixel * 3 + 1] = File.readByte();
					Buffer[Pixel * 3 + 2] = File.readByte();
					Read += 3;
					Pixel++;
				}
			}
			File.close();
		} catch (TMyException &) {
			throw (PROGRAM_ERROR)4;
		}
	}
	TEND(212)
}

// 0x48bea0, lines 244-272. Tibia.pic: signature, number of pictures, then
// per picture its size in tiles, its colour key and the tile offsets. A file
// error becomes PROGRAM_ERROR 3.
void LoadPictures(const char *FileName)
{
	TASSERT(244, FileName!=NULL);
	TASSERT(245, strlen(FileName)<PATH_MAX);
	TBEGIN
	PictureFileName.Assign(FileName);
	try {
		TReadBinaryFile File;
		File.open(FileName);
		PictureSignature = File.readQuad();
		NumberOfPictures = File.readWord();
		Pictures = new TPicture[NumberOfPictures];
		for (int i = 0; i < NumberOfPictures; i++) {
			Pictures[i].TilesX = File.readByte();
			Pictures[i].TilesY = File.readByte();
			Pictures[i].ColorKey.R = File.readByte();
			Pictures[i].ColorKey.G = File.readByte();
			Pictures[i].ColorKey.B = File.readByte();
			const int Tiles = Pictures[i].TilesY * Pictures[i].TilesX;
			Pictures[i].Offsets = new int[Tiles];
			for (int j = 0; j < Tiles; j++)
				Pictures[i].Offsets[j] = File.readQuad();
		}
		File.close();
	} catch (TMyException &) {
		throw (PROGRAM_ERROR)3;
	}
	TEND(272)
}

// 0x48c3b0, lines 280-322. Decodes one tile of a picture like a sprite. The
// tile assertions are inside the inner try block, so a bad tile also becomes
// PROGRAM_ERROR 5, like a file error.
void LoadPictureTile(int PictureNumber, int TileX, int TileY, uint8 *Buffer)
{
	TASSERT2(280, In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	TASSERT(281, Buffer!=NULL);
	TBEGIN
	const TPicture &Picture = Pictures[PictureNumber - 1];
	try {
		TReadBinaryFile File;
		File.open(PictureFileName);
		TASSERT2(289, In(TileX,0,Picture.TilesX-1), TileX, Picture.TilesX-1);
		TASSERT2(290, In(TileY,0,Picture.TilesY-1), TileY, Picture.TilesY-1);
		File.seek(Picture.Offsets[Picture.TilesX * TileY + TileX]);
		for (int i = 0; i < 32 * 32; i++) {
			Buffer[i * 3] = (uint8)Picture.ColorKey.R;
			Buffer[i * 3 + 1] = (uint8)Picture.ColorKey.G;
			Buffer[i * 3 + 2] = (uint8)Picture.ColorKey.B;
		}
		const int Size = File.readWord();
		int Read = 0;
		int Pixel = 0;
		while (Read < Size) {
			Pixel += File.readWord();
			const int Count = File.readWord();
			Read += 4;
			for (int i = 0; i < Count; i++) {
				Buffer[Pixel * 3] = File.readByte();
				Buffer[Pixel * 3 + 1] = File.readByte();
				Buffer[Pixel * 3 + 2] = File.readByte();
				Read += 3;
				Pixel++;
			}
		}
		File.close();
	} catch (TMyException &) {
		throw (PROGRAM_ERROR)5;
	}
	TEND1(322, PictureNumber)
}

// --- Fonts ---------------------------------------------------------------------

// 0x492640, lines 348-387. The
// picture holds 224 characters, CharsPerLine per row. The visible width of a
// character is one more than its rightmost column with a pixel that is not
// the colour key, searched from column CharWidth-1 down to 1; without one it
// is CharWidth. The visible height likewise from the rows CharHeight-1 down
// to 1, else 1. Column 0 and row 0 are never looked at. Height becomes the
// highest row found (not the stored height: a blank character counts -1).
void InitFont(int FontName, int PictureNumber, int CharsPerLine, int CharWidth, int CharHeight,
	int Spacing, int LineHeight)
{
	TASSERT1(348, CharsPerLine>0, CharsPerLine);
	TASSERT1(349, CharWidth>0, CharWidth);
	TASSERT1(350, CharHeight>0, CharHeight);
	TBEGIN
	TFontInfo &Font = Fonts[FontName - 1];
	Font.CharHeight = CharHeight;
	Font.Spacing = Spacing;
	Font.Height = 0;
	Font.PictureNumber = PictureNumber;
	Font.CharsPerLine = CharsPerLine;
	Font.CharWidth = CharWidth;
	Font.LineHeight = LineHeight;
	const TRGBColor &ColorKey = Pictures[PictureNumber - 1].ColorKey;
	for (int c = 0; c < 224; c++) {
		const int y = c / CharsPerLine * CharHeight;
		const int x = c % CharsPerLine * CharWidth;
		const TMemoryCacheItem *Item = GetMemoryPicture(PictureNumber, x / 32, y / 32);
		int Right = -1;
		for (int i = CharWidth - 1; i > 0 && Right == -1; i--) {
			for (int j = 0; j < CharHeight && Right == -1; j++) {
				const uint8 *Pixel = &Item->Data[((x % 32) + ((y % 32) + j) * 32 + i) * 3];
				if (Pixel[0] != ColorKey.R || Pixel[1] != ColorKey.G || Pixel[2] != ColorKey.B)
					Right = i + 1;
			}
		}
		int Bottom = -1;
		for (int j = CharHeight - 1; j > 0 && Bottom == -1; j--) {
			for (int i = 0; i < CharWidth && Bottom == -1; i++) {
				const uint8 *Pixel = &Item->Data[((x % 32) + ((y % 32) + j) * 32 + i) * 3];
				if (Pixel[0] != ColorKey.R || Pixel[1] != ColorKey.G || Pixel[2] != ColorKey.B)
					Bottom = j + 1;
			}
		}
		Font.Widths[c] = (Right != -1 ? Right : CharWidth);
		Font.Heights[c] = (Bottom != -1 ? Bottom : 1);
		if (Bottom < Font.Height)
			Bottom = Font.Height;
		Font.Height = Bottom;
	}
	TEND2(387, FontName, PictureNumber)
}

// 0x492c30, line 403. The four fonts and their pictures.
void InitFonts()
{
	TBEGIN
	NumberOfFonts = 4;
	Fonts = new TFontInfo[4];
	InitFont(1, 3, 32, 8, 16, 1, 12);
	InitFont(2, 5, 32, 16, 16, -1, 15);
	InitFont(3, 6, 32, 8, 8, 0, 10);
	InitFont(4, 8, 32, 8, 16, 1, 14);
	TEND(403)
}

// --- The memory cache ------------------------------------------------------------

// 0x48c9c0, line 458. Items in one LRU list, item 0 the most recent.
void InitMemoryCache(int MemoryCacheBytes)
{
	TBEGIN
	MemoryCacheSize = MemoryCacheBytes / (32 * 32 * 3);
	MemoryCacheData = new uint8[MemoryCacheSize * 32 * 32 * 3];
	MemoryCache = new TMemoryCacheItem[MemoryCacheSize];
	for (int i = 0; i < MemoryCacheSize; i++) {
		TMemoryCacheItem &Item = MemoryCache[i];
		Item.Data = &MemoryCacheData[i * 32 * 32 * 3];
		Item.Type = EMPTY_ITEM;
		Item.Next = i + 1;
		Item.Previous = i - 1;
		Item.HashNext = -1;
		Item.HashPrevious = -1;
		Item.HashValue = -1;
	}
	MemoryCache[0].Previous = -1;
	MemoryCache[MemoryCacheSize - 1].Next = -1;
	LeastRecentMemoryItem = MemoryCacheSize - 1;
	MostRecentMemoryItem = 0;
	for (int i = 0; i < HASH_TABLE_SIZE; i++)
		MemoryHashTable[i] = -1;
	TEND(458)
}

// 0x48cc30, line 466. Makes an item the most recent one.
void TouchMemoryItem(int Position)
{
	TASSERT1(466, In(Position,0,MemoryCacheSize-1), Position);
	if (Position == MostRecentMemoryItem)
		return;
	TMemoryCacheItem &Item = MemoryCache[Position];
	if (Item.Next != -1)
		MemoryCache[Item.Next].Previous = Item.Previous;
	if (Item.Previous != -1)
		MemoryCache[Item.Previous].Next = Item.Next;
	if (Position == LeastRecentMemoryItem)
		LeastRecentMemoryItem = Item.Previous;
	Item.Previous = -1;
	Item.Next = MostRecentMemoryItem;
	MemoryCache[MostRecentMemoryItem].Previous = Position;
	MostRecentMemoryItem = Position;
}

// 0x48ce00. The handlers of the TBEGIN/TEND here and in the other Find
// functions were removed by the compiler (nothing in them can throw); the
// exception states remain.
int FindMemorySprite(int SpriteNumber)
{
	TBEGIN
	int Position = MemoryHashTable[SpriteNumber % HASH_TABLE_SIZE];
	while (Position != -1) {
		const TMemoryCacheItem &Item = MemoryCache[Position];
		if (Item.Type == SPRITE_ITEM && Item.Number == SpriteNumber)
			return Position;
		Position = Item.HashNext;
	}
	return -1;
	TEND(498)
}

// 0x48ce70
int FindMemoryPicture(int PictureNumber, int TileX, int TileY)
{
	TBEGIN
	int Position = MemoryHashTable[(PictureNumber + TileX + TileY) % HASH_TABLE_SIZE];
	while (Position != -1) {
		const TMemoryCacheItem &Item = MemoryCache[Position];
		if (Item.Type == PICTURE_ITEM && Item.Number == PictureNumber && Item.TileX == TileX
			&& Item.TileY == TileY)
			return Position;
		Position = Item.HashNext;
	}
	return -1;
	TEND(516)
}

// 0x48cef0, line 522. Empties an item and takes it out of its hash chain.
void RemoveMemoryItem(int Position)
{
	TASSERT1(522, In(Position,0,MemoryCacheSize-1), Position);
	TMemoryCacheItem &Item = MemoryCache[Position];
	if (Item.Type == EMPTY_ITEM)
		return;
	Item.Type = EMPTY_ITEM;
	if (Item.HashNext != -1)
		MemoryCache[Item.HashNext].HashPrevious = Item.HashPrevious;
	if (Item.HashPrevious != -1)
		MemoryCache[Item.HashPrevious].HashNext = Item.HashNext;
	if (Item.HashPrevious == -1)
		MemoryHashTable[Item.HashValue] = Item.HashNext;
}

// 0x48d0a0, lines 536-539. Puts a filled item at the head of a hash chain.
void InsertMemoryItem(int Position, int HashValue)
{
	TASSERT1(536, In(Position,0,MemoryCacheSize-1), Position);
	TASSERT2(537, In(HashValue,0,HASH_TABLE_SIZE-1), HashValue, Position);
	TMemoryCacheItem &Item = MemoryCache[Position];
	TASSERT1(539, Item.Type!=EMPTY_ITEM, Position);
	Item.HashValue = HashValue;
	Item.HashPrevious = -1;
	Item.HashNext = MemoryHashTable[HashValue];
	if (MemoryHashTable[HashValue] != -1)
		MemoryCache[MemoryHashTable[HashValue]].HashPrevious = Position;
	MemoryHashTable[HashValue] = Position;
}

// 0x48d440, lines 553-569. A missing sprite replaces the least recent item.
TMemoryCacheItem *GetMemorySprite(int SpriteNumber)
{
	TASSERT2(553, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	int Position = -1;
	TBEGIN
	Position = FindMemorySprite(SpriteNumber);
	if (Position == -1) {
		RemoveMemoryItem(LeastRecentMemoryItem);
		Position = LeastRecentMemoryItem;
		TMemoryCacheItem &Item = MemoryCache[LeastRecentMemoryItem];
		LoadSprite(SpriteNumber, Item.Data);
		Item.Type = SPRITE_ITEM;
		Item.Number = SpriteNumber;
		InsertMemoryItem(Position, SpriteNumber % HASH_TABLE_SIZE);
	}
	TouchMemoryItem(Position);
	return &MemoryCache[Position];
	TEND2(569, SpriteNumber, Position)
}

// 0x48d740, lines 578-595
TMemoryCacheItem *GetMemoryPicture(int PictureNumber, int TileX, int TileY)
{
	TASSERT2(578, In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	TBEGIN
	int Position = FindMemoryPicture(PictureNumber, TileX, TileY);
	if (Position == -1) {
		RemoveMemoryItem(LeastRecentMemoryItem);
		Position = LeastRecentMemoryItem;
		TMemoryCacheItem &Item = MemoryCache[LeastRecentMemoryItem];
		LoadPictureTile(PictureNumber, TileX, TileY, Item.Data);
		Item.TileX = TileX;
		Item.Number = PictureNumber;
		Item.Type = PICTURE_ITEM;
		Item.TileY = TileY;
		InsertMemoryItem(Position, (TileX + PictureNumber + TileY) % HASH_TABLE_SIZE);
	}
	TouchMemoryItem(Position);
	return &MemoryCache[Position];
	TEND1(595, PictureNumber)
}

// --- The video cache -------------------------------------------------------------

// 0x48da50, line 636. Called by Player.cpp once the surfaces exist and by
// ResetVideoCache. The slots fill the cache surfaces (all as large as
// surface 6) row by row. Without cache surfaces the cache is NULL and
// VideoCacheSize keeps its old value.
void InitVideoCache()
{
	TBEGIN
	const int Surfaces = GetSurface()->GetCacheSurfaces();
	if (Surfaces == 0) {
		VideoCache = nullptr;
		return;
	}
	TRect Rect = {0, 0, 0, 0};
	GetSurface()->GetSurfaceRect(6, &Rect);
	const int PerSurface = (Rect.height / 32) * (Rect.width / 32);
	VideoCacheSize = PerSurface * Surfaces;
	VideoCache = new TVideoCacheItem[VideoCacheSize];
	for (int i = 0; i < VideoCacheSize; i++) {
		TVideoCacheItem &Item = VideoCache[i];
		Item.Surface = i / PerSurface;
		Item.y = i % PerSurface / (Rect.width / 32) * 32;
		Item.x = i % PerSurface % (Rect.width / 32) * 32;
		Item.Type = EMPTY_ITEM;
		Item.Next = i + 1;
		Item.Previous = i - 1;
		Item.HashNext = -1;
		Item.HashPrevious = -1;
		Item.HashValue = -1;
	}
	VideoCache[0].Previous = -1;
	VideoCache[VideoCacheSize - 1].Next = -1;
	LeastRecentVideoItem = VideoCacheSize - 1;
	MostRecentVideoItem = 0;
	for (int i = 0; i < HASH_TABLE_SIZE; i++)
		VideoHashTable[i] = -1;
	TEND(636)
}

// 0x48dd70, line 644
void TouchVideoItem(int Position)
{
	TASSERT1(644, In(Position,0,VideoCacheSize-1), Position);
	if (Position == MostRecentVideoItem)
		return;
	TVideoCacheItem &Item = VideoCache[Position];
	if (Item.Next != -1)
		VideoCache[Item.Next].Previous = Item.Previous;
	if (Item.Previous != -1)
		VideoCache[Item.Previous].Next = Item.Next;
	if (Position == LeastRecentVideoItem)
		LeastRecentVideoItem = Item.Previous;
	Item.Previous = -1;
	Item.Next = MostRecentVideoItem;
	VideoCache[MostRecentVideoItem].Previous = Position;
	MostRecentVideoItem = Position;
}

// 0x48df40. The video Find functions give up after VideoCacheSize+2 chain
// steps: the chains can be corrupt, see RemoveVideoItem.
int FindVideoSprite(int SpriteNumber)
{
	TBEGIN
	int Position = VideoHashTable[SpriteNumber % HASH_TABLE_SIZE];
	int Count = 0;
	while (Position != -1) {
		const TVideoCacheItem &Item = VideoCache[Position];
		if (Item.Type == SPRITE_ITEM && Item.Number == SpriteNumber && Item.MaskNumber == 0)
			return Position;
		Position = Item.HashNext;
		if (Count++ > VideoCacheSize)
			return -1;
	}
	return -1;
	TEND(681)
}

// 0x48dfc0. The fourth colour is compared through the out-of-line
// operator==, the others inline.
int FindVideoMaskedSprite(int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	TBEGIN
	int Position = VideoHashTable[(MaskNumber + SpriteNumber) % HASH_TABLE_SIZE];
	int Count = 0;
	while (Position != -1) {
		const TVideoCacheItem &Item = VideoCache[Position];
		if (Item.Type == SPRITE_ITEM && Item.Number == SpriteNumber && Item.MaskNumber == MaskNumber
			&& Item.Colors[0] == Colors[0] && Item.Colors[1] == Colors[1]
			&& Item.Colors[2] == Colors[2] && Item.Colors[3] == Colors[3])
			return Position;
		Position = Item.HashNext;
		if (Count++ > VideoCacheSize)
			return -1;
	}
	return -1;
	TEND(706)
}

// 0x48e0f0. Matches font characters of the picture too (sic).
int FindVideoPicture(int PictureNumber, int TileX, int TileY)
{
	TBEGIN
	int Position = VideoHashTable[(TileX + PictureNumber + TileY) % HASH_TABLE_SIZE];
	int Count = 0;
	while (Position != -1) {
		const TVideoCacheItem &Item = VideoCache[Position];
		if (Item.Type == PICTURE_ITEM && Item.Number == PictureNumber && Item.TileX == TileX
			&& Item.TileY == TileY)
			return Position;
		Position = Item.HashNext;
		if (Count++ > VideoCacheSize)
			return -1;
	}
	return -1;
	TEND(728)
}

// 0x48e190. A plain picture tile matches colour 0,0,0 (sic).
int FindVideoFontChar(int PictureNumber, int TileX, int TileY, TRGBColor Color)
{
	TBEGIN
	int Position = VideoHashTable[(TileX + PictureNumber + TileY) % HASH_TABLE_SIZE];
	int Count = 0;
	while (Position != -1) {
		const TVideoCacheItem &Item = VideoCache[Position];
		if (Item.Type == PICTURE_ITEM && Item.Number == PictureNumber && Item.TileX == TileX
			&& Item.TileY == TileY && Item.Color == Color)
			return Position;
		Position = Item.HashNext;
		if (Count++ > VideoCacheSize)
			return -1;
	}
	return -1;
	TEND(750)
}

// 0x48e250, line 762. Unlike RemoveMemoryItem this ignores position 0 (sic):
// when slot 0 is reused it stays in its old hash chain, and InsertVideoItem
// links it into a second one.
void RemoveVideoItem(int Position)
{
	if (Position <= 0)
		return;
	TASSERT1(762, In(Position,0,VideoCacheSize-1), Position);
	TVideoCacheItem &Item = VideoCache[Position];
	if (Item.Type == EMPTY_ITEM)
		return;
	Item.Type = EMPTY_ITEM;
	if (Item.HashNext != -1)
		VideoCache[Item.HashNext].HashPrevious = Item.HashPrevious;
	if (Item.HashPrevious != -1)
		VideoCache[Item.HashPrevious].HashNext = Item.HashNext;
	if (Item.HashPrevious == -1)
		VideoHashTable[Item.HashValue] = Item.HashNext;
}

// 0x48e410, lines 776-779
void InsertVideoItem(int Position, int HashValue)
{
	TASSERT1(776, In(Position,0,VideoCacheSize-1), Position);
	TASSERT2(777, In(HashValue,0,HASH_TABLE_SIZE-1), HashValue, Position);
	TVideoCacheItem &Item = VideoCache[Position];
	TASSERT2(779, Item.Type!=EMPTY_ITEM, Position, HashValue);
	Item.HashValue = HashValue;
	Item.HashPrevious = -1;
	Item.HashNext = VideoHashTable[HashValue];
	if (VideoHashTable[HashValue] != -1)
		VideoCache[VideoHashTable[HashValue]].HashPrevious = Position;
	VideoHashTable[HashValue] = Position;
}

// 0x48e7b0, lines 793-814. OpenGL and DirectX 9 (engines 1 and 2) get the
// colour key of the slot's surface first.
TVideoCacheItem *GetVideoSprite(int SpriteNumber)
{
	TASSERT2(793, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	int Position = -1;
	TBEGIN
	Position = FindVideoSprite(SpriteNumber);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemorySprite(SpriteNumber);
		if (curEngine == 1 || curEngine == 2)
			GetSurface()->SetColorKey(Item.Surface + 6, Sprites[SpriteNumber - 1].ColorKey);
		GetSurface()->WriteImage(Item.Surface + 6, Item.x, Item.y, 32, 32, Tile->Data);
		Item.Type = SPRITE_ITEM;
		Item.Number = SpriteNumber;
		Item.MaskNumber = 0;
		InsertVideoItem(Position, SpriteNumber % HASH_TABLE_SIZE);
	}
	TouchVideoItem(Position);
	return &VideoCache[Position];
	TEND2(814, SpriteNumber, Position)
}

// 0x48eb30, lines 824-850. A sprite coloured through a mask (outfits).
TVideoCacheItem *GetVideoMaskedSprite(int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	TASSERT2(824, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	TASSERT2(825, In(MaskNumber,1,NumberOfSprites), MaskNumber, NumberOfSprites);
	TBEGIN
	int Position = FindVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemorySprite(SpriteNumber);
		const TMemoryCacheItem *Mask = GetMemorySprite(MaskNumber);
		if (curEngine == 1 || curEngine == 2)
			GetSurface()->SetColorKey(Item.Surface + 6, Sprites[SpriteNumber - 1].ColorKey);
		GetSurface()->WriteMaskedImage(Item.Surface + 6, Item.x, Item.y, 32, 32, Tile->Data, Mask->Data,
			Colors);
		Item.MaskNumber = MaskNumber;
		Item.Type = SPRITE_ITEM;
		Item.Number = SpriteNumber;
		for (int i = 0; i < 4; i++)
			Item.Colors[i] = Colors[i];
		InsertVideoItem(Position, (SpriteNumber + MaskNumber) % HASH_TABLE_SIZE);
	}
	TouchVideoItem(Position);
	return &VideoCache[Position];
	TEND2(850, SpriteNumber, MaskNumber)
}

// 0x48f020, lines 859-885. The colour key test reads the next picture's key
// (Pictures[PictureNumber], past the end for the last picture), and both of
// its branches set the same key (sic).
TVideoCacheItem *GetVideoPicture(int PictureNumber, int TileX, int TileY)
{
	TASSERT2(859, In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	int Position = -1;
	TBEGIN
	Position = FindVideoPicture(PictureNumber, TileX, TileY);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemoryPicture(PictureNumber, TileX, TileY);
		if (curEngine == 1 || curEngine == 2) {
			if (Pictures[PictureNumber].ColorKey != TRGBColor(1, 1, 1))
				GetSurface()->SetColorKey(Item.Surface + 6, Pictures[PictureNumber - 1].ColorKey);
			else
				GetSurface()->SetColorKey(Item.Surface + 6, Pictures[PictureNumber - 1].ColorKey);
		}
		GetSurface()->WriteImage(Item.Surface + 6, Item.x, Item.y, 32, 32, Tile->Data);
		Item.TileY = TileY;
		Item.TileX = TileX;
		Item.Number = PictureNumber;
		Item.Type = PICTURE_ITEM;
		Item.Color = TRGBColor(0, 0, 0);
		InsertVideoItem(Position, (PictureNumber + TileX + TileY) % HASH_TABLE_SIZE);
	}
	TouchVideoItem(Position);
	return &VideoCache[Position];
	TEND2(885, PictureNumber, Position)
}

// 0x48f430, lines 894-919. A tile of a font's picture in a text colour: the
// surface paints the pixels that are not the colour key in Color.
TVideoCacheItem *GetVideoFontChar(int FontNumber, int TileX, int TileY, TRGBColor Color)
{
	TASSERT2(894, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	int Position = -1;
	TBEGIN
	const int PictureNumber = Fonts[FontNumber - 1].PictureNumber;
	Position = FindVideoFontChar(PictureNumber, TileX, TileY, Color);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemoryPicture(PictureNumber, TileX, TileY);
		TRGBColor Colors[4] = {Color, Pictures[PictureNumber - 1].ColorKey, TRGBColor(0, 0, 0),
			TRGBColor(0, 0, 0)};
		if (curEngine == 1 || curEngine == 2)
			GetSurface()->SetColorKey(Item.Surface + 6, Pictures[PictureNumber - 1].ColorKey);
		GetSurface()->WriteMaskedImage(Item.Surface + 6, Item.x, Item.y, 32, 32, Tile->Data, nullptr,
			Colors);
		Item.TileX = TileX;
		Item.TileY = TileY;
		Item.Type = PICTURE_ITEM;
		Item.Number = PictureNumber;
		Item.Color = Color;
		InsertVideoItem(Position, (TileX + PictureNumber + TileY) % HASH_TABLE_SIZE);
	}
	TouchVideoItem(Position);
	return &VideoCache[Position];
	TEND2(919, FontNumber, Position)
}

// --- Sprites ---------------------------------------------------------------------

// 0x48f870, line 933. No try block.
void DrawSprite(int Surface, int x, int y, int SpriteNumber)
{
	if (SpriteNumber == 0)
		return;
	TASSERT2(933, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoSprite(SpriteNumber);
	GetSurface()->Blit(Surface, x, y, Item->Surface + 6, TRect(Item->x, Item->y, 32, 32),
		Sprites[SpriteNumber - 1].ColorKey);
}

// 0x48fa60, lines 948-953. Unlike DrawSprite this skips negative numbers too.
void DrawSpriteClipped(int Surface, int x, int y, int SpriteNumber, TRect Clip)
{
	if (SpriteNumber <= 0)
		return;
	TASSERT2(948, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	TBEGIN
	const TVideoCacheItem *Item = GetVideoSprite(SpriteNumber);
	GetSurface()->BlitClipped(Surface, x, y, Item->Surface + 6, TRect(Item->x, Item->y, 32, 32),
		Sprites[SpriteNumber - 1].ColorKey, Clip);
	TEND2(953, Surface, SpriteNumber)
}

// 0x48fd80, lines 979-984
void DrawSpriteScaled(int Surface, int x, int y, int Width, int Height, int SpriteNumber,
	TRGBColor Highlight, TRect Clip)
{
	if (SpriteNumber == 0)
		return;
	TASSERT2(979, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	TBEGIN
	const TVideoCacheItem *Item = GetVideoSprite(SpriteNumber);
	GetSurface()->StretchBlit(Surface, TRect(x, y, Width, Height), Item->Surface + 6,
		TRect(Item->x, Item->y, 32, 32), Sprites[SpriteNumber - 1].ColorKey, Highlight, Clip);
	TEND2(984, Surface, SpriteNumber)
}

// 0x4900d0, lines 998-1003. The assertions are inside the try block here.
void DrawMaskedSprite(int Surface, int x, int y, int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	TBEGIN
	if (MaskNumber == 0) {
		DrawSprite(Surface, x, y, SpriteNumber);
		return;
	}
	TASSERT2(998, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	TASSERT2(999, In(MaskNumber,1,NumberOfSprites), MaskNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
	GetSurface()->Blit(Surface, x, y, Item->Surface + 6, TRect(Item->x, Item->y, 32, 32),
		Sprites[SpriteNumber - 1].ColorKey);
	TEND2(1003, Surface, SpriteNumber)
}

// 0x4904b0, lines 1059-1064
void DrawMaskedSpriteScaled(int Surface, int x, int y, int Width, int Height, int SpriteNumber,
	int MaskNumber, TRGBColor *Colors, TRGBColor Highlight, TRect Clip)
{
	TBEGIN
	if (MaskNumber == 0) {
		DrawSpriteScaled(Surface, x, y, Width, Height, SpriteNumber, Highlight, Clip);
		return;
	}
	TASSERT2(1059, In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	TASSERT2(1060, In(MaskNumber,1,NumberOfSprites), MaskNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
	GetSurface()->StretchBlit(Surface, TRect(x, y, Width, Height), Item->Surface + 6,
		TRect(Item->x, Item->y, 32, 32), Sprites[SpriteNumber - 1].ColorKey, Highlight, Clip);
	TEND2(1064, Surface, SpriteNumber)
}

// 0x494ca0, line 1100. An
// object in a Size x Size box whose bottom right corner is x+Size, y+Size,
// in the animation phase of the action counter; the count of a cumulative
// object above 1 (or any count with ShowCount) is written in its bottom
// right corner. Count holds three digits; more overflow it (sic).
void DrawObject(int Surface, int x, int y, int Size, TObject Obj, TRGBColor Highlight, TRect Clip,
	TFont Font, bool ShowCount)
{
	TBEGIN
	if (Obj.Type == 0)
		return;
	int PatternX = 1;
	int PatternY = 1;
	if (GetObjectFlag(Obj.Type, CUMULATIVE))
		GetCumulativePattern(Obj, &PatternX, &PatternY);
	if (GetObjectFlag(Obj.Type, LIQUIDPOOL) || GetObjectFlag(Obj.Type, LIQUIDCONTAINER))
		GetLiquidPattern(Obj, &PatternX, &PatternY);
	const int Action = GetActionNumber();
	const int Phase = Action / 5 % GetObjectPhases(Obj.Type) + 1;
	const int Scale = Size * 32 / GetObjectExactSize(Obj.Type);
	for (int Layer = 1; Layer <= GetObjectLayers(Obj.Type); Layer++) {
		for (int px = 1; px <= GetObjectWidth(Obj.Type); px++) {
			for (int py = 1; py <= GetObjectHeight(Obj.Type); py++) {
				const int SpriteNumber = GetObjectSpriteNumber(Obj.Type, px, py, Layer, PatternX, PatternY, 1,
					Phase);
				DrawSpriteScaled(Surface, x - px * Scale + Size, y - py * Scale + Size, Scale, Scale,
					SpriteNumber, Highlight, Clip);
			}
		}
	}
	if (GetObjectFlag(Obj.Type, CUMULATIVE) && (Obj.Data > 1 || ShowCount)) {
		TFixedString<4> Count; // the count, a byte of the server (three digits)
		Count.Format("{}", Obj.Data);
		const int Height = GetTextHeight(Font.FontNumber, Count);
		PrintFormattedTextClipped(Surface, x - 1 + Size, y - Height - 1 + Size, Font, Count, Clip);
	}
	TEND1(1100, Surface)
}

// 0x495080, line 1137. A creature's outfit facing south (direction 2) in
// its four colours (indices of the outfit palette), with the colour mask on
// layer 2. Outfit 0 shows the object Colors[0] instead (none if 0).
void DrawOutfit(int Surface, int x, int y, int Size, int OutfitNumber, int *Colors,
	TRGBColor Highlight, TRect Clip, TFont Font)
{
	TBEGIN
	if (OutfitNumber == 0) {
		if (Colors[0] != 0) {
			const TObject Obj = {Colors[0], 0, 0};
			DrawObject(Surface, x, y, Size, Obj, Highlight, Clip, Font, false);
		}
		return;
	}
	TRGBColor OutfitColors[4]; // zeroed by the constructor (the binary's array loop)
	int R, G, B;
	GetOutfitColor(Colors[0], &R, &G, &B);
	OutfitColors[0] = TRGBColor(R, G, B);
	GetOutfitColor(Colors[1], &R, &G, &B);
	OutfitColors[1] = TRGBColor(R, G, B);
	GetOutfitColor(Colors[2], &R, &G, &B);
	OutfitColors[2] = TRGBColor(R, G, B);
	GetOutfitColor(Colors[3], &R, &G, &B);
	OutfitColors[3] = TRGBColor(R, G, B);
	const int Scale = Size * 32 / GetOutfitExactSize(OutfitNumber);
	for (int px = 1; px <= GetOutfitWidth(OutfitNumber); px++) {
		for (int py = 1; py <= GetOutfitHeight(OutfitNumber); py++) {
			const int SpriteNumber = GetOutfitSpriteNumber(OutfitNumber, px, py, 1, 2, 1);
			int MaskNumber;
			if (GetOutfitLayers(OutfitNumber) == 2)
				MaskNumber = GetOutfitSpriteNumber(OutfitNumber, px, py, 2, 2, 1);
			else
				MaskNumber = 0;
			DrawMaskedSpriteScaled(Surface, x - px * Scale + Size, y - py * Scale + Size, Scale, Scale,
				SpriteNumber, MaskNumber, OutfitColors, Highlight, Clip);
		}
	}
	TEND1(1137, Surface)
}

// --- Pictures --------------------------------------------------------------------

// 0x490920, lines 1145-1155. The binary interleaves the loops with the
// catch blocks (Ghidra splits off 0x490a58).
void DrawPicture(int Surface, int x, int y, int PictureNumber)
{
	TASSERT2(1145, In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	TBEGIN
	const TPicture &Picture = Pictures[PictureNumber - 1];
	for (int TileY = 0; TileY < Picture.TilesY; TileY++) {
		for (int TileX = 0; TileX < Picture.TilesX; TileX++) {
			const TVideoCacheItem *Item = GetVideoPicture(PictureNumber, TileX, TileY);
			GetSurface()->Blit(Surface, TileX * 32 + x, TileY * 32 + y, Item->Surface + 6,
				TRect(Item->x, Item->y, 32, 32), Picture.ColorKey);
		}
	}
	TEND2(1155, Surface, PictureNumber)
}

// 0x490c60, lines 1164-1182. The part SourceX, SourceY, Width, Height of a
// picture, tile by tile. The exception tables have the states of two more
// assertions between the first one and the try block; the empty part returns
// before them, so the compiler dropped them (their line numbers are
// unknown).
void DrawPicturePart(int Surface, int x, int y, int PictureNumber, int SourceX, int SourceY,
	int Width, int Height)
{
	TASSERT2(1164, In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	if (Width <= 0 || Height <= 0)
		return;
	TASSERT1(1166, Width>0, Width);
	TASSERT1(1167, Height>0, Height);
	TBEGIN
	const TPicture &Picture = Pictures[PictureNumber - 1];
	for (int TileY = (SourceY / 32 < 0 ? 0 : SourceY / 32);
		TileY < ((SourceY + Height + 31) / 32 > Picture.TilesY ? Picture.TilesY : (SourceY + Height + 31) / 32);
		TileY++) {
		for (int TileX = (SourceX / 32 < 0 ? 0 : SourceX / 32);
			TileX < ((SourceX + Width + 31) / 32 > Picture.TilesX ? Picture.TilesX : (SourceX + Width + 31) / 32);
			TileX++) {
			const TVideoCacheItem *Item = GetVideoPicture(PictureNumber, TileX, TileY);
			const int OffsetY = SourceY - TileY * 32;
			const int PartHeight = (Height + OffsetY > 32 ? 32 : Height + OffsetY);
			const int OffsetX = SourceX - TileX * 32;
			const int PartWidth = (Width + OffsetX > 32 ? 32 : Width + OffsetX);
			const int LeftX = (OffsetX < 0 ? 0 : OffsetX);
			const int TopY = (OffsetY < 0 ? 0 : OffsetY);
			const int DestX = (TileX * 32 - SourceX < 0 ? 0 : TileX * 32 - SourceX);
			const int DestY = (TileY * 32 - SourceY < 0 ? 0 : TileY * 32 - SourceY);
			GetSurface()->Blit(Surface, DestX + x, DestY + y, Item->Surface + 6,
				TRect(Item->x + LeftX, Item->y + TopY, PartWidth - LeftX, PartHeight - TopY), Picture.ColorKey);
		}
	}
	TEND2(1182, Surface, PictureNumber)
}

// 0x4910e0, lines 1191-1206. As DrawPicturePart, with a clipping rectangle;
// an empty part fails the assertions.
void DrawPicturePartClipped(int Surface, int x, int y, int PictureNumber, int SourceX, int SourceY,
	int Width, int Height, TRect Clip)
{
	TASSERT2(1191, In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	TASSERT1(1192, Width>0, Width);
	TASSERT1(1193, Height>0, Height);
	TBEGIN
	const TPicture &Picture = Pictures[PictureNumber - 1];
	for (int TileY = (SourceY / 32 < 0 ? 0 : SourceY / 32);
		TileY < ((SourceY + Height + 31) / 32 > Picture.TilesY ? Picture.TilesY : (SourceY + Height + 31) / 32);
		TileY++) {
		for (int TileX = (SourceX / 32 < 0 ? 0 : SourceX / 32);
			TileX < ((SourceX + Width + 31) / 32 > Picture.TilesX ? Picture.TilesX : (SourceX + Width + 31) / 32);
			TileX++) {
			const TVideoCacheItem *Item = GetVideoPicture(PictureNumber, TileX, TileY);
			const int OffsetY = SourceY - TileY * 32;
			const int PartHeight = (OffsetY + Height > 32 ? 32 : OffsetY + Height);
			const int OffsetX = SourceX - TileX * 32;
			const int PartWidth = (Width + OffsetX > 32 ? 32 : Width + OffsetX);
			const int LeftX = (OffsetX < 0 ? 0 : OffsetX);
			const int TopY = (OffsetY < 0 ? 0 : OffsetY);
			const int DestX = (TileX * 32 - SourceX < 0 ? 0 : TileX * 32 - SourceX);
			const int DestY = (TileY * 32 - SourceY < 0 ? 0 : TileY * 32 - SourceY);
			GetSurface()->BlitClipped(Surface, DestX + x, DestY + y, Item->Surface + 6,
				TRect(Item->x + LeftX, Item->y + TopY, PartWidth - LeftX, PartHeight - TopY), Picture.ColorKey,
				Clip);
		}
	}
	TEND2(1206, Surface, PictureNumber)
}

// --- Text ------------------------------------------------------------------------

// 0x492e30, lines 1215-1239. The first Length characters of Text; x is the
// left end, the middle or the right end by Alignment. Each character is cut
// from its font picture's tile in Color. A blank advances by the width of
// character index 12 (',') (sic).
void PrintTextPart(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Length, int Alignment)
{
	TASSERT2(1215, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	TASSERT(1216, Text!=NULL);
	TASSERT2(1217, Length<=(int)strlen(Text), Length, strlen(Text));
	TASSERTB1(1218, Alignment==LEFT_ALIGN || Alignment==CENTER_ALIGN || Alignment==RIGHT_ALIGN, Alignment,
		"[bug0000845]");
	TBEGIN
	const TFontInfo &Font = Fonts[FontNumber - 1];
	if (Alignment != LEFT_ALIGN) {
		const int Width = GetTextPartWidth(FontNumber, Text, Length);
		if (Alignment == CENTER_ALIGN)
			x -= Width / 2;
		else if (Alignment == RIGHT_ALIGN)
			x -= Width;
	}
	for (int i = 0; i < Length; i++) {
		const char c = Text[i];
		int Index;
		if (c > ' ')
			Index = c - 32;
		else if (c < 0)
			Index = c + 224;
		else if (c == '\n' || c == ' ')
			Index = -1;
		else
			Index = 95;
		if (Index == -1) {
			x += Font.Spacing + Font.Widths[12];
			continue;
		}
		const int SourceY = Font.CharHeight * (Index / Font.CharsPerLine);
		const int SourceX = Font.CharWidth * (Index % Font.CharsPerLine);
		const TVideoCacheItem *Item = GetVideoFontChar(FontNumber, SourceX / 32, SourceY / 32, Color);
		GetSurface()->Blit(Surface, x, y, Item->Surface + 6,
			TRect(Item->x + SourceX % 32, Item->y + SourceY % 32, Font.CharWidth, Font.CharHeight),
			Pictures[Font.PictureNumber - 1].ColorKey);
		x += Font.Widths[Index] + Font.Spacing;
	}
	TEND2(1239, Surface, FontNumber)
}

// 0x4934e0, lines 1249-1273. The whole text, with a clipping rectangle.
void PrintTextClipped(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Alignment, TRect Clip)
{
	TASSERT2(1249, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	TASSERT(1250, Text!=NULL);
	TASSERTB1(1251, Alignment==LEFT_ALIGN || Alignment==CENTER_ALIGN || Alignment==RIGHT_ALIGN, Alignment,
		"[bug0000845]");
	TBEGIN
	const TFontInfo &Font = Fonts[FontNumber - 1];
	const int Length = (int)strlen(Text);
	if (Alignment != LEFT_ALIGN) {
		const int Width = GetTextPartWidth(FontNumber, Text, Length);
		if (Alignment == CENTER_ALIGN)
			x -= Width / 2;
		else if (Alignment == RIGHT_ALIGN)
			x -= Width;
	}
	for (int i = 0; i < Length; i++) {
		const char c = Text[i];
		int Index;
		if (c > ' ')
			Index = c - 32;
		else if (c < 0)
			Index = c + 224;
		else if (c == '\n' || c == ' ')
			Index = -1;
		else
			Index = 95;
		if (Index == -1) {
			x += Font.Spacing + Font.Widths[12];
			continue;
		}
		const int SourceY = Font.CharHeight * (Index / Font.CharsPerLine);
		const int SourceX = Font.CharWidth * (Index % Font.CharsPerLine);
		const TVideoCacheItem *Item = GetVideoFontChar(FontNumber, SourceX / 32, SourceY / 32, Color);
		GetSurface()->BlitClipped(Surface, x, y, Item->Surface + 6,
			TRect(Item->x + SourceX % 32, Item->y + SourceY % 32, Font.CharWidth, Font.CharHeight),
			Pictures[Font.PictureNumber - 1].ColorKey, Clip);
		x += Font.Widths[Index] + Font.Spacing;
	}
	TEND2(1273, Surface, FontNumber)
}

// 0x493af0, lines 1282-1288. The whole text.
void PrintText(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Alignment)
{
	TASSERT2(1282, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	TASSERT(1283, Text!=NULL);
	TASSERTB1(1284, Alignment==LEFT_ALIGN || Alignment==CENTER_ALIGN || Alignment==RIGHT_ALIGN, Alignment,
		"[bug0000845]");
	TBEGIN
	PrintTextPart(Surface, x, y, FontNumber, Color, Text, (int)strlen(Text), Alignment);
	TEND2(1288, Surface, FontNumber)
}

// 0x493f60, lines 1295-1299
void PrintFormattedText(int Surface, int x, int y, TFont Format, const char *Text)
{
	TASSERT1(1295, Format.Alignment==LEFT_ALIGN || Format.Alignment==CENTER_ALIGN || Format.Alignment==RIGHT_ALIGN,
		Format.Alignment);
	TBEGIN
	PrintText(Surface, x, y, Format.FontNumber, Format.Color, Text, Format.Alignment);
	TEND1(1299, Surface)
}

// 0x4941d0, line 1309
void PrintFormattedTextClipped(int Surface, int x, int y, TFont Format, const char *Text, TRect Clip)
{
	TBEGIN
	PrintTextClipped(Surface, x, y, Format.FontNumber, Format.Color, Text, Format.Alignment, Clip);
	TEND1(1309, Surface)
}

// 0x4943b0, line 1319
void PrintFormattedTextPart(int Surface, int x, int y, TFont Format, const char *Text, int Length)
{
	TBEGIN
	PrintTextPart(Surface, x, y, Format.FontNumber, Format.Color, Text, Length, Format.Alignment);
	TEND1(1319, Surface)
}

// 0x491710, line 1343. No try block.
int GetFontHeight(int FontNumber)
{
	TASSERT2(1343, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	return Fonts[FontNumber - 1].Height;
}

// 0x491880, lines 1352-1353. The highest visible character of the text.
// The handlers of this TBEGIN/TEND and of the two following functions were
// removed by the compiler; the exception states remain.
int GetTextHeight(int FontNumber, const char *Text)
{
	TASSERT2(1352, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	TASSERT(1353, Text!=NULL);
	TBEGIN
	const int Length = (int)strlen(Text);
	int Height = 0;
	for (int i = 0; i < Length; i++) {
		const char c = Text[i];
		int Index;
		if (c > ' ')
			Index = c - 32;
		else if (c < 0)
			Index = c + 224;
		else if (c == '\n' || c == ' ')
			Index = -1;
		else
			Index = 95;
		if (Index != -1 && Fonts[FontNumber - 1].Heights[Index] >= Height)
			Height = Fonts[FontNumber - 1].Heights[Index];
	}
	return Height;
	TEND(1368)
}

// 0x494570, lines 1373-1378
int GetTextWidth(int FontNumber, const char *Text)
{
	TASSERT2(1373, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	TASSERT(1374, Text!=NULL);
	TBEGIN
	return GetTextPartWidth(FontNumber, Text, (int)strlen(Text));
	TEND1(1378, FontNumber)
}

// 0x491b80, lines 1386-1388. The width of the first Length characters,
// without the spacing after the last.
int GetTextPartWidth(int FontNumber, const char *Text, int Length)
{
	TASSERT2(1386, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	TASSERT(1387, Text!=NULL);
	TASSERT1(1388, Length<=(int)strlen(Text), Length);
	TBEGIN
	const TFontInfo &Font = Fonts[FontNumber - 1];
	int Width = 0;
	for (int i = 0; i < Length; i++) {
		const char c = Text[i];
		int Index;
		if (c > ' ')
			Index = c - 32;
		else if (c < 0)
			Index = c + 224;
		else if (c == '\n' || c == ' ')
			Index = -1;
		else
			Index = 95;
		if (Index == -1)
			Width += Font.Widths[12] + Font.Spacing;
		else
			Width += Font.Widths[Index] + Font.Spacing;
	}
	if (Length > 0)
		Width -= Font.Spacing;
	return Width;
	TEND(1404)
}

// 0x491f50, lines 1410-1411. The number of characters that fit into Width:
// the index of the first character whose right end (with its spacing) lies
// beyond Width, else the length.
int GetFittingTextLength(int FontNumber, const char *Text, int Width)
{
	TASSERT2(1410, In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	TASSERT(1411, Text!=NULL);
	TBEGIN
	const TFontInfo &Font = Fonts[FontNumber - 1];
	int Position = -Font.Spacing;
	const int Length = (int)strlen(Text);
	for (int i = 0; i < Length; i++) {
		const char c = Text[i];
		int Index;
		if (c > ' ')
			Index = c - 32;
		else if (c < 0)
			Index = c + 224;
		else if (c == '\n' || c == ' ')
			Index = -1;
		else
			Index = 95;
		if (Index == -1)
			Position += Font.Widths[12] + Font.Spacing;
		else
			Position += Font.Widths[Index] + Font.Spacing;
		if (Position > Width)
			return i;
	}
	return Length;
	TEND(1437)
}

// 0x492260. Where a text with Format goes in the box x, y, Width, Height:
// horizontally by its alignment, vertically centred. No exception frame.
void GetTextPosition(int *TextX, int *TextY, int x, int y, int Width, int Height, TFont *Format)
{
	int Offset = 0;
	if (Format->Alignment == LEFT_ALIGN)
		Offset = 0;
	else if (Format->Alignment == CENTER_ALIGN)
		Offset = Width / 2;
	else if (Format->Alignment == RIGHT_ALIGN)
		Offset = Width;
	*TextX = Offset + x;
	*TextY = (Height - GetFontHeight(Format->FontNumber)) / 2 + y;
}

// --- Start-up and shut-down ----------------------------------------------------

// 0x4922c0, line 1474. Empties the video cache and builds it again for the
// current surface (ChangeEngine calls it after a new surface is set up).
void ResetVideoCache()
{
	TBEGIN
	if (VideoCache != nullptr) {
		for (int i = 0; i < VideoCacheSize; i++)
			VideoCache[i].Type = EMPTY_ITEM;
		for (int i = 0; i < HASH_TABLE_SIZE; i++)
			VideoHashTable[i] = -1;
		if (VideoCache != nullptr)
			delete[] VideoCache;
		VideoCache = nullptr;
	}
	InitVideoCache();
	TEND(1474)
}

// 0x492490, line 1498. After Tibia.spr was patched (event 0x46).
void ReloadSprites()
{
	TBEGIN
	ResetVideoCache();
	ClearMemoryCache();
	delete[] Sprites;
	Sprites = nullptr;
	LoadSprites("Tibia.spr");
	TEND(1498)
}

// 0x494900, line 1517. After Tibia.pic was patched (event 0x47).
void ReloadPictures()
{
	TBEGIN
	ResetVideoCache();
	ClearMemoryCache();
	delete[] Fonts;
	Fonts = nullptr;
	for (int i = 0; i < NumberOfPictures; i++)
		delete[] Pictures[i].Offsets;
	delete[] Pictures;
	Pictures = nullptr;
	LoadPictures("Tibia.pic");
	InitFonts();
	TEND(1517)
}

// 0x494af0, line 1539. Called by InitControl with 10 MB. The video cache
// comes later (InitVideoCache), when the surfaces exist.
void InitGraphics(int MemoryCacheBytes)
{
	TBEGIN
	LoadSprites("Tibia.spr");
	LoadPictures("Tibia.pic");
	InitMemoryCache(MemoryCacheBytes);
	InitFonts();
	TEND1(1539, MemoryCacheBytes)
}
