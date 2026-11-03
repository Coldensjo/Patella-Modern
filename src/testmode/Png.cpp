// Png.cpp: PNG pictures with stored deflate blocks (Png.h; issue #22).

#include "Png.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace patella::testmode {

namespace {

// The largest stored deflate block.
constexpr std::size_t StoredBlock = 65535;

constexpr std::array<std::uint32_t, 256> MakeCrcTable()
{
	std::array<std::uint32_t, 256> Table{};
	for (std::uint32_t i = 0; i < 256; i++) {
		std::uint32_t Value = i;
		for (int k = 0; k < 8; k++)
			Value = (Value & 1U) != 0 ? 0xedb88320U ^ (Value >> 1U) : Value >> 1U;
		Table.at(i) = Value;
	}
	return Table;
}

constexpr std::array<std::uint32_t, 256> CrcTable = MakeCrcTable();

void PutBig32(std::vector<std::uint8_t> &Out, std::uint32_t Value)
{
	Out.push_back(static_cast<std::uint8_t>(Value >> 24U));
	Out.push_back(static_cast<std::uint8_t>(Value >> 16U));
	Out.push_back(static_cast<std::uint8_t>(Value >> 8U));
	Out.push_back(static_cast<std::uint8_t>(Value));
}

void Chunk(std::vector<std::uint8_t> &Out, std::string_view Kind, std::span<const std::uint8_t> Data)
{
	PutBig32(Out, static_cast<std::uint32_t>(Data.size()));
	std::size_t Start = Out.size();
	for (char c : Kind)
		Out.push_back(static_cast<std::uint8_t>(c));
	Out.insert(Out.end(), Data.begin(), Data.end());
	std::uint32_t Crc = Crc32(std::span<const std::uint8_t>(Out).subspan(Start));
	PutBig32(Out, Crc);
}

} // namespace

std::uint32_t Crc32(std::span<const std::uint8_t> Data, std::uint32_t Crc)
{
	Crc = ~Crc;
	for (std::uint8_t Byte : Data)
		Crc = CrcTable.at((Crc ^ Byte) & 0xffU) ^ (Crc >> 8U);
	return ~Crc;
}

std::uint32_t Adler32(std::span<const std::uint8_t> Data)
{
	constexpr std::uint32_t Modulus = 65521;
	std::uint32_t a = 1;
	std::uint32_t b = 0;
	for (std::uint8_t Byte : Data) {
		a = (a + Byte) % Modulus;
		b = (b + a) % Modulus;
	}
	return (b << 16U) | a;
}

std::vector<std::uint8_t> ZlibStored(std::span<const std::uint8_t> Data)
{
	std::vector<std::uint8_t> Out;
	Out.reserve(Data.size() + Data.size() / StoredBlock * 5 + 16);
	// CMF: deflate, 32K window; FLG: no dictionary, the check bits make CMF*256+FLG a multiple of 31.
	Out.push_back(0x78);
	Out.push_back(0x01);
	std::size_t Position = 0;
	do {
		std::size_t Length = std::min(StoredBlock, Data.size() - Position);
		const bool Last = Position + Length == Data.size();
		Out.push_back(Last ? 1 : 0); // BFINAL, BTYPE 00 (stored), aligned to the byte
		Out.push_back(static_cast<std::uint8_t>(Length));
		Out.push_back(static_cast<std::uint8_t>(Length >> 8U));
		Out.push_back(static_cast<std::uint8_t>(~Length));
		Out.push_back(static_cast<std::uint8_t>(~Length >> 8U));
		Out.insert(Out.end(), Data.begin() + static_cast<std::ptrdiff_t>(Position),
			Data.begin() + static_cast<std::ptrdiff_t>(Position + Length));
		Position += Length;
	} while (Position < Data.size());
	PutBig32(Out, Adler32(Data));
	return Out;
}

std::vector<std::uint8_t> EncodePngRgb(int Width, int Height, std::span<const std::uint8_t> Rgb)
{
	auto Row = static_cast<std::size_t>(Width) * 3;
	std::vector<std::uint8_t> Raw;
	Raw.reserve((Row + 1) * static_cast<std::size_t>(Height));
	for (int y = 0; y < Height; y++) {
		Raw.push_back(0); // filter type 0 (none)
		auto Start = Rgb.begin() + static_cast<std::ptrdiff_t>(Row * static_cast<std::size_t>(y));
		Raw.insert(Raw.end(), Start, Start + static_cast<std::ptrdiff_t>(Row));
	}
	std::vector<std::uint8_t> Out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	std::vector<std::uint8_t> Header;
	PutBig32(Header, static_cast<std::uint32_t>(Width));
	PutBig32(Header, static_cast<std::uint32_t>(Height));
	// Bit depth 8, colour type 2 (RGB), compression 0, filter 0, no interlace.
	Header.insert(Header.end(), {8, 2, 0, 0, 0});
	Chunk(Out, "IHDR", Header);
	Chunk(Out, "IDAT", ZlibStored(Raw));
	Chunk(Out, "IEND", {});
	return Out;
}

std::vector<std::uint8_t> BgrxToRgb(const std::uint8_t *Bits, std::size_t Pitch, int Width, int Height)
{
	std::vector<std::uint8_t> Rgb;
	Rgb.reserve(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 3);
	for (int y = 0; y < Height; y++) {
		const std::span<const std::uint8_t> Line(Bits + Pitch * static_cast<std::size_t>(y), static_cast<std::size_t>(Width) * 4);
		for (std::size_t x = 0; x < static_cast<std::size_t>(Width); x++) {
			Rgb.push_back(Line[x * 4 + 2]);
			Rgb.push_back(Line[x * 4 + 1]);
			Rgb.push_back(Line[x * 4]);
		}
	}
	return Rgb;
}

std::vector<std::uint8_t> MaskToRgb(std::span<const std::uint8_t> Mask)
{
	std::vector<std::uint8_t> Rgb;
	Rgb.reserve(Mask.size() * 3);
	for (std::uint8_t Value : Mask) {
		std::uint8_t Shade = Value != 0 ? 0xff : 0;
		Rgb.insert(Rgb.end(), {Shade, Shade, Shade});
	}
	return Rgb;
}

} // namespace patella::testmode
