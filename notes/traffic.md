# Network traffic comparison (issue #19)

Patella.exe and the original Tibia.exe play the same scripted session against the local test server
(issue #18, notes/testserver.md); every packet is recorded and decrypted, and the clients' packets, their
state after every step and their Tibia.cfg are compared. Choices and their reasons: notes/decisions.md,
"Network traffic comparison (issue #19)".

## Running it

```
python -I tools/build.py windows-x64
python -I tools/compare_traffic.py [--out build\traffic\result] [--skip-live] [--no-cross] [--patella EXE]
```

Since Patella-Modern issue #147 the whole run happens inside Windows Sandbox (both clients and the proxy; the test
server stays on the host, its ports tunnelled in; notes/testserver.md, "Windows Sandbox"); `--desktop` runs it on this
desktop as before.

`--patella` names the x64 Patella.exe to test (default build\windows-x64\Release\Patella.exe since Patella-Modern issue
#94; its test build runs, issue #92). Until #94 the default was the 32-bit oracle's build\oracle-x86\Release\Patella.exe
(build\Release\Patella.exe, the MSBuild output, before issue #10), read with its PDB (issue #6).

The test server must be stopped (the script starts and stops it itself, with `python -I tools/testserver.py
start` and `stop`). About 9 minutes: two live sessions (server start, session, server stop with its
save) and four replays. Exit code 0 when nothing differs. `--skip-live` replays and compares the live
recordings already in `--out` (no server needed); `--no-cross` replays only Tibia.exe's recording. A recording whose
look step is not this session's (one made before issue #137 looks north) is refused before its replay, with the step
it has (issue #140: session_client.recorded_look reads the look packet's field against the player's start field).

What it leaves in `--out`: `<client>-live.jsonl` (the recordings), `<client>-replay-of-<recording>.jsonl`,
`*.states.json` (the client state and window tree after every step), `*.Tibia.cfg` (as each client wrote
it on exit) and `report.txt`. The run folders are build\traffic\orig, build\traffic\patella and
build\traffic\helper (copies of the data files and the patched clients, made by the script from the root's
Tibia.exe and the Patella.exe under test). Before every run (live, replay, the helper's) the folder is
emptied (Patella-Modern issue #12, compare_traffic.clean_run_dir): everything but the data files and the
patched client goes, files and folders (the automap's `*.map` files, error.txt, Tibia.cfg, channel files),
and a data file that differs from the root's is copied again; then Tibia.cfg is written. Before #12 the
automap files of one run stayed for the next, so a later run read the automap an earlier one wrote.

## The pieces

| File | What it does |
| :--- | :--- |
| tools/proxy.py | TCP proxy on 127.0.0.1:7271 (login) and 7272 (game): records and decrypts, or replays a recording |
| tools/session_client.py | drives a client through the session by window messages |
| tools/client_state.py | reads a client's state and window tree from its memory (both binaries) |
| tools/compare_traffic.py | set-up, server, live sessions, replays, comparison, report |

### Recording and decryption (tools/proxy.py)

The clients under test are patched with `tools/patch_client.py --host 127.0.0.1 --port 7271 --rsa
tibia.pem`, so they log in to the proxy (x86 and, since Patella-Modern issue #15, x64 images: the port's
immediates are found through the RIP-relative reference to server.tibia.com there; notes/testserver.md). The proxy forwards the login connection to 127.0.0.1:7171 and
the game connection to 127.0.0.1:7172. The character list carries the game server's port (7172); the
proxy decrypts the login server's reply, puts its own game port (7272) in, and encrypts it again with the
same padding (the recording keeps the server's original as "orig"). Both clients get the same rewrite.
A recording therefore holds the game port of the proxy that made it (7272, or its private server's run's since #146).
A replay sets every game port of a character list to its own game port (issue #157, Proxy.replay_event, the only place
a replay departs from the recorded bytes; the frame's length stays), so a recording replays wherever it was made:
7272 recordings byte for byte, a private server's in compare_rendering, compare_network and a later `--skip-live`.

The first packet of each connection is unencrypted: `[u16 size][0x01 login | 0x0a game][u16 os][u16
version]([u32 dat][u32 spr][u32 pic] for the login)[128-byte RSA block]`. The server's private key
(C:\Tibia\gameserver\tibia.pem, or `PATELLA_TESTSERVER`'s tibia.pem; read only; the patched clients encrypt with its public half) decrypts the
RSA block: `[0][16-byte XTEA key][fields][random bytes]` (src/Communication.cpp SendLogin: account and
password; SendGameLogin: gamemaster flag, account, character name, password). Every later packet, both
ways, is `[u16 size][XTEA([u16 length][payload][padding to 8 bytes])]` (src/Network.cpp SendCommand and
PrepareNextPacket, src/TXTEA.cpp) and is decrypted with that key.

Each event of a recording (JSON lines): time, connection number and kind, direction (c2s, s2c, close),
the raw frame, the payload and the padding; for the first packet also the key, the decoded fields and the
RSA block's random tail.

**Masked**: the XTEA key and the RSA block's random tail (rand() in both clients, seeded with the time:
src/sysinc.cpp srand(timeGetTime())), and the XTEA padding (rand() % 256, SendCommand), which the
comparison never looks at because it compares payloads. Instead of being ignored, the masked bytes are
checked: the key and the random tail of every first packet must be one run of MSVC rand() results (the low
byte of a rand() result depends only on the low 24 bits of the LCG state, so the 16 unknown bits are
searched; proxy.rand_stream), as SendLogin/SendGameLogin make them (16 rand() for the key, then 128 for
the whole block, whose start the fields overwrite); a changed bit fails the check. Everything else is
compared byte for byte, including the signatures of Tibia.dat/.spr/.pic, the account, the password and the
character name inside the RSA block.

The alternative, fixing rand()'s seed and the clock in both clients, would mean patching the clients'
code; comparing after decryption needs no change to either client.

### Replay

In replay mode the proxy is the server: it answers each connection of the client from a live recording.
The client's first packet gives it the client's key; the recorded server payloads are encrypted with it
and sent in the recorded order, each after the client packet that preceded it in the recording and with
the recorded delay after it. Every client packet is checked against the recording as it comes (a
mismatch is reported; the replay goes on). So both clients get the same server data byte for byte, and
their states can be compared exactly: in a live session the world moves on its own (creatures walk,
the server's frames are cut differently), so live server streams never match.

`wait_idle()` tells the session driver when the server side is quiet: in a replay, when every recorded
server packet before the client's next packet has been sent and no client packet came for 0.5 s (a
connection counts as busy from its accept until it waits for the client); live (since issue #144; a fixed 1.5 s
until then), when no server packet came for 0.5 s, counted from the end of the step at the earliest
(compare_traffic.py `live_wait`; a server that sends for 30 s without such a pause fails the run).

### The session (tools/session_client.py)

Character "Patella Tester" (account 772001), with "Patella Second" logged in by the helper as the trade
partner. Fifteen steps, each followed by the wait and a state snapshot:

| Step | Input | Client packet |
| :--- | :--- | :--- |
| login | click Enter Game, account, Tab, password, Return; Down to "Patella Tester" in the list, Return | login 0x01, game login 0x0a, fight modes 0xa0 |
| walk north, east, south, west | arrow keys (WM_KEYDOWN/UP), back to the start | 0x65, 0x66, 0x67, 0x68 |
| turn south | Ctrl+Down | 0x71 |
| say | "hello world", Return | 0x96 (Cipfried, the temple NPC, answers "hello") |
| look at own field | Shift+right click on map field (8,6), the player's own (issue #137; compare_rendering.py's too since #140; "look north", field (8,5), in the frozen reference recordings) | 0x8c |
| open container | Ctrl+right click on body slot 3 (the bag) | 0x82 |
| look in container | Shift+right click on the bag's first slot | 0x8c |
| use in container | Ctrl+right click on it (eats the red apple) | 0x82 |
| trade request | right click on body slot 5 (the club): context menu, "Trade with ...", left click on Patella Second | 0x7d |
| look at offer | Shift+right click on the own offer's first slot | 0x7e |
| cancel trade | the trade window's Cancel button | 0x80 |
| close container | the bag window's close button | 0x87 |
| logout | Ctrl+L | 0x14 |

No screenshots are used (the display was off during this work; screenshots would be white): every click
place is computed from the client's memory: the window tree (tools/client_state.py gui(): TComponent's
Parent/Next/OffsetX/OffsetY/Width/Height, TWindow's FirstChild; a component is a window when its
GetFirstChild slot is `mov eax, [ecx+24h]; ret`, on x64 `mov rax, [rcx+XXh]; ret`), the skin's slot areas (SkinProperties: Style288.Data60
for the body slots, Data62C-Data64C for container slots, Style6FC for the trade offers), TMapWindow::GetField's
field mapping, the map window's creature name positions (Names, as GetCreatureAt uses them) and the context
menu entries' texts. Patella.exe's classes are named by its RTTI; Tibia.exe has no RTTI for its windows, so
the few classes the driver needs are named by their vtables (client_state.ORIGINAL_VTABLES).

The said text was "hello patella" at first: the client then shows the "Account Data Warning" dialog and
sends nothing, because the text contains the password (both clients do this).

### The waits (issue #144)

The session sleeps for no fixed time. Each gesture's messages are posted back to back: a key with its modifier's
key down and up around it, a click's modifier, mouse move, button down and up, and a typed string's WM_CHARs
(account, Tab and password as one string; "hello world" and its Return as one). Windows' queue keeps their order,
and the clients take each message on its own (the modifiers are TMainFrame's KeyFlags from the WM_KEYDOWN/UP
messages, not GetKeyState), so the messages, their order and the packets they cause are the same as with the
sleeps between them that were there until #144 (50 to 100 ms each, at least 250 ms a click, 150 ms a key). The
scripted test client (compare_rendering.py's TestDriver) gets a gesture as one control-channel command of post
lines instead of one command per message; it still delivers each in an idle call of its own with the quiet idle
calls after it (notes/testmode.md, "The control channel").

Instead of the fixed sleeps, each step waits for what it is meant to do, as both clients show it (Tibia.exe
through tools/client_state.py, the test client through its probe), with a timeout (10 s; 30 s for the start
screen) and an error naming what did not come (session_client.Driver.wait_until):

| Where | Waits for | Fixed time until #144 |
| :--- | :--- | :--- |
| start | the start screen: a TStartWindow in the window tree | 4 s after the window appeared |
| login: Enter Game | the login dialog (TLoginDialog) | 1.5 s (compare_rendering.py 1 s) |
| login: Return | the character list dialog (TCharacterListDialog) with its names, after the step's wait | 0.3 s before Return, 0.5 s after the wait |
| login step | the player's own creature in the client's table (the server's game login answer) | none |
| the helper's login | the character list, then the helper in the game | 1.5 s, then 2 s |
| walk north, east, south, west | the player's position one field on (read before the key) | 1.0 s after the key |
| turn south | the player's direction south (2) | none |
| say | one more console line "Patella Tester: hello world" | none |
| open container, close container | one TContainerWindowBody more, one less | none |
| context menu | the entry "Trade with ..." | none |
| trade with | the entry gone (the menu closed) | none |
| trade request, cancel trade | a TTradeOffers there, gone | none |
| after the logout | the start screen again, before WM_CLOSE (compare_traffic.py) | 1 s |

The looks, "use in container" and the trade request's look at the offer change nothing a reading shows but the
server's text: the step's wait covers them, as before. With a real clock (compare_traffic.py, compare_files.py) the
condition is waited for right after the input, then the step's wait; with a manual clock (compare_rendering.py:
Tibia.exe under patcap, the test client with `clock manual`) the clock moves only in the step's wait (its advance),
so the condition is checked after it. Tibia.exe's login dialog and character list are named by their vtables
(client_state.ORIGINAL_VTABLES: 0x5627d8, 0x562370).

The time saved: notes/decisions.md, "#144".

### State compared (tools/client_state.py state())

Connection state; player ID, health, experience, level, magic level, mana, soul, capacity, state icons,
attack and follow IDs, skills and their progress; the map centre and every map point's objects (type,
data, data2 of ObjectCount objects, the creature ID for a creature); the creature table (ID, name,
position, direction, outfit, light, mark, health, speed, visibility, skull, shield); the ten body slots;
the open containers (object, name, capacity, parent flag, objects); both trade offers; the default
channel's lines (mode and text). Left out on purpose: times and animation state (step end and duration,
mark time, effects), which follow the clock, not the packets. Also compared: the window tree (depth,
place, size, label and menu texts; Tibia.exe's vtable names must pair one to one with Patella's classes).
Tibia.exe's addresses and field offsets are the ones in src/ comments; Patella.exe's come from its PDB
(dbghelp; issue #6): the globals relative to its base, and every field offset, structure size, array length
and the GetFirstChild vtable slot by type and member name (client_state.LAYOUT_FIELDS, LAYOUT_SIZES,
LAYOUT_VSLOTS), so the x64 build, whose layouts are its own, is read correctly too. The oracle-x86 build has
the original layouts (static_asserts in src/); `python -I tools/client_state.py --layout <Patella.exe>`
lists every entry and, for an x86 image, compares it with the original's constant (0 mismatches);
`--self-test` checks the process side (bitness, module base, pointer reads) against the Python process.
compare_files.py and compare_rendering.py read their controls through the same table (issue #15): the
option dialogs' check boxes and buttons, TCheckBox.Checked, the frame rate scroll bar's LeftButton, the
hotkey dialog's AutoSendCheckBox and Selected, the mark dialog's TypeButtons, TAutomapWindow's zoom buttons
and four SkinProperties members; pointers with the image's pointer size.

### Live sessions and the server's state

Before each live session the test characters' player files (data\usr\74\15146574.usr, data\usr\75\
15146575.usr) are put back to what they were when the script started (a file that did not exist is
removed), and the server is started fresh: so both clients start from the same position, inventory (the
apple is eaten in every session) and world (creatures at their spawn points). After the last session the
files are restored again. The helper is a third copy of Tibia.exe (build\traffic\helper, patched without
--port: it logs in straight to the server), started after the client under test with the command-line
word "gamemaster": the client refuses to start while another Tibia window exists unless it is a
gamemaster client (src/Player.cpp, CountTibiaWindows), and this server ignores the client's gamemaster
flag (database/db_sqlite.cc never reads GamemasterRequired). The helper is not compared. It did not always
log out on Ctrl+L (2 of 4 sessions) nor close on WM_CLOSE; it is then killed, and the server logs the
character out (its file is restored anyway).

**The server's start-up race**: run-testserver.ps1 once reported `0.0.0.0:7171`: the login server
listened on all interfaces although config.cfg says 127.0.0.1. The server passes the login thread a
pointer to its global GameAddress (src/main.cc: `bindIP = GameAddress`, login/login_server.cc:
`std::thread(LoginServerWorker, Port, BindIP)`), and InitAll rewrites GameAddress while the thread starts:
ReadConfig first sets it to "0.0.0.0" (game/config.cc), then reads bind_address again, and
LoadWorldConfig later writes the world's address into it; a thread that binds in that window gets
0.0.0.0. compare_traffic.py now stops and restarts the server when 7171 is on 0.0.0.0. In that one
session (try3, Patella.exe's live session, about 100 s) 7171 was reachable from the network if the firewall
let it through. Not fixed in the server (its configuration and code are the user's).

## Results (November 2, 2026)

`python -I tools/compare_traffic.py` (out build\traffic\result, 10:38-10:44): **no differences**.

- **a. Client packets, live**: 18 packets in each session (login connection: 0x01; game connection: 0x0a,
  0xa0, 0x65, 0x66, 0x67, 0x68, 0x71, 0x96, 0x8c, 0x82, 0x8c, 0x82, 0x7d, 0x7e, 0x80, 0x87, 0x14), identical
  in order and content between Tibia.exe and Patella.exe. The masked bytes: in all 6 first packets of each
  client (a login and a game packet in each of its three runs: one live, two replays) the key and the
  random tail are one rand() run; all 12 keys differ.
- **b. Client packets, replays**: both clients replaying Tibia.exe's recording and both replaying
  Patella.exe's: 18 packets each, identical to the recording; the proxy reported no mismatch.
- **c. Client state**: after each of the 15 steps, in both replays, Tibia.exe's and Patella.exe's states
  (about 578 map objects, 3-5 creatures, the bag and its apple, the trade offers, the console lines) and
  window trees (50-84 windows; Tibia.exe's 33 window vtables pair one to one with Patella's classes) are
  identical. Control: the same client's states in the replays of the two different recordings differ at
  all 15 steps (other creatures, other times in the console), so the check does see differences.
- **d. Tibia.cfg**: identical in both live sessions and in all four replays.
- **e. Server packets, live** (informational): 25 and 30 frames (3,288 and 3,402 bytes), 4 equal at the
  same index; the rest differs by the world: rabbits and Cipfried walking (11 and 13 frames are creature
  moves, 0x6d), the time in "Your last visit", how the server cut its frames. That these differences are
  the server's and not the clients' is what the replays show: given the same server data, the clients
  send the same packets and end in the same state.

Earlier runs while the scripts were written (build\traffic\try1-try3) found only problems of the harness:
the server inheriting a pipe (the script hung), the session typing the password ("Account Data
Warning"), the character list's order (by level and last login, so the character is now chosen by name),
the login step's snapshot taken before the replay was idle, and the map window's Names list read while a
Draw was refilling it. try3 (the same comparison, before the rand() check and the control) also reported no
difference.

**Client bugs found**: none. No change to src/.

**Side effects on the server's data**: 9 server starts and clean stops in all (each stop saves the map and
the player files, as every clean shutdown does); twice the login server came up on 0.0.0.0:7171 (try3's
second session, about 100 s, not yet detected; the final run's first start, 14 s until the restart). Each live session
logs in both test characters (Characters.LastLoginTime in tibia.db changes) and eats Patella Tester's apple;
the player files are restored afterwards: 15146574.usr is again byte-identical to its state before this
issue (kept in build\traffic\usr-before-19), 15146575.usr ("Patella Second", which had never logged in) is
removed again. The logs in data\log grew. Only account 772001 was used; config.cfg was not changed. The
server is stopped.
