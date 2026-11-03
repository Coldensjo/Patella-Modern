// CONTRACT_ASSERT of src/compat/cxx26.h: the same tests for contract_assert (GCC 16) and the
// stand-in (MSVC, and GCC with PATELLA_CXX26_STANDIN). The violations end the program, so they run
// in a process of their own (RunContractViolation below, checked by RunContractViolation.cmake).

#include <doctest/doctest.h>

#include "compat/cxx26.h"

#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace {

int Calls = 0;

bool Counted(bool Result)
{
	++Calls;
	return Result;
}

constexpr int Checked(int Value)
{
	CONTRACT_ASSERT(Value > 0);
	return Value;
}

int Branch(int Value)
{
	if (Value > 0)
		CONTRACT_ASSERT(Value > 0);
	else
		CONTRACT_ASSERT(Value <= 0);
	return Value;
}

bool Throws(int Value)
{
	if (Value == 0)
		throw std::runtime_error("the predicate threw");
	return true;
}

void ViolateFalse(int Value)
{
	CONTRACT_ASSERT(Value > 0);
}

void ViolateThrow(int Value)
{
	CONTRACT_ASSERT(Throws(Value));
}

} // namespace

TEST_CASE("cxx26.h selects the expected contract path")
{
#ifdef PATELLA_EXPECT_CXX26
	CHECK(PATELLA_HAS_CONTRACTS == PATELLA_EXPECT_CXX26);
#endif
#ifdef PATELLA_CXX26_STANDIN
	CHECK(PATELLA_HAS_CONTRACTS == 0);
#endif
	CHECK((PATELLA_HAS_CONTRACTS == 0 || PATELLA_HAS_CONTRACTS == 1));
}

TEST_CASE("CONTRACT_ASSERT evaluates a true predicate once and continues")
{
	Calls = 0;
	CONTRACT_ASSERT(Counted(true));
	CHECK(Calls == 1);
	CONTRACT_ASSERT(Counted(true) && Counted(true));
	CHECK(Calls == 3);
}

TEST_CASE("CONTRACT_ASSERT in constant evaluation and in constexpr functions")
{
	static_assert(Checked(3) == 3);
	constexpr int Value = Checked(7);
	CHECK(Value == 7);
	CHECK(Checked(1) == 1);
}

TEST_CASE("CONTRACT_ASSERT takes predicates with commas inside brackets")
{
	CONTRACT_ASSERT(std::is_same_v<int, int>);
	CONTRACT_ASSERT(std::is_convertible_v<short, int> && !std::is_same_v<short, int>);
	CHECK(true);
}

TEST_CASE("CONTRACT_ASSERT is one statement")
{
	CHECK(Branch(2) == 2);
	CHECK(Branch(-2) == -2);
}

// Zero is 0, from the command line, so that the violation happens at run time.
int RunContractViolation(std::string_view Scenario, int Zero)
{
	if (Scenario == "predicate_false")
		ViolateFalse(Zero);
	else if (Scenario == "evaluation_exception")
		ViolateThrow(Zero);
	else if (Scenario == "precondition") {
		// The stand-in TStaticVector checks its preconditions; std::inplace_vector's are undefined
		// behaviour (77: CTest's skip).
		if (PATELLA_HAS_INPLACE_VECTOR)
			return 77;
		patella::TStaticVector<int, 2> Vector{1};
		std::printf("element %d\n", Vector[static_cast<std::size_t>(1 - Zero)]);
	} else {
		std::printf("unknown scenario\n");
		return 2;
	}
	std::printf("after the violation\n");
	return 0;
}
