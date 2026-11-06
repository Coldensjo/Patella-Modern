# patcap.dll (tools/capture/patcap.c), the capture DLL that the tools load into Tibia.exe: frame capture, the
# virtual clock and the main timer's ticks for the reference recordings' record step (tools/reference_record.py)
# and the live comparisons' Tibia.exe runs (tools/compare_rendering.py; notes/rendering.md). The DLL is loaded into
# the client it captures, so it has the client's bitness: 32-bit, for Tibia.exe. The preset capture-x86
# (CMakeLists.txt, PATELLA_CAPTURE; Patella-Modern issue #94) builds it and nothing else, into
# build\capture-x86\Release\patcap.dll; compare_rendering.py builds the target when the DLL is missing or older than
# patcap.c. The x64 client is read through its test mode (Patella-Modern issue #92): patcap.c refuses to compile
# for x64.
#
# The options of the former tools\build-capture.cmd: /O2 /W4 /WX, the static CRT, no manifest. Until issue #94 the
# 32-bit oracle's preset built it with every CMake default flag cleared and /O2 named; here CMake's defaults stay (/DWIN32
# /D_WINDOWS, the Release configuration's /O2 /Ob2 /DNDEBUG; capture-x86 builds Release only), with /W4 /WX after
# them.
#
# A test instrument, not a product target: never cmake/Warnings.cmake or cmake/Hardening.cmake.

include_guard(DIRECTORY)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "capture-x86 needs MSVC's x86 compiler (cl.exe for x86): run python -I tools/build.py "
		"capture-x86, or configure from a developer prompt set up with vcvarsall.bat x86 (found "
		"${CMAKE_C_COMPILER_ID}, ${CMAKE_SIZEOF_VOID_P}-byte pointers)")
endif()

add_library(patcap SHARED "${PROJECT_SOURCE_DIR}/tools/capture/patcap.c")
set_target_properties(patcap PROPERTIES
	MSVC_RUNTIME_LIBRARY MultiThreaded
	MSVC_DEBUG_INFORMATION_FORMAT ""
	DEFINE_SYMBOL "")
target_compile_options(patcap PRIVATE /W4 /WX)
target_link_libraries(patcap PRIVATE user32 gdi32 winmm ddraw opengl32)
target_link_options(patcap PRIVATE /MANIFEST:NO)
