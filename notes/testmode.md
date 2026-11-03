# The test mode: virtual clock, scripted input and the dumps (Patella-Modern issues #21, #22)

The client's built-in test mode (notes/modernization-plan.md, 4.4; M2): one clock behind timeGetTime, the wall
clock and the frame timer, and a script of keyboard, mouse and text events at virtual times (issue #21); the
state dump, the packet log and the frame dump (issue #22, "The dumps" below). It is compiled only into the test
build, PatellaTest.exe; Patella.exe and the oracle-x86 build contain none of it. The session scripts and
reference recordings (issue #23) come later and build on it. Choices and their reasons: notes/decisions.md,
"Test mode: virtual clock and scripted input (Patella-Modern issue #21)" and "Test mode: the dumps
(Patella-Modern issue #22)".

## Running it

```
python -I tools/build.py windows-x64            # builds Patella.exe and PatellaTest.exe (and the unit tests)
build\windows-x64\Release\PatellaTest.exe --test-script <file> [--test-log <file>] [--test-out <folder>]
                                          [--test-sync <name>] [the client's own words]
python -I tools/testmode_check.py               # the determinism and dump check against the test server
python -I tools/testmode_check.py --self-test   # the check's comparisons on made-up data (no server, no client)
```

Run PatellaTest.exe from a run folder (copies of the data files, a patched client: notes/testserver.md), never
from the root. Without `--test-script` it is the client on the system clock, through the same hooks. With a
script it plays the script on the virtual clock and drops the keyboard and mouse input that comes from Windows,
so the desktop's own mouse and keyboard cannot reach it (the limit of the GUI comparisons in
notes/reconstruction.md, "Test server"); its window is still a real window. `--test-log` lists every event it
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

## The pieces

| File | What it does |
| :--- | :--- |
| src/TestHooks.h | the hooks in the client: the system calls themselves without PATELLA_TEST_MODE |
| src/testmode/Clock.h, Clock.cpp | IClock and TVirtualClock (portable) |
| src/testmode/InputScript.h, InputScript.cpp | the script: parser, key table, scheduler (portable) |
| src/testmode/StateDump.h, StateDump.cpp | the state dump: its data and JSON form, the key lists (portable; #22) |
| src/testmode/PacketLog.h, PacketLog.cpp | the packet log: the line format, the connections (portable; #22) |
| src/testmode/Png.h, Png.cpp | the frame dump's PNG pictures: CRC-32, Adler-32, stored deflate (portable; #22) |
| src/testmode/TestModeWin32.cpp | the Windows side: TSystemClock, the command line, the driver, the messages, the dumps' files |
| src/testmode/ClientStateWin32.cpp, ClientView.h | the state dump's reading of the client (test build only; #22) |
| cmake/TestMode.cmake | patella_testmode, the portable part, on every product preset |
| cmake/WindowsX64.cmake | PatellaTest.exe (option PATELLA_TEST_BUILD, default ON) next to Patella.exe |
| tests/unit/testmode_*.cpp | tests_testmode: the clock, the parser, the scheduler, the driver loop; the dumps' formats |
| tests/testmode/login-walk.script | the check's script (6 dumps, 3 frames) |
| tools/testmode_check.py | the determinism check and the check of the dumps |
| tools/testmode_dumps.py | the dumps' readers and comparisons, the static key lists, the self-test |

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
| PATELLA_TEST_RSA_BLOCK(Block) | nothing (a statement macro, #22) | Communication.cpp SendLogin, SendGameLogin: the RSA block before RSA.Encrypt |
| PATELLA_TEST_PACKET_SENT(Data, Size, Padded) | nothing (#22) | Network.cpp SendCommand: the packet complete, before XTEA (or before the unencrypted send) |
| PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize) | nothing (#22) | Network.cpp PrepareNextPacket: the next packet decrypted |
| PATELLA_TEST_FRAME_OBSERVERS(Patcap, ...) | nothing (#22) | vulkan/SurfaceVulkan.cpp InitDisplay: without patcap.dll, the frame dump observes the engine |

The state dump also reads the client's file-local data through four View functions at the ends of
Creatures.cpp, Inventory.cpp, Map.cpp and Status.cpp (src/testmode/ClientView.h), inside `#if
defined(PATELLA_TEST_MODE)`: they hand out const pointers and write nothing; at the ends of the files, so no
line of the product's code moves.

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
pings). Issue #23 removes this: the client replays the server side of a recording, in virtual time.

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

### What issue #23 adds

The sessions of compare_traffic.py and compare_files.py as scripts in tests/testmode; the server side of a
recording delivered by the driver at virtual times (a second event source for TScheduler::NextStop), with
`pace 0`; reference recordings from Tibia.exe (its packets, client_state.py's readings, patcap's frames) and the
record/compare split, against the formats above.
