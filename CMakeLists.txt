# Patella-Modern: the build (notes/modernization-plan.md, section 5).
#
# One workflow preset per platform configures, builds and tests (CMakePresets.json):
#	cmake --workflow --preset oracle-x86
#	cmake --workflow --preset windows-x64
#	cmake --workflow --preset windows-x64-gcc
#	cmake --workflow --preset linux-x64-gcc
#	cmake --workflow --preset linux-x64-clang
# The MSVC presets need the compiler set up (vcvarsall.bat); tools/build.py does that and runs the
# workflow from any shell: python -I tools/build.py <preset> (issue #10).
# oracle-x86 (issue #4): today's 32-bit Patella.exe with exactly the flags of the former
# Patella.vcxproj, and the differential tests of the former tools\difftest\build.cmd as CTest tests; a
# test instrument until M6.
# windows-x64 (MSVC x64) builds the unit tests of tests/unit (issue #9) and the client, a 64-bit
# Patella.exe with x64 MFC and the original engines (cmake/WindowsX64.cmake, issue #5), and the 64-bit
# patcap.dll of tools/compare_rendering.py (cmake/Capture.cmake, issue #18);
# windows-x64-gcc (mingw-w64 GCC 16, plan D6) builds patella_core and the unit tests;
# linux-x64-gcc (GCC 16) and linux-x64-clang (Clang, with AddressSanitizer and UBSan) the same on
# Linux (issue #7; on Windows tools/build.py runs them in the Docker image of docker/linux-x64).
# Every product preset builds patella_core (cmake/Core.cmake), the portable code, and patella_testmode
# (cmake/TestMode.cmake), the portable part of the test mode (issue #21). Their targets take
# the language, warning and hardening settings of cmake/Warnings.cmake and cmake/Hardening.cmake
# (issue #8), which the oracle never uses.

cmake_minimum_required(VERSION 3.30)

project(Patella
	DESCRIPTION "Patella-Modern, the modernized reconstruction of the Tibia 7.72 client"
	LANGUAGES C CXX)

option(PATELLA_ORACLE "Build the oracle: today's 32-bit Patella.exe and the difftest (preset oracle-x86)" OFF)
option(PATELLA_CLIENT "Build the client with MSVC x64 (preset windows-x64; MSVC only, the Linux presets set it OFF); needs Tibia.exe for its resources" ON)

if(PATELLA_ORACLE)
	include(cmake/OracleX86.cmake)
else()
	enable_testing()
	include(cmake/Core.cmake)
	include(cmake/TestMode.cmake)
	add_subdirectory(tests/unit)
	if(PATELLA_CLIENT AND MSVC)
		include(cmake/WindowsX64.cmake)
	endif()
	# The 64-bit capture DLL of the rendering comparison (issue #18), with or without the client.
	if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
		include(cmake/Capture.cmake)
	endif()
endif()
