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
| error.txt | Player.cpp HandleException (appends a report); deleted by Control.cpp SendErrorReports on entering the game, or the error report dialog's Cancel (Status.cpp DeleteErrorFile) | a TMyException that reaches InitInstance or Run |
| `<channel>.txt` | ConsoleWindow.cpp SaveChannel (appends) | "Save Window" in a console tab's menu |
| Tibia.da_, Tibia.sp_, Tibia.pi_, Patch.ex_; Tibia.bak | Script.cpp ApplyPatch / ApplyFullPatch | a patch from the login server (0x1e difference patch, 0x1f whole file, 0x20 blocks; Communication.cpp ReceivePatch*) |
| Tibia.dat, Tibia.spr, Tibia.pic, Patch.exe (replaced) | Script.cpp InstallPatch (remove, rename `_` to the name) | messages 0x41-0x44 after the patch, once the login connection is closed |

The login server decides about patches: a 7.72 server can send them in answer to any login (the client's
first packet carries the three files' signatures), so a crafted replay of the login connection triggers
every one of them. A Tibia.exe patch is written to Tibia.bak; the client then shows "Updating", closes
and runs patch.exe (Player.cpp Run, `_execl`), which is CipSoft's separate patcher and not part of the
client: what it does with Tibia.bak cannot be compared here (we do not have it, and running a program the
session wrote is out of the question). The scenario leaves patch.exe out, so `_execl` fails and the client
just ends.

## Running it

```
tools\build-patella.cmd
python -I tools/compare_files.py [--out build\files\result] [--only options,walk,...] [--skip-live] [--no-repeat] [--patella EXE]
```

`--patella` names the Patella.exe to test, x86 or x64, with its PDB next to it (default build\Release\Patella.exe;
as compare_traffic.py's; Patella-Modern issue #15).

The test server must be stopped; the script starts it for the one live recording and stops it (a clean stop
saves, as always) and puts the test characters' player files back, as compare_traffic.py does. Everything
else runs against tools/proxy.py's replay, without a server. About 18 minutes for all scenarios. Exit code 0
when nothing differs. `--skip-live` reuses `--out`'s walk-live.jsonl (no server); `--no-repeat` skips the
second run of Tibia.exe that shows what the masks hide.

The run folders are build\files\orig and build\files\patella (compare_traffic.py's set-up, via its new
set_folder: copies of the data files and the patched clients). Before each run the folder is reset: the
data files as in the root, compare_traffic.py's Tibia.cfg, nothing else but the client (and in Patella's
folder a copy of the patched Tibia.exe, the input of the Tibia.exe patch). Results in `--out`:
`<scenario>\<client>\` (the files the run wrote), `<scenario>\<client>.jsonl` (the replay's packets),
`repeat\<scenario>\orig\` (the second Tibia.exe run), walk-live.jsonl and walk-live\orig\ (the live
recording and its files) and report.txt.

## The scenarios

| Scenario | Session | Server data |
| :--- | :--- | :--- |
| options | start window: Options; General (all five check boxes), Console (all five), Network (Test Server Login), Graphics (the next fullscreen resolution), Advanced (light, stretching, mouse pointer, the frame rate limit three steps down, the OpenGL engine: a live engine switch), Hotkeys (F1-F5: two texts, two sent automatically, one empty); OK everywhere; Tibia.cfg after the options dialog and after the exit | none |
| walk | login; automap: three marks set (types 3, 9, none), one edited (text and type), one deleted; zoom in once, out twice; 116 steps over floors 7, 6, 8, 7, 6, 7 into a second automap block; "Save Window" of the default channel; logout | the live recording (Tibia.exe against the test server), replayed |
| error | login, then the server packet 0xff (unknown type in the game): HandleException, the critical error box (its OK is pressed), exit code 1 | the recording's login and first game packets, then the crafted packet |
| error-login | error.txt holds an earlier report: the error report dialog at the start (a comment, Send); login; the login server answers 0x63 (unknown type at the login): the report is appended | crafted login answer |
| report | error.txt is Tibia.exe's from "error" in both folders: the dialog (a comment, Send), login; on entering the game the client sends the report (0xe8) and deletes error.txt; logout | the recording's login and first game packets; the crafted script expects the 0xe8 packet computed from error.txt (independently of the clients) and the logout, then closes |
| patch | login; four login connections in a row: a difference patch of Tibia.dat (all four operations copy/skip/insert/replace, all count forms: in the command byte, byte, word and quad), of Tibia.spr, the whole Tibia.pic and the whole Patch.exe; each installed, then the client reconnects by itself, with the new signatures in its login packet | crafted, from the recording's first login packet |
| patch-exe | login; a difference patch of Tibia.exe (eight bytes of the DOS stub) to Tibia.bak; "Updating" (OK), the client closes | crafted |

Every patch writes a new signature into its file (or eight bytes of Tibia.exe), so a patch that silently did
nothing would be seen; the script also checks each patched file against the result it computes itself.

Walking (tools/compare_files.py ROUTE): from the temple of Rookgaard (32097,32219,7), where Patella Tester
starts, 29 fields north, the wooden stairs up (32098,32190,7 to 32098,32189,6), the hole next to them down
(32098,32190,6 to 32098,32191,7), the stairs down (32096,32190,7 to 32096,32191,8), back up (32096,32190,8 to
32096,32189,7), then south to (32110,32252,7), where the map shows y 32256 and more (block 125/126/7); the
shortest way south goes up the wooden stairs and down the hole once more. Planned on the server's map
(gameserver\data\map sector files, objects.srv's Bank/Unpass/Avoid flags and moveuse.dat's
`Collision, IsType (Obj1,T) -> MoveTopRel(Obj1,[dx,dy,dz])` rules; build\scratch\plan_tour.py in the i21
worktree). In the live session every arrow key is repeated until the player moved (read from the creature
table); the replay presses the directions of the step packets the recording holds, each repeated until the
client sends its step (a client sends no step while a creature stands on the field, and when the creature
moves on depends on when the key comes).

Clicks come from the clients' memory as in issue #19: the dialogs' members (TGeneralOptionsDialog's check
boxes at +0x2c-+0x3c, THotkeyDialog's AutoSendCheckBox +0x2458 and Selected +0x245c, TEditMarkDialog's
TypeButtons +0xb0, TAutomapWindow's zoom buttons +0x54/+0x58, ...), the window tree (tools/client_state.py
now names the option and mark dialogs, TCheckBox, TAutomapView, TAutomapWindow, TTab and TStartWindow by
their Tibia.exe vtables and reads TTextButton's text) and the context menu entries. A check box's state
(+0x30) is read after each click and must have flipped. FatalError's message box is found among the
process's windows (#32770) and closed with its one button, whose ID is IDCANCEL (an MB_OK box), not IDOK.

## Masks

Compared after masking, because two runs of the same client differ there (the script runs Tibia.exe a second
time for walk, error and error-login and shows that the two runs differ in exactly these places):

- error.txt: the line after "Debug Assertion ...", ctime of the crash.
- `<channel>.txt`: "Channel saved at <ctime>".

The masks accept only MSVC 7.1's form of ctime (the day with two digits): Patella's first run wrote
"Mon Nov  2" where Tibia.exe wrote "Mon Nov 02", and a looser mask hid that (below).

Not masked: the line numbers in error.txt's `<file> <line>`. Since issue #35 every error site of our sources
names the original's line (tools/lines.py checks them against Tibia.exe), so error.txt is compared byte for
byte apart from the time. Before #35 they were compared through a map (each differing pair inside the same
function, symbols.tsv's lines; the results below are of that run).

## Results (November 2, 2026)

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
accept only the two-digit form. notes/decisions.md "Written files (issue #21)".

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
