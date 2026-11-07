// types/GraphicsTypes.h: the value types of the drawing interface (TRect, TRGBColor, TFont and the text
// alignments), moved here from graphics.h (Patella-Modern issue #116) so that Surface.cpp (TSurface's slots take them)
// and Skins.h (SkinProperties holds them) need them without graphics.cpp's module: the {Surface, graphics} module cycle
// was Surface.cpp's include of graphics.h for these types (notes/decisions.md, "#116"). Header only, portable
// (patella_core's header list). graphics.h exports it, so the files that include graphics.h see them as before.

#pragma once

#include "compat/msvc.h" // PATELLA_NO_SANITIZE_ADDRESS
#include <type_traits>

// Text alignment (TFont::Alignment, the Alignment parameters).
// An unnamed enum of int constants (TFont's int Alignment), unscoped as the other unnamed ones (notes/decisions.md,
// "The other ten stay unscoped"); clang-tidy's image checks it since it is a core header (issue #116).
enum : int { // NOLINT(cppcoreguidelines-use-enum-class): int constants, above
	LEFT_ALIGN = 0,
	CENTER_ALIGN = 1,
	RIGHT_ALIGN = 2,
};
static_assert(sizeof(decltype(LEFT_ALIGN)) == 4);
static_assert(std::is_same_v<std::underlying_type_t<decltype(LEFT_ALIGN)>, int>);

// --- Graphics types -----------------------------------------------------------
// The types of the drawing interface, used by every window file (GUI.h
// includes graphics.h, which exports this header). Default construction zeroes them, as the binary's
// compiler-generated constructors do (SkinProperties' constructor 0x496800,
// the new TSprite[] and new TPicture[] loops, DrawOutfit's colours).

// A rectangle: position and size. The members are named by the assertions
// "FromRect.width>0 && FromRect.height>0" (the surfaces' Blit and BlitRect)
// and "ToRect.width<=..." (StretchBlit), and by the TRect members of
// SkinProperties that GUI.cpp's assertions name with .x and .y
// (StatusWindow.HitpointsBar.x, BuddylistWindow.Logo.y); TSkin (Skins.h) and
// TDrawing (GUI.h) have the same lower-case names (issue #29).
struct TRect { // passed by value (16 bytes)
	constexpr TRect() : x(0), y(0), width(0), height(0) {}
	// The parameters are not named as the members (Patella-Modern issue #116): patella_core's GCC -Wshadow, since
	// Skins.h includes this header.
	constexpr TRect(int X, int Y, int Width, int Height) : x(X), y(Y), width(Width), height(Height) {}
	int x, y, width, height;
};

struct TRGBColor { // passed by value (12 bytes)
	constexpr TRGBColor() : R(0), G(0), B(0) {}
	constexpr TRGBColor(int Red, int Green, int Blue) : R(Red), G(Green), B(Blue) {}
	// 0x425950 (out-of-line copy between ConsoleWindow.cpp and
	// ContentWindows.cpp; inlined in Map.cpp's SetAmbientLight): a colour of
	// the 6x6x6 palette, Color = R*36 + G*6 + B with steps of 51.
	TRGBColor(int Color)
	{
		R = Color / 36 * 51;
		B = Color % 6 * 51;
		G = Color / 6 % 6 * 51;
	}
	// 0x48b380 and 0x48b3b0: out-of-line copies at the start of graphics.obj,
	// the first object that calls them (SurfaceGL.cpp calls 0x48b380 too).
	// operator!= negates the inlined operator==, as the binary's does.
	// Not checked by AddressSanitizer (issue #70): GetVideoPicture compares the colour key of the picture after
	// the one it draws, past the end of Pictures for the last one, as the original does (graphics.cpp).
	PATELLA_NO_SANITIZE_ADDRESS bool operator==(TRGBColor Other) const
	{
		return R == Other.R && G == Other.G && B == Other.B;
	}
	bool operator!=(TRGBColor Other) const
	{
		return !(*this == Other);
	}
	int R, G, B;
};

// The colour TFont's default constructor reads from memory. Every object
// that constructs a TFont has its own copy (GUI.obj 0x5c1954, DialogElements
// 0x5c1230, an earlier object 0x5c066c), so it is a constant defined in a
// header (internal linkage). The name is ours.
constexpr TRGBColor DefaultFontColor(224, 224, 224); // constexpr since issue #116 (constant-initialised as before)

// A text format: font, colour and alignment ("Format.Alignment" in
// PrintFormattedText's assertion).
struct TFont { // passed by value (20 bytes)
	TFont() : FontNumber(4), Color(DefaultFontColor), Alignment(LEFT_ALIGN) {}
	int FontNumber;
	TRGBColor Color;
	int Alignment; // LEFT_ALIGN, CENTER_ALIGN, RIGHT_ALIGN
};
