// format.h: C++20's std::format_to_n for the code that formats into fixed buffers (types/FixedString.h), and a
// C++17 stand-in, written for the 32-bit oracle's /std:c++17 (which left the fork with issue #94) and kept for the
// libraries without std::format and tests/unit's stand-in build. Patella-Modern issue #57. Not part of the original.
//
// After this header PATELLA_HAS_FORMAT is 1 where the library has std::format (the product presets: MSVC
// /std:c++latest, GCC 16, Clang 23) and 0 elsewhere (a C++17 library) or with PATELLA_FORMAT_STANDIN, which
// tests/unit's stand-in build defines for a whole target (never for one file), so that the stand-in runs the
// tests std::format passes, on the same compiler.
//
// patella::compat::FormatToNStandIn(Out, Count, Format, Values...) is std::format_to_n(Out, Count, Format,
// Values...) for the part of the format language the client uses, with the same output: it writes at most Count
// characters (none for a Count below 1, no terminating NUL) and returns where it stopped and the length of the
// whole text. The digits come from std::to_chars, as in the standard libraries' std::format. What it formats:
//	- replacement fields with automatic numbering ("{}", "{:...}"), "{{" and "}}";
//	- the standard format specification [[fill]align][sign]['0'][width]['.' precision][type], with the types
//	  d, x, X (integers), f (floating point), s (strings, bool), c (char) or none;
//	- the arguments: bool, char, the signed and unsigned integer types (unsigned char and signed char as
//	  numbers, as std::format does), float, double, const char * and char * (a C string), std::string_view and
//	  std::string, and any type with a View() that returns a std::string_view (TFixedString).
// What it does not: manual argument numbers, nested width or precision ("{:{}}"), '#', 'L', the other types
// (b, o, e, g, a, p, ?), a char formatted as a number, and display widths beyond bytes (a width or precision
// on a string counts bytes; std::format counts the display width of UTF-8, the same for ASCII). These throw
// patella::compat::TFormatError where std::format would not, and so does every format std::format_to_n
// rejects at compile time on the product presets (which therefore never reach the stand-in with it).

#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <version> // IWYU pragma: keep (the feature-test macro of the #if line)

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L && !defined(PATELLA_FORMAT_STANDIN)
#define PATELLA_HAS_FORMAT 1
#include <format> // IWYU pragma: export
#else
#define PATELLA_HAS_FORMAT 0
#endif

namespace patella::compat {

// std::format_to_n_result<char *>.
struct TFormatToNResult {
	char *Out;           // after the last character written
	std::ptrdiff_t Size; // the length of the whole text
};

// A format the stand-in does not format (std::format_error where std::format rejects it too).
class TFormatError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

namespace detail {

enum class TArgumentKind : unsigned char {
	None,
	Bool,
	Char,
	Signed,
	Unsigned,
	Float,
	Double,
	String,
};

struct TArgument {
	TArgumentKind Kind = TArgumentKind::None;
	bool Bool = false;
	char Char = 0;
	long long Signed = 0;
	unsigned long long Unsigned = 0;
	float Float = 0.0F;
	double Double = 0.0;
	std::string_view String;
};

// Whether T has a View() that returns a std::string_view (TFixedString).
template <typename T, typename = void>
struct THasView : std::false_type {};
template <typename T>
struct THasView<T, std::void_t<decltype(std::declval<const T &>().View())>>
	: std::is_same<decltype(std::declval<const T &>().View()), std::string_view> {};

template <typename>
inline constexpr bool AlwaysFalse = false;

template <typename T>
TArgument MakeArgument(const T &Value)
{
	using Type = std::remove_cv_t<T>;
	TArgument Argument;
	if constexpr (std::is_same_v<Type, bool>) {
		Argument.Kind = TArgumentKind::Bool;
		Argument.Bool = Value;
	} else if constexpr (std::is_same_v<Type, char>) {
		Argument.Kind = TArgumentKind::Char;
		Argument.Char = Value;
	} else if constexpr (std::is_same_v<Type, wchar_t> || std::is_same_v<Type, char16_t>
		|| std::is_same_v<Type, char32_t>) {
		static_assert(AlwaysFalse<T>, "std::format<char> does not format wide characters");
	} else if constexpr (std::is_integral_v<Type> && std::is_signed_v<Type>) {
		Argument.Kind = TArgumentKind::Signed;
		Argument.Signed = Value;
	} else if constexpr (std::is_integral_v<Type>) {
		Argument.Kind = TArgumentKind::Unsigned;
		Argument.Unsigned = Value;
	} else if constexpr (std::is_same_v<Type, float>) {
		Argument.Kind = TArgumentKind::Float;
		Argument.Float = Value;
	} else if constexpr (std::is_same_v<Type, double>) {
		Argument.Kind = TArgumentKind::Double;
		Argument.Double = Value;
	} else if constexpr (THasView<Type>::value) {
		Argument.Kind = TArgumentKind::String;
		Argument.String = Value.View();
	} else if constexpr (std::is_convertible_v<const T &, std::string_view>) {
		Argument.Kind = TArgumentKind::String;
		Argument.String = Value;
	} else {
		static_assert(AlwaysFalse<T>, "a type the stand-in of std::format_to_n does not format");
	}
	return Argument;
}

// [[fill]align][sign]['0'][width]['.' precision][type]
struct TSpecification {
	char Fill = ' ';
	char Align = 0; // '<', '>', '^' or 0 (the type's default)
	char Sign = 0;  // '+', '-', ' ' or 0
	bool Zero = false;
	std::size_t Width = 0;
	bool HasPrecision = false;
	std::size_t Precision = 0;
	char Type = 0;
};

// The output: at most Limit characters, the length of all of them.
class TOutput {
public:
	TOutput(char *Buffer, std::ptrdiff_t Count) : Out(Buffer), Limit(Count < 0 ? 0 : Count) {}

	void Put(char Character)
	{
		if (Size < Limit)
			Out[Size] = Character;
		Size++;
	}
	void Put(std::string_view Text)
	{
		for (const char Character : Text)
			Put(Character);
	}
	void Repeat(char Character, std::size_t Count)
	{
		for (std::size_t i = 0; i < Count; i++)
			Put(Character);
	}
	[[nodiscard]] TFormatToNResult Result() const { return {Out + (Size < Limit ? Size : Limit), Size}; }

private:
	char *Out;
	std::ptrdiff_t Limit;
	std::ptrdiff_t Size = 0;
};

inline bool IsDigit(char Character)
{
	return Character >= '0' && Character <= '9';
}

inline bool IsAlign(char Character)
{
	return Character == '<' || Character == '>' || Character == '^';
}

inline std::size_t ParseNumber(std::string_view Text, std::size_t &Position)
{
	std::size_t Value = 0;
	while (Position < Text.size() && IsDigit(Text[Position])) {
		Value = Value * 10 + static_cast<std::size_t>(Text[Position] - '0');
		if (Value > 0xFFFF)
			throw TFormatError("a width or precision the stand-in of std::format_to_n does not take");
		Position++;
	}
	return Value;
}

// The specification between ':' and '}' (Text without them).
inline TSpecification ParseSpecification(std::string_view Text)
{
	TSpecification Specification;
	std::size_t Position = 0;
	if (Text.size() >= 2 && IsAlign(Text[1]) && Text[0] != '{' && Text[0] != '}') {
		Specification.Fill = Text[0];
		Specification.Align = Text[1];
		Position = 2;
	} else if (!Text.empty() && IsAlign(Text[0])) {
		Specification.Align = Text[0];
		Position = 1;
	}
	if (Position < Text.size() && (Text[Position] == '+' || Text[Position] == '-' || Text[Position] == ' '))
		Specification.Sign = Text[Position++];
	if (Position < Text.size() && Text[Position] == '0') {
		Specification.Zero = true;
		Position++;
	}
	Specification.Width = ParseNumber(Text, Position);
	if (Position < Text.size() && Text[Position] == '.') {
		Position++;
		if (Position >= Text.size() || !IsDigit(Text[Position]))
			throw TFormatError("a precision without digits");
		Specification.HasPrecision = true;
		Specification.Precision = ParseNumber(Text, Position);
	}
	if (Position < Text.size()) {
		const char Type = Text[Position++];
		if (Type != 'd' && Type != 'x' && Type != 'X' && Type != 'f' && Type != 's' && Type != 'c')
			throw TFormatError("a format specification the stand-in of std::format_to_n does not take");
		Specification.Type = Type;
	}
	if (Position != Text.size())
		throw TFormatError("a format specification the stand-in of std::format_to_n does not take");
	return Specification;
}

// Text padded to the width: Align (or Default) decides where the fill goes.
inline void PutPadded(TOutput &Output, const TSpecification &Specification, char Default, std::string_view Text)
{
	const std::size_t Padding = Specification.Width > Text.size() ? Specification.Width - Text.size() : 0;
	const char Align = Specification.Align != 0 ? Specification.Align : Default;
	std::size_t Before = 0;
	if (Align == '>')
		Before = Padding;
	else if (Align == '^')
		Before = Padding / 2;
	Output.Repeat(Specification.Fill, Before);
	Output.Put(Text);
	Output.Repeat(Specification.Fill, Padding - Before);
}

// A number's text (Sign: its sign, empty or one character; Digits: the rest), padded: with '0' and no
// alignment the zeros go between the sign and the digits (Zero: false for infinity and NaN).
inline void PutNumber(TOutput &Output, const TSpecification &Specification, std::string_view Sign,
	std::string_view Digits, bool Zero)
{
	if (Specification.Zero && Zero && Specification.Align == 0) {
		const std::size_t Length = Sign.size() + Digits.size();
		Output.Put(Sign);
		Output.Repeat('0', Specification.Width > Length ? Specification.Width - Length : 0);
		Output.Put(Digits);
		return;
	}
	std::string Text(Sign);
	Text.append(Digits);
	PutPadded(Output, Specification, '>', Text);
}

inline std::string_view SignOf(bool Negative, char Sign)
{
	if (Negative)
		return "-";
	if (Sign == '+')
		return "+";
	if (Sign == ' ')
		return " ";
	return {};
}

inline void PutInteger(TOutput &Output, const TSpecification &Specification, bool Negative,
	unsigned long long Magnitude)
{
	if (Specification.HasPrecision)
		throw TFormatError("a precision for an integer");
	int Base = 10;
	if (Specification.Type == 'x' || Specification.Type == 'X')
		Base = 16;
	else if (Specification.Type != 0 && Specification.Type != 'd')
		throw TFormatError("a type the stand-in of std::format_to_n does not take for an integer");
	std::array<char, 64> Digits = {};
	const std::to_chars_result Result = std::to_chars(Digits.data(), Digits.data() + Digits.size(), Magnitude, Base);
	const auto Length = static_cast<std::size_t>(Result.ptr - Digits.data());
	if (Specification.Type == 'X') {
		for (std::size_t i = 0; i < Length; i++)
			if (Digits[i] >= 'a' && Digits[i] <= 'f')
				Digits[i] = static_cast<char>(Digits[i] - 'a' + 'A');
	}
	PutNumber(Output, Specification, SignOf(Negative, Specification.Sign), std::string_view(Digits.data(), Length), true);
}

template <typename Floating>
void PutFloating(TOutput &Output, const TSpecification &Specification, Floating Value)
{
	if (Specification.Type != 0 && Specification.Type != 'f')
		throw TFormatError("a type the stand-in of std::format_to_n does not take for floating point");
	std::array<char, 512> Digits = {};
	std::to_chars_result Result{};
	if (Specification.Type == 'f') {
		const int Precision = Specification.HasPrecision ? static_cast<int>(Specification.Precision) : 6;
		Result = std::to_chars(Digits.data(), Digits.data() + Digits.size(), Value, std::chars_format::fixed, Precision);
	} else if (Specification.HasPrecision) {
		Result = std::to_chars(Digits.data(), Digits.data() + Digits.size(), Value, std::chars_format::general,
			static_cast<int>(Specification.Precision));
	} else {
		Result = std::to_chars(Digits.data(), Digits.data() + Digits.size(), Value);
	}
	if (Result.ec != std::errc())
		throw TFormatError("a floating-point number longer than the stand-in's buffer");
	std::string_view Text(Digits.data(), static_cast<std::size_t>(Result.ptr - Digits.data()));
	const bool Negative = !Text.empty() && Text[0] == '-';
	if (Negative)
		Text.remove_prefix(1);
	const bool Finite = !Text.empty() && IsDigit(Text[0]);
	PutNumber(Output, Specification, SignOf(Negative, Specification.Sign), Text, Finite);
}

inline void PutString(TOutput &Output, const TSpecification &Specification, std::string_view Text)
{
	if (Specification.Sign != 0 || Specification.Zero)
		throw TFormatError("a sign or '0' for a string");
	if (Specification.HasPrecision && Specification.Precision < Text.size())
		Text = Text.substr(0, Specification.Precision);
	PutPadded(Output, Specification, '<', Text);
}

inline void PutArgument(TOutput &Output, std::string_view SpecificationText, const TArgument &Argument)
{
	const TSpecification Specification = ParseSpecification(SpecificationText);
	switch (Argument.Kind) {
	case TArgumentKind::None: throw TFormatError("a replacement field without an argument");
	case TArgumentKind::Bool:
		if (Specification.Type != 0 && Specification.Type != 's')
			throw TFormatError("a type the stand-in of std::format_to_n does not take for bool");
		PutString(Output, Specification, Argument.Bool ? "true" : "false");
		return;
	case TArgumentKind::Char:
		if (Specification.Type != 0 && Specification.Type != 'c')
			throw TFormatError("a char as a number, which the standard libraries format differently");
		PutString(Output, Specification, std::string_view(&Argument.Char, 1));
		return;
	case TArgumentKind::Signed: {
		const bool Negative = Argument.Signed < 0;
		// The magnitude without overflow for the most negative value.
		const unsigned long long Magnitude = Negative ? 0ULL - static_cast<unsigned long long>(Argument.Signed)
													  : static_cast<unsigned long long>(Argument.Signed);
		PutInteger(Output, Specification, Negative, Magnitude);
		return;
	}
	case TArgumentKind::Unsigned: PutInteger(Output, Specification, false, Argument.Unsigned); return;
	case TArgumentKind::Float: PutFloating(Output, Specification, Argument.Float); return;
	case TArgumentKind::Double: PutFloating(Output, Specification, Argument.Double); return;
	case TArgumentKind::String:
		if (Specification.Type != 0 && Specification.Type != 's')
			throw TFormatError("a type the stand-in of std::format_to_n does not take for a string");
		PutString(Output, Specification, Argument.String);
		return;
	}
}

inline TFormatToNResult FormatArguments(char *Out, std::ptrdiff_t Count, std::string_view Format,
	const TArgument *Arguments, std::size_t ArgumentCount)
{
	TOutput Output(Out, Count);
	std::size_t Next = 0;
	std::size_t Position = 0;
	while (Position < Format.size()) {
		const char Character = Format[Position];
		if (Character == '}') {
			if (Position + 1 >= Format.size() || Format[Position + 1] != '}')
				throw TFormatError("an unmatched '}' in the format");
			Output.Put('}');
			Position += 2;
			continue;
		}
		if (Character != '{') {
			Output.Put(Character);
			Position++;
			continue;
		}
		if (Position + 1 < Format.size() && Format[Position + 1] == '{') {
			Output.Put('{');
			Position += 2;
			continue;
		}
		const std::size_t End = Format.find('}', Position + 1);
		if (End == std::string_view::npos)
			throw TFormatError("an unmatched '{' in the format");
		std::string_view Field = Format.substr(Position + 1, End - Position - 1);
		if (Field.find('{') != std::string_view::npos)
			throw TFormatError("a nested replacement field, which the stand-in does not take");
		if (!Field.empty() && Field[0] != ':')
			throw TFormatError("an argument number, which the stand-in does not take");
		if (!Field.empty())
			Field.remove_prefix(1);
		if (Next >= ArgumentCount)
			throw TFormatError("more replacement fields than arguments");
		PutArgument(Output, Field, Arguments[Next++]);
		Position = End + 1;
	}
	return Output.Result();
}

} // namespace detail

// std::format_to_n(Out, Count, Format, Values...) for the client's formats (above).
template <typename... Arguments>
TFormatToNResult FormatToNStandIn(char *Out, std::ptrdiff_t Count, std::string_view Format,
	const Arguments &...Values)
{
	if constexpr (sizeof...(Arguments) == 0) {
		return detail::FormatArguments(Out, Count, Format, nullptr, 0);
	} else {
		const std::array<detail::TArgument, sizeof...(Arguments)> Converted = {detail::MakeArgument(Values)...};
		return detail::FormatArguments(Out, Count, Format, Converted.data(), sizeof...(Arguments));
	}
}

} // namespace patella::compat
