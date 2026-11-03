# The client of the preset windows-x64 (issue #5; notes/modernization-plan.md, section 7, M1): today's
# code as a 64-bit Patella.exe with MSVC x64, static x64 MFC and the original engines (DirectDraw,
# Direct3D 9, OpenGL).
#
# - The sources, definitions, import libraries and linker options of the oracle's Patella.exe
#   (cmake/OracleX86.cmake), so the program does what that one does: static MFC (nafxcw.lib, pulled
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

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "windows-x64 needs MSVC's x64 compiler: run python -I tools/build.py windows-x64, or "
		"configure from a developer prompt set up with vcvarsall.bat x64 (found ${CMAKE_SIZEOF_VOID_P}-byte "
		"pointers)")
endif()

enable_language(RC)

# The Python on PATH before any registered elsewhere, as for the oracle.
set(Python3_FIND_REGISTRY LAST)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(PATELLA_TIBIA_EXE "${PROJECT_SOURCE_DIR}/Tibia.exe" CACHE FILEPATH
	"The original Tibia.exe: the resource pictures are extracted from it")
if(NOT EXISTS "${PATELLA_TIBIA_EXE}")
	message(FATAL_ERROR "${PATELLA_TIBIA_EXE} is missing: the resource pictures are extracted from it "
		"(notes/resources.md; set PATELLA_TIBIA_EXE to use another copy, or PATELLA_CLIENT=OFF to build only "
		"the unit tests)")
endif()

set(PATELLA_RES_DIR "${PROJECT_BINARY_DIR}/res")
include("${PROJECT_SOURCE_DIR}/cmake/Resources.cmake")
patella_extract_resources("${PATELLA_TIBIA_EXE}" "${PATELLA_RES_DIR}")

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
	$<$<COMPILE_LANGUAGE:C,CXX>:_WIN32_WINNT=0x0601>)
patella_target_language(Patella)
patella_target_warnings(Patella)
patella_target_hardening(Patella)
target_link_libraries(Patella PRIVATE
	glu32 opengl32 ddraw dxguid winmm kernel32 user32 gdi32 winspool advapi32 shell32 comctl32 shlwapi
	oleaut32 ws2_32 delayimp)
target_link_options(Patella PRIVATE
	/NODEFAULTLIB:wsock32.lib /DELAYLOAD:oleacc.dll /MANIFEST:NO /DEBUG
	$<$<CONFIG:Release>:/INCREMENTAL:NO /OPT:REF /OPT:ICF>)
