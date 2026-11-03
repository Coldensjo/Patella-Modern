// Differential tests of Script.cpp against the original code.
//
// The image's stdio functions are redirected to ours (both sides then share
// one CRT and one kind of FILE), as are remove, rename and the file-exists
// check, so the original reads and writes real files in a temporary
// directory. Inputs are generated so that no error is raised: the error
// paths would need the original's exception machinery.

#include "difftest.h"
#include "../../src/Script.h"
#include <stdarg.h>
#include <io.h>
#include <string>
#include <vector>

// The reconstructed side's external function (sysinc.cpp).
bool FileExists(const char *FileName) { return _access(FileName, 0) == 0; }

// --- Replacements patched into the image -----------------------------------

static FILE *__cdecl ImageFopen(const char *Name, const char *Mode) { return fopen(Name, Mode); }
static int __cdecl ImageFclose(FILE *File) { return fclose(File); }
static int __cdecl ImageGetc(FILE *File) { return getc(File); }
static int __cdecl ImageUngetc(int c, FILE *File) { return ungetc(c, File); }
static int __cdecl ImageFputc(int c, FILE *File) { return fputc(c, File); }
static size_t __cdecl ImageFread(void *Buffer, size_t Size, size_t Count, FILE *File) { return fread(Buffer, Size, Count, File); }
static size_t __cdecl ImageFwrite(const void *Buffer, size_t Size, size_t Count, FILE *File) { return fwrite(Buffer, Size, Count, File); }
static int __cdecl ImageFseek(FILE *File, long Offset, int Origin) { return fseek(File, Offset, Origin); }
static int __cdecl ImageFileno(FILE *File) { return _fileno(File); }
static int __cdecl ImageEof(int Handle) { return _eof(Handle); }
static int __cdecl ImageRemove(const char *Name) { return remove(Name); }
static int __cdecl ImageRename(const char *Old, const char *New) { return rename(Old, New); }
static bool __cdecl ImageFileExists(const char *Name) { return _access(Name, 0) == 0; }
static int __cdecl ImageSprintf(char *Buffer, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Result = vsprintf(Buffer, Format, Args);
	va_end(Args);
	return Result;
}

void PatchScript()
{
	static bool Done = false;
	if (Done)
		return;
	Done = true;
	PatchImage();
	PatchJump(0x51227f, (void *)ImageFopen);
	PatchJump(0x512122, (void *)ImageFclose);
	PatchJump(0x513237, (void *)ImageGetc);
	PatchJump(0x513039, (void *)ImageUngetc);
	PatchJump(0x5133da, (void *)ImageFputc);
	PatchJump(0x513a29, (void *)ImageFread);
	PatchJump(0x51338e, (void *)ImageFwrite);
	PatchJump(0x5134e3, (void *)ImageFseek);
	PatchJump(0x523017, (void *)ImageFileno);
	PatchJump(0x5136de, (void *)ImageEof);
	PatchJump(0x513c96, (void *)ImageRemove);
	PatchJump(0x513a75, (void *)ImageRename);
	PatchJump(0x4fcba0, (void *)ImageFileExists);
	PatchJump(0x511d6b, (void *)ImageSprintf);
}

// Member functions of the original, called with this in ECX.
typedef void (__fastcall *ThisFn)(void *This, void *);
typedef void (__fastcall *ThisStrFn)(void *This, void *, const char *);
typedef void (__fastcall *ThisIntFn)(void *This, void *, int);
typedef int (__fastcall *ThisRetIntFn)(void *This, void *);
typedef char *(__fastcall *ThisRetStrFn)(void *This, void *);
typedef char (__fastcall *ThisRetCharFn)(void *This, void *);
typedef void (__fastcall *ThisCharFn)(void *This, void *, char);
typedef void (__fastcall *ThisBytesFn)(void *This, void *, const void *, int);

static std::string TempDir;

static std::string TempPath(const char *Name)
{
	return TempDir + Name;
}

static void WriteFileText(const std::string &Name, const std::string &Text)
{
	FILE *File = fopen(Name.c_str(), "wb");
	fwrite(Text.data(), 1, Text.size(), File);
	fclose(File);
}

static bool ReadFileText(const std::string &Name, std::string &Text)
{
	FILE *File = fopen(Name.c_str(), "rb");
	if (File == NULL)
		return false;
	Text.clear();
	int c;
	while ((c = getc(File)) != EOF)
		Text += (char)c;
	fclose(File);
	return true;
}

// --- Script text generator (never raises an error) -------------------------

static std::string RandomIdentifier(bool Ascii = false)
{
	static const char First[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\xe4\xf6\xfc\xc4\xd6\xdc";
	static const char Rest[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_\xdf";
	std::string s(1, First[Random() % (sizeof(First) - (Ascii ? 7 : 1))]);
	int Length = Random() % 19;
	for (int i = 0; i < Length; i++)
		s += Rest[Random() % (sizeof(Rest) - (Ascii ? 2 : 1))];
	return s;
}

static std::string RandomNumberText()
{
	char Help[16];
	switch (Random() % 3) {
	case 0: sprintf(Help, "%u", Random() % 10); break;
	case 1: sprintf(Help, "%u", Random() % 100000); break;
	default: sprintf(Help, "%u", Random() % 2000000000); break;
	}
	return Help;
}

static std::string RandomToken()
{
	switch (Random() % 14) {
	case 0: case 1: return RandomIdentifier();
	case 2: return RandomNumberText();
	case 3: return "-" + RandomNumberText();
	case 4: { // byte sequence
		std::string s = RandomNumberText();
		int n = 1 + Random() % 8;
		for (int i = 0; i < n; i++)
			s += "-" + RandomNumberText();
		return s;
	}
	case 5: { // string
		static const char Chars[] = "abc XYZ 019 .,;:!?()[]{}<>=-#'\t\xe4\xdf";
		std::string s = "\"";
		int n = Random() % 60;
		for (int i = 0; i < n; i++) {
			switch (Random() % 12) {
			case 0: s += "\\\""; break;
			case 1: s += "\\\\"; break;
			case 2: s += "\\n"; break;
			case 3: s += "\\x"; break;
			case 4: s += "\n"; break;
			default: s += Chars[Random() % (sizeof(Chars) - 1)]; break;
			}
		}
		return s + "\"";
	}
	case 6: return "[" + RandomNumberText() + "," + RandomNumberText() + "," + RandomNumberText() + "]";
	case 7: { static const char *S[] = {"<=", ">=", "<>", "->", "<", ">", "-", "[", "="}; return S[Random() % 9]; }
	case 8: { static const char S[] = "=(),{}:;.+*/!?&|%$@~^'"; return std::string(1, S[Random() % (sizeof(S) - 1)]); }
	case 9: return "# comment " + RandomIdentifier() + "\n";
	case 10: return "\n";
	case 11: return "\t";
	case 12: return "\r\n";
	default: return "[x";
	}
}

static std::string RandomScript()
{
	std::string s;
	int n = Random() % 80;
	for (int i = 0; i < n; i++) {
		s += RandomToken();
		s += " ";
	}
	if (Random() % 2)
		s += RandomToken(); // a token right at the end of the file
	return s;
}

static_assert(sizeof(TReadScriptFile) == 0x80c, "TReadScriptFile layout");
static_assert(sizeof(TWriteScriptFile) == 0x408, "TWriteScriptFile layout");
static_assert(sizeof(TReadBinaryFile) == 0x404, "TReadBinaryFile layout");

// --- Tests --------------------------------------------------------------------

static void CompareReader(const TReadScriptFile &Ours, const uint8 *Original, const char *What, int Index)
{
	const TReadScriptFile &O = *(const TReadScriptFile *)Original;
	bool Same = Ours.Token == O.Token && Ours.Line == O.Line && Ours.Number == O.Number
		&& Ours.CoordX == O.CoordX && Ours.CoordY == O.CoordY && Ours.CoordZ == O.CoordZ
		&& Ours.Special == O.Special && memcmp(Ours.String, O.String, sizeof(Ours.String)) == 0
		&& strcmp(Ours.Filename, O.Filename) == 0
		&& (Ours.File == NULL) == (O.File == NULL)
		&& (uint8 *)Ours.Bytes - (uint8 *)&Ours == *(uint8 **)(Original + 0x7f4) - Original;
	CHECK(Same, "%s, token %d: ours %d '%s' %d [%d,%d,%d] '%c' line %d, original %d '%s' %d [%d,%d,%d] '%c' line %d",
		What, Index, (int)Ours.Token, Ours.String, Ours.Number, Ours.CoordX, Ours.CoordY, Ours.CoordZ, Ours.Special, Ours.Line,
		(int)O.Token, O.String, O.Number, O.CoordX, O.CoordY, O.CoordZ, O.Special, O.Line);
}

static void TestReader()
{
	static uint8 Original[sizeof(TReadScriptFile)];
	std::string Name = TempPath("script.txt");
	for (int Round = 0; Round < 3000; Round++) {
		WriteFileText(Name, RandomScript());
		memset(Original, 0x55, sizeof(Original));
		At<ThisFn>(0x4df550)(Original, NULL);
		TReadScriptFile Ours;
		Ours.Token = (TOKEN)0x55555555; // not set by the constructor
		CompareReader(Ours, Original, "constructor", 0);
		Ours.open(Name.c_str());
		At<ThisStrFn>(0x4df6d0)(Original, NULL, Name.c_str());
		CompareReader(Ours, Original, "open", 0);
		for (int i = 0; i < 200; i++) {
			// Mix the plain tokenizer with the read helpers where the next
			// token is known to fit.
			Ours.nextToken();

			At<ThisFn>(0x4dfb60)(Original, NULL);
			CompareReader(Ours, Original, "nextToken", i);
			TReadScriptFile &O = *(TReadScriptFile *)Original;
			if (Ours.Token != O.Token)
				break;
			if (Ours.Token == TOKEN::ENDOFFILE)
				break;
			if (Ours.Token == TOKEN::IDENTIFIER) {
				char *a = Ours.getIdentifier();
				char *b = At<ThisRetStrFn>(0x4e0420)(Original, NULL);
				CHECK(a - (char *)&Ours == b - (char *)Original && strcmp(a, b) == 0, "getIdentifier '%s' '%s'", a, b);
			} else if (Ours.Token == TOKEN::NUMBER) {
				int a = Ours.getNumber(), b = At<ThisRetIntFn>(0x4e0450)(Original, NULL);
				CHECK(a == b, "getNumber %d %d", a, b);
			} else if (Ours.Token == TOKEN::STRING) {
				char *a = Ours.getString();
				char *b = At<ThisRetStrFn>(0x4e0470)(Original, NULL);
				CHECK(a - (char *)&Ours == b - (char *)Original, "getString");
			} else if (Ours.Token == TOKEN::SPECIAL) {
				char a = Ours.getSpecial(), b = At<ThisRetCharFn>(0x4e0490)(Original, NULL);
				CHECK(a == b, "getSpecial %d %d", a, b);
			}
			CompareReader(Ours, Original, "get", i);
		}
		Ours.close();
		At<ThisFn>(0x4df5d0)(Original, NULL);
		CompareReader(Ours, Original, "close", 0);
		At<ThisFn>(0x4df5b0)(Original, NULL);
	}

	// The read helpers on scripts made of the expected tokens.
	for (int Round = 0; Round < 2000; Round++) {
		std::string Text;
		std::vector<int> Kinds;
		std::vector<char> Symbols;
		int n = 1 + Random() % 30;
		for (int i = 0; i < n; i++) {
			int Kind = Random() % 4;
			Kinds.push_back(Kind);
			Symbols.push_back("=(),{}"[Random() % 6]);
			switch (Kind) {
			case 0: Text += RandomIdentifier(true); break; // getc gives umlauts as 0xe4 etc., which IsAlpha rejects
			case 1: Text += (Random() % 2 ? "-" : "") + RandomNumberText(); break;
			case 2: { std::string s = RandomToken(); while (s[0] != '"') s = RandomToken(); Text += s; break; }
			default: Text += Symbols[i]; break;
			}
			Text += Random() % 4 ? " " : "\n";
		}
		WriteFileText(Name, Text);
		At<ThisFn>(0x4df550)(Original, NULL);
		TReadScriptFile Ours;
		Ours.open(Name.c_str());
		At<ThisStrFn>(0x4df6d0)(Original, NULL, Name.c_str());
		for (int i = 0; i < n; i++) {
			switch (Kinds[i]) {
			case 0: {
				char *a = Ours.readIdentifier();
				char *b = At<ThisRetStrFn>(0x4e3920)(Original, NULL);
				CHECK(strcmp(a, b) == 0, "readIdentifier '%s' '%s'", a, b);
				break;
			}
			case 1: {
				int a = Ours.readNumber(), b = At<ThisRetIntFn>(0x4e3950)(Original, NULL);
				CHECK(a == b, "readNumber %d %d", a, b);
				break;
			}
			case 2: {
				char *a = Ours.readString();
				char *b = At<ThisRetStrFn>(0x4e39a0)(Original, NULL);
				CHECK(strcmp(a, b) == 0, "readString");
				break;
			}
			default: {
				Ours.readSymbol(Symbols[i]);
				At<ThisCharFn>(0x4e39d0)(Original, NULL, Symbols[i]);
				break;
			}
			}
			CompareReader(Ours, Original, "read", i);
		}
		At<ThisFn>(0x4df5b0)(Original, NULL);
	}
}

static void TestWriter()
{
	static uint8 Original[sizeof(TWriteScriptFile)];
	std::string NameOurs = TempPath("ours.txt"), NameOriginal = TempPath("original.txt");
	for (int Round = 0; Round < 2000; Round++) {
		TWriteScriptFile Ours;
		At<ThisFn>(0x4df640)(Original, NULL);
		CHECK(memcmp(&Ours, Original, sizeof(Original)) == 0, "TWriteScriptFile constructor");
		Ours.open(NameOurs.c_str());
		At<ThisStrFn>(0x4e04b0)(Original, NULL, NameOriginal.c_str());
		int n = Random() % 40;
		for (int i = 0; i < n; i++) {
			char Text[200];
			int Length = Random() % 60;
			for (int j = 0; j < Length; j++) {
				static const char Chars[] = "ab \"\\\n\t=,()\xe4";
				Text[j] = Chars[Random() % (sizeof(Chars) - 1)];
			}
			Text[Length] = 0;
			switch (Random() % 4) {
			case 0: Ours.writeLn(); At<ThisFn>(0x4e0790)(Original, NULL); break;
			case 1: Ours.writeText(Text); At<ThisStrFn>(0x4e08f0)(Original, NULL, Text); break;
			case 2: Ours.writeString(Text); At<ThisStrFn>(0x4e0bb0)(Original, NULL, Text); break;
			default: {
				int Number = (int)Random();
				if (Random() % 2)
					Number %= 1000;
				Ours.writeNumber(Number);
				At<ThisIntFn>(0x4e0ec0)(Original, NULL, Number);
				break;
			}
			}
		}
		CHECK(Ours.Line == ((TWriteScriptFile *)Original)->Line, "TWriteScriptFile Line");
		Ours.close();
		At<ThisFn>(0x4df660)(Original, NULL);
		CHECK(Ours.File == NULL && memcmp(&Ours, Original, sizeof(Original)) == 0, "TWriteScriptFile close");
		std::string A, B;
		ReadFileText(NameOurs, A);
		ReadFileText(NameOriginal, B);
		CHECK(A == B, "TWriteScriptFile output differs (%d and %d bytes)", (int)A.size(), (int)B.size());
	}
}

static void TestBinary()
{
	static uint8 Original[sizeof(TReadBinaryFile)];
	std::string NameOurs = TempPath("ours.bin"), NameOriginal = TempPath("original.bin");
	for (int Round = 0; Round < 2000; Round++) {
		// Writing.
		TWriteBinaryFile Ours;
		memset(Original, 0x55, sizeof(Original));
		At<ThisFn>(0x4df690)(Original, NULL);
		CHECK(memcmp(&Ours, Original, sizeof(Original)) == 0, "TWriteBinaryFile constructor");
		Ours.open(NameOurs.c_str());
		At<ThisStrFn>(0x4e21a0)(Original, NULL, NameOriginal.c_str());
		std::vector<int> Kinds;
		std::vector<uint32> Values;
		std::vector<std::string> Strings;
		int n = Random() % 60;
		for (int i = 0; i < n; i++) {
			int Kind = Random() % 5;
			uint32 Value = Random();
			std::string Text;
			Kinds.push_back(Kind);
			Values.push_back(Value);
			switch (Kind) {
			case 0: Ours.writeByte((uint8)Value); At<ThisIntFn>(0x4e2480)(Original, NULL, (uint8)Value); break;
			case 1: Ours.writeWord((uint16)Value); At<ThisIntFn>(0x4e2750)(Original, NULL, (uint16)Value); break;
			case 2: Ours.writeQuad(Value); At<ThisIntFn>(0x4e29e0)(Original, NULL, (int)Value); break;
			case 3: {
				int Length = Random() % 50;
				for (int j = 0; j < Length; j++)
					Text += (char)(1 + Random() % 255);
				Ours.writeString(Text.c_str());
				At<ThisStrFn>(0x4e2f50)(Original, NULL, Text.c_str());
				break;
			}
			default: {
				int Length = 1 + Random() % 50;
				for (int j = 0; j < Length; j++)
					Text += (char)Random();
				Ours.writeBytes((const uint8 *)Text.data(), Length);
				At<ThisBytesFn>(0x4e2c80)(Original, NULL, Text.data(), Length);
				break;
			}
			}
			Strings.push_back(Text);
		}
		Ours.close();
		At<ThisFn>(0x4df6b0)(Original, NULL);
		std::string A, B;
		ReadFileText(NameOurs, A);
		ReadFileText(NameOriginal, B);
		CHECK(A == B, "TWriteBinaryFile output differs (%d and %d bytes)", (int)A.size(), (int)B.size());

		// Reading the same file back on both sides.
		TReadBinaryFile In;
		At<ThisFn>(0x4df690)(Original, NULL);
		In.open(NameOurs.c_str());
		At<ThisStrFn>(0x4e1040)(Original, NULL, NameOurs.c_str());
		CHECK(strcmp(In.Filename, ((TReadBinaryFile *)Original)->Filename) == 0, "TReadBinaryFile open");
		for (int i = 0; i < n; i++) {
			if (Random() % 8 == 0) {
				bool a = In.eof();
				bool b = At<bool (__fastcall *)(void *, void *)>(0x4e15e0)(Original, NULL);
				CHECK(a == b, "eof %d %d", a, b);
			}
			switch (Kinds[i]) {
			case 0: {
				uint8 a = In.readByte(), b = At<uint8 (__fastcall *)(void *, void *)>(0x4e1740)(Original, NULL);
				CHECK(a == b && a == (uint8)Values[i], "readByte %d %d", a, b);
				break;
			}
			case 1: {
				int a = In.readWord(), b = At<ThisRetIntFn>(0x4e1a00)(Original, NULL);
				CHECK(a == b && a == (uint16)Values[i], "readWord %d %d", a, b);
				break;
			}
			case 2: {
				int a = In.readQuad(), b = At<ThisRetIntFn>(0x4e1ba0)(Original, NULL);
				CHECK(a == b && (uint32)a == Values[i], "readQuad %d %d", a, b);
				break;
			}
			case 3: {
				char *a = In.readString();
				char *b = At<ThisRetStrFn>(0x4e3a10)(Original, NULL);
				CHECK(strcmp(a, b) == 0 && Strings[i] == a, "readString");
				delete[] a;
				free(b);
				break;
			}
			default: {
				uint8 a[64], b[64];
				int Count = (int)Strings[i].size();
				In.readBytes(a, Count);
				At<ThisBytesFn>(0x4e1e50)(Original, NULL, b, Count);
				CHECK(memcmp(a, b, Count) == 0, "readBytes");
				break;
			}
			}
		}
		bool a = In.eof();
		bool b = At<bool (__fastcall *)(void *, void *)>(0x4e15e0)(Original, NULL);
		CHECK(a == b, "eof at the end %d %d", a, b);
		int Offset = A.empty() ? 0 : (int)(Random() % A.size());
		In.seek(Offset);
		At<ThisIntFn>(0x4e1320)(Original, NULL, Offset);
		if (!A.empty()) {
			uint8 x = In.readByte(), y = At<uint8 (__fastcall *)(void *, void *)>(0x4e1740)(Original, NULL);
			CHECK(x == y && x == (uint8)A[Offset], "seek %d", Offset);
		}
		In.close();
		At<ThisFn>(0x4df6b0)(Original, NULL);
		CHECK(((TReadBinaryFile *)Original)->File == NULL, "close");
	}
}

// A random patch that never reads past the end of the old file.
static std::string RandomPatch(int OldSize, std::string &Expected, const std::string &Old)
{
	std::string Patch;
	int Pos = 0;
	Expected.clear();
	int n = Random() % 30;
	for (int i = 0; i < n; i++) {
		int Op = (Random() % 4) << 6;
		int Count;
		switch (Random() % 5) {
		case 0: Count = Random() % 61; break;
		case 1: Count = Random() % 200; break;
		case 2: Count = Random() % 1000; break;
		default: Count = Random() % 20; break;
		}
		if ((Op == 0x00 || Op == 0x40 || Op == 0xc0) && Count > OldSize - Pos)
			Count = OldSize - Pos;
		int Form = Count < 61 ? (int)(Random() % 4) : 1 + (int)(Random() % 3);
		if (Count > 255 && Form == 1)
			Form = 2;
		switch (Form) {
		case 0: Patch += (char)(Op | Count); break;
		case 1: Patch += (char)(Op | 61); Patch += (char)Count; break;
		case 2: Patch += (char)(Op | 62); Patch += (char)Count; Patch += (char)(Count >> 8); break;
		default: Patch += (char)(Op | 63); Patch += (char)Count; Patch += (char)(Count >> 8); Patch += (char)0; Patch += (char)0; break;
		}
		for (int j = 0; j < Count; j++) {
			char c = (char)Random();
			switch (Op) {
			case 0x00: Expected += Old[Pos++]; break;
			case 0x40: Pos++; break;
			case 0x80: Patch += c; Expected += c; break;
			default: Patch += c; Expected += c; Pos++; break;
			}
		}
	}
	return Patch;
}

static void TestPatches()
{
	typedef bool (__cdecl *PatchFn)(const char *, const uint8 *, int, uint32);
	typedef bool (__cdecl *FullPatchFn)(const char *, const uint8 *, int);
	typedef void (__cdecl *InstallFn)(const char *);
	std::string NameOurs = TempPath("ours.dat"), NameOriginal = TempPath("original.dat");
	std::string PatchedOurs = TempPath("ours.da_"), PatchedOriginal = TempPath("original.da_");
	for (int Round = 0; Round < 500; Round++) {
		std::string Old;
		int OldSize = 4 + Random() % 3000;
		for (int i = 0; i < OldSize; i++)
			Old += (char)Random();
		uint32 Signature = (uint8)Old[0] | ((uint8)Old[1] << 8) | ((uint8)Old[2] << 16) | ((uint32)(uint8)Old[3] << 24);
		WriteFileText(NameOurs, Old);
		WriteFileText(NameOriginal, Old);
		std::string Expected;
		std::string Patch = RandomPatch(OldSize, Expected, Old);
		uint32 Check;
		switch (Random() % 3) {
		case 0: Check = 0; break;
		case 1: Check = Signature; break;
		default: Check = Random(); break;
		}
		bool a, b;
		if (Random() % 4 == 0) {
			a = ApplyFullPatch(NameOurs.c_str(), (const uint8 *)Patch.data(), (int)Patch.size());
			b = At<FullPatchFn>(0x4e3500)(NameOriginal.c_str(), (const uint8 *)Patch.data(), (int)Patch.size());
			Expected = Patch;
		} else {
			a = ApplyPatch(NameOurs.c_str(), (const uint8 *)Patch.data(), (int)Patch.size(), Check);
			b = At<PatchFn>(0x4e3100)(NameOriginal.c_str(), (const uint8 *)Patch.data(), (int)Patch.size(), Check);
			if (Check != 0 && Check != Signature)
				Expected = "";
		}
		CHECK(a == b, "patch result %d %d", a, b);
		std::string A, B;
		bool HaveA = ReadFileText(PatchedOurs, A), HaveB = ReadFileText(PatchedOriginal, B);
		CHECK(HaveA == HaveB && A == B, "patched files differ (%d and %d bytes)", (int)A.size(), (int)B.size());
		if (a && b) {
			CHECK(A == Expected, "patched file is not the expected one");
			InstallPatch(NameOurs.c_str());
			At<InstallFn>(0x4e3610)(NameOriginal.c_str());
			HaveA = ReadFileText(NameOurs, A);
			HaveB = ReadFileText(NameOriginal, B);
			CHECK(HaveA && HaveB && A == B && A == Expected, "installed files differ");
			CHECK(_access(PatchedOurs.c_str(), 0) != 0 && _access(PatchedOriginal.c_str(), 0) != 0, "patched file left behind");
		}
		remove(PatchedOurs.c_str());
		remove(PatchedOriginal.c_str());
	}
	remove(NameOurs.c_str());
	remove(NameOriginal.c_str());
}

// --- Length limits (issue #24) ---------------------------------------------

// Its own random numbers, so that the later tests' inputs stay the same.
static uint32 LongSeed = 24;
static uint32 LongRandom()
{
	LongSeed = LongSeed * 1103515245 + 12345;
	return LongSeed >> 8;
}

// nextToken's limits are compared with Size at the end of the loop body: the
// identifier state's `cmp ebx,0x14` (0x4dfde1) right after its getc, and
// `cmp [esp+0x10],0x3e7` (0x4e0203), the target of every state's break.
// Identifiers of 19 characters and strings of 998 are the longest the image
// accepts: both sides read them; one more must raise our error (the image
// cannot throw in the child, so that case runs on our side only).
static std::string LongToken(bool String, int Length)
{
	static const char Plain[] = "abcXYZ019 .,;:!?()[]{}<>=-#'\t";
	static const char Alpha[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static const char Rest[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
	if (!String) {
		std::string s(1, Alpha[LongRandom() % (sizeof(Alpha) - 1)]);
		while ((int)s.size() < Length)
			s += Rest[LongRandom() % (sizeof(Rest) - 1)];
		return s;
	}
	std::string s = "\"";
	for (int i = 0; i < Length; i++) { // every element stores one character
		switch (LongRandom() % 10) {
		case 0: s += "\\\""; break;
		case 1: s += "\\\\"; break;
		case 2: s += "\\n"; break;
		default: s += Plain[LongRandom() % (sizeof(Plain) - 1)]; break;
		}
	}
	return s + "\"";
}

static void TestLongTokens()
{
	static uint8 Original[sizeof(TReadScriptFile)];
	CHECK(*(const uint8 *)0x4dfde3 == MAX_IDENT_LENGTH, "identifier limit 0x%x", *(const uint8 *)0x4dfde3);
	CHECK(*(const int *)0x4e0207 == MAX_STRING_LENGTH - 1, "string limit 0x%x", *(const int *)0x4e0207);
	std::string Name = TempPath("script.txt");
	for (int Round = 0; Round < 200; Round++) {
		// Tokens at the limits, between short ones; the last one is the
		// longest accepted string right at the end of the file.
		std::string Text;
		int n = 1 + LongRandom() % 6;
		for (int i = 0; i < n; i++) {
			bool String = LongRandom() % 2 != 0;
			int Length = String ? MAX_STRING_LENGTH - 2 - LongRandom() % 3 : MAX_IDENT_LENGTH - 1 - LongRandom() % 2;
			Text += LongToken(String, Length);
			Text += LongRandom() % 2 ? " 7 " : "\n";
		}
		Text += LongToken(true, MAX_STRING_LENGTH - 2);
		WriteFileText(Name, Text);
		memset(Original, 0x55, sizeof(Original));
		At<ThisFn>(0x4df550)(Original, NULL);
		TReadScriptFile Ours;
		Ours.open(Name.c_str());
		At<ThisStrFn>(0x4df6d0)(Original, NULL, Name.c_str());
		for (int i = 0; i < 40; i++) {
			try {
				Ours.nextToken();
			} catch (TMyException &E) {
				printf("FAIL Script (long tokens, round %d, token %d): our side raised %s\n", Round, i,
					E.GetMessage().c_str());
				exit(1);
			}
			At<ThisFn>(0x4dfb60)(Original, NULL);
			CompareReader(Ours, Original, "long tokens", i);
			if (Ours.Token != ((TReadScriptFile *)Original)->Token || Ours.Token == TOKEN::ENDOFFILE)
				break;
		}
		Ours.close();
		At<ThisFn>(0x4df5d0)(Original, NULL);
		At<ThisFn>(0x4df5b0)(Original, NULL);

		// One character more: our side only.
		bool String = Round % 2 != 0;
		WriteFileText(Name, "x " + LongToken(String, String ? MAX_STRING_LENGTH - 1 : MAX_IDENT_LENGTH) + " y");
		TReadScriptFile Over;
		Over.open(Name.c_str());
		Over.nextToken();
		bool Raised = false;
		try {
			Over.nextToken();
		} catch (...) {
			Raised = true;
		}
		CHECK(Raised, "%s one character over the limit accepted", String ? "string" : "identifier");
		if (!Raised)
			Over.close();
	}
}

void TestScript()
{
	PatchScript();
	char Help[MAX_PATH];
	GetTempPathA(MAX_PATH, Help);
	TempDir = std::string(Help) + "tibia-difftest\\";
	CreateDirectoryA(TempDir.c_str(), NULL);
	TestReader();
	TestLongTokens();
	TestWriter();
	TestBinary();
	TestPatches();
	remove(TempPath("script.txt").c_str());
	remove(TempPath("ours.txt").c_str());
	remove(TempPath("original.txt").c_str());
	remove(TempPath("ours.bin").c_str());
	remove(TempPath("original.bin").c_str());
	RemoveDirectoryA(TempDir.c_str());
}
