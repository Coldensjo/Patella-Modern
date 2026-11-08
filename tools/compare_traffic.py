r"""Compare the network traffic of Patella.exe and the original Tibia.exe (issue #19).

Usage (from the repository or a worktree; the test server of issue #18 must be stopped):
  python -I tools/compare_traffic.py [--out build\traffic\result] [--skip-live] [--no-cross]
  python -I tools/compare_traffic.py record|compare [tools/reference.py's options]   (issue #23: the
      reference recordings of this tool's sessions; notes/testmode.md, "Reference recordings")

Runs the scripted session of tools/session_client.py with both clients and compares:

1. Live: for each client (Tibia.exe first, then Patella.exe) the test characters' player
   files are restored to what they were when the script started, the server is started
   (tools/testserver.py start: both ports and the loaded world, issue #68), the client under test is started, then the helper (a third
   copy of Tibia.exe, started with "gamemaster" so that it skips the one-client check; this
   server ignores the flag) logs "Patella Second" in, the trade partner. The client under test
   goes through tools/proxy.py (recording, decryption) and plays the session as "Patella
   Tester"; then both log out and the server is stopped (it saves). Result:
   <out>\<client>-live.jsonl.
2. Replay: each client runs the session again against tools/proxy.py in replay mode, which
   plays the server side of Tibia.exe's live recording, then of Patella.exe's (with
   --no-cross only Tibia.exe's), to it. No server is needed and both clients get the same
   server data byte for byte.
   Result: <out>\<client>-replay-of-<source>.jsonl and the client state at every step.
3. Compare (the report goes to <out>\report.txt and the console):
   a. client packets of the two live sessions: identical in order and content (the first
      packet of each connection with the XTEA key and the RSA block's random bytes masked;
      every later packet decrypted, without its padding). The masked bytes are checked
      instead: in every first packet of every run the key and the random tail must be one
      run of MSVC rand() results (proxy.rand_stream), and the keys must differ;
   b. client packets of every replay against the recording it replayed: identical (the
      proxy checks each one as it comes);
   c. the client state (tools/client_state.py: player, map, creatures, inventory, containers,
      trade, console) and the window tree (classes, places, sizes, label texts) after every
      step, Tibia.exe against Patella.exe in the replay of the same recording: identical (the x64
      client's from its test mode's probe, below);
   d. the options each client wrote when it closed: equal across all runs of the same kind,
      by value (Patella-Modern issue #45: Tibia.exe's Tibia.cfg, the x64 client's Patella.ini;
      tools/settings_file.py);
   e. server packets of the two live sessions: listed, not required to be equal (the
      server's world moves on its own: creatures walk, times differ); the replay shows that
      the clients handle the same server data the same way.
Exit code 0 when a-d found no difference, 1 otherwise (or on a failed run).

--patella: the x64 Patella.exe to test (default build\windows-x64\Release\Patella.exe; python -I tools/build.py
windows-x64 builds it). The client under test is the test build next to it, PatellaTest.exe (Patella-Modern issue
#92: the same client with the test mode compiled in; without a script it runs on the system clock and takes
Windows' input, as Patella.exe), started with its control channel and read through it (tools/testmode_client.py:
the probe's state, window tree and the places the session clicks at), never through its memory:
tools/client_state.py reads Tibia.exe only. The 32-bit oracle's Patella.exe, which was the default until issue #94
and was read through its memory with its PDB, left the fork with the oracle.
"""

import argparse
import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import desktop  # noqa: E402
import proxy as px  # noqa: E402
import sandbox  # noqa: E402
import session_client as sc  # noqa: E402
import settings_file  # noqa: E402
import login_client as lc  # noqa: E402
import testserver  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_DIR = testserver.SERVER_DIR  # PATELLA_TESTSERVER, else C:\Tibia\gameserver (testserver.py)
PEM = os.path.join(SERVER_DIR, "tibia.pem")
SERVER_EXE = os.path.join(SERVER_DIR, "tibia-server.exe")
PLAYER_FILES = {  # the test account's characters (notes/testserver.md)
	"Patella Tester": os.path.join(SERVER_DIR, "data", "usr", "74", "15146574.usr"),
	"Patella Second": os.path.join(SERVER_DIR, "data", "usr", "75", "15146575.usr"),
}
CLIENTS = {
	"orig": {"exe": "Tibia.exe", "source": os.path.join(ROOT, "Tibia.exe"), "pdb": None, "test": False},
	# The x64 client, run as its test build next to it (use_patella; Patella-Modern issues #92, #94).
	"patella": {"exe": "PatellaTest.exe", "source": os.path.join(ROOT, "build", "windows-x64", "Release",
		"PatellaTest.exe"), "pdb": None, "test": True},
}
DEFAULT_PATELLA = os.path.join(ROOT, "build", "windows-x64", "Release", "Patella.exe")
TEST_BUILD = "PatellaTest.exe"  # the x64 client's test build, next to its Patella.exe (cmake/WindowsX64.cmake)
IMAGE_FILE_MACHINE_AMD64 = 0x8664


def pe_machine(path):
	"""The machine field of a PE image's file header (client_state.pe_machine, without its Windows imports)."""
	with open(path, "rb") as f:
		head = f.read(0x400)
	pe = int.from_bytes(head[0x3c:0x40], "little") if head[:2] == b"MZ" else -1
	if pe < 0 or head[pe:pe + 4] != b"PE\0\0":
		raise RunError("%s: not a PE image" % path)
	return int.from_bytes(head[pe + 4:pe + 6], "little")


def use_patella(path):
	"""--patella: the client under test, an x64 Patella.exe: the test build next to it runs in its place, read
	through its control channel (issue #92). A 32-bit one (the oracle's, read through its memory with its PDB until
	issue #94) is refused. Returns the line the report names it with."""
	path = os.path.abspath(path)
	if not os.path.isfile(path):
		raise RunError("no %s (python -I tools/build.py windows-x64 builds it)" % path)
	info = CLIENTS["patella"]
	if pe_machine(path) != IMAGE_FILE_MACHINE_AMD64:
		raise RunError("%s is not an x64 client: the fork's only client is the x64 Patella.exe, read through its test "
			"build (the 32-bit oracle left with Patella-Modern issue #94)" % path)
	test = os.path.join(os.path.dirname(path), TEST_BUILD)
	if not os.path.isfile(test):
		raise RunError("no %s next to %s: the x64 client is read through its test build (python -I tools/build.py "
			"windows-x64 builds both)" % (TEST_BUILD, path))
	info.update(exe=TEST_BUILD, source=test, pdb=None, test=True)
	return "patella: %s, run as its test build %s (read through its test mode's control channel)" % (path, test)
HELPER = {"exe": "Tibia.exe", "source": os.path.join(ROOT, "Tibia.exe")}
TRAFFIC = os.path.join(ROOT, "build", "traffic")
LIVE_SETTLE = 1.5  # seconds after each live step until issue #144 (compare_files.py's live walk still uses it)
# Issue #144: after each live step, until no server packet came for LIVE_QUIET seconds, counted from the step's end
# at the earliest (the server answers in milliseconds; its world's own packets, a creature walking, come seconds
# apart); a server that is not quiet within LIVE_QUIET_TIMEOUT fails the run.
LIVE_QUIET = 0.5
LIVE_QUIET_TIMEOUT = 30


def live_wait(proxy):
	"""run_session's wait for a live run through proxy (issue #144: instead of a fixed LIVE_SETTLE)."""
	def wait(name):
		start = time.time()
		while time.time() - max(proxy.last_s2c, start) < LIVE_QUIET:
			if time.time() - start > LIVE_QUIET_TIMEOUT:
				raise RunError("%s: the server sent packets for %d s without a pause of %.1f s" % (name,
					LIVE_QUIET_TIMEOUT, LIVE_QUIET))
			time.sleep(0.05)
	return wait

# The Tibia.cfg every run starts with: a window that fits a 1024x768 screen (the size the
# display falls back to with the monitor off), no hint dialogs (they would take keys).
TIBIA_CFG = """# Tibia - graphical Multi-User-Dungeon
# DO NOT EDIT THIS FILE

TestServer = no
WindowsMousepointer = yes
LightEffects = yes
TextualEffects = yes
CreatureInfo = 2
NoStretching = no
ConfineFramerate = 0
WindowedMode = (10,10,800,600,no)
ConsoleHeight = 0
AutomapZoom = 2
BuddySortmethods = (2,1,0)
BuddyHideOffline = no
LastMotD = 0
InfoMessages = no
EventMessages = yes
StatusMessages = no
TimeStamps = no
PrivateMessages = yes
ClassicControl = no
AutoChaseOff = yes
AttackMode = 2
ChaseMode = 0
SecureMode = 1
White = {}
Ignore = {}
KnownHints = {}
ShowNewHints = no
LevelBar = yes
MagLevelBar = yes
SkillBars = {0,1,2,3,4,5,6}
Engine = 2
"""


class RunError(Exception):
	pass


def log(text):
	lc.log(text)


# --- set-up ------------------------------------------------------------------------------

def set_folder(path):
	"""Use other run folders (and server-scripts.log) than build\\traffic: tools/compare_files.py
	(issue #21) runs its sessions in build\\files with the same set-up."""
	global TRAFFIC
	TRAFFIC = path


def run_dir(name):
	return os.path.join(TRAFFIC, name)


DATA_FILES = ("Tibia.dat", "Tibia.spr", "Tibia.pic")
RUN_EXES = {}  # run folder -> the client's file name in it, filled by setup()
ROOT_DATA = {}  # data file -> sha256 of the repository root's copy


def client_of(name):
	"""The client a run folder is for: "orig" for orig, orig-repeat, orig-2; "patella"; "helper"."""
	return name.split("-")[0]


def setup(extra=()):
	"""Run folders with copies of the data files and the patched clients (tools/patch_client.py):
	the clients under test log in through the proxy's port, the helper straight to the server.
	extra: more run folders for the clients under test (orig-repeat: a second run of Tibia.exe gets
	a folder of its own, issue #12)."""
	# the helper's login port: the server's (7171 needs no patch; a run's private server has its own, issue #146)
	runs = [("orig", CLIENTS["orig"], px.PROXY_LOGIN_PORT), ("patella", CLIENTS["patella"], px.PROXY_LOGIN_PORT),
		("helper", HELPER, None if px.SERVER_LOGIN_PORT == 7171 else px.SERVER_LOGIN_PORT)]
	runs += [(name, CLIENTS[client_of(name)], px.PROXY_LOGIN_PORT) for name in extra]
	for name, info, port in runs:
		folder = run_dir(name)
		os.makedirs(folder, exist_ok=True)
		for data in DATA_FILES:
			target = os.path.join(folder, data)
			source = os.path.join(ROOT, data)
			if not os.path.isfile(target) or os.path.getsize(target) != os.path.getsize(source):
				shutil.copyfile(source, target)
		RUN_EXES[name] = info["exe"]
		exe = os.path.join(folder, info["exe"])
		if not os.path.isfile(info["source"]):
			raise RunError("%s not found (build it with python -I tools/build.py windows-x64)" % info["source"])
		if os.path.isfile(exe):
			os.remove(exe)
		command = [sys.executable, "-I", os.path.join(ROOT, "tools", "patch_client.py"), info["source"], exe,
			"--host", "127.0.0.1", "--rsa", PEM]
		if port:
			command += ["--port", str(port)]
		result = subprocess.run(command, capture_output=True, text=True)
		if result.returncode != 0:
			raise RunError("patch_client.py failed for %s: %s" % (name, result.stderr.strip()))


def file_sha(path):
	digest = hashlib.sha256()
	with open(path, "rb") as f:
		for chunk in iter(lambda: f.read(1 << 20), b""):
			digest.update(chunk)
	return digest.hexdigest()


def clean_run_dir(name, keep=()):
	"""Empties the run folder before a run (issue #12): everything but the data files, the patched
	client and the names in keep is deleted, files and folders alike (the automap's *.map files,
	error.txt, Tibia.cfg, channel files, what an earlier failed run or a stray click left), and a data
	file that differs from the repository root's (a patch scenario rewrites them) is copied again.
	The caller writes Tibia.cfg afterwards. Returns the names it removed (and the data files it
	copied again)."""
	if name not in RUN_EXES:
		raise RunError("run folder %s was not set up (setup(extra=...))" % name)
	folder = run_dir(name)
	exe = RUN_EXES[name]
	if not os.path.isfile(os.path.join(folder, exe)):
		raise RunError("%s has no %s (setup() makes it)" % (folder, exe))
	stay = set(DATA_FILES) | {exe} | set(keep)
	removed = []
	for entry in sorted(os.listdir(folder)):
		if entry in stay:
			continue
		path = os.path.join(folder, entry)
		if os.path.isdir(path) and not os.path.islink(path):
			shutil.rmtree(path)
		else:
			os.remove(path)
		removed.append(entry)
	for data in DATA_FILES:
		source = os.path.join(ROOT, data)
		if data not in ROOT_DATA:
			ROOT_DATA[data] = file_sha(source)
		target = os.path.join(folder, data)
		if not os.path.isfile(target) or file_sha(target) != ROOT_DATA[data]:
			shutil.copyfile(source, target)
			removed.append(data + " (copied again)")
	left = sorted(set(os.listdir(folder)) - stay)
	if left:
		raise RunError("%s still holds %s after cleaning" % (folder, left))
	if removed:
		log("run folder %s: removed %s" % (name, ", ".join(removed)))
	return removed


def reset_cfg(name):
	with open(os.path.join(run_dir(name), "Tibia.cfg"), "w", encoding="ascii", newline="\r\n") as f:
		f.write(TIBIA_CFG)


def settings_file_name(client):
	"""The file a client keeps its options in: the x64 client's Patella.ini (issue #45), Tibia.exe's Tibia.cfg."""
	return settings_file.PATELLA_INI if client_of(client) == "patella" else settings_file.TIBIA_CFG


def read_cfg(name):
	with open(os.path.join(run_dir(name), "Tibia.cfg"), "r", encoding="latin-1") as f:
		return f.read()


# --- the server --------------------------------------------------------------------------

def server_running():
	"""A tibia-server.exe from SERVER_DIR runs (started by these tools or not); inside Windows Sandbox the host's
	answer (issue #147)."""
	if sandbox.inside():
		return sandbox.server_running(SERVER_DIR)
	return testserver.running(SERVER_DIR)  # issue #146: the run's private server, not another run's


def start_server():
	"""testserver.py start, the one start of every tool (compare_files.py, testmode_check.py, reference.py's
	live recordings): it returns when the server listens on both ports and its world is loaded ("Server
	Online!"), and stops and restarts a server of its own that stalls (Patella-Modern issue #68). Again if the
	login server listens on 0.0.0.0. The server's login thread reads its bind address from a global
	(GameAddress) that LoadWorldConfig overwrites while the thread starts (src/main.cc,
	login/login_server.cc), so now and then it binds 0.0.0.0:7171 although config.cfg says 127.0.0.1."""
	anywhere = "0.0.0.0:%d" % px.SERVER_LOGIN_PORT  # 7171, or the run's private server's (issue #146)
	for attempt in range(1, 6):
		output = server_script("start")
		if anywhere not in output:
			return
		log("the login server listens on %s (the server's start-up race): restarting it" % anywhere)
		server_script("stop")
	raise RunError("the login server listened on %s five times" % anywhere)


def server_script(command):
	"""python -I tools/testserver.py <command> (start or stop): its output to server-scripts.log and
	the log; returns the output."""
	# output to a file, not a pipe: a pipe the server inherited would stay open (and subprocess
	# wait for its end) as long as the server runs
	path = os.path.join(TRAFFIC, "server-scripts.log")
	name = "testserver.py " + command
	with open(path, "a", encoding="utf-8") as f:
		start = f.tell()
		code = subprocess.call([sys.executable, "-I", os.path.join(ROOT, "tools", "testserver.py"), command],
			stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, cwd=ROOT)
	with open(path, "r", encoding="utf-8", errors="replace") as f:
		f.seek(start)
		output = f.read()
	for line in output.strip().splitlines():
		log("  " + line)
	if code != 0:
		raise RunError("%s failed (exit %d)" % (name, code))
	return output


def snapshot_players():
	"""The test characters' player files (None: no file yet); the server must be stopped."""
	snapshot = {}
	for name, path in PLAYER_FILES.items():
		snapshot[name] = open(path, "rb").read() if os.path.isfile(path) else None
	return snapshot


def restore_players(snapshot):
	for name, path in PLAYER_FILES.items():
		data = snapshot[name]
		if data is None:
			if os.path.isfile(path):
				os.remove(path)
		else:
			current = open(path, "rb").read() if os.path.isfile(path) else None
			if current != data:
				with open(path, "wb") as f:
					f.write(data)


# --- runs --------------------------------------------------------------------------------

def gui_summary(nodes):
	return [{k: n[k] for k in ("depth", "class", "x", "y", "w", "h")} | ({"text": n["text"]} if "text" in n else {})
		for n in nodes]


# The session's look step: the player's own field, not north of it: no creature can stand there (issue #137).
# compare_rendering.py replays this tool's orig-live.jsonl with the same step (issue #140).
LOOK = sc.LOOK_OWN_FIELD


def play(client, proxy, wait, out_prefix, helper=None):
	"""One session of `client` through `proxy`. Returns [{"step", "state", "gui"}] in step order."""
	info = CLIENTS[client]
	clean_run_dir(client)
	reset_cfg(client)
	driver = sc.Driver(run_dir(client), info["exe"], info["pdb"], test=info.get("test", False))
	checkpoints = []

	def checkpoint(name):
		state, gui = driver.mem.snapshot()
		checkpoints.append({"step": name, "state": state, "gui": gui_summary(gui)})

	try:
		driver.start()
		if helper is not None:
			helper()
		sc.run_session(driver, wait, checkpoint, proxy=proxy, look=LOOK)
		# issue #144: the start screen after the logout (a fixed 1 s until then), then WM_CLOSE
		driver.wait_window("TStartWindow")
		code = driver.close()
		if code != 0:
			raise RunError("%s exited with code %d" % (info["exe"], code))
	finally:
		driver.kill()
	with open(out_prefix + ".states.json", "w", encoding="utf-8", newline="\n") as f:
		json.dump(checkpoints, f)
	name = settings_file_name(client)
	shutil.copyfile(os.path.join(run_dir(client), name), out_prefix + "." + name)
	return checkpoints


def live_run(client, out, players):
	log("=== live session: %s" % client)
	restore_players(players)
	start_server()
	helper = sc.Driver(run_dir("helper"), HELPER["exe"], None, game_port=px.SERVER_GAME_PORT)
	clean_run_dir("helper")
	reset_cfg("helper")
	prefix = os.path.join(out, "%s-live" % client)
	try:
		def start_helper():
			helper.start(["gamemaster"])
			# issue #144: no wait after the login (sc.login waits for the character list itself), then the helper
			# in the game as it shows it (the server has its player), instead of fixed 1.5 s and 2 s
			sc.login(helper, lambda name: None, sc.TRADE_PARTNER)
			sc.wait_in_game(helper)
			log("helper: %s is in the game" % sc.TRADE_PARTNER)

		proxy = px.Proxy(prefix + ".jsonl", px.load_private_key(PEM), log=log)
		with proxy:
			# the proxy listens from here on; the client under test logs in only in the session
			play(client, proxy, live_wait(proxy), prefix, helper=start_helper)
		if proxy.errors:
			raise RunError("proxy errors: %s" % proxy.errors)
		helper_logout(helper)
	finally:
		helper.kill()
		server_script("stop")
	return prefix + ".jsonl"


def helper_logout(helper):
	"""Ctrl+L; if the helper is still in the game, WM_CLOSE and Return (the "Are you sure you
	want to leave Tibia?" question: logout). Killing it is the last resort: the server then logs
	the character out itself (and its player file is restored afterwards anyway)."""
	helper.ctrl("l")
	if not helper.wait_connected(helper.game_port, False, 10):
		log("helper: still in the game after Ctrl+L; closing the window")
		lc.post(helper.hwnd, lc.WM_CLOSE)
		time.sleep(1)
		helper.char(13)
		if not helper.wait_connected(helper.game_port, False, 10):
			log("helper: still in the game; killed")
			helper.kill()
			return
	time.sleep(1)
	log("helper: logged out")
	# the helper's Tibia.cfg does not matter: a WM_CLOSE, and if it does not exit (the
	# "gamemaster" client did not always), it is killed
	if helper.proc.poll() is None:
		lc.post(helper.hwnd, lc.WM_CLOSE)
		try:
			helper.proc.wait(5)
		except subprocess.TimeoutExpired:
			helper.kill()


def replay_run(client, source, out):
	try:
		sc.check_recorded_look(px.load_events(source), LOOK, source)  # issue #140: --skip-live's older recordings
	except sc.SessionError as e:
		raise RunError(str(e)) from e
	name = os.path.splitext(os.path.basename(source))[0]
	log("=== replay of %s: %s" % (name, client))
	prefix = os.path.join(out, "%s-replay-of-%s" % (client, name))
	proxy = px.Proxy(prefix + ".jsonl", px.load_private_key(PEM), replay=px.load_events(source), log=log)
	with proxy:
		play(client, proxy, lambda step: proxy.wait_idle(0.5, 60), prefix)
	return prefix, proxy.mismatches + proxy.errors


# --- comparison --------------------------------------------------------------------------

def client_packets(events):
	return [(e["kind"], e["conn_ordinal"], px.masked(e)) for e in events if e["dir"] == "c2s"]


def server_packets(events):
	return [(e["kind"], e["conn_ordinal"], e["plain"]) for e in events if e["dir"] == "s2c"]


def diff_lists(a, b, label_a, label_b, limit=10):
	out = []
	for i in range(max(len(a), len(b))):
		x = a[i] if i < len(a) else None
		y = b[i] if i < len(b) else None
		if x != y:
			out.append("  #%d %s: %s\n      %s: %s" % (i, label_a, x, label_b, y))
			if len(out) >= limit:
				out.append("  ...")
				break
	return out


def diff_values(a, b, path="", out=None, limit=20):
	out = [] if out is None else out
	if len(out) >= limit:
		return out
	if isinstance(a, dict) and isinstance(b, dict):
		for key in sorted(set(a) | set(b)):
			if key not in a or key not in b:
				out.append("%s.%s: only in %s" % (path, key, "one"))
			else:
				diff_values(a[key], b[key], "%s.%s" % (path, key), out, limit)
	elif isinstance(a, list) and isinstance(b, list):
		if len(a) != len(b):
			out.append("%s: %d entries against %d" % (path, len(a), len(b)))
		for i, (x, y) in enumerate(zip(a, b)):
			diff_values(x, y, "%s[%d]" % (path, i), out, limit)
	elif a != b:
		out.append("%s: %r against %r" % (path, a, b))
	return out


def compare_gui(orig, patella, mapping):
	"""Window trees: everything equal, Tibia.exe's vtable names consistently paired with classes."""
	out = []
	if len(orig) != len(patella):
		return ["%d windows against %d" % (len(orig), len(patella))]
	for i, (o, p) in enumerate(zip(orig, patella)):
		for key in ("depth", "x", "y", "w", "h", "text"):
			if o.get(key) != p.get(key):
				out.append("window #%d (%s/%s): %s %r against %r" % (i, o["class"], p["class"], key, o.get(key), p.get(key)))
		if o["class"].startswith("vt_") or o["class"] == "(modal)":
			if mapping.setdefault(o["class"], p["class"]) != p["class"]:
				out.append("window #%d: %s is %s here, %s elsewhere" % (i, o["class"], p["class"], mapping[o["class"]]))
		elif o["class"] != p["class"]:
			out.append("window #%d: class %s against %s" % (i, o["class"], p["class"]))
	return out


def main():
	# The record/compare split (Patella-Modern issue #23): record or compare as the first word runs
	# tools/reference.py with this tool's sessions (notes/testmode.md, "Reference recordings").
	if len(sys.argv) > 1 and sys.argv[1] in ("record", "compare"):
		import reference
		return reference.main([sys.argv[1], "--sessions", ",".join(reference.tool_sessions("compare_traffic"))] + sys.argv[2:])
	ctypes.windll.user32.SetProcessDPIAware()
	parser = argparse.ArgumentParser(description="Compare the traffic of Patella.exe and Tibia.exe.")
	parser.add_argument("--out", default=os.path.join(TRAFFIC, "result"))
	parser.add_argument("--skip-live", action="store_true", help="reuse the live recordings in --out")
	parser.add_argument("--no-cross", action="store_true", help="replay only Tibia.exe's live recording")
	parser.add_argument("--patella", metavar="EXE", help="the x64 Patella.exe to test (its test build PatellaTest.exe "
		"next to it runs, issue #92); default build/windows-x64/Release/Patella.exe")
	parser.add_argument(desktop.HIDDEN, action="store_true", help=desktop.HIDDEN_HELP)
	parser.add_argument(sandbox.DESKTOP, action="store_true", help=sandbox.DESKTOP_HELP)
	args = parser.parse_args()
	named = use_patella(args.patella or DEFAULT_PATELLA)
	out = os.path.abspath(args.out)
	os.makedirs(out, exist_ok=True)
	report = []
	failed = False

	def say(text=""):
		print(text, flush=True)
		report.append(text)

	log(named)

	setup()
	live = {client: os.path.join(out, "%s-live.jsonl" % client) for client in CLIENTS}
	if not args.skip_live:
		if server_running():
			raise RunError("the test server is running: stop it first (python -I tools/testserver.py stop)")
		players = snapshot_players()
		try:
			for client in CLIENTS:
				live_run(client, out, players)
		finally:
			restore_players(players)
			log("player files of the test characters restored")
	sources = [live["orig"]] + ([] if args.no_cross else [live["patella"]])
	replays = {}
	for source in sources:
		for client in CLIENTS:
			prefix, problems = replay_run(client, source, out)
			replays[(client, source)] = (prefix, problems)

	say("Traffic comparison, %s" % time.strftime("%Y-%m-%d %H:%M"))
	say("results in %s" % out)
	events = {client: px.load_events(path) for client, path in live.items()}

	say()
	say("a. client packets, live sessions (Tibia.exe against Patella.exe)")
	a = client_packets(events["orig"])
	b = client_packets(events["patella"])
	differences = diff_lists(a, b, "Tibia.exe", "Patella.exe")
	say("  %d and %d packets, %s" % (len(a), len(b), "identical" if not differences else "DIFFERENT"))
	for line in differences:
		say(line)
	failed |= bool(differences)
	firsts = {"Tibia.exe": [], "Patella.exe": []}
	for path in list(live.values()) + [prefix + ".jsonl" for prefix, problems in replays.values()]:
		exe = "Tibia.exe" if os.path.basename(path).startswith("orig") else "Patella.exe"
		firsts[exe] += [e for e in px.load_events(path) if "fields_end" in e]
	for exe, packets in firsts.items():
		good = sum(1 for e in packets if px.rand_stream(e))
		keys = len({e["key"] for e in packets})
		say("  masked bytes, %s: %d of %d first packets have key and random tail from one rand() run; %d different keys"
			% (exe, good, len(packets), keys))
		failed |= good != len(packets)

	say()
	say("b. client packets of the replays against the replayed recording")
	for (client, source), (prefix, problems) in replays.items():
		replayed = client_packets(px.load_events(prefix + ".jsonl"))
		recorded = client_packets(px.load_events(source))
		differences = diff_lists(recorded, replayed, "recorded", "replay") + ["  proxy: " + p for p in problems]
		say("  %s replaying %s: %d packets, %s" % (CLIENTS[client]["exe"], os.path.basename(source), len(replayed),
			"identical" if not differences else "DIFFERENT"))
		for line in differences:
			say(line)
		failed |= bool(differences)

	say()
	say("c. client state and windows after every step, replay of the same recording")
	mapping = {}
	for source in sources:
		states = {}
		for client in CLIENTS:
			with open(replays[(client, source)][0] + ".states.json", encoding="utf-8") as f:
				states[client] = {c["step"]: c for c in json.load(f)}
		steps = list(states["orig"])
		say("  %s: %d steps (%s)" % (os.path.basename(source), len(steps), ", ".join(steps)))
		for step in steps:
			if step not in states["patella"]:
				say("    %s: missing in Patella.exe's run" % step)
				failed = True
				continue
			o, p = states["orig"][step], states["patella"][step]
			differences = diff_values(o["state"], p["state"], "state") + compare_gui(o["gui"], p["gui"], mapping)
			player = [c for c in o["state"]["creatures"] if c["id"] == o["state"]["player"]["PlayerID"]]
			say("    %-18s %s  (player at %s, %d map objects, %d creatures, %d windows)" % (step,
				"identical" if not differences else "DIFFERENT", player[0]["pos"] if player else "-",
				sum(len(point) for point in o["state"]["map"]), len(o["state"]["creatures"]), len(o["gui"])))
			for line in differences:
				say("      " + line)
			failed |= bool(differences)
	say("  Tibia.exe's window classes paired with Patella's: %d vtables" % len(mapping))
	if len(sources) > 1:
		# control: the same client fed two different recordings must show different states
		runs = []
		for source in sources:
			with open(replays[("orig", source)][0] + ".states.json", encoding="utf-8") as f:
				runs.append(json.load(f))
		differing = sum(1 for x, y in zip(*runs) if diff_values(x["state"], y["state"]))
		say("  control: Tibia.exe's states in the replays of the two different recordings differ at %d of %d steps"
			% (differing, len(runs[0])))

	say()
	say("d. the options written on exit (issue #45: Tibia.exe's Tibia.cfg, the x64 client's Patella.ini, by value)")
	cfgs = {}
	for client in CLIENTS:
		name = settings_file_name(client)
		path = os.path.join(out, "%s-live.%s" % (client, name))
		if os.path.isfile(path):
			with open(path, "rb") as f:
				cfgs[("live", "%s live" % client)] = settings_file.values_of(name, f.read())
		for source in sources:
			with open(replays[(client, source)][0] + "." + name, "rb") as f:
				cfgs[("replay", "%s replay of %s" % (client, os.path.basename(source)))] = settings_file.values_of(name,
					f.read())
	for kind in ("live", "replay"):
		group = {k[1]: v for k, v in cfgs.items() if k[0] == kind}
		if not group:
			continue
		names = list(group)
		found = [(name, settings_file.differences(group[names[0]], group[name], names[0], name)) for name in names[1:]]
		same = not any(lines for _, lines in found)
		say("  %s runs: %d files, %s" % (kind, len(group), "equal" if same else "DIFFERENT"))
		if not same:
			failed = True
			for name, lines in found:
				for line in lines:
					say("    " + line)

	say()
	say("e. server packets, live sessions (not required to be equal: the world moves on its own)")
	a = server_packets(events["orig"])
	b = server_packets(events["patella"])
	same = sum(1 for x, y in zip(a, b) if x == y)
	say("  %d and %d packets (%d and %d bytes), %d equal at the same index" % (len(a), len(b),
		sum(len(x[2]) // 2 for x in a), sum(len(x[2]) // 2 for x in b), same))
	for client, packets in (("Tibia.exe", a), ("Patella.exe", b)):
		moves = sum(1 for x in packets if x[2].startswith("6d"))
		say("  %s: %d packets start with a creature move (0x6d)" % (client, moves))

	say()
	say("RESULT: %s" % ("differences found" if failed else "no differences"))
	with open(os.path.join(out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(report) + "\n")
	return 1 if failed else 0


if __name__ == "__main__":
	# issue #146: a private test server of its own (a copy of the master, its own ports), then (issue #147) inside
	# Windows Sandbox unless --desktop (or --hidden) is given
	testserver.isolate(skip=("record", "compare", "--check-only", "-h", "--help"))
	sandbox.redirect(writable=[TRAFFIC], skip=("record", "compare", "--check-only", "-h", "--help"))
	desktop.hide(skip=("--check-only", "-h", "--help"))  # issue #56: only with --hidden
	try:
		sys.exit(main())
	except (RunError, sc.SessionError, px.ProtocolError) as e:
		traceback.print_exc()
		print("compare_traffic.py: %s" % e, file=sys.stderr)
		sys.exit(1)
