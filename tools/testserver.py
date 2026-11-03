r"""Start and stop the local 7.72 test server (issue #18, notes/testserver.md); Python since issue #10.

Usage:
  python -I tools/testserver.py start [--server-dir DIR] [--timeout 300] [--window]
  python -I tools/testserver.py stop [--server-dir DIR] [--timeout 60] [--force]

start runs tibia-server.exe (tibia-server elsewhere) from its own folder (default C:\Tibia\gameserver,
where config.cfg, tibia.db and tibia.pem are found relative to the working directory) in a hidden
console of its own (--window: minimised), writes its output to build\testserver\server.log (stderr:
server.err; both overwritten at each start) and its PID to build\testserver\server.pid, and waits
until it listens on the login port (7171) and the game port (7172). It refuses to start when a
tibia-server from that folder already runs or anything listens on 7171 or 7172. An inherited "ignore
Ctrl+C" flag (a shell such as Git Bash passes it on) is cleared before the start, so that stop works.
Exit code 0 when the server is up, 1 when it is already running, failed to start, exited, or did not
listen within --timeout seconds.

stop sends Ctrl+C to the server's console (the shutdown it documents: it stops the login server,
logs all players out and saves, closes the database) and waits for it to exit; on Linux SIGINT. It
stops only the server start started: the process in build\testserver\server.pid, if it is still that
process (the same executable, the same creation time). A server the tools did not start (the user's
own, say) is never touched. The Ctrl+C is sent by a separate copy of this script that has no console
of its own, attaches to the server's and ignores the event itself. With --force, a server that has
not exited after --timeout seconds is killed (no save). Exit code 0 when the server the tools started
is not running afterwards, 1 otherwise (also when a server the tools did not start is running).

The printed lines start with "testserver start:" / "testserver stop:"; when the server listens,
start prints "listening on <address>:<port>, ..." (tools/compare_traffic.py looks for 0.0.0.0:7171
there, the server's start-up race).
"""

import argparse
import ctypes
import ipaddress
import os
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
SERVER_DIR = r"C:\Tibia\gameserver"
EXE_NAME = "tibia-server.exe" if os.name == "nt" else "tibia-server"
LOGIN_PORT, GAME_PORT = 7171, 7172
WINDOWS = os.name == "nt"


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


def start(server_dir, timeout, window):
	exe = os.path.join(server_dir, EXE_NAME)
	if not os.path.isfile(exe):
		say("start", "%s not found" % exe)
		return 1
	exe = os.path.abspath(exe)
	running = server_processes(server_exe(server_dir))
	if running:
		say("start", "already running (PID %s)" % ", ".join(map(str, running)))
		return 1
	for port in (LOGIN_PORT, GAME_PORT):
		if any(p == port for _, _, p in listeners()):
			say("start", "port %d is already in use" % port)
			return 1
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
	say("start", "started PID %d, log %s" % (process.pid, LOG_FILE))

	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		code = process.poll()
		if code is not None:
			say("start", "the server exited with code %d; see %s and %s" % (code, LOG_FILE, ERR_FILE))
			remove_pid_file()
			return 1
		listening = ["%s:%d" % (address, port) for pid, address, port in listeners()
			if pid == process.pid and port in (LOGIN_PORT, GAME_PORT)]
		if (any(entry.endswith(":%d" % LOGIN_PORT) for entry in listening)
				and any(entry.endswith(":%d" % GAME_PORT) for entry in listening)):
			say("start", "listening on %s" % ", ".join(listening))
			return 0
		time.sleep(0.5)
	say("start", "not listening on %d and %d after %d s (still running, PID %d); see %s"
		% (LOGIN_PORT, GAME_PORT, timeout, process.pid, LOG_FILE))
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


def main(argv):
	if len(argv) == 2 and argv[0] == "_ctrl-c":
		return send_ctrl_c(int(argv[1]))
	parser = argparse.ArgumentParser(description="Start and stop the local test server (notes/testserver.md).")
	commands = parser.add_subparsers(dest="command", required=True)
	start_parser = commands.add_parser("start", help="start the server and wait until it listens")
	start_parser.add_argument("--server-dir", default=SERVER_DIR)
	start_parser.add_argument("--timeout", type=int, default=300, help="seconds to wait for the ports")
	start_parser.add_argument("--window", action="store_true", help="show the server's console, minimised")
	stop_parser = commands.add_parser("stop", help="Ctrl+C to the server start started, and wait")
	stop_parser.add_argument("--server-dir", default=SERVER_DIR)
	stop_parser.add_argument("--timeout", type=int, default=60, help="seconds to wait for the exit")
	stop_parser.add_argument("--force", action="store_true", help="kill it after --timeout")
	args = parser.parse_args(argv)
	if args.command == "start":
		return start(args.server_dir, args.timeout, args.window)
	return stop(args.server_dir, args.timeout, args.force)


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
