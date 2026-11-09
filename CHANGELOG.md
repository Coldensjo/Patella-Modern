# Changelog

What has changed in Patella-Modern, for people who play with it, build it or work on it. Patella-Modern is a modernized
fork of Patella, the reconstruction of the Tibia 7.72 client of 2006. The longer story of each step is in
`docs/modernization-changes.md`, and the technical record is in `notes/decisions.md`; the numbers below are GitHub
issues.

The rule behind everything here: the game behaves as the 2006 client did. Recorded game sessions are replayed
after every change and their network messages, saved files and pictures compared with the original program's. A place where
the behaviour changes on purpose is listed under "Deliberately different from the 2006 client".

## Unreleased

No version has been released yet. This section is the state of the main branch.

### What you need

- Your own copies of the original game files (`Tibia.dat`, `Tibia.spr`, `Tibia.pic`). They are not part of this project and
  never will be, and neither is the original `Tibia.exe`.
- To build the client as it looks in the original, a local `Tibia.exe`: the cursors, icon and pictures are taken from it at
  build time and stay on your machine. Without one, the build can use neutral stand-in pictures.
- To build on Windows: Visual Studio 2026 with the C++ workload, Python 3.13 and CMake 4.4.5 (README, "Building"). To build for
  Linux: the Docker image of `docker/linux-x64`, which has everything else.

### New platforms and graphics

- The client is a 64-bit program on Windows (#5). The 32-bit build is gone since #94; the last copy is at the tag
  `layout-frozen-final`.
- The client also builds and runs on Linux, from the same code (#131). So far it has been run in a container with the
  software renderer: replaying every recorded session, logging in to a test server and playing a short scripted session.
  Graphics cards under Linux and a real Linux desktop have not been tried yet.
- Graphics are drawn with Vulkan (#19, #20) instead of DirectX and old OpenGL. A computer without a suitable graphics card
  uses a software renderer (Mesa's lavapipe) that comes with the client on Windows and from the system on Linux. The
  graphics options offer "Automatic", "GPU" and "CPU" (#31).
- The window, its events, the keyboard and mouse handling that needs it, the clipboard, the message boxes and web links
  come from SDL3 (#122 to #124, #126), and Microsoft's MFC is gone (#127).
- Server addresses can be IPv6 (#125).

### Settings and files

- Settings are in `Patella.ini`, a readable text file (#45, UTF-8 since #117). An existing `Tibia.cfg` is imported once.
- The game files are found in the folder the client is started in, or next to the program, or in a folder given with
  `--data` or written in `Patella.ini` as `DataFolder` (#130). On Linux, the original file names are found whatever their
  capital letters.
- A developer log writes errors, and if you ask for it more detail, to standard error and to a file you name (#77): the
  `LogLevel` and `LogFile` keys of `Patella.ini`.

### Deliberately different from the 2006 client

- The crash report is gone: the client no longer writes `error.txt` or sends it to a game server (#74). An error shows its
  message box and goes to the developer log.
- The built-in updater is gone: no server can replace the client's files or start a program any more (#75).
- A malformed packet, an over-long name or a damaged data file can no longer make the client read or write outside its
  memory (#72, #86, #139). A broken server message is dropped and noted in the developer log, and the game goes on where
  the original read past its data, froze or quit. An empty server message is skipped (#142).
- Letters the game's character set cannot show (from a Polish or Russian keyboard, say) arrive as a question mark instead of a
  different letter (#123).
- The login server and its encryption key can no longer be changed from the program's environment. That exists in the test
  version only (#158).

### Safer

- All text buffers are bounded, and `strcpy`, `strcat`, `sprintf` and their relatives cannot come back (#57).
- Memory has clear owners, and sizes travel with the data (#58, #59, #60, #69).
- Every build uses the standard library's own safety checks, the hardened mode of each library (#133). Together with Control
  Flow Guard, shadow stacks and stack protection (#8), a wrong index now stops the program instead of corrupting memory.
- The packet parser and the file loaders are fuzzed (#132), and three bugs of ours and seven weaknesses of the original
  were fixed that way (#132, #139).
- AddressSanitizer and UndefinedBehaviorSanitizer run over the replayed sessions on Windows and Linux (#63, #70).

### For contributors

- One command builds and tests on any machine: `python -I tools/build.py <preset>` with `windows-x64`, `windows-x64-debug`,
  `windows-x64-asan`, `windows-x64-gcc`, `linux-x64-gcc` or `linux-x64-clang` (#4, #10, #7). CMake is pinned to 4.4.5
  everywhere (#141), and every downloaded dependency is pinned by SHA-256 and retried when a server fails (#153).
- The code is C++23 with C++26 features through one header, built with warnings as errors, formatted by clang-format and
  checked by clang-tidy against a baseline (#8, #9, #49, #63). `import std;` works for new code (#113, #121).
- The game no longer depends on Windows: everything but the small platform layer is in the library `patella_core`,
  which builds and is checked on Windows and Linux (#110, #111). The global variables became objects owned by one `TClient`
  (#104 to #107, #116, #118), and the original's list types became standard containers (#99 to #103).
- Tests: a test build of the client with a fake clock, scripted input and dumps (#21, #22), headless (#145); frozen
  reference recordings and a corpus of about 17 million checks (#23, #24, #93), which are local and never published; live
  comparisons with the original in Windows Sandbox on a private test server per run (#146, #147).
- Builds are reproducible: two Release builds of one commit in different folders give the same `Patella` and
  `PatellaTest` binaries, on Windows and Linux (#136). `python -I tools/repro_check.py <preset>` checks it, and
  `notes/release-checklist.md` lists what to do for a release.
