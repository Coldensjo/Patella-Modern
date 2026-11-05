# patella_core (issue #7; notes/modernization-plan.md, sections 5 and 7, M1): the static library of
# the portable code, built by every product preset (windows-x64, windows-x64-gcc, linux-x64-gcc,
# linux-x64-clang) and tested by tests_core (tests/unit). It starts with the sources that need no
# platform header and grows as files lose their Windows dependencies:
#
#	vlong.cpp, TRSA.cpp   big numbers and the login packet's RSA
#	TXTEA.cpp             the session's XTEA cipher
#	TMyException.cpp      the exception type of the TASSERT/TERROR/TEND macros (common.h)
#	Utils.cpp             the Latin-1 string helpers, outfit colours and packet buffers
#	Skins.cpp             the rectangles of the GUI graphics in Tibia.pic
#	settings/Settings.cpp Patella.ini, the fork's own settings file (issue #31, plan D12)
#	log/LogSettings.cpp   the developer log's keys in Patella.ini (issue #77, plan D16; the log itself,
#	                      log/Log.h, is header only)
#	Extern/regexp         Henry Spencer's regexp (K&R C, compiled as C17)
#	Container.h           the container templates (header only)
#
# PATELLA_CORE (common.h) leaves out MFC and the Windows headers, so no core source can use them
# (plan section 8). The client (cmake/WindowsX64.cmake) and the oracle still compile these sources
# themselves, with MFC; the library is not linked into them yet.

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

set(patella_core_sources
	src/vlong.cpp
	src/TRSA.cpp
	src/TXTEA.cpp
	src/TMyException.cpp
	src/Utils.cpp
	src/Skins.cpp
	src/settings/Settings.cpp
	src/log/LogSettings.cpp
	src/Extern/regexp/regexp.c)
list(TRANSFORM patella_core_sources PREPEND "${PROJECT_SOURCE_DIR}/")

add_library(patella_core STATIC ${patella_core_sources})
target_include_directories(patella_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(patella_core PUBLIC PATELLA_CORE)
patella_target_language(patella_core)
patella_target_warnings(patella_core)
patella_target_hardening(patella_core)

# Static analysis of the core library (issue #63; notes/modernization-plan.md, section 7, M4): MSVC's code
# analysis (/analyze; not the headers in angle brackets, /analyze:external-) and GCC's static analyzer
# (-fanalyzer) compile every source of patella_core, so the build fails on a finding (/WX, -Werror) like on a
# warning: the hosted CI's windows-x64 job (MSVC) and linux-x64-gcc job run them, Release and Debug, and so
# does every local build of those presets and of windows-x64-gcc. Clang has none here (clang-tidy's
# bugprone-* is the clang-tidy job's, .clang-tidy). The findings that stay are suppressed in the code with
# their reasons, listed in notes/decisions.md ("Static analysis of the core library"). The code analysis
# does not change the objects; the client and the oracle compile these sources themselves, without it.
# PATELLA_STATIC_ANALYSIS=OFF leaves it out (a quicker local build).
option(PATELLA_STATIC_ANALYSIS "MSVC /analyze and GCC -fanalyzer on patella_core (issue #63)" ON)
if(PATELLA_STATIC_ANALYSIS)
	if(MSVC)
		target_compile_options(patella_core PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/analyze /analyze:external->)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(patella_core PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-fanalyzer>)
	endif()
endif()
