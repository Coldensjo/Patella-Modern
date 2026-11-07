// Utils.cpp: reconstructed from Tibia.exe 7.72 (0x508fa0-0x50bfef).
//
// The functions before line 66 have no assertions; they are assigned to this
// file because they run straight up to its first assertion and are inlined
// into its later functions. None of the functions has the original's catch
// wrappers (common.h).

#include "Utils.h"
#include "common.h"
#include "compat/span.h"
#include "types/FixedString.h"
#include <cstring>

// 0x508fa0
int IsSpace(int c)
{
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

// 0x508fd0
int IsAlpha(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		|| c == (char)0xe4 || c == (char)0xf6 || c == (char)0xfc // ä ö ü
		|| c == (char)0xc4 || c == (char)0xd6 || c == (char)0xdc // Ä Ö Ü
		|| c == (char)0xdf;                                      // ß
}

// 0x509020
int IsDigit(int c)
{
	return c >= '0' && c <= '9';
}

// 0x509040
int ToLower(int c)
{
	if ((c >= 'A' && c <= 'Z') || ((uint8)c >= 0xc0 && (uint8)c <= 0xde))
		c += 0x20;
	return c;
}

// 0x509060
int ToUpper(int c)
{
	if ((c >= 'a' && c <= 'z') || ((uint8)c >= 0xe0 && (uint8)c != 0xff))
		c -= 0x20;
	return c;
}

// 0x509080. All components 0..1; results 0..255, truncated.
void HSVToRGB(double H, double S, double V, int *R, int *G, int *B)
{
	if (V == 0.0) {
		*R = 0;
		*G = 0;
		*B = 0;
		return;
	}
	if (S == 0.0) {
		*R = *G = *B = (int)(V * 255.0);
		return;
	}
	double r, g, b;
	if (H < 1.0 / 6) {
		b = (1.0 - S) * V;
		g = (V - b) * H * 6.0 + b;
		r = V;
	} else if (H < 1.0 / 3) {
		g = V;
		b = (1.0 - S) * V;
		r = V - (H - 1.0 / 6) * (V - b) * 6.0;
	} else if (H < 0.5) {
		g = V;
		r = (1.0 - S) * V;
		b = (H - 1.0 / 3) * (V - r) * 6.0 + r;
	} else if (H < 2.0 / 3) {
		b = V;
		r = (1.0 - S) * V;
		g = V - (H - 0.5) * (V - r) * 6.0;
	} else if (H < 5.0 / 6) {
		b = V;
		g = (1.0 - S) * V;
		r = (H - 2.0 / 3) * (V - g) * 6.0 + g;
	} else {
		r = V;
		g = (1.0 - S) * V;
		b = V - (H - 5.0 / 6) * (V - g) * 6.0;
	}
	*R = (int)(r * 255.0);
	*G = (int)(g * 255.0);
	*B = (int)(b * 255.0);
}

// 0x509280. The outfit palette: 7 rows of 19 colours; column 0 is grey,
// columns 1-18 are hues.
void GetOutfitColor(int Color, int *R, int *G, int *B)
{
	if (Color >= 133)
		Color = 0;
	if (Color % 19 == 0) {
		// The row, Color / 19, is an integer on purpose (the binary's).
		HSVToRGB(0.0, 0.0, 1.0 - (Color / 19) * (1.0 / 7), R, G, B); // NOLINT(bugprone-integer-division)
		return;
	}
	double S = 1.0;
	double V = 1.0;
	switch (Color / 19) {
		// clang-format off
	case 0: S = 0.25; V = 1.0; break;
	case 1: S = 0.25; V = 0.75; break;
	case 2: S = 0.5; V = 0.75; break;
	case 3: S = 0.667; V = 0.75; break;
	case 4: S = 1.0; V = 1.0; break;
	case 5: S = 1.0; V = 0.75; break;
	case 6: S = 1.0; V = 0.5; break;
	default: break; // a negative Color: S and V stay 1.0, as in the binary
		// clang-format on
	}
	HSVToRGB((Color % 19) * (1.0 / 18), S, V, R, G, B);
}

// 0x5093d0, line 66
void LowerString(char *s)
{
	PATELLA_ASSERT(s!=nullptr);
	while (*s != 0) {
		*s = (char)ToLower(*s);
		s++;
	}
}

// 0x509540, line 88. Upper-cases the first letter of each word.
void CapitalizeString(char *s)
{
	PATELLA_ASSERT(s!=nullptr);
	bool WordStart = true;
	for (int i = 0; i < (int)strlen(s); i++) {
		if (IsAlpha(s[i])) {
			if (WordStart)
				s[i] = (char)ToUpper((uint8)s[i]);
			WordStart = false;
		} else {
			WordStart = true;
		}
	}
}

// 0x509710, lines 106-107. Like strnicmp, with the Latin-1 case rules above.
int CompareStrings(const char *s1, const char *s2, int Count)
{
	PATELLA_ASSERT(s1!=nullptr);
	PATELLA_ASSERT(s2!=nullptr);
	for (int i = 0; i != Count; i++) {
		if (ToLower(s1[i]) > ToLower(s2[i]))
			return 1;
		if (ToLower(s1[i]) < ToLower(s2[i]))
			return -1;
		if (s1[i] == 0)
			return 0;
	}
	return 0;
}

// 0x509a60, lines 124-125. The original's parameters (Destination, Size) are the span's pointer and its size as an int,
// the names of the assertions' text: a null pointer still fails 124, a size above INT_MAX is the original's negative
// Size (issue #69, as #59).
void CopyString(patella::TSpan<char> DestinationBytes, const char *Source)
{
	char *const Destination = DestinationBytes.data();
	const int Size = static_cast<int>(DestinationBytes.size());
	PATELLA_ASSERT(Destination!=nullptr);
	PATELLA_ASSERT(Source!=nullptr);
	strncpy(Destination, Source, Size);
	Destination[Size - 1] = 0;
}

// 0x509d10, lines 158-159. Copies Source without leading and trailing white space.
void TrimString(char *Destination, const char *Source)
{
	PATELLA_ASSERT(Source!=nullptr);
	PATELLA_ASSERT(Destination!=nullptr);
	int i = 0;
	while (Source[i] != 0 && IsSpace(Source[i]))
		i++;
	// strcpy: the caller's Destination holds Source (the console's caption, a talk line; it may be Source).
	const char *Trimmed = &Source[i];
	TStringBuffer(Destination, strlen(Trimmed) + 1).Assign(Trimmed);
	for (int Length = (int)strlen(Destination); Length > 0 && IsSpace(Destination[Length - 1]); Length--)
		Destination[Length - 1] = 0;
}

// 0x50a040, line 179. Cuts Text to MaxLength characters ending in "...";
// returns whether it was cut.
bool ShortenString(char *Text, int MaxLength)
{
	PATELLA_ASSERT(Text!=nullptr);
	const int Length = (int)strlen(Text);
	if (Length <= MaxLength)
		return false;
	const int Position = MaxLength < 3 ? 0 : MaxLength - 3;
	memset(&Text[Position], '.', Length < 3 ? Length : 3);
	Text[Position + 3] = 0;
	return true;
}

// 0x50a200, lines 286-287. The original's parameters (data, size) are the span's pointer and its size as an int, the
// names of the assertions' text: a null pointer, or a size above INT_MAX (the original's negative size), still fails
// them (issue #59).
TReadBuffer::TReadBuffer(patella::TSpan<uint8> Bytes)
{
	uint8 *const data = Bytes.data();
	const int size = static_cast<int>(Bytes.size());
	PATELLA_ASSERT(data!=nullptr);
	PATELLA_ASSERT(size>=0);
	Size = size;
	Position = 0;
	Data = data;
}

// 0x50a4b0, line 299
uint8 TReadBuffer::GetByte()
{
	PATELLA_ASSERT_VALUES(Position<=Size-1, Position, Size);
	return Data[Position++];
}

// 0x50a610, line 310
uint16 TReadBuffer::GetWord()
{
	PATELLA_ASSERT_VALUES(Position<=Size-2, Position, Size);
	const uint16 Result = (uint16)(Data[Position] + Data[Position + 1] * 0x100);
	Position += 2;
	return Result;
}

// 0x50a780, line 321
uint32 TReadBuffer::GetQuad()
{
	PATELLA_ASSERT_VALUES(Position<=Size-4, Position, Size);
	const uint32 Result = Data[Position] | (Data[Position + 1] << 8) | (Data[Position + 2] << 16)
		| (Data[Position + 3] << 24);
	Position += 4;
	return Result;
}

// 0x50a900, lines 334-340. Reads at most MaxLength-1 characters; skips the rest. Text is the original's (Buffer,
// MaxLength): its size as an int is MaxLength, so a size above INT_MAX is the original's negative MaxLength, no
// limit (only the tests pass one; issue #59).
void TReadBuffer::GetString(patella::TSpan<char> Text)
{
	char *const Buffer = Text.data();
	const int MaxLength = static_cast<int>(Text.size());
	PATELLA_ASSERT(Buffer!=nullptr);
	const int Length = GetWord();
	Buffer[0] = 0;
	if (Length == 0)
		return;
	PATELLA_ASSERT_VALUES(Position<=Size-Length, Position, Size-Length);
	if (MaxLength < 0 || Length < MaxLength) {
		memcpy(Buffer, &Data[Position], Length);
		Buffer[Length] = 0;
	} else {
		memcpy(Buffer, &Data[Position], MaxLength - 1);
		Buffer[MaxLength - 1] = 0;
	}
	Position += Length;
}

// 0x50abb0, line 359. The caller owns the result. Patella-Modern issue #139, item 4, a deliberate difference
// (notes/decisions.md, "#139"): the check comes before the allocation, so a text that does not fit throws with
// nothing allocated. The binary allocated first and lost the text when the check threw.
char *TReadBuffer::GetString()
{
	const int Length = GetWord();
	PATELLA_ASSERT_VALUES(Position<=Size-Length, Position, Size-Length);
	char *Result = new char[Length + 1];
	memcpy(Result, &Data[Position], Length);
	Result[Length] = 0;
	Position += Length;
	return Result;
}

// 0x50ad50, lines 371-372
void TReadBuffer::GetBytes(patella::TSpan<uint8> Bytes)
{
	uint8 *const Buffer = Bytes.data();
	const int Count = static_cast<int>(Bytes.size());
	PATELLA_ASSERT(Buffer!=nullptr);
	PATELLA_ASSERT_VALUES(Position<=Size-Count, Position, Size-Count);
	memcpy(Buffer, &Data[Position], Count);
	Position += Count;
}

// 0x50b030, lines 387-388; the parameters as in TReadBuffer's constructor.
TWriteBuffer::TWriteBuffer(patella::TSpan<uint8> Bytes)
{
	uint8 *const data = Bytes.data();
	const int size = static_cast<int>(Bytes.size());
	PATELLA_ASSERT(data!=nullptr);
	PATELLA_ASSERT(size>=0);
	Size = size;
	Position = 0;
	Data = data;
}

// 0x50b2e0, line 400
void TWriteBuffer::PutByte(uint8 Data)
{
	PATELLA_ASSERT_VALUES(Position<=Size-1, Position, Size);
	this->Data[Position++] = Data;
}

// 0x50b450, line 410
void TWriteBuffer::PutWord(uint16 Data)
{
	PATELLA_ASSERT_VALUES(Position<=Size-2, Position, Size);
	this->Data[Position] = (uint8)Data;
	this->Data[Position + 1] = (uint8)(Data >> 8);
	Position += 2;
}

// 0x50b5e0, line 421
void TWriteBuffer::PutQuad(uint32 Data)
{
	PATELLA_ASSERT_VALUES(Position<=Size-4, Position, Size);
	this->Data[Position] = (uint8)Data;
	this->Data[Position + 1] = (uint8)(Data >> 8);
	this->Data[Position + 2] = (uint8)(Data >> 16);
	this->Data[Position + 3] = (uint8)(Data >> 24);
	Position += 4;
}

// 0x50b780, lines 437-441. It stays a C string (issue #59): a std::string_view could not carry the null pointer
// that line 437 reports (its construction from a null C string faults before the assertion).
void TWriteBuffer::PutString(const char *s)
{
	PATELLA_ASSERT(s!=nullptr);
	const int Length = (int)strlen(s);
	PutWord((uint16)Length);
	if (Length == 0)
		return;
	PATELLA_ASSERT_VALUES(Position<=Size-Length, Position, Size-Length);
	memcpy(&Data[Position], s, Length);
	Position += Length;
}

// 0x50ba00, lines 451-452
void TWriteBuffer::PutBytes(patella::TSpan<const uint8> Bytes)
{
	const uint8 *const Buffer = Bytes.data();
	const int Count = static_cast<int>(Bytes.size());
	PATELLA_ASSERT(Buffer!=nullptr);
	PATELLA_ASSERT_VALUES(Position<=Size-Count, Position, Size-Count);
	for (int i = 0; i < Count; i++)
		PutByte(Buffer[i]);
}

// 0x50bce0, lines 467-468. Buffer is the span's pointer, the name of the assertion's text (issue #69). The text is
// built where the original built it in the caller's buffer, in Text: at most 10 digits and 3 commas ("2,147,483,647")
// and the NUL. Copied into the caller's buffer it is the original's bytes (it wrote nothing after the final NUL), cut
// at the buffer's size where the original wrote past it (#57's contract; every caller's buffer holds 14).
void FormatNumber(int Number, patella::TSpan<char> BufferBytes)
{
	char *const Buffer = BufferBytes.data();
	PATELLA_ASSERT(Buffer!=nullptr);
	PATELLA_ASSERT_VALUES(Number>=0, Number);
	TFixedString<14> Text;
	Text.Format("{}", Number);
	for (int Digits = 3; Digits < (int)strlen(Text); Digits += 4) {
		char *Position = &Text[strlen(Text) - Digits];
		memmove(Position + 1, Position, Digits + 1);
		*Position = ',';
	}
	if (!BufferBytes.empty())
		TStringBuffer(Buffer, BufferBytes.size()).Assign(Text.View());
}
