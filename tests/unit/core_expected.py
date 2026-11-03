"""The expected values of tests_core (issue #7), from Python's exact integers and IEEE doubles, the way
tools/difftest/vlongcheck.py checks vlongtest.exe. Prints the constants the tests compare with:

	python -I tests/unit/core_expected.py

- vlong and TRSA: vlongtest.cpp's cases (the same generator, seed 4242: 200 RSA blocks, 3,000
  operations, one decimal conversion), digested with FNV-1a (64 bits) over the result bytes in order.
- TXTEA: XTEA with 32 cycles on little-endian words, written from the algorithm.
- GetOutfitColor: the 133 colours (and 133, which is colour 0), each R, G, B as one byte, digested.
"""

MODULUS = int(
	'124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428'
	'185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403'
	'808045846444647284499123164879035103627004668521005328367415259939915284902061793')


class Fnv:
	def __init__(self):
		self.value = 0xcbf29ce484222325

	def update(self, data):
		for byte in data:
			self.value = ((self.value ^ byte) * 0x100000001b3) & 0xffffffffffffffff


class Random:
	def __init__(self):
		self.seed = 4242

	def __call__(self):
		self.seed = (self.seed * 1103515245 + 12345) & 0xffffffff
		return self.seed >> 8


def vlong_cases():
	rnd = Random()
	digest = Fnv()
	cases = 0
	for t in range(200):
		block = bytearray(rnd() & 0xff for _ in range(128))
		block[0] &= 0x7f
		if t == 0:
			block = bytearray(128)
		plain = int.from_bytes(block, 'big')
		digest.update(pow(plain, 65537, MODULUS).to_bytes(128, 'big'))
		cases += 1

	def random_bytes():
		used = 1 + rnd() % 64
		data = bytearray(64)
		for i in range(64 - used, 64):
			data[i] = rnd() & 0xff
		return int.from_bytes(data, 'big')

	for t in range(3000):
		x = random_bytes()
		y = random_bytes()
		op = '+-*/%'[t % 5]
		if op == '-' and x < y:
			x, y = y, x
		if op in '/%' and y == 0:
			continue
		r = {'+': x + y, '-': x - y, '*': x * y, '/': x // y, '%': x % y}[op]
		digest.update(r.to_bytes(128, 'big'))
		cases += 1
	digest.update((123456789012345678901234567890).to_bytes(16, 'big'))
	cases += 1
	return cases, digest.value


def xtea_encrypt(v0, v1, key):
	total, delta, mask = 0, 0x9e3779b9, 0xffffffff
	for _ in range(32):
		v0 = (v0 + ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (total + key[total & 3]))) & mask
		total = (total + delta) & mask
		v1 = (v1 + ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (total + key[(total >> 11) & 3]))) & mask
	return v0, v1


def xtea_bytes(key_bytes, block_bytes):
	key = [int.from_bytes(key_bytes[i:i + 4], 'little') for i in range(0, 16, 4)]
	v0 = int.from_bytes(block_bytes[0:4], 'little')
	v1 = int.from_bytes(block_bytes[4:8], 'little')
	v0, v1 = xtea_encrypt(v0, v1, key)
	return v0.to_bytes(4, 'little') + v1.to_bytes(4, 'little')


def hsv_to_rgb(h, s, v):
	if v == 0.0:
		return 0, 0, 0
	if s == 0.0:
		x = int(v * 255.0)
		return x, x, x
	if h < 1.0 / 6:
		b = (1.0 - s) * v
		g = (v - b) * h * 6.0 + b
		r = v
	elif h < 1.0 / 3:
		g = v
		b = (1.0 - s) * v
		r = v - (h - 1.0 / 6) * (v - b) * 6.0
	elif h < 0.5:
		g = v
		r = (1.0 - s) * v
		b = (h - 1.0 / 3) * (v - r) * 6.0 + r
	elif h < 2.0 / 3:
		b = v
		r = (1.0 - s) * v
		g = v - (h - 0.5) * (v - r) * 6.0
	elif h < 5.0 / 6:
		b = v
		g = (1.0 - s) * v
		r = (h - 2.0 / 3) * (v - g) * 6.0 + g
	else:
		r = v
		g = (1.0 - s) * v
		b = v - (h - 5.0 / 6) * (v - g) * 6.0
	return int(r * 255.0), int(g * 255.0), int(b * 255.0)


def outfit_color(color):
	if color >= 133:
		color = 0
	if color % 19 == 0:
		return hsv_to_rgb(0.0, 0.0, 1.0 - (color // 19) * (1.0 / 7))
	s, v = [(0.25, 1.0), (0.25, 0.75), (0.5, 0.75), (0.667, 0.75), (1.0, 1.0), (1.0, 0.75), (1.0, 0.5)][color // 19]
	return hsv_to_rgb((color % 19) * (1.0 / 18), s, v)


def main():
	cases, digest = vlong_cases()
	print(f'vlong cases {cases}, digest 0x{digest:016x}')
	print(f'modulus: {MODULUS.bit_length()} bits, first byte 0x{MODULUS.to_bytes(128, "big")[0]:02x}')
	print(f'rsa 2 -> {pow(2, 65537, MODULUS).to_bytes(128, "big").hex()}')
	print(f'decimal 123456789012345678901234567890 -> {(123456789012345678901234567890).to_bytes(16, "big").hex()}')
	for key, block in ((bytes(16), bytes(8)), (bytes(range(16)), bytes(range(0x41, 0x49))),
			(bytes.fromhex('2bd6459f82c5b300952c49104881ff48'), bytes.fromhex('ea024714ad5c4d84'))):
		print(f'xtea key {key.hex()} block {block.hex()} -> {xtea_bytes(key, block).hex()}')
	digest = Fnv()
	for color in range(134):
		digest.update(bytes(outfit_color(color)))
	print(f'outfit colours 0..133 digest 0x{digest.value:016x}')
	for color in (0, 1, 18, 19, 57, 95, 114, 132):
		print(f'outfit colour {color}: {outfit_color(color)}')


if __name__ == '__main__':
	main()
