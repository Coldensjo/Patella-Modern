// print.h: printf on stdout with std::format's language, for the client's one diagnostic line on stdout
// (ContentWindows.cpp's MouseHover), with a C++17 stand-in (the 32-bit oracle's /std:c++17, until issue #94) and the
// same call site, as compat/format.h is for std::format_to_n. Patella-Modern issue #64. Not part of the original
// source.
//
// patella::compat::PrintLine(Format, Values...) writes the text formatted with std::format's language and a '\n'
// to stdout, with printf's semantics: the conversions the client used give the same characters (%s: {}, %d: {}),
// stdout's own mode does the rest (the Microsoft C runtime's text mode writes "\r\n"), and a failed write is
// ignored. Not std::println: that throws std::system_error where the write fails (C++23 [print.fun]), and a GUI
// program like the client has no valid stdout unless it is redirected, so every call would throw inside the
// caller and end the client; on Windows it would also write UTF-16 to a console where printf wrote
// the bytes. The text comes from std::format where the library has it (PATELLA_HAS_FORMAT, the product presets),
// elsewhere from the stand-in of compat/format.h; FormatLine is the text alone (the unit tests compare the two).

#pragma once

#include "compat/format.h"

#include <cstddef> // IWYU pragma: keep (the stand-in's branch below)
#include <cstdio>
#include <string>
#include <string_view> // IWYU pragma: keep (the stand-in's branch below)
#include <utility>     // IWYU pragma: keep (std::format's branch below)

namespace patella::compat {

#if PATELLA_HAS_FORMAT
// The text PrintLine writes, '\n' included.
template <typename... Arguments>
std::string FormatLine(std::format_string<Arguments...> Format, Arguments &&...Values)
{
	std::string Text = std::format(Format, std::forward<Arguments>(Values)...);
	Text.push_back('\n');
	return Text;
}

template <typename... Arguments>
void PrintLine(std::format_string<Arguments...> Format, Arguments &&...Values)
{
	const std::string Text = FormatLine(Format, std::forward<Arguments>(Values)...);
	std::fwrite(Text.data(), 1, Text.size(), stdout);
}
#else
template <typename... Arguments>
std::string FormatLine(std::string_view Format, const Arguments &...Values)
{
	const std::ptrdiff_t Size = FormatToNStandIn(nullptr, 0, Format, Values...).Size;
	std::string Text(static_cast<std::size_t>(Size), '\0');
	FormatToNStandIn(Text.data(), Size, Format, Values...);
	Text.push_back('\n');
	return Text;
}

template <typename... Arguments>
void PrintLine(std::string_view Format, const Arguments &...Values)
{
	const std::string Text = FormatLine(Format, Values...);
	std::fwrite(Text.data(), 1, Text.size(), stdout);
}
#endif

} // namespace patella::compat
