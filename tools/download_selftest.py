r"""Shows the build's retrying downloads against a local server (issue #153; notes/decisions.md, "#153").

Usage:
  python -I tools/download_selftest.py

The three download paths of the builds, none of them touching the network: tools/build.py's download_verified (the
Windows CMake), docker/linux-x64/fetch.sh (the image's CMake and Ninja) and cmake/Download.cmake (the libraries'
archives). The server on 127.0.0.1 answers each path from a script: 504 then the file, a 404, a wrong file, a transfer
cut short, a dropped connection. For each path it checks what is retried and what is not, that the pauses are kept, that
the fallback is used and checked, and above all that no file with a wrong SHA-256 is ever left behind as the result,
whether it arrives first, on a retry or from the fallback. cmake/Download.cmake is also run through
patella_fetch_declare in a small project: an extracted archive, a DOWNLOAD_NO_EXTRACT file, an installed package, a
second configure that needs no network.

Needs sh, curl and sha256sum (Git's on Windows) and CMake: the pinned one on Windows, the first on PATH elsewhere.
Exit code 0 when every check passed.
"""

import contextlib
import hashlib
import http.server
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FETCH_SH = os.path.join(ROOT, "docker", "linux-x64", "fetch.sh")
DOWNLOAD_CMAKE = os.path.join(ROOT, "cmake", "Download.cmake")

GOOD = bytes(range(256)) * 200
BAD = b"this is not the pinned file" * 100
SHA256 = hashlib.sha256(GOOD).hexdigest()


def load_build():
	spec = importlib.util.spec_from_file_location("patella_build", os.path.join(ROOT, "tools", "build.py"))
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


build = load_build()


class Server(http.server.ThreadingHTTPServer):
	daemon_threads = True

	def __init__(self):
		super().__init__(("127.0.0.1", 0), Handler)
		self.scripts = {}
		self.requests = []
		self.lock = threading.Lock()

	def script(self, scripts):
		"""`scripts`: path -> the answers of its requests in order: an int (the status), bytes (the file), ("short",
		bytes) (half of them, the full length announced), "drop" (the connection closed without an answer)."""
		with self.lock:
			self.scripts = {path: list(answers) for path, answers in scripts.items()}
			self.requests = []

	def next_answer(self, path):
		with self.lock:
			self.requests.append((path, time.monotonic()))
			answers = self.scripts.get(path)
			# 410 is not retried by any path: a request the scenario did not expect ends it visibly.
			return answers.pop(0) if answers else 410

	def count(self, path):
		with self.lock:
			return sum(1 for name, _ in self.requests if name == path)

	def times(self, path):
		with self.lock:
			return [moment for name, moment in self.requests if name == path]

	@property
	def base(self):
		return "http://127.0.0.1:%d" % self.server_address[1]


class Handler(http.server.BaseHTTPRequestHandler):
	protocol_version = "HTTP/1.1"

	def log_message(self, *args):
		pass

	def do_GET(self):
		answer = self.server.next_answer(self.path)
		self.close_connection = True
		if answer == "drop":
			return
		if isinstance(answer, int):
			self.send_response(answer)
			self.send_header("Content-Length", "0")
			self.send_header("Connection", "close")
			self.end_headers()
			return
		short = isinstance(answer, tuple)
		body = answer[1] if short else answer
		self.send_response(200)
		self.send_header("Content-Length", str(len(body)))
		self.send_header("Connection", "close")
		self.end_headers()
		self.wfile.write(body[:len(body) // 2] if short else body)
		self.wfile.flush()


def environment(tries, first_tries, pause):
	env = dict(os.environ)
	env["PATELLA_DOWNLOAD_TRIES"] = str(tries)
	env["PATELLA_DOWNLOAD_FIRST_TRIES"] = str(first_tries)
	env["PATELLA_DOWNLOAD_PAUSE"] = str(pause)
	return env


def find_sh():
	if os.name == "nt":
		for folder in (os.environ.get("ProgramFiles", r"C:\Program Files"), r"C:\Program Files"):
			candidate = os.path.join(folder, "Git", "bin", "sh.exe")
			if os.path.isfile(candidate):
				return candidate
	found = shutil.which("sh")
	if found is None:
		raise SystemExit("download_selftest.py: no sh found (Git's on Windows)")
	return found


def run_python(urls, sha256, output, tries, first_tries, pause):
	log = io.StringIO()
	try:
		with contextlib.redirect_stdout(log):
			build.download_verified(urls, sha256, output, tries=tries, first_tries=first_tries, pause=pause)
	except build.BuildError as e:
		return False, log.getvalue() + str(e)
	return True, log.getvalue()


def run_shell(urls, sha256, output, tries, first_tries, pause):
	result = subprocess.run([find_sh(), FETCH_SH.replace("\\", "/"), sha256, output.replace("\\", "/")] + urls,
		env=environment(tries, first_tries, pause), capture_output=True, text=True, encoding="utf-8", errors="replace")
	return result.returncode == 0, result.stdout + result.stderr


def cmake_executable():
	return build.pinned_cmake(["cmake"])[0]


def run_cmake(urls, sha256, output, tries, first_tries, pause):
	"""cmake/Download.cmake's patella_download has no fallback: only the first URL."""
	folder = os.path.dirname(output)
	script = os.path.join(folder, "run.cmake")
	with open(script, "w", encoding="utf-8") as f:
		f.write('include("%s")\npatella_download("%s" "%s" "%s")\n' % (DOWNLOAD_CMAKE.replace("\\", "/"), urls[0], sha256,
			output.replace("\\", "/")))
	result = subprocess.run([cmake_executable(), "-P", script], env=environment(tries, first_tries, pause),
		capture_output=True, text=True, encoding="utf-8", errors="replace")
	return result.returncode == 0, result.stdout + result.stderr


PATHS = [("build.py", run_python, True), ("fetch.sh", run_shell, True), ("Download.cmake", run_cmake, False)]


class Checks:
	def __init__(self):
		self.failed = 0
		self.passed = 0

	def check(self, name, condition, detail=""):
		if condition:
			self.passed += 1
			print("  ok    %s" % name, flush=True)
		else:
			self.failed += 1
			print("  FAIL  %s\n%s" % (name, detail), flush=True)


def scenarios():
	"""(name, the first URL's answers, the fallback's or None, succeeds, requests of the first, of the fallback).
	Run with tries 3, first_tries 2 when there is a fallback, pause 0."""
	return [
		("504, 504, then the file", [504, 504, GOOD], None, True, 3, 0),
		("500, then 503, then the file", [500, 503, GOOD], None, True, 3, 0),
		("408, then 429, then the file", [408, 429, GOOD], None, True, 3, 0),
		("a dropped connection, then the file", ["drop", GOOD], None, True, 2, 0),
		("a transfer cut short, then the file", [("short", GOOD), GOOD], None, True, 2, 0),
		("504 for ever: gives up", [504, 504, 504, GOOD], None, False, 3, 0),
		("404: no retry", [404, GOOD], None, False, 1, 0),
		("504 then 404: stops at the 404", [504, 404, GOOD], None, False, 2, 0),
		("403: no retry", [403, GOOD], None, False, 1, 0),
		("a wrong file: no retry, nothing accepted", [BAD, GOOD], None, False, 1, 0),
		("504, then a wrong file: the retry accepts nothing", [504, BAD, GOOD], None, False, 2, 0),
		("a cut transfer, then a wrong file: nothing accepted", [("short", GOOD), BAD, GOOD], None, False, 2, 0),
		("fallback after the first URL keeps answering 504", [504, 504, GOOD], [GOOD], True, 2, 1),
		("fallback answers 504 once, then the file", [504, 504], [504, GOOD], True, 2, 2),
		("fallback serves a wrong file: nothing accepted", [504, 504], [BAD, GOOD], False, 2, 1),
		("first URL serves a wrong file: the fallback is not tried", [BAD], [GOOD], False, 1, 0),
		("404 on the first URL: the fallback is not tried", [404], [GOOD], False, 1, 0),
		("both keep failing: gives up", [504, 504], [504, 504, 504], False, 2, 3),
	]


def assert_result(checks, label, name, ok, expected, output, log):
	part = output + ".part"
	checks.check("%s: %s: %s" % (label, name, "succeeds" if expected else "fails"), ok == expected, log)
	if expected:
		content = open(output, "rb").read() if os.path.exists(output) else b""
		checks.check("%s: %s: the file has the pinned SHA-256" % (label, name), content == GOOD, log)
	else:
		checks.check("%s: %s: no file left as the result" % (label, name), not os.path.exists(output), log)
	checks.check("%s: %s: no partial file left" % (label, name), not os.path.exists(part), log)


def run_scenarios(checks, server, scratch):
	for label, run, has_fallback in PATHS:
		print("%s:" % label, flush=True)
		for index, (name, first, fallback, expected, first_count, fallback_count) in enumerate(scenarios()):
			if fallback is not None and not has_fallback:
				continue
			folder = os.path.join(scratch, "%s-%d" % (label, index))
			os.makedirs(folder)
			output = os.path.join(folder, "file.bin")
			scripts = {"/first": first}
			urls = [server.base + "/first"]
			if fallback is not None:
				scripts["/fallback"] = fallback
				urls.append(server.base + "/fallback")
			server.script(scripts)
			ok, log = run(urls, SHA256, output, 3, 2, 0)
			assert_result(checks, label, name, ok, expected, output, log)
			checks.check("%s: %s: %d request(s) to the first URL" % (label, name, first_count),
				server.count("/first") == first_count, "%d requests\n%s" % (server.count("/first"), log))
			if fallback is not None:
				checks.check("%s: %s: %d request(s) to the fallback" % (label, name, fallback_count),
					server.count("/fallback") == fallback_count, "%d requests\n%s" % (server.count("/fallback"), log))


def run_pauses(checks, server, scratch):
	"""The pause grows with the attempt: 1 s, then 2 s, at a pause of 1 s."""
	for label, run, _ in PATHS:
		folder = os.path.join(scratch, "pause-%s" % label)
		os.makedirs(folder)
		output = os.path.join(folder, "file.bin")
		server.script({"/first": [504, 504, GOOD]})
		ok, log = run([server.base + "/first"], SHA256, output, 3, 2, 1)
		times = server.times("/first")
		gaps = [later - earlier for earlier, later in zip(times, times[1:])]
		checks.check("%s: pauses of at least 1 s and 2 s between attempts (%s)" % (
			label, ", ".join("%.2f s" % gap for gap in gaps)),
			ok and len(gaps) == 2 and gaps[0] >= 0.95 and gaps[1] >= 1.95, log)


def write_tar(path, member, content):
	with tarfile.open(path, "w:gz") as tar:
		info = tarfile.TarInfo(member)
		info.size = len(content)
		tar.addfile(info, io.BytesIO(content))


def run_project(checks, server, scratch):
	"""patella_fetch_declare as the build uses it."""
	print("patella_fetch_declare:", flush=True)
	folder = os.path.join(scratch, "project")
	source = os.path.join(folder, "source")
	binary = os.path.join(folder, "binary")
	package = os.path.join(folder, "package")
	os.makedirs(source)
	os.makedirs(package)
	archive = os.path.join(folder, "thing.tar.gz")
	write_tar(archive, "thing-1.0/hello.txt", b"hello\n")
	with open(archive, "rb") as f:
		archive_bytes = f.read()
	raw = b"raw file" * 64
	with open(os.path.join(package, "fakepkgConfig.cmake"), "w", encoding="utf-8") as f:
		f.write("set(fakepkg_FOUND TRUE)\n")
	with open(os.path.join(source, "CMakeLists.txt"), "w", encoding="utf-8") as f:
		f.write('cmake_minimum_required(VERSION 3.30)\nproject(selftest LANGUAGES NONE)\ninclude("%s")\n' %
			DOWNLOAD_CMAKE.replace("\\", "/"))
		f.write('patella_fetch_declare(thing URL %s/thing-1.0.tar.gz URL_HASH SHA256=%s)\n' % (
			server.base, hashlib.sha256(archive_bytes).hexdigest()))
		f.write('patella_fetch_declare(raw URL %s/raw.7z URL_HASH SHA256=%s DOWNLOAD_NO_EXTRACT TRUE)\n' % (
			server.base, hashlib.sha256(raw).hexdigest()))
		f.write('patella_fetch_declare(fakepkg URL %s/never.tar.gz URL_HASH SHA256=%s FIND_PACKAGE_ARGS CONFIG)\n' % (
			server.base, "0" * 64))
		f.write('FetchContent_MakeAvailable(thing raw fakepkg)\n')
		f.write('file(WRITE "${CMAKE_BINARY_DIR}/result.txt" "${thing_SOURCE_DIR}\\n${raw_SOURCE_DIR}\\n")\n')
	command = [cmake_executable(), "-S", source, "-B", binary, "-Dfakepkg_DIR=" + package.replace("\\", "/")]
	if shutil.which("ninja"):
		command += ["-G", "Ninja"]
	env = environment(3, 2, 0)
	server.script({"/thing-1.0.tar.gz": [504, archive_bytes], "/raw.7z": [503, 502, raw]})
	result = subprocess.run(command, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")
	log = result.stdout + result.stderr
	checks.check("the project configures after 504s", result.returncode == 0, log)
	checks.check("the archive took two requests", server.count("/thing-1.0.tar.gz") == 2, log)
	checks.check("the file without extraction took three", server.count("/raw.7z") == 3, log)
	checks.check("an installed package is not downloaded", server.count("/never.tar.gz") == 0, log)
	thing_dir, raw_dir = "", ""
	if result.returncode == 0:
		with open(os.path.join(binary, "result.txt"), encoding="utf-8") as f:
			thing_dir, raw_dir = f.read().splitlines()[:2]
	checks.check("the archive is extracted", os.path.isfile(os.path.join(thing_dir, "hello.txt")), log)
	checks.check("the file without extraction has its own name in the source folder",
		os.path.isfile(os.path.join(raw_dir, "raw.7z")), log)
	server.script({})
	result = subprocess.run(command + ["--fresh"], env=env, capture_output=True, text=True, encoding="utf-8",
		errors="replace")
	checks.check("a second configure (--fresh) needs no download", result.returncode == 0 and not server.requests,
		result.stdout + result.stderr)
	# FetchContent's own URL_HASH check on the local file stands behind patella_download's.
	binary_bad = os.path.join(folder, "binary-bad")
	server.script({"/thing-1.0.tar.gz": [504, BAD]})
	command_bad = [part if part != binary else binary_bad for part in command]
	result = subprocess.run(command_bad, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")
	checks.check("a wrong archive after a 504 fails the configure", result.returncode != 0, result.stdout + result.stderr)
	checks.check("the wrong archive is not kept", not any(name.endswith(".tar.gz")
		for _, _, names in os.walk(os.path.join(binary_bad, "_deps", "patella-downloads")) for name in names),
		result.stdout + result.stderr)


def main():
	checks = Checks()
	scratch = tempfile.mkdtemp(prefix="patella-download-selftest-")
	server = Server()
	thread = threading.Thread(target=server.serve_forever, daemon=True)
	thread.start()
	try:
		run_scenarios(checks, server, scratch)
		run_pauses(checks, server, scratch)
		run_project(checks, server, scratch)
	finally:
		server.shutdown()
		server.server_close()
		shutil.rmtree(scratch, ignore_errors=True)
	print("download_selftest.py: %d checks passed, %d failed" % (checks.passed, checks.failed), flush=True)
	return 1 if checks.failed else 0


if __name__ == "__main__":
	sys.exit(main())
