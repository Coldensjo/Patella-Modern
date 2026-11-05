r"""Run a live check on a hidden Windows desktop of its own, with --hidden (Patella-Modern issue #56;
experimental).

The live checks (tools/reference.py compare and record, tools/testmode_check.py, tools/compare_traffic.py,
compare_files.py, compare_rendering.py, compare_network.py) start clients in real windows. Started from
the user's desktop they appear on it: the user had to keep off the mouse and the keyboard while one ran,
and a foreground change there could disturb a run (issue #52's lost scripted click). With --hidden
each of those tools first runs itself again on a new desktop (CreateDesktopW in the tool's own window
station, the child started with STARTUPINFO.lpDesktop); every process the child starts (the clients,
the proxy, the test server, patch_client.py, a build) inherits that desktop, so nothing appears on the
user's screen and the input desktop's mouse and keyboard cannot reach the clients. The tools need no
other change: EnumWindows, FindWindow and the screen DC of a thread see its own desktop, posted window
messages, ReadProcessMemory (tools/client_state.py), patcap.dll's capture and the test mode's frame
dumps do not depend on the desktop at all.

The default is the user's desktop, as before this module: the clients do not behave on a desktop that is
not the input desktop as they do on the user's (notes/decisions.md, "The live checks on a hidden desktop").
DirectDraw and Direct3D 9 do not start there (Tibia.exe and the oracle's Patella.exe fall back to OpenGL),
the Vulkan CPU driver's first present fails (lavapipe, PATELLA_VULKAN=cpu: the client ends after 1 s), and
GetCursorPos fails (the posted clicks of compare_traffic.py and compare_files.py miss). What gives the same
results there: PatellaTest.exe on a GPU (reference.py compare --vulkan gpu), the OpenGL engine
(compare_rendering.py --engines gl) and compare_network.py. The owner's way to keep the live checks off
the user's desktop is a second interactive session (notes/reconstruction.md, "Checking").

In a tool (its `if __name__ == "__main__":` block, before main):
  desktop.hide(skip=("--self-test", ...))
returns at once, and the tool goes on in this process, when
  - --hidden is not on the command line (the default: the user's desktop, as on main); --hidden is
    removed from sys.argv either way, and the child does not get it;
  - the process already runs on a desktop this module made (PATELLA_DESKTOP names it, and it is the
    thread's desktop);
  - an argument in skip is given (the tool starts no client then: --self-test, --check-only, list...);
  - the platform is not Windows (the Linux runs start no window this module could hide);
otherwise it runs the same command line (python with the same -I, the tool, the arguments) on a new
desktop with the same standard handles, waits for it, and exits with its exit code. The child is in a
job object of its own: after it exits, every process of the run still in the job is listed; a client
or a helper left behind is ended (it was started by this run, on a desktop nobody else can see); the
test server (tibia-server.exe and its console host) is left running and named, for testserver.py stop.
Then the desktop handle is closed: the desktop goes away with its last process.

`python -I tools/desktop.py --self-test` starts a child on a hidden desktop that opens a window, finds
it with EnumWindows on its own desktop and checks the desktop's name; the parent checks that it cannot
see that window on its own desktop. `python -I tools/desktop.py --run <command...>` runs any command
on a hidden desktop (the same job handling) and returns its exit code.
"""

import os
import subprocess
import sys
import time
import uuid

ENV = "PATELLA_DESKTOP"
HIDDEN = "--hidden"
HIDDEN_HELP = ("experimental: run on a hidden desktop of its own (tools/desktop.py, issue #56); DirectDraw, Direct3D 9, "
	"the Vulkan CPU driver and GetCursorPos do not work there (default: the user's desktop)")
WINDOWS = os.name == "nt"
# What may stay running after the run: the test server keeps the desktop until testserver.py stop ends it.
SERVER_NAMES = ("tibia-server.exe", "conhost.exe")

if WINDOWS:
	import ctypes
	import ctypes.wintypes as wt

	user32 = ctypes.WinDLL("user32", use_last_error=True)
	kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

	GENERIC_ALL = 0x10000000
	UOI_NAME = 2
	STARTF_USESTDHANDLES = 0x00000100
	CREATE_SUSPENDED = 0x00000004
	CREATE_UNICODE_ENVIRONMENT = 0x00000400
	CREATE_NO_WINDOW = 0x08000000
	HANDLE_FLAG_INHERIT = 0x00000001
	STD_HANDLES = (-10, -11, -12)  # STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE
	WAIT_OBJECT_0 = 0
	PROCESS_TERMINATE = 0x0001
	PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
	SYNCHRONIZE = 0x00100000
	JOB_OBJECT_BASIC_PROCESS_ID_LIST = 3

	class STARTUPINFOW(ctypes.Structure):
		_fields_ = [("cb", wt.DWORD), ("lpReserved", wt.LPWSTR), ("lpDesktop", wt.LPWSTR), ("lpTitle", wt.LPWSTR),
			("dwX", wt.DWORD), ("dwY", wt.DWORD), ("dwXSize", wt.DWORD), ("dwYSize", wt.DWORD),
			("dwXCountChars", wt.DWORD), ("dwYCountChars", wt.DWORD), ("dwFillAttribute", wt.DWORD),
			("dwFlags", wt.DWORD), ("wShowWindow", wt.WORD), ("cbReserved2", wt.WORD),
			("lpReserved2", ctypes.c_void_p), ("hStdInput", wt.HANDLE), ("hStdOutput", wt.HANDLE),
			("hStdError", wt.HANDLE)]

	class PROCESS_INFORMATION(ctypes.Structure):
		_fields_ = [("hProcess", wt.HANDLE), ("hThread", wt.HANDLE), ("dwProcessId", wt.DWORD),
			("dwThreadId", wt.DWORD)]

	user32.CreateDesktopW.restype = wt.HANDLE
	user32.CreateDesktopW.argtypes = [wt.LPCWSTR, wt.LPCWSTR, ctypes.c_void_p, wt.DWORD, wt.DWORD, ctypes.c_void_p]
	user32.CloseDesktop.argtypes = [wt.HANDLE]
	user32.GetProcessWindowStation.restype = wt.HANDLE
	user32.GetThreadDesktop.restype = wt.HANDLE
	user32.GetThreadDesktop.argtypes = [wt.DWORD]
	user32.GetUserObjectInformationW.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_void_p, wt.DWORD,
		ctypes.POINTER(wt.DWORD)]
	kernel32.CreateProcessW.argtypes = [wt.LPCWSTR, wt.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, wt.BOOL, wt.DWORD,
		ctypes.c_void_p, wt.LPCWSTR, ctypes.POINTER(STARTUPINFOW), ctypes.POINTER(PROCESS_INFORMATION)]
	kernel32.GetStdHandle.restype = wt.HANDLE
	kernel32.GetStdHandle.argtypes = [wt.DWORD]
	kernel32.SetHandleInformation.argtypes = [wt.HANDLE, wt.DWORD, wt.DWORD]
	kernel32.GetConsoleWindow.restype = wt.HWND
	kernel32.CreateJobObjectW.restype = wt.HANDLE
	kernel32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wt.LPCWSTR]
	kernel32.AssignProcessToJobObject.argtypes = [wt.HANDLE, wt.HANDLE]
	kernel32.QueryInformationJobObject.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_void_p, wt.DWORD,
		ctypes.POINTER(wt.DWORD)]
	kernel32.ResumeThread.argtypes = [wt.HANDLE]
	kernel32.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
	kernel32.WaitForSingleObject.restype = wt.DWORD
	kernel32.GetExitCodeProcess.argtypes = [wt.HANDLE, ctypes.POINTER(wt.DWORD)]
	kernel32.TerminateProcess.argtypes = [wt.HANDLE, wt.UINT]
	kernel32.OpenProcess.restype = wt.HANDLE
	kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
	kernel32.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
	kernel32.CloseHandle.argtypes = [wt.HANDLE]
	kernel32.GetCurrentThreadId.restype = wt.DWORD


class DesktopError(Exception):
	pass


def say(text):
	print("[%s] desktop: %s" % (time.strftime("%H:%M:%S"), text), file=sys.stderr, flush=True)


def object_name(handle):
	"""The name of a window station or desktop handle (GetUserObjectInformationW, UOI_NAME)."""
	buffer = ctypes.create_unicode_buffer(256)
	needed = wt.DWORD()
	if not user32.GetUserObjectInformationW(handle, UOI_NAME, buffer, ctypes.sizeof(buffer), ctypes.byref(needed)):
		raise DesktopError("GetUserObjectInformationW failed (error %d)" % ctypes.get_last_error())
	return buffer.value


def current_desktop():
	"""The name of this thread's desktop ("Default" on the user's)."""
	return object_name(user32.GetThreadDesktop(kernel32.GetCurrentThreadId()))


def on_hidden_desktop():
	"""True in a process this module started: PATELLA_DESKTOP names this thread's desktop."""
	name = os.environ.get(ENV)
	return bool(WINDOWS and name and current_desktop() == name)


def python_command(argv):
	"""This interpreter with the flags the tools are run with (-I), then argv."""
	flags = ["-I"] if sys.flags.isolated else []
	return [sys.executable] + flags + list(argv)


def environment_block(env):
	return "".join("%s=%s\0" % (k, v) for k, v in sorted(env.items(), key=lambda kv: kv[0].upper())) + "\0"


def job_processes(job):
	"""The PIDs of the processes in a job (JobObjectBasicProcessIdList)."""
	count = 1024
	buffer = (ctypes.c_size_t * (2 + count))()
	if not kernel32.QueryInformationJobObject(job, JOB_OBJECT_BASIC_PROCESS_ID_LIST, buffer, ctypes.sizeof(buffer),
		None):
		return []
	return [int(buffer[2 + i]) for i in range(min(int(buffer[1]), count))]


def image_name(handle):
	buffer = ctypes.create_unicode_buffer(1024)
	size = wt.DWORD(len(buffer))
	if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
		return "?"
	return buffer.value


def end_leftovers(job, child_pid):
	"""After the run: every process of the job but the test server is ended; returns the PIDs left
	running (the test server's)."""
	left = []
	for pid in job_processes(job):
		if pid == child_pid:
			continue
		handle = kernel32.OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, False, pid)
		if not handle:
			continue
		try:
			path = image_name(handle)
			if os.path.basename(path).lower() in SERVER_NAMES:
				left.append(pid)
				say("PID %d (%s) still runs on the hidden desktop (python -I tools/testserver.py stop ends the "
					"test server)" % (pid, path))
				continue
			say("PID %d (%s) was left running by the run: ended" % (pid, path))
			kernel32.TerminateProcess(handle, 1)
			kernel32.WaitForSingleObject(handle, 10000)
		finally:
			kernel32.CloseHandle(handle)
	return left


def run(command, cwd=None, env=None):
	"""Runs command (a list) on a new hidden desktop, in a job of its own, with this process's standard
	handles; waits for it and returns its exit code."""
	if not WINDOWS:
		return subprocess.call(command, cwd=cwd, env=env)
	station = object_name(user32.GetProcessWindowStation())
	name = "patella-%d-%s" % (os.getpid(), uuid.uuid4().hex[:8])
	desktop = user32.CreateDesktopW(name, None, None, 0, GENERIC_ALL, None)
	if not desktop:
		raise DesktopError("CreateDesktopW failed (error %d)" % ctypes.get_last_error())
	job = None
	info = PROCESS_INFORMATION()
	try:
		job = kernel32.CreateJobObjectW(None, None)
		if not job:
			raise DesktopError("CreateJobObjectW failed (error %d)" % ctypes.get_last_error())
		startup = STARTUPINFOW()
		startup.cb = ctypes.sizeof(startup)
		startup.lpDesktop = "%s\\%s" % (station, name)
		handles = [kernel32.GetStdHandle(h) for h in STD_HANDLES]
		if all(h and h != wt.HANDLE(-1).value for h in handles):
			for h in handles:
				kernel32.SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)
			startup.dwFlags = STARTF_USESTDHANDLES
			startup.hStdInput, startup.hStdOutput, startup.hStdError = handles
		environment = dict(os.environ if env is None else env)
		environment[ENV] = name
		block = ctypes.create_unicode_buffer(environment_block(environment))
		flags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT
		if not kernel32.GetConsoleWindow():
			flags |= CREATE_NO_WINDOW  # no console to share: none of its own either
		line = ctypes.create_unicode_buffer(subprocess.list2cmdline(command))
		if not kernel32.CreateProcessW(None, line, None, None, True, flags, block, cwd, ctypes.byref(startup),
			ctypes.byref(info)):
			raise DesktopError("CreateProcessW failed (error %d)" % ctypes.get_last_error())
		if not kernel32.AssignProcessToJobObject(job, info.hProcess):
			# a job that forbids nesting: the run goes on without the leftover check
			say("AssignProcessToJobObject failed (error %d): leftovers are not checked" % ctypes.get_last_error())
			kernel32.CloseHandle(job)
			job = None
		kernel32.ResumeThread(info.hThread)
		say("running on the hidden desktop %s (PID %d; without --hidden: on this desktop)" % (startup.lpDesktop,
			info.dwProcessId))
		interrupted = False
		while True:
			try:
				if kernel32.WaitForSingleObject(info.hProcess, 200) == WAIT_OBJECT_0:
					break
			except KeyboardInterrupt:
				# the child shares the console and got the Ctrl+C too: it cleans up its own clients
				interrupted = True
		code = wt.DWORD()
		kernel32.GetExitCodeProcess(info.hProcess, ctypes.byref(code))
		if job is not None:
			end_leftovers(job, info.dwProcessId)
		if interrupted:
			raise KeyboardInterrupt
		return code.value
	finally:
		for handle in (info.hThread, info.hProcess, job):
			if handle:
				kernel32.CloseHandle(handle)
		user32.CloseDesktop(desktop)


def hide(skip=()):
	"""The start of a live check (see the module's text): returns to go on in this process, else (with
	--hidden) runs the tool on a hidden desktop and exits with its code."""
	hidden = HIDDEN in sys.argv[1:]
	if hidden:
		sys.argv[1:] = [a for a in sys.argv[1:] if a != HIDDEN]
	if not hidden or not WINDOWS or on_hidden_desktop() or any(a in skip for a in sys.argv[1:]):
		return
	script = os.path.abspath(sys.argv[0])
	sys.exit(run(python_command([script] + sys.argv[1:])))


# --- self-test ---------------------------------------------------------------------------

CHILD_TEST = r"""
import ctypes, ctypes.wintypes as wt, os, sys
sys.path.insert(0, %r)
import desktop
user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.CreateWindowExW.restype = wt.HWND
user32.CreateWindowExW.argtypes = [wt.DWORD, wt.LPCWSTR, wt.LPCWSTR, wt.DWORD, ctypes.c_int, ctypes.c_int,
	ctypes.c_int, ctypes.c_int, wt.HWND, wt.HMENU, wt.HINSTANCE, wt.LPVOID]
user32.FindWindowW.restype = wt.HWND
name = desktop.current_desktop()
title = "patella-desktop-test-%%d" %% os.getpid()
hwnd = user32.CreateWindowExW(0, "STATIC", title, 0x10000000, 10, 10, 200, 100, None, None, None, None)
found = user32.FindWindowW("STATIC", title)
print("desktop=%%s env=%%s hidden=%%s window=%%s found=%%s" %% (name, os.environ.get(desktop.ENV), desktop.on_hidden_desktop(),
	bool(hwnd), found == hwnd), flush=True)
with open(sys.argv[1], "w") as f:
	f.write("%%d %%s" %% (hwnd or 0, name))
import time
time.sleep(1.5)
sys.exit(0 if hwnd and found == hwnd and desktop.on_hidden_desktop() else 3)
"""


def top_level_window(hwnd):
	"""Whether EnumWindows (this thread's desktop) lists hwnd."""
	found = []
	callback = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(
		lambda h, _: (found.append(h) if h == hwnd else None) is None)
	user32.EnumWindows(callback, 0)
	return bool(found)


def self_test():
	import tempfile
	failures = []

	def check(what, condition, detail=""):
		print("%s: %s%s" % ("ok" if condition else "FAILED", what, " (%s)" % detail if detail else ""), flush=True)
		if not condition:
			failures.append(what)

	check("python_command keeps -I", python_command(["x.py"])[1:] == (["-I", "x.py"] if sys.flags.isolated else ["x.py"]))
	saved = list(sys.argv)
	try:
		sys.argv = ["tool.py", "--a", "b"]
		hide()
		check("without --hidden: hide() returns (the user's desktop)", sys.argv == ["tool.py", "--a", "b"],
			str(sys.argv))
		sys.argv = ["tool.py", "--self-test", HIDDEN]
		hide(skip=("--self-test",))
		check("--hidden with a skip argument: hide() returns and removes --hidden", sys.argv == ["tool.py",
			"--self-test"], str(sys.argv))
	finally:
		sys.argv = saved
	if not WINDOWS:
		print("not Windows: the desktop part is skipped")
		return 1 if failures else 0
	here = current_desktop()
	check("this process is on the user's desktop or a hidden one", bool(here), here)
	with tempfile.TemporaryDirectory() as folder:
		script = os.path.join(folder, "child.py")
		result = os.path.join(folder, "result.txt")
		with open(script, "w", encoding="utf-8") as f:
			f.write(CHILD_TEST % os.path.dirname(os.path.abspath(__file__)))
		import threading
		codes = []
		thread = threading.Thread(target=lambda: codes.append(run(python_command([script, result]))))
		thread.start()
		hwnd, name, seen = 0, "", None
		deadline = time.time() + 30
		while thread.is_alive() and time.time() < deadline and not hwnd:
			time.sleep(0.1)
			if os.path.isfile(result):
				with open(result, encoding="utf-8") as f:
					text = f.read().split(" ", 1)
				if len(text) == 2:
					hwnd, name = int(text[0]), text[1]
					seen = top_level_window(hwnd)
		thread.join()
		code = codes[0] if codes else -1
		check("the child ran on its hidden desktop, made a window and found it there", code == 0, "exit %d" % code)
		check("this desktop's EnumWindows does not see the child's window", hwnd != 0 and seen is False,
			"window 0x%x, seen %s" % (hwnd, seen))
		check("the child's desktop is a patella-* desktop, not this one", name.startswith("patella-") and name != here,
			name)
	return 1 if failures else 0


def main(argv):
	if argv[:1] == ["--self-test"]:
		return self_test()
	if argv[:1] == ["--run"] and len(argv) > 1:
		return run(argv[1:])
	print(__doc__)
	return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
