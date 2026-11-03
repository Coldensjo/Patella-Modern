r"""The dumps of the test mode (issue #22): read them and compare them with what the tools see from outside.

PatellaTest.exe --test-out <folder> writes (notes/testmode.md, "The dumps"):
  packets.log            every packet sent before encryption and received after decryption
                         (src/testmode/PacketLog.h): <time> <dir> <kind> <ordinal> <form> <payload> <padding>
  state-<NNN>.json       at each dump command of the script: client_state.py's state() and gui()
                         (src/testmode/StateDump.h)
  frame-<NNN>.png        at each frame command: the next frame the Vulkan engine shows (8-bit RGB)
  frame-<NNN>.filtered.png  its device-filtered pixels, white (as compare_rendering.py's <scene>.filtered.png)
  frames.txt             <step> <requested ms> <line> <shown ms> <width> <height> <filtered pixels>
tools/testmode_check.py adds external-<NNN>.json: client_state.py's reading of the same moment from
outside (the client waits for it, --test-sync).

What is compared (the issue's criteria):
  - state-<NNN>.json against external-<NNN>.json: "state" and "gui" equal, value for value;
  - packets.log against tools/proxy.py's decrypted recording of the same run: per connection (kind and
    ordinal), the client packets identical and in order (payload and padding; the first packet with
    its RSA block before encryption, proxy.py's "plain"), the server packets the client decrypted
    identical to the proxy's, in order, from the first; server packets the proxy passed on after the
    client's last one are listed (never read: the connection closed), not counted as differences;
  - the keys of the dump (StateDump.h's lists) against the keys client_state.py writes (read from its
    source with ast), statically.

Run on its own: python -I tools/testmode_dumps.py --self-test (the comparisons on made-up data: equal
data passes, a changed field, packet or missing dump fails; and the static key lists).
"""

import argparse
import ast
import json
import os
import re
import shutil
import struct
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE_HEADER = os.path.join(ROOT, "src", "testmode", "StateDump.h")
CLIENT_STATE = os.path.join(ROOT, "tools", "client_state.py")
STATE_FORMAT = "patella-state 1"
PACKET_HEADER = "# patella-packets 1"
# The gui() keys that hold this run's addresses; dropped when runs or clients are compared.
RUN_SPECIFIC_GUI = ("ptr", "vtable")


class DumpError(Exception):
	pass


# --- the script -------------------------------------------------------------------------------

def script_commands(path):
	"""(dumps, frames): how many dump and frame commands the script has (src/testmode/InputScript.cpp:
	one command per line, # starts a comment, the command caseless)."""
	dumps = frames = 0
	with open(path, encoding="utf-8-sig") as f:
		for line in f:
			words = line.split("#", 1)[0].split()
			if not words:
				continue
			command = words[0].lower()
			dumps += command == "dump"
			frames += command == "frame"
	return dumps, frames


# --- the static key lists ---------------------------------------------------------------------

def header_key_lists(path=STATE_HEADER):
	"""StateDump.h's key lists: name -> [keys]."""
	with open(path, encoding="utf-8") as f:
		text = f.read()
	lists = {}
	for match in re.finditer(r"inline constexpr std::array<std::string_view, (\d+)> (\w+) = \{(.*?)\};", text, re.S):
		keys = re.findall(r'"([^"]*)"', match.group(3))
		if len(keys) != int(match.group(1)):
			raise DumpError("%s: %s has %d keys, declared %s" % (path, match.group(2), len(keys), match.group(1)))
		lists[match.group(2)] = keys
	return lists


def _method(tree, name):
	for node in ast.walk(tree):
		if isinstance(node, ast.ClassDef) and node.name == "Client":
			for item in node.body:
				if isinstance(item, ast.FunctionDef) and item.name == name:
					return item
	raise DumpError("client_state.py: no Client.%s" % name)


def _dict_keys(function, require=None):
	"""The keys of the dict literals in function (the first one that has `require`, else the first)."""
	found = []
	for node in ast.walk(function):
		if isinstance(node, ast.Dict) and node.keys and all(isinstance(k, ast.Constant) for k in node.keys):
			keys = [k.value for k in node.keys]
			if require is None or require in keys:
				found.append(keys)
	if not found:
		raise DumpError("client_state.py: no dict literal in %s" % function.name)
	return found[0]


def client_state_key_lists(path=CLIENT_STATE):
	"""The keys client_state.py's state() and gui() write, by StateDump.h's list names."""
	with open(path, encoding="utf-8") as f:
		tree = ast.parse(f.read())
	state = _method(tree, "state")
	state_keys, player = [], None
	for node in state.body:
		if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Subscript):
			key = node.targets[0].slice
			if isinstance(key, ast.Constant):
				state_keys.append(key.value)
				if key.value == "player":
					for sub in ast.walk(node.value):
						if isinstance(sub, ast.Tuple) and all(isinstance(e, ast.Constant) for e in sub.elts):
							player = [e.value for e in sub.elts]
	if player is None:
		raise DumpError("client_state.py: state() has no tuple of player globals")
	gui = _method(tree, "gui")
	gui_keys = _dict_keys(gui, "depth")
	for node in ast.walk(gui):
		if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Subscript):
			key = node.targets[0].slice
			if isinstance(key, ast.Constant) and key.value not in gui_keys:
				gui_keys.append(key.value)
	return {"StateKeys": state_keys, "PlayerFields": player, "CreatureKeys": _dict_keys(_method(tree, "creatures")),
		"ContainerKeys": _dict_keys(_method(tree, "containers")), "OfferKeys": _dict_keys(_method(tree, "offer")),
		"GuiKeys": gui_keys}


def compare_field_lists(header=STATE_HEADER, client_state=CLIENT_STATE):
	"""(problems, lines): StateDump.h's key lists against client_state.py's keys, list for list, in order."""
	ours = header_key_lists(header)
	theirs = client_state_key_lists(client_state)
	problems, lines = [], []
	for name in sorted(set(ours) | set(theirs)):
		a, b = ours.get(name), theirs.get(name)
		if a == b:
			lines.append("  %-14s %2d keys, the same: %s" % (name, len(a), " ".join(a)))
		else:
			problems.append("  %s: StateDump.h %s\n      client_state.py %s" % (name, a, b))
	return problems, lines


# --- the packet log ---------------------------------------------------------------------------

def parse_packet_line(line):
	fields = line.rstrip("\r\n").split(" ")
	if len(fields) != 7 or fields[1] not in ("c2s", "s2c") or fields[4] not in ("plain", "xtea"):
		raise DumpError("not a packet line: %r" % line[:80])
	return {"time": int(fields[0]), "dir": fields[1], "kind": fields[2], "ordinal": int(fields[3]),
		"plain": fields[4] == "plain", "payload": "" if fields[5] == "-" else fields[5],
		"pad": "" if fields[6] == "-" else fields[6]}


def load_packet_log(path):
	with open(path, encoding="ascii") as f:
		lines = f.read().splitlines()
	if not lines or lines[0] != PACKET_HEADER:
		raise DumpError("%s: does not start with %r" % (path, PACKET_HEADER))
	return [parse_packet_line(line) for line in lines[1:] if line and not line.startswith("#")]


def compare_packet_log(records, events, limit=10):
	"""(differences, summary): the client's packet log against proxy.py's events of the same run."""
	ours, theirs = {}, {}
	for r in records:
		ours.setdefault((r["kind"], r["ordinal"]), {"c2s": [], "s2c": []})[r["dir"]].append(
			(r["payload"], r["pad"] if not r["plain"] else ""))
	for e in events:
		if e["dir"] not in ("c2s", "s2c"):
			continue
		theirs.setdefault((e["kind"], e["conn_ordinal"]), {"c2s": [], "s2c": []})[e["dir"]].append(
			(e["plain"], e.get("pad", "") if "fields_end" not in e else ""))
	differences = []
	summary = {"connections": 0, "c2s": 0, "s2c": 0, "unread": 0}
	for key in sorted(set(ours) | set(theirs)):
		label = "%s %d" % key
		a = ours.get(key, {"c2s": [], "s2c": []})
		b = theirs.get(key, {"c2s": [], "s2c": []})
		summary["connections"] += 1
		summary["c2s"] += len(a["c2s"])
		summary["s2c"] += len(a["s2c"])
		if a["c2s"] != b["c2s"]:
			for i in range(max(len(a["c2s"]), len(b["c2s"]))):
				x = a["c2s"][i] if i < len(a["c2s"]) else None
				y = b["c2s"][i] if i < len(b["c2s"]) else None
				if x != y and len(differences) < limit:
					differences.append("  %s c2s #%d: log %s\n      proxy %s" % (label, i, x, y))
		read, passed = a["s2c"], b["s2c"]
		if read != passed[:len(read)]:
			for i, x in enumerate(read):
				y = passed[i] if i < len(passed) else None
				if x != y and len(differences) < limit:
					differences.append("  %s s2c #%d: log %s\n      proxy %s" % (label, i, x, y))
		else:
			summary["unread"] += len(passed) - len(read)
	return differences, summary


# --- the state dumps --------------------------------------------------------------------------

def numbered(folder, prefix, suffix):
	"""{step: path} of prefix-<NNN>suffix in folder."""
	out = {}
	if os.path.isdir(folder):
		for name in os.listdir(folder):
			match = re.fullmatch(re.escape(prefix) + r"-(\d+)" + re.escape(suffix), name)
			if match:
				out[int(match.group(1))] = os.path.join(folder, name)
	return out


def load_json(path):
	with open(path, encoding="utf-8") as f:
		return json.load(f)


def diff_values(a, b, path="", out=None, limit=20):
	"""Where two JSON values differ (compare_traffic.py's diff_values, with the indexes)."""
	out = [] if out is None else out
	if len(out) >= limit:
		return out
	if isinstance(a, dict) and isinstance(b, dict):
		for key in sorted(set(a) | set(b)):
			if key not in a or key not in b:
				out.append("%s.%s: only in %s" % (path, key, "the dump" if key in a else "the reading"))
			else:
				diff_values(a[key], b[key], "%s.%s" % (path, key), out, limit)
	elif isinstance(a, list) and isinstance(b, list):
		if len(a) != len(b):
			out.append("%s: %d entries against %d" % (path, len(a), len(b)))
		for i, (x, y) in enumerate(zip(a, b)):
			diff_values(x, y, "%s[%d]" % (path, i), out, limit)
	elif a != b or type(a) is not type(b):
		out.append("%s: %r against %r" % (path, a, b))
	return out


def compare_state_dumps(folder, expected, limit=20):
	"""(differences, lines): every state-<NNN>.json against external-<NNN>.json; expected: the script's
	number of dump commands."""
	dumps = numbered(folder, "state", ".json")
	readings = numbered(folder, "external", ".json")
	differences, lines = [], []
	if sorted(dumps) != list(range(1, expected + 1)):
		differences.append("  %d state dumps (%s), the script has %d dump commands" % (len(dumps),
			" ".join(str(s) for s in sorted(dumps)), expected))
	for step in sorted(dumps):
		dump = load_json(dumps[step])
		if dump.get("format") != STATE_FORMAT or dump.get("step") != step:
			differences.append("  state-%03d.json: format %r, step %r" % (step, dump.get("format"), dump.get("step")))
			continue
		if step not in readings:
			differences.append("  step %d: no external-%03d.json (client_state.py's reading)" % (step, step))
			continue
		reading = load_json(readings[step])
		if "error" in reading:
			differences.append("  step %d: client_state.py failed: %s" % (step, reading["error"]))
			continue
		found = diff_values(dump["state"], reading["state"], "state", None, limit)
		found = diff_values(dump["gui"], reading["gui"], "gui", found, limit)
		state = dump["state"]
		lines.append("  step %d (%d ms, line %d): %s; %d map objects, %d creatures, %d containers, %d console lines,"
			" %d windows" % (step, dump["time"], dump["line"], "equal" if not found else "DIFFERENT",
			sum(len(p) for p in state.get("map", [])), len(state.get("creatures", [])),
			len(state.get("containers", [])), len(state.get("console", [])), len(dump["gui"])))
		differences.extend("  step %d: %s" % (step, d) for d in found)
	return differences, lines


def comparable(dump):
	"""A dump without this run's addresses (gui's ptr and vtable; a vt_ class name is an address too)."""
	gui = []
	for node in dump["gui"]:
		node = {k: v for k, v in node.items() if k not in RUN_SPECIFIC_GUI}
		if node["class"].startswith("vt_"):
			node["class"] = "vt"
		gui.append(node)
	return {"state": dump["state"], "gui": gui}


# --- the frames -------------------------------------------------------------------------------

def read_png(path):
	"""(width, height, rgb rows) of an 8-bit RGB PNG with filter 0 rows (Png.h, compare_rendering.py)."""
	with open(path, "rb") as f:
		data = f.read()
	if data[:8] != b"\x89PNG\r\n\x1a\n":
		raise DumpError("%s: not a PNG" % path)
	at, idat, header = 8, b"", None
	while at < len(data):
		length, = struct.unpack_from(">I", data, at)
		kind = data[at + 4:at + 8]
		body = data[at + 8:at + 8 + length]
		crc, = struct.unpack_from(">I", data, at + 8 + length)
		if zlib.crc32(kind + body) & 0xffffffff != crc:
			raise DumpError("%s: chunk %s has a wrong CRC" % (path, kind))
		if kind == b"IHDR":
			header = struct.unpack(">IIBBBBB", body)
		elif kind == b"IDAT":
			idat += body
		at += 12 + length
	if header is None or header[2:] != (8, 2, 0, 0, 0):
		raise DumpError("%s: not 8-bit RGB (%s)" % (path, header))
	width, height = header[:2]
	raw = zlib.decompress(idat)
	stride = width * 3 + 1
	if len(raw) != stride * height or any(raw[y * stride] != 0 for y in range(height)):
		raise DumpError("%s: %d bytes of rows, expected %d with filter 0" % (path, len(raw), stride * height))
	return width, height, [raw[y * stride + 1:(y + 1) * stride] for y in range(height)]


def check_frames(folder, expected):
	"""(problems, lines): the frame dumps of a run: as many as the script's frame commands, readable,
	the mask of the same size, frames.txt's line for each."""
	frames = numbered(folder, "frame", ".png")
	masks = numbered(folder, "frame", ".filtered.png")
	problems, lines = [], []
	index = {}
	path = os.path.join(folder, "frames.txt")
	if os.path.isfile(path):
		with open(path, encoding="ascii") as f:
			for line in f.read().splitlines():
				fields = [int(v) for v in line.split()]
				index[fields[0]] = fields
	if sorted(frames) != list(range(1, expected + 1)):
		problems.append("  %d frame dumps, the script has %d frame commands" % (len(frames), expected))
	for step in sorted(frames):
		try:
			width, height, rows = read_png(frames[step])
			mask = None
			if step in masks:
				mask_width, mask_height, mask_rows = read_png(masks[step])
				if (mask_width, mask_height) != (width, height):
					problems.append("  frame %d: mask %dx%d, frame %dx%d" % (step, mask_width, mask_height, width, height))
				mask = sum(row.count(b"\xff\xff\xff") for row in mask_rows)
			entry = index.get(step)
			if entry is None or entry[4:6] != [width, height]:
				problems.append("  frame %d: frames.txt says %s" % (step, entry))
			lines.append("  frame %d: %dx%d, requested at %s ms (line %s), shown at %s ms, %s filtered pixels" % (step,
				width, height, entry[1] if entry else "?", entry[2] if entry else "?", entry[3] if entry else "?",
				"no mask," if mask is None else mask))
		except (DumpError, zlib.error, struct.error) as e:
			problems.append("  frame %d: %s" % (step, e))
	return problems, lines


# --- the self-test ----------------------------------------------------------------------------

def _fake_run(folder, number, script):
	"""A made-up run in folder: proxy.py's recording, the client's packet log, a state dump and its
	reading, a frame, the driver's log and Tibia.cfg, all consistent."""
	header = bytes([0x01, 0x02, 0x00, 0x04, 0x03]) + bytes(12)
	block = bytes([0]) + bytes(range(1, 17)) + struct.pack("<I", 772001) + b"\x07\x00patella" + bytes(98)
	login = (header + block).hex()
	game = (bytes([0x0a, 0x02, 0x00, 0x04, 0x03]) + block).hex()
	events = [
		{"t": 0.1, "conn": 1, "kind": "login", "conn_ordinal": 1, "dir": "c2s", "raw": "", "plain": login,
			"key": block[1:17].hex(), "fields": {}, "fields_end": 30, "rsapad": block[30:].hex()},
		{"t": 0.2, "conn": 1, "kind": "login", "conn_ordinal": 1, "dir": "s2c", "raw": "", "plain": "6400", "pad": "aabb"},
		{"t": 0.3, "conn": 1, "kind": "login", "conn_ordinal": 1, "dir": "close", "side": "server"},
		{"t": 0.4, "conn": 2, "kind": "game", "conn_ordinal": 1, "dir": "c2s", "raw": "", "plain": game,
			"key": block[1:17].hex(), "fields": {}, "fields_end": 30, "rsapad": block[30:].hex()},
		{"t": 0.5, "conn": 2, "kind": "game", "conn_ordinal": 1, "dir": "s2c", "raw": "", "plain": "0a01020304", "pad": "01"},
		{"t": 0.6, "conn": 2, "kind": "game", "conn_ordinal": 1, "dir": "c2s", "raw": "", "plain": "65", "pad": "11223344"},
		{"t": 0.7, "conn": 2, "kind": "game", "conn_ordinal": 1, "dir": "c2s", "raw": "", "plain": "14", "pad": "55667788"},
		{"t": 0.8, "conn": 2, "kind": "game", "conn_ordinal": 1, "dir": "s2c", "raw": "", "plain": "1e", "pad": ""},
	]
	with open(os.path.join(folder, "run%d.jsonl" % number), "w", encoding="utf-8") as f:
		for e in events:
			f.write(json.dumps(e) + "\n")
	dumps = os.path.join(folder, "run%d" % number)
	os.makedirs(dumps)
	with open(os.path.join(dumps, "packets.log"), "w", encoding="ascii", newline="\n") as f:
		f.write(PACKET_HEADER + "\n")
		f.write("100 c2s login 1 plain %s -\n" % login)
		f.write("200 s2c login 1 xtea 6400 aabb\n")
		f.write("300 c2s game 1 plain %s -\n" % game)
		f.write("400 s2c game 1 xtea 0a01020304 01\n")
		f.write("500 c2s game 1 xtea 65 11223344\n")
		f.write("600 c2s game 1 xtea 14 55667788\n")
	state = {"connection": 8, "player": {"PlayerID": 268435457, "PlayerHealth": 150},
		"skills": [10] * 7, "map": [[[102, 0, 0]], []], "creatures": [{"slot": 0, "id": 268435457, "name": "Patella"}],
		"console": [[1, "hello"], [17, None]]}
	gui = [{"depth": 0, "class": "TMainWindow", "x": 0, "y": 0, "w": 800, "h": 600, "ptr": 4096, "vtable": 8192},
		{"depth": 1, "class": "TLabel", "x": 4, "y": 5, "w": 6, "h": 7, "ptr": 12288, "vtable": 16384, "text": None}]
	with open(os.path.join(dumps, "state-001.json"), "w", encoding="utf-8") as f:
		json.dump({"format": STATE_FORMAT, "step": 1, "time": 2000, "line": 4, "state": state, "gui": gui}, f,
			separators=(",", ":"))
	with open(os.path.join(dumps, "external-001.json"), "w", encoding="utf-8") as f:
		json.dump({"format": STATE_FORMAT, "source": "client_state.py", "step": 1, "state": state, "gui": gui}, f)
	width, height = 4, 3

	def png(rows):
		def chunk(kind, body):
			return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff)
		return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
			+ chunk(b"IDAT", zlib.compress(b"".join(b"\0" + r for r in rows))) + chunk(b"IEND", b""))

	with open(os.path.join(dumps, "frame-001.png"), "wb") as f:
		f.write(png([bytes(range(12))] * height))
	with open(os.path.join(dumps, "frame-001.filtered.png"), "wb") as f:
		f.write(png([b"\xff\xff\xff" + bytes(9)] * height))
	with open(os.path.join(dumps, "frames.txt"), "w", encoding="ascii") as f:
		f.write("1 2000 5 2000 %d %d %d\n" % (width, height, height))
	with open(os.path.join(folder, "run%d.log" % number), "w", encoding="utf-8") as f:
		f.write("testmode: script %s\n2000 ms, line 4: dump\n2000 ms, line 5: frame\n" % script)
	with open(os.path.join(folder, "run%d.Tibia.cfg" % number), "w", encoding="latin-1") as f:
		f.write("Engine = 2\n")


def self_test(compare, out=print):
	"""Runs compare(folder, runs, script, quiet) (tools/testmode_check.py's) on made-up runs: equal data
	gives 0, a changed state field, a changed client packet, a changed server packet and a missing dump
	give 1. Returns the number of failed expectations."""
	failures = 0
	cases = []

	def change_json(path, edit):
		data = load_json(path)
		edit(data)
		with open(path, "w", encoding="utf-8") as f:
			json.dump(data, f)

	def change_text(path, old, new):
		with open(path, encoding="ascii") as f:
			text = f.read()
		if old not in text:
			raise DumpError("%s: %r not found" % (path, old))
		with open(path, "w", encoding="ascii", newline="\n") as f:
			f.write(text.replace(old, new, 1))

	cases.append(("identical runs", None, 0, "RESULT: no differences"))
	cases.append(("a state field differs from client_state.py's reading",
		lambda d: change_json(os.path.join(d, "run1", "external-001.json"),
			lambda v: v["state"]["player"].__setitem__("PlayerHealth", 149)), 1, "step 1: state.player.PlayerHealth: 150 against 149"))
	cases.append(("a window's text differs",
		lambda d: change_json(os.path.join(d, "run2", "external-001.json"),
			lambda v: v["gui"][1].__setitem__("text", "")), 1, "step 1: gui[1].text: None against ''"))
	cases.append(("a client packet differs from the proxy's",
		lambda d: change_text(os.path.join(d, "run1", "packets.log"), "xtea 65 11223344", "xtea 66 11223344"), 1, "game 1 c2s #1: log ('66', '11223344')"))
	cases.append(("a client packet's padding differs",
		lambda d: change_text(os.path.join(d, "run1", "packets.log"), "xtea 14 55667788", "xtea 14 55667789"), 1, "game 1 c2s #2: log ('14', '55667789')"))
	cases.append(("a server packet differs from the proxy's",
		lambda d: change_text(os.path.join(d, "run2", "packets.log"), "s2c game 1 xtea 0a01020304", "s2c game 1 xtea 0a01020305"), 1, "game 1 s2c #0: log ('0a01020305', '01')"))
	cases.append(("a dump is missing", lambda d: os.remove(os.path.join(d, "run1", "state-001.json")), 1,
		"0 state dumps (), the script has 1 dump commands"))
	cases.append(("client_state.py's reading is missing", lambda d: os.remove(os.path.join(d, "run2", "external-001.json")), 1,
		"step 1: no external-001.json"))
	cases.append(("a frame is missing", lambda d: os.remove(os.path.join(d, "run1", "frame-001.png")), 1,
		"0 frame dumps, the script has 1 frame commands"))
	for title, change, expected, marker in cases:
		folder = tempfile.mkdtemp(prefix="testmode-selftest-")
		try:
			script = os.path.join(folder, "made-up.script")
			with open(script, "w", encoding="utf-8") as f:
				f.write("pace 0\nwait 2000\n# the dumps\ndump\nframe  # the next frame\nquit\n")
			for number in (1, 2):
				_fake_run(folder, number, script)
			if change is not None:
				change(folder)
			report = []
			got = compare(folder, 2, script, quiet=True, report=report)
		finally:
			shutil.rmtree(folder, ignore_errors=True)
		ok = got == expected and any(marker in line for line in report)
		failures += not ok
		out("  %-55s exit %d, expected %d, the report says so: %s" % (title, got, expected, "ok" if ok else "FAILED"))
		if not ok:
			for line in report:
				out("    " + line)
	problems, lines = compare_field_lists()
	for line in lines:
		out(line)
	for problem in problems:
		out(problem)
	out("  %-55s %s" % ("StateDump.h's keys = client_state.py's keys", "ok" if not problems else "FAILED"))
	failures += bool(problems)
	# The checker of the key lists finds a difference.
	folder = tempfile.mkdtemp(prefix="testmode-selftest-")
	try:
		changed = os.path.join(folder, "StateDump.h")
		with open(STATE_HEADER, encoding="utf-8") as f:
			text = f.read()
		with open(changed, "w", encoding="utf-8") as f:
			f.write(text.replace('"skull", "shield"', '"skull", "shields"', 1))
		problems, _ = compare_field_lists(header=changed)
	finally:
		shutil.rmtree(folder, ignore_errors=True)
	ok = len(problems) == 1
	failures += not ok
	out("  %-55s %s" % ("a renamed key in StateDump.h is found", "ok" if ok else "FAILED"))
	out("self test: %d failures" % failures)
	return failures


def main(argv=None):
	parser = argparse.ArgumentParser(description="The test mode's dumps: the static key lists and the self-test.")
	parser.add_argument("--self-test", action="store_true", help="the comparisons on made-up data")
	parser.add_argument("--fields", action="store_true", help="StateDump.h's keys against client_state.py's")
	args = parser.parse_args(argv)
	if not args.self_test and not args.fields:
		parser.error("nothing to do: --self-test and/or --fields")
	failures = 0
	if args.fields:
		problems, lines = compare_field_lists()
		for line in lines + problems:
			print(line)
		print("field lists: %s" % ("the same" if not problems else "%d differ" % len(problems)))
		failures += len(problems)
	if args.self_test:
		sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
		import testmode_check  # noqa: E402 (it imports this module)
		failures += self_test(testmode_check.compare)
	return 1 if failures else 0


if __name__ == "__main__":
	sys.exit(main())
