r"""Record, decrypt and replay the traffic between a Tibia 7.72 client and a server (issue #19).

A TCP proxy on 127.0.0.1 between a client (patched by tools/patch_client.py --port to the
proxy's login port) and the local test server. Two modes:

- live (Proxy(record_path)): the login connection goes to 127.0.0.1:7171, the game
  connection to 127.0.0.1:7172. The character list the login server sends carries the game
  server's address; its port (7172) is rewritten to the proxy's game port, so the client's
  game connection comes through the proxy as well.
- replay (Proxy(record_path, replay=events)): no server. Each connection of the client is
  answered from a recording of a live session: the recorded server packets are re-encrypted
  with the client's key and sent in the recorded order, each after the client packet that
  preceded it in the recording, with the recorded delay. Every client packet is checked
  against the recording; the client gets the same server data whatever it sends.

Decryption: the first packet of both connections is unencrypted ([u16 size][opcode 0x01
login, 0x0a game][u16 os][u16 version]([u32 dat][u32 spr][u32 pic], login only)[128 bytes
RSA]). The RSA block (src/Communication.cpp SendLogin/SendGameLogin) holds [0][16-byte XTEA
key][the fields][rand() bytes]; tibia.pem (the server's private key, the patched clients'
modulus) decrypts it. All later packets both ways are [u16 size][XTEA([u16 length][payload]
[padding to 8 bytes])] (src/Network.cpp SendCommand/PrepareNextPacket) and are decrypted
with that key.

Crafted and fuzzed server streams (issue #37, tools/compare_network.py): a replayed s2c event
may also say how its frame is made and delivered:
  "frame_hex": these bytes instead of the encrypted payload (a broken frame);
  "xtea_key": encrypt with this key (hex, 16 bytes) instead of the client's (wrong encryption);
  "inner": the frame's inner length (normally the payload's; bad padding, wrong lengths);
  "hold": true: keep the frame and send it together with the next one (coalesced frames);
  "pieces": [n, ...]: send the bytes in pieces of these sizes (repeated), "piece_delay" seconds
      apart (fragmented frames, slow reads);
  "cut": n: send only the first n bytes (of the held ones and this frame) and close the
      connection (a disconnect inside or between frames).
Proxy(..., refuse=("login",)) does not listen on that port (connection refused).

Virtual time (issue #23, tools/reference.py): Proxy(..., vclock=f) calls f() whenever it sends a
replayed server packet or closes a replayed connection and stores the result as the event's "vt"
(the client's virtual milliseconds then: patcap's clock, which stands still while the packet is on its
way); the server close events of a replay also get "replayed", the index of the recorded event.

The recording is a JSON-lines file, one object per event:
  {"t": seconds since the proxy started, "conn": connection number (from 1), "kind":
   "login"/"game", "dir": "c2s"/"s2c"/"close", "raw": the frame as it went over the wire
   (hex), "plain": the payload (hex), "pad": the XTEA padding (hex), ...}
The first client packet has "plain" = the frame body with the RSA block decrypted and
"key", "fields" (the RSA block's fields after the key) and "rsapad" (its random tail);
masked() gives the form the comparison uses. A "close" event names the side that closed.
s2c frames that the proxy changed (the character list) also have "orig" (the server's
payload). Run standalone, it records one live session: python -I tools/proxy.py out.jsonl
"""

import base64
import json
import os
import re
import socket
import struct
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import testserver  # noqa: E402

SERVER_HOST = "127.0.0.1"
SERVER_LOGIN_PORT = 7171
SERVER_GAME_PORT = 7172
PROXY_LOGIN_PORT = 7271
PROXY_GAME_PORT = 7272
DEFAULT_PEM = os.path.join(testserver.SERVER_DIR, "tibia.pem")  # the test server's (testserver.py, PATELLA_TESTSERVER)
OPCODE_LOGIN = 0x01
OPCODE_GAME = 0x0a
OPCODE_PING = 0x1e


class ProtocolError(Exception):
	pass


# --- RSA ---------------------------------------------------------------------------------

def _der(data, pos):
	tag = data[pos]
	length = data[pos + 1]
	pos += 2
	if length & 0x80:
		n = length & 0x7f
		length = int.from_bytes(data[pos:pos + n], "big")
		pos += n
	return tag, pos, pos + length


def _der_children(data, start, end):
	out = []
	while start < end:
		tag, body, after = _der(data, start)
		out.append(int.from_bytes(data[body:after], "big") if tag == 2 else None)
		start = after
	return out


def load_private_key(path=DEFAULT_PEM):
	"""(n, d) of a PKCS#1 'RSA PRIVATE KEY' PEM file; the file is only read."""
	with open(path, "r", encoding="ascii") as f:
		text = f.read()
	match = re.search(r"-----BEGIN RSA PRIVATE KEY-----(.*?)-----END RSA PRIVATE KEY-----", text, re.S)
	if not match:
		raise ProtocolError("%s: no RSA PRIVATE KEY block" % path)
	der = base64.b64decode("".join(match.group(1).split()))
	tag, body, end = _der(der, 0)
	version, n, e, d = _der_children(der, body, end)[:4]
	if e != 65537:
		raise ProtocolError("%s: public exponent %d, the client's is 65537" % (path, e))
	return n, d


def rsa_decrypt(block, key):
	n, d = key
	value = pow(int.from_bytes(block, "big"), d, n)
	return value.to_bytes(128, "big")


# --- XTEA (src/TXTEA.cpp) ----------------------------------------------------------------

DELTA = 0x9e3779b9
MASK = 0xffffffff


def xtea_key(key_bytes):
	return struct.unpack("<4I", key_bytes)


def xtea_decrypt(data, key):
	out = bytearray(len(data))
	for i in range(0, len(data), 8):
		v0, v1 = struct.unpack_from("<2I", data, i)
		total = (DELTA * 32) & MASK
		for _ in range(32):
			v1 = (v1 - ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (total + key[(total >> 11) & 3]))) & MASK
			total = (total - DELTA) & MASK
			v0 = (v0 - ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (total + key[total & 3]))) & MASK
		struct.pack_into("<2I", out, i, v0, v1)
	return bytes(out)


def xtea_encrypt(data, key):
	out = bytearray(len(data))
	for i in range(0, len(data), 8):
		v0, v1 = struct.unpack_from("<2I", data, i)
		total = 0
		for _ in range(32):
			v0 = (v0 + ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (total + key[total & 3]))) & MASK
			total = (total + DELTA) & MASK
			v1 = (v1 + ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (total + key[(total >> 11) & 3]))) & MASK
		struct.pack_into("<2I", out, i, v0, v1)
	return bytes(out)


def decrypt_frame(body, key):
	"""(payload, padding) of an encrypted frame body (the bytes after the u16 size)."""
	if len(body) % 8 != 0 or not body:
		raise ProtocolError("encrypted frame of %d bytes" % len(body))
	plain = xtea_decrypt(body, key)
	length = struct.unpack_from("<H", plain, 0)[0]
	if length + 2 > len(plain):
		raise ProtocolError("inner length %d in a frame of %d bytes" % (length, len(body)))
	return plain[2:2 + length], plain[2 + length:]


def encrypt_frame(payload, key, padding=None):
	plain = struct.pack("<H", len(payload)) + payload
	need = (-len(plain)) % 8
	if padding is None or len(padding) != need:
		padding = bytes(need)
	body = xtea_encrypt(plain + padding, key)
	return struct.pack("<H", len(body)) + body


# --- the first packet ----------------------------------------------------------------------

def read_string(data, pos):
	length = struct.unpack_from("<H", data, pos)[0]
	return data[pos + 2:pos + 2 + length], pos + 2 + length


def parse_first(body, rsa_key):
	"""The client's unencrypted first packet: kind, XTEA key and the decoded RSA block."""
	opcode = body[0]
	if opcode == OPCODE_LOGIN:
		kind, header = "login", 17
	elif opcode == OPCODE_GAME:
		kind, header = "game", 5
	else:
		raise ProtocolError("first packet with opcode 0x%02x" % opcode)
	if len(body) != header + 128:
		raise ProtocolError("%s packet of %d bytes, expected %d" % (kind, len(body), header + 128))
	block = rsa_decrypt(body[header:], rsa_key)
	if block[0] != 0:
		raise ProtocolError("RSA block does not start with 0 (wrong key?): %s" % block[:4].hex())
	key = block[1:17]
	pos = 17
	if kind == "login":
		account = struct.unpack_from("<I", block, pos)[0]
		pos += 4
		password, pos = read_string(block, pos)
		fields = {"account": account, "password": password.decode("latin-1")}
	else:
		gamemaster = block[pos]
		account = struct.unpack_from("<I", block, 18)[0]
		pos = 22
		name, pos = read_string(block, pos)
		password, pos = read_string(block, pos)
		fields = {"gamemaster": gamemaster, "account": account, "name": name.decode("latin-1"),
			"password": password.decode("latin-1")}
	return {"kind": kind, "header": body[:header], "block": block, "key": key, "fields_end": pos,
		"fields": fields}


def masked_first(body_hex, fields_end):
	"""The first packet with the XTEA key and the RSA block's random tail masked."""
	body = bytes.fromhex(body_hex)
	header = 17 if body[0] == OPCODE_LOGIN else 5
	block = body[header:]
	return "%s rsa[00 <key:16> %s <random:%d>]" % (body[:header].hex(), block[17:fields_end].hex(),
		128 - fields_end)


def rand_stream(event):
	"""True when the first packet's XTEA key and the RSA block's random tail are the low bytes of
	one run of MSVC rand() results, as SendLogin/SendGameLogin make them: rand() for the 16 key
	bytes, then rand() for all 128 bytes of the block, whose start the fields then overwrite.
	rand(): state = state * 214013 + 2531011, result (state >> 16) & 0x7fff; its low byte depends
	only on the state's low 24 bits, so the 16 unknown bits are searched."""
	known = {i: b for i, b in enumerate(bytes.fromhex(event["key"]))}
	tail = bytes.fromhex(event["rsapad"])
	start = 16 + 128 - len(tail)
	known.update({start + i: b for i, b in enumerate(tail)})
	last = max(known)
	for low in range(0x10000):
		state = (known[0] << 16) | low
		for i in range(1, last + 1):
			state = (state * 214013 + 2531011) & 0xffffff
			if i in known and (state >> 16) & 0xff != known[i]:
				break
		else:
			return True
	return False


def masked(event):
	"""What a c2s event is compared by."""
	if "fields_end" in event:
		return masked_first(event["plain"], event["fields_end"])
	return event["plain"]


# --- login server reply: the character list ------------------------------------------------

def rewrite_character_list(payload, old_port, new_port):
	"""The login server's reply with the game port old_port replaced by new_port (same length)."""
	out = bytearray(payload)
	pos = 0
	changed = 0
	while pos < len(payload):
		opcode = payload[pos]
		pos += 1
		if opcode in (0x0a, 0x0b, 0x14, 0x1e):  # error, error, message of the day, ...: a string
			text, pos = read_string(payload, pos)
		elif opcode == 0x64:
			count = payload[pos]
			pos += 1
			for _ in range(count):
				name, pos = read_string(payload, pos)
				world, pos = read_string(payload, pos)
				pos += 4
				port = struct.unpack_from("<H", payload, pos)[0]
				if port == old_port:
					struct.pack_into("<H", out, pos, new_port)
					changed += 1
				pos += 2
			pos += 2  # premium days
		else:
			break  # unknown: leave the rest alone
	return bytes(out), changed


# --- frames ------------------------------------------------------------------------------

def recv_exact(sock, count):
	data = b""
	while len(data) < count:
		chunk = sock.recv(count - len(data))
		if not chunk:
			return None
		data += chunk
	return data


def recv_frame(sock):
	head = recv_exact(sock, 2)
	if head is None:
		return None
	size = struct.unpack("<H", head)[0]
	body = recv_exact(sock, size)
	if body is None:
		return None
	return head + body


# --- the proxy ---------------------------------------------------------------------------

class Connection:
	def __init__(self, proxy, number, client):
		self.proxy = proxy
		self.number = number
		self.client = client
		self.server = None
		self.kind = None
		self.key = None
		self.closed = False
		self.lock = threading.Lock()
		self.pending = b""  # replay: held frames (coalescing)


class Proxy:
	"""Live recording (replay=None) or replay of a recorded session (replay=list of events)."""

	def __init__(self, record_path, rsa_key=None, replay=None, login_port=PROXY_LOGIN_PORT,
			game_port=PROXY_GAME_PORT, log=None, refuse=(), vclock=None):
		self.record_path = record_path
		self.vclock = vclock
		self.refuse = refuse
		self.rsa_key = rsa_key or load_private_key()
		self.replay = replay
		self.login_port = login_port
		self.game_port = game_port
		self.log = log or (lambda text: None)
		self.events = []
		self.errors = []
		self.mismatches = []
		self.start = time.time()
		self.lock = threading.Lock()
		self.counter = 0
		self.kind_counter = {"login": 0, "game": 0}
		self.listeners = []
		self.threads = []
		self.stopping = False
		self.busy = set()
		self.c2s_count = 0  # replay: connections with server packets still to send
		self.active = 0
		self.last_s2c = time.time()
		if replay is not None:
			self.replay_conns = {}
			for event in replay:
				key = (event["kind"], event["conn_ordinal"])
				self.replay_conns.setdefault(key, []).append(event)

	# events

	def now(self):
		return round(time.time() - self.start, 4)

	def record(self, event):
		with self.lock:
			self.events.append(event)
			if event["dir"] == "c2s":
				self.c2s_count += 1

	def error(self, text):
		self.log("proxy: " + text)
		with self.lock:
			self.errors.append(text)

	# lifecycle

	def __enter__(self):
		for kind, port in (("login", self.login_port), ("game", self.game_port)):
			if kind in self.refuse:
				continue
			sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			sock.bind(("127.0.0.1", port))
			sock.listen(4)
			self.listeners.append(sock)
			thread = threading.Thread(target=self.accept_loop, args=(sock,), daemon=True)
			thread.start()
		return self

	def __exit__(self, *exc):
		self.close()

	def close(self):
		self.stopping = True
		for sock in self.listeners:
			try:
				sock.close()
			except OSError:
				pass
		self.listeners = []
		for thread in list(self.threads):
			thread.join(5)
		if self.record_path:
			with open(self.record_path, "w", encoding="utf-8", newline="\n") as f:
				for event in self.events:
					f.write(json.dumps(event, sort_keys=True) + "\n")

	def accept_loop(self, listener):
		while not self.stopping:
			try:
				client, address = listener.accept()
			except OSError:
				return
			with self.lock:
				self.counter += 1
				number = self.counter
				self.active += 1
			client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
			conn = Connection(self, number, client)
			if self.replay is not None:
				self.set_busy(conn, True)
			target = self.replay_connection if self.replay is not None else self.live_connection
			thread = threading.Thread(target=target, args=(conn,), daemon=True)
			self.threads.append(thread)
			thread.start()

	def wait_idle(self, quiet=0.6, timeout=30):
		"""Live: until no server packet came for `quiet` seconds. Replay: until every recorded server
		packet before the next expected client packet has been sent, then `quiet` seconds."""
		deadline = time.time() + timeout
		if self.replay is not None:
			# busy until the client's next packet is due; a client packet that comes during the
			# quiet time (the answer to the action just taken) starts the wait again
			while time.time() < deadline:
				while time.time() < deadline and self.busy:
					time.sleep(0.02)
				count = self.c2s_count
				time.sleep(quiet)
				if not self.busy and self.c2s_count == count:
					return True
			return False
		while time.time() < deadline:
			if time.time() - self.last_s2c >= quiet:
				return True
			time.sleep(0.05)
		return False

	# first packet, common to both modes

	def first_packet(self, conn, frame):
		body = frame[2:]
		info = parse_first(body, self.rsa_key)
		conn.kind = info["kind"]
		conn.key = xtea_key(info["key"])
		with self.lock:
			self.kind_counter[conn.kind] += 1
			conn.ordinal = self.kind_counter[conn.kind]
		plain = info["header"] + info["block"]
		event = {"t": self.now(), "conn": conn.number, "kind": conn.kind, "conn_ordinal": conn.ordinal,
			"dir": "c2s", "raw": frame.hex(), "plain": plain.hex(), "key": info["key"].hex(),
			"fields": info["fields"], "fields_end": info["fields_end"],
			"rsapad": info["block"][info["fields_end"]:].hex()}
		self.record(event)
		self.log("proxy: %s connection %d, account %s" % (conn.kind, conn.number, info["fields"].get("account")))
		return event

	def c2s_event(self, conn, frame):
		payload, padding = decrypt_frame(frame[2:], conn.key)
		event = {"t": self.now(), "conn": conn.number, "kind": conn.kind, "conn_ordinal": conn.ordinal,
			"dir": "c2s", "raw": frame.hex(), "plain": payload.hex(), "pad": padding.hex()}
		self.record(event)
		return event

	# live

	def live_connection(self, conn):
		try:
			frame = recv_frame(conn.client)
			if frame is None:
				return
			self.first_packet(conn, frame)
			port = SERVER_LOGIN_PORT if conn.kind == "login" else SERVER_GAME_PORT
			conn.server = socket.create_connection((SERVER_HOST, port), timeout=10)
			conn.server.settimeout(None)
			conn.server.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
			conn.server.sendall(frame)
			pump = threading.Thread(target=self.live_s2c, args=(conn,), daemon=True)
			pump.start()
			while True:
				frame = recv_frame(conn.client)
				if frame is None:
					self.close_conn(conn, "client")
					break
				self.c2s_event(conn, frame)
				conn.server.sendall(frame)
			pump.join(10)
		except (OSError, ProtocolError) as e:
			if not conn.closed:
				self.error("connection %d: %s" % (conn.number, e))
				self.close_conn(conn, "error")
		finally:
			with self.lock:
				self.active -= 1

	def live_s2c(self, conn):
		try:
			while True:
				frame = recv_frame(conn.server)
				if frame is None:
					self.close_conn(conn, "server")
					return
				payload, padding = decrypt_frame(frame[2:], conn.key)
				event = {"t": self.now(), "conn": conn.number, "kind": conn.kind,
					"conn_ordinal": conn.ordinal, "dir": "s2c", "raw": frame.hex(), "plain": payload.hex(),
					"pad": padding.hex()}
				if conn.kind == "login":
					new, changed = rewrite_character_list(payload, SERVER_GAME_PORT, self.game_port)
					if changed:
						frame = encrypt_frame(new, conn.key, padding)
						event["orig"] = event["plain"]
						event["plain"] = new.hex()
						event["raw_sent"] = frame.hex()
				self.record(event)
				self.last_s2c = time.time()
				conn.client.sendall(frame)
		except (OSError, ProtocolError) as e:
			if not conn.closed:
				self.error("connection %d (server side): %s" % (conn.number, e))
				self.close_conn(conn, "error")

	def close_conn(self, conn, side, extra=None):
		with conn.lock:
			if conn.closed:
				return
			conn.closed = True
		self.record(dict({"t": self.now(), "conn": conn.number, "kind": conn.kind,
			"conn_ordinal": getattr(conn, "ordinal", 0), "dir": "close", "side": side}, **(extra or {})))
		for sock in (conn.client, conn.server):
			if sock is not None:
				try:
					sock.shutdown(socket.SHUT_RDWR)
				except OSError:
					pass
				try:
					sock.close()
				except OSError:
					pass

	# replay

	def set_busy(self, conn, value):
		"""A replayed connection is busy from its accept until it waits for the next client packet
		(and again from that packet on), so wait_idle() returns only when every recorded server
		packet before the client's next action has been sent."""
		with self.lock:
			if value:
				self.busy.add(conn.number)
			else:
				self.busy.discard(conn.number)

	def replay_connection(self, conn):
		try:
			frame = recv_frame(conn.client)
			if frame is None:
				return
			event = self.first_packet(conn, frame)
			script = self.replay_conns.get((conn.kind, conn.ordinal))
			if not script:
				self.error("replay: no recorded %s connection %d" % (conn.kind, conn.ordinal))
				self.close_conn(conn, "error")
				return
			if script[0]["dir"] != "c2s" or "fields_end" not in script[0]:
				self.error("replay: recorded %s connection %d does not start with the login packet"
					% (conn.kind, conn.ordinal))
			self.check(conn, script[0], event)
			received = []
			reader = threading.Thread(target=self.replay_reader, args=(conn, received), daemon=True)
			reader.start()
			trigger_time = script[0]["t"]
			trigger_real = time.time()
			index = 1
			while index < len(script):
				recorded = script[index]
				if recorded["dir"] == "s2c":
					delay = (recorded["t"] - trigger_time) - (time.time() - trigger_real)
					if delay > 0:
						time.sleep(delay)
					frame = self.build_frame(conn, recorded)
					event = {"t": self.now(), "conn": conn.number, "kind": conn.kind,
						"conn_ordinal": conn.ordinal, "dir": "s2c", "raw": frame.hex(), "plain": recorded.get("plain", ""),
						"pad": "", "replayed": index}
					if self.vclock is not None:
						event["vt"] = self.vclock()
					self.record(event)
					self.deliver(conn, frame, recorded)
					self.last_s2c = time.time()
					index += 1
					if conn.closed:
						break
				elif recorded["dir"] == "c2s":
					self.set_busy(conn, False)
					got = self.wait_received(conn, received, 60)
					self.set_busy(conn, True)
					if got is None:
						self.error("replay: %s connection %d: the client sent nothing where the recording has %s"
							% (conn.kind, conn.ordinal, recorded["plain"][:40]))
						break
					self.check(conn, recorded, got)
					trigger_time = recorded["t"]
					trigger_real = time.time()
					index += 1
				else:  # close
					if recorded["side"] == "server":
						delay = (recorded["t"] - trigger_time) - (time.time() - trigger_real)
						if delay > 0:
							time.sleep(delay)
						extra = {"replayed": index}
						if self.vclock is not None:
							extra["vt"] = self.vclock()
						self.close_conn(conn, "server", extra)
					else:
						self.set_busy(conn, False)
						got = self.wait_received(conn, received, 60)
						if got is not None:
							self.check(conn, recorded, got)
					index += 1
					break
			self.set_busy(conn, False)
			reader.join(30)
			# client packets after the end of the recording
			while received:
				extra = received.pop(0)
				if extra != "closed":
					self.mismatch(conn, None, extra)
		except (OSError, ProtocolError) as e:
			if not conn.closed:
				self.error("replay connection %d: %s" % (conn.number, e))
				self.close_conn(conn, "error")
		finally:
			self.set_busy(conn, False)
			with self.lock:
				self.active -= 1

	def build_frame(self, conn, event):
		"""The frame of a replayed s2c event (the crafted forms: see the module's text)."""
		if "frame_hex" in event:
			return bytes.fromhex(event["frame_hex"])
		payload = bytes.fromhex(event["plain"])
		key = xtea_key(bytes.fromhex(event["xtea_key"])) if "xtea_key" in event else conn.key
		if "inner" in event:
			plain = struct.pack("<H", event["inner"] & 0xffff) + payload
			plain += bytes((-len(plain)) % 8)
			body = xtea_encrypt(plain, key)
			return struct.pack("<H", len(body)) + body
		return encrypt_frame(payload, key)

	def deliver(self, conn, frame, event):
		"""Sends a frame as the event says: held for the next one, in pieces, or cut off with a close."""
		data = conn.pending + frame
		conn.pending = b""
		if event.get("hold"):
			conn.pending = data
			return
		cut = event.get("cut")
		if cut is not None:
			data = data[:cut]
		pieces = event.get("pieces")
		if pieces:
			position = 0
			i = 0
			while position < len(data):
				size = max(1, pieces[i % len(pieces)])
				conn.client.sendall(data[position:position + size])
				position += size
				i += 1
				if position < len(data):
					time.sleep(event.get("piece_delay", 0.0))
		elif data:
			conn.client.sendall(data)
		if cut is not None:
			time.sleep(0.2)
			self.close_conn(conn, "server")

	def replay_reader(self, conn, received):
		try:
			while True:
				frame = recv_frame(conn.client)
				if frame is None:
					self.close_conn(conn, "client")
					received.append("closed")
					return
				received.append(self.c2s_event(conn, frame))
		except (OSError, ProtocolError) as e:
			if not conn.closed:
				self.error("replay connection %d (reader): %s" % (conn.number, e))
			received.append("closed")

	def wait_received(self, conn, received, timeout):
		deadline = time.time() + timeout
		while time.time() < deadline:
			if received:
				return received.pop(0)
			time.sleep(0.01)
		return None

	def check(self, conn, recorded, got):
		if recorded["dir"] == "close":
			if got != "closed":
				self.mismatch(conn, recorded, got)
			return
		if got == "closed" or masked(recorded) != masked(got):
			self.mismatch(conn, recorded, got)

	def mismatch(self, conn, recorded, got):
		text = "%s connection %d: recorded %s, client sent %s" % (conn.kind, conn.ordinal,
			"nothing" if recorded is None else (masked(recorded) if recorded["dir"] == "c2s" else "close"),
			got if got == "closed" else masked(got))
		self.log("proxy: MISMATCH " + text)
		with self.lock:
			self.mismatches.append(text)


def load_events(path):
	with open(path, "r", encoding="utf-8") as f:
		return [json.loads(line) for line in f if line.strip()]


def main():
	import argparse
	parser = argparse.ArgumentParser(description="Record one live session through the proxy.")
	parser.add_argument("output")
	parser.add_argument("--pem", default=DEFAULT_PEM, help="the server's private key (default %(default)s)")
	parser.add_argument("--replay", default=None, help="a recording to replay instead of a live server")
	args = parser.parse_args()
	replay = load_events(args.replay) if args.replay else None
	with Proxy(args.output, load_private_key(args.pem), replay=replay, log=print) as proxy:
		print("listening on 127.0.0.1:%d (login) and :%d (game); Ctrl+C to stop" % (proxy.login_port, proxy.game_port))
		try:
			while True:
				time.sleep(1)
		except KeyboardInterrupt:
			pass
	print("%d events written to %s" % (len(proxy.events), args.output))
	return 0


if __name__ == "__main__":
	sys.exit(main())
