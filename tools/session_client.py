r"""Drive a client through a scripted game session by window messages (issue #19).

The session the traffic comparison runs with both clients (tools/compare_traffic.py): log in,
take a character from the list, enter the game, walk, turn, talk, look, open a container,
look at and use (eat) an object in it, offer an object for trade to another player, inspect
the offer, cancel the trade, close the container and log out. Every action is a posted
window message (as in tools/login_client.py: no focus, no real mouse); the place of every
click comes from the client's own data, never from a screenshot: the window tree, the skin's
slot areas and the map window's creature names, read from Tibia.exe's memory
(tools/client_state.py) or, for the x64 client, from its test mode's probe (Patella-Modern issue
#92: PatellaTest.exe under its control channel, tools/testmode_client.py; Driver(test=True)). After each step
the driver waits until the server side is quiet (the proxy's wait_idle) and, if asked,
records the client's state.

Session steps (STEPS below); the coordinates are those of the map window's fields (the
player is on field 8,6) and of the body slots (3: the container slot):
  login, charlist, enter, walk N/E/S/W (arrow keys, back to the start), turn S (Ctrl+Down),
  say "hello world", look N (Shift+right click on field 8,5), open the container slot
  (Ctrl+right click, "use"), look at and eat the container's first object (Shift+/Ctrl+
  right click), trade: a right click on the body slot 5 object opens the context menu,
  "Trade with ..." in it, then a click on the other player's name position; look at the own
  offer (Shift+click in the trade window), Cancel; close the container window; Ctrl+L.
"""

import ctypes
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import login_client as lc  # noqa: E402
import testmode_client as tc  # noqa: E402

WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_RBUTTONDOWN = 0x0204
WM_RBUTTONUP = 0x0205
MK_RBUTTON = 0x0002
MK_SHIFT = 0x0004
MK_CONTROL = 0x0008
VK_RETURN = 0x0d
VK_SHIFT = 0x10
VK_CONTROL = 0x11
VK_ESCAPE = 0x1b
VK_LEFT = 0x25
VK_UP = 0x26
VK_RIGHT = 0x27
VK_DOWN = 0x28
SCAN = {VK_SHIFT: 0x2a, VK_CONTROL: 0x1d, VK_LEFT: 0x14b, VK_UP: 0x148, VK_RIGHT: 0x14d, VK_DOWN: 0x150,
	VK_RETURN: 0x1c, VK_ESCAPE: 0x01}


class SessionError(Exception):
	pass


NODE_TIMEOUT = 5.0  # seconds Driver.node reads the window tree again before a window counts as missing


def key_lparam(vk, up):
	scan = SCAN.get(vk, 0)
	value = 1 | ((scan & 0xff) << 16)
	if scan & 0x100:
		value |= 1 << 24
	if up:
		value |= 0xc0000000
	return value


def test_out(run_dir):
	"""The test client's --test-out folder: next to its run folder (the comparisons compare the files a
	client writes into its run folder)."""
	return os.path.abspath(run_dir) + ".testmode"


class Driver:
	"""One client. test: the client is the test build (PatellaTest.exe, the x64 client), started with
	its control channel (--test-control, test_arguments before the caller's words) and read through it
	(tools/testmode_client.py), never through its memory; else Tibia.exe (the oracle's Patella.exe until #94),
	read with tools/client_state.py (pdb_exe: the exe whose PDB it uses, None for Tibia.exe). scripted:
	the test client runs a script (test_arguments has --test-script), so it drops Windows' input, and
	post() sends every message through the channel instead."""

	def __init__(self, run_dir, exe, pdb_exe=None, game_port=7272, log=lc.log, test=False, test_arguments=(),
			scripted=False):
		self.run_dir = os.path.abspath(run_dir)
		self.exe = exe
		self.pdb_exe = pdb_exe
		self.game_port = game_port
		self.log = log
		self.proc = None
		self.hwnd = None
		self.mem = None
		self.test = test
		self.test_arguments = list(test_arguments)
		self.scripted = scripted
		self.channel = None

	# process

	def start(self, arguments=()):
		path = os.path.join(self.run_dir, self.exe)
		words = list(arguments)
		if self.test:
			self.channel = tc.Channel(test_out(self.run_dir))
			words = self.channel.arguments() + self.test_arguments + words
		self.proc = subprocess.Popen([path] + words, cwd=self.run_dir, env=self.environment())
		self.log("started %s, PID %d%s" % (path, self.proc.pid, " (test mode, control channel)" if self.test else ""))
		deadline = time.time() + 30
		while self.hwnd is None and time.time() < deadline:
			if self.proc.poll() is not None:
				raise SessionError("the client exited with code %d" % self.proc.returncode)
			self.hwnd = lc.find_window(self.proc.pid)
			time.sleep(0.25)
		if self.hwnd is None:
			raise SessionError("no TibiaClient window after 30 s")
		time.sleep(4)
		self.mem = self.reader()

	def environment(self):
		"""The client's environment (None: this one's)."""
		return None

	def alive(self):
		return self.proc is not None and self.proc.poll() is None

	def reader(self):
		"""What reads the client: the test client's channel, else its memory."""
		if self.test:
			return tc.TestClient(self.channel, self.alive)
		import client_state  # Tibia.exe only (issues #92, #94)
		return client_state.Client(self.proc.pid, self.pdb_exe)

	def kill(self):
		if self.proc is not None and self.proc.poll() is None:
			self.proc.kill()
			self.proc.wait()
			self.log("killed PID %d" % self.proc.pid)
		if self.mem is not None:
			self.mem.close()
			self.mem = None

	def close(self):
		lc.post(self.hwnd, lc.WM_CLOSE)
		try:
			self.proc.wait(20)
		except subprocess.TimeoutExpired:
			raise SessionError("the client did not exit within 20 s of WM_CLOSE")
		self.log("the client exited with code %d" % self.proc.returncode)
		if self.mem is not None:
			self.mem.close()
			self.mem = None
		return self.proc.returncode

	def connected(self, port):
		return (port, 5) in lc.tcp_connections(self.proc.pid)

	def wait_connected(self, port, present, seconds):
		return lc.wait_connection(self.proc.pid, port, present, seconds)

	# input

	def post(self, msg, wparam=0, lparam=0):
		if self.scripted:
			# The scripted test client drops Windows' input: the message goes through its control channel and
			# is dispatched as Windows would (a script's post line).
			self.channel.command("post 0x%x 0x%x 0x%x" % (msg, wparam & 0xffffffffffffffff, lparam & 0xffffffffffffffff),
				alive=self.alive)
			return
		lc.post(self.hwnd, msg, wparam, lparam)

	def key(self, vk, modifier=None):
		if modifier:
			self.post(WM_KEYDOWN, modifier, key_lparam(modifier, False))
			time.sleep(0.05)
		self.post(WM_KEYDOWN, vk, key_lparam(vk, False))
		time.sleep(0.05)
		self.post(WM_KEYUP, vk, key_lparam(vk, True))
		if modifier:
			time.sleep(0.05)
			self.post(WM_KEYUP, modifier, key_lparam(modifier, True))
		time.sleep(0.1)

	# login_client.py's key_char, type_text and ctrl_char, through post()

	def char(self, code):
		self.post(lc.WM_CHAR, code, 1)
		time.sleep(0.1)

	def type(self, text):
		for ch in text:
			self.post(lc.WM_CHAR, ord(ch), 1)
			time.sleep(0.05)

	def ctrl(self, letter):
		self.post(WM_KEYDOWN, VK_CONTROL, 0x001d0001)
		time.sleep(0.05)
		self.post(lc.WM_CHAR, ord(letter.lower()) - 0x60, 1)
		time.sleep(0.05)
		self.post(WM_KEYUP, VK_CONTROL, 0xc01d0001)
		time.sleep(0.1)

	def click(self, x, y, right=False, modifier=None):
		x, y = int(x), int(y)
		flags = {None: 0, VK_SHIFT: MK_SHIFT, VK_CONTROL: MK_CONTROL}[modifier]
		if modifier:
			self.post(WM_KEYDOWN, modifier, key_lparam(modifier, False))
			time.sleep(0.05)
		self.post(lc.WM_MOUSEMOVE, flags, lc.lparam_xy(x, y))
		time.sleep(0.1)
		if right:
			self.post(WM_RBUTTONDOWN, MK_RBUTTON | flags, lc.lparam_xy(x, y))
			time.sleep(0.1)
			self.post(WM_RBUTTONUP, flags, lc.lparam_xy(x, y))
		else:
			self.post(lc.WM_LBUTTONDOWN, lc.MK_LBUTTON | flags, lc.lparam_xy(x, y))
			time.sleep(0.1)
			self.post(lc.WM_LBUTTONUP, flags, lc.lparam_xy(x, y))
		time.sleep(0.1)
		if modifier:
			self.post(WM_KEYUP, modifier, key_lparam(modifier, True))
		time.sleep(0.1)

	# places, from the client's memory

	def node(self, cls, index=0, timeout=NODE_TIMEOUT):
		"""The index-th window of class cls. The tree is read from the running client without any
		lock, so one read can miss a window that is there (a traffic run found no TMapWindow after
		"say", issue #12): it is read again for up to timeout seconds before the window counts as
		missing."""
		deadline = time.time() + timeout
		while True:
			nodes = [n for n in self.mem.gui() if n["class"] == cls]
			if len(nodes) > index:
				return nodes[index]
			if time.time() >= deadline:
				raise SessionError("no window of class %s (#%d) within %.0f s" % (cls, index, timeout))
			time.sleep(0.2)

	def node_by_ptr(self, ptr):
		for n in self.mem.gui():
			if n["ptr"] == ptr:
				return n
		raise SessionError("component 0x%x not in the window tree" % ptr)

	def field_point(self, fx, fy):
		"""Client coordinates of the centre of map field fx, fy (the player is on 8, 6)."""
		m = self.map_window()
		return (m["x"] + ((fx - 1) * 32 + 16) * m["w"] // 480, m["y"] + ((fy - 1) * 32 + 16) * m["h"] // 352)

	def map_window(self):
		return self.node("TMapWindow")

	def body_slot_point(self, slot):
		view = self.node("TInventoryView")
		x, y, w, h = self.mem.skin_rects("Style288.Data60", 10)[slot - 1]
		return view["x"] + x + w // 2, view["y"] + y + h // 2

	def container_slot_point(self, position, body_index=0):
		body = self.node("TContainerWindowBody", body_index)
		columns = self.mem.skin_int("Data62C")
		ox, oy = self.mem.skin_int("Data630.x"), self.mem.skin_int("Data630.y")
		sw, sh = self.mem.skin_int("Data638.width"), self.mem.skin_int("Data638.height")
		pitch_x, pitch_y = self.mem.skin_int("Data648"), self.mem.skin_int("Data64C")
		column, row = position % columns, position // columns
		return body["x"] + ox + column * pitch_x + sw // 2, body["y"] + oy + row * pitch_y + sh // 2

	def creature_point(self, creature_id):
		"""Where GetCreatureAt finds the creature: its name position plus half a field down."""
		m = self.map_window()
		# the map window refills Names in every Draw: a read from outside can come while it is empty
		for attempt in range(40):
			for cid, nx, ny in self.mem.map_names(m["ptr"]):
				if cid == creature_id:
					return (m["x"] + nx * m["w"] // 480, m["y"] + ny * m["h"] // 352 + (m["h"] // 11) // 2)
			time.sleep(0.05)
		raise SessionError("creature %d has no name on the map" % creature_id)

	def menu_entry_point(self, text):
		for n in self.mem.gui():
			if n["class"] == "TContextMenuEntry" and n.get("text") == text:
				return n["x"] + n["w"] // 2, n["y"] + n["h"] // 2
		raise SessionError("no context menu entry %r" % text)


	def offer_slot_point(self, side, position):
		offers = self.node("TTradeOffers")
		x, y, w, h = self.mem.skin_rects("Style6FC.Data68" if side == 0 else "Style6FC.Data78", 1)[0]
		row_pitch, column_pitch, columns = (self.mem.skin_int("Style6FC.Data9C"), self.mem.skin_int("Style6FC.DataA0"),
			self.mem.skin_int("Style6FC.DataA4"))
		column, row = position % columns, position // columns
		return offers["x"] + x + column * column_pitch + w // 2, offers["y"] + y + row * row_pitch + h // 2

	def component_point(self, ptr):
		n = self.node_by_ptr(ptr)
		return n["x"] + n["w"] // 2, n["y"] + n["h"] // 2

	def creature_id(self, name):
		for c in self.mem.state()["creatures"]:
			if c["name"] == name:
				return c["id"]
		raise SessionError("no creature %r in the client's table" % name)


ACCOUNT = "772001"
PASSWORD = "patella"
SAY = "hello world"
CHARACTER = "Patella Tester"
TRADE_PARTNER = "Patella Second"


def game_connections(proxy):
	"""The number of game connections whose first packet the proxy has seen."""
	return sum(1 for e in list(proxy.events) if e["kind"] == "game" and "fields_end" in e)


def login(driver, wait, character, proxy=None):
	"""Start screen to the game: Enter Game, account, password, the character named `character`
	in the list (Down until it is highlighted: the list comes in the server's order). The game
	connection counts as made when the client's TCP table shows it established or, with a proxy,
	when the proxy got the client's game login packet: the TCP table check alone missed a connection
	the proxy had (issue #21; the replay into x86 Patella of issue #12's first attempts, where the
	connection "dropped" when the client was killed after that check)."""
	_, _, width, height = lc.client_offset(driver.hwnd)
	driver.click(113, height - 220)
	time.sleep(1.5)
	driver.type(ACCOUNT)
	driver.char(9)
	driver.type(PASSWORD)
	time.sleep(0.3)
	driver.char(13)
	wait("login")
	time.sleep(0.5)
	names = driver.mem.characters()
	if character not in names:
		raise SessionError("%s is not in the character list %s" % (character, names))
	for _ in range(names.index(character)):
		driver.key(VK_DOWN)
	before = game_connections(proxy) if proxy is not None else 0
	driver.char(13)
	deadline = time.time() + 10
	while time.time() < deadline:
		if driver.connected(driver.game_port):
			return
		if proxy is not None and game_connections(proxy) > before:
			driver.log("login: the proxy has the game connection (the TCP table shows %s)"
				% lc.tcp_connections(driver.proc.pid))
			return
		time.sleep(0.2)
	raise SessionError("no game connection (port %d) within 10 s; the client's TCP connections: %s"
		% (driver.game_port, lc.tcp_connections(driver.proc.pid)))


def logout(driver):
	driver.ctrl("l")
	if not driver.wait_connected(driver.game_port, False, 10):
		raise SessionError("still connected to the game server 10 s after Ctrl+L")


class StopSession(Exception):
	pass


def run_session(driver, wait, checkpoint, until=None, with_login=True, proxy=None):
	"""The scripted session. wait(name): until the server side is quiet; checkpoint(name): record
	the client state. Returns the list of step names. until: stop before this step (testing).
	with_login False: the caller has logged in already (tools/compare_rendering.py, issue #20).
	proxy: the proxy the client goes through (login's check of the game connection)."""
	steps = []

	def step(name, action, snapshot=True):
		if name == until:
			raise StopSession(name)
		driver.log("step: " + name)
		action()
		wait(name)
		steps.append(name)
		if snapshot:
			checkpoint(name)

	if with_login:
		step("login", lambda: login(driver, wait, CHARACTER, proxy))
	for name, vk in (("walk north", VK_UP), ("walk east", VK_RIGHT), ("walk south", VK_DOWN), ("walk west", VK_LEFT)):
		step(name, lambda vk=vk: (driver.key(vk), time.sleep(1.0)))
	step("turn south", lambda: driver.key(VK_DOWN, VK_CONTROL))
	step("say", lambda: (driver.type(SAY), driver.char(13)))
	step("look north", lambda: driver.click(*driver.field_point(8, 5), right=True, modifier=VK_SHIFT))
	step("open container", lambda: driver.click(*driver.body_slot_point(3), right=True, modifier=VK_CONTROL))
	step("look in container", lambda: driver.click(*driver.container_slot_point(0), right=True, modifier=VK_SHIFT))
	step("use in container", lambda: driver.click(*driver.container_slot_point(0), right=True, modifier=VK_CONTROL))
	step("context menu", lambda: driver.click(*driver.body_slot_point(5), right=True), snapshot=False)
	step("trade with", lambda: driver.click(*driver.menu_entry_point("Trade with ...")), snapshot=False)
	step("trade request", lambda: driver.click(*driver.creature_point(driver.creature_id(TRADE_PARTNER))))
	step("look at offer", lambda: driver.click(*driver.offer_slot_point(0, 0), right=True, modifier=VK_SHIFT))
	step("cancel trade", lambda: driver.click(*driver.component_point(
		driver.mem.member(driver.node("TTradeButtons")["ptr"], "TTradeButtons.CancelButton"))))

	def close_container():
		body = driver.node("TContainerWindowBody")
		window = driver.mem.member(body["ptr"], "TComponent.Parent")
		driver.click(*driver.component_point(driver.mem.member(window, "TSubWindow.CloseButton")))

	step("close container", close_container)
	step("logout", lambda: logout(driver), snapshot=False)
	return steps
