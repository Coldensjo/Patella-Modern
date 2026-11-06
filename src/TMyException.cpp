// TMyException.cpp: reconstructed from Tibia.exe 7.72 (0x508300-0x508968).
// The code between 0x508270 and 0x508390 that is not listed here is STL
// (std::string::find_last_of and the substring constructor), instantiated in
// this object.

#include "TMyException.h"
#include "compat/assertion.h"
#include <sstream>
#include <ostream>
#include <string>

namespace {

// 0x508390. The part of a path after the last '/' or '\'.
std::string GetFileName(const std::string &Path)
{
	const std::string::size_type Separator = Path.find_last_of("/\\");
	if (Separator != std::string::npos && Separator + 1 < Path.size())
		return std::string(Path, Separator + 1, Path.size() - Separator);
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

// 0x508400
TMyException::TMyException(const std::string &File, int Line, const std::string &Kind,
	const std::string &Name1, int Value1, const std::string &Name2, int Value2,
	const std::string &Expression)
{
	std::stringstream Text;
	Text << GetFileName(File) << " " << Line;
	Origin = Text.str();
	Text << ": " << Kind;
	if (Name1.size() != 0)
		Text << " (" << Name1 << " = " << Value1 << ")";
	if (Name2.size() != 0)
		Text << " (" << Name2 << " = " << Value2 << ")";
	if (Expression.size() != 0)
		Text << ", reason:" << std::endl
			 << Expression;
	Message = Text.str();
}

// 0x5086a0
TMyException::TMyException(const std::string &File, int Line, const std::string &Kind,
	const std::string &Name1, int Value1, const std::string &Name2, int Value2,
	const TMyException &Inner)
{
	std::stringstream Text;
	Text << GetFileName(File) << " " << Line << ": " << Kind;
	if (Name1.size() != 0)
		Text << " (" << Name1 << " = " << Value1 << ")";
	if (Name2.size() != 0)
		Text << " (" << Name2 << " = " << Value2 << ")";
	Text << ", reason:" << std::endl
		 << Inner.GetMessage();
	Message = Text.str();
	Origin = Inner.GetOrigin();
}

// The failures of PATELLA_ASSERT and PATELLA_ASSERT_VALUES (compat/assertion.h, issue #79): the TMyException of
// common.h's assertion macros before it, with the assertion's own file and line.
namespace patella {

void AssertionFailed(const char *Expression, const TSourceLocation &Location)
{
	throw TMyException(Location.file_name(), GetLine(Location), "assertion failed", "", 0, "", 0, Expression);
}

void AssertionFailed(const char *Expression, const TSourceLocation &Location, const char *Names, int Value1)
{
	throw TMyException(Location.file_name(), GetLine(Location), "assertion failed", GetValueName(Names, 0), Value1, "",
		0, Expression);
}

void AssertionFailed(const char *Expression, const TSourceLocation &Location, const char *Names, int Value1, int Value2)
{
	throw TMyException(Location.file_name(), GetLine(Location), "assertion failed", GetValueName(Names, 0), Value1,
		GetValueName(Names, 1), Value2, Expression);
}

} // namespace patella
