// difftest.exe's Objects test (moved here from tools/difftest/objectstest.cpp, issue #27): Objects.cpp
// against the original, as a golden-corpus test (golden.h).
//
// In the oracle the image's malloc and free (PatchImage) and the stdio functions that its
// TReadBinaryFile uses (fopen, fread, fclose) are redirected to ours, so the original loads real files.
// Both sides load the real Tibia.dat (next to Tibia.exe, if it is there; the runner gets it from the
// corpus, golden::DataFile) and random well-formed .dat files; then every accessor is called on both
// with valid arguments. Failing assertions are not exercised: the original would need its exception
// machinery.
//
// The original's tables (array<TProperties>, whose entries hold a pointer to their sprites) are recorded
// by value: per table its presence and bounds, per entry the fields without the pointer and the sprite
// numbers it points to. The old test's memcmp of two entries with their pointers cleared is the same
// comparison field by field: TProperties has no padding in either layout.

#include "golden_test.h"

#include "Objects.h"

#include <direct.h>
#include <string.h>
#include <string>

#if GOLDEN_ORACLE
static_assert(sizeof(TProperties) == 0x4c, "TProperties is 0x4c bytes");
static_assert(sizeof(array<TProperties>) == 12, "array<T> is 12 bytes");
#endif

namespace ObjectsTest {

#if GOLDEN_ORACLE
// --- Replacements patched into the image -----------------------------------

FILE *__cdecl ImageFopen(const char *Name, const char *Mode) { return fopen(Name, Mode); }
int __cdecl ImageFclose(FILE *File) { return fclose(File); }
size_t __cdecl ImageFread(void *Buffer, size_t Size, size_t Count, FILE *File) { return fread(Buffer, Size, Count, File); }

void PatchObjects()
{
	static bool Done = false;
	if (Done)
		return;
	Done = true;
	PatchImage();
	PatchJump(0x51227f, (void *)ImageFopen);
	PatchJump(0x512122, (void *)ImageFclose);
	PatchJump(0x513a29, (void *)ImageFread);
}

// --- Original functions --------------------------------------------------------

typedef void(__cdecl *VoidFn)();
typedef void(__cdecl *LoadFn)(const char *FileName);
typedef int(__cdecl *IntFn)(int);
typedef bool(__cdecl *FlagFn)(int, int);
typedef int(__cdecl *Sprite8Fn)(int, int, int, int, int, int, int, int);
typedef int(__cdecl *Sprite6Fn)(int, int, int, int, int, int);
typedef int(__cdecl *Sprite5Fn)(int, int, int, int, int);
typedef void(__cdecl *PatternFn)(TObject *, int *, int *);

array<TProperties> *&OriginalTable(int Kind)
{
	static const uint32 Address[4] = {0x719d98, 0x719d94, 0x719d90, 0x719d8c};
	return Global<array<TProperties> *>(Address[Kind]);
}
#endif

// Our tables are std::unique_ptr since issue #58: the pointer they hold.
array<TProperties> *OurTable(int Kind)
{
	const std::unique_ptr<array<TProperties>> *Tables[4] = {&ObjectProperties, &OutfitProperties, &EffectProperties,
		&MissileProperties};
	return Tables[Kind]->get();
}

const char *KindName[4] = {"object", "outfit", "effect", "missile"};

int SpriteCount(const TProperties &E)
{
	return E.Width * E.Height * E.Layers * E.PatternWidth * E.PatternHeight * E.PatternDepth * E.Phases;
}

// An entry's fields without the sprite pointer, in the original's order.
const int FIELDS = 18;
void GetFields(const TProperties &E, int32_t *Fields)
{
	const int Values[FIELDS] = {E.Width, E.Height, E.ExactSize, E.Layers, E.PatternWidth, E.PatternHeight,
		E.PatternDepth, E.Phases, 0, E.Waypoints, E.MaxLength, E.Brightness, E.LightColor, E.ShiftX, E.ShiftY,
		E.Elevation, E.AutomapColor, E.LensHelp};
	memcpy(Fields, Values, sizeof(Values));
	memcpy(&Fields[8], E.Flags, 4);
}

// Compares the four tables of both sides entry by entry.
void CompareTables(const char *What)
{
	uint32 Signature = ORIGINAL_VALUE("signature", uint32, Global<uint32>(0x719d9c));
	CHECK(DatSignature == Signature, "%s: signature 0x%x, original 0x%x", What, DatSignature, Signature);
	for (int k = 0; k < 4; k++) {
		array<TProperties> *Ours = OurTable(k);
		ORIGINAL(array<TProperties> *Original = OriginalTable(k));
		bool OriginalPresent = ORIGINAL_VALUE("table", bool, Original != NULL);
		CHECK((Ours == nullptr) == !OriginalPresent, "%s: %s table presence differs", What, KindName[k]);
		if (Ours == nullptr || !OriginalPresent)
			continue;
		int Bounds[2] = {};
		ORIGINAL(Bounds[0] = Original->min; Bounds[1] = Original->d);
		golden::Output("table bounds", Bounds);
		CHECK(Ours->min == Bounds[0] && Ours->d == Bounds[1], "%s: %s table %d+%d, original %d+%d", What, KindName[k],
			Ours->min, Ours->d, Bounds[0], Bounds[1]);
		if (Ours->min != Bounds[0] || Ours->d != Bounds[1])
			continue;
		for (int i = 0; i < Ours->d; i++) {
			const TProperties &A = Ours->entry[i];
			int32_t FieldsA[FIELDS], FieldsB[FIELDS] = {};
			GetFields(A, FieldsA);
			ORIGINAL(GetFields(Original->entry[i], FieldsB));
			golden::Output("entry", FieldsB);
			int CountB = FieldsB[0] * FieldsB[1] * FieldsB[3] * FieldsB[4] * FieldsB[5] * FieldsB[6] * FieldsB[7];
			std::string SpritesB(CountB > 0 ? (size_t)CountB * sizeof(uint16) : 0, '\0');
			ORIGINAL(if (CountB > 0) memcpy(&SpritesB[0], Original->entry[i].Sprites, SpritesB.size()));
			if (CountB > 0)
				golden::OutputArray("sprites", (uint16 *)&SpritesB[0], (size_t)CountB);
			bool Same = SpriteCount(A) == CountB
				&& (CountB <= 0 || memcmp(A.Sprites, SpritesB.data(), CountB * sizeof(uint16)) == 0);
			Same = Same && memcmp(FieldsA, FieldsB, sizeof(FieldsA)) == 0;
			CHECK(Same, "%s: %s %d differs", What, KindName[k], Ours->min + i);
		}
	}
}

// --- Accessors -------------------------------------------------------------

bool HasFlag(const TProperties &E, int Flag)
{
	return (FlagByte(E, Flag) & (1 << (Flag % 8))) != 0;
}

struct TIntAccessor {
	uint32 Address;
	int (*Ours)(int);
	int Flag; // -1: no flag needed
	const char *Name;
};

const TIntAccessor ObjectAccessors[] = {
	{0x4d1f30, GetObjectWaypoints, BANK, "GetObjectWaypoints"},
	{0x4d20d0, GetObjectBrightness, LIGHT, "GetObjectBrightness"},
	{0x4d2270, GetObjectLightColor, LIGHT, "GetObjectLightColor"},
	{0x4d2410, GetObjectShiftX, SHIFT, "GetObjectShiftX"},
	{0x4d25b0, GetObjectShiftY, SHIFT, "GetObjectShiftY"},
	{0x4d2750, GetObjectElevation, HEIGHT, "GetObjectElevation"},
	{0x4d28f0, GetObjectAutomapColor, AUTOMAP, "GetObjectAutomapColor"},
	{0x4d2a90, GetObjectLensHelp, LENSHELP, "GetObjectLensHelp"},
	{0x4d2c30, GetObjectWidth, -1, "GetObjectWidth"},
	{0x4d2dd0, GetObjectHeight, -1, "GetObjectHeight"},
	{0x4d2f70, GetObjectExactSize, -1, "GetObjectExactSize"},
	{0x4d3110, GetObjectLayers, -1, "GetObjectLayers"},
	{0x4d32b0, GetObjectPatternWidth, -1, "GetObjectPatternWidth"},
	{0x4d3450, GetObjectPatternHeight, -1, "GetObjectPatternHeight"},
	{0x4d35f0, GetObjectPatternDepth, -1, "GetObjectPatternDepth"},
	{0x4d3790, GetObjectPhases, -1, "GetObjectPhases"},
};
const TIntAccessor OutfitAccessors[] = {
	{0x4d3cc0, GetOutfitBrightness, LIGHT, "GetOutfitBrightness"},
	{0x4d3e60, GetOutfitLightColor, LIGHT, "GetOutfitLightColor"},
	{0x4d4000, GetOutfitShiftX, SHIFT, "GetOutfitShiftX"},
	{0x4d41a0, GetOutfitShiftY, SHIFT, "GetOutfitShiftY"},
	{0x4d4340, GetOutfitWidth, -1, "GetOutfitWidth"},
	{0x4d44e0, GetOutfitExactSize, -1, "GetOutfitExactSize"},
	{0x4d4680, GetOutfitHeight, -1, "GetOutfitHeight"},
	{0x4d4820, GetOutfitLayers, -1, "GetOutfitLayers"},
	{0x4d49c0, GetOutfitPhases, -1, "GetOutfitPhases"},
};
const TIntAccessor EffectAccessors[] = {
	{0x4d4ef0, GetEffectBrightness, LIGHT, "GetEffectBrightness"},
	{0x4d5090, GetEffectLightColor, LIGHT, "GetEffectLightColor"},
	{0x4d5230, GetEffectWidth, -1, "GetEffectWidth"},
	{0x4d53d0, GetEffectHeight, -1, "GetEffectHeight"},
	{0x4d5570, GetEffectLayers, -1, "GetEffectLayers"},
	{0x4d5710, GetEffectPhases, -1, "GetEffectPhases"},
};
const TIntAccessor MissileAccessors[] = {
	{0x4d5c40, GetMissileBrightness, LIGHT, "GetMissileBrightness"},
	{0x4d5de0, GetMissileLightColor, LIGHT, "GetMissileLightColor"},
	{0x4d5f80, GetMissileWidth, -1, "GetMissileWidth"},
	{0x4d6120, GetMissileHeight, -1, "GetMissileHeight"},
	{0x4d62c0, GetMissileLayers, -1, "GetMissileLayers"},
};

struct TAccessorSet {
	const TIntAccessor *List;
	int Count;
	uint32 FlagAddress;
	bool (*OurFlag)(int, int);
};

const TAccessorSet Sets[4] = {
	{ObjectAccessors, sizeof(ObjectAccessors) / sizeof(ObjectAccessors[0]), 0x4d1c40, GetObjectFlag},
	{OutfitAccessors, sizeof(OutfitAccessors) / sizeof(OutfitAccessors[0]), 0x4d3af0, GetOutfitFlag},
	{EffectAccessors, sizeof(EffectAccessors) / sizeof(EffectAccessors[0]), 0x4d4d20, GetEffectFlag},
	{MissileAccessors, sizeof(MissileAccessors) / sizeof(MissileAccessors[0]), 0x4d5a70, GetMissileFlag},
};

int Pick(int Max) // 1..Max
{
	return 1 + (int)(Random() % (uint32)Max);
}

void TestEntry(int Kind, int Number, const TProperties &E)
{
	const TAccessorSet &S = Sets[Kind];
	for (int f = 0; f < 32; f++) {
		bool A = S.OurFlag(Number, f);
		bool B = ORIGINAL_VALUE("flag", bool, At<FlagFn>(S.FlagAddress)(Number, f));
		CHECK(A == B, "%s flag %d of %d: %d, original %d", KindName[Kind], f, Number, A, B);
	}
	for (int a = 0; a < S.Count; a++) {
		const TIntAccessor &F = S.List[a];
		if (F.Flag >= 0 && !HasFlag(E, F.Flag))
			continue;
		int A = F.Ours(Number);
		int B = ORIGINAL_VALUE("accessor", int, At<IntFn>(F.Address)(Number));
		CHECK(A == B, "%s(%d): %d, original %d", F.Name, Number, A, B);
	}
	if (SpriteCount(E) <= 0)
		return;
	for (int t = 0; t < 4; t++) {
		int X = Pick(E.Width), Y = Pick(E.Height), Layer = Pick(E.Layers), Phase = Pick(E.Phases);
		int A = 0, B = 0;
		switch (Kind) {
		case 0: {
			int PX = Pick(E.PatternWidth), PY = Pick(E.PatternHeight), PZ = Pick(E.PatternDepth);
			A = GetObjectSpriteNumber(Number, X, Y, Layer, PX, PY, PZ, Phase);
			B = ORIGINAL_VALUE("sprite number", int, At<Sprite8Fn>(0x4d3930)(Number, X, Y, Layer, PX, PY, PZ, Phase));
			break;
		}
		case 1: {
			int Direction = Pick(E.PatternWidth) - 1;
			A = GetOutfitSpriteNumber(Number, X, Y, Layer, Direction, Phase);
			B = ORIGINAL_VALUE("sprite number", int, At<Sprite6Fn>(0x4d4b60)(Number, X, Y, Layer, Direction, Phase));
			break;
		}
		case 2:
			A = GetEffectSpriteNumber(Number, X, Y, Layer, Phase);
			B = ORIGINAL_VALUE("sprite number", int, At<Sprite5Fn>(0x4d58b0)(Number, X, Y, Layer, Phase));
			break;
		case 3: {
			if (E.PatternWidth < 3 || E.PatternHeight < 3)
				return;
			int Direction = (int)(Random() % 8);
			A = GetMissileSpriteNumber(Number, X, Y, Layer, Direction);
			B = ORIGINAL_VALUE("sprite number", int, At<Sprite5Fn>(0x4d6460)(Number, X, Y, Layer, Direction));
			break;
		}
		}
		CHECK(A == B, "%s %d sprite number: %d, original %d", KindName[Kind], Number, A, B);
	}
}

void TestPatterns(int Type, const TProperties &E)
{
	struct {
		uint32 Address;
		void (*Ours)(TObject &, int *, int *);
		bool Usable;
		const char *Name;
	} Functions[] = {
		{0x4d6800, GetCumulativePattern, HasFlag(E, CUMULATIVE), "GetCumulativePattern"},
		{0x4d6b20, GetLiquidPattern, HasFlag(E, LIQUIDPOOL) || HasFlag(E, LIQUIDCONTAINER), "GetLiquidPattern"},
		{0x4d6df0, GetHangPattern, HasFlag(E, HANG), "GetHangPattern"},
	};
	static const int Data[] = {-1000, -101, -9, -5, -4, -3, -1, 0, 1, 2, 3, 4, 5, 9, 10, 24, 25, 49, 50, 100, 255, 1 << 20};
	for (auto &F : Functions) {
		if (!F.Usable)
			continue;
		for (int t = 0; t < 24; t++) {
			TObject Obj;
			Obj.Type = Type;
			Obj.Data = t < 22 ? Data[t] : (int)Random32();
			Obj.Data2 = t % 4 == 0 ? HOOKSOUTH : t % 4 == 1 ? HOOKEAST
															: (int)(Random() % 40) - 5;
			int X1 = -7, Y1 = -7, Pattern[2] = {-7, -7};
			F.Ours(Obj, &X1, &Y1);
			ORIGINAL(At<PatternFn>(F.Address)(&Obj, &Pattern[0], &Pattern[1]));
			golden::Output("pattern", Pattern);
			CHECK(X1 == Pattern[0] && Y1 == Pattern[1], "%s(%d, %d, %d): %d,%d original %d,%d", F.Name, Type, Obj.Data,
				Obj.Data2, X1, Y1, Pattern[0], Pattern[1]);
		}
	}
}

void TestAccessors()
{
	for (int k = 0; k < 4; k++) {
		array<TProperties> *T = OurTable(k);
		for (int i = 0; i < T->d; i++) {
			TestEntry(k, T->min + i, T->entry[i]);
			if (k == 0)
				TestPatterns(T->min + i, T->entry[i]);
		}
	}
	for (int f = 0; f < 32; f++) {
		bool A = GetObjectFlag(CREATURE, f);
		bool B = ORIGINAL_VALUE("creature flag", bool, At<FlagFn>(0x4d1c40)(CREATURE, f));
		CHECK(A == B, "GetObjectFlag(CREATURE, %d): %d, original %d", f, A, B);
	}
}

// --- Random .dat files -------------------------------------------------------

void Put16(std::string &s, int v)
{
	s += (char)(v & 0xff);
	s += (char)((v >> 8) & 0xff);
}

void RandomEntry(std::string &s, bool Wild)
{
	static const int Known[] = {BANK, WRITE, WRITEONCE, LIGHT, SHIFT, HEIGHT, AUTOMAP, LENSHELP, CUMULATIVE,
		LIQUIDPOOL, LIQUIDCONTAINER, HANG, HOOKSOUTH, HOOKEAST, UNPASS};
	int Flags = (int)(Random() % 12);
	for (int f = 0; f < Flags; f++) {
		int Flag;
		switch (Random() % 4) {
		case 0:
			Flag = (int)(Random() % 32);
			break;
		case 1:
			Flag = Wild ? (int)(Random() % 255) : (int)(Random() % 30);
			break;
		default:
			Flag = Known[Random() % (sizeof(Known) / sizeof(Known[0]))];
			break;
		}
		s += (char)Flag;
		switch (Flag) {
		case BANK:
		case WRITE:
		case WRITEONCE:
		case HEIGHT:
		case AUTOMAP:
		case LENSHELP:
			Put16(s, (int)Random());
			break;
		case LIGHT:
		case SHIFT:
			Put16(s, (int)Random());
			Put16(s, (int)Random());
			break;
		}
	}
	s += static_cast<char>(-1); // 0xff
	int Width = Pick(2), Height = Pick(2);
	s += (char)Width;
	s += (char)Height;
	if (Width > 1 || Height > 1)
		s += (char)(Random() % 256);
	int Layers = Pick(2), PW = Pick(4), PH = Pick(4), PD = Pick(2), Phases = Pick(3);
	s += (char)Layers;
	s += (char)PW;
	s += (char)PH;
	s += (char)PD;
	s += (char)Phases;
	int Count = Width * Height * Layers * PW * PH * PD * Phases;
	for (int i = 0; i < Count; i++)
		Put16(s, (int)Random());
}

std::string RandomDat(bool Wild)
{
	std::string s;
	uint32 Signature = Random32();
	for (int i = 0; i < 4; i++)
		s += (char)(Signature >> (8 * i));
	int Objects = 100 + (int)(Random() % 40), Outfits = Pick(8), Effects = Pick(8), Missiles = Pick(8);
	Put16(s, Objects);
	Put16(s, Outfits);
	Put16(s, Effects);
	Put16(s, Missiles);
	for (int i = 100; i <= Objects; i++)
		RandomEntry(s, Wild);
	for (int i = 0; i < Outfits + Effects + Missiles; i++)
		RandomEntry(s, Wild);
	return s;
}

void WriteFile(const std::string &Name, const std::string &Data)
{
	FILE *File = fopen(Name.c_str(), "wb");
	fwrite(Data.data(), 1, Data.size(), File);
	fclose(File);
}

void ExitBoth(const char *What)
{
	ExitObjects();
	ORIGINAL(At<VoidFn>(0x4d79b0)());
	for (int k = 0; k < 4; k++) {
		bool OriginalLeft = ORIGINAL_VALUE("table left", bool, OriginalTable(k) != NULL);
		CHECK(OurTable(k) == nullptr && !OriginalLeft, "%s leaves the %s table", What, KindName[k]);
	}
}

} // namespace ObjectsTest

void TestObjects([[maybe_unused]] const char *ExePath)
{
	using namespace ObjectsTest;
	ORIGINAL(PatchObjects());
	int Empty[3] = {};
	ORIGINAL(memcpy(Empty, (void *)(uintptr_t)0x719da0, sizeof(Empty)));
	golden::Output("NoObject", Empty);
	const int Ours[3] = {NoObject.Type, NoObject.Data, NoObject.Data2};
	CHECK(memcmp(Ours, Empty, sizeof(Ours)) == 0, "NoObject differs");

	// The real Tibia.dat, next to Tibia.exe (the runner's from the corpus).
	std::string Dat = golden::DataFile("Tibia.dat", ExePath);
	if (!Dat.empty()) {
		LoadObjects(Dat.c_str());
		ORIGINAL(At<LoadFn>(0x4d7380)(Dat.c_str()));
		CompareTables("Tibia.dat");
		TestAccessors();
		ExitBoth("ExitObjects");
	} else {
		printf("note: Tibia.dat not found next to %s, testing generated files only\n", ExePath != nullptr ? ExePath : "Tibia.exe");
	}

	char Help[MAX_PATH];
	GetTempPathA(MAX_PATH, Help);
	std::string TempDir = std::string(Help) + "tibia-objectstest" GOLDEN_SLASH;
	CreateDirectoryA(TempDir.c_str(), nullptr);
	std::string Name = TempDir + "Tibia.dat";

	// Random files through LoadObjects, which first releases the old tables.
	for (int t = 0; t < 60; t++) {
		WriteFile(Name, RandomDat(t % 3 == 2));
		LoadObjects(Name.c_str());
		ORIGINAL(At<LoadFn>(0x4d7380)(Name.c_str()));
		CompareTables("random .dat");
		if (t % 3 != 2)
			TestAccessors(); // flags above 31 change the fields; compared above only
	}
	ExitBoth("ExitObjects");

	// InitObjects and ReloadObjects read "Tibia.dat" in the current directory.
	char Saved[MAX_PATH];
	_getcwd(Saved, MAX_PATH);
	_chdir(TempDir.c_str());
	WriteFile(Name, RandomDat(false));
	InitObjects();
	ORIGINAL(At<VoidFn>(0x4d77f0)());
	CompareTables("InitObjects");
	WriteFile(Name, RandomDat(false));
	ReloadObjects();
	ORIGINAL(At<VoidFn>(0x4d7b40)());
	CompareTables("ReloadObjects");
	TestAccessors();
	ExitBoth("ExitObjects after ReloadObjects");
	ReleaseObjects();
	ORIGINAL(At<VoidFn>(0x4d70c0)());
	_chdir(Saved);

	remove(Name.c_str());
	RemoveDirectoryA(TempDir.c_str());
}
