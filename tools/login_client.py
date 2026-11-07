r"""Log a client into the local test server and out again, with screenshots (issue #18).

Usage:
  python -I tools/login_client.py <run-dir> <exe> [--account 772001] [--password patella]
                                  [--stay 5] [--enter-game X,Y] [--keep-open] [--shot-only]

Starts <run-dir>\<exe> with <run-dir> as its working directory (a folder with copies of
Tibia.dat/.spr/.pic/.cfg and a client patched by tools/patch_client.py; never the repository
root), waits for its window and then drives it with posted window messages, so the window
needs neither the focus nor the mouse:

  1. start screen: screenshot 1-start.png; a click (WM_LBUTTONDOWN/UP) on "Enter Game"
     (MainWindow ignores keys while disconnected, so there is no keyboard way to open the
     login dialog). The button's place follows from the skin (GUI.cpp, Style0D8): the start
     panel's frame at (60, height-240), its border 10, the 86-wide button at (0,0) in the
     panel, so its middle is about (113, height-220) in the client area; --enter-game X,Y
     overrides it;
  2. login dialog: the account number, Tab, the password (WM_CHAR each), screenshot
     2-login.png, Return;
  3. the message of the day (shown when it is new to this Tibia.cfg) and the character list:
     a screenshot of each dialog (3-dialogN.png) and Return, until the client holds an
     established connection to port 7172 (GetExtendedTcpTable), at most three times;
     Return on the character list takes the first, highlighted character. The last dialog's
     screenshot is renamed 3-charlist.png;
  4. in the game: Return after 2 seconds (closes the "First Login" hint a new character
     gets; otherwise it goes to the empty console line), waits --stay seconds, screenshot
     4-game.png, Ctrl+L (WM_KEYDOWN VK_CONTROL, WM_CHAR 0x0c, WM_KEYUP: the logout hotkey
     0x44c, SendLogout); the game connection must be gone within 10 seconds (one more
     Return and Ctrl+L if not);
  5. back at the start screen: screenshot 5-logout.png, WM_CLOSE, waits for the process to
     exit (the client then rewrites the Tibia.cfg in <run-dir>).

The screenshots go into <run-dir> as PNG: a copy of the screen under the window (BitBlt from
the screen DC), taken while the window is made topmost for a moment (without activating it):
PrintWindow, even with PW_RENDERFULLCONTENT, returns a white client area for the clients'
DirectX surface.
--shot-only starts the client, takes 1-start.png and closes it (to find the button);
--keep-open leaves the client running at the end and prints its PID. Exit code 0 when the
client went through all steps and exited, 1 otherwise; the client is killed if it is still
running after a failure (only the process this script started).
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time
import zlib

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
iphlpapi = ctypes.WinDLL("iphlpapi")

WM_CLOSE = 0x0010
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_CHAR = 0x0102
WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
VK_CONTROL = 0x11
SRCCOPY = 0x00cc0020
HWND_TOPMOST = -1
HWND_NOTOPMOST = -2
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_NOACTIVATE = 0x0010

WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows.argtypes = [WNDENUMPROC, wt.LPARAM]
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.GetDC.argtypes = [wt.HWND]
user32.GetDC.restype = wt.HDC
user32.ReleaseDC.argtypes = [wt.HWND, wt.HDC]
user32.SetWindowPos.argtypes = [wt.HWND, wt.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.UINT]
gdi32.BitBlt.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.HDC, ctypes.c_int,
	ctypes.c_int, wt.DWORD]
user32.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]
gdi32.CreateCompatibleDC.argtypes = [wt.HDC]
gdi32.CreateCompatibleDC.restype = wt.HDC
gdi32.CreateCompatibleBitmap.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int]
gdi32.CreateCompatibleBitmap.restype = wt.HBITMAP
gdi32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
gdi32.SelectObject.restype = wt.HGDIOBJ
gdi32.DeleteObject.argtypes = [wt.HGDIOBJ]
gdi32.DeleteDC.argtypes = [wt.HDC]
gdi32.GetDIBits.argtypes = [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT, ctypes.c_void_p, ctypes.c_void_p, wt.UINT]


def tcp_connections(pid):
	"""(remote port, state) of the IPv4 TCP connections of a process; state 5 is ESTABLISHED."""
	size = wt.DWORD(0)
	iphlpapi.GetExtendedTcpTable(None, ctypes.byref(size), False, 2, 5, 0)
	buffer = ctypes.create_string_buffer(size.value + 4096)
	size = wt.DWORD(len(buffer))
	if iphlpapi.GetExtendedTcpTable(buffer, ctypes.byref(size), False, 2, 5, 0) != 0:
		return []
	count = struct.unpack_from("<I", buffer, 0)[0]
	result = []
	for i in range(count):
		state, laddr, lport, raddr, rport, owner = struct.unpack_from("<6I", buffer, 4 + 24 * i)
		if owner == pid:
			result.append((((rport & 0xff) << 8) | ((rport >> 8) & 0xff), state))
	return result


def wait_connection(pid, port, present, seconds):
	deadline = time.time() + seconds
	while time.time() < deadline:
		established = (port, 5) in tcp_connections(pid)
		if established == present:
			return True
		time.sleep(0.2)
	return False


class Failure(Exception):
	pass


def log(text):
	print("[%s] %s" % (time.strftime("%H:%M:%S"), text), flush=True)


def find_window(pid, visible=True):
	"""The process's TibiaClient window; with visible=False also a hidden one (a headless test client's, issue #145)."""
	found = []

	def callback(hwnd, lparam):
		owner = wt.DWORD()
		user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
		if owner.value == pid and (not visible or user32.IsWindowVisible(hwnd)):
			name = ctypes.create_unicode_buffer(64)
			user32.GetClassNameW(hwnd, name, 64)
			if name.value == "TibiaClient":
				found.append(hwnd)
		return True

	user32.EnumWindows(WNDENUMPROC(callback), 0)
	return found[0] if found else None


def lparam_xy(x, y):
	return (y << 16) | (x & 0xffff)


def post(hwnd, msg, wparam=0, lparam=0):
	if not user32.PostMessageW(hwnd, msg, wparam, lparam):
		raise Failure("PostMessage 0x%x failed (error %d)" % (msg, ctypes.get_last_error()))


def click(hwnd, x, y):
	post(hwnd, WM_MOUSEMOVE, 0, lparam_xy(x, y))
	time.sleep(0.2)
	post(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lparam_xy(x, y))
	time.sleep(0.15)
	post(hwnd, WM_LBUTTONUP, 0, lparam_xy(x, y))
	time.sleep(0.2)


def type_text(hwnd, text):
	for ch in text:
		post(hwnd, WM_CHAR, ord(ch), 1)
		time.sleep(0.05)


def key_char(hwnd, code):
	post(hwnd, WM_CHAR, code, 1)
	time.sleep(0.1)


def ctrl_char(hwnd, letter):
	post(hwnd, WM_KEYDOWN, VK_CONTROL, 0x001d0001)
	time.sleep(0.05)
	post(hwnd, WM_CHAR, ord(letter.lower()) - 0x60, 1)
	time.sleep(0.05)
	post(hwnd, WM_KEYUP, VK_CONTROL, 0xc01d0001)
	time.sleep(0.1)


def write_png(path, width, height, bgra):
	rows = []
	stride = width * 4
	for y in range(height):
		row = bgra[y * stride:(y + 1) * stride]
		rgb = bytearray(width * 3)
		rgb[0::3] = row[2::4]
		rgb[1::3] = row[1::4]
		rgb[2::3] = row[0::4]
		rows.append(b"\0" + bytes(rgb))

	def chunk(kind, data):
		body = kind + data
		return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)

	png = b"\x89PNG\r\n\x1a\n"
	png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
	png += chunk(b"IDAT", zlib.compress(b"".join(rows), 6))
	png += chunk(b"IEND", b"")
	with open(path, "wb") as f:
		f.write(png)


def screenshot(hwnd, path):
	flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
	user32.SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags)
	time.sleep(0.5)
	rect = wt.RECT()
	user32.GetWindowRect(hwnd, ctypes.byref(rect))
	width, height = rect.right - rect.left, rect.bottom - rect.top
	screen = user32.GetDC(None)
	dc = gdi32.CreateCompatibleDC(screen)
	bitmap = gdi32.CreateCompatibleBitmap(screen, width, height)
	old = gdi32.SelectObject(dc, bitmap)
	ok = gdi32.BitBlt(dc, 0, 0, width, height, screen, rect.left, rect.top, SRCCOPY)
	gdi32.SelectObject(dc, old)
	user32.SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, flags)
	# BITMAPINFOHEADER, top-down 32-bit
	header = struct.pack("<IiiHHIIiiII", 40, width, -height, 1, 32, 0, 0, 0, 0, 0, 0)
	info = ctypes.create_string_buffer(header + bytes(16))
	bits = ctypes.create_string_buffer(width * height * 4)
	lines = gdi32.GetDIBits(dc, bitmap, 0, height, bits, info, 0)
	gdi32.DeleteObject(bitmap)
	gdi32.DeleteDC(dc)
	user32.ReleaseDC(None, screen)
	if not ok or lines != height:
		raise Failure("screenshot of the window failed (BitBlt %d, GetDIBits %d)" % (ok, lines))
	write_png(path, width, height, bits.raw)
	log("screenshot %s (%dx%d)" % (path, width, height))


def client_offset(hwnd):
	# where the client area starts in the window screenshot
	rect = wt.RECT()
	user32.GetWindowRect(hwnd, ctypes.byref(rect))
	origin = wt.POINT(0, 0)
	user32.ClientToScreen(hwnd, ctypes.byref(origin))
	client = wt.RECT()
	user32.GetClientRect(hwnd, ctypes.byref(client))
	return origin.x - rect.left, origin.y - rect.top, client.right, client.bottom


def main():
	ctypes.windll.user32.SetProcessDPIAware()
	parser = argparse.ArgumentParser(description="Log a client into the local test server and out again.")
	parser.add_argument("run_dir")
	parser.add_argument("exe")
	parser.add_argument("--account", default="772001")
	parser.add_argument("--password", default="patella")
	parser.add_argument("--stay", type=float, default=5.0, help="seconds in the game before logging out")
	parser.add_argument("--enter-game", default=None, help="client coordinates X,Y of the Enter Game button")
	parser.add_argument("--start-wait", type=float, default=6.0, help="seconds for the start screen")
	parser.add_argument("--login-wait", type=float, default=4.0, help="seconds for the character list")
	parser.add_argument("--game-wait", type=float, default=4.0, help="seconds for the game connection after a Return")
	parser.add_argument("--keep-open", action="store_true")
	parser.add_argument("--shot-only", action="store_true")
	args = parser.parse_args()

	run_dir = os.path.abspath(args.run_dir)
	exe = os.path.join(run_dir, args.exe)
	if not os.path.isfile(exe):
		print("login_client.py: %s not found" % exe, file=sys.stderr)
		return 1
	for name in ("Tibia.dat", "Tibia.spr", "Tibia.pic", "Tibia.cfg"):
		if not os.path.isfile(os.path.join(run_dir, name)):
			print("login_client.py: %s has no %s" % (run_dir, name), file=sys.stderr)
			return 1

	proc = subprocess.Popen([exe], cwd=run_dir)
	log("started %s, PID %d" % (exe, proc.pid))
	success = False
	try:
		hwnd = None
		deadline = time.time() + 30
		while hwnd is None and time.time() < deadline:
			if proc.poll() is not None:
				raise Failure("the client exited with code %d before showing its window" % proc.returncode)
			hwnd = find_window(proc.pid)
			time.sleep(0.25)
		if hwnd is None:
			raise Failure("no TibiaClient window after 30 s")
		time.sleep(args.start_wait)
		left, top, cw, ch = client_offset(hwnd)
		log("window 0x%x, client area %dx%d at (%d,%d) in the window" % (hwnd, cw, ch, left, top))
		screenshot(hwnd, os.path.join(run_dir, "1-start.png"))
		if args.shot_only:
			success = True
			return 0

		if args.enter_game:
			x, y = (int(v) for v in args.enter_game.split(","))
		else:
			x, y = 113, ch - 220
		log("click Enter Game at (%d,%d)" % (x, y))
		click(hwnd, x, y)
		time.sleep(1.5)
		log("account %s, password" % args.account)
		type_text(hwnd, args.account)
		key_char(hwnd, 9)
		type_text(hwnd, args.password)
		time.sleep(0.5)
		screenshot(hwnd, os.path.join(run_dir, "2-login.png"))
		key_char(hwnd, 13)
		time.sleep(args.login_wait)
		# the message of the day (when it is new to this Tibia.cfg) comes before the character list
		for dialog in range(1, 4):
			shot = os.path.join(run_dir, "3-dialog%d.png" % dialog)
			screenshot(hwnd, shot)
			log("Return (dialog %d)" % dialog)
			key_char(hwnd, 13)
			if wait_connection(proc.pid, 7172, True, args.game_wait):
				break
		else:
			raise Failure("no game connection (port 7172) after three Returns")
		os.replace(shot, os.path.join(run_dir, "3-charlist.png"))
		log("connected to the game server (port 7172); the last dialog, the character list, is 3-charlist.png")
		# a new character gets the client's "First Login" hint dialog, which would take the
		# Ctrl+L; Return closes it (without a dialog it goes to the empty console line)
		time.sleep(2)
		log("Return (hint dialog)")
		key_char(hwnd, 13)
		time.sleep(args.stay)
		screenshot(hwnd, os.path.join(run_dir, "4-game.png"))
		for attempt in (1, 2):
			log("Ctrl+L: logout")
			ctrl_char(hwnd, "l")
			if wait_connection(proc.pid, 7172, False, 10):
				break
			log("still connected: Return, then Ctrl+L again")
			key_char(hwnd, 13)
		else:
			raise Failure("still connected to the game server after two Ctrl+L")
		log("the game connection is closed")
		time.sleep(2)
		screenshot(hwnd, os.path.join(run_dir, "5-logout.png"))
		if args.keep_open:
			log("left running, PID %d" % proc.pid)
			success = True
			return 0
		post(hwnd, WM_CLOSE)
		try:
			proc.wait(20)
		except subprocess.TimeoutExpired:
			raise Failure("the client did not exit within 20 s of WM_CLOSE")
		log("the client exited with code %d" % proc.returncode)
		success = True
		return 0
	except Failure as e:
		print("login_client.py: %s" % e, file=sys.stderr)
		return 1
	finally:
		if (not success or args.shot_only) and proc.poll() is None:
			if args.shot_only:
				hwnd = find_window(proc.pid)
				if hwnd:
					post(hwnd, WM_CLOSE)
				try:
					proc.wait(10)
				except subprocess.TimeoutExpired:
					pass
			if proc.poll() is None:
				proc.kill()
				proc.wait()
				log("killed PID %d" % proc.pid)


if __name__ == "__main__":
	sys.exit(main())
