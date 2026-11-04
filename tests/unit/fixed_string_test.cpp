// TFixedString<N> of src/types/FixedString.h and the std::format_to_n stand-in of src/compat/format.h (issue
// #57). tests_unit runs them with std::format_to_n, tests_unit_cxx26_standin (PATELLA_FORMAT_STANDIN) with the
// stand-in the oracle's C++17 build uses; both compare the stand-in with std::format_to_n directly. Each write
// is held against the C function it replaces on a char[N] filled the same way, byte for byte (the bytes after
// the NUL included), where the C function stays inside the buffer; where it would not, against the truncation
// the type promises.

// The C functions themselves, as the references, without MSVC's deprecation warnings.
#define _CRT_SECURE_NO_WARNINGS

#include <doctest/doctest.h>

#include "compat/format.h"
#include "compat/msvc.h"
#include "types/FixedString.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <format>
#include <limits>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string_view with operator<<)
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

// A buffer of '#', as the C functions and the type find it.
template <std::size_t N>
TFixedString<N> Filled()
{
	TFixedString<N> Result;
	std::memset(Result.Chars, '#', N);
	return Result;
}

template <std::size_t N>
std::string Bytes(const TFixedString<N> &Value)
{
	return std::string(Value.Chars, N);
}

template <std::size_t N>
std::string Bytes(const char (&Value)[N])
{
	return std::string(Value, N);
}

// A frozen structure's shape: members before and after the text keep their offsets.
struct TLayout {
	int Before;
	TFixedString<30> Name;
	char Flag;
	TFixedString<3> Short;
	int After;
};
struct TLayoutArrays {
	int Before;
	char Name[30];
	char Flag;
	char Short[3];
	int After;
};

} // namespace

TEST_CASE("TFixedString is exactly char[N]")
{
	static_assert(sizeof(TFixedString<1>) == 1 && alignof(TFixedString<1>) == 1);
	static_assert(sizeof(TFixedString<30>) == 30 && alignof(TFixedString<30>) == 1);
	static_assert(sizeof(TFixedString<2048>) == 2048);
	static_assert(std::is_standard_layout_v<TFixedString<30>> && std::is_trivially_copyable_v<TFixedString<30>>);
	static_assert(std::is_trivially_default_constructible_v<TFixedString<30>> && std::is_aggregate_v<TFixedString<30>>);
	static_assert(IsFixedStringLayout<7> && IsFixedStringLayout<256>);
	static_assert(sizeof(TLayout) == sizeof(TLayoutArrays) && alignof(TLayout) == alignof(TLayoutArrays));
	static_assert(offsetof(TLayout, Name) == offsetof(TLayoutArrays, Name));
	static_assert(offsetof(TLayout, Flag) == offsetof(TLayoutArrays, Flag));
	static_assert(offsetof(TLayout, Short) == offsetof(TLayoutArrays, Short));
	static_assert(offsetof(TLayout, After) == offsetof(TLayoutArrays, After));
	static_assert(std::is_standard_layout_v<TLayout> && std::is_trivially_copyable_v<TLayout>);

	// Aggregate initialisation as the array's, and memcpy in and out.
	const TFixedString<8> Literal = {"Tibia"};
	CHECK(Literal.View() == "Tibia");
	CHECK(Literal.Chars[5] == 0);
	const TFixedString<8> Zero = {};
	CHECK(Bytes(Zero) == std::string(8, '\0'));
	TLayoutArrays Arrays{};
	std::memcpy(Arrays.Name, "Knight", 7);
	TLayout Copy{};
	static_assert(sizeof(Copy) == sizeof(Arrays));
	std::memcpy(&Copy, &Arrays, sizeof(Copy));
	CHECK(Copy.Name.View() == "Knight");
	CHECK(TFixedString<30>::Capacity() == 30);
	CHECK(sizeof(Copy.Name) == 30);
}

TEST_CASE("TFixedString reads as the array did")
{
	TFixedString<16> Text = Filled<16>();
	Text.Assign("Address");
	// strlen, strcmp, indexing and the error macros' expressions see a char *.
	CHECK(std::strlen(Text) == 7);
	CHECK(std::strcmp(Text, "Address") == 0);
	CHECK(Text[0] == 'A');
	Text[0] = 'a';
	CHECK(Text.View() == "address");
	const TFixedString<16> &Constant = Text;
	const char *Pointer = Constant;
	CHECK(Pointer == Text.Chars);
	CHECK(Constant[1] == 'd');
	char *Mutable = Text;
	CHECK(Mutable == Text.Chars);
	CHECK((std::strlen(Text) < 100));
	CHECK(Text.Length() == 7);
}

TEST_CASE("Assign writes what strcpy writes, and truncates where strcpy would overflow")
{
	for (const char *Source : {"", "a", "Knight", "exactly-11c", "Player Name"}) {
		TFixedString<12> Fixed = Filled<12>();
		char Array[12];
		std::memset(Array, '#', sizeof(Array));
		std::strcpy(Array, Source); // NOLINT(clang-analyzer-security.insecureAPI.strcpy): the reference
		CHECK(Fixed.Assign(Source) == std::strlen(Source));
		CHECK(Bytes(Fixed) == Bytes(Array));
		// The same through a std::string_view.
		TFixedString<12> FromView = Filled<12>();
		CHECK(FromView.Assign(std::string_view(Source)) == std::strlen(Source));
		CHECK(Bytes(FromView) == Bytes(Array));
	}
	// Too long: the first N - 1 characters and the NUL, nothing past the end.
	TFixedString<6> Short = Filled<6>();
	CHECK(Short.Assign("Longer than six") == 5);
	CHECK(Bytes(Short) == std::string("Longe\0", 6));
	CHECK(Short.Assign(std::string_view("abcdefgh", 8)) == 5);
	CHECK(Bytes(Short) == std::string("abcde\0", 6));
	// N == 1 holds only the NUL.
	TFixedString<1> One = Filled<1>();
	CHECK(One.Assign("x") == 0);
	CHECK(One.Chars[0] == 0);
	// A source inside the buffer (Utils.cpp's strcpy(Destination, &Source[i]) on itself).
	TFixedString<16> Self = Filled<16>();
	Self.Assign("   trimmed");
	CHECK(Self.Assign(Self.Chars + 3) == 7);
	CHECK(Self.View() == "trimmed");
	CHECK(Bytes(Self) == std::string("trimmed\0ed\0#####", 16));
}

TEST_CASE("Append writes what strcat writes, and truncates where strcat would overflow")
{
	TFixedString<32> Fixed = Filled<32>();
	char Array[32];
	std::memset(Array, '#', sizeof(Array));
	Fixed.Assign("Error: ");
	std::strcpy(Array, "Error: "); // NOLINT(clang-analyzer-security.insecureAPI.strcpy): the reference
	CHECK(Fixed.Append("Connection") == 17);
	std::strcat(Array, "Connection"); // NOLINT(clang-analyzer-security.insecureAPI.strcpy): the reference
	CHECK(Bytes(Fixed) == Bytes(Array));
	CHECK(Fixed.Append(std::string_view(" refused.")) == 26);
	std::strcat(Array, " refused."); // NOLINT(clang-analyzer-security.insecureAPI.strcpy): the reference
	CHECK(Bytes(Fixed) == Bytes(Array));
	CHECK(Fixed.Append("") == 26);
	CHECK(Bytes(Fixed) == Bytes(Array));
	// Past the end: cut at N - 1.
	CHECK(Fixed.Append(" And more text.") == 31);
	CHECK(Fixed.View() == "Error: Connection refused. And ");
	CHECK(Fixed.Chars[31] == 0);
	CHECK(Fixed.Append("x") == 31);
	CHECK(Fixed.View() == "Error: Connection refused. And ");
	// A buffer without a NUL (MsvcSnprintf filled it): Append ends it at N - 1.
	TFixedString<4> Full = Filled<4>();
	CHECK(Full.Length() == 4);
	CHECK(Full.View() == "####");
	CHECK(Full.Append("x") == 3);
	CHECK(Bytes(Full) == std::string("###\0", 4));
}

TEST_CASE("Format writes what sprintf writes for the client's formats")
{
	// Each pair: the original's printf format and its std::format form, with the arguments the client passes.
	struct TCase {
		std::string Printf;
		std::string Format;
	};
	TFixedString<64> Fixed = Filled<64>();
	char Array[64];
	const auto Same = [&](std::size_t Written) {
		CHECK(Written == std::strlen(Array));
		CHECK(Bytes(Fixed) == Bytes(Array));
	};
	for (const int Value : {0, 1, 7, 42, 99, 100, 255, 999, 1000, 12345, -1, -42, -100,
			 std::numeric_limits<int>::max(), std::numeric_limits<int>::min()}) {
		std::memset(Array, '#', sizeof(Array));
		Fixed = Filled<64>();
		std::snprintf(Array, sizeof(Array), "%d", Value);
		Same(Fixed.Format("{}", Value));
		std::memset(Array, '#', sizeof(Array));
		Fixed = Filled<64>();
		std::snprintf(Array, sizeof(Array), "%03d ", Value);
		Same(Fixed.Format("{:03} ", Value));
		std::memset(Array, '#', sizeof(Array));
		Fixed = Filled<64>();
		std::snprintf(Array, sizeof(Array), "%02d:%02d ", Value, Value);
		Same(Fixed.Format("{:02}:{:02} ", Value, Value));
		std::memset(Array, '#', sizeof(Array));
		Fixed = Filled<64>();
		std::snprintf(Array, sizeof(Array), "%03d%03d%02d.map", Value, Value, Value);
		Same(Fixed.Format("{:03}{:03}{:02}.map", Value, Value, Value));
	}
	for (const unsigned int Value : {0U, 5U, 100U, 255U, 4294967295U}) {
		std::memset(Array, '#', sizeof(Array));
		Fixed = Filled<64>();
		std::snprintf(Array, sizeof(Array), " %03u", Value);
		Same(Fixed.Format(" {:03}", Value));
	}
	for (const double Value : {0.0, 0.004, 0.005, 0.015, 1.0, 9.995, 12.345, 59.999, 60.0, 99.5, 123.456, 1e6,
			 -0.0, -1.25, 0.125, 2.675}) {
		std::memset(Array, '#', sizeof(Array));
		Fixed = Filled<64>();
		std::snprintf(Array, sizeof(Array), "Current framerate: %2.2f", Value);
		Same(Fixed.Format("Current framerate: {:2.2f}", Value));
	}
	for (const char Value : {'a', 'Z', ' ', '\n', '\xe4'}) {
		std::memset(Array, '#', sizeof(Array));
		Fixed = Filled<64>();
		std::snprintf(Array, sizeof(Array), "%c", Value);
		Same(Fixed.Format("{}", Value));
	}
	const char *Name = "Bubble";
	const TFixedString<30> FixedName = {"Cip\xe4s"};
	std::memset(Array, '#', sizeof(Array));
	Fixed = Filled<64>();
	std::snprintf(Array, sizeof(Array), "%s has logged in.", Name);
	Same(Fixed.Format("{} has logged in.", Name));
	std::memset(Array, '#', sizeof(Array));
	Fixed = Filled<64>();
	std::snprintf(Array, sizeof(Array), "Message to %s", FixedName.Chars);
	Same(Fixed.Format("Message to {}", FixedName));
	std::memset(Array, '#', sizeof(Array));
	Fixed = Filled<64>();
	std::snprintf(Array, sizeof(Array), "%s (%s)", Name, "Antica");
	Same(Fixed.Format("{} ({})", Name, std::string_view("Antica")));
	std::memset(Array, '#', sizeof(Array));
	Fixed = Filled<64>();
	std::snprintf(Array, sizeof(Array), "100%% {braces}");
	Same(Fixed.Format("100% {{braces}}"));
	// unsigned char and signed char are numbers, as %d of their promotion.
	std::memset(Array, '#', sizeof(Array));
	Fixed = Filled<64>();
	std::snprintf(Array, sizeof(Array), "%d %d", 200, -5);
	Same(Fixed.Format("{} {}", static_cast<unsigned char>(200), static_cast<signed char>(-5)));
}

TEST_CASE("Format and AppendFormat truncate where sprintf would overflow")
{
	TFixedString<8> Fixed = Filled<8>();
	CHECK(Fixed.Format("{}", "exactly") == 7);
	CHECK(Bytes(Fixed) == std::string("exactly\0", 8));
	Fixed = Filled<8>();
	CHECK(Fixed.Format("{} {}", "too", "long text") == 7);
	CHECK(Bytes(Fixed) == std::string("too lon\0", 8));
	Fixed = Filled<8>();
	CHECK(Fixed.Format("{}", 123456789) == 7);
	CHECK(Bytes(Fixed) == std::string("1234567\0", 8));
	// AppendFormat is sprintf(Text + strlen(Text), ...).
	TFixedString<16> Text = Filled<16>();
	char Array[16];
	std::memset(Array, '#', sizeof(Array));
	Text.Assign("Bob\n");
	std::strcpy(Array, "Bob\n"); // NOLINT(clang-analyzer-security.insecureAPI.strcpy): the reference
	CHECK(Text.AppendFormat("{}\n", "Alice") == 10);
	std::snprintf(Array + std::strlen(Array), sizeof(Array) - std::strlen(Array), "%s\n", "Alice");
	CHECK(Bytes(Text) == Bytes(Array));
	CHECK(Text.AppendFormat("{}\n", "Carolina") == 15);
	CHECK(Text.View() == "Bob\nAlice\nCarol");
	CHECK(Text.Chars[15] == 0);
	CHECK(Text.AppendFormat("{}", 7) == 15);
	CHECK(Text.View() == "Bob\nAlice\nCarol");
}

TEST_CASE("MsvcSnprintf into a TFixedString keeps _snprintf's results")
{
	// The truncating writes stay MsvcSnprintf(Fixed.Chars, sizeof(Fixed), ...): no NUL when the text fills the
	// buffer, -1 when it does not fit (Status.cpp's IPAddress, Dialogs.h's reconnect text).
	TFixedString<16> Address = Filled<16>();
	CHECK(MsvcSnprintf(Address.Chars, sizeof(Address), "%d.%d.%d.%d", 127, 0, 0, 1) == 9);
	CHECK(Bytes(Address) == std::string("127.0.0.1\0######", 16));
	Address = Filled<16>();
	CHECK(MsvcSnprintf(Address.Chars, sizeof(Address), "%d.%d.%d.%d", 255, 255, 255, 255) == 15);
	CHECK(Bytes(Address) == "255.255.255.255" + std::string(1, '\0'));
	Address = Filled<16>();
	CHECK(MsvcSnprintf(Address.Chars, sizeof(Address), "%d.%d.%d.%d", 1000, 1000, 1000, 1) == 16);
	CHECK(Bytes(Address) == "1000.1000.1000.1");
	CHECK(Address.Length() == 16);
	CHECK(Address.View() == "1000.1000.1000.1");
	Address = Filled<16>();
	CHECK(MsvcSnprintf(Address.Chars, sizeof(Address), "%d.%d.%d.%d", 1000, 1000, 1000, 1000) == -1);
	CHECK(Bytes(Address) == "1000.1000.1000.1");
}

TEST_CASE("TStringBuffer writes into a buffer known by its pointer and size")
{
	// GetErrorMessage's Message: every caller passes new char[1024].
	std::vector<char> Storage(1024, '#');
	std::vector<char> Reference(1024, '#');
	TStringBuffer Message(Storage.data(), Storage.size());
	Message.Assign("");
	Reference[0] = 0;
	CHECK(Message.Append("Cannot connect to a login server.\n\nError: ") == 42);
	std::strcat(Reference.data(), "Cannot connect to a login server.\n\nError: ");
	const TFixedString<100> Text = {"Connection refused."};
	CHECK(Message.Append(Text) == 61);
	std::strcat(Reference.data(), Text);
	CHECK(Message.AppendFormat("\n{}", 10061) == 67);
	std::strcat(Reference.data(), "\n10061");
	CHECK(Storage == Reference);
	CHECK(Message.View() == std::string_view(Reference.data()));
	CHECK(Message.Length() == 67);
	// A row of a table (Talk.cpp's lines, the hotkeys): the capacity the table gives it.
	char Rows[2][8];
	std::memset(Rows, '#', sizeof(Rows));
	TStringBuffer Row(Rows[0], sizeof(Rows[0]));
	CHECK(Row.Format("{}:{}", "speaker", 12) == 7);
	CHECK(std::string(Rows[0], 8) == std::string("speaker\0", 8));
	CHECK(std::string(Rows[1], 8) == "########");
	CHECK(Row.Assign(std::string_view("0123456789")) == 7);
	CHECK(std::string(Rows[1], 8) == "########");
	// TFixedString's own buffer.
	TFixedString<6> Fixed = Filled<6>();
	CHECK(Fixed.Buffer().Assign("abc") == 3);
	CHECK(Bytes(Fixed) == std::string("abc\0##", 6));
}

namespace {

// The stand-in against std::format_to_n for every count from 0 to past the length.
template <typename... Arguments>
void CompareStandIn(std::format_string<const Arguments &...> Format, const Arguments &...Values)
{
	const std::string Expected = std::format(Format, Values...);
	for (std::ptrdiff_t Count = 0; Count <= static_cast<std::ptrdiff_t>(Expected.size()) + 1; Count++) {
		std::vector<char> Real(Expected.size() + 4, '#');
		std::vector<char> StandIn(Expected.size() + 4, '#');
		const auto RealResult = std::format_to_n(Real.data(), Count, Format, Values...);
		const auto StandInResult = patella::compat::FormatToNStandIn(StandIn.data(), Count, Format.get(), Values...);
		CHECK(RealResult.size == StandInResult.Size);
		CHECK(RealResult.out - Real.data() == StandInResult.Out - StandIn.data());
		CHECK(std::string_view(Real.data(), Real.size()) == std::string_view(StandIn.data(), StandIn.size()));
	}
}

} // namespace

TEST_CASE("The std::format_to_n stand-in formats as std::format_to_n")
{
	CompareStandIn("plain text");
	CompareStandIn("{{}} {{{}}}", 5);
	for (const int Value : {0, 3, -3, 42, 255, -1000, std::numeric_limits<int>::max(), std::numeric_limits<int>::min()}) {
		CompareStandIn("{} {:03} {:02} {:5} {:<5}| {:^6}| {:>4} {:*^9} {:+} {: } {:-} {:+05} {:x} {:X} {:08x} {:d}",
			Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value, Value,
			Value);
	}
	for (const long long Value : {std::numeric_limits<long long>::min(), -1LL, 0LL, 1LL << 40,
			 std::numeric_limits<long long>::max()})
		CompareStandIn("{} {:025} {:x}", Value, Value, Value);
	for (const unsigned long long Value : {0ULL, 9ULL, std::numeric_limits<unsigned long long>::max()})
		CompareStandIn("{} {:03} {:X}", Value, Value, Value);
	const unsigned char Byte = 200;
	const signed char Signed = -7;
	const short Short = -300;
	const unsigned short UnsignedShort = 65535;
	CompareStandIn("{} {} {} {} {:03}", Byte, Signed, Short, UnsignedShort, Byte);
	for (const double Value : {0.0, -0.0, 0.004, 0.005, 0.125, 1.0, 2.675, 9.995, 12.345, 59.999, 1e6, 1e21, 1e-7,
			 -1.25, 3.14159265358979, 1.0 / 3.0}) {
		CompareStandIn("{} {:.2f} {:2.2f} {:f} {:.0f} {:8.3f} {:08.2f} {:+.1f} {:<9.2f}| {:^9.1f}| {:.3}", Value, Value,
			Value, Value, Value, Value, Value, Value, Value, Value, Value);
	}
	for (const float Value : {0.1F, 1.5F, 3.14159F, -2.5F, 1e10F})
		CompareStandIn("{} {:.2f} {:.4} {:6.1f}", Value, Value, Value, Value);
	for (const double Value : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
		CompareStandIn("{} {:.2f} {:6.2f} {:+} {:<6}|", Value, Value, Value, Value, Value);
	const char *Name = "Knight";
	const std::string String = "World";
	const std::string_view View = "View";
	const TFixedString<12> Fixed = {"Fixed"};
	const char Character = 'k';
	const bool True = true;
	CompareStandIn("{} {} {} {} {:s} {:10}| {:>10}| {:^10}| {:.3} {:-<8}|", Name, String, View, Fixed.View(), Name,
		Name, Name, Name, Name, View);
	// The stand-in takes a TFixedString itself (std::format gets its View() from TFixedString::Format).
	char Out[16] = {};
	const auto Result = patella::compat::FormatToNStandIn(Out, 16, "<{:>7}>", Fixed);
	CHECK(Result.Size == 9);
	CHECK(std::string_view(Out, 9) == "<  Fixed>");
	CompareStandIn("{} {:c} {:3}| {:>3}| {} {:s} {:>6}|", Character, Character, Character, Character, True, True,
		True);
	CompareStandIn("{}", std::string_view("Latin-1 \xe4\xf6\xfc as bytes"));
	CompareStandIn("{}{}{}", 1, "", 2);
	// A char array (the original's members, Addressee[30]) is its text up to the NUL, not its N bytes.
	char Array[12];
	std::memset(Array, '#', sizeof(Array));
	std::memcpy(Array, "Bubble", 7);
	CompareStandIn("{}|{:>8}|", Array, Array);
	CHECK(std::format("{}", Array) == "Bubble");
	TFixedString<16> Formatted = Filled<16>();
	CHECK(Formatted.Format("{}:", Array) == 7);
	CHECK(Formatted.View() == "Bubble:");
}

TEST_CASE("The stand-in rejects what it does not format")
{
	char Buffer[16] = {};
	using patella::compat::FormatToNStandIn;
	using patella::compat::TFormatError;
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{0}", 1), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{:#x}", 1), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{:e}", 1.0), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{:d}", 'c'), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{:.2}", 5), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{:05}", "text"), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{} {}", 1), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{", 1), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "}", 1), TFormatError);
	CHECK_THROWS_AS(FormatToNStandIn(Buffer, 16, "{:{}}", 1, 2), TFormatError);
#if defined(PATELLA_FORMAT_STANDIN)
	CHECK(PATELLA_HAS_FORMAT == 0);
#else
	CHECK(PATELLA_HAS_FORMAT == 1);
#endif
}
