r"""Start and stop the local 7.72 test server (issue #18, notes/testserver.md); Python since issue #10.

Usage:
  python -I tools/testserver.py start [--server-dir DIR] [--timeout 900] [--stall 180] [--after-online 60]
                                      [--retries 1] [--bind-retries 3] [--window]
  python -I tools/testserver.py stop [--server-dir DIR] [--timeout SECONDS] [--force]
  python -I tools/testserver.py self-test      (no server: the start's wait on made-up servers, issue #68; the private
                                               copies on a made-up master, issue #146)
  python -I tools/testserver.py run [--keep] -- <command> [arguments]
  python -I tools/testserver.py cleanup | instances
  python -I tools/testserver.py checksums (--write FILE | --compare FILE) [--server-dir DIR]

A private server per run (Patella-Modern issue #146; notes/testserver.md, "A private server per run"): `run` makes a
private copy of the master server folder (C:\Tibia\gameserver, PATELLA_TESTSERVER) in PATELLA_TESTSERVER_RUNS (default
%TEMP%\patella-testservers\<run>\server): config.cfg with ports of its own, tibia.db with the world's port changed,
data\map, data\usr, data\dat and data\save copied, data\mon, data\npc and data\origmap hard links (the server never
writes them), data\log empty. It runs <command> with PATELLA_TESTSERVER_RUN=<run folder>; in that environment start and
stop run the master's tibia-server.exe (its firewall rule; nothing copied of it) with the copy as its working
directory, on the run's ports (login, game, and the proxy's pair: 4 ports from 7300-7499), and every tool takes the
copy's folder and ports from here. Afterwards the server is stopped (never killed: it saves) and the copy removed. A
copy whose run ended without that (killed) is cleaned up by the next run's start (`cleanup` does it at once).
The comparisons call isolate() before sandbox.redirect(), so each live run has its own copy. Without
PATELLA_TESTSERVER_RUN, start refuses: the master is never written by the tools (`start --master` runs the master
itself, the owner's own use). `checksums` writes or compares the SHA-256 of every file of a server folder.

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
import hashlib
import ipaddress
import json
import os
import re
import shutil
import signal
import socket
import sqlite3
import stat
import struct
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MASTER_STATE_DIR = os.path.join(ROOT, "build", "testserver")  # start --master's server.pid and server.log
# The test server's folder (issue #2): PATELLA_TESTSERVER, else C:\Tibia\gameserver: the master copy, which the tools
# only read since issue #146. The one place of that default: compare_traffic.py (and through it the other
# comparisons), proxy.py and compare_files.py take SERVER_DIR from here.
DEFAULT_SERVER_DIR = r"C:\Tibia\gameserver"
MASTER_DIR = os.environ.get("PATELLA_TESTSERVER") or DEFAULT_SERVER_DIR
EXE_NAME = "tibia-server.exe" if os.name == "nt" else "tibia-server"
WINDOWS = os.name == "nt"
DEFAULT_PORTS = (7171, 7172, 7271, 7272)  # the master's login and game port, the proxy's (tools/proxy.py)
# --- a private server per run (issue #146) ---
ENV_RUN = "PATELLA_TESTSERVER_RUN"  # the run's folder: instance.json, server.log, server.pid, server\ (the copy)
RUNS_DIR = os.environ.get("PATELLA_TESTSERVER_RUNS") or os.path.join(tempfile.gettempdir(), "patella-testservers")
INSTANCE_FILE = "instance.json"
RUN_NAME = re.compile(r"^\d{8}-\d{6}-\d+$")
PORT_FIRST, PORT_BLOCKS = 7300, 50  # 4 ports a run: login, game, the proxy's login and game; 7300-7499
LINKED = ("mon", "npc", "origmap")  # data\ folders the server only reads: hard links to the master's files
EMPTY = ("log",)  # data\ folders that start empty (the master's logs are 100 MB and only grow)
# The config.cfg keys that name files or folders: relative paths only, or the copy's server would use the master's
CONFIG_PATHS = ("data_path", "map_path", "monster_path", "npc_path", "user_path", "save_path", "log_path",
	"database_file", "rsa_key_file")


def load_instance(folder):
	"""instance.json of a run's folder as a dict (its paths made absolute), or None."""
	try:
		with open(os.path.join(folder, INSTANCE_FILE), encoding="utf-8") as f:
			value = json.load(f)
	except (OSError, ValueError):
		return None
	value["folder"] = os.path.abspath(folder)
	value["server"] = os.path.join(value["folder"], "server")
	return value


RUN_DIR = os.environ.get(ENV_RUN) or None
INSTANCE = load_instance(RUN_DIR) if RUN_DIR else None
if RUN_DIR and INSTANCE is None:
	raise RuntimeError("%s=%s: no %s there (the run's private server is gone)" % (ENV_RUN, RUN_DIR, INSTANCE_FILE))
# The server folder every tool uses: the run's private copy, else the master (read only).
SERVER_DIR = INSTANCE["server"] if INSTANCE else MASTER_DIR
LOGIN_PORT, GAME_PORT, PROXY_LOGIN_PORT, PROXY_GAME_PORT = (tuple(INSTANCE["ports"]) if INSTANCE else DEFAULT_PORTS)
STATE_DIR = INSTANCE["folder"] if INSTANCE else MASTER_STATE_DIR
SERVER_DIR_HELP = ("the server's folder (default %(default)s: the run's private copy, else PATELLA_TESTSERVER, else "
	+ DEFAULT_SERVER_DIR.replace("%", "%%") + ")")


def set_state(folder):
	"""The folder of server.pid, server.log and server.err that start and stop use (a run's, or the master's)."""
	global STATE_DIR, PID_FILE, LOG_FILE, ERR_FILE
	STATE_DIR = folder
	PID_FILE = os.path.join(folder, "server.pid")
	LOG_FILE = os.path.join(folder, "server.log")
	ERR_FILE = os.path.join(folder, "server.err")


set_state(STATE_DIR)
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
		clock=time.monotonic, sleep=time.sleep, ports=None):
	"""Waits until a starting server is ready (issue #68): both ports listening and its world loaded.
	poll() is the process's exit code or None, listening() its listening sockets as "address:port",
	log_text() server.log's text, cpu() its CPU time in seconds (or None). Returns (state, detail):
	("ready", (listening, "online" or "unknown")), ("exited", code), ("bind", (server, port, code or None, the
	line)) as soon as server.log has a bind error (issue #95), ("stalled", text) or ("timeout", text).
	ports: (login, game), default the run's (issue #146)."""
	login_port, game_port = ports or (LOGIN_PORT, GAME_PORT)
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
		both = login_port in ports and game_port in ports
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
		missing = ", ".join(str(port) for port in (login_port, game_port) if port not in ports)
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


LOG_MAP_LOADING = "Loading map sectors"


def stop_timeout(log_text):
	"""The seconds a server gets for its Ctrl+C before it is killed: SAVING_STOP once it has begun loading its map (a
	server that finishes loading saves the map on the way out: never kill it in the middle, issue #137; server.log
	can lag behind the server, so waiting for "Server Online!" alone let a kill cut a sector file again in #123),
	else STALLED_STOP."""
	return SAVING_STOP if (LOG_ONLINE in log_text or LOG_MAP_LOADING in log_text) else STALLED_STOP


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


def run_exe(server_dir):
	"""The executable start runs for server_dir: the master's for the run's private copy (issue #146: its own path, so
	Windows Firewall's rule for it holds and nothing of it is copied), else the folder's own."""
	if INSTANCE is not None and norm(server_dir) == norm(INSTANCE["server"]):
		return INSTANCE["exe"]
	return os.path.abspath(os.path.join(server_dir, EXE_NAME))


def norm(path):
	return os.path.normcase(os.path.abspath(path))


def running(server_dir=None):
	"""Whether the server of server_dir runs (compare_traffic.server_running): for the run's private copy the server
	its start started (server.pid; the master's executable runs other runs' servers too), else any process of the
	folder's executable."""
	server_dir = server_dir or SERVER_DIR
	exe = server_exe_path(run_exe(server_dir))
	if INSTANCE is not None and norm(server_dir) == norm(INSTANCE["server"]):
		return started_server(exe) is not None
	return bool(server_processes(exe))


def server_exe_path(exe):
	return os.path.normcase(os.path.abspath(exe) if WINDOWS else os.path.realpath(exe))


def start(server_dir, timeout=START_TIMEOUT, window=False, stall=START_STALL, after_online=AFTER_ONLINE,
		retries=START_RETRIES, bind_retries=BIND_RETRIES, master=False):
	private = INSTANCE is not None and norm(server_dir) == norm(INSTANCE["server"])
	if not private and not master:
		# issue #146: the master (C:\Tibia\gameserver) is never written by the tools
		say("start", "no private server for %s: a run's own copy comes with `python -I tools/testserver.py run -- "
			"<command>` (the comparisons make one themselves); --master starts the master folder's server itself "
			"(it writes that folder: the owner's own use, never the tools')" % server_dir)
		return 1
	exe = run_exe(server_dir)
	if not os.path.isfile(exe):
		say("start", "%s not found" % exe)
		return 1
	failures = {"bind": 0, "other": 0}
	attempt = 0
	while True:
		attempt += 1
		# before every attempt: a server the tools did not start, or a port someone else holds, is never touched
		running_now = [started_server(server_exe_path(exe))] if private else server_processes(server_exe(server_dir))
		running_now = [pid for pid in running_now if pid is not None]
		if running_now:
			say("start", "already running (PID %s)" % ", ".join(map(str, running_now)))
			return 1
		for port in (LOGIN_PORT, GAME_PORT):
			if any(p == port for _, _, p in listeners()):
				say("start", "port %d is already in use" % port)
				return 1
		process = spawn(exe, server_dir, window)
		say("start", "started PID %d, log %s (attempt %d)%s" % (process.pid, LOG_FILE, attempt, (", private copy %s,"
			" ports %d and %d" % (server_dir, LOGIN_PORT, GAME_PORT)) if private else ""))
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
			# its Ctrl+C once the world is loaded: the usual shutdown, not one in the middle of the map's load. Up to
			# --stall (180 s), not --after-online: a run's fresh private copy loads its map in 45 to 120 s (issue #146)
			loading = max(after_online, stall)
			online = wait_online(process.poll, read_log, loading)
			say("start", "stopping PID %d (the one this start started)%s" % (process.pid, "" if online else
				", %r not said within %d s" % (LOG_ONLINE, loading)))
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
	if not process_running(pid):  # ended, its handle still held (issue #146)
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
	private = INSTANCE is not None and norm(server_dir) == norm(INSTANCE["server"])
	exe = server_exe_path(run_exe(server_dir))
	pid = started_server(exe)
	if pid is None:
		remove_pid_file()
		# a run's copy: the master's executable also runs other runs' servers, none of them this run's
		others = [] if private else server_processes(exe)
		if others:
			say("stop", "a server these tools did not start is running (PID %s): not touched"
				% ", ".join(map(str, others)))
			return 1
		say("stop", "no server running")
		return 0
	return stop_pid(pid, timeout, force)


def stop_pid(pid, timeout, force):
	"""Ctrl+C (SIGINT) to the server `pid` that start started (server.pid of STATE_DIR), then the wait; with force it is
	killed after `timeout`. 0 when it is gone."""
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


# --- a private server per run (issue #146) -------------------------------------------------

def run_say(text):
	print("testserver run: %s" % text, flush=True)


class RunsLock:
	"""The lock of RUNS_DIR (held while a run picks its ports and writes its instance.json): a byte-range lock on
	RUNS_DIR\\lock, which Windows (or flock) releases when its process ends, killed or not."""

	def __init__(self, root=None, timeout=300):
		self.root = root or RUNS_DIR
		self.timeout = timeout
		self.file = None

	def __enter__(self):
		os.makedirs(self.root, exist_ok=True)
		self.file = open(os.path.join(self.root, "lock"), "a+b")
		deadline = time.monotonic() + self.timeout
		while True:
			try:
				if WINDOWS:
					import msvcrt
					self.file.seek(0)
					msvcrt.locking(self.file.fileno(), msvcrt.LK_NBLCK, 1)
				else:
					import fcntl
					fcntl.flock(self.file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
				return self
			except OSError:
				if time.monotonic() > deadline:
					self.file.close()
					raise RuntimeError("%s: locked for %d s by another run" % (os.path.join(self.root, "lock"),
						self.timeout))
				time.sleep(0.2)

	def __exit__(self, *exc):
		try:
			if WINDOWS:
				import msvcrt
				self.file.seek(0)
				msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
			else:
				import fcntl
				fcntl.flock(self.file.fileno(), fcntl.LOCK_UN)
		finally:
			self.file.close()


def is_reparse(entry_stat):
	"""A symbolic link or a junction (never followed into: a junction may lead into the master)."""
	return stat.S_ISLNK(entry_stat.st_mode) or bool(getattr(entry_stat, "st_file_attributes", 0) & 0x400)


def scan(root, dirs=None):
	"""{relative path: (size, mtime_ns)} of every file under root, and the relative paths of the links and junctions
	found (not followed); the folders' relative paths are appended to dirs (a list) when given."""
	files = {}
	links = []
	pending = [""]
	while pending:
		rel = pending.pop()
		with os.scandir(os.path.join(root, rel)) as entries:
			for entry in entries:
				path = os.path.join(rel, entry.name)
				info = entry.stat(follow_symlinks=False)
				if is_reparse(info):
					links.append(path)
				elif stat.S_ISDIR(info.st_mode):
					pending.append(path)
					if dirs is not None:
						dirs.append(path)
				else:
					files[path] = (info.st_size, info.st_mtime_ns)
	return files, links


def state_changes(before, after):
	"""The relative paths that differ between two scans' files (changed, added, removed), sorted."""
	return sorted(path for path in set(before) | set(after) if before.get(path) != after.get(path))


def config_value(text, key):
	"""The value of an uncommented `key = value` line of config.cfg (quotes removed), or None; ValueError when the key
	is there more than once."""
	values = re.findall(r'(?m)^[ \t]*%s[ \t]*=[ \t]*("([^"\n]*)"|[^\s#]+)' % re.escape(key), text)
	if len(values) > 1:
		raise ValueError("config.cfg has %s %d times" % (key, len(values)))
	if not values:
		return None
	return values[0][1] if values[0][0].startswith('"') else values[0][0]


def private_config(text, login_port, game_port):
	"""config.cfg for a private copy: its own login and game port. Refuses (ValueError) a configuration whose files
	would not be the copy's own: an absolute or `..` path, a database other than SQLite (a MySQL or PostgreSQL server
	is shared)."""
	for key in CONFIG_PATHS:
		value = config_value(text, key)
		if value is None:
			continue
		parts = re.split(r"[\\/]", value)
		if os.path.isabs(value) or re.match(r"^[A-Za-z]:", value) or ".." in parts:
			raise ValueError("config.cfg's %s = %r is not inside the server's folder: the private copy's server would use"
				" the master's files" % (key, value))
	db_type = config_value(text, "db_type")
	if db_type is not None and db_type.lower() != "sqlite":
		raise ValueError("config.cfg's db_type = %r: only SQLite (tibia.db, copied) keeps a run's database its own"
			% db_type)
	for key, port in (("login_port", login_port), ("game_port", game_port)):
		text, count = re.subn(r"(?m)^([ \t]*%s[ \t]*=[ \t]*)\d+" % key, lambda m: m.group(1) + str(port), text)
		if count != 1:
			raise ValueError("config.cfg has %d %s lines (one expected)" % (count, key))
	return text


def private_database(path, game_port):
	"""The copy's tibia.db: every world's Port (the character list's game port) set to the run's."""
	connection = sqlite3.connect(path)
	try:
		count = connection.execute("UPDATE Worlds SET Port = ?", (game_port,)).rowcount
		connection.commit()
	finally:
		connection.close()
	if count < 1:
		raise ValueError("%s: no world in the Worlds table" % path)
	return count


def copy_plan(master, files, folders=()):
	"""[(action, relative path)] of a private copy, from scan(master)'s files: "link" (a hard link: data\\mon, npc,
	origmap, which the server only reads), "copy" (everything the server or the tools may write: config.cfg, tibia.db,
	data\\map, usr, dat, save), "dir" (data\\log, empty). Left out: the executable and its DLLs (the master's run),
	tibia.db-shm (rebuilt from the WAL) and every other folder than data (backups)."""
	plan = []
	dirs = set()
	for rel in sorted(files):
		parts = rel.split(os.sep)
		if len(parts) == 1:
			name = parts[0].lower()
			if name == EXE_NAME.lower() or (WINDOWS and name.endswith(".dll")) or name.endswith("-shm"):
				continue
			plan.append(("copy", rel))
		elif parts[0].lower() == "data":
			sub = parts[1].lower() if len(parts) > 2 else None
			if sub in EMPTY:
				dirs.add(os.path.join(parts[0], parts[1]))
			else:
				plan.append(("link" if sub in LINKED else "copy", rel))
	for rel in folders:  # the empty ones too (data\save)
		parts = rel.split(os.sep)
		if parts[0].lower() == "data" and not (len(parts) > 2 and parts[1].lower() in EMPTY):
			dirs.add(rel)
	for name in EMPTY:  # data\log even when the master has none
		if os.path.isdir(os.path.join(master, "data")):
			dirs.add(os.path.join("data", name))
	return [("dir", rel) for rel in sorted(dirs)] + plan


def copy_master(master, target, plan, threads=16):
	"""Carries out copy_plan into target; returns {"copy": (files, bytes), "link": (files, bytes)}. A hard link that
	cannot be made (another volume) is a copy."""
	made = set()
	for action, rel in plan:
		folder = os.path.join(target, rel) if action == "dir" else os.path.dirname(os.path.join(target, rel))
		if folder not in made:
			os.makedirs(folder, exist_ok=True)
			made.add(folder)
	totals = {"copy": [0, 0], "link": [0, 0]}

	def one(item):
		action, rel = item
		source, destination = os.path.join(master, rel), os.path.join(target, rel)
		if action == "link":
			try:
				os.link(source, destination)
				return "link", os.path.getsize(destination)
			except OSError:
				pass
		shutil.copy2(source, destination)
		return "copy", os.path.getsize(destination)

	with ThreadPoolExecutor(threads) as pool:
		for kind, size in pool.map(one, [item for item in plan if item[0] != "dir"]):
			totals[kind][0] += 1
			totals[kind][1] += size
	return {kind: tuple(value) for kind, value in totals.items()}


def port_free(port, listening):
	"""Nothing listens on the port and it binds on 0.0.0.0 (the game server binds all interfaces)."""
	if port in listening:
		return False
	s = socket.socket()
	try:
		s.bind(("0.0.0.0", port))
		return True
	except OSError:
		return False
	finally:
		s.close()


def pick_ports(used, listening, excluded, first=PORT_FIRST, blocks=PORT_BLOCKS, start=0):
	"""The run's 4 ports (login, game, proxy login, proxy game): the first block of 4 from `first` (beginning at block
	`start`, round) of which no port is another run's (`used`), listened on, in an excluded range or unbindable."""
	for i in range(blocks):
		base = first + 4 * ((start + i) % blocks)
		ports = (base, base + 1, base + 2, base + 3)
		if any(port in used for port in ports):
			continue
		if any(lo <= port <= hi for port in ports for lo, hi in excluded or ()):
			continue
		if all(port_free(port, listening) for port in ports):
			return ports
	raise RuntimeError("no free block of 4 ports in %d-%d" % (first, first + 4 * blocks - 1))


def run_folders(root=None):
	root = root or RUNS_DIR
	try:
		names = sorted(os.listdir(root))
	except FileNotFoundError:
		return []
	return [os.path.join(root, name) for name in names if RUN_NAME.match(name) and os.path.isdir(os.path.join(root,
		name))]


def write_instance(folder, value):
	value = {k: v for k, v in value.items() if k not in ("folder", "server")}
	tmp = os.path.join(folder, INSTANCE_FILE + ".tmp")
	with open(tmp, "w", encoding="utf-8", newline="\n") as f:
		json.dump(value, f, indent=1)
	os.replace(tmp, os.path.join(folder, INSTANCE_FILE))


def process_running(pid, started=None):
	"""Whether the process `pid` (with that creation time, when given) runs. A process that ended can still be opened
	while someone holds a handle to it (its parent's), with its creation time: its exit is what counts."""
	if not pid:
		return False
	if started is not None and process_started(pid) != started:
		return False
	if WINDOWS:
		handle = open_process(pid, SYNCHRONIZE)
		if handle is None:
			return False
		try:
			return kernel32.WaitForSingleObject(handle, 0) != WAIT_OBJECT_0
		finally:
			kernel32.CloseHandle(handle)
	return alive(pid)


def owner_alive(instance):
	owner = instance.get("owner") or [0, None]
	return process_running(owner[0], owner[1])


def create_instance(master=None, root=None, say=run_say, owner=None, attempts=3):
	"""A run's private copy of the master (see the module's text); returns its instance (load_instance)."""
	master = os.path.abspath(master or MASTER_DIR)
	root = root or RUNS_DIR
	exe = os.path.join(master, EXE_NAME)
	if not os.path.isfile(exe):
		raise RuntimeError("%s not found" % exe)
	with open(os.path.join(master, "config.cfg"), encoding="utf-8", errors="surrogateescape") as f:
		config = f.read()
	private_config(config, 1, 2)  # refused before anything is made
	owner = owner or [os.getpid(), process_started(os.getpid())]
	with RunsLock(root):
		used = set()
		for folder in run_folders(root):
			other = load_instance(folder)
			if other is not None:
				used.update(other.get("ports", ()))
		ports = pick_ports(used, {port for _, _, port in listeners()}, windows_excluded_ranges(),
			start=os.getpid() % PORT_BLOCKS)
		name = time.strftime("%Y%m%d-%H%M%S") + "-%d" % os.getpid()
		folder = os.path.join(root, name)
		os.makedirs(os.path.join(folder, "server"))
		instance = {"name": name, "master": master, "exe": exe, "ports": list(ports), "owner": owner,
			"created": time.strftime("%Y-%m-%d %H:%M:%S"), "state": "copying"}
		write_instance(folder, instance)
	instance = load_instance(folder)
	try:
		return fill_instance(instance, master, config, ports, say, attempts)
	except BaseException:
		remove_tree(folder, root)
		raise


def fill_instance(instance, master, config, ports, say, attempts):
	"""create_instance's copy, config.cfg and tibia.db."""
	folder = instance["folder"]
	begun = time.monotonic()
	for attempt in range(1, attempts + 1):
		folders = []
		before, links = scan(master, folders)
		plan = copy_plan(master, before, folders)
		totals = copy_master(master, instance["server"], plan)
		after, _ = scan(master)
		changed = state_changes(before, after)
		if not changed:
			break
		# someone else's server saved the master meanwhile: a copy of half a save is no world
		say("the master changed while it was copied (%d files, %s...): copying again" % (len(changed), changed[0]))
		remove_tree(instance["server"], folder)
		os.makedirs(instance["server"])
	else:
		raise RuntimeError("the master kept changing while it was copied (%d attempts)" % attempts)
	with open(os.path.join(instance["server"], "config.cfg"), "w", encoding="utf-8", errors="surrogateescape",
			newline="") as f:
		f.write(private_config(config, ports[0], ports[1]))
	worlds = private_database(os.path.join(instance["server"], "tibia.db"), ports[1])
	instance["state"] = "ready"
	instance["copied"] = {"files": totals["copy"][0], "bytes": totals["copy"][1], "linked": totals["link"][0],
		"linked_bytes": totals["link"][1], "seconds": round(time.monotonic() - begun, 1), "worlds": worlds}
	write_instance(folder, instance)
	instance["master_state"] = after
	say("private server %s: ports %d (login), %d (game), %d and %d (the proxy); %d files copied (%.0f MB), %d hard"
		" links (%.0f MB), %.1f s%s" % (instance["server"], ports[0], ports[1], ports[2], ports[3], totals["copy"][0],
		totals["copy"][1] / 1e6, totals["link"][0], totals["link"][1] / 1e6, time.monotonic() - begun,
		"; not followed: %s" % ", ".join(links) if links else ""))
	return instance


def remove_tree(folder, root):
	"""Deletes folder (under root) file by file, never through a link or junction (a junction's own entry is removed,
	what it leads to is not: it may be the master). A hard link's removal leaves the master's file as it is."""
	folder, root = os.path.abspath(folder), os.path.abspath(root)
	if not norm(folder).startswith(norm(root).rstrip("\\/") + os.sep):
		raise RuntimeError("%s is not under %s: not removed" % (folder, root))

	def remove(path):
		with os.scandir(path) as it:
			entries = list(it)
		for entry in entries:
			info = entry.stat(follow_symlinks=False)
			if is_reparse(info):
				try:
					os.rmdir(entry.path)  # a junction or directory link: the link only
				except OSError:
					os.remove(entry.path)
			elif stat.S_ISDIR(info.st_mode):
				remove(entry.path)
			else:
				try:
					os.remove(entry.path)
				except PermissionError:
					if info.st_nlink > 1:  # a hard link's attributes are the master's file's: never changed
						raise
					os.chmod(entry.path, stat.S_IWRITE)
					os.remove(entry.path)
		os.rmdir(path)

	if os.path.isdir(folder):
		remove(folder)


def stop_instance(instance, say=run_say):
	"""Stops the server a run's start started and left running (Ctrl+C; the wait for its save, never a kill: issue
	#137). True when none runs afterwards."""
	previous = STATE_DIR
	set_state(instance["folder"])
	try:
		pid = started_server(server_exe_path(instance["exe"]))
		if pid is None:
			return True
		say("the run's server (PID %d) still runs: stopping it (Ctrl+C, waiting for its save)" % pid)
		return stop_pid(pid, stop_timeout(read_log()), False) == 0
	finally:
		set_state(previous)


def keep_logs(instance):
	"""The run's server.log (and server.err, attempt logs, instance.json) in build\\testserver\\runs\\<run>."""
	target = os.path.join(MASTER_STATE_DIR, "runs", instance["name"])
	os.makedirs(target, exist_ok=True)
	for name in os.listdir(instance["folder"]):
		if name.startswith("server") and (name.endswith(".log") or name.endswith(".err")) or name == INSTANCE_FILE:
			try:
				shutil.copyfile(os.path.join(instance["folder"], name), os.path.join(target, name))
			except OSError:
				pass
	return target


def claim(folder):
	"""cleanup.claim in a stale run's folder: True when this process may clean it (no other living cleaner)."""
	path = os.path.join(folder, "cleanup.claim")
	me = "%d %s" % (os.getpid(), process_started(os.getpid()))
	try:
		fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
	except FileExistsError:
		try:
			with open(path, encoding="ascii") as f:
				pid, started = f.read().split()
			if process_running(int(pid), None if started == "None" else int(started)):
				return False
		except (OSError, ValueError):
			pass
		with open(path, "w", encoding="ascii") as f:
			f.write(me)
		return True
	with os.fdopen(fd, "w", encoding="ascii") as f:
		f.write(me)
	return True


def sweep(root=None, say=run_say, keep=None):
	"""Cleans up the runs whose owner is gone (a run killed midway): their server stopped (Ctrl+C, its save waited
	for), their logs kept, their folder removed. A folder without instance.json (its creation died) older than 10
	minutes is removed too. Returns the number of folders removed."""
	root = root or RUNS_DIR
	removed = 0
	for folder in run_folders(root):
		instance = load_instance(folder)
		if instance is None:
			try:
				age = time.time() - os.path.getmtime(folder)
			except OSError:
				continue
			if age > 600:
				remove_tree(folder, root)
				removed += 1
			continue
		if owner_alive(instance) or not claim(folder):
			continue
		say("cleaning up %s (its run ended without it: owner PID %s gone)" % (folder, instance.get("owner", ["?"])[0]))
		if not stop_instance(instance, say):
			say("its server still runs: left for the next cleanup")
			try:
				os.remove(os.path.join(folder, "cleanup.claim"))
			except OSError:
				pass
			continue
		if keep is not False:
			keep_logs(instance)
		remove_tree(folder, root)
		removed += 1
	return removed


def run_private(command, keep=False, say=run_say):
	"""Runs command with a private server of its own (PATELLA_TESTSERVER_RUN); afterwards stops it and removes the
	copy. Returns the command's exit code."""
	sweep(say=say)
	instance = create_instance(say=say)
	env = dict(os.environ)
	env[ENV_RUN] = instance["folder"]
	code = 2
	try:
		code = subprocess.call(command, env=env)
	finally:
		stopped = stop_instance(instance, say)
		logs = keep_logs(instance)
		if keep:
			say("--keep: %s stays (python -I tools/testserver.py cleanup removes it)" % instance["folder"])
			instance["owner"] = [0, None]
			write_instance(instance["folder"], instance)
		elif stopped:
			begun = time.monotonic()
			remove_tree(instance["folder"], RUNS_DIR)
			say("private server removed (%.1f s); its logs: %s" % (time.monotonic() - begun, logs))
		else:
			say("the run's server did not stop: %s left for the next run's cleanup" % instance["folder"])
		after, _ = scan(instance["master"])
		changed = state_changes(instance["master_state"], after)
		if changed:
			say("WARNING: the master %s changed during the run (%d files: %s); this run's server did not write it,"
				" another user of the master did" % (instance["master"], len(changed), ", ".join(changed[:10])))
		else:
			say("the master is unchanged (%d files, size and time)" % len(after))
	return code


def isolate(skip=()):
	"""In a live tool's `if __name__ == "__main__":` block, before sandbox.redirect: runs the same command line again
	with a private server of its own (run_private) and exits with its exit code; returns at once inside such a run,
	inside Windows Sandbox (the host's run made it), or with an argument of skip (no server needed)."""
	if RUN_DIR or os.environ.get("PATELLA_SANDBOX_IO") or any(a in skip for a in sys.argv[1:]):
		return
	try:
		code = run_private([sys.executable, "-I", os.path.abspath(sys.argv[0])] + sys.argv[1:])
	except (RuntimeError, ValueError, OSError, sqlite3.Error) as e:
		print("testserver.py: no private server: %s" % e, file=sys.stderr)
		code = 2
	sys.exit(code)


def file_hash(path):
	digest = hashlib.sha256()
	with open(path, "rb") as f:
		for chunk in iter(lambda: f.read(1 << 20), b""):
			digest.update(chunk)
	return digest.hexdigest()


def checksums(folder, threads=8):
	"""{relative path: (size, sha256)} of every file under folder (links and junctions not followed)."""
	files, _ = scan(folder)
	names = sorted(files)
	with ThreadPoolExecutor(threads) as pool:
		hashes = list(pool.map(lambda rel: file_hash(os.path.join(folder, rel)), names))
	return {rel: (files[rel][0], digest) for rel, digest in zip(names, hashes)}


def write_checksums(folder, path):
	sums = checksums(folder)
	with open(path, "w", encoding="utf-8", newline="\n") as f:
		f.write("# sha256\tsize\tpath (%s, %s)\n" % (folder, time.strftime("%Y-%m-%d %H:%M:%S")))
		for rel, (size, digest) in sorted(sums.items()):
			f.write("%s\t%d\t%s\n" % (digest, size, rel.replace(os.sep, "/")))
	return sums


def read_checksums(path):
	sums = {}
	with open(path, encoding="utf-8") as f:
		for line in f:
			if line.startswith("#") or not line.strip():
				continue
			digest, size, rel = line.rstrip("\n").split("\t")
			sums[rel.replace("/", os.sep)] = (int(size), digest)
	return sums


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
			sleep=lambda seconds: now.__setitem__(0, now[0] + seconds), ports=(7171, 7172))
		return state, detail, now[0]

	login_port, game_port = 7171, 7172  # the made-up servers' (not the run's)
	both = (login_port, game_port)
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
	state, detail, waited = run(lambda t: (5, (login_port,), t))
	check("online, only the login port: stalled, names 7172", state == "stalled" and detail.endswith(
		"nothing listens on 7172 60 s later"))
	state, detail, waited = run(lambda t: (3, both, t), timeout=900)
	check("progress that never ends: timeout after --timeout", state == "timeout" and waited == 900)
	state, detail, waited = run(lambda t: (3, both, t), exits=10)
	check("the server exits while it loads: exited", state == "exited" and waited == 10)
	state, detail, waited = run(lambda t: (0, both, t))
	check("a log not written as it goes: the ports alone", state == "ready" and detail[1] == "unknown")
	state, detail, waited = run(lambda t: (2 if t < 30 else 5, (login_port,) if t < 30 else both, t))
	check("the login port first, the game port with the world", state == "ready" and waited == 30)

	out("a bind error in server.log (issue #95)")
	login_error = "2026-11-06 10:00:01 [ERR] LoginServerWorker: Failed to bind Login Server on port 7171 (error 10049)"
	bind_log = SELF_TEST_LOG[:2] + (login_error,) + SELF_TEST_LOG[2:]
	# #88's and #92's starts: the login thread's bind fails, the server goes on, says online, listens on 7172 only
	state, detail, waited = run(lambda t: (2 if t < 1 else 6, (game_port,) if t >= 2 else (), t), log=bind_log)
	check("login bind error 10049: failed at once, port and code", state == "bind" and waited == 1
		and detail[:3] == ("login", 7171, 10049) and detail[3] == login_error)
	state, detail, waited = run(lambda t: (6, (game_port,), t), log=bind_log)
	check("the error with 'Server Online!' after it: still the bind", state == "bind" and waited == 0)
	game_log = SELF_TEST_LOG[:4] + ("LaunchServer: Failed to bind Game Server on port 7172.",) + SELF_TEST_LOG[4:]
	state, detail, waited = run(lambda t: (5, (login_port,), t), log=game_log)
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
	check("stop timeout: map loading, time to save", stop_timeout("\n".join(SELF_TEST_LOG[:4])) == SAVING_STOP)
	out("testserver self test: %d failures" % failures)
	return failures


def private_self_test(out=print):
	"""The private copies (issue #146) on a made-up master in a temporary folder: the copy's plan, config.cfg and
	tibia.db, the hard links, the ports, the cleanup of a run whose owner is gone (and one through a junction that
	must not reach the master), the master unchanged. No server. Returns the number of failed expectations."""
	failures = 0
	out("the private copies (issue #146)")

	def check(title, ok):
		nonlocal failures
		failures += not ok
		out("  %-62s %s" % (title, "ok" if ok else "FAILED"))

	base = tempfile.mkdtemp(prefix="testserver-self-test-")
	try:
		master = os.path.join(base, "master")
		root = os.path.join(base, "runs")
		files = {EXE_NAME: b"exe", "z.dll": b"dll", "tibia.pem": b"pem", "schema.sql": b"sql",
			os.path.join("backup-2026-11-02", "tibia.db"): b"old", os.path.join("data", "map", "1000-1000-07.sec"): b"map",
			os.path.join("data", "usr", "74", "15146574.usr"): b"usr", os.path.join("data", "dat", "owners.dat"): b"own",
			os.path.join("data", "mon", "rat.mon"): b"rat", os.path.join("data", "npc", "cipfried.npc"): b"npc",
			os.path.join("data", "origmap", "1000-1000-07.sec"): b"orig", os.path.join("data", "log", "game.log"): b"log"}
		for rel, data in files.items():
			os.makedirs(os.path.dirname(os.path.join(master, rel)) or master, exist_ok=True)
			with open(os.path.join(master, rel), "wb") as f:
				f.write(data)
		os.makedirs(os.path.join(master, "data", "save"))
		config = ('login_port = 7171\ngame_port = 7172\nbind_address = "127.0.0.1"\ndata_path = "data"\n'
			'# map_path = "C:/elsewhere"\ndb_type = "sqlite"\ndatabase_file = "tibia.db"\n')
		with open(os.path.join(master, "config.cfg"), "w", encoding="utf-8", newline="\n") as f:
			f.write(config)
		connection = sqlite3.connect(os.path.join(master, "tibia.db"))
		connection.execute("CREATE TABLE Worlds (WorldID INTEGER, Name TEXT, Host TEXT, Port INTEGER)")
		connection.execute("INSERT INTO Worlds VALUES (1, 'Zanera', 'localhost', 7172)")
		connection.commit()
		connection.close()
		before = checksums(master)

		# the configuration's checks
		check("config: ports replaced, the rest kept", private_config(config, 7300, 7301) == config.replace("7171",
			"7300").replace("7172", "7301"))
		for bad, what in (('data_path = "C:/Tibia/gameserver/data"', "an absolute data_path"), ('user_path = '
				'"../gameserver/data/usr"', "a .. user_path"), ('db_type = "mysql"', "a MySQL database")):
			try:
				private_config(config + bad + "\n", 1, 2)
				refused = False
			except ValueError:
				refused = True
			check("config refused: %s" % what, refused)
		try:
			private_config(config.replace("game_port = 7172\n", ""), 1, 2)
			refused = False
		except ValueError:
			refused = True
		check("config refused: no game_port line", refused)

		# the ports
		check("ports: the first free block", pick_ports(set(), set(), []) == (7300, 7301, 7302, 7303))
		check("ports: another run's block skipped", pick_ports({7301}, set(), []) == (7304, 7305, 7306, 7307))
		check("ports: a listened port's block skipped", pick_ports(set(), {7302}, []) == (7304, 7305, 7306, 7307))
		check("ports: an excluded range skipped", pick_ports(set(), set(), [(7290, 7305)]) == (7308, 7309, 7310, 7311))
		check("ports: from the start block, round", pick_ports(set(), set(), [], start=49) == (7496, 7497, 7498, 7499))
		held = socket.socket()
		held.bind(("0.0.0.0", 0))
		port = held.getsockname()[1]
		check("ports: a port bound elsewhere is not free", not port_free(port, set()))
		held.close()

		# a run's copy
		instance = create_instance(master, root, say=lambda text: None)
		copy = instance["server"]
		check("instance: 4 ports, in instance.json", load_instance(instance["folder"])["ports"] == instance["ports"]
			and len(set(instance["ports"])) == 4)
		check("copy: no executable or DLL (the master's runs)", not os.path.exists(os.path.join(copy, EXE_NAME))
			and not os.path.exists(os.path.join(copy, "z.dll")) and instance["exe"] == os.path.join(master, EXE_NAME))
		check("copy: no backup folder", not os.path.exists(os.path.join(copy, "backup-2026-11-02")))
		with open(os.path.join(copy, "config.cfg"), encoding="utf-8") as f:
			text = f.read()
		check("copy: config.cfg with the run's ports", config_value(text, "login_port") == str(instance["ports"][0])
			and config_value(text, "game_port") == str(instance["ports"][1]))
		connection = sqlite3.connect(os.path.join(copy, "tibia.db"))
		port = connection.execute("SELECT Port FROM Worlds").fetchone()[0]
		connection.close()
		check("copy: tibia.db's world on the run's game port", port == instance["ports"][1])
		same = lambda rel: os.path.samefile(os.path.join(master, rel), os.path.join(copy, rel))  # noqa: E731
		check("copy: data\\mon, npc, origmap hard links", all(same(os.path.join("data", d, n)) for d, n in (("mon",
			"rat.mon"), ("npc", "cipfried.npc"), ("origmap", "1000-1000-07.sec"))))
		check("copy: data\\map, usr, dat copies (never links)", not any(same(os.path.join("data", d, n)) for d, n in
			(("map", "1000-1000-07.sec"), ("usr", os.path.join("74", "15146574.usr")), ("dat", "owners.dat"))))
		check("copy: data\\log empty, data\\save there", os.listdir(os.path.join(copy, "data", "log")) == []
			and os.path.isdir(os.path.join(copy, "data", "save")))
		with open(os.path.join(copy, "data", "map", "1000-1000-07.sec"), "wb") as f:
			f.write(b"saved")  # the copy's server saves its map
		with open(os.path.join(copy, "data", "usr", "74", "15146574.usr"), "wb") as f:
			f.write(b"restored")  # a tool restores a player file
		check("a write to the copy leaves the master as it was", checksums(master) == before)
		check("a living owner's run is not cleaned up", sweep(root, say=lambda text: None, keep=False) == 0
			and os.path.isdir(instance["folder"]))

		# an owner that ended while its parent still holds a handle to it (the killed run of #146's proof)
		ended = subprocess.Popen([sys.executable, "-I", "-c", "import time; time.sleep(1)"])
		started = process_started(ended.pid)
		check("an owner that runs: alive", owner_alive({"owner": [ended.pid, started]}))
		ended.wait()
		check("an owner that ended, its handle still held: gone", not owner_alive({"owner": [ended.pid, started]}))
		del ended
		# a run killed midway: its owner gone; a junction in it leads into the master
		instance["owner"] = [0, None]
		write_instance(instance["folder"], instance)
		junction = os.path.join(instance["folder"], "junction")
		made = False
		if WINDOWS:
			try:
				import _winapi
				_winapi.CreateJunction(os.path.join(master, "data"), junction)
				made = True
			except (ImportError, AttributeError, OSError):
				pass
		else:
			os.symlink(os.path.join(master, "data"), junction)
			made = True
		removed = sweep(root, say=lambda text: None, keep=False)
		check("a run whose owner is gone: removed by the next sweep", removed == 1
			and not os.path.exists(instance["folder"]))
		check("the master unchanged after the copy, the writes and the cleanup%s" % ("" if made else
			" (no junction made)"), checksums(master) == before)
		try:
			remove_tree(master, root)
			refused = False
		except RuntimeError:
			refused = True
		check("remove_tree refuses a folder outside the runs' folder", refused and os.path.isdir(master))
	finally:
		shutil.rmtree(base, ignore_errors=True)
	out("private copies self test: %d failures" % failures)
	return failures


def main(argv):
	# issue #147: inside Windows Sandbox the server stays on the host: start and stop run there (tools/sandbox.py)
	sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
	import sandbox
	forwarded = sandbox.forward_testserver(argv)
	if forwarded is not None:
		return forwarded
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
	start_parser.add_argument("--master", action="store_true", help="outside a run (no %s): start the master folder's"
		" server itself, which writes that folder (the owner's own use; the tools never do, issue #146)" % ENV_RUN)
	stop_parser = commands.add_parser("stop", help="Ctrl+C to the server start started, and wait")
	stop_parser.add_argument("--server-dir", default=SERVER_DIR, help=SERVER_DIR_HELP)
	stop_parser.add_argument("--timeout", type=int, default=None, help="seconds to wait for the exit (default: %d once the world is loaded, since it saves the map on the way out; else %d)" % (SAVING_STOP, STALLED_STOP))
	stop_parser.add_argument("--force", action="store_true", help="kill it after --timeout")
	commands.add_parser("self-test", help="the start's wait on made-up servers, the private copies on a made-up master"
		" (no server)")
	run_parser = commands.add_parser("run", help="run a command with a private server of its own (issue #146)")
	run_parser.add_argument("--keep", action="store_true", help="keep the copy afterwards (cleanup removes it)")
	run_parser.add_argument("argv", nargs=argparse.REMAINDER, metavar="command", help="-- <command>"
		" [arguments]")
	commands.add_parser("cleanup", help="stop and remove the private servers whose run is gone")
	commands.add_parser("instances", help="list the private servers")
	sums_parser = commands.add_parser("checksums", help="SHA-256 of every file of a server folder")
	sums_parser.add_argument("--server-dir", default=MASTER_DIR, help="the folder (default %(default)s: the master)")
	sums_group = sums_parser.add_mutually_exclusive_group(required=True)
	sums_group.add_argument("--write", help="write them to this file")
	sums_group.add_argument("--compare", help="compare with this file (exit 1 on a difference)")
	args = parser.parse_args(argv)
	if args.command == "self-test":
		return 1 if self_test() + private_self_test() else 0
	if args.command == "run":
		command = args.argv[1:] if args.argv[:1] == ["--"] else args.argv
		if not command:
			parser.error("no command given")
		if RUN_DIR:
			parser.error("already inside a run (%s=%s)" % (ENV_RUN, RUN_DIR))
		return run_private(command, args.keep)
	if args.command == "cleanup":
		run_say("%d private server%s removed" % ((lambda n: (n, "" if n == 1 else "s"))(sweep())))
		return 0
	if args.command == "instances":
		for folder in run_folders():
			instance = load_instance(folder)
			if instance is None:
				print("%s: no %s" % (folder, INSTANCE_FILE))
				continue
			set_state(folder)
			server = started_server(server_exe_path(instance["exe"]))
			print("%s: ports %s, %s, owner PID %s %s, server %s" % (folder, "/".join(map(str, instance["ports"])),
				instance.get("state"), instance["owner"][0], "running" if owner_alive(instance) else "gone",
				"PID %d" % server if server else "not running"))
		return 0
	if args.command == "checksums":
		folder = os.path.abspath(args.server_dir)
		if args.write:
			sums = write_checksums(folder, args.write)
			print("testserver checksums: %d files, %.0f MB of %s written to %s" % (len(sums), sum(v[0] for v in
				sums.values()) / 1e6, folder, args.write))
			return 0
		old = read_checksums(args.compare)
		new = checksums(folder)
		changed = sorted(rel for rel in set(old) | set(new) if old.get(rel) != new.get(rel))
		for rel in changed[:50]:
			print("  %s: %s -> %s" % (rel, old.get(rel, "missing"), new.get(rel, "missing")))
		print("testserver checksums: %d files of %s compared with %s: %s" % (len(new), folder, args.compare,
			"identical" if not changed else "%d differ" % len(changed)))
		return 1 if changed else 0
	if args.command == "start":
		return start(args.server_dir, args.timeout, args.window, args.stall, args.after_online, max(0, args.retries),
			max(0, args.bind_retries), args.master)
	# A server with its world loaded saves the map on Ctrl+C, which can take minutes on a busy machine: wait for it
	# rather than report a failed stop (issue #141's finding; #137 for the kill that cut a sector file).
	timeout = args.timeout if args.timeout is not None else stop_timeout(read_log())
	return stop(args.server_dir, timeout, args.force)


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
