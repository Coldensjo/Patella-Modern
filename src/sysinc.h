// sysinc.h: system helpers (time, files, the clipboard, error texts).
//
// Names from the binary: GetMilliseconds and RemoveFile were named before
// from their use; textdata and help come from the assertions. The rest are
// ours.

#pragma once

#include "common.h"

int GetMilliseconds();
bool FileExists(const char *FileName);
bool RemoveFile(const char *FileName);
bool IsClipboardEmpty();
const char *GetErrorText(int Error);
const char *GetErrorHint(int Error);
void InitSysinc();
void ExitSysinc();
void ResizeClipboardBuffer(unsigned int Size);
char *GetClipboardText();
bool SetClipboardText(const char *Text);

// One half of the XOR-encoded window title and class name; Player.cpp has the
// keys and decodes them.
extern const int TitleCode[8];      // 0x5733a4
extern const int ClassNameCode[11]; // 0x5733c4
