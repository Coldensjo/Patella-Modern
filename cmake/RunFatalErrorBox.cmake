# The CTest test fatal_error_box (cmake/WindowsX64.cmake; Patella-Modern issue #67, part 2): PatellaTest.exe started
# where no Vulkan driver loads (VK_LOADER_DRIVERS_DISABLE=*, PATELLA_VULKAN=cpu) ends the start-up with PROGRAM_ERROR
# 12 before InitInstance sets the main window. FatalError must still show its box, "Tibia Error" with "(Error Code
# 12)" (the test mode writes it to messages.txt instead of opening it), and end the client with exit code 1, as the
# original does: its AfxGetMainWnd()->ShowWindow on NULL raised an access violation that the original's catch (...)
# (MSVC 7.1) swallowed, and under /EHsc ended the client without the box.
#
# The run folder (<TMPROOT>/fatal-error-box-<time and random suffix>) gets copies of the data files next to
# Tibia.exe (Tibia.dat, Tibia.spr and Tibia.pic, CipSoft's: they stay on this machine, in the build folder, and the
# folder is removed afterwards) and a one-line script. Without them the test is skipped (exit code 77).
# Arguments: -DEXE=<PatellaTest.exe> -DDATA=<the folder of Tibia.exe> -DTMPROOT=<folder>

foreach(argument EXE DATA TMPROOT)
	if(NOT DEFINED ${argument})
		message(FATAL_ERROR "RunFatalErrorBox.cmake: -D${argument}=... is missing")
	endif()
endforeach()

set(files Tibia.dat Tibia.spr Tibia.pic)
foreach(file IN LISTS files)
	if(NOT EXISTS "${DATA}/${file}")
		message("fatal_error_box: ${DATA}/${file} is missing; skipped")
		cmake_language(EXIT 77)
	endif()
endforeach()

file(MAKE_DIRECTORY "${TMPROOT}")
string(TIMESTAMP stamp "%Y%m%d%H%M%S%f")
string(RANDOM LENGTH 6 ALPHABET 0123456789abcdef RANDOM_SEED "${stamp}" suffix)
set(dir "${TMPROOT}/fatal-error-box-${stamp}-${suffix}")
if(EXISTS "${dir}")
	message(FATAL_ERROR "RunFatalErrorBox.cmake: ${dir} exists already")
endif()
file(MAKE_DIRECTORY "${dir}/out")
foreach(file IN LISTS files)
	file(COPY "${DATA}/${file}" DESTINATION "${dir}")
endforeach()
file(WRITE "${dir}/start.script" "wait 1000\n")

set(ENV{VK_LOADER_DRIVERS_DISABLE} "*")
set(ENV{PATELLA_VULKAN} "cpu")
execute_process(COMMAND "${EXE}" --test-script start.script --test-out out WORKING_DIRECTORY "${dir}"
	RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 120)
set(messages "")
if(EXISTS "${dir}/out/messages.txt")
	file(READ "${dir}/out/messages.txt" messages)
endif()
file(REMOVE_RECURSE "${dir}")
if(EXISTS "${dir}")
	message(WARNING "RunFatalErrorBox.cmake: cannot remove ${dir}")
endif()

message("fatal_error_box: exit code ${result}\nstderr:\n${errors}\nmessages.txt:\n${messages}")
if(NOT result STREQUAL "1")
	message(FATAL_ERROR "fatal_error_box: the client's exit code is ${result}, not 1 (the box's exit)")
endif()
if(NOT messages MATCHES "\"caption\":\"Tibia Error\"" OR NOT messages MATCHES "\\(Error Code 12\\)")
	message(FATAL_ERROR "fatal_error_box: messages.txt has no \"Tibia Error\" box with \"(Error Code 12)\"")
endif()
