r"""Compare the rendering of Patella.exe and the original Tibia.exe (issue #20).

Both clients play the same scenes with each graphics engine (DirectX 5: SurfaceDX.cpp, DirectX 9:
surfaceDX9.cpp, OpenGL: SurfaceGL.cpp); the frame each client composes is read back just before
it is shown and the pictures are compared pixel by pixel. notes/rendering.md describes the
method and the results; notes/decisions.md "Rendering comparison (issue #20)" the choices.

Usage:
  python -I tools/build.py oracle-x86
  python -I tools/compare_rendering.py [--out build\render\result] [--engines dx5,dx9,gl]
	[--modes windowed,fullscreen] [--no-repeat] [--recording build\render\session.jsonl] [--compare-only]
	[--patella build\windows-x64\Release\Patella.exe] [--tolerance N]

--engines vulkan,vulkan-cpu (Patella-Modern issue #19): Patella.exe (the x64 client, whose engine 2 is
the Vulkan engine) runs it on a GPU (PATELLA_VULKAN=gpu) or on the CPU driver next to it
(PATELLA_VULKAN=cpu), Tibia.exe its DirectX 9 engine, the reference (plan D7); both with Engine = 2.
The run checks that Patella.exe's frames came from the Vulkan engine on that kind of device. With
--engines dx9 the x64 Patella.exe gets PATELLA_VULKAN=off, so its engine 2 is surfaceDX9.cpp again
(until issue #20 removes it).
--region-tolerance N (default 2, the user's decision of November 3, 2026): on the pixels the Vulkan
engine marks for each captured frame (those whose value comes from the device's bilinear filter or the
light's interpolation: the stretches and the lit map, notes/vulkan.md "Pixels"; stored as
<scene>.filtered.png, white where marked), a pixel whose colour channels differ by at most N counts as
equal; everywhere else the comparison stays exact.
--tolerance N: the same everywhere (default 0). The report gives every scene's largest channel
difference either way.

--patella: the Patella.exe to compare, x86 or x64 (Patella-Modern issue #18), with its PDB next to it
(default the oracle's, build\oracle-x86\Release\Patella.exe); an x64 one: python -I tools/build.py
windows-x64 first.

The capture (tools/capture/patcap.c) is a DLL of the client's bitness (cmake/Capture.cmake):
build\oracle-x86\Release\patcap.dll for the 32-bit clients (Tibia.exe, the oracle's Patella.exe),
build\windows-x64\Release\patcap.dll for the x64 Patella.exe; each is its preset's patcap target, which
this script builds when the DLL is missing or older than its source. The run folders' copies of the
clients import it (add_import, PE32 and PE32+), so it is loaded before the client's entry point. It
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
import build  # noqa: E402

ROOT = ct.ROOT
RENDER = os.path.join(ROOT, "build", "render")
# patcap.dll has the bitness of the client it is loaded into (cmake/Capture.cmake): the patcap target
# of the oracle-x86 preset for the 32-bit clients (Tibia.exe, the oracle's Patella.exe), of the
# windows-x64 preset for the x64 Patella.exe (issue #18). By the client's PE machine field:
CAPTURE_PRESETS = {client_state.IMAGE_FILE_MACHINE_I386: "oracle-x86",
	client_state.IMAGE_FILE_MACHINE_AMD64: "windows-x64"}


def capture_preset(exe):
	"""The preset whose patcap.dll fits the client `exe`."""
	machine = client_state.pe_machine(exe)
	if machine not in CAPTURE_PRESETS:
		raise RunError("%s: machine 0x%x, neither x86 nor x64" % (exe, machine))
	return CAPTURE_PRESETS[machine]


def capture_dll(preset):
	return os.path.join(ROOT, "build", preset, "Release", "patcap.dll")
CLIENTS = ct.CLIENTS
RunError = ct.RunError
log = ct.log

# patcap.c struct Shared
CMD_ADVANCE, CMD_CAPTURE, CMD_REALTIME = 1, 2, 3
API_NAMES = {0: "none", 1: "DirectDraw Blt", 2: "DirectDraw Flip", 3: "Direct3D 9 Present", 4: "OpenGL SwapBuffers",
	5: "Vulkan"}
OFF = {"Magic": 0, "Version": 4, "CmdSeq": 8, "DoneSeq": 12, "Cmd": 16, "Arg0": 20, "Arg1": 24, "Result": 28,
	"Frames": 32, "Api": 36, "Now": 40, "Ticks": 44, "TimerWindow": 48, "TimerActive": 52, "Width": 56,
	"Height": 60, "SourceBits": 64, "Hooks": 68, "Path": 72, "Message": 592, "ApiFrames": 848, "Realtime": 868,
	"FirstCall": 872, "VulkanFrames": 876, "VulkanType": 880, "VulkanCpuDriver": 884, "VulkanDevice": 888,
	"MaskPixels": 1016}
# VkPhysicalDeviceType
VULKAN_TYPES = {0: "other", 1: "integrated GPU", 2: "discrete GPU", 3: "virtual GPU", 4: "CPU"}
# The CPU driver next to the x64 Patella.exe (cmake/Vulkan.cmake), copied into its run folder.
CPU_DRIVER_FILES = ("vulkan_lvp.dll", "lvp_icd.x86_64.json")
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

	def vulkan_device(self):
		"""The Vulkan engine's device as the engine reported it (None before, or for another engine)."""
		if self.get("Version") < 2 or not self.text("VulkanDevice", 128):
			return None
		return {"name": self.text("VulkanDevice", 128), "type": VULKAN_TYPES.get(self.get("VulkanType"), "?"),
			"cpu_driver": bool(self.get("VulkanCpuDriver"))}


def build_capture(preset):
	"""Builds the patcap target of `preset` when its DLL is missing or older than patcap.c."""
	dll = capture_dll(preset)
	source = os.path.join(ROOT, "tools", "capture", "patcap.c")
	if os.path.isfile(dll) and os.path.getmtime(dll) >= os.path.getmtime(source):
		return
	log("building the capture helper (the patcap target of the %s preset)" % preset)
	try:
		env = build.environment(preset)
	except build.BuildError as e:
		raise RunError("cannot build patcap.dll: %s" % e)
	commands = [["cmake", "--build", "--preset", preset, "--target", "patcap"]]
	if not os.path.isfile(os.path.join(ROOT, "build", preset, "CMakeCache.txt")):
		commands.insert(0, ["cmake", "--preset", preset])
	for command in commands:
		result = subprocess.run(command, capture_output=True, text=True, env=env, cwd=ROOT)
		if result.returncode != 0:
			raise RunError("%s failed:\n%s%s" % (" ".join(command), result.stdout, result.stderr))


# --- the import of patcap.dll ------------------------------------------------------------

def add_import(source, target, dll="patcap.dll", function="PatcapLoaded"):
	"""Copies the PE image `source` (PE32 or PE32+, x86 or x64) to `target` with one more import
	descriptor: `dll`, by name `function`. A new last section (.patcap, initialised read/write
	data) holds the new import directory (the old descriptors, unchanged, then the new one), its
	lookup and address tables (thunks of the image's pointer size) and the names; the import data directory points at it, the bound import directory (stale once
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
	magic, = struct.unpack_from("<H", data, opt)
	if magic not in (0x10b, 0x20b):
		raise RunError("%s: neither PE32 nor PE32+ (optional header magic 0x%x)" % (source, magic))
	thunk = 4 if magic == 0x10b else 8  # PE32: 32-bit thunks; PE32+ (x64): 64-bit thunks
	section_align, file_align = struct.unpack_from("<II", data, opt + 32)
	size_of_image, size_of_headers = struct.unpack_from("<II", data, opt + 56)
	dirs = opt + (96 if magic == 0x10b else 112)  # PE32+ has a 64-bit ImageBase and 64-bit stack/heap sizes
	if struct.unpack_from("<I", data, dirs - 4)[0] < 12:
		raise RunError("%s: fewer than 12 data directories" % source)
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
	# layout of the new section: the descriptors and a zero one, then the lookup and address tables
	# (one thunk and a zero one each, thunk-aligned), the hint/name entry and the DLL's name
	directory = 0
	lookup = (20 * (len(old) + 2) + thunk - 1) // thunk * thunk
	address = lookup + 2 * thunk
	hint_name = address + 2 * thunk
	dll_name = hint_name + 2 + len(function) + 1
	dll_name += dll_name & 1
	end = dll_name + len(dll) + 1
	body = bytearray(end)
	for i, descriptor in enumerate(old):
		body[20 * i:20 * i + 20] = descriptor
	struct.pack_into("<IIIII", body, 20 * len(old), va + lookup, 0, 0, va + dll_name, va + address)
	thunk_format = "<I" if thunk == 4 else "<Q"  # an RVA of a hint/name entry (not by ordinal)
	struct.pack_into(thunk_format, body, lookup, va + hint_name)
	struct.pack_into(thunk_format, body, address, va + hint_name)
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

	def __init__(self, *args, env=None, **kwargs):
		super().__init__(*args, **kwargs)
		self.env = env

	def start(self, arguments=()):
		path = os.path.join(self.run_dir, self.exe)
		self.proc = subprocess.Popen([path] + list(arguments), cwd=self.run_dir, env=self.env)
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


def diff_pictures(a, b, masks=(), tolerance=0, filtered=None, region_tolerance=0):
	"""Differing pixels of two pictures: (count, bounding box (x0, y0, x1, y1) or None, the
	differing pixels inside the masks, which are not counted, the largest channel difference
	outside the masks, the pixels that differ by at most `tolerance` per channel, or by at most
	`region_tolerance` where the picture `filtered` (the Vulkan engine's mask) is not black, which
	are not counted either)."""
	wa, ha, ra = a
	wb, hb, rb = b
	if (wa, ha) != (wb, hb):
		return wa * ha, (0, 0, max(wa, wb), max(ha, hb)), 0, 255, 0
	count, box, hidden, largest, within = 0, None, 0, 0, 0
	for y in range(ha):
		if ra[y] == rb[y]:
			continue
		for x in range(wa):
			if ra[y][3 * x:3 * x + 3] != rb[y][3 * x:3 * x + 3]:
				if masked(x, y, masks):
					hidden += 1
					continue
				channel = max(abs(ra[y][3 * x + i] - rb[y][3 * x + i]) for i in range(3))
				largest = max(largest, channel)
				if channel <= tolerance or (channel <= region_tolerance and filtered is not None
						and filtered[2][y][3 * x] != 0):
					within += 1
					continue
				count += 1
				box = (x, y, x + 1, y + 1) if box is None else (min(box[0], x), min(box[1], y), max(box[2], x + 1),
					max(box[3], y + 1))
	return count, box, hidden, largest, within


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
	# The Vulkan engine in Patella.exe (PATELLA_VULKAN, Surface.cpp; issue #19) against Tibia.exe's
	# DirectX 9 frames (plan D7); "vulkan" on a GPU, "vulkan-cpu" on the shipped CPU driver.
	"vulkan": {"engine": 2, "api": "Direct3D 9 Present", "patella_api": "Vulkan", "vulkan": "gpu",
		"source": "Patella.exe: src/vulkan/SurfaceVulkan.cpp on a GPU; Tibia.exe: surfaceDX9.cpp"},
	"vulkan-cpu": {"engine": 2, "api": "Direct3D 9 Present", "patella_api": "Vulkan", "vulkan": "cpu",
		"source": "Patella.exe: src/vulkan/SurfaceVulkan.cpp on the CPU driver; Tibia.exe: surfaceDX9.cpp"},
}


def client_env(engine, client):
	"""The environment of a run: Patella.exe gets PATELLA_VULKAN, the device for a Vulkan engine and
	"off" otherwise (the x64 client's engine 2 is then surfaceDX9.cpp; the oracle's ignores it);
	Tibia.exe never gets it."""
	env = dict(os.environ)
	env.pop("PATELLA_VULKAN", None)
	if client == "patella":
		env["PATELLA_VULKAN"] = ENGINES[engine].get("vulkan", "off")
	return env

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


RUNS = ("orig", "patella", "orig-repeat")  # run folders; the second Tibia.exe run has its own (issue #12)


def setup():
	"""Run folders build\\render\\orig, build\\render\\patella and build\\render\\orig-repeat:
	compare_traffic's set-up (copies of the data files, the clients patched by patch_client.py),
	then the import of patcap.dll, the one of the client's bitness."""
	ct.set_folder(RENDER)
	ct.setup(extra=["orig-repeat"])
	for run in RUNS:
		exe = os.path.join(ct.run_dir(run), CLIENTS[ct.client_of(run)]["exe"])
		preset = capture_preset(exe)
		build_capture(preset)
		count = add_import(exe, exe)
		shutil.copyfile(capture_dll(preset), os.path.join(ct.run_dir(run), "patcap.dll"))
		log("%s: patcap.dll of %s added to its %d imports" % (exe, preset, count))
		# The Vulkan engine's CPU driver, when the client under test has one next to it.
		source_dir = os.path.dirname(CLIENTS[ct.client_of(run)]["source"])
		for name in CPU_DRIVER_FILES:
			if os.path.isfile(os.path.join(source_dir, name)):
				shutil.copyfile(os.path.join(source_dir, name), os.path.join(ct.run_dir(run), name))


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
	(out\\<engine>-fullscreen\\<label> in fullscreen mode). The client runs in the run folder named
	label (orig, patella, orig-repeat), emptied first (compare_traffic.clean_run_dir: the automap
	files and error.txt of an earlier run go). Returns {"scenes": {name: digest}, "facts": {...},
	"problems": [...]}."""
	label = label or client
	if ct.client_of(label) != client:
		raise RunError("run folder %s is not %s's" % (label, client))
	folder = os.path.join(out, engine + ("-fullscreen" if fullscreen else ""), label)
	if os.path.isdir(folder):
		shutil.rmtree(folder)
	os.makedirs(folder)
	log("=== %s, %s (%s)" % (engine, label, CLIENTS[client]["exe"]))
	ct.clean_run_dir(label, keep=("patcap.dll",) + CPU_DRIVER_FILES)
	with open(os.path.join(ct.run_dir(label), "Tibia.cfg"), "w", encoding="ascii", newline="\r\n") as f:
		f.write(cfg_for(engine, fullscreen))
	mode_before = display_mode()
	info = CLIENTS[client]
	driver = Driver(ct.run_dir(label), info["exe"], info["pdb"], env=client_env(engine, client))
	scenes, apis, scene_masks, filtered = {}, set(), {}, {}
	proxy = px.Proxy(os.path.join(folder, "replay.jsonl"), px.load_private_key(ct.PEM),
		replay=px.load_events(recording), log=log)

	def shot(name, masks=None):
		if masks:
			scene_masks[name] = masks
		bmp = os.path.join(folder, "capture.bmp")
		width, height, api = driver.capture(bmp, SKIP_FRAMES)
		apis.add(api)
		if os.path.isfile(bmp + ".mask"):
			# The Vulkan engine's device-filtered pixels (patcap's PatcapVulkanMask), as a picture.
			with open(bmp + ".mask", "rb") as f:
				mask = f.read()
			os.remove(bmp + ".mask")
			if len(mask) == width * height:
				write_png(os.path.join(folder, filtered_file(name)), width, height,
					[bytes(255 if v else 0 for v in mask[y * width:(y + 1) * width] for _ in range(3))
						for y in range(height)])
				filtered[name] = sum(1 for v in mask if v)
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
				"virtual_ms": driver.cap.get("Now"), "hooks": driver.cap.hooks(),
				"vulkan_device": driver.cap.vulkan_device()}
		driver.kill()
	problems = proxy.mismatches + proxy.errors
	api = "DirectDraw Flip" if fullscreen and engine == "dx5" else ENGINES[engine]["api"]
	if client == "patella" and "patella_api" in ENGINES[engine]:
		api = ENGINES[engine]["patella_api"]
	if apis != {api}:
		problems.append("the frames came from %s, not %s (the client chose another engine or mode)"
			% (sorted(apis), api))
	if client == "patella" and "vulkan" in ENGINES[engine]:
		device = (facts_cap or {}).get("vulkan_device")
		want_cpu = ENGINES[engine]["vulkan"] == "cpu"
		if device is None:
			problems.append("the Vulkan engine reported no device")
		elif device["cpu_driver"] != want_cpu or (device["type"] == "CPU") != want_cpu:
			problems.append("the Vulkan engine ran on %s (%s), not on %s" % (device["name"], device["type"],
				"the CPU driver" if want_cpu else "a GPU"))
	mode_after = display_mode()
	if mode_after != mode_before:
		problems.append("the display mode changed from %s to %s" % (mode_before, mode_after))
	result = {"scenes": scenes, "masks": scene_masks, "filtered_pixels": filtered, "facts": dict(facts, **(facts_cap or {})),
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


def filtered_file(name):
	return name.replace(" ", "-") + ".filtered.png"


def compare_runs(out, engine, a, b, write_diffs=True, scenes=None, tolerance=0, region_tolerance=0):
	"""Scene by scene: (name, None when identical, "missing", ("masks", masks of a, masks of b)
	when the runs masked different areas, else (count, box, diff file, masked pixels that
	differ, masks, the largest channel difference, pixels within the tolerance)); count 0 means
	identical outside the masks (up to the tolerance)."""
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
		mask_file = os.path.join(out, engine, b, filtered_file(name))
		filtered = read_png(mask_file) if os.path.isfile(mask_file) else None
		count, box, hidden, largest, within = diff_pictures(picture_a, picture_b, masks, tolerance, filtered,
			region_tolerance)
		diff = None
		if write_diffs:
			diff = os.path.join(out, engine, "diff-%s-%s" % (b, scene_file(name)))
			write_diff(diff, picture_a, picture_b)
		rows.append((name, (count, box, diff, hidden, masks, largest, within)))
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
	parser.add_argument("--tolerance", type=int, default=0, help="a pixel whose channels differ by at most this "
		"much counts as equal (default 0: exact)")
	parser.add_argument("--region-tolerance", type=int, default=2, help="the same on the device-filtered "
		"pixels the Vulkan engine marks (default 2: the bilinear stretches and the light)")
	parser.add_argument("--patella", metavar="EXE", help="the Patella.exe to test, x86 or x64, with its PDB next to "
		"it (default build/oracle-x86/Release/Patella.exe; e.g. build/windows-x64/Release/Patella.exe; as "
		"tools/compare_traffic.py's --patella)")
	args = parser.parse_args()
	if args.patella:
		CLIENTS["patella"]["source"] = CLIENTS["patella"]["pdb"] = os.path.abspath(args.patella)
		if not os.path.isfile(CLIENTS["patella"]["source"]):
			raise SystemExit("no %s" % CLIENTS["patella"]["source"])
	args.out = os.path.abspath(args.out)  # the DLL writes the pictures from the client's folder
	args.recording = os.path.abspath(args.recording)
	engines = [e for e in args.engines.split(",") if e]
	for e in engines:
		if e not in ENGINES:
			raise SystemExit("unknown engine %r (%s)" % (e, ", ".join(ENGINES)))
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
	patella = CLIENTS["patella"]["source"]
	say("patella: %s (%s)" % (patella, capture_preset(patella) if os.path.isfile(patella) else "missing"))
	if args.tolerance:
		say("tolerance: pixels whose channels differ by at most %d count as equal" % args.tolerance)
	say("region tolerance: on the Vulkan engine's device-filtered pixels (<scene>.filtered.png), pixels whose "
		"channels differ by at most %d count as equal" % args.region_tolerance)
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
					scenes=FULLSCREEN_SCENES if fullscreen else SCENES, tolerance=args.tolerance,
					region_tolerance=args.region_tolerance):
				if result is None:
					say("    %-18s identical" % name)
				elif result == "missing":
					say("    %-18s missing" % name)
					differences += 1
				elif result[0] == "masks":
					say("    %-18s the runs masked different areas: %s, %s" % (name, result[1], result[2]))
					differences += 1
				else:
					count, box, diff, hidden, masks, largest, within = result
					spread = "largest channel difference %d" % largest
					if within:
						spread += ", %d pixels within the tolerance" % within
					if count == 0 and largest == 0 and hidden == 0:
						say("    %-18s identical" % name)
						continue
					if count == 0:
						say("    %-18s identical outside the mask %s (%d pixels differ inside it; %s); %s"
							% (name, masks, hidden, spread, os.path.relpath(diff, args.out)))
					else:
						say("    %-18s %d pixels differ, in %s (%d more inside the mask %s; %s); %s"
							% (name, count, box, hidden, masks, spread, os.path.relpath(diff, args.out)))
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
	for run in RUNS:
		path = os.path.join(ct.run_dir(run), CLIENTS[ct.client_of(run)]["exe"]).lower()
		result = subprocess.run(["powershell", "-NoProfile", "-Command",
			"Get-Process | Where-Object { $_.Path -and $_.Path.ToLower() -eq '%s' } | Stop-Process -Force" % path],
			capture_output=True, text=True)
		if result.returncode != 0:
			log("could not stop %s: %s" % (path, result.stderr.strip()))


if __name__ == "__main__":
	sys.exit(main())
