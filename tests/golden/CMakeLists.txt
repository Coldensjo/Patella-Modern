# The golden corpus's runner (notes/modernization-plan.md, 4.3; issue #24): golden_runner replays the
# corpus that the oracle-x86 preset's golden_corpus target records (tests/golden/golden.h), against this
# build's code and without Tibia.exe: on Windows x64 with MSVC (plan M2) and, since issue #91 (plan M6), on Linux x64
# with GCC and Clang (linux-x64-gcc, linux-x64-clang), where tests/golden/linux stands in for the Windows API and
# MFC parts the client's code and the tests use (tests/golden/linux/README.md).
#
# CTest test golden: the corpus in PATELLA_GOLDEN_DIR (default build/golden of the source tree, where
# golden_corpus records it). Without a corpus (hosted CI: it is derived from Tibia.exe and never leaves
# the recording machine) the runner says so and the test is skipped (exit code 77); with
# PATELLA_GOLDEN_REQUIRE=ON (the self-hosted workflow) a missing corpus fails it.
#
# The ported tests (core_tests.cpp, regexp_tests.cpp, sysinc_tests.cpp, and since issue #26 the
# *_tests.cpp of the client's code) are the oracle's difftest and sysinctest sources as well
# (cmake/OracleX86.cmake); here they see no original (GOLDEN_ORACLE unset).

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	return()
endif()
if(MSVC)
	set(golden_linux OFF)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set(golden_linux ON)
else()
	return()
endif()

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

# The default: the environment's PATELLA_GOLDEN_DIR at the first configure (tools/build.py sets it to /golden in the
# Docker image, where it mounts the corpus read-only; issue #91), else build/golden of the source tree.
if(DEFINED ENV{PATELLA_GOLDEN_DIR} AND NOT "$ENV{PATELLA_GOLDEN_DIR}" STREQUAL "")
	file(TO_CMAKE_PATH "$ENV{PATELLA_GOLDEN_DIR}" golden_dir_default)
else()
	set(golden_dir_default "${PROJECT_SOURCE_DIR}/build/golden")
endif()
set(PATELLA_GOLDEN_DIR "${golden_dir_default}" CACHE PATH
	"The golden corpus that the CTest test golden replays (recorded by oracle-x86's golden_corpus; local only)")
option(PATELLA_GOLDEN_REQUIRE "The CTest test golden fails instead of being skipped when there is no corpus" OFF)

# The client's code the ported tests call (issue #26) is not portable yet either: compiled against
# src/compat/mfc_shim.h, as the oracle's difftest compiles it, with the product's settings. The real
# MFC headers switch C4201 (nameless struct/union, graphics.h) off for every file that includes them
# (afx.h); the shim does not, so the files that include graphics.h get the same switch here.
# On Linux (issue #91) the Windows headers the shim includes are tests/golden/linux's (windows.h, winsock2.h: the
# types, constants and functions the code uses; the functions themselves, behind import slots, linux/win32.cpp).
# The tests redirect our functions to their fakes by overwriting the functions' first bytes (golden_patch.h), so
# the code is compiled with semantic interposition (-fsemantic-interposition; the objects are position-independent,
# -fPIC): the compiler neither inlines a global function into a caller, nor calls a clone of it, nor concludes from
# its body that it cannot throw (a fake throws in the error rounds), as MSVC does not across its object files.
set(golden_linux_dir "${CMAKE_CURRENT_SOURCE_DIR}/linux")
# What MSVC does that GCC and Clang do only when told (golden_linux_options), and the warnings of theirs that the
# original's code and the tests' ways with it give where MSVC gives none (golden_linux_warnings, after -Wall on each
# source: Clang takes the last of two options, GCC the explicit one). notes/decisions.md, "The golden corpus on
# Linux (issue #91)".
# - -fno-strict-aliasing: the tests read our objects, vtables and member pointers through other types (as difftest
#   does); MSVC never optimises on the aliasing rules.
# - -fno-delete-null-pointer-checks: the MFC shim's GetSafeHwnd compares this with nullptr, as MFC's does, and MSVC
#   keeps the comparison; GCC and Clang warn about it (-Wno-nonnull-compare, -Wno-tautological-undefined-compare).
# - -fwrapv: the error rounds give the code extreme sizes and positions (INT_MAX), whose sums the original and MSVC's
#   code wrap around (TWindow::FitToChild, GUI.cpp); GCC and Clang assume a signed int never overflows, and UBSan
#   reports it (signed-integer-overflow, which -fwrapv makes defined).
# - With UBSan (linux-x64-clang): -fno-sanitize=vptr,object-size. The window tests put objects of their own (fakes,
#   throwers, stand-ins smaller than the real class) where our code expects its classes, as difftest does: our code
#   calls them through pointers of our types, whose dynamic type is not the static one. UBSan's checks of the dynamic
#   type and of the object's size report each such call and cast; its other checks stay.
# - GCC: -fno-lifetime-dse: the tests fill memory with a pattern before they construct an object in it, to compare
#   the bytes a constructor leaves untouched (TSkinProperties, TTalkBuffer), which GCC removes as dead stores;
#   -fno-toplevel-reorder: the globals in the order of their definitions, as MSVC places them (Network.cpp's
#   ReceiveData, whose over-read the Network test replays, #72, is followed by SendData).
# - -Wno-invalid-offsetof: the fakes and dumps take offsetof of our classes, which are not standard-layout (GCC and
#   Clang compute it as MSVC does: no virtual bases).
# - -Wno-overloaded-virtual: the original's classes hide virtual functions of their bases (DialogElements.h).
# - -Wno-delete-non-virtual-dtor (Clang also -Wno-delete-non-abstract-non-virtual-dtor): the tests' buffers, and
#   Talk.cpp's TTalkBuffer, are polymorphic classes without a virtual destructor, deleted through their own type.
# - GCC: -Wno-class-memaccess (the tests clear and copy our objects with memset and memcpy, as difftest does),
#   -Wno-stringop-truncation (the original's strncpy to the whole buffer, Status.cpp and Script.cpp) and
#   -Wno-mismatched-new-delete (commworld.cpp's windows keep their memory in a class operator delete of their own).
# - Clang: -Wno-non-pod-varargs: TMultiStateButton's constructor takes its callbacks (TCallback, trivially copyable
#   but with a constructor) through ..., as the original's does (DialogElements.cpp, consolewindow_tests.cpp).
set(golden_linux_options -fno-strict-aliasing -fno-delete-null-pointer-checks -fwrapv)
set(golden_linux_warnings -Wno-invalid-offsetof -Wno-overloaded-virtual -Wno-delete-non-virtual-dtor)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	list(APPEND golden_linux_options -fno-lifetime-dse -fno-toplevel-reorder)
	list(APPEND golden_linux_warnings -Wno-class-memaccess -Wno-stringop-truncation -Wno-mismatched-new-delete
		-Wno-nonnull-compare)
else()
	if(PATELLA_SANITIZE MATCHES "undefined")
		list(APPEND golden_linux_options -fno-sanitize=vptr,object-size)
	endif()
	list(APPEND golden_linux_warnings -Wno-tautological-undefined-compare -Wno-delete-non-abstract-non-virtual-dtor
		-Wno-non-pod-varargs)
endif()
# GCC and Clang compile the client's code and the ported tests for the first time here (issue #91): at the warning
# baseline's level (cmake/Warnings.cmake: -Wall, warnings as errors), as MSVC compiles most of them (its baseline,
# /W3), with golden_linux_warnings after it. Every source of a golden target gets them before
# patella_target_warnings, which then leaves the source's level as it is.
function(golden_linux_warning_level target)
	get_target_property(sources ${target} SOURCES)
	foreach(source IN LISTS sources)
		if(source MATCHES "\\.(c|cpp)$")
			set_property(SOURCE "${source}" TARGET_DIRECTORY ${target} APPEND PROPERTY COMPILE_OPTIONS -Wall
				${golden_linux_warnings})
		endif()
	endforeach()
endfunction()
function(golden_client_settings target)
	target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
	target_compile_definitions(${target} PRIVATE TIBIA_MFC_SHIM)
	patella_target_language(${target})
	if(golden_linux)
		golden_linux_warning_level(${target})
	endif()
	patella_target_warnings(${target})
	patella_target_hardening(${target})
	if(golden_linux)
		target_include_directories(${target} BEFORE PRIVATE "${golden_linux_dir}")
		target_compile_options(${target} PRIVATE -fsemantic-interposition ${golden_linux_options})
	else()
		target_compile_options(${target} PRIVATE /wd4201)
	endif()
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
if(golden_linux)
	# The constructors of TToggleButton and TTextToggleButton pass their base the button's own Tooltip before it is
	# set, as the original's do (DialogElements.cpp); GCC and Clang warn (-Wuninitialized), MSVC does not.
	set_property(SOURCE "${PROJECT_SOURCE_DIR}/src/DialogElements.cpp" TARGET_DIRECTORY golden_client APPEND PROPERTY
		COMPILE_OPTIONS -Wno-uninitialized)
endif()

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
	dialogs_tests.cpp
	mapwindow_tests.cpp
	mainwindow_tests.cpp
	errors_tests.cpp
	divisions_tests.cpp)
golden_client_settings(golden_client_tests)
# Network (issue #30): network_tests.cpp in a directory of its own (network/CMakeLists.txt, its exception
# model).
add_subdirectory(network)
# Communication (issue #30): its world, Communication.cpp and Control.cpp in a directory of their own
# (comm/CMakeLists.txt, their exception model).
add_subdirectory(comm)

add_executable(golden_runner
	runner.cpp
	golden.cpp
	core_tests.cpp
	regexp_tests.cpp
	sysinc_tests.cpp
	$<TARGET_OBJECTS:golden_sysinc>
	$<TARGET_OBJECTS:golden_client>
	$<TARGET_OBJECTS:golden_client_tests>
	$<TARGET_OBJECTS:golden_network_tests>
	$<TARGET_OBJECTS:golden_comm_tests>)
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/build/golden" golden_default_dir)
target_compile_definitions(golden_runner PRIVATE "GOLDEN_DEFAULT_DIR=\"${golden_default_dir}\"")
if(golden_linux)
	# The runner's own sources see the same stand-ins (golden_test.h includes common.h, which includes the MFC
	# shim), and linux/win32.cpp has the Windows functions. The C library's functions the tests replace through
	# their import slots on Windows (time, the _time64 of the MSVC runtime, and fwrite; rand is the stand-ins'
	# own, linux/windows.h) are wrapped (ld --wrap): every call of the runner's objects goes to __wrap_<name>,
	# which calls through a slot of linux/win32.cpp, the C library's function at first (__real_<name>).
	target_sources(golden_runner PRIVATE linux/win32.cpp)
	golden_linux_warning_level(golden_runner)
endif()
patella_target_language(golden_runner)
patella_target_warnings(golden_runner)
patella_target_hardening(golden_runner)
if(golden_linux)
	target_include_directories(golden_runner BEFORE PRIVATE "${golden_linux_dir}")
	target_compile_definitions(golden_runner PRIVATE TIBIA_MFC_SHIM)
	target_compile_options(golden_runner PRIVATE ${golden_linux_options})
	target_link_libraries(golden_runner PRIVATE patella_core)
	target_link_options(golden_runner PRIVATE LINKER:--wrap=time LINKER:--wrap=fwrite)
else()
	target_link_libraries(golden_runner PRIVATE patella_core winmm user32)
	# The ported tests redirect some of our functions to their fakes (golden_patch.h): no function may share
	# its code with another. Nor may a function have two incremental-link thunks (issue #28): the element fakes
	# find our constructors through their decorated names (elementfakes.cpp), which an incremental link gives a
	# thunk of their own, so that patching it would not reach the calls; without incremental linking every
	# address is the function's code, in Debug as in Release.
	target_link_options(golden_runner PRIVATE /OPT:NOICF /INCREMENTAL:NO)
endif()

if(PATELLA_GOLDEN_REQUIRE)
	add_test(NAME golden COMMAND golden_runner --require "${PATELLA_GOLDEN_DIR}")
else()
	add_test(NAME golden COMMAND golden_runner "${PATELLA_GOLDEN_DIR}")
endif()
# Issue #84: CTest runs the tests in parallel (CMakePresets.json, execution.jobs); the longest tests start first.
set_tests_properties(golden PROPERTIES SKIP_RETURN_CODE 77 COST 1000)
# Linux with AddressSanitizer (linux-x64-clang, issue #91): the original's mismatched releases (free of memory from new,
# Talk.cpp, Preferences.cpp and the others of notes/decisions.md's ownership map, #58: kept) and the leaks of the
# original and of the tests' worlds are not reported, as MSVC's AddressSanitizer reports neither on Windows; every
# other check of ASan and UBSan stays (a finding ends the run: -fno-sanitize-recover=all).
if(golden_linux AND PATELLA_SANITIZE MATCHES "address")
	set_tests_properties(golden PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:alloc_dealloc_mismatch=0")
endif()

# Issue #65: TDialogWindow::SetSize when a child's own SetSize deletes it (dialogwindow_setsize_test.cpp). Our
# columns alone, no corpus: the runner's objects with the test in place of runner.cpp. Under ASan
# (windows-x64-asan) the use after free of the code before the fix is a heap-use-after-free report.
add_library(golden_dialogwindow_setsize OBJECT dialogwindow_setsize_test.cpp)
golden_client_settings(golden_dialogwindow_setsize)
target_link_libraries(golden_dialogwindow_setsize PRIVATE doctest::doctest)
add_executable(dialogwindow_setsize
	golden.cpp
	core_tests.cpp
	regexp_tests.cpp
	sysinc_tests.cpp
	$<TARGET_OBJECTS:golden_sysinc>
	$<TARGET_OBJECTS:golden_client>
	$<TARGET_OBJECTS:golden_client_tests>
	$<TARGET_OBJECTS:golden_network_tests>
	$<TARGET_OBJECTS:golden_comm_tests>
	$<TARGET_OBJECTS:golden_dialogwindow_setsize>)
target_link_libraries(dialogwindow_setsize PRIVATE patella_core winmm user32)
patella_target_language(dialogwindow_setsize)
patella_target_warnings(dialogwindow_setsize)
patella_target_hardening(dialogwindow_setsize)
target_link_options(dialogwindow_setsize PRIVATE /OPT:NOICF /INCREMENTAL:NO)
add_test(NAME dialogwindow_setsize COMMAND dialogwindow_setsize)

# Issue #66: two runners at once over the same corpus (concurrent.py), each in its temporary folder of its own
# (runner.cpp, TRunFolder): 0 failures each, and neither folder left below TEMP. Skipped without a corpus.
# Issue #84: only in the Release configuration of the presets without ASan (windows-x64). What it checks is the
# runner's folder handling, which is the same code in Debug and under ASan; there it only doubled the longest test
# (windows-x64-debug 360 s, windows-x64-asan 820 s). It runs next to `golden` (CTest in parallel), a third runner at
# once, and holds two processors (PROCESSORS 2).
if(PATELLA_SANITIZE STREQUAL "")
	set(Python3_FIND_REGISTRY LAST)
	find_package(Python3 REQUIRED COMPONENTS Interpreter)
	if(PATELLA_GOLDEN_REQUIRE)
		set(golden_concurrent_require --require)
	else()
		set(golden_concurrent_require)
	endif()
	add_test(NAME golden_concurrent CONFIGURATIONS Release COMMAND "${Python3_EXECUTABLE}" -I
		"${CMAKE_CURRENT_SOURCE_DIR}/concurrent.py" $<TARGET_FILE:golden_runner> ${golden_concurrent_require}
		"${PATELLA_GOLDEN_DIR}")
	set_tests_properties(golden_concurrent PROPERTIES SKIP_RETURN_CODE 77 PROCESSORS 2 COST 1000)
endif()
