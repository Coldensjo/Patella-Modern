# The vlongtest CTest test (cmake/OracleX86.cmake), as tools\difftest\build.cmd runs it:
#	vlongtest.exe > vlong.txt, then python -I tools/difftest/vlongcheck.py vlong.txt
# Arguments: -DVLONGTEST=<vlongtest.exe> -DPYTHON=<python> -DCHECK=<vlongcheck.py> -DOUTPUT=<vlong.txt>

foreach(argument VLONGTEST PYTHON CHECK OUTPUT)
	if(NOT DEFINED ${argument})
		message(FATAL_ERROR "RunVlongTest.cmake: -D${argument}=... is missing")
	endif()
endforeach()

get_filename_component(output_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

execute_process(COMMAND "${VLONGTEST}" OUTPUT_FILE "${OUTPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "vlongtest.exe failed (${result})")
endif()

execute_process(COMMAND "${PYTHON}" -I "${CHECK}" "${OUTPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "vlongcheck.py failed (${result})")
endif()
