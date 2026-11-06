# Handover: Patella-Modern, next steps

You are continuing the modernization of **Patella-Modern** (`C:\Patella-Modern`,
https://github.com/Coldensjo/Patella-Modern, currently **private**). It is a fork of Patella
(`C:\Tibia`, https://github.com/Coldensjo/Patella, private): a source reconstruction of the Tibia 7.72 client
(`Tibia.exe`, 32-bit, MSVC 7.1, 2006), checked function by function against the binary. The fork turns that
reconstruction into a modern C++ code base for **Windows x64 and Linux x64** without changing what the client
does.

## Read first

1. `notes/modernization-plan.md`: the plan (milestones M0 to M9, test strategy, the equivalence contract,
   decisions D1 to D17). This is the source of truth.
2. `notes/decisions.md`, last section "Modernization (Patella-Modern, November 2, 2026)": every decision in
   short.
3. `docs/modernization-explained.md`: the plain-language summary (keep it in step when something visible
   changes).
4. `notes/reconstruction.md` and `notes/progress.md`: how the reconstruction, the build and the tests work today.
5. The open issues: `gh issue list --repo Coldensjo/Patella-Modern --label modernization`.

## Decisions already made (do not reopen them)

- Targets: Windows x64 and Linux x64. No 32-bit product. The `oracle-x86` preset (today's 32-bit build
  plus difftest) stays as a **test instrument only** until M6.
- Build: **CMake** 3.30+ with presets and Ninja; one `cmake --workflow --preset <name>` per platform. The
  `.sln`/`.vcxproj` and the `.cmd` build scripts are removed in M1 (issue #10).
- Compilers: MSVC on Windows x64, GCC 16 on Linux, Clang on Linux in CI. C++23 baseline; C++26 features only
  through `src/compat/cxx26.h` with feature-test macros.
- Rendering: **Vulkan 1.3 only**, hand-written (volk, VMA, vk-bootstrap, glslang via CMake), on both
  platforms; a pinned CPU Vulkan driver (lavapipe or SwiftShader) as the fallback and for the tests. It comes
  in M2, first on the existing MFC window (`VK_KHR_win32_surface`).
- Window, input and clipboard: **SDL3** (M8). Not GLFW.
- `error.txt`, the error report dialog and packet 0xE8 are **removed** in M5, together with `TBEGIN`/`TEND`.
- D8 to D16 adopted: configurable servers and RSA key; the built-in patcher removed; integer scaling of an
  original-size canvas; borderless fullscreen; a settings file of the fork's own (Tibia.cfg unchanged); vsync
  by default; data file lookup (no CipSoft files in packages); `namespace patella` with the original naming
  style; a small developer log.
- D17: the repository goes **public** after the M0 cleanup (issue #3), with its history as it is (it
  contains earlier analysis output: `rea-out/`, `names.tsv`, `symbols.tsv`). The user accepted the legal risk.
  **Never** commit or publish CipSoft files (`Tibia.exe`, `.dat`, `.spr`, `.pic`, `.cfg`), golden data or
  reference recordings.

## What comes next, in order

1. **#1** README (what the fork is, targets, build, links to the plan and the explainer, the upstream sync
   rule, "no CipSoft files included").
2. **#2** The test server path becomes a setting (`PATELLA_TESTSERVER`, default `C:\Tibia\gameserver`).
3. **#3** Go public, but only after #1 and the self-hosted runner restrictions of plan 4.7 are in place. **Ask
   the user before switching the visibility**; that step publishes everything and is hard to undo.
4. **#4** The CMake `oracle-x86` preset. It must reproduce today's Patella.exe: `compare_imports.py` and
   `compare_resources.py` 0 differences against Tibia.exe, and every difftest check count unchanged (the last
   counts are in `notes/progress.md`, "Last difftest"). Everything else in M1 builds on this.
5. **#8** Compiler settings and style configuration, **#9** `cxx26.h`.
6. **#5** The Windows x64 build of the current code, with **#6** (`client_state.py` reads field offsets from the
   PDB) so that `compare_traffic.py` and `compare_files.py` run on x64.
7. **#7** The Linux presets and the portable core library (in Docker).
8. **#10** Remove the old project and scripts, **#11** CI.
9. Then M2 (plan section 7): the Vulkan engine, the client's deterministic test mode, reference recordings,
   the golden corpus recorder. Open its issues when M1 is done.

The repository's issue-loop workflow can work one issue at a time (`/issue-loop:work`). One issue per pull
request or commit, every gate of plan 4.1 green.

## Rules that matter on every change

- **Behaviour must not change** (plan section 3, tier A: packets, written files, frames). Deliberate changes
  only through a recorded decision.
- **Until M6 the structure layouts are frozen in the oracle-x86 build:** only same-size replacements, with
  `static_assert`s. **Until M5, never rewrite a macro argument of `TASSERT*`, `TERROR*` or `TEND*`.** The
  expression text ends up in the error message and difftest compares it (plan 4.2). This applies to
  clang-tidy fix-its as well, e.g. `NULL` to `nullptr`.
- The difftest check counts must stay the same. A drop means a test stopped testing something.
- Tabs for indentation (tab size 4) in all new and edited files, except where a format requires spaces.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; pull request bodies end
  with `🤖 Generated with [Claude Code](https://claude.com/claude-code)`. Commit and push only when asked or
  when the issue workflow says so.

## Environment facts (checked on November 2, 2026)

- Visual Studio 2026 Community, MSVC 14.51 (`cl` 19.51.36260), vcvarsall:
  `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat`. Static MFC is
  installed for **x86 and x64** (`atlmfc\lib\x64\nafxcw.lib` exists), so #5 can use x64 MFC.
- MSVC `/std:c++latest`: `std::format`, `std::print`, `std::expected`, `std::mdspan`, `std::stacktrace`,
  `std::flat_map`, `import std`. **No** contracts, reflection, `#embed`, `std::inplace_vector`.
- GCC 16.2 (mingw-w64, `C:\Servers\Software\mingw64`): contracts, reflection (`-freflection`), `#embed`,
  `std::inplace_vector`; no `std::stacktrace` in the default library.
- CMake 4.1.2 (`C:\Program Files\CMake`), Ninja 1.13.2, `clang-tidy` and `clang-format` in Visual Studio's
  `VC\Tools\Llvm\x64\bin`. `clang-cl` is **not** installed.
- Docker 29.6.2 (Docker Desktop; start it first). No WSL Linux distribution. No GCC 16 Linux image built yet.
- Vulkan: the loader (`vulkan-1.dll`) is installed; the LunarG Vulkan SDK (validation layers) is **not**.
- Python 3.13 (`python -I` for the repository's tools). `PYTHONUTF8=1` is set.
- The client data files (`Tibia.exe`, `.dat`, `.spr`, `.pic`, `.cfg`) are copied into `C:\Patella-Modern`
  (gitignored). The test server is shared: `C:\Tibia\gameserver` (source in `gameserver\src`; it logs
  packet 0xE8 in `game/receiving.cc` `CErrorFileEntry`).
- The decompilation workspace is not in the fork. If analysis is needed, it is in `C:\Tibia\decompilation`;
  restore it there with `tools\decompilation.ps1 -Restore` (C:\Tibia's CLAUDE.md). The `rea` MCP server is
  registered for `C:\Tibia` only.

## Shell pitfalls seen in this session

- In PowerShell, a profile function intercepts `cmd` (it fails with a Start-Process "file not found" error). Call `& "$env:windir\System32\cmd.exe" /d /c "<absolute path>.cmd"` instead, and
  `cd /d` to an absolute directory inside the batch file.
- From Git Bash, run batch files as `cmd //c 'tools\difftest\build.cmd'` (see `notes/reconstruction.md`).
- Long Python one-offs fed through a Bash heredoc failed to parse once. Write the script to a file under
  `%TEMP%` and run it with `python -I <file>`.
- Freshly built test executables in `%TEMP%` could not be started through `cmd`; running them directly from
  PowerShell worked.
