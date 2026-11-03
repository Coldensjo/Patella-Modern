# Local 7.72 test server (issue #18)

A local server on 127.0.0.1 that the original Tibia.exe and Patella.exe both log into: login, character
list, entering the game, logout. Everything here stays on the machine; nothing is downloaded. Choices and
their reasons: notes/decisions.md, "Test server (issue #18)".

## The server

`C:\Tibia\gameserver` (gitignored, installed by the user, not part of the repository): `tibia-server.exe`,
a standalone Windows port of the decompiled CipSoft 7.72 server (Nottinghster's "tibia-gameserver-unified",
on Fusion32's decompilation; x64, MSVC 19.51). One process holds the login server (port 7171) and the game
server (port 7172); the accounts and characters are in SQLite (`tibia.db`), the world data in
`gameserver\data` (dat, map, mon, npc, usr, ...). Its README.md documents config.cfg, the database and the
accounts; its sources are in `gameserver\src`.

- **Start**: `powershell -NoProfile -ExecutionPolicy Bypass -File tools\run-testserver.ps1` (from the
  repository or a worktree). Runs tibia-server.exe from `C:\Tibia\gameserver` (it finds config.cfg,
  tibia.db and tibia.pem relative to its working directory) in a hidden console of its own, writes its
  output to `build\testserver\server.log` (overwritten at each start; stderr: server.err) and its PID to
  `build\testserver\server.pid`, and returns when both ports listen (about 5 s: the map, 9,873 sectors and
  8.9 million objects, loads in 3.3 s). `-Window` shows the console minimised instead; `-ServerDir` for
  another copy. Refuses to start a second server or to start when 7171/7172 is taken.
- **Stop**: `powershell -NoProfile -ExecutionPolicy Bypass -File tools\stop-testserver.ps1`. Sends Ctrl+C
  to the server's console, the shutdown the server documents ("Press Ctrl+C to shutdown the server": stops
  the login server, logs everyone out and saves, closes the database), and waits for the exit. The Ctrl+C
  comes from a separate hidden PowerShell that detaches from its own console, attaches to the server's and
  ignores the event itself. Only a process whose path is `C:\Tibia\gameserver\tibia-server.exe` is touched;
  `-Force` kills it after `-TimeoutSeconds` (60) without saving.
- **Ctrl+C must not be ignored by the server**: a process that ignores Ctrl+C (SetConsoleCtrlHandler(NULL,
  TRUE)) passes that to its children. A server started from Git Bash (whose processes ignore it) did not
  react to stop-testserver.ps1 at all; it was re-enabled with a remote thread calling
  SetConsoleCtrlHandler(NULL, FALSE) in it (build\scratch\enable_ctrlc.py) and then stopped cleanly.
  run-testserver.ps1 now clears the flag in its own process before starting the server; started from Git
  Bash and from PowerShell, the stop works.
- Both scripts look for a listening socket by remote port 0: on this Windows build (26300)
  Get-NetTCPConnection returns no State, so `-State Listen` matches nothing.

### Configuration

`C:\Tibia\gameserver\config.cfg`, one change; the original is kept as `config.cfg.orig` next to it and in
the backup:

| Setting | Was | Now | Why |
| :--- | :--- | :--- | :--- |
| `bind_address` | `"0.0.0.0"` | `"127.0.0.1"` | the login server listens on loopback only |

Left as they were:

- `world_address = "127.0.0.1"`: the address the character list sends. The world's Host in the database
  is `localhost`, which the server turns into 127.0.0.1 as well.
- `data_path = "../data"`: from the working directory `C:\Tibia\gameserver` that is `C:\Tibia\data`,
  which does not exist; the server then tries its built-in candidates and takes `data` (the first with a
  `dat` folder), `C:\Tibia\gameserver\data`. `data\ip-address.txt` (67.15.99.127) is not read by this
  server (no reference in its sources).
- `rsa_key_file = "tibia.pem"`: the server's own key; the clients are patched to it (below).
- The motd "Welcome to Tibia!" has no "number\n" prefix, so the clients show no message-of-the-day
  dialog (SetMotD in Preferences.cpp returns false).

**Not limited to localhost**: the game port is always bound to 0.0.0.0. bind_address only reaches the login
server: the game server's OpenSocket (src/game/communication.cc) binds INADDR_ANY, and LoadWorldConfig
overwrites the bind address with the world's Host after the login server has started. Windows Firewall
already had two inbound "Allow" rules for `C:\tibia\gameserver\tibia-server.exe` before this issue (made
when the server was first run); they were not changed. To keep 7172 off the network, remove or disable those
rules (or block the program) in Windows Defender Firewall; the server only needs loopback.

**What a run writes in gameserver\data**: every clean shutdown saves the world: all 9,873 sector files in
`data\map` are rewritten (the user's `data\map.bak` is the old copy) and player files in `data\usr`
(about 520 characters per shutdown, logged in `data\log\error.log` as "SavePlayerData: Failed to save
character N to database, falling back to legacy file"); the logs in `data\log` grow (game.log is
865 MB). The user's own run at 09:06, before this issue, did the same.

### Backup

`C:\Tibia\gameserver\backup-2026-11-02\`: tibia.db, tibia.db-wal, tibia.db-shm and config.cfg as they were
before the first start (sha256 of tibia.db 9d13e49b..., of tibia.db-wal 4b0ca9c0...). The -shm was copied
after a read-only sqlite3 query had opened the database (it only holds the WAL index; the database and WAL
files were unchanged, mtime 09:06). To restore: stop the server, copy the four files back.

### Test account

Account **772001**, password **patella** (free account), on world 1 (Zanera):

| CharacterID | Name | Sex |
| ---: | :--- | :--- |
| 15146574 | Patella Tester | male |
| 15146575 | Patella Second | male |

Created by `tools/testserver-account.sql` (`sqlite3 C:\Tibia\gameserver\tibia.db < tools\testserver-account.sql`,
server stopped), the INSERTs schema.sql uses for its sample account. Auth is the password as plain text:
schema.sql's form (SHA256(SHA256(password) XOR salt) and the salt, which Python verifies) was refused with
"Password is not correct."; the server's DB_VerifyPassword passes the shipped 111111/tibia only through a
special case for its first four bytes, and also accepts plain text. A new character has no saved state;
on its first login the server uses its defaults (no vocation, Rookgaard) and on logout writes
`data\usr\74\15146574.usr` (StartPosition and CurrentPosition [32097,32219,7], Rookgaard's temple). Player
state goes to these legacy .usr files although config.cfg says `player_storage_mode = "database"`: no
PlayerState table exists in tibia.db. The character list is ordered by level and last login (Patella
Tester, level 1 after its first login, before Patella Second, level 0), then the server's "Account
Manager" entry (account_manager defaults to on).

The shipped accounts 111111-666666 (password tibia, thousands of RealOTs characters each) were not
changed, but one login with 111111 was made while finding out why 772001 failed: its first character,
"Savage Saint" (CharacterID 8395503, level 152), was in the game for a few seconds (the client was killed,
the server logged it out at its shutdown). The server rewrote `data\usr\03\8395503.usr` (149,619 bytes,
new last login/logout times) and Characters.LastLoginTime; there is no copy of the old .usr file
(`data\usr.bak` has .dat/.obj files only), the database backup has the old row. Use 772001.

## Pointing the clients at it: tools/patch_client.py

Both clients have the login servers and CipSoft's RSA modulus compiled in (src/Network.cpp InitServerList:
server.tibia.com, server2.tibia.com, tibia1.cipsoft.com, tibia2.cipsoft.com, port 7171, a random one first;
src/Communication.cpp: the modulus for the login and the game connection). The test server option is no way
in: Tibia.cfg's `TestServer = yes` only makes the client log in to TestServer, whose address Player.cpp sets
to test.cipsoft.com:7172 unconditionally (the "host"/"port" command-line values are dropped), and 7172 is
this server's game port. So the clients are patched like an "IP changer", on copies:

```
python -I tools/patch_client.py Tibia.exe build/run-orig/Tibia.exe --host 127.0.0.1 --rsa C:\Tibia\gameserver\tibia.pem
python -I tools/patch_client.py build/Release/Patella.exe build/run-patella/Patella.exe --host 127.0.0.1 --rsa C:\Tibia\gameserver\tibia.pem
```

- The four server names are replaced in place by 127.0.0.1, NUL-padded to their own length (both clients
  copy each literal with a fixed number of bytes: mov/movups of the whole literal), so all four entries
  point at the test server whichever is tried first.
- **RSA**: tibia.pem is not CipSoft's key: its modulus is the OpenTibia one (309 digits, starting
  10912013..., exponent 65537), the client's is CipSoft's (309 digits, 12471045...). We do not have
  CipSoft's private key, so the clients' modulus text is replaced (same length, no padding needed);
  `--rsa` takes the PEM (RSA private/public key, PKCS#1, PKCS#8 or X.509) or a decimal number and checks
  the exponent.
- `--port N` changes the immediate 7171 in InitServerList (once in Tibia.exe, the loop's `mov ecx, 7171`;
  four times in Patella.exe, whose compiler unrolled the loop), found in the 0x200 bytes after the first code
  reference to the server.tibia.com literal: on x86 its absolute address, on x64 (Patella-Modern issue #15) a
  RIP-relative disp32 after a ModRM byte with mod 00, r/m 101 whose target is inside the literal; the four
  stores are then `mov dword ptr [rip+disp32], 7171` (`c7 05 <disp32> 03 1c 00 00`). compare_traffic.py's
  clients use `--port 7271` (the proxy).
- Everything is found by searching the file (no fixed offsets); PE32 (x86) and PE32+ (x64) images alike; the
  input is never written; afterwards the output is compared with the input byte by byte and must differ only
  inside the patched ranges (else the output is deleted and the script fails).

| | Tibia.exe (file offset, VA) | Patella.exe (file offset, VA) |
| :--- | :--- | :--- |
| server.tibia.com | 0x16d374, 0x56d374 | 0x2cd56c, 0x6ce56c |
| server2.tibia.com | 0x16d360, 0x56d360 | 0x2cd580, 0x6ce580 |
| tibia1.cipsoft.com | 0x16d34c, 0x56d34c | 0x2cd594, 0x6ce594 |
| tibia2.cipsoft.com | 0x16d338, 0x56d338 | 0x2cd5a8, 0x6ce5a8 |
| RSA modulus | 0x15b620, 0x55b620 | 0x2bbb78, 0x6bcb78 |
| bytes changed | 344 | 344 |

(Patella.exe's offsets are those of the build from 8ac8a06; it is ASLR-relocatable, the VAs are at its
preferred base 0x400000.)

The x64 build (build\windows-x64\Release\Patella.exe of 21a40b1, preferred base 0x140000000; issue #15): the
names at file offsets 0x323d48, 0x323d60, 0x323d78, 0x323d90, the modulus at 0x30d7a0, the first reference to
server.tibia.com at 0x84aa3 (`movups xmm0, [rip+29fea1h]`), the four port immediates at 0x84b20, 0x84b31,
0x84b42, 0x84b53; 344 bytes changed without `--port`, 348 with it, as in the oracle-x86 build.

## Run folders

The clients run from `build\run-orig\` (patched Tibia.exe) and `build\run-patella\` (patched Patella.exe),
each with copies of Tibia.dat, Tibia.spr, Tibia.pic and Tibia.cfg (the client rewrites Tibia.cfg on exit;
never run a client from the repository root or C:\Tibia). Set up:

```
mkdir build\run-orig build\run-patella
copy Tibia.dat, Tibia.spr, Tibia.pic, Tibia.cfg into both
tools\build-patella.cmd, then the two patch_client.py lines above
```

`WindowedMode` in the copied Tibia.cfg was set to (200,200,800,600,no); with the monitor off (below) the
screen was 1024x768, the client found that window off the screen and used its default, a 600x450 client
area centred, which it then saved.

## Login automation: tools/login_client.py

```
python -I tools/login_client.py build\run-orig Tibia.exe
python -I tools/login_client.py build\run-patella Patella.exe
```

Defaults: account 772001, password patella, 5 s in the game. Starts the client in its run folder, finds its
window (class TibiaClient) and drives it with posted window messages, so it needs neither the focus nor the
mouse and works the same on both clients (MainFrm.cpp takes WM_LBUTTON*/WM_KEYDOWN/WM_CHAR from the queue,
the modifier state from WM_KEYDOWN):

1. Start screen: a click on "Enter Game". The start window ignores keys while disconnected
   (TMainWindow::KeyPress), so there is no keyboard way; the button's place follows from the skin
   (GUI.cpp Style0D8: frame at (60, height-240), border 10, the 86-wide button at the panel's top left),
   about (113, height-220) in the client area; `--enter-game X,Y` overrides it.
2. Login dialog: the account (WM_CHAR per digit), Tab (TEditLine::KeyPress: next in the focus group), the
   password, Return.
3. Character list (and the message of the day if a server sends a new one): Return until the client holds
   an established TCP connection to port 7172 (GetExtendedTcpTable, by PID), at most three times. Return
   takes the highlighted character, the first.
4. In the game: Return after 2 s (closes the client's "First Login" hint, which a fresh Tibia.cfg shows and
   which would take the next key), `--stay` seconds, Ctrl+L (WM_KEYDOWN VK_CONTROL, WM_CHAR 0x0c, WM_KEYUP:
   hotkey 0x44c, MainWindow event 0x1fa, SendLogout); the game connection must close within 10 s (once more
   Return and Ctrl+L if not). The client then shows the character list again.
5. WM_CLOSE (not in the game, so CloseRequest lets it close) and the process must exit.

Screenshots `1-start.png`, `2-login.png`, `3-charlist.png` (`3-dialogN.png` for earlier dialogs),
`4-game.png` and `5-logout.png` go into the run folder: the screen under the window (BitBlt of the screen
DC), with the window made topmost for the moment without activating it; PrintWindow (also with
PW_RENDERFULLCONTENT) returns a white client area for the clients' DirectX/OpenGL surface. Exit code 0 when
every step passed; on a failure the client it started is killed. `--shot-only` takes 1-start.png and
closes; `--keep-open` leaves the client running.

## What happened (November 2, 2026)

Server started with run-testserver.ps1 and stopped with stop-testserver.ps1 five times; every stop was
clean ("Server shut down successfully."), one after re-enabling its Ctrl+C (above). It is stopped now. Server log lines per login: `HandleLoginRequest: RSADecrypt
return=1` (the patched modulus works), `Login request from 127.0.0.1: Account=772001`, `JoinGame:
Initializing CharacterID=15146574`.

| | Tibia.exe (build\run-orig) | Patella.exe (build\run-patella) |
| :--- | :--- | :--- |
| login, character list | yes | yes |
| game connection (7172 established) | yes | yes |
| logout, back to the character list | yes | yes |
| WM_CLOSE, exit code | 0 | 0 |
| full runs of login_client.py | 3 passed (after 2 failed ones while it was written) | 3 passed |

The character list and the game were also checked from the clients' memory (a scratch probe,
build\scratch\probe6.py, ReadProcessMemory: ModalWindow and Status.cpp's ConnectionState, Tibia.exe at
0x5d16ac/0x71c588, Patella.exe at RVA 0x37cd58/0x4c86e0 from its PDB): both show the "Select Character"
dialog after the login (Tibia.exe's lines "Patella Tester (Zanera)", "Patella Second (Zanera)"),
connection state 8 (in the game) with no dialog after the hint, and the "Select Character" dialog again,
state 0, after Ctrl+L. Output: build\run-orig\memory-evidence.txt, build\run-patella\memory-evidence.txt.

**Screenshots**: build\run-orig\*.png and build\run-patella\*.png exist but show a white client area. The
user's monitor was off for the whole run: the primary display had fallen back to 1024x768 with no monitor
present (it was 5120x1440 when the work began), so DWM composed nothing (Desktop Duplication delivered no
frame either) and no capture method can see the clients' surfaces. Run the two login_client.py lines again
with the monitor on to get real ones.

### Differences between the clients

None in behaviour. After the same first login and logout both clients wrote byte-identical Tibia.cfg files
(KnownHints {1,2}, OpenDialogs 3/4/1/5, ConsoleHeight 132, the window placement). Two things looked like
differences and are not:

- On the very first login of Patella Tester (Tibia.exe's run) the first Ctrl+L did not log out, the second
  did; Patella.exe's first Ctrl+L worked. With fresh Tibia.cfg files both clients logged out on the first
  Ctrl+L, so this was the timing of the First Login hint on the character's first login, not the client.
- One later pair of fresh runs saved KnownHints {1} (Tibia.exe) and {1,2} (Patella.exe). Event help 2 is
  queued when the creature "Cipfried" comes into view (Help.cpp CheckEventHelpName), the temple NPC who
  walks around: world state, not the client.

No issue was opened.

## Next

Issue #19 can reuse all of this: run-testserver.ps1/stop-testserver.ps1, the run folders and
login_client.py (`--keep-open` to keep a client in the game). tibia.pem is the private half of the key
the patched clients encrypt with, so a recorder can decrypt the login packets' RSA block, take the XTEA
key from it and decrypt the rest of both connections ("compare after decryption").

Done in issue #19 (notes/traffic.md). Found there: the server's login thread sometimes binds 0.0.0.0:7171
in spite of bind_address (a start-up race in the server's code, notes/traffic.md);
tools/compare_traffic.py restarts the server when run-testserver.ps1 reports it.
