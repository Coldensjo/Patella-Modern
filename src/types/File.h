// File.h: TFile, the client's open files (the C library's FILE *) with an owner. Patella-Modern issue #64
// (notes/modernization-plan.md, section 7, M4: "an RAII TFile instead of FILE *"). Not part of the original source.
//
// TFile is exactly a FILE *: its only member is the pointer, so it has the pointer's size and alignment and a
// FILE * member of a frozen structure (Script.h's four file classes) becomes a TFile without moving anything
// (IsFileLayout below; the classes check it in the oracle build). It closes its file when it is destroyed, it
// moves but does not copy, and it compares with nullptr (and NULL) as the pointer did, so the conditions the
// original wrote (`File != NULL`, the assertion `F!=NULL`) stay as they were.
//
// The functions are the C library's, with their results and their errors: Open is fopen (false where fopen
// returned NULL, errno set by the library), Close is fclose (its result; nothing to do on a closed TFile),
// Read/Write are fread/fwrite (the number of whole items), PutChar is fputc, Seek is fseek, Flush is fflush, and
// Descriptor is _fileno/fileno (the descriptor under the FILE, for _eof). Nothing throws, nothing reports: the
// callers check the results and raise their own errors (TERROR texts, frozen until M5), as they did. Print
// writes a text formatted with std::format's language (fprintf with the client's conversions, which give the same
// characters: notes/decisions.md, issue #64); on the oracle's C++17 through the stand-in of compat/format.h.
//
// The bytes on the disk are the original's on every platform:
//	- TContent::Binary ("b"): the bytes as written.
//	- TContent::Text ("t", writing only): every '\n' written becomes "\r\n" and nothing else changes, as the
//	  Microsoft C runtime's text mode wrote it (MSVC 7.1's and the UCRT's lowio _write). On Windows that is the
//	  runtime's own text mode (the original's fopen "at" itself); elsewhere the stream is a glibc fopencookie
//	  stream whose write function adds the '\r' and writes to the descriptor (POSIX has no text mode). The FILE
//	  itself translates, so every write (Write, PutChar, Print) and every flush point is the same as on Windows.
//	  Reading in text mode is refused (false, errno EINVAL): the client reads only binary files.
// Open is fopen with the original's sharing on Windows (_fsopen with _SH_DENYNO, which is what fopen calls);
// the UTF-16 name overload of the Windows-only test mode is _wfopen_s, as its callers had (not shared).
// The name is the client's text, Windows-1252 (issue #112, plan D4; types/Cp1252.h): on Windows it is opened by its
// UTF-16 name (_wfsopen, the same sharing), so it no longer depends on the system's ANSI code page (where that is
// 1252 the file is the same); elsewhere by its UTF-8 name. An ASCII name is the same file everywhere.

#pragma once

#include "compat/format.h"
#include "types/Cp1252.h"
#include "types/FixedString.h" // IWYU pragma: keep (std::format's branch of Print: TFormatArgument)

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string> // IWYU pragma: keep (the stand-in's branch of Print)
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#include <share.h>
#else
#include <fcntl.h>
#include <stdio.h> // NOLINT(modernize-deprecated-headers): fileno and fopencookie, which POSIX and glibc declare there
#include <sys/types.h>
#include <unistd.h>
#endif

class TFile {
public:
	// fopen's first letter: "r", "w" (created or emptied), "a" (created, every write at the end).
	enum class TAccess : unsigned char { Read,
		Write,
		Append };
	// fopen's "b" or "t" (above).
	enum class TContent : unsigned char { Binary,
		Text };

	TFile() noexcept = default;
	~TFile() { Close(); }
	TFile(const TFile &) = delete;
	TFile &operator=(const TFile &) = delete;
	TFile(TFile &&Other) noexcept : Handle(std::exchange(Other.Handle, nullptr)) {}
	TFile &operator=(TFile &&Other) noexcept
	{
		if (this != &Other) {
			Close();
			Handle = std::exchange(Other.Handle, nullptr);
		}
		return *this;
	}

	// fopen(Name, Mode), Name in the client's code page (above). False where fopen returned NULL (errno set), on a
	// TFile that is already open (EBUSY; the callers check that themselves first, with their own error), and where
	// the converted name cannot be made (ENOMEM).
	bool Open(const char *Name, TAccess Access, TContent Content = TContent::Binary) noexcept
	{
		if (!Usable(Access, Content))
			return false;
		if (Name == nullptr) {
			errno = EINVAL;
			return false;
		}
		try {
#if defined(_WIN32)
			const std::wstring SystemName = patella::text::Cp1252ToWide(Name);
#else
			const std::string SystemName = patella::text::Cp1252ToUtf8(Name);
#endif
			return OpenSystem(SystemName.c_str(), Access, Content);
		} catch (...) { // std::bad_alloc of the name
			errno = ENOMEM;
			return false;
		}
	}

#if defined(_WIN32)
	using TSystemChar = wchar_t;
#else
	using TSystemChar = char;
#endif
	// Open of a name in the system's own form (std::filesystem::path's c_str(); issue #130: the data folder's files,
	// platform/FileSystem.h): UTF-16 on Windows, opened as Open opens it (_wfsopen, _SH_DENYNO), the bytes elsewhere.
	bool OpenSystemName(const TSystemChar *Name, TAccess Access, TContent Content = TContent::Binary) noexcept
	{
		if (!Usable(Access, Content))
			return false;
		if (Name == nullptr) {
			errno = EINVAL;
			return false;
		}
		return OpenSystem(Name, Access, Content);
	}

#if defined(_WIN32)
	// _wfopen_s(&File, Name, Mode): a UTF-16 name, the file not shared while it is open (the test mode's dumps).
	bool Open(const wchar_t *Name, TAccess Access, TContent Content = TContent::Binary) noexcept
	{
		if (!Usable(Access, Content))
			return false;
		const std::array<wchar_t, 4> WideMode = WideModeText(Access, Content);
		std::FILE *Opened = nullptr;
		if (_wfopen_s(&Opened, Name, WideMode.data()) != 0)
			Opened = nullptr;
		Handle = Opened;
		return Handle != nullptr;
	}
#endif

	// tmpfile(): a new binary file for reading and writing ("w+b"), deleted when it is closed.
	bool OpenTemporary() noexcept
	{
		if (Handle != nullptr) {
			errno = EBUSY;
			return false;
		}
#if defined(_MSC_VER)
		if (tmpfile_s(&Handle) != 0)
			Handle = nullptr;
#else
		Handle = std::tmpfile(); // NOLINT(cppcoreguidelines-owning-memory): TFile owns it (Close)
#endif
		return Handle != nullptr;
	}

	// fclose: its result, 0 on a closed TFile.
	int Close() noexcept
	{
		if (Handle == nullptr)
			return 0;
		return std::fclose(std::exchange(Handle, nullptr));
	}

	[[nodiscard]] bool IsOpen() const noexcept { return Handle != nullptr; }
	// ferror: true when a read or write of the open file failed.
	[[nodiscard]] bool Failed() const noexcept { return std::ferror(Handle) != 0; }

	// fread and fwrite: the number of whole items of Size bytes.
	std::size_t Read(void *Buffer, std::size_t Size, std::size_t Count) noexcept
	{
		return std::fread(Buffer, Size, Count, Handle);
	}
	std::size_t Write(const void *Buffer, std::size_t Size, std::size_t Count) noexcept
	{
		return std::fwrite(Buffer, Size, Count, Handle);
	}
	// The characters of Text (fputs; fwrite of them): true when all were written.
	bool Write(std::string_view Text) noexcept
	{
		return Text.empty() || std::fwrite(Text.data(), 1, Text.size(), Handle) == Text.size();
	}
	// fputc: the character, or EOF.
	int PutChar(int Character) noexcept { return std::fputc(Character, Handle); }
	// getc and ungetc: the next byte (0-255) or EOF; the byte back, for the next GetChar.
	int GetChar() noexcept { return std::getc(Handle); }
	int UngetChar(int Character) noexcept { return std::ungetc(Character, Handle); }

#if PATELLA_HAS_FORMAT
	// fprintf with std::format's language: the number of characters, or -1 where they were not all written.
	template <typename... Arguments>
	int Print(std::format_string<TFormatArgument<Arguments>...> Format, Arguments &&...Values)
	{
		return PutFormatted(std::format(Format, PassFormatArgument(std::forward<Arguments>(Values))...));
	}
#else
	template <typename... Arguments>
	int Print(std::string_view Format, const Arguments &...Values)
	{
		const std::ptrdiff_t Size = patella::compat::FormatToNStandIn(nullptr, 0, Format, Values...).Size;
		std::string Text(static_cast<std::size_t>(Size), '\0');
		patella::compat::FormatToNStandIn(Text.data(), Size, Format, Values...);
		return PutFormatted(Text);
	}
#endif

	// fseek (Seek(0, SEEK_SET) for rewind, which reports nothing), fflush.
	int Seek(long Offset, int Origin) noexcept { return std::fseek(Handle, Offset, Origin); }
	int Flush() noexcept { return std::fflush(Handle); }

	// _fileno (fileno): the descriptor under the file, -1 for a text file off Windows (no descriptor of its own).
	[[nodiscard]] int Descriptor() const noexcept
	{
#if defined(_WIN32)
		return _fileno(Handle);
#else
		return fileno(Handle);
#endif
	}

	// The FILE's address as a number: what the original's error report shows of it (TTalkBuffer::Write's catch).
	[[nodiscard]] std::uintptr_t Address() const noexcept { return reinterpret_cast<std::uintptr_t>(Handle); }

	friend bool operator==(const TFile &File, std::nullptr_t) noexcept { return File.Handle == nullptr; }
	friend bool operator==(std::nullptr_t, const TFile &File) noexcept { return File.Handle == nullptr; }
	friend bool operator!=(const TFile &File, std::nullptr_t) noexcept { return File.Handle != nullptr; }
	friend bool operator!=(std::nullptr_t, const TFile &File) noexcept { return File.Handle != nullptr; }

private:
	// fopen of a usable mode on a closed TFile, the name in the system's form (Open, OpenSystemName).
	bool OpenSystem(const TSystemChar *Name, TAccess Access, TContent Content) noexcept
	{
#if defined(_WIN32)
		Handle = _wfsopen(Name, WideModeText(Access, Content).data(), _SH_DENYNO);
#else
		if (Content == TContent::Text)
			Handle = OpenText(Name, Access);
		else
			Handle = std::fopen(Name, ModeText(Access, Content)); // NOLINT(cppcoreguidelines-owning-memory): TFile owns it (Close)
#endif
		return Handle != nullptr;
	}

	std::FILE *Handle = nullptr;

	// A mode Open takes, on a closed TFile: else false with errno.
	[[nodiscard]] bool Usable(TAccess Access, TContent Content) const noexcept
	{
		if (Handle != nullptr) {
			errno = EBUSY;
			return false;
		}
		if (Access == TAccess::Read && Content == TContent::Text) {
			errno = EINVAL;
			return false;
		}
		return true;
	}

	static const char *ModeText(TAccess Access, TContent Content) noexcept
	{
		const bool Text = Content == TContent::Text;
		switch (Access) {
		case TAccess::Read:
			return "rb";
		case TAccess::Write:
			return Text ? "wt" : "wb";
		case TAccess::Append:
			return Text ? "at" : "ab";
		}
		return "rb";
	}

#if defined(_WIN32)
	// ModeText as the wide functions take it.
	static std::array<wchar_t, 4> WideModeText(TAccess Access, TContent Content) noexcept
	{
		const char *Mode = ModeText(Access, Content);
		std::array<wchar_t, 4> WideMode = {};
		for (std::size_t i = 0; i < 3 && Mode[i] != 0; i++)
			WideMode[i] = static_cast<wchar_t>(Mode[i]);
		return WideMode;
	}
#endif

	int PutFormatted(std::string_view Text) noexcept
	{
		return Write(Text) ? static_cast<int>(Text.size()) : -1;
	}

#if !defined(_WIN32)
	// The Microsoft C runtime's text mode for writing: a stream over the descriptor whose writes put "\r\n" for
	// every '\n'. The cookie is the descriptor.
	static int CookieDescriptor(void *Cookie) noexcept
	{
		return static_cast<int>(reinterpret_cast<std::intptr_t>(Cookie));
	}
	static bool WriteAll(int Descriptor, const char *Bytes, std::size_t Count) noexcept
	{
		while (Count > 0) {
			const ssize_t Written = ::write(Descriptor, Bytes, Count);
			if (Written < 0) {
				if (errno == EINTR)
					continue;
				return false;
			}
			Bytes += Written;
			Count -= static_cast<std::size_t>(Written);
		}
		return true;
	}
	static ssize_t TextWrite(void *Cookie, const char *Bytes, std::size_t Count) noexcept
	{
		const int Descriptor = CookieDescriptor(Cookie);
		std::array<char, 1024> Translated;
		std::size_t Used = 0;
		for (std::size_t i = 0; i < Count; i++) {
			if (Used + 2 > sizeof(Translated)) {
				if (!WriteAll(Descriptor, Translated.data(), Used))
					return -1;
				Used = 0;
			}
			if (Bytes[i] == '\n')
				Translated[Used++] = '\r';
			Translated[Used++] = Bytes[i];
		}
		if (!WriteAll(Descriptor, Translated.data(), Used))
			return -1;
		return static_cast<ssize_t>(Count);
	}
	static int TextClose(void *Cookie) noexcept { return ::close(CookieDescriptor(Cookie)); }

	static std::FILE *OpenText(const char *Name, TAccess Access) noexcept
	{
		const int Flags = O_WRONLY | O_CREAT | O_CLOEXEC | (Access == TAccess::Append ? O_APPEND : O_TRUNC);
		const int Descriptor = ::open(Name, Flags, 0666); // NOLINT(cppcoreguidelines-pro-type-vararg): open's mode
		if (Descriptor < 0)
			return nullptr;
		// glibc declares the type in <stdio.h> (from bits/types/cookie_io_functions_t.h)
		const cookie_io_functions_t Functions = {nullptr, &TextWrite, nullptr, &TextClose}; // NOLINT(misc-include-cleaner)
		// "w" either way: the descriptor appends (O_APPEND), and the stream has no seek function to append with
		std::FILE *Stream = fopencookie(reinterpret_cast<void *>(static_cast<std::intptr_t>(Descriptor)), "w", Functions);
		if (Stream == nullptr) {
			const int Error = errno;
			::close(Descriptor);
			errno = Error;
		}
		return Stream;
	}
#endif
};

// The layout the frozen structures rely on: exactly a FILE *.
inline constexpr bool IsFileLayout = sizeof(TFile) == sizeof(std::FILE *) && alignof(TFile) == alignof(std::FILE *)
	&& std::is_standard_layout_v<TFile>;
static_assert(IsFileLayout);
