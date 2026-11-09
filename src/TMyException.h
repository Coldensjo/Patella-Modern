// TMyException: the client's exception type, the only game class with RTTI.
// Implemented at 0x508300-0x508968, between Talk.cpp and Utils.cpp; the
// original file name is unknown ("TMyException" fits the alphabetical link
// order, which puts this file after Talk and before TRSA, TXTEA and Utils).
//
// Since Patella-Modern issue #80 (M5) a message and the source location of the
// site that threw it. The original's two constructors took the file, the line,
// the kind, two named values and a reason (a text, or a caught TMyException
// whose message became the reason and whose origin was kept); their formatting
// is ErrorText now, the message's text, and the origin ("<file> <line>" of the
// site that first raised the error) went with error.txt's report (issue #74),
// its only reader. The sites build the exception through common.h's
// TMYEXCEPTION and TERROR* and compat/assertion.h's PATELLA_ASSERT*, which pass
// their own location (notes/decisions.md, issue #80).

#pragma once

#include "compat/assertion.h"

#include <string>
#include <string_view>

// A recoverable failure (issue #80): what a TMyException would carry, returned in a patella::TExpected<T, TError>
// (compat/expected.h) instead of thrown. A function that the original exposes throws it on (TMyException(TError));
// its callers that recover test the result instead of catching (notes/decisions.md, issue #80, 80c).
struct TError {
	std::string Message;
	patella::TSourceLocation Location;
};

class TMyException {
public:
	TMyException(std::string Message, const patella::TSourceLocation &Location);
	// A returned failure thrown after all: the same message and location.
	explicit TMyException(TError Error);
	virtual ~TMyException(); // 0x508300; scalar deleting destructor at 0x404570

	// Inline; the original's GetMessage was emitted at 0x4042f0 and 0x4d9110.
	[[nodiscard]] std::string GetMessage() const { return Message; }
	[[nodiscard]] const patella::TSourceLocation &GetLocation() const { return Location; }

private:
	std::string Message;
	patella::TSourceLocation Location;
};

namespace patella {

// The text of the original's constructor 0x508400: "<file> <line>: <Kind>[ (<Name1> = <Value1>)][ (<Name2> =
// <Value2>)][, reason:\n<Reason>]", the file without its path, each part only when its text is not empty.
std::string ErrorText(std::string_view File, int Line, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, std::string_view Reason);
// The text of the wrapping constructor 0x5086a0: the same, with the caught exception's message as the reason, which
// is always appended (", reason:\n" also before an empty message).
std::string ErrorText(std::string_view File, int Line, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, const TMyException &Inner);
// The original's origin, "<file> <line>", the file without its path (for the tests' comparison with the original's
// constructors and the failure reports).
std::string ErrorOrigin(std::string_view File, int Line);
// The origin of an exception's location.
std::string ErrorOrigin(const TMyException &Exception);

// TMYEXCEPTION's exception: ErrorText's message with the site's file and line, and the site's location.
TMyException MakeError(const TSourceLocation &Location, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, std::string_view Reason);
TMyException MakeError(const TSourceLocation &Location, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, const TMyException &Inner);
// TUNEXPECTED_REASON's failure: MakeError's message and location as a TError.
TError MakeFailure(const TSourceLocation &Location, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, std::string_view Reason);

// The original's handlers around InitInstance and Run (Player.cpp, after its PROGRAM_ERROR handler), in one place since
// issue #159. Called inside a catch; throws the TMyException, for HandleException, that the exception being handled
// becomes, with Location as the site. Its texts are the original's. An out-of-memory error, std::bad_alloc here, says
// "unknown exception occurred": the original's operator new was MFC's (nafxcw.lib, 0x5237e7), whose handler threw a
// CMemoryException *, which only the original's catch (...) took. Its "bad_alloc_exception occurred" was never that error's.
[[noreturn]] void ThrowTopLevelError(const TSourceLocation &Location);

} // namespace patella
