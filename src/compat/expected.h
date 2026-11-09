// expected.h: C++23's std::expected for the recoverable failures (Patella-Modern issue #80, M5), and a C++17 stand-in,
// written for the 32-bit oracle's /std:c++17 (which left the fork with issue #94) and kept for tests/unit's stand-in
// build. Not part of the original source.
//
// After this header PATELLA_HAS_EXPECTED is 1 where the library has std::expected (every product preset: MSVC
// /std:c++latest, GCC 16, Clang 23) and 0 elsewhere (a C++17 library) or with PATELLA_EXPECTED_STANDIN, which
// tests/unit's stand-in build defines for a whole target (never for one file: the two are different types), so that
// the stand-in runs the tests std::expected passes, on the same compiler.
//
// patella::TExpected<T, E> is std::expected<T, E> and patella::TUnexpected<E> std::unexpected<E> where
// PATELLA_HAS_EXPECTED is 1, and the stand-ins otherwise, with the part of the interface the code uses and the same
// meaning:
//	- TUnexpected<E>(Error), error();
//	- TExpected<T, E>: from a T (a value; T default-constructible: also empty), from a TUnexpected<G> (an error),
//	  has_value, explicit operator bool, operator* and operator-> (a value), value_or, error() (an error);
//	- TExpected<void, E>: empty (a value), from a TUnexpected<G>, has_value, operator bool, error().
// What it does not have: value() (std::expected's throws bad_expected_access; the code tests has_value first),
// emplace, swap, the monadic operations (and_then, transform, or_else), comparisons, in-place construction. Like
// std::expected, operator* and error() on the wrong alternative are undefined (the stand-in reads a std::variant
// with std::get_if and does not check either). The code spells TUnexpected's argument type (no deduction guides).

#pragma once

#include <version> // IWYU pragma: keep (the feature-test macro of the #if line)

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L && !defined(PATELLA_EXPECTED_STANDIN)
#define PATELLA_HAS_EXPECTED 1
#include <expected> // IWYU pragma: export
#else
#define PATELLA_HAS_EXPECTED 0
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#endif

namespace patella {

#if PATELLA_HAS_EXPECTED

template <class T, class E>
using TExpected = std::expected<T, E>;

template <class E>
using TUnexpected = std::unexpected<E>;

#else

template <class E>
class TUnexpected {
public:
	constexpr explicit TUnexpected(const E &Error_) : Error(Error_) {}
	constexpr explicit TUnexpected(E &&Error_) : Error(std::move(Error_)) {}
	[[nodiscard]] constexpr const E &error() const & noexcept { return Error; }
	[[nodiscard]] constexpr E &error() & noexcept { return Error; }
	[[nodiscard]] constexpr E &&error() && noexcept { return std::move(Error); }

private:
	E Error;
};

template <class T, class E>
class TExpected {
public:
	using value_type = T;
	using error_type = E;

	template <class U = T, std::enable_if_t<std::is_default_constructible_v<U>, int> = 0>
	constexpr TExpected() : Storage(std::in_place_index<0>)
	{
	}
	constexpr TExpected(const T &Value) : Storage(std::in_place_index<0>, Value) {}
	constexpr TExpected(T &&Value) : Storage(std::in_place_index<0>, std::move(Value)) {}
	template <class G>
	constexpr TExpected(const TUnexpected<G> &Error) : Storage(std::in_place_index<1>, Error.error())
	{
	}
	template <class G>
	constexpr TExpected(TUnexpected<G> &&Error) : Storage(std::in_place_index<1>, std::move(Error).error())
	{
	}

	[[nodiscard]] constexpr bool has_value() const noexcept { return Storage.index() == 0; }
	constexpr explicit operator bool() const noexcept { return has_value(); }
	[[nodiscard]] constexpr const T &operator*() const & noexcept { return *std::get_if<0>(&Storage); }
	[[nodiscard]] constexpr T &operator*() & noexcept { return *std::get_if<0>(&Storage); }
	[[nodiscard]] constexpr const T *operator->() const noexcept { return std::get_if<0>(&Storage); }
	[[nodiscard]] constexpr T *operator->() noexcept { return std::get_if<0>(&Storage); }
	template <class U>
	[[nodiscard]] constexpr T value_or(U &&Default) const &
	{
		return has_value() ? **this : static_cast<T>(std::forward<U>(Default));
	}
	[[nodiscard]] constexpr const E &error() const & noexcept { return *std::get_if<1>(&Storage); }
	[[nodiscard]] constexpr E &error() & noexcept { return *std::get_if<1>(&Storage); }

private:
	std::variant<T, E> Storage;
};

template <class E>
class TExpected<void, E> {
public:
	using value_type = void;
	using error_type = E;

	constexpr TExpected() noexcept = default;
	template <class G>
	constexpr TExpected(const TUnexpected<G> &Error_) : Error(std::in_place, Error_.error())
	{
	}
	template <class G>
	constexpr TExpected(TUnexpected<G> &&Error_) : Error(std::in_place, std::move(Error_).error())
	{
	}

	[[nodiscard]] constexpr bool has_value() const noexcept { return !Error.has_value(); }
	constexpr explicit operator bool() const noexcept { return has_value(); }
	constexpr void operator*() const noexcept {}
	[[nodiscard]] constexpr const E &error() const & noexcept { return *Error; }
	[[nodiscard]] constexpr E &error() & noexcept { return *Error; }

private:
	std::optional<E> Error;
};

#endif

} // namespace patella
