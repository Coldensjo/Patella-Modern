# The test mode: virtual clock and scripted input (Patella-Modern issue #21)

The first part of the client's built-in test mode (notes/modernization-plan.md, 4.4; M2): one clock behind
timeGetTime, the wall clock and the frame timer, and a script of keyboard, mouse and text events at virtual
times. It is compiled only into the test build, PatellaTest.exe; Patella.exe and the oracle-x86 build contain
none of it. The state dump, the packet log and the frame dump (issue #22) and the session scripts and
reference recordings (issue #23) come later and build on it. Choices and their reasons: notes/decisions.md,
"Test mode: virtual clock and scripted input (Patella-Modern issue #21)".

## Running it

```
python -I tools/build.py windows-x64            # builds Patella.exe and PatellaTest.exe (and the unit tests)
build\windows-x64\Release\PatellaTest.exe --test-script <file> [--test-log <file>] [the client's own words]
python -I tools/testmode_check.py               # the determinism check against the test server
```

Run PatellaTest.exe from a run folder (copies of the data files, a patched client: notes/testserver.md), never
from the root. Without `--test-script` it is the client on the system clock, through the same hooks. With a
script it plays the script on the virtual clock and drops the keyboard and mouse input that comes from Windows,
so the desktop's own mouse and keyboard cannot reach it (the limit of the GUI comparisons in
notes/reconstruction.md, "Test server"); its window is still a real window. `--test-log` lists every event it
dispatched with its virtual time and script line. A script that cannot be read or does not parse ends the
program before its window opens: exit code 3, the message (`testmode: <file>:<line>: <error>`) on stderr, in the
log and through OutputDebugString, never a dialog. The client ignores the four words (its command line parser
skips words it does not know).

`tools/testmode_check.py` runs PatellaTest.exe twice with `tests/testmode/login-walk.script` against the test
server through tools/proxy.py (each run: player files restored, server started fresh, run folder
build\testmode\testmode emptied, compare_traffic.py's Tibia.cfg; the server stopped afterwards) and compares the
runs: (a) the client packets as compare_traffic.py compares them (the first packets' XTEA key and RSA padding
masked, the payloads decrypted), the issue's criterion; (b) the raw client frames, encrypted, keys and padding
included, which a fixed seed makes equal too; (c) the driver's logs; (d) the Tibia.cfg written on exit. It also
lists whether the session reached the game (game login, walks, logout) and the server packets. Exit code 0 when
a-d show no difference. `--runs N`, `--exe`, `--script`, `--out` (default build\testmode\result),
`--timeout` (seconds per run, default 180), `--compare-only` (the recordings in --out again). The test server
must be stopped; the script starts and stops it. About 25 s of script per run at real-time pace, plus the
server's start and stop: two runs take two to three minutes.

## The pieces

| File | What it does |
| :--- | :--- |
| src/TestHooks.h | the hooks in the client: the system calls themselves without PATELLA_TEST_MODE |
| src/testmode/Clock.h, Clock.cpp | IClock and TVirtualClock (portable) |
| src/testmode/InputScript.h, InputScript.cpp | the script: parser, key table, scheduler (portable) |
| src/testmode/TestModeWin32.cpp | the Windows side: TSystemClock, the command line, the driver, the messages |
| cmake/TestMode.cmake | patella_testmode, the portable part, on every product preset |
| cmake/WindowsX64.cmake | PatellaTest.exe (option PATELLA_TEST_BUILD, default ON) next to Patella.exe |
| tests/unit/testmode_*.cpp | tests_testmode: the clock, the parser, the scheduler, the driver loop |
| tests/testmode/login-walk.script | the determinism check's script |
| tools/testmode_check.py | the determinism check |

### The hooks (src/TestHooks.h)

| Hook | Replaces | Where |
| :--- | :--- | :--- |
| PATELLA_TIME_GET_TIME() | timeGetTime() | sysinc.cpp (StartTime, GetMilliseconds, srand), the three engines' ResetFrameTimer and WaitForNextFrame |
| PATELLA_TIME32(Time) | _time32(Time) | Talk.cpp (talk lines' stamps), Creatures.cpp (skull times), ContentWindows.cpp, ConsoleWindow.cpp (saved channels) |
| PATELLA_TIME(Time) | time(Time) | Player.cpp (error.txt) |
| PATELLA_FRAME_SLEEP(ms) | Sleep(ms) | WaitForNextFrame of the three engines |
| PATELLA_GET_CURSOR_POS(Point) | GetCursorPos(Point) | Player.cpp (InitInstance, OnIdle), MainFrm.cpp (OnSetCursor, OnCaptureChanged) |
| PATELLA_SET_MAIN_TIMER(...) | ::SetTimer(...) | MainFrm.cpp OnCreate (the 100 ms timer of ActionNumber) |
| PATELLA_TEST_IDLE | nothing (a statement macro without a semicolon, like TBEGIN) | the start of CPlayerApp::OnIdle |

In the test build CPlayerApp also overrides PreTranslateMessage (Player.h/.cpp, `#if
defined(PATELLA_TEST_MODE)`): with a script it drops WM_KEYFIRST-WM_KEYLAST, WM_MOUSEFIRST-WM_MOUSELAST and the
main timer's WM_TIMER from the queue. Every other message (window management, the host lookup's WM_USER+1,
WM_CLOSE) goes through. The other Sleep calls (the engines' surface waits, Redraw's Sleep(10) while the surface
is lost) stay real.

### The clock (src/testmode/Clock.h)

IClock: `Milliseconds()` (timeGetTime), `WallSeconds()` (seconds since 1970, UTC), `FrameWait(ms)`.
TSystemClock (TestModeWin32.cpp): timeGetTime, _time64, Sleep. TVirtualClock: timeGetTime 100000 + the elapsed
virtual milliseconds, the wall clock 2026-11-02 12:00:00 UTC (1793620800) + the elapsed whole seconds: the start
of tools/capture/patcap.c's clock, so srand(timeGetTime()) gets 100000 in every run and the client makes the
same XTEA keys and RSA padding as in the rendering comparison's runs. The time moves only by AdvanceTo (never
backwards); FrameWait does not move it and sleeps a real millisecond (so that the idle loop, which redraws
until the driver moves the clock, does not spin). The wall clock goes through the C library's local time for
the talk stamps and error.txt (`_localtime32`, `ctime`), so the stamps follow the machine's time zone.

### The script (src/testmode/InputScript.h)

One command per line; `#` starts a comment. The time starts at 0 and moves only with `wait <ms>` and `at <ms>`;
every command happens at the current time.

| Command | Events |
| :--- | :--- |
| `pace <ms>` | real milliseconds per 100 virtual ms (default 100, real time; 0 as fast as the client runs); before the first event |
| `wait <ms>`, `at <ms>` | the time moves on by ms / to ms (not backwards) |
| `key <combo>` | e.g. `Return`, `Up`, `Ctrl+L`, `Shift+Ctrl+Down`: the modifiers down, the key down, its character (as TranslateMessage makes it for a US layout), the key up, the modifiers up |
| `keydown <key>`, `keyup <key>` | one key; a held Shift, Ctrl or Alt counts for the commands after it |
| `char <code>` | one WM_CHAR (0-255) |
| `text "<string>"` | one WM_CHAR per character (as tools/login_client.py types): Latin-1, the file UTF-8, escapes `\"`, `\\`, `\xHH` |
| `move <x> <y>` | the mouse to a point of the client area |
| `click`, `rclick <x> <y> [<mods>]` | move, button down, up; `<mods>` (`Shift`, `Ctrl+Shift`) held around it |
| `dclick`, `rdclick <x> <y> [<mods>]` | move, down, up, double click, up |
| `ldown`, `lup`, `rdown`, `rup <x> <y> [<mods>]` | move, one button event |
| `wheel <notches>` | at the mouse; positive turns it up |
| `quit` | WM_CLOSE (posted) |

A coordinate is pixels from the left or top of the client area, or `R-<n>` / `B-<n>` from its right or bottom
edge, resolved when the event is delivered (tools/login_client.py's Enter Game is `click 113 B-220`). Keys:
A-Z, 0-9, F1-F12, Up, Down, Left, Right, Home, End, PageUp, PageDown, Insert, Delete (extended, as on the block
above the arrows), NumHome, NumEnd, NumPageUp, NumPageDown (the keypad's, not extended: TranslateKey tells them
apart), Return/Enter, Escape, Tab, Back/Backspace, Space, Shift, Ctrl/Control, Alt; caseless. The characters
`key` makes: letters (Shift: capitals; Ctrl: 1-26), digits (Shift: `)!@#$%^&*(`), Space, Return 13 (Ctrl: 10),
Escape 27, Tab 9, Back 8 (Ctrl: 127); with Alt none (the client has no WM_SYSCHAR handler). The arrows and
the function keys make none.

### The driver (src/testmode/TestModeWin32.cpp)

MFC's message loop calls CPlayerApp::OnIdle whenever the queue is empty, and the client's OnIdle always asks
for more, so the driver at its start runs between every two passes of the client's idle work (the mouse poll,
ProcessEvents, Redraw). Each call does one thing:

1. the next event of the script that is due at the virtual time, if any: dispatched to the main frame as the
   window message Windows would send (DispatchMessageA, as the message loop does): WM_KEYDOWN/WM_KEYUP, or
   WM_SYSKEYDOWN/WM_SYSKEYUP with Alt held and for Alt and F10, with repeat count 1, the scan code, the
   extended flag, the context code and the transition bits in lParam; WM_CHAR; WM_LBUTTONDOWN/UP/DBLCLK and the
   right button's with MK_ flags and the client point; WM_MOUSEWHEEL with 120 per notch and the screen point;
   a move only sets the virtual cursor, which GetCursorPos returns and OnIdle polls right after. One event per
   call, so that the client handles each one (and sees a move) before the next, as with input at human speed;
2. otherwise, once the real time for it has come (the pace), the clock moves to the next stop: the next
   multiple of 100 virtual ms or the next event, whichever comes first; at a multiple of 100 the main timer's
   tick (WM_TIMER 1: OnTimer, TimerEvent, ActionNumber) is dispatched.

So over a run every event, tick and clock value comes in the same order at the same virtual time;
tests_testmode simulates the loop on the virtual clock and checks exactly that. After the last event the clock
goes on at the pace; the client runs until it exits (the script's `quit`).

What still depends on real time with a live server: when the server's packets arrive, relative to the virtual
clock. The pace gives the server real time to answer (at 100, as much as the script waits). The issue's check
(the client's packets) holds when the server answers within the script's waits; the server-initiated ping (the
client answers 0x1e) would differ in number if a run took another real time (testmode_check.py lists the
pings). Issue #23 removes this: the client replays the server side of a recording, in virtual time.

### What issues #22 and #23 add

- #22 (state dump, packet log, frame dump): more script commands (the parser's table in InputScript.cpp; for
  example a dump at a step) and writers in the Windows side; the driver's log already names every event's
  virtual time and line, the anchor for dumps. The frame dump's source is the Vulkan engine's read-back
  (issue #19, notes/vulkan.md): `TSurfaceVulkan::ReadScreen` gives the canvas's presented rectangle as the next
  frame shows it (D3DCOLOR, rows top-down), on the GPU or on the CPU driver, with no window system involved
  (`patella::vulkan::TRenderer::ReadBack` underneath).
- #23 (session scripts, reference recordings): the sessions of compare_traffic.py and compare_files.py as
  scripts in tests/testmode; the server side of a recording delivered by the driver at virtual times (a second
  event source for TScheduler::NextStop), with `pace 0`.
