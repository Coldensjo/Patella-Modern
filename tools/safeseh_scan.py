"""Find the x86 exception frames whose handler is missing from the image's SafeSEH table (issue #89).

Usage: python -I tools/safeseh_scan.py [-v] [--no-symbols] <image> [<image> ...]
       python -I tools/safeseh_scan.py --self-test

On x86 an exception handler is registered at run time: the function pushes it into a record of the fs:[0]
chain. With SafeSEH (/SAFESEH) Windows calls a handler of an image only when it is in the image's table
(IMAGE_LOAD_CONFIG_DIRECTORY32's SEHandlerTable and SEHandlerCount); any other ends the process
(STATUS_INVALID_EXCEPTION_HANDLER) when an exception passes the frame. MSVC x86 can keep a function's
exception frame and leave its handler out of the object's .sxdata (issue #79: TPlayerStates::GetIcon, after an
unreachable TERROR1 was removed), so the link puts it in no table. This scans the executable sections for the
registrations MSVC and its CRT emit, each read from the instruction that loads fs:[0]:

- C++ EH and the inline SEH3 frame: push -1 (6A FF); push __ehhandler$<function> (68 imm32); and with SEH3 a
  second push imm32, the handler after the scope table; then the fs:[0] load (mov reg, fs:[0] or push fs:[0]).
- The inline SEH4 frame: push -2 (6A FE); push <scope table>; push _except_handler4; the fs:[0] load.
- push <handler>; push fs:[0] (68 imm32, 64 FF 35 00000000): __SEH_prolog4 and hand-written frames.
- mov [ebp+d], <handler> (C7 45 d8 imm32 / C7 85 d32 imm32) shortly before the load: the CRT's guard frames
  (__CatchGuardHandler, __TranslatorGuardHandler).
- mov eax, <handler>; call __EH_prolog3 and the other prolog helpers (B8 imm32, E8 rel32 to a function that
  starts push eax; push fs:[0], or push -1; push eax; mov eax, fs:[0], or builds the aligned frame of
  __EH_prolog3_align and __EH_prolog3_GS_align before its load): MFC's and the CRT's frames.

A handler must lie in an executable section, or the bytes are not taken as a registration. Each fs:[0] load no
pattern explains (walks of the chain, the helpers' own loads) is counted as "other" (-v lists them). An image
with a table is checked against it; one without (linked /SAFESEH:NO, or no load config) is reported as such and
fails, and so does one with IMAGE_DLLCHARACTERISTICS_NO_SEH that registers a handler. Function names come from the
image's PDB (its CodeView record, then <image>.pdb) or <image>.map, when present and matching; otherwise the RVA.

Exit status: 0 every image clean, 1 a finding or an image without a SafeSEH table, 2 an image that cannot be
read (not a PE32 x86 image) or a failed self-test. Standard library only.
"""

import os
import re
import struct
import sys
import tempfile

sys.dont_write_bytecode = True  # no tools/__pycache__

IMAGE_FILE_MACHINE_I386 = 0x14C
IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_DLLCHARACTERISTICS_NO_SEH = 0x0400
DIRECTORY_DEBUG = 6
DIRECTORY_LOAD_CONFIG = 10
DEBUG_TYPE_CODEVIEW = 2
# IMAGE_LOAD_CONFIG_DIRECTORY32: SEHandlerTable at 0x40, SEHandlerCount at 0x44.
LOAD_CONFIG_SEH_END = 0x48


class ScanError(Exception):
	pass


# ---------------------------------------------------------------------------------------------------
# The PE32 image


class Image:
	def __init__(self, path, data):
		self.path = path
		self.data = data
		if len(data) < 0x40 or data[:2] != b"MZ":
			raise ScanError("not a PE image (no MZ header)")
		pe = self.u32(0x3C)
		if data[pe:pe + 4] != b"PE\0\0":
			raise ScanError("not a PE image (no PE signature)")
		machine, section_count = struct.unpack_from("<HH", data, pe + 4)
		optional_size = self.u16(pe + 20)
		optional = pe + 24
		magic = self.u16(optional)
		if magic != 0x10B or machine != IMAGE_FILE_MACHINE_I386:
			raise ScanError("not a PE32 x86 image (machine 0x%x, optional header magic 0x%x): SafeSEH is x86 only"
				% (machine, magic))
		self.image_base = self.u32(optional + 28)
		self.dll_characteristics = self.u16(optional + 70)
		directory_count = self.u32(optional + 92)
		self.directories = []
		for i in range(min(directory_count, 16)):
			self.directories.append(struct.unpack_from("<II", data, optional + 96 + 8 * i))
		self.sections = []
		for i in range(section_count):
			o = optional + optional_size + 40 * i
			name = data[o:o + 8].rstrip(b"\0").decode("latin-1")
			vsize, va, rsize, rptr = struct.unpack_from("<IIII", data, o + 8)
			flags = self.u32(o + 36)
			self.sections.append((name, va, vsize, rptr, rsize, flags))

	def u16(self, offset):
		return struct.unpack_from("<H", self.data, offset)[0]

	def u32(self, offset):
		return struct.unpack_from("<I", self.data, offset)[0]

	def directory(self, index):
		return self.directories[index] if index < len(self.directories) else (0, 0)

	def section_of(self, rva):
		for section in self.sections:
			name, va, vsize, rptr, rsize, flags = section
			if va <= rva < va + max(vsize, rsize):
				return section
		return None

	def offset_of(self, rva, size=1):
		section = self.section_of(rva)
		if section is None:
			raise ScanError("RVA 0x%x is in no section" % rva)
		name, va, vsize, rptr, rsize, flags = section
		if rva - va + size > rsize:
			raise ScanError("RVA 0x%x is beyond the raw data of %s" % (rva, name))
		return rptr + rva - va

	def is_code(self, rva):
		section = self.section_of(rva)
		return section is not None and bool(section[5] & IMAGE_SCN_MEM_EXECUTE) and rva < section[1] + section[2]

	def code_sections(self):
		for name, va, vsize, rptr, rsize, flags in self.sections:
			if flags & IMAGE_SCN_MEM_EXECUTE:
				yield name, va, self.data[rptr:rptr + min(vsize, rsize)]

	def safeseh_table(self):
		"""(state, set of handler RVAs): state 'table', 'no-load-config', 'no-table' or 'short-load-config'."""
		rva, size = self.directory(DIRECTORY_LOAD_CONFIG)
		if rva == 0 or size == 0:
			return "no-load-config", set()
		offset = self.offset_of(rva, 4)
		struct_size = self.u32(offset)
		if struct_size < LOAD_CONFIG_SEH_END:
			return "short-load-config", set()
		offset = self.offset_of(rva, LOAD_CONFIG_SEH_END)
		table_va, count = struct.unpack_from("<II", self.data, offset + 0x40)
		if table_va == 0:
			return "no-table", set()
		if table_va < self.image_base:
			raise ScanError("SEHandlerTable 0x%x is below the image base 0x%x" % (table_va, self.image_base))
		table = set()
		if count:
			start = self.offset_of(table_va - self.image_base, 4 * count)
			table.update(struct.unpack_from("<%dI" % count, self.data, start))
		return "table", table

	def codeview_pdb(self):
		"""(path, GUID bytes) of the image's RSDS record, or None."""
		rva, size = self.directory(DIRECTORY_DEBUG)
		if rva == 0 or size == 0:
			return None
		try:
			offset = self.offset_of(rva, size)
		except ScanError:
			return None
		for i in range(size // 28):
			entry = offset + 28 * i
			kind, data_size, data_rva, data_ptr = struct.unpack_from("<IIII", self.data, entry + 12)
			if kind != DEBUG_TYPE_CODEVIEW or data_size < 24 or data_ptr + data_size > len(self.data):
				continue
			record = self.data[data_ptr:data_ptr + data_size]
			if record[:4] != b"RSDS":
				continue
			path = record[24:].split(b"\0", 1)[0].decode("utf-8", "replace")
			return path, record[4:20]
		return None


# ---------------------------------------------------------------------------------------------------
# The registrations

FS0_LOAD = re.compile(rb"\x64(?:\xa1|\x8b[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]|\xff\x35)\x00\x00\x00\x00", re.DOTALL)
PROLOG_HELPER_STARTS = (b"\x50\x64\xff\x35\x00\x00\x00\x00", b"\x6a\xff\x50\x64\xa1\x00\x00\x00\x00")
# push ecx; mov ecx, [esp+0Ch]; mov [esp+0Ch], ebx; lea ebx, [esp+0Ch]; push eax
PROLOG_ALIGN_START = b"\x51\x8b\x4c\x24\x0c\x89\x5c\x24\x0c\x8d\x5c\x24\x0c\x50"
PROLOG_ALIGN_LOOKBACK = 0x80
GUARD_LOOKBACK = 128


def guard_registration(text, va, at, code_rva):
	"""A record built in the frame: mov reg, fs:[0]; mov [ebp+d], reg, after mov [ebp+d+4], <handler>."""
	if text[at + 1] == 0xA1:
		register, end = 0, at + 6
	elif text[at + 1] == 0x8B:
		register, end = (text[at + 2] >> 3) & 7, at + 7
	else:
		return None
	if end + 3 > len(text) or text[end] != 0x89:
		return None
	modrm = text[end + 1]
	if modrm == 0x45 | register << 3:
		displacement = struct.unpack_from("<b", text, end + 2)[0]
	elif modrm == 0x85 | register << 3 and end + 6 <= len(text):
		displacement = struct.unpack_from("<i", text, end + 2)[0]
	else:
		return None
	handler_at = displacement + 4
	if -128 <= handler_at <= 127:
		store = b"\xc7\x45" + struct.pack("<b", handler_at)
	else:
		store = b"\xc7\x85" + struct.pack("<i", handler_at)
	i = text.rfind(store, max(0, at - GUARD_LOOKBACK), at)
	if i < 0 or i + len(store) + 4 > at:
		return None
	handler = code_rva(struct.unpack_from("<I", text, i + len(store))[0])
	return None if handler is None else Registration(va + i, handler, "guard")


class Registration:
	def __init__(self, site, handler, kind):
		self.site = site  # RVA of the instruction that starts the registration
		self.handler = handler  # RVA of the handler
		self.kind = kind


def find_registrations(image):
	"""(registrations, RVAs of the fs:[0] loads no pattern explains)."""
	base = image.image_base

	def code_rva(va):
		rva = va - base
		return rva if va >= base and image.is_code(rva) else None

	registrations = []
	others = []
	helpers = set()
	for name, va, text in image.code_sections():
		for match in FS0_LOAD.finditer(text):
			at = match.start()
			found = None
			# push -1 / -2; push imm32 [; push imm32]; the load
			if at >= 12 and text[at - 12:at - 10] in (b"\x6a\xff", b"\x6a\xfe") and text[at - 10] == 0x68 \
					and text[at - 5] == 0x68:
				handler = code_rva(struct.unpack_from("<I", text, at - 4)[0])
				if handler is not None:
					found = Registration(va + at - 12, handler, "SEH3" if text[at - 11] == 0xFF else "SEH4")
			if found is None and at >= 7 and text[at - 7:at - 5] == b"\x6a\xff" and text[at - 5] == 0x68:
				handler = code_rva(struct.unpack_from("<I", text, at - 4)[0])
				if handler is not None:
					found = Registration(va + at - 7, handler, "C++ EH")
			if found is None and at >= 5 and text[at - 5] == 0x68 and text[at + 1] == 0xFF:
				handler = code_rva(struct.unpack_from("<I", text, at - 4)[0])
				if handler is not None:
					found = Registration(va + at - 5, handler, "push handler")
			if found is None:
				found = guard_registration(text, va, at, code_rva)
			if found is not None:
				registrations.append(found)
				continue
			# The prolog helpers' own loads: push eax; push fs:[0], or push -1; push eax; mov eax, fs:[0].
			helper = None
			if at >= 1 and text.startswith(PROLOG_HELPER_STARTS[0], at - 1):
				helper = at - 1
			elif at >= 3 and text.startswith(PROLOG_HELPER_STARTS[1], at - 3):
				helper = at - 3
			if helper is None:
				# __EH_prolog3_align and __EH_prolog3_GS_align: the aligned frame first, then the load.
				i = text.rfind(PROLOG_ALIGN_START, max(0, at - PROLOG_ALIGN_LOOKBACK), at)
				helper = i if i >= 0 else None
			if helper is not None:
				helpers.add(va + helper)
				continue
			others.append(va + at)
	# mov eax, imm32; call <a prolog helper>
	if helpers:
		for name, va, text in image.code_sections():
			i = text.find(b"\xb8")
			while 0 <= i <= len(text) - 10:
				if text[i + 5] == 0xE8:
					target = va + i + 10 + struct.unpack_from("<i", text, i + 6)[0]
					if target in helpers:
						handler = code_rva(struct.unpack_from("<I", text, i + 1)[0])
						if handler is not None:
							registrations.append(Registration(va + i, handler, "EH_prolog"))
				i = text.find(b"\xb8", i + 1)
	registrations.sort(key=lambda r: r.site)
	return registrations, others, helpers


# ---------------------------------------------------------------------------------------------------
# Names: the PDB (MSF 7.00) or the linker's map

MSF_MAGIC = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0"
S_PUB32 = 0x110E
PROC_KINDS = (0x110F, 0x1110, 0x1146, 0x1147)  # S_LPROC32, S_GPROC32 and their _ID forms


class Symbols:
	def __init__(self, source):
		self.source = source
		self.functions = []  # (start RVA, size, name)
		self.publics = []  # (RVA, name)

	def finish(self):
		self.functions.sort()
		self.publics.sort()

	def name_at(self, rva):
		"""The function containing rva (the PDB's procedures), else the nearest public before it with the
		distance, else None."""
		best = None
		for start, size, name in self.functions:
			if start > rva:
				break
			if rva < start + size:
				best = name
		if best is not None:
			return best
		for start, name in self.publics:
			if start > rva:
				break
			best = name if start == rva else "%s+0x%x" % (name, rva - start)
		return best

	def exact(self, rva):
		for start, size, name in self.functions:
			if start == rva:
				return name
		for start, name in self.publics:
			if start == rva:
				return name
		return None


def read_msf(data):
	if not data.startswith(MSF_MAGIC):
		raise ScanError("not an MSF 7.00 file")
	block_size, _, block_count, directory_bytes, _, block_map = struct.unpack_from("<6I", data, 32)

	def blocks(indices, size):
		out = bytearray()
		for index in indices:
			out += data[index * block_size:(index + 1) * block_size]
		return bytes(out[:size])

	directory_blocks = (directory_bytes + block_size - 1) // block_size
	directory_indices = struct.unpack_from("<%dI" % directory_blocks, data, block_map * block_size)
	directory = blocks(directory_indices, directory_bytes)
	stream_count = struct.unpack_from("<I", directory, 0)[0]
	sizes = struct.unpack_from("<%dI" % stream_count, directory, 4)
	position = 4 + 4 * stream_count
	streams = []
	for size in sizes:
		if size == 0xFFFFFFFF:
			streams.append(b"")
			continue
		count = (size + block_size - 1) // block_size
		indices = struct.unpack_from("<%dI" % count, directory, position)
		position += 4 * count
		streams.append(blocks(indices, size))
	return streams


def cstring(data, offset):
	end = data.index(b"\0", offset)
	return data[offset:end].decode("utf-8", "replace")


def records(stream, start):
	position = start
	while position + 4 <= len(stream):
		length, kind = struct.unpack_from("<HH", stream, position)
		if length < 2:
			break
		yield kind, stream[position + 4:position + 2 + length]
		position += 2 + length


def section_rva(image, segment, offset):
	if 1 <= segment <= len(image.sections):
		return image.sections[segment - 1][1] + offset
	return None


def read_pdb(image, path, guid):
	with open(path, "rb") as f:
		streams = read_msf(f.read())
	if len(streams) < 4 or len(streams[1]) < 28:
		raise ScanError("no PDB info stream")
	if guid is not None and streams[1][12:28] != guid:
		raise ScanError("its GUID is not the image's")
	symbols = Symbols(path)
	dbi = streams[3]
	if len(dbi) < 64:
		raise ScanError("no DBI stream")
	record_stream = struct.unpack_from("<H", dbi, 20)[0]
	module_info_size = struct.unpack_from("<i", dbi, 24)[0]
	if record_stream < len(streams):
		for kind, body in records(streams[record_stream], 0):
			if kind == S_PUB32 and len(body) >= 10:
				flags, offset, segment = struct.unpack_from("<IIH", body, 0)
				rva = section_rva(image, segment, offset)
				if rva is not None:
					symbols.publics.append((rva, cstring(body, 10)))
	position = 64
	end = 64 + module_info_size
	while position + 64 <= end:
		module_stream, symbol_bytes = struct.unpack_from("<HI", dbi, position + 34)
		names = position + 64
		module_name_end = dbi.index(b"\0", names)
		object_name_end = dbi.index(b"\0", module_name_end + 1)
		position = (object_name_end + 1 + 3) & ~3
		if module_stream == 0xFFFF or module_stream >= len(streams):
			continue
		stream = streams[module_stream][:symbol_bytes]
		for kind, body in records(stream, 4):
			if kind in PROC_KINDS and len(body) >= 35:
				size = struct.unpack_from("<I", body, 12)[0]
				offset, segment = struct.unpack_from("<IH", body, 28)
				rva = section_rva(image, segment, offset)
				if rva is not None:
					symbols.functions.append((rva, size, cstring(body, 35)))
	symbols.finish()
	return symbols


MAP_LINE = re.compile(r"^\s*([0-9A-Fa-f]{4}):([0-9A-Fa-f]{8})\s+(\S+)\s+([0-9A-Fa-f]{8})\s")


def read_map(image, path):
	symbols = Symbols(path)
	with open(path, encoding="latin-1") as f:
		for line in f:
			match = MAP_LINE.match(line)
			if match:
				rva = section_rva(image, int(match.group(1), 16), int(match.group(2), 16))
				if rva is not None:
					symbols.publics.append((rva, match.group(3)))
	symbols.finish()
	return symbols


def load_symbols(image, notes):
	stem = os.path.splitext(image.path)[0]
	codeview = image.codeview_pdb()
	candidates = []
	if codeview is not None:
		path, guid = codeview
		candidates += [(path, guid), (os.path.join(os.path.dirname(image.path), os.path.basename(path)), guid)]
	candidates.append((stem + ".pdb", codeview[1] if codeview else None))
	seen = set()
	for path, guid in candidates:
		key = os.path.normcase(os.path.abspath(path))
		if key in seen or not os.path.isfile(path):
			continue
		seen.add(key)
		try:
			return read_pdb(image, path, guid)
		except (ScanError, ValueError, struct.error) as error:
			notes.append("PDB %s not used: %s" % (path, error))
	if os.path.isfile(stem + ".map"):
		return read_map(image, stem + ".map")
	return None


# ---------------------------------------------------------------------------------------------------
# The scan


def scan(path, verbose=False, use_symbols=True, out=sys.stdout):
	"""Prints the report of one image; returns 0 clean, 1 failed, 2 unreadable."""
	try:
		with open(path, "rb") as f:
			image = Image(path, f.read())
		state, table = image.safeseh_table()
		registrations, others, helpers = find_registrations(image)
	except (OSError, ScanError, struct.error, IndexError) as error:
		print("%s: cannot be scanned: %s" % (path, error), file=out)
		return 2
	notes = []
	symbols = load_symbols(image, notes) if use_symbols else None

	def describe(rva):
		name = symbols.name_at(rva) if symbols else None
		return "RVA 0x%08x" % rva if name is None else "%s (RVA 0x%08x)" % (name, rva)

	def describe_handler(rva):
		name = symbols.exact(rva) if symbols else None
		return "RVA 0x%08x" % rva if name is None else "%s (RVA 0x%08x)" % (name, rva)

	no_seh = bool(image.dll_characteristics & IMAGE_DLLCHARACTERISTICS_NO_SEH)
	kinds = {}
	for registration in registrations:
		kinds[registration.kind] = kinds.get(registration.kind, 0) + 1
	summary = ", ".join("%d %s" % (kinds[k], k) for k in sorted(kinds)) or "none"
	names = "names from %s" % symbols.source if symbols else "no PDB or map: RVAs only"
	for note in notes:
		print("%s: %s" % (path, note), file=out)
	if state != "table":
		reason = {
			"no-load-config": "no load configuration directory",
			"short-load-config": "the load configuration ends before SEHandlerTable",
			"no-table": "the load configuration has no SEHandlerTable",
		}[state]
		if no_seh and not registrations:
			print("%s: IMAGE_DLLCHARACTERISTICS_NO_SEH and no handler registered: clean" % path, file=out)
			return 0
		print("%s: no SafeSEH table (%s; linked /SAFESEH:NO?), %d registrations (%s): not checked, fails"
			% (path, reason, len(registrations), summary), file=out)
		return 1
	missing = [r for r in registrations if no_seh or r.handler not in table]
	print("%s: SafeSEH table %d handlers; %d registrations (%s), %d other fs:[0] loads; %s; %d with a handler "
		"not in the table" % (path, len(table), len(registrations), summary, len(others), names, len(missing)),
		file=out)
	if no_seh and registrations:
		print("  IMAGE_DLLCHARACTERISTICS_NO_SEH is set: Windows calls none of these handlers", file=out)
	for registration in missing:
		function = symbols.name_at(registration.site) if symbols else None
		print("  %s: %s frame at RVA 0x%08x, handler %s not in the SafeSEH table" % (
			function or "function not named", registration.kind, registration.site,
			describe_handler(registration.handler)), file=out)
	if verbose:
		for helper in sorted(helpers):
			print("  prolog helper %s" % describe(helper), file=out)
		for rva in others:
			print("  other fs:[0] load at %s" % describe(rva), file=out)
		for registration in registrations:
			print("  %s %s handler %s%s" % (registration.kind, describe(registration.site),
				describe_handler(registration.handler), "" if registration.handler in table else " MISSING"),
				file=out)
	return 1 if missing else 0


# ---------------------------------------------------------------------------------------------------
# The self-test: small made-up PE32 images

TEXT_RVA, RDATA_RVA, BASE = 0x1000, 0x2000, 0x400000


def make_image(code, handlers, table=True, load_config=True, dll_characteristics=0x0140, rsds=None,
		machine=IMAGE_FILE_MACHINE_I386):
	"""A PE32 image: .text (code, file 0x400) and .rdata (load config, table, debug data, file 0x600)."""
	rdata = bytearray(0x200)
	if load_config:
		struct.pack_into("<I", rdata, 0, 0x5C)  # IMAGE_LOAD_CONFIG_DIRECTORY32's size (up to the GuardFlags)
		if table:
			struct.pack_into("<II", rdata, 0x40, BASE + RDATA_RVA + 0x60, len(handlers))
			for i, handler in enumerate(sorted(handlers)):
				struct.pack_into("<I", rdata, 0x60 + 4 * i, handler)
	debug = None
	if rsds is not None:
		guid, pdb = rsds
		record = b"RSDS" + guid + struct.pack("<I", 1) + pdb.encode() + b"\0"
		rdata[0x140:0x140 + len(record)] = record
		struct.pack_into("<IIHHIIII", rdata, 0x120, 0, 0, 0, 0, DEBUG_TYPE_CODEVIEW, len(record),
			RDATA_RVA + 0x140, 0x600 + 0x140)
		debug = (RDATA_RVA + 0x120, 28)
	text = bytes(code) + b"\xcc" * (0x200 - len(code))
	headers = bytearray(0x400)
	headers[0:2] = b"MZ"
	struct.pack_into("<I", headers, 0x3C, 0x80)
	headers[0x80:0x84] = b"PE\0\0"
	struct.pack_into("<HHIIIHH", headers, 0x84, machine, 2, 0, 0, 0, 0xE0, 0x0102)
	optional = 0x98
	struct.pack_into("<HBBIIIIIII", headers, optional, 0x10B, 14, 0, 0x200, 0x200, 0, TEXT_RVA, TEXT_RVA,
		RDATA_RVA, BASE)
	struct.pack_into("<IIHHHHHHIIIIHHIIIIII", headers, optional + 32, 0x1000, 0x200, 6, 0, 0, 0, 6, 0, 0, 0x3000,
		0x400, 0, 2, dll_characteristics, 0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
	if load_config:
		struct.pack_into("<II", headers, optional + 96 + 8 * DIRECTORY_LOAD_CONFIG, RDATA_RVA, 0x5C)
	if debug:
		struct.pack_into("<II", headers, optional + 96 + 8 * DIRECTORY_DEBUG, *debug)
	sections = optional + 0xE0
	struct.pack_into("<8sIIIIIIHHI", headers, sections, b".text", 0x200, TEXT_RVA, 0x200, 0x400, 0, 0, 0, 0,
		0x60000020)
	struct.pack_into("<8sIIIIIIHHI", headers, sections + 40, b".rdata", 0x200, RDATA_RVA, 0x200, 0x600, 0, 0, 0, 0,
		0x40000040)
	return bytes(headers) + text + bytes(rdata)


def make_msf(streams, block_size=512):
	"""An MSF 7.00 file with these streams."""
	blocks = [None, b"", b""]  # the superblock and the two free block maps

	def place(content):
		indices = []
		for i in range(0, max(len(content), 1), block_size):
			chunk = content[i:i + block_size]
			if not chunk:
				break
			indices.append(len(blocks))
			blocks.append(chunk)
		return indices

	stream_blocks = [place(s) for s in streams]
	directory = struct.pack("<I", len(streams)) + b"".join(struct.pack("<I", len(s)) for s in streams)
	directory += b"".join(struct.pack("<%dI" % len(b), *b) for b in stream_blocks)
	directory_blocks = place(directory)
	block_map = len(blocks)
	blocks.append(struct.pack("<%dI" % len(directory_blocks), *directory_blocks))
	blocks[0] = MSF_MAGIC + struct.pack("<6I", block_size, 1, len(blocks) + 1, len(directory), 0, block_map)
	return b"".join(b.ljust(block_size, b"\0") for b in blocks)


def make_pdb(guid, procs, publics):
	"""A PDB with a DBI stream, one module (S_GPROC32 records) and the publics (S_PUB32 records)."""

	def record(kind, body):
		body += b"\0" * (-(len(body) + 2) % 4)
		return struct.pack("<HH", len(body) + 2, kind) + body

	module = struct.pack("<I", 4)
	for rva, size, name in procs:
		module += record(0x1110, struct.pack("<IIIIIIIIHB", 0, 0, 0, size, 0, size, 0, rva - TEXT_RVA, 1, 0)
			+ name.encode() + b"\0")
	public_records = b"".join(record(S_PUB32, struct.pack("<IIH", 2, rva - TEXT_RVA, 1) + name.encode() + b"\0")
		for rva, name in publics)
	module_info = bytearray(64)
	struct.pack_into("<HI", module_info, 34, 5, len(module))
	module_info += b"test.obj\0test.obj\0"
	module_info += b"\0" * (-len(module_info) % 4)
	dbi = bytearray(64)
	struct.pack_into("<iII", dbi, 0, -1, 19990903, 1)
	struct.pack_into("<H", dbi, 20, 4)
	struct.pack_into("<i", dbi, 24, len(module_info))
	dbi += module_info
	info = struct.pack("<III", 20000404, 0, 1) + guid
	return make_msf([b"", info, b"", bytes(dbi), public_records, module])


def self_test():
	failures = []

	def check(label, condition):
		print("  %s %s" % ("ok  " if condition else "FAIL", label))
		if not condition:
			failures.append(label)

	def va(rva):
		return BASE + rva

	def push_handler(rva):
		return b"\x68" + struct.pack("<I", va(rva))

	frame = b"\x55\x8b\xec\x6a\xff"  # push ebp; mov ebp, esp; push -1
	load = b"\x64\xa1\x00\x00\x00\x00\x50"  # mov eax, fs:[0]; push eax
	f1, f2, f3, f4, f5, helper = 0x1000, 0x1040, 0x1080, 0x10C0, 0x1100, 0x1180
	h1, h2, h3, h4, h5 = 0x1140, 0x1150, 0x1160, 0x1170, 0x11A0
	f6, align_helper, h6 = 0x11E0, 0x11C0, 0x11F0
	f7, h7 = 0x1020, 0x1038
	code = bytearray(b"\xcc" * 0x200)

	def put(rva, data):
		code[rva - TEXT_RVA:rva - TEXT_RVA + len(data)] = data

	# f1: the C++ EH frame (push -1; push __ehhandler$f1; mov eax, fs:[0]).
	put(f1, frame + push_handler(h1) + load)
	# f7: the inline SEH3 frame (push -1; push <scope table>; push _except_handler3).
	put(f7, b"\x6a\xff\x68" + struct.pack("<I", va(RDATA_RVA + 0x100)) + push_handler(h7) + load)
	# f2: the inline SEH4 frame (push -2; push <scope table in .rdata>; push _except_handler4).
	put(f2, b"\x55\x8b\xec\x6a\xfe" + b"\x68" + struct.pack("<I", va(RDATA_RVA + 0x100)) + push_handler(h2) + load)
	# f3: push handler; push fs:[0].
	put(f3, push_handler(h3) + b"\x64\xff\x35\x00\x00\x00\x00")
	# f4: mov eax, __ehhandler$f4; call __EH_prolog3.
	put(f4, b"\xb8" + struct.pack("<I", va(h4)) + b"\xe8" + struct.pack("<i", helper - (f4 + 10)))
	put(helper, b"\x50\x64\xff\x35\x00\x00\x00\x00\x8d\x44\x24\x0c")
	# f6: mov eax, __ehhandler$f6; call __EH_prolog3_align (the aligned frame, then push fs:[0]).
	put(f6, b"\xb8" + struct.pack("<I", va(h6)) + b"\xe8" + struct.pack("<i", align_helper - (f6 + 10)))
	put(align_helper, PROLOG_ALIGN_START + b"\x8d\x44\x24\x08\x64\xff\x35\x00\x00\x00\x00")
	# f5: a guard frame (mov [ebp-14h], handler; ...; mov eax, fs:[0]).
	put(f5, b"\x55\x8b\xec\xc7\x45\xec" + struct.pack("<I", va(h5)) + b"\x89\x4d\xf8\x64\xa1\x00\x00\x00\x00"
		b"\x89\x45\xe8")
	# Not a registration: push -1; push <data>; mov eax, fs:[0] (the immediate is not code).
	put(0x1120, b"\x6a\xff\x68" + struct.pack("<I", va(RDATA_RVA + 0x100)) + b"\x64\xa1\x00\x00\x00\x00")
	every = [h1, h2, h3, h4, h5, h6, h7]  # the table holds RVAs
	guid = bytes(range(16))

	with tempfile.TemporaryDirectory() as directory:
		def run(name, data, **options):
			path = os.path.join(directory, name)
			with open(path, "wb") as f:
				f.write(data)

			class Capture:
				text = ""

				def write(self, s):
					self.text += s

			capture = Capture()
			status = scan(path, out=capture, **options)
			for line in capture.text.splitlines():
				print("    | " + line)
			return status, capture.text

		print("A clean image (every kind of frame, every handler in the table):")
		status, text = run("clean.exe", make_image(code, every))
		check("exit 0", status == 0)
		check("7 registrations, 1 other fs:[0] load", "; 7 registrations (" in text and "1 other fs:[0] loads"
			in text)
		check("one of each kind", all(k in text for k in ("1 C++ EH", "1 SEH3", "1 SEH4", "1 push handler", "2 EH_prolog",
			"1 guard")))

		print("A frame whose handler is missing from the table:")
		status, text = run("missing.exe", make_image(code, [h for h in every if h != h1]))
		check("exit 1", status == 1)
		check("1 finding, by RVA", "1 with a handler not in the table" in text and "named: C++ EH frame at RVA 0x%08x" % (f1 + 3)
			in text and "handler RVA 0x%08x" % h1 in text)

		print("The same image with its PDB (found through the CodeView record):")
		pdb = make_pdb(guid, [(f1, 0x20, "TPlayerStates::GetIcon")],
			[(f1, "?GetIcon@TPlayerStates@@QAEHH@Z"), (h1, "__ehhandler$?GetIcon@TPlayerStates@@QAEHH@Z")])
		with open(os.path.join(directory, "named.pdb"), "wb") as f:
			f.write(pdb)
		image = make_image(code, [h for h in every if h != h1], rsds=(guid, "Z:\\elsewhere\\named.pdb"))
		status, text = run("named.exe", image)
		check("exit 1, the function named", status == 1 and "TPlayerStates::GetIcon: C++ EH frame at RVA 0x%08x"
			% (f1 + 3) in text)
		check("the handler named", "__ehhandler$?GetIcon@TPlayerStates@@QAEHH@Z (RVA 0x%08x)" % h1 in text)

		print("A PDB of another build (its GUID differs) is not used:")
		image = make_image(code, [h for h in every if h != h1], rsds=(bytes(16), "named.pdb"))
		status, text = run("other.exe", image)
		check("exit 1, RVAs only", status == 1 and "not used" in text and "named: C++ EH frame at RVA"
			in text)

		print("A map names it too:")
		with open(os.path.join(directory, "mapped.map"), "w", encoding="latin-1") as f:
			f.write(" 0001:00000000       ?GetIcon@TPlayerStates@@QAEHH@Z 00401000 f   ContentWindows.obj\n")
		status, text = run("mapped.exe", make_image(code, [h for h in every if h != h1]))
		check("exit 1, named from the map", status == 1 and "?GetIcon@TPlayerStates@@QAEHH@Z+0x3: C++ EH frame"
			in text)

		print("No SafeSEH table (/SAFESEH:NO), and no load configuration:")
		status, text = run("noseh.exe", make_image(code, every, table=False))
		check("exit 1, reported as no table", status == 1 and "no SafeSEH table" in text)
		status, text = run("noconfig.exe", make_image(code, every, load_config=False))
		check("exit 1, reported as no load configuration", status == 1 and "no load configuration" in text)

		print("IMAGE_DLLCHARACTERISTICS_NO_SEH:")
		status, text = run("nosehflag.exe", make_image(b"\x90\xc3", [], table=False, dll_characteristics=0x0540))
		check("no registration: exit 0", status == 0)
		status, text = run("nosehframe.exe", make_image(code, every, dll_characteristics=0x0540))
		check("a registration: exit 1", status == 1 and "7 with a handler not in the table" in text)

		print("Images that cannot be scanned:")
		status, text = run("x64.exe", make_image(code, every, machine=0x8664))
		check("x64: exit 2", status == 2 and "not a PE32 x86 image" in text)
		status, text = run("text.exe", b"not an image")
		check("not a PE: exit 2", status == 2)

	print("self-test: %s" % ("%d failed" % len(failures) if failures else "passed"))
	return 2 if failures else 0


def main(argv):
	args = list(argv)
	if args == ["--self-test"]:
		return self_test()
	verbose = "-v" in args
	use_symbols = "--no-symbols" not in args
	paths = [a for a in args if a not in ("-v", "--no-symbols")]
	if not paths or any(p.startswith("-") for p in paths):
		print(__doc__.split("\n\n")[1], file=sys.stderr)
		return 2
	worst = 0
	for path in paths:
		worst = max(worst, scan(path, verbose=verbose, use_symbols=use_symbols))
	return worst


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
