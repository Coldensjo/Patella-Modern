# The CTest test start_without_data (cmake/WindowsX64.cmake; Patella-Modern issue #127): PatellaTest.exe started in
# an empty folder (no Tibia.dat, Tibia.spr or Tibia.pic) ends the start-up with PROGRAM_ERROR 1 in InitControl:
# FatalError's box "Cannot find file "Tibia.dat". (Error Code 1)" (the test mode writes it to messages.txt) and exit
# code 1, the original's error. It reads no CipSoft file, so the hosted CI, whose build has no such file anywhere,
# runs it. The client runs headless (--test-headless, issue #145), no Vulkan driver loads
# (VK_LOADER_DRIVERS_DISABLE=*, PATELLA_VULKAN=cpu): the error comes before the surface.
# Arguments: -DEXE=<PatellaTest.exe> -DTMPROOT=<folder>

foreach(argument EXE TMPROOT)
	if(NOT DEFINED ${argument})
		message(FATAL_ERROR "RunStartWithoutData.cmake: -D${argument}=... is missing")
	endif()
endforeach()

file(MAKE_DIRECTORY "${TMPROOT}")
string(TIMESTAMP stamp "%Y%m%d%H%M%S%f")
string(RANDOM LENGTH 6 ALPHABET 0123456789abcdef RANDOM_SEED "${stamp}" suffix)
set(dir "${TMPROOT}/start-without-data-${stamp}-${suffix}")
if(EXISTS "${dir}")
	message(FATAL_ERROR "RunStartWithoutData.cmake: ${dir} exists already")
endif()
file(MAKE_DIRECTORY "${dir}/out")
file(WRITE "${dir}/start.script" "wait 1000\n")

set(ENV{VK_LOADER_DRIVERS_DISABLE} "*")
set(ENV{PATELLA_VULKAN} "cpu")
execute_process(COMMAND "${EXE}" --test-script start.script --test-out out --test-headless
	WORKING_DIRECTORY "${dir}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 120)
set(messages "")
if(EXISTS "${dir}/out/messages.txt")
	file(READ "${dir}/out/messages.txt" messages)
endif()
file(REMOVE_RECURSE "${dir}")
if(EXISTS "${dir}")
	message(WARNING "RunStartWithoutData.cmake: cannot remove ${dir}")
endif()

message("start_without_data: exit code ${result}\nstderr:\n${errors}\nmessages.txt:\n${messages}")
if(NOT result STREQUAL "1")
	message(FATAL_ERROR "start_without_data: the client's exit code is ${result}, not 1 (the box's exit)")
endif()
if(NOT messages MATCHES "\"caption\":\"Tibia Error\"" OR NOT messages MATCHES "Tibia\\.dat" OR NOT messages MATCHES "\\(Error Code 1\\)")
	message(FATAL_ERROR "start_without_data: messages.txt has no \"Tibia Error\" box naming Tibia.dat with \"(Error Code 1)\"")
endif()
