// difftest.exe's first tests (moved here from tools/difftest/difftest.cpp and statustest.cpp, issue
// #24): Utils.cpp's string, colour and buffer helpers, TXTEA and Skins.cpp against the original, as
// golden-corpus tests (golden.h): the oracle calls the original inside ORIGINAL and #if GOLDEN_ORACLE,
// the runner gets the original's results from the corpus.

#include "golden_test.h"

#include "Skins.h"
#include "TXTEA.h"
#include "Utils.h"

#include <string.h>

#if GOLDEN_ORACLE
// Original functions.
typedef void(__cdecl *XTEAFn)(uint32 *Block, const uint32 *Key);
typedef int(__cdecl *CharFn)(int c);
typedef void(__cdecl *HSVFn)(double H, double S, double V, int *R, int *G, int *B);
typedef void(__cdecl *OutfitFn)(int Color, int *R, int *G, int *B);
typedef void(__cdecl *StrFn)(char *s);
typedef int(__cdecl *CompareFn)(const char *s1, const char *s2, int Count);
typedef void(__cdecl *CopyFn)(char *Destination, const char *Source, int Size);
typedef void(__cdecl *TrimFn)(char *Destination, const char *Source);
typedef bool(__cdecl *ShortenFn)(char *Text, int MaxLength);
typedef void(__cdecl *FormatFn)(int Number, char *Buffer);
// thiscall members through __fastcall: ECX = this, EDX unused, rest on the stack.
typedef void(__fastcall *BufInitFn)(void *This, void *, uint8 *Data, int Size);
typedef uint32(__fastcall *BufGetFn)(void *This, void *);
typedef void(__fastcall *BufGetStringFn)(void *This, void *, char *Buffer, int MaxLength);
typedef void(__fastcall *BufGetBytesFn)(void *This, void *, uint8 *Buffer, int Count);
typedef void(__fastcall *BufPutFn)(void *This, void *, uint32 Data);
typedef void(__fastcall *BufPutStringFn)(void *This, void *, const char *s);
typedef void(__fastcall *BufPutBytesFn)(void *This, void *, const uint8 *Buffer, int Count);
typedef void(__fastcall *XTEAKeyFn)(void *This, void *, const uint8 *Key);
typedef void(__fastcall *XTEABlockFn)(void *This, void *, uint8 *Block);
#endif

void TestXTEA()
{
#if GOLDEN_ORACLE
	XTEAFn Encrypt = At<XTEAFn>(0x508cd0);
	XTEAFn Decrypt = At<XTEAFn>(0x508e00);
	XTEAKeyFn SetKey = At<XTEAKeyFn>(0x508ca0);
	XTEABlockFn EncryptBlock = At<XTEABlockFn>(0x508f60);
	XTEABlockFn DecryptBlock = At<XTEABlockFn>(0x508f80);
#endif
	for (int t = 0; t < 2000; t++) {
		uint8 Key[16];
		for (int i = 0; i < 16; i++)
			Key[i] = (uint8)Random();
		uint32 Plain[2] = {Random32(), Random32()};
		golden::Input("key", Key);
		golden::Input("plain", Plain);

		uint32 Original[2] = {Plain[0], Plain[1]};
		ORIGINAL(Encrypt(Original, (const uint32 *)Key));
		golden::Output("encrypt", Original);
		TXTEA Cipher;
		Cipher.SetKey(Key);
		uint32 Ours[2] = {Plain[0], Plain[1]};
		Cipher.EncryptBlock(TXTEA::TBlock((uint8 *)Ours, 8));
		CHECK(Ours[0] == Original[0] && Ours[1] == Original[1], "XTEA encrypt differs");

		// The original object: vtable, then the key.
		uint32 Wrapped[2] = {Plain[0], Plain[1]};
#if GOLDEN_ORACLE
		uint32 Object[5] = {0, 0, 0, 0, 0};
		SetKey(Object, 0, Key);
		EncryptBlock(Object, 0, (uint8 *)Wrapped);
#endif
		golden::Output("encrypt block", Wrapped);
		CHECK(Wrapped[0] == Original[0] && Wrapped[1] == Original[1], "TXTEA::EncryptBlock differs");

		ORIGINAL(Decrypt(Original, (const uint32 *)Key));
		golden::Output("decrypt", Original);
		Cipher.DecryptBlock(TXTEA::TBlock((uint8 *)Ours, 8));
		ORIGINAL(DecryptBlock(Object, 0, (uint8 *)Wrapped));
		golden::Output("decrypt block", Wrapped);
		CHECK(Original[0] == Plain[0] && Ours[0] == Plain[0] && Ours[1] == Plain[1] && Wrapped[0] == Plain[0],
			"XTEA decrypt differs");
	}
}

void TestCharacters()
{
	struct {
		uint32 Address;
		int (*Ours)(int);
		const char *Name;
	} Functions[] = {
		{0x508fa0, IsSpace, "IsSpace"},
		{0x508fd0, IsAlpha, "IsAlpha"},
		{0x509020, IsDigit, "IsDigit"},
		{0x509040, ToLower, "ToLower"},
		{0x509060, ToUpper, "ToUpper"},
	};
	for (auto &F : Functions) {
		ORIGINAL(CharFn Original = At<CharFn>(F.Address));
		for (int c = -300; c < 600; c++) {
			int Result = 0;
			ORIGINAL(Result = Original(c));
			golden::Output(F.Name, Result);
			CHECK(F.Ours(c) == Result, "%s(%d): %d, original %d", F.Name, c, F.Ours(c), Result);
		}
	}
}

void TestColors()
{
	ORIGINAL(HSVFn HSV = At<HSVFn>(0x509080));
	ORIGINAL(OutfitFn Outfit = At<OutfitFn>(0x509280));
	for (int Color = -5; Color < 140; Color++) {
		int r1, g1, b1, r2 = 0, g2 = 0, b2 = 0;
		GetOutfitColor(Color, &r1, &g1, &b1);
		ORIGINAL(Outfit(Color, &r2, &g2, &b2));
		int Rgb[3] = {r2, g2, b2};
		golden::Output("outfit", Rgb);
		r2 = Rgb[0];
		g2 = Rgb[1];
		b2 = Rgb[2];
		CHECK(r1 == r2 && g1 == g2 && b1 == b2, "GetOutfitColor(%d): %d,%d,%d original %d,%d,%d",
			Color, r1, g1, b1, r2, g2, b2);
	}
	for (int t = 0; t < 20000; t++) {
		double H = (Random() % 1001) / 1000.0, S = (Random() % 1001) / 1000.0, V = (Random() % 1001) / 1000.0;
		if (t % 7 == 0)
			S = 0.0;
		if (t % 11 == 0)
			V = 0.0;
		double Hsv[3] = {H, S, V};
		golden::Input("hsv", Hsv);
		int r1, g1, b1, r2 = 0, g2 = 0, b2 = 0;
		HSVToRGB(H, S, V, &r1, &g1, &b1);
		ORIGINAL(HSV(H, S, V, &r2, &g2, &b2));
		int Rgb[3] = {r2, g2, b2};
		golden::Output("rgb", Rgb);
		r2 = Rgb[0];
		g2 = Rgb[1];
		b2 = Rgb[2];
		CHECK(r1 == r2 && g1 == g2 && b1 == b2, "HSVToRGB(%g,%g,%g): %d,%d,%d original %d,%d,%d",
			H, S, V, r1, g1, b1, r2, g2, b2);
	}
}

void TestStrings()
{
#if GOLDEN_ORACLE
	StrFn Lower = At<StrFn>(0x5093d0);
	StrFn Capitalize = At<StrFn>(0x509540);
	CompareFn Compare = At<CompareFn>(0x509710);
	CopyFn Copy = At<CopyFn>(0x509a60);
	TrimFn Trim = At<TrimFn>(0x509d10);
	ShortenFn Shorten = At<ShortenFn>(0x50a040);
#endif
	for (int t = 0; t < 5000; t++) {
		char A[64], B[64], C[64], D[64];
		int Length = Random() % 40;
		RandomText(A, Length);
		golden::InputBytes("lower input", A, (size_t)Length + 1);
		strcpy(B, A);
		LowerString(A);
		ORIGINAL(Lower(B));
		golden::OutputBytes("lower", B, (size_t)Length + 1);
		CHECK(strcmp(A, B) == 0, "LowerString differs");

		RandomText(A, Length);
		golden::InputBytes("capitalize input", A, (size_t)Length + 1);
		strcpy(B, A);
		CapitalizeString(A);
		ORIGINAL(Capitalize(B));
		golden::OutputBytes("capitalize", B, (size_t)Length + 1);
		CHECK(strcmp(A, B) == 0, "CapitalizeString differs");

		RandomText(A, Length);
		strcpy(B, A);
		if (Random() % 3 == 0)
			for (int i = 0; B[i]; i++)
				if (Random() % 4 == 0)
					B[i] = (char)ToUpper(B[i]);
		if (Random() % 4 == 0 && Length > 0)
			B[Random() % Length] = (char)Random();
		int Count = Random() % 45;
		golden::InputBytes("compare a", A, (size_t)Length + 1);
		golden::InputBytes("compare b", B, (size_t)Length + 1);
		golden::Input("compare count", Count);
		int Compared = 0;
		ORIGINAL(Compared = Compare(A, B, Count));
		golden::Output("compare", Compared);
		CHECK(CompareStrings(A, B, Count) == Compared, "CompareStrings differs");

		int Size = 1 + Random() % 50;
		golden::Input("copy size", Size);
		memset(C, 'x', sizeof(C));
		memset(D, 'x', sizeof(D));
		CopyString(OriginalSpan(C, Size), A);
		ORIGINAL(Copy(D, A, Size));
		golden::Output("copy", D);
		CHECK(memcmp(C, D, sizeof(C)) == 0, "CopyString differs");

		memset(C, 'x', sizeof(C));
		memset(D, 'x', sizeof(D));
		TrimString(C, A);
		ORIGINAL(Trim(D, A));
		golden::Output("trim", D);
		CHECK(memcmp(C, D, sizeof(C)) == 0, "TrimString differs");

		strcpy(C, A);
		strcpy(D, A);
		int MaxLength = Random() % 45;
		golden::Input("shorten length", MaxLength);
		bool r1 = ShortenString(C, MaxLength);
		bool r2 = false;
		ORIGINAL(r2 = Shorten(D, MaxLength));
		golden::Output("shorten result", r2);
		golden::Output("shorten", D);
		CHECK(r1 == r2 && strcmp(C, D) == 0, "ShortenString(\"%s\", %d) differs", A, MaxLength);
	}
}

#if GOLDEN_ORACLE
// The probe: the original's sprintf needs the original's C runtime, which the oracle does not start.
static uint8 FormatNumberFails(FormatFn Format)
{
	__try {
		char Probe[32];
		Format(1234567, Probe);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return 1;
	}
	return 0;
}
#endif

void TestFormatNumber()
{
	// The runner skips when the oracle did.
	uint8 Skip = 0;
#if GOLDEN_ORACLE
	FormatFn Format = At<FormatFn>(0x50bce0);
	Skip = FormatNumberFails(Format);
#endif
	golden::Output("skip", Skip);
	if (Skip != 0) {
		printf("skip FormatNumber: the original sprintf needs the original CRT\n");
		return;
	}
	for (int t = 0; t < 2000; t++) {
		int Number = t < 1000 ? t * t * 37 : (int)(Random() & 0x7fffffff);
		golden::Input("number", Number);
		char A[32], B[32] = {};
		FormatNumber(Number, A);
		ORIGINAL(Format(Number, B));
		std::string Original = golden::OutputText("format", B);
		CHECK(strcmp(A, Original.c_str()) == 0, "FormatNumber(%d): \"%s\" original \"%s\"", Number, A,
			Original.c_str());
	}
}

void TestBuffers()
{
#if GOLDEN_ORACLE
	BufInitFn ReadInit = At<BufInitFn>(0x50a200);
	BufGetFn GetByte = At<BufGetFn>(0x50a4b0), GetWord = At<BufGetFn>(0x50a610), GetQuad = At<BufGetFn>(0x50a780);
	BufGetStringFn GetStr = At<BufGetStringFn>(0x50a900);
	BufGetBytesFn GetBytes = At<BufGetBytesFn>(0x50ad50);
	BufInitFn WriteInit = At<BufInitFn>(0x50b030);
	BufPutFn PutByte = At<BufPutFn>(0x50b2e0), PutWord = At<BufPutFn>(0x50b450), PutQuad = At<BufPutFn>(0x50b5e0);
	BufPutStringFn PutStr = At<BufPutStringFn>(0x50b780);
	BufPutBytesFn PutBytes = At<BufPutBytesFn>(0x50ba00);
#endif

	for (int t = 0; t < 2000; t++) {
		uint8 Ours[1024], Original[1024];
		memset(Ours, 0xcd, sizeof(Ours));
		memset(Original, 0xcd, sizeof(Original));
		TWriteBuffer W(Ours);
#if GOLDEN_ORACLE
		uint32 WO[3];
		WriteInit(WO, 0, Original, sizeof(Original));
#endif
		// Random sequence of writes, at most ~800 bytes.
		uint32 Ops[40], Values[40];
		// Zeroed (issue #55): PutBytes takes 7 bytes, also those after a shorter text's end, which were the
		// stack's leftovers, so two recordings differed there.
		char Texts[40][32] = {};
		int OpCount = Random() % 40;
		golden::Input("operations", OpCount);
		for (int i = 0; i < OpCount; i++) {
			Ops[i] = Random() % 5;
			Values[i] = Random32();
			RandomText(Texts[i], Random() % 20);
			golden::Input("operation", Ops[i]);
			golden::Input("value", Values[i]);
			golden::InputBytes("text", Texts[i], strlen(Texts[i]) + 1);
			switch (Ops[i]) {
			case 0:
				W.PutByte((uint8)Values[i]);
				ORIGINAL(PutByte(WO, 0, (uint8)Values[i]));
				break;
			case 1:
				W.PutWord((uint16)Values[i]);
				ORIGINAL(PutWord(WO, 0, (uint16)Values[i]));
				break;
			case 2:
				W.PutQuad(Values[i]);
				ORIGINAL(PutQuad(WO, 0, Values[i]));
				break;
			case 3:
				W.PutString(Texts[i]);
				ORIGINAL(PutStr(WO, 0, Texts[i]));
				break;
			case 4:
				W.PutBytes(OriginalSpan((const uint8 *)Texts[i], 7));
				ORIGINAL(PutBytes(WO, 0, (const uint8 *)Texts[i], 7));
				break;
			}
		}
		golden::Output("written", Original);
		uint32 WrittenPosition = 0;
		ORIGINAL(WrittenPosition = WO[2]);
		golden::Output("write position", WrittenPosition);
		CHECK(memcmp(Ours, Original, sizeof(Ours)) == 0 && W.Position == (int)WrittenPosition, "TWriteBuffer differs");

		TReadBuffer R(OriginalSpan(Ours, W.Position));
#if GOLDEN_ORACLE
		uint32 RO[3];
		ReadInit(RO, 0, Original, WO[2]);
#endif
		for (int i = 0; i < OpCount; i++) {
			char A[32], B[32];
			uint8 X[8], Y[8] = {};
			int MaxLength = (int)(Random() % 24) - 2;
			if (MaxLength == 0)
				MaxLength = -1;
			golden::Input("max length", MaxLength);
			uint32 Read = 0;
			switch (Ops[i]) {
			case 0:
				ORIGINAL(Read = GetByte(RO, 0));
				golden::Output("byte", Read);
				CHECK(R.GetByte() == (uint8)Read, "GetByte differs");
				break;
			case 1:
				ORIGINAL(Read = GetWord(RO, 0));
				golden::Output("word", Read);
				CHECK(R.GetWord() == (uint16)Read, "GetWord differs");
				break;
			case 2:
				ORIGINAL(Read = GetQuad(RO, 0));
				golden::Output("quad", Read);
				CHECK(R.GetQuad() == Read, "GetQuad differs");
				break;
			case 3:
				memset(A, 'x', sizeof(A));
				memset(B, 'x', sizeof(B));
				R.GetString(OriginalSpan(A, MaxLength));
				ORIGINAL(GetStr(RO, 0, B, MaxLength));
				golden::Output("string", B);
				CHECK(memcmp(A, B, sizeof(A)) == 0, "GetString(MaxLength %d) differs", MaxLength);
				break;
			case 4:
				R.GetBytes(OriginalSpan(X, 7));
				ORIGINAL(GetBytes(RO, 0, Y, 7));
				golden::OutputBytes("bytes", Y, 7);
				CHECK(memcmp(X, Y, 7) == 0, "GetBytes differs");
				break;
			}
			uint32 ReadPosition = 0;
			ORIGINAL(ReadPosition = RO[2]);
			golden::Output("read position", ReadPosition);
			CHECK(R.Position == (int)ReadPosition, "read position differs");
		}
	}
}

void TestSkins()
{
	int OriginalCount = 0;
#if GOLDEN_ORACLE
	PatchImage();
	At<void(__cdecl *)()>(0x4e3cb0)();
	OriginalCount = Global<int>(0x71c450);
	TSkin *&OriginalSkins = Global<TSkin *>(0x71c454);
#endif
	InitSkins();
	golden::Output("count", OriginalCount);
	CHECK(NumberOfSkins == OriginalCount, "NumberOfSkins %d, original %d", NumberOfSkins, OriginalCount);
	for (int i = 0; i < NumberOfSkins && i < OriginalCount; i++) {
		if (i == 152)
			continue; // left uninitialised by both
		int Original[4] = {};
#if GOLDEN_ORACLE
		Original[0] = OriginalSkins[i].x;
		Original[1] = OriginalSkins[i].y;
		Original[2] = OriginalSkins[i].width;
		Original[3] = OriginalSkins[i].height;
#endif
		golden::Output("skin", Original);
		Checks++;
		if (Skins[i].x != Original[0] || Skins[i].y != Original[1] || Skins[i].width != Original[2]
			|| Skins[i].height != Original[3]) {
			Failures++;
			printf("FAIL Skins[%d]: %d,%d,%d,%d original %d,%d,%d,%d\n", i, Skins[i].x, Skins[i].y,
				Skins[i].width, Skins[i].height, Original[0], Original[1], Original[2], Original[3]);
		}
	}
	uint8 OriginalFreed = 0;
#if GOLDEN_ORACLE
	At<void(__cdecl *)()>(0x4e6da0)();
	OriginalFreed = OriginalSkins == NULL;
#endif
	ExitSkins();
	golden::Output("freed", OriginalFreed);
	CHECK(Skins == NULL && OriginalFreed != 0, "ExitSkins leaves a pointer");
}
