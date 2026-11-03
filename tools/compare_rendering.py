r"""Compare the rendering of Patella.exe and the original Tibia.exe (issue #20).

Both clients play the same scenes with each graphics engine (DirectX 5: SurfaceDX.cpp, DirectX 9:
surfaceDX9.cpp, OpenGL: SurfaceGL.cpp); the frame each client composes is read back just before
it is shown and the pictures are compared pixel by pixel. notes/rendering.md describes the
method and the results; notes/decisions.md "Rendering comparison (issue #20)" the choices.

Usage:
  tools\build-patella.cmd
  python -I tools/compare_rendering.py [--out build\render\result] [--engines dx5,dx9,gl]
	[--modes windowed,fullscreen] [--no-repeat] [--recording build\render\session.jsonl] [--compare-only]

The capture (tools/capture/patcap.c, built by tools\build-capture.cmd) is a DLL the run folders'
copies of the clients import (add_import), so it is loaded before the client's entry point. It
reads the composed frame from the engine's own surfaces (DirectDraw Blt/Flip, Direct3D 9 Present,
SwapBuffers), so no display is needed, and it makes time deterministic: the clock (timeGetTime,
the wall clock) and the main timer's ticks (WM_TIMER, ActionNumber) move only when this script
says so, by the same amounts in both clients. The game scenes come from a recorded session
replayed by tools/proxy.py (both clients get the same server bytes); no server is needed.

Exit code 0 when every scene of every engine is pixel-identical outside the masks (one: the
measured frame rate in the advanced graphics options, notes/rendering.md), 1 on a difference, 2
when a run failed.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import mmap
import os
import shutil
import struct
import subprocess
import sys
import time
import traceback
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_traffic as ct  # noqa: E402
import proxy as px  # noqa: E402
import session_client as sc  # noqa: E402
import login_client as lc  # noqa: E402
import client_state  # noqa: E402

ROOT = ct.ROOT
RENDER = os.path.join(ROOT, "build", "render")
CAPTURE = os.path.join(ROOT, "build", "capture")
DLL = os.path.join(CAPTURE, "patcap.dll")
CLIENTS = ct.CLIENTS
RunError = ct.RunError
log = ct.log

# patcap.c struct Shared
CMD_ADVANCE, CMD_CAPTURE, CMD_REALTIME = 1, 2, 3
API_NAMES = {0: "none", 1: "DirectDraw Blt", 2: "DirectDraw Flip", 3: "Direct3D 9 Present", 4: "OpenGL SwapBuffers"}
OFF = {"Magic": 0, "Version": 4, "CmdSeq": 8, "DoneSeq": 12, "Cmd": 16, "Arg0": 20, "Arg1": 24, "Result": 28,
	"Frames": 32, "Api": 36, "Now": 40, "Ticks": 44, "TimerWindow": 48, "TimerActive": 52, "Width": 56,
	"Height": 60, "SourceBits": 64, "Hooks": 68, "Path": 72, "Message": 592, "ApiFrames": 848, "Realtime": 868,
	"FirstCall": 872}
HOOK_NAMES = ["timeGetTime", "GetSystemTimeAsFileTime", "GetSystemTimePreciseAsFileTime", "GetProcAddress",
	"SetTimer", "KillTimer", "GetCursorPos", "PeekMessageA", "SwapBuffers", "wglMakeCurrent", "DirectDrawCreate"]


class Capture:
	"""The DLL's shared memory in one client."""

	def __init__(self, pid):
		self.pid = pid
		self.map = mmap.mmap(-1, 4096, tagname="Local\\PatellaCapture-%d" % pid)
		if self.get("Magic") != 0x50434150:  # 'PCAP' as the compiler stores the multi-character constant
			raise RunError("patcap.dll is not loaded in PID %d (magic 0x%x)" % (pid, self.get("Magic")))

	def close(self):
		self.map.close()

	def get(self, name, index=0):
		return struct.unpack_from("<i", self.map, OFF[name] + 4 * index)[0]

	def put(self, name, value):
		struct.pack_into("<i", self.map, OFF[name], value)

	def text(self, name, size):
		raw = self.map[OFF[name]:OFF[name] + size]
		return raw.split(b"\0", 1)[0].decode("latin-1")

	def hooks(self):
		bits = self.get("Hooks")
		return [n for i, n in enumerate(HOOK_NAMES) if bits & (1 << i)]

	def command(self, cmd, arg0=0, arg1=0, path="", timeout=30, alive=None):
		if self.get("CmdSeq") != self.get("DoneSeq"):
			raise RunError("the previous command is not done")
		raw = path.encode("latin-1") + b"\0"
		self.map[OFF["Path"]:OFF["Path"] + len(raw)] = raw
		self.put("Cmd", cmd)
		self.put("Arg0", arg0)
		self.put("Arg1", arg1)
		self.put("Result", -1)
		seq = self.get("CmdSeq") + 1
		self.put("CmdSeq", seq)
		deadline = time.time() + timeout
		while self.get("DoneSeq") != seq:
			if time.time() > deadline:
				raise RunError("command %d not done within %d s (frames %d)" % (cmd, timeout, self.get("Frames")))
			if alive is not None and not alive():
				raise RunError("the client ended during command %d" % cmd)
			time.sleep(0.01)
		if self.get("Result") != 0:
			raise RunError("command %d failed: %d %s" % (cmd, self.get("Result"), self.text("Message", 256)))

	def advance(self, ms, ticks, alive=None):
		self.command(CMD_ADVANCE, ms, ticks, alive=alive)

	def capture(self, path, skip=2, alive=None):
		self.command(CMD_CAPTURE, skip, 0, path, alive=alive)
		return self.get("Width"), self.get("Height"), API_NAMES.get(self.get("Api"), "?")


def build_capture():
	source = os.path.join(ROOT, "tools", "capture", "patcap.c")
	if os.path.isfile(DLL) and os.path.getmtime(DLL) >= os.path.getmtime(source):
		return
	log("building the capture helper (tools\\build-capture.cmd)")
	result = subprocess.run(["cmd", "/c", os.path.join(ROOT, "tools", "build-capture.cmd")], capture_output=True,
		text=True)
	if result.returncode != 0:
		raise RunError("build-capture.cmd failed:\n%s%s" % (result.stdout, result.stderr))


# --- the import of patcap.dll ------------------------------------------------------------

def add_import(source, target, dll="patcap.dll", function="PatcapLoaded"):
	"""Copies the PE image `source` to `target` with one more import descriptor: `dll`, by name
	`function`. A new last section (.patcap, initialised read/write data) holds the new import
	directory (the old descriptors, unchanged, then the new one), its lookup and address tables
	and the names; the import data directory points at it, the bound import directory (stale once
	the directory moved) is cleared. The loader then loads the DLL with the client's other
	dependencies, before the client's entry point. Nothing else changes: afterwards the bytes
	outside the headers and the new section are compared with the source."""
	with open(source, "rb") as f:
		data = bytearray(f.read())
	pe = struct.unpack_from("<I", data, 0x3c)[0]
	if data[pe:pe + 4] != b"PE\0\0":
		raise RunError("%s: not a PE image" % source)
	sections, = struct.unpack_from("<H", data, pe + 6)
	optsize, = struct.unpack_from("<H", data, pe + 20)
	opt = pe + 24
	if struct.unpack_from("<H", data, opt)[0] != 0x10b:
		raise RunError("%s: not PE32" % source)
	section_align, file_align = struct.unpack_from("<II", data, opt + 32)
	size_of_image, size_of_headers = struct.unpack_from("<II", data, opt + 56)
	dirs = opt + 96
	import_rva, import_size = struct.unpack_from("<II", data, dirs + 8)
	table = opt + optsize
	new_header = table + 40 * sections
	first_raw = min(struct.unpack_from("<I", data, table + 40 * i + 20)[0] for i in range(sections)
		if struct.unpack_from("<I", data, table + 40 * i + 16)[0])
	if new_header + 40 > min(size_of_headers, first_raw) or any(data[new_header:new_header + 40]):
		raise RunError("%s: no room for another section header" % source)

	def rva_to_offset(rva):
		for i in range(sections):
			vsize, va, rawsize, raw = struct.unpack_from("<IIII", data, table + 40 * i + 8)
			if va <= rva < va + max(vsize, rawsize):
				return raw + rva - va
		raise RunError("RVA 0x%x outside the sections" % rva)

	old = []
	at = rva_to_offset(import_rva)
	while any(data[at:at + 20]):
		old.append(bytes(data[at:at + 20]))
		at += 20
	# (va, vsize) of the section with the highest address
	last_va, last_vsize = max(struct.unpack_from("<II", data, table + 40 * i + 8)[::-1] for i in range(sections))
	va =(last_va + last_vsize + section_align - 1) // section_align * section_align
	raw = (len(data) + file_align - 1) // file_align * file_align
	# layout of the new section
	directory = 0
	lookup = 20 * (len(old) + 2)
	address = lookup + 8
	hint_name = address + 8
	dll_name = hint_name + 2 + len(function) + 1
	dll_name += dll_name & 1
	end = dll_name + len(dll) + 1
	body = bytearray(end)
	for i, descriptor in enumerate(old):
		body[20 * i:20 * i + 20] = descriptor
	struct.pack_into("<IIIII", body, 20 * len(old), va + lookup, 0, 0, va + dll_name, va + address)
	struct.pack_into("<I", body, lookup, va + hint_name)
	struct.pack_into("<I", body, address, va + hint_name)
	body[hint_name + 2:hint_name + 2 + len(function)] = function.encode("ascii")
	body[dll_name:dll_name + len(dll)] = dll.encode("ascii")
	raw_size = (len(body) + file_align - 1) // file_align * file_align
	body += bytes(raw_size - len(body))
	data += bytes(raw - len(data))
	data += body
	header = struct.pack("<8sIIIIIIHHI", b".patcap", end, va, raw_size, raw, 0, 0, 0, 0,
		0x40 | 0x40000000 | 0x80000000)  # initialised data, read, write
	data[new_header:new_header + 40] = header
	struct.pack_into("<H", data, pe + 6, sections + 1)
	struct.pack_into("<I", data, opt + 56, (va + end + section_align - 1) // section_align * section_align)
	struct.pack_into("<II", data, dirs + 8, va + directory, 20 * (len(old) + 1))
	struct.pack_into("<II", data, dirs + 8 * 11, 0, 0)  # bound imports
	struct.pack_into("<I", data, opt + 64, 0)  # checksum (not checked for an EXE)
	with open(source, "rb") as f:
		original = f.read()
	changed = [i for i in range(min(len(original), len(data))) if original[i] != data[i]]
	allowed = set(range(pe + 6, pe + 8)) | set(range(opt + 56, opt + 60)) | set(range(opt + 64, opt + 68)) \
		| set(range(dirs + 8, dirs + 16)) | set(range(dirs + 88, dirs + 96)) | set(range(new_header, new_header + 40))
	if any(i not in allowed for i in changed):
		raise RunError("add_import changed bytes outside the headers it should touch")
	with open(target, "wb") as f:
		f.write(data)
	return len(old)


class Driver(sc.Driver):
	"""session_client's driver for a client whose copy imports patcap.dll (add_import)."""

	def start(self, arguments=()):
		path = os.path.join(self.run_dir, self.exe)
		self.proc = subprocess.Popen([path] + list(arguments), cwd=self.run_dir)
		self.log("started %s, PID %d" % (path, self.proc.pid))
		deadline = time.time() + 30
		self.cap = None
		while (self.hwnd is None or self.cap is None) and time.time() < deadline:
			if self.proc.poll() is not None:
				raise sc.SessionError("the client exited with code %d" % self.proc.returncode)
			if self.cap is None:
				try:
					self.cap = Capture(self.proc.pid)
				except RunError:
					pass
			self.hwnd = lc.find_window(self.proc.pid)
			time.sleep(0.25)
		if self.cap is None:
			raise RunError("patcap.dll is not loaded in %s" % self.exe)
		if self.hwnd is None:
			raise sc.SessionError("no TibiaClient window after 30 s")
		self.log("patcap.dll loaded; hooks: %s" % ", ".join(self.cap.hooks()))
		time.sleep(2)
		self.mem = client_state.Client(self.proc.pid, self.pdb_exe)

	def alive(self):
		return self.proc is not None and self.proc.poll() is None

	def advance(self, ms=2000, ticks=20):
		self.cap.advance(ms, ticks, alive=self.alive)

	def capture(self, path, skip=2):
		return self.cap.capture(path, skip, alive=self.alive)


# --- pictures ----------------------------------------------------------------------------

def read_bmp(path):
	"""(width, height, rows of RGB bytes top-down) of patcap's 24-bit BMP."""
	with open(path, "rb") as f:
		data = f.read()
	offset, = struct.unpack_from("<I", data, 10)
	width, height, planes, bits = struct.unpack_from("<iiHH", data, 18)
	if bits != 24 or height <= 0:
		raise RunError("%s: not a bottom-up 24-bit BMP" % path)
	row = (width * 3 + 3) & ~3
	rows = []
	for y in range(height - 1, -1, -1):
		line = data[offset + y * row:offset + y * row + width * 3]
		rows.append(swap_bgr(line))
	return width, height, rows


def swap_bgr(line):
	out = bytearray(line)
	out[0::3], out[2::3] = line[2::3], line[0::3]
	return bytes(out)


def write_png(path, width, height, rows):
	def chunk(kind, payload):
		return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)

	raw = b"".join(b"\0" + r for r in rows)
	with open(path, "wb") as f:
		f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
			+ chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))




def read_png(path):
	"""(width, height, rows) of a PNG written by write_png (8-bit RGB, filter 0, one IDAT)."""
	with open(path, "rb") as f:
		data = f.read()
	width, height = struct.unpack_from(">II", data, 16)
	at, idat = 8, b""
	while at < len(data):
		length, = struct.unpack_from(">I", data, at)
		kind = data[at + 4:at + 8]
		if kind == b"IDAT":
			idat += data[at + 8:at + 8 + length]
		at += 12 + length
	raw = zlib.decompress(idat)
	stride = width * 3 + 1
	rows = []
	for y in range(height):
		if raw[y * stride] != 0:
			raise RunError("%s: unexpected PNG filter" % path)
		rows.append(raw[y * stride + 1:(y + 1) * stride])
	return width, height, rows


def pixel_digest(rows):
	return hashlib.sha256(b"".join(rows)).hexdigest()


def masked(x, y, masks):
	return any(m[0] <= x < m[2] and m[1] <= y < m[3] for m in masks)


def diff_pictures(a, b, masks=()):
	"""Differing pixels of two pictures: (count, bounding box (x0, y0, x1, y1) or None, the
	differing pixels inside the masks, which are not counted)."""
	wa, ha, ra = a
	wb, hb, rb = b
	if (wa, ha) != (wb, hb):
		return wa * ha, (0, 0, max(wa, wb), max(ha, hb)), 0
	count, box, hidden = 0, None, 0
	for y in range(ha):
		if ra[y] == rb[y]:
			continue
		for x in range(wa):
			if ra[y][3 * x:3 * x + 3] != rb[y][3 * x:3 * x + 3]:
				if masked(x, y, masks):
					hidden += 1
					continue
				count += 1
				box = (x, y, x + 1, y + 1) if box is None else (min(box[0], x), min(box[1], y), max(box[2], x + 1),
					max(box[3], y + 1))
	return count, box, hidden


def write_diff(path, a, b):
	"""The first picture darkened, the differing pixels in red."""
	width, height, ra = a
	rb = b[2]
	rows = []
	for y in range(height):
		row = bytearray(ra[y][i] // 3 for i in range(width * 3))
		if ra[y] != rb[y]:
			for x in range(width):
				if ra[y][3 * x:3 * x + 3] != rb[y][3 * x:3 * x + 3]:
					row[3 * x:3 * x + 3] = b"\xff\x00\x00"
		rows.append(bytes(row))
	write_png(path, width, height, rows)


# --- engines and scenes ------------------------------------------------------------------

# Tibia.cfg's Engine (Surface.cpp GetSurface: 0 DirectX 5, 1 OpenGL, 2 DirectX 9) and the API
# that must show the frames (else the client fell back to another engine).
ENGINES = {
	"dx5": {"engine": 0, "api": "DirectDraw Blt", "source": "SurfaceDX.cpp"},
	"dx9": {"engine": 2, "api": "Direct3D 9 Present", "source": "surfaceDX9.cpp"},
	"gl": {"engine": 1, "api": "OpenGL SwapBuffers", "source": "SurfaceGL.cpp"},
}

# Every step ends with this much virtual time and these many ticks of the main timer, the same
# in every run: animations, steps, effects and talk boxes are then at the same point.
ADVANCE_MS = 2000
ADVANCE_TICKS = 20
SKIP_FRAMES = 2  # frames drawn after the advance before the capture

# The scenes, in session order (the replayed session: tools/session_client.py run_session).
SCENES = ["start", "options", "graphics-options", "advanced-options", "login", "charlist", "game", "automap-zoom",
	"walk north", "walk east", "walk south", "walk west", "turn south", "say", "look north", "open container",
	"look in container", "use in container", "context menu", "trade with", "trade request", "look at offer",
	"cancel trade", "close container", "side windows", "logout"]


FULLSCREEN_SCENES = [n for n in SCENES if n not in ("options", "graphics-options", "advanced-options")]

# The fullscreen runs use the desktop's own mode, so that no display mode really changes.
FULLSCREEN_MODE = (1024, 768, 32)


def display_mode():
	"""The primary display's current mode: (width, height, bits, frequency)."""
	class DEVMODE(ctypes.Structure):
		_fields_ = [("dmDeviceName", ctypes.c_wchar * 32), ("dmSpecVersion", wt.WORD), ("dmDriverVersion", wt.WORD),
			("dmSize", wt.WORD), ("dmDriverExtra", wt.WORD), ("dmFields", wt.DWORD), ("dmPositionX", ctypes.c_long),
			("dmPositionY", ctypes.c_long), ("dmDisplayOrientation", wt.DWORD), ("dmDisplayFixedOutput", wt.DWORD),
			("dmColor", ctypes.c_short), ("dmDuplex", ctypes.c_short), ("dmYResolution", ctypes.c_short),
			("dmTTOption", ctypes.c_short), ("dmCollate", ctypes.c_short), ("dmFormName", ctypes.c_wchar * 32),
			("dmLogPixels", wt.WORD), ("dmBitsPerPel", wt.DWORD), ("dmPelsWidth", wt.DWORD),
			("dmPelsHeight", wt.DWORD), ("dmDisplayFlags", wt.DWORD), ("dmDisplayFrequency", wt.DWORD)]

	mode = DEVMODE()
	mode.dmSize = ctypes.sizeof(mode)
	if not ctypes.windll.user32.EnumDisplaySettingsW(None, -1, ctypes.byref(mode)):  # ENUM_CURRENT_SETTINGS
		raise RunError("EnumDisplaySettings failed")
	return (mode.dmPelsWidth, mode.dmPelsHeight, mode.dmBitsPerPel, mode.dmDisplayFrequency)


def scene_file(name):
	return name.replace(" ", "-") + ".png"


def cfg_for(engine, fullscreen=False):
	text = ct.TIBIA_CFG
	if "Engine = 2" not in text or "TestServer = no\n" not in text:
		raise RunError("compare_traffic.TIBIA_CFG has no 'Engine = 2' or 'TestServer = no' line")
	text = text.replace("Engine = 2", "Engine = %d" % ENGINES[engine]["engine"])
	if fullscreen:
		text = text.replace("TestServer = no\n", "TestServer = no\nFullscreenMode = (%d,%d,%d)\n" % FULLSCREEN_MODE)
	return text


def setup():
	"""Run folders build\\render\\orig and build\\render\\patella: compare_traffic's set-up (copies
	of the data files, the clients patched by patch_client.py), then the import of patcap.dll."""
	build_capture()
	ct.set_folder(RENDER)
	ct.setup()
	for client in ("orig", "patella"):
		exe = os.path.join(ct.run_dir(client), CLIENTS[client]["exe"])
		count = add_import(exe, exe)
		shutil.copyfile(DLL, os.path.join(ct.run_dir(client), "patcap.dll"))
		log("%s: patcap.dll added to its %d imports" % (exe, count))


def button(driver, text):
	for n in driver.mem.gui():
		if n["class"] == "TTextButton" and n.get("text") == text:
			return n
	raise RunError("no button %r" % text)


def click_node(driver, node):
	driver.click(node["x"] + node["w"] // 2, node["y"] + node["h"] // 2)


def wait_node(driver, cls, timeout=10):
	deadline = time.time() + timeout
	while time.time() < deadline:
		for n in driver.mem.gui():
			if n["class"] == cls:
				return n
		time.sleep(0.2)
	raise RunError("no window of class %s after %d s" % (cls, timeout))


def wait_gone(driver, cls, timeout=10):
	deadline = time.time() + timeout
	while time.time() < deadline:
		if not any(n["class"] == cls for n in driver.mem.gui()):
			return
		time.sleep(0.2)
	raise RunError("the window of class %s is still open after %d s" % (cls, timeout))


def switch_fullscreen(driver):
	"""Options, Graphics: the Fullscreen check box (TGraphicsOptionsDialog.Fullscreen, +0x2c in the
	original; TCheckBox.Checked, +0x30) and OK: HandleEvent stores the selected mode (Tibia.cfg's
	FullscreenMode, the desktop's own) and calls SetDisplayMode(2). Then the options dialog's Ok."""
	click_node(driver, button(driver, "Options"))
	wait_node(driver, "TOptionsDialog")
	click_node(driver, button(driver, "Graphics"))
	dialog = wait_node(driver, "TGraphicsOptionsDialog")
	box = driver.mem.ptr(dialog["ptr"] + driver.mem.off("TGraphicsOptionsDialog.Fullscreen"))
	node = driver.node_by_ptr(box)
	driver.click(node["x"] + 6, node["y"] + node["h"] // 2)
	time.sleep(0.3)
	if driver.mem.read(box + driver.mem.off("TCheckBox.Checked"), 1)[0] == 0:
		raise RunError("the Fullscreen check box did not change")
	driver.char(13)
	wait_gone(driver, "TGraphicsOptionsDialog")
	time.sleep(2)
	click_node(driver, button(driver, "Ok"))
	wait_gone(driver, "TOptionsDialog")
	time.sleep(1)


def session(driver, proxy, shot, fullscreen=False):
	"""The scenes: the start screen, the option dialogs, the login, then the replayed game session
	(session_client.run_session) with an automap zoom; shot(name) captures. fullscreen: switch to
	fullscreen mode first, then the same scenes without the option dialogs."""
	def settle(name=None):
		if proxy is not None:
			proxy.wait_idle(0.5, 60)
		time.sleep(0.3)
		driver.advance(ADVANCE_MS, ADVANCE_TICKS)
		if proxy is not None:
			proxy.wait_idle(0.5, 60)
		time.sleep(0.3)

	def step(name, action):
		action()
		settle()
		shot(name)

	settle()
	if fullscreen:
		switch_fullscreen(driver)
		settle()
		shot("start")
	else:
		shot("start")
		step("options", lambda: click_node(driver, button(driver, "Options")))
		wait_node(driver, "TOptionsDialog")
		step("graphics-options", lambda: click_node(driver, button(driver, "Graphics")))
		wait_node(driver, "TGraphicsOptionsDialog")
		click_node(driver, button(driver, "Advanced"))
		dialog = wait_node(driver, "TAdvancedGraphicsDialog")
		settle()
		shot("advanced-options", masks=[framerate_mask(driver, dialog)])
		for dialog in ("TAdvancedGraphicsDialog", "TGraphicsOptionsDialog", "TOptionsDialog"):
			texts = [n.get("text") for n in driver.mem.gui() if n["class"] == "TTextButton"]
			click_node(driver, button(driver, "Cancel" if "Cancel" in texts else "Ok"))
			wait_gone(driver, dialog)
			settle()

	def enter_login():
		_, _, width, height = lc.client_offset(driver.hwnd)
		driver.click(113, height - 220)  # Enter Game (session_client.login)
		time.sleep(1.0)
		driver.type(sc.ACCOUNT)
		driver.char(9)
		driver.type(sc.PASSWORD)

	step("login", enter_login)

	def charlist():
		driver.char(13)
		proxy.wait_idle(0.5, 60)
		time.sleep(0.5)

	step("charlist", charlist)

	def enter_game():
		names = driver.mem.characters()
		if sc.CHARACTER not in names:
			raise RunError("%s is not in the character list %s" % (sc.CHARACTER, names))
		for _ in range(names.index(sc.CHARACTER)):
			driver.key(sc.VK_DOWN)
		driver.char(13)
		deadline = time.time() + 15
		while not any(e["kind"] == "game" and e["dir"] == "c2s" for e in proxy.events):
			if time.time() > deadline:
				raise RunError("no game connection")
			time.sleep(0.2)

	step("game", enter_game)

	automap = wait_node(driver, "TAutomapWindow")

	def zoom_in():
		click_node(driver, driver.node_by_ptr(driver.mem.ptr(automap["ptr"]
			+ driver.mem.off("TAutomapWindow.ZoomInButton"))))

	step("automap-zoom", zoom_in)
	click_node(driver, driver.node_by_ptr(driver.mem.ptr(automap["ptr"]
		+ driver.mem.off("TAutomapWindow.ZoomOutButton"))))  # back to zoom 2
	settle()

	def side_windows():
		"""The side bar's Skills, Battle and VIP toggle buttons (client-side windows, no packets).
		TButtonsWindow (ContentWindows.cpp) puts them at SkinProperties.Data60C's x/y and
		width/height and Data61C's x/y, and the Logout TTextButton at Data61C's width/height:
		the places follow from the Logout button's node (Tibia.exe has no name for the
		TTextToggleButton class)."""
		logout = button(driver, "Logout")
		x60c, y60c, w60c, h60c = driver.mem.skin_rects("Data60C", 1)[0]
		x61c, y61c, w61c, h61c = driver.mem.skin_rects("Data61C", 1)[0]
		left, top = logout["x"] - w61c, logout["y"] - h61c
		for x, y in ((x60c, y60c), (w60c, h60c), (x61c, y61c)):
			driver.click(left + x + logout["w"] // 2, top + y + logout["h"] // 2)
			time.sleep(0.3)

	def session_wait(name):
		settle()
		if name in ("context menu", "trade with"):  # steps run_session takes no snapshot of
			shot(name)

	def checkpoint(name):
		shot(name)
		if name == "close container":  # the last step before the logout: the side windows stay open
			step("side windows", side_windows)

	sc.run_session(driver, session_wait, checkpoint, with_login=False)
	shot("logout")


# "Current framerate: %2.2f" (Dialogs.cpp TAdvancedGraphicsDialog::Draw) at SkinProperties.Style1394.Data70
# from the dialog's corner: the frame rate the engine measured (frames drawn per second of the
# clock), which depends on how fast the machine draws, not on the client. Its number is masked:
# from FRAMERATE_PREFIX pixels after the point (the width of "Current framerate: " in the font,
# measured in the pictures: the colon ends 110 pixels after it, the number starts at 113),
# FRAMERATE_WIDTH wide (the number takes about 45), FRAMERATE_HEIGHT high (the text 9).
FRAMERATE_PREFIX = 112
FRAMERATE_WIDTH = 100
FRAMERATE_HEIGHT = 16


def framerate_mask(driver, dialog):
	x = dialog["x"] + driver.mem.skin_int("Style1394.Data70.x") + FRAMERATE_PREFIX
	y = dialog["y"] + driver.mem.skin_int("Style1394.Data70.y")
	return [x, y, x + FRAMERATE_WIDTH, y + FRAMERATE_HEIGHT]


def run(engine, client, out, recording, label=None, fullscreen=False):
	"""One client with one engine through all scenes; the pictures go to out\\<engine>\\<label>
	(out\\<engine>-fullscreen\\<label> in fullscreen mode). Returns {"scenes": {name: digest},
	"facts": {...}, "problems": [...]}."""
	label = label or client
	folder = os.path.join(out, engine + ("-fullscreen" if fullscreen else ""), label)
	if os.path.isdir(folder):
		shutil.rmtree(folder)
	os.makedirs(folder)
	log("=== %s, %s (%s)" % (engine, label, CLIENTS[client]["exe"]))
	with open(os.path.join(ct.run_dir(client), "Tibia.cfg"), "w", encoding="ascii", newline="\r\n") as f:
		f.write(cfg_for(engine, fullscreen))
	mode_before = display_mode()
	info = CLIENTS[client]
	driver = Driver(ct.run_dir(client), info["exe"], info["pdb"])
	scenes, apis, scene_masks = {}, set(), {}
	proxy = px.Proxy(os.path.join(folder, "replay.jsonl"), px.load_private_key(ct.PEM),
		replay=px.load_events(recording), log=log)

	def shot(name, masks=None):
		if masks:
			scene_masks[name] = masks
		bmp = os.path.join(folder, "capture.bmp")
		width, height, api = driver.capture(bmp, SKIP_FRAMES)
		apis.add(api)
		picture = read_bmp(bmp)
		os.remove(bmp)
		write_png(os.path.join(folder, scene_file(name)), *picture)
		scenes[name] = pixel_digest(picture[2])
		log("scene %s: %dx%d (%s)" % (name, width, height, api))

	try:
		with proxy:
			driver.start()
			session(driver, proxy, shot, fullscreen)
			time.sleep(0.5)
			code = driver.close()
			if code != 0:
				raise RunError("%s exited with code %d" % (info["exe"], code))
		facts = {"apis": sorted(apis), "frames": driver.cap.get("Frames") if driver.cap else None}
	finally:
		facts_cap = None
		if getattr(driver, "cap", None) is not None:
			facts_cap = {"frames": driver.cap.get("Frames"), "ticks": driver.cap.get("Ticks"),
				"virtual_ms": driver.cap.get("Now"), "hooks": driver.cap.hooks()}
		driver.kill()
	problems = proxy.mismatches + proxy.errors
	api = "DirectDraw Flip" if fullscreen and engine == "dx5" else ENGINES[engine]["api"]
	if apis != {api}:
		problems.append("the frames came from %s, not %s (the client chose another engine or mode)"
			% (sorted(apis), api))
	mode_after = display_mode()
	if mode_after != mode_before:
		problems.append("the display mode changed from %s to %s" % (mode_before, mode_after))
	result = {"scenes": scenes, "masks": scene_masks, "facts": dict(facts, **(facts_cap or {})),
		"problems": [str(p) for p in problems]}
	with open(os.path.join(folder, "run.json"), "w", encoding="utf-8", newline="\n") as f:
		json.dump(result, f, indent=1)
	return result


def run_masks(out, engine, label):
	path = os.path.join(out, engine, label, "run.json")
	if not os.path.isfile(path):
		return {}
	with open(path, encoding="utf-8") as f:
		return json.load(f).get("masks", {})


def compare_runs(out, engine, a, b, write_diffs=True, scenes=None):
	"""Scene by scene: (name, None when identical, "missing", ("masks", masks of a, masks of b)
	when the runs masked different areas, else (count, box, diff file, masked pixels that
	differ, masks)); count 0 means identical outside the masks."""
	rows = []
	masks_a, masks_b = run_masks(out, engine, a), run_masks(out, engine, b)
	for name in scenes or SCENES:
		pa = os.path.join(out, engine, a, scene_file(name))
		pb = os.path.join(out, engine, b, scene_file(name))
		if not (os.path.isfile(pa) and os.path.isfile(pb)):
			rows.append((name, "missing"))
			continue
		if masks_a.get(name) != masks_b.get(name):
			rows.append((name, ("masks", masks_a.get(name), masks_b.get(name))))
			continue
		masks = masks_a.get(name) or []
		picture_a, picture_b = read_png(pa), read_png(pb)
		if picture_a == picture_b:
			rows.append((name, None))
			continue
		count, box, hidden = diff_pictures(picture_a, picture_b, masks)
		diff = None
		if write_diffs:
			diff = os.path.join(out, engine, "diff-%s-%s" % (b, scene_file(name)))
			write_diff(diff, picture_a, picture_b)
		rows.append((name, (count, box, diff, hidden, masks)))
	return rows


def main():
	parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
	parser.add_argument("--out", default=os.path.join(RENDER, "result"))
	parser.add_argument("--engines", default="dx5,dx9,gl")
	parser.add_argument("--modes", default="windowed,fullscreen",
		help="windowed and/or fullscreen (in the desktop's own mode, %dx%dx%d)" % FULLSCREEN_MODE)
	parser.add_argument("--recording", default=os.path.join(RENDER, "session.jsonl"),
		help="a live recording of session_client's session (compare_traffic.py's <client>-live.jsonl)")
	parser.add_argument("--no-repeat", action="store_true", help="skip the second run of Tibia.exe")
	parser.add_argument("--compare-only", action="store_true", help="compare the pictures already in --out")
	args = parser.parse_args()
	args.out = os.path.abspath(args.out)  # the DLL writes the pictures from the client's folder
	args.recording = os.path.abspath(args.recording)
	engines = [e for e in args.engines.split(",") if e]
	for e in engines:
		if e not in ENGINES:
			raise SystemExit("unknown engine %r (dx5, dx9, gl)" % e)
	modes = [m for m in args.modes.split(",") if m]
	for m in modes:
		if m not in ("windowed", "fullscreen"):
			raise SystemExit("unknown mode %r (windowed, fullscreen)" % m)
	if "fullscreen" in modes and display_mode()[:3] != FULLSCREEN_MODE:
		raise SystemExit("the desktop is in %s, not %s: the fullscreen runs would change the display mode"
			% (display_mode(), FULLSCREEN_MODE))
	passes = [(e, m == "fullscreen") for m in modes for e in engines]
	if not os.path.isfile(args.recording):
		raise SystemExit("no recording %s: copy compare_traffic.py's orig-live.jsonl there, or pass --recording"
			% args.recording)
	os.makedirs(args.out, exist_ok=True)
	runs = {}
	failed = []
	if not args.compare_only:
		setup()
		for engine, fullscreen in passes:
			plan = [("orig", "orig"), ("patella", "patella")] + ([] if args.no_repeat else [("orig", "orig-repeat")])
			for client, label in plan:
				name = engine + (" fullscreen" if fullscreen else "")
				try:
					runs[(name, label)] = run(engine, client, args.out, args.recording, label, fullscreen)
				except Exception as e:
					traceback.print_exc()
					failed.append("%s %s: %s" % (name, label, e))
					kill_clients()
				if display_mode()[:3] != FULLSCREEN_MODE and "fullscreen" in modes:
					failed.append("the display mode is now %s: no more runs" % (display_mode(),))
					break
	report = []
	say = report.append
	say("Rendering comparison (issue #20), %s" % time.strftime("%Y-%m-%d %H:%M"))
	say("recording %s; every step: %d ms and %d ticks of virtual time, capture after %d frames"
		% (args.recording, ADVANCE_MS, ADVANCE_TICKS, SKIP_FRAMES))
	differences = 0
	for engine, fullscreen in passes:
		say("")
		say("== %s%s (%s, Engine = %d)" % (engine, " fullscreen %dx%dx%d" % FULLSCREEN_MODE if fullscreen else " windowed",
			ENGINES[engine]["source"], ENGINES[engine]["engine"]))
		folder = engine + ("-fullscreen" if fullscreen else "")
		for label in ("orig", "patella", "orig-repeat"):
			path = os.path.join(args.out, folder, label, "run.json")
			if os.path.isfile(path):
				with open(path, encoding="utf-8") as f:
					r = json.load(f)
				say("  %s: %d scenes, %s, problems: %s" % (label, len(r["scenes"]), r["facts"], r["problems"] or "none"))
				if r["problems"]:
					differences += 1
		pairs = [("orig", "patella")]
		if os.path.isdir(os.path.join(args.out, folder, "orig-repeat")):
			pairs.append(("orig", "orig-repeat"))
		for a, b in pairs:
			say("  %s against %s:" % (b, a))
			for name, result in compare_runs(args.out, folder, a, b,
					scenes=FULLSCREEN_SCENES if fullscreen else SCENES):
				if result is None:
					say("    %-18s identical" % name)
				elif result == "missing":
					say("    %-18s missing" % name)
					differences += 1
				elif result[0] == "masks":
					say("    %-18s the runs masked different areas: %s, %s" % (name, result[1], result[2]))
					differences += 1
				else:
					count, box, diff, hidden, masks = result
					if count == 0:
						say("    %-18s identical outside the mask %s (%d pixels differ inside it); %s"
							% (name, masks, hidden, os.path.relpath(diff, args.out)))
					else:
						say("    %-18s %d pixels differ, in %s (%d more inside the mask %s); %s"
							% (name, count, box, hidden, masks, os.path.relpath(diff, args.out)))
						differences += 1
	if failed:
		say("")
		say("FAILED RUNS:")
		for f in failed:
			say("  " + f)
	say("")
	say("result: %s" % ("FAILED" if failed else "differences" if differences else "all scenes identical"))
	text = "\n".join(report) + "\n"
	with open(os.path.join(args.out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write(text)
	print(text)
	return 2 if failed else 1 if differences else 0


def kill_clients():
	"""Ends a client of this script's run folders left behind by a failed run (only those)."""
	for client in ("orig", "patella"):
		path = os.path.join(ct.run_dir(client), CLIENTS[client]["exe"]).lower()
		result = subprocess.run(["powershell", "-NoProfile", "-Command",
			"Get-Process | Where-Object { $_.Path -and $_.Path.ToLower() -eq '%s' } | Stop-Process -Force" % path],
			capture_output=True, text=True)
		if result.returncode != 0:
			log("could not stop %s: %s" % (path, result.stderr.strip()))


if __name__ == "__main__":
	sys.exit(main())
