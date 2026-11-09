r"""Compare the rendering of Patella.exe and the original Tibia.exe (issue #20).

Both clients play the same scenes, Patella.exe (the x64 client) with its Vulkan engine against Tibia.exe's
DirectX 9; the frame each client composes is read back just before it is shown and the pictures are
compared pixel by pixel. Until Patella-Modern issue #94 the 32-bit oracle's Patella.exe was compared with each of
the original engines too (DirectX 5, DirectX 9, OpenGL: dx5, dx9, gl); they left the fork with the oracle.
notes/rendering.md describes the method and the results; notes/decisions.md "Rendering comparison (issue #20)" the
choices.

Usage:
  python -I tools/build.py windows-x64
  python -I tools/compare_rendering.py [--out build\render\result] [--engines vulkan,vulkan-cpu]
  python -I tools/compare_rendering.py record|compare [tools/reference.py's options]   (issue #23: the
      reference recordings of this tool's sessions; notes/testmode.md, "Reference recordings")
	[--modes windowed,fullscreen] [--no-repeat] [--recording build\render\session.jsonl] [--compare-only]
	[--patella build\windows-x64\Release\Patella.exe] [--tolerance N]

--engines vulkan,vulkan-cpu (Patella-Modern issue #19): Patella.exe (the x64 client, whose only engine
is the Vulkan engine since issue #20) runs it on a GPU (PATELLA_VULKAN=gpu) or on the CPU driver next to
it (PATELLA_VULKAN=cpu), Tibia.exe its DirectX 9 engine, the reference (plan D7); both with Engine = 2.
The run checks that Patella.exe's frames came from the Vulkan engine on that kind of device. The default
--engines is both.
--region-tolerance N (default 2, the user's decision of November 3, 2026): on the pixels the Vulkan
engine marks for each captured frame (those whose value comes from the device's bilinear filter or the
light's interpolation: the stretches and the lit map, notes/vulkan.md "Pixels"; stored as
<scene>.filtered.png, white where marked), a pixel whose colour channels differ by at most N counts as
equal; everywhere else the comparison stays exact.
--tolerance N: the same everywhere (default 0). The report gives every scene's largest channel
difference either way.

--patella: the x64 Patella.exe to compare (Patella-Modern issue #18; default build\windows-x64\Release\Patella.exe,
python -I tools/build.py windows-x64 first).

Tibia.exe runs under the capture DLL (tools/capture/patcap.c, cmake/Capture.cmake:
build\capture-x86\Release\patcap.dll, the capture-x86 preset's patcap target, which this script builds when the DLL
is missing or older than its source). The run folders' copies of Tibia.exe import it (add_import), so it is loaded before the client's
entry point. It reads the composed frame from the engine's own surfaces (DirectDraw Blt/Flip, Direct3D 9
Present, SwapBuffers), so no display is needed, and it makes time deterministic: the clock (timeGetTime,
the wall clock) and the main timer's ticks (WM_TIMER, ActionNumber) move only when this script says so,
by the same amounts in both clients.

The x64 client is never hooked (Patella-Modern issue #92): its test build next to it, PatellaTest.exe,
runs in its place with the same time course from its own test mode (tools/testmode_client.py,
TestDriver): the script `clock manual` and `cursor 1020 760` (patcap's), then through its control
channel every window message as a post line (it drops Windows' input), patcap's ADVANCE as advance
lines and each capture as a frame line (the frame dump: the next frame the Vulkan engine shows, and its
device-filtered pixels, the forms of patcap's picture and mask); the probe names the engine's device and
counts its frames. The game scenes come from a recorded session replayed by tools/proxy.py (both
clients get the same server bytes); no server is needed. Since issue #145 its windowed runs are headless
(--test-headless: the window never shown, the Vulkan engine without a surface, the frames read back as before);
its fullscreen runs and --windowed show the window. Tibia.exe's runs always need the desktop.

The display stays on while the tool runs (Patella-Modern issue #98): SetThreadExecutionState with
ES_DISPLAY_REQUIRED, as a video player does; the posted window messages are not user input, so without it
the display's idle timeout (Windows' "turn off the display after", 15 minutes here) ran out during the
runs, and with the display off Tibia.exe's Direct3D 9 device was not created (no frame: the first capture
timed out) and the x64 client's window lost its size (0x0 frames, the trade partner not found). Tibia.exe's
run waits for its first frame (patcap.dll's Present) before the first scene. A run that fails says why:
the window's state (visible, minimized, the foreground), the display's (on or off, the user's idle time
against the timeout, the input desktop: locked or not, the monitors) and patcap.dll's Direct3D 9 course
(the device's creation and its result, TestCooperativeLevel, Present), and the run folder's patcap.log
(the DLL's events) is kept as <out>\<engine>\orig\patcap.log. notes/rendering.md, "When a run fails".

--self-test checks the offsets of patcap.dll's shared memory against tools/capture/patcap.c and the
diagnosis on made-up data (no client, no display).

Exit code 0 when every scene of every engine is pixel-identical outside the masks (the measured frame
rate in the advanced graphics options, notes/rendering.md; for vulkan and vulkan-cpu also the graphics
device choice there, Patella-Modern issue #31: device_choice_mask), 1 on a difference, 2 when a run
failed.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import mmap
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import traceback
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import desktop  # noqa: E402
import compare_traffic as ct  # noqa: E402
import proxy as px  # noqa: E402
import session_client as sc  # noqa: E402
import login_client as lc  # noqa: E402
import testmode_client as tc  # noqa: E402
import build  # noqa: E402
import sandbox  # noqa: E402

ROOT = ct.ROOT
RENDER = os.path.join(ROOT, "build", "render")
IMAGE_FILE_MACHINE_I386 = 0x014c
# The kind of a client by its PE machine field: the 32-bit one (Tibia.exe; the oracle's Patella.exe until issue
# #94) runs under the capture-x86 preset's patcap.dll; the x64 client is read through its test mode (issue #92).
CLIENT_PRESETS = {IMAGE_FILE_MACHINE_I386: "capture-x86", ct.IMAGE_FILE_MACHINE_AMD64: "windows-x64"}


def client_preset(exe):
	"""The preset whose client `exe` is: capture-x86 (32-bit: Tibia.exe) or windows-x64."""
	machine = ct.pe_machine(exe)
	if machine not in CLIENT_PRESETS:
		raise RunError("%s: machine 0x%x, neither x86 nor x64" % (exe, machine))
	return CLIENT_PRESETS[machine]


def capture_preset(exe):
	"""The preset whose patcap.dll fits the client `exe`: capture-x86's. patcap.dll is Tibia.exe's only: the x64
	client is never hooked (issue #92), so an x64 image is refused."""
	preset = client_preset(exe)
	if preset != "capture-x86":
		raise RunError("%s is an x64 client: patcap.dll is Tibia.exe's only; "
			"the x64 client is read through its test build's control channel (issue #92)" % exe)
	return preset


def capture_dll(preset):
	return os.path.join(ROOT, "build", preset, "Release", "patcap.dll")
CLIENTS = ct.CLIENTS
RunError = ct.RunError
log = ct.log

# patcap.c struct Shared, version 5 (issue #98: Direct3D 9's course where versions 2 and 3 had the Vulkan
# engine's fields, which left with the DLL's Vulkan exports; --self-test compares the offsets with patcap.c)
PATCAP_VERSION = 5  # 5: issue #134's profile windows (the commands below, the heap hooks), no new field
CMD_ADVANCE, CMD_CAPTURE, CMD_REALTIME, CMD_PROFILE_START, CMD_PROFILE_STOP = 1, 2, 3, 4, 5
PROFILE_ALLOCS = 1
API_NAMES = {0: "none", 1: "DirectDraw Blt", 2: "DirectDraw Flip", 3: "Direct3D 9 Present", 4: "OpenGL SwapBuffers"}
OFF = {"Magic": 0, "Version": 4, "CmdSeq": 8, "DoneSeq": 12, "Cmd": 16, "Arg0": 20, "Arg1": 24, "Result": 28,
	"Frames": 32, "Api": 36, "Now": 40, "Ticks": 44, "TimerWindow": 48, "TimerActive": 52, "Width": 56,
	"Height": 60, "SourceBits": 64, "Hooks": 68, "Path": 72, "Message": 592, "ApiFrames": 848, "Realtime": 868,
	"FirstCall": 872, "Direct3DCreates": 876, "DeviceCreates": 880, "DeviceResult": 884, "DeviceTime": 888,
	"DeviceWidth": 892, "DeviceHeight": 896, "DeviceWindow": 900, "CooperativeLevel": 904, "LostChecks": 908,
	"PresentResult": 912, "PresentFailures": 916, "FirstPresentTime": 920, "LastPresentTime": 924, "LoadTick": 928,
	"DeviceInside": 932, "DeviceCallTime": 936}
# HRESULTs of Direct3D 9 that patcap.dll reports (d3d9.h); 1: the call has not been made yet.
HRESULTS = {0: "D3D_OK", 1: "not called yet", 0x88760868: "D3DERR_DEVICELOST", 0x88760869: "D3DERR_DEVICENOTRESET",
	0x8876086a: "D3DERR_NOTAVAILABLE", 0x8876086c: "D3DERR_INVALIDCALL", 0x8876017c: "D3DERR_OUTOFVIDEOMEMORY",
	0x88760870: "D3DERR_DEVICEREMOVED", 0x88760874: "D3DERR_DEVICEHUNG", 0x08760873: "S_PRESENT_OCCLUDED",
	0x80004005: "E_FAIL", 0x8007000e: "E_OUTOFMEMORY"}
# Issue #161: the start the hook missed (patcap.dll's stale vtable entry, fixed there) was the failure of 4 of 8 first starts.
# How long Tibia.exe may take from its window to its first frame (patcap.dll's first Present): the device
# is made at start-up; normally the first frame comes within a second.
FIRST_FRAME_TIMEOUT = 30
# Issue #98: the driver's CreateDevice did not return once in a while at the first start after the GPU was idle; the
# next start worked. session_client.START_ATTEMPTS starts (issue #148: every failed start) before a run fails.


class FirstFrameError(RunError):
	"""Tibia.exe showed no frame after its start (Driver.start)."""
# VkPhysicalDeviceType
VULKAN_TYPES = {0: "other", 1: "integrated GPU", 2: "discrete GPU", 3: "virtual GPU", 4: "CPU"}
# The CPU driver next to the x64 Patella.exe (cmake/Vulkan.cmake), copied into its run folder.
CPU_DRIVER_FILES = ("vulkan_lvp.dll", "lvp_icd.x86_64.json")
API_NAMES_INDEX = {v: k for k, v in API_NAMES.items()}
HOOK_NAMES = ["timeGetTime", "GetSystemTimeAsFileTime", "GetSystemTimePreciseAsFileTime", "GetProcAddress",
	"SetTimer", "KillTimer", "GetCursorPos", "PeekMessageA", "SwapBuffers", "wglMakeCurrent", "DirectDrawCreate",
	"HeapAlloc", "HeapReAlloc", "HeapFree"]


class Capture:
	"""The DLL's shared memory in one client."""

	def __init__(self, pid):
		self.pid = pid
		self.map = mmap.mmap(-1, 4096, tagname="Local\\PatellaCapture-%d" % pid)
		if self.get("Magic") != 0x50434150:  # 'PCAP' as the compiler stores the multi-character constant
			raise RunError("patcap.dll is not loaded in PID %d (magic 0x%x)" % (pid, self.get("Magic")))
		if self.get("Version") != PATCAP_VERSION:
			raise RunError("patcap.dll in PID %d has the shared memory's version %d, not %d: rebuild it (python -I "
				"tools/build.py capture-x86)" % (pid, self.get("Version"), PATCAP_VERSION))
		self.explain = None  # a function: the window's and the display's state, for a timeout's message

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
				raise RunError("command %d not done within %d s (frames %d)%s" % (cmd, timeout, self.get("Frames"),
					"; " + self.explain() if self.explain else ""))
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

	def d3d9(self):
		"""Direct3D 9's course as patcap.dll saw it (version 4, issue #98)."""
		names = ("Direct3DCreates", "DeviceCreates", "DeviceResult", "DeviceTime", "DeviceWidth", "DeviceHeight",
			"DeviceWindow", "CooperativeLevel", "LostChecks", "PresentResult", "PresentFailures", "FirstPresentTime",
			"LastPresentTime", "DeviceInside", "DeviceCallTime")
		facts = {n: self.get(n) for n in names}
		facts["Presents"] = self.get("ApiFrames", API_NAMES_INDEX["Direct3D 9 Present"])
		facts["Loaded"] = (ctypes.windll.kernel32.GetTickCount() - self.get("LoadTick")) & 0xffffffff
		return facts


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
	try:
		# the pinned CMake (issue #141), not the first one on PATH
		commands = [build.pinned_cmake(command) for command in commands]
	except build.BuildError as e:
		raise RunError("cannot build patcap.dll: %s" % e)
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


class TestDriver(sc.Driver):
	"""session_client's driver for the x64 test client (issue #92): patcap's time course from the test mode
	itself. The script it starts with is clock manual and cursor 1020 760 (patcap's CURSOR_X, CURSOR_Y);
	every message goes through the control channel (scripted: post lines), advance() is an advance line,
	frame() a frame line (the next frame the Vulkan engine shows, frame-NNN.png and .filtered.png)."""

	manual_clock = True  # clock manual: run_session checks a step's condition after its settle (issue #144)

	def __init__(self, run_dir, exe, script, env=None, headless=False, **kwargs):
		super().__init__(run_dir, exe, None, test=True, scripted=True, test_arguments=["--test-script", script]
			+ (["--test-headless"] if headless else []), **kwargs)
		self.env = env
		self.cap = None
		self.frames = 0

	def environment(self):
		return self.env

	def other_start(self):
		"""Issue #161: whether this start took the other course patcap.dll logged (the Direct3D 9 object's vtable back at
		its original slot before the device; seen in Windows Sandbox, at a Sandbox's first start and after a Vulkan run
		on the GPU, cause unknown). That start works and shows frames, but Tibia.exe then lists a 16-bit display mode that the ordinary
		start does not (graphics-options: 445 pixels), so the run takes one more start (session_client's retry). A client
		that always starts that way still fails after START_ATTEMPTS."""
		if not self.refuse_other_start:
			return False
		try:
			with open(os.path.join(self.run_dir, "patcap.log"), encoding="utf-8", errors="replace") as f:
				text = f.read()  # the file keeps earlier starts' lines: this process's only
			return "patching it again" in text[text.rfind("loaded in PID %d" % self.proc.pid):]
		except OSError:
			return False

	def advance(self, ms=2000, ticks=20):
		self.channel.command("advance %d %d" % (ms, ticks), alive=self.alive)

	def frame(self):
		"""The next frame shown: (picture file, mask file or None)."""
		self.channel.command("frame", alive=self.alive)
		self.frames += 1
		png = os.path.join(self.channel.out, "frame-%03d.png" % self.frames)
		mask = os.path.join(self.channel.out, "frame-%03d.filtered.png" % self.frames)
		if not os.path.isfile(png):
			raise RunError("the frame dump wrote no %s" % png)
		return png, mask if os.path.isfile(mask) else None

	def facts(self):
		"""patcap's facts from the probe: frames shown, ticks, virtual milliseconds, the Vulkan device."""
		probe = self.mem.refresh()
		device = probe.get("device")
		return {"frames": probe["frames"], "ticks": probe["ticks"], "virtual_ms": probe["time"],
			"hooks": ["none: the test mode's control channel"],
			"vulkan_device": None if device is None else {"name": device["name"],
				"type": VULKAN_TYPES.get(device["type"], "?"), "cpu_driver": bool(device["cpu_driver"])}}


# The start of TestDriver's script: patcap's clock and cursor (tools/capture/patcap.c CURSOR_X, CURSOR_Y).
TEST_START_SCRIPT = "# compare_rendering.py (issue #92): patcap's time course\nclock manual\ncursor 1020 760\n"


class Driver(sc.Driver):
	"""session_client's driver for a client whose copy imports patcap.dll (add_import): Tibia.exe (and the
	oracle's 32-bit Patella.exe, until issue #94)."""

	manual_clock = True  # patcap's clock: run_session checks a step's condition after its settle (issue #144)
	refuse_other_start = True  # other_start (issue #161); sandbox_trial.py counts those starts instead

	def __init__(self, *args, env=None, **kwargs):
		super().__init__(*args, **kwargs)
		self.env = env

	def environment(self):
		return self.env

	# Issue #98: a client that shows no first frame within FIRST_FRAME_TIMEOUT (its Direct3D 9 device not made: the
	# driver's CreateDevice did not return) is ended and started once more: nothing has been sent to it yet, so the
	# run is the same as with a first start that worked. Since issue #148 that is session_client's Driver.start, for
	# every failed start (start_once below), with the refused start's box and the other clients' windows said.

	def reset_start(self):
		super().reset_start()
		if getattr(self, "cap", None) is not None:
			self.cap.close()
			self.cap = None

	def start_once(self, arguments=()):
		path = os.path.join(self.run_dir, self.exe)
		self.proc = subprocess.Popen([path] + list(arguments), cwd=self.run_dir, env=self.env)
		self.log("started %s, PID %d" % (path, self.proc.pid))
		deadline = time.time() + 30
		self.cap = None
		while (self.hwnd is None or self.cap is None) and time.time() < deadline:
			if self.proc.poll() is not None:
				raise sc.SessionError("the client exited with code %d" % self.proc.returncode)
			self.check_refused()  # issue #148
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
		self.cap.explain = lambda: explain(self.hwnd, self.cap)
		self.log("patcap.dll loaded; hooks: %s" % ", ".join(self.cap.hooks()))
		# Issue #98: the first frame (patcap's Present) before the first command, not a fixed time; a client
		# whose Direct3D 9 device was not made never shows one, and that is said here, with why.
		deadline = time.time() + FIRST_FRAME_TIMEOUT
		while self.cap.get("Frames") == 0:
			if self.proc.poll() is not None:
				raise sc.SessionError("the client exited with code %d before its first frame" % self.proc.returncode)
			if time.time() > deadline:
				raise FirstFrameError("no frame within %d s of the start; %s" % (FIRST_FRAME_TIMEOUT,
					explain(self.hwnd, self.cap)))
			time.sleep(0.05)
		facts = self.cap.d3d9()
		self.log("first frame after %d ms (%s; CreateDevice %s at %d ms)" % (facts["FirstPresentTime"],
			API_NAMES.get(self.cap.get("Api"), "?"), hresult(facts["DeviceResult"]), facts["DeviceTime"]))
		if self.other_start():
			raise sc.SessionError("this start reset the Direct3D 9 object between its engine test and its device (patcap.log: patching it again); such a start lists another set of display modes, the comparison wants the ordinary one")
		time.sleep(2)
		self.mem = self.reader()

	def other_start(self):
		"""Issue #161: whether this start took the other course patcap.dll logged (the Direct3D 9 object's vtable back at
		its original slot before the device: in Windows Sandbox, the first Direct3D 9 start after the GPU was idle or used
		by another API). That start works and shows frames, but Tibia.exe then lists a 16-bit display mode that the ordinary
		start does not (graphics-options: 445 pixels), so the run takes one more start (session_client's retry). A client
		that always starts that way still fails after START_ATTEMPTS."""
		if not self.refuse_other_start:
			return False
		try:
			with open(os.path.join(self.run_dir, "patcap.log"), encoding="utf-8", errors="replace") as f:
				text = f.read()  # the file keeps earlier starts' lines: this process's only
			return "patching it again" in text[text.rfind("loaded in PID %d" % self.proc.pid):]
		except OSError:
			return False

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
# The original engines (dx5, dx9, gl) were compared on the 32-bit oracle's Patella.exe until Patella-Modern
# issue #94; they left the fork with it.
ENGINES = {
	# The Vulkan engine in Patella.exe (the x64 client's only engine since issue #20; issue #19) against
	# Tibia.exe's DirectX 9 frames (plan D7); "vulkan" on a GPU, "vulkan-cpu" on the shipped CPU driver.
	"vulkan": {"engine": 2, "api": "Direct3D 9 Present", "patella_api": "Vulkan", "vulkan": "gpu",
		"source": "Patella.exe: src/vulkan/SurfaceVulkan.cpp on a GPU; Tibia.exe: surfaceDX9.cpp"},
	"vulkan-cpu": {"engine": 2, "api": "Direct3D 9 Present", "patella_api": "Vulkan", "vulkan": "cpu",
		"source": "Patella.exe: src/vulkan/SurfaceVulkan.cpp on the CPU driver; Tibia.exe: surfaceDX9.cpp"},
}


# The engines Patella.exe has (client_preset): the x64 client only the Vulkan engine (issue #20).
PATELLA_ENGINES = {"windows-x64": ("vulkan", "vulkan-cpu")}


def client_env(engine, client):
	"""The environment of a run: Patella.exe gets PATELLA_VULKAN, the device, for a Vulkan engine;
	otherwise, and for Tibia.exe, it is removed."""
	env = dict(os.environ)
	env.pop("PATELLA_VULKAN", None)
	if client == "patella" and "vulkan" in ENGINES[engine]:
		env["PATELLA_VULKAN"] = ENGINES[engine]["vulkan"]
	return env

# Every step ends with this much virtual time and these many ticks of the main timer, the same
# in every run: animations, steps, effects and talk boxes are then at the same point.
ADVANCE_MS = 2000
ADVANCE_TICKS = 20
SKIP_FRAMES = 2  # frames drawn after the advance before the capture

# The look step of the replayed session (Patella-Modern issue #140): compare_traffic.py's (issue #137), whose
# orig-live.jsonl is the recording replayed here. main refuses a recording with another look step
# (session_client.check_recorded_look); tools/reference_record.py passes LOOK_NORTH, the frozen reference recordings'.
LOOK = sc.LOOK_OWN_FIELD


def scenes(look=LOOK, fullscreen=False):
	"""The scenes, in session order (the replayed session: tools/session_client.py run_session, its look step
	look); fullscreen: without the option dialogs."""
	names = ["start", "options", "graphics-options", "advanced-options", "login", "charlist", "game", "automap-zoom",
		"walk north", "walk east", "walk south", "walk west", "turn south", "say", look[0], "open container",
		"look in container", "use in container", "context menu", "trade with", "trade request", "look at offer",
		"cancel trade", "close container", "side windows", "logout"]
	if fullscreen:
		names = [n for n in names if n not in ("options", "graphics-options", "advanced-options")]
	return names


SCENES = scenes()
FULLSCREEN_SCENES = scenes(fullscreen=True)

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
	then, for the 32-bit clients, the import of patcap.dll (the x64 test client takes none, issue #92)."""
	ct.set_folder(RENDER)
	ct.setup(extra=["orig-repeat"])
	for run in RUNS:
		exe = os.path.join(ct.run_dir(run), CLIENTS[ct.client_of(run)]["exe"])
		if CLIENTS[ct.client_of(run)].get("test"):
			log("%s: the test build, driven through its control channel (no patcap.dll)" % exe)
		else:
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
	box = driver.mem.member(dialog["ptr"], "TGraphicsOptionsDialog.Fullscreen")
	node = driver.node_by_ptr(box)
	driver.click(node["x"] + 6, node["y"] + node["h"] // 2)
	time.sleep(0.3)
	if not driver.mem.checked(box):
		raise RunError("the Fullscreen check box did not change")
	driver.char(13)
	wait_gone(driver, "TGraphicsOptionsDialog")
	time.sleep(2)
	click_node(driver, button(driver, "Ok"))
	wait_gone(driver, "TOptionsDialog")
	time.sleep(1)


def session(driver, proxy, shot, fullscreen=False, device_choice=False, look=LOOK):
	"""The scenes: the start screen, the option dialogs, the login, then the replayed game session
	(session_client.run_session) with an automap zoom; shot(name) captures. fullscreen: switch to
	fullscreen mode first, then the same scenes without the option dialogs. device_choice: the advanced
	options' engine buttons and the line above them are masked too (device_choice_mask). look: run_session's
	look step (LOOK, compare_traffic.py's; tools/reference_record.py: LOOK_NORTH)."""
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
		masks = [framerate_mask(driver, dialog)]
		if device_choice:
			masks.append(device_choice_mask(driver, dialog))
		shot("advanced-options", masks=masks)
		for dialog in ("TAdvancedGraphicsDialog", "TGraphicsOptionsDialog", "TOptionsDialog"):
			texts = [n.get("text") for n in driver.mem.gui() if n["class"] == "TTextButton"]
			click_node(driver, button(driver, "Cancel" if "Cancel" in texts else "Ok"))
			wait_gone(driver, dialog)
			settle()

	# Enter Game, the login dialog (waited for, not a fixed 1 s: issue #144), account, Tab, password
	step("login", lambda: sc.enter_login(driver))

	def charlist():
		driver.char(13)
		proxy.wait_idle(0.5, 60)

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
		click_node(driver, driver.node_by_ptr(driver.mem.member(automap["ptr"], "TAutomapWindow.ZoomInButton")))

	step("automap-zoom", zoom_in)
	click_node(driver, driver.node_by_ptr(driver.mem.member(automap["ptr"],
		"TAutomapWindow.ZoomOutButton")))  # back to zoom 2
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

	sc.run_session(driver, session_wait, checkpoint, with_login=False, look=look)
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


# The recorded exception to tier A of Patella-Modern issue #31 (plan 6.3): in the x64 client the advanced
# options' three engine buttons are the graphics device choice ("Automatic", "GPU", "CPU", the selected
# one the device choice's) and the line above them names the device ("Device: <name>", Dialogs.cpp
# TAdvancedGraphicsDialog), where Tibia.exe shows "Select graphics engine:" and DirectX5, DirectX9,
# OpenGL. For the Vulkan engines that area is masked in both runs: from the label's corner
# (SkinProperties.Style1394.Data08) to the dialog's width (Data00) and the buttons' bottom (Data10.y plus
# the 20 pixels of the button skin 16). The rest of the dialog stays exact against Tibia.exe.
DEVICE_BUTTON_HEIGHT = 20


def device_choice_mask(driver, dialog):
	x = dialog["x"] + driver.mem.skin_int("Style1394.Data08.x")
	y = dialog["y"] + driver.mem.skin_int("Style1394.Data08.y")
	right = dialog["x"] + driver.mem.skin_int("Style1394.Data00")
	bottom = dialog["y"] + driver.mem.skin_int("Style1394.Data10.y") + DEVICE_BUTTON_HEIGHT
	return [x, y, right, bottom]


def run(engine, client, out, recording, label=None, fullscreen=False, headless=False):
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
	if info.get("test"):
		script = os.path.join(folder, "start.script")
		with open(script, "w", encoding="utf-8", newline="\n") as f:
			f.write(TEST_START_SCRIPT)
		driver = TestDriver(ct.run_dir(label), info["exe"], script, env=client_env(engine, client), headless=headless)
	else:
		driver = Driver(ct.run_dir(label), info["exe"], info["pdb"], env=client_env(engine, client))
	scenes, apis, scene_masks, filtered = {}, set(), {}, {}
	proxy = px.Proxy(os.path.join(folder, "replay.jsonl"), px.load_private_key(ct.PEM),
		replay=px.load_events(recording), log=log)

	def shot(name, masks=None):
		if masks:
			scene_masks[name] = masks
		if os.name == "nt" and ctypes.windll.user32.IsIconic(driver.hwnd):
			# Issue #98: a minimized window has no client area (Windows minimizes the windows of a monitor that
			# goes away: a DisplayPort monitor asleep); the scene is not taken, and the run says so.
			raise RunError("scene %s: the client's window is minimized; %s" % (name, explain(driver.hwnd,
				getattr(driver, "cap", None))))
		if isinstance(driver, TestDriver):
			# The test mode's frame dump: the frame and its device-filtered pixels (white), as patcap's.
			png, mask = driver.frame()
			picture = read_png(png)
			if picture[0] == 0 or picture[1] == 0:
				raise RunError("scene %s: the frame dump is %dx%d (the window has no client area); %s"
					% (name, picture[0], picture[1], explain(driver.hwnd)))
			apis.add("Vulkan")
			if mask is not None:
				shutil.copyfile(mask, os.path.join(folder, filtered_file(name)))
				filtered[name] = sum(row[0::3].count(255) for row in read_png(mask)[2])
			write_png(os.path.join(folder, scene_file(name)), *picture)
			scenes[name] = pixel_digest(picture[2])
			log("scene %s: %dx%d (the test mode's frame dump)" % (name, picture[0], picture[1]))
			return
		bmp = os.path.join(folder, "capture.bmp")
		width, height, api = driver.capture(bmp, SKIP_FRAMES)
		apis.add(api)
		picture = read_bmp(bmp)
		os.remove(bmp)
		write_png(os.path.join(folder, scene_file(name)), *picture)
		scenes[name] = pixel_digest(picture[2])
		log("scene %s: %dx%d (%s)" % (name, width, height, api))

	facts_cap = None
	log("display: %s" % display_summary())
	try:
		with proxy:
			try:
				driver.start()
				session(driver, proxy, shot, fullscreen, "vulkan" in ENGINES[engine])
			except Exception as e:
				# Issue #98: the window's and the display's state with the failure, once.
				if "user idle " not in str(e) and driver.hwnd is not None:
					raise RunError("%s; %s" % (e, explain(driver.hwnd, getattr(driver, "cap", None)))) from e
				raise
			time.sleep(0.5)
			if isinstance(driver, TestDriver):
				facts_cap = driver.facts()
			code = driver.close()
			if code != 0:
				raise RunError("%s exited with code %d" % (info["exe"], code))
		facts = {"apis": sorted(apis), "frames": driver.cap.get("Frames") if driver.cap else None}
		if isinstance(driver, TestDriver):
			facts["frames"] = facts_cap["frames"]
	finally:
		if getattr(driver, "cap", None) is not None:
			facts_cap = {"frames": driver.cap.get("Frames"), "ticks": driver.cap.get("Ticks"),
				"virtual_ms": driver.cap.get("Now"), "hooks": driver.cap.hooks(), "d3d9": driver.cap.d3d9()}
		driver.kill()
		patcap_log = os.path.join(ct.run_dir(label), "patcap.log")
		if os.path.isfile(patcap_log):
			shutil.copyfile(patcap_log, os.path.join(folder, "patcap.log"))
	problems = proxy.mismatches + proxy.errors
	api = ENGINES[engine]["api"]
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
	# The record/compare split (Patella-Modern issue #23): record or compare as the first word runs
	# tools/reference.py with this tool's sessions (notes/testmode.md, "Reference recordings").
	if len(sys.argv) > 1 and sys.argv[1] in ("record", "compare"):
		import reference
		return reference.main([sys.argv[1], "--sessions", ",".join(reference.tool_sessions("compare_rendering"))] + sys.argv[2:])
	parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
	parser.add_argument("--out", default=os.path.join(RENDER, "result"))
	parser.add_argument("--engines", help="the engines (%s); default: both" % ", ".join(ENGINES))
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
	parser.add_argument("--patella", metavar="EXE", help="the x64 Patella.exe to test (its test build PatellaTest.exe "
		"next to it runs, read through its test mode, issue #92); default build/windows-x64/Release/Patella.exe; as "
		"tools/compare_traffic.py's --patella")
	parser.add_argument("--windowed", action="store_true", help="show the x64 client's window in the windowed runs too "
		"(default: headless there, --test-headless, issue #145)")
	parser.add_argument(desktop.HIDDEN, action="store_true", help=desktop.HIDDEN_HELP)
	parser.add_argument(sandbox.DESKTOP, action="store_true", help=sandbox.DESKTOP_HELP)
	parser.add_argument("--self-test", action="store_true", help="check patcap.dll's shared-memory offsets against "
		"tools/capture/patcap.c and the failure diagnosis on made-up data; no client")
	args = parser.parse_args()
	if args.self_test:
		return self_test()
	args.out = os.path.abspath(args.out)  # the DLL writes the pictures from the client's folder
	args.recording = os.path.abspath(args.recording)
	patella_preset = None
	named = None
	patella_path = os.path.abspath(args.patella) if args.patella else ct.DEFAULT_PATELLA
	if args.patella and not os.path.isfile(patella_path):
		raise SystemExit("no %s" % patella_path)
	if os.path.isfile(patella_path):
		try:
			patella_preset = client_preset(patella_path)
			named = ct.use_patella(patella_path)
		except RunError as e:
			raise SystemExit(str(e))
	if args.engines is None:
		args.engines = ",".join(PATELLA_ENGINES["windows-x64"])
	engines = [e for e in args.engines.split(",") if e]
	for e in engines:
		if e not in ENGINES:
			raise SystemExit("unknown engine %r (%s)" % (e, ", ".join(ENGINES)))
		if not args.compare_only and patella_preset is not None and e not in PATELLA_ENGINES[patella_preset]:
			raise SystemExit("%s (%s) has no engine %s: it has %s (the x64 client only the Vulkan engine since "
				"issue #20)" % (CLIENTS["patella"]["source"], patella_preset, e,
					", ".join(PATELLA_ENGINES[patella_preset])))
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
	try:
		sc.check_recorded_look(px.load_events(args.recording), LOOK, args.recording)  # issue #140
	except sc.SessionError as e:
		raise SystemExit("the recording does not fit this session: %s" % e)
	os.makedirs(args.out, exist_ok=True)
	runs = {}
	failed = []
	if not args.compare_only:
		setup()
		keep_display_on(True)
		log("display: %s" % display_summary())
		for engine, fullscreen in passes:
			plan = [("orig", "orig"), ("patella", "patella")] + ([] if args.no_repeat else [("orig", "orig-repeat")])
			for client, label in plan:
				name = engine + (" fullscreen" if fullscreen else "")
				try:
					runs[(name, label)] = run(engine, client, args.out, args.recording, label, fullscreen,
						headless=not fullscreen and not args.windowed)
				except sc.StartError:  # issue #148: a client that cannot start ends the run
					keep_display_on(False)
					raise
				except Exception as e:
					traceback.print_exc()
					failed.append("%s %s: %s" % (name, label, e))
					kill_clients()
				if display_mode()[:3] != FULLSCREEN_MODE and "fullscreen" in modes:
					failed.append("the display mode is now %s: no more runs" % (display_mode(),))
					break
		keep_display_on(False)
	report = []
	say = report.append
	say("Rendering comparison (issue #20), %s" % time.strftime("%Y-%m-%d %H:%M"))
	say("recording %s (its look step: %s); every step: %d ms and %d ticks of virtual time, capture after %d frames"
		% (args.recording, LOOK[0], ADVANCE_MS, ADVANCE_TICKS, SKIP_FRAMES))
	patella = CLIENTS["patella"]["source"]
	say(named + " (%s)" % patella_preset if named else "patella: %s (missing)" % patella)
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


# --- the window and the display (issue #98) -------------------------------------------------

ES_CONTINUOUS = 0x80000000
ES_SYSTEM_REQUIRED = 0x00000001
ES_DISPLAY_REQUIRED = 0x00000002


def keep_display_on(on):
	"""While on: the display and the system stay on (SetThreadExecutionState, this thread; a display that the idle
	timeout had turned off is turned on again). The tool's input is posted window messages, which Windows does not
	count as the user's input. Off: back to the idle timeouts. Never input of its own: the user's mouse and keyboard
	are left alone."""
	flags = ES_CONTINUOUS | (ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED if on else 0)
	if os.name == "nt" and not ctypes.windll.kernel32.SetThreadExecutionState(flags):
		log("SetThreadExecutionState(0x%x) failed" % flags)


GUID_CONSOLE_DISPLAY_STATE = "6fe69556-704a-47a0-8f24-c28d936fda47"
GUID_VIDEO_SUBGROUP = "7516b95f-f776-4464-8c53-06167f40cc99"
GUID_VIDEO_POWERDOWN_TIMEOUT = "3c0bc021-c8a8-4e07-a973-6b14cbcb2b7e"


def console_display_state():
	"""The console display's power state: "on", "off", "dimmed" or "unknown" (RegisterPowerSettingNotification of
	GUID_CONSOLE_DISPLAY_STATE: Windows sends the current state at once to a message-only window)."""
	import uuid
	user32, kernel32 = ctypes.windll.user32, ctypes.windll.kernel32
	proc_type = ctypes.WINFUNCTYPE(ctypes.c_ssize_t, wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM)
	user32.DefWindowProcW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
	user32.DefWindowProcW.restype = ctypes.c_ssize_t
	user32.CreateWindowExW.restype = wt.HWND
	user32.CreateWindowExW.argtypes = [wt.DWORD, wt.LPCWSTR, wt.LPCWSTR, wt.DWORD, ctypes.c_int, ctypes.c_int,
		ctypes.c_int, ctypes.c_int, wt.HWND, wt.HMENU, wt.HINSTANCE, wt.LPVOID]
	user32.RegisterPowerSettingNotification.restype = ctypes.c_void_p
	user32.RegisterPowerSettingNotification.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD]
	user32.UnregisterPowerSettingNotification.argtypes = [ctypes.c_void_p]
	states = []

	def proc(hwnd, msg, wparam, lparam):
		if msg == 0x218 and wparam == 0x8013:  # WM_POWERBROADCAST, PBT_POWERSETTINGCHANGE
			data = ctypes.string_at(lparam, 24)  # POWERBROADCAST_SETTING: GUID, DataLength, Data
			states.append(struct.unpack_from("<I", data, 20)[0])
		return user32.DefWindowProcW(hwnd, msg, wparam, lparam)

	callback = proc_type(proc)

	class WNDCLASS(ctypes.Structure):
		_fields_ = [("style", wt.UINT), ("proc", proc_type), ("extra", ctypes.c_int * 2), ("instance", wt.HINSTANCE),
			("icon", wt.HICON), ("cursor", wt.HANDLE), ("brush", wt.HBRUSH), ("menu", wt.LPCWSTR), ("name", wt.LPCWSTR)]

	name = "compare_rendering_display_%d" % os.getpid()
	instance = kernel32.GetModuleHandleW(None)
	window_class = WNDCLASS(proc=callback, instance=instance, name=name)
	if not user32.RegisterClassW(ctypes.byref(window_class)):
		return "unknown"
	hwnd = user32.CreateWindowExW(0, name, "", 0, 0, 0, 0, 0, wt.HWND(-3), None, instance, None)  # HWND_MESSAGE
	try:
		if not hwnd:
			return "unknown"
		guid = uuid.UUID(GUID_CONSOLE_DISPLAY_STATE).bytes_le
		handle = user32.RegisterPowerSettingNotification(hwnd, guid, 0)
		message = wt.MSG()
		for _ in range(50):
			while user32.PeekMessageW(ctypes.byref(message), hwnd, 0, 0, 1):
				user32.DispatchMessageW(ctypes.byref(message))
			if states:
				break
			time.sleep(0.01)
		if handle:
			user32.UnregisterPowerSettingNotification(handle)
	finally:
		if hwnd:
			user32.DestroyWindow(hwnd)
		user32.UnregisterClassW(name, instance)
	return {0: "off", 1: "on", 2: "dimmed"}.get(states[0], str(states[0])) if states else "unknown"


def idle_seconds():
	"""Seconds since the user's last input (GetLastInputInfo; posted messages are not input)."""
	class LASTINPUTINFO(ctypes.Structure):
		_fields_ = [("cbSize", wt.UINT), ("dwTime", wt.DWORD)]

	info = LASTINPUTINFO(ctypes.sizeof(LASTINPUTINFO), 0)
	if not ctypes.windll.user32.GetLastInputInfo(ctypes.byref(info)):
		return None
	return ((ctypes.windll.kernel32.GetTickCount() - info.dwTime) & 0xffffffff) // 1000


def display_timeout():
	"""The active power scheme's "turn off the display after" on AC power, in seconds (0: never; None: unknown)."""
	import uuid
	powrprof = ctypes.windll.powrprof
	scheme = ctypes.c_void_p()
	if powrprof.PowerGetActiveScheme(None, ctypes.byref(scheme)) != 0:
		return None
	try:
		value = wt.DWORD()
		if powrprof.PowerReadACValueIndex(None, scheme, uuid.UUID(GUID_VIDEO_SUBGROUP).bytes_le,
				uuid.UUID(GUID_VIDEO_POWERDOWN_TIMEOUT).bytes_le, ctypes.byref(value)) != 0:
			return None
		return value.value
	finally:
		ctypes.windll.kernel32.LocalFree(scheme)


def input_desktop():
	"""The input desktop's name ("Default" normally, "Winlogon" while the session is locked or at the sign-in screen),
	or "none" (no input desktop for this session: disconnected)."""
	user32 = ctypes.windll.user32
	user32.OpenInputDesktop.restype = wt.HANDLE
	handle = user32.OpenInputDesktop(0, False, 0x0001)  # DESKTOP_READOBJECTS
	if not handle:
		return "none"
	try:
		buffer = ctypes.create_unicode_buffer(256)
		needed = wt.DWORD()
		if not user32.GetUserObjectInformationW(handle, 2, buffer, ctypes.sizeof(buffer), ctypes.byref(needed)):
			return "?"
		return buffer.value
	finally:
		user32.CloseDesktop(handle)


def display_summary():
	"""The display's state in one line: power, the user's idle time against the timeout, the input desktop, the
	monitors and the primary display's mode."""
	if os.name != "nt":
		return "not Windows"
	idle, timeout = idle_seconds(), display_timeout()
	parts = ["display %s" % console_display_state(),
		"user idle %s s (the display's timeout %s)" % (idle, "never" if timeout == 0 else "%s s" % timeout),
		"input desktop %s" % input_desktop(), "%d monitor(s)" % ctypes.windll.user32.GetSystemMetrics(80)]
	try:
		parts.append("mode %dx%dx%d" % display_mode()[:3])
	except RunError:
		parts.append("mode unknown")
	return ", ".join(parts)


def window_summary(hwnd):
	"""The client window's state in one line: visible, minimized, the foreground (or which window is), its client
	area, and whether the foreground window covers its monitor (another program in full-screen)."""
	user32 = ctypes.windll.user32
	user32.GetForegroundWindow.restype = wt.HWND
	user32.MonitorFromWindow.restype = wt.HANDLE
	if not hwnd or not user32.IsWindow(hwnd):
		return "window gone"
	client = wt.RECT()
	user32.GetClientRect(hwnd, ctypes.byref(client))
	parts = ["window %s" % ("visible" if user32.IsWindowVisible(hwnd) else "hidden"),
		"minimized" if user32.IsIconic(hwnd) else "not minimized", "client area %dx%d" % (client.right, client.bottom)]
	foreground = user32.GetForegroundWindow()
	if foreground == hwnd:
		parts.append("the foreground")
	elif not foreground:
		parts.append("no foreground window")
	else:
		name = ctypes.create_unicode_buffer(256)
		user32.GetClassNameW(foreground, name, 256)
		pid = wt.DWORD()
		user32.GetWindowThreadProcessId(foreground, ctypes.byref(pid))
		rect = wt.RECT()
		user32.GetWindowRect(foreground, ctypes.byref(rect))

		class MONITORINFO(ctypes.Structure):
			_fields_ = [("cbSize", wt.DWORD), ("rcMonitor", wt.RECT), ("rcWork", wt.RECT), ("dwFlags", wt.DWORD)]

		info = MONITORINFO(ctypes.sizeof(MONITORINFO))
		monitor = user32.MonitorFromWindow(foreground, 2)  # MONITOR_DEFAULTTONEAREST
		covers = bool(user32.GetMonitorInfoW(monitor, ctypes.byref(info))) and rect.left <= info.rcMonitor.left \
			and rect.top <= info.rcMonitor.top and rect.right >= info.rcMonitor.right and rect.bottom >= info.rcMonitor.bottom
		parts.append("the foreground is %s of PID %d%s" % (name.value, pid.value, ", covering its monitor" if covers else ""))
	return ", ".join(parts)


def hresult(value):
	value &= 0xffffffff
	return HRESULTS.get(value, "0x%08x" % value)


def d3d9_summary(facts):
	"""patcap.dll's Direct3D 9 course in one line (Capture.d3d9)."""
	if facts["Direct3DCreates"] == 0:
		return "Direct3D 9: Direct3DCreate9 not called (%d ms since the load)" % facts["Loaded"]
	if facts["DeviceCreates"] == 0:
		return "Direct3D 9: Direct3DCreate9 called, CreateDevice not (%d ms since the load)" % facts["Loaded"]
	if facts["DeviceInside"]:
		# Issue #98's hang: the client's thread is inside the driver's CreateDevice.
		text = "Direct3D 9: CreateDevice call %d has not returned (called at %d ms, %d ms since the load; the call " 			"before: %s)" % (facts["DeviceCreates"], facts["DeviceCallTime"], facts["Loaded"], hresult(facts["DeviceResult"]))
		return text + (", %d Presents before it" % facts["Presents"] if facts["Presents"] else ", no Present")
	text = "Direct3D 9: CreateDevice %s (call %d, at %d ms)" % (hresult(facts["DeviceResult"]), facts["DeviceCreates"],
		facts["DeviceTime"])
	text += ", TestCooperativeLevel %s (%d not D3D_OK)" % (hresult(facts["CooperativeLevel"]), facts["LostChecks"])
	if facts["Presents"] == 0:
		return text + ", no Present (%d ms since the load)" % facts["Loaded"]
	return text + ", %d Presents (first at %d ms, last at %d ms of %d; the last %s, %d failed)" % (facts["Presents"],
		facts["FirstPresentTime"], facts["LastPresentTime"], facts["Loaded"], hresult(facts["PresentResult"]),
		facts["PresentFailures"])


def explain(hwnd, cap=None):
	"""Why a client may show no frame: the window, the display and (Tibia.exe) patcap.dll's Direct3D 9 course."""
	parts = []
	try:
		parts.append(window_summary(hwnd))
		parts.append(display_summary())
	except Exception as e:  # the diagnosis never hides the failure it explains
		parts.append("(window or display state unreadable: %s)" % e)
	if cap is not None:
		try:
			parts.append(d3d9_summary(cap.d3d9()))
		except Exception as e:
			parts.append("(patcap.dll's state unreadable: %s)" % e)
	return "; ".join(parts)


def self_test():
	"""The offsets of OFF against patcap.c's struct Shared comments, and the diagnosis texts on made-up data."""
	source = os.path.join(ROOT, "tools", "capture", "patcap.c")
	with open(source, encoding="utf-8") as f:
		text = f.read()
	struct_text = text[text.index("typedef struct {"):text.index("} Shared;")]
	found = {}
	for m in re.finditer(r"^\s*(?:volatile\s+)?(?:LONG|char)\s+(\w+)(?:\[\d+\])?;\s*/\*\s*(\d+)", struct_text, re.M):
		found[m.group(1)] = int(m.group(2))
	if found != OFF:
		raise SystemExit("self-test: OFF differs from patcap.c's struct Shared: only here %s, only there %s, other offsets %s"
			% (sorted(set(OFF) - set(found)), sorted(set(found) - set(OFF)),
				sorted(n for n in set(OFF) & set(found) if OFF[n] != found[n])))
	version = re.search(r"S->Version = (\d+);", text)
	if version is None or int(version.group(1)) != PATCAP_VERSION:
		raise SystemExit("self-test: patcap.c sets version %s, the tool reads %d" % (version and version.group(1), PATCAP_VERSION))
	if re.search(r"dllexport.*PatcapVulkan", text):
		raise SystemExit("self-test: patcap.c has Vulkan exports again (they left with issue #98)")
	none = {"Direct3DCreates": 1, "DeviceCreates": 1, "DeviceResult": 0x8876086a - (1 << 32), "DeviceTime": 412,
		"DeviceWidth": 0, "DeviceHeight": 0, "DeviceWindow": 0x1234, "CooperativeLevel": 1, "LostChecks": 0,
		"PresentResult": 1, "PresentFailures": 0, "FirstPresentTime": -1, "LastPresentTime": -1, "Presents": 0,
		"Loaded": 31000, "DeviceInside": 0, "DeviceCallTime": 400}
	checks = [
		(d3d9_summary(none), "Direct3D 9: CreateDevice D3DERR_NOTAVAILABLE (call 1, at 412 ms), TestCooperativeLevel "
			"not called yet (0 not D3D_OK), no Present (31000 ms since the load)"),
		(d3d9_summary(dict(none, Direct3DCreates=0)), "Direct3D 9: Direct3DCreate9 not called (31000 ms since the load)"),
		(d3d9_summary(dict(none, DeviceResult=0, CooperativeLevel=0x88760868 - (1 << 32), LostChecks=7, Presents=9,
			FirstPresentTime=500, LastPresentTime=900, PresentResult=0x88760868 - (1 << 32), PresentFailures=3)),
			"Direct3D 9: CreateDevice D3D_OK (call 1, at 412 ms), TestCooperativeLevel D3DERR_DEVICELOST (7 not D3D_OK), "
			"9 Presents (first at 500 ms, last at 900 ms of 31000; the last D3DERR_DEVICELOST, 3 failed)"),
		(d3d9_summary(dict(none, DeviceCreates=2, DeviceResult=0, DeviceInside=1, DeviceCallTime=484)),
			"Direct3D 9: CreateDevice call 2 has not returned (called at 484 ms, 31000 ms since the load; the call before: "
			"D3D_OK), no Present"),
		(hresult(0x12345678), "0x12345678"),
	]
	for got, want in checks:
		if got != want:
			raise SystemExit("self-test: %r, expected %r" % (got, want))
	looks = look_self_test()
	if os.name == "nt":
		summary = display_summary()  # reads only: no window of its own stays, no setting changes
		if not summary.startswith("display "):
			raise SystemExit("self-test: display_summary() gave %r" % summary)
		print("compare_rendering.py: this display: %s" % summary)
	print("compare_rendering.py: self-test passed (%d offsets, %d diagnosis texts, %d recording checks)"
		% (len(OFF), len(checks), looks))
	return 0


def look_self_test():
	"""session_client.recorded_look and check_recorded_look on made-up recordings (issue #140): the game login
	answer at [32098, 32219, 7] (the test server's start field) and a look at the own field, north of it, east of it,
	none. Returns the number of checks."""
	def game(direction, plain):
		return {"kind": "game", "dir": direction, "plain": plain}

	answer = game("s2c", "0a4e1ee700c8000064627ddb7d07" + "67" * 20)
	login = {"kind": "game", "dir": "c2s", "plain": "0a02", "fields_end": 47}

	def recording(look_hex):
		return [login, answer, game("c2s", "65"), game("c2s", "96010b0068656c6c6f20776f726c64")] + (
			[game("c2s", look_hex)] if look_hex else []) + [game("c2s", "8cffff4000000c0c0000")]

	own, north, east = "8c627ddb7d07630001", "8c627dda7d079a0100", "8c637ddb7d079a0100"
	checks = 0
	for events, want in ((recording(own), sc.LOOK_OWN_FIELD), (recording(north), sc.LOOK_NORTH)):
		got = sc.recorded_look(events)
		if got != want:
			raise SystemExit("self-test: recorded_look gave %r, expected %r" % (got, want))
		checks += 1
	for events, part in ((recording(east), "none of the session's look steps"),
			(recording(None)[:-1], "no look packet"), ([login, game("c2s", own)], "no game login answer"),
			(recording(north), "recorded with the look step 'look north'")):
		try:
			sc.check_recorded_look(events, LOOK, "made-up.jsonl")
		except sc.SessionError as e:
			if part not in str(e):
				raise SystemExit("self-test: check_recorded_look said %r, expected %r in it" % (str(e), part))
		else:
			raise SystemExit("self-test: check_recorded_look accepted a recording that should say %r" % part)
		checks += 1
	sc.check_recorded_look(recording(own), LOOK, "made-up.jsonl")
	return checks + 1


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
	# issue #147: inside Windows Sandbox unless --desktop (or --hidden) is given, its display at the fullscreen runs'
	# 1024x768; patcap.dll is built here first (the Sandbox has no compiler)
	skip = ("record", "compare", "--check-only", "--compare-only", "--self-test", "-h", "--help")
	if sandbox.wanted(skip):
		build_capture("capture-x86")
	sandbox.redirect(writable=[RENDER], skip=skip, display=FULLSCREEN_MODE[:2])  # the fullscreen runs' 1024x768
	desktop.hide(skip=("--check-only", "--compare-only", "--self-test", "-h", "--help"))  # issue #56: only with --hidden
	sys.exit(main())
