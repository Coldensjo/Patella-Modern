"""clang-tidy for src/, with the arguments of TASSERT*, TERROR* and TEND* left alone (issue #8).

Those macros stringize their arguments into the exception messages that error.txt records and difftest
compares (notes/modernization-plan.md, 4.2): until M5 no fix may change them, not even in whitespace.
This wrapper runs clang-tidy with the repository's .clang-tidy on each file and
- reports the findings outside those arguments (one line each, as clang-tidy writes them, and a count
  per check), and counts the findings inside them apart: they wait for M5;
- with --fix, lets clang-tidy apply its fixes and then reverts every edit inside a macro argument,
  character by character, in every file under src/ that changed (headers included).

The findings baseline (issue #11; plan 4.1, "no new findings"): --baseline <file> compares the findings
outside the macro arguments with a committed list and fails only on new ones; --write-baseline <file>
writes that list. A finding is counted by file, check and message, not by line, so code that moves keeps
its entry; a finding that is gone is reported (rewrite the list to drop it), a new one, or one more of
a kind, fails. CI (.github/workflows/hosted.yml) checks tools/tidy-baseline.tsv against the compile
commands of the preset linux-x64-clang in the Docker image of docker/linux-x64 (clang-tidy 22.1.8):

	python -I tools/tidy.py -p build/linux-x64-clang --all --baseline tools/tidy-baseline.tsv

tools/tidy_ci.py runs that job locally, with the same result on the same commit (issue #51).

Checks that must stay at zero (issue #63, plan section 7, M4: bugprone-* on the core library): --zero <prefix>
fails on any finding of a check whose name starts with the prefix, outside the macro arguments or inside them,
whatever the baseline says; such a finding is fixed, or suppressed in the code with a NOLINT that names the
check and the reason (notes/decisions.md, "Static analysis of the core library"). CI passes --zero bugprone-.

Headers (issue #49): clang-tidy checks the includes of the main file only (misc-include-cleaner), so a
header is checked as the main file of its own command. --headers takes the commands of the header checks
of cmake/HeaderCheck.cmake (the generated build/<preset>/header-check/<target>/<header>.cpp, one line
`#include "<header>"`, compiled with the settings of a target that uses the header) and runs clang-tidy on
each header itself with its source's command, in a compilation database of its own
(<build dir>/header-check/<target>/compile_commands.json). A header that is in several checks of the
build (another target's settings) is checked once per command. Only misc-include-cleaner runs on the
headers as main files, whatever --checks says: the other checks see every header already through the
sources that include it (HeaderFilterRegex), and on a header alone they would also report what no source
of the build instantiates or includes (Container.h's templates, compat/cxx26.h), which is not the
include check's business.

Usage:
	python -I tools/tidy.py [-p <build dir>] [--fix] [--checks <list>] [--extra-arg <arg>] [-v]
		[--baseline <file> | --write-baseline <file>] [--zero <prefix>] (--all | --headers | <file> ...)
	-p        the directory with compile_commands.json (CMAKE_EXPORT_COMPILE_COMMANDS; default
	          build/windows-x64, the preset of issue #5)
	--fix     apply the fixes outside the macro arguments
	--checks  passed to clang-tidy (added to .clang-tidy's)
	--extra-arg  passed to clang-tidy (repeatable), e.g. -m32 for an x86 build's commands: clang
	          takes the target from its own default, x64, not from the compiler of the build
	-v        also list the findings inside macro arguments
	--all     every source of compile_commands.json under src/ (not src/Extern), besides the files named
	--headers every header of the build's header checks (cmake/HeaderCheck.cmake) as a main file, with
	          misc-include-cleaner only (above), besides the files named
	--baseline        compare with this findings list: exit 1 only for findings that are not in it
	--write-baseline  write the findings to this list (the form --baseline reads)
	--zero    fail on any finding of the checks that start with this prefix (repeatable), the baseline's
	          and those inside macro arguments included
clang-tidy: the environment variable CLANG_TIDY, else clang-tidy on PATH, else Visual Studio's
(VC\\Tools\\Llvm\\x64\\bin). Exit code 0 without findings outside the macro arguments (with --baseline:
without new ones; with --zero: and none of those checks), 1 with, 2 when clang-tidy could not compile a file.
"""
import argparse
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'src'

# Every macro of src/ that stringizes an argument (the local ones of single files included); the same
# list as WhitespaceSensitiveMacros in .clang-format.
MACROS = (
	'TASSERT', 'TASSERT1', 'TASSERT2', 'TASSERTB', 'TASSERTB1', 'TASSERTB2', 'TASSERT1_BUG', 'TASSERT2_BUG',
	'TERROR', 'TERROR1', 'TERROR2', 'TERROR1B', 'TERROR_REASON', 'TERROR1_REASON',
	'TEND', 'TEND1', 'TEND2', 'TEND_HANDLERS', 'TMYEXCEPTION',
)
MACRO_CALL = re.compile(r'\b(' + '|'.join(MACROS) + r')\s*\(')
DIAGNOSTIC = re.compile(r'^(.*?):(\d+):(\d+): (warning|error): (.*?)(?: \[([^\]]+)\])?$')


def find_clang_tidy():
	if os.environ.get('CLANG_TIDY'):
		return os.environ['CLANG_TIDY']
	found = shutil.which('clang-tidy')
	if found:
		return found
	vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio' / 'Installer' / 'vswhere.exe'
	if vswhere.exists():
		out = subprocess.run([str(vswhere), '-latest', '-products', '*', '-property', 'installationPath'],
			capture_output=True, text=True).stdout.strip()
		candidate = Path(out) / 'VC' / 'Tools' / 'Llvm' / 'x64' / 'bin' / 'clang-tidy.exe'
		if out and candidate.exists():
			return str(candidate)
	sys.exit('tidy.py: clang-tidy not found (set CLANG_TIDY)')


def protected_spans(text):
	"""The character ranges [start, end) of every macro call of MACROS: the name, the parentheses and
	everything between them. Comments, strings and character literals are skipped when matching."""
	spans = []
	for m in MACRO_CALL.finditer(text):
		# Skip definitions (#define TASSERT(...)) and matches inside comments or strings: a call is
		# preceded on its line by code, not by '#define', '//' or an open string.
		line_start = text.rfind('\n', 0, m.start()) + 1
		before = text[line_start:m.start()]
		if before.lstrip().startswith('#define') or '//' in before or before.count('"') % 2:
			continue
		depth = 0
		i = m.end() - 1
		while i < len(text):
			c = text[i]
			if c in '"\'':
				j = i + 1
				while j < len(text) and text[j] != c:
					j += 2 if text[j] == '\\' else 1
				i = j
			elif text.startswith('//', i):
				i = text.find('\n', i)
				if i < 0:
					i = len(text)
				continue
			elif text.startswith('/*', i):
				i = text.find('*/', i)
				if i < 0:
					break
				i += 1
			elif c == '(':
				depth += 1
			elif c == ')':
				depth -= 1
				if depth == 0:
					spans.append((m.start(), i + 1))
					break
			i += 1
	return spans


def inside(spans, start, end=None):
	if end is None or end == start:
		return any(a < start < b for a, b in spans)
	return any(start < b and end > a for a, b in spans)


def offset(text, line, column):
	pos = 0
	for _ in range(line - 1):
		pos = text.index('\n', pos) + 1
	return pos + column - 1


def revert_in_macros(old, new):
	"""new with every edit that touches a protected span of old undone; the number of edits undone."""
	spans = protected_spans(old)
	old_lines = old.splitlines(keepends=True)
	new_lines = new.splitlines(keepends=True)
	starts = [0]
	for l in old_lines:
		starts.append(starts[-1] + len(l))
	result = []
	undone = 0
	for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, old_lines, new_lines, autojunk=False).get_opcodes():
		a = ''.join(old_lines[i1:i2])
		b = ''.join(new_lines[j1:j2])
		base = starts[i1]
		if tag == 'equal' or not inside(spans, base, base + len(a)) and not inside(spans, base):
			result.append(b)
			continue
		for t, k1, k2, l1, l2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
			if t != 'equal' and inside(spans, base + k1, base + k2):
				result.append(a[k1:k2])
				undone += 1
			else:
				result.append(b[l1:l2])
	return ''.join(result), undone


def compile_commands_files(build):
	"""Every source of compile_commands.json under src/ but not src/Extern, sorted, each once."""
	with open(Path(build) / 'compile_commands.json', encoding='utf-8') as f:
		entries = json.load(f)
	files = set()
	for entry in entries:
		path = Path(entry['directory'], entry['file']).resolve()
		try:
			relative = path.relative_to(SRC)
		except ValueError:
			continue
		if relative.parts and relative.parts[0] != 'Extern':
			files.add(path)
	return sorted(files)


HEADER_CHECK_INCLUDE = re.compile(r'^#include "([^"]+)"$', re.M)


def header_commands(build):
	"""The header checks' commands of compile_commands.json, each for the header itself: (database
	directory, header) pairs, one database per check target, so that a header that several targets
	check (with other settings) is checked with each target's command."""
	with open(Path(build) / 'compile_commands.json', encoding='utf-8') as f:
		entries = json.load(f)
	databases = {}
	for entry in entries:
		source = Path(entry['directory'], entry['file'])
		parts = source.parts
		if 'header-check' not in parts or source.suffix != '.cpp':
			continue
		target = parts[parts.index('header-check') + 1]
		m = HEADER_CHECK_INCLUDE.search(source.read_text(encoding='utf-8'))
		if not m:
			sys.exit(f'tidy.py: {source} is not a header check (no #include line)')
		header = (SRC / m.group(1)).resolve()
		if not header.exists():
			sys.exit(f'tidy.py: {source} includes {m.group(1)}, which is not in src/')
		command = entry.get('command')
		if command is None:
			command = ' '.join(entry['arguments'])
		# The header in the source's place. MSVC's commands say /TP (C++) already; GCC's and Clang's get
		# -x c++-header, so the driver does not take the .h for C. A command line names the source as
		# written in the entry ("file"), absolute with CMake.
		names = [n for n in (entry['file'], str(Path(entry['file']))) if command.count(n) == 1]
		if not names:
			sys.exit(f'tidy.py: the command of {source} does not name it once')
		replacement = str(header) if ' /TP ' in command else '-x c++-header ' + str(header)
		command = command.replace(names[0], replacement)
		# Ninja Multi-Config lists every configuration: the Release command, as for the sources.
		release = 'Release' in Path(entry.get('output', '')).parts
		items = databases.setdefault(target, {})
		if str(header) not in items or release:
			items[str(header)] = {'directory': entry['directory'], 'file': str(header), 'command': command}
	result = []
	for target, by_header in sorted(databases.items()):
		items = list(by_header.values())
		directory = Path(build) / 'header-check' / target
		directory.mkdir(parents=True, exist_ok=True)
		with open(directory / 'compile_commands.json', 'w', encoding='utf-8') as f:
			json.dump(items, f, indent=1)
		result += [(str(directory), item['file']) for item in sorted(items, key=lambda e: e['file'])]
	return result


def relative_name(path):
	"""The name of a finding's file in the baseline: relative to the repository, with slashes."""
	try:
		return Path(path).resolve().relative_to(ROOT).as_posix()
	except ValueError:
		return Path(path).as_posix()


BASELINE_HEADER = (
	'# The clang-tidy findings baseline of tools/tidy.py (issue #11): count, file, check, message; tab-separated.\n'
	'# Written in the Docker image of docker/linux-x64 (clang-tidy 22.1.8) after configuring linux-x64-clang:\n'
	'#\tpython3 -I tools/tidy.py -p build/linux-x64-clang --all --write-baseline tools/tidy-baseline.tsv\n'
	'# CI fails on a finding that is not here (notes/reconstruction.md, "Checking"). Write it again when findings\n'
	'# are fixed (it only shrinks), or when a new one is accepted on purpose (say why in the commit).\n')


def baseline_counts(findings):
	"""The findings counted by (file, check, message)."""
	return Counter((relative_name(key[0]), key[3], message) for key, (line, message) in findings.items())


def read_baseline(name):
	counts = Counter()
	with open(name, encoding='utf-8') as f:
		for line in f:
			line = line.rstrip('\r\n')
			if not line or line.startswith('#'):
				continue
			count, file, check, message = line.split('\t', 3)
			counts[(file, check, message)] += int(count)
	return counts


def write_baseline(name, counts):
	with open(name, 'w', encoding='utf-8', newline='\n') as f:
		f.write(BASELINE_HEADER)
		for (file, check, message), count in sorted(counts.items()):
			f.write(f'{count}\t{file}\t{check}\t{message}\n')


def compare_baseline(name, counts, findings):
	"""Lists the new and the gone findings; 1 when a (file, check, message) has more than the baseline."""
	baseline = read_baseline(name)
	new = {k: n - baseline[k] for k, n in counts.items() if n > baseline[k]}
	gone = {k: n - counts[k] for k, n in baseline.items() if n > counts[k]}
	for (file, check, message), n in sorted(new.items()):
		print(f'tidy.py: new: {n} x {file}: {message} [{check}]')
		for key, (line, text) in sorted(findings.items()):
			if relative_name(key[0]) == file and key[3] == check and text == message:
				print('tidy.py:     ' + line)
	for (file, check, message), n in sorted(gone.items()):
		print(f'tidy.py: gone: {n} x {file}: {message} [{check}]')
	print(f'tidy.py: baseline {name}: {sum(baseline.values())} findings, now {sum(counts.values())}: '
		f'{sum(new.values())} new, {sum(gone.values())} gone'
		+ (' (write the baseline again to drop them)' if gone else ''))
	return 1 if new else 0


def snapshot():
	return {p: p.read_bytes() for p in SRC.rglob('*') if p.suffix.lower() in ('.cpp', '.h', '.c')}


def main():
	parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
	parser.add_argument('-p', dest='build', default=str(ROOT / 'build' / 'windows-x64'))
	parser.add_argument('--fix', action='store_true')
	parser.add_argument('--checks')
	parser.add_argument('--extra-arg', action='append', default=[])
	parser.add_argument('-v', action='store_true')
	parser.add_argument('--all', action='store_true')
	parser.add_argument('--headers', action='store_true')
	baseline = parser.add_mutually_exclusive_group()
	baseline.add_argument('--baseline')
	baseline.add_argument('--write-baseline')
	parser.add_argument('--zero', action='append', default=[], metavar='PREFIX')
	parser.add_argument('files', nargs='*')
	args = parser.parse_args()
	if not (Path(args.build) / 'compile_commands.json').exists():
		sys.exit(f'tidy.py: {Path(args.build) / "compile_commands.json"} is missing (configure with CMAKE_EXPORT_COMPILE_COMMANDS)')
	# (database directory, file): the build's own database for the sources, the header checks' for headers.
	runs = [(args.build, f, False) for f in args.files]
	if args.all:
		runs += [(args.build, str(p), False) for p in compile_commands_files(args.build)]
	if args.headers:
		headers = header_commands(args.build)
		if not headers:
			sys.exit(f'tidy.py: {args.build} has no header checks (cmake/HeaderCheck.cmake)')
		runs += [(database, header, True) for database, header in headers]
	if not runs:
		parser.error('no files: name them, or --all, or --headers')
	if args.fix and (args.baseline or args.write_baseline):
		parser.error('--fix changes the files: not with a baseline')
	tidy = find_clang_tidy()
	# MSVC's command lines: clang does not use some of their options, and /WX would make that an error.
	command = ['--quiet', '--extra-arg=-Wno-unused-command-line-argument', '--extra-arg=-Wno-error']
	command += ['--extra-arg=' + a for a in args.extra_arg]
	if args.fix:
		command.append('--fix')
	findings = {}
	in_macros = {}
	failed = []
	reverted = 0
	for database, file, header in runs:
		before = snapshot() if args.fix else None
		checks = ['--checks=-*,misc-include-cleaner'] if header else ['--checks=' + args.checks] if args.checks else []
		run = subprocess.run([tidy, '-p', database] + checks + command + [str(Path(file).resolve())],
			capture_output=True, text=True, encoding='utf-8', errors='replace')
		if 'Found compiler error' in run.stdout + run.stderr:
			failed.append(file)
		if args.fix:
			for path, data in snapshot().items():
				if before.get(path) not in (None, data):
					old = before[path].decode('utf-8')
					text, undone = revert_in_macros(old, data.decode('utf-8'))
					path.write_bytes(text.encode('utf-8'))
					reverted += undone
		spans = {}
		for line in run.stdout.splitlines():
			m = DIAGNOSTIC.match(line)
			if not m or m.group(6) is None:
				continue
			path = Path(m.group(1)).resolve()
			key = (str(path), int(m.group(2)), int(m.group(3)), m.group(6))
			if path not in spans:
				data = (before or {}).get(path)
				text = data.decode('utf-8') if data is not None else path.read_text(encoding='utf-8')
				spans[path] = (text, protected_spans(text))
			text, file_spans = spans[path]
			pos = offset(text, key[1], key[2])
			(in_macros if any(a <= pos < b for a, b in file_spans) else findings)[key] = (line, m.group(5))
	for key in sorted(findings):
		print(findings[key][0])
	if args.v:
		for key in sorted(in_macros):
			print('in a macro argument: ' + in_macros[key][0])
	counts = Counter(k[3] for k in findings)
	for check, n in sorted(counts.items()):
		print(f'tidy.py: {n:6} {check}')
	print(f'tidy.py: {len(findings)} findings, {len(in_macros)} more inside TASSERT/TERROR/TEND arguments (left for M5)'
		+ (f', {reverted} fix edits inside them reverted' if args.fix else ''))
	if failed:
		print('tidy.py: clang-tidy could not compile: ' + ', '.join(failed))
		return 2
	zero = 0
	if args.zero:
		everything = {**findings, **in_macros}
		for key in sorted(everything):
			if key[3].startswith(tuple(args.zero)):
				print('tidy.py: must be zero: ' + everything[key][0])
				zero += 1
		print(f'tidy.py: {zero} findings of the checks that must stay at zero ({", ".join(args.zero)})')
	if args.write_baseline:
		written = baseline_counts(findings)
		write_baseline(args.write_baseline, written)
		print(f'tidy.py: wrote {args.write_baseline}: {sum(written.values())} findings in {len(written)} entries')
		return 1 if zero else 0
	if args.baseline:
		return max(compare_baseline(args.baseline, baseline_counts(findings), findings), 1 if zero else 0)
	return 1 if findings or zero else 0


if __name__ == '__main__':
	sys.exit(main())
