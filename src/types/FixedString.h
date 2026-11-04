// FixedString.h: TFixedString<N>, the client's fixed text buffers (char[N]) with bounded writes, and
// TStringBuffer, the same writes into a buffer the code has only a pointer to and knows the size of. Patella-Modern
// issue #57 (notes/modernization-plan.md, section 7, M4). Not part of the original source.
//
// TFixedString<N> is exactly char[N]: its only member is that array, so it has the array's size and alignment
// (1), it is standard layout, trivially copyable and an aggregate (no constructor: a local is as uninitialised as
// the array was, `= {}` zeroes it, `= {"Text"}` initialises it from a literal). A char[N] member of a frozen
// structure becomes a TFixedString<N> without moving anything (plan, section 2, rule 4: each structure checks it
// with static_asserts in the oracle build; IsFixedStringLayout<N> below for the type itself).
//
// It converts to char * and const char * as the array did, so the code that reads it (strlen, strcmp, the error
// macros' expressions, whose text must not change until M5: `strlen(Address)<100`) stays as it is. Never pass it
// through "..." (MsvcSnprintf, a varargs constructor): that copies the N bytes, not a pointer; pass the Chars
// member or View().
//
// The writes never pass the end and always leave a terminating NUL: Assign, Append, Format and AppendFormat
// write the text and its NUL exactly where strcpy, strcat and sprintf wrote them, and nothing else (the bytes
// after the NUL keep what they were, as with the C functions: a structure written to a file stays
// byte-identical). Where the text does not fit, which in the original wrote past the buffer (undefined), they
// keep its first N - 1 characters and the NUL. Where the original asserts a length (`strlen(Address)<100`), the
// assertion stays in the caller, before the write; where it truncated with _snprintf (MsvcSnprintf), the call
// stays MsvcSnprintf(Fixed.Chars, sizeof(Fixed), ...) with _snprintf's own result (no NUL when the text fills
// the buffer).
//
// Length() and View() stop at the first NUL or at N (a buffer that _snprintf filled has none); Append starts
// there (at N - 1 without a NUL). Assign(const char *) reads a null pointer as strcpy did (it faults). A source
// inside the buffer itself is allowed (memmove).
//
// Format and AppendFormat take std::format's language: std::format_to_n on the product presets, the C++17
// stand-in of compat/format.h in the oracle (PATELLA_HAS_FORMAT 0), with the same output. A TFixedString passed to
// them is its text.

#pragma once

#include "compat/format.h"

#include <cstddef>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>

template <std::size_t N>
struct TFixedString;

// A TFixedString passed to Format is its text, a std::string_view (the standard libraries take a type that
// converts to const char * for a C string, not for one with a std::formatter of its own).
template <typename T>
struct TIsFixedString : std::false_type {};
template <std::size_t N>
struct TIsFixedString<TFixedString<N>> : std::true_type {};
template <typename T>
using TFormatArgument = std::conditional_t<TIsFixedString<std::remove_cv_t<std::remove_reference_t<T>>>::value,
	std::string_view, T>;
template <typename T>
decltype(auto) PassFormatArgument(T &&Value) noexcept
{
	if constexpr (TIsFixedString<std::remove_cv_t<std::remove_reference_t<T>>>::value)
		return Value.View();
	else
		return std::forward<T>(Value);
}

// A text buffer of Capacity bytes (at least 1) at Chars, which the caller owns: TFixedString's writes for a
// buffer the code knows only by a pointer (a parameter, an allocation, a row of a table). The capacity is what the
// original's callers pass (each use says where it comes from).
class TStringBuffer {
public:
	TStringBuffer(char *Buffer, std::size_t Size) noexcept : Chars(Buffer), Capacity(Size) {}

	// The characters before the first NUL, at most Capacity.
	[[nodiscard]] std::size_t Length() const noexcept
	{
		std::size_t Result = 0;
		while (Result < Capacity && Chars[Result] != 0)
			Result++;
		return Result;
	}

	[[nodiscard]] std::string_view View() const noexcept { return {Chars, Length()}; }

	// strcpy: the text and its NUL, at most Capacity - 1 characters. Returns the characters written.
	std::size_t Assign(std::string_view Text) noexcept { return Write(0, Text.data(), Text.size()); }
	std::size_t Assign(const char *Text) noexcept { return Write(0, Text, BoundedLength(Text, Capacity - 1)); }

	// strcat: the text and its NUL after the first NUL (after Capacity - 1 characters where there is none).
	// Returns the length afterwards.
	std::size_t Append(std::string_view Text) noexcept
	{
		const std::size_t Start = End();
		return Start + Write(Start, Text.data(), Text.size());
	}
	std::size_t Append(const char *Text) noexcept
	{
		const std::size_t Start = End();
		return Start + Write(Start, Text, BoundedLength(Text, Capacity - 1 - Start));
	}

#if PATELLA_HAS_FORMAT
	// What sprintf wrote into Chars, with std::format's language. Returns the characters written.
	template <typename... Arguments>
	std::size_t Format(std::format_string<TFormatArgument<Arguments>...> Text, Arguments &&...Values)
	{
		return Terminate(0,
			std::format_to_n(Chars, Limit(0), Text, PassFormatArgument(std::forward<Arguments>(Values))...).out);
	}
	// What sprintf wrote at Chars + strlen(Chars). Returns the length afterwards.
	template <typename... Arguments>
	std::size_t AppendFormat(std::format_string<TFormatArgument<Arguments>...> Text, Arguments &&...Values)
	{
		const std::size_t Start = End();
		return Start
			+ Terminate(Start, std::format_to_n(Chars + Start, Limit(Start), Text, PassFormatArgument(std::forward<Arguments>(Values))...).out);
	}
#else
	template <typename... Arguments>
	std::size_t Format(std::string_view Text, const Arguments &...Values)
	{
		return Terminate(0, patella::compat::FormatToNStandIn(Chars, Limit(0), Text, Values...).Out);
	}
	template <typename... Arguments>
	std::size_t AppendFormat(std::string_view Text, const Arguments &...Values)
	{
		const std::size_t Start = End();
		return Start + Terminate(Start, patella::compat::FormatToNStandIn(Chars + Start, Limit(Start), Text, Values...).Out);
	}
#endif

private:
	char *Chars;
	std::size_t Capacity;

	// The length of Text, at most Maximum, reading no further.
	static std::size_t BoundedLength(const char *Text, std::size_t Maximum) noexcept
	{
		std::size_t Result = 0;
		while (Result < Maximum && Text[Result] != 0)
			Result++;
		return Result;
	}

	// Where Append starts: the first NUL, or Capacity - 1.
	[[nodiscard]] std::size_t End() const noexcept
	{
		const std::size_t Result = Length();
		return Result < Capacity ? Result : Capacity - 1;
	}

	// The room before the last byte, from Start.
	[[nodiscard]] std::ptrdiff_t Limit(std::size_t Start) const noexcept
	{
		return static_cast<std::ptrdiff_t>(Capacity - 1 - Start);
	}

	// Count characters of Text at Start, as many as fit, and the NUL. Returns the characters written.
	std::size_t Write(std::size_t Start, const char *Text, std::size_t Count) noexcept
	{
		const std::size_t Room = Capacity - 1 - Start;
		const std::size_t Written = Count < Room ? Count : Room;
		std::memmove(Chars + Start, Text, Written);
		Chars[Start + Written] = 0;
		return Written;
	}

	// The NUL after a formatted text that ends at Out. Returns the characters written.
	std::size_t Terminate(std::size_t Start, const char *Out) noexcept
	{
		const auto Written = static_cast<std::size_t>(Out - (Chars + Start));
		Chars[Start + Written] = 0;
		return Written;
	}
};

template <std::size_t N>
struct TFixedString {
	static_assert(N > 0, "a fixed string holds at least its terminating NUL");

	// The original's array, the only member (public: the type is an aggregate, as the array was).
	char Chars[N]; // NOLINT(modernize-avoid-c-arrays): the type is the array (issue #57); .Chars is the C string varargs need

	// As the array decays: the code that reads the buffer as a C string is unchanged.
	operator char *() noexcept { return Chars; }
	operator const char *() const noexcept { return Chars; }

	[[nodiscard]] static constexpr std::size_t Capacity() noexcept { return N; }

	// The writes, as a TStringBuffer of N bytes.
	TStringBuffer Buffer() noexcept { return {Chars, N}; }

	[[nodiscard]] std::size_t Length() const noexcept
	{
		std::size_t Result = 0;
		while (Result < N && Chars[Result] != 0)
			Result++;
		return Result;
	}
	[[nodiscard]] std::string_view View() const noexcept { return {Chars, Length()}; }

	std::size_t Assign(std::string_view Text) noexcept { return Buffer().Assign(Text); }
	std::size_t Assign(const char *Text) noexcept { return Buffer().Assign(Text); }
	std::size_t Append(std::string_view Text) noexcept { return Buffer().Append(Text); }
	std::size_t Append(const char *Text) noexcept { return Buffer().Append(Text); }

#if PATELLA_HAS_FORMAT
	template <typename... Arguments>
	std::size_t Format(std::format_string<TFormatArgument<Arguments>...> Text, Arguments &&...Values)
	{
		return Buffer().Format(Text, std::forward<Arguments>(Values)...);
	}
	template <typename... Arguments>
	std::size_t AppendFormat(std::format_string<TFormatArgument<Arguments>...> Text, Arguments &&...Values)
	{
		return Buffer().AppendFormat(Text, std::forward<Arguments>(Values)...);
	}
#else
	template <typename... Arguments>
	std::size_t Format(std::string_view Text, const Arguments &...Values)
	{
		return Buffer().Format(Text, Values...);
	}
	template <typename... Arguments>
	std::size_t AppendFormat(std::string_view Text, const Arguments &...Values)
	{
		return Buffer().AppendFormat(Text, Values...);
	}
#endif
};

// The layout the frozen structures rely on: exactly char[N].
template <std::size_t N>
inline constexpr bool IsFixedStringLayout = sizeof(TFixedString<N>) == N && alignof(TFixedString<N>) == 1
	&& std::is_standard_layout_v<TFixedString<N>> && std::is_trivially_copyable_v<TFixedString<N>>
	&& std::is_trivially_default_constructible_v<TFixedString<N>> && std::is_aggregate_v<TFixedString<N>>;

static_assert(IsFixedStringLayout<1> && IsFixedStringLayout<12> && IsFixedStringLayout<30>
	&& IsFixedStringLayout<100> && IsFixedStringLayout<1024>);
