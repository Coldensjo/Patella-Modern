// difftest.exe's Script test (moved here from tools/difftest/scripttest.cpp, issue #26): Script.cpp
// against the original, as a golden-corpus test (golden.h).
//
// In the oracle the image's stdio functions are redirected to ours (both sides then share one CRT and
// one kind of FILE), as are remove, rename and the file-exists check, so the original reads and writes
// real files in a temporary directory. Inputs are generated so that no error is raised: the error
// paths would need the original's exception machinery.
//
// The original's objects (TReadScriptFile 0x80c bytes, TWriteScriptFile 0x408, TReadBinaryFile and
// TWriteBinaryFile 0x404: the oracle's own classes have the original's layout, asserted below) are
// recorded by field: the numbers, whether File is NULL, where Bytes points (as an offset from String),
// and the String and Filename arrays by their changes from the previous record (golden::OutputChanges).
// A whole-object memcmp of the oracle's test is the same comparison field by field (File is NULL on
// both sides wherever it was compared). The files the original writes are recorded by their bytes.

#include "golden_test.h"

#include "Script.h"

#include <io.h>
#include <stdarg.h>
#include <string>
#include <vector>

// The reconstructed side's external function (sysinc.cpp).
bool FileExists(const char *FileName) { return _access(FileName, 0) == 0; }

#if GOLDEN_ORACLE
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
typedef void(__fastcall *ThisFn)(void *This, void *);
typedef void(__fastcall *ThisStrFn)(void *This, void *, const char *);
typedef void(__fastcall *ThisIntFn)(void *This, void *, int);
typedef int(__fastcall *ThisRetIntFn)(void *This, void *);
typedef char *(__fastcall *ThisRetStrFn)(void *This, void *);
typedef char(__fastcall *ThisRetCharFn)(void *This, void *);
typedef void(__fastcall *ThisCharFn)(void *This, void *, char);
typedef void(__fastcall *ThisBytesFn)(void *This, void *, const void *, int);

// The oracle's classes have the original's layouts (the runner's do not: FILE * and Bytes are 8 bytes).
static_assert(sizeof(TReadScriptFile) == 0x80c, "TReadScriptFile layout");
static_assert(sizeof(TWriteScriptFile) == 0x408, "TWriteScriptFile layout");
static_assert(sizeof(TReadBinaryFile) == 0x404, "TReadBinaryFile layout");
#endif

namespace {

std::string TempDir;

std::string TempPath(const char *Name)
{
	return TempDir + Name;
}

void WriteFileText(const std::string &Name, const std::string &Text)
{
	FILE *File = fopen(Name.c_str(), "wb");
	fwrite(Text.data(), 1, Text.size(), File);
	fclose(File);
}

bool ReadFileText(const std::string &Name, std::string &Text)
{
	FILE *File = fopen(Name.c_str(), "rb");
	if (File == nullptr)
		return false;
	Text.clear();
	int c;
	while ((c = getc(File)) != EOF)
		Text += (char)c;
	fclose(File);
	return true;
}

// A file the original wrote, by value (the oracle reads it).
bool OriginalFileText(const char *Name, [[maybe_unused]] const std::string &FileName, std::string &Text)
{
	uint8 Have = 0;
	std::string Read;
	ORIGINAL(Have = ReadFileText(FileName, Read));
	golden::Output(Name, Have);
	if (Have != 0)
		Text = golden::OutputText(Name, Read.data(), Read.size());
	return Have != 0;
}

// --- The original's objects, by field --------------------------------------------

#if GOLDEN_ORACLE
// The original's TReadScriptFile (TestReader, TestLongTokens), TWriteScriptFile (TestWriter) and
// TReadBinaryFile / TWriteBinaryFile (TestBinary).
alignas(4) uint8 Original[0x80c];
#endif

// A file name of the original. The test's files are in a directory of TEMP, which differs between
// the recording and the replay: a name in it is recorded as TEMP_DIR_MARK and the rest, and the runner
// puts its own directory back. The name is the text up to the 0 (the bytes after it are 0 wherever a
// whole object was compared: after the constructor and close).
const char TEMP_DIR_MARK = 1;

struct TFilename {
	char Recorded[1024]; // as recorded the time before (golden::OutputChanges)
	char Name[1024];     // with the directory
};

void OriginalFilename(const char *What, TFilename &Filename, [[maybe_unused]] const char *Name)
{
	char Value[1024] = {};
#if GOLDEN_ORACLE
	size_t Length = strnlen(Name, sizeof(Value) - 1);
	if (!TempDir.empty() && strncmp(Name, TempDir.c_str(), TempDir.size()) == 0) {
		Value[0] = TEMP_DIR_MARK;
		memcpy(Value + 1, Name + TempDir.size(), Length - TempDir.size());
	} else
		memcpy(Value, Name, Length);
#endif
	golden::OutputChanges(What, Filename.Recorded, Value, sizeof(Value));
	memset(Filename.Name, 0, sizeof(Filename.Name));
	if (Filename.Recorded[0] == TEMP_DIR_MARK)
		snprintf(Filename.Name, sizeof(Filename.Name), "%s%s", TempDir.c_str(), Filename.Recorded + 1);
	else
		memcpy(Filename.Name, Filename.Recorded, sizeof(Filename.Name));
}

// The original's TReadScriptFile as last recorded.
struct TReaderState {
	int Token, Line, Number, CoordX, CoordY, CoordZ, Special, FileNull, BytesFromString;
	char String[MAX_STRING_LENGTH];
	TFilename Filename;
} Reader;

const TReaderState &OriginalReader()
{
	int Fields[9] = {};
#if GOLDEN_ORACLE
	const TReadScriptFile &O = *(const TReadScriptFile *)Original;
	Fields[0] = (int)O.Token;
	Fields[1] = O.Line;
	Fields[2] = O.Number;
	Fields[3] = O.CoordX;
	Fields[4] = O.CoordY;
	Fields[5] = O.CoordZ;
	Fields[6] = O.Special;
	Fields[7] = O.File == NULL;
	Fields[8] = (int)(O.Bytes - (const uint8 *)O.String.Chars);
#endif
	golden::Output("reader", Fields);
	Reader.Token = Fields[0];
	Reader.Line = Fields[1];
	Reader.Number = Fields[2];
	Reader.CoordX = Fields[3];
	Reader.CoordY = Fields[4];
	Reader.CoordZ = Fields[5];
	Reader.Special = Fields[6];
	Reader.FileNull = Fields[7];
	Reader.BytesFromString = Fields[8];
	golden::OutputChanges("reader string", Reader.String,
		ORIGINAL_OR(((const TReadScriptFile *)Original)->String, nullptr), sizeof(Reader.String));
	OriginalFilename("reader file name", Reader.Filename, ORIGINAL_OR(((const TReadScriptFile *)Original)->Filename, nullptr));
	return Reader;
}

// The original's TWriteScriptFile, TReadBinaryFile and TWriteBinaryFile: File, Filename, the
// script writer's Line.
struct TFileState {
	int FileNull, Line;
	TFilename Filename;
} Writer, Binary;

const TFileState &OriginalFile(TFileState &State, const char *Name, [[maybe_unused]] bool Script)
{
	int Fields[2] = {};
#if GOLDEN_ORACLE
	if (Script) {
		const TWriteScriptFile &O = *(const TWriteScriptFile *)Original;
		Fields[0] = O.File == NULL;
		Fields[1] = O.Line;
	} else
		Fields[0] = ((const TReadBinaryFile *)Original)->File == NULL;
#endif
	golden::Output(Name, Fields);
	State.FileNull = Fields[0];
	State.Line = Fields[1];
	OriginalFilename(Name, State.Filename,
		ORIGINAL_OR(Script ? ((const TWriteScriptFile *)Original)->Filename : ((const TReadBinaryFile *)Original)->Filename,
			nullptr));
	return State;
}

// The offset of a pointer the original returned into its TReadScriptFile, from String.
int OriginalStringOffset(const char *Name, [[maybe_unused]] const char *Pointer)
{
	return ORIGINAL_VALUE(Name, int, Pointer - ((const TReadScriptFile *)Original)->String);
}

// --- Script text generator (never raises an error) -------------------------

std::string RandomIdentifier(bool Ascii = false)
{
	static const char First[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\xe4\xf6\xfc\xc4\xd6\xdc";
	static const char Rest[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_\xdf";
	std::string s(1, First[Random() % (sizeof(First) - (Ascii ? 7 : 1))]);
	int Length = Random() % 19;
	for (int i = 0; i < Length; i++)
		s += Rest[Random() % (sizeof(Rest) - (Ascii ? 2 : 1))];
	return s;
}

std::string RandomNumberText()
{
	char Help[16];
	switch (Random() % 3) {
	case 0: sprintf(Help, "%u", Random() % 10); break;
	case 1: sprintf(Help, "%u", Random() % 100000); break;
	default: sprintf(Help, "%u", Random() % 2000000000); break;
	}
	return Help;
}

std::string RandomToken()
{
	switch (Random() % 14) {
	case 0:
	case 1: return RandomIdentifier();
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
	case 6: {
		// Issue #55: operator+'s arguments right to left, as the oracle's.
		const std::string C = RandomNumberText(), B = RandomNumberText(), A = RandomNumberText();
		return "[" + A + "," + B + "," + C + "]";
	}
	case 7: {
		static const char *S[] = {"<=", ">=", "<>", "->", "<", ">", "-", "[", "="};
		return S[Random() % 9];
	}
	case 8: {
		static const char S[] = "=(),{}:;.+*/!?&|%$@~^'";
		return std::string(1, S[Random() % (sizeof(S) - 1)]);
	}
	case 9: return "# comment " + RandomIdentifier() + "\n";
	case 10: return "\n";
	case 11: return "\t";
	case 12: return "\r\n";
	default: return "[x";
	}
}

std::string RandomScript()
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

// --- Tests --------------------------------------------------------------------

void CompareReader(const TReadScriptFile &Ours, const char *What, int Index)
{
	const TReaderState &O = OriginalReader();
	bool Same = (int)Ours.Token == O.Token && Ours.Line == O.Line && Ours.Number == O.Number
		&& Ours.CoordX == O.CoordX && Ours.CoordY == O.CoordY && Ours.CoordZ == O.CoordZ
		&& Ours.Special == (char)O.Special && memcmp(Ours.String, O.String, sizeof(Ours.String)) == 0
		&& strcmp(Ours.Filename, O.Filename.Name) == 0
		&& (Ours.File == nullptr) == (O.FileNull != 0)
		&& (const uint8 *)Ours.Bytes - (const uint8 *)Ours.String.Chars == O.BytesFromString;
	CHECK(Same, "%s, token %d: ours %d '%s' %d [%d,%d,%d] '%c' line %d, original %d '%s' %d [%d,%d,%d] '%c' line %d",
		What, Index, (int)Ours.Token, Ours.String.Chars, Ours.Number, Ours.CoordX, Ours.CoordY, Ours.CoordZ, Ours.Special, Ours.Line,
		O.Token, O.String, O.Number, O.CoordX, O.CoordY, O.CoordZ, (char)O.Special, O.Line);
}

void TestReader()
{
	std::string Name = TempPath("script.txt");
	for (int Round = 0; Round < 3000; Round++) {
		WriteFileText(Name, RandomScript());
		ORIGINAL(memset(Original, 0x55, sizeof(Original)));
		ORIGINAL(At<ThisFn>(0x4df550)(Original, NULL));
		TReadScriptFile Ours;
		Ours.Token = (TOKEN)0x55555555; // not set by the constructor
		CompareReader(Ours, "constructor", 0);
		Ours.open(Name.c_str());
		ORIGINAL(At<ThisStrFn>(0x4df6d0)(Original, NULL, Name.c_str()));
		CompareReader(Ours, "open", 0);
		for (int i = 0; i < 200; i++) {
			// Mix the plain tokenizer with the read helpers where the next
			// token is known to fit.
			Ours.nextToken();

			ORIGINAL(At<ThisFn>(0x4dfb60)(Original, NULL));
			CompareReader(Ours, "nextToken", i);
			if ((int)Ours.Token != Reader.Token)
				break;
			if (Ours.Token == TOKEN::ENDOFFILE)
				break;
			if (Ours.Token == TOKEN::IDENTIFIER) {
				char *a = Ours.getIdentifier();
				const char *b = nullptr;
				ORIGINAL(b = At<ThisRetStrFn>(0x4e0420)(Original, NULL));
				int Offset = OriginalStringOffset("getIdentifier offset", b);
				golden::TText Text = ORIGINAL_TEXT("getIdentifier", b);
				CHECK(a - Ours.String == Offset && strcmp(a, Text.c_str()) == 0, "getIdentifier '%s' '%s'", a,
					Text.c_str());
			} else if (Ours.Token == TOKEN::NUMBER) {
				int a = Ours.getNumber();
				int b = ORIGINAL_VALUE("getNumber", int, At<ThisRetIntFn>(0x4e0450)(Original, NULL));
				CHECK(a == b, "getNumber %d %d", a, b);
			} else if (Ours.Token == TOKEN::STRING) {
				char *a = Ours.getString();
				const char *b = nullptr;
				ORIGINAL(b = At<ThisRetStrFn>(0x4e0470)(Original, NULL));
				int Offset = OriginalStringOffset("getString offset", b);
				CHECK(a - Ours.String == Offset, "getString");
			} else if (Ours.Token == TOKEN::SPECIAL) {
				char a = Ours.getSpecial();
				char b = ORIGINAL_VALUE("getSpecial", char, At<ThisRetCharFn>(0x4e0490)(Original, NULL));
				CHECK(a == b, "getSpecial %d %d", a, b);
			}
			CompareReader(Ours, "get", i);
		}
		Ours.close();
		ORIGINAL(At<ThisFn>(0x4df5d0)(Original, NULL));
		CompareReader(Ours, "close", 0);
		ORIGINAL(At<ThisFn>(0x4df5b0)(Original, NULL));
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
			case 1: {
				const std::string Number = RandomNumberText(); // issue #55: operator+'s arguments right to left
				Text += (Random() % 2 ? "-" : "") + Number;
				break;
			}
			case 2: {
				std::string s = RandomToken();
				while (s[0] != '"')
					s = RandomToken();
				Text += s;
				break;
			}
			default: Text += Symbols[i]; break;
			}
			Text += Random() % 4 ? " " : "\n";
		}
		WriteFileText(Name, Text);
		ORIGINAL(At<ThisFn>(0x4df550)(Original, NULL));
		TReadScriptFile Ours;
		Ours.open(Name.c_str());
		ORIGINAL(At<ThisStrFn>(0x4df6d0)(Original, NULL, Name.c_str()));
		for (int i = 0; i < n; i++) {
			switch (Kinds[i]) {
			case 0: {
				char *a = Ours.readIdentifier();
				golden::TText b = ORIGINAL_TEXT("readIdentifier", At<ThisRetStrFn>(0x4e3920)(Original, NULL));
				CHECK(strcmp(a, b.c_str()) == 0, "readIdentifier '%s' '%s'", a, b.c_str());
				break;
			}
			case 1: {
				int a = Ours.readNumber();
				int b = ORIGINAL_VALUE("readNumber", int, At<ThisRetIntFn>(0x4e3950)(Original, NULL));
				CHECK(a == b, "readNumber %d %d", a, b);
				break;
			}
			case 2: {
				char *a = Ours.readString();
				golden::TText b = ORIGINAL_TEXT("readString", At<ThisRetStrFn>(0x4e39a0)(Original, NULL));
				CHECK(strcmp(a, b.c_str()) == 0, "readString");
				break;
			}
			default: {
				Ours.readSymbol(Symbols[i]);
				ORIGINAL(At<ThisCharFn>(0x4e39d0)(Original, NULL, Symbols[i]));
				break;
			}
			}
			CompareReader(Ours, "read", i);
		}
		ORIGINAL(At<ThisFn>(0x4df5b0)(Original, NULL));
	}
}

void TestWriter()
{
	std::string NameOurs = TempPath("ours.txt"), NameOriginal = TempPath("original.txt");
	for (int Round = 0; Round < 2000; Round++) {
		TWriteScriptFile Ours;
		ORIGINAL(At<ThisFn>(0x4df640)(Original, NULL));
		const TFileState &Constructed = OriginalFile(Writer, "script writer", true);
		CHECK(Ours.File == nullptr && Constructed.FileNull != 0 && Ours.Line == Constructed.Line
				&& memcmp(Ours.Filename, Constructed.Filename.Name, sizeof(Ours.Filename)) == 0,
			"TWriteScriptFile constructor");
		Ours.open(NameOurs.c_str());
		ORIGINAL(At<ThisStrFn>(0x4e04b0)(Original, NULL, NameOriginal.c_str()));
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
			case 0:
				Ours.writeLn();
				ORIGINAL(At<ThisFn>(0x4e0790)(Original, NULL));
				break;
			case 1:
				Ours.writeText(Text);
				ORIGINAL(At<ThisStrFn>(0x4e08f0)(Original, NULL, Text));
				break;
			case 2:
				Ours.writeString(Text);
				ORIGINAL(At<ThisStrFn>(0x4e0bb0)(Original, NULL, Text));
				break;
			default: {
				int Number = (int)Random();
				if (Random() % 2)
					Number %= 1000;
				Ours.writeNumber(Number);
				ORIGINAL(At<ThisIntFn>(0x4e0ec0)(Original, NULL, Number));
				break;
			}
			}
		}
		int Line = ORIGINAL_VALUE("script writer line", int, ((TWriteScriptFile *)Original)->Line);
		CHECK(Ours.Line == Line, "TWriteScriptFile Line");
		Ours.close();
		ORIGINAL(At<ThisFn>(0x4df660)(Original, NULL));
		const TFileState &Closed = OriginalFile(Writer, "script writer", true);
		CHECK(Ours.File == nullptr && Closed.FileNull != 0 && Ours.Line == Closed.Line
				&& memcmp(Ours.Filename, Closed.Filename.Name, sizeof(Ours.Filename)) == 0,
			"TWriteScriptFile close");
		std::string A, B;
		ReadFileText(NameOurs, A);
		OriginalFileText("script writer output", NameOriginal, B);
		CHECK(A == B, "TWriteScriptFile output differs (%d and %d bytes)", (int)A.size(), (int)B.size());
	}
}

void TestBinary()
{
#if GOLDEN_ORACLE
	typedef uint8(__fastcall * ReadByteFn)(void *, void *);
	typedef bool(__fastcall * EofFn)(void *, void *);
#endif
	std::string NameOurs = TempPath("ours.bin"), NameOriginal = TempPath("original.bin");
	for (int Round = 0; Round < 2000; Round++) {
		// Writing.
		TWriteBinaryFile Ours;
		ORIGINAL(memset(Original, 0x55, 0x404));
		ORIGINAL(At<ThisFn>(0x4df690)(Original, NULL));
		const TFileState &Constructed = OriginalFile(Binary, "binary file", false);
		CHECK(Ours.File == nullptr && Constructed.FileNull != 0
				&& memcmp(Ours.Filename, Constructed.Filename.Name, sizeof(Ours.Filename)) == 0,
			"TWriteBinaryFile constructor");
		Ours.open(NameOurs.c_str());
		ORIGINAL(At<ThisStrFn>(0x4e21a0)(Original, NULL, NameOriginal.c_str()));
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
			case 0:
				Ours.writeByte((uint8)Value);
				ORIGINAL(At<ThisIntFn>(0x4e2480)(Original, NULL, (uint8)Value));
				break;
			case 1:
				Ours.writeWord((uint16)Value);
				ORIGINAL(At<ThisIntFn>(0x4e2750)(Original, NULL, (uint16)Value));
				break;
			case 2:
				Ours.writeQuad(Value);
				ORIGINAL(At<ThisIntFn>(0x4e29e0)(Original, NULL, (int)Value));
				break;
			case 3: {
				int Length = Random() % 50;
				for (int j = 0; j < Length; j++)
					Text += (char)(1 + Random() % 255);
				Ours.writeString(Text.c_str());
				ORIGINAL(At<ThisStrFn>(0x4e2f50)(Original, NULL, Text.c_str()));
				break;
			}
			default: {
				int Length = 1 + Random() % 50;
				for (int j = 0; j < Length; j++)
					Text += (char)Random();
				Ours.writeBytes(OriginalSpan((const uint8 *)Text.data(), Length));
				ORIGINAL(At<ThisBytesFn>(0x4e2c80)(Original, NULL, Text.data(), Length));
				break;
			}
			}
			Strings.push_back(Text);
		}
		Ours.close();
		ORIGINAL(At<ThisFn>(0x4df6b0)(Original, NULL));
		std::string A, B;
		ReadFileText(NameOurs, A);
		OriginalFileText("binary writer output", NameOriginal, B);
		CHECK(A == B, "TWriteBinaryFile output differs (%d and %d bytes)", (int)A.size(), (int)B.size());

		// Reading the same file back on both sides.
		TReadBinaryFile In;
		ORIGINAL(At<ThisFn>(0x4df690)(Original, NULL));
		In.open(NameOurs.c_str());
		ORIGINAL(At<ThisStrFn>(0x4e1040)(Original, NULL, NameOurs.c_str()));
		const TFileState &Opened = OriginalFile(Binary, "binary file", false);
		CHECK(strcmp(In.Filename, Opened.Filename.Name) == 0, "TReadBinaryFile open");
		for (int i = 0; i < n; i++) {
			if (Random() % 8 == 0) {
				bool a = In.eof();
				bool b = ORIGINAL_VALUE("eof", bool, At<EofFn>(0x4e15e0)(Original, NULL));
				CHECK(a == b, "eof %d %d", a, b);
			}
			switch (Kinds[i]) {
			case 0: {
				uint8 a = In.readByte();
				uint8 b = ORIGINAL_VALUE("readByte", uint8, At<ReadByteFn>(0x4e1740)(Original, NULL));
				CHECK(a == b && a == (uint8)Values[i], "readByte %d %d", a, b);
				break;
			}
			case 1: {
				int a = In.readWord();
				int b = ORIGINAL_VALUE("readWord", int, At<ThisRetIntFn>(0x4e1a00)(Original, NULL));
				CHECK(a == b && a == (uint16)Values[i], "readWord %d %d", a, b);
				break;
			}
			case 2: {
				int a = In.readQuad();
				int b = ORIGINAL_VALUE("readQuad", int, At<ThisRetIntFn>(0x4e1ba0)(Original, NULL));
				CHECK(a == b && (uint32)a == Values[i], "readQuad %d %d", a, b);
				break;
			}
			case 3: {
				char *a = In.readString();
				golden::TText b;
				ORIGINAL(char *Read = At<ThisRetStrFn>(0x4e3a10)(Original, NULL));
				b = ORIGINAL_TEXT("readString", Read);
				ORIGINAL(free(Read));
				CHECK(strcmp(a, b.c_str()) == 0 && Strings[i] == a, "readString");
				delete[] a;
				break;
			}
			default: {
				uint8 a[64], b[64] = {};
				int Count = (int)Strings[i].size();
				In.readBytes(OriginalSpan(a, Count));
				ORIGINAL(At<ThisBytesFn>(0x4e1e50)(Original, NULL, b, Count));
				golden::OutputArray("readBytes", b, (size_t)Count);
				CHECK(memcmp(a, b, Count) == 0, "readBytes");
				break;
			}
			}
		}
		bool a = In.eof();
		bool b = ORIGINAL_VALUE("eof at the end", bool, At<EofFn>(0x4e15e0)(Original, NULL));
		CHECK(a == b, "eof at the end %d %d", a, b);
		int Offset = A.empty() ? 0 : (int)(Random() % A.size());
		In.seek(Offset);
		ORIGINAL(At<ThisIntFn>(0x4e1320)(Original, NULL, Offset));
		if (!A.empty()) {
			uint8 x = In.readByte();
			uint8 y = ORIGINAL_VALUE("readByte after seek", uint8, At<ReadByteFn>(0x4e1740)(Original, NULL));
			CHECK(x == y && x == (uint8)A[Offset], "seek %d", Offset);
		}
		In.close();
		ORIGINAL(At<ThisFn>(0x4df6b0)(Original, NULL));
		const TFileState &Closed = OriginalFile(Binary, "binary file", false);
		CHECK(Closed.FileNull != 0, "close");
	}
}

// A random patch that never reads past the end of the old file.
std::string RandomPatch(int OldSize, std::string &Expected, const std::string &Old)
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
		case 1:
			Patch += (char)(Op | 61);
			Patch += (char)Count;
			break;
		case 2:
			Patch += (char)(Op | 62);
			Patch += (char)Count;
			Patch += (char)(Count >> 8);
			break;
		default:
			Patch += (char)(Op | 63);
			Patch += (char)Count;
			Patch += (char)(Count >> 8);
			Patch += (char)0;
			Patch += (char)0;
			break;
		}
		for (int j = 0; j < Count; j++) {
			char c = (char)Random();
			switch (Op) {
			case 0x00: Expected += Old[Pos++]; break;
			case 0x40: Pos++; break;
			case 0x80:
				Patch += c;
				Expected += c;
				break;
			default:
				Patch += c;
				Expected += c;
				Pos++;
				break;
			}
		}
	}
	return Patch;
}

// ApplyPatch, ApplyFullPatch and InstallPatch are gone with the patcher (Patella-Modern issue #75, decision D9), and
// their 500 rounds with them. Their random numbers are still drawn, as they were, so that the later tests keep their
// inputs.
void SkipPatches()
{
	for (int Round = 0; Round < 500; Round++) {
		std::string Old;
		int OldSize = 4 + Random() % 3000;
		for (int i = 0; i < OldSize; i++)
			Old += (char)Random();
		std::string Expected;
		RandomPatch(OldSize, Expected, Old);
		if (Random() % 3 == 2)
			Random(); // the wrong signature
		Random();     // the whole file or a difference patch
	}
}

// --- Length limits (issue #24) ---------------------------------------------

// Its own random numbers, so that the later tests' inputs stay the same.
uint32 LongSeed = 24;
uint32 LongRandom()
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
std::string LongToken(bool String, int Length)
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

void TestLongTokens()
{
	uint8 IdentifierLimit = ORIGINAL_VALUE("identifier limit", uint8, *(const uint8 *)0x4dfde3);
	CHECK(IdentifierLimit == MAX_IDENT_LENGTH, "identifier limit 0x%x", IdentifierLimit);
	int StringLimit = ORIGINAL_VALUE("string limit", int, *(const int *)0x4e0207);
	CHECK(StringLimit == MAX_STRING_LENGTH - 1, "string limit 0x%x", StringLimit);
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
		ORIGINAL(memset(Original, 0x55, sizeof(Original)));
		ORIGINAL(At<ThisFn>(0x4df550)(Original, NULL));
		TReadScriptFile Ours;
		Ours.open(Name.c_str());
		ORIGINAL(At<ThisStrFn>(0x4df6d0)(Original, NULL, Name.c_str()));
		for (int i = 0; i < 40; i++) {
			try {
				Ours.nextToken();
			} catch (TMyException &E) {
				printf("FAIL Script (long tokens, round %d, token %d): our side raised %s\n", Round, i,
					E.GetMessage().c_str());
				exit(1);
			}
			ORIGINAL(At<ThisFn>(0x4dfb60)(Original, NULL));
			CompareReader(Ours, "long tokens", i);
			if ((int)Ours.Token != Reader.Token || Ours.Token == TOKEN::ENDOFFILE)
				break;
		}
		Ours.close();
		ORIGINAL(At<ThisFn>(0x4df5d0)(Original, NULL));
		ORIGINAL(At<ThisFn>(0x4df5b0)(Original, NULL));

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

} // namespace

void TestScript()
{
	ORIGINAL(PatchScript());
	char Help[MAX_PATH];
	GetTempPathA(MAX_PATH, Help);
	TempDir = std::string(Help) + "tibia-difftest\\";
	CreateDirectoryA(TempDir.c_str(), nullptr);
	TestReader();
	TestLongTokens();
	TestWriter();
	TestBinary();
	SkipPatches();
	remove(TempPath("script.txt").c_str());
	remove(TempPath("ours.txt").c_str());
	remove(TempPath("original.txt").c_str());
	remove(TempPath("ours.bin").c_str());
	remove(TempPath("original.bin").c_str());
	RemoveDirectoryA(TempDir.c_str());
}
