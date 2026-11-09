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
#include "BuildConfig.h"
#include "common.h"
#include "compat/msvc_io.h"
#include "platform/Platform.h"
#include <cstdio>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory off for the file:
// the original's malloc'd entries and texts, freed by their lists; the original's raw owners (notes/ownership.md):
// the window tree owns its children, the message queue its blocks, the lists their entries
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

// The other half of Player.cpp's XOR-encoded window title ("Tibia   ") and
// class name ("TibiaClient"); the keys are Player.cpp's. First in this
// object's .rdata, before the PROGRAM_ERROR texts (owner: notes/decisions.md,
// "Unknown-owner data (issue #12)"; low).
const std::array<int, 8> TitleCode = {68, 75, 129, 81, 98, 78, 96, 141};                         // 0x5733a4
const std::array<int, 11> ClassNameCode = {57, 193, 225, 58, 140, 118, 195, 214, 239, 244, 137}; // 0x5733c4

namespace {

// Data, in .bss order.
char *ClipboardBuffer;            // 0x71dbc4
unsigned int ClipboardBufferSize; // 0x71dbc8
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): the binary's initialiser (0x55a130) reads the clock before main; the platform's clock does not throw
int StartTime = (int)patella::platform::Platform().Clock.Milliseconds(); // 0x71dbcc, initialiser at 0x55a130

} // namespace

// 0x4fcb90
int GetMilliseconds()
{
	return (int)patella::platform::Platform().Clock.Milliseconds() - StartTime;
}

// 0x4fcba0
bool FileExists(const char *FileName)
{
	return MsvcAccess(FileName, 0) == 0;
}

// 0x4fcbc0
bool RemoveFile(const char *FileName)
{
	return std::remove(FileName) == 0;
}

// 0x4fcbe0. True when the clipboard has no text. Since Patella-Modern issue
// #112 the clipboard's text is its UTF-16 text (CF_UNICODETEXT; Windows makes
// it from CF_TEXT and CF_TEXT from it, so the one is there when the other is).
// Since issue #119 the clipboard is the platform's IClipboard (platform/Clipboard.h; Win32's: TWin32Clipboard).
bool IsClipboardEmpty()
{
	return !patella::platform::Platform().Clipboard.HasText();
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

namespace {

// The last hint names the bug report address (BuildConfig.h, issue #169).
constexpr const char *BugReportBefore = "Please submit a detailed bugreport including the error code number to ";
constexpr const char *BugReportAfter = ".";
constexpr auto BugReportHint = patella::buildconfig::ComposeText<patella::buildconfig::ComposedSize(BugReportBefore,
	patella::buildconfig::BugReportAddress, BugReportAfter)>(BugReportBefore, patella::buildconfig::BugReportAddress, BugReportAfter);

} // namespace

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
		return BugReportHint.data();
	}
}

// 0x4fcd00. Also seeds rand.
void InitSysinc()
{
	ClipboardBuffer = new char[1024];
	ClipboardBufferSize = 1024;
	srand(patella::platform::Platform().Clock.Milliseconds());
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
		PATELLA_ASSERT(help!=nullptr);
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
// Patella-Modern issue #112 (plan D4): the original read CF_TEXT, the text in
// the system's ANSI code page (or the code page of the input language that
// put it there); the client's text is Windows-1252 on every system, so the
// clipboard's UTF-16 text is converted (types/Cp1252.h: '?' for a character
// the code page does not have). Size is the converted text's with its NUL
// (the original's was the CF_TEXT memory's, at least that).
// Since issue #119 the clipboard's calls (OpenClipboard to CloseClipboard, the
// conversion with them) are the platform's IClipboard::Text (platform/
// Clipboard.h; Win32's: TWin32Clipboard, the same calls): the clipboard is
// closed before the buffer is filled, and a clipboard without text gives an
// empty text, Size 1 (the original's Size 0 and a NUL in the buffer: the same
// buffer).
// GCC's analyzer (Debug, issue #111): the original's read one byte past the text at a removed CR (above), kept.
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-out-of-bounds\"")
char *GetClipboardText()
{
	const std::optional<std::string> Text = patella::platform::Platform().Clipboard.Text();
	if (!Text.has_value())
		return nullptr;
	const std::size_t Size = Text->size() + 1;
	ResizeClipboardBuffer((unsigned int)Size);
	memcpy(ClipboardBuffer, Text->c_str(), Size);
	for (std::size_t i = 0; i < Size; i++) {
		if (ClipboardBuffer[i] == '\r')
			memmove(&ClipboardBuffer[i], &ClipboardBuffer[i + 1], Size - i);
		if (ClipboardBuffer[i] == '\n')
			ClipboardBuffer[i] = ' ';
		if (ClipboardBuffer[i] == 0)
			break;
	}
	return ClipboardBuffer;
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")

// 0x4fd1b0, lines 139-143. Since Patella-Modern issue #112 the text (the
// client's, Windows-1252) goes on the clipboard as UTF-16 (CF_UNICODETEXT),
// instead of the original's CF_TEXT, so that another program reads it as the client
// shows it whatever the system's ANSI code page and the input language.
// Since issue #119 the platform's IClipboard::SetText (TWin32Clipboard: the
// same calls).
bool SetClipboardText(const char *Text)
{
	return patella::platform::Platform().Clipboard.SetText(Text);
}
// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
