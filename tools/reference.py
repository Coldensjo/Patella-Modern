r"""Reference recordings (Patella-Modern issue #23): record them from Tibia.exe, compare the test client
with them.

Usage (from the repository or a worktree):
  python -I tools/reference.py record  [--reference DIR] [--sessions a,b] [--live DIR] [--reuse-live]
  python -I tools/reference.py compare [--reference DIR] [--sessions a,b] [--exe PatellaTest.exe]
                                       [--out DIR] [--vulkan cpu] [--timeout 600] [--check-only]
  python -I tools/reference.py list    [--reference DIR]
  python -I tools/reference.py --self-test

The two steps of the comparison tools (notes/modernization-plan.md, 4.4; notes/testmode.md,
"Reference recordings"):

record (Windows, Tibia.exe; tools/reference_record.py): every session of compare_traffic.py,
  compare_files.py and compare_rendering.py is played once by Tibia.exe under tools/capture/patcap.c's
  virtual clock, against tools/proxy.py replaying the session's server side (a live recording of the
  test server, made first, or a crafted one), and written to <reference>\<session>\: the test-mode
  script of the run (every window message the record step posted, every advance of the clock, a dump
  and a frame at every checkpoint), the server side for the test client (replay.txt), Tibia.exe's
  packets (reference.jsonl), its state at every checkpoint (states\), its frames (frames\), the
  files it wrote (files\), the files it started with (seed\) and meta.json (format, Tibia.exe's and
  the data files' SHA-256, exit code, message boxes, frame masks, the display modes the machine
  reported: display_modes, issue #48). The live test server is needed only here, for the live
  recordings.

compare (the test client, PatellaTest.exe; this file, Python only, no Windows-only code): every
  session runs once in a fresh run folder (the data files, the client patched to 127.0.0.1:7271 by
  tools/patch_client.py, the session's Tibia.cfg and seed files, the CPU driver) with
  --test-script session.script --test-replay replay.txt (the server side, replayed inside the client:
  no server) --test-display-modes (the recording machine's display modes, issue #48: the Graphics
  Options dialog lists them, not this machine's) --test-out, PATELLA_VULKAN=cpu and TZ=UTC0 (as the
  record step's Tibia.exe); then compared:
  a. the client's packets (packets.log) against Tibia.exe's, per connection, in order: the payloads,
     the XTEA padding and the first packets with their whole RSA block (key and random bytes: the
     virtual clock gives rand() the same seed); the server packets the client decrypted against those
     Tibia.exe was sent;
  b. every state dump against Tibia.exe's state at the same checkpoint ("state" equal, the window tree
     equal but for this run's addresses, Tibia.exe's vtable names paired with classes consistently);
  c. every frame against Tibia.exe's (exact; on the pixels the Vulkan engine marks as device-filtered
     up to 2 per channel, the user's decision of November 3, 2026; the recorded masks: the measured
     frame rate);
  d. the files written (created or changed, byte for byte) and removed, the files saved during the
     run (save), Tibia.cfg among them; the message boxes; the exit code.
  --check-only compares the results already in --out again (any platform, no client run).

The reference set is CipSoft-derived local data: never committed, pushed or uploaded. Default
<repository>\build\reference (gitignored); the orchestrator's set is the main checkout's
<repository>\build\reference (a worktree passes it with --reference).
A session is stale when its format is not FORMAT, or its Tibia.exe or data files' SHA-256 are not
the ones it was recorded with (compare refuses it; record it again). meta.json's display_modes (issue
#48) is optional in format 1: the sessions recorded before it lack it and were all recorded on the
machine of FORMAT1_DISPLAY_MODES, which they get instead.

Exit code 0 when every session matches, 1 on a difference or a stale session, 2 when a run failed.
"""

import argparse
import hashlib
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import testmode_dumps as td  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORMAT = "patella-reference 1"
REPLAY_HEADER = "# patella-replay 1"
# CLAUDE.md: the Tibia 7.72 client every reference is recorded from.
TIBIA_SHA256 = "1a82fe97a39e536c29327d6d5dde316775f537dcc01506831429b1a0433b76f8"
DEFAULT_REFERENCE = os.path.join(ROOT, "build", "reference")
DEFAULT_OUT = os.path.join(ROOT, "build", "reference-compare")
DEFAULT_EXE = os.path.join(ROOT, "build", "windows-x64", "Release", "PatellaTest.exe")
DATA_FILES = ("Tibia.dat", "Tibia.spr", "Tibia.pic")
# The proxy's ports (tools/proxy.py PROXY_LOGIN_PORT, PROXY_GAME_PORT): the record step's Tibia.exe was
# patched to the first, the character list it got names the second.
LOGIN_PORT, GAME_PORT = 7271, 7272
# The CPU driver next to PatellaTest.exe (cmake/Vulkan.cmake).
CPU_DRIVER_FILES = ("vulkan_lvp.dll", "lvp_icd.x86_64.json")
# The time zone of both clients' local time (error.txt's ctime, Default.txt's "Channel saved at").
TIME_ZONE = "UTC0"
# The x64 client's own settings file (issue #31), which Tibia.exe does not have: listed, not compared
# (compare_files.py PATELLA_INI).
PATELLA_INI = "Patella.ini"
REGION_TOLERANCE = 2
# Issue #48: the display modes of a session that has no display_modes in its meta.json (format 1,
# recorded before issue #48, November 3, 2026): the modes the recording machine reported then, as
# Tibia.exe's Graphics Options dialog listed them (DirectDraw's EnumDisplayModes; the same list as
# EnumDisplaySettings', at least 640x480 and 16 bits, each once: issue #20). The traffic session's frame
# 3 shows the first seven; the machine reports the same 24 again on November 4 (the 1176x664 mode of
# issue #48 has gone).
FORMAT1_DISPLAY_MODES = [[640, 480, 32], [720, 480, 32], [720, 576, 32], [800, 600, 32], [1024, 768, 32],
	[1152, 864, 32], [1280, 720, 32], [1280, 768, 32], [1280, 800, 32], [1280, 960, 32], [1280, 1024, 32],
	[1360, 768, 32], [1366, 768, 32], [1440, 1080, 32], [1600, 900, 32], [1600, 1024, 32], [1600, 1200, 32],
	[1680, 1050, 32], [1920, 1080, 32], [1920, 1200, 32], [1920, 1440, 32], [2048, 1536, 32], [2560, 1440, 32],
	[2560, 1600, 32]]
DISPLAY_MODES_HEADER = "# patella-display-modes 1"  # src/testmode/DisplayModes.h
DISPLAY_MODES_FILE = "display-modes.txt"
# The sessions, in recording order ("report" needs "error"'s error.txt).
SESSIONS = ["traffic", "options", "walk", "error", "error-login", "report", "patch", "patch-exe"]
# Which tool's session each one is (the record/compare split of the three tools).
SESSION_TOOLS = {"traffic": ("compare_traffic", "compare_rendering"), "options": ("compare_files",),
	"walk": ("compare_files",), "error": ("compare_files",), "error-login": ("compare_files",),
	"report": ("compare_files",), "patch": ("compare_files",), "patch-exe": ("compare_files",)}
OPCODE_LOGIN = 0x01


def tool_sessions(tool):
	"""The sessions of a comparison tool (compare_traffic, compare_files, compare_rendering)."""
	return [name for name in SESSIONS if tool in SESSION_TOOLS[name]]


class ReferenceError_(Exception):
	pass


def log(text):
	print(time.strftime("%H:%M:%S ") + text, flush=True)


def file_sha(path):
	digest = hashlib.sha256()
	with open(path, "rb") as f:
		for chunk in iter(lambda: f.read(1 << 20), b""):
			digest.update(chunk)
	return digest.hexdigest()


def write_json(path, value, indent=None):
	with open(path, "w", encoding="utf-8", newline="\n") as f:
		json.dump(value, f, indent=indent)
		f.write("\n")


def load_events(path):
	with open(path, "r", encoding="utf-8") as f:
		return [json.loads(line) for line in f if line.strip()]


# --- the reference set ----------------------------------------------------------------------------

def session_dir(reference, name):
	return os.path.join(reference, name)


def load_meta(folder):
	path = os.path.join(folder, "meta.json")
	if not os.path.isfile(path):
		raise ReferenceError_("%s: no meta.json (not recorded)" % folder)
	with open(path, encoding="utf-8") as f:
		return json.load(f)


def staleness(meta, data_dir=ROOT):
	"""Why a recorded session cannot be compared (empty: it can)."""
	problems = []
	if meta.get("format") != FORMAT:
		problems.append("format %r, this tool reads %r" % (meta.get("format"), FORMAT))
	if meta.get("tibia_sha256") != TIBIA_SHA256:
		problems.append("recorded from a Tibia.exe with SHA-256 %s, not %s" % (meta.get("tibia_sha256"), TIBIA_SHA256))
	exe = os.path.join(data_dir, "Tibia.exe")
	if os.path.isfile(exe) and file_sha(exe) != meta.get("tibia_sha256"):
		problems.append("%s has another SHA-256 than the recording's Tibia.exe" % exe)
	for name, digest in (meta.get("data_sha256") or {}).items():
		path = os.path.join(data_dir, name)
		if not os.path.isfile(path):
			problems.append("no %s" % path)
		elif file_sha(path) != digest:
			problems.append("%s has another SHA-256 than the recording's" % path)
	if not meta.get("data_sha256"):
		problems.append("no data files' SHA-256 in meta.json")
	return problems


# --- the display modes (issue #48, src/testmode/DisplayModes.h) -----------------------------------

def display_modes(meta):
	"""(modes, source): the display modes a session's client lists, [width, height, bits per pixel] in
	EnumDisplaySettings' order: meta.json's display_modes, or for a session recorded before issue #48
	FORMAT1_DISPLAY_MODES."""
	modes = meta.get("display_modes")
	if modes is None:
		return [list(m) for m in FORMAT1_DISPLAY_MODES], "the format 1 default (recorded before issue #48)"
	if not isinstance(modes, list) or not modes or any(not isinstance(m, list) or len(m) != 3
			or not all(isinstance(v, int) and v > 0 for v in m) for m in modes):
		raise ReferenceError_("meta.json: display_modes is not a list of [width, height, bits per pixel]")
	return [list(m) for m in modes], "meta.json"


def display_modes_text(modes):
	"""The --test-display-modes file of PatellaTest.exe (src/testmode/DisplayModes.h)."""
	return "\n".join([DISPLAY_MODES_HEADER] + ["%d %d %d" % tuple(m) for m in modes]) + "\n"


def listed_modes(modes):
	"""The modes as both clients' dialogs list them (vulkan/SurfaceVulkan.cpp GetFullscreenModes, the
	DirectDraw callback): at least 640x480 and 16 bits, each size and depth once, in order."""
	out = []
	for m in modes:
		m = list(m)
		if m[0] >= 640 and m[1] >= 480 and m[2] >= 16 and m not in out:
			out.append(m)
	return out


# --- the server side for the test client (src/testmode/ServerReplay.h) ----------------------------

def connection_key(event):
	return (event["kind"], event["conn_ordinal"])


def replay_text(source, reference, listen=((LOGIN_PORT, "login"), (GAME_PORT, "game"))):
	"""The replay file of a session: per connection the events of the recording the proxy replayed
	(source, its order of client and server packets), each server packet and close with the virtual time
	at which the proxy sent it to Tibia.exe in the reference run (reference: the proxy's events there,
	"vt" and "replayed"). A connection's schedule ends before the first server event Tibia.exe never
	got (it had closed the connection, or crashed)."""
	sent = {}
	for e in reference:
		if e["dir"] in ("s2c", "close") and "replayed" in e:
			sent[connection_key(e) + (e["replayed"],)] = e
	order, events = [], {}
	for e in source:
		key = connection_key(e)
		if key not in events:
			order.append(key)
			events[key] = []
		events[key].append(e)
	lines = [REPLAY_HEADER]
	lines += ["listen %s %d" % (kind, port) for port, kind in listen]
	for key in order:
		kind, ordinal = key
		for index, e in enumerate(events[key]):
			if e["dir"] == "c2s":
				lines.append("%s %d c2s" % (kind, ordinal))
				continue
			if e["dir"] == "close" and e.get("side") != "server":
				lines.append("%s %d close client" % (kind, ordinal))
				continue
			got = sent.get(key + (index,))
			if got is None or got.get("vt") is None:
				break
			if e["dir"] == "s2c":
				lines.append("%s %d s2c %d %s" % (kind, ordinal, got["vt"], e.get("plain") or "-"))
			else:
				lines.append("%s %d close server %d" % (kind, ordinal, got["vt"]))
	return "\n".join(lines) + "\n"


# --- packets --------------------------------------------------------------------------------------

def fields_end(plain_hex):
	"""Where the fields of a first packet's RSA block end (tools/proxy.py parse_first, on the block as
	it was before RSA encryption)."""
	body = bytes.fromhex(plain_hex)
	header = 17 if body[0] == OPCODE_LOGIN else 5
	block = body[header:]

	def string_end(pos):
		return pos + 2 + struct.unpack_from("<H", block, pos)[0]

	if body[0] == OPCODE_LOGIN:
		return string_end(17 + 4)
	return string_end(string_end(22))


def masked_first(plain_hex):
	body = bytes.fromhex(plain_hex)
	header = 17 if body[0] == OPCODE_LOGIN else 5
	end = fields_end(plain_hex)
	return "%s rsa[00 <key:16> %s <random:%d>]" % (body[:header].hex(), body[header + 17:header + end].hex(),
		128 - end)


def compare_packets(records, events, limit=10):
	"""(differences, notes, summary): the test client's packet log against Tibia.exe's packets in the
	reference run (the proxy's events): per connection the client packets equal in order, payload and
	padding, the first packets with the whole RSA block; the server packets the client decrypted equal to
	the ones Tibia.exe was sent, from the first (the ones after the client's last are listed)."""
	ours, theirs = {}, {}
	for r in records:
		ours.setdefault((r["kind"], r["ordinal"]), {"c2s": [], "s2c": []})[r["dir"]].append(
			(r["payload"], "" if r["plain"] else r["pad"], r["plain"]))
	for e in events:
		if e["dir"] not in ("c2s", "s2c"):
			continue
		first = "fields_end" in e
		theirs.setdefault(connection_key(e), {"c2s": [], "s2c": []})[e["dir"]].append(
			(e["plain"], "" if first or e["dir"] == "s2c" else e.get("pad", ""), first))
	differences, notes = [], []
	summary = {"connections": 0, "c2s": 0, "s2c": 0, "unread": 0, "masked_only": 0}
	for key in sorted(set(ours) | set(theirs)):
		label = "%s %d" % key
		a = ours.get(key, {"c2s": [], "s2c": []})
		b = theirs.get(key, {"c2s": [], "s2c": []})
		summary["connections"] += 1
		summary["c2s"] += len(a["c2s"])
		summary["s2c"] += len(a["s2c"])
		for i in range(max(len(a["c2s"]), len(b["c2s"]))):
			x = a["c2s"][i] if i < len(a["c2s"]) else None
			y = b["c2s"][i] if i < len(b["c2s"]) else None
			if x is not None and y is not None and x[:2] == y[:2]:
				continue
			if x is not None and y is not None and x[2] and y[2] and masked_first(x[0]) == masked_first(y[0]):
				summary["masked_only"] += 1
				text = "  %s c2s #%d: equal but for the XTEA key and the RSA block's random bytes" % (label, i)
			elif x is not None and y is not None and x[0] == y[0]:
				summary["masked_only"] += 1
				text = "  %s c2s #%d: equal but for the padding (%s against %s)" % (label, i, x[1], y[1])
			else:
				text = "  %s c2s #%d: test client %s\n      Tibia.exe   %s" % (label, i, x and x[:2], y and y[:2])
			if len(differences) < limit:
				differences.append(text)
		read, passed = a["s2c"], [(p, "", False) for p, _, _ in b["s2c"]]
		read = [(p, "", False) for p, _, _ in read]
		if read != passed[:len(read)]:
			for i, x in enumerate(read):
				y = passed[i] if i < len(passed) else None
				if x != y and len(differences) < limit:
					differences.append("  %s s2c #%d: test client %s\n      Tibia.exe got %s" % (label, i, x[0][:80],
						y[0][:80] if y else None))
		elif len(passed) > len(read):
			summary["unread"] += len(passed) - len(read)
			notes.append("  %s: %d server packets after the client's last one (not read: the connection closed)"
				% (label, len(passed) - len(read)))
	return differences, notes, summary


# --- states ---------------------------------------------------------------------------------------

def compare_gui(orig, ours, mapping, rects=()):
	"""Window trees (compare_traffic.py compare_gui): everything equal but for this run's addresses, a
	vtable name of Tibia.exe's (vt_<address>, "(modal)") paired with the same class everywhere. rects:
	the recorded masks of the checkpoint (the advanced options' device choice of issue #31, the frame
	rate): the texts of the windows inside them are not compared, and a window that lies inside the same
	rectangle in both clients may differ in place and size there (#31's device line is wider)."""
	out = []
	if len(orig) != len(ours):
		out.append("%d windows against %d" % (len(ours), len(orig)))

	def within(w, r):
		x0, y0, x1, y1 = r
		return x0 <= w["x"] and w["x"] + w["w"] <= x1 and y0 <= w["y"] and w["y"] + w["h"] <= y1

	for i, (o, p) in enumerate(zip(orig, ours)):
		inside = any(within(o, r) for r in rects)
		both = any(within(o, r) and within(p, r) for r in rects)
		keys = ("depth",) if both else ("depth", "x", "y", "w", "h")
		for key in keys + (() if inside else ("text",)):
			if o.get(key) != p.get(key):
				out.append("window #%d (%s/%s): %s %r against %r" % (i, p["class"], o["class"], key, p.get(key), o.get(key)))
		if o["class"].startswith("vt_") or o["class"] == "(modal)":
			if mapping.setdefault(o["class"], p["class"]) != p["class"]:
				out.append("window #%d: %s is %s here, %s elsewhere" % (i, o["class"], p["class"], mapping[o["class"]]))
		elif o["class"] != p["class"]:
			out.append("window #%d: class %s against %s" % (i, p["class"], o["class"]))
		if len(out) > 20:
			break
	return out


def compare_states(dumps_dir, states_dir, mapping, masks=None, limit=20):
	"""(differences, lines): the test client's state-<NNN>.json against Tibia.exe's."""
	ours = td.numbered(dumps_dir, "state", ".json")
	theirs = td.numbered(states_dir, "state", ".json")
	differences, lines = [], []
	if sorted(ours) != sorted(theirs):
		differences.append("  %d state dumps, the reference has %d" % (len(ours), len(theirs)))
	for step in sorted(set(ours) & set(theirs)):
		a, b = td.load_json(ours[step]), td.load_json(theirs[step])
		found = td.diff_values(a["state"], b["state"], "state", None, limit)
		found += compare_gui(b["gui"], a["gui"], mapping, [tuple(r) for r in (masks or {}).get(str(step), [])])
		lines.append("  state %3d %-20s %s" % (step, b.get("name", ""), "equal" if not found else "DIFFERENT"))
		differences.extend("  state %d (%s): %s" % (step, b.get("name", ""), d) for d in found[:limit])
	return differences, lines


# --- frames ---------------------------------------------------------------------------------------

def in_rects(x, y, rects):
	return any(x0 <= x < x1 and y0 <= y < y1 for x0, y0, x1, y1 in rects)


def diff_frames(ours, theirs, filtered=None, rects=(), tolerance=REGION_TOLERANCE):
	"""(count, largest, within, masked): pixels that differ outside the rectangles, beyond the tolerance on
	the filtered pixels (exact elsewhere); the largest channel difference; the pixels within the
	tolerance; the differing pixels inside the rectangles."""
	(wa, ha, ra), (wb, hb, rb) = ours, theirs
	if (wa, ha) != (wb, hb):
		return (wa * ha, 255, 0, 0)
	count = largest = within = masked = 0
	for y in range(ha):
		if ra[y] == rb[y]:
			continue
		row_a, row_b = ra[y], rb[y]
		mask_row = filtered[2][y] if filtered is not None else None
		for x in range(wa):
			i = x * 3
			if row_a[i:i + 3] == row_b[i:i + 3]:
				continue
			if in_rects(x, y, rects):
				masked += 1
				continue
			spread = max(abs(row_a[i + c] - row_b[i + c]) for c in range(3))
			largest = max(largest, spread)
			if mask_row is not None and mask_row[i] != 0 and spread <= tolerance:
				within += 1
				continue
			count += 1
	return count, largest, within, masked


def compare_frames(dumps_dir, frames_dir, masks, tolerance=REGION_TOLERANCE):
	"""(differences, lines): the test client's frame-<NNN>.png against Tibia.exe's."""
	ours = td.numbered(dumps_dir, "frame", ".png")
	ours = {k: v for k, v in ours.items() if not v.endswith(".filtered.png")}
	theirs = td.numbered(frames_dir, "frame", ".png")
	differences, lines = [], []
	if sorted(ours) != sorted(theirs):
		differences.append("  %d frames, the reference has %d" % (len(ours), len(theirs)))
	for step in sorted(set(ours) & set(theirs)):
		a, b = td.read_png(ours[step]), td.read_png(theirs[step])
		mask_path = os.path.join(dumps_dir, "frame-%03d.filtered.png" % step)
		filtered = td.read_png(mask_path) if os.path.isfile(mask_path) else None
		rects = [tuple(r) for r in masks.get(str(step), [])]
		count, largest, within, hidden = diff_frames(a, b, filtered, rects, tolerance)
		text = "identical" if a == b else "%s (largest channel difference %d, %d pixels within the tolerance%s)" % (
			"equal" if count == 0 else "%d PIXELS DIFFER" % count, largest, within,
			", %d inside the masks %s" % (hidden, rects) if hidden else "")
		lines.append("  frame %3d %s" % (step, text))
		if count:
			differences.append("  frame %d: %d pixels differ (largest channel difference %d)" % (step, count, largest))
	return differences, lines


# --- files ----------------------------------------------------------------------------------------

def snapshot(folder, skip=()):
	out = {}
	for name in sorted(os.listdir(folder)):
		full = os.path.join(folder, name)
		if os.path.isfile(full) and name not in skip:
			out[name] = file_sha(full)
	return out


def compare_files(written_dir, removed, meta, reference):
	"""(differences, lines): the files the test client wrote and removed against Tibia.exe's."""
	differences, lines = [], []
	ours = sorted(os.listdir(written_dir)) if os.path.isdir(written_dir) else []
	if PATELLA_INI in ours:
		ours.remove(PATELLA_INI)
		with open(os.path.join(written_dir, PATELLA_INI), "rb") as f:
			lines.append("  %s (the x64 client's own, not compared): %r" % (PATELLA_INI, f.read()))
	theirs = sorted(meta.get("written", []))
	if ours != theirs:
		differences.append("  files written: %s, the reference %s" % (ours, theirs))
	for name in sorted(set(ours) & set(theirs)):
		with open(os.path.join(written_dir, name), "rb") as f:
			a = f.read()
		with open(os.path.join(reference, "files", name), "rb") as f:
			b = f.read()
		lines.append("  %-14s %s (%d bytes)" % (name, "identical" if a == b else "DIFFERENT", len(a)))
		if a != b:
			at = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
			differences.append("  %s: differs from byte %d (%d bytes against %d): %r against %r" % (name, at, len(a),
				len(b), a[at:at + 40], b[at:at + 40]))
	if sorted(removed) != sorted(meta.get("removed", [])):
		differences.append("  files removed: %s, the reference %s" % (sorted(removed), sorted(meta.get("removed", []))))
	return differences, lines


def compare_saved(dumps_dir, reference):
	differences, lines = [], []
	ours = sorted(n for n in os.listdir(dumps_dir) if n.startswith("saved-")) if os.path.isdir(dumps_dir) else []
	saved = os.path.join(reference, "saved")
	theirs = sorted(os.listdir(saved)) if os.path.isdir(saved) else []
	if ours != theirs:
		differences.append("  saved files %s, the reference %s" % (ours, theirs))
	for name in sorted(set(ours) & set(theirs)):
		with open(os.path.join(dumps_dir, name), "rb") as f:
			a = f.read()
		with open(os.path.join(saved, name), "rb") as f:
			b = f.read()
		lines.append("  %-22s %s" % (name, "identical" if a == b else "DIFFERENT"))
		if a != b:
			differences.append("  %s differs" % name)
	return differences, lines


def load_messages(dumps_dir):
	path = os.path.join(dumps_dir, "messages.txt")
	if not os.path.isfile(path):
		return []
	with open(path, encoding="utf-8") as f:
		return [[m["caption"], m["text"]] for m in (json.loads(line) for line in f if line.strip())]


def normalize_box_text(text):
	"""A message box's text as the record step reads it from the box (GetWindowText: CRLF) and as the
	client passes it (LF)."""
	return text.replace("\r\n", "\n").rstrip("\n")


# --- compare --------------------------------------------------------------------------------------

def ports_free(ports):
	busy = []
	for port in ports:
		sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			sock.bind(("127.0.0.1", port))
		except OSError:
			busy.append(port)
		finally:
			sock.close()
	return busy


def prepare_run(folder, exe, meta, reference, data_dir):
	"""A fresh run folder: the data files, the patched client, its CPU driver, the session's Tibia.cfg
	and seed files. Returns the snapshot before the run."""
	if os.path.isdir(folder):
		shutil.rmtree(folder)
	os.makedirs(folder)
	for name in DATA_FILES:
		shutil.copyfile(os.path.join(data_dir, name), os.path.join(folder, name))
	target = os.path.join(folder, os.path.basename(exe))
	result = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "patch_client.py"), exe, target,
		"--host", "127.0.0.1", "--port", str(LOGIN_PORT)], capture_output=True, text=True)
	if result.returncode != 0:
		raise ReferenceError_("patch_client.py failed: %s" % (result.stderr.strip() or result.stdout.strip()))
	for name in CPU_DRIVER_FILES:
		source = os.path.join(os.path.dirname(exe), name)
		if os.path.isfile(source):
			shutil.copyfile(source, os.path.join(folder, name))
	shutil.copyfile(os.path.join(reference, "Tibia.cfg.start"), os.path.join(folder, "Tibia.cfg"))
	seed = os.path.join(reference, "seed")
	if os.path.isdir(seed):
		for name in os.listdir(seed):
			shutil.copyfile(os.path.join(seed, name), os.path.join(folder, name))
	return snapshot(folder, skip=(os.path.basename(exe),))


def run_session(name, reference, exe, out, vulkan, timeout, data_dir):
	"""One test client run of a session: <out>\\<name>\\run (the run folder), dumps (--test-out),
	files (what it wrote), run.json (exit code, removed files, timings)."""
	meta = load_meta(reference)
	base = os.path.join(out, name)
	folder, dumps, written = (os.path.join(base, n) for n in ("run", "dumps", "files"))
	for path in (dumps, written):
		if os.path.isdir(path):
			shutil.rmtree(path)
		os.makedirs(path)
	exe_name = os.path.basename(exe)
	before = prepare_run(folder, exe, meta, reference, data_dir)
	replay = os.path.join(reference, "replay.txt")
	modes_path = os.path.join(base, DISPLAY_MODES_FILE)
	with open(modes_path, "w", encoding="ascii", newline="\n") as f:
		f.write(display_modes_text(display_modes(meta)[0]))
	command = [os.path.join(folder, exe_name), "--test-script", os.path.join(reference, "session.script"),
		"--test-out", dumps, "--test-log", os.path.join(dumps, "driver.log"), "--test-display-modes", modes_path]
	if meta.get("server"):
		busy = ports_free((LOGIN_PORT, GAME_PORT))
		if busy:
			raise ReferenceError_("ports %s are in use (a proxy or another client?)" % busy)
		command += ["--test-replay", replay]
	env = dict(os.environ, PATELLA_VULKAN=vulkan, TZ=TIME_ZONE)
	log("=== %s: %s" % (name, " ".join(command)))
	start = time.time()
	with open(os.path.join(base, "client-output.txt"), "w", encoding="utf-8") as output:
		proc = subprocess.Popen(command, cwd=folder, env=env, stdout=output, stderr=subprocess.STDOUT)
		try:
			code = proc.wait(timeout)
		except subprocess.TimeoutExpired:
			proc.kill()
			proc.wait()
			raise ReferenceError_("%s did not exit within %d s" % (exe_name, timeout))
	seconds = time.time() - start
	after = snapshot(folder, skip=(exe_name,))
	for file, digest in after.items():
		if before.get(file) != digest:
			shutil.copyfile(os.path.join(folder, file), os.path.join(written, file))
	removed = sorted(n for n in before if n not in after)
	result = {"exit": code, "removed": removed, "seconds": round(seconds, 1), "command": command}
	write_json(os.path.join(base, "run.json"), result, indent=1)
	log("%s exited with code %d after %.0f s" % (exe_name, code, seconds))
	return result


def check_session(name, reference, out):
	"""(differences, lines) of a session's run in <out>\\<name> against its reference."""
	meta = load_meta(reference)
	base = os.path.join(out, name)
	with open(os.path.join(base, "run.json"), encoding="utf-8") as f:
		result = json.load(f)
	dumps = os.path.join(base, "dumps")
	differences, lines = [], []

	def part(title, found):
		diffs, texts = found[0], found[1]
		lines.append(" %s: %s" % (title, "no differences" if not diffs else "%d DIFFERENCES" % len(diffs)))
		lines.extend("  " + t for t in texts)
		lines.extend(diffs)
		differences.extend("%s, %s: %s" % (name, title, d.strip()) for d in diffs)

	modes, source = display_modes(meta)
	lines.append(" display modes: %d from %s" % (len(modes), source))
	if result["exit"] != meta.get("exit", 0):
		part("exit code",(["  %d, Tibia.exe's %d" % (result["exit"], meta.get("exit", 0))], []))
	else:
		part("exit code", ([], ["%d" % result["exit"]]))
	reference_events = load_events(os.path.join(reference, "reference.jsonl")) if meta.get("server") else []
	packets_path = os.path.join(dumps, "packets.log")
	records = td.load_packet_log(packets_path) if os.path.isfile(packets_path) else []
	diffs, notes, summary = compare_packets(records, reference_events)
	part("a. packets", (diffs, ["%d connections, %d client packets, %d server packets read" % (summary["connections"],
		summary["c2s"], summary["s2c"])] + [n.strip() for n in notes]))
	mapping = {}
	part("b. states", compare_states(dumps, os.path.join(reference, "states"), mapping, meta.get("state_masks", {})))
	part("c. frames", compare_frames(dumps, os.path.join(reference, "frames"), meta.get("frame_masks", {})))
	found = compare_files(os.path.join(base, "files"), result["removed"], meta, reference)
	saved = compare_saved(dumps, reference)
	part("d. files", (found[0] + saved[0], found[1] + saved[1]))
	ours = [[c, normalize_box_text(t)] for c, t in load_messages(dumps)]
	theirs = [[c, normalize_box_text(t)] for c, t in meta.get("messages", [])]
	part("d. message boxes", ([] if ours == theirs else ["  %s, Tibia.exe's %s" % (ours, theirs)],
		["%d" % len(ours)] + ["%r" % m[0] for m in ours]))
	log_path = os.path.join(dumps, "driver.log")
	if os.path.isfile(log_path):
		with open(log_path, encoding="utf-8", errors="replace") as f:
			given_up = [line.strip() for line in f if "gave up waiting" in line or "not recorded" in line]
		if given_up:
			part("replay", (["  " + g for g in given_up], []))
		if "--test-display-modes" in result.get("command", []):
			part("display modes", taken_modes(log_path, modes))
	return differences, lines


def taken_modes(log_path, modes):
	"""(differences, lines): whether the client took the --test-display-modes list (the driver's log
	line; a test client before issue #48 ignores the option and lists this machine's modes)."""
	with open(log_path, encoding="utf-8", errors="replace") as f:
		taken = [line.strip() for line in f if line.startswith("testmode: display modes ")]
	if not taken:
		return ["  the client did not take --test-display-modes (no \"testmode: display modes\" in its log)"], []
	if not taken[0].endswith(", %d modes" % len(modes)):
		return ["  the client took another list: %s" % taken[0]], []
	return [], [taken[0]]


def selected(argument):
	names = SESSIONS if argument in (None, "", "all") else [n for n in argument.split(",") if n]
	for n in names:
		if n not in SESSIONS:
			raise SystemExit("unknown session %r (%s)" % (n, ", ".join(SESSIONS)))
	return names


def compare_main(args):
	reference = os.path.abspath(args.reference)
	out = os.path.abspath(args.out)
	exe = os.path.abspath(args.exe)
	os.makedirs(out, exist_ok=True)
	report = []

	def say(text=""):
		print(text, flush=True)
		report.append(text)

	failed_runs, differing, stale = [], [], []
	say("Reference comparison (issue #23), %s" % time.strftime("%Y-%m-%d %H:%M"))
	say("reference %s; test client %s; PATELLA_VULKAN=%s; results in %s" % (reference, exe, args.vulkan, out))
	for name in selected(args.sessions):
		folder = session_dir(reference, name)
		say()
		try:
			meta = load_meta(folder)
		except ReferenceError_ as e:
			say("== %s: %s" % (name, e))
			stale.append(name)
			continue
		problems = staleness(meta, args.data)
		say("== %s (%s; recorded %s, %d dumps, %d frames)" % (name, " and ".join(SESSION_TOOLS[name]),
			meta.get("recorded"), meta.get("dumps", 0), meta.get("frames", 0)))
		if problems:
			say("  STALE: " + "; ".join(problems))
			stale.append(name)
			continue
		if not args.check_only:
			try:
				run_session(name, folder, exe, out, args.vulkan, args.timeout, args.data)
			except (ReferenceError_, OSError) as e:
				traceback.print_exc()
				say("  RUN FAILED: %s" % e)
				failed_runs.append(name)
				continue
		try:
			differences, lines = check_session(name, folder, out)
		except (ReferenceError_, OSError, ValueError, td.DumpError) as e:
			traceback.print_exc()
			say("  CHECK FAILED: %s" % e)
			failed_runs.append(name)
			continue
		for line in lines:
			say(line)
		say("  %s: %s" % (name, "matches the reference" if not differences else "%d DIFFERENCES" % len(differences)))
		if differences:
			differing.append(name)
	say()
	say("RESULT: %s" % ("runs failed: %s" % failed_runs if failed_runs else
		"differences in %s" % differing if differing else "stale: %s" % stale if stale else
		"every session matches its reference"))
	with open(os.path.join(out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(report) + "\n")
	return 2 if failed_runs else 1 if differing or stale else 0


def list_main(args):
	reference = os.path.abspath(args.reference)
	for name in SESSIONS:
		folder = session_dir(reference, name)
		try:
			meta = load_meta(folder)
		except ReferenceError_:
			print("%-12s not recorded" % name)
			continue
		size = sum(os.path.getsize(os.path.join(d, f)) for d, _, files in os.walk(folder) for f in files)
		problems = staleness(meta, args.data)
		print("%-12s %s, recorded %s, %d dumps, %d frames, %d written files, %.1f MB%s" % (name, meta.get("format"),
			meta.get("recorded"), meta.get("dumps", 0), meta.get("frames", 0), len(meta.get("written", [])),
			size / 1e6, "; STALE: " + "; ".join(problems) if problems else ""))
	return 0


# --- self-test ------------------------------------------------------------------------------------

def self_test():
	"""The pure parts on made-up data: the replay file, the packet and frame comparisons."""
	failures = []

	def check(what, condition):
		print("  %-60s %s" % (what, "ok" if condition else "FAILED"))
		if not condition:
			failures.append(what)

	block = bytes([0]) + bytes(range(1, 17)) + struct.pack("<I", 772001) + b"\x07\x00patella" + bytes(98)
	login = (bytes([0x01, 0x02, 0x00, 0x04, 0x03]) + bytes(12) + block).hex()
	source = [
		{"kind": "login", "conn_ordinal": 1, "dir": "c2s", "plain": login, "fields_end": 30, "t": 0},
		{"kind": "login", "conn_ordinal": 1, "dir": "s2c", "plain": "14aa", "t": 0.1},
		{"kind": "login", "conn_ordinal": 1, "dir": "close", "side": "server", "t": 0.2},
		{"kind": "game", "conn_ordinal": 1, "dir": "c2s", "plain": "0a", "fields_end": 30, "t": 1},
		{"kind": "game", "conn_ordinal": 1, "dir": "s2c", "plain": "", "t": 1.1},
		{"kind": "game", "conn_ordinal": 1, "dir": "c2s", "plain": "14", "t": 2},
		{"kind": "game", "conn_ordinal": 1, "dir": "s2c", "plain": "ff", "t": 2.1},
	]
	reference = [
		{"kind": "login", "conn_ordinal": 1, "dir": "s2c", "plain": "14aa", "replayed": 1, "vt": 0},
		{"kind": "login", "conn_ordinal": 1, "dir": "close", "side": "server", "replayed": 2, "vt": 2000},
		{"kind": "game", "conn_ordinal": 1, "dir": "s2c", "plain": "", "replayed": 1, "vt": 4000},
	]
	text = replay_text(source, reference)
	check("replay file: header, listen lines, entries in order", text.splitlines() == [REPLAY_HEADER,
		"listen login 7271", "listen game 7272", "login 1 c2s", "login 1 s2c 0 14aa", "login 1 close server 2000",
		"game 1 c2s", "game 1 s2c 4000 -", "game 1 c2s"])
	check("first packet: the fields' end", fields_end(login) == 17 + 4 + 2 + 7)
	records = [{"kind": "login", "ordinal": 1, "dir": "c2s", "plain": True, "payload": login, "pad": ""},
		{"kind": "login", "ordinal": 1, "dir": "s2c", "plain": False, "payload": "14aa", "pad": "000000"}]
	events = [dict(source[0]), {"kind": "login", "conn_ordinal": 1, "dir": "s2c", "plain": "14aa"}]
	check("packets: equal", compare_packets(records, events)[0] == [])
	other = bytearray(bytes.fromhex(login))
	other[17 + 3] ^= 1  # a byte of the XTEA key
	records2 = [dict(records[0], payload=bytes(other).hex())] + records[1:]
	diffs, _, summary = compare_packets(records2, events)
	check("packets: another key is a difference (masked equal)", len(diffs) == 1 and summary["masked_only"] == 1)
	records3 = records + [{"kind": "login", "ordinal": 1, "dir": "c2s", "plain": False, "payload": "14", "pad": "00"}]
	check("packets: an extra client packet is a difference", len(compare_packets(records3, events)[0]) == 1)
	row = bytes([10, 20, 30] * 4)
	a = (4, 2, [row, row])
	b = (4, 2, [row, bytes([10, 20, 30, 10, 20, 31, 10, 20, 30, 10, 20, 30])])
	mask = (4, 2, [bytes(12), bytes([0, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0])])
	check("frames: identical", diff_frames(a, a)[0] == 0)
	check("frames: a pixel differs", diff_frames(a, b)[0] == 1)
	check("frames: within the tolerance on a filtered pixel", diff_frames(a, b, mask)[:3] == (0, 1, 1))
	check("frames: inside a mask rectangle", diff_frames(a, b, None, [(1, 1, 2, 2)])[3] == 1)
	check("gui: vtable names paired", compare_gui([{"depth": 0, "class": "vt_1", "x": 0, "y": 0, "w": 1, "h": 1}],
		[{"depth": 0, "class": "TFoo", "x": 0, "y": 0, "w": 1, "h": 1}], {}) == [])
	label = {"depth": 1, "class": "TLabel", "x": 10, "y": 10, "w": 131, "h": 12, "text": "Select graphics engine:"}
	wider = dict(label, w=200, text="Device: GPU")
	check("gui: a window inside a mask in both clients may change size and text",
		compare_gui([label], [wider], {}, [(10, 10, 210, 30)]) == [])
	check("gui: a window that leaves the mask is a difference",
		len(compare_gui([label], [dict(wider, w=250)], {}, [(10, 10, 210, 30)])) == 1)
	check("gui: outside a mask the size is compared", len(compare_gui([label], [wider], {})) == 2)
	check("staleness: another format", any("format" in p for p in staleness({"format": "x"}, tempfile.gettempdir())))
	# Issue #48: the display modes. A made-up machine's list (a 1176x664 mode, as on November 3) is never
	# the one the client gets: a session lists the modes it was recorded with.
	machine = [[640, 480, 32], [1176, 664, 32], [1280, 720, 32]]
	modes, source = display_modes({"format": FORMAT})
	check("display modes: a session without them gets the format 1 default",
		modes == FORMAT1_DISPLAY_MODES and "format 1" in source and [1176, 664, 32] not in modes)
	check("display modes: the default lists the traffic session's frame 3", modes[:7] == [[640, 480, 32],
		[720, 480, 32], [720, 576, 32], [800, 600, 32], [1024, 768, 32], [1152, 864, 32], [1280, 720, 32]])
	check("display modes: the default is a dialog's list", listed_modes(modes) == modes and len(modes) == 24)
	check("display modes: meta.json's list wins", display_modes({"display_modes": machine}) == (machine, "meta.json"))
	check("display modes: the file", display_modes_text(FORMAT1_DISPLAY_MODES[:2])
		== DISPLAY_MODES_HEADER + "\n640 480 32\n720 480 32\n")
	check("display modes: the dialog's filter", listed_modes([[320, 200, 32], [640, 480, 8], [800, 600, 32],
		[800, 600, 32], [800, 600, 16]]) == [[800, 600, 32], [800, 600, 16]])
	for bad in ([[640, 480]], [], [[640, 480, 0]], "640 480 32"):
		try:
			display_modes({"display_modes": bad})
			refused = False
		except ReferenceError_:
			refused = True
		check("display modes: a malformed list is refused (%r)" % (bad,), refused)
	folder = tempfile.mkdtemp(prefix="reference-self-test-")
	try:
		log_path = os.path.join(folder, "driver.log")
		with open(log_path, "w", encoding="utf-8") as f:
			f.write("testmode: script s, 3 events\ntestmode: display modes x/display-modes.txt, 24 modes\n")
		check("display modes: the client took the list", taken_modes(log_path, FORMAT1_DISPLAY_MODES)[0] == [])
		check("display modes: another length is a difference", len(taken_modes(log_path, machine)[0]) == 1)
		with open(log_path, "w", encoding="utf-8") as f:
			f.write("testmode: script s, 3 events\n")
		check("display modes: a client that ignored the option is a difference",
			len(taken_modes(log_path, FORMAT1_DISPLAY_MODES)[0]) == 1)
	finally:
		shutil.rmtree(folder, ignore_errors=True)
	print("self-test: %s" % ("passed" if not failures else "FAILED: %s" % failures))
	return 1 if failures else 0


def main(argv=None):
	parser = argparse.ArgumentParser(description="Reference recordings: record (Tibia.exe) and compare (the test client).")
	parser.add_argument("--self-test", action="store_true")
	sub = parser.add_subparsers(dest="command")
	for command in ("record", "compare", "list"):
		p = sub.add_parser(command)
		p.add_argument("--reference", default=DEFAULT_REFERENCE, help="the reference set (default %(default)s)")
		p.add_argument("--sessions", default="all", help="comma-separated: %s" % ", ".join(SESSIONS))
		p.add_argument("--data", default=ROOT, help="the folder with Tibia.dat, Tibia.spr, Tibia.pic (default the root)")
		if command == "record":
			p.add_argument("--live", default=os.path.join(ROOT, "build", "reference-run", "live-result"),
				help="where the live recordings go (default %(default)s)")
			p.add_argument("--reuse-live", action="store_true", help="use the live recordings already in --live")
		if command == "compare":
			p.add_argument("--exe", default=DEFAULT_EXE, help="the test client (default %(default)s)")
			p.add_argument("--out", default=DEFAULT_OUT, help="the results (default %(default)s)")
			p.add_argument("--vulkan", default="cpu", help="PATELLA_VULKAN for the client (default cpu: the pinned CPU driver)")
			p.add_argument("--timeout", type=int, default=600, help="seconds per run (default 600)")
			p.add_argument("--check-only", action="store_true", help="compare the results already in --out")
	args = parser.parse_args(argv)
	if args.self_test:
		return self_test()
	if args.command == "compare":
		return compare_main(args)
	if args.command == "list":
		return list_main(args)
	if args.command == "record":
		import reference_record  # Windows only: Tibia.exe, patcap.dll, the test server
		return reference_record.record_main(args)
	parser.print_help()
	return 2


if __name__ == "__main__":
	sys.exit(main())
