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
// types/GraphicsTypes.h since issue #116) and TVideoCacheItem's constructor (0x48b3f0, below), which the
// binary emits before the first function.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none apart from the
// loaders' inner handlers, which turn a file error into a PROGRAM_ERROR.
//
// Names: see graphics.h and notes/decisions.md ("graphics.cpp").

#include "graphics.h"
#include "Objects.h"
#include "Script.h"
#include "Status.h"
#include "Surface.h"
#include "Utils.h"
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include "Skins.h"
#include <array>
#include <cstddef>
#include <cstring>
#include <memory>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

// Data: TGraphics' members since issue #116 (graphics.h), in the original's .bss order.

// The six tables as std::unique_ptr (issue #58): each a pointer, as the original's. Their elements have no
// destructors that act (a picture's Offsets stays raw: ExitGraphics and ReloadPictures free them first), so a table
// still set when FatalError calls exit(1) is only memory freed during static destruction. Where the original stored a
// new table over one it may still have held (InitMemoryCache, InitFonts, LoadSprites, LoadPictures, InitVideoCache;
// the golden tests call them again), release() drops the old one unfreed first: the leak is the original's.
static_assert(sizeof(std::unique_ptr<uint8[]>) == sizeof(uint8 *) && alignof(std::unique_ptr<uint8[]>) == alignof(uint8 *)
		&& sizeof(std::unique_ptr<TVideoCacheItem[]>) == sizeof(TVideoCacheItem *)
		&& sizeof(std::unique_ptr<TMemoryCacheItem[]>) == sizeof(TMemoryCacheItem *)
		&& sizeof(std::unique_ptr<TSprite[]>) == sizeof(TSprite *) && sizeof(std::unique_ptr<TPicture[]>) == sizeof(TPicture *)
		&& sizeof(std::unique_ptr<TFontInfo[]>) == sizeof(TFontInfo *),
	"the graphics tables' std::unique_ptrs are pointers");

// The arrays std::arrays (issue #60): the original's layouts (no pointers: the same on x64).
static_assert(sizeof(TFontInfo) == 0x71c && offsetof(TFontInfo, Widths) == 0x010 && offsetof(TFontInfo, Heights) == 0x390
		&& sizeof(TFontInfo::Widths) == 0x380,
	"TFontInfo's layout");
static_assert(sizeof(TVideoCacheItem) == 0x5c && offsetof(TVideoCacheItem, Colors) == 0x18 && sizeof(TVideoCacheItem::Colors) == 0x30,
	"TVideoCacheItem's layout");
static_assert(sizeof(patella::TGraphics::MemoryHashTable) == HASH_TABLE_SIZE * sizeof(int)
		&& sizeof(patella::TGraphics::VideoHashTable) == HASH_TABLE_SIZE * sizeof(int),
	"the hash tables");

// 0x48b3f0, passed to the eh vector constructor iterator by InitVideoCache.
// It clears the four colours, then the font colour, which overlaps them
// (the binary stores both).
TVideoCacheItem::TVideoCacheItem()
{
	for (int i = 0; i < 4; i++)
		Colors[i] = TRGBColor(0, 0, 0);
	Tile.Color = TRGBColor(0, 0, 0);
}

// --- The DialogWindow/graphics gap -------------------------------------------

// 0x48b430. The line distance of a font. Numbers below 0 or above
// NumberOfFonts give font 3's; 0 is not caught and reads before the table
// (sic).
int patella::TGraphics::GetFontLineHeight(int FontNumber) const
{
	if (FontNumber < 0 || FontNumber > NumberOfFonts)
		FontNumber = 3;
	return Fonts[FontNumber - 1].LineHeight;
}

// 0x48b460. Empties every memory cache item without touching the LRU list.
void patella::TGraphics::ClearMemoryCache()
{
	for (int i = 0; i < MemoryCacheSize; i++)
		MemoryCache[i].Type = EMPTY_ITEM;
	for (int i = 0; i < HASH_TABLE_SIZE; i++)
		MemoryHashTable[i] = -1;
}

// 0x48b4a0. Called by ExitControl.
void patella::TGraphics::ExitGraphics()
{
	VideoCache.reset();
	VideoCache = nullptr;
	MemoryCacheData.reset();
	MemoryCacheData = nullptr;
	MemoryCache.reset();
	MemoryCache = nullptr;
	Fonts.reset();
	Fonts = nullptr;
	for (int i = 0; i < NumberOfPictures; i++)
		delete[] Pictures[i].Offsets;
	Pictures.reset();
	Pictures = nullptr;
	Sprites.reset();
	Sprites = nullptr;
}

// --- Files ---------------------------------------------------------------------

// 0x48b550, lines 130-148. Tibia.spr: signature, number of sprites, one
// offset per sprite. A file error becomes PROGRAM_ERROR 2. The file is
// closed twice: by close() and by the destructor.
void patella::TGraphics::LoadSprites(const char *FileName)
{
	PATELLA_ASSERT(FileName!=nullptr);
	PATELLA_ASSERT(strlen(FileName)<GRAPHICS_PATH_MAX);
	SpriteFileName.Assign(FileName);
	try {
		TReadBinaryFile File;
		File.open(FileName);
		SpriteSignature = File.readQuad();
		NumberOfSprites = File.readWord();
		static_cast<void>(Sprites.release()); // NOLINT(bugprone-unused-return-value): a table still held leaks, as in the original
		Sprites.reset(new TSprite[NumberOfSprites]);
		for (int i = 0; i < NumberOfSprites; i++)
			Sprites[i].Offset = File.readQuad();
		File.close();
	} catch (TMyException &) {
		throw TProgramError{PROGRAM_ERROR::SPRITES_FILE};
	}
}

// 0x48b9a0, lines 156-212. Decodes a sprite into 32*32 RGB triples: the
// colour key, then runs of transparent and coloured pixels. Opens Tibia.spr
// for every sprite. An empty sprite (offset 0) gets the colour key 0,0,0. A
// file error becomes PROGRAM_ERROR 4. Patella-Modern issue #139, item 3, a deliberate difference
// (notes/decisions.md, "#139"): a run past the 1024th pixel is a data error too (each byte is read, then the pixel
// checked, then written, so every sprite the binary decoded without writing past the buffer decodes as before);
// the binary wrote past the buffer.
void patella::TGraphics::LoadSprite(int SpriteNumber, uint8 *Buffer)
{
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	PATELLA_ASSERT(Buffer!=nullptr);
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
					for (int c = 0; c < 3; c++) {
						const uint8 Value = File.readByte();
						PATELLA_ASSERT_VALUES(Pixel<32*32, Pixel);
						Buffer[Pixel * 3 + c] = Value;
					}
					Read += 3;
					Pixel++;
				}
			}
			File.close();
		} catch (TMyException &) {
			throw TProgramError{PROGRAM_ERROR::SPRITES_DATA};
		}
	}
}

// 0x48bea0, lines 244-272. Tibia.pic: signature, number of pictures, then
// per picture its size in tiles, its colour key and the tile offsets. A file
// error becomes PROGRAM_ERROR 3.
void patella::TGraphics::LoadPictures(const char *FileName)
{
	PATELLA_ASSERT(FileName!=nullptr);
	PATELLA_ASSERT(strlen(FileName)<GRAPHICS_PATH_MAX);
	PictureFileName.Assign(FileName);
	try {
		TReadBinaryFile File;
		File.open(FileName);
		PictureSignature = File.readQuad();
		NumberOfPictures = File.readWord();
		static_cast<void>(Pictures.release()); // NOLINT(bugprone-unused-return-value): a table still held leaks, as in the original
		Pictures.reset(new TPicture[NumberOfPictures]);
		// Patella-Modern issue #139, item 2, a deliberate difference (notes/decisions.md, "#139"): every picture's
		// offsets start empty, so ExitGraphics and ReloadPictures, after a file that ends before the last picture,
		// delete only the tables allocated here. The binary left the later pictures' Offsets indeterminate.
		for (int i = 0; i < NumberOfPictures; i++)
			Pictures[i].Offsets = nullptr;
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
		throw TProgramError{PROGRAM_ERROR::PICTURES_FILE};
	}
}

// 0x48c3b0, lines 280-322. Decodes one tile of a picture like a sprite. The
// tile assertions are inside the inner try block, so a bad tile also becomes
// PROGRAM_ERROR 5, like a file error; since Patella-Modern issue #139, item 3, so does a run past the 1024th pixel
// (as in LoadSprite).
void patella::TGraphics::LoadPictureTile(int PictureNumber, int TileX, int TileY, uint8 *Buffer)
{
	PATELLA_ASSERT_VALUES(In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	PATELLA_ASSERT(Buffer!=nullptr);
	const TPicture &Picture = Pictures[PictureNumber - 1];
	try {
		TReadBinaryFile File;
		File.open(PictureFileName);
		PATELLA_ASSERT_VALUES(In(TileX,0,Picture.TilesX-1), TileX, Picture.TilesX-1);
		PATELLA_ASSERT_VALUES(In(TileY,0,Picture.TilesY-1), TileY, Picture.TilesY-1);
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
				for (int c = 0; c < 3; c++) {
					const uint8 Value = File.readByte();
					PATELLA_ASSERT_VALUES(Pixel<32*32, Pixel);
					Buffer[Pixel * 3 + c] = Value;
				}
				Read += 3;
				Pixel++;
			}
		}
		File.close();
	} catch (TMyException &) {
		throw TProgramError{PROGRAM_ERROR::PICTURES_DATA};
	}
}

// --- Fonts ---------------------------------------------------------------------

// 0x492640, lines 348-387. The
// picture holds 224 characters, CharsPerLine per row. The visible width of a
// character is one more than its rightmost column with a pixel that is not
// the colour key, searched from column CharWidth-1 down to 1; without one it
// is CharWidth. The visible height likewise from the rows CharHeight-1 down
// to 1, else 1. Column 0 and row 0 are never looked at. Height becomes the
// highest row found (not the stored height: a blank character counts -1).
void patella::TGraphics::InitFont(int FontName, int PictureNumber, int CharsPerLine, int CharWidth, int CharHeight,
	int Spacing, int LineHeight)
{
	PATELLA_ASSERT_VALUES(CharsPerLine>0, CharsPerLine);
	PATELLA_ASSERT_VALUES(CharWidth>0, CharWidth);
	PATELLA_ASSERT_VALUES(CharHeight>0, CharHeight);
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
}

// 0x492c30, line 403. The four fonts and their pictures.
void patella::TGraphics::InitFonts()
{
	NumberOfFonts = 4;
	static_cast<void>(Fonts.release()); // NOLINT(bugprone-unused-return-value): a table still held leaks, as in the original
	Fonts.reset(new TFontInfo[4]);
	InitFont(1, 3, 32, 8, 16, 1, 12);
	InitFont(2, 5, 32, 16, 16, -1, 15);
	InitFont(3, 6, 32, 8, 8, 0, 10);
	InitFont(4, 8, 32, 8, 16, 1, 14);
}

// --- The memory cache ------------------------------------------------------------

// 0x48c9c0, line 458. Items in one LRU list, item 0 the most recent.
void patella::TGraphics::InitMemoryCache(int MemoryCacheBytes)
{
	MemoryCacheSize = MemoryCacheBytes / (32 * 32 * 3);
	static_cast<void>(MemoryCacheData.release()); // NOLINT(bugprone-unused-return-value): a table still held leaks, as in the original
	MemoryCacheData.reset(new uint8[MemoryCacheSize * 32 * 32 * 3]);
	static_cast<void>(MemoryCache.release()); // NOLINT(bugprone-unused-return-value): a table still held leaks, as in the original
	MemoryCache.reset(new TMemoryCacheItem[MemoryCacheSize]);
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
}

// 0x48cc30, line 466. Makes an item the most recent one.
void patella::TGraphics::TouchMemoryItem(int Position)
{
	PATELLA_ASSERT_VALUES(In(Position,0,MemoryCacheSize-1), Position);
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

// 0x48ce00. The handlers of the original's catch wrappers here and in the
// other Find functions were removed by the compiler (nothing in them can
// throw); the exception states remain.
int patella::TGraphics::FindMemorySprite(int SpriteNumber) const
{
	int Position = MemoryHashTable[SpriteNumber % HASH_TABLE_SIZE];
	while (Position != -1) {
		const TMemoryCacheItem &Item = MemoryCache[Position];
		if (Item.Type == SPRITE_ITEM && Item.Number == SpriteNumber)
			return Position;
		Position = Item.HashNext;
	}
	return -1;
}

// 0x48ce70
int patella::TGraphics::FindMemoryPicture(int PictureNumber, int TileX, int TileY) const
{
	int Position = MemoryHashTable[(PictureNumber + TileX + TileY) % HASH_TABLE_SIZE];
	while (Position != -1) {
		const TMemoryCacheItem &Item = MemoryCache[Position];
		if (Item.Type == PICTURE_ITEM && Item.Number == PictureNumber && Item.TileX == TileX
			&& Item.TileY == TileY)
			return Position;
		Position = Item.HashNext;
	}
	return -1;
}

// 0x48cef0, line 522. Empties an item and takes it out of its hash chain.
void patella::TGraphics::RemoveMemoryItem(int Position)
{
	PATELLA_ASSERT_VALUES(In(Position,0,MemoryCacheSize-1), Position);
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
void patella::TGraphics::InsertMemoryItem(int Position, int HashValue)
{
	PATELLA_ASSERT_VALUES(In(Position,0,MemoryCacheSize-1), Position);
	PATELLA_ASSERT_VALUES(In(HashValue,0,HASH_TABLE_SIZE-1), HashValue, Position);
	TMemoryCacheItem &Item = MemoryCache[Position];
	PATELLA_ASSERT_VALUES(Item.Type!=EMPTY_ITEM, Position);
	Item.HashValue = HashValue;
	Item.HashPrevious = -1;
	Item.HashNext = MemoryHashTable[HashValue];
	if (MemoryHashTable[HashValue] != -1)
		MemoryCache[MemoryHashTable[HashValue]].HashPrevious = Position;
	MemoryHashTable[HashValue] = Position;
}

// 0x48d440, lines 553-569. A missing sprite replaces the least recent item.
TMemoryCacheItem *patella::TGraphics::GetMemorySprite(int SpriteNumber)
{
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	int Position = -1;
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
}

// 0x48d740, lines 578-595
TMemoryCacheItem *patella::TGraphics::GetMemoryPicture(int PictureNumber, int TileX, int TileY)
{
	PATELLA_ASSERT_VALUES(In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
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
}

// --- The video cache -------------------------------------------------------------

// 0x48da50, line 636. Called by Player.cpp once the surfaces exist and by
// ResetVideoCache. The slots fill the cache surfaces (all as large as
// surface 6) row by row. Without cache surfaces the cache is NULL and
// VideoCacheSize keeps its old value.
void patella::TGraphics::InitVideoCache()
{
	const int Surfaces = GetSurface()->GetCacheSurfaces();
	if (Surfaces == 0) {
		static_cast<void>(VideoCache.release()); // NOLINT(bugprone-unused-return-value): a table still held leaks, as in the original
		return;
	}
	TRect Rect = {0, 0, 0, 0};
	GetSurface()->GetSurfaceRect(6, &Rect);
	const int PerSurface = (Rect.height / 32) * (Rect.width / 32);
	VideoCacheSize = PerSurface * Surfaces;
	static_cast<void>(VideoCache.release()); // NOLINT(bugprone-unused-return-value): a table still held leaks, as in the original
	VideoCache.reset(new TVideoCacheItem[VideoCacheSize]);
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
}

// 0x48dd70, line 644
void patella::TGraphics::TouchVideoItem(int Position)
{
	PATELLA_ASSERT_VALUES(In(Position,0,VideoCacheSize-1), Position);
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
int patella::TGraphics::FindVideoSprite(int SpriteNumber) const
{
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
}

// 0x48dfc0. The fourth colour is compared through the out-of-line
// operator==, the others inline.
int patella::TGraphics::FindVideoMaskedSprite(int SpriteNumber, int MaskNumber, TRGBColor *Colors) const
{
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
}

// 0x48e0f0. Matches font characters of the picture too (sic).
int patella::TGraphics::FindVideoPicture(int PictureNumber, int TileX, int TileY) const
{
	int Position = VideoHashTable[(TileX + PictureNumber + TileY) % HASH_TABLE_SIZE];
	int Count = 0;
	while (Position != -1) {
		const TVideoCacheItem &Item = VideoCache[Position];
		if (Item.Type == PICTURE_ITEM && Item.Number == PictureNumber && Item.TileX == TileX
			&& Item.Tile.TileY == TileY)
			return Position;
		Position = Item.HashNext;
		if (Count++ > VideoCacheSize)
			return -1;
	}
	return -1;
}

// 0x48e190. A plain picture tile matches colour 0,0,0 (sic).
int patella::TGraphics::FindVideoFontChar(int PictureNumber, int TileX, int TileY, TRGBColor Color) const
{
	int Position = VideoHashTable[(TileX + PictureNumber + TileY) % HASH_TABLE_SIZE];
	int Count = 0;
	while (Position != -1) {
		const TVideoCacheItem &Item = VideoCache[Position];
		if (Item.Type == PICTURE_ITEM && Item.Number == PictureNumber && Item.TileX == TileX
			&& Item.Tile.TileY == TileY && Item.Tile.Color == Color)
			return Position;
		Position = Item.HashNext;
		if (Count++ > VideoCacheSize)
			return -1;
	}
	return -1;
}

// 0x48e250, line 762. Unlike RemoveMemoryItem this ignores position 0 (sic):
// when slot 0 is reused it stays in its old hash chain, and InsertVideoItem
// links it into a second one.
void patella::TGraphics::RemoveVideoItem(int Position)
{
	if (Position <= 0)
		return;
	PATELLA_ASSERT_VALUES(In(Position,0,VideoCacheSize-1), Position);
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
void patella::TGraphics::InsertVideoItem(int Position, int HashValue)
{
	PATELLA_ASSERT_VALUES(In(Position,0,VideoCacheSize-1), Position);
	PATELLA_ASSERT_VALUES(In(HashValue,0,HASH_TABLE_SIZE-1), HashValue, Position);
	TVideoCacheItem &Item = VideoCache[Position];
	PATELLA_ASSERT_VALUES(Item.Type!=EMPTY_ITEM, Position, HashValue);
	Item.HashValue = HashValue;
	Item.HashPrevious = -1;
	Item.HashNext = VideoHashTable[HashValue];
	if (VideoHashTable[HashValue] != -1)
		VideoCache[VideoHashTable[HashValue]].HashPrevious = Position;
	VideoHashTable[HashValue] = Position;
}

// 0x48e7b0, lines 793-814. OpenGL and DirectX 9 (engines 1 and 2) get the
// colour key of the slot's surface first.
TVideoCacheItem *patella::TGraphics::GetVideoSprite(int SpriteNumber)
{
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	int Position = -1;
	Position = FindVideoSprite(SpriteNumber);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemorySprite(SpriteNumber);
		if (Engine.curEngine == 1 || Engine.curEngine == 2)
			GetSurface()->SetColorKey(Item.Surface + 6, Sprites[SpriteNumber - 1].ColorKey);
		GetSurface()->WriteImage(Item.Surface + 6, Item.x, Item.y, 32, 32, Tile->Data);
		Item.Type = SPRITE_ITEM;
		Item.Number = SpriteNumber;
		Item.MaskNumber = 0;
		InsertVideoItem(Position, SpriteNumber % HASH_TABLE_SIZE);
	}
	TouchVideoItem(Position);
	return &VideoCache[Position];
}

// 0x48eb30, lines 824-850. A sprite coloured through a mask (outfits).
TVideoCacheItem *patella::TGraphics::GetVideoMaskedSprite(int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	PATELLA_ASSERT_VALUES(In(MaskNumber,1,NumberOfSprites), MaskNumber, NumberOfSprites);
	int Position = FindVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemorySprite(SpriteNumber);
		const TMemoryCacheItem *Mask = GetMemorySprite(MaskNumber);
		if (Engine.curEngine == 1 || Engine.curEngine == 2)
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
}

// 0x48f020, lines 859-885. The colour key test reads the next picture's key
// (Pictures[PictureNumber], past the end for the last picture), and both of
// its branches set the same key (sic). Under AddressSanitizer that read is a heap overflow (issue #70), in
// TRGBColor::operator== (graphics.h).
TVideoCacheItem *patella::TGraphics::GetVideoPicture(int PictureNumber, int TileX, int TileY)
{
	PATELLA_ASSERT_VALUES(In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	int Position = -1;
	Position = FindVideoPicture(PictureNumber, TileX, TileY);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemoryPicture(PictureNumber, TileX, TileY);
		if (Engine.curEngine == 1 || Engine.curEngine == 2) {
			// NOLINTNEXTLINE(bugprone-branch-clone): the original's two identical branches (both the previous picture's key)
			if (Pictures[PictureNumber].ColorKey != TRGBColor(1, 1, 1))
				GetSurface()->SetColorKey(Item.Surface + 6, Pictures[PictureNumber - 1].ColorKey);
			else
				GetSurface()->SetColorKey(Item.Surface + 6, Pictures[PictureNumber - 1].ColorKey);
		}
		GetSurface()->WriteImage(Item.Surface + 6, Item.x, Item.y, 32, 32, Tile->Data);
		Item.Tile.TileY = TileY;
		Item.TileX = TileX;
		Item.Number = PictureNumber;
		Item.Type = PICTURE_ITEM;
		Item.Tile.Color = TRGBColor(0, 0, 0);
		InsertVideoItem(Position, (PictureNumber + TileX + TileY) % HASH_TABLE_SIZE);
	}
	TouchVideoItem(Position);
	return &VideoCache[Position];
}

// 0x48f430, lines 894-919. A tile of a font's picture in a text colour: the
// surface paints the pixels that are not the colour key in Color.
TVideoCacheItem *patella::TGraphics::GetVideoFontChar(int FontNumber, int TileX, int TileY, TRGBColor Color)
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	int Position = -1;
	const int PictureNumber = Fonts[FontNumber - 1].PictureNumber;
	Position = FindVideoFontChar(PictureNumber, TileX, TileY, Color);
	if (Position == -1) {
		RemoveVideoItem(LeastRecentVideoItem);
		Position = LeastRecentVideoItem;
		TVideoCacheItem &Item = VideoCache[LeastRecentVideoItem];
		const TMemoryCacheItem *Tile = GetMemoryPicture(PictureNumber, TileX, TileY);
		std::array<TRGBColor, 4> Colors = {Color, Pictures[PictureNumber - 1].ColorKey, TRGBColor(0, 0, 0),
			TRGBColor(0, 0, 0)};
		if (Engine.curEngine == 1 || Engine.curEngine == 2)
			GetSurface()->SetColorKey(Item.Surface + 6, Pictures[PictureNumber - 1].ColorKey);
		GetSurface()->WriteMaskedImage(Item.Surface + 6, Item.x, Item.y, 32, 32, Tile->Data, nullptr,
			Colors.data());
		Item.TileX = TileX;
		Item.Tile.TileY = TileY;
		Item.Type = PICTURE_ITEM;
		Item.Number = PictureNumber;
		Item.Tile.Color = Color;
		InsertVideoItem(Position, (TileX + PictureNumber + TileY) % HASH_TABLE_SIZE);
	}
	TouchVideoItem(Position);
	return &VideoCache[Position];
}

// --- Sprites ---------------------------------------------------------------------

// 0x48f870, line 933. No try block.
void patella::TGraphics::DrawSprite(int Surface, int x, int y, int SpriteNumber)
{
	if (SpriteNumber == 0)
		return;
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoSprite(SpriteNumber);
	GetSurface()->Blit(Surface, x, y, Item->Surface + 6, TRect(Item->x, Item->y, 32, 32),
		Sprites[SpriteNumber - 1].ColorKey);
}

// 0x48fa60, lines 948-953. Unlike DrawSprite this skips negative numbers too.
void patella::TGraphics::DrawSpriteClipped(int Surface, int x, int y, int SpriteNumber, TRect Clip)
{
	if (SpriteNumber <= 0)
		return;
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoSprite(SpriteNumber);
	GetSurface()->BlitClipped(Surface, x, y, Item->Surface + 6, TRect(Item->x, Item->y, 32, 32),
		Sprites[SpriteNumber - 1].ColorKey, Clip);
}

// 0x48fd80, lines 979-984
void patella::TGraphics::DrawSpriteScaled(int Surface, int x, int y, int Width, int Height, int SpriteNumber,
	TRGBColor Highlight, TRect Clip)
{
	if (SpriteNumber == 0)
		return;
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoSprite(SpriteNumber);
	GetSurface()->StretchBlit(Surface, TRect(x, y, Width, Height), Item->Surface + 6,
		TRect(Item->x, Item->y, 32, 32), Sprites[SpriteNumber - 1].ColorKey, Highlight, Clip);
}

// 0x4900d0, lines 998-1003. The assertions are inside the try block here.
void patella::TGraphics::DrawMaskedSprite(int Surface, int x, int y, int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	if (MaskNumber == 0) {
		DrawSprite(Surface, x, y, SpriteNumber);
		return;
	}
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	PATELLA_ASSERT_VALUES(In(MaskNumber,1,NumberOfSprites), MaskNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
	GetSurface()->Blit(Surface, x, y, Item->Surface + 6, TRect(Item->x, Item->y, 32, 32),
		Sprites[SpriteNumber - 1].ColorKey);
}

// 0x4904b0, lines 1059-1064
void patella::TGraphics::DrawMaskedSpriteScaled(int Surface, int x, int y, int Width, int Height, int SpriteNumber,
	int MaskNumber, TRGBColor *Colors, TRGBColor Highlight, TRect Clip)
{
	if (MaskNumber == 0) {
		DrawSpriteScaled(Surface, x, y, Width, Height, SpriteNumber, Highlight, Clip);
		return;
	}
	PATELLA_ASSERT_VALUES(In(SpriteNumber,1,NumberOfSprites), SpriteNumber, NumberOfSprites);
	PATELLA_ASSERT_VALUES(In(MaskNumber,1,NumberOfSprites), MaskNumber, NumberOfSprites);
	const TVideoCacheItem *Item = GetVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
	GetSurface()->StretchBlit(Surface, TRect(x, y, Width, Height), Item->Surface + 6,
		TRect(Item->x, Item->y, 32, 32), Sprites[SpriteNumber - 1].ColorKey, Highlight, Clip);
}

// 0x494ca0, line 1100. An
// object in a Size x Size box whose bottom right corner is x+Size, y+Size,
// in the animation phase of the action counter; the count of a cumulative
// object above 1 (or any count with ShowCount) is written in its bottom
// right corner. Count holds three digits; more overflow it (sic). A phase
// count or exact size of 0 in Tibia.dat throws a TMyException
// (PATELLA_DIVISOR, issue #67).
void patella::TGraphics::DrawObject(int Surface, int x, int y, int Size, TObject Obj, TRGBColor Highlight, TRect Clip,
	TFont Font, bool ShowCount)
{
	if (Obj.Type == 0)
		return;
	int PatternX = 1;
	int PatternY = 1;
	if (GetObjectFlag(Obj.Type, CUMULATIVE))
		GetCumulativePattern(Obj, &PatternX, &PatternY);
	if (GetObjectFlag(Obj.Type, LIQUIDPOOL) || GetObjectFlag(Obj.Type, LIQUIDCONTAINER))
		GetLiquidPattern(Obj, &PatternX, &PatternY);
	const int Action = GetActionNumber();
	const int Phase = Action / 5 % PATELLA_DIVISOR(GetObjectPhases(Obj.Type)) + 1;
	const int Scale = Size * 32 / PATELLA_DIVISOR(GetObjectExactSize(Obj.Type));
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
}

// 0x495080, line 1137. A creature's outfit facing south (direction 2) in
// its four colours (indices of the outfit palette), with the colour mask on
// layer 2. Outfit 0 shows the object Colors[0] instead (none if 0). An exact
// size of 0 in Tibia.dat throws a TMyException (PATELLA_DIVISOR, issue #67).
void patella::TGraphics::DrawOutfit(int Surface, int x, int y, int Size, int OutfitNumber, patella::TSpan<const int, 4> Colors,
	TRGBColor Highlight, TRect Clip, TFont Font)
{
	if (OutfitNumber == 0) {
		if (Colors[0] != 0) {
			const TObject Obj = {Colors[0], 0, 0};
			DrawObject(Surface, x, y, Size, Obj, Highlight, Clip, Font, false);
		}
		return;
	}
	std::array<TRGBColor, 4> OutfitColors; // zeroed by the constructor (the binary's array loop)
	int R, G, B;
	GetOutfitColor(Colors[0], &R, &G, &B);
	OutfitColors[0] = TRGBColor(R, G, B);
	GetOutfitColor(Colors[1], &R, &G, &B);
	OutfitColors[1] = TRGBColor(R, G, B);
	GetOutfitColor(Colors[2], &R, &G, &B);
	OutfitColors[2] = TRGBColor(R, G, B);
	GetOutfitColor(Colors[3], &R, &G, &B);
	OutfitColors[3] = TRGBColor(R, G, B);
	const int Scale = Size * 32 / PATELLA_DIVISOR(GetOutfitExactSize(OutfitNumber));
	for (int px = 1; px <= GetOutfitWidth(OutfitNumber); px++) {
		for (int py = 1; py <= GetOutfitHeight(OutfitNumber); py++) {
			const int SpriteNumber = GetOutfitSpriteNumber(OutfitNumber, px, py, 1, 2, 1);
			int MaskNumber;
			if (GetOutfitLayers(OutfitNumber) == 2)
				MaskNumber = GetOutfitSpriteNumber(OutfitNumber, px, py, 2, 2, 1);
			else
				MaskNumber = 0;
			DrawMaskedSpriteScaled(Surface, x - px * Scale + Size, y - py * Scale + Size, Scale, Scale,
				SpriteNumber, MaskNumber, OutfitColors.data(), Highlight, Clip);
		}
	}
}

// --- Pictures --------------------------------------------------------------------

// 0x490920, lines 1145-1155. The binary interleaves the loops with the
// catch blocks (Ghidra splits off 0x490a58).
void patella::TGraphics::DrawPicture(int Surface, int x, int y, int PictureNumber)
{
	PATELLA_ASSERT_VALUES(In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	const TPicture &Picture = Pictures[PictureNumber - 1];
	for (int TileY = 0; TileY < Picture.TilesY; TileY++) {
		for (int TileX = 0; TileX < Picture.TilesX; TileX++) {
			const TVideoCacheItem *Item = GetVideoPicture(PictureNumber, TileX, TileY);
			GetSurface()->Blit(Surface, TileX * 32 + x, TileY * 32 + y, Item->Surface + 6,
				TRect(Item->x, Item->y, 32, 32), Picture.ColorKey);
		}
	}
}

// 0x490c60, lines 1164-1182. The part SourceX, SourceY, Width, Height of a
// picture, tile by tile. The exception tables have the states of two more
// assertions between the first one and the try block; the empty part returns
// before them, so the compiler dropped them (their line numbers are
// unknown).
void patella::TGraphics::DrawPicturePart(int Surface, int x, int y, int PictureNumber, int SourceX, int SourceY,
	int Width, int Height)
{
	PATELLA_ASSERT_VALUES(In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	if (Width <= 0 || Height <= 0)
		return;
	PATELLA_ASSERT_VALUES(Width>0, Width);
	PATELLA_ASSERT_VALUES(Height>0, Height);
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
}

// 0x4910e0, lines 1191-1206. As DrawPicturePart, with a clipping rectangle;
// an empty part fails the assertions.
void patella::TGraphics::DrawPicturePartClipped(int Surface, int x, int y, int PictureNumber, int SourceX, int SourceY,
	int Width, int Height, TRect Clip)
{
	PATELLA_ASSERT_VALUES(In(PictureNumber,1,NumberOfPictures), PictureNumber, NumberOfPictures);
	PATELLA_ASSERT_VALUES(Width>0, Width);
	PATELLA_ASSERT_VALUES(Height>0, Height);
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
}

// --- Text ------------------------------------------------------------------------

// 0x492e30, lines 1215-1239. The first Length characters of Text; x is the
// left end, the middle or the right end by Alignment. Each character is cut
// from its font picture's tile in Color. A blank advances by the width of
// character index 12 (',') (sic).
void patella::TGraphics::PrintTextPart(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Length, int Alignment)
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	PATELLA_ASSERT(Text!=nullptr);
	PATELLA_ASSERT_VALUES(Length<=(int)strlen(Text), Length, strlen(Text));
	PATELLA_ASSERT_VALUES(Alignment==LEFT_ALIGN || Alignment==CENTER_ALIGN || Alignment==RIGHT_ALIGN, Alignment);
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
}

// 0x4934e0, lines 1249-1273. The whole text, with a clipping rectangle.
void patella::TGraphics::PrintTextClipped(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Alignment, TRect Clip)
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	PATELLA_ASSERT(Text!=nullptr);
	PATELLA_ASSERT_VALUES(Alignment==LEFT_ALIGN || Alignment==CENTER_ALIGN || Alignment==RIGHT_ALIGN, Alignment);
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
}

// 0x493af0, lines 1282-1288. The whole text.
void patella::TGraphics::PrintText(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text,
	int Alignment)
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	PATELLA_ASSERT(Text!=nullptr);
	PATELLA_ASSERT_VALUES(Alignment==LEFT_ALIGN || Alignment==CENTER_ALIGN || Alignment==RIGHT_ALIGN, Alignment);
	PrintTextPart(Surface, x, y, FontNumber, Color, Text, (int)strlen(Text), Alignment);
}

// 0x493f60, lines 1295-1299
void patella::TGraphics::PrintFormattedText(int Surface, int x, int y, TFont Format, const char *Text)
{
	PATELLA_ASSERT_VALUES(Format.Alignment==LEFT_ALIGN || Format.Alignment==CENTER_ALIGN || Format.Alignment==RIGHT_ALIGN,
		Format.Alignment);
	PrintText(Surface, x, y, Format.FontNumber, Format.Color, Text, Format.Alignment);
}

// 0x4941d0, line 1309
void patella::TGraphics::PrintFormattedTextClipped(int Surface, int x, int y, TFont Format, const char *Text, TRect Clip)
{
	PrintTextClipped(Surface, x, y, Format.FontNumber, Format.Color, Text, Format.Alignment, Clip);
}

// 0x4943b0, line 1319
void patella::TGraphics::PrintFormattedTextPart(int Surface, int x, int y, TFont Format, const char *Text, int Length)
{
	PrintTextPart(Surface, x, y, Format.FontNumber, Format.Color, Text, Length, Format.Alignment);
}

// 0x491710, line 1343. No try block.
int patella::TGraphics::GetFontHeight(int FontNumber) const
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	return Fonts[FontNumber - 1].Height;
}

// 0x491880, lines 1352-1353. The highest visible character of the text.
// The handlers of the original's catch wrappers here and of the two following
// functions were removed by the compiler; the exception states remain.
int patella::TGraphics::GetTextHeight(int FontNumber, const char *Text) const
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	PATELLA_ASSERT(Text!=nullptr);
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
}

// 0x494570, lines 1373-1378
int patella::TGraphics::GetTextWidth(int FontNumber, const char *Text) const
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	PATELLA_ASSERT(Text!=nullptr);
	return GetTextPartWidth(FontNumber, Text, (int)strlen(Text));
}

// 0x491b80, lines 1386-1388. The width of the first Length characters,
// without the spacing after the last.
int patella::TGraphics::GetTextPartWidth(int FontNumber, const char *Text, int Length) const
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	PATELLA_ASSERT(Text!=nullptr);
	PATELLA_ASSERT_VALUES(Length<=(int)strlen(Text), Length);
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
}

// 0x491f50, lines 1410-1411. The number of characters that fit into Width:
// the index of the first character whose right end (with its spacing) lies
// beyond Width, else the length.
int patella::TGraphics::GetFittingTextLength(int FontNumber, const char *Text, int Width) const
{
	PATELLA_ASSERT_VALUES(In(FontNumber,1,NumberOfFonts), FontNumber, NumberOfFonts);
	PATELLA_ASSERT(Text!=nullptr);
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
}

// 0x492260. Where a text with Format goes in the box x, y, Width, Height:
// horizontally by its alignment, vertically centred. No exception frame.
void patella::TGraphics::GetTextPosition(int *TextX, int *TextY, int x, int y, int Width, int Height, TFont *Format) const
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
void patella::TGraphics::ResetVideoCache()
{
	if (VideoCache != nullptr) {
		for (int i = 0; i < VideoCacheSize; i++)
			VideoCache[i].Type = EMPTY_ITEM;
		for (int i = 0; i < HASH_TABLE_SIZE; i++)
			VideoHashTable[i] = -1;
		if (VideoCache != nullptr)
			VideoCache.reset();
		VideoCache = nullptr;
	}
	InitVideoCache();
}

// 0x492490, line 1498. After Tibia.spr was patched (event 0x46).
void patella::TGraphics::ReloadSprites()
{
	ResetVideoCache();
	ClearMemoryCache();
	Sprites.reset();
	Sprites = nullptr;
	LoadSprites("Tibia.spr");
}

// 0x494900, line 1517. After Tibia.pic was patched (event 0x47).
void patella::TGraphics::ReloadPictures()
{
	ResetVideoCache();
	ClearMemoryCache();
	Fonts.reset();
	Fonts = nullptr;
	for (int i = 0; i < NumberOfPictures; i++)
		delete[] Pictures[i].Offsets;
	Pictures.reset();
	Pictures = nullptr;
	LoadPictures("Tibia.pic");
	InitFonts();
}

// 0x494af0, line 1539. Called by InitControl with 10 MB. The video cache
// comes later (InitVideoCache), when the surfaces exist.
void patella::TGraphics::InitGraphics(int MemoryCacheBytes)
{
	LoadSprites("Tibia.spr");
	LoadPictures("Tibia.pic");
	InitMemoryCache(MemoryCacheBytes);
	InitFonts();
}

// The original's free functions (issue #116): each calls its member on the one TClient's TGraphics. The other files
// call them, and the golden tests fake them by these symbols (the window tests' drawing and text functions).

void InitGraphics(int MemoryCacheBytes)
{
	patella::bridge::Graphics().InitGraphics(MemoryCacheBytes);
}

void InitVideoCache()
{
	patella::bridge::Graphics().InitVideoCache();
}

void ResetVideoCache()
{
	patella::bridge::Graphics().ResetVideoCache();
}

void ReloadSprites()
{
	patella::bridge::Graphics().ReloadSprites();
}

void ReloadPictures()
{
	patella::bridge::Graphics().ReloadPictures();
}

void ExitGraphics()
{
	patella::bridge::Graphics().ExitGraphics();
}

void LoadSprites(const char *FileName)
{
	patella::bridge::Graphics().LoadSprites(FileName);
}

void LoadSprite(int SpriteNumber, uint8 *Buffer)
{
	patella::bridge::Graphics().LoadSprite(SpriteNumber, Buffer);
}

void LoadPictures(const char *FileName)
{
	patella::bridge::Graphics().LoadPictures(FileName);
}

void LoadPictureTile(int PictureNumber, int TileX, int TileY, uint8 *Buffer)
{
	patella::bridge::Graphics().LoadPictureTile(PictureNumber, TileX, TileY, Buffer);
}

void InitFont(int FontName, int PictureNumber, int CharsPerLine, int CharWidth, int CharHeight, int Spacing, int LineHeight)
{
	patella::bridge::Graphics().InitFont(FontName, PictureNumber, CharsPerLine, CharWidth, CharHeight, Spacing, LineHeight);
}

void InitFonts()
{
	patella::bridge::Graphics().InitFonts();
}

void InitMemoryCache(int MemoryCacheBytes)
{
	patella::bridge::Graphics().InitMemoryCache(MemoryCacheBytes);
}

void ClearMemoryCache()
{
	patella::bridge::Graphics().ClearMemoryCache();
}

void TouchMemoryItem(int Position)
{
	patella::bridge::Graphics().TouchMemoryItem(Position);
}

int FindMemorySprite(int SpriteNumber)
{
	return patella::bridge::Graphics().FindMemorySprite(SpriteNumber);
}

int FindMemoryPicture(int PictureNumber, int TileX, int TileY)
{
	return patella::bridge::Graphics().FindMemoryPicture(PictureNumber, TileX, TileY);
}

void RemoveMemoryItem(int Position)
{
	patella::bridge::Graphics().RemoveMemoryItem(Position);
}

void InsertMemoryItem(int Position, int HashValue)
{
	patella::bridge::Graphics().InsertMemoryItem(Position, HashValue);
}

TMemoryCacheItem *GetMemorySprite(int SpriteNumber)
{
	return patella::bridge::Graphics().GetMemorySprite(SpriteNumber);
}

TMemoryCacheItem *GetMemoryPicture(int PictureNumber, int TileX, int TileY)
{
	return patella::bridge::Graphics().GetMemoryPicture(PictureNumber, TileX, TileY);
}

void TouchVideoItem(int Position)
{
	patella::bridge::Graphics().TouchVideoItem(Position);
}

int FindVideoSprite(int SpriteNumber)
{
	return patella::bridge::Graphics().FindVideoSprite(SpriteNumber);
}

int FindVideoMaskedSprite(int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	return patella::bridge::Graphics().FindVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
}

int FindVideoPicture(int PictureNumber, int TileX, int TileY)
{
	return patella::bridge::Graphics().FindVideoPicture(PictureNumber, TileX, TileY);
}

int FindVideoFontChar(int PictureNumber, int TileX, int TileY, TRGBColor Color)
{
	return patella::bridge::Graphics().FindVideoFontChar(PictureNumber, TileX, TileY, Color);
}

void RemoveVideoItem(int Position)
{
	patella::bridge::Graphics().RemoveVideoItem(Position);
}

void InsertVideoItem(int Position, int HashValue)
{
	patella::bridge::Graphics().InsertVideoItem(Position, HashValue);
}

TVideoCacheItem *GetVideoSprite(int SpriteNumber)
{
	return patella::bridge::Graphics().GetVideoSprite(SpriteNumber);
}

TVideoCacheItem *GetVideoMaskedSprite(int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	return patella::bridge::Graphics().GetVideoMaskedSprite(SpriteNumber, MaskNumber, Colors);
}

TVideoCacheItem *GetVideoPicture(int PictureNumber, int TileX, int TileY)
{
	return patella::bridge::Graphics().GetVideoPicture(PictureNumber, TileX, TileY);
}

TVideoCacheItem *GetVideoFontChar(int FontNumber, int TileX, int TileY, TRGBColor Color)
{
	return patella::bridge::Graphics().GetVideoFontChar(FontNumber, TileX, TileY, Color);
}

void DrawSprite(int Surface, int x, int y, int SpriteNumber)
{
	patella::bridge::Graphics().DrawSprite(Surface, x, y, SpriteNumber);
}

void DrawSpriteClipped(int Surface, int x, int y, int SpriteNumber, TRect Clip)
{
	patella::bridge::Graphics().DrawSpriteClipped(Surface, x, y, SpriteNumber, Clip);
}

void DrawSpriteScaled(int Surface, int x, int y, int Width, int Height, int SpriteNumber, TRGBColor Highlight, TRect Clip)
{
	patella::bridge::Graphics().DrawSpriteScaled(Surface, x, y, Width, Height, SpriteNumber, Highlight, Clip);
}

void DrawMaskedSprite(int Surface, int x, int y, int SpriteNumber, int MaskNumber, TRGBColor *Colors)
{
	patella::bridge::Graphics().DrawMaskedSprite(Surface, x, y, SpriteNumber, MaskNumber, Colors);
}

void DrawMaskedSpriteScaled(int Surface, int x, int y, int Width, int Height, int SpriteNumber, int MaskNumber, TRGBColor *Colors, TRGBColor Highlight, TRect Clip)
{
	patella::bridge::Graphics().DrawMaskedSpriteScaled(Surface, x, y, Width, Height, SpriteNumber, MaskNumber, Colors, Highlight, Clip);
}

void DrawObject(int Surface, int x, int y, int Size, TObject Obj, TRGBColor Highlight, TRect Clip, TFont Font, bool ShowCount)
{
	patella::bridge::Graphics().DrawObject(Surface, x, y, Size, Obj, Highlight, Clip, Font, ShowCount);
}

void DrawOutfit(int Surface, int x, int y, int Size, int OutfitNumber, patella::TSpan<const int, 4> Colors, TRGBColor Highlight, TRect Clip, TFont Font)
{
	patella::bridge::Graphics().DrawOutfit(Surface, x, y, Size, OutfitNumber, Colors, Highlight, Clip, Font);
}

void DrawPicture(int Surface, int x, int y, int PictureNumber)
{
	patella::bridge::Graphics().DrawPicture(Surface, x, y, PictureNumber);
}

void DrawPicturePart(int Surface, int x, int y, int PictureNumber, int SourceX, int SourceY, int Width, int Height)
{
	patella::bridge::Graphics().DrawPicturePart(Surface, x, y, PictureNumber, SourceX, SourceY, Width, Height);
}

void DrawPicturePartClipped(int Surface, int x, int y, int PictureNumber, int SourceX, int SourceY, int Width, int Height, TRect Clip)
{
	patella::bridge::Graphics().DrawPicturePartClipped(Surface, x, y, PictureNumber, SourceX, SourceY, Width, Height, Clip);
}

void PrintTextPart(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text, int Length, int Alignment)
{
	patella::bridge::Graphics().PrintTextPart(Surface, x, y, FontNumber, Color, Text, Length, Alignment);
}

void PrintTextClipped(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text, int Alignment, TRect Clip)
{
	patella::bridge::Graphics().PrintTextClipped(Surface, x, y, FontNumber, Color, Text, Alignment, Clip);
}

void PrintText(int Surface, int x, int y, int FontNumber, TRGBColor Color, const char *Text, int Alignment)
{
	patella::bridge::Graphics().PrintText(Surface, x, y, FontNumber, Color, Text, Alignment);
}

void PrintFormattedText(int Surface, int x, int y, TFont Format, const char *Text)
{
	patella::bridge::Graphics().PrintFormattedText(Surface, x, y, Format, Text);
}

void PrintFormattedTextClipped(int Surface, int x, int y, TFont Format, const char *Text, TRect Clip)
{
	patella::bridge::Graphics().PrintFormattedTextClipped(Surface, x, y, Format, Text, Clip);
}

void PrintFormattedTextPart(int Surface, int x, int y, TFont Format, const char *Text, int Length)
{
	patella::bridge::Graphics().PrintFormattedTextPart(Surface, x, y, Format, Text, Length);
}

int GetFontLineHeight(int FontNumber)
{
	return patella::bridge::Graphics().GetFontLineHeight(FontNumber);
}

int GetFontHeight(int FontNumber)
{
	return patella::bridge::Graphics().GetFontHeight(FontNumber);
}

int GetTextHeight(int FontNumber, const char *Text)
{
	return patella::bridge::Graphics().GetTextHeight(FontNumber, Text);
}

int GetTextWidth(int FontNumber, const char *Text)
{
	return patella::bridge::Graphics().GetTextWidth(FontNumber, Text);
}

int GetTextPartWidth(int FontNumber, const char *Text, int Length)
{
	return patella::bridge::Graphics().GetTextPartWidth(FontNumber, Text, Length);
}

int GetFittingTextLength(int FontNumber, const char *Text, int Width)
{
	return patella::bridge::Graphics().GetFittingTextLength(FontNumber, Text, Width);
}

void GetTextPosition(int *TextX, int *TextY, int x, int y, int Width, int Height, TFont *Format)
{
	patella::bridge::Graphics().GetTextPosition(TextX, TextY, x, y, Width, Height, Format);
}

// Skins.cpp's free functions (Skins.h), here because Skins.cpp is patella_core's, which has no TClient (issue #116):
// each calls its member on the one TClient's TSkins. Control.cpp calls them, the golden tests call and fake them.
void InitSkins()
{
	patella::bridge::Skins().InitSkins();
}

void ExitSkins()
{
	patella::bridge::Skins().ExitSkins();
}
// NOLINTEND(cppcoreguidelines-owning-memory)
