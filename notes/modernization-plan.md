# Patella-Modern: modernization plan

Patella-Modern is a fork of Patella (https://github.com/Coldensjo/Patella, forked at df08bac on November 2,
2026). Patella stays the faithful reconstruction of Tibia.exe 7.72, checked function by function against the
binary. This fork turns that reconstruction into a maintainable, portable C++ code base:

- **Targets: Windows x64 and Linux x64.** No 32-bit product build. A 32-bit build survived only as a test
  instrument until milestone M6 (see 1.2); since M6 (November 6, 2026, issue #94) the only 32-bit build is
  `capture-x86`, the capture DLL the tools load into Tibia.exe.
- **Build: CMake** (presets, Ninja), on both platforms. `Patella.sln` / `Patella.vcxproj` go away.
- **Rendering: Vulkan**, one engine on both platforms (section 6). The three original
  engines (DirectDraw 5, Direct3D 9, OpenGL 1.x through WGL) are Windows-only and are retired.
- **Behaviour does not change:** the client sends the same packets, writes the same files and draws the same
  pictures as Tibia.exe (section 3 says exactly what "same" means).

A plain-language summary for non-programmers: [docs/modernization-explained.md](../docs/modernization-explained.md).

The order of the work is set by the reconstruction's test harness: what it can check, on which platform, and
when each part of it can be replaced.

---

## 1. Where the code stands (measured on df08bac)

| What | Count / state |
|---|---|
| Reconstructed sources | 76 files in `src/`, 46,104 lines; 1,846 functions with their original address |
| Globals | about 262 with their original address; 485 file-scope `static` declarations in all |
| Error macros | 1,174 `TBEGIN`, 1,156 `TEND*`, 942 `TASSERT*`/`TERROR*`; 2,089 sites carry the original's line number |
| Assertion texts that contain `NULL` | 262 (stringized into the error message, see 4.2) |
| Raw memory | 608 `new`, 193 `delete`, 24 `malloc`/`free` calls |
| C strings | 125 fixed `char[]` buffers, 89 `strcpy`/`strcat`, 13 `strncpy`/`strncat`, 78 `sprintf`, 52 `_snprintf` |
| Other legacy constructs | 1,527 `NULL`, about 228 C-style casts, 79 `#define`s, 18 `goto`, 17 `BOOL` |
| Containers | CipSoft's own `vector<T>` (min..max indexed, grows in blocks) and `array<T>` in `Container.h` (13 instances), `matrix3d`; `std::` only for strings and streams (138 uses) |
| Windows dependencies | MFC (static, ANSI: `CWinApp`, `CFrameWnd`, `CWnd`, `CAsyncSocket`, `AfxSocketInit`, message maps with `ON_WM_KEYDOWN`/`ON_WM_CHAR`...); Win32 in 7 .cpp files (Control, MainFrm, Network, Player and the four surface files): `SetCapture`/`ReleaseCapture` (64 calls), `GetClientRect`/`ClientToScreen` (37), `GetSystemMetrics` (12), `EnumDisplaySettings`/`ChangeDisplaySettings`, the clipboard, `ShellExecute`, `MessageBox`, `SetTimer`, `timeGetTime`, `Sleep` |
| Network | `CAsyncSocket`, `WSAAsyncGetHostByName` (IPv4 only, deprecated) |
| Rendering | `TSurface` (`Surface.h`): a 2D blitting interface of 37 virtual slots: fills, lines, frames, image uploads, colour-keyed and stretched blits between numbered surfaces (a video cache for sprites), and `ShadeRectangles` (the light: rectangles with a colour per corner). Three engines: DirectDraw 5 (with Direct3D 7 for the light), Direct3D 9 (loaded at run time), OpenGL 1.x immediate mode with `glh` extensions |
| Crypto | `vlong` big integers + `TRSA` (raw RSA, as the protocol requires), `TXTEA` |
| Third party | Henry Spencer's regexp (C), `glh_genext.h` |
| Build | `Patella.vcxproj`, v145, `/std:c++17`, `/W3`, x86 only, `/MT`, warnings fail the build |
| Tests | `tools/difftest` (about 40,000 lines; 16.8 million checks against the original machine code), `vlongtest`, the black-box comparisons `compare_traffic.py`, `compare_files.py`, `compare_rendering.py`, `compare_network.py`, `compare_resources.py`, `compare_imports.py` |

### 1.1 Toolchains (probed on this machine, November 2, 2026)

| Feature | MSVC 19.51.36260 (`/std:c++latest`) | GCC 16.2 (mingw-w64 UCRT build, `-std=c++26`; same front end and libstdc++ as GCC 16 on Linux) |
|---|---|---|
| `std::format`, `std::print`, `std::expected`, `std::flat_map`, deducing `this`, `std::ranges::to` | yes | yes |
| `std::mdspan` | yes | yes |
| `std::stacktrace` | yes | not in the default library (libstdc++ ships it in `libstdc++exp`; check per distribution) |
| `import std;` | yes | no feature macro in this build |
| Contracts (`pre`, `post`, `contract_assert`) | no | yes (`__cpp_contracts` 202502) |
| Static reflection | no | yes, with `-freflection` (`__cpp_impl_reflection` 202603) |
| `#embed` | no | yes |
| `std::inplace_vector`, saturation arithmetic, pack indexing | no | yes |

Also available: CMake 4.1.2, Ninja 1.13.2, `clang-tidy` and `clang-format` (Visual Studio's LLVM component;
`clang-cl` is not installed), Docker Desktop (no WSL distribution apart from `docker-desktop`).

Consequence: **C++23 is the common baseline. C++26 features are used through a thin compatibility layer**
(`src/compat/cxx26.h`) with feature-test macros: the real feature where the compiler has it, a C++23
stand-in elsewhere. When MSVC catches up, the stand-ins are deleted. Decision D6 (section 9) chooses whether
Windows should be built with GCC or Clang instead, to get C++26 on both platforms now.

### 1.2 What ties the code to 32-bit x86 and to the original's memory layout

- `difftest` maps Tibia.exe (a 32-bit image) into its own process and calls the original machine code with
  our objects. That needs a **32-bit MSVC build** with the original's structure layouts (its tests
  `static_assert` the sizes: `TMapPoint` 0xac, `TAutomapBlock` 0x200a8, ...) and MSVC's ABI and exception
  model.
- `tools/client_state.py` reads Patella.exe's memory through the PDB with **the original's field offsets**.
- `tools/capture/patcap.c` captures frames by hooking the three original engines, in 32-bit processes.

So the plan keeps one preset, **`oracle-x86`**, which builds today's 32-bit client and the difftest exactly
as now. It is never shipped. It disappears in M6, once the golden corpus and the client's own test mode
(section 4) check everything it checked. Until then, structure layouts are frozen **in the oracle build**.
The x64 and Linux builds have their natural layouts from the start, so every check that runs on them
must not depend on layout.

Done in M6 (November 6, 2026; issue #94): the oracle, difftest and the x86 `static_assert`s left the fork;
client_state.py reads Tibia.exe only, and patcap.c is built for Tibia.exe alone by the preset `capture-x86`.

---

## 2. Rules for the fork

1. **Behaviour is the invariant.** Every change is a refactoring. Where a contract is relaxed on purpose
   (section 3), that is its own pull request with a decision record in `notes/decisions.md`.
2. **Every pull request passes the gates** (4.1). A gate is never weakened to let a change through.
3. **Mechanical before semantic:** clang-tidy fix-its, one check per pull request, come before hand
   redesigns.
4. **The layout stays frozen in `oracle-x86` until M6.** Before then, a member may only become a type of
   the same size and alignment on x86, checked by a `static_assert` that is active in the oracle build:
   `char[N]` to `TFixedString<N>`, `T*` to `std::unique_ptr<T>`, `T[N]` to `std::array<T, N>`, an enum to an
   `enum class` of the same underlying type.
5. **Small steps:** one subsystem or one check per pull request, each revertible on its own. One GitHub
   issue per task, as in the repository's issue-loop workflow.
6. **Style stays:** TitleCase functions and members, `T`-prefixed classes, tabs. `.clang-format` encodes
   it. The additions are in section 8.
7. **Upstream sync:** until M6, merge `upstream/main` regularly (reconstruction fixes still apply). After M6,
   port them by hand.

---

## 3. The equivalence contract (decision D1)

| Tier | Artefact | Checked by | Kept |
|---|---|---|---|
| A | Packets sent and their order; the client state the server's packets lead to | `compare_traffic.py`, and the reference recordings on Linux (4.4) | always, on every target |
| A | Files the client writes: the options (Tibia.cfg; since #45 the x64 client's Patella.ini, compared with Tibia.exe's Tibia.cfg by value, D12), `*.map`, saved channels, patched files (until #75) | `compare_files.py`, reference recordings (`tools/settings_file.py` for the options) | always |
| A | Rendered frames | `compare_rendering.py` and the reference frames (4.5) | always; exact, apart from the light overlay (6.1) |
| A | User-visible text, dialogs, keyboard and mouse handling | the GUI tree of the state dump, frames | always, apart from deliberate changes (the engine buttons become a graphics device choice, 6.3) |
| B | Function-level results against the original machine code | `difftest` (oracle-x86), then the golden corpus | difftest until M6 (gone with #94); the golden corpus (frozen, #93) remains, on every target |
| B | Structure layout and field offsets | `static_assert`s and `client_state.py` in oracle-x86 | until M6: the x86 `static_assert`s gone with #94; the file formats' sizes are checked at their serialisers (the automap file's layers, Map.cpp) |
| C | `error.txt`, the error report dialog and packet 0xE8 | `lines.py`, `compare_files.py`, `compare_traffic.py` | until M5, then removed (D2) |
| C | Import table, resource section byte for byte, no manifest | `compare_imports.py`, `compare_resources.py` | oracle-x86 only, until M6 (gone with #94). The product builds are not compared |

Tier A is the product, and it does not change. Tiers B and C are scaffolding from the reconstruction. Each
can be given up at the named milestone, once something else checks what it used to check.

---

## 4. Test strategy

### 4.1 Gates for every pull request

1. CMake presets `windows-x64` (Debug, Release), `oracle-x86` (until M6; `capture-x86` since), and from M1
   `linux-x64-gcc` / `linux-x64-clang` for whatever already builds there: no warnings.
2. Until M6 (issue #94): `oracle-x86`: `difftest` passes and **its check counts are unchanged**; `lines.py check`
   (until M5); `compare_imports.py` and `compare_resources.py` against Tibia.exe. Since M6 the golden corpus
   (gate 5) has the oracle's counts: 15,803,218 replayed + 1,512,822 retired = 17,316,040, 0 failures.
3. Tier A on Windows x64: `compare_traffic.py`, `compare_files.py`: 0 differences. `compare_rendering.py`
   (or, from M2, the reference frames) on every pull request that touches drawing, GUI or surfaces; the full
   matrix before a milestone closes.
4. From the point where the Linux client runs (M8): tier A against the reference recordings on Linux.
5. Unit tests and the golden corpus on every preset.
6. `clang-tidy` with the fork's configuration, with no new findings; `clang-format` check.
7. From M4: AddressSanitizer (MSVC `/fsanitize=address`, GCC/Clang `-fsanitize=address,undefined`) over the
   replayed sessions.

### 4.2 The stringized-expression trap

`TASSERT(Line, Expr)` stringizes `Expr` into the exception message, which `error.txt` records and difftest
compares. Rewriting `TASSERT(132, Address!=NULL)` as `Address!=nullptr` changes the message, and 262
assertions contain `NULL` (more contain casts or macros). **Until M5, no rewrite touches a macro argument of
`TASSERT*`, `TERROR*` or `TEND*`.** The clang-tidy wrapper reverts hunks inside those macros; `lines.py` and
difftest catch the rest.

Retired on November 5, 2026 (issue #76, notes/decisions.md): difftest and the golden corpus compare the error
paths by outcome only (M5 below), the corpus records no exception texts, and tools/tidy.py keeps no macro
arguments apart any more; macrocheck (an uncommitted helper of issue #8) is retired with the rule. `lines.py`
went with issue #74.

### 4.3 The golden corpus (by M6)

Turn difftest's knowledge into data that needs neither Tibia.exe nor x86:

- A record mode in the difftest harness writes, for every comparison, the inputs and the original's outputs
  (return values, the changed globals, written files, exception kind and text) **by value** (a field name
  and its value, never a memory image) to `build/golden/<test>.bin`. Use the full random runs with fixed
  seeds.
- `tests/golden/`: a portable runner (x64 Windows and Linux) that replays the records against the modern
  code through its public functions.
- Golden data derived from Tibia.dat is CipSoft's data. It stays out of the repository (generated locally,
  or kept in private storage). The format and the runner are in the repository.

### 4.4 A deterministic test mode in the client (M2)

The comparisons drive Windows clients from outside: `login_client.py` sends window input, `patcap.dll`
captures frames and makes time virtual, and `client_state.py` reads memory. None of that works on Linux or
survives a layout change. The modern client gets the same abilities built in, compiled only into test
builds:

- **A virtual clock:** one `IClock` behind `timeGetTime`, the wall clock and the frame timer.
- **Scripted input:** a script of keyboard, mouse and text events at virtual times (the sessions of
  `compare_traffic.py` and `compare_files.py`, written as data).
- **A state dump:** the JSON that `client_state.py`'s `state()` and `gui()` produce today, written by the
  client at script steps.
- **A packet log** (plaintext, before encryption) and **a frame dump** (the Vulkan engine's off-screen canvas, read back; 6.2).

On the Tibia.exe side, the existing tools record **reference recordings** once per session on Windows: the
server's packets, the client's packets, state, frames and written files. The modern client replays the
server side of a recording, on Windows or Linux, without a server, and is compared with the recording.
`compare_traffic.py` and the others become two-step tools: record (Windows, Tibia.exe) and compare (any
platform, the modern client). The live test server is still used for new recordings.

As an interim step for Windows x64 before the test mode exists, `client_state.py` reads **field offsets
from the PDB's type information** (dbghelp `SymGetTypeInfo`) instead of using the original's constants.
That works for x86 and x64 as long as the field names stay the same.

### 4.5 Reference frames

`compare_rendering.py` records Tibia.exe's frames per engine. The Vulkan engine, on the pinned CPU Vulkan
driver, is compared with the DirectX 9 frames (D7): exactly outside the light overlay, within a documented
tolerance inside it (6.1).
The comparison runs on Linux too, from the stored frames.

### 4.6 Unit tests

New pure layers (packet framing, the string types, the container replacements, the config parser, the
Vulkan engine's batching and sprite atlas) get ordinary unit tests in `tests/unit/` with doctest, pinned through CMake
(4.7). They need no proprietary files and run in hosted CI.

### 4.7 CI

- **Hosted (GitHub Actions):** `windows-latest` (MSVC x64) and `ubuntu-latest` or a container with GCC 16 /
  Clang (pin the image). Build, clang-tidy, clang-format, unit tests. Nothing that reads CipSoft files.
- **Self-hosted on this machine** (registered to this repository only): `oracle-x86` and
  difftest, the golden corpus, the comparisons against the test server, and the Linux replay tests in a
  Docker container (a pinned image with GCC 16, Clang, SDL3's dependencies and the pinned CPU Vulkan driver). These read Tibia.exe and the
  data files, which never leave the machine.
- **The repository is public (D17), so the self-hosted runner must never run code from strangers:** its
  workflows trigger only on `push` to this repository's branches and on `workflow_dispatch`, never on
  `pull_request` from forks; the repository requires approval before any workflow runs for outside
  contributors; the runner runs as a separate, unprivileged Windows user. Pull requests from forks get the
  hosted checks only.

---

## 5. Build system: CMake (M1)

**Why CMake:** it is the standard build description for C++ on Windows and Linux, and the only one that every
dependency of this plan ships support for (SDL3, Vulkan-Headers, volk, VMA, vk-bootstrap, glslang, doctest).
Visual Studio, VS Code and CLion open it directly, and it has the most mature support for `import std` with
MSVC, GCC and Clang. Meson and xmake were considered (cleaner syntax, smaller ecosystems, weaker module
support) and rejected.

**One command per platform, nothing else:**

```
cmake --workflow --preset windows-x64      # configure, build, test (Windows)
cmake --workflow --preset linux-x64-gcc    # the same on Linux
```

The Windows batch and PowerShell wrappers go: `build-patella.cmd`, `compile-check.cmd`,
`build-resources.cmd`, `build-capture.cmd` and `difftest\build.cmd` become CMake targets and CTest tests, and
`run-testserver.ps1` / `stop-testserver.ps1` become Python (like the other tools), so that the same tool runs
on both systems. Only `decompilation.ps1` stays, because it belongs to upstream's analysis workspace. CMake
3.30 or newer is required (4.1.2 is installed here; on Linux distributions with an older CMake, Kitware's
packages or `pip install cmake` provide it), and Ninja (included with Visual Studio; a package on Linux).

- **Layout:** a top-level `CMakeLists.txt` with `cmake_minimum_required(VERSION 3.30)` (`import std`
  support in CMake is experimental from 3.30; 4.1.2 is installed); the targets:
  - `patella_core`: the static library of portable code. It starts small (vlong, TRSA, TXTEA, Container.h,
    regexp, Utils, Script, Objects ...) and grows as files lose their Windows dependencies.
  - `patella_platform_win32` (MFC; until M8), later `patella_platform_sdl`.
  - `patella_render_vulkan`: the Vulkan engine (from M2).
  - `patella`: the executable.
  - `tests_unit`, `tests_golden`.
  - in `oracle-x86` only: `difftest`, `sysinctest`, `communicationtest`, `vlongtest` and the capture DLL
    (MASM for `imageehseh.asm` through `enable_language(ASM_MASM)`).
- **Presets** (`CMakePresets.json`, Ninja Multi-Config):

  | Preset | Compiler | Use |
  |---|---|---|
  | `windows-x64` | MSVC | the Windows product |
  | `windows-x64-asan` | MSVC `/fsanitize=address` | sanitizer runs |
  | `linux-x64-gcc` | GCC 16 | the Linux product |
  | `linux-x64-clang` | Clang (libc++ or libstdc++) | second compiler, `-fsanitize=address,undefined` |
  | `oracle-x86` | MSVC x86, static MFC, the original's flags | today's Patella.exe and the difftest, until M6 (removed in #94) |
  | `capture-x86` | MSVC x86 | only patcap.dll for Tibia.exe (since M6, #94) |
  | `windows-x64-gcc` | mingw-w64 GCC 16 (optional, D6) | C++26 parity build on Windows |

- **Toolchain options in one place** (`cmake/Warnings.cmake`, `cmake/Hardening.cmake`):
  - MSVC: `/W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor /Zc:inline`, no
    `/Zc:strictStrings-` (fix the `char*` literals instead), `/guard:cf`, `/CETCOMPAT`.
  - GCC/Clang: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`, `-fstack-protector-strong`,
    `-D_FORTIFY_SOURCE=3`, `-D_GLIBCXX_ASSERTIONS` in debug builds, PIE, RELRO.
  - Raise warnings file by file with a baseline list, so the build never stops being green.
  - The oracle preset keeps exactly today's flags (`/std:c++17`, `/W3`), so that its output stays
    comparable.
- **Dependencies** (pinned, reproducible, offline after the first fetch): `FetchContent` with URL + SHA256
  for SDL3, doctest, Vulkan-Headers, volk, VMA, vk-bootstrap and glslang; `find_package` first so that
  Linux distributions can use their own SDL3. Shaders compile to SPIR-V in the build (6.2). No vcpkg (the
  project already turned it off).
- **Tooling:** `CMAKE_EXPORT_COMPILE_COMMANDS` for clang-tidy, CTest for every test (difftest included), CPack
  for packages (M9). Visual Studio opens the folder directly (CMake mode), so
  `Patella.sln` and `Patella.vcxproj` are deleted once the oracle preset reproduces today's Patella.exe
  (`compare_imports.py` and `compare_resources.py` show 0 differences).
- **Resources:** `Patella.rc` and the extraction of the pictures from Tibia.exe stay a Windows build step.
  The Linux build needs the same pictures (icons, cursors, the bitmaps the client draws): extract them to
  PNG/raw files at configure time with `tools/extract_resources.py` (it reads Tibia.exe, so it works anywhere
  Python does), and load them at run time, or embed them with `#embed` where the compiler has it.

---

## 6. Rendering engines (decision D5)

### 6.1 What an engine has to do

`TSurface` is a 2D compositor: numbered surfaces (the screen, a temporary surface, a video cache for
sprites), solid fills and lines, uploads of 8-bit and masked images with a palette, copies with and without a
colour key, stretched copies with a highlight colour, a clip rectangle, a frame limiter, display modes, and
`ShadeRectangles`: rectangles with one colour per corner, interpolated (the light overlay).

Everything except `ShadeRectangles` is integer work with exact results. The light is rasterized by the
graphics driver in all three original engines (DirectX 5 draws it with Direct3D 7 `D3DTLVERTEX`
triangles), so even the original's frames depend on the GPU and driver. **No new engine can promise exact
pixels for the light.** The contract is: exact outside the light; inside it, at most ±1 per colour channel
against Tibia.exe's DirectX 9 frames (D7), with the rule documented in `notes/rendering.md`.

### 6.2 Decision (D5): Vulkan, and nothing else

**One renderer on both platforms: a hand-written Vulkan 1.3 engine** (`TSurfaceVulkan`, a `TSurface`).
There is no second engine in our code. Machines without a usable Vulkan driver get a **CPU implementation of
Vulkan** that ships with the client and is loaded only when no hardware device is found: older GPUs, virtual
machines, Remote Desktop sessions, CI. That fallback is a driver, not code we maintain.

- **Availability.** Windows: the NVIDIA, AMD and Intel drivers include Vulkan (the loader, `vulkan-1.dll`,
  is already on this machine). Linux: Mesa (RADV, ANV, NVK) and NVIDIA's driver. Vulkan 1.3 covers GPUs from
  about 2015 on with current drivers; anything older uses the CPU driver, which is fast enough for a 2D
  client at these resolutions.
- **CPU fallback:** Mesa's **lavapipe** (Linux distribution packages, and prebuilt Windows builds) or Google's
  **SwiftShader** (Windows and Linux, built from source). Pick one in M2, pin its version and ship it next to
  the executable. The client loads it explicitly through the loader's driver settings, never system-wide.
- **The same driver runs the tests.** The test mode (4.4) renders off-screen and reads the frames back. CI and
  the reference comparisons use the pinned CPU driver, so frames are the same on every run and every machine.
  Hardware GPUs are compared with it on the self-hosted machine.
- **Window and surface:** SDL3 creates the window and the Vulkan surface (`SDL_Vulkan_CreateSurface`). Until
  SDL3 replaces MFC (M8), the engine uses `VK_KHR_win32_surface` on the existing MFC window, so the Vulkan
  engine can replace the three original engines in the Windows client **before** the platform layer
  changes.
- **Design** (a 2D compositor, deliberately small):
  - Vulkan 1.3 core features: dynamic rendering (no render passes or framebuffers), synchronization2,
    timeline semaphores; two frames in flight.
  - Libraries, pinned through CMake: **Vulkan-Headers**, **volk** (function loading without linking the
    loader), **VMA** (GPU memory), **vk-bootstrap** (instance, device and swapchain setup).
  - The numbered surfaces become images: the game canvas at the original size (D10), the temporary surface,
    and a sprite atlas for the video cache (uploads through a staging buffer; the colour key turns into
    alpha 0 when a sprite is uploaded).
  - Pipelines: colour-keyed copy (`discard` on alpha 0, so no blending error), opaque copy, solid fill and
    lines, the light rectangles (per-vertex colour, the same blend as the original), and the final scaled
    presentation of the canvas (nearest-neighbour integer scale, optionally smooth). Draw calls are batched
    per frame into one vertex buffer.
  - Shaders in GLSL, compiled to SPIR-V at build time by `glslang` (fetched and built by CMake, so the
    build needs no Vulkan SDK) and embedded in the executable.
  - Validation layers and debug names in debug builds (the LunarG Vulkan SDK on developer machines; it is
    not installed here yet).
  - Swapchain recreation on resize, minimise and fullscreen changes; `FIFO` present mode for vsync (D13),
    `MAILBOX` or `IMMEDIATE` when vsync is off.
- **Known trouble spots:** overlay tools that inject implicit Vulkan layers (some screen recorders and FPS
  counters) can crash or misbehave. Allow disabling implicit layers from the settings file (D12), and log
  the active layers at start-up (D16).
- **macOS** stays possible later through MoltenVK (Vulkan on Metal), with no second renderer.
- **Size:** roughly 3,000-5,000 lines of engine code, the most demanding single item of the plan. It gets
  its own design note (`notes/vulkan.md`) and unit tests for the batching and the atlas.

Considered and rejected: a software engine plus SDL3's 2D renderer (two engines, little control), SDL3's GPU
API forced to Vulkan (less code, but the graphics would depend on SDL's abstraction), OpenGL 3.3
(maintenance mode), bgfx and WebGPU (large dependencies), Direct3D 12 (Windows only).

The three original engines stay in `oracle-x86` until M6, and leave `windows-x64` as soon as the Vulkan
engine passes tier A there (M2). `glh_genext.h` and patcap's engine hooks go with them. (Done: the sources and
`glh_genext.h` left with the oracle in #94; patcap's hooks of the three APIs stay, for Tibia.exe.)

### 6.3 The engine choice the user sees

The graphics options dialog has three engine buttons (DirectX 5, DirectX 9, OpenGL), and Tibia.cfg stores the
engine number. Both are tier A today. With one engine, the buttons become a **graphics device** choice:
"Automatic", one entry per Vulkan GPU found (useful on laptops with two GPUs), and "CPU (compatibility)".
The choice is stored in the fork's own settings file (D12). Tibia.cfg keeps an engine number in the
original's range, so it stays valid for Tibia.exe (since #45 the x64 client keeps that number as Patella.ini's
`Engine` and writes no Tibia.cfg). This is a recorded exception to tier A, and the dialog's
reference frames are re-recorded once.

---

## 7. Milestones

Sizes: S = a few pull requests, M = a subsystem's worth, L = touches most files, XL = a multi-week redesign.
Each milestone ends when every gate passes and its exit criteria are met.

### M0. Fork bootstrap (S)

- [x] Create `Coldensjo/Patella-Modern` (private until the cleanup below) with Patella's history; `upstream`
      remote set.
- [x] Copy the client data files (gitignored) into `C:\Patella-Modern` (since November 4, 2026
      `C:\Tibia\Patella-Modern`; notes/reconstruction.md, "The machine's folders").
- [x] README: what the fork is, the targets, a link to this plan, the upstream sync rule.
- [x] Decisions D1 to D17 recorded in `notes/decisions.md` under "Modernization".
- [ ] Go public (D17): a README and a notice on what the project is and that it contains no CipSoft files;
      the self-hosted runner restricted (4.7); then switch the repository to public. The history is
      published as it is (decision of November 2, 2026), including the analysis output that earlier commits
      contain (`rea-out/`, `names.tsv`, `symbols.tsv`).
- [ ] One GitHub issue per M1 task, a `modernization` label, `main` protected (pull requests only).
- [x] The test server path (`C:\Tibia\gameserver`, hard-coded in `run-testserver.ps1`, `stop-testserver.ps1`,
      `compare_traffic.py`, `proxy.py`) becomes one setting with that default: `PATELLA_TESTSERVER`,
      tools/testserver.py's `SERVER_DIR` (November 4, 2026, with the folder layout).

### M1. CMake, the presets and the first x64 build (M)

- CMake as described in section 5. **First** the `oracle-x86` preset, reproducing today's Patella.exe
  (0 differences in imports and resources) and the four test executables, with CTest running them.
- Then `windows-x64`: the current code with x64 MFC and the original engines (DirectDraw, Direct3D 9 and
  OpenGL all exist for x64). Fix pointer/int truncation (C4311/C4312/C4302), `LRESULT`/`WPARAM`/`LPARAM`
  widths in the message handlers, `int` vs. `size_t`, and any `(int)` of a pointer. On x86 these are
  value-preserving and do not move layouts, so the oracle stays green.
- `client_state.py` with PDB-derived offsets (4.4), so that tier A runs on the x64 build.
- `linux-x64-gcc` / `linux-x64-clang` build `patella_core` with whatever is already portable (at least vlong,
  TRSA, TXTEA, regexp, Container.h) and run its unit tests. Linux-only issues show up early: `char` is
  unsigned on some Linux architectures (use `-fsigned-char` or fix the comparisons; x64 is signed),
  `wchar_t` is 4 bytes, `long` is 8 bytes (the code uses `int`, and there are no `unsigned long`s), case-
  sensitive file names (`Tibia.dat`), `_snprintf`/`_stricmp` and MSVC calling-convention keywords.
- `src/compat/cxx26.h` with the first stand-ins (contract macros, `TStaticVector` for `inplace_vector`).
- Delete `Patella.sln` / `Patella.vcxproj` and the `.cmd` build scripts (section 5); the test server scripts
  become Python.

Exit: Windows x64 Patella passes tier A; oracle-x86 green with unchanged difftest counts; the core library
builds and tests on Linux with GCC and Clang.

### M2. Layout-independent test infrastructure (L)

- The Vulkan engine (6.2) in the Windows x64 client, on the MFC window: off-screen rendering and read-back
  for the test mode, the CPU driver chosen and pinned, the three original engines removed from `windows-x64`
  once the engine passes `compare_rendering.py` (exact outside the light).
- The test mode (4.4): virtual clock, scripted input, state dump, packet log, frame dump; the sessions of
  `compare_traffic.py` and `compare_files.py` as scripts.
- Reference recordings of every session from Tibia.exe; the record-and-compare split of the comparison tools.
- The golden corpus recorder in difftest (4.3), and its runner on x64 Windows.

Exit: the x64 client, run in the test mode with the Vulkan engine on the CPU driver, matches every reference recording on
Windows.

### M3. Mechanical, layout-neutral modernization (L)

One clang-tidy check (or one hand pattern) per pull request, macro arguments excluded (4.2):

| Change | Scope | Check / tool |
|---|---|---|
| `NULL` to `nullptr` outside the error macros | about 1,265 | `modernize-use-nullptr` |
| `override` / `final` | 167 `virtual` | `modernize-use-override` |
| `= default` / `= delete` (copying owning classes is deleted) | classes with owning pointers | `modernize-use-equals-*`, by hand |
| `typedef` to `using`; `uint8/16/32` as aliases of `std::uint8_t`... | `common.h`, 5 typedefs | `modernize-use-using` |
| Constant `#define`s to `constexpr` or enumerators (`MAPSIZE_X`, `EFFECTS`, ...) | 79; those inside asserted expressions wait for M5 | by hand |
| Enums with an explicit underlying type, then `enum class` where nothing does arithmetic on them | 11 | by hand, `static_assert` sizes |
| `[[nodiscard]]`, `[[fallthrough]]`, `[[maybe_unused]]` | | `modernize-use-nodiscard`, warnings |
| `const`-correctness of locals, parameters and member functions | | `misc-const-correctness`, `readability-make-member-function-const`, `readability-non-const-parameter` (issue #50) |
| `<cstring>`-style headers; each file includes what it uses | | `modernize-deprecated-headers`, include-what-you-use |
| C-style casts to named casts | about 228 | `cppcoreguidelines-pro-type-cstyle-cast` |
| The 18 `goto`s: keep the original's cleanup paths, restructure the rest | 18 | by hand |
| MSVC-only spellings (`_snprintf`, `_stricmp`, `__cdecl`...) behind portable functions | | by hand |

File-scope `static` to anonymous namespaces waits until nothing looks up those names in the PDB (after M2). Done in
issue #44: the PDB keeps the plain names of the data (client_state.py's lookups are unchanged); three statics stay
for the images' .bss (notes/decisions.md).
Each header includes what it uses too (issue #49, after #40's .cpp files): every header of src/ compiles on its own
(cmake/HeaderCheck.cmake, in every preset's build) and misc-include-cleaner is clean on it as a main file
(`tools/tidy.py --headers`, in the hosted job); the includes kept for the images say so (notes/decisions.md).

Exit: clang-tidy's `modernize-*` clean outside the error macros; every gate green; difftest counts unchanged.
Read as (the user's decision, November 4, 2026): the checks this table names are clean and enabled in CI; the other
`modernize-*` checks each have a recorded decision (notes/decisions.md, "M3's exit"): `avoid-c-arrays` and
`use-std-print` belong to M4, the rest are adopted later or not at all. Met on November 4, 2026 (issues #32 to #44,
#49 to #51).

### M4. Safety inside the frozen layout (L)

Layout-preserving types, each with a `static_assert` on its size and alignment in the oracle build and its own
unit tests:

- **`TFixedString<N>`:** exactly `char[N]` (standard layout, trivially copyable), with `Assign`, `Append`,
  `Format` (bounded, `std::format_to_n`) and `View()` (`std::string_view`). It replaces the 125 buffers'
  `strcpy`/`strcat`/`sprintf` uses (about 180 call sites). The original's truncation and overflow behaviour
  is part of the contract: where the original asserts (`strlen(Address)<100`), the assertion stays; where it
  truncates (`_snprintf`), so does the type.
  Done in issue #57 (notes/decisions.md): `src/types/FixedString.h` with `TStringBuffer` for the buffers known by a
  pointer; the oracle's /std:c++17 formats through the stand-in of `src/compat/format.h`; the grep gate is
  `tools/banned_calls.py` in the hosted CI.
- **Ownership:** first an ownership map per subsystem (`notes/ownership.md`: who creates, who frees, what lives
  until exit; 608 `new` against 193 `delete`). Then `std::unique_ptr<T>` for owning members and globals
  (pointer-sized with the default deleter on all three compilers). Non-owning pointers stay raw, or become
  references where never null. No `std::shared_ptr`.
  Done in issue #58 (notes/decisions.md): the map in two parts; 46 owning pointers to `std::unique_ptr` (33 members,
  among them TComponent's drawing list and Container.h's entries, and 13 globals: the account's characters, the patch
  buffer, the object, skin and graphics tables), each `delete` an explicit `reset()` in place and order, the original's
  leaks kept with `release()`; every owner that stays raw is in notes/ownership.md's section 7 with the milestone that
  unblocks it (M5: the `noexcept(false)` pointees and a TASSERT's `strlen(Text)`; M7: the GUI tree, the queue, the
  containers' pointers, the mismatched releases, the globals that act at `exit(1)`; M8: MFC's and `malloc`'s).
- **Views:** `std::string_view` and `std::span<const std::uint8_t>` parameters (the packet buffers, `TXTEA`,
  `TRSA`).
  Done in issue #59 (notes/decisions.md): `src/compat/span.h`'s `patella::TSpan` (std::span; a C++17 stand-in in the
  oracle) for the packet buffers, Network.h's wrappers, TXTEA's key and blocks and TRSA's block; the C strings with a
  null assertion or a NUL-ended text (PutString, SendString, SetPublicKey) stay `const char *`.
  Done in issue #69 (notes/decisions.md): the remaining pairs and unsized buffers (Script.h's readBytes/writeBytes,
  CheckClipboardText, CopyString, FormatNumber, GetErrorMessage, DrawOutfit's colours) take spans, fixed extents where
  the golden tests fake the function by address; CompareStrings' Count and ShortenString's MaxLength are not sizes of
  a buffer and stay.
- **Formatting and I/O:** `std::format_to_n` into `TFixedString`, `std::print` for diagnostics, an RAII
  `TFile` instead of `FILE*`. The bytes written stay the same (`compare_files.py`).
  Done in issue #64 (notes/decisions.md): `src/types/File.h`'s TFile (exactly a `FILE *`; text mode writes "\r\n" on
  every platform), the constant `MsvcSnprintf` formats that always fit as `Format`; stdout's printf is
  `src/compat/print.h`'s PrintLine, not `std::print`, which throws where the write fails. The grep gate bans `FILE`
  and the fopen, fclose and printf families outside File.h.
- **C arrays in structures** to `std::array` (same layout); array parameters to `std::span`.
  Done in issue #60 (notes/decisions.md): members, globals and locals `std::array` (texts `TFixedString`), each frozen
  structure's size and offsets asserted in the oracle build; vlong's byte arrays and the test mode's packet hooks take
  spans. The arrays that stay (needed as such by a TASSERT/TERROR/TEND argument until M5, TFixedString's own, the
  skin table, a parameter the tests fake by address) carry a NOLINT with the reason.
- **`crt71.h`:** one `FormatCRT71Time` built on `std::chrono` and `std::format`, with the two-digit day, instead
  of patching `ctime`'s buffer; this also removes `ctime`, which behaves differently in glibc.
- **Times:** 64-bit `time_t` everywhere (`SaveChannel` calls `_ctime32`, which ends in 2038).
- **Static analysis** to zero findings or documented suppressions: MSVC `/analyze`, clang-tidy `bugprone-*`,
  GCC `-fanalyzer` on the core library.
  Done in issue #63 (notes/decisions.md): cmake/Core.cmake compiles patella_core with `/analyze` and `-fanalyzer`
  (a finding fails the build: the hosted windows-x64 and linux-x64-gcc jobs), the clang-tidy job fails on any
  bugprone-* finding (`tools/tidy.py --zero bugprone-`); 114, 2 and 6 findings, each fixed or suppressed in the code
  with its reason (the table in notes/decisions.md); vlong::operator='s self-assignment is the original's, kept.

Exit: no `strcpy`, `strcat`, `sprintf` or `gets` left (a grep gate); sanitizers clean over all replayed
sessions on both platforms; difftest counts unchanged.
The sanitizers, done in issue #70 (notes/decisions.md, "Sanitizers over the replayed sessions"): the preset
`windows-x64-asan` (MSVC `/fsanitize=address`, Release and Debug) over the golden corpus, the 8 reference sessions and
the unit tests; linux-x64-clang (ASan, UBSan, LeakSanitizer) over what Linux builds, the unit tests (the Linux
golden_runner comes in M6, the Linux client's replays in M8); the original's two out-of-bounds reads ASan reports are
suppressed with their reasons. With the grep gate (issue #57) and the unchanged counts, the exit is met on November 5,
2026.

### M5. Error handling, and the removal of error.txt (decision D2) (L)

`TBEGIN`/`TEND` is a hand-written stack trace: every function catches, adds its parameters and line number,
and throws again (1,174 try blocks). Its only consumer is `error.txt`, the crash report that the client
writes, offers to send at the next start, and sends to the game server as packet 0xE8. D2 removes that
mechanism, so the trace has no reader left and the macros can simply go.

- **Removed:** `CPlayerApp::HandleException`'s file, `ReadErrorFile`, the error report list in `Status.cpp`
  (`AddErrorReport` and the rest, `DeleteErrorFile`), `TErrorReportDialog` and `CreateErrorReportDialog`,
  `SendErrorReports`, `SendDebugReport` (packet 0xE8), events 0x2a and 0x2b, `OutputControlErrorInformations`
  and `CMainFrame::OutputErrorInformations`, the error texts only they use, `lines.py`, and the error
  sessions of `compare_files.py`. This is a deliberate tier A change (a file no longer written, a dialog no
  longer shown, a packet no longer sent), recorded in `notes/decisions.md`. The server does not depend on
  it: it only logs packet 0xE8 (`gameserver/src/game/receiving.cc` `CErrorFileEntry`).
- **Kept:** an unrecoverable error still ends the client with the "Tibia Error" message box (SDL's message
  box from M8), and the message goes to `stderr`. Debug builds also print a `std::stacktrace` where the
  library has it, and break into an attached debugger. `crt71.h` stays, because `SaveChannel` uses it.
- `TASSERT*` becomes `PATELLA_ASSERT(expr)` in `cxx26.h`: on GCC 16, `contract_assert` (and `pre(...)` for
  preconditions); elsewhere a function with `std::source_location`. A violation throws, as today, so the
  top-level handler ends the client the same way. Assertions on untrusted input (packets, files) are
  checked in every build; the rest may be compiled out of release builds later, once fuzzing (M9) has run.
  Done in issue #79 (notes/decisions.md), differently: `PATELLA_ASSERT(expr)` and `PATELLA_ASSERT_VALUES(expr,
  values)` in `src/compat/assertion.h` are the function with `std::source_location` on every compiler, no
  `contract_assert` (a violation would have to throw through the one replaceable handler, which a compiler
  flag can bypass and which cannot pass a predicate's own exception on); 802 sites converted.
- `TBEGIN`/`TEND*` are deleted. A catch stays only where a function really handles an error (the inner
  `try`/`catch` blocks with their own messages, such as the encryption failures).
- `TMyException` shrinks to a message and a `std::source_location`. `PROGRAM_ERROR` becomes an
  `enum class` carried by its own exception type. Recoverable failures (a file that does not load, a host
  that does not resolve) return `std::expected<T, TError>`.
- difftest (until M6) and the golden corpus compare the error paths by outcome only from here on: both
  sides throw, or neither does. The texts and line numbers are no longer compared.
  Done in issue #76 (notes/decisions.md): one check per compared call, the kind kept (TMyException, a
  PROGRAM_ERROR's value, another type by name); the line-pair checks are gone (127,267 of difftest's,
  70,539 of communicationtest's), and the corpus records the outcome only (format 2).
- The built-in patcher (decision D9) goes in M5 too: done in issue #75 (packets 0x1e, 0x1f and 0x20 end the login in
  the login error dialog; ApplyPatch, ApplyFullPatch, InstallPatch, the update dialog, Run's start of patch.exe and
  compare_files.py's patch sessions removed; notes/decisions.md).
- Order: remove the reporting first (one pull request, tier A change recorded), then the macros file by file
  (mechanical), then the assertions.

Exit: no `TBEGIN`/`TEND`, no `error.txt`, no packet 0xE8; every gate green.

### M6. Oracle retirement and layout unfreeze (M): the point of no return

1. The golden corpus is complete and green on x64 Windows and Linux.
   The rendering engines' tests (SurfaceDX, SurfaceDX9, SurfaceGL: 1,488,299 checks) are not ported: they test
   DirectDraw, Direct3D 9 and OpenGL, which leave the fork with the oracle (the user's decision, November 4, 2026; #29).
2. The test mode's state dump and frame dump have replaced `client_state.py`'s memory reading and patcap for
   the modern client.
3. `oracle-x86`, difftest, the x86 `static_assert`s, `compare_imports.py` and `compare_resources.py` leave the
   fork (they stay in upstream). Size asserts that matter for wire and file formats move to the serialisers.
4. Tag `layout-frozen-final`. From here, upstream fixes are ported by hand.

Exit: there is no 32-bit build; tier A, the golden corpus and the unit tests pass without the oracle.

Done November 6, 2026 (items 1 and 2: #91, #92; the final recording #93; items 3 and 4: #94). The tag
`layout-frozen-final` is on ee1705c, the last commit with the oracle. Removed with #94: the oracle-x86 preset and
cmake/OracleX86.cmake with its CTest tests (difftest, sysinctest, communicationtest, vlongtest, safeseh), the
golden corpus's recorder (golden_corpus, golden_reproducible, the GOLDEN_ORACLE code of tests/golden),
tools/difftest, compare_imports.py, compare_resources.py, safeseh_scan.py, the engines' sources (SurfaceDX,
surfaceDX9, SurfaceGL, Extern/glh) and PATELLA_ORIGINAL_LAYOUT with the x86 `static_assert`s; the automap file's
layer sizes are checked at Map.cpp's reader and writer instead. Kept: a `capture-x86` preset that builds only the
32-bit patcap.dll (the owner's decision, November 6, 2026: the reference recordings' and the comparisons' Tibia.exe
runs need it), client_state.py's reading of Tibia.exe, the frozen corpus and reference recordings with their
manifests. The only 32-bit build is that DLL; no client is 32-bit. notes/decisions.md, "The oracle retired (issue
#94)".

### M7. Data structures and architecture (XL)

- **Containers:** CipSoft's `vector<T>` becomes `TIndexedVector<T>` over `std::vector<T>` with the same
  semantics (min..max indexing, block growth, `init` fill). Call sites that index from 0 and never grow by
  indexing move to `std::vector`, `std::array` or `std::flat_map`. `matrix3d<T>` and the automap's
  `uint8 Color[256][256]` become `std::mdspan` views over owned storage.
  Done in issue #99 (notes/decisions.md): `src/types/IndexedVector.h`'s TIndexedVector<T> over a std::vector (its
  allocator allocates as `new T[n]`), every `vector<T>` converted, Container.h keeps array, matrix and matrix3d. The
  call sites that need no min..max or growth move to standard containers in #100.
  Done in #101 (notes/decisions.md, "#101: the matrices and the automap layers as mdspan views"): `patella::TMdspan`
  (`src/compat/mdspan.h`, std::mdspan or a stand-in where the library lacks it); matrix3d and matrix index their
  storage through `view()`, the automap's Color and Speed are `TAutomapLayer` (`src/AutomapLayer.h`), owned bytes in
  the file's order with an mdspan view; array stays.
- **Fixed-capacity lists** (`TMapPoint::Objects[10]` + `ObjectCount`, the effect table):
  `std::inplace_vector<T, N>` where available, `TStaticVector<T, N>` (same interface) elsewhere.
  Done in #102 for the effect table (notes/decisions.md, "#102: fixed-capacity lists"): `TSlotTable<TEffect, EFFECTS>`
  of `src/types/SlotTable.h` over TStaticVector, the original's numbers and its full table (-1, the effect dropped).
  Done in #102b for `TMapPoint::Objects` (notes/decisions.md, "#102b"): a TStaticVector in `src/MapPoint.h` with the
  original's full-point behaviour; the golden Container test's array<TMapPoint> round has the original's point as T.
- **The effect table's index links** (`Field`, `Previous`, `Next` as `int`) become typed handles
  (`enum class TEffectId : int`) with an iterator over a point's effects.
  Done in #103 (notes/decisions.md, "#103: the effect table's links as TEffectId"): `TEffectId` (`src/MapPoint.h`,
  None for the original's -1) in TMapPoint::EffectNumber, Previous and Next, the table and TMap's effect members;
  `TSlotTable::Chain` (TPointEffects) walks a point's effects, `TMapEffects` MapWindow's loop. `Field` is a map point's
  number, not an effect's, and stays an int; the original's free functions keep their int numbers (golden's seam).
- **Globals into subsystems:** the file-scope statics and shared globals become members of explicit objects
  (`TMapState`, `TNetwork`, `TCommunication`, `TGUI`, `TPreferences`, ...), owned by one `TClient` that
  passes dependencies in. This is when the include cycles noted in `notes/progress.md` get broken. The
  original's file structure stays as the module map, so the reconstruction notes still point at the right
  code.
  Designed in issue #104 (notes/decisions.md, "#104: TClient and the subsystem objects"): the objects, the owner of each
  global, the order of the moves (#105-#111), the bridge (one TClient made on first use, an accessor per subsystem,
  the original's free functions forwarding until their callers hold the object); TPreferences is the first, done.
  Done in #105: TNetwork (Network.cpp's globals, PacketHeader, the reconnect pair) and TCommunication
  (Communication.cpp's globals, Control.cpp's packet types, Status.cpp's session as its TSession); Network and
  Communication are in no include cycle any more (notes/decisions.md, "The second move: TNetwork and TCommunication").
  Done in #106: TMapState (src/MapState.h) with TObjectTypes, TMap, TCreatures, TPlayerState and TInventory (the
  globals of Objects.cpp, Map.cpp, Creatures.cpp and Inventory.cpp); TCommunication writes the game state through
  them (notes/decisions.md, "The third move: TMapState").
  Done in #107 (part 1): TGUI owns TTalk (Talk.cpp, now TPreferences' TTalkLists), THelp (Help.cpp) and TInteraction
  (Status.cpp's user interaction and message queue), and holds GUI.cpp's timers and hotkeys, ContentWindows.cpp's,
  Control.cpp's and MainWindow.cpp's data; ModalWindow and MainWindow are its members (raw, for #108); the windows
  reach Control.cpp through TGUIController. The GUI cycle is 4 modules (was 6). Not yet: SkinProperties, Control.cpp's
  and MainWindow.cpp's functions as members (notes/decisions.md, "The fourth move: TGUI").
  Done in #108: the window destructors' work is the components' close (`TComponent::Close`, run before each delete),
  so TComponent's destructor and the window destructors are noexcept; TTabBar's two windows and TGUI's MainWindow are
  `std::unique_ptr`s, ModalWindow, the conditional members and the text buffers stay raw with their reasons; FatalError
  abandons the main window before `exit(1)` (notes/decisions.md, "#108"; notes/ownership.md, sections 6 and 7).
  Done in #118 (TGUI part 2): Control.cpp's and MainWindow.cpp's functions are TGUI's members, TGUIController's adapter
  is gone, the GUI module cycle is gone (2 cycles left: Surface/graphics, MainFrm/Player), TMap's and TCreatures' calls
  up go through TMapConsole and TCreaturesGUI; TNetworkSession's adapter stays (notes/decisions.md, "#118").
- **The core/platform split is completed:** packet parsing and building, game state (Creatures, Map,
  Objects, Inventory, Player), preferences, and the GUI's logic are in `patella_core` and
  include neither `<windows.h>` nor MFC. The platform interfaces are `IWindow`, `IInput`, `IClock`,
  `IClipboard`, `ISocket`, `IFileSystem` and `TSurface`.
  Done in #109 (part 1; notes/decisions.md, "#109: the platform interfaces"): `IClock`, `IFileSystem` and `ISocket` in
  `src/platform` with their Win32 implementations (timeGetTime, time, Sleep; TFile and std::filesystem; CAsyncSocket
  and WinSock) and the test mode's clock; TClient passes TPreferences the file system and TNetwork the socket. Not yet:
  `IClipboard`, `IInput`, `IWindow`, `TSurface`.
  Done in #120 (notes/decisions.md, "#120: the server replay's socket inside the process"): with `--test-replay` the
  test build's `ISocket` is TPipeSocketWin32 over TPipeNetwork (src/testmode), the replay's network inside the
  process: no ports, so replays run side by side; the live runs keep Win32's sockets.
- **Text encoding:** the 7.72 protocol and files use the client's 8-bit code page (Latin-1 / Windows-1252).
  Internally, decide (D4) between keeping 8-bit text (simplest, closest to the original) and UTF-8 with
  conversion at the protocol and file boundaries. The platform layer converts in either case: SDL3 and Linux
  speak UTF-8.
  Done in issue #112 (notes/decisions.md, "#112: text encoding at the platform boundary"): 8-bit inside (D4);
  `src/types/Cp1252.h` converts Windows-1252 to and from UTF-8 and UTF-16 (the undefined bytes as U+0081...,
  `?` for characters it lacks); the clipboard (CF_UNICODETEXT), TFile's file names, the developer log's lines (UTF-8)
  and the Vulkan device names use it; titles, message boxes, URLs and the command line are ASCII and stay; the
  keyboard (WM_CHAR) and the CPU driver's path wait for M8.
  Done in #117 (notes/decisions.md, "#117: Patella.ini in UTF-8"): Patella.ini is UTF-8 (PreferencesVersion 2), its
  texts and LogFile converted to Windows-1252 when read; a version-1 file is read as Windows-1252 and converted once.
- **Reflection** (GCC 16 `-freflection`) is a candidate for generating the state dump, the Tibia.cfg
  serialiser and the golden-record serialisers. Use it only behind a generator that also has a hand-written
  or macro-based path for MSVC, until MSVC has it.
  Done in #114 (notes/decisions.md, "#114: reflection for the state dump and the serialisers"): evaluated, adopt later.
  GCC 16.2's reflection gives the same bytes and works reliably, but with the fallback MSVC and Clang need it saves no
  code. Revisit when MSVC and upstream Clang ship P2996; the experiment is tests/reflection/state_dump_reflection.cpp.
- **Modules:** `import std;` in new code once all three compilers support it under CMake. The client's own
  headers become named modules after MFC is gone (M8): one module per subsystem, partitions per original
  file.
- **Debug presets replay the corpus too:** done in #115 (notes/decisions.md, "#115: an empty function's code at
  every optimisation level"): the golden Communication test's check that ExitCommunication is empty accepts GCC's -O0
  frame, so linux-x64-gcc-debug and linux-x64-clang-debug replay the corpus with 0 failures.

Exit: `patella_core` contains everything except the platform layer and builds on both platforms;
no CipSoft `vector` in core code; tier A green.

### M8. Platform layer on SDL3, and the Linux client (XL)

One platform layer for both operating systems, with **SDL3** (D3) for the things the Win32/MFC code does now:

| Today | With SDL3 / the standard library |
|---|---|
| `CWinApp`, `CFrameWnd`, the message map, `SetTimer` | `SDL_CreateWindow`, the event loop, the game timer in the loop |
| `ON_WM_KEYDOWN`, `ON_WM_CHAR`, mouse messages | `SDL_EVENT_KEY_DOWN`, `SDL_EVENT_TEXT_INPUT` (UTF-8, converted to the client's code page), mouse and wheel events |
| `SetCapture`/`ReleaseCapture` (64 calls), `GetCursorPos`, `ClientToScreen` | `SDL_CaptureMouse`, window-relative mouse coordinates |
| `LoadCursor`, the client's cursors | `SDL_CreateColorCursor` from the extracted cursor resources |
| `EnumDisplaySettings`, `ChangeDisplaySettings`, `GetSystemMetrics` | `SDL_GetFullscreenDisplayModes`, `SDL_SetWindowFullscreenMode`, display bounds |
| Clipboard, `ShellExecute`, `MessageBox` | `SDL_SetClipboardText`/`SDL_GetClipboardText`, `SDL_OpenURL`, `SDL_ShowSimpleMessageBox` |
| `timeGetTime`, `Sleep` | `IClock` over `std::chrono::steady_clock`, `std::this_thread::sleep_for` (or `SDL_DelayPrecise`) |
| `CAsyncSocket`, `WSAAsyncGetHostByName` | a small `TSocket` over Winsock and POSIX sockets (non-blocking, polled from the game loop); `getaddrinfo` on a `std::jthread` with the result posted back to the loop (IPv4 and IPv6). The client stays single-threaded apart from that |
| `VK_KHR_win32_surface` on the MFC window (M2) | `SDL_Vulkan_CreateSurface` (section 6) |

- MFC and the MFC shim are deleted (the original engines and `glh` went in M2). The Windows build no longer needs the
  Visual Studio MFC component, so hosted CI can build everything.
- **Crypto:** XTEA and raw RSA are protocol requirements and stay. `vlong` stays as a portable, tested
  component (`vlongtest`'s cases become unit tests). The protocol's lack of RSA padding is documented as a
  property of the protocol.
  Done in #128 (notes/decisions.md, "#128"): each of vlongtest's 3,201 cases is checked on its own in tests_core,
  against a digest per case from Python's exact integers (tests/unit/core_vlong_cases.inc), on every preset with
  ASan and UBSan; the unpadded RSA is documented there and at the call site (Communication.cpp, TRSA::Encrypt).
- **regexp:** Henry Spencer's code stays (compiled as C17), behind an RAII wrapper. `std::regex` is not a
  drop-in (syntax, speed, exact matching behaviour).
  Done in #129 (notes/decisions.md, "#129"): regexp.c is C17 on every compiler (MSVC `/std:c17` too) with prototypes
  for its K&R definitions, off the warning baselines, 0 warnings at the full level; `patella::TRegexp`
  (src/types/Regexp.h) owns regcomp's program, Status.cpp's link check goes through it, unit tests of the client's
  pattern in tests_core.
- **Files and paths:** `std::filesystem`; the data files are found next to the executable or in a configured
  directory, with case-insensitive lookup of the original names on Linux. Tibia.cfg's location stays next to
  the data by default (portable), with an XDG option on Linux (D4); since #45 that is Patella.ini's (D12), and
  Tibia.cfg is only read, once, for the import.

Exit: the Linux client logs into the test server and plays the scripted sessions; tier A green on Windows
x64 and Linux x64 against the reference recordings; the Vulkan engine passes the reference frames on Linux
(hardware drivers and the CPU driver).

### M9. Hardening, performance, release (M)

- **Fuzzing:** the packet parser and the file loaders (Tibia.cfg, `*.map`, Tibia.dat/.spr/.pic) with libFuzzer
  (Clang on Linux) and MSVC `/fsanitize=fuzzer`, seeded with the reference recordings.
- **Hardening:** `_MSVC_STL_HARDENING` and libstdc++'s/libc++'s hardened modes in release builds where the
  library supports them; `-D_GLIBCXX_ASSERTIONS` / bounds-checked `std::span` and `std::mdspan` in debug.
- **Profiling:** frame time and allocations per frame against the reference client; GPU timings of the Vulkan engine
  (timestamp queries).
- **Packages** through CPack: a Windows zip (and optionally an installer), a Linux tarball and AppImage or
  Flatpak (D4). The packages contain no CipSoft files: the user points the client at their own data files.
- Reproducible builds, a changelog, a release checklist.

---

## 8. Coding standard additions (written in M1, enforced by clang-tidy)

- No owning raw pointers; no `new`/`delete` outside `std::make_unique` or a documented arena.
- No C string functions outside `TFixedString`; no `sprintf` family; formatting with `std::format`.
- `nullptr`, `override`, `enum class`, `constexpr`, `[[nodiscard]]` by default.
- No platform headers in `patella_core`. Every Windows or Linux difference sits behind a platform interface.
- C++26 features only through `src/compat/cxx26.h`, each with its C++23 stand-in and the feature-test macro
  that selects it.
- Every structure that is a wire or file format has a size assertion and a serialiser test; fixed-width
  integer types and explicit little-endian reads and writes (both targets are little-endian, but the
  serialisers say so).
- Behaviour that looks like a bug but is the original's keeps its comment, as in upstream. The address
  comments (`// 0x4ca730, lines 152-164`) stay as provenance (`// Tibia.exe 0x4ca730`) on functions that still
  correspond to an original one.

---

## 9. Decisions

Decided on November 2, 2026 (recorded in `notes/decisions.md` under "Modernization" in M0):

| # | Decision | Choice |
|---|---|---|
| D1 | The equivalence contract | As in section 3: tier A always, tier B until M6, tier C until M5/M6 |
| D2 | `error.txt` and the error report | **Removed** in M5: no file, no report dialog, no packet 0xE8. A fatal error still shows the "Tibia Error" message box and writes the message to `stderr`. The server only logs packet 0xE8 (`gameserver/src/game/receiving.cc` `CErrorFileEntry`), so nothing depends on it |
| D3 | Targets | Windows x64 and Linux x64, no 32-bit product; SDL3 as the platform layer for both |
| D4 | Linux details | The client's 8-bit code page inside (as the original, Windows-1252), UTF-8 only at the platform boundary (#112: types/Cp1252.h); the settings file next to the data files (Patella.ini since #45, D12, in UTF-8 since #117 (PreferencesVersion 2); Tibia.cfg is only read once, for the import); AppImage for Linux, a zip for Windows |
| D5 | Rendering | **Vulkan 1.3 only**, hand-written (volk, VMA, vk-bootstrap), on both platforms; a pinned CPU Vulkan driver (lavapipe or SwiftShader) ships as the fallback and runs the tests. SDL3 only creates the window and the surface (section 6) |
| D6 | Compilers | GCC 16 on Linux (the reference compiler), MSVC on Windows x64, Clang on Linux in CI. Revisit Windows after M8 (clang-cl or mingw-w64 GCC is a preset change). C++26 only through `cxx26.h` |
| D7 | The reference engine for frames | Tibia.exe's DirectX 9 frames |
| — | Build system | CMake with presets and Ninja; `.sln`/`.vcxproj` removed in M1 |

D8 to D17 (section 11) were decided on the same day; SDL3 stays the platform library (rather than GLFW).

## 10. Risks

| Risk | Mitigation |
|---|---|
| A refactoring changes behaviour that no gate covers (timing, rarely used dialogs) | difftest counts must not drop; extend the scripted sessions before refactoring a subsystem without tier A coverage |
| clang-tidy rewrites inside the error macros change messages | Excluded ranges (4.2); `lines.py` and difftest catch the rest |
| The golden corpus misses cases that difftest's random inputs found | Record the full random runs with fixed seeds; keep the recorder until M6 |
| Keyboard and text input differ on Linux (layouts, dead keys, IME) | SDL's text input events converted to the client's code page; scripted sessions with non-ASCII names, chat and keyboard shortcuts on both platforms |
| The light overlay cannot match exactly | A documented tolerance (6.1); everything else exact |
| GPU driver differences between machines | The pinned CPU Vulkan driver is the reference in CI; hardware GPUs are compared with it on the self-hosted machine |
| The Vulkan engine is the largest single piece of new code | Vulkan 1.3 core features, vk-bootstrap and VMA keep it small; validation layers in every debug run; its own design note and unit tests |
| Machines without Vulkan (old GPUs, VMs, Remote Desktop) | The shipped CPU driver; the device choice in the options dialog (6.3) |
| MSVC's C++26 support arrives late | Everything works on C++23 stand-ins; D6 offers GCC or Clang on Windows |
| `std::stacktrace` is missing or differs on Linux | Error reports do not depend on it; it is extra information only |
| Proprietary files leak through CI, packages or golden data | Only the self-hosted runner reads them; golden data and reference recordings stay out of the repository; packages contain no CipSoft files |
| The public repository's self-hosted runner runs a stranger's code | Push and manual triggers only, approval for outside contributors, an unprivileged runner account (4.7) |
| Legal action over a public reconstruction of a proprietary client | Accepted when deciding D17; no CipSoft files are ever committed or shipped |
| Upstream keeps changing during M1 to M5 | Merge upstream regularly while layouts are frozen; the mechanical changes rebase cleanly |
| Scope creep (gameplay changes, protocol upgrades, new UI) | Out of scope: this fork changes how the client is built and written, not what it does |

---

## 11. Further modernization (D8 to D17, decided)

These change what the user sees or what the client accepts from a server, so they are deliberate tier A
changes, each recorded in `notes/decisions.md`. All were adopted on November 2, 2026, D17 with the choice
"public" (last column).

| # | Change | Why | Affects | Decision |
|---|---|---|---|---|
| D8 | **Configurable servers and RSA key.** Today the login servers (`server.tibia.com`, `server2.tibia.com`, `tibia1/2.cipsoft.com`, all gone) and CipSoft's RSA key are compiled in; `tools/patch_client.py` patches the executable to point it elsewhere | Connect to any 7.72 server without patching the binary; the test tools stop patching Patella | Network.cpp, TRSA, the test tools (M8, or earlier as a small change) | Yes. The original list and key stay the defaults |
| D9 | **Remove the built-in patcher.** A login server can send patches (packets 0x1e, 0x1f, 0x20) that rewrite Tibia.dat, Tibia.spr, Tibia.pic, Patch.exe and Tibia.exe (as Tibia.bak), after which the client runs Patch.exe. Any server you log into can replace the client's files and have a program run | Removes a remote code execution path by design. CipSoft's patch servers are gone; data files are updated by hand | Communication.cpp `ReceivePatch*`, Script.cpp `ApplyPatch`/`InstallPatch`, Player.cpp's `_execl`, the update dialog, `compare_files.py`'s patch sessions | Yes, in M5 with error.txt. A patch packet ends the login with a message ("This server wants to update the client"). Done in issue #75 (notes/decisions.md, "The built-in patcher removed"): the message in the login error dialog ("Sorry") |
| D10 | **Pixel-perfect scaling and high-DPI.** The client draws at fixed pixel sizes, which look tiny on high-resolution screens | Draw everything at the original size into an off-screen canvas and scale it to the window by a whole number (2x, 3x...) with sharp pixels, or smoothly as an option | The renderer design (D5): the canvas has to exist from the start; tests compare the 1x canvas | Yes. Integer scaling by default |
| D11 | **Borderless fullscreen** instead of changing the monitor's display mode (`ChangeDisplaySettings`) | Mode changes are slow, rearrange other windows and break multi-monitor setups. With D10, fullscreen is a borderless window at the desktop's resolution | `SurfaceDX`/`DX9` display modes, the graphics options dialog (the resolution list becomes a scale choice) | Yes. Drop exclusive display modes |
| D12 | **A settings file of the fork's own**, `Patella.ini` (plain `key=value`): everything new (servers and key (D8), scale (D10), the graphics device, vsync, data folder, log level) and, since M6 (#45, the owner's decision of November 3, 2026), every option the original keeps in Tibia.cfg, under Tibia.cfg's names (src/settings/PreferenceKeys.h). The x64 client imports an existing Tibia.cfg once (the first start without the options in Patella.ini) and never writes Tibia.cfg | Until M6 Tibia.cfg kept exactly the original's format, so Tibia.exe could read it and tier A compared it byte for byte; after the oracle's retirement one file is simpler for the user, and tier A compares the options by value (Tibia.exe's Tibia.cfg against the x64 client's Patella.ini, `tools/settings_file.py`) | Preferences.cpp, src/settings, M2 (#31), M6 (#45) | Yes. A small parser of our own, no dependency |
| D13 | **Frame pacing:** vsync by default, the original's frame-rate slider kept as a cap | Smooth motion on 60-240 Hz screens; game timing is in milliseconds and does not depend on the frame rate | `WaitForNextFrame`, the SDL engine (M8) | Yes |
| D14 | **Finding the data files:** the packages contain no CipSoft files; the client looks next to itself, then in the folder from D12 or `--data <folder>`, and otherwise asks once | Needed for Linux packages (AppImage) and for keeping CipSoft's files out of anything we build | M8 file layer | Yes |
| D15 | **One namespace, the original's names.** Everything in `namespace patella`; the TitleCase/`T`-prefix style stays | CipSoft's `vector`/`array` no longer collide with `std::`; renaming everything to another style later would be churn with no benefit, and the notes keep matching the code | M3 (style), M7 (namespace) | Yes |
| D16 | **A small developer log:** levels, `std::print` to `stderr`, optionally a file (D12). Nothing is sent anywhere | Gives developers back the diagnostics that error.txt used to hold (last packets, position, the failed check) without a crash-report system | M5 | Yes. No logging library |
| D17 | **Distribution scope.** The code is a reconstruction of CipSoft's proprietary client | Publishing the source or binaries carries legal risk (noted when deciding) | M0, CI (4.7), M9 | **Public**: the repository goes public after the M0 cleanup, with its history as it is. Packages and releases never contain CipSoft files (Tibia.exe, .dat, .spr, .pic), golden data or reference recordings |

Considered and not adopted: macOS (cheap later with SDL3 and Clang, if the core stays free of
platform code, which section 8 already requires), a browser build (SDL3 + Emscripten would need a WebSocket
proxy for the protocol), localisation and full Unicode text (the 7.72 protocol is 8-bit, D4), sound (7.72
has none), scripting or plugins and protocol upgrades (new features, out of scope).

