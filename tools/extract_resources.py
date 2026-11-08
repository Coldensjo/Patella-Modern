"""Extract the resources of Tibia.exe (or any PE32 image) into files rc.exe can read.

Usage: python -I tools/extract_resources.py <Tibia.exe> <outdir> [--list]

Writes, byte-exact from the image's .rsrc section:
	cursor_<id>.cur   RT_GROUP_CURSOR <id> with its RT_CURSOR images
	icon_<id>.ico     RT_GROUP_ICON <id> with its RT_ICON images
	bitmap_<id>.bmp   RT_BITMAP <id> with a BITMAPFILEHEADER in front
	<type>_<id>.bin   any other resource whose type src/res/Patella.rc does not write as
	                  text (none in Tibia.exe 7.72)
	resources.tsv     every entry of the image: type, name, language, code page, size, RVA

The text resources (version information, string tables, the dialog) are written by hand in
src/res/Patella.rc; tools/compare_resources.py checks that the compiled script gives the
same entries. --list prints the table instead of writing files.

The PE parser is shared with tools/compare_resources.py. CipSoft's icons and cursors and
the MFC cursors and bitmaps of the image (not used since MFC went, issue #127, but extracted with the rest) are not
committed: run this script against a local Tibia.exe to get them (notes/resources.md).
"""

import os
import struct
import sys

RT_CURSOR = 1
RT_BITMAP = 2
RT_ICON = 3
RT_MENU = 4
RT_DIALOG = 5
RT_STRING = 6
RT_ACCELERATOR = 9
RT_RCDATA = 10
RT_GROUP_CURSOR = 12
RT_GROUP_ICON = 14
RT_VERSION = 16
RT_MANIFEST = 24

TYPE_NAMES = {
	RT_CURSOR: "RT_CURSOR", RT_BITMAP: "RT_BITMAP", RT_ICON: "RT_ICON", RT_MENU: "RT_MENU",
	RT_DIALOG: "RT_DIALOG", RT_STRING: "RT_STRING", 7: "RT_FONTDIR", 8: "RT_FONT",
	RT_ACCELERATOR: "RT_ACCELERATOR", RT_RCDATA: "RT_RCDATA", 11: "RT_MESSAGETABLE",
	RT_GROUP_CURSOR: "RT_GROUP_CURSOR", RT_GROUP_ICON: "RT_GROUP_ICON", RT_VERSION: "RT_VERSION",
	17: "RT_DLGINCLUDE", 19: "RT_PLUGPLAY", 20: "RT_VXD", 21: "RT_ANICURSOR", 22: "RT_ANIICON",
	23: "RT_HTML", RT_MANIFEST: "RT_MANIFEST",
}

# Types the script writes as text; everything else not handled below goes to a .bin file.
TEXT_TYPES = {RT_MENU, RT_DIALOG, RT_STRING, RT_ACCELERATOR, RT_VERSION}


class Entry:
	"""One leaf of the resource tree: type, name and language are ints or strings."""

	def __init__(self, type_, name, lang, codepage, data, rva):
		self.type = type_
		self.name = name
		self.lang = lang
		self.codepage = codepage
		self.data = data
		self.rva = rva

	def key(self):
		return (self.type, self.name, self.lang)


def type_name(type_):
	if isinstance(type_, int):
		return TYPE_NAMES.get(type_, str(type_))
	return '"%s"' % type_


def read_pe_resources(path):
	"""Returns the entries of the image's resource directory in tree order."""
	with open(path, "rb") as f:
		image = f.read()
	if image[:2] != b"MZ":
		raise ValueError("%s: not an MZ image" % path)
	pe = struct.unpack_from("<I", image, 0x3C)[0]
	if image[pe:pe + 4] != b"PE\0\0":
		raise ValueError("%s: no PE header" % path)
	sections_count = struct.unpack_from("<H", image, pe + 6)[0]
	optional_size = struct.unpack_from("<H", image, pe + 20)[0]
	magic = struct.unpack_from("<H", image, pe + 24)[0]
	if magic != 0x10B:
		raise ValueError("%s: not PE32" % path)
	# Data directory 2 (resources) of a PE32 optional header.
	dir_rva, dir_size = struct.unpack_from("<II", image, pe + 24 + 96 + 2 * 8)
	sections = []
	for i in range(sections_count):
		o = pe + 24 + optional_size + 40 * i
		virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", image, o + 8)
		sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer))

	def offset(rva):
		for virtual_address, size, raw_pointer in sections:
			if virtual_address <= rva < virtual_address + size:
				return raw_pointer + rva - virtual_address
		raise ValueError("RVA 0x%x is in no section" % rva)

	base = offset(dir_rva)

	def read_name(value):
		if value & 0x80000000:
			o = base + (value & 0x7FFFFFFF)
			length = struct.unpack_from("<H", image, o)[0]
			return image[o + 2:o + 2 + 2 * length].decode("utf-16le")
		return value

	entries = []

	def walk(directory, level, path_):
		named, ids = struct.unpack_from("<HH", image, base + directory + 12)
		for i in range(named + ids):
			name, target = struct.unpack_from("<II", image, base + directory + 16 + 8 * i)
			key = read_name(name)
			if target & 0x80000000:
				if level >= 2:
					raise ValueError("resource tree deeper than three levels")
				walk(target & 0x7FFFFFFF, level + 1, path_ + [key])
			else:
				if level != 2:
					raise ValueError("resource data above the language level")
				data_rva, size, codepage, _ = struct.unpack_from("<IIII", image, base + target)
				o = offset(data_rva)
				entries.append(Entry(path_[0], path_[1], key, codepage, image[o:o + size], data_rva))

	walk(0, 0, [])
	return entries, (dir_rva, dir_size)


def find(entries, type_, name, lang=None):
	for entry in entries:
		if entry.type == type_ and entry.name == name and (lang is None or entry.lang == lang):
			return entry
	return None


def build_icon(entries, group):
	"""An .ico file from an RT_GROUP_ICON and its RT_ICON images. The directory entries copy
	the group's fields, so rc.exe writes the same group back."""
	reserved, kind, count = struct.unpack_from("<HHH", group.data, 0)
	assert reserved == 0 and kind == 1 and len(group.data) == 6 + 14 * count
	images = []
	header = struct.pack("<HHH", 0, 1, count)
	position = 6 + 16 * count
	directory = b""
	for i in range(count):
		width, height, colors, reserved_, planes, bits, size, image_id = struct.unpack_from("<BBBBHHIH", group.data, 6 + 14 * i)
		image = find(entries, RT_ICON, image_id, group.lang)
		assert image is not None and len(image.data) == size, "icon image %d" % image_id
		directory += struct.pack("<BBBBHHII", width, height, colors, reserved_, planes, bits, size, position)
		images.append(image.data)
		position += size
	return header + directory + b"".join(images)


def build_cursor(entries, group):
	"""A .cur file from an RT_GROUP_CURSOR and its RT_CURSOR images. An RT_CURSOR image is the
	hot spot (two WORDs) followed by the DIB; a .cur file keeps the hot spot in its directory.
	The group stores the DIB's width and doubled height; the .cur entry has width, height."""
	reserved, kind, count = struct.unpack_from("<HHH", group.data, 0)
	assert reserved == 0 and kind == 2 and len(group.data) == 6 + 14 * count
	header = struct.pack("<HHH", 0, 2, count)
	position = 6 + 16 * count
	directory = b""
	images = []
	for i in range(count):
		width, height, planes, bits, size, image_id = struct.unpack_from("<HHHHIH", group.data, 6 + 14 * i)
		image = find(entries, RT_CURSOR, image_id, group.lang)
		assert image is not None and len(image.data) == size, "cursor image %d" % image_id
		hot_x, hot_y = struct.unpack_from("<HH", image.data, 0)
		dib = image.data[4:]
		directory += struct.pack("<BBBBHHII", width & 0xFF, (height // 2) & 0xFF, 0, 0, hot_x, hot_y, len(dib), position)
		images.append(dib)
		position += len(dib)
	return header + directory + b"".join(images)


def build_bitmap(entry):
	"""A .bmp file: BITMAPFILEHEADER + the resource (BITMAPINFOHEADER, colour table, bits)."""
	data = entry.data
	header_size = struct.unpack_from("<I", data, 0)[0]
	if header_size == 12:  # BITMAPCOREHEADER
		bits = struct.unpack_from("<H", data, 10)[0]
		colors = (1 << bits) if bits <= 8 else 0
		table = 3 * colors
	else:
		bits, compression = struct.unpack_from("<HI", data, 14)
		used = struct.unpack_from("<I", data, 32)[0]
		colors = used if used else ((1 << bits) if bits <= 8 else 0)
		table = 4 * colors
		if compression == 3 and header_size == 40:  # BI_BITFIELDS masks
			table += 12
	off_bits = 14 + header_size + table
	return struct.pack("<2sIHHI", b"BM", 14 + len(data), 0, 0, off_bits) + data


def resource_files(entries):
	"""(file name, bytes) for every resource that the .rc takes from a file."""
	files = []
	used = set()
	for entry in entries:
		if entry.type == RT_GROUP_CURSOR:
			files.append(("cursor_%s.cur" % entry.name, build_cursor(entries, entry)))
		elif entry.type == RT_GROUP_ICON:
			files.append(("icon_%s.ico" % entry.name, build_icon(entries, entry)))
		elif entry.type == RT_BITMAP:
			files.append(("bitmap_%s.bmp" % entry.name, build_bitmap(entry)))
		elif entry.type in (RT_CURSOR, RT_ICON) or entry.type in TEXT_TYPES:
			continue
		else:
			name = "%s_%s.bin" % (entry.type, entry.name)
			if name in used:
				name = "%s_%s_%d.bin" % (entry.type, entry.name, entry.lang)
			files.append((name, entry.data))
		used.add(files[-1][0])
	return files


def table(entries):
	lines = ["type\ttype_name\tname\tlanguage\tcodepage\tsize\trva"]
	for e in entries:
		lines.append("%s\t%s\t%s\t%s\t%d\t%d\t0x%x" % (e.type, type_name(e.type), e.name, e.lang, e.codepage, len(e.data), e.rva))
	return "\n".join(lines) + "\n"


def main(argv):
	if len(argv) < 2 or (len(argv) < 3 and "--list" not in argv):
		print(__doc__.strip().splitlines()[2], file=sys.stderr)
		return 2
	entries, (dir_rva, dir_size) = read_pe_resources(argv[1])
	if "--list" in argv:
		sys.stdout.write(table(entries))
		print("%d entries; resource directory at RVA 0x%x, 0x%x bytes" % (len(entries), dir_rva, dir_size))
		return 0
	outdir = argv[2]
	os.makedirs(outdir, exist_ok=True)
	files = resource_files(entries)
	for name, data in files:
		with open(os.path.join(outdir, name), "wb") as f:
			f.write(data)
	with open(os.path.join(outdir, "resources.tsv"), "w", encoding="utf-8", newline="\n") as f:
		f.write(table(entries))
	print("%d resource entries, %d files written to %s" % (len(entries), len(files), outdir))
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
