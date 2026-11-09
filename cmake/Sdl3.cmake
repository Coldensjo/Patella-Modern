# SDL3, the platform layer of plan M8 (decision D3; notes/modernization-plan.md, sections 5 and 7, M8;
# notes/decisions.md, "#122: the SDL3 window and its event loop"). Included by cmake/WindowsX64.cmake: the x64 client's
# window and event loop (src/SdlFrame.cpp, issue #122), its input since #123; the clipboard, URLs and message boxes
# since #124 (src/platform/SdlPlatform.cpp, the display modes SDL3's since #152; SDL_OpenURL in
# SdlFrame.cpp); the Vulkan engine's
# surfaces are SDL_Vulkan_CreateSurface's since #126 (src/vulkan/WindowSystem.h, also in tests_vulkan). patella_core
# does not use it.
#
#	SDL3::SDL3-static   SDL 3.4.18, built from its release archive with its own CMakeLists as a static library
#	                    with the client's CRT (the static one), only the subsystems the client needs (video and
#	                    events; Vulkan on: SDL_Vulkan_CreateSurface, #126)
#	SDL3::Headers       its headers alone: what the header checks compile against, so they do not build the library
#
# Pinned as cmake/Vulkan.cmake pins its libraries: one release, by URL and SHA256 (the digest GitHub publishes for the
# release asset SDL3-3.4.18.tar.gz), fetched at the first configure into build/<preset>/_deps and built there; nothing
# is installed on the system. Plan section 5: `find_package` first where a distribution has its own SDL3, which is
# Linux's (FetchContent's FIND_PACKAGE_ARGS, 3.4.18 or a newer 3.x); on Windows always the pinned archive. The Linux client
# (issue #131, cmake/LinuxX64.cmake) builds the pinned archive in the Docker image, which has SDL3's build dependencies
# (docker/linux-x64/Dockerfile: its X11 and Wayland video drivers, no SDL3 package).

include_guard(GLOBAL)
include(FetchContent)
include("${PROJECT_SOURCE_DIR}/cmake/Download.cmake")

set(PATELLA_SDL3_VERSION 3.4.18)
if(WIN32)
	patella_fetch_declare(patella_sdl3
		URL https://github.com/libsdl-org/SDL/releases/download/release-${PATELLA_SDL3_VERSION}/SDL3-${PATELLA_SDL3_VERSION}.tar.gz
		URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
		EXCLUDE_FROM_ALL)
else()
	patella_fetch_declare(patella_sdl3
		URL https://github.com/libsdl-org/SDL/releases/download/release-${PATELLA_SDL3_VERSION}/SDL3-${PATELLA_SDL3_VERSION}.tar.gz
		URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
		EXCLUDE_FROM_ALL
		FIND_PACKAGE_ARGS ${PATELLA_SDL3_VERSION} CONFIG NAMES SDL3)
endif()

# SDL's own options, in a function so that they stay SDL's. The static library only (no shared one, no test
# library, no tests, examples or install rules); the subsystems the client does not use are off.
function(patella_add_sdl3)
	set(SDL_SHARED OFF)
	set(SDL_STATIC ON)
	set(SDL_TEST_LIBRARY OFF)
	set(SDL_TESTS OFF)
	set(SDL_EXAMPLES OFF)
	set(SDL_INSTALL OFF)
	set(SDL_UNINSTALL OFF)
	set(SDL_DISABLE_INSTALL ON)
	set(SDL_AUDIO OFF)
	set(SDL_CAMERA OFF)
	set(SDL_JOYSTICK OFF)
	set(SDL_HAPTIC OFF)
	set(SDL_HIDAPI OFF)
	set(SDL_SENSOR OFF)
	set(SDL_POWER OFF)
	set(SDL_GPU OFF)
	set(SDL_RENDER OFF)
	set(SDL_DIALOG OFF)
	set(SDL_TRAY OFF)
	set(SDL_OPENGL OFF)
	set(SDL_OPENGLES OFF)
	set(SDL_WERROR OFF)
	# Linux (issue #131): the X11 and Wayland video drivers, loaded with dlopen at run time; what the client does not use is
	# off (the input methods' bus connections, the kernel's mode setting and device discovery).
	set(SDL_IBUS OFF)
	set(SDL_FCITX OFF)
	set(SDL_DBUS OFF)
	set(SDL_LIBUDEV OFF)
	set(SDL_KMSDRM OFF)
	set(SDL_X11_XTEST OFF)
	# The client's CRT: the static one (cmake/WindowsX64.cmake); CMake gives it to the targets SDL makes here.
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	# MSVC's /d1trimfile (cmake/Reproducible.cmake, issue #136) does not reach __FILE__ in a source compiled with a precompiled
	# header, which SDL_assert's file name is.
	if(MSVC)
		set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
	endif()
	FetchContent_MakeAvailable(patella_sdl3)
endfunction()
patella_add_sdl3()

if(TARGET SDL3-static AND NOT MSVC)
	target_compile_options(SDL3-static PRIVATE -w) # third-party code: no warnings of its own in the build log (build.py)
	set_target_properties(SDL3-static PROPERTIES POSITION_INDEPENDENT_CODE ON) # the product is position independent (cmake/Hardening.cmake)
endif()
if(TARGET SDL3-static AND MSVC)
	# Third-party code: not with the product's warning settings, and no warning of its own in the build log (build.py
	# fails a build on any warning, issue #17). SDL gives its sources /W3; /W0 replaces it (a later /W option wins
	# without the D9025 warning that a /W0 in CMAKE_C_FLAGS before SDL's /W3 would print).
	get_target_property(patella_sdl3_options SDL3-static COMPILE_OPTIONS)
	if(patella_sdl3_options)
		list(FILTER patella_sdl3_options EXCLUDE REGEX "/W[0-4]")
		set_target_properties(SDL3-static PROPERTIES COMPILE_OPTIONS "${patella_sdl3_options}")
	endif()
	target_compile_options(SDL3-static PRIVATE /W0)
	set_target_properties(SDL3-static PROPERTIES MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
endif()

# tests_sdl_clipboard (issue #159, tests/unit/sdl_clipboard_test.cpp): TSdlClipboard on SDL3's dummy video driver, which
# touches no desktop clipboard. Built with the client, with the static CRT as SDL here.
add_executable(tests_sdl_clipboard "${PROJECT_SOURCE_DIR}/tests/unit/sdl_clipboard_test.cpp"
	"${PROJECT_SOURCE_DIR}/src/platform/SdlClipboard.cpp")
target_include_directories(tests_sdl_clipboard PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tests_sdl_clipboard PRIVATE doctest::doctest
	$<IF:$<TARGET_EXISTS:SDL3::SDL3-static>,SDL3::SDL3-static,SDL3::SDL3>)
set_target_properties(tests_sdl_clipboard PROPERTIES
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
patella_target_language(tests_sdl_clipboard)
patella_target_warnings(tests_sdl_clipboard)
patella_target_hardening(tests_sdl_clipboard)
add_test(NAME tests_sdl_clipboard COMMAND tests_sdl_clipboard)
