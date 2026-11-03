r"""Configure, build and test a CMake preset in one command, from any shell (issue #10).

Usage:
  python -I tools/build.py <workflow preset> [cmake --workflow options, e.g. --fresh]
  python -I tools/build.py --env <preset> <command> [arguments ...]
  python -I tools/build.py --list

The first form runs `cmake --workflow --preset <workflow preset>` (CMakePresets.json: oracle-x86,
windows-x64, windows-x64-debug, windows-x64-gcc, windows-x64-gcc-debug): configure, build, test. The
second runs any command (cmake --build, ctest, ...) in the compiler environment of <preset> (a
configure, build, test or workflow preset), e.g.
  python -I tools/build.py --env oracle-x86 cmake --build --preset oracle-x86-debug
  python -I tools/build.py --env oracle-x86 ctest --preset oracle-x86 -R vlongtest

The MSVC presets mark their architecture and toolset `external`: CMake does not set up the compiler
for them. This script does what a developer prompt does: it runs Visual Studio's vcvarsall.bat with
the architecture of the preset (x86 for oracle-x86, x64 for windows-x64; found through vswhere, the
newest Visual Studio with the C++ tools) and runs CMake in the environment it sets. A shell that is
already a developer prompt for that architecture is used as it is. The other presets (mingw-w64 GCC,
later Linux) run CMake in the shell's own environment.

Warnings (issue #17): every cmake run (the first form, and the second when the command is cmake) is
checked for compiler, resource compiler, assembler and linker warnings (MSVC's "warning C4101",
"warning LNK4075", "warning RC4005", "warning A4018", "Command line warning D9025" ...; GCC's and Clang's
"file:line:col: warning:"). The output is passed through unchanged; the scan stops where the test step
begins, so the tests' output is not scanned. A warning makes the script exit 1 after the command ends
(when the command itself succeeded), with the warnings listed again. The product presets build with
/WX or -Werror already; this is what makes a warning fail oracle-x86, which keeps the former
Patella.vcxproj's /WX- until M6 (plan section 4.1: no warnings on any preset). Ninja recompiles only
what changed, so a build that is up to date shows no warnings: --fresh checks every file.

Exit code: the command's, or 1 when it succeeded but warned.
"""

import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS = os.path.join(ROOT, "CMakePresets.json")
VCVARSALL = r"VC\Auxiliary\Build\vcvarsall.bat"
# where the .cmd scripts found it before issue #10; vswhere's answer comes first
DEFAULT_VS = r"C:\Program Files\Microsoft Visual Studio\18\Community"


# A warning of a tool of the build: MSVC's cl, link, lib, rc, cvtres and ml print "warning <code>" with a
# letter prefix and four or five digits (cl's command line warnings "Command line warning D9025");
# GCC and Clang "<file>:<line>[:<column>]: warning:".
WARNING = re.compile(r"\bwarning\s+[A-Z]{1,3}\d{4,5}\b|:\d+(?::\d+)?: warning: ")
# cmake --workflow's line before the test step; ctest's output after it is not the build's.
TEST_STEP = re.compile(r"^Executing workflow step \d+ of \d+: test preset ")


class BuildError(Exception):
	pass


def load_presets():
	with open(PRESETS, "r", encoding="utf-8") as f:
		return json.load(f)


def configure_preset_of(presets, name):
	"""The configure preset behind `name`, a configure, build, test or workflow preset."""
	for kind in ("configurePresets", "buildPresets", "testPresets"):
		for preset in presets.get(kind, []):
			if preset["name"] == name:
				return name if kind == "configurePresets" else preset["configurePreset"]
	for preset in presets.get("workflowPresets", []):
		if preset["name"] == name:
			for step in preset["steps"]:
				if step["type"] == "configure":
					return step["name"]
	raise BuildError("no preset %r in CMakePresets.json" % name)


def resolved(presets, name):
	"""The configure preset `name` with what it inherits (the first parent wins, as in CMake)."""
	by_name = {p["name"]: p for p in presets.get("configurePresets", [])}
	if name not in by_name:
		raise BuildError("no configure preset %r in CMakePresets.json" % name)
	preset = by_name[name]
	result = {}
	parents = preset.get("inherits", [])
	if isinstance(parents, str):
		parents = [parents]
	for parent in reversed(parents):
		merged = resolved(presets, parent)
		cache = dict(result.get("cacheVariables", {}))
		cache.update(merged.get("cacheVariables", {}))
		result.update(merged)
		result["cacheVariables"] = cache
	cache = dict(result.get("cacheVariables", {}))
	cache.update(preset.get("cacheVariables", {}))
	result.update(preset)
	result["cacheVariables"] = cache
	return result


def value(field):
	if isinstance(field, dict):
		return field.get("value"), field.get("strategy", "set")
	return field, "set"


def vcvars_argument(preset):
	"""vcvarsall.bat's architecture argument for an MSVC preset with an external architecture, or
	None when the preset needs no developer environment."""
	compiler = preset.get("cacheVariables", {}).get("CMAKE_CXX_COMPILER", "")
	if isinstance(compiler, dict):
		compiler = compiler.get("value", "")
	if os.path.basename(str(compiler)).lower() not in ("cl", "cl.exe"):
		return None
	target, strategy = value(preset.get("architecture"))
	if not target or strategy != "external":
		return None
	toolset, _ = value(preset.get("toolset"))
	host = target
	for part in str(toolset or "").split(","):
		key, _, setting = part.partition("=")
		if key.strip() == "host" and setting.strip():
			host = setting.strip()
	target = target.lower()
	host = host.lower()
	return target if host == target else "%s_%s" % (host, target)


def find_vcvarsall():
	installer = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
		"Microsoft Visual Studio", "Installer")
	vswhere = os.path.join(installer, "vswhere.exe")
	candidates = []
	if os.path.isfile(vswhere):
		result = subprocess.run([vswhere, "-latest", "-products", "*", "-requires",
			"Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
			capture_output=True, text=True)
		candidates += [line.strip() for line in result.stdout.splitlines() if line.strip()]
	candidates.append(DEFAULT_VS)
	for folder in candidates:
		path = os.path.join(folder, VCVARSALL)
		if os.path.isfile(path):
			return path, installer
	raise BuildError("vcvarsall.bat not found (Visual Studio with the C++ workload; tried %s)"
		% ", ".join(candidates))


def developer_environment(argument):
	"""os.environ as vcvarsall.bat <argument> leaves it."""
	# a developer prompt for the same target and host already
	target = argument.split("_")[-1]
	host = argument.split("_")[0]
	if (os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == target
			and os.environ.get("VSCMD_ARG_HOST_ARCH", "").lower() == host):
		return dict(os.environ)
	vcvarsall, installer = find_vcvarsall()
	env = dict(os.environ)
	# vcvarsall.bat finds vswhere.exe on PATH
	env["PATH"] = env.get("PATH", "") + os.pathsep + installer
	comspec = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "cmd.exe")
	dump = 'import json, os; print(json.dumps(dict(os.environ)))'
	command = '"%s" /d /s /c ""%s" %s >nul 2>&1 && "%s" -I -c "%s""' % (comspec, vcvarsall, argument,
		sys.executable, dump)
	result = subprocess.run(command, env=env, capture_output=True, text=True, encoding="utf-8")
	if result.returncode != 0 or not result.stdout.strip():
		raise BuildError("vcvarsall.bat %s failed (exit %d): %s" % (argument, result.returncode,
			result.stderr.strip()))
	env = json.loads(result.stdout.strip().splitlines()[-1])
	if not shutil.which("cl", path=env.get("PATH") or env.get("Path")):
		raise BuildError("cl.exe is not on PATH after vcvarsall.bat %s" % argument)
	return env


def environment(name):
	"""The environment to run CMake in for the preset `name`."""
	presets = load_presets()
	preset = resolved(presets, configure_preset_of(presets, name))
	argument = vcvars_argument(preset)
	if argument is None:
		return dict(os.environ)
	if os.name != "nt":
		raise BuildError("preset %s needs MSVC (Windows)" % name)
	return developer_environment(argument)


def run(command, env, check_warnings=False):
	executable = shutil.which(command[0], path=env.get("PATH") or env.get("Path"))
	if executable is None:
		raise BuildError("%s not found on PATH" % command[0])
	print("build.py: %s" % subprocess.list2cmdline(command), flush=True)
	if not check_warnings:
		return subprocess.call([executable] + command[1:], env=env, cwd=ROOT)
	return run_checked([executable] + command[1:], env)


def run_checked(command, env):
	"""Runs the command with its output passed through byte for byte and scanned for warnings (up to
	the test step); exit 1 when the command succeeded but warned."""
	sys.stdout.flush()
	out = sys.stdout.buffer
	warnings = []
	scanning = True
	process = subprocess.Popen(command, env=env, cwd=ROOT, stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT)
	try:
		for raw in process.stdout:
			out.write(raw)
			out.flush()
			if not scanning:
				continue
			# the tools write the console's code page; ASCII is all the patterns need
			line = raw.decode("latin-1").rstrip("\r\n")
			if TEST_STEP.match(line):
				scanning = False
			elif WARNING.search(line):
				warnings.append(line)
	finally:
		process.stdout.close()
		code = process.wait()
	if warnings:
		print("build.py: %d warning%s in the build (any compiler or linker warning fails the build; "
			"issue #17):" % (len(warnings), "" if len(warnings) == 1 else "s"), file=sys.stderr)
		for line in warnings:
			print("build.py:   %s" % line, file=sys.stderr)
		sys.stderr.flush()
		if code == 0:
			return 1
	return code


def main(argv):
	if not argv or argv[0] in ("-h", "--help"):
		print(__doc__)
		return 0 if argv else 2
	if argv[0] == "--list":
		for preset in load_presets().get("workflowPresets", []):
			print("%-24s %s" % (preset["name"], preset.get("displayName", "")))
		return 0
	if argv[0] == "--env":
		if len(argv) < 3:
			print("build.py: --env <preset> <command> [arguments ...]", file=sys.stderr)
			return 2
		command = argv[2:]
		is_cmake = os.path.splitext(os.path.basename(command[0]))[0].lower() == "cmake"
		return run(command, environment(argv[1]), check_warnings=is_cmake)
	name = argv[0]
	if name not in [p["name"] for p in load_presets().get("workflowPresets", [])]:
		raise BuildError("no workflow preset %r (python -I tools/build.py --list)" % name)
	return run(["cmake", "--workflow", "--preset", name] + argv[1:], environment(name), check_warnings=True)


if __name__ == "__main__":
	try:
		sys.exit(main(sys.argv[1:]))
	except BuildError as e:
		print("build.py: %s" % e, file=sys.stderr)
		sys.exit(1)
