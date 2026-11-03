// Png.h: the frame dump's pictures (issue #22; notes/testmode.md, "The frame dump").
//
// The test build writes a frame of the Vulkan engine as an 8-bit RGB PNG, and the frame's
// device-filtered pixels (src/vulkan/FilterMask.h) as a second one, white where filtered, black
// elsewhere: the forms tools/compare_rendering.py writes and reads (<scene>.png and
// <scene>.filtered.png), so issue #23 can compare them with its reference frames under the same
// tolerance. No library: the image data is zlib with stored (uncompressed) deflate blocks, filter 0
// on every row. Portable: no platform header.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace patella::testmode {

// CRC-32 (ISO 3309, as PNG's chunks and zlib.crc32), continuing from Crc (0 to start).
[[nodiscard]] std::uint32_t Crc32(std::span<const std::uint8_t> Data, std::uint32_t Crc = 0);
// Adler-32 (zlib's check value).
[[nodiscard]] std::uint32_t Adler32(std::span<const std::uint8_t> Data);
// Data as a zlib stream of stored deflate blocks.
[[nodiscard]] std::vector<std::uint8_t> ZlibStored(std::span<const std::uint8_t> Data);

// A PNG of Width x Height pixels, Rgb: 3 bytes (red, green, blue) per pixel, rows top-down without gaps.
[[nodiscard]] std::vector<std::uint8_t> EncodePngRgb(int Width, int Height, std::span<const std::uint8_t> Rgb);
// The RGB bytes of D3DCOLOR pixels (B, G, R, X bytes in memory), rows Pitch bytes apart.
[[nodiscard]] std::vector<std::uint8_t> BgrxToRgb(const std::uint8_t *Bits, std::size_t Pitch, int Width, int Height);
// The RGB bytes of a mask (a byte per pixel): white where it is not 0, black elsewhere.
[[nodiscard]] std::vector<std::uint8_t> MaskToRgb(std::span<const std::uint8_t> Mask);

} // namespace patella::testmode
