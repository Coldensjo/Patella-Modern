r"""Build the client twice, in two different folders, and compare the binaries byte for byte (issue #136).

Usage:
  python -I tools/repro_check.py <preset> [<preset> ...] [--keep] [--work <folder>] [--show-strings]
  python -I tools/repro_check.py --list

<preset> is windows-x64, linux-x64-gcc or linux-x64-clang. Each is configured and built twice (Release, the targets
Patella and PatellaTest) from two copies of the repository's files that git knows (tracked, and untracked ones that are not
ignored: what tools/build.py gives the Linux image too, never Tibia.exe or the data files), in folders of different names
and depths. Source folder, build folder and _deps folder therefore all differ between the two, so a path that reaches a
binary is found. Nothing in the repository's own build folder is touched. Each preset's two trees are removed when
it is compared; --keep leaves them for a look. --work names the parent of the run's folder (default: the system's
temporary folder).

The files compared are named below (ARTIFACTS): the executables are required to be identical, and a difference in one of them
ends the run with exit code 1. The debug information next to them (ARTIFACTS' second list) is compared too and always reported,
but its difference does not fail the run: a PDB records where the objects were built (notes/decisions.md, "#136").

The report names each file, its size, whether it is identical, and for a file that differs the number of differing byte
ranges and the first ones, with the PE or ELF section each lies in. It prints offsets only, never the bytes or a hash of a
binary (the local builds embed pictures of the local Tibia.exe; cmake/Resources.cmake); --show-strings adds the printable text
around each range, which is how an embedded path shows itself, and is for the local screen and logs. All of this output
stays on this machine.

On Windows a Linux preset runs inside the build image (tools/build.py --env), with this tool as the command there, its two trees
under the bind-mounted build/<preset> (not the image's fixed /work, which is the same for every build; not a container
tmpfs, which the VM's memory may not hold). On a machine without Tibia.exe (PATELLA_TIBIA_EXE) both builds take the neutral placeholder pictures (PATELLA_PLACEHOLDER_RESOURCES), as
in the hosted CI, and the comparison means the same.

Exit code: 0 when every required file is identical in every preset, 1 when one differs, 2 when a build failed.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import build  # noqa: E402

# (required identical, debug information reported only), relative to the build folder.
ARTIFACTS = {
	"windows-x64": (["Release/Patella.exe", "Release/PatellaTest.exe"], ["Release/Patella.pdb", "Release/PatellaTest.pdb"]),
	"linux-x64-gcc": (["Release/Patella", "Release/PatellaTest"], []),
	"linux-x64-clang": (["Release/Patella", "Release/PatellaTest"], []),
}
TARGETS = ["Patella", "PatellaTest"]
# Differing runs closer than this are one range in the report.
RANGE_GAP = 16
# How many ranges of a differing file are listed.
MAX_RANGES = 20
BLOCK = 1 << 16


def source_files():
	"""The repository's files, relative to ROOT: git's list when there is a .git (the host), else every file but the build
	folder (the Linux image's /work holds what tools/build.py copied for this run)."""
	if os.path.exists(os.path.join(ROOT, ".git")):
		listed = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
			cwd=ROOT, capture_output=True, check=True).stdout
		return [os.fsdecode(name) for name in listed.split(b"\0")
			if name and os.path.lexists(os.path.join(ROOT, os.fsdecode(name)))]
	names = []
	for folder, subfolders, files in os.walk(ROOT):
		if os.path.samefile(folder, ROOT) and "build" in subfolders:
			subfolders.remove("build")
		names += [os.path.relpath(os.path.join(folder, file), ROOT) for file in files]
	return names


def export_tree(target):
	"""A copy of the repository's files in `target`, with their times."""
	for name in source_files():
		destination = os.path.join(target, name)
		os.makedirs(os.path.dirname(destination), exist_ok=True)
		shutil.copy2(os.path.join(ROOT, name), destination)


def seed_downloads(preset, trees):
	"""The pinned archives (cmake/Download.cmake) of the repository's own build folder, when it has them, for both trees: the
	same files either way, without the network."""
	source = os.path.join(ROOT, "build", preset, "_deps", "patella-downloads")
	if not os.path.isdir(source):
		return
	for tree in trees:
		shutil.copytree(source, os.path.join(tree, "build", preset, "_deps", "patella-downloads"), dirs_exist_ok=True)


def run_logged(command, env, log, cwd):
	"""Runs the command with its output appended to `log`; returns its exit code."""
	with open(log, "ab") as out:
		out.write(b"\n=== " + subprocess.list2cmdline(command).encode("utf-8") + b"\n")
		out.flush()
		return subprocess.call(command, env=env, cwd=cwd, stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)


def tail(path, lines=60):
	with open(path, "rb") as f:
		return b"\n".join(f.read().splitlines()[-lines:]).decode("utf-8", "replace")


def tibia_exe():
	"""The Tibia.exe the two builds embed from, or None (placeholders)."""
	if os.environ.get("PATELLA_TIBIA_EXE"):
		return os.environ["PATELLA_TIBIA_EXE"]
	path = os.path.join(ROOT, "Tibia.exe")
	return path if os.path.isfile(path) else None


def build_tree(preset, tree, log):
	"""Configures and builds the preset in `tree`, through that tree's own tools/build.py. Returns an exit code."""
	tool = os.path.join(tree, "tools", "build.py")
	env = dict(os.environ)
	configure = [sys.executable, "-I", tool, "--env", preset, "cmake", "--preset", preset]
	exe = tibia_exe()
	if exe is None:
		env["PATELLA_PLACEHOLDER_RESOURCES"] = "1"
	if os.name == "nt":
		configure.append("-DPATELLA_TIBIA_EXE=" + exe if exe else "-DPATELLA_PLACEHOLDER_RESOURCES=ON")
	code = run_logged(configure, env, log, tree)
	if code != 0:
		return code
	return run_logged([sys.executable, "-I", tool, "--env", preset, "cmake", "--build", "--preset", preset, "--target"] + TARGETS,
		env, log, tree)


def sections(data):
	"""[(name, start, end)] of a PE or ELF64 file's sections, as file offsets; empty for anything else."""
	if data[:2] == b"MZ" and len(data) > 0x40:
		pe = struct.unpack_from("<I", data, 0x3C)[0]
		if data[pe:pe + 4] != b"PE\0\0":
			return []
		count, optional = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<H", data, pe + 20)[0]
		table = pe + 24 + optional
		found = [("headers", 0, table + 40 * count)]
		for i in range(count):
			raw = data[table + 40 * i:table + 40 * i + 40]
			name = raw[:8].rstrip(b"\0").decode("latin-1")
			size, start = struct.unpack_from("<II", raw, 16)
			found.append((name, start, start + size))
		return found
	if data[:4] == b"\x7fELF" and data[4] == 2:
		shoff, = struct.unpack_from("<Q", data, 0x28)
		size, count, names = struct.unpack_from("<HHH", data, 0x3A)
		if shoff == 0:
			return []
		name_offset, = struct.unpack_from("<Q", data, shoff + names * size + 0x18)
		found = [("headers", 0, 64)]
		for i in range(count):
			entry = data[shoff + i * size:shoff + (i + 1) * size]
			name, = struct.unpack_from("<I", entry, 0)
			kind, = struct.unpack_from("<I", entry, 4)
			start, length = struct.unpack_from("<QQ", entry, 0x18)
			end = name_offset + name
			label = data[end:data.index(b"\0", end)].decode("latin-1")
			if kind != 8 and length:  # 8: SHT_NOBITS, no file bytes
				found.append((label, start, start + length))
		found.append(("section headers", shoff, shoff + count * size))
		return found
	return []


def section_of(table, offset):
	for name, start, end in table:
		if start <= offset < end:
			return name
	return "outside the sections"


def differing_ranges(a, b):
	"""The ranges [(start, end)] where two equal-length byte strings differ, runs closer than RANGE_GAP merged."""
	ranges = []
	view_a, view_b = memoryview(a), memoryview(b)
	for block in range(0, len(a), BLOCK):
		if view_a[block:block + BLOCK] == view_b[block:block + BLOCK]:
			continue
		for i in range(block, min(block + BLOCK, len(a))):
			if a[i] != b[i]:
				if ranges and i - ranges[-1][1] < RANGE_GAP:
					ranges[-1][1] = i + 1
				else:
					ranges.append([i, i + 1])
	return ranges


def printable_around(data, start):
	"""The printable ASCII text around a file offset (a path, usually), for --show-strings."""
	low = start
	while low > 0 and start - low < 200 and 32 <= data[low - 1] < 127:
		low -= 1
	high = start
	while high < len(data) and high - start < 200 and 32 <= data[high] < 127:
		high += 1
	return data[low:high].decode("latin-1")


def compare(a_path, b_path, show_strings):
	"""(identical, the report's words) for two files."""
	with open(a_path, "rb") as f:
		a = f.read()
	with open(b_path, "rb") as f:
		b = f.read()
	if a == b:
		return True, "identical, %d bytes" % len(a)
	if len(a) != len(b):
		common = min(len(a), len(b))
		first = next((i for i in range(common) if a[i] != b[i]), common)
		return False, "DIFFERENT, %d and %d bytes; first differing offset 0x%x (%s)" % (len(a), len(b), first,
			section_of(sections(a), first))
	ranges = differing_ranges(a, b)
	table = sections(a)
	listed = ["0x%x..0x%x (%s)" % (start, end, section_of(table, start)) for start, end in ranges[:MAX_RANGES]]
	words = "DIFFERENT, %d bytes, %d differing ranges: %s%s" % (len(a), len(ranges), ", ".join(listed),
		", ..." if len(ranges) > MAX_RANGES else "")
	if show_strings:
		for start, end in ranges[:MAX_RANGES]:
			words += "\n    0x%x: A %r | B %r" % (start, printable_around(a, start), printable_around(b, start))
	return False, words


def check_preset(preset, work, show_strings):
	"""Builds the preset twice under `work` and compares. Returns 0, 1 or 2 as the module's exit code."""
	# Different names and depths, so that a path of either build could only match by accident.
	a = os.path.join(work, preset, "a")
	b = os.path.join(work, preset, "folder of another length", "b")
	logs = os.path.join(work, preset)
	print("repro_check: %s: build A in %s" % (preset, a), flush=True)
	print("repro_check: %s: build B in %s" % (preset, b), flush=True)
	for tree in (a, b):
		os.makedirs(tree)
		export_tree(tree)
	seed_downloads(preset, (a, b))
	for name, tree in (("A", a), ("B", b)):
		log = os.path.join(logs, "build-%s.log" % name)
		started = time.monotonic()
		code = build_tree(preset, tree, log)
		print("repro_check: %s: build %s: exit %d, %.0f s" % (preset, name, code, time.monotonic() - started), flush=True)
		if code != 0:
			print(tail(log), flush=True)
			print("repro_check: %s: the build failed; its log: %s" % (preset, log), flush=True)
			return 2
	required, reported = ARTIFACTS[preset]
	result = 0
	for name in required + reported:
		pair = [os.path.join(tree, "build", preset, *name.split("/")) for tree in (a, b)]
		if not all(os.path.isfile(path) for path in pair):
			print("repro_check: %s: %s: MISSING in %s" % (preset, name, ", ".join(p for p in pair if not os.path.isfile(p))),
				flush=True)
			if name in required:
				result = 1
			continue
		same, words = compare(*pair, show_strings)
		kind = "" if name in required else " (debug information, reported only)"
		print("repro_check: %s: %s%s: %s" % (preset, name, kind, words), flush=True)
		if not same and name in required:
			result = 1
	return result


def run_in_image(preset, keep, show_strings):
	"""Runs this tool for the preset inside the Linux build image; its trees are under the bind-mounted build folder."""
	env = dict(os.environ)
	command = [sys.executable, "-I", os.path.join(ROOT, "tools", "build.py"), "--env", preset, "python3", "-I",
		"/work/tools/repro_check.py", preset, "--inside-image", "--work", "/work/build/" + preset] + (["--show-strings"] if show_strings else [])
	if keep:
		command.append("--keep")
	return subprocess.call(command, env=env, cwd=ROOT, stdin=subprocess.DEVNULL)


def describe_commit():
	if not os.path.exists(os.path.join(ROOT, ".git")):
		return
	head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
	dirty = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
	print("repro_check: commit %s%s" % (head, " with local changes" if dirty else ""), flush=True)


def main(argv):
	parser = argparse.ArgumentParser(description="Build twice in different folders and compare the binaries (issue #136).")
	parser.add_argument("presets", nargs="*", help="windows-x64, linux-x64-gcc, linux-x64-clang")
	parser.add_argument("--list", action="store_true", help="the presets and the files each compares")
	parser.add_argument("--keep", action="store_true", help="leave the two trees of a Windows preset in the run's folder")
	parser.add_argument("--work", help="the parent folder of the run's folder (default: the temporary folder)")
	parser.add_argument("--show-strings", action="store_true",
		help="print the printable text around each differing range, the build's own paths usually (local output only)")
	parser.add_argument("--inside-image", action="store_true", help=argparse.SUPPRESS)
	args = parser.parse_args(argv)
	if args.list:
		for preset, (required, reported) in ARTIFACTS.items():
			print("%-18s required: %s; reported: %s" % (preset, ", ".join(required), ", ".join(reported) or "none"))
		return 0
	if not args.presets:
		parser.print_help()
		return 2
	unknown = [preset for preset in args.presets if preset not in ARTIFACTS]
	if unknown:
		print("repro_check: not a checked preset: %s (--list)" % ", ".join(unknown), file=sys.stderr)
		return 2
	describe_commit()
	result = 0
	for preset in args.presets:
		if not args.inside_image and build.in_docker(preset):
			code = run_in_image(preset, args.keep, args.show_strings)
		else:
			work = tempfile.mkdtemp(prefix="patella-repro-", dir=args.work)
			try:
				code = check_preset(preset, work, args.show_strings)
			finally:
				if args.keep:
					print("repro_check: kept %s" % work, flush=True)
				else:
					# shutil.rmtree unlinks a link or junction instead of following it, and this folder is the run's own.
					shutil.rmtree(work, ignore_errors=True)
		print("repro_check: %s: %s" % (preset, {0: "identical", 1: "NOT IDENTICAL", 2: "BUILD FAILED"}.get(code, "exit %d" % code)),
			flush=True)
		result = max(result, code)
	return result


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
