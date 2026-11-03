"""Compare the import tables of two PE images (issue #16).

Usage: python -I tools/compare_imports.py Tibia.exe build/oracle-x86/Release/Patella.exe [--all]

Reads the import directory and the delay-load import directory of both images (by name or
ordinal per DLL, DLL names compared without case) and prints the DLLs of each in table order,
then per DLL the functions only the first image imports ("-") and only the second imports
("+"); --all also lists the functions both import. Exits with 0; the differences are expected
(notes/decisions.md, "Building Patella.exe (issue #16)").
"""

import struct
import sys


class Image:
	def __init__(self, path):
		with open(path, "rb") as f:
			self.data = f.read()
		d = self.data
		pe = struct.unpack_from("<I", d, 0x3c)[0]
		assert d[pe:pe + 4] == b"PE\0\0", path
		sections = struct.unpack_from("<H", d, pe + 6)[0]
		optsize = struct.unpack_from("<H", d, pe + 20)[0]
		opt = pe + 24
		magic = struct.unpack_from("<H", d, opt)[0]
		assert magic == 0x10b, "PE32 only"
		self.dirs = [struct.unpack_from("<II", d, opt + 96 + 8 * i) for i in range(16)]
		self.sections = []
		for i in range(sections):
			s = opt + optsize + 40 * i
			vsize, va, rawsize, raw = struct.unpack_from("<IIII", d, s + 8)
			self.sections.append((va, max(vsize, rawsize), raw))

	def offset(self, rva):
		for va, size, raw in self.sections:
			if va <= rva < va + size:
				return raw + rva - va
		raise ValueError("RVA 0x%x outside the sections" % rva)

	def string(self, rva):
		o = self.offset(rva)
		return self.data[o:self.data.index(b"\0", o)].decode("ascii")

	def thunks(self, rva):
		names = []
		o = self.offset(rva)
		while True:
			t = struct.unpack_from("<I", self.data, o)[0]
			if t == 0:
				return names
			if t & 0x80000000:
				names.append("#%d" % (t & 0xffff))
			else:
				names.append(self.string(t + 2))
			o += 4

	def imports(self):
		"""[(dll, [names])] of the import directory."""
		result = []
		rva, size = self.dirs[1]
		if rva == 0:
			return result
		o = self.offset(rva)
		while True:
			ilt, _, _, name, iat = struct.unpack_from("<IIIII", self.data, o)
			if name == 0:
				return result
			result.append((self.string(name), self.thunks(ilt or iat)))
			o += 20

	def delay_imports(self):
		"""[(dll, [names])] of the delay-load import directory."""
		result = []
		rva, size = self.dirs[13]
		if rva == 0:
			return result
		o = self.offset(rva)
		while True:
			attrs, name, hmod, iat, int_, bound, unload, stamp = struct.unpack_from("<8I", self.data, o)
			if name == 0:
				return result
			base = 0
			if not attrs & 1:  # VC6-style: virtual addresses, not RVAs
				base = struct.unpack_from("<I", self.data, struct.unpack_from("<I", self.data, 0x3c)[0] + 24 + 28)[0]
			result.append((self.string(name - base), self.thunks(int_ - base)))
			o += 32


def compare(title, a, b, show_all):
	print("== %s" % title)
	print("first:  %s" % ", ".join(n for n, _ in a))
	print("second: %s" % ", ".join(n for n, _ in b))
	da = {n.upper(): (n, set(f)) for n, f in a}
	db = {n.upper(): (n, set(f)) for n, f in b}
	for key in list(da) + [k for k in db if k not in da]:
		fa = da.get(key, (key, set()))[1]
		fb = db.get(key, (key, set()))[1]
		name = (da.get(key) or db.get(key))[0]
		where = "both" if key in da and key in db else ("first only" if key in da else "second only")
		print("%s (%s): %d / %d functions, %d in both" % (name, where, len(fa), len(fb), len(fa & fb)))
		if show_all:
			for f in sorted(fa & fb):
				print("    %s" % f)
		for f in sorted(fa - fb):
			print("  - %s" % f)
		for f in sorted(fb - fa):
			print("  + %s" % f)


def main(argv):
	args = [a for a in argv[1:] if not a.startswith("--")]
	if len(args) != 2:
		print(__doc__.strip().splitlines()[2], file=sys.stderr)
		return 2
	a, b = Image(args[0]), Image(args[1])
	compare("imports", a.imports(), b.imports(), "--all" in argv)
	compare("delay-load imports", a.delay_imports(), b.delay_imports(), "--all" in argv)
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
