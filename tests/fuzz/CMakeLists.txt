# The fuzz targets (Patella-Modern issue #132, plan M9; notes/decisions.md, "#132"): the packet parser and the file
# loaders, each an LLVMFuzzerTestOneInput of its own (fuzz.h).
#
#	fuzz_settings   Patella.ini (patella_core: settings/, log/LogSettings.cpp), with its round trips
#	fuzz_network    TNetwork's frames (Network.cpp: OnReceive, PrepareNextPacket, GetNextCommand, the readers; #72)
#	fuzz_packets    ReceivePackets and Communication.cpp's Receive* in the golden Communication test's world
#	fuzz_tibia_cfg  the import of Tibia.cfg (Preferences.cpp, Script.cpp) and the Patella.ini it writes (#45)
#	fuzz_map        the automap files <bx><by><bz>.map (Map.cpp)
#	fuzz_dat        Tibia.dat (Objects.cpp)
#	fuzz_sprites    Tibia.spr (graphics.cpp)
#	fuzz_pictures   Tibia.pic (graphics.cpp)
#
# fuzz_settings needs patella_core only and is built by every product preset. The others need the client's code, which
# is not patella_core's yet: they link the golden runner's objects (tests/golden/CMakeLists.txt: the client's code
# against the MFC shim, on Linux the Windows stand-ins) as bounded_reads does, with client.cpp in place of runner.cpp,
# so they are built where those are (windows-x64, -debug, -asan, linux-x64-gcc, linux-x64-clang; not windows-x64-gcc).
#
# PATELLA_FUZZ=OFF (every preset): each target is linked with driver.cpp, and CTest's fuzz_<name> replays the
# synthetic seeds of seeds/<name> (written by hand, committed) and a fixed number of the driver's own mutations of them:
# the smoke run, under each preset's sanitizers. PATELLA_FUZZ=ON (a build tree of its own on top of linux-x64-clang,
# Clang's libFuzzer, or windows-x64-asan, MSVC's /fsanitize=fuzzer): the targets and the code they reach are compiled
# with the fuzzer's coverage, linked with libFuzzer, and CTest's fuzz_<name> runs libFuzzer for 10 seconds on the
# synthetic seeds. tools/fuzz.py configures, builds and runs those trees, with the seeds made locally from the reference
# recordings and the client's files (CipSoft-derived: in build/fuzz-seeds, never committed; notes/reconstruction.md,
# "Fuzzing").

option(PATELLA_FUZZ "Link the fuzz targets of tests/fuzz with libFuzzer (Clang -fsanitize=fuzzer, MSVC /fsanitize=fuzzer; issue #132)" OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	return()
endif()

include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

set(fuzz_seeds "${CMAKE_CURRENT_SOURCE_DIR}/seeds")

if(PATELLA_FUZZ)
	if(MSVC)
		if(NOT PATELLA_SANITIZE STREQUAL "address")
			message(FATAL_ERROR "PATELLA_FUZZ with MSVC: on top of windows-x64-asan (PATELLA_SANITIZE=address)")
		endif()
		set(fuzz_coverage /fsanitize=fuzzer)
		set(fuzz_link_options)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		set(fuzz_coverage -fsanitize=fuzzer-no-link)
		set(fuzz_link_options -fsanitize=fuzzer)
	else()
		message(FATAL_ERROR "PATELLA_FUZZ: libFuzzer needs Clang (linux-x64-clang) or MSVC (windows-x64-asan)")
	endif()
	# The code the targets reach gets the fuzzer's coverage: patella_core and the golden runner's objects.
	foreach(target IN ITEMS patella_core golden_sysinc golden_client golden_client_tests golden_network_tests
			golden_comm_tests)
		if(TARGET ${target})
			target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:${fuzz_coverage}>)
		endif()
	endforeach()
endif()

# A fuzz target: its sources and support.cpp, with the driver (PATELLA_FUZZ=OFF) or libFuzzer, and its CTest test.
# MUTATIONS: the smoke run's mutations without libFuzzer; OPTIONS: libFuzzer's options of the target.
function(patella_fuzz_test name)
	cmake_parse_arguments(PARSE_ARGV 1 fuzz "" "MUTATIONS" "OPTIONS;ENVIRONMENT")
	if(PATELLA_FUZZ)
		target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:${fuzz_coverage}>)
		target_link_options(${name} PRIVATE ${fuzz_link_options})
		# Under MSVC's /fsanitize=fuzzer every input of fuzz_packets ends in a heap report inside the golden
		# Communication world's own strings (its redirections of our code); built, not tested (notes/decisions.md, "#132").
		if(MSVC AND name STREQUAL "fuzz_packets")
			return()
		endif()
		set(corpus "${CMAKE_CURRENT_BINARY_DIR}/smoke/${name}")
		file(MAKE_DIRECTORY "${corpus}")
		add_test(NAME ${name} COMMAND ${name} -max_total_time=10 -seed=1 ${fuzz_OPTIONS} "${corpus}"
			"${fuzz_seeds}/${name}")
	else()
		target_sources(${name} PRIVATE driver.cpp)
		add_test(NAME ${name} COMMAND ${name} --mutations=${fuzz_MUTATIONS} --seed=1 "${fuzz_seeds}/${name}")
	endif()
	if(fuzz_ENVIRONMENT)
		set_tests_properties(${name} PROPERTIES ENVIRONMENT "${fuzz_ENVIRONMENT}")
	endif()
	set_property(TEST ${name} APPEND PROPERTY LABELS fuzz)
endfunction()

add_executable(fuzz_settings fuzz_settings.cpp support.cpp)
target_link_libraries(fuzz_settings PRIVATE patella_core)
patella_target_language(fuzz_settings)
patella_target_warnings(fuzz_settings)
patella_target_hardening(fuzz_settings)
patella_fuzz_test(fuzz_settings MUTATIONS 5000 OPTIONS -max_len=16384)

if(NOT TARGET golden_client)
	return()
endif()

# The golden directory's settings that its functions read (golden_client_settings, golden_executable_settings): the
# variables of tests/golden/CMakeLists.txt, read from there.
foreach(variable IN ITEMS golden_linux golden_linux_dir golden_linux_options golden_linux_warnings)
	get_directory_property(${variable} DIRECTORY "${PROJECT_SOURCE_DIR}/tests/golden" DEFINITION ${variable})
endforeach()

# The ASan options of the golden objects' tests (tests/golden/CMakeLists.txt): the original's mismatched releases and
# leaks are not reported (the loaders' tables a load lets leak, as the original's).
if(golden_linux AND PATELLA_SANITIZE MATCHES "address")
	set(fuzz_client_environment "ASAN_OPTIONS=detect_leaks=0:alloc_dealloc_mismatch=0")
else()
	set(fuzz_client_environment)
endif()

# The golden objects, with one fuzz target's sources, as bounded_reads links them.
function(patella_fuzz_client name)
	cmake_parse_arguments(PARSE_ARGV 1 fuzz "" "MUTATIONS" "SOURCES;OPTIONS")
	add_library(${name}_objects OBJECT ${fuzz_SOURCES} client.cpp support.cpp)
	golden_client_settings(${name}_objects)
	target_include_directories(${name}_objects PRIVATE "${PROJECT_SOURCE_DIR}/tests/golden")
	if(PATELLA_FUZZ)
		target_compile_options(${name}_objects PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:${fuzz_coverage}>)
	endif()
	add_executable(${name}
		"${PROJECT_SOURCE_DIR}/tests/golden/golden.cpp"
		"${PROJECT_SOURCE_DIR}/tests/golden/core_tests.cpp"
		"${PROJECT_SOURCE_DIR}/tests/golden/regexp_tests.cpp"
		"${PROJECT_SOURCE_DIR}/tests/golden/sysinc_tests.cpp"
		$<TARGET_OBJECTS:golden_sysinc>
		$<TARGET_OBJECTS:golden_client>
		$<TARGET_OBJECTS:golden_client_tests>
		$<TARGET_OBJECTS:golden_network_tests>
		$<TARGET_OBJECTS:golden_comm_tests>
		$<TARGET_OBJECTS:${name}_objects>)
	golden_executable_settings(${name})
	patella_fuzz_test(${name} MUTATIONS ${fuzz_MUTATIONS} OPTIONS ${fuzz_OPTIONS} ENVIRONMENT ${fuzz_client_environment})
endfunction()

patella_fuzz_client(fuzz_network SOURCES fuzz_network.cpp MUTATIONS 20000 OPTIONS -max_len=65536)
patella_fuzz_client(fuzz_packets SOURCES fuzz_packets.cpp MUTATIONS 5000 OPTIONS -max_len=65536)
patella_fuzz_client(fuzz_tibia_cfg SOURCES fuzz_tibia_cfg.cpp MUTATIONS 1000 OPTIONS -max_len=16384)
patella_fuzz_client(fuzz_map SOURCES fuzz_map.cpp MUTATIONS 200 OPTIONS -max_len=200000)
patella_fuzz_client(fuzz_dat SOURCES fuzz_dat.cpp MUTATIONS 1000 OPTIONS -rss_limit_mb=8192 -malloc_limit_mb=8192)
patella_fuzz_client(fuzz_sprites SOURCES fuzz_sprites.cpp MUTATIONS 1000)
patella_fuzz_client(fuzz_pictures SOURCES fuzz_pictures.cpp MUTATIONS 1000)
