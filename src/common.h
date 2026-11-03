// Shared definitions for the reconstructed Tibia 7.72 client.
//
// The error macros reproduce the client's error-trace pattern as seen in the
// binary. Their names are ours; the strings they produce (expression text,
// "assertion failed", "exception occurred", "unknown exception occurred",
// file and line) match the original.

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
#include <cstdint>
#include <string>
#include <type_traits>
#include <cstring>
#include <cstdio>
#include <cstdlib>
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

// Every site names the original's source line as its first argument, the line
// the binary pushes for that site: TASSERT(66, s!=NULL), TEND1(874, Obj).
// tools/lines.py reads the lines from Tibia.exe and checks every site against
// them. The file is __FILE__ (the messages name it without its path).
#define TMYEXCEPTION(Line, Kind, N1, V1, N2, V2, Last) \
	TMyException(__FILE__, Line, Kind, N1, (int)(V1), N2, (int)(V2), Last)

// Assertions record the expression and up to two values with their source text.
#define TASSERT(Line, Expr) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", "", 0, "", 0, #Expr)
#define TASSERT1(Line, Expr, V1) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, "", 0, #Expr)
#define TASSERT2(Line, Expr, V1, V2) \
	if (!(Expr)) throw TMYEXCEPTION(Line, "assertion failed", #V1, V1, #V2, V2, #Expr)

#define TERROR(Line, Message) \
	throw TMYEXCEPTION(Line, Message, "", 0, "", 0, "")
#define TERROR1(Line, Message, V1) \
	throw TMYEXCEPTION(Line, Message, #V1, V1, "", 0, "")
#define TERROR2(Line, Message, V1, V2) \
	throw TMYEXCEPTION(Line, Message, #V1, V1, #V2, V2, "")
// An error with a text as its reason (Script.cpp: the file name), in the
// place of an assertion's expression: "<Message>, reason:\n<Reason>".
#define TERROR_REASON(Line, Message, Reason) \
	throw TMYEXCEPTION(Line, Message, "", 0, "", 0, Reason)
#define TERROR1_REASON(Line, Message, V1, Reason) \
	throw TMYEXCEPTION(Line, Message, #V1, V1, "", 0, Reason)

// Most functions wrap their body in TBEGIN ... TEND. The three handlers are
// visible in the binary as catch blocks after each function: a PROGRAM_ERROR
// passes through unchanged, a TMyException is wrapped with the listed
// parameters, and anything else becomes "unknown exception occurred". Both
// messages name the TEND's line.
#define TBEGIN try {
// clang-format off
#define TEND_HANDLERS(Line, N1, V1, N2, V2) \
	} catch (PROGRAM_ERROR Error_) { \
		throw Error_; \
	} catch (TMyException &Exception_) { \
		throw TMYEXCEPTION(Line, "exception occurred", N1, V1, N2, V2, Exception_); \
	} catch (...) { \
		throw TMYEXCEPTION(Line, "unknown exception occurred", N1, V1, N2, V2, ""); \
	}
#define TEND(Line) TEND_HANDLERS(Line, "", 0, "", 0)
#define TEND1(Line, V1) TEND_HANDLERS(Line, #V1, V1, "", 0)
#define TEND2(Line, V1, V2) TEND_HANDLERS(Line, #V1, V1, #V2, V2)
// clang-format on
