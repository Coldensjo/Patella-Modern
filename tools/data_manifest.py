r"""The manifest of a frozen data set (Patella-Modern issue #93): MANIFEST.tsv in the folder of the golden corpus
(tests/golden, recorded by oracle-x86's golden_corpus) and of the reference recordings (tools/reference.py).

Usage (from the repository or a worktree):
  python -I tools/data_manifest.py write --kind golden|reference --time-zone TZ [--tibia Tibia.exe]
                                         [--tool name=value ...] [--program name=path ...]
                                         [--commit HASH] [--date DATE] FOLDER
  python -I tools/data_manifest.py verify [--kind golden|reference] FOLDER
  python -I tools/data_manifest.py --self-test

The data sets are CipSoft-derived (local only, never committed, pushed or uploaded); so is the manifest, which
stays in the folder with them. Only this format and the code that writes and checks it are in the repository.

The format (UTF-8, LF line ends, one field per line, tab-separated; a line starting with '#' is a comment):
	format	patella-data-manifest 1           the first line
	kind	golden | reference
	commit	<hash>                            the commit the data was recorded at, or "unknown"
	clean	yes | no | unknown                whether that working tree had no changes to tracked files
	date	<YYYY-MM-DDThh:mm:ssZ>            when it was recorded (UTC), or a free text when unknown
	time_zone	<POSIX TZ>                     the zone of the local times in the data (issue #93: UTC0)
	tibia_sha256	<hex>                       the Tibia.exe it was recorded from
	tool	<name>	<value>                     the recorder's tools (any number, in order)
	program	<name>	<size>	<sha256>          the recorder's programs (any number, in order)
	manifest	<commit>	<date>              who wrote the manifest (when it was written later than the data)
	files	<count>	<bytes>                     the files below
	file	<size>	<sha256>	<path>           every file in the folder but MANIFEST.tsv, '/' separated, sorted
golden_runner (tests/golden/golden.cpp, golden::VerifyDataManifest) and tools/reference.py read it: a folder
matches its manifest when every listed file is there with its size and SHA-256, no other file is, and the counts
are the "files" line's. Each refuses a missing or mismatched manifest; --unverified (golden_runner also
PATELLA_GOLDEN_UNVERIFIED=1) replays anyway, for local experiments.
"""

import argparse
import datetime
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAME = "MANIFEST.tsv"
FORMAT = "patella-data-manifest 1"
KINDS = ("golden", "reference")
# How many problems verify lists before it says "and n more".
SHOWN = 20


class ManifestError(Exception):
	pass


def file_sha(path):
	digest = hashlib.sha256()
	with open(path, "rb") as f:
		for chunk in iter(lambda: f.read(1 << 20), b""):
			digest.update(chunk)
	return digest.hexdigest()


def git(*args):
	"""git's output in the repository, or None when git fails."""
	try:
		result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
	except OSError:
		return None
	return result.stdout.strip() if result.returncode == 0 else None


def commit_facts():
	"""(the repository's HEAD, "yes"/"no"/"unknown": whether its tracked files are unchanged)."""
	head = git("rev-parse", "HEAD") or "unknown"
	status = git("status", "--porcelain", "--untracked-files=no")
	return head, "unknown" if status is None else "yes" if status == "" else "no"


def utc_now():
	return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def listing(folder):
	"""Every file below folder but the manifest: [(path with '/', full path)], sorted by path."""
	found = []
	for directory, folders, files in os.walk(folder):
		folders.sort()
		for name in files:
			full = os.path.join(directory, name)
			relative = os.path.relpath(full, folder).replace(os.sep, "/")
			if relative != NAME:
				found.append((relative, full))
	return sorted(found)


def check_field(name, value):
	if "\t" in value or "\n" in value or "\r" in value:
		raise ManifestError("%s: %r has a tab or a line end" % (name, value))
	return value


def write(folder, kind, time_zone, tibia_sha256, tools=(), programs=(), commit=None, clean=None, date=None,
		manifest=None):
	"""Writes folder's MANIFEST.tsv; returns (file count, bytes). tools: [(name, value)]; programs: [(name,
	path)], hashed here; commit, clean, date: the recording's (default: this repository's HEAD, its state, now);
	manifest: (commit, date) of a manifest written after the data, or None."""
	if kind not in KINDS:
		raise ManifestError("kind %r is not one of %s" % (kind, ", ".join(KINDS)))
	if not os.path.isdir(folder):
		raise ManifestError("%s is not a folder" % folder)
	head, state = commit_facts()
	lines = ["format\t" + FORMAT, "kind\t" + kind, "commit\t" + check_field("commit", commit or head),
		"clean\t" + check_field("clean", clean or state), "date\t" + check_field("date", date or utc_now()),
		"time_zone\t" + check_field("time zone", time_zone), "tibia_sha256\t" + check_field("tibia", tibia_sha256)]
	for name, value in tools:
		lines.append("tool\t%s\t%s" % (check_field("tool", name), check_field("tool", value)))
	for name, path in programs:
		if not os.path.isfile(path):
			raise ManifestError("program %s: %s does not exist" % (name, path))
		lines.append("program\t%s\t%d\t%s" % (check_field("program", name), os.path.getsize(path), file_sha(path)))
	if manifest is not None:
		lines.append("manifest\t%s\t%s" % (check_field("manifest", manifest[0]), check_field("manifest", manifest[1])))
	files = [(relative, os.path.getsize(full), file_sha(full)) for relative, full in listing(folder)]
	if not files:
		raise ManifestError("%s has no files" % folder)
	total = sum(size for _, size, _ in files)
	lines.append("files\t%d\t%d" % (len(files), total))
	for relative, size, digest in files:
		lines.append("file\t%d\t%s\t%s" % (size, digest, check_field("path", relative)))
	path = os.path.join(folder, NAME)
	temporary = path + ".new"
	with open(temporary, "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(lines) + "\n")
	os.replace(temporary, path)
	return len(files), total


def read(folder):
	"""The manifest of folder as a dict: its single fields by name, "tool" and "program" lists, "files" [(path,
	size, sha256)], "count" and "bytes". Raises ManifestError when there is none or it is malformed."""
	path = os.path.join(folder, NAME)
	if not os.path.isfile(path):
		raise ManifestError("%s has no %s" % (folder, NAME))
	fields = {"tool": [], "program": [], "files": []}
	with open(path, encoding="utf-8", newline="") as f:
		lines = f.read().split("\n")
	if lines and lines[-1] == "":
		lines.pop()
	for number, line in enumerate(lines, 1):
		if line.startswith("#"):
			continue
		parts = line.split("\t")
		key = parts[0]
		try:
			if number == 1 and line != "format\t" + FORMAT:
				raise ManifestError("the first line is %r, not %r" % (line, "format\t" + FORMAT))
			if key in ("format", "kind", "commit", "clean", "date", "time_zone", "tibia_sha256") and len(parts) == 2:
				if key in fields:
					raise ManifestError("%s twice" % key)
				fields[key] = parts[1]
			elif key == "tool" and len(parts) == 3:
				fields["tool"].append((parts[1], parts[2]))
			elif key == "program" and len(parts) == 4:
				fields["program"].append((parts[1], int(parts[2]), parts[3]))
			elif key == "manifest" and len(parts) == 3:
				fields["manifest"] = (parts[1], parts[2])
			elif key == "files" and len(parts) == 3:
				fields["count"], fields["bytes"] = int(parts[1]), int(parts[2])
			elif key == "file" and len(parts) == 4:
				fields["files"].append((parts[3], int(parts[1]), parts[2]))
			else:
				raise ManifestError("unknown line %r" % line)
		except ValueError:
			raise ManifestError("%s, line %d: not a number in %r" % (path, number, line)) from None
		except ManifestError as e:
			raise ManifestError("%s, line %d: %s" % (path, number, e)) from None
	for key in ("format", "kind", "commit", "date", "time_zone", "count"):
		if key not in fields:
			raise ManifestError("%s has no %s line" % (path, "files" if key == "count" else key))
	return fields


def verify(folder, kind=None):
	"""(the manifest, the problems: empty when the folder is exactly its files). Raises ManifestError when there is
	no readable manifest."""
	manifest = read(folder)
	problems = []
	if kind is not None and manifest["kind"] != kind:
		problems.append("the manifest is of kind %r, not %r" % (manifest["kind"], kind))
	listed = {path: (size, digest) for path, size, digest in manifest["files"]}
	if len(listed) != len(manifest["files"]):
		problems.append("the manifest lists a file twice")
	if len(manifest["files"]) != manifest["count"] or sum(s for _, s, _ in manifest["files"]) != manifest["bytes"]:
		problems.append("the manifest lists %d files of %d bytes, its files line says %d of %d" % (
			len(manifest["files"]), sum(s for _, s, _ in manifest["files"]), manifest["count"], manifest["bytes"]))
	present = dict(listing(folder))
	for path in sorted(listed):
		size, digest = listed[path]
		full = present.get(path)
		if full is None:
			problems.append("%s is missing" % path)
		elif os.path.getsize(full) != size:
			problems.append("%s has %d bytes, the manifest %d" % (path, os.path.getsize(full), size))
		elif file_sha(full) != digest:
			problems.append("%s has another SHA-256 than the manifest's" % path)
	for path in sorted(set(present) - set(listed)):
		problems.append("%s is not in the manifest" % path)
	return manifest, problems


def describe(manifest):
	"""One line about a manifest: the commit, the date, the files, the zone."""
	return "recorded at %s (%s) on %s, %d files, %d bytes, time zone %s" % (manifest["commit"][:12],
		"clean" if manifest.get("clean") == "yes" else "tree " + manifest.get("clean", "unknown"), manifest["date"],
		manifest["count"], manifest["bytes"], manifest["time_zone"])


def problem_lines(problems):
	shown = ["  " + p for p in problems[:SHOWN]]
	if len(problems) > SHOWN:
		shown.append("  ... and %d more" % (len(problems) - SHOWN))
	return shown


# --- self-test ------------------------------------------------------------------------------------

def self_test():
	failures = []

	def check(what, condition):
		if not condition:
			failures.append(what)
			print("FAILED: " + what)

	folder = tempfile.mkdtemp(prefix="data-manifest-test-")
	try:
		os.makedirs(os.path.join(folder, "a"))
		with open(os.path.join(folder, "a", "one.bin"), "wb") as f:
			f.write(b"\x00\x01\x02")
		with open(os.path.join(folder, "two.txt"), "wb") as f:
			f.write(b"two\n")
		program = os.path.join(folder, "a", "one.bin")
		count, total = write(folder, "golden", "UTC0", "ab" * 32, tools=[("msvc", "19.0")], programs=[("one", program)],
			commit="c" * 40, clean="yes", date="2026-11-06T00:00:00Z")
		check("write counts the files", (count, total) == (2, 7))
		manifest, problems = verify(folder, "golden")
		check("a written manifest verifies", problems == [])
		check("the fields are read back", manifest["time_zone"] == "UTC0" and manifest["tool"] == [("msvc", "19.0")]
			and manifest["program"] == [("one", 3, file_sha(program))] and manifest["count"] == 2)
		check("another kind is a problem", len(verify(folder, "reference")[1]) == 1)
		with open(os.path.join(folder, "a", "one.bin"), "r+b") as f:
			f.write(b"\x09")
		check("a changed byte is found", verify(folder)[1] == ["a/one.bin has another SHA-256 than the manifest's"])
		with open(os.path.join(folder, "a", "one.bin"), "ab") as f:
			f.write(b"x")
		check("a changed size is found", verify(folder)[1] == ["a/one.bin has 4 bytes, the manifest 3"])
		os.remove(os.path.join(folder, "a", "one.bin"))
		with open(os.path.join(folder, "three"), "wb") as f:
			f.write(b"")
		check("a missing and an extra file are found",
			verify(folder)[1] == ["a/one.bin is missing", "three is not in the manifest"])
		with open(os.path.join(folder, NAME), "a", encoding="utf-8", newline="\n") as f:
			f.write("bogus\tline\n")
		try:
			read(folder)
			check("an unknown line is refused", False)
		except ManifestError:
			pass
		os.remove(os.path.join(folder, NAME))
		try:
			verify(folder)
			check("no manifest is refused", False)
		except ManifestError:
			pass
	finally:
		shutil.rmtree(folder, ignore_errors=True)  # the test's own folder of plain files
	print("self-test: %s" % ("passed" if not failures else "FAILED: %s" % failures))
	return 1 if failures else 0


def pairs(values, what):
	result = []
	for value in values or ():
		name, sep, rest = value.partition("=")
		if not sep or not name:
			raise SystemExit("--%s %r: name=value" % (what, value))
		result.append((name, rest))
	return result


def main(argv=None):
	parser = argparse.ArgumentParser(description="The manifest of a frozen data set (issue #93).")
	parser.add_argument("--self-test", action="store_true")
	sub = parser.add_subparsers(dest="command")
	w = sub.add_parser("write", help="write FOLDER's MANIFEST.tsv")
	w.add_argument("--kind", required=True, choices=KINDS)
	w.add_argument("--time-zone", required=True, help="the POSIX TZ of the data's local times")
	w.add_argument("--tibia", help="the Tibia.exe the data was recorded from (hashed)")
	w.add_argument("--tibia-sha256", help="its SHA-256, when the file is not given")
	w.add_argument("--tool", action="append", help="name=value: a tool of the recorder (repeated)")
	w.add_argument("--program", action="append", help="name=path: a program of the recorder, hashed (repeated)")
	w.add_argument("--commit", help="the commit the data was recorded at (default HEAD)")
	w.add_argument("--clean", choices=("yes", "no", "unknown"), help="whether its tree was clean (default: HEAD's)")
	w.add_argument("--date", help="when it was recorded (default now, UTC)")
	w.add_argument("folder")
	v = sub.add_parser("verify", help="check FOLDER against its MANIFEST.tsv")
	v.add_argument("--kind", choices=KINDS)
	v.add_argument("folder")
	args = parser.parse_args(argv)
	if args.self_test:
		return self_test()
	try:
		if args.command == "write":
			tibia = args.tibia_sha256 or (file_sha(args.tibia) if args.tibia else None)
			if not tibia:
				raise ManifestError("--tibia or --tibia-sha256 is needed")
			count, total = write(args.folder, args.kind, args.time_zone, tibia, pairs(args.tool, "tool"),
				pairs(args.program, "program"), args.commit, args.clean, args.date)
			print("data_manifest: %s: %d files, %d bytes" % (os.path.join(args.folder, NAME), count, total))
			return 0
		if args.command == "verify":
			manifest, problems = verify(args.folder, args.kind)
			print("data_manifest: %s: %s" % (args.folder, describe(manifest)))
			if problems:
				print("data_manifest: %d problems:" % len(problems))
				print("\n".join(problem_lines(problems)))
				return 1
			print("data_manifest: every file matches the manifest")
			return 0
	except ManifestError as e:
		print("data_manifest: %s" % e)
		return 1
	parser.print_help()
	return 2


if __name__ == "__main__":
	sys.exit(main())
