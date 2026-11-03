# The golden corpus's runner (notes/modernization-plan.md, 4.3; issue #24): golden_runner replays the
# corpus that the oracle-x86 preset's golden_corpus target records (tests/golden/golden.h), against this
# build's code and without Tibia.exe. So far on Windows x64 with MSVC (plan M2); the Linux runner comes
# later.
#
# CTest test golden: the corpus in PATELLA_GOLDEN_DIR (default build/golden of the source tree, where
# golden_corpus records it). Without a corpus (hosted CI: it is derived from Tibia.exe and never leaves
# the recording machine) the runner says so and the test is skipped (exit code 77); with
# PATELLA_GOLDEN_REQUIRE=ON (the self-hosted workflow) a missing corpus fails it.
#
# The ported tests (core_tests.cpp, regexp_tests.cpp, sysinc_tests.cpp) are the oracle's difftest and sysinctest sources
# as well (cmake/OracleX86.cmake); here they see no original (GOLDEN_ORACLE unset).

if(NOT (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8))
	return()
endif()

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

set(PATELLA_GOLDEN_DIR "${PROJECT_SOURCE_DIR}/build/golden" CACHE PATH
	"The golden corpus that the CTest test golden replays (recorded by oracle-x86's golden_corpus; local only)")
option(PATELLA_GOLDEN_REQUIRE "The CTest test golden fails instead of being skipped when there is no corpus" OFF)

# sysinc.cpp is not portable yet (the clipboard, timeGetTime): compiled against src/compat/mfc_shim.h,
# as the oracle's sysinctest compiles it.
add_library(golden_sysinc OBJECT "${PROJECT_SOURCE_DIR}/src/sysinc.cpp")
target_include_directories(golden_sysinc PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(golden_sysinc PRIVATE TIBIA_MFC_SHIM)
patella_target_language(golden_sysinc)
patella_target_warnings(golden_sysinc)
patella_target_hardening(golden_sysinc)

add_executable(golden_runner
	runner.cpp
	golden.cpp
	core_tests.cpp
	regexp_tests.cpp
	sysinc_tests.cpp
	$<TARGET_OBJECTS:golden_sysinc>)
target_link_libraries(golden_runner PRIVATE patella_core winmm user32)
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/build/golden" golden_default_dir)
target_compile_definitions(golden_runner PRIVATE "GOLDEN_DEFAULT_DIR=\"${golden_default_dir}\"")
patella_target_language(golden_runner)
patella_target_warnings(golden_runner)
patella_target_hardening(golden_runner)

if(PATELLA_GOLDEN_REQUIRE)
	add_test(NAME golden COMMAND golden_runner --require "${PATELLA_GOLDEN_DIR}")
else()
	add_test(NAME golden COMMAND golden_runner "${PATELLA_GOLDEN_DIR}")
endif()
set_tests_properties(golden PROPERTIES SKIP_RETURN_CODE 77)
