// vlong.cpp: reconstructed from Tibia.exe 7.72 (0x50bff0-0x50eae7).
// George Barwood's public-domain vlong (1999) with CipSoft's additions; see
// vlong.h. Functions are in binary order. Barwood functions the client does not
// use (gcd, the unsigned conversion) are absent from the binary and left out.

#include "vlong.h"
#include "common.h"
#include "compat/span.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

constexpr auto BPU = 8 * sizeof(unsigned); // bits per unit (size_t, as the macro was)
#define lo(x) ((x) & ((1 << (BPU / 2)) - 1))
#define hi(x) ((x) >> (BPU / 2))
#define lh(x) ((x) << (BPU / 2))

// flex_unit::a as a std::unique_ptr<unsigned[]> (issue #58): a pointer at the original's offset. reserve() frees the
// old block (reset()) before it moves the new one in, the destructor frees it where the original had delete[].
static_assert(sizeof(std::unique_ptr<unsigned[]>) == sizeof(unsigned *)
		&& alignof(std::unique_ptr<unsigned[]>) == alignof(unsigned *),
	"std::unique_ptr<unsigned[]> is a pointer");

// 0x50bff0
void flex_unit::reserve(unsigned x)
{
	if (x > z) {
		// The new block in a unique_ptr from the start (GCC's analyzer, Debug: a raw na handed to reset() is a "leak").
		std::unique_ptr<unsigned[]> na(new unsigned[x]);
		for (unsigned i = 0; i < n; i += 1)
			na[i] = a[i];
		a.reset();
		a = std::move(na);
		z = x;
	}
}

// 0x50c050. GCC's analyzer (issue #63) follows vlong_value::copy into a reserve that keeps a null a, which needs
// i + 1 to wrap to 0: i is below a source's n, which never reaches UINT_MAX (a false positive).
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-null-dereference\"")
void flex_unit::set(unsigned i, unsigned x)
{
	if (i < n) {
		a[i] = x;
		if (x == 0)
			while (n && a[n - 1] == 0)
				n -= 1; // normalise
	} else if (x) {
		reserve(i + 1);
		for (unsigned j = n; j < i; j += 1)
			a[j] = 0;
		a[i] = x;
		n = i + 1;
	}
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")

// 0x50c0d0. *this = (x*y) % 2**keep
void flex_unit::fast_mul(flex_unit &x, flex_unit &y, unsigned keep)
{
	unsigned i, limit = (keep + BPU - 1) / BPU; // size of result in words
	reserve(limit);
	for (i = 0; i < limit; i += 1)
		a[i] = 0;
	unsigned min = x.n;
	if (min > limit)
		min = limit;
	for (i = 0; i < min; i += 1) {
		const unsigned m = x.a[i];
		unsigned c = 0; // carry
		// jmin, not min, which hid the outer one (MSVC /analyze's C6246, issue #63).
		unsigned jmin = i + y.n;
		if (jmin > limit)
			jmin = limit;
		unsigned j;
		for (j = i; j < jmin; j += 1) {
			// c:a[j] = a[j] + c + m*y.a[j-i];
			unsigned w, v = a[j], p = y.a[j - i];
			v += c;
			c = (v < c);
			w = lo(p) * lo(m);
			v += w;
			c += (v < w);
			w = lo(p) * hi(m);
			c += hi(w);
			w = lh(w);
			v += w;
			c += (v < w);
			w = hi(p) * lo(m);
			c += hi(w);
			w = lh(w);
			v += w;
			c += (v < w);
			c += hi(p) * hi(m);
			a[j] = v;
		}
		while (c && j < limit) {
			a[j] += c;
			c = a[j] < c;
			j += 1;
		}
	}
	// eliminate unwanted bits (1u: with keep 31 the int (1 << keep) - 1 overflowed, which UBSan
	// reports; the mask has the same bits; issue #7)
	keep %= BPU;
	if (keep)
		a[limit - 1] &= (1u << keep) - 1;
	// calculate n
	while (limit && a[limit - 1] == 0)
		limit -= 1;
	n = limit;
}

int vlong_value::test(unsigned i) const
{
	return (get(i / BPU) & (1 << (i % BPU))) != 0;
}

// 0x50c2b0
unsigned vlong_value::bits() const
{
	unsigned x = n * BPU;
	while (x && test(x - 1) == 0)
		x -= 1;
	return x;
}

// 0x50c300
int vlong_value::cf(vlong_value &x) const
{
	if (n > x.n)
		return +1;
	if (n < x.n)
		return -1;
	unsigned i = n;
	while (i) {
		i -= 1;
		if (get(i) > x.get(i))
			return +1;
		if (get(i) < x.get(i))
			return -1;
	}
	return 0;
}

// Inlined into divide.
void vlong_value::shl()
{
	unsigned carry = 0;
	const unsigned N = n; // necessary, since n can change
	for (unsigned i = 0; i <= N; i += 1) {
		const unsigned u = get(i);
		set(i, (u << 1) + carry);
		carry = u >> (BPU - 1);
	}
}

// Inlined into divide.
void vlong_value::shr()
{
	unsigned carry = 0;
	unsigned i = n;
	while (i) {
		i -= 1;
		const unsigned u = get(i);
		set(i, (u >> 1) + carry);
		carry = u << (BPU - 1);
	}
}

// 0x50c390
void vlong_value::shr(unsigned x)
{
	const unsigned delta = x / BPU;
	x %= BPU;
	for (unsigned i = 0; i < n; i += 1) {
		unsigned u = get(i + delta);
		if (x) {
			u >>= x;
			u += get(i + delta + 1) << (BPU - x);
		}
		set(i, u);
	}
}

// 0x50c420
void vlong_value::add(vlong_value &x)
{
	unsigned carry = 0;
	unsigned max = n;
	if (max < x.n)
		max = x.n;
	reserve(max);
	for (unsigned i = 0; i < max + 1; i += 1) {
		unsigned u = get(i);
		u = u + carry;
		carry = (u < carry);
		const unsigned ux = x.get(i);
		u = u + ux;
		carry += (u < ux);
		set(i, u);
	}
}

// 0x50c490
void vlong_value::subtract(vlong_value &x)
{
	unsigned carry = 0;
	const unsigned N = n;
	for (unsigned i = 0; i < N; i += 1) {
		unsigned ux = x.get(i);
		ux += carry;
		if (ux >= carry) {
			const unsigned u = get(i);
			const unsigned nu = u - ux;
			carry = nu > u;
			set(i, nu);
		}
	}
}

// 0x50c4f0
void vlong_value::copy(vlong_value &x)
{
	clear();
	unsigned i = x.n;
	while (i) {
		i -= 1;
		set(i, x.get(i));
	}
}

// 0x50c530. Clears the units before freeing them.
flex_unit::~flex_unit()
{
	unsigned i = z;
	while (i) {
		i -= 1;
		a[i] = 0;
	}
	a.reset();
}

// 0x50c560
vlong::vlong(unsigned x)
{
	value = new vlong_value;
	negative = 0;
	value->init(x);
}

// 0x50c5b0
vlong::vlong(const vlong &x)
{
	negative = x.negative;
	value = x.value;
	value->share += 1;
}

// 0x50c5d0. Gives this vlong its own copy before a change.
void vlong::docopy()
{
	if (value->share) {
		value->share -= 1;
		vlong_value *nv = new vlong_value;
		nv->copy(*value);
		value = nv;
	}
}

// 0x50c610. Barwood's, as in the binary: a self-assignment of an unshared value deletes it before taking it
// again (issue #63: kept until M6; nothing in the client assigns a vlong to itself).
vlong &vlong::operator=(const vlong &x) // NOLINT(bugprone-unhandled-self-assignment): the binary's, above
{
	if (value->share)
		value->share -= 1;
	else
		delete value;
	value = x.value;
	value->share += 1;
	negative = x.negative;
	return *this;
}

// 0x50c660. The first byte is the most significant. The original's parameters from the span (issue #60): a null
// pointer throws as before, and the size is cut to the uint16 the original took.
void ByteArrayToVlong(patella::TSpan<const uint8> Data, vlong &Number)
{
	const uint8 *pData = Data.data();
	const auto Size = static_cast<uint16>(Data.size());
	if (pData == nullptr)
		throw "pData pointer is NULL";
	const int Length = Size;
	Number.value->n = 0;
	for (int i = 0; i < Length; i += 4) {
		unsigned Unit = 0;
		if (i < Length)
			Unit = pData[i] << 24;
		if (i + 1 < Length)
			Unit += pData[i + 1] << 16;
		if (i + 2 < Length)
			Unit += pData[i + 2] << 8;
		if (i + 3 < Length)
			Unit += pData[i + 3];
		Number.value->set(((Length + 3) >> 2) - i / 4 - 1, Unit);
	}
}

// 0x50c720
void vlong_value::divide(vlong_value &x, vlong_value &y, vlong_value &rem)
{
	init(0);
	rem.copy(x);
	vlong_value m, s;
	m.copy(y);
	s.init(1);
	while (rem.cf(m) > 0) {
		m.shl();
		s.shl();
	}
	while (rem.cf(y) >= 0) {
		while (rem.cf(m) < 0) {
			m.shr();
			s.shr();
		}
		rem.subtract(m);
		add(s);
	}
}

// 0x50c9a0
vlong::~vlong()
{
	if (value->share)
		value->share -= 1;
	else
		delete value;
}

// 0x50c9d0
int vlong::cf(const vlong x) const
{
	const int neg = negative && !value->is_zero();
	if (neg == (x.negative && !x.value->is_zero()))
		return value->cf(*x.value);
	else if (neg)
		return -1;
	else
		return +1;
}

// 0x50cb50
vlong &vlong::operator+=(const vlong &x)
{
	if (negative == x.negative) {
		docopy();
		value->add(*x.value);
	} else if (value->cf(*x.value) >= 0) {
		docopy();
		value->subtract(*x.value);
	} else {
		const vlong tmp = *this;
		*this = x;
		*this += tmp;
	}
	return *this;
}

// 0x50cc80
vlong &vlong::operator-=(const vlong &x)
{
	if (negative != x.negative) {
		docopy();
		value->add(*x.value);
	} else if (value->cf(*x.value) >= 0) {
		docopy();
		value->subtract(*x.value);
	} else {
		const vlong tmp = *this;
		*this = x;
		*this -= tmp;
		negative = 1 - negative;
	}
	return *this;
}

// 0x50cdc0
vlong operator+(const vlong &x, const vlong &y)
{
	vlong result = x;
	result += y;
	return result;
}

// 0x50ceb0
vlong operator-(const vlong &x, const vlong &y)
{
	vlong result = x;
	result -= y;
	return result;
}

void vlong_value::mul(vlong_value &x, vlong_value &y)
{
	fast_mul(x, y, x.bits() + y.bits());
}

// 0x50cfa0
vlong operator*(const vlong &x, const vlong &y)
{
	vlong result;
	result.value->mul(*x.value, *y.value);
	result.negative = x.negative ^ y.negative;
	return result;
}

// 0x50d0c0
vlong operator/(const vlong &x, const vlong &y)
{
	vlong result;
	vlong_value rem;
	result.value->divide(*x.value, *y.value, rem);
	result.negative = x.negative ^ y.negative;
	return result;
}

// 0x50d200
vlong operator%(const vlong &x, const vlong &y)
{
	vlong result;
	vlong_value divide;
	divide.divide(*x.value, *y.value, *result.value);
	result.negative = x.negative;
	return result;
}

// Montgomery modular exponentiation. Its implicit destructor is at 0x50d330.
class monty {
	vlong R, R1, m, n1; // +0x00, +0x0c, +0x18, +0x24
	vlong T, k;         // +0x30, +0x3c, work registers
	unsigned N;         // +0x48, bits for R
	void mul(vlong &x, const vlong &y);

public:
	vlong exp(const vlong &x, const vlong &e);
	monty(const vlong &M);
};

// 0x50d530. Fills Size bytes, most significant first; the parameters from the span as above (issue #60).
void VlongToByteArray(vlong Number, patella::TSpan<uint8> Data)
{
	uint8 *pData = Data.data();
	const auto Size = static_cast<uint16>(Data.size());
	if (pData == nullptr)
		throw "pData pointer is NULL";
	const int Length = Size;
	memset(pData, 0, Length);
	for (int i = 0; i < Length; i += 4) {
		const unsigned Unit = Number.value->get(((Length + 3) >> 2) - i / 4 - 1);
		for (int j = 0; j < 4; j++) {
			if (i + j < Length)
				pData[i + j] = (uint8)(Unit >> ((3 - j) * 8));
		}
	}
}

// 0x50d8a0
void monty::mul(vlong &x, const vlong &y)
{
	// T = x*y;
	T.value->fast_mul(*x.value, *y.value, N * 2);
	// k = ( T * n1 ) % R;
	k.value->fast_mul(*T.value, *n1.value, N);
	// x = ( T + k*m ) / R;
	x.value->fast_mul(*k.value, *m.value, N * 2);
	x += T;
	x.value->shr(N);
	if (x >= m)
		x -= m;
}

// 0x50d930
vlong monty::exp(const vlong &x, const vlong &e)
{
	vlong result = R - m, t = (x * R) % m;
	const unsigned bits = e.value->bits();
	unsigned i = 0;
	while (1) {
		if (e.value->test(i))
			mul(result, t);
		i += 1;
		if (i == bits)
			break;
		mul(t, t);
	}
	return (result * R1) % m;
}

// 0x50dbc0. Returns i in range 1..m-1 such that i*a = 1 mod m; a must be in range 1..m-1.
vlong modinv(const vlong &a, const vlong &m)
{
	vlong j = 1, i = 0, b = m, c = a, x, y;
	while (c != 0) {
		x = b / c;
		y = b - x * c;
		b = c;
		c = y;
		y = j;
		j = i - j * x;
		i = y;
	}
	if (i < 0)
		i += m;
	return i;
}

// 0x50e360. Non-digit characters count as 0 (atoi), as in the binary.
void DecimalToVlong(const char *pDecimal, vlong &Number)
{
	if (pDecimal == nullptr)
		throw "pDecimal pointer is NULL";
	std::array<char, 2> Digit;
	Digit[0] = 0;
	Digit[1] = 1; // as in the binary; atoi stops at it
	Number = 0;
	for (Digit[0] = *pDecimal; Digit[0] != 0; Digit[0] = *++pDecimal) {
		const long Value = atoi(Digit.data()); // NOLINT(bugprone-unchecked-string-to-number-conversion): the binary's
		if (Value < 0 || Value > 9)
			throw "digit out of range";
		Number = Number * 10L + Value;
	}
}

// 0x50e5e0
monty::monty(const vlong &M)
{
	m = M;
	N = 0;
	R = 1;
	while (R < M) {
		R += R;
		N += 1;
	}
	R1 = modinv(R - m, m);
	n1 = R - modinv(m, R);
}

// 0x50ea70
vlong modexp(const vlong &x, const vlong &e, const vlong &m)
{
	monty me(m);
	return me.exp(x, e);
}
