# Local 7.72 test server (issue #18)

A local server on 127.0.0.1 that the original Tibia.exe and Patella.exe both log into: login, character
list, entering the game, logout. Everything here stays on the machine; nothing is downloaded. Choices and
their reasons: notes/decisions.md, "Test server (issue #18)".

## The server

`C:\Tibia\gameserver` (installed by the user, not part of the repository; the environment variable
`PATELLA_TESTSERVER` names another folder for every tool, issue #2: tools/testserver.py's `SERVER_DIR`, which
compare_traffic.py, proxy.py, compare_files.py and the other comparisons take): `tibia-server.exe`,
a standalone Windows port of the decompiled CipSoft 7.72 server (Nottinghster's "tibia-gameserver-unified",
on Fusion32's decompilation; x64, MSVC 19.51). One process holds the login server (port 7171) and the game
server (port 7172); the accounts and characters are in SQLite (`tibia.db`), the world data in
`gameserver\data` (dat, map, mon, npc, usr, ...). Its README.md documents config.cfg, the database and the
accounts; its sources are in `gameserver\src`.

- **Start**: `python -I tools/testserver.py start` (from the repository or a worktree; Python since issue #10,
  before it tools\run-testserver.ps1, which did the same). Since Patella-Modern issue #146 only inside a run, on
  the run's private copy and its ports (below, "A private server per run"; the ports here are the master's,
  7171/7172); `start --master` for the master itself. Runs tibia-server.exe from `C:\Tibia\gameserver`
  (`PATELLA_TESTSERVER`)
  (it finds config.cfg, tibia.db and tibia.pem relative to its working directory) in a hidden console of its
  own, writes its output to `build\testserver\server.log` (overwritten at each start; stderr: server.err) and
  its PID (and the process's creation time) to `build\testserver\server.pid`, and returns when both ports
  listen and the world is loaded, server.log's `Server Online!` (Patella-Modern issue #68; about 5 s: the map,
  9,873 sectors and 8.9 million objects, loads in 2 to 3.3 s), printing `listening on 127.0.0.1:7171,
  127.0.0.1:7172` and `world loaded`. An attempt that stalls (no progress in server.log, the ports or the
  server's CPU time for `--stall`, 180 s; `Server Online!` without both ports `--after-online`, 60 s, later;
  `--timeout`, 900 s, in all) or exits is stopped (it is the one this start started) and the server started
  again, `--retries` (1) times, the failed attempt's log kept as `server-attempt<N>.log`. `--window` shows the
  console minimised instead; `--server-dir` for another copy. A bind error in server.log (Patella-Modern issue
  #95: `[ERR] LoginServerWorker: Failed to bind Login Server on port 7171 (error 10049)`, or the game server's
  `LaunchServer: Failed to bind Game Server on port 7172.`) fails the attempt at once, with the line, the error
  code's meaning and whether Windows excludes the port (`netsh int ipv4|ipv6 show excludedportrange
  protocol=tcp`); the attempt's server is stopped once it says `Server Online!` (the usual shutdown, not a Ctrl+C
  during the map's load) and started again, up to `--bind-retries` (3) times on top of `--retries` (below, "The
  bind race"). A failed attempt's server that never said `Server Online!` is killed 30 s after its Ctrl+C (it
  has nothing to save); one whose world is loaded saves every map sector on the way out and gets 600 s
  (Patella-Modern issue #137: on a busy machine the save took more than 30 s, the kill cut
  `data\map\1040-1009-10.sec` off at 4096 bytes, and every later start failed with "Cannot load sector").
  Refuses to start a second server or to start when
  7171/7172 is taken, also before a retry. Exit code 0 when the server is ready. `self-test` checks the wait on
  made-up servers (no server).
- **Stop**: `python -I tools/testserver.py stop`. Sends Ctrl+C to the server's console, the shutdown the
  server documents ("Press Ctrl+C to shutdown the server": stops the login server, logs everyone out and
  saves, closes the database), and waits for the exit. The Ctrl+C comes from a separate copy of the script
  started without a console, which attaches to the server's and ignores the event itself. Only the server
  `start` started is touched: the process in server.pid, if it is still `C:\Tibia\gameserver\tibia-server.exe`
  with the same creation time. A server the tools did not start (one started by hand) is never stopped: stop
  names its PID and exits with 1 (the former stop-testserver.ps1 also stopped a tibia-server.exe from that
  folder it found by name). Stop waits `--timeout` seconds for the exit: by default 600 once server.log shows the
  world loaded (the server saves its map on Ctrl+C, which can take minutes on a busy machine; a stop that gave up
  after 60 s failed compare_traffic in #141 while the server was still saving), else 30. `--force` kills the server
  after `--timeout` without saving: never on a server that is saving (#137 cut a sector file that way).
- **Ctrl+C must not be ignored by the server**: a process that ignores Ctrl+C (SetConsoleCtrlHandler(NULL,
  TRUE)) passes that to its children. A server started from Git Bash (whose processes ignore it) did not
  react to stop-testserver.ps1 at all; it was re-enabled with a remote thread calling
  SetConsoleCtrlHandler(NULL, FALSE) in it (build\scratch\enable_ctrlc.py) and then stopped cleanly.
  `start` clears the flag in its own process before starting the server (on Linux it resets SIGINT to its
  default), so the stop works from Git Bash and from PowerShell.
- The listening sockets come from GetExtendedTcpTable (the owner PID of each listening socket, IPv4 and IPv6;
  on Linux /proc/net/tcp and tcp6), the processes from their image paths: no PowerShell and no pip package.
  On Linux the executable is `tibia-server` in `--server-dir`, stopped with SIGINT.

### The bind race (Patella-Modern issue #95)

Now and then the login server does not bind 127.0.0.1:7171: server.log has `<date> <time> [ERR]
LoginServerWorker: Failed to bind Login Server on port 7171 (error 10049)` (WSAEADDRNOTAVAIL), the server goes on,
says `Server Online!` and listens on 7172 only. The cause is the server's own code (read from tibia-server.exe's
disassembly, read only; its sources are no longer in the folder), the same race as issue #19's 0.0.0.0:7171
(notes/traffic.md):

- main (0x140169120) passes StartLoginServer (0x140168040) a pointer to the global address buffer (char[16] at
  0x1401cd960, filled by the first ReadConfig) and the login thread is created with that pointer. The
  std::string LoginServerWorker (0x140168200) takes is built in the new thread (0x140166690: strlen, then a copy),
  that is, whenever the thread runs.
- Right after "Starting Login Server... [done]" the main thread's InitAll calls ReadConfig again ("Loading
  config... [config.cfg]"): it resets the buffer to "0.0.0.0" (strncpy of 8 bytes, 0x1400e8eb7), parses the file
  and copies bind_address over it again (strncpy of 15, 0x1400ed0f3); LoadWorldConfig then writes the world's
  address into it (`%d.%d.%d.%d`, 0x140119ed5, "127.0.0.1" again).
- LoginServerWorker binds INADDR_ANY when the string is "0.0.0.0" (or empty), else `inet_addr` of it. A thread
  that copies the buffer while it is "0.0.0.0" binds all interfaces (issue #19); one that copies it half-written
  gets a string such as "1.0.0.0", "120.0.0" or "127.0.0" (strlen of the old text, the copy of the new: the
  first 7 bytes of "127.0.0.1"), which is not this machine's: bind fails with 10049. inet_addr and bind of each
  of them on this machine: build\logs\issue-95\bind-probe.log of the issue's worktree (127.0.0.1, 0.0.0.0 and
  127.0.0.2 bind; 127.0.0.0, 120.0.0.0, 1.0.0.0, 23.0.0.1 and 255.255.255.255 fail with 10049).
- Not the cause: Windows' excluded port ranges (on November 6, 2026: 5357, 13508, 50000-50059, 55777-56463 in
  five blocks, 61555-61654; neither 7171 nor 7172, and an excluded port fails with 10013, not 10049); a TIME_WAIT
  socket of the previous server (that would be 10048, WSAEADDRINUSE, and only on the same address and port); the Docker,
  WSL (vEthernet 172.24.112.1) and VirtualBox (192.168.56.1) adapters (127.0.0.1 stays on the loopback
  pseudo-interface, 127.0.0.0/8; the failing address is not 127.0.0.1).

How often: 10 cold starts and stops in a row on November 6, 2026 (issue #95): 9 bound 127.0.0.1:7171 at once, 1
(the 7th) logged 10049 0.5 s after the start, while the main thread was at "Initializing database threads &
queues" (the error is printed into the middle of that line); its server was stopped once online and the second
attempt bound, 19.3 s in all; none bound 0.0.0.0. Issue #92's run had two such attempts in a row, so the race
comes back at random, about one start in ten here.

Only the server can fix it: a copy of the address into the thread before it starts (or a lock). Its binary and
code are the user's; tools/testserver.py retries instead (above), and compare_traffic.py still restarts a server
on 0.0.0.0:7171.

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

## A private server per run (Patella-Modern issue #146)

Every live run has a test server of its own; the tools never write `C:\Tibia\gameserver` (the master) again. Two
world files were cut on November 7 (#137, #123) by runs that shared the one server (a stop that killed it while it
saved its map), and every live run waited for that server. notes/decisions.md, "#146".

- **How a run gets one.** compare_traffic.py, compare_files.py (when it records its live walk) and testmode_check.py
  (its online runs) call `testserver.isolate()` first in their `__main__` block, before the Sandbox: it makes the
  run's copy, runs the same command line again with `PATELLA_TESTSERVER_RUN=<the run's folder>`, then stops the
  run's server if it still runs and removes the copy. Any other command: `python -I tools/testserver.py run [--keep]
  -- <command> [arguments]`. Inside such a run `testserver.SERVER_DIR` is the copy and the ports are the run's, so
  every tool takes them from there (compare_traffic.PLAYER_FILES and PEM, proxy.py's four ports, session_client's
  game port, login_client.py, the Sandbox's tunnel and mappings).
- **The copy** (`PATELLA_TESTSERVER_RUNS`, default `%TEMP%\patella-testservers\<date>-<time>-<PID>\server`, next to
  the run's instance.json, server.log, server.err and server.pid): config.cfg with the run's login_port and
  game_port, tibia.db with every world's Port set to the run's game port (the character list's), data\map, data\usr,
  data\dat and data\save copied (the server rewrites every map sector and about 520 player files on a clean stop,
  data\dat\owners.dat too, and the tools restore the test characters' player files), data\mon, data\npc and
  data\origmap as hard links to the master's files (read only: none of them was written in the six days of runs
  before this issue, their times are 2006's), data\log empty (the master's logs are 112 MB). Left out: the
  executable and its DLLs, the backup folder, tibia.db-shm. About 74,400 files (363 MB) copied and 10,400 hard links
  (182 MB) in 44 to 65 s, up to 105 s beside another run's copy; the first map load of a fresh copy takes 42 to 116 s
  (cold files), its restarts 3.5 to 8 s. A configuration whose files would not be the copy's (an absolute or `..` path in data_path,
  map_path, user_path..., database_file, rsa_key_file; a db_type other than sqlite) is refused before anything is
  made. When the master changed while it was copied (someone else's server saved it), the copy is made again.
- **The server.** The master's own tibia-server.exe runs, with the copy as its working directory (it finds
  config.cfg, tibia.db, tibia.pem and data relative to it): its path is the one Windows Firewall's rules name, so no
  firewall prompt comes up (a copied executable at a new path would ask), and nothing of it is copied. Its login
  server and game server listen on the run's ports. start and stop keep their rules (the 600 s wait once the map
  loads, never a kill of a server that saves, the bind race's retries); with the master's executable shared by
  several runs, start and stop know their server by the run's server.pid only (PID, path and creation time), and a
  run's stop never reports another run's server as "not started by the tools". Its log is kept in
  `build\testserver\runs\<run>` when the copy is removed.
- **The ports.** Four a run, one block of 7300-7499 (50 blocks): the login and game server's, and the proxy's pair
  (compare_traffic's and testmode_check's clients are patched to that login port; 7271/7272 without a run). A block
  is taken when no run's instance.json has it, nothing listens on its ports, they bind on 0.0.0.0 and none is in a
  Windows excluded range; the choice and the instance.json are made under a lock (RUNS_DIR\lock, released by Windows
  when its process ends).
- **A run's recording** names that run's proxy game port in its character list; replays rewrite it to their own (tools/proxy.py,
  issue #157), so it replays elsewhere, with no private server at all, unchanged.
- **Inside Windows Sandbox** the tunnel listens on the run's login and game port (the host writes them to
  tunnel.json in the exchange folder), so the character list's 127.0.0.1:<game port> is right inside too; the run's
  folder is mapped read-only and its copy's data\usr writable. The master is not mapped.
- **Cleanup.** At the end of a run its server, if still running, gets Ctrl+C and up to 600 s for its save; then the
  folder is removed file by file, never through a link or junction (a hard link's removal leaves the master's file
  as it is). A run killed midway leaves its folder (and maybe its server): the next run's start (or `python -I
  tools/testserver.py cleanup`) finds every folder whose owner process has exited (an exited process can still be opened while its parent holds a
  handle: its exit counts, not its PID), stops its server the same way (never
  killed: a server that does not end within 600 s is left for the next cleanup) and removes it. `instances` lists the
  runs' folders.
- **The master stays as it was.** The tools only read it (the copy, tibia.pem, compare_files --check-route's map).
  Every run compares the size and time of all its files before the copy and after the run and says "the master is
  unchanged", or names the files that changed (then another user of the master changed them: tools of a branch
  before #146, or the owner's own `start --master`). `python -I tools/testserver.py checksums --write FILE` /
  `--compare FILE` takes and compares the SHA-256 of every file. `start` without a run refuses; `start --master` runs
  the master's server itself, for the owner's own use (it writes the master, as before #146).
- **Disk**: about 550 MB a running copy (365 MB of it new data), so two runs at once take about 1.1 GB of `%TEMP%`'s
  drive (it must be the master's for the hard links; elsewhere they are copies).

## From a Linux container (issue #131)

The server stays a Windows program on the host, on the run's private copy (below), bound to 127.0.0.1. A container
reaches it through Docker Desktop's `host.docker.internal`, whose back end dials the host's localhost: measured with a
listener on 127.0.0.1 only, so no firewall rule and no change of the machine. Inside the container tools/linux_live.py
listens on 127.0.0.1 on the same ports and forwards to host.docker.internal, so the character list's world address
(127.0.0.1) reaches the server without touching the private copy's config.

## Pointing the clients at it: tools/patch_client.py

Both clients have the login servers and CipSoft's RSA modulus compiled in (src/Network.cpp InitServerList:
server.tibia.com, server2.tibia.com, tibia1.cipsoft.com, tibia2.cipsoft.com, port 7171, a random one first;
src/Communication.cpp: the modulus for the login and the game connection). The test server option is no way
in: Tibia.cfg's `TestServer = yes` only makes the client log in to TestServer, whose address Player.cpp sets
to test.cipsoft.com:7172 unconditionally (the "host"/"port" command-line values are dropped), and 7172 is
this server's game port. So the clients are patched like an "IP changer", on copies:

```
python -I tools/patch_client.py Tibia.exe build/run-orig/Tibia.exe --host 127.0.0.1 --rsa C:\Tibia\gameserver\tibia.pem
python -I tools/patch_client.py build/windows-x64/Release/PatellaTest.exe build/run-patella/PatellaTest.exe --host 127.0.0.1 --rsa C:\Tibia\gameserver\tibia.pem
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
  stores are then `mov dword ptr [rip+disp32], 7171` (`c7 05 <disp32> 03 1c 00 00`). Since issue #57 Patella.exe's
  stores are found through its PDB (Servers, TServer::Port); the Debug builds (windows-x64-debug,
  windows-x64-asan-debug: Patella-Modern issue #73) keep the loop and store through a register (`mov dword ptr
  [rax], 7171`, `c7 00 03 1c 00 00`), patched once, in the PDB's InitServerList. compare_traffic.py's
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

## Windows Sandbox (Patella-Modern issue #147)

Tibia.exe's live runs no longer use the user's desktop: compare_traffic.py, compare_files.py, compare_rendering.py and
compare_network.py run themselves inside Windows Sandbox by default (tools/sandbox.py; `--desktop` runs them here as
before), and `python -I tools/sandbox.py run -- <tool.py> [arguments]` runs any tool there. The Sandbox's window is
made on a hidden desktop of its own: nothing of it, of Tibia.exe or of the x64 client appears on the user's screen,
and nothing takes the focus. One Sandbox at a time: a second run waits for the first. notes/decisions.md, "#147".

**The limit is Windows' (measured in issue #154, November 8, 2026; Windows 11 Pro N build 26300, `wsb` 0.8.107.0).**
With no other Sandbox running, and sandbox.py's lock held so that none could start:

| second start, beside a running Sandbox | what Windows did |
|---|---|
| `WindowsSandbox.exe <file>.wsb` (as sandbox.py starts it) | The launcher exits at once, no second virtual machine, and `wsb list` still shows one ID. A new WindowsSandboxRemoteSession.exe opens a window titled "Windows Sandbox" with the text "Fel" and "Det går bara att köra en instans av Windows Sandbox-miljö åt gången." (Error: only one instance of Windows Sandbox can run at a time) and a "Stäng" button. It stays until it is closed. |
| `wsb start --id <guid> -c "<configuration>"` | Exit code 0x800401F6 and "Det går inte att köra programmet mer än en gång (0x800401F6 (CO_E_APPSINGLEUSE))". No window, no second ID. |

The first Sandbox was not disturbed either way (its heartbeat went on: 237 beats in 240 s after the refused start).
Because no second Sandbox ever started, separate network, mapped folders and GPUs per Sandbox do not arise. One
consequence for anyone starting one by hand beside a run: the refused start's error window is a
WindowsSandboxRemoteSession.exe, so sandbox.py's check for other Sandbox processes makes every later run wait until
it is closed. On a hidden desktop a WM_CLOSE did not close it; only ending the process did. Not tried: a Sandbox
started by each of the two ways at once, or from another Windows user's session. notes/decisions.md, "#154".

- **The server stays here.** The tool runs inside, with the clients and the proxy; the test server stays on the host
  (since issue #146 the run's private copy, below). `testserver.py start|stop` inside the Sandbox is passed to the
  host, which runs it here, so a run started inside starts and stops the host's server exactly as before (and one it
  leaves running is stopped when the run ends). A host-side live run (testmode_check's online part, headless) runs at
  the same time on a private server of its own.
- **The ports reach the Sandbox through a reverse tunnel.** Inside, 127.0.0.1:7171 and 127.0.0.1:7172 (since #146
  the run's login and game port, from tunnel.json) are the host's:
  the Sandbox listens on them and the host, which connected to the Sandbox at its start, opens each connection to its
  own 127.0.0.1 and passes the bytes. So the clients are patched to 127.0.0.1 (and the proxy's 7271/7272 inside the
  Sandbox are its own) as on the host, the character list's 127.0.0.1:7172 is right inside too, and the server keeps
  its bind_address 127.0.0.1. No firewall rule on the host (the Sandbox cannot connect to the host: its connections
  to the host's addresses time out); the Sandbox turns its own firewall off for the host's connection.
- **What a tool may read from the TCP table inside** (issue #161). A row's PID and ports are right, but its state can
  read wrong for the row's whole life (seen on Tibia.exe's game connection: 0, 1, 2, 6, about one login in seven). So
  login's wait also takes "a row to the game port and the player in the game", and a logout waits for the row to go.
  `python -I tools/sandbox_trial.py login|frame` repeats the login and the first start with the table's trace.
- **The files**: the run's folder (its private server: instance.json and the copy) is mapped read-only at its own
  path (tibia.pem, the map), the copy's data\usr writable (the tools restore the test characters' player files from
  inside); the master is not mapped (issue #146; until then its data\usr was writable). The server's own files are
  written only by the server on the host.

## Run folders

The clients run from `build\run-orig\` (patched Tibia.exe) and `build\run-patella\` (patched Patella.exe),
each with copies of Tibia.dat, Tibia.spr, Tibia.pic and Tibia.cfg (the client rewrites Tibia.cfg on exit;
never run a client from the repository root or C:\Tibia\Patella). Set up:

```
mkdir build\run-orig build\run-patella
copy Tibia.dat, Tibia.spr, Tibia.pic, Tibia.cfg into both
python -I tools/build.py windows-x64, then the two patch_client.py lines above
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
