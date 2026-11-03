r"""Compare the files Patella.exe and the original Tibia.exe write (issue #21).

Usage (from the repository or a worktree; python -I tools/build.py oracle-x86 first; the test server of
issue #18 must be stopped, the script starts and stops it for the one live recording):
  python -I tools/compare_files.py [--out build\files\result] [--only a,b] [--skip-live] [--patella EXE]

Both clients play the same sessions in run folders with copies of the data files
(build\files\orig, build\files\patella, made like tools/compare_traffic.py's); the files each
client creates, changes or deletes in its folder are collected and compared byte for byte.
Scenarios (--only takes their names):

  options  No server. The start window's Options: every check box of the general, console and
           network options, the next fullscreen resolution, the advanced graphics options (three
           check boxes, the frame rate limit, the OpenGL engine), four hotkeys (two sent
           automatically). Tibia.cfg after the options dialog closes and after the exit.
  walk     A live session (Tibia.exe through tools/proxy.py against the test server) is
           recorded once and replayed to both clients: login, automap marks set, edited and
           deleted, the automap zoomed, 111 steps over floors 7, 6, 8 and into a second automap
           block, the default channel saved, logout. The *.map files, Default.txt and Tibia.cfg.
  error    The walk recording up to the game login and the first packets, then a server packet
           with the unknown type 0xff: HandleException writes error.txt and shows the critical
           error box, which is closed (exit code 1). error.txt and Tibia.cfg.
  error-login  The login server answers with the unknown type 0x63; error.txt has the same
           earlier report in both folders (its dialog at the start is answered with Send), so the
           new report is appended.
  report   Both folders get the same error.txt (Tibia.exe's from "error"): the error report
           dialog at the start (a comment is typed and sent), login to the game, where the
           client sends the report (0xe8) and deletes error.txt, logout. The client packets of
           both clients are compared, and the folder (error.txt gone).
  patch    Crafted login server answers (no server): a difference patch of Tibia.dat (all four
           operations, all count forms, a new signature), a difference patch of Tibia.spr (new
           signature, one copy of the rest), the whole Tibia.pic (new signature) and the whole
           Patch.exe; each is installed (Tibia.da_ -> Tibia.dat ...) and the client reconnects.
           The patched files, Tibia.cfg, and the login packets (with the new signatures).
  patch-exe  A difference patch of Tibia.exe (written to Tibia.bak; the client then closes to
           run patch.exe, which is not there). Patella's folder has a copy of the patched
           Tibia.exe as the patch's input, so both patch the same file.

Masked, after two runs of Tibia.exe showed that they differ from run to run: the time in
error.txt (ctime of the crash) and in Default.txt ("Channel saved at"). Everything else is
compared byte for byte, error.txt's "<file> <line>" too: every error site of our sources names
the original's line (issue #35, tools/lines.py). Report: <out>\report.txt; exit code 0 when
nothing differs.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_traffic as ct  # noqa: E402
import proxy as px  # noqa: E402
import session_client as sc  # noqa: E402
import login_client as lc  # noqa: E402

ROOT = ct.ROOT
FILES = os.path.join(ROOT, "build", "files")
CLIENTS = ct.CLIENTS
DATA_FILES = ("Tibia.dat", "Tibia.spr", "Tibia.pic")
VK_DOWN = sc.VK_DOWN
user32 = ctypes.WinDLL("user32", use_last_error=True)
WM_COMMAND = 0x0111

RunError = ct.RunError
log = ct.log


# --- run folders -------------------------------------------------------------------------

# A run is named after its run folder: "orig", "patella", and "orig-repeat", the second run of
# Tibia.exe (--repeat), which has a folder of its own (issue #12).
REPEAT_RUN = "orig-repeat"


def folder(run):
	return ct.run_dir(run)


def client_exe(run):
	return CLIENTS[ct.client_of(run)]["exe"]


def sha(data):
	return hashlib.sha256(data).hexdigest()


def snapshot(run):
	"""{name: sha256} of the files in the run's folder, without the client itself."""
	out = {}
	path = folder(run)
	for name in sorted(os.listdir(path)):
		full = os.path.join(path, name)
		if os.path.isfile(full) and name != client_exe(run):
			with open(full, "rb") as f:
				out[name] = sha(f.read())
	return out


def prepare(run, seed=None, cfg=None):
	"""The folder as every scenario starts (compare_traffic.clean_run_dir: emptied, the data files as
	in the root, the patched client), then Tibia.exe (in Patella's folder the patched Tibia.exe, the
	input of the Tibia.exe patch), Tibia.cfg (compare_traffic's), the files in seed ({name: bytes}),
	nothing else."""
	path = folder(run)
	ct.clean_run_dir(run)
	if ct.client_of(run) == "patella":
		shutil.copyfile(os.path.join(folder("orig"), "Tibia.exe"), os.path.join(path, "Tibia.exe"))
	with open(os.path.join(path, "Tibia.cfg"), "w", encoding="ascii", newline="\r\n") as f:
		f.write(cfg or ct.TIBIA_CFG)
	for name, data in (seed or {}).items():
		with open(os.path.join(path, name), "wb") as f:
			f.write(data)
	return snapshot(run)


def collect(run, before, target):
	"""The files the run created or changed (copied to target) and the ones it deleted."""
	after = snapshot(run)
	os.makedirs(target, exist_ok=True)
	written = {}
	for name, digest in after.items():
		if before.get(name) != digest:
			with open(os.path.join(folder(run), name), "rb") as f:
				written[name] = f.read()
			with open(os.path.join(target, name), "wb") as f:
				f.write(written[name])
	removed = sorted(name for name in before if name not in after)
	return {"written": written, "removed": removed}


# --- the client --------------------------------------------------------------------------

def start(run, arguments=()):
	info = CLIENTS[ct.client_of(run)]
	driver = sc.Driver(folder(run), info["exe"], info["pdb"])
	driver.start(arguments)
	return driver


def close(driver, expect=0):
	code = driver.close()
	if code != expect:
		raise RunError("%s exited with code %d, expected %d" % (driver.exe, code, expect))


def dialog_windows(pid):
	"""Visible top-level message boxes (#32770) of a process: [(hwnd, caption, text)]."""
	found = []
	WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

	def callback(hwnd, lparam):
		owner = wt.DWORD()
		user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
		if owner.value == pid and user32.IsWindowVisible(hwnd):
			name = ctypes.create_unicode_buffer(64)
			user32.GetClassNameW(hwnd, name, 64)
			if name.value == "#32770":
				caption = ctypes.create_unicode_buffer(256)
				user32.GetWindowTextW(hwnd, caption, 256)
				texts = []

				def child(h, l):
					buffer = ctypes.create_unicode_buffer(1024)
					user32.GetWindowTextW(h, buffer, 1024)
					cls = ctypes.create_unicode_buffer(64)
					user32.GetClassNameW(h, cls, 64)
					if cls.value == "Static" and buffer.value:
						texts.append(buffer.value)
					return True

				user32.EnumChildWindows(hwnd, WNDENUMPROC(child), 0)
				found.append((hwnd, caption.value, "\n".join(texts)))
		return True

	user32.EnumWindows(WNDENUMPROC(callback), 0)
	return found


def close_error_box(driver, timeout=60):
	"""Waits for FatalError's message box and presses its OK (again for every further box); the
	client then ends with exit(1). Returns (the first box's caption and text, all boxes' texts, the
	exit code)."""
	deadline = time.time() + timeout
	boxes = []
	while time.time() < deadline:
		if driver.proc.poll() is not None:
			break
		for hwnd, caption, text in dialog_windows(driver.proc.pid):
			if (hwnd, caption, text) not in boxes:
				log("message box %r: %r" % (caption, text))
				boxes.append((hwnd, caption, text))
			time.sleep(0.5)
			# the box's button: an MB_OK box gives its one button the ID IDCANCEL (2), not IDOK
			button = user32.FindWindowExW(hwnd, None, "Button", None)
			if button:
				user32.PostMessageW(hwnd, WM_COMMAND, user32.GetDlgCtrlID(button), button)
		time.sleep(0.5)
	if driver.proc.poll() is None:
		raise RunError("the client still runs %d s after its first message box (boxes: %s)"
			% (timeout, [b[1:] for b in boxes]))
	if not boxes:
		raise RunError("the client exited (code %d) without a message box" % driver.proc.returncode)
	log("the client exited with code %d" % driver.proc.returncode)
	if driver.mem is not None:
		driver.mem.close()
		driver.mem = None
	return boxes[0][1], [b[2] for b in boxes], driver.proc.returncode


def game_login(driver, wait, proxy):
	"""sc.login (start screen to the game); its check of the client's TCP table for the game
	connection missed an established connection twice (the proxy had accepted it), so the proxy's
	own events count as well (sc.login's proxy, issue #12)."""
	sc.login(driver, wait, sc.CHARACTER, proxy)


def nodes(driver, cls):
	return [n for n in driver.mem.gui() if n["class"] == cls]


def wait_node(driver, cls, timeout=10):
	"""The first window of class cls, once it is there and its place has stopped changing (a dialog
	appears in the tree before it is moved to its place: a click on one of its buttons at the first
	place read can miss)."""
	deadline = time.time() + timeout
	while time.time() < deadline:
		found = nodes(driver, cls)
		if found:
			node = found[0]
			while time.time() < deadline:
				time.sleep(0.2)
				again = [n for n in nodes(driver, cls) if n["ptr"] == node["ptr"]]
				if not again:
					break
				if (again[0]["x"], again[0]["y"], again[0]["w"], again[0]["h"]) == (node["x"], node["y"], node["w"], node["h"]):
					return again[0]
				node = again[0]
			continue
		time.sleep(0.2)
	raise RunError("no window of class %s after %d s" % (cls, timeout))


def click_open(driver, find, cls, wait=4, attempts=3):
	"""Clicks the button find() returns (read again before every click) until a window of class cls
	is open: a click that opened nothing (issue #12: no TAdvancedGraphicsDialog within 10 s in a
	first attempt) is repeated, at most `attempts` times, each after `wait` seconds without the
	window."""
	for attempt in range(1, attempts + 1):
		click_node(driver, find())
		try:
			return wait_node(driver, cls, wait)
		except RunError:
			if attempt == attempts:
				raise RunError("no window of class %s after %d clicks, %g s each" % (cls, attempts, wait))
			log("no %s after the click: once more" % cls)


def wait_gone(driver, cls, timeout=10):
	deadline = time.time() + timeout
	while time.time() < deadline:
		if not nodes(driver, cls):
			return
		time.sleep(0.2)
	raise RunError("the window of class %s is still open after %d s" % (cls, timeout))


def click_node(driver, node, right=False, dx=None):
	x = node["x"] + (node["w"] // 2 if dx is None else dx)
	driver.click(x, node["y"] + node["h"] // 2, right=right)


def click_ptr(driver, ptr, dx=None):
	click_node(driver, driver.node_by_ptr(ptr), dx=dx)


def button(driver, text):
	for n in driver.mem.gui():
		if n["class"] == "TTextButton" and n.get("text") == text:
			return n
	raise RunError("no button %r" % text)


def member(driver, component, path):
	"""The pointer member `path` (Type.Member, client_state.LAYOUT_FIELDS) of the component at
	`component`: its offset from the layout (the PDB for Patella.exe), 4 or 8 bytes."""
	return driver.mem.ptr(component + driver.mem.off(path))


def checked(driver, box):
	"""TCheckBox's Checked (+0x30 in the original, after the label pointer)."""
	return driver.mem.read(box + driver.mem.off("TCheckBox.Checked"), 1)[0] != 0


def toggle(driver, box):
	"""A click on the check box's box (left of its label); the state must flip."""
	old = checked(driver, box)
	click_ptr(driver, box, dx=6)
	time.sleep(0.2)
	if checked(driver, box) == old:
		raise RunError("the check box at 0x%x did not change" % box)


# --- scenario: options -------------------------------------------------------------------

def options_session(driver, collect_cfg):
	"""The start window's Options and every option dialog; collect_cfg(name) keeps Tibia.cfg."""
	def open_options():
		return click_open(driver, lambda: button(driver, "Options"), "TOptionsDialog")

	def choose(text, cls):
		return click_open(driver, lambda: button(driver, text), cls)

	def ok(cls):
		driver.char(13)
		wait_gone(driver, cls)
		time.sleep(0.5)

	open_options()
	log("options: general")
	body = choose("General", "TGeneralOptionsDialog")
	for name in ("TextualEffects", "CreatureNames", "ShowHints", "ClassicControl", "AutoChaseOff"):
		toggle(driver, member(driver, body["ptr"], "TGeneralOptionsDialog." + name))
	ok("TGeneralOptionsDialog")
	wait_node(driver, "TOptionsDialog")
	log("options: console")
	body = choose("Console", "TConsoleOptionsDialog")
	for name in ("InfoMessages", "EventMessages", "StatusMessages", "TimeStamps", "PrivateMessages"):
		toggle(driver, member(driver, body["ptr"], "TConsoleOptionsDialog." + name))
	ok("TConsoleOptionsDialog")
	wait_node(driver, "TOptionsDialog")
	log("options: network")
	body = choose("Network", "TNetworkOptionsDialog")
	toggle(driver, member(driver, body["ptr"], "TNetworkOptionsDialog.TestServerLogin"))
	ok("TNetworkOptionsDialog")
	wait_node(driver, "TOptionsDialog")
	log("options: graphics")
	choose("Graphics", "TGraphicsOptionsDialog")
	driver.key(VK_DOWN)  # the next fullscreen resolution
	time.sleep(0.3)
	body = click_open(driver, lambda: button(driver, "Advanced"), "TAdvancedGraphicsDialog")
	for name in ("LightEffects", "NoStretching", "WindowsMousepointer"):
		toggle(driver, member(driver, body["ptr"], "TAdvancedGraphicsDialog." + name))
	scroll = member(driver, body["ptr"], "TAdvancedGraphicsDialog.Framerate")  # three clicks on its left button
	left = member(driver, scroll, "THorizontalScrollBar.LeftButton")
	for _ in range(3):
		click_ptr(driver, left)
	if OPTIONS_ENGINE_SWITCH:
		click_ptr(driver, member(driver, body["ptr"], "TAdvancedGraphicsDialog.OpenGLButton"))
		time.sleep(3)
		body = wait_node(driver, "TAdvancedGraphicsDialog")
	ok("TAdvancedGraphicsDialog")
	wait_node(driver, "TGraphicsOptionsDialog")
	ok("TGraphicsOptionsDialog")
	wait_node(driver, "TOptionsDialog")
	log("options: hotkeys")
	body = choose("Hotkeys", "THotkeyDialog")
	box = member(driver, body["ptr"], "THotkeyDialog.AutoSendCheckBox")
	for number, (text, auto) in enumerate((("hello patella", False), ("exura", True), ("hi", True), ("", False),
			("trade offer", False))):
		driver.key(VK_DOWN)  # the next line: F1, F2, ...
		time.sleep(0.2)
		if driver.mem.i32(body["ptr"] + driver.mem.off("THotkeyDialog.Selected")) != number:
			raise RunError("hotkey %d not selected" % number)
		driver.type(text)
		if auto:
			toggle(driver, box)
	ok("THotkeyDialog")
	wait_node(driver, "TOptionsDialog")
	driver.char(13)  # the options dialog: saves
	wait_gone(driver, "TOptionsDialog")
	time.sleep(1)
	collect_cfg("after-options")


OPTIONS_ENGINE_SWITCH = True  # the advanced options' OpenGL button: a live engine switch


def run_options(run, out):
	before = prepare(run)
	driver = start(run)
	target = os.path.join(out, "options", ct.client_of(run))
	os.makedirs(target, exist_ok=True)
	try:
		def keep(name):
			shutil.copyfile(os.path.join(folder(run), "Tibia.cfg"), os.path.join(target, "%s.Tibia.cfg" % name))

		options_session(driver, keep)
		close(driver)
	finally:
		driver.kill()
	return collect(run, before, target)


# --- scenario: walk ----------------------------------------------------------------------

# From the Rookgaard temple (32097,32219,7), where the test character starts: north to the
# wooden stairs up (floor 6), one step back down through the hole next to them, to the stairs
# down (floor 8), back up, then south until the map shows y 32256 and more (the second automap
# block, 125/126/7); the shortest way south goes up the wooden stairs and down the hole once
# more. Planned on the server's map (gameserver\data\map, objects.srv and moveuse.dat's
# MoveTopRel rules; notes/written-files.md). FLOOR_CHANGES: the field a step goes onto -> where
# the player ends up.
ROUTE = ("N" * 29 + "E" + "S" + "WNW" + "N"
	+ "ESESSSSSSSSSSSESSSSSSSSSSEEEEEEESEESSSSSSSSSSSSSSSSSSSSSSSSSSSSSSWSWSSESSSEEESSSS")
FLOOR_CHANGES = {
	(32098, 32190, 7): (32098, 32189, 6),  # wooden stairs up
	(32098, 32190, 6): (32098, 32191, 7),  # the hole next to them
	(32096, 32190, 7): (32096, 32191, 8),  # stairs down
	(32096, 32190, 8): (32096, 32189, 7),  # stairs up
}
START = (32097, 32219, 7)
END = (32110, 32252, 7)
STEP = {"N": (0, -1, sc.VK_UP), "E": (1, 0, sc.VK_RIGHT), "S": (0, 1, VK_DOWN), "W": (-1, 0, sc.VK_LEFT)}


def player_position(driver):
	"""The player's creature's position (Creatures.cpp's table, as client_state reads it)."""
	player = driver.mem.var("PlayerID")
	for creature in driver.mem.creatures():
		if player != 0 and creature["id"] == player:
			return tuple(creature["pos"])
	return None


def menu_entry(driver, text):
	deadline = time.time() + 5
	while time.time() < deadline:
		try:
			return driver.menu_entry_point(text)
		except sc.SessionError:
			time.sleep(0.2)
	raise RunError("no context menu entry %r" % text)


def automap_point(driver, dx, dy):
	view = wait_node(driver, "TAutomapView")
	return view["x"] + view["w"] // 2 + dx, view["y"] + view["h"] // 2 + dy


def set_mark(driver, dx, dy, text, kind, entry="Set Mark"):
	driver.click(*automap_point(driver, dx, dy), right=True)
	driver.click(*menu_entry(driver, entry))
	body = wait_node(driver, "TEditMarkDialog")
	driver.type(text)
	if kind is not None:
		if not 0 <= kind < driver.mem.layout.count("TEditMarkDialog.TypeButtons"):
			raise RunError("mark symbol %d out of range" % kind)
		click_ptr(driver, driver.mem.ptr(body["ptr"] + driver.mem.off("TEditMarkDialog.TypeButtons")
			+ driver.mem.pointer_size * kind))  # TypeButtons[kind]
	driver.char(13)
	wait_gone(driver, "TEditMarkDialog")


def delete_mark(driver, dx, dy):
	driver.click(*automap_point(driver, dx, dy), right=True)
	driver.click(*menu_entry(driver, "Delete Mark"))
	time.sleep(0.5)


def key_sent(driver, proxy, vk, timeout):
	"""Presses an arrow key; True when the client sent a packet within timeout. A client sends no
	step while a creature stands on the field (it knows the field is taken), and when that
	creature moves away depends on the time the key comes, so a press without a packet is not
	part of the session."""
	count = proxy.c2s_count
	driver.key(vk)
	deadline = time.time() + timeout
	while time.time() < deadline:
		if proxy.c2s_count != count:
			return True
		time.sleep(0.05)
	return False


def walk_session(driver, wait, proxy, actions=None):
	"""Login, automap marks and zoom, the ROUTE, the default channel saved, logout. Live
	(actions None): every arrow key is repeated until the player moved (a creature may stand in
	the way); the keys that made the client send a step are recorded and returned. Replay: each
	recorded key, repeated until the client sends its step."""
	game_login(driver, wait, proxy)
	wait("login")
	time.sleep(2)
	position = player_position(driver)
	if position != START:
		raise RunError("the player starts at %s, not %s" % (position, START))
	log("walk: automap marks")
	set_mark(driver, 20, 0, "Patella one", 3)
	set_mark(driver, -12, 10, "second mark", 9)
	set_mark(driver, 0, -15, "to be deleted", None)
	set_mark(driver, 20, 0, " edited", 5, entry="Edit Mark")
	delete_mark(driver, 0, -15)
	automap = wait_node(driver, "TAutomapWindow")
	click_ptr(driver, member(driver, automap["ptr"], "TAutomapWindow.ZoomInButton"))  # zoom in: 4
	for _ in range(2):
		click_ptr(driver, member(driver, automap["ptr"], "TAutomapWindow.ZoomOutButton"))  # zoom out: 2, 1
	log("walk: the route")
	keys = []
	if actions is None:
		position = START
		for d in ROUTE:
			dx, dy, vk = STEP[d]
			expected = (position[0] + dx, position[1] + dy, position[2])
			expected = FLOOR_CHANGES.get(expected, expected)
			for attempt in range(12):
				if not key_sent(driver, proxy, vk, 1.0):
					log("walk: %s from %s: the client sent nothing, again" % (d, position))
					time.sleep(1.0)
					continue
				keys.append(d)
				deadline = time.time() + 3
				now = position
				while time.time() < deadline:
					now = player_position(driver)
					if now != position:
						time.sleep(0.3)  # a floor change: the position settles
						now = player_position(driver)
						break
					time.sleep(0.05)
				if now == expected:
					break
				if now != position:
					raise RunError("step %s from %s went to %s, expected %s" % (d, position, now, expected))
				log("walk: %s from %s blocked, again" % (d, position))
				time.sleep(1.5)
			else:
				raise RunError("cannot step %s from %s" % (d, position))
			position = expected
			time.sleep(0.8)
	else:
		for d in actions:
			for attempt in range(20):
				if key_sent(driver, proxy, STEP[d][2], 1.0):
					break
				log("walk: %s: the client sent nothing, again" % d)
			else:
				raise RunError("the client sends no step %s" % d)
			keys.append(d)
			wait("step")
			time.sleep(0.4)
	position = player_position(driver)
	log("walk: the player is at %s after %d keys" % (position, len(keys)))
	if position != END:
		raise RunError("the walk ended at %s, not %s" % (position, END))
	log("walk: save the default channel")
	tab = wait_node(driver, "TTab")
	click_node(driver, tab, right=True)
	driver.click(*menu_entry(driver, "Save Window"))
	time.sleep(1)
	sc.logout(driver)
	wait("logout")
	time.sleep(1)
	return keys


def record_walk(out):
	"""The live session with Tibia.exe through the recording proxy: <out>\\walk-live.jsonl."""
	path = os.path.join(out, "walk-live.jsonl")
	if ct.server_running():
		raise RunError("the test server is running: stop it first (python -I tools/testserver.py stop)")
	players = ct.snapshot_players()
	before = prepare("orig")
	driver = None
	try:
		ct.restore_players(players)
		ct.start_server()
		proxy = px.Proxy(path, px.load_private_key(ct.PEM), log=log)
		with proxy:
			driver = start("orig")
			keys = walk_session(driver, lambda name: time.sleep(ct.LIVE_SETTLE), proxy)
			close(driver)
		if proxy.errors:
			raise RunError("proxy errors: %s" % proxy.errors)
	finally:
		if driver is not None:
			driver.kill()
		try:
			ct.server_script("stop")
		finally:
			ct.restore_players(players)
			log("player files of the test characters restored")
	collect("orig", before, os.path.join(out, "walk-live", "orig"))
	if walk_keys(px.load_events(path)) != keys:
		raise RunError("the recording's steps are not the keys the session pressed")
	return path


def walk_keys(events):
	"""The directions of the steps (0x65-0x68) the client sent in a recording: the keys a replay
	presses."""
	directions = {"65": "N", "66": "E", "67": "S", "68": "W"}
	return [directions[e["plain"][:2]] for e in events
		if e["kind"] == "game" and e["dir"] == "c2s" and e["plain"][:2] in directions]


def replay(run, source_events, out_prefix, session, seed=None):
	"""Runs session(driver, wait, proxy) against a replay of source_events. Returns (collected files,
	the replay's events, proxy problems)."""
	before = prepare(run, seed=seed)
	os.makedirs(os.path.dirname(out_prefix), exist_ok=True)
	proxy = px.Proxy(out_prefix + ".jsonl", px.load_private_key(ct.PEM), replay=source_events, log=log)
	driver = None
	extra = {}
	try:
		with proxy:
			driver = start(run)
			extra = session(driver, lambda name: proxy.wait_idle(0.5, 60), proxy) or {}
	finally:
		if driver is not None:
			driver.kill()
	result = collect(run, before, out_prefix)
	result.update(extra)
	result["events"] = proxy.events
	result["problems"] = proxy.mismatches + proxy.errors
	return result


def run_walk(run, out, recording, keys):
	def session(driver, wait, proxy):
		walk_session(driver, wait, proxy, keys)
		close(driver)

	return replay(run, recording, os.path.join(out, "walk", ct.client_of(run)), session)


# --- crafted replays ---------------------------------------------------------------------

def connection(events, kind, ordinal=1):
	return [e for e in events if e["kind"] == kind and e["conn_ordinal"] == ordinal]


def s2c(kind, ordinal, t, payload):
	return {"t": round(t, 4), "conn": 0, "kind": kind, "conn_ordinal": ordinal, "dir": "s2c", "plain": payload.hex()}


def c2s_expected(kind, ordinal, t, payload):
	"""A client packet the crafted replay waits for (it goes on whatever comes; a different one
	is a proxy mismatch, reported)."""
	return {"t": round(t, 4), "conn": 0, "kind": kind, "conn_ordinal": ordinal, "dir": "c2s", "plain": payload.hex()}


def close_event(kind, ordinal, t):
	return {"t": round(t, 4), "conn": 0, "kind": kind, "conn_ordinal": ordinal, "dir": "close", "side": "server"}


def game_start(events):
	"""The recording's login connection and its game connection up to the fight modes (0xa0) the
	client sends on entering the game and the server packets of the 3 s after it."""
	login = [e for e in connection(events, "login") if e["dir"] != "close"]
	game = connection(events, "game")
	out = []
	seen = None
	for e in game:
		if seen is not None and (e["dir"] != "s2c" or e["t"] > seen + 3):
			break
		if e["dir"] == "close":
			break
		out.append(e)
		if e["dir"] == "c2s" and e["plain"].startswith("a0"):
			seen = e["t"]
	if seen is None:
		raise RunError("the recording has no fight modes packet")
	return login + [close_event("login", 1, login[-1]["t"] + 0.1)], out


def craft_error(events):
	"""After the game login: a server packet of the unknown type 0xff."""
	login, game = game_start(events)
	return login + game + [s2c("game", 1, game[-1]["t"] + 1.0, b"\xff")]


def craft_error_login(events):
	"""The login server answers with the unknown type 0x63."""
	first = connection(events, "login")[0]
	return [first, s2c("login", 1, first["t"] + 0.5, b"\x63")]


REPORT_COMMENT = "patella comment"


def debug_reports(error_txt, comment):
	"""The 0xe8 packets (src/Communication.cpp SendDebugReport) the client sends for an error.txt
	(Player.cpp ReadErrorFile: after a "+" line the title, the date, then the text lines up to a
	"-" line, joined with "\\n") and the comment typed into the error report dialog, which the
	client sends as "Comment: <text>" (the last report gets it)."""
	reports = []
	state = 0
	for line in error_txt.decode("latin-1").replace("\r\n", "\n").split("\n"):
		if not line:
			continue
		if line.startswith("-"):
			state = 0
		elif line.startswith("+"):
			state = 1
		elif state == 1:
			reports.append([line, "", []])
			state = 2
		elif state == 2:
			reports[-1][1] = line
			state = 3
		elif state == 3:
			reports[-1][2].append(line)
	out = []

	def string(text):
		data = text.encode("latin-1")
		return struct.pack("<H", len(data)) + data

	for i, (title, date, text) in enumerate(reports):
		note = "Comment: " + comment if i == len(reports) - 1 else ""
		out.append(b"\xe8" + string(title) + string(date) + string("\n".join(text)) + string(note))
	return out


def craft_report(events, error_txt):
	"""The game login; then the client's error report (0xe8, the packet computed from error.txt)
	and logout (0x14) are awaited and the server closes the connection."""
	login, game = game_start(events)
	t = game[-1]["t"]
	expected = [c2s_expected("game", 1, t + 1 + 0.1 * i, packet) for i, packet in
		enumerate(debug_reports(error_txt, REPORT_COMMENT))]
	return login + game + expected + [c2s_expected("game", 1, t + 2, b"\x14"), close_event("game", 1, t + 2.5)]


def enter_login(driver):
	"""Start screen to the login: Enter Game, account, Tab, password, Return (sc.login's start)."""
	_, _, width, height = lc.client_offset(driver.hwnd)
	driver.click(113, height - 220)
	time.sleep(1.5)
	driver.type(sc.ACCOUNT)
	driver.char(9)
	driver.type(sc.PASSWORD)
	time.sleep(0.3)
	driver.char(13)


EARLIER_REPORT = (
	"\r\n+---------------------------------------------------------------\r\n"
	"Debug Assertion 7.72 Map.cpp 1199\r\n"
	"Sun Nov 01 12:00:00 2026\r\n"
	"Windows Version: 6.2 build 9200 on 2\r\n"
	"Graphic Engine: 2\r\n"
	"Last Packet Types: 100 106 109 000 000 000 000 000 000 000\r\n"
	"Map.cpp 1199: assertion failed (bx = 1000), reason:\r\n"
	"In(bx,0,999)\r\n"
	"----------------------------------------------------------------").encode("ascii")


def run_error(run, out, events, name="error"):
	"""name "error": the crash in the game, no error.txt before; "error-login": the crash at the
	login, with an earlier report in error.txt. That report opens the error report dialog at the
	start (Player.cpp InitInstance, message 0x2a): Send keeps the file (the client would send the
	report and delete the file only on entering the game), so the new report is appended."""
	crafted = craft_error_login(events) if name == "error-login" else craft_error(events)
	seed = {"error.txt": EARLIER_REPORT} if name == "error-login" else None

	def session(driver, wait, proxy):
		if name == "error-login":
			wait_node(driver, "TErrorReportDialog")
			driver.type("first comment")
			driver.char(13)
			wait_gone(driver, "TErrorReportDialog")
			enter_login(driver)
		else:
			game_login(driver, wait, proxy)
		caption, text, code = close_error_box(driver)
		return {"box": [caption, text], "exit": code}

	return replay(run, crafted, os.path.join(out, name, ct.client_of(run)), session, seed=seed)


def run_report(run, out, events, error_txt):
	def session(driver, wait, proxy):
		wait_node(driver, "TErrorReportDialog")
		driver.type(REPORT_COMMENT)
		driver.char(13)
		wait_gone(driver, "TErrorReportDialog")
		game_login(driver, wait, proxy)
		deadline = time.time() + 20
		while os.path.isfile(os.path.join(folder(run), "error.txt")) and time.time() < deadline:
			time.sleep(0.2)
		time.sleep(1)
		sc.logout(driver)
		time.sleep(1)
		close(driver)
		return {}

	return replay(run, craft_report(events, error_txt), os.path.join(out, "report", ct.client_of(run)), session,
		seed={"error.txt": error_txt})


# --- scenarios: patch, patch-exe ---------------------------------------------------------

def patch_command(operation, count):
	"""A command of a difference patch (src/Script.cpp ApplyPatch): the operation in the top two
	bits, a count below 61 in the low six; 61, 62, 63 announce a byte, word or quad count."""
	if count < 61:
		return bytes([operation | count])
	if count < 0x100:
		return bytes([operation | 61, count])
	if count < 0x10000:
		return bytes([operation | 62]) + struct.pack("<H", count)
	return bytes([operation | 63]) + struct.pack("<I", count)


COPY, SKIP, INSERT, REPLACE = 0x00, 0x40, 0x80, 0xc0


def read_root(name):
	with open(os.path.join(ROOT, name), "rb") as f:
		return f.read()


def new_signature(data, change):
	return struct.pack("<I", struct.unpack_from("<I", data, 0)[0] ^ change)


def dat_patch(data):
	"""Every operation and count form; the result is the file with a new signature."""
	patch = patch_command(REPLACE, 4) + new_signature(data, 0x5a5a0000)
	patch += patch_command(COPY, 10)                       # 4..13
	patch += patch_command(COPY, 200)                      # 14..213, byte count
	patch += patch_command(SKIP, 5) + patch_command(INSERT, 5) + data[214:219]  # the same bytes again
	patch += patch_command(COPY, 3000)                     # 219..3218, word count
	patch += patch_command(REPLACE, 8) + data[3219:3227]
	patch += patch_command(COPY, len(data) - 3227)         # the rest, quad count
	return patch, new_signature(data, 0x5a5a0000) + data[4:]


def spr_patch(data):
	patch = patch_command(REPLACE, 4) + new_signature(data, 0x00a5a500) + patch_command(COPY, len(data) - 4)
	return patch, new_signature(data, 0x00a5a500) + data[4:]


PATCH_EXE = b"MZ" + b"Patella test data for Patch.exe, not a program\r\n" * 20


def exe_patch(data):
	"""Tibia.exe: eight bytes of the DOS stub replaced."""
	patch = patch_command(COPY, 0x40) + patch_command(REPLACE, 8) + b"PATELLA!" + patch_command(COPY, len(data) - 0x48)
	return patch, data[:0x40] + b"PATELLA!" + data[0x48:]


def login_packet(template, dat, spr, pic):
	"""The client's first login packet (as recorded) with the given file signatures."""
	plain = bytearray(bytes.fromhex(template["plain"]))
	plain[5:17] = dat[:4] + spr[:4] + pic[:4]
	event = dict(template)
	event["plain"] = bytes(plain).hex()
	return event


def transfer(ordinal, t, whole, number, data, block=16000):
	"""The login server's answer: 0x1e (difference patch) or 0x1f (whole file) with the file number
	and size, then 0x20 blocks; then the server closes the connection."""
	events = [s2c("login", ordinal, t, bytes([0x1f if whole else 0x1e, number]) + struct.pack("<I", len(data)))]
	for start_ in range(0, len(data), block):
		piece = data[start_:start_ + block]
		t += 0.05
		events.append(s2c("login", ordinal, t, b"\x20" + struct.pack("<H", len(piece)) + piece))
	events.append(close_event("login", ordinal, t + 0.5))
	return events


def craft_patch(events):
	"""Four login connections: Tibia.dat (difference patch), Tibia.spr (difference patch), Tibia.pic
	(whole file), Patch.exe (whole file). After the first three the client reconnects with the new
	signatures. Returns (events, the expected files)."""
	template = connection(events, "login")[0]
	dat, spr, pic = (read_root(name) for name in DATA_FILES)
	dat_data, dat_new = dat_patch(dat)
	spr_data, spr_new = spr_patch(spr)
	pic_new = new_signature(pic, 0x0000a55a) + pic[4:]
	out = []
	signatures = [dat, spr, pic]
	for ordinal, (whole, number, data, index, new) in enumerate((
			(False, 1, dat_data, 0, dat_new), (False, 2, spr_data, 1, spr_new), (True, 3, pic_new, 2, pic_new),
			(True, 4, PATCH_EXE, None, None)), 1):
		t = ordinal * 100.0
		first = login_packet(template, *signatures)
		first["conn_ordinal"] = ordinal
		first["t"] = t
		out.append(first)
		out += transfer(ordinal, t + 0.3, whole, number, data)
		if index is not None:
			signatures[index] = new
	expected = {"Tibia.dat": dat_new, "Tibia.spr": spr_new, "Tibia.pic": pic_new, "Patch.exe": PATCH_EXE}
	return out, expected


def craft_patch_exe(events, exe):
	template = connection(events, "login")[0]
	data, expected = exe_patch(exe)
	first = dict(template)
	return [first] + transfer(1, first["t"] + 0.3, False, 0, data), {"Tibia.bak": expected}


def logins_done(proxy, count, timeout):
	deadline = time.time() + timeout
	while time.time() < deadline:
		closed = [e for e in proxy.events if e["kind"] == "login" and e["dir"] == "close"]
		if len(closed) >= count:
			return True
		time.sleep(0.2)
	return False


def run_patch(run, out, events, name="patch"):
	if name == "patch":
		crafted, expected = craft_patch(events)
		connections = 4
	else:
		with open(os.path.join(folder("orig"), "Tibia.exe"), "rb") as f:
			crafted, expected = craft_patch_exe(events, f.read())
		connections = 1
	def session(driver, wait, proxy):
		enter_login(driver)
		if not logins_done(proxy, connections, 180):
			raise RunError("the client did not finish %d login connections" % connections)
		time.sleep(3)
		if name == "patch":
			close(driver)
			return {"exit": 0}
		# Tibia.exe: the message "Updating" ("Program will now shut down and run an external patching
		# program..."); its OK closes the client (message 0x48), which then runs patch.exe (not there)
		wait_node(driver, "TDialog")
		driver.char(13)
		try:
			driver.proc.wait(30)
		except subprocess.TimeoutExpired:
			raise RunError("the client did not close after the Tibia.exe patch")
		code = driver.proc.returncode
		log("the client exited with code %d" % code)
		if driver.mem is not None:
			driver.mem.close()
			driver.mem = None
		return {"exit": code}

	result = replay(run, crafted, os.path.join(out, name, ct.client_of(run)), session)
	result["expected"] = expected
	return result


# --- comparison --------------------------------------------------------------------------

# ctime's text as MSVC 7.1's CRT writes it: the day with two digits (src/compat/crt71.h)
CTIME = rb"[A-Z][a-z]{2} [A-Z][a-z]{2} \d\d \d\d:\d\d:\d\d \d{4}"


def mask(name, data):
	"""The bytes a file is compared by: times masked (error.txt: the ctime line of each report;
	<channel>.txt: "Channel saved at <ctime>")."""
	if name.lower() == "error.txt":
		return re.sub(rb"(?m)^" + CTIME + rb"(?=\r?$)", b"<time>", data)
	if name.lower().endswith(".txt"):
		return re.sub(rb"Channel saved at " + CTIME, b"Channel saved at <time>", data)
	return data


def masked_spans(name, data):
	"""The masked parts themselves (the evidence that they change from run to run)."""
	if name.lower() == "error.txt":
		return [m.group(0).decode() for m in re.finditer(rb"(?m)^" + CTIME + rb"(?=\r?$)", data)]
	if name.lower().endswith(".txt"):
		return [m.group(0).decode() for m in re.finditer(rb"Channel saved at " + CTIME, data)]
	return []


def client_packets(events):
	"""The client's packets of a replay, per connection, as compared (px.masked)."""
	return [(e["kind"], e["conn_ordinal"], px.masked(e)) for e in events if e["dir"] == "c2s"]


def compare_files(say, name, results, repeat=None):
	"""Both clients' written and removed files; masked files of two runs of Tibia.exe (repeat)."""
	failed = False
	o, p = results["orig"], results["patella"]
	if sorted(o["written"]) != sorted(p["written"]) or o["removed"] != p["removed"]:
		say("    written: %s / %s, removed: %s / %s: DIFFERENT" % (sorted(o["written"]), sorted(p["written"]),
			o["removed"], p["removed"]))
		failed = True
	for file in sorted(set(o["written"]) & set(p["written"])):
		x, y = o["written"][file], p["written"][file]
		same = x == y
		masked_same = mask(file, x) == mask(file, y)
		line = "    %-14s %8d bytes  %s" % (file, len(x), "identical" if same else
			"identical after masking" if masked_same else "DIFFERENT")
		say(line)
		if not masked_same:
			failed = True
			for i, (u, v) in enumerate(zip(mask(file, x).split(b"\n"), mask(file, y).split(b"\n"))):
				if u != v:
					say("      line %d: %r / %r" % (i + 1, u[:150], v[:150]))
					break
			else:
				if len(x) != len(y):
					say("      sizes %d / %d" % (len(x), len(y)))
				else:
					first = next(i for i in range(len(x)) if x[i] != y[i])
					count = sum(1 for i in range(len(x)) if x[i] != y[i])
					say("      %d bytes differ, the first at offset 0x%x" % (count, first))
		if not same and masked_same:
			spans = masked_spans(file, x), masked_spans(file, y)
			say("      masked: Tibia.exe %s, Patella.exe %s" % spans)
	removed = o["removed"]
	if removed:
		say("    removed by both: %s" % ", ".join(removed))
	if repeat is not None:
		for file in sorted(set(o["written"]) & set(repeat["written"])):
			x, y = o["written"][file], repeat["written"][file]
			if x == y:
				continue
			if mask(file, x) == mask(file, y):
				say("    two runs of Tibia.exe: %s differs only where masked: %s / %s" % (file, masked_spans(file, x),
					masked_spans(file, y)))
			else:
				say("    two runs of Tibia.exe: %s differs outside the masks: DIFFERENT" % file)
				failed = True
	return failed


def compare_packets(say, results):
	failed = False
	a = client_packets(results["orig"]["events"])
	b = client_packets(results["patella"]["events"])
	differences = ct.diff_lists(a, b, "Tibia.exe", "Patella.exe")
	say("    client packets: %d and %d, %s" % (len(a), len(b), "identical" if not differences else "DIFFERENT"))
	for line in differences:
		say("  " + line)
	failed |= bool(differences)
	for client in ("orig", "patella"):
		problems = results[client]["problems"]
		if problems:
			say("    proxy, %s: %d against the replayed script: %s" % (CLIENTS[client]["exe"], len(problems),
				"; ".join(x[:160] for x in problems[:4])))
	return failed


# --- main ----------------------------------------------------------------------------------

SCENARIOS = ["options", "walk", "error", "error-login", "report", "patch", "patch-exe"]
REPEATED = {"walk", "error", "error-login"}  # the scenarios with masked files: Tibia.exe runs twice


def main():
	ctypes.windll.user32.SetProcessDPIAware()
	parser = argparse.ArgumentParser(description="Compare the files Patella.exe and Tibia.exe write.")
	parser.add_argument("--out", default=os.path.join(FILES, "result"))
	parser.add_argument("--only", default=",".join(SCENARIOS), help="scenarios, comma-separated: " + ", ".join(SCENARIOS))
	parser.add_argument("--skip-live", action="store_true",
		help="reuse --out's walk-live.jsonl (no server needed)")
	parser.add_argument("--no-repeat", action="store_true", help="run Tibia.exe once only (no evidence for the masks)")
	parser.add_argument("--patella", metavar="EXE", help="the Patella.exe to test, x86 or x64, with its PDB next to "
		"it (default build/oracle-x86/Release/Patella.exe; as tools/compare_traffic.py's --patella)")
	args = parser.parse_args()
	if args.patella:
		CLIENTS["patella"]["source"] = CLIENTS["patella"]["pdb"] = os.path.abspath(args.patella)
	out = os.path.abspath(args.out)
	os.makedirs(out, exist_ok=True)
	only = [name.strip() for name in args.only.split(",") if name.strip()]
	for name in only:
		if name not in SCENARIOS:
			raise RunError("unknown scenario %s" % name)
	ct.set_folder(FILES)
	os.makedirs(FILES, exist_ok=True)
	ct.setup(extra=[REPEAT_RUN])
	report = []
	failed = False

	def say(text=""):
		print(text, flush=True)
		report.append(text)

	recording = os.path.join(out, "walk-live.jsonl")
	if any(name != "options" for name in only):
		if not (args.skip_live and os.path.isfile(recording)):
			log("=== live recording (Tibia.exe against the test server)")
			record_walk(out)
		events = px.load_events(recording)
		keys = walk_keys(events)
	results = {}
	errors = {}
	for name in only:
		runs = ["orig", "patella"] + ([REPEAT_RUN] if name in REPEATED and not args.no_repeat else [])
		results[name] = {}
		for run in runs:
			log("=== %s: %s" % (name, run))
			try:
				target = os.path.join(out, "repeat") if run == REPEAT_RUN else out  # <target>\<scenario>\<client>
				if name == "options":
					result = run_options(run, target)
				elif name == "walk":
					result = run_walk(run, target, events, keys)
				elif name in ("error", "error-login"):
					result = run_error(run, target, events, name)
				elif name == "report":
					path = os.path.join(out, "error", "orig", "error.txt")
					if not os.path.isfile(path):
						raise RunError("report needs Tibia.exe's error.txt of the scenario error (%s)" % path)
					with open(path, "rb") as f:
						result = run_report(run, target, events, f.read())
				else:
					result = run_patch(run, target, events, name)
				results[name][run] = result
			except Exception as e:  # noqa: BLE001 - one failed run must not stop the others
				traceback.print_exc()
				errors.setdefault(name, []).append("%s: %s" % (run, e))
				for leftover in ("orig", "patella", REPEAT_RUN):
					kill_client(leftover)

	say("Written files comparison, %s" % time.strftime("%Y-%m-%d %H:%M"))
	say("results in %s" % out)
	for name in only:
		say()
		say("%s" % name)
		if name in errors:
			for text in errors[name]:
				say("    FAILED RUN %s" % text)
			failed = True
			continue
		r = results[name]
		failed |= compare_files(say, name, r, r.get(REPEAT_RUN))
		for key in ("exit", "box"):
			if key in r["orig"] or key in r["patella"]:
				same = r["orig"].get(key) == r["patella"].get(key)
				say("    %s: %r / %r %s" % (key, r["orig"].get(key), r["patella"].get(key), "identical" if same else "DIFFERENT"))
				failed |= not same
		if "expected" in r["orig"]:
			for client in ("orig", "patella"):
				for file, data in r[client]["expected"].items():
					got = r[client]["written"].get(file)
					ok = got == data
					say("    %s %s: %s" % (CLIENTS[client]["exe"], file, "as expected (%d bytes)" % len(data) if ok else
						"NOT as expected (%s)" % ("missing" if got is None else "%d bytes" % len(got))))
					failed |= not ok
		if "events" in r["orig"]:
			failed |= compare_packets(say, r)
	say()
	say("RESULT: %s" % ("differences found" if failed else "no differences"))
	with open(os.path.join(out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(report) + "\n")
	return 1 if failed else 0


def kill_client(run):
	"""Ends a client of ours left running after a failed run: only the exe in its run folder."""
	path = os.path.join(folder(run), client_exe(run)).lower()
	result = subprocess.run(["powershell", "-NoProfile", "-Command",
		"Get-Process | Where-Object { $_.Path -and $_.Path.ToLower() -eq '%s' } | Stop-Process -Force" % path.replace("'", "''")],
		capture_output=True, text=True)
	return result.returncode


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (RunError, sc.SessionError, px.ProtocolError) as e:
		traceback.print_exc()
		print("compare_files.py: %s" % e, file=sys.stderr)
		sys.exit(1)
