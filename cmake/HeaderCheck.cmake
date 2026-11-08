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
#
# Until Patella-Modern issue #94 the 32-bit oracle preset had two more, header_check_oracle (real MFC, C++17, x86) and
# header_check_oracle_shim (TIBIA_MFC_SHIM); they left with the oracle.
#
# The x64 client's two exist with PATELLA_CLIENT_COMPILE_ONLY too (the hosted CI builds them by name).
# Included by CMakeLists.txt after the targets they copy: patella_header_checks() makes the ones whose
# target exists in the preset.

include_guard(GLOBAL)

# The portable code (patella_core, cmake/Core.cmake) and what it includes.
set(PATELLA_HEADERS_CORE
	common.h
	compat/assertion.h
	compat/crt71.h
	compat/expected.h
	compat/format.h
	compat/msvc.h
	compat/msvc_io.h
	compat/msvc_portable.h
	compat/print.h
	compat/mdspan.h
	compat/span.h
	AutomapLayer.h
	Container.h
	log/Log.h
	log/LogSettings.h
	MapPoint.h
	platform/Clock.h
	platform/FileSystem.h
	platform/Platform.h
	platform/PortableSocket.h
	platform/Socket.h
	settings/PreferenceKeys.h
	settings/Settings.h
	Skins.h
	TMyException.h
	TRSA.h
	TXTEA.h
	types/Cp1252.h
	types/File.h
	types/FixedString.h
	types/IndexedVector.h
	types/SlotTable.h
	types/Time32.h
	Utils.h
	vlong.h)
# Issue #116: the drawing interface's value types (TRect, TRGBColor, TFont), graphics.h's until then; Skins.h includes it.
list(APPEND PATELLA_HEADERS_CORE types/GraphicsTypes.h)
# TRegexp, the owner of a compiled regexp (issue #129; patella_core's regexp.c, Status.cpp's link check).
list(APPEND PATELLA_HEADERS_CORE types/Regexp.h)
# Issue #119: the platform interfaces of part 2 (portable).
list(APPEND PATELLA_HEADERS_CORE platform/Clipboard.h platform/Input.h platform/Window.h)
# Issue #138: TSurface, the platform's drawing surface (portable; the engines implement it).
list(APPEND PATELLA_HEADERS_CORE platform/Surface.h)
# Issue #122: the SDL3 window's loop timer and the choice of the window path (portable).
list(APPEND PATELLA_HEADERS_CORE platform/LoopTimer.h platform/WindowChoice.h)
# Issue #123: the client's keyboard and mouse rules over SDL3's events, and the cursor resources' decoder (portable).
list(APPEND PATELLA_HEADERS_CORE platform/CursorImage.h platform/KeyInput.h)
# Issue #124: the display's modes over SDL3 as the client lists and sets them (portable).
list(APPEND PATELLA_HEADERS_CORE platform/DisplayModeList.h)
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
	testmode/PipeNetwork.h
	testmode/Png.h
	testmode/ServerReplay.h
	testmode/StateDump.h)
# The Vulkan engine's portable part (tests_render on every preset; patella_render_vulkan on x64).
set(PATELLA_HEADERS_RENDER
	vulkan/DeviceChoice.h
	vulkan/FilterMask.h
	vulkan/Quads.h)
# The client's own headers (Windows, MFC or its shim): the x64 client's, and the golden tests' (with the shim).
set(PATELLA_HEADERS_CLIENT
	Client.h
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
	MapState.h
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
	platform/SdlPlatform.h
	platform/Win32Platform.h
	sysinc.h
	res/resource.h)
# Issue #110: patella_core compiles Network.cpp, Communication.cpp and the game state's files, so their headers and
# the headers they include are portable (checked with patella_core's settings, and still with the client's below).
set(patella_headers_core_110
	Communication.h
	Creatures.h
	Help.h
	Inventory.h
	Map.h
	MapState.h
	Network.h
	Objects.h
	Preferences.h
	Script.h
	Status.h
	Talk.h
	TestHooks.h
	Text.h
	sysinc.h)
list(REMOVE_ITEM PATELLA_HEADERS_CLIENT ${patella_headers_core_110})
list(APPEND PATELLA_HEADERS_CORE ${patella_headers_core_110} types/Callback.h)
# Issue #122: the SDL3 window and its loop (no SDL header in it; the golden tests compile Player.cpp, which includes it).
list(APPEND PATELLA_HEADERS_CLIENT SdlFrame.h)
# The x64 client's Vulkan engine (patella_render_vulkan and TSurfaceVulkan, cmake/Vulkan.cmake).
set(PATELLA_HEADERS_VULKAN_CLIENT
	vulkan/DeviceSettings.h
	vulkan/PresentWindow.h
	vulkan/Renderer.h
	vulkan/SurfaceVulkan.h
	vulkan/WindowSystem.h)
# The Windows side of the test mode (PatellaTest.exe).
set(PATELLA_HEADERS_TEST_BUILD
	testmode/ClientStateWin32.h
	testmode/PipeSocketWin32.h
	testmode/SdlInputWin32.h)
# Issue #111: patella_core compiles the rest of the client but the platform layer (the preferences, the state, the GUI's
# logic, the windows and the dialogs), so their headers and what they include are portable too: checked with
# patella_core's settings, and still with the client's below. vulkan/DeviceSettings.h (the graphics device choice,
# no Vulkan header) is what Dialogs.cpp and Control.cpp call of the platform's engine.
set(patella_headers_core_111
	Client.h
	ConsoleWindow.h
	ContentWindows.h
	ContextMenus.h
	Control.h
	DialogElements.h
	DialogWindow.h
	Dialogs.h
	GUI.h
	MainWindow.h
	MapWindow.h
	Surface.h
	graphics.h)
list(REMOVE_ITEM PATELLA_HEADERS_CLIENT ${patella_headers_core_111})
list(REMOVE_ITEM PATELLA_HEADERS_VULKAN_CLIENT vulkan/DeviceSettings.h)
list(APPEND PATELLA_HEADERS_CORE ${patella_headers_core_111} vulkan/DeviceSettings.h)
# Headers that are not checked on their own, each with the reason. (The shaders' SPIR-V headers, ColorKeyFrag.h and
# the others, are generated into the build tree by glslang, cmake/Vulkan.cmake: not headers of src/.)
# - compat/mfc_shim.h: MFC's stand-in (TIBIA_MFC_SHIM), which only the golden tests' targets compile (tests/golden,
#   through common.h, which includes it first). Its check of its own was the oracle's header_check_oracle_shim,
#   which left with the oracle (issue #94); every golden target still compiles it, on Windows and on Linux.
set(PATELLA_HEADER_CHECK_EXCEPTIONS
	compat/mfc_shim.h)

# Every header of src/ outside src/Extern is on a list.
file(GLOB_RECURSE patella_all_headers RELATIVE "${PROJECT_SOURCE_DIR}/src" CONFIGURE_DEPENDS
	"${PROJECT_SOURCE_DIR}/src/*.h" "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/src/*.inl"
	"${PROJECT_SOURCE_DIR}/src/*.inc")
list(FILTER patella_all_headers EXCLUDE REGEX "^Extern/")
set(patella_listed_headers ${PATELLA_HEADERS_CORE} ${PATELLA_HEADERS_CXX26} ${PATELLA_HEADERS_TESTMODE}
	${PATELLA_HEADERS_RENDER} ${PATELLA_HEADERS_CLIENT} ${PATELLA_HEADERS_VULKAN_CLIENT} ${PATELLA_HEADERS_TEST_BUILD}
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

function(patella_header_checks)
	if(TARGET patella_core)
		patella_header_check(header_check_core ${PATELLA_HEADERS_CORE} ${PATELLA_HEADERS_CXX26})
		target_link_libraries(header_check_core PRIVATE patella_core)
		patella_target_language(header_check_core)
		patella_target_warnings(header_check_core)
		patella_target_hardening(header_check_core)
		# With the stand-ins of tests_unit_cxx26_standin: compat/format.h's (issue #57), compat/span.h's (issue
		# #59) and compat/mdspan.h's (issue #101) too, and the headers that format through it (issue #64; the developer log, issue #77); compat/assertion.h's
		# source location (issue #79).
		patella_header_check(header_check_cxx26_standin ${PATELLA_HEADERS_CXX26} compat/assertion.h compat/format.h
			compat/mdspan.h compat/print.h compat/span.h log/Log.h types/Cp1252.h types/File.h types/FixedString.h
			types/SlotTable.h)
		target_compile_definitions(header_check_cxx26_standin PRIVATE PATELLA_CXX26_STANDIN PATELLA_FORMAT_STANDIN
			PATELLA_SPAN_STANDIN PATELLA_SOURCE_LOCATION_STANDIN PATELLA_MDSPAN_STANDIN)
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
	if(TARGET Patella_compile OR TARGET Patella)
		set(headers ${PATELLA_HEADERS_CORE} ${PATELLA_HEADERS_RENDER} ${PATELLA_HEADERS_CLIENT}
			${PATELLA_HEADERS_VULKAN_CLIENT})
		patella_header_check(header_check_client ${headers})
		patella_client_compile_settings(header_check_client)
		patella_header_check(header_check_test_build ${headers} ${PATELLA_HEADERS_TESTMODE}
			${PATELLA_HEADERS_TEST_BUILD})
		patella_client_compile_settings(header_check_test_build)
		target_compile_definitions(header_check_test_build PRIVATE PATELLA_TEST_MODE)
	endif()
endfunction()
