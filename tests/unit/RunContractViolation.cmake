# One contract violation scenario of the unit tests (tests/unit/CMakeLists.txt):
#	cmake -DPROGRAM=<tests_unit> -DSCENARIO=<name> -DFUNCTION=<name> -DPREDICATE=<text> -P RunContractViolation.cmake
# The program runs `--contract-violation <scenario>`. It passes when the program ended abnormally
# (not exit code 0) before "after the violation", and its stderr has the violation's message:
#	contract violation in function <...FUNCTION...> at <file>:<line>: <PREDICATE>
#	[assertion_kind: assert, semantic: enforce, ...
# which contract_assert (GCC 16) and the stand-in of src/compat/cxx26.h both print. Exit code 77 of
# the program (the scenario does not apply to the path) is passed on: CTest's skip.

cmake_minimum_required(VERSION 3.30)

foreach(variable PROGRAM SCENARIO FUNCTION PREDICATE)
	if(NOT DEFINED ${variable})
		message(FATAL_ERROR "RunContractViolation.cmake: ${variable} is not set")
	endif()
endforeach()

execute_process(
	COMMAND "${PROGRAM}" --contract-violation "${SCENARIO}"
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error)
message("exit: ${result}\nstdout:\n${output}\nstderr:\n${error}")

if(result STREQUAL "77")
	message("skipped: the scenario does not apply to this path")
	cmake_language(EXIT 77)
endif()

function(escape text out)
	string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1" escaped "${text}")
	set(${out} "${escaped}" PARENT_SCOPE)
endfunction()
escape("${FUNCTION}" function)
escape("${PREDICATE}" predicate)

set(failures "")
if(result STREQUAL "0")
	list(APPEND failures "the program ended normally")
endif()
if(output MATCHES "after the violation")
	list(APPEND failures "the program continued after the violation")
endif()
if(NOT error MATCHES "contract violation in function [^\n]*${function}[^\n]* at [^\n]+:[0-9]+: ${predicate}\n\\[assertion_kind: assert, semantic: enforce, ")
	list(APPEND failures "stderr has not the violation's message")
endif()
if(failures)
	list(JOIN failures "; " failures)
	message(FATAL_ERROR "contract violation ${SCENARIO}: ${failures}")
endif()
message("contract violation ${SCENARIO}: passed")
