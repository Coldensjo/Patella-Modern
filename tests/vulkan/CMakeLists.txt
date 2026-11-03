# tests_vulkan (issue #19; notes/vulkan.md, "Checks"): the Vulkan back end (src/vulkan/Renderer.cpp)
# headless, on the CPU driver next to the executable and, when the machine has one, on a GPU. Known
# primitives (fills, copies, the automap's point-sampled copy, stretches, the colour key, the light)
# are drawn off screen, read back and compared with a model of Direct3D 9's pixel rules: exact, except
# the filtered stretch and the light (at most 1 per channel). No window is shown: one test (the client's
# start-up order, issue-31-fix) presents to a window that is created hidden and never shown. Exit code 77 (a
# skipped CTest test) when the machine has no Vulkan loader. Built with the client
# (cmake/Vulkan.cmake), with the static CRT as the back end.

add_executable(tests_vulkan render_test.cpp)
target_link_libraries(tests_vulkan PRIVATE patella_render_vulkan doctest::doctest)
set_target_properties(tests_vulkan PROPERTIES
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
patella_target_language(tests_vulkan)
patella_target_warnings(tests_vulkan)
patella_target_hardening(tests_vulkan)
patella_vulkan_cpu_driver(tests_vulkan)
add_test(NAME tests_vulkan COMMAND tests_vulkan)
set_tests_properties(tests_vulkan PROPERTIES SKIP_RETURN_CODE 77)
