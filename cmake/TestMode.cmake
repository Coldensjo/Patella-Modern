# patella_testmode (issue #21; notes/modernization-plan.md, 4.4 and section 7, M2): the portable part
# of the test mode, built by every product preset and tested by tests_testmode (tests/unit):
#
#	src/testmode/Clock.cpp        IClock and the virtual clock
#	src/testmode/InputScript.cpp  the input script: parser, key table, scheduler
#	src/testmode/StateDump.cpp    the state dump's JSON (issue #22)
#	src/testmode/PacketLog.cpp    the packet log's lines and connections (issue #22)
#	src/testmode/Png.cpp          the frame dump's PNG pictures (issue #22)
#	src/testmode/ServerReplay.cpp the server replay's file format and schedule, XTEA (issue #23)
#	src/testmode/DisplayModes.cpp the fixed display modes' file format and choice (issue #48)
#
# The test build of the client (PatellaTest.exe, cmake/WindowsX64.cmake) links it with
# src/testmode/TestModeWin32.cpp and ClientStateWin32.cpp, the Windows side. Patella.exe and the
# oracle-x86 build never link it: their hooks (src/TestHooks.h) are the system calls themselves, or
# nothing.

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

add_library(patella_testmode STATIC
	"${PROJECT_SOURCE_DIR}/src/testmode/Clock.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/InputScript.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/StateDump.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/PacketLog.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/Png.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/ServerReplay.cpp"
	"${PROJECT_SOURCE_DIR}/src/testmode/DisplayModes.cpp")
target_include_directories(patella_testmode PUBLIC "${PROJECT_SOURCE_DIR}/src")
patella_target_language(patella_testmode)
patella_target_warnings(patella_testmode)
patella_target_hardening(patella_testmode)
