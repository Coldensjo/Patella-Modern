r"""The determinism check of the test mode (issue #21): the same script, the same packets; and the
check of its dumps (issue #22): the client's own state dump, packet log and frame dump against what the
tools see from outside.

Usage (from the repository or a worktree; the test server of issue #18 must be stopped):
  python -I tools/build.py windows-x64
  python -I tools/testmode_check.py [--exe build\windows-x64\Release\PatellaTest.exe]
                                    [--script tests\testmode\login-walk.script] [--runs 2]
                                    [--out build\testmode\result] [--timeout 180] [--vulkan cpu]
                                    [--compare-only]
  python -I tools/testmode_check.py --self-test     (no server, no client: the comparisons on made-up
                                                     data and the static key lists; tools/testmode_dumps.py)

Runs PatellaTest.exe (the test build, src/testmode/TestModeWin32.cpp) --runs times with the same
input script against the local test server, through the recording proxy (tools/proxy.py), and
compares the runs. Each run: the test characters' player files are put back to what they were when
the script started (as tools/compare_traffic.py does), the server is started fresh
(tools/testserver.py start), the run folder build\testmode\run is emptied down to the data files and
the patched client (tools/patch_client.py: 127.0.0.1, the proxy's port 7271, the server's RSA key)
and gets compare_traffic.py's Tibia.cfg; the client is started with --test-script <script>
--test-log <out>\run<N>.log and plays the script on its virtual clock; the script ends with quit and
the client exits; then the server is stopped (it saves). Nothing drives the window from outside: the
script is the only input, and the client drops the desktop's own mouse and keyboard while it runs.
The client's window is still a real window on the desktop.

Compared, run 1 against every later run:
  a. the client packets (compare_traffic.client_packets: the first packet of each connection with
     the XTEA key and the RSA block's random tail masked, every later packet decrypted, without its
     padding): identical in order and content. This is the issue's criterion;
  b. the raw client frames as sent (encrypted, with the XTEA key, the RSA block's random bytes and the
     padding): identical too, because the virtual clock gives srand the same seed in every run;
  c. the driver's logs (every event with its virtual time and script line): identical;
  d. the Tibia.cfg the client wrote when it closed: identical.
Compared in every run (issue #22; the client runs with --test-out <out>\run<N> --test-sync <name>,
tools/testmode_dumps.py):
  e. each state dump of the client (state-<NNN>.json, at the script's dump commands) against
     client_state.py's reading of the same moment from outside (external-<NNN>.json: the client
     waits at each dump until this tool has read it): "state" and "gui" equal, and one dump per dump
     command;
  f. the client's packet log (packets.log, before encryption and after decryption) against the
     proxy's decrypted recording of the same run: every client packet identical, payload and padding,
     the first packets with the RSA block before encryption; the server packets the client decrypted
     identical to the proxy's, from the first (those after the client's last one are listed: never
     read);
  g. the frame dumps (frame-<NNN>.png, .filtered.png, frames.txt): one per frame command, readable.
  h. statically: the dump's keys (src/testmode/StateDump.h) against client_state.py's.
Listed: the client packets' opcodes, whether the session reached the game (a game login, a walk
0x65-0x68, a logout 0x14), the server packets (not compared: the world moves on its own), and whether
the runs' state dumps and frames are the same (not compared: the world moves on its own).
Exit code 0 when a-h found no difference, 1 otherwise (or on a failed run). --compare-only compares
the recordings already in --out (no server, no client).

The client runs with PATELLA_VULKAN=cpu (--vulkan): the pinned CPU driver next to PatellaTest.exe,
copied into the run folder, for frames that do not depend on the machine's GPU.

Results in --out: run<N>.jsonl (the proxy's recordings), run<N>.log (the driver's), run<N>.Tibia.cfg,
run<N>\ (the client's dumps and client_state.py's readings), report.txt.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import json
import os
import shutil
import subprocess
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_traffic as ct  # noqa: E402
import proxy as px  # noqa: E402
import testmode_dumps as td  # noqa: E402

ROOT = ct.ROOT
TESTMODE = os.path.join(ROOT, "build", "testmode")
RUN = "testmode"  # the run folder's name, build\testmode\testmode
EXE_NAME = "PatellaTest.exe"
DEFAULT_EXE = os.path.join(ROOT, "build", "windows-x64", "Release", EXE_NAME)
DEFAULT_SCRIPT = os.path.join(ROOT, "tests", "testmode", "login-walk.script")
SCRIPT_ERROR_EXIT = 3  # src/testmode/TestModeWin32.cpp: the script does not parse
# The Vulkan engine's CPU driver next to PatellaTest.exe (cmake/Vulkan.cmake; compare_rendering.py's).
CPU_DRIVER_FILES = ("vulkan_lvp.dll", "lvp_icd.x86_64.json")
WAIT_OBJECT_0, WAIT_TIMEOUT = 0, 0x102


class CheckError(Exception):
	pass


def log(text):
	ct.log(text)


def setup(exe):
	"""The run folder: the data files and the patched test client."""
	ct.set_folder(TESTMODE)
	folder = ct.run_dir(RUN)
	os.makedirs(folder, exist_ok=True)
	for data in ct.DATA_FILES:
		target = os.path.join(folder, data)
		source = os.path.join(ROOT, data)
		if not os.path.isfile(target) or os.path.getsize(target) != os.path.getsize(source):
			shutil.copyfile(source, target)
	if not os.path.isfile(exe):
		raise CheckError("%s not found (build it with python -I tools/build.py windows-x64)" % exe)
	patched = os.path.join(folder, EXE_NAME)
	if os.path.isfile(patched):
		os.remove(patched)
	command = [sys.executable, "-I", os.path.join(ROOT, "tools", "patch_client.py"), exe, patched,
		"--host", "127.0.0.1", "--rsa", ct.PEM, "--port", str(px.PROXY_LOGIN_PORT)]
	result = subprocess.run(command, capture_output=True, text=True)
	if result.returncode != 0:
		raise CheckError("patch_client.py failed: %s" % (result.stderr.strip() or result.stdout.strip()))
	# The Vulkan engine's CPU driver (PATELLA_VULKAN=cpu) next to the run folder's client.
	for name in CPU_DRIVER_FILES:
		source = os.path.join(os.path.dirname(exe), name)
		if os.path.isfile(source):
			shutil.copyfile(source, os.path.join(folder, name))
	ct.RUN_EXES[RUN] = EXE_NAME


class StateReader:
	"""client_state.py's reading at each state dump (--test-sync): the client sets <name>-ready after
	writing state-<NNN>.json and waits for <name>-continue; meanwhile this reads state() and gui()
	from its memory into external-<NNN>.json."""

	def __init__(self, dumps, pdb_exe):
		self.kernel32 = k = ctypes.WinDLL("kernel32", use_last_error=True)
		k.CreateEventW.restype = wt.HANDLE
		k.CreateEventW.argtypes = [ctypes.c_void_p, wt.BOOL, wt.BOOL, wt.LPCWSTR]
		k.SetEvent.argtypes = [wt.HANDLE]
		k.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
		k.WaitForSingleObject.restype = wt.DWORD
		k.CloseHandle.argtypes = [wt.HANDLE]
		self.name = "Local\\patella-testmode-%d-%d" % (os.getpid(), int(time.time() * 1000) % 1000000)
		self.ready = k.CreateEventW(None, False, False, self.name + "-ready")
		self.go_on = k.CreateEventW(None, False, False, self.name + "-continue")
		if not self.ready or not self.go_on:
			raise CheckError("CreateEventW failed (error %d)" % ctypes.get_last_error())
		self.dumps = dumps
		self.pdb_exe = pdb_exe
		self.client = None
		self.steps = 0

	def close(self):
		if self.client is not None:
			self.client.close()
		for handle in (self.ready, self.go_on):
			self.kernel32.CloseHandle(handle)

	def serve(self, proc, deadline):
		"""Answers the client's dumps until it exits or the deadline passes; False on the deadline."""
		import client_state as cs
		while proc.poll() is None:
			if time.time() > deadline:
				return False
			if self.kernel32.WaitForSingleObject(self.ready, 250) != WAIT_OBJECT_0:
				continue
			self.steps += 1
			reading = {"format": td.STATE_FORMAT, "source": "client_state.py", "step": self.steps}
			started = time.time()
			try:
				if self.client is None:
					self.client = cs.Client(proc.pid, self.pdb_exe)
				reading["state"] = self.client.state()
				reading["gui"] = self.client.gui()
			except (cs.ReadError, OSError) as e:
				reading["error"] = str(e)
			with open(os.path.join(self.dumps, "external-%03d.json" % self.steps), "w", encoding="utf-8") as f:
				json.dump(reading, f, separators=(",", ":"))
			self.kernel32.SetEvent(self.go_on)
			log("state dump %d read by client_state.py in %.2f s%s" % (self.steps, time.time() - started,
				" (%s)" % reading["error"] if "error" in reading else ""))
		return True


def run_client(script, log_path, timeout, dumps, pdb_exe, vulkan):
	"""Starts the test client in the run folder with the script, answers its state dumps, and waits
	for it to exit."""
	folder = ct.run_dir(RUN)
	reader = StateReader(dumps, pdb_exe)
	command = [os.path.join(folder, EXE_NAME), "--test-script", script, "--test-log", log_path,
		"--test-out", dumps, "--test-sync", reader.name]
	env = dict(os.environ)
	env["PATELLA_VULKAN"] = vulkan
	started = time.time()
	proc = subprocess.Popen(command, cwd=folder, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
		stdin=subprocess.DEVNULL, env=env)
	log("started %s, PID %d, PATELLA_VULKAN=%s" % (EXE_NAME, proc.pid, vulkan))
	try:
		in_time = reader.serve(proc, started + timeout)
		if not in_time:
			proc.kill()
			proc.communicate()
			raise CheckError("the client did not exit within %d s (the script ends with quit; see %s)"
				% (timeout, log_path))
		_, stderr = proc.communicate(timeout=30)
	finally:
		reader.close()
	text = stderr.decode("utf-8", "replace").strip()
	if proc.returncode == SCRIPT_ERROR_EXIT:
		raise CheckError("the script was refused: %s" % text)
	if proc.returncode != 0:
		raise CheckError("the client exited with code %d %s" % (proc.returncode, text))
	log("the client exited after %.1f s" % (time.time() - started))


def live_run(number, script, out, players, timeout, pdb_exe, vulkan):
	log("=== run %d" % number)
	ct.restore_players(players)
	ct.start_server()
	prefix = os.path.join(out, "run%d" % number)
	try:
		ct.clean_run_dir(RUN, keep=CPU_DRIVER_FILES)
		ct.reset_cfg(RUN)
		if os.path.isdir(prefix):
			shutil.rmtree(prefix)
		os.makedirs(prefix)
		proxy = px.Proxy(prefix + ".jsonl", px.load_private_key(ct.PEM), log=log)
		with proxy:
			run_client(script, prefix + ".log", timeout, prefix, pdb_exe, vulkan)
		if proxy.errors:
			raise CheckError("proxy errors: %s" % proxy.errors)
		shutil.copyfile(os.path.join(ct.run_dir(RUN), "Tibia.cfg"), prefix + ".Tibia.cfg")
	finally:
		ct.server_script("stop")


# --- comparison ----------------------------------------------------------------------------

def raw_client_frames(events):
	return [(e["kind"], e["conn_ordinal"], e["raw"]) for e in events if e["dir"] == "c2s"]


def opcode(event):
	"""The first byte of a client packet's payload, or the connection kind for a first packet."""
	if "fields_end" in event:
		return event["kind"] + "-login"
	return event["plain"][:2]


def session_summary(events):
	"""What the session reached, from the client packets."""
	c2s = [e for e in events if e["dir"] == "c2s"]
	game_login = any(e["kind"] == "game" and "fields_end" in e for e in c2s)
	codes = [opcode(e) for e in c2s if e["kind"] == "game" and "fields_end" not in e]
	walks = sum(1 for c in codes if c in ("65", "66", "67", "68"))
	return {"packets": len(c2s), "game login": game_login, "walks": walks, "logout": "14" in codes,
		"talk": "96" in codes, "pings": codes.count("1e"), "opcodes": " ".join(opcode(e) for e in c2s)}


def driver_lines(path):
	"""The driver's log without the first line (it names the script's path)."""
	if not os.path.isfile(path):
		return None
	with open(path, encoding="utf-8", errors="replace") as f:
		lines = f.read().splitlines()
	return [line for line in lines if not line.startswith("testmode: script ")]


def compare(out, runs, script, quiet=False, report=None):
	"""Compares the recordings in out (see the module's description); report: a list that gets the
	report's lines. Returns the exit code."""
	report = [] if report is None else report
	failed = False

	def say(text=""):
		if not quiet:
			print(text, flush=True)
		report.append(text)

	say("Test mode determinism check, %s" % time.strftime("%Y-%m-%d %H:%M"))
	say("results in %s" % out)
	events = {}
	for number in range(1, runs + 1):
		path = os.path.join(out, "run%d.jsonl" % number)
		if not os.path.isfile(path):
			raise CheckError("%s is missing" % path)
		events[number] = px.load_events(path)

	say()
	say("the sessions")
	for number, run in events.items():
		summary = session_summary(run)
		say("  run %d: %d client packets, game login %s, %d walks, talk %s, logout %s, %d pings"
			% (number, summary["packets"], "yes" if summary["game login"] else "NO", summary["walks"],
			"yes" if summary["talk"] else "no", "yes" if summary["logout"] else "NO", summary["pings"]))
		say("    opcodes: %s" % summary["opcodes"])
		if not summary["game login"] or not summary["logout"]:
			say("    the session did not reach the game and leave it again: the script's places or waits do not"
				" fit (see run%d.log)" % number)
			failed = True

	checks = (
		("a. client packets (masked first packets, decrypted payloads)", ct.client_packets),
		("b. raw client frames (encrypted, keys and padding included)", raw_client_frames),
	)
	for title, extract in checks:
		say()
		say(title)
		first = extract(events[1])
		for number in range(2, runs + 1):
			other = extract(events[number])
			differences = ct.diff_lists(first, other, "run 1", "run %d" % number)
			say("  run 1 against run %d: %d and %d packets, %s" % (number, len(first), len(other),
				"identical" if not differences else "DIFFERENT"))
			for line in differences:
				say(line)
			failed |= bool(differences)

	say()
	say("c. the driver's logs")
	first = driver_lines(os.path.join(out, "run1.log"))
	for number in range(2, runs + 1):
		other = driver_lines(os.path.join(out, "run%d.log" % number))
		if first is None or other is None:
			say("  run 1 against run %d: a log is missing" % number)
			failed = True
			continue
		differences = ct.diff_lists(first, other, "run 1", "run %d" % number)
		say("  run 1 against run %d: %d and %d lines, %s" % (number, len(first), len(other),
			"identical" if not differences else "DIFFERENT"))
		for line in differences:
			say(line)
		failed |= bool(differences)

	say()
	say("d. Tibia.cfg written on exit")
	cfgs = {}
	for number in range(1, runs + 1):
		path = os.path.join(out, "run%d.Tibia.cfg" % number)
		cfgs[number] = open(path, encoding="latin-1").read() if os.path.isfile(path) else None
	distinct = set(cfgs.values())
	say("  %d files, %s" % (len(cfgs), "identical" if len(distinct) == 1 and None not in distinct else "DIFFERENT"))
	failed |= len(distinct) != 1 or None in distinct

	say()
	say("server packets (not compared: the world moves on its own)")
	for number, run in events.items():
		packets = ct.server_packets(run)
		say("  run %d: %d packets, %d bytes" % (number, len(packets), sum(len(p[2]) // 2 for p in packets)))

	# Issue #22: the dumps, in every run.
	dumps_wanted, frames_wanted = td.script_commands(script)
	say()
	say("e. the state dumps against client_state.py's reading of the same moment (%d dump commands)" % dumps_wanted)
	for number in range(1, runs + 1):
		differences, lines = td.compare_state_dumps(os.path.join(out, "run%d" % number), dumps_wanted)
		say("  run %d: %s" % (number, "equal" if not differences else "DIFFERENT"))
		for line in lines + differences:
			say("  " + line)
		failed |= bool(differences)

	say()
	say("f. the packet log against the proxy's decrypted recording")
	for number in range(1, runs + 1):
		path = os.path.join(out, "run%d" % number, "packets.log")
		if not os.path.isfile(path):
			say("  run %d: packets.log is missing" % number)
			failed = True
			continue
		try:
			records = td.load_packet_log(path)
		except td.DumpError as e:
			say("  run %d: %s" % (number, e))
			failed = True
			continue
		differences, summary = td.compare_packet_log(records, events[number])
		say("  run %d: %d connections, %d client and %d server packets in the log, %s; %d server packets the"
			" client never read" % (number, summary["connections"], summary["c2s"], summary["s2c"],
			"identical to the proxy's" if not differences else "DIFFERENT", summary["unread"]))
		for line in differences:
			say(line)
		failed |= bool(differences)

	say()
	say("g. the frame dumps (%d frame commands)" % frames_wanted)
	for number in range(1, runs + 1):
		problems, lines = td.check_frames(os.path.join(out, "run%d" % number), frames_wanted)
		say("  run %d: %s" % (number, "complete" if not problems else "PROBLEMS"))
		for line in lines + problems:
			say("  " + line)
		failed |= bool(problems)

	say()
	say("h. the dump's keys (src/testmode/StateDump.h) against client_state.py's")
	problems, lines = td.compare_field_lists()
	for line in lines + problems:
		say(line)
	failed |= bool(problems)

	say()
	say("the runs' dumps against each other (not compared: the world moves on its own)")
	first_dumps = td.numbered(os.path.join(out, "run1"), "state", ".json")
	first_frames = td.numbered(os.path.join(out, "run1"), "frame", ".png")
	for number in range(2, runs + 1):
		folder = os.path.join(out, "run%d" % number)
		dumps = td.numbered(folder, "state", ".json")
		same = [step for step in sorted(set(first_dumps) & set(dumps)) if td.comparable(td.load_json(first_dumps[step]))
			== td.comparable(td.load_json(dumps[step]))]
		frames = td.numbered(folder, "frame", ".png")
		same_frames = [step for step in sorted(set(first_frames) & set(frames))
			if open(first_frames[step], "rb").read() == open(frames[step], "rb").read()]
		say("  run 1 against run %d: state dumps %s of %d the same (without addresses), frames %s of %d the same"
			% (number, " ".join(str(s) for s in same) or "none", len(dumps), " ".join(str(s) for s in same_frames)
			or "none", len(frames)))

	say()
	say("RESULT: %s" % ("differences found" if failed else "no differences"))
	with open(os.path.join(out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(report) + "\n")
	return 1 if failed else 0


def main(argv=None):
	parser = argparse.ArgumentParser(description="Run PatellaTest.exe twice with the same script and compare.")
	parser.add_argument("--exe", default=DEFAULT_EXE, help="the test build (default %(default)s)")
	parser.add_argument("--script", default=DEFAULT_SCRIPT, help="the input script (default %(default)s)")
	parser.add_argument("--runs", type=int, default=2, help="how many runs (at least 2; default 2)")
	parser.add_argument("--out", default=os.path.join(TESTMODE, "result"))
	parser.add_argument("--timeout", type=int, default=180, help="seconds a run may take (default 180)")
	parser.add_argument("--vulkan", default="cpu", help="PATELLA_VULKAN of the client (default %(default)s: the"
		" pinned CPU driver)")
	parser.add_argument("--compare-only", action="store_true", help="compare the recordings already in --out")
	parser.add_argument("--self-test", action="store_true", help="the comparisons on made-up data (no server,"
		" no client)")
	args = parser.parse_args(argv)
	if args.self_test:
		return 1 if td.self_test(compare) else 0
	if args.runs < 2:
		parser.error("--runs must be at least 2")
	out = os.path.abspath(args.out)
	os.makedirs(out, exist_ok=True)
	script = os.path.abspath(args.script)
	if not os.path.isfile(script):
		raise CheckError("%s not found" % script)
	if not args.compare_only:
		exe = os.path.abspath(args.exe)
		setup(exe)
		if ct.server_running():
			raise CheckError("the test server is running: stop it first (python -I tools/testserver.py stop)")
		players = ct.snapshot_players()
		try:
			for number in range(1, args.runs + 1):
				live_run(number, script, out, players, args.timeout, exe, args.vulkan)
		finally:
			ct.restore_players(players)
			log("player files of the test characters restored")
	return compare(out, args.runs, script)


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (CheckError, ct.RunError, px.ProtocolError) as e:
		traceback.print_exc()
		print("testmode_check.py: %s" % e, file=sys.stderr)
		sys.exit(1)
