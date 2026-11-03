r"""Compare how Patella.exe and the original Tibia.exe take crafted and fuzzed server streams (issue #37).

Usage (from the repository or a worktree; python -I tools/build.py oracle-x86 first; no server is needed):
  python -I tools/compare_network.py [--out build\network\result] [--only a,b] [--list] [--repeat]
      [--recording build\network\orig-live.jsonl]

Both clients (patched copies in build\network\orig and build\network\patella, made like
tools/compare_traffic.py's) log in through tools/proxy.py, which answers from a recording of a live
session of Tibia.exe (issue #19's orig-live.jsonl, copied to build\network) changed into crafted and
fuzzed streams: the login server's answers (message of the day and character list as one or two
frames, errors, messages, overload, empty and long character lists), the game stream after the
character's login (the map frame and the frames after it), each delivered whole, fragmented (one
byte at a time, random pieces with delays: slow reads), coalesced (several frames in one send),
cut off by the server at every structural point (before, inside and after the size field, inside
and after each frame), with a frame size that is not a multiple of 8, a wrong XTEA key, inner
lengths above and below the frame's, empty frames, pings, a login wait, game errors, no answer at
all, and refused connections (login and game port). For every scenario and client the script
records:
  - the packets the client sent (decrypted; the key and RSA padding masked, proxy.masked);
  - the message boxes it showed (FatalError's critical error box: caption and text) and how it
    ended: the exit code after the box or after WM_CLOSE, or that it refused to close (in the
    game: the logout warning) and was killed;
  - the window tree (tools/client_state.py gui(): the dialogs, their captions and texts, the
    character list), the character list in memory and the client state (state(): connection
    state, player, map, creatures, containers, console) before WM_CLOSE;
  - error.txt (the crash time masked) and Tibia.cfg as written on exit.
and compares the two clients. A difference is reported; --repeat runs Tibia.exe a second time for
every scenario that differs, to show whether the difference is the client's run-to-run variation; the
scenarios marked variable (a wrong XTEA key: the garbage the client decrypts depends on its random session key)
always get the second run, and differ without failing only when Tibia.exe's two runs differ as well.
Report: <out>\report.txt and <out>\<scenario>\*.json; exit code 0 when nothing differs.
"""

import argparse
import ctypes
import json
import os
import random
import re
import struct
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_traffic as ct  # noqa: E402
import compare_files as cf  # noqa: E402
import proxy as px  # noqa: E402
import session_client as sc  # noqa: E402
import login_client as lc  # noqa: E402

ROOT = ct.ROOT
NETWORK = os.path.join(ROOT, "build", "network")
RECORDING = os.path.join(NETWORK, "orig-live.jsonl")
SETTLE = 2.0  # seconds after the proxy is idle before the client's state is taken
log = ct.log
RunError = ct.RunError


# --- the recording and the crafted streams -----------------------------------------------

def connection(events, kind, ordinal=1):
	return [dict(e) for e in events if e["kind"] == kind and e["conn_ordinal"] == ordinal]


def s2c(kind, ordinal, t, payload, **options):
	event = cf.s2c(kind, ordinal, t, payload)
	event.update(options)
	return event


def string(text):
	data = text.encode("latin-1")
	return struct.pack("<H", len(data)) + data


class Base:
	"""The pieces of the recording the scenarios are made of."""

	def __init__(self, events):
		login = connection(events, "login")
		self.login_first = login[0]
		answer = bytes.fromhex(login[1]["plain"])
		if answer[0] != 0x14:
			raise RunError("the recorded login answer does not start with the message of the day")
		length = struct.unpack_from("<H", answer, 1)[0]
		self.motd = answer[:3 + length]
		self.characters = answer[3 + length:]
		if self.characters[0] != 0x64:
			raise RunError("no character list after the message of the day")
		self.t = self.login_first["t"]
		self.login_events, self.game_events = cf.game_start(events)
		self.game_first = self.game_events[0]
		self.game_s2c = [e for e in self.game_events if e["dir"] == "s2c"]
		self.world = self.character_world()

	def character_world(self):
		"""(name, world, address, port) of the recorded list's first character, for crafted lists."""
		data = self.characters
		pos = 2
		name, pos = px.read_string(data, pos)
		world, pos = px.read_string(data, pos)
		return name, world, data[pos:pos + 4], data[pos + 4:pos + 6]

	def character_list(self, count, premium=0):
		name, world, address, port = self.world
		out = b"\x64" + bytes([count])
		for i in range(count):
			entry = name if i == 0 else ("Fuzz Character %02d" % i).encode("latin-1")
			out += struct.pack("<H", len(entry)) + entry + struct.pack("<H", len(world)) + world + address + port
		return out + struct.pack("<H", premium)

	def login(self, frames, close=True):
		"""The login connection: the client's first packet, the frames (dicts made by s2c), a close."""
		out = [dict(self.login_first)]
		t = self.t + 0.05
		for f in frames:
			f = dict(f)
			f["t"] = round(t, 4)
			out.append(f)
			t += 0.01
		if close:
			out.append(cf.close_event("login", 1, t + 0.1))
		return out

	def frame(self, payload, **options):
		return s2c("login", 1, 0, payload, **options)

	def game(self, transform=None, extra=None):
		"""The recorded login (normal) and game connection up to the fight modes; transform(list of the
		game connection's events) may change them."""
		game = [dict(e) for e in self.game_events]
		if transform is not None:
			game = transform(game)
		if extra:
			game += extra
		return self.login_events + game


def s2c_indexes(events):
	return [i for i, e in enumerate(events) if e["dir"] == "s2c"]


def with_options(index, **options):
	"""A transform: the index-th s2c frame of the game connection gets the options."""
	def transform(events):
		i = s2c_indexes(events)[index]
		events[i].update(options)
		return events
	return transform


def fragment_all(pieces, delay):
	def transform(events):
		for i in s2c_indexes(events):
			events[i]["pieces"] = pieces
			events[i]["piece_delay"] = delay
		return events
	return transform


def coalesce_runs(events):
	"""Every run of consecutive server frames is sent at once (the last of a run carries them)."""
	for i, e in enumerate(events):
		if e["dir"] == "s2c" and i + 1 < len(events) and events[i + 1]["dir"] == "s2c":
			e["hold"] = True
	return events


def cut_game(frame_index, offset):
	"""The server closes the game connection after `offset` bytes of the frame_index-th frame."""
	def transform(events):
		i = s2c_indexes(events)[frame_index]
		events[i]["cut"] = offset
		return events[:i + 1]
	return transform


def insert_game(frame_index, event):
	def transform(events):
		i = s2c_indexes(events)[frame_index]
		new = dict(event)
		new["t"] = events[i]["t"]
		return events[:i] + [new] + events[i:]
	return transform


def bad_size_frame(size=9):
	"""A frame whose size is not a multiple of 8 (PrepareNextPacket's TERROR)."""
	return (struct.pack("<H", size) + bytes(range(size))).hex()


WRONG_KEY = "00112233445566778899aabbccddeeff"


def scenarios(base):
	"""{name: (description, events or None (refused), flow, refuse)}."""
	out = {}

	def add(name, description, events, flow, refuse=(), variable=False):
		out[name] = (description, events, flow, refuse, variable)

	b = base
	motd, chars = b.motd, b.characters
	add("login-normal", "the recorded answer: message of the day and character list in one frame",
		b.login([b.frame(motd + chars)]), "login")
	add("login-two-frames", "message of the day and character list as two frames",
		b.login([b.frame(motd), b.frame(chars)]), "login")
	add("login-coalesced", "both frames in one send", b.login([b.frame(motd, hold=True), b.frame(chars)]), "login")
	add("login-bytewise", "both frames one byte at a time (slow reads)",
		b.login([b.frame(motd, pieces=[1], piece_delay=0.003), b.frame(chars, pieces=[1], piece_delay=0.003)]), "login")
	rng = random.Random(37)
	add("login-pieces", "both frames coalesced, in random pieces of 1-13 bytes",
		b.login([b.frame(motd, hold=True), b.frame(chars, pieces=[rng.randint(1, 13) for _ in range(40)],
			piece_delay=0.01)]), "login")
	add("login-empty-frame", "an empty frame (size 0) before the answer",
		b.login([b.frame(b"", frame_hex="0000"), b.frame(motd + chars)]), "login")
	add("login-error", "0x0a: an error text", b.login([b.frame(b"\x0a" + string("Account number or password is not correct."))]), "login")
	add("login-message", "0x0b: a message", b.login([b.frame(b"\x0b" + string("The server is going down for maintenance."))]), "login")
	add("login-overloaded", "0x28: the login server is overloaded", b.login([b.frame(b"\x28")]), "login")
	add("login-motd-only", "only the message of the day, then the server closes", b.login([b.frame(motd)]), "login")
	add("login-no-characters", "a character list without characters", b.login([b.frame(motd + b.character_list(0, 5))]), "login")
	add("login-many-characters", "a character list of 20 characters",
		b.login([b.frame(motd + b.character_list(20, 65535))]), "login")
	add("login-bad-size", "a frame of 9 bytes (not a multiple of 8)", b.login([b.frame(b"", frame_hex=bad_size_frame(9))]), "login")
	# A wrong key: the client decrypts with its own session key, which rand() makes anew in every run,
	# so the garbage it reads differs from run to run (variable: compared, and Tibia.exe run twice).
	add("login-wrong-key", "the answer encrypted with a wrong key", b.login([b.frame(motd + chars, xtea_key=WRONG_KEY)]),
		"login", variable=True)
	add("login-inner-long", "an inner length 100 bytes beyond the frame", b.login([b.frame(motd + chars, inner=len(motd + chars) + 100)]), "login")
	add("login-inner-short", "an inner length that ends inside the message of the day",
		b.login([b.frame(motd + chars, inner=len(motd) - 5)]), "login")
	add("login-silent", "the server accepts and never answers", b.login([], close=False), "login-silent")
	add("login-refused", "nothing listens on the login port", None, "login-refused", refuse=("login",))
	# the server closes the login connection at every structural point of a two-frame answer
	frame_size = 2 + ((2 + len(motd) + 7) // 8) * 8
	for name, frames in (
			("start", [b.frame(motd, cut=0)]),
			("size-1", [b.frame(motd, cut=1)]),
			("size", [b.frame(motd, cut=2)]),
			("body-8", [b.frame(motd, cut=10)]),
			("body-1", [b.frame(motd, cut=frame_size - 1)]),
			("frame-1", [b.frame(motd, cut=frame_size)]),
			("frame-2-size-1", [b.frame(motd, hold=True), b.frame(chars, cut=frame_size + 1)]),
			("frame-2-body", [b.frame(motd, hold=True), b.frame(chars, cut=frame_size + 2 + 16)]),
			("frame-2-body-1", [b.frame(motd, hold=True), b.frame(chars, cut=frame_size + 2 + ((2 + len(chars) + 7) // 8) * 8 - 1)])):
		add("login-cut-" + name, "the server closes after %s" % name, b.login(frames, close=False), "login")
	# the game connection
	first_size = len(bytes.fromhex(b.game_s2c[0]["plain"]))
	add("game-normal", "the recorded game start", b.game(), "game")
	add("game-bytewise", "every game frame one byte at a time", b.game(fragment_all([1], 0.0)), "game")
	add("game-pieces", "every game frame in pieces of 1-300 bytes, 10 ms apart",
		b.game(fragment_all([rng.randint(1, 300) for _ in range(30)], 0.01)), "game")
	add("game-coalesced", "consecutive game frames in one send", b.game(coalesce_runs), "game")
	for offset in (0, 1, 2, 3, 100, first_size // 2):
		add("game-cut-map-%d" % offset, "the server closes after %d bytes of the map frame" % offset,
			b.game(cut_game(0, offset)), "game")
	add("game-cut-second", "the server closes inside the second frame", b.game(cut_game(1, 5)), "game")
	add("game-bad-size", "a frame of 9 bytes after the map frame", b.game(insert_game(1, s2c("game", 1, 0, b"",
		frame_hex=bad_size_frame(9)))), "game")
	add("game-wrong-key", "the second frame encrypted with a wrong key", b.game(with_options(1, xtea_key=WRONG_KEY)), "game",
		variable=True)
	add("game-inner-long", "the second frame's inner length 40 bytes beyond the frame",
		b.game(with_options(1, inner=len(bytes.fromhex(b.game_s2c[1]["plain"])) + 40)), "game")
	add("game-inner-short", "the second frame's inner length 3 bytes short",
		b.game(with_options(1, inner=max(0, len(bytes.fromhex(b.game_s2c[1]["plain"])) - 3))), "game")
	# Empty frames: PrepareNextPacket takes the two bytes after an empty frame as its inner length,
	# so what follows depends on whether the next frame is already there (coalesced) or the buffer
	# still holds the login connection's bytes (alone): each case is sent as such, since how the
	# separate sends of three frames arrive depends on timing. Coalesced, the client reads the next
	# frame's undecrypted bytes as commands: they depend on the random session key (variable).
	add("game-empty-together", "an empty frame sent together with the map frame",
		b.game(insert_game(0, s2c("game", 1, 0, b"", frame_hex="0000", hold=True))), "game", variable=True)
	add("game-two-empty-together", "two empty frames sent together with the map frame (the second is the first's inner length)",
		b.game(lambda ev: insert_game(0, s2c("game", 1, 0, b"", frame_hex="0000", hold=True))(
			insert_game(0, s2c("game", 1, 0, b"", frame_hex="0000", hold=True))(ev))), "game")
	add("game-empty-alone", "an empty frame alone, the map frame 0.5 s later",
		b.game(lambda ev: with_options(1, pieces=[2, 100000], piece_delay=0.5)(
			insert_game(0, s2c("game", 1, 0, b"", frame_hex="0000", hold=True))(ev))), "game")
	add("game-empty-after", "an empty frame after the map frame, sent together with the next frame",
		b.game(insert_game(1, s2c("game", 1, 0, b"", frame_hex="0000", hold=True))), "game", variable=True)
	add("game-ping", "a ping (0x1e) after the game start", b.game(extra=[s2c("game", 1, b.game_events[-1]["t"] + 0.5, b"\x1e")]),
		"game")
	add("game-error", "0x14: a game error instead of the map",
		b.login_events + [dict(b.game_first), s2c("game", 1, b.game_first["t"] + 0.2, b"\x14" + string("Your character is banished."))],
		"game")
	add("game-message", "0x15: a message after the game start",
		b.game(extra=[s2c("game", 1, b.game_events[-1]["t"] + 0.5, b"\x15" + string("Server message."))]), "game")
	add("game-wait", "0x16: the waiting list (2 s) instead of the map",
		b.login_events + [dict(b.game_first), s2c("game", 1, b.game_first["t"] + 0.2, b"\x16" + string("Too many players online.") + b"\x02")],
		"game")
	add("game-refused", "nothing listens on the game port", b.login_events, "game", refuse=("game",))
	return out


# --- one run -------------------------------------------------------------------------------

def choose_character(driver):
	names = driver.mem.characters()
	if sc.CHARACTER not in names:
		raise RunError("%s is not in the character list %s" % (sc.CHARACTER, names))
	for _ in range(names.index(sc.CHARACTER)):
		driver.key(sc.VK_DOWN)
	driver.char(13)


def clean_gui(nodes):
	return [{k: n[k] for k in ("depth", "class", "x", "y", "w", "h")} | ({"text": n["text"]} if "text" in n else {})
		for n in nodes]


def read_file(client, name):
	path = os.path.join(cf.folder(client), name)
	if not os.path.isfile(path):
		return None
	with open(path, "rb") as f:
		return f.read().decode("latin-1")


def mask_error_txt(text):
	"""The crash time (ctime) of every report: two runs of the same client differ there (issue #21)."""
	if text is None:
		return None
	return re.sub(r"\r\n[A-Z][a-z]{2} [A-Z][a-z]{2} \d\d \d\d:\d\d:\d\d \d{4}\r\n", "\r\n<time>\r\n", text)


def finish(driver):
	"""The client's end: a message box (FatalError) is closed and the exit waited for; otherwise its
	window tree, character list and state are read, then WM_CLOSE, and a client that does not exit
	within 15 s (the logout warning in the game) is killed."""
	result = {}
	time.sleep(SETTLE)
	boxes = cf.dialog_windows(driver.proc.pid) if driver.proc.poll() is None else []
	if boxes or driver.proc.poll() is not None:
		if driver.proc.poll() is None:
			caption, texts, code = cf.close_error_box(driver)
			result["box"] = [caption, texts]
		else:
			code = driver.proc.returncode
		result["exit"] = code
		return result
	result["gui"] = clean_gui(driver.mem.gui())
	result["characters"] = driver.mem.characters()
	result["state"] = driver.mem.state()
	lc.post(driver.hwnd, lc.WM_CLOSE)
	deadline = time.time() + 15
	while time.time() < deadline and driver.proc.poll() is None:
		time.sleep(0.2)
	if driver.proc.poll() is None:
		boxes = cf.dialog_windows(driver.proc.pid)
		if boxes:
			caption, texts, code = cf.close_error_box(driver)
			result["box"] = [caption, texts]
			result["exit"] = code
			return result
		result["gui_after_close"] = clean_gui(driver.mem.gui())
		result["exit"] = "running (killed)"
		driver.kill()
	else:
		result["exit"] = driver.proc.returncode
	return result


def run(client, name, scenario, out):
	description, events, flow, refuse, variable = scenario
	before = cf.prepare(client)
	prefix = os.path.join(out, name, client)
	os.makedirs(os.path.dirname(prefix), exist_ok=True)
	proxy = px.Proxy(prefix + ".jsonl", px.load_private_key(ct.PEM), replay=events or [], log=log, refuse=refuse)
	driver = None
	result = {}
	try:
		with proxy:
			driver = cf.start(client)
			cf.enter_login(driver)
			if flow == "login-refused":
				time.sleep(20)  # four login servers (all this address), each refused
			elif flow == "login-silent":
				time.sleep(8)
			else:
				proxy.wait_idle(0.5, 60)
			if flow == "game" and driver.proc.poll() is None and not cf.dialog_windows(driver.proc.pid):
				try:
					choose_character(driver)
				except (RunError, client_state_error()) as e:
					result["note"] = "no character chosen: %s" % e
				time.sleep(1)
				proxy.wait_idle(0.5, 60)
				if refuse:
					time.sleep(8)
			result.update(finish(driver))
	finally:
		if driver is not None:
			driver.kill()
	result["packets"] = ct.client_packets(proxy.events)
	result["problems"] = proxy.mismatches + proxy.errors
	result["error.txt"] = mask_error_txt(read_file(client, "error.txt"))
	result["Tibia.cfg"] = read_file(client, "Tibia.cfg")
	files = cf.collect(client, before, prefix + "-files")
	result["files"] = sorted(files["written"]) + ["-" + n for n in files["removed"]]
	with open(prefix + ".json", "w", encoding="utf-8", newline="\n") as f:
		json.dump(result, f, indent=1, sort_keys=True)
	return result


def client_state_error():
	import client_state
	return client_state.ReadError


# --- comparison --------------------------------------------------------------------------

def compare(a, b):
	"""Differences between Tibia.exe's result a and Patella.exe's b."""
	out = []
	for key in ("exit", "box", "characters", "error.txt", "Tibia.cfg", "files", "note"):
		if a.get(key) != b.get(key):
			out.append("%s: %r against %r" % (key, a.get(key), b.get(key)))
	out += ct.diff_lists(a.get("packets", []), b.get("packets", []), "Tibia.exe", "Patella.exe")
	if "state" in a or "state" in b:
		out += ["state" + d for d in ct.diff_values(a.get("state"), b.get("state"))]
	for key in ("gui", "gui_after_close"):
		if key in a or key in b:
			mapping = {}
			out += ["%s: %s" % (key, d) for d in ct.compare_gui(a.get(key) or [], b.get(key) or [], mapping)]
	# The proxy's mismatches (client packets against the crafted stream) are compared; a socket error of
	# its own (a reset: the client ended with data unread, which depends on when it exited) is not
	# the client's behaviour (WinError 10053/10054).
	problems_a = [p for p in a.get("problems", []) if not re.search(r"WinError 1005[34]", p)]
	problems_b = [p for p in b.get("problems", []) if not re.search(r"WinError 1005[34]", p)]
	if problems_a != problems_b:
		out.append("proxy: %r against %r" % (problems_a, problems_b))
	return out


def summary(result):
	parts = ["exit %s" % result.get("exit")]
	if result.get("box"):
		parts.append("box %r" % result["box"][0])
	labels = [n["text"] for n in result.get("gui", []) if n.get("text") and n["class"] in ("TLabel", "vt_560bb8")]
	if labels:
		parts.append("labels %r" % labels[:4])
	parts.append("%d packets" % len(result.get("packets", [])))
	if result.get("error.txt"):
		match = re.search(r"\r\n([A-Za-z]+\.cpp \d+: [^\r]*)", result["error.txt"])
		parts.append("error.txt: %s" % (match.group(1) if match else "written"))
	return ", ".join(parts)


def main():
	ctypes.windll.user32.SetProcessDPIAware()
	parser = argparse.ArgumentParser(description="Compare Patella.exe and Tibia.exe on crafted server streams.")
	parser.add_argument("--out", default=os.path.join(NETWORK, "result"))
	parser.add_argument("--recording", default=RECORDING)
	parser.add_argument("--only", default=None, help="comma-separated scenario names (prefixes)")
	parser.add_argument("--list", action="store_true")
	parser.add_argument("--repeat", action="store_true", help="run Tibia.exe again where the clients differ")
	args = parser.parse_args()
	base = Base(px.load_events(args.recording))
	all_scenarios = scenarios(base)
	if args.list:
		for name, s in all_scenarios.items():
			print("%-28s %s" % (name, s[0]))
		return 0
	chosen = [n for n in all_scenarios if not args.only or any(n.startswith(o) for o in args.only.split(","))]
	ct.set_folder(NETWORK)
	ct.setup()
	os.makedirs(args.out, exist_ok=True)
	start = time.time()
	lines = []
	failed = 0
	variable = 0

	def say(text=""):
		print(text, flush=True)
		lines.append(text)

	say("compare_network.py: %d scenarios, recording %s" % (len(chosen), args.recording))
	for name in chosen:
		scenario = all_scenarios[name]
		results = {}
		try:
			for client in ("orig", "patella"):
				log("== %s: %s" % (name, client))
				results[client] = run(client, name, scenario, args.out)
		except Exception as e:  # a harness problem: reported, the run goes on
			failed += 1
			say("%s: HARNESS ERROR %s" % (name, e))
			traceback.print_exc()
			continue
		differences = compare(results["orig"], results["patella"])
		varies = False
		if differences and (args.repeat or scenario[4]):
			again = run("orig", name + "-repeat", scenario, args.out)
			varies = bool(compare(results["orig"], again))
			if varies:
				differences.append("(Tibia.exe's second run differs from its first as well: run-to-run variation)")
		if differences and scenario[4] and varies:
			variable += 1
			say("%s: VARIABLE (%s): the clients differ, and so do two runs of Tibia.exe" % (name, scenario[0]))
			for d in differences:
				say("    " + d)
		elif differences:
			failed += 1
			say("%s: DIFFERENT (%s)" % (name, scenario[0]))
			for d in differences:
				say("    " + d)
		else:
			say("%s: same (%s): %s" % (name, scenario[0], summary(results["orig"])))
	say("%d of %d scenarios differ or failed, %d variable (the session key's random garbage); %.0f s"
		% (failed, len(chosen), variable, time.time() - start))
	with open(os.path.join(args.out, "report.txt"), "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(lines) + "\n")
	return 0 if failed == 0 else 1


if __name__ == "__main__":
	sys.exit(main())
