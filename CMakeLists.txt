# Patella-Modern: the build (notes/modernization-plan.md, section 5).
#
# One workflow preset per platform configures, builds and tests (CMakePresets.json):
#	cmake --workflow --preset windows-x64
#	cmake --workflow --preset windows-x64-gcc
#	cmake --workflow --preset linux-x64-gcc
#	cmake --workflow --preset linux-x64-clang
#	cmake --workflow --preset capture-x86
# The MSVC presets need the compiler set up (vcvarsall.bat); tools/build.py does that and runs the
# workflow from any shell: python -I tools/build.py <preset> (issue #10).
# capture-x86 (Patella-Modern issue #94, M6): only the 32-bit capture DLL patcap.dll (cmake/Capture.cmake) for
# Tibia.exe, which the reference recordings' record step and the live comparisons load into it. No client and no
# test programs: the 32-bit oracle (its Patella.exe, the differential tests, the golden corpus's recorder) left the
# fork with issue #94; it is in upstream Patella and at the tag layout-frozen-final.
# windows-x64 (MSVC x64) builds the unit tests of tests/unit (issue #9) and the client, a 64-bit
# Patella.exe on SDL3 and the Vulkan engine (cmake/WindowsX64.cmake, issue #5) and its test build
# PatellaTest.exe; until Patella-Modern issue #92 also a 64-bit patcap.dll (the tools read the x64 client
# through its test mode since then; patcap.dll is Tibia.exe's, cmake/Capture.cmake);
# windows-x64-gcc (mingw-w64 GCC 16, plan D6) builds patella_core and the unit tests;
# linux-x64-gcc (GCC 16) and linux-x64-clang (Clang, with AddressSanitizer and UBSan) the same on
# Linux (issue #7; on Windows tools/build.py runs them in the Docker image of docker/linux-x64), and since issue #131 the
# Linux client, Patella and PatellaTest (cmake/LinuxX64.cmake).
# Every product preset builds patella_core (cmake/Core.cmake), the portable code, and patella_testmode
# (cmake/TestMode.cmake), the portable part of the test mode (issue #21). Their targets take
# the language, warning and hardening settings of cmake/Warnings.cmake and cmake/Hardening.cmake
# (issue #8). Every product preset compiles each header of src/ on its own (cmake/HeaderCheck.cmake, issue #49).

cmake_minimum_required(VERSION 3.30)

# import std (issue #113): the option PATELLA_IMPORT_STD and CMake's experimental gate, which must come before the
# CXX language is enabled (cmake/ImportStd.cmake).
include(cmake/ImportStd.cmake)

project(Patella
	DESCRIPTION "Patella-Modern, the modernized reconstruction of the Tibia 7.72 client"
	LANGUAGES C CXX)

option(PATELLA_CAPTURE "Build only the 32-bit capture DLL patcap.dll for Tibia.exe (preset capture-x86)" OFF)
option(PATELLA_CLIENT "Build the client (MSVC x64 and the Linux presets; windows-x64-gcc has none); needs Tibia.exe for its resources (PATELLA_PLACEHOLDER_RESOURCES=ON: none, the hosted CI)" ON)
option(PATELLA_PACKAGE "The install rules and CPack packages (cmake/Package.cmake, issue #135); refused without PATELLA_PLACEHOLDER_RESOURCES=ON: the presets package-windows-x64 and package-linux-x64" OFF)

if(PATELLA_CAPTURE)
	include(cmake/Capture.cmake)
	return()
endif()

enable_testing()
patella_import_std_check()
include(cmake/Core.cmake)
include(cmake/TestMode.cmake)
add_subdirectory(tests/unit)
add_subdirectory(tests/golden)
# The fuzz targets (issue #132), after the golden objects some of them link.
add_subdirectory(tests/fuzz)
if(PATELLA_CLIENT AND MSVC)
	include(cmake/WindowsX64.cmake)
elseif(PATELLA_CLIENT AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# The Linux client (issue #131, plan M8): SDL3 and the Vulkan engine, the Linux presets' Patella and PatellaTest.
	include(cmake/LinuxX64.cmake)
endif()
if(PATELLA_PACKAGE)
	include(cmake/Package.cmake)
endif()

# Each header of src/ compiled on its own, with the settings of the targets above that use it (issue #49).
include(cmake/HeaderCheck.cmake)
patella_header_checks()
