// TMyException.cpp: reconstructed from Tibia.exe 7.72 (0x508300-0x508968).
// The code between 0x508270 and 0x508390 that is not listed here is STL
// (std::string::find_last_of and the substring constructor), instantiated in
// this object.

#include "TMyException.h"
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
