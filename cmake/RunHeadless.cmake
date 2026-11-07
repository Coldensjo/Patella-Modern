# The CTest test headless_client (cmake/WindowsX64.cmake; Patella-Modern issue #145): PatellaTest.exe with
# --test-headless runs without a desktop (its window never shown, the Vulkan engine without a surface) and still
# writes its dumps: a script that waits, takes a state dump and a frame and quits ends with exit code 0, the driver's
# log names the headless mode, the state dump and both frame pictures are written. --test-headless without a script
# ends the client before its window with exit code 3 and the reason on stderr.
#
# The run folder (<TMPROOT>/headless-<time and random suffix>) gets copies of the data files next to Tibia.exe
# (Tibia.dat, Tibia.spr and Tibia.pic, CipSoft's: they stay on this machine, in the build folder, and the folder is
# removed afterwards); the client runs on the CPU driver next to it in the build folder (PATELLA_VULKAN=cpu). Without
# the data files the test is skipped (exit code 77).
# Arguments: -DEXE=<PatellaTest.exe> -DDATA=<the folder of Tibia.exe> -DTMPROOT=<folder>

foreach(argument EXE DATA TMPROOT)
	if(NOT DEFINED ${argument})
		message(FATAL_ERROR "RunHeadless.cmake: -D${argument}=... is missing")
	endif()
endforeach()

set(files Tibia.dat Tibia.spr Tibia.pic)
foreach(file IN LISTS files)
	if(NOT EXISTS "${DATA}/${file}")
		message("headless_client: ${DATA}/${file} is missing; skipped")
		cmake_language(EXIT 77)
	endif()
endforeach()

file(MAKE_DIRECTORY "${TMPROOT}")
string(TIMESTAMP stamp "%Y%m%d%H%M%S%f")
string(RANDOM LENGTH 6 ALPHABET 0123456789abcdef RANDOM_SEED "${stamp}" suffix)
set(dir "${TMPROOT}/headless-${stamp}-${suffix}")
if(EXISTS "${dir}")
	message(FATAL_ERROR "RunHeadless.cmake: ${dir} exists already")
endif()
file(MAKE_DIRECTORY "${dir}/out")
foreach(file IN LISTS files)
	file(COPY "${DATA}/${file}" DESTINATION "${dir}")
endforeach()
file(WRITE "${dir}/start.script" "pace 0\nwait 2000\ndump\nframe\nwait 500\nquit\n")

set(ENV{PATELLA_VULKAN} "cpu")
execute_process(COMMAND "${EXE}" --test-script start.script --test-out out --test-log out/driver.log --test-headless
	WORKING_DIRECTORY "${dir}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 120)
set(log "")
if(EXISTS "${dir}/out/driver.log")
	file(READ "${dir}/out/driver.log" log)
endif()
set(missing "")
foreach(file state-001.json frame-001.png frame-001.filtered.png frames.txt)
	if(NOT EXISTS "${dir}/out/${file}")
		list(APPEND missing "${file}")
	endif()
endforeach()
execute_process(COMMAND "${EXE}" --test-headless WORKING_DIRECTORY "${dir}"
	RESULT_VARIABLE unscripted OUTPUT_VARIABLE unscripted_output ERROR_VARIABLE unscripted_errors TIMEOUT 120)
file(REMOVE_RECURSE "${dir}")
if(EXISTS "${dir}")
	message(WARNING "RunHeadless.cmake: cannot remove ${dir}")
endif()

message("headless_client: exit code ${result}\nstderr:\n${errors}\ndriver.log:\n${log}")
message("headless_client without a script: exit code ${unscripted}\nstderr:\n${unscripted_errors}")
if(NOT result STREQUAL "0")
	message(FATAL_ERROR "headless_client: the client's exit code is ${result}, not 0")
endif()
if(NOT log MATCHES "testmode: headless")
	message(FATAL_ERROR "headless_client: the driver's log does not name the headless mode")
endif()
if(missing)
	message(FATAL_ERROR "headless_client: not written: ${missing}")
endif()
if(NOT unscripted STREQUAL "3" OR NOT unscripted_errors MATCHES "--test-headless needs --test-script")
	message(FATAL_ERROR "headless_client: without a script the exit code is ${unscripted}, not 3 with the reason")
endif()
