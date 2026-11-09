// WindowIcon.h: the Linux client's window icon (Patella-Modern issue #162; notes/decisions.md, "#162"). Not part of the
// original source. The decoder is portable (no platform header); on Windows the icon is the window class's, resource 128
// (SdlFrame.cpp), and nothing here is used.
//
// An RT_ICON resource is a DIB: a BITMAPINFOHEADER with twice the height, a palette, the colour bitmap (1, 4 or 8 bits a
// pixel) and the AND bitmap (1 bit a pixel), rows bottom-up, each padded to 4 bytes. AND 1 is transparent, AND 0 the
// palette's colour. DecodeIconImage turns that into rows of SDL_PIXELFORMAT_ARGB8888.

#pragma once

#include "platform/CursorImage.h" // detail::ReadLittle

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#if !defined(_WIN32)
struct SDL_Window;
#endif

namespace patella::platform {

struct TIconImage {
	int Width = 0;
	int Height = 0;
	std::vector<std::uint32_t> Pixels; // Height rows of Width pixels, top-down, each 0xAARRGGBB
};

#if !defined(_WIN32)
// The Linux client's built-in copy of the program's icon (one RT_ICON image; cmake/LinuxX64.cmake, tools/embed_cursors.py).
[[nodiscard]] std::span<const std::uint8_t> EmbeddedIcon();

// Sets the built-in icon as the window's (platform/WindowIconLinux.cpp). A failure is logged at the warn level, nothing
// else: the client goes on without an icon.
void ApplyWindowIcon(SDL_Window *Window);
#endif

// An RT_ICON resource's bytes as ARGB pixels; nothing where they are not a palette icon of a sensible size or are too
// short.
[[nodiscard]] inline std::optional<TIconImage> DecodeIconImage(std::span<const std::uint8_t> Resource)
{
	constexpr std::size_t HeaderSize = 40; // BITMAPINFOHEADER
	if (Resource.size() < HeaderSize)
		return std::nullopt;
	const std::uint32_t InfoSize = detail::ReadLittle(Resource, 0, 4);
	const auto Width = static_cast<std::int32_t>(detail::ReadLittle(Resource, 4, 4));
	const auto DoubleHeight = static_cast<std::int32_t>(detail::ReadLittle(Resource, 8, 4));
	const std::uint32_t BitCount = detail::ReadLittle(Resource, 14, 2);
	const std::uint32_t Compression = detail::ReadLittle(Resource, 16, 4);
	std::uint32_t Colours = detail::ReadLittle(Resource, 32, 4);
	if (InfoSize < HeaderSize || InfoSize > Resource.size() || (BitCount != 1 && BitCount != 4 && BitCount != 8) || Compression != 0 || Width <= 0 || Width > 256 || DoubleHeight <= 0 || DoubleHeight > 512 || DoubleHeight % 2 != 0)
		return std::nullopt;
	const std::uint32_t MostColours = 1U << BitCount;
	if (Colours == 0)
		Colours = MostColours;
	if (Colours > MostColours)
		return std::nullopt;
	const int Height = DoubleHeight / 2;
	const auto Rows = static_cast<std::size_t>(Height);
	const std::size_t ColourStride = ((static_cast<std::size_t>(Width) * BitCount + 31) / 32) * 4;
	const std::size_t AndStride = ((static_cast<std::size_t>(Width) + 31) / 32) * 4;
	const std::size_t PaletteOffset = InfoSize;
	const std::size_t ColourOffset = PaletteOffset + static_cast<std::size_t>(Colours) * 4;
	const std::size_t AndOffset = ColourOffset + ColourStride * Rows;
	if (Resource.size() < AndOffset + AndStride * Rows)
		return std::nullopt;

	TIconImage Icon;
	Icon.Width = Width;
	Icon.Height = Height;
	Icon.Pixels.assign(static_cast<std::size_t>(Width) * Rows, 0);
	const std::uint32_t PixelMask = MostColours - 1;
	for (std::size_t y = 0; y < Rows; y++) {
		const std::size_t SourceRow = Rows - 1 - y; // bottom-up
		for (std::size_t x = 0; x < static_cast<std::size_t>(Width); x++) {
			const std::uint8_t AndByte = Resource[AndOffset + SourceRow * AndStride + x / 8];
			if ((AndByte & (0x80U >> (x % 8))) != 0)
				continue; // transparent: stays 0
			const std::size_t BitPosition = x * BitCount;
			const std::uint8_t ColourByte = Resource[ColourOffset + SourceRow * ColourStride + BitPosition / 8];
			const std::uint32_t Index = (static_cast<std::uint32_t>(ColourByte) >> (8 - BitCount - BitPosition % 8)) & PixelMask;
			std::uint32_t Colour = 0; // an index past a short palette: black
			if (Index < Colours)
				Colour = detail::ReadLittle(Resource, PaletteOffset + static_cast<std::size_t>(Index) * 4, 3); // RGBQUAD: blue, green, red
			Icon.Pixels[y * static_cast<std::size_t>(Width) + x] = 0xff000000U | Colour;
		}
	}
	return Icon;
}

} // namespace patella::platform
