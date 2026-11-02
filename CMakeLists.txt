# Patella-Modern: the build (notes/modernization-plan.md, section 5).
#
# One workflow preset per platform configures, builds and tests (CMakePresets.json):
#	cmake --workflow --preset oracle-x86
# So far only oracle-x86 exists (issue #4): today's 32-bit Patella.exe with exactly the flags of
# Patella.vcxproj, and the differential tests of tools\difftest\build.cmd as CTest tests. It is a test
# instrument until M6. The x64 and Linux presets come with issues #5 and #7; their targets take the
# language, warning and hardening settings of cmake/Warnings.cmake and cmake/Hardening.cmake (issue #8),
# which the oracle never uses.

cmake_minimum_required(VERSION 3.30)

project(Patella
	DESCRIPTION "Patella-Modern, the modernized reconstruction of the Tibia 7.72 client"
	LANGUAGES C CXX)

option(PATELLA_ORACLE "Build the oracle: today's 32-bit Patella.exe and the difftest (preset oracle-x86)" OFF)

if(PATELLA_ORACLE)
	include(cmake/OracleX86.cmake)
else()
	message(FATAL_ERROR "Only the oracle-x86 preset exists so far: cmake --workflow --preset oracle-x86")
endif()
