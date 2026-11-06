// import std (issue #113, plan section 7, M7): the standard library as the named module std, the proof that the
// presets with PATELLA_IMPORT_STD (cmake/ImportStd.cmake) build and run new code that imports it. A few of the
// library's parts through the module: formatting, ranges and algorithms, containers, std::expected and std::span.
// No #include: a module import and the library's headers in one translation unit are not what new code does here,
// and the module exports no macros (no EXIT_SUCCESS, no stdout), so the result is the return value.

import std;

namespace {

int Failures = 0;

void Check(bool Condition, std::string_view What)
{
	if (!Condition) {
		++Failures;
		std::println(std::cerr, "tests_import_std: failed: {}", What);
	}
}

// A recoverable failure as the code returns it (src/compat/expected.h: std::expected where the library has it).
std::expected<int, std::string> ParseDigit(char Character)
{
	if (Character < '0' || Character > '9') {
		return std::unexpected(std::format("not a digit: '{}'", Character));
	}
	return Character - '0';
}

} // namespace

int main()
{
	// Formatting.
	Check(std::format("{:>5}|{:<3}|{:#x}", 42, "ab", 255) == "   42|ab |0xff", "std::format");

	// Ranges and algorithms.
	const std::vector<int> Values{5, 3, 8, 1};
	Check(std::ranges::fold_left(Values, 0, std::plus{}) == 17, "std::ranges::fold_left");
	auto Sorted = Values;
	std::ranges::sort(Sorted);
	Check(Sorted == std::vector<int>{1, 3, 5, 8}, "std::ranges::sort");
	auto Even = Values | std::views::filter([](int Value) { return Value % 2 == 0; });
	Check(std::ranges::distance(Even) == 1 && *Even.begin() == 8, "std::views::filter");

	// Containers and std::span.
	std::map<std::string, int, std::less<>> Counts;
	for (const std::string_view Word : {"a", "b", "a"}) {
		++Counts[std::string{Word}];
	}
	Check(Counts.size() == 2 && Counts.find("a")->second == 2, "std::map");
	const std::array<std::uint8_t, 4> Bytes{1, 2, 3, 4};
	const std::span<const std::uint8_t> Tail = std::span{Bytes}.subspan(2);
	Check(Tail.size() == 2 && Tail[0] == 3, "std::span");

	// std::expected.
	Check(ParseDigit('7') == 7, "std::expected value");
	const auto Error = ParseDigit('x');
	Check(!Error.has_value() && Error.error() == "not a digit: 'x'", "std::expected error");

	if (Failures != 0) {
		return 1;
	}
	std::println("tests_import_std: all checks passed");
	return 0;
}
