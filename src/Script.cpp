// Script.cpp: reconstructed from Tibia.exe 7.72 (0x4df6d0-0x4e3c9f).
//
// The script-file tokenizer and writer and binary file access; the patching of
// the data files with patches from the login server is gone (issue #75). In
// line order, which is also binary order, except that the compiler placed
// readString of TReadBinaryFile (line 644, 0x4e3a10) and the readIdentifier,
// readNumber, readString and readSymbol helpers (no line evidence,
// 0x4e3920-0x4e3a0f) after InstallPatch (0x4e3610, gone). readString is here
// in line order; the helpers stay at the end, where the binary has them.
//
// Most errors are TERRORs thrown at the start of a function. The original's
// catch wrappers (common.h) around the calls to other members are gone (issue #78).

#include "Script.h"
#include "compat/msvc_io.h"
#include "Utils.h"
#include "common.h"
#include "compat/expected.h"
#include "compat/span.h"
#include "platform/Platform.h"
#include "types/FixedString.h"
#include "types/File.h"
#include <cstdio>
#include <cstring>
#include <utility>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries; the original's varargs (TMultiStateButton's constructor, _snprintf)
// NOLINTBEGIN(cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg)

// 0x4df6d0, lines 48-50: TryOpen, its failure thrown.
void TReadScriptFile::open(const char *FileName)
{
	patella::TExpected<void, TError> Result = TryOpen(FileName);
	if (!Result)
		throw TMyException(std::move(Result.error()));
}

// open's code, its two TERROR_REASONs returned (issue #80).
patella::TExpected<void, TError> TReadScriptFile::TryOpen(const char *FileName)
{
	if (File != nullptr)
		return TUNEXPECTED_REASON("file still open", Filename);
	if (!patella::platform::Platform().FileSystem.Open(File, FileName, TFile::TAccess::Read, TFile::TContent::Binary))
		return TUNEXPECTED_REASON("failed to open file", FileName);
	Line = 1;
	// Filename stays char[1024] until M5: TERROR_REASON's reason (a std::string) names it (issue #57).
	TStringBuffer(Filename, sizeof(Filename)).Assign(FileName);
	return {};
}

// 0x4df9b0, line 84. Closes the file and throws; the message names the file
// and line, the reason is Text. The message buffer has 1056 bytes but only
// 1054 are used.
void TReadScriptFile::error(const char *Text)
{
	char Message[1056] = {}; // NOLINT(modernize-avoid-c-arrays): TMYEXCEPTION(84)'s argument, frozen until M5 (issue #60)
	TFixedString<10> Unknown = {"(unknown)"};
	File.Close();
	MsvcSnprintf(Message, 1054, "error in script file '%s' in line %d", Filename, Line);
	if (Text == nullptr)
		Text = Unknown;
	// MSVC's analysis (C6053, issue #111): _snprintf leaves the text unterminated when it fills the 1054 bytes;
	// Message's zeroed last two bytes end it then (the buffer is zeroed since issue #111; the original's was not).
	PATELLA_MSVC_WARNING(suppress : 6053)
	throw TMYEXCEPTION(Message, "", 0, "", 0, Text);
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
		TERROR("no file open");
	int State = 0;
	int Size = 0;
	memset(String, 0, sizeof(String));
	Number = 0;
	for (;;) {
		int c;
		switch (State) {
		case 0:
			c = File.GetChar();
			if (c == EOF) {
				Token = TOKEN::ENDOFFILE;
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
			c = File.GetChar();
			if (c == EOF) {
				Token = TOKEN::ENDOFFILE;
				return;
			}
			if (c == '\n') {
				Line++;
				State = 0;
			}
			break;
		case 2: // identifier
			c = File.GetChar();
			if (Size == MAX_IDENT_LENGTH)
				error("identifier too long");
			if (c == EOF) {
				Token = TOKEN::IDENTIFIER;
				return;
			}
			if (!IsAlpha(c) && !IsDigit(c) && c != '_') {
				File.UngetChar(c);
				Token = TOKEN::IDENTIFIER;
				return;
			}
			String[Size++] = (char)c;
			break;
		case 3: // number
			c = File.GetChar();
			if (c == EOF) {
				Token = TOKEN::NUMBER;
				return;
			}
			if (IsDigit(c)) {
				Number = Number * 10 + c - '0';
			} else if (c == '-') {
				Bytes[Size++] = (uint8)Number;
				State = 4;
			} else {
				File.UngetChar(c);
				Token = TOKEN::NUMBER;
				return;
			}
			break;
		case 4: // byte sequence, after '-'
			c = File.GetChar();
			if (c == EOF)
				error("unexpected end of file");
			if (!IsDigit(c))
				error("syntax error");
			Number = c - '0';
			State = 5;
			break;
		case 5:
			c = File.GetChar();
			if (c == EOF) {
				Bytes[Size] = (uint8)Number;
				Token = TOKEN::BYTES;
				return;
			}
			if (IsDigit(c)) {
				Number = Number * 10 + c - '0';
			} else if (c == '-') {
				Bytes[Size++] = (uint8)Number;
				State = 4;
			} else {
				File.UngetChar(c);
				Bytes[Size] = (uint8)Number;
				Token = TOKEN::BYTES;
				return;
			}
			break;
		case 6: // string
			c = File.GetChar();
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
			c = File.GetChar();
			if (c == EOF)
				error("unexpected end of file");
			if (c == 'n')
				String[Size++] = '\n';
			else
				String[Size++] = (char)c;
			State = 6;
			break;
		case 8:
			Token = TOKEN::STRING;
			return;
		case 10:
			Token = TOKEN::SPECIAL;
			return;
		case 11: // coordinate
			c = File.GetChar();
			Special = '[';
			if (c == EOF) {
				Token = TOKEN::SPECIAL;
				return;
			}
			if (!IsDigit(c)) {
				File.UngetChar(c);
				Token = TOKEN::SPECIAL;
				return;
			}
			Number = c - '0';
			State = 12;
			break;
		case 12:
			c = File.GetChar();
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
			c = File.GetChar();
			if (c == EOF)
				error("unexpected end of file");
			if (!IsDigit(c))
				error("syntax error");
			Number = c - '0';
			State = 14;
			break;
		case 14:
			c = File.GetChar();
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
			c = File.GetChar();
			if (c == EOF)
				error("unexpected end of file");
			if (!IsDigit(c))
				error("syntax error");
			Number = c - '0';
			State = 16;
			break;
		case 16:
			c = File.GetChar();
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
			Token = TOKEN::COORDINATE;
			return;
		case 22: // '<', "<=", "<>"
			c = File.GetChar();
			Special = '<';
			if (c == EOF) {
				Token = TOKEN::SPECIAL;
				return;
			}
			if (c == '=') {
				State = 23;
			} else if (c == '>') {
				State = 24;
			} else {
				File.UngetChar(c);
				Token = TOKEN::SPECIAL;
				return;
			}
			break;
		case 23:
			Special = 'L';
			Token = TOKEN::SPECIAL;
			return;
		case 24:
			Special = 'N';
			Token = TOKEN::SPECIAL;
			return;
		case 25: // '>', ">="
			c = File.GetChar();
			Special = '>';
			if (c == EOF) {
				Token = TOKEN::SPECIAL;
				return;
			}
			if (c != '=') {
				File.UngetChar(c);
				Token = TOKEN::SPECIAL;
				return;
			}
			State = 26;
			break;
		case 26:
			Special = 'G';
			Token = TOKEN::SPECIAL;
			return;
		case 27: // '-', "->"
			c = File.GetChar();
			Special = '-';
			if (c == EOF) {
				Token = TOKEN::SPECIAL;
				return;
			}
			if (c != '>') {
				File.UngetChar(c);
				Token = TOKEN::SPECIAL;
				return;
			}
			State = 28;
			break;
		case 28:
			Special = 'I';
			Token = TOKEN::SPECIAL;
			return;
		default:
			TERROR1("invalid state", State);
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
	if (Token != TOKEN::IDENTIFIER)
		error("identifier expected");
	LowerString(String);
	return String;
}

// 0x4e0450
int TReadScriptFile::getNumber()
{
	if (Token != TOKEN::NUMBER)
		error("number expected");
	return Number;
}

// 0x4e0470
char *TReadScriptFile::getString()
{
	if (Token != TOKEN::STRING)
		error("string expected");
	return String;
}

// 0x4e0490
char TReadScriptFile::getSpecial()
{
	if (Token != TOKEN::SPECIAL)
		error("special-char expected");
	return Special;
}

// 0x4e04b0, lines 403-405: TryOpen, its failure thrown.
void TWriteScriptFile::open(const char *FileName)
{
	patella::TExpected<void, TError> Result = TryOpen(FileName);
	if (!Result)
		throw TMyException(std::move(Result.error()));
}

// open's code, its two TERROR_REASONs returned (issue #80). Line is reset but never counted.
patella::TExpected<void, TError> TWriteScriptFile::TryOpen(const char *FileName)
{
	if (File != nullptr)
		return TUNEXPECTED_REASON("file still open", Filename);
	if (!patella::platform::Platform().FileSystem.Open(File, FileName, TFile::TAccess::Write, TFile::TContent::Binary))
		return TUNEXPECTED_REASON("failed to open file", FileName);
	Line = 0;
	// Filename stays char[1024] until M5: TERROR_REASON's reason (a std::string) names it (issue #57).
	TStringBuffer(Filename, sizeof(Filename)).Assign(FileName);
	return {};
}

// 0x4e0790, line 441. The file is binary, so the line ends in CR LF.
void TWriteScriptFile::writeLn()
{
	if (File == nullptr)
		TERROR("no file open");
	File.PutChar('\r');
	File.PutChar('\n');
}

// 0x4e08f0, lines 451-452
void TWriteScriptFile::writeText(const char *Text)
{
	if (File == nullptr)
		TERROR("no file open");
	if (Text == nullptr)
		TERROR("no text");
	while (*Text != 0) {
		File.PutChar(*Text);
		Text++;
	}
}

// 0x4e0bb0, lines 462-463. Quotes, escaping '"' and '\\', and writes a
// line break as "\n".
void TWriteScriptFile::writeString(const char *Text)
{
	if (File == nullptr)
		TERROR("no file open");
	if (Text == nullptr)
		TERROR("no text");
	File.PutChar('"');
	while (*Text != 0) {
		if (*Text == '"' || *Text == '\\')
			File.PutChar('\\');
		if (*Text == '\n') {
			File.PutChar('\\');
			File.PutChar('n');
		} else {
			File.PutChar(*Text);
		}
		Text++;
	}
	File.PutChar('"');
}

// 0x4e0ec0, line 481
void TWriteScriptFile::writeNumber(int Number)
{
	if (File == nullptr)
		TERROR("no file open");
	TFixedString<20> Help;
	Help.Format("{}", Number);
	writeText(Help);
}

// 0x4e1040, lines 552-554: TryOpen, its failure thrown.
void TReadBinaryFile::open(const char *FileName)
{
	patella::TExpected<void, TError> Result = TryOpen(FileName);
	if (!Result)
		throw TMyException(std::move(Result.error()));
}

// open's code, its two TERROR_REASONs returned (issue #80).
patella::TExpected<void, TError> TReadBinaryFile::TryOpen(const char *FileName)
{
	if (File != nullptr)
		return TUNEXPECTED_REASON("file still open", Filename);
	if (!patella::platform::Platform().FileSystem.Open(File, FileName, TFile::TAccess::Read, TFile::TContent::Binary))
		return TUNEXPECTED_REASON("failed to open file", FileName);
	// The original's copy: unterminated when the name fills the buffer (GCC's -Wstringop-truncation, issue #111).
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wstringop-truncation\"")
	strncpy(Filename, FileName, sizeof(Filename));
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")
	return {};
}

// 0x4e1320, lines 572-573
void TReadBinaryFile::seek(int Offset)
{
	if (File == nullptr)
		TERROR("no file open");
	if (Offset < 0)
		TERROR1_REASON("negative offset", Offset, Filename);
	File.Seek(Offset, SEEK_SET);
}

// 0x4e15e0, line 582
bool TReadBinaryFile::eof() const
{
	if (File == nullptr)
		TERROR("no file open");
	return MsvcEof(File.Descriptor()) != 0;
}

// 0x4e1740, lines 603-605
uint8 TReadBinaryFile::readByte()
{
	if (File == nullptr)
		TERROR("no file open");
	uint8 Byte;
	if (File.Read(&Byte, 1, 1) == 0)
		TERROR_REASON("out of data", Filename);
	return Byte;
}

// 0x4e1a00, line 619. Little-endian. Returns a 16-bit value: every caller
// zero-extends AX.
uint16 TReadBinaryFile::readWord()
{
	const uint8 Low = readByte();
	const uint8 High = readByte();
	return (uint16)(Low | (High << 8));
}

// 0x4e1ba0, lines 627-635
int TReadBinaryFile::readQuad()
{
	if (File == nullptr)
		TERROR("no file open");
	const uint8 Byte0 = readByte();
	const uint8 Byte1 = readByte();
	const uint8 Byte2 = readByte();
	const uint8 Byte3 = readByte();
	return Byte0 | (Byte1 << 8) | (Byte2 << 16) | (Byte3 << 24);
}

// 0x4e3a10, lines 644-653. The caller frees the string with delete[].
char *TReadBinaryFile::readString()
{
	if (File == nullptr)
		TERROR("no file open");
	const int Length = readWord() & 0xffff;
	char *Text = new char[Length + 1];
	if (Length != 0)
		readBytes(patella::TSpan<uint8>(reinterpret_cast<uint8 *>(Text), static_cast<std::size_t>(Length)));
	Text[Length] = 0;
	return Text;
}

// 0x4e1e50, lines 661-664. The original's parameters (Buffer, Count) are the span's pointer and its size as an int, the
// name the "out of data" error's text uses (issue #69, as #59).
void TReadBinaryFile::readBytes(patella::TSpan<uint8> Bytes)
{
	uint8 *const Buffer = Bytes.data();
	const int Count = static_cast<int>(Bytes.size());
	if (File == nullptr)
		TERROR("no file open");
	if (Buffer == nullptr)
		TERROR("Buffer==NULL");
	if ((int)File.Read(Buffer, 1, Count) != Count)
		TERROR1_REASON("out of data", Count, Filename);
}

// 0x4e21a0, lines 706-708: TryOpen, its failure thrown.
void TWriteBinaryFile::open(const char *FileName)
{
	patella::TExpected<void, TError> Result = TryOpen(FileName);
	if (!Result)
		throw TMyException(std::move(Result.error()));
}

// open's code, its two TERROR_REASONs returned (issue #80).
patella::TExpected<void, TError> TWriteBinaryFile::TryOpen(const char *FileName)
{
	if (File != nullptr)
		return TUNEXPECTED_REASON("file still open", Filename);
	if (!patella::platform::Platform().FileSystem.Open(File, FileName, TFile::TAccess::Write, TFile::TContent::Binary))
		return TUNEXPECTED_REASON("failed to open file", FileName);
	// The original's copy: unterminated when the name fills the buffer (GCC's -Wstringop-truncation, issue #111).
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wstringop-truncation\"")
	strncpy(Filename, FileName, sizeof(Filename));
	PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")
	return {};
}

// 0x4e2480, lines 727-729
void TWriteBinaryFile::writeByte(uint8 Byte)
{
	if (File == nullptr)
		TERROR("no file open");
	if (File.Write(&Byte, 1, 1) != 1)
		TERROR_REASON("failed to write file", Filename);
}

// 0x4e2750, lines 737-742
void TWriteBinaryFile::writeWord(uint16 Word)
{
	if (File == nullptr)
		TERROR("no file open");
	writeByte((uint8)Word);
	writeByte((uint8)(Word >> 8));
}

// 0x4e29e0, lines 750-757
void TWriteBinaryFile::writeQuad(uint32 Quad)
{
	if (File == nullptr)
		TERROR("no file open");
	writeByte((uint8)Quad);
	writeByte((uint8)(Quad >> 8));
	writeByte((uint8)(Quad >> 16));
	writeByte((uint8)(Quad >> 24));
}

// 0x4e2c80, lines 765-766. Writes one item of Count bytes, so Count 0 fails
// (writeString does not call it for an empty string). Buffer and Count are the span's pointer and its size as an int
// (issue #69).
void TWriteBinaryFile::writeBytes(patella::TSpan<const uint8> Bytes)
{
	const uint8 *const Buffer = Bytes.data();
	const int Count = static_cast<int>(Bytes.size());
	if (File == nullptr)
		TERROR("no file open");
	if (File.Write(Buffer, Count, 1) != 1)
		TERROR1_REASON("failed to write file", Count, Filename);
}

// 0x4e2f50, line 779. A word with the length, then the characters; longer
// strings are cut to the low 16 bits of their length.
void TWriteBinaryFile::writeString(const char *Text)
{
	const int Length = (int)strlen(Text);
	writeWord((uint16)Length);
	if ((uint16)Length != 0)
		writeBytes(patella::TSpan<const uint8>(reinterpret_cast<const uint8 *>(Text), (uint16)Length));
}

// 0x4e3100 ApplyPatch (a difference patch of a file), 0x4e3500 ApplyFullPatch (a whole new file), both written next
// to the file ("Tibia.exe" as "Tibia.bak", any other name with '_' as its last character), and 0x4e3610 InstallPatch
// (lines 879-883: the written file moved into place) are gone with the patcher since Patella-Modern issue #75
// (decision D9).

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
	if (Token == TOKEN::SPECIAL && Special == '-') {
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
	if (Token != TOKEN::SPECIAL)
		error("special-char expected");
	if (Special != Symbol)
		error("special-char expected");
}
// NOLINTEND(cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg)
