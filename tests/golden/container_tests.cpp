// difftest.exe's Container test (moved here from tools/difftest/containertest.cpp, issue #26):
// Container.h's templates (issue #13) against the image's instances, as a golden-corpus test (golden.h).
// Every instance of vector<T>, array<T>, matrix<T> and matrix3d<T> that the image has is called next to
// ours on the same inputs, and both objects and all their entries are compared after every call. The
// image's and our operator new[] fill new memory with the same random byte pattern, so entries that
// neither side initialises agree too. Also the other header functions that have a path the image can
// run (TLineFormat's constructor, TBuffer::SetSize, TChannelTalkBuffer::GetChannelID).
//
// Not run: the error paths (the assertions of the constructors throw, the index errors of array, matrix
// and matrix3d build a std::stringstream; the image's C++ runtime cannot do either in the test child),
// and GUI.h's GetFirstChild/GetLastChild and TRuleViolationChannel::AddText, which only throw.
//
// The comparisons are byte for byte in the original's layout. Each element type is described field by
// field (TLayout): an int, a pointer (4 bytes in the original, 8 in the x64 runner), a char array. Our
// entries are projected to the original's layout to be compared (each field, and the padding after it,
// which our padding after the same field holds; the upper half of a pointer has no counterpart), and an
// entry the test writes (random bytes in the original's layout) is put into ours the other way, a
// pointer zero-extended. In the oracle both layouts are the original's and both mappings copy every
// byte: the comparisons and the random numbers are the old test's. The runner fills our new memory
// with the pattern through its replacement operator new[] (the oracle patches its CRT's), and the
// original's objects are recorded by their bytes (the entries by their changes).
//
// DIFFTEST_ONLY=Container runs this test alone in the oracle.

#include "golden_test.h"

#include "ConsoleWindow.h"
#include "Container.h"
#include "DialogElements.h"
#include "Dialogs.h"
#include "GUI.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Status.h"
#include "Talk.h"
#include "Utils.h"

#include <algorithm>
#include <new>
#include <stddef.h>
#include <string.h>
#include <string>
#include <type_traits>
#include <vector>

namespace ContainerTest {

#if GOLDEN_ORACLE
typedef void(__fastcall *Ctor2Fn)(void *This, void *, int a, int b);
typedef void(__fastcall *Ctor3Fn)(void *This, void *, int Min, int Max, int Block);
typedef void(__fastcall *Ctor4Fn)(void *This, void *, int a, int b, int c, int d);
typedef void(__fastcall *Ctor6Fn)(void *This, void *, int a, int b, int c, int d, int e, int f);
typedef void *(__fastcall *At1Fn)(void *This, void *, int i);
typedef void *(__fastcall *At2Fn)(void *This, void *, int x, int y);
typedef void *(__fastcall *At3Fn)(void *This, void *, int x, int y, int z);
typedef void(__fastcall *DtorFn)(void *This, void *);
#endif

// Its own random numbers: the shared Random() stays where it was, so the
// tests after this one get the same inputs as without it.
static uint32 Seed = 13;
static uint32 Rnd()
{
	Seed = Seed * 1103515245 + 12345;
	return Seed >> 8;
}

// Status.h's TErrorReport until Patella-Modern issue #74 (an error report read from error.txt; the reports are gone
// from the client). The image's vector<TErrorReport> is still one of Container.h's vector instances to compare ours
// with, so the test keeps the record: the original's layout (0xa5a bytes of char arrays, byte aligned) and its
// constructor, which clears them.
struct TErrorReport {
	char Title[64];    // +0x000
	char Date[26];     // +0x040
	char Text[2048];   // +0x05a
	char Comment[512]; // +0x85a
	TErrorReport()
		: Title{}, Date{}, Text{}, Comment{}
	{
	}
};
static_assert(sizeof(TErrorReport) == 0xa5a && alignof(TErrorReport) == 1 && offsetof(TErrorReport, Comment) == 0x85a,
	"TErrorReport's layout");

// New memory of both sides gets the same byte pattern.
static uint8 Pattern = 0xab;
static void *__cdecl PatternNew(size_t Size)
{
	void *Memory = malloc(Size == 0 ? 1 : Size);
	memset(Memory, Pattern, Size);
	return Memory;
}
#if !GOLDEN_ORACLE
// The runner: our operator new[] (below) fills while this is set.
bool PatternActive = false;
#endif

#if GOLDEN_ORACLE
struct TSaved {
	uint8 *Code;
	uint8 Bytes[5];
};
static std::vector<TSaved> SavedCode;
static void Redirect(void *Function, void *Target)
{
	TSaved S;
	S.Code = (uint8 *)Function;
	DWORD Protection;
	VirtualProtect(Function, 5, PAGE_EXECUTE_READWRITE, &Protection);
	memcpy(S.Bytes, Function, 5);
	PatchJump((uint32)(uintptr_t)Function, Target);
	VirtualProtect(Function, 5, Protection, &Protection);
	FlushInstructionCache(GetCurrentProcess(), Function, 5);
	SavedCode.push_back(S);
}
static void RestoreAll()
{
	for (size_t i = SavedCode.size(); i-- > 0;) {
		DWORD Protection;
		VirtualProtect(SavedCode[i].Code, 5, PAGE_EXECUTE_READWRITE, &Protection);
		memcpy(SavedCode[i].Code, SavedCode[i].Bytes, 5);
		VirtualProtect(SavedCode[i].Code, 5, Protection, &Protection);
		FlushInstructionCache(GetCurrentProcess(), SavedCode[i].Code, 5);
	}
	SavedCode.clear();
}
#endif

static void RandomBytes(void *Data, size_t Size)
{
	for (size_t i = 0; i < Size; i++)
		((uint8 *)Data)[i] = (uint8)Rnd();
}

// --- The layouts ---------------------------------------------------------------------

// An element type field by field: "i" an int, "p" a pointer (or INT_PTR), "cN" N bytes and "cN@A" N
// bytes aligned to A (chars, or a type without pointers as a whole), separated by spaces. Laid out with
// 4-byte pointers (the original) and with this build's (ours).
struct TLayout {
	struct TField {
		int Kind; // 'i', 'p', 'c'
		int Count, Align;
		size_t Offset[2], Size[2], Gap[2]; // [0] the original's, [1] ours: the padding after the field
	};
	std::string Description;
	std::vector<TField> Fields;
	size_t Size[2] = {0, 0};

	TLayout(const char *Text, size_t OurSize, const char *Name) : Description(Text)
	{
		for (const char *p = Text; *p != 0;) {
			if (*p == ' ') {
				p++;
				continue;
			}
			TField Field = {};
			Field.Kind = *p++;
			Field.Count = 1;
			Field.Align = 1;
			if (Field.Kind == 'c') {
				Field.Count = (int)strtol(p, (char **)&p, 10);
				if (*p == '@')
					Field.Align = (int)strtol(p + 1, (char **)&p, 10);
			}
			Fields.push_back(Field);
		}
		for (int Side = 0; Side < 2; Side++) {
			size_t Pointer = Side == 0 ? 4 : sizeof(void *);
			size_t Offset = 0, Align = 1;
			for (TField &Field : Fields) {
				size_t FieldSize = Field.Kind == 'i' ? 4 : Field.Kind == 'p' ? Pointer
																			 : (size_t)Field.Count;
				size_t FieldAlign = Field.Kind == 'c' ? (size_t)Field.Align : FieldSize;
				Offset = (Offset + FieldAlign - 1) / FieldAlign * FieldAlign;
				Field.Offset[Side] = Offset;
				Field.Size[Side] = FieldSize;
				Offset += FieldSize;
				Align = (std::max)(Align, FieldAlign);
			}
			Size[Side] = (Offset + Align - 1) / Align * Align;
			for (size_t i = 0; i < Fields.size(); i++) {
				size_t Next = i + 1 < Fields.size() ? Fields[i + 1].Offset[Side] : Size[Side];
				Fields[i].Gap[Side] = Next - Fields[i].Offset[Side] - Fields[i].Size[Side];
			}
		}
		bool Valid = Size[1] == OurSize;
#if GOLDEN_ORACLE
		Valid = Valid && Size[0] == OurSize; // the oracle's classes have the original's layout
#endif
		for (const TField &Field : Fields)
			Valid = Valid && Field.Gap[0] <= Field.Gap[1];
		if (!Valid) {
			printf("golden: Container: the layout of %s does not fit (%d bytes, ours %d)\n", Name, (int)Size[0],
				(int)OurSize);
			exit(1);
		}
	}

	// Our entry (Size[1] bytes) in the original's layout (Size[0] bytes).
	void Project(const uint8 *Ours, uint8 *Original) const
	{
		for (const TField &Field : Fields) {
			memcpy(Original + Field.Offset[0], Ours + Field.Offset[1], Field.Size[0]); // a pointer's lower half
			memcpy(Original + Field.Offset[0] + Field.Size[0], Ours + Field.Offset[1] + Field.Size[1], Field.Gap[0]);
		}
	}
	// An entry in the original's layout put into ours: a pointer zero-extended, the padding our layout has
	// in addition left as it is.
	void Inject(const uint8 *Original, uint8 *Ours) const
	{
		for (const TField &Field : Fields) {
			memset(Ours + Field.Offset[1], 0, Field.Size[1]);
			memcpy(Ours + Field.Offset[1], Original + Field.Offset[0], Field.Size[0]);
			memcpy(Ours + Field.Offset[1] + Field.Size[1], Original + Field.Offset[0] + Field.Size[0], Field.Gap[0]);
		}
	}
	// Count entries of ours in the original's layout.
	std::string ProjectAll(const void *Ours, size_t Count) const
	{
		std::string Result(Count * Size[0], '\0');
		for (size_t i = 0; i < Count; i++)
			Project((const uint8 *)Ours + i * Size[1], (uint8 *)&Result[i * Size[0]]);
		return Result;
	}
};

template <class T> const TLayout &LayoutOf();
#define LAYOUT(Type, Description) \
	template <> const TLayout &LayoutOf<Type>() \
	{ \
		static const TLayout Layout(Description, sizeof(Type), #Type); \
		return Layout; \
	}
// The types without pointers are the same on both sides: their bytes.
#define PLAIN_LAYOUT(Type) \
	template <> const TLayout &LayoutOf<Type>() \
	{ \
		static const TLayout Layout(("c" + std::to_string(sizeof(Type)) + "@" + std::to_string(alignof(Type))).c_str(), \
			sizeof(Type), #Type); \
		return Layout; \
	}
LAYOUT(TLineFormat, "p i i i i i i i p c1") // LineText ... Color (three ints), FontNumber, TextLine, LineEnd
LAYOUT(TSkinButton *, "p")
PLAIN_LAYOUT(TMark)
PLAIN_LAYOUT(int)
LAYOUT(TListBoxLine, "p i i i i i") // Text, Font (FontNumber, Color, Alignment)
PLAIN_LAYOUT(TIgnoreEntry)
LAYOUT(THotkey, "i p i") // Key, Callback (Receiver, Event)
LAYOUT(TTimer *, "p")
PLAIN_LAYOUT(TContentWindowSize)
PLAIN_LAYOUT(TBuddy)
LAYOUT(TMessage, "i p p") // Type, Param1, Param2 (INT_PTR)
PLAIN_LAYOUT(TErrorReport)
LAYOUT(TTalkFilter *, "p")
PLAIN_LAYOUT(TMapPoint)
LAYOUT(TProperties, "i i i i i i i i p c4 i i i i i i i i i") // ... Phases, Sprites, Flags, Waypoints ...
LAYOUT(TPathNode, "i i c1 i i i p p")                         // x, y, Blocked, Cost, Speed, Estimate, Predecessor, Next
LAYOUT(TBuffer, "p i i")                                      // Data, Size, Position

// The original's bytes at a comparison: the oracle's Image, recorded by their changes from the last
// time (Last: one buffer per call site).
const uint8 *OriginalBytes(const char *Name, std::string &Last, [[maybe_unused]] const void *Image, size_t Size)
{
	uint32 Length = golden::Recorded<uint32>("length", (uint32)Size);
	if (Last.size() < (std::max)((size_t)Length, Size))
		Last.resize((std::max)((size_t)Length, Size), '\0');
	golden::OutputChanges(Name, &Last[0], Image, Length);
	return (const uint8 *)Last.data();
}

// --- vector<T> ---------------------------------------------------------------------

// The image's copies of one vector<T> instance. Init is the 4-byte initial
// value of the initialized constructors (Ctor4).
struct TVectorInstance {
	const char *Name;
	uint32 Ctor3; // vector(min, max, block), 0 if the image has none
	uint32 Ctor4; // vector(min, max, block, init), 0 if the image has none
	uint32 At;    // operator()
	uint32 Dtor;  // out-of-line ~vector, 0 if inline only
};

// Everything but the entry pointer (min ... initialized, init: vector<T>'s layout made of T's), then the
// whole allocated array.
template <class T> static bool SameVector(vector<T> *Ours, [[maybe_unused]] vector<T> *Image, const char *Name, const char *What)
{
	static const TLayout Header(("i i i i i c1 " + LayoutOf<T>().Description + " p").c_str(), sizeof(vector<T>),
		"vector<T>");
	static std::string LastHead, LastEntries;
	size_t HeadSize = Header.Size[0] - 4; // the original's offset of entry, its last member
	const uint8 *Head = OriginalBytes("vector header", LastHead, ORIGINAL_OR(Image, nullptr), HeadSize);
	int Fields[6];
	memcpy(Fields, Head, 5 * sizeof(int));
	Fields[5] = Head[20];
	bool Same = memcmp(Header.ProjectAll(Ours, 1).data(), Head, HeadSize) == 0;
	CHECK(Same, "%s %s: min %d/%d max %d/%d start %d/%d space %d/%d block %d/%d initialized %d/%d", Name, What,
		Ours->min, Fields[0], Ours->max, Fields[1], Ours->start, Fields[2], Ours->space, Fields[3], Ours->block,
		Fields[4], Ours->initialized, Fields[5]);
	if (!Same)
		return false;
	const TLayout &Layout = LayoutOf<T>();
	size_t Size = (size_t)Ours->space * Layout.Size[0];
	const uint8 *Entries = OriginalBytes("vector entries", LastEntries, ORIGINAL_OR(Image->entry.get(), nullptr), Size);
	Same = memcmp(Layout.ProjectAll(Ours->entry.get(), (size_t)Ours->space).data(), Entries, Size) == 0;
	CHECK(Same, "%s %s: entries differ (space %d)", Name, What, Ours->space);
	return Same;
}

template <class T> static void TestVector(const TVectorInstance &Instance, int Rounds)
{
	const TLayout &Layout = LayoutOf<T>();
	CHECK(offsetof(vector<T>, entry) + sizeof(T *) == sizeof(vector<T>), "%s: entry is not the last member", Instance.Name);
	for (int Round = 0; Round < Rounds; Round++) {
		Pattern = (uint8)Rnd();
		int Min = (int)(Rnd() % 11) - 5;
		int Max = Min + (int)(Rnd() % 6);
		int Block = (int)(Rnd() % 5);
		bool WithInit = Instance.Ctor4 != 0 && (Instance.Ctor3 == 0 || Rnd() % 2 == 0);
		uint32 Init = Rnd() % 4 == 0 ? 0 : Rnd();
		vector<T> *Ours = (vector<T> *)PatternNew(sizeof(vector<T>));
		vector<T> *Image = ORIGINAL_OR((vector<T> *)PatternNew(sizeof(vector<T>)), nullptr);
		// The entries get another pattern than the object, so that an entry
		// set from init (whose bytes the constructor leaves) differs from one
		// that is not.
		Pattern = (uint8)(Pattern + 1 + Rnd() % 255);
		if constexpr (std::is_same<T, int>::value || std::is_pointer<T>::value) {
			// The initialized vectors have 4-byte elements in the original (int and pointers).
			if (WithInit) {
				T Value{};
				Layout.Inject((const uint8 *)&Init, (uint8 *)&Value);
				new (Ours) vector<T>(Min, Max, Block, Value);
				ORIGINAL(At<Ctor4Fn>(Instance.Ctor4)(Image, NULL, Min, Max, Block, (int)Init));
			}
		}
		if (!WithInit) {
			new (Ours) vector<T>(Min, Max, Block);
			ORIGINAL(At<Ctor3Fn>(Instance.Ctor3)(Image, NULL, Min, Max, Block));
		}
		bool Same = SameVector(Ours, Image, Instance.Name, WithInit ? "vector(min, max, block, init)" : "vector(min, max, block)");
		// Indices inside, above and below the used range; the arrays stay below 64 KB (in the original).
		int Operations = 1 + (int)(Rnd() % 40);
		for (int Op = 0; Same && Op < Operations; Op++) {
			int Kind = (int)(Rnd() % 10);
			int Spread = 2 * Ours->space + 3;
			int i;
			if (Kind < 4)
				i = Ours->min + (int)(Rnd() % (uint32)(Ours->max - Ours->min + 1));
			else if (Kind < 7)
				i = Ours->max + 1 + (int)(Rnd() % (uint32)Spread);
			else
				i = Ours->min - 1 - (int)(Rnd() % (uint32)Spread);
			int Low = (std::min)(i, Ours->min), High = (std::max)(i, Ours->max);
			if ((High - Low + 1) * 4 * (int)Layout.Size[0] > 65536)
				continue;
			T *a = &(*Ours)(i);
			ORIGINAL(T *b = (T *)At<At1Fn>(Instance.At)(Image, NULL, i));
			int Entry = ORIGINAL_VALUE("vector entry", int, b - Image->entry.get());
			CHECK(a - Ours->entry.get() == Entry, "%s operator()(%d): entry %d, original %d", Instance.Name, i,
				(int)(a - Ours->entry.get()), Entry);
			if (Rnd() % 3 != 0) {
				std::vector<uint8> Value(Layout.Size[0]);
				RandomBytes(Value.data(), Value.size());
				Layout.Inject(Value.data(), (uint8 *)a);
				ORIGINAL(memcpy(b, Value.data(), sizeof(T)));
			}
			Same = SameVector(Ours, Image, Instance.Name, "operator()");
		}
		// The destructor: ours inline, the image's out-of-line copy where it has one.
		Ours->~vector<T>();
#if GOLDEN_ORACLE
		if (Instance.Dtor != 0)
			At<DtorFn>(Instance.Dtor)(Image, NULL);
		else
			free(Image->entry.get());
		free(Image);
#endif
		free(Ours);
	}
}

// --- array<T>, matrix<T>, matrix3d<T> ----------------------------------------------

template <class T> static void TestArray(const char *Name, [[maybe_unused]] uint32 Ctor, [[maybe_unused]] uint32 AtFunction,
	int MaxEntries, int Rounds)
{
	const TLayout &Layout = LayoutOf<T>();
	static std::string LastEntries;
	for (int Round = 0; Round < Rounds; Round++) {
		Pattern = (uint8)Rnd();
		int Min = (int)(Rnd() % 201) - 100;
		int Max = Min + (int)(Rnd() % (uint32)MaxEntries);
		array<T> *Ours = (array<T> *)PatternNew(sizeof(array<T>));
		[[maybe_unused]] array<T> *Image = ORIGINAL_OR((array<T> *)PatternNew(sizeof(array<T>)), nullptr);
		new (Ours) array<T>(Min, Max);
		ORIGINAL(At<Ctor2Fn>(Ctor)(Image, NULL, Min, Max));
		int Head[2] = {};
		ORIGINAL(Head[0] = Image->min; Head[1] = Image->d);
		golden::Output("array header", Head);
		bool Same = Ours->min == Head[0] && Ours->d == Head[1];
		CHECK(Same, "%s array(%d, %d): min %d/%d d %d/%d", Name, Min, Max, Ours->min, Head[0], Ours->d, Head[1]);
		if (Same) {
			size_t Size = (size_t)Ours->d * Layout.Size[0];
			const uint8 *Entries = OriginalBytes("array entries", LastEntries, ORIGINAL_OR(Image->entry.get(), nullptr), Size);
			Same = memcmp(Layout.ProjectAll(Ours->entry.get(), (size_t)Ours->d).data(), Entries, Size) == 0;
			CHECK(Same, "%s array(%d, %d): entries differ", Name, Min, Max);
		}
		CHECK(Ours->low() == Min && Ours->high() == Max, "%s low/high %d %d", Name, Ours->low(), Ours->high());
		for (int Op = 0; Same && Op < 30; Op++) {
			int i = Min + (int)(Rnd() % (uint32)(Max - Min + 1));
			T *a = &(*Ours)(i);
			int Entry = ORIGINAL_VALUE("array entry", int, (T *)At<At1Fn>(AtFunction)(Image, NULL, i) - Image->entry.get());
			CHECK(a - Ours->entry.get() == Entry, "%s operator()(%d)", Name, i);
		}
		Ours->~array<T>();
		ORIGINAL(free(Image->entry.get()); free(Image));
		free(Ours);
	}
}

template <class T> static void TestMatrix(const char *Name, [[maybe_unused]] uint32 Ctor, [[maybe_unused]] uint32 AtFunction,
	[[maybe_unused]] uint32 Dtor, int MaxSide, int Rounds)
{
	const TLayout &Layout = LayoutOf<T>();
	static std::string LastHead, LastEntries;
	for (int Round = 0; Round < Rounds; Round++) {
		Pattern = (uint8)Rnd();
		int XMin = (int)(Rnd() % 201) - 100, YMin = (int)(Rnd() % 201) - 100;
		int XMax = XMin + (int)(Rnd() % (uint32)MaxSide), YMax = YMin + (int)(Rnd() % (uint32)MaxSide);
		matrix<T> *Ours = (matrix<T> *)PatternNew(sizeof(matrix<T>));
		[[maybe_unused]] matrix<T> *Image = ORIGINAL_OR((matrix<T> *)PatternNew(sizeof(matrix<T>)), nullptr);
		new (Ours) matrix<T>(XMin, XMax, YMin, YMax);
		ORIGINAL(At<Ctor4Fn>(Ctor)(Image, NULL, XMin, XMax, YMin, YMax));
		size_t Head = offsetof(matrix<T>, entry); // 16 bytes on both sides
		const uint8 *OriginalHead = OriginalBytes("matrix header", LastHead, ORIGINAL_OR(Image, nullptr), Head);
		bool Same = memcmp(Ours, OriginalHead, Head) == 0;
		CHECK(Same, "%s matrix(%d, %d, %d, %d): header differs", Name, XMin, XMax, YMin, YMax);
		if (Same) {
			size_t Count = (size_t)Ours->dx * (size_t)Ours->dy;
			const uint8 *Entries = OriginalBytes("matrix entries", LastEntries, ORIGINAL_OR(Image->entry.get(), nullptr),
				Count * Layout.Size[0]);
			Same = memcmp(Layout.ProjectAll(Ours->entry.get(), Count).data(), Entries, Count * Layout.Size[0]) == 0;
			CHECK(Same, "%s matrix(%d, %d, %d, %d): entries differ", Name, XMin, XMax, YMin, YMax);
		}
		for (int Op = 0; Same && Op < 30; Op++) {
			int x = XMin + (int)(Rnd() % (uint32)(XMax - XMin + 1));
			int y = YMin + (int)(Rnd() % (uint32)(YMax - YMin + 1));
			T *a = &(*Ours)(x, y);
			int Entry = ORIGINAL_VALUE("matrix entry", int, (T *)At<At2Fn>(AtFunction)(Image, NULL, x, y) - Image->entry.get());
			CHECK(a - Ours->entry.get() == Entry, "%s operator()(%d, %d)", Name, x, y);
		}
		Ours->~matrix<T>();
		ORIGINAL(At<DtorFn>(Dtor)(Image, NULL); free(Image));
		free(Ours);
	}
}

template <class T> static void TestMatrix3d(const char *Name, [[maybe_unused]] uint32 Ctor, [[maybe_unused]] uint32 AtFunction,
	int MaxSide, int Rounds)
{
	const TLayout &Layout = LayoutOf<T>();
	static std::string LastHead, LastEntries;
	for (int Round = 0; Round < Rounds; Round++) {
		Pattern = (uint8)Rnd();
		int Low[3], High[3];
		for (int k = 0; k < 3; k++) {
			Low[k] = (int)(Rnd() % 41) - 20;
			High[k] = Low[k] + (int)(Rnd() % (uint32)MaxSide);
		}
		matrix3d<T> *Ours = (matrix3d<T> *)PatternNew(sizeof(matrix3d<T>));
		[[maybe_unused]] matrix3d<T> *Image = ORIGINAL_OR((matrix3d<T> *)PatternNew(sizeof(matrix3d<T>)), nullptr);
		new (Ours) matrix3d<T>(Low[0], High[0], Low[1], High[1], Low[2], High[2]);
		ORIGINAL(At<Ctor6Fn>(Ctor)(Image, NULL, Low[0], High[0], Low[1], High[1], Low[2], High[2]));
		size_t Head = offsetof(matrix3d<T>, entry); // 24 bytes on both sides
		const uint8 *OriginalHead = OriginalBytes("matrix3d header", LastHead, ORIGINAL_OR(Image, nullptr), Head);
		bool Same = memcmp(Ours, OriginalHead, Head) == 0;
		CHECK(Same, "%s matrix3d: header differs", Name);
		if (Same) {
			size_t Count = (size_t)Ours->dx * (size_t)Ours->dy * (size_t)Ours->dz;
			const uint8 *Entries = OriginalBytes("matrix3d entries", LastEntries, ORIGINAL_OR(Image->entry.get(), nullptr),
				Count * Layout.Size[0]);
			Same = memcmp(Layout.ProjectAll(Ours->entry.get(), Count).data(), Entries, Count * Layout.Size[0]) == 0;
			CHECK(Same, "%s matrix3d: entries differ", Name);
		}
		for (int Op = 0; Same && Op < 30; Op++) {
			int x = Low[0] + (int)(Rnd() % (uint32)(High[0] - Low[0] + 1));
			int y = Low[1] + (int)(Rnd() % (uint32)(High[1] - Low[1] + 1));
			int z = Low[2] + (int)(Rnd() % (uint32)(High[2] - Low[2] + 1));
			T *a = &(*Ours)(x, y, z);
			int Entry = ORIGINAL_VALUE("matrix3d entry", int,
				(T *)At<At3Fn>(AtFunction)(Image, NULL, x, y, z) - Image->entry.get());
			CHECK(a - Ours->entry.get() == Entry, "%s operator()(%d, %d, %d)", Name, x, y, z);
		}
		Ours->~matrix3d<T>();
		ORIGINAL(free(Image->entry.get()); free(Image));
		free(Ours);
	}
}

// --- The other header functions ------------------------------------------------------

#if GOLDEN_ORACLE
typedef void(__fastcall *LineFormatFn)(void *This, void *, TTalkLine *TextLine, const char *LineText, int LineLength,
	int OriginalOffset, int Indent, int FontNumber, TRGBColor Color, bool LineEnd);
typedef void(__fastcall *SetSizeFn)(void *This, void *, int NewSize);
typedef int(__fastcall *GetChannelIDFn)(void *This, void *);
#endif

static void TestHeaderInlines()
{
	// ConsoleWindow.h's TLineFormat constructor 0x418020 (valid arguments only).
	const TLayout &LineFormat = LayoutOf<TLineFormat>();
	for (int Round = 0; Round < 200; Round++) {
		Pattern = (uint8)Rnd();
		TTalkLine *Line = (TTalkLine *)(uintptr_t)(1 + Rnd());
		const char *Text = (const char *)(uintptr_t)(1 + Rnd());
		int Length = 1 + (int)(Rnd() % 100), Offset = (int)(Rnd() % 100), Indent = (int)(Rnd() % 50);
		int Font = (int)(Rnd() % 8);
		TRGBColor Color((int)(Rnd() % 256), (int)(Rnd() % 256), (int)(Rnd() % 256));
		bool End = Rnd() % 2 == 0;
		TLineFormat *Ours = (TLineFormat *)PatternNew(sizeof(TLineFormat));
		new (Ours) TLineFormat(Line, Text, Length, Offset, Indent, Font, Color, End);
		uint8 Image[0x28] = {};
#if GOLDEN_ORACLE
		TLineFormat *ImageObject = (TLineFormat *)PatternNew(sizeof(TLineFormat));
		At<LineFormatFn>(0x418020)(ImageObject, NULL, Line, Text, Length, Offset, Indent, Font, Color, End);
		memcpy(Image, ImageObject, sizeof(Image));
		free(ImageObject);
#endif
		golden::Output("TLineFormat", Image);
		CHECK(memcmp(LineFormat.ProjectAll(Ours, 1).data(), Image, LineFormat.Size[0]) == 0, "TLineFormat(%d, %d, %d, %d)",
			Length, Offset, Indent, Font);
		free(Ours);
	}
	// Utils.h's TBuffer::SetSize 0x4ca300 (sizes >= 0 only).
	const TLayout &Buffer = LayoutOf<TBuffer>();
	for (int Round = 0; Round < 200; Round++) {
		uint8 Bytes[12];
		RandomBytes(Bytes, Buffer.Size[0]);
		TBuffer Ours;
		Buffer.Inject(Bytes, (uint8 *)&Ours);
		int Size = Rnd() % 4 == 0 ? 0 : (int)(Rnd() & 0x7fffffff);
		Ours.SetSize(Size);
		uint8 Image[12];
		memcpy(Image, Bytes, sizeof(Image));
		ORIGINAL(At<SetSizeFn>(0x4ca300)(Image, NULL, Size));
		golden::Output("TBuffer", Image);
		CHECK(memcmp(Buffer.ProjectAll(&Ours, 1).data(), Image, Buffer.Size[0]) == 0, "TBuffer::SetSize(%d)", Size);
	}
	// Talk.h's TChannelTalkBuffer::GetChannelID 0x507060 (folded with
	// DialogElements.h's TListBox::GetNumberOfLines) on our objects.
	for (int Round = 0; Round < 50; Round++) {
		int Channel = (int)Rnd();
		TChannelTalkBuffer TalkBuffer(0, Channel);
		int Original = ORIGINAL_VALUE("GetChannelID", int, At<GetChannelIDFn>(0x507060)(&TalkBuffer, NULL));
		CHECK(Original == TalkBuffer.GetChannelID(), "GetChannelID %d", Channel);
	}
}

static void Run()
{
#if GOLDEN_ORACLE
	PatchImage();
	// operator new[] of both sides (and the image's operator new) fill with the pattern.
	Redirect((void *)(uintptr_t)0x523817, (void *)PatternNew);
	Redirect((void *)(uintptr_t)0x5237e7, (void *)PatternNew);
	Redirect((void *)(void *(__cdecl *)(size_t)) operator new[], (void *)PatternNew);
#else
	PatternActive = true;
#endif
	int Before = Checks;

	// One entry per instance in the image (Container.h's list).
	TestVector<TLineFormat>({"vector<TLineFormat>", 0x417aa0, 0, 0x417710, 0}, 300);
	TestVector<TSkinButton *>({"vector<TSkinButton *>", 0, 0x425e70, 0x452ac0, 0x452c50}, 300);
	TestVector<TMark>({"vector<TMark>", 0x426170, 0, 0x425b20, 0x425b10}, 200);
	TestVector<int>({"vector<int>", 0x4a2b90, 0x453300, 0x452ac0, 0x452c50}, 600);
	TestVector<TListBoxLine>({"vector<TListBoxLine>", 0x453600, 0, 0x452eb0, 0x452c60}, 300);
	TestVector<TIgnoreEntry>({"vector<TIgnoreEntry>", 0x46c0d0, 0, 0x46bb70, 0x46bb60}, 300);
	TestVector<THotkey>({"vector<THotkey>", 0x4976f0, 0, 0x496570, 0}, 300);
	TestVector<TTimer *>({"vector<TTimer *>", 0, 0x4973f0, 0x452ac0, 0}, 300);
	TestVector<TContentWindowSize>({"vector<TContentWindowSize>", 0x4dbe90, 0, 0x4db880, 0}, 300);
	TestVector<TBuddy>({"vector<TBuddy>", 0x4dc170, 0, 0x4dba60, 0}, 200);
	TestVector<TMessage>({"vector<TMessage>", 0x4e7f50, 0, 0x4e7390, 0}, 300);
	TestVector<TErrorReport>({"vector<TErrorReport>", 0x4e8230, 0, 0x4e7860, 0}, 60);
	TestVector<TTalkFilter *>({"vector<TTalkFilter *>", 0, 0x4fd6e0, 0x452ac0, 0}, 300);

	TestArray<TMapPoint>("array<TMapPoint>", 0x4b0bc0, 0x4b5900, 50, 100);
	TestArray<TProperties>("array<TProperties>", 0x4cfa30, 0x4d1a10, 50, 100);
	TestMatrix<TPathNode>("matrix<TPathNode>", 0x4b1100, 0x4b5e90, 0x4b0b20, 30, 100);
	TestMatrix3d<int>("matrix3d<int>", 0x4b0d70, 0x4b5b40, 12, 100);

	TestHeaderInlines();
#if GOLDEN_ORACLE
	RestoreAll();
#else
	PatternActive = false;
#endif
	printf("Container: %d checks\n", Checks - Before);
}

} // namespace ContainerTest

#if !GOLDEN_ORACLE
// The runner's operator new[]: the pattern while the test runs (the oracle patches its CRT's instead),
// else malloc, as MSVC's; delete[] frees.
void *operator new[](size_t Size)
{
	if (ContainerTest::PatternActive)
		return ContainerTest::PatternNew(Size);
	void *Memory = malloc(Size == 0 ? 1 : Size);
	if (Memory == nullptr)
		throw std::bad_alloc();
	return Memory;
}
void operator delete[](void *Memory) noexcept { free(Memory); }
void operator delete[](void *Memory, size_t) noexcept { free(Memory); }
#endif

void TestContainer()
{
	ContainerTest::Run();
}
