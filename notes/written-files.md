# Files the client writes (issue #21)

Patella.exe and the original Tibia.exe play the same sessions; every file each client creates, changes or
deletes in its folder is collected and compared byte for byte. Choices and their reasons: notes/decisions.md,
"Written files (issue #21)". Builds on issue #19's machinery (notes/traffic.md: the proxy and its replay, the
session driver, the memory reader) and the test server of issue #18 (notes/testserver.md).

## What the client writes

From the sources (every fopen, ofstream, remove and rename in src/):

| File | Written by | When |
| :--- | :--- | :--- |
| Tibia.cfg | Preferences.cpp SavePreferences | closing the options dialog, the hotkey dialog, the exit |
| `<bx><by><bz>.map` (`%03d%03d%02d.map`, 256x256 fields of one floor) | Map.cpp SaveAutomapBlock | a changed automap block leaving the cache of blocks, the exit (ExitMap) |
| error.txt | Tibia.exe only since Patella-Modern issue #74 (D2): its HandleException appends a report; deleted by its SendErrorReports on entering the game, or its error report dialog's Cancel. The fork's clients write, read and delete none (a deliberate difference, tools/deliberate.py) | a TMyException that reaches InitInstance or Run |
| `<channel>.txt` | ConsoleWindow.cpp SaveChannel (appends) | "Save Window" in a console tab's menu |
| Tibia.da_, Tibia.sp_, Tibia.pi_, Patch.ex_; Tibia.bak | Script.cpp ApplyPatch / ApplyFullPatch | a patch from the login server (0x1e difference patch, 0x1f whole file, 0x20 blocks; Communication.cpp ReceivePatch*) |
| Patella.ini (the x64 client only, Patella-Modern issue #31) | src/vulkan/DeviceSettings.cpp SaveDeviceChoice | a click in the advanced graphics options that changes the graphics device (notes/vulkan.md, "Choosing the device") |
| Tibia.dat, Tibia.spr, Tibia.pic, Patch.exe (replaced) | Script.cpp InstallPatch (remove, rename `_` to the name) | messages 0x41-0x44 after the patch, once the login connection is closed |

Since Patella-Modern issue #64 each of these files except error.txt (a std::ofstream, Player.cpp, until issue #74) is a `TFile`
(src/types/File.h): Tibia.cfg and the binary files (`.map`, the patch files) through Script.h's four classes
("rb"/"wb", binary: Tibia.cfg's CR LF are written by writeLn itself), the saved channel through ConsoleWindow.cpp's
SaveChannel in text mode ("at": every "\n" of the text becomes "\r\n", as the Microsoft C runtime wrote it, on
Windows and on Linux alike). The test mode's dumps are TFiles too: packets.log, frames.txt and messages.txt binary, the
`--test-log` text. The bytes are the same as before (tests/unit/file_test.cpp; notes/decisions.md, issue #64).

The login server decides about patches: a 7.72 server can send them in answer to any login (the client's
first packet carries the three files' signatures), so a crafted replay of the login connection triggers
every one of them. A Tibia.exe patch is written to Tibia.bak; the client then shows "Updating", closes
and runs patch.exe (Player.cpp Run, `_execl`), which is CipSoft's separate patcher and not part of the
client: what it does with Tibia.bak cannot be compared here (we do not have it, and running a program the
session wrote is out of the question). The scenario leaves patch.exe out, so `_execl` fails and the client
just ends.

## Running it

```
python -I tools/build.py oracle-x86
python -I tools/compare_files.py [--out build\files\result] [--only options,walk,...] [--skip-live] [--repeat]
                                 [--walk ladder|stairs] [--patella EXE]
python -I tools/compare_files.py --check-route     # the walks against the server's map, no client
```

`--patella` names the Patella.exe to test, x86 or x64, with its PDB next to it (default
build\oracle-x86\Release\Patella.exe; as compare_traffic.py's; Patella-Modern issue #15).

The x64 client's own settings file Patella.ini (issue #31) is not compared: Tibia.exe has none. The report names
it with its contents (the options scenario's click in OpenGL's place, the x64 client's "CPU" button, writes
`Device=cpu`). For that click the run folder of an x64 client also gets the CPU driver next to it
(vulkan_lvp.dll, lvp_icd.x86_64.json from the client's build folder, copied in every scenario's preparation):
without it the CPU button is disabled and Tibia.cfg would keep `Engine = 2` where Tibia.exe writes 1.

The test server must be stopped; the script starts it for the one live recording and stops it (a clean stop
saves, as always) and puts the test characters' player files back, as compare_traffic.py does. Everything
else runs against tools/proxy.py's replay, without a server. Exit code 0 when nothing differs. Since
Patella-Modern issue #16 the live recording is also Tibia.exe's walk run (its files are walk-live\orig\):
only Patella.exe replays the walk, and the walk itself is 19 actions instead of 116 steps (below). About
6 minutes for all scenarios (estimated from the November 3 timings; before #16 about 18, of which 13.5 were
the four walks of about 3 minutes each). `--skip-live` reuses `--out`'s walk-live.jsonl (no server); Tibia.exe
then replays it for the walk as before, and the walk is the recording's (walk-live.walk). `--repeat` runs
Tibia.exe a second time for walk (walk, error and error-login until issue #74), the evidence for the masks (about 2 minutes more);
before #16 that was the default and `--no-repeat` skipped it (still accepted, it changes nothing now).

The run folders are build\files\orig, build\files\patella and build\files\orig-repeat, the second
Tibia.exe run's own (Patella-Modern issue #12; before, it ran in build\files\orig) (compare_traffic.py's
set-up, via its set_folder: copies of the data files and the patched clients). Before each run the folder
is emptied (compare_traffic.clean_run_dir, files and subfolders), then: the data files as in the root,
compare_traffic.py's Tibia.cfg, nothing else but the client (and in Patella's folder a copy of the patched
Tibia.exe, the input of the Tibia.exe patch). Results in `--out`:
`<scenario>\<client>\` (the files the run wrote), `<scenario>\<client>.jsonl` (the replay's packets),
`repeat\<scenario>\orig\` (the second Tibia.exe run), walk-live.jsonl and walk-live\orig\ (the live
recording and its files) and report.txt.

## The scenarios

| Scenario | Session | Server data |
| :--- | :--- | :--- |
| options | start window: Options; General (all five check boxes), Console (all five), Network (Test Server Login), Graphics (the next fullscreen resolution), Advanced (light, stretching, mouse pointer, the frame rate limit three steps down, the OpenGL engine: a live engine switch), Hotkeys (F1-F5: two texts, two sent automatically, one empty); OK everywhere; Tibia.cfg after the options dialog and after the exit | none |
| walk | login; automap: three marks set (types 3, 9, none), one edited (text and type), one deleted; zoom in once, out twice; 19 actions over floors 7, 6, 7, 8, 7 into a second automap block (116 steps over floors 7, 6, 8, 7, 6, 7 before issue #16); "Save Window" of the default channel; logout | the live recording (Tibia.exe against the test server); Patella.exe replays it |
| error | login, then the server packet 0xff (unknown type in the game): HandleException, the critical error box (its OK is pressed), exit code 1; Tibia.exe also writes error.txt, and its box names that file (since issue #74 expected, the deliberate differences; error-login and report, the error report dialog and packet 0xe8, went with the report) | the recording's login and first game packets, then the crafted packet |
| patch | login; four login connections in a row: a difference patch of Tibia.dat (all four operations copy/skip/insert/replace, all count forms: in the command byte, byte, word and quad), of Tibia.spr, the whole Tibia.pic and the whole Patch.exe; each installed, then the client reconnects by itself, with the new signatures in its login packet | crafted, from the recording's first login packet |
| patch-exe | login; a difference patch of Tibia.exe (eight bytes of the DOS stub) to Tibia.bak; "Updating" (OK), the client closes | crafted |

Every patch writes a new signature into its file (or eight bytes of Tibia.exe), so a patch that silently did
nothing would be seen; the script also checks each patched file against the result it computes itself.

Walking (tools/compare_files.py WALKS, Patella-Modern issue #16). What the walk is for: the client writes the
automap blocks 125/125 on floors 6, 7 and 8 and 125/126 on floor 7 (12512506.map, 12512507.map with the marks,
12512508.map, 12512607.map), no other. The client records only the player's floor (Map.cpp UpdateAutomap), x-8
to x+9 and y-6 to y+7 around the player, so the player has to stand on floors 6 and 8 with y at most 32248
and on floor 7 with y at least 32249; the marks (automap pixels from the player, one field each at
AutomapZoom 2: the old marks were at +20,0 and -12,+10 of the temple) have to lie in block 125/125/7.

- **ladder** (the default, 19 actions): the test character starts on the ladder of a house in the south of
  the town (32109,32244,7); record_walk writes that into its player file (CurrentPosition) for the live
  recording and puts the file back afterwards. U: the ladder (Ctrl+right click on the player's own field,
  packet 0x82; moveuse.dat `Use, IsType (Obj1,1948) -> MoveRel(User,Obj1,[0,1,-1])`) to (32109,32245,6); N
  onto the trapdoor (32109,32244,6) down to the ladder's field; E, S onto the stairs (32110,32245,7) down to
  (32110,32246,8); N onto the stairs up (32110,32245,8) to (32110,32244,7); out of the house and south to
  (32107,32249,7). About 24 s live and 28 s in a replay (116 steps took 148 s and 168 s).
- **stairs** (44 steps, no use; `--walk stairs`): from (32095,32235,7) the ramp west of the town up to floor
  6 (32093,32235,7 to 32092,32235,6) and its stairs down (32093,32235,6 to 32094,32235,7), east to the same
  house, its stairs down and up, then south to (32107,32249,7). The fallback if the ladder does not work.
- Before #16: from the temple (32097,32219,7) 29 fields north, the wooden stairs up (32098,32190,7 to
  32098,32189,6), the hole next to them down, the stairs down (32096,32190,7 to 32096,32191,8) and up, then
  about 80 fields south to (32110,32252,7), with the wooden stairs and the hole once more: 116 steps.

Planned on the server's map: the sector files (gameserver\data\map), objects.srv's flags (a field to stand on
has a Bank object and no Unpass, Avoid or event object, at most one object with a Height and no event: the upper
half of a ramp), houses.dat (no house fields), moveuse.dat's `Collision, IsType (Obj1,T) ->
MoveTopRel(Obj1,[dx,dy,dz])` and `Use, IsType (Obj1,T) -> MoveRel(User,Obj1,[dx,dy,dz])` rules. A search over
every start on floor 7 (build\logs\issue-16\route-search.log) found: 14 steps from (32025,32242,7) west
of the town, among wolves and spiders (a fight would keep the character from logging out); 43 steps inside the
town (the stairs walk takes one more to start on plain ground); with a ladder 18 actions from (32054,32240,7),
nearer the wolves, and 19 from the house in the town, which the default takes. `python -I tools/compare_files.py --check-route` checks every walk against the map (each target field,
each floor change and its landing against floor_changes, the end, the automap blocks, the marks) without a
client; the old route passes the same check (the four blocks above). In the live session every action is
repeated until the player moved (read from the creature table); the replay takes the actions of the step
and use packets the recording holds, each repeated until the client sends its packet (a client sends no step
while a creature stands on the field, and when the creature moves on depends on when the key comes).

Clicks come from the clients' memory as in issue #19: the dialogs' members (TGeneralOptionsDialog's check
boxes at +0x2c-+0x3c, THotkeyDialog's AutoSendCheckBox +0x2458 and Selected +0x245c, TEditMarkDialog's
TypeButtons +0xb0, TAutomapWindow's zoom buttons +0x54/+0x58, ...), the window tree (tools/client_state.py
now names the option and mark dialogs, TCheckBox, TAutomapView, TAutomapWindow, TTab and TStartWindow by
their Tibia.exe vtables and reads TTextButton's text) and the context menu entries. A check box's state
(+0x30) is read after each click and must have flipped. FatalError's message box is found among the
process's windows (#32770) and closed with its one button, whose ID is IDCANCEL (an MB_OK box), not IDOK.

## Masks

Compared after masking, because two runs of the same client differ there (the script runs Tibia.exe a second
time for walk and shows that the two runs differ in exactly these places):

- `<channel>.txt`: "Channel saved at <ctime>".
- error.txt's ctime line, until issue #74 (since then only Tibia.exe writes the file, and it is not compared).

The masks accept only MSVC 7.1's form of ctime (the day with two digits): Patella's first run wrote
"Mon Nov  2" where Tibia.exe wrote "Mon Nov 02", and a looser mask hid that (below).

Not masked, until issue #74: the line numbers in error.txt's `<file> <line>`. Since issue #35 every error site
of our sources names the original's line (tools/lines.py checked them against Tibia.exe), so error.txt was
compared byte for byte apart from the time. Before #35 they were compared through a map (each differing pair inside the same
function, symbols.tsv's lines; the results below are of that run).

## Results (November 2, 2026)

Before issue #74; error, error-login and report then compared error.txt and the 0xe8 packet.

`python -I tools/compare_files.py` (out build\files\result, 12:36-12:54, 17 min 46 s): **no differences**.

| Scenario | Files written (bytes) | Result |
| :--- | :--- | :--- |
| options | Tibia.cfg (811; also after the options dialog) | identical; every option the dialogs offer changed (TestServer, FullscreenMode, WindowsMousepointer, LightEffects, TextualEffects, CreatureInfo, NoStretching, ConfineFramerate 198, five hotkeys, the console's five, ClassicControl, AutoChaseOff, ShowNewHints, Engine 1) |
| walk | 12512506.map, 12512507.map (131133: two marks; the deleted one is not written), 12512508.map, 12512607.map (131076 each), Default.txt (117), Tibia.cfg (751: AutomapZoom 1, ConsoleHeight, OpenDialogs) | identical; Default.txt after masking its time; the automap files and Tibia.cfg of both replays are also identical to those of the live run; 120 client packets identical |
| error | error.txt (657 / 654), Tibia.cfg | identical after masking the time and mapping four line numbers; the same message box, exit code 1 |
| error-login | error.txt (999 / 996: the earlier report and the new one) | the same |
| report | 12512507.map, Tibia.cfg; error.txt deleted by both | identical; the 0xe8 packet is the one computed from error.txt, the comment as "Comment: patella comment"; 5 client packets identical |
| patch | Tibia.dat, Tibia.spr (15,877,515), Tibia.pic (1,360,102), Patch.exe (962) | identical and as computed; the reconnects' login packets carry the new signatures; no temporary `_` file left |
| patch-exe | Tibia.bak (1,875,968) | identical and as computed; both close with exit code 0 |

The two runs of Tibia.exe (evidence for the masks): Default.txt's "Channel saved at Mon Nov 02 12:44:26 2026"
against "... 12:51:26 2026", error.txt's "Mon Nov 02 12:51:42 2026" against "... 12:52:17 2026" (error-login
likewise); nothing else differed between them.

Line numbers in error.txt (Tibia.exe -> Patella.exe; the function and its original lines from symbols.tsv):
Control.cpp 1274 -> 775 and 1208 -> 690 (the two TERRORs of ReceivePackets 0x44a900, lines 1208, 1274, 1280),
Control.cpp 1280 -> 780 (its TEND), Network.cpp 885 -> 679 (0x4ce530), Player.cpp 383 -> 388 (Run 0x4dae60).
Issue #35 makes them equal: those sites now name 1208, 1274, 1280, 885 and 383 (not run again in #35).

**Client bug found and fixed**: ctime's day. The first replays of the walk (before the fix) wrote "Channel
saved at Mon Nov 02 12:14:46 2026" (Tibia.exe) and "Channel saved at Mon Nov  2 12:18:13 2026"
(Patella.exe): MSVC 7.1's asctime (0x517f6d) writes the day with two digits, the UCRT with a space. A first
version of the mask accepted both forms and hid it; it was found reading the files. Fixed in src/
(src/compat/crt71.h, used by Player.cpp HandleException and ConsoleWindow.cpp SaveChannel); the masks now
accept only the two-digit form. Since issue #61 the client writes the text itself (`FormatCRT71Time`, std::format
on the C library's local time) instead of patching ctime's buffer. notes/decisions.md "Written files (issue #21)".

Harness problems found on the way (no client difference): a 256-byte memory read of a label's text that
crossed the end of a heap region (client_state.cstr now reads to the page end first); FatalError's box
ignored WM_COMMAND IDOK (its button is IDCANCEL); the replayed client walked one step early where the live
client had sent nothing for a key (a creature on the field); login_client's TCP table check missed an
established game connection twice; the error report dialog at the start took the login's clicks when
error.txt held a report.

**Not compared**: what patch.exe does after a Tibia.exe patch (CipSoft's separate program, not available;
above). The other option dialogs outside Options (VIP list, ignore and white lists, channels, the combat
controls) were not driven; their Tibia.cfg lines (Buddy, White, Ignore, OpenChannel, AttackMode, ChaseMode,
SecureMode) are written unchanged in every scenario. Other channel files than Default.txt use the same code
(the caption is the file name).

**Side effects**: one server start and clean stop per full run; four in all while this issue was worked on
(four live recordings, each logging in Patella Tester and walking; one stopped by a wrong expected position
in the script, one by the TCP table check; the player files are restored after each, as in issue #19). The
stops save the map and player files, as every clean stop does.
The login server bound 127.0.0.1:7171 every time. Only account 772001 was used.
