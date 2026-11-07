# The Linux build image of Patella-Modern (issue #7; notes/modernization-plan.md, sections 4.7 and 5):
# GCC 16 and Clang, CMake 4.4.5 and Ninja 1.13.2, for the presets linux-x64-gcc and linux-x64-clang, and
# clang-format and clang-tidy for the checks of CI (issue #11). Every version is pinned: the base image
# by digest, the Debian packages by version, CMake and Ninja by their SHA-256.
#
#	docker build -t patella-modern/linux-x64:gcc16.2-clang22.1-libstdcxx16-cmake4.4.5-ninja1.13.2 docker/linux-x64
#
# - GCC 16.2.0: Docker Hub's official gcc image (built from source by the Docker library, installed in
#   /usr/local, libstdc++exp.a included; /usr/local/lib64 is first in ld.so.conf). It is Debian 13
#   (trixie) with buildpack-deps, whose own GCC 14 stays in /usr/bin behind /usr/local/bin.
# - Clang 22.1.8 with compiler-rt (the sanitizers of linux-x64-clang), clang-format and clang-tidy
#   22.1.8 (issue #11): Debian trixie's packages.
# - Clang's GCC installation (issue #121; Debian's GCC 14 until then, Clang's default): GCC 16.2's, so its
#   C++ library is libstdc++ 16, the one of linux-x64-gcc, with its std module (import std) and <mdspan>.
#   Clang's default configuration files next to its executables, /usr/lib/llvm-22/bin/clang++.cfg and
#   clang.cfg, say --gcc-install-dir=/usr/local/lib/gcc/x86_64-linux-gnu/16.2.0: part of the compiler, so
#   every caller has it, CMake's probe for the std module (`clang++ -print-file-name=libstdc++.modules.json`,
#   which passes no CMAKE_CXX_FLAGS) and clang-tidy (its driver reads the same file) as well as the builds.
#   The RUN checks that clang++ selects that installation and finds the module's description.
# - CMake 4.4.5 (issue #141; 4.1.2 since issue #113, Debian's CMake 3.31.6 before): Kitware's release for Linux
#   x86_64, checked against its SHA-256 (cmake-4.4.5-SHA-256.txt of the release), in /opt/cmake, linked into
#   /usr/local/bin. The version of the Windows builds, local and hosted (one version everywhere); 3.31 has no
#   `import std` for GCC and only libc++'s for Clang (cmake/ImportStd.cmake, notes/decisions.md, "import std
#   (issue #113)"). GCC 16.2's std module (bits/std.cc, libstdc++.modules.json) comes with the gcc image.
# - Ninja 1.13.2 (issue #141; Debian trixie's 1.12.1 until then, and Debian has no 1.13 package): the ninja-build
#   project's release binary for Linux (ninja-linux.zip), checked against the SHA-256 GitHub publishes for the
#   release asset, in /usr/local/bin. The version of the Windows builds.
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
	&& rm -rf /var/lib/apt/lists/* \
	&& ln -s /usr/bin/clang-22 /usr/local/bin/clang \
	&& ln -s /usr/bin/clang++-22 /usr/local/bin/clang++ \
	&& ln -s /usr/bin/clang-format-22 /usr/local/bin/clang-format \
	&& ln -s /usr/bin/clang-tidy-22 /usr/local/bin/clang-tidy

RUN gcc_dir="$(dirname "$(gcc -print-libgcc-file-name)")" 	&& test "$gcc_dir" = /usr/local/lib/gcc/x86_64-linux-gnu/16.2.0 	&& for driver in clang clang++; do echo "--gcc-install-dir=$gcc_dir" > /usr/lib/llvm-22/bin/$driver.cfg; done 	&& clang++ -v -E -x c++ /dev/null 2>&1 | grep -qx "Selected GCC installation: $gcc_dir" 	&& clang -v -E -x c /dev/null 2>&1 | grep -qx "Selected GCC installation: $gcc_dir" 	&& test -f "$(clang++ -print-file-name=libstdc++.modules.json)"

ARG CMAKE_VERSION=4.4.5
ARG CMAKE_SHA256=ed85135c5a5135b45e50423cb406dea04ca3338da0b8ef3c9ede963559680be0
RUN curl -fsSL -o /tmp/cmake.tar.gz \
		"https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
	&& echo "${CMAKE_SHA256}  /tmp/cmake.tar.gz" | sha256sum -c - \
	&& mkdir /opt/cmake \
	&& tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1 \
	&& rm /tmp/cmake.tar.gz \
	&& for tool in cmake ctest cpack; do ln -s /opt/cmake/bin/$tool /usr/local/bin/$tool; done

ARG NINJA_VERSION=1.13.2
ARG NINJA_SHA256=5749cbc4e668273514150a80e387a957f933c6ed3f5f11e03fb30955e2bbead6
RUN curl -fsSL -o /tmp/ninja.zip \
		"https://github.com/ninja-build/ninja/releases/download/v${NINJA_VERSION}/ninja-linux.zip" \
	&& echo "${NINJA_SHA256}  /tmp/ninja.zip" | sha256sum -c - \
	&& python3 -I -m zipfile -e /tmp/ninja.zip /usr/local/bin \
	&& chmod 755 /usr/local/bin/ninja \
	&& rm /tmp/ninja.zip \
	&& test "$(ninja --version)" = "${NINJA_VERSION}"

WORKDIR /work
