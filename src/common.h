// Shared definitions for the reconstructed Tibia 7.72 client.
//
// The error macros reproduce the client's error messages as seen in the
// binary. Their names are ours; the strings they produce (expression text,
// "assertion failed", "exception occurred", "unknown exception occurred")
// match the original's, the file and line are the site's own (issues #79, #80).

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
// PATELLA_CORE: the portable code of the patella_core library (cmake/Core.cmake, issue #7), built
// without MFC and the Windows headers (notes/modernization-plan.md, section 8).
#if defined(PATELLA_CORE)
#elif defined(TIBIA_MFC_SHIM)
#include "compat/mfc_shim.h"
#else
#include <afxwin.h>
#include <afxsock.h>
#endif
// <string>, <cstring>, <cstdio> and <cstdlib>: nothing of them is used here, and every file that uses them
// includes them itself (issue #40), but here they come before compat/msvc.h's C headers and TestHooks.h. Without
// them every translation unit declares these names in another order, and MSVC, which orders an object's globals
// by the names its translation unit declares (issue #43), places the test build's in another order (measured:
// PatellaTest.exe's .data moves; issue #49).
#include <cstdint>
#include <string> // IWYU pragma: keep
#include <type_traits>
#include <cstring> // IWYU pragma: keep
#include <cstdio>  // IWYU pragma: keep
#include <cstdlib> // IWYU pragma: keep
// The Microsoft CRT's and compiler's spellings behind portable names (issue #43).
#include "compat/msvc.h" // IWYU pragma: export
#if !defined(PATELLA_CORE)
// The test mode's hooks (issue #21): the system calls themselves outside the test build.
#include "TestHooks.h" // IWYU pragma: export
#endif

// The original's unsigned integer types, as aliases of the std types (Patella-Modern issue #35). They
// are the same types as before, not only the same sizes: std::uint8_t, std::uint16_t and std::uint32_t
// are unsigned char, unsigned short and unsigned int with MSVC (x86 and x64), mingw-w64 GCC, and GCC and
// Clang on Linux, so mangled names, overloads and the PDB's type names stay as they were.
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
static_assert(sizeof(uint8) == 1 && std::is_same_v<uint8, unsigned char>, "uint8 is unsigned char");
static_assert(sizeof(uint16) == 2 && std::is_same_v<uint16, unsigned short>, "uint16 is unsigned short");
static_assert(sizeof(uint32) == 4 && std::is_same_v<uint32, unsigned int>, "uint32 is unsigned int");

// The original's structure layouts (its sizes and offsets, the static_asserts that check them) hold
// in the 32-bit build, where the oracle-x86 preset freezes them until M6; the x64 build has its
// natural layout, in which pointers and size_t are 8 bytes (notes/modernization-plan.md, 1.2 and
// section 2, rule 4).
#if defined(_M_IX86)
#define PATELLA_ORIGINAL_LAYOUT 1
#else
#define PATELLA_ORIGINAL_LAYOUT 0
#endif

// Range check used in many assertions, e.g. "In(CommandType,0,255)".
inline bool In(int Value, int Min, int Max)
{
	return Value >= Min && Value <= Max;
}

// Keeps *Value between Min and Max, in either order. The name is ours. The
// out-of-line copy 0x425920 is ContentWindows.obj's (TTradeWindowBody::SetSize
// inlines it); DialogElements.cpp calls it (0x45d8a3 and four more).
inline void Limit(int *Value, int Min, int Max)
{
	if (Min > Max) {
		if (*Value < Max)
			*Value = Max;
		if (*Value > Min)
			*Value = Min;
	} else {
		if (*Value < Min)
			*Value = Min;
		if (*Value > Max)
			*Value = Max;
	}
}

// Thrown by value (ThrowInfo at 0x57b3b4).
#include "TMyException.h" // IWYU pragma: export

// The client's second exception type: an error code thrown by value
// (ThrowInfo 0x57b3c4, ".?AW4PROGRAM_ERROR@@"). The enumerator names are
// unknown; values seen so far: 6 (AfxSocketInit failed, InitNetwork).
enum PROGRAM_ERROR : int {
	PROGRAM_ERROR_SOCKET_INIT = 6, // our name
};
static_assert(sizeof(PROGRAM_ERROR) == 4);
static_assert(std::is_same_v<std::underlying_type_t<PROGRAM_ERROR>, int>);

// The assertions: PATELLA_ASSERT and PATELLA_ASSERT_VALUES (Patella-Modern issue
// #79; until then three macros here, which named the original's line).
#include "compat/assertion.h" // IWYU pragma: export

// The error sites' exception (Patella-Modern issue #80): a TMyException with
// ErrorText's message ("<file> <line>: <Kind>[ (<V1's text> = <V1>)]...[,
// reason:\n<Reason>]") and the site's own source location, as PATELLA_ASSERT
// has since issue #79. Until issue #80 every site named the original's source
// line as its first argument (TERROR1(1274, "unknown packet type during game",
// Type)); the lines stay in git history, in the image and in the comments that
// name a function's lines. Since issue #76 difftest and the golden corpus
// compare the outcome only (thrown or not, and the kind), not the messages.
// TMYEXCEPTION's Reason is a text or a caught TMyException, whose message
// becomes the reason (the original's wrapping constructor).
#define TMYEXCEPTION(Kind, N1, V1, N2, V2, Reason) \
	::patella::MakeError(PATELLA_SOURCE_LOCATION(), Kind, N1, (int)(V1), N2, (int)(V2), Reason)

#define TERROR(Message) \
	throw TMYEXCEPTION(Message, "", 0, "", 0, "")
#define TERROR1(Message, V1) \
	throw TMYEXCEPTION(Message, #V1, V1, "", 0, "")
#define TERROR2(Message, V1, V2) \
	throw TMYEXCEPTION(Message, #V1, V1, #V2, V2, "")
// An error with a text as its reason (Script.cpp: the file name), in the
// place of an assertion's expression: "<Message>, reason:\n<Reason>".
#define TERROR_REASON(Message, Reason) \
	throw TMYEXCEPTION(Message, "", 0, "", 0, Reason)
#define TERROR1_REASON(Message, V1, Reason) \
	throw TMYEXCEPTION(Message, #V1, V1, "", 0, Reason)

// The original's catch wrappers: until Patella-Modern issue #78 most functions
// wrapped their body in a try block with three handlers, visible in the binary
// as catch blocks after each function: a PROGRAM_ERROR passed through
// unchanged, a TMyException was wrapped with the wrapper's line and up to two
// values ("exception occurred"), and anything else became "unknown exception
// occurred". A hand-written stack trace for error.txt (gone since issue #74);
// the function comments still name the wrappers' lines and values as line
// evidence. Since issue #78 an exception passes unchanged to the first handler
// that really handles it (notes/decisions.md, issue #78's section).
