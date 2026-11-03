# Patella-Modern

[![hosted](https://github.com/Coldensjo/Patella-Modern/actions/workflows/hosted.yml/badge.svg?branch=main)](https://github.com/Coldensjo/Patella-Modern/actions/workflows/hosted.yml)

Patella-Modern turns [Patella](https://github.com/Coldensjo/Patella), a source reconstruction of the Tibia
7.72 client (`Tibia.exe`, built 2006-06-08 with MSVC 7.1), into a maintainable, portable C++ code base.

Patella aims to be a **faithful copy**: its sources are rebuilt one original file at a time and checked
function by function against the binary. Patella-Modern is **not** a faithful copy. It is a modernization that
started from Patella (forked at df08bac on November 2, 2026): the code is rewritten freely, but the client's
**behaviour does not change**. It sends the same packets, writes the same files and draws the same pictures as
`Tibia.exe`. The few deliberate changes (for example, removing `error.txt` and the built-in patcher) are
each recorded as a decision.

- **Targets:** Windows x64 and Linux x64. A 32-bit build (the `oracle-x86` preset) stays only as a test
  instrument until milestone M6.
- **Language and compilers:** C++23 (C++26 features only behind `src/compat/cxx26.h`); MSVC on Windows,
  GCC 16 and Clang on Linux.
- **Rendering:** Vulkan 1.3 on both platforms. A CPU Vulkan driver is the fallback.
- **Window, input and clipboard:** SDL3 (milestone M8).

## No CipSoft files included

This repository contains **no CipSoft files**: no `Tibia.exe`, `Tibia.dat`, `Tibia.spr`, `Tibia.pic` or
`Tibia.cfg`, no data derived from them (golden data, reference recordings) and no game server. Nothing built
from it (packages, releases) contains them either. To build or run the client, you need **your own copy of
the Tibia 7.72 client files**. The build and the tests expect them in the repository root, where they are
gitignored.

## Building

The build is CMake (3.30 or newer) with Ninja, one workflow preset per platform. On Windows it needs Visual
Studio 2026 with the C++ workload and the MFC component, Python 3.13 on PATH, and `Tibia.exe` in the root
(the resource pictures are extracted from it). The MSVC presets need the compiler set up (`vcvarsall.bat`);
`tools/build.py` does that and runs the workflow from any shell (a developer prompt is not needed):

```
python -I tools/build.py windows-x64         # configure, build Release, run the unit tests
python -I tools/build.py windows-x64-debug   # the same for Debug
python -I tools/build.py windows-x64-gcc     # the unit tests with mingw-w64 GCC 16
python -I tools/build.py linux-x64-gcc       # the portable library and its tests on Linux, GCC 16
python -I tools/build.py linux-x64-clang     # the same with Clang, AddressSanitizer and UBSan
python -I tools/build.py oracle-x86          # the 32-bit oracle and the differential tests
```

From a developer prompt (or for the GCC preset from any shell) `cmake --workflow --preset <name>` does the
same; `python -I tools/build.py --env <preset> <command>` runs any other command (`cmake --build`, `ctest`)
with that preset's compiler set up.

The 64-bit client is `build\windows-x64\<Configuration>\Patella.exe` with its PDB. `oracle-x86` builds today's
32-bit `Patella.exe` (`build\oracle-x86\<Configuration>`) with the original's flags and runs the differential
tests; it is a test instrument until milestone M6, not a product. Run either client from a folder that holds
copies of the data files (for example `build\run`), never from the root, because the client rewrites
`Tibia.cfg` on exit. See "Building" in [notes/reconstruction.md](notes/reconstruction.md) for the details and
for the tests.

On Linux there is no client yet: the presets `linux-x64-gcc` and `linux-x64-clang` build `patella_core`, the
library of the code that is already portable (big numbers and RSA, XTEA, the string helpers and packet
buffers, the regexp, the container templates), and run its unit tests. They need no CipSoft file. On Linux,
`cmake --workflow --preset linux-x64-gcc` (GCC 16, CMake 3.30+, Ninja); on Windows, `tools/build.py` runs them
in a Docker image with GCC 16.2, Clang 22, CMake and Ninja, which you build once from
[docker/linux-x64/Dockerfile](docker/linux-x64/Dockerfile):

```
docker build -t patella-modern/linux-x64:gcc16.2-clang22.1 docker/linux-x64
```

CI (GitHub Actions): `.github/workflows/hosted.yml` builds and tests the Windows (MSVC x64) and Linux (GCC 16,
Clang 22) presets and checks clang-format and clang-tidy on every push and pull request, without any CipSoft
file. The checks that need the CipSoft files (the oracle, the differential tests, the comparisons against the
test server) are in `.github/workflows/self-hosted.yml`, for the maintainer's own machine; they never run for
pull requests. Details: "Checking" in [notes/reconstruction.md](notes/reconstruction.md).

## Documents

- [notes/modernization-plan.md](notes/modernization-plan.md): the plan. It covers milestones M0 to M9, the
  test strategy, the equivalence contract (what "behaviour does not change" means exactly) and the decisions.
  It is the source of truth.
- [docs/modernization-explained.md](docs/modernization-explained.md): the same plan in plain language,
  written for readers who are not programmers.
- [notes/decisions.md](notes/decisions.md): every judgement call, including the modernization decisions D1
  to D17.
- [notes/reconstruction.md](notes/reconstruction.md) and [notes/progress.md](notes/progress.md): how the
  reconstruction, the build and the tests work, and where they stand.
- `src/`: the sources. `tools/`: the build, the differential tests (`tools/difftest`) and the comparisons of
  the running client against the original (traffic, written files, rendering, network).

## Keeping up with upstream

Patella (`upstream`) keeps fixing the reconstruction. **Until milestone M6**, while the structure layouts
are frozen in the `oracle-x86` build, `upstream/main` is merged into this repository regularly:

```
git fetch upstream
git merge upstream/main
```

After M6 (tag `layout-frozen-final`), the layouts and the code have moved too far apart for a merge.
Upstream fixes are then ported by hand.

The decompilation workspace (the decompiler output, the name and symbol databases, the analysis notes and
tools) belongs to upstream and is not part of this repository.
