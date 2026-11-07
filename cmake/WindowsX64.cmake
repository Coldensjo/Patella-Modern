# The client of the preset windows-x64 (issue #5; notes/modernization-plan.md, section 7, M1): today's
# code as a 64-bit Patella.exe with MSVC x64 and static x64 MFC. Its only graphics engine is the Vulkan
# engine (issue #19; cmake/Vulkan.cmake, notes/vulkan.md) as engine 2: since issue #20 (plan M2) the
# original engines (DirectDraw, Direct3D 9, OpenGL) are not compiled, and ddraw, dxguid, opengl32 and glu32
# are not linked; their sources left the fork with the 32-bit oracle (Patella-Modern issue #94).
#
# - The sources, definitions, import libraries and linker options of the oracle's Patella.exe (the former
#   Patella.vcxproj's, until issue #94), so the program does what that one does: static MFC (nafxcw.lib, pulled
#   in by the MFC headers), the static CRT, oleacc.dll delay-loaded, no manifest (a manifest would
#   change the common controls and the DPI handling the user sees).
# - The product settings instead of the original's flags: cmake/Warnings.cmake (C++23, /W4 with the
#   baseline list, /WX) and cmake/Hardening.cmake (/guard:cf, /CETCOMPAT); CMake's default
#   optimisation flags; a PDB in Release and Debug.
# - The resource pictures are extracted from the local Tibia.exe at configure time, as for the oracle.
#
# Configure with MSVC for x64 set up (vcvarsall.bat x64; `python -I tools/build.py windows-x64` does
# it). Without Tibia.exe (PATELLA_TIBIA_EXE) the configure stops; PATELLA_CLIENT=OFF builds only the
# unit tests, for machines without it.
#
# PATELLA_CLIENT_COMPILE_ONLY=ON (issues #25, #46; hosted CI, which has no Tibia.exe): no Patella.exe and
# no PatellaTest.exe, only two object libraries, compiled and never linked: Patella_compile, Patella.exe's
# C and C++ sources with its definitions, warnings and hardening, and PatellaTest_compile, the test
# build's with the test build's. Without the resource script (its pictures come from Tibia.exe) nothing
# reads a CipSoft file.

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "windows-x64 needs MSVC's x64 compiler: run python -I tools/build.py windows-x64, or "
		"configure from a developer prompt set up with vcvarsall.bat x64 (found ${CMAKE_SIZEOF_VOID_P}-byte "
		"pointers)")
endif()

enable_language(RC)

# The Python on PATH before any registered elsewhere, as for the oracle.
set(Python3_FIND_REGISTRY LAST)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(PATELLA_CLIENT_COMPILE_ONLY "Compile the client's and the test build's sources without Tibia.exe (object libraries Patella_compile and PatellaTest_compile; no resources, no link, no executables; hosted CI, issues #25, #46)" OFF)

set(PATELLA_RES_DIR "${PROJECT_BINARY_DIR}/res")
if(NOT PATELLA_CLIENT_COMPILE_ONLY)
	set(PATELLA_TIBIA_EXE "${PROJECT_SOURCE_DIR}/Tibia.exe" CACHE FILEPATH
		"The original Tibia.exe: the resource pictures are extracted from it")
	if(NOT EXISTS "${PATELLA_TIBIA_EXE}")
		message(FATAL_ERROR "${PATELLA_TIBIA_EXE} is missing: the resource pictures are extracted from it "
			"(notes/resources.md; set PATELLA_TIBIA_EXE to use another copy, PATELLA_CLIENT=OFF to build only "
			"the unit tests, or PATELLA_CLIENT_COMPILE_ONLY=ON to compile the test build's sources only)")
	endif()
	include("${PROJECT_SOURCE_DIR}/cmake/Resources.cmake")
	patella_extract_resources("${PATELLA_TIBIA_EXE}" "${PATELLA_RES_DIR}")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

# rc.exe as for the oracle.
set(CMAKE_RC_FLAGS "/l0x0409 /nologo")

file(GLOB patella_cpp_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
list(SORT patella_cpp_sources CASE INSENSITIVE)
file(GLOB patella_c_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/Extern/regexp/*.c")
list(SORT patella_c_sources CASE INSENSITIVE)
set(patella_rc "${PROJECT_SOURCE_DIR}/src/res/Patella.rc")
set_source_files_properties("${patella_rc}" PROPERTIES OBJECT_DEPENDS "${PATELLA_RES_DIR}/resources.tsv")

# The Vulkan engine (issue #19): cmake/Vulkan.cmake builds the back end and the CPU driver; the client
# adds TSurfaceVulkan (src/vulkan, which the oracle's glob of src/*.cpp does not reach) and the
# definition PATELLA_VULKAN, with which Surface.cpp and Control.cpp take the client's own engine
# selection (issue #20). The graphics device choice (issue #31): src/vulkan/DeviceSettings.cpp, the
# advanced options' buttons and Patella.ini (src/settings/Settings.cpp, patella_core's parser compiled
# here again for the static CRT); the environment variable PATELLA_VULKAN=auto|gpu|gpuN|cpu overrides it. The
# developer log's keys in Patella.ini (issue #77): src/log/LogSettings.cpp, patella_core's, compiled here again too, and
# so are the options' keys in it (issue #45: src/settings/PreferenceKeys.cpp, Preferences.cpp's Patella.ini).
include("${PROJECT_SOURCE_DIR}/cmake/Vulkan.cmake")
# SDL3 (issue #122, plan M8; cmake/Sdl3.cmake): the window and the event loop of src/SdlFrame.cpp, the default window
# path (PATELLA_WINDOW=mfc: MFC's until issue #127). PATELLA_SDL3 gives Patella.exe's Platform() the SDL3 window's input
# (src/platform/Win32Platform.cpp); the golden runner compiles that file without it.
include("${PROJECT_SOURCE_DIR}/cmake/Sdl3.cmake")
# The platform's Win32 implementations (issue #109, src/platform: the clock, the files, the network; Patella.exe's
# Platform(), PatellaTest.exe's being the test mode's) are compiled with them.
set(patella_vulkan_client_sources "${PROJECT_SOURCE_DIR}/src/vulkan/SurfaceVulkan.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/DeviceSettings.cpp" "${PROJECT_SOURCE_DIR}/src/settings/Settings.cpp"
	"${PROJECT_SOURCE_DIR}/src/settings/PreferenceKeys.cpp" "${PROJECT_SOURCE_DIR}/src/log/LogSettings.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/Win32Platform.cpp")

# The client: Patella.exe, the product; PatellaTest.exe, the test build (issue #21; plan 4.4), the
# same sources with PATELLA_TEST_MODE, which turns the hooks of src/TestHooks.h into the test mode:
# src/testmode/TestModeWin32.cpp and patella_testmode's sources (cmake/TestMode.cmake), compiled here
# again because the client links the static CRT and the library the DLL one. Patella.exe compiles
# none of them. The client's sources are compiled once per target: the definition changes every one.
# patella_client_compile_settings: what the compiler sees, shared with Patella_compile and
# PatellaTest_compile.
function(patella_client_compile_settings target)
	set_target_properties(${target} PROPERTIES
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
	target_include_directories(${target} PRIVATE
		"$<$<COMPILE_LANGUAGE:C,CXX>:${PROJECT_SOURCE_DIR}/src>"
		"$<$<COMPILE_LANGUAGE:RC>:${PATELLA_RES_DIR}>")
	target_compile_definitions(${target} PRIVATE
		$<$<CONFIG:Debug>:_DEBUG>
		$<$<CONFIG:Release>:NDEBUG>
		$<$<COMPILE_LANGUAGE:C,CXX>:_WIN32_WINNT=0x0601>)
	patella_target_language(${target})
	patella_target_warnings(${target})
	patella_target_hardening(${target})
	target_compile_definitions(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:PATELLA_VULKAN>)
	target_link_libraries(${target} PRIVATE patella_render_vulkan)
	# Issue #122: SDL3's headers (the library itself is linked by patella_client: the compile-only targets and the
	# header checks do not build it).
	target_compile_definitions(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:PATELLA_SDL3>)
	target_link_libraries(${target} PRIVATE SDL3::Headers)
endfunction()

function(patella_client target)
	add_executable(${target} WIN32 ${patella_cpp_sources} ${patella_c_sources} "${patella_rc}"
		${patella_vulkan_client_sources} ${ARGN})
	patella_client_compile_settings(${target})
	target_link_libraries(${target} PRIVATE
		winmm kernel32 user32 gdi32 winspool advapi32 shell32 comctl32 shlwapi
		oleaut32 ws2_32 delayimp SDL3::SDL3-static)
	target_link_options(${target} PRIVATE
		/NODEFAULTLIB:wsock32.lib /DELAYLOAD:oleacc.dll /MANIFEST:NO /DEBUG
		$<$<CONFIG:Release>:/INCREMENTAL:NO /OPT:REF /OPT:ICF>)
	patella_vulkan_cpu_driver(${target})
endfunction()

get_target_property(patella_testmode_sources patella_testmode SOURCES)
set(patella_test_build_sources "${PROJECT_SOURCE_DIR}/src/testmode/TestModeWin32.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/ClientStateWin32.cpp" "${PROJECT_SOURCE_DIR}/src/testmode/PipeSocketWin32.cpp"
	${patella_testmode_sources})

if(PATELLA_CLIENT_COMPILE_ONLY)
	# Each executable's sources without the resource script, compiled as Patella.exe and PatellaTest.exe
	# compile them.
	add_library(Patella_compile OBJECT ${patella_cpp_sources} ${patella_c_sources}
		${patella_vulkan_client_sources})
	patella_client_compile_settings(Patella_compile)
	add_library(PatellaTest_compile OBJECT ${patella_cpp_sources} ${patella_c_sources}
		${patella_vulkan_client_sources} ${patella_test_build_sources})
	patella_client_compile_settings(PatellaTest_compile)
	target_compile_definitions(PatellaTest_compile PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:PATELLA_TEST_MODE>)
	return()
endif()

patella_client(Patella)

option(PATELLA_TEST_BUILD "Build PatellaTest.exe, the client with the test mode (issue #21)" ON)
if(PATELLA_TEST_BUILD)
	patella_client(PatellaTest ${patella_test_build_sources})
	target_compile_definitions(PatellaTest PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:PATELLA_TEST_MODE>)
	# Issues #67 (part 2) and #83: a fatal error before the main window is set still shows FatalError's box and exits
	# with code 1 (cmake/RunFatalErrorBox.cmake; skipped without the data files next to Tibia.exe).
	get_filename_component(patella_data_dir "${PATELLA_TIBIA_EXE}" DIRECTORY)
	add_test(NAME fatal_error_box COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:PatellaTest>"
		"-DDATA=${patella_data_dir}" "-DTMPROOT=${PROJECT_BINARY_DIR}/tmp"
		-P "${PROJECT_SOURCE_DIR}/cmake/RunFatalErrorBox.cmake")
	set_tests_properties(fatal_error_box PROPERTIES SKIP_RETURN_CODE 77)
	# Issue #90: a PROGRAM_ERROR in InitControl (a missing Tibia.dat, or the test hook at any of its steps) ends
	# with the same box and exit code 1 (cmake/RunInitControlFailure.cmake; skipped without the data files).
	add_test(NAME init_control_failure COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:PatellaTest>"
		"-DDATA=${patella_data_dir}" "-DTMPROOT=${PROJECT_BINARY_DIR}/tmp"
		-P "${PROJECT_SOURCE_DIR}/cmake/RunInitControlFailure.cmake")
	set_tests_properties(init_control_failure PROPERTIES SKIP_RETURN_CODE 77)
endif()
