# The client of the Linux presets (Patella-Modern issue #131, plan M8; notes/decisions.md, "#131"): the client's sources as
# on Windows (cmake/WindowsX64.cmake), on the SDL3 window and the Vulkan engine, with the platform layer's Linux files
# (src/platform/*Linux.cpp) in place of the Win32 ones. No resource compiler: the cursors are built in from the local
# Tibia.exe's (or the placeholders, tools/embed_cursors.py; the window icon and the MFC-era resources are not used).
#
# The client's code is compiled with PATELLA_CORE (common.h: no Windows headers, as patella_core; the types its platform
# layer still names come from compat/win32_types.h, PATELLA_WIN32_TYPES), and with MSVC's rand (compat/msvc.h,
# PATELLA_MSVC_RAND), so the client's random numbers are the ones of a Windows run with the same seed.
#
# Tibia.exe (PATELLA_TIBIA_EXE, or the environment's, which tools/build.py sets when it mounts the local copy into the
# image) is only read for the cursors; without it the configure stops, unless PATELLA_PLACEHOLDER_RESOURCES=ON (the hosted
# CI), which refuses where a Tibia.exe is present, as on Windows.

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

# The default is the environment's (PATELLA_PLACEHOLDER_RESOURCES=1 in the hosted CI's containers, whose workflows cannot
# pass -D through tools/build.py).
if(DEFINED ENV{PATELLA_PLACEHOLDER_RESOURCES} AND NOT "$ENV{PATELLA_PLACEHOLDER_RESOURCES}" STREQUAL "")
	set(patella_placeholder_default ON)
else()
	set(patella_placeholder_default OFF)
endif()
option(PATELLA_PLACEHOLDER_RESOURCES "Build the client with neutral placeholder pictures instead of Tibia.exe's (the hosted CI's build; refused where a Tibia.exe is present, issue #127)" ${patella_placeholder_default})

if(DEFINED ENV{PATELLA_TIBIA_EXE} AND NOT "$ENV{PATELLA_TIBIA_EXE}" STREQUAL "")
	set(patella_tibia_exe_default "$ENV{PATELLA_TIBIA_EXE}")
else()
	set(patella_tibia_exe_default "${PROJECT_SOURCE_DIR}/Tibia.exe")
endif()
set(PATELLA_TIBIA_EXE "${patella_tibia_exe_default}" CACHE FILEPATH
	"The original Tibia.exe: its cursors are built into the Linux client")
include("${PROJECT_SOURCE_DIR}/cmake/Resources.cmake")
if(PATELLA_PLACEHOLDER_RESOURCES)
	foreach(tibia_exe "${PATELLA_TIBIA_EXE}" "${PROJECT_SOURCE_DIR}/Tibia.exe")
		if(EXISTS "${tibia_exe}")
			message(FATAL_ERROR "PATELLA_PLACEHOLDER_RESOURCES=ON refused: ${tibia_exe} exists. The placeholders are "
				"for machines without Tibia.exe (the hosted CI), and must never replace the real pictures in a local build.")
		endif()
	endforeach()
	set(PATELLA_RES_DIR "${PROJECT_BINARY_DIR}/res-placeholder")
	patella_placeholder_resources("${PATELLA_RES_DIR}")
else()
	if(NOT EXISTS "${PATELLA_TIBIA_EXE}")
		message(FATAL_ERROR "${PATELLA_TIBIA_EXE} is missing: the cursors are taken from it (set PATELLA_TIBIA_EXE, "
			"PATELLA_CLIENT=OFF for only the unit tests, or PATELLA_PLACEHOLDER_RESOURCES=ON on a machine without Tibia.exe)")
	endif()
	set(PATELLA_RES_DIR "${PROJECT_BINARY_DIR}/res")
	patella_extract_resources("${PATELLA_TIBIA_EXE}" "${PATELLA_RES_DIR}")
endif()
set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${PROJECT_SOURCE_DIR}/tools/embed_cursors.py")
execute_process(
	COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/embed_cursors.py" "${PATELLA_RES_DIR}"
		"${PROJECT_BINARY_DIR}/EmbeddedCursors.cpp"
	RESULT_VARIABLE embed_result ERROR_VARIABLE embed_error)
if(NOT embed_result EQUAL 0)
	message(FATAL_ERROR "tools/embed_cursors.py failed (${embed_result}):\n${embed_error}")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Vulkan.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Sdl3.cmake")

file(GLOB patella_cpp_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
list(SORT patella_cpp_sources CASE INSENSITIVE)
file(GLOB patella_c_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/Extern/regexp/*.c")
list(SORT patella_c_sources CASE INSENSITIVE)

set(patella_linux_client_sources
	"${PROJECT_SOURCE_DIR}/src/vulkan/SurfaceVulkan.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/WindowSystemSdl.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/DeviceSettings.cpp"
	"${PROJECT_SOURCE_DIR}/src/settings/Settings.cpp"
	"${PROJECT_SOURCE_DIR}/src/settings/PreferenceKeys.cpp"
	"${PROJECT_SOURCE_DIR}/src/log/LogSettings.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/LinuxPlatform.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/SdlPlatform.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/SdlClipboard.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/PortableSocket.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/NetworkLinux.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/DataLocationLinux.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/EntryLinux.cpp"
	"${PROJECT_BINARY_DIR}/EmbeddedCursors.cpp")

# The compile settings of the client's targets and of the header checks (cmake/HeaderCheck.cmake).
function(patella_client_compile_settings target)
	target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	target_compile_definitions(${target} PRIVATE
		$<$<CONFIG:Debug>:_DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
		PATELLA_CORE PATELLA_WIN32_TYPES PATELLA_MSVC_RAND PATELLA_VULKAN PATELLA_SDL3)
	patella_target_language(${target})
	patella_target_warnings(${target})
	patella_target_hardening(${target})
	target_link_libraries(${target} PRIVATE patella_render_vulkan SDL3::Headers)
	if(PATELLA_PLACEHOLDER_RESOURCES)
		target_compile_definitions(${target} PRIVATE PATELLA_PLACEHOLDER_RESOURCES)
	endif()
endfunction()

function(patella_client target)
	add_executable(${target} ${patella_cpp_sources} ${patella_c_sources} ${patella_linux_client_sources} ${ARGN})
	patella_client_compile_settings(${target})
	# The client's shared sources are not in compile_commands.json through these two targets (tools/tidy.py --all checks every
	# src/ file of the database: they compile the client's code that the Windows presets' runs check, with the Windows
	# headers); the sources only it compiles are, through patella_tidy_linux_client below (issue #160).
	set_property(TARGET ${target} PROPERTY EXPORT_COMPILE_COMMANDS OFF)
	target_link_libraries(${target} PRIVATE
		$<IF:$<TARGET_EXISTS:SDL3::SDL3-static>,SDL3::SDL3-static,SDL3::SDL3> Threads::Threads ${CMAKE_DL_LIBS})
endfunction()

# The sources only the Linux client compiles (src/platform/*Linux*.cpp) in compile_commands.json for tools/tidy.py (issue
# #160; notes/decisions.md, "#160"), with the client's settings: the database's only commands for them. Never built (not
# in `all`, nothing depends on it), so the client, its objects and the reproducible builds are as before; hosted.yml's
# clang-tidy job fails when one of these files is not in the database (tools/tidy.py --require).
set(patella_tidy_linux_sources ${patella_linux_client_sources})
list(FILTER patella_tidy_linux_sources INCLUDE REGEX "/src/platform/[^/]*Linux[^/]*\\.cpp$")
add_library(patella_tidy_linux_client OBJECT EXCLUDE_FROM_ALL ${patella_tidy_linux_sources})
patella_client_compile_settings(patella_tidy_linux_client)

get_target_property(patella_testmode_sources patella_testmode SOURCES)
set(patella_test_build_sources
	"${PROJECT_SOURCE_DIR}/src/testmode/TestModeWin32.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/ClientStateWin32.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/PipeSocketWin32.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/SdlInputWin32.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/ProfileWin32.cpp" # issue #134's; on Linux its stubs
	${patella_testmode_sources})

patella_client(Patella)

option(PATELLA_TEST_BUILD "Build PatellaTest, the client with the test mode (issue #21)" ON)
if(PATELLA_TEST_BUILD)
	patella_client(PatellaTest ${patella_test_build_sources})
	target_compile_definitions(PatellaTest PRIVATE PATELLA_TEST_MODE)
	get_filename_component(patella_data_dir "${PATELLA_TIBIA_EXE}" DIRECTORY)
	# The start without its data files (issue #127's test; no CipSoft file, so the hosted CI runs it too).
	add_test(NAME start_without_data COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:PatellaTest>"
		"-DTMPROOT=${PROJECT_BINARY_DIR}/tmp" -P "${PROJECT_SOURCE_DIR}/cmake/RunStartWithoutData.cmake")
	# Issue #145's headless run (skipped without the data files next to Tibia.exe).
	add_test(NAME headless_client COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:PatellaTest>"
		"-DDATA=${patella_data_dir}" "-DTMPROOT=${PROJECT_BINARY_DIR}/tmp"
		-P "${PROJECT_SOURCE_DIR}/cmake/RunHeadless.cmake")
	set_tests_properties(headless_client PROPERTIES SKIP_RETURN_CODE 77)
	# With AddressSanitizer (linux-x64-clang): the original's mismatched releases and leaks are not reported, as for the
	# golden runner (tests/golden/CMakeLists.txt) and as MSVC's AddressSanitizer reports neither on Windows.
	if(PATELLA_SANITIZE MATCHES "address")
		set_tests_properties(start_without_data headless_client PROPERTIES ENVIRONMENT
			"ASAN_OPTIONS=detect_leaks=0:alloc_dealloc_mismatch=0")
	endif()
endif()
