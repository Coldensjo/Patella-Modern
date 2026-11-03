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
	src/Extern/regexp/regexp.c)
list(TRANSFORM patella_core_sources PREPEND "${PROJECT_SOURCE_DIR}/")

add_library(patella_core STATIC ${patella_core_sources})
target_include_directories(patella_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(patella_core PUBLIC PATELLA_CORE)
patella_target_language(patella_core)
patella_target_warnings(patella_core)
patella_target_hardening(patella_core)
