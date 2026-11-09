# CPACK_PRE_BUILD_SCRIPTS of cmake/Package.cmake (issue #135): the package-time refusal. CPack has installed the files
# into its staging folder and has not made the package yet: tools/package_check.py --staged checks those very files
# (no file like the data files, the placeholders in the executable, the expected list), so a failing tree gives no
# package at all, whatever the build folder's cache says.

foreach(argument CPACK_PATELLA_PYTHON CPACK_PATELLA_CHECK CPACK_TEMPORARY_DIRECTORY)
	if(NOT DEFINED ${argument})
		message(FATAL_ERROR "PackagePreBuild.cmake: ${argument} is not set")
	endif()
endforeach()

execute_process(
	COMMAND "${CPACK_PATELLA_PYTHON}" -I "${CPACK_PATELLA_CHECK}" --staged "${CPACK_TEMPORARY_DIRECTORY}"
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE output)
message(STATUS "${output}")
if(NOT result EQUAL 0)
	message(FATAL_ERROR "tools/package_check.py --staged refused this package (exit ${result})")
endif()
