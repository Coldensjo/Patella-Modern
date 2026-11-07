// MapWindow.cpp: reconstructed from Tibia.exe 7.72 (0x4bf860-0x4ca0df).
//
// The map view (TMapWindow) and its frame (TMapFrame); see MapWindow.h. The
// inline members of TMapWindow (destructor, minimum size, IsFieldVisible,
// SetLightColor) are in the header; their out-of-line copies open the object
// (0x4bf860-0x4bf9ff). The functions follow in line order; TMapFrame::Arrange
// has no line evidence and stays at the end, where the binary has it.
//
// The drawing works on two surfaces: 2 is the map surface (480x352, the
// visible 15x11 fields of the 18x14 map), 4 the cache of 16x12 field tiles.
// Draw composes the levels bottom up; DrawField draws one field's objects,
// creatures and effects through DrawObject/DrawCreature/DrawFieldSprite,
// which put static sprites into the field's cache tile (CacheSprite) and draw
// animated ones directly onto the map surface after the tiles below them were
// blitted (FlushField). Light sources add up in Light[][], which is drawn
// last as shaded rectangles or as darkness tiles of picture 7.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none apart from
// AddLightSource's, which names the object whose flag could not be read.

#include "MapWindow.h"
#include "Communication.h"
#include "Creatures.h"
#include "DialogElements.h"
#include "Help.h"
#include "Inventory.h"
#include "Map.h"
#include "Objects.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Text.h"
#include "Utils.h"
#include "graphics.h"
#include "GUI.h"
#include "common.h"
#include "types/FixedString.h"

#include <cstdint>
#include <array>
#include <cmath>
#include <sstream>
#include <cstddef>
#include <cstring>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

namespace {

// At 0x7152e8, constructed by the static initialiser 0x559fc0 as
// TCallback(NULL, 0) and never used; the only datum of MapWindow.obj besides its copy of the
// text table (Text.h).
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): the binary's global; TCallback's constructor (GUI.cpp) stores two values
TCallback NoCallback(nullptr, 0);

// Limits Value to the range between Bound1 and Bound2, which may come in
// either order. The binary has no copy: it is inlined everywhere (with
// constant bounds only the first branch is left).
inline void Limit(int &Value, int Bound1, int Bound2)
{
	if (Bound1 > Bound2) {
		if (Value < Bound2)
			Value = Bound2;
		if (Value > Bound1)
			Value = Bound1;
	} else {
		if (Value < Bound1)
			Value = Bound1;
		if (Value > Bound2)
			Value = Bound2;
	}
}

} // namespace

// The arrays std::arrays (issue #60). TCacheField has no pointers (the same on x64); TMapWindow has a vtable.
static_assert(sizeof(TCacheField) == 0x8c && offsetof(TCacheField, Sprite) == 0x0c && offsetof(TCacheField, SpriteX) == 0x34
		&& offsetof(TCacheField, SpriteY) == 0x5c,
	"TCacheField's layout");

// A thrown error whose expression text is a bug number, like Map.cpp's
// assertions ("unknown creature party flag", "[bug0001106]").
#define TERROR1B(Message, V1, Bug) throw TMYEXCEPTION(Message, #V1, V1, "", 0, Bug)

// --- TMapWindow ------------------------------------------------------------

// 0x4bfa00, line 130. The light array is cleared before the try block (its
// TRGBColor constructors). The hotkey 0x44e (Ctrl+N) cycles the creature
// info, the hotkey 0x457 (Ctrl+W) removes the oldest talkbox.
TMapWindow::TMapWindow()
{
	HintButton = nullptr;
	MaxLevel = 0;
	AddHotkey(0x44e, TCallback(this, 600));
	AddHotkey(0x457, TCallback(this, 601));
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 12; j++) {
			Cache[i][j].x = i << 5;
			Cache[i][j].y = j << 5;
			Cache[i][j].CachedSprites = 0;
		}
	}
}

// 0x4bfc30, line 152
void TMapWindow::SetSize(int Width, int Height)
{
	SetTalkChanged();
	TWindow::SetSize(Width, Height);
	Arrange();
}

// 0x4bfde0, line 164. The area is the TRect SkinProperties.Data140, copied
// as one 16-byte block (sub esp,0x10 and four stores).
void TMapWindow::Arrange()
{
	if (HintButton != nullptr)
		SetChildArea(HintButton, SkinProperties().Data140);
}

// 0x4c24d0, line 196. Starts dragging the top movable object of the field.
// Always reports the click as handled.
bool TMapWindow::LeftButtonDown(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonDown(x, y, Flags)) {
		int FieldX, FieldY, FieldZ;
		GetField(x, y, &FieldX, &FieldY, &FieldZ);
		int Position;
		const TObject Obj = GetTopMoveObject(FieldX, FieldY, FieldZ, Position);
		if (Obj.Type != 0) {
			int ax = FieldX, ay = FieldY, az = FieldZ;
			RelativeToAbsolute(&ax, &ay, &az);
			SetDragObject(Obj, Position, ax, ay, az);
		}
	}
	return true;
}

// 0x4c2710, lines 219-272. Action state 7: use the object chosen before
// with the top multi-use object of the field (or the creature on it);
// 8: offer the trade object to the player under the mouse; 6: drop the
// dragged object here (a cumulative pile asks for the count unless Shift
// (0x400) is held or both buttons are released, 0x200); otherwise a click
// without both buttons walks to the field, Flags != 0 is handled as a
// right-button release.
bool TMapWindow::LeftButtonUp(int x, int y, int Flags)
{
	if (TWindow::LeftButtonUp(x, y, Flags))
		return true;
	int FieldX, FieldY, FieldZ;
	GetField(x, y, &FieldX, &FieldY, &FieldZ);
	if (GetActionState() == 7) {
		int Position;
		const TObject Obj = GetTopMultiUseObject(FieldX, FieldY, FieldZ, Position);
		if (Obj.Type != 0) {
			int SrcIndex, SrcX, SrcY, SrcZ;
			const TObject &SrcObj = *GetUseObject(&SrcIndex, &SrcX, &SrcY, &SrcZ);
			PATELLA_ASSERT(SrcObj!=0);
			PATELLA_ASSERT(SrcObj.Type!=CREATURE);
			if (Obj.Type == CREATURE && !IsPlayer(Obj.Data)) {
				SendUseObjectOnCreature(SrcX, SrcY, SrcZ, SrcObj.Type, SrcIndex, Obj.Data);
				return true;
			}
			int DestX = FieldX, DestY = FieldY, DestZ = FieldZ;
			RelativeToAbsolute(&DestX, &DestY, &DestZ);
			SendUseObjectWith(SrcX, SrcY, SrcZ, SrcObj.Type, SrcIndex, DestX, DestY, DestZ, Obj.Type,
				Position);
			return true;
		}
	} else if (GetActionState() == 8) {
		int TradeIndex, TradeX, TradeY, TradeZ;
		const TObject &TradeObject = *GetTradeObject(&TradeIndex, &TradeX, &TradeY, &TradeZ);
		PATELLA_ASSERT(TradeObject!=0);
		const uint32 CreatureID = GetCreatureAt(x, y);
		if (CreatureID != 0 && CreatureID != GetPlayerID() && IsPlayer(CreatureID)) {
			SendRequestTrade(TradeX, TradeY, TradeZ, TradeObject.Type, TradeIndex, CreatureID);
			return true;
		}
		SetStatusMessage(TextSelectPlayerToTrade, false);
		return true;
	} else if (GetActionState() == 6) {
		int DragIndex, DragX, DragY, DragZ;
		const TObject *DragObject = GetDragObject(&DragIndex, &DragX, &DragY, &DragZ);
		const int Type = DragObject->Type;
		const int Count = DragObject->Data;
		int DestX = FieldX, DestY = FieldY, DestZ = FieldZ;
		RelativeToAbsolute(&DestX, &DestY, &DestZ);
		if (DragX != DestX || DragY != DestY || DragZ != DestZ) {
			if (GetObjectFlag(Type, CUMULATIVE) && Count > 1 && (Flags & 0x200) == 0) {
				if (Flags & 0x400) {
					SendMoveObject(DragX, DragY, DragZ, Type, DragIndex, DestX, DestY, DestZ, Count);
					return true;
				}
				SetDragTarget(DestX, DestY, DestZ);
				AddMessage(13, 0, 0);
				return true;
			}
			SendMoveObject(DragX, DragY, DragZ, Type, DragIndex, DestX, DestY, DestZ, 1);
			return true;
		}
	} else {
		if (Flags != 0)
			return RightButtonUp(x, y, Flags);
		int DestX = FieldX, DestY = FieldY, DestZ = GetMapLevel();
		RelativeToAbsolute(&DestX, &DestY, &DestZ);
		switch (SetPlayerDestination(DestX, DestY, DestZ, true, false)) {
		case 1:
			SendWalk();
			break;
		case 2:
			SendStop();
			return true;
		default:
			break;
		}
	}
	return true;
}

// 0x4c2e50, lines 335-368. A field the player cannot see gives "You can't
// see anything." (fields next to him always count as visible). Otherwise the
// look and use objects of the field and the creature under the mouse
// become the trade object, use object and trade info (Status.cpp). With the
// classic control, Ctrl+Shift (0x200) looks, Shift (0x400) on an object
// opens the context menu (message 0x34), and a plain click attacks the
// creature (unless the use object has FORCEUSE) or uses the object
// (message 0x2c for a multi-use one). Otherwise 0x800 attacks, 0x200
// looks, 0x400 uses and a plain click opens the context menu.
bool TMapWindow::RightButtonUp(int x, int y, int Flags)
{
	int FieldX, FieldY, FieldZ;
	GetField(x, y, &FieldX, &FieldY, &FieldZ);
	if ((!In(FieldX - 8, -1, 1) || !In(FieldY - 6, -1, 1)) && Light[FieldX][FieldY].R == 0) {
		AddMessageTalkbox(TALK_INFO_MESSAGE, "You can't see anything.");
		return true;
	}
	SetTradeObject(TObject(), 0, 0, 0, 0);
	SetUseObject(TObject(), 0, 0, 0, 0);
	SetTradeInfo(0);
	int Position;
	TObject Obj = GetTopLookObject(FieldX, FieldY, FieldZ, Position);
	if (Obj.Type != 0) {
		int ax = FieldX, ay = FieldY, az = FieldZ;
		RelativeToAbsolute(&ax, &ay, &az);
		SetTradeObject(Obj, Position, ax, ay, az);
	}
	Obj = GetTopUseObject(FieldX, FieldY, FieldZ, Position);
	if (Obj.Type != 0) {
		int ax = FieldX, ay = FieldY, az = FieldZ;
		RelativeToAbsolute(&ax, &ay, &az);
		SetUseObject(Obj, Position, ax, ay, az);
	}
	const uint32 CreatureID = GetCreatureAt(x, y);
	if (CreatureID != 0)
		SetTradeInfo(CreatureID);
	int Index, ox, oy, oz;
	if (GetClassicControl()) {
		if (Flags & 0x200) {
			const TObject *LookObj = GetTradeObject(&Index, &ox, &oy, &oz);
			if (LookObj->Type != 0)
				SendLook(ox, oy, oz, LookObj->Type, Index);
			return true;
		}
		if ((Flags & 0x400) && GetTradeObject(&Index, &ox, &oy, &oz)->Type != 0) {
			AddMessage(0x34, 0, 0);
			return true;
		}
		const uint32 Target = GetTradeInfo();
		TObject &UseObj = *GetUseObject(&Index, &ox, &oy, &oz);
		if (Target != 0 && Target != GetPlayerID() && UseObj.Type != 0 && !GetObjectFlag(UseObj, FORCEUSE)) {
			SetAttackID(Target != GetAttackID() ? Target : 0);
			SendAttack(GetAttackID());
			return true;
		}
		if (UseObj.Type == 0)
			return true;
		PATELLA_ASSERT(UseObj!=CREATURE);
		if (GetObjectFlag(UseObj, MULTIUSE)) {
			AddMessage(0x2c, 0, 0);
			return true;
		}
		SendUseObject(ox, oy, oz, UseObj.Type, Index, GetFreeContainer());
		return true;
	} else {
		if (Flags & 0x800) {
			const uint32 Target = GetTradeInfo();
			if (Target == 0 || Target == GetPlayerID())
				return true;
			SetAttackID(Target != GetAttackID() ? Target : 0);
			SendAttack(GetAttackID());
			return true;
		}
		if (Flags & 0x200) {
			const TObject *LookObj = GetTradeObject(&Index, &ox, &oy, &oz);
			if (LookObj->Type != 0)
				SendLook(ox, oy, oz, LookObj->Type, Index);
			return true;
		}
		if (Flags & 0x400) {
			TObject &UseObj = *GetUseObject(&Index, &ox, &oy, &oz);
			if (UseObj.Type == 0)
				return true;
			PATELLA_ASSERT(UseObj!=CREATURE);
			if (GetObjectFlag(UseObj, MULTIUSE)) {
				AddMessage(0x2c, 0, 0);
				return true;
			}
			SendUseObject(ox, oy, oz, UseObj.Type, Index, GetFreeContainer());
			return true;
		}
		if (GetTradeObject(&Index, &ox, &oy, &oz)->Type != 0)
			AddMessage(0x34, 0, 0);
		return true;
	}
}

// 0x4bffa0, lines 382-388. Events 600 and 601 are the hotkeys of the
// constructor, 27 the hint button: it shows the next event help as a message.
// GCC's analyzer (issue #111): the message queue owns the block it gets as an integer (AddMessage; the message's handler deletes it).
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wanalyzer-malloc-leak\"")
void TMapWindow::HandleEvent(int Event)
{
	if (Event == 600)
		SetCreatureInfo((GetCreatureInfo() + 1) % 3);
	if (Event == 601)
		RemoveOldestTalkbox();
	if (Event == 27) {
		DeleteChild(HintButton);
		HintButton = nullptr;
		const int Hint = PopEventHelp();
		PATELLA_ASSERT_VALUES(In(Hint,1,EVENTHELPS), Hint);
		const std::size_t Size = strlen(GetEventHelpText(Hint)) + 1;
		char *Text = new char[Size];
		TStringBuffer(Text, Size).Assign(GetEventHelpText(Hint));
		AddMessage(2, reinterpret_cast<std::intptr_t>(GetEventHelpTitle(Hint)), reinterpret_cast<std::intptr_t>(Text));
	}
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")

// 0x4c3520, lines 417-432. Help mode: the help bubble for the creature or
// object under the mouse (1001 the player, 1000 another creature, the
// object's lens help, 1002 a container), placed over the field.
bool TMapWindow::HelpMouseMove(int x, int y)
{
	int FieldX, FieldY, FieldZ;
	GetField(x, y, &FieldX, &FieldY, &FieldZ);
	if ((!In(FieldX - 8, -1, 1) || !In(FieldY - 6, -1, 1)) && Light[FieldX][FieldY].R == 0)
		return false;
	int Position = 0;
	TObject Obj = GetTopLookObject(FieldX, FieldY, FieldZ, Position);
	int Handle = 0;
	if (Obj.Type == CREATURE && Obj.Data == (int)GetPlayerID())
		Handle = 1001;
	else if (Obj.Type == CREATURE)
		Handle = 1000;
	else if (GetObjectFlag(Obj, LENSHELP))
		Handle = GetObjectLensHelp(Obj.Type);
	else if (GetObjectFlag(Obj, CONTAINER))
		Handle = 1002;
	if (Handle == 0)
		return false;
	const TCreature *Player = GetCreature(GetPlayerID());
	PATELLA_ASSERT(Player!=nullptr);
	// The help position goes into x and y (the binary stores it in the
	// parameters' stack slots, 0x4c372a/0x4c3751 and 0x4c3793/0x4c37bd), so
	// the original's catch wrapper printed it after an error in
	// MoveHelpPosition or SetHelpSize (issue #34).
	if (GetNoStretching()) {
		x = FieldX * 32 - (480 - Width) / 2 - Player->GetDeltaX() - 32;
		y = FieldY * 32 - (352 - Height) / 2 - Player->GetDeltaY() - 32;
		MoveHelpPosition(x, y);
		SetHelpSize(32, 32);
	} else {
		x = (FieldX * 32 - Player->GetDeltaX() - 32) * Width / 480;
		y = (FieldY * 32 - Player->GetDeltaY() - 32) * Height / 352;
		MoveHelpPosition(x, y);
		SetHelpSize(Width * 32 / 480, Height * 32 / 352);
	}
	SetHelpHandle(Handle);
	return true;
}

// 0x4c7580, lines 456-834 (Ghidra split it at 0x4c7750). The blocks with
// their own try (lines 477, 495, 527, 550, 757 with 599 inside, 776, 815
// with 794 inside, 830) follow the binary's exception table. MSVC's analysis (C6262, issue #111): the original's
// buffers on the stack.
PATELLA_MSVC_WARNING(suppress : 6262)
void TMapWindow::Draw(int Surface, int x, int y)
{
	const bool LightEffects = GetLightEffects() && GetSurface()->CanShadeRectangles();
	const bool ShowTexts = GetTextualEffects();
	const int CreatureInfo = GetCreatureInfo();
	ProcessMissileEffects();
	const TCreature *Player = GetCreature(GetPlayerID());
	PATELLA_ASSERT(Player!=nullptr);
	ScrollX = Player->GetDeltaX() + 32;
	ScrollY = Player->GetDeltaY() + 32;
	const int Level = GetMapLevel();
	MaxLevel = 7;

	// The levels drawn end below the first ground above the player's cross
	// of fields (and the diagonal lines from it up the levels), unless it is
	// DONTHIDE.
	{
		TObject Obj;
		for (int FieldX = 7; FieldX < 10; FieldX++) {
			for (int FieldY = 5; FieldY < 8; FieldY++) {
				if ((FieldX == 8 || FieldY == 6) && IsFieldThrowable(FieldX, FieldY, Level)) {
					for (int z = Level + 1; z < 8 && FieldX - z + Level >= 0 && FieldY - z + Level >= 0; z++) {
						if (z - 1 < MaxLevel) {
							Obj = GetFirstObject(FieldX - z + Level, FieldY - z + Level, z);
							if (Obj.Type != 0 && GetObjectFlag(Obj, BANK) && !GetObjectFlag(Obj, DONTHIDE))
								MaxLevel = z - 1;
							Obj = GetFirstObject(FieldX, FieldY, z);
							if (Obj.Type != 0 && z - 1 < MaxLevel
								&& (GetObjectFlag(Obj, BANK) || GetObjectFlag(Obj, BOTTOM))
								&& !GetObjectFlag(Obj, DONTHIDE))
								MaxLevel = z - 1;
						}
					}
				}
			}
		}
	}

	{
		TObject Obj;
		for (int FieldX = 0; FieldX < MAPSIZE_X; FieldX++) {
			for (int FieldY = 0; FieldY < MAPSIZE_Y; FieldY++) {
				CoverLevel[FieldX][FieldY] = 0;
				for (int z = MaxLevel; z > 0; z--) {
					Obj = GetFirstObject(FieldX, FieldY, z);
					if (Obj.Type != 0 && GetObjectFlag(Obj, FULLBANK)) {
						CoverLevel[FieldX][FieldY] = z;
						break;
					}
				}
			}
		}
	}

	// The levels bottom up. Each ground sets the light of its field to the
	// ambient light (none below the surface); a translucent one keeps 4/5 of
	// the light below it. The light below the player's level fades by 1/5.
	{
		TObject Obj;
		NumberOfNames = 0;
		NumberOfTextualEffects = 0;
		GetMapPosition(&PositionX, &PositionY, &PositionZ);
		if (GetSurface()->SurfaceRecreated(2)) {
			for (int i = 0; i < 16; i++) {
				for (int j = 0; j < 12; j++)
					Cache[i][j].CachedSprites = 0;
			}
		}
		for (int i = 0; i < 16; i++) {
			for (int j = 0; j < 12; j++) {
				Cache[i][j].Sprites = 0;
				Cache[i][j].Copied = false;
			}
		}
		for (int z = 0; z <= MaxLevel; z++) {
			if (z != 0 && z == Level) {
				for (int FieldX = 0; FieldX < MAPSIZE_X; FieldX++) {
					for (int FieldY = 0; FieldY < MAPSIZE_Y; FieldY++) {
						TRGBColor &Color = Light[FieldX][FieldY];
						Color.R = Color.R * 4 / 5;
						Color.G = Color.G * 4 / 5;
						Color.B = Color.B * 4 / 5;
					}
				}
			}
			for (int FieldX = 0; FieldX < MAPSIZE_X; FieldX++) {
				for (int FieldY = 0; FieldY < MAPSIZE_Y; FieldY++) {
					Obj = GetFirstObject(FieldX, FieldY, z);
					if (z == 0 || (Obj.Type != 0 && GetObjectFlag(Obj, BANK))) {
						const TRGBColor Old = Light[FieldX][FieldY];
						SetLightColor(FieldX, FieldY, PositionZ <= 7 ? GetAmbientBrightness() : 0);
						if (z > 0 && IsFieldTranslucent(FieldX, FieldY, z)) {
							const int B = Old.B * 4 / 5;
							const int G = Old.G * 4 / 5;
							const int R = Old.R * 4 / 5;
							TRGBColor &Color = Light[FieldX][FieldY];
							Color.R = Color.R > R ? Color.R : R;
							Color.G = Color.G > G ? Color.G : G;
							Color.B = Color.B > B ? Color.B : B;
						}
					}
					if (z == PositionZ + Level - 8 && z < 7 && FieldX > 0 && FieldY > 0
						&& IsFieldTranslucent(FieldX - 1, FieldY - 1, z + 1))
						SetLightColor(FieldX, FieldY, GetAmbientBrightness() - 1);
				}
			}
			for (int FieldX = 0; FieldX < MAPSIZE_X; FieldX++) {
				for (int FieldY = 0; FieldY < MAPSIZE_Y; FieldY++)
					DrawField(FieldX * 32 - ScrollX, FieldY * 32 - ScrollY, FieldX, FieldY, z, 0);
			}
		}
		for (int FieldX = 0; FieldX < MAPSIZE_X; FieldX++) {
			for (int FieldY = 0; FieldY < MAPSIZE_Y; FieldY++)
				FlushField(FieldX, FieldY);
		}
	}

	// The light: rectangles shaded between the field centres, or one
	// darkness tile per field.
	if (LightEffects) {
		std::array<TRect, (MAPSIZE_X + 1) * (MAPSIZE_Y + 1)> Rects;
		std::array<TRGBColor, (MAPSIZE_X + 1) * (MAPSIZE_Y + 1) * 4> Colors;
		for (int FieldY = 0; FieldY <= MAPSIZE_Y; FieldY++) {
			for (int FieldX = 0; FieldX <= MAPSIZE_X; FieldX++) {
				const int i = FieldY * (MAPSIZE_X + 1) + FieldX;
				Rects[i] = TRect(FieldX * 32 - ScrollX - 16, FieldY * 32 - ScrollY - 16, 32, 32);
				int Left = FieldX - 1, Right = FieldX, Top = FieldY - 1, Bottom = FieldY;
				Limit(Left, 0, MAPSIZE_X - 1);
				Limit(Top, 0, MAPSIZE_Y - 1);
				Colors[i * 4] = Light[Left][Top];
				Limit(Right, 0, MAPSIZE_X - 1);
				Colors[i * 4 + 1] = Light[Right][Top];
				Limit(Bottom, 0, MAPSIZE_Y - 1);
				Colors[i * 4 + 2] = Light[Left][Bottom];
				Colors[i * 4 + 3] = Light[Right][Bottom];
			}
		}
		GetSurface()->ShadeRectangles(2, (MAPSIZE_X + 1) * (MAPSIZE_Y + 1), Rects.data(), Colors.data());
	} else {
		for (int FieldX = 0; FieldX < MAPSIZE_X; FieldX++) {
			for (int FieldY = 0; FieldY < MAPSIZE_Y; FieldY++)
				DrawLightMask(FieldX * 32 - ScrollX, FieldY * 32 - ScrollY, Light[FieldX][FieldY].R);
		}
	}

	// The map surface onto the screen, then the names and health bars. A
	// creature hidden by a ground above it gets a grey bar; the colour fades
	// with the light on its field (the player's own light counts with at
	// least 2). The field index is not checked for creatures outside the map
	// (Light[][] is read out of range, as in the binary).
	const int ScreenWidth = Width;
	const int ScreenHeight = Height;
	int BorderX = 0, BorderY = 0;
	if (GetNoStretching()) {
		PATELLA_ASSERT_VALUES(getWidth()<=MAP_WIDTH*32, getWidth());
		PATELLA_ASSERT_VALUES(getHeight()<=MAP_HEIGHT*32, getHeight());
		if (Width >= 480 && Height >= 352) {
			GetSurface()->BlitRect(Surface, x, y, 2, TRect(0, 0, 480, 352));
		} else {
			BorderX = (480 - Width) / 2;
			BorderY = (352 - Height) / 2;
			GetSurface()->BlitRect(Surface, x, y, 2, TRect(BorderX, BorderY, Width, Height));
		}
	} else {
		GetSurface()->StretchBlitRect(Surface, TRect(x, y, ScreenWidth, ScreenHeight), 2,
			TRect(0, 0, 480, 352));
	}
	if (CreatureInfo != 0) {
		for (int i = 0; i < NumberOfNames; i++) {
			TCreature *Creature = GetCreature(Names[i].CreatureID);
			if (Creature == nullptr)
				continue;
			int cx = Creature->RealX, cy = Creature->RealY, cz = Creature->RealZ;
			AbsoluteToRelative(&cx, &cy, &cz);
			bool Visible = false;
			if (In(cx, 0, MAPSIZE_X - 1) && In(cy, 0, MAPSIZE_Y - 1)) {
				Visible = true;
				while (++cz <= MaxLevel) {
					TObject Obj;
					Obj = GetFirstObject(cx, cy, cz);
					if (Obj.Type != 0 && GetObjectFlag(Obj, BANK))
						Visible = false;
				}
			}
			const TRGBColor Color = Visible ? Creature->GetHealthColor() : TRGBColor(192, 192, 192);
			int Brightness = Light[cx][cy].R;
			if (Creature->CreatureID == GetPlayerID()) {
				int Strength = Creature->LightStrength;
				Limit(Strength, 2, 5);
				if (Brightness <= Strength * 51)
					Brightness = Strength * 51;
			}
			Brightness = Brightness / 5;
			const TRGBColor NameColor(Color.R * Brightness / 51, Color.G * Brightness / 51, Color.B * Brightness / 51);
			int nx = Names[i].x, ny = Names[i].y;
			if (GetNoStretching()) {
				nx -= BorderX;
				ny -= BorderY;
			} else {
				nx = Width * nx / 480;
				ny = Height * ny / 352;
			}
			int BarY = ny - 4;
			if (CreatureInfo == 2) {
				int BarX = nx - 14;
				const int TextWidth = GetTextWidth(2, Creature->Name);
				Limit(BarX, 1, Width - 28);
				Limit(BarY, GetFontHeight(2) + 1, Height - GetFontHeight(2) - 5);
				int TextX = BarX - TextWidth / 2 + 14;
				const int TextY = BarY - GetFontHeight(2);
				Limit(TextX, 1, Width - TextWidth - 1);
				GetSurface()->FillRectangle(Surface, TRect(BarX + x, BarY + y, 27, 4), TRGBColor(0, 0, 0));
				GetSurface()->FillRectangle(Surface, TRect(BarX + x + 1, BarY + y + 1, Creature->Health / 4, 2),
					NameColor);
				PrintText(Surface, TextX + x, TextY + y, 2, NameColor, Creature->Name, 0);
			} else {
				int BarX = nx - 14;
				Limit(BarX, 1, Width - 28);
				Limit(BarY, 1, Height - 5);
				GetSurface()->FillRectangle(Surface, TRect(BarX + x, BarY + y, 27, 4), TRGBColor(0, 0, 0));
				GetSurface()->FillRectangle(Surface, TRect(BarX + x + 1, BarY + y + 1, Creature->Health / 4, 2),
					NameColor);
			}
			// The party shield, then the skull (both blink when the skull is
			// about to go); a shield of type 1 shows only while the player is
			// in no party.
			int Icon1 = 0, Icon2 = 0;
			bool Blink = true;
			if (Creature->PARTYFlag > 0) {
				switch (Creature->PARTYFlag) {
				case 1:
					if (!IsPlayerInParty())
						Icon1 = 252;
					break;
				case 2:
					Icon1 = 253;
					break;
				case 3:
					Icon1 = 254;
					break;
				case 4:
					Icon1 = 255;
					break;
				default:
					TERROR1B("unknown creature party flag", Creature->PARTYFlag, "[bug0001106]");
				}
			}
			if (Creature->PKFlag > 0) {
				int Skull;
				switch (Creature->PKFlag) {
				case 1:
					Skull = 244;
					break;
				case 2:
					Skull = 245;
					break;
				case 3:
					Skull = 246;
					break;
				case 4:
					Skull = 247;
					break;
				default:
					TERROR1("unknown creature PK flag", Creature->PKFlag);
				}
				Icon2 = Skull;
				if (Icon1 == 0) {
					Icon1 = Skull;
					Icon2 = 0;
				}
				if (Creature->SkullTime != 0) // a kept skull (issue #62: its 32 bits are negative from 2038 to 2106)
					Blink = GetBlinkState();
			}
			if (Icon1 != 0) {
				const TSkin *Skin = &Skins()[Icon1];
				int IconX = nx - Skin->width + 20;
				int IconY = ny + 1;
				if (Icon2 != 0) {
					Limit(IconX, 1, Width - Skin->width - 14);
					Limit(IconY, 1, Height - Skin->height - 1);
					DrawSkin(Surface, IconX + x, IconY + y, Skin->width, Skin->height, Icon1, 0, 0);
					if (Blink)
						DrawSkin(Surface, IconX + x + 13, IconY + y, Skins()[Icon2].width, Skins()[Icon2].height,
							Icon2, 0, 0);
				} else {
					Limit(IconX, 1, Width - Skin->width - 1);
					Limit(IconY, 1, Height - Skin->height - 1);
					if (Blink)
						DrawSkin(Surface, IconX + x, IconY + y, Skin->width, Skin->height, Icon1, 0, 0);
				}
			}
		}
	}

	// The talkboxes (their positions follow the walk unless they stick to
	// an edge) and the status message.
	if (GetTalkChanged())
		ArrangeTalkboxes();
	const int Talkboxes = GetNumberOfTalkboxes();
	for (int Talkbox = 1; Talkbox <= Talkboxes; Talkbox++) {
		int tx, ty, Alignment, Fixed;
		GetTalkboxRectangle(Talkbox, &tx, &ty, &Alignment, &Fixed);
		if ((Fixed & 1) == 0)
			tx -= GetNoStretching() ? Player->GetDeltaX() : Player->GetDeltaX() * Width / 480;
		if ((Fixed & 2) == 0)
			ty -= GetNoStretching() ? Player->GetDeltaY() : Player->GetDeltaY() * Height / 352;
		const int Lines = GetTalkboxLines(Talkbox);
		TRGBColor Color;
		GetTalkboxColor(&Color, Talkbox);
		for (int Line = 0; Line < Lines; Line++)
			PrintTextClipped(Surface, tx + x, Line * GetFontLineHeight(2) + ty + y, 2, Color,
				GetTalkboxLine(Talkbox, Line), Alignment, TRect(x, y, ScreenWidth, ScreenHeight));
	}
	if (GetStatusMessage() != nullptr) {
		const int MessageWidth = Width, MessageHeight = Height;
		const int Length = GetFittingTextLength(2, GetStatusMessage(), MessageWidth);
		PrintTextPart(Surface, MessageWidth / 2 + x, MessageHeight - GetFontHeight(2) + y, 2,
			TRGBColor(255, 255, 255), GetStatusMessage(), Length, 1);
	}

	// The animated texts of visible fields and of the player's level.
	{
		TObject Obj;
		if (ShowTexts) {
			for (int i = 0; i < NumberOfTextualEffects; i++) {
				const int FieldX = TextualEffects[i].FieldX, FieldY = TextualEffects[i].FieldY;
				int z = TextualEffects[i].FieldZ;
				bool Visible = false;
				if (In(FieldX, 0, MAPSIZE_X - 1) && In(FieldY, 0, MAPSIZE_Y - 1)) {
					Visible = true;
					while (++z <= MaxLevel) {
						Obj = GetFirstObject(FieldX, FieldY, z);
						if (Obj.Type != 0 && GetObjectFlag(Obj, BANK))
							Visible = false;
					}
				}
				if (!Visible && TextualEffects[i].FieldZ != GetMapLevel())
					continue;
				const int Color = GetTextualEffectColor(TextualEffects[i].EffectNumber);
				const char *Text = GetTextualEffectText(TextualEffects[i].EffectNumber);
				int tx = TextualEffects[i].x, ty = TextualEffects[i].y;
				if (GetNoStretching()) {
					tx -= BorderX;
					ty -= BorderY;
				} else {
					tx = tx * Width / 480;
					ty = ty * Height / 352;
				}
				Limit(tx, GetTextWidth(2, Text) / 2, Width - GetTextWidth(2, Text) / 2);
				Limit(ty, 0, Height - GetFontHeight(2));
				PrintText(Surface, tx + x, ty + y, 2, TRGBColor(Color), Text, 1);
			}
		}
	}

	// The hint button while there is an event help to show.
	if (GetShowNewHints() && GetEventHelp() != 0 && HintButton == nullptr) {
		HintButton = new TBlinkingButton(0, 0, 61, 62, 63, 64, TCallback(this, 27), TextReceiveHint);
		InsertChild(HintButton);
		Arrange();
		if (GetEventHelp() == 1)
			HandleEvent(27);
	} else if (!GetShowNewHints() && HintButton != nullptr) {
		DeleteChild(HintButton);
		HintButton = nullptr;
	}
	TWindow::Draw(Surface, x, y);
}

// 0x4c5520, lines 874-1062. One field at map surface position posx/posy.
// Mode 0 draws everything; Mode 1 (the fields left of and above a lying
// object, drawn again over it) skips the objects below the top ones, and
// both lights and animated texts are only collected in Mode 0. Objects are
// raised by the HEIGHT of the objects below them (at most 24 pixels).
// Creatures are drawn from the 3x3 fields around, those whose figure ends on
// this field, at most 10, sorted from the back; they get the highlight,
// attack and follow frames and their mark.
void TMapWindow::DrawField(int posx, int posy, int FieldX, int FieldY, int FieldZ, int Mode)
{
	int x = FieldX, y = FieldY, z = FieldZ;
	RelativeToAbsolute(&x, &y, &z);
	TObject Obj = GetFirstObject(FieldX, FieldY, FieldZ);
	if (Obj.Type == 0 && FieldZ == 0 && Mode == 0 && IsFieldVisible(FieldX, FieldY, 0))
		DrawFieldSprite(posx, posy, 0, false, 0, 0, nullptr);
	bool Lying = false;
	int Elevation = 0;
	TObject TopObject;
	for (Obj = GetFirstObject(FieldX, FieldY, FieldZ); Obj.Type != 0; Obj = GetNextObject()) {
		if (Mode == 0 && Obj.Type != CREATURE && GetObjectFlag(Obj, LIGHT)) {
			int ShiftX = 0, ShiftY = 0;
			if (GetObjectFlag(Obj, SHIFT)) {
				ShiftX = GetObjectShiftX(Obj.Type);
				ShiftY = GetObjectShiftY(Obj.Type);
			}
			AddLightSource(posx - ShiftX - Elevation + 16, posy - ShiftY - Elevation + 16, FieldZ,
				GetObjectBrightness(Obj.Type), TRGBColor(GetObjectLightColor(Obj.Type)));
		}
		if (GetObjectFlag(Obj, TOP)) {
			TopObject = Obj;
		} else if (Obj.Type != CREATURE && Mode != 1) {
			DrawObject(posx - Elevation, posy - Elevation, FieldZ, Obj, x, y, z);
			if (GetObjectFlag(Obj, LYINGOBJECT))
				Lying = true;
		}
		Elevation += GetObjectFlag(Obj, HEIGHT) ? GetObjectElevation(Obj.Type) : 0;
		if (Elevation > 24)
			Elevation = 24;
	}
	// The catch block prints the object (0x4c72da, line 874: "Obj" and the
	// type at [ebp-0x4c]; issue #34).
	if (Lying) {
		if (FieldX > 0) {
			if (FieldY > 0)
				DrawField(posx - 32, posy - 32, FieldX - 1, FieldY - 1, FieldZ, 1);
			DrawField(posx - 32, posy, FieldX - 1, FieldY, FieldZ, 1);
		}
		if (FieldY > 0)
			DrawField(posx, posy - 32, FieldX, FieldY - 1, FieldZ, 1);
	}
	int NumberOfCreatures = 0;
	std::array<TCreature *, 10> Creatures;
	std::array<int, 10> OffsetX;
	std::array<int, 10> OffsetY;
	std::array<int, 10> ShiftX;
	std::array<int, 10> ShiftY;
	for (int i = FieldX - 1; i <= FieldX + 1; i++) {
		for (int j = FieldY - 1; j <= FieldY + 1; j++) {
			if (i < 0 || j < 0 || i >= MAPSIZE_X || j >= MAPSIZE_Y)
				continue;
			for (Obj = GetFirstObject(i, j, FieldZ); Obj.Type != 0; Obj = GetNextObject()) {
				if (Obj.Type != CREATURE || NumberOfCreatures == 10)
					continue;
				TCreature *Creature = GetCreature(Obj.Data);
				if (Creature == nullptr)
					continue;
				int sx = 0, sy = 0;
				if (Creature->OutfitType != 0) {
					if (GetOutfitFlag(Creature->OutfitType, SHIFT)) {
						sx = GetOutfitShiftX(Creature->OutfitType);
						sy = GetOutfitShiftY(Creature->OutfitType);
					}
				} else if (Creature->Colors[0] != 0) {
					if (GetObjectFlag(Creature->Colors[0], SHIFT)) {
						sx = GetObjectShiftX(Creature->Colors[0]);
						sy = GetObjectShiftY(Creature->Colors[0]);
					}
				} else if (IsPlayer(Creature->CreatureID)) {
					sx = 8;
					sy = 8;
				} else {
					continue;
				}
				const int dx = (i - FieldX) * 32 - sx + Creature->GetDeltaX();
				const int dy = (j - FieldY) * 32 - sy + Creature->GetDeltaY();
				if (dx >= -31 && dx <= 0 && dy >= -31 && dy <= 0) {
					Creatures[NumberOfCreatures] = Creature;
					OffsetY[NumberOfCreatures] = dy;
					OffsetX[NumberOfCreatures] = dx;
					ShiftX[NumberOfCreatures] = sx;
					ShiftY[NumberOfCreatures] = sy;
					NumberOfCreatures++;
				}
			}
		}
	}
	for (int i = 0; i < NumberOfCreatures - 1; i++) {
		for (int j = i + 1; j < NumberOfCreatures; j++) {
			if (OffsetY[i] > OffsetY[j] || (OffsetY[i] == OffsetY[j] && OffsetX[i] >= OffsetX[j])) {
				TCreature *Creature = Creatures[i];
				Creatures[i] = Creatures[j];
				Creatures[j] = Creature;
				int Swap = OffsetX[i];
				OffsetX[i] = OffsetX[j];
				OffsetX[j] = Swap;
				Swap = OffsetY[i];
				OffsetY[i] = OffsetY[j];
				OffsetY[j] = Swap;
				Swap = ShiftX[i];
				ShiftX[i] = ShiftX[j];
				ShiftX[j] = Swap;
				Swap = ShiftY[i];
				ShiftY[i] = ShiftY[j];
				ShiftY[j] = Swap;
			}
		}
	}
	int i;
	for (i = 0; i < NumberOfCreatures; i++) {
		TRGBColor Color;
		if (Creatures[i]->CreatureID == GetHighlightID())
			Color = SkinProperties().HighlightColor;
		if (Creatures[i]->CreatureID == GetAttackID())
			Color = Creatures[i]->CreatureID == GetHighlightID() ? SkinProperties().HighlightedAttackColor
																 : SkinProperties().AttackColor;
		if (Creatures[i]->CreatureID == GetFollowID())
			Color = Creatures[i]->CreatureID == GetHighlightID() ? SkinProperties().HighlightedFollowColor
																 : SkinProperties().FollowColor;
		const int ox = OffsetX[i], oy = OffsetY[i];
		if (Color != TRGBColor(0, 0, 0)) {
			const int cx = (ScrollX - Elevation + ox + posx) / 32, cy = (ScrollY - Elevation + oy + posy) / 32;
			FlushField(cx, cy);
			FlushField(cx + 1, cy);
			FlushField(cx, cy + 1);
			FlushField(cx + 1, cy + 1);
			GetSurface()->DrawFrameClipped(2, TRect(ox - Elevation + posx, oy - Elevation + posy, 32, 32), 2,
				Color, TRect(0, 0, 480, 352));
		}
		if (Creatures[i]->IsMarked()) {
			const int cx = (ScrollX - Elevation + ox + posx) / 32, cy = (ScrollY - Elevation + oy + posy) / 32;
			FlushField(cx, cy);
			FlushField(cx + 1, cy);
			FlushField(cx, cy + 1);
			FlushField(cx + 1, cy + 1);
			const int Margin = Color == TRGBColor(0, 0, 0) ? 0 : 2;
			GetSurface()->DrawFrameClipped(2,
				TRect(Margin + ox - Elevation + posx, Margin - Elevation + oy + posy,
					(16 - Margin) * 2, (16 - Margin) * 2),
				2, TRGBColor(Creatures[i]->Mark), TRect(0, 0, 480, 352));
		}
		const int OutfitType = Creatures[i]->OutfitType;
		if (OutfitType == 0) {
			if (Creatures[i]->Colors[0] == 0) {
				// An invisible creature: the sparkles of effect 13.
				const int SpriteNumber = GetEffectSpriteNumber(13, 1, 1, 1, GetActionNumber() / 3 % 3 + 2);
				const int sx = ox + ShiftX[i], sy = oy + ShiftY[i];
				const int cx = (ScrollX - Elevation + sx + posx - 8) / 32, cy = (ScrollY - Elevation + sy + posy - 8) / 32;
				FlushField(cx, cy);
				FlushField(cx + 1, cy);
				FlushField(cx, cy + 1);
				FlushField(cx + 1, cy + 1);
				DrawSprite(2, sx - Elevation + posx - 8, sy - Elevation + posy - 8, SpriteNumber);
			} else {
				// A creature shown as an object.
				const TObject Shape = {Creatures[i]->Colors[0], 0, 0};
				DrawObject(ShiftX[i] - Elevation + ox + posx, ShiftY[i] - Elevation + oy + posy, FieldZ, Shape, 0, 0, 0);
			}
		} else {
			DrawCreature(ShiftX[i] - Elevation + ox + posx, ShiftY[i] - Elevation + oy + posy, FieldZ, Creatures[i]);
		}
		if (Mode == 0) {
			if ((IsOpponent(Creatures[i]) || Creatures[i]->CreatureID == GetPlayerID()) && FieldZ == GetMapLevel()) {
				Names[NumberOfNames].CreatureID = Creatures[i]->CreatureID;
				Names[NumberOfNames].x = ox - Elevation + posx + 16;
				Names[NumberOfNames].y = oy - Elevation + posy;
				NumberOfNames++;
			}
			const int lx = ox - Elevation + posx + 16, ly = oy - Elevation + posy + 16;
			AddLightSource(lx, ly, FieldZ, Creatures[i]->LightStrength, TRGBColor(Creatures[i]->LightColor));
			if (OutfitType != 0 && GetOutfitFlag(OutfitType, LIGHT))
				AddLightSource(lx, ly, FieldZ, GetOutfitBrightness(OutfitType), TRGBColor(GetOutfitLightColor(OutfitType)));
			if (Creatures[i]->CreatureID == GetPlayerID() && Creatures[i]->LightStrength < 2)
				AddLightSource(lx, ly, FieldZ, 2, TRGBColor(255, 255, 255));
		}
	}
	for (const int EffectNumber : TMapEffects(FieldX, FieldY, FieldZ)) {
		if (EffectNumber < 0 || EffectNumber > EFFECTS - 1) {
			std::stringstream s;
			s << "EffectNumber: " << EffectNumber << " X: " << FieldX << " Y: " << FieldY << " Z: " << FieldZ;
			const TCreature *Player = GetCreature(GetPlayerID());
			if (Player != nullptr)
				s << " PlayerX: " << Player->RealX << " PlayerY: " << Player->RealY << " PlayerZ: " << Player->RealZ;
			throw TMYEXCEPTION("effect number out of range [bug0000913]", "", 0, "", 0, s.str());
		}
		if (GetEffectStatus(EffectNumber) == 1) {
			const int Type = GetGraphicalEffectType(EffectNumber);
			if (Type != -1) {
				const int Phase = GetGraphicalEffectPhase(EffectNumber);
				for (int X = 1; X <= GetEffectWidth(Type); X++) {
					for (int Y = 1; Y <= GetEffectHeight(Type); Y++) {
						for (int Layer = 1; Layer <= GetEffectLayers(Type); Layer++) {
							const int SpriteNumber = GetEffectSpriteNumber(Type, X, Y, Layer, Phase);
							if (SpriteNumber > 0)
								DrawFieldSprite(posx - X * 32 - Elevation + 32, posy - Y * 32 - Elevation + 32, FieldZ,
									false, SpriteNumber, 0, nullptr);
						}
					}
				}
				// The light grows and fades with the phases, up to the
				// effect's brightness. The binary passes the level and the
				// brightness the wrong way round (the missile light below
				// has them right): the level becomes the radius.
				if (Mode == 0 && GetEffectFlag(Type, LIGHT)) {
					const int Rest = GetEffectPhases(Type) - Phase + 1;
					int Brightness = Phase < Rest ? Phase : Rest;
					const int Maximum = GetEffectBrightness(Type);
					if (Brightness >= Maximum)
						Brightness = Maximum;
					AddLightSource(posx - Elevation + 16, posy - Elevation + 16, Brightness, FieldZ,
						TRGBColor(GetEffectLightColor(Type)));
				}
			}
		}
		if (GetEffectStatus(EffectNumber) == 2) {
			if (Mode != 0)
				continue;
			TTextualEffectPosition &Text = TextualEffects[NumberOfTextualEffects];
			Text.EffectNumber = EffectNumber;
			Text.x = posx - Elevation + 16;
			Text.y = posy - GetTextualEffectPhase(EffectNumber) * 2 - Elevation;
			Text.FieldX = FieldX;
			Text.FieldY = FieldY;
			Text.FieldZ = FieldZ;
			NumberOfTextualEffects++;
		}
		if (GetEffectStatus(EffectNumber) == 3) {
			const int Type = GetMissileEffectType(EffectNumber);
			if (Type != -1) {
				for (int X = 1; X <= GetMissileWidth(Type); X++) {
					for (int Y = 1; Y <= GetMissileHeight(Type); Y++) {
						for (int Layer = 1; Layer <= GetMissileLayers(Type); Layer++) {
							const int SpriteNumber = GetMissileSpriteNumber(Type, X, Y, Layer,
								GetMissileEffectDirection(EffectNumber));
							if (SpriteNumber > 0)
								DrawFieldSprite(GetMissileEffectX(EffectNumber) - X * 32 - Elevation + posx + 32,
									GetMissileEffectY(EffectNumber) - Y * 32 - Elevation + posy + 32, FieldZ, true,
									SpriteNumber, 0, nullptr);
						}
					}
				}
				if (Mode == 0 && GetMissileFlag(Type, LIGHT))
					AddLightSource(GetMissileEffectX(EffectNumber) - Elevation + posx + 16,
						GetMissileEffectY(EffectNumber) - Elevation + posy + 16, FieldZ,
						GetMissileBrightness(Type), TRGBColor(GetMissileLightColor(Type)));
			}
		}
	}
	if (TopObject.Type != 0)
		DrawObject(posx, posy, FieldZ, TopObject, x, y, z);
}

// 0x4c4af0, lines 1071-1094. The pattern comes from the absolute position
// (x, y, z), the count, the liquid or the hook; objects with several phases
// are animated and bypass the field cache. A pattern size or phase count of 0
// in Tibia.dat throws a TMyException (PATELLA_DIVISOR, issue #67).
void TMapWindow::DrawObject(int posx, int posy, int Level, TObject Obj, int x, int y, int z)
{
	PATELLA_ASSERT(Obj.Type!=0);
	int PatternX = x % PATELLA_DIVISOR(GetObjectPatternWidth(Obj.Type)) + 1;
	int PatternY = y % PATELLA_DIVISOR(GetObjectPatternHeight(Obj.Type)) + 1;
	const int PatternZ = z % PATELLA_DIVISOR(GetObjectPatternDepth(Obj.Type)) + 1;
	const int Phase = GetActionNumber() / 5 % PATELLA_DIVISOR(GetObjectPhases(Obj.Type)) + 1;
	if (GetObjectFlag(Obj, CUMULATIVE))
		GetCumulativePattern(Obj, &PatternX, &PatternY);
	if (GetObjectFlag(Obj, LIQUIDPOOL) || GetObjectFlag(Obj, LIQUIDCONTAINER))
		GetLiquidPattern(Obj, &PatternX, &PatternY);
	if (GetObjectFlag(Obj, HANG))
		GetHangPattern(Obj, &PatternX, &PatternY);
	int ShiftX = 0, ShiftY = 0;
	if (GetObjectFlag(Obj, SHIFT)) {
		ShiftX = GetObjectShiftX(Obj.Type);
		ShiftY = GetObjectShiftY(Obj.Type);
	}
	for (int X = 1; X <= GetObjectWidth(Obj.Type); X++) {
		for (int Y = 1; Y <= GetObjectHeight(Obj.Type); Y++) {
			for (int Layer = 1; Layer <= GetObjectLayers(Obj.Type); Layer++) {
				const int SpriteNumber = GetObjectSpriteNumber(Obj.Type, X, Y, Layer, PatternX, PatternY, PatternZ, Phase);
				if (SpriteNumber > 0)
					DrawFieldSprite(posx - X * 32 - ShiftX + 32, posy - Y * 32 - ShiftY + 32, Level,
						GetObjectPhases(Obj.Type) > 1, SpriteNumber, 0, nullptr);
			}
		}
	}
}

// 0x4c4f80, lines 1106-1129. Outfits are always animated; the second layer
// is the colour mask (head, body, legs, feet). A phase count of 0 in
// Tibia.dat throws a TMyException (PATELLA_DIVISOR, issue #67).
void TMapWindow::DrawCreature(int posx, int posy, int Level, TCreature *Creature)
{
	PATELLA_ASSERT(Creature!=nullptr);
	PATELLA_ASSERT(Creature->OutfitType!=0);
	const int OutfitType = Creature->OutfitType;
	std::array<TRGBColor, 4> Colors;
	int R, G, B;
	GetOutfitColor(Creature->Colors[0], &R, &G, &B);
	Colors[0] = TRGBColor(R, G, B);
	GetOutfitColor(Creature->Colors[1], &R, &G, &B);
	Colors[1] = TRGBColor(R, G, B);
	GetOutfitColor(Creature->Colors[2], &R, &G, &B);
	Colors[2] = TRGBColor(R, G, B);
	GetOutfitColor(Creature->Colors[3], &R, &G, &B);
	Colors[3] = TRGBColor(R, G, B);
	int ShiftX = 0, ShiftY = 0;
	if (GetOutfitFlag(OutfitType, SHIFT)) {
		ShiftX = GetOutfitShiftX(OutfitType);
		ShiftY = GetOutfitShiftY(OutfitType);
	}
	int Phase;
	if (GetOutfitFlag(OutfitType, ANIMATEALWAYS))
		Phase = GetActionNumber() / 3 % PATELLA_DIVISOR(GetOutfitPhases(OutfitType)) + 1;
	else
		Phase = Creature->GetAnimationPhase(GetOutfitPhases(OutfitType));
	for (int X = 1; X <= GetOutfitWidth(OutfitType); X++) {
		for (int Y = 1; Y <= GetOutfitHeight(OutfitType); Y++) {
			const int SpriteNumber = GetOutfitSpriteNumber(OutfitType, X, Y, 1, Creature->GetDrawDirection(), Phase);
			const int MaskNumber = GetOutfitLayers(OutfitType) == 2
				? GetOutfitSpriteNumber(OutfitType, X, Y, 2, Creature->GetDrawDirection(), Phase)
				: 0;
			if (SpriteNumber > 0)
				DrawFieldSprite(posx - X * 32 - ShiftX + 32, posy - Y * 32 - ShiftY + 32, Level, true, SpriteNumber,
					MaskNumber, Colors.data());
		}
	}
}

// 0x4c02c0, lines 1138-1144. One of the darkness tiles of picture 7 (0
// darkest; brightness 1-27 gets tile 1, from 224 on nothing is drawn).
void TMapWindow::DrawLightMask(int posx, int posy, int Brightness)
{
	PATELLA_ASSERT_VALUES(In(Brightness,0,255), Brightness);
	int Tile;
	if (In(Brightness, 1, 27))
		Tile = 1;
	else
		Tile = Brightness / 28;
	if (Tile <= 7)
		DrawPicturePart(2, posx, posy, 7, Tile << 5, 0, 32, 32);
}

// 0x4c46a0, line 1180. A sprite at map surface position x/y on the fields it
// covers that Level shows on. Static sprites go into the cache tiles; an
// animated (or masked) sprite flushes those tiles and is drawn directly.
void TMapWindow::DrawFieldSprite(int x, int y, int Level, bool Animated, int SpriteNumber, int MaskNumber,
	TRGBColor *Colors)
{
	const int ax = ScrollX + x;
	const int ay = ScrollY + y;
	const int FieldX = (ax + 32) / 32 - 1;
	const int FieldY = (ay + 32) / 32 - 1;
	if (MaskNumber == 0 && !Animated) {
		const int dx = (ax + 32) % 32;
		const int dy = (ay + 32) % 32;
		if (IsFieldVisible(FieldX, FieldY, Level))
			CacheSprite(FieldX, FieldY, SpriteNumber, dx, dy);
		if (ax % 32 != 0 && IsFieldVisible(FieldX + 1, FieldY, Level))
			CacheSprite(FieldX + 1, FieldY, SpriteNumber, dx - 32, dy);
		if (ay % 32 != 0 && IsFieldVisible(FieldX, FieldY + 1, Level))
			CacheSprite(FieldX, FieldY + 1, SpriteNumber, dx, dy - 32);
		if (ax % 32 != 0 && ay % 32 != 0 && IsFieldVisible(FieldX + 1, FieldY + 1, Level))
			CacheSprite(FieldX + 1, FieldY + 1, SpriteNumber, dx - 32, dy - 32);
		return;
	}
	bool Flushed = false;
	if (IsFieldVisible(FieldX, FieldY, Level)) {
		FlushField(FieldX, FieldY);
		Flushed = true;
	}
	if (ax % 32 != 0 && IsFieldVisible(FieldX + 1, FieldY, Level)) {
		FlushField(FieldX + 1, FieldY);
		Flushed = true;
	}
	if (ay % 32 != 0 && IsFieldVisible(FieldX, FieldY + 1, Level)) {
		FlushField(FieldX, FieldY + 1);
		Flushed = true;
	}
	if (ax % 32 != 0 && ay % 32 != 0 && IsFieldVisible(FieldX + 1, FieldY + 1, Level)) {
		FlushField(FieldX + 1, FieldY + 1);
		Flushed = true;
	}
	if (Flushed) {
		if (MaskNumber == 0)
			DrawSprite(2, x, y, SpriteNumber);
		else
			DrawMaskedSprite(2, x, y, SpriteNumber, MaskNumber, Colors);
	}
}

// 0x4c41e0, line 1231. Adds a sprite at offset dx/dy to a field's cache
// tile. A tile that holds the same sprites as in the last frame is not drawn
// again; on the first difference its sprites are drawn again from the start
// (without clearing the tile). A full tile (10 sprites) or one already
// blitted to the map surface gets the sprite drawn directly onto the map
// surface, clipped to the field.
void TMapWindow::CacheSprite(int FieldX, int FieldY, int SpriteNumber, int dx, int dy)
{
	if (!In(FieldX, 0, MAPSIZE_X - 1) || !In(FieldY, 0, MAPSIZE_Y - 1))
		return;
	const int x = FieldX * 32 - ScrollX;
	if (x <= -32)
		return;
	const int y = FieldY * 32 - ScrollY;
	if (y <= -32 || x >= 480 || y >= 352)
		return;
	TCacheField *Field = GetCacheField(FieldX, FieldY);
	if (Field->Sprites == 10)
		FlushField(FieldX, FieldY);
	if (Field->Copied) {
		int ClipX = x, ClipY = y, ClipWidth = 32, ClipHeight = 32;
		if (x < 0) {
			ClipWidth = x + 32;
			ClipX = 0;
		}
		if (y < 0) {
			ClipHeight = y + 32;
			ClipY = 0;
		}
		if (ClipWidth + ClipX >= 480)
			ClipWidth = 480 - ClipX;
		if (ClipY + ClipHeight >= 352)
			ClipHeight = 352 - ClipY;
		if (SpriteNumber != 0)
			DrawSpriteClipped(2, x + dx, y + dy, SpriteNumber, TRect(ClipX, ClipY, ClipWidth, ClipHeight));
		else
			DrawPicturePartClipped(2, x + dx, y + dy, 7, 0, 0, 32, 32, TRect(ClipX, ClipY, ClipWidth, ClipHeight));
		return;
	}
	if (Field->Sprites == Field->CachedSprites) {
		if (SpriteNumber == 0)
			DrawPicturePartClipped(4, Field->x + dx, Field->y + dy, 7, 0, 0, 32, 32, TRect(Field->x, Field->y, 32, 32));
		else
			DrawSpriteClipped(4, Field->x + dx, Field->y + dy, SpriteNumber, TRect(Field->x, Field->y, 32, 32));
		Field->Sprite[Field->CachedSprites] = SpriteNumber;
		Field->SpriteX[Field->CachedSprites] = dx;
		Field->SpriteY[Field->CachedSprites] = dy;
		Field->CachedSprites++;
		// MSVC's analysis (C28020 on the next line, issue #111) takes Sprites as any int: it is below CachedSprites
		// there (they differ, and Sprites only counts up to it), and a field with 10 was flushed above.
		PATELLA_MSVC_WARNING(suppress : 28020)
	} else if (Field->Sprite[Field->Sprites] != SpriteNumber || Field->SpriteX[Field->Sprites] != dx
		|| Field->SpriteY[Field->Sprites] != dy) {
		for (int i = 0; i < Field->Sprites; i++) {
			if (Field->Sprite[i] == 0)
				DrawPicturePartClipped(4, Field->x + Field->SpriteX[i], Field->y + Field->SpriteY[i], 7, 0, 0, 32, 32,
					TRect(Field->x, Field->y, 32, 32));
			else
				DrawSpriteClipped(4, Field->x + Field->SpriteX[i], Field->y + Field->SpriteY[i], Field->Sprite[i],
					TRect(Field->x, Field->y, 32, 32));
		}
		if (SpriteNumber == 0)
			DrawPicturePartClipped(4, Field->x + dx, Field->y + dy, 7, 0, 0, 32, 32, TRect(Field->x, Field->y, 32, 32));
		else
			DrawSpriteClipped(4, Field->x + dx, Field->y + dy, SpriteNumber, TRect(Field->x, Field->y, 32, 32));
		Field->Sprite[Field->Sprites] = SpriteNumber;
		Field->SpriteX[Field->Sprites] = dx;
		Field->SpriteY[Field->Sprites] = dy;
		Field->CachedSprites = Field->Sprites + 1;
	}
	Field->Sprites++;
}

// 0x4c3980, line 1256. Blits a field's cache tile to the map surface, once
// per frame. If the tile holds more sprites than this frame's, they are
// drawn again over it first (without clearing it).
void TMapWindow::FlushField(int FieldX, int FieldY)
{
	if (In(FieldX, 0, MAPSIZE_X - 1) && In(FieldY, 0, MAPSIZE_Y - 1)) {
		const int x = FieldX * 32 - ScrollX;
		if (x > -32) {
			const int y = FieldY * 32 - ScrollY;
			if (y > -32 && x < 480 && y < 352) {
				TCacheField *Field = GetCacheField(FieldX, FieldY);
				if (!Field->Copied) {
					if (Field->Sprites < Field->CachedSprites) {
						for (int i = 0; i < Field->Sprites; i++) {
							if (Field->Sprite[i] == 0)
								DrawPicturePartClipped(4, Field->x + Field->SpriteX[i], Field->y + Field->SpriteY[i], 7, 0,
									0, 32, 32, TRect(Field->x, Field->y, 32, 32));
							else
								DrawSpriteClipped(4, Field->x + Field->SpriteX[i], Field->y + Field->SpriteY[i],
									Field->Sprite[i], TRect(Field->x, Field->y, 32, 32));
						}
						Field->CachedSprites = Field->Sprites;
					}
					Field->Copied = true;
					GetSurface()->BlitRect(2, FieldX * 32 - ScrollX, FieldY * 32 - ScrollY, 4,
						TRect(Field->x, Field->y, 32, 32));
				}
			}
		}
	}
}

// 0x4c0590, lines 1264-1265. No try block. The tiles wrap with the map
// position, so a field keeps its tile while the map scrolls.
TCacheField *TMapWindow::GetCacheField(int FieldX, int FieldY)
{
	PATELLA_ASSERT_VALUES(In(FieldX,0,MAPSIZE_X-1), FieldX);
	PATELLA_ASSERT_VALUES(In(FieldY,0,MAPSIZE_Y-1), FieldY);
	return &Cache[(PositionX + FieldX) % 16][(PositionY + FieldY) % 12];
}

// 0x4c3c80, lines 1338-1347. Lights the fields within Brightness fields of
// map surface position posx/posy (which are moved to field pixels first;
// the catch blocks print the moved values): the colour falls off linearly
// over Brightness*32 pixels (scaled by 1/160), by a further 1/5 on fields
// without a ground, and each field keeps the brightest value of each
// component.
void TMapWindow::AddLightSource(int posx, int posy, int z, int Brightness, TRGBColor Color)
{
	if (Brightness <= 0)
		return;
	posx += ScrollX;
	posy += ScrollY;
	int x = posx / 32 - Brightness;
	while (++x < posx / 32 + Brightness) {
		if (x < 0 || x >= MAPSIZE_X)
			continue;
		for (int y = posy / 32 - Brightness + 1; y < posy / 32 + Brightness; y++) {
			if (y < 0 || y >= MAPSIZE_Y)
				continue;
			const int dx = x * 32 - posx + 16;
			const int dy = y * 32 - posy + 16;
			const int Intensity = Brightness * 32 - (int)sqrt((double)dx * dx + dy * dy);
			int B = Color.B * Intensity / 160;
			int G = Color.G * Intensity / 160;
			int R = Color.R * Intensity / 160;
			TObject Obj = GetMapObject(x, y, z, 0);
			try {
				if (Obj.Type == 0 || !GetObjectFlag(Obj, BANK)) {
					B = B * 4 / 5;
					G = G * 4 / 5;
					R = R * 4 / 5;
				}
			} catch (TMyException &Exception) {
				std::stringstream s;
				s << "failed to get object flag from type " << Obj.Type << " (" << posx << "," << posy << ","
				  << z << "->" << x << "," << y << "," << z << ")";
				throw TMYEXCEPTION(s.str().c_str(), "", 0, "", 0, Exception);
			}
			if (R < 0)
				R = 0;
			else if (R > 255)
				R = 255;
			if (G < 0)
				G = 0;
			else if (G > 255)
				G = 255;
			if (B < 0)
				B = 0;
			else if (B > 255)
				B = 255;
			TRGBColor &FieldLight = Light[x][y];
			FieldLight.R = FieldLight.R > R ? FieldLight.R : R;
			FieldLight.G = FieldLight.G > G ? FieldLight.G : G;
			FieldLight.B = FieldLight.B > B ? FieldLight.B : B;
		}
	}
}

// 0x4c0880, lines 1405-1470. Places the talkboxes: a box of a speaking
// creature over its field (Alignment 0-2 tells which side the creature is
// on), the others in the middle by talk mode. Overlapping boxes are pushed
// apart pixel by pixel within their limits while the total overlap shrinks.
// Fixed bit 1/2: the box sticks to a left/right or top/bottom edge, or has
// no creature (then it does not follow the walk).
void TMapWindow::ArrangeTalkboxes()
{
	std::array<int, TALKBOXES> BoxWidth;
	std::array<int, TALKBOXES> BoxHeight;
	const int Talkboxes = GetNumberOfTalkboxes();
	for (int Talkbox = 1; Talkbox <= Talkboxes; Talkbox++) {
		BoxWidth[Talkbox - 1] = 0;
		const int Lines = GetTalkboxLines(Talkbox);
		for (int Line = 0; Line < Lines; Line++) {
			const int LineWidth = GetTextWidth(2, GetTalkboxLine(Talkbox, Line));
			BoxWidth[Talkbox - 1] = BoxWidth[Talkbox - 1] > LineWidth ? BoxWidth[Talkbox - 1] : LineWidth;
		}
		BoxHeight[Talkbox - 1] = GetFontLineHeight(2) * Lines;
	}
	int StatusHeight;
	if (GetStatusMessage() == nullptr)
		StatusHeight = 0;
	else
		StatusHeight = GetFontHeight(2) + 10;
	const bool NoStretching = GetNoStretching();
	const int BorderX = (480 - Width) / 2;
	const int BorderY = (352 - Height) / 2;
	std::array<int, TALKBOXES> CentreX;
	std::array<int, TALKBOXES> PosX;
	std::array<int, TALKBOXES> PosY;
	std::array<int, TALKBOXES> MinX;
	std::array<int, TALKBOXES> MaxX;
	std::array<int, TALKBOXES> MinY;
	std::array<int, TALKBOXES> MaxY;
	std::array<int, TALKBOXES> Fixed;
	for (int Talkbox = 1; Talkbox <= Talkboxes; Talkbox++) {
		const int i = Talkbox - 1;
		int x, y, z;
		GetTalkboxPosition(Talkbox, &x, &y, &z);
		int CentreY;
		if (x != 0) {
			AbsoluteToRelative(&x, &y, &z);
			int cx;
			if (NoStretching)
				cx = x * 32 - BorderX - 16;
			else
				cx = (x - 1) * Width / 15 + Width / 15 / 2;
			CentreX[i] = cx;
			int cy;
			if (NoStretching)
				cy = y * 32 - BorderY - 32;
			else
				cy = (y - 1) * Height / 11;
			cy -= GetTalkboxLines(Talkbox) * GetFontLineHeight(2) / 2;
			CentreY = cy;
			const int HalfWidth = BoxWidth[i] / 2;
			MinX[i] = HalfWidth + 1 > cx - HalfWidth ? HalfWidth + 1 : cx - HalfWidth;
			MaxX[i] = Width - HalfWidth < cx + HalfWidth ? Width - HalfWidth : cx + HalfWidth;
			const int HalfHeight = BoxHeight[i] / 2;
			MinY[i] = HalfHeight + 1 > cy ? HalfHeight + 1 : cy;
			MaxY[i] = Height - HalfHeight - StatusHeight < cy + BoxHeight[i]
				? Height - HalfHeight - StatusHeight
				: cy + BoxHeight[i];
			Fixed[i] = 0;
		} else {
			const int cx = Width / 2;
			CentreX[i] = cx;
			int cy;
			switch (GetTalkboxMode(Talkbox)) {
			case TALK_INFO_MESSAGE:
				cy = Height / 2 - BoxHeight[i] / 2 - 5;
				break;
			case TALK_GAMEMASTER_BROADCAST:
			case TALK_GAMEMASTER_MESSAGE:
			case TALK_ADMIN_MESSAGE:
			case TALK_EVENT_MESSAGE:
				cy = BoxHeight[i] / 2 + Height / 2 + 5;
				break;
			case TALK_PRIVATE_MESSAGE:
				cy = Height / 4;
				break;
			default:
				TERROR1("unknown talk box mode", GetTalkboxMode(Talkbox));
			}
			CentreY = cy;
			const int HalfWidth = BoxWidth[i] / 2;
			MinX[i] = HalfWidth + 1 > cx - HalfWidth ? HalfWidth + 1 : cx - HalfWidth;
			MaxX[i] = Width - HalfWidth < cx + HalfWidth ? Width - HalfWidth : cx + HalfWidth;
			const int HalfHeight = BoxHeight[i] / 2;
			MinY[i] = HalfHeight + 1 > cy - BoxHeight[i] ? HalfHeight + 1 : cy - BoxHeight[i];
			MaxY[i] = Height - HalfHeight - StatusHeight < BoxHeight[i] + cy
				? Height - HalfHeight - StatusHeight
				: BoxHeight[i] + cy;
			Fixed[i] = 3;
		}
		PosX[i] = CentreX[i];
		PosY[i] = CentreY;
		Limit(PosX[i], MinX[i], MaxX[i]);
		Limit(PosY[i], MinY[i], MaxY[i]);
		Limit(PosX[i], BoxWidth[i] / 2 + 1, Width - BoxWidth[i] / 2);
		Limit(PosY[i], BoxHeight[i] / 2 + 1, Height - BoxHeight[i] / 2 - StatusHeight);
	}
	int Overlap = 0x7fffffff;
	int LastOverlap;
	do {
		LastOverlap = Overlap;
		Overlap = 0;
		std::array<int, TALKBOXES> PushX;
		std::array<int, TALKBOXES> PushY;
		for (int i = 0; i < Talkboxes; i++) {
			PushX[i] = 0;
			PushY[i] = 0;
			for (int j = 0; j < Talkboxes; j++) {
				if (i == j)
					continue;
				const int dx = PosX[j] - PosX[i];
				const int dy = PosY[j] - PosY[i];
				const int Distance = abs(dx);
				const int HalfWidth1 = BoxWidth[i] / 2, HalfWidth2 = BoxWidth[j] / 2;
				if (Distance >= HalfWidth1 + HalfWidth2 + 10)
					continue;
				const int DistanceY = abs(dy);
				const int HalfHeight1 = BoxHeight[i] / 2, HalfHeight2 = BoxHeight[j] / 2;
				if (DistanceY >= HalfHeight1 + HalfHeight2 + 10)
					continue;
				int OverlapX, OverlapY;
				if (HalfWidth2 >= HalfWidth1 + Distance)
					OverlapX = BoxWidth[i] + 20;
				else if (HalfWidth1 >= HalfWidth2 + Distance)
					OverlapX = BoxWidth[j] + 20;
				else
					OverlapX = HalfWidth1 + HalfWidth2 - Distance + 10;
				if (HalfHeight2 >= HalfHeight1 + DistanceY)
					OverlapY = BoxHeight[i] + 20;
				else if (HalfHeight1 >= HalfHeight2 + DistanceY)
					OverlapY = BoxHeight[j] + 20;
				else
					OverlapY = HalfHeight1 + HalfHeight2 - DistanceY + 10;
				Overlap += OverlapY * OverlapX;
				if (dx > 0)
					PushX[i] -= OverlapX;
				if (dx < 0)
					PushX[i] += OverlapX;
				if (dy > 0)
					PushY[i] -= OverlapY;
				if (dy < 0)
					PushY[i] += OverlapY;
				if (dx == 0 && dy == 0)
					PushX[i] += i - j;
			}
		}
		for (int i = 0; i < Talkboxes; i++) {
			if (PushX[i] < 0 && PosX[i] > MinX[i])
				PosX[i]--;
			if (PushX[i] > 0 && PosX[i] < MaxX[i])
				PosX[i]++;
			if (PushY[i] < 0 && PosY[i] > MinY[i])
				PosY[i]--;
			if (PushY[i] > 0 && PosY[i] < MaxY[i])
				PosY[i]++;
		}
	} while (Overlap < LastOverlap);
	for (int Talkbox = 1; Talkbox <= Talkboxes; Talkbox++) {
		const int i = Talkbox - 1;
		const int HalfWidth = BoxWidth[i] / 2;
		if (PosX[i] == HalfWidth + 1 || PosX[i] == Width - HalfWidth)
			Fixed[i] |= 1;
		const int HalfHeight = BoxHeight[i] / 2;
		if (PosY[i] == HalfHeight + 1 || PosY[i] == Height - HalfHeight - StatusHeight)
			Fixed[i] |= 2;
		const int Quarter = BoxWidth[i] / 4;
		const int Top = PosY[i] - HalfHeight;
		if (PosX[i] < CentreX[i] - Quarter)
			SetTalkboxRectangle(Talkbox, PosX[i] + HalfWidth, Top, 2, Fixed[i]);
		else if (PosX[i] > CentreX[i] + Quarter)
			SetTalkboxRectangle(Talkbox, PosX[i] - HalfWidth, Top, 0, Fixed[i]);
		else
			SetTalkboxRectangle(Talkbox, PosX[i], Top, 1, Fixed[i]);
	}
}

// 0x4c12e0, lines 1480-1499. The map field under window position x/y: the
// level is the highest one down from MaxLevel with a ground or a BOTTOM
// object (first or second) on the field; 0 if there is none.
void TMapWindow::GetField(int x, int y, int *FieldX, int *FieldY, int *FieldZ)
{
	const TCreature *Player = GetCreature(GetPlayerID());
	PATELLA_ASSERT(Player!=nullptr);
	if (GetNoStretching()) {
		*FieldX = ((480 - Width) / 2 + Player->GetDeltaX() + x) / 32 + 1;
		*FieldY = ((352 - Height) / 2 + Player->GetDeltaY() + y) / 32 + 1;
	} else {
		*FieldX = (x * 480 / Width + Player->GetDeltaX()) / 32 + 1;
		*FieldY = (y * 352 / Height + Player->GetDeltaY()) / 32 + 1;
	}
	for (*FieldZ = MaxLevel; *FieldZ > 0; (*FieldZ)--) {
		TObject Obj = GetFirstObject(*FieldX, *FieldY, *FieldZ);
		if (Obj.Type != 0) {
			if (GetObjectFlag(Obj, BANK) || GetObjectFlag(Obj, BOTTOM))
				return;
			Obj = GetNextObject();
			if (Obj.Type != 0 && GetObjectFlag(Obj, BOTTOM))
				return;
		}
	}
}

// 0x4c16b0, line 1545. The creature whose figure (its name position, the
// top centre) is under window position x/y, among the creatures with a name
// that no ground hides; 0 if there is none.
uint32 TMapWindow::GetCreatureAt(int x, int y)
{
	const int FieldWidth = Width / 15;
	const int FieldHeight = Height / 11;
	for (int i = 0; i < NumberOfNames; i++) {
		const TCreature *Creature = GetCreature(Names[i].CreatureID);
		if (Creature == nullptr)
			continue;
		int cx = Creature->RealX, cy = Creature->RealY, cz = Creature->RealZ;
		AbsoluteToRelative(&cx, &cy, &cz);
		if (!In(cx, 0, MAPSIZE_X - 1) || !In(cy, 0, MAPSIZE_Y - 1))
			continue;
		bool Visible = true;
		while (++cz <= MaxLevel) {
			TObject Obj = GetFirstObject(cx, cy, cz);
			if (Obj.Type != 0 && GetObjectFlag(Obj, BANK))
				Visible = false;
		}
		if (!Visible)
			continue;
		if (GetNoStretching()) {
			const int dx = (480 - Width) / 2 - Names[i].x + x;
			const int dy = y - (Names[i].y - (352 - Height) / 2);
			if (In(dx, -16, 16) && In(dy, 0, 32))
				return Creature->CreatureID;
		} else {
			const int dx = x - Names[i].x * Width / 480;
			const int dy = y - Names[i].y * Height / 352;
			if (In(dx, -(FieldWidth / 2), FieldWidth / 2) && In(dy, 0, FieldHeight))
				return Creature->CreatureID;
		}
	}
	return 0;
}

// --- TMapFrame -------------------------------------------------------------

// 0x4c1a40, line 1564. Eight drawing list entries: the four border sides
// and the background around them.
TMapFrame::TMapFrame()
{
	MapWindow = new TMapWindow;
	InsertChild(MapWindow);
	DrawingListLength = 8;
	DrawingList.reset(new TDrawing[8]);
}

// 0x4c1c40, line 1584. The map view keeps 15:11 (at most 480 pixels wide
// without stretching) and is centred horizontally; the frame's height
// follows it.
void TMapFrame::SetSize(int Width, int Height)
{
	const int Border = SkinProperties().Data13C;
	const int MaxWidth = (Height - Border * 2) * 15 / 11 + 2;
	ViewWidth = Width - Border * 2 < MaxWidth ? Width - Border * 2 : MaxWidth;
	if (GetNoStretching())
		ViewWidth = ViewWidth < 480 ? ViewWidth : 480;
	ViewHeight = (int)floor(ViewWidth * (11.0f / 15.0f));
	ViewX = (Width - SkinProperties().Data13C * 2 - ViewWidth) / 2 + SkinProperties().Data13C;
	ViewY = SkinProperties().Data13C;
	MapWindow->SetSize(ViewWidth, ViewHeight);
	MapWindow->SetPosition(ViewX, ViewY);
	TWindow::SetSize(Width, ViewHeight + SkinProperties().Data13C * 2);
	Arrange();
}

// 0x4c1ea0, line 1595
int TMapFrame::getMinimumWidth()
{
	return MapWindow->getMinimumWidth() + SkinProperties().Data13C * 2;
}

// 0x4c2040, line 1606
int TMapFrame::getMinimumHeight()
{
	return MapWindow->getMinimumHeight() + SkinProperties().Data13C * 2;
}

// 0x4c21e0. No exception frame. The border (skins 8-11) and skin 7 tiled
// over the rest, its pattern aligned to the frame.
void TMapFrame::Arrange()
{
	DrawingList[0] = TDrawing(9, ViewX - Skins()[11].width, ViewY - Skins()[9].height,
		Skins()[10].width + Skins()[11].width + ViewWidth, Skins()[9].height, 0, 0);
	DrawingList[1] = TDrawing(11, ViewX - Skins()[11].width, ViewY, Skins()[11].width, Skins()[8].height + ViewHeight, 0, 0);
	DrawingList[2] = TDrawing(10, ViewX + ViewWidth, ViewY, Skins()[10].width, Skins()[8].height + ViewHeight, 0, 0);
	DrawingList[3] = TDrawing(8, ViewX, ViewY + ViewHeight, ViewWidth, Skins()[8].height, 0, 0);
	const int Top = ViewY - Skins()[9].height;
	const int Left = ViewX - Skins()[11].width;
	const int Right = Skins()[10].width + ViewX + ViewWidth;
	const int RightWidth = Width - Right;
	const int Bottom = Skins()[8].height + ViewY + ViewHeight;
	DrawingList[4] = TDrawing(7, 0, 0, Width, Top, 0, 0);
	DrawingList[5] = TDrawing(7, 0, Top, Left, Height - Top, 0, Top % Skins()[7].height);
	DrawingList[6] = TDrawing(7, Right, Top, RightWidth, Height - Top, Right % Skins()[7].width, Top % Skins()[7].height);
	DrawingList[7] = TDrawing(7, Left, Bottom, Width - RightWidth - Left, Height - Bottom, Left % Skins()[7].width,
		Bottom % Skins()[7].height);
}
// NOLINTEND(cppcoreguidelines-owning-memory)
