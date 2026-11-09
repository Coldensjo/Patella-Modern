r"""What a package may hold: the proof that it holds no CipSoft file (Patella-Modern issue #135, plan M9, D17).

  python -I tools/package_check.py <package> [<package> ...]
      every check on a zip, tarball or AppImage. Exit 0 when all pass, 1 with the problems listed.
  python -I tools/package_check.py --run <package> [<package> ...]
      unpacks the package and starts the client in an empty folder; it must end at the original's error for a missing
      Tibia.dat. Windows: only in Windows Sandbox or on CI (run it with tools/sandbox.py, never on a user's desktop: the
      error is a message box). Linux: under Xvfb, in the image of docker/linux-x64.
  python -I tools/package_check.py --local <data folder> [--manifest <MANIFEST.tsv>]... <package> [<package> ...]
      local only, on the machine with the real files: each package file against the real files' SHA-256 and the frozen
      manifests' files, and Tibia.exe's own cursors, icons and bitmaps searched for inside every package file. It also
      shows that the format detectors below recognize the real files. It prints counts only. Never give it to CI.
  python -I tools/package_check.py --staged <folder>
      cmake/PackagePreBuild.cmake's check of CPack's staged files, before the package is made.
  python -I tools/package_check.py --placeholders <folder>
      cmake/Package.cmake's check at configure time: the folder has exactly tools/placeholder_resources.py's files.
  python -I tools/package_check.py --self-test
      the checks on made-up packages, good ones and bad ones.

The checks:
- names: no file named like the data files (Tibia.* in any case; *.dat, *.spr, *.pic, *.cfg, *.map);
- contents: no file in the format of Tibia.spr, Tibia.pic, Tibia.dat or Tibia.cfg under another name (the loaders' own
  layouts: src/graphics.cpp LoadSprites and LoadPictures, src/Objects.cpp LoadObjects, the option keys of
  src/Preferences.cpp), and no 32-bit PE (Tibia.exe is one, a package has only x64 programs);
- the list: exactly the files of EXPECTED, so nothing else can ride along;
- the placeholders: the client carries the generated pictures (tools/placeholder_resources.py), pinned here by
  SHA-256. Windows: every cursor and icon of the program's resources is the placeholder's, and it has no bitmap.
  Linux: the placeholder cursor's bytes are in the program. A build with Tibia.exe's pictures fails this, also on
  a machine without Tibia.exe to compare with. Both: the warning text of a placeholder build (src/Player.cpp).
"""

import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# tools/placeholder_resources.py's files. A change of the placeholders is a conscious change of these three lines.
PLACEHOLDER_SHA256 = {
	"cursor_132.cur": "48fb5fd582b1fb5a63983e049c8a83b1dd4f1b4f93c16ae9f583dcb3f432d600",
	"cursor_137.cur": "48fb5fd582b1fb5a63983e049c8a83b1dd4f1b4f93c16ae9f583dcb3f432d600",
	"icon_128.ico": "8e7139fd12e53736f05a013ccf9d6a8f716f3bfdbea77f46cf699f0569ad75e0",
}
PLACEHOLDER_WARNING = b"this build has placeholder cursors and icon"

DATA_EXTENSIONS = (".dat", ".spr", ".pic", ".cfg", ".map")
CFG_KEYS = re.compile(rb"^(TestServer|FullscreenMode|WindowedMode|CreatureInfo|ConsoleHeight|AutomapZoom|Hotkey|ContentWindow|"
	rb"BuddySortmethods|ChaseMode|SecureMode)\s*=", re.MULTILINE)

LICENSES = ["LICENSE", "NOTICE", "licenses/THIRD-PARTY.txt", "licenses/regexp.txt", "licenses/SDL3.txt", "licenses/volk.txt",
	"licenses/VulkanMemoryAllocator.txt", "licenses/vk-bootstrap.txt", "licenses/Vulkan-Headers.txt",
	"licenses/Vulkan-Headers-MIT.txt"]
DOCS = "share/doc/patella/"
EXPECTED = {
	"windows-zip": ["Patella.exe", "vulkan_lvp.dll", "lvp_icd.x86_64.json", "README.txt"] + LICENSES
		+ ["licenses/Mesa.txt", "licenses/LLVM.txt"],
	"linux-tgz": ["bin/Patella", "share/applications/patella.desktop", "share/icons/hicolor/scalable/apps/patella.svg",
		DOCS + "README.txt"] + [DOCS + name for name in LICENSES],
}
# What appimagetool and CPack's generator add to the staged files (the last three are links into share/).
EXPECTED["appimage"] = EXPECTED["linux-tgz"] + ["AppRun", ".DirIcon", "patella.desktop", "patella.svg"]
CLIENT = {"windows-zip": "Patella.exe", "linux-tgz": "bin/Patella", "appimage": "bin/Patella"}

RT_CURSOR, RT_BITMAP, RT_ICON = 1, 2, 3


def placeholder_module():
	"""tools/placeholder_resources.py, loaded by its path (python -I has no script folder on sys.path)."""
	path = os.path.join(ROOT, "tools", "placeholder_resources.py")
	spec = importlib.util.spec_from_file_location("placeholder_resources", path)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


def placeholders():
	"""(the bytes of the .cur and .ico by file name, a cursor's RT_CURSOR bytes, the icon's RT_ICON bytes), generated by
	the script and compared with the pins."""
	module = placeholder_module()
	middle = module.SIZE // 2
	files = {"cursor_132.cur": module.cursor_file(middle, middle), "cursor_137.cur": module.cursor_file(middle, middle),
		"icon_128.ico": module.icon_file()}
	for name, data in files.items():
		digest = hashlib.sha256(data).hexdigest()
		if digest != PLACEHOLDER_SHA256[name]:
			raise SystemExit("package_check: tools/placeholder_resources.py's %s has SHA-256 %s, not the pinned %s"
				% (name, digest, PLACEHOLDER_SHA256[name]))
	# A .cur or .ico: a 6-byte header, a 16-byte entry, the DIB at offset 22 (placeholder_resources.py).
	cursor = struct.pack("<HH", middle, middle) + files["cursor_132.cur"][22:]
	return files, cursor, files["icon_128.ico"][22:]


# -- the formats ----------------------------------------------------------------------------------------------------


def is_executable(data):
	return data[:2] == b"MZ" or data[:4] == b"\x7fELF"


def looks_like_spr(data):
	"""Tibia.spr: signature, a 16-bit count, the offsets, the first sprite right after them (LoadSprites)."""
	if len(data) < 10 or is_executable(data):
		return False
	count = struct.unpack_from("<H", data, 4)[0]
	table_end = 6 + 4 * count
	if count == 0 or table_end > len(data):
		return False
	offsets = struct.unpack_from("<%dI" % count, data, 6)
	used = [offset for offset in offsets if offset]
	return bool(used) and min(used) == table_end and max(used) < len(data)


def looks_like_pic(data):
	"""Tibia.pic: signature, a count, for each picture its tiles' size, colour key and offsets (LoadPictures)."""
	if len(data) < 11 or is_executable(data):
		return False
	count = struct.unpack_from("<H", data, 4)[0]
	if count == 0:
		return False
	position = 6
	offsets = []
	for _ in range(count):
		if position + 5 > len(data):
			return False
		width, height = data[position], data[position + 1]
		position += 5
		tiles = width * height
		if tiles == 0 or position + 4 * tiles > len(data):
			return False
		offsets.extend(struct.unpack_from("<%dI" % tiles, data, position))
		position += 4 * tiles
	used = [offset for offset in offsets if offset]
	return bool(used) and min(used) == position and max(used) < len(data)


def looks_like_dat(data):
	"""Tibia.dat: signature, four 16-bit counts, the entries (LoadObjects, LoadProperties): the attribute bytes up to 0xff
	with their words, the sizes, the sprite numbers. Parses up to 64 entries, or all of a smaller file."""
	if len(data) < 12 or is_executable(data):
		return False
	objects, outfits, effects, missiles = struct.unpack_from("<4H", data, 4)
	if objects < 100 or outfits == 0 or effects == 0 or missiles == 0:
		return False
	position = 12
	reads_word = {0, 8, 9, 25, 28, 29}
	reads_two_words = {21, 24}
	parsed = 0
	entries = objects - 99 + outfits + effects + missiles
	while parsed < min(entries, 64):
		while True:
			if position >= len(data):
				return False
			flag = data[position]
			position += 1
			if flag == 0xff:
				break
			if flag > 31:
				return False
			if flag in reads_word:
				position += 2
			elif flag in reads_two_words:
				position += 4
		if position + 2 > len(data):
			return False
		width, height = data[position], data[position + 1]
		position += 2
		if width == 0 or height == 0:
			return False
		if width > 1 or height > 1:
			position += 1
		if position + 5 > len(data):
			return False
		layers, pattern_width, pattern_height, pattern_depth, phases = data[position:position + 5]
		position += 5
		count = width * height * layers * pattern_width * pattern_height * pattern_depth * phases
		if count == 0:
			return False
		position += 2 * count
		if position > len(data):
			return False
		parsed += 1
	return parsed > 0


def looks_like_cfg(data):
	"""Tibia.cfg: text, "key = value" lines with the client's option keys (src/Preferences.cpp)."""
	if is_executable(data) or len(data) > 1 << 20:
		return False
	return len(set(match.group(1) for match in CFG_KEYS.finditer(data))) >= 2


FORMATS = [("Tibia.spr", looks_like_spr), ("Tibia.pic", looks_like_pic), ("Tibia.dat", looks_like_dat), ("Tibia.cfg", looks_like_cfg)]


def pe_info(data):
	"""A PE's (machine, resources), the resources {(type, name id, language): bytes}; None for anything else."""
	if data[:2] != b"MZ" or len(data) < 0x40:
		return None
	header = struct.unpack_from("<I", data, 0x3c)[0]
	if data[header:header + 4] != b"PE\0\0":
		return None
	machine, sections = struct.unpack_from("<HH", data, header + 4)
	optional_size = struct.unpack_from("<H", data, header + 20)[0]
	optional = header + 24
	magic = struct.unpack_from("<H", data, optional)[0]
	directories = optional + (96 if magic == 0x10b else 112)
	resources_rva = struct.unpack_from("<I", data, directories + 16)[0]
	table = []
	for index in range(sections):
		base = optional + optional_size + 40 * index
		virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<4I", data, base + 8)
		table.append((virtual_address, max(virtual_size, raw_size), raw_pointer))

	def file_offset(rva):
		for address, size, pointer in table:
			if address <= rva < address + size:
				return rva - address + pointer
		raise ValueError("RVA 0x%x is in no section" % rva)

	found = {}
	if resources_rva == 0:
		return machine, found
	root = file_offset(resources_rva)

	def walk(offset, path):
		named, numbered = struct.unpack_from("<HH", data, root + offset + 12)
		for entry in range(named + numbered):
			name, target = struct.unpack_from("<II", data, root + offset + 16 + 8 * entry)
			next_path = path + (name,)
			if target & 0x80000000:
				walk(target & 0x7fffffff, next_path)
			else:
				rva, size = struct.unpack_from("<II", data, root + target)
				start = file_offset(rva)
				found[next_path] = data[start:start + size]

	walk(0, ())
	return machine, found


def resource_data(resources, kind):
	return [(key, value) for key, value in resources.items() if key[0] == kind]


# -- the checks -------------------------------------------------------------------------------------------------------


def name_problem(path):
	base = path.rsplit("/", 1)[-1].lower()
	if base.startswith("tibia."):
		return "%s: named like a data file" % path
	if base.endswith(DATA_EXTENSIONS):
		return "%s: has a data file's extension" % path
	return None


def content_problems(path, data):
	found = []
	for name, test in FORMATS:
		if test(data):
			found.append("%s: has the format of %s" % (path, name))
	info = pe_info(data)
	if info is not None and info[0] == 0x14c:
		found.append("%s: a 32-bit PE (Tibia.exe's format)" % path)
	return found


def placeholder_problems(path, data):
	"""The client's positive marker of the placeholder build."""
	_, cursor, icon = placeholders()
	found = []
	if PLACEHOLDER_WARNING not in data:
		found.append("%s: no placeholder build's warning text (not a PATELLA_PLACEHOLDER_RESOURCES build)" % path)
	info = pe_info(data)
	if info is not None:
		_, resources = info
		cursors = resource_data(resources, RT_CURSOR)
		icons = resource_data(resources, RT_ICON)
		if len(cursors) < 2 or any(value != cursor for _, value in cursors):
			found.append("%s: its RT_CURSOR resources are not exactly the placeholder's (%d found)" % (path, len(cursors)))
		if len(icons) < 1 or any(value != icon for _, value in icons):
			found.append("%s: its RT_ICON resources are not exactly the placeholder's (%d found)" % (path, len(icons)))
		if resource_data(resources, RT_BITMAP):
			found.append("%s: has RT_BITMAP resources; the placeholder build has none" % path)
	elif data[:4] == b"\x7fELF":
		if cursor not in data:
			found.append("%s: the placeholder cursor's bytes are not in the program" % path)
	else:
		found.append("%s: neither a PE nor an ELF program" % path)
	return found


def check_tree(files, kind):
	"""The problems of a package's files [(relative path, bytes or None for a link)]."""
	problems = []
	names = [path for path, _ in files]
	for path, data in files:
		problem = name_problem(path)
		if problem:
			problems.append(problem)
		if data is not None:
			problems.extend(content_problems(path, data))
	expected = set(EXPECTED[kind])
	for path in sorted(set(names) - expected):
		problems.append("%s: not on this package's list of files" % path)
	for path in sorted(expected - set(names)):
		problems.append("%s: missing" % path)
	client = dict(files).get(CLIENT[kind])
	if client:
		problems.extend(placeholder_problems(CLIENT[kind], client))
	return problems


# -- reading a package ---------------------------------------------------------------------------------------------


def strip_top(files):
	"""Removes the one folder every path of an archive is under."""
	tops = {path.split("/", 1)[0] for path, _ in files}
	if len(tops) != 1 or any("/" not in path for path, _ in files):
		return files
	return [(path.split("/", 1)[1], data) for path, data in files]


def read_folder(root):
	files = []
	for folder, directories, names in os.walk(root):
		for name in names:
			full = os.path.join(folder, name)
			path = os.path.relpath(full, root).replace(os.sep, "/")
			if os.path.islink(full):
				files.append((path, None))
			else:
				with open(full, "rb") as f:
					files.append((path, f.read()))
	return files


def appimage_folder(package, work):
	"""Unpacks an AppImage (its own runtime; no FUSE needed) in `work`; the AppDir's path."""
	if os.name == "nt":
		raise SystemExit("package_check: an AppImage is read on Linux only: run this in the image of docker/linux-x64")
	os.chmod(package, 0o755)
	result = subprocess.run([os.path.abspath(package), "--appimage-extract"], cwd=work, stdout=subprocess.DEVNULL,
		stderr=subprocess.PIPE)
	folder = os.path.join(work, "squashfs-root")
	if result.returncode != 0 or not os.path.isdir(folder):
		raise SystemExit("package_check: %s --appimage-extract failed (%d): %s" % (package, result.returncode,
			result.stderr.decode("utf-8", "replace")))
	return folder


def kind_of(package):
	name = package.lower()
	if name.endswith(".zip"):
		return "windows-zip"
	if name.endswith((".tar.gz", ".tgz")):
		return "linux-tgz"
	if name.endswith(".appimage"):
		return "appimage"
	raise SystemExit("package_check: %s is not a zip, tarball or AppImage" % package)


def read_package(package):
	"""A package's (kind, files)."""
	kind = kind_of(package)
	if kind == "windows-zip":
		with zipfile.ZipFile(package) as z:
			files = [(i.filename, z.read(i)) for i in z.infolist() if not i.is_dir()]
		return kind, strip_top(files)
	if kind == "linux-tgz":
		files = []
		with tarfile.open(package) as t:
			for member in t.getmembers():
				if member.isfile():
					files.append((member.name, t.extractfile(member).read()))
				elif not member.isdir():
					files.append((member.name, None))
		return kind, strip_top(files)
	with tempfile.TemporaryDirectory(dir=os.path.dirname(os.path.abspath(package)), prefix="appimage-") as work:
		return kind, read_appimage_tree(appimage_folder(package, work))


def read_appimage_tree(folder):
	"""An AppDir's files relative to its usr folder, and its own (AppRun and the rest) under their names."""
	return read_folder(folder)


def report(name, problems):
	if problems:
		print("package_check: %s: %d problem(s):" % (name, len(problems)))
		for problem in problems:
			print("  " + problem)
		return 1
	print("package_check: %s: no CipSoft file, the placeholder pictures, the expected files" % name)
	return 0


def check_package(package):
	kind, files = read_package(package)
	problems = check_tree(files, kind)
	return report(package, problems)


def check_staged(folder):
	"""CPack's staging folder (one folder deep or not): the tree before the package exists."""
	files = read_folder(folder)
	names = {path for path, _ in files}
	kind = "windows-zip" if any(path.rsplit("/", 1)[-1] == "Patella.exe" for path in names) else "linux-tgz"
	expected_client = CLIENT[kind]
	roots = {path[:-len(expected_client)] for path in names if path == expected_client or path.endswith("/" + expected_client)}
	if len(roots) != 1:
		print("package_check: %s: no single %s (%d found)" % (folder, expected_client, len(roots)))
		return 1
	root = roots.pop()
	files = [(path[len(root):], data) for path, data in files if path.startswith(root)]
	return report(folder + " (staged)", check_tree(files, kind))


def check_placeholders(folder):
	files, _, _ = placeholders()
	problems = []
	for name, data in files.items():
		path = os.path.join(folder, name)
		if not os.path.isfile(path):
			problems.append("%s: missing" % path)
			continue
		with open(path, "rb") as f:
			if f.read() != data:
				problems.append("%s: not the generated placeholder" % path)
	if problems:
		for problem in problems:
			print(problem)
		return 1
	print("package_check: %s has the pinned placeholders" % folder)
	return 0


# -- the local check ------------------------------------------------------------------------------------------------


def check_local(data_folder, manifests, packages):
	"""Whole-file hashes and Tibia.exe's own resources against the package files. Prints counts only."""
	names = ["Tibia.exe", "Tibia.dat", "Tibia.spr", "Tibia.pic", "Tibia.cfg"]
	real = {}
	detected = 0
	for name in names:
		path = os.path.join(data_folder, name)
		with open(path, "rb") as f:
			contents = f.read()
		real[hashlib.sha256(contents).hexdigest()] = name
		for format_name, test in FORMATS:
			if format_name == name:
				if not test(contents):
					print("package_check: the detector of %s does not recognize the real file" % name)
					return 1
				detected += 1
		if name == "Tibia.exe":
			exe = contents
	info = pe_info(exe)
	if info is None or info[0] != 0x14c:
		print("package_check: the real Tibia.exe is not a 32-bit PE here")
		return 1
	blobs = []
	for key, value in info[1].items():
		if key[0] in (RT_CURSOR, RT_BITMAP, RT_ICON):
			blobs.append(value)
	manifest_hashes = set()
	for manifest in manifests:
		with open(manifest, encoding="utf-8") as f:
			for line in f:
				for field in line.rstrip("\n").split("\t"):
					if re.fullmatch(r"[0-9a-f]{64}", field):
						manifest_hashes.add(field)
	failures = 0
	for package in packages:
		_, files = read_package(package)
		checked = 0
		for path, data in files:
			if data is None:
				continue
			checked += 1
			digest = hashlib.sha256(data).hexdigest()
			if digest in real:
				print("package_check: %s: a package file is the real %s" % (package, real[digest]))
				failures += 1
			if digest in manifest_hashes:
				print("package_check: %s: a package file is in a frozen manifest" % package)
				failures += 1
			for blob in blobs:
				if len(blob) >= 32 and blob in data:
					print("package_check: %s: a package file contains one of Tibia.exe's resources" % package)
					failures += 1
					break
		print("package_check: %s: %d files against %d real files, %d manifest hashes, %d of Tibia.exe's resources: "
			"%d matches" % (package, checked, len(real), len(manifest_hashes), len(blobs), failures))
	print("package_check: the format detectors recognize the %d real data files" % detected)
	return 1 if failures else 0


# -- starting a package -------------------------------------------------------------------------------------------


MISSING_DATA = "Cannot find file"


def wait_for_error(process, seconds):
	"""Reads the client's stderr until the missing-data error appears, the process ends or `seconds` pass; the lines."""
	lines = []

	def reader():
		for raw in process.stderr:
			lines.append(raw.decode("utf-8", "replace").rstrip("\r\n"))

	thread = threading.Thread(target=reader, daemon=True)
	thread.start()
	deadline = time.monotonic() + seconds
	while time.monotonic() < deadline:
		if any(MISSING_DATA in line and "Tibia.dat" in line for line in lines):
			return lines, True
		if process.poll() is not None:
			thread.join(5)
			return lines, any(MISSING_DATA in line and "Tibia.dat" in line for line in lines)
		time.sleep(0.2)
	return lines, False


def expect_exit_one(process, lines, found, label):
	"""After the error is on stderr (or not): stops the process if it is still on its box, and judges the run."""
	if process.poll() is None:
		process.kill()
		process.wait(30)
		ended = "stopped on its error box"
	else:
		ended = "exit code %d" % process.returncode
		if process.returncode != 1:
			found = False
	print("package_check: %s: %s" % (label, ended))
	for line in lines:
		print("  stderr: %s" % line)
	if not found:
		print("package_check: %s: no \"%s ... Tibia.dat\" line on stderr" % (label, MISSING_DATA))
		return 1
	print("package_check: %s: the original's error for a missing Tibia.dat" % label)
	return 0


def run_windows(package, work):
	if not os.environ.get("GITHUB_ACTIONS") and os.environ.get("USERNAME") != "WDAGUtilityAccount":
		print("package_check: --run on Windows is only for CI and Windows Sandbox (the error is a message box): "
			"python -I tools/sandbox.py run -- tools/package_check.py --run <package>")
		return 2
	import ctypes
	sys.path.insert(0, os.path.join(ROOT, "tools"))
	import client_start
	with zipfile.ZipFile(package) as z:
		z.extractall(work)
	exe = None
	for folder, _, names in os.walk(work):
		if "Patella.exe" in names:
			exe = os.path.join(folder, "Patella.exe")
	empty = os.path.join(work, "empty")
	os.makedirs(empty)
	env = dict(os.environ)
	env["VK_LOADER_DRIVERS_DISABLE"] = "*"
	env["PATELLA_VULKAN"] = "cpu"
	process = subprocess.Popen([exe], cwd=empty, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
	lines, found = wait_for_error(process, 120)
	# The box is FatalError's: closed the way a user closes it, then the exit code is the original's 1.
	deadline = time.monotonic() + 20
	while process.poll() is None and time.monotonic() < deadline:
		for hwnd, caption, text in client_start.message_boxes(process.pid):
			print("package_check: its box: %r %r" % (caption, text))
			ctypes.windll.user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
		time.sleep(0.5)
	return expect_exit_one(process, lines, found, package)


LAVAPIPE_MANIFEST = "/usr/share/vulkan/icd.d/lvp_icd.json"


def run_linux(package, kind, work):
	empty = os.path.join(work, "empty")
	os.makedirs(empty)
	env = dict(os.environ)
	env["PATELLA_VULKAN"] = "cpu"
	env["VK_DRIVER_FILES"] = env["VK_ICD_FILENAMES"] = LAVAPIPE_MANIFEST
	env["DISPLAY"] = ":135"
	env.pop("WAYLAND_DISPLAY", None)
	if kind == "linux-tgz":
		with tarfile.open(package) as t:
			t.extractall(work, filter="fully_trusted")
		exe = None
		for folder, _, names in os.walk(work):
			if "Patella" in names and os.path.basename(folder) == "bin":
				exe = os.path.join(folder, "Patella")
		command = [exe]
	else:
		env["APPIMAGE_EXTRACT_AND_RUN"] = "1"
		env["TMPDIR"] = work
		copy = os.path.join(work, os.path.basename(package))
		with open(package, "rb") as source, open(copy, "wb") as target:
			target.write(source.read())
		os.chmod(copy, 0o755)
		command = [copy]
	xvfb = subprocess.Popen(["Xvfb", env["DISPLAY"], "-screen", "0", "1024x768x24", "-ac"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
	try:
		time.sleep(2)
		process = subprocess.Popen(command, cwd=empty, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
			stderr=subprocess.PIPE)
		lines, found = wait_for_error(process, 120)
		time.sleep(1)
		return expect_exit_one(process, lines, found, package)
	finally:
		xvfb.terminate()
		xvfb.wait(30)


def run_package(package):
	kind = kind_of(package)
	package = os.path.abspath(package)
	base = os.path.join(os.path.dirname(package), "package-run")
	os.makedirs(base, exist_ok=True)
	work = tempfile.mkdtemp(dir=base, prefix="run-")
	try:
		if kind == "windows-zip":
			return run_windows(package, work)
		return run_linux(package, kind, work)
	finally:
		# The folder of this run: the package's own files and the client's empty working folder.
		shutil.rmtree(work, ignore_errors=True)


# -- self-test ----------------------------------------------------------------------------------------------------


def fake_pe(resources, machine=0x8664, marker=b""):
	"""A minimal PE32+ with a resource section of the given {(type, id): bytes} (language 0)."""
	ids = sorted(resources)
	types = sorted({kind for kind, _ in ids})
	section_rva = 0x1000
	# Layout: the root directory, each type's directory, each id's language directory, the data entries, the data.
	offset = 16 + 8 * len(types)
	type_offsets = {}
	for kind in types:
		type_offsets[kind] = offset
		offset += 16 + 8 * len([key for key in ids if key[0] == kind])
	language_offsets = {}
	for key in ids:
		language_offsets[key] = offset
		offset += 16 + 8
	entry_offsets = {}
	for key in ids:
		entry_offsets[key] = offset
		offset += 16
	data_offsets = {}
	for key in ids:
		data_offsets[key] = offset
		offset += len(resources[key])
	blob = bytearray()

	def directory(count):
		blob.extend(struct.pack("<IIHHHH", 0, 0, 0, 0, 0, count))

	directory(len(types))
	for kind in types:
		blob.extend(struct.pack("<II", kind, 0x80000000 | type_offsets[kind]))
	for kind in types:
		of_type = [key for key in ids if key[0] == kind]
		directory(len(of_type))
		for key in of_type:
			blob.extend(struct.pack("<II", key[1], 0x80000000 | language_offsets[key]))
	for key in ids:
		directory(1)
		blob.extend(struct.pack("<II", 0, entry_offsets[key]))
	for key in ids:
		blob.extend(struct.pack("<IIII", section_rva + data_offsets[key], len(resources[key]), 0, 0))
	for key in ids:
		blob.extend(resources[key])
	blob = bytes(blob)
	header = bytearray(0x400)
	header[:2] = b"MZ"
	struct.pack_into("<I", header, 0x3c, 0x80)
	header[0x80:0x84] = b"PE\0\0"
	struct.pack_into("<HH", header, 0x84, machine, 1)
	struct.pack_into("<H", header, 0x94, 240)  # the optional header's size
	struct.pack_into("<H", header, 0x98, 0x20b)
	struct.pack_into("<II", header, 0x98 + 112 + 16, section_rva, len(blob))
	section = 0x98 + 240
	header[section:section + 8] = b".rsrc\0\0\0"
	struct.pack_into("<IIII", header, section + 8, len(blob), section_rva, len(blob), 0x400)
	return bytes(header) + blob + marker


def fake_elf(marker=b""):
	head = bytearray(64)
	head[:4] = b"\x7fELF"
	head[4] = 2
	head[5] = 1
	head[6] = 1
	struct.pack_into("<H", head, 0x12, 0x3e)
	return bytes(head) + marker


def self_test():
	failures = []

	def expect(label, ok):
		print("self-test: %s: %s" % ("ok  " if ok else "FAIL", label))
		if not ok:
			failures.append(label)

	_, cursor, icon = placeholders()
	good_resources = {(RT_CURSOR, 1): cursor, (RT_CURSOR, 2): cursor, (RT_ICON, 3): icon}
	good_pe = fake_pe(good_resources, marker=PLACEHOLDER_WARNING)
	wrong_cursor = fake_pe({(RT_CURSOR, 1): b"\x10\0\x10\0real", (RT_CURSOR, 2): cursor, (RT_ICON, 3): icon}, marker=PLACEHOLDER_WARNING)
	with_bitmap = fake_pe({**good_resources, (RT_BITMAP, 9): b"bitmap"}, marker=PLACEHOLDER_WARNING)
	no_warning = fake_pe(good_resources)
	good_elf = fake_elf(cursor + PLACEHOLDER_WARNING)
	no_cursor_elf = fake_elf(PLACEHOLDER_WARNING)
	# Files in the data files' formats, made up here (nothing of CipSoft's).
	spr = struct.pack("<IHIII", 0x12345678, 3, 0, 18, 26) + bytes(30)
	pic = struct.pack("<IHBBBBB", 0x12345678, 1, 1, 1, 255, 0, 255) + struct.pack("<I", 15) + bytes(10)
	dat_entry = bytes([0x00, 0x05, 0x00, 0xff, 1, 1, 1, 1, 1, 1, 1, 0x34, 0x12])
	dat = struct.pack("<I4H", 0x12345678, 100, 1, 1, 1) + dat_entry * 4
	cfg = b"TestServer = no\nFullscreenMode = (800,600,32)\nConsoleHeight = 140\n"

	def windows_files(exe=good_pe, extra=(), drop=()):
		files = {"Patella.exe": exe, "vulkan_lvp.dll": fake_pe({}), "lvp_icd.x86_64.json": b"{}", "README.txt": b"text"}
		for name in EXPECTED["windows-zip"]:
			files.setdefault(name, b"text")
		for name in drop:
			del files[name]
		files.update(dict(extra))
		return files

	def linux_files(exe=good_elf, extra=()):
		files = {name: b"text" for name in EXPECTED["linux-tgz"]}
		files["bin/Patella"] = exe
		files.update(dict(extra))
		return files

	with tempfile.TemporaryDirectory(prefix="package-check-") as folder:
		def zip_package(files, name):
			path = os.path.join(folder, name)
			with zipfile.ZipFile(path, "w") as z:
				for entry, data in files.items():
					z.writestr("Patella-Modern-dev-windows-x64/" + entry, data)
			return path

		def tar_package(files, name):
			path = os.path.join(folder, name)
			with tarfile.open(path, "w:gz") as t:
				for entry, data in files.items():
					info = tarfile.TarInfo("Patella-Modern-dev-linux-x64/" + entry)
					info.size = len(data)
					t.addfile(info, io.BytesIO(data))
			return path

		def problems(path):
			return check_tree(*reversed(read_package(path)))

		expect("a good zip passes", problems(zip_package(windows_files(), "good.zip")) == [])
		expect("a good tarball passes", problems(tar_package(linux_files(), "good.tar.gz")) == [])
		found = problems(zip_package(windows_files(extra={"Tibia.dat": b"dummy"}), "planted.zip"))
		expect("a planted Tibia.dat fails by its name", any("named like a data file" in p for p in found))
		found = problems(tar_package(linux_files(extra={"sub/TIBIA.SPR": b"dummy"}), "case.tar.gz"))
		expect("the name is checked in any case", any("named like a data file" in p for p in found))
		found = problems(zip_package(windows_files(extra={"notes/x.pic": b"dummy"}), "ext.zip"))
		expect("a data file's extension fails", any("extension" in p for p in found))
		found = problems(zip_package(windows_files(exe=wrong_cursor), "cursor.zip"))
		expect("a program with another cursor fails", any("RT_CURSOR" in p for p in found))
		found = problems(zip_package(windows_files(exe=with_bitmap), "bitmap.zip"))
		expect("a program with a bitmap fails", any("RT_BITMAP" in p for p in found))
		found = problems(zip_package(windows_files(exe=no_warning), "nowarning.zip"))
		expect("a program without the placeholder build's text fails", any("warning text" in p for p in found))
		found = problems(tar_package(linux_files(exe=no_cursor_elf), "nocursor.tar.gz"))
		expect("an ELF without the placeholder cursor fails", any("placeholder cursor" in p for p in found))
		found = problems(zip_package(windows_files(extra={"a.bin": spr}), "spr.zip"))
		expect("Tibia.spr's format under another name fails", any("format of Tibia.spr" in p for p in found))
		found = problems(zip_package(windows_files(extra={"b.bin": pic}), "pic.zip"))
		expect("Tibia.pic's format under another name fails", any("format of Tibia.pic" in p for p in found))
		found = problems(zip_package(windows_files(extra={"c.bin": dat}), "dat.zip"))
		expect("Tibia.dat's format under another name fails", any("format of Tibia.dat" in p for p in found))
		found = problems(zip_package(windows_files(extra={"d.txt": cfg}), "cfg.zip"))
		expect("Tibia.cfg's format under another name fails", any("format of Tibia.cfg" in p for p in found))
		found = problems(zip_package(windows_files(extra={"old.exe": fake_pe({}, machine=0x14c)}), "pe32.zip"))
		expect("a 32-bit PE fails", any("32-bit PE" in p for p in found))
		found = problems(zip_package(windows_files(extra={"stray.txt": b"x"}), "stray.zip"))
		expect("a file off the list fails", any("not on this package's list" in p for p in found))
		found = problems(zip_package(windows_files(drop=["licenses/SDL3.txt"]), "nolicence.zip"))
		expect("a missing licence fails", any("licenses/SDL3.txt: missing" in p for p in found))
		stage = os.path.join(folder, "stage", "Patella-Modern-dev-windows-x64")
		for entry, data in windows_files().items():
			path = os.path.join(stage, entry)
			os.makedirs(os.path.dirname(path), exist_ok=True)
			with open(path, "wb") as f:
				f.write(data)
		expect("--staged passes a good tree", check_staged(os.path.join(folder, "stage")) == 0)
		with open(os.path.join(stage, "Tibia.dat"), "wb") as f:
			f.write(b"dummy")
		expect("--staged fails a planted Tibia.dat", check_staged(os.path.join(folder, "stage")) == 1)
	if failures:
		print("package_check: %d self-test failure(s)" % len(failures))
		return 1
	print("package_check: self-test passed")
	return 0


def main(argv):
	if not argv or argv[0] in ("-h", "--help"):
		print(__doc__)
		return 0 if argv else 2
	if argv[0] == "--self-test":
		return self_test()
	if argv[0] == "--placeholders" and len(argv) == 2:
		return check_placeholders(argv[1])
	if argv[0] == "--staged" and len(argv) == 2:
		return check_staged(argv[1])
	if argv[0] == "--local":
		rest = argv[1:]
		if not rest:
			return 2
		data_folder = rest.pop(0)
		manifests = []
		while len(rest) >= 2 and rest[0] == "--manifest":
			manifests.append(rest[1])
			rest = rest[2:]
		return check_local(data_folder, manifests, rest) if rest else 2
	if argv[0] == "--run":
		return max(run_package(package) for package in argv[1:]) if argv[1:] else 2
	return max(check_package(package) for package in argv)


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
