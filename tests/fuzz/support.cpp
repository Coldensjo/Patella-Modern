// support.cpp: the fuzz targets' work folder, files and failures (fuzz.h; Patella-Modern issue #132).

#include "fuzz.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// The process's folder: made once, removed at the end (the old working directory back first).
class TWorkFolder {
public:
	TWorkFolder() = default;
	TWorkFolder(const TWorkFolder &) = delete;
	TWorkFolder &operator=(const TWorkFolder &) = delete;
	TWorkFolder(TWorkFolder &&) = delete;
	TWorkFolder &operator=(TWorkFolder &&) = delete;
	~TWorkFolder()
	{
		if (Path.empty())
			return;
		std::error_code Error;
		fs::current_path(Old, Error);
		fs::remove_all(Path, Error);
	}
	void Enter(const char *Target)
	{
		if (!Path.empty())
			return;
		std::error_code Error;
		Old = fs::current_path(Error);
		std::random_device Device;
		for (int Attempt = 0; Attempt < 100 && Path.empty(); Attempt++) {
			const fs::path Candidate =
				fs::temp_directory_path() / ("patella-fuzz-" + std::string(Target) + "-" + std::to_string(Device()));
			if (fs::create_directory(Candidate, Error))
				Path = Candidate;
		}
		if (Path.empty())
			patella::fuzz::Fail(Target, "no work folder below " + fs::temp_directory_path().string());
		fs::current_path(Path, Error);
		if (Error)
			patella::fuzz::Fail(Target, "cannot enter " + Path.string());
	}

private:
	fs::path Path;
	fs::path Old;
};

TWorkFolder WorkFolder;

} // namespace

void patella::fuzz::EnterWorkFolder(const char *Target)
{
	WorkFolder.Enter(Target);
}

void patella::fuzz::WriteFile(const char *Name, const std::uint8_t *Data, std::size_t Size)
{
	std::ofstream File(Name, std::ios::binary | std::ios::trunc);
	if (Size != 0)
		File.write(reinterpret_cast<const char *>(Data), static_cast<std::streamsize>(Size));
	File.close();
	if (!File)
		Fail("support", std::string("cannot write ") + Name);
}

void patella::fuzz::WriteFile(const char *Name, const std::string &Text)
{
	WriteFile(Name, reinterpret_cast<const std::uint8_t *>(Text.data()), Text.size());
}

bool patella::fuzz::FileExists(const char *Name)
{
	std::error_code Error;
	return fs::exists(Name, Error);
}

void patella::fuzz::RemoveFile(const char *Name)
{
	std::error_code Error;
	fs::remove(Name, Error);
}

void patella::fuzz::Fail(const char *Target, const std::string &What)
{
	std::fprintf(stderr, "FUZZ FINDING (%s): %s\n", Target, What.c_str());
	std::fflush(stderr);
	std::abort();
}
