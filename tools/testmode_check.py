r"""The determinism check of the test mode (issue #21): the same script, the same packets.

Usage (from the repository or a worktree; the test server of issue #18 must be stopped):
  python -I tools/build.py windows-x64
  python -I tools/testmode_check.py [--exe build\windows-x64\Release\PatellaTest.exe]
                                    [--script tests\testmode\login-walk.script] [--runs 2]
                                    [--out build\testmode\result] [--timeout 180] [--compare-only]

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
Listed: the client packets' opcodes, whether the session reached the game (a game login, a walk
0x65-0x68, a logout 0x14), the server packets (not compared: the world moves on its own).
Exit code 0 when a-d found no difference, 1 otherwise (or on a failed run). --compare-only compares
the recordings already in --out (no server, no client).

Results in --out: run<N>.jsonl (the proxy's recordings), run<N>.log (the driver's), run<N>.Tibia.cfg,
report.txt.
"""

import argparse
import os
import shutil
import subprocess
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_traffic as ct  # noqa: E402
import proxy as px  # noqa: E402

ROOT = ct.ROOT
TESTMODE = os.path.join(ROOT, "build", "testmode")
RUN = "testmode"  # the run folder's name, build\testmode\testmode
EXE_NAME = "PatellaTest.exe"
DEFAULT_EXE = os.path.join(ROOT, "build", "windows-x64", "Release", EXE_NAME)
DEFAULT_SCRIPT = os.path.join(ROOT, "tests", "testmode", "login-walk.script")
SCRIPT_ERROR_EXIT = 3  # src/testmode/TestModeWin32.cpp: the script does not parse


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
	ct.RUN_EXES[RUN] = EXE_NAME


def run_client(script, log_path, timeout):
	"""Starts the test client in the run folder with the script and waits for it to exit."""
	folder = ct.run_dir(RUN)
	command = [os.path.join(folder, EXE_NAME), "--test-script", script, "--test-log", log_path]
	started = time.time()
	proc = subprocess.Popen(command, cwd=folder, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
		stdin=subprocess.DEVNULL)
	log("started %s, PID %d" % (EXE_NAME, proc.pid))
	try:
		_, stderr = proc.communicate(timeout=timeout)
	except subprocess.TimeoutExpired:
		proc.kill()
		proc.communicate()
		raise CheckError("the client did not exit within %d s (the script ends with quit; see %s)"
			% (timeout, log_path))
	text = stderr.decode("utf-8", "replace").strip()
	if proc.returncode == SCRIPT_ERROR_EXIT:
		raise CheckError("the script was refused: %s" % text)
	if proc.returncode != 0:
		raise CheckError("the client exited with code %d %s" % (proc.returncode, text))
	log("the client exited after %.1f s" % (time.time() - started))


def live_run(number, script, out, players, timeout):
	log("=== run %d" % number)
	ct.restore_players(players)
	ct.start_server()
	prefix = os.path.join(out, "run%d" % number)
	try:
		ct.clean_run_dir(RUN)
		ct.reset_cfg(RUN)
		proxy = px.Proxy(prefix + ".jsonl", px.load_private_key(ct.PEM), log=log)
		with proxy:
			run_client(script, prefix + ".log", timeout)
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


def compare(out, runs):
	report = []
	failed = False

	def say(text=""):
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
	parser.add_argument("--compare-only", action="store_true", help="compare the recordings already in --out")
	args = parser.parse_args(argv)
	if args.runs < 2:
		parser.error("--runs must be at least 2")
	out = os.path.abspath(args.out)
	os.makedirs(out, exist_ok=True)
	if not args.compare_only:
		script = os.path.abspath(args.script)
		if not os.path.isfile(script):
			raise CheckError("%s not found" % script)
		setup(os.path.abspath(args.exe))
		if ct.server_running():
			raise CheckError("the test server is running: stop it first (python -I tools/testserver.py stop)")
		players = ct.snapshot_players()
		try:
			for number in range(1, args.runs + 1):
				live_run(number, script, out, players, args.timeout)
		finally:
			ct.restore_players(players)
			log("player files of the test characters restored")
	return compare(out, args.runs)


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (CheckError, ct.RunError, px.ProtocolError) as e:
		traceback.print_exc()
		print("testmode_check.py: %s" % e, file=sys.stderr)
		sys.exit(1)
