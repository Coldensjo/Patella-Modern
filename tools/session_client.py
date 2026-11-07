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

Timing (Patella-Modern issue #144; notes/traffic.md, "The waits"): a gesture's messages (a key with its modifier, a
click with its modifier and mouse move, a typed string) are posted back to back, with no sleep between them: Windows'
queue keeps their order and the clients handle each message on its own (the scripted test client gets them as one
control-channel command of post lines, each still delivered in an idle call of its own). No step sleeps for a fixed
time: each waits for what it is meant to do, as both clients show it (Tibia.exe through tools/client_state.py, the
test client through its probe), with a timeout and an error that names what did not happen (Driver.wait_until):
the start screen, the login dialog, the character list, the player in the game, the player's position after each
walk, the direction after the turn, the said line in the console, a window that opens or closes. With a manual clock
(compare_rendering.py's drivers: the clock moves only by the step's wait, an advance) the condition is checked after
the step's wait instead of before it.

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
import struct
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
STEP_TIMEOUT = 10.0  # seconds a step's condition (Driver.wait_until) may take (issue #144)
START_TIMEOUT = 30.0  # seconds from the window to the start screen (TStartWindow in the window tree)
POLL = 0.05  # seconds between two readings of a condition
# The player's direction (TCreature.Direction): north, east, south, west.
NORTH, EAST, SOUTH, WEST = 0, 1, 2, 3


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
	post() sends every message through the channel instead. manual_clock: the client's clock moves only by the
	step's wait (compare_rendering.py's drivers, an advance), so run_session checks a step's condition after the
	wait, not before it (issue #144)."""

	manual_clock = False

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
		self.mem = self.reader()
		# Issue #144: the start screen, not a fixed 4 s (the Enter Game click needs its window).
		self.wait_window("TStartWindow", START_TIMEOUT)

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
		self.post_all([(msg, wparam, lparam)])

	def post_all(self, messages):
		"""Posts the messages [(msg, wparam, lparam)] back to back, in their order (issue #144): Windows' queue
		keeps it. The scripted test client drops Windows' input: there they go through its control channel as one
		command of post lines (a script's post lines, each dispatched as Windows would, in an idle call of its own
		with the quiet idle calls after it)."""
		if self.scripted:
			self.channel.command("\n".join("post 0x%x 0x%x 0x%x" % (msg, wparam & 0xffffffffffffffff,
				lparam & 0xffffffffffffffff) for msg, wparam, lparam in messages), alive=self.alive)
			return
		for msg, wparam, lparam in messages:
			lc.post(self.hwnd, msg, wparam, lparam)

	def key(self, vk, modifier=None):
		messages = [(WM_KEYDOWN, vk, key_lparam(vk, False)), (WM_KEYUP, vk, key_lparam(vk, True))]
		if modifier:
			messages = [(WM_KEYDOWN, modifier, key_lparam(modifier, False))] + messages + [
				(WM_KEYUP, modifier, key_lparam(modifier, True))]
		self.post_all(messages)

	# login_client.py's key_char, type_text and ctrl_char, through post_all() and without their sleeps

	def char(self, code):
		self.post(lc.WM_CHAR, code, 1)

	def type(self, text):
		"""The text's WM_CHARs, all at once (issue #144)."""
		self.post_all([(lc.WM_CHAR, ord(ch), 1) for ch in text])

	def ctrl(self, letter):
		self.post_all([(WM_KEYDOWN, VK_CONTROL, 0x001d0001), (lc.WM_CHAR, ord(letter.lower()) - 0x60, 1),
			(WM_KEYUP, VK_CONTROL, 0xc01d0001)])

	def click(self, x, y, right=False, modifier=None):
		x, y = int(x), int(y)
		flags = {None: 0, VK_SHIFT: MK_SHIFT, VK_CONTROL: MK_CONTROL}[modifier]
		down, up, button = ((WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON) if right
			else (lc.WM_LBUTTONDOWN, lc.WM_LBUTTONUP, lc.MK_LBUTTON))
		point = lc.lparam_xy(x, y)
		messages = [(lc.WM_MOUSEMOVE, flags, point), (down, button | flags, point), (up, flags, point)]
		if modifier:
			messages = [(WM_KEYDOWN, modifier, key_lparam(modifier, False))] + messages + [
				(WM_KEYUP, modifier, key_lparam(modifier, True))]
		self.post_all(messages)

	# waits (issue #144)

	def wait_until(self, what, test, timeout=STEP_TIMEOUT):
		"""test()'s value once it is true, read every POLL seconds. SessionError naming `what` when it is not
		within timeout seconds or the client ends. A reading that fails (a window tree read while the client
		changes it) counts as not yet; the last failure is in the error."""
		deadline = time.time() + timeout
		failure = None
		while True:
			try:
				value = test()
				if value:
					return value
			except SessionError:
				raise
			except Exception as e:  # client_state.ReadError, testmode_client.ControlError, ...
				failure = e
			if not self.alive():
				raise SessionError("the client ended while waiting for %s" % what)
			if time.time() >= deadline:
				raise SessionError("%s: not within %.0f s%s" % (what, timeout,
					"" if failure is None else " (the last reading failed: %s)" % failure))
			time.sleep(POLL)

	def windows(self, cls):
		return [n for n in self.mem.gui() if n["class"] == cls]

	def wait_window(self, cls, timeout=STEP_TIMEOUT):
		"""The first window of class cls, once there is one."""
		return self.wait_until("a window of class %s" % cls, lambda: (self.windows(cls) or [None])[0], timeout)

	def wait_no_window(self, cls, timeout=STEP_TIMEOUT):
		self.wait_until("no window of class %s" % cls, lambda: not self.windows(cls), timeout)

	def player(self, state=None):
		"""The player's own creature in the client's table (None before the game login)."""
		state = state if state is not None else self.mem.state()
		player_id = state["player"]["PlayerID"]
		for c in state["creatures"]:
			if player_id and c["id"] == player_id:
				return c
		return None

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

	def menu_entry(self, text):
		"""The context menu's entry with this text (None: none)."""
		for n in self.mem.gui():
			if n["class"] == "TContextMenuEntry" and n.get("text") == text:
				return n
		return None

	def menu_entry_point(self, text):
		n = self.menu_entry(text)
		if n is None:
			raise SessionError("no context menu entry %r" % text)
		return n["x"] + n["w"] // 2, n["y"] + n["h"] // 2


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
# The map field the session's look step looks at (Shift+right click), with the step's name. LOOK_NORTH is the
# frozen reference recordings' (tools/reference_record.py through compare_rendering.py's session); compare_traffic.py's
# live runs look at the player's own field (issue #137): north of the player Cipfried, the temple NPC, wanders onto the
# field, and then one client's look names him and the other's the field's top item. No creature can stand on the
# player's field, so both clients look at the player. compare_rendering.py replays compare_traffic.py's recordings, so
# it has LOOK_OWN_FIELD too, and both check a recording's look step before they replay it (recorded_look, issue #140).
LOOK_NORTH = ("look north", (8, 5))
LOOK_OWN_FIELD = ("look at own field", (8, 6))
LOOKS = (LOOK_NORTH, LOOK_OWN_FIELD)
# The player's own map field (the middle of the 15x11 view, Map.cpp), against which a look field is a direction.
PLAYER_FIELD = (8, 6)


def _plain_game(events, direction):
	return [bytes.fromhex(e["plain"]) for e in events
		if e.get("kind") == "game" and e.get("dir") == direction and "plain" in e and "fields_end" not in e]


def recorded_look(events):
	"""The look step of a recorded session (proxy events, tools/proxy.py: compare_traffic.py's <client>-live.jsonl):
	LOOK_NORTH or LOOK_OWN_FIELD, from the client's look packet (0x8c x y z, the first in the session: "look in
	container" and "look at offer" come later) against the player's position in the game server's first packet (0x0a
	player, beat, bug reports, then 0x64 x y z: the map description). The four walk steps bring the player back to
	that field before the look. Raises SessionError when the recording has no look or looks at another field, so that a
	recording of another scenario is refused, not compared (Patella-Modern issue #140)."""
	server = _plain_game(events, "s2c")
	if not server or len(server[0]) < 14 or server[0][0] != 0x0a or server[0][8] != 0x64:
		raise SessionError("the recording has no game login answer (0x0a, then the map description 0x64)")
	start = struct.unpack_from("<HHB", server[0], 9)
	looks = [p for p in _plain_game(events, "c2s") if p[:1] == b"\x8c" and len(p) >= 6]
	if not looks:
		raise SessionError("the recording has no look packet (0x8c)")
	at = struct.unpack_from("<HHB", looks[0], 1)
	if at[2] != start[2]:
		raise SessionError("the recording's look is at %s, on another floor than the player's start %s" % (at, start))
	offset = (at[0] - start[0], at[1] - start[1])
	for look in LOOKS:
		if offset == (look[1][0] - PLAYER_FIELD[0], look[1][1] - PLAYER_FIELD[1]):
			return look
	raise SessionError("the recording's look is at %s, %s from the player's start %s: none of the session's look steps"
		% (at, offset, start))


def check_recorded_look(events, look, path):
	"""SessionError unless the recording at path (its events) has the look step look (recorded_look)."""
	found = recorded_look(events)
	if found != look:
		raise SessionError("%s was recorded with the look step %r (map field %s), the session here has %r (map field "
			"%s): a recording made before issue #137 or by another scenario; make a fresh one with "
			"python -I tools/compare_traffic.py (its orig-live.jsonl)" % (path, found[0], found[1], look[0], look[1]))


def game_connections(proxy):
	"""The number of game connections whose first packet the proxy has seen."""
	return sum(1 for e in list(proxy.events) if e["kind"] == "game" and "fields_end" in e)


def enter_login(driver):
	"""Start screen to the filled login dialog: Enter Game (a click: the client takes no key for it while
	disconnected), the login dialog (waited for, issue #144), then the account, Tab and the password, typed at once
	(the same WM_CHARs as before, one batch). Return is the caller's."""
	_, _, width, height = lc.client_offset(driver.hwnd)
	driver.click(113, height - 220)
	driver.wait_window("TLoginDialog")
	driver.type(ACCOUNT + "\t" + PASSWORD)


def wait_character_list(driver):
	"""The character list dialog and its names (issue #144: instead of a fixed 0.5 s after the login's wait)."""
	return driver.wait_until("the character list",
		lambda: driver.windows("TCharacterListDialog") and driver.mem.characters())


def wait_in_game(driver):
	"""The player's own creature in the client's table: the server's game login answer is in (issue #144)."""
	return driver.wait_until("the player in the game", driver.player)


def login(driver, wait, character, proxy=None):
	"""Start screen to the game: Enter Game, account, password, the character named `character`
	in the list (Down until it is highlighted: the list comes in the server's order). The game
	connection counts as made when the client's TCP table shows it established or, with a proxy,
	when the proxy got the client's game login packet: the TCP table check alone missed a connection
	the proxy had (issue #21; the replay into x86 Patella of issue #12's first attempts, where the
	connection "dropped" when the client was killed after that check). The player in the game (wait_in_game) is
	the caller's to wait for: tools/compare_files.py's crafted error replay ends the game login with a critical
	error box instead."""
	enter_login(driver)
	driver.char(13)
	wait("login")
	names = wait_character_list(driver)
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


def run_session(driver, wait, checkpoint, until=None, with_login=True, proxy=None, look=LOOK_NORTH):
	"""The scripted session. wait(name): until the server side is quiet; checkpoint(name): record
	the client state. Returns the list of step names. until: stop before this step (testing).
	with_login False: the caller has logged in already (tools/compare_rendering.py, issue #20).
	proxy: the proxy the client goes through (login's check of the game connection). look: the look step's name
	and map field (LOOK_NORTH, LOOK_OWN_FIELD).

	Each step's done (issue #144), when it has one: called before the step's input, it returns (what, test), the
	condition the step is meant to bring about as the client shows it; Driver.wait_until waits for it after the input
	(with a manual clock after the step's wait, which moves the clock) and the step fails with `what` when it does
	not come. The steps without one (the looks, use in container, trade with's click on the player) change nothing a
	reading shows but the server's text; the step's wait covers them, as before."""
	steps = []

	def step(name, action, snapshot=True, done=None):
		if name == until:
			raise StopSession(name)
		driver.log("step: " + name)
		condition = done() if done is not None else None
		action()
		if condition is not None and not driver.manual_clock:
			driver.wait_until(*condition)
		wait(name)
		if condition is not None and driver.manual_clock:
			driver.wait_until(*condition)
		steps.append(name)
		if snapshot:
			checkpoint(name)

	def walked(dx, dy):
		def done():
			x, y, z = driver.wait_until("the player's position", driver.player)["pos"]
			target = [x + dx, y + dy, z]
			return "the player on %s (walked from %s)" % (target, [x, y, z]), lambda: driver.player()["pos"] == target
		return done

	def turned(direction):
		return lambda: ("the player turned to direction %d" % direction,
			lambda: driver.player()["direction"] == direction)

	def said(text):
		def lines():
			return sum(1 for mode, line in driver.mem.state()["console"] if line == text)

		def done():
			before = lines()
			return "the console line %r" % text, lambda: lines() > before
		return done

	def window_count(cls, change):
		"""The number of windows of class cls changed by change (+1: one opened, -1: one closed)."""
		def done():
			target = len(driver.windows(cls)) + change
			return ("%d window(s) of class %s" % (target, cls)), lambda: len(driver.windows(cls)) == target
		return done

	def menu_entry(text):
		return lambda: ("the context menu entry %r" % text, lambda: driver.menu_entry(text) is not None)

	def menu_gone(text):
		return lambda: ("the context menu closed (no entry %r)" % text, lambda: driver.menu_entry(text) is None)

	if with_login:
		step("login", lambda: login(driver, wait, CHARACTER, proxy),
			done=lambda: ("the player in the game", driver.player))
	for name, vk, dx, dy in (("walk north", VK_UP, 0, -1), ("walk east", VK_RIGHT, 1, 0), ("walk south", VK_DOWN, 0, 1),
			("walk west", VK_LEFT, -1, 0)):
		step(name, lambda vk=vk: driver.key(vk), done=walked(dx, dy))
	step("turn south", lambda: driver.key(VK_DOWN, VK_CONTROL), done=turned(SOUTH))
	step("say", lambda: driver.type(SAY + "\r"), done=said("%s: %s" % (CHARACTER, SAY)))
	look_name, look_field = look
	step(look_name, lambda: driver.click(*driver.field_point(*look_field), right=True, modifier=VK_SHIFT))
	step("open container", lambda: driver.click(*driver.body_slot_point(3), right=True, modifier=VK_CONTROL),
		done=window_count("TContainerWindowBody", 1))
	step("look in container", lambda: driver.click(*driver.container_slot_point(0), right=True, modifier=VK_SHIFT))
	step("use in container", lambda: driver.click(*driver.container_slot_point(0), right=True, modifier=VK_CONTROL))
	step("context menu", lambda: driver.click(*driver.body_slot_point(5), right=True), snapshot=False,
		done=menu_entry("Trade with ..."))
	step("trade with", lambda: driver.click(*driver.menu_entry_point("Trade with ...")), snapshot=False,
		done=menu_gone("Trade with ..."))
	step("trade request", lambda: driver.click(*driver.creature_point(driver.creature_id(TRADE_PARTNER))),
		done=window_count("TTradeOffers", 1))
	step("look at offer", lambda: driver.click(*driver.offer_slot_point(0, 0), right=True, modifier=VK_SHIFT))
	step("cancel trade", lambda: driver.click(*driver.component_point(
		driver.mem.member(driver.node("TTradeButtons")["ptr"], "TTradeButtons.CancelButton"))),
		done=window_count("TTradeOffers", -1))

	def close_container():
		body = driver.node("TContainerWindowBody")
		window = driver.mem.member(body["ptr"], "TComponent.Parent")
		driver.click(*driver.component_point(driver.mem.member(window, "TSubWindow.CloseButton")))

	step("close container", close_container, done=window_count("TContainerWindowBody", -1))
	step("logout", lambda: logout(driver), snapshot=False)
	return steps
