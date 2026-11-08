# The resource pictures (cursors, icon, bitmaps) of the local Tibia.exe, which src/res/Patella.rc
# includes (notes/resources.md): extracted at configure time by tools/extract_resources.py. Used by
# cmake/WindowsX64.cmake:
#
#	include(cmake/Resources.cmake)
#	patella_extract_resources(<Tibia.exe> <output directory>)
#	patella_placeholder_resources(<output directory>)
#
# Tibia.exe and the script are configure dependencies, so a new Tibia.exe configures again. Needs
# Python3_EXECUTABLE (find_package(Python3 ... Interpreter)). The placeholders (issue #127,
# tools/placeholder_resources.py; only for the hosted CI, which has no Tibia.exe): neutral pictures under the same
# file names, generated from nothing but the script.

include_guard(GLOBAL)

function(patella_extract_resources tibia_exe out_dir)
	set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${tibia_exe}" "${PROJECT_SOURCE_DIR}/tools/extract_resources.py")
	execute_process(
		COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/extract_resources.py"
			"${tibia_exe}" "${out_dir}"
		RESULT_VARIABLE extract_result
		OUTPUT_VARIABLE extract_output
		ERROR_VARIABLE extract_output)
	if(NOT extract_result EQUAL 0)
		message(FATAL_ERROR "tools/extract_resources.py failed (${extract_result}):\n${extract_output}")
	endif()
endfunction()

function(patella_placeholder_resources out_dir)
	set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${PROJECT_SOURCE_DIR}/tools/placeholder_resources.py")
	execute_process(
		COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/placeholder_resources.py" "${out_dir}"
		RESULT_VARIABLE placeholder_result
		OUTPUT_VARIABLE placeholder_output
		ERROR_VARIABLE placeholder_output)
	if(NOT placeholder_result EQUAL 0)
		message(FATAL_ERROR "tools/placeholder_resources.py failed (${placeholder_result}):\n${placeholder_output}")
	endif()
endfunction()
