# One recording of golden_corpus (cmake/OracleX86.cmake; issue #66): the test program in a temporary folder of
# its own, its TEMP, TMP and current directory (<TMPROOT>/golden-<NAME>-<time and random suffix>), removed
# afterwards, as golden_runner's run folder (tests/golden/runner.cpp, TRunFolder).
# The program runs with TZ set to -DTZ (issue #93: UTC0, GOLDEN_TIME_ZONE of cmake/OracleX86.cmake), so that the local
# times in the corpus (Talk's and Creatures' fixed clocks) do not depend on the recording machine's zone; the
# corpus's MANIFEST.tsv names the zone, and golden_runner replays in it.
# Arguments: -DTEST=<difftest.exe ...> -DTIBIA=<Tibia.exe> -DRECORD=<corpus directory> -DTMPROOT=<folder>
# -DNAME=<test name> -DTZ=<POSIX time zone>

foreach(argument TEST TIBIA RECORD TMPROOT NAME TZ)
	if(NOT DEFINED ${argument})
		message(FATAL_ERROR "RunGoldenRecord.cmake: -D${argument}=... is missing")
	endif()
endforeach()

file(MAKE_DIRECTORY "${TMPROOT}")
string(TIMESTAMP stamp "%Y%m%d%H%M%S%f")
string(RANDOM LENGTH 6 ALPHABET 0123456789abcdef RANDOM_SEED "${stamp}" suffix)
set(dir "${TMPROOT}/golden-${NAME}-${stamp}-${suffix}")
if(EXISTS "${dir}")
	message(FATAL_ERROR "RunGoldenRecord.cmake: ${dir} exists already")
endif()
file(MAKE_DIRECTORY "${dir}")
file(TO_NATIVE_PATH "${dir}" native)

set(ENV{DIFFTEST_RECORD} "${RECORD}")
set(ENV{TEMP} "${native}")
set(ENV{TMP} "${native}")
set(ENV{TZ} "${TZ}")
execute_process(COMMAND "${TEST}" "${TIBIA}" WORKING_DIRECTORY "${dir}" RESULT_VARIABLE result)
file(REMOVE_RECURSE "${dir}")
if(EXISTS "${dir}")
	message(WARNING "RunGoldenRecord.cmake: cannot remove ${dir}")
endif()
if(NOT result EQUAL 0)
	message(FATAL_ERROR "${NAME} failed (${result})")
endif()
