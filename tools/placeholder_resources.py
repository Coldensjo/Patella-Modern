"""Write neutral placeholder pictures for src/res/Patella.rc, for builds without Tibia.exe.

Usage: python -I tools/placeholder_resources.py <outdir>

Issue #127: hosted CI builds the full client but has no Tibia.exe and must never get it. Patella.rc takes
cursor_132.cur, cursor_137.cur and icon_128.ico from the include path; on a machine with Tibia.exe they come from
tools/extract_resources.py (cmake/Resources.cmake). Here the same file names hold plain generated shapes (a hollow square
for the cursors, a solid square for the icon) drawn from nothing but this script; they are not CipSoft's pictures and
not derived from them.

cmake/WindowsX64.cmake runs this only with PATELLA_PLACEHOLDER_RESOURCES=ON and refuses that option where a Tibia.exe
exists, so a local build never gets them. resources.tsv (the marker the build depends on) says what they are.
"""

import os
import struct
import sys

SIZE = 32


def rows_top_down(pixel):
	"""The 1-bit rows of a SIZE x SIZE picture, each 4 bytes wide, pixel(x, y) True for a set bit."""
	rows = []
	for y in range(SIZE):
		value = 0
		for x in range(SIZE):
			value = (value << 1) | (1 if pixel(x, y) else 0)
		rows.append(struct.pack(">I", value))
	return rows


def dib(xor_pixel, and_pixel):
	"""A BITMAPINFOHEADER with a two-colour table, the XOR mask, then the AND mask (both bottom-up)."""
	header = struct.pack("<IiiHHIIiiII", 40, SIZE, SIZE * 2, 1, 1, 0, 0, 0, 0, 2, 0)
	table = struct.pack("<II", 0x00000000, 0x00ffffff)
	xor_rows = b"".join(reversed(rows_top_down(xor_pixel)))
	and_rows = b"".join(reversed(rows_top_down(and_pixel)))
	return header + table + xor_rows + and_rows


def border(x, y, thickness=2, inset=4):
	"""A hollow square of the given thickness, inset from the picture's edge."""
	lo, hi = inset, SIZE - 1 - inset
	if not (lo <= x <= hi and lo <= y <= hi):
		return False
	return x < lo + thickness or x > hi - thickness or y < lo + thickness or y > hi - thickness


def inside(x, y, inset=4):
	return inset <= x < SIZE - inset and inset <= y < SIZE - inset


def cursor_file(hot_x, hot_y):
	"""A monochrome .cur: the XOR mask is the white border, the AND mask keeps everything outside the square."""
	image = dib(border, lambda x, y: not inside(x, y))
	directory = struct.pack("<HHH", 0, 2, 1) + struct.pack("<BBBBHHII", SIZE, SIZE, 2, 0, hot_x, hot_y, len(image), 22)
	return directory + image


def icon_file():
	"""A monochrome .ico: a solid white square on a transparent background."""
	image = dib(inside, lambda x, y: not inside(x, y))
	directory = struct.pack("<HHH", 0, 1, 1) + struct.pack("<BBBBHHII", SIZE, SIZE, 2, 0, 1, 1, len(image), 22)
	return directory + image


def main(argv):
	if len(argv) != 2:
		print(__doc__.strip().splitlines()[2], file=sys.stderr)
		return 2
	outdir = argv[1]
	os.makedirs(outdir, exist_ok=True)
	files = {
		"cursor_132.cur": cursor_file(SIZE // 2, SIZE // 2),
		"cursor_137.cur": cursor_file(SIZE // 2, SIZE // 2),
		"icon_128.ico": icon_file(),
	}
	for name, data in files.items():
		with open(os.path.join(outdir, name), "wb") as f:
			f.write(data)
	with open(os.path.join(outdir, "resources.tsv"), "w", encoding="utf-8", newline="\n") as f:
		f.write("placeholder resources (tools/placeholder_resources.py); not Tibia.exe's pictures\n")
	print("%d placeholder files written to %s" % (len(files), outdir))
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
