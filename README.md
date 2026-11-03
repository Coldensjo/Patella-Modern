# Patella-Modern

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

**Today (until milestone M1 is done)**, the build is the one inherited from Patella. It is Windows only and
produces a 32-bit `Patella.exe`. It needs Visual Studio 2026 with the C++ workload and the MFC component,
Python on PATH, and `Tibia.exe` in the root (the resource pictures are extracted from it):

```
tools\build-patella.cmd [Release|Debug]
```

The output is `build\<Configuration>\Patella.exe`. Run it from a folder that holds copies of the data files
(for example `build\run`), never from the root, because the client rewrites `Tibia.cfg` on exit. See
"Building" in [notes/reconstruction.md](notes/reconstruction.md) for the details and for the tests.

The same 32-bit build, with the differential tests, also runs with CMake (issue #4), from a developer
prompt set up for x86 (`vcvarsall.bat x86`):

```
cmake --workflow --preset oracle-x86       # configure, build, test (output in build\oracle-x86)
```

`oracle-x86` is a test instrument until milestone M6, not a product.

The 64-bit Windows client (issue #5) builds from a developer prompt set up for x64 (`vcvarsall.bat x64`),
with Tibia.exe in the root for its resource pictures:

```
cmake --workflow --preset windows-x64      # configure, build Release, run the unit tests
cmake --build --preset windows-x64-debug   # the Debug build
```

The output is `build\windows-x64\<Configuration>\Patella.exe` with its PDB; the same rule applies: run it from a
folder with copies of the data files.

**After M1**, there will be one CMake command per platform (CMake 3.30 or newer, Ninja):

```
cmake --workflow --preset windows-x64      # configure, build, test (Windows)
cmake --workflow --preset linux-x64-gcc    # the same on Linux
```

At that point `Patella.sln`, `Patella.vcxproj` and the `.cmd` build scripts will be removed.

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
