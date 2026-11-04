# The unit tests (notes/modernization-plan.md, 4.6; issue #9): doctest, no proprietary file, a CTest
# test per program and one per contract violation scenario.
#
# - tests_unit: src/compat/cxx26.h as the compiler selects it (MSVC 19.51: the stand-ins; GCC 16:
#   contract_assert and std::inplace_vector).
# - tests_unit_cxx26_standin: the same sources with PATELLA_CXX26_STANDIN, so on GCC 16 the
#   stand-ins run the tests the real features pass.
# - Both: TFixedString<N> (src/types/FixedString.h, issue #57), tests_unit with std::format_to_n and
#   tests_unit_cxx26_standin with PATELLA_FORMAT_STANDIN, the C++17 stand-in of src/compat/format.h that the
#   oracle-x86 build formats with; both compare the stand-in with std::format_to_n.
# - Both: FormatCRT71Time (src/compat/crt71.h, issue #61), ctime's text as MSVC 7.1 wrote it, through both paths of
#   TFixedString::Format: against texts computed apart, std::chrono's calendar and, on Windows, the UCRT's _ctime32.
#
# - tests_core: patella_core (cmake/Core.cmake, issue #7): vlong and TRSA, TXTEA, Utils and
#   TMyException's messages, Skins, the regexp, Container.h, Patella.ini's parser (issue #31) and
#   src/compat/msvc.h, the Microsoft CRT's semantics behind portable names (issue #43); the expected values that are not the code's own come from tests/unit/core_expected.py.
#
# - tests_testmode: patella_testmode (cmake/TestMode.cmake, issue #21): the virtual clock, the input
#   script's parser and key table, the scheduler, the driver's loop on the virtual clock, and that the
#   scripts in tests/testmode parse; the dumps of issue #22: the script's dump and frame commands, the
#   state dump's JSON, the packet log's lines and connections, the frame dump's PNG pictures; issue #23:
#   the reference scripts' commands (clock manual, advance, cursor, post, save), the server replay's
#   file, schedule and XTEA frames; issue #48: the fixed display modes' file and choice.
#
# - tests_render: src/vulkan/Quads.cpp, the Vulkan engine's Direct3D 9 quads, and src/vulkan/FilterMask.cpp,
#   its device-filtered pixels (issue #19); src/vulkan/DeviceChoice.cpp, the graphics device choice
#   (issue #31).
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
		cxx26_static_vector_test.cpp
		fixed_string_test.cpp
		crt71_test.cpp)
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
target_compile_definitions(tests_unit_cxx26_standin PRIVATE PATELLA_CXX26_STANDIN PATELLA_EXPECT_CXX26=0
	PATELLA_FORMAT_STANDIN)

add_executable(tests_core
	core_main.cpp
	core_platform_test.cpp
	core_vlong_test.cpp
	core_txtea_test.cpp
	core_utils_test.cpp
	core_skins_test.cpp
	core_regexp_test.cpp
	core_container_test.cpp
	core_settings_test.cpp
	core_msvc_test.cpp)
target_link_libraries(tests_core PRIVATE patella_core doctest::doctest)
patella_target_language(tests_core)
patella_target_warnings(tests_core)
patella_target_hardening(tests_core)
add_test(NAME tests_core COMMAND tests_core)

add_executable(tests_testmode
	testmode_main.cpp
	testmode_clock_test.cpp
	testmode_script_test.cpp
	testmode_dump_test.cpp
	testmode_replay_test.cpp
	testmode_display_modes_test.cpp)
target_link_libraries(tests_testmode PRIVATE patella_testmode doctest::doctest)
target_compile_definitions(tests_testmode PRIVATE "PATELLA_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")
patella_target_language(tests_testmode)
patella_target_warnings(tests_testmode)
patella_target_hardening(tests_testmode)
add_test(NAME tests_testmode COMMAND tests_testmode)

# tests_render (issue #19): the Vulkan engine's quads (src/vulkan/Quads.cpp, compiled here: the
# portable part of the engine, without Vulkan), so every preset checks how Direct3D 9's quads become
# Vulkan vertices. The engine itself is checked by tests/vulkan (Windows x64 with the client).
add_executable(tests_render
	render_quads_test.cpp
	render_filtermask_test.cpp
	render_device_choice_test.cpp
	"${PROJECT_SOURCE_DIR}/src/vulkan/Quads.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/FilterMask.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/DeviceChoice.cpp")
target_include_directories(tests_render PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tests_render PRIVATE doctest::doctest)
patella_target_language(tests_render)
patella_target_warnings(tests_render)
patella_target_hardening(tests_render)
add_test(NAME tests_render COMMAND tests_render)
