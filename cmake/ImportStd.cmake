# import std (Patella-Modern issue #113, plan section 7, M7): the standard library as the named module std, for new
# code, where the compiler and CMake support it. CMake's support is experimental (CMAKE_CXX_MODULE_STD and the target
# property CXX_MODULE_STD; notes/decisions.md, "import std (issue #113)"): it needs the gate
# CMAKE_EXPERIMENTAL_CXX_IMPORT_STD, whose value is a UUID that changes with CMake's minor version, set before the
# CXX language is enabled, and a Ninja generator (the presets' Ninja Multi-Config, Ninja 1.11 or newer).
#
#	include(cmake/ImportStd.cmake)          before project(): the option and the gate
#	patella_import_std_check()              after project(): PATELLA_IMPORT_STD_AVAILABLE
#	patella_target_import_std(<target>)     a target whose sources may say import std;
#
# PATELLA_IMPORT_STD is set ON by the presets where it works (CMakePresets.json): windows-x64 and windows-x64-asan
# (MSVC 14.51), windows-x64-gcc (mingw-w64 GCC 16.2), linux-x64-gcc (GCC 16.2; the image's CMake 4.4.5) and since
# issue #121 linux-x64-clang (Clang 22 with GCC 16.2's libstdc++, the image's Clang configuration files; Debian's
# libstdc++ 14 until then, which has no std module; notes/decisions.md).
#
# The gates below are the ones verified. Since issue #141 every build of the repository takes the 4.4 path: CMake 4.4.5
# everywhere (tools/build.py's pinned portable CMake on Windows, local and hosted, and the Linux image). The 4.1 path
# (4.1.2: the builds from issue #113 to #141) stays for a build with another CMake, e.g. a distribution's, as does
# cmake_minimum_required 3.30 (notes/decisions.md, "#141"). On another CMake version the option is turned off with a
# warning at configure, so a newer CMake does not fail the build; the version's gate is then added here (its value:
# `grep -ao "CxxImportStd.\{40\}"` on the cmake executable, or Help/dev/experimental.rst of its sources). On a known
# version a toolchain that has no std module fails the configure: the option was set where it does not work.

include_guard(GLOBAL)

option(PATELLA_IMPORT_STD "import std; in the targets of patella_target_import_std (issue #113; experimental in CMake)" OFF)

if(PATELLA_IMPORT_STD)
	if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.1 AND CMAKE_VERSION VERSION_LESS 4.2)
		set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
	elseif(CMAKE_VERSION VERSION_GREATER_EQUAL 4.4 AND CMAKE_VERSION VERSION_LESS 4.5)
		set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
	else()
		message(WARNING "PATELLA_IMPORT_STD: no known gate of CMake ${CMAKE_VERSION} for import std "
			"(cmake/ImportStd.cmake knows 4.1 and 4.4); import std is off")
		set(PATELLA_IMPORT_STD OFF CACHE BOOL "" FORCE)
	endif()
endif()

# After project(): PATELLA_IMPORT_STD_AVAILABLE is ON when the option is on and the toolchain has a std module for the
# standard the targets compile with (MSVC: 23, /std:c++latest; GCC and Clang: 26, cmake/Warnings.cmake).
macro(patella_import_std_check)
	set(PATELLA_IMPORT_STD_AVAILABLE OFF)
	if(PATELLA_IMPORT_STD)
		if(MSVC)
			set(PATELLA_IMPORT_STD_LEVEL 23)
		else()
			set(PATELLA_IMPORT_STD_LEVEL 26)
		endif()
		if(PATELLA_IMPORT_STD_LEVEL IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
			set(PATELLA_IMPORT_STD_AVAILABLE ON)
			message(STATUS "import std: on (C++${PATELLA_IMPORT_STD_LEVEL}, CMake ${CMAKE_VERSION}, "
				"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
		else()
			message(FATAL_ERROR "PATELLA_IMPORT_STD: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} has no std "
				"module for C++${PATELLA_IMPORT_STD_LEVEL} under CMake ${CMAKE_VERSION}: "
				"${CMAKE_CXX${PATELLA_IMPORT_STD_LEVEL}_COMPILER_IMPORT_STD_NOT_FOUND_MESSAGE} "
				"(set PATELLA_IMPORT_STD=OFF; notes/decisions.md, \"import std (issue #113)\")")
		endif()
	endif()
endmacro()

# A target whose sources may say import std;, called after its patella_target_language, patella_target_warnings and
# patella_target_hardening. CMake adds the std module's target (__CMAKE::CXX<level>: the library's std.ixx or
# bits/std.cc) only to a target with the compile feature cxx_std_<level>, so the target gets it; without extensions,
# so GCC's standard flag is -std=c++26, the one of patella_target_language, not -std=gnu++26 (MSVC: cxx_std_23 is
# /std:c++latest, the same as its flag).
#
# CMake 4.1 compiles the std module with the directory's flags only (CMAKE_CXX_FLAGS_<CONFIG> and the standard), not
# with the importer's options (4.4 compiles its BMI with the importer's; below), so the module and its importer would
# differ in what the options define: MSVC warns C5050 ("Possible incompatible environment while importing module 'std': _CONTROL_FLOW_GUARD=1 is defined in current
# command line and not in module command line", likewise _UTF8 of /utf-8; an error under /WX), and GCC accepts a module
# built without the importer's -D_FORTIFY_SOURCE=3, -D_GLIBCXX_ASSERTIONS, -fstack-protector-strong or the
# sanitizers, which change the library's inline code. So the module gets the target's own options: on MSVC the module
# is a static library of the build (__cmake_cxx23, which the alias names), its COMPILE_OPTIONS; on GCC an imported
# target that CMake compiles for its importers, its INTERFACE_COMPILE_OPTIONS (which the importer gets too, the same
# options again).
#
# Clang with glibc's _FORTIFY_SOURCE (Hardening.cmake's -D_FORTIFY_SOURCE=3 in optimised builds): glibc 2.41 declares
# its checked printf family for Clang as static overloads (__fortify_clang_overload_arg), which the std module cannot
# export ("using declaration referring to 'printf' with internal linkage cannot be exported", bits/std.cc; GCC's
# wrappers are extern inline and export), so the target and its module are compiled without _FORTIFY_SOURCE (issue
# #121: a -U_FORTIFY_SOURCE after the hardening options; since issue #141 the hardening's item is taken out of the
# target's options, below). The target says only import std;, no header, so it has no call fortify would check. The
# warning level is a source property (patella_target_warnings), so the module keeps the compiler's default level; its headers are the library's own. One target per build uses it (tests_import_std), so the
# options are that target's.
function(patella_target_import_std target)
	if(NOT PATELLA_IMPORT_STD_AVAILABLE)
		message(FATAL_ERROR "patella_target_import_std(${target}): import std is not available (PATELLA_IMPORT_STD)")
	endif()
	get_property(user GLOBAL PROPERTY PATELLA_IMPORT_STD_TARGET)
	if(user)
		message(FATAL_ERROR "patella_target_import_std(${target}): the std module has the options of ${user} already; "
			"a second target needs the same options (cmake/ImportStd.cmake)")
	endif()
	set_property(GLOBAL PROPERTY PATELLA_IMPORT_STD_TARGET ${target})
	target_compile_features(${target} PRIVATE cxx_std_${PATELLA_IMPORT_STD_LEVEL})
	set_target_properties(${target} PROPERTIES CXX_MODULE_STD ON CXX_EXTENSIONS OFF)

	# CMake 4.4 creates the std module's target at generate time (@cmake_cxx_std) and compiles its BMI for each
	# importer with the importer's own options (@cmake_cxx_std@synth_<n>), so there is nothing to give it; 4.1
	# creates __CMAKE::CXX<level> at project() and compiles it with the directory's flags only.
	#
	# Clang (issue #141): the target's options lose Hardening.cmake's _FORTIFY_SOURCE item (-U_FORTIFY_SOURCE
	# -D_FORTIFY_SOURCE=3 in optimised configurations) instead of getting a second -U_FORTIFY_SOURCE after it (#121):
	# CMake 4.4 de-duplicates the importer's options for the synthesized BMI and keeps the first -U only, so the BMI
	# was compiled with -D_FORTIFY_SOURCE=3 and failed. And the directory gets -Wno-reserved-module-identifier, which
	# 4.1 gave its own module target (Clang-CXX-CXXImportStd.cmake) and 4.4 does not: 4.4 also compiles bits/std.cc
	# into @cmake_cxx_std's objects with the directory's flags only, and Clang warns "'std' is a reserved name for a
	# module" (build.py fails any warning). The directory is the caller's (tests/unit), whose only module is std.
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT MSVC)
		get_target_property(options ${target} COMPILE_OPTIONS)
		if(options)
			list(FILTER options EXCLUDE REGEX "_FORTIFY_SOURCE")
			set_property(TARGET ${target} PROPERTY COMPILE_OPTIONS ${options})
		endif()
		if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.4)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reserved-module-identifier" PARENT_SCOPE)
		endif()
	endif()

	set(module __CMAKE::CXX${PATELLA_IMPORT_STD_LEVEL})
	get_target_property(options ${target} COMPILE_OPTIONS)
	if(NOT options OR NOT TARGET ${module})
		return()
	endif()
	get_target_property(aliased ${module} ALIASED_TARGET)
	if(aliased)
		set_property(TARGET ${aliased} APPEND PROPERTY COMPILE_OPTIONS ${options})
	else()
		set_property(TARGET ${module} APPEND PROPERTY INTERFACE_COMPILE_OPTIONS ${options})
	endif()
endfunction()
