# Crafted and fuzzed server streams (issue #37)

Patella.exe and the original Tibia.exe get the same crafted and fuzzed server streams, and what each does is
compared: the packets it sends, the dialogs it shows, its state and how it ends. This is the second line of the
comparison of Network.cpp and MainFrm.cpp; the first is the difftest (networktest.cpp, mainfrmtest.cpp,
notes/decisions.md "Network.cpp and MainFrm.cpp (issue #37)"), which compares every function of both files in
the child but stops at the calls into MFC and Windows. Here the real clients run, with their own MFC (7.1 and 14),
WinSock and message loop.

## Running it

```
python -I tools/build.py oracle-x86
python -I tools/compare_network.py [--out build\network\result] [--only login,game-cut] [--list] [--repeat]
```

No server is needed (the test server stays stopped). The script copies issue #19's live recording of Tibia.exe
(build\traffic\result\orig-live.jsonl of that issue's worktree; sha256 61103b95...825b) to
build\network\orig-live.jsonl once; `--recording` takes another. It sets up build\network\orig and
build\network\patella like tools/compare_traffic.py (copies of the data files, the clients patched by
tools/patch_client.py to log in at 127.0.0.1:7271 with tibia.pem's modulus) and runs each scenario with
compare_files.py's prepare() (a fresh Tibia.cfg, no other files). `--only` takes name prefixes, `--list` lists the
scenarios, `--repeat` runs Tibia.exe a second time where the clients differ (to tell the client's run-to-run
variation from a difference). Exit code 0 when nothing differs. Out: report.txt, and per scenario
`<scenario>\<client>.jsonl` (the proxy's events), `<client>.json` (everything compared) and the files the run
wrote.

## How the streams are made

tools/proxy.py's replay sends the recorded server packets, each after the client packet that came before it in the
recording; a replayed server event can now also say how its frame is made and delivered (issue #37):

| Field | Effect |
| :--- | :--- |
| `frame_hex` | these bytes instead of the encrypted payload (a broken frame) |
| `xtea_key` | encrypt with this key instead of the client's (wrong encryption) |
| `inner` | the frame's inner length (bad padding, a length past or inside the frame) |
| `hold` | keep the frame, send it with the next one (coalesced frames) |
| `pieces`, `piece_delay` | send the bytes in pieces of these sizes, this many seconds apart (fragments, slow reads) |
| `cut` | send only the first n bytes (held frames included), then close (a disconnect at any point) |

`Proxy(refuse=("login",))` or `("game",)` leaves that port closed (connection refused). Recordings and the other
scripts use none of these, so they replay as before.

The scenarios (`--list`): the login server's answer as recorded (the message of the day and the character list in
one frame), as two frames, coalesced, one byte at a time 3 ms apart, in random pieces, after an empty frame; the
login answers 0x0a (an error), 0x0b (a message), 0x28 (overloaded), the message of the day alone, a list without
characters and with 20; a frame of 9 bytes (not a multiple of 8), the answer under a wrong key, inner lengths 100
bytes past the frame and inside the message of the day; no answer at all; the login port refused (the client tries
all four servers, all patched to this address); the server closing the login connection before the frame, after one
and both size bytes, inside the first frame's body and at its last byte, after it, inside the second frame's size,
body and at its last byte. In the game (after the recorded login and the character's selection): the recorded game
start, every frame one byte at a time, in random pieces 10 ms apart, consecutive frames coalesced, the server
closing after 0, 1, 2, 3, 100 and half the bytes of the map frame and inside the second frame, a 9-byte frame after
the map, the second frame under a wrong key, its inner length 40 bytes too long and 3 bytes short, empty frames
around the map, a ping (0x1e, which the client answers), a game error (0x14) and the waiting list (0x16, 2 s, then
a reconnection the replay does not answer) instead of the map, a server message (0x15), and the game port refused.

## What is compared

For each scenario and client: the packets the client sent (decrypted; the XTEA key and the RSA block's random tail
masked as in notes/traffic.md, proxy.masked), the proxy's mismatches against the crafted stream (informational, and
compared between the clients), the message boxes (FatalError's critical error box: caption and texts), the exit
(the code after the box or after WM_CLOSE, or "running (killed)" when the client refuses to close: in the game it
shows the logout warning), and before WM_CLOSE the window tree (client_state.py gui(): every dialog with its labels,
so "Connection failed" and its text, the character list dialog, the "Connecting" box), the character list in memory
and the client state (state(): connection state, player, map, creatures, inventory, containers, console), then
error.txt (each crash time masked: two runs of a client differ there, issue #21) and Tibia.cfg as written on exit,
and the names of the files the run wrote or deleted.

## Results (November 2, 2026)

`python -I tools/compare_network.py --repeat` (build\network\result, 51 scenarios, 1,568 s): **47 the same, 4
variable, no difference** (the comparison of the proxy's own socket errors was narrowed afterwards, see below; the
saved results compared again with it: the same 47 and 4).

- **The same in both clients** (47): every login answer as recorded, as two frames, coalesced, byte by byte, in
  random pieces and after an empty frame (the character list dialog, "Select Character"); 0x0a ("Sorry" with the
  text), 0x0b ("For Your Information"), 0x28 ("Connection failed ... Error: Disconnected from server.": the
  overload code is reported through the disconnect), the message of the day alone ("Connecting" stays), the empty
  and the 20-character lists; a 9-byte frame, inner lengths past and inside the frame (the critical error box,
  exit code 1, error.txt equal but for the time); no answer ("Connecting"); the refused login port (all four
  servers tried: "Connection refused ... All login server are offline"); every cut of the login connection
  ("Disconnected from server" up to the first frame's last byte, "Connecting" after a complete first frame);
  the game start whole, byte by byte, in pieces and coalesced (both clients in the game, the same map, creatures
  and inventory, the same three packets, and both refuse WM_CLOSE: the logout warning); every cut of the game
  connection ("Cannot connect to the game server ... Disconnected from server"; inside the second frame: back at
  the character list); a 9-byte frame and wrong inner lengths in the game (the critical error); two empty frames
  coalesced with the map frame (the second is the first's inner length: GetNextCommand returns -1 and the map
  frame waits unread in both clients, "Connecting"); an empty frame alone before the map frame (the login
  connection's bytes still in the buffer are read as commands: the critical error in both); the ping (both
  answer it); the game error ("Sorry", "Your character is banished."), the server message, the waiting list (a
  reconnection the replay does not answer) and the refused game port ("The game server is offline").
- **Variable** (4): the login answer and the second game frame under a wrong key, and an empty frame coalesced
  with the map frame or the next frame. The client reads garbage that depends on its own session key (rand(),
  seeded with the time), so Tibia.exe's two runs differ from each other as much as from Patella.exe's (each run
  twice; the outcome is of the same kind: a critical error with another packet type, or a reconnection).
- **Found while writing it** (notes/decisions.md "Network.cpp and MainFrm.cpp (issue #37)"): three frames sent
  in three sends right after each other arrive differently: Patella.exe, which answers FD_READ sooner, read the
  first empty frame alone, Tibia.exe all three at once; with the arrival fixed (the two scenarios above) the
  clients do exactly the same. The proxy's reader sometimes saw a connection reset when a client exited after
  the critical error box with data unread (once for Patella.exe, never in the same scenario's other runs): socket
  errors of the proxy itself (WinError 10053/10054) are no longer compared, its mismatches still are.

No reconstruction bug found; src/ unchanged. Run time about 26 minutes (about 25 s per scenario and client; the
variable ones run Tibia.exe a third time).
