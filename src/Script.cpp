// Script.cpp: reconstructed from Tibia.exe 7.72 (0x4df6d0-0x4e3c9f).
//
// The script-file tokenizer and writer, binary file access, and the patching
// of the data files with patches from the login server. In line order, which
// is also binary order, except that the compiler placed readString of
// TReadBinaryFile (line 644, 0x4e3a10) and the readIdentifier, readNumber,
// readString and readSymbol helpers (no line evidence, 0x4e3920-0x4e3a0f)
// after InstallPatch. readString is here in line order; the helpers stay at
// the end, where the binary has them.
//
// Most errors are TERRORs thrown before a TBEGIN; the functions that call
// other members wrap the calls in TBEGIN/TEND.

#include "Script.h"
#include <io.h>
#include "Utils.h"
#include "sysinc.h"

// 0x4df6d0, lines 48-50
void TReadScriptFile::open(const char *FileName)
{
	if (File != nullptr)
		TERROR_REASON(48, "file still open", Filename);
	File = fopen(FileName, "rb");
	if (File == nullptr)
		TERROR_REASON(50, "failed to open file", FileName);
	Line = 1;
	strcpy(Filename, FileName);
}

// 0x4df9b0, line 84. Closes the file and throws; the message names the file
// and line, the reason is Text. The message buffer has 1056 bytes but only
// 1054 are used.
void TReadScriptFile::error(const char *Text)
{
	char Message[1056];
	char Unknown[] = "(unknown)";
	if (File != nullptr)
		fclose(File);
	File = nullptr;
	_snprintf(Message, 1054, "error in script file '%s' in line %d", Filename, Line);
	if (Text == nullptr)
		Text = Unknown;
	throw TMYEXCEPTION(84, Message, "", 0, "", 0, Text);
}

// 0x4dfb60, lines 93-230. A state machine over getc. String is cleared
// first, so identifiers and strings need no terminator; Bytes shares its
// buffer. Identifiers start with a letter; '-' before a number is a SPECIAL
// token (readNumber handles the sign). IsAlpha compares with signed chars,
// so umlauts from getc (0xe4 and so on) are not letters here: they become
// SPECIAL tokens. The jump table has the 24 cases
// below; states 9 and 18-21 do not exist and go to the default.
void TReadScriptFile::nextToken()
{
	if (File == nullptr)
		TERROR(93, "no file open");
	int State = 0;
	int Size = 0;
	memset(String, 0, sizeof(String));
	Number = 0;
	for (;;) {
		int c;
		switch (State) {
		case 0:
			c = getc(File);
			if (c == EOF) {
				Token = ENDOFFILE;
				return;
			}
			if (c == '\n') {
				Line++;
			} else if (!IsSpace(c)) {
				if (c == '#') {
					State = 1;
				} else if (IsAlpha(c)) {
					String[Size++] = (char)c;
					State = 2;
				} else if (IsDigit(c)) {
					Number = c - '0';
					State = 3;
				} else if (c == '"') {
					State = 6;
				} else if (c == '[') {
					State = 11;
				} else if (c == '<') {
					State = 22;
				} else if (c == '>') {
					State = 25;
				} else if (c == '-') {
					State = 27;
				} else {
					Special = (char)c;
					State = 10;
				}
			}
			break;
		case 1: // comment
			c = getc(File);
			if (c == EOF) {
				Token = ENDOFFILE;
				return;
			}
			if (c == '\n') {
				Line++;
				State = 0;
			}
			break;
		case 2: // identifier
			c = getc(File);
			if (Size == MAX_IDENT_LENGTH)
				error("identifier too long");
			if (c == EOF) {
				Token = IDENTIFIER;
				return;
			}
			if (!IsAlpha(c) && !IsDigit(c) && c != '_') {
				ungetc(c, File);
				Token = IDENTIFIER;
				return;
			}
			String[Size++] = (char)c;
			break;
		case 3: // number
			c = getc(File);
			if (c == EOF) {
				Token = NUMBER;
				return;
			}
			if (IsDigit(c)) {
				Number = Number * 10 + c - '0';
			} else if (c == '-') {
				Bytes[Size++] = (uint8)Number;
				State = 4;
			} else {
				ungetc(c, File);
				Token = NUMBER;
				return;
			}
			break;
		case 4: // byte sequence, after '-'
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (!IsDigit(c))
				error("syntax error");
			Number = c - '0';
			State = 5;
			break;
		case 5:
			c = getc(File);
			if (c == EOF) {
				Bytes[Size] = (uint8)Number;
				Token = BYTES;
				return;
			}
			if (IsDigit(c)) {
				Number = Number * 10 + c - '0';
			} else if (c == '-') {
				Bytes[Size++] = (uint8)Number;
				State = 4;
			} else {
				ungetc(c, File);
				Bytes[Size] = (uint8)Number;
				Token = BYTES;
				return;
			}
			break;
		case 6: // string
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (c == '"') {
				State = 8;
			} else if (c == '\\') {
				State = 7;
			} else {
				if (c == '\n')
					Line++;
				String[Size++] = (char)c;
			}
			break;
		case 7: // escape
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (c == 'n')
				String[Size++] = '\n';
			else
				String[Size++] = (char)c;
			State = 6;
			break;
		case 8:
			Token = STRING;
			return;
		case 10:
			Token = SPECIAL;
			return;
		case 11: // coordinate
			c = getc(File);
			Special = '[';
			if (c == EOF) {
				Token = SPECIAL;
				return;
			}
			if (!IsDigit(c)) {
				ungetc(c, File);
				Token = SPECIAL;
				return;
			}
			Number = c - '0';
			State = 12;
			break;
		case 12:
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (IsDigit(c)) {
				Number = Number * 10 + c - '0';
			} else if (c == ',') {
				CoordX = Number;
				State = 13;
			} else {
				error("syntax error");
			}
			break;
		case 13:
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (!IsDigit(c))
				error("syntax error");
			Number = c - '0';
			State = 14;
			break;
		case 14:
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (IsDigit(c)) {
				Number = Number * 10 + c - '0';
			} else if (c == ',') {
				CoordY = Number;
				State = 15;
			} else {
				error("syntax error");
			}
			break;
		case 15:
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (!IsDigit(c))
				error("syntax error");
			Number = c - '0';
			State = 16;
			break;
		case 16:
			c = getc(File);
			if (c == EOF)
				error("unexpected end of file");
			if (IsDigit(c)) {
				Number = Number * 10 + c - '0';
			} else if (c == ']') {
				CoordZ = Number;
				State = 17;
			} else {
				error("syntax error");
			}
			break;
		case 17:
			Token = COORDINATE;
			return;
		case 22: // '<', "<=", "<>"
			c = getc(File);
			Special = '<';
			if (c == EOF) {
				Token = SPECIAL;
				return;
			}
			if (c == '=') {
				State = 23;
			} else if (c == '>') {
				State = 24;
			} else {
				ungetc(c, File);
				Token = SPECIAL;
				return;
			}
			break;
		case 23:
			Special = 'L';
			Token = SPECIAL;
			return;
		case 24:
			Special = 'N';
			Token = SPECIAL;
			return;
		case 25: // '>', ">="
			c = getc(File);
			Special = '>';
			if (c == EOF) {
				Token = SPECIAL;
				return;
			}
			if (c != '=') {
				ungetc(c, File);
				Token = SPECIAL;
				return;
			}
			State = 26;
			break;
		case 26:
			Special = 'G';
			Token = SPECIAL;
			return;
		case 27: // '-', "->"
			c = getc(File);
			Special = '-';
			if (c == EOF) {
				Token = SPECIAL;
				return;
			}
			if (c != '>') {
				ungetc(c, File);
				Token = SPECIAL;
				return;
			}
			State = 28;
			break;
		case 28:
			Special = 'I';
			Token = SPECIAL;
			return;
		default:
			TERROR1(230, "invalid state", State);
		}
		// The binary checks this after every state change (the first pass
		// starts at the switch).
		if (Size == MAX_STRING_LENGTH - 1)
			error("string too long");
	}
}

// 0x4e0420
char *TReadScriptFile::getIdentifier()
{
	if (Token != IDENTIFIER)
		error("identifier expected");
	LowerString(String);
	return String;
}

// 0x4e0450
int TReadScriptFile::getNumber()
{
	if (Token != NUMBER)
		error("number expected");
	return Number;
}

// 0x4e0470
char *TReadScriptFile::getString()
{
	if (Token != STRING)
		error("string expected");
	return String;
}

// 0x4e0490
char TReadScriptFile::getSpecial()
{
	if (Token != SPECIAL)
		error("special-char expected");
	return Special;
}

// 0x4e04b0, lines 403-405. Line is reset but never counted.
void TWriteScriptFile::open(const char *FileName)
{
	if (File != nullptr)
		TERROR_REASON(403, "file still open", Filename);
	File = fopen(FileName, "wb");
	if (File == nullptr)
		TERROR_REASON(405, "failed to open file", FileName);
	Line = 0;
	strcpy(Filename, FileName);
}

// 0x4e0790, line 441. The file is binary, so the line ends in CR LF.
void TWriteScriptFile::writeLn()
{
	if (File == nullptr)
		TERROR(441, "no file open");
	fputc('\r', File);
	fputc('\n', File);
}

// 0x4e08f0, lines 451-452
void TWriteScriptFile::writeText(const char *Text)
{
	if (File == nullptr)
		TERROR(451, "no file open");
	if (Text == nullptr)
		TERROR(452, "no text");
	while (*Text != 0) {
		fputc(*Text, File);
		Text++;
	}
}

// 0x4e0bb0, lines 462-463. Quotes, escaping '"' and '\\', and writes a
// line break as "\n".
void TWriteScriptFile::writeString(const char *Text)
{
	if (File == nullptr)
		TERROR(462, "no file open");
	if (Text == nullptr)
		TERROR(463, "no text");
	fputc('"', File);
	while (*Text != 0) {
		if (*Text == '"' || *Text == '\\')
			fputc('\\', File);
		if (*Text == '\n') {
			fputc('\\', File);
			fputc('n', File);
		} else {
			fputc(*Text, File);
		}
		Text++;
	}
	fputc('"', File);
}

// 0x4e0ec0, line 481
void TWriteScriptFile::writeNumber(int Number)
{
	if (File == nullptr)
		TERROR(481, "no file open");
	char Help[20];
	sprintf(Help, "%d", Number);
	writeText(Help);
}

// 0x4e1040, lines 552-554
void TReadBinaryFile::open(const char *FileName)
{
	if (File != nullptr)
		TERROR_REASON(552, "file still open", Filename);
	File = fopen(FileName, "rb");
	if (File == nullptr)
		TERROR_REASON(554, "failed to open file", FileName);
	strncpy(Filename, FileName, sizeof(Filename));
}

// 0x4e1320, lines 572-573
void TReadBinaryFile::seek(int Offset)
{
	if (File == nullptr)
		TERROR(572, "no file open");
	if (Offset < 0)
		TERROR1_REASON(573, "negative offset", Offset, Filename);
	fseek(File, Offset, SEEK_SET);
}

// 0x4e15e0, line 582
bool TReadBinaryFile::eof()
{
	if (File == nullptr)
		TERROR(582, "no file open");
	return _eof(_fileno(File)) != 0;
}

// 0x4e1740, lines 603-605
uint8 TReadBinaryFile::readByte()
{
	if (File == nullptr)
		TERROR(603, "no file open");
	uint8 Byte;
	if (fread(&Byte, 1, 1, File) == 0)
		TERROR_REASON(605, "out of data", Filename);
	return Byte;
}

// 0x4e1a00, line 619. Little-endian. Returns a 16-bit value: every caller
// zero-extends AX.
uint16 TReadBinaryFile::readWord()
{
	TBEGIN
	uint8 Low = readByte();
	uint8 High = readByte();
	return (uint16)(Low | (High << 8));
	TEND(619)
}

// 0x4e1ba0, lines 627-635
int TReadBinaryFile::readQuad()
{
	if (File == nullptr)
		TERROR(627, "no file open");
	TBEGIN
	uint8 Byte0 = readByte();
	uint8 Byte1 = readByte();
	uint8 Byte2 = readByte();
	uint8 Byte3 = readByte();
	return Byte0 | (Byte1 << 8) | (Byte2 << 16) | (Byte3 << 24);
	TEND(635)
}

// 0x4e3a10, lines 644-653. The caller frees the string with delete[].
char *TReadBinaryFile::readString()
{
	if (File == nullptr)
		TERROR(644, "no file open");
	TBEGIN
	int Length = readWord() & 0xffff;
	char *Text = new char[Length + 1];
	if (Length != 0)
		readBytes((uint8 *)Text, Length);
	Text[Length] = 0;
	return Text;
	TEND(653)
}

// 0x4e1e50, lines 661-664
void TReadBinaryFile::readBytes(uint8 *Buffer, int Count)
{
	if (File == nullptr)
		TERROR(661, "no file open");
	if (Buffer == nullptr)
		TERROR(662, "Buffer==NULL");
	if ((int)fread(Buffer, 1, Count, File) != Count)
		TERROR1_REASON(664, "out of data", Count, Filename);
}

// 0x4e21a0, lines 706-708
void TWriteBinaryFile::open(const char *FileName)
{
	if (File != nullptr)
		TERROR_REASON(706, "file still open", Filename);
	File = fopen(FileName, "wb");
	if (File == nullptr)
		TERROR_REASON(708, "failed to open file", FileName);
	strncpy(Filename, FileName, sizeof(Filename));
}

// 0x4e2480, lines 727-729
void TWriteBinaryFile::writeByte(uint8 Byte)
{
	if (File == nullptr)
		TERROR(727, "no file open");
	if (fwrite(&Byte, 1, 1, File) != 1)
		TERROR_REASON(729, "failed to write file", Filename);
}

// 0x4e2750, lines 737-742
void TWriteBinaryFile::writeWord(uint16 Word)
{
	if (File == nullptr)
		TERROR(737, "no file open");
	TBEGIN
	writeByte((uint8)Word);
	writeByte((uint8)(Word >> 8));
	TEND(742)
}

// 0x4e29e0, lines 750-757
void TWriteBinaryFile::writeQuad(uint32 Quad)
{
	if (File == nullptr)
		TERROR(750, "no file open");
	TBEGIN
	writeByte((uint8)Quad);
	writeByte((uint8)(Quad >> 8));
	writeByte((uint8)(Quad >> 16));
	writeByte((uint8)(Quad >> 24));
	TEND(757)
}

// 0x4e2c80, lines 765-766. Writes one item of Count bytes, so Count 0 fails
// (writeString does not call it for an empty string).
void TWriteBinaryFile::writeBytes(const uint8 *Buffer, int Count)
{
	if (File == nullptr)
		TERROR(765, "no file open");
	if (fwrite(Buffer, Count, 1, File) != 1)
		TERROR1_REASON(766, "failed to write file", Count, Filename);
}

// 0x4e2f50, line 779. A word with the length, then the characters; longer
// strings are cut to the low 16 bits of their length.
void TWriteBinaryFile::writeString(const char *Text)
{
	TBEGIN
	int Length = (int)strlen(Text);
	writeWord((uint16)Length);
	if ((uint16)Length != 0)
		writeBytes((const uint8 *)Text, (uint16)Length);
	TEND(779)
}

// The patched file is written next to the original: "Tibia.exe" becomes
// "Tibia.bak", any other name gets '_' as its last character.

// 0x4e3100. Applies a difference patch to FileName and writes the result
// to the patched file name (InstallPatch moves it into place). A nonzero
// Signature must match the first four bytes of the old file. Each command
// byte has the operation in its top two bits and the count in the low six;
// counts 61, 62 and 63 mean that a byte, word or quad with the count
// follows. Operations: copy (0x00), skip (0x40), insert from the patch
// (0x80), and replace with bytes from the patch (0xc0). Input after the
// last command is dropped. Any error gives false.
bool ApplyPatch(const char *FileName, const uint8 *Patch, int Size, uint32 Signature)
{
	TReadBinaryFile In;
	TWriteBinaryFile Out;
	try {
		In.open(FileName);
		if (Signature != 0) {
			if ((uint32)In.readQuad() != Signature) {
				In.close();
				Out.close();
				return false;
			}
			In.seek(0);
		}
		char Name[1024];
		const char *OutName;
		if (strcmp(FileName, "Tibia.exe") == 0) {
			OutName = "Tibia.bak";
		} else {
			strcpy(Name, FileName);
			Name[strlen(FileName) - 1] = '_';
			OutName = Name;
		}
		Out.open(OutName);
		int Pos = 0;
		while (Pos < Size) {
			int Command = Patch[Pos++];
			int Count = Command & 0x3f;
			if (Count == 61) {
				Count = Patch[Pos++];
			} else if (Count == 62) {
				Count = Patch[Pos] | (Patch[Pos + 1] << 8);
				Pos += 2;
			} else if (Count == 63) {
				Count = Patch[Pos] | (Patch[Pos + 1] << 8) | (Patch[Pos + 2] << 16) | (Patch[Pos + 3] << 24);
				Pos += 4;
			}
			int i;
			switch (Command & 0xc0) {
			case 0x00:
				for (i = 0; i < Count; i++)
					Out.writeByte(In.readByte());
				break;
			case 0x40:
				for (i = 0; i < Count; i++)
					In.readByte();
				break;
			case 0x80:
				for (i = 0; i < Count; i++)
					Out.writeByte(Patch[Pos++]);
				break;
			case 0xc0:
				for (i = 0; i < Count; i++) {
					Out.writeByte(Patch[Pos++]);
					In.readByte();
				}
				break;
			}
		}
		In.close();
		Out.close();
	} catch (TMyException &) {
		In.close();
		Out.close();
		return false;
	}
	return true;
}

// 0x4e3500. Writes a complete new file under the patched file name.
bool ApplyFullPatch(const char *FileName, const uint8 *Data, int Size)
{
	TWriteBinaryFile Out;
	try {
		char Name[1024];
		const char *OutName;
		if (strcmp(FileName, "Tibia.exe") == 0) {
			OutName = "Tibia.bak";
		} else {
			strcpy(Name, FileName);
			Name[strlen(FileName) - 1] = '_';
			OutName = Name;
		}
		Out.open(OutName);
		for (int i = 0; i < Size; i++)
			Out.writeByte(Data[i]);
	} catch (...) {
		Out.close();
		return false;
	}
	return true;
}

// 0x4e3610, lines 879-883. Replaces FileName with its patched file. For
// "Tibia.exe" it would look for "Tibia.ex_", not the "Tibia.bak" that the
// patch functions write (the client lets Patch.exe handle that case).
void InstallPatch(const char *FileName)
{
	if (FileName == nullptr)
		TERROR(879, "no file name");
	char Name[1024];
	strcpy(Name, FileName);
	Name[strlen(FileName) - 1] = '_';
	if (!FileExists(Name))
		TERROR_REASON(883, "file does not exists", Name);
	remove(FileName);
	rename(Name, FileName);
}

// 0x4e3920. The next token, which must be an identifier, in lower case.
char *TReadScriptFile::readIdentifier()
{
	nextToken();
	return getIdentifier();
}

// 0x4e3950. Accepts a '-' token before the number.
int TReadScriptFile::readNumber()
{
	int Sign = 1;
	nextToken();
	if (Token == SPECIAL && Special == '-') {
		Sign = -1;
		nextToken();
	}
	return getNumber() * Sign;
}

// 0x4e39a0
char *TReadScriptFile::readString()
{
	nextToken();
	return getString();
}

// 0x4e39d0
void TReadScriptFile::readSymbol(char Symbol)
{
	nextToken();
	if (Token != SPECIAL)
		error("special-char expected");
	if (Special != Symbol)
		error("special-char expected");
}
