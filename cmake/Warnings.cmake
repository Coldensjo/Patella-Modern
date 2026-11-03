# The language and warning settings of the product targets (notes/modernization-plan.md, section 5;
# issue #8), in one place. The presets windows-x64 (issue #5) and linux-x64-gcc / linux-x64-clang
# (issue #7) apply them to their targets:
#
#	include(cmake/Warnings.cmake)
#	patella_target_language(<target>)   C++23 and the conformance switches
#	patella_target_warnings(<target>)   the warning level, file by file, and warnings as errors
#
# Never for the oracle-x86 targets: cmake/OracleX86.cmake keeps exactly the flags of the former
# Patella.vcxproj and tools\difftest\build.cmd (/std:c++17 /W3 /Zc:strictStrings- ...) until M6.

# DIRECTORY, not GLOBAL: the variables below are directory-scoped, and a directory that includes the
# file after another one did (the client after tests/unit) must get them too.
include_guard(DIRECTORY)

# The repository root: the baseline below names its files relative to it.
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH PATELLA_ROOT_DIR)

# The warning baseline: the sources that do not build warning-free at the full level yet. They build
# one level lower (MSVC /W3; GCC and Clang -Wall), still with warnings as errors, so the build never
# stops being green. A file leaves the list when it builds clean at the full level, and the list only
# shrinks; no file is added. Measured with MSVC 14.51 (cl 19.51) for x86 against static MFC
# (notes/decisions.md, "Compiler settings, warnings and code style (issue #8)") and again for x64 (the
# same 24 files and 1,234 warnings once the x64 conversions were fixed; "Windows x64 build (issue #5)");
# issue #7 measures it for GCC and Clang. Paths are relative to the repository root.
set(PATELLA_WARNING_BASELINE
	src/Communication.cpp
	src/ConsoleWindow.cpp
	src/ContentWindows.cpp
	src/ContextMenus.cpp
	src/Control.cpp
	src/Creatures.cpp
	src/DialogElements.cpp
	src/DialogWindow.cpp
	src/Dialogs.cpp
	src/Extern/regexp/regexp.c
	src/GUI.cpp
	src/MainFrm.cpp
	src/MainWindow.cpp
	src/MapWindow.cpp
	src/Player.cpp
	src/Preferences.cpp
	src/Status.cpp
	src/Surface.cpp
	src/SurfaceDX.cpp
	src/SurfaceGL.cpp
	src/TXTEA.cpp
	src/Utils.cpp
	src/surfaceDX9.cpp
	src/vlong.cpp
)

# C++23 with the conformance switches. MSVC has no /std:c++23 yet, so /std:c++latest; GCC and Clang
# compile as C++26, whose features the code reaches only through src/compat/cxx26.h (issue #9). The
# targets do not set CXX_STANDARD, which would add a second standard flag.
function(patella_target_language target)
	if(MSVC)
		target_compile_options(${target} PRIVATE
			$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest /permissive- /Zc:__cplusplus>
			$<$<COMPILE_LANGUAGE:C,CXX>:/utf-8 /Zc:preprocessor /Zc:inline>)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++26>)
		# GCC 16 keeps the default contract violation handler, which contract_assert calls
		# (src/compat/cxx26.h), in libstdc++exp.
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
			target_link_libraries(${target} PRIVATE $<$<LINK_LANGUAGE:CXX>:stdc++exp>)
		endif()
		# mingw-w64: the compiler's runtime DLLs are copied next to the executable, where Windows
		# looks first, so it never loads another GCC's libstdc++-6.dll that comes earlier on PATH (a
		# loader error dialog at start). Linking them in (-static) is not possible with GCC 16.2: in
		# C++26 an unoptimised object defines std::logic_error's copy constructor, which the static
		# libstdc++.a defines too (multiple definition).
		get_target_property(type ${target} TYPE)
		if(MINGW AND type STREQUAL "EXECUTABLE")
			cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compiler_dir)
			set(runtime "")
			foreach(dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
				if(EXISTS "${compiler_dir}/${dll}")
					list(APPEND runtime "${compiler_dir}/${dll}")
				endif()
			endforeach()
			if(runtime)
				add_custom_command(TARGET ${target} POST_BUILD
					COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime} "$<TARGET_FILE_DIR:${target}>"
					VERBATIM)
			endif()
		endif()
	else()
		message(FATAL_ERROR "patella_target_language: no settings for ${CMAKE_CXX_COMPILER_ID}")
	endif()
endfunction()

# The full warning level for every source of the target, the baseline level for the files on
# PATELLA_WARNING_BASELINE, and warnings as errors (COMPILE_WARNING_AS_ERROR: /WX or -Werror;
# `cmake --compile-no-warning-as-error` turns it off to measure). The level is set per source, so no
# command line has two levels. Headers included with angle brackets (Windows SDK, MFC, the standard
# library) are external to MSVC and give no warnings (/external:anglebrackets /external:W0).
function(patella_target_warnings target)
	if(MSVC)
		set(full /W4)
		set(baseline /W3)
		target_compile_options(${target} PRIVATE
			$<$<COMPILE_LANGUAGE:C,CXX>:/external:anglebrackets /external:W0>)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		set(full -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
		set(baseline -Wall)
	else()
		message(FATAL_ERROR "patella_target_warnings: no settings for ${CMAKE_CXX_COMPILER_ID}")
	endif()
	set(baseline_files "")
	foreach(file IN LISTS PATELLA_WARNING_BASELINE)
		cmake_path(ABSOLUTE_PATH file BASE_DIRECTORY "${PATELLA_ROOT_DIR}" NORMALIZE OUTPUT_VARIABLE path)
		list(APPEND baseline_files "${path}")
	endforeach()
	get_target_property(sources ${target} SOURCES)
	get_target_property(source_dir ${target} SOURCE_DIR)
	foreach(source IN LISTS sources)
		cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${source_dir}" NORMALIZE OUTPUT_VARIABLE path)
		cmake_path(GET path EXTENSION LAST_ONLY extension)
		string(TOLOWER "${extension}" extension)
		if(NOT extension MATCHES "^\\.(c|cc|cpp|cxx)$")
			continue()
		endif()
		if(path IN_LIST baseline_files)
			set(level ${baseline})
		else()
			set(level ${full})
		endif()
		# A source property belongs to the directory, not the target: a source that several targets
		# share (the unit tests' two builds) gets its level once.
		get_property(options SOURCE "${source}" TARGET_DIRECTORY ${target} PROPERTY COMPILE_OPTIONS)
		list(GET level 0 first)
		if(first IN_LIST options)
			continue()
		endif()
		set_property(SOURCE "${source}" TARGET_DIRECTORY ${target} APPEND PROPERTY COMPILE_OPTIONS ${level})
	endforeach()
	set_property(TARGET ${target} PROPERTY COMPILE_WARNING_AS_ERROR ON)
endfunction()
