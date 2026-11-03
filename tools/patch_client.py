"""Point a copy of the client at another login server, like an "IP changer" (issue #18).

Usage: python -I tools/patch_client.py <in.exe> <out.exe> --host 127.0.0.1 [--port 7171] [--rsa <modulus|pem>]

Works on Tibia.exe and on Patella.exe alike, x86 (PE32) and x64 (PE32+; Patella-Modern issue #15):
everything is found by searching the file, no fixed offsets. The input is only read; the output
must be a different file.

- The four login server names (server.tibia.com, server2.tibia.com, tibia1.cipsoft.com,
  tibia2.cipsoft.com: InitServerList in src/Network.cpp) are overwritten in place by --host,
  NUL-padded to their own length. InitServerList copies each literal with a fixed number of
  bytes (inlined strcpy), so a shorter name followed by NULs is read as the shorter name.
  The test server name (test.cipsoft.com, port 7172: Player.cpp) is left alone.
- --port changes the immediate 7171 that InitServerList stores in each server's port, found
  as `03 1c 00 00` in the 0x200 code bytes after the first reference to the server.tibia.com
  literal: once in Tibia.exe (the loop's `mov ecx, 7171`), four times in Patella.exe (its
  compiler unrolls the loop: `mov dword ptr [Servers+i], 7171`, RIP-relative on x64); any
  other count is an error. Without --port, or with 7171, nothing is changed. A reference is
  the literal's absolute address on x86; on x64 it is a RIP-relative disp32 after a ModRM byte
  with mod 00, r/m 101 (the strcpy's `movups xmm0, [rip+disp32]`), whose target, the address
  after the disp32 plus the disp32, is inside the literal (an instruction with an immediate
  after its disp32 would have another target; InitServerList's loads have none).
- --rsa replaces the decimal text of CipSoft's RSA modulus (TRSA::SetPublicKey's argument
  in src/Communication.cpp, one literal used by both the login and the game connection).
  The argument is the decimal modulus or a PEM file (an RSA private or public key, PKCS#1 or
  X.509 SubjectPublicKeyInfo). The new text must not be longer than the old one (309
  digits); a shorter one is NUL-padded. The public exponent is 65537 in the client and must
  be the key's too.

After writing, the output is compared with the input byte by byte: every changed byte must
be inside one of the patched ranges, or the script fails (and deletes the output). Exit
code 0 on success, 1 on any error.
"""

import argparse
import base64
import os
import re
import struct
import sys

LOGIN_SERVERS = [b"server.tibia.com", b"server2.tibia.com", b"tibia1.cipsoft.com", b"tibia2.cipsoft.com"]

# CipSoft's public modulus as compiled into Tibia.exe (src/Communication.cpp)
CIPSOFT_MODULUS = (
	"1247104594268279430043764498979855821678017079606970371640449048629485693808504213969045976869538770"
	"2239460423942818549828416906858180227761208102796672433631944853781144171907648434092285492927351730"
	"8661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284"
	"902061793"
)
CLIENT_EXPONENT = 65537


class PatchError(Exception):
	pass


class Image:
	def __init__(self, data):
		self.data = data
		pe = struct.unpack_from("<I", data, 0x3c)[0]
		if data[pe:pe + 4] != b"PE\0\0":
			raise PatchError("not a PE file")
		count = struct.unpack_from("<H", data, pe + 6)[0]
		optsize = struct.unpack_from("<H", data, pe + 20)[0]
		opt = pe + 24
		magic = struct.unpack_from("<H", data, opt)[0]
		if magic == 0x10b:  # PE32: x86
			self.x64 = False
			self.base = struct.unpack_from("<I", data, opt + 28)[0]
		elif magic == 0x20b:  # PE32+: x64, a 64-bit ImageBase at +24
			self.x64 = True
			self.base = struct.unpack_from("<Q", data, opt + 24)[0]
		else:
			raise PatchError("optional header magic 0x%x: neither PE32 nor PE32+" % magic)
		self.sections = []
		for i in range(count):
			o = opt + optsize + 40 * i
			name = data[o:o + 8].rstrip(b"\0").decode("latin-1")
			vsize, va, rsize, raw = struct.unpack_from("<IIII", data, o + 8)
			chars = struct.unpack_from("<I", data, o + 36)[0]
			self.sections.append((name, va, vsize, raw, rsize, chars))

	def offset_to_va(self, offset):
		for name, va, vsize, raw, rsize, chars in self.sections:
			if raw <= offset < raw + rsize:
				return self.base + va + offset - raw
		raise PatchError("file offset 0x%x is in no section" % offset)

	def code_ranges(self):
		# IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE
		return [(raw, raw + min(rsize, vsize)) for name, va, vsize, raw, rsize, chars in self.sections
			if chars & 0x20000020]


def read_der(data, pos):
	tag = data[pos]
	length = data[pos + 1]
	pos += 2
	if length & 0x80:
		n = length & 0x7f
		length = int.from_bytes(data[pos:pos + n], "big")
		pos += n
	return tag, pos, pos + length


def der_children(data, start, end):
	children = []
	while start < end:
		tag, body, after = read_der(data, start)
		children.append((tag, body, after))
		start = after
	return children


def modulus_from_pem(path):
	with open(path, "r", encoding="ascii") as f:
		text = f.read()
	match = re.search(r"-----BEGIN ([A-Z ]+)-----(.*?)-----END \1-----", text, re.S)
	if not match:
		raise PatchError("%s: no PEM block" % path)
	kind = match.group(1)
	der = base64.b64decode("".join(match.group(2).split()))
	tag, body, end = read_der(der, 0)
	items = der_children(der, body, end)
	if kind == "RSA PRIVATE KEY":
		# RSAPrivateKey: version, n, e, d, p, q, ...
		n, e = items[1], items[2]
	elif kind == "RSA PUBLIC KEY":
		n, e = items[0], items[1]
	elif kind == "PUBLIC KEY":
		# SubjectPublicKeyInfo: algorithm, BIT STRING (0 unused bits) holding RSAPublicKey
		tag, body, after = items[1]
		tag, inner, inner_end = read_der(der, body + 1)
		n, e = der_children(der, inner, inner_end)[:2]
	elif kind == "PRIVATE KEY":
		# PKCS#8: version, algorithm, OCTET STRING holding RSAPrivateKey
		tag, body, after = items[2]
		tag, inner, inner_end = read_der(der, body)
		fields = der_children(der, inner, inner_end)
		n, e = fields[1], fields[2]
	else:
		raise PatchError("%s: unsupported PEM block %s" % (path, kind))
	modulus = int.from_bytes(der[n[1]:n[2]], "big")
	exponent = int.from_bytes(der[e[1]:e[2]], "big")
	return modulus, exponent


def parse_rsa(value):
	if os.path.isfile(value):
		modulus, exponent = modulus_from_pem(value)
		if exponent != CLIENT_EXPONENT:
			raise PatchError("the key's public exponent is %d, the client's is %d" % (exponent, CLIENT_EXPONENT))
		return str(modulus)
	if not value.isdigit():
		raise PatchError("--rsa: neither a file nor a decimal number: %s" % value)
	return value.lstrip("0") or "0"


def find_unique(data, needle, what):
	hits = [m.start() for m in re.finditer(re.escape(needle), data)]
	if len(hits) != 1:
		raise PatchError("%s: expected one occurrence, found %d" % (what, len(hits)))
	return hits[0]


def first_reference(image, data, lo, hi):
	"""The file offset of the first code reference to an address in [lo, hi]: x86 an absolute
	address, x64 a RIP-relative disp32 (ModRM mod 00, r/m 101 before it) whose target is there."""
	for start, end in image.code_ranges():
		if not image.x64:
			for pos in range(start, end - 3):
				value = struct.unpack_from("<I", data, pos)[0]
				if lo <= value <= hi:
					return pos
			continue
		for pos in range(max(start, 1), end - 3):
			if data[pos - 1] & 0xc7 != 0x05:
				continue
			target = image.offset_to_va(pos) + 4 + struct.unpack_from("<i", data, pos)[0]
			if lo <= target <= hi:
				return pos
	return None


def find_port(image, data, anchor_offset, anchor_len):
	# the first code reference to an address inside the server.tibia.com literal
	lo = image.offset_to_va(anchor_offset)
	hi = lo + anchor_len
	first = first_reference(image, data, lo, hi)
	if first is None:
		raise PatchError("no code reference to server.tibia.com: InitServerList not found")
	window = data[first:first + 0x200]
	hits = [first + m.start() for m in re.finditer(re.escape(struct.pack("<I", 7171)), window)]
	if len(hits) not in (1, len(LOGIN_SERVERS)):
		raise PatchError("port 7171: expected 1 or %d immediates after 0x%x, found %d"
			% (len(LOGIN_SERVERS), first, len(hits)))
	return hits


def patch(data, host, port, rsa):
	image = Image(data)
	out = bytearray(data)
	ranges = []
	host_bytes = host.encode("ascii")
	anchor = None
	for name in LOGIN_SERVERS:
		offset = find_unique(data, name + b"\0", name.decode())
		if len(host_bytes) > len(name):
			raise PatchError("--host %s is longer than %s (%d characters)" % (host, name.decode(), len(name)))
		out[offset:offset + len(name)] = host_bytes.ljust(len(name), b"\0")
		ranges.append((offset, offset + len(name), "%s -> %s" % (name.decode(), host)))
		if anchor is None:
			anchor = (offset, len(name))
	if port is not None and port != 7171:
		if not 0 < port < 65536:
			raise PatchError("--port %d out of range" % port)
		for offset in find_port(image, data, *anchor):
			out[offset:offset + 4] = struct.pack("<I", port)
			ranges.append((offset, offset + 4, "login port 7171 -> %d" % port))
	if rsa is not None:
		old = CIPSOFT_MODULUS.encode("ascii")
		new = rsa.encode("ascii")
		offset = find_unique(data, b"\0" + old + b"\0", "the RSA modulus") + 1
		if len(new) > len(old):
			raise PatchError("the new modulus has %d digits, more than the client's %d" % (len(new), len(old)))
		if new != old:
			out[offset:offset + len(old)] = new.ljust(len(old), b"\0")
			ranges.append((offset, offset + len(old), "RSA modulus (%d digits) -> %s...%s (%d digits)"
				% (len(old), rsa[:8], rsa[-8:], len(new))))
	return image, bytes(out), ranges


def verify(image, original, patched, ranges):
	if len(original) != len(patched):
		raise PatchError("size changed")
	changed = [i for i in range(len(original)) if original[i] != patched[i]]
	for i in changed:
		if not any(start <= i < end for start, end, what in ranges):
			raise PatchError("byte 0x%x changed outside the patched ranges" % i)
	return changed


def main():
	parser = argparse.ArgumentParser(description="Point a copy of the client at another login server.")
	parser.add_argument("input")
	parser.add_argument("output")
	parser.add_argument("--host", required=True, help="login server name or IPv4 address")
	parser.add_argument("--port", type=int, default=None, help="login server port (default 7171, unchanged)")
	parser.add_argument("--rsa", default=None, help="decimal modulus, or a PEM file with the server's key")
	args = parser.parse_args()

	if os.path.exists(args.output) and os.path.samefile(args.input, args.output):
		print("patch_client.py: the output must not be the input", file=sys.stderr)
		return 1
	try:
		with open(args.input, "rb") as f:
			original = f.read()
		rsa = parse_rsa(args.rsa) if args.rsa is not None else None
		image, patched, ranges = patch(original, args.host, args.port, rsa)
		with open(args.output, "wb") as f:
			f.write(patched)
		with open(args.output, "rb") as f:
			written = f.read()
		try:
			changed = verify(image, original, written, ranges)
		except PatchError:
			os.remove(args.output)
			raise
	except (PatchError, OSError) as e:
		print("patch_client.py: %s" % e, file=sys.stderr)
		return 1
	for start, end, what in ranges:
		print("0x%06x-0x%06x (VA 0x%08x): %s" % (start, end - 1, image.offset_to_va(start), what))
	print("%d bytes changed, all inside the patched ranges; wrote %s" % (len(changed), args.output))
	return 0


if __name__ == "__main__":
	sys.exit(main())
