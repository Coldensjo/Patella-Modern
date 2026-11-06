r"""Drive and read a running test client through its control channel (Patella-Modern issue #92).

The comparison tools read the x64 client only through its test mode: they run PatellaTest.exe (the test
build next to Patella.exe) with `--test-control <name> --test-out <folder>` (src/testmode/TestModeWin32.cpp)
and never read its memory (tools/client_state.py) or hook its frames (tools/capture/patcap.c); both stay
for Tibia.exe and the oracle's 32-bit Patella.exe. notes/testmode.md, "The control channel".

Channel: the two events <name>-command and <name>-done (made here), control.txt and control-reply.txt in
the test-out folder. command(text) writes the text, sets <name>-command and waits for <name>-done; the
client answers at an idle call, after every earlier command is done:
  - "probe": probe.json, the state dump's state and gui, the character list, the skin values, the members
    of the components (by "ptr"), the map window's names, the Vulkan engine's device and frames
    (src/testmode/StateDump.h, "The probe");
  - with a script (--test-script), lines of a script (src/testmode/InputScript.h): post, advance, frame,
    ...; done once they are handed out, the idle calls after them passed and their frames written.

TestClient has the reading interface of client_state.Client that the session drivers use (state(), gui(),
characters(), creatures(), player_id(), skin_rects(), skin_int(), member(), member_index(), checked(),
map_names(), snapshot(), close()), each from a fresh probe.

python -I tools/testmode_client.py --self-test: the reader on made-up probes (no client).
"""

import ctypes
import json
import os
import sys
import time

WAIT_OBJECT_0 = 0
PROBE_FORMAT = "patella-probe 1"  # src/testmode/StateDump.h ProbeFormat
COMMAND_TIMEOUT = 60  # seconds a command may take (a frame waits for the next frame shown)
_counter = [0]


class ControlError(Exception):
	pass


class Channel:
	"""The control channel of one test client: its events and its test-out folder."""

	def __init__(self, out):
		self.out = os.path.abspath(out)
		os.makedirs(self.out, exist_ok=True)
		_counter[0] += 1
		self.name = "Local\\patella-control-%d-%d-%d" % (os.getpid(), _counter[0], int(time.time() * 1000) % 1000000)
		self.kernel32 = None
		self.handles = []
		if sys.platform == "win32":
			import ctypes.wintypes as wt
			k = ctypes.WinDLL("kernel32", use_last_error=True)
			k.CreateEventW.restype = wt.HANDLE
			k.CreateEventW.argtypes = [ctypes.c_void_p, wt.BOOL, wt.BOOL, wt.LPCWSTR]
			k.SetEvent.argtypes = [wt.HANDLE]
			k.ResetEvent.argtypes = [wt.HANDLE]
			k.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
			k.WaitForSingleObject.restype = wt.DWORD
			k.CloseHandle.argtypes = [wt.HANDLE]
			self.kernel32 = k
			self.command_event = k.CreateEventW(None, False, False, self.name + "-command")
			self.done_event = k.CreateEventW(None, False, False, self.name + "-done")
			if not self.command_event or not self.done_event:
				raise ControlError("CreateEventW failed (error %d)" % ctypes.get_last_error())
			self.handles = [self.command_event, self.done_event]
		self.commands = 0

	def arguments(self):
		"""The client's command line words for the channel."""
		return ["--test-control", self.name, "--test-out", self.out]

	def close(self):
		for handle in self.handles:
			self.kernel32.CloseHandle(handle)
		self.handles = []

	def command(self, text, timeout=COMMAND_TIMEOUT, alive=None):
		"""Sends one command (text: "probe" or script lines) and waits for its answer."""
		if not self.handles:
			raise ControlError("the control channel is closed")
		reply = os.path.join(self.out, "control-reply.txt")
		if os.path.isfile(reply):
			os.remove(reply)
		with open(os.path.join(self.out, "control.txt"), "w", encoding="utf-8", newline="\n") as f:
			f.write(text.rstrip("\n") + "\n")
		self.commands += 1
		self.kernel32.ResetEvent(self.done_event)
		self.kernel32.SetEvent(self.command_event)
		deadline = time.time() + timeout
		while self.kernel32.WaitForSingleObject(self.done_event, 50) != WAIT_OBJECT_0:
			if alive is not None and not alive():
				raise ControlError("the client ended during the command %r" % first_line(text))
			if time.time() > deadline:
				raise ControlError("no answer to the command %r within %d s" % (first_line(text), timeout))
		try:
			with open(reply, encoding="utf-8") as f:
				answer = f.read().strip()
		except OSError as e:
			raise ControlError("the command %r: no control-reply.txt (%s)" % (first_line(text), e))
		if answer != "ok":
			raise ControlError("the command %r: %s" % (first_line(text), answer))

	def probe(self, alive=None):
		"""The client's probe.json now (a dict)."""
		self.command("probe", alive=alive)
		return load_probe(os.path.join(self.out, "probe.json"))


def first_line(text):
	return text.strip().split("\n", 1)[0]


def load_probe(path):
	with open(path, encoding="utf-8") as f:
		data = json.load(f)
	if data.get("format") != PROBE_FORMAT:
		raise ControlError("%s: format %r, not %r" % (path, data.get("format"), PROBE_FORMAT))
	return data


class TestClient:
	"""client_state.Client's reading interface over the control channel's probes (every call a fresh
	probe, so a value is the client's at that call; snapshot() gives state and gui of one moment)."""

	def __init__(self, channel, alive=None, source=None):
		self.channel = channel
		self.alive = alive
		self.data = None
		self.source = source  # tests: a function returning a probe dict instead of the channel

	def refresh(self):
		self.data = self.source() if self.source is not None else self.channel.probe(self.alive)
		return self.data

	def close(self):
		if self.channel is not None:
			self.channel.close()

	# the readings of client_state.Client

	def state(self):
		return self.refresh()["state"]

	def gui(self):
		return self.refresh()["gui"]

	def snapshot(self):
		data = self.refresh()
		return data["state"], data["gui"]

	def characters(self):
		return list(self.refresh()["characters"])

	def creatures(self):
		return self.refresh()["state"]["creatures"]

	def player_id(self):
		return self.refresh()["state"]["player"]["PlayerID"]

	def skin(self, member):
		data = self.data if self.data is not None else self.refresh()
		if member not in data["skin"]:
			raise ControlError("the probe has no skin value %r (src/testmode/ClientStateWin32.cpp ReadProbe)" % member)
		return data["skin"][member]

	def skin_rects(self, member, count):
		rects = self.skin(member)
		if count > len(rects):
			raise ControlError("the probe has %d rectangles of %s, not %d" % (len(rects), member, count))
		return [tuple(r) for r in rects[:count]]

	def skin_int(self, member):
		return self.skin(member)

	def member(self, component, path):
		"""The member path (client_state.LAYOUT_FIELDS' name) of the component at `component` (a gui "ptr"):
		a pointer as a "ptr" (0: none), an int, a bool's byte."""
		members = self.refresh()["members"].get(str(component))
		if members is None:
			raise ControlError("component 0x%x is not in the window tree" % component)
		if path not in members:
			raise ControlError("the probe has no %s of component 0x%x (its class has %s)" % (path, component,
				", ".join(sorted(members))))
		return members[path]

	def member_index(self, component, path, index):
		return self.member(component, "%s[%d]" % (path, index))

	def checked(self, box):
		return self.member(box, "TCheckBox.Checked") != 0

	def map_names(self, map_window):
		"""[(creature ID, x, y)] of the map window's names (TMapWindow.Names, NumberOfNames of them)."""
		names = self.refresh()["names"].get(str(map_window))
		if names is None:
			raise ControlError("0x%x is not a map window of the window tree" % map_window)
		return [tuple(n) for n in names]


def self_test(out=print):
	"""The reader on made-up probes. Returns the number of failures."""
	failures = 0

	def check(title, ok):
		nonlocal failures
		failures += not ok
		out("  %-60s %s" % (title, "ok" if ok else "FAILED"))

	probe = {"format": PROBE_FORMAT, "step": 1, "time": 0, "state": {"player": {"PlayerID": 7},
		"creatures": [{"id": 7, "name": "Patella Tester"}]}, "gui": [{"depth": 0, "class": "TMainWindow", "ptr": 10}],
		"characters": ["Patella Tester"], "skin": {"Data62C": 4, "Style288.Data60": [[1, 2, 3, 4], [5, 6, 7, 8]]},
		"members": {"10": {"TComponent.Parent": 0, "TCheckBox.Checked": 1, "TEditMarkDialog.TypeButtons[2]": 99}},
		"names": {"10": [[7, 240, 160]]}, "device": None, "frames": 0, "ticks": 0}
	calls = [0]

	def source():
		calls[0] += 1
		return probe

	out("the control channel's reader (issue #92)")
	client = TestClient(None, source=source)
	check("state, gui, characters", client.state()["player"]["PlayerID"] == 7 and client.gui()[0]["ptr"] == 10
		and client.characters() == ["Patella Tester"])
	check("every reading a fresh probe", calls[0] == 3)
	check("player_id, creatures", client.player_id() == 7 and client.creatures()[0]["name"] == "Patella Tester")
	check("skin_int, skin_rects", client.skin_int("Data62C") == 4
		and client.skin_rects("Style288.Data60", 2) == [(1, 2, 3, 4), (5, 6, 7, 8)])
	check("member, member_index, checked", client.member(10, "TComponent.Parent") == 0
		and client.member_index(10, "TEditMarkDialog.TypeButtons", 2) == 99 and client.checked(10))
	check("map_names", client.map_names(10) == [(7, 240, 160)])
	for title, call in (("a component not in the tree", lambda: client.member(11, "TComponent.Parent")),
			("a member its class does not have", lambda: client.member(10, "TSubWindow.CloseButton")),
			("a skin value the probe lacks", lambda: client.skin_int("Data999")),
			("more rectangles than the probe has", lambda: client.skin_rects("Style288.Data60", 3)),
			("not a map window", lambda: client.map_names(11))):
		try:
			call()
			check(title + ": refused", False)
		except ControlError:
			check(title + ": refused", True)
	out("control channel self test: %d failures" % failures)
	return failures


if __name__ == "__main__":
	if sys.argv[1:] == ["--self-test"]:
		sys.exit(1 if self_test() else 0)
	print(__doc__)
	sys.exit(2)
