r"""The grep gate of issue #57 (notes/modernization-plan.md, section 7, M4's exit): no call of strcpy, strcat,
sprintf, vsprintf or gets in src/ (src/Extern, Henry Spencer's regexp, keeps its own code). The client's text
buffers write through TFixedString and TStringBuffer (src/types/FixedString.h) instead; a bounded MsvcSnprintf
(_snprintf's semantics, src/compat/msvc.h) and strncpy are not banned.

Comments and string and character literals are skipped, so a comment may name the function it replaced. Each
call found is printed as <file>:<line>: <function>; the exit code is 1 when there is one, 0 otherwise.

Usage:
	python -I tools/banned_calls.py [--root <checkout>] [--self-test]
The hosted CI runs it (.github/workflows/hosted.yml, job "banned C string calls").
"""

import argparse
import pathlib
import re
import sys

BANNED = ('strcpy', 'strcat', 'sprintf', 'vsprintf', 'gets')
CALL = re.compile(r'\b(' + '|'.join(BANNED) + r')\s*\(')
EXTENSIONS = {'.c', '.cpp', '.cc', '.h', '.hpp', '.inl', '.inc'}


def strip(text):
	"""The text with its comments and literals blanked out (their line breaks kept, so lines still count)."""
	out = []
	i = 0
	n = len(text)
	while i < n:
		c = text[i]
		if text.startswith('//', i):
			j = text.find('\n', i)
			j = n if j < 0 else j
			i = j
		elif text.startswith('/*', i):
			j = text.find('*/', i + 2)
			j = n if j < 0 else j + 2
			out.append('\n' * text.count('\n', i, j))
			i = j
		elif c == '"' or c == "'":
			# A raw string literal R"delimiter( ... )delimiter".
			raw = c == '"' and i > 0 and text[i - 1] == 'R' and (i < 2 or not (text[i - 2].isalnum() or text[i - 2] == '_'))
			if raw:
				open_paren = text.find('(', i)
				delimiter = text[i + 1:open_paren]
				j = text.find(')' + delimiter + '"', open_paren)
				j = n if j < 0 else j + len(delimiter) + 2
			else:
				j = i + 1
				while j < n and text[j] != c and text[j] != '\n':
					j += 2 if text[j] == '\\' else 1
				j = min(j + 1, n)
			out.append(' ' + '\n' * text.count('\n', i, j))
			i = j
		else:
			out.append(c)
			i += 1
	return ''.join(out)


def calls(text):
	"""(line, function) of every banned call in a source text."""
	code = strip(text)
	return [(code.count('\n', 0, m.start()) + 1, m.group(1)) for m in CALL.finditer(code)]


def self_test():
	assert calls('strcpy(a, b);\n') == [(1, 'strcpy')]
	assert calls('x;\n  sprintf (Buffer, "%d", 1);') == [(2, 'sprintf')]
	assert calls('// strcpy(a, b) replaced\n/* strcat(a,\n b) */ int x;\n') == []
	assert calls('const char *T = "sprintf(";\nchar c = \'(\'; gets(Line);') == [(2, 'gets')]
	assert calls('MsvcSnprintf(a, 4, "x"); strncpy(a, b, 3); snprintf(a, 3, "y"); fgets(a, 3, f);') == []
	assert calls('auto s = R"x(strcpy(a, b))x"; strcat(a, b);') == [(1, 'strcat')]
	assert calls('Text.Assign(Name); vsprintf(a, f, v);') == [(1, 'vsprintf')]
	assert calls('/* unterminated') == []
	print('banned_calls.py: self-test passed')


def main():
	parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
	parser.add_argument('--root', default=str(pathlib.Path(__file__).resolve().parent.parent))
	parser.add_argument('--self-test', action='store_true')
	arguments = parser.parse_args()
	if arguments.self_test:
		self_test()
		return 0
	source = pathlib.Path(arguments.root) / 'src'
	found = 0
	files = 0
	for path in sorted(source.rglob('*')):
		relative = path.relative_to(source)
		if path.suffix not in EXTENSIONS or not path.is_file() or relative.parts[0] == 'Extern':
			continue
		files += 1
		for line, function in calls(path.read_text(encoding='utf-8', errors='replace')):
			print(f'src/{relative.as_posix()}:{line}: {function}')
			found += 1
	print(f'banned_calls.py: {found} calls of {", ".join(BANNED)} in {files} files of src/ (src/Extern not checked)')
	return 1 if found else 0


if __name__ == '__main__':
	sys.exit(main())
