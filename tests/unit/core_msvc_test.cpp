// src/compat/msvc.h, msvc_io.h and msvc_portable.h (issue #43): the Microsoft CRT's semantics behind portable names. Every case is held
// against the values the Microsoft functions give (UCRT sources, cited in msvc.h): on Windows the Msvc*
// functions are those functions themselves, so the expected values are checked against the UCRT, and the
// portable implementations (patella::compat) against the UCRT next to them; on Linux the Msvc* functions are
// the portable implementations. No window, no dialog: no case passes what the UCRT's invalid parameter
// handler would catch (a bad descriptor, an invalid _access mode, a null buffer with a count).

#include <doctest/doctest.h>

#include "common.h"
#include "compat/msvc.h"
#include "compat/msvc_io.h"
#include "compat/msvc_portable.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>

namespace {

// A buffer of 16 '#', formatted into with Count; the result and the 16 bytes.
struct TFormatted {
	int Result = 0;
	std::string Bytes;
};

template <typename... Arguments>
TFormatted Msvc(std::size_t Count, const char *Format, Arguments... Values)
{
	char Buffer[16];
	std::memset(Buffer, '#', sizeof(Buffer));
	const int Result = MsvcSnprintf(Buffer, Count, Format, Values...);
	return {Result, std::string(Buffer, sizeof(Buffer))};
}

template <typename... Arguments>
TFormatted Portable(std::size_t Count, const char *Format, Arguments... Values)
{
	char Buffer[16];
	std::memset(Buffer, '#', sizeof(Buffer));
	const int Result = patella::compat::PortableSnprintf(Buffer, Count, Format, Values...);
	return {Result, std::string(Buffer, sizeof(Buffer))};
}

// MsvcVsnprintf through a va_list.
int Vformat(char *Buffer, std::size_t Count, const char *Format, ...) // NOLINT(cppcoreguidelines-pro-type-vararg)
{
	std::va_list Values;
	va_start(Values, Format); // NOLINT(cppcoreguidelines-pro-type-vararg)
	const int Result = MsvcVsnprintf(Buffer, Count, Format, Values);
	va_end(Values); // NOLINT(cppcoreguidelines-pro-type-vararg)
	return Result;
}

std::string Expected(const char *Text, std::size_t Length)
{
	std::string Bytes(16, '#');
	Bytes.replace(0, Length, Text, Length);
	return Bytes;
}

// The portable implementation gives what the Microsoft function gives.
void CheckSame(const TFormatted &A, const TFormatted &B)
{
	CHECK(A.Result == B.Result);
	CHECK(A.Bytes == B.Bytes);
}

// A file of the test's own in the temporary folder, removed afterwards.
struct TTemporaryFile {
	explicit TTemporaryFile(const char *Name, const char *Contents)
		: Path((std::filesystem::temp_directory_path() / Name).string())
	{
		std::FILE *File = std::fopen(Path.c_str(), "wb");
		REQUIRE(File != nullptr);
		std::fputs(Contents, File);
		std::fclose(File);
	}
	~TTemporaryFile() { std::remove(Path.c_str()); }
	TTemporaryFile(const TTemporaryFile &) = delete;
	TTemporaryFile &operator=(const TTemporaryFile &) = delete;
	std::string Path;
};

} // namespace

TEST_CASE("msvc: _snprintf writes the NUL only when it fits, and returns -1 when the text is longer")
{
	// shorter than Count: the text and its NUL, the length
	const TFormatted Fits = Msvc(8, "%s", "abc");
	CHECK(Fits.Result == 3);
	CHECK(Fits.Bytes == Expected("abc\0", 4));
	CheckSame(Portable(8, "%s", "abc"), Fits);

	// as long as Count: no NUL, Count (snprintf would write "ab" and a NUL, and return 3)
	const TFormatted Exact = Msvc(3, "%s", "abc");
	CHECK(Exact.Result == 3);
	CHECK(Exact.Bytes == Expected("abc", 3));
	CheckSame(Portable(3, "%s", "abc"), Exact);

	// longer: Count characters, no NUL, -1 (snprintf: 5)
	const TFormatted Longer = Msvc(3, "%d", 12345);
	CHECK(Longer.Result == -1);
	CHECK(Longer.Bytes == Expected("123", 3));
	CheckSame(Portable(3, "%d", 12345), Longer);

	// Count 0: nothing written; -1 for a text, 0 for an empty one
	const TFormatted Nothing = Msvc(0, "%s", "x");
	CHECK(Nothing.Result == -1);
	CHECK(Nothing.Bytes == std::string(16, '#'));
	CheckSame(Portable(0, "%s", "x"), Nothing);
	const TFormatted Empty = Msvc(0, "%s", "");
	CHECK(Empty.Result == 0);
	CHECK(Empty.Bytes == std::string(16, '#'));
	CheckSame(Portable(0, "%s", ""), Empty);

	// no buffer and Count 0: the length
	CHECK(MsvcSnprintf(nullptr, 0, "%d", 12345) == 5);
	CHECK(patella::compat::PortableSnprintf(nullptr, 0, "%d", 12345) == 5);

	// the client's own forms: Control.cpp's frame rate, Status.cpp's address, Dialogs.h's float promotion
	const TFormatted Fps = Msvc(16, "%s %.02f fps", "DX9", 59.996f);
	CHECK(Fps.Result == 13);
	CHECK(Fps.Bytes == Expected("DX9 60.00 fps\0", 14));
	CheckSame(Portable(16, "%s %.02f fps", "DX9", 59.996f), Fps);
	const TFormatted Address = Msvc(16, "%d.%d.%d.%d", 127, 0, 0, 1);
	CHECK(Address.Result == 9);
	CHECK(Address.Bytes == Expected("127.0.0.1\0", 10));
	CheckSame(Portable(16, "%d.%d.%d.%d", 127, 0, 0, 1), Address);
	const TFormatted Char = Msvc(16, "%c%c", 'a', static_cast<char>('b'));
	CHECK(Char.Result == 2);
	CheckSame(Portable(16, "%c%c", 'a', static_cast<char>('b')), Char);
}

TEST_CASE("msvc: _vsnprintf has the same semantics")
{
	char Buffer[8];
	std::memset(Buffer, '#', sizeof(Buffer));
	CHECK(Vformat(Buffer, 4, "%s", "abcd") == 4);
	CHECK(std::string(Buffer, 8) == "abcd####");
	CHECK(Vformat(Buffer, 4, "%s", "xyz12") == -1);
	CHECK(std::string(Buffer, 8) == "xyz1####");
	CHECK(Vformat(Buffer, 8, "%d", 42) == 2);
	CHECK(std::string(Buffer, 8) == std::string("42\0"
												"1####",
			  8));
}

TEST_CASE("msvc: _stricmp folds A-Z to lower case, and nothing else")
{
	struct TCase {
		const char *Left;
		const char *Right;
		int Result;
	};
	const TCase Cases[] = {
		{"gamemaster", "GameMaster", 0},
		{"", "", 0},
		{"a", "B", 'a' - 'b'},
		// lower case, not upper case: "abc_" < "ABCD" ('_' < 'd'; folded to upper case '_' > 'D')
		{"abc_", "ABCD", '_' - 'd'},
		{"[", "a", '[' - 'a'},
		{"A", "[", 'a' - '['},
		// the bytes from 0x80 compare unsigned and unfolded: Latin-1 A umlaut against a umlaut
		{"\xC4", "\xE4", 0xC4 - 0xE4},
		{"\x80", "a", 0x80 - 'a'},
		{"Abc", "ab", 'c'},
		{"ab", "ABC", -'c'},
	};
	for (const TCase &Case : Cases) {
		CAPTURE(Case.Left);
		CAPTURE(Case.Right);
		CHECK(MsvcStricmp(Case.Left, Case.Right) == Case.Result);
		CHECK(patella::compat::PortableStricmp(Case.Left, Case.Right) == Case.Result);
	}
}

TEST_CASE("msvc: _strdup copies into memory from malloc, and a null text stays null")
{
	const char Text[] = "Tibia";
	char *Copy = MsvcStrdup(Text);
	REQUIRE(Copy != nullptr);
	CHECK(Copy != Text);
	CHECK(std::strcmp(Copy, Text) == 0);
	std::free(Copy); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
	char *PortableCopy = patella::compat::PortableStrdup(Text);
	REQUIRE(PortableCopy != nullptr);
	CHECK(std::strcmp(PortableCopy, Text) == 0);
	std::free(PortableCopy); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
	CHECK(MsvcStrdup(nullptr) == nullptr);
	CHECK(patella::compat::PortableStrdup(nullptr) == nullptr);
}

TEST_CASE("msvc: _access and _eof")
{
	const TTemporaryFile Data("patella_msvc_test.bin", "abcd");
	CHECK(MsvcAccess(Data.Path.c_str(), 0) == 0);
	CHECK(MsvcAccess(Data.Path.c_str(), 4) == 0);
	const std::string Missing = (std::filesystem::temp_directory_path() / "patella_msvc_test_missing.bin").string();
	errno = 0;
	CHECK(MsvcAccess(Missing.c_str(), 0) == -1);
	CHECK(errno == ENOENT);

	// _eof looks at the descriptor's position: unbuffered, it is the FILE's
	std::FILE *File = std::fopen(Data.Path.c_str(), "rb");
	REQUIRE(File != nullptr);
	REQUIRE(std::setvbuf(File, nullptr, _IONBF, 0) == 0);
	const int Descriptor = MsvcFileno(File);
	CHECK(Descriptor >= 0);
	CHECK(MsvcEof(Descriptor) == 0);
	char Bytes[4];
	CHECK(std::fread(Bytes, 1, 4, File) == 4);
	CHECK(MsvcEof(Descriptor) == 1);
	std::fseek(File, 1, SEEK_SET);
	CHECK(MsvcEof(Descriptor) == 0);
	// the position stays where it was
	CHECK(std::fread(Bytes, 1, 3, File) == 3);
	CHECK(std::string(Bytes, 3) == "bcd");
	CHECK(MsvcEof(Descriptor) == 1);
	std::fclose(File);

	const TTemporaryFile Empty("patella_msvc_test_empty.bin", "");
	File = std::fopen(Empty.Path.c_str(), "rb");
	REQUIRE(File != nullptr);
	CHECK(MsvcEof(MsvcFileno(File)) == 1);
	std::fclose(File);
}

TEST_CASE("msvc: _dupenv_s and _putenv_s, where an empty value removes the variable")
{
	const char *Name = "PATELLA_MSVC_TEST_VARIABLE";
	CHECK(MsvcPutenv(Name, "") == 0);
	char *Value = nullptr;
	std::size_t Length = 99;
	CHECK(MsvcDupenv(&Value, &Length, Name) == 0);
	CHECK(Value == nullptr);
	CHECK(Length == 0);

	CHECK(MsvcPutenv(Name, "lavapipe") == 0);
	CHECK(MsvcDupenv(&Value, &Length, Name) == 0);
	REQUIRE(Value != nullptr);
	CHECK(std::string(Value) == "lavapipe");
	CHECK(Length == 9);
	std::free(Value); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

	CHECK(MsvcPutenv(Name, "") == 0);
	Value = nullptr;
	CHECK(MsvcDupenv(&Value, &Length, Name) == 0);
	CHECK(Value == nullptr);
#if !defined(_WIN32)
	// the portable implementation's EINVALs (the UCRT's go to its invalid parameter handler)
	CHECK(MsvcPutenv("", "x") == EINVAL);
	CHECK(MsvcPutenv("A=B", "x") == EINVAL);
	CHECK(MsvcDupenv(nullptr, &Length, Name) == EINVAL);
#endif
}

PATELLA_MSVC_WARNING(push)
PATELLA_MSVC_WARNING(disable : 4100)
TEST_CASE("msvc: PATELLA_CDECL and PATELLA_MSVC_WARNING")
{
	// One convention on x64 and Linux: the function types are the same.
	static_assert(std::is_same_v<void(PATELLA_CDECL *)(int), void (*)(int)>);
	static_assert(std::is_same_v<int(PATELLA_CDECL *)(const void *, int, int, int), int (*)(const void *, int, int, int)>);
	CHECK(true);
}
PATELLA_MSVC_WARNING(pop)
