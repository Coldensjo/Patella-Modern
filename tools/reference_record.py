r"""The record step of the reference recordings (Patella-Modern issue #23): Windows, Tibia.exe.

python -I tools/reference.py record [--reference DIR] [--sessions a,b] [--live DIR] [--reuse-live]

1. The live recordings (the test server; skipped with --reuse-live): compare_traffic.py's session of
   Tibia.exe with the helper (live_run: Patella Tester trades with Patella Second) and compare_files.py's
   walk (record_walk), both through tools/proxy.py, into --live (traffic-live.jsonl, walk-live.jsonl).
   The test characters' player files are put back afterwards, the server is stopped.
2. The reference runs (no server): Tibia.exe in build\reference-run\ref\orig, patched to the proxy's
   port and importing patcap.dll (tools/capture/patcap.c: the virtual clock, the main timer's ticks,
   the fixed cursor, the frame capture), with TZ=UTC0, plays each session against tools/proxy.py
   replaying its server side (the live recording, or one crafted from it as compare_files.py crafts
   them), driven by the sessions' own code (compare_rendering.py session, compare_files.py
   options_session, walk_session, ...). The Recorder sees every window message posted to Tibia.exe's
   window (login_client.post) and every advance of patcap's clock and writes them as the test-mode
   script; at every checkpoint (every settle of the clock) it reads Tibia.exe's state (client_state.py
   state() and gui()) and captures its frame.
3. Per session <reference>\<session>\ (tools/reference.py): meta.json, session.script, replay.txt,
   server.jsonl (what the proxy replayed), reference.jsonl (the proxy's events of the run: Tibia.exe's
   packets, the server packets with their virtual time), states\, frames\, files\, saved\, seed\,
   Tibia.cfg.start.
"""

import contextlib
import ctypes
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import reference as ref  # noqa: E402
import compare_traffic as ct  # noqa: E402
import compare_files as cf  # noqa: E402
import compare_rendering as cr  # noqa: E402
import session_client as sc  # noqa: E402
import login_client as lc  # noqa: E402
import proxy as px  # noqa: E402

ROOT = ref.ROOT
RECORD = os.path.join(ROOT, "build", "reference-run")
LIVE_RUNS = os.path.join(RECORD, "live")  # run folders of the live recordings (no patcap)
REF_RUNS = os.path.join(RECORD, "ref")    # run folder of the reference runs (patcap)
WM_CLOSE = 0x0010
CURSOR = (1020, 760)  # tools/capture/patcap.c CURSOR_X, CURSOR_Y
log = ct.log
RunError = ct.RunError


def make_run(name, source, exe, port, patcap):
	"""compare_traffic.setup for one run folder: the data files, the client patched by patch_client.py
	(127.0.0.1, the server's RSA key, port: the proxy's login port or None), with patcap: its import
	and the 32-bit patcap.dll."""
	folder = ct.run_dir(name)
	os.makedirs(folder, exist_ok=True)
	for data in ct.DATA_FILES:
		target = os.path.join(folder, data)
		if not os.path.isfile(target) or ct.file_sha(target) != ct.file_sha(os.path.join(ROOT, data)):
			shutil.copyfile(os.path.join(ROOT, data), target)
	ct.RUN_EXES[name] = exe
	path = os.path.join(folder, exe)
	if os.path.isfile(path):
		os.remove(path)
	command = [sys.executable, "-I", os.path.join(ROOT, "tools", "patch_client.py"), source, path, "--host",
		"127.0.0.1", "--rsa", ct.PEM]
	if port:
		command += ["--port", str(port)]
	result = subprocess.run(command, capture_output=True, text=True)
	if result.returncode != 0:
		raise RunError("patch_client.py failed for %s: %s" % (name, result.stderr.strip()))
	if patcap:
		preset = cr.capture_preset(path)
		cr.build_capture(preset)
		cr.add_import(path, path)
		shutil.copyfile(cr.capture_dll(preset), os.path.join(folder, "patcap.dll"))


def setup_live():
	ct.set_folder(LIVE_RUNS)
	make_run("orig", ct.CLIENTS["orig"]["source"], "Tibia.exe", px.PROXY_LOGIN_PORT, False)
	make_run("helper", ct.HELPER["source"], "Tibia.exe", None, False)


def setup_ref():
	ct.set_folder(REF_RUNS)
	make_run("orig", ct.CLIENTS["orig"]["source"], "Tibia.exe", px.PROXY_LOGIN_PORT, True)


# --- the live recordings ---------------------------------------------------------------------------

def check_ports():
	busy = ref.ports_free((7171, 7172, px.PROXY_LOGIN_PORT, px.PROXY_GAME_PORT))
	if busy:
		raise RunError("ports %s are in use: is a server, proxy or client running?" % busy)


def live_recordings(live, sessions):
	"""traffic-live.jsonl (compare_traffic.py's live session of Tibia.exe) and walk-live.jsonl
	(compare_files.py's record_walk) in live, as the sessions need them."""
	os.makedirs(live, exist_ok=True)
	if ct.server_running():
		raise RunError("the test server is running: stop it first (python -I tools/testserver.py stop)")
	check_ports()
	setup_live()
	need_traffic = "traffic" in sessions
	need_walk = any(s in sessions for s in ("walk", "error"))
	if need_traffic:
		players = ct.snapshot_players()
		try:
			path = ct.live_run("orig", live, players)
		finally:
			ct.restore_players(players)
			log("player files of the test characters restored")
		shutil.copyfile(path, os.path.join(live, "traffic-live.jsonl"))
	if need_walk:
		cf.record_walk(live, cf.DEFAULT_WALK)


# --- the recorder ----------------------------------------------------------------------------------

class Recorder:
	"""Writes a reference run as it happens: the script lines, the dumps, the frames."""

	def __init__(self, folder, name):
		self.folder = folder
		self.name = name
		self.driver = None
		self.proxy = None
		self.lines = []
		self.dumps = 0
		self.frames = 0
		self.saves = 0
		self.masks = {}
		self.state_masks = {}
		self.checkpoints = []
		self.original_post = None
		self.original_advance = None
		os.makedirs(os.path.join(folder, "states"))
		os.makedirs(os.path.join(folder, "frames"))

	def attach(self, driver, proxy):
		self.driver = driver
		self.proxy = proxy
		self.original_post = lc.post

		def post(hwnd, msg, wparam=0, lparam=0):
			self.original_post(hwnd, msg, wparam, lparam)
			if driver.hwnd is not None and hwnd == driver.hwnd:
				self.posted(msg, wparam, lparam)

		lc.post = post
		self.original_advance = driver.advance
		driver.advance = self.advance

	def detach(self):
		if self.original_post is not None:
			lc.post = self.original_post
			self.original_post = None
		if self.driver is not None and self.original_advance is not None:
			self.driver.advance = self.original_advance

	def posted(self, msg, wparam, lparam):
		if msg == WM_CLOSE:
			self.lines.append("quit")
		else:
			self.lines.append("post 0x%04x 0x%x 0x%x" % (msg, wparam & 0xffffffffffffffff, lparam & 0xffffffffffffffff))

	def advance(self, ms=cr.ADVANCE_MS, ticks=cr.ADVANCE_TICKS):
		# The line first: a client that ends during the advance (patch-exe, until issue #75) got it too.
		self.lines.append("advance %d %d" % (ms, ticks))
		self.original_advance(ms, ticks)

	def settle(self):
		"""compare_rendering.py's settle: the server quiet, the clock 2000 ms and 20 ticks on, quiet again."""
		if self.proxy is not None:
			self.proxy.wait_idle(0.5, 60)
		time.sleep(0.3)
		self.advance()
		if self.proxy is not None:
			self.proxy.wait_idle(0.5, 60)
		time.sleep(0.3)

	def dump(self, name):
		self.dumps += 1
		mem = self.driver.mem
		dump = {"format": "patella-state 1", "step": self.dumps, "name": name, "time": self.driver.cap.get("Now"),
			"state": mem.state(), "gui": mem.gui()}
		ref.write_json(os.path.join(self.folder, "states", "state-%03d.json" % self.dumps), dump)
		self.lines.append("dump  # %d: %s" % (self.dumps, name))

	def frame(self, name, masks=None):
		self.frames += 1
		bmp = os.path.join(self.folder, "frames", "capture.bmp")
		width, height, api = self.driver.capture(bmp, cr.SKIP_FRAMES)
		picture = cr.read_bmp(bmp)
		os.remove(bmp)
		cr.write_png(os.path.join(self.folder, "frames", "frame-%03d.png" % self.frames), *picture)
		if masks:
			self.masks[str(self.frames)] = [list(m) for m in masks]
		self.lines.append("frame  # %d: %s (%dx%d, %s)" % (self.frames, name, width, height, api))

	def checkpoint(self, name, frame=True, masks=None):
		self.checkpoints.append(name)
		log("checkpoint %s" % name)
		self.dump(name)
		if masks:
			self.state_masks[str(self.dumps)] = [list(m) for m in masks]
		if frame:
			self.frame(name, masks)

	def save(self, file):
		self.saves += 1
		target = os.path.join(self.folder, "saved")
		os.makedirs(target, exist_ok=True)
		shutil.copyfile(os.path.join(ct.run_dir("orig"), file), os.path.join(target, "saved-%03d-%s" % (self.saves, file)))
		self.lines.append("save %s" % file)

	def wait(self, name):
		"""A session's wait(name): settle, then a checkpoint."""
		self.settle()
		self.checkpoint(name)

	def close(self):
		"""WM_CLOSE (the script's quit); the exit code."""
		time.sleep(0.5)
		return self.driver.close()

	def script(self, comment):
		head = ["# Patella-Modern reference recording, session %s (tools/reference.py record, %s)." % (self.name,
			datetime.date.today().isoformat()),
			"# Generated from Tibia.exe's reference run under patcap.dll: every window message the record step",
			"# posted to its window, every advance of patcap's clock, a dump (and a frame) at every checkpoint.",
			"# Played by PatellaTest.exe with --test-replay replay.txt (the server side, no server).",
			"# " + comment, "", "clock manual", "cursor %d %d" % CURSOR, ""]
		return "\n".join(head + self.lines) + "\n"


# --- the sessions ----------------------------------------------------------------------------------

def session_traffic(rec, driver, proxy):
	"""compare_rendering.py's scenes (windowed): the start screen, the option dialogs, the login, the
	game session of compare_traffic.py (session_client.run_session) with the automap zoom and the side
	windows, the logout; a dump and a frame at every scene. Its look step is "look north", the frozen reference
	recordings' (compare_traffic.py and compare_rendering.py look at the player's own field since issues #137, #140)."""
	cr.session(driver, proxy, lambda name, masks=None: rec.checkpoint(name, True, masks), device_choice=True,
		look=sc.LOOK_NORTH)
	return {"exit": rec.close()}


def session_options(rec, driver, proxy):
	"""compare_files.py's options scenario: every option dialog; Tibia.cfg after the dialog (save) and
	after the exit."""
	def collect(name):
		# No frame: Tibia.exe draws with its OpenGL engine now (the advanced options' engine switch),
		# compared with the Vulkan engine nowhere else.
		rec.checkpoint(name, frame=False)
		rec.save("Tibia.cfg")

	cf.options_session(driver, collect)
	return {"exit": rec.close()}


def session_walk(rec, driver, proxy, keys):
	"""compare_files.py's walk: login, automap marks, zoom, the 19 actions, the channel saved, logout;
	a checkpoint after every settle."""
	cf.walk_session(driver, rec.wait, proxy, cf.WALKS[cf.DEFAULT_WALK], keys)
	return {"exit": rec.close()}


def quiet(proxy):
	return lambda name: proxy.wait_idle(0.5, 60)


def error_box(rec, driver):
	caption, texts, code = cf.close_error_box(driver)
	return {"exit": code, "messages": [[caption, texts[0]]] if texts else []}


def session_error(rec, driver, proxy):
	"""compare_files.py's error: the game login, then the server packet 0xff; the error box (closed by
	the record step; the test client does not show it). No settle: the clock stands still (a modal box
	runs its own message loop, where patcap takes no commands)."""
	cf.game_login(driver, quiet(proxy), proxy)
	return error_box(rec, driver)


# patch_session (compare_files.py's patch and patch-exe: the login connections of the patches, the message
# "Updating" and the client's close) went with the patcher in issue #75.


# --- the display modes (issue #48) ----------------------------------------------------------------

class DEVMODEW(ctypes.Structure):
	"""wingdi.h's DEVMODEW, the display members."""
	_fields_ = [("dmDeviceName", ctypes.c_wchar * 32), ("dmSpecVersion", ctypes.c_ushort),
		("dmDriverVersion", ctypes.c_ushort), ("dmSize", ctypes.c_ushort), ("dmDriverExtra", ctypes.c_ushort),
		("dmFields", ctypes.c_ulong), ("dmPositionX", ctypes.c_long), ("dmPositionY", ctypes.c_long),
		("dmDisplayOrientation", ctypes.c_ulong), ("dmDisplayFixedOutput", ctypes.c_ulong), ("dmColor", ctypes.c_short),
		("dmDuplex", ctypes.c_short), ("dmYResolution", ctypes.c_short), ("dmTTOption", ctypes.c_short),
		("dmCollate", ctypes.c_short), ("dmFormName", ctypes.c_wchar * 32), ("dmLogPixels", ctypes.c_ushort),
		("dmBitsPerPel", ctypes.c_ulong), ("dmPelsWidth", ctypes.c_ulong), ("dmPelsHeight", ctypes.c_ulong),
		("dmDisplayFlags", ctypes.c_ulong), ("dmDisplayFrequency", ctypes.c_ulong)]


def machine_display_modes():
	"""The display modes Tibia.exe's Graphics Options dialog lists on this machine (meta.json's
	display_modes, tools/reference.py): EnumDisplaySettings' modes as the dialog filters them (DirectDraw's
	EnumDisplayModes gives the same list here, issue #20; reference.listed_modes)."""
	mode = DEVMODEW()
	mode.dmSize = ctypes.sizeof(DEVMODEW)
	found, index = [], 0
	while ctypes.windll.user32.EnumDisplaySettingsW(None, index, ctypes.byref(mode)):
		found.append([mode.dmPelsWidth, mode.dmPelsHeight, mode.dmBitsPerPel])
		index += 1
	return ref.listed_modes(found)


# --- a reference run -------------------------------------------------------------------------------

def reference_run(reference, name, events, session, seed=None, comment=""):
	"""One reference run of Tibia.exe (patcap, the replayed server side events, or no server with
	None): the session folder <reference>\\<name>."""
	folder = ref.session_dir(reference, name)
	if os.path.isdir(folder):
		shutil.rmtree(folder)
	os.makedirs(folder)
	log("=== reference run: %s" % name)
	ct.clean_run_dir("orig", keep=("patcap.dll",))
	ct.reset_cfg("orig")
	shutil.copyfile(os.path.join(ct.run_dir("orig"), "Tibia.cfg"), os.path.join(folder, "Tibia.cfg.start"))
	if seed:
		os.makedirs(os.path.join(folder, "seed"))
		for file, data in seed.items():
			with open(os.path.join(ct.run_dir("orig"), file), "wb") as f:
				f.write(data)
			with open(os.path.join(folder, "seed", file), "wb") as f:
				f.write(data)
	before = ref.snapshot(ct.run_dir("orig"), skip=("Tibia.exe",))
	modes = machine_display_modes()
	env = dict(os.environ, TZ=ref.TIME_ZONE)
	driver = cr.Driver(ct.run_dir("orig"), "Tibia.exe", None, env=env)
	rec = Recorder(folder, name)
	proxy = None
	if events is not None:
		ref.write_json(os.path.join(folder, "server.json"), {"events": len(events)})
		with open(os.path.join(folder, "server.jsonl"), "w", encoding="utf-8", newline="\n") as f:
			for e in events:
				f.write(json.dumps(e, sort_keys=True) + "\n")
		os.remove(os.path.join(folder, "server.json"))
		proxy = px.Proxy(os.path.join(folder, "reference.jsonl"), px.load_private_key(ct.PEM), replay=events,
			log=log, vclock=lambda: driver.cap.get("Now") if getattr(driver, "cap", None) is not None else 0)
	result = {}
	try:
		with proxy if proxy is not None else contextlib.nullcontext():
			driver.start()
			rec.attach(driver, proxy)
			if session is not session_traffic:
				# The start screen (compare_rendering.py's session settles first too: the client shows its
				# first frame only once the clock has moved).
				rec.settle()
				rec.checkpoint("start")
			result = session(rec, driver, proxy)
	finally:
		rec.detach()
		driver.kill()
	problems = (proxy.mismatches + proxy.errors) if proxy is not None else []
	if machine_display_modes() != modes:
		problems.append("the display modes changed during the run (issue #48)")
	after = ref.snapshot(ct.run_dir("orig"), skip=("Tibia.exe",))
	written = []
	os.makedirs(os.path.join(folder, "files"))
	for file, digest in after.items():
		if before.get(file) != digest:
			shutil.copyfile(os.path.join(ct.run_dir("orig"), file), os.path.join(folder, "files", file))
			written.append(file)
	removed = sorted(n for n in before if n not in after)
	with open(os.path.join(folder, "session.script"), "w", encoding="utf-8", newline="\n") as f:
		f.write(rec.script(comment))
	if events is not None:
		with open(os.path.join(folder, "replay.txt"), "w", encoding="ascii", newline="\n") as f:
			f.write(ref.replay_text(events, px.load_events(os.path.join(folder, "reference.jsonl"))))
	meta = {"format": ref.FORMAT, "session": name, "tools": list(ref.SESSION_TOOLS[name]),
		"recorded": datetime.datetime.now().isoformat(timespec="seconds"),
		"tibia_sha256": ct.file_sha(ct.CLIENTS["orig"]["source"]),
		"data_sha256": {d: ct.file_sha(os.path.join(ROOT, d)) for d in ct.DATA_FILES},
		"server": events is not None, "exit": result.get("exit", 0), "messages": result.get("messages", []),
		"dumps": rec.dumps, "frames": rec.frames, "frame_masks": rec.masks, "state_masks": rec.state_masks, "checkpoints": rec.checkpoints,
		"written": written, "removed": removed, "seed": sorted(seed or {}), "time_zone": ref.TIME_ZONE, "display_modes": modes,
		"proxy_problems": [str(p) for p in problems], "comment": comment}
	ref.write_json(os.path.join(folder, "meta.json"), meta, indent=1)
	log("%s: exit %s, %d dumps, %d frames, written %s, removed %s, proxy problems %s" % (name, meta["exit"],
		rec.dumps, rec.frames, written, removed, problems or "none"))
	if problems:
		raise RunError("%s: the reference run had proxy problems: %s" % (name, problems))
	return meta


def record_main(args):
	import ctypes
	ctypes.windll.user32.SetProcessDPIAware()
	reference = os.path.abspath(args.reference)
	live = os.path.abspath(args.live)
	sessions = ref.selected(args.sessions)
	os.makedirs(reference, exist_ok=True)
	if not args.reuse_live:
		live_recordings(live, sessions)
	check_ports()
	setup_ref()
	traffic = os.path.join(live, "traffic-live.jsonl")
	walk = os.path.join(live, "walk-live.jsonl")
	failed = []

	def events_of(path):
		if not os.path.isfile(path):
			raise RunError("no live recording %s (record without --reuse-live)" % path)
		return px.load_events(path)

	for name in sessions:
		try:
			if name == "traffic":
				reference_run(reference, name, events_of(traffic), session_traffic,
					comment="compare_rendering.py's scenes on compare_traffic.py's live session")
			elif name == "options":
				reference_run(reference, name, None, session_options, comment="compare_files.py's options")
			elif name == "walk":
				events = events_of(walk)
				keys = cf.walk_keys(events)
				reference_run(reference, name, events, lambda r, d, p: session_walk(r, d, p, keys),
					comment="compare_files.py's walk (%d actions)" % len(keys))
			elif name == "error":
				reference_run(reference, name, cf.craft_error(events_of(walk)), session_error,
					comment="compare_files.py's error")
		except sc.StartError:  # issue #148: a client that cannot start ends the recording
			raise
		except Exception as e:  # noqa: BLE001 - every session is tried
			import traceback
			traceback.print_exc()
			failed.append("%s: %s" % (name, e))
			cr.kill_clients()
	ct.set_folder(os.path.join(ROOT, "build", "traffic"))
	if failed:
		print("record: FAILED: %s" % "; ".join(failed))
		return 2
	# The set's manifest (issue #93): every file of the folder, the commit, the tools, the time zone.
	capture = cr.capture_dll("capture-x86")
	programs = [("patcap.dll", capture)] if os.path.isfile(capture) else []
	count, total = ref.write_manifest(reference, programs)
	print("record: %d sessions in %s; its %s: %d files, %d bytes" % (len(sessions), reference, ref.data_manifest.NAME,
		count, total))
	return 0
