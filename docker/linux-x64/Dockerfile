# The Linux build image of Patella-Modern (issue #7; notes/modernization-plan.md, sections 4.7 and 5):
# GCC 16 and Clang, CMake 4.1.2 and Ninja, for the presets linux-x64-gcc and linux-x64-clang, and
# clang-format and clang-tidy for the checks of CI (issue #11). Every version is pinned: the base image
# by digest, the Debian packages by version, CMake by its SHA-256.
#
#	docker build -t patella-modern/linux-x64:gcc16.2-clang22.1 docker/linux-x64
#
# - GCC 16.2.0: Docker Hub's official gcc image (built from source by the Docker library, installed in
#   /usr/local, libstdc++exp.a included; /usr/local/lib64 is first in ld.so.conf). It is Debian 13
#   (trixie) with buildpack-deps, whose own GCC 14 stays in /usr/bin behind /usr/local/bin.
# - Clang 22.1.8 with compiler-rt (the sanitizers of linux-x64-clang), clang-format and clang-tidy
#   22.1.8 (issue #11) and Ninja 1.12.1: Debian trixie's packages. Clang uses Debian's
#   GCC 14 installation, so its C++ library is libstdc++ 14 (its default); at run time the loader finds
#   GCC 16's newer libstdc++.so.6 first, which is compatible.
# - CMake 4.1.2 (issue #113; Debian's CMake 3.31.6 until then): Kitware's release for Linux x86_64, checked
#   against its SHA-256, in /opt/cmake, linked into /usr/local/bin. The version of the Windows builds; 3.31 has
#   no `import std` for GCC and only libc++'s for Clang (cmake/ImportStd.cmake, notes/decisions.md, "import std
#   (issue #113)"). GCC 16.2's std module (bits/std.cc, libstdc++.modules.json) comes with the gcc image.
#
# When Debian replaces one of the pinned package versions (a point release), the build fails at
# apt-get install: take the new version from `apt-cache policy <package>`, or point the sources at
# http://snapshot.debian.org/archive/debian/<date> of the version, and record the change in
# notes/decisions.md ("The Linux presets and patella_core (issue #7)").

FROM gcc:16.2.0-trixie@sha256:07280eb2e744f1b7a2d8d4a3894c089ea07266c109758afc388a634bd098e4eb

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		clang-22=1:22.1.8-1~deb13u4 \
		libclang-rt-22-dev=1:22.1.8-1~deb13u4 \
		clang-format-22=1:22.1.8-1~deb13u4 \
		clang-tidy-22=1:22.1.8-1~deb13u4 \
		ninja-build=1.12.1-1 \
	&& rm -rf /var/lib/apt/lists/* \
	&& ln -s /usr/bin/clang-22 /usr/local/bin/clang \
	&& ln -s /usr/bin/clang++-22 /usr/local/bin/clang++ \
	&& ln -s /usr/bin/clang-format-22 /usr/local/bin/clang-format \
	&& ln -s /usr/bin/clang-tidy-22 /usr/local/bin/clang-tidy

ARG CMAKE_VERSION=4.1.2
ARG CMAKE_SHA256=773cc679c3a7395413bd096523f8e5d6c39f8718af4e12eb4e4195f72f35e4ab
RUN curl -fsSL -o /tmp/cmake.tar.gz \
		"https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
	&& echo "${CMAKE_SHA256}  /tmp/cmake.tar.gz" | sha256sum -c - \
	&& mkdir /opt/cmake \
	&& tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1 \
	&& rm /tmp/cmake.tar.gz \
	&& for tool in cmake ctest cpack; do ln -s /opt/cmake/bin/$tool /usr/local/bin/$tool; done

WORKDIR /work
