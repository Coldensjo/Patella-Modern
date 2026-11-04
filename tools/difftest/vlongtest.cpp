// Prints results of the reconstructed vlong and TRSA code for checking against
// exact integer arithmetic (tools/difftest/vlongcheck.py). One line per case:
//   rsa <plain hex> <cipher hex>
//   <op> <a hex> <b hex> <result hex>      op is one of + - * / %
// Numbers are written big-endian through VlongToByteArray, so the conversions
// are checked too.

#include "../../src/TRSA.h"
#include <stdio.h>

static const char MODULUS[] =
	"124710459426827943004376449897985582167801707960697037164044904862948569380850421396904597686953877022394604239428185498284169068581802277612081027966724336319448537811441719076484340922854929273517308661370727105382899118999403808045846444647284499123164879035103627004668521005328367415259939915284902061793";

static unsigned Seed = 4242;
static unsigned Random()
{
	Seed = Seed * 1103515245 + 12345;
	return Seed >> 8;
}

static void PrintHex(const uint8 *Data, int Size)
{
	for (int i = 0; i < Size; i++)
		printf("%02x", Data[i]);
}

static void RandomBytes(uint8 *Data, int Size, int Used)
{
	memset(Data, 0, Size);
	for (int i = Size - Used; i < Size; i++)
		Data[i] = (uint8)Random();
}

int main()
{
	TRSA Rsa;
	Rsa.SetPublicKey(MODULUS);
	for (int t = 0; t < 200; t++) {
		uint8 Block[128];
		for (int i = 0; i < 128; i++)
			Block[i] = (uint8)Random();
		Block[0] &= 0x7f;
		if (t == 0)
			memset(Block, 0, sizeof(Block));
		printf("rsa ");
		PrintHex(Block, 128);
		Rsa.Encrypt(Block);
		printf(" ");
		PrintHex(Block, 128);
		printf("\n");
	}

	const char Ops[] = "+-*/%";
	for (int t = 0; t < 3000; t++) {
		uint8 A[64], B[64], R[128];
		RandomBytes(A, 64, 1 + Random() % 64);
		RandomBytes(B, 64, 1 + Random() % 64);
		char Op = Ops[t % 5];
		vlong x, y, r;
		ByteArrayToVlong(A, x);
		ByteArrayToVlong(B, y);
		if (Op == '-' && x < y) {
			vlong Swap = x;
			x = y;
			y = Swap;
			uint8 Tmp[64];
			memcpy(Tmp, A, 64);
			memcpy(A, B, 64);
			memcpy(B, Tmp, 64);
		}
		if ((Op == '/' || Op == '%') && y == vlong(0))
			continue;
		switch (Op) {
		case '+': r = x + y; break;
		case '-': r = x - y; break;
		case '*': r = x * y; break;
		case '/': r = x / y; break;
		case '%': r = x % y; break;
		}
		VlongToByteArray(r, R);
		printf("%c ", Op);
		PrintHex(A, 64);
		printf(" ");
		PrintHex(B, 64);
		printf(" ");
		PrintHex(R, 128);
		printf("\n");
	}

	vlong Decimal;
	DecimalToVlong("123456789012345678901234567890", Decimal);
	uint8 D[16];
	VlongToByteArray(Decimal, D);
	printf("dec 123456789012345678901234567890 ");
	PrintHex(D, 16);
	printf("\n");
	return 0;
}
