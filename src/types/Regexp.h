// Regexp.h: TRegexp, a compiled expression of Henry Spencer's regexp (src/Extern/regexp, the client's of 1986,
// compiled as C17) that owns what regcomp returned. Patella-Modern issue #129 (notes/modernization-plan.md, section
// 7, M8; notes/decisions.md, "The regexp behind TRegexp"). Not part of the original source.
//
// regcomp mallocs the program and the caller frees it; the client's one user (Status.cpp's link check of copied
// text) did that by hand, with free. A TRegexp frees it when it is reset, assigned or destroyed, so no caller does.
// The regexp's own behaviour is the 1986 one, untouched: its syntax (egrep's, no {n,m}, no classes like [:alpha:]),
// leftmost-first alternation, and NSUBEXP (10) subexpressions; std::regex is not a drop-in for it.
//
// Every call goes to regcomp and regexec as they are, with their errors: a pattern that does not compile, or
// nullptr, leaves the TRegexp empty, and Execute on an empty one is regexec(NULL, ...), which returns 0; both tell
// regerrorhandler (nothing in the client sets it). Neither function writes to the text it is given, though the K&R
// interface takes char *.

#pragma once

#include "Extern/regexp/regexp.h"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string_view>

namespace patella {

class TRegexp {
public:
	// Empty: Execute returns 0, as regexec on NULL.
	TRegexp() noexcept = default;
	// regcomp(Pattern); empty when it fails (Pattern nullptr, a syntax error, a program over 32,767 bytes).
	explicit TRegexp(const char *Pattern) noexcept : Program(regcomp(const_cast<char *>(Pattern))) {}

	// Whether regcomp gave a program.
	[[nodiscard]] bool IsCompiled() const noexcept { return Program != nullptr; }
	explicit operator bool() const noexcept { return IsCompiled(); }

	// regexec on Text: 1 when the expression matches somewhere in it (the subexpressions then point into Text), 0
	// when not, when Text is nullptr or the TRegexp is empty.
	int Execute(const char *Text) noexcept { return regexec(Program.get(), const_cast<char *>(Text)); }

	// Subexpression Number (0: the whole match, 1 to NSUBEXP - 1: the parentheses in order) of the last match, as
	// regexec left it: empty when that one did not take part in the match, the TRegexp is empty or Number is out
	// of range. Valid while the text of the last Execute is.
	[[nodiscard]] std::string_view Subexpression(int Number) const noexcept
	{
		if (!Program || Number < 0 || Number >= NSUBEXP)
			return {};
		const char *const Start = Program->startp[Number];
		const char *const End = Program->endp[Number];
		if (Start == nullptr || End == nullptr || End < Start)
			return {};
		return {Start, static_cast<std::size_t>(End - Start)};
	}

	// Frees the program; the TRegexp is empty again.
	void Reset() noexcept { Program.reset(); }

	// The compiled program itself (nullptr when empty), for the tests that read its fields.
	[[nodiscard]] const regexp *Get() const noexcept { return Program.get(); }

private:
	// regcomp's allocation is malloc's.
	struct TFree {
		void operator()(regexp *P) const noexcept { std::free(P); } // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): regcomp's malloc
	};
	std::unique_ptr<regexp, TFree> Program;
};

} // namespace patella
