# The test mode: virtual clock, scripted input, the dumps and the reference recordings (Patella-Modern issues #21, #22, #23)

The client's built-in test mode (notes/modernization-plan.md, 4.4; M2): one clock behind timeGetTime, the wall
clock and the frame timer, and a script of keyboard, mouse and text events at virtual times (issue #21); the
state dump, the packet log and the frame dump (issue #22, "The dumps" below). It is compiled only into the test
build, PatellaTest.exe; Patella.exe and the oracle-x86 build contain none of it. The reference recordings,
the server replay and the record/compare split of the comparison tools (issue #23): "Reference recordings"
below. Choices and their reasons: notes/decisions.md,
"Test mode: virtual clock and scripted input (Patella-Modern issue #21)" and "Test mode: the dumps
(Patella-Modern issue #22)".

## Running it

```
python -I tools/build.py windows-x64            # builds Patella.exe and PatellaTest.exe (and the unit tests)
build\windows-x64\Release\PatellaTest.exe --test-script <file> [--test-log <file>] [--test-out <folder>]
                                          [--test-sync <name>] [--test-replay <file>]
                                          [--test-display-modes <file>] [the client's own words]
python -I tools/testmode_check.py               # the determinism and dump check against the test server
python -I tools/testmode_check.py --self-test   # the check's comparisons on made-up data (no server, no client)
```

Run PatellaTest.exe from a run folder (copies of the data files, a patched client: notes/testserver.md), never
from the root. Without `--test-script` it is the client on the system clock, through the same hooks. With a
script it plays the script on the virtual clock and drops the keyboard and mouse input that comes from Windows,
so the desktop's own mouse and keyboard cannot reach it (the limit of the GUI comparisons in
notes/reconstruction.md, "Test server"); its window is still a real window, on the desktop of the session it is started in (a second session keeps
it apart from the user's: notes/reconstruction.md, "Checking", "The live checks in a second session"). On a hidden
desktop (the tools' experimental `--hidden`, tools/desktop.py, issue #56) it runs only on a GPU, not on the CPU
driver: its first frame comes before the window is shown, and lavapipe's present into a window not yet shown fails
there (notes/decisions.md, "The live checks on a hidden desktop"). `--test-log` lists every event it
dispatched with its virtual time and script line. A script that cannot be read or does not parse ends the
program before its window opens: exit code 3, the message (`testmode: <file>:<line>: <error>`) on stderr, in the
log and through OutputDebugString, never a dialog. The client ignores the test mode's words (its command line
parser skips words it does not know). `--test-out <folder>` writes the dumps there (below); a script with
`dump` or `frame` needs it (exit code 3 otherwise). `--test-sync <name>` is testmode_check.py's handshake.

`tools/testmode_check.py` runs PatellaTest.exe twice with `tests/testmode/login-walk.script` against the test
server through tools/proxy.py (each run: player files restored, server started fresh, run folder
build\testmode\testmode emptied, compare_traffic.py's Tibia.cfg; the server stopped afterwards) and compares the
runs: (a) the client packets as compare_traffic.py compares them (the first packets' XTEA key and RSA padding
masked, the payloads decrypted), the issue's criterion; (b) the raw client frames, encrypted, keys and padding
included, which a fixed seed makes equal too; (c) the driver's logs; (d) the Tibia.cfg written on exit. In
every run it also checks the dumps (issue #22; "The check of the dumps" below): (e) each state dump against
client_state.py's reading of the same moment, (f) the packet log against the proxy's decrypted recording, (g)
the frame dumps, (h) the dump's keys against client_state.py's. It also lists whether the session reached the
game (game login, walks, logout), the server packets, and which state dumps and frames the runs share. Exit code
0 when a-h show no difference. `--runs N`, `--exe`, `--script`, `--out` (default build\testmode\result),
`--timeout` (seconds per run, default 180), `--vulkan` (the client's PATELLA_VULKAN, default `cpu`: the pinned
CPU driver, copied into the run folder), `--compare-only` (the recordings in --out again), `--self-test`. The
test server must be stopped; the script starts and stops it. About 25 s of script per run at real-time pace,
plus the server's start and stop and the six readings of client_state.py (about a second each): two runs take
two to four minutes.

### The cold start (issue #25)

The test server listens on both ports (where `testserver.py start` returns) before it has loaded the world. In
the first live run of issue #21 the server's start took 21 s (a cold start: its files not in the disk cache),
and the game server never answered the client's game login (two attempts, no reply); that run never reached the
game, the next run and a full rerun passed (build\logs\issue-21 of the orchestrator's checkout). So each run of
testmode_check.py now:

- waits, after the start, until build\testserver\server.log says `Server Online!` (the server's line once the
  world is loaded), at most `--online-timeout` seconds (default 180; the server's exit or the timeout end the
  check with an error). When the log does not have the server's early line `Starting Login Server` at that
  point, its output does not reach the file as it is written, and the wait is skipped with a line in the log;
- is run once more (the server started again, the player files restored) when the client sent a game login and
  no game connection got a single server packet; the first attempt's files stay in --out as
  `run<N>-attempt1.jsonl`, `.log`, `.Tibia.cfg` and `run<N>-attempt1\`, the log names it, and only the last
  attempt is compared. A second unanswered attempt is compared as it is (and fails: no game).

`--self-test` checks both without a server or a client: the wait on a made-up server.log that grows with a
made-up clock (online at once, loading then online, never online, the server gone, a log not written as it
goes, no log), the unanswered login on recordings shaped like issue #21's two runs, and the retry with a made-up
run (one attempt, a retry with the first attempt kept, two unanswered attempts).

## The pieces

| File | What it does |
| :--- | :--- |
| src/TestHooks.h | the hooks in the client: the system calls themselves without PATELLA_TEST_MODE |
| src/testmode/Clock.h, Clock.cpp | IClock and TVirtualClock (portable) |
| src/testmode/InputScript.h, InputScript.cpp | the script: parser, key table, scheduler (portable) |
| src/testmode/StateDump.h, StateDump.cpp | the state dump: its data and JSON form, the key lists (portable; #22) |
| src/testmode/PacketLog.h, PacketLog.cpp | the packet log: the line format, the connections (portable; #22) |
| src/testmode/Png.h, Png.cpp | the frame dump's PNG pictures: CRC-32, Adler-32, stored deflate (portable; #22) |
| src/testmode/ServerReplay.h, ServerReplay.cpp | the server replay: its file, its schedule, XTEA frames (portable; #23) |
| src/testmode/TestModeWin32.cpp | the Windows side: TSystemClock, the command line, the driver, the messages, the dumps' files, the replay's sockets (#23) |
| src/testmode/ClientStateWin32.cpp, ClientView.h | the state dump's reading of the client (test build only; #22) |
| cmake/TestMode.cmake | patella_testmode, the portable part, on every product preset |
| cmake/WindowsX64.cmake | PatellaTest.exe (option PATELLA_TEST_BUILD, default ON) next to Patella.exe; PatellaTest_compile, its sources compiled without Tibia.exe (PATELLA_CLIENT_COMPILE_ONLY, the hosted CI; #25; Patella_compile the same for Patella.exe, #46) |
| tests/unit/testmode_*.cpp | tests_testmode: the clock, the parser, the scheduler, the driver loop; the dumps' formats |
| tests/testmode/login-walk.script | the check's script (6 dumps, 3 frames) |
| tools/testmode_check.py | the determinism check and the check of the dumps |
| tools/testmode_dumps.py | the dumps' readers and comparisons, the static key lists, the self-test |
| tests/unit/testmode_replay_test.cpp | the reference scripts' commands, the replay's file, schedule and frames (#23) |
| tools/reference.py | the reference recordings: record (through reference_record.py) and compare (#23) |
| tools/reference_record.py | the record step: live recordings, Tibia.exe's reference runs under patcap.dll (#23) |

### The hooks (src/TestHooks.h)

| Hook | Replaces | Where |
| :--- | :--- | :--- |
| PATELLA_TIME_GET_TIME() | timeGetTime() | sysinc.cpp (StartTime, GetMilliseconds, srand), the three engines' ResetFrameTimer and WaitForNextFrame |
| PATELLA_TIME(Time) | time(Time) | Player.cpp (error.txt), Talk.cpp (talk lines' stamps), Creatures.cpp (skull times), ContentWindows.cpp, ConsoleWindow.cpp (saved channels); 64 bits (issue #62: PATELLA_TIME32 and _time32 went) |
| PATELLA_FRAME_SLEEP(ms) | Sleep(ms) | WaitForNextFrame of the three engines |
| PATELLA_GET_CURSOR_POS(Point) | GetCursorPos(Point) | Player.cpp (InitInstance, OnIdle), MainFrm.cpp (OnSetCursor, OnCaptureChanged) |
| PATELLA_SET_MAIN_TIMER(...) | ::SetTimer(...) | MainFrm.cpp OnCreate (the 100 ms timer of ActionNumber) |
| PATELLA_TEST_IDLE | nothing (a statement macro without a semicolon, like TBEGIN) | the start of CPlayerApp::OnIdle |
| PATELLA_TEST_RSA_BLOCK(Block) | nothing (a statement macro, #22) | Communication.cpp SendLogin, SendGameLogin: the RSA block before RSA.Encrypt |
| PATELLA_TEST_PACKET_SENT(Data, Size, Padded) | nothing (#22) | Network.cpp SendCommand: the packet complete, before XTEA (or before the unencrypted send) |
| PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize) | nothing (#22) | Network.cpp PrepareNextPacket: the next packet decrypted |
| PATELLA_TEST_FRAME_OBSERVERS(Patcap, ...) | nothing (#22) | vulkan/SurfaceVulkan.cpp InitDisplay: without patcap.dll, the frame dump observes the engine |
| PATELLA_MESSAGE_BOX(Window, Text, Caption, Type) | MessageBoxA (#23) | Player.cpp FatalError and the start's two boxes: with a script not shown ("Message boxes") |
| PATELLA_TEST_CAPTURE_CHANGED | nothing (a statement macro, Patella-Modern #52) | MainFrm.cpp OnCaptureChanged, before its body: with a script skipped (below) |
| PATELLA_ENUM_DISPLAY_SETTINGS(Device, Index, Mode) | EnumDisplaySettingsA (Patella-Modern #48) | vulkan/SurfaceVulkan.cpp GetFullscreenModes (the Graphics Options dialog's list): with `--test-display-modes` the file's modes ("The display modes") |

The state dump also reads the client's file-local data through four View functions at the ends of
Creatures.cpp, Inventory.cpp, Map.cpp and Status.cpp (src/testmode/ClientView.h), inside `#if
defined(PATELLA_TEST_MODE)`: they hand out const pointers and write nothing; at the ends of the files, so no
line of the product's code moves.

In the test build CPlayerApp also overrides PreTranslateMessage (Player.h/.cpp, `#if
defined(PATELLA_TEST_MODE)`): with a script it drops WM_KEYFIRST-WM_KEYLAST, WM_MOUSEFIRST-WM_MOUSELAST and the
main timer's WM_TIMER from the queue. Every other message (window management, the host lookup's WM_USER+1,
WM_CLOSE) goes through. The other Sleep calls (the engines' surface waits, Redraw's Sleep(10) while the surface
is lost) stay real.

The desktop also reaches the client through the mouse capture, which is not a queued message: when another
window becomes active (or the user clicks elsewhere) while the main window holds the capture, Windows sends it
WM_CAPTURECHANGED, and CMainFrame::OnCaptureChanged releases the held buttons at the cursor, which a script's
`cursor` puts anywhere. A reference compare lost the click on "Graphics" that way (Patella-Modern issue #52,
November 4, 2026: the state dumps 3 and 4 still on the Options dialog, every other one equal). So with a script
PATELLA_TEST_CAPTURE_CHANGED skips the handler (TestModeWin32.cpp IgnoreCaptureChange): the buttons are pressed
and released only by the script's events; the client's own ReleaseCapture comes after it has cleared its button
flags, when the handler does nothing anyway. Without a script nothing changes.

### The clock (src/testmode/Clock.h)

IClock: `Milliseconds()` (timeGetTime), `WallSeconds()` (seconds since 1970, UTC), `FrameWait(ms)`.
TSystemClock (TestModeWin32.cpp): timeGetTime, _time64 (std::time), Sleep. TVirtualClock: timeGetTime 100000 + the elapsed
virtual milliseconds, the wall clock 2026-11-02 12:00:00 UTC (1793620800) + the elapsed whole seconds: the start
of tools/capture/patcap.c's clock, so srand(timeGetTime()) gets 100000 in every run and the client makes the
same XTEA keys and RSA padding as in the rendering comparison's runs. The time moves only by AdvanceTo (never
backwards); FrameWait does not move it and sleeps a real millisecond (so that the idle loop, which redraws
until the driver moves the clock, does not spin). The wall clock goes through the C library's local time for
the talk stamps, error.txt and the saved channels (compat/crt71.h's CRT71LocalTime, issues #61 and #62:
`_localtime64_s` on Windows, `localtime_r` on Linux), so the stamps follow the machine's time zone or TZ.

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
| `dump` | the state dump (#22): state-<NNN>.json, after every earlier event was handled |
| `frame` | the frame dump (#22): the next frame the Vulkan engine shows, frame-<NNN>.png and .filtered.png |

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
pings). Issue #23 removes this: the client replays the server side of a recording, in virtual time ("The server
replay").

## The dumps (issue #22)

With `--test-out <folder>` the test build writes into that folder (created if needed):

| File | When | What |
| :--- | :--- | :--- |
| packets.log | always (with or without a script) | every packet the client sends, before encryption, and every packet it decrypts |
| state-<NNN>.json | at each `dump` of the script (NNN from 001) | client_state.py's state() and gui(), read in the process |
| frame-<NNN>.png | at each `frame`: the next frame the Vulkan engine shows | the frame, 8-bit RGB |
| frame-<NNN>.filtered.png | with the frame | its device-filtered pixels (src/vulkan/FilterMask.h), white; black elsewhere |
| frames.txt | a line per frame | `<step> <requested ms> <script line> <shown ms> <width> <height> <filtered pixels>` |

None of it changes what the client does: the state dump only reads the client's data (no function of the
client is called), the packet log copies the bytes the client is about to encrypt or has just decrypted, the
frame observer only reads the canvas back (as patcap.dll's does in compare_rendering.py). `dump` and `frame` are
events of the script at their virtual time, so they never move the virtual clock: a `dump` is done in the idle
call that delivers the next event (or moves the clock), so with or without dumps the client gets the same
events in the same idle calls; a `frame` takes an idle call of its own, as an input event does, because it
waits for the next frame drawn. The driver's log names both (`2000 ms, line 18: dump`). The files are local results like the other
captures of the test server's runs: never committed or published (notes/reconstruction.md, "Test server").

### The state dump (src/testmode/StateDump.h)

One line of JSON per file: `{"format":"patella-state 1","step":N,"time":T,"line":L,"state":{...},"gui":[...]}`
(step: the dump's number; time: the script's virtual milliseconds; line: the script line). "state" and "gui" are
exactly what tools/client_state.py's state() and gui() return for the same moment, key for key, in the same
order, with the same integer and text forms:

| Key | Client data (read as client_state.py reads it) |
| :--- | :--- |
| connection | Status.cpp ConnectionState |
| player | Creatures.cpp's PlayerID, PlayerHealth, PlayerMaxHealth, PlayerExperience, PlayerLevel, PlayerLevelProgress, PlayerMagicLevel, PlayerMagicLevelProgress, PlayerMana, PlayerMaxMana, PlayerSoul, PlayerCapacity, PlayerState, AttackID, FollowID (signed 32-bit, IDs too) |
| skills, skill_progress | PlayerSkillValue[7], PlayerSkillProgress[7] |
| map_centre | Map.cpp [MapX, MapY, MapZ] |
| map | Map.cpp Fields: the 2,016 map points' ObjectCount objects, each [Type, Data, Data2] (a creature: 99 and its ID) |
| creatures | the used slots of Creatures[150] (CreatureID not 0): slot, id (unsigned), name (char[32]), pos [RealX, RealY, RealZ], direction, outfit [OutfitType, Colors[4]], light [LightStrength, LightColor], mark, health, speed, visible (the bool's byte), skull (PKFlag), shield (PARTYFlag) |
| body | Inventory.cpp Body[10] |
| containers | the open ones of Containers[16]: index, object, name (char[30]), capacity, has_parent (the byte), objects (Count, clamped to 0-36) |
| trade | [OwnOffer, CounterOffer]: name, count (clamped to 0-100), objects |
| console | Talk.cpp DefaultTextBuffer's lines (at most 2,000): [Mode, WithoutTime up to 512 bytes, or null] |

"gui" is the window tree under MainWindow and then, after a node of class "(modal)", under ModalWindow: per
component depth, class (the RTTI name, `vt_<address>` without one), x and y in the client area, w, h, ptr and
vtable (this run's addresses), and text for TLabel (FullText up to 256 bytes, or null), TContextMenuEntry and
TTextButton (Text). Left out on purpose, as in client_state.py: times and animation state. Texts are the
client's Latin-1 bytes as JSON strings (outside printable ASCII as `\u00XX`, as Python writes them). For a
comparison between runs or with Tibia.exe (#23), "ptr", "vtable" and `vt_` class names are this run's own
(tools/testmode_dumps.py comparable() drops them); Tibia.exe's readings have no RTTI class names
(client_state.py's ORIGINAL_VTABLES names the ones the sessions need).

### The packet log (src/testmode/PacketLog.h)

A text file: the line `# patella-packets 1`, then a line per packet, seven fields separated by one space:

```
<time> <dir> <kind> <ordinal> <form> <payload> <padding>
12300 c2s game 1 xtea 65 2923be84e1
12500 s2c game 1 xtea 1e 000000000000
```

time: the virtual milliseconds (without a script: since the first packet); dir: c2s (sent) or s2c (received and
decrypted); kind and ordinal: the connection, as tools/proxy.py's kind and conn_ordinal (every unencrypted packet
the client sends starts one: opcode 0x01 login, 0x0a game, anything else other); form: `plain` for a
connection's first packet, which is not XTEA-encrypted, written with its RSA block as it was before RSA
encryption (`[0][XTEA key][fields][random bytes]`: proxy.py's "plain" of a first packet, which it decrypts with
the server's key), or `xtea`; payload: the packet without its length and padding (proxy.py's "plain"), lower-case
hex; padding: an XTEA packet's padding (proxy.py's "pad"). `-` stands for an empty field. Server packets appear
when the client decrypts them (PrepareNextPacket), so a packet that arrives just before the connection closes
and is never read is not in the log.

### The frame dump

A `frame` asks for the next frame the Vulkan engine shows: the engine's frame observer (the hook in
TSurfaceVulkan's InitDisplay; patcap.dll's observer when patcap.dll is loaded, then the test mode does not
observe) gets the frame's device-filtered pixels and the canvas read back (TRenderer::ReadBack), before
Present, and the test mode writes them as PNG files (8-bit RGB, filter 0, stored deflate blocks: no library).
They have the forms of compare_rendering.py's `<scene>.png` and `<scene>.filtered.png`, so issue #23 can compare
them with Tibia.exe's frames under the same tolerance (exact, or up to 2 per channel on the white pixels of the
mask; notes/vulkan.md, "Pixels"). The engine keeps the masks only while an observer is there: the test mode
observes only when the script has `frame` commands. For deterministic frames run the pinned CPU driver
(`PATELLA_VULKAN=cpu`, as testmode_check.py does). `TSurfaceVulkan::ReadScreen` (issue #19) is not used: it reads
the canvas at any moment, while the observer gives the frame as it is shown, with its mask.

### The check of the dumps (tools/testmode_check.py, tools/testmode_dumps.py)

In every run of testmode_check.py the client gets `--test-out <out>\run<N>` and `--test-sync <name>`: the
checker makes two events, `<name>-ready` and `<name>-continue`; after writing a state dump the client sets the
first and waits for the second (at most 300 s), while the checker reads client_state.py's state() and gui() from
its memory (the PDB of the PatellaTest.exe it was given) into `external-<NNN>.json`. The wait is taken out of
the pace (the driver's real start moves by it), so the virtual clock and the events do not see it. Compared:

- (e) every state-<NNN>.json against external-<NNN>.json: "state" and "gui" equal value for value, one dump per
  `dump` of the script: the issue's first criterion;
- (f) packets.log against the proxy's recording of the same run, per connection: the client packets identical
  and in order (payload and padding; the first packets with the RSA block before encryption), the server
  packets the client decrypted identical to the proxy's, in order from the first (the ones the proxy passed on
  after the client's last are listed as never read): the issue's second criterion;
- (g) one frame per `frame`, readable, its mask of the same size, its line in frames.txt;
- (h) statically, StateDump.h's key lists against the keys client_state.py writes (read from its source with
  Python's ast): the same names in the same order.

`python -I tools/testmode_check.py --self-test` runs the comparisons on made-up runs (equal data passes; a
changed state field, window text, client packet, padding, server packet, a missing dump, reading or frame each
fail, and the report names it) and the static key lists, without a server or a client.

## Reference recordings (issue #23)

The tier A comparisons (compare_traffic.py, compare_files.py, compare_rendering.py) drive two clients from
outside against the live test server. Issue #23 splits them into two steps: **record** (Windows, Tibia.exe,
the live server only for new recordings) writes a reference recording of every session once; **compare**
(Python only; the test client) plays each session with PatellaTest.exe, which replays the server side of the
recording itself, and compares it with Tibia.exe's recording. Choices and their reasons: notes/decisions.md,
"Reference recordings (Patella-Modern issue #23)".

```
python -I tools/reference.py record  [--reference DIR] [--sessions a,b] [--live DIR] [--reuse-live] [--hidden]
python -I tools/reference.py compare [--reference DIR] [--sessions a,b] [--exe PatellaTest.exe] [--out DIR]
                                     [--vulkan cpu] [--timeout 600] [--check-only] [--hidden]
python -I tools/reference.py list    [--reference DIR]
python -I tools/reference.py --self-test
python -I tools/compare_traffic.py record|compare ...   (the same, with that tool's sessions)
```

The reference set's default is `<repository>\build\reference` (gitignored); the orchestrator's set is
the main checkout's, `C:\Tibia\Patella-Modern\build\reference` (pass `--reference`). Reference recordings are CipSoft-derived local data:
never committed, pushed or uploaded.

Both run their clients on the desktop of the session they are started in; to keep the user's input away from
them, run them in a second session (notes/reconstruction.md, "Checking", "The live checks in a second session":
it has to stay connected and unlocked for the whole run). `--hidden` (issue #56, experimental) runs them on a
hidden desktop of their own: there `compare` matches only on a GPU (`--vulkan gpu`; the CPU driver's first present
fails), and `record` gets OpenGL frames from Tibia.exe instead of DirectX 9 ones (notes/decisions.md, "The live
checks on a hidden desktop").

### The sessions

| Session | From | Server side | Checkpoints |
| :--- | :--- | :--- | :--- |
| traffic | compare_rendering.py's windowed scenes, which contain compare_traffic.py's session (session_client.run_session) | compare_traffic.py's live session of Tibia.exe with the helper (the trade) | 26: every scene, state and frame |
| options | compare_files.py options_session | none | start, after-options (state; Tibia.cfg saved); Tibia.cfg on exit |
| walk | compare_files.py walk_session (19 actions, automap marks, the saved channel) | compare_files.py's live walk (record_walk) | every settle: 23 |
| error | compare_files.py: the game login, then 0xff | crafted from the walk (craft_error) | start; the message box; exit code 1 |
| error-login | compare_files.py: an earlier error.txt, the report dialog, 0x63 at the login | crafted (craft_error_login) | start; the box; exit code 1 |
| report | compare_files.py: error.txt from the session error, its report sent on entering the game | crafted (craft_report) | 4 |
| patch | compare_files.py: four login connections patch Tibia.dat, .spr, .pic, Patch.exe | crafted (craft_patch) | 2 |
| patch-exe | compare_files.py: a difference patch of Tibia.exe (Tibia.bak), the client closes | crafted (craft_patch_exe) | 3 |

Not covered: compare_rendering.py's fullscreen scenes (they need a 1024x768 desktop) and its oracle engines
(dx5, dx9, gl: the oracle's, not the x64 client's); compare_traffic.py's second live session (Patella.exe's, a
comparison of two live runs, which the reference replaces by Tibia.exe's recording) and compare_files.py's
`--repeat` (the masks it gave evidence for are gone: the virtual clock and TZ make the times equal).

### The record step (tools/reference_record.py)

1. The live recordings (skipped with `--reuse-live`): compare_traffic.py's live session of Tibia.exe (live_run,
   with the helper) and compare_files.py's walk (record_walk), through tools/proxy.py into `--live`
   (traffic-live.jsonl, walk-live.jsonl); the test characters' player files are put back, the server stopped.
2. The reference runs, no server: Tibia.exe in build\reference-run\ref\orig (patched to the proxy's port,
   importing patcap.dll: the virtual clock that stands still, the main timer's ticks only by ADVANCE, the
   cursor at the screen point 1020,760, the frame capture), TZ=UTC0, plays each session against tools/proxy.py
   replaying its server side, driven by the sessions' own code. The proxy stores with every server packet and
   close it sends the client's virtual time then (`vclock`: patcap's clock, which stands still while the packet
   is on its way). The Recorder sees every window message posted to Tibia.exe's window and every advance of the
   clock and writes them as the script; at every checkpoint (a settle: the server quiet, 2000 ms and 20 ticks,
   quiet again) it reads Tibia.exe's state and window tree (client_state.py) and captures its frame (patcap).
   Every session starts with a settle and the checkpoint "start": Tibia.exe draws its first frame only once
   the clock has moved.

### A session's folder

| File | What |
| :--- | :--- |
| meta.json | format `patella-reference 1`, Tibia.exe's and the data files' SHA-256, the date, server yes/no, exit code, message boxes, checkpoints, written and removed files, the seed files, frame and state masks, the time zone, the display modes (`display_modes`, optional: "The display modes") |
| session.script | the test-mode script (below) |
| replay.txt | the server side for the test client (src/testmode/ServerReplay.h) |
| server.jsonl | what the proxy replayed (the live recording or the crafted one) |
| reference.jsonl | the proxy's events of the reference run: Tibia.exe's packets (decrypted), the server packets with "vt" |
| states\state-NNN.json | Tibia.exe's state() and gui() at each checkpoint (the state dump's form, plus "name") |
| frames\frame-NNN.png | Tibia.exe's DirectX 9 frame at each checkpoint |
| files\ | the files Tibia.exe created or changed in its folder |
| saved\saved-NNN-file | files copied during the run (the script's save) |
| seed\ | the files the folder starts with besides the data files, the client and Tibia.cfg |
| Tibia.cfg.start | the Tibia.cfg it starts with (compare_traffic.py's) |

A session is stale (compare refuses it) when its format is not the tool's, when its Tibia.exe was not the one of
CLAUDE.md (SHA-256 1a82fe97...), when the root's Tibia.exe or a data file has another SHA-256 than the
recording's: record it again.

### The script of a recording

```
clock manual
cursor 1020 760
advance 2000 20
dump  # 1: start
frame  # 1: start (780x557, Direct3D 9 Present)
post 0x0200 0x0 0x1920076
...
quit
```

New commands for it (src/testmode/InputScript.h): `clock manual` (the clock moves only by advance: no wait or
at, no automatic ticks), `advance <ms> <ticks>` (patcap's ADVANCE: each tick moves the clock by ms/ticks and is
sent with SendMessage, all in one idle call), `cursor <x> <y>` (GetCursorPos returns this screen point),
`post <msg> <wparam> <lparam>` (the message exactly as the record step posted it to Tibia.exe, dispatched to
the main window; the posted messages are WM_MOUSEMOVE, the buttons, WM_CHAR and WM_KEYDOWN/UP of keys that
make no character, so the missing TranslateMessage changes nothing), `save <file>` (a copy of a file of the
client's folder to saved-NNN-<file> in --test-out). A posted WM_CLOSE becomes `quit`.

With a manual clock the driver gives the client 10 idle calls (QuietPasses, each with the client's
ProcessEvents) after every input event and every network action before the next one: Tibia.exe gets many idle
passes between the record step's messages, and a dialog the last event opens (the character list after the
login's answer) must be there before the next key comes.

### The server replay (src/testmode/ServerReplay.h, TestModeWin32.cpp)

`--test-replay <file>`: the file says the ports (`listen login 7271`, `listen game 7272`: the proxy's, to which
the record step's Tibia.exe was patched and which the recorded character list names) and per connection the
recorded order of client packets, server packets (with their virtual time) and closes. The test client is
patched by tools/patch_client.py to 127.0.0.1:7271 (no RSA key needed: the replay takes each connection's XTEA
key from the client's RSA block before encryption, the hook PATELLA_TEST_RSA_BLOCK). The schedule is the
proxy's: a server packet or close is due once the client packets recorded before it have come and its time has
come; a client packet that comes early waits for its place; after the client's close nothing more is sent.

The driver, at the start of each idle call: accepts connections, reads the client's frames and closes, sends
what is left of a frame; while the network is busy it does nothing else and the clock stands still (each wait at
most 10 s real, then it gives up, writes "gave up waiting" in the log, and compare reports it):

- the client resolves or connects (connection state 1, 2 or 5);
- a client packet (the hook PATELLA_TEST_PACKET_SENT counts them) has not been read by the replay;
- a server packet has not been decrypted by the client (PATELLA_TEST_PACKET_RECEIVED);
- the server's close (shutdown for sending) has not been answered by the client's close.

Otherwise the next due server packet (XTEA-encrypted with zero padding, as the proxy does) or close is sent, an
idle call of its own; then the script's events. With an automatic clock the next due replay time is a stop of
the clock too. So the server's packets reach the client at the same point of the script in every run, whatever
the real time: a session runs in 2 to 5 seconds.

### Message boxes

With a script the client's message boxes (FatalError's critical error box, the start's two boxes; the hook
PATELLA_MESSAGE_BOX in Player.cpp) are not shown: their caption, text and type go to messages.txt in
--test-out and the driver's log, the box counts as answered with OK. While a real box is open its modal loop
dispatches the thread's messages (after FatalError's ShutdownConnection the server's close comes, the client
leaves the game and saves Tibia.cfg); the hook does the same, with the replay pumped, until both have been quiet
for 500 ms (at most 5 s).

### The display modes (Patella-Modern issue #48)

The Graphics Options dialog (the traffic session's frame and state 3; the options session selects its first
mode, written to Tibia.cfg as FullscreenMode) lists the display modes Windows reports: EnumDisplaySettings in
the x64 client (vulkan/SurfaceVulkan.cpp GetFullscreenModes), DirectDraw's EnumDisplayModes in Tibia.exe; the same
list on this machine (issue #20), at least 640x480 and 16 bits, each size and depth once. They depend on the
monitor, the driver and the scaling, which change between a recording and its comparison: on November 3, 2026 the
display reported a 1176x664 mode it had not reported at the recording, and frame 3 differed in 664 pixels (the
list moved down a line), though nothing in the client had changed.

So a session's client lists the modes of the machine it was recorded on:

- the record step stores them in meta.json as `display_modes`, `[[width, height, bits per pixel], ...]` in
  EnumDisplaySettings' order, filtered as the dialog filters them (reference_record.py machine_display_modes,
  read before and after the run: a change during the run is a recording problem);
- `PatellaTest.exe --test-display-modes <file>` answers the client's `EnumDisplaySettings(nullptr, Index)` from
  the file (the hook PATELLA_ENUM_DISPLAY_SETTINGS, TestModeWin32.cpp EnumDisplayMode; src/testmode/DisplayModes.h),
  never asking the machine; the current and registry settings (the desktop's mode) and other devices stay the
  machine's. A file that does not parse ends the program with exit code 3, like a script. The file:

  ```
  # patella-display-modes 1
  640 480 32
  720 480 32
  ...
  ```

- the compare step writes `<out>\<session>\display-modes.txt` from meta.json and passes it; a session without
  `display_modes` (all recorded before issue #48, on November 3, 2026) gets FORMAT1_DISPLAY_MODES, the 24 modes
  this machine reported then (the frame shows the first seven; the machine reports the same 24 again). The report
  names the list's source, and a client whose log lacks "testmode: display modes" (one before issue #48 ignores
  the option) is a difference.

The key is optional, so the format stays `patella-reference 1` and the recorded set needs no new recording. Without
`--test-display-modes` the test build asks Windows, as Patella.exe does; the live modes of compare_traffic.py and
compare_rendering.py run both clients on one machine at one time, so both list the same modes and need no fixed
list.

### The compare step (tools/reference.py)

Python only (no Windows-only code; the client run is a subprocess): per session a fresh run folder in
`--out` (default build\reference-compare): the data files (their SHA-256 checked against meta.json), the test
client patched by patch_client.py, the CPU driver next to it, Tibia.cfg.start, the seed files; then
`PatellaTest.exe --test-script session.script --test-replay replay.txt --test-out dumps --test-log
dumps\driver.log --test-display-modes display-modes.txt` with PATELLA_VULKAN=cpu (`--vulkan`) and TZ=UTC0.
Compared:

a. packets: the client's packet log against Tibia.exe's packets (reference.jsonl) per connection, in order:
   payload and XTEA padding, the first packets with their whole RSA block (XTEA key and random bytes: the virtual
   clock gives rand() the same seed in both clients, so nothing is masked); a difference that is only the key or
   the padding is named as such. The server packets the client decrypted against the ones Tibia.exe was sent;
b. states: every state dump against Tibia.exe's at the same checkpoint: "state" equal, the window tree equal but
   for the addresses, Tibia.exe's vtable names paired with the same class everywhere; the texts of windows inside
   a recorded mask are not compared (the advanced options' device choice of issue #31);
c. frames: every frame against Tibia.exe's: exact, up to 2 per channel on the pixels the Vulkan engine marks as
   device-filtered (the user's decision of November 3, 2026), the recorded rectangles masked (the measured
   frame rate; the device choice of issue #31: compare_rendering.py's device_choice_mask);
d. files: the files written and removed (byte for byte; Tibia.cfg, the *.map files, Default.txt, error.txt with
   its time, the patched files, Tibia.bak), the saved files, the message boxes (caption and text), the exit code.
   Patella.ini (the x64 client's own, issue #31) is listed, not compared.

`--check-only` compares the results in --out again (any platform). Report: <out>\report.txt; exit code 0 when
every session matches, 1 on a difference or a stale session, 2 when a run failed.

The live modes of the three tools are unchanged and still work (`python -I tools/compare_traffic.py` etc.):
they compare two live clients against the server. `record` and `compare` as their first word forward to
tools/reference.py with the tool's sessions.
