// Utils.cpp: Latin-1 character and string helpers, outfit colours, and the
// packet buffers. Member and parameter names come from the assertion strings
// ("data", "size", "Position", "Size", "NewSize", "s", "s1", "Destination",
// "Source", "Text", "Buffer", "Number"); function and class names are ours.

#pragma once

#include "common.h"
#include "compat/span.h"

// Characters are Latin-1; the letter tests include the German umlauts and ß.
int IsSpace(int c);                                                  // 0x508fa0
int IsAlpha(int c);                                                  // 0x508fd0
int IsDigit(int c);                                                  // 0x509020
int ToLower(int c);                                                  // 0x509040
int ToUpper(int c);                                                  // 0x509060
void HSVToRGB(double H, double S, double V, int *R, int *G, int *B); // 0x509080
void GetOutfitColor(int Color, int *R, int *G, int *B);              // 0x509280, Color 0..132

void LowerString(char *s);                                                  // 0x5093d0
void CapitalizeString(char *s);                                             // 0x509540
int CompareStrings(const char *s1, const char *s2, int Count);              // 0x509710, caseless
void CopyString(patella::TSpan<char> DestinationBytes, const char *Source); // 0x509a60
void TrimString(char *Destination, const char *Source);                     // 0x509d10
bool ShortenString(char *Text, int MaxLength);                              // 0x50a040
void FormatNumber(int Number, patella::TSpan<char> BufferBytes);            // 0x50bce0, "1,234,567"
// CompareStrings and ShortenString keep the original's parameters: Count limits the comparison of two C strings (-1:
// none) and MaxLength is the length Text is cut to, not the size of a buffer (issue #69).

class TBuffer {
public:
	// Utils.h line 50 (inline, emitted at 0x4ca300).
	void SetSize(int NewSize)
	{
		PATELLA_ASSERT_VALUES(NewSize>=0, NewSize);
		Size = NewSize;
	}

	uint8 *Data;  // +0x0
	int Size;     // +0x4
	int Position; // +0x8
};

// Little-endian reads; strings are a u16 length followed by the characters.
class TReadBuffer : public TBuffer {
public:
	explicit TReadBuffer(patella::TSpan<uint8> Bytes); // 0x50a200 (data, size)
	uint8 GetByte();                                   // 0x50a4b0
	uint16 GetWord();                                  // 0x50a610
	uint32 GetQuad();                                  // 0x50a780
	void GetString(patella::TSpan<char> Text);         // 0x50a900 (Buffer, MaxLength); see Utils.cpp
	char *GetString();                                 // 0x50abb0, returns a new[] copy
	void GetBytes(patella::TSpan<uint8> Bytes);        // 0x50ad50 (Buffer, Count)
};

class TWriteBuffer : public TBuffer {
public:
	explicit TWriteBuffer(patella::TSpan<uint8> Bytes); // 0x50b030 (data, size)
	void PutByte(uint8 Data);                           // 0x50b2e0
	void PutWord(uint16 Data);                          // 0x50b450
	void PutQuad(uint32 Data);                          // 0x50b5e0
	void PutString(const char *s);                      // 0x50b780, a C string (see Utils.cpp)
	void PutBytes(patella::TSpan<const uint8> Bytes);   // 0x50ba00 (Buffer, Count)
};
