# The pinned downloads of the configure step, with retries (issue #153; notes/decisions.md, "#153").
#
#	patella_fetch_declare(<name> URL <url> URL_HASH SHA256=<hash> <FetchContent_Declare's other options>)
#
# FetchContent_Declare with the same arguments, except that the archive is fetched first, here, and FetchContent gets
# the local file. A GitHub release download answers 504 for hours at times, and CMake's own download (ExternalProject's
# download.cmake) gives up on it at once: it retries only curl's codes 6, 7, 8, 15, 28 and 35, and an HTTP error is
# curl's 22, so a list of URLs or a TIMEOUT option does not help. Nothing else in CMake retries a 5xx.
#
# Retried, with a pause (PATELLA_DOWNLOAD_PAUSE seconds times the attempt, up to six times that): HTTP 408, 429 and
# 5xx, timeouts, a refused or dropped connection, an unfinished transfer. PATELLA_DOWNLOAD_TRIES attempts, 10 by
# default: about 6.5 minutes of pauses. Stopped at once: any other HTTP error (a 404 is a wrong pin) and a SHA-256
# mismatch. The file only gets its final name after its SHA-256 matched, and FetchContent checks URL_HASH on it again.
# Both environment variables are for tools/download_selftest.py.
#
# The files stay in <FETCHCONTENT_BASE_DIR>/patella-downloads/<name>, so a later configure, also a --fresh one, uses
# them without the network. FETCHCONTENT_FULLY_DISCONNECTED and FETCHCONTENT_SOURCE_DIR_<NAME> keep their meaning,
# and with FIND_PACKAGE_ARGS (which must come last, as FetchContent asks) an installed package means no download.

include_guard(GLOBAL)
include(FetchContent)

# Fetches `url` to `path` unless `path` already has the SHA-256 `sha256`; any failure is a fatal error.
function(patella_download url sha256 path)
	if(EXISTS "${path}")
		file(SHA256 "${path}" present)
		if(present STREQUAL sha256)
			return()
		endif()
		message(STATUS "patella_download: ${path} has SHA-256 ${present}, not ${sha256}: fetching it again")
		file(REMOVE "${path}")
	endif()
	set(tries 10)
	if(DEFINED ENV{PATELLA_DOWNLOAD_TRIES})
		set(tries "$ENV{PATELLA_DOWNLOAD_TRIES}")
	endif()
	set(pause 10)
	if(DEFINED ENV{PATELLA_DOWNLOAD_PAUSE})
		set(pause "$ENV{PATELLA_DOWNLOAD_PAUSE}")
	endif()
	get_filename_component(folder "${path}" DIRECTORY)
	file(MAKE_DIRECTORY "${folder}")
	set(part "${path}.part")
	foreach(attempt RANGE 1 ${tries})
		file(REMOVE "${part}")
		file(DOWNLOAD "${url}" "${part}" STATUS status LOG log TIMEOUT 900 INACTIVITY_TIMEOUT 60 TLS_VERIFY ON)
		list(GET status 0 code)
		list(GET status 1 text)
		if(code EQUAL 0)
			file(SHA256 "${part}" actual)
			if(NOT actual STREQUAL sha256)
				file(REMOVE "${part}")
				message(FATAL_ERROR "patella_download: ${url} has SHA-256 ${actual}, not ${sha256}; not retried")
			endif()
			file(RENAME "${part}" "${path}")
			return()
		endif()
		file(REMOVE "${part}")
		# curl's 22 is an HTTP error; the answer's status is the last one of the log, after any redirects.
		set(retry FALSE)
		if(code EQUAL 22)
			string(REGEX MATCHALL "HTTP/[0-9.]+ [0-9][0-9][0-9]" answers "${log}")
			set(http 0)
			if(answers)
				list(GET answers -1 answer)
				string(REGEX REPLACE "^.* " "" http "${answer}")
			endif()
			set(text "HTTP ${http}")
			if(http EQUAL 408 OR http EQUAL 429 OR (http GREATER_EQUAL 500 AND http LESS 600))
				set(retry TRUE)
			endif()
		elseif(code MATCHES "^(5|6|7|16|18|28|35|52|55|56|92)$")
			set(retry TRUE)
		endif()
		if(NOT retry)
			message(FATAL_ERROR "patella_download: ${url}: ${text} (curl ${code}); not retried")
		endif()
		if(attempt EQUAL tries)
			message(FATAL_ERROR "patella_download: ${url}: ${text} (curl ${code}), attempt ${attempt} of ${tries}; giving up")
		endif()
		set(step ${attempt})
		if(step GREATER 6)
			set(step 6)
		endif()
		math(EXPR wait "${pause} * ${step}")
		message(STATUS "patella_download: ${url}: ${text} (curl ${code}), attempt ${attempt} of ${tries}; again in ${wait} s")
		execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep ${wait})
	endforeach()
endfunction()

function(patella_fetch_declare name)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "URL;URL_HASH" "")
	if(NOT arg_URL_HASH MATCHES "^SHA256=([0-9a-fA-F]+)$")
		message(FATAL_ERROR "patella_fetch_declare(${name}): URL_HASH SHA256=<hash> is required")
	endif()
	string(TOLOWER "${CMAKE_MATCH_1}" sha256)
	set(source "${arg_URL}")
	string(TOUPPER "${name}" upper)
	set(installed FALSE)
	list(FIND arg_UNPARSED_ARGUMENTS FIND_PACKAGE_ARGS keyword)
	if(keyword GREATER_EQUAL 0)
		math(EXPR first "${keyword} + 1")
		list(SUBLIST arg_UNPARSED_ARGUMENTS ${first} -1 find_arguments)
		find_package(${name} ${find_arguments} QUIET)
		set(installed "${${name}_FOUND}")
	endif()
	if(NOT installed AND NOT FETCHCONTENT_FULLY_DISCONNECTED AND NOT FETCHCONTENT_SOURCE_DIR_${upper})
		# The file's own name: DOWNLOAD_NO_EXTRACT puts it under that name in the source folder (cmake/Vulkan.cmake).
		string(REGEX REPLACE "[?#].*$" "" file_name "${arg_URL}")
		get_filename_component(file_name "${file_name}" NAME)
		set(source "${FETCHCONTENT_BASE_DIR}/patella-downloads/${name}/${file_name}")
		patella_download("${arg_URL}" "${sha256}" "${source}")
	endif()
	FetchContent_Declare(${name} URL "${source}" URL_HASH "${arg_URL_HASH}" ${arg_UNPARSED_ARGUMENTS})
endfunction()
