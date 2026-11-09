# The packages (Patella-Modern issue #135, plan M9, D4; notes/decisions.md, "#135"): CPack's ZIP on Windows, TGZ and
# AppImage on Linux. Included by CMakeLists.txt only with PATELLA_PACKAGE=ON, after the client's own cmake file, so
# without that option a build has no install rules, no CPackConfig.cmake and no "package" target. The presets
# package-windows-x64 and package-linux-x64 set it, with PATELLA_PLACEHOLDER_RESOURCES=ON.
#
# A package never holds CipSoft's files, and a local build embeds Tibia.exe's cursors and icon (cmake/Resources.cmake),
# so a package is only made from a build of the placeholder pictures (tools/placeholder_resources.py). Refused here at
# configure time otherwise, with what an ordinary local build has; and again when the package is made, on the staged
# files themselves: CPACK_PRE_BUILD_SCRIPTS (cmake/PackagePreBuild.cmake) runs tools/package_check.py --staged, which
# looks for the placeholders in the executable and for anything like the data files.
#
# What a package holds: the client (Patella, not the test build, which reads the login server and the RSA key from the
# environment), on Windows the CPU driver next to it (Mesa's lavapipe, cmake/Vulkan.cmake), README.txt, the licences.
# Linux's CPU driver is the system's, and the Vulkan loader is the system's on both. The list is
# tools/package_check.py's EXPECTED, which fails a package with any other file.
#
# The AppImage's build tools, only here and only fetched with this option (patella_fetch_declare, SHA-256 pinned):
# appimagetool and its runtime (AppImage project), patchelf (CPack's generator calls it to set the RPATH). Not in the
# Docker image: nothing else uses them, and a new tag would rebuild every Linux job's image.

include_guard(GLOBAL)

if(NOT TARGET Patella)
	message(FATAL_ERROR "PATELLA_PACKAGE=ON needs the client (PATELLA_CLIENT=ON, on MSVC x64 or Linux)")
endif()
if(NOT PATELLA_PLACEHOLDER_RESOURCES)
	message(FATAL_ERROR "PATELLA_PACKAGE=ON refused: this build embeds the pictures of the local Tibia.exe "
		"(cmake/Resources.cmake), which are CipSoft's and never go into a package. Packages come only from a build with "
		"PATELLA_PLACEHOLDER_RESOURCES=ON (the presets package-windows-x64 and package-linux-x64), on a machine or in a "
		"folder without Tibia.exe.")
endif()
if(NOT PATELLA_RES_DIR STREQUAL "${PROJECT_BINARY_DIR}/res-placeholder")
	message(FATAL_ERROR "PATELLA_PACKAGE=ON refused: the pictures come from ${PATELLA_RES_DIR}, not res-placeholder")
endif()
execute_process(
	COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/package_check.py" --placeholders "${PATELLA_RES_DIR}"
	RESULT_VARIABLE patella_placeholders_result
	OUTPUT_VARIABLE patella_placeholders_output
	ERROR_VARIABLE patella_placeholders_output)
if(NOT patella_placeholders_result EQUAL 0)
	message(FATAL_ERROR "PATELLA_PACKAGE=ON refused: ${PATELLA_RES_DIR} is not the placeholders:\n${patella_placeholders_output}")
endif()

set(PATELLA_PACKAGE_VERSION "dev" CACHE STRING "The version in the packages' names")

FetchContent_GetProperties(patella_sdl3)
# The licence texts of what ships, from the pinned archives (the folders FetchContent left); the texts no archive has
# are in packaging/licenses.
set(patella_package_licenses
	"LICENSE=${PROJECT_SOURCE_DIR}/LICENSE"
	"NOTICE=${PROJECT_SOURCE_DIR}/NOTICE"
	"licenses/THIRD-PARTY.txt=${PROJECT_SOURCE_DIR}/packaging/licenses/THIRD-PARTY.txt"
	"licenses/regexp.txt=${PROJECT_SOURCE_DIR}/packaging/licenses/regexp.txt"
	"licenses/SDL3.txt=${patella_sdl3_SOURCE_DIR}/LICENSE.txt"
	"licenses/volk.txt=${patella_volk_SOURCE_DIR}/LICENSE.md"
	"licenses/VulkanMemoryAllocator.txt=${patella_vma_SOURCE_DIR}/LICENSE.txt"
	"licenses/vk-bootstrap.txt=${patella_vk_bootstrap_SOURCE_DIR}/LICENSE.txt"
	"licenses/Vulkan-Headers.txt=${patella_vulkan_headers_SOURCE_DIR}/LICENSE.md"
	"licenses/Vulkan-Headers-MIT.txt=${patella_vulkan_headers_SOURCE_DIR}/LICENSES/MIT.txt")
if(WIN32)
	list(APPEND patella_package_licenses
		"licenses/Mesa.txt=${PROJECT_SOURCE_DIR}/packaging/licenses/Mesa.txt"
		"licenses/LLVM.txt=${PROJECT_SOURCE_DIR}/packaging/licenses/LLVM.txt")
endif()

if(WIN32)
	set(patella_package_docs ".")
	install(TARGETS Patella RUNTIME DESTINATION .)
	install(FILES ${patella_lavapipe_files} DESTINATION .)
	install(FILES "${PROJECT_SOURCE_DIR}/packaging/README-windows.txt" DESTINATION . RENAME README.txt)
	set(CPACK_GENERATOR ZIP)
	set(patella_package_platform windows-x64)
else()
	set(patella_package_docs share/doc/patella)
	install(TARGETS Patella RUNTIME DESTINATION bin)
	install(FILES "${PROJECT_SOURCE_DIR}/packaging/README-linux.txt" DESTINATION share/doc/patella RENAME README.txt)
	install(FILES "${PROJECT_SOURCE_DIR}/packaging/patella.desktop" DESTINATION share/applications)
	install(FILES "${PROJECT_SOURCE_DIR}/packaging/patella.svg" DESTINATION share/icons/hicolor/scalable/apps)
	set(CPACK_GENERATOR TGZ AppImage)
	set(patella_package_platform linux-x64)
	# The image's GCC 16 is newer than any distribution's: its C++ library is linked in, this target only, so a package
	# needs no libstdc++ of that version. The GCC Runtime Library Exception covers it (licenses/THIRD-PARTY.txt).
	target_link_options(Patella PRIVATE -static-libstdc++ -static-libgcc)
	set(CPACK_STRIP_FILES TRUE)

	patella_fetch_declare(patella_appimagetool
		URL https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage
		URL_HASH SHA256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
		DOWNLOAD_NO_EXTRACT TRUE)
	patella_fetch_declare(patella_appimage_runtime
		URL https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64
		URL_HASH SHA256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
		DOWNLOAD_NO_EXTRACT TRUE)
	patella_fetch_declare(patella_patchelf
		URL https://github.com/NixOS/patchelf/releases/download/0.19.3/patchelf-0.19.3-x86_64.tar.gz
		URL_HASH SHA256=38861b171079e440b25ac86483284f690d0ee7921dba98b0c8fe04a9b0a11667)
	FetchContent_MakeAvailable(patella_appimagetool patella_appimage_runtime patella_patchelf)
	# appimagetool is an AppImage itself, and a container has no FUSE: its own contents, unpacked once.
	set(patella_appimagetool_dir "${PROJECT_BINARY_DIR}/appimagetool")
	set(patella_appimagetool_stamp "${patella_appimagetool_dir}/extracted-1.9.1")
	if(NOT EXISTS "${patella_appimagetool_stamp}")
		file(REMOVE_RECURSE "${patella_appimagetool_dir}")
		file(MAKE_DIRECTORY "${patella_appimagetool_dir}")
		file(COPY_FILE "${patella_appimagetool_SOURCE_DIR}/appimagetool-x86_64.AppImage"
			"${patella_appimagetool_dir}/appimagetool.AppImage")
		file(CHMOD "${patella_appimagetool_dir}/appimagetool.AppImage" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
		execute_process(COMMAND "${patella_appimagetool_dir}/appimagetool.AppImage" --appimage-extract
			WORKING_DIRECTORY "${patella_appimagetool_dir}"
			RESULT_VARIABLE patella_appimagetool_result OUTPUT_QUIET ERROR_VARIABLE patella_appimagetool_error)
		if(NOT patella_appimagetool_result EQUAL 0 OR NOT EXISTS "${patella_appimagetool_dir}/squashfs-root/AppRun")
			message(FATAL_ERROR "appimagetool --appimage-extract failed (${patella_appimagetool_result}):\n${patella_appimagetool_error}")
		endif()
		file(TOUCH "${patella_appimagetool_stamp}")
	endif()
	file(CHMOD "${patella_appimage_runtime_SOURCE_DIR}/runtime-x86_64" "${patella_patchelf_SOURCE_DIR}/bin/patchelf"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
	set(CPACK_APPIMAGE_TOOL_EXECUTABLE "${patella_appimagetool_dir}/squashfs-root/AppRun")
	set(CPACK_APPIMAGE_PATCHELF_EXECUTABLE "${patella_patchelf_SOURCE_DIR}/bin/patchelf")
	set(CPACK_APPIMAGE_RUNTIME_FILE "${patella_appimage_runtime_SOURCE_DIR}/runtime-x86_64")
	set(CPACK_APPIMAGE_DESKTOP_FILE patella.desktop)
	set(CPACK_APPIMAGE_NO_APPSTREAM TRUE)
	set(CPACK_PACKAGE_ICON "patella.svg")
endif()
foreach(entry IN LISTS patella_package_licenses)
	string(REPLACE "=" ";" entry "${entry}")
	list(GET entry 0 target)
	list(GET entry 1 source)
	if(NOT EXISTS "${source}")
		message(FATAL_ERROR "the licence text ${source} is missing")
	endif()
	get_filename_component(target_folder "${target}" DIRECTORY)
	get_filename_component(target_name "${target}" NAME)
	install(FILES "${source}" DESTINATION "${patella_package_docs}/${target_folder}" RENAME "${target_name}")
endforeach()

set(CPACK_PACKAGE_NAME "Patella-Modern")
set(CPACK_PACKAGE_VERSION "${PATELLA_PACKAGE_VERSION}")
set(CPACK_PACKAGE_VENDOR "Patella-Modern contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}. You bring your own Tibia 7.72 data files; the package has none.")
set(CPACK_PACKAGE_FILE_NAME "Patella-Modern-${PATELLA_PACKAGE_VERSION}-${patella_package_platform}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_PRE_BUILD_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake/PackagePreBuild.cmake")
# For cmake/PackagePreBuild.cmake.
set(CPACK_PATELLA_PYTHON "${Python3_EXECUTABLE}")
set(CPACK_PATELLA_CHECK "${PROJECT_SOURCE_DIR}/tools/package_check.py")
include(CPack)
