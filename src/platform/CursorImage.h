// CursorImage.h: a monochrome cursor resource as the bitmaps of a monochrome SDL cursor (Patella-Modern issue #123,
// plan M8; notes/decisions.md, "#123: input over SDL3"). Not part of the original source. Portable: no platform header.
//
// The client's own cursors (resources 0x84 and 0x89, the crosshair and the magnifier: cursor_132.cur, cursor_137.cur of
// the extracted resources) are 32x32 monochrome cursors. An RT_CURSOR resource is the hot spot (two 16-bit numbers)
// followed by a DIB: a BITMAPINFOHEADER with twice the height, a palette of two colours, the XOR bitmap and the AND
// bitmap, each 1 bit a pixel, rows bottom-up, padded to 4 bytes. Windows draws a pixel with AND 0 in the XOR bit's
// colour, AND 1 XOR 0 transparent, AND 1 XOR 1 inverted. SDL_CreateCursor takes rows top-down, 1 bit a pixel, padded
// to bytes: mask 1 data 1 black, mask 1 data 0 white, mask 0 data 0 transparent, mask 0 data 1 inverted (on Windows;
// black elsewhere). DecodeMonochromeCursor turns the one into the other.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace patella::platform {

struct TMonochromeCursor {
	int Width = 0;
	int Height = 0;
	int HotX = 0;
	int HotY = 0;
	// SDL_CreateCursor's data and mask: Height rows of (Width + 7) / 8 bytes, top-down, the leftmost pixel in the high
	// bit.
	std::vector<std::uint8_t> Data;
	std::vector<std::uint8_t> Mask;
};

namespace detail {

constexpr std::uint32_t ReadLittle(std::span<const std::uint8_t> Bytes, std::size_t Offset, std::size_t Size) noexcept
{
	std::uint32_t Value = 0;
	for (std::size_t i = 0; i < Size; i++)
		Value |= static_cast<std::uint32_t>(Bytes[Offset + i]) << (8 * i);
	return Value;
}

} // namespace detail

#if !defined(_WIN32)
// Issue #131 (cmake/LinuxX64.cmake, tools/embed_cursors.py): the Linux client's built-in copy of a cursor resource (an
// RT_CURSOR image), or empty when the build has none of that number.
[[nodiscard]] std::span<const std::uint8_t> EmbeddedCursor(int Id);
#endif

// An RT_CURSOR resource's bytes (the hot spot and the DIB) as a monochrome cursor; nothing where they are not a 1-bit
// cursor of a sensible size or are too short.
[[nodiscard]] inline std::optional<TMonochromeCursor> DecodeMonochromeCursor(std::span<const std::uint8_t> Resource)
{
	constexpr std::size_t HotSpotSize = 4;
	constexpr std::size_t HeaderSize = 40; // BITMAPINFOHEADER
	if (Resource.size() < HotSpotSize + HeaderSize)
		return std::nullopt;
	const std::span<const std::uint8_t> Bitmap = Resource.subspan(HotSpotSize);
	const std::uint32_t InfoSize = detail::ReadLittle(Bitmap, 0, 4);
	const auto Width = static_cast<std::int32_t>(detail::ReadLittle(Bitmap, 4, 4));
	const auto DoubleHeight = static_cast<std::int32_t>(detail::ReadLittle(Bitmap, 8, 4));
	const std::uint32_t BitCount = detail::ReadLittle(Bitmap, 14, 2);
	const std::uint32_t Compression = detail::ReadLittle(Bitmap, 16, 4);
	std::uint32_t Colours = detail::ReadLittle(Bitmap, 32, 4);
	if (Colours == 0)
		Colours = 2;
	if (InfoSize < HeaderSize || BitCount != 1 || Compression != 0 || Colours > 2 || Width <= 0 || Width > 256 || DoubleHeight <= 0 || DoubleHeight > 512 || DoubleHeight % 2 != 0)
		return std::nullopt;
	const int Height = DoubleHeight / 2;
	const std::size_t Stride = ((static_cast<std::size_t>(Width) + 31) / 32) * 4;
	const std::size_t PaletteOffset = InfoSize;
	const std::size_t XorOffset = PaletteOffset + static_cast<std::size_t>(Colours) * 4;
	const std::size_t AndOffset = XorOffset + Stride * static_cast<std::size_t>(Height);
	if (Bitmap.size() < AndOffset + Stride * static_cast<std::size_t>(Height))
		return std::nullopt;
	// Whether each palette entry (RGBQUAD: blue, green, red, reserved) is black; a missing second entry is white.
	bool Black[2] = {true, false};
	for (std::uint32_t i = 0; i < Colours; i++) {
		const std::size_t Entry = PaletteOffset + static_cast<std::size_t>(i) * 4;
		Black[i] = Bitmap[Entry] == 0 && Bitmap[Entry + 1] == 0 && Bitmap[Entry + 2] == 0;
	}

	TMonochromeCursor Cursor;
	Cursor.Width = Width;
	Cursor.Height = Height;
	Cursor.HotX = static_cast<int>(detail::ReadLittle(Resource, 0, 2));
	Cursor.HotY = static_cast<int>(detail::ReadLittle(Resource, 2, 2));
	const std::size_t RowBytes = (static_cast<std::size_t>(Width) + 7) / 8;
	Cursor.Data.assign(RowBytes * static_cast<std::size_t>(Height), 0);
	Cursor.Mask.assign(RowBytes * static_cast<std::size_t>(Height), 0);
	for (int y = 0; y < Height; y++) {
		const std::size_t Source = static_cast<std::size_t>(Height - 1 - y) * Stride; // bottom-up
		for (int x = 0; x < Width; x++) {
			const std::size_t Byte = static_cast<std::size_t>(x) / 8;
			const auto Bit = static_cast<std::uint8_t>(0x80U >> (static_cast<unsigned int>(x) % 8));
			const bool Xor = (Bitmap[XorOffset + Source + Byte] & Bit) != 0;
			const bool And = (Bitmap[AndOffset + Source + Byte] & Bit) != 0;
			// Opaque: the XOR bit's colour (data 1 black); AND 1: transparent, or inverted where XOR is 1.
			const bool DataBit = And ? Xor : Black[Xor ? 1 : 0];
			const std::size_t Target = static_cast<std::size_t>(y) * RowBytes + Byte;
			if (DataBit)
				Cursor.Data[Target] |= Bit;
			if (!And)
				Cursor.Mask[Target] |= Bit;
		}
	}
	return Cursor;
}

} // namespace patella::platform
