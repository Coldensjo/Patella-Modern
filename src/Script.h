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

#pragma once

#include "common.h"

#define MAX_IDENT_LENGTH 20    // "identifier too long"
#define MAX_STRING_LENGTH 1000 // "string too long" at 999 characters

enum TOKEN {
	ENDOFFILE,
	IDENTIFIER,
	NUMBER,
	STRING,
	BYTES,      // "1-2-3"
	COORDINATE, // "[x,y,z]"
	SPECIAL,    // any other character; "<=" 'L', ">=" 'G', "<>" 'N', "->" 'I'
};

// 0x80c bytes.
class TReadScriptFile {
public:
	// 0x4df550. Token is not set.
	TReadScriptFile()
	{
		File = NULL;
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
		memset(String, 0, sizeof(String));
		Bytes = (uint8 *)String;
		Number = 0;
		CoordX = 0;
		CoordY = 0;
		CoordZ = 0;
		Special = '#';
	}

	// 0x4df5b0
	~TReadScriptFile()
	{
		if (File != NULL)
			fclose(File);
		File = NULL;
	}

	// 0x4df5d0. Resets everything but Token, like the constructor.
	void close()
	{
		if (File != NULL)
			fclose(File);
		File = NULL;
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
		memset(String, 0, sizeof(String));
		Number = 0;
		CoordX = 0;
		CoordY = 0;
		CoordZ = 0;
		Bytes = (uint8 *)String;
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
	FILE *File;                     // +0x004
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
	// 0x4df640
	TWriteScriptFile()
	{
		File = NULL;
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
	}

	// Only closes the file; the copy the linker kept is 0x4df6b0, folded with
	// the binary files' close() (the Tibia.cfg saver calls close() and then
	// this destructor).
	~TWriteScriptFile()
	{
		if (File != NULL)
			fclose(File);
		File = NULL;
	}

	// 0x4df660
	void close()
	{
		if (File != NULL)
			fclose(File);
		File = NULL;
		memset(Filename, 0, sizeof(Filename));
		Line = 0;
	}

	void open(const char *FileName);
	void writeLn();
	void writeText(const char *Text);
	void writeString(const char *Text);
	void writeNumber(int Number);

	FILE *File;          // +0x000
	char Filename[1024]; // +0x004
	int Line;            // +0x404, set to 0 and never used
};

// 0x404 bytes. The constructor (0x4df690) and close() (0x4df6b0) are shared
// with TWriteBinaryFile: the linker folded the identical code. The
// destructor is close().
class TReadBinaryFile {
public:
	TReadBinaryFile()
	{
		File = NULL;
		memset(Filename, 0, sizeof(Filename));
	}

	~TReadBinaryFile()
	{
		close();
	}

	void close()
	{
		if (File != NULL)
			fclose(File);
		File = NULL;
	}

	void open(const char *FileName);
	void seek(int Offset);
	bool eof();
	uint8 readByte();
	uint16 readWord();
	int readQuad();
	char *readString();
	void readBytes(uint8 *Buffer, int Count);

	FILE *File;          // +0x000
	char Filename[1024]; // +0x004
};

class TWriteBinaryFile {
public:
	TWriteBinaryFile()
	{
		File = NULL;
		memset(Filename, 0, sizeof(Filename));
	}

	~TWriteBinaryFile()
	{
		close();
	}

	void close()
	{
		if (File != NULL)
			fclose(File);
		File = NULL;
	}

	void open(const char *FileName);
	void writeByte(uint8 Byte);
	void writeWord(uint16 Word);
	void writeQuad(uint32 Quad);
	void writeBytes(const uint8 *Buffer, int Count);
	void writeString(const char *Text);

	FILE *File;          // +0x000
	char Filename[1024]; // +0x004
};

bool ApplyPatch(const char *FileName, const uint8 *Patch, int Size, uint32 Signature);
bool ApplyFullPatch(const char *FileName, const uint8 *Data, int Size);
void InstallPatch(const char *FileName);
