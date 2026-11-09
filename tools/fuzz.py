r"""The fuzz targets of tests/fuzz (Patella-Modern issue #132, plan M9): their seeds, their libFuzzer builds and runs.

Usage (from the repository or a worktree):
  python -I tools/fuzz.py synthetic
  python -I tools/fuzz.py seeds [--reference DIR] [--client DIR] [--out DIR]
  python -I tools/fuzz.py build --platform linux|windows
  python -I tools/fuzz.py run --platform linux|windows [--time SECONDS] [--logs DIR] [target ...]

synthetic   writes the hand-made seeds of tests/fuzz/seeds/<target> (committed; nothing in them comes from CipSoft's
            files or recordings: built here from the formats the client's code reads). The CTest smoke runs use them.
seeds       writes local seeds to build/fuzz-seeds/<target> (default --out): the server's packets of the reference
            recordings (--reference, default build/reference: each session's replay.txt, as fuzz_packets' and
            fuzz_network's inputs), the Tibia.cfg and *.map files the sessions start with, and small files cut from
            the client's Tibia.dat, Tibia.spr and Tibia.pic (--client, default the repository's root). CipSoft-derived:
            build/ is ignored by git; never commit, push or upload them, and never give them to CI.
build       configures and builds the libFuzzer tree: linux, build/linux-x64-clang-fuzz (the preset linux-x64-clang
            with PATELLA_FUZZ=ON, Clang 23's libFuzzer, ASan and UBSan; on Windows in the Docker image, as
            tools/build.py runs the Linux presets), or windows, build/windows-x64-fuzz (windows-x64-asan with
            PATELLA_FUZZ=ON and PATELLA_CLIENT=OFF: MSVC's /fsanitize=fuzzer). A warning fails it, as in build.py.
run         runs each target (default all) with libFuzzer for --time seconds (default 600), on the synthetic seeds
            and the local ones (when there are any), the corpus it grows in <tree>/fuzz-corpus/<target>, what it
            finds in <tree>/fuzz-artifacts/<target>/. Each target's log goes to --logs (default <tree>/fuzz-logs);
            summary.tsv there has a line per target: the exit code, executions, executions per second, coverage
            (cov, ft), the corpus, and the findings (crash-, leak-, timeout-, oom- files). Exit 1 when one found
            something.
"""

import argparse
import importlib.util
import os
import re
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEEDS = os.path.join(ROOT, "tests", "fuzz", "seeds")
TARGETS = ["fuzz_settings", "fuzz_network", "fuzz_packets", "fuzz_tibia_cfg", "fuzz_map", "fuzz_dat",
	"fuzz_sprites", "fuzz_pictures"]
# libFuzzer's options per target (tests/fuzz/CMakeLists.txt has the same).
OPTIONS = {
	"fuzz_settings": ["-max_len=16384"],
	"fuzz_tibia_cfg": ["-max_len=16384"],
	"fuzz_network": ["-max_len=65536"],
	"fuzz_packets": ["-max_len=65536"],
	"fuzz_map": ["-max_len=200000"],
	"fuzz_dat": ["-rss_limit_mb=8192", "-malloc_limit_mb=8192"],
}
# Not run there (notes/decisions.md, "#132"): under MSVC's /fsanitize=fuzzer every input of fuzz_packets ends in a heap
# report inside the golden Communication world's own strings (its redirections of our code; clean under Clang's libFuzzer
# and MSVC's ASan without the fuzzer).
UNSUPPORTED = {("windows", "fuzz_packets")}
TREES = {"linux": "linux-x64-clang-fuzz", "windows": "windows-x64-fuzz"}
KEY = bytes(range(1, 17)) # fuzz_network.cpp's session key


def load_build():
	"""tools/build.py as a module (its Docker command and MSVC environment)."""
	spec = importlib.util.spec_from_file_location("patella_build", os.path.join(ROOT, "tools", "build.py"))
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


# --- The synthetic seeds ------------------------------------------------------------------------------------------

def u8(value):
	return struct.pack("<B", value & 0xff)


def u16(value):
	return struct.pack("<H", value & 0xffff)


def u32(value):
	return struct.pack("<I", value & 0xffffffff)


def text(value):
	data = value.encode("latin-1")
	return u16(len(data)) + data


def frames(*payloads):
	"""[u16 size][bytes] per frame (fuzz_network's clear frames, fuzz_packets' frames)."""
	return b"".join(u16(len(p)) + p for p in payloads)


def network_frame(payload, inner=None, padding=b"\x5a"):
	"""A frame's clear bytes: [u16 inner length][payload], padded to a multiple of 8."""
	plain = u16(len(payload) if inner is None else inner) + payload
	while len(plain) % 8:
		plain += padding
	return plain


def synthetic_settings():
	return {
		"version2.ini": (b"# Patella-Modern's settings\r\nPreferencesVersion=2\r\nTestServer=no\r\n"
			b"FullscreenMode=800,600,32\r\nHotkey0=\"hello\\nthere\",yes\r\nHotkey35=\"x\\x01\",no\r\n"
			b"WindowedMode=10,20,800,600,yes\r\nContentWindow1=1,200\r\nContentWindow2=3,100\r\n"
			b"BuddySortmethods=2,1,0\r\nOpenDialogs=1,2\r\nOpenChannel=5\r\nBuddy1=11,1,\"friend \\\"one\\\"\",yes\r\n"
			b"White=\"a\",\"b\"\r\nIgnore=\"c\"\r\nKnownHints=1,2,3\r\nSkillBars=0,2,6\r\nEngine=2\r\n"
			b"LogLevel=debug\r\nDevice=cpu\r\n"),
		"version1.ini": (b"PreferencesVersion=1\nHotkey1=\"caf\xe9\",no\nBuddy1=1,2,\"\x81\x9d\",no\nWhite=\"\xe4\"\n"),
		"odd.ini": (b"\xef\xbb\xbf; a comment\n# another\n\nno equals sign\n Key = value \nHotkey01=\"dup\",yes\n"
			b"Hotkey1=\"one\",no\nBuddy7=1,2\nPreferencesVersion=3\nSkillBars=9\nWindowedMode=1,2\nLogFile=\n"
			b"=empty key\nKEY=second\n"),
		"empty.ini": b"",
	}


def synthetic_network():
	seeds = {}
	ok = network_frame(b"\x01\x02\x03\x04\x05", inner=6)
	seeds["inner-fits.bin"] = u8(0) + frames(ok, network_frame(b"\x09"))
	# Issue #72's short packets: an inner length past the frame (7, 8, 100, 0x3ff0, 0xffff in a frame of 8), a frame of
	# size 0 (no inner length), a frame that fills ReceiveData with the length 0xffff.
	for inner in (7, 8, 100, 0x3ff0, 0xffff):
		seeds["72-inner-%04x.bin" % inner] = u8(0) + frames(network_frame(b"\x01\x02\x03\x04\x05", inner=inner),
			network_frame(b"\x09"))
	seeds["72-empty-frame.bin"] = u8(0) + frames(b"", network_frame(b"\x07"))
	seeds["72-full.bin"] = u8(0) + frames(network_frame(b"\x03" * (0x3ff0 - 2 - 8), inner=0xffff))
	seeds["odd-size.bin"] = u8(0) + frames(b"\x02\x00\x01\x02\x03")
	# Issue #139: a frame whose size is no multiple of 8 (item 7) and one larger than ReceiveData (item 6), each with
	# frames before and after it, which are read; the large one also as 64-byte receives.
	seeds["139-7-odd-sizes.bin"] = u8(0) + frames(b"\x02\x00\x01\x02\x03", network_frame(b"\x09"), bytes(13),
		network_frame(b"\x0a"))
	large = frames(network_frame(b"\x08"), b"\x22" * 0x5000, network_frame(b"\x09"))
	seeds["139-6-too-large.bin"] = u8(0) + large
	seeds["139-6-too-large-split.bin"] = u8(4) + large
	seeds["139-6-largest.bin"] = u8(0) + frames(network_frame(b"\x05" * 0x3ff6))
	# Issue #142: empty packets (inner length 0) with frames after them, which are read in the same call; and #139's
	# fuzzing case: an empty packet received 2 bytes at a time, then a frame that fits ReceiveData only without it.
	empty = network_frame(b"")
	seeds["142-empty-packets.bin"] = u8(0) + frames(empty, network_frame(b"\x09"), empty, empty, network_frame(b"\x07"))
	seeds["142-empty-then-largest.bin"] = u8(2) + frames(empty, network_frame(b"\x05" * 0x3ff6), network_frame(b"\x09"))
	seeds["split.bin"] = u8(1) + frames(ok, ok, network_frame(b"\x03hello\x00"))
	seeds["commands.bin"] = u8(5) + frames(network_frame(bytes(range(7)) + text("abc") + b"\x04" + u32(7)),
		network_frame(b"\x05" + bytes(5) + b"\x06" + bytes(20)))
	return seeds


# fuzz_packets: [state][frames]; the states' indices in fuzz_packets.cpp: 0..4 = 3, 4, 6, 7, 8.
LOGIN_STATE, GAME_STATE, IN_GAME_STATE = 0, 2, 4
GAME_TYPES = [0x0a, 0x0b, 0x14, 0x15, 0x16, 0x1e, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
	0x6f, 0x70, 0x71, 0x72, 0x78, 0x79, 0x7d, 0x7e, 0x7f, 0x82, 0x83, 0x84, 0x85, 0x86, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x96, 0x97, 0xa0, 0xa1, 0xa2, 0xa3, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5,
	0xbe, 0xbf, 0xc8, 0xd2, 0xd3, 0xd4]
LOGIN_TYPES = [0x0a, 0x0b, 0x14, 0x1e, 0x28, 0x64]


def synthetic_packets():
	seeds = {}
	character_list = (b"\x64" + u8(2) + text("Fuzz One") + text("World") + u32(0x0100007f) + u16(7172)
		+ text("Fuzz Two") + text("World") + u32(0x0100007f) + u16(7172) + u16(30))
	seeds["login-list.bin"] = u8(LOGIN_STATE) + frames(b"\x14" + text("1\nHello") + character_list)
	seeds["login-error.bin"] = u8(LOGIN_STATE) + frames(b"\x0a" + text("Sorry"))
	seeds["login-update.bin"] = u8(LOGIN_STATE) + frames(b"\x1e")
	for kind in LOGIN_TYPES:
		seeds["login-%02x.bin" % kind] = u8(1) + frames(u8(kind) + bytes(24))
	seeds["game-login.bin"] = u8(GAME_STATE) + frames(b"\x0a" + u32(0x10000001) + u16(50) + u8(1),
		b"\x1e", b"\x82" + u8(200) + u8(215), b"\xb4" + u8(0x13) + text("Hello there"),
		b"\xa0" + u16(150) + u16(150) + u16(400) + u32(1000) + u16(8) + u8(50) + u16(55) + u16(55) + u8(0) + u8(0)
		+ u16(0) + u8(0) + u8(0))
	seeds["game-talk.bin"] = u8(IN_GAME_STATE) + frames(b"\xaa" + text("Fuzz") + u8(1) + u16(100) + u16(100) + u8(7)
		+ text("hi"), b"\xac" + u16(5) + text("Channel"), b"\xb3" + u16(5))
	for kind in GAME_TYPES:
		seeds["game-%02x.bin" % kind] = u8(IN_GAME_STATE) + frames(u8(kind) + bytes(24))
	return seeds


def synthetic_tibia_cfg():
	return {
		"options.cfg": (b"TestServer = no\nFullscreenMode = (800,600,32)\nWindowsMousepointer = yes\n"
			b"LightEffects = yes\nCreatureInfo = 2\nEngine = 2\nHotkey = (0,\"hi\\n\")\nHotkey = (35,\"a\\nb\")\n"
			b"WindowedMode = (0,0,800,600,no)\nConsoleHeight = 140\nAutomapZoom = 2\nContentWindow = (1,200)\n"
			b"BuddySortmethods = (2,1,0)\nOpenDialogs = 1\nOpenDialogs = 1\nOpenChannel = 5\nChaseMode = 2\n"
			b"SecureMode = 1\nBuddy = (11,1,\"friend\",yes)\nWhite = {\"a\",\"b\"}\nIgnore = {\"c\"}\n"
			b"KnownHints = {1,2}\nSkillBars = {0,3}\nShowNewHints = no\n"),
		"unknown-key.cfg": b"Unknown = 5\nTestServer = yes\n",
		"bad-hotkey.cfg": b"Hotkey = (36,\"x\")\n",
		"empty.cfg": b"",
	}


def synthetic_map():
	mark = u32(256 * 125 + 7) + u32(256 * 125 + 9) + u32(3) + text("a mark")
	return {
		"marks.bin": u8(0) + u32(2) + mark + mark,
		"no-marks.bin": u8(0),
		"save.bin": u8(2) + u32(1) + mark,
		"short.bin": u8(1) + bytes(100),
		"negative-count.bin": u8(0) + u32(0xffffffff),
		# Issue #139, item 5: a negative count with a mark added and the block saved (the original grew the marks
		# down to the count).
		"139-5-negative-save.bin": u8(2) + u32(0xfffffffb),
		"139-5-most-negative-save.bin": u8(2) + u32(0x80000000),
	}


def dat_entry(flags=b"", width=1, height=1, layers=1, pattern=(1, 1, 1), phases=1):
	sizes = u8(width) + u8(height) + (u8(32) if width > 1 or height > 1 else b"")
	sizes += u8(layers) + u8(pattern[0]) + u8(pattern[1]) + u8(pattern[2]) + u8(phases)
	count = width * height * layers * pattern[0] * pattern[1] * pattern[2] * phases
	return flags + b"\xff" + sizes + b"".join(u16(i + 1) for i in range(count))


def synthetic_dat():
	ground = dat_entry(b"\x00" + u16(150))
	light = dat_entry(b"\x15" + u16(3) + u16(215) + b"\x18" + u16(8) + u16(8) + b"\x19" + u16(8))
	big = dat_entry(b"\x1c" + u16(100) + b"\x1d" + u16(1102), width=2, height=2, layers=2)
	outfit = dat_entry(b"\x03", layers=2, pattern=(4, 1, 1), phases=3)
	header = u32(0x4495fa2b) + u16(102) + u16(1) + u16(1) + u16(1)
	return {
		"small.dat": header + ground + light + big + outfit + dat_entry() + dat_entry(),
		"short.dat": header + ground + light[:5],
		"no-objects.dat": u32(1) + u16(99) + u16(1) + u16(1) + u16(1),
		# Issue #139, item 1: a load that fails after the first entry, and one whose outfits' table refuses its count
		# after the objects' table was allocated.
		"139-1-ends-after-one.dat": header + ground,
		"139-1-no-outfits.dat": u32(1) + u16(101) + u16(0) + u16(1) + u16(1) + dat_entry(),
		"high-flag.dat": u32(1) + u16(100) + u16(1) + u16(1) + u16(1) + dat_entry(b"\x20\xfe") + dat_entry()
			+ dat_entry() + dat_entry(),
	}


def runs(pixels):
	"""A sprite's or tile's runs: [(skip, [(r, g, b), ...]), ...] as [u16 size][u16 skip][u16 count][rgb...]..."""
	data = b""
	for skip, colours in pixels:
		data += u16(skip) + u16(len(colours)) + b"".join(bytes(c) for c in colours)
	return u16(len(data)) + data


def synthetic_sprites():
	first = b"\xff\x00\xff" + runs([(0, [(1, 2, 3), (4, 5, 6)]), (30, [(7, 8, 9)])])
	last = b"\x00\x00\x00" + runs([(1000, [(9, 9, 9)] * 24)]) # the 1024th pixel exactly
	header_size = 4 + 2 + 4 * 3
	offsets = [0, header_size, header_size + len(first)]
	data = u32(0x449a1c41) + u16(3) + b"".join(u32(o) for o in offsets) + first + last
	return {
		"sprites.spr": data,
		"short.spr": data[:header_size + 5],
		"bad-offset.spr": u32(1) + u16(1) + u32(0xfffffff0),
		# Issue #139, item 3: runs past the 1024th pixel (the original wrote past the buffer).
		"139-3-overflow.spr": u32(1) + u16(2) + u32(4 + 2 + 8) + u32(4 + 2 + 8 + 3 + 2 + 4 + 30 * 3)
			+ b"\x00\x00\x00" + runs([(1000, [(9, 9, 9)] * 30)]) + b"\x00\x00\x00" + runs([(1024, [(1, 1, 1)])]),
	}


def synthetic_pictures():
	tile = runs([(0, [(1, 2, 3)] * 4), (100, [(4, 5, 6)])])
	header_size = 4 + 2 + 5 + 4 * 2 + 5 + 4
	offsets = [header_size, header_size + len(tile), header_size + 2 * len(tile)]
	data = (u32(0x43fc1d0c) + u16(2) + u8(2) + u8(1) + b"\xff\x00\xff" + u32(offsets[0]) + u32(offsets[1])
		+ u8(1) + u8(1) + b"\x00\x00\x00" + u32(offsets[2]) + tile + tile + tile)
	return {
		"pictures.pic": data,
		"short.pic": data[:12],
		"empty-picture.pic": u32(1) + u16(1) + u8(0) + u8(0) + b"\x00\x00\x00",
		# Issue #139: a file that ends after the first picture's table (item 2), a tile whose runs pass the 1024th
		# pixel (item 3).
		"139-2-ends-after-one.pic": u32(1) + u16(3) + u8(1) + u8(1) + b"\x00\x00\x00" + u32(0) + u8(1),
		"139-3-overflow.pic": u32(1) + u16(1) + u8(1) + u8(1) + b"\x00\x00\x00" + u32(4 + 2 + 5 + 4)
			+ runs([(0, [(1, 2, 3)]), (1022, [(4, 5, 6)] * 2)]),
	}


SYNTHETIC = {
	"fuzz_settings": synthetic_settings,
	"fuzz_network": synthetic_network,
	"fuzz_packets": synthetic_packets,
	"fuzz_tibia_cfg": synthetic_tibia_cfg,
	"fuzz_map": synthetic_map,
	"fuzz_dat": synthetic_dat,
	"fuzz_sprites": synthetic_sprites,
	"fuzz_pictures": synthetic_pictures,
}


def write_seeds(folder, seeds):
	"""Replaces the folder's files (its own files only: a seed folder has no links) with the seeds."""
	os.makedirs(folder, exist_ok=True)
	for name in os.listdir(folder):
		path = os.path.join(folder, name)
		if os.path.isfile(path) and not os.path.islink(path):
			os.remove(path)
	for name, data in sorted(seeds.items()):
		with open(os.path.join(folder, name), "wb") as f:
			f.write(data)


def command_synthetic(_args):
	for target, make in SYNTHETIC.items():
		seeds = make()
		write_seeds(os.path.join(SEEDS, target), seeds)
		print("fuzz.py: %s: %d synthetic seeds" % (target, len(seeds)))
	return 0


# --- The local seeds (CipSoft-derived) ---------------------------------------------------------------------------

def replay_packets(path):
	"""The server's packets of a replay.txt, per connection: [(kind, [payload, ...]), ...] in the file's order."""
	connections = {}
	order = []
	with open(path, encoding="ascii") as f:
		for line in f:
			fields = line.split()
			if len(fields) >= 5 and fields[2] == "s2c":
				key = (fields[0], fields[1])
				if key not in connections:
					connections[key] = []
					order.append(key)
				connections[key].append(bytes.fromhex(fields[4]))
	return [(key[0], connections[key]) for key in order]


def cut_dat(data, entries):
	"""Tibia.dat's header and its first entries, the header's counts made to fit (objects only, then one of each)."""
	position = 12
	ends = []
	words = {0, 8, 9, 25, 28, 29}
	while len(ends) < entries + 3 and position < len(data):
		while True:
			flag = data[position]
			position += 1
			if flag == 0xff:
				break
			position += 2 * (1 if flag in words else 2 if flag in (21, 24) else 0)
		width, height = data[position], data[position + 1]
		position += 2 + (1 if width > 1 or height > 1 else 0)
		layers, pw, ph, pd, phases = data[position:position + 5]
		position += 5 + 2 * width * height * layers * pw * ph * pd * phases
		ends.append(position)
	# The first `entries` objects, then three more entries read as an outfit, an effect and a missile.
	header = data[:4] + u16(99 + entries) + u16(1) + u16(1) + u16(1)
	return header + data[12:ends[entries + 2]]


def cut_spr(data, count):
	"""Tibia.spr's first `count` sprites with their offsets made to fit."""
	number = struct.unpack_from("<H", data, 4)[0]
	offsets = struct.unpack_from("<%dI" % number, data, 6)
	header = 6 + 4 * count
	body = b""
	new = []
	for offset in offsets[:count]:
		if offset == 0:
			new.append(0)
			continue
		size = struct.unpack_from("<H", data, offset + 3)[0]
		new.append(header + len(body))
		body += data[offset:offset + 5 + size]
	return data[:4] + u16(count) + b"".join(u32(o) for o in new) + body


def cut_pic(data, count):
	"""Tibia.pic's first `count` pictures with their tiles, the offsets made to fit."""
	position = 6
	pictures = []
	for _ in range(count):
		tx, ty = data[position], data[position + 1]
		key = data[position + 2:position + 5]
		offsets = struct.unpack_from("<%dI" % (tx * ty), data, position + 5)
		position += 5 + 4 * tx * ty
		pictures.append((tx, ty, key, offsets))
	header = 6 + sum(5 + 4 * tx * ty for tx, ty, _, _ in pictures)
	table = b""
	body = b""
	for tx, ty, key, offsets in pictures:
		new = []
		for offset in offsets:
			size = struct.unpack_from("<H", data, offset)[0]
			new.append(header + len(body))
			body += data[offset:offset + 2 + size]
		table += u8(tx) + u8(ty) + key + b"".join(u32(o) for o in new)
	return data[:4] + u16(count) + table + body


def command_seeds(args):
	out = args.out
	reference = args.reference
	client = args.client
	made = {target: {} for target in TARGETS}
	if os.path.isdir(reference):
		for session in sorted(os.listdir(reference)):
			folder = os.path.join(reference, session)
			replay = os.path.join(folder, "replay.txt")
			if os.path.isfile(replay):
				for number, (kind, payloads) in enumerate(replay_packets(replay)):
					login = kind == "login"
					# fuzz_packets: the connection's packets in groups of 4 frames (the first in state 3 or 6, the
					# rest in the game, state 8).
					for start in range(0, len(payloads), 4):
						state = LOGIN_STATE if login else GAME_STATE if start == 0 else IN_GAME_STATE
						made["fuzz_packets"]["%s-%d-%03d.bin" % (session, number, start)] = (u8(state)
							+ frames(*payloads[start:start + 4]))
					# fuzz_network: the same packets as frames, 4 to a seed.
					for start in range(0, len(payloads), 4):
						made["fuzz_network"]["%s-%d-%03d.bin" % (session, number, start)] = u8(0) + frames(
							*[network_frame(p) for p in payloads[start:start + 4]])
			for name in ("Tibia.cfg.start", os.path.join("files", "Tibia.cfg")):
				path = os.path.join(folder, name)
				if os.path.isfile(path):
					with open(path, "rb") as f:
						made["fuzz_tibia_cfg"]["%s-%s.cfg" % (session, os.path.basename(name))] = f.read()
			files = os.path.join(folder, "files")
			if os.path.isdir(files):
				for name in sorted(os.listdir(files)):
					if name.endswith(".map"):
						with open(os.path.join(files, name), "rb") as f:
							made["fuzz_map"]["%s-%s.bin" % (session, name)] = u8(1) + f.read()
	else:
		print("fuzz.py: no reference recordings in %s" % reference)
	paths = {name: os.path.join(client, name) for name in ("Tibia.dat", "Tibia.spr", "Tibia.pic", "Tibia.cfg")}
	if os.path.isfile(paths["Tibia.dat"]):
		with open(paths["Tibia.dat"], "rb") as f:
			dat = f.read()
		for entries in (1, 10, 100):
			made["fuzz_dat"]["Tibia-%d.dat" % entries] = cut_dat(dat, entries)
	if os.path.isfile(paths["Tibia.spr"]):
		with open(paths["Tibia.spr"], "rb") as f:
			spr = f.read()
		for count in (2, 16, 64):
			made["fuzz_sprites"]["Tibia-%d.spr" % count] = cut_spr(spr, count)
	if os.path.isfile(paths["Tibia.pic"]):
		with open(paths["Tibia.pic"], "rb") as f:
			pic = f.read()
		for count in (1, 3):
			made["fuzz_pictures"]["Tibia-%d.pic" % count] = cut_pic(pic, count)
	if os.path.isfile(paths["Tibia.cfg"]):
		with open(paths["Tibia.cfg"], "rb") as f:
			made["fuzz_tibia_cfg"]["Tibia.cfg"] = f.read()
	for target in TARGETS:
		write_seeds(os.path.join(out, target), made[target])
		print("fuzz.py: %s: %d local seeds in %s" % (target, len(made[target]), os.path.join(out, target)))
	return 0


# --- The libFuzzer trees -------------------------------------------------------------------------------------------

def docker(build, tree, command, extra_mounts):
	"""The command in the Docker image (build.py's), with the tree at /work/build/<tree> and the extra mounts."""
	full, listing = build.docker_command(tree, command)
	image = os.environ.get("PATELLA_DOCKER_IMAGE", build.DOCKER_IMAGE)
	at = full.index(image)
	for host, inside in extra_mounts:
		full[at:at] = ["-v", "%s:%s:ro" % (host, inside)]
		at += 2
	return full, listing


def run_tree(platform, command, log=None, extra_mounts=(), check_warnings=False):
	"""Runs the command for the platform's tree (linux: in the image, from /work; windows: MSVC's environment)."""
	build = load_build()
	tree = TREES[platform]
	listing = None
	if platform == "linux" and os.name == "nt":
		command, listing = docker(build, tree, command, extra_mounts)
		env = dict(os.environ)
	elif platform == "windows":
		env = build.environment("windows-x64-asan")
		# the pinned CMake (issue #141), not the first one on PATH
		command = build.pinned_cmake(command)
	else:
		env = dict(os.environ)
	stdin = open(listing, "rb") if listing else None
	try:
		if check_warnings:
			return build.run_checked(command, env, stdin)
		if log is None:
			return subprocess.call(command, env=env, cwd=ROOT, stdin=stdin)
		with open(log, "wb") as out:
			return subprocess.call(command, env=env, cwd=ROOT, stdin=stdin, stdout=out, stderr=subprocess.STDOUT)
	finally:
		if stdin is not None:
			stdin.close()
			os.remove(listing)


def command_build(args):
	tree = TREES[args.platform]
	binary = "build/%s" % tree
	if args.platform == "linux":
		configure = ["cmake", "--preset", "linux-x64-clang", "-B", binary, "-DPATELLA_FUZZ=ON",
			"-DPATELLA_GOLDEN_DIR=none"]
		script = "%s && cmake --build %s --config Release --target %s" % (subprocess.list2cmdline(configure),
			binary, " ".join(TARGETS))
		return run_tree("linux", ["bash", "-c", script], check_warnings=True)
	configure = ["cmake", "--preset", "windows-x64-asan", "-B", binary, "-DPATELLA_FUZZ=ON", "-DPATELLA_CLIENT=OFF"]
	code = run_tree("windows", configure, check_warnings=True)
	if code != 0:
		return code
	return run_tree("windows", ["cmake", "--build", binary, "--config", "Release", "--target"] + TARGETS,
		check_warnings=True)


STAT = re.compile(r"stat::(\w+):\s*(\d+)")
# MSVC's libFuzzer has no PC table: its lines have no "cov:" (then 0).
COVERAGE = re.compile(r"(?:\bcov: (\d+) )?\bft: (\d+) corp: (\d+)/")


def summarize(log):
	"""What a libFuzzer log says: its final stats and the last coverage line."""
	stats = {}
	coverage = (0, 0, 0)
	with open(log, encoding="utf-8", errors="replace") as f:
		for line in f:
			m = STAT.search(line)
			if m:
				stats[m.group(1)] = int(m.group(2))
			m = COVERAGE.search(line)
			if m:
				coverage = tuple(int(g) if g else 0 for g in m.groups())
	return stats, coverage


def command_run(args):
	tree = TREES[args.platform]
	host_tree = os.path.join(ROOT, "build", tree)
	logs = args.logs or os.path.join(host_tree, "fuzz-logs")
	os.makedirs(logs, exist_ok=True)
	local = os.path.join(ROOT, "build", "fuzz-seeds")
	targets = args.targets or [t for t in TARGETS if (args.platform, t) not in UNSUPPORTED]
	failed = False
	rows = []
	for target in targets:
		if target not in TARGETS:
			print("fuzz.py: no target %s" % target, file=sys.stderr)
			return 2
		corpus = os.path.join(host_tree, "fuzz-corpus", target)
		artifacts = os.path.join(host_tree, "fuzz-artifacts", target)
		os.makedirs(corpus, exist_ok=True)
		os.makedirs(artifacts, exist_ok=True)
		has_local = os.path.isdir(os.path.join(local, target)) and os.listdir(os.path.join(local, target))
		mounts = []
		if args.platform == "linux" and os.name == "nt":
			base = "/work/build/%s" % tree
			exe = "%s/tests/fuzz/Release/%s" % (base, target)
			seed_dirs = ["/work/tests/fuzz/seeds/%s" % target]
			if has_local:
				mounts.append((os.path.join(local, target), "/seeds/%s" % target))
				seed_dirs.append("/seeds/%s" % target)
			# The corpus libFuzzer grows on the container's own file system (each new unit is a file: on the mounted
			# folder a write costs more than a run), copied from the tree before the run and back after it.
			kept = "%s/fuzz-corpus/%s" % (base, target)
			corpus_arg, artifact_arg = "/tmp/corpus", "%s/fuzz-artifacts/%s/" % (base, target)
		else:
			folder = os.path.join(host_tree, "tests", "fuzz", "Release")
			exe = os.path.join(folder, target + (".exe" if args.platform == "windows" else ""))
			seed_dirs = [os.path.join(SEEDS, target)] + ([os.path.join(local, target)] if has_local else [])
			corpus_arg, artifact_arg = corpus, artifacts + os.sep
		command = [exe, "-max_total_time=%d" % args.time, "-print_final_stats=1", "-artifact_prefix=" + artifact_arg,
			"-timeout=25"] + OPTIONS.get(target, []) + [corpus_arg] + seed_dirs
		if args.platform == "linux":
			# The image has no llvm-symbolizer: binutils' addr2line names the frames.
			symbolizer = "external_symbolizer_path=/usr/bin/addr2line"
			environment = "ASAN_OPTIONS=detect_leaks=%d:alloc_dealloc_mismatch=0:%s" % (
				1 if target == "fuzz_settings" else 0, symbolizer)
			command = ["env", environment, "UBSAN_OPTIONS=print_stacktrace=1:" + symbolizer] + command
			if os.name == "nt":
				script = ('mkdir -p /tmp/corpus && cp -r %s/. /tmp/corpus/ && cd /tmp && { "$@"; code=$?; }; '
					'cp -r /tmp/corpus/. %s/; exit $code') % (kept, kept)
				command = ["bash", "-c", script, "bash"] + command
		log = os.path.join(logs, "%s.log" % target)
		print("fuzz.py: %s for %d s (log %s)" % (target, args.time, log), flush=True)
		code = run_tree(args.platform, command, log=log, extra_mounts=mounts)
		stats, coverage = summarize(log)
		findings = sorted(name for name in os.listdir(artifacts)
			if name.split("-")[0] in ("crash", "leak", "timeout", "oom", "slow"))
		executions = stats.get("number_of_executed_units", 0)
		rate = stats.get("average_exec_per_sec", 0)
		rows.append((target, code, executions, rate, coverage[0], coverage[1], len(os.listdir(corpus)),
			" ".join(findings) or "-"))
		print("fuzz.py: %s: exit %d, %d executions (%d/s), cov %d ft %d, corpus %d, findings %s" % rows[-1][:8],
			flush=True)
		failed |= code != 0 or bool(findings)
	with open(os.path.join(logs, "summary.tsv"), "w", encoding="utf-8", newline="\n") as f:
		f.write("target\texit\texecutions\texec_per_s\tcov\tft\tcorpus\tfindings\n")
		for row in rows:
			f.write("\t".join(str(v) for v in row) + "\n")
	return 1 if failed else 0


def main(argv):
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	commands = parser.add_subparsers(dest="command", required=True)
	commands.add_parser("synthetic")
	seeds = commands.add_parser("seeds")
	seeds.add_argument("--reference", default=os.path.join(ROOT, "build", "reference"))
	seeds.add_argument("--client", default=ROOT)
	seeds.add_argument("--out", default=os.path.join(ROOT, "build", "fuzz-seeds"))
	build = commands.add_parser("build")
	build.add_argument("--platform", choices=sorted(TREES), required=True)
	run = commands.add_parser("run")
	run.add_argument("--platform", choices=sorted(TREES), required=True)
	run.add_argument("--time", type=int, default=600)
	run.add_argument("--logs")
	run.add_argument("targets", nargs="*")
	args = parser.parse_args(argv)
	return {"synthetic": command_synthetic, "seeds": command_seeds, "build": command_build, "run": command_run}[
		args.command](args)


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
