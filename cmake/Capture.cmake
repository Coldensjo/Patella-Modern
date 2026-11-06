# patcap.dll (tools/capture/patcap.c), the capture DLL of the rendering comparison
# (tools/compare_rendering.py; notes/rendering.md), with the options of the former tools\build-capture.cmd:
# /O2 /W4 /WX, the static CRT, no manifest. The DLL is loaded into the client it captures, so it has the
# client's bitness: the oracle-x86 preset (cmake/OracleX86.cmake) builds the 32-bit one for Tibia.exe and the
# 32-bit Patella.exe, build\oracle-x86\Release\patcap.dll. compare_rendering.py builds the target when the DLL
# is missing or older than patcap.c. Tibia.exe's and the oracle's only (Patella-Modern issue #92): the tools
# read the x64 client through its test mode, so the windows-x64 preset no longer builds a 64-bit one (issue
# #18 had added it) and patcap.c refuses to compile for x64.
#
# A test instrument, not a product target: never cmake/Warnings.cmake or cmake/Hardening.cmake.

include_guard(DIRECTORY)

add_library(patcap SHARED "${PROJECT_SOURCE_DIR}/tools/capture/patcap.c")
set_target_properties(patcap PROPERTIES
	MSVC_RUNTIME_LIBRARY MultiThreaded
	MSVC_DEBUG_INFORMATION_FORMAT ""
	DEFINE_SYMBOL "")
# The oracle clears CMake's default flags (cmake/OracleX86.cmake), so it names /O2 in every configuration, as
# build-capture.cmd did.
target_compile_options(patcap PRIVATE /O2 /W4 /WX)
target_link_libraries(patcap PRIVATE user32 gdi32 winmm ddraw opengl32)
target_link_options(patcap PRIVATE /MANIFEST:NO)
