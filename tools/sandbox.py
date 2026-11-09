r"""Run a tool's Tibia.exe side in Windows Sandbox, off the user's desktop (Patella-Modern issue #147).

Tibia.exe's live runs (compare_traffic.py, compare_files.py, compare_rendering.py: the original client, DirectX 9,
patcap.dll, read through its memory by tools/client_state.py and driven by posted window messages) need a desktop.
Until this issue they used the user's: their windows came up in front of the user's own programs. Now these tools run
themselves again inside Windows Sandbox by default (a clean, disposable Windows with a desktop of its own), and
`--desktop` keeps the old way. notes/decisions.md, "#147"; notes/testserver.md, "Windows Sandbox".

  python -I tools/sandbox.py run [--show-window] [--keep] [--display WxH] [--timeout S] [--writable DIR]...
      -- <tool.py> [arguments]
      runs `python -I -u <tool.py> [arguments]` inside a new Sandbox and returns its exit code, its output streamed
  python -I tools/sandbox.py --self-test
      the parts that need no Sandbox: the configuration, the mappings, the tunnel's protocol on loopback
  python -I tools/sandbox.py normal-user -- <command> [arguments]
      runs the command here, no Sandbox, with a normal user's token at medium integrity, and returns its exit code; what
      GitHub's elevated runner needs for tests_vulkan (issue #163, notes/vulkan.md, "The CPU driver")
  python -I tools/sandbox.py guest <exchange folder>
      the Sandbox's side (its LogonCommand; not for a user)

In a tool (its `if __name__ == "__main__":` block, before main):
  sandbox.redirect(writable=[<the tool's work folder>], skip=("--self-test", ...))
returns at once, and the tool goes on in this process, inside the Sandbox, with `--desktop` (removed from sys.argv)
or desktop.HIDDEN on the command line, with an argument of skip, or off Windows; otherwise it runs the same command
line in the Sandbox (run below) and exits with its exit code.

How a run goes (run):
  1. One Sandbox at a time (Windows refuses a second one, by WindowsSandbox.exe and by `wsb start`: measured in issue
     #154, notes/decisions.md "#154"): a lock file in the user's temporary folder, held while the Sandbox runs; a
     second run waits for it (and for a Sandbox someone else started), it does not fail.
  2. The configuration (tools/sandbox/patella.wsb.template, filled in; build\sandbox\<run>\patella.wsb): virtual GPU
     and networking on; mapped at the same paths as on the host, so every path of the tools and of the command line
     is valid inside: the portable Python (read-only), the repository (read-only), the run's private test server
     (issue #146: its folder read-only, its copy's data\usr writable: the tools restore the test characters' player
     files; without one the master's folder, read-only), the tool's work
     folders and --out (writable), the folder of any other existing path on the command line (read-only), and the
     run's exchange folder build\sandbox\<run> (writable).
  3. WindowsSandbox.exe starts it on a hidden desktop of its own (CreateDesktopW, STARTUPINFO.lpDesktop, as
     tools/desktop.py), so the Sandbox's window is made there and never appears on the user's desktop, in the
     taskbar or in front of anything, and never takes the focus. (The Sandbox's Windows does not care: its own
     desktop is the input desktop inside the virtual machine, unlike desktop.py's hidden desktop for a client.)
     --show-window (and --keep) puts it on this desktop instead, minimized without activation; Windows Sandbox opens
     it restored and in the foreground anyway, so WindowKeeper minimizes it again within 50 ms. --display WxH sizes
     the window so that the Sandbox's display has that size (it follows its window; compare_rendering.py's fullscreen
     runs need 1024x768). The Sandbox logs on and starts this script's guest part (LogonCommand), which turns the
     Sandbox's own firewall off (only inside it), starts the tunnel and writes ready.json.
  4. The test server stays on the host (since issue #146 the run's private one, tools/testserver.py). Its ports
     (127.0.0.1:7171 and 7172, or the run's own: tunnel.json tells the guest) appear inside the Sandbox at the same
     numbers on the Sandbox's own 127.0.0.1, so the clients are patched
     and the proxy forwards exactly as on the host: a connection to them inside goes, through a reverse tunnel, to
     the host's 127.0.0.1. Reverse: the host connects to the Sandbox (its address from ready.json), never the other
     way, because the host's firewall drops what comes from the Sandbox's network (and the login server listens on
     127.0.0.1 only). `testserver.py start|stop` inside the Sandbox is passed to the host (forward_testserver), which
     runs it there, as does compare_traffic.server_running's question.
  5. The guest runs the command with the environment's PATELLA_* variables and its output in output.log, which the
     host prints as it grows; then exit.json. The command runs with a normal user's token at medium integrity (as
     the tools run on the host): the Sandbox's account runs at high integrity, where the Vulkan loader ignores
     VK_DRIVER_FILES as for an elevated process, so the x64 client's pinned CPU driver was never found. The host asks the Sandbox to shut down (shutdown /s inside it) and waits
     for its processes to end. A test server started for the run and left running is stopped.

Nothing CipSoft-derived leaves the machine: the data files and results stay in the mapped folders of this computer.
"""

import argparse
import ctypes
import json
import os
import re
import socket
import subprocess
import sys
import tempfile
import threading
import time
import uuid

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE = os.path.join(ROOT, "tools", "sandbox", "patella.wsb.template")
RUNS = os.path.join(ROOT, "build", "sandbox")
ENV_IO = "PATELLA_SANDBOX_IO"  # inside the Sandbox: the run's exchange folder
DESKTOP = "--desktop"
DESKTOP_HELP = ("run Tibia.exe on this session's desktop, as before issue #147 (default: inside Windows Sandbox, "
	"tools/sandbox.py)")
HIDDEN = "--hidden"  # desktop.HIDDEN: the experimental hidden desktop is on the host's
WINDOWS = os.name == "nt"
SANDBOX_EXE = r"C:\Windows\System32\WindowsSandbox.exe"
# The Sandbox's processes on the host: the window (WindowsSandboxRemoteSession.exe), the launcher and its server,
# the virtual machine's memory process.
SANDBOX_PROCESSES = ("WindowsSandbox.exe", "WindowsSandboxRemoteSession.exe", "WindowsSandboxClient.exe",
	"vmmemWindowsSandbox")
WINDOW_PROCESSES = ("WindowsSandbox.exe", "WindowsSandboxRemoteSession.exe", "WindowsSandboxClient.exe")
SERVER_PORTS = (7171, 7172)  # the test server's (tools/testserver.py), tunnelled into the Sandbox; a run's own: run()
CONTROL_PORT = 47170  # inside the Sandbox: the host's control connection of the tunnel
DATA_PORT = 47169  # inside the Sandbox: the host's data connections, one per tunnelled connection
MEMORY_MB = 8192
READY_TIMEOUT = 300  # seconds from WindowsSandbox.exe's start to the guest's ready.json
SHUTDOWN_TIMEOUT = 120  # seconds for the Sandbox's processes to end after its shutdown
POLL = 0.2


class SandboxError(Exception):
	pass


def log(text):
	print("[sandbox] %s" % text, flush=True)


def inside():
	"""True inside the Sandbox (the guest's command has PATELLA_SANDBOX_IO)."""
	return bool(os.environ.get(ENV_IO))


# --- files of the exchange folder ----------------------------------------------------------------------------------

def write_json(path, value):
	"""Written whole or not at all: a temporary file renamed (the other side polls for the name)."""
	tmp = path + ".tmp"
	with open(tmp, "w", encoding="utf-8", newline="\n") as f:
		json.dump(value, f)
	os.replace(tmp, path)


def read_json(path):
	try:
		with open(path, encoding="utf-8") as f:
			return json.load(f)
	except (OSError, ValueError):
		return None


def wait_json(path, timeout, alive=None, what=None):
	deadline = time.monotonic() + timeout
	while True:
		value = read_json(path)
		if value is not None:
			return value
		if time.monotonic() > deadline:
			raise SandboxError("no %s within %d s" % (what or os.path.basename(path), timeout))
		if alive is not None and not alive():
			raise SandboxError("the Sandbox ended before %s" % (what or os.path.basename(path)))
		time.sleep(POLL)


# --- requests from the guest to the host (testserver.py) ------------------------------------------------------------

def request(op, args, timeout=1800):
	"""Inside the Sandbox: asks the host to do `op` (requests\\<id>.json), waits for its answer (answers\\<id>.json)."""
	io = os.environ[ENV_IO]
	name = "%d-%s" % (time.time_ns(), uuid.uuid4().hex[:8])
	os.makedirs(os.path.join(io, "requests"), exist_ok=True)
	write_json(os.path.join(io, "requests", name + ".json"), {"op": op, "args": args})
	answer = os.path.join(io, "answers", name + ".json")
	value = wait_json(answer, timeout, what="the host's answer to %s %s" % (op, " ".join(args)))
	os.remove(answer)
	return value


def forward_testserver(argv):
	"""testserver.py's main inside the Sandbox: start and stop run on the host (the server stays there); prints the
	host's output and returns its exit code. None: not forwarded (self-test, or not inside the Sandbox)."""
	if not inside() or not argv or argv[0] not in ("start", "stop"):
		return None
	answer = request("testserver", list(argv))
	sys.stdout.write(answer["output"])
	sys.stdout.flush()
	return answer["code"]


def server_running(server_dir):
	"""Inside the Sandbox: whether the host runs a tibia-server.exe of server_dir (testserver.running)."""
	return request("server-running", [server_dir])["running"]


def serve_request(io, name, value, state):
	"""The host's side of one request."""
	op, args = value.get("op"), value.get("args", [])
	if op == "testserver" and args and args[0] in ("start", "stop"):
		path = os.path.join(io, "testserver-%s.log" % name)
		# output to a file, not a pipe: the server inherits the handles (compare_traffic.server_script)
		with open(path, "w", encoding="utf-8") as f:
			code = subprocess.call([sys.executable, "-I", os.path.join(ROOT, "tools", "testserver.py")] + args,
				stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, cwd=ROOT)
		with open(path, encoding="utf-8", errors="replace") as f:
			output = f.read()
		if args[0] == "start" and code == 0:
			state["server"] = True
		elif args[0] == "stop" and code == 0:
			state["server"] = False
		log("testserver.py %s for the Sandbox: exit %d" % (" ".join(args), code))
		return {"code": code, "output": output}
	if op == "server-running" and len(args) == 1:
		sys.path.insert(0, os.path.join(ROOT, "tools"))
		import testserver
		return {"running": testserver.running(args[0])}  # issue #146: the run's private server
	return {"code": 2, "output": "sandbox.py: unknown request %r\n" % value}


def serve_requests(io, state, stop):
	folder = os.path.join(io, "requests")
	answers = os.path.join(io, "answers")
	os.makedirs(folder, exist_ok=True)
	os.makedirs(answers, exist_ok=True)
	while not stop.is_set():
		for entry in sorted(os.listdir(folder)):
			if not entry.endswith(".json"):
				continue
			path = os.path.join(folder, entry)
			value = read_json(path)
			if value is None:
				continue
			os.remove(path)
			name = entry[:-5]
			try:
				answer = serve_request(io, name, value, state)
			except Exception as e:  # the guest waits for an answer: give it the failure
				answer = {"code": 2, "output": "sandbox.py: %s failed on the host: %s\n" % (value, e), "running": False}
			write_json(os.path.join(answers, entry), answer)
		stop.wait(POLL)


# --- the tunnel: the host's 127.0.0.1 ports inside the Sandbox ------------------------------------------------------

def pipe(a, b):
	"""Copies a's bytes to b until a's end, then ends b's sending half (the other direction goes on)."""
	try:
		while True:
			data = a.recv(65536)
			if not data:
				break
			b.sendall(data)
	except OSError:
		pass
	try:
		b.shutdown(socket.SHUT_WR)
	except OSError:
		pass


def join(a, b):
	"""Both directions between a and b; both closed when both are done."""
	def run():
		other = threading.Thread(target=pipe, args=(b, a), daemon=True)
		other.start()
		pipe(a, b)
		other.join()
		for s in (a, b):
			try:
				s.close()
			except OSError:
				pass
	threading.Thread(target=run, daemon=True).start()


class GuestTunnel:
	"""Inside the Sandbox: listens on 127.0.0.1 at `ports`; each connection is announced to the host on the control
	connection ("open <id> <port>"), and the host's data connection that names the id ("<id>") carries it."""

	def __init__(self, ports, control_port=CONTROL_PORT, data_port=DATA_PORT, bind="0.0.0.0", local="127.0.0.1"):
		self.lock = threading.Lock()
		self.pending = {}
		self.next_id = 1
		self.control = None
		self.control_ready = threading.Event()
		self.listeners = []
		for port in ports:
			s = socket.socket()
			s.bind((local, port))
			s.listen(16)
			self.listeners.append((s, port))
		self.control_listener = socket.socket()
		self.control_listener.bind((bind, control_port))
		self.control_listener.listen(1)
		self.data_listener = socket.socket()
		self.data_listener.bind((bind, data_port))
		self.data_listener.listen(16)

	def start(self):
		threading.Thread(target=self.accept_control, daemon=True).start()
		threading.Thread(target=self.accept_data, daemon=True).start()
		for s, port in self.listeners:
			threading.Thread(target=self.accept_local, args=(s, port), daemon=True).start()

	def accept_control(self):
		while True:
			conn, _ = self.control_listener.accept()
			with self.lock:
				if self.control is not None:
					try:
						self.control.close()
					except OSError:
						pass
				self.control = conn
			self.control_ready.set()

	def accept_local(self, listener, port):
		while True:
			client, _ = listener.accept()
			if not self.control_ready.wait(30):
				client.close()
				continue
			with self.lock:
				ident = self.next_id
				self.next_id += 1
				self.pending[ident] = client
				try:
					self.control.sendall(b"open %d %d\n" % (ident, port))
				except OSError:
					self.pending.pop(ident).close()

	def accept_data(self):
		while True:
			conn, _ = self.data_listener.accept()
			threading.Thread(target=self.pair, args=(conn,), daemon=True).start()

	def pair(self, conn):
		line = b""
		try:
			conn.settimeout(30)
			while not line.endswith(b"\n") and len(line) < 64:
				byte = conn.recv(1)
				if not byte:
					break
				line += byte
			conn.settimeout(None)
			words = line.split()
			ident = int(words[0])
		except (OSError, ValueError, IndexError):
			conn.close()
			return
		with self.lock:
			client = self.pending.pop(ident, None)
		if client is None:
			conn.close()
		elif len(words) > 1:  # "<id> refused": the host's port does not answer
			client.close()
			conn.close()
		else:
			join(client, conn)


class HostTunnel:
	"""On the host: the control connection to the guest; each "open <id> <port>" connects 127.0.0.1:<port> here and a
	data connection to the guest, and joins them."""

	def __init__(self, address, control_port=CONTROL_PORT, data_port=DATA_PORT, local="127.0.0.1"):
		self.address = address
		self.data_port = data_port
		self.local = local
		self.control = socket.create_connection((address, control_port), timeout=30)
		self.control.settimeout(None)
		self.opened = 0

	def start(self):
		threading.Thread(target=self.run, daemon=True).start()

	def close(self):
		try:
			self.control.close()
		except OSError:
			pass

	def run(self):
		buffer = b""
		while True:
			try:
				data = self.control.recv(4096)
			except OSError:
				return
			if not data:
				return
			buffer += data
			while b"\n" in buffer:
				line, buffer = buffer.split(b"\n", 1)
				words = line.split()
				if len(words) == 3 and words[0] == b"open":
					threading.Thread(target=self.open, args=(int(words[1]), int(words[2])), daemon=True).start()

	def open(self, ident, port):
		self.opened += 1
		try:
			local = socket.create_connection((self.local, port), timeout=10)
			local.settimeout(None)
		except OSError:
			local = None
		try:
			data = socket.create_connection((self.address, self.data_port), timeout=10)
			data.settimeout(None)
			data.sendall(b"%d\n" % ident if local is not None else b"%d refused\n" % ident)
		except OSError:
			if local is not None:
				local.close()
			return
		if local is None:
			data.close()
		else:
			join(local, data)


# --- the guest (inside the Sandbox) --------------------------------------------------------------------------------

def guest_address():
	"""The Sandbox's IPv4 address on its virtual network (not loopback, not link-local)."""
	found = []
	for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
		address = info[4][0]
		if not address.startswith(("127.", "169.254.")) and address not in found:
			found.append(address)
	if not found:
		raise SandboxError("the Sandbox has no network address (Networking is enabled in the configuration)")
	return found[0]


def call_as_normal_user(argv, cwd, env, out, say, stop=lambda: False):
	"""Runs argv with a normal user's token at medium integrity (SAFER's NORMALUSER level, the integrity label
	lowered), its output to the file out; returns its exit code. The Sandbox's account (WDAGUtilityAccount) runs at
	high integrity, where the Vulkan loader ignores VK_DRIVER_FILES as for an elevated process, so the pinned CPU
	driver next to the x64 client (lavapipe) was never found; on the host the tools run at medium integrity. Falls
	back to the guest's own token (said in guest.log) when Windows refuses. stop(): polled every 0.5 s; when it says
	so (the host gave up on the run), the job is ended and its code is 3."""
	import ctypes.wintypes as wt
	advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
	kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

	class SID_AND_ATTRIBUTES(ctypes.Structure):
		_fields_ = [("Sid", ctypes.c_void_p), ("Attributes", wt.DWORD)]

	class STARTUPINFOW(ctypes.Structure):
		_fields_ = [("cb", wt.DWORD), ("lpReserved", wt.LPWSTR), ("lpDesktop", wt.LPWSTR), ("lpTitle", wt.LPWSTR),
			("dwX", wt.DWORD), ("dwY", wt.DWORD), ("dwXSize", wt.DWORD), ("dwYSize", wt.DWORD),
			("dwXCountChars", wt.DWORD), ("dwYCountChars", wt.DWORD), ("dwFillAttribute", wt.DWORD),
			("dwFlags", wt.DWORD), ("wShowWindow", wt.WORD), ("cbReserved2", wt.WORD), ("lpReserved2", ctypes.c_void_p),
			("hStdInput", wt.HANDLE), ("hStdOutput", wt.HANDLE), ("hStdError", wt.HANDLE)]

	class PROCESS_INFORMATION(ctypes.Structure):
		_fields_ = [("hProcess", wt.HANDLE), ("hThread", wt.HANDLE), ("dwProcessId", wt.DWORD),
			("dwThreadId", wt.DWORD)]

	advapi32.SaferCreateLevel.argtypes = (wt.DWORD, wt.DWORD, wt.DWORD, ctypes.POINTER(wt.HANDLE), ctypes.c_void_p)
	advapi32.SaferComputeTokenFromLevel.argtypes = (wt.HANDLE, wt.HANDLE, ctypes.POINTER(wt.HANDLE), wt.DWORD,
		ctypes.c_void_p)
	advapi32.ConvertStringSidToSidW.argtypes = (wt.LPCWSTR, ctypes.POINTER(ctypes.c_void_p))
	advapi32.SetTokenInformation.argtypes = (wt.HANDLE, ctypes.c_int, ctypes.c_void_p, wt.DWORD)
	advapi32.CreateProcessAsUserW.argtypes = (wt.HANDLE, wt.LPCWSTR, wt.LPWSTR, ctypes.c_void_p, ctypes.c_void_p,
		wt.BOOL, wt.DWORD, ctypes.c_void_p, wt.LPCWSTR, ctypes.POINTER(STARTUPINFOW), ctypes.POINTER(PROCESS_INFORMATION))
	kernel32.WaitForSingleObject.argtypes = (wt.HANDLE, wt.DWORD)
	kernel32.GetExitCodeProcess.argtypes = (wt.HANDLE, ctypes.POINTER(wt.DWORD))
	kernel32.CloseHandle.argtypes = (wt.HANDLE,)
	kernel32.SetHandleInformation.argtypes = (wt.HANDLE, wt.DWORD, wt.DWORD)
	level = wt.HANDLE()
	token = wt.HANDLE()
	sid = ctypes.c_void_p()
	# SAFER_SCOPEID_USER, SAFER_LEVELID_NORMALUSER, SAFER_LEVEL_OPEN; S-1-16-8192: medium integrity
	if not (advapi32.SaferCreateLevel(2, 0x20000, 1, ctypes.byref(level), None)
			and advapi32.SaferComputeTokenFromLevel(level, None, ctypes.byref(token), 0, None)
			and advapi32.ConvertStringSidToSidW("S-1-16-8192", ctypes.byref(sid))):
		say("no normal user's token (error %d): the job runs with the guest's" % ctypes.get_last_error())
		return call_plain(argv, cwd, env, out, stop)
	label = SID_AND_ATTRIBUTES(sid, 0x20)  # SE_GROUP_INTEGRITY
	if not advapi32.SetTokenInformation(token, 25, ctypes.byref(label), ctypes.sizeof(label) + 64):  # TokenIntegrityLevel
		say("the token's integrity not lowered (error %d)" % ctypes.get_last_error())
	handle = wt.HANDLE(msvcrt_handle(out))
	kernel32.SetHandleInformation(handle, 1, 1)  # HANDLE_FLAG_INHERIT
	startup = STARTUPINFOW()
	startup.cb = ctypes.sizeof(startup)
	startup.dwFlags = 0x100  # STARTF_USESTDHANDLES
	startup.hStdInput = None
	startup.hStdOutput = handle
	startup.hStdError = handle
	info = PROCESS_INFORMATION()
	block = ctypes.create_unicode_buffer("".join("%s=%s\0" % (k, v) for k, v in sorted(env.items())) + "\0")
	line = ctypes.create_unicode_buffer(subprocess.list2cmdline(argv))
	# CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW
	if not advapi32.CreateProcessAsUserW(token, None, line, None, None, True, 0x400 | 0x08000000, block, cwd,
			ctypes.byref(startup), ctypes.byref(info)):
		say("CreateProcessAsUserW failed (error %d): the job runs with the guest's token" % ctypes.get_last_error())
		return call_plain(argv, cwd, env, out, stop)
	say("job at medium integrity, a normal user's token (PID %d)" % info.dwProcessId)
	kernel32.TerminateProcess.argtypes = (wt.HANDLE, wt.UINT)
	while kernel32.WaitForSingleObject(info.hProcess, 500) != 0:
		if stop():
			say("the host ended the run: the job is ended")
			kernel32.TerminateProcess(info.hProcess, 3)
			kernel32.WaitForSingleObject(info.hProcess, 10000)
	code = wt.DWORD()
	kernel32.GetExitCodeProcess(info.hProcess, ctypes.byref(code))
	for h in (info.hThread, info.hProcess, token):
		kernel32.CloseHandle(h)
	return ctypes.c_int32(code.value).value


def call_plain(argv, cwd, env, out, stop):
	process = subprocess.Popen(argv, cwd=cwd, env=env, stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
	while process.poll() is None:
		if stop():
			process.kill()
		time.sleep(0.5)
	return process.returncode


def msvcrt_handle(f):
	import msvcrt as crt
	return crt.get_osfhandle(f.fileno())


def watch_display(io):
	"""The guest's screen size in display.json whenever it changes (the host sizes the Sandbox's display by it)."""
	user32 = ctypes.windll.user32
	user32.SetProcessDPIAware()
	last = None
	while True:
		now = {"width": user32.GetSystemMetrics(0), "height": user32.GetSystemMetrics(1)}
		if now != last:
			write_json(os.path.join(io, "display.json"), now)
			last = now
		time.sleep(0.25)


def guest(io):
	"""The Sandbox's LogonCommand: tunnel, ready.json, the job of job.json, exit.json, the shutdown."""
	started = time.time()
	note = open(os.path.join(io, "guest.log"), "a", encoding="utf-8")

	def say(text):
		note.write("%s %s\n" % (time.strftime("%H:%M:%S"), text))
		note.flush()

	try:
		# the Sandbox's own firewall (inside the disposable Windows only): the host's tunnel connects in
		result = subprocess.run(["netsh", "advfirewall", "set", "allprofiles", "state", "off"], capture_output=True,
			text=True)
		say("firewall inside the Sandbox off: exit %d" % result.returncode)
		threading.Thread(target=watch_display, args=(io,), daemon=True).start()
		ports = (read_json(os.path.join(io, "tunnel.json")) or {}).get("ports") or SERVER_PORTS  # issue #146
		tunnel = GuestTunnel(ports)
		tunnel.start()
		address = guest_address()
		write_json(os.path.join(io, "ready.json"), {"address": address, "control": CONTROL_PORT, "data": DATA_PORT,
			"ports": list(ports), "python": sys.version, "user": os.environ.get("USERNAME"),
			"logon": started})
		say("ready at %s" % address)
		deadline = time.monotonic() + 3600
		while True:  # the job, or the host's shutdown without one (a failed preparation)
			job = read_json(os.path.join(io, "job.json"))
			if job is not None or read_json(os.path.join(io, "shutdown.json")) is not None:
				break
			if time.monotonic() > deadline:
				raise SandboxError("no job.json within 3600 s")
			time.sleep(POLL)
		if job is None:
			raise SandboxError("shut down before a job came")
		env = dict(os.environ)
		env.update(job.get("env", {}))
		env[ENV_IO] = io
		say("job: %s" % job["argv"])
		begin = time.time()
		with open(os.path.join(io, "output.log"), "wb") as out:
			code = call_as_normal_user(job["argv"], job.get("cwd") or ROOT, env, out, say,
				stop=lambda: read_json(os.path.join(io, "shutdown.json")) is not None)
		say("job exit %d after %.1f s" % (code, time.time() - begin))
		write_json(os.path.join(io, "exit.json"), {"code": code, "seconds": time.time() - begin})
	except Exception as e:
		say("failed: %r" % e)
		write_json(os.path.join(io, "exit.json"), {"code": 2, "error": repr(e)})
	finally:
		value = wait_json(os.path.join(io, "shutdown.json"), 24 * 3600, what="shutdown.json")
		say("shutdown: %s" % value)
		note.close()
		if value.get("shutdown", True):
			subprocess.call(["shutdown", "/s", "/t", "0", "/f"])


# --- the host ------------------------------------------------------------------------------------------------------

if WINDOWS:
	import ctypes
	import ctypes.wintypes as wt
	import msvcrt

	user32 = ctypes.WinDLL("user32", use_last_error=True)
	kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
	SW_SHOWMINNOACTIVE = 7
	PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
	TH32CS_SNAPPROCESS = 0x2
	ENUM_PROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

	class PROCESSENTRY32W(ctypes.Structure):
		_fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
			("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
			("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
			("szExeFile", ctypes.c_wchar * 260)]

	kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
	kernel32.CreateToolhelp32Snapshot.argtypes = (wt.DWORD, wt.DWORD)
	kernel32.Process32FirstW.argtypes = (wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W))
	kernel32.Process32NextW.argtypes = (wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W))
	kernel32.CloseHandle.argtypes = (wt.HANDLE,)
	user32.EnumWindows.argtypes = (ENUM_PROC, wt.LPARAM)
	user32.GetWindowThreadProcessId.argtypes = (wt.HWND, ctypes.POINTER(wt.DWORD))
	user32.IsWindowVisible.argtypes = (wt.HWND,)
	user32.IsIconic.argtypes = (wt.HWND,)
	user32.ShowWindow.argtypes = (wt.HWND, ctypes.c_int)
	user32.EnumDesktopWindows.argtypes = (wt.HANDLE, ENUM_PROC, wt.LPARAM)
	user32.GetClassNameW.argtypes = (wt.HWND, wt.LPWSTR, ctypes.c_int)
	user32.GetWindowRect.argtypes = (wt.HWND, ctypes.POINTER(wt.RECT))
	user32.GetClientRect.argtypes = (wt.HWND, ctypes.POINTER(wt.RECT))
	user32.SetWindowPos.argtypes = (wt.HWND, wt.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.UINT)
	user32.PostMessageW.argtypes = (wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM)
	user32.GetForegroundWindow.restype = wt.HWND


def processes():
	"""{pid: image name} of every process (CreateToolhelp32Snapshot: also the Sandbox's vmmem process, which
	OpenProcess refuses)."""
	found = {}
	snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
	if not snap or snap == wt.HANDLE(-1).value:
		return found
	try:
		entry = PROCESSENTRY32W()
		entry.dwSize = ctypes.sizeof(entry)
		ok = kernel32.Process32FirstW(snap, ctypes.byref(entry))
		while ok:
			found[entry.th32ProcessID] = entry.szExeFile
			ok = kernel32.Process32NextW(snap, ctypes.byref(entry))
	finally:
		kernel32.CloseHandle(snap)
	return found


def sandbox_processes():
	"""{pid: name} of the Sandbox's processes running now (any Sandbox, also one started by hand)."""
	names = {n.lower() for n in SANDBOX_PROCESSES}
	return {pid: name for pid, name in processes().items() if name.lower() in names}


def sandbox_windows():
	"""[(hwnd, iconic, foreground)] of the visible top-level windows of the Sandbox's processes."""
	names = {n.lower() for n in WINDOW_PROCESSES}
	pids = {pid for pid, name in processes().items() if name.lower() in names}
	found = []
	foreground = user32.GetForegroundWindow()

	def visit(hwnd, _):
		pid = wt.DWORD()
		user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
		if pid.value in pids and user32.IsWindowVisible(hwnd):
			found.append((hwnd, bool(user32.IsIconic(hwnd)), hwnd == foreground))
		return True

	user32.EnumWindows(ENUM_PROC(visit), 0)
	return found


class WindowKeeper:
	"""Keeps the Sandbox's windows minimized without activating them (SW_SHOWMINNOACTIVE), polling every 50 ms:
	Windows Sandbox shows its window restored whatever its launcher was told. Records what it saw for the run's
	summary: how often a window of the Sandbox showed restored, and whether one was the foreground window."""

	def __init__(self):
		self.stop = threading.Event()
		self.restored = []  # seconds after the start at which a restored window was minimized
		self.foreground = []  # seconds after the start at which a Sandbox window was the foreground window
		self.started = time.monotonic()
		self.thread = threading.Thread(target=self.run, daemon=True)

	def run(self):
		while not self.stop.is_set():
			now = round(time.monotonic() - self.started, 2)
			for hwnd, iconic, foreground in sandbox_windows():
				if foreground:
					self.foreground.append(now)
				if not iconic:
					user32.ShowWindow(hwnd, SW_SHOWMINNOACTIVE)
					self.restored.append(now)
			self.stop.wait(0.05)

	def summary(self):
		text = ("its window showed restored %d time(s)%s" % (len(self.restored), " (first %.2f s after the start)" %
			self.restored[0] if self.restored else ""))
		if self.foreground:
			text += ", was the foreground window in %d poll(s) (%.2f to %.2f s)" % (len(self.foreground),
				self.foreground[0], self.foreground[-1])
		else:
			text += ", never the foreground window"
		return text


class Lock:
	"""One Sandbox at a time: an exclusive lock on a file in the user's temporary folder (msvcrt.locking; Windows
	releases it when the process ends). A second run waits for it: a second WindowsSandbox.exe would only leave an
	error window ("only one instance", issue #154) that stays until it is closed."""

	PATH = os.path.join(tempfile.gettempdir(), "patella-sandbox.lock")

	def __init__(self, what):
		self.what = what
		self.file = None

	def __enter__(self):
		announced = False
		self.file = open(self.PATH, "a+b")
		while True:
			try:
				self.file.seek(0)
				msvcrt.locking(self.file.fileno(), msvcrt.LK_NBLCK, 1)
			except OSError:
				if not announced:
					log("another run holds the Sandbox (%s): waiting" % self.holder())
					announced = True
				time.sleep(5)
				continue
			others = sandbox_processes()
			if not others:
				break
			msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
			if not announced:
				log("a Sandbox runs already (%s): waiting until it ends" % ", ".join(sorted(set(others.values()))))
				announced = True
			time.sleep(5)
		with open(self.PATH + ".holder", "w", encoding="utf-8") as f:
			f.write("pid %d since %s: %s" % (os.getpid(), time.strftime("%H:%M:%S"), self.what))
		return self

	def holder(self):
		try:
			with open(self.PATH + ".holder", encoding="utf-8") as f:
				return f.read().strip()
		except OSError:
			return "unknown"

	def __exit__(self, *exc):
		try:
			self.file.seek(0)
			msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
		except OSError:
			pass
		self.file.close()


def python_home():
	"""The portable Python's own folder (links resolved: the Sandbox maps a real folder)."""
	return os.path.realpath(sys.base_prefix)


def norm(path):
	return os.path.normcase(os.path.realpath(os.path.abspath(path)))


def under(path, folder):
	path, folder = norm(path), norm(folder)
	return path == folder or path.startswith(folder.rstrip("\\/") + os.sep)


def server_folders():
	"""(the folder mapped read-only, its writable data\\usr or None, the ports tunnelled): the run's private server
	(testserver.py, issue #146: its run folder with instance.json and the copy), else the master, read-only."""
	sys.path.insert(0, os.path.join(ROOT, "tools"))
	import testserver
	if testserver.INSTANCE is not None:
		return (testserver.INSTANCE["folder"], os.path.join(testserver.SERVER_DIR, "data", "usr"),
			(testserver.LOGIN_PORT, testserver.GAME_PORT))
	return testserver.MASTER_DIR, None, SERVER_PORTS


def path_arguments(args):
	"""The existing absolute paths among the arguments (also as --name=path)."""
	found = []
	for arg in args:
		value = arg.split("=", 1)[1] if arg.startswith("--") and "=" in arg else arg
		if os.path.isabs(value) and os.path.exists(value):
			found.append(value)
	return found


def out_arguments(args):
	"""The values of --out (the tools' result folder): written inside the Sandbox."""
	found = []
	for i, arg in enumerate(args):
		if arg == "--out" and i + 1 < len(args):
			found.append(args[i + 1])
		elif arg.startswith("--out="):
			found.append(arg.split("=", 1)[1])
	return [os.path.abspath(p) for p in found]


def mappings(io, writable, args, gameserver, usr=None):
	"""[(folder, read_only)] in the order the Sandbox maps them, each at its own path inside. A writable folder under
	a read-only one comes after it. gameserver: the test server's folder (read-only), usr: its writable data\\usr
	(the run's private copy's; never the master's, issue #146)."""
	result = [(python_home(), True), (ROOT, True)]
	if gameserver and os.path.isdir(gameserver):
		result.append((gameserver, True))
		if usr and os.path.isdir(usr):
			result.append((usr, False))
	for path in path_arguments(args):
		folder = path if os.path.isdir(path) else os.path.dirname(path)
		if not any(under(folder, f) for f, _ in result):
			result.append((folder, True))
	for folder in list(writable) + [io]:
		os.makedirs(folder, exist_ok=True)
		if not any(norm(folder) == norm(f) for f, _ in result):
			result.append((folder, False))
	return [(os.path.realpath(os.path.abspath(f)), ro) for f, ro in result]


def xml_text(text):
	return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def configuration(folders, command, memory=MEMORY_MB):
	"""The .wsb text: the template with its comment dropped and the fields filled in."""
	with open(TEMPLATE, encoding="utf-8") as f:
		template = f.read()
	template = re.sub(r"<!--.*?-->\s*", "", template, flags=re.S)
	lines = ""
	for folder, read_only in folders:
		lines += ("\t\t<MappedFolder>\n\t\t\t<HostFolder>%s</HostFolder>\n\t\t\t<SandboxFolder>%s</SandboxFolder>\n"
			"\t\t\t<ReadOnly>%s</ReadOnly>\n\t\t</MappedFolder>\n" % (xml_text(folder), xml_text(folder),
				"true" if read_only else "false"))
	return template.replace("{folders}", lines).replace("{command}", xml_text(command)).replace("{memory}",
		str(memory))


def guest_command(io):
	return '"%s" -I "%s" guest "%s"' % (os.path.join(python_home(), "python.exe"), os.path.join(ROOT, "tools",
		"sandbox.py"), io)


def tail(path, position):
	"""Prints what was added to path since position; returns the new position."""
	try:
		with open(path, "rb") as f:
			f.seek(position)
			data = f.read()
	except OSError:
		return position
	if data:  # the guest's bytes as they are (its encoding is the tool's): no decoding that could fail here
		sys.stdout.flush()
		sys.stdout.buffer.write(data)
		sys.stdout.buffer.flush()
	return position + len(data)


class Launcher:
	"""WindowsSandbox.exe, started minimized without activation; with hidden_desktop on a new desktop of its own
	(desktop.py's CreateDesktopW), kept open until close()."""

	def __init__(self, wsb, hidden_desktop):
		self.desktop = None
		self.process = None
		if not hidden_desktop:
			info = subprocess.STARTUPINFO()
			info.dwFlags = subprocess.STARTF_USESHOWWINDOW
			info.wShowWindow = SW_SHOWMINNOACTIVE
			self.process = subprocess.Popen([SANDBOX_EXE, wsb], startupinfo=info)
			return
		sys.path.insert(0, os.path.join(ROOT, "tools"))
		import desktop
		station = desktop.object_name(desktop.user32.GetProcessWindowStation())
		name = "patella-sandbox-%d-%s" % (os.getpid(), uuid.uuid4().hex[:8])
		self.desktop = desktop.user32.CreateDesktopW(name, None, None, 0, desktop.GENERIC_ALL, None)
		if not self.desktop:
			raise SandboxError("CreateDesktopW failed (error %d)" % ctypes.get_last_error())
		startup = desktop.STARTUPINFOW()
		startup.cb = ctypes.sizeof(startup)
		startup.lpDesktop = station + "\\" + name
		startup.dwFlags = 0x1  # STARTF_USESHOWWINDOW
		startup.wShowWindow = SW_SHOWMINNOACTIVE
		info = desktop.PROCESS_INFORMATION()
		line = ctypes.create_unicode_buffer(subprocess.list2cmdline([SANDBOX_EXE, wsb]))
		if not desktop.kernel32.CreateProcessW(None, line, None, None, False, 0, None, None, ctypes.byref(startup),
			ctypes.byref(info)):
			raise SandboxError("CreateProcessW failed (error %d)" % ctypes.get_last_error())
		desktop.kernel32.CloseHandle(info.hThread)
		self.handle = info.hProcess
		log("WindowsSandbox.exe on the hidden desktop %s" % startup.lpDesktop)

	def poll(self):
		if self.process is not None:
			return self.process.poll()
		return None if ctypes.windll.kernel32.WaitForSingleObject(wt.HANDLE(self.handle), 0) else 0

	def windows(self):
		"""[(hwnd, class)] of the visible top-level windows on the hidden desktop."""
		found = []
		if not self.desktop:
			return found

		def visit(hwnd, _):
			if user32.IsWindowVisible(hwnd):
				name = ctypes.create_unicode_buffer(256)
				user32.GetClassNameW(hwnd, name, 256)
				found.append((hwnd, name.value))
			return True

		user32.EnumDesktopWindows(wt.HANDLE(self.desktop), ENUM_PROC(visit), 0)
		return found

	def resize(self, width, height):
		"""Sizes the Sandbox's window on the hidden desktop so that its client area is width x height (the Sandbox
		follows its window's size with its display's). Returns the windows it sized."""
		sized = []
		for hwnd, name in self.windows():
			if name != "WinUIDesktopWin32WindowClass":
				continue
			window = wt.RECT()
			client = wt.RECT()
			user32.GetWindowRect(hwnd, ctypes.byref(window))
			user32.GetClientRect(hwnd, ctypes.byref(client))
			extra_w = (window.right - window.left) - (client.right - client.left)
			extra_h = (window.bottom - window.top) - (client.bottom - client.top)
			user32.SetWindowPos(hwnd, None, 0, 0, width + extra_w, height + extra_h, 0x0010 | 0x0004)  # NOACTIVATE, NOZORDER
			sized.append(hwnd)
		return sized

	def close(self):
		if self.desktop:
			ctypes.windll.user32.CloseDesktop(wt.HANDLE(self.desktop))
			self.desktop = None


def run(argv, writable=(), keep=False, timeout=None, show_window=False, display=None):
	"""Runs `python -I -u <argv>` inside a new Sandbox (see the module's text); returns its exit code."""
	if not WINDOWS:
		raise SandboxError("Windows Sandbox is Windows'")
	if not os.path.isfile(SANDBOX_EXE):
		raise SandboxError("%s not found: Windows Sandbox is not enabled (the optional feature "
			"Containers-DisposableClientVM)" % SANDBOX_EXE)
	writable = list(writable) + out_arguments(argv[1:])
	what = " ".join([os.path.basename(argv[0])] + list(argv[1:]))
	with Lock(what):
		name = time.strftime("%Y%m%d-%H%M%S") + "-%d" % os.getpid()
		io = os.path.join(RUNS, name)
		os.makedirs(io)
		gameserver, usr, ports = server_folders()
		folders = mappings(io, writable, argv[1:], gameserver, usr)
		write_json(os.path.join(io, "tunnel.json"), {"ports": list(ports)})
		wsb = os.path.join(io, "patella.wsb")
		with open(wsb, "w", encoding="utf-8", newline="\r\n") as f:
			f.write(configuration(folders, guest_command(io)))
		log("run %s: %s (test server ports %s tunnelled)" % (io, what, ", ".join(map(str, ports))))
		for folder, read_only in folders:
			log("  mapped %s %s" % ("read-only" if read_only else "writable ", folder))
		keeper = WindowKeeper()
		keeper.thread.start()
		started = time.monotonic()
		launcher = Launcher(wsb, not (show_window or keep))  # a kept Sandbox needs a window to be closed with
		launcher.io = io
		state = {"server": False}
		stop = threading.Event()
		tunnel = None
		code = 2
		try:
			ready = wait_json(os.path.join(io, "ready.json"), READY_TIMEOUT,
				alive=lambda: bool(sandbox_processes()) or launcher.poll() is None, what="ready.json from the Sandbox")
			log("Sandbox ready after %.1f s at %s" % (time.monotonic() - started, ready["address"]))
			if display is not None:
				set_display(launcher, display)
			tunnel = HostTunnel(ready["address"], ready["control"], ready["data"])
			tunnel.start()
			threading.Thread(target=serve_requests, args=(io, state, stop), daemon=True).start()
			env = {k: v for k, v in os.environ.items() if k.startswith("PATELLA_") and k != ENV_IO}
			write_json(os.path.join(io, "job.json"), {"argv": [os.path.join(python_home(), "python.exe"), "-I", "-u"] + list(argv), "env": env,
				"cwd": ROOT})
			position = 0
			deadline = None if timeout is None else time.monotonic() + timeout
			output = os.path.join(io, "output.log")
			while True:
				position = tail(output, position)
				result = read_json(os.path.join(io, "exit.json"))
				if result is not None:
					position = tail(output, position)
					code = result["code"]
					if "error" in result:
						log("the guest failed: %s" % result["error"])
					break
				if not sandbox_processes():
					raise SandboxError("the Sandbox ended during the run (see %s)" % os.path.join(io, "guest.log"))
				if deadline is not None and time.monotonic() > deadline:
					raise SandboxError("the run took more than %d s" % timeout)
				time.sleep(POLL)
			log("%s: exit %d after %.1f s in all (%d tunnelled connections); %s" % (os.path.basename(argv[0]), code,
				time.monotonic() - started, tunnel.opened, keeper.summary()))
		finally:
			if state["server"]:
				log("the run left the test server running: stopping it")
				serve_request(io, "cleanup", {"op": "testserver", "args": ["stop"]}, state)
			stop.set()
			write_json(os.path.join(io, "shutdown.json"), {"shutdown": not keep})
			if tunnel is not None:
				tunnel.close()
			if keep:
				log("--keep: the Sandbox stays (close its window when done)")
			else:
				shutdown(started, launcher)
				launcher.close()
			keeper.stop.set()
	return code


def set_display(launcher, display):
	"""The Sandbox's display at display = (width, height): its window (on the hidden desktop) sized to it, then the
	guest's report of its screen (display.json, written by the guest's display watcher) awaited."""
	io = launcher.io
	width, height = display
	for attempt in range(4):
		before = read_json(os.path.join(io, "display.json"))
		if before is not None and (before["width"], before["height"]) == tuple(display):
			log("the Sandbox's display is %dx%d" % display)
			return
		if not launcher.resize(width, height):
			log("no Sandbox window to size for the display %dx%d (with --show-window it is the user's to size)" % display)
			return
		deadline = time.monotonic() + 15
		while time.monotonic() < deadline:
			now = read_json(os.path.join(io, "display.json"))
			if now is not None and now != before:
				break
			time.sleep(POLL)
		now = read_json(os.path.join(io, "display.json"))
		if now is None:
			break
		# the window's own bars inside its client area: grow it by what the display lacks
		width += display[0] - now["width"]
		height += display[1] - now["height"]
	now = read_json(os.path.join(io, "display.json"))
	raise SandboxError("the Sandbox's display is %s, not %dx%d" % ("%(width)dx%(height)d" % now if now else "unknown",
		display[0], display[1]))


def shutdown(started, launcher):
	"""Waits for the Sandbox's processes to end after the guest's shutdown; if they do not, closes its window."""
	deadline = time.monotonic() + SHUTDOWN_TIMEOUT
	while sandbox_processes():
		if time.monotonic() > deadline:
			log("the Sandbox did not end within %d s of its shutdown: closing its window" % SHUTDOWN_TIMEOUT)
			for hwnd in [w[0] for w in sandbox_windows()] + [w[0] for w in launcher.windows()]:
				user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
			end = time.monotonic() + 60
			while sandbox_processes() and time.monotonic() < end:
				time.sleep(1)
			break
		time.sleep(1)
	left = sandbox_processes()
	if left:
		log("still running: %s" % left)
	else:
		log("Sandbox closed, %.1f s after its start" % (time.monotonic() - started))


def wanted(skip=()):
	"""Whether redirect(skip=skip) would run the tool in the Sandbox (the arguments as they are now)."""
	args = sys.argv[1:]
	return WINDOWS and not inside() and DESKTOP not in args and HIDDEN not in args and not any(a in skip for a in args)


def redirect(writable=(), skip=(), display=None):
	"""In a tool's __main__ block (see the module's text): runs the tool again inside the Sandbox and exits with its
	exit code, unless it runs there already, --desktop or desktop.HIDDEN is given, an argument of skip is, or this is
	not Windows."""
	if not wanted(skip):
		sys.argv[1:] = [a for a in sys.argv[1:] if a != DESKTOP]
		return
	args = sys.argv[1:]
	try:
		code = run([os.path.abspath(sys.argv[0])] + args, writable=writable, display=display)
	except SandboxError as e:
		print("sandbox.py: %s" % e, file=sys.stderr)
		code = 2
	sys.exit(code)


# --- self-test -----------------------------------------------------------------------------------------------------

def self_test():
	"""The configuration and the mappings, and the tunnel's protocol on loopback (guest and host in this process, other
	ports than the server's)."""
	failures = []

	def check(ok, text):
		print("%s %s" % ("ok  " if ok else "FAIL", text))
		if not ok:
			failures.append(text)

	io = os.path.join(RUNS, "self-test")
	work = os.path.join(ROOT, "build", "traffic")
	folders = mappings(io, [work], ["--patella", sys.executable, "--count", "3"], None)
	check(folders[0] == (python_home(), True), "the portable Python first, read-only")
	check((os.path.realpath(ROOT), True) in folders, "the repository read-only")
	check((os.path.realpath(work), False) in folders and (os.path.realpath(io), False) in folders,
		"the work folder and the exchange folder writable")
	check(sum(1 for f, _ in folders if norm(f) == norm(os.path.dirname(sys.executable))) <= 1,
		"an argument's folder mapped once (here inside the Python's)")
	text = configuration(folders, guest_command(io))
	check("{" not in text and "<!--" not in text, "the template filled in, its comment dropped")
	check(text.count("<MappedFolder>") == len(folders), "one MappedFolder per mapping")
	check(out_arguments(["--out", "x", "--out=y"]) == [os.path.abspath("x"), os.path.abspath("y")], "--out found")

	# the tunnel: a local "server" behind the host side, a client on the guest side's port
	server = socket.socket()
	server.bind(("127.0.0.1", 0))
	server.listen(4)
	server_port = server.getsockname()[1]

	def echo():
		while True:
			try:
				conn, _ = server.accept()
			except OSError:
				return
			data = b""
			while True:
				chunk = conn.recv(4096)
				if not chunk:
					break
				data += chunk
			conn.sendall(data[::-1])
			conn.close()

	threading.Thread(target=echo, daemon=True).start()
	free = []
	for _ in range(3):
		s = socket.socket()
		s.bind(("127.0.0.1", 0))
		free.append(s.getsockname()[1])
		s.close()
	listen_port, control, data_port = free
	# the guest's listener at listen_port is the "server port"; the host maps it to server_port
	guest_side = GuestTunnel([listen_port], control, data_port, bind="127.0.0.1")
	guest_side.start()
	host_side = HostTunnel("127.0.0.1", control, data_port)

	def open_mapped(ident, port):
		HostTunnel.open(host_side, ident, server_port if port == listen_port else port)

	host_side.open = open_mapped
	host_side.start()
	results = []
	for i in range(3):
		c = socket.create_connection(("127.0.0.1", listen_port), timeout=10)
		payload = (b"patella %d " % i) * 1000
		c.sendall(payload)
		c.shutdown(socket.SHUT_WR)
		got = b""
		while True:
			chunk = c.recv(65536)
			if not chunk:
				break
			got += chunk
		c.close()
		results.append(got == payload[::-1])
	check(all(results), "three connections through the tunnel, both directions and the half-close")
	check(host_side.opened == 3, "the host opened one connection per tunnelled one")
	server.close()
	host_side.close()
	print("self-test: %s" % ("passed" if not failures else "%d failed" % len(failures)))
	return not failures


def normal_user(command):
	"""The command with a normal user's token at medium integrity, its output on this process's (issue #163): GitHub's
	Windows runner is elevated, where the Vulkan loader ignores VK_DRIVER_FILES, as in the Sandbox's account."""
	command = command[1:] if command[:1] == ["--"] else command
	if not command:
		print("sandbox.py normal-user: no command given", file=sys.stderr)
		return 2
	sys.stdout.flush()
	return call_as_normal_user(command, os.getcwd(), dict(os.environ), sys.stdout,
		lambda text: print("sandbox.py normal-user: %s" % text, file=sys.stderr, flush=True))


def main(argv):
	if argv and argv[0] == "normal-user":
		return normal_user(argv[1:])
	if argv and argv[0] == "guest":
		guest(argv[1])
		return 0
	if argv and argv[0] == "--self-test":
		return 0 if self_test() else 1
	parser = argparse.ArgumentParser(description="Run a tool inside Windows Sandbox (issue #147).")
	commands = parser.add_subparsers(dest="command", required=True)
	run_parser = commands.add_parser("run", help="run python -I -u <tool> [arguments] inside a new Sandbox")
	run_parser.add_argument("--keep", action="store_true", help="leave the Sandbox running afterwards (its window on "
		"this desktop, minimized: close it when done)")
	run_parser.add_argument("--show-window", action="store_true", help="the Sandbox's window on this desktop, minimized "
		"(default: on a hidden desktop of its own, never shown)")
	run_parser.add_argument("--timeout", type=int, help="seconds the run may take")
	run_parser.add_argument("--writable", action="append", default=[], help="a folder written inside (mapped "
		"writable; the repository is read-only)")
	run_parser.add_argument("--display", help="WxH: the Sandbox's display size (its window on the hidden desktop sized "
		"to it)")
	run_parser.add_argument("tool", nargs=argparse.REMAINDER, help="-- <tool.py> [arguments]")
	args = parser.parse_args(argv)
	tool = args.tool[1:] if args.tool[:1] == ["--"] else args.tool
	if not tool:
		parser.error("no tool given")
	tool[0] = os.path.abspath(tool[0])
	try:
		return run(tool, writable=[os.path.abspath(w) for w in args.writable], keep=args.keep, timeout=args.timeout,
			show_window=args.show_window, display=tuple(int(v) for v in args.display.split("x")) if args.display else None)
	except SandboxError as e:
		print("sandbox.py: %s" % e, file=sys.stderr)
		return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
