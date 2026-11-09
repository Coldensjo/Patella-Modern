# Reproducible builds (Patella-Modern issue #136, plan M9; notes/decisions.md, "#136"): two Release builds of one commit, in
# different folders, give byte-identical executables. tools/repro_check.py builds twice and compares; this file holds what
# that took, each item measured against a build without it:
#
#	include(cmake/Reproducible.cmake)      in the root CMakeLists.txt, before the first target (FetchContent's included)
#
# - MSVC: /Brepro for the compiler and the linker (the time stamps of the objects, of the PE header and of the resources'
#   conversion; the PDB's GUID), /d1trimfile, undocumented (the absolute source folder off __FILE__ and std::source_location,
#   except in a source compiled with a precompiled header, so cmake/Sdl3.cmake turns SDL3's off; and off the
#   PDB's source names, which become relative to the repository root), /PDBALTPATH:%_PDB% (the PDB's file name instead of its
#   absolute path in the PE's debug directory). Not the documented /experimental:deterministic with /pathmap: measured
#   with MSVC 14.51, /pathmap makes the link lose the objects' debug information without a warning (WinMain 76 bytes with its
#   line numbers becomes a public symbol with neither).
# - GCC and Clang: -ffile-prefix-map (the same paths, and those of debug information). Their linkers' build ids are hashes of
#   the output already.
# - Clang with AddressSanitizer (linux-x64-clang): cmake/RelativeSource.sh as the compiler launcher, every configuration:
#   ASan's module name is the source path as given on the command line, which no prefix map reaches. UBSan's check
#   locations in a header have the header's absolute path, also out of the prefix map's reach:
#   -fsanitize-undefined-strip-path-components=-1 keeps the file's name, with its line, in the report.
#
# Release only otherwise: Debug keeps absolute paths, which debuggers open without being told where the sources are. The source
# folder is removed from the paths (MSVC) or replaced by "." (GCC, Clang), so __FILE__ reads "src\Map.cpp" or "./src/Map.cpp"
# (from the build folder, "../../src/Map.cpp" under the launcher); what shows it to a person takes the file's name alone
# (patella::ErrorOrigin, src/TMyException.cpp). A build folder outside the source folder gets the same treatment; one inside
# it (the presets') is covered by the source folder's.
#
# The hardening and analysis settings are not touched (cmake/Hardening.cmake, cmake/Core.cmake), and the 32-bit capture
# DLL never comes here (root CMakeLists.txt returns before it).

cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${PROJECT_BINARY_DIR}" NORMALIZE patella_binary_dir_inside)
set(patella_repro_folders "${PROJECT_SOURCE_DIR}")
if(NOT patella_binary_dir_inside)
	list(APPEND patella_repro_folders "${PROJECT_BINARY_DIR}")
endif()

# One generator expression per option: a list inside one would be split where a path has a semicolon.
function(patella_repro_compile_option option)
	add_compile_options("$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:${option}>")
endfunction()

if(MSVC)
	patella_repro_compile_option(/Brepro)
	foreach(folder IN LISTS patella_repro_folders)
		# Forward slashes, as MSVC accepts here: a backslash before the list separator would escape it where a list of options
		# is copied (cmake/Sdl3.cmake does).
		patella_repro_compile_option("/d1trimfile:${folder}/")
	endforeach()
	add_link_options("$<$<CONFIG:Release>:/Brepro>" "$<$<CONFIG:Release>:/PDBALTPATH:%_PDB%>")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	foreach(folder IN LISTS patella_repro_folders)
		patella_repro_compile_option("-ffile-prefix-map=${folder}=.")
	endforeach()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND PATELLA_SANITIZE MATCHES "undefined")
		patella_repro_compile_option(-fsanitize-undefined-strip-path-components=-1)
	endif()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND PATELLA_SANITIZE MATCHES "address")
		set(CMAKE_C_COMPILER_LAUNCHER sh "${PROJECT_SOURCE_DIR}/cmake/RelativeSource.sh")
		set(CMAKE_CXX_COMPILER_LAUNCHER sh "${PROJECT_SOURCE_DIR}/cmake/RelativeSource.sh")
	endif()
endif()
