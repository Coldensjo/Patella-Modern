// The unit tests (notes/modernization-plan.md, 4.6): doctest's runner, which first names the paths
// src/compat/cxx26.h selected. With `--contract-violation <scenario>` the program instead runs one
// contract violation (cxx26_contracts_test.cpp), which ends it; RunContractViolation.cmake checks how.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "compat/cxx26.h"

#include <cstdio>
#include <cstdlib>
#include <print>
#include <string_view>

int RunContractViolation(std::string_view Scenario, int Zero);

int main(int argc, char **argv)
{
#ifdef _WIN32
	// A failed test or a contract violation ends the process with exit code 3, without the abort
	// message box (Debug CRT) and without Windows Error Reporting.
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
	if (argc == 3 && std::string_view(argv[1]) == "--contract-violation")
		return RunContractViolation(argv[2], argc - 3);

	std::print("cxx26.h: contracts {}, inplace_vector {}\n", PATELLA_HAS_CONTRACTS ? "real" : "stand-in",
		PATELLA_HAS_INPLACE_VECTOR ? "real" : "stand-in");
	std::fflush(stdout);
	doctest::Context Context(argc, argv);
	return Context.run();
}
