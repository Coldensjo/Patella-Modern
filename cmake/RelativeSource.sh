#!/bin/sh
# The compiler launcher of the Clang sanitizer preset (cmake/Reproducible.cmake, issue #136): AddressSanitizer records each
# source as its module's name, the path given on the command line, which -ffile-prefix-map does not reach. The launcher
# rewrites the argument of -c to a path relative to the build folder (Ninja's working directory), which does not depend on
# where the repository is.
count=$#
previous=
while [ "$count" -gt 0 ]; do
	argument=$1
	shift
	if [ "$previous" = "-c" ] && [ "${argument#/}" != "$argument" ]; then
		argument=$(realpath --relative-to=. "$argument")
	fi
	set -- "$@" "$argument"
	previous=$argument
	count=$((count - 1))
done
exec "$@"
