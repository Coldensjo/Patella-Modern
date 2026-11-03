// crt71.h: behaviour of MSVC 7.1's CRT (libcmt.lib, linked into Tibia.exe) that the CRT Patella.exe links
// (the UCRT) does differently and that reaches a file the client writes. Not part of the original source.
//
// asctime/ctime: MSVC 7.1 writes the day of the month with two digits (asctime 0x517f6d:
// tm_mday / 10 + '0', tm_mday % 10 + '0'), "Mon Nov 02 12:14:46 2026\n"; the UCRT pads it with a space,
// "Mon Nov  2 12:14:46 2026\n", as the C standard asks. The client writes ctime's text into error.txt
// (Player.cpp HandleException) and saved channels (ConsoleWindow.cpp SaveChannel); found by
// tools/compare_files.py (issue #21). The other fields have the same form in both.

#pragma once

// The text ctime/asctime returned, in MSVC 7.1's form (changed in place; NULL stays NULL).
inline char *CRT71Time(char *Text)
{
	if (Text != nullptr && Text[8] == ' ')
		Text[8] = '0';
	return Text;
}
