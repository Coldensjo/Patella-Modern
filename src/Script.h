// Script.h: reading and writing script files (Tibia.cfg) and binary files.
//
// The class, member and token names follow CipSoft's game server, whose
// TReadScriptFile has the same tokenizer (the binary names its state variable
// "State", as the server does). The client's version reads a single file (no
// #include) and has a smaller string buffer. The names are not in the client
// binary; see notes/decisions.md.
//
// The constructors, destructors and close() functions are defined here. The
// linker kept their out-of-line copies from Preferences.obj, the first object
// that uses them (0x4df550-0x4df6c9, after the Tibia.cfg loader and saver).
// The FILE * members are TFiles (types/File.h, Patella-Modern issue #64):
// fopen, fclose and the reads and writes with an owner, the same bytes.

#pragma once

#include "common.h"
#include "types/File.h"
#include <cstddef>
#include <cstring>
#include <type_traits>

inline constexpr int MAX_IDENT_LENGTH = 20;    // "identifier too long"
inline constexpr int MAX_STRING_LENGTH = 1000; // "string too long" at 999 characters

// The kind of the current token (TReadScriptFile::Token). Only assigned and
// compared, so it is scoped (issue #37); int as before, the member's 4 bytes.
enum class TOKEN : int {
	ENDOFFILE,
	IDENTIFIER,
	NUMBER,
	STRING,
	BYTES,      // "1-2-3"
	COORDINATE, // "[x,y,z]"
	SPECIAL,    // any other character; "<=" 'L', ">=" 'G', "<>" 'N', "->" 'I'
};
static_assert(sizeof(TOKEN) == 4);
static_assert(std::is_same_v<std::underlying_type_t<TOKEN>, int>);

// 0x80c bytes.
class TReadScriptFile {
public:
	TReadScriptFile(const TReadScriptFile &) = delete;
	TReadScriptFile &operator=(const TReadScriptFile &) = delete;
	// 0x4df550. Token is not set.
	TReadScriptFile()
	{
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
		memset(String, 0, sizeof(String));
		Bytes = reinterpret_cast<uint8 *>(String);
		Number = 0;
		CoordX = 0;
		CoordY = 0;
		CoordZ = 0;
		Special = '#';
	}

	// 0x4df5b0
	~TReadScriptFile()
	{
		File.Close();
	}

	// 0x4df5d0. Resets everything but Token, like the constructor.
	void close()
	{
		File.Close();
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
		memset(String, 0, sizeof(String));
		Number = 0;
		CoordX = 0;
		CoordY = 0;
		CoordZ = 0;
		Bytes = reinterpret_cast<uint8 *>(String);
		Special = '#';
	}

	void open(const char *FileName);
	void error(const char *Text);
	void nextToken();
	char *getIdentifier();
	int getNumber();
	char *getString();
	char getSpecial();
	char *readIdentifier();
	int readNumber();
	char *readString();
	void readSymbol(char Symbol);

	TOKEN Token;                    // +0x000
	TFile File;                     // +0x004
	char Filename[1024];            // +0x008
	int Line;                       // +0x408
	char String[MAX_STRING_LENGTH]; // +0x40c, identifier or string
	uint8 *Bytes;                   // +0x7f4, points to String
	int Number;                     // +0x7f8
	int CoordX;                     // +0x7fc
	int CoordY;                     // +0x800
	int CoordZ;                     // +0x804
	char Special;                   // +0x808
};

// 0x408 bytes.
class TWriteScriptFile {
public:
	TWriteScriptFile(const TWriteScriptFile &) = delete;
	TWriteScriptFile &operator=(const TWriteScriptFile &) = delete;
	// 0x4df640
	TWriteScriptFile()
	{
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
	}

	// Only closes the file; the copy the linker kept is 0x4df6b0, folded with
	// the binary files' close() (the Tibia.cfg saver calls close() and then
	// this destructor).
	~TWriteScriptFile()
	{
		File.Close();
	}

	// 0x4df660
	void close()
	{
		File.Close();
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
	}

	void open(const char *FileName);
	void writeLn();
	void writeText(const char *Text);
	void writeString(const char *Text);
	void writeNumber(int Number);

	TFile File;          // +0x000
	char Filename[1024]; // +0x004
	int Line;            // +0x404, set to 0 and never used
};

// 0x404 bytes. The constructor (0x4df690) and close() (0x4df6b0) are shared
// with TWriteBinaryFile: the linker folded the identical code. The
// destructor is close().
class TReadBinaryFile {
public:
	TReadBinaryFile(const TReadBinaryFile &) = delete;
	TReadBinaryFile &operator=(const TReadBinaryFile &) = delete;
	TReadBinaryFile()
	{
		memset(Filename, 0, sizeof(Filename));
	}

	~TReadBinaryFile()
	{
		close();
	}

	void close()
	{
		File.Close();
	}

	void open(const char *FileName);
	void seek(int Offset);
	bool eof() const;
	uint8 readByte();
	uint16 readWord();
	int readQuad();
	char *readString();
	void readBytes(uint8 *Buffer, int Count);

	TFile File;          // +0x000
	char Filename[1024]; // +0x004
};

class TWriteBinaryFile {
public:
	TWriteBinaryFile(const TWriteBinaryFile &) = delete;
	TWriteBinaryFile &operator=(const TWriteBinaryFile &) = delete;
	TWriteBinaryFile()
	{
		memset(Filename, 0, sizeof(Filename));
	}

	~TWriteBinaryFile()
	{
		close();
	}

	void close()
	{
		File.Close();
	}

	void open(const char *FileName);
	void writeByte(uint8 Byte);
	void writeWord(uint16 Word);
	void writeQuad(uint32 Quad);
	void writeBytes(const uint8 *Buffer, int Count);
	void writeString(const char *Text);

	TFile File;          // +0x000
	char Filename[1024]; // +0x004
};

// The FILE * members are TFiles (issue #64): the same 4 bytes in the original's layout.
#if PATELLA_ORIGINAL_LAYOUT
static_assert(sizeof(TReadScriptFile) == 0x80c && offsetof(TReadScriptFile, File) == 0x004
	&& offsetof(TReadScriptFile, Filename) == 0x008 && offsetof(TReadScriptFile, Special) == 0x808);
static_assert(sizeof(TWriteScriptFile) == 0x408 && offsetof(TWriteScriptFile, File) == 0x000
	&& offsetof(TWriteScriptFile, Filename) == 0x004 && offsetof(TWriteScriptFile, Line) == 0x404);
static_assert(sizeof(TReadBinaryFile) == 0x404 && offsetof(TReadBinaryFile, File) == 0x000
	&& offsetof(TReadBinaryFile, Filename) == 0x004);
static_assert(sizeof(TWriteBinaryFile) == 0x404 && offsetof(TWriteBinaryFile, File) == 0x000
	&& offsetof(TWriteBinaryFile, Filename) == 0x004);
#endif

bool ApplyPatch(const char *FileName, const uint8 *Patch, int Size, uint32 Signature);
bool ApplyFullPatch(const char *FileName, const uint8 *Data, int Size);
void InstallPatch(const char *FileName);
