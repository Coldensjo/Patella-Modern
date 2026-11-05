// vlong: arbitrary-precision unsigned arithmetic for RSA, at 0x50bff0-0x50eae7.
//
// This is George Barwood's public-domain vlong library (1999), with CipSoft's
// changes: a virtual destructor (vtable 0x574048), conversions between vlong and
// big-endian byte arrays or decimal text, and overloads taking an integer by
// reference. The file name "vlong" matches the link order (right after Utils).
// Errors are thrown as const char*.

#pragma once

#include "common.h"
#include "compat/span.h"
#include <memory>

class flex_unit {
public:
	unsigned n; // +0x0, units in use
private:
	std::unique_ptr<unsigned[]> a; // +0x4, the units (issue #58: their only owner)
	unsigned z;                    // +0x8, units allocated
public:
	flex_unit()
	{
		z = 0;
		a = nullptr;
		n = 0;
	}
	~flex_unit();
	flex_unit(const flex_unit &) = delete;
	flex_unit &operator=(const flex_unit &) = delete;
	void clear() { n = 0; }
	[[nodiscard]] unsigned get(unsigned i) const
	{
		if (i >= n) return 0;
		return a[i];
	}
	void set(unsigned i, unsigned x);
	void reserve(unsigned x);
	void fast_mul(flex_unit &x, flex_unit &y, unsigned keep);
};

class vlong_value : public flex_unit {
public:
	unsigned share; // +0xc, number of extra vlongs sharing this value

	vlong_value() { share = 0; }
	[[nodiscard]] int is_zero() const { return n == 0; }
	[[nodiscard]] int test(unsigned i) const;
	[[nodiscard]] unsigned bits() const;
	int cf(vlong_value &x) const;
	void shl();
	void shr();
	void shr(unsigned n);
	void add(vlong_value &x);
	void subtract(vlong_value &x);
	void init(unsigned x)
	{
		clear();
		set(0, x);
	}
	void copy(vlong_value &x);
	void mul(vlong_value &x, vlong_value &y);
	void divide(vlong_value &x, vlong_value &y, vlong_value &rem);
};

class vlong {
public:
	friend vlong operator+(const vlong &x, const vlong &y);
	friend vlong operator-(const vlong &x, const vlong &y);
	friend vlong operator*(const vlong &x, const vlong &y);
	friend vlong operator/(const vlong &x, const vlong &y);
	friend vlong operator%(const vlong &x, const vlong &y);
	vlong &operator+=(const vlong &x);
	vlong &operator-=(const vlong &x);

	friend inline int operator!=(const vlong &x, const vlong &y) { return x.cf(y) != 0; }
	friend inline int operator==(const vlong &x, const vlong &y) { return x.cf(y) == 0; }
	friend inline int operator>=(const vlong &x, const vlong &y) { return x.cf(y) >= 0; } // 0x508970
	friend inline int operator<=(const vlong &x, const vlong &y) { return x.cf(y) <= 0; }
	friend inline int operator>(const vlong &x, const vlong &y) { return x.cf(y) > 0; }
	friend inline int operator<(const vlong &x, const vlong &y) { return x.cf(y) < 0; }

	// CipSoft additions; the binary has out-of-line copies (0x50d660, 0x50d750, 0x50d840). The
	// casts are the conversions the 32-bit build does implicitly; long has 8 bytes on Linux (issue #7).
	friend vlong operator+(const vlong &x, const long &y) { return x + vlong((unsigned)y); }
	friend vlong operator*(const vlong &x, const long &y) { return x * vlong((unsigned)y); }
	friend int operator<(const vlong &x, const long &y) { return x.cf(vlong((unsigned)y)) < 0; }

	vlong(unsigned x = 0);
	vlong(const vlong &x);
	virtual ~vlong(); // scalar deleting destructor at 0x50db70
	vlong &operator=(const vlong &x);

private:
	vlong_value *value; // +0x4
	int negative;       // +0x8
	[[nodiscard]] int cf(const vlong x) const;
	void docopy();
	friend class monty;
	friend void ByteArrayToVlong(patella::TSpan<const uint8> Data, vlong &Number);
	friend void VlongToByteArray(vlong Number, patella::TSpan<uint8> Data);
};

vlong modexp(const vlong &x, const vlong &e, const vlong &m); // m must be odd
vlong modinv(const vlong &a, const vlong &m);

// CipSoft additions. Byte arrays are big-endian. The original's pointer and uint16 size are a span (issue #60).
void ByteArrayToVlong(patella::TSpan<const uint8> Data, vlong &Number); // 0x50c660
void VlongToByteArray(vlong Number, patella::TSpan<uint8> Data);        // 0x50d530
void DecimalToVlong(const char *pDecimal, vlong &Number);               // 0x50e360
