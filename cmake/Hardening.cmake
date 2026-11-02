# The hardening settings of the product targets (notes/modernization-plan.md, section 5; issue #8),
# applied by the presets windows-x64 (issue #5) and linux-x64-gcc / linux-x64-clang (issue #7):
#
#	include(cmake/Hardening.cmake)
#	patella_target_hardening(<target>)
#
# - MSVC: Control Flow Guard (/guard:cf, compiler and linker) and CET shadow stack compatibility
#   (/CETCOMPAT); ASLR and DEP (/DYNAMICBASE /NXCOMPAT) are the linker's defaults.
# - GCC and Clang: -fstack-protector-strong; _FORTIFY_SOURCE=3 where the code is optimised (it needs
#   optimisation and warns without it; any level the compiler predefines is replaced);
#   _GLIBCXX_ASSERTIONS in Debug; position-independent executables; on Linux full RELRO.
#
# Never for the oracle-x86 targets: cmake/OracleX86.cmake keeps exactly Patella.vcxproj's options.

include_guard(GLOBAL)

include(CheckPIESupported)

function(patella_target_hardening target)
	get_target_property(type ${target} TYPE)
	if(MSVC)
		target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/guard:cf>)
		if(type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "MODULE_LIBRARY")
			target_link_options(${target} PRIVATE /guard:cf /CETCOMPAT)
		endif()
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(${target} PRIVATE
			$<$<COMPILE_LANGUAGE:C,CXX>:-fstack-protector-strong>
			$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CONFIG:Debug>>>:-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3>
			$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-D_GLIBCXX_ASSERTIONS>)
		check_pie_supported(LANGUAGES C CXX)
		set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
		if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
				AND (type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "MODULE_LIBRARY"))
			target_link_options(${target} PRIVATE LINKER:-z,relro LINKER:-z,now)
		endif()
	else()
		message(FATAL_ERROR "patella_target_hardening: no settings for ${CMAKE_CXX_COMPILER_ID}")
	endif()
endfunction()
