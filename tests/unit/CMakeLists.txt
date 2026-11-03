# The unit tests (notes/modernization-plan.md, 4.6; issue #9): doctest, no proprietary file, a CTest
# test per program and one per contract violation scenario.
#
# - tests_unit: src/compat/cxx26.h as the compiler selects it (MSVC 19.51: the stand-ins; GCC 16:
#   contract_assert and std::inplace_vector).
# - tests_unit_cxx26_standin: the same sources with PATELLA_CXX26_STANDIN, so on GCC 16 the
#   stand-ins run the tests the real features pass.
#
# - tests_core: patella_core (cmake/Core.cmake, issue #7): vlong and TRSA, TXTEA, Utils and
#   TMyException's messages, Skins, the regexp and Container.h; the expected values that are not the
#   code's own come from tests/unit/core_expected.py.
#
# - tests_testmode: patella_testmode (cmake/TestMode.cmake, issue #21): the virtual clock, the input
#   script's parser and key table, the scheduler, the driver's loop on the virtual clock, and that the
#   scripts in tests/testmode parse.
#
# All get the product settings of cmake/Warnings.cmake and cmake/Hardening.cmake.

include(FetchContent)

# doctest: an installed one first (find_package), otherwise the pinned release.
set(DOCTEST_WITH_TESTS OFF)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF)
set(DOCTEST_NO_INSTALL ON)
FetchContent_Declare(doctest
	URL https://github.com/doctest/doctest/releases/download/v2.5.3/doctest-v2.5.3.tar.gz
	URL_HASH SHA256=eb4a39729d091e43d4d593888bf4d145ee9a0f0e13f7404484b6f2fb60d5d50f
	FIND_PACKAGE_ARGS 2.5.3 CONFIG)
FetchContent_MakeAvailable(doctest)

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

# Which path tests_unit must have taken (PATELLA_EXPECT_CXX26: 1 the real features, 0 the
# stand-ins), where it is known; elsewhere the tests take what the header selected.
if(MSVC)
	set(PATELLA_EXPECT_CXX26 0)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
	set(PATELLA_EXPECT_CXX26 1)
else()
	set(PATELLA_EXPECT_CXX26 "")
endif()

function(patella_unit_tests target)
	add_executable(${target}
		main.cpp
		cxx26_contracts_test.cpp
		cxx26_static_vector_test.cpp)
	target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	target_link_libraries(${target} PRIVATE doctest::doctest)
	patella_target_language(${target})
	patella_target_warnings(${target})
	patella_target_hardening(${target})

	add_test(NAME ${target} COMMAND ${target})
	foreach(scenario IN ITEMS predicate_false evaluation_exception precondition)
		if(scenario STREQUAL "predicate_false")
			set(function ViolateFalse)
			set(predicate "Value > 0")
		elseif(scenario STREQUAL "evaluation_exception")
			set(function ViolateThrow)
			set(predicate "Throws(Value)")
		else()
			set(function TStaticVector)
			set(predicate "Index < Length")
		endif()
		add_test(NAME ${target}.contract_violation.${scenario}
			COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:${target}>" "-DSCENARIO=${scenario}"
				"-DFUNCTION=${function}" "-DPREDICATE=${predicate}"
				-P "${CMAKE_CURRENT_SOURCE_DIR}/RunContractViolation.cmake")
		# 77: the scenario does not apply to the path (the precondition checks of the stand-in).
		set_tests_properties(${target}.contract_violation.${scenario} PROPERTIES SKIP_RETURN_CODE 77)
	endforeach()
endfunction()

patella_unit_tests(tests_unit)
if(NOT PATELLA_EXPECT_CXX26 STREQUAL "")
	target_compile_definitions(tests_unit PRIVATE PATELLA_EXPECT_CXX26=${PATELLA_EXPECT_CXX26})
endif()

patella_unit_tests(tests_unit_cxx26_standin)
target_compile_definitions(tests_unit_cxx26_standin PRIVATE PATELLA_CXX26_STANDIN PATELLA_EXPECT_CXX26=0)

add_executable(tests_core
	core_main.cpp
	core_platform_test.cpp
	core_vlong_test.cpp
	core_txtea_test.cpp
	core_utils_test.cpp
	core_skins_test.cpp
	core_regexp_test.cpp
	core_container_test.cpp)
target_link_libraries(tests_core PRIVATE patella_core doctest::doctest)
patella_target_language(tests_core)
patella_target_warnings(tests_core)
patella_target_hardening(tests_core)
add_test(NAME tests_core COMMAND tests_core)

add_executable(tests_testmode
	testmode_main.cpp
	testmode_clock_test.cpp
	testmode_script_test.cpp)
target_link_libraries(tests_testmode PRIVATE patella_testmode doctest::doctest)
target_compile_definitions(tests_testmode PRIVATE "PATELLA_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")
patella_target_language(tests_testmode)
patella_target_warnings(tests_testmode)
patella_target_hardening(tests_testmode)
add_test(NAME tests_testmode COMMAND tests_testmode)
