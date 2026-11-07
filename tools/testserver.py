r"""Start and stop the local 7.72 test server (issue #18, notes/testserver.md); Python since issue #10.

Usage:
  python -I tools/testserver.py start [--server-dir DIR] [--timeout 900] [--stall 180] [--after-online 60]
                                      [--retries 1] [--bind-retries 3] [--window]
  python -I tools/testserver.py stop [--server-dir DIR] [--timeout 60] [--force]
  python -I tools/testserver.py self-test      (no server: the start's wait on made-up servers, issue #68)

start runs tibia-server.exe (tibia-server elsewhere) from its own folder (--server-dir; default the
environment variable PATELLA_TESTSERVER, else C:\Tibia\gameserver, issue #2; where config.cfg, tibia.db
and tibia.pem are found relative to the working directory) in a hidden
console of its own (--window: minimised), writes its output to build\testserver\server.log (stderr:
server.err; both overwritten at each start) and its PID to build\testserver\server.pid, and waits
until the server is ready: it listens on the login port (7171) and the game port (7172) AND its world is
loaded, server.log's "Server Online!" (Patella-Modern issue #68: the ports open before the map loads, issue
#25, and a client that logged in then got no game). When server.log does not show the server's early line
"Starting Login Server" once both ports listen, its output does not reach the file as it is written and the
ports alone count (a line says so). It refuses to start when a tibia-server from that folder already runs or
anything listens on 7171 or 7172. An inherited "ignore Ctrl+C" flag (a shell such as Git Bash passes it on)
is cleared before the start, so that stop works.

The wait (issue #68) gives up on an attempt when the server exits; when it stalls: no progress (server.log
grew, a port began to listen, the process used CPU time) for --stall seconds, or "Server Online!" said but not
both ports listening --after-online seconds later (issue #88's run); or after --timeout seconds in all. A
busy machine only slows the start down (issue #75's second start took more than the old 300 s), so the
progress, not the clock, decides. A failed attempt's server, the one this start started (server.pid), is
stopped (Ctrl+C, killed after 30 s when it never came online and has nothing to save; once its world is loaded,
"Server Online!", it saves every map sector on the way out and gets 600 s: issue #137, where a kill after 30 s cut
a sector file off and every later start failed), its server.log kept as
server-attempt<N>.log, and the server started again, --retries times (default 1). A server the tools did not
start is never touched: when one runs or holds a port before a retry, start gives up.

A bind error in server.log (issue #95: "Failed to bind Login Server on port 7171 (error 10049)", the line the
server writes when its login thread cannot bind, after which it still says "Server Online!" and never listens on
7171; or its game server's "Failed to bind Game Server on port 7172.") fails the attempt at once, with the error
code and what it means (10049: the server's own start-up race, below; 10048: the port is taken; 10013: the port
is in a Windows excluded port range, which start then names, or held exclusively). The attempt's server is
stopped once its world is loaded ("Server Online!", so that its Ctrl+C is the usual shutdown, not one during the
map's load) and started again. Bind failures have their own --bind-retries (default 3), on top of --retries:
the race is the server's, comes back at random, and costs only a few seconds an attempt. The race (issue #95,
notes/testserver.md): the server's login thread copies its bind address from the global buffer that the main
thread's second ReadConfig resets to "0.0.0.0" and then fills with bind_address again; a copy taken in the
middle ("127.0.0", "1.0.0.0"...) is not a local address, and bind fails with WSAEADDRNOTAVAIL (10049); a copy of
"0.0.0.0" binds all interfaces (compare_traffic.py restarts the server then).
Exit code 0 when the server is ready, 1 when it is already running, failed to start, exited, or stalled in
every attempt.

stop sends Ctrl+C to the server's console (the shutdown it documents: it stops the login server,
logs all players out and saves, closes the database) and waits for it to exit; on Linux SIGINT. It
stops only the server start started: the process in build\testserver\server.pid, if it is still that
process (the same executable, the same creation time). A server the tools did not start (the user's
own, say) is never touched. The Ctrl+C is sent by a separate copy of this script that has no console
of its own, attaches to the server's and ignores the event itself. With --force, a server that has
not exited after --timeout seconds is killed (no save). Exit code 0 when the server the tools started
is not running afterwards, 1 otherwise (also when a server the tools did not start is running).

The printed lines start with "testserver start:" / "testserver stop:"; when the server is ready,
start prints "listening on <address>:<port>, ..." (tools/compare_traffic.py looks for 0.0.0.0:7171
there, the server's start-up race) and then "world loaded" or "world: not shown in server.log".
"""
import argparse
import ctypes
import ipaddress
import os
import re
import signal
import struct
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE_DIR = os.path.join(ROOT, "build", "testserver")
PID_FILE = os.path.join(STATE_DIR, "server.pid")
LOG_FILE = os.path.join(STATE_DIR, "server.log")
ERR_FILE = os.path.join(STATE_DIR, "server.err")
# The test server's folder (issue #2): PATELLA_TESTSERVER, else C:\Tibia\gameserver. The one place of
# that default: compare_traffic.py (and through it the other comparisons), proxy.py and compare_files.py
# take SERVER_DIR from here.
DEFAULT_SERVER_DIR = r"C:\Tibia\gameserver"
SERVER_DIR = os.environ.get("PATELLA_TESTSERVER") or DEFAULT_SERVER_DIR
SERVER_DIR_HELP = ("the server's folder (default %(default)s: PATELLA_TESTSERVER, else "
	+ DEFAULT_SERVER_DIR.replace("%", "%%") + ")")
EXE_NAME = "tibia-server.exe" if os.name == "nt" else "tibia-server"
LOGIN_PORT, GAME_PORT = 7171, 7172
WINDOWS = os.name == "nt"
# The server's output in server.log: a line it writes before it listens, and the line it writes once the
# world is loaded and it is online (issue #25; the wait moved here from testmode_check.py in issue #68).
LOG_EARLY = "Starting Login Server"
LOG_ONLINE = "Server Online!"
START_TIMEOUT = 900  # seconds an attempt may take in all
START_STALL = 180  # seconds without progress (output, a port, CPU time) before an attempt is given up
AFTER_ONLINE = 60  # seconds from "Server Online!" to both ports listening
START_RETRIES = 1
BIND_RETRIES = 3  # more starts after a bind error in server.log (issue #95), on top of START_RETRIES
STALLED_STOP = 30  # seconds a stalled server gets for its Ctrl+C before it is killed
# Seconds a failed attempt's server whose world is loaded ("Server Online!") gets for its Ctrl+C: it saves every map
# sector then, which took more than 30 s on a busy machine, and a kill in the middle left a sector file cut off (issue
# #137: 1040-1009-10.sec at 4096 bytes; every later start failed with "Cannot load sector").
SAVING_STOP = 600
# The lines the server writes when a bind fails (issue #95; its strings, login/login_server.cc and
# game/communication.cc): "<date> <time> [ERR] LoginServerWorker: Failed to bind Login Server on port 7171 (error
# 10049)" and "LaunchServer: Failed to bind Game Server on port 7172." (no code).
BIND_ERROR = re.compile(r"Failed to bind (Login|Game) Server on port (\d+)(?: \(error (\d+)\))?")
LOG_STAMP = re.compile(r"\d{4}-\d\d-\d\d \d\d:\d\d:\d\d \[")  # the time stamp of the server's log lines
# What a WinSock error code of a bind means here (issue #95).
BIND_CODES = {
	10049: "WSAEADDRNOTAVAIL: the address is not one of this machine's. Here it is the server's own start-up race:"
		" its login thread copies the bind address from a buffer the main thread rewrites at the same moment"
		" (notes/testserver.md, \"The bind race\"), not a Windows setting",
	10048: "WSAEADDRINUSE: another socket holds the port (a server of someone else's)",
	10013: "WSAEACCES: the port is in one of Windows' excluded port ranges (Hyper-V, WSL, Docker reserve them;"
		" netsh int ipv4 show excludedportrange protocol=tcp) or another process holds it exclusively",
}



def server_exe(server_dir):
	"""The server's path as the process list shows it (Windows: as started; Linux: resolved)."""
	path = os.path.join(server_dir, EXE_NAME)
	return os.path.normcase(os.path.abspath(path) if WINDOWS else os.path.realpath(path))


# --- processes ----------------------------------------------------------------------------

if WINDOWS:
	import ctypes.wintypes as wt

	kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
	iphlpapi = ctypes.WinDLL("iphlpapi")
	PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
	PROCESS_TERMINATE = 0x0001
	SYNCHRONIZE = 0x00100000
	WAIT_OBJECT_0 = 0
	kernel32.OpenProcess.restype = wt.HANDLE
	kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
	kernel32.CloseHandle.argtypes = [wt.HANDLE]
	kernel32.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
	kernel32.GetProcessTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4
	kernel32.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
	kernel32.WaitForSingleObject.restype = wt.DWORD
	kernel32.TerminateProcess.argtypes = [wt.HANDLE, wt.UINT]
	kernel32.K32EnumProcesses.argtypes = [ctypes.POINTER(wt.DWORD), wt.DWORD, ctypes.POINTER(wt.DWORD)]
	kernel32.SetConsoleCtrlHandler.argtypes = [ctypes.c_void_p, wt.BOOL]
	kernel32.AttachConsole.argtypes = [wt.DWORD]
	kernel32.GenerateConsoleCtrlEvent.argtypes = [wt.DWORD, wt.DWORD]
	iphlpapi.GetExtendedTcpTable.argtypes = [ctypes.c_void_p, ctypes.POINTER(wt.DWORD), wt.BOOL, wt.ULONG,
		ctypes.c_int, wt.ULONG]
	iphlpapi.GetExtendedTcpTable.restype = wt.DWORD

	def open_process(pid, access=PROCESS_QUERY_LIMITED_INFORMATION):
		return kernel32.OpenProcess(access, False, pid) or None

	def process_path(pid):
		handle = open_process(pid)
		if handle is None:
			return None
		try:
			size = wt.DWORD(32768)
			buffer = ctypes.create_unicode_buffer(size.value)
			if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
				return None
			return os.path.normcase(buffer.value)
		finally:
			kernel32.CloseHandle(handle)

	def process_started(pid):
		"""The creation time (100 ns units), which tells a process from a later one with its PID."""
		handle = open_process(pid)
		if handle is None:
			return None
		try:
			times = [wt.FILETIME() for _ in range(4)]
			if not kernel32.GetProcessTimes(handle, *[ctypes.byref(t) for t in times]):
				return None
			return (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
		finally:
			kernel32.CloseHandle(handle)

	def process_cpu(pid):
		"""The CPU time the process has used (kernel and user, seconds), or None."""
		handle = open_process(pid)
		if handle is None:
			return None
		try:
			times = [wt.FILETIME() for _ in range(4)]
			if not kernel32.GetProcessTimes(handle, *[ctypes.byref(t) for t in times]):
				return None
			return sum((t.dwHighDateTime << 32) | t.dwLowDateTime for t in times[2:]) / 1e7
		finally:
			kernel32.CloseHandle(handle)

	def all_pids():
		count = 4096
		while True:
			pids = (wt.DWORD * count)()
			needed = wt.DWORD()
			if not kernel32.K32EnumProcesses(pids, ctypes.sizeof(pids), ctypes.byref(needed)):
				raise OSError(ctypes.get_last_error(), "EnumProcesses failed")
			if needed.value < ctypes.sizeof(pids):
				return list(pids[:needed.value // ctypes.sizeof(wt.DWORD)])
			count *= 2

	def wait_exit(pid, seconds):
		"""True when the process `pid` ended within `seconds` (or is gone already)."""
		handle = open_process(pid, SYNCHRONIZE)
		if handle is None:
			return True
		try:
			return kernel32.WaitForSingleObject(handle, int(seconds * 1000)) == WAIT_OBJECT_0
		finally:
			kernel32.CloseHandle(handle)

	def kill(pid):
		handle = open_process(pid, PROCESS_TERMINATE)
		if handle is None:
			return
		try:
			kernel32.TerminateProcess(handle, 1)
		finally:
			kernel32.CloseHandle(handle)

	def listeners():
		"""[(pid, address, port)] of every listening TCP socket, IPv4 and IPv6."""
		result = []
		for family, row_size in ((2, 24), (23, 56)):  # AF_INET, AF_INET6
			size = wt.DWORD(0)
			buffer = None
			for _ in range(5):
				buffer = ctypes.create_string_buffer(max(size.value, 4))
				# TCP_TABLE_OWNER_PID_LISTENER
				status = iphlpapi.GetExtendedTcpTable(buffer, ctypes.byref(size), False, family, 3, 0)
				if status == 0:
					break
				if status != 122:  # ERROR_INSUFFICIENT_BUFFER
					raise OSError(status, "GetExtendedTcpTable failed")
			else:
				raise OSError(122, "GetExtendedTcpTable: the table kept growing")
			data = buffer.raw
			count = struct.unpack_from("<I", data, 0)[0]
			for i in range(count):
				row = 4 + i * row_size
				if family == 2:
					_, local, port, _, _, pid = struct.unpack_from("<IIIIII", data, row)
					address = str(ipaddress.IPv4Address(struct.pack("<I", local)))
				else:
					address = str(ipaddress.IPv6Address(data[row:row + 16]))
					port = struct.unpack_from("<I", data, row + 20)[0]
					pid = struct.unpack_from("<I", data, row + 52)[0]
				result.append((pid, address, ((port & 0xff) << 8) | ((port >> 8) & 0xff)))
		return result
else:
	def process_path(pid):
		try:
			return os.path.normcase(os.path.realpath("/proc/%d/exe" % pid)) if os.path.exists(
				"/proc/%d/exe" % pid) else None
		except OSError:
			return None

	def process_started(pid):
		try:
			with open("/proc/%d/stat" % pid, "r", encoding="ascii", errors="replace") as f:
				fields = f.read().rpartition(")")[2].split()
			return int(fields[19])  # starttime, field 22
		except (OSError, ValueError, IndexError):
			return None

	def process_cpu(pid):
		try:
			with open("/proc/%d/stat" % pid, "r", encoding="ascii", errors="replace") as f:
				fields = f.read().rpartition(")")[2].split()
			return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")  # utime, stime: fields 14, 15
		except (OSError, ValueError, IndexError):
			return None

	def all_pids():
		return [int(name) for name in os.listdir("/proc") if name.isdigit()]

	def alive(pid):
		try:
			with open("/proc/%d/stat" % pid, "r", encoding="ascii", errors="replace") as f:
				return f.read().rpartition(")")[2].split()[0] != "Z"
		except OSError:
			return False

	def wait_exit(pid, seconds):
		deadline = time.monotonic() + seconds
		while alive(pid):
			if time.monotonic() >= deadline:
				return False
			time.sleep(0.2)
		return True

	def kill(pid):
		try:
			os.kill(pid, signal.SIGKILL)
		except ProcessLookupError:
			pass

	def socket_inodes(pid):
		inodes = set()
		try:
			for fd in os.listdir("/proc/%d/fd" % pid):
				try:
					target = os.readlink("/proc/%d/fd/%s" % (pid, fd))
				except OSError:
					continue
				if target.startswith("socket:["):
					inodes.add(target[8:-1])
		except OSError:
			pass
		return inodes

	def listeners():
		rows = []
		for name, size in (("tcp", 4), ("tcp6", 16)):
			try:
				with open("/proc/net/" + name, "r", encoding="ascii") as f:
					lines = f.read().splitlines()[1:]
			except OSError:
				continue
			for line in lines:
				fields = line.split()
				if len(fields) < 10 or fields[3] != "0A":  # TCP_LISTEN
					continue
				address, port = fields[1].split(":")
				raw = bytes.fromhex(address)
				# the kernel prints each 32-bit word in host (little-endian) order
				raw = b"".join(raw[i:i + 4][::-1] for i in range(0, len(raw), 4))
				text = str(ipaddress.IPv4Address(raw) if size == 4 else ipaddress.IPv6Address(raw))
				rows.append((fields[9], text, int(port, 16)))
		owners = {}
		if rows:
			for pid in all_pids():
				for inode in socket_inodes(pid):
					owners.setdefault(inode, pid)
		return [(owners.get(inode, 0), address, port) for inode, address, port in rows]


def server_processes(exe):
	"""PIDs of the running processes whose executable is `exe`."""
	found = []
	for pid in all_pids():
		if pid == 0:
			continue
		path = process_path(pid)
		if path is not None and path == exe:
			found.append(pid)
	return found


def read_pid_file():
	"""(pid, creation time or None) from server.pid, or None."""
	try:
		with open(PID_FILE, "r", encoding="ascii") as f:
			lines = f.read().split()
		return int(lines[0]), (int(lines[1]) if len(lines) > 1 else None)
	except (OSError, ValueError, IndexError):
		return None


def remove_pid_file():
	try:
		os.remove(PID_FILE)
	except FileNotFoundError:
		pass


# --- start --------------------------------------------------------------------------------

def say(command, text):
	print("testserver %s: %s" % (command, text), flush=True)


def read_log():
	try:
		with open(LOG_FILE, "r", encoding="utf-8", errors="replace") as f:
			return f.read()
	except OSError:
		return ""


def wait_ready(poll, listening, log_text, cpu, timeout=START_TIMEOUT, stall=START_STALL, after_online=AFTER_ONLINE,
		clock=time.monotonic, sleep=time.sleep):
	"""Waits until a starting server is ready (issue #68): both ports listening and its world loaded.
	poll() is the process's exit code or None, listening() its listening sockets as "address:port",
	log_text() server.log's text, cpu() its CPU time in seconds (or None). Returns (state, detail):
	("ready", (listening, "online" or "unknown")), ("exited", code), ("bind", (server, port, code or None, the
	line)) as soon as server.log has a bind error (issue #95), ("stalled", text) or ("timeout", text)."""
	started = clock()
	last_progress = started
	seen = None
	online_since = None
	while True:
		code = poll()
		if code is not None:
			return "exited", code
		entries = sorted(listening())
		text = log_text()
		failed = bind_error(text)
		if failed is not None:
			return "bind", failed
		ports = {int(entry.rpartition(":")[2]) for entry in entries}
		both = LOGIN_PORT in ports and GAME_PORT in ports
		online = LOG_ONLINE in text
		if both and online:
			return "ready", (entries, "online")
		if both and LOG_EARLY not in text:
			return "ready", (entries, "unknown")
		now = clock()
		used = cpu()
		state = (len(text), tuple(entries))
		if seen is None or state != seen[0] or (used is not None and seen[1] is not None and used > seen[1]):
			seen = (state, used)
			last_progress = now
		elif used is not None and seen[1] is None:
			seen = (seen[0], used)
		missing = ", ".join(str(port) for port in (LOGIN_PORT, GAME_PORT) if port not in ports)
		if online:
			if online_since is None:
				online_since = now
			if now - online_since >= after_online:
				return "stalled", "server.log says %r, but nothing listens on %s %d s later" % (LOG_ONLINE, missing,
					after_online)
		elif now - last_progress >= stall:
			return "stalled", "no progress for %d s (server.log, the ports, CPU time; %s)" % (stall,
				"world not loaded" if not missing else "not listening on " + missing)
		if now - started >= timeout:
			return "timeout", "not ready after %d s (%s)" % (timeout, "world not loaded" if not missing
				else "not listening on " + missing)
		sleep(0.5)


def bind_error(text):
	"""The first bind error in server.log's text: (server, port, code or None, the line), or None (issue #95)."""
	match = BIND_ERROR.search(text)
	if match is None:
		return None
	begin = text.rfind("\n", 0, match.start()) + 1
	# the main thread's text may come first on that line (issue #95's run: ":: Initializing database threads &
	# queues... 2026-11-06 ... [ERR] ..."): the line from its time stamp
	stamped = list(LOG_STAMP.finditer(text, begin, match.start()))
	if stamped:
		begin = stamped[-1].start()
	end = text.find("\n", match.end())
	line = text[begin:end if end >= 0 else len(text)].strip()
	return match.group(1).lower(), int(match.group(2)), None if match.group(3) is None else int(match.group(3)), line


def excluded_ranges(text):
	"""[(first, last)] of `netsh int ipv4 show excludedportrange protocol=tcp`'s output."""
	return [(int(first), int(last)) for first, last in re.findall(r"^\s*(\d+)\s+(\d+)\b", text, re.MULTILINE)]


def windows_excluded_ranges():
	"""Windows' excluded TCP port ranges (IPv4 and IPv6), or None when netsh cannot tell (not Windows)."""
	if not WINDOWS:
		return None
	ranges = []
	for family in ("ipv4", "ipv6"):
		try:
			output = subprocess.run(["netsh", "int", family, "show", "excludedportrange", "protocol=tcp"],
				capture_output=True, text=True, timeout=30, stdin=subprocess.DEVNULL).stdout
		except (OSError, subprocess.SubprocessError):
			return None
		ranges += excluded_ranges(output or "")
	return ranges


def explain_bind(server, port, code, ranges):
	"""The text start prints for a bind error (issue #95): the code's meaning, and an excluded port range (from
	windows_excluded_ranges(); None: not known) that holds the port, whatever the code."""
	text = "the %s server could not bind port %d (%s)" % (server, port, "no error code in the line" if code is None
		else "error %d, %s" % (code, BIND_CODES.get(code, "see WinSock's error codes")))
	holding = [(first, last) for first, last in ranges or () if first <= port <= last]
	if holding:
		text += "; port %d is in Windows' excluded port range %s: free it there (an administrator's change, not" \
			" the tools')" % (port, ", ".join("%d-%d" % r for r in holding))
	elif ranges is not None:
		text += "; port %d is in no excluded port range" % port
	return text


def retry_allowed(kind, failures, retries, bind_retries):
	"""Whether start tries again after a failed attempt of `kind` ("bind" or "other"), given the failures so far
	{"bind": n, "other": m}, this one counted (issue #95): bind failures use --bind-retries first, then
	--retries, which the other failures use."""
	return failures["other"] + max(0, failures["bind"] - bind_retries) <= retries


def wait_online(poll, log_text, seconds, clock=time.monotonic, sleep=time.sleep):
	"""Waits up to `seconds` until server.log says "Server Online!" or the server exits; True when online."""
	deadline = clock() + seconds
	while LOG_ONLINE not in log_text():
		if poll() is not None or clock() >= deadline:
			return False
		sleep(0.5)
	return True


def stop_timeout(log_text):
	"""The seconds a failed attempt's server gets for its Ctrl+C before it is killed: SAVING_STOP once its world is
	loaded (it saves the map on the way out: never kill it in the middle, issue #137), else STALLED_STOP."""
	return SAVING_STOP if LOG_ONLINE in log_text else STALLED_STOP


def spawn(exe, server_dir, window):
	"""Starts the server (its output to server.log, its PID to server.pid); returns the Popen."""
	os.makedirs(STATE_DIR, exist_ok=True)
	options = {}
	if WINDOWS:
		# A process that ignores Ctrl+C passes that on to its children (SetConsoleCtrlHandler(NULL,
		# TRUE) is inherited); started from Git Bash, for one, the server would then ignore stop's
		# Ctrl+C. Clear the flag in this process before starting the server.
		kernel32.SetConsoleCtrlHandler(None, False)
		startup = subprocess.STARTUPINFO()
		startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
		startup.wShowWindow = 7 if window else 0  # SW_SHOWMINNOACTIVE, SW_HIDE
		options = {"creationflags": subprocess.CREATE_NEW_CONSOLE, "startupinfo": startup}
	else:
		# the same for SIGINT: an ignored signal stays ignored across exec
		signal.signal(signal.SIGINT, signal.SIG_DFL)
		options = {"start_new_session": True}
	with open(LOG_FILE, "wb") as log, open(ERR_FILE, "wb") as err:
		process = subprocess.Popen([exe], cwd=server_dir, stdin=subprocess.DEVNULL, stdout=log, stderr=err,
			**options)
	started = process_started(process.pid)
	with open(PID_FILE, "w", encoding="ascii", newline="\n") as f:
		f.write("%d\n%s\n" % (process.pid, "" if started is None else started))
	return process


def keep_attempt_log(attempt):
	"""server.log of a failed attempt as server-attempt<N>.log (replacing an older one)."""
	target = os.path.join(STATE_DIR, "server-attempt%d.log" % attempt)
	try:
		os.replace(LOG_FILE, target)
	except OSError:
		return LOG_FILE
	return target


def start(server_dir, timeout=START_TIMEOUT, window=False, stall=START_STALL, after_online=AFTER_ONLINE,
		retries=START_RETRIES, bind_retries=BIND_RETRIES):
	exe = os.path.join(server_dir, EXE_NAME)
	if not os.path.isfile(exe):
		say("start", "%s not found" % exe)
		return 1
	exe = os.path.abspath(exe)
	failures = {"bind": 0, "other": 0}
	attempt = 0
	while True:
		attempt += 1
		# before every attempt: a server the tools did not start, or a port someone else holds, is never touched
		running = server_processes(server_exe(server_dir))
		if running:
			say("start", "already running (PID %s)" % ", ".join(map(str, running)))
			return 1
		for port in (LOGIN_PORT, GAME_PORT):
			if any(p == port for _, _, p in listeners()):
				say("start", "port %d is already in use" % port)
				return 1
		process = spawn(exe, server_dir, window)
		say("start", "started PID %d, log %s (attempt %d)" % (process.pid, LOG_FILE, attempt))
		begun = time.monotonic()
		state, detail = wait_ready(process.poll,
			lambda: ["%s:%d" % (address, port) for pid, address, port in listeners()
				if pid == process.pid and port in (LOGIN_PORT, GAME_PORT)],
			read_log, lambda: process_cpu(process.pid), timeout, stall, after_online)
		if state == "ready":
			entries, world = detail
			say("start", "listening on %s" % ", ".join(entries))
			if world == "online":
				say("start", "world loaded (%r) after %.1f s" % (LOG_ONLINE, time.monotonic() - begun))
			else:
				say("start", "world: not shown in server.log (%r missing: the output does not reach the file as it"
					" is written); the ports alone count" % LOG_EARLY)
			return 0
		kind = "other"
		if state == "exited":
			say("start", "the server exited with code %d; see %s and %s" % (detail, LOG_FILE, ERR_FILE))
			remove_pid_file()
		elif state == "bind":
			kind = "bind"
			server, port, code, line = detail
			say("start", "bind error in server.log after %.1f s: %r" % (time.monotonic() - begun, line))
			say("start", explain_bind(server, port, code, windows_excluded_ranges()))
			# its Ctrl+C once the world is loaded: the usual shutdown, not one in the middle of the map's load
			online = wait_online(process.poll, read_log, after_online)
			say("start", "stopping PID %d (the one this start started)%s" % (process.pid, "" if online else
				", %r not said within %d s" % (LOG_ONLINE, after_online)))
			if process.poll() is not None:
				remove_pid_file()
			elif stop(server_dir, stop_timeout(read_log()), True) != 0:
				say("start", "could not stop PID %d: giving up" % process.pid)
				return 1
		else:
			say("start", "the server (PID %d) %s: %s; stopping it (the one this start started)" % (process.pid,
				"stalled" if state == "stalled" else "timed out", detail))
			if stop(server_dir, stop_timeout(read_log()), True) != 0:
				say("start", "could not stop PID %d: giving up" % process.pid)
				return 1
		kept = keep_attempt_log(attempt)
		failures[kind] += 1
		if not retry_allowed(kind, failures, retries, bind_retries):
			break
		say("start", "attempt %d failed (its log: %s): starting the server again" % (attempt, kept))
	say("start", "the server did not become ready in %d attempt%s (%d with a bind error; the last log: %s)" % (
		attempt, "" if attempt == 1 else "s", failures["bind"], kept))
	return 1


# --- stop ---------------------------------------------------------------------------------

def started_server(exe):
	"""The PID of the server start started, if it still runs; else None."""
	entry = read_pid_file()
	if entry is None:
		return None
	pid, started = entry
	if process_path(pid) != exe:
		return None
	if started is not None and process_started(pid) != started:
		return None
	return pid


def send_ctrl_c(pid):
	"""In a copy of this script started without a console (DETACHED_PROCESS): attach to the
	server's console, ignore Ctrl+C here, send it to every process on that console."""
	signal.signal(signal.SIGINT, signal.SIG_IGN)
	kernel32.FreeConsole()
	if not kernel32.AttachConsole(pid):
		return ctypes.get_last_error() or 1
	kernel32.SetConsoleCtrlHandler(None, True)
	ok = kernel32.GenerateConsoleCtrlEvent(0, 0)  # CTRL_C_EVENT, every process on the console
	kernel32.FreeConsole()
	return 0 if ok else 2


def stop(server_dir, timeout, force):
	exe = server_exe(server_dir)
	pid = started_server(exe)
	if pid is None:
		remove_pid_file()
		others = server_processes(exe)
		if others:
			say("stop", "a server these tools did not start is running (PID %s): not touched"
				% ", ".join(map(str, others)))
			return 1
		say("stop", "no server running")
		return 0

	if WINDOWS:
		DETACHED_PROCESS = 0x00000008
		code = subprocess.call([sys.executable, "-I", os.path.abspath(__file__), "_ctrl-c", str(pid)],
			stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
			creationflags=DETACHED_PROCESS)
		say("stop", "sent Ctrl+C to PID %d (sender exit code %d)" % (pid, code))
	else:
		os.kill(pid, signal.SIGINT)
		say("stop", "sent SIGINT to PID %d" % pid)

	if wait_exit(pid, timeout):
		say("stop", "the server exited")
		remove_pid_file()
		return 0
	if force:
		kill(pid)
		wait_exit(pid, 10)
		say("stop", "killed PID %d after %d s" % (pid, timeout))
		remove_pid_file()
		return 0
	say("stop", "PID %d still running after %d s (use --force to kill it)" % (pid, timeout))
	return 1


# --- the self-test (issue #68) -------------------------------------------------------------

SELF_TEST_LOG = (":: Initializing network subsystem... [done]", ":: Starting Login Server on port 7171... [done]",
	":: Initializing connections & network... [done]", ":: Loading map sectors... [done]",
	">> Zanera (Tibia 7.72) Server Online!")


def self_test(out=print):
	"""wait_ready on made-up servers that change as a made-up clock goes on (no server, no process). Returns
	the number of failed expectations."""
	failures = 0
	out("testserver start's wait (issue #68)")

	def check(title, ok):
		nonlocal failures
		failures += not ok
		out("  %-62s %s" % (title, "ok" if ok else "FAILED"))

	def run(script, timeout=900, stall=180, after_online=60, exits=None, log=SELF_TEST_LOG):
		"""wait_ready on a made-up server: script(t) -> (log lines, ports, cpu seconds) at made-up second t, the
		first lines of `log`; exits: the second it exits at. Returns (state, detail, seconds waited)."""
		now = [0.0]

		def at():
			lines, ports, cpu = script(now[0])
			return lines, ports, cpu

		state, detail = wait_ready(lambda: 0 if exits is not None and now[0] >= exits else None,
			lambda: ["127.0.0.1:%d" % port for port in at()[1]],
			lambda: "".join(line + "\n" for line in log[:at()[0]]),
			lambda: at()[2], timeout, stall, after_online, clock=lambda: now[0],
			sleep=lambda seconds: now.__setitem__(0, now[0] + seconds))
		return state, detail, now[0]

	both = (LOGIN_PORT, GAME_PORT)
	state, detail, waited = run(lambda t: (5, both, 1.0))
	check("listening and online at once: ready", state == "ready" and detail[1] == "online" and waited == 0)
	# issue #64's run: both ports listen at once, the world loads for 20 s (CPU busy), then online
	state, detail, waited = run(lambda t: (3 if t < 20 else 5, both, t))
	check("ports first, the world 20 s later: waited for the world", state == "ready" and detail[1] == "online"
		and waited == 20)
	# issue #75: a busy machine, a slow start that keeps using CPU time, 600 s to the world
	state, detail, waited = run(lambda t: (3 if t < 600 else 5, both, t / 100))
	check("a slow start with progress (600 s): ready, no stall", state == "ready" and waited == 600)
	# the log grows but no CPU time is read (None): the output is the progress
	state, detail, waited = run(lambda t: (min(5, 1 + int(t // 100)), both if t >= 150 else (), None))
	check("progress by the output alone: ready", state == "ready" and waited == 400)
	state, detail, waited = run(lambda t: (3, both, 2.0))
	check("loading without any progress: stalled after --stall", state == "stalled" and waited == 180
		and "no progress" in detail)
	# issue #88: "Server Online" said, neither port listening
	state, detail, waited = run(lambda t: (5, (), t))
	check("online but not listening: stalled after --after-online", state == "stalled" and waited == 60
		and "7171, 7172" in detail)
	state, detail, waited = run(lambda t: (5, (LOGIN_PORT,), t))
	check("online, only the login port: stalled, names 7172", state == "stalled" and detail.endswith(
		"nothing listens on 7172 60 s later"))
	state, detail, waited = run(lambda t: (3, both, t), timeout=900)
	check("progress that never ends: timeout after --timeout", state == "timeout" and waited == 900)
	state, detail, waited = run(lambda t: (3, both, t), exits=10)
	check("the server exits while it loads: exited", state == "exited" and waited == 10)
	state, detail, waited = run(lambda t: (0, both, t))
	check("a log not written as it goes: the ports alone", state == "ready" and detail[1] == "unknown")
	state, detail, waited = run(lambda t: (2 if t < 30 else 5, (LOGIN_PORT,) if t < 30 else both, t))
	check("the login port first, the game port with the world", state == "ready" and waited == 30)

	out("a bind error in server.log (issue #95)")
	login_error = "2026-11-06 10:00:01 [ERR] LoginServerWorker: Failed to bind Login Server on port 7171 (error 10049)"
	bind_log = SELF_TEST_LOG[:2] + (login_error,) + SELF_TEST_LOG[2:]
	# #88's and #92's starts: the login thread's bind fails, the server goes on, says online, listens on 7172 only
	state, detail, waited = run(lambda t: (2 if t < 1 else 6, (GAME_PORT,) if t >= 2 else (), t), log=bind_log)
	check("login bind error 10049: failed at once, port and code", state == "bind" and waited == 1
		and detail[:3] == ("login", 7171, 10049) and detail[3] == login_error)
	state, detail, waited = run(lambda t: (6, (GAME_PORT,), t), log=bind_log)
	check("the error with 'Server Online!' after it: still the bind", state == "bind" and waited == 0)
	game_log = SELF_TEST_LOG[:4] + ("LaunchServer: Failed to bind Game Server on port 7172.",) + SELF_TEST_LOG[4:]
	state, detail, waited = run(lambda t: (5, (LOGIN_PORT,), t), log=game_log)
	check("game bind error (no code): failed at once", state == "bind" and detail[:3] == ("game", 7172, None))
	state, detail, waited = run(lambda t: (6, both, 1.0), log=SELF_TEST_LOG[:2]
		+ ("Failed to listen on Login Server on port 7171",) + SELF_TEST_LOG[2:])
	check("another error line, both ports listening: ready", state == "ready")
	failed = bind_error(":: Initializing database threads & queues... " + login_error + "\n[done]\n")
	check("bind_error: the line from its time stamp (issue #95's run)", failed is not None and failed[3]
		== login_error)
	check("bind_error: no error in the made-up log", bind_error("".join(line + "\n" for line in SELF_TEST_LOG))
		is None)
	text = explain_bind("login", 7171, 10049, [(5357, 5357), (50000, 50059)])
	check("10049 named as the server's race, no excluded range", "error 10049, WSAEADDRNOTAVAIL" in text
		and "start-up race" in text and text.endswith("port 7171 is in no excluded port range"))
	text = explain_bind("login", 7171, 10013, [(7100, 7199)])
	check("10013 in an excluded range: the range named", "WSAEACCES" in text and "excluded port range 7100-7199"
		in text)
	text = explain_bind("game", 7172, None, None)
	check("no code, ranges unknown: said so, no range line", text == "the game server could not bind port 7172"
		" (no error code in the line)")
	netsh = ("\nProtocol tcp Port Exclusion Ranges\n\nStart Port    End Port      \n----------    --------      \n"
		"      5357        5357      \n     50000       50059     *\n     55777       55876      \n\n"
		"* - Administered port exclusions.\n")
	check("netsh's excluded port ranges parsed", excluded_ranges(netsh) == [(5357, 5357), (50000, 50059),
		(55777, 55876)])
	check("excluded_ranges of an empty output: none", excluded_ranges("") == [])

	def allowed(kinds, retries=1, bind_retries=3):
		"""retry_allowed after each failure in `kinds` ("b": bind, "o": other): the answers as a string."""
		counts = {"bind": 0, "other": 0}
		answers = ""
		for kind in kinds:
			counts["bind" if kind == "b" else "other"] += 1
			answers += "y" if retry_allowed("bind" if kind == "b" else "other", counts, retries, bind_retries) else "n"
		return answers

	check("retries: 3 bind failures, then --retries' 1 more", allowed("bbbbb") == "yyyyn")
	check("retries: other failures use --retries only", allowed("oo") == "yn")
	check("retries: a bind failure keeps the other's retry", allowed("bo") == "yy" and allowed("bbbbo")
		== "yyyyn")
	check("retries: --bind-retries 0 --retries 0: none", allowed("b", 0, 0) == "n")
	now = [0.0]
	lines = [""]

	def tick(seconds):
		now[0] += seconds
		if now[0] >= 3:
			lines[0] = "\n".join(SELF_TEST_LOG)

	check("wait_online: online after 3 s", wait_online(lambda: None, lambda: lines[0], 60, lambda: now[0], tick)
		and now[0] == 3)
	now[0], lines[0] = 0.0, ""
	check("wait_online: never online, gives up after the time", not wait_online(lambda: None, lambda: "", 60,
		lambda: now[0], tick) and now[0] == 60)

	# a failed attempt's stop (issue #137): a loaded world saves on Ctrl+C, so no kill after 30 s
	check("stop timeout: world not loaded, 30 s", stop_timeout("\n".join(SELF_TEST_LOG[:2])) == STALLED_STOP)
	check("stop timeout: world loaded, time to save", stop_timeout("\n".join(SELF_TEST_LOG)) == SAVING_STOP)
	out("testserver self test: %d failures" % failures)
	return failures


def main(argv):
	if len(argv) == 2 and argv[0] == "_ctrl-c":
		return send_ctrl_c(int(argv[1]))
	parser = argparse.ArgumentParser(description="Start and stop the local test server (notes/testserver.md).")
	commands = parser.add_subparsers(dest="command", required=True)
	start_parser = commands.add_parser("start", help="start the server and wait until it listens and its world is"
		" loaded")
	start_parser.add_argument("--server-dir", default=SERVER_DIR, help=SERVER_DIR_HELP)
	start_parser.add_argument("--timeout", type=int, default=START_TIMEOUT, help="seconds an attempt may take in all"
		" (default %(default)s)")
	start_parser.add_argument("--stall", type=int, default=START_STALL, help="seconds without progress (server.log,"
		" the ports, CPU time) before an attempt is given up (default %(default)s)")
	start_parser.add_argument("--after-online", type=int, default=AFTER_ONLINE, help="seconds from %r to both"
		" ports listening (default %%(default)s)" % LOG_ONLINE)
	start_parser.add_argument("--retries", type=int, default=START_RETRIES, help="starts again after a failed"
		" attempt, its server stopped (default %(default)s)")
	start_parser.add_argument("--bind-retries", type=int, default=BIND_RETRIES, help="more starts after a bind error"
		" in server.log, on top of --retries (default %(default)s; issue #95)")
	start_parser.add_argument("--window", action="store_true", help="show the server's console, minimised")
	stop_parser = commands.add_parser("stop", help="Ctrl+C to the server start started, and wait")
	stop_parser.add_argument("--server-dir", default=SERVER_DIR, help=SERVER_DIR_HELP)
	stop_parser.add_argument("--timeout", type=int, default=60, help="seconds to wait for the exit")
	stop_parser.add_argument("--force", action="store_true", help="kill it after --timeout")
	commands.add_parser("self-test", help="the start's wait on made-up servers (no server)")
	args = parser.parse_args(argv)
	if args.command == "self-test":
		return 1 if self_test() else 0
	if args.command == "start":
		return start(args.server_dir, args.timeout, args.window, args.stall, args.after_online, max(0, args.retries),
			max(0, args.bind_retries))
	return stop(args.server_dir, args.timeout, args.force)


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
