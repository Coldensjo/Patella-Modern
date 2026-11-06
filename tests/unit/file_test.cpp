// TFile of src/types/File.h and PrintLine's text of src/compat/print.h (issue #64). tests_unit runs them with
// std::format, tests_unit_cxx26_standin (PATELLA_FORMAT_STANDIN) with the stand-in the oracle's C++17 build uses.
// The bytes each open mode puts on the disk are compared with fixed expected bytes on every platform (text: every
// '\n' as "\r\n", the Microsoft C runtime's text mode) and, on Windows, with the C runtime's own fopen/fprintf in the
// mode the original used; Print's text with snprintf's for the conversions the client's calls had.

// The C functions themselves, as the references, without MSVC's deprecation warnings.
#define _CRT_SECURE_NO_WARNINGS

#include <doctest/doctest.h>

#include "compat/msvc_io.h"
#include "compat/print.h"
#include "types/File.h"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <ostream> // IWYU pragma: keep (doctest prints a std::string with operator<<)
#include <random>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

namespace {

// A file name of its own in the temporary folder (other sessions' tests share it: issue #66), removed at the end.
struct TTemporaryPath {
	explicit TTemporaryPath(const char *Name)
	{
		std::random_device Random;
		const std::string Unique = std::to_string(Random()) + "_" + std::to_string(Random());
		Path = (std::filesystem::temp_directory_path() / ("patella_file_test_" + Unique + "_" + Name)).string();
	}
	~TTemporaryPath()
	{
		std::error_code Error;
		std::filesystem::remove(Path, Error);
	}
	TTemporaryPath(const TTemporaryPath &) = delete;
	TTemporaryPath &operator=(const TTemporaryPath &) = delete;
	[[nodiscard]] const char *Name() const { return Path.c_str(); }
	std::string Path;
};

std::string FileBytes(const std::string &Path)
{
	std::ifstream In(Path, std::ios::binary);
	return {std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>()};
}

// The C runtime's bytes for Text written with fopen(Mode) and fputs: the reference on Windows.
[[maybe_unused]] std::string RuntimeBytes(const char *Mode, const std::string &Before, const std::string &Text)
{
	const TTemporaryPath Path("reference.txt");
	if (!Before.empty()) {
		std::FILE *Old = std::fopen(Path.Name(), "wb");
		REQUIRE(Old != nullptr);
		std::fputs(Before.c_str(), Old);
		std::fclose(Old);
	}
	std::FILE *File = std::fopen(Path.Name(), Mode);
	REQUIRE(File != nullptr);
	std::fputs(Text.c_str(), File);
	std::fclose(File);
	return FileBytes(Path.Path);
}

// What the text-mode writes below put into the file, written through every TFile write.
void WriteSample(TFile &File)
{
	CHECK(File.PutChar('\n') == '\n');
	CHECK(File.Write("x\ny\r\n"));
	CHECK(File.Print("{} {}\n", 1, "z") == 4);
	const char Bytes[] = {'\n', 'w'};
	CHECK(File.Write(Bytes, 1, 2) == 2);
}
constexpr const char *SampleText = "\nx\ny\r\n1 z\n\nw";

} // namespace

TEST_CASE("file: exactly a FILE *, closed until opened")
{
	static_assert(IsFileLayout);
	static_assert(sizeof(TFile) == sizeof(void *) && alignof(TFile) == alignof(void *));
	static_assert(!std::is_copy_constructible_v<TFile> && std::is_nothrow_move_constructible_v<TFile>);
	const TFile File;
	CHECK(!File.IsOpen());
	CHECK(File == nullptr);
	CHECK(nullptr == File);
	CHECK(!(File != nullptr));
	CHECK(File == NULL); // the original's spelling (TTalkBuffer::Write's PATELLA_ASSERT(F!=NULL))
	CHECK(File.Address() == 0);
	TFile Other;
	CHECK(Other.Close() == 0);
}

TEST_CASE("file: a failed open is fopen's (false, errno), and the file stays closed")
{
	const TTemporaryPath Missing("missing.bin");
	TFile File;
	errno = 0;
	CHECK(!File.Open(Missing.Name(), TFile::TAccess::Read));
	CHECK(errno == ENOENT);
	CHECK(File == nullptr);
	const std::string InMissingFolder = (std::filesystem::path(Missing.Path) / "file.txt").string();
	CHECK(!File.Open(InMissingFolder.c_str(), TFile::TAccess::Write));
	CHECK(!File.Open(InMissingFolder.c_str(), TFile::TAccess::Append, TFile::TContent::Text));
	CHECK(!File.IsOpen());
	// reading in text mode is not offered
	const TTemporaryPath Present("present.bin");
	REQUIRE(File.Open(Present.Name(), TFile::TAccess::Write));
	CHECK(File.Close() == 0);
	errno = 0;
	CHECK(!File.Open(Present.Name(), TFile::TAccess::Read, TFile::TContent::Text));
	CHECK(errno == EINVAL);
	CHECK(!File.IsOpen());
}

TEST_CASE("file: an open TFile is not opened again")
{
	const TTemporaryPath First("first.bin");
	const TTemporaryPath Second("second.bin");
	TFile File;
	REQUIRE(File.Open(First.Name(), TFile::TAccess::Write));
	const std::uintptr_t Address = File.Address();
	CHECK(Address != 0);
	errno = 0;
	CHECK(!File.Open(Second.Name(), TFile::TAccess::Write));
	CHECK(errno == EBUSY);
	CHECK(File.Address() == Address);
	CHECK(!File.OpenTemporary());
	CHECK(File.Close() == 0);
	CHECK(!std::filesystem::exists(Second.Path));
}

TEST_CASE("file: binary files keep the bytes")
{
	const TTemporaryPath Path("binary.bin");
	{
		TFile File;
		REQUIRE(File.Open(Path.Name(), TFile::TAccess::Write));
		WriteSample(File);
	} // the destructor closes (and flushes)
	CHECK(FileBytes(Path.Path) == SampleText);
	{
		TFile File;
		REQUIRE(File.Open(Path.Name(), TFile::TAccess::Append));
		CHECK(File.Write("tail\n"));
		CHECK(File.Close() == 0);
	}
	CHECK(FileBytes(Path.Path) == std::string(SampleText) + "tail\n");
	{
		TFile File; // "w" empties it
		REQUIRE(File.Open(Path.Name(), TFile::TAccess::Write));
		CHECK(File.Write("new"));
	}
	CHECK(FileBytes(Path.Path) == "new");
#if defined(_WIN32)
	CHECK(RuntimeBytes("wb", "", SampleText) == SampleText);
#endif
}

TEST_CASE("file: text files end their lines in CR LF on every platform, as the Microsoft C runtime wrote them")
{
	const std::string Expected = "\r\nx\r\ny\r\r\n1 z\r\n\r\nw";
	const TTemporaryPath Path("text.txt");
	{
		TFile File;
		REQUIRE(File.Open(Path.Name(), TFile::TAccess::Write, TFile::TContent::Text));
		WriteSample(File);
		CHECK(File.Flush() == 0);
		CHECK(FileBytes(Path.Path) == Expected); // a flush puts them on the disk (the test mode's log)
		CHECK(File.Write("\n"));
	}
	CHECK(FileBytes(Path.Path) == Expected + "\r\n");
	{
		TFile File; // SaveChannel's "at": after what is there
		REQUIRE(File.Open(Path.Name(), TFile::TAccess::Append, TFile::TContent::Text));
		CHECK(File.Print("\nChannel saved at {}", "Sat Nov  4 12:00:00 2023\n") == 43);
		CHECK(File.Close() == 0);
	}
	CHECK(FileBytes(Path.Path) == Expected + "\r\n\r\nChannel saved at Sat Nov  4 12:00:00 2023\r\n");
	{
		TFile File;
		REQUIRE(File.Open(Path.Name(), TFile::TAccess::Write, TFile::TContent::Text));
		CHECK(File.Write("a\n"));
	}
	CHECK(FileBytes(Path.Path) == "a\r\n");
	{
		const TTemporaryPath Created("created.txt");
		TFile File;
		REQUIRE(File.Open(Created.Name(), TFile::TAccess::Append, TFile::TContent::Text));
		CHECK(File.Write("b\n"));
		CHECK(File.Close() == 0);
		CHECK(FileBytes(Created.Path) == "b\r\n");
	}
#if defined(_WIN32)
	CHECK(RuntimeBytes("wt", "", SampleText) == Expected);
	CHECK(RuntimeBytes("w", "", SampleText) == Expected);
	CHECK(RuntimeBytes("at", "old\n", "\nnew\n") == "old\n\r\nnew\r\n");
#endif
}

TEST_CASE("file: reads, getc and ungetc, seeks, and _eof through the descriptor")
{
	const TTemporaryPath Path("read.bin");
	{
		TFile File;
		REQUIRE(File.Open(Path.Name(), TFile::TAccess::Write));
		CHECK(File.Write("abcd\xe4", 1, 5) == 5);
	}
	TFile File;
	REQUIRE(File.Open(Path.Name(), TFile::TAccess::Read));
	CHECK(File.Descriptor() >= 0);
	CHECK(MsvcEof(File.Descriptor()) == 0);
	CHECK(File.GetChar() == 'a');
	// the FILE's buffer read the whole file: the descriptor is at its end (TReadBinaryFile::eof's behaviour)
	CHECK(MsvcEof(File.Descriptor()) == 1);
	CHECK(File.UngetChar('a') == 'a');
	char Bytes[8] = {};
	CHECK(File.Read(Bytes, 1, 4) == 4);
	CHECK(std::string(Bytes, 4) == "abcd");
	CHECK(File.GetChar() == 0xe4); // a byte, not a negative char
	CHECK(File.GetChar() == EOF);
	CHECK(File.Read(Bytes, 1, 1) == 0);
	CHECK(File.Seek(2, SEEK_SET) == 0);
	CHECK(File.Read(Bytes, 2, 1) == 1);
	CHECK(std::string(Bytes, 2) == "cd");
	CHECK(File.Read(Bytes, 2, 1) == 0); // one byte left: no whole item
	REQUIRE(File.Seek(0, SEEK_SET) == 0);
	CHECK(File.GetChar() == 'a');
	CHECK(File.Close() == 0);
	CHECK(File.Close() == 0);
}

TEST_CASE("file: a write of a whole item, and Count 0 (TWriteBinaryFile::writeBytes)")
{
	const TTemporaryPath Path("items.bin");
	TFile File;
	REQUIRE(File.Open(Path.Name(), TFile::TAccess::Write));
	CHECK(File.Write("abc", 3, 1) == 1);
	CHECK(File.Write("abc", 0, 1) == 0); // fwrite(Buffer, 0, 1): no item written, the original's error
	CHECK(File.Write("", 1, 0) == 0);
	CHECK(File.Write(std::string()));
	CHECK(File.Close() == 0);
	CHECK(FileBytes(Path.Path) == "abc");
}

TEST_CASE("file: moves hand the file over, and the moved-from TFile is closed")
{
	const TTemporaryPath Path("moved.bin");
	TFile First;
	REQUIRE(First.Open(Path.Name(), TFile::TAccess::Write));
	const std::uintptr_t Address = First.Address();
	TFile Second(std::move(First));
	CHECK(First == nullptr); // NOLINT(bugprone-use-after-move): the moved-from state is the point
	CHECK(Second.Address() == Address);
	TFile Third;
	Third = std::move(Second);
	CHECK(Second == nullptr); // NOLINT(bugprone-use-after-move)
	CHECK(Third.Write("moved"));
	Third = TFile();
	CHECK(FileBytes(Path.Path) == "moved");
}

TEST_CASE("file: a temporary file reads back what was written (tmpfile, binary)")
{
	TFile File;
	REQUIRE(File.OpenTemporary());
	CHECK(File.Print("{}\n{}\n", "line", "next") == 10);
	REQUIRE(File.Seek(0, SEEK_SET) == 0);
	char Bytes[16] = {};
	CHECK(File.Read(Bytes, 1, sizeof(Bytes)) == 10);
	CHECK(std::string(Bytes, 10) == "line\nnext\n");
}

TEST_CASE("file: Print writes fprintf's characters for the client's conversions")
{
	// fprintf's text for the same values: snprintf's
	char Buffer[256];
	const auto Printf = [&](int Length) {
		REQUIRE(Length >= 0);
		return std::string(Buffer, static_cast<std::size_t>(Length));
	};
	const auto Printed = [](auto Write) {
		TFile File;
		REQUIRE(File.OpenTemporary());
		const int Count = Write(File);
		REQUIRE(File.Seek(0, SEEK_SET) == 0);
		char Bytes[256] = {};
		const std::size_t Read = File.Read(Bytes, 1, sizeof(Bytes));
		CHECK(Count == static_cast<int>(Read));
		return std::string(Bytes, Read);
	};
	// TTalkBuffer::Write: "%s\n"
	for (const char *Text : {"", "12:34 Name: text", "\xe4\xf6\xfc umlauts", "100% {braces}"})
		CHECK(Printed([&](TFile &File) { return File.Print("{}\n", Text); })
			== Printf(std::snprintf(Buffer, sizeof(Buffer), "%s\n", Text))); // NOLINT(cppcoreguidelines-pro-type-vararg)
	// SaveChannel: "\nChannel saved at %s", with ctime's text or "(null)"
	for (const char *Text : {"Sat Nov  4 12:00:00 2023\n", "(null)"})
		CHECK(Printed([&](TFile &File) { return File.Print("\nChannel saved at {}", Text); })
			== Printf(std::snprintf(Buffer, sizeof(Buffer), "\nChannel saved at %s", Text))); // NOLINT(cppcoreguidelines-pro-type-vararg)
	// the test mode's frames.txt: "%d %u %d %u %d %d %lld\n"
	const struct {
		int Step;
		std::uint32_t Requested;
		int Line;
		std::uint32_t Shown;
		int Width, Height;
		long long Filtered;
	} Frames[] = {{0, 0, 0, 0, 0, 0, -1}, {12, 4000, 31, 4016, 800, 600, 123456},
		{INT_MIN, UINT32_MAX, INT_MAX, 1, -1, 1, LLONG_MIN}, {INT_MAX, 7, -5, UINT32_MAX, 1024, 768, LLONG_MAX}};
	for (const auto &Frame : Frames) {
		CHECK(Printed([&](TFile &File) {
			return File.Print("{} {} {} {} {} {} {}\n", Frame.Step, Frame.Requested, Frame.Line, Frame.Shown, Frame.Width,
				Frame.Height, Frame.Filtered);
		}) == Printf(std::snprintf(Buffer, sizeof(Buffer), "%d %u %d %u %d %d %lld\n", Frame.Step, Frame.Requested, // NOLINT(cppcoreguidelines-pro-type-vararg)
				  Frame.Line, Frame.Shown, Frame.Width, Frame.Height, Frame.Filtered)));
	}
}

TEST_CASE("print: PrintLine's text is printf's \"%s\\n\"")
{
	for (const char *Text : {"", "a mark", "\xe4 100%"}) {
		char Buffer[64];
		const int Length = std::snprintf(Buffer, sizeof(Buffer), "%s\n", Text); // NOLINT(cppcoreguidelines-pro-type-vararg)
		CHECK(patella::compat::FormatLine("{}", Text) == std::string(Buffer, static_cast<std::size_t>(Length)));
	}
}
