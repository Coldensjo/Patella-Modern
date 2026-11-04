# The oracle-x86 preset (issue #4; notes/modernization-plan.md, sections 5 and 7, M1): a test
# instrument until M6, not a product.
#
# - Patella.exe with exactly the compiler, resource compiler and linker command lines of the former
#   Patella.vcxproj (Release and Debug; the MSBuild project and the .cmd build scripts were removed in
#   issue #10): /std:c++17 /W3, static MFC (nafxcw.lib, pulled in by the MFC headers), /MT, Tibia.exe's
#   import libraries in their order, oleacc.dll delay-loaded, no manifest; SurfaceGL.cpp alone compiles
#   without /FC (issue #14, below). The resource pictures are extracted from the local Tibia.exe at
#   configure time; after every link tools/compare_resources.py checks the resources against Tibia.exe's,
#   as tools\build-patella.cmd did.
# - difftest, sysinctest, communicationtest and vlongtest with exactly the command lines of the former
#   tools\difftest\build.cmd (the same in every configuration), run as CTest tests against
#   PATELLA_TIBIA_EXE, each with a private TEMP; the 32-bit capture DLL patcap.dll of the former
#   tools\build-capture.cmd (cmake/Capture.cmake; tools/compare_rendering.py uses
#   build\oracle-x86\Release\patcap.dll for the 32-bit clients); and
#   mfc_shim_check, every source compiled against src/compat/mfc_shim.h with the options of the former
#   tools\compile-check.cmd.
# - The golden corpus's recorder (issue #24): the test programs also compile tests/golden/golden.cpp and
#   the ported tests of tests/golden with GOLDEN_ORACLE; the target golden_corpus records the corpus.
#
# CMake's own default flags (/DWIN32 /D_WINDOWS /GR /EHsc, /Ob2, /machine, /INCREMENTAL, its
# standard libraries) are cleared below, so every flag on the command lines is one written here.
# Configure with MSVC for x86 set up (vcvarsall.bat x86): `python -I tools/build.py oracle-x86` does it.

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC"
		OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "oracle-x86 needs MSVC's x86 compiler (cl.exe for x86): run python -I tools/build.py "
		"oracle-x86, or configure from a developer prompt set up with vcvarsall.bat x86 (found "
		"${CMAKE_CXX_COMPILER_ID}, ${CMAKE_SIZEOF_VOID_P}-byte pointers)")
endif()

enable_language(RC)
enable_language(ASM_MASM)

# The Python on PATH, as the former .cmd scripts ran it, before any registered elsewhere.
set(Python3_FIND_REGISTRY LAST)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(PATELLA_TIBIA_EXE "${PROJECT_SOURCE_DIR}/Tibia.exe" CACHE FILEPATH
	"The original Tibia.exe: the resource pictures are extracted from it and the tests compare against it")
if(NOT EXISTS "${PATELLA_TIBIA_EXE}")
	message(FATAL_ERROR "${PATELLA_TIBIA_EXE} is missing: the resource pictures are extracted from it and "
		"the tests compare against it (notes/resources.md; set PATELLA_TIBIA_EXE to use another copy)")
endif()
file(TO_NATIVE_PATH "${PATELLA_TIBIA_EXE}" PATELLA_TIBIA_EXE_NATIVE)

# No flag of CMake's own: the targets below set every one.
foreach(lang C CXX RC ASM_MASM)
	set(CMAKE_${lang}_FLAGS "")
	foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
		set(CMAKE_${lang}_FLAGS_${config} "")
	endforeach()
endforeach()
set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_CXX_STANDARD_LIBRARIES "")
foreach(kind EXE SHARED MODULE)
	set(CMAKE_${kind}_LINKER_FLAGS "")
	foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
		set(CMAKE_${kind}_LINKER_FLAGS_${config} "")
	endforeach()
endforeach()
set(CMAKE_ASM_MASM_FLAGS "/nologo")
# rc.exe as MSBuild runs it (its /D NDEBUG or /D _DEBUG come from the target's definitions).
set(CMAKE_RC_FLAGS "/l0x0409 /nologo")
# Not removable: Ninja Multi-Config defines CMAKE_INTDIR="<configuration>" on every compile, which
# no source uses.

set(DIFFTEST_DIR "${PROJECT_SOURCE_DIR}/tools/difftest")
set(GOLDEN_DIR "${PROJECT_SOURCE_DIR}/tests/golden")
set(ORACLE_TMP_DIR "${PROJECT_BINARY_DIR}/tmp")

# ---------------------------------------------------------------------------------------------------
# The resource pictures (cursors, icon, bitmaps) of the local Tibia.exe, for src/res/Patella.rc; the
# former MSBuild project's ExtractResources target did the same before the resources compiled.

set(PATELLA_RES_DIR "${PROJECT_BINARY_DIR}/res")
include("${PROJECT_SOURCE_DIR}/cmake/Resources.cmake")
patella_extract_resources("${PATELLA_TIBIA_EXE}" "${PATELLA_RES_DIR}")

# ---------------------------------------------------------------------------------------------------
# Patella.exe: the former Patella.vcxproj's command lines (its MSBuild log), in their order. /Oy- must
# follow /O2, which turns frame pointer omission on.

# MSBuild's order, in which the objects are linked: src\*.cpp sorted without case (the file system's
# order), then the C files of the regexp.
file(GLOB patella_cpp_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
list(SORT patella_cpp_sources CASE INSENSITIVE)
file(GLOB patella_c_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/Extern/regexp/*.c")
list(SORT patella_c_sources CASE INSENSITIVE)
set(patella_rc "${PROJECT_SOURCE_DIR}/src/res/Patella.rc")
set_source_files_properties("${patella_rc}" PROPERTIES OBJECT_DEPENDS "${PATELLA_RES_DIR}/resources.tsv")

add_executable(Patella WIN32 ${patella_cpp_sources} ${patella_c_sources} "${patella_rc}")
set_target_properties(Patella PROPERTIES
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
target_include_directories(Patella PRIVATE
	"$<$<COMPILE_LANGUAGE:C,CXX>:${PROJECT_SOURCE_DIR}/src>"
	"$<$<COMPILE_LANGUAGE:RC>:${PATELLA_RES_DIR}>")
target_compile_definitions(Patella PRIVATE
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Release>:NDEBUG>
	$<$<COMPILE_LANGUAGE:C,CXX>:WIN32>
	$<$<COMPILE_LANGUAGE:C,CXX>:_WINDOWS>
	$<$<COMPILE_LANGUAGE:C,CXX>:_WIN32_WINNT=0x0601>)
target_compile_options(Patella PRIVATE
	$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:Debug>>:/JMC>
	$<$<COMPILE_LANGUAGE:C,CXX>:/W3 /WX- /diagnostics:column /sdl->
	$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:Release>>:/O2 /Oi>
	$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:Debug>>:/Od>
	$<$<COMPILE_LANGUAGE:C,CXX>:/Oy- /Gm- /EHsc>
	$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:Debug>>:/RTC1>
	$<$<COMPILE_LANGUAGE:C,CXX>:/GS>
	$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:Release>>:/Gy>
	$<$<COMPILE_LANGUAGE:C,CXX>:/fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline>
	$<$<COMPILE_LANGUAGE:CXX>:/std:c++17>
	$<$<COMPILE_LANGUAGE:C,CXX>:/permissive- /external:W3 /Gd /analyze->)
# The vcxproj's last two options, /FC /Zc:strictStrings-, per source (a source's options follow the
# target's, so every other command line stays the vcxproj's), and /FC not for SurfaceGL.cpp (issue #14): /FC
# makes the name in glh_extensions.h's `#line 173 "Extern\glh/glh_extensions.h"` absolute from the
# compiler's working directory, and the original's malloc-failure message prints it as written. Every
# other name cl gets is absolute already, so /FC changes nothing else there. The genex keeps the
# options off mfc_shim_check, which compiles the same sources (source properties are the directory's).
set(patella_fc_sources ${patella_cpp_sources} ${patella_c_sources})
list(LENGTH patella_fc_sources patella_source_count)
list(REMOVE_ITEM patella_fc_sources "${PROJECT_SOURCE_DIR}/src/SurfaceGL.cpp")
list(LENGTH patella_fc_sources patella_fc_count)
math(EXPR patella_fc_count "${patella_fc_count} + 1")
if(NOT patella_fc_count EQUAL patella_source_count)
	message(FATAL_ERROR "src/SurfaceGL.cpp is not among Patella's sources (the /FC exception, issue #14)")
endif()
set_property(SOURCE ${patella_fc_sources} APPEND PROPERTY COMPILE_OPTIONS
	"$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,Patella>:/FC>")
set_property(SOURCE ${patella_cpp_sources} ${patella_c_sources} APPEND PROPERTY COMPILE_OPTIONS
	"$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,Patella>:/Zc:strictStrings->")
# Tibia.exe's import table in its order (notes/decisions.md, "Building Patella.exe (issue #16)");
# Direct3D 9 is loaded at run time. MSBuild adds delayimp.lib for /DELAYLOAD.
target_link_libraries(Patella PRIVATE
	glu32 opengl32 ddraw dxguid winmm kernel32 user32 gdi32 winspool advapi32 shell32 comctl32 shlwapi
	oleaut32 ws2_32 delayimp)
# afxsock.h asks for wsock32.lib, which MFC's socket code would then import from; Tibia.exe imports
# every Windows Sockets function from WS2_32.dll. /MANIFEST:NO also keeps CMake from embedding one.
target_link_options(Patella PRIVATE
	/INCREMENTAL:NO /NODEFAULTLIB:wsock32.lib /DELAYLOAD:oleacc.dll /MANIFEST:NO /DEBUG
	$<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
	/TLBID:1 /DYNAMICBASE /NXCOMPAT /MACHINE:X86 /SAFESEH)
# The resources against Tibia.exe's after every link, as tools\build-patella.cmd checked them: a
# difference fails the build. A step after the link, so the image does not change.
add_custom_command(TARGET Patella POST_BUILD
	COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/compare_resources.py"
		"${PATELLA_TIBIA_EXE_NATIVE}" "$<TARGET_FILE:Patella>"
	VERBATIM)

# ---------------------------------------------------------------------------------------------------
# mfc_shim_check: every source of Patella.exe compiled, not linked, against src/compat/mfc_shim.h
# instead of MFC, with the options of the former tools\compile-check.cmd when TIBIA_MFC_SHIM was set
# (the same in every configuration, for the C files too, as cl got them):
#	/std:c++17 /EHsc /MT /O2 /W3 /permissive- /Zc:strictStrings- /D_WIN32_WINNT=0x0601 /DTIBIA_MFC_SHIM
# Patella.exe is the check against real MFC. One source alone: cmake --build --preset oracle-x86
# --target CMakeFiles/mfc_shim_check.dir/Release/src/<File>.cpp.obj (or Patella.dir for real MFC).

add_library(mfc_shim_check OBJECT ${patella_cpp_sources} ${patella_c_sources})
set_target_properties(mfc_shim_check PROPERTIES
	MSVC_RUNTIME_LIBRARY MultiThreaded
	MSVC_DEBUG_INFORMATION_FORMAT "")
target_include_directories(mfc_shim_check PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(mfc_shim_check PRIVATE _WIN32_WINNT=0x0601 TIBIA_MFC_SHIM)
target_compile_options(mfc_shim_check PRIVATE /std:c++17 /EHsc /O2 /W3 /permissive- /Zc:strictStrings-)

# ---------------------------------------------------------------------------------------------------
# The tests of the former tools\difftest\build.cmd. Its flags in every configuration:
#	/std:c++17 /MT /O2 /W3 /DTIBIA_MFC_SHIM /D_WIN32_WINNT=0x0601 /I src, and /EHa or /EHsc,
# for the C file too, as cl gets them there; cl links with only the libraries and options given.

function(oracle_test_target target eh)
	set_target_properties(${target} PROPERTIES
		MSVC_RUNTIME_LIBRARY MultiThreaded
		MSVC_DEBUG_INFORMATION_FORMAT "")
	target_include_directories(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:${PROJECT_SOURCE_DIR}/src>")
	target_compile_definitions(${target} PRIVATE
		$<$<COMPILE_LANGUAGE:C,CXX>:TIBIA_MFC_SHIM>
		$<$<COMPILE_LANGUAGE:C,CXX>:_WIN32_WINNT=0x0601>)
	target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/std:c++17 /O2 /W3 ${eh}>)
	# The programs cl links have no manifest; CMake would embed one.
	target_link_options(${target} PRIVATE /MANIFEST:NO)
endfunction()

# The golden corpus's recorder (tests/golden/golden.h, issue #24) in difftest, sysinctest and
# communicationtest: the ported tests of tests/golden/*_tests.cpp see the original (GOLDEN_ORACLE).
function(oracle_golden_target target)
	target_sources(${target} PRIVATE "${GOLDEN_DIR}/golden.cpp")
	target_compile_definitions(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:GOLDEN_ORACLE=1>)
endfunction()

# Each test gets a private TEMP, as build.cmd gave its run one: several tests write fixed
# directories there.
function(oracle_test_environment test)
	file(MAKE_DIRECTORY "${ORACLE_TMP_DIR}/${test}")
	file(TO_NATIVE_PATH "${ORACLE_TMP_DIR}/${test}" tmp)
	set_tests_properties(${test} PROPERTIES ENVIRONMENT "TEMP=${tmp};TMP=${tmp}")
endfunction()

# The base layer's sources in difftest.exe and communicationtest.exe, in the former build.cmd's order.
set(difftest_game_sources
	Script.cpp Talk.cpp Objects.cpp)
list(TRANSFORM difftest_game_sources PREPEND "${PROJECT_SOURCE_DIR}/src/")
set(difftest_window_sources
	Surface.cpp SurfaceDX.cpp surfaceDX9.cpp SurfaceGL.cpp Help.cpp Inventory.cpp Creatures.cpp Map.cpp
	graphics.cpp GUI.cpp DialogWindow.cpp ContextMenus.cpp ConsoleWindow.cpp ContentWindows.cpp
	DialogElements.cpp Dialogs.cpp MapWindow.cpp MainWindow.cpp Extern/regexp/regexp.c)
list(TRANSFORM difftest_window_sources PREPEND "${PROJECT_SOURCE_DIR}/src/")

# difftest.exe: the reconstructed functions against the original machine code. imageeh.cpp's
# exception handler trampoline must be in the SafeSEH table: only the assembler can put it there.
set(difftest_tests
	difftest.cpp playertest.cpp
	consolewindowtest.cpp contentwindowstest.cpp
	dialogelementstest.cpp dialogstest.cpp surfacedx9test.cpp surfacegltest.cpp
	surfacedxtest.cpp mapwindowtest.cpp mainwindowtest.cpp errortest.cpp
	imageeh.cpp imageehseh.asm)
list(TRANSFORM difftest_tests PREPEND "${DIFFTEST_DIR}/")
# The first tests (XTEA ... Buffers, Skins) and Regexp are golden-corpus tests (issue #24), and so are
# those of issue #26 (Status ...).
list(APPEND difftest_tests "${GOLDEN_DIR}/core_tests.cpp" "${GOLDEN_DIR}/regexp_tests.cpp"
	"${GOLDEN_DIR}/status_tests.cpp" "${GOLDEN_DIR}/preferences_tests.cpp" "${GOLDEN_DIR}/script_tests.cpp"
	"${GOLDEN_DIR}/player_tests.cpp" "${GOLDEN_DIR}/help_tests.cpp" "${GOLDEN_DIR}/inventory_tests.cpp"
	"${GOLDEN_DIR}/talk_tests.cpp" "${GOLDEN_DIR}/container_tests.cpp" "${GOLDEN_DIR}/objects_tests.cpp"
	"${GOLDEN_DIR}/map_tests.cpp" "${GOLDEN_DIR}/creatures_tests.cpp")
# Issue #54: the error rounds' comparison (golden_eh.cpp, imageeh.cpp's before) and MainFrm.
list(APPEND difftest_tests "${GOLDEN_DIR}/golden_eh.cpp" "${GOLDEN_DIR}/mainfrm_tests.cpp")
# Issue #30: Network (it includes Network.cpp).
list(APPEND difftest_tests "${GOLDEN_DIR}/network_tests.cpp")
# Issue #28: Graphics and GUI.
list(APPEND difftest_tests "${GOLDEN_DIR}/graphics_tests.cpp" "${GOLDEN_DIR}/gui_tests.cpp")
# Issue #28 (second part): the shared element fakes, DialogWindow and ContextMenus.
list(APPEND difftest_tests "${GOLDEN_DIR}/elementfakes.cpp" "${GOLDEN_DIR}/dialogwindow_tests.cpp"
	"${GOLDEN_DIR}/contextmenus_tests.cpp")
set_source_files_properties("${DIFFTEST_DIR}/imageehseh.asm" PROPERTIES COMPILE_OPTIONS /safeseh)
add_executable(difftest ${difftest_tests}
	"${PROJECT_SOURCE_DIR}/src/Utils.cpp" "${PROJECT_SOURCE_DIR}/src/TXTEA.cpp"
	"${PROJECT_SOURCE_DIR}/src/TMyException.cpp" "${PROJECT_SOURCE_DIR}/src/Skins.cpp"
	"${PROJECT_SOURCE_DIR}/src/Status.cpp" "${PROJECT_SOURCE_DIR}/src/Preferences.cpp"
	${difftest_game_sources} "${PROJECT_SOURCE_DIR}/src/Player.cpp" ${difftest_window_sources})
oracle_test_target(difftest /EHa)
oracle_golden_target(difftest)
target_link_libraries(difftest PRIVATE ddraw dxguid winmm user32)
target_link_options(difftest PRIVATE /BASE:0x20000000 /DYNAMICBASE:NO /FIXED)

# sysinctest.exe: sysinc.cpp on its own (difftest.exe replaces some of its functions with recorders); its
# test is a golden-corpus test (issue #24).
add_executable(sysinctest "${DIFFTEST_DIR}/difftest.cpp" "${GOLDEN_DIR}/sysinc_tests.cpp"
	"${PROJECT_SOURCE_DIR}/src/Utils.cpp" "${PROJECT_SOURCE_DIR}/src/TXTEA.cpp"
	"${PROJECT_SOURCE_DIR}/src/TMyException.cpp" "${PROJECT_SOURCE_DIR}/src/sysinc.cpp")
oracle_test_target(sysinctest /EHa)
oracle_golden_target(sysinctest)
target_compile_definitions(sysinctest PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:DIFFTEST_SYSINC>)
target_link_libraries(sysinctest PRIVATE user32)
target_link_options(sysinctest PRIVATE /BASE:0x20000000 /DYNAMICBASE:NO /FIXED)

# communicationtest.exe: Communication.cpp and Control.cpp with every callee faked (issue #36).
add_executable(communicationtest
	"${DIFFTEST_DIR}/difftest.cpp" "${DIFFTEST_DIR}/communicationtest.cpp" "${DIFFTEST_DIR}/controltest.cpp"
	"${DIFFTEST_DIR}/commworld.cpp" "${DIFFTEST_DIR}/imageeh.cpp" "${DIFFTEST_DIR}/imageehseh.asm"
	"${GOLDEN_DIR}/golden_eh.cpp"
	"${PROJECT_SOURCE_DIR}/src/Communication.cpp" "${PROJECT_SOURCE_DIR}/src/Control.cpp"
	"${PROJECT_SOURCE_DIR}/src/Network.cpp" "${PROJECT_SOURCE_DIR}/src/sysinc.cpp"
	"${PROJECT_SOURCE_DIR}/src/TRSA.cpp" "${PROJECT_SOURCE_DIR}/src/vlong.cpp"
	"${PROJECT_SOURCE_DIR}/src/Utils.cpp" "${PROJECT_SOURCE_DIR}/src/TXTEA.cpp"
	"${PROJECT_SOURCE_DIR}/src/TMyException.cpp" "${PROJECT_SOURCE_DIR}/src/Skins.cpp"
	"${PROJECT_SOURCE_DIR}/src/Status.cpp" "${PROJECT_SOURCE_DIR}/src/Preferences.cpp"
	${difftest_game_sources} ${difftest_window_sources})
oracle_test_target(communicationtest /EHa)
oracle_golden_target(communicationtest)
target_compile_definitions(communicationtest PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:DIFFTEST_COMMUNICATION>)
target_link_libraries(communicationtest PRIVATE
	ddraw dxguid winmm user32 ws2_32 gdi32 opengl32 glu32 shell32)
target_link_options(communicationtest PRIVATE /BASE:0x20000000 /DYNAMICBASE:NO /FIXED /OPT:NOICF)

# vlongtest.exe: vlong and TRSA against Python's exact integers (vlongcheck.py).
add_executable(vlongtest "${DIFFTEST_DIR}/vlongtest.cpp" "${PROJECT_SOURCE_DIR}/src/vlong.cpp"
	"${PROJECT_SOURCE_DIR}/src/TRSA.cpp" "${PROJECT_SOURCE_DIR}/src/TMyException.cpp")
oracle_test_target(vlongtest /EHsc)

# The capture DLL of the rendering comparison, 32-bit (cmake/Capture.cmake): tools/compare_rendering.py
# builds this target when build\oracle-x86\Release\patcap.dll is missing or older than patcap.c, and
# copies it into its run folders.
include("${PROJECT_SOURCE_DIR}/cmake/Capture.cmake")

enable_testing()

add_test(NAME difftest COMMAND difftest "${PATELLA_TIBIA_EXE_NATIVE}")
oracle_test_environment(difftest)
add_test(NAME sysinctest COMMAND sysinctest "${PATELLA_TIBIA_EXE_NATIVE}")
oracle_test_environment(sysinctest)
add_test(NAME communicationtest COMMAND communicationtest "${PATELLA_TIBIA_EXE_NATIVE}")
oracle_test_environment(communicationtest)
add_test(NAME vlongtest COMMAND "${CMAKE_COMMAND}"
	"-DVLONGTEST=$<TARGET_FILE:vlongtest>"
	"-DPYTHON=${Python3_EXECUTABLE}"
	"-DCHECK=${DIFFTEST_DIR}/vlongcheck.py"
	"-DOUTPUT=${ORACLE_TMP_DIR}/vlongtest/vlong.txt"
	-P "${PROJECT_SOURCE_DIR}/cmake/RunVlongTest.cmake")
oracle_test_environment(vlongtest)
# The full difftest runs for a long time; DIFFTEST_TIMEOUT (default 1800 s) stops a hung child.
set_tests_properties(difftest communicationtest PROPERTIES TIMEOUT 7200)

# The golden corpus (tests/golden/golden.h, issue #24): cmake --build --preset oracle-x86 --target
# golden_corpus runs difftest, sysinctest and communicationtest in full with DIFFTEST_RECORD set, each
# with a private TEMP as their CTest tests. The corpus is derived from Tibia.exe: it stays on this
# machine, in PATELLA_GOLDEN_DIR (default build/golden, which .gitignore keeps out of git), and is
# never committed or uploaded. The windows-x64 preset's golden_runner replays it.
set(PATELLA_GOLDEN_DIR "${PROJECT_SOURCE_DIR}/build/golden" CACHE PATH
	"Where golden_corpus records the golden corpus (tests/golden; local, never committed)")
set(golden_commands)
foreach(test difftest sysinctest communicationtest)
	file(MAKE_DIRECTORY "${ORACLE_TMP_DIR}/golden-${test}")
	file(TO_NATIVE_PATH "${ORACLE_TMP_DIR}/golden-${test}" tmp)
	list(APPEND golden_commands COMMAND "${CMAKE_COMMAND}" -E env "DIFFTEST_RECORD=${PATELLA_GOLDEN_DIR}"
		"TEMP=${tmp}" "TMP=${tmp}" "$<TARGET_FILE:${test}>" "${PATELLA_TIBIA_EXE_NATIVE}")
endforeach()
add_custom_target(golden_corpus ${golden_commands}
	DEPENDS difftest sysinctest communicationtest
	COMMENT "Recording the golden corpus into ${PATELLA_GOLDEN_DIR}"
	USES_TERMINAL
	VERBATIM)
