// client.cpp: what the golden runner's objects take from runner.cpp, for the fuzz targets that link them in its place
// (Patella-Modern issue #132; as tests/golden/bounded_reads_test.cpp has them).

#include "common.h"

int Failures = 0;
int Checks = 0;
int FailuresPrinted = 20;
namespace {
uint32 Seed = 12345;
} // namespace
uint32 Random()
{
	Seed = Seed * 1103515245 + 12345;
	return Seed >> 8;
}
uint32 GetRandomSeed()
{
	return Seed;
}
void SetRandomSeed(uint32 Value)
{
	Seed = Value;
}
void RandomText(char *Text, int Length)
{
	for (int i = 0; i < Length; i++)
		Text[i] = 'a';
	Text[Length] = 0;
}
