# The resource pictures (cursors, icon, bitmaps) of the local Tibia.exe, which src/res/Patella.rc
# includes (notes/resources.md): extracted at configure time by tools/extract_resources.py. Used by
# cmake/OracleX86.cmake and cmake/WindowsX64.cmake:
#
#	include(cmake/Resources.cmake)
#	patella_extract_resources(<Tibia.exe> <output directory>)
#
# Tibia.exe and the script are configure dependencies, so a new Tibia.exe configures again. Needs
# Python3_EXECUTABLE (find_package(Python3 ... Interpreter)).

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
