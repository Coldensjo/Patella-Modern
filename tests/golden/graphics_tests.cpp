// difftest.exe's Graphics test (moved here from tools/difftest/graphicstest.cpp, issue #28): graphics.cpp
// against the original, as a golden-corpus test (golden.h).
//
// In the oracle the image's malloc/free and stdio are redirected (PatchScript), so both sides load the real
// Tibia.spr and Tibia.pic (next to Tibia.exe; the runner gets them from the corpus, golden::DataFile): every
// sprite and every picture tile is decoded on both sides and compared. The surface is a recording fake: both
// sides' GetSurface return the same TTestSurface object (the image's surface pointer 0x71d0cc is set to it;
// the runner redirects our GetSurface with golden::RedirectOwn), whose slots write their arguments, and the
// tiles they get, to a log per side. After each operation the logs, the memory cache, the video cache, their
// hash tables and LRU lists are compared. The caches are small so that items are replaced all the time (and
// the video cache's slot 0 bug is reached). Objects and outfits use the real Tibia.dat, loaded on both sides.
// Uninitialised fields of freshly allocated cache items (never read before they are written) are cleared on
// both sides so that they can be compared. Failing assertions are not exercised.
//
// The original's state is recorded by value (plan 4.3): the tables without their pointers (a memory cache
// item's Data as its offset in the cache's data, a picture's offsets as an array), the caches by their
// changes since the last record (golden::OutputRuns), the file names relative to the data directory. The
// 32*32 RGB tiles (the decoded sprites and picture tiles, the memory cache's slots and the tiles the surface
// log holds; 115,306 written to the video cache alone) go through a dictionary of the tiles recorded so far
// (RecordTile): a tile recorded before is its index, so each distinct tile is in the corpus once.

#include "golden_test.h"

#include "Objects.h"
#include "Status.h"
#include "Surface.h"
#include "graphics.h"
#include "testsurface.h"

#include "golden_patch.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

static_assert(sizeof(TRGBColor) == 12, "TRGBColor is 12 bytes");
static_assert(sizeof(TRect) == 16, "TRect is 16 bytes");
static_assert(sizeof(TFont) == 20, "TFont is 20 bytes");
static_assert(sizeof(TSprite) == 0x10, "TSprite is 0x10 bytes");
static_assert(sizeof(TFontInfo) == 0x71c, "TFontInfo is 0x71c bytes");
static_assert(sizeof(TVideoCacheItem) == 0x5c, "TVideoCacheItem is 0x5c bytes");
static_assert(offsetof(TVideoCacheItem, Tile.Color) == 0x1c, "TVideoCacheItem::Tile.Color at +0x1c");
static_assert(offsetof(TVideoCacheItem, Previous) == 0x48, "TVideoCacheItem::Previous at +0x48");

namespace GraphicsTest {

// Our graphics.cpp's data: TGraphics' members since issue #116 (the one TClient's, through the bridge).
patella::TGraphics &Ours()
{
	return patella::bridge::Graphics();
}

const int TILE = 32 * 32 * 3;

// --- The recording surface -------------------------------------------------

enum { OURS = 0,
	ORIGINAL = 1 };
int Side;
std::string Log[2];
// Where the original's log holds tiles (the oracle; recorded through the dictionary).
std::vector<uint32> LogTiles;
int GraphicsCacheSurfaces = 2;
int CacheWidth = 96;
int CacheHeight = 64;
int Writes, Blits, DecodedSprites, DecodedTiles; // our side's statistics

void Put(int Value) { Log[Side].append((const char *)&Value, sizeof(Value)); }
void PutColor(TRGBColor Color)
{
	Put(Color.R);
	Put(Color.G);
	Put(Color.B);
}
void PutRect(TRect Rect)
{
	Put(Rect.x);
	Put(Rect.y);
	Put(Rect.width);
	Put(Rect.height);
}
void PutTile(const uint8 *Data)
{
	if (Data == nullptr)
		Put(-1);
	else {
		if (Side == ORIGINAL)
			LogTiles.push_back((uint32)Log[Side].size());
		Log[Side].append((const char *)Data, TILE);
	}
}

// --- The tile dictionary ------------------------------------------------------

// Every tile recorded so far, in the order recorded; the same on both sides.
std::vector<std::string> Tiles;
std::unordered_map<std::string, int> TileIndex; // the oracle's look-up

// A 32*32 RGB tile of the original: the oracle records Tile (as the index of an equal tile recorded before,
// or its bytes), the runner gets the recorded tile into Tile.
void RecordTile(const char *Name, uint8 *Tile)
{
	if (golden::Mode() == golden::TMode::Off)
		return;
	int Index = -1;
	golden::Output(Name, Index);
	if (Index >= 0) {
		if (Index >= (int)Tiles.size())
			throw golden::TStale{std::string(Name) + ": a tile that was not recorded"};
		memcpy(Tile, Tiles[Index].data(), TILE);
		return;
	}
	golden::OutputBytes(Name, Tile, TILE);
	Tiles.push_back(std::string((const char *)Tile, TILE));
	ORIGINAL(TileIndex[Tiles.back()] = (int)Tiles.size() - 1);
}

// The original's log (the oracle's Log[ORIGINAL]; the runner's from the corpus): the log without its tiles,
// where they were, and the tiles through the dictionary.
void OutputOriginalLog()
{
	if (golden::Mode() == golden::TMode::Off) {
		LogTiles.clear();
		return;
	}
	uint32 Count = ORIGINAL_VALUE("log tiles", uint32, LogTiles.size());
	std::vector<uint32> Offsets(Count);
	ORIGINAL(Offsets = LogTiles);
	golden::OutputArray("log tile offsets", Offsets.data(), Count);
	std::string Rest;
	Rest = golden::OutputText("log", Rest.data(), Rest.size());
	std::string Full;
	size_t From2 = 0;
	uint8 Tile[TILE];
	for (uint32 i = 0; i < Count; i++) {
		size_t At = Offsets[i] - (size_t)i * TILE; // in Rest
		if (At < From2 || At > Rest.size())
			throw golden::TStale{"log: a tile outside the log"};
		Full.append(Rest, From2, At - From2);
		ORIGINAL(memcpy(Tile, Log[ORIGINAL].data() + Offsets[i], TILE));
		RecordTile("log tile", Tile);
		Full.append((const char *)Tile, TILE);
		From2 = At;
	}
	Full.append(Rest, From2, std::string::npos);
	ORIGINAL_OR((void)0, Log[ORIGINAL] = Full);
	LogTiles.clear();
}

} // namespace GraphicsTest

TTestSurface::TTestSurface() {}
void TTestSurface::WriteImage(int ToSurface, int x, int y, int ImageWidth, int ImageHeight, uint8 *Data)
{
	using namespace GraphicsTest;
	Writes += Side == OURS;
	Put(0x28);
	Put(ToSurface);
	Put(x);
	Put(y);
	Put(ImageWidth);
	Put(ImageHeight);
	PutTile(Data);
}
void TTestSurface::WriteMaskedImage(int ToSurface, int x, int y, int ImageWidth, int ImageHeight, uint8 *Data, uint8 *Mask,
	TRGBColor *Colors)
{
	using namespace GraphicsTest;
	Writes += Side == OURS;
	Put(0x2c);
	Put(ToSurface);
	Put(x);
	Put(y);
	Put(ImageWidth);
	Put(ImageHeight);
	PutTile(Data);
	PutTile(Mask);
	for (int i = 0; i < 4; i++)
		PutColor(Colors[i]);
}
void TTestSurface::Blit(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey)
{
	using namespace GraphicsTest;
	Blits += Side == OURS;
	Put(0x34);
	Put(ToSurface);
	Put(x);
	Put(y);
	Put(FromSurface);
	Put(FromRect.x);
	Put(FromRect.y);
	Put(FromRect.width);
	Put(FromRect.height);
	PutColor(ColorKey);
}
void TTestSurface::BlitClipped(int ToSurface, int x, int y, int FromSurface, TRect FromRect, TRGBColor ColorKey,
	TRect Clip)
{
	using namespace GraphicsTest;
	Blits += Side == OURS;
	Put(0x3c);
	Put(ToSurface);
	Put(x);
	Put(y);
	Put(FromSurface);
	Put(FromRect.x);
	Put(FromRect.y);
	Put(FromRect.width);
	Put(FromRect.height);
	PutColor(ColorKey);
	PutRect(Clip);
}
void TTestSurface::StretchBlit(int ToSurface, TRect ToRect, int FromSurface, TRect FromRect, TRGBColor ColorKey,
	TRGBColor Highlight, TRect Clip)
{
	using namespace GraphicsTest;
	Blits += Side == OURS;
	Put(0x48);
	Put(ToSurface);
	Put(ToRect.x);
	Put(ToRect.y);
	Put(ToRect.width);
	Put(ToRect.height);
	Put(FromSurface);
	Put(FromRect.x);
	Put(FromRect.y);
	Put(FromRect.width);
	Put(FromRect.height);
	PutColor(ColorKey);
	PutColor(Highlight);
	PutRect(Clip);
}
void TTestSurface::SetSourceColorKey(int Surface, TRGBColor ColorKey)
{
	using namespace GraphicsTest;
	Put(0x54);
	Put(Surface);
	PutColor(ColorKey);
}
int TTestSurface::GetCacheSurfaces()
{
	using namespace GraphicsTest;
	Put(0x90);
	return GraphicsCacheSurfaces;
}
void TTestSurface::GetSurfaceRect(int Surface, TRect *Rect)
{
	using namespace GraphicsTest;
	Put(0x94);
	Put(Surface);
	PutRect(*Rect);
	Rect->width = CacheWidth;
	Rect->height = CacheHeight;
}

namespace GraphicsTest {


// --- Comparisons -----------------------------------------------------------------

// The directory of the data files (the oracle: Tibia.exe's; the runner: its copy of the corpus's).
std::string DataDirectory;

// A file name relative to the data directory: the directory as "<data>/" (the runner's differs from the
// oracle's), any other name unchanged.
std::string Relative(const char *Name)
{
	if (!DataDirectory.empty() && strncmp(Name, DataDirectory.c_str(), DataDirectory.size()) == 0)
		return "<data>/" + std::string(Name + DataDirectory.size());
	return Name;
}

// The original's counts the loops of the tests use (their last records).
int OriginalSprites, OriginalPictures;

void CompareLogs(const char *What)
{
	OutputOriginalLog();
	bool Same = Log[OURS] == Log[ORIGINAL];
	if (!Same) {
		size_t i = 0;
		while (i < Log[OURS].size() && i < Log[ORIGINAL].size() && Log[OURS][i] == Log[ORIGINAL][i])
			i++;
		CHECK(Same, "%s: surface calls differ at byte %u (%u/%u bytes)", What, (unsigned)i,
			(unsigned)Log[OURS].size(), (unsigned)Log[ORIGINAL].size());
	} else {
		CHECK(Same, "%s", What);
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

void CompareSprites(const char *What)
{
	uint32 Signature = ORIGINAL_VALUE("sprite signature", uint32, OSpriteSignature);
	OriginalSprites = ORIGINAL_VALUE("sprites", int, ONumberOfSprites);
	CHECK(Ours().SpriteSignature == Signature && Ours().NumberOfSprites == OriginalSprites, "%s: sprite file %x/%d, original %x/%d",
		What, Ours().SpriteSignature, Ours().NumberOfSprites, Signature, OriginalSprites);
	std::string Name = golden::OutputText("sprite file name", ORIGINAL_OR(Relative(OSpriteFileName), std::string()).c_str());
	CHECK(Relative(Ours().SpriteFileName) == Name, "%s: sprite file name", What);
	bool Present = ORIGINAL_VALUE("sprite table", bool, OSprites != NULL);
	if (Ours().NumberOfSprites != OriginalSprites || Ours().Sprites == nullptr || !Present)
		return;
	std::vector<uint8> Table((size_t)Ours().NumberOfSprites * sizeof(TSprite));
	ORIGINAL(memcpy(Table.data(), OSprites, Table.size()));
	golden::OutputBytes("sprite table", Table.data(), Table.size());
	CHECK(memcmp(Ours().Sprites.get(), Table.data(), Table.size()) == 0, "%s: sprite table differs", What);
}

void ComparePictures(const char *What)
{
	uint32 Signature = ORIGINAL_VALUE("picture signature", uint32, OPictureSignature);
	OriginalPictures = ORIGINAL_VALUE("pictures", int, ONumberOfPictures);
	CHECK(Ours().PictureSignature == Signature && Ours().NumberOfPictures == OriginalPictures,
		"%s: picture file %x/%d, original %x/%d", What, Ours().PictureSignature, Ours().NumberOfPictures, Signature,
		OriginalPictures);
	std::string Name = golden::OutputText("picture file name", ORIGINAL_OR(Relative(OPictureFileName), std::string()).c_str());
	CHECK(Relative(Ours().PictureFileName) == Name, "%s: picture file name", What);
	bool Present = ORIGINAL_VALUE("picture table", bool, OPictures != NULL);
	if (Ours().NumberOfPictures != OriginalPictures || Ours().Pictures == nullptr || !Present)
		return;
	for (int i = 0; i < Ours().NumberOfPictures; i++) {
		TPicture &A = Ours().Pictures[i];
		// The original's picture: TilesX, TilesY, ColorKey, then its offsets.
		int B[5] = {};
		ORIGINAL(B[0] = OPictures[i].TilesX; B[1] = OPictures[i].TilesY; B[2] = OPictures[i].ColorKey.R;
			B[3] = OPictures[i].ColorKey.G; B[4] = OPictures[i].ColorKey.B);
		golden::Output("picture", B);
		std::vector<int> Offsets(B[0] > 0 && B[1] > 0 ? (size_t)B[0] * B[1] : 0);
		ORIGINAL(if (!Offsets.empty()) memcpy(Offsets.data(), OPictures[i].Offsets, Offsets.size() * sizeof(int)));
		golden::OutputArray("picture offsets", Offsets.data(), Offsets.size());
		bool Same = A.TilesX == B[0] && A.TilesY == B[1] && A.ColorKey == TRGBColor(B[2], B[3], B[4])
			&& memcmp(A.Offsets, Offsets.data(), A.TilesX * A.TilesY * sizeof(int)) == 0;
		CHECK(Same, "%s: picture %d differs", What, i + 1);
	}
}

void CompareFonts(const char *What)
{
	int Count = ORIGINAL_VALUE("fonts", int, ONumberOfFonts);
	CHECK(Ours().NumberOfFonts == Count, "%s: %d fonts, original %d", What, Ours().NumberOfFonts, Count);
	bool Present = ORIGINAL_VALUE("font table", bool, OFonts != NULL);
	if (Ours().Fonts == nullptr || !Present) {
		CHECK(Ours().Fonts == nullptr && !Present, "%s: font table presence", What);
		return;
	}
	for (int i = 0; i < Ours().NumberOfFonts && i < Count; i++) {
		TFontInfo B = {};
		ORIGINAL(B = OFonts[i]);
		golden::OutputBytes("font", &B, sizeof(B));
		CHECK(memcmp(&Ours().Fonts[i], &B, sizeof(TFontInfo)) == 0, "%s: font %d differs", What, i + 1);
	}
}

// A memory cache item without its pointer: Data as its offset in the cache's data, then the ints.
const int ITEM_FIELDS = 10;
void GetItemFields(const TMemoryCacheItem &Item, const uint8 *CacheData, int32_t *Fields)
{
	Fields[0] = (int32_t)(Item.Data - CacheData);
	Fields[1] = Item.Type;
	Fields[2] = Item.Number;
	Fields[3] = Item.TileX;
	Fields[4] = Item.TileY;
	Fields[5] = Item.Previous;
	Fields[6] = Item.Next;
	Fields[7] = Item.HashPrevious;
	Fields[8] = Item.HashNext;
	Fields[9] = Item.HashValue;
}

// The original's memory cache as recorded the last time (the same on both sides).
std::vector<int32_t> LastItems;
std::vector<uint8> LastData;

void CompareMemoryCache(const char *What)
{
	int Lists[3] = {};
	ORIGINAL(Lists[0] = OMemoryCacheSize; Lists[1] = OLeastRecentMemoryItem; Lists[2] = OMostRecentMemoryItem);
	golden::Output("memory cache", Lists);
	CHECK(Ours().MemoryCacheSize == Lists[0] && Ours().LeastRecentMemoryItem == Lists[1] && Ours().MostRecentMemoryItem == Lists[2],
		"%s: memory cache %d %d-%d, original %d %d-%d", What, Ours().MemoryCacheSize, Ours().MostRecentMemoryItem,
		Ours().LeastRecentMemoryItem, Lists[0], Lists[2], Lists[1]);
	int Hash[HASH_TABLE_SIZE] = {};
	ORIGINAL(memcpy(Hash, OMemoryHashTable, sizeof(Hash)));
	golden::Output("memory hash table", Hash);
	CHECK(memcmp(Ours().MemoryHashTable.data(), Hash, sizeof(Ours().MemoryHashTable)) == 0, "%s: memory hash table", What);
	bool Present = ORIGINAL_VALUE("memory cache table", bool, OMemoryCache != NULL);
	if (Ours().MemoryCacheSize != Lists[0] || Ours().MemoryCache == nullptr || !Present)
		return;
	std::vector<int32_t> Items((size_t)Ours().MemoryCacheSize * ITEM_FIELDS);
	ORIGINAL(for (int i = 0; i < Ours().MemoryCacheSize; i++)
			GetItemFields(OMemoryCache[i], OMemoryCacheData, &Items[(size_t)i * ITEM_FIELDS]));
	if (LastItems.size() != Items.size())
		LastItems.assign(Items.size(), 0);
	golden::OutputRuns("memory cache items", LastItems.data(), Items.data(), Items.size() * sizeof(int32_t));
	for (int i = 0; i < Ours().MemoryCacheSize; i++) {
		int32_t A[ITEM_FIELDS];
		GetItemFields(Ours().MemoryCache[i], Ours().MemoryCacheData.get(), A);
		bool Same = memcmp(A, &LastItems[(size_t)i * ITEM_FIELDS], sizeof(A)) == 0;
		CHECK(Same, "%s: memory cache item %d differs", What, i);
	}
	// The data, slot by slot: an unchanged slot as such, a changed one through the tile dictionary.
	size_t Size = (size_t)Ours().MemoryCacheSize * TILE;
	if (LastData.size() != Size)
		LastData.assign(Size, 0);
	std::vector<uint8> Changed(Ours().MemoryCacheSize);
	ORIGINAL(for (int i = 0; i < Ours().MemoryCacheSize; i++) Changed[i] =
				 memcmp(&LastData[(size_t)i * TILE], OMemoryCacheData + (size_t)i * TILE, TILE) != 0);
	golden::OutputArray("memory cache slots", Changed.data(), Changed.size());
	for (int i = 0; i < Ours().MemoryCacheSize; i++) {
		if (Changed[i] == 0)
			continue;
		ORIGINAL(memcpy(&LastData[(size_t)i * TILE], OMemoryCacheData + (size_t)i * TILE, TILE));
		RecordTile("memory cache slot", &LastData[(size_t)i * TILE]);
	}
	CHECK(memcmp(Ours().MemoryCacheData.get(), LastData.data(), Size) == 0, "%s: memory cache data differs", What);
}

std::vector<uint8> LastVideo;

void CompareVideoCache(const char *What)
{
	bool Present = ORIGINAL_VALUE("video cache table", bool, OVideoCache != NULL);
	CHECK((Ours().VideoCache == nullptr) == !Present, "%s: video cache presence", What);
	int Lists[3] = {};
	ORIGINAL(Lists[0] = OVideoCacheSize; Lists[1] = OLeastRecentVideoItem; Lists[2] = OMostRecentVideoItem);
	golden::Output("video cache", Lists);
	CHECK(Ours().VideoCacheSize == Lists[0] && Ours().LeastRecentVideoItem == Lists[1] && Ours().MostRecentVideoItem == Lists[2],
		"%s: video cache %d %d-%d, original %d %d-%d", What, Ours().VideoCacheSize, Ours().MostRecentVideoItem,
		Ours().LeastRecentVideoItem, Lists[0], Lists[2], Lists[1]);
	int Hash[HASH_TABLE_SIZE] = {};
	ORIGINAL(memcpy(Hash, OVideoHashTable, sizeof(Hash)));
	golden::Output("video hash table", Hash);
	CHECK(memcmp(Ours().VideoHashTable.data(), Hash, sizeof(Ours().VideoHashTable)) == 0, "%s: video hash table", What);
	if (Ours().VideoCache == nullptr || !Present || Ours().VideoCacheSize != Lists[0])
		return;
	size_t Size = (size_t)Ours().VideoCacheSize * sizeof(TVideoCacheItem);
	if (LastVideo.size() != Size)
		LastVideo.assign(Size, 0);
	golden::OutputRuns("video cache items", LastVideo.data(), ORIGINAL_OR((const void *)OVideoCache, nullptr), Size);
	for (int i = 0; i < Ours().VideoCacheSize; i++)
		CHECK(memcmp(&Ours().VideoCache[i], &LastVideo[i * sizeof(TVideoCacheItem)], sizeof(TVideoCacheItem)) == 0,
			"%s: video cache item %d differs", What, i);
}

void CompareCaches(const char *What)
{
	CompareLogs(What);
	CompareMemoryCache(What);
	CompareVideoCache(What);
}

// Clears the fields of new cache items that are written before they are
// read, on both sides.
void ClearNewMemoryItems()
{
	for (int i = 0; i < Ours().MemoryCacheSize; i++)
		Ours().MemoryCache[i].Number = Ours().MemoryCache[i].TileX = Ours().MemoryCache[i].TileY = 0;
	ORIGINAL(for (int i = 0; i < Ours().MemoryCacheSize; i++) OMemoryCache[i].Number = OMemoryCache[i].TileX =
				 OMemoryCache[i].TileY = 0);
	memset(Ours().MemoryCacheData.get(), 0, (size_t)Ours().MemoryCacheSize * TILE);
	ORIGINAL(memset(OMemoryCacheData, 0, OMemoryCacheSize * TILE));
}

void ClearNewVideoItems()
{
	bool Present = ORIGINAL_VALUE("video cache cleared", bool, OVideoCache != NULL);
	if (Ours().VideoCache == nullptr || !Present)
		return;
	for (int i = 0; i < Ours().VideoCacheSize; i++)
		Ours().VideoCache[i].Number = Ours().VideoCache[i].MaskNumber = 0;
	ORIGINAL(for (int i = 0; i < Ours().VideoCacheSize; i++) OVideoCache[i].Number = OVideoCache[i].MaskNumber = 0);
}

// --- Inputs ----------------------------------------------------------------------

int Range(int Min, int Max)
{
	return Min + (int)(Random() % (uint32)(Max - Min + 1));
}

TRGBColor RandomColor()
{
	// The oracle's order of the three calls (TRGBColor's arguments, right to left; issue #55).
	int B = Range(0, 255);
	int G = Range(0, 255);
	int R = Range(0, 255);
	return TRGBColor(R, G, B);
}

TRect RandomRect()
{
	TRect Rect = {Range(-20, 300), Range(-20, 300), Range(0, 200), Range(0, 200)};
	return Rect;
}

TFont RandomFont()
{
	TFont Font;
	Font.FontNumber = Range(1, 4);
	Font.Color = RandomColor();
	Font.Alignment = Range(0, 2);
	return Font;
}

// Any byte but 0, mostly letters, with blanks, newlines and control characters.
void RandomString(char *Text, int Length)
{
	for (int i = 0; i < Length; i++) {
		switch (Random() % 4) {
		case 0:
			Text[i] = (char)Range(1, 255);
			break;
		case 1:
			Text[i] = " \n\t"[Random() % 3];
			break;
		default:
			Text[i] = (char)Range('A', 'z');
			break;
		}
	}
	Text[Length] = 0;
}

// Object types and outfits whose sprites can be drawn without failing an
// assertion (decided with our Objects.cpp; an exception skips the type).
std::vector<int> DrawableObjects;
std::vector<int> DrawableOutfits;

bool SpriteOK(int SpriteNumber) { return SpriteNumber >= 0 && SpriteNumber <= Ours().NumberOfSprites; }

void FindDrawables()
{
	for (int Type = patella::bridge::ObjectTypes().ObjectProperties->min; Type < patella::bridge::ObjectTypes().ObjectProperties->min + patella::bridge::ObjectTypes().ObjectProperties->d; Type++) {
		try {
			if (GetObjectExactSize(Type) <= 0 || GetObjectPhases(Type) <= 0)
				continue;
			bool OK = true;
			for (int Phase = 1; Phase <= GetObjectPhases(Type) && OK; Phase++)
				for (int Layer = 1; Layer <= GetObjectLayers(Type) && OK; Layer++)
					for (int x = 1; x <= GetObjectWidth(Type) && OK; x++)
						for (int y = 1; y <= GetObjectHeight(Type) && OK; y++)
							for (int px = 1; px <= GetObjectPatternWidth(Type) && OK; px++)
								for (int py = 1; py <= GetObjectPatternHeight(Type) && OK; py++)
									OK = SpriteOK(GetObjectSpriteNumber(Type, x, y, Layer, px, py, 1, Phase));
			if (OK)
				DrawableObjects.push_back(Type);
		} catch (...) {
		}
	}
	for (int Outfit = patella::bridge::ObjectTypes().OutfitProperties->min; Outfit < patella::bridge::ObjectTypes().OutfitProperties->min + patella::bridge::ObjectTypes().OutfitProperties->d; Outfit++) {
		try {
			if (GetOutfitExactSize(Outfit) <= 0)
				continue;
			bool OK = true;
			for (int x = 1; x <= GetOutfitWidth(Outfit) && OK; x++) {
				for (int y = 1; y <= GetOutfitHeight(Outfit) && OK; y++) {
					int Sprite = GetOutfitSpriteNumber(Outfit, x, y, 1, 2, 1);
					int Mask = GetOutfitLayers(Outfit) == 2 ? GetOutfitSpriteNumber(Outfit, x, y, 2, 2, 1) : 0;
					OK = SpriteOK(Sprite) && SpriteOK(Mask) && (Mask == 0 || Sprite >= 1);
				}
			}
			if (OK)
				DrawableOutfits.push_back(Outfit);
		} catch (...) {
		}
	}
}

void SetEngine(int Engine)
{
	patella::bridge::GraphicsEngine().curEngine = Engine;
	ORIGINAL(Global<int>(0x71d0c4) = Engine);
}

// --- The tests -------------------------------------------------------------------

void TestFiles(const std::string &Spr, const std::string &Pic)
{
	LoadSprites(Spr.c_str());
	ORIGINAL(At<LoadFn>(0x48b550)(Spr.c_str()));
	CompareSprites("LoadSprites");
	static uint8 A[TILE], B[TILE];
	for (int s = 1; s <= Ours().NumberOfSprites && s <= OriginalSprites; s++) {
		memset(A, 0x55, sizeof(A));
		memset(B, 0x55, sizeof(B));
		LoadSprite(s, A);
		DecodedSprites++;
		ORIGINAL(At<LoadSpriteFn>(0x48b9a0)(s, B));
		RecordTile("sprite", B);
		int Key[3] = {};
		ORIGINAL(Key[0] = OSprites[s - 1].ColorKey.R; Key[1] = OSprites[s - 1].ColorKey.G;
			Key[2] = OSprites[s - 1].ColorKey.B);
		golden::Output("sprite colour key", Key);
		CHECK(memcmp(A, B, sizeof(A)) == 0 && Ours().Sprites[s - 1].ColorKey == TRGBColor(Key[0], Key[1], Key[2]),
			"LoadSprite(%d) differs", s);
	}
	LoadPictures(Pic.c_str());
	ORIGINAL(At<LoadFn>(0x48bea0)(Pic.c_str()));
	ComparePictures("LoadPictures");
	for (int p = 1; p <= Ours().NumberOfPictures && p <= OriginalPictures; p++) {
		for (int ty = 0; ty < Ours().Pictures[p - 1].TilesY; ty++) {
			for (int tx = 0; tx < Ours().Pictures[p - 1].TilesX; tx++) {
				memset(A, 0x55, sizeof(A));
				memset(B, 0x55, sizeof(B));
				LoadPictureTile(p, tx, ty, A);
				DecodedTiles++;
				ORIGINAL(At<LoadTileFn>(0x48c3b0)(p, tx, ty, B));
				RecordTile("picture tile", B);
				CHECK(memcmp(A, B, sizeof(A)) == 0, "LoadPictureTile(%d, %d, %d) differs", p, tx, ty);
			}
		}
	}
}

void TestMemoryCache()
{
	InitMemoryCache(7 * TILE + 1000);
	ORIGINAL(At<IntFn>(0x48c9c0)(7 * TILE + 1000));
	ClearNewMemoryItems();
	CompareMemoryCache("InitMemoryCache");
	InitFonts();
	ORIGINAL(At<VoidFn>(0x492c30)());
	CompareFonts("InitFonts");
	CompareMemoryCache("InitFonts");
	for (int t = 0; t < 4000; t++) {
		int Op = Random() % 10;
		if (Op < 4) {
			int s = Range(1, Ours().NumberOfSprites);
			if (t % 3 == 0)
				s = Range(1, 30);
			TMemoryCacheItem *A = GetMemorySprite(s);
			int B = ORIGINAL_VALUE("GetMemorySprite", int, At<MemSpriteFn>(0x48d440)(s) - OMemoryCache);
			CHECK(A - Ours().MemoryCache.get() == B, "GetMemorySprite(%d): %d, original %d", s, (int)(A - Ours().MemoryCache.get()), B);
		} else if (Op < 8) {
			int p = Range(1, Ours().NumberOfPictures);
			int tx = Range(0, Ours().Pictures[p - 1].TilesX - 1);
			int ty = Range(0, Ours().Pictures[p - 1].TilesY - 1);
			TMemoryCacheItem *A = GetMemoryPicture(p, tx, ty);
			int B = ORIGINAL_VALUE("GetMemoryPicture", int, At<MemPictureFn>(0x48d740)(p, tx, ty) - OMemoryCache);
			CHECK(A - Ours().MemoryCache.get() == B, "GetMemoryPicture(%d, %d, %d)", p, tx, ty);
		} else if (Op == 8) {
			int s = Range(1, 30);
			CHECK(FindMemorySprite(s) == ORIGINAL_VALUE("FindMemorySprite", int, At<IntRetFn>(0x48ce00)(s)),
				"FindMemorySprite(%d)", s);
			int p = Range(1, Ours().NumberOfPictures);
			int tx = Range(0, 3);
			int ty = Range(0, 3);
			CHECK(FindMemoryPicture(p, tx, ty) == ORIGINAL_VALUE("FindMemoryPicture", int, At<Find3Fn>(0x48ce70)(p, tx, ty)),
				"FindMemoryPicture(%d, %d, %d)", p, tx, ty);
		} else if (t % 50 == 9) {
			ClearMemoryCache();
			ORIGINAL(At<VoidFn>(0x48b460)());
		}
		CompareMemoryCache("memory cache operation");
	}
}

void TestText()
{
	char Text[80];
	for (int t = 0; t < 20000; t++) {
		RandomString(Text, Range(0, 60));
		int Font = Range(1, 4);
		int Length = Range(0, (int)strlen(Text));
		CHECK(GetTextWidth(Font, Text) == ORIGINAL_VALUE("GetTextWidth", int, At<WidthFn>(0x494570)(Font, Text)),
			"GetTextWidth(%d, '%s')", Font, Text);
		CHECK(GetTextPartWidth(Font, Text, Length)
				== ORIGINAL_VALUE("GetTextPartWidth", int, At<PartWidthFn>(0x491b80)(Font, Text, Length)),
			"GetTextPartWidth(%d, '%s', %d)", Font, Text, Length);
		CHECK(GetTextHeight(Font, Text) == ORIGINAL_VALUE("GetTextHeight", int, At<WidthFn>(0x491880)(Font, Text)),
			"GetTextHeight(%d, '%s')", Font, Text);
		int Width = Range(-20, 400);
		CHECK(GetFittingTextLength(Font, Text, Width)
				== ORIGINAL_VALUE("GetFittingTextLength", int, At<PartWidthFn>(0x491f50)(Font, Text, Width)),
			"GetFittingTextLength(%d, '%s', %d)", Font, Text, Width);
		TFont Format = RandomFont();
		int x = Range(-50, 500);
		int y = Range(-50, 500);
		int w = Range(-10, 300);
		int h = Range(-10, 100);
		int ax = 0, ay = 0;
		int Position[2] = {1, 1};
		GetTextPosition(&ax, &ay, x, y, w, h, &Format);
		ORIGINAL(At<PositionFn>(0x492260)(&Position[0], &Position[1], x, y, w, h, &Format));
		golden::Output("GetTextPosition", Position);
		CHECK(ax == Position[0] && ay == Position[1], "GetTextPosition: %d,%d, original %d,%d", ax, ay, Position[0],
			Position[1]);
	}
	for (int Font = -3; Font <= 6; Font++) {
		if (Font != 0) // reads before the table, on each side's own heap
			CHECK(GetFontLineHeight(Font) == ORIGINAL_VALUE("GetFontLineHeight", int, At<IntRetFn>(0x48b430)(Font)),
				"GetFontLineHeight(%d)", Font);
		if (Font >= 1 && Font <= 4)
			CHECK(GetFontHeight(Font) == ORIGINAL_VALUE("GetFontHeight", int, At<IntRetFn>(0x491710)(Font)),
				"GetFontHeight(%d)", Font);
	}
}

// One random drawing operation on both sides. Every input is drawn before
// the two calls.
void RandomDraw(int t)
{
	int Surface = Range(0, 5);
	int x = Range(-40, 400);
	int y = Range(-40, 400);
	TRect Clip = RandomRect();
	TRGBColor Highlight = RandomColor();
	TRGBColor Colors[4] = {RandomColor(), RandomColor(), RandomColor(), RandomColor()};
	if (t % 4 == 0) // repeated colours, so that cached masked sprites are found again
		Colors[0] = Colors[1] = Colors[2] = Colors[3] = TRGBColor(t % 3, 0, 0);
	int Sprite = t % 2 ? Range(0, 40) : Range(0, Ours().NumberOfSprites);
	char Text[80];
	RandomString(Text, Range(0, 30));
	int Length = Range(0, (int)strlen(Text));
	int Width = Range(1, 64);
	int Height = Range(1, 64);
	int SmallMask = t % 3 == 0 ? 0 : Range(1, 40);
	int LargeMask = t % 3 == 0 ? 0 : Range(1, Ours().NumberOfSprites);
	int Picture = 1 + t % Ours().NumberOfPictures;
	int SourceX = Range(-40, 300);
	int SourceY = Range(-40, 300);
	int PartWidth = Range(-2, 200);
	int PartHeight = Range(-2, 200);
	TFont F = RandomFont();
	if (t % 2)
		F.Color = TRGBColor(0, 0, 0); // the colour of plain picture tiles
	TObject Obj = {0, 0, 0};
	if (t % 7 != 0) {
		Obj.Type = DrawableObjects[Random() % DrawableObjects.size()];
		Obj.Data = Range(0, 120);
	}
	int Size = t % 2 ? 32 : Range(8, 64);
	bool ShowCount = (Random() & 1) != 0;
	int Outfit = t % 5 == 0 ? 0 : DrawableOutfits[Random() % DrawableOutfits.size()];
	int OutfitColors[4] = {Range(0, 132), Range(0, 132), Range(0, 132), Range(0, 132)};
	if (Outfit == 0)
		OutfitColors[0] = t % 3 ? DrawableObjects[Random() % DrawableObjects.size()] : 0;
	int Op = Random() % 17;
	for (int Pass = 0; Pass < 1; Pass++) {
		bool O = Pass == ORIGINAL;
		Side = Pass;
		switch (Op) {
		case 0:
			if (O) {
				ORIGINAL(At<DrawSpriteFn>(0x48f870)(Surface, x, y, Sprite));
			} else
				DrawSprite(Surface, x, y, Sprite);
			break;
		case 1:
			if (O) {
				ORIGINAL(At<DrawClippedFn>(0x48fa60)(Surface, x, y, Sprite - 2, Clip));
			} else
				DrawSpriteClipped(Surface, x, y, Sprite - 2, Clip);
			break;
		case 2:
			if (O) {
				ORIGINAL(At<DrawScaledFn>(0x48fd80)(Surface, x, y, Width, Height, Sprite, Highlight, Clip));
			} else
				DrawSpriteScaled(Surface, x, y, Width, Height, Sprite, Highlight, Clip);
			break;
		case 3: {
			int s = SmallMask == 0 ? Sprite : (Sprite == 0 ? 1 : Sprite);
			if (O) {
				ORIGINAL(At<DrawMaskedFn>(0x4900d0)(Surface, x, y, s, SmallMask, Colors));
			} else
				DrawMaskedSprite(Surface, x, y, s, SmallMask, Colors);
			break;
		}
		case 4: {
			int s = LargeMask == 0 ? Sprite : (Sprite == 0 ? 1 : Sprite);
			if (O) {
				ORIGINAL(At<DrawMaskedScaledFn>(0x4904b0)(Surface, x, y, Width, Height, s, LargeMask, Colors, Highlight,
					Clip));
			} else
				DrawMaskedSpriteScaled(Surface, x, y, Width, Height, s, LargeMask, Colors, Highlight, Clip);
			break;
		}
		case 5:
			if (O) {
				ORIGINAL(At<DrawSpriteFn>(0x490920)(Surface, x, y, Picture));
			} else
				DrawPicture(Surface, x, y, Picture);
			break;
		case 6:
			if (O) {
				ORIGINAL(At<PartFn>(0x490c60)(Surface, x, y, Picture, SourceX, SourceY, PartWidth, PartHeight));
			} else
				DrawPicturePart(Surface, x, y, Picture, SourceX, SourceY, PartWidth, PartHeight);
			break;
		case 7:
			if (O) {
				ORIGINAL(At<PartClippedFn>(0x4910e0)(Surface, x, y, Picture, SourceX, SourceY, Width * 3, Height * 3,
					Clip));
			} else
				DrawPicturePartClipped(Surface, x, y, Picture, SourceX, SourceY, Width * 3, Height * 3, Clip);
			break;
		case 8:
			if (O) {
				ORIGINAL(At<TextPartFn>(0x492e30)(Surface, x, y, F.FontNumber, F.Color, Text, Length, F.Alignment));
			} else
				PrintTextPart(Surface, x, y, F.FontNumber, F.Color, Text, Length, F.Alignment);
			break;
		case 9:
			if (O) {
				ORIGINAL(At<TextClippedFn>(0x4934e0)(Surface, x, y, F.FontNumber, F.Color, Text, F.Alignment, Clip));
			} else
				PrintTextClipped(Surface, x, y, F.FontNumber, F.Color, Text, F.Alignment, Clip);
			break;
		case 10:
			if (O) {
				ORIGINAL(At<TextFn>(0x493af0)(Surface, x, y, F.FontNumber, F.Color, Text, F.Alignment));
			} else
				PrintText(Surface, x, y, F.FontNumber, F.Color, Text, F.Alignment);
			break;
		case 11:
			if (O) {
				ORIGINAL(At<FormatFn>(0x493f60)(Surface, x, y, F, Text));
			} else
				PrintFormattedText(Surface, x, y, F, Text);
			break;
		case 12:
			if (O) {
				ORIGINAL(At<FormatClippedFn>(0x4941d0)(Surface, x, y, F, Text, Clip));
			} else
				PrintFormattedTextClipped(Surface, x, y, F, Text, Clip);
			break;
		case 13:
			if (O) {
				ORIGINAL(At<FormatPartFn>(0x4943b0)(Surface, x, y, F, Text, Length));
			} else
				PrintFormattedTextPart(Surface, x, y, F, Text, Length);
			break;
		case 14:
		case 15:
			if (O) {
				ORIGINAL(At<DrawObjectFn>(0x494ca0)(Surface, x, y, Size, Obj, Highlight, Clip, F, ShowCount));
			} else
				DrawObject(Surface, x, y, Size, Obj, Highlight, Clip, F, ShowCount);
			break;
		case 16:
			if (O) {
				ORIGINAL(At<DrawOutfitFn>(0x495080)(Surface, x, y, Size, Outfit, OutfitColors, Highlight, Clip, F));
			} else
				DrawOutfit(Surface, x, y, Size, Outfit, OutfitColors, Highlight, Clip, F);
			break;
		}
	}
	char What[64];
	snprintf(What, sizeof(What), "drawing operation %d (%d)", Op, t);
	CompareCaches(What);
}

// The recording surface. Our GetSurface returns it from the first video cache
// test on, for good (it used to create external.h's TSurfaceGL sketch, which
// was the recording surface; difftest.exe links the real TSurfaceGL now).
// mapwindowtest.cpp takes the same object from GetSurface.
TSurface *RecordingSurface;
TSurface *__cdecl GetRecordingSurface() { return RecordingSurface; }

void TestVideoCache()
{
	SetEngine(1);
	if (RecordingSurface == nullptr) {
		RecordingSurface = new TTestSurface;
		golden::RedirectOwn((void *)GetSurface, (void *)GetRecordingSurface);
	}
	[[maybe_unused]] TSurface *Fake = GetSurface();
	ORIGINAL(Global<TSurface *>(0x71d0cc) = Fake);
	for (int Round = 0; Round < 6; Round++) {
		SetEngine(Round % 3);
		GraphicsCacheSurfaces = 1 + Round % 2;
		CacheWidth = 96 + 32 * (Round % 3) + Round;
		CacheHeight = 64 + 7 * Round;
		Side = OURS;
		if (Round == 0)
			InitVideoCache();
		else
			ResetVideoCache();
		Side = ORIGINAL;
		ORIGINAL(At<VoidFn>(Round == 0 ? 0x48da50 : 0x4922c0)());
		ClearNewVideoItems();
		CompareCaches("InitVideoCache");
		for (int t = 0; t < 2500; t++) {
			if (t % 100 == 0) {
				IncrementActionNumber();
				ORIGINAL(Global<int>(0x71c5ec) = GetActionNumber());
			}
			RandomDraw(Round * 10000 + t);
		}
	}
	// No cache surfaces: the cache becomes NULL, its size stays.
	GraphicsCacheSurfaces = 0;
	Side = OURS;
	ResetVideoCache();
	Side = ORIGINAL;
	ORIGINAL(At<VoidFn>(0x4922c0)());
	CompareCaches("ResetVideoCache without cache surfaces");
	GraphicsCacheSurfaces = 2;
	Side = OURS;
	InitVideoCache();
	Side = ORIGINAL;
	ORIGINAL(At<VoidFn>(0x48da50)());
	ClearNewVideoItems();
	CompareCaches("InitVideoCache again");
}

} // namespace GraphicsTest

void TestGraphics([[maybe_unused]] const char *ExePath)
{
	using namespace GraphicsTest;
	ORIGINAL(PatchScript());
	int OldChecks = Checks, OldFailures = Failures;
	// The data files: next to Tibia.exe in the oracle, from the corpus in the runner (golden::DataFile).
	std::string Spr = golden::DataFile("Tibia.spr", ExePath);
	std::string Pic = golden::DataFile("Tibia.pic", ExePath);
	std::string Dat = golden::DataFile("Tibia.dat", ExePath);
	for (const std::string *Name : {&Spr, &Pic, &Dat}) {
		if (Name->empty()) {
			printf("note: Tibia.spr, Tibia.pic or Tibia.dat not found next to %s, graphics.cpp not tested\n",
				ExePath != nullptr ? ExePath : "Tibia.exe");
			return;
		}
	}
	size_t Slash = Spr.find_last_of("\\/");
	std::string Dir = Slash == std::string::npos ? std::string() : Spr.substr(0, Slash + 1);
	DataDirectory = Dir;
	TestFiles(Spr, Pic);
	TestMemoryCache();
	TestText();

	LoadObjects(Dat.c_str());
	ORIGINAL(At<LoadFn>(0x4d7380)(Dat.c_str()));
	FindDrawables();
	CHECK(!DrawableObjects.empty() && !DrawableOutfits.empty(), "no drawable objects (%d) or outfits (%d)",
		(int)DrawableObjects.size(), (int)DrawableOutfits.size());
	int SavedEngine = patella::bridge::GraphicsEngine().curEngine;
	if (!DrawableObjects.empty() && !DrawableOutfits.empty())
		TestVideoCache();

	// InitGraphics, ReloadSprites and ReloadPictures read the files in the
	// current directory.
	char Saved[MAX_PATH];
	if (_getcwd(Saved, MAX_PATH) == nullptr)
		Saved[0] = 0;
	if (_chdir(Dir.c_str()) != 0)
		printf("note: cannot change to %s\n", Dir.c_str());
	Side = OURS;
	ReloadSprites();
	Side = ORIGINAL;
	ORIGINAL(At<VoidFn>(0x492490)());
	ClearNewVideoItems();
	CompareSprites("ReloadSprites");
	CompareCaches("ReloadSprites");
	Side = OURS;
	ReloadPictures();
	Side = ORIGINAL;
	ORIGINAL(At<VoidFn>(0x494900)());
	ClearNewVideoItems();
	ComparePictures("ReloadPictures");
	CompareFonts("ReloadPictures");
	CompareCaches("ReloadPictures");
	ExitGraphics();
	ORIGINAL(At<VoidFn>(0x48b4a0)());
	CHECK(Ours().VideoCache == nullptr && Ours().MemoryCacheData == nullptr && Ours().MemoryCache == nullptr && Ours().Fonts == nullptr
			&& Ours().Pictures == nullptr && Ours().Sprites == nullptr,
		"ExitGraphics leaves a table");
	bool Left = ORIGINAL_VALUE("tables left", bool,
		!(OVideoCache == NULL && OMemoryCacheData == NULL && OMemoryCache == NULL && OFonts == NULL
			&& OPictures == NULL && OSprites == NULL));
	CHECK(!Left, "the original ExitGraphics leaves a table");
	Side = OURS;
	InitGraphics(5 * TILE);
	Side = ORIGINAL;
	ORIGINAL(At<IntFn>(0x494af0)(5 * TILE));
	CompareSprites("InitGraphics");
	ComparePictures("InitGraphics");
	CompareFonts("InitGraphics");
	CompareLogs("InitGraphics");
	ExitGraphics();
	ORIGINAL(At<VoidFn>(0x48b4a0)());
	if (Saved[0] != 0 && _chdir(Saved) != 0)
		printf("note: cannot change back to %s\n", Saved);

	ReleaseObjects();
	ORIGINAL(At<VoidFn>(0x4d70c0)());
	SetEngine(SavedEngine);
	printf("Graphics: %d checks, %d failures (%d sprites and %d picture tiles decoded; %d tiles written to the "
		   "video cache, %d blits)\n",
		Checks - OldChecks, Failures - OldFailures, DecodedSprites, DecodedTiles, Writes, Blits);
}
