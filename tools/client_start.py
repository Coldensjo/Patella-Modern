r"""Another client's window and a refused start (Patella-Modern issue #148).

Tibia.exe (and the product Patella.exe) refuse to start beside another client: after the frame window,
InitInstance counts every top-level window of the desktop whose title is "Tibia   " or whose class is
"TibiaClient" (caseless; src/Player.cpp CountTibiaWindows, hidden windows too: EnumWindows), its own included,
and with more than one shows "A Tibia client is already running!" and waits on that box until it is closed. The
test build counts only its own windows since issue #96, so PatellaTest.exe never refuses, but its window (also a
headless one) still counts for Tibia.exe.

Until #148 the tools left such a client on its box and started the next one, which met the same box: 9 boxes in 4
minutes in front of the owner's work. This module gives the tools (tools/session_client.py's Driver.start, which
compare_traffic.py, compare_files.py, compare_network.py, compare_rendering.py and reference_record.py use, and
tools/login_client.py) what they need to stop that:

  client_windows(exclude)  the windows Tibia.exe would count, with their process (PID and name);
  wait_free(log, timeout)  before a start: waits until there are none, naming them once; returns those still open;
  message_boxes(pid)       a process's message boxes and their texts (a refused start's box);
  refused(boxes)           whether one of them is "A Tibia client is already running!".

The policy (one more start after a failed one, then a run-ending error) is session_client.Driver.start's.

PATELLA_CLIENT_WAIT in the tools' environment sets wait_free's timeout in seconds (default 120); "off" skips the
check, so a start meets the box (the issue's proof of the failed-start path only).

  python -I tools/client_start.py              lists the client windows open now
  python -I tools/client_start.py --self-test  checks the window list on a hidden window of its own
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows.argtypes = [WNDENUMPROC, wt.LPARAM]
user32.EnumChildWindows.argtypes = [wt.HWND, WNDENUMPROC, wt.LPARAM]
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [wt.HWND]
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
kernel32.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
kernel32.CloseHandle.argtypes = [wt.HANDLE]

# What CountTibiaWindows compares (src/Player.cpp GetWindowTitle, GetWindowClassName: TitleCode ^ TitleKey is
# "Tibia" and three spaces), caseless.
TITLE = "Tibia   "
CLASS = "TibiaClient"
REFUSED = "A Tibia client is already running!"
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
FREE_TIMEOUT = 120.0  # seconds a start waits for another client's window to go (wait_free; PATELLA_CLIENT_WAIT)
POLL = 1.0


def process_name(pid):
	"""The file name of a process's executable, or "?" (a process this user may not query)."""
	handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
	if not handle:
		return "?"
	try:
		buffer = ctypes.create_unicode_buffer(1024)
		size = wt.DWORD(len(buffer))
		if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
			return "?"
		return os.path.basename(buffer.value)
	finally:
		kernel32.CloseHandle(handle)


def window_pid(hwnd):
	owner = wt.DWORD()
	user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
	return owner.value


def text_of(hwnd, function, size=256):
	buffer = ctypes.create_unicode_buffer(size)
	function(hwnd, buffer, size)
	return buffer.value


def client_windows(exclude=()):
	"""The top-level windows Tibia.exe's start counts, hidden ones too, but those of the PIDs in exclude:
	[{"hwnd", "pid", "process", "class", "title", "visible"}]."""
	found = []

	def callback(hwnd, lparam):
		title = text_of(hwnd, user32.GetWindowTextW)
		cls = text_of(hwnd, user32.GetClassNameW, 64)
		if title.lower() == TITLE.lower() or cls.lower() == CLASS.lower():
			pid = window_pid(hwnd)
			if pid not in exclude:
				found.append({"hwnd": hwnd, "pid": pid, "class": cls, "title": title,
					"visible": bool(user32.IsWindowVisible(hwnd))})
		return True

	user32.EnumWindows(WNDENUMPROC(callback), 0)
	for window in found:
		window["process"] = process_name(window["pid"])
	return found


def describe(windows):
	"""One line for windows of client_windows."""
	return "; ".join("PID %d (%s): window 0x%x, class %r, title %r, %s" % (w["pid"], w["process"], w["hwnd"],
		w["class"], w["title"], "visible" if w["visible"] else "hidden") for w in windows)


def free_timeout():
	"""wait_free's timeout: PATELLA_CLIENT_WAIT's seconds, FREE_TIMEOUT without it, None for "off" (no check)."""
	value = os.environ.get("PATELLA_CLIENT_WAIT", "").strip().lower()
	if not value:
		return FREE_TIMEOUT
	if value == "off":
		return None
	try:
		return max(0.0, float(value))
	except ValueError:
		raise SystemExit("PATELLA_CLIENT_WAIT=%r: seconds or off" % value)


def wait_free(log, timeout=None, exclude=()):
	"""Waits until no other client window is open (client_windows), at most timeout seconds (None:
	free_timeout()); says which windows it waits for, once. Returns the windows still open ([]: free)."""
	if timeout is None:
		timeout = free_timeout()
		if timeout is None:
			log("PATELLA_CLIENT_WAIT=off: no check for another client's window")
			return []
	deadline = time.time() + timeout
	windows = client_windows(exclude)
	if not windows:
		return []
	log("another client's window is open, Tibia.exe would refuse to start (\"%s\"): %s; waiting up to %d s for "
		"it to close" % (REFUSED, describe(windows), timeout))
	while windows and time.time() < deadline:
		time.sleep(min(POLL, max(0.0, deadline - time.time())))
		windows = client_windows(exclude)
	if not windows:
		log("the other client's window is closed")
	return windows


def message_boxes(pid):
	"""The visible top-level message boxes (#32770) of a process: [(hwnd, caption, text)], the text its Static
	controls' lines."""
	found = []

	def callback(hwnd, lparam):
		if window_pid(hwnd) == pid and user32.IsWindowVisible(hwnd) \
				and text_of(hwnd, user32.GetClassNameW, 64) == "#32770":
			texts = []

			def child(h, l):
				if text_of(h, user32.GetClassNameW, 64) == "Static":
					text = text_of(h, user32.GetWindowTextW, 1024)
					if text:
						texts.append(text)
				return True

			user32.EnumChildWindows(hwnd, WNDENUMPROC(child), 0)
			found.append((hwnd, text_of(hwnd, user32.GetWindowTextW), "\n".join(texts)))
		return True

	user32.EnumWindows(WNDENUMPROC(callback), 0)
	return found


def refused(boxes):
	"""Whether one of message_boxes' boxes is the refused start's."""
	return any(REFUSED in text for _, _, text in boxes)


def self_test(out=print):
	"""client_windows, describe and wait_free on a hidden top-level window of this process with the clients'
	title (Tibia.exe would count it; it is never shown). Returns the number of failed expectations."""
	failures = 0

	def check(title, ok):
		nonlocal failures
		failures += not ok
		out("  %-60s %s" % (title, "ok" if ok else "FAILED"))

	user32.CreateWindowExW.restype = wt.HWND
	user32.CreateWindowExW.argtypes = [wt.DWORD, wt.LPCWSTR, wt.LPCWSTR, wt.DWORD, ctypes.c_int, ctypes.c_int,
		ctypes.c_int, ctypes.c_int, wt.HWND, wt.HMENU, wt.HINSTANCE, wt.LPVOID]
	user32.DestroyWindow.argtypes = [wt.HWND]
	own = os.getpid()
	others = [w["pid"] for w in client_windows((own,))]
	out("client windows (issue #148)")
	check("none of this process before", not client_windows(set(others)))
	hwnd = user32.CreateWindowExW(0, "STATIC", "tibia   ", 0, 0, 0, 10, 10, None, None, None, None)
	if not hwnd:
		check("CreateWindowExW", False)
		return failures
	try:
		mine = client_windows(set(others))
		check("a hidden window titled 'tibia   ' counts (caseless)", len(mine) == 1 and mine[0]["hwnd"] == hwnd
			and mine[0]["pid"] == own and not mine[0]["visible"])
		check("its process is named", bool(mine) and mine[0]["process"].lower().startswith("python"))
		check("described with its PID", bool(mine) and ("PID %d (" % own) in describe(mine))
		check("excluded by its PID", not client_windows(set(others) | {own}))
		lines = []
		start = time.time()
		left = wait_free(lines.append, timeout=0.5, exclude=set(others))
		check("wait_free: still open after its timeout, said once", len(left) == 1 and len(lines) == 1
			and REFUSED in lines[0] and time.time() - start < 5)
	finally:
		user32.DestroyWindow(hwnd)
	lines = []
	check("wait_free: free once the window is gone, nothing said", wait_free(lines.append, 0.5, set(others)) == []
		and not lines)
	check("refused: the box's text", refused([(0, "Tibia - Error", REFUSED)])
		and not refused([(0, "Tibia - Error", "Application is corrupted! Please re-install Tibia.")]))
	saved = os.environ.pop("PATELLA_CLIENT_WAIT", None)
	try:
		check("PATELLA_CLIENT_WAIT: unset 120 s, '5' 5 s, 'off' no check", free_timeout() == FREE_TIMEOUT
			and (os.environ.update(PATELLA_CLIENT_WAIT="5") or free_timeout()) == 5.0
			and (os.environ.update(PATELLA_CLIENT_WAIT="off") or free_timeout()) is None)
	finally:
		os.environ.pop("PATELLA_CLIENT_WAIT", None)
		if saved is not None:
			os.environ["PATELLA_CLIENT_WAIT"] = saved
	driver_test(check)  # its failures through check
	out("client_start self test: %d failures" % failures)
	return failures


def driver_test(check):
	"""session_client.Driver.start's policy on a driver whose starts fail without a process: one more start after
	a failed one, then StartError; a client window open past the wait: StartError before any start."""
	sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
	import session_client as sc

	class Failing(sc.Driver):
		def start_once(self, arguments=()):
			self.starts += 1
			raise sc.SessionError("no TibiaClient window after 30 s")

	lines = []
	driver = Failing(".", "Tibia.exe", log=lines.append)
	driver.starts = 0
	driver.counts_windows = lambda arguments: False
	try:
		driver.start()
		check("two failed starts: StartError", False)
	except sc.StartError as e:
		check("two failed starts: StartError, with the reason", driver.starts == sc.START_ATTEMPTS
			and "no TibiaClient window" in str(e) and any("starting the client again" in line for line in lines))
	hwnd = user32.CreateWindowExW(0, "STATIC", TITLE, 0, 0, 0, 10, 10, None, None, None, None)
	saved = os.environ.get("PATELLA_CLIENT_WAIT")
	os.environ["PATELLA_CLIENT_WAIT"] = "0.3"
	try:
		driver = Failing(".", "Tibia.exe", log=lines.append)
		driver.starts = 0
		try:
			driver.start()
			check("a client window open: StartError, nothing started", False)
		except sc.StartError as e:
			check("a client window open: StartError naming it, nothing started", driver.starts == 0
				and ("PID %d (" % os.getpid()) in str(e))
	finally:
		user32.DestroyWindow(hwnd)
		os.environ.pop("PATELLA_CLIENT_WAIT", None)
		if saved is not None:
			os.environ["PATELLA_CLIENT_WAIT"] = saved


def main():
	parser = argparse.ArgumentParser(description="List the windows that make Tibia.exe refuse to start (issue #148).")
	parser.add_argument("--self-test", action="store_true")
	args = parser.parse_args()
	if args.self_test:
		return 1 if self_test() else 0
	windows = client_windows()
	print(describe(windows) if windows else "no client window open")
	return 0


if __name__ == "__main__":
	sys.exit(main())
