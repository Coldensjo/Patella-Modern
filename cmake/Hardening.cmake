# The hardening settings of the product targets (notes/modernization-plan.md, section 5; issue #8),
# applied by the presets windows-x64 (issue #5) and linux-x64-gcc / linux-x64-clang (issue #7):
#
#	include(cmake/Hardening.cmake)
#	patella_target_hardening(<target>)
#
# - MSVC: Control Flow Guard (/guard:cf, compiler and linker) and CET shadow stack compatibility
#   (/CETCOMPAT); ASLR and DEP (/DYNAMICBASE /NXCOMPAT) are the linker's defaults. The MSVC STL's hardened mode,
#   _MSVC_STL_HARDENING=1 (since MSVC 14.44, VS 2022 17.14; every container's, string's, span's, mdspan's, optional's
#   and expected's preconditions, a violation __fastfail(5)), for C++ in every configuration (issue #133; Debug's
#   _ITERATOR_DEBUG_LEVEL 2 checks them already).
# - GCC and Clang: -fstack-protector-strong; _FORTIFY_SOURCE=3 where the code is optimised (it needs
#   optimisation and warns without it; any level the compiler predefines is replaced);
#   libstdc++'s hardened mode, _GLIBCXX_ASSERTIONS (the one -fhardened defines; libstdc++ 16 has no other that keeps
#   the ABI, _GLIBCXX_DEBUG changes it), for C++ in every configuration (issue #133; Debug only until then; a
#   violation __glibcxx_assert_fail, an abort); position-independent executables; on Linux full RELRO.
# - PATELLA_SANITIZE (GCC and Clang; the preset linux-x64-clang sets it to address,undefined, plan
#   section 5): -fsanitize=<list> for compiling and linking, every finding fatal
#   (-fno-sanitize-recover=all), frame pointers kept for the reports.
# - PATELLA_SANITIZE with MSVC (issue #70; the preset windows-x64-asan sets it to address): only address,
#   MSVC's AddressSanitizer (/fsanitize=address, compiling and linking; MSVC has no UBSan or LeakSanitizer).
#   Every executable is linked with /DEBUG and /INCREMENTAL:NO (ASan's metadata cannot be linked
#   incrementally) and gets the ASan runtime DLL, clang_rt.asan_dynamic-x86_64.dll (the one runtime of every
#   CRT and configuration since MSVC 14.37: Debug imports it too), copied next to it from the compiler's
#   folder, where Windows looks first (not only on a developer prompt's PATH). /RTC is incompatible: the preset leaves it out
#   of CMAKE_<LANG>_FLAGS_DEBUG, and the configure stops when it is still there.
#
# Until M6 (Patella-Modern issue #94) never for the 32-bit oracle's targets, which kept exactly the former
# Patella.vcxproj's options; the oracle has left the fork.

include_guard(GLOBAL)

include(CheckPIESupported)

set(PATELLA_SANITIZE "" CACHE STRING
	"The sanitizers of the product targets: GCC and Clang as for -fsanitize= (e.g. address,undefined); MSVC only address")

# MSVC's AddressSanitizer: only address, no /RTC in the flags of any configuration, and the runtime DLL next
# to the compiler (checked once).
function(patella_msvc_check_sanitize)
	get_property(checked GLOBAL PROPERTY PATELLA_MSVC_SANITIZE_CHECKED)
	if(checked)
		return()
	endif()
	if(NOT PATELLA_SANITIZE STREQUAL "address")
		message(FATAL_ERROR "PATELLA_SANITIZE=${PATELLA_SANITIZE}: MSVC has only address (/fsanitize=address)")
	endif()
	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		message(FATAL_ERROR "PATELLA_SANITIZE: MSVC's AddressSanitizer only for x64 here (windows-x64-asan)")
	endif()
	foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES ITEMS ${CMAKE_BUILD_TYPE})
		string(TOUPPER "${config}" config)
		foreach(language C CXX)
			if(CMAKE_${language}_FLAGS_${config} MATCHES "/RTC")
				message(FATAL_ERROR "PATELLA_SANITIZE: /fsanitize=address is incompatible with /RTC "
					"(CMAKE_${language}_FLAGS_${config} is \"${CMAKE_${language}_FLAGS_${config}}\"; the preset "
					"windows-x64-asan sets it without)")
			endif()
		endforeach()
	endforeach()
	cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compiler_dir)
	if(NOT EXISTS "${compiler_dir}/clang_rt.asan_dynamic-x86_64.dll")
		message(FATAL_ERROR "PATELLA_SANITIZE: clang_rt.asan_dynamic-x86_64.dll is not next to ${CMAKE_CXX_COMPILER} "
			"(Visual Studio's C++ AddressSanitizer component)")
	endif()
	set_property(GLOBAL PROPERTY PATELLA_MSVC_SANITIZE_CHECKED TRUE)
endfunction()

function(patella_target_hardening target)
	get_target_property(type ${target} TYPE)
	if(MSVC)
		# Options, not compile definitions: patella_target_import_std gives the std module the target's
		# COMPILE_OPTIONS, and MSVC warns (C5050) when a macro of the importer's is not the module's.
		target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/guard:cf>
			$<$<COMPILE_LANGUAGE:CXX>:/D_MSVC_STL_HARDENING=1>)
		if(type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "MODULE_LIBRARY")
			target_link_options(${target} PRIVATE /guard:cf /CETCOMPAT)
		endif()
		if(PATELLA_SANITIZE)
			patella_msvc_check_sanitize()
			target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/fsanitize=address>)
			if(type STREQUAL "EXECUTABLE")
				target_link_options(${target} PRIVATE /DEBUG /INCREMENTAL:NO)
				cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compiler_dir)
				set(runtime "${compiler_dir}/clang_rt.asan_dynamic-x86_64.dll")
				add_custom_command(TARGET ${target} POST_BUILD
					COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${runtime}" "$<TARGET_FILE_DIR:${target}>"
					VERBATIM)
			endif()
		endif()
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(${target} PRIVATE
			$<$<COMPILE_LANGUAGE:C,CXX>:-fstack-protector-strong>
			$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CONFIG:Debug>>>:-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3>
			$<$<COMPILE_LANGUAGE:CXX>:-D_GLIBCXX_ASSERTIONS>)
		check_pie_supported(LANGUAGES C CXX)
		set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
		if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
				AND (type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "MODULE_LIBRARY"))
			target_link_options(${target} PRIVATE LINKER:-z,relro LINKER:-z,now)
		endif()
		if(PATELLA_SANITIZE)
			set(sanitize -fsanitize=${PATELLA_SANITIZE} -fno-sanitize-recover=all)
			target_compile_options(${target} PRIVATE
				$<$<COMPILE_LANGUAGE:C,CXX>:${sanitize} -fno-omit-frame-pointer>)
			target_link_options(${target} PRIVATE ${sanitize})
		endif()
	else()
		message(FATAL_ERROR "patella_target_hardening: no settings for ${CMAKE_CXX_COMPILER_ID}")
	endif()
endfunction()
