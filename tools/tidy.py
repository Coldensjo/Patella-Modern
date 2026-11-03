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

Usage:
	python -I tools/tidy.py [-p <build dir>] [--fix] [--checks <list>] [--extra-arg <arg>] [-v]
		[--baseline <file> | --write-baseline <file>] (--all | <file> ...)
	-p        the directory with compile_commands.json (CMAKE_EXPORT_COMPILE_COMMANDS; default
	          build/windows-x64, the preset of issue #5)
	--fix     apply the fixes outside the macro arguments
	--checks  passed to clang-tidy (added to .clang-tidy's)
	--extra-arg  passed to clang-tidy (repeatable), e.g. -m32 for an x86 build's commands: clang
	          takes the target from its own default, x64, not from the compiler of the build
	-v        also list the findings inside macro arguments
	--all     every source of compile_commands.json under src/ (not src/Extern), besides the files named
	--baseline        compare with this findings list: exit 1 only for findings that are not in it
	--write-baseline  write the findings to this list (the form --baseline reads)
clang-tidy: the environment variable CLANG_TIDY, else clang-tidy on PATH, else Visual Studio's
(VC\\Tools\\Llvm\\x64\\bin). Exit code 0 without findings outside the macro arguments (with --baseline:
without new ones), 1 with, 2 when clang-tidy could not compile a file.
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
	baseline = parser.add_mutually_exclusive_group()
	baseline.add_argument('--baseline')
	baseline.add_argument('--write-baseline')
	parser.add_argument('files', nargs='*')
	args = parser.parse_args()
	if not (Path(args.build) / 'compile_commands.json').exists():
		sys.exit(f'tidy.py: {Path(args.build) / "compile_commands.json"} is missing (configure with CMAKE_EXPORT_COMPILE_COMMANDS)')
	if args.all:
		args.files += [str(p) for p in compile_commands_files(args.build)]
	if not args.files:
		parser.error('no files: name them, or --all')
	if args.fix and (args.baseline or args.write_baseline):
		parser.error('--fix changes the files: not with a baseline')
	tidy = find_clang_tidy()
	# MSVC's command lines: clang does not use some of their options, and /WX would make that an error.
	command = [tidy, '-p', args.build, '--quiet', '--extra-arg=-Wno-unused-command-line-argument',
		'--extra-arg=-Wno-error']
	if args.checks:
		command.append('--checks=' + args.checks)
	command += ['--extra-arg=' + a for a in args.extra_arg]
	if args.fix:
		command.append('--fix')
	findings = {}
	in_macros = {}
	failed = []
	reverted = 0
	for file in args.files:
		before = snapshot() if args.fix else None
		run = subprocess.run(command + [str(Path(file).resolve())], capture_output=True, text=True,
			encoding='utf-8', errors='replace')
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
	if args.write_baseline:
		written = baseline_counts(findings)
		write_baseline(args.write_baseline, written)
		print(f'tidy.py: wrote {args.write_baseline}: {sum(written.values())} findings in {len(written)} entries')
		return 0
	if args.baseline:
		return compare_baseline(args.baseline, baseline_counts(findings), findings)
	return 1 if findings else 0


if __name__ == '__main__':
	sys.exit(main())
