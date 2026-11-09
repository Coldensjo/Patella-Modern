# BuildConfig.cmake (Patella-Modern issue #169, the owner's decision after #166): the values someone fixes when building
# the client for their own server, src/BuildConfig.h and src/BuildConfigDefaults.h.
#
# PATELLA_BUILD_CONFIG names a developer's own copy of src/BuildConfigDefaults.h (unset by default): the whole build compiles
# with it through the definition PATELLA_BUILD_CONFIG_FILE, so pulling updates never conflicts with its values. src/BuildConfig.h
# applies the same compile-time checks to it. README.md, "Building for your own server".
#
# Below, on every configure of every preset, three compiles of tests/unit/build_config/check.cpp show that the checks hold
# (no CTest test: a compile that is meant to fail needs the configure's compiler environment, and it is cheap here):
#	the defaults compile; a developer's own file compiles;
#	a file with a bad value of every kind does not, and its compiler output names each one.

include_guard(GLOBAL)

set(PATELLA_BUILD_CONFIG "" CACHE FILEPATH "A developer's own copy of src/BuildConfigDefaults.h (a client for their own server; empty: the defaults)")

if(PATELLA_BUILD_CONFIG)
	get_filename_component(patella_build_config_file "${PATELLA_BUILD_CONFIG}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
	if(NOT EXISTS "${patella_build_config_file}" OR IS_DIRECTORY "${patella_build_config_file}")
		message(FATAL_ERROR "PATELLA_BUILD_CONFIG: no such file: ${patella_build_config_file}")
	endif()
	message(STATUS "PATELLA_BUILD_CONFIG: ${patella_build_config_file} replaces src/BuildConfigDefaults.h")
	add_compile_definitions("PATELLA_BUILD_CONFIG_FILE=\"${patella_build_config_file}\"")
endif()

# Compiles check.cpp with Header as the build's values ("": the defaults): Result is true or false, Log the compiler's
# output.
function(patella_build_config_compile Header Result Log)
	set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
	set(definition "")
	if(NOT Header STREQUAL "")
		set(definition "-DPATELLA_BUILD_CONFIG_FILE=\"${Header}\"")
	endif()
	try_compile(compiled
		SOURCES "${PROJECT_SOURCE_DIR}/tests/unit/build_config/check.cpp"
		CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PROJECT_SOURCE_DIR}/src"
		COMPILE_DEFINITIONS ${definition}
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
		NO_CACHE
		OUTPUT_VARIABLE log)
	set(${Result} "${compiled}" PARENT_SCOPE)
	set(${Log} "${log}" PARENT_SCOPE)
endfunction()

set(patella_build_config_tests "${PROJECT_SOURCE_DIR}/tests/unit/build_config")

patella_build_config_compile("" patella_build_config_compiled patella_build_config_log)
if(NOT patella_build_config_compiled)
	message(FATAL_ERROR "src/BuildConfig.h: the defaults do not compile (issue #169):\n${patella_build_config_log}")
endif()

patella_build_config_compile("${patella_build_config_tests}/own_values.h" patella_build_config_compiled patella_build_config_log)
if(NOT patella_build_config_compiled)
	message(FATAL_ERROR "src/BuildConfig.h: a developer's own file does not compile (issue #169):\n${patella_build_config_log}")
endif()

patella_build_config_compile("${patella_build_config_tests}/bad_values.h" patella_build_config_compiled patella_build_config_log)
if(patella_build_config_compiled)
	message(FATAL_ERROR "src/BuildConfig.h: a file of bad values compiled: the checks do not hold (issue #169)")
endif()
foreach(patella_build_config_message IN ITEMS
		"LoginServers must have exactly four"
		"LoginServers must be non-empty"
		"LoginPort must be a port"
		"RsaModulus must be 306 to 309"
		"TestServerHost must be a non-empty"
		"TestServerPort must be a port"
		"LinkWebsite must start with"
		"LinkManual must start with"
		"LinkFAQ must start with"
		"LinkOptionsHelp must start with"
		"LinkGraphicsHelp must start with"
		"LinkConsoleHelp must start with"
		"LinkNetworkHelp must start with"
		"LinkAccount must start with"
		"LinkSignup must start with"
		"WebsiteCaption must be 1 to 49"
		"SiteName must be printable"
		"BugReportAddress must be printable")
	string(FIND "${patella_build_config_log}" "${patella_build_config_message}" patella_build_config_at)
	if(patella_build_config_at EQUAL -1)
		message(FATAL_ERROR "src/BuildConfig.h: a bad value did not fail with its message \"${patella_build_config_message}\" (issue #169):\n${patella_build_config_log}")
	endif()
endforeach()
message(STATUS "src/BuildConfig.h: the checks hold (the defaults and a developer's file compile, 18 kinds of bad value do not)")
