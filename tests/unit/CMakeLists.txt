# The unit tests (notes/modernization-plan.md, 4.6; issue #9): doctest, no proprietary file, a CTest
# test per program and one per contract violation scenario.
#
# - tests_unit: src/compat/cxx26.h as the compiler selects it (MSVC 19.51: the stand-ins; GCC 16:
#   contract_assert and std::inplace_vector).
# - tests_unit_cxx26_standin: the same sources with PATELLA_CXX26_STANDIN, so on GCC 16 the
#   stand-ins run the tests the real features pass.
# - Both: TFixedString<N> (src/types/FixedString.h, issue #57), tests_unit with std::format_to_n and
#   tests_unit_cxx26_standin with PATELLA_FORMAT_STANDIN, the C++17 stand-in of src/compat/format.h that the
#   32-bit oracle formatted with until issue #94; both compare the stand-in with std::format_to_n.
# - Both: FormatCRT71Time (src/compat/crt71.h, issue #61), ctime's text as MSVC 7.1 wrote it, through both paths of
#   TFixedString::Format: against texts computed apart, std::chrono's calendar and, on Windows, the UCRT's _ctime32.
# - Both: Time32, StoreTime32 and LoadTime32 (src/types/Time32.h, issue #62), the original's 4-byte time fields and
#   their conversion from and to the 64-bit time, held against the original's 32-bit arithmetic, past 2038 and 2106.
# - Both: TIndexedVector<T> (src/types/IndexedVector.h, issue #99), CipSoft's growable vector<T> over a std::vector: the
#   range and negative indices, growth by block and doubling, the init fill, the padding a growth carries (#91), the
#   storage's allocator (new T[n]'s) and the assertions.
# - Both: TSlotTable<T, N> (src/types/SlotTable.h, issue #102), Map.cpp's effect table over TStaticVector
#   (std::inplace_vector in tests_unit on GCC 16): the numbers New() gives, a full table (-1: the new effect is dropped), freed entries taken
#   again, Clear, and random operations against the original's array and search.
# - Both: the client's code page and Unicode (src/types/Cp1252.h, issue #112, plan D4): every byte's character, the
#   round trips through UTF-8 and UTF-16, '?' for the characters Windows-1252 does not have and for ill-formed UTF-8
#   and UTF-16, every code point; on Windows against Windows' own code page 1252.
# - Both: TFile (src/types/File.h, issue #64), the client's files: the bytes of each open mode (text: "\r\n" on every
#   platform), fopen's and fclose's results, reads, seeks, _eof through Descriptor, Print through both paths of the
#   format (std::format and the stand-in) against fprintf's text, and compat/print.h's FormatLine.
# - Both: patella::TSpan (src/compat/span.h, issue #59), std::span in tests_unit and the C++17 stand-in that the
#   32-bit oracle passed the packet buffers', TXTEA's and TRSA's spans with (until #94) in tests_unit_cxx26_standin
#   (PATELLA_SPAN_STANDIN): the same construction, conversion, size and part tests for both.
# - Both: the developer log (src/log/Log.h, issue #77): the levels, the lines on the console and in the file, the
#   formats through both paths (std::format and the stand-in), the fatal path's stack trace (std::stacktrace where the
#   library has it) and debugger break.
# - Both: PATELLA_ASSERT and PATELLA_ASSERT_VALUES (src/compat/assertion.h, issue #79) with TMyException.cpp: the
#   TMyException a false expression throws (this file, the line, the expression, the values), none for a true one, the
#   expression evaluated once and the values only when it is false; tests_unit with std::source_location,
#   tests_unit_cxx26_standin with the oracle's C++17 stand-in (PATELLA_SOURCE_LOCATION_STANDIN).
# - Both: TMyException (src/TMyException.h, issue #80), a message and a source location, and patella::ErrorText,
#   ErrorOrigin and MakeError, the texts of the original's two constructors that the sites' messages are made of.
# - Both: patella::TExpected and TUnexpected (src/compat/expected.h, issue #80) and TError: std::expected in
#   tests_unit, the oracle's C++17 stand-in in tests_unit_cxx26_standin (PATELLA_EXPECTED_STANDIN).
#
# - tests_core: patella_core (cmake/Core.cmake, issue #7): vlong and TRSA, TXTEA, Utils and
#   TMyException's messages, Skins, the regexp, Container.h, Patella.ini's parser (issue #31), the options' keys in it and their escaping (issue #45), the developer log's keys in it (issue #77) and
#   src/compat/msvc.h, the Microsoft CRT's semantics behind portable names (issue #43; its 32-bit time went in #62); the expected values that are not the code's own come from tests/unit/core_expected.py;
#   common.h's TERROR*, TMYEXCEPTION and TProgramError (issue #80).
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
		cp1252_test.cpp
		crt71_test.cpp
		time32_test.cpp
		indexed_vector_test.cpp
		slot_table_test.cpp
		file_test.cpp
		span_test.cpp
		log_test.cpp
		assertion_test.cpp
		exception_test.cpp
		expected_test.cpp
		"${PROJECT_SOURCE_DIR}/src/TMyException.cpp")
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
	PATELLA_FORMAT_STANDIN PATELLA_SPAN_STANDIN PATELLA_SOURCE_LOCATION_STANDIN PATELLA_EXPECTED_STANDIN)

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
	core_preferences_test.cpp
	core_log_settings_test.cpp
	core_msvc_test.cpp
	core_errors_test.cpp)
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

# tests_import_std (issue #113): import std; in new code, where the preset turns it on (PATELLA_IMPORT_STD,
# cmake/ImportStd.cmake: windows-x64, windows-x64-asan, windows-x64-gcc, linux-x64-gcc). One source that imports the
# standard library as a module and checks a few of its parts (formatting, ranges, containers, std::expected), with the
# product settings; no doctest, whose header would put #include and import std into one translation unit.
if(PATELLA_IMPORT_STD_AVAILABLE)
	add_executable(tests_import_std import_std_test.cpp)
	patella_target_language(tests_import_std)
	patella_target_warnings(tests_import_std)
	patella_target_hardening(tests_import_std)
	# Last: the std module gets the options above (cmake/ImportStd.cmake).
	patella_target_import_std(tests_import_std)
	add_test(NAME tests_import_std COMMAND tests_import_std)
endif()
