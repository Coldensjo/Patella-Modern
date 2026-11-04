r"""The hosted CI's clang-tidy job, run locally: the same findings on the same commit (issue #51).

The job "clang-tidy (no new findings)" of .github/workflows/hosted.yml builds the image of docker/linux-x64
from the checkout's Dockerfile, mounts a fresh checkout of the commit at /work and runs its script there
(clang-tidy --version, cmake --preset linux-x64-clang, tools/tidy.py --all --baseline on linux-x64-clang's
compile commands). This script does the same on this machine (Docker Desktop):
- the image is built from the Dockerfile of the checked revision (docker build of `git archive <rev>
  docker/linux-x64`; Docker's cache makes it a no-op when nothing changed; untagged, so build.py's image
  keeps its tag);
- the files are the revision's (`git archive <rev>`), streamed into the container's own case-sensitive
  file system at /work: no build tree of an earlier run, no untracked or ignored file, no CipSoft file;
- the script is the job's own, read from that revision's hosted.yml (the single-quoted `bash -c` argument
  of the clang-tidy job's tidy.py step), so a change of the job is a change of this check.
With --worktree it checks the working tree instead of a commit (the files git knows: tracked, and untracked
ones that are not ignored, as build.py copies them), for a check before committing; the image and the
script are then the working tree's.

Usage:
	python -I tools/tidy_ci.py [--rev <rev> | --worktree] [--log <file>] [--hosted <run id> [--repo <owner/name>]]
	--rev       the commit to check (default HEAD)
	--worktree  the working tree instead of a commit
	--log       write the full output there too (default build/logs/tidy-ci/<rev>.log)
	--hosted    compare with the clang-tidy job of that hosted workflow run (gh run view --log): its
	            clang-tidy version line, finding lines and tidy.py summary lines against this run's, in order
	--repo      the repository of --hosted (default: the remote origin's, owner/name)
The comparison can also run alone on two saved outputs:
	python -I tools/tidy_ci.py --compare <local log> <hosted log>

Exit code: the job script's (0 no new findings, 1 new findings, 2 clang-tidy could not compile a file), or
3 when --hosted or --compare finds a difference.
Which check a change must pass: notes/reconstruction.md, "Checking".
"""
import argparse
import difflib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = '.github/workflows/hosted.yml'
JOB_NAME = 'clang-tidy (no new findings)'
DOCKER_DIR = 'docker/linux-x64'
# A finding as tidy.py prints it (clang-tidy's line), a tidy.py summary line, clang-tidy's version line.
FINDING = re.compile(r'^/work/\S+:\d+:\d+: (?:warning|error): .*\[[^\]]+\]$')
SUMMARY = re.compile(r'^tidy\.py: ')
VERSION = re.compile(r'^\s*(?:\S+ )?LLVM version \S+$')
# The prefix of a line of `gh run view --log`: job, step, time stamp; of a job log of the API: the time stamp.
GH_RUN_LINE = re.compile(r'^([^\t]*)\t[^\t]*\t\d{4}-\d\d-\d\dT[\d:.]+Z ?')
STAMP = re.compile(r'^\d{4}-\d\d-\d\dT[\d:.]+Z ?')


def git(*args, binary=False):
	# core.autocrlf off: the files as a Linux checkout has them (.gitattributes asks for LF anyway).
	run = subprocess.run(['git', '-c', 'core.autocrlf=false', '-C', str(ROOT)] + list(args), capture_output=True,
		check=True)
	return run.stdout if binary else run.stdout.decode('utf-8')


def job_script(workflow_text):
	"""The single-quoted bash -c script of the clang-tidy job's tools/tidy.py step."""
	job = re.search(r'^  clang-tidy:\n(.*?)(?=^  \S|\Z)', workflow_text, re.M | re.S)
	if not job:
		sys.exit(f'tidy_ci.py: no job clang-tidy in {WORKFLOW}')
	match = re.search(r"bash -c\s+'([^']*tools/tidy\.py[^']*)'", job.group(1))
	if not match:
		sys.exit(f'tidy_ci.py: no `bash -c \'... tools/tidy.py ...\'` in the clang-tidy job of {WORKFLOW}')
	return ' '.join(match.group(1).split())


def worktree_tree():
	"""A tree of the files git knows in the working tree (build.py's set), for git archive."""
	names = git('ls-files', '-z', '--cached', '--others', '--exclude-standard').split('\0')
	names = [n for n in names if n and (ROOT / n).is_file()]
	# Stage them in a temporary index and write that tree: tracked and new files with their content now.
	index = ROOT / 'build' / 'tidy-ci-index'
	index.parent.mkdir(exist_ok=True)
	index.unlink(missing_ok=True)
	env = dict(os.environ, GIT_INDEX_FILE=str(index))
	subprocess.run(['git', '-C', str(ROOT), 'add', '--pathspec-from-file=-', '--pathspec-file-nul'],
		input='\0'.join(names).encode('utf-8'), env=env, check=True)
	tree = subprocess.run(['git', '-C', str(ROOT), 'write-tree'], env=env, capture_output=True,
		check=True).stdout.decode().strip()
	index.unlink(missing_ok=True)
	return tree


def build_image(treeish):
	"""docker build of treeish's docker/linux-x64; the image id."""
	context = git('archive', '--format=tar', treeish, DOCKER_DIR, binary=True)
	# The context is the archive's root: the Dockerfile sits at docker/linux-x64/Dockerfile in it.
	run = subprocess.run(['docker', 'build', '-q', '-f', DOCKER_DIR + '/Dockerfile', '-'], input=context,
		capture_output=True)
	if run.returncode != 0:
		sys.stdout.write(run.stdout.decode('utf-8', 'replace') + run.stderr.decode('utf-8', 'replace'))
		sys.exit('tidy_ci.py: docker build failed')
	return run.stdout.decode().strip()


def relevant(lines):
	"""The lines both runs must agree on: clang-tidy's version, the findings, tidy.py's summary."""
	return [l for l in lines if VERSION.match(l) or FINDING.match(l) or SUMMARY.match(l)]


def hosted_lines(text):
	"""The output of the clang-tidy job, from a job log of the GitHub API (time stamp, text), from `gh run view
	--log` of a whole run (job, step, time stamp, text; the other jobs' lines are left out) or from a saved
	output."""
	lines = []
	for line in text.lstrip('﻿').splitlines():
		run_line = GH_RUN_LINE.match(line)
		if run_line:
			if run_line.group(1) != JOB_NAME:
				continue
			line = line[run_line.end():]
		else:
			line = STAMP.sub('', line, count=1)
		lines.append(line)
	return lines


def origin_repository():
	"""owner/name of the remote origin on GitHub (gh would take another remote's, e.g. the upstream's)."""
	url = git('remote', 'get-url', 'origin').strip()
	match = re.search(r'github\.com[:/]([^/]+/[^/]+?)(?:\.git)?/?$', url)
	if not match:
		sys.exit(f'tidy_ci.py: origin ({url}) is not a GitHub repository; name it with --repo')
	return match.group(1)


def hosted_log(run_id, repository):
	"""The log of the clang-tidy job of a hosted run (the job may be done before the run), and the run's commit."""
	view = subprocess.run(['gh', 'run', 'view', run_id, '--repo', repository, '--json', 'headSha,jobs'], cwd=ROOT,
		capture_output=True)
	if view.returncode != 0:
		sys.stdout.write(view.stderr.decode('utf-8', 'replace'))
		sys.exit('tidy_ci.py: gh run view failed')
	data = json.loads(view.stdout)
	jobs = [j for j in data['jobs'] if j['name'] == JOB_NAME]
	if not jobs or jobs[0]['status'] != 'completed':
		sys.exit(f'tidy_ci.py: run {run_id} has no completed job "{JOB_NAME}"')
	# The job's log through the API: gh run view --log waits for the whole run.
	log = subprocess.run(['gh', 'api', f'repos/{repository}/actions/jobs/{jobs[0]["databaseId"]}/logs'], cwd=ROOT,
		capture_output=True)
	if log.returncode != 0:
		sys.stdout.write(log.stderr.decode('utf-8', 'replace'))
		sys.exit('tidy_ci.py: gh api .../logs failed')
	return log.stdout.decode('utf-8', 'replace'), data['headSha']


def compare(local, hosted):
	a, b = relevant(local), relevant(hosted)
	if not b:
		print('tidy_ci.py: the hosted log has no clang-tidy output')
		return 3
	if a == b:
		findings = sum(1 for l in a if FINDING.match(l))
		print(f'tidy_ci.py: identical to the hosted job: {len(a)} lines ({findings} finding lines, the version and '
			'the tidy.py summary)')
		return 0
	print('tidy_ci.py: differs from the hosted job (- local, + hosted):')
	for line in difflib.unified_diff(a, b, 'local', 'hosted', lineterm='', n=0):
		print(line)
	return 3


def main():
	parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
	which = parser.add_mutually_exclusive_group()
	which.add_argument('--rev', default='HEAD')
	which.add_argument('--worktree', action='store_true')
	which.add_argument('--compare', nargs=2, metavar=('LOCAL', 'HOSTED'))
	parser.add_argument('--log')
	parser.add_argument('--hosted')
	parser.add_argument('--repo')
	args = parser.parse_args()
	if args.compare:
		return compare(Path(args.compare[0]).read_text(encoding='utf-8', errors='replace').splitlines(),
			hosted_lines(Path(args.compare[1]).read_text(encoding='utf-8', errors='replace')))

	if args.worktree:
		treeish = worktree_tree()
		label = 'worktree'
		described = 'the working tree (files git knows)'
	else:
		commit = git('rev-parse', '--verify', args.rev + '^{commit}').strip()
		treeish = commit
		label = commit[:7]
		described = f'commit {commit}'
	script = job_script(git('show', f'{treeish}:{WORKFLOW}'))
	log = Path(args.log) if args.log else ROOT / 'build' / 'logs' / 'tidy-ci' / f'{label}.log'
	log.parent.mkdir(parents=True, exist_ok=True)

	print(f'tidy_ci.py: {described}')
	image = build_image(treeish)
	print(f'tidy_ci.py: image {image} (docker/linux-x64 of that tree)')
	print(f'tidy_ci.py: the job\'s script: {script}')
	files = git('archive', '--format=tar', treeish, binary=True)
	command = ['docker', 'run', '--rm', '-i', image, 'bash', '-c',
		'set -e; mkdir -p /work; tar -x -C /work; cd /work; exec bash -c "$0"', script]
	run = subprocess.run(command, input=files, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = run.stdout.decode('utf-8', 'replace')
	sys.stdout.write(output)
	with open(log, 'w', encoding='utf-8', newline='\n') as f:
		f.write(output)
	print(f'tidy_ci.py: exit code {run.returncode}; output in {log}')
	result = run.returncode

	if args.hosted:
		hosted_text, head = hosted_log(args.hosted, args.repo or origin_repository())
		if not args.worktree and head != treeish:
			print(f'tidy_ci.py: note: run {args.hosted} is of commit {head}, not {treeish}')
		with open(log.with_suffix('.hosted.log'), 'w', encoding='utf-8', newline='\n') as f:
			f.write('\n'.join(hosted_lines(hosted_text)) + '\n')
		if compare(output.splitlines(), hosted_lines(hosted_text)):
			result = 3
	return result


if __name__ == '__main__':
	sys.exit(main())
