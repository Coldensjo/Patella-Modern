r"""Compare the files Patella.exe and the original Tibia.exe write (issue #21).

Usage (from the repository or a worktree; python -I tools/build.py windows-x64 first; the test server of
issue #18 must be stopped, the script starts and stops it for the one live recording):
  python -I tools/compare_files.py [--out build\files\result] [--only a,b] [--skip-live] [--repeat]
  python -I tools/compare_files.py record|compare [tools/reference.py's options]   (issue #23: the
      reference recordings of this tool's sessions; notes/testmode.md, "Reference recordings")
                                   [--walk ladder|stairs] [--patella EXE]
  python -I tools/compare_files.py --check-route [--walk NAME] [--server-dir DIR]
  python -I tools/compare_files.py --self-test   (issue #68: the window placement, no client, no server)

Both clients play the same sessions in run folders with copies of the data files
(build\files\orig, build\files\patella, build\files\orig-repeat, made like tools/compare_traffic.py's,
emptied before every run); the files each client creates, changes or deletes in its folder are
collected and compared byte for byte.
Scenarios (--only takes their names):

  options  No server. The start window's Options: every check box of the general, console and
           network options, the next fullscreen resolution, the advanced graphics options (three
           check boxes, the frame rate limit, the OpenGL engine), four hotkeys (two sent
           automatically). Tibia.cfg after the options dialog closes and after the exit (the
           x64 client's Patella.ini, issue #45).
  walk     A live session (Tibia.exe through tools/proxy.py against the test server) is
           recorded once; that run is Tibia.exe's, and Patella.exe replays it (with --skip-live
           Tibia.exe replays it too): login, automap marks set, edited and deleted, the automap
           zoomed, the walk (WALKS: 19 actions from a ladder in Rookgaard over floors 7, 6, 8 into
           a second automap block), the default channel saved, logout. The *.map files,
           Default.txt and Tibia.cfg. --check-route checks the walks against the server's map.
  error    The walk recording up to the game login and the first packets, then a server packet
           with the unknown type 0xff: HandleException shows the critical error box, which is
           closed (exit code 1). Tibia.cfg. Tibia.exe also writes error.txt, and its box names
           that file; Patella.exe does neither (issue #74, the deliberate differences of
           tools/deliberate.py, which the report lists). The sessions error-login and report
           (the error report dialog, packet 0xe8) went with the error report in issue #74.

The sessions patch and patch-exe (crafted login server answers that patched Tibia.dat, Tibia.spr,
Tibia.pic, Patch.exe and Tibia.exe) went with the built-in patcher in issue #75 (decision D9): the
fork's clients end the login at a patch packet (tools/deliberate.py), so they write none of those
files; the golden corpus's Communication test checks that (TestUpdateRequests).

The window's place is an input of every session (Patella-Modern issue #68): before each WM_CLOSE, the exit
that writes the last Tibia.cfg (the x64 client's Patella.ini), the client's window is put back where the run's
Tibia.cfg put it ((10,10),
800x600, not maximised) if Windows moved it, and the report lists every window it moved; WindowedMode stays
compared in full. The live recording's server start waits for the ports and the loaded world
(tools/testserver.py start, issue #68). `--self-test` checks the placement on a window of its own.

--patella: the x64 Patella.exe (default build\windows-x64\Release\Patella.exe; Patella-Modern issue #92): its test
build PatellaTest.exe runs in its place, read through its control channel, never through its memory
(compare_traffic.py's use_patella; the 32-bit oracle's Patella.exe left the fork with issue #94).

Masked, after two runs of Tibia.exe showed that they differ from run to run (--repeat runs
Tibia.exe a second time for walk to show it again): the time in Default.txt ("Channel saved
at"). Everything else is compared byte for byte, but the options (Patella-Modern issue #45): the x64
client writes them into Patella.ini (at its start, the import of the run's Tibia.cfg, and where Tibia.exe
writes Tibia.cfg) and never writes Tibia.cfg, so Tibia.exe's Tibia.cfg (the one it wrote, else the run's)
is compared with the x64 client's Patella.ini by value (tools/settings_file.py; tools/deliberate.py), and so
are the options scenario's copies after the options dialog. Report: <out>\report.txt; exit code 0 when
nothing differs.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import client_state as cs  # noqa: E402
import desktop  # noqa: E402
import sandbox  # noqa: E402
import compare_traffic as ct  # noqa: E402
import deliberate  # noqa: E402
import proxy as px  # noqa: E402
import session_client as sc  # noqa: E402
import settings_file  # noqa: E402
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


# Patella-Modern issues #31, #45: the x64 client's settings file. Its options are compared by value with
# Tibia.exe's Tibia.cfg (settings_file.compare_options); its own keys (the options scenario's CPU button writes
# Device=cpu) are not compared.
PATELLA_INI = deliberate.PATELLA_INI
# The x64 client's CPU driver (cmake/Vulkan.cmake), copied next to it in its run folder, so that the
# advanced options' CPU button, in the OpenGL button's place, is enabled and switches as Tibia.exe's
# OpenGL button does (both write Engine = 1).
CPU_DRIVER_FILES = ("vulkan_lvp.dll", "lvp_icd.x86_64.json")


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
	in the root, the patched client), then (in Patella's folder) the x64 client's CPU driver:
	CPU_DRIVER_FILES, Tibia.cfg (compare_traffic's), the files in seed ({name: bytes}), nothing else.
	Until issue #75 Patella's folder also had a copy of the patched Tibia.exe, the input of the Tibia.exe
	patch (patch-exe)."""
	path = folder(run)
	ct.clean_run_dir(run)
	if ct.client_of(run) == "patella":
		source_dir = os.path.dirname(CLIENTS["patella"]["source"])
		for name in CPU_DRIVER_FILES:
			if os.path.isfile(os.path.join(source_dir, name)):
				shutil.copyfile(os.path.join(source_dir, name), os.path.join(path, name))
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
	driver = sc.Driver(folder(run), info["exe"], info["pdb"], test=info.get("test", False))
	driver.start(arguments)
	return driver


class WINDOWPLACEMENT(ctypes.Structure):
	_fields_ = [("length", wt.UINT), ("flags", wt.UINT), ("showCmd", wt.UINT), ("ptMinPosition", wt.POINT),
		("ptMaxPosition", wt.POINT), ("rcNormalPosition", wt.RECT)]


user32.GetWindowPlacement.argtypes = [wt.HWND, ctypes.POINTER(WINDOWPLACEMENT)]
user32.SetWindowPlacement.argtypes = [wt.HWND, ctypes.POINTER(WINDOWPLACEMENT)]
SW_SHOWNORMAL, SW_SHOWNOACTIVATE = 1, 4


def cfg_window(cfg=ct.TIBIA_CFG):
	"""(x, y, width, height) of a Tibia.cfg's WindowedMode line."""
	match = re.search(r"^WindowedMode = \((-?\d+),(-?\d+),(\d+),(\d+),(yes|no)\)", cfg, re.M)
	if match is None:
		raise RunError("no WindowedMode line in the run's Tibia.cfg")
	return tuple(int(value) for value in match.groups()[:4])


def window_placement(hwnd):
	"""(x, y, width, height, showCmd) of a window's normal (restored) place: what the client's OnDestroy
	writes to Tibia.cfg's WindowedMode (GetWindowPlacement's rcNormalPosition, src/MainFrm.cpp)."""
	placement = WINDOWPLACEMENT()
	placement.length = ctypes.sizeof(WINDOWPLACEMENT)
	if not user32.GetWindowPlacement(hwnd, ctypes.byref(placement)):
		raise RunError("GetWindowPlacement failed (error %d)" % ctypes.get_last_error())
	r = placement.rcNormalPosition
	return r.left, r.top, r.right - r.left, r.bottom - r.top, placement.showCmd


# Patella-Modern issue #68: where the window was found before the WM_CLOSE that writes the last Tibia.cfg,
# when it was not where the run's Tibia.cfg put it: [(scenario, run, found, placed)], listed in the report.
PLACEMENTS = []
SCENARIO = [""]  # the scenario running, for PLACEMENTS


def place_window(hwnd, target, run="", settle=0.3):
	"""Puts the window at target (x, y, width, height; normal, not maximised) when it is elsewhere (issue #68:
	Windows now and then moves a client's window on its own, (10,10) became (-4,6) in Tibia.exe's Tibia.cfg).
	The window's place is an input of the session, like the Tibia.cfg it starts with: both clients get the
	same one, and WindowedMode stays compared in full. Returns where it was, or None when it was in place."""
	found = window_placement(hwnd)
	if found[:4] == tuple(target) and found[4] == SW_SHOWNORMAL:
		return None
	placement = WINDOWPLACEMENT()
	placement.length = ctypes.sizeof(WINDOWPLACEMENT)
	placement.showCmd = SW_SHOWNOACTIVATE
	x, y, width, height = target
	placement.rcNormalPosition = wt.RECT(x, y, x + width, y + height)
	if not user32.SetWindowPlacement(hwnd, ctypes.byref(placement)):
		raise RunError("SetWindowPlacement failed (error %d)" % ctypes.get_last_error())
	time.sleep(settle)
	placed = window_placement(hwnd)
	log("%s: the window was at %s (showCmd %d), not at Tibia.cfg's %s: placed there, now %s (showCmd %d)" % (
		run or "window", found[:4], found[4], tuple(target), placed[:4], placed[4]))
	PLACEMENTS.append((SCENARIO[0], run, found, placed))
	return found


def close(driver, expect=0):
	place_window(driver.hwnd, cfg_window(), os.path.basename(driver.run_dir))
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


READ_AGAIN = 2.0  # seconds a failed window tree reading is read again (issue #149)
READ_POLL = 0.05  # seconds between two such readings


def gui(driver, deadline=None):
	"""The client's window tree (client_state's gui(), the test client's probe). Patella-Modern issue #149:
	Tibia.exe's tree is read from outside while the client changes it, and a walk that meets a dialog being built
	or freed (the options dialogs, polled right after the click or Return that opens or closes one since #144's
	waits) follows a stale pointer: ReadProcessMemory fails at 0x9, 0xa, 0xf5f5ff or the text "edit". Such a
	reading is no answer, so it is read again, every READ_POLL s until deadline (default READ_AGAIN s from now);
	the last failure (client_state.ReadError, not a RunError: click_open must not take it for a click that opened
	nothing) is raised after it. Only a failed reading is repeated: what a reading that succeeds shows is used as
	before (wait_node still wants the same place twice)."""
	deadline = time.time() + READ_AGAIN if deadline is None else deadline
	failed = 0
	while True:
		try:
			tree = driver.mem.gui()
		except cs.ReadError as e:
			if time.time() >= deadline:
				raise
			if not failed:
				log("the window tree changed during its reading (%s): read again" % e)
			failed += 1
			time.sleep(READ_POLL)
			continue
		if failed > 1:
			log("the window tree read after %d failed readings" % failed)
		return tree


def nodes(driver, cls, deadline=None):
	return [n for n in gui(driver, deadline) if n["class"] == cls]


def wait_node(driver, cls, timeout=10):
	"""The first window of class cls, once it is there and its place has stopped changing (a dialog
	appears in the tree before it is moved to its place: a click on one of its buttons at the first
	place read can miss). A failed reading is read again within the timeout (gui(), issue #149)."""
	deadline = time.time() + timeout
	while time.time() < deadline:
		found = nodes(driver, cls, deadline)
		if found:
			node = found[0]
			while time.time() < deadline:
				time.sleep(0.2)
				again = [n for n in nodes(driver, cls, deadline) if n["ptr"] == node["ptr"]]
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
	"""Until no window of class cls is open; a failed reading is read again within the timeout (gui(), issue
	#149)."""
	deadline = time.time() + timeout
	while time.time() < deadline:
		if not nodes(driver, cls, deadline):
			return
		time.sleep(0.2)
	raise RunError("the window of class %s is still open after %d s" % (cls, timeout))


def click_node(driver, node, right=False, dx=None):
	x = node["x"] + (node["w"] // 2 if dx is None else dx)
	driver.click(x, node["y"] + node["h"] // 2, right=right)


def click_ptr(driver, ptr, dx=None):
	click_node(driver, driver.node_by_ptr(ptr), dx=dx)


def button(driver, text):
	for n in gui(driver):
		if n["class"] == "TTextButton" and n.get("text") == text:
			return n
	raise RunError("no button %r" % text)


def member(driver, component, path):
	"""The pointer member `path` (Type.Member, client_state.LAYOUT_FIELDS) of the component at
	`component`: from Tibia.exe's memory (its offset from the layout), or from the x64 test client's probe
	(issue #92)."""
	return driver.mem.member(component, path)


def checked(driver, box):
	"""TCheckBox's Checked (+0x30 in the original, after the label pointer)."""
	return driver.mem.checked(box)


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
		if driver.mem.member(body["ptr"], "THotkeyDialog.Selected") != number:
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


def settings_name(run):
	"""The file a run's client keeps its options in: Tibia.exe's Tibia.cfg, the x64 client's Patella.ini (issue
	#45)."""
	return PATELLA_INI if ct.client_of(run) == "patella" else "Tibia.cfg"


def run_options(run, out):
	before = prepare(run)
	driver = start(run)
	target = os.path.join(out, "options", ct.client_of(run))
	os.makedirs(target, exist_ok=True)
	kept = {}
	try:
		def keep(name):
			source = settings_name(run)
			with open(os.path.join(folder(run), source), "rb") as f:
				kept[name] = f.read()
			with open(os.path.join(target, "%s.%s" % (name, source)), "wb") as f:
				f.write(kept[name])

		options_session(driver, keep)
		close(driver)
	finally:
		driver.kill()
	result = collect(run, before, target)
	result["kept"] = kept
	return result


# --- scenario: walk ----------------------------------------------------------------------

# What the walk is for (Patella-Modern issue #16 shortened issue #21's 116 steps from the temple):
# the client writes the automap blocks 125/125 on floors 6, 7 and 8 and 125/126 on floor 7
# (WALK_BLOCKS: 12512506.map, 12512507.map with the marks, 12512508.map, 12512607.map) and no
# other. The client records the fields of its map on the player's floor only (Map.cpp
# UpdateAutomap, from ReadField and the field updates with rz == GetMapLevel()), x-8..x+9 and
# y-6..y+7 around the player, so the player must stand on floors 6 and 8 with y <= 32248 and on
# floor 7 with y >= 32249 (y+7 in block 126). The marks (MARKS: automap pixels from the player,
# one field each at Tibia.cfg's AutomapZoom 2) must lie in block 125/125/7. Each walk is planned
# on the server's map (gameserver\data: the sector files, objects.srv's flags, moveuse.dat's
# rules) and checked against it by check_route (`--check-route`, no client). The test character
# starts where the walk begins: record_walk writes the start into its player file (CurrentPosition)
# and puts the file back afterwards.
#
# Actions: N, E, S, W are arrow keys (steps, 0x65-0x68); U uses the ladder the player stands on
# (Ctrl+right click on its own field; GetTopUseObject takes the FORCEUSE ladder under the player;
# moveuse.dat `Use, IsType (Obj1,1948) -> MoveRel(User,Obj1,[0,1,-1])`; packet 0x82).
# floor_changes: the field a step goes onto -> where the player ends up (moveuse.dat's
# `Collision, IsType (Obj1,T) -> MoveTopRel(Obj1,[dx,dy,dz])`).
WALKS = {
	# 19 actions. A house in the south of the town: up the ladder (32109,32244,7) to floor 6, down
	# its trapdoor (32109,32244,6) back onto the ladder's field, the stairs down (32110,32245,7) to
	# floor 8 and up again (32110,32245,8), out of the house and south to (32107,32249,7).
	"ladder": {
		"start": (32109, 32244, 7),
		"route": "UNESN" + "NNWWWSWSSESSSS",
		"floor_changes": {
			(32109, 32244, 6): (32109, 32244, 7),  # trapdoor (411)
			(32110, 32245, 7): (32110, 32246, 8),  # stairs down (428)
			(32110, 32245, 8): (32110, 32244, 7),  # stairs up (1947)
		},
		"end": (32107, 32249, 7),
	},
	# 44 steps, no use: the ramp west of the town (32093,32235,7) up to floor 6 and its stairs down
	# (32093,32235,6), east to the house of "ladder" and its stairs down and up, then south. The
	# fallback if the ladder ever does not work (--walk stairs).
	"stairs": {
		"start": (32095, 32235, 7),
		"route": "WWE" + "E" * 11 + "SEEEEESSSSSSSSSNNNWWWSWSSESSSS",
		"floor_changes": {
			(32093, 32235, 7): (32092, 32235, 6),  # ramp up (1952)
			(32093, 32235, 6): (32094, 32235, 7),  # stairs down (467)
			(32110, 32245, 7): (32110, 32246, 8),  # stairs down (428)
			(32110, 32245, 8): (32110, 32244, 7),  # stairs up (1947)
		},
		"end": (32107, 32249, 7),
	},
}
DEFAULT_WALK = "ladder"
WALK_BLOCKS = {(125, 125, 6), (125, 125, 7), (125, 125, 8), (125, 126, 7)}
MARKS = ((20, 0), (-12, 10), (0, -15))  # set (and the first edited), set, set and deleted
STEP = {"N": (0, -1, sc.VK_UP), "E": (1, 0, sc.VK_RIGHT), "S": (0, 1, VK_DOWN), "W": (-1, 0, sc.VK_LEFT)}
LADDER = (0, 1, -1)  # MoveRel(User,Obj1,[0,1,-1]): where the ladder takes the player
# Seconds per action, measured on November 3, 2026 (116 steps: 148 s live, 168 s in a replay)
LIVE_ACTION_S, REPLAY_ACTION_S = 1.28, 1.45


def next_position(walk, position, action):
	"""Where an action takes the player."""
	x, y, z = position
	if action == "U":
		return (x + LADDER[0], y + LADDER[1], z + LADDER[2])
	dx, dy, _ = STEP[action]
	target = (x + dx, y + dy, z)
	return walk["floor_changes"].get(target, target)


def player_position(driver):
	"""The player's creature's position (Creatures.cpp's table, as client_state reads it)."""
	player = driver.mem.player_id()
	for creature in driver.mem.creatures():
		if player != 0 and creature["id"] == player:
			return tuple(creature["pos"])
	return None


MARK_SYMBOLS = 14  # TEditMarkDialog.TypeButtons (Dialogs.h)


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
		if not 0 <= kind < MARK_SYMBOLS:
			raise RunError("mark symbol %d out of range" % kind)
		click_ptr(driver, driver.mem.member_index(body["ptr"], "TEditMarkDialog.TypeButtons", kind))
	driver.char(13)
	wait_gone(driver, "TEditMarkDialog")


def delete_mark(driver, dx, dy):
	driver.click(*automap_point(driver, dx, dy), right=True)
	driver.click(*menu_entry(driver, "Delete Mark"))
	time.sleep(0.5)


def press(driver, action):
	if action == "U":
		driver.click(*driver.field_point(8, 6), right=True, modifier=sc.VK_CONTROL)  # the player's own field
	else:
		driver.key(STEP[action][2])


def action_sent(driver, proxy, action, timeout):
	"""Presses an arrow key (or uses the ladder); True when the client sent a packet within timeout.
	A client sends no step while a creature stands on the field (it knows the field is taken), and
	when that creature moves away depends on the time the key comes, so a press without a packet
	is not part of the session."""
	count = proxy.c2s_count
	press(driver, action)
	deadline = time.time() + timeout
	while time.time() < deadline:
		if proxy.c2s_count != count:
			return True
		time.sleep(0.05)
	return False


def walk_session(driver, wait, proxy, walk, actions=None):
	"""Login, automap marks and zoom, the walk's route, the default channel saved, logout. Live
	(actions None): every action is repeated until the player moved (a creature may stand in the
	way); the actions that made the client send a packet are recorded and returned. Replay: each
	recorded action, repeated until the client sends its packet."""
	game_login(driver, wait, proxy)
	wait("login")
	time.sleep(2)
	position = player_position(driver)
	if position != walk["start"]:
		raise RunError("the player starts at %s, not %s" % (position, walk["start"]))
	log("walk: automap marks")
	set_mark(driver, *MARKS[0], "Patella one", 3)
	set_mark(driver, *MARKS[1], "second mark", 9)
	set_mark(driver, *MARKS[2], "to be deleted", None)
	set_mark(driver, *MARKS[0], " edited", 5, entry="Edit Mark")
	delete_mark(driver, *MARKS[2])
	automap = wait_node(driver, "TAutomapWindow")
	click_ptr(driver, member(driver, automap["ptr"], "TAutomapWindow.ZoomInButton"))  # zoom in: 4
	for _ in range(2):
		click_ptr(driver, member(driver, automap["ptr"], "TAutomapWindow.ZoomOutButton"))  # zoom out: 2, 1
	log("walk: the route, %d actions" % len(walk["route"]))
	keys = []
	if actions is None:
		position = walk["start"]
		for d in walk["route"]:
			expected = next_position(walk, position, d)
			for attempt in range(12):
				if not action_sent(driver, proxy, d, 1.0):
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
					raise RunError("%s from %s went to %s, expected %s" % (d, position, now, expected))
				log("walk: %s from %s blocked, again" % (d, position))
				time.sleep(1.5)
			else:
				raise RunError("cannot do %s from %s" % (d, position))
			position = expected
			time.sleep(0.8)
	else:
		for d in actions:
			for attempt in range(20):
				if action_sent(driver, proxy, d, 1.0):
					break
				log("walk: %s: the client sent nothing, again" % d)
			else:
				raise RunError("the client sends nothing for %s" % d)
			keys.append(d)
			wait("step")
			time.sleep(0.4)
	position = player_position(driver)
	log("walk: the player is at %s after %d actions" % (position, len(keys)))
	if position != walk["end"]:
		raise RunError("the walk ended at %s, not %s" % (position, walk["end"]))
	log("walk: save the default channel")
	tab = wait_node(driver, "TTab")
	click_node(driver, tab, right=True)
	driver.click(*menu_entry(driver, "Save Window"))
	time.sleep(1)
	sc.logout(driver)
	wait("logout")
	time.sleep(1)
	return keys


def set_start(players, position):
	"""Writes position as the test character's CurrentPosition into its player file: the snapshot's
	text with that one value changed (the server logs the character in there). restore_players puts
	the snapshot back."""
	data = players.get(sc.CHARACTER)
	if data is None:
		raise RunError("%s has no player file yet (%s): log the character in once (tools/login_client.py)"
			% (sc.CHARACTER, ct.PLAYER_FILES[sc.CHARACTER]))
	text = data.decode("latin-1")
	new, count = re.subn(r"(?m)^(CurrentPosition\s*=\s*)\[\d+,\d+,\d+\]",
		lambda m: m.group(1) + "[%d,%d,%d]" % position, text)
	if count != 1:
		raise RunError("%s: %d CurrentPosition lines" % (ct.PLAYER_FILES[sc.CHARACTER], count))
	with open(ct.PLAYER_FILES[sc.CHARACTER], "wb") as f:
		f.write(new.encode("latin-1"))
	log("player file of %s: CurrentPosition [%d,%d,%d]" % ((sc.CHARACTER,) + tuple(position)))


def record_walk(out, walk_name):
	"""The live session with Tibia.exe through the recording proxy: <out>\\walk-live.jsonl (and the
	walk's name in walk-live.walk). Returns the run's result, which is Tibia.exe's walk run of the
	comparison (issue #16: the replay of the recording into Tibia.exe gave the same files, so only
	Patella.exe walks again)."""
	walk = WALKS[walk_name]
	path = os.path.join(out, "walk-live.jsonl")
	if ct.server_running():
		raise RunError("the test server is running: stop it first (python -I tools/testserver.py stop)")
	players = ct.snapshot_players()
	before = prepare("orig")
	driver = None
	try:
		ct.restore_players(players)
		set_start(players, walk["start"])
		ct.start_server()
		proxy = px.Proxy(path, px.load_private_key(ct.PEM), log=log)
		with proxy:
			driver = start("orig")
			keys = walk_session(driver, lambda name: time.sleep(ct.LIVE_SETTLE), proxy, walk)
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
	with open(os.path.join(out, "walk-live.walk"), "w", encoding="ascii", newline="\n") as f:
		f.write(walk_name + "\n")
	result = collect("orig", before, os.path.join(out, "walk-live", "orig"))
	if walk_keys(px.load_events(path)) != keys:
		raise RunError("the recording's actions are not the ones the session took")
	result["events"] = proxy.events
	result["problems"] = proxy.mismatches + proxy.errors
	return result


def walk_keys(events):
	"""The actions the client sent in a recording, the ones a replay takes: the steps (0x65-0x68)
	and the ladder's use (0x82, the only use in the session)."""
	actions = {"65": "N", "66": "E", "67": "S", "68": "W", "82": "U"}
	return [actions[e["plain"][:2]] for e in events
		if e["kind"] == "game" and e["dir"] == "c2s" and e["plain"][:2] in actions]


# --- the route against the server's map (--check-route) ------------------------------------

def object_flags(server_dir):
	"""{type: flags} from objects.srv."""
	flags = {}
	current = None
	with open(os.path.join(server_dir, "data", "dat", "objects.srv"), encoding="latin-1") as f:
		for line in f:
			m = re.match(r"\s*TypeID\s*=\s*(\d+)", line)
			if m:
				current = int(m.group(1))
				flags[current] = set()
				continue
			m = re.match(r"\s*Flags\s*=\s*\{(.*)\}", line)
			if m and current is not None:
				flags[current] = {x.strip() for x in m.group(1).split(",") if x.strip()}
	return flags


def move_rules(server_dir):
	"""moveuse.dat: ({type: (dx, dy, dz)} of the plain `Collision, IsType (Obj1,T) ->
	MoveTopRel(Obj1,[dx,dy,dz])` rules, {type: delta} of the plain `Use, IsType (Obj1,T) ->
	MoveRel(User,Obj1,[dx,dy,dz])` rules, the other types a Collision or Separation rule names)."""
	collision, use, other = {}, {}, set()
	with open(os.path.join(server_dir, "data", "dat", "moveuse.dat"), encoding="latin-1") as f:
		for line in f:
			line = line.strip()
			m = re.fullmatch(r"Collision, IsType \(Obj1,(\d+)\) -> MoveTopRel\(Obj1,\[(-?\d+),(-?\d+),(-?\d+)\]\)", line)
			if m:
				collision[int(m.group(1))] = tuple(int(m.group(i)) for i in (2, 3, 4))
				continue
			m = re.fullmatch(r"Use, IsType \(Obj1,(\d+)\) -> MoveRel\(User,Obj1,\[(-?\d+),(-?\d+),(-?\d+)\]\)", line)
			if m:
				use[int(m.group(1))] = tuple(int(m.group(i)) for i in (2, 3, 4))
				continue
			if line.startswith(("Collision", "Separation")):
				other |= {int(t) for t in re.findall(r"IsType \(Obj[12],(\d+)\)", line)}
	return collision, use, other - set(collision)


def content_types(content):
	"""The type numbers at the first level of a sector file's Content={...}."""
	items = []
	depth = 0
	quoted = expect = False
	i = 0
	while i < len(content):
		ch = content[i]
		if quoted:
			if ch == "\\":
				i += 1
			elif ch == '"':
				quoted = False
		elif ch == '"':
			quoted = True
		elif ch == "{":
			depth += 1
			expect = depth == 1
		elif ch == "}":
			depth -= 1
		elif ch == ",":
			expect = depth == 1
		elif ch.isdigit() and expect:
			j = i
			while j < len(content) and content[j].isdigit():
				j += 1
			items.append(int(content[i:j]))
			expect = False
			i = j
			continue
		elif not ch.isspace():
			expect = False
		i += 1
	return items


class ServerMap:
	"""The fields of the server's map (read-only): what a character can stand on, where a floor
	change takes it."""

	def __init__(self, server_dir):
		self.server_dir = server_dir
		self.flags = object_flags(server_dir)
		self.collision, self.use, self.other = move_rules(server_dir)
		with open(os.path.join(server_dir, "data", "dat", "houses.dat"), encoding="latin-1") as f:
			self.houses = {tuple(int(v) for v in p) for p in re.findall(r"\[(\d+),(\d+),(\d+)\]", f.read())}
		self.sectors = {}

	def field(self, p):
		x, y, z = p
		key = (x // 32, y // 32, z)
		if key not in self.sectors:
			fields = {}
			path = os.path.join(self.server_dir, "data", "map", "%04d-%04d-%02d.sec" % key)
			if os.path.isfile(path):
				with open(path, encoding="latin-1") as f:
					for line in f:
						m = re.match(r"(\d+)-(\d+):\s*(.*)", line)
						if m:
							rest = m.group(3)
							k = rest.find("Content=")
							fields[(key[0] * 32 + int(m.group(1)), key[1] * 32 + int(m.group(2)), z)] = (
								content_types(rest[k + len("Content="):]) if k >= 0 else [])
			self.sectors[key] = fields
		return self.sectors[key].get(p)

	def classify(self, p):
		"""("plain", None): a field to stand on; ("change", delta): one that moves the creature on;
		("blocked", why)."""
		items = self.field(p)
		if items is None:
			return "blocked", "no field"
		if p in self.houses:
			return "blocked", "a house"
		if not any("Bank" in self.flags.get(t, ()) for t in items):
			return "blocked", "no ground"
		change = None
		heights = 0
		for t in items:
			f = self.flags.get(t, set())
			if "Unpass" in f:
				return "blocked", "unpassable %d" % t
			if t in self.collision:
				if change is not None:
					return "blocked", "two floor changes"
				change = self.collision[t]
			elif t in self.other or "CollisionEvent" in f or "SeparationEvent" in f:
				return "blocked", "event object %d" % t
			elif "Avoid" in f:
				return "blocked", "avoid %d" % t
			elif "Height" in f:
				heights += 1
		if heights > (0 if change else 1):
			# one object with an elevation and no event (the upper half of a ramp) can be stepped on
			return "blocked", "%d objects with a height" % heights
		return ("change", change) if change else ("plain", None)

	def ladder(self, p):
		"""The delta of the use rule of an object on field p (a ladder), or None."""
		for t in self.field(p) or []:
			if t in self.use:
				return self.use[t]
		return None


def view_blocks(p):
	"""The automap blocks the client's map touches at p: x-8..x+9, y-6..y+7, the player's floor."""
	x, y, z = p
	return {(bx, by, z) for bx in {(x - 8) // 256, (x + 9) // 256} for by in {(y - 6) // 256, (y + 7) // 256}}


def check_route(walk_name, server_dir=ct.SERVER_DIR, say=print):
	"""Walks the route on the server's map: every target field one to stand on or a floor change
	that floor_changes names with the map's delta, every floor_changes entry used, the end, the
	automap blocks (WALK_BLOCKS, no more), the marks in block 125/125/7. Returns the problems."""
	walk = WALKS[walk_name]
	m = ServerMap(server_dir)
	problems = []
	position = walk["start"]
	say("walk %s: start %s, %d actions: %s" % (walk_name, position, len(walk["route"]), walk["route"]))
	kind, detail = m.classify(position)
	if kind != "plain" or position[2] != 7:
		problems.append("the start %s is %s %s" % (position, kind, detail or ""))
	blocks = set(view_blocks(position))
	floors = []
	used = set()
	for i, d in enumerate(walk["route"], 1):
		if d == "U":
			delta = m.ladder(position)
			target = position
			if delta is None:
				problems.append("action %d: U on %s, which has no ladder" % (i, position))
				break
			land = (position[0] + delta[0], position[1] + delta[1], position[2] + delta[2])
			note = "use the ladder"
			if delta != LADDER:
				problems.append("action %d: the ladder's rule moves by %s, the script expects %s" % (i, delta, LADDER))
		else:
			dx, dy, _ = STEP[d]
			target = (position[0] + dx, position[1] + dy, position[2])
			kind, detail = m.classify(target)
			if kind == "blocked":
				problems.append("action %d: %s from %s onto %s: %s" % (i, d, position, target, detail))
				break
			if kind == "change":
				land = (target[0] + detail[0], target[1] + detail[1], target[2] + detail[2])
				note = "floor change %s" % (detail,)
				used.add(target)
				if walk["floor_changes"].get(target) != land:
					problems.append("action %d: %s onto %s takes the player to %s, floor_changes says %s"
						% (i, d, target, land, walk["floor_changes"].get(target)))
			else:
				land = target
				note = ""
				if target in walk["floor_changes"]:
					problems.append("action %d: floor_changes names %s, the map has no floor change there" % (i, target))
		kind, detail = m.classify(land)
		if kind != "plain":
			problems.append("action %d: the player ends up on %s, which is %s %s" % (i, land, kind, detail))
		if land[2] != position[2]:
			floors.append(land[2])
		if next_position(walk, position, d) != land:
			problems.append("action %d: the script expects %s, the map gives %s" % (i, next_position(walk, position, d), land))
		say("  %2d %s %s -> %s %s" % (i, d, target, land, note))
		position = land
		blocks |= view_blocks(position)
	unused = sorted(set(walk["floor_changes"]) - used)
	if unused:
		problems.append("floor_changes not used by the route: %s" % unused)
	if position != walk["end"]:
		problems.append("the route ends at %s, not %s" % (position, walk["end"]))
	marks = [(walk["start"][0] + dx, walk["start"][1] + dy, 7) for dx, dy in MARKS]
	for mark in marks:
		if (mark[0] // 256, mark[1] // 256, 7) != (125, 125, 7):
			problems.append("the mark at %s is outside block 125/125/7" % (mark,))
	names = sorted("%03d%03d%02d.map" % b for b in blocks)
	say("  floors after the start: %s; end %s; automap blocks %s; marks at %s" % (floors, position, names, marks))
	if blocks != WALK_BLOCKS:
		problems.append("the route touches the automap blocks %s, not %s" % (names,
			sorted("%03d%03d%02d.map" % b for b in WALK_BLOCKS)))
	say("  %d actions: about %.0f s live, %.0f s in a replay (%.2f and %.2f s per action)" % (len(walk["route"]),
		len(walk["route"]) * LIVE_ACTION_S, len(walk["route"]) * REPLAY_ACTION_S, LIVE_ACTION_S, REPLAY_ACTION_S))
	for p in problems:
		say("  PROBLEM: " + p)
	say("  %s" % ("ok" if not problems else "%d problems" % len(problems)))
	return problems


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


def run_walk(run, out, recording, keys, walk):
	def session(driver, wait, proxy):
		walk_session(driver, wait, proxy, walk, keys)
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


def enter_login(driver):
	"""Start screen to the login: Enter Game, account, Tab, password (sc.enter_login: the login dialog waited for,
	issue #144), Return."""
	sc.enter_login(driver)
	driver.char(13)


def run_error(run, out, events, name="error"):
	"""The crash in the game (the server packet 0xff after the game login): the critical error box, closed."""

	def session(driver, wait, proxy):
		game_login(driver, wait, proxy)
		caption, text, code = close_error_box(driver)
		return {"box": [caption, text], "exit": code}

	return replay(run, craft_error(events), os.path.join(out, name, ct.client_of(run)), session)


# --- comparison --------------------------------------------------------------------------

# ctime's text as MSVC 7.1's CRT writes it: the day with two digits (src/compat/crt71.h)
CTIME = rb"[A-Z][a-z]{2} [A-Z][a-z]{2} \d\d \d\d:\d\d:\d\d \d{4}"


def mask(name, data):
	"""The bytes a file is compared by: times masked (<channel>.txt: "Channel saved at <ctime>"; the
	ctime line of each report in error.txt too until issue #74, which only Tibia.exe writes now)."""
	if name.lower().endswith(".txt"):
		return re.sub(rb"Channel saved at " + CTIME, b"Channel saved at <time>", data)
	return data


def masked_spans(name, data):
	"""The masked parts themselves (the evidence that they change from run to run)."""
	if name.lower().endswith(".txt"):
		return [m.group(0).decode() for m in re.finditer(rb"Channel saved at " + CTIME, data)]
	return []


def client_packets(events):
	"""The client's packets of a replay, per connection, as compared (px.masked)."""
	return [(e["kind"], e["conn_ordinal"], px.masked(e)) for e in events if e["dir"] == "c2s"]


def compare_files(say, name, results, repeat=None):
	"""Both clients' written and removed files; masked files of two runs of Tibia.exe (repeat)."""
	failed = False
	o, p = dict(results["orig"]), dict(results["patella"])
	# Tibia.exe's files as Patella.exe is expected to write them (tools/deliberate.py: no error.txt, issue #74; its
	# options in Patella.ini, compared by value, issue #45).
	for line in deliberate.expected_lines(o["written"], o["removed"]):
		say("    " + line)
	cfg = o["written"].get("Tibia.cfg")
	source = "Tibia.exe's Tibia.cfg" if cfg is not None else "the run's Tibia.cfg"
	if cfg is None:
		cfg = ct.TIBIA_CFG.replace("\n", "\r\n").encode("latin-1")
	o["written"] = {name: data for name, data in o["written"].items() if name in deliberate.files([name])}
	o["removed"] = deliberate.files(o["removed"])
	p["written"] = dict(p["written"])
	differences, lines = settings_file.compare_options(cfg, p["written"].pop(PATELLA_INI, None), source)
	for when in sorted(set(o.get("kept", {})) | set(p.get("kept", {}))):
		if when not in o.get("kept", {}) or when not in p.get("kept", {}):
			differences.append("%s: kept by one client only" % when)
			continue
		found, texts = settings_file.compare_options(o["kept"][when], p["kept"][when], "Tibia.exe's %s.Tibia.cfg" % when)
		lines += texts
		differences += found
	for line in lines:
		say("    " + line)
	for line in differences:
		say("    DIFFERENT " + line)
	failed |= bool(differences)
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
		first = results["orig"]["written"]  # Tibia.cfg too: two runs of Tibia.exe
		for file in sorted(set(first) & set(repeat["written"])):
			x, y = first[file], repeat["written"][file]
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

SCENARIOS = ["options", "walk", "error"]  # patch and patch-exe went with the patcher (issue #75)
REPEATED = {"walk"}  # the scenarios with masked files: Tibia.exe runs twice with --repeat


def main():
	# The record/compare split (Patella-Modern issue #23): record or compare as the first word runs
	# tools/reference.py with this tool's sessions (notes/testmode.md, "Reference recordings").
	if len(sys.argv) > 1 and sys.argv[1] in ("record", "compare"):
		import reference
		return reference.main([sys.argv[1], "--sessions", ",".join(reference.tool_sessions("compare_files"))] + sys.argv[2:])
	ctypes.windll.user32.SetProcessDPIAware()
	parser = argparse.ArgumentParser(description="Compare the files Patella.exe and Tibia.exe write.")
	parser.add_argument("--out", default=os.path.join(FILES, "result"))
	parser.add_argument("--only", default=",".join(SCENARIOS), help="scenarios, comma-separated: " + ", ".join(SCENARIOS))
	parser.add_argument("--skip-live", action="store_true",
		help="reuse --out's walk-live.jsonl (no server needed; Tibia.exe then replays it for the walk)")
	parser.add_argument("--repeat", action="store_true", help="run Tibia.exe a second time for %s (the evidence "
		"for the masks)" % ", ".join(sorted(REPEATED)))
	parser.add_argument("--no-repeat", action="store_true", help="the default since issue #16 (kept for old "
		"command lines)")
	parser.add_argument("--walk", choices=sorted(WALKS), default=None, help="the walk of the live recording "
		"(default %s; with --skip-live the recording's own, walk-live.walk)" % DEFAULT_WALK)
	parser.add_argument("--check-route", action="store_true", help="check the walks against the server's map "
		"(read-only, no client, no server) and exit")
	parser.add_argument("--server-dir", default=ct.SERVER_DIR, help="the test server's folder for --check-route "
		"(default %(default)s: PATELLA_TESTSERVER, else C:\\Tibia\\gameserver, tools/testserver.py)")
	parser.add_argument("--patella", metavar="EXE", help="the x64 Patella.exe to test (its test build next to it "
		"runs, issue #92); default build/windows-x64/Release/Patella.exe; as tools/compare_traffic.py's --patella")
	parser.add_argument("--self-test", action="store_true", help="the window placement on a window of its own "
		"(no client, no server) and exit")
	parser.add_argument(desktop.HIDDEN, action="store_true", help=desktop.HIDDEN_HELP)
	parser.add_argument(sandbox.DESKTOP, action="store_true", help=sandbox.DESKTOP_HELP)
	args = parser.parse_args()
	if args.self_test:
		return 1 if self_test() else 0
	if args.check_route:
		problems = []
		for name in ([args.walk] if args.walk else sorted(WALKS)):
			problems += check_route(name, args.server_dir)
			print()
		print("check-route: %s" % ("ok" if not problems else "%d problems" % len(problems)))
		return 1 if problems else 0
	if args.repeat and args.no_repeat:
		raise RunError("--repeat and --no-repeat")
	named = ct.use_patella(args.patella or ct.DEFAULT_PATELLA)
	log(named)
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
	walk_name = args.walk or DEFAULT_WALK
	live = None  # the live recording's run: Tibia.exe's walk run when it is made here (issue #16)
	if any(name != "options" for name in only):
		if not (args.skip_live and os.path.isfile(recording)):
			log("=== live recording (Tibia.exe against the test server), walk %s" % walk_name)
			SCENARIO[0] = "walk"
			live = record_walk(out, walk_name)
		else:
			path = os.path.join(out, "walk-live.walk")
			if os.path.isfile(path):
				with open(path, encoding="ascii") as f:
					recorded = f.read().strip()
				if args.walk and args.walk != recorded:
					raise RunError("%s was recorded with the walk %s, not %s" % (recording, recorded, args.walk))
				walk_name = recorded
			log("reusing %s (walk %s)" % (recording, walk_name))
		events = px.load_events(recording)
		keys = walk_keys(events)
	results = {}
	errors = {}
	for name in only:
		runs = ["orig", "patella"] + ([REPEAT_RUN] if name in REPEATED and args.repeat else [])
		results[name] = {}
		for run in runs:
			log("=== %s: %s" % (name, run))
			SCENARIO[0] = name
			try:
				target = os.path.join(out, "repeat") if run == REPEAT_RUN else out  # <target>\<scenario>\<client>
				if name == "options":
					result = run_options(run, target)
				elif name == "walk" and run == "orig" and live is not None:
					log("the live recording's run is Tibia.exe's (%s)" % os.path.join(out, "walk-live", "orig"))
					result = live
				elif name == "walk":
					result = run_walk(run, target, events, keys, WALKS[walk_name])
				else:
					result = run_error(run, target, events, name)
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
		if name == "walk":
			say("    walk %s, %d actions; Tibia.exe: %s" % (walk_name, len(WALKS[walk_name]["route"]),
				"the live recording's run" if live is not None else "a replay of the recording"))
		failed |= compare_files(say, name, r, r.get(REPEAT_RUN))
		for key in ("exit", "box"):
			if key in r["orig"] or key in r["patella"]:
				expected = r["orig"].get(key)
				if key == "box" and expected:
					# Tibia.exe's boxes as Patella.exe shows them (tools/deliberate.py: the critical error box, #74).
					caption, texts = expected
					for line in deliberate.expected_lines(boxes=[(caption, t) for t in texts]):
						say("    " + line)
					expected = [caption, [deliberate.box_text(caption, t) for t in texts]]
				same = expected == r["patella"].get(key)
				say("    %s: %r / %r %s" % (key, r["orig"].get(key), r["patella"].get(key), "as expected" if same and
					expected != r["orig"].get(key) else "identical" if same else "DIFFERENT"))
				failed |= not same
		if "events" in r["orig"]:
			failed |= compare_packets(say, r)
	moved = [entry for entry in PLACEMENTS if entry[0] in only]
	say()
	say("window placement before the exit (issue #68): %s" % ("every window where Tibia.cfg put it" if not moved
		else "%d moved back to Tibia.cfg's %s" % (len(moved), cfg_window())))
	for scenario, run, found, placed in moved:
		say("    %s, %s: found at %s (showCmd %d), placed at %s" % (scenario, client_exe(run) if run in (
			"orig", "patella", REPEAT_RUN) else run, found[:4], found[4], placed[:4]))
	say()
	say("RESULT: %s" % ("differences found" if failed else "no differences"))
	with open(os.path.join(out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(report) + "\n")
	return 1 if failed else 0


def self_test(out=print):
	"""The window tree's readings with a reader that fails (issue #149, no client), place_window on a window of
	this process's own (a STATIC control as a top-level window, no client, no server) and cfg_window on Tibia.cfg
	lines. Returns the number of failed expectations."""
	failures = 0

	def check(title, ok):
		nonlocal failures
		failures += not ok
		out("  %-60s %s" % (title, "ok" if ok else "FAILED"))

	out("window tree readings (issue #149)")

	class Reader:
		"""A window tree whose first `fail` readings fail as a torn walk of Tibia.exe's does."""
		def __init__(self, fail, tree):
			self.fail, self.tree, self.reads = fail, tree, 0

		def gui(self):
			self.reads += 1
			if self.reads <= self.fail:
				raise cs.ReadError("ReadProcessMemory 0x9 (4 bytes) failed")
			return self.tree

	class FakeDriver:
		def __init__(self, fail, tree):
			self.mem = Reader(fail, tree)
			self.clicks = 0

		def click(self, x, y, right=False):
			self.clicks += 1

	dialog = {"class": "THotkeyDialog", "ptr": 0x1000, "x": 1, "y": 2, "w": 3, "h": 4}
	ok_button = {"class": "TTextButton", "text": "Ok", "ptr": 0x2000, "x": 0, "y": 0, "w": 1, "h": 1}
	fake = FakeDriver(3, [dialog])
	check("three failed readings, then the dialog: found", wait_node(fake, "THotkeyDialog", 5) == dialog
		and fake.mem.reads == 5)
	fake = FakeDriver(2, [])
	wait_gone(fake, "THotkeyDialog", 5)
	check("two failed readings, then no dialog: gone", fake.mem.reads == 3)
	fake = FakeDriver(1, [ok_button])
	check("a failed reading, then the button: found", button(fake, "Ok") == ok_button)
	fake = FakeDriver(10 ** 6, [dialog])
	started = time.time()
	try:
		wait_node(fake, "THotkeyDialog", 0.5)
		check("readings that always fail: the ReadError after the timeout", False)
	except cs.ReadError:
		check("readings that always fail: the ReadError after the timeout", 0.5 <= time.time() - started < 2)
	fake = FakeDriver(10 ** 6, [ok_button])
	try:
		click_open(fake, lambda: ok_button, "THotkeyDialog", wait=0.3)
		check("click_open: a failed reading is no click that opened nothing", False)
	except cs.ReadError:
		check("click_open: a failed reading is no click that opened nothing", fake.clicks == 1)
	except Exception:  # noqa: BLE001
		check("click_open: a failed reading is no click that opened nothing", False)

	out("window placement (issue #68)")
	check("compare_traffic's Tibia.cfg: (10,10,800,600)", cfg_window() == (10, 10, 800, 600))
	check("a negative position", cfg_window("WindowedMode = (-4,6,800,600,no)\r\n") == (-4, 6, 800, 600))
	try:
		cfg_window("Engine = 2\n")
		check("no WindowedMode line: RunError", False)
	except RunError:
		check("no WindowedMode line: RunError", True)
	user32.CreateWindowExW.restype = wt.HWND
	user32.CreateWindowExW.argtypes = [wt.DWORD, wt.LPCWSTR, wt.LPCWSTR, wt.DWORD, ctypes.c_int, ctypes.c_int,
		ctypes.c_int, ctypes.c_int, wt.HWND, wt.HMENU, wt.HINSTANCE, wt.LPVOID]
	user32.DestroyWindow.argtypes = [wt.HWND]
	user32.MoveWindow.argtypes = [wt.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.BOOL]
	user32.ShowWindow.argtypes = [wt.HWND, ctypes.c_int]
	WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW, WS_EX_NOACTIVATE = 0x00CF0000, 0x00000080, 0x08000000
	hwnd = user32.CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, "STATIC", "compare_files self-test",
		WS_OVERLAPPEDWINDOW, 10, 10, 800, 600, None, None, None, None)
	if not hwnd:
		check("CreateWindowExW", False)
		return failures
	PLACEMENTS.clear()
	try:
		user32.ShowWindow(hwnd, SW_SHOWNOACTIVATE)
		target = (10, 10, 800, 600)
		check("in place: left alone", place_window(hwnd, target, "self-test", settle=0) is None and not PLACEMENTS)
		user32.MoveWindow(hwnd, -4, 6, 800, 600, True)
		found = place_window(hwnd, target, "self-test", settle=0)
		check("moved to (-4,6): found there, placed back", found is not None and found[:4] == (-4, 6, 800, 600)
			and window_placement(hwnd)[:4] == target and len(PLACEMENTS) == 1)
		user32.ShowWindow(hwnd, 3)  # SW_MAXIMIZE
		found = place_window(hwnd, target, "self-test", settle=0)
		placed = window_placement(hwnd)
		check("maximised: restored to the place", found is not None and found[4] == 3 and placed[:4] == target
			and placed[4] == SW_SHOWNORMAL)
	finally:
		user32.DestroyWindow(hwnd)
		PLACEMENTS.clear()
	out("compare_files self test: %d failures" % failures)
	return failures


def kill_client(run):
	"""Ends a client of ours left running after a failed run: only the exe in its run folder."""
	path = os.path.join(folder(run), client_exe(run)).lower()
	result = subprocess.run(["powershell", "-NoProfile", "-Command",
		"Get-Process | Where-Object { $_.Path -and $_.Path.ToLower() -eq '%s' } | Stop-Process -Force" % path.replace("'", "''")],
		capture_output=True, text=True)
	return result.returncode


if __name__ == "__main__":
	# issue #147: inside Windows Sandbox unless --desktop (or --hidden) is given
	sandbox.redirect(writable=[FILES], skip=("record", "compare", "--check-only", "--check-route", "--self-test", "-h",
		"--help"))
	desktop.hide(skip=("--check-only", "--check-route", "--self-test", "-h", "--help"))  # issue #56: only with --hidden
	try:
		sys.exit(main())
	except (RunError, sc.SessionError, px.ProtocolError) as e:
		traceback.print_exc()
		print("compare_files.py: %s" % e, file=sys.stderr)
		sys.exit(1)
