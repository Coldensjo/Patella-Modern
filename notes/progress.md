# Reconstruction progress

Read this and `git log` first when resuming.

## Backlog

1. Script.cpp — **done** (commit "Reconstruct Script.cpp").
2. Preferences/Script gap (0x4ddc00-0x4df6ca) — **done**: loader/saver/ExitPreferences in Preferences.cpp, inline code in Script.h.
3. Player.cpp, incl. the error.txt reader (0x4da140) — **done**.
4. Small gaps — **done**: ContextMenus/Control (Control.cpp), start/Communication
   (InitCommunication + STL), Status/SurfaceDX (Surface.cpp + SurfaceDX.cpp range).
5. Objects.cpp and the Objects/Player gap — **done** (Claude Opus 5.5 delegate, reviewed, merged). The gap is all Player.obj.
6. Map.cpp and the gaps on both sides — **done** (Claude Opus 5.5 delegate, third attempt, reviewed, merged).
7. MapWindow.cpp and the MapWindow/Network gap — **done** (issue #1, Claude Opus 5.5; the gap is Network.cpp).
7a. MainWindow.cpp and the MainFrm/MainWindow gap — **done** (issue #2, Claude Opus 5.5).
7b. DialogWindow.cpp and the Dialogs/DialogWindow gap — **done** (issue #3, Claude Opus 5.5).
7c. ContextMenus.cpp and the ContentWindows/ContextMenus gap — **done** (issue #4, Claude Opus 5.5).
7d. ConsoleWindow.cpp and the gaps on both sides — **done** (issue #5, Claude Opus 5.5; the second gap is ContentWindows.obj).
7e. ContentWindows.cpp — **done** (issue #6, Claude Opus 5.5; the gap before it was settled in issue #5).
7f. DialogElements.cpp and the DialogElements/Dialogs gap — **done** (issue #7, Claude Opus 5.5; the gap is Dialogs.obj).
7g. Dialogs.cpp with its leading run of header code — **done** (issue #8, Claude Opus 5.5).
7h. SurfaceDX.cpp with its trailing DirectDraw functions — **done** (issue #9, GPT-6.1-sol with Claude Opus 5.5 delegates; PR for review).
7i. surfaceDX9.cpp and the surfaceDX9/SurfaceGL gap — **done** (issue #10, Claude Opus 5.5; the gap is SurfaceGL.obj, merged with #9).
7j. SurfaceGL.cpp with its glh_extensions.h code (the surfaceDX9/SurfaceGL gap) — **done** (issue #11, Claude Opus 5.5).
7k. The library region and its third-party code (Henry Spencer's regexp) — **done** (issue #15, Claude Opus 5.5).
8. Creatures.cpp and the Control/Creatures, Creatures/DialogElements gaps — **done** (Claude Opus 5.5 delegate, reviewed, merged; the second gap is DialogElements.cpp + Container.h).
9. Talk.cpp and the gaps next to it — **done** (ChatGPT gpt-6.1-sol, sent back once for names/shims, then merged).
10. The remaining files in link order, smallest first. Done: sysinc.cpp, Help.cpp, Inventory.cpp, MainFrm.cpp, GUI.cpp, graphics.cpp (delegates, merged).
11. The remaining header code (Container.h, Talk.h, Utils.h, ConsoleWindow.h, GUI.h) — **done** (issue #13, Claude Opus 5.5; glh_extensions.h's is #11's).

## Current item

SurfaceGL.cpp done (issue #11, November 2, 2026): the three engines are reconstructed.
The remaining header code done (issue #13, November 2, 2026): every header function of symbols.tsv is in its
header (glh_extensions.h's with SurfaceGL.cpp, #11); containertest.cpp tests Container.h's instances.
The library region done (issue #15, November 2, 2026): every library function has a names.tsv row,
the regexp is src/Extern/regexp. The atoi callers fixed (issue #30).
Review of the low-confidence decisions done (issue #24, November 2, 2026): 80 behaviour-relevant entries
confirmed by the binary or a test, two static objects added (MainFrm.cpp's NoCallback, TRSA.h's vlong),
scripttest.cpp tests nextToken's length limits; summary table at the end of notes/decisions.md.
TRect's members (issue #29, November 2, 2026): TRect is {x, y, width, height} and the blit slots take
TRect FromRect/ToRect, both from the assertion texts and the callers' 16-byte blocks.
SetChildArea (issue #27, November 2, 2026): TWindow::SetChildArea(TComponent *Child, TRect Area); all 136
calls in the binary build the area as a 16-byte block. Ten areas built from two SkinProperties points are
#31.
SkinProperties point pairs (issue #31, November 2, 2026): the ten pairs of TGUIPoints that SetChildArea
took as one area are TRects (nothing else in the binary reads them); the callers pass them.
external.h removed (issue #14, November 2, 2026): every declaration is in its owner's header; the text
table is src/Text.h (147 entries, one static copy per including object, as the image's 16 copies).
Patella.exe builds (issue #16, November 2, 2026): Patella.sln/Patella.vcxproj and tools\build-patella.cmd,
real MFC 14 static, x86, no warnings; it starts from build\run and shows the start screen. compile-check uses
real MFC (TIBIA_MFC_SHIM=1 forces the shim). The resource check of #17 passes on Patella.exe.
Local test server (issue #18, November 2, 2026): C:\Tibia\gameserver on 127.0.0.1, started and stopped by
tools\run-testserver.ps1 / stop-testserver.ps1; tools/patch_client.py points copies of both clients at it
(server names and RSA modulus), tools/login_client.py logs in with account 772001. Tibia.exe and
Patella.exe both reach the character list, enter the game and log out, with no difference; the
screenshots of that run are white (the monitor was off). notes/testserver.md.
Network traffic compared (issue #19, November 2, 2026): tools/compare_traffic.py records both clients'
sessions through tools/proxy.py (decrypted with the server's key), replays each recording to both
clients and compares their packets, their memory state after every step (tools/client_state.py) and
Tibia.cfg: no differences, no client bug. notes/traffic.md.
Written files compared (issue #21, November 2, 2026): tools/compare_files.py plays seven sessions with both
clients (the option dialogs; a replayed walk over three floors with automap marks and a saved channel; two
forced errors; the error report flow; patches of all five files from crafted login server answers) and
compares every file they write: Tibia.cfg, the *.map files, Default.txt, error.txt, the patched files and
Tibia.bak are byte-identical, apart from the times (masked, two runs of Tibia.exe as evidence) and error.txt's
line numbers (mapped then; equal since issue #35). One client bug fixed: the UCRT's ctime pads the day with a space, MSVC 7.1's
with 0 (src/compat/crt71.h). notes/written-files.md.
Rendering compared (issue #20, November 2, 2026): tools/compare_rendering.py plays issue #19's session (replayed)
with both clients and every engine (DirectX 5, DirectX 9, OpenGL), windowed and fullscreen, and compares the
frames pixel by pixel; tools/capture/patcap.c reads each frame from the engine's own surfaces (no display
needed) and makes time virtual (timeGetTime, the wall clock and the main timer's ticks move only by the
script's steps). Every scene of every engine and mode is pixel-identical, except the measured frame rate in the
advanced graphics options (masked: two runs of Tibia.exe differ there). No client bug, no change to src/.
notes/rendering.md.
Original line numbers (issue #35, November 2, 2026): every error site names the original's line as its
macro's first argument (TASSERT(66, s!=NULL), TEND1(1280, Type)); tools/lines.py reads the lines from
Tibia.exe and checks all 2,089 sites (2,053 matched, 36 explained, 0 mismatches). The difftest compares the
lines exactly (same check counts), tools/compare_files.py compares error.txt byte for byte except the time.
Three reconstruction bugs fixed on the way (Talk.cpp IsTalkIgnored's TEND2, Map.cpp AppendObject's reason,
Communication.cpp's "unknown talk mode" reason). notes/decisions.md "Original line numbers (issue #35)".
Test mode, first part (Patella-Modern issue #21, November 3, 2026): PatellaTest.exe, the test build of the x64
client, sends timeGetTime, the wall clock and the frame timer through one IClock and plays a script of
keyboard, mouse and text events on a virtual clock (src/testmode, src/TestHooks.h); Patella.exe and the oracle
are unchanged by it (pecompare against main: 0 unexpected differences). tools/testmode_check.py is the
determinism check against the test server. notes/testmode.md.
Test mode, second part (Patella-Modern issue #22, November 3, 2026): PatellaTest.exe --test-out writes the
packet log (every packet before encryption and after decryption, the first packets with the RSA block in
plain), the state dump at a script's `dump` (client_state.py's state() and gui() as JSON, read in the process)
and the frame dump at a script's `frame` (the Vulkan engine's frame and its device-filtered pixels as PNG).
tools/testmode_check.py compares, in every run, the state dumps with client_state.py's reading of the same
moment and the packet log with the proxy's decrypted recording; offline: the formats' unit tests, the
checker's self-test on made-up data, the dump's keys against client_state.py's. Patella.exe and the oracle
unchanged (pecompare: 0 unexpected differences). notes/testmode.md, "The dumps".
Reference recordings (Patella-Modern issue #23, November 3, 2026; M2's exit): tools/reference.py record plays
every session of compare_traffic.py, compare_files.py and compare_rendering.py once with Tibia.exe under
patcap.dll against the replayed server side (two live recordings from the test server, the rest crafted) and
writes the reference set (the test-mode script the run makes, the server side, Tibia.exe's packets, states,
frames, files); tools/reference.py compare runs PatellaTest.exe on each script with --test-replay (the server
side replayed inside the client over loopback, the virtual clock held while the network is busy: no server)
and compares. All eight sessions match on the CPU driver (packets with keys and padding, 62 states, 61 frames,
every written file), 2 to 5 s each. Patella.exe and the oracle unchanged. notes/testmode.md, "Reference
recordings".
The test build in CI (Patella-Modern issue #25, November 3, 2026): the hosted CI compiles PatellaTest.exe's
sources without Tibia.exe (PATELLA_CLIENT_COMPILE_ONLY: the object library PatellaTest_compile, no resources, no
link), self-hosted.yml runs tools/testmode_check.py, and testmode_check.py waits for the server's "Server
Online!" after each start and runs a run once more whose game login got no answer (issue #21's cold start).
notes/testmode.md, "The cold start (issue #25)".
Patella.exe's sources in the hosted CI too (Patella-Modern issue #46, November 3, 2026): PATELLA_CLIENT_COMPILE_ONLY
also builds the object library Patella_compile, Patella.exe's sources with its own definitions (no
PATELLA_TEST_MODE), compiled in hosted.yml's job windows-test-build next to PatellaTest_compile. The compile flags
of all 36 sources are Patella.exe's; Patella.exe's and PatellaTest.exe's are unchanged against main. notes/decisions.md,
"Patella.exe's sources in the hosted CI (Patella-Modern issue #46)".

NULL to nullptr (Patella-Modern issue #32, November 3, 2026; M3's first change): modernize-use-nullptr through
tools/tidy.py on every target that compiles src/ (windows-x64 and oracle-x86), 1,223 replacements, plus 40 by hand
where the check does not look (Container.h's template parameter, _execl's sentinel); the 262 NULLs inside
TASSERT/TERROR/TEND arguments wait for M5 (macrocheck: 0 differences). The oracle Patella.exe and the x64
Patella.exe and PatellaTest.exe unchanged (pecompare); the clang-tidy baseline 31 to 21 findings.
notes/decisions.md, "NULL to nullptr outside the error macros (Patella-Modern issue #32)".
override (Patella-Modern issue #33, November 3, 2026; M3's second change): modernize-use-override through
tools/tidy.py on every target that compiles src/, 667 declarations in 16 headers and Network.cpp (21 of them
`virtual` before, 646 implicit overriders); no fix inside a TASSERT/TERROR/TEND argument (macrocheck: 0
differences). `final` left out: it devirtualizes calls and changes the oracle's code (it waits for M6). The
oracle Patella.exe and the x64 Patella.exe and PatellaTest.exe unchanged (pecompare); the clang-tidy baseline
unchanged (21). notes/decisions.md, "override on every overrider, and no final yet (Patella-Modern issue #33)".
using (Patella-Modern issue #35, November 3, 2026; M3's third change): modernize-use-using through tools/tidy.py
on every target that compiles src/ (six typedefs in ConsoleWindow.cpp, surfaceDX9.cpp and SurfaceVulkan.h), and
common.h's uint8, uint16 and uint32 by hand as aliases of std::uint8_t, std::uint16_t and std::uint32_t, each with a
static_assert of its size and type (unsigned char, short, int: the same types as before on every compiler);
.clang-tidy enables the check. The oracle Patella.exe and the x64 Patella.exe and PatellaTest.exe unchanged
(pecompare); macrocheck 0 differences; the clang-tidy baseline unchanged (21). notes/decisions.md, "using instead
of typedef; uint8/16/32 as the std types (Patella-Modern issue #35)".
C headers (Patella-Modern issue #40, November 3, 2026; M3's fourth change, first part): modernize-deprecated-headers
through tools/tidy.py on every target that compiles src/, 19 includes in 12 files (`<cstring>` for `<string.h>`
and so on, common.h's included; .clang-tidy enables the check with CheckHeaderFile). The oracle Patella.exe and
the x64 Patella.exe and PatellaTest.exe unchanged (pecompare); macrocheck 0 differences; the clang-tidy baseline
unchanged (21). Second part: misc-include-cleaner, on the translation units of every target, 0 findings after
196 includes added and two removed, placed by hand after each file's own includes (clang-tidy's insertions broke
the DIRECT3D_VERSION and GLH_EXT_SINGLE_FILE order); .clang-tidy enables it, with IgnoreHeaders for the Windows
SDK, MFC and the MFC shim, `IWYU pragma: keep` on the Text.h and Status.h includes that are there for the
objects' table copies, and `IWYU pragma: export` on common.h's TestHooks.h and TMyException.h. The headers on
their own are not done (their findings change Text.h's and Status.h's copies). Oracle unchanged; on x64 the new
`<ostream>` moves `<charconv>`'s digit-pair table in .rdata (code identical after address normalisation).
notes/decisions.md, "The <c...> forms of the C headers; each file includes what it uses (Patella-Modern issue
#40)".
Attributes (Patella-Modern issue #38, November 3, 2026; M3's fifth change, three commits): modernize-use-nodiscard
through tools/tidy.py on every target that compiles src/ (17 const member functions in 7 files; no caller discards
a result); `[[fallthrough]];` on the six cases that fall through (all already commented by the reconstruction, all
the original's, difftest drives them), with -Wimplicit-fallthrough (GCC level 5, Clang) on the product targets;
`[[maybe_unused]]` on 212 unused parameters in 18 files and instead of Talk.h's casts to void, ClientStateWin32.cpp's
`#pragma warning(disable : 4100)` and the golden runner's cast, with the unused warnings on for every C++ source,
the baseline files included (MSVC /w34100 /w34189 /w34505; -Wunused -Wunused-parameter
-Wunused-but-set-parameter). The oracle Patella.exe and the x64 Patella.exe and PatellaTest.exe unchanged after
each commit (pecompare); macrocheck 0 differences; the clang-tidy baseline 21 to 14 (the use-nodiscard entries).
notes/decisions.md, "[[nodiscard]], [[fallthrough]] and [[maybe_unused]] (Patella-Modern issue #38)".
Constants (Patella-Modern issue #36, November 3, 2026; M3's sixth change, by hand): the 38 constant `#define`s of
src/ become `inline constexpr int` in their headers (`constexpr int` in Map.cpp; vlong.cpp's BPU `constexpr auto`, a
size_t as before), with the macros' literals, so their types stay. The 26 used inside TASSERT arguments too: the
assertions stringize their parameter unexpanded, so the messages keep their text (macrocheck 0 differences). Left
as macros: those read by `#if` or by a header's own preprocessor checks, SurfaceGL.cpp's GL/WGL API names, the
resource IDs. The oracle Patella.exe and the x64 Patella.exe and PatellaTest.exe unchanged (pecompare); the
clang-tidy baseline unchanged (14). notes/decisions.md, "Constant #defines to constexpr (Patella-Modern issue #36)".
Enums (Patella-Modern issue #37, November 3, 2026; M3's seventh change, by hand, two commits): every enum of src/
says `: int` (MSVC's type for them; GCC and Clang had `unsigned int`, which promotes alike), with static_asserts of
its size and underlying type in every preset; Script.h's TOKEN, only assigned and compared, is an `enum class`. The
other ten stay unscoped: PROGRAM_ERROR and TALK_MODE are used as ints (TALK_MODE also in TASSERT texts), the unnamed
ones are int constants. The oracle Patella.exe and the x64 Patella.exe and PatellaTest.exe unchanged after each
commit (pecompare); macrocheck 0 differences; the clang-tidy baseline unchanged (14). notes/decisions.md, "Enums
with an explicit underlying type, then enum class (Patella-Modern issue #37)".
gotos (Patella-Modern issue #42, November 4, 2026; M3's eleventh change, by hand, one function per commit): 24 of the
25 gotos restructured (TBattleWindowBody::Draw's skin of 0 for none; the three WriteMaskedImage with an Index of -1
for a copy); ProcessEvents' jump to the binary's shared exit block 0x44a2f5 stays, with a NOLINT, and
cppcoreguidelines-avoid-goto is enabled. The touched functions' code changes (oracle: Draw and the GL and DX9
WriteMaskedImage; x64: TSurfaceVulkan::WriteMaskedImage only), nothing else does (function by function, every other
difference a moved address); difftest runs every new path, the Vulkan one is checked exhaustively. notes/decisions.md,
"The gotos: restructured, and the one shared exit kept (Patella-Modern issue #42)".
Device switches (Patella-Modern issue #52, November 4, 2026; M2): quick Automatic/GPU/CPU clicks froze the picture;
the client went on drawing and presenting, but the compositor kept the AMD Radeon iGPU's last cross-adapter frame on
the main window after that device was gone. Each device now presents in a child window of its own
(src/vulkan/PresentWindow.h), destroyed after the next device's first frame; the device test uses a hidden one.
tests_vulkan 13 test cases (quick switches on every device; a held click across the device windows' changes). The
test mode now ignores the desktop's capture changes with a script (PATELLA_TEST_CAPTURE_CHANGED; a reference
compare had lost a scripted click that way). The oracle unchanged (pecompare).
notes/vulkan.md, "One window per device (issue #52)"; notes/decisions.md, "One presentation window per device
(Patella-Modern issue #52)".
Handles across device switches (Patella-Modern issue #53, November 4, 2026; M2): the 56 handles per RTX, CPU, RTX
round are the drivers', none ours: lavapipe's rasterizer threads (24 per device here, never joined on Windows,
Mesa's lp_rast.c), its `thrd_current` (1 per instance) and NVIDIA's driver (3 events per device). Documented, not
worked around (the pinned driver). tests_vulkan's new "switching to and from the CPU driver leaves no handles of
ours" counts them apart and requires the rest flat. notes/vulkan.md, "Handles the drivers leave (issue #53)";
notes/decisions.md, "Handles the drivers leave (Patella-Modern issue #53)".
The window's background during a live resize (Patella-Modern issue #71, November 5, 2026; M2): dragging the border
larger showed stripes of old pixels in the uncovered area until the mouse was released. In the modal size loop no frame
is drawn, the device's window keeps its size, and the main window's class has no brush (as the original's; Tibia.exe
shows the same stripes). TPresentWindows now subclasses the main window and fills on WM_ERASEBKGND what no device
window covers black: a deliberate on-screen difference. tests_vulkan 15 test cases. notes/vulkan.md, "The main window's
background during a live resize (issue #71)"; notes/decisions.md, "The main window's background during a live resize
(Patella-Modern issue #71)".
The error report removed (Patella-Modern issue #74, November 5, 2026; M5's first issue, decision D2): no error.txt
is written or read, no error report dialog exists, no packet 0xE8 is sent (Player.cpp ReadErrorFile and HandleException's
file, Status.cpp's report list, Dialogs.cpp's TErrorReportDialog, Control.cpp's SendErrorReports,
OutputControlErrorInformations and events 0x2a/0x2b, Communication.cpp's SendDebugReport, MainFrm.cpp's
OutputErrorInformations, texts 117 and 118). A fatal error still ends with the "Tibia Error" box; FatalError writes its
caption and text to stderr, HandleException the exception's message; the box's text is the first sentence of the
original's. The comparisons expect the deliberate differences (tools/deliberate.py: reference.py, compare_files.py,
compare_network.py); compare_files.py's error-login and report sessions and lines.py are gone. difftest 16,599,178 /
67 / 1,114,552 / 3,201 (each drop counted by function); the golden corpus recorded again: 16,193,194 replayed +
1,520,603 retired = 17,713,797. notes/decisions.md, "The error report removed: error.txt, the report dialog, packet
0xE8 (Patella-Modern issue #74, D2)".
The built-in patcher removed (Patella-Modern issue #75, November 5, 2026; M5, decision D9): a patch packet from the
login server (0x1e, 0x1f, 0x20) ends the login in the login error dialog ("Sorry", "This server wants to update the
client"; Communication.cpp ReceiveUpdateRequest); no file is patched, replaced or started (ReceivePatch*, CancelPatch,
ApplyPatch, ApplyFullPatch, InstallPatch, the update dialog and texts 74-76, events 0x29 and 0x40-0x48, CloseProgram,
Run's `_execl`, MsvcExecl removed; CPlayerApp's RunPatch is padding). #67's zero-sized patch block cannot crash any
more (#67 stays open for its list of SEH-dependent catch blocks). compare_files.py's patch sessions and the reference
sessions patch and patch-exe are gone; tools/deliberate.py lists the differences. difftest 16,459,078 / 67 / 856,621 /
3,201 (each drop counted by function); the golden corpus recorded again: 15,802,944 replayed + 1,512,822 retired =
17,315,766. Found: TIgnoreListDialog cuts a 30-character name to 29 since #57 (not changed). notes/decisions.md, "The
built-in patcher removed: packets 0x1e, 0x1f, 0x20, Patch.exe (Patella-Modern issue #75, D9)".
The 30-character ignore names (Patella-Modern issue #85, November 5, 2026): a #57 regression found in #75.
TIgnoreListDialog keeps a name of 30 characters (its edit line's most) whole again, and the Tibia.cfg reader a buddy
description of 128 characters, as the original's strcpy did (the NUL in the next member, which the next statement
sets): TIgnoreEntry::AssignName and TBuddy::AssignDescriptionRead write over the rest of the structure. Every other
#57 conversion was checked: asserted, bounded by its source or sized by its allocation. New checks that run whatever
the random inputs: Dialogs 80 (LongIgnoreNames), PreferencesFile 179; without the fix they fail 21. difftest
16,459,337 / 67 / 856,621 / 3,201; golden 15,803,203 replayed + 1,512,822 retired = 17,316,025. notes/decisions.md,
"The 30-character ignore names and the 128-character buddy descriptions (Patella-Modern issue #85)".
TBEGIN/TEND deleted, part 1 (Patella-Modern issue #78, branch issue-78a, November 5, 2026; M5): the plan for all
seven parts is in notes/decisions.md ("TBEGIN/TEND deleted, file by file"). Part 1 deletes the 153 try blocks of
Script.cpp, Inventory.cpp, Player.cpp, Status.cpp, Objects.cpp, Map.cpp and Creatures.cpp (977 left in 15 files).
One kind change is kept at its boundary: InitObjects catches every exception of LoadObjects, so a damaged Tibia.dat
whose sprite count makes new[] throw still ends with PROGRAM_ERROR 1. One is recorded: with the heap exhausted,
readString's std::bad_alloc under LoadAutomapBlock now ends the client instead of emptying the automap block.
difftest 16,459,337 / 67 / 856,621 / 3,201 (unchanged); golden 15,803,203 replayed + 1,512,822 retired, 0 failures.
The developer log and the fatal-error path (Patella-Modern issue #77, branch issue-77, November 5, 2026; M5, D16):
src/log/Log.h, a small log of our own (levels error, warn, info, debug; lines "[<level>] <message>" on stderr and an
optional file; std::format or the C++17 stand-in), Patella.ini's LogLevel and LogFile (src/log/LogSettings.cpp, x64
client; defaults info and no file). FatalError and HandleException write through it at error; HandleException writes
error.txt's old diagnostics at debug (engine, last packet types, last packet, player position); Debug builds add the
std::stacktrace (MSVC, libstdc++ with libstdc++exp; fallback line on mingw-w64 and the oracle) and break into an
attached debugger. oracle-x86: the log at its defaults, no Patella.ini. Counts unchanged; notes/decisions.md, "The
developer log and the fatal-error path (Patella-Modern issue #77, D16)".

TBEGIN/TEND deleted, part 2 (Patella-Modern issue #78, branch issue-78b, November 5, 2026; M5): the 181 try blocks
of graphics.cpp, GUI.cpp, MainWindow.cpp, MapWindow.cpp and MainFrm.cpp go (796 left in 10 files). Kept: the four
loaders' file errors to PROGRAM_ERROR 2-5, MainWindow::Draw's start window to PROGRAM_ERROR 12, AddLightSource's
message. No kind change reaches another handler; one is recorded: with the heap exhausted, MFC's CMemoryException in
a frame handler is now reported by MFC's window procedure instead of ending the client. difftest 16,459,337 / 67 /
856,621 / 3,201 (unchanged); golden 15,803,203 replayed + 1,512,822 retired, 0 failures.
TBEGIN/TEND deleted, part 3 (Patella-Modern issue #78, branch issue-78c, November 6, 2026; M5): the 168 try blocks
of Talk.cpp, ConsoleWindow.cpp and ContextMenus.cpp go (628 left in 7 files); none of the three files has a handler of
its own, so no catch stays. No kind change reaches another handler; two are recorded, both only with the heap
exhausted: a filter list's std::bad_alloc while InitPreferences reads Tibia.cfg now ends the client instead of
setting the defaults, and under /EHa only (difftest) a nothrow null's access violation in SavePreferences' list texts
is no longer ignored. difftest 16,459,337 / 67 / 856,621 / 3,201 (unchanged); golden 15,803,203 replayed + 1,512,822
retired, 0 failures.


TBEGIN/TEND deleted, part 4 (Patella-Modern issue #78, branch issue-78d, November 6, 2026; M5): the 177 try blocks
of ContentWindows.cpp and DialogWindow.cpp go (the content windows, the sub-windows and the side panel's columns);
neither file has a real handler, so no catch stays. No kind change reaches another handler (only fixed-size
allocations in the part); recorded as in part 2: with the heap exhausted, MFC's CMemoryException under the window
procedure is now reported by MFC instead of ending the client. difftest 16,459,337 / 67 / 856,621 / 3,201
(unchanged); golden 15,803,203 replayed + 1,512,822 retired, 0 failures.

TBEGIN/TEND deleted, part 6 (Patella-Modern issue #78, branch issue-78f, November 6, 2026; M5): the 129 try blocks
of Dialogs.cpp go (499 left in 6 files on the branch, parts 4, 5 and 7); the file has no handler of its own, so no
catch stays. No kind change: every path into the dialogs comes through Control.cpp's remaining TENDs or
MainWindow::Draw's `catch (...)`, and no allocation's size comes from input unbounded (the channel list's count is a
byte). difftest 16,459,337 / 67 / 856,621 / 3,201 (unchanged); golden 15,803,203 replayed + 1,512,822 retired, 0
failures.

TBEGIN/TEND deleted, part 5 (Patella-Modern issue #78, branch issue-78e, November 6, 2026; M5): the 141 try blocks
of DialogElements.cpp go (310 left in 4 files after parts 4 and 5); the file has no handler of its own, so no catch
stays. No kind change reaches another handler: none of the handlers that catch a TMyException by type reaches the
part, and its only third type is new's std::bad_alloc of bounded sizes (an exhausted heap: the same box as before).
difftest 16,459,337 / 67 / 856,621 / 3,201 (unchanged); golden 15,803,203 replayed + 1,512,822 retired, 0 failures.

TBEGIN/TEND deleted, part 7, the last (Patella-Modern issue #78, branch issue-78g, November 6, 2026; M5): the 181 try
blocks of Communication.cpp, Network.cpp and Control.cpp go, then the macros out of src/common.h and .clang-format,
and every comment of src/ that named them says "the original's catch wrapper": no TBEGIN or TEND is left in src/
(1,130 try blocks over the seven parts). Kept: the encryption failures, the cylinder loop, CheckSurface,
ChangeEngine, InitControl. No kind change reaches another handler; recorded as in parts 2 and 4: MFC's
CMemoryException under the socket window's or the frame's window procedure is now reported by MFC. EnterGame stays
out of line (PATELLA_NOINLINE: the golden Communication test fakes it); the Control test models the two effects of
the original's wrappers its error round sees (MouseMove's extra GetActionState, a structured exception made a
TMyException). difftest 16,459,337 / 67 / 856,621 / 3,201 (unchanged); golden 15,803,203 replayed + 1,512,822
retired, 0 failures. notes/decisions.md, "Part 7" and "The closing summary".
TASSERT* to PATELLA_ASSERT (Patella-Modern issue #79, branch issue-79, November 6, 2026; M5): src/compat/assertion.h's
PATELLA_ASSERT(expr) and PATELLA_ASSERT_VALUES(expr, values), a function-based check with std::source_location (an
aggregate stand-in in the oracle's C++17) that throws the same TMyException with the assertion's own file and line; no
contract_assert (GCC 16 could throw only through the program's one violation handler, which -fcontract-evaluation-
semantic can bypass and which cannot pass a predicate's exception on). 802 sites in 30 files converted by a script,
the three macros and nine local bug variants deleted (the CipSoft bug numbers dropped), Utils.cpp's 16 NULL in
assertions nullptr (tidy baseline 20 to 4). TPlayerStates::GetIcon's unreachable TERROR1 default left out: MSVC x86 had
kept its exception frame without the handler in the SafeSEH table. notes/decisions.md, "TASSERT* to PATELLA_ASSERT".
The SafeSEH gate (Patella-Modern issue #89, branch issue-89, November 6, 2026): tools/safeseh_scan.py finds the x86
exception-handler registrations (C++ EH prologues, SEH3/SEH4 frames, the CRT's guard records, __EH_prolog3 and its
variants) and fails on a handler missing from the image's SafeSEH table or an image without one; names from the PDB
or a map, else RVAs; `--self-test` on made-up PE32 images. oracle-x86's CTest test `safeseh` runs it on difftest,
sysinctest, communicationtest, vlongtest and Patella.exe (CTest 5/5). notes/decisions.md, "The SafeSEH gate".
TMyException with a source location, TProgramError, std::expected (Patella-Modern issue #80, branch issue-80, November 6,
2026; M5): 80a: TMyException is a message and a std::source_location (the stand-in in the oracle's C++17); the
original constructors' formatting is patella::ErrorText and ErrorOrigin, compared with the original's as before;
TMYEXCEPTION, TERROR* and PATELLA_ASSERT build it through MakeError; the original's line argument is gone from 132
sites. 80b: PROGRAM_ERROR is an enum class with named codes, thrown inside TProgramError (62 throws, 7 catches);
golden_eh's OurThrow gives it the original's kind (a PROGRAM_ERROR with its value). 80c: compat/expected.h
(std::expected, a C++17 stand-in) and TError; the four file classes' TryOpen return open's failures, open still
throws them, the automap's load and save test the result; the other recoverable paths are listed with why they stay.
difftest 16,459,337 / 67 / 856,621 / 3,201 (unchanged); tidy baseline 4 to 3. notes/decisions.md, "TMyException with
a source location, TProgramError, std::expected".
unique_ptr for the owners blocked by noexcept(false) destructors (Patella-Modern issue #81, branch issue-81, November
6, 2026; M5's last): `~TTalkBuffer` is noexcept (it cannot throw: Clear only removes the first line), so
`TConsoleWindow::History` is a std::unique_ptr; `TEditLine::Text` and `TPasswordEditLine::Stars` are
std::unique_ptr<char[]> (Draw's swaps `Text.swap(Stars)`). `TComponent`'s destructor stays noexcept(false): `~TTab`
asserts and sends, and the golden ConsoleWindow test throws through it; so `TTabBar::LeftTabs`/`RightTabs`,
`ModalWindow` and `MainWindow` stay raw until M7 (notes/ownership.md section 7). difftest 16,459,337 / 67 / 856,621 /
3,201 (unchanged); golden 17,316,025, 0 failures. notes/decisions.md, "unique_ptr for the owners blocked by
noexcept(false) destructors".
A fatal error before the main window is set (Patella-Modern issue #83, branch issue-83, November 6, 2026; M5): a
failing first present (#56: the CPU driver on a hidden desktop) crashed the x64 client in FatalError, not in
ExitControl: InitInstance's first Redraw comes before m_pMainWnd is set, and `AfxGetMainWnd()->ShowWindow` ran on
null. Tibia.exe makes the same call, inside a `catch (...)` that MSVC 7.1 lets catch the access violation, and shows
the box; ours now skips ShowWindow without a main window and ends the same way ("Tibia Error", exit code 1). The test
build's `PATELLA_TEST_FAIL_PRESENT=1` (hook PATELLA_TEST_PRESENT_FAILURE) reproduces it. difftest 16,459,337 / 67 /
856,621 / 3,201 (unchanged); golden 17,316,025, 0 failures. notes/decisions.md, "A fatal error before the main
window is set".
The catch blocks that relied on structured exceptions (Patella-Modern issue #67, part 2, branch issue-67b, November 6,
2026; M5): the 43 `catch (...)` of src/ decided (21 not SEH-dependent, 17 a driver's or the oracle's engines' traps, 2
fixed, FatalError's ShowWindow settled in #83, 2 left to #83 and not fixed there: InitControl's ExitControl and
FatalError's SetDisplayMode step). PATELLA_DIVISOR
(compat/assertion.h) throws the original's TMyException where a divisor from input traps: the game login's Beat in
StartMovement, Tibia.dat's pattern sizes, phases and exact sizes in six functions. The CTest fatal_error_box checks #83's
ShowWindow fix from a start-up error without a Vulkan driver (the box, exit code 1).
difftest's Divisions test (15 checks; ported, `Since = "#67"`: main's corpus lacks it); difftest 16,459,352 / 67 /
856,621 / 3,201; golden 17,316,025, 0 failures. notes/decisions.md, "The catch blocks that relied on catching
structured exceptions".
A PROGRAM_ERROR in InitControl (Patella-Modern issue #90, branch issue-90, November 6, 2026; M5): a missing Tibia.dat
(or any failing InitControl step) crashed the x64 client in ExitControl (`GetSurface()->ExitSurfaces()` on the NULL
surface) and then in FatalError's SetDisplayMode (CWnd::ModifyStyle on the freed frame). Tibia.exe faults in both
places too (cdb) and its `catch (...)` blocks swallow the faults before the box; ours now skips ExitSurfaces without a
surface and SetDisplayMode without a main window or a surface, and ends with the box and exit code 1. The test build's
`PATELLA_TEST_FAIL_INIT=<n>` (hook PATELLA_TEST_INIT_FAILURE) fails InitControl before step n; the CTest
init_control_failure runs a missing Tibia.dat and steps 1-16. difftest 16,459,352 / 67 / 856,621 / 3,201 (unchanged);
golden 17,316,040 (main's re-recorded corpus, with Divisions), 0 failures. notes/decisions.md, "A PROGRAM_ERROR in
InitControl".
The comparisons read the x64 client through its test mode only (Patella-Modern issue #92, branch issue-92, November 6,
2026; M6): PatellaTest.exe's control channel (`--test-control`: `probe` writes probe.json, the state dump's state and
window tree plus the character list, skin values, components' members, the map's names and the Vulkan device; with a
script, script lines after the last event) and tools/testmode_client.py. An x64 `--patella` of compare_traffic.py,
compare_files.py and compare_rendering.py runs the test build next to it and reads it only through the channel
(compare_rendering.py: `clock manual`, `cursor`, advance, frame and post lines instead of patcap.dll).
tools/client_state.py refuses a 64-bit process, patcap.c does not build for x64 (windows-x64 builds no patcap.dll);
both stay for Tibia.exe and the oracle. testmode_check.py's e checks each dump on its own. Patella.exe unchanged
(pecompare). notes/decisions.md, "The comparisons read the x64 client through its test mode only".

TDialogWindow::SetSize and the child its own SetSize deleted (Patella-Modern issue #65, branch issue-65, November
6, 2026; M6): the loop went on through the Next of a child that its own SetSize had deleted (FitChildren), as the
original's did (a use after free that read NULL). It now ends when the child is no longer in the column: the same
outcome, without the read. The CTest dialogwindow_setsize (tests/golden, the runner's objects, no corpus) is a
heap-use-after-free under windows-x64-asan before the fix and passes after it. The golden runner's operator delete for
DialogWindow's test is gone, and the Debug runner passes without it. The MainWindow test's TFakeMapFrame frees without
a size (the operator delete had hidden ASan's size check). difftest 16,459,352 / 67 / 856,621 / 3,201 (unchanged);
golden 17,316,040, 0 failures. notes/decisions.md, "TDialogWindow::SetSize and the child its own SetSize deleted".

The test server's bind errors (Patella-Modern issue #95, branch issue-95, November 6, 2026): the 10049 of #88 and
#92 is the server's own start-up race (its login thread copies the bind address from a buffer the second ReadConfig
rewrites; a half-written copy is not a local address); excluded port ranges, TIME_WAIT and the virtual adapters
ruled out. tools/testserver.py fails an attempt at once on the bind error line, says the code's meaning and any
excluded range, stops its server once online and retries (`--bind-retries` 3, on top of `--retries`). 10 cold
starts: 9 bound at once, 1 hit 10049 and the retry bound. notes/decisions.md, "The test server's bind errors".

The golden corpus on Linux (Patella-Modern issue #91, branch issue-91, November 6, 2026; M6): golden_runner (and
golden_concurrent, dialogwindow_setsize) build in linux-x64-gcc and linux-x64-clang against the Windows stand-ins of
tests/golden/linux and replay the corpus, which tools/build.py mounts read-only at /golden with Windows' time zone.
The port found an evaluation-order read of freed memory (Status.cpp AddMessageFirst), unaligned XTEA words, the flag
bytes past Flags[4] (Objects.cpp), an unset bool read (TTabBar), the entries' padding of vector growth (Container.h),
and calls whose order the original fixes (Control.cpp, MainFrm.cpp, ContentWindows.cpp, ContextMenus.cpp): fixed.
golden 15,803,218 replayed + 1,512,822 retired, 0 failures on both compilers (Clang under ASan and UBSan, no finding).
notes/decisions.md, "The golden corpus on Linux".

The start's window count in the test build (Patella-Modern issue #96, branch issue-96, November 6, 2026):
fatal_error_box failed with "A Tibia client is already running!" whenever another client's window was open on the
machine. The check is not a mutex but InitInstance's EnumWindows count of the windows titled "Tibia" or of class
"TibiaClient". The test build's hook PATELLA_ENUM_WINDOWS (src/TestHooks.h) hands CountTibiaWindows only the test
client's own windows; Patella.exe's EnumWindows is unchanged (pecompare: identical apart from link stamps). No
RESOURCE_LOCK: the two tests no longer see each other. notes/decisions.md, "The start's window count in the test
build".

The final oracle recording (Patella-Modern issue #93, branch issue-93, November 6, 2026; M6): the golden corpus and the
reference recordings are frozen, each with a MANIFEST.tsv in its folder (tools/data_manifest.py: every file's size and
SHA-256, the commit, the tools, the date, the time zone; local, with the data), which golden_runner and reference.py
compare check before they replay and refuse when missing or mismatched (`--unverified`, PATELLA_GOLDEN_UNVERIFIED=1:
local experiments only). golden_corpus records with TZ=UTC0 and the runner replays in the manifest's zone (#91's open
point: the corpus no longer depends on the recording machine's zone). The final corpus: commit ab1391c, 2026-11-06
12:00:38 UTC, 43 files, 908,993,929 bytes, reproducible (golden_reproducible 0 different, and two recordings of the
corpus identical); in `C:\Tibia\Patella-Modern\build\golden-final` for the orchestrator to install. The reference set
of #23 unchanged (compare matches), its manifest in a copy, `build\reference-final`. Regenerated only from the tag
`layout-frozen-final` of #94; new checks are unit tests or test-mode recordings against Tibia.exe. notes/decisions.md,
"The final oracle recording".

The oracle retired (Patella-Modern issue #94, branch issue-94, November 6, 2026; M6 done): no 32-bit client build in
the fork. Removed: the oracle-x86 preset and cmake/OracleX86.cmake with difftest, sysinctest, communicationtest,
vlongtest and safeseh, golden_corpus and golden_reproducible, tools/difftest, compare_imports.py, compare_resources.py,
safeseh_scan.py, the engines SurfaceDX/surfaceDX9/SurfaceGL and Extern/glh, PATELLA_ORIGINAL_LAYOUT with its 73 x86
asserts, tests/golden's GOLDEN_ORACLE side and golden.cpp's recorder. The automap file's layer sizes are checked at
Map.cpp's reader and writer. New: capture-x86, only the 32-bit patcap.dll for Tibia.exe. The tools take the x64 client
by default; the self-hosted workflow replays the frozen corpus and builds capture-x86. Golden 17,316,040 (15,803,218 +
1,512,822), 0 failures on every preset; the x64 Patella.exe differs from main's only in line-number immediates.
notes/decisions.md, "The oracle retired".

Patella.exe without the patcap.dll lookup (Patella-Modern issue #97, branch issue-97, November 6, 2026): the Vulkan
engine no longer asks for patcap.dll and its four Vulkan exports; the test build's frame dump is its only frame and
device observer (PATELLA_TEST_FRAME_OBSERVERS without its patcap argument). PatellaTest.exe's `--test-sync`, unused
since #92, is gone. The x64 Patella.exe differs from main's only in CreateSurfaces (pecompare, byte by byte: every
other function the same or moved by the 0x70 bytes less code). notes/decisions.md, "Patella.exe without the
patcap.dll lookup".

Patella.ini as the only settings file (Patella-Modern issue #45, branch issue-45, November 6, 2026; M6): every option
of Tibia.cfg is a key of Patella.ini under Tibia.cfg's name (src/settings/PreferenceKeys.h: flags yes/no, numbers,
comma lists, quoted texts with `\` escapes, Latin-1 bytes as they are; Hotkey<N>, ContentWindow<n>, Buddy<n>). The
first start without them imports Tibia.cfg once through the original's reader (ReadTibiaCfg) and writes Patella.ini at
once; the client never writes Tibia.cfg (the original's writer, WriteTibiaCfg, stays for the golden tests: no check
retired, 17,316,040). Tests: tests_core (keys, escaping) and preferences_file (the import with the client's own code).
The comparisons compare the options by value (tools/settings_file.py: Tibia.exe's Tibia.cfg against the x64 client's
Patella.ini); two new deliberate differences (tools/deliberate.py). notes/decisions.md, "Patella.ini as the only
settings file".
TIndexedVector over std::vector (Patella-Modern issue #99, branch issue-99, November 6, 2026; M7): Container.h's
vector<T> is TIndexedVector<T> of src/types/IndexedVector.h, the original's semantics (min..max, growth by block or
doubling, the init fill, #91's padding-carrying swaps) over a std::vector whose allocator allocates as new T[n]; all 24
objects and 4 functions' parameters converted, Container.h keeps array, matrix and matrix3d. Golden 17,316,040, 0
failures on every preset; tests/unit/indexed_vector_test.cpp on every compiler. #100 moves the plain call sites to
standard containers. notes/decisions.md, "TIndexedVector over std::vector".
Fixed-capacity lists (Patella-Modern issue #102, branch issue-102, November 6, 2026; M7): Map.cpp's effect table is a
TSlotTable<TEffect, EFFECTS> (src/types/SlotTable.h) over TStaticVector (std::inplace_vector with GCC 16): it only
grows and reuses free entries, so every effect keeps the original's number, and a full table drops the new effect
(-1) as the original's NewEffect did. tests/unit/slot_table_test.cpp on every compiler (against the original's
search). TMapPoint::Objects is not converted: the golden Container test compares the bytes new TMapPoint[d] leaves,
which a vector's constructor changes; the owner decides (notes/decisions.md, "#102: fixed-capacity lists").
TMapPoint's objects (Patella-Modern issue #102, branch issue-102b, November 6, 2026; M7): TMapPoint (src/MapPoint.h)
keeps its objects in a TStaticVector<TObject, 10>, its size the count, with the original's full point (InsertObject's
top object falling off, AddObject's drop, AppendObject's throw, DeleteObject's true) and its Objects[0] of an empty
point (Vacated). The golden Container test's array<TMapPoint> round has the original's point as its element type
(it tests array<T>); every check unchanged. tests/unit/core_map_point_test.cpp. notes/decisions.md, "#102b".

compare_rendering.py's failures explained (Patella-Modern issue #98, branch issue-98, November 6, 2026): Tibia.exe's
first capture got no frame when its start-up's second IDirect3D9::CreateDevice did not return (1 start of 15 in the
repro, inside the driver); the tool now waits for the first frame and starts a client without one once more. The
x64 client's "late" trade window was a minimized window (0x0 frames, the click on the partner missed), most likely
the DisplayPort monitor asleep after the display timeout (posted messages are not input): the tool keeps the display
on (SetThreadExecutionState) and fails a minimized scene at once. Every failure names the window's, the display's and
patcap.dll's Direct3D 9 state (patcap.dll's shared memory version 4 and patcap.log; compare_rendering.py
--self-test). patcap.dll's four PatcapVulkan* exports, unused since #97, are gone. notes/rendering.md, "When a run
fails"; notes/decisions.md, "compare_rendering.py's failed starts and runs explained".

import std (Patella-Modern issue #113, branch issue-113, November 6, 2026; M7): `import std;` works with MSVC 14.51,
mingw-w64 GCC 16.2 and GCC 16.2 under CMake 4.1.2 (and MSVC under 4.4.5), not with Clang 22 in the image (Debian's
libstdc++ 14 has no std module). The option PATELLA_IMPORT_STD (cmake/ImportStd.cmake) is ON in windows-x64,
windows-x64-asan, windows-x64-gcc and linux-x64-gcc; tests_import_std (tests/unit/import_std_test.cpp) is its one
user, and the std module gets that target's options. The Linux image has Kitware's CMake 4.1.2 instead of Debian's
3.31.6. Clang's unblock (GCC 16's libstdc++ or libc++) is the owner's decision. notes/decisions.md, "import std".

TClient and TPreferences (Patella-Modern issue #104, branch issue-104, November 6, 2026; M7's first issue): the design
of M7's globals-into-subsystems (the objects, which object owns each global, the order of #105-#111, the bridge,
golden_runner's and the test mode's access, the module cycles) is notes/decisions.md, "#104: TClient and the subsystem
objects". First move: Preferences.cpp's 43 globals are TPreferences' members (Preferences.h), owned by TClient
(src/Client.h, Client.cpp: one lazy instance during the transition, bridge::Client()); the original's free functions
forward to bridge::Preferences(), so no caller changed. Talk's White and Ignore lists through TTalkLists: the module
cycle {Preferences, Talk, graphics, Surface} is {Surface, graphics} now. Golden 17,316,040, 0 failures.
Text encoding at the platform boundary (Patella-Modern issue #112, branch issue-112, November 6, 2026; M7, D4): the
client keeps Windows-1252 inside; src/types/Cp1252.h converts it to and from UTF-8 and UTF-16 (the five undefined bytes
as U+0081..., '?' for a character without a byte and for ill-formed input; unit tests on every compiler, on Windows
against Windows' code page 1252). The clipboard is CF_UNICODETEXT, TFile opens a name as its Unicode name (_wfsopen on
Windows, UTF-8 elsewhere), the developer log writes UTF-8 lines, the Vulkan device names come into the client's code
page; the other boundaries are ASCII and stay, the keyboard and the CPU driver's path wait for M8. Golden 17,316,040,
0 failures. notes/decisions.md, "#112: text encoding at the platform boundary".

TNetwork and TCommunication (Patella-Modern issue #105, branch issue-105, November 6, 2026; M7): Network.cpp's 17
globals, MainFrm.cpp's PacketHeader and Control.cpp's reconnect pair are TNetwork's members (Network.h), Communication.cpp's
globals and Control.cpp's packet types TCommunication's (Communication.h), Status.cpp's session (connection state,
account, character list, rights) TSession's (Status.h), which TCommunication owns; TClient makes TPreferences, TNetwork,
TCommunication. The free functions forward through the bridge, so no other caller changed. TNetwork takes TPreferences
and TNetworkSession (Status.cpp's functions, Client.cpp's adapter); TCommunication sends and reads through its TNetwork.
The golden tests reach the members (the Communication world fakes the members too). Module cycles: main's 12-module
cycle is one of 6 and {MainFrm, Player}; Network and Communication are in none. Golden 17,316,040, 0 failures.
notes/decisions.md, "The second move: TNetwork and TCommunication".
Golden on the Debug presets (Patella-Modern issue #115, branch issue-115, November 6, 2026; M7): the Communication
test's check that ExitCommunication is empty (the only one, since the fakes replace it in every comparison) accepted
`ret`, `ret 0` and Clang's frame-pointer frame; it now also accepts GCC's -O0 frame with its nop (55 48 89 E5 90 5D C3),
and nothing looser. linux-x64-gcc-debug and linux-x64-clang-debug replay the corpus, 17,316,040, 0 failures.
notes/decisions.md, "#115: an empty function's code at every optimisation level".

Patella.ini in UTF-8 (Patella-Modern issue #117, branch issue-117, November 6, 2026; M7, the owner's decision): the file
is UTF-8 with PreferencesVersion=2; its texts (hotkeys, buddy descriptions, the lists' names, LogFile) are converted to
the client's Windows-1252 when read ('?' for a character it lacks), the escapes stay (and the five undefined bytes are
escaped); a version-1 file is read as Windows-1252 and rewritten whole as version 2 at the next save.
tools/settings_file.py reads both versions by value. notes/decisions.md, "#117: Patella.ini in UTF-8".


TMapState (Patella-Modern issue #106, branch issue-106, November 6, 2026; M7): the game state's globals are the members
of five objects that TMapState owns (src/MapState.h) and TClient makes after TNetwork: TObjectTypes (Objects.cpp),
TMap (Map.cpp), TCreatures and TPlayerState (Creatures.cpp's creature and buddy tables, and the player's values and
walk) and TInventory (Inventory.cpp). Each file's functions are members of the same names; the free functions forward
through the bridge, so no other caller changed; TCreatures asks the later TPlayerState through TCreaturesPlayer, which
TMapState implements. TCommunication writes the game state through references to the five. The golden tests read the
members and fake them too (the Communication world, the Creatures test). Module cycles: main's 3, unchanged; the
game state is in none. Golden 17,316,040, 0 failures. notes/decisions.md, "The third move: TMapState".
TGUI, part 1 (Patella-Modern issue #107, branch issue-107, November 6, 2026; M7): TClient makes TGUI (GUI.h) after
TCommunication. TGUI owns TTalk (Talk.cpp's 15 globals and UpdateTalkboxes' static, its 66 functions; it is
TPreferences' TTalkLists, so Client.cpp's adapter went), THelp (Help.cpp's 9 globals, 18 functions) and TInteraction
(Status.cpp's 40 non-session globals, 43 functions), and holds GUI.cpp's timers and hotkeys (their 8 functions and
UpdateTimers' static), ContentWindows.cpp's two texts, Control.cpp's six statics, MainWindow.cpp's callbacks and
ModalWindow and MainWindow (raw pointers as before, no longer globals; #108 makes them owners). The free functions
forward through the bridge. MainWindow.cpp and Dialogs.cpp call Control.cpp through TGUIController (Client.cpp's
adapter), so the GUI cycle is {ContentWindows, ContextMenus, DialogWindow, MainWindow} (was 6 modules). Still
globals: SkinProperties (2,096 uses); Control.cpp's and MainWindow.cpp's functions are not members yet. Golden
17,316,040, 0 failures. notes/decisions.md, "The fourth move: TGUI".

The platform interfaces, part 1 (Patella-Modern issue #109, branch issue-109, November 6, 2026; M7): IClock,
IFileSystem and ISocket in src/platform (portable headers; TPlatform and Platform() in Platform.h). Win32's
(platform/Win32Platform.cpp: timeGetTime, time and Sleep; the portable TSystemFileSystem; CAsyncSocket and WinSock, with
Network.cpp's TSocket moved there) is Patella.exe's and the golden runner's platform; the test build's is the test
mode's clock with Win32's files and sockets. TClient takes the platform: TPreferences the file system, TNetwork the
socket (TNetwork is the connection's ISocketEvents). The clocks' callers, Script.cpp's and ConsoleWindow.cpp's opens go
through Platform(); the hooks PATELLA_TIME_GET_TIME, PATELLA_TIME and PATELLA_FRAME_SLEEP are gone. TFolderFileSystem,
the file system's test implementation, is tested in tests_core. Not yet: IClipboard, IInput, IWindow, TSurface.
Golden 17,316,040, 0 failures. notes/decisions.md, "#109: the platform interfaces".

Reflection evaluated (Patella-Modern issue #114, branch issue-114, November 6, 2026; M7): adopt later. GCC 16.2's
`-freflection` wrote the state dump's creature and container entries with the same bytes as the product and as a
member-pointer table that MSVC and Clang could build (tests/reflection/state_dump_reflection.cpp, outside the build;
its static_assert keeps the table in sync with the members). With that fallback it saves no code, and Clang and MSVC
have no reflection yet. Revisit when MSVC and upstream Clang ship P2996. notes/decisions.md, "#114: reflection for the
state dump and the serialisers".

The effect table's links as typed handles (Patella-Modern issue #103, branch issue-103, November 6, 2026; M7): a map
point's first effect and the effects' Previous and Next are `enum class TEffectId : int` (src/MapPoint.h; its value the
original's number, None its -1), the table is TSlotTable<TEffect, EFFECTS, TEffectId>, TMap's effect members take and
give TEffectId; TSlotTable::Chain walks a point's effects (GetEffectIteratorNumber, ClearField's deletes) and
TMapEffects wraps GetFirstEffect/GetNextEffect for DrawField's loop. Field (a map point's number) and the original's
free functions (golden calls and fakes them) keep ints. Golden 17,316,040, 0 failures. notes/decisions.md, "#103".

noexcept destructors (Patella-Modern issue #108, branch issue-108, November 6, 2026; M7): the window destructors'
work that could fail or acts outside the window (the Send functions, the preferences, the hotkeys, TTab's assertion,
DeleteTextBuffer, SetHighlightID) is the components' close: `TComponent::Close` and the classes' `OnClose`, in the
destructors' order (a class's own part, then its base's; a window's children first to last), with the original's
unwinding (the bases' closes run while an exception passes). Every delete of a component closes it first
(DeleteComponent, TWindow::DeleteChild); TComponent's destructor and every window destructor are noexcept and only
free, in the original's order. The golden ConsoleWindow test's nameless report now throws from TTab::OnClose, before
the delete. Converted: TTabBar::LeftTabs and RightTabs, TGUI::MainWindow (FatalError abandons it before exit(1), so
TClient's static destruction leaves it); kept raw with the reason: ModalWindow (the help bubble's freed pointer that
the golden Control test compares), the conditional members, the text buffers. Golden 17,316,040, 0 failures.
notes/decisions.md, "#108".

The matrices and the automap layers as mdspan views (Patella-Modern issue #101, branch issue-101, November 7, 2026; M7):
`src/compat/mdspan.h` gives patella::TMdspan (std::mdspan in MSVC and GCC 16's libstdc++, a stand-in for
linux-x64-clang's libstdc++ 14 and with PATELLA_MDSPAN_STANDIN); Container.h's matrix and matrix3d index their
unique_ptr storage through a layout_left `view()` (members unchanged: golden compares them); TAutomapBlock's Color and
Speed are `TAutomapLayer` (src/AutomapLayer.h), 256 x 256 owned bytes in the file's order with a layout_right view, and
Map.cpp's file format check also checks the view's mapping. array<T> stays (one-dimensional). Golden 17,316,040, 0
failures. notes/decisions.md, "#101".

The server replay's socket inside the process (Patella-Modern issue #120, branch issue-120, November 7, 2026; M7):
with --test-replay PatellaTest.exe's ISocket is TPipeSocketWin32 (src/testmode/PipeSocketWin32.h) over TPipeNetwork
(src/testmode/PipeNetwork.h, portable, tested in tests_testmode): the replay's listeners and connections inside the
process, the events WinSock's and posted through a message-only window as MFC's. No port is opened, so two reference
compares ran at the same time and both matched; the packet and driver logs are the loopback run's. The live runs
(without --test-replay) keep Win32's sockets; Patella.exe is not touched. Golden 17,316,040, 0 failures.
notes/decisions.md, "#120".
TGUI, part 2 (Patella-Modern issue #118, branch issue-118, November 7, 2026; M7): Control.cpp's 26 functions and
MainWindow.cpp's InitMainWindow and ExitMainWindow are TGUI's members (the free functions forward); TGUI takes TNetwork
and TCommunication by reference, so Control.cpp no longer uses the bridge; TGUIController and its adapter are gone. The
GUI module cycle is gone (main: {ContentWindows, ContextMenus, DialogWindow, MainWindow}; 213 edges, 205 now; left:
{Surface, graphics}, {MainFrm, Player}). TMap and TCreatures/TPlayerState call up into Talk, Help and Status through
TMapConsole and TCreaturesGUI (Client.cpp's adapters); TMapState's GetPlayerID calls TPlayerState's member.
TNetworkSession's adapter stays (justified). Golden 17,316,040, 0 failures. notes/decisions.md, "#118".
vlong as a tested component (Patella-Modern issue #128, branch issue-128, November 7, 2026; M8): vlongtest's 3,201
cases, until now one combined digest in tests_core, are checked case by case (a test case per operation, the failing
case's number, operands and result in the message) against tests/unit/core_vlong_cases.inc, one FNV-1a digest per
case from Python's exact integers (core_expected.py --vlong-cases); a test of the unpadded RSA's properties. vlong
needed no change on any preset. The protocol's raw RSA without padding is documented (notes/decisions.md, "#128")
and commented at Communication.cpp's two Encrypt calls. Golden 17,316,040, 0 failures.

The original's reads past their data, bounded (Patella-Modern issues #72 and #86, branch issue-72, November 7, 2026;
M7; the owner's decision of November 7, deliberate differences): PrepareNextPacket drops a frame whose inner length
runs past it (or of size 0) and throws "packet length exceeds the received data", as a read past a packet's end
throws; TIgnoreEntry::NameText and TBuddy::DescriptionText read at most 30 and 128 characters (the ignore list's
rebuilt lines, GetBuddyDescription, Patella.ini, WriteTibiaCfg). PATELLA_IN_DEFINITION_ORDER and golden_linux's
-fno-toplevel-reorder gone. The golden runner's deliberate parts (golden::BeginDeliberate): #72's 280 parts, 37,052
of the Network test's checks, 14,165 differing, counted as #72's; #86 affects no golden check. Golden 17,316,040, 0
failures. bounded_reads (ASan poisons the bytes after the data; the original's reads give 5 ASan reports).
notes/decisions.md, "#72 and #86".

linux-x64-clang on GCC 16's libstdc++ (Patella-Modern issue #121, branch issue-121, November 7, 2026; M7): the image's
Clang 22 uses GCC 16.2's installation through its configuration files (clang++.cfg, clang.cfg: --gcc-install-dir),
so CMake's std module probe, the builds and clang-tidy see libstdc++ 16; PATELLA_IMPORT_STD is ON in linux-x64-clang
and tests_import_std passes there under ASan and UBSan (the target and its module without _FORTIFY_SOURCE on Clang:
glibc's Clang wrappers of the printf family cannot be exported). std::mdspan is std's in every preset; the stand-in is
a test switch only. cxx26.h keeps the inplace_vector stand-in on Clang (libstdc++ 16's fails under Clang 22). The image's
tag is now patella-modern/linux-x64:gcc16.2-clang22.1-libstdcxx16. Golden 17,316,040, 0 failures; tidy baseline 3,
unchanged. notes/decisions.md, "#121".

TGraphics (Patella-Modern issue #116, branch issue-116, November 7, 2026; M7): graphics.cpp's 21 globals and 57
functions are TGraphics' members (graphics.h), Surface.cpp's 18 globals and 4 functions TGraphicsEngine's (Surface.h),
Skins.cpp's table and GUI.cpp's SkinProperties TSkins' (Skins.h, with SkinProperties' types from GUI.h); TClient makes
TGraphics between TCommunication and TGUI, the free functions forward. The windows read the skin data through the
accessors SkinProperties() and Skins(). TRect, TRGBColor and TFont are types/GraphicsTypes.h's, so the {Surface,
graphics} module cycle is gone (205 edges and 2 cycles on main, 207 and 1 now: {MainFrm, Player}). Golden 17,316,040,
0 failures. notes/decisions.md, "#116".

The regexp as C17 behind TRegexp (Patella-Modern issue #129, branch issue-129, November 7, 2026; M8): regexp.c's K&R
definitions are prototypes (same types; the implicit conversions now explicit casts), C17 on every compiler (MSVC
`/std:c17`), off the three warning baselines and without Clang's -Wno-deprecated-non-prototype or /analyze's
suppressions: 0 warnings at /W4 and -Wall -Wextra -Wpedantic -Wconversion -Wshadow. `patella::TRegexp`
(src/types/Regexp.h) owns regcomp's program (free on reset, assignment, destruction); Status.cpp's LinkRegexp is one.
tests_core's regexp tests go through it. Golden 17,316,040, 0 failures. notes/decisions.md, "#129".

Hardened standard library modes (Patella-Modern issue #133, branch issue-133, November 7, 2026; M9): the MSVC STL's
_MSVC_STL_HARDENING=1 and libstdc++'s _GLIBCXX_ASSERTIONS (until now Debug only) for C++ in every configuration of
every product preset (cmake/Hardening.cmake); no check fired. Cost, one run each: golden +16% on Windows, +23% on
Linux, the reference compare +2%. Golden 17,316,040, 0 failures. notes/decisions.md, "#133".

The platform interfaces, part 2 (Patella-Modern issue #119, branch issue-119, November 7, 2026; M7; part 1 of the
issue): IClipboard, IInput and IWindow in src/platform (portable, no Win32 type in a signature, for M8's SDL3), with
Win32 implementations (the calls sysinc.cpp, MainFrm.cpp, Player.cpp and the Vulkan engine made) and the test mode's
(TTestInput, TTestWindow; TMemoryClipboard with a script). TestHooks.h lost seven hooks (the cursor, the main timer,
the idle step, the capture change, the message boxes, the windows' count, the display modes). Patella.ini and LogFile
open through IFileSystem. CMainFrame takes the registered window class, so the {MainFrm, Player} cycle, the client's
last, is gone (207 edges and 1 cycle on main, 206 and 0 now). Patella.exe changes only at those call sites (the image
comparison). Not yet: TSurface as the platform's interface (part 2). Golden 17,316,040, 0 failures.
notes/decisions.md, "#119".

compare_traffic's look step (Patella-Modern issue #137, branch issue-137, November 7, 2026; M7): the live part a.
failed now and then because Cipfried, the temple NPC, wandered onto the field north of the player that "look north"
looked at. compare_traffic's sessions now look at the player's own field ("look at own field", map field (8,6)),
where no creature can stand; run_session keeps "look north" as its default for the recorded sessions
(reference_record.py, compare_rendering.py). testserver.py gives a failed attempt's server with its world loaded 600 s
to save instead of killing it after 30 s (a kill in the middle of the save had cut a map sector off; the sector was
rebuilt). notes/decisions.md, "#137".

The vectors without min..max (Patella-Modern issue #100, branch issue-100, November 7, 2026; M7): every
TIndexedVector use classified; Help's event helps, Preferences' ContentWindows, OpenChannels, Buddies and KnownHints,
GUI's timers, Status' messages and GetAutomapMarks' out parameter are std::vectors (their counts their sizes), GUI's hotkeys a
std::flat_map; kept with the reason: OpenDialogs (its duplicate check reads min..max - 1), the automap blocks' marks
(a negative count from the file), ConsoleWindow's lines, DialogElements' and Dialogs' vectors and Talk's lists (golden
compares their headers or their min..max). Golden 17,316,040, 0 failures at
each commit. notes/decisions.md, "#100".
final on the classes nothing derives from (Patella-Modern issue #47, branch issue-47, November 7, 2026; M7):
70 of the 100 classes of src/ that override and have no subclass in src/ or tests/ are `final`; 30 stay without it
because the golden window tests use them as seams (constructors redirected to the tests' fakes, stand-ins passed
as the class, TContainerWindowBody::UpdateUpButton patched and inlined once `final`); TXTEA's destructor is
`= default` (#34's case). Patella.exe's .text +0x4e0. Golden 17,316,040, 0 failures. notes/decisions.md, "#47".

CMake 4.4.5 everywhere (Patella-Modern issue #141, branch issue-141, November 7, 2026; M7): the owner's decision, one
CMake version. tools/build.py runs cmake, ctest and cpack on Windows from the portable C:\Servers\Software\cmake
(Kitware's zip, SHA-256 checked; `--install-cmake <folder>` installs it, PATELLA_CMAKE_DIR names another folder) and
refuses a missing one or another version; the Linux image has 4.4.5 (new tag
patella-modern/linux-x64:gcc16.2-clang22.1-libstdcxx16-cmake4.4.5), hosted CI's Windows jobs install the same zip.
Clang's std module under 4.4: the target loses the hardening's _FORTIFY_SOURCE item (4.4 de-duplicates the second
-U of #121 away) and tests/unit gets -Wno-reserved-module-identifier (4.1 gave it its module target). Build folders
from 4.1.2 need one --fresh. Golden 17,316,040, 0 failures. notes/decisions.md, "#141".
Part 2 (branch issue-141b): the CPU driver is Mesa 26.2.6's lavapipe (reference compare and compare_rendering within the
tolerance), and the image has the ninja-build project's Ninja 1.13.2 (SHA-256 checked) instead of Debian's 1.12.1, tag
patella-modern/linux-x64:gcc16.2-clang22.1-libstdcxx16-cmake4.4.5-ninja1.13.2.

compare_rendering's look step (Patella-Modern issue #140, branch issue-140, November 7, 2026; M7): compare_rendering.py
replays compare_traffic.py's live recording but still scripted "look north", so a recording made since #137 replayed
one client packet off. Its session now looks at the player's own field as compare_traffic's does, and both tools read
a recording's look step first (the look packet's field against the player's start field) and refuse a recording
with another one, naming it. The frozen reference recordings keep "look north" (reference_record.py passes it).
notes/decisions.md, "#140".

TSurface as a platform interface (Patella-Modern issue #138, branch issue-138, November 7, 2026; M7; #119's part 2):
TSurface is in src/platform/Surface.h (portable, no Win32 type: the window a TWindowHandle, the rectangles a
TEdgeRect). The data the engines share with the client is platform::TSurfaceShared, TGraphicsEngine's base (the same
80 bytes), lent to the surface at its construction, so the Vulkan engine and TSurface's TestEngine and
GetAvailableEngines no longer use bridge::GraphicsEngine(). SurfaceInitInstance, TGUI's WindowHandle and InitWindow,
TNetwork's NetworkWindow and SetNetworkWindow take a TWindowHandle; CreateSurfaces' desktop depth is
IWindow::DesktopMode. Patella.exe changes only at those call sites and in displacements (the image comparison).
Golden 17,316,040, 0 failures; compare_rendering all 26 scenes identical. notes/decisions.md, "#138".

The preferences and the GUI's logic into patella_core (Patella-Modern issue #111, branch issue-111, November 7, 2026;
M7's exit): Preferences, Status, Talk, Help, Script, sysinc, graphics, Client (the bridge), GUI, the window and dialog
files and Control.cpp are patella_core's (34 sources, 38,744 lines; was 16, 12,278; 70 core headers, was 56). The library
is the whole client but the platform layer (SdlFrame.cpp, MainFrm.cpp, Player.cpp, Surface.cpp, src/platform's Win32, src/vulkan, the
test mode's Windows files); it needs of them only platform::Platform(), GetSurface, SurfaceExitInstance and the graphics
device choice (vulkan/DeviceSettings.h), which tests_core_link proves by linking TClient against stand-ins of those.
INT_PTR is std::intptr_t there, Dialogs.h lost PATELLA_VULKAN's guard; 879 clang-tidy findings on the newly seen code
fixed or suppressed with reasons (the baseline stays at 3). No tests/golden/linux file can go yet (the golden runner's
shim). Golden 17,316,040, 0 failures. notes/decisions.md, "#111".

Packet parsing and the game state into patella_core (Patella-Modern issue #110, branch issue-110, November 7, 2026;
M7, half of its exit): Network.cpp, Communication.cpp, Objects.cpp, Map.cpp, Creatures.cpp and Inventory.cpp are
patella_core's (16 sources, 12,278 lines; was 10, 3,671), with their headers and those they include on the core
header list (56; was 40); linux-x64-gcc and linux-x64-clang compile them without tests/golden/linux. INT_PTR is
std::intptr_t there, WinSock's numbers are platform/Socket.h's, the login signatures come through TLoginSignatures,
TCallback is types/Callback.h's; the image's clang-tidy findings on the newly seen code (296) are fixed or NOLINTed
with reasons (the baseline stays at 3). The golden runner keeps its own copies (its fakes), so no stand-in file goes
yet. Golden 17,316,040, 0 failures. notes/decisions.md, "#110".

The SDL3 window and event loop (Patella-Modern issue #122, branch issue-122, November 7, 2026; M8): SDL 3.4.18 is
pinned (cmake/Sdl3.cmake: release archive by URL and SHA256, built static with the client's CRT, video and events only).
The Windows x64 client runs on src/SdlFrame.cpp by default: SDL_CreateWindow with the client's window class and MFC's
client edge, placement, size, fullscreen, activation, close and display events through an event watch, the thread's
messages pumped as MFC did, the 100 ms timer in the loop (platform/LoopTimer.h, TSdlLoopInput). PATELLA_WINDOW=mfc
keeps the MFC frame (platform/WindowChoice.h, a run-time switch) until #127. Keys, buttons and wheel stay Win32 messages
through a subclass until #123. The reference compare matches on both paths. notes/decisions.md, "#122".

Input over SDL3 (Patella-Modern issue #123, branch issue-123, November 7, 2026; M8): on the SDL3 window the IME's text (SDL_EVENT_TEXT_INPUT, converted to Windows-1252), the wheel, the capture (SDL_CaptureMouse, SetCapture where
SDL takes none),
the window coordinates (SDL_GetWindowPosition; the cursor's screen position stays GetCursorPos until #127) and the cursors (SDL's system cursors, the client's
two from its resources through SDL_CreateCursor) are SDL3's. The client's rules (TranslateKey, the modifier flags,
OnChar, the control characters TranslateMessage made, Windows' double clicks over SDL's click counts, the wheel's
notches) are portable in platform/KeyInput.h, the cursor decoder in platform/CursorImage.h, both unit-tested. The
subclass keeps WM_CAPTURECHANGED (SDL has no event for a lost capture), the key messages, WM_CHAR and the button
messages (SDL's key, text and button events differ from them for posted messages and keys Windows does not hold), the test mode's WM_TIMER,
the socket's message and the fullscreen hit test. The test mode's scripted wheel becomes the same SDL event (testmode/SdlInputWin32.cpp); its other messages are dispatched.
The places where SDL cannot give the same input are listed in notes/decisions.md, "#123".

The Vulkan surface through SDL3 (Patella-Modern issue #126, branch issue-126, November 7, 2026; M8): the engine's
window system (src/vulkan/WindowSystem.h: the client area, the screen's size, the surfaces) is SDL3's on the default
path (SDL_Vulkan_CreateSurface on each device's own window, wrapped by SDL for the call; SDL_GetWindowSizeInPixels, the
primary display's bounds) and Win32's on the MFC window until #127; SurfaceVulkan.cpp calls no GetClientRect,
ClientToScreen, GetSystemMetrics or vkCreateWin32SurfaceKHR any more, its rectangles are TEdgeRects. tests_vulkan runs
its device windows' tests on SDL3's surfaces. notes/decisions.md, "#126".

Faster scripted sessions (Patella-Modern issue #144, branch issue-144, November 7, 2026; M7): tools/session_client.py
posts a gesture's messages back to back (a key, a click, a whole typed string; the scripted test client gets them as
one control-channel command of post lines) and waits on what each step is meant to do, as both clients show it (the
start screen, the login dialog, the character list, the player in the game, each walk's position, the turn, the said
line, the windows opening and closing), with a timeout and a named error, instead of fixed sleeps; compare_traffic's
live wait is 0.5 s of server quiet after the step instead of a fixed 1.5 s. The same messages in the same order: three
compare_traffic runs in a row without a difference. compare_traffic 351 s to 145-189 s (a session 44-62 s to 14-20 s),
compare_rendering 473 s to 392 s, compare_files 251 s to 157 s. notes/traffic.md, "The waits"; notes/decisions.md, "#144".

A headless test client (Patella-Modern issue #145, branch issue-145, November 8, 2026; M7): PatellaTest.exe
`--test-headless` (with a script; IWindow::Headless, platform/Window.h) never shows its window (SDL3's or MFC's) and
gives the Vulkan device no surface (the frame dump reads the canvas back as before; fullscreen leaves the display
alone). reference.py compare, testmode_check.py, compare_rendering.py's windowed x64 runs and the client's CTest tests
use it by default (`--windowed`); CTest headless_client. The reference compare matches headless on both window paths
and on the GPU, equal to windowed byte for byte but for the state dumps' addresses; two headless compares beside a
windowed one all matched, no headless window ever visible. notes/testmode.md, "Headless"; notes/decisions.md, "#145".

Tibia.exe's live runs in Windows Sandbox (Patella-Modern issue #147, branch issue-147, November 8, 2026; M7):
tools/sandbox.py runs a tool invocation inside Windows Sandbox (the configuration generated from
tools/sandbox/patella.wsb.template, every folder mapped at its own path, the repository read-only; WindowsSandbox.exe
on a hidden desktop, so nothing shows on the user's desktop; the job at medium integrity, else the Vulkan loader
ignores VK_DRIVER_FILES; one Sandbox at a time, a second run waits). The test server stays on the host: a reverse
tunnel gives the Sandbox the host's 127.0.0.1:7171/7172, and testserver.py start|stop inside runs on the host.
compare_traffic.py, compare_files.py, compare_rendering.py (its display at 1024x768: the fullscreen pass too) and
compare_network.py run inside by default (`--desktop`: as before). compare_traffic and compare_files without a
difference (one compare_traffic run of three failed at the helper's game login; rerun passed), compare_rendering
every scene identical on the GPU and the CPU driver, compare_network as on the host; ready 8-12 s after the start.
notes/testserver.md, "Windows Sandbox"; notes/decisions.md, "#147".

A refused Tibia.exe start (Patella-Modern issue #148, branch issue-148, November 8, 2026; M7): the tools that start
Tibia.exe (session_client.py's Driver for compare_traffic, compare_files, compare_network, compare_rendering and
reference_record; login_client.py) wait up to 120 s for another client's window (tools/client_start.py, naming its
process), close a failed start (its box's text logged, its own PID only), start once more, and end the run on a
second failure instead of piling "A Tibia client is already running!" boxes. Shown in Windows Sandbox with a
Tibia.exe kept open beside each of the four comparisons: refused and named, or the box logged and the client closed,
nothing left. notes/decisions.md, "#148".

A private test server per run (Patella-Modern issue #146, branch issue-146, November 8, 2026; M7): every live run
(compare_traffic, compare_files' live walk, testmode_check's online runs: testserver.isolate(); any command with
`python -I tools/testserver.py run -- ...`) gets a copy of the master C:\Tibia\gameserver in
%TEMP%\patella-testservers\<run> (map, usr, dat, tibia.db, config.cfg copied; mon, npc, origmap hard links; the master's
tibia-server.exe run in it) on 4 ports of its own from 7300-7499, passed to the proxy, the clients and the Sandbox's
tunnel; removed afterwards, a killed run's copy by the next run. The tools never write the master (start without a run
refuses). Shown: compare_traffic or compare_files in the Sandbox and testmode_check on the host at the same time, both
servers with game connections at once, both passing; the master's checksums identical after them and after a killed
run's cleanup. notes/testserver.md, "A private server per run"; notes/decisions.md, "#146".

MFC removed (Patella-Modern issue #127, branch issue-127, November 8, 2026; M8): no source includes MFC; the SDL3
window is the client's only one (PATELLA_WINDOW and platform/WindowChoice.h gone, the Win32 window system gone with it),
CPlayerApp derives from nothing, WinMain is platform/EntryWin32.cpp. The original's CMainFrame and the MFC declarations
the frozen golden corpus's calls are named after moved to tests/golden/mfc (test code; the MainFrm cases replay against
it, not against src/SdlFrame.cpp). Patella.rc has only the client's own resources. Hosted CI builds and tests the full
client and test build with neutral placeholder pictures (PATELLA_PLACEHOLDER_RESOURCES, tools/placeholder_resources.py;
refused where a Tibia.exe exists); the compile-only targets of #25/#46 are gone. notes/decisions.md, "#127".

The Linux client (Patella-Modern issue #131, branch issue-131, November 8, 2026; M8): linux-x64-gcc and linux-x64-clang
build Patella and PatellaTest (cmake/LinuxX64.cmake, the Windows client's sources with the platform layer's *Linux.cpp
files), and PatellaTest replays all four reference sessions on Linux headless on lavapipe, frames included (GCC; Clang
under ASan and UBSan). tools/linux_live.py runs the live login and the scripted walk on a private test server from
the container, headless and under Xvfb, and starts the product under Xvfb. Hardware Vulkan and a real desktop were not
run (no GPU, no display in the container). notes/decisions.md, "#131".

Data files and paths (Patella-Modern issue #130, branch issue-130, November 8, 2026; M8): the client's files go through
platform::TDataFileSystem (platform/FileSystem.h), the data folder chosen at start-up by settings::ChooseLocations
(--data, the working folder with Tibia.dat, the executable's folder, Patella.ini's DataFolder); Patella.ini next to the
data or in the XDG folder (Linux option); FindNoCase finds the original names without regard to ASCII case on Linux.
The working folder stays the data folder wherever it has Tibia.dat, so the tools and the golden runner open the same
files by the same names. Unit tests in tests_core on every preset. notes/decisions.md, "#130".

The portable socket (Patella-Modern issue #125, branch issue-125, November 7, 2026; M8): TPortableSocket
(src/platform/PortableSocket.h, patella_platform, tests_platform on Windows and Linux) is Patella.exe's and the test
build's live network over Winsock or POSIX sockets: non-blocking IPv6 sockets with IPv4 mapped, polled at the start of
every idle step (Player.cpp's OnIdle, both window paths) with WSAAsyncSelect's event rules, and getaddrinfo on a
std::jthread whose result the loop's Poll delivers (IPv4 first, IPv6). CAsyncSocket, AfxSocketInit,
WSAAsyncGetHostByName and afxsock.h are gone from the client; the golden runner keeps those calls
(tests/golden/network/MfcSocket.cpp, its fakes' level). A message-only window's timer polls the modal loops while a
connection or a lookup is open (platform/NetworkWin32.cpp). The SDL3 frame's WM_USER + 1 went (SdlFrame.cpp's HostResolved).
notes/decisions.md, "#125".

Display modes, clipboard, URLs and message boxes over SDL3 (Patella-Modern issue #124, branch issue-124, November 8,
2026; M8): the SDL3 window's Platform() has SDL3's clipboard (UTF-8 to and from Windows-1252), message boxes
(SDL_ShowSimpleMessageBox, read from outside by the tools as MessageBoxA's) and URLs (SDL_OpenURL; src/platform/
SdlPlatform.cpp); the Vulkan engine's mode change is IWindow::SetFullscreenMode. On Windows the display modes stay
EnumDisplaySettingsA/ChangeDisplaySettingsA behind IWindow: a probe found SDL3 listing the desktop's custom 5120x1440
mode (25 modes, Windows 24) and other refresh rates; SDL3's versions are the other systems' (platform/DisplayModeList.h,
unit-tested). The MFC path keeps Win32's. notes/decisions.md, "#124".

compare_files' options scenario (Patella-Modern issue #149, branch issue-149, November 8, 2026; M7): Tibia.exe's
window tree is read from outside while the client builds or frees an options dialog, and since #144's waits the first
poll comes during that change: 4 of 25 runs of Tibia.exe's side failed with a ReadError through a stale pointer.
compare_files.gui reads a failed reading again within the wait's timeout (2 s for a single reading) and then raises
it; what a successful reading shows is used as before. compare_files --self-test covers it without a client. In Windows Sandbox
(#147) five options runs in a row and a full compare_files run found no differences; the first options run met one
such torn reading and read it again. notes/decisions.md, "#149".

compare_network's bad frames (Patella-Modern issue #150, branch issue-150, November 8, 2026; M7): tools/deliberate.py's
FRAME_SCENARIOS lists the six bad-frame scenarios of #72, #139 and #142 (login-bad-size, game-bad-size,
login-inner-long, game-inner-long, game-empty-alone, game-two-empty-together) with Tibia.exe's outcome (the critical
error box, or stalled at "Connecting") and the fork's client's (the login server's close, or the game); compare_network
reports exactly those as DELIBERATE with their issue, still compares the rest, and lists every deliberate difference
it expected. In Windows Sandbox: 41 the same, 4 variable, 6 deliberate, 0 differ. notes/decisions.md, "#150".

login-walk.script's walk (Patella-Modern issue #151, branch issue-151, November 8, 2026; M7): Cipfried, the temple NPC,
could stand on the field of the script's first step north, and the client then sends no step (1 of 22 runs of #146).
The square now goes south first and stays on rows beyond his radius (#137's way: the scenario, not the check);
testmode_check is unchanged. 20 testmode_check runs in a row passed, 4 walks in every run; none of them met a block.
notes/decisions.md, "#151".

CI's downloads (Patella-Modern issue #153, branch issue-153, November 8, 2026; M7): GitHub's release downloads answered
504 for hours and failed whole runs. A timeout or 5xx is now retried with a pause everywhere the builds download, and
CMake falls back to Kitware's own site; a 404 or a wrong SHA-256 stops at once. tools/build.py's download_verified,
docker/linux-x64/fetch.sh and cmake/Download.cmake's patella_fetch_declare; tools/download_selftest.py shows them
against a local server, also in hosted CI's clang-format job. notes/decisions.md, "#153".

Two Sandboxes at once (Patella-Modern issue #154, branch issue-154, November 8, 2026; M7): measured, and Windows
refuses. A second WindowsSandbox.exe leaves only an error window ("only one instance"), a second `wsb start` fails with
CO_E_APPSINGLEUSE, and the first Sandbox is not disturbed. tools/sandbox.py's lock stays as it is; #147's unmeasured
note corrected. notes/decisions.md, "#154"; notes/testserver.md, "Windows Sandbox".

Fuzzing (Patella-Modern issue #132, branch issue-132, November 7, 2026; M9): tests/fuzz has eight libFuzzer targets,
fuzz_settings (Patella.ini, patella_core only), fuzz_network (TNetwork's frames, #72's short packets among the seeds),
fuzz_packets (ReceivePackets and Communication.cpp's handlers in the golden Communication world), fuzz_tibia_cfg,
fuzz_map, fuzz_dat, fuzz_sprites and fuzz_pictures; every preset runs each as a CTest smoke test (label fuzz) on the
hand-made seeds of tests/fuzz/seeds; tools/fuzz.py builds and runs the libFuzzer trees (Clang 22 in the image, MSVC
/fsanitize=fuzzer) with local seeds from the reference recordings. Fixed, the fork's Patella.ini code: Parse dropped a
CR at the end of a CRLF file's last line without a line end; WritePreferences kept a numbered key spelled otherwise
(Buddy01), which then shadowed the renumbered entry; Set took a key starting with a byte order mark. The original's
findings are filed (notes/decisions.md, "#132").

The original's failures on corrupt files and hostile frames, fixed (Patella-Modern issue #139 and #72's outcome,
branch issue-139, November 7, 2026; M9; the owner's decisions of November 7, deliberate differences): a server frame
the client cannot use (#72's inner length past the frame or size 0, a size that is no multiple of 8, a frame larger
than the 16 KB receive buffer) is dropped, a warn line goes to the developer log, and the connection goes on with the
next frame (#72's frame no longer ends the client; the original read past it, threw at every later call, or waited
forever). A failed Tibia.dat or Tibia.pic load frees only what it allocated; a sprite's or tile's run past the 1024th
pixel is a data error (PROGRAM_ERROR 4 or 5); GetString checks before it allocates; a .map file with a negative mark
count is a corrupted file. Golden 17,316,040, 0 failures; the Network test's deliberate parts: #72 274 parts, 36,104
checks, 16,588 differing; #139 33 parts (item 6's "frames" in the corpus's out-of-step streams), 3,343 checks, 1,178
differing (before: #72 280, 37,052, 14,165). bounded_reads covers each item; the fuzz harnesses' guards are gone,
properties check the fixes, new synthetic seeds. notes/decisions.md, "#139".

An empty packet no longer stalls the reading (Patella-Modern issue #142, branch issue-142, November 7, 2026; M9; the
owner's decision of November 7, a deliberate difference): a server packet of inner length 0 is read past to the next
frame in the same call and its frame leaves the receive buffer at once (not a drop, no log line); the original
stopped there with the frames behind it waiting, and a frame that fit only without it never completed. Golden
17,316,040, 0 failures; the Network test's deliberate parts: #72 269 parts, 35,460 checks, 16,386 differing; #139 32,
3,303, 1,167; #142 36, 3,688, 1,037 (before: 307 parts, 39,447 checks, 17,766 differing). bounded_reads has three
new cases; fuzz_network's stall property holds in full. notes/decisions.md, "#142".

Display modes of the reference sessions (Patella-Modern issue #48, November 4, 2026; M2): the Graphics Options
dialog listed the machine's display modes, which had gained a 1176x664 mode since the recording (traffic frame 3:
664 pixels). The test build now takes the list from `--test-display-modes <file>` (the hook
PATELLA_ENUM_DISPLAY_SETTINGS, src/testmode/DisplayModes.h); reference.py writes it from meta.json's new optional
`display_modes` (the record step stores it), and the sessions of November 3 get the 24 modes of their recording
(format still `patella-reference 1`, no new recording). tests_testmode "DisplayModes" (a made-up machine with
1176x664), reference.py --self-test. Patella.exe and the oracle unchanged (pecompare). notes/testmode.md, "The
display modes"; notes/decisions.md, "The display modes of a reference session (Patella-Modern issue #48)".

const pointer parameters (Patella-Modern issue #50, November 4, 2026; M3): readability-non-const-parameter through
tools/tidy.py on all 11 targets reported four `int *` parameters that are only read (CreateTextDialog,
TOutfitDialog's and TNewChannelDialog's constructors, TOutfitIcon::SetColors); they are `const int *`, the difftest's
fake of CreateTextDialog too, and the check is enabled. The oracle Patella.exe and the x64 Patella.exe and
PatellaTest.exe unchanged (pecompare); macrocheck 0 differences; the clang-tidy baseline unchanged (14).
notes/decisions.md, "const pointer parameters (Patella-Modern issue #50)".
Golden corpus, settings, scripts and player data (Patella-Modern issue #26, November 4, 2026; M6): nine of the ten
tests ported to tests/golden, one commit each (Preferences and PreferencesFile together): Status 565,809, Preferences
102,060, Script 458,232, PreferencesFile 112,155, Player 18,442, Help 70,631, Inventory 158,400, Talk 1,214,929,
Container 192,832, each with the oracle's count and 0 failures; the runner replays 3,267,008 of difftest's checks.
Errors (40,408) is not ported: it needs our side's state from the not yet ported Objects, Map, Creatures, Graphics
and GUI tests. The runner links the reconstructed files it compares (golden_client, MFC shim) and stubs of the rest;
Container compares through a layout projection, Talk points the runner's _time32 import at the fixed clock. No src/
change. notes/decisions.md, "The golden corpus: settings, scripts and player data (Patella-Modern issue #26)".
Golden corpus, the game world (Patella-Modern issue #27, November 4, 2026; M6): three of the six tests ported to
tests/golden, one commit each: Objects 327,886, Map 639,877, Creatures 2,610,987, each with the oracle's count and 0
failures; the runner replays 6,845,758 of difftest's checks. Tibia.dat is in the corpus (golden::DataFile), the
creature tables are recorded by their changed bytes (golden::OutputRuns), and the runner redirects our Map.cpp and
Help.cpp functions to Creatures' fakes as difftest does (golden_patch.h). Not ported: MainFrm 49,576, MapWindow
127,529, MainWindow 57,314 (error rounds without a runner-side layer, MFC fakes, TMapWindow's layout, the GUI and
graphics fakes of #28/#29). No src/ change. notes/decisions.md, "The golden corpus: the game world (Patella-Modern
issue #27)".
Golden corpus, error rounds (Patella-Modern issue #54, November 4, 2026; M6): the error rounds' comparison (TThrown,
CompareThrown with its line pairs, TErrorRound) moved from tools/difftest/imageeh to tests/golden/golden_eh, the same
code in the oracle and the runner; the oracle records the original's exception by value (ORIGINAL_THROWN), the
runner throws our side's assertion with OurSideAssertion (GUI.cpp's SetSize assertion, checked against the real one
in the oracle). MainFrm ported with it: 49,576 checks, the oracle's count, 0 failures (31,424 + its error round's
18,152: 5,504 calls compared, 535 that threw, equal to the oracle's); the runner replays 6,895,334 of difftest's
checks. No src/ change. notes/decisions.md, "The golden corpus: error rounds (Patella-Modern issue #54)".
Golden corpus, the GUI, graphics and the window files (Patella-Modern issue #28, November 4, 2026; M6): the runner
compiles graphics.cpp, GUI.cpp and every window file (golden_client; stubs of Communication.cpp's Send functions,
Control.cpp and the Vulkan device queries below them). Two of the eight tests ported to tests/golden, one commit
each: Graphics 570,954 (the tiles through a dictionary, Tibia.spr and Tibia.pic in the corpus) and GUI 5,277,938
(31,909 in its error round; the original's tree recorded as node indices, our side's assertion the real SetSize in
the runner too), each with the oracle's count and 0 failures; the runner replays 12,858,032 of difftest's checks.
Not ported: DialogWindow, ContextMenus, ConsoleWindow, ContentWindows, DialogElements, Dialogs (2,277,170 checks;
elementfakes.cpp's mirror classes with the x86 offsets, `__fastcall` stand-ins on our side, Dialogs.cpp's Vulkan
variant). No src/ change. notes/decisions.md, "The golden corpus: the GUI, graphics and the window files
(Patella-Modern issue #28)".
Golden corpus, the window tests (Patella-Modern issue #28, second part, branch issue-28b, November 4, 2026; M6): five
of the six window tests ported to tests/golden, one commit each: DialogWindow 34,068, ContextMenus 41,660,
ConsoleWindow 318,882, ContentWindows 221,270, DialogElements 1,362,116 (1,977,996 checks, 1,701,409 of them in their
error rounds), each with the oracle's count and 0 failures in Release and Debug; the runner replays 14,836,028 of
difftest's checks. The DialogElements fakes are compiled on both sides (tests/golden/elementfakes.h: the x86 layout in
the oracle, ours in the runner, our constructors redirected through their x64 decorated names); member-function
fakes are GOLDEN_THISCALL stand-ins; the dumps go through golden::OutputTextChanges. Not ported: Dialogs (299,174),
whose advanced graphics dialog (9,069 of its checks) is the x64 device choice in the runner: needs the user's
decision. Found: TDialogWindow::SetSize reads a deleted child (as the original; the runner keeps freed memory while
DialogWindow runs). No src/ change. notes/decisions.md, "The golden corpus: the window tests (Patella-Modern issue
#28, second part)".
Golden corpus, Dialogs (Patella-Modern issue #28, third part, branch issue-28c, November 4, 2026; M6): Dialogs
ported to tests/golden: 290,045 of its 299,174 checks replayed with 0 failures in Release and Debug (194,419 in its
error round, whose counts are the oracle's); the 9,129 checks of the parts that open the advanced graphics dialog
(3,263 in its rounds, 5,806 in its error rounds, 60 in three direct calls in the assertions; the x64 dialog has the
device buttons of #31) retire with the oracle, the user's decision. golden::RetiredBegin/RetiredEnd: the oracle records
a part's end (its checks, its values), the runner skips to it and restores what the later rounds read (seed, counters,
the last dump, the clock of our last UpdateTimers call), and the replayed and retired checks together must be the
oracle's. The dump prints members by type (the x86 words could not be rebuilt on x64 and depended on the address
space). The runner replays 15,126,073 of difftest's checks and retires 9,129. No src/ change. notes/decisions.md, "The
golden corpus: Dialogs and retired parts (Patella-Modern issue #28, third part)".
Golden corpus, MapWindow and MainWindow (Patella-Modern issue #27, second part, branch issue-27b, November 4, 2026;
M6): both ported to tests/golden, one commit each: MapWindow 127,529 (12,359 in its error round) and MainWindow
57,314 (43,150 in its error round), the oracle's counts with 0 failures in Release and Debug. TMapWindow's compared
members have no pointers: recorded as their bytes in the x86 order, ours read by member (no layout projection).
MainWindow's fakes have their real classes' sizes in the runner and our constructors are reached through their x64
names; elementfakes.cpp has no "not ported" fake left. The runner reports #29's engine tests (SurfaceDX, DX9, GL,
1,488,299 checks) as retired with the oracle (the user's decision); it replays 15,310,983 of the oracle's 17,983,332
checks, retires 1,497,428 and lists 1,174,921 as not ported (Errors, Communication, Control). No src/ change.
notes/decisions.md, "The golden corpus: MapWindow, MainWindow and the retired engine tests (Patella-Modern issue
#27, second part)".

ctime's text (Patella-Modern issue #61, branch issue-61, November 4, 2026; M4): src/compat/crt71.h's
FormatCRT71Time writes MSVC 7.1's ctime text (every number in two digits, the day too) with TFixedString::Format
(std::format_to_n; the oracle's stand-in) on the C library's local time (_localtime32_s/_localtime64_s, localtime_r),
replacing CRT71Time's patch of ctime's buffer in HandleException and SaveChannel; MsvcCtime32 and PortableCtime32
removed, no ctime/_ctime32 call in src/. Found: the UCRT's 32-bit localtime stops at 2038-01-18 23:59:59 UTC, 7.1's at
INT32_MAX. notes/decisions.md, "ctime's text written by the client: FormatCRT71Time (Patella-Modern issue #61)".
Golden corpus, Errors (Patella-Modern issue #26, second part, branch issue-26b, November 4, 2026; M6): errortest.cpp
moved to tests/golden/errors_tests.cpp: 40,372 of its 40,408 checks replayed, 0 failures in Release and Debug; the 36
of its Statics round's SurfaceInitInstance part (the x86 engine selection) retired with the oracle, the user's
decision. The comparisons skipped for functions earlier tests redirected are the oracle's answer, recorded per
comparison; our statics found through the getters' x64 code; HandleException's reports written in TZ=UTC0 and the
script errors' paths TEMP-relative, so the corpus does not depend on the machine. The runner replays 15,351,355 of
the oracle's 17,983,332 checks, retires 1,497,464 and lists 1,134,513 (Communication, Control) as not ported. No src/
change. notes/decisions.md, "The golden corpus: Errors (Patella-Modern issue #26, second part)".


64-bit time everywhere (Patella-Modern issue #62, branch issue-62, November 4, 2026; M4): every wall clock read is
PATELLA_TIME (time(), 64 bits); PATELLA_TIME32, WallTime32, MsvcTime32, MsvcLocaltime32, PortableTime32 and
PortableLocaltime32 removed, and CRT71LocalTime takes _localtime64_s only. The two frozen 4-byte fields
(TCreature::SkullTime, TTalkLine::Time) are src/types/Time32.h's Time32, written with StoreTime32 (the original's
low 32 bits) and read with LoadTime32 (relative to now); TCreature::SkullKept is the skull check in 64 bits.
Unchanged up to 2038; after it the stamps, the saved channel's text and the kept skull work as before 2038 (the
original crashed on a talk line, wrote "(null)" and lost the skull's blinking). The golden tests of Talk,
Creatures, ConsoleWindow and ContentWindows fake our clock through _time64 now (forced). notes/decisions.md, "64-bit
time everywhere (Patella-Modern issue #62)".

Patella-Modern issue #30, second part (branch issue-30b, November 4, 2026): communicationtest's Communication test
and its fake world moved to tests/golden (communication_tests.cpp, commworld.cpp): 856,366 of its checks replayed, 0
failures in Release and Debug, with the original's side of every comparison recorded by value. One runner: the
group's clashing definitions are the oracle's only, golden_patch.h undoes what difftest's tests left patched before the
group (RestoreAllOwn/RestoreAllImports); Communication.cpp and Control.cpp compile into the runner in tests/golden/comm
with /EHa and without /RTC1 in Debug. The runner replays 16,207,721 of the oracle's 17,983,332 checks, retires
1,497,464 and lists Control's 278,147 as not ported: 21,979 of its checks (1,518 comparisons) reach Control.cpp's
ChangeEngine, whose x64 version is the device choice of issue #31 (the user's decision; notes/decisions.md, "The golden
corpus: Communication, and Control's engine change (Patella-Modern issue #30, second part)"). No src/ change.

Patella-Modern issue #30, third part (branch issue-30c, November 4, 2026): Control moved to tests/golden
(control_tests.cpp, Control.cpp compiled into it in the runner): 253,992 of its 278,147 checks replayed, 24,155 retired
with the oracle (the user's decision: the 1,518 comparisons that reach ChangeEngine, the x86 engine selection, and the
statics' checks right after them), per comparison with the oracle's flag recorded first and the oracle's state set
after each retired part. Every oracle test is now replayed or retired: 16,461,713 replayed + 1,521,619 retired =
17,983,332. The replay found an x64 defect in src/Control.cpp (event 0x12 reads Communication's `int[]` channel list
as INT_PTR[]: the new-channel dialog's IDs shifted by one on x64), fixed in src/Control.cpp (the oracle's Patella.exe
unchanged, pecompare); the queue's other messages audited, no other defect. Control replays with 0 failures.
notes/decisions.md, "The golden corpus: Control (Patella-Modern issue #30, third part)".
Next: the live check of the x64 client's "Open a channel" dialog; the other open issues; see GitHub.

## Resources (issue #17)

src/res/Patella.rc + resource.h rebuild all 55 entries of Tibia.exe's .rsrc byte for byte (types, IDs,
languages). `tools\build-resources.cmd` regenerates the pictures (tools/extract_resources.py Tibia.exe
build/res; not committed), compiles build\res\Patella.res with rc.exe and compares it with Tibia.exe
(tools/compare_resources.py: 0 differences). Linked into Patella.exe by #16 (Patella.vcxproj: /I build\res, no
afxres.rc, /MANIFEST:NO): compare_resources.py Tibia.exe build\Release\Patella.exe reports 0 differences.
Details and the users of every ID: notes/resources.md.

## Delegates (running)

None.

## Open questions

- None yet.

## Last difftest

None since Patella-Modern issue #94 (November 6, 2026): the oracle and its differential tests left the fork. The
golden corpus replays their last recording (issue #93) with the oracle's counts: 15,803,218 replayed + 1,512,822
retired = 17,316,040, 0 failures, on windows-x64 (Release, Debug, ASan), linux-x64-gcc and linux-x64-clang (issue #94's
gates, build\logs\issue-94). The entries below are history.

Patella-Modern issue #93 (branch issue-93, November 6, 2026): the final oracle recording, with its manifest.
golden_reproducible: 43 files compared, 0 different. golden_corpus at ab1391c (clean): 0 failures, 43 files,
908,993,929 bytes, time zone UTC0. golden against it, the manifest verified: windows-x64 17/17, -asan 16/16,
linux-x64-gcc 14/14, linux-x64-clang 13/13, each 15,803,218 replayed + 1,512,822 retired = 17,316,040, 0 failures. A
changed byte (difftest/Talk.bin) and TZ=CET-1 refused (exit 1); --unverified replays the changed copy into a stale
Talk. reference.py compare on build\reference-final: every session matches, the manifest verified; a changed file
refused. `python -I tools/build.py oracle-x86`: 5/5, difftest 16,459,352, sysinctest 67, communicationtest 856,621,
vlongtest 3,201, 0 failures (unchanged), safeseh passed. tidy_ci, banned_calls 0, clang-format clean. Logs:
build\logs\issue-93.

Patella-Modern issue #92 (branch issue-92, November 6, 2026): the x64 client read through its test mode's control
channel only. No difftest change (the oracle is untouched). `python -I tools/build.py windows-x64`: exit 0, 16/16
(golden skipped: no corpus in the worktree). The x64 Release Patella.exe of main 0810767 and of the branch, built at
the same path: identical apart from the link stamps (6 bytes before masking, 0 after). Live, on the x64 client:
reference.py compare: every session matches; testmode_check.py --runs 2: exit 0, a-h pass (the first attempt's server
never bound the login port, error 10049, before any client ran; rerun once); compare_traffic.py and compare_files.py
--patella build/windows-x64/Release/Patella.exe: exit 0, no differences; compare_rendering.py --engines
vulkan,vulkan-cpu --modes windowed --no-repeat: every scene identical (the first attempt failed in Tibia.exe's first
patcap capture, no frame within 30 s; rerun once). The tools' self-tests 0 failures; tidy_ci 3 findings, 0 new;
tools/tidy.py on the Windows-only changed files: no finding on a changed line; banned_calls 0; clang-format clean.
Logs: build\logs\issue-92.

Patella-Modern issue #55 (branch issue-55, November 6, 2026): two recordings of one build byte for byte the same
(Buffers' uninitialised text bytes zeroed and their record gone; Talk's lines identified by contents, not by heap
address), and the 53 unsequenced Random() expressions of tests/golden and tools/difftest's surface tests written in
the oracle's order (the recording unchanged by it); the target golden_reproducible records twice and compares. No
src/ change. `python -I tools/build.py oracle-x86`: CTest 5/5, difftest 16,459,352, sysinctest 67, communicationtest
856,621, vlongtest 3,201, 0 failures (unchanged), safeseh 0. golden on the re-recorded corpus 15,803,218 replayed +
1,512,822 retired = 17,316,040, 0 failures (windows-x64 16/16, -debug 15/15, -asan 15/15; fatal_error_box failed
once in windows-x64 and -debug, "A Tibia client is already running!" from another worktree's client, and passed
alone); windows-x64-gcc and the Linux presets 11/11, 0 warnings. golden_reproducible: 43 files compared, 0
different. Logs: build\logs\issue-55.

Patella-Modern issue #65 (branch issue-65, November 6, 2026): TDialogWindow::SetSize ends its loop at a child its own
SetSize deleted (dialogwindow_setsize: heap-use-after-free under ASan before, 3/3 cases after). `python -I
tools/build.py oracle-x86`: exit 0, CTest 5/5, difftest 16,459,352, sysinctest 67, communicationtest 856,621,
vlongtest 3,201, 0 failures (unchanged), safeseh 0. golden on main's corpus 15,803,218 replayed + 1,512,822 retired =
17,316,040, 0 failures (windows-x64 17/17, -debug 16/16 without the runner's operator delete, -asan 16/16, no ASan
finding; fatal_error_box rerun alone in -debug and -asan: a client of a parallel test was running); windows-x64-gcc and
the Linux presets 11/11, 0 warnings. Images by function: SetSize, line constants only elsewhere. tidy_ci 3 findings, 0
new; banned_calls 0; clang-format clean. reference.py compare: every session matches. Logs: build\logs\issue-65.
Patella-Modern issue #90 (branch issue-90, November 6, 2026): ExitControl skips ExitSurfaces without a surface,
FatalError skips SetDisplayMode without a main window or a surface (PatellaTest.exe without Tibia.dat: 0xC0000005
before, the box and exit code 1 after). `python -I tools/build.py oracle-x86`: exit 0, CTest 5/5, difftest 16,459,352,
sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures (unchanged), safeseh 0. golden on main's
re-recorded corpus 15,803,218 replayed + 1,512,822 retired = 17,316,040, 0 failures (windows-x64 16/16, -debug 15/15,
-asan 15/15, no ASan finding; init_control_failure passes); windows-x64-gcc and the Linux presets 11/11, 0 warnings.
Images by function: ExitControl and FatalError, line constants only elsewhere. tidy_ci 3 findings, 0 new;
banned_calls 0; clang-format clean. reference.py compare: every session matches. Logs: build\logs\issue-90.
Patella-Modern issue #83 (branch issue-83, November 6, 2026): FatalError skips ShowWindow without a main window, so a
failing first present ends with "Tibia Error" and exit code 1 (PatellaTest.exe with PATELLA_TEST_FAIL_PRESENT=1:
0xC0000005 before, the box and exit code 1 after). `python -I tools/build.py oracle-x86`: exit 0, CTest 5/5, difftest
16,459,337, sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures (unchanged), safeseh 0. golden on
main's corpus 15,803,203 replayed + 1,512,822 retired = 17,316,025, 0 failures (windows-x64 14/14, -debug 13/13, -asan
13/13, no ASan finding); windows-x64-gcc and the Linux presets 11/11, 0 warnings. Images by function: FatalError, and
line constants or switch-table bytes only elsewhere. tidy_ci 3 findings, 0 new; banned_calls 0; clang-format clean.
reference.py compare: every session matches. Logs: build\logs\issue-83.
Patella-Modern issue #67, part 2 (branch issue-67b, November 6, 2026): PATELLA_DIVISOR at the input divisions
(FatalError's NULL main window settled in #83; the numbers are from before the merge of main 3678f82). `python -I tools/build.py oracle-x86`: exit 0, CTest 5/5, difftest 16,459,352 (+15:
Divisions), sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures, safeseh 0 findings. golden on main's
corpus 17,316,025, 0 failures (Divisions not in it, noted); windows-x64 15/15, -debug and -asan 14/14 (fatal_error_box
passes), windows-x64-gcc and the Linux presets 11/11, 0 warnings. Images: only the changed functions and line
constants. tidy_ci 3 findings, 0 new; banned_calls 0; clang-format clean. reference.py compare: every session matches.
Logs: build\logs\issue-67b.

Patella-Modern issue #81 (branch issue-81, November 6, 2026): unique_ptr for History, TEditLine::Text and
TPasswordEditLine::Stars; ~TTalkBuffer noexcept. `python -I tools/build.py oracle-x86`: exit 0, CTest 5/5, difftest
16,459,337, sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures (unchanged), safeseh 0 findings.
golden on main's corpus 15,803,203 replayed + 1,512,822 retired = 17,316,025, 0 failures (windows-x64 14/14, -debug
13/13, -asan 13/13, no ASan finding); windows-x64-gcc and the Linux presets 11/11, 0 warnings. Images by function: x64
109 changed (91 assertion line constants only), x86 106 (83); the rest the converted owners' functions. tidy_ci 3
findings, 0 new; banned_calls 0; clang-format clean. reference.py compare: every session matches. Logs:
build\logs\issue-81.

Patella-Modern issue #68 (branch issue-68, November 6, 2026): `tools/testserver.py start` now returns only when both
ports listen and the world is loaded ("Server Online!"). That is the one start of compare_traffic.py,
compare_files.py, testmode_check.py and reference.py's live recordings. An attempt that stalls (no progress in
server.log, the ports or CPU time for 180 s; "Server Online!" without the ports 60 s later; 900 s in all) or exits is
stopped (the start's own server only) and started once more. compare_files.py puts each client's window back at
Tibia.cfg's place before the exit when Windows moved it, and the report lists it, so WindowedMode stays compared in
full (notes/decisions.md, "The tools wait for the world; the window's place before the exit"). Tools and docs only, no
src/ change. Self-tests of testserver.py, compare_files.py and testmode_check.py: 0 failures. Two consecutive
`python -I tools/compare_files.py --patella build/windows-x64/Release/Patella.exe` from a started server: exit 0, no
differences, both. `python -I tools/compare_traffic.py --patella build/windows-x64/Release/Patella.exe`: exit 0, no
differences. `python -I tools/testmode_check.py --runs 2`: exit 0, a-h pass. Logs: build\logs\issue-68.

Patella-Modern issue #88 (branch issue-88, November 6, 2026): tools/client_state.py recognises TWindow::GetFirstChild
by the PDB's address as well as by the Release code pattern, so it walks a Debug client's window tree (/Od compiles
the getter through the stack, which the pattern missed; notes/decisions.md, "client_state.py's window test on the
Debug builds"). Tools only, no src/ change. `python -I tools/client_state.py --self-test` and `python -I
tools/testmode_check.py --self-test`: 0 failures. `python -I tools/testmode_check.py --exe
build/windows-x64/Debug/PatellaTest.exe --runs 2 --timeout 600`: exit 0, a-h pass (e: 9-50 windows read from outside, as
dumped); the same with build/windows-x64/Release/PatellaTest.exe: exit 0, a-h pass (the second attempt: in the first the
test server never listened within 300 s). Logs: build\logs\issue-88.

Patella-Modern issue #89 (branch issue-89, November 6, 2026): the SafeSEH gate. `python -I tools/safeseh_scan.py
--self-test`: passed. `python -I tools/build.py oracle-x86`: exit 0, CTest 5/5, difftest 16,459,337, sysinctest 67,
communicationtest 856,621, vlongtest 3,201, 0 failures (unchanged), safeseh 0 findings. With #79's GetIcon fix
reverted (not committed): exit 8, CTest 3/5, safeseh names TPlayerStates::GetIcon in Patella.exe and finds the frame
in difftest.exe and communicationtest.exe, difftest ends in TestContentWindows; restored: 5/5. No src/ change. Logs:
build\logs\issue-89.

Patella-Modern issue #82 (branch issue-82, November 6, 2026): tools/patch_client.py --rsa patches the Debug clients'
two copies of the RSA modulus (Debug builds do not pool the two identical literals of src/Communication.cpp; each
copy must be referenced by code); notes/decisions.md, "patch_client.py --rsa on the Debug builds". No src/ change.
`python -I tools/patch_client.py --self-test`: passed (made-up PE32 and PE32+ images, one copy, two copies, the refused
cases). main's tool and the branch's with `--host 127.0.0.1 --rsa tibia.pem`, with and without `--port 7271`, on
Tibia.exe, the oracle-x86 Release Patella.exe and the windows-x64 and windows-x64-asan Release Patella.exe and
PatellaTest.exe: identical byte for byte; the four Debug images (windows-x64-debug, windows-x64-asan-debug): main
refused them, the branch patches both copies (620 bytes, 621 with the port; size unchanged). `python -I
tools/testmode_check.py --exe build/windows-x64/Debug/PatellaTest.exe --runs 2 --timeout 600`: both runs log in, walk,
talk and log out, a-d and f-h pass; exit 1 from e alone (client_state.py reads the Debug image's window tree as 1-3
windows: found, not done). Logs: build\logs\issue-82.

Patella-Modern issue #87 (branch issue-87, November 6, 2026): tests_vulkan's thread census now counts after the CPU
driver's threads have ended. Before, a census could catch lavapipe's rasterizer threads still exiting, which gave 21
and 27 for 24 under ASan. Each census in the rounds now waits up to 30 s for the new live threads that started in
vulkan_lvp.dll or the ASan runtime. The checks keep their exact numbers (notes/decisions.md, "The thread census counts
after lavapipe's threads have ended (Patella-Modern issue #87)"). tests/vulkan only, no src/ change. `python -I
tools/build.py windows-x64` 14/14 and `windows-x64-asan` 13/13, 0 warnings (no golden corpus in the worktree: golden
skipped). tests_vulkan `--repeat until-fail:20`: 20/20 on windows-x64, 20/20 on windows-x64-asan, 24 per device in
every run. In these 40 runs no census found a thread still ending, so the wait never ran. Logs: build\logs\issue-87.

Patella-Modern issue #79 (branch issue-79, November 6, 2026): TASSERT* to PATELLA_ASSERT. `python -I tools/build.py
oracle-x86`: difftest 16,459,337, sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures, CTest 4/4
(unchanged). golden on main's corpus 15,803,203 replayed + 1,512,822 retired = 17,316,025, 0 failures (windows-x64
14/14, -debug 13/13, -asan 13/13, no ASan finding); windows-x64-gcc and the Linux presets 11/11, 0 warnings.
tests_unit 72 cases on Windows (MSVC 10,166, stand-in 10,170), 71 on Linux. reference.py compare: every session
matches. Images by function: x64 579 changed (447 assertion sites, the rest inlining that follows), x86 652 (500).
tidy_ci: 4 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-79.

Patella-Modern issue #78, part 7 (branch issue-78g, November 6, 2026): TBEGIN/TEND deleted from Communication.cpp,
Network.cpp and Control.cpp (181 try blocks in 180 functions), then the macros. `python -I tools/build.py oracle-x86`:
difftest 16,459,337, sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures, CTest 4/4 (every test's
own numbers main's). golden on main's corpus 15,803,203 replayed + 1,512,822 retired = 17,316,025, 0 failures
(windows-x64 14/14, -debug 13/13, -asan 13/13, no ASan finding); windows-x64-gcc and the Linux presets 11/11, 0
warnings. reference.py compare: 4 sessions match; compare_traffic.py (live, x64): no differences. Images by function
(x86 and x64): only the part's functions changed (171), 7 on x86 and 6 on x64 now inlined into the part's callers.
tidy_ci: 20 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-78g.

Patella-Modern issue #78, part 5 (branch issue-78e, November 6, 2026): TBEGIN/TEND deleted from DialogElements.cpp
(141 try blocks in 141 functions). `python -I tools/build.py oracle-x86`: difftest 16,459,337, sysinctest 67,
communicationtest 856,621, vlongtest 3,201, 0 failures, CTest 4/4 (every count unchanged; DialogElements errors
401,790 exceptions compared, as main). golden on main's corpus 15,803,203 replayed + 1,512,822 retired = 17,316,025,
0 failures (windows-x64, -debug, -asan, no ASan finding; -asan's tests_vulkan thread census failed once under load
and passed on its rerun); windows-x64-gcc and the Linux presets 11/11, 0 warnings. reference.py compare: 4 sessions
match. Images by function (x86 and x64): only the part's functions changed (134), 7 of them now folded into
identical functions. tidy_ci: 20 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-78e.

Patella-Modern issue #77 (branch issue-77, November 5, 2026): the developer log and the fatal-error path.
`python -I tools/build.py oracle-x86`: difftest 16,459,337, sysinctest 67, communicationtest 856,621, vlongtest 3,201,
0 failures, CTest 4/4 (unchanged). golden on main's corpus 15,803,203 replayed + 1,512,822 retired = 17,316,025, 0
failures (windows-x64 14/14, -debug and -asan 13/13, no ASan finding); windows-x64-gcc and the Linux presets 11/11, 0
warnings; tests_unit 65 cases on Windows, 64 on Linux. reference.py compare: 4 sessions match. pecompare by function
(x86 and x64): only FatalError, HandleException and (x64) InitInstance changed, the log's functions new. tidy_ci: 20
findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-77.

Patella-Modern issue #78, part 4 (branch issue-78d, November 6, 2026): TBEGIN/TEND deleted from ContentWindows.cpp
and DialogWindow.cpp (177 try blocks in 177 functions); no catch kept, no parameter [[maybe_unused]], the four
destructors' C4297 suppressions gone. `python -I tools/build.py oracle-x86`: difftest 16,459,337, sysinctest 67,
communicationtest 856,621, vlongtest 3,201, 0 failures, CTest 4/4 (every test's own numbers main's). golden on main's
corpus 15,803,203 replayed + 1,512,822 retired = 17,316,025, 0 failures (windows-x64 14/14, -debug 13/13, -asan 13/13,
no ASan finding); windows-x64-gcc and the Linux presets 11/11, 0 warnings. reference.py compare: 4 sessions match.
Images by function (x86 and x64): only the part's functions changed (24 on x86, 25 on x64 now inlined into the part's
callers) and the four deleting destructors that inline its destructors. tidy_ci: 20 findings, 0 new. banned_calls 0,
clang-format clean. Logs: build\logs\issue-78d.

Patella-Modern issue #78, part 6 (branch issue-78f, November 6, 2026): TBEGIN/TEND deleted from Dialogs.cpp (129 try
blocks in 129 functions). `python -I tools/build.py oracle-x86`: difftest 16,459,337, sysinctest 67, communicationtest
856,621, vlongtest 3,201, 0 failures, CTest 4/4 (every count unchanged). golden on main's corpus 15,803,203 replayed +
1,512,822 retired = 17,316,025, 0 failures (windows-x64 14/14, -debug 13/13, -asan 13/13, no ASan finding); windows-x64-gcc and
the Linux presets 11/11, 0 warnings. reference.py compare: 4 sessions match. Images by function (x86 and x64): only
the part's functions changed (11 on x86, 12 on x64 now inlined into the part's callers or folded with identical
code). tidy_ci: 20 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-78f.

Patella-Modern issue #78, part 2 (branch issue-78b, November 5, 2026): TBEGIN/TEND deleted from graphics.cpp,
GUI.cpp, MainWindow.cpp, MapWindow.cpp and MainFrm.cpp (181 try blocks in 165 functions); three MainFrm parameters
[[maybe_unused]]. `python -I tools/build.py oracle-x86`: difftest 16,459,337, sysinctest 67, communicationtest 856,621,
vlongtest 3,201, 0 failures, CTest 4/4 (every count unchanged). golden on main's corpus 15,803,203 replayed + 1,512,822
retired = 17,316,025, 0 failures (windows-x64 14/14, -debug 13/13, -asan 13/13, no ASan finding); windows-x64-gcc and
the Linux presets 11/11, 0 warnings. reference.py compare: 4 sessions match. Images by function (x86 and x64): only
the part's functions changed (18 on x86, 17 on x64 now inlined into the part's callers) and the two deleting destructors that inline
its destructors. tidy_ci: 20 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-78b.

Patella-Modern issue #78, part 1 (branch issue-78a, November 5, 2026): TBEGIN/TEND deleted from Script.cpp,
Inventory.cpp, Player.cpp, Status.cpp, Objects.cpp, Map.cpp and Creatures.cpp (153 try blocks); InitObjects catches
every exception of LoadObjects. `python -I tools/build.py oracle-x86`: difftest 16,459,337, sysinctest 67,
communicationtest 856,621, vlongtest 3,201, 0 failures, CTest 4/4 (every count unchanged). golden 15,803,203 replayed +
1,512,822 retired = 17,316,025, 0 failures (windows-x64, -debug, -asan, 14/14 each, golden_concurrent passes, no ASan
finding); windows-x64-gcc and the Linux presets 11/11, 0 warnings. reference.py compare: 4 sessions match. pecompare
by function (x86 and x64): only the part's functions lose their EH (16 now inlined), 4 callers inline them, switch
tables moved. tidy_ci: 20 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-78a.

Patella-Modern issue #85 (branch issue-85, November 5, 2026): 30-character ignore names and 128-character buddy
descriptions kept whole, as the original kept them. `python -I tools/build.py oracle-x86`: difftest 16,459,078 to
16,459,337 (+259: Dialogs' LongIgnoreNames +80, PreferencesFile's long descriptions +179; the src fix reverted: 21
failures), sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures, CTest 4/4. golden 15,803,203 replayed +
1,512,822 retired = 17,316,025, 0 failures (windows-x64, -debug, -asan, 14/14 each, golden_concurrent passes, no ASan
finding); windows-x64-gcc and the Linux presets 11/11, 0 warnings. reference.py compare: 4 sessions match. pecompare by
function (x86 and x64): only HandleEvent, InitPreferences and SetBuddy (inlining) differ, TFixedString<128>::Assign
gone. tidy_ci: 20 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-85.

Patella-Modern issue #84 (branch issue-84, November 5, 2026): CTest runs each preset's tests in parallel
(CMakePresets.json, execution.jobs 16; COST on the long tests), golden_concurrent only in windows-x64's Release (not Debug,
not ASan); no test needed RESOURCE_LOCK or RUN_SERIAL. No src/ change. notes/decisions.md, "Tests in parallel". Test time
before to after: windows-x64 228.6 s to 118.3 s, windows-x64-debug 780.7 s to 266.6 s, windows-x64-asan 1,821.1 s to
775.8 s, oracle-x86 unchanged within noise (difftest alone 220-263 s). Counts unchanged (main 7764a95): golden 15,802,944
replayed + 1,512,822 retired, 0 failures (windows-x64 14/14, -debug and -asan 13/13, no ASan finding); difftest 16,459,078,
sysinctest 67, communicationtest 856,621, vlongtest 3,201, 0 failures, CTest 4/4; windows-x64-gcc and the Linux presets
green. Logs: build\logs\issue-84.

Patella-Modern issue #75 (branch issue-75, November 5, 2026): the built-in patcher removed (decision D9). `python -I
tools/build.py oracle-x86`: difftest 16,471,911 to 16,459,078 (-12,833: Dialogs -3,633 and errors -6,083, MainFrm -598
and errors -285, Errors -100, Script -2,134), communicationtest 1,044,013 to 856,621 (-187,392: Communication -36,654
with TestUpdateRequests' +1,800, its errors -150,696, Control -42), sysinctest 67, vlongtest 3,201, 0 failures, CTest 4/4.
golden 15,802,944 replayed + 1,512,822 retired = 17,315,766, 0 failures (windows-x64, -debug, -asan, 14/14 each,
golden_concurrent passes, no ASan finding); windows-x64-gcc and the Linux presets 11/11, 0 warnings. reference.py
compare: 4 sessions match; compare_files.py (live) and compare_traffic.py (live): no differences. PatellaTest.exe on the
recorded patch session and a zero-sized patch: the "Sorry" dialog, no file written. pecompare by function: 318 gone, 5
new, 9 resized (x64). tidy_ci: 20 findings, 0 new. banned_calls 0, clang-format clean. Logs: build\logs\issue-75.

Patella-Modern issue #76 (branch issue-76, November 5, 2026): the error paths compared by outcome only (thrown or not,
and the kind) in difftest, communicationtest and the golden corpus; the line-pair checks are gone, the corpus records
no exception texts (format 2, recorded again); plan 4.2's stringized-expression rule, macrocheck and tools/tidy.py's
guard of the TASSERT/TERROR/TEND arguments retired. notes/decisions.md, "The error paths compared by outcome only".
No src/ change. `python -I tools/build.py oracle-x86`: difftest 16,599,178 to 16,471,911 (-127,267 line-pair checks),
communicationtest 1,114,552 to 1,044,013 (-70,539), sysinctest 67, vlongtest 3,201, 0 failures, CTest 4/4; the
exceptions compared unchanged in every test. golden 16,001,985 replayed + 1,514,006 retired = 17,515,991, 0 failures
(windows-x64, -debug, -asan, 14/14 each, golden_concurrent passes, no ASan finding); windows-x64-gcc and the Linux
presets 11/11, 0 warnings. Mutations (reverted): GUI.cpp's SetSize and Utils.cpp's LowerString assertions with another
text and line pass both; LowerString not throwing fails both (1 failure each). pecompare (x64 Patella.exe, main and the
branch at the same path): link stamps only. tidy_ci: 20 findings (the 16 use-nullptr ones of Utils.cpp's assertions
now count; baseline rewritten), 0 new, 0 bugprone. banned_calls: 0. clang-format: clean. Logs: build\logs\issue-76.

Patella-Modern issue #56 (branch issue-56b, from issue-56, November 5, 2026, not merged): the owner's decision is
a second interactive session for the live checks (notes/reconstruction.md, "Checking", "The live checks in a second
session"); every live tool runs on the desktop of the session that starts it, as on main, and tools/desktop.py's
hidden desktop only with `--hidden` (experimental; `--visible` is gone). No src/ change. windows-x64 (14 tests, 2
golden skipped) and oracle-x86 (4 difftests) built, exit 0. Self-tests pass: desktop.py, reference.py,
patch_client.py, client_state.py, testmode_check.py. Visible: reference.py compare all 8 sessions match;
compare_files.py with its live walk no differences; compare_traffic.py --skip-live on the main checkout's live
recordings no differences. Its live half and testmode_check.py fail on main's tools too (rerun once each): two
trolls attack the test character in the temple, so the Ctrl+L logout is refused ("still connected to the game
server 10 s after Ctrl+L"; testmode_check.py: the client does not close within 180 s of quit). Hidden:
reference.py compare --sessions traffic --vulkan gpu --hidden matches. Logs: build\logs\issue-56 of the worktree.

The first version (branch issue-56, the same day): tools/desktop.py ran each live check on a hidden desktop of its
own (`--visible`: the old behaviour); the mechanism
and `python -I tools/desktop.py --self-test` work, but the clients behave differently there (no DirectDraw or
Direct3D 9, the CPU driver's first present fails, GetCursorPos fails): notes/decisions.md, "The live checks on a
hidden desktop". No src/ change. windows-x64 and oracle-x86 built (exit 0). Visible, replays from the main checkout's
live recordings (the user's tibia-server.exe from C:\Tibia\gameserver blocked the live runs and testmode_check.py):
reference.py compare all 8 sessions match; compare_traffic.py --skip-live no differences; compare_files.py --skip-live
no differences (report rerun once: Patella.exe had not closed in 20 s); compare_network.py 0 of 51 differ;
compare_rendering.py vulkan, vulkan-cpu, dx5, dx9, gl windowed identical (vulkan rerun once: the display mode
changed during the run; dx9 rerun once: two packets swapped in the oracle's replay). Hidden: only compare_network.py
and the gl engine match, and reference.py compare --vulkan gpu (traffic session).

Patella-Modern issue #73 (branch issue-73, November 5, 2026): tools/patch_client.py patches the Debug clients'
login port (windows-x64-debug and windows-x64-asan-debug: InitServerList's loop stores through a register; found
through the PDB's InitServerList when none of the four stores to Servers[i].Port is there); notes/decisions.md,
"patch_client.py on the Debug builds". No src/ change. `python -I tools/patch_client.py --self-test`: passed (the new
form covered). main's tool and the branch's on Tibia.exe, the oracle-x86 Release Patella.exe and the windows-x64 and
windows-x64-asan Release Patella.exe and PatellaTest.exe (--port 7271 with and without --rsa, --host alone): identical
byte for byte; the Debug images: --host alone identical, --port now patched (main refused them). `python -I
tools/build.py windows-x64-asan-debug`, `windows-x64-debug` and `windows-x64-asan` (CTest 14/14 each; golden and
golden_concurrent skipped: no corpus in the worktree). `python -I tools/reference.py compare --exe
build\windows-x64-asan\Debug\PatellaTest.exe --out build\reference-compare-asan-debug`: every session matches, no ASan
finding. Found: `--rsa` refuses the Debug images (the modulus twice: no string pooling).

Patella-Modern issue #71 (branch issue-71, November 5, 2026): the main window's uncovered client area filled black
during a live resize (src/vulkan/PresentWindow.cpp; notes/decisions.md, "The main window's background during a live
resize"). No code the oracle compiles changed (oracle-x86 not rebuilt). windows-x64 and -debug (--fresh, CTest
13/13, 0 warnings): golden 16,461,713 replayed, 1,521,619 retired, 0 not ported, 0 failures; tests_unit 57/10,080,
standin 57/10,082, tests_core 38/360, tests_testmode 35/709, tests_render 14/359 unchanged; tests_vulkan 14/1,309 to
15/1,392 (the new case, 83 assertions). windows-x64-asan (--fresh) 13/13; windows-x64-gcc and the Linux presets
(--fresh) 11/11 (Linux tests_unit 56/10,062, Clang 10,060, standin 56/10,064, tests_core 38/363), 0 warnings.
`python -I tools/reference.py compare`: every session matches. pecompare 3de8059 and issue-71 at the same path (x64):
TPresentWindows' destructor and Open changed, ExcludeChild and ParentProc new, the COMCTL32.dll import (ordinals
410, 412, 413); the rest moved. macrocheck: 2,108 calls, 0 differences. tidy_ci: 4 findings, 0 new, 0 bugprone.
banned_calls: 0. clang-format: clean.

Patella-Modern issue #66 (branch issue-66, November 5, 2026): each golden run in a temporary folder of its own.
golden_runner's TRunFolder (`%TEMP%\golden-run-<pid>-<n>`, its TEMP, TMP and current directory, removed at the end),
golden_corpus through cmake/RunGoldenRecord.cmake (a folder per recording below build\oracle-x86\tmp, removed); the new
CTest test golden_concurrent (tests/golden/concurrent.py) runs two runners at once. No src/ change; notes/decisions.md,
"Each golden run in a temporary folder of its own". `python -I tools/build.py oracle-x86` (CTest 4/4, 0 warnings):
16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures; golden_corpus into the branch's build\golden: the
same counts, build\oracle-x86\tmp and %TEMP% without a folder of it afterwards. windows-x64 and -debug (--fresh) and
windows-x64-asan (CTest 14/14 each, 0 warnings): golden 16,461,713 replayed, 1,521,619 retired, 0 not ported, 0
failures, golden_concurrent both runners the same, nothing left in %TEMP%; the unit tests' counts unchanged. windows-x64-gcc
and the Linux presets (--fresh) 11/11, 0 warnings. tidy_ci: 4 findings, 0 new, 0 bugprone. banned_calls: 0.
clang-format: clean.

Patella-Modern issue #70 (branch issue-70, November 5, 2026): sanitizers over the replayed sessions, M4's exit. The
preset windows-x64-asan (MSVC /fsanitize=address through PATELLA_SANITIZE, Release and Debug); the original's two
out-of-bounds reads suppressed with PATELLA_NO_SANITIZE_ADDRESS (TRGBColor::operator==, Network.cpp's ReceiveData and
SendData); three test instruments taught ASan's code (tests_vulkan's census, FindOwnStatic's search, RunFrames' stack
pattern); notes/decisions.md, "Sanitizers over the replayed sessions". `python -I tools/build.py windows-x64-asan
--fresh` and `windows-x64-asan-debug --fresh` (CTest 13/13 each, 0 warnings): golden_runner 16,461,713 replayed,
1,521,619 retired, 0 not ported, 0 failures, no ASan finding; the unit tests' counts as without ASan. `python -I
tools/reference.py compare --exe build\windows-x64-asan\Release\PatellaTest.exe --out build\reference-compare-asan`: every session
matches, no finding (the Debug ASan client is not patched by patch_client.py: its port stores go through a register).
linux-x64-clang and -debug (--fresh, ASan, UBSan, LeakSanitizer): 11/11, clean.
`python -I tools/build.py oracle-x86 --fresh` (CTest 4/4, 0 warnings): 16,848,752 + 67 + 1,134,513 checks and 3,201
vlong checks, 0 failures; with recording on (`golden_corpus`, the branch's own build\golden) the same counts.
windows-x64 and -debug (--fresh, CTest 13/13): golden 16,461,713 replayed, 1,521,619 retired, 0 failures; tests_unit
57/10,080, standin 57/10,082, tests_core 38/360, tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309;
windows-x64-gcc and the Linux presets (--fresh) 11/11 (Linux tests_unit 56/10,062, Clang 10,060, standin 56/10,064,
tests_core 38/363), 0 warnings, the analyzers clean. pecompare aa396fa and issue-70 at the same path: the link
stamps only (.text, .data and .rdata identical, oracle and x64). macrocheck: 2,108 calls, 0 differences. tidy_ci: 4
findings, 0 new, 0 bugprone. banned_calls: 0. clang-format: clean.

Patella-Modern issue #58, second part (branch issue-58b, November 5, 2026): std::unique_ptr for 7 more globals
(Skins, the six graphics tables) and 11 more members (TComponent::DrawingList, TLabel::Text and Line,
TFormattedText::Text and Line, TGraphicsOptionsDialog::Modes, Container.h's entry in its four templates,
flex_unit::a); every owner that stays raw is in notes/ownership.md's section 7 with the milestone that unblocks it.
`python -I tools/build.py oracle-x86 --fresh` (CTest 4/4, 0 warnings; the new static_asserts of the original's sizes
and offsets compile): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures; with recording on
(`golden_corpus`, the branch's own build\golden, 903 MB) the same counts. The x64 golden_runner (windows-x64 and
-debug, --fresh, CTest 13/13) replays 16,461,713 checks, 1,521,619 retired, 0 not ported, 0 failures. tests/unit:
tests_core 36/352 to 38/360 on Windows and with mingw-w64, Linux 36/355 to 38/363 (core_skins_test.cpp's two new
cases); tests_unit 57/10,080, standin 57/10,082 (Linux 56/10,062, Clang 10,060, standin 56/10,064), tests_testmode
35/709, tests_render 14/359, tests_vulkan 14/1,309 unchanged; windows-x64-gcc and the Linux presets (--fresh) 11/11, 0
warnings, the analyzers clean (Clang's ASan/UBSan and LeakSanitizer too). pecompare 3f6042a and issue-58b at the same
path: code only (the oracle resizes 194 functions, .text -0x600, adds the seven globals' initialisers and atexit
destructors and the unique_ptrs' out-of-line destructors; x64 231, .text +0x1c00; no string of .rdata changed).
macrocheck: 2,108 calls, 0 differences. tidy_ci: 4 findings (from 7: the raw Skins' two, reserve's raw block), 0 new, 0 bugprone; the
baseline written again. banned_calls: 0. clang-format: clean. client_state.py --layout: the oracle equal to the
original, x64 0 problems; its and patch_client.py's self-tests pass.

Patella-Modern issue #69 (branch issue-69, November 5, 2026): the remaining pointer and length parameters to spans
(readBytes/writeBytes, CheckClipboardText, CopyString, FormatNumber, GetErrorMessage, DrawOutfit; CompareStrings and
ShortenString kept). `python -I tools/build.py oracle-x86 --fresh` (CTest 4/4, 0 warnings): 16,848,752 + 67 +
1,134,513 checks and 3,201 vlong checks, 0 failures; with recording on (`golden_corpus`, the branch's own
build\golden, 903 MB) the same counts. The x64 golden_runner (windows-x64 and -debug, --fresh, CTest 13/13) replays
16,461,713 checks, 1,521,619 retired, 0 not ported, 0 failures. tests/unit: tests_core 35/337 to 36/352 on Windows
and with mingw-w64, Linux 35/340 to 36/355 (the new spans case); tests_unit 57/10,080, standin 57/10,082 (Linux
56/10,062, Clang 10,060, standin 56/10,064), tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309
unchanged; windows-x64-gcc and the Linux presets (--fresh) 11/11, 0 warnings, the analyzers clean. pecompare 7ad3e31
and issue-69 at the same path: code only (the oracle resizes 20 functions, .text +0x2b0; x64 51, .text +0x2c0; no
function added or removed, no string of .rdata changed). macrocheck: 2,108 calls, 0 differences. tidy_ci: 7
findings, 0 new, 0 bugprone. banned_calls: 0. clang-format: clean.

Patella-Modern issue #63 (branch issue-63, November 5, 2026): static analysis of patella_core, MSVC /analyze and GCC
-fanalyzer in its build, clang-tidy's bugprone-* at zero (`tools/tidy.py --zero bugprone-`). `python -I tools/build.py
oracle-x86 --fresh` (CTest 4/4, 0 warnings): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures;
with recording on (`golden_corpus`, the branch's own build\golden, 903 MB) the same counts. The x64 golden_runner
(windows-x64 and -debug, --fresh, CTest 13/13) replays 16,461,713 checks, 1,521,619 retired, 0 not ported, 0
failures. tests/unit unchanged: tests_unit 57/10,080, standin 57/10,082, tests_core 35/337, tests_testmode 35/709,
tests_render 14/359, tests_vulkan 14/1,309 (Linux 56/10,062, Clang 10,060, standin 56/10,064, tests_core 35/340);
windows-x64-gcc and the Linux presets (--fresh, Release and Debug) 11/11, 0 warnings everywhere, the analyzers
clean in every configuration. pecompare bf3e6df and issue-63 at the same path: oracle and x64 identical but for the
link stamps. macrocheck: 2,108 calls, 0 differences. tidy_ci: 7 findings (from 13), 0 new, 0 bugprone.
banned_calls: 0. clang-format: clean.

Patella-Modern issue #60 (branch issue-60, November 5, 2026): std::array and TFixedString for the C arrays (members,
globals, locals), spans for vlong's byte arrays and the test mode's packet hooks. `python -I tools/build.py oracle-x86
--fresh` (CTest 4/4, 0 warnings; the new static_asserts of the frozen layouts compile): 16,848,752 + 67 + 1,134,513
checks and 3,201 vlong checks, 0 failures; with recording on (`golden_corpus`, the branch's own build\golden, 903 MB)
the same counts. The x64 golden_runner (windows-x64 and -debug, --fresh, CTest 13/13; the Debug build checks every
std::array index) replays 16,461,713 checks, 1,521,619 retired, 0 not ported, 0 failures. tests/unit: tests_core 34/329
to 35/337 on Windows and with mingw-w64, Linux 34/332 to 35/340 (vlong's span edge cases); tests_unit 57/10,080,
tests_unit_cxx26_standin 57/10,082 (Linux 56/10,062, Clang 10,060, standin 56/10,064), tests_testmode 35/709,
tests_render 14/359, tests_vulkan 14/1,309 unchanged; windows-x64-gcc and the Linux presets (--fresh) 11/11, 0 warnings
everywhere. pecompare d9b3cfd and issue-60 at the same path: code only (the oracle resizes 83 functions, .text -0x950;
x64 75, .text +0x800; out-of-line std::array::operator[] copies; no string of .rdata changed); `client_state.py
--layout` on the new oracle equal to the original. macrocheck d9b3cfd issue-60: 2,108 calls, 0 differences. tidy_ci:
13 findings, 0 new. banned_calls: 0. clang-format: clean.

Patella-Modern issue #59 (branch issue-59, November 4, 2026): std::span parameters (src/compat/span.h's TSpan) for the
packet buffers, Network.h's wrappers, TXTEA and TRSA. `python -I tools/build.py oracle-x86 --fresh` (CTest 4/4, 0
warnings): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures; with recording on (`golden_corpus`,
the branch's own build\golden, 903 MB) the same counts, and CTest again on the final head. The x64 golden_runner
(windows-x64 and -debug, --fresh, CTest 13/13) replays 16,461,713 checks, 1,521,619 retired, 0 not ported, 0
failures. tests/unit: tests_unit 52/10,027 to 57/10,080 and tests_unit_cxx26_standin 52/10,029 to 57/10,082 on
Windows and with mingw-w64 (span_test.cpp: 5 cases, 53 assertions), Linux 51/10,009 to 56/10,062 (Clang 10,007 to
10,060) and standin 51/10,011 to 56/10,064; tests_core 32/313 to 34/329 (Linux 32/316 to 34/332: the buffers' span
edge cases and a TXTEA block inside a packet); tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309
unchanged; windows-x64-gcc and the Linux presets (--fresh) 11/11, 0 warnings everywhere. pecompare db0e223 and
issue-59 at the same path: the oracle resizes the 12 changed functions, x64 38 (notes/decisions.md); no string
changed. macrocheck db0e223 issue-59: 2,108 calls, 0 differences. tidy_ci: 13 findings, 0 new; Visual Studio's
clang-tidy on windows-x64's commands of the changed .cpp files: nothing new on a changed line. banned_calls: 0.
clang-format: clean.

Patella-Modern issue #30, third part (branch issue-30c, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh`
(CTest 4/4, 0 warnings) after Control moved to tests/golden: 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong
checks, 0 failures; with recording on (`golden_corpus`, 903 MB) the same counts. The x64 golden_runner (windows-x64 and
-debug, --fresh, CTest 13/13) replays 16,461,713 of the oracle's 17,983,332 checks and retires 1,521,619 (Control's
24,155 new), 0 failures, after src/Control.cpp's event 0x12 fix (before it: 60 failures, all that call). src/ against
8d48db9: that fix only; pecompare of the oracle's Patella.exe (8d48db9 and the fix, at the same path): 0 unexpected
differences; macrocheck 8d48db9 issue-30c: 2,108 calls, 0 differences.

Patella-Modern issue #30, second part (branch issue-30b, November 4, 2026): `python -I tools/build.py oracle-x86
--fresh` (CTest 4/4, 0 warnings) after Communication and its world moved to tests/golden: 16,848,752 + 67 + 1,134,513
checks (Communication 364,041 + 492,325, Control 114,595 + 163,552) and 3,201 vlong checks, 0 failures; with
recording on (`golden_corpus`, 886 MB) the same counts. The x64 golden_runner (windows-x64 and -debug, --fresh, CTest
13/13) replays 16,207,721 of the oracle's 17,983,332 checks, 0 failures: the earlier 15,351,355 as before and
Communication's 856,366; retired 1,497,464 as before; not ported 278,147 (Control). src/ unchanged (no diff against
8e76abc, main with issue #62); macrocheck 8e76abc issue-30b: 2,108 calls, 0 differences.

Patella-Modern issue #64 (branch issue-64, November 4, 2026): TFile, PrintLine and the constant formats. `python -I
tools/build.py oracle-x86 --fresh` (CTest 4/4, 0 warnings; Script.h's new static_asserts compile): 16,848,752 + 67 +
1,134,513 checks and 3,201 vlong checks, 0 failures; with recording on (`golden_corpus`, the branch's own build\golden,
846 MB) the same counts. The first run had 403 ContentWindows failures ("logs differ"): the golden test logged our
MouseHover output through a fake printf, which our side no longer calls; it fakes our stdout fwrite now (forced,
notes/decisions.md). The x64 golden_runner (windows-x64 and -debug, --fresh, CTest 13/13) replays 15,351,355 checks,
1,497,464 retired, 0 failures. tests/unit: tests_unit 52/10,027 and tests_unit_cxx26_standin 52/10,029 on Windows and
with mingw-w64 (file_test.cpp: 11 cases, 160 assertions), 51/10,009 (10,007 on Clang) and 51/10,011 on Linux;
tests_core 32/313 (Linux 32/316), tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309 unchanged;
windows-x64-gcc(-debug) and the four Linux presets (--fresh) 11/11, 0 warnings everywhere. macrocheck 8e76abc
issue-64: 2,108 calls, 0 differences. tidy_ci: 13 findings, 0 new; Visual Studio's clang-tidy on windows-x64's
commands of the changed files: modernize-use-std-print 4 to 0, nothing new. banned_calls: 0 (with the file
functions). clang-format: clean.

Patella-Modern issue #62 (branch issue-62, November 4, 2026): 64-bit time everywhere. `python -I tools/build.py
oracle-x86 --fresh` (CTest 4/4, 0 warnings; the new static_asserts of SkullTime +0x98 and TTalkLine::Time +0x3c
compile): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures; with recording on (`golden_corpus`,
the branch's own build\golden, 843 MB) the same counts. The x64 golden_runner (windows-x64 and -debug, --fresh, CTest
13/13) replays 15,310,983 checks, 1,497,428 retired, 0 failures, as on main (Talk, Creatures, ConsoleWindow and
ContentWindows fake our _time64 now; the first Debug run had 753 failures in ConsoleWindow's SaveChannel file, ours
not removed between calls and one fopen refused: another worktree's golden_runner ran at the same time, and both
write ConsoleWindow's channel file in the user's TEMP; its rerun alone, `ctest -R golden`, 0 failures). tests/unit: tests_unit 41/9,867 and tests_unit_cxx26_standin
41/9,869 on Windows (time32_test.cpp: 6 cases, 81 assertions; also with mingw-w64), 40/9,858 (9,856 on Clang) and
40/9,860 on Linux; tests_core 32/313 (32/316 on Linux: msvc's 32-bit time case, 19 assertions, went with it);
tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309 unchanged; windows-x64-gcc(-debug) and the four
Linux presets (--fresh) 11/11. macrocheck 01feaf9 issue-62: 2,108 calls, 0 differences. tidy_ci: 13 findings, 0 new.
banned_calls: 0. clang-format: clean.

Patella-Modern issue #27, second part (branch issue-27b, November 4, 2026): `python -I tools/build.py oracle-x86
--fresh` (CTest 4/4, 0 warnings) after MapWindow and MainWindow moved to tests/golden: 16,848,752 + 67 + 1,134,513
checks and 3,201 vlong checks, 0 failures (MapWindow 115,169 + 12,359 in its error round, 1,394 calls that threw;
MainWindow 14,164 + 43,150, 229 threw; as before); with recording on (`golden_corpus`, 843 MB) the same counts. The
x64 golden_runner (windows-x64 and -debug, --fresh, CTest 13/13) replays 15,310,983 of the oracle's 17,983,332 checks,
0 failures: the earlier 15,126,140 as before, MapWindow's 127,529 and MainWindow's 57,314; retired with the oracle
1,497,428 (Dialogs' 9,129 and #29's SurfaceDX, DX9, GL, 1,488,299); not ported 1,174,921 (Errors 40,408,
Communication 856,366, Control 278,147). src/ unchanged (no diff against 96b245b); macrocheck 96b245b issue-27b:
2,108 calls, 0 differences.

Patella-Modern issue #61 (branch issue-61, November 4, 2026): FormatCRT71Time instead of ctime. `python -I tools/build.py
oracle-x86 --fresh` (CTest 4/4, 0 warnings, compare_resources 0 differences): 16,848,752 + 67 + 1,134,513 checks and
3,201 vlong checks, 0 failures (errortest's image ctime now the UCRT's _ctime32 of its clock in 7.1's form, forced);
with recording on (`golden_corpus`, the branch's own build\golden, 661 MB) the same counts. The x64 golden_runner
(windows-x64 and -debug, --fresh, CTest 13/13) replays 15,126,140 checks, 9,129 retired, 0 failures, as on main.
tests/unit: tests_unit 35/9,786 and tests_unit_cxx26_standin 35/9,788 on Windows (crt71_test.cpp: 7 cases, 95
assertions in each, the _ctime32 comparison among them; also with mingw-w64), 34/9,777 (9,775 on Clang) and 34/9,779
on Linux (6 cases, 86); tests_core 33/332 (33/335 on Linux: MsvcCtime32's 8 assertions went with it), tests_testmode
35/709, tests_render 14/359, tests_vulkan 14/1,309 unchanged; windows-x64-gcc(-debug) and the four Linux presets
(--fresh) 11/11. macrocheck 9f8e5a9 issue-61: 2,108 calls, 0 differences. tidy_ci: 13 findings, 0 new.
banned_calls: 0.

Patella-Modern issue #28, third part (branch issue-28c, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh`
(CTest 4/4, 0 warnings) after Dialogs moved to tests/golden: 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong
checks, 0 failures (Dialogs 98,889 + 200,285 in its error round, as before); with recording on (`golden_corpus`) the
same counts. The x64 golden_runner (windows-x64 and -debug, --fresh, CTest 13/13) replays 15,126,073 of difftest's
checks and sysinctest's 67, 0 failures: the second part's 14,836,028 as before and Dialogs' 290,045, with its 9,129
checks of the advanced graphics dialog retired with the oracle (replayed + retired = 299,174, the oracle's); not yet
ported: difftest's 1,713,550 other checks (SurfaceDX, DX9, GL, Errors, MapWindow, MainWindow) and
communicationtest's 1,134,513. src/ unchanged (no diff against ce3ed2b); macrocheck ce3ed2b issue-28c: 2,108 calls,
0 differences.

Patella-Modern issue #58 (branch issue-58, November 4, 2026): notes/ownership.md (the ownership map), then
std::unique_ptr for 22 owning members of 17 classes and 6 globals (ConsoleWindow, ContentWindows, Dialogs,
DialogElements, Status, Communication, Objects; the original's deletes as explicit reset()s in place and order).
`python -I tools/build.py oracle-x86 --fresh` (CTest 4/4, 0 warnings; the new static_asserts of the original's sizes and
offsets compile): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures (also on b37760b, after the
first two conversions); with recording on (`golden_corpus`, the branch's own build\golden, 609 MB) the same counts. The
x64 golden_runner (windows-x64 and -debug, --fresh, CTest 13/13) replays 14,836,028 of difftest's checks and
sysinctest's 67, 0 failures. tests/unit unchanged: tests_unit 28/9,691 (9,689 on Clang), tests_unit_cxx26_standin
28/9,693, tests_core 33/340 (33/343 on Linux), tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309;
windows-x64-gcc and the four Linux presets (--fresh) 11/11. macrocheck c0a4fd8 issue-58: 2,108 calls, 0 differences.
tidy_ci: 13 findings, 0 new. banned_calls: 0. client_state.py --layout: 0 problems on the new x64 Patella.exe and
PatellaTest.exe, the new oracle equal to the original (main's tool reports the unique_ptr members as problems).

Patella-Modern issue #28, second part (branch issue-28b, November 4, 2026): `python -I tools/build.py oracle-x86
--fresh` (CTest 4/4, 0 warnings) after DialogWindow, ContextMenus, ConsoleWindow, ContentWindows and DialogElements
moved to tests/golden: 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures; with recording on
(`golden_corpus`) the same counts. The x64 golden_runner (windows-x64 and -debug, --fresh, CTest 13/13) replays
14,836,028 of difftest's checks and sysinctest's 67, 0 failures (the first part's 12,858,032 as before, DialogWindow
34,068, ContextMenus 41,660, ConsoleWindow 318,882, ContentWindows 221,270, DialogElements 1,362,116, each error round
with the oracle's counts); not yet ported: difftest's 2,012,724 other checks (Dialogs, SurfaceDX, DX9, GL, Errors,
MapWindow, MainWindow) and communicationtest's 1,134,513. src/ unchanged (no diff against 0ab4d3c); macrocheck
0ab4d3c issue-28b: 2,108 calls, 0 differences.

Patella-Modern issue #57 (branch issue-57, November 4, 2026): TFixedString<N> and TStringBuffer
(src/types/FixedString.h) for the client's text buffers; no strcpy, strcat, sprintf, vsprintf or gets left in src/
(tools/banned_calls.py, a job of the hosted CI). `python -I tools/build.py oracle-x86 --fresh` (CTest 4/4, 0
warnings; compare_resources 0 differences): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures,
after each of the three conversion commits too; with recording on (`golden_corpus`, a corpus of the branch in its
own build\golden, 394 MB) the same counts. The x64 golden_runner (windows-x64 --fresh, CTest 13/13) replays
12,858,032 of difftest's checks and sysinctest's 67, 0 failures; windows-x64-debug --fresh had 3 failures in the
first run (golden's Script test: our reader saw an empty script file at the start of one round, then the replay
lost its place), and its rerun replayed the same 12,858,099 with 0 failures (not reproduced). The images change
(new code, moved data): pecompare is not the proof any more. tests/unit: tests_unit 28 cases / 9,691 assertions
(18/158 before; 9,689 on Clang), tests_unit_cxx26_standin 28/9,693 (18/160), the 10 new cases 9,533 assertions in
each; tests_core 33/340 (33/343 on Linux), tests_testmode 35/709, tests_render 14/359, tests_vulkan 14/1,309
unchanged; windows-x64-gcc and the four Linux presets 11/11. macrocheck b061421 issue-57: 2,108 calls, 0
differences. tidy_ci: 13 findings, 0 new (the baseline lost Utils.cpp's sprintf).

Patella-Modern issue #28 (branch issue-28, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after Graphics and GUI moved to tests/golden: 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong
checks, 0 failures (Graphics 570,954; GUI 5,246,029 + its error round's 31,909, 2,750 calls compared, 2,154 that
threw); with recording on (`golden_corpus`) the same counts. The x64 golden_runner (windows-x64 and -debug, --fresh,
CTest 13/13) replays 12,858,032 of difftest's checks and sysinctest's 67, 0 failures (issue #30's 7,009,140 as
before, Graphics 570,954, GUI 5,277,938 with the oracle's error-round counts); not yet ported: difftest's 3,990,720
other checks (DialogWindow, ContextMenus, ConsoleWindow, ContentWindows, DialogElements, Dialogs, SurfaceDX, DX9, GL,
Errors, MapWindow, MainWindow) and communicationtest's 1,134,513. src/ unchanged (pecompare of the oracle and the x64
Patella.exe: 0 unexpected differences); macrocheck 6b40354 issue-28: 2,108 calls, 0 differences.

Patella-Modern issue #30 (branch issue-30, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after Network moved to tests/golden: 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0
failures (Network 74,928 + its error round's 38,878, 7,393 calls compared, 556 that threw); with recording on
(`golden_corpus`) the same counts. The x64 golden_runner (windows-x64 and -debug, CTest 13/13) replays 7,009,140 of
difftest's checks and sysinctest's 67, 0 failures (issue #54's 6,895,334 as before, Network 113,806 with the
oracle's error-round counts); not yet ported: difftest's 9,839,612 other checks (Errors 40,408, MapWindow 127,529,
MainWindow 57,314 among them) and communicationtest's 1,134,513 (Communication 856,366, Control 278,147: they need
GUI.cpp and the window files in the runner, notes/decisions.md, issue #30). src/ unchanged; macrocheck 7df9603
issue-30: 2,108 calls, 0 differences.

Patella-Modern issue #54 (branch issue-54, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the error rounds' comparison moved to tests/golden/golden_eh and MainFrm to tests/golden:
16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures (MainFrm 31,424 + its error round's 18,152,
5,504 calls compared, 535 that threw); with recording on (`golden_corpus`) the same counts. The x64 golden_runner
(windows-x64 and -debug, CTest 13/13) replays 6,895,334 of difftest's checks and sysinctest's 67, 0 failures
(issue #27's 6,845,758 as before, MainFrm 49,576 with the oracle's error-round counts); not yet ported: difftest's
9,953,418 other checks (Errors 40,408, MapWindow 127,529, MainWindow 57,314 among them) and communicationtest's
1,134,513. src/ unchanged; macrocheck 839de10 issue-54: 2,108 calls, 0 differences.

Patella-Modern issue #27 (branch issue-27, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after Objects, Map and Creatures moved to tests/golden: 16,848,752 + 67 + 1,134,513 checks and 3,201
vlong checks, 0 failures; with recording on (`golden_corpus`) the same counts. The x64 golden_runner (windows-x64 and
-debug, CTest 13/13) replays 6,845,758 of difftest's checks and sysinctest's 67, 0 failures (issue #26's 3,267,008 as
before, Objects 327,886, Map 639,877, Creatures 2,610,987); not yet ported: difftest's 10,002,994 other checks (Errors
40,408, MainFrm 49,576, MapWindow 127,529, MainWindow 57,314 among them) and communicationtest's 1,134,513. src/
unchanged; macrocheck d34c304 issue-27: 2,108 calls, 0 differences.

Patella-Modern issue #26 (branch issue-26, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after nine tests moved to tests/golden: 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0
failures, every per-test count as below; with recording on (`golden_corpus`) the same counts. The x64 golden_runner
(windows-x64 and -debug, CTest 13/13) replays 3,267,008 of difftest's checks and sysinctest's 67, 0 failures (issue
#24's eight tests as before, Status 565,809, Preferences 102,060, Script 458,232, PreferencesFile 112,155, Player
18,442, Help 70,631, Inventory 158,400, Talk 1,214,929, Container 192,832); not yet ported: difftest's 13,581,744
other checks (Errors 40,408 among them) and communicationtest's 1,134,513. src/ unchanged; macrocheck 85eafb1
issue-26: 2,108 calls, 0 differences.

Patella-Modern issue #50 (branch issue-50, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the four const pointer parameters (readability-non-const-parameter): 16,848,752 + 67 +
1,134,513 checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release
Patella.exe and PatellaTest.exe built from main (a338fa8) and from the branch at the same path: 0 unexpected
differences (pecompare); macrocheck a338fa8 issue-50: 2,108 calls, 0 differences.

Patella-Modern issue #49 (branch issue-49, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings, the header checks header_check_oracle and header_check_oracle_shim built) after the headers' includes
and the header checks (four commits): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures, as below;
the oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe built from main (968ec47) and from
the branch at the same path: 0 unexpected differences (pecompare); macrocheck 968ec47 issue-49: 2,108 calls, 0
differences.

Patella-Modern issue #44 (branch issue-44, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the file-scope statics into anonymous namespaces (three commits): 16,848,752 + 67 + 1,134,513
checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe and
PatellaTest.exe built from main (9312a61) and from the branch at the same path: 0 unexpected differences (pecompare;
`.text` and `.data` identical, every symbol at the same address), `client_state.py --layout` unchanged; macrocheck
9312a61 issue-44: 2,108 calls, 0 differences.

Patella-Modern issue #43 (branch issue-43, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the MSVC-only spellings behind compat/msvc.h (three commits): 16,848,752 + 67 + 1,134,513
checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe and
PatellaTest.exe built from main (bc7417c) and from the branch at the same path: 0 unexpected differences (pecompare);
macrocheck bc7417c issue-43: 2,108 calls, 0 differences.

Patella-Modern issue #42 (branch issue-42, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the gotos (five commits and the check): 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong
checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe
built from main (f2b3374) and from each commit at the same path: only the touched function's code differs per commit
(TBattleWindowBody::Draw, TSurfaceGL::WriteMaskedImage, TSurfaceDX9::WriteMaskedImage on the oracle;
TSurfaceVulkan::WriteMaskedImage on x64), every other difference a moved address (notes/decisions.md, issue #42);
macrocheck f2b3374 issue-42: 2,108 calls, 0 differences.

Patella-Modern issue #41 (branch issue-41, November 4, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the C-style casts to named casts (two commits): 16,848,752 + 67 + 1,134,513 checks and 3,201
vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe
built from main (ac12de2) and from each commit at the same path: `.text`, `.data` and `.reloc` identical, 0 unexpected
differences (pecompare); macrocheck ac12de2 issue-41: 2,108 calls, 0 differences.

Patella-Modern issue #39 (branch issue-39, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the const locals and the const member functions (three commits): 16,848,752 + 67 + 1,134,513
checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe and
PatellaTest.exe built from main (762f81b) and from each commit at the same path: 0 unexpected differences (pecompare);
macrocheck 762f81b issue-39: 2,108 calls, 0 differences.

Patella-Modern issue #34 (branch issue-34, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after = default / = delete and the non-copyable owning classes (three commits): 16,848,752 + 67 +
1,134,513 checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release
Patella.exe and PatellaTest.exe built from main (b123283) and from each commit at the same path: `.text`, `.data` and
`.pdata` identical, 0 unexpected differences (pecompare); macrocheck b123283 issue-34: 2,108 calls, 0 differences.

Patella-Modern issue #37 (branch issue-37, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the explicit enum types and TOKEN as an enum class (two commits): 16,848,752 + 67 + 1,134,513
checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe
and PatellaTest.exe built from main (c797628) and from each commit at the same path: `.text`, `.data` and `.pdata`
identical, 0 unexpected differences (pecompare); macrocheck c797628 issue-37: 2,108 calls, 0 differences.

Patella-Modern issue #36 (branch issue-36, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the constant #defines to constexpr: 16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks,
0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe and PatellaTest.exe built from
main (93315c1) and from the branch at the same path: `.text`, `.rdata` and `.data` identical, 0 unexpected differences
(pecompare); macrocheck 93315c1 issue-36: 2,108 calls, 0 differences.

Patella-Modern issue #38 (branch issue-38, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after [[nodiscard]], [[fallthrough]] and [[maybe_unused]] (three commits): 16,848,752 + 67 +
1,134,513 checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release
Patella.exe and PatellaTest.exe built from main (2ee58c9) and from each commit at the same path: `.text` and `.data`
identical, 0 unexpected differences (pecompare); macrocheck 2ee58c9 issue-38: 2,108 calls, 0 differences.

Patella-Modern issue #40 (branch issue-40, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after the <c...> headers (first commit) and the includes of misc-include-cleaner (second commit):
16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe built from
main (e089936), the first commit and the second at the same path: `.text` identical, 0 unexpected differences
(pecompare); the windows-x64 Release Patella.exe and PatellaTest.exe identical after the first commit and, after the
second, different only in where <charconv>'s digit-pair table sits in .rdata (instructions identical after address
normalisation); macrocheck e089936 issue-40: 2,108 calls, 0 differences.

Patella-Modern issue #35 (branch issue-35, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after using instead of typedef (common.h, ConsoleWindow.cpp, surfaceDX9.cpp, SurfaceVulkan.h):
16,848,752 + 67 + 1,134,513 checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the
windows-x64 Release Patella.exe and PatellaTest.exe built from main (c57b39c) and from the branch at the same path:
`.text` identical, 0 unexpected differences (pecompare); macrocheck c57b39c issue-35: 2,108 calls, 0 differences.

Patella-Modern issue #33 (branch issue-33, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after override on every overrider (17 files of src/): 16,848,752 + 67 + 1,134,513 checks and
3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe and
PatellaTest.exe built from main (6a20d32) and from the branch at the same path: `.text` identical, 0 unexpected
differences (pecompare); macrocheck 6a20d32 issue-33: 2,108 calls, 0 differences.

Patella-Modern issue #32 (branch issue-32, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest
4/4, 0 warnings) after NULL to nullptr outside the error macros (45 files of src/): 16,848,752 + 67 + 1,134,513
checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64 Release Patella.exe
and PatellaTest.exe built from main (3249d6b) and from the branch at the same path: `.text` identical, 0 unexpected
differences (pecompare); macrocheck 3249d6b issue-32: 2,108 calls, 0 differences.

Patella-Modern issue #31 (branch issue-31, November 3, 2026): `python -I tools/build.py oracle-x86` (CTest 4/4, 0
warnings) after the graphics device choice and Patella.ini (x64-only branches in Surface.cpp, Control.cpp,
Dialogs.cpp and Dialogs.h; src/settings and the new src/vulkan files outside the oracle's glob): 16,848,752 + 67 +
1,134,513 checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe built from main (a8d6b01) and
from the branch at the same path: 0 unexpected differences (pecompare); macrocheck main issue-31: 2,108 calls, 0
differences.

Patella-Modern issue #20 (branch issue-20, November 3, 2026): `python -I tools/build.py oracle-x86` (CTest, 0
warnings) after the original engines left windows-x64 (x64-only branches in Surface.cpp and Control.cpp): the same
counts as below; the oracle Patella.exe built from main d624a34 and from the branch at the same path: 0 unexpected
differences (pecompare); macrocheck 2,108 calls, 0 differences.

Patella-Modern issue #22 (branch issue-22, November 3, 2026): `python -I tools/build.py oracle-x86` (CTest 4/4, 0
warnings) after the test mode's dump hooks in src/ (statement macros that are nothing outside the test build in
Network.cpp, Communication.cpp and vulkan/SurfaceVulkan.cpp; View functions inside `#if
defined(PATELLA_TEST_MODE)` at the ends of Creatures.cpp, Inventory.cpp, Map.cpp, Status.cpp): 16,848,752 +
67 + 1,134,513 checks and 3,201 vlong checks, 0 failures, as below; the oracle Patella.exe and the windows-x64
Release Patella.exe built from main (d624a34) and from the branch at the same path: 0 unexpected differences
(pecompare); macrocheck main issue-22: 2,108 calls, 0 differences.

Patella-Modern issue #19, second part (branch issue-19 after merging main 193dee3, November 3, 2026): `python -I
tools/build.py oracle-x86` (CTest, 0 warnings) after the Edge copy, the device-filtered masks and engine 2 as the
Vulkan engine in the x64 client: the same counts as below; the oracle Patella.exe built from main 193dee3 and
from the branch at the same path: 0 unexpected differences (pecompare).

Patella-Modern issue #19 (branch issue-19, November 3, 2026): `python -I tools/build.py oracle-x86` (CTest, 0
warnings) after the Vulkan engine (src/vulkan, not in the oracle; Surface.cpp's choice of engine 2 behind macros
that are surfaceDX9.cpp's calls in the oracle): the same counts as below; the oracle Patella.exe built from main
(3655cdf) and from the branch at the same path differ only in the link time stamps and the PDB record (pecompare:
0 unexpected differences).

Patella-Modern issue #24 (branch issue-24, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh`
(CTest, 0 warnings) after the first tests moved to tests/golden as golden-corpus tests: the same counts as below;
and with recording on (`golden_corpus`): 16,848,752 + 67 + 1,134,513 checks, 0 failures, every per-test count as
in the run without it. The x64 golden_runner replays 373,518 of difftest's checks (XTEA 6,000, Characters 4,500,
Colors 20,145, Strings 30,000, FormatNumber 2,000, Buffers 80,652, Skins 263, Regexp 229,958) and sysinctest's
67, 0 failures; not yet ported: difftest's 16,475,234 other checks and communicationtest's 1,134,513.

Patella-Modern issue #21 (branch issue-21, November 3, 2026): `python -I tools/build.py oracle-x86` (CTest, 0
warnings) after the test mode's hooks in src/ (src/TestHooks.h, the system calls themselves outside the test
build): the same counts as below; the oracle Patella.exe is identical to main's apart from the link time
stamps and the PDB record.

Issue #7 (branch issue-7, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` (CTest, 0 warnings)
after the portability changes of src/ (common.h, vlong.h, vlong.cpp, regexp.c): the same counts as below.

Issue #10 (branch issue-10, November 3, 2026): `python -I tools/build.py oracle-x86 --fresh` from Git Bash (no
developer prompt; CTest, 0 warnings) after the scripts were removed: the same counts as below.

Issue #5 (branch issue-5, November 3, 2026): `cmake --workflow --preset oracle-x86 --fresh` (CTest, 0 warnings)
after the x64 fixes of src/: the same counts as below.

Issue #8 (branch issue-8, November 3, 2026): `cmake --workflow --preset oracle-x86` (CTest, from an empty build
directory, 0 warnings) after the LinkPattern change and the reformat of src/: the same counts as below.

Issue #4 (branch issue-4, November 3, 2026): `cmake --workflow --preset oracle-x86` (CTest) and the full build.cmd
give the same counts as below, every per-test count included.

16,848,752 checks against the original + 67 in sysinctest.exe + 1,134,513 in communicationtest.exe, 0 failures;
3,201 vlong checks, 0 failures (main after merging #36 and #37, full build.cmd, November 2, 2026: #37 added
Network 74,928 + 38,878 and MainFrm 31,424 + 18,152 to difftest.exe, every other count unchanged; #36 added
communicationtest.exe. Before: issue #35, branch issue-35, full build.cmd, November 2, 2026: the line numbers of every compared exception
now compared exactly; every count, the error rounds' too, is main's: the line pairs' checks became equality
checks, one per pair as before. Main after merging #34's third part (issue-34c): the second part's 15,258,447 plus the error rounds of
Dialogs 200,285 and DialogElements 1,226,638. The second part (issue-34b): the first part's 14,745,663 plus the error rounds of
ConsoleWindow 287,934, ContentWindows 168,764, MapWindow 12,359 and MainWindow 43,150, statics now 8,496
(NumberOfBuddies, SurfaceInitInstance); GUI 31,909, DialogWindow 9,501, ContextMenus 8,572, SurfaceDX 155,615,
SurfaceDX9 4,934, SurfaceGL 2,620 as before; every per-test count as before; #21 (src/compat/crt71.h) changed
no total. Full build.cmd, November 2, 2026).

## Notes for later files

- Rendering (issue #20): `python -I tools/compare_rendering.py` compares what both clients draw; a change to
  drawing code (GUI, dialogs, map window, the surfaces) can be checked with it (`--engines dx5 --modes windowed
  --no-repeat` takes a few minutes; `--patella build\windows-x64\Release\Patella.exe` compares the x64 client,
  Patella-Modern issue #18). patcap.dll (tools/capture/patcap.c) also makes a client's time fully
  virtual: useful for any comparison that must not depend on timing. Do not inject DLLs with a remote thread on
  this machine: Windows Defender stops the injector; an import entry in the run folder's copy (add_import) works.

- The Bash tool expands `\n` inside heredocs and sed replacements: write
  C/C++ escapes with the Edit/Write tools, not through shell commands.
- tools/callcheck.py now resolves member calls named `Class_method` in
  names.tsv (own class first, else a unique method name).
- Map.cpp (delegate branch): TObject now clears itself (default member
  initializers), TRGBColor has constructors (palette index 0x425950, R/G/B,
  default), vector<T> has its destructor, Container.h has matrix<T> and
  matrix3d<T>. Communication.cpp's map code uses TObject; TCommunicationObject
  is left for the Inventory.cpp calls. The Map/MapWindow gap has names.tsv
  rows (TMapWindow_*) for MapWindow.cpp.
- The difftest image's static initialisers never run: call the original
  constructors of a file's static objects explicitly (maptest.cpp does it
  for Map.cpp's), and keep inputs away from paths that raise exceptions in
  the original (e.g. opening a missing file) unless the test enables the image's exceptions (next note).
- Error paths (issue #22): tools/difftest/imageeh.cpp makes the image's exceptions work in the child
  (EnableImageExceptions ... DisableImageExceptions around the calls); ImageThrow([&] { ... }) and
  OurThrow([&] { ... }) return what escaped as a TThrown, errortest.cpp's Compare compares two of them (by their
  outcome since Patella-Modern issue #76: thrown or not, and the kind; until then the messages, with the line
  numbers exactly since issue #35, mapped before, see notes/decisions.md "Error paths (issue #22)"). With it the image's stringstream works
  (TMyException messages, error.txt); its ofstream needs errortest.cpp's VC7.1 FILE fakes. A fake of a
  function the original compiled as noreturn (FatalError) must not return. An unbound import the image calls
  while it is enabled is named in a "crash:" line. DIFFTEST_ONLY=Errors runs errortest.cpp alone (ER_TRACE=1,
  ER_SEED=<n>, ER_WATCHDOG=<seconds> prints where a hung run is; ER_LINES=1 went with issue #76).
- Original line numbers (issue #35; since Patella-Modern issue #76 no comparison reads them, and M5's issues may
  change them): every TASSERT*/TERROR*/TEND*/TMYEXCEPTION site (and the file-local
  TASSERTB*, TASSERT*_BUG, TERROR1B) takes the original's line as its first argument: TASSERT(66, s!=NULL),
  TEND(1280), TEND1(1280, Type). A new site gets the line the binary pushes for it: `python -I tools/lines.py
  binary` lists every unit (file, line, strings, function addresses; a TEND's two catch blocks are one unit);
  then `python -I tools/lines.py check` must report 0 mismatches. A site the binary has no code for (a TEND
  whose catch blocks the compiler dropped, an assertion it folded or dropped) gets an estimate between its
  neighbours' lines and an entry in lines.py's EXPLAINED with the reason. A new error macro needs an entry in
  lines.py's MACROS (how its arguments make the strings). lines.py needs objdump (mingw64) on PATH.
- Error paths of the window and surface files (issue #34): a test's error round is a TErrorRound
  (imageeh.h) after its counted checks, with its own "<Test> errors: ..." line (the test's own count stays
  unchanged: subtract the round's checks if it runs before the test's print). Test objects throw on demand
  with ThrowSideAssertion (the image's exception on the image side); a fake that a destructor of ours calls
  must not throw (noexcept). Clear what a throw leaves behind before the next call (TDialogWindow's Busy and
  Dragged). DIFFTEST_SKIP=<names> leaves tests out of the full run (find which earlier test leaves state
  behind); DIFFTEST_FAILS=<n> prints n failures in every test. notes/decisions.md "Error paths of the window
  and surface files" lists what is covered and what is not reachable (its "Second part": MapWindow,
  MainWindow, ConsoleWindow, ContentWindows, NumberOfBuddies, Surface.cpp; its "Third part": Dialogs and
  DialogElements).
- Failure injection by call count (issue #34, second part): a window test's Logf calls Tick(), which counts
  one side's fake calls and makes the FailAt-th throw ThrowSideAssertion of the calling side; a getter that
  does not log can tick too. Both sides make the same calls, so the throw lands at the same place. A Tick in
  a getter that was silent before can show a call-order difference (TTradeWindowBody::SetSize). A direct call
  with Fail = 1-3 (ContentCall, ConsoleCall) reaches the TEND blocks the random rounds miss. After a throw
  members may point to deleted windows: skip the dumps, compare the logs, abandon the trees.
- Image-side test objects (issue #34): a test class's object on the image side runs our base code for what
  it does not override, whose TEND catches the image's exception with our layout. imageeh.h's
  ImageSideVtable(our vtable, our base vtable, the image's base vtable) gives it the image's base entries
  (in the error rounds only); an override that must call the base calls the image's on the image side.
- Destructors and exceptions (issue #34): VC7.1 lets exceptions out of destructors, C++11 makes them
  noexcept (terminate). TComponent's destructor is noexcept(false), so the window classes' destructors throw
  as in the binary; a class whose base is MFC's (CMainFrame) cannot (C2694).
- Error rounds of Dialogs.cpp and DialogElements.cpp (issue #34, third part): Tick() in the getter fakes
  too, failure counts per kind and action, Operation(Forced) for the target action (same random numbers, so
  the counted part is unchanged), sweeps (a call failing at each of its fake calls). A wrapper of the test
  that calls a ticking fake must not tick itself. Abandoned objects with timers must leave the timer lists
  (dialogstest.cpp RemoveAbandonedTimers). With the image's exceptions enabled the image's error report
  dialog constructor works (std::stringstream).
- build.cmd (issue #23) gives the same result from cmd, PowerShell and Git Bash. The old silent
  "exit 1" runs were the child dying without a word (0xc0000374 from talktest.cpp's double delete,
  which only some memory layouts showed) under `|| exit /b 1`; the Git Bash runs that failed with
  output were dialogstest.cpp's dump, which took a heap pointer 0x02abab30 for the 0xab fill
  pattern, or a number both sides hold that was readable memory for the second dump only
  (#23 patched both rules; #32 replaced them: the dump classifies words by the round's allocations,
  never by readability or byte patterns). Git Bash's larger environment block moves the heap. A test that is
  suspected of depending on the layout: run it alone (DIFFTEST_ONLY) many times with an environment
  variable of growing length (issue #23 did 30-100 such runs of each window test). From Git Bash run `cmd //c 'tools\difftest\build.cmd'` (`cmd /c` becomes
  `cmd C:/`: an interactive cmd that runs nothing and exits 0). A locked difftest.exe no longer
  breaks the link: the child dies with its parent (job object) and build.cmd stops a copy of this
  checkout that still runs, by its full path. notes/reconstruction.md ("Checking") has the crash
  reports.
- MapWindow.cpp: TObject converts to int (Type), so TEND1(Obj) and
  comparisons like "SrcObj!=0" compile as in the original; an rvalue TObject
  now also matches GetObjectFlag(int, int). Status.cpp's Set/Get*Object take
  (Index, x, y, z). The surface slots +0x20, +0x30, +0x40, +0x4c, +0xa4 have
  names and parameters (Surface.h); a test that needs them defines the
  TSurfaceGL methods (mapwindowtest.cpp), the others stay playertest.cpp's
  stubs.
- MainWindow.cpp creates TMapFrame (MapWindow.h) with operator new(0x40); its
  deleting destructor is folded into TStartWindow's 0x4adc40 (MainWindow.obj).
- The text header's table is copied into every object that includes it; use
  the copy of the object at hand: include src/Text.h in the .cpp (its entries
  are static; Text.h lists the sixteen copies and their objects).
- Status.h defines LinkPattern (static, issue #33): every object that includes Status.h has a copy in
  .data (17 in the image); only Status.cpp reads its own. A .cpp that uses Status.cpp includes Status.h.
- mapwindowtest.cpp's PatchWorld redirects many functions of our Map, Creatures,
  Status, Preferences, Talk, Help, graphics, GUI and Inventory code for good;
  keep it the last test in difftest.cpp or restore them.
- A difftest crash or difference that depends on the environment (TEMP's length, the shell) is
  memory-layout dependent: a heap error (difftest.cpp runs _heapchk() after every test and names
  the test; LFH small-block overruns can still slip through) or a dump that classifies values by
  address.
- MainWindow.cpp: the windows it creates from DialogElements.cpp were sketches in external.h
  (TBorder, TTextButton, TLabel, TObjectIcon); they are the real classes since issue #7.
- mainwindowtest.cpp makes the not-reconstructed windows fakes for both sides: the image's
  constructors jump to fastcall wrappers that placement-new the fake, whose constructor then sets
  the vtable of a test subclass (extra overrides, same layout). Reuse it for the window files; when
  one of those files is reconstructed, its fakes in mainwindowtest.cpp must go (duplicate
  definitions). DIFFTEST_ONLY=MainWindow runs that test alone.
- SkinProperties.Style0D8 is now five TGUIPoints (+0x10-+0x37) and a TRect at +0x38.
- DialogWindow.cpp (issue #3): the content windows of the dialog columns are TContentWindow (base,
  0x4854c0) and TSubWindow (with a body); ContentWindows.cpp's side-bar windows derive from
  TContentWindow and its creators size their windows before Rearrange(16) (MakeRoom calls
  Work->SetSize before Work is a child). Preferences.h's stored size is TContentWindowSize now.
- The shared fakes of TResizer, TSkinButton::SetSkins and the container body's UpdateUpButton
  (FakeUpdateUpButton) are in dialogwindowtest.cpp, reporting through difftest.h's TDialogFakes
  hooks; when DialogElements.cpp is reconstructed, those definitions must go (duplicates) and the
  tests that use them (dialogwindowtest.cpp, mainwindowtest.cpp, contentwindowstest.cpp) need the
  real classes or new fakes.
  DIFFTEST_ONLY=DialogWindow runs the dialog window test alone; it restores every patch it makes.
- tools/callcheck.py resolves `Class::method(` (with TWindow -> TComponent) and does not match
  `x->Name(` to a free function; differences counted in older notes were made without that.
- Building Patella.exe (issue #16): tools\build-patella.cmd [Release|Debug]; run the client only from a
  folder with copies of the data files (build\run), never from the root (it rewrites Tibia.cfg on exit).
  Real MFC hides Win32 functions inside CWnd/CWinApp members (GetClientRect, SetTimer, LoadIcon, ...):
  call them as ::Name(m_hWnd, ...) there. A file that includes SurfaceDX.h (DirectX 6/7 d3d.h) defines
  DIRECT3D_VERSION 0x0700 before its first include (MFC 14 pulls in d3d9types.h); timeGetTime needs
  <mmsystem.h> (MFC defines WIN32_LEAN_AND_MEAN). compile-check uses real MFC; set TIBIA_MFC_SHIM=1 for the
  shim, which the difftest keeps using.
- Python on Windows writes CRLF in text mode: pass newline='\n' when an edit script rewrites a file
  (and write such scripts with the Write tool: the Bash tool expands backslash escapes in them).
- ContextMenus.cpp (issue #4): ContextMenus.h needs Map.h (TMark, TCoordinate); its users include it
  directly (Control.cpp).
  (The console's classes are real since issue #5; contextmenustest.cpp fakes their methods on both
  sides.) When DialogElements.cpp (TEditLine,
  TFormattedText 0x465e60, TSeparator 0x452cb0) or ContentWindows.cpp (TSkillsWindowBody) are
  reconstructed, contextmenustest.cpp's definitions of their methods and constructors must go
  (duplicates), and its fakes must move to the real classes. DIFFTEST_ONLY=ContextMenus runs the test
  alone; it restores every patch it makes.
- Redirecting "our" copy of a function that a test file defines itself can patch the fake into a jump
  to itself: the linker folds a definition that only calls the fake with the fake. Redirect only the
  image's address for such functions (contextmenustest.cpp's Send* and IsClipboardEmpty).
- A menu's Show shrinks the last entry each time it runs; tests must not call it again once the last
  entry would drop below one pixel (both versions fail).
- ConsoleWindow.cpp (issue #5): ConsoleWindow.h needs only GUI.h and Talk.h (the DialogElements classes it
  points to are forward declarations); its users include it. consolewindowtest.cpp defines the shared
  fakes of the DialogElements classes the console creates (TEditLine, TScrollBar/TScrollBarRange,
  TSkinButton's constructor, TToggleButton, TMultiStateButton); they log only while that test runs, and
  contextmenustest.cpp and dialogwindowtest.cpp link them. When DialogElements.cpp is reconstructed, those
  definitions must go (duplicates) and the three tests need the real classes or new fakes.
- mainwindowtest.cpp reaches our TConsoleWindow constructor through its decorated name
  (#pragma comment(linker, "/alternatename:...")): a constructor has no address in C++. The same works for
  any constructor a fake must replace.
- Test files must not define classes (with inline members or vtables) of the same name: the linker keeps one
  COMDAT, and an object of the other file's class gets the wrong vtable (consolewindowtest.cpp's first buffer
  class called contextmenustest.cpp's TTestBuffer methods).
- Talk.cpp's TTalkBuffer::Remove(int) only finds line 0 (the binary never increments its counter); tests that
  remove other lines use Remove(TTalkLine *).
- Our DrawSkin is inlined into TComponent::Draw (GUI.cpp): patching DrawSkin does not reach the drawing lists;
  fake DrawPicturePart instead and let DrawSkin run on both sides (consolewindowtest.cpp).
- tools/callcheck.py splits functions at every comment line that starts with "// 0x"; write continuation lines
  so that they do not.
- tools/triage.py lets header evidence win over file-ranges.tsv (as assertion evidence does).
- ContentWindows.cpp (issue #6): ContentWindows.h needs Map.h (TMark), so its users include it directly
  (MainWindow.cpp, DialogWindow.cpp, ContextMenus.cpp). TSubWindowBody's virtuals are inline in it
  (Minimize, Maximize, IsMinimized, GetMaximumHeight, LeftButtonDoubleClick returning true); a test body
  derived from it must implement GetInitialHeight and GetContentHeight, and a body that MainWindow.cpp's
  OpenContainer gets needs a first new virtual at +0xb8 (the container body's Update).
- The shared DialogElements fakes report every object they make through difftest.h's ElementCreated
  (contentwindowstest.cpp sets it; "@%p" in the arguments is replaced by the object's name). New fakes
  of the members ContentWindows.cpp uses are in contentwindowstest.cpp (TTextButton's second constructor,
  TTextToggleButton, TLabel's area constructor and SetText, TSkinIcon, SetRadioButtonGroup,
  TScrollBarRange::SetValue). mainwindowtest.cpp's and contextmenustest.cpp's fakes log and register only
  while their tests run (Running). When DialogElements.cpp is reconstructed, all of them must go.
- contentwindowstest.cpp makes the image's operator new and ours fill new memory with the same byte
  pattern (members the original leaves uninitialised then agree, also in objects the creators make);
  world values a window sets and reads in the same operation need one copy per side; never call
  Random() inside Both (the sides would diverge), and decide test inputs before Both from our side's
  state. Our printf is faked only while our side runs (CHECK prints with it).
- SkinProperties: Style288 has arrays (Data10[10] points, Data60[10] rects, Data100[10] ints), PlayerStates
  is a block at +0x470, Style4AC/Style4FC are TCombatControlsProperties, Data670/Data67C and
  BuddylistWindow.Data3C/Data48 are colours (issue #6). TSurface's +0x0c, +0x10, +0x18 have names.
- DIFFTEST_ONLY=ContentWindows runs contentwindowstest.cpp alone (CT_TRACE=1 traces rounds and actions,
  CT_LOGS=<file> writes every compared log, for checking the coverage).
- DialogElements.cpp (issue #7): DialogElements.h includes Objects.h (for TObject), Status.h and
  Skins.h; its users include it directly. The window tests' DialogElements fakes are mirror classes of
  the old sketches (tools/difftest/elementfakes.h, TFake*): elementfakes.cpp's PatchElementFakes
  redirects the image's and our constructors (decorated names through /alternatename) and non-virtual
  members to them; each of those tests calls it with its own Redirect. TObjectIcon's and TSeparator's
  constructors and TMultiStateButton::Reset are inline in DialogElements.h (our callers inline them), so
  they run for real on both sides. A new window test that needs element fakes calls PatchElementFakes;
  one that needs real elements (dialogelementstest.cpp) must run while no fake is patched.
- GUI.h's +0x3c/+0x40 are getMinimumWidth/getMinimumHeight (DialogElements.cpp's assertions).
- Surface.h's +0x1c is DrawVerticalLine; SkinProperties.CheckBox.Data08 is a TRect.
- tools/ehinfo.py also reads the prologue PUSH -1; MOV EAX, FS:[0]; PUSH stub.
- Dialogs.cpp (next window file): the DialogElements/Dialogs gap 0x46b790-0x46c0ce is Dialogs.obj's
  leading run (Dialogs.h's TLabel subclasses and other inline constructors and destructors, Container.h's
  vector of a 31-byte element); Dialogs.cpp calls the DialogElements.h inline copies (SetOutfit,
  TSkinButton::SetEnabled, TListBox::GetSelection/SetEditable, TFontListBox, TEditText,
  THorizontalScrollBar with its callback, TLink with OpenURL).
- DIFFTEST_ONLY=DialogElements runs dialogelementstest.cpp alone (DE_TRACE=1, DE_LOGS=<file>).
- Dialogs.cpp (issue #8): Dialogs.h includes DialogElements.h, Map.h and Preferences.h; its users include it
  directly (Control.cpp). The creators return the frame TDialog (a TWindow) for ModalWindow. TDialog's body is
  a TComponent (labels are bodies too). TObject's second member is Data (assertion text), TBorder frames a
  TComponent, TListBox has GetNumberOfLines (inline), TSurface has GetFullscreenModes (+0x8c),
  SetFramerateLimit (+0xac) and the member GetAvailableEngines.
- SkinProperties: build/scratch/dg8/retype.py relayouts blocks from a table (offset, type, name), checks the
  constructor-zeroed bytes and rewrites every use by offset; reuse it for later retypes.
- SetChildArea takes a TRect by value: TWindow::SetChildArea(TComponent *Child, TRect Area) (issue #27; every
  caller builds a 16-byte block). Pass a SkinProperties TRect itself, build TRect(...) for computed areas.
- The image's iostreams cannot run in the test child (the CRT's static initialisers are not run and call
  imports): functions that build a std::stringstream in the image (the error report dialog's constructor,
  Container.h's index errors) cannot be called on the image side, unless the test calls
  EnableImageExceptions (imageeh.h, issue #22), which makes them work.
- dialogstest.cpp's dump prints every member word with pointers named (tree components, receivers, timers)
  and strings only for the members listed as strings. A word is a pointer only by where it points, never by
  readability (issue #32): into a block of the round's allocations (H<size>+<offset>, D when freed; freed
  blocks stay allocated until the round ends), into an image or module (S) or into the stack (K); otherwise
  it is a number. The fill pattern needs no rule (0xabababab is no address). Event 31 only with a timer,
  event 11 of the hotkey dialog only with a selection.
- DIFFTEST_ONLY=Dialogs runs dialogstest.cpp alone (DG_TRACE=1, DG_LOGS=<file>, DG_HEAPSHIFT=<bytes> moves the
  heap layout, DG_BLOCKS=1 counts the blocks left in the table).
- SurfaceDX.cpp (issue #9): the engine uses DirectDraw4/Surface4 and
  Direct3D3/Device3/Viewport3 despite its DirectX 5 name. TSurface is 0x70
  bytes (8-byte alignment), TSurfaceDX is 0x250. Base Window is now hWindow;
  the +0x50/+0x54 color-key, +0x64 attachment and +0x9c recreation slots
  have typed names. Keep the shared +0xac name SetFramerateLimit.
- Shared surface statistics span 0x71d0bc-0x71d0f4; private DirectDraw
  SurfaceMemory starts at 0x71d0f8. CacheSurfaceWidth/Height start at 256,
  InitSurfaces sets them to 320. Do not duplicate these globals for DX9/GL.
  Source ownership is inferred; their common binary use is verified.
- DIFFTEST_ONLY=SurfaceDX runs surfacedxtest.cpp alone (DX_TRACE=1 prints
  phase coverage, DX_LOGS=<file> writes compared logs). All code/IAT patches
  are restored. Never read the uninitialized DDSCAPS2 words after dwCaps.
  The display-mode callback intentionally tests the refresh flag in
  ddpfPixelFormat.dwFlags rather than the descriptor's dwFlags.
- tools/ehinfo.py accepts interleaved argument loads in the SEH prologue
  and validates FuncInfo magic (needed by SurfaceDX's stretch wrappers).
- surfaceDX9.cpp (issue #10): surfaceDX9.h includes the SDK's d3d9.h, which is empty after SurfaceDX.h's
  DirectX 6 d3d.h (DIRECT3D_VERSION 0x700): never include both; Surface.h declares IsDirect3D9Installed and
  CreateSurfaceDX9 for Surface.cpp. TSurfaceDX9's own members start at +0x70 (Ready); its surfaces are
  textures (Surface9[]) whose level 0 is RenderTarget[].
- SurfaceGL.cpp (#11): its object starts at 0x4f46f0 with glh_extensions.h code; six of its functions
  (0x4f9d00, 0x4fa110, 0x4fa2c0, 0x4fab70, 0x4fb5e0, 0x4fb5f0) are also the DX5/DX9 slots, written in those
  files too. The display-mode callback and the frame timer are the engines' identical copies as well.
- surfacedx9test.cpp fakes the Windows functions by writing the fakes into the image's import slots and
  difftest.exe's own (OwnImportSlot), restored at the end; its fake COM objects are generic (a vtable of traps
  with the used slots filled in, MakeVtbl), reusable for SurfaceGL.cpp's test. Fake texture memory must
  be large enough for every write (a too small test texture corrupted the heap once). DIFFTEST_ONLY=SurfaceDX9
  runs it alone (DX9_TRACE=1, DX9_LOGS=<file>, DX9_ROUNDS=<n>).
- TRect (graphics.h) is {x, y, width, height} (issue #29, assertion texts); TSkin (Skins.h) and TDrawing
  (GUI.h) use the same lower-case names, TSkin stays its own struct. Other Width/Height members (TComponent,
  TSurface, TFullscreenMode, TFont, window sizes) keep their capitals.
- The surface blit slots take the source as TRect FromRect (Surface.h, issue #29): BlitRect(ToSurface, x, y,
  FromSurface, FromRect), Blit(..., FromRect, ColorKey), BlitRectClipped(..., FromRect, Clip),
  BlitClipped(..., FromRect, ColorKey, Clip); StretchBlitUnclipped/StretchBlit(ToSurface, TRect ToRect,
  FromSurface, TRect FromRect, ColorKey, Highlight[, Clip]). A new test surface overrides them with these
  signatures (testsurface.h). When a slot's callers build a 16-byte block (`sub esp,0x10` and four stores)
  the parameter is a TRect; build/scratch/slotcalls.py (i29 worktree) lists the argument pattern of every
  slot call.
- Settled in #27: SetChildArea takes TRect Area, the callers pass SkinProperties' TRects themselves.
  build/scratch/areacalls.py (i27 worktree) lists the argument pattern of every +0x8c slot call and of
  every direct call of 0x49f5d0.
- Settled in #31: SkinProperties.Data140, Style194.Data28/Data60/Data70 and Style6FC.Data04/Data34/Data58/
  DataA8/DataB8/DataE8 are TRects (they were pairs of TGUIPoints; each is used only by InitSkinProperties'
  stores and one SetChildArea call). Every SetChildArea caller now passes a SkinProperties TRect or a
  computed TRect(...). build/scratch/i31/refs.py (i31 worktree) lists every absolute reference into a range
  of SkinProperties, for checking a member's other uses before a retype.
- Unknown-owner data (issue #12, notes/decisions.md): every object that includes the text header has
  a full copy of its table (147 entries, 0x24c bytes, "You are poisoned" ... "Report Rule Violation");
  the object whose code reads an entry owns that copy (Player.obj 0x5c22cc, MapWindow.obj 0x5c2064,
  Control.obj 0x5c0b34; all sixteen copies are in src/Text.h since issue #14). The window objects' unused NoCallback sits at the end of their .bss, one
  __xc initialiser per object (Control.obj 0x5c4558, Creatures.obj 0x5cc418). The "engine N" value
  is Surface.cpp's CommandLineEngine (Surface.h).
- SurfaceGL.cpp (#11): the XOR code tables right after its last .rdata string (0x5733a4/0x5733c4) were
  given to sysinc.cpp (low: file-scope data comes first in an object's .rdata); if SurfaceGL.cpp turns
  out to define file-scope data after its functions, move them there.
- SurfaceGL.cpp (issue #11): SurfaceGL.h includes the SDK's GL/gl.h (no conflict with either DirectX header;
  Surface.cpp includes it for new TSurfaceGL and IsOpenGLWorking). NVIDIA's glh_extensions.h and
  glh_genext.h are reconstructed in src/Extern/glh (glh_genext.h generated from the binary by
  build/scratch/gl/genext.py); only SurfaceGL.cpp may define GLH_EXT_SINGLE_FILE. TSurface's +0x04/+0x08/+0x80
  are SelectSurface, SetTextureFilter and AddDirtyRect(TRect) (overridden only by SurfaceGL.cpp), +0x0c is
  DeviceContext and +0x28 ScreenRect. MainFrm.cpp's DisplayChanged (0x5d1670) is global (Init reads it).
- The tests' recording surface is tools/difftest/testsurface.h's TTestSurface (formerly external.h's
  TSurfaceGL sketch): a new window test derives from it; the slots graphicstest.cpp and mapwindowtest.cpp
  define are the recorders, the others abort.
- difftest.exe links SurfaceGL.cpp but not opengl32/glu32: surfacegltest.cpp provides the OpenGL import
  symbols (__imp__glXxx@n via /alternatename to its GLSlot_ variables) and gluOrtho2D. A test can find a
  static of ours (no name outside its file) by making it take a sentinel value and searching difftest.exe's
  writable sections (surfacegltest.cpp's FindWord/FindChange). DIFFTEST_ONLY=SurfaceGL runs it alone
  (GL_TRACE=1, GL_LOGS=<file>, GL_ROUNDS=<n>); it has its own random generator.
- Container.h (issue #13): every instance of the templates in the image is listed at the top of the
  header; a new user of vector/array/matrix gets the existing template, and its instance's address goes
  into that list (tools/triage.py counts a header function as written when src/ names its address).
  containertest.cpp (DIFFTEST_ONLY=Container) calls every image instance next to ours with both sides'
  operator new[] filling the same pattern; add a line to its Run() for a new instance. Mutating
  Container.h needs a full rebuild of difftest.exe (every object instantiates the templates and the
  linker keeps one COMDAT): build/scratch/mutate13.py's way.
- A temporary coverage build (issue #13, build/scratch/cover in the i13 worktree): INT3 on the first
  byte of the image's and our functions (ours from the link map), the first hit per test counted, re-armed
  at the next test. Hitting a breakpoint on every call makes the suite far too slow.
- Library region (issue #15): names.tsv has a row for each of the 1,102 functions from 0x50eae8 (kind stl, the
  library first in the note: "CRT: MSVC 7.1 libcmt.lib", "MFC 7.1: nafxcw.lib", "C++ library: MSVC 7.1 libcpmt.lib",
  "ATL 7.1 header code in MFC 7.1", import/delay-load thunks, "third-party: Henry Spencer regexp"); file-ranges.tsv
  has the blocks and tools/triage.py writes them into symbols.tsv's file column (a block that names a game file
  makes its rows kind game: Inventory.cpp's initialiser 0x559ea0). The CRT names drop one leading underscore of
  Ghidra's (memmove, _lock); 0x51262a is atoi (the sources' atol calls: #30).
- tools/callcheck.py counts a library-region name only when Ghidra left the function unnamed and the file's
  pseudocode calls it by that name (Ghidra's own names such as _memcpy stay in the pseudocode).
- src/Extern/regexp is Henry Spencer's regexp (C, K&R): include "Extern/regexp/regexp.h" (extern "C"
  prototypes). regerrorhandler (the image's 0x71f034) is NULL in the client; regexptest.cpp sets both sides'.
  DIFFTEST_ONLY=Regexp runs regexptest.cpp alone (RX_SEED, RX_ROUNDS, RX_TRACE=1 prints the coverage);
  it has its own random numbers and restores the image's hook. compile-check also compiles src/Extern/regexp/*.c.
- Issue #24 (review of the low-confidence decisions, notes/decisions.md's last section): static data and
  their initialisers are checked by comparing our compiled dynamic initialisers (??__E) and atexit
  destructors (??__F) per object (tools\listing.cmd) with the binary's __xc table 0x5c002c-0x5c00c4
  (tools/xctable.py); they agree now. A new static object with a constructor or destructor must
  match an entry there. TRSA.h has a header static (HeaderExponent): every includer constructs one.
- Eliminated try blocks: tools\listing.cmd File.cpp, then python -I tools/ehcompare.py Name compares our
  unwind states and try blocks with tools/ehinfo.py's; a catch whose try body
  only calls C functions is removed by our compiler too, so a guessed catch body is never emitted (difftest
  builds with /EHa, where it stays).
- DIFFTEST_ONLY=Script runs scripttest.cpp alone; its TestLongTokens has its own random generator.
- Library behaviour (issue #21): Patella links the UCRT, Tibia.exe MSVC 7.1's libcmt; where the two CRTs format
  differently and the result reaches a file or a packet, the call site converts to MSVC 7.1's form
  (src/compat/crt71.h, CRT71Time for ctime's two-digit day). The difftest replaces the image's CRT functions
  (ctime, localtime, ...) with the test's, so it cannot see such differences; tools/compare_files.py runs the
  real executables. Look for more when a session writes or sends text the CRT formats (asctime, strftime,
  printf's %g/%e, ...).
- Network.cpp and MainFrm.cpp (issue #37): networktest.cpp and mainfrmtest.cpp (tests/golden/mainfrm_tests.cpp since
  Patella-Modern issue #54) `#include` the sources (build.cmd
  links neither file itself any more), so a test reaches their statics by name; DIFFTEST_ONLY=Network (NW_TRACE=1,
  2 prints every fake call, NW_LOGS=<file>, NW_ROUNDS=<n>) and DIFFTEST_ONLY=MainFrm (MF_TRACE, MF_LOGS, MF_ROUNDS).
  They fake MFC at the calls into it (the image's MFC functions jump to the fakes; the shim's declarations are the
  tests' definitions): a new test that needs CWnd/CFrameWnd/CAsyncSocket members gets them from there.
  playertest.cpp's stubs call StubCalled(__FUNCTION__) now: identical abort() bodies were folded by the linker into
  one function, so redirecting one stub redirected all of them. A test that makes a user32 fake throw must
  remember that /EHa keeps our handlers where /EHsc (both real builds) drops them (try blocks that call only C
  functions: MainFrm.cpp's AddFrameSize and ExitProgram).
- tools/compare_network.py (issue #37, notes/network.md): crafted and fuzzed server streams for both clients
  through tools/proxy.py's replay, whose s2c events can now carry frame_hex, xtea_key, inner, hold, pieces
  (piece_delay) and cut, and Proxy(refuse=("login",)) leaves a port closed. A new scenario is an entry in its
  scenarios(); --only, --list, --repeat (Tibia.exe again where the clients differ).
- tools/compare_files.py (issue #21) reuses compare_traffic.py's set-up through set_folder(); its replay()
  runs any session(driver, wait, proxy) against recorded or crafted server packets (connection(),
  s2c(), c2s_expected(), close_event() build a script). tools/client_state.py names more Tibia.exe vtables
  (option and mark dialogs, TCheckBox, TAutomapView/Window, TTab, TStartWindow) and reads TTextButton's text.
- Communication.cpp and Control.cpp (issue #36): tools\difftest\build.cmd builds communicationtest.exe (difftest.cpp
  with DIFFTEST_COMMUNICATION, communicationtest.cpp, controltest.cpp, commworld.cpp, imageeh.cpp and every src file but
  MainFrm.cpp and Player.cpp); DIFFTEST_ONLY=Communication or Control runs one test. commworld.cpp fakes every callee
  on both sides (a fake logs into the log of Side and answers from that side's world); Network.cpp's buffer
  functions stay real on buffers the test sets (ReceiveBufferOf/SendBufferOf, SetReceiveData), SendCommand logs the
  packet. CM_TRACE/CT_TRACE=1 (=2 names each comparison before it runs), CM_LOGS/CT_LOGS=<file>, CM_ROUNDS/CT_ROUNDS,
  CM_SEED/CT_SEED, CM_LIVE=<text> prints the calls of the comparisons whose name contains it as they happen,
  CM_HEAPCHK=1 runs _heapchk() after every comparison. The seeds (commseeds.h) come from issue #19's recordings
  through commseeds.py. When Network.cpp gets linked into difftest.exe (issue #37), communicationtest.exe is not
  affected; when Communication.cpp or Control.cpp change, both tests must still pass, and a new callee needs a fake
  in commworld.cpp (an unfaked callee runs real code and usually crashes the image side).
- A test whose fakes stand in for a whole layer can reach the original's error paths through the fakes: a fake that
  throws the side's own exception on its n-th call (commworld.cpp Tick/FailAt; controltest.cpp also a structured
  exception and the image's PROGRAM_ERROR through _CxxThrowException 0x511d2b with ThrowInfo 0x57b3c4, a const char *
  with 0x5bb5d0). Fill the stack with a pattern before each side's call (commworld.h FillStack) so locals the catch
  blocks print before they are assigned compare equal, and locals the original zero-initialises show up.
- VC7.1 overload order in vtables: MSVC places overloaded virtual functions in reverse order of declaration (both
  compilers); a class with an overloaded virtual must declare them in the reverse of the binary's slot order (Talk.h
  TTalkBuffer::Remove, issue #36; it was the only pair in src/).
- Communication.cpp keeps the original's heap overflow: ReceivePatch and ReceiveWholeFile store the new size before
  they assert that no transfer is running; a later block writes past the old buffer. Any test that feeds patch
  messages must not send a block after a second start (communicationtest.cpp Mutate).
