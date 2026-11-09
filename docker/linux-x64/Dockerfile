# The Linux build image of Patella-Modern (issue #7; notes/modernization-plan.md, sections 4.7 and 5):
# GCC 16 and Clang, CMake 4.4.5 and Ninja 1.13.2, for the presets linux-x64-gcc and linux-x64-clang, and
# clang-format and clang-tidy for the checks of CI (issue #11). Every version is pinned: the base image
# by digest, the Debian packages by version, CMake and Ninja by their SHA-256.
#
#	docker build -t patella-modern/linux-x64:gcc16.2-clang22.1-libstdcxx16-cmake4.4.5-ninja1.13.2-sdl3-mesa25.0.7 docker/linux-x64
#
# - GCC 16.2.0: Docker Hub's official gcc image (built from source by the Docker library, installed in
#   /usr/local, libstdc++exp.a included; /usr/local/lib64 is first in ld.so.conf). It is Debian 13
#   (trixie) with buildpack-deps, whose own GCC 14 stays in /usr/bin behind /usr/local/bin.
# - Clang 23.1 with compiler-rt (the sanitizers of linux-x64-clang), clang-format and clang-tidy (issue #143; Debian
#   trixie's 22.1.8 until then, and Debian has no LLVM 23): LLVM's own apt repository, apt.llvm.org, through
#   llvm-apt.sh. It is a rolling snapshot of LLVM's release branch with one version at a time, so the pin is the signing
#   key (its file's SHA-256 and its fingerprint), the suite and the version pattern 1:23.1.*; which snapshot of the
#   branch is not fixed, and the build prints it (notes/decisions.md, "#143"). The one exception to "every version is
#   pinned" above.
# - Clang's GCC installation (issue #121; Debian's GCC 14 until then, Clang's default): GCC 16.2's, so its
#   C++ library is libstdc++ 16, the one of linux-x64-gcc, with its std module (import std) and <mdspan>.
#   Clang's default configuration files next to its executables, /usr/lib/llvm-23/bin/clang++.cfg and
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
# - The downloads (issue #153): fetch.sh, retries of a timeout or 5xx with a pause, for CMake a fallback to Kitware's
#   own site; every file checked against its SHA-256 before it is used. It is only mounted for the RUNs that download,
#   so it is not in the image, and the tag says what is in the image: it stays. RUN --mount needs BuildKit, Docker's
#   default. llvm-apt.sh (issue #143) retries apt's update and install the same way.
#
# When Debian replaces one of the pinned package versions (a point release), the build fails at
# apt-get install: take the new version from `apt-cache policy <package>`, or point the sources at
# http://snapshot.debian.org/archive/debian/<date> of the version, and record the change in
# notes/decisions.md ("The Linux presets and patella_core (issue #7)").

FROM gcc:16.2.0-trixie@sha256:07280eb2e744f1b7a2d8d4a3894c089ea07266c109758afc388a634bd098e4eb

ARG LLVM_MAJOR=23
ARG LLVM_VERSION_PATTERN=1:23.1.*
ARG LLVM_KEY_SHA256=8b2a587ffd672c4687e7581dad4b2f6c1bb2ad6b480cd9771ba2ff48e0b8c75d
ARG LLVM_KEY_FINGERPRINT=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421
RUN --mount=type=bind,source=fetch.sh,target=/tmp/fetch.sh \
	--mount=type=bind,source=llvm-apt.sh,target=/tmp/llvm-apt.sh \
	sh /tmp/llvm-apt.sh /tmp/fetch.sh "${LLVM_KEY_SHA256}" "${LLVM_KEY_FINGERPRINT}" "${LLVM_MAJOR}" "${LLVM_VERSION_PATTERN}" \
	&& ln -s /usr/bin/clang-${LLVM_MAJOR} /usr/local/bin/clang \
	&& ln -s /usr/bin/clang++-${LLVM_MAJOR} /usr/local/bin/clang++ \
	&& ln -s /usr/bin/clang-format-${LLVM_MAJOR} /usr/local/bin/clang-format \
	&& ln -s /usr/bin/clang-tidy-${LLVM_MAJOR} /usr/local/bin/clang-tidy \
	&& clang --version | grep -q "clang version 23\.1\."

# The Linux client (issue #131, plan M8; notes/decisions.md, "#131"): SDL3 3.4.18 is built from its source archive at
# the first configure (cmake/Sdl3.cmake); these are its build dependencies and what the client needs at run time.
# - libx11-dev, libxext-dev, libxcursor-dev, libxi-dev, libxfixes-dev, libxrandr-dev, libxss-dev: SDL3's X11 video driver
#   (SDL loads the libraries with dlopen at run time, so the client does not link them).
# - libwayland-dev, wayland-protocols, libxkbcommon-dev, libegl-dev: its Wayland video driver (wayland-scanner, the
#   protocols; SDL's CMake also asks for egl.pc).
# - libvulkan1: the Vulkan loader (volk opens libvulkan.so.1). mesa-vulkan-drivers: lavapipe, the CPU driver of
#   PATELLA_VULKAN=cpu (Windows has its Mesa next to the executable, cmake/Vulkan.cmake; on Linux it is the system's).
# - xvfb: a virtual X display for the windowed runs without a desktop (tools/linux_session.py; never a real desktop).
RUN apt-get -o Acquire::Retries=5 update 	&& apt-get -o Acquire::Retries=5 install -y --no-install-recommends 		libx11-dev=2:1.8.12-1 		libxext-dev=2:1.3.4-1+b3 		libxcursor-dev=1:1.2.3-1 		libxi-dev=2:1.8.2-1 		libxfixes-dev=1:6.0.0-2+b4 		libxrandr-dev=2:1.5.4-1+b3 		libxss-dev=1:1.2.3-1+b3 		libwayland-dev=1.23.1-3 		wayland-protocols=1.44-1 		libxkbcommon-dev=1.7.0-2 		libegl-dev 		libvulkan1=1.4.309.0-1 		mesa-vulkan-drivers=25.0.7-2+deb13u1 		xvfb=2:21.1.16-1.3+deb13u4 	&& rm -rf /var/lib/apt/lists/*

RUN gcc_dir="$(dirname "$(gcc -print-libgcc-file-name)")" 	&& test "$gcc_dir" = /usr/local/lib/gcc/x86_64-linux-gnu/16.2.0 	&& for driver in clang clang++; do echo "--gcc-install-dir=$gcc_dir" > /usr/lib/llvm-${LLVM_MAJOR}/bin/$driver.cfg; done 	&& clang++ -v -E -x c++ /dev/null 2>&1 | grep -qx "Selected GCC installation: $gcc_dir" 	&& clang -v -E -x c /dev/null 2>&1 | grep -qx "Selected GCC installation: $gcc_dir" 	&& test -f "$(clang++ -print-file-name=libstdc++.modules.json)"

ARG CMAKE_VERSION=4.4.5
ARG CMAKE_SHA256=ed85135c5a5135b45e50423cb406dea04ca3338da0b8ef3c9ede963559680be0
RUN --mount=type=bind,source=fetch.sh,target=/tmp/fetch.sh \
	sh /tmp/fetch.sh "${CMAKE_SHA256}" /tmp/cmake.tar.gz \
		"https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
		"https://cmake.org/files/v${CMAKE_VERSION%.*}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
	&& mkdir /opt/cmake \
	&& tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1 \
	&& rm /tmp/cmake.tar.gz \
	&& for tool in cmake ctest cpack; do ln -s /opt/cmake/bin/$tool /usr/local/bin/$tool; done

ARG NINJA_VERSION=1.13.2
ARG NINJA_SHA256=5749cbc4e668273514150a80e387a957f933c6ed3f5f11e03fb30955e2bbead6
RUN --mount=type=bind,source=fetch.sh,target=/tmp/fetch.sh \
	sh /tmp/fetch.sh "${NINJA_SHA256}" /tmp/ninja.zip \
		"https://github.com/ninja-build/ninja/releases/download/v${NINJA_VERSION}/ninja-linux.zip" \
	&& python3 -I -m zipfile -e /tmp/ninja.zip /usr/local/bin \
	&& chmod 755 /usr/local/bin/ninja \
	&& rm /tmp/ninja.zip \
	&& test "$(ninja --version)" = "${NINJA_VERSION}"

WORKDIR /work
