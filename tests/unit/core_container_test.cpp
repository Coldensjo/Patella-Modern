// Container.h's templates: array, matrix and matrix3d, with their errors (TMyException messages naming Container.h
// and the original's lines); the growing vector is TIndexedVector since issue #99 (indexed_vector_test.cpp). On the GCC
// and Clang warning baselines (cmake/Warnings.cmake): Container.h's constructor parameters hide its members (-Wshadow)
// and its new[] sizes are int (-Wsign-conversion); a warning fix of M3.

#include <doctest/doctest.h>

#include "Container.h"

#include <string>

namespace {

// An assertion's message names its own line since issue #79 (PATELLA_ASSERT: the line moves with the code), an
// error's (TERROR, TMYEXCEPTION) since issue #80, so a message is compared as "<file> N: ...".
std::string WithoutLine(const std::string &Message)
{
	const std::string::size_type Space = Message.find(' ');
	const std::string::size_type Colon = Message.find(": ");
	if (Space == std::string::npos || Colon == std::string::npos || Colon < Space)
		return Message;
	return Message.substr(0, Space + 1) + "N" + Message.substr(Colon);
}

template <class F> std::string MessageOf(F Function)
{
	try {
		Function();
	} catch (const TMyException &Exception) {
		return WithoutLine(Exception.GetMessage());
	}
	return "no exception";
}

} // namespace

TEST_CASE("Container.h: array")
{
	array<int> A(-2, 2);
	CHECK(A.low() == -2);
	CHECK(A.high() == 2);
	A(-2) = 5;
	A(2) = 7;
	CHECK(A(-2) == 5);
	CHECK(A(2) == 7);
	CHECK(MessageOf([&] { A(3); })
		== "Container.h N: index out of range (original i: 3, i: 5, min: -2, d: 5)");
	CHECK(MessageOf([] { array<int> Empty(1, 0); }) == "Container.h N: assertion failed (d = 0), reason:\nd>0");
}

TEST_CASE("Container.h: matrix and matrix3d")
{
	matrix<int> M(0, 2, 10, 11);
	M(2, 11) = 9;
	CHECK(M(2, 11) == 9);
	CHECK(M.entry[1 * 3 + 2] == 9);
	CHECK(MessageOf([&] { M(0, 12); })
		== "Container.h N: index out of range (original x: 0, original y: 12, x: 0, y: 2, xmin: 0, ymin: 10, dx: 3, dy: 2)");
	CHECK(MessageOf([] { matrix<int> Empty(0, 1, 5, 4); }) == "Container.h N: assertion failed (dy = 0), reason:\ndy>0");

	matrix3d<int> C(0, 1, 0, 1, -1, 0);
	C(1, 0, 0) = 4;
	CHECK(C.entry[(1 * 2 + 0) * 2 + 1] == 4);
	CHECK(MessageOf([&] { C(0, 0, 1); })
		== "Container.h N: index out of range (original x: 0, original y: 0, original z: 1, x: 0, y: 0, z: 2, "
		   "xmin: 0, ymin: 0, zmin: -1, dx: 2, dy: 2, dz: 2), reason:\n[bug0000013]");
}
