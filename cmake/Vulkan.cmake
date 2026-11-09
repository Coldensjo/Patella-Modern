# The Vulkan engine of the x64 client (issue #19; notes/vulkan.md; notes/modernization-plan.md, 6.2 and M2), the
# client's only engine since issue #20. Included by cmake/WindowsX64.cmake and, since issue #131, cmake/LinuxX64.cmake
# (there the CPU driver is the system's Mesa lavapipe, and tests_vulkan, which is Windows' windows and messages, is left
# out):
#
#	patella_vulkan_deps     the libraries' sources: volk, vk-bootstrap, VMA (src/vulkan/Vma.cpp), without
#	                        the product's warning settings (they are not ours)
#	patella_render_vulkan   the back end, src/vulkan/Renderer.cpp, Quads.cpp, FilterMask.cpp,
#	                        DeviceChoice.cpp (the device choice's words, issue #31) and PresentWindow.cpp
#	                        (one window per device, issue #52), with the product's
#	                        settings; the shaders of src/vulkan/shaders compiled to SPIR-V by glslang
#	tests_vulkan            the headless check of the back end on the CPU driver (and a GPU if there is
#	                        one): known primitives drawn off screen, read back and compared (tests/vulkan)
#	the CPU driver          Mesa's lavapipe (vulkan_lvp.dll and lvp_icd.x86_64.json), copied next to
#	                        every executable that uses the engine (patella_vulkan_cpu_driver)
#
# The client adds src/vulkan/SurfaceVulkan.cpp (TSurfaceVulkan, the TSurface on the SDL3 window) and
# PATELLA_VULKAN (Surface.cpp's and Control.cpp's engine selection of the x64 client). The libraries
# and the client link the static CRT, as the client does. Every dependency is pinned by URL and SHA256
# (notes/vulkan.md, "Dependencies"); the first configure downloads them (about 85 MB, most of it Mesa's
# archive), later ones reuse build/<preset>/_deps.

include_guard(GLOBAL)
include(FetchContent)
include("${PROJECT_SOURCE_DIR}/cmake/Download.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/Hardening.cmake")

# Sources only (no CMakeLists.txt is used): SOURCE_SUBDIR names a directory that does not exist.
patella_fetch_declare(patella_vulkan_headers
	URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
	URL_HASH SHA256=4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e
	SOURCE_SUBDIR patella-sources-only)
patella_fetch_declare(patella_volk
	URL https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
	URL_HASH SHA256=1547d8d74395d4048fb3f4a6313da56db626e89b55db238d0d7e8944c8a645f3
	SOURCE_SUBDIR patella-sources-only)
patella_fetch_declare(patella_vma
	URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.5.0.tar.gz
	URL_HASH SHA256=c5f3db848fca9cb9726b41f256cc3007773321fa92a86d1e0902e66474e8a9e3
	SOURCE_SUBDIR patella-sources-only)
patella_fetch_declare(patella_vk_bootstrap
	URL https://github.com/charles-lunarg/vk-bootstrap/archive/refs/tags/v1.4.363.tar.gz
	URL_HASH SHA256=11927b43a5c5d64646982b273d6436d2243acaa2da8834f7ba382060431155fe
	SOURCE_SUBDIR patella-sources-only)
# glslang is built with its own CMakeLists (only the glslang executable, which compiles the shaders),
# with the compiler's warnings off: third-party code; the build checks ours.
patella_fetch_declare(patella_glslang
	URL https://github.com/KhronosGroup/glslang/archive/refs/tags/16.6.0.tar.gz
	URL_HASH SHA256=9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2
	EXCLUDE_FROM_ALL)
# The CPU driver: Mesa 26.2.6's lavapipe, prebuilt with MSVC by the mesa-dist-win project (the archive's
# SHA256 is the one GitHub publishes for the release asset). Only two files are taken from it.
if(WIN32)
	patella_fetch_declare(patella_lavapipe
		URL https://github.com/pal1000/mesa-dist-win/releases/download/26.2.6/mesa3d-26.2.6-release-msvc.7z
		URL_HASH SHA256=b2e00615feaba45942343c265cc8c5f576142ca682c1ac6661922d4b72492877
		DOWNLOAD_NO_EXTRACT TRUE)
	FetchContent_MakeAvailable(patella_lavapipe)
endif()

FetchContent_MakeAvailable(patella_vulkan_headers patella_volk patella_vma patella_vk_bootstrap)

function(patella_add_glslang)
	set(ENABLE_OPT OFF)
	set(ENABLE_HLSL OFF)
	set(ENABLE_GLSLANG_BINARIES ON)
	set(ENABLE_SPVREMAPPER OFF)
	set(ENABLE_PCH OFF)
	set(ENABLE_CTEST OFF)
	set(GLSLANG_TESTS OFF)
	set(GLSLANG_ENABLE_INSTALL OFF)
	set(BUILD_EXTERNAL OFF)
	set(BUILD_SHARED_LIBS OFF)
	if(MSVC)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W0")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
	endif()
	FetchContent_MakeAvailable(patella_glslang)
endfunction()
patella_add_glslang()

# The libraries' sources.
add_library(patella_vulkan_deps STATIC
	"${patella_volk_SOURCE_DIR}/volk.c"
	"${patella_vk_bootstrap_SOURCE_DIR}/src/VkBootstrap.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/Vma.cpp")
target_include_directories(patella_vulkan_deps SYSTEM PUBLIC
	"${patella_vulkan_headers_SOURCE_DIR}/include"
	"${patella_volk_SOURCE_DIR}"
	"${patella_vma_SOURCE_DIR}/include"
	"${patella_vk_bootstrap_SOURCE_DIR}/src")
# volk declares the functions (VK_NO_PROTOTYPES everywhere); its loader also loads the Win32 surface
# functions, which SurfaceVulkan.cpp declares the same way.
target_compile_definitions(patella_vulkan_deps PUBLIC VK_NO_PROTOTYPES)
if(WIN32)
	set_source_files_properties("${patella_volk_SOURCE_DIR}/volk.c" PROPERTIES COMPILE_DEFINITIONS VK_USE_PLATFORM_WIN32_KHR)
endif()
set_target_properties(patella_vulkan_deps PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
if(MSVC)
	target_compile_options(patella_vulkan_deps PRIVATE /W0 /utf-8)
else()
	target_compile_options(patella_vulkan_deps PRIVATE -w)
	target_link_libraries(patella_vulkan_deps PUBLIC ${CMAKE_DL_LIBS})
endif()
patella_target_hardening(patella_vulkan_deps)

# The shaders, compiled by the glslang of this build into headers of SPIR-V words (per configuration:
# the command runs the configuration's glslang).
set(patella_shader_dir "${PROJECT_BINARY_DIR}/shaders/$<CONFIG>")
set(patella_shader_headers "")
foreach(pair IN ITEMS Quad.vert:QuadVert Quad.frag:QuadFrag Fill.frag:FillFrag ColorKey.frag:ColorKeyFrag)
	string(REPLACE ":" ";" pair "${pair}")
	list(GET pair 0 file)
	list(GET pair 1 name)
	set(source "${PROJECT_SOURCE_DIR}/src/vulkan/shaders/${file}")
	set(header "${patella_shader_dir}/${name}.h")
	add_custom_command(OUTPUT "${header}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${patella_shader_dir}"
		COMMAND glslang-standalone -V --target-env vulkan1.3 --quiet --vn ${name}Spirv -o "${header}" "${source}"
		DEPENDS "${source}" "${PROJECT_SOURCE_DIR}/src/vulkan/shaders/Constants.glsl" glslang-standalone
		COMMENT "glslang ${file}"
		VERBATIM)
	list(APPEND patella_shader_headers "${header}")
endforeach()

# The back end, with the product's settings.
add_library(patella_render_vulkan STATIC
	"${PROJECT_SOURCE_DIR}/src/vulkan/Renderer.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/Quads.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/FilterMask.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/DeviceChoice.cpp"
	"${PROJECT_SOURCE_DIR}/src/vulkan/PresentWindow.cpp"
	${patella_shader_headers})
target_include_directories(patella_render_vulkan PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_include_directories(patella_render_vulkan PRIVATE "${patella_shader_dir}")
# comctl32: PresentWindow.cpp subclasses the main window (SetWindowSubclass, issue #71).
if(WIN32)
	target_link_libraries(patella_render_vulkan PUBLIC patella_vulkan_deps comctl32)
else()
	# PresentWindow.cpp's device test window (Linux, issue #131).
	target_link_libraries(patella_render_vulkan PUBLIC patella_vulkan_deps PRIVATE SDL3::Headers)
endif()
set_target_properties(patella_render_vulkan PROPERTIES
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
patella_target_language(patella_render_vulkan)
patella_target_warnings(patella_render_vulkan)
patella_target_hardening(patella_render_vulkan)
if(NOT WIN32)
	# Issue #131: as the Linux client's sources (cmake/LinuxX64.cmake): the Windows presets' clang-tidy runs check them.
	set_property(TARGET patella_vulkan_deps patella_render_vulkan PROPERTY EXPORT_COMPILE_COMMANDS OFF)

	# Renderer.cpp and PresentWindow.cpp in compile_commands.json for tools/tidy.py (issue #164, notes/decisions.md "#164"),
	# with patella_render_vulkan's settings. Never built (not in `all`, nothing depends on it). The hosted clang-tidy job
	# only configures, so the generated SPIR-V headers (above) do not exist there: this target includes stand-ins of the
	# same four names, one word each, which only the analysis reads; the real ones stay the only ones that are compiled.
	set(patella_tidy_shader_dir "${PROJECT_BINARY_DIR}/tidy-shaders")
	foreach(name IN ITEMS QuadVert QuadFrag FillFrag ColorKeyFrag)
		file(WRITE "${patella_tidy_shader_dir}/${name}.h"
			"// Generated by cmake/Vulkan.cmake for clang-tidy only (issue #164); never compiled into anything.\n"
			"#pragma once\n#include <cstdint>\nconst std::uint32_t ${name}Spirv[1] = {0};\n")
	endforeach()
	add_library(patella_tidy_linux_render OBJECT EXCLUDE_FROM_ALL
		"${PROJECT_SOURCE_DIR}/src/vulkan/Renderer.cpp"
		"${PROJECT_SOURCE_DIR}/src/vulkan/PresentWindow.cpp")
	target_include_directories(patella_tidy_linux_render PRIVATE "${PROJECT_SOURCE_DIR}/src" "${patella_tidy_shader_dir}")
	target_link_libraries(patella_tidy_linux_render PRIVATE patella_vulkan_deps SDL3::Headers)
	patella_target_language(patella_tidy_linux_render)
	patella_target_warnings(patella_tidy_linux_render)
	patella_target_hardening(patella_tidy_linux_render)
endif()

# The CPU driver's two files, taken from the archive once per build tree (Windows; on Linux lavapipe is the system's,
# the distribution's mesa-vulkan-drivers package, DeviceSettings.cpp FindCpuDriverManifest).
if(NOT WIN32)
	function(patella_vulkan_cpu_driver target)
	endfunction()
	return()
endif()
set(PATELLA_VULKAN_CPU_DRIVER_DIR "${PROJECT_BINARY_DIR}/vulkan-cpu")
set(patella_lavapipe_archive "${patella_lavapipe_SOURCE_DIR}/mesa3d-26.2.6-release-msvc.7z")
set(patella_lavapipe_files "${PATELLA_VULKAN_CPU_DRIVER_DIR}/x64/vulkan_lvp.dll"
	"${PATELLA_VULKAN_CPU_DRIVER_DIR}/x64/lvp_icd.x86_64.json")
set(patella_lavapipe_stamp "${PATELLA_VULKAN_CPU_DRIVER_DIR}/extracted-b2e00615")
if(NOT EXISTS "${patella_lavapipe_stamp}")
	file(REMOVE_RECURSE "${PATELLA_VULKAN_CPU_DRIVER_DIR}")
	file(MAKE_DIRECTORY "${PATELLA_VULKAN_CPU_DRIVER_DIR}")
	execute_process(
		COMMAND "${CMAKE_COMMAND}" -E tar xf "${patella_lavapipe_archive}" x64/vulkan_lvp.dll x64/lvp_icd.x86_64.json
		WORKING_DIRECTORY "${PATELLA_VULKAN_CPU_DRIVER_DIR}"
		RESULT_VARIABLE result)
	foreach(file IN LISTS patella_lavapipe_files)
		if(NOT result EQUAL 0 OR NOT EXISTS "${file}")
			message(FATAL_ERROR "cannot take ${file} from ${patella_lavapipe_archive}")
		endif()
	endforeach()
	file(TOUCH "${patella_lavapipe_stamp}")
endif()

# Copies the CPU driver next to the target's executable, where the engine looks for it.
function(patella_vulkan_cpu_driver target)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${patella_lavapipe_files} "$<TARGET_FILE_DIR:${target}>"
		VERBATIM)
endfunction()

add_subdirectory("${PROJECT_SOURCE_DIR}/tests/vulkan" "${PROJECT_BINARY_DIR}/tests/vulkan")
