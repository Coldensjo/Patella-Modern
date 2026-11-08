"""Frame time and heap calls per frame of Tibia.exe and the x64 client on the same live session, and the Vulkan
engine's GPU time (Patella-Modern issue #134, M9; notes/testmode.md "Profile windows", notes/progress.md the numbers).

	python -I tools/profiling.py [--reps 5] [--variants gpu,cpu] [--out DIR]
	python -I tools/profiling.py --report-only --out DIR
	python -I tools/profiling.py --self-test

Each run: a client starts (cfg as the comparisons': a window of 800x600, ConfineFramerate 0, Tibia.exe's engine
Direct3D 9 and the x64 client's Vulkan engine on the device of the variant), then per scene a profile window: "start"
(the title screen), "game" (logged in, standing), "walk" (walking a square of four fields). A scene is three windows
in a row: "time" (--time seconds: only the performance counter at every Present), "allocs" (--allocs seconds: the
same with the client's heap calls counted) and, for the x64 client, "gpu" (--gpu seconds: the same with the Vulkan
engine's timestamp queries). The runs alternate Tibia.exe and the x64 client, --reps times each; the report gives,
per scene and client, the median of the runs' medians and the runs' spread.

Where it runs: inside Windows Sandbox (tools/sandbox.py) on a private copy of the test server (issue #146), both
clients, so both have the same machine, driver stack (GPU paravirtualization of the host's GPU) and scheduling. These
are numbers of that environment, not bare-metal ones: the report and the notes say so, and a difference is a
regression only when the two clients' run spreads do not overlap.

What is measured:
- Tibia.exe: patcap.dll in its real-time mode (no virtual clock): the counter at each IDirect3DDevice9::Present and
  Present's own duration (CMD_PROFILE_START/STOP); the heap calls at its imports of HeapAlloc, HeapReAlloc, HeapFree.
- The x64 client: PatellaTest.exe without a script, so on the system clock (not the virtual one) and through the same
  code path as Patella.exe; the control channel's "profile start/stop" (src/testmode/ProfileWin32.cpp) around
  TRenderer::Present; the same heap imports of the executable.
- A frame is one Present to the next: the frame time ("interval") includes the client's whole idle pass. "present" is
  the call's own part, and "before present" the interval without it: the client's work (windows, map, Vulkan recording
  or Direct3D 9 calls).
- GPU time: the Vulkan engine only (vkCmdWriteTimestamp2 around the frame's command buffers): Tibia.exe cannot be
  measured this way (it is the original program; its Direct3D 9 work is the driver's). No GPU number is compared.
- The heap counts compare the heap calls of each client's own code; a 32-bit and a 64-bit build of different
  containers: counts are comparable, bytes less so (pointers are twice as wide). Calls made by system DLLs and drivers
  (Direct3D 9's, the Vulkan driver's, SDL's if it has its own allocator) are in neither client's count."""
import argparse
import json
import math
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_rendering as cr  # noqa: E402
import compare_traffic as ct  # noqa: E402
import login_client as lc  # noqa: E402
import proxy as px  # noqa: E402
import sandbox  # noqa: E402
import session_client as sc  # noqa: E402
import testserver  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROFILE = os.path.join(ROOT, "build", "profile")
SKIP_ARGS = ("--self-test", "--report-only", "-h", "--help")

SCENES = ("start", "game", "walk")
WINDOWS_ORIG = ("time", "allocs")
WINDOWS_PATELLA = ("time", "allocs", "gpu")
SETTLE = 2.0  # seconds of a scene before its first window
# A step of the walking loop (Up, Right, Down, Left: a square, the character ends where it began): one step takes
# 200-500 ms at the test characters' speed; the next key is posted after WALK_GAP.
WALK_GAP = 1.0
WALK_KEYS = (sc.VK_UP, sc.VK_RIGHT, sc.VK_DOWN, sc.VK_LEFT)
# A difference counts as a candidate regression only when the x64 client's best run is this much worse than Tibia.exe's
# worst one (the windows' spread must not overlap).
THRESHOLD = 1.10


class ProfileError(Exception):
	pass


# --- the runs ----------------------------------------------------------------------------------------------------

class OrigDriver(cr.Driver):
	"""Tibia.exe under patcap.dll in its real-time mode (the client's clock and timer are the system's)."""

	manual_clock = False

	def start_once(self, arguments=()):
		super().start_once(arguments)
		self.cap.command(cr.CMD_REALTIME, 1, alive=self.alive)
		self.wait_window("TStartWindow", sc.START_TIMEOUT)

	def profile_start(self, kind, path):
		self.cap.command(cr.CMD_PROFILE_START, cr.PROFILE_ALLOCS if kind == "allocs" else 0, 0, path, alive=self.alive)

	def profile_stop(self):
		self.cap.command(cr.CMD_PROFILE_STOP, alive=self.alive)


class PatellaDriver(sc.Driver):
	"""PatellaTest.exe without a script: the system clock, the control channel."""

	def __init__(self, run_dir, exe, env, **kwargs):
		super().__init__(run_dir, exe, None, test=True, **kwargs)
		self.env = env

	def environment(self):
		return self.env

	def profile_start(self, kind, path):
		self.channel.command("profile start %s %s" % (kind, path), alive=self.alive)

	def profile_stop(self):
		self.channel.command("profile stop", alive=self.alive)


def run_label(client):
	return "orig" if client == "orig" else "patella"


def one_run(client, variant, rep, out, durations, proxy_key):
	"""A session of one client: its scenes' profile windows into out/<label>/<client>-<variant>/<rep>/<scene>-<kind>.txt;
	returns facts (the device, the Windows it ran on)."""
	label = run_label(client)
	folder = os.path.join(out, "runs", "%s-%s" % (client, variant), "%02d" % rep)
	if os.path.isdir(folder):
		shutil.rmtree(folder)
	os.makedirs(folder)
	ct.clean_run_dir(label, keep=("patcap.dll",) + cr.CPU_DRIVER_FILES)
	ct.reset_cfg(label)
	exe = ct.CLIENTS[label]["exe"]
	if client == "orig":
		env = dict(os.environ)
		env.pop("PATELLA_VULKAN", None)
		driver = OrigDriver(ct.run_dir(label), exe, None, env=env)
	else:
		env = dict(os.environ, PATELLA_VULKAN=variant)
		driver = PatellaDriver(ct.run_dir(label), exe, env)
	windows = WINDOWS_ORIG if client == "orig" else WINDOWS_PATELLA
	proxy = px.Proxy(os.path.join(folder, "proxy.jsonl"), proxy_key, log=lc.log)

	def windows_of(scene, activity=None):
		time.sleep(SETTLE)
		for kind in windows:
			path = os.path.join(folder, "%s-%s.txt" % (scene, kind))
			driver.profile_start(kind, path)
			deadline = time.time() + durations[kind]
			if activity is not None:
				activity(deadline)
			else:
				time.sleep(max(0.0, deadline - time.time()))
			driver.profile_stop()
			if not os.path.isfile(path):
				raise ProfileError("no %s after the profile window" % path)

	def walk(deadline):
		position = 0
		while time.time() < deadline:
			driver.key(WALK_KEYS[position % len(WALK_KEYS)])
			position += 1
			time.sleep(min(WALK_GAP, max(0.0, deadline - time.time())))

	facts = {"client": client, "variant": variant}
	try:
		with proxy:
			driver.start()
			windows_of("start")
			wait = ct.live_wait(proxy)
			sc.login(driver, wait, sc.CHARACTER, proxy)
			sc.wait_in_game(driver)
			windows_of("game")
			windows_of("walk", activity=walk)
			if client == "patella":
				probe = driver.mem.refresh()
				facts["device"] = probe.get("device")
			else:
				facts["d3d9"] = driver.cap.d3d9()
				facts["frames_api"] = cr.API_NAMES.get(driver.cap.get("Api"), "?")
			sc.logout(driver)
			driver.wait_window("TStartWindow")
			code = driver.close()
			if code != 0:
				raise ProfileError("%s exited with code %d" % (exe, code))
		if proxy.errors:
			raise ProfileError("proxy errors: %s" % proxy.errors)
	finally:
		driver.kill()
	return facts


def setup(patella):
	ct.set_folder(PROFILE)
	named = ct.use_patella(patella)
	lc.log(named)
	ct.setup()
	exe = os.path.join(ct.run_dir("orig"), ct.CLIENTS["orig"]["exe"])
	cr.add_import(exe, exe)
	shutil.copyfile(cr.capture_dll("capture-x86"), os.path.join(ct.run_dir("orig"), "patcap.dll"))
	source = os.path.dirname(ct.CLIENTS["patella"]["source"])
	for name in cr.CPU_DRIVER_FILES:
		if os.path.isfile(os.path.join(source, name)):
			shutil.copyfile(os.path.join(source, name), os.path.join(ct.run_dir("patella"), name))


def environment_facts():
	facts = {"inside_sandbox": sandbox.inside(), "date": time.strftime("%Y-%m-%d %H:%M"), "platform": platform.platform(),
		"cpus": os.cpu_count()}
	try:
		mode = cr.display_mode()
		facts["display"] = "%dx%d, %d bits, %d Hz" % mode
	except Exception as e:  # noqa: BLE001 - a fact of the report, not of the measurement
		facts["display"] = "unknown (%s)" % e
	return facts


def run_all(args):
	if not sandbox.inside():
		raise ProfileError("the runs belong inside Windows Sandbox (run this tool without --desktop)")
	setup(args.patella)
	if ct.server_running():
		raise ProfileError("the test server is running: stop it first")
	durations = {"time": args.time, "allocs": args.allocs, "gpu": args.gpu}
	key = px.load_private_key(ct.PEM)
	players = ct.snapshot_players()
	variants = ["gpu"] if not args.variants else [v for v in args.variants.split(",") if v]
	runs = []
	for v in variants:
		if v not in ("gpu", "cpu"):
			raise ProfileError("the variants are gpu and cpu, not %r" % v)
		for rep in range(1, args.reps + 1):
			if v == "gpu":
				runs.append(("orig", "d3d9", rep))
			runs.append(("patella", v, rep))
	summary = {"environment": environment_facts(), "reps": args.reps, "durations": durations, "facts": []}
	failures = 0
	try:
		ct.start_server()
		for client, variant, rep in runs:
			lc.log("=== %s (%s), run %d of %d" % (client, variant, rep, args.reps))
			try:
				summary["facts"].append(one_run(client, variant, rep, args.out, durations, key))
			except (ProfileError, sc.SessionError, ct.RunError, px.ProtocolError, OSError) as e:
				failures += 1
				lc.log("%s run %d failed: %s" % (client, rep, e))
				traceback.print_exc()
				if failures > args.reps:
					raise ProfileError("more than %d runs failed" % args.reps) from e
	finally:
		ct.server_script("stop")
		ct.restore_players(players)
		lc.log("player files of the test characters restored")
	summary["failed_runs"] = failures
	with open(os.path.join(args.out, "meta.json"), "w", encoding="utf-8", newline="\n") as f:
		json.dump(summary, f, indent=1)


# --- the numbers -------------------------------------------------------------------------------------------------

def parse_profile(path):
	"""The window's file: {"freq", "dropped", "frames": [(start, present, allocs, frees, bytes, small)], "gpu": [ns]}."""
	freq, dropped = None, 0
	frames, gpu = [], []
	with open(path, encoding="ascii") as f:
		for line in f:
			if line.startswith("# freq "):
				freq = int(line.split()[2])
			elif line.startswith("# dropped "):
				dropped = int(line.split()[2])
			elif line.startswith("F "):
				_, start, present, allocs, frees, bytes_asked, small = line.split()
				frames.append((int(start), int(present), int(allocs), int(frees), int(bytes_asked), int(small)))
			elif line.startswith("G "):
				gpu.append(int(line.split()[1]))
	if freq is None:
		raise ProfileError("%s: no frequency line" % path)
	return {"freq": freq, "dropped": dropped, "frames": frames, "gpu": gpu}


def percentile(values, p):
	"""Nearest-rank percentile of a sorted list."""
	if not values:
		return float("nan")
	index = max(0, min(len(values) - 1, math.ceil(p / 100 * len(values)) - 1))
	return values[index]


def window_numbers(profile):
	"""The numbers of one window. Times in milliseconds. A window's first and last frames are dropped: the client takes
	the window's commands between them."""
	frames = profile["frames"][1:-1] if len(profile["frames"]) > 2 else profile["frames"]
	result = {"frames": len(profile["frames"]), "dropped": profile["dropped"]}
	if len(frames) >= 2:
		freq = profile["freq"]
		intervals = sorted((b[0] - a[0]) * 1000.0 / freq for a, b in zip(frames, frames[1:]))
		presents = sorted(f[1] * 1000.0 / freq for f in frames)
		work = sorted(max(0.0, (b[0] - a[0] - a[1]) * 1000.0 / freq) for a, b in zip(frames, frames[1:]))
		span = (frames[-1][0] - frames[0][0]) / freq
		result.update({"fps": (len(frames) - 1) / span if span > 0 else 0.0,
			"interval": {"median": statistics.median(intervals), "p95": percentile(intervals, 95),
				"p99": percentile(intervals, 99), "max": intervals[-1]},
			"present": {"median": statistics.median(presents), "p95": percentile(presents, 95)},
			"before_present": {"median": statistics.median(work), "p95": percentile(work, 95)}})
	if frames:
		alloc_values = sorted(f[2] for f in frames)
		count = len(frames)
		asked = sum(f[4] for f in frames)
		small = sum(f[5] for f in frames)
		allocated = sum(f[2] for f in frames)
		result.update({"allocs_per_frame": {"mean": allocated / count, "median": statistics.median(alloc_values),
			"p99": percentile(alloc_values, 99), "max": alloc_values[-1]},
			"frees_per_frame": sum(f[3] for f in frames) / count, "bytes_per_frame": asked / count,
			"small_share": (small / allocated) if allocated else None})
	if profile["gpu"]:
		gpu = sorted(v / 1e6 for v in profile["gpu"])
		result["gpu"] = {"frames": len(gpu), "median": statistics.median(gpu), "p95": percentile(gpu, 95), "max": gpu[-1]}
	return result


def gather(out):
	"""{"<client>-<variant>": {scene: {kind: [window numbers, one per run]}}} from out/runs."""
	runs = os.path.join(out, "runs")
	data = {}
	if not os.path.isdir(runs):
		raise ProfileError("no %s" % runs)
	for who in sorted(os.listdir(runs)):
		for rep in sorted(os.listdir(os.path.join(runs, who))):
			folder = os.path.join(runs, who, rep)
			for name in sorted(os.listdir(folder)):
				if not name.endswith(".txt"):
					continue
				scene, kind = name[:-4].rsplit("-", 1)
				numbers = window_numbers(parse_profile(os.path.join(folder, name)))
				data.setdefault(who, {}).setdefault(scene, {}).setdefault(kind, []).append(numbers)
	return data


def aggregate(values):
	"""The run-to-run spread: median, min, max of per-run values, None for no values."""
	values = [v for v in values if v is not None]
	if not values:
		return None
	return {"median": statistics.median(values), "min": min(values), "max": max(values), "n": len(values)}


def run_values(windows, *path):
	values = []
	for window in windows:
		value = window
		for step in path:
			value = value.get(step) if isinstance(value, dict) else None
			if value is None:
				break
		values.append(value)
	return values


def fmt(stats, digits=3):
	if stats is None:
		return "n/a"
	return "%.*f (%.*f-%.*f, %d runs)" % (digits, stats["median"], digits, stats["min"], digits, stats["max"], stats["n"])


def report(out, data):
	lines = []
	say = lines.append
	path = os.path.join(out, "meta.json")
	meta = json.load(open(path, encoding="utf-8")) if os.path.isfile(path) else {}
	env = meta.get("environment", {})
	say("Frame time and heap calls per frame: Tibia.exe against the x64 client (issue #134)")
	say("")
	say("ENVIRONMENT (not a bare-metal measurement): %s; %s; display %s; %d logical CPUs; %s" % (
		"Windows Sandbox with the host's GPU through GPU paravirtualization" if env.get("inside_sandbox") else "not the Sandbox",
		env.get("platform", "?"), env.get("display", "?"), env.get("cpus") or 0, env.get("date", "?")))
	if "failed_runs" in meta:
		say("runs per client %d, failed runs: %d; windows (seconds): %s" % (meta.get("reps", 0), meta["failed_runs"],
			meta.get("durations")))
	devices = {json.dumps(f.get("device")) for f in meta.get("facts", []) if f.get("client") == "patella"}
	say("the x64 client's devices: %s" % ", ".join(sorted(devices)))
	say("Tibia.exe's frames: %s" % ", ".join(sorted({str(f.get("frames_api")) for f in meta.get("facts", []) if f.get("client") == "orig"})))
	say("")
	say("The client under the profile: PatellaTest.exe (the test build, on the system clock, the code of Patella.exe")
	say("plus the test hooks); Tibia.exe under patcap.dll in real-time mode. Frame = Present to Present. Numbers as")
	say("median (min-max of the runs' medians, or the runs' spread), N runs. The heap counts are the client's own calls of")
	say("HeapAlloc/HeapReAlloc/HeapFree at its imports (drivers and system DLLs not counted). GPU time: the Vulkan")
	say("engine's only; Tibia.exe's Direct3D 9 work cannot be timed.")
	sections = [("frame time, ms (time windows)", "time", ("interval", "median"), 3),
		("frame time p99, ms (time windows)", "time", ("interval", "p99"), 3),
		("frames per second (time windows)", "time", ("fps",), 0),
		("before Present, ms (time windows)", "time", ("before_present", "median"), 3),
		("Present's duration, ms (time windows)", "time", ("present", "median"), 3),
		("heap allocations per frame, mean (allocs windows)", "allocs", ("allocs_per_frame", "mean"), 2),
		("heap allocations per frame, p99 (allocs windows)", "allocs", ("allocs_per_frame", "p99"), 1),
		("heap frees per frame (allocs windows)", "allocs", ("frees_per_frame",), 2),
		("bytes asked for per frame (allocs windows)", "allocs", ("bytes_per_frame",), 0),
		("share of allocations of at most 1016 bytes (allocs windows)", "allocs", ("small_share",), 3),
		("frame time during the allocs windows, ms", "allocs", ("interval", "median"), 3),
		("GPU time per frame, ms (gpu windows: Vulkan engine only)", "gpu", ("gpu", "median"), 3),
		("GPU time per frame p95, ms (gpu windows)", "gpu", ("gpu", "p95"), 3),
		("frame time during the gpu windows, ms", "gpu", ("interval", "median"), 3)]
	clients = sorted(data)
	findings = []
	for title, kind, value, digits in sections:
		say("")
		say(title)
		for scene in SCENES:
			row = {}
			for who in clients:
				windows = data.get(who, {}).get(scene, {}).get(kind)
				if windows is not None:
					row[who] = aggregate(run_values(windows, *value))
			say("  %-5s %s" % (scene, ";  ".join("%s %s" % (who, fmt(stats, digits)) for who, stats in row.items())))
			if kind == "time" and value == ("before_present", "median"):
				findings.extend(judge(scene, row, "client work before Present (ms)"))
			if kind == "allocs" and value == ("allocs_per_frame", "mean"):
				findings.extend(judge(scene, row, "heap allocations per frame", absolute=1.0))
	say("")
	say("JUDGEMENT (the gpu variant against Tibia.exe; a candidate needs non-overlapping run spreads and at least %.0f%% "
		"worse, or for the heap calls at least one more per frame). The whole frame time and Present's duration are not "
		"judged: they differ by the graphics API behind the same GPU paravirtualization, which only a bare-metal run could "
		"separate from the client (notes/decisions.md \"#134\"):" % ((THRESHOLD - 1) * 100))
	say("  " + "\n  ".join(findings) if findings else "  none")
	text = "\n".join(lines) + "\n"
	with open(os.path.join(out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write(text)
	with open(os.path.join(out, "report.json"), "w", encoding="utf-8", newline="\n") as f:
		json.dump(data, f)
	print(text)


def judge(scene, row, what="client work before Present (ms)", absolute=None):
	"""The findings of one scene and measure: the x64 client on a GPU against Tibia.exe. A candidate needs the runs'
	spreads apart and, with absolute, at least that much more, else at least THRESHOLD times."""
	findings = []
	orig = row.get("orig-d3d9")
	ours = row.get("patella-gpu")
	if orig is None or ours is None:
		return findings
	apart = ours["min"] > orig["max"]
	if absolute is not None:
		worse = apart and ours["median"] - orig["median"] >= absolute
		compared = "%.3f more" % (ours["median"] - orig["median"])
	else:
		ratio = ours["median"] / orig["median"] if orig["median"] else float("nan")
		worse = apart and ratio >= THRESHOLD
		compared = "%.2f times" % ratio
	better = ours["max"] < orig["min"]
	findings.append("%s, %s: the x64 client %.3f, Tibia.exe %.3f: %s; %s" % (what, scene, ours["median"], orig["median"], compared,
		"CANDIDATE REGRESSION" if worse else ("better, spreads apart" if better else "no regression (spreads overlap or below the threshold)")))
	return findings


# --- self-test ---------------------------------------------------------------------------------------------------

def self_test():
	"""The parser and the numbers on made-up data; no client."""
	import tempfile
	sample = "\n".join(["# patella-profile 1", "# freq 1000", "# dropped 2"] +
		["F %d 1 %d 1 %d 1" % (1000 + 10 * i, 4, 16) for i in range(5)] + ["G 2000000", "G 4000000"]) + "\n"
	folder = tempfile.mkdtemp(prefix="profiling-selftest")
	try:
		path = os.path.join(folder, "start-time.txt")
		with open(path, "w", encoding="ascii", newline="\n") as f:
			f.write(sample)
		profile = parse_profile(path)
		checks = [(profile["freq"], 1000), (profile["dropped"], 2), (len(profile["frames"]), 5), (profile["gpu"], [2000000, 4000000])]
		numbers = window_numbers(profile)
		checks += [(numbers["interval"]["median"], 10.0), (numbers["fps"], 100.0), (numbers["allocs_per_frame"]["mean"], 4.0),
			(numbers["frees_per_frame"], 1.0), (numbers["bytes_per_frame"], 16.0), (numbers["small_share"], 0.25),
			(numbers["gpu"]["median"], 3.0), (numbers["present"]["median"], 1.0), (numbers["before_present"]["median"], 9.0)]
		checks += [(percentile([1, 2, 3, 4], 50), 2), (percentile([1, 2, 3, 4], 99), 4), (percentile([1, 2, 3, 4], 25), 1)]
		same = {"median": 10.0, "min": 9.0, "max": 11.0, "n": 3}
		checks += [("CANDIDATE" in judge("x", {"orig-d3d9": same, "patella-gpu": {"median": 20.0, "min": 19.0, "max": 21.0, "n": 3}})[0], True),
			("no regression" in judge("x", {"orig-d3d9": same, "patella-gpu": {"median": 10.5, "min": 9.5, "max": 11.5, "n": 3}})[0], True),
			("better" in judge("x", {"orig-d3d9": same, "patella-gpu": {"median": 5.0, "min": 4.5, "max": 5.5, "n": 3}})[0], True),
			("CANDIDATE" in judge("x", {"orig-d3d9": {"median": 0.0, "min": 0.0, "max": 0.0, "n": 3}, "patella-gpu": same}, "heap", absolute=1.0)[0], True),
			("no regression" in judge("x", {"orig-d3d9": {"median": 0.0, "min": 0.0, "max": 0.0, "n": 3},
				"patella-gpu": {"median": 0.4, "min": 0.3, "max": 0.5, "n": 3}}, "heap", absolute=1.0)[0], True),
			(aggregate([1.0, None, 3.0])["n"], 2), (aggregate([None]), None)]
		for got, want in checks:
			if got != want:
				raise SystemExit("self-test: %r, expected %r" % (got, want))
	finally:
		shutil.rmtree(folder, ignore_errors=True)
	source = open(os.path.join(ROOT, "tools", "capture", "patcap.c"), encoding="utf-8").read()
	cpp = open(os.path.join(ROOT, "src", "testmode", "ProfileWin32.cpp"), encoding="utf-8").read()
	for text in (source, cpp):
		if "# patella-profile 1" not in text and "patella-profile 1" not in text:
			raise SystemExit("self-test: a writer lost the file format's name")
	if "CMD_PROFILE_START 4" not in source or cr.CMD_PROFILE_START != 4 or cr.CMD_PROFILE_STOP != 5:
		raise SystemExit("self-test: the commands' numbers differ between patcap.c and compare_rendering.py")
	print("profiling.py: self-test passed (%d checks)" % len(checks))
	return 0


def main():
	parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
	parser.add_argument("--out", default=os.path.join(PROFILE, "result"))
	parser.add_argument("--reps", type=int, default=5, help="runs per client (default 5, alternating)")
	parser.add_argument("--variants", default="gpu", help="the x64 client's devices, gpu and/or cpu (default gpu; the Tibia.exe "
		"runs come with gpu)")
	parser.add_argument("--time", type=float, default=10.0, help="seconds of a time window")
	parser.add_argument("--allocs", type=float, default=4.0, help="seconds of an allocs window")
	parser.add_argument("--gpu", type=float, default=4.0, help="seconds of a gpu window")
	parser.add_argument("--patella", default=ct.DEFAULT_PATELLA, help="the x64 Patella.exe (its test build next to it runs)")
	parser.add_argument("--report-only", action="store_true", help="the report from --out again, no client")
	parser.add_argument("--self-test", action="store_true", help="the parser and numbers on made-up data; no client")
	parser.add_argument(sandbox.DESKTOP, action="store_true", help=argparse.SUPPRESS)
	args = parser.parse_args()
	if args.self_test:
		return self_test()
	args.out = os.path.abspath(args.out)
	args.patella = os.path.abspath(args.patella)
	os.makedirs(args.out, exist_ok=True)
	if not args.report_only:
		run_all(args)
	report(args.out, gather(args.out))
	return 0


if __name__ == "__main__":
	testserver.isolate(skip=SKIP_ARGS)
	if not sandbox.inside() and not any(a in sys.argv[1:] for a in SKIP_ARGS):
		cr.build_capture("capture-x86")  # no compiler inside the Sandbox
		os.makedirs(PROFILE, exist_ok=True)
	sandbox.redirect(writable=[PROFILE], skip=SKIP_ARGS, display=(1024, 768))
	try:
		sys.exit(main())
	except (ProfileError, ct.RunError, sc.SessionError, cr.RunError, px.ProtocolError) as e:
		traceback.print_exc()
		print("profiling.py: %s" % e, file=sys.stderr)
		sys.exit(1)
