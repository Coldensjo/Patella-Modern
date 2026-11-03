# Patella-Modern: the build (notes/modernization-plan.md, section 5).
#
# One workflow preset per platform configures, builds and tests (CMakePresets.json):
#	cmake --workflow --preset oracle-x86
#	cmake --workflow --preset windows-x64
#	cmake --workflow --preset windows-x64-gcc
# oracle-x86 (issue #4): today's 32-bit Patella.exe with exactly the flags of Patella.vcxproj, and the
# differential tests of tools\difftest\build.cmd as CTest tests; a test instrument until M6.
# windows-x64 (MSVC) and windows-x64-gcc (mingw-w64 GCC 16, plan D6) build only the unit tests of
# tests/unit so far (issue #9); the client comes with issue #5, the Linux presets with issue #7. Their
# targets take the language, warning and hardening settings of cmake/Warnings.cmake and
# cmake/Hardening.cmake (issue #8), which the oracle never uses.

cmake_minimum_required(VERSION 3.30)

project(Patella
	DESCRIPTION "Patella-Modern, the modernized reconstruction of the Tibia 7.72 client"
	LANGUAGES C CXX)

option(PATELLA_ORACLE "Build the oracle: today's 32-bit Patella.exe and the difftest (preset oracle-x86)" OFF)

if(PATELLA_ORACLE)
	include(cmake/OracleX86.cmake)
else()
	enable_testing()
	add_subdirectory(tests/unit)
endif()
