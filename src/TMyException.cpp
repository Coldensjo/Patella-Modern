// TMyException.cpp: reconstructed from Tibia.exe 7.72 (0x508300-0x508968).
// The code between 0x508270 and 0x508390 that is not listed here is STL
// (std::string::find_last_of and the substring constructor), instantiated in
// this object. Since Patella-Modern issue #80 the constructors' formatting is
// ErrorText's (TMyException.h).

#include "TMyException.h"
#include "compat/assertion.h"
#include <exception>
#include <new>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

// 0x508390. The part of a path after the last '/' or '\'.
std::string_view GetFileName(std::string_view Path)
{
	const std::string_view::size_type Separator = Path.find_last_of("/\\");
	if (Separator != std::string_view::npos && Separator + 1 < Path.size())
		return Path.substr(Separator + 1, Path.size() - Separator);
	return Path;
}

// The text of the value at Index (0 or 1) in PATELLA_ASSERT_VALUES's stringized values: split at the top-level commas,
// without the spaces around it.
std::string GetValueName(const char *Names, int Index)
{
	const std::string Text(Names);
	std::string::size_type Start = 0;
	int Depth = 0;
	int Current = 0;
	for (std::string::size_type Position = 0; Position <= Text.size(); ++Position) {
		const char Character = Position < Text.size() ? Text[Position] : ',';
		if (Character == '(' || Character == '[' || Character == '{') {
			++Depth;
		} else if (Character == ')' || Character == ']' || Character == '}') {
			--Depth;
		} else if (Character == ',' && Depth == 0) {
			if (Current == Index) {
				const std::string::size_type First = Text.find_first_not_of(' ', Start);
				if (First == std::string::npos || First >= Position)
					return std::string();
				const std::string::size_type Last = Text.find_last_not_of(' ', Position - 1);
				return Text.substr(First, Last - First + 1);
			}
			++Current;
			Start = Position + 1;
		}
	}
	return std::string();
}

int GetLine(const patella::TSourceLocation &Location)
{
	return static_cast<int>(Location.line());
}

} // namespace

// 0x508300
TMyException::~TMyException() = default;

TMyException::TMyException(std::string Message_, const patella::TSourceLocation &Location_)
	: Message(std::move(Message_)), Location(Location_)
{
}

TMyException::TMyException(TError Error) : Message(std::move(Error.Message)), Location(Error.Location) {}

namespace patella {

// 0x508400's text
std::string ErrorText(std::string_view File, int Line, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, std::string_view Reason)
{
	std::stringstream Text;
	Text << GetFileName(File) << " " << Line << ": " << Kind;
	if (Name1.size() != 0)
		Text << " (" << Name1 << " = " << Value1 << ")";
	if (Name2.size() != 0)
		Text << " (" << Name2 << " = " << Value2 << ")";
	if (Reason.size() != 0)
		Text << ", reason:" << std::endl
			 << Reason;
	return Text.str();
}

// 0x5086a0's text
std::string ErrorText(std::string_view File, int Line, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, const TMyException &Inner)
{
	std::stringstream Text;
	Text << GetFileName(File) << " " << Line << ": " << Kind;
	if (Name1.size() != 0)
		Text << " (" << Name1 << " = " << Value1 << ")";
	if (Name2.size() != 0)
		Text << " (" << Name2 << " = " << Value2 << ")";
	Text << ", reason:" << std::endl
		 << Inner.GetMessage();
	return Text.str();
}

std::string ErrorOrigin(std::string_view File, int Line)
{
	std::stringstream Text;
	Text << GetFileName(File) << " " << Line;
	return Text.str();
}

std::string ErrorOrigin(const TMyException &Exception)
{
	return ErrorOrigin(Exception.GetLocation().file_name(), GetLine(Exception.GetLocation()));
}

TMyException MakeError(const TSourceLocation &Location, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, std::string_view Reason)
{
	return TMyException(ErrorText(Location.file_name(), GetLine(Location), Kind, Name1, Value1, Name2, Value2, Reason),
		Location);
}

TMyException MakeError(const TSourceLocation &Location, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, const TMyException &Inner)
{
	return TMyException(ErrorText(Location.file_name(), GetLine(Location), Kind, Name1, Value1, Name2, Value2, Inner),
		Location);
}

TError MakeFailure(const TSourceLocation &Location, std::string_view Kind, std::string_view Name1, int Value1,
	std::string_view Name2, int Value2, std::string_view Reason)
{
	return TError{ErrorText(Location.file_name(), GetLine(Location), Kind, Name1, Value1, Name2, Value2, Reason),
		Location};
}

// The failures of PATELLA_ASSERT and PATELLA_ASSERT_VALUES (compat/assertion.h, issue #79): the TMyException of
// common.h's assertion macros before it, with the assertion's own file and line.
void AssertionFailed(const char *Expression, const TSourceLocation &Location)
{
	throw MakeError(Location, "assertion failed", "", 0, "", 0, Expression);
}

void AssertionFailed(const char *Expression, const TSourceLocation &Location, const char *Names, int Value1)
{
	throw MakeError(Location, "assertion failed", GetValueName(Names, 0), Value1, "", 0, Expression);
}

void AssertionFailed(const char *Expression, const TSourceLocation &Location, const char *Names, int Value1, int Value2)
{
	throw MakeError(Location, "assertion failed", GetValueName(Names, 0), Value1, GetValueName(Names, 1), Value2,
		Expression);
}

// PATELLA_DIVISOR's failure (compat/assertion.h, issue #67): a divisor from input that is zero.
void DivisionByZero(const char *Expression, const TSourceLocation &Location)
{
	throw MakeError(Location, "integer division by zero", "", 0, "", 0, Expression);
}

void ThrowTopLevelError(const TSourceLocation &Location)
{
	try {
		throw;
	} catch (const TMyException &Exception) {
		throw MakeError(Location, "exception occurred", "", 0, "", 0, Exception);
	} catch (const std::bad_alloc &) {
		throw MakeError(Location, "unknown exception occurred", "", 0, "", 0, "");
	} catch (const std::out_of_range &) {
		throw MakeError(Location, "out_of_range_exception occurred", "", 0, "", 0, "");
	} catch (const std::exception &) {
		throw MakeError(Location, "exception occurred", "", 0, "", 0, "");
	} catch (const char *) {
		throw MakeError(Location, "const char* exception occurred", "", 0, "", 0, "");
	} catch (...) {
		throw MakeError(Location, "unknown exception occurred", "", 0, "", 0, "");
	}
}

} // namespace patella
