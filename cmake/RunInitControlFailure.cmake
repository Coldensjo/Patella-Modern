# The CTest test init_control_failure (cmake/WindowsX64.cmake; Patella-Modern issue #90): a PROGRAM_ERROR in
# InitControl (CMainFrame's constructor, before the frame window, the main window and the surface exist) must end
# PatellaTest.exe with FatalError's "Tibia Error" box (the test mode writes it to messages.txt instead of opening
# it) and exit code 1, as Tibia.exe does. Before #90 InitControl's catch called ExitControl, whose
# GetSurface()->ExitSurfaces() faulted on the null x64 surface, and then FatalError's SetDisplayMode faulted on the
# freed frame: exit code 0xC0000005, no box. Tibia.exe faults in both places too, and its catch (...) (MSVC 7.1:
# an access violation as well) goes on to the box.
#
# The runs, each in its own folder (<TMPROOT>/init-control-failure-<time and random suffix>/<run>):
#   no-tibia-dat: Tibia.spr and Tibia.pic without Tibia.dat, the box "Cannot find file "Tibia.dat". (Error Code 1)"
#   step-1 ... step-16: all three data files and PATELLA_TEST_FAIL_INIT=<n> (src/TestHooks.h): InitControl throws
#     PROGRAM_ERROR 6 before its step n (16: after the last), the box with "(Error Code 6)"
# The data files are CipSoft's: they stay on this machine, in the build folder, and the folder is removed
# afterwards. Without them the test is skipped (exit code 77). No Vulkan driver loads (VK_LOADER_DRIVERS_DISABLE=*,
# PATELLA_VULKAN=cpu): the error comes before the surface, and a run that got past InitControl would end with
# PROGRAM_ERROR 12 instead, which fails the test. No window is shown: the frame window is never created.
# Arguments: -DEXE=<PatellaTest.exe> -DDATA=<the folder of Tibia.exe> -DTMPROOT=<folder>

foreach(argument EXE DATA TMPROOT)
	if(NOT DEFINED ${argument})
		message(FATAL_ERROR "RunInitControlFailure.cmake: -D${argument}=... is missing")
	endif()
endforeach()

set(files Tibia.dat Tibia.spr Tibia.pic)
foreach(file IN LISTS files)
	if(NOT EXISTS "${DATA}/${file}")
		message("init_control_failure: ${DATA}/${file} is missing; skipped")
		cmake_language(EXIT 77)
	endif()
endforeach()

file(MAKE_DIRECTORY "${TMPROOT}")
string(TIMESTAMP stamp "%Y%m%d%H%M%S%f")
string(RANDOM LENGTH 6 ALPHABET 0123456789abcdef RANDOM_SEED "${stamp}" suffix)
set(root "${TMPROOT}/init-control-failure-${stamp}-${suffix}")
if(EXISTS "${root}")
	message(FATAL_ERROR "RunInitControlFailure.cmake: ${root} exists already")
endif()

set(ENV{VK_LOADER_DRIVERS_DISABLE} "*")
set(ENV{PATELLA_VULKAN} "cpu")
set(failures "")

# One run: the folder <root>/<name> with the data files <data>, PATELLA_TEST_FAIL_INIT=<step> (0: unset); the box
# must have the text <expected> (a regular expression).
function(run_client name data step expected)
	set(dir "${root}/${name}")
	file(MAKE_DIRECTORY "${dir}/out")
	foreach(file IN LISTS data)
		file(COPY "${DATA}/${file}" DESTINATION "${dir}")
	endforeach()
	file(WRITE "${dir}/start.script" "wait 1000\n")
	if(step EQUAL 0)
		unset(ENV{PATELLA_TEST_FAIL_INIT})
	else()
		set(ENV{PATELLA_TEST_FAIL_INIT} "${step}")
	endif()
	execute_process(COMMAND "${EXE}" --test-script start.script --test-out out WORKING_DIRECTORY "${dir}"
		RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 120)
	set(messages "")
	if(EXISTS "${dir}/out/messages.txt")
		file(READ "${dir}/out/messages.txt" messages)
	endif()
	message("init_control_failure ${name}: exit code ${result}\nstderr:\n${errors}\nmessages.txt:\n${messages}")
	if(NOT result STREQUAL "1")
		list(APPEND failures "${name}: the client's exit code is ${result}, not 1 (the box's exit)")
	elseif(NOT messages MATCHES "\"caption\":\"Tibia Error\"" OR NOT messages MATCHES "${expected}")
		list(APPEND failures "${name}: messages.txt has no \"Tibia Error\" box with ${expected}")
	endif()
	set(failures "${failures}" PARENT_SCOPE)
endfunction()

run_client(no-tibia-dat "Tibia.spr;Tibia.pic" 0 "Cannot find file \\\\\"Tibia\\.dat\\\\\"\\. \\(Error Code 1\\)")
foreach(step RANGE 1 16)
	run_client(step-${step} "${files}" ${step} "\\(Error Code 6\\)")
endforeach()

file(REMOVE_RECURSE "${root}")
if(EXISTS "${root}")
	message(WARNING "RunInitControlFailure.cmake: cannot remove ${root}")
endif()

list(LENGTH failures count)
if(count GREATER 0)
	list(JOIN failures "\n" text)
	message(FATAL_ERROR "init_control_failure: ${count} of 17 runs failed:\n${text}")
endif()
message("init_control_failure: 17 runs, each with the box and exit code 1")
