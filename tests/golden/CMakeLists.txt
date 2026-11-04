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
# The ported tests (core_tests.cpp, regexp_tests.cpp, sysinc_tests.cpp, and since issue #26 the
# *_tests.cpp of the client's code) are the oracle's difftest and sysinctest sources as well
# (cmake/OracleX86.cmake); here they see no original (GOLDEN_ORACLE unset).

if(NOT (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8))
	return()
endif()

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

set(PATELLA_GOLDEN_DIR "${PROJECT_SOURCE_DIR}/build/golden" CACHE PATH
	"The golden corpus that the CTest test golden replays (recorded by oracle-x86's golden_corpus; local only)")
option(PATELLA_GOLDEN_REQUIRE "The CTest test golden fails instead of being skipped when there is no corpus" OFF)

# The client's code the ported tests call (issue #26) is not portable yet either: compiled against
# src/compat/mfc_shim.h, as the oracle's difftest compiles it, with the product's settings. The real
# MFC headers switch C4201 (nameless struct/union, graphics.h) off for every file that includes them
# (afx.h); the shim does not, so the files that include graphics.h get the same switch here.
function(golden_client_settings target)
	target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	target_compile_definitions(${target} PRIVATE TIBIA_MFC_SHIM)
	patella_target_language(${target})
	patella_target_warnings(${target})
	patella_target_hardening(${target})
	target_compile_options(${target} PRIVATE /wd4201)
endfunction()

# sysinc.cpp is not portable yet (the clipboard, timeGetTime): compiled against src/compat/mfc_shim.h,
# as the oracle's sysinctest compiles it. The oracle keeps it out of difftest.exe, whose tests define
# GetMilliseconds, RemoveFile, SetClipboardText and FileExists as recorders (status_tests.cpp,
# script_tests.cpp); the runner has both in one program, so sysinc.cpp's four are renamed here.
add_library(golden_sysinc OBJECT "${PROJECT_SOURCE_DIR}/src/sysinc.cpp")
golden_client_settings(golden_sysinc)
target_compile_definitions(golden_sysinc PRIVATE GetMilliseconds=SysincGetMilliseconds RemoveFile=SysincRemoveFile
	SetClipboardText=SysincSetClipboardText FileExists=SysincFileExists)

# The reconstructed files the ported tests compare (difftest.exe links them all; these are the ones
# the ported tests call, and the ones that are called from them on the paths the tests run), with
# PATELLA_VULKAN as Patella.exe has it (only Surface.cpp differs: the Vulkan engine as its only engine);
# client_stubs.cpp has the functions of the other files, which the tests never call. Since issue #28
# graphics.cpp, GUI.cpp and every window file (the classes the window tests build and the files Errors,
# MapWindow, MainWindow, Communication and Control reach) are here too, as in difftest.exe; the stubs are the
# files below them (Communication.cpp's Send functions, Control.cpp, the Vulkan engine). MainFrm.cpp and
# Network.cpp are compiled into their tests (mainfrm_tests.cpp, network_tests.cpp, as in the oracle: the
# tests read their statics).
add_library(golden_client OBJECT
	"${PROJECT_SOURCE_DIR}/src/Status.cpp"
	"${PROJECT_SOURCE_DIR}/src/Preferences.cpp"
	"${PROJECT_SOURCE_DIR}/src/Script.cpp"
	"${PROJECT_SOURCE_DIR}/src/Talk.cpp"
	"${PROJECT_SOURCE_DIR}/src/Player.cpp"
	"${PROJECT_SOURCE_DIR}/src/Surface.cpp"
	"${PROJECT_SOURCE_DIR}/src/Help.cpp"
	"${PROJECT_SOURCE_DIR}/src/Inventory.cpp"
	"${PROJECT_SOURCE_DIR}/src/Objects.cpp"
	"${PROJECT_SOURCE_DIR}/src/Map.cpp"
	"${PROJECT_SOURCE_DIR}/src/Creatures.cpp"
	"${PROJECT_SOURCE_DIR}/src/graphics.cpp"
	"${PROJECT_SOURCE_DIR}/src/GUI.cpp"
	"${PROJECT_SOURCE_DIR}/src/DialogWindow.cpp"
	"${PROJECT_SOURCE_DIR}/src/ContextMenus.cpp"
	"${PROJECT_SOURCE_DIR}/src/ConsoleWindow.cpp"
	"${PROJECT_SOURCE_DIR}/src/ContentWindows.cpp"
	"${PROJECT_SOURCE_DIR}/src/DialogElements.cpp"
	"${PROJECT_SOURCE_DIR}/src/Dialogs.cpp"
	"${PROJECT_SOURCE_DIR}/src/MapWindow.cpp"
	"${PROJECT_SOURCE_DIR}/src/MainWindow.cpp")
golden_client_settings(golden_client)
target_compile_definitions(golden_client PRIVATE PATELLA_VULKAN)

# The ported tests of the client's code and the stubs, with the same settings.
add_library(golden_client_tests OBJECT
	client_stubs.cpp
	status_tests.cpp
	preferences_tests.cpp
	script_tests.cpp
	player_tests.cpp
	help_tests.cpp
	inventory_tests.cpp
	talk_tests.cpp
	container_tests.cpp
	objects_tests.cpp
	map_tests.cpp
	creatures_tests.cpp
	golden_eh.cpp
	mainfrm_tests.cpp
	graphics_tests.cpp
	gui_tests.cpp
	elementfakes.cpp
	dialogwindow_tests.cpp
	contextmenus_tests.cpp
	consolewindow_tests.cpp
	contentwindows_tests.cpp
	dialogelements_tests.cpp
	dialogs_tests.cpp)
golden_client_settings(golden_client_tests)
# Network (issue #30): network_tests.cpp in a directory of its own (network/CMakeLists.txt, its exception
# model).
add_subdirectory(network)

add_executable(golden_runner
	runner.cpp
	golden.cpp
	core_tests.cpp
	regexp_tests.cpp
	sysinc_tests.cpp
	$<TARGET_OBJECTS:golden_sysinc>
	$<TARGET_OBJECTS:golden_client>
	$<TARGET_OBJECTS:golden_client_tests>
	$<TARGET_OBJECTS:golden_network_tests>)
target_link_libraries(golden_runner PRIVATE patella_core winmm user32)
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/build/golden" golden_default_dir)
target_compile_definitions(golden_runner PRIVATE "GOLDEN_DEFAULT_DIR=\"${golden_default_dir}\"")
patella_target_language(golden_runner)
patella_target_warnings(golden_runner)
patella_target_hardening(golden_runner)
# The ported tests redirect some of our functions to their fakes (golden_patch.h): no function may share
# its code with another. Nor may a function have two incremental-link thunks (issue #28): the element fakes
# find our constructors through their decorated names (elementfakes.cpp), which an incremental link gives a
# thunk of their own, so that patching it would not reach the calls; without incremental linking every
# address is the function's code, in Debug as in Release.
target_link_options(golden_runner PRIVATE /OPT:NOICF /INCREMENTAL:NO)

if(PATELLA_GOLDEN_REQUIRE)
	add_test(NAME golden COMMAND golden_runner --require "${PATELLA_GOLDEN_DIR}")
else()
	add_test(NAME golden COMMAND golden_runner "${PATELLA_GOLDEN_DIR}")
endif()
set_tests_properties(golden PROPERTIES SKIP_RETURN_CODE 77)
