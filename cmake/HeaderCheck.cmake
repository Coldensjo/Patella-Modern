# Each header of src/ compiles on its own (issue #49; notes/modernization-plan.md, sections 4.1 and 7,
# M3): for every header a generated one-line source, `#include "<header>"`, compiled with the settings
# of a target that uses the header, in an object library that is never linked. A header that needs
# something it does not include fails the build of every preset that compiles it. The object libraries
# are in the default build, so `cmake --workflow` (tools/build.py) and the hosted CI build them; their
# compile commands (compile_commands.json) are what tools/tidy.py --headers runs clang-tidy with, on the
# header itself as the main file (misc-include-cleaner checks only the main file's includes).
#
# The lists below name every header of src/ (not src/Extern) by the targets whose code includes it; the
# configure fails on a header that is in none of them (a new header is added to a list, or to
# PATELLA_HEADER_CHECK_EXCEPTIONS with the reason). Where a header's content depends on the target's
# definitions (common.h: PATELLA_CORE, TIBIA_MFC_SHIM, PATELLA_VULKAN, PATELLA_TEST_MODE), it is checked
# once per such target:
#
#	header_check_core            patella_core's settings (PATELLA_CORE)               every product preset
#	header_check_cxx26_standin   tests_unit_cxx26_standin's (PATELLA_CXX26_STANDIN,    every product preset
#	                             PATELLA_FORMAT_STANDIN)
#	header_check_testmode        patella_testmode's                                    every product preset
#	header_check_render          tests_render's (the Vulkan engine's portable part)    every product preset
#	header_check_client          Patella.exe's (PATELLA_VULKAN)                        windows-x64 (client)
#	header_check_test_build      PatellaTest.exe's (PATELLA_VULKAN, PATELLA_TEST_MODE) windows-x64 (client)
#	header_check_oracle          the oracle Patella.exe's (real MFC, C++17, x86)       oracle-x86
#	header_check_oracle_shim     mfc_shim_check's and the difftest's (TIBIA_MFC_SHIM)  oracle-x86
#
# The x64 client's two exist with PATELLA_CLIENT_COMPILE_ONLY too (the hosted CI builds them by name).
# Included by CMakeLists.txt after the targets they copy: patella_header_checks() makes the ones whose
# target exists in the preset.

include_guard(GLOBAL)

# The portable code (patella_core, cmake/Core.cmake) and what it includes.
set(PATELLA_HEADERS_CORE
	common.h
	compat/crt71.h
	compat/format.h
	compat/msvc.h
	compat/msvc_io.h
	compat/msvc_portable.h
	compat/print.h
	compat/span.h
	Container.h
	log/Log.h
	log/LogSettings.h
	settings/Settings.h
	Skins.h
	TMyException.h
	TRSA.h
	TXTEA.h
	types/File.h
	types/FixedString.h
	types/Time32.h
	Utils.h
	vlong.h)
# The C++26 features behind feature-test macros (issue #9): tests/unit, real or stand-ins.
set(PATELLA_HEADERS_CXX26
	compat/cxx26.h)
# The portable part of the test mode (patella_testmode, cmake/TestMode.cmake).
set(PATELLA_HEADERS_TESTMODE
	testmode/ClientView.h
	testmode/Clock.h
	testmode/DisplayModes.h
	testmode/InputScript.h
	testmode/PacketLog.h
	testmode/Png.h
	testmode/ServerReplay.h
	testmode/StateDump.h)
# The Vulkan engine's portable part (tests_render on every preset; patella_render_vulkan on x64).
set(PATELLA_HEADERS_RENDER
	vulkan/DeviceChoice.h
	vulkan/FilterMask.h
	vulkan/Quads.h)
# The client's own headers (Windows, MFC or its shim): the oracle's and the x64 client's.
set(PATELLA_HEADERS_CLIENT
	Communication.h
	ConsoleWindow.h
	ContentWindows.h
	ContextMenus.h
	Control.h
	Creatures.h
	DialogElements.h
	DialogWindow.h
	Dialogs.h
	GUI.h
	Help.h
	Inventory.h
	MainFrm.h
	MainWindow.h
	Map.h
	MapWindow.h
	Network.h
	Objects.h
	Player.h
	Preferences.h
	Script.h
	Status.h
	Surface.h
	Talk.h
	TestHooks.h
	Text.h
	graphics.h
	sysinc.h
	res/resource.h)
# The original engines, compiled only in the oracle (DirectDraw, Direct3D 9, OpenGL; issue #20).
set(PATELLA_HEADERS_ORACLE_ENGINES
	SurfaceDX.h
	SurfaceGL.h
	surfaceDX9.h)
# MFC's stand-in (TIBIA_MFC_SHIM: mfc_shim_check and the difftest).
set(PATELLA_HEADERS_MFC_SHIM
	compat/mfc_shim.h)
# The x64 client's Vulkan engine (patella_render_vulkan and TSurfaceVulkan, cmake/Vulkan.cmake).
set(PATELLA_HEADERS_VULKAN_CLIENT
	vulkan/DeviceSettings.h
	vulkan/PresentWindow.h
	vulkan/Renderer.h
	vulkan/SurfaceVulkan.h)
# The Windows side of the test mode (PatellaTest.exe).
set(PATELLA_HEADERS_TEST_BUILD
	testmode/ClientStateWin32.h)
# Headers that are not meant to stand alone, each with the reason. None: every header of src/ is
# checked. (The shaders' SPIR-V headers, ColorKeyFrag.h and the others, are generated into the build
# tree by glslang, cmake/Vulkan.cmake: not headers of src/.)
set(PATELLA_HEADER_CHECK_EXCEPTIONS)

# Every header of src/ outside src/Extern is on a list.
file(GLOB_RECURSE patella_all_headers RELATIVE "${PROJECT_SOURCE_DIR}/src" CONFIGURE_DEPENDS
	"${PROJECT_SOURCE_DIR}/src/*.h" "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/src/*.inl"
	"${PROJECT_SOURCE_DIR}/src/*.inc")
list(FILTER patella_all_headers EXCLUDE REGEX "^Extern/")
set(patella_listed_headers ${PATELLA_HEADERS_CORE} ${PATELLA_HEADERS_CXX26} ${PATELLA_HEADERS_TESTMODE}
	${PATELLA_HEADERS_RENDER} ${PATELLA_HEADERS_CLIENT} ${PATELLA_HEADERS_ORACLE_ENGINES}
	${PATELLA_HEADERS_MFC_SHIM} ${PATELLA_HEADERS_VULKAN_CLIENT} ${PATELLA_HEADERS_TEST_BUILD}
	${PATELLA_HEADER_CHECK_EXCEPTIONS})
foreach(header IN LISTS patella_all_headers)
	if(NOT header IN_LIST patella_listed_headers)
		message(FATAL_ERROR "src/${header} is on no list of cmake/HeaderCheck.cmake: add it to the list of "
			"the targets that include it (or to PATELLA_HEADER_CHECK_EXCEPTIONS with the reason)")
	endif()
endforeach()
foreach(header IN LISTS patella_listed_headers)
	if(NOT header IN_LIST patella_all_headers)
		message(FATAL_ERROR "cmake/HeaderCheck.cmake names src/${header}, which does not exist")
	endif()
endforeach()

# patella_header_check(<target> <header> ...): the object library <target>, one generated source per
# header (relative to src/), build/<preset>/header-check/<target>/<header>.cpp. The caller gives it the
# settings of the target it stands for. The first line of each source names the header for
# tools/tidy.py --headers, which reads the second.
function(patella_header_check target)
	set(sources "")
	foreach(header IN LISTS ARGN)
		set(source "${PROJECT_BINARY_DIR}/header-check/${target}/${header}.cpp")
		file(CONFIGURE OUTPUT "${source}" CONTENT
			"// Generated by cmake/HeaderCheck.cmake (issue #49): src/@header@ compiles on its own.\n#include \"@header@\"\n"
			@ONLY)
		# patella_target_warnings (cmake/Warnings.cmake) gives it the level of the header's baseline entry.
		set_source_files_properties("${source}" PROPERTIES PATELLA_HEADER "${PROJECT_SOURCE_DIR}/src/${header}")
		list(APPEND sources "${source}")
	endforeach()
	add_library(${target} OBJECT ${sources})
	target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
endfunction()

# The settings of an oracle target, copied: OracleX86.cmake sets them on the target itself (the former
# Patella.vcxproj's command lines), and /Zc:strictStrings- per source of Patella.exe.
function(patella_header_check_copy target from)
	foreach(property IN ITEMS COMPILE_OPTIONS COMPILE_DEFINITIONS INCLUDE_DIRECTORIES MSVC_RUNTIME_LIBRARY
			MSVC_DEBUG_INFORMATION_FORMAT)
		get_target_property(value ${from} ${property})
		if(value)
			set_property(TARGET ${target} PROPERTY ${property} "${value}")
		endif()
	endforeach()
endfunction()

function(patella_header_checks)
	if(TARGET patella_core)
		patella_header_check(header_check_core ${PATELLA_HEADERS_CORE} ${PATELLA_HEADERS_CXX26})
		target_link_libraries(header_check_core PRIVATE patella_core)
		patella_target_language(header_check_core)
		patella_target_warnings(header_check_core)
		patella_target_hardening(header_check_core)
		# With the stand-ins of tests_unit_cxx26_standin: compat/format.h's (issue #57) and compat/span.h's (issue
		# #59) too, and the headers that format through it (issue #64; the developer log, issue #77).
		patella_header_check(header_check_cxx26_standin ${PATELLA_HEADERS_CXX26} compat/format.h
			compat/print.h compat/span.h log/Log.h types/File.h types/FixedString.h)
		target_compile_definitions(header_check_cxx26_standin PRIVATE PATELLA_CXX26_STANDIN PATELLA_FORMAT_STANDIN
			PATELLA_SPAN_STANDIN)
		patella_target_language(header_check_cxx26_standin)
		patella_target_warnings(header_check_cxx26_standin)
		patella_target_hardening(header_check_cxx26_standin)
	endif()
	if(TARGET patella_testmode)
		patella_header_check(header_check_testmode ${PATELLA_HEADERS_TESTMODE})
		target_link_libraries(header_check_testmode PRIVATE patella_testmode)
		patella_target_language(header_check_testmode)
		patella_target_warnings(header_check_testmode)
		patella_target_hardening(header_check_testmode)
	endif()
	if(TARGET tests_render)
		patella_header_check(header_check_render ${PATELLA_HEADERS_RENDER})
		patella_target_language(header_check_render)
		patella_target_warnings(header_check_render)
		patella_target_hardening(header_check_render)
	endif()
	# The x64 client: every header its sources include, with its settings (cmake/WindowsX64.cmake).
	if(TARGET Patella_compile OR (TARGET Patella AND NOT PATELLA_ORACLE))
		set(headers ${PATELLA_HEADERS_CORE} ${PATELLA_HEADERS_RENDER} ${PATELLA_HEADERS_CLIENT}
			${PATELLA_HEADERS_VULKAN_CLIENT})
		patella_header_check(header_check_client ${headers})
		patella_client_compile_settings(header_check_client)
		patella_header_check(header_check_test_build ${headers} ${PATELLA_HEADERS_TESTMODE}
			${PATELLA_HEADERS_TEST_BUILD})
		patella_client_compile_settings(header_check_test_build)
		target_compile_definitions(header_check_test_build PRIVATE PATELLA_TEST_MODE)
	endif()
	# The oracle: Patella.exe's sources (src/*.cpp) and what they include, with real MFC and with the shim.
	if(PATELLA_ORACLE)
		set(headers common.h compat/format.h compat/msvc.h compat/msvc_io.h compat/print.h compat/span.h Container.h
			log/Log.h Skins.h TMyException.h TRSA.h TXTEA.h types/File.h types/FixedString.h Utils.h vlong.h ${PATELLA_HEADERS_CLIENT}
			${PATELLA_HEADERS_ORACLE_ENGINES})
		patella_header_check(header_check_oracle ${headers})
		patella_header_check_copy(header_check_oracle Patella)
		target_compile_options(header_check_oracle PRIVATE /Zc:strictStrings-)
		patella_header_check(header_check_oracle_shim ${headers} ${PATELLA_HEADERS_MFC_SHIM})
		patella_header_check_copy(header_check_oracle_shim mfc_shim_check)
	endif()
endfunction()
