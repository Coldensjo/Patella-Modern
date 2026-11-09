r"""Trials of the two starts that failed now and then in Windows Sandbox (Patella-Modern issue #161).

  python -I tools/sandbox_trial.py login [--count N] [--out DIR] [--broken]
  python -I tools/sandbox_trial.py frame [--count N] [--out DIR] [--pause SECONDS]
  python -I tools/sandbox_trial.py self-test

login: what compare_traffic.py's live sessions do up to the session: a private test server (testserver.isolate), a
client under test at its start screen (Tibia.exe, as in the live run), then the helper's start, login and game entry
(session_client.login, wait_in_game), its logout. Every 20 ms it reads the TCP table; the table's rows for the helper
and for the run's game port, and when each new state was first seen, go to <out>\login.jsonl.

--broken: a client whose game connection is refused (a proxy that does not listen on its game port): every login must
fail, the proof that a login that is really broken still fails.

frame: compare_rendering.py's start of Tibia.exe under patcap.dll, --count times with --pause seconds between them, the
first frame's time or the failure, and for a start that shows no frame within compare_rendering's wait the state of its
main thread (where it is, whether its window answers, the top of its stack).

Both run inside Windows Sandbox, one Sandbox at a time, as the comparisons do; --desktop is not offered.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import json
import os
import socket
import struct
import sys
import threading
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_rendering as cr  # noqa: E402
import compare_traffic as ct  # noqa: E402
import login_client as lc  # noqa: E402
import proxy as px  # noqa: E402
import sandbox  # noqa: E402
import session_client as sc  # noqa: E402
import testserver  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TRIAL = os.path.join(ROOT, "build", "sandbox-trial")
STATES = {1: "CLOSED", 2: "LISTEN", 3: "SYN_SENT", 4: "SYN_RCVD", 5: "ESTABLISHED", 6: "FIN_WAIT1", 7: "FIN_WAIT2",
	8: "CLOSE_WAIT", 9: "CLOSING", 10: "LAST_ACK", 11: "TIME_WAIT", 12: "DELETE_TCB"}


def log(text):
	lc.log(text)


def tcp_rows():
	"""Every row of the IPv4 TCP table: (state, local address, local port, remote address, remote port, PID)."""
	size = wt.DWORD(0)
	lc.iphlpapi.GetExtendedTcpTable(None, ctypes.byref(size), False, 2, 5, 0)
	buffer = ctypes.create_string_buffer(size.value + 4096)
	size = wt.DWORD(len(buffer))
	if lc.iphlpapi.GetExtendedTcpTable(buffer, ctypes.byref(size), False, 2, 5, 0) != 0:
		return []
	rows = []
	for i in range(struct.unpack_from("<I", buffer, 0)[0]):
		state, laddr, lport, raddr, rport, owner = struct.unpack_from("<6I", buffer, 4 + 24 * i)
		rows.append((state, socket.inet_ntoa(struct.pack("<I", laddr)), socket.ntohs(lport & 0xffff),
			socket.inet_ntoa(struct.pack("<I", raddr)), socket.ntohs(rport & 0xffff), owner))
	return rows


class Trace(threading.Thread):
	"""The TCP table every 20 ms: each row of the watched PIDs or the watched port, when it was first seen and when
	its state changed. Times are seconds after t0 (mark())."""

	def __init__(self, port):
		super().__init__(daemon=True)
		self.port = port
		self.pids = set()
		self.t0 = time.monotonic()
		self.events = []
		self.seen = {}
		self.stop_flag = threading.Event()

	def mark(self):
		self.t0 = time.monotonic()
		self.events.append({"t": 0.0, "mark": True})

	def run(self):
		while not self.stop_flag.is_set():
			now = round(time.monotonic() - self.t0, 3)
			rows = tcp_rows()
			current = {}
			for state, laddr, lport, raddr, rport, owner in rows:
				if owner in self.pids or rport == self.port or lport == self.port:
					current[(laddr, lport, raddr, rport, owner)] = state
			for key, state in current.items():
				if self.seen.get(key) != state:
					self.events.append({"t": now, "state": STATES.get(state, state), "local": "%s:%d" % key[:2],
						"remote": "%s:%d" % key[2:4], "pid": key[4]})
					self.seen[key] = state
			for key in [k for k in self.seen if k not in current]:
				self.events.append({"t": now, "gone": True, "local": "%s:%d" % key[:2], "remote": "%s:%d" % key[2:4],
					"pid": key[4]})
				del self.seen[key]
			self.stop_flag.wait(0.02)

	def finish(self):
		self.stop_flag.set()
		self.join()
		return self.events


def table_trial(args):
	"""Known sockets against the TCP table's state column: a listener and both ends of an accepted connection, read
	args.count times. Prints how often each socket read as each state."""
	listener = socket.socket()
	listener.bind(("127.0.0.1", 0))
	listener.listen(4)
	client = socket.create_connection(listener.getsockname())
	server, _ = listener.accept()
	known = {"listener (LISTEN)": listener.getsockname() + ("0.0.0.0", 0),
		"client end (ESTABLISHED)": client.getsockname() + client.getpeername(),
		"server end (ESTABLISHED)": server.getsockname() + server.getpeername()}
	counts = {name: {} for name in known}
	missing = {name: 0 for name in known}
	for _ in range(args.count):
		rows = {(r[1], r[2], r[3], r[4]): r[0] for r in tcp_rows() if r[5] == os.getpid()}
		for name, key in known.items():
			if key in rows:
				state = STATES.get(rows[key], rows[key])
				counts[name][state] = counts[name].get(state, 0) + 1
			else:
				missing[name] += 1
		time.sleep(0.01)
	for name in known:
		log("%s: %s; row missing %d of %d" % (name, counts[name], missing[name], args.count))
	return 0


class DiagnosedDriver(sc.Driver):
	"""A driver that says where a client that failed to start is, before it is killed."""

	diagnoses = None

	def failed_start(self, error):
		if self.proc is not None and self.proc.poll() is None:
			entry = {"error": str(error), "diagnosis": diagnose(self.proc.pid, self.hwnd)}
			self.diagnoses = (self.diagnoses or []) + [entry]
			log("failed start: %s" % json.dumps(entry)[:3000])
		return super().failed_start(error)


def login_trial(args):
	os.makedirs(args.out, exist_ok=True)
	ct.set_folder(TRIAL)
	ct.setup()
	players = ct.snapshot_players()
	results = []
	path = os.path.join(args.out, "login.jsonl")
	ct.start_server()
	log("the server runs; the game port is %d" % px.SERVER_GAME_PORT)
	try:
		with open(path, "a", encoding="utf-8") as out:
			for number in range(1, args.count + 1):
				result = one_login(number)
				results.append(result)
				out.write(json.dumps(result) + "\n")
				out.flush()
				log("login %d: %s" % (number, "ok" if result["ok"] else "FAILED: " + result["error"]))
	finally:
		ct.server_script("stop")
		ct.restore_players(players)
	failed = [r["number"] for r in results if not r["ok"]]
	log("%d of %d logins failed%s" % (len(failed), len(results), ": %s" % failed if failed else ""))
	return 1 if failed else 0


def broken_login(args):
	"""A login that is really broken must still fail: the client under test logs in through a proxy that does not
	listen on its game port, so that the game connection is refused."""
	os.makedirs(args.out, exist_ok=True)
	ct.set_folder(TRIAL)
	ct.setup()
	players = ct.snapshot_players()
	ct.start_server()
	failures = 0
	try:
		for number in range(1, args.count + 1):
			ct.clean_run_dir("orig")
			ct.reset_cfg("orig")
			driver = sc.Driver(ct.run_dir("orig"), "Tibia.exe")
			proxy = px.Proxy(os.path.join(args.out, "broken-%d.jsonl" % number), px.load_private_key(ct.PEM), log=log,
				refuse=("game",))
			started = time.monotonic()
			try:
				with proxy:
					driver.start()
					sc.login(driver, lambda name: None, sc.CHARACTER, proxy=proxy)
				failures += 1
				log("broken login %d: login() PASSED, a real failure was hidden" % number)
			except sc.SessionError as e:
				log("broken login %d: failed as it must, after %.1f s: %s" % (number, time.monotonic() - started, e))
			finally:
				driver.kill()
	finally:
		ct.server_script("stop")
		ct.restore_players(players)
	log("%d of %d broken logins were hidden" % (failures, args.count))
	return 1 if failures else 0


def self_test():
	"""wait_game_connection on made-up clients, no Sandbox, no client: which readings count as a connection."""
	failures = []

	class Fake:
		game_port = 7172
		proc = type("Proc", (), {"pid": 0})()
		log = staticmethod(lambda text: None)

		def __init__(self, established=False, row=False, player=False):
			self.established, self.row, self.in_game = established, row, player

		def connected(self, port):
			return self.established

		def holds(self, port):
			return self.row

		def player(self):
			if self.in_game == "unreadable":
				raise OSError("a torn reading")
			return {"pos": [1, 1, 7]} if self.in_game else None

	cases = [("established", Fake(established=True), True),
		("a row in a wrong state and the player: the Sandbox's reading (#161)", Fake(row=True, player=True), True),
		("no row, no player: a closed port", Fake(), False),
		("a row, no player: a connection that never got the game answer", Fake(row=True), False),
		("a row, the player unreadable", Fake(row=True, player="unreadable"), False),
		("the player, no row: left from an earlier session", Fake(player=True), False)]
	for name, fake, expected in cases:
		try:
			sc.wait_game_connection(fake, timeout=0.3)
			got = True
		except sc.SessionError:
			got = False
		print("%s %s" % ("ok  " if got == expected else "FAIL", name))
		if got != expected:
			failures.append(name)
	print("sandbox_trial self test: %d failures" % len(failures))
	return 1 if failures else 0


def one_login(number):
	ct.clean_run_dir("orig")
	ct.reset_cfg("orig")
	ct.clean_run_dir("helper")
	ct.reset_cfg("helper")
	under_test = DiagnosedDriver(ct.run_dir("orig"), "Tibia.exe")
	helper = DiagnosedDriver(ct.run_dir("helper"), "Tibia.exe", None, game_port=px.SERVER_GAME_PORT)
	trace = Trace(px.SERVER_GAME_PORT)
	trace.start()
	result = {"number": number, "ok": False, "error": ""}
	try:
		under_test.start()
		helper.start(["gamemaster"])
		trace.pids.add(helper.proc.pid)
		result["helper_pid"] = helper.proc.pid
		trace.mark()
		started = time.monotonic()
		try:
			sc.login(helper, lambda name: None, sc.TRADE_PARTNER)
			result["login_seconds"] = round(time.monotonic() - started, 3)
			sc.wait_in_game(helper)
			result["in_game_seconds"] = round(time.monotonic() - started, 3)
			result["ok"] = True
		except sc.SessionError as e:
			result["error"] = str(e)
			result["failed_after"] = round(time.monotonic() - started, 3)
			result["connections_at_failure"] = lc.tcp_connections(helper.proc.pid)
			# what the client itself shows in the next seconds (the tool's wait failed; has the client got in?)
			try:
				result["client_connection_state"] = helper.mem.var("ConnectionState")
				result["client_player"] = bool(helper.player())
			except Exception as read:  # noqa: BLE001 - a diagnosis, the failure itself is already in the result
				result["client_read"] = repr(read)
			time.sleep(5)
			try:
				result["client_connection_state_5s_later"] = helper.mem.var("ConnectionState")
				result["client_player_5s_later"] = bool(helper.player())
			except Exception as read:  # noqa: BLE001
				result["client_read_5s_later"] = repr(read)
			result["connections_5s_later"] = lc.tcp_connections(helper.proc.pid)
		if result["ok"]:
			ct.helper_logout(helper)
			if helper.proc.poll() is None:
				helper.kill()
	finally:
		helper.kill()
		under_test.kill()
		events = trace.finish()
		result["trace"] = events
	return result


def frame_trial(args):
	os.makedirs(args.out, exist_ok=True)
	cr.setup()
	path = os.path.join(args.out, "frame.jsonl")
	failed = 0
	with open(path, "a", encoding="utf-8") as out:
		for number in range(1, args.count + 1):
			if number > 1 and args.pause:
				time.sleep(args.pause)
			result = one_start(number, args.out)
			out.write(json.dumps(result) + "\n")
			out.flush()
			failed += not result["ok"]
			log("start %d: %s" % (number, "first frame after %d ms" % result["first_frame_ms"] if result["ok"]
				else "NO FRAME: " + result["error"]))
	log("%d of %d starts showed no frame" % (failed, args.count))
	return 1 if failed else 0


def one_start(number, out):
	label = "orig"
	ct.clean_run_dir(label, keep=("patcap.dll",) + cr.CPU_DRIVER_FILES)
	with open(os.path.join(ct.run_dir(label), "Tibia.cfg"), "w", encoding="ascii", newline="\r\n") as f:
		f.write(cr.cfg_for("vulkan"))
	driver = cr.Driver(ct.run_dir(label), "Tibia.exe", None, env=cr.client_env("vulkan", "orig"))
	driver.refuse_other_start = False  # counted here, not retried
	result = {"number": number, "ok": False}
	started = time.monotonic()
	try:
		try:
			driver.start_once()
			result["ok"] = True
			result["start_seconds"] = round(time.monotonic() - started, 3)
			facts = driver.cap.d3d9()
			result["first_frame_ms"] = facts["FirstPresentTime"]
			result["create_device_ms"] = facts["DeviceTime"]
		except cr.FirstFrameError as e:
			result["error"] = str(e)
			result["diagnosis"] = diagnose(driver.proc.pid, driver.hwnd)
			time.sleep(30)
			result["frames_after_60s"] = driver.cap.get("Frames")
			if not result["frames_after_60s"]:
				result["diagnosis_60s"] = diagnose(driver.proc.pid, driver.hwnd)
		except Exception as e:  # noqa: BLE001 - one start's failure of another kind is that start's result
			result["error"] = "%s: %s" % (type(e).__name__, e)
			if driver.proc is not None and driver.proc.poll() is None:
				result["diagnosis"] = diagnose(driver.proc.pid, driver.hwnd)
		log_path = os.path.join(ct.run_dir(label), "patcap.log")
		if os.path.isfile(log_path):
			with open(log_path, encoding="utf-8", errors="replace") as f:
				result["patcap_log"] = f.read()[:4000]
	finally:
		driver.kill()
		if getattr(driver, "cap", None) is not None:
			driver.cap.close()
	return result


# --- where a client's main thread is -----------------------------------------------------------------------------

TH32CS_SNAPTHREAD, TH32CS_SNAPMODULE, TH32CS_SNAPMODULE32 = 4, 8, 0x10
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.OpenThread.restype = wt.HANDLE
kernel32.OpenProcess.restype = wt.HANDLE


class THREADENTRY32(ctypes.Structure):
	_fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD), ("th32OwnerProcessID", wt.DWORD),
		("tpBasePri", ctypes.c_long), ("tpDeltaPri", ctypes.c_long), ("dwFlags", wt.DWORD)]


class MODULEENTRY32W(ctypes.Structure):
	_fields_ = [("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD), ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
		("ProccntUsage", wt.DWORD), ("modBaseAddr", ctypes.c_void_p), ("modBaseSize", wt.DWORD), ("hModule", wt.HMODULE),
		("szModule", ctypes.c_wchar * 256), ("szExePath", ctypes.c_wchar * 260)]


class WOW64_CONTEXT(ctypes.Structure):
	_fields_ = [("ContextFlags", wt.DWORD), ("Dr", wt.DWORD * 6), ("FloatSave", ctypes.c_byte * 112),
		("SegGs", wt.DWORD), ("SegFs", wt.DWORD), ("SegEs", wt.DWORD), ("SegDs", wt.DWORD), ("Edi", wt.DWORD), ("Esi", wt.DWORD),
		("Ebx", wt.DWORD), ("Edx", wt.DWORD), ("Ecx", wt.DWORD), ("Eax", wt.DWORD), ("Ebp", wt.DWORD), ("Eip", wt.DWORD),
		("SegCs", wt.DWORD), ("EFlags", wt.DWORD), ("Esp", wt.DWORD), ("SegSs", wt.DWORD), ("Extended", ctypes.c_byte * 512)]


def threads(pid):
	snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
	entry = THREADENTRY32()
	entry.dwSize = ctypes.sizeof(entry)
	found = []
	more = kernel32.Thread32First(snapshot, ctypes.byref(entry))
	while more:
		if entry.th32OwnerProcessID == pid:
			found.append(entry.th32ThreadID)
		more = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
	kernel32.CloseHandle(snapshot)
	return found


def modules(pid):
	snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
	entry = MODULEENTRY32W()
	entry.dwSize = ctypes.sizeof(entry)
	found = []
	more = kernel32.Module32FirstW(snapshot, ctypes.byref(entry))
	while more:
		found.append((entry.modBaseAddr or 0, entry.modBaseSize, entry.szModule))
		more = kernel32.Module32NextW(snapshot, ctypes.byref(entry))
	kernel32.CloseHandle(snapshot)
	return found


def name_of(address, mods):
	for base, size, name in mods:
		if base <= address < base + size:
			return "%s+0x%x" % (name, address - base)
	return None


def diagnose(pid, hwnd):
	"""The state of the client's threads: whether its window answers, each thread's instruction pointer and the code
	addresses on the top of its stack (a thread's stack scan: more than a call chain, each entry a return address or
	stale data)."""
	result = {"hung": bool(user32.IsHungAppWindow(hwnd)), "threads": []}
	process = kernel32.OpenProcess(0x0410, False, pid)  # QUERY_INFORMATION | VM_READ
	mods = modules(pid)
	for tid in threads(pid):
		handle = kernel32.OpenThread(0x0042 | 0x0008 | 0x0040, False, tid)  # SUSPEND_RESUME, GET_CONTEXT, QUERY
		if not handle:
			continue
		entry = {"tid": tid}
		if kernel32.Wow64SuspendThread(handle) != 0xffffffff:
			try:
				context = WOW64_CONTEXT()
				context.ContextFlags = 0x10003  # WOW64_CONTEXT_CONTROL | INTEGER
				if kernel32.Wow64GetThreadContext(handle, ctypes.byref(context)):
					entry["eip"] = name_of(context.Eip, mods) or hex(context.Eip)
					size = 0x1800
					buffer = ctypes.create_string_buffer(size)
					read = ctypes.c_size_t()
					if kernel32.ReadProcessMemory(process, ctypes.c_void_p(context.Esp), buffer, size, ctypes.byref(read)):
						stack = []
						for i in range(0, read.value - 3, 4):
							name = name_of(struct.unpack_from("<I", buffer, i)[0], mods)
							if name is not None:
								stack.append(name)
						entry["stack"] = stack[:30]
			finally:
				kernel32.ResumeThread(handle)
		kernel32.CloseHandle(handle)
		result["threads"].append(entry)
	kernel32.CloseHandle(process)
	return result


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("what", choices=("login", "frame", "table", "self-test"))
	parser.add_argument("--count", type=int, default=10)
	parser.add_argument("--out", default=os.path.join(TRIAL, "result"))
	parser.add_argument("--pause", type=float, default=0.0)
	parser.add_argument("--broken", action="store_true")
	args = parser.parse_args()
	args.out = os.path.abspath(args.out)
	if args.what == "self-test":
		return self_test()
	if args.what == "table":
		return table_trial(args)
	if args.what == "login":
		return broken_login(args) if args.broken else login_trial(args)
	return frame_trial(args)


if __name__ == "__main__":
	testserver.isolate(skip=("frame", "table", "self-test", "--host", "-h", "--help"))
	if "self-test" in sys.argv:
		pass
	elif "--host" in sys.argv:
		sys.argv.remove("--host")
	elif "table" in sys.argv:
		sandbox.redirect(writable=[TRIAL])
	elif "login" in sys.argv:
		sandbox.redirect(writable=[TRIAL])
	else:
		if sandbox.wanted(("-h", "--help")):
			cr.build_capture("capture-x86")
		sandbox.redirect(writable=[TRIAL, cr.RENDER], display=cr.FULLSCREEN_MODE[:2])
	try:
		sys.exit(main())
	except (ct.RunError, cr.RunError, sc.SessionError) as e:
		traceback.print_exc()
		print("sandbox_trial.py: %s" % e, file=sys.stderr)
		sys.exit(1)
