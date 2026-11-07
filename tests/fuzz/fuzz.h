// fuzz.h: what the fuzz targets of tests/fuzz share (Patella-Modern issue #132; tests/fuzz/CMakeLists.txt).
//
// Each target is one LLVMFuzzerTestOneInput (and an LLVMFuzzerInitialize). With PATELLA_FUZZ=ON it is linked with
// libFuzzer (Clang's -fsanitize=fuzzer, MSVC's /fsanitize=fuzzer); otherwise with driver.cpp, which replays the inputs
// it is given and a fixed number of its own mutations of them (the CTest smoke runs, every preset).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Whether the build has AddressSanitizer (MSVC and GCC: __SANITIZE_ADDRESS__; Clang: the feature).
#if defined(__SANITIZE_ADDRESS__)
#define PATELLA_FUZZ_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define PATELLA_FUZZ_ASAN 1
#endif
#endif
#if !defined(PATELLA_FUZZ_ASAN)
#define PATELLA_FUZZ_ASAN 0
#endif

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *Data, std::size_t Size);
extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv);

namespace patella::fuzz {

// A folder of this process's own below the temporary folder becomes the working directory (the loaders open their files
// by name, Tibia.cfg's and Patella.ini's folder); removed when the process ends. Called by LLVMFuzzerInitialize.
void EnterWorkFolder(const char *Target);

// Writes the bytes to the file Name of the working directory (created or replaced); aborts when that fails.
void WriteFile(const char *Name, const std::uint8_t *Data, std::size_t Size);
void WriteFile(const char *Name, const std::string &Text);

// Whether the file exists in the working directory; removes it (nothing when there is none).
bool FileExists(const char *Name);
void RemoveFile(const char *Name);

// A finding of the harness itself (a property that does not hold, an exception the client's code is not expected to
// throw): printed with the target's name, then abort(), which libFuzzer and the smoke runs report as a crash.
[[noreturn]] void Fail(const char *Target, const std::string &What);

// The harness's own reading of the input (little-endian, as the client's files and packets): 0 past the end.
class TInput {
public:
	TInput(const std::uint8_t *Data, std::size_t Size) : Begin(Data), Length(Size) {}
	[[nodiscard]] bool Empty() const { return Position >= Length; }
	[[nodiscard]] std::size_t Left() const { return Position < Length ? Length - Position : 0; }
	std::uint8_t Byte() { return Position < Length ? Begin[Position++] : 0; }
	std::uint16_t Word()
	{
		const std::uint16_t Low = Byte();
		return static_cast<std::uint16_t>(Low | (Byte() << 8));
	}
	// The next Count bytes (fewer at the end).
	std::string Bytes(std::size_t Count)
	{
		const std::size_t Taken = Count < Left() ? Count : Left();
		std::string Result(reinterpret_cast<const char *>(Begin + Position), Taken);
		Position += Taken;
		return Result;
	}
	std::string Rest() { return Bytes(Left()); }

private:
	const std::uint8_t *Begin;
	std::size_t Length;
	std::size_t Position = 0;
};

} // namespace patella::fuzz
