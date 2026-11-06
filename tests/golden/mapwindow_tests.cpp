// difftest.exe's MapWindow test (moved here from tools/difftest/mapwindowtest.cpp, issue #27): MapWindow.cpp
// against the original, as a golden-corpus test (golden.h).
//
// Both sides get one fake world: the map (objects and effects per field,
// throwable and translucent fields, the map position and level), the
// creatures, the talkboxes, the preferences, the event helps and the action
// state are shared fakes; the image's functions and ours (PatchOwn) jump to
// the same code, which knows the calling side from Side. Everything MapWindow
// draws or sends goes to a log per side: the graphics.cpp and GUI.cpp drawing
// functions, the surface slots (the TTestSurface methods below; both sides'
// GetSurface return the same object), the Communication.cpp messages (our
// side's definitions are here), the Status, Talk and Help setters and the
// hotkeys. Objects, outfits, effects and missiles come from the real
// Tibia.dat, loaded on both sides; TCreature's own members run on both sides
// (GetDeltaX/GetDeltaY/GetDrawDirection, which read Creatures.cpp's player
// step, are fakes that return the creature's fields).
//
// The windows: an original TMapWindow made by the image's constructor
// (0x4bfa00) and ours, both in zeroed memory, get the same random operations;
// after each one the return values, the logs and the windows from +0x14 on
// (TComponent's position and size and all of TMapWindow's members) are
// compared. TMapFrame likewise (0x4c1a40), with the members of its map view
// that the constructor leaves uninitialised cleared on both sides.
// Failing assertions are not exercised, nor the hint button's creation (its
// class, DialogElements.cpp's TBlinkingButton, is not reconstructed).
//
// By value (plan 4.3): the original's side exists in the oracle only (every use of it is inside ORIGINAL or
// #if GOLDEN_ORACLE); the runner has our windows alone. The old comparison of the windows' bytes from +0x14
// is a comparison of members: TComponent's OffsetX, OffsetY, Width and Height and every member of TMapWindow
// up to HintButton, which have no pointers and the same bytes (padding included) on x86 and x64; only where
// they start differs (TComponent's pointers before them). Per comparison the corpus holds the original's log
// (text), those members of its window (by their runs of changed bytes, golden::OutputRuns), whether it has a
// hint button, the results of its calls, how many fake calls its call made (the error round draws the
// failing call from them) and what it threw (GOLDEN_ROUND_COMPARE); for TMapFrame its geometry and view,
// its map view's members and its drawing list. Offsets the old test printed or compared (GetCacheField's
// result, the first differing byte) are the x86 offsets on both sides. The runner redirects our functions to
// the fakes with golden::RedirectOwn, for good, as the oracle does.

#include "golden_test.h"

#include "elementfakes.h"
#include "golden_eh.h"
#include "testsurface.h"
#include "golden_patch.h"

#include "Communication.h"
#include "Creatures.h"
#include "GUI.h"
#include "Help.h"
#include "Inventory.h"
#include "Map.h"
#include "MapWindow.h"
#include "Objects.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Utils.h"
#include "graphics.h"

#include <new>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static_assert(sizeof(TCacheField) == 0x8c, "TCacheField is 0x8c bytes");
// TMapWindow's members up to HintButton have no pointers: the same bytes on both targets, at the same
// distances from the first one.
static_assert(offsetof(TMapWindow, HintButton) - offsetof(TMapWindow, NumberOfNames) == 0x92d4 - 0x2c,
	"TMapWindow's members have their x86 layout");
static_assert(offsetof(TMapWindow, Light) - offsetof(TMapWindow, NumberOfNames) == 0x19fc - 0x2c
		&& offsetof(TMapWindow, PositionX) - offsetof(TMapWindow, NumberOfNames) == 0x25cc - 0x2c
		&& offsetof(TMapWindow, Cache) - offsetof(TMapWindow, NumberOfNames) == 0x29d4 - 0x2c,
	"TMapWindow's members at their x86 distances");
static_assert(offsetof(TComponent, Height) - offsetof(TComponent, OffsetX) == 0xc, "TComponent's geometry");
static_assert(offsetof(TMapFrame, ViewHeight) - offsetof(TMapFrame, ViewY) == 0xc, "TMapFrame's view");

// The test's own definitions: in a namespace of their own (the other window tests, linked into the same
// executable, define names of their own at global scope).
namespace MapWindowTest {

enum { OURS = 0,
	ORIGINAL = 1 };
// The sides that have objects: both in the oracle, ours in the runner.
static const int SIDES = 1; // ours alone (the oracle had the original's side too, until issue #94)
static int Side;
static std::string Log[2];

// The error round (issue #34): the FailAt-th call of a fake (every logged
// call and the map, creature and effect getters) throws an assertion of the
// calling side; 0 means none. Ticks counts the calls of one side's call.
static int FailAt, Ticks;
static void Tick()
{
	Ticks++;
	if (FailAt > 0 && --FailAt == 0)
		ThrowSideAssertion(Side == ORIGINAL, -Ticks);
}

static void Logf(const char *Format, ...)
{
	Tick();
	char Buffer[512];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	Log[Side] += Buffer;
}

// Our functions to the fakes, for good (the oracle patches difftest.exe's code with PatchJump, the runner its
// own with golden::RedirectOwn).
static void PatchOwn(void *Function, void *Target)
{
	golden::RedirectOwn(Function, Target);
}

template <class T> static void *Member(T Function) { return *(void **)&Function; }

} // namespace MapWindowTest
using namespace MapWindowTest;

// DialogElements.cpp: never constructed here (GetShowNewHints is false
// whenever the hint button could be created).
TFakeBlinkingButton::TFakeBlinkingButton(int, int, int, int, int, int, TCallback, const char *) { abort(); }

namespace MapWindowTest {

// --- The fake world ----------------------------------------------------------

// Object types by flag, outfits and missiles that can be drawn (SortTypes).
static std::vector<int> Banks, FullBanks, Tops, Lights, Lyings, Heights, Plain, Movables, Cumulatives, Outfits,
	Missiles;
static int LastEffect;

struct TFakeField {
	int Count;
	TObject Objects[6];
	int EffectCount;
	int Effects[3];
	bool Throwable;
	bool Translucent;
};
static TFakeField Fields[MAPSIZE_X][MAPSIZE_Y][MAPSIZE_Z];
static int CenterX = 1000, CenterY = 1000, CenterZ = 7, Level = 7;
static int AmbientBrightness = 200;
static TRGBColor Ambient(200, 180, 160);
static int IterX, IterY, IterZ, IterIndex; // GetFirstObject/GetNextObject
static int BadMapType, NullCreatureAt;     // the error round's inputs (0: none)
static int EffectX, EffectY, EffectZ, EffectIndex;

struct TFakeEffect {
	int Status, Type, Phase, Color, TextPhase, Direction, x, y;
	char Text[12];
};
static TFakeEffect Effects[EFFECTS];

static bool OnMap(int x, int y, int z) { return In(x, 0, MAPSIZE_X - 1) && In(y, 0, MAPSIZE_Y - 1) && In(z, 0, MAPSIZE_Z - 1); }

static void __cdecl FakeGetMapPosition(int *x, int *y, int *z)
{
	*x = CenterX;
	*y = CenterY;
	*z = CenterZ;
}
static int __cdecl FakeGetMapLevel() { return Level; }
static void __cdecl FakeRelativeToAbsolute(int *x, int *y, int *z)
{
	*x += CenterX - 8;
	*y += CenterY - 6;
	*z += 100;
}
static void __cdecl FakeAbsoluteToRelative(int *x, int *y, int *z)
{
	*x -= CenterX - 8;
	*y -= CenterY - 6;
	*z -= 100;
}
static TRGBColor __cdecl FakeGetAmbientColor() { return Ambient; }
static int __cdecl FakeGetAmbientBrightness() { return AmbientBrightness; }
static TObject __cdecl FakeGetFirstObject(int x, int y, int z)
{
	Tick();
	IterX = x;
	IterY = y;
	IterZ = z;
	IterIndex = 0;
	if (!OnMap(x, y, z) || Fields[x][y][z].Count == 0)
		return TObject();
	return Fields[x][y][z].Objects[0];
}
static TObject __cdecl FakeGetNextObject()
{
	Tick();
	IterIndex++;
	if (!OnMap(IterX, IterY, IterZ) || IterIndex >= Fields[IterX][IterY][IterZ].Count)
		return TObject();
	return Fields[IterX][IterY][IterZ].Objects[IterIndex];
}
static TObject __cdecl FakeGetMapObject(int x, int y, int z, int Position)
{
	Logf("GM%d,%d,%d;", x, y, z); // AddLightSource's fields, in order
	if (BadMapType != 0) {        // the error round: a type Tibia.dat does not have
		TObject Bad = {BadMapType, 0, 0};
		return Bad;
	}
	if (!OnMap(x, y, z) || Position >= Fields[x][y][z].Count)
		return TObject();
	return Fields[x][y][z].Objects[Position];
}
static TObject TopObject(int x, int y, int z, int &Position, int Kind)
{
	Tick();
	if (!OnMap(x, y, z) || Fields[x][y][z].Count == 0) {
		Position = -1;
		return TObject();
	}
	Position = (x + y * 3 + z * 5 + Kind) % Fields[x][y][z].Count;
	// On every other field the use target is the creature there, if any
	// (deterministic: both sides must get the same answers).
	if (Kind == 3 && (x + y) % 2 == 0) {
		for (int i = 0; i < Fields[x][y][z].Count; i++) {
			if (Fields[x][y][z].Objects[i].Type == CREATURE)
				Position = i;
		}
	}
	return Fields[x][y][z].Objects[Position];
}
static TObject __cdecl FakeTopMove(int x, int y, int z, int &Position) { return TopObject(x, y, z, Position, 0); }
// The look object of an empty field is a ground: HelpMouseMove asks for the
// flags of whatever it gets, and type 0 fails the assertion in the original
// as well.
static TObject __cdecl FakeTopLook(int x, int y, int z, int &Position)
{
	TObject Obj = TopObject(x, y, z, Position, 1);
	if (Obj.Type == 0) {
		Obj.Type = Banks[0];
		Position = 0;
	}
	return Obj;
}
static TObject __cdecl FakeTopUse(int x, int y, int z, int &Position) { return TopObject(x, y, z, Position, 2); }
static TObject __cdecl FakeTopMultiUse(int x, int y, int z, int &Position) { return TopObject(x, y, z, Position, 3); }
static bool __cdecl FakeThrowable(int x, int y, int z)
{
	Tick();
	return OnMap(x, y, z) && Fields[x][y][z].Throwable;
}
static bool __cdecl FakeTranslucent(int x, int y, int z) { return OnMap(x, y, z) && Fields[x][y][z].Translucent; }
static void __cdecl FakeProcessMissileEffects() { Logf("M;"); }
static int __cdecl FakeGetFirstEffect(int x, int y, int z)
{
	Tick();
	EffectX = x;
	EffectY = y;
	EffectZ = z;
	EffectIndex = 0;
	if (!OnMap(x, y, z) || Fields[x][y][z].EffectCount == 0)
		return -1;
	return Fields[x][y][z].Effects[0];
}
static int __cdecl FakeGetNextEffect()
{
	Tick();
	EffectIndex++;
	if (EffectIndex >= Fields[EffectX][EffectY][EffectZ].EffectCount)
		return -1;
	return Fields[EffectX][EffectY][EffectZ].Effects[EffectIndex];
}
static int __cdecl FakeEffectStatus(int n) { return Effects[n].Status; }
static int __cdecl FakeEffectType(int n) { return Effects[n].Type; }
static int __cdecl FakeEffectPhase(int n) { return Effects[n].Phase; }
static int __cdecl FakeTextColor(int n) { return Effects[n].Color; }
static char *__cdecl FakeText(int n) { return Effects[n].Text; }
static int __cdecl FakeTextPhase(int n) { return Effects[n].TextPhase; }
static int __cdecl FakeMissileDirection(int n) { return Effects[n].Direction; }
static int __cdecl FakeMissileX(int n) { return Effects[n].x; }
static int __cdecl FakeMissileY(int n) { return Effects[n].y; }

// Creatures; World[0] is the player.
static TCreature World[12];
static int WorldSize = 0;
static uint32 HighlightID, AttackID, FollowID;
static bool InParty;
static int Destination;

static TCreature *__cdecl FakeGetCreature(uint32 ID)
{
	Tick();
	if (NullCreatureAt > 0 && --NullCreatureAt == 0)
		return NULL; // the error round: the NullCreatureAt-th call finds nothing
	for (int i = 0; i < WorldSize; i++) {
		if (World[i].CreatureID == ID)
			return &World[i];
	}
	return NULL;
}
static uint32 __cdecl FakeGetPlayerID() { return World[0].CreatureID; }
static bool __cdecl FakeIsPlayer(uint32 ID) { return ID != 0 && ID < 0x40000000; }
static uint32 __cdecl FakeHighlightID() { return HighlightID; }
static uint32 __cdecl FakeAttackID() { return AttackID; }
static uint32 __cdecl FakeFollowID() { return FollowID; }
static void __cdecl FakeSetAttackID(uint32 ID) { Logf("AT%u;", ID); }
static bool __cdecl FakeIsOpponent(TCreature *Creature) { return Creature->Visible; }
static bool __cdecl FakeInParty() { return InParty; }
static int __cdecl FakeSetPlayerDestination(int x, int y, int z, bool Exact, bool Diagonal)
{
	Logf("PD%d,%d,%d,%d,%d;", x, y, z, Exact, Diagonal);
	return Destination;
}
static int GOLDEN_THISCALL FakeDeltaX(GOLDEN_THIS_NAMED(TCreature *This)) { return This->DeltaX; }
static int GOLDEN_THISCALL FakeDeltaY(GOLDEN_THIS_NAMED(TCreature *This)) { return This->DeltaY; }
static int GOLDEN_THISCALL FakeDrawDirection(GOLDEN_THIS_NAMED(TCreature *This)) { return This->Direction; }

// Status.cpp.
static int ActionState, ActionNumber;
static bool Blink;
static TObject DraggedObject, TradedObject, UsedObject;
static int SlotData[3][4];
static int TradeInfo;
static int __cdecl FakeActionState() { return ActionState; }
static int __cdecl FakeActionNumber() { return ActionNumber; }
static bool __cdecl FakeBlinkState() { return Blink; }
static void __cdecl FakeSetDragObject(TObject Obj, int Index, int x, int y, int z) { Logf("SD%d,%d,%d,%d,%d,%d,%d;", Obj.Type, Obj.Data, Obj.Data2, Index, x, y, z); }
static void __cdecl FakeSetTradeObject(TObject Obj, int Index, int x, int y, int z) { Logf("ST%d,%d,%d,%d,%d,%d,%d;", Obj.Type, Obj.Data, Obj.Data2, Index, x, y, z); }
static void __cdecl FakeSetUseObject(TObject Obj, int Index, int x, int y, int z) { Logf("SU%d,%d,%d,%d,%d,%d,%d;", Obj.Type, Obj.Data, Obj.Data2, Index, x, y, z); }
static void __cdecl FakeSetDragTarget(int x, int y, int z) { Logf("SG%d,%d,%d;", x, y, z); }
static void __cdecl FakeSetTradeInfo(int Info) { Logf("SI%d;", Info); }
static int __cdecl FakeGetTradeInfo() { return TradeInfo; }
static TObject *Slot(int n, TObject *Obj, int *Index, int *x, int *y, int *z)
{
	*Index = SlotData[n][0];
	*x = SlotData[n][1];
	*y = SlotData[n][2];
	*z = SlotData[n][3];
	return Obj;
}
static TObject *__cdecl FakeGetDragObject(int *i, int *x, int *y, int *z) { return Slot(0, &DraggedObject, i, x, y, z); }
static TObject *__cdecl FakeGetTradeObject(int *i, int *x, int *y, int *z) { return Slot(1, &TradedObject, i, x, y, z); }
static TObject *__cdecl FakeGetUseObject(int *i, int *x, int *y, int *z) { return Slot(2, &UsedObject, i, x, y, z); }
static void __cdecl FakeAddMessage(int Type, INT_PTR Param1, INT_PTR Param2)
{
	if (Type == 2) {
		Logf("AM2,%s,%s;", (const char *)Param1, (const char *)Param2); // the text is a new[] copy, left alone
	} else {
		Logf("AM%d,%d,%d;", Type, (int)Param1, (int)Param2);
	}
}

// Preferences.cpp.
static bool LightEffects, TextualEffects, NoStretching, ClassicControl, ShowNewHints;
static int CreatureInfo;
static bool __cdecl FakeLightEffects() { return LightEffects; }
static bool __cdecl FakeTextualEffects() { return TextualEffects; }
static int __cdecl FakeCreatureInfo() { return CreatureInfo; }
static void __cdecl FakeSetCreatureInfo(int Value) { Logf("CI%d;", Value); }
static bool __cdecl FakeNoStretching() { return NoStretching; }
static bool __cdecl FakeClassicControl() { return ClassicControl; }
static bool __cdecl FakeShowNewHints()
{
	Tick();
	return ShowNewHints;
}

// Talk.cpp.
struct TFakeTalkbox {
	int Mode, x, y, z, Lines;
	char Line[4][24];
	TRGBColor Color;
	int RectX, RectY, Alignment, Fixed;
};
static TFakeTalkbox Talkboxes[TALKBOXES];
static int NumberOfTalkboxes;
static const char *StatusMessage;
static bool TalkChanged;
static int __cdecl FakeNumberOfTalkboxes() { return NumberOfTalkboxes; }
static int __cdecl FakeTalkboxLines(int n)
{
	Tick();
	return Talkboxes[n - 1].Lines;
}
static const char *__cdecl FakeTalkboxLine(int n, int Line) { return Talkboxes[n - 1].Line[Line]; }
static int __cdecl FakeTalkboxMode(int n) { return Talkboxes[n - 1].Mode; }
static void __cdecl FakeTalkboxPosition(int n, int *x, int *y, int *z)
{
	*x = Talkboxes[n - 1].x;
	*y = Talkboxes[n - 1].y;
	*z = Talkboxes[n - 1].z;
}
static void __cdecl FakeTalkboxRectangle(int n, int *x, int *y, int *Alignment, int *Fixed)
{
	*x = Talkboxes[n - 1].RectX;
	*y = Talkboxes[n - 1].RectY;
	*Alignment = Talkboxes[n - 1].Alignment;
	*Fixed = Talkboxes[n - 1].Fixed;
}
static TRGBColor *__cdecl FakeTalkboxColor(TRGBColor *Color, int n)
{
	*Color = Talkboxes[n - 1].Color;
	return Color;
}
static void __cdecl FakeSetTalkboxRectangle(int n, int x, int y, int Alignment, int Fixed)
{
	Logf("TR%d,%d,%d,%d,%d;", n, x, y, Alignment, Fixed);
}
static const char *__cdecl FakeStatusMessage() { return StatusMessage; }
static void __cdecl FakeSetStatusMessage(const char *Message, bool Permanent) { Logf("SM%s,%d;", Message, Permanent); }
static bool __cdecl FakeTalkChanged() { return TalkChanged; }
static void __cdecl FakeSetTalkChanged() { Logf("TC;"); }
static void __cdecl FakeRemoveOldestTalkbox() { Logf("RO;"); }
static void __cdecl FakeAddMessageTalkbox(int Mode, const char *Text) { Logf("MT%d,%s;", Mode, Text); }

// Help.cpp.
static int EventHelp, NextHint;
static int __cdecl FakePopEventHelp() { return NextHint; }
static int __cdecl FakeGetEventHelp() { return EventHelp; }
static const char *__cdecl FakeEventHelpText(int n)
{
	static char Text[32];
	sprintf(Text, "text of hint %d", n);
	return Text;
}
static const char *__cdecl FakeEventHelpTitle(int n)
{
	static char Title[32];
	sprintf(Title, "title %d", n);
	return Title;
}
static void __cdecl FakeMoveHelpPosition(int dx, int dy) { Logf("HP%d,%d;", dx, dy); }
static void __cdecl FakeSetHelpSize(int Width, int Height) { Logf("HS%d,%d;", Width, Height); }
static void __cdecl FakeSetHelpHandle(int Handle) { Logf("HH%d;", Handle); }

// graphics.cpp, GUI.cpp and Inventory.cpp.
static int FreeContainer;
static void __cdecl FakeDrawSprite(int Surface, int x, int y, int Sprite) { Logf("DS%d,%d,%d,%d;", Surface, x, y, Sprite); }
static void __cdecl FakeDrawSpriteClipped(int Surface, int x, int y, int Sprite, TRect C)
{
	Logf("DC%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Sprite, C.x, C.y, C.width, C.height);
}
static void __cdecl FakeDrawMaskedSprite(int Surface, int x, int y, int Sprite, int Mask, TRGBColor *Colors)
{
	Logf("DM%d,%d,%d,%d,%d", Surface, x, y, Sprite, Mask);
	for (int i = 0; i < 4; i++)
		Logf(",%d/%d/%d", Colors[i].R, Colors[i].G, Colors[i].B);
	Logf(";");
}
static void __cdecl FakeDrawPicturePart(int Surface, int x, int y, int Picture, int sx, int sy, int w, int h)
{
	Logf("DP%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Picture, sx, sy, w, h);
}
static void __cdecl FakeDrawPicturePartClipped(int Surface, int x, int y, int Picture, int sx, int sy, int w, int h,
	TRect C)
{
	Logf("DQ%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, Picture, sx, sy, w, h, C.x, C.y, C.width, C.height);
}
static void __cdecl FakePrintText(int Surface, int x, int y, int Font, TRGBColor Color, const char *Text, int Alignment)
{
	Logf("PT%d,%d,%d,%d,%d/%d/%d,%s,%d;", Surface, x, y, Font, Color.R, Color.G, Color.B, Text, Alignment);
}
static void __cdecl FakePrintTextClipped(int Surface, int x, int y, int Font, TRGBColor Color, const char *Text,
	int Alignment, TRect C)
{
	Logf("PC%d,%d,%d,%d,%d/%d/%d,%s,%d,%d,%d,%d,%d;", Surface, x, y, Font, Color.R, Color.G, Color.B, Text, Alignment,
		C.x, C.y, C.width, C.height);
}
static void __cdecl FakePrintTextPart(int Surface, int x, int y, int Font, TRGBColor Color, const char *Text,
	int Length, int Alignment)
{
	Logf("PP%d,%d,%d,%d,%d/%d/%d,%s,%d,%d;", Surface, x, y, Font, Color.R, Color.G, Color.B, Text, Length, Alignment);
}
static int __cdecl FakeTextWidth(int Font, const char *Text) { return (int)strlen(Text) * 7 + Font; }
static int __cdecl FakeFontHeight(int Font) { return 9 + Font; }
static int __cdecl FakeFontLineHeight(int Font) { return 11 + Font; }
static int __cdecl FakeFittingLength(int Font, const char *Text, int Width)
{
	int Length = Width / (7 + Font);
	return Length < (int)strlen(Text) ? Length : (int)strlen(Text);
}
static void __cdecl FakeDrawSkin(int Surface, int x, int y, int w, int h, int Skin, int sx, int sy)
{
	Logf("SK%d,%d,%d,%d,%d,%d,%d,%d;", Surface, x, y, w, h, Skin, sx, sy);
}
static void *Window[2]; // the windows under test, to tell their callbacks apart
static void __cdecl FakeAddHotkey(int Key, TCallback Callback)
{
	Logf("HK+%d,%d,%d;", Key, Callback.Receiver == Window[Side], Callback.Event);
}
// (~TMapWindow calls it: a destructor of ours must not throw, so this fake
// does not count for the error round's FailAt.)
static void __cdecl FakeRemoveHotkey(int Key)
{
	int Saved = FailAt;
	FailAt = 0;
	Logf("HK-%d;", Key);
	FailAt = Saved;
}
static int __cdecl FakeFreeContainer() { return FreeContainer; }

// Communication.cpp: the fakes of the Send functions MapWindow.cpp calls. In the oracle they are our side's
// definitions (difftest.exe has no Communication.cpp), which the image's jump to; in the runner our
// definitions are client_stubs.cpp's, redirected to these.
static void FakeSendWalk() { Logf("CW;"); }
static void FakeSendStop() { Logf("CS;"); }
static void FakeSendAttack(uint32 CreatureID) { Logf("CA%u;", CreatureID); }
static void FakeSendLook(int x, int y, int z, int Type, int Position) { Logf("CL%d,%d,%d,%d,%d;", x, y, z, Type, Position); }
static void FakeSendMoveObject(int x, int y, int z, int Type, int Position, int dx, int dy, int dz, int Count)
{
	Logf("CM%d,%d,%d,%d,%d,%d,%d,%d,%d;", x, y, z, Type, Position, dx, dy, dz, Count);
}
static void FakeSendRequestTrade(int x, int y, int z, int Type, int Position, uint32 CreatureID)
{
	Logf("CT%d,%d,%d,%d,%d,%u;", x, y, z, Type, Position, CreatureID);
}
static void FakeSendUseObject(int x, int y, int z, int Type, int Position, int Container)
{
	Logf("CU%d,%d,%d,%d,%d,%d;", x, y, z, Type, Position, Container);
}
static void FakeSendUseObjectWith(int x1, int y1, int z1, int Type1, int Position1, int x2, int y2, int z2, int Type2,
	int Position2)
{
	Logf("CX%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", x1, y1, z1, Type1, Position1, x2, y2, z2, Type2, Position2);
}
static void FakeSendUseObjectOnCreature(int x, int y, int z, int Type, int Position, uint32 CreatureID)
{
	Logf("CC%d,%d,%d,%d,%d,%u;", x, y, z, Type, Position, CreatureID);
}

static bool SurfaceLost, CanShade;

} // namespace MapWindowTest
using namespace MapWindowTest;


// The surface slots MapWindow.cpp calls (the TTestSurface methods; the others
// are graphics_tests.cpp's recorders and playertest.cpp's stubs).
void TTestSurface::FillRectangle(int Surface, TRect R, TRGBColor C)
{
	Logf("SF%d,%d,%d,%d,%d,%d/%d/%d;", Surface, R.x, R.y, R.width, R.height, C.R, C.G, C.B);
}
void TTestSurface::DrawFrameClipped(int Surface, TRect R, int LineWidth, TRGBColor C, TRect Clip)
{
	Logf("SR%d,%d,%d,%d,%d,%d,%d/%d/%d,%d,%d,%d,%d;", Surface, R.x, R.y, R.width, R.height, LineWidth, C.R, C.G, C.B,
		Clip.x, Clip.y, Clip.width, Clip.height);
}
void TTestSurface::BlitRect(int To, int x, int y, int From, TRect S)
{
	Logf("SB%d,%d,%d,%d,%d,%d,%d,%d;", To, x, y, From, S.x, S.y, S.width, S.height);
}
void TTestSurface::StretchBlitRect(int To, TRect D, int From, TRect S)
{
	Logf("SS%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;", To, D.x, D.y, D.width, D.height, From, S.x, S.y, S.width, S.height);
}
void TTestSurface::ShadeRectangles(int Surface, int Count, TRect *Rects, TRGBColor *Colors)
{
	Logf("SH%d,%d", Surface, Count);
	for (int i = 0; i < Count; i++)
		Logf(",%d/%d/%d/%d", Rects[i].x, Rects[i].y, Rects[i].width, Rects[i].height);
	for (int i = 0; i < Count * 4; i++)
		Logf(",%d/%d/%d", Colors[i].R, Colors[i].G, Colors[i].B);
	Logf(";");
}
bool TTestSurface::SurfaceRecreated(int Surface)
{
	Logf("S9%d;", Surface);
	return SurfaceLost;
}
bool TTestSurface::CanShadeRectangles() { return CanShade; }

namespace MapWindowTest {

static TSurface *FakeSurface;
static TSurface *__cdecl FakeGetSurface() { return FakeSurface; }

static void PatchWorld()
{
	struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Table[] = {
		{0x4b08c0, (void *)GetMapPosition, (void *)FakeGetMapPosition},
		{0x4b08f0, (void *)GetMapLevel, (void *)FakeGetMapLevel},
		{0x4b0910, (void *)RelativeToAbsolute, (void *)FakeRelativeToAbsolute},
		{0x4b0960, (void *)AbsoluteToRelative, (void *)FakeAbsoluteToRelative},
		{0x4b0af0, (void *)GetAmbientColor, (void *)FakeGetAmbientColor},
		{0x4b5610, (void *)GetAmbientBrightness, (void *)FakeGetAmbientBrightness},
		{0x4b6140, (void *)GetFirstObject, (void *)FakeGetFirstObject},
		{0x4b6510, (void *)GetNextObject, (void *)FakeGetNextObject},
		{0x4b6580, (void *)GetMapObject, (void *)FakeGetMapObject},
		{0x4b6a00, (void *)GetTopMoveObject, (void *)FakeTopMove},
		{0x4b6ee0, (void *)GetTopLookObject, (void *)FakeTopLook},
		{0x4b73a0, (void *)GetTopUseObject, (void *)FakeTopUse},
		{0x4b78d0, (void *)GetTopMultiUseObject, (void *)FakeTopMultiUse},
		{0x4be390, (void *)IsFieldThrowable, (void *)FakeThrowable},
		{0x4be570, (void *)IsFieldTranslucent, (void *)FakeTranslucent},
		{0x4bbd40, (void *)ProcessMissileEffects, (void *)FakeProcessMissileEffects},
		{0x4ba100, (void *)GetFirstEffect, (void *)FakeGetFirstEffect},
		{0x4ba540, (void *)GetNextEffect, (void *)FakeGetNextEffect},
		{0x4b1440, (void *)GetEffectStatus, (void *)FakeEffectStatus},
		{0x4b15a0, (void *)GetGraphicalEffectType, (void *)FakeEffectType},
		{0x4b1860, (void *)GetGraphicalEffectPhase, (void *)FakeEffectPhase},
		{0x4b1b20, (void *)GetTextualEffectColor, (void *)FakeTextColor},
		{0x4b1de0, (void *)GetTextualEffectText, (void *)FakeText},
		{0x4b20a0, (void *)GetTextualEffectPhase, (void *)FakeTextPhase},
		{0x4b2360, (void *)GetMissileEffectType, (void *)FakeEffectType},
		{0x4b2620, (void *)GetMissileEffectDirection, (void *)FakeMissileDirection},
		{0x4b28e0, (void *)GetMissileEffectX, (void *)FakeMissileX},
		{0x4b2bb0, (void *)GetMissileEffectY, (void *)FakeMissileY},
		{0x44c6e0, (void *)GetCreature, (void *)FakeGetCreature},
		{0x44b9c0, (void *)GetPlayerID, (void *)FakeGetPlayerID},
		{0x44b910, (void *)IsPlayer, (void *)FakeIsPlayer},
		{0x44bb70, (void *)GetHighlightID, (void *)FakeHighlightID},
		{0x44bb50, (void *)GetAttackID, (void *)FakeAttackID},
		{0x44bb60, (void *)GetFollowID, (void *)FakeFollowID},
		{0x44bb00, (void *)SetAttackID, (void *)FakeSetAttackID},
		{0x44d740, (void *)IsOpponent, (void *)FakeIsOpponent},
		{0x44d6f0, (void *)IsPlayerInParty, (void *)FakeInParty},
		{0x451400, (void *)SetPlayerDestination, (void *)FakeSetPlayerDestination},
		{0x44bbf0, Member(&TCreature::GetDeltaX), (void *)FakeDeltaX},
		{0x44bc10, Member(&TCreature::GetDeltaY), (void *)FakeDeltaY},
		{0x44bc30, Member(&TCreature::GetDrawDirection), (void *)FakeDrawDirection},
		{0x4e6e30, (void *)GetActionState, (void *)FakeActionState},
		{0x4e6e00, (void *)GetActionNumber, (void *)FakeActionNumber},
		{0x4e7370, (void *)GetBlinkState, (void *)FakeBlinkState},
		{0x4e6f70, (void *)SetDragObject, (void *)FakeSetDragObject},
		{0x4e6fc0, (void *)SetDragTarget, (void *)FakeSetDragTarget},
		{0x4e7030, (void *)GetDragObject, (void *)FakeGetDragObject},
		{0x4e70a0, (void *)SetTradeObject, (void *)FakeSetTradeObject},
		{0x4e70f0, (void *)SetUseObject, (void *)FakeSetUseObject},
		{0x4e7140, (void *)SetTradeInfo, (void *)FakeSetTradeInfo},
		{0x4e7150, (void *)GetTradeObject, (void *)FakeGetTradeObject},
		{0x4e7190, (void *)GetUseObject, (void *)FakeGetUseObject},
		{0x4e71d0, (void *)GetTradeInfo, (void *)FakeGetTradeInfo},
		{0x4e9020, (void *)AddMessage, (void *)FakeAddMessage},
		{0x4db3d0, (void *)GetLightEffects, (void *)FakeLightEffects},
		{0x4db3e0, (void *)GetTextualEffects, (void *)FakeTextualEffects},
		{0x4db3f0, (void *)GetCreatureInfo, (void *)FakeCreatureInfo},
		{0x4db330, (void *)SetCreatureInfo, (void *)FakeSetCreatureInfo},
		{0x4db400, (void *)GetNoStretching, (void *)FakeNoStretching},
		{0x4db640, (void *)GetClassicControl, (void *)FakeClassicControl},
		{0x4db6d0, (void *)GetShowNewHints, (void *)FakeShowNewHints},
		{0x4fd450, (void *)GetNumberOfTalkboxes, (void *)FakeNumberOfTalkboxes},
		{0x4fe620, (void *)GetTalkboxLines, (void *)FakeTalkboxLines},
		{0x4fe7c0, (void *)GetTalkboxLine, (void *)FakeTalkboxLine},
		{0x4fdf10, (void *)GetTalkboxMode, (void *)FakeTalkboxMode},
		{0x4fe0b0, (void *)GetTalkboxPosition, (void *)FakeTalkboxPosition},
		{0x4fe2b0, (void *)GetTalkboxRectangle, (void *)FakeTalkboxRectangle},
		{0x4fe470, (void *)GetTalkboxColor, (void *)FakeTalkboxColor},
		{0x4fef70, (void *)SetTalkboxRectangle, (void *)FakeSetTalkboxRectangle},
		{0x4fd4d0, (void *)GetStatusMessage, (void *)FakeStatusMessage},
		{0x4ff500, (void *)SetStatusMessage, (void *)FakeSetStatusMessage},
		{0x4fd440, (void *)GetTalkChanged, (void *)FakeTalkChanged},
		{0x4fd4c0, (void *)SetTalkChanged, (void *)FakeSetTalkChanged},
		{0x4ff330, (void *)RemoveOldestTalkbox, (void *)FakeRemoveOldestTalkbox},
		{0x505ac0, (void *)AddMessageTalkbox, (void *)FakeAddMessageTalkbox},
		{0x4a2b20, (void *)PopEventHelp, (void *)FakePopEventHelp},
		{0x4a2b00, (void *)GetEventHelp, (void *)FakeGetEventHelp},
		{0x4a3fd0, (void *)GetEventHelpText, (void *)FakeEventHelpText},
		{0x4a3b60, (void *)GetEventHelpTitle, (void *)FakeEventHelpTitle},
		{0x4a2a20, (void *)MoveHelpPosition, (void *)FakeMoveHelpPosition},
		{0x4a2a50, (void *)SetHelpSize, (void *)FakeSetHelpSize},
		{0x4a2a70, (void *)SetHelpHandle, (void *)FakeSetHelpHandle},
		{0x48f870, (void *)DrawSprite, (void *)FakeDrawSprite},
		{0x48fa60, (void *)DrawSpriteClipped, (void *)FakeDrawSpriteClipped},
		{0x4900d0, (void *)DrawMaskedSprite, (void *)FakeDrawMaskedSprite},
		{0x490c60, (void *)DrawPicturePart, (void *)FakeDrawPicturePart},
		{0x4910e0, (void *)DrawPicturePartClipped, (void *)FakeDrawPicturePartClipped},
		{0x493af0, (void *)PrintText, (void *)FakePrintText},
		{0x4934e0, (void *)PrintTextClipped, (void *)FakePrintTextClipped},
		{0x492e30, (void *)PrintTextPart, (void *)FakePrintTextPart},
		{0x494570, (void *)GetTextWidth, (void *)FakeTextWidth},
		{0x491710, (void *)GetFontHeight, (void *)FakeFontHeight},
		{0x48b430, (void *)GetFontLineHeight, (void *)FakeFontLineHeight},
		{0x491f50, (void *)GetFittingTextLength, (void *)FakeFittingLength},
		{0x497a00, (void *)DrawSkin, (void *)FakeDrawSkin},
		{0x4a10a0, (void *)AddHotkey, (void *)FakeAddHotkey},
		{0x4a12a0, (void *)RemoveHotkey, (void *)FakeRemoveHotkey},
		{0x4a6cf0, (void *)GetFreeContainer, (void *)FakeFreeContainer},
		{0x4eaa90, (void *)GetSurface, (void *)FakeGetSurface},
	};
	for (auto &T : Table) {
		ORIGINAL(PatchJump(T.Image, T.Fake));
		PatchOwn(T.Ours, T.Fake);
	}
	// The image's Send functions jump to ours (this file's, the oracle); the runner's (client_stubs.cpp's)
	// to the fakes.
	static const struct {
		uint32 Image;
		void *Ours;
		void *Fake;
	} Sends[] = {
		{0x414700, (void *)SendWalk, (void *)FakeSendWalk},
		{0x414a10, (void *)SendStop, (void *)FakeSendStop},
		{0x408080, (void *)SendAttack, (void *)FakeSendAttack},
		{0x4065e0, (void *)SendLook, (void *)FakeSendLook},
		{0x404db0, (void *)SendMoveObject, (void *)FakeSendMoveObject},
		{0x405000, (void *)SendRequestTrade, (void *)FakeSendRequestTrade},
		{0x4056f0, (void *)SendUseObject, (void *)FakeSendUseObject},
		{0x4058f0, (void *)SendUseObjectWith, (void *)FakeSendUseObjectWith},
		{0x405b20, (void *)SendUseObjectOnCreature, (void *)FakeSendUseObjectOnCreature},
	};
	for (auto &S : Sends) {
		PatchOwn(S.Ours, S.Fake);
	}
}

// --- Random worlds -----------------------------------------------------------


static int Pick(const std::vector<int> &List) { return List[Random() % List.size()]; }

// TRGBColor(Random() % Max, Random() % Max, Random() % Max) with its calls in the x86 oracle's order (issue
// #55: the constructor's arguments right to left).
static TRGBColor RandomColor(uint32 Max)
{
	int B = (int)(Random() % Max);
	int G = (int)(Random() % Max);
	int R = (int)(Random() % Max);
	return TRGBColor(R, G, B);
}

static void SortTypes()
{
	for (int Type = 100; Type <= ObjectProperties->high(); Type++) {
		if (GetObjectFlag(Type, FULLBANK))
			FullBanks.push_back(Type);
		if (GetObjectFlag(Type, BANK))
			Banks.push_back(Type);
		else if (GetObjectFlag(Type, TOP))
			Tops.push_back(Type);
		else if (GetObjectFlag(Type, LIGHT))
			Lights.push_back(Type);
		else if (GetObjectFlag(Type, LYINGOBJECT))
			Lyings.push_back(Type);
		else if (GetObjectFlag(Type, HEIGHT))
			Heights.push_back(Type);
		else
			Plain.push_back(Type);
		if (!GetObjectFlag(Type, BANK) && !GetObjectFlag(Type, UNMOVE))
			Movables.push_back(Type);
		if (GetObjectFlag(Type, CUMULATIVE))
			Cumulatives.push_back(Type);
	}
	// Outfits with the four directions, missiles with the 3x3 directions
	// (GetOutfitSpriteNumber and GetMissileSpriteNumber assert them).
	for (int i = 1; i <= OutfitProperties->high(); i++) {
		if ((*OutfitProperties)(i).PatternWidth >= 4)
			Outfits.push_back(i);
	}
	for (int i = 1; i <= MissileProperties->high(); i++) {
		if ((*MissileProperties)(i).PatternWidth >= 3 && (*MissileProperties)(i).PatternHeight >= 3)
			Missiles.push_back(i);
	}
	LastEffect = EffectProperties->high();
}

static TObject RandomObject()
{
	int r = Random() % 12;
	const std::vector<int> &List = r == 0 ? Tops : r == 1 ? Lights
		: r == 2                                          ? Lyings
		: r == 3                                          ? Heights
		: r == 4 && !FullBanks.empty()                    ? FullBanks
														  : Plain;
	TObject Obj = {Pick(List), (int)(Random() % 100), (int)(Random() % 3 == 0 ? HOOKSOUTH + Random() % 2 : Random() % 4)};
	return Obj;
}

static void RandomCreature(int i, int fx, int fy, int fz)
{
	TCreature &C = World[i];
	memset(&C, 0, sizeof(C));
	C.CreatureID = i == 0 ? 0x1000 : (Random() % 2 ? 0x2000 + i : 0x40000000 + i);
	sprintf(C.Name, "%s %d", Random() % 2 ? "Rat" : "Sir Lancelot", i);
	C.RealX = CenterX - 8 + fx;
	C.RealY = CenterY - 6 + fy;
	C.RealZ = fz + 100;
	C.DeltaX = Random() % 3 == 0 ? (int)(Random() % 64) - 32 : 0;
	C.DeltaY = Random() % 3 == 0 ? (int)(Random() % 64) - 32 : 0;
	C.Direction = Random() % 4;
	C.StepEnd = Random() % 200000;
	int Kind = Random() % 8;
	if (Kind > 1)
		C.OutfitType = Pick(Outfits);
	else if (Kind == 1)
		C.Colors[0] = Pick(Plain);
	if (C.OutfitType != 0) {
		for (int k = 0; k < 4; k++)
			C.Colors[k] = Random() % 133;
	}
	C.LightStrength = Random() % 3 == 0 ? Random() % 8 : 0;
	C.LightColor = Random() % 216;
	C.Mark = Random() % 216;
	C.MarkTime = Random() % 2 ? 200000 : 0;
	C.Health = Random() % 101;
	C.PKFlag = Random() % 3 == 0 ? Random() % 5 : 0;
	C.PARTYFlag = Random() % 3 == 0 ? Random() % 5 : 0;
	C.SkullTime = Random() % 4 == 0 ? 1 : 0;
	C.Visible = Random() % 2 != 0; // IsOpponent
}

static void RandomWorld()
{
	memset(Fields, 0, sizeof(Fields));
	memset(Effects, 0, sizeof(Effects));
	CenterX = 1000 + Random() % 100;
	CenterY = 1000 + Random() % 100;
	CenterZ = Random() % 2 ? 7 : 3 + Random() % 9;
	Level = Random() % 3 == 0 ? 5 + Random() % 3 : 7;
	AmbientBrightness = Random() % 300 - 20;
	Ambient = RandomColor(256);
	int Density = 1 + Random() % 4;
	for (int x = 0; x < MAPSIZE_X; x++) {
		for (int y = 0; y < MAPSIZE_Y; y++) {
			for (int z = 0; z < MAPSIZE_Z; z++) {
				TFakeField &F = Fields[x][y][z];
				F.Throwable = Random() % 3 != 0;
				F.Translucent = Random() % 4 == 0;
				if (z != Level && Random() % 6 >= (unsigned)Density)
					continue;
				if (Random() % 8 != 0) {
					TObject Ground = {Random() % 6 == 0 && !FullBanks.empty() ? Pick(FullBanks) : Pick(Banks), 0, 0};
					F.Objects[F.Count++] = Ground;
				}
				int Count = Random() % 4;
				for (int k = 0; k < Count; k++)
					F.Objects[F.Count++] = RandomObject();
			}
		}
	}
	WorldSize = 1 + Random() % 11;
	for (int i = 0; i < WorldSize; i++) {
		int fx = i == 0 ? 8 : Random() % MAPSIZE_X, fy = i == 0 ? 6 : Random() % MAPSIZE_Y;
		int fz = i == 0 || Random() % 2 ? Level : Random() % MAPSIZE_Z;
		RandomCreature(i, fx, fy, fz);
		TFakeField &F = Fields[fx][fy][fz];
		if (F.Count < 6) {
			TObject Obj = {CREATURE, (int)World[i].CreatureID, 0};
			if (Random() % 6 == 0) // a creature that is not in the table
				Obj.Data = 0x7000000 + i;
			F.Objects[F.Count++] = Obj;
		}
	}
	for (int n = 0; n < EFFECTS; n++) {
		TFakeEffect &E = Effects[n];
		E.Status = 1 + Random() % 3;
		if (E.Status == 1) {
			E.Type = Random() % 10 == 0 ? -1 : 1 + Random() % LastEffect;
			E.Phase = E.Type == -1 ? 1 : 1 + Random() % GetEffectPhases(E.Type);
		} else if (E.Status == 2) {
			E.Color = Random() % 216;
			E.TextPhase = Random() % 10;
			RandomText(E.Text, 1 + Random() % 9);
		} else {
			E.Type = Random() % 10 == 0 ? -1 : Pick(Missiles);
			E.Direction = Random() % 8;
			E.x = Random() % 64 - 32;
			E.y = Random() % 64 - 32;
		}
	}
	int Count = Random() % 40;
	for (int k = 0; k < Count; k++) {
		TFakeField &F = Fields[Random() % MAPSIZE_X][Random() % MAPSIZE_Y][Random() % 2 ? Level : Random() % MAPSIZE_Z];
		if (F.EffectCount < 3)
			F.Effects[F.EffectCount++] = Random() % EFFECTS;
	}
	HighlightID = Random() % 3 == 0 ? World[Random() % WorldSize].CreatureID : 0;
	AttackID = Random() % 2 ? World[Random() % WorldSize].CreatureID : 0;
	FollowID = Random() % 3 == 0 ? World[Random() % WorldSize].CreatureID : 0;
	InParty = Random() % 2 != 0;
	ActionNumber = Random() % 10000;
	Blink = Random() % 2 != 0;
	LightEffects = Random() % 2 != 0;
	CanShade = Random() % 4 != 0;
	TextualEffects = Random() % 4 != 0;
	CreatureInfo = Random() % 3;
	ShowNewHints = false;
	EventHelp = Random() % 3;
	SurfaceLost = Random() % 8 == 0;
	SetTestClock(100000 + Random() % 200000);
}

static void RandomTalkboxes()
{
	static const int Modes[] = {TALK_PRIVATE_MESSAGE, TALK_GAMEMASTER_BROADCAST, TALK_GAMEMASTER_MESSAGE,
		TALK_ADMIN_MESSAGE, TALK_EVENT_MESSAGE, TALK_INFO_MESSAGE};
	NumberOfTalkboxes = Random() % (TALKBOXES + 1);
	for (int i = 0; i < NumberOfTalkboxes; i++) {
		TFakeTalkbox &T = Talkboxes[i];
		T.Mode = Modes[Random() % 6];
		T.x = Random() % 3 == 0 ? 0 : CenterX - 8 + Random() % MAPSIZE_X;
		T.y = CenterY - 6 + Random() % MAPSIZE_Y;
		T.z = 100 + Level;
		T.Lines = 1 + Random() % 4;
		for (int k = 0; k < T.Lines; k++)
			RandomText(T.Line[k], Random() % 20);
		T.Color = RandomColor(256);
		T.RectX = Random() % 500 - 10;
		T.RectY = Random() % 400 - 10;
		T.Alignment = Random() % 3;
		T.Fixed = Random() % 4;
	}
	StatusMessage = Random() % 3 == 0 ? "You are poisoned" : NULL;
	TalkChanged = Random() % 2 != 0;
}

// --- The windows -------------------------------------------------------------


static TMapWindow *Ours;
[[maybe_unused]] static uint8 *Original; // the oracle's (the runner has none)

static void NewWindows()
{
	void *Memory = calloc(1, sizeof(TMapWindow));
	Window[OURS] = Memory;
	Side = OURS;
	Ours = new (Memory) TMapWindow;
	ORIGINAL(Original = (uint8 *)calloc(1, sizeof(TMapWindow)); Window[ORIGINAL] = Original; Side = ORIGINAL;
		At<CtorFn>(0x4bfa00)(Original, NULL));
}

static void DeleteWindows()
{
	Side = OURS;
	Ours->~TMapWindow();
	free(Ours);
	ORIGINAL(Side = ORIGINAL; At<DeleteFn>(0x4bf9e0)(Original, NULL, 0); free(Original));
}

// Coverage: how often our side's logs held each kind of entry.
static const char *const Kinds[] = {"CW;", "CS;", "CA", "CL", "CM", "CT", "CU", "CX", "CC", "SD", "SG", "SM", "MT",
	"AM2,", "AM13,", "AM44,", "AM52,", "AT", "PD", "HH", "CI", "RO", "TR", "SH", "SB", "SS", "SR", "SF", "SK", "PT", "PC",
	"PP", "DM", "DS", "DC", "DQ", "DP", "GM"};
static int KindCount[sizeof(Kinds) / sizeof(Kinds[0])];

static void CountKinds(const std::string &Text)
{
	for (size_t i = 0; i < sizeof(Kinds) / sizeof(Kinds[0]); i++) {
		size_t Length = strlen(Kinds[i]);
		for (size_t p = 0; (p = Text.find(Kinds[i], p)) != std::string::npos; p += Length) {
			if (p == 0 || Text[p - 1] == ';')
				KindCount[i]++;
		}
	}
}

static void CompareLogs(const char *What, int Arg = 0)
{
	// The original's log: the oracle's, recorded; the runner's from the corpus.
	std::string Text = golden::OutputText("log", Log[ORIGINAL].data(), Log[ORIGINAL].size());
	ORIGINAL_OR((void)0, Log[ORIGINAL] = Text);
	CountKinds(Log[OURS]);
	Checks++;
	if (Log[OURS] != Log[ORIGINAL]) {
		Failures++;
		if (Failures <= FailuresPrinted) {
			size_t i = 0;
			while (i < Log[OURS].size() && i < Log[ORIGINAL].size() && Log[OURS][i] == Log[ORIGINAL][i])
				i++;
			size_t From = i > 80 ? i - 80 : 0;
			printf("FAIL %s (%d): logs differ at %d\n  ours:     %.200s\n  original: %.200s\n", What, Arg, (int)i,
				Log[OURS].c_str() + From, Log[ORIGINAL].c_str() + From);
		}
	}
	Log[OURS].clear();
	Log[ORIGINAL].clear();
}

// The compared members of a map view, in the order of the x86 bytes from +0x14 the old test compared
// (TWindow's children at +0x24 left out): TComponent's OffsetX, OffsetY, Width and Height (+0x14), then
// TMapWindow's members from NumberOfNames (+0x2c) up to HintButton (+0x92d4). None of them is a pointer;
// their bytes (padding included) are the same on both targets.
static const size_t GEOMETRY = 4 * sizeof(int);
static const size_t MEMBERS = offsetof(TMapWindow, HintButton) - offsetof(TMapWindow, NumberOfNames);
static const size_t STATE = GEOMETRY + MEMBERS;
static int X86Offset(size_t i) { return (int)(i < GEOMETRY ? 0x14 + i : 0x2c + (i - GEOMETRY)); }
static void StateOf(const TMapWindow *Map, uint8 *State)
{
	memcpy(State, &Map->OffsetX, GEOMETRY);
	memcpy(State + GEOMETRY, &Map->NumberOfNames, MEMBERS);
}
// The original's members (Value, the oracle's; ignored by the runner) by their changes since the last
// record into Last, which then holds the original's members on both sides (zeroed at first on both).
static void OutputState(const char *Name, uint8 *Last, const uint8 *Value) { golden::OutputRuns(Name, Last, Value, STATE); }
static std::vector<uint8> LastWindow(STATE);

// The window from TComponent::OffsetX on; HintButton is compared as set or
// not (the children are the two sides' own objects).
static void Compare(const char *What, int Arg = 0)
{
	CompareLogs(What, Arg);
	std::vector<uint8> A(STATE);
	StateOf(Ours, A.data());
	OutputState("window", LastWindow.data(), nullptr);
	const uint8 *B = LastWindow.data();
	size_t Differ = STATE;
	for (size_t i = 0; i < STATE && Differ == STATE; i++) {
		if (A[i] != B[i])
			Differ = i;
	}
	// The ints at the first difference: the 4-byte word of the x86 offset (the geometry and the members
	// start at multiples of four).
	int WordA = 0, WordB = 0;
	if (Differ < STATE) {
		size_t Word = Differ & ~(size_t)3;
		memcpy(&WordA, A.data() + Word, sizeof(int));
		memcpy(&WordB, B + Word, sizeof(int));
	}
	CHECK(Differ == STATE, "%s (%d): windows differ at +0x%x (%d, original %d)", What, Arg,
		Differ < STATE ? X86Offset(Differ) : -1, WordA, WordB);
	bool OriginalHint = ORIGINAL_VALUE("hint button", bool, *(void **)(Original + 0x92d4) != NULL);
	CHECK((Ours->HintButton == NULL) == !OriginalHint, "%s (%d): hint button", What, Arg);
}

static void SetSizes()
{
	int Width, Height;
	if (NoStretching) {
		Width = Random() % 4 == 0 ? 480 : 240 + Random() % 241;
		Height = Random() % 4 == 0 ? 352 : 176 + Random() % 177;
	} else {
		Width = 240 + Random() % 600;
		Height = 176 + Random() % 450;
	}
	Side = OURS;
	Ours->SetSize(Width, Height);
	ORIGINAL(Side = ORIGINAL; At<Int2Fn>(0x4bfc30)(Original, NULL, Width, Height));
	Compare("SetSize", Width);
	int Ox = Random() % 50, Oy = Random() % 50;
	Ours->OffsetX = Ox;
	Ours->OffsetY = Oy;
	ORIGINAL(*(int *)(Original + 0x14) = Ox; *(int *)(Original + 0x18) = Oy);
}

// Copies a random part of our window's state to the original one (the oracle; our x86 layout is the
// original's).
static void Mirror()
{
	ORIGINAL(memcpy(Original + 0x14, (uint8 *)Ours + 0x14, 0x10);
		memcpy(Original + 0x2c, (uint8 *)Ours + 0x2c, offsetof(TMapWindow, HintButton) - 0x2c));
}

static void RandomState()
{
	for (int x = 0; x < MAPSIZE_X; x++) {
		for (int y = 0; y < MAPSIZE_Y; y++) {
			Ours->CoverLevel[x][y] = Random() % 3 == 0 ? Random() % 8 : 0;
			Ours->Light[x][y] = RandomColor(256);
		}
	}
	Ours->ScrollX = Random() % 64;
	Ours->ScrollY = Random() % 64;
	Ours->PositionX = CenterX;
	Ours->PositionY = CenterY;
	Ours->PositionZ = CenterZ;
	Ours->MaxLevel = Level + Random() % (8 - Level);
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 12; j++) {
			TCacheField &F = Ours->Cache[i][j];
			F.CachedSprites = Random() % 11;
			for (int k = 0; k < 10; k++) {
				F.Sprite[k] = Random() % 4 == 0 ? 0 : 1 + Random() % 9;
				F.SpriteX[k] = Random() % 3 - 1;
				F.SpriteY[k] = Random() % 3 - 1;
			}
			F.Sprites = Random() % (F.CachedSprites + 1);
			F.Copied = Random() % 4 == 0;
		}
	}
	Mirror();
}

static int Executed[24];

// GetCacheField's result as an offset in the x86 window (the old test compared those): the tile's offset in
// the cache plus the cache's x86 offset.
static int CacheOffset(const TCacheField *Field)
{
	return (int)((const uint8 *)Field - (const uint8 *)&Ours->Cache[0][0]) + 0x29d4;
}

static void TestOperations(int Rounds)
{
	for (int t = 0; t < Rounds; t++) {
		int Op = Random() % 24;
		Executed[Op]++;
		// Our side runs first; an error it raises (an input outside the
		// original's assumptions) stops the test before the image is called.
		try {
			switch (Op) {
			case 0: { // IsFieldVisible, SetLightColor (the image's out-of-line copies)
				RandomState();
				int x = Random() % 20 - 1, y = Random() % 16 - 1, z = Random() % 9;
				Side = OURS;
				bool A = Ours->IsFieldVisible(x, y, z);
				bool B = ORIGINAL_VALUE("result", bool, (Side = ORIGINAL, At<Bool3Fn>(0x4bf8f0)(Original, NULL, x, y, z)));
				CHECK(A == B, "IsFieldVisible(%d,%d,%d)", x, y, z);
				x = Random() % MAPSIZE_X;
				y = Random() % MAPSIZE_Y;
				int b = Random() % 400 - 70;
				Side = OURS;
				Ours->SetLightColor(x, y, b);
				ORIGINAL(Side = ORIGINAL; At<Int3Fn>(0x4bf930)(Original, NULL, x, y, b));
				Compare("SetLightColor", b);
				break;
			}
			case 1: { // GetCacheField
				RandomState();
				int x = Random() % MAPSIZE_X, y = Random() % MAPSIZE_Y;
				Side = OURS;
				int A = CacheOffset(Ours->GetCacheField(x, y));
				int B = ORIGINAL_VALUE("result", int,
					(Side = ORIGINAL, (int)((uint8 *)At<CacheFn>(0x4c0590)(Original, NULL, x, y) - Original)));
				CHECK(A == B, "GetCacheField(%d,%d): +0x%x, original +0x%x", x, y, A, B);
				break;
			}
			case 2:
			case 3: { // CacheSprite, FlushField
				if (Random() % 4 == 0)
					RandomState();
				for (int k = 0; k < 20; k++) {
					int x = Random() % 20 - 1, y = Random() % 16 - 1;
					if (Op == 2) {
						int Sprite = Random() % 4 == 0 ? 0 : 1 + Random() % 9, dx = Random() % 3 - 1, dy = Random() % 3 - 1;
						Side = OURS;
						Ours->CacheSprite(x, y, Sprite, dx, dy);
						ORIGINAL(Side = ORIGINAL; At<Int5Fn>(0x4c41e0)(Original, NULL, x, y, Sprite, dx, dy));
					} else {
						Side = OURS;
						Ours->FlushField(x, y);
						ORIGINAL(Side = ORIGINAL; At<Int2Fn>(0x4c3980)(Original, NULL, x, y));
					}
					Compare(Op == 2 ? "CacheSprite" : "FlushField", x * 100 + y);
				}
				break;
			}
			case 4: { // DrawFieldSprite
				if (Random() % 4 == 0)
					RandomState();
				TRGBColor Colors[4] = {TRGBColor(1, 2, 3), TRGBColor(4, 5, 6), TRGBColor(7, 8, 9), TRGBColor(10, 11, 12)};
				for (int k = 0; k < 20; k++) {
					int x = Random() % 560 - 64, y = Random() % 440 - 64, z = Random() % 8;
					bool Animated = Random() % 3 == 0;
					int Sprite = 1 + Random() % 9, Mask = Random() % 4 == 0 ? 1 + Random() % 5 : 0;
					Side = OURS;
					Ours->DrawFieldSprite(x, y, z, Animated, Sprite, Mask, Colors);
					ORIGINAL(Side = ORIGINAL; At<SpriteFn>(0x4c46a0)(Original, NULL, x, y, z, Animated, Sprite, Mask, Colors));
					Compare("DrawFieldSprite", x);
				}
				break;
			}
			case 5: { // DrawLightMask, AddLightSource
				RandomState();
				for (int k = 0; k < 10; k++) {
					int x = Random() % 600 - 60, y = Random() % 450 - 50, b = Random() % 256;
					Side = OURS;
					Ours->DrawLightMask(x, y, b);
					ORIGINAL(Side = ORIGINAL; At<Int3Fn>(0x4c02c0)(Original, NULL, x, y, b));
					Compare("DrawLightMask", b);
					int z = Random() % 8, Brightness = Random() % 9 - 1;
					TRGBColor Color = RandomColor(300);
					Side = OURS;
					Ours->AddLightSource(x, y, z, Brightness, Color);
					ORIGINAL(Side = ORIGINAL; At<LightFn>(0x4c3c80)(Original, NULL, x, y, z, Brightness, Color));
					Compare("AddLightSource", Brightness);
				}
				break;
			}
			case 6: { // DrawObject
				RandomState();
				for (int k = 0; k < 10; k++) {
					TObject Obj = RandomObject();
					int x = Random() % 560 - 40, y = Random() % 420 - 40, z = Random() % 8;
					int ax = Random() % 2000, ay = Random() % 2000, az = Random() % 16;
					Side = OURS;
					Ours->DrawObject(x, y, z, Obj, ax, ay, az);
					ORIGINAL(Side = ORIGINAL; At<ObjectFn>(0x4c4af0)(Original, NULL, x, y, z, Obj, ax, ay, az));
					Compare("DrawObject", Obj.Type);
				}
				break;
			}
			case 7: { // DrawCreature
				RandomState();
				for (int k = 0; k < 10; k++) {
					RandomCreature(11, 8, 6, Level);
					if (World[11].OutfitType == 0)
						World[11].OutfitType = Pick(Outfits);
					int x = Random() % 560 - 40, y = Random() % 420 - 40, z = Random() % 8;
					Side = OURS;
					Ours->DrawCreature(x, y, z, &World[11]);
					ORIGINAL(Side = ORIGINAL; At<CreatureFn>(0x4c4f80)(Original, NULL, x, y, z, &World[11]));
					Compare("DrawCreature", World[11].OutfitType);
				}
				break;
			}
			case 8:
			case 9: { // DrawField
				RandomState();
				Ours->NumberOfNames = Random() % 5;
				Ours->NumberOfTextualEffects = Random() % 5;
				Mirror();
				for (int k = 0; k < 6; k++) {
					int fx = Random() % MAPSIZE_X, fy = Random() % MAPSIZE_Y, fz = Random() % 2 ? Level : Random() % 8;
					int Mode = Random() % 3 == 0 ? 1 : 0;
					int x = fx * 32 - Ours->ScrollX, y = fy * 32 - Ours->ScrollY;
					Side = OURS;
					Ours->DrawField(x, y, fx, fy, fz, Mode);
					ORIGINAL(Side = ORIGINAL; At<Int6Fn>(0x4c5520)(Original, NULL, x, y, fx, fy, fz, Mode));
					Compare("DrawField", fx * 100 + fy);
				}
				break;
			}
			case 10:
			case 11:
			case 12: { // Draw
				if (Random() % 2)
					SetSizes();
				RandomTalkboxes();
				int x = Random() % 100, y = Random() % 100;
				Side = OURS;
				Ours->Draw(1, x, y);
				ORIGINAL(Side = ORIGINAL; At<Int3Fn>(0x4c7580)(Original, NULL, 1, x, y));
				Compare("Draw", t);
				break;
			}
			case 13: { // ArrangeTalkboxes
				RandomTalkboxes();
				Side = OURS;
				Ours->ArrangeTalkboxes();
				ORIGINAL(Side = ORIGINAL; At<VoidFn>(0x4c0880)(Original, NULL));
				Compare("ArrangeTalkboxes", NumberOfTalkboxes);
				break;
			}
			case 14: { // GetField, GetCreatureAt
				for (int k = 0; k < 10; k++) {
					int x = Random() % Ours->Width, y = Random() % Ours->Height;
					int A[3], B[3] = {0, 0, 0};
					Side = OURS;
					Ours->GetField(x, y, &A[0], &A[1], &A[2]);
					uint32 IDA = Ours->GetCreatureAt(x, y);
					ORIGINAL(Side = ORIGINAL; At<FieldFn>(0x4c12e0)(Original, NULL, x, y, &B[0], &B[1], &B[2]));
					golden::Output("field", B);
					uint32 IDB = ORIGINAL_VALUE("creature", uint32, At<ID2Fn>(0x4c16b0)(Original, NULL, x, y));
					CHECK(A[0] == B[0] && A[1] == B[1] && A[2] == B[2], "GetField(%d,%d): %d,%d,%d, original %d,%d,%d", x, y,
						A[0], A[1], A[2], B[0], B[1], B[2]);
					CHECK(IDA == IDB, "GetCreatureAt(%d,%d): %u, original %u", x, y, IDA, IDB);
					Compare("GetField", x);
				}
				break;
			}
			case 15:
			case 16:
			case 17:
			case 18: { // mouse buttons and help
				static const int States[] = {0, 6, 7, 8, 9};
				ActionState = States[Random() % 5];
				DraggedObject = RandomObject();
				TradedObject = RandomObject();
				UsedObject = RandomObject();
				if (Random() % 3 == 0)
					DraggedObject.Type = Pick(Movables);
				else if (Random() % 2 == 0)
					DraggedObject.Type = Pick(Cumulatives);
				// Darkness away from the player: "You can't see anything".
				if (Random() % 4 == 0) {
					memset(Ours->Light.data(), 0, sizeof(Ours->Light));
					ORIGINAL(memset(Original + offsetof(TMapWindow, Light), 0, sizeof(Ours->Light)));
				}
				for (int n = 0; n < 3; n++) {
					SlotData[n][0] = Random() % 4;
					SlotData[n][1] = CenterX - 8 + Random() % MAPSIZE_X;
					SlotData[n][2] = CenterY - 6 + Random() % MAPSIZE_Y;
					SlotData[n][3] = 100 + (Random() % 2 ? Level : Random() % 8);
				}
				TradeInfo = Random() % 2 ? World[Random() % WorldSize].CreatureID : Random() % 3 == 0 ? 0
																									  : 0x7000000;
				ClassicControl = Random() % 2 != 0;
				Destination = Random() % 4;
				FreeContainer = Random() % 16;
				int x = Random() % Ours->Width, y = Random() % Ours->Height;
				// Often on a creature's name position (GetCreatureAt finds it).
				if (Ours->NumberOfNames > 0 && Random() % 2 == 0) {
					TCreatureName &Name = Ours->Names[Random() % Ours->NumberOfNames];
					if (NoStretching) {
						x = Name.x - (480 - Ours->Width) / 2 + (int)(Random() % 9) - 4;
						y = Name.y - (352 - Ours->Height) / 2 + (int)(Random() % 30);
					} else {
						x = Name.x * Ours->Width / 480 + (int)(Random() % 9) - 4;
						y = Name.y * Ours->Height / 352 + (int)(Random() % 30);
					}
					x = x < 0 ? 0 : x >= Ours->Width ? Ours->Width - 1
													 : x;
					y = y < 0 ? 0 : y >= Ours->Height ? Ours->Height - 1
													  : y;
				}
				static const int FlagSets[] = {0, 0x200, 0x400, 0x800, 0x600, 0x100};
				int Flags = FlagSets[Random() % 6];
				bool A, B = false;
				Side = OURS;
				if (Op == 15)
					A = Ours->LeftButtonDown(x, y, Flags);
				else if (Op == 16)
					A = Ours->LeftButtonUp(x, y, Flags);
				else if (Op == 17)
					A = Ours->RightButtonUp(x, y, Flags);
				else
					A = Ours->HelpMouseMove(x, y);
				golden::Output("result", B);
				CHECK(A == B, "mouse operation %d at %d,%d (flags 0x%x): %d, original %d", Op, x, y, Flags, A, B);
				Compare("mouse", Op);
				break;
			}
			case 19: {                                     // HandleEvent
				static const int Events[] = {600, 601, 5}; // 27 needs the hint button (case 20)
				int Event = Events[Random() % 3];
				NextHint = 1 + Random() % EVENTHELPS;
				Side = OURS;
				Ours->HandleEvent(Event);
				ORIGINAL(Side = ORIGINAL; At<Int1Fn>(0x4bffa0)(Original, NULL, Event));
				Compare("HandleEvent", Event);
				break;
			}
			case 20: { // Arrange with a hint button, then HandleEvent(27) and Draw remove it
				TElement *Element = new TElement;
				Side = OURS;
				Ours->InsertChild(Element);
				Ours->HintButton = Element;
				Ours->Arrange();
				ORIGINAL(Side = ORIGINAL; At<void(__fastcall *)(void *, void *, void *)>(0x49c190)(Original, NULL, OriginalElement);
					*(void **)(Original + 0x92d4) = OriginalElement; At<VoidFn>(0x4bfde0)(Original, NULL));
				Compare("Arrange");
				// The original button's offset and width (both recorded: the old && read the width only when the
				// offsets agreed, a read without side effects).
				int OriginalX = ORIGINAL_VALUE("button x", int, ((TComponent *)OriginalElement)->OffsetX);
				int OriginalWidth = ORIGINAL_VALUE("button width", int, ((TComponent *)OriginalElement)->Width);
				CHECK(Element->OffsetX == OriginalX && Element->Width == OriginalWidth, "Arrange: hint button area");
				if (Random() % 2) {
					NextHint = 1 + Random() % EVENTHELPS;
					Side = OURS;
					Ours->HandleEvent(27);
					ORIGINAL(Side = ORIGINAL; At<Int1Fn>(0x4bffa0)(Original, NULL, 27));
					Compare("HandleEvent", 27);
				} else {
					RandomTalkboxes();
					Side = OURS;
					Ours->Draw(1, 0, 0);
					ORIGINAL(Side = ORIGINAL; At<Int3Fn>(0x4c7580)(Original, NULL, 1, 0, 0));
					Compare("Draw with a hint button");
				}
				break;
			}
			case 21: { // the minimum size
				Side = OURS;
				int A = Ours->getMinimumWidth() * 1000 + Ours->getMinimumHeight();
				int B = ORIGINAL_VALUE("result", int,
					(Side = ORIGINAL, At<IntFn>(0x4bf8d0)(Original, NULL) * 1000 + At<IntFn>(0x4bf8e0)(Original, NULL)));
				CHECK(A == B, "minimum size %d, original %d", A, B);
				break;
			}
			case 22: // a new world
				RandomWorld();
				break;
			case 23:
				SetSizes();
				break;
			}
		} catch (TMyException &Exception) {
			printf("FAIL operation %d (%d): our side raised: %s\n", Op, t, Exception.GetMessage().c_str());
			exit(3);
		}
	}
}

// --- TMapFrame ---------------------------------------------------------------

// Clears what TMapWindow's constructor leaves uninitialised (the old test's x86 ranges +0x2c..+0x19fc,
// +0x25cc..+0x29d4 and each cache tile from +0xc, by member).
static void ClearMapWindow(TMapWindow *Map)
{
	memset(&Map->NumberOfNames, 0, offsetof(TMapWindow, Light) - offsetof(TMapWindow, NumberOfNames));
	memset(&Map->PositionX, 0, offsetof(TMapWindow, Cache) - offsetof(TMapWindow, PositionX));
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 12; j++)
			memset(&Map->Cache[i][j].CachedSprites, 0, sizeof(TCacheField) - offsetof(TCacheField, CachedSprites));
	}
}

// A frame's compared members, in the order of the x86 bytes the old test compared: its OffsetX, OffsetY,
// Width and Height (+0x14), its ViewY, ViewX, ViewWidth and ViewHeight (+0x30).
static void FrameOf(const TMapFrame *Frame, int Members[8])
{
	memcpy(Members, &Frame->OffsetX, 4 * sizeof(int));
	memcpy(Members + 4, &Frame->ViewY, 4 * sizeof(int));
}
static std::vector<uint8> LastFrameView(STATE);

static void TestFrames(int Rounds)
{
	for (int t = 0; t < Rounds; t++) {
		uint8 *A = (uint8 *)calloc(1, sizeof(TMapFrame));
		// The map views do not exist yet: their hotkey callbacks count as foreign.
		Window[OURS] = NULL;
		Window[ORIGINAL] = NULL;
		Side = OURS;
		TMapFrame *Frame = new (A) TMapFrame;
		TMapWindow *MapA = Frame->MapWindow;
		Window[OURS] = MapA;
		ClearMapWindow(MapA);
		CompareLogs("TMapFrame constructor");
		for (int k = 0; k < 4; k++) {
			NoStretching = Random() % 2 != 0;
			int Width = 260 + Random() % 700, Height = 200 + Random() % 500;
			Side = OURS;
			Frame->SetSize(Width, Height);
			int MinA = Frame->getMinimumWidth() * 1000 + Frame->getMinimumHeight();
			ORIGINAL(Side = ORIGINAL; At<Int2Fn>(0x4c1c40)(B, NULL, Width, Height));
			int MinB = ORIGINAL_VALUE("minimum", int, At<IntFn>(0x4c1ea0)(B, NULL) * 1000 + At<IntFn>(0x4c2040)(B, NULL));
			CHECK(MinA == MinB, "TMapFrame minimum size %d, original %d", MinA, MinB);
			int FrameA[8], FrameB[8] = {};
			FrameOf(Frame, FrameA);
			ORIGINAL(memcpy(FrameB, B + 0x14, 0x10); memcpy(FrameB + 4, B + 0x30, 0x10));
			golden::Output("frame", FrameB);
			CHECK(memcmp(FrameA, FrameB, 4 * sizeof(int)) == 0 && memcmp(FrameA + 4, FrameB + 4, 4 * sizeof(int)) == 0,
				"TMapFrame::SetSize(%d,%d)", Width, Height);
			std::vector<uint8> ViewA(STATE);
			StateOf(MapA, ViewA.data());
			OutputState("map view", LastFrameView.data(), nullptr);
			CHECK(memcmp(ViewA.data(), LastFrameView.data(), GEOMETRY) == 0
					&& memcmp(ViewA.data() + GEOMETRY, LastFrameView.data() + GEOMETRY, MEMBERS) == 0,
				"TMapFrame::SetSize(%d,%d): the map view", Width, Height);
			// The drawing list: eight TDrawing (seven ints, no pointers; the same bytes on both targets).
			static_assert(sizeof(TDrawing) == 0x1c, "TDrawing is 0x1c bytes");
			int LengthB = ORIGINAL_VALUE("drawing list length", int, *(int *)(B + 8));
			uint8 ListB[8 * sizeof(TDrawing)] = {};
			ORIGINAL(memcpy(ListB, *(void **)(B + 4), sizeof(ListB)));
			golden::OutputBytes("drawing list", ListB, sizeof(ListB));
			CHECK(Frame->DrawingListLength == LengthB && memcmp(Frame->DrawingList.get(), ListB, 8 * sizeof(TDrawing)) == 0,
				"TMapFrame drawing list after SetSize(%d,%d)", Width, Height);
			CompareLogs("TMapFrame::SetSize", Width);
		}
		Side = OURS;
		Frame->~TMapFrame();
		free(A);
		ORIGINAL(Side = ORIGINAL; At<DeleteFn>(0x4adc40)(B, NULL, 0); free(B));
		CompareLogs("TMapFrame destructor");
	}
}

// --- Error paths (issue #34) ---------------------------------------------------

// The hint button's place in the error round: a child that throws from
// SetPosition and SetSize while armed (Arrange's TEND), our class on both
// sides with the image's base code on the image side.
class TMapThrower : public TElement {
public:
	explicit TMapThrower(int _Owner) : Owner(_Owner), Armed(false) {}
	void Throw()
	{
		if (Armed)
			ThrowSideAssertion(Owner == ORIGINAL, -77);
	}
	void SetSize(int w, int h) override
	{
		Throw();
		if (Owner == OURS)
			TElement::SetSize(w, h);
		else
			ImageSetSize(w, h);
	}
	void SetPosition(int x, int y) override
	{
		Throw();
		if (Owner == OURS)
			TElement::SetPosition(x, y);
		else
			ImageSetPosition(x, y);
	}
	// TMapFrame's minimum size asks its map view (a thrower in its place).
	int getMinimumWidth() override
	{
		Throw();
		return 240;
	}
	int getMinimumHeight() override
	{
		Throw();
		return 176;
	}
	int Owner;
	bool Armed;

private:
	// The image's TComponent::SetSize and SetPosition (the oracle's image side only).
	void ImageSetSize([[maybe_unused]] int w, [[maybe_unused]] int h)
	{
		abort();
	}
	void ImageSetPosition([[maybe_unused]] int x, [[maybe_unused]] int y)
	{
		abort();
	}
};

static TErrorRound *MWRound;

// One call on both sides: the Fail-th fake call throws (0: none), the
// NullAt-th GetCreature finds nothing (0: none); then the windows and logs.
// Ticks is the original's fake calls afterwards (it ran last), recorded.
template <class F> static void MWCall(const char *What, int Fail, int NullAt, F Call)
{
	char Name[128];
	snprintf(Name, sizeof(Name), "%s (fail at %d, no creature at %d)", What, Fail, NullAt);
	GOLDEN_ROUND_COMPARE(*MWRound, Name, (Side = OURS, Ticks = 0, FailAt = Fail, NullCreatureAt = NullAt, Call(OURS)),
		(Side = ORIGINAL, Ticks = 0, FailAt = Fail, NullCreatureAt = NullAt, Call(ORIGINAL)));
	Ticks = ORIGINAL_VALUE("ticks", int, Ticks);
	FailAt = 0;
	NullCreatureAt = 0;
	Compare(Name, Fail);
}

// A fake call to fail at, of Count: uniform, near the start or near the end
// (both on a logarithmic scale: the early and late blocks of Draw have few
// of its calls).
static int FailIndex(int Count)
{
	int Kind = Random() % 3, Range = 1 << (Random() % 16);
	Range = Range < Count ? Range : Count;
	if (Kind == 0)
		return 1 + (int)(Random() % Count);
	if (Kind == 1)
		return 1 + (int)(Random() % Range);
	return Count - (int)(Random() % Range);
}

// The windows' state the error round restores before each failing call: the old test's bytes from +0x14 up
// to HintButton, which in our window are the members from OffsetX on (TWindow's children included, as
// there).
static uint8 *SavedFrom() { return (uint8 *)&Ours->OffsetX; }
static size_t SavedSize() { return (size_t)((uint8 *)&Ours->HintButton - SavedFrom()); }

// A call without failures (it counts the fake calls), then Tries calls that
// fail at one of them, each from the windows' state before the first (a
// second Draw makes fewer calls: the cache tiles hold the sprites).
template <class F> static void MWFailing(const char *What, int Tries, F Call)
{
	std::vector<uint8> Saved(SavedFrom(), SavedFrom() + SavedSize());
	MWCall(What, 0, 0, Call);
	int Count = Ticks;
	for (int t = 0; t < Tries && Count > 0; t++) {
		memcpy(SavedFrom(), Saved.data(), Saved.size());
		ORIGINAL(memcpy(Original + From, SavedOriginal.data(), Size));
		MWCall(What, FailIndex(Count), 0, Call);
	}
}

static void SetWidth(int Width, int Height)
{
	Ours->Width = Width;
	Ours->Height = Height;
	ORIGINAL(*(int *)(Original + 0x1c) = Width; *(int *)(Original + 0x20) = Height);
}

static void PlaceThrowers(TMapThrower *T[2])
{
	T[ORIGINAL] = NULL;
	for (int s = 0; s < SIDES; s++)
		T[s] = new TMapThrower(s);
	Side = OURS;
	Ours->InsertChild(T[OURS]);
	Ours->HintButton = T[OURS];
	ORIGINAL(Side = ORIGINAL; At<void(__fastcall *)(void *, void *, void *)>(0x49c190)(Original, NULL, T[ORIGINAL]);
		*(void **)(Original + 0x92d4) = T[ORIGINAL]);
}

// Arms or disarms the throwers of both sides (the runner has ours only).
static void Arm(TMapThrower *T[2], bool Armed)
{
	for (int s = 0; s < SIDES; s++)
		T[s]->Armed = Armed;
}

// Every PATELLA_ASSERT/TERROR of MapWindow.cpp and the TEND blocks: the fakes throw
// at the n-th call (FailAt) inside every function the test calls, then the
// assertions get their bad inputs (no player, bad flags, effect numbers,
// brightness, field indices, objects and talkbox modes).
static void TestMapWindowErrors()
{
	TErrorRound R("MapWindow");
	MWRound = &R;
	// The logging surface of this file's TTestSurface methods (alone, the
	// surface of engine 1 is another object; in the full run an earlier
	// test's TTestSurface): the round is the same in both.
	TSurface *SavedSurface = FakeSurface;
	TTestSurface *RoundSurface = new TTestSurface;
	FakeSurface = RoundSurface;
	TRGBColor Colors[4] = {TRGBColor(1, 2, 3), TRGBColor(4, 5, 6), TRGBColor(7, 8, 9), TRGBColor(10, 11, 12)};
	for (int Round = 0; Round < 12; Round++) {
		RandomWorld();
		while (Round % 3 == 0 && (Level == 7 || CreatureInfo == 0)) // names below the top level
			RandomWorld();
		NoStretching = Round % 2 != 0;
		SetSizes();
		RandomState();
		RandomTalkboxes();
		int x = Random() % 100, y = Random() % 100;
		MWFailing("TMapWindow::Draw", 8, [&](int s) {
			if (s == OURS) Ours->Draw(1, x, y); else ORIGINAL(At<Int3Fn>(0x4c7580)(Original, NULL, 1, x, y)); });
		// Below the top level, with names: the names' visibility loop (a few
		// hundred calls before the end).
		if (Level < 7 && CreatureInfo != 0) {
			std::vector<uint8> Saved(SavedFrom(), SavedFrom() + SavedSize());
			MWCall("TMapWindow::Draw below the top", 0, 0, [&](int s) {
				if (s == OURS) Ours->Draw(1, x, y); else ORIGINAL(At<Int3Fn>(0x4c7580)(Original, NULL, 1, x, y)); });
			int Count = Ticks;
			for (int t = 0; t < 12; t++) {
				memcpy(SavedFrom(), Saved.data(), Saved.size());
				ORIGINAL(memcpy(Original + From, SavedOriginal.data(), Size));
				MWCall("TMapWindow::Draw below the top", Count - (int)(Random() % (Count < 400 ? Count : 400)), 0,
					[&](int s) { if (s == OURS) Ours->Draw(1, x, y); else ORIGINAL(At<Int3Fn>(0x4c7580)(Original, NULL, 1, x, y)); });
			}
		}
		RandomState();
		Ours->NumberOfNames = Random() % 5;
		Ours->NumberOfTextualEffects = Random() % 5;
		Mirror();
		for (int k = 0; k < 4; k++) {
			// The first one is the player's field (creatures to draw).
			int fx = k == 0 ? 8 : Random() % MAPSIZE_X, fy = k == 0 ? 6 : Random() % MAPSIZE_Y;
			int fz = k == 0 || Random() % 2 ? Level : Random() % 8;
			int Mode = Random() % 3 == 0 ? 1 : 0, px = fx * 32 - Ours->ScrollX, py = fy * 32 - Ours->ScrollY;
			MWFailing("TMapWindow::DrawField", 8, [&](int s) {
				if (s == OURS) Ours->DrawField(px, py, fx, fy, fz, Mode);
				else ORIGINAL(At<Int6Fn>(0x4c5520)(Original, NULL, px, py, fx, fy, fz, Mode)); });
		}
		{
			TObject Obj = RandomObject();
			int px = Random() % 560 - 40, py = Random() % 420 - 40, z = Random() % 8;
			int ax = Random() % 2000, ay = Random() % 2000, az = Random() % 16;
			MWFailing("TMapWindow::DrawObject", 3, [&](int s) {
				if (s == OURS) Ours->DrawObject(px, py, z, Obj, ax, ay, az);
				else ORIGINAL(At<ObjectFn>(0x4c4af0)(Original, NULL, px, py, z, Obj, ax, ay, az)); });
			RandomCreature(11, 8, 6, Level);
			if (World[11].OutfitType == 0)
				World[11].OutfitType = Pick(Outfits);
			MWFailing("TMapWindow::DrawCreature", 3, [&](int s) {
				if (s == OURS) Ours->DrawCreature(px, py, z, &World[11]);
				else ORIGINAL(At<CreatureFn>(0x4c4f80)(Original, NULL, px, py, z, &World[11])); });
			bool Animated = Random() % 3 == 0;
			int Sprite = 1 + Random() % 9, Mask = Random() % 4 == 0 ? 1 + Random() % 5 : 0;
			MWFailing("TMapWindow::DrawFieldSprite", 3, [&](int s) {
				if (s == OURS) Ours->DrawFieldSprite(px, py, z, Animated, Sprite, Mask, Colors);
				else ORIGINAL(At<SpriteFn>(0x4c46a0)(Original, NULL, px, py, z, Animated, Sprite, Mask, Colors)); });
			int cx = Random() % MAPSIZE_X, cy = Random() % MAPSIZE_Y, dx = Random() % 3 - 1, dy = Random() % 3 - 1;
			MWFailing("TMapWindow::CacheSprite", 2, [&](int s) {
				if (s == OURS) Ours->CacheSprite(cx, cy, Sprite, dx, dy);
				else ORIGINAL(At<Int5Fn>(0x4c41e0)(Original, NULL, cx, cy, Sprite, dx, dy)); });
			// A tile with more sprites than this frame's (drawn again before the blit).
			int fx = 1 + Random() % 13, fy = 1 + Random() % 9;
			TCacheField *Tile = Ours->GetCacheField(fx, fy);
			Tile->Copied = false;
			Tile->Sprites = 1 + Random() % 5;
			Tile->CachedSprites = Tile->Sprites + 1 + Random() % 4;
			Mirror();
			MWFailing("TMapWindow::FlushField", 3, [&](int s) {
				if (s == OURS) Ours->FlushField(fx, fy); else ORIGINAL(At<Int2Fn>(0x4c3980)(Original, NULL, fx, fy)); });
			int b = Random() % 256;
			MWFailing("TMapWindow::DrawLightMask", 1, [&](int s) {
				if (s == OURS) Ours->DrawLightMask(px, py, b); else ORIGINAL(At<Int3Fn>(0x4c02c0)(Original, NULL, px, py, b)); });
			int Brightness = 1 + Random() % 7;
			TRGBColor Color = RandomColor(300);
			MWFailing("TMapWindow::AddLightSource", 3, [&](int s) {
				if (s == OURS) Ours->AddLightSource(px, py, z, Brightness, Color);
				else ORIGINAL(At<LightFn>(0x4c3c80)(Original, NULL, px, py, z, Brightness, Color)); });
			// A ground type Tibia.dat does not have: GetObjectFlag's error, wrapped
			// in AddLightSource's own catch block.
			BadMapType = Random() % 2 ? 60000 + Random() % 100 : 1 + Random() % 98;
			int lx = (Random() % MAPSIZE_X) * 32 + 16 - Ours->ScrollX, ly = (Random() % MAPSIZE_Y) * 32 + 16 - Ours->ScrollY;
			MWCall("TMapWindow::AddLightSource bad type", 0, 0, [&](int s) {
				if (s == OURS) Ours->AddLightSource(lx, ly, z, Brightness, Color);
				else ORIGINAL(At<LightFn>(0x4c3c80)(Original, NULL, lx, ly, z, Brightness, Color)); });
			BadMapType = 0;
		}
		RandomTalkboxes();
		MWFailing("TMapWindow::ArrangeTalkboxes", 3, [&](int s) {
			if (s == OURS) Ours->ArrangeTalkboxes(); else ORIGINAL(At<VoidFn>(0x4c0880)(Original, NULL)); });
		for (int k = 0; k < 3; k++) {
			int mx = Random() % Ours->Width, my = Random() % Ours->Height;
			MWFailing("TMapWindow::GetField", 2, [&](int s) {
				int f[3];
				if (s == OURS) Ours->GetField(mx, my, &f[0], &f[1], &f[2]);
				else ORIGINAL(At<FieldFn>(0x4c12e0)(Original, NULL, mx, my, &f[0], &f[1], &f[2])); });
			MWFailing("TMapWindow::GetCreatureAt", 2, [&](int s) {
				if (s == OURS) Ours->GetCreatureAt(mx, my); else ORIGINAL(At<ID2Fn>(0x4c16b0)(Original, NULL, mx, my)); });
		}
		// The mouse, with the inputs of the counted test.
		for (int Op = 15; Op <= 18; Op++) {
			static const int States[] = {0, 6, 7, 8, 9};
			ActionState = States[Random() % 5];
			DraggedObject = RandomObject();
			TradedObject = RandomObject();
			UsedObject = RandomObject();
			if (Random() % 3 == 0)
				DraggedObject.Type = Pick(Movables);
			else if (Random() % 2 == 0)
				DraggedObject.Type = Pick(Cumulatives);
			for (int n = 0; n < 3; n++) {
				SlotData[n][0] = Random() % 4;
				SlotData[n][1] = CenterX - 8 + Random() % MAPSIZE_X;
				SlotData[n][2] = CenterY - 6 + Random() % MAPSIZE_Y;
				SlotData[n][3] = 100 + (Random() % 2 ? Level : Random() % 8);
			}
			TradeInfo = Random() % 2 ? World[Random() % WorldSize].CreatureID : Random() % 3 == 0 ? 0
																								  : 0x7000000;
			ClassicControl = Random() % 2 != 0;
			Destination = Random() % 4;
			FreeContainer = Random() % 16;
			int mx = Random() % Ours->Width, my = Random() % Ours->Height;
			static const int FlagSets[] = {0, 0x200, 0x400, 0x800, 0x600, 0x100};
			int Flags = FlagSets[Random() % 6];
			MWFailing("TMapWindow mouse", 3, [&](int s) {
				if (s == OURS) {
					if (Op == 15) Ours->LeftButtonDown(mx, my, Flags);
					else if (Op == 16) Ours->LeftButtonUp(mx, my, Flags);
					else if (Op == 17) Ours->RightButtonUp(mx, my, Flags);
					else Ours->HelpMouseMove(mx, my);
				} else {
				}
			});
		}
		int Event = Random() % 2 ? 600 : 601;
		MWFailing("TMapWindow::HandleEvent", 1, [&](int s) {
			if (s == OURS) Ours->HandleEvent(Event); else ORIGINAL(At<Int1Fn>(0x4bffa0)(Original, NULL, Event)); });
		int Wd = 240 + Random() % 600, Ht = 176 + Random() % 450;
		MWFailing("TMapWindow::SetSize", 1, [&](int s) {
			if (s == OURS) Ours->SetSize(Wd, Ht); else ORIGINAL(At<Int2Fn>(0x4bfc30)(Original, NULL, Wd, Ht)); });
		SetSizes();

		// The assertions.
		int Bad;
		if (Random() % 2)
			Bad = -1 - (int)(Random() % 5);
		else {
			// (Random() % 2 ? MAPSIZE_X : MAPSIZE_Y) + (int)(Random() % 5) in the x86 oracle's order: the left
			// operand first (a probe with the oracle's flags, and the oracle's unchanged counts, show it).
			int Base = Random() % 2 ? MAPSIZE_X : MAPSIZE_Y;
			Bad = Base + (int)(Random() % 5);
		}
		int Good = Random() % MAPSIZE_Y;
		MWCall("TMapWindow::GetCacheField x", 0, 0, [&](int s) {
			if (s == OURS) Ours->GetCacheField(Bad, Good); else ORIGINAL(At<CacheFn>(0x4c0590)(Original, NULL, Bad, Good)); });
		MWCall("TMapWindow::GetCacheField y", 0, 0, [&](int s) {
			if (s == OURS) Ours->GetCacheField(Good % MAPSIZE_X, Bad);
			else ORIGINAL(At<CacheFn>(0x4c0590)(Original, NULL, Good % MAPSIZE_X, Bad)); });
		int BadBrightness = Random() % 2 ? -1 - (int)(Random() % 300) : 256 + (int)(Random() % 300);
		MWCall("TMapWindow::DrawLightMask brightness", 0, 0, [&](int s) {
			if (s == OURS) Ours->DrawLightMask(10, 20, BadBrightness);
			else ORIGINAL(At<Int3Fn>(0x4c02c0)(Original, NULL, 10, 20, BadBrightness)); });
		{
			TObject None;
			MWCall("TMapWindow::DrawObject type 0", 0, 0, [&](int s) {
				if (s == OURS) Ours->DrawObject(32, 32, 7, None, 1000, 1000, 7);
				else ORIGINAL(At<ObjectFn>(0x4c4af0)(Original, NULL, 32, 32, 7, None, 1000, 1000, 7)); });
			MWCall("TMapWindow::DrawCreature NULL", 0, 0, [&](int s) {
				if (s == OURS) Ours->DrawCreature(32, 32, 7, NULL);
				else ORIGINAL(At<CreatureFn>(0x4c4f80)(Original, NULL, 32, 32, 7, NULL)); });
			RandomCreature(11, 8, 6, Level);
			World[11].OutfitType = 0;
			MWCall("TMapWindow::DrawCreature no outfit", 0, 0, [&](int s) {
				if (s == OURS) Ours->DrawCreature(32, 32, 7, &World[11]);
				else ORIGINAL(At<CreatureFn>(0x4c4f80)(Original, NULL, 32, 32, 7, &World[11])); });
		}
		// No player: Draw's, GetField's and (its second GetCreature) HelpMouseMove's.
		MWCall("TMapWindow::Draw without a player", 0, 1, [&](int s) {
			if (s == OURS) Ours->Draw(1, 0, 0); else ORIGINAL(At<Int3Fn>(0x4c7580)(Original, NULL, 1, 0, 0)); });
		for (int k = 0; k < 6; k++) {
			int mx = Random() % Ours->Width, my = Random() % Ours->Height;
			MWCall("TMapWindow::GetField without a player", 0, 1, [&](int s) {
				int f[3];
				if (s == OURS) Ours->GetField(mx, my, &f[0], &f[1], &f[2]);
				else ORIGINAL(At<FieldFn>(0x4c12e0)(Original, NULL, mx, my, &f[0], &f[1], &f[2])); });
			MWCall("TMapWindow::HelpMouseMove without a player", 0, 2, [&](int s) {
				if (s == OURS) Ours->HelpMouseMove(mx, my); else ORIGINAL(At<Bool2Fn>(0x4c3520)(Original, NULL, mx, my)); });
		}
		// The map wider or higher than the map surface without stretching.
		{
			int SavedWidth = Ours->Width, SavedHeight = Ours->Height;
			bool SavedStretching = NoStretching;
			NoStretching = true;
			bool Wide = Random() % 2 != 0;
			// SetWidth's two arguments in the x86 oracle's order: right to left.
			int NewHeight = Wide ? 200 : 353 + Random() % 100;
			int NewWidth = Wide ? 481 + Random() % 100 : 200 + Random() % 280;
			SetWidth(NewWidth, NewHeight);
			MWCall(Wide ? "TMapWindow::Draw too wide" : "TMapWindow::Draw too high", 0, 0, [&](int s) {
				if (s == OURS) Ours->Draw(1, 0, 0); else ORIGINAL(At<Int3Fn>(0x4c7580)(Original, NULL, 1, 0, 0)); });
			SetWidth(SavedWidth, SavedHeight);
			NoStretching = SavedStretching;
		}
		// Unknown party and skull flags of the creatures with names.
		{
			int Flag = 5 + Random() % 20;
			bool Party = Random() % 2 != 0;
			int SavedInfo = CreatureInfo;
			CreatureInfo = 1 + Random() % 2;
			for (int i = 0; i < WorldSize; i++) {
				if (Party)
					World[i].PARTYFlag = Flag;
				else
					World[i].PKFlag = Flag;
			}
			MWCall(Party ? "TMapWindow::Draw party flag" : "TMapWindow::Draw skull flag", 0, 0, [&](int s) {
				if (s == OURS) Ours->Draw(1, 0, 0); else ORIGINAL(At<Int3Fn>(0x4c7580)(Original, NULL, 1, 0, 0)); });
			for (int i = 0; i < WorldSize; i++)
				World[i].PARTYFlag = World[i].PKFlag = 0;
			CreatureInfo = SavedInfo;
		}
		// An effect number outside the table.
		{
			int fx = Random() % MAPSIZE_X, fy = Random() % MAPSIZE_Y;
			TFakeField &F = Fields[fx][fy][Level];
			int SavedCount = F.EffectCount, Saved0 = F.Effects[0];
			F.EffectCount = 1;
			F.Effects[0] = Random() % 2 ? EFFECTS + (int)(Random() % 50) : -2 - (int)(Random() % 50);
			int px = fx * 32 - Ours->ScrollX, py = fy * 32 - Ours->ScrollY;
			MWCall("TMapWindow::DrawField effect number", 0, 0, [&](int s) {
				if (s == OURS) Ours->DrawField(px, py, fx, fy, Level, 0);
				else ORIGINAL(At<Int6Fn>(0x4c5520)(Original, NULL, px, py, fx, fy, Level, 0)); });
			F.EffectCount = SavedCount;
			F.Effects[0] = Saved0;
		}
		// A talkbox without a creature and with a mode ArrangeTalkboxes does not know.
		RandomTalkboxes();
		if (NumberOfTalkboxes == 0)
			NumberOfTalkboxes = 1;
		{
			TFakeTalkbox &T = Talkboxes[Random() % NumberOfTalkboxes];
			T.x = 0;
			T.Mode = Random() % 2 ? 1 + (int)(Random() % 3) : 40 + (int)(Random() % 10);
			if (T.Lines == 0)
				T.Lines = 1;
			MWCall("TMapWindow::ArrangeTalkboxes mode", 0, 0, [&](int s) {
				if (s == OURS) Ours->ArrangeTalkboxes(); else ORIGINAL(At<VoidFn>(0x4c0880)(Original, NULL)); });
		}
		// The use and trade objects the mouse asserts.
		for (int k = 0; k < 8; k++) {
			int Which = k % 4;
			ActionState = Which == 0 ? 7 : Which == 1 ? 8
													  : 0;
			ClassicControl = Which == 2;
			TradeInfo = 0;
			UsedObject = RandomObject();
			TradedObject = RandomObject();
			if (Which == 0)
				UsedObject.Type = Random() % 2 ? 0 : CREATURE;
			else if (Which == 1)
				TradedObject.Type = 0;
			else
				UsedObject.Type = CREATURE;
			int Flags = Which == 3 ? 0x400 : 0;
			int mx = Random() % Ours->Width, my = Random() % Ours->Height;
			MWCall("TMapWindow mouse objects", 0, 0, [&](int s) {
				if (s == OURS) {
					if (Which <= 1) Ours->LeftButtonUp(mx, my, 0);
					else Ours->RightButtonUp(mx, my, Flags);
				} else {
				}
			});
		}
		// The hint button: Arrange with a child that throws, then HandleEvent(27)
		// (which deletes it) with a hint outside 1..EVENTHELPS.
		{
			TMapThrower *T[2];
			PlaceThrowers(T);
			Arm(T, true);
			MWCall("TMapWindow::Arrange", 0, 0, [&](int s) {
				if (s == OURS) Ours->Arrange(); else ORIGINAL(At<VoidFn>(0x4bfde0)(Original, NULL)); });
			Arm(T, false);
			NextHint = Random() % 3 == 0 ? 1 + Random() % EVENTHELPS : Random() % 2 ? 0
																					: EVENTHELPS + 1 + Random() % 5;
			MWCall("TMapWindow::HandleEvent hint", 0, 0, [&](int s) {
				if (s == OURS) Ours->HandleEvent(27); else ORIGINAL(At<Int1Fn>(0x4bffa0)(Original, NULL, 27)); });
		}
	}
	// The constructors: TMapWindow's hotkeys, TMapFrame's map view.
	void *SavedWindows[2] = {Window[OURS], Window[ORIGINAL]};
	for (int Fail = 1; Fail <= 2; Fail++) {
		uint8 *A = (uint8 *)calloc(1, sizeof(TMapWindow));
		Window[OURS] = A;
		ORIGINAL(uint8 *B = (uint8 *)calloc(1, sizeof(TMapWindow)); Window[ORIGINAL] = B);
		GOLDEN_ROUND_COMPARE(R, "TMapWindow::TMapWindow", (Side = OURS, Ticks = 0, FailAt = Fail, new (A) TMapWindow),
			(Side = ORIGINAL, Ticks = 0, FailAt = Fail, At<CtorFn>(0x4bfa00)(B, NULL)));
		FailAt = 0;
		CompareLogs("TMapWindow::TMapWindow", Fail);
		free(A);
		ORIGINAL(free(B));
		A = (uint8 *)calloc(1, sizeof(TMapFrame));
		ORIGINAL(B = (uint8 *)calloc(1, sizeof(TMapFrame)));
		Window[OURS] = Window[ORIGINAL] = NULL;
		GOLDEN_ROUND_COMPARE(R, "TMapFrame::TMapFrame", (Side = OURS, Ticks = 0, FailAt = Fail, new (A) TMapFrame),
			(Side = ORIGINAL, Ticks = 0, FailAt = Fail, At<CtorFn>(0x4c1a40)(B, NULL)));
		FailAt = 0;
		CompareLogs("TMapFrame::TMapFrame", Fail);
		free(A);
		ORIGINAL(free(B));
	}
	// TMapFrame::SetSize through its map view's SetSize.
	{
		uint8 *A = (uint8 *)calloc(1, sizeof(TMapFrame));
		Window[OURS] = Window[ORIGINAL] = NULL;
		Side = OURS;
		TMapFrame *Frame = new (A) TMapFrame;
		ClearMapWindow(Frame->MapWindow);
		CompareLogs("TMapFrame errors: constructor");
		for (int Fail = 0; Fail <= 4; Fail++) {
			int Width = 260 + Random() % 700, Height = 200 + Random() % 500;
			GOLDEN_ROUND_COMPARE(R, "TMapFrame::SetSize", (Side = OURS, Ticks = 0, FailAt = Fail, Frame->SetSize(Width, Height)),
				(Side = ORIGINAL, Ticks = 0, FailAt = Fail, At<Int2Fn>(0x4c1c40)(B, NULL, Width, Height)));
			FailAt = 0;
			CompareLogs("TMapFrame::SetSize", Fail);
		}
		// The minimum size with a map view that throws.
		TMapWindow *View = Frame->MapWindow;
		TMapThrower *T[2] = {new TMapThrower(OURS), NULL};
		ORIGINAL(void *OriginalView = *(void **)(B + 0x2c); T[ORIGINAL] = new TMapThrower(ORIGINAL));
		Arm(T, true);
		Frame->MapWindow = (TMapWindow *)(TComponent *)T[OURS];
		ORIGINAL(*(void **)(B + 0x2c) = T[ORIGINAL]);
		GOLDEN_ROUND_COMPARE(R, "TMapFrame::getMinimumWidth", (Side = OURS, Frame->getMinimumWidth()),
			(Side = ORIGINAL, At<IntFn>(0x4c1ea0)(B, NULL)));
		GOLDEN_ROUND_COMPARE(R, "TMapFrame::getMinimumHeight", (Side = OURS, Frame->getMinimumHeight()),
			(Side = ORIGINAL, At<IntFn>(0x4c2040)(B, NULL)));
		Frame->MapWindow = View;
		ORIGINAL(*(void **)(B + 0x2c) = OriginalView);
		delete T[OURS];
		ORIGINAL(delete T[ORIGINAL]);
		Side = OURS;
		Frame->~TMapFrame();
		free(A);
		ORIGINAL(Side = ORIGINAL; At<DeleteFn>(0x4adc40)(B, NULL, 0); free(B));
		CompareLogs("TMapFrame errors: destructor");
	}
	Window[OURS] = SavedWindows[OURS];
	Window[ORIGINAL] = SavedWindows[ORIGINAL];
	FakeSurface = SavedSurface;
	delete RoundSurface;
	R.Finish();
	MWRound = NULL;
}

} // namespace MapWindowTest
using namespace MapWindowTest;

void TestMapWindow(const char *ExePath)
{
	int Before = Checks, BeforeFailures = Failures;
	ORIGINAL(PatchImage(); PatchScript());
	std::string Dat = golden::DataFile("Tibia.dat", ExePath);
	if (Dat.empty()) {
		printf("note: Tibia.dat not found next to %s, MapWindow tests skipped\n", ExePath != nullptr ? ExePath : "Tibia.exe");
		return;
	}
	LoadObjects(Dat.c_str());
	ORIGINAL(At<void(__cdecl *)(const char *)>(0x4d7380)(Dat.c_str()));
	SortTypes();
	InitSkins();
	ORIGINAL(Global<int>(0x71c450) = NumberOfSkins; Global<TSkin *>(0x71c454) = Skins.get());
	InitSkinProperties();
	ORIGINAL(At<void(__cdecl *)()>(0x498160)());
	int SavedEngine = curEngine;
	curEngine = 1;
	FakeSurface = GetSurface();
	PatchWorld();
	CHECK(GetEffectPhases(13) >= 4, "effect 13 has %d phases", GetEffectPhases(13));

	RandomWorld();
	NewWindows();
	Compare("constructor");
	NoStretching = false;
	SetSizes();
	TestOperations(3000);
	for (int Round = 0; Round < 4; Round++) {
		NoStretching = Round % 2 != 0;
		SetSizes();
		TestOperations(800);
	}
	DeleteWindows();
	CompareLogs("destructor");
	TestFrames(40);
	// The error round (issue #34), on new windows: its checks and log entries
	// are reported apart.
	int RoundChecks = Checks, SavedKinds[sizeof(KindCount) / sizeof(KindCount[0])];
	memcpy(SavedKinds, KindCount, sizeof(KindCount));
	NewWindows();
	TestMapWindowErrors();
	DeleteWindows();
	CompareLogs("errors: destructor");
	memcpy(KindCount, SavedKinds, sizeof(KindCount));
	RoundChecks = Checks - RoundChecks;

	ExitSkins();
	ORIGINAL(Global<TSkin *>(0x71c454) = NULL);
	curEngine = SavedEngine;
	ReleaseObjects();
	ORIGINAL(At<void(__cdecl *)()>(0x4d70c0)());
	int Least = Executed[0];
	for (int i = 1; i < 24; i++)
		Least = Executed[i] < Least ? Executed[i] : Least;
	printf("MapWindow: %d checks, %d failures (each of the 24 operations at least %d times; entries:",
		Checks - Before - RoundChecks, Failures - BeforeFailures, Least);
	for (size_t i = 0; i < sizeof(Kinds) / sizeof(Kinds[0]); i++)
		printf(" %.*s %d", (int)strcspn(Kinds[i], ",;"), Kinds[i], KindCount[i]);
	printf(")\n");
}
