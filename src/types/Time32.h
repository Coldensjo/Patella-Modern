// Time32.h: the 4-byte time fields of the original's frozen layout, and their one conversion from and to the
// client's 64-bit time. Patella-Modern issue #62 (notes/modernization-plan.md, section 7, M4; notes/decisions.md,
// "64-bit time everywhere"). Not part of the original source.
//
// The client reads the wall clock in 64 bits (the platform clock's WallSeconds, time(): seconds since 1970-01-01 00:00
// UTC) and computes with that time. Two fields of the original's layout hold a time in 4 bytes, and keep them until
// the layout unfreezes (M6): TCreature::SkullTime (+0x98, Creatures.h) and TTalkLine::Time (Talk.h). They are
// Time32, and each is written through StoreTime32 and read through LoadTime32, nowhere else.
//
// StoreTime32 keeps what the original kept: its time() (MSVC 7.1's, Tibia.exe 0x5121ea, decompiled with REA) was
// the 64-bit count of seconds from GetSystemTimeAsFileTime truncated to its low 32 bits, never -1. So a field holds
// the time itself up to 2038-01-19 03:14:07 UTC (INT32_MAX), then the original's wrapped value: negative up to
// 2106-02-07 06:28:15 UTC, then from 0 again. The same on every platform (the UCRT's _time32, -1 after 2038-01-18
// 23:59:59 UTC, and the old Linux stand-in, -1 after INT32_MAX, are no longer called).
//
// LoadTime32 gives the field back in 64 bits as seen at a time Now: the time nearest Now with the field's 32 bits.
// For a field and a Now both from 1970 to INT32_MAX that is the field itself, so every comparison the original made
// in 32 bits gives the same result; after 2038 the comparison goes on as before 2038, where the original's
// wrapped (a field written up to 68 years before or after Now comes back exactly).

#pragma once

#include <cstdint>

// A time field of the original's layout: seconds since 1970-01-01 00:00 UTC in 32 bits (the original's time_t).
using Time32 = std::int32_t;
static_assert(sizeof(Time32) == 4 && alignof(Time32) == 4, "Time32: the original's 4-byte time_t");

// The 32 bits a Time32 field keeps of Time: its low 32 bits as a signed number, what the original's 32-bit time_t
// held (Time itself from 0 to INT32_MAX).
constexpr Time32 StoreTime32(std::int64_t Time)
{
	return static_cast<Time32>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(Time)));
}

// The 64-bit time that Stored (a StoreTime32 result) stands for at Now: the time nearest Now whose StoreTime32 is
// Stored, from Now - 2^31 to Now + 2^31 - 1. Stored itself when Stored and Now are both from 0 to INT32_MAX.
constexpr std::int64_t LoadTime32(Time32 Stored, std::int64_t Now)
{
	// Stored - Now in the 32-bit circle, as a signed number from -2^31 to 2^31 - 1
	const std::uint32_t Difference = static_cast<std::uint32_t>(Stored) - static_cast<std::uint32_t>(StoreTime32(Now));
	return Now + static_cast<std::int32_t>(Difference);
}
