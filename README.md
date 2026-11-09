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

- **Targets:** Windows x64 and Linux x64. There is no 32-bit client build: the 32-bit test instrument (the
  oracle) left the fork in milestone M6 (tag `layout-frozen-final`). The only 32-bit build is `capture-x86`, the
  capture DLL the comparisons load into the original `Tibia.exe`.
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
Studio 2026 with the C++ workload, Python 3.13 on PATH, and `Tibia.exe` in the root
(the resource pictures are extracted from it). The MSVC presets need the compiler set up (`vcvarsall.bat`);
`tools/build.py` does that and runs the workflow from any shell (a developer prompt is not needed):

```
python -I tools/build.py windows-x64         # configure, build Release, run the unit tests
python -I tools/build.py windows-x64-debug   # the same for Debug
python -I tools/build.py windows-x64-gcc     # the unit tests with mingw-w64 GCC 16
python -I tools/build.py linux-x64-gcc       # the portable library and its tests on Linux, GCC 16
python -I tools/build.py linux-x64-clang     # the same with Clang, AddressSanitizer and UBSan
python -I tools/build.py capture-x86         # patcap.dll, the 32-bit capture DLL for Tibia.exe
```

From a developer prompt (or for the GCC preset from any shell) `cmake --workflow --preset <name>` does the
same; `python -I tools/build.py --env <preset> <command>` runs any other command (`cmake --build`, `ctest`)
with that preset's compiler set up.

The 64-bit client is `build\windows-x64\<Configuration>\Patella.exe` with its PDB. `capture-x86` builds only
`build\capture-x86\Release\patcap.dll`, which the comparisons and the reference recordings load into `Tibia.exe`;
it is a test instrument, not a product. Run the client from a folder that holds
copies of the data files (for example `build\run`), never from the root, because the client writes its settings
(`Patella.ini`) and the automap next to the data. The client finds its data files (issue #130) in the working folder,
else next to `Patella.exe`, else in the folder of `--data <folder>` (which comes first) or of `DataFolder=<folder>` in
a `Patella.ini`; `Patella.ini` stays next to the data. See "Building" in
[notes/reconstruction.md](notes/reconstruction.md) for the details and for the tests.

The buttons that open a web page can go somewhere else (issue #169): add these keys to `Patella.ini`, one per line as
`Key=https://your.address/`. The client never writes them, and without them every button goes where it always did.

| Key | Button |
| --- | --- |
| `LinkWebsite` | Info: "www.tibia.com" |
| `LinkManual` | Help: "Manual" |
| `LinkFAQ` | Help: "FAQ" |
| `LinkOptionsHelp` | General Options: "Help" |
| `LinkGraphicsHelp` | Graphics Options and Advanced Graphics: "Help" |
| `LinkConsoleHelp` | Console Options: "Help" |
| `LinkNetworkHelp` | Network Options: "Help" |
| `LinkAccount` | Title screen: "Access Account" |
| `LinkSignup` | Login: "Create Account" |
| `LinkWebsiteLabel` | The caption of Info's button, in place of "www.tibia.com"; cut to fit the button |

An address must start with `http://` or `https://` and hold no spaces or control characters. Any other value is ignored
with a warning in the log, and the button keeps its original address, so a setting can never start a program.

On Linux there is no client yet: the presets `linux-x64-gcc` and `linux-x64-clang` build `patella_core`, the
library of the code that is already portable (big numbers and RSA, XTEA, the string helpers and packet
buffers, the regexp, the container templates), and run its unit tests. They need no CipSoft file. On Linux,
`cmake --workflow --preset linux-x64-gcc` (GCC 16, CMake 3.30+, Ninja); on Windows, `tools/build.py` runs them
in a Docker image with GCC 16.2, Clang 23, CMake 4.4.5 and Ninja 1.13.2, which you build once from
[docker/linux-x64/Dockerfile](docker/linux-x64/Dockerfile):

```
docker build -t patella-modern/linux-x64:gcc16.2-clang23.1-libstdcxx16-cmake4.4.5-ninja1.13.2-sdl3-mesa25.0.7 docker/linux-x64
```

CI (GitHub Actions): `.github/workflows/hosted.yml` builds and tests the Windows (MSVC x64) and Linux (GCC 16,
Clang 23) presets and checks clang-format and clang-tidy on every push and pull request, without any CipSoft
file. The checks that need the CipSoft files (the golden corpus, the reference recordings, the comparisons
against the test server) are in `.github/workflows/self-hosted.yml`, for the maintainer's own machine; they never run for
pull requests. Details: "Checking" in [notes/reconstruction.md](notes/reconstruction.md).

## Documents

- [notes/modernization-plan.md](notes/modernization-plan.md): the plan. It covers milestones M0 to M9, the
  test strategy, the equivalence contract (what "behaviour does not change" means exactly) and the decisions.
  It is the source of truth.
- [docs/modernization-explained.md](docs/modernization-explained.md): the same plan in plain language,
  written for readers who are not programmers.
- [docs/modernization-changes.md](docs/modernization-changes.md): every major modernization done so far, in plain
  language: what changed, what it gives, how it modernizes the code.
- [notes/decisions.md](notes/decisions.md): every judgement call, including the modernization decisions D1
  to D17.
- [notes/reconstruction.md](notes/reconstruction.md) and [notes/progress.md](notes/progress.md): how the
  reconstruction, the build and the tests work, and where they stand.
- `src/`: the sources. `tests/`: the unit tests and the golden corpus's runner. `tools/`: the build and the
  comparisons of the running client against the original (traffic, written files, rendering, network).

## Keeping up with upstream

Patella (`upstream`) keeps fixing the reconstruction. Until milestone M6, while the structure layouts were
frozen in the 32-bit oracle build, `upstream/main` was merged into this repository regularly. Since M6 (November 7,
2026; tag `layout-frozen-final`, the last commit with the oracle) the layouts and the code move apart, and upstream
fixes are ported by hand.

The decompilation workspace (the decompiler output, the name and symbol databases, the analysis notes and
tools) belongs to upstream and is not part of this repository.

## License

MIT, see [LICENSE](LICENSE); [NOTICE](NOTICE) gives its scope. It covers this repository's own work; `src/Extern` keeps its own licenses, and
Tibia and its files belong to CipSoft GmbH and are not licensed here.
