# Modernization changes, in plain words

This is the list of every major change that has made Patella-Modern more modern, in the order the work happened.
Each entry says what changed, what it gives us, and how it moves the code from 2006 to today. The issue numbers
link the entry to the full technical record in `notes/decisions.md`.

`docs/modernization-explained.md` describes the goal. This file records what has actually been done. A new entry is
added whenever a major modernization is merged.

One rule holds for everything below: **the game behaves exactly like the 2006 original.** After every change,
automatic tests replay recorded game sessions and compare the messages sent to the server, the files written and
the pictures drawn with the original program. Where we change behaviour on purpose (only to remove a danger or a
dead feature), it is listed as a deliberate difference.

---

## M1: a modern build, a 64-bit program, the first Linux code

**CMake instead of Visual Studio project files (#4, #10).**
The program used to be built from a Visual Studio project and a handful of batch scripts that only worked on one
machine. Now one standard build description (CMake, with "presets") builds it on Windows and Linux with a single
command. *Why it matters:* anyone can build it, on any machine, and the same description drives the automatic
checks on GitHub.

**A 64-bit Windows program (#5).**
The original is a 32-bit program, a format from the 1990s that is limited to 4 GB of memory. The client now builds
as a 64-bit program, today's standard. Code that squeezed memory addresses into 32-bit numbers was fixed.

**Strict compiler settings and a code style (#8).**
All warnings are turned on and treated as errors, and one formatting style (clang-format) covers the whole code.
*Why it matters:* the compiler now catches whole classes of mistakes before they reach a test.

**The first portable code on Linux (#7).**
The parts of the client that never touched Windows (encryption, big-number maths, the pattern matcher, the basic
containers) became one library, `patella_core`, which builds and is tested on Linux with two compilers (GCC and
Clang) inside a pinned Docker image.

**Automatic checks on GitHub (#11).**
Every change is built and tested by GitHub's own machines before it can be merged.

## M2: tests that don't depend on the old program's memory layout

**Vulkan graphics (#19, #20).**
The original draws with DirectX 5, DirectX 9 or old OpenGL, technology from 1997 to 2004. The client now draws with
Vulkan, today's standard on Windows and Linux. Computers without a suitable graphics card use a version of Vulkan
that runs on the processor (Mesa's lavapipe), so the game runs everywhere. The graphics device is a simple choice in
the options (#31).

**A test mode inside the client (#21, #22).**
A special build of the client can run with a fake clock, scripted mouse and keyboard input, and dumps of its state,
its network messages and its frames. *Why it matters:* the tests no longer need to read the old program's memory
at fixed addresses, which made the 2006 memory layout untouchable.

**Reference recordings and the golden corpus (#23, #24).**
Every test session was recorded once from the original Tibia.exe, and about 17 million function-level checks were
recorded from the reconstruction checked against the original. Both are frozen and kept locally (they contain data
from the original game, so they are never published). Every later change is compared against them.

## M3: modern C++ syntax, without changing a single instruction

A long series of mechanical updates, each checked to produce the same program (#32 to #44, #49 to #51):

- `NULL` became `nullptr`, `typedef` became `using`, and constants written as `#define` became typed constants.
- Classes that override a function say `override`, and copying is forbidden where it would have been a bug.
- Enumerations got fixed sizes, and `enum class` where nothing did arithmetic on them.
- Old C-style casts became named casts that say what they do.
- Most `goto`s were restructured.
- Each file and each header includes exactly what it uses and compiles on its own.
- Microsoft-only spellings (`_snprintf`, `_stricmp`, `__cdecl`...) go through one portable header.

*Why it matters:* the code reads like current C++, and tools can check it, without any risk to behaviour.

## M4: safety, without moving any data

**Safe text buffers (#57).**
The original copied text with `strcpy`, `strcat` and `sprintf`, functions that write past the end of a buffer when
the text is too long: the classic cause of crashes and security holes. All 125 buffers became a safe fixed-size
string type with the same memory size. A check on every change makes sure those functions never come back.

**Clear ownership of memory (#58).**
The original allocated memory 608 times and freed it only 193 times by hand. An ownership map now records who
creates and who frees each object, and 46 owning pointers became `std::unique_ptr`, which frees automatically.

**Views and arrays with their size (#59, #60, #69).**
Packet buffers and the encryption code take `std::span` (a pointer that knows its length) instead of a bare
pointer, and C arrays in structures became `std::array`.

**Safe files and formatting (#64).**
Files are opened through a small class that always closes them, and numbers are formatted with bounded modern
formatting. The bytes written to disk stay identical.

**Static analysis and sanitizers (#63, #70).**
Three analysers (Microsoft's `/analyze`, GCC's `-fanalyzer`, clang-tidy's bug checks) run on the core library, and
any finding fails the build. AddressSanitizer and UndefinedBehaviorSanitizer run over every replayed session on
Windows and Linux.

## M5: modern error handling, and two dead features removed

**The crash report removed (#74).**
The old client wrote `error.txt` when it crashed and sent it to the game server at the next start. Those servers are
gone, so the report, its dialog and its network message were removed (a deliberate difference). An error now shows
the message box and goes to a developer log (#77).

**The hand-written stack trace removed (#78).**
Every function in the original caught every error, added its own name and line, and threw it again (1,174 try
blocks), only to fill that crash report. With the report gone, all of it was deleted.

**The built-in updater removed (#75).**
Any server could replace the game's files and start a program on the player's computer. That was a security risk,
so it was removed (a deliberate difference).

**Modern assertions and exceptions (#79, #80).**
Checks became `PATELLA_ASSERT`, which records its own file and line, and errors became small typed exceptions. A
failure that can be recovered from returns `std::expected`, a value that is either the result or the error.

## M6: the old 32-bit build retired

**The point of no return (#91 to #94).**
Until now, every change had to keep a 32-bit build that matched the original byte for byte, so the old memory
layout could not change. Once the golden checks ran on 64-bit Windows and on Linux, and the test mode had replaced
the memory reading, the 32-bit build was removed (tag `layout-frozen-final` keeps the last copy). *Why it matters:*
from here on the data structures themselves can be modernized.

**One settings file (#45).**
`Patella.ini`, a readable text file next to the game, replaced the original's binary `Tibia.cfg`. The old file is
imported once. It is stored as UTF-8 since #117.

## M7: data structures and architecture (done, November 7, 2026)

**Standard containers (#99, #100, #101, #102, #103).**
The original used its own homemade list types, including one indexed from any number (even negative) that grows
when you write past its end. These became standard C++ containers where possible: `std::vector`, `std::flat_map`,
fixed-capacity lists, `std::mdspan` grids for the map and the automap, and typed handles instead of plain numbers
for the effect table. The few places that really need the old behaviour keep it, each with the reason written down.

**From global variables to objects (#104 to #107, #116, #118).**
The original keeps its whole state in hundreds of global variables that every part of the program reads and writes
directly. That state now lives in a few clear objects, all owned by one `TClient`: the network (`TNetwork`), the
conversation with the server (`TCommunication`), the game world (`TMapState`), the graphics (`TGraphics`), the
interface (`TGUI`) and the preferences (`TPreferences`). *Why it matters:* each part can be understood, tested and
moved on its own.

**No more tangled files (#118, #116, #119).**
Some source files depended on each other in circles, so none of them could be separated. The last of these circles
is gone: the client has no circular dependency between its files any more. This is what makes it possible to split
the code into libraries and, later, into C++ modules.

**Cleaner object lifetimes (#108).**
Windows and dialogs close themselves in an explicit step before they are destroyed, so destructors can never throw,
and the main window and the tab bars are owned by smart pointers.

**A platform layer (#109, #119, #138).**
The game code used to call Windows directly for the clock, files, network, clipboard, mouse, window and the drawing
surface. It now calls small interfaces (`IClock`, `IFileSystem`, `ISocket`, `IClipboard`, `IInput`, `IWindow`,
`TSurface`) that have a Windows version and a test version. *Why it matters:* M8 adds a cross-platform version of each
(using SDL3), which is how the client will run on Linux.

**Text encoding at the edges (#112, #117).**
Inside, the game keeps the original's 8-bit Western European text, as the protocol requires. At the edges (the
clipboard, file names, the log, the settings file) it converts to and from UTF-8, the encoding every modern system
speaks.

**`import std` (#113, #121).**
New code can import the whole C++ standard library as a module (`import std;`) with Microsoft's compiler, GCC and
Clang.

**The heart of the game moves into the portable library (#110).**
The code that talks to the server (the network buffers and every message the client sends and understands) and
the game world (objects, the map, creatures, the inventory) now build as part of `patella_core`, the library that
contains no Windows code at all. The library grew from about 3,700 to about 12,300 lines, and it builds and is
checked on Windows and Linux with the strictest analysers. *Why it matters:* the core of the game no longer
depends on Windows, which is half of what M7 set out to do; the preferences and the interface's logic follow in
#111.

**The whole game, without Windows, in the portable library (#111).**
The rest of the game followed: the settings, the chat consoles, the hints, the picture caches and the whole interface
(every window, dialog, button and list, and the code that runs the game's main loop) now build inside `patella_core`.
The library grew from about 12,300 to about 38,700 lines and now holds everything except the "platform layer", the
small part that opens the window, draws with the graphics card and talks to Windows. A test proves that the library
needs nothing else. *Why it matters:* this is M7's goal reached: the game itself no longer depends on Windows, so
replacing the platform layer (M8, with SDL3) is all it takes to run it on Linux.

**Test replays without network ports (#120).**
The test client used to replay a recorded server over real network ports, so two test runs at once would collide.
The replay now runs inside the program, so many runs can happen side by side.

**Test runs without a window (#145).**
Every automatic test of our own client used to open a real game window on the screen, which popped up in front of
whatever the owner was doing and made parallel test runs fight over the one desktop. The test client can now run
"headless": the window exists inside the program but is never shown, and the graphics are drawn into memory instead
of onto the screen, so the pictures the tests compare are exactly the same. The replay comparisons, the determinism
check and our client's rendering runs now work this way by default, several at a time, while someone uses the
computer. Only the runs of the original Tibia.exe (and a few that test the window itself, such as fullscreen) still
need the desktop.

## M8: one platform layer on SDL3 (in progress)

**The game window now comes from SDL3 (#122).**
The client's window and its main loop (the part that waits for events and keeps the game ticking) were built on
Microsoft's MFC, a Windows-only framework from the 1990s. They now come from SDL3, a widely used cross-platform library,
fixed to one exact version and built with the project. The window looks and behaves the same: same size and position
from the settings file, same frame, same timing of events, and every recorded test session still plays back
identically. *Why it matters:* this is the first step toward a Linux client. Keyboard and mouse handling still use
the Windows path for now (next step), and the old MFC window can still be chosen for comparison until MFC is removed.

**Keyboard and mouse now come through SDL3 (#123).**
The mouse wheel, the mouse pointer's shapes, holding the mouse during a drag and foreign-language text input in the
SDL3 window now come from SDL3 instead of Windows messages. On Windows, key presses, typed characters and mouse clicks
still come the Windows way, because SDL3 would treat some of them differently from the original game (for example
clicks and keys sent by another program). The rules the original game applied to them (which keys do what, which
characters reach the chat, what counts as a double click) were taken out into a small piece of code of their own that
works on any operating system and is tested on its own. Typed text is converted to the game's character set from
Unicode, so a letter the game cannot show (from a Polish or Russian keyboard, say) becomes a question mark instead of a
different, wrong letter. Recorded test sessions still play back identically. *Why it matters:* the input rules no
longer depend on Windows, and a Linux client can feed them from SDL3.

**The graphics now attach to the window through SDL3 (#126).**
The graphics engine used to hook its drawing area onto the window with a Windows-only call. It now asks SDL3 for it,
the same library that makes the window, through one small interface that also answers the window's size. Nothing
looks different: the same pictures, the same choice of graphics card or the built-in software renderer. *Why it
matters:* the graphics no longer depend directly on Windows, so the same code can draw into a Linux window later.

## M8 and M9, started early

**Big-number maths fully unit-tested (#128).**
All 3,201 cases of the old big-number test now run one by one as unit tests on every compiler, and the
protocol's unpadded RSA is documented as part of the protocol.

**The pattern matcher as modern C (#129).**
The 1986 regular-expression code compiles as C17 with zero warnings under strict settings, and a small class owns
each compiled pattern so nothing is freed by hand.

**The original's out-of-bounds reads closed (#72, #86).**
A malformed packet or an over-long name in the original made the program read memory past the end of its data.
These reads are now bounded. This is a deliberate difference, recorded in the tests.

**Hardened standard library (#133).**
Every build turns on the standard library's own safety checks (Microsoft's STL hardening, libstdc++'s
assertions). Indexing past the end of a container, for example, now stops the program instead of corrupting
memory. It costs about 2% in a realistic run.

**Fuzzing (#132).**
Eight fuzz targets feed the packet parser and every file loader millions of random, broken inputs on Linux and
Windows to find crashes. They found three bugs in our own settings code (fixed) and seven weaknesses in the
original's code (fixed in #139, below).

**Broken data and hostile servers handled safely (#139, and a change to #72).**
The seven weaknesses the fuzzing found in the original's code are fixed, each as a deliberate difference. A damaged
Tibia.dat or Tibia.pic no longer makes the program free memory it never allocated; a damaged picture can no longer
write past its buffer; a damaged map file with an impossible number of marks is treated as damaged instead of
eating memory for minutes; a message that does not fit no longer leaks memory. A server message that is broken
(too long for its own frame, too large for the receive buffer, or the wrong size for its encryption) is now thrown
away, noted in the developer log, and the game carries on with the next message, where the original read past its
data, froze waiting forever, or quit. *Why it matters:* a corrupt file or a hostile server can no longer crash,
freeze or corrupt the client; every one of these cases has a unit test and a fuzzing seed. An empty server message,
which made the original stop reading the messages after it and could freeze the connection, is now simply skipped
(#142).
