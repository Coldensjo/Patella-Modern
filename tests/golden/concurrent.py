"""Two golden_runner processes at once over the same corpus (issue #66): the CTest test golden_concurrent.

Usage: python -I concurrent.py <golden_runner.exe> [--require] <corpus directory>

Each runner has a temporary folder of its own (runner.cpp, TRunFolder: %TEMP%\\golden-run-<process id>-<n>);
before issue #66 they shared fixed folders below %TEMP% and two at once failed (748 failures in ConsoleWindow's
SaveChannel). Both start together, with their output in golden-concurrent-1.log and -2.log next to the runner; the
test passes when both exit with 0 and neither run's folder is left below %TEMP% afterwards. Without a corpus both
say so with 77, and so does this script (CTest: skipped).
"""

import pathlib
import subprocess
import sys
import tempfile


def main(argv):
	if len(argv) < 2:
		print('usage: concurrent.py <golden_runner.exe> [--require] <corpus directory>')
		return 2
	runner = pathlib.Path(argv[0])
	arguments = argv[1:]
	temp = pathlib.Path(tempfile.gettempdir())
	logs = [runner.parent / f'golden-concurrent-{n}.log' for n in (1, 2)]
	processes = []
	for log in logs:
		with open(log, 'wb') as output:
			processes.append(subprocess.Popen([str(runner), *arguments], stdout=output, stderr=subprocess.STDOUT))
	codes = [process.wait() for process in processes]
	ok = True
	for n, (process, code, log) in enumerate(zip(processes, codes, logs), 1):
		lines = log.read_text(encoding='utf-8', errors='replace').splitlines()
		summary = next((line for line in reversed(lines) if line.startswith('golden:')), '(no summary)')
		left = sorted(path.name for path in temp.glob(f'golden-run-{process.pid}-*'))
		print(f'runner {n} (process {process.pid}): exit code {code}; {summary}; log {log}')
		if left:
			print(f'runner {n}: left below {temp}: {", ".join(left)}')
			ok = False
	if codes == [77, 77]:
		return 77
	if codes != [0, 0]:
		ok = False
	print('golden_concurrent: ' + ('passed' if ok else 'FAILED'))
	return 0 if ok else 1


if __name__ == '__main__':
	sys.exit(main(sys.argv[1:]))
