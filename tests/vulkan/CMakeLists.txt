# tests_vulkan (issue #19; notes/vulkan.md, "Checks"): the Vulkan back end (src/vulkan/Renderer.cpp)
# headless, on the CPU driver next to the executable and, when the machine has one, on a GPU. Known
# primitives (fills, copies, the automap's point-sampled copy, stretches, the colour key, the light)
# are drawn off screen, read back and compared with a model of Direct3D 9's pixel rules: exact, except
# the filtered stretch and the light (at most 1 per channel). No window is shown: the tests that present (the client's
# start-up order, issue-31-fix; the device windows of issues #52 and #126) use windows that are created hidden and never
# shown. Exit code 77 (a
# skipped CTest test) when the machine has no Vulkan loader. Built with the client
# (cmake/Vulkan.cmake), with the static CRT as the back end.

# The engine's window systems (src/vulkan/WindowSystem.h, issue #126) with SDL3 (cmake/Sdl3.cmake, included after this
# file: the target is resolved when the build is generated): the devices for a window get their surfaces through
# SDL_Vulkan_CreateSurface, the client's only path since issue #127.
add_executable(tests_vulkan render_test.cpp "${PROJECT_SOURCE_DIR}/src/vulkan/WindowSystemSdl.cpp")
target_link_libraries(tests_vulkan PRIVATE patella_render_vulkan doctest::doctest SDL3::SDL3-static)
set_target_properties(tests_vulkan PROPERTIES
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
patella_target_language(tests_vulkan)
patella_target_warnings(tests_vulkan)
patella_target_hardening(tests_vulkan)
patella_vulkan_cpu_driver(tests_vulkan)
add_test(NAME tests_vulkan COMMAND tests_vulkan)
set_tests_properties(tests_vulkan PROPERTIES SKIP_RETURN_CODE 77)
