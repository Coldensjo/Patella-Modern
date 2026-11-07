// GUI.cpp: reconstructed from Tibia.exe 7.72 (0x495480-0x4a29df; the
// 0x4a29e0 deleting destructor is TWindow's).
//
// The window system's base classes (TComponent, TElement, TWindow), skin
// drawing, the timers, the hotkey table and the skin properties. Functions
// without line evidence come first, in binary order (the gap before the
// first asserted function: the compiler-generated constructors of
// SkinProperties and its blocks, Container.h code and the small functions
// without exception frame), then the asserted ones in line order. The
// TComponent functions that the binary has only as inline copies in earlier
// objects (GetNext, GetParent, ...) are in GUI.h.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none.

#include "GUI.h"
#include "Skins.h"
#include "Status.h" // IWYU pragma: keep (uses nothing from it, but GUI.obj has its LinkPattern copy, 0x5c1704)
#include "Text.h"   // IWYU pragma: keep (the object's own copy of the text table, see Text.h)
#include "sysinc.h"
#include "Help.h"
#include "common.h"
#include "graphics.h"
#include "log/Log.h"

#include <sstream>
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

// TComponent::DrawingList as a std::unique_ptr<TDrawing[]> (issue #58): a pointer at the original's offset. The
// subclasses' constructors allocate it with reset(new TDrawing[n]) (each list is the first: TComponent's constructor
// leaves it null, and no class allocates over a base's), ~TComponent frees it with reset() where the original had
// delete[]; TDrawing's destructor is trivial, so the reset cannot throw.
static_assert(sizeof(std::unique_ptr<TDrawing[]>) == sizeof(TDrawing *)
		&& alignof(std::unique_ptr<TDrawing[]>) == alignof(TDrawing *),
	"std::unique_ptr<TDrawing[]> is a pointer");

// The data of .bss (NumberOfHotkeys 0x5ccf98, Timers 0x5ccfa4, Hotkeys 0x5ce4f0, UpdateTimers' static LastTime 0x5ce518)
// is TGUI's (GUI.h, issue #107); SkinProperties (0x5ccfc8, initialiser 0x559e50, constructor 0x496800) TGraphics'
// TSkins' (Skins.h, issue #116), which InitSkinProperties below fills.

// The skins' arrays std::arrays (issue #60): the original's layouts (no pointers: the same on x64).
static_assert(sizeof(TSkinProperties288) == 0x158 && offsetof(TSkinProperties288, Data10) == 0x10
		&& offsetof(TSkinProperties288, Data60) == 0x60 && offsetof(TSkinProperties288, Data100) == 0x100,
	"TSkinProperties288's layout");
static_assert(sizeof(TSkinPropertiesB08) == 0x8c && offsetof(TSkinPropertiesB08, Data2C) == 0x2c, "TSkinPropertiesB08's layout");
static_assert(sizeof(TSkinPropertiesF8C) == 0xc0 && offsetof(TSkinPropertiesF8C, Data38) == 0x38, "TSkinPropertiesF8C's layout");
static_assert(sizeof(TCombatControlsProperties) == 0x50 && offsetof(TCombatControlsProperties, Buttons) == 0x08,
	"TCombatControlsProperties' layout");
static_assert(sizeof(TSkinProperties) == 0x1528 && offsetof(TSkinProperties, DataC88) == 0xc88
		&& offsetof(TSkinProperties, Data1204) == 0x1204,
	"TSkinProperties' layout");

// --- Functions without exception frame (no line evidence) ------------------

// 0x496000
int TTimer::GetTime() const
{
	return Remaining / 100;
}

// 0x496020
bool TTimer::IsExpired() const
{
	return !(Active && Remaining != 0);
}

// 0x496040, inlined into the deleting destructor 0x496520. noexcept since issue
// #108: the work of the window classes' destructors that could throw (TTab's
// assertion, the Send functions, the preferences, DeleteTextBuffer) is their
// Close (OnClose), which every delete of a component runs first; it was
// noexcept(false) from issue #34 to #108 (VC7.1's destructors let exceptions
// out; notes/ownership.md, section 7).
TComponent::~TComponent()
{
	if (DrawingList != nullptr)
		DrawingList.reset();
}

static_assert(std::is_nothrow_destructible_v<TComponent> && std::is_nothrow_destructible_v<TWindow>,
	"issue #108: the window classes' destructors are noexcept");
// offsetof of a class with virtual functions: conditionally supported, and supported by every compiler here (GCC's
// and Clang's -Winvalid-offsetof, issue #111).
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(sizeof(void *) != 8 || (sizeof(TComponent) == 56 && offsetof(TComponent, Parent) == 24),
	"issue #108: Closed is in the padding after DrawingListLength, TComponent's x64 layout unchanged");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Issue #108 (not in the original). Closed is set first: a close that throws is
// not run again, as the original's destructor was not.
void TComponent::Close()
{
	if (Closed)
		return;
	Closed = true;
	OnClose();
}

// Issue #108.
void TComponent::CheckClosed(const char *Class) const noexcept
{
	if (!Closed)
		patella::log::Error("{} destroyed without Close: its close was not done (issue #108)", Class);
}

// Issue #108: the original's `delete Component`.
void DeleteComponent(TComponent *Component)
{
	if (Component == nullptr)
		return;
	Component->Close();
	delete Component;
}

// 0x496060
void TComponent::SetPosition(int _OffsetX, int _OffsetY)
{
	OffsetX = _OffsetX;
	OffsetY = _OffsetY;
}

// 0x496080
TWindow::TWindow()
{
	FirstChild = nullptr;
	Capture = nullptr;
}

// 0x4960b0
bool TWindow::IsChild(TComponent *Child)
{
	for (const TComponent *Search = FirstChild; Search != nullptr; Search = Search->Next) {
		if (Search == Child)
			return true;
	}
	return false;
}

// 0x4960e0
TComponent *TWindow::GetLastChild()
{
	if (FirstChild == nullptr)
		return nullptr;
	TComponent *Search = FirstChild;
	while (Search->Next != nullptr)
		Search = Search->Next;
	return Search;
}

// 0x496100
TElement::TElement() = default;

// 0x496130. Identical code to any two-member constructor, so the linker may
// have folded others into it.
TCallback::TCallback(TComponent *Receiver, int Event)
{
	this->Receiver = Receiver;
	this->Event = Event;
}

// 0x496150. Called last by InitControl.
void patella::TGUI::InitHotkeys()
{
	Hotkeys.clear();
}

// 0x496160, the one copy the linker kept of many identical empty functions
// (folded). ExitControl calls this one first; TComponent::Arrange and
// PositionChanged and TElement::StopDragging have the same body.
void patella::TGUI::ExitHotkeys()
{
}

// --- Skins ----------------------------------------------------------------

// 0x497a00, lines 42-86. Tiles the skin over width x height; srcx and srcy
// cut that much off the first column and row. The assertions come before the
// try block (it starts at state 35).
void DrawSkin(int Surface, int x, int y, int width, int height, int skin, int srcx, int srcy)
{
	if (skin == 0 || width == 0 || height == 0)
		return;
	PATELLA_ASSERT_VALUES(x>=0, x);
	PATELLA_ASSERT_VALUES(y>=0, y);
	PATELLA_ASSERT_VALUES(In(skin,0,LASTSKIN-1), skin);
	PATELLA_ASSERT_VALUES(Skins()[skin].width>0, Skins()[skin].width);
	PATELLA_ASSERT_VALUES(Skins()[skin].height>0, Skins()[skin].height);
	PATELLA_ASSERT_VALUES(width>0, width);
	PATELLA_ASSERT_VALUES(height>0, height);
	if (Skins()[skin].width == width && Skins()[skin].height == height) {
		DrawPicturePart(Surface, x, y, 4, Skins()[skin].x, Skins()[skin].y, Skins()[skin].width, Skins()[skin].height);
	} else {
		int PieceHeight = 0;
		int dy = 0;
		for (int h = height; h > 0; h -= PieceHeight) {
			int dx = 0;
			int PieceWidth;
			for (int w = width; w > 0; w -= PieceWidth) {
				int SourceX, SourceY;
				if (w == width && srcx > 0) {
					PieceWidth = Skins()[skin].width - srcx;
					SourceX = Skins()[skin].x + srcx;
				} else {
					SourceX = Skins()[skin].x;
					PieceWidth = Skins()[skin].width;
				}
				if (w < PieceWidth)
					PieceWidth = w;
				if (h == height && srcy > 0) {
					SourceY = Skins()[skin].y + srcy;
					PieceHeight = Skins()[skin].height - srcy;
				} else {
					SourceY = Skins()[skin].y;
					PieceHeight = Skins()[skin].height;
				}
				if (PieceHeight > h)
					PieceHeight = h;
				DrawPicturePart(Surface, dx + x, dy + y, 4, SourceX, SourceY, PieceWidth, PieceHeight);
				dx += PieceWidth;
			}
			dy += PieceHeight;
		}
	}
}

// 0x4a1690, lines 99-160. DrawSkin with a clipping rectangle, which is
// passed on to the picture function. The assertions come before the try
// block (it starts at state 15).
void DrawSkinClipped(int Surface, int x, int y, int width, int height, int skin, int ClipX, int ClipY,
	int ClipWidth, int ClipHeight, int skinoffsetx, int skinoffsety)
{
	if (skin == 0 || width == 0 || height == 0)
		return;
	PATELLA_ASSERT_VALUES(In(skin,0,LASTSKIN-1), skin);
	PATELLA_ASSERT_VALUES(width>0, width);
	PATELLA_ASSERT_VALUES(height>0, height);
	if (Skins()[skin].width < 1 || Skins()[skin].height < 1) {
		std::stringstream s;
		s << "some assertions failed " << "(surface: " << Surface << " x: " << x << " y: " << y
		  << " width: " << width << " height: " << height << " skin: " << skin
		  << " skinwidth: " << Skins()[skin].width << " skinheight: " << Skins()[skin].height
		  << " skinoffsetx: " << skinoffsetx << " skinoffsety: " << skinoffsety
		  << " cliprect: " << ClipX << "/" << ClipY << "/" << ClipWidth << "/" << ClipHeight << ")";
		TERROR(s.str().c_str());
	}
	if (Skins()[skin].width == width && Skins()[skin].height == height) {
		DrawPicturePartClipped(Surface, x, y, 4, Skins()[skin].x, Skins()[skin].y, Skins()[skin].width,
			Skins()[skin].height, TRect(ClipX, ClipY, ClipWidth, ClipHeight));
	} else {
		int PieceHeight = 0;
		int dy = 0;
		for (int h = height; h > 0; h -= PieceHeight) {
			int dx = 0;
			int PieceWidth;
			for (int w = width; w > 0; w -= PieceWidth) {
				int SourceX, SourceY;
				if (w == width && skinoffsetx > 0) {
					PieceWidth = Skins()[skin].width - skinoffsetx;
					SourceX = Skins()[skin].x + skinoffsetx;
				} else {
					PieceWidth = Skins()[skin].width;
					SourceX = Skins()[skin].x;
				}
				if (w < PieceWidth)
					PieceWidth = w;
				if (h == height && skinoffsety > 0) {
					SourceY = Skins()[skin].y + skinoffsety;
					PieceHeight = Skins()[skin].height - skinoffsety;
				} else {
					SourceY = Skins()[skin].y;
					PieceHeight = Skins()[skin].height;
				}
				if (PieceHeight > h)
					PieceHeight = h;
				DrawPicturePartClipped(Surface, dx + x, dy + y, 4, SourceX, SourceY, PieceWidth, PieceHeight,
					TRect(ClipX, ClipY, ClipWidth, ClipHeight));
				dx += PieceWidth;
			}
			dy += PieceHeight;
		}
	}
}

// --- Skin properties ------------------------------------------------------

// 0x498160. Called by InitControl after InitSkins. The exception tables have
// the states of a catch wrapper but no try block (the compiler removed it: the
// body cannot throw); the catch line is unknown. The binary stores the
// values in another order; they are written here by offset.
void InitSkinProperties()
{
	SkinProperties().CursorHotspot1.x = 0;
	SkinProperties().CursorHotspot1.y = 0;
	SkinProperties().CursorHotspot2.x = 4;
	SkinProperties().CursorHotspot2.y = 9;
	SkinProperties().CursorHotspot3.x = 6;
	SkinProperties().CursorHotspot3.y = 10;
	SkinProperties().CursorHotspot4.x = 9;
	SkinProperties().CursorHotspot4.y = 9;
	SkinProperties().CursorHotspot5.x = 9;
	SkinProperties().CursorHotspot5.y = 9;
	SkinProperties().CursorHotspot6.x = 6;
	SkinProperties().CursorHotspot6.y = 6;
	SkinProperties().TooltipFont.FontNumber = 1;
	SkinProperties().TooltipFont.Color.R = 64;
	SkinProperties().TooltipFont.Color.G = 64;
	SkinProperties().TooltipFont.Color.B = 64;
	SkinProperties().TooltipFont.Alignment = 0;
	SkinProperties().TooltipBorder = 4;
	SkinProperties().TooltipBackground.R = 192;
	SkinProperties().TooltipBackground.G = 192;
	SkinProperties().TooltipBackground.B = 192;
	SkinProperties().TooltipFrame.R = 0;
	SkinProperties().TooltipFrame.G = 0;
	SkinProperties().TooltipFrame.B = 0;
	SkinProperties().HighlightColor.R = 248;
	SkinProperties().HighlightColor.G = 248;
	SkinProperties().HighlightColor.B = 248;
	SkinProperties().AttackColor.R = 224;
	SkinProperties().AttackColor.G = 64;
	SkinProperties().AttackColor.B = 64;
	SkinProperties().FollowColor.R = 64;
	SkinProperties().FollowColor.G = 224;
	SkinProperties().FollowColor.B = 64;
	SkinProperties().HighlightedAttackColor.R = 248;
	SkinProperties().HighlightedAttackColor.G = 164;
	SkinProperties().HighlightedAttackColor.B = 164;
	SkinProperties().HighlightedFollowColor.R = 180;
	SkinProperties().HighlightedFollowColor.G = 248;
	SkinProperties().HighlightedFollowColor.B = 180;
	SkinProperties().Data09C = 40;
	SkinProperties().Data0A0.x = 4;
	SkinProperties().Data0A0.y = 4;
	SkinProperties().Data0A0.width = -4;
	SkinProperties().Data0A0.height = -4;
	SkinProperties().Data0B0.FontNumber = 2;
	SkinProperties().Data0B0.Color.R = 224;
	SkinProperties().Data0B0.Color.G = 224;
	SkinProperties().Data0B0.Color.B = 224;
	SkinProperties().Data0B0.Alignment = 0;
	SkinProperties().Data0C4 = 8;
	SkinProperties().Data0C8.R = 128;
	SkinProperties().Data0C8.G = 128;
	SkinProperties().Data0C8.B = 128;
	SkinProperties().Data0D4 = 2;
	SkinProperties().Style0D8.Data00 = 86;
	SkinProperties().Style0D8.Data04 = 140;
	SkinProperties().Style0D8.Data08.x = 60;
	SkinProperties().Style0D8.Data08.y = -240;
	SkinProperties().Style0D8.Data10.x = 0;
	SkinProperties().Style0D8.Data10.y = 0;
	SkinProperties().Style0D8.Data18.x = 0;
	SkinProperties().Style0D8.Data18.y = 60;
	SkinProperties().Style0D8.Data20.x = 0;
	SkinProperties().Style0D8.Data20.y = 90;
	SkinProperties().Style0D8.Data28.x = 0;
	SkinProperties().Style0D8.Data28.y = 120;
	SkinProperties().Style0D8.Data30.x = 0;
	SkinProperties().Style0D8.Data30.y = 30;
	SkinProperties().Data134 = 10;
	SkinProperties().Data138 = 5;
	SkinProperties().Data13C = 5;
	SkinProperties().Data140.x = -35;
	SkinProperties().Data140.y = 5;
	SkinProperties().Data140.width = 32;
	SkinProperties().Data140.height = 32;
	SkinProperties().Data150 = 43;
	SkinProperties().Data154 = 43;
	SkinProperties().Data158 = 322;
	SkinProperties().Data15C = 447;
	SkinProperties().Data160.x = 4;
	SkinProperties().Data160.y = 4;
	SkinProperties().Data160.width = -4;
	SkinProperties().Data160.height = -4;
	SkinProperties().Data170 = 4;
	SkinProperties().Data174 = 3;
	SkinProperties().Data178 = 3;
	SkinProperties().Data17C = 400;
	SkinProperties().Data180 = 340;
	SkinProperties().Data184 = 176;
	SkinProperties().Data188.R = 0;
	SkinProperties().Data188.G = 0;
	SkinProperties().Data188.B = 0;
	SkinProperties().Style194.Data00.x = 4;
	SkinProperties().Style194.Data00.y = 2;
	SkinProperties().Style194.Data00.width = 12;
	SkinProperties().Style194.Data00.height = 12;
	SkinProperties().Style194.Data10.x = -15;
	SkinProperties().Style194.Data10.y = 2;
	SkinProperties().Style194.Data18.x = -28;
	SkinProperties().Style194.Data18.y = 2;
	SkinProperties().Style194.Data20.x = -39;
	SkinProperties().Style194.Data20.y = 2;
	SkinProperties().Style194.Data28.x = 20;
	SkinProperties().Style194.Data28.y = 4;
	SkinProperties().Style194.Data28.width = -41;
	SkinProperties().Style194.Data28.height = 10;
	SkinProperties().Style194.Data38.FontNumber = 4;
	SkinProperties().Style194.Data38.Color.R = 144;
	SkinProperties().Style194.Data38.Color.G = 144;
	SkinProperties().Style194.Data38.Color.B = 144;
	SkinProperties().Style194.Data38.Alignment = 0;
	SkinProperties().Style194.Data4C = 19;
	SkinProperties().Style194.Data50.x = 4;
	SkinProperties().Style194.Data50.y = 15;
	SkinProperties().Style194.Data50.width = -4;
	SkinProperties().Style194.Data50.height = -4;
	SkinProperties().Style194.Data60.x = 3;
	SkinProperties().Style194.Data60.y = -16;
	SkinProperties().Style194.Data60.width = 13;
	SkinProperties().Style194.Data60.height = 13;
	SkinProperties().Style194.Data70.x = 0;
	SkinProperties().Style194.Data70.y = -4;
	SkinProperties().Style194.Data70.width = 0;
	SkinProperties().Style194.Data70.height = 0;
	SkinProperties().Style194.Data80.R = 56;
	SkinProperties().Style194.Data80.G = 56;
	SkinProperties().Style194.Data80.B = 56;
	SkinProperties().Data220 = 117;
	SkinProperties().Data224.x = 8;
	SkinProperties().Data224.y = 4;
	SkinProperties().Data224.width = 108;
	SkinProperties().Data224.height = 108;
	SkinProperties().Data234.x = 9;
	SkinProperties().Data234.y = 5;
	SkinProperties().Data234.width = 106;
	SkinProperties().Data234.height = 106;
	SkinProperties().Data244.x = 124;
	SkinProperties().Data244.y = 71;
	SkinProperties().Data244.width = 124;
	SkinProperties().Data244.height = 50;
	SkinProperties().Data254.x = 147;
	SkinProperties().Data254.y = 50;
	SkinProperties().Data254.width = 147;
	SkinProperties().Data254.height = 71;
	SkinProperties().Data264.x = 124;
	SkinProperties().Data264.y = 92;
	SkinProperties().Data264.width = 124;
	SkinProperties().Data264.height = 5;
	SkinProperties().Data274 = 170;
	SkinProperties().Data278.x = 124;
	SkinProperties().Data278.y = 19;
	SkinProperties().Data278.width = 8;
	SkinProperties().Data278.height = 4;
	SkinProperties().Style288.Data00 = 108;
	SkinProperties().Style288.Data04 = 163;
	SkinProperties().Style288.Data08.x = 0;
	SkinProperties().Style288.Data08.y = 0;
	SkinProperties().Style288.Data10[0].x = 38;
	SkinProperties().Style288.Data10[0].y = 1;
	SkinProperties().Style288.Data10[1].x = 1;
	SkinProperties().Style288.Data10[1].y = 15;
	SkinProperties().Style288.Data10[2].x = 75;
	SkinProperties().Style288.Data10[2].y = 15;
	SkinProperties().Style288.Data10[3].x = 38;
	SkinProperties().Style288.Data10[3].y = 38;
	SkinProperties().Style288.Data10[4].x = 75;
	SkinProperties().Style288.Data10[4].y = 52;
	SkinProperties().Style288.Data10[5].x = 1;
	SkinProperties().Style288.Data10[5].y = 52;
	SkinProperties().Style288.Data10[6].x = 38;
	SkinProperties().Style288.Data10[6].y = 75;
	SkinProperties().Style288.Data10[7].x = 38;
	SkinProperties().Style288.Data10[7].y = 112;
	SkinProperties().Style288.Data10[8].x = 1;
	SkinProperties().Style288.Data10[8].y = 89;
	SkinProperties().Style288.Data10[9].x = 75;
	SkinProperties().Style288.Data10[9].y = 89;
	SkinProperties().Style288.Data60[0].x = 37;
	SkinProperties().Style288.Data60[0].y = 0;
	SkinProperties().Style288.Data60[0].width = 34;
	SkinProperties().Style288.Data60[0].height = 34;
	SkinProperties().Style288.Data60[1].x = 0;
	SkinProperties().Style288.Data60[1].y = 14;
	SkinProperties().Style288.Data60[1].width = 34;
	SkinProperties().Style288.Data60[1].height = 34;
	SkinProperties().Style288.Data60[2].x = 74;
	SkinProperties().Style288.Data60[2].y = 14;
	SkinProperties().Style288.Data60[2].width = 34;
	SkinProperties().Style288.Data60[2].height = 34;
	SkinProperties().Style288.Data60[3].x = 37;
	SkinProperties().Style288.Data60[3].y = 37;
	SkinProperties().Style288.Data60[3].width = 34;
	SkinProperties().Style288.Data60[3].height = 34;
	SkinProperties().Style288.Data60[4].x = 74;
	SkinProperties().Style288.Data60[4].y = 51;
	SkinProperties().Style288.Data60[4].width = 34;
	SkinProperties().Style288.Data60[4].height = 34;
	SkinProperties().Style288.Data60[5].x = 0;
	SkinProperties().Style288.Data60[5].y = 51;
	SkinProperties().Style288.Data60[5].width = 34;
	SkinProperties().Style288.Data60[5].height = 34;
	SkinProperties().Style288.Data60[6].x = 37;
	SkinProperties().Style288.Data60[6].y = 74;
	SkinProperties().Style288.Data60[6].width = 34;
	SkinProperties().Style288.Data60[6].height = 34;
	SkinProperties().Style288.Data60[7].x = 37;
	SkinProperties().Style288.Data60[7].y = 111;
	SkinProperties().Style288.Data60[7].width = 34;
	SkinProperties().Style288.Data60[7].height = 34;
	SkinProperties().Style288.Data60[8].x = 0;
	SkinProperties().Style288.Data60[8].y = 88;
	SkinProperties().Style288.Data60[8].width = 34;
	SkinProperties().Style288.Data60[8].height = 34;
	SkinProperties().Style288.Data60[9].x = 74;
	SkinProperties().Style288.Data60[9].y = 88;
	SkinProperties().Style288.Data60[9].width = 34;
	SkinProperties().Style288.Data60[9].height = 34;
	SkinProperties().Style288.Data100[0] = 88;
	SkinProperties().Style288.Data100[1] = 89;
	SkinProperties().Style288.Data100[2] = 90;
	SkinProperties().Style288.Data100[3] = 91;
	SkinProperties().Style288.Data100[4] = 93;
	SkinProperties().Style288.Data100[5] = 92;
	SkinProperties().Style288.Data100[6] = 94;
	SkinProperties().Style288.Data100[7] = 95;
	SkinProperties().Style288.Data100[8] = 96;
	SkinProperties().Style288.Data100[9] = 97;
	SkinProperties().Style288.Data128.x = 74;
	SkinProperties().Style288.Data128.y = 124;
	SkinProperties().Style288.Data128.width = 34;
	SkinProperties().Style288.Data128.height = 21;
	SkinProperties().Style288.Data138.x = 0;
	SkinProperties().Style288.Data138.y = 124;
	SkinProperties().Style288.Data138.width = 34;
	SkinProperties().Style288.Data138.height = 21;
	SkinProperties().Style288.Data148.x = 0;
	SkinProperties().Style288.Data148.y = 147;
	SkinProperties().Style288.Data148.width = 108;
	SkinProperties().Style288.Data148.height = 13;
	SkinProperties().Style3E0.Data00.x = 17;
	SkinProperties().Style3E0.Data00.y = 2;
	SkinProperties().Style3E0.Data00.width = 32;
	SkinProperties().Style3E0.Data00.height = 9;
	SkinProperties().Style3E0.Data10.FontNumber = 3;
	SkinProperties().Style3E0.Data10.Color.R = 255;
	SkinProperties().Style3E0.Data10.Color.G = 255;
	SkinProperties().Style3E0.Data10.Color.B = 255;
	SkinProperties().Style3E0.Data10.Alignment = 1;
	SkinProperties().Style3E0.Data24.x = 17;
	SkinProperties().Style3E0.Data24.y = 11;
	SkinProperties().Style3E0.Data24.width = 32;
	SkinProperties().Style3E0.Data24.height = 9;
	SkinProperties().Style3E0.Data34.FontNumber = 4;
	SkinProperties().Style3E0.Data34.Color.R = 176;
	SkinProperties().Style3E0.Data34.Color.G = 176;
	SkinProperties().Style3E0.Data34.Color.B = 176;
	SkinProperties().Style3E0.Data34.Alignment = 1;
	SkinProperties().Style428.Data00.x = 17;
	SkinProperties().Style428.Data00.y = 2;
	SkinProperties().Style428.Data00.width = 32;
	SkinProperties().Style428.Data00.height = 9;
	SkinProperties().Style428.Data10.FontNumber = 3;
	SkinProperties().Style428.Data10.Color.R = 255;
	SkinProperties().Style428.Data10.Color.G = 255;
	SkinProperties().Style428.Data10.Color.B = 255;
	SkinProperties().Style428.Data10.Alignment = 1;
	SkinProperties().Style428.Data24.x = 17;
	SkinProperties().Style428.Data24.y = 11;
	SkinProperties().Style428.Data24.width = 32;
	SkinProperties().Style428.Data24.height = 9;
	SkinProperties().Style428.Data34.FontNumber = 4;
	SkinProperties().Style428.Data34.Color.R = 176;
	SkinProperties().Style428.Data34.Color.G = 176;
	SkinProperties().Style428.Data34.Color.B = 176;
	SkinProperties().Style428.Data34.Alignment = 1;
	SkinProperties().PlayerStates.Data00.x = 2;
	SkinProperties().PlayerStates.Data00.y = 2;
	SkinProperties().PlayerStates.Data00.width = 104;
	SkinProperties().PlayerStates.Data00.height = 9;
	SkinProperties().PlayerStates.Data10.x = 2;
	SkinProperties().PlayerStates.Data10.y = 2;
	SkinProperties().PlayerStates.Data10.width = 90;
	SkinProperties().PlayerStates.Data10.height = 9;
	SkinProperties().PlayerStates.NumberOfIcons = 9;
	SkinProperties().PlayerStates.Data24 = 10;
	SkinProperties().Data498.FontNumber = 2;
	SkinProperties().Data498.Color.R = 192;
	SkinProperties().Data498.Color.G = 192;
	SkinProperties().Data498.Color.B = 192;
	SkinProperties().Data498.Alignment = 2;
	SkinProperties().Style4AC.Height = 134;
	SkinProperties().Style4AC.Width = 43;
	SkinProperties().Style4AC.Buttons[0].x = 0;
	SkinProperties().Style4AC.Buttons[0].y = 0;
	SkinProperties().Style4AC.Buttons[1].x = 0;
	SkinProperties().Style4AC.Buttons[1].y = 20;
	SkinProperties().Style4AC.Buttons[2].x = 0;
	SkinProperties().Style4AC.Buttons[2].y = 40;
	SkinProperties().Style4AC.Buttons[3].x = 23;
	SkinProperties().Style4AC.Buttons[3].y = 0;
	SkinProperties().Style4AC.Buttons[4].x = 23;
	SkinProperties().Style4AC.Buttons[4].y = 20;
	SkinProperties().Style4AC.Buttons[5].x = 23;
	SkinProperties().Style4AC.Buttons[5].y = 40;
	SkinProperties().Style4AC.Buttons[6].x = 0;
	SkinProperties().Style4AC.Buttons[6].y = 62;
	SkinProperties().Style4AC.Buttons[7].x = 0;
	SkinProperties().Style4AC.Buttons[7].y = 86;
	SkinProperties().Style4AC.Buttons[8].x = 0;
	SkinProperties().Style4AC.Buttons[8].y = 108;
	SkinProperties().Style4FC.Height = 42;
	SkinProperties().Style4FC.Width = 111;
	SkinProperties().Style4FC.Buttons[0].x = 0;
	SkinProperties().Style4FC.Buttons[0].y = 0;
	SkinProperties().Style4FC.Buttons[1].x = 20;
	SkinProperties().Style4FC.Buttons[1].y = 0;
	SkinProperties().Style4FC.Buttons[2].x = 40;
	SkinProperties().Style4FC.Buttons[2].y = 0;
	SkinProperties().Style4FC.Buttons[3].x = 0;
	SkinProperties().Style4FC.Buttons[3].y = 22;
	SkinProperties().Style4FC.Buttons[4].x = 20;
	SkinProperties().Style4FC.Buttons[4].y = 22;
	SkinProperties().Style4FC.Buttons[5].x = 40;
	SkinProperties().Style4FC.Buttons[5].y = 22;
	SkinProperties().Style4FC.Buttons[6].x = 68;
	SkinProperties().Style4FC.Buttons[6].y = 0;
	SkinProperties().Style4FC.Buttons[7].x = 68;
	SkinProperties().Style4FC.Buttons[7].y = 22;
	SkinProperties().Style4FC.Buttons[8].x = 90;
	SkinProperties().Style4FC.Buttons[8].y = 22;
	SkinProperties().Data54C = 64;
	SkinProperties().Data550.x = 56;
	SkinProperties().Data550.y = 4;
	SkinProperties().Data550.width = 8;
	SkinProperties().Data550.height = 4;
	SkinProperties().Data560.x = 22;
	SkinProperties().Data560.y = 4;
	SkinProperties().Data560.width = 32;
	SkinProperties().Data560.height = 21;
	SkinProperties().Data570.x = 22;
	SkinProperties().Data570.y = 25;
	SkinProperties().Data570.width = 32;
	SkinProperties().Data570.height = 21;
	SkinProperties().Data580.x = 22;
	SkinProperties().Data580.y = 48;
	SkinProperties().Data580.width = 94;
	SkinProperties().Data580.height = 13;
	SkinProperties().Data590.x = 22;
	SkinProperties().Data590.y = 4;
	SkinProperties().Data590.width = 32;
	SkinProperties().Data590.height = 42;
	SkinProperties().StatusWindow.Data00 = 32;
	SkinProperties().StatusWindow.Data04.x = 9;
	SkinProperties().StatusWindow.Data04.y = 5;
	SkinProperties().StatusWindow.Data04.width = 9;
	SkinProperties().StatusWindow.Data04.height = 18;
	SkinProperties().StatusWindow.HitpointsBar.x = 26;
	SkinProperties().StatusWindow.HitpointsBar.y = 4;
	SkinProperties().StatusWindow.HitpointsBar.width = 90;
	SkinProperties().StatusWindow.HitpointsBar.height = 11;
	SkinProperties().StatusWindow.ManaBar.x = 26;
	SkinProperties().StatusWindow.ManaBar.y = 17;
	SkinProperties().StatusWindow.ManaBar.width = 90;
	SkinProperties().StatusWindow.ManaBar.height = 11;
	SkinProperties().StatusWindow.Data34.x = 127;
	SkinProperties().StatusWindow.Data34.y = 5;
	SkinProperties().StatusWindow.Data34.width = 39;
	SkinProperties().StatusWindow.Data34.height = 9;
	SkinProperties().StatusWindow.Data44.x = 127;
	SkinProperties().StatusWindow.Data44.y = 18;
	SkinProperties().StatusWindow.Data44.width = 39;
	SkinProperties().StatusWindow.Data44.height = 9;
	SkinProperties().StatusWindow.Data54.FontNumber = 4;
	SkinProperties().StatusWindow.Data54.Color.R = 176;
	SkinProperties().StatusWindow.Data54.Color.G = 176;
	SkinProperties().StatusWindow.Data54.Color.B = 176;
	SkinProperties().StatusWindow.Data54.Alignment = 0;
	SkinProperties().Data608 = 26;
	SkinProperties().Data60C.x = 8;
	SkinProperties().Data60C.y = 3;
	SkinProperties().Data60C.width = 45;
	SkinProperties().Data60C.height = 3;
	SkinProperties().Data61C.x = 82;
	SkinProperties().Data61C.y = 3;
	SkinProperties().Data61C.width = 124;
	SkinProperties().Data61C.height = 3;
	SkinProperties().Data62C = 4;
	SkinProperties().Data630.x = 7;
	SkinProperties().Data630.y = 4;
	SkinProperties().Data638.x = 34;
	SkinProperties().Data638.y = 34;
	SkinProperties().Data638.width = 8;
	SkinProperties().Data638.height = 5;
	SkinProperties().Data648 = 37;
	SkinProperties().Data64C = 37;
	SkinProperties().Data650 = 41;
	SkinProperties().Data654.FontNumber = 4;
	SkinProperties().Data654.Color.R = 176;
	SkinProperties().Data654.Color.G = 176;
	SkinProperties().Data654.Color.B = 176;
	SkinProperties().Data654.Alignment = 0;
	SkinProperties().Data668 = 10;
	SkinProperties().Data66C = 10;
	SkinProperties().Data670.R = 192;
	SkinProperties().Data670.G = 0;
	SkinProperties().Data670.B = 0;
	SkinProperties().Data67C.R = 0;
	SkinProperties().Data67C.G = 192;
	SkinProperties().Data67C.B = 0;
	SkinProperties().Style688.Data00 = 22;
	SkinProperties().Style688.Data04.x = 2;
	SkinProperties().Style688.Data04.y = 2;
	SkinProperties().Style688.Data04.width = 18;
	SkinProperties().Style688.Data04.height = 18;
	SkinProperties().Style688.Data14.x = 23;
	SkinProperties().Style688.Data14.y = 2;
	SkinProperties().Style688.Data14.width = 110;
	SkinProperties().Style688.Data14.height = 11;
	SkinProperties().Style688.Data24 = 13;
	SkinProperties().Style688.Data28.x = 23;
	SkinProperties().Style688.Data28.y = 14;
	SkinProperties().Style688.Data28.width = 122;
	SkinProperties().Style688.Data28.height = 5;
	SkinProperties().Style688.Data38.x = 1;
	SkinProperties().Style688.Data38.y = 1;
	SkinProperties().Style688.Data38.width = 20;
	SkinProperties().Style688.Data38.height = 20;
	SkinProperties().Style688.Data48.R = 0;
	SkinProperties().Style688.Data48.G = 0;
	SkinProperties().Style688.Data48.B = 0;
	SkinProperties().Style688.Data54.FontNumber = 4;
	SkinProperties().Style688.Data54.Color.R = 176;
	SkinProperties().Style688.Data54.Color.G = 176;
	SkinProperties().Style688.Data54.Color.B = 176;
	SkinProperties().Style688.Data54.Alignment = 0;
	SkinProperties().Style688.Data68.R = 71;
	SkinProperties().Style688.Data68.G = 71;
	SkinProperties().Style688.Data68.B = 71;
	SkinProperties().Style6FC.Data00 = 78;
	SkinProperties().Style6FC.Data04.x = 0;
	SkinProperties().Style6FC.Data04.y = 0;
	SkinProperties().Style6FC.Data04.width = 0;
	SkinProperties().Style6FC.Data04.height = 12;
	SkinProperties().Style6FC.Data14.x = 3;
	SkinProperties().Style6FC.Data14.y = 2;
	SkinProperties().Style6FC.Data14.width = 71;
	SkinProperties().Style6FC.Data14.height = 9;
	SkinProperties().Style6FC.Data24.x = 82;
	SkinProperties().Style6FC.Data24.y = 2;
	SkinProperties().Style6FC.Data24.width = 71;
	SkinProperties().Style6FC.Data24.height = 9;
	SkinProperties().Style6FC.Data34.x = 0;
	SkinProperties().Style6FC.Data34.y = -2;
	SkinProperties().Style6FC.Data34.width = 0;
	SkinProperties().Style6FC.Data34.height = 2;
	SkinProperties().Style6FC.Data44.FontNumber = 3;
	SkinProperties().Style6FC.Data44.Color.R = 255;
	SkinProperties().Style6FC.Data44.Color.G = 255;
	SkinProperties().Style6FC.Data44.Color.B = 255;
	SkinProperties().Style6FC.Data44.Alignment = 0;
	SkinProperties().Style6FC.Data58.x = 0;
	SkinProperties().Style6FC.Data58.y = 12;
	SkinProperties().Style6FC.Data58.width = 0;
	SkinProperties().Style6FC.Data58.height = -26;
	SkinProperties().Style6FC.Data68.x = 3;
	SkinProperties().Style6FC.Data68.y = 3;
	SkinProperties().Style6FC.Data68.width = 34;
	SkinProperties().Style6FC.Data68.height = 34;
	SkinProperties().Style6FC.Data78.x = 82;
	SkinProperties().Style6FC.Data78.y = 3;
	SkinProperties().Style6FC.Data78.width = 34;
	SkinProperties().Style6FC.Data78.height = 34;
	SkinProperties().Style6FC.Data88.x = 4;
	SkinProperties().Style6FC.Data88.y = 4;
	SkinProperties().Style6FC.Data88.width = 83;
	SkinProperties().Style6FC.Data88.height = 4;
	SkinProperties().Style6FC.Data98 = 32;
	SkinProperties().Style6FC.Data9C = 36;
	SkinProperties().Style6FC.DataA0 = 36;
	SkinProperties().Style6FC.DataA4 = 2;
	SkinProperties().Style6FC.DataA8.x = 77;
	SkinProperties().Style6FC.DataA8.y = 0;
	SkinProperties().Style6FC.DataA8.width = 2;
	SkinProperties().Style6FC.DataA8.height = 0;
	SkinProperties().Style6FC.DataB8.x = 0;
	SkinProperties().Style6FC.DataB8.y = -26;
	SkinProperties().Style6FC.DataB8.width = 0;
	SkinProperties().Style6FC.DataB8.height = 0;
	SkinProperties().Style6FC.DataC8.x = 74;
	SkinProperties().Style6FC.DataC8.y = 4;
	SkinProperties().Style6FC.DataC8.width = 121;
	SkinProperties().Style6FC.DataC8.height = 4;
	SkinProperties().Style6FC.DataD8.x = 4;
	SkinProperties().Style6FC.DataD8.y = 4;
	SkinProperties().Style6FC.DataD8.width = 113;
	SkinProperties().Style6FC.DataD8.height = 20;
	SkinProperties().Style6FC.DataE8.x = 0;
	SkinProperties().Style6FC.DataE8.y = 0;
	SkinProperties().Style6FC.DataE8.width = 0;
	SkinProperties().Style6FC.DataE8.height = 2;
	SkinProperties().Style6FC.DataF8.FontNumber = 3;
	SkinProperties().Style6FC.DataF8.Color.R = 255;
	SkinProperties().Style6FC.DataF8.Color.G = 255;
	SkinProperties().Style6FC.DataF8.Color.B = 255;
	SkinProperties().Style6FC.DataF8.Alignment = 1;
	SkinProperties().BuddylistWindow.Data00 = 32;
	SkinProperties().BuddylistWindow.Data04 = 16;
	SkinProperties().BuddylistWindow.Logo.x = 2;
	SkinProperties().BuddylistWindow.Logo.y = 2;
	SkinProperties().BuddylistWindow.Logo.width = 12;
	SkinProperties().BuddylistWindow.Logo.height = 12;
	SkinProperties().BuddylistWindow.Data18.x = 20;
	SkinProperties().BuddylistWindow.Data18.y = 4;
	SkinProperties().BuddylistWindow.Data18.width = 132;
	SkinProperties().BuddylistWindow.Data18.height = 13;
	SkinProperties().BuddylistWindow.Data28.FontNumber = 4;
	SkinProperties().BuddylistWindow.Data28.Color.R = 224;
	SkinProperties().BuddylistWindow.Data28.Color.G = 224;
	SkinProperties().BuddylistWindow.Data28.Color.B = 224;
	SkinProperties().BuddylistWindow.Data28.Alignment = 0;
	SkinProperties().BuddylistWindow.Data3C.R = 96;
	SkinProperties().BuddylistWindow.Data3C.G = 248;
	SkinProperties().BuddylistWindow.Data3C.B = 96;
	SkinProperties().BuddylistWindow.Data48.R = 248;
	SkinProperties().BuddylistWindow.Data48.G = 96;
	SkinProperties().BuddylistWindow.Data48.B = 96;
	SkinProperties().BuddylistWindow.Data54.R = 248;
	SkinProperties().BuddylistWindow.Data54.G = 248;
	SkinProperties().BuddylistWindow.Data54.B = 248;
	SkinProperties().Data868 = 10;
	SkinProperties().Data86C = 14;
	SkinProperties().Data870 = 12;
	SkinProperties().Data874 = 38;
	SkinProperties().Data878 = 12;
	SkinProperties().Data87C = 14;
	SkinProperties().Data880 = 12;
	SkinProperties().Data884 = 38;
	SkinProperties().Data888 = 12;
	SkinProperties().Buttons.Data00.FontNumber = 3;
	SkinProperties().Buttons.Data00.Color.R = 255;
	SkinProperties().Buttons.Data00.Color.G = 255;
	SkinProperties().Buttons.Data00.Color.B = 255;
	SkinProperties().Buttons.Data00.Alignment = 1;
	SkinProperties().Buttons.Data14.FontNumber = 3;
	SkinProperties().Buttons.Data14.Color.R = 192;
	SkinProperties().Buttons.Data14.Color.G = 192;
	SkinProperties().Buttons.Data14.Color.B = 192;
	SkinProperties().Buttons.Data14.Alignment = 1;
	SkinProperties().Buttons.Data28 = 1;
	SkinProperties().Buttons.Texture.x = 2;
	SkinProperties().Buttons.Texture.y = 2;
	SkinProperties().Data8C0 = 100;
	SkinProperties().Style8C4.Data00.FontNumber = 4;
	SkinProperties().Style8C4.Data00.Color.R = 224;
	SkinProperties().Style8C4.Data00.Color.G = 224;
	SkinProperties().Style8C4.Data00.Color.B = 224;
	SkinProperties().Style8C4.Data00.Alignment = 0;
	SkinProperties().Style8C4.Data14.FontNumber = 4;
	SkinProperties().Style8C4.Data14.Color.R = 255;
	SkinProperties().Style8C4.Data14.Color.G = 255;
	SkinProperties().Style8C4.Data14.Color.B = 255;
	SkinProperties().Style8C4.Data14.Alignment = 0;
	SkinProperties().Style8C4.Data28.FontNumber = 4;
	SkinProperties().Style8C4.Data28.Color.R = 160;
	SkinProperties().Style8C4.Data28.Color.G = 160;
	SkinProperties().Style8C4.Data28.Color.B = 255;
	SkinProperties().Style8C4.Data28.Alignment = 0;
	SkinProperties().CheckBox.Item.x = 6;
	SkinProperties().CheckBox.Item.y = 5;
	SkinProperties().CheckBox.Data08.x = 24;
	SkinProperties().CheckBox.Data08.y = 4;
	SkinProperties().CheckBox.Data08.width = -10;
	SkinProperties().CheckBox.Data08.height = -6;
	SkinProperties().CheckBox.Data18.FontNumber = 4;
	SkinProperties().CheckBox.Data18.Color.R = 176;
	SkinProperties().CheckBox.Data18.Color.G = 176;
	SkinProperties().CheckBox.Data18.Color.B = 176;
	SkinProperties().CheckBox.Data18.Alignment = 0;
	SkinProperties().CheckBox.Data2C.FontNumber = 4;
	SkinProperties().CheckBox.Data2C.Color.R = 112;
	SkinProperties().CheckBox.Data2C.Color.G = 112;
	SkinProperties().CheckBox.Data2C.Color.B = 112;
	SkinProperties().CheckBox.Data2C.Alignment = 0;
	SkinProperties().Data940 = 1;
	SkinProperties().Style944.Data00.R = 54;
	SkinProperties().Style944.Data00.G = 54;
	SkinProperties().Style944.Data00.B = 54;
	SkinProperties().Style944.Data0C.R = 71;
	SkinProperties().Style944.Data0C.G = 71;
	SkinProperties().Style944.Data0C.B = 71;
	SkinProperties().Style944.Data18.R = 255;
	SkinProperties().Style944.Data18.G = 255;
	SkinProperties().Style944.Data18.B = 255;
	SkinProperties().Style944.Data24.FontNumber = 4;
	SkinProperties().Style944.Data24.Color.R = 224;
	SkinProperties().Style944.Data24.Color.G = 224;
	SkinProperties().Style944.Data24.Color.B = 224;
	SkinProperties().Style944.Data24.Alignment = 0;
	SkinProperties().Style944.Data38.FontNumber = 4;
	SkinProperties().Style944.Data38.Color.R = 176;
	SkinProperties().Style944.Data38.Color.G = 176;
	SkinProperties().Style944.Data38.Color.B = 176;
	SkinProperties().Style944.Data38.Alignment = 0;
	SkinProperties().Style944.Data4C.R = 128;
	SkinProperties().Style944.Data4C.G = 128;
	SkinProperties().Style944.Data4C.B = 128;
	SkinProperties().Data99C = 3;
	SkinProperties().Style9A0.Data00.R = 64;
	SkinProperties().Style9A0.Data00.G = 64;
	SkinProperties().Style9A0.Data00.B = 64;
	SkinProperties().Style9A0.Data0C.FontNumber = 4;
	SkinProperties().Style9A0.Data0C.Color.R = 176;
	SkinProperties().Style9A0.Data0C.Color.G = 176;
	SkinProperties().Style9A0.Data0C.Color.B = 176;
	SkinProperties().Style9A0.Data0C.Alignment = 0;
	SkinProperties().Style9A0.Data20 = 3;
	SkinProperties().Style9A0.Data24.R = 112;
	SkinProperties().Style9A0.Data24.G = 112;
	SkinProperties().Style9A0.Data24.B = 112;
	SkinProperties().Style9A0.Data30.FontNumber = 4;
	SkinProperties().Style9A0.Data30.Color.R = 255;
	SkinProperties().Style9A0.Data30.Color.G = 255;
	SkinProperties().Style9A0.Data30.Color.B = 255;
	SkinProperties().Style9A0.Data30.Alignment = 0;
	SkinProperties().Data9E4 = 1;
	SkinProperties().Style9E8.Data00 = 106;
	SkinProperties().Style9E8.Data04.x = 18;
	SkinProperties().Style9E8.Data04.y = 0;
	SkinProperties().Style9E8.Data04.width = -82;
	SkinProperties().Style9E8.Data04.height = 18;
	SkinProperties().Style9E8.Data14.x = 0;
	SkinProperties().Style9E8.Data14.y = 0;
	SkinProperties().Style9E8.Data14.width = 0;
	SkinProperties().Style9E8.Data14.height = 18;
	SkinProperties().Style9E8.Data24.x = 23;
	SkinProperties().Style9E8.Data24.y = -20;
	SkinProperties().Style9E8.Data24.width = -4;
	SkinProperties().Style9E8.Data24.height = 16;
	SkinProperties().Style9E8.Data34.x = 55;
	SkinProperties().Style9E8.Data34.y = -20;
	SkinProperties().Style9E8.Data34.width = -4;
	SkinProperties().Style9E8.Data34.height = 16;
	SkinProperties().Style9E8.Data44.x = 4;
	SkinProperties().Style9E8.Data44.y = 21;
	SkinProperties().Style9E8.Data44.width = -4;
	SkinProperties().Style9E8.Data44.height = -22;
	SkinProperties().Style9E8.Data54.x = 0;
	SkinProperties().Style9E8.Data54.y = 0;
	SkinProperties().Style9E8.Data5C.x = -82;
	SkinProperties().Style9E8.Data5C.y = 0;
	SkinProperties().Style9E8.Data64.x = 5;
	SkinProperties().Style9E8.Data64.y = -20;
	SkinProperties().Style9E8.Data6C.x = -64;
	SkinProperties().Style9E8.Data6C.y = 0;
	SkinProperties().Style9E8.Data74.x = -48;
	SkinProperties().Style9E8.Data74.y = 0;
	SkinProperties().Style9E8.Data7C.x = -16;
	SkinProperties().Style9E8.Data7C.y = 0;
	SkinProperties().Style9E8.Data84.x = -32;
	SkinProperties().Style9E8.Data84.y = 0;
	SkinProperties().Style9E8.Data8C.x = 5;
	SkinProperties().Style9E8.Data8C.y = 5;
	SkinProperties().Style9E8.Data8C.width = -17;
	SkinProperties().Style9E8.Data8C.height = -5;
	SkinProperties().Style9E8.Data9C.R = 128;
	SkinProperties().Style9E8.Data9C.G = 128;
	SkinProperties().Style9E8.Data9C.B = 128;
	SkinProperties().Style9E8.DataA8.FontNumber = 4;
	SkinProperties().Style9E8.DataA8.Color.R = 160;
	SkinProperties().Style9E8.DataA8.Color.G = 160;
	SkinProperties().Style9E8.DataA8.Color.B = 255;
	SkinProperties().Style9E8.DataA8.Alignment = 0;
	SkinProperties().StyleAA4.Data00 = 96;
	SkinProperties().StyleAA4.Data04 = 18;
	SkinProperties().StyleAA4.Data08.x = 48;
	SkinProperties().StyleAA4.Data08.y = 4;
	SkinProperties().StyleAA4.Data10 = 4;
	SkinProperties().StyleAA4.Data14.FontNumber = 2;
	SkinProperties().StyleAA4.Data14.Color.R = 224;
	SkinProperties().StyleAA4.Data14.Color.G = 224;
	SkinProperties().StyleAA4.Data14.Color.B = 224;
	SkinProperties().StyleAA4.Data14.Alignment = 1;
	SkinProperties().StyleAA4.Data28.FontNumber = 2;
	SkinProperties().StyleAA4.Data28.Color.R = 128;
	SkinProperties().StyleAA4.Data28.Color.G = 128;
	SkinProperties().StyleAA4.Data28.Color.B = 128;
	SkinProperties().StyleAA4.Data28.Alignment = 1;
	SkinProperties().StyleAA4.Data3C.FontNumber = 2;
	SkinProperties().StyleAA4.Data3C.Color.R = 248;
	SkinProperties().StyleAA4.Data3C.Color.G = 96;
	SkinProperties().StyleAA4.Data3C.Color.B = 96;
	SkinProperties().StyleAA4.Data3C.Alignment = 1;
	SkinProperties().StyleAA4.Data50.FontNumber = 2;
	SkinProperties().StyleAA4.Data50.Color.R = 248;
	SkinProperties().StyleAA4.Data50.Color.G = 248;
	SkinProperties().StyleAA4.Data50.Color.B = 248;
	SkinProperties().StyleAA4.Data50.Alignment = 1;
	SkinProperties().StyleB08.Data00 = 162;
	SkinProperties().StyleB08.Data04 = 68;
	SkinProperties().StyleB08.Data08.x = 5;
	SkinProperties().StyleB08.Data08.y = 5;
	SkinProperties().StyleB08.Data08.width = -5;
	SkinProperties().StyleB08.Data08.height = 10;
	SkinProperties().StyleB08.Data18.FontNumber = 4;
	SkinProperties().StyleB08.Data18.Color.R = 144;
	SkinProperties().StyleB08.Data18.Color.G = 144;
	SkinProperties().StyleB08.Data18.Color.B = 144;
	SkinProperties().StyleB08.Data18.Alignment = 1;
	SkinProperties().StyleB08.Data2C[0].x = -56;
	SkinProperties().StyleB08.Data2C[0].y = -30;
	SkinProperties().StyleB08.Data2C[1].x = -109;
	SkinProperties().StyleB08.Data2C[1].y = -30;
	SkinProperties().StyleB08.Data2C[2].x = -162;
	SkinProperties().StyleB08.Data2C[2].y = -30;
	SkinProperties().StyleB08.Data44.x = 13;
	SkinProperties().StyleB08.Data44.y = -40;
	SkinProperties().StyleB08.Data44.width = -13;
	SkinProperties().StyleB08.Data44.height = 2;
	SkinProperties().StyleB08.Data54.x = 18;
	SkinProperties().StyleB08.Data54.y = 32;
	SkinProperties().StyleB08.Data54.width = -18;
	SkinProperties().StyleB08.Data54.height = -60;
	SkinProperties().StyleB08.Data64.FontNumber = 4;
	SkinProperties().StyleB08.Data64.Color.R = 192;
	SkinProperties().StyleB08.Data64.Color.G = 192;
	SkinProperties().StyleB08.Data64.Color.B = 192;
	SkinProperties().StyleB08.Data64.Alignment = 0;
	SkinProperties().StyleB08.Data78.FontNumber = 4;
	SkinProperties().StyleB08.Data78.Color.R = 192;
	SkinProperties().StyleB08.Data78.Color.G = 192;
	SkinProperties().StyleB08.Data78.Color.B = 192;
	SkinProperties().StyleB08.Data78.Alignment = 0;
	SkinProperties().DataB94 = 200;
	SkinProperties().DataB98 = 86;
	SkinProperties().DataB9C.x = 0;
	SkinProperties().DataB9C.y = 2;
	SkinProperties().DataB9C.width = 107;
	SkinProperties().DataB9C.height = 14;
	SkinProperties().DataBAC.x = -86;
	SkinProperties().DataBAC.y = 0;
	SkinProperties().DataBAC.width = 0;
	SkinProperties().DataBAC.height = 16;
	SkinProperties().DataBBC.x = 0;
	SkinProperties().DataBBC.y = 31;
	SkinProperties().DataBBC.width = 107;
	SkinProperties().DataBBC.height = 14;
	SkinProperties().DataBCC.x = -86;
	SkinProperties().DataBCC.y = 29;
	SkinProperties().DataBCC.width = 0;
	SkinProperties().DataBCC.height = 16;
	SkinProperties().DataBDC.x = 0;
	SkinProperties().DataBDC.y = 58;
	SkinProperties().DataBDC.width = 0;
	SkinProperties().DataBDC.height = 28;
	SkinProperties().DataBEC = 13;
	SkinProperties().DataBF0 = 200;
	SkinProperties().DataBF4 = 202;
	SkinProperties().DataBF8.x = 0;
	SkinProperties().DataBF8.y = 0;
	SkinProperties().DataBF8.width = 0;
	SkinProperties().DataBF8.height = 14;
	SkinProperties().DataC08.x = 0;
	SkinProperties().DataC08.y = 15;
	SkinProperties().DataC08.width = 0;
	SkinProperties().DataC08.height = 146;
	SkinProperties().DataC18.x = 0;
	SkinProperties().DataC18.y = 174;
	SkinProperties().DataC18.width = 0;
	SkinProperties().DataC18.height = 28;
	SkinProperties().DataC28 = 200;
	SkinProperties().DataC2C = 44;
	SkinProperties().DataC30.x = 45;
	SkinProperties().DataC30.y = 32;
	SkinProperties().DataC30.width = 0;
	SkinProperties().DataC30.height = 12;
	SkinProperties().DataC40.x = 45;
	SkinProperties().DataC40.y = 2;
	SkinProperties().DataC40.width = 0;
	SkinProperties().DataC40.height = 28;
	SkinProperties().DataC50.x = 0;
	SkinProperties().DataC50.y = 0;
	SkinProperties().DataC58 = 384;
	SkinProperties().DataC5C = 133;
	SkinProperties().DataC60.x = 0;
	SkinProperties().DataC60.y = 0;
	SkinProperties().DataC60.width = 68;
	SkinProperties().DataC60.height = 68;
	SkinProperties().DataC70 = 8;
	SkinProperties().DataC74.R = 71;
	SkinProperties().DataC74.G = 71;
	SkinProperties().DataC74.B = 71;
	SkinProperties().DataC80.x = 13;
	SkinProperties().DataC80.y = 72;
	SkinProperties().DataC88[0].x = 81;
	SkinProperties().DataC88[0].y = 0;
	SkinProperties().DataC88[1].x = 81;
	SkinProperties().DataC88[1].y = 24;
	SkinProperties().DataC88[2].x = 81;
	SkinProperties().DataC88[2].y = 48;
	SkinProperties().DataC88[3].x = 81;
	SkinProperties().DataC88[3].y = 72;
	SkinProperties().DataCA8.x = 137;
	SkinProperties().DataCA8.y = 0;
	SkinProperties().DataCB0 = 13;
	SkinProperties().DataCB4 = 13;
	SkinProperties().DataCB8.x = 0;
	SkinProperties().DataCB8.y = 105;
	SkinProperties().DataCC0 = 250;
	SkinProperties().DataCC4 = 177;
	SkinProperties().DataCC8.x = 0;
	SkinProperties().DataCC8.y = 0;
	SkinProperties().DataCC8.width = 32;
	SkinProperties().DataCC8.height = 32;
	SkinProperties().DataCD8.R = 71;
	SkinProperties().DataCD8.G = 71;
	SkinProperties().DataCD8.B = 71;
	SkinProperties().DataCE4.x = 45;
	SkinProperties().DataCE4.y = 0;
	SkinProperties().DataCE4.width = 0;
	SkinProperties().DataCE4.height = 42;
	SkinProperties().DataCF4.x = 0;
	SkinProperties().DataCF4.y = 45;
	SkinProperties().DataCF4.width = 0;
	SkinProperties().DataCF4.height = 132;
	SkinProperties().DataD04 = 250;
	SkinProperties().DataD08 = 177;
	SkinProperties().DataD0C.x = 0;
	SkinProperties().DataD0C.y = 0;
	SkinProperties().DataD0C.width = 32;
	SkinProperties().DataD0C.height = 32;
	SkinProperties().DataD1C.x = 45;
	SkinProperties().DataD1C.y = 0;
	SkinProperties().DataD1C.width = 0;
	SkinProperties().DataD1C.height = 28;
	SkinProperties().DataD2C.x = 0;
	SkinProperties().DataD2C.y = 45;
	SkinProperties().DataD2C.width = 0;
	SkinProperties().DataD2C.height = 132;
	SkinProperties().StyleD3C.Data00 = 498;
	SkinProperties().StyleD3C.Data04 = 272;
	SkinProperties().StyleD3C.Data08.x = 0;
	SkinProperties().StyleD3C.Data08.y = 0;
	SkinProperties().StyleD3C.Data08.width = 242;
	SkinProperties().StyleD3C.Data08.height = 22;
	SkinProperties().StyleD3C.Data18.x = 0;
	SkinProperties().StyleD3C.Data18.y = 35;
	SkinProperties().StyleD3C.Data18.width = 0;
	SkinProperties().StyleD3C.Data18.height = 14;
	SkinProperties().StyleD3C.Data28.x = 0;
	SkinProperties().StyleD3C.Data28.y = 50;
	SkinProperties().StyleD3C.Data28.width = 242;
	SkinProperties().StyleD3C.Data28.height = 104;
	SkinProperties().StyleD3C.Data38.x = 161;
	SkinProperties().StyleD3C.Data38.y = 159;
	SkinProperties().StyleD3C.Data40.x = 208;
	SkinProperties().StyleD3C.Data40.y = 159;
	SkinProperties().StyleD3C.Data48.x = 0;
	SkinProperties().StyleD3C.Data48.y = 158;
	SkinProperties().StyleD3C.Data48.width = 146;
	SkinProperties().StyleD3C.Data48.height = 22;
	SkinProperties().StyleD3C.Data58.x = 0;
	SkinProperties().StyleD3C.Data58.y = 197;
	SkinProperties().StyleD3C.Data58.width = 0;
	SkinProperties().StyleD3C.Data58.height = 14;
	SkinProperties().StyleD3C.Data68.x = 0;
	SkinProperties().StyleD3C.Data68.y = 215;
	SkinProperties().StyleD3C.Data68.width = 242;
	SkinProperties().StyleD3C.Data68.height = 22;
	SkinProperties().StyleD3C.Data78.x = 0;
	SkinProperties().StyleD3C.Data78.y = 250;
	SkinProperties().StyleD3C.Data78.width = 242;
	SkinProperties().StyleD3C.Data78.height = 22;
	SkinProperties().StyleD3C.Data88.R = 192;
	SkinProperties().StyleD3C.Data88.G = 0;
	SkinProperties().StyleD3C.Data88.B = 0;
	SkinProperties().StyleD3C.Data94.x = 256;
	SkinProperties().StyleD3C.Data94.y = 0;
	SkinProperties().StyleD3C.Data94.width = 242;
	SkinProperties().StyleD3C.Data94.height = 22;
	SkinProperties().StyleD3C.DataA4.x = 256;
	SkinProperties().StyleD3C.DataA4.y = 35;
	SkinProperties().StyleD3C.DataA4.width = 0;
	SkinProperties().StyleD3C.DataA4.height = 14;
	SkinProperties().StyleD3C.DataB4.x = 256;
	SkinProperties().StyleD3C.DataB4.y = 50;
	SkinProperties().StyleD3C.DataB4.width = 242;
	SkinProperties().StyleD3C.DataB4.height = 104;
	SkinProperties().StyleD3C.DataC4.x = 417;
	SkinProperties().StyleD3C.DataC4.y = 159;
	SkinProperties().StyleD3C.DataCC.x = 464;
	SkinProperties().StyleD3C.DataCC.y = 159;
	SkinProperties().StyleD3C.DataD4.x = 256;
	SkinProperties().StyleD3C.DataD4.y = 158;
	SkinProperties().StyleD3C.DataD4.width = 146;
	SkinProperties().StyleD3C.DataD4.height = 22;
	SkinProperties().StyleD3C.DataE4.x = 256;
	SkinProperties().StyleD3C.DataE4.y = 197;
	SkinProperties().StyleD3C.DataE4.width = 0;
	SkinProperties().StyleD3C.DataE4.height = 14;
	SkinProperties().StyleD3C.DataF4.x = 256;
	SkinProperties().StyleD3C.DataF4.y = 215;
	SkinProperties().StyleD3C.DataF4.width = 242;
	SkinProperties().StyleD3C.DataF4.height = 22;
	SkinProperties().DataE40 = 250;
	SkinProperties().DataE44 = 161;
	SkinProperties().DataE48.x = 0;
	SkinProperties().DataE48.y = 29;
	SkinProperties().DataE48.width = 0;
	SkinProperties().DataE48.height = 132;
	SkinProperties().DataE58.x = 0;
	SkinProperties().DataE58.y = 0;
	SkinProperties().DataE58.width = 0;
	SkinProperties().DataE58.height = 28;
	SkinProperties().StyleE68.Data00 = 190;
	SkinProperties().StyleE68.Data04 = 111;
	SkinProperties().StyleE68.Data08.FontNumber = 4;
	SkinProperties().StyleE68.Data08.Color.R = 176;
	SkinProperties().StyleE68.Data08.Color.G = 176;
	SkinProperties().StyleE68.Data08.Color.B = 176;
	SkinProperties().StyleE68.Data08.Alignment = 1;
	SkinProperties().StyleE68.Data1C.FontNumber = 4;
	SkinProperties().StyleE68.Data1C.Color.R = 176;
	SkinProperties().StyleE68.Data1C.Color.G = 176;
	SkinProperties().StyleE68.Data1C.Color.B = 176;
	SkinProperties().StyleE68.Data1C.Alignment = 0;
	SkinProperties().StyleE68.Data30 = 8;
	SkinProperties().StyleE68.Data34.x = 0;
	SkinProperties().StyleE68.Data34.y = 0;
	SkinProperties().StyleE68.Data34.width = 0;
	SkinProperties().StyleE68.Data34.height = 70;
	SkinProperties().StyleE68.Data44.x = 0;
	SkinProperties().StyleE68.Data44.y = 91;
	SkinProperties().StyleE68.Data44.width = 0;
	SkinProperties().StyleE68.Data44.height = 20;
	SkinProperties().StyleE68.Data54.x = 0;
	SkinProperties().StyleE68.Data54.y = 74;
	SkinProperties().StyleE68.Data54.width = 0;
	SkinProperties().StyleE68.Data54.height = 2;
	SkinProperties().DataECC = 200;
	SkinProperties().DataED0 = 163;
	SkinProperties().DataED4.x = 0;
	SkinProperties().DataED4.y = 0;
	SkinProperties().DataED4.width = 0;
	SkinProperties().DataED4.height = 14;
	SkinProperties().DataEE4.x = 0;
	SkinProperties().DataEE4.y = 15;
	SkinProperties().DataEE4.width = 0;
	SkinProperties().DataEE4.height = 104;
	SkinProperties().DataEF4.x = 0;
	SkinProperties().DataEF4.y = 132;
	SkinProperties().DataEF4.width = 0;
	SkinProperties().DataEF4.height = 14;
	SkinProperties().DataF04.x = 0;
	SkinProperties().DataF04.y = 147;
	SkinProperties().DataF04.width = 0;
	SkinProperties().DataF04.height = 16;
	SkinProperties().DataF14 = 225;
	SkinProperties().DataF18 = 31;
	SkinProperties().DataF1C.x = 0;
	SkinProperties().DataF1C.y = 15;
	SkinProperties().DataF1C.width = 0;
	SkinProperties().DataF1C.height = 16;
	SkinProperties().DataF2C.x = 0;
	SkinProperties().DataF2C.y = 0;
	SkinProperties().DataF2C.width = 0;
	SkinProperties().DataF2C.height = 14;
	SkinProperties().DataF3C = 250;
	SkinProperties().DataF40 = 87;
	SkinProperties().DataF44.x = 0;
	SkinProperties().DataF44.y = 71;
	SkinProperties().DataF44.width = 0;
	SkinProperties().DataF44.height = 16;
	SkinProperties().DataF54.x = 0;
	SkinProperties().DataF54.y = 0;
	SkinProperties().DataF54.width = 0;
	SkinProperties().DataF54.height = 70;
	SkinProperties().DataF64 = 200;
	SkinProperties().DataF68 = 31;
	SkinProperties().DataF6C.x = 0;
	SkinProperties().DataF6C.y = 15;
	SkinProperties().DataF6C.width = 0;
	SkinProperties().DataF6C.height = 16;
	SkinProperties().DataF7C.x = 0;
	SkinProperties().DataF7C.y = 0;
	SkinProperties().DataF7C.width = 0;
	SkinProperties().DataF7C.height = 14;
	SkinProperties().StyleF8C.Data00 = 250;
	SkinProperties().StyleF8C.Data04 = 174;
	SkinProperties().StyleF8C.Data08.x = 0;
	SkinProperties().StyleF8C.Data08.y = 0;
	SkinProperties().StyleF8C.Data08.width = 0;
	SkinProperties().StyleF8C.Data08.height = 14;
	SkinProperties().StyleF8C.Data18.x = 0;
	SkinProperties().StyleF8C.Data18.y = 15;
	SkinProperties().StyleF8C.Data18.width = 0;
	SkinProperties().StyleF8C.Data18.height = 2;
	SkinProperties().StyleF8C.Data28.x = 0;
	SkinProperties().StyleF8C.Data28.y = 32;
	SkinProperties().StyleF8C.Data28.width = 0;
	SkinProperties().StyleF8C.Data28.height = 14;
	SkinProperties().StyleF8C.Data38[0].x = 0;
	SkinProperties().StyleF8C.Data38[0].y = 47;
	SkinProperties().StyleF8C.Data38[1].x = 20;
	SkinProperties().StyleF8C.Data38[1].y = 47;
	SkinProperties().StyleF8C.Data38[2].x = 40;
	SkinProperties().StyleF8C.Data38[2].y = 47;
	SkinProperties().StyleF8C.Data38[3].x = 60;
	SkinProperties().StyleF8C.Data38[3].y = 47;
	SkinProperties().StyleF8C.Data38[4].x = 80;
	SkinProperties().StyleF8C.Data38[4].y = 47;
	SkinProperties().StyleF8C.Data38[5].x = 100;
	SkinProperties().StyleF8C.Data38[5].y = 47;
	SkinProperties().StyleF8C.Data38[6].x = 120;
	SkinProperties().StyleF8C.Data38[6].y = 47;
	SkinProperties().StyleF8C.Data38[7].x = 140;
	SkinProperties().StyleF8C.Data38[7].y = 47;
	SkinProperties().StyleF8C.Data38[8].x = 160;
	SkinProperties().StyleF8C.Data38[8].y = 47;
	SkinProperties().StyleF8C.Data38[9].x = 180;
	SkinProperties().StyleF8C.Data38[9].y = 47;
	SkinProperties().StyleF8C.Data38[10].x = 200;
	SkinProperties().StyleF8C.Data38[10].y = 47;
	SkinProperties().StyleF8C.Data90.x = 0;
	SkinProperties().StyleF8C.Data90.y = 76;
	SkinProperties().StyleF8C.Data90.width = 0;
	SkinProperties().StyleF8C.Data90.height = 14;
	SkinProperties().StyleF8C.DataA0.x = 0;
	SkinProperties().StyleF8C.DataA0.y = 91;
	SkinProperties().StyleF8C.DataA0.width = 0;
	SkinProperties().StyleF8C.DataA0.height = 48;
	SkinProperties().StyleF8C.DataB0.x = 0;
	SkinProperties().StyleF8C.DataB0.y = 152;
	SkinProperties().StyleF8C.DataB0.width = 0;
	SkinProperties().StyleF8C.DataB0.height = 22;
	SkinProperties().Data104C = 250;
	SkinProperties().Data1050 = 29;
	SkinProperties().Data1054.x = 0;
	SkinProperties().Data1054.y = 0;
	SkinProperties().Data1054.width = 0;
	SkinProperties().Data1054.height = 14;
	SkinProperties().Data1074.x = 0;
	SkinProperties().Data1074.y = 18;
	SkinProperties().Data1074.width = 0;
	SkinProperties().Data1074.height = 11;
	SkinProperties().Style1084.Data00 = 250;
	SkinProperties().Style1084.Data04 = 185;
	SkinProperties().Style1084.Data08.x = 0;
	SkinProperties().Style1084.Data08.y = 0;
	SkinProperties().Style1084.Data10.x = 0;
	SkinProperties().Style1084.Data10.y = 33;
	SkinProperties().Style1084.Data18.x = 0;
	SkinProperties().Style1084.Data18.y = 66;
	SkinProperties().Style1084.Data20.x = 0;
	SkinProperties().Style1084.Data20.y = 99;
	SkinProperties().Style1084.Data28.x = 0;
	SkinProperties().Style1084.Data28.y = 132;
	SkinProperties().Style1084.Data30.x = 0;
	SkinProperties().Style1084.Data30.y = 165;
	SkinProperties().Style1084.Data38.x = 99;
	SkinProperties().Style1084.Data38.y = 0;
	SkinProperties().Style1084.Data40.x = 99;
	SkinProperties().Style1084.Data40.y = 33;
	SkinProperties().Style1084.Data48.x = 99;
	SkinProperties().Style1084.Data48.y = 66;
	SkinProperties().Style1084.Data50.x = 99;
	SkinProperties().Style1084.Data50.y = 99;
	SkinProperties().Style1084.Data58.x = 99;
	SkinProperties().Style1084.Data58.y = 132;
	SkinProperties().Style1084.Data60.x = 99;
	SkinProperties().Style1084.Data60.y = 165;
	SkinProperties().Data10EC = 200;
	SkinProperties().Data10F0 = 119;
	SkinProperties().Data10F4.x = 0;
	SkinProperties().Data10F4.y = 0;
	SkinProperties().Data10F4.width = 0;
	SkinProperties().Data10F4.height = 14;
	SkinProperties().Data1104.x = 0;
	SkinProperties().Data1104.y = 15;
	SkinProperties().Data1104.width = 0;
	SkinProperties().Data1104.height = 104;
	SkinProperties().Style1114.Data00 = 300;
	SkinProperties().Style1114.Data04 = 346;
	SkinProperties().Style1114.Data08.x = 0;
	SkinProperties().Style1114.Data08.y = 0;
	SkinProperties().Style1114.Data08.width = 0;
	SkinProperties().Style1114.Data08.height = 14;
	SkinProperties().Style1114.Data18.x = 0;
	SkinProperties().Style1114.Data18.y = 15;
	SkinProperties().Style1114.Data18.width = 0;
	SkinProperties().Style1114.Data18.height = 16;
	SkinProperties().Style1114.Data28.x = 0;
	SkinProperties().Style1114.Data28.y = 92;
	SkinProperties().Style1114.Data28.width = 0;
	SkinProperties().Style1114.Data28.height = 14;
	SkinProperties().Style1114.Data38.x = 0;
	SkinProperties().Style1114.Data38.y = 107;
	SkinProperties().Style1114.Data38.width = 0;
	SkinProperties().Style1114.Data38.height = 77;
	SkinProperties().Style1114.Data48.x = 0;
	SkinProperties().Style1114.Data48.y = 199;
	SkinProperties().Style1114.Data48.width = 0;
	SkinProperties().Style1114.Data48.height = 14;
	SkinProperties().Style1114.Data58.x = 0;
	SkinProperties().Style1114.Data58.y = 214;
	SkinProperties().Style1114.Data58.width = 0;
	SkinProperties().Style1114.Data58.height = 49;
	SkinProperties().Style1114.Data68.x = 0;
	SkinProperties().Style1114.Data68.y = 315;
	SkinProperties().Style1114.Data68.width = 0;
	SkinProperties().Style1114.Data68.height = 14;
	SkinProperties().Style1114.Data78.x = 0;
	SkinProperties().Style1114.Data78.y = 330;
	SkinProperties().Style1114.Data78.width = 0;
	SkinProperties().Style1114.Data78.height = 16;
	SkinProperties().Style1114.Data88.x = 0;
	SkinProperties().Style1114.Data88.y = 46;
	SkinProperties().Style1114.Data88.width = 0;
	SkinProperties().Style1114.Data88.height = 14;
	SkinProperties().Style1114.Data98.x = 0;
	SkinProperties().Style1114.Data98.y = 61;
	SkinProperties().Style1114.Data98.width = 0;
	SkinProperties().Style1114.Data98.height = 16;
	SkinProperties().Style1114.DataA8.x = 0;
	SkinProperties().Style1114.DataA8.y = 278;
	SkinProperties().Style1114.DataA8.width = 0;
	SkinProperties().Style1114.DataA8.height = 22;
	SkinProperties().Data11CC = 250;
	SkinProperties().Data11D0 = 97;
	SkinProperties().Data11D4.x = 0;
	SkinProperties().Data11D4.y = 0;
	SkinProperties().Data11D4.width = 0;
	SkinProperties().Data11D4.height = 14;
	SkinProperties().Data11E4.x = 0;
	SkinProperties().Data11E4.y = 15;
	SkinProperties().Data11E4.width = 0;
	SkinProperties().Data11E4.height = 16;
	SkinProperties().Data11F4.x = 0;
	SkinProperties().Data11F4.y = 46;
	SkinProperties().Data11F4.width = 0;
	SkinProperties().Data11F4.height = 14;
	SkinProperties().Data1204[0].x = 0;
	SkinProperties().Data1204[0].y = 61;
	SkinProperties().Data1204[1].x = 20;
	SkinProperties().Data1204[1].y = 61;
	SkinProperties().Data1204[2].x = 40;
	SkinProperties().Data1204[2].y = 61;
	SkinProperties().Data1204[3].x = 60;
	SkinProperties().Data1204[3].y = 61;
	SkinProperties().Data1204[4].x = 80;
	SkinProperties().Data1204[4].y = 61;
	SkinProperties().Data1204[5].x = 100;
	SkinProperties().Data1204[5].y = 61;
	SkinProperties().Data1204[6].x = 120;
	SkinProperties().Data1204[6].y = 61;
	SkinProperties().Data1204[7].x = 0;
	SkinProperties().Data1204[7].y = 81;
	SkinProperties().Data1204[8].x = 20;
	SkinProperties().Data1204[8].y = 81;
	SkinProperties().Data1204[9].x = 40;
	SkinProperties().Data1204[9].y = 81;
	SkinProperties().Data1204[10].x = 60;
	SkinProperties().Data1204[10].y = 81;
	SkinProperties().Data1204[11].x = 80;
	SkinProperties().Data1204[11].y = 81;
	SkinProperties().Data1204[12].x = 100;
	SkinProperties().Data1204[12].y = 81;
	SkinProperties().Data1204[13].x = 120;
	SkinProperties().Data1204[13].y = 81;
	SkinProperties().Style1274.Data00 = 250;
	SkinProperties().Style1274.Data04 = 202;
	SkinProperties().Style1274.Data08.x = 0;
	SkinProperties().Style1274.Data08.y = 0;
	SkinProperties().Style1274.Data10.x = 0;
	SkinProperties().Style1274.Data10.y = 33;
	SkinProperties().Style1274.Data18.x = 0;
	SkinProperties().Style1274.Data18.y = 66;
	SkinProperties().Style1274.Data20.x = 0;
	SkinProperties().Style1274.Data20.y = 99;
	SkinProperties().Style1274.Data28.x = 0;
	SkinProperties().Style1274.Data28.y = 132;
	SkinProperties().Style1274.Data30.x = 0;
	SkinProperties().Style1274.Data30.y = 182;
	SkinProperties().Style1274.Data38.x = 99;
	SkinProperties().Style1274.Data38.y = 0;
	SkinProperties().Style1274.Data40.x = 99;
	SkinProperties().Style1274.Data40.y = 33;
	SkinProperties().Style1274.Data48.x = 99;
	SkinProperties().Style1274.Data48.y = 66;
	SkinProperties().Style1274.Data50.x = 99;
	SkinProperties().Style1274.Data50.y = 99;
	SkinProperties().Style1274.Data58.x = 99;
	SkinProperties().Style1274.Data58.y = 132;
	SkinProperties().Style1274.Data60.x = 99;
	SkinProperties().Style1274.Data60.y = 182;
	SkinProperties().Style1274.Data68.x = 0;
	SkinProperties().Style1274.Data68.y = 165;
	SkinProperties().Style1274.Data68.width = 0;
	SkinProperties().Style1274.Data68.height = 2;
	SkinProperties().Data12EC = 200;
	SkinProperties().Data12F0 = 154;
	SkinProperties().Data12F4.x = 0;
	SkinProperties().Data12F4.y = 0;
	SkinProperties().Data12F4.width = 0;
	SkinProperties().Data12F4.height = 22;
	SkinProperties().Data1304.x = 0;
	SkinProperties().Data1304.y = 33;
	SkinProperties().Data1304.width = 0;
	SkinProperties().Data1304.height = 22;
	SkinProperties().Data1314.x = 0;
	SkinProperties().Data1314.y = 66;
	SkinProperties().Data1314.width = 0;
	SkinProperties().Data1314.height = 22;
	SkinProperties().Data1324.x = 0;
	SkinProperties().Data1324.y = 132;
	SkinProperties().Data1324.width = 0;
	SkinProperties().Data1324.height = 22;
	SkinProperties().Data1334.x = 0;
	SkinProperties().Data1334.y = 99;
	SkinProperties().Data1334.width = 0;
	SkinProperties().Data1334.height = 22;
	SkinProperties().Data1344 = 200;
	SkinProperties().Data1348 = 185;
	SkinProperties().Data134C.x = 0;
	SkinProperties().Data134C.y = 0;
	SkinProperties().Data134C.width = 0;
	SkinProperties().Data134C.height = 22;
	SkinProperties().Data135C.x = 0;
	SkinProperties().Data135C.y = 50;
	SkinProperties().Data135C.width = 0;
	SkinProperties().Data135C.height = 104;
	SkinProperties().Data136C.x = 0;
	SkinProperties().Data136C.y = 35;
	SkinProperties().Data136C.width = 0;
	SkinProperties().Data136C.height = 14;
	SkinProperties().Data137C.x = -86;
	SkinProperties().Data137C.y = 167;
	SkinProperties().Data1384.x = 0;
	SkinProperties().Data1384.y = 171;
	SkinProperties().Data1384.width = 0;
	SkinProperties().Data1384.height = 14;
	SkinProperties().Style1394.Data00 = 200;
	SkinProperties().Style1394.Data04 = 213;
	SkinProperties().Style1394.Data08.x = 0;
	SkinProperties().Style1394.Data08.y = 0;
	SkinProperties().Style1394.Data10.x = 0;
	SkinProperties().Style1394.Data10.y = 15;
	SkinProperties().Style1394.Data18.x = 70;
	SkinProperties().Style1394.Data18.y = 15;
	SkinProperties().Style1394.Data20.x = 140;
	SkinProperties().Style1394.Data20.y = 15;
	SkinProperties().Style1394.Data28.x = 0;
	SkinProperties().Style1394.Data28.y = 51;
	SkinProperties().Style1394.Data28.width = 0;
	SkinProperties().Style1394.Data28.height = 22;
	SkinProperties().Style1394.Data38.x = 0;
	SkinProperties().Style1394.Data38.y = 86;
	SkinProperties().Style1394.Data38.width = 0;
	SkinProperties().Style1394.Data38.height = 22;
	SkinProperties().Style1394.Data48.x = 0;
	SkinProperties().Style1394.Data48.y = 121;
	SkinProperties().Style1394.Data48.width = 0;
	SkinProperties().Style1394.Data48.height = 22;
	SkinProperties().Style1394.Data58.x = 0;
	SkinProperties().Style1394.Data58.y = 181;
	SkinProperties().Style1394.Data58.width = 0;
	SkinProperties().Style1394.Data58.height = 12;
	SkinProperties().Style1394.Data68.x = 0;
	SkinProperties().Style1394.Data68.y = 166;
	SkinProperties().Style1394.Data70.x = 0;
	SkinProperties().Style1394.Data70.y = 199;
	SkinProperties().Data140C = 260;
	SkinProperties().Data1410 = 162;
	SkinProperties().Data1414.x = 0;
	SkinProperties().Data1414.y = 0;
	SkinProperties().Data1414.width = 0;
	SkinProperties().Data1414.height = 22;
	SkinProperties().Data1424.x = 0;
	SkinProperties().Data1424.y = 35;
	SkinProperties().Data1424.width = 0;
	SkinProperties().Data1424.height = 22;
	SkinProperties().Data1434.x = 0;
	SkinProperties().Data1434.y = 70;
	SkinProperties().Data1434.width = 0;
	SkinProperties().Data1434.height = 22;
	SkinProperties().Data1444.x = 0;
	SkinProperties().Data1444.y = 105;
	SkinProperties().Data1444.width = 0;
	SkinProperties().Data1444.height = 22;
	SkinProperties().Data1454.x = 0;
	SkinProperties().Data1454.y = 140;
	SkinProperties().Data1454.width = 0;
	SkinProperties().Data1454.height = 22;
	SkinProperties().Data1464 = 200;
	SkinProperties().Data1468 = 22;
	SkinProperties().Data146C.x = 0;
	SkinProperties().Data146C.y = 0;
	SkinProperties().Data146C.width = 0;
	SkinProperties().Data146C.height = 22;
	SkinProperties().Data147C = 200;
	SkinProperties().Data1480 = 198;
	SkinProperties().Data1484.x = 0;
	SkinProperties().Data1484.y = 15;
	SkinProperties().Data1484.width = 0;
	SkinProperties().Data1484.height = 104;
	SkinProperties().Data1494.x = 0;
	SkinProperties().Data1494.y = 0;
	SkinProperties().Data1494.width = 0;
	SkinProperties().Data1494.height = 14;
	SkinProperties().Data14A4.x = 0;
	SkinProperties().Data14A4.y = 147;
	SkinProperties().Data14A4.width = 0;
	SkinProperties().Data14A4.height = 16;
	SkinProperties().Data14B4.x = 0;
	SkinProperties().Data14B4.y = 132;
	SkinProperties().Data14B4.width = 0;
	SkinProperties().Data14B4.height = 14;
	SkinProperties().Data14C4.x = 0;
	SkinProperties().Data14C4.y = 176;
	SkinProperties().Data14C4.width = 0;
	SkinProperties().Data14C4.height = 22;
	SkinProperties().Data14D4.R = 255;
	SkinProperties().Data14D4.G = 255;
	SkinProperties().Data14D4.B = 255;
	SkinProperties().Data14E0 = 475;
	SkinProperties().Data14E4 = 284;
	SkinProperties().Data14E8.x = 0;
	SkinProperties().Data14E8.y = 0;
	SkinProperties().Data14E8.width = 0;
	SkinProperties().Data14E8.height = 56;
	SkinProperties().Data14F8.x = 0;
	SkinProperties().Data14F8.y = 57;
	SkinProperties().Data14F8.width = 0;
	SkinProperties().Data14F8.height = 174;
	SkinProperties().Data1508.x = 0;
	SkinProperties().Data1508.y = 253;
	SkinProperties().Data1508.width = 0;
	SkinProperties().Data1508.height = 14;
	SkinProperties().Data1518.x = 0;
	SkinProperties().Data1518.y = 268;
	SkinProperties().Data1518.width = 0;
	SkinProperties().Data1518.height = 16;
}

// --- Timers ---------------------------------------------------------------

// 0x49af20, line 851. Takes the first free entry.
void patella::TGUI::AddTimer(TTimer *Timer)
{
	const auto Free = std::ranges::find(Timers, nullptr);
	if (Free == Timers.end())
		Timers.push_back(Timer);
	else
		*Free = Timer;
}

// 0x49b0d0, line 868
void patella::TGUI::RemoveTimer(TTimer *Timer)
{
	const auto Entry = std::ranges::find(Timers, Timer);
	if (Entry != Timers.end())
		*Entry = nullptr;
}

// 0x4a27c0, line 887. Called by ProcessEvents. The first call only starts
// the clock.
void patella::TGUI::UpdateTimers()
{
	if (!LastTimeSet) { // the original's function-local static, initialised on the first call (issue #107)
		LastTime = GetMilliseconds();
		LastTimeSet = true;
	}
	const int elapsed = GetMilliseconds() - LastTime;
	LastTime = GetMilliseconds();
	// By index, the size read each time: a timer's callback may add or remove timers, as in the original.
	for (std::size_t i = 0; i < Timers.size(); i++) {
		if (Timers[i] != nullptr && !Timers[i]->IsExpired())
			Timers[i]->Update(elapsed);
	}
}

// 0x49b290, line 905. A time of 0 leaves the timer inactive.
TTimer::TTimer(int Time, TCallback Callback)
{
	this->Callback = Callback;
	Remaining = Time * 100;
	if (Remaining == 0) {
		Active = false;
	} else {
		AddTimer(this);
		Active = true;
	}
}

// 0x4a1dc0, the deleting destructor; this destructor is inlined into it.
TTimer::~TTimer()
{
	RemoveTimer(this);
	Active = false;
}

// 0x4a1df0, line 947. The callback is sent on every update, not only when
// the time is up.
void TTimer::Update(int elapsed)
{
	if (Active) {
		if (Remaining > 0) {
			Remaining -= elapsed;
			if (Remaining < 0)
				Remaining = 0;
		}
		Callback.Send();
		if (Remaining == 0) {
			RemoveTimer(this);
			Active = false;
		}
	}
}

// --- TComponent -------------------------------------------------------------

// 0x49b450, line 993
void TComponent::InsertChild([[maybe_unused]] TComponent *Child)
{
	TERROR("not possible");
}

// 0x49b570, line 1001
void TComponent::AppendChild([[maybe_unused]] TComponent *Child)
{
	TERROR("not possible");
}

// 0x49b690, line 1009
void TComponent::RemoveChild([[maybe_unused]] TComponent *Child)
{
	TERROR("not possible");
}

// 0x49b7b0, line 1017
void TComponent::DeleteChild([[maybe_unused]] TComponent *Child)
{
	TERROR("not possible");
}

// 0x49b8d0, line 1025
void TComponent::InsertChildAfter([[maybe_unused]] TComponent *Previous, [[maybe_unused]] TComponent *Child)
{
	TERROR("not possible");
}

// Line 1033 has no function in the binary (removed by the linker).

// 0x49b9f0, line 1041
void TComponent::SwapChildren([[maybe_unused]] TComponent *Child1, [[maybe_unused]] TComponent *Child2)
{
	TERROR("not possible");
}

// 0x49bb10, line 1049
bool TComponent::IsChild([[maybe_unused]] TComponent *Child)
{
	TERROR("not possible");
}

// 0x49bc30, lines 1057-1066. The assertion comes before the try block.
TComponent *TComponent::GetPrevious()
{
	PATELLA_ASSERT(Parent!=nullptr);
	TComponent *Search = Parent->GetFirstChild();
	while (Search != nullptr) {
		if (Search->GetNext() == this)
			return Search;
		Search = Search->GetNext();
	}
	return nullptr;
}

// 0x49bee0, lines 1092-1093. No try block.
void TComponent::SetSize(int _Width, int _Height)
{
	PATELLA_ASSERT_VALUES(_Width>=0, _Width);
	PATELLA_ASSERT_VALUES(_Height>=0, _Height);
	Height = _Height;
	Width = _Width;
}

// 0x4a1fb0, lines 1118-1163. Draws the drawing list at (x, y); entries
// without a size draw the whole skin once.
void TComponent::Draw(int Surface, int x, int y)
{
	int i = 0;
	for (; i < DrawingListLength; i++) {
		if (DrawingList[i].skin == 0)
			continue;
		PATELLA_ASSERT_VALUES(DrawingList[i].skin<patella::bridge::Skins().NumberOfSkins, DrawingList[i].skin);
		if (DrawingList[i].width == 0 && DrawingList[i].height == 0) {
			const TSkin *Skin = &Skins()[DrawingList[i].skin];
			DrawPicturePart(Surface, DrawingList[i].x + x, DrawingList[i].y + y, 4, Skin->x, Skin->y, Skin->width,
				Skin->height);
			continue;
		}
		if (DrawingList[i].x + x < 0 || DrawingList[i].y + y < 0) {
			std::stringstream s;
			const char *Name = " (CONSOLETEXT_CORNER_TOPRIGHT)";
			if (DrawingList[i].skin != 170)
				Name = "";
			s << "dialog origin outside main window " << "(surface: " << Surface << " x: " << x << " y: " << y
			  << " DrawingListLength: " << DrawingListLength << " i: " << i << " DrawingList.Skin: "
			  << DrawingList[i].skin << Name << " DrawingList.x: " << DrawingList[i].x
			  << " DrawingList.y: " << DrawingList[i].y << " DrawingList.width: " << DrawingList[i].width
			  << " DrawingList.height: " << DrawingList[i].height << " DrawingList.srcx: "
			  << DrawingList[i].srcx << " DrawingList.srcy: " << DrawingList[i].srcy << ")";
			TERROR(s.str().c_str());
		}
		DrawSkin(Surface, DrawingList[i].x + x, DrawingList[i].y + y, DrawingList[i].width,
			DrawingList[i].height, DrawingList[i].skin, DrawingList[i].srcx, DrawingList[i].srcy);
	}
}

// --- TWindow ----------------------------------------------------------------

// 0x49c190, lines 1193-1194. Inserts the child at the front. No try block.
void TWindow::InsertChild(TComponent *Child)
{
	PATELLA_ASSERT(Child!=nullptr);
	PATELLA_ASSERT(Child->Parent==nullptr);
	Child->Next = FirstChild;
	FirstChild = Child;
	Child->Parent = this;
}

// 0x49c430, lines 1206-1207. No try block. The first child's Next is not
// reset.
void TWindow::AppendChild(TComponent *Child)
{
	PATELLA_ASSERT(Child!=nullptr);
	PATELLA_ASSERT(Child->Parent==nullptr);
	if (FirstChild == nullptr) {
		FirstChild = Child;
	} else {
		TComponent *Search = FirstChild;
		while (Search->Next != nullptr)
			Search = Search->Next;
		Search->Next = Child;
		Child->Next = nullptr;
	}
	Child->Parent = this;
}

// 0x49c6f0, line 1231. The child is closed after it is removed, where the
// original's delete ran its destructor (issue #108).
void TWindow::DeleteChild(TComponent *Child)
{
	RemoveChild(Child);
	DeleteComponent(Child);
}

// 0x49c890, lines 1239-1260. The assertions before the try block. A child
// holding the capture stops dragging and releases it through itself (with
// NULL), which reaches this window's ReleaseCapture by its parent.
void TWindow::RemoveChild(TComponent *Child)
{
	PATELLA_ASSERT(Child!=nullptr);
	PATELLA_ASSERT(Child->Parent==this);
	if (Child == Capture) {
		Child->StopDragging();
		Child->ReleaseCapture(nullptr);
	}
	if (Child == FirstChild) {
		FirstChild = Child->Next;
	} else {
		TComponent *Search = FirstChild;
		for (;;) {
			PATELLA_ASSERT(Search!=nullptr);
			if (Search->Next == Child)
				break;
			Search = Search->Next;
		}
		Search->Next = Child->Next;
	}
	Child->Parent = nullptr;
	Child->Next = nullptr;
}

// 0x49cd00, lines 1269-1287. Inserts the child after Previous, at the front
// when Previous is NULL. The first two assertions are before the try block.
void TWindow::InsertChildAfter(TComponent *Previous, TComponent *Child)
{
	PATELLA_ASSERT(Child!=nullptr);
	PATELLA_ASSERT(Child->Parent==nullptr);
	if (Previous == nullptr) {
		InsertChild(Child);
		return;
	}
	TComponent *Search = FirstChild;
	PATELLA_ASSERT(Search!=nullptr);
	while (Search != nullptr && Search != Previous)
		Search = Search->Next;
	PATELLA_ASSERT(Search!=nullptr);
	Child->Parent = this;
	Child->Next = Search->Next;
	Search->Next = Child;
}

// 0x49d200, lines 1295-1347. The first five assertions are before the try
// block. Odd behaviour kept: two children that are not neighbours are
// removed and inserted again after their own predecessors, so they keep
// their places.
void TWindow::SwapChildren(TComponent *Child1, TComponent *Child2)
{
	PATELLA_ASSERT(Child1!=nullptr);
	PATELLA_ASSERT(Child2!=nullptr);
	PATELLA_ASSERT(Child1->Parent==this);
	PATELLA_ASSERT(Child2->Parent==this);
	PATELLA_ASSERT(FirstChild!=nullptr);
	TComponent *Before1 = nullptr;
	TComponent *Search1 = FirstChild;
	while (Search1 != nullptr && Search1 != Child1) {
		Before1 = Search1;
		Search1 = Search1->Next;
	}
	PATELLA_ASSERT(Search1!=nullptr);
	TComponent *Before2 = nullptr;
	TComponent *Search2 = FirstChild;
	while (Search2 != nullptr && Search2 != Child2) {
		Before2 = Search2;
		Search2 = Search2->Next;
	}
	PATELLA_ASSERT(Search2!=nullptr);
	PATELLA_ASSERT(Before1!=Before2);
	TComponent *After2 = Search2->Next;
	if (After2 == Search1) {
		TComponent *After1 = Search1->Next;
		if (FirstChild == Search2)
			FirstChild = Search1;
		else
			Before2->Next = Search1;
		Search1->Next = Search2;
		Search2->Next = After1;
	} else if (Search1->Next == Search2) {
		if (FirstChild == Search1)
			FirstChild = Search2;
		else
			Before1->Next = Search2;
		Search2->Next = Search1;
		Search1->Next = After2;
	} else {
		RemoveChild(Search1);
		RemoveChild(Search2);
		InsertChildAfter(Before1, Search1);
		InsertChildAfter(Before2, Search2);
	}
}

// 0x49da60, line 1389
void TWindow::SetPosition(int _OffsetX, int _OffsetY)
{
	OffsetX = _OffsetX;
	OffsetY = _OffsetY;
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next)
		Child->PositionChanged();
}

// 0x4a2590. Deletes the children. The deleting destructor is 0x4a29e0. After
// the window's Close (issue #108) its children are closed and DeleteChild only
// removes and frees them. A window destroyed without a close is one whose
// constructor threw (its fully made bases are destroyed): then DeleteChild
// closes each child, as the original's destructors did their work there, and an
// exception of it ends the program, as the original's second exception during
// the unwinding did.
TWindow::~TWindow()
{
	while (FirstChild != nullptr)
		DeleteChild(FirstChild);
}

// Issue #108: ~TWindow's order, the first child first. The next child is taken
// before the close (a close does not change the list; the original's
// destructors ran on children already removed from it).
void TWindow::OnClose()
{
	TComponent *Child = FirstChild;
	while (Child != nullptr) {
		TComponent *Next = Child->Next;
		Child->Close();
		Child = Next;
	}
}

// 0x4a2600, line 1408. The window's own drawing list, then the children.
void TWindow::Draw(int Surface, int x, int y)
{
	TComponent::Draw(Surface, x, y);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next)
		Child->Draw(Surface, Child->OffsetX + x, Child->OffsetY + y);
}

// 0x49dc20, line 1424
void TWindow::Arrange()
{
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next)
		Child->Arrange();
}

// 0x49ddc0, line 1448. The mouse functions go to the capture if there is
// one, else to the first child under the point that takes the event, in
// child coordinates.
bool TWindow::MouseMove(int x, int y)
{
	if (Capture != nullptr)
		return Capture->MouseMove(x - Capture->OffsetX, y - Capture->OffsetY);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)
			&& Child->MouseMove(x - Child->OffsetX, y - Child->OffsetY))
			return true;
	}
	return false;
}

// 0x49dff0, line 1472
bool TWindow::LeftButtonDown(int x, int y, int Flags)
{
	if (Capture != nullptr)
		return Capture->LeftButtonDown(x - Capture->OffsetX, y - Capture->OffsetY, Flags);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)
			&& Child->LeftButtonDown(x - Child->OffsetX, y - Child->OffsetY, Flags))
			return true;
	}
	return false;
}

// 0x49e230, line 1496
bool TWindow::LeftButtonUp(int x, int y, int Flags)
{
	if (Capture != nullptr)
		return Capture->LeftButtonUp(x - Capture->OffsetX, y - Capture->OffsetY, Flags);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)
			&& Child->LeftButtonUp(x - Child->OffsetX, y - Child->OffsetY, Flags))
			return true;
	}
	return false;
}

// 0x49e470, line 1520
bool TWindow::LeftButtonDoubleClick(int x, int y, int Flags)
{
	if (Capture != nullptr)
		return Capture->LeftButtonDoubleClick(x - Capture->OffsetX, y - Capture->OffsetY, Flags);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)
			&& Child->LeftButtonDoubleClick(x - Child->OffsetX, y - Child->OffsetY, Flags))
			return true;
	}
	return false;
}

// 0x49e6b0, line 1544
bool TWindow::RightButtonUp(int x, int y, int Flags)
{
	if (Capture != nullptr)
		return Capture->RightButtonUp(x - Capture->OffsetX, y - Capture->OffsetY, Flags);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)
			&& Child->RightButtonUp(x - Child->OffsetX, y - Child->OffsetY, Flags))
			return true;
	}
	return false;
}

// 0x49e8f0, line 1568
bool TWindow::MouseWheel(int x, int y, int Direction)
{
	if (Capture != nullptr)
		return Capture->MouseWheel(x - Capture->OffsetX, y - Capture->OffsetY, Direction);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)
			&& Child->MouseWheel(x - Child->OffsetX, y - Child->OffsetY, Direction))
			return true;
	}
	return false;
}

// 0x49eb30, line 1588. No capture check; only the first child under the
// point gets it.
void TWindow::MouseHover(int x, int y)
{
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)) {
			Child->MouseHover(x - Child->OffsetX, y - Child->OffsetY);
			return;
		}
	}
}

// 0x49ed10, line 1611. No capture check. The child that explains itself
// moves the help area by its offset; without one the help handle is cleared.
bool TWindow::HelpMouseMove(int x, int y)
{
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->Next) {
		if (In(x - Child->OffsetX, 0, Child->Width - 1) && In(y - Child->OffsetY, 0, Child->Height - 1)
			&& Child->HelpMouseMove(x - Child->OffsetX, y - Child->OffsetY)) {
			MoveHelpPosition(Child->OffsetX, Child->OffsetY);
			return true;
		}
	}
	SetHelpHandle(0);
	return false;
}

// 0x49ef10, lines 1633-1638. The assertion is before the try block. The
// parents up to the main window capture this window in turn.
void TWindow::SetCapture(TComponent *Component)
{
	PATELLA_ASSERT(Capture==nullptr);
	Capture = Component;
	if (Parent != nullptr)
		Parent->SetCapture(this);
}

// 0x49f1a0, lines 1648-1653. The assertion is before the try block.
void TWindow::ReleaseCapture(TComponent *Component)
{
	PATELLA_ASSERT(Capture==Component);
	Capture = nullptr;
	if (Parent != nullptr)
		Parent->ReleaseCapture(this);
}

// 0x49f430, line 1668. Does not release the parents' capture.
void TWindow::StopDragging()
{
	if (Capture != nullptr) {
		Capture->StopDragging();
		Capture = nullptr;
	}
}

// 0x49f5d0, lines 1679-1690. Negative coordinates count from the right and
// bottom edges, sizes below 1 are relative to the space left. The
// assertions are before the try block; they name the window's Width and
// Height, not the area (whose name is ours, as FitToChild's).
void TWindow::SetChildArea(TComponent *Child, TRect Area)
{
	PATELLA_ASSERT(Child->Parent==this);
	PATELLA_ASSERT_VALUES(Width>0, Width);
	PATELLA_ASSERT_VALUES(Height>0, Height);
	if (Area.x < 0)
		Area.x = Width + Area.x;
	if (Area.y < 0)
		Area.y = Height + Area.y;
	Child->SetPosition(Area.x, Area.y);
	if (Area.width < 1)
		Area.width = (Width - Area.x) + Area.width;
	if (Area.height < 1)
		Area.height = (Height - Area.y) + Area.height;
	Child->SetSize(Area.width, Area.height);
}

// 0x49fa40, lines 1700-1708. The assertions are before the try block.
void TWindow::SetChildPosition(TComponent *Child, int x, int y)
{
	PATELLA_ASSERT(Child->Parent==this);
	PATELLA_ASSERT_VALUES(Width>0, Width);
	PATELLA_ASSERT_VALUES(Height>0, Height);
	if (x < 0)
		x = Width + x;
	if (y < 0)
		y = Height + y;
	Child->SetPosition(x, y);
}

// 0x49fe70, lines 1724-1738. Sizes the window to the child placed at
// (Area.x, Area.y), with a negative Area.width and Area.height as margins
// after it, and not below the minimum size. Notify chooses the virtual
// SetSize; otherwise only the size fields change. The assertions are before
// the try block.
void TWindow::FitToChild(TComponent *Child, TRect Area, bool Notify)
{
	PATELLA_ASSERT(Child!=nullptr);
	PATELLA_ASSERT_VALUES(Child->getWidth()>0, Child->getWidth());
	PATELLA_ASSERT_VALUES(Child->getHeight()>0, Child->getHeight());
	int NewWidth = Child->Width + (Area.x > 0 ? Area.x : 0) + (Area.width < 0 ? -Area.width : 0);
	int NewHeight = (Area.y > 0 ? Area.y : 0) + Child->Height + (Area.height < 0 ? -Area.height : 0);
	const int MinimumWidth = getMinimumWidth();
	if (NewWidth < MinimumWidth)
		NewWidth = MinimumWidth;
	const int MinimumHeight = getMinimumHeight();
	if (NewHeight < MinimumHeight)
		NewHeight = MinimumHeight;
	if (Notify)
		SetSize(NewWidth, NewHeight);
	else
		TComponent::SetSize(NewWidth, NewHeight);
}

// 0x4a0300, lines 1772-1794. Moves the children so that their bounding box
// starts at (|left margin|, |top margin|) and sizes the window to it plus
// the right and bottom margins (also taken as absolute values), not below
// the minimum size. Nothing happens without children.
void TWindow::FitToChildren(TRect Margin, bool Notify)
{
	if (FirstChild == nullptr)
		return;
	int LeastX = INT_MAX;
	int LeastY = INT_MAX;
	int Width = 0;
	int Height = 0;
	TComponent *Child;
	for (Child = FirstChild; Child != nullptr; Child = Child->GetNext()) {
		if (Child->OffsetX <= LeastX)
			LeastX = Child->OffsetX;
		if (Child->OffsetY <= LeastY)
			LeastY = Child->OffsetY;
		if (Width <= Child->OffsetX + Child->Width)
			Width = Child->OffsetX + Child->Width;
		if (Height <= Child->Height + Child->OffsetY)
			Height = Child->Height + Child->OffsetY;
	}
	PATELLA_ASSERT_VALUES(Width>0, Width);
	PATELLA_ASSERT_VALUES(Height>0, Height);
	PATELLA_ASSERT_VALUES(LeastX<INT_MAX, LeastX);
	PATELLA_ASSERT_VALUES(LeastY<INT_MAX, LeastY);
	const int dx = abs(Margin.x) - LeastX;
	const int dy = abs(Margin.y) - LeastY;
	Width = Width + abs(Margin.width) + abs(Margin.x);
	Height = Height + abs(Margin.height) + abs(Margin.y);
	for (Child = FirstChild; Child != nullptr; Child = Child->GetNext())
		Child->SetPosition(Child->OffsetX + dx, Child->OffsetY + dy);
	const int MinimumWidth = getMinimumWidth();
	if (Width < MinimumWidth)
		Width = MinimumWidth;
	const int MinimumHeight = getMinimumHeight();
	if (Height < MinimumHeight)
		Height = MinimumHeight;
	if (Notify)
		SetSize(Width, Height);
	else
		TComponent::SetSize(Width, Height);
}

// 0x4a0920, line 1860. Cuts the children to the window. A child that
// sticks out is deleted when Strict is set (or when nothing of it is left),
// else moved and resized to the visible part. The checks against the right
// and bottom edges use the old position.
void TWindow::ClipChildren(bool Strict)
{
	if (FirstChild != nullptr) {
		TComponent *Next = FirstChild;
		while (Next != nullptr) {
			TComponent *Child = Next;
			Next = Child->GetNext();
			int x = Child->OffsetX;
			int NewHeight = Child->Height;
			const int y = Child->OffsetY;
			const int OldWidth = Child->Width;
			bool Changed = x < 0;
			int NewWidth = OldWidth;
			if (Changed) {
				NewWidth = Child->OffsetX + OldWidth;
				x = 0;
			}
			bool Remove = Changed && Strict;
			int NewY = y;
			if (y < 0) {
				Remove = Strict || Remove;
				NewHeight = Child->Height + y;
				NewY = 0;
			}
			Changed = y < 0 || Changed;
			if (Width < Child->OffsetX + OldWidth) {
				if (Strict)
					Remove = true;
				NewWidth = Width - Child->OffsetX;
				Changed = true;
			}
			if (Height < Child->Height + y) {
				if (Strict)
					Remove = true;
				NewHeight = Height - y;
				Changed = true;
			}
			if (NewWidth < 0 || NewHeight < 0 || Remove) {
				DeleteChild(Child);
			} else if (Changed) {
				Child->SetPosition(x, NewY);
				Child->SetSize(NewWidth, NewHeight);
			}
		}
	}
}

// --- TElement -------------------------------------------------------------

// 0x4a0bc0, line 1893. Captures this element in the parent; the argument is
// not used.
void TElement::SetCapture([[maybe_unused]] TComponent *Component)
{
	if (GetParent() != nullptr)
		GetParent()->SetCapture(this);
}

// 0x4a0d60, line 1905
void TElement::ReleaseCapture([[maybe_unused]] TComponent *Component)
{
	if (GetParent() != nullptr)
		GetParent()->ReleaseCapture(this);
}

// --- TCallback --------------------------------------------------------------

// 0x4a0f00, line 1931
void TCallback::Send() const
{
	if (Receiver != nullptr)
		Receiver->HandleEvent(Event);
}

// --- Hotkeys --------------------------------------------------------------

// 0x4a10a0, line 1965. Replaces the callback of a key that is already there.
void patella::TGUI::AddHotkey(int Key, TCallback Callback)
{
	Hotkeys.insert_or_assign(Key, Callback);
}

// 0x4a12a0, lines 1983. The last entry takes the place of the removed one.
void patella::TGUI::RemoveHotkey(int Key)
{
	Hotkeys.erase(Key);
}

// 0x4a1480, line 2004. Called by Control.cpp's KeyPress when no modal window
// is open. Flag 0x200 is ignored and lower-case letters count as upper case.
bool patella::TGUI::CheckHotkey(int Key)
{
	if ((Key & 0x200) != 0)
		Key -= 0x200;
	if ((Key & 0x1ff) > 0x60 && (Key & 0x1ff) < 0x7b)
		Key -= 0x20;
	const auto Hotkey = Hotkeys.find(Key);
	if (Hotkey == Hotkeys.end())
		return false;
	// A copy: the receiver may add or remove hotkeys (the original's Send read the entry before it called).
	const TCallback Callback = Hotkey->second;
	Callback.Send();
	return true;
}

// The functions of the original (issue #107): each calls its member on the one TClient's TGUI.

void AddTimer(TTimer *Timer)
{
	patella::bridge::GUI().AddTimer(Timer);
}

void RemoveTimer(TTimer *Timer)
{
	patella::bridge::GUI().RemoveTimer(Timer);
}

void UpdateTimers()
{
	patella::bridge::GUI().UpdateTimers();
}

void InitHotkeys()
{
	patella::bridge::GUI().InitHotkeys();
}

void ExitHotkeys()
{
	patella::TGUI::ExitHotkeys();
}

void AddHotkey(int Key, TCallback Callback)
{
	patella::bridge::GUI().AddHotkey(Key, Callback);
}

void RemoveHotkey(int Key)
{
	patella::bridge::GUI().RemoveHotkey(Key);
}

bool CheckHotkey(int Key)
{
	return patella::bridge::GUI().CheckHotkey(Key);
}
// NOLINTEND(cppcoreguidelines-owning-memory)
