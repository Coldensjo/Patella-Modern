// sysinc.cpp: reconstructed from Tibia.exe 7.72 (0x4fcb90-0x4fd43f).
//
// System helpers: the millisecond clock, file checks, the PROGRAM_ERROR
// texts, the clipboard with its buffer, and half of the encoded window title
// and class name. The functions without line
// evidence (the gap after SurfaceGL.cpp, 0x4fcb90-0x4fcd4f) come first, in
// binary order; they use only this file's data (0x71dbc4-0x71dbcc). The
// asserted functions follow in line order. 0x4fcd50-0x4fce4f is ATL header
// code kept in this object (code page lookup, AtlThrow, a free list), not
// part of the source; names.tsv lists it as kind stl.

#include "sysinc.h"
#include <io.h>
#include <mmsystem.h>
#include "common.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "winmm.lib")

// The other half of Player.cpp's XOR-encoded window title ("Tibia   ") and
// class name ("TibiaClient"); the keys are Player.cpp's. First in this
// object's .rdata, before the PROGRAM_ERROR texts (owner: notes/decisions.md,
// "Unknown-owner data (issue #12)"; low).
const int TitleCode[8] = {68, 75, 129, 81, 98, 78, 96, 141};                         // 0x5733a4
const int ClassNameCode[11] = {57, 193, 225, 58, 140, 118, 195, 214, 239, 244, 137}; // 0x5733c4

// Data, in .bss order.
static char *ClipboardBuffer;                        // 0x71dbc4
static unsigned int ClipboardBufferSize;             // 0x71dbc8
static int StartTime = (int)PATELLA_TIME_GET_TIME(); // 0x71dbcc, initialiser at 0x55a130

// 0x4fcb90
int GetMilliseconds()
{
	return (int)PATELLA_TIME_GET_TIME() - StartTime;
}

// 0x4fcba0
bool FileExists(const char *FileName)
{
	return _access(FileName, 0) == 0;
}

// 0x4fcbc0
bool RemoveFile(const char *FileName)
{
	return remove(FileName) == 0;
}

// 0x4fcbe0. True when the clipboard has no text.
bool IsClipboardEmpty()
{
	return !IsClipboardFormatAvailable(CF_TEXT);
}

// 0x4fcbf0. What failed, for a PROGRAM_ERROR code (Player.cpp).
const char *GetErrorText(int Error)
{
	switch (Error) {
	case 1:
		return "Cannot find file \"Tibia.dat\".";
	case 2:
		return "Cannot find file \"Tibia.spr\".";
	case 3:
		return "Cannot find file \"Tibia.pic\".";
	case 4:
		return "Cannot open file \"Tibia.spr\".";
	case 5:
		return "Cannot open file \"Tibia.pic\".";
	case 6:
		return "Cannot initialize Windows Sockets.";
	case 7:
		return "Cannot initialize DirectX.";
	case 8:
		return "Too low color depth detected.";
	case 9:
	case 10:
	case 11:
		return "Cannot initialize video graphics mode.";
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
		return "Video surface allocation failed.";
	default:
		return "Unknown error.";
	}
}

// 0x4fcc90. What to do about it.
const char *GetErrorHint(int Error)
{
	switch (Error) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		return "Please re-install the program.";
	case 6:
		return "Please check the network settings of your system.";
	case 7:
		return "Please install DirectX version 5.0 or later.";
	case 8:
		return "Please switch to a graphics mode with a color depth of at least 16 bit.";
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
		return "Please choose another video graphics mode.";
	default:
		return "Please submit a detailed bugreport including the error code number to cip@tibia.com.";
	}
}

// 0x4fcd00. Also seeds rand.
void InitSysinc()
{
	ClipboardBuffer = new char[1024];
	ClipboardBufferSize = 1024;
	srand(PATELLA_TIME_GET_TIME());
}

// 0x4fcd30. The buffer comes from new but is released with free.
void ExitSysinc()
{
	free(ClipboardBuffer);
	ClipboardBuffer = nullptr;
}

// 0x4fce50, line 63. Grows the buffer, keeping its contents.
void ResizeClipboardBuffer(unsigned int Size)
{
	if (ClipboardBufferSize < Size) {
		char *help = new char[Size];
		TASSERT(63, help!=NULL);
		memcpy(help, ClipboardBuffer, ClipboardBufferSize);
		free(ClipboardBuffer);
		ClipboardBufferSize = Size;
		ClipboardBuffer = help;
	}
}

// 0x4fcff0, line 104. The clipboard text in the buffer with CRs removed and
// line breaks turned into spaces; NULL when the clipboard cannot be opened.
// Odd behaviour kept: each removed CR moves Size-i bytes from i+1, one byte
// past the clipboard data.
char *GetClipboardText()
{
	if (!OpenClipboard(nullptr))
		return nullptr;
	HANDLE Memory = GetClipboardData(CF_TEXT);
	const SIZE_T Size = GlobalSize(Memory);
	ResizeClipboardBuffer((unsigned int)Size);
	const char *Data = (char *)GlobalLock(Memory);
	if (Data == nullptr) {
		ClipboardBuffer[0] = 0;
	} else {
		memcpy(ClipboardBuffer, Data, Size);
		const BOOL result = GlobalUnlock(Memory);
		TASSERT(104, result!=0 || GetLastError()==NO_ERROR);
	}
	CloseClipboard();
	for (SIZE_T i = 0; i < Size; i++) {
		if (ClipboardBuffer[i] == '\r')
			memmove(&ClipboardBuffer[i], &ClipboardBuffer[i + 1], Size - i);
		if (ClipboardBuffer[i] == '\n')
			ClipboardBuffer[i] = ' ';
		if (ClipboardBuffer[i] == 0)
			break;
	}
	return ClipboardBuffer;
}

// 0x4fd1b0, lines 139-143
bool SetClipboardText(const char *Text)
{
	const size_t Length = strlen(Text);
	if (!OpenClipboard(nullptr))
		return false;
	HGLOBAL Memory;
	if (EmptyClipboard() && (Memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, Length + 1)) != nullptr) {
		char *textdata = (char *)GlobalLock(Memory);
		TASSERT(139, textdata!=NULL);
		memcpy(textdata, Text, Length);
		textdata[Length] = 0;
		const BOOL result = GlobalUnlock(Memory);
		TASSERT2(143, result==0 || GetLastError()==NO_ERROR, result, GetLastError());
		if (SetClipboardData(CF_TEXT, Memory) != nullptr) {
			CloseClipboard();
			return true;
		}
	}
	CloseClipboard();
	return false;
}
