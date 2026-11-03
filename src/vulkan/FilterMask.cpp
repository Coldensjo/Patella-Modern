// FilterMask.cpp: which pixels of the engine's images come from the device's filter (FilterMask.h).

#include "vulkan/FilterMask.h"
#include "vulkan/Quads.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace patella::vulkan {

namespace {

int Clamp(int Value, int Size)
{
	return Value < 0 ? 0 : Value >= Size ? Size - 1
										 : Value;
}

int Wrap(int Value, int Size)
{
	return ((Value % Size) + Size) % Size;
}

} // namespace

void TFilterMasks::Ensure(int Image, int Width, int Height)
{
	if (Image < 0)
		return;
	if (Masks.size() <= static_cast<size_t>(Image))
		Masks.resize(static_cast<size_t>(Image) + 1);
	TMask &Mask = Masks[static_cast<size_t>(Image)];
	if (Mask.Width != Width || Mask.Height != Height || Mask.Pixels.empty()) {
		Mask.Width = Width;
		Mask.Height = Height;
		Mask.Pixels.assign(static_cast<size_t>(Width > 0 ? Width : 0) * static_cast<size_t>(Height > 0 ? Height : 0),
			Transparent);
	}
}

void TFilterMasks::Clear()
{
	Masks.clear();
}

TFilterMasks::TMask *TFilterMasks::Find(int Image)
{
	if (Image < 0 || static_cast<size_t>(Image) >= Masks.size() || Masks[static_cast<size_t>(Image)].Pixels.empty())
		return nullptr;
	return &Masks[static_cast<size_t>(Image)];
}

uint8_t TFilterMasks::At(int Image, int x, int y) const
{
	if (Image < 0 || static_cast<size_t>(Image) >= Masks.size())
		return 0;
	const TMask &Mask = Masks[static_cast<size_t>(Image)];
	if (x < 0 || y < 0 || x >= Mask.Width || y >= Mask.Height || Mask.Pixels.empty())
		return 0;
	return Mask.Pixels[static_cast<size_t>(y) * static_cast<size_t>(Mask.Width) + static_cast<size_t>(x)];
}

void TFilterMasks::Fill(int Image, int Left, int Top, int Right, int Bottom)
{
	TMask *Mask = Find(Image);
	if (Mask == nullptr)
		return;
	Left = Left < 0 ? 0 : Left;
	Top = Top < 0 ? 0 : Top;
	Right = Right > Mask->Width ? Mask->Width : Right;
	Bottom = Bottom > Mask->Height ? Mask->Height : Bottom;
	for (int y = Top; y < Bottom && Left < Right; y++)
		std::memset(&Mask->Pixels[static_cast<size_t>(y) * static_cast<size_t>(Mask->Width) + static_cast<size_t>(Left)], 0,
			static_cast<size_t>(Right - Left));
}

void TFilterMasks::Upload(int Image, int x, int y, int Width, int Height, const uint32_t *Pixels)
{
	TMask *Mask = Find(Image);
	if (Mask == nullptr)
		return;
	for (int i = 0; i < Height; i++) {
		for (int j = 0; j < Width; j++) {
			if (x + j < 0 || y + i < 0 || x + j >= Mask->Width || y + i >= Mask->Height)
				continue;
			const uint32_t Pixel = Pixels[static_cast<size_t>(i) * static_cast<size_t>(Width) + static_cast<size_t>(j)];
			Mask->Pixels[static_cast<size_t>(y + i) * static_cast<size_t>(Mask->Width) + static_cast<size_t>(x + j)] = (Pixel >> 24) == 0 ? Transparent : 0;
		}
	}
}

void TFilterMasks::Draw(int Target, int Texture, TFilter Filter, const TQuad *Quads, size_t Count)
{
	TMask *To = Find(Target);
	TMask *FromMask = Find(Texture);
	if (To == nullptr || FromMask == nullptr)
		return;
	// A draw into its own texture samples a copy taken before it (Renderer.cpp).
	std::vector<uint8_t> Copy;
	const uint8_t *From = FromMask->Pixels.data();
	if (Texture == Target) {
		Copy = FromMask->Pixels;
		From = Copy.data();
	}
	const int TextureWidth = FromMask->Width;
	const int TextureHeight = FromMask->Height;
	for (size_t i = 0; i < Count; i++) {
		const TQuad &Q = Quads[i];
		const int Left = Q.Left < 0 ? 0 : Q.Left;
		const int Top = Q.Top < 0 ? 0 : Q.Top;
		const int Right = Q.Right > To->Width ? To->Width : Q.Right;
		const int Bottom = Q.Bottom > To->Height ? To->Height : Q.Bottom;
		if (Left >= Right || Top >= Bottom)
			continue;
		const TQuadVertices Converted = ConvertQuad(Q, TextureWidth, TextureHeight, Filter);
		if (!Converted.Nearest) {
			for (int y = Top; y < Bottom; y++)
				std::memset(&To->Pixels[static_cast<size_t>(y) * static_cast<size_t>(To->Width) + static_cast<size_t>(Left)],
					Filtered, static_cast<size_t>(Right - Left));
			continue;
		}
		const TAxis Horizontal = MapAxis(Q.Left, Q.Right, Q.LeftU, Q.RightU, TextureWidth, Filter);
		const TAxis Vertical = MapAxis(Q.Top, Q.Bottom, Q.TopV, Q.BottomV, TextureHeight, Filter);
		for (int y = Top; y < Bottom; y++) {
			int ty = Vertical.CopyStart + (y - Q.Top);
			ty = Converted.Clamp ? Clamp(ty, TextureHeight) : Wrap(ty, TextureHeight);
			for (int x = Left; x < Right; x++) {
				int tx = Horizontal.CopyStart + (x - Q.Left);
				tx = Converted.Clamp ? Clamp(tx, TextureWidth) : Wrap(tx, TextureWidth);
				const uint8_t Source = From[static_cast<size_t>(ty) * static_cast<size_t>(TextureWidth) + static_cast<size_t>(tx)];
				if ((Source & Transparent) == 0)
					To->Pixels[static_cast<size_t>(y) * static_cast<size_t>(To->Width) + static_cast<size_t>(x)] = Source;
			}
		}
	}
}

} // namespace patella::vulkan
