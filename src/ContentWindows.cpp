// ContentWindows.cpp: the windows of the side bar and of the dialog column
// (c:\mud\tibiaplayer\ContentWindows.cpp); see ContentWindows.h.
//
// Order: the functions follow the source's line numbers. The binary moves the
// creators and most constructors behind the rest (each calls functions defined
// further down, the pattern of Creatures.cpp): the creators come last, after
// the constructors of the bodies and of TAutomapWindow, TInventoryView's
// mouse handlers, TStatusWindow's and the container body's constructors.
// Functions without line evidence go where the binary has them. The inline
// header code at the start of the object (0x425920-0x425b20 and the
// destructors 0x425a70-0x425e6f) is in ContentWindows.h, common.h and
// Container.h.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none.

#include "ContentWindows.h"
#include <cstdint>
#include <algorithm>
#include <array>
#include "Communication.h"
#include "ContextMenus.h"
#include "Creatures.h"
#include "Inventory.h"
#include "Objects.h"
#include "Map.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Text.h"
#include "Utils.h"
#include "graphics.h"
#include "DialogElements.h"
#include "DialogWindow.h"
#include "GUI.h"
#include "Help.h"
#include "Talk.h"
#include "common.h"
#include "compat/print.h"
#include "platform/Platform.h"
#include "types/FixedString.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries; the original's varargs (TMultiStateButton's constructor, _snprintf)
// NOLINTBEGIN(cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg)


namespace {

// Data, in .bss order. ContentWindows.obj's copy of the text table is
// Text.h's (0x5c0678-0x5c08c3, then its copy of DefaultFontColor).
// 0x5c4398 AutomapMouseOverText and 0x5c4400 SkillMouseOverText are TGUI's (GUI.h, issue #107).
// At 0x5c4500, constructed by the static initialiser 0x559dd0 as
// TCallback(NULL, 0) like the other window files' (ConsoleWindow.obj's is at
// 0x5c4390, initialiser 0x559dc0); here TTradeButtons' constructor copies it.
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): the binary's global; TCallback's constructor (GUI.cpp) stores two values
TCallback NoCallback(nullptr, 0);

} // namespace

// The texts TFixedStrings (issue #60).
static_assert(sizeof(patella::bridge::GUI().AutomapMouseOverText) == 100 && sizeof(patella::bridge::GUI().SkillMouseOverText) == 256, "the mouse-over texts");

// --- The creators -----------------------------------------------------------------

// 0x4362b0, line 106. The inventory: a plain content window with the body
// slots and the large combat controls.
TContentWindow *CreateInventoryWindow(TWindow *Receiver)
{
	TContentWindow *Window = new TContentWindow(1, true);
	Window->SetSize(SkinProperties().Data184, SkinProperties().Data274);
	TInventoryView *View = new TInventoryView(Receiver);
	Window->InsertChild(View);
	Window->SetChildPosition(View, SkinProperties().Data278.width, SkinProperties().Data278.height);
	TCombatControls *Controls = new TCombatControls(Receiver, true);
	Window->InsertChild(Controls);
	Window->SetChildPosition(Controls, SkinProperties().Data278.x, SkinProperties().Data278.y);
	Window->Arrange();
	return Window;
}

// 0x436510, line 140. The small inventory: the small combat controls, the
// "maximise" button (event 24 of the main window), capacity, soul and states.
TContentWindow *CreateSmallInventoryWindow(TWindow *Receiver)
{
	TSmallInventoryWindow *Window = new TSmallInventoryWindow;
	Window->SetSize(SkinProperties().Data184, SkinProperties().Data54C);
	TCombatControls *Controls = new TCombatControls(Receiver, false);
	Window->InsertChild(Controls);
	Window->SetChildPosition(Controls, SkinProperties().Data550.x, SkinProperties().Data550.y);
	TSkinButton *Maximise = new TSkinButton(0, 0, 140, 141, TCallback(Receiver, 24), TextMaximiseInventory, false);
	Window->InsertChild(Maximise);
	Window->SetChildPosition(Maximise, SkinProperties().Data550.width, SkinProperties().Data550.height);
	TCapacityLabel *Capacity = new TCapacityLabel;
	Window->InsertChild(Capacity);
	Window->SetChildArea(Capacity, SkinProperties().Data560);
	TSoulLabel *Soul = new TSoulLabel;
	Window->InsertChild(Soul);
	Window->SetChildArea(Soul, SkinProperties().Data570);
	TPlayerStates *States = new TPlayerStates(false);
	Window->InsertChild(States);
	Window->SetChildArea(States, SkinProperties().Data580);
	return Window;
}

// 0x43c520, line 170. A container's window: close (event 10) and minimize
// (event 9) go to the window, "up" (event 3) to the body; the icon is the
// container object, the title its name. The window gets the body's initial
// height before MainWindow.cpp adds it to the column.
TSubWindow *CreateContainerWindow(int Container)
{
	TContainerWindowBody *Body = new TContainerWindowBody(Container);
	TSubWindow *Window = new TSubWindow(Container + 0x40, Body, true, false);
	TSkinButton *CloseButton = new TSkinButton(0, 0, 136, 137, TCallback(Window, 10), TextCloseWindow, false);
	TSkinButton *MinimizeButton = new TSkinButton(0, 0, 138, 139, TCallback(Window, 9), TextMaximiseOrMinimise, false);
	TSkinButton *UpButton = new TSkinButton(0, 0, 126, 127, TCallback(Body, 3), TextShowHigherContainer, false);
	TObjectIcon *Icon = new TObjectIcon(GetContainerObject(Container), &SkinProperties().Style194.Data80, false);
	TLabel *Title = new TLabel(SkinProperties().Style194.Data38, GetContainerName(Container));
	Window->SetSize(SkinProperties().Data184, TSubWindow::GetFrameHeight() + Body->GetInitialHeight());
	Window->SetCloseButton(CloseButton);
	Window->SetMinimizeButton(MinimizeButton);
	Window->SetUpButton(UpButton);
	Window->SetIcon(Icon);
	Window->SetTitle(Title);
	Window->Arrange();
	return Window;
}

// 0x43c920, line 197
TSubWindow *CreateSkillsWindow()
{
	TSkillsWindowBody *Body = new TSkillsWindowBody;
	TSubWindow *Window = new TSubWindow(6, Body, true, false);
	TSkinButton *CloseButton = new TSkinButton(0, 0, 136, 137, TCallback(Window, 10), TextCloseWindow, false);
	TSkinButton *MinimizeButton = new TSkinButton(0, 0, 138, 139, TCallback(Window, 9), TextMaximiseOrMinimise, false);
	TLabel *Title = new TLabel(SkinProperties().Style194.Data38, "Skills");
	TSkinIcon *Icon = new TSkinIcon(142);
	Window->SetSize(SkinProperties().Data184, TSubWindow::GetFrameHeight() + Body->GetInitialHeight());
	Window->SetCloseButton(CloseButton);
	Window->SetMinimizeButton(MinimizeButton);
	Window->SetIcon(Icon);
	Window->SetTitle(Title);
	Window->Arrange();
	return Window;
}

// 0x43cc90, line 225
TSubWindow *CreateBattleWindow()
{
	TBattleWindowBody *Body = new TBattleWindowBody;
	TSubWindow *Window = new TSubWindow(7, Body, true, false);
	TSkinButton *CloseButton = new TSkinButton(0, 0, 136, 137, TCallback(Window, 10), TextCloseWindow, false);
	TSkinButton *MinimizeButton = new TSkinButton(0, 0, 138, 139, TCallback(Window, 9), TextMaximiseOrMinimise, false);
	TLabel *Title = new TLabel(SkinProperties().Style194.Data38, "Battle");
	TSkinIcon *Icon = new TSkinIcon(144);
	Window->SetSize(SkinProperties().Data184, TSubWindow::GetFrameHeight() + Body->GetInitialHeight());
	Window->SetCloseButton(CloseButton);
	Window->SetMinimizeButton(MinimizeButton);
	Window->SetIcon(Icon);
	Window->SetTitle(Title);
	Window->Arrange();
	return Window;
}

// 0x43d000, line 253. The cancel button closes the window (event 10).
TSubWindow *CreateTradeWindow()
{
	TTradeWindowBody *Body = new TTradeWindowBody;
	TSubWindow *Window = new TSubWindow(TRADE_WINDOW, Body, true, false);
	TSkinButton *CloseButton = new TSkinButton(0, 0, 136, 137, TCallback(Window, 10), TextCloseWindow, false);
	TSkinButton *MinimizeButton = new TSkinButton(0, 0, 138, 139, TCallback(Window, 9), TextMaximiseOrMinimise, false);
	TLabel *Title = new TLabel(SkinProperties().Style194.Data38, "Trade");
	TSkinIcon *Icon = new TSkinIcon(145);
	Window->SetSize(SkinProperties().Data184, TSubWindow::GetFrameHeight() + Body->GetInitialHeight());
	Window->SetCloseButton(CloseButton);
	Window->SetMinimizeButton(MinimizeButton);
	Window->SetIcon(Icon);
	Window->SetTitle(Title);
	Body->Buttons->Callback = TCallback(Window, 10);
	Body->Buttons->SetCancelButton(true);
	Window->Arrange();
	return Window;
}

// 0x43d390, line 280
TSubWindow *CreateBuddylistWindow()
{
	TBuddylistWindowBody *Body = new TBuddylistWindowBody;
	TSubWindow *Window = new TSubWindow(9, Body, true, false);
	TSkinButton *CloseButton = new TSkinButton(0, 0, 136, 137, TCallback(Window, 10), TextCloseWindow, false);
	TSkinButton *MinimizeButton = new TSkinButton(0, 0, 138, 139, TCallback(Window, 9), TextMaximiseOrMinimise, false);
	TLabel *Title = new TLabel(SkinProperties().Style194.Data38, "VIP");
	TSkinIcon *Icon = new TSkinIcon(143);
	Window->SetSize(SkinProperties().Data184, TSubWindow::GetFrameHeight() + Body->GetInitialHeight());
	Window->SetCloseButton(CloseButton);
	Window->SetMinimizeButton(MinimizeButton);
	Window->SetIcon(Icon);
	Window->SetTitle(Title);
	Window->Arrange();
	return Window;
}

// --- TButtonsWindow ---------------------------------------------------------------

// 0x426480, lines 292-309. The assertion comes before the try block (after
// the base class).
TButtonsWindow::TButtonsWindow(TDialogWindow *_CheckObject, TWindow *Receiver)
	: TContentWindow(5, true)
{
	PATELLA_ASSERT(_CheckObject!=nullptr);
	CheckObject = _CheckObject;
	SetSize(SkinProperties().Data184, SkinProperties().Data608);
	SkillsButton = new TTextToggleButton(SkinProperties().Data60C.x, SkinProperties().Data60C.y, "Skills", 14, 15,
		TCallback(Receiver, 14), TextOpenSkillsWindow, TextCloseSkillsWindow, false);
	InsertChild(SkillsButton);
	BattleButton = new TTextToggleButton(SkinProperties().Data60C.width, SkinProperties().Data60C.height, "Battle", 14,
		15, TCallback(Receiver, 15), TextOpenBattleWindow, TextCloseBattleWindow, false);
	InsertChild(BattleButton);
	BuddyButton = new TTextToggleButton(SkinProperties().Data61C.x, SkinProperties().Data61C.y, "VIP", 14, 15,
		TCallback(Receiver, 22), TextOpenVIPWindow, TextCloseVIPWindow, false);
	InsertChild(BuddyButton);
	LogoutButton = new TTextButton(SkinProperties().Data61C.width, SkinProperties().Data61C.height, "Logout",
		TCallback(Receiver, 507), TextLeaveGame);
	InsertChild(LogoutButton);
}

// 0x4268c0, line 322. A button shows as pressed while its window is open.
void TButtonsWindow::Draw(int Surface, int x, int y)
{
	SkillsButton->State = CheckObject->hasContentWindow(6);
	BattleButton->State = CheckObject->hasContentWindow(7);
	BuddyButton->State = CheckObject->hasContentWindow(9);
	TContentWindow::Draw(Surface, x, y);
}

// 0x426aa0, line 334
bool TButtonsWindow::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(16);
	SetHelpSize(Width, Height);
	return true;
}

// --- TAutomapWindow ---------------------------------------------------------------

// 0x4368c0, line 387. The try block starts after the base class. The zoom
// comes from the preferences (2 unless it is 1, 2, 4 or 8); the hotkeys
// Alt+Home/End zoom, Alt+PageUp/PageDown change the floor, Alt+cursor
// scrolls.
TAutomapWindow::TAutomapWindow()
	: TContentWindow(3, true)
{
	DrawingListLength = 4;
	DrawingList.reset(new TDrawing[4]);
	SetSize(SkinProperties().Data184, SkinProperties().Data220);
	Changed = true;
	Centred = true;
	Zoom = GetAutomapZoom();
	if (Zoom != 1 && Zoom != 2 && Zoom != 4 && Zoom != 8)
		Zoom = 2;
	View = new TAutomapView;
	CentreButton = new TTextButton(SkinProperties().Data264.x, SkinProperties().Data264.y, "Centre", TCallback(this, 106),
		TextCentreAroundPlayer);
	FloorUpButton = new TSkinButton(SkinProperties().Data254.x, SkinProperties().Data254.y, 70, 71, TCallback(this, 3),
		TextShowHigherFloor, false);
	FloorDownButton = new TSkinButton(SkinProperties().Data254.width, SkinProperties().Data254.height, 72, 73,
		TCallback(this, 4), TextShowLowerFloor, false);
	ZoomInButton = new TSkinButton(SkinProperties().Data244.x, SkinProperties().Data244.y, 66, 67, TCallback(this, 107),
		TextZoomIn, false);
	ZoomOutButton = new TSkinButton(SkinProperties().Data244.width, SkinProperties().Data244.height, 68, 69,
		TCallback(this, 108), TextZoomOut, false);
	CompassRose = new TCompassRose(65);
	InsertChild(View);
	InsertChild(CentreButton);
	InsertChild(FloorUpButton);
	InsertChild(FloorDownButton);
	InsertChild(ZoomInButton);
	InsertChild(ZoomOutButton);
	InsertChild(CompassRose);
	SetChildArea(View, SkinProperties().Data234);
	SetChildPosition(CompassRose, SkinProperties().Data264.width, SkinProperties().Data264.height);
	CompassRose->SetSize(Skins()[65].width, Skins()[65].height);
	Arrange();
	AddHotkey(0x912, TCallback(this, 108));
	AddHotkey(0x913, TCallback(this, 107));
	AddHotkey(0x914, TCallback(this, 3));
	AddHotkey(0x915, TCallback(this, 4));
	AddHotkey(0x90c, TCallback(this, 109));
	AddHotkey(0x90d, TCallback(this, 112));
	AddHotkey(0x90e, TCallback(this, 110));
	AddHotkey(0x90f, TCallback(this, 111));
}

// 0x426c40, line 406 (deleting destructor 0x436e80). Its work is the close
// since issue #108.
TAutomapWindow::~TAutomapWindow()
{
	CheckClosed("TAutomapWindow");
}

// The destructor's work until issue #108 (0x426c40): the zoom goes back into
// the preferences, the hotkeys are removed; then the children.
void TAutomapWindow::OnClose()
{
	try {
		SetAutomapZoom(Zoom);
		RemoveHotkey(0x912);
		RemoveHotkey(0x913);
		RemoveHotkey(0x914);
		RemoveHotkey(0x915);
		RemoveHotkey(0x90c);
		RemoveHotkey(0x90d);
		RemoveHotkey(0x90e);
		RemoveHotkey(0x90f);
	} catch (...) {
		CloseWhileUnwinding([this] { TContentWindow::OnClose(); });
		throw;
	}
	TContentWindow::OnClose();
}

// 0x426e40, line 418
bool TAutomapWindow::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(10);
	SetHelpSize(Width, Height);
	return true;
}

// 0x426fe0, line 433. The text of the mark under the mouse. Odd, kept: a
// mark is taken only when both of its coordinates are not 0, and the buffer
// is printed to stdout every time (a debugging leftover; printf("%s\n"), since
// issue #64 compat/print.h's PrintLine, the same bytes).
void TAutomapWindow::MouseHover(int x, int y)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	TMark Mark = View->GetMark(x - View->OffsetX, y - View->OffsetY);
	if (Mark.x != 0 && Mark.y != 0) {
		const int Length = (int)strlen(Mark.Text);
		if (Length != 0) {
			CopyString(GUI.AutomapMouseOverText.Chars, Mark.Text);
			SetMouseOverText(GUI.AutomapMouseOverText);
		}
	}
	patella::compat::PrintLine("{}", static_cast<const char *>(GUI.AutomapMouseOverText));
}

// 0x427200, line 465. The frame around the view: skins 9 (top), 11 (left),
// 10 (right) and 8 (bottom) along the edges of Data224.
void TAutomapWindow::Arrange()
{
	TContentWindow::Arrange();
	Changed = true;
	DrawingList[0] = TDrawing(9, SkinProperties().Data224.x, SkinProperties().Data224.y, SkinProperties().Data224.width,
		Skins()[9].height, 0, 0);
	DrawingList[1] = TDrawing(11, SkinProperties().Data224.x, SkinProperties().Data224.y, Skins()[11].width,
		SkinProperties().Data224.height, 0, 0);
	DrawingList[2] = TDrawing(10, SkinProperties().Data224.x - 1 + SkinProperties().Data224.width,
		SkinProperties().Data224.y, Skins()[10].width, SkinProperties().Data224.height, 0, 0);
	DrawingList[3] = TDrawing(8, SkinProperties().Data224.x, SkinProperties().Data224.height - 1 + SkinProperties().Data224.y,
		SkinProperties().Data224.width, Skins()[8].height, 0, 0);
}

// 0x436ea0, line 523. The jump table 0x43716c (byte table 0x437194, Event-3)
// has the cases 3, 4 and 106-112. A scrolled view leaves the player first
// (the view's centre is fixed where it is).
void TAutomapWindow::HandleEvent(int Event)
{
	switch (Event) {
	case 106:
		Centred = true;
		Changed = true;
		break;
	case 3:
		if (Centred) {
			GetMapPosition(&ViewX, &ViewY, &ViewZ);
			GetMapPosition(&PlayerX, &PlayerY, &PlayerZ);
			Centred = false;
		}
		if (ViewZ > 0)
			ViewZ--;
		Changed = true;
		break;
	case 4:
		if (Centred) {
			GetMapPosition(&ViewX, &ViewY, &ViewZ);
			GetMapPosition(&PlayerX, &PlayerY, &PlayerZ);
			Centred = false;
		}
		if (ViewZ < 15)
			ViewZ++;
		Changed = true;
		break;
	case 107:
		if (Zoom < 8) {
			Zoom *= 2;
			Changed = true;
		}
		break;
	case 108:
		if (Zoom > 1) {
			Zoom /= 2;
			Changed = true;
		}
		break;
	case 109:
		Scroll(1);
		break;
	case 111:
		Scroll(2);
		break;
	case 112:
		Scroll(3);
		break;
	case 110:
		Scroll(4);
		break;
	default:
		break;
	}
}

// 0x4274c0, line 550. 0 centres the view; 1-4 move it by 24 pixels (north,
// south, west, east; the jump table 0x4276e0 has 1-4).
void TAutomapWindow::Scroll(int Direction)
{
	if (Direction == 0) {
		Centred = true;
		Changed = true;
		return;
	}
	if (Centred) {
		GetMapPosition(&ViewX, &ViewY, &ViewZ);
		GetMapPosition(&PlayerX, &PlayerY, &PlayerZ);
		Centred = false;
	}
	switch (Direction) {
	case 1:
		ViewY += -24 / Zoom;
		break;
	case 2:
		ViewY += 24 / Zoom;
		break;
	case 3:
		ViewX += -24 / Zoom;
		break;
	case 4:
		ViewX += 24 / Zoom;
		break;
	default:
		break;
	}
	Changed = true;
}

// --- TAutomapView -----------------------------------------------------------------

// 0x4276f0, line 652. The map is rendered into the cache surface 5 only when
// it changed (or the surface was lost): 106x106 pixels from the automap
// colours, a white cross at the player's position and the marks of the four
// 256x256 blocks around the view (skins 74 and up, 11x11); then the surface
// is copied to the screen. Odd, kept: at zoom 1 the centre is rounded down
// to even coordinates once per pixel, inside the loop. MSVC's analysis (C6262, issue #111): the pixels' buffer
// on the stack, the original's.
PATELLA_MSVC_WARNING(suppress : 6262)
void TAutomapView::Draw(int Surface, int x, int y)
{
	TAutomapWindow *Window = static_cast<TAutomapWindow *>(GetParent());
	int PosX, PosY, PosZ;
	GetMapPosition(&PosX, &PosY, &PosZ);
	if (!Window->Centred) {
		if (PosX == Window->PlayerX && PosY == Window->PlayerY && PosZ == Window->PlayerZ) {
			PosX = Window->ViewX;
			PosY = Window->ViewY;
			PosZ = Window->ViewZ;
		} else {
			Window->Centred = true;
			Window->Changed = true;
		}
	}
	if (GetAutomapChanged() || Window->Changed || GetSurface()->SurfaceRecreated(0)) {
		std::array<uint8, 106 * 106 * 3> Pixels;
		for (int i = 0; i < 106; i++) {
			for (int j = 0; j < 106; j++) {
				if (Window->Zoom == 1) {
					PosX = PosX / 2 * 2;
					PosY = PosY / 2 * 2;
				}
				const int Zoom = Window->Zoom;
				const TRGBColor Color = GetAutomapColor((Zoom * PosX + Zoom / 2 - 106 + i * 2) / Zoom,
					(Zoom * PosY + Zoom / 2 - 106 + j * 2) / Zoom, PosZ);
				Pixels[(j * 106 + i) * 3] = (uint8)Color.R;
				Pixels[(j * 106 + i) * 3 + 1] = (uint8)Color.G;
				Pixels[(j * 106 + i) * 3 + 2] = (uint8)Color.B;
			}
		}
		GetSurface()->WriteImage(5, 0, 0, 106, 106, Pixels.data());
		int CrossX = 53;
		int CrossY = 53;
		bool SameLevel = true;
		bool Visible = true;
		if (!Window->Centred) {
			GetMapPosition(&PosX, &PosY, &PosZ);
			CrossX = (PosX - Window->ViewX) * Window->Zoom / 2 + 53;
			CrossY = (PosY - Window->ViewY) * Window->Zoom / 2 + 53;
			if (PosZ != Window->ViewZ)
				SameLevel = false;
			Visible = CrossX >= 0 && CrossX <= 105 && CrossY >= 0 && CrossY <= 105 && SameLevel;
		}
		if (Visible) {
			if (Window->Zoom <= 2) {
				GetSurface()->SetPixel(5, CrossX, CrossY, TRGBColor(255, 255, 255));
				GetSurface()->SetPixel(5, CrossX + 1, CrossY, TRGBColor(255, 255, 255));
				GetSurface()->SetPixel(5, CrossX - 1, CrossY, TRGBColor(255, 255, 255));
				GetSurface()->SetPixel(5, CrossX, CrossY + 1, TRGBColor(255, 255, 255));
				GetSurface()->SetPixel(5, CrossX, CrossY - 1, TRGBColor(255, 255, 255));
			} else {
				for (int i = -3; i < 3; i++) {
					GetSurface()->SetPixel(5, CrossX + i, CrossY, TRGBColor(255, 255, 255));
					GetSurface()->SetPixel(5, CrossX + i, CrossY - 1, TRGBColor(255, 255, 255));
					GetSurface()->SetPixel(5, CrossX, CrossY + i, TRGBColor(255, 255, 255));
					GetSurface()->SetPixel(5, CrossX - 1, CrossY + i, TRGBColor(255, 255, 255));
				}
			}
		}
		int CentreX = PosX;
		int CentreY = PosY;
		if (!Window->Centred) {
			PosZ = Window->ViewZ;
			CentreX = Window->ViewX;
			CentreY = Window->ViewY;
		}
		const int Left = CentreX - 106 / Window->Zoom;
		const int Top = CentreY - 106 / Window->Zoom;
		for (int BlockX = Left / 256; BlockX < Left / 256 + 2; BlockX++) {
			for (int BlockY = Top / 256; BlockY < Top / 256 + 2; BlockY++) {
				std::vector<TMark> Marks; // the original's vector<TMark>(0, 0, 10) (issue #100)
				int Count;
				GetAutomapMarks(BlockX, BlockY, PosZ, &Marks, &Count);
				for (int i = 0; i < Count; i++) {
					const TMark Mark = Marks[static_cast<std::size_t>(i)];
					if (Mark.x != 0 || Mark.y != 0) {
						const int MarkX = (Mark.x - Left) * Window->Zoom / 2;
						const int MarkY = (Mark.y - Top) * Window->Zoom / 2;
						if (MarkX >= 0 && MarkY >= 0 && MarkX < 106 && MarkY < 106)
							DrawSkinClipped(5, MarkX - 5, MarkY - 5, 11, 11, Mark.Type + 74, 0, 0, 106, 106, 0, 0);
					}
				}
			}
		}
		Window->Changed = false;
	}
	GetSurface()->BlitRect(Surface, x, y, 5, TRect(0, 0, 106, 106));
}

// 0x427e40, line 696. A click walks to the mark under the mouse, else to the
// point clicked.
bool TAutomapView::LeftButtonUp(int x, int y, [[maybe_unused]] int Flags)
{
	TAutomapWindow *Window = static_cast<TAutomapWindow *>(GetParent());
	TMark Mark = GetMark(x, y);
	if (Window->Centred)
		GetMapPosition(&Window->ViewX, &Window->ViewY, &Window->ViewZ);
	if (Mark.x == 0 || Mark.y == 0) {
		Mark.x = (x - Width / 2) * 2 / Window->Zoom + Window->ViewX;
		Mark.y = (y - Height / 2) * 2 / Window->Zoom + Window->ViewY;
	}
	const int Result = SetPlayerDestination(Mark.x, Mark.y, Window->ViewZ, false, false);
	if (Result == 1)
		SendWalk();
	else if (Result == 2)
		SendStop();
	return true;
}

// 0x428080, line 732. The mark menu (Control.cpp's message 59) gets the
// point clicked and the mark under the mouse.
bool TAutomapView::RightButtonUp(int x, int y, [[maybe_unused]] int Flags)
{
	TMarkMenuData *Data = new TMarkMenuData;
	TAutomapWindow *Window = static_cast<TAutomapWindow *>(GetParent());
	Data->Window = Window;
	if (Window->Centred)
		GetMapPosition(&Window->ViewX, &Window->ViewY, &Window->ViewZ);
	Data->Coordinate.x = (x - Width / 2) * 2 / Window->Zoom + Window->ViewX;
	Data->Coordinate.y = (y - Height / 2) * 2 / Window->Zoom + Window->ViewY;
	Data->Coordinate.z = Window->ViewZ;
	Data->Mark = GetMark(x, y);
	AddMessage(59, reinterpret_cast<std::intptr_t>(Data), 0);
	return true;
}

// 0x4282e0, line 784. The last mark of the four blocks that lies within 5
// pixels of x, y; an empty mark if there is none.
TMark TAutomapView::GetMark(int x, int y)
{
	TAutomapWindow *Window = static_cast<TAutomapWindow *>(GetParent());
	if (Window->Centred)
		GetMapPosition(&Window->ViewX, &Window->ViewY, &Window->ViewZ);
	const int Left = Window->ViewX - 106 / Window->Zoom;
	const int Top = Window->ViewY - 106 / Window->Zoom;
	for (int BlockX = Left / 256; BlockX < Left / 256 + 2; BlockX++) {
		for (int BlockY = Top / 256; BlockY < Top / 256 + 2; BlockY++) {
			std::vector<TMark> Marks; // the original's vector<TMark>(0, 0, 10) (issue #100)
			int Count;
			GetAutomapMarks(BlockX, BlockY, Window->ViewZ, &Marks, &Count);
			for (int i = Count - 1; i >= 0; i--) {
				TMark Mark = Marks[static_cast<std::size_t>(i)];
				if (Mark.x != 0 || Mark.y != 0) {
					const int MarkX = (Mark.x - Left) * Window->Zoom / 2;
					const int MarkY = (Mark.y - Top) * Window->Zoom / 2;
					if (MarkX >= 0 && MarkY >= 0 && MarkX < 106 && MarkY < 106 && abs(x - MarkX) < 6
						&& abs(y - MarkY) < 6)
						return Mark;
				}
			}
		}
	}
	return TMark();
}

// --- TCompassRose -----------------------------------------------------------------

// 0x428680, line 791. No try block; the assertion is the only EH state.
TCompassRose::TCompassRose(int _RoseSkin)
{
	PATELLA_ASSERT_VALUES(_RoseSkin<patella::bridge::Skins().NumberOfSkins, _RoseSkin, patella::bridge::Skins().NumberOfSkins);
	RoseSkin = _RoseSkin;
}

// 0x428800, line 803
void TCompassRose::Draw(int Surface, int x, int y)
{
	DrawPicturePart(Surface, x, y, 4, Skins()[RoseSkin].x, Skins()[RoseSkin].y, Skins()[RoseSkin].width,
		Skins()[RoseSkin].height);
}

// 0x4289c0, line 825. The middle (10x10 pixels) centres the view, the four
// arms scroll it (Scroll's directions); the later tests win.
bool TCompassRose::LeftButtonUp(int x, int y, [[maybe_unused]] int Flags)
{
	int Direction = -1;
	if (Width / 2 - 5 <= x && x <= Width / 2 + 5 && Height / 2 - 5 <= y && y <= Height / 2 + 5)
		Direction = 0;
	if (Width / 2 - 5 <= x && x <= Width / 2 + 5 && 0 <= y && y <= Height / 2 - 5)
		Direction = 1;
	if (Width / 2 - 5 <= x && x <= Width / 2 + 5 && Height / 2 + 5 <= y && y <= Height)
		Direction = 2;
	if (0 <= x && x <= Width / 2 - 5 && Height / 2 - 5 <= y && y <= Height / 2 + 5)
		Direction = 3;
	if (Width / 2 + 5 <= x && x <= Width && Height / 2 - 5 <= y && y <= Height / 2 + 5)
		Direction = 4;
	if (Direction != -1)
		(static_cast<TAutomapWindow *>(GetParent()))->Scroll(Direction);
	return true;
}

// 0x428cb0, line 836
void TCompassRose::MouseHover([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetMouseOverText(TextScrollAutomap);
}

// --- TCapacityLabel, TSoulLabel ---------------------------------------------------

// 0x428e40, line 858
void TCapacityLabel::Draw(int Surface, int x, int y)
{
	TFixedString<20> Text;
	PrintFormattedText(Surface, SkinProperties().Style3E0.Data00.x + x, SkinProperties().Style3E0.Data00.y + y,
		SkinProperties().Style3E0.Data10, "Cap:");
	Text.Format("{}", GetPlayerCapacity());
	PrintFormattedText(Surface, SkinProperties().Style3E0.Data24.x + x, SkinProperties().Style3E0.Data24.y + y,
		SkinProperties().Style3E0.Data34, Text);
}

// 0x429090, line 875
void TSoulLabel::Draw(int Surface, int x, int y)
{
	TFixedString<20> Text;
	PrintFormattedText(Surface, SkinProperties().Style428.Data00.x + x, SkinProperties().Style428.Data00.y + y,
		SkinProperties().Style428.Data10, "Soul:");
	Text.Format("{}", GetPlayerSoul());
	PrintFormattedText(Surface, SkinProperties().Style428.Data24.x + x, SkinProperties().Style428.Data24.y + y,
		SkinProperties().Style428.Data34, Text);
}

// --- TPlayerStates ----------------------------------------------------------------

// 0x4292e0, line 901. The try block starts after the base class. The small
// row has a background (skin 100) with a right edge (101).
TPlayerStates::TPlayerStates(bool islarge)
{
	IsLarge = islarge;
	if (!islarge) {
		MaxIcons = 9;
		DrawingListLength = 2;
		DrawingList.reset(new TDrawing[2]);
		DrawingList[0] = TDrawing(100, 0, 0, SkinProperties().Data580.width, SkinProperties().Data580.height, 0, 0);
		DrawingList[1] = TDrawing(101, SkinProperties().Data580.width - 1, 0, Skins()[101].width, Skins()[101].height, 0,
			0);
	} else {
		MaxIcons = 10;
		DrawingListLength = 1;
		DrawingList.reset(new TDrawing[1]);
		DrawingList[0] = TDrawing(100, 0, 0, SkinProperties().Style288.Data148.width,
			SkinProperties().Style288.Data148.height, 0, 0);
	}
}

// 0x437210, lines 920-925. The icons side by side, PlayerStates.Data24
// pixels apart.
void TPlayerStates::Draw(int Surface, int x, int y)
{
	TComponent::Draw(Surface, x, y);
	const int Icons = GetNumberOfIcons();
	for (int i = 0; i < Icons; i++) {
		int destx = SkinProperties().PlayerStates.Data24 * i;
		int desty;
		if (IsLarge) {
			destx += SkinProperties().PlayerStates.Data00.x;
			desty = SkinProperties().PlayerStates.Data00.y;
		} else {
			destx += SkinProperties().PlayerStates.Data10.x;
			desty = SkinProperties().PlayerStates.Data10.y;
		}
		const int Skin = GetIcon(i);
		PATELLA_ASSERT_VALUES(x+destx>=0, x+destx);
		PATELLA_ASSERT_VALUES(y+desty>=0, y+desty);
		if (Skin != 0)
			DrawSkin(Surface, x + destx, y + desty, Skins()[Skin].width, Skins()[Skin].height, Skin, 0, 0);
	}
}

// 0x4295b0, line 947
int TPlayerStates::GetNumberOfIcons() const
{
	int Count = IsPlayerPoisoned() ? 1 : 0;
	if (IsPlayerBurning())
		Count++;
	if (IsPlayerManaShielded())
		Count++;
	if (IsPlayerParalyzed())
		Count++;
	if (HasPlayerSkull())
		Count++;
	if (IsPlayerInFight())
		Count++;
	if (IsPlayerDrunk())
		Count++;
	if (IsPlayerElectrified())
		Count++;
	if (IsPlayerHasted())
		Count++;
	if (Count > MaxIcons)
		Count = MaxIcons;
	return Count;
}

// 0x437630, lines 973-976. The jump table 0x4378f8 has the skins 228-239.
void TPlayerStates::MouseHover(int x, int y)
{
	const int Position = GetIconAt(x, y);
	if (Position != -1) {
		const int skin = GetIcon(Position);
		if (skin != 0) {
			const char *Text;
			switch (skin) {
			case 228:
				Text = TextPoisoned;
				break;
			case 229:
				Text = TextOnFire;
				break;
			case 230:
				Text = TextStruckByEnergy;
				break;
			case 231:
				Text = TextDrunk;
				break;
			case 232:
				Text = TextMagicShield;
				break;
			case 233:
				Text = TextParalysed;
				break;
			case 234:
				Text = TextHasted;
				break;
			case 235:
				Text = TextNoLogoutInFight;
				break;
			case 236:
				Text = TextMayBeAttacked;
				break;
			case 237:
				Text = TextPartyMember;
				break;
			case 238:
				Text = TextAttackedUnmarked;
				break;
			case 239:
				Text = TextKilledTooMany;
				break;
			default:
				TERROR1("unknown skin", skin);
			}
			SetMouseOverText(Text);
		}
	}
}

// 0x4297a0, line 995. The icon under x, y; -1 for none. Each icon area is
// one pixel larger than the distance.
int TPlayerStates::GetIconAt(int x, int y) const
{
	const int Icons = GetNumberOfIcons();
	for (int i = 0; i < Icons; i++) {
		int IconX = SkinProperties().PlayerStates.Data10.x;
		int IconY = SkinProperties().PlayerStates.Data10.y;
		if (IsLarge) {
			IconX = SkinProperties().PlayerStates.Data00.x;
			IconY = SkinProperties().PlayerStates.Data00.y;
		}
		IconX = SkinProperties().PlayerStates.Data24 * i + IconX;
		if (IconX <= x && x <= IconX + SkinProperties().PlayerStates.Data24 && IconY <= y
			&& y <= IconY + SkinProperties().PlayerStates.Data24)
			return i;
	}
	return -1;
}

// 0x429990, lines 1036-1070. The assertion comes before the try block. The
// skin of the Position'th state that is on, in the order of the jump
// table 0x429e78 (0-9; the skull has its own table 0x429ea0, 1-4); 0 when fewer
// states are on. State 9 has no icon. The original's default, never reached
// (i is 0-9), was TERROR1(1063, "index out of range", i); it does nothing
// here (issue #79): MSVC x86 removed its code but kept the function's exception
// frame, without the frame's handler in the SafeSEH table, so an exception of
// a callee ended the program (notes/decisions.md, issue #79).
int TPlayerStates::GetIcon(int Position)
{
	PATELLA_ASSERT_VALUES(In(Position,0,SkinProperties().PlayerStates.NumberOfIcons-1), Position,
		SkinProperties().PlayerStates.NumberOfIcons);
	int Skin = 0;
	int Found = 0;
	for (int i = 0; i <= 9 && Found <= Position; i++) {
		Skin = 0;
		switch (i) {
		case 0:
			if (IsPlayerPoisoned()) {
				Found++;
				Skin = 228;
			}
			break;
		case 1:
			if (IsPlayerBurning()) {
				Found++;
				Skin = 229;
			}
			break;
		case 2:
			if (IsPlayerManaShielded()) {
				Found++;
				Skin = 232;
			}
			break;
		case 3:
			if (IsPlayerParalyzed()) {
				Found++;
				Skin = 233;
			}
			break;
		case 4:
			switch (GetCreature(GetPlayerID())->PKFlag) {
			case 1:
				Found++;
				Skin = 236;
				break;
			case 2:
				Found++;
				Skin = 237;
				break;
			case 3:
				Found++;
				Skin = 238;
				break;
			case 4:
				Found++;
				Skin = 239;
				break;
			default:
				break;
			}
			break;
		case 5:
			if (IsPlayerInFight()) {
				Found++;
				Skin = 235;
			}
			break;
		case 6:
			if (IsPlayerDrunk()) {
				Found++;
				Skin = 231;
			}
			break;
		case 7:
			if (IsPlayerElectrified()) {
				Found++;
				Skin = 230;
			}
			break;
		case 8:
			if (IsPlayerHasted()) {
				Found++;
				Skin = 234;
			}
			break;
		case 9:
			Skin = 0;
			break;
		default: // never reached (i is 0-9): the original's TERROR1, see above
			break;
		}
	}
	return Skin;
}

// --- TSmallInventoryWindow --------------------------------------------------------

// 0x429eb0, line 1083. The try block starts after the base class.
TSmallInventoryWindow::TSmallInventoryWindow()
	: TContentWindow(2, true)
{
	DrawingListLength = 1;
	DrawingList.reset(new TDrawing[1]);
	DrawingList[0] = TDrawing(99, SkinProperties().Data590.x, SkinProperties().Data590.y, SkinProperties().Data590.width,
		SkinProperties().Data590.height, 0, 0);
}

// 0x42a0c0, line 1099. A white frame around the drop area while an object
// is dragged over it.
void TSmallInventoryWindow::Draw(int Surface, int x, int y)
{
	TContentWindow::Draw(Surface, x, y);
	if (In(GetMouseX() - SkinProperties().Data590.x - x, 0, SkinProperties().Data590.width - 1)
		&& In(GetMouseY() - SkinProperties().Data590.y - y, 0, SkinProperties().Data590.height - 1)
		&& GetActionState() == 6)
		GetSurface()->DrawFrame(Surface,
			TRect(SkinProperties().Data590.x + x, SkinProperties().Data590.y + y,
				SkinProperties().Data590.width, SkinProperties().Data590.height),
			1, TRGBColor(255, 255, 255));
}

// 0x42a300, line 1127. An object dropped on the area goes into the
// inventory (0xffff, 0, 0: the first free slot); a pile asks for the count
// unless Ctrl (0x400, all) or Shift (0x200, one) is held.
bool TSmallInventoryWindow::LeftButtonUp(int x, int y, int Flags)
{
	const bool Result = TContentWindow::LeftButtonUp(x, y, Flags);
	if (In(x - SkinProperties().Data590.x, 0, SkinProperties().Data590.width - 1)
		&& In(y - SkinProperties().Data590.y, 0, SkinProperties().Data590.height - 1) && GetActionState() == 6) {
		int Index, SrcX, SrcY, SrcZ;
		const TObject *Obj = GetDragObject(&Index, &SrcX, &SrcY, &SrcZ);
		if (!GetObjectFlag(Obj->Type, CUMULATIVE) || Obj->Data < 2 || (Flags & 0x200) != 0) {
			SendMoveObject(SrcX, SrcY, SrcZ, Obj->Type, Index, 0xffff, 0, 0, 1);
		} else if ((Flags & 0x400) == 0) {
			SetDragTarget(0xffff, 0, 0);
			AddMessage(13, 0, 0);
		} else {
			SendMoveObject(SrcX, SrcY, SrcZ, Obj->Type, Index, 0xffff, 0, 0, Obj->Data);
		}
	}
	return Result;
}

// 0x42a590, line 1141
bool TSmallInventoryWindow::HelpMouseMove(int x, int y)
{
	if (!TWindow::HelpMouseMove(x, y)) {
		SetHelpHandle(13);
		SetHelpSize(Width, Height);
	}
	return true;
}

// --- TInventoryView ---------------------------------------------------------------

// 0x42a750, line 1170. The try block starts after the base class. The
// "minimise" button is event 23 of the main window.
TInventoryView::TInventoryView(TWindow *Receiver)
{
	SetSize(SkinProperties().Style288.Data00, SkinProperties().Style288.Data04);
	TSkinButton *Minimise = new TSkinButton(0, 0, 138, 139, TCallback(Receiver, 23), TextMinimiseInventory, false);
	InsertChild(Minimise);
	SetChildPosition(Minimise, SkinProperties().Style288.Data08.x, SkinProperties().Style288.Data08.y);
	TCapacityLabel *Capacity = new TCapacityLabel;
	InsertChild(Capacity);
	SetChildArea(Capacity, SkinProperties().Style288.Data128);
	TSoulLabel *Soul = new TSoulLabel;
	InsertChild(Soul);
	SetChildArea(Soul, SkinProperties().Style288.Data138);
	TPlayerStates *States = new TPlayerStates(true);
	InsertChild(States);
	SetChildArea(States, SkinProperties().Style288.Data148);
	Arrange();
}

// 0x42aa90, lines 1181-1209. The backgrounds of capacity and soul (skin 98),
// the children, then the ten slots: a frame (skin 102), the object or the
// slot's empty picture (skins Style288.Data100), and a white frame while an
// object is dragged over a slot.
void TInventoryView::Draw(int Surface, int x, int y)
{
	int destx = SkinProperties().Style288.Data128.x + x;
	int desty = SkinProperties().Style288.Data128.y + y;
	PATELLA_ASSERT_VALUES(destx>=0, destx);
	PATELLA_ASSERT_VALUES(desty>=0, desty);
	DrawSkin(Surface, destx, desty, SkinProperties().Style288.Data128.width, SkinProperties().Style288.Data128.height, 98,
		0, 0);
	destx = SkinProperties().Style288.Data138.x + x;
	desty = y + SkinProperties().Style288.Data138.y;
	PATELLA_ASSERT_VALUES(destx>=0, destx);
	PATELLA_ASSERT_VALUES(desty>=0, desty);
	DrawSkin(Surface, destx, desty, SkinProperties().Style288.Data138.width, SkinProperties().Style288.Data138.height, 98,
		0, 0);
	TWindow::Draw(Surface, x, y);
	for (int i = 0; i < 10; i++) {
		const TObject Obj = *GetBodyObject(i + 1);
		DrawPicturePart(Surface, SkinProperties().Style288.Data60[i].x + x, SkinProperties().Style288.Data60[i].y + y, 4,
			Skins()[102].x, Skins()[102].y, Skins()[102].width, Skins()[102].height);
		const int ObjX = SkinProperties().Style288.Data10[i].x + x;
		const int ObjY = SkinProperties().Style288.Data10[i].y + y;
		if (Obj.Type == 0) {
			const int Empty = SkinProperties().Style288.Data100[i];
			DrawPicturePart(Surface, ObjX, ObjY, 4, Skins()[Empty].x, Skins()[Empty].y, Skins()[Empty].width,
				Skins()[Empty].height);
		} else {
			DrawObject(Surface, ObjX, ObjY, 32, Obj, TRGBColor(0, 0, 0), TRect(ObjX, ObjY, 32, 32),
				SkinProperties().Data498, false);
		}
		if (GetActionState() == 6 && In(GetMouseX(), ObjX, ObjX + 31) && In(GetMouseY(), ObjY, ObjY + 31))
			GetSurface()->DrawFrame(Surface, TRect(ObjX - 1, ObjY - 1, 34, 34), 1, TRGBColor(255, 255, 255));
	}
}

// 0x437930, line 1229. A press on a filled slot starts dragging its object.
bool TInventoryView::LeftButtonDown(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonDown(x, y, Flags)) {
		const int Slot = GetSlotAt(x, y);
		if (Slot == 0)
			return false;
		const TObject *Obj = GetBodyObject(Slot);
		if (Obj->Type != 0)
			SetDragObject(*Obj, 0, 0xffff, Slot, 0);
	}
	return true;
}

// 0x437b30, lines 1251-1277. Action state 7 uses the use object with the
// slot's object, 8 asks for a player, 6 drops the dragged object into the
// slot (a pile asks for the count unless Ctrl or Shift is held); a release
// with flags is a right click.
bool TInventoryView::LeftButtonUp(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonUp(x, y, Flags)) {
		const int Slot = GetSlotAt(x, y);
		if (Slot == 0)
			return false;
		if (GetActionState() == 7) {
			const TObject *Obj = GetBodyObject(Slot);
			if (Obj->Type != 0) {
				int SrcIndex, SrcX, SrcY, SrcZ;
				const TObject &SrcObj = *GetUseObject(&SrcIndex, &SrcX, &SrcY, &SrcZ);
				PATELLA_ASSERT(SrcObj!=0);
				PATELLA_ASSERT(SrcObj.Type!=CREATURE);
				SendUseObjectWith(SrcX, SrcY, SrcZ, SrcObj.Type, SrcIndex, 0xffff, Slot, 0, Obj->Type, 0);
				return true;
			}
		} else if (GetActionState() == 8) {
			SetStatusMessage("Select a player to trade with.", false);
			return true;
		} else if (GetActionState() == 6) {
			int DragIndex, DragX, DragY, DragZ;
			const TObject *DragObject = GetDragObject(&DragIndex, &DragX, &DragY, &DragZ);
			if (DragX != 0xffff || DragY != Slot) {
				if (!GetObjectFlag(DragObject->Type, CUMULATIVE) || DragObject->Data < 2 || (Flags & 0x200) != 0) {
					SendMoveObject(DragX, DragY, DragZ, DragObject->Type, DragIndex, 0xffff, Slot, 0, 1);
				} else if ((Flags & 0x400) == 0) {
					SetDragTarget(0xffff, Slot, 0);
					AddMessage(13, 0, 0);
					return true;
				} else {
					SendMoveObject(DragX, DragY, DragZ, DragObject->Type, DragIndex, 0xffff, Slot, 0,
						DragObject->Data);
				}
			}
		} else if (Flags != 0) {
			return RightButtonUp(x, y, Flags);
		}
	}
	return true;
}

// 0x437fe0, line 1329. The trade and use objects become the slot's object.
// Shift looks at it; with classic controls Ctrl opens the menu (message 52)
// and a plain click uses it, otherwise the other way round. An object used
// with something (MULTIUSE) starts action state 7 (message 44).
bool TInventoryView::RightButtonUp(int x, int y, int Flags)
{
	if (!TWindow::RightButtonUp(x, y, Flags)) {
		const int Slot = GetSlotAt(x, y);
		if (Slot == 0)
			return false;
		SetTradeObject(TObject(), 0, 0, 0, 0);
		SetUseObject(TObject(), 0, 0, 0, 0);
		SetTradeInfo(0);
		const TObject *Obj = GetBodyObject(Slot);
		SetTradeObject(*Obj, 0, 0xffff, Slot, 0);
		SetUseObject(*Obj, 0, 0xffff, Slot, 0);
		if (Obj->Type != 0) {
			if (GetClassicControl()) {
				if ((Flags & 0x200) != 0)
					SendLook(0xffff, Slot, 0, Obj->Type, 0);
				else if ((Flags & 0x400) != 0)
					AddMessage(52, 0, 0);
				else if (GetObjectFlag(Obj->Type, MULTIUSE))
					AddMessage(44, 0, 0);
				else
					SendUseObject(0xffff, Slot, 0, Obj->Type, 0, GetFreeContainer());
			} else {
				if ((Flags & 0x200) != 0)
					SendLook(0xffff, Slot, 0, Obj->Type, 0);
				else if ((Flags & 0x400) == 0)
					AddMessage(52, 0, 0);
				else if (GetObjectFlag(Obj->Type, MULTIUSE))
					AddMessage(44, 0, 0);
				else
					SendUseObject(0xffff, Slot, 0, Obj->Type, 0, GetFreeContainer());
			}
		}
	}
	return true;
}

// 0x42b1b0, lines 1339-1344
bool TInventoryView::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(12);
	const TComponent *Parent = GetParent();
	PATELLA_ASSERT(Parent!=nullptr);
	SetHelpSize(Parent->Width, Parent->Height);
	MoveHelpPosition(-OffsetX, -OffsetY);
	return true;
}

// 0x42b450. The body slot (1-10) whose area (Style288.Data60) holds x, y;
// 0 for none. The try block was dropped (nothing in it can throw).
int TInventoryView::GetSlotAt(int x, int y)
{
	for (int i = 0; i < 10; i++) {
		const TRect &Area = SkinProperties().Style288.Data60[i];
		if (x - Area.x >= 0 && x - Area.x <= Area.width && y - Area.y >= 0 && y - Area.y <= Area.height)
			return i + 1;
	}
	return 0;
}

// --- TCombatControls --------------------------------------------------------------

// 0x42b4d0, line 1425. The try block starts after the base class. The
// layout (Style4AC for the large controls, Style4FC for the small ones) is
// copied first. The radio groups have no selection until the modes are read
// at the end (none for an unknown mode).
TCombatControls::TCombatControls(TWindow *Receiver, bool Large)
{
	IsLarge = Large;
	const TCombatControlsProperties Layout = Large ? SkinProperties().Style4AC : SkinProperties().Style4FC;
	AttackModes.reset(new TRadioButtonGroup);
	ChaseModes.reset(new TRadioButtonGroup);
	SetSize(Layout.Width, Layout.Height);
	TSkinButton *Offensive = new TSkinButton(Layout.Buttons[0].x, Layout.Buttons[0].y, 103, 104, TCallback(this, 100),
		TextOffensiveFighting, false);
	Offensive->SetRadioButtonGroup(AttackModes.get());
	InsertChild(Offensive);
	TSkinButton *Balanced = new TSkinButton(Layout.Buttons[1].x, Layout.Buttons[1].y, 105, 106, TCallback(this, 101),
		TextBalancedFighting, false);
	Balanced->SetRadioButtonGroup(AttackModes.get());
	InsertChild(Balanced);
	TSkinButton *Defensive = new TSkinButton(Layout.Buttons[2].x, Layout.Buttons[2].y, 107, 108, TCallback(this, 102),
		TextDefensiveFighting, false);
	Defensive->SetRadioButtonGroup(AttackModes.get());
	InsertChild(Defensive);
	StandButton = new TSkinButton(Layout.Buttons[3].x, Layout.Buttons[3].y, 109, 110, TCallback(this, 103),
		TextStandWhileFighting, false);
	StandButton->SetRadioButtonGroup(ChaseModes.get());
	InsertChild(StandButton);
	ChaseButton = new TSkinButton(Layout.Buttons[4].x, Layout.Buttons[4].y, 111, 112, TCallback(this, 104),
		TextChaseOpponent, false);
	ChaseButton->SetRadioButtonGroup(ChaseModes.get());
	InsertChild(ChaseButton);
	SecureButton = new TToggleButton(Layout.Buttons[5].x, Layout.Buttons[5].y, 113, 114, TCallback(this, 105),
		TextCannotAttackUnmarked, TextCanAttackAny, false);
	InsertChild(SecureButton);
	InsertChild(new TTextButton(Layout.Buttons[6].x, Layout.Buttons[6].y, "Stop", TCallback(Receiver, 32),
		TextStopCurrentAction));
	if (!IsLarge) {
		InsertChild(new TSkinButton(Layout.Buttons[7].x, Layout.Buttons[7].y, 115, 116, TCallback(Receiver, 8),
			TextOpenOptions, false));
		InsertChild(new TSkinButton(Layout.Buttons[8].x, Layout.Buttons[8].y, 117, 118, TCallback(Receiver, 701),
			TextOpenHelpMenu, false));
	} else {
		InsertChild(new TTextButton(Layout.Buttons[7].x, Layout.Buttons[7].y, "Options", TCallback(Receiver, 8),
			TextOpenOptions));
		InsertChild(new TTextButton(Layout.Buttons[8].x, Layout.Buttons[8].y, "Help", TCallback(Receiver, 701),
			TextOpenHelpMenu));
	}
	switch (GetAttackMode()) {
	case 1:
		AttackModes->Selected = Offensive;
		break;
	case 2:
		AttackModes->Selected = Balanced;
		break;
	case 3:
		AttackModes->Selected = Defensive;
		break;
	default:
		break;
	}
	switch (GetChaseMode()) {
	case 0:
		ChaseModes->Selected = StandButton;
		break;
	case 1:
		ChaseModes->Selected = ChaseButton;
		break;
	default:
		break;
	}
	SecureButton->State = !GetSecureMode();
	Arrange();
}

// 0x42bb70, line 1450. The chase mode can change elsewhere (Escape):
// the selection follows it.
void TCombatControls::Draw(int Surface, int x, int y)
{
	switch (GetChaseMode()) {
	case 0:
		if (ChaseModes->Selected != StandButton)
			ChaseModes->Selected = StandButton;
		break;
	case 1:
		if (ChaseModes->Selected != ChaseButton)
			ChaseModes->Selected = ChaseButton;
		break;
	default:
		break;
	}
	TWindow::Draw(Surface, x, y);
}

// 0x42bd30, lines 1461-1466
bool TCombatControls::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	if (!IsLarge)
		SetHelpHandle(15);
	else
		SetHelpHandle(14);
	const TComponent *Parent = GetParent();
	PATELLA_ASSERT(Parent!=nullptr);
	SetHelpSize(Parent->Width, Parent->Height);
	MoveHelpPosition(-OffsetX, -OffsetY);
	return true;
}

// 0x42bff0, line 1502. The jump table 0x42c1c8 has 100-105; every change is
// sent to the server.
void TCombatControls::HandleEvent(int Event)
{
	switch (Event) {
	case 102:
		SetAttackMode(3);
		break;
	case 101:
		SetAttackMode(2);
		break;
	case 100:
		SetAttackMode(1);
		break;
	case 103:
		SetChaseMode(0);
		break;
	case 104:
		SetChaseMode(1);
		break;
	case 105:
		SetSecureMode(!SecureButton->State);
		break;
	default:
		return;
	}
	SendFightModes();
}

// --- TStatusWindow ----------------------------------------------------------------

// 0x438310, line 1522. The try block starts after the base class.
TStatusWindow::TStatusWindow()
	: TContentWindow(4, true)
{
	DrawingListLength = 4;
	DrawingList.reset(new TDrawing[4]);
	Data44 = 100;
	Data48 = 100;
	SetSize(SkinProperties().Data184, SkinProperties().StatusWindow.Data00);
	Arrange();
}

// 0x42c1e0, lines 1550-1559. The values, then the bars cut to the share of
// the maximum (full when the value is not below it).
void TStatusWindow::Draw(int Surface, int x, int y)
{
	TContentWindow::Draw(Surface, x, y);
	const int Health = GetPlayerHealth();
	const int MaxHealth = GetPlayerMaxHealth();
	const int Mana = GetPlayerMana();
	const int MaxMana = GetPlayerMaxMana();
	TFixedString<16> Text;
	Text.Format("{}", Health);
	PrintFormattedText(Surface, SkinProperties().StatusWindow.Data34.x + x, SkinProperties().StatusWindow.Data34.y + y,
		SkinProperties().StatusWindow.Data54, Text);
	Text.Format("{}", Mana);
	PrintFormattedText(Surface, SkinProperties().StatusWindow.Data44.x + x, SkinProperties().StatusWindow.Data44.y + y,
		SkinProperties().StatusWindow.Data54, Text);
	int BarWidth = SkinProperties().StatusWindow.HitpointsBar.width;
	if (Health < MaxHealth)
		BarWidth = SkinProperties().StatusWindow.HitpointsBar.width * Health / MaxHealth;
	PATELLA_ASSERT_VALUES(x+SkinProperties().StatusWindow.HitpointsBar.x>=0, x);
	PATELLA_ASSERT_VALUES(y+SkinProperties().StatusWindow.HitpointsBar.y>=0, y);
	DrawSkin(Surface, SkinProperties().StatusWindow.HitpointsBar.x + x, y + SkinProperties().StatusWindow.HitpointsBar.y,
		BarWidth, Skins()[121].height, 121, 0, 0);
	BarWidth = SkinProperties().StatusWindow.ManaBar.width;
	if (Mana < MaxMana)
		BarWidth = SkinProperties().StatusWindow.ManaBar.width * Mana / MaxMana;
	PATELLA_ASSERT_VALUES(x+SkinProperties().StatusWindow.ManaBar.x>=0, x);
	PATELLA_ASSERT_VALUES(y+SkinProperties().StatusWindow.ManaBar.y>=0, y);
	DrawSkin(Surface, SkinProperties().StatusWindow.ManaBar.x + x, SkinProperties().StatusWindow.ManaBar.y + y, BarWidth,
		Skins()[124].height, 124, 0, 0);
}

// 0x42c830, line 1571
bool TStatusWindow::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(11);
	SetHelpSize(Width, Height);
	return true;
}

// 0x42c9d0, line 1598. Two pictures (skins 119, 122) and the two bars' backgrounds
// (120, 123).
void TStatusWindow::Arrange()
{
	TContentWindow::Arrange();
	DrawingList[0] = TDrawing(119, SkinProperties().StatusWindow.Data04.x, SkinProperties().StatusWindow.Data04.y,
		Skins()[119].width, Skins()[119].height, 0, 0);
	DrawingList[1] = TDrawing(122, SkinProperties().StatusWindow.Data04.width, SkinProperties().StatusWindow.Data04.height,
		Skins()[122].width, Skins()[122].height, 0, 0);
	DrawingList[2] = TDrawing(120, SkinProperties().StatusWindow.HitpointsBar.x, SkinProperties().StatusWindow.HitpointsBar.y,
		SkinProperties().StatusWindow.HitpointsBar.width, SkinProperties().StatusWindow.HitpointsBar.height, 0, 0);
	DrawingList[3] = TDrawing(123, SkinProperties().StatusWindow.ManaBar.x, SkinProperties().StatusWindow.ManaBar.y,
		SkinProperties().StatusWindow.ManaBar.width, SkinProperties().StatusWindow.ManaBar.height, 0, 0);
}

// --- TContainerWindowBody ---------------------------------------------------------

// 0x438500, line 1618. The try block starts after the base class.
TContainerWindowBody::TContainerWindowBody(int _Container)
{
	Container = _Container;
	ScrollBar = new TScrollBar(TRect(0, 0, SkinProperties().Data870, SkinProperties().Data874), 0, 0, 0, 0, 10);
	InsertChild(ScrollBar);
	HasParent = false;
	Arrange();
}

// 0x42cc70, line 1628 (deleting destructor 0x438700). Its work is the close
// since issue #108.
TContainerWindowBody::~TContainerWindowBody()
{
	CheckClosed("TContainerWindowBody");
}

// The destructor's work until issue #108 (0x42cc70): closing the window
// closes the container; then the children.
void TContainerWindowBody::OnClose()
{
	try {
		SendCloseContainer(Container);
	} catch (...) {
		CloseWhileUnwinding([this] { TSubWindowBody::OnClose(); });
		throw;
	}
	TSubWindowBody::OnClose();
}

// 0x42ce10, lines 1644-1666. The background (skin 7, scrolled with the
// view), then the slots row by row, cut at the top and the bottom edge:
// the slot picture (skin 125), the object and, while an object is dragged
// over the slot, a white frame. Odd, kept: a slot cut at the top shows the
// top of its picture, not the part below the edge.
void TContainerWindowBody::Draw(int Surface, int x, int y)
{
	if (ObjectCount != GetContainerObjectCount(Container))
		Arrange();
	if (!IsMinimized()) {
		if (!Minimized)
			TWindow::Draw(Surface, x, y);
		const int ScrollY = ScrollBar->Range->Value;
		const int Bottom = Height - 1;
		PATELLA_ASSERT_VALUES(x>=0, x);
		PATELLA_ASSERT_VALUES(y>=0, y);
		DrawSkin(Surface, x, y, Width - ScrollBar->Width, Height, 7, 0, ScrollY % Skins()[7].height);
		if (IsContainerOpen(Container)) {
			for (int i = 0; i < Capacity; i++) {
				const int SlotX = i % SkinProperties().Data62C * SkinProperties().Data648;
				const int SlotY = i / SkinProperties().Data62C * SkinProperties().Data64C - ScrollY;
				int Top = SlotY + SkinProperties().Data630.y;
				int ClipHeight = SkinProperties().Data638.y;
				if (Top + ClipHeight > 0 && Top <= Bottom) {
					if (Top < 0) {
						ClipHeight = Top + ClipHeight;
						Top = 0;
					}
					if (ClipHeight + Top > Bottom)
						ClipHeight = Bottom - Top + 1;
					const int ScreenX = SkinProperties().Data630.x + SlotX + x;
					const int ScreenY = Top + y;
					const int SlotWidth = SkinProperties().Data638.x;
					DrawPicturePart(Surface, ScreenX, ScreenY, 4, Skins()[125].x, Skins()[125].y, SlotWidth, ClipHeight);
					if (i < ObjectCount)
						DrawObject(Surface, SkinProperties().Data638.width + SlotX + x,
							SkinProperties().Data638.height + SlotY + y, 32, GetContainerContent(Container, i),
							TRGBColor(0, 0, 0), TRect(ScreenX, ScreenY, SlotWidth, ClipHeight), SkinProperties().Data498,
							false);
					if (GetActionState() == 6 && In(GetMouseX(), ScreenX, ScreenX + SlotWidth - 1)
						&& In(GetMouseY(), ScreenY, ScreenY + ClipHeight - 1))
						GetSurface()->DrawFrameClipped(Surface,
							TRect(SkinProperties().Data630.x + SlotX + x,
								SlotY + SkinProperties().Data630.y + y, 34, 34),
							1, TRGBColor(255, 255, 255),
							TRect(ScreenX, ScreenY, SlotWidth, ClipHeight));
				}
			}
		}
	}
}

// 0x42d420, lines 1675-1683. A window taller than its content keeps its
// height but does not grow; the scroll bar stays at the right edge.
void TContainerWindowBody::SetSize(int Width, int Height)
{
	const int ScrollbarX = Width - ScrollBar->Width;
	PATELLA_ASSERT_VALUES(ScrollbarX>0, ScrollbarX);
	const int Maximum = GetMaximumHeight();
	if (Height > Maximum && this->Height < Height)
		Height = (std::max)(this->Height, Maximum);
	TWindow::SetSize(Width, Height);
	ScrollBar->SetPosition(ScrollbarX, 0);
	ScrollBar->SetSize(ScrollBar->Width, Height);
	Arrange();
}

// 0x42d6f0, line 1693
int TContainerWindowBody::getMinimumHeight()
{
	return (std::max)(ScrollBar->getMinimumHeight(), SkinProperties().Data650);
}

// 0x42d890, line 1712
int TContainerWindowBody::GetMaximumHeight()
{
	return GetContentHeight();
}

// 0x42da20, line 1744. The counts of the container, the scroll range and,
// in a window, the icon and title again.
void TContainerWindowBody::Arrange()
{
	ObjectCount = GetContainerObjectCount(Container);
	Capacity = GetContainerCapacity(Container);
	int ContentHeight = GetContentHeight();
	if (ContentHeight <= Height)
		ContentHeight = Height;
	ScrollBar->Range->SetRange(0, ContentHeight - Height, ScrollBar->Range->Value, Height);
	TSubWindow *Window = static_cast<TSubWindow *>(GetParent());
	if (Window != nullptr) {
		TObjectIcon *Icon = new TObjectIcon(GetContainerObject(Container), &SkinProperties().Style194.Data80, false);
		TLabel *Title = new TLabel(SkinProperties().Style194.Data38, GetContainerName(Container));
		Window->SetIcon(Icon);
		Window->SetTitle(Title);
	}
}

// 0x438720, line 1764. A press on a filled slot starts dragging its object
// (from 0xffff, 0x40 plus the container, the slot).
bool TContainerWindowBody::LeftButtonDown(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonDown(x, y, Flags)) {
		const int Position = GetSlotAt(x, y, true);
		if (Position != -1) {
			const TObject Obj = GetContainerContent(Container, Position);
			if (Obj.Type != 0)
				SetDragObject(Obj, Position, 0xffff, Container + 0x40, Position);
		}
	}
	return true;
}

// 0x438920, lines 1785-1812. As TInventoryView::LeftButtonUp for a
// container slot; a drop may go into an empty slot.
bool TContainerWindowBody::LeftButtonUp(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonUp(x, y, Flags)) {
		if (GetActionState() == 7) {
			const int Position = GetSlotAt(x, y, true);
			if (Position != -1) {
				const TObject Obj = GetContainerContent(Container, Position);
				if (Obj.Type != 0) {
					int SrcIndex, SrcX, SrcY, SrcZ;
					const TObject &SrcObj = *GetUseObject(&SrcIndex, &SrcX, &SrcY, &SrcZ);
					PATELLA_ASSERT(SrcObj!=0);
					PATELLA_ASSERT(SrcObj.Type!=CREATURE);
					SendUseObjectWith(SrcX, SrcY, SrcZ, SrcObj.Type, SrcIndex, 0xffff, Container + 0x40, Position,
						Obj.Type, Position);
				}
			}
		} else if (GetActionState() == 8) {
			SetStatusMessage("Select a player to trade with.", false);
		} else if (GetActionState() == 6) {
			const int Position = GetSlotAt(x, y, false);
			int DragIndex, DragX, DragY, DragZ;
			const TObject *DragObject = GetDragObject(&DragIndex, &DragX, &DragY, &DragZ);
			if (DragX != 0xffff || DragY != Container + 0x40 || DragZ != Position) {
				if (!GetObjectFlag(DragObject->Type, CUMULATIVE) || DragObject->Data < 2 || (Flags & 0x200) != 0) {
					SendMoveObject(DragX, DragY, DragZ, DragObject->Type, DragIndex, 0xffff, Container + 0x40, Position,
						1);
				} else if ((Flags & 0x400) == 0) {
					SetDragTarget(0xffff, Container + 0x40, Position);
					AddMessage(13, 0, 0);
					return true;
				} else {
					SendMoveObject(DragX, DragY, DragZ, DragObject->Type, DragIndex, 0xffff, Container + 0x40, Position,
						DragObject->Data);
				}
			}
		} else if (Flags != 0) {
			return RightButtonUp(x, y, Flags);
		}
	}
	return true;
}

// 0x438dd0, line 1862. As TInventoryView::RightButtonUp; a container's
// object is used into the container itself.
bool TContainerWindowBody::RightButtonUp(int x, int y, int Flags)
{
	if (!TWindow::RightButtonUp(x, y, Flags)) {
		const int Position = GetSlotAt(x, y, true);
		if (Position == -1)
			return true;
		SetTradeObject(TObject(), 0, 0, 0, 0);
		SetUseObject(TObject(), 0, 0, 0, 0);
		SetTradeInfo(0);
		const TObject Obj = GetContainerContent(Container, Position);
		SetTradeObject(Obj, Position, 0xffff, Container + 0x40, Position);
		SetUseObject(Obj, Position, 0xffff, Container + 0x40, Position);
		if (Obj.Type == 0)
			return true;
		if (GetClassicControl()) {
			if ((Flags & 0x200) != 0)
				SendLook(0xffff, Container + 0x40, Position, Obj.Type, Position);
			else if ((Flags & 0x400) != 0)
				AddMessage(52, 0, 0);
			else if (GetObjectFlag(Obj.Type, MULTIUSE))
				AddMessage(44, 0, 0);
			else
				SendUseObject(0xffff, Container + 0x40, Position, Obj.Type, Position, Container);
		} else {
			if ((Flags & 0x200) != 0)
				SendLook(0xffff, Container + 0x40, Position, Obj.Type, Position);
			else if ((Flags & 0x400) == 0)
				AddMessage(52, 0, 0);
			else if (GetObjectFlag(Obj.Type, MULTIUSE))
				AddMessage(44, 0, 0);
			else
				SendUseObject(0xffff, Container + 0x40, Position, Obj.Type, Position, Container);
		}
	}
	return true;
}

// 0x42dcb0, line 1875. The wheel scrolls the view (1 up, 2 down).
bool TContainerWindowBody::MouseWheel(int x, int y, int Code)
{
	if (!TWindow::MouseWheel(x, y, Code) && (Code == 1 || Code == 2))
		return ScrollBar->MouseWheel(x, y, Code);
	return true;
}

// 0x42de80, lines 1885-1890
bool TContainerWindowBody::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(20);
	const TComponent *Parent = GetParent();
	PATELLA_ASSERT(Parent!=nullptr);
	SetHelpSize(Parent->Width, Parent->Height);
	MoveHelpPosition(-OffsetX, -OffsetY);
	return true;
}

// 0x42e120, line 1904. The "up" button (event 3) opens the container the
// container is in, once.
void TContainerWindowBody::HandleEvent(int Event)
{
	if (Event == 3 && HasParent) {
		HasParent = false;
		SendContainerUp(Container);
	}
}

// 0x439100, line 1917. MainWindow.cpp's OpenContainer on an open window:
// the view goes back to the top.
void TContainerWindowBody::Update()
{
	ScrollBar->Range->SetValue(0);
	Arrange();
	UpdateUpButton();
}

// 0x42e2c0, line 1940. The slot under x, y (counted over the whole
// capacity); -1 for none, or, with Occupied, for an empty one.
int TContainerWindowBody::GetSlotAt(int x, int y, bool Occupied) const
{
	// x and y become positions in the slot grid in their own stack slots
	// (0x42e302, 0x42e30c): the original's catch wrapper printed them (issue #34).
	x -= SkinProperties().Data630.x;
	y += ScrollBar->Range->Value - SkinProperties().Data630.y;
	const int Column = x / SkinProperties().Data648;
	if (Column < SkinProperties().Data62C && x % SkinProperties().Data648 <= SkinProperties().Data638.x
		&& y % SkinProperties().Data64C <= SkinProperties().Data638.y) {
		const int Position = SkinProperties().Data62C * (y / SkinProperties().Data64C) + Column;
		if (Occupied && GetContainerObjectCount(Container) <= Position)
			return -1;
		return Position;
	}
	return -1;
}

// 0x42e4e0, line 1960. Also called by TSubWindow::Arrange for the windows
// of containers.
void TContainerWindowBody::UpdateUpButton()
{
	TSubWindow *Window = static_cast<TSubWindow *>(GetParent());
	if (GetContainerHasParent(Container)) {
		HasParent = true;
		if (Window != nullptr)
			Window->ShowUpButton();
	} else {
		HasParent = false;
		if (Window != nullptr)
			Window->HideUpButton();
	}
}

// --- TSkillsWindowBody ------------------------------------------------------------

// 0x4392a0, line 1980. The try block starts after the base class. The
// scroll bar moves by half a line.
TSkillsWindowBody::TSkillsWindowBody()
{
	ContextMenu = nullptr;
	ScrollBar = new TScrollBar(TRect(0, 0, SkinProperties().Data870, SkinProperties().Data874), 0, 0, 0, 0,
		GetFontLineHeight(SkinProperties().Data654.FontNumber) / 2);
	InsertChild(ScrollBar);
	Lines = 15;
	Arrange();
}

// 0x42e6b0 (deleting destructor 0x4394b0). An open skill menu forgets the
// window. The try block was dropped (nothing in it can throw).
TSkillsWindowBody::~TSkillsWindowBody()
{
	if (ContextMenu != nullptr)
		ContextMenu->SkillsWindow = nullptr;
}

// Not in the original (issue #118; ContextMenus.h): TSkillContextMenu's stores into the window and its call of the
// window's Arrange, here where the class is complete.
void SetSkillsWindowMenu(TSkillsWindowBody *Window, TSkillContextMenu *Menu) noexcept
{
	Window->ContextMenu = Menu;
}

void ArrangeSkillsWindow(TSkillsWindowBody *Window)
{
	Window->Arrange();
}

// Not in the original (issue #118; DialogWindow.h): TSubWindow::Arrange's call for a container's window.
void UpdateContainerUpButton(TSubWindowBody *Body)
{
	(static_cast<TContainerWindowBody *>(Body))->UpdateUpButton();
}

// 0x4394d0, lines 2003-2063. One line per skill: the name on the left, the
// value on the right; a bar of the progress below it when the skill's bar
// is on (Data670 for the first seven lines, then Data67C); line 8 is a
// separator. Odd, kept: a line with a bar is 10 pixels taller here, but 9
// in MouseHover and GetContentHeight.
void TSkillsWindowBody::Draw(int Surface, int x, int y)
{
	if (!IsMinimized()) {
		const int ScrollY = ScrollBar->Range->Value;
		const int Bottom = Height - 1;
		PATELLA_ASSERT_VALUES(x>=0, x);
		PATELLA_ASSERT_VALUES(y>=0, y);
		DrawSkin(Surface, x, y, Width - ScrollBar->Width, Height, 7, 0, ScrollY % Skins()[7].height);
		if (!Minimized)
			TWindow::Draw(Surface, x, y);
		const int LineHeight = GetFontLineHeight(SkinProperties().Data654.FontNumber);
		int LineY = -ScrollY;
		const int TextWidth = Width - SkinProperties().Data668 * 2 - ScrollBar->Width;
		int Progress = -1;
		const int Margin = SkinProperties().Data668;
		for (int i = 0; i < Lines; i++) {
			TFixedString<100> Text;
			int Value;
			const bool HasValue = GetLine(i + 1, Text, Value, Progress);
			int ItemHeight = LineHeight;
			if (Progress != -1)
				ItemHeight = LineHeight + 10;
			int Top = LineY + SkinProperties().Data66C;
			int ClipHeight = ItemHeight;
			if (Top + ItemHeight > 0) {
				if (Top > Bottom)
					return;
				if (Top < 0) {
					ClipHeight = Top + ItemHeight;
					Top = 0;
				}
				if (ClipHeight + Top > Bottom)
					ClipHeight = Bottom - Top + 1;
				const int ClipX = SkinProperties().Data668 + x;
				const int ClipY = Top + y;
				const int LineMiddle = GetFontHeight(SkinProperties().Data654.FontNumber) / 2 + SkinProperties().Data66C - 1 + LineY;
				if (!HasValue) {
					if (LineMiddle >= 0 && LineMiddle <= Bottom)
						GetSurface()->DrawHorizontalLine(Surface, ClipX, LineMiddle + y, TextWidth,
							SkinProperties().Data654.Color);
				} else {
					TFixedString<20> Number;
					FormatNumber(Value, Number.Chars);
					PrintTextClipped(Surface, SkinProperties().Data668 + x, SkinProperties().Data66C + LineY + y,
						SkinProperties().Data654.FontNumber, SkinProperties().Data654.Color, Text, 0,
						TRect(ClipX, ClipY, TextWidth, ClipHeight));
					PrintTextClipped(Surface, SkinProperties().Data668 + TextWidth - 1 + x, SkinProperties().Data66C + LineY + y,
						SkinProperties().Data654.FontNumber, SkinProperties().Data654.Color, Number, 2,
						TRect(ClipX, ClipY, TextWidth, ClipHeight));
					if (Progress != -1) {
						const int BarHeight = 5;
						GetSurface()->DrawFrameClipped(Surface,
							TRect(Margin + x, LineHeight + 8 + LineY + y, TextWidth,
								BarHeight),
							1, TRGBColor(0), TRect(ClipX, ClipY, TextWidth, ClipHeight));
						GetSurface()->FillRectangleClipped(Surface,
							TRect(Margin + x + 1, LineHeight + 8 + LineY + y + 1,
								(TextWidth - 2) * Progress / 100, BarHeight - 2),
							i < 7 ? SkinProperties().Data670
								  : SkinProperties().Data67C,
							TRect(ClipX, ClipY, TextWidth, ClipHeight));
					}
				}
			}
			LineY += ItemHeight;
		}
	}
}

// 0x42e700, line 2076
bool TSkillsWindowBody::MouseWheel(int x, int y, int Code)
{
	if (!TWindow::MouseWheel(x, y, Code) && (Code == 1 || Code == 2))
		return ScrollBar->MouseWheel(x, y, Code);
	return true;
}

// 0x42e8d0, lines 2086-2091
bool TSkillsWindowBody::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(17);
	const TComponent *Parent = GetParent();
	PATELLA_ASSERT(Parent!=nullptr);
	SetHelpSize(Parent->Width, Parent->Height);
	MoveHelpPosition(-OffsetX, -OffsetY);
	return true;
}

// 0x439c30, line 2133. Over a line with a bar: the percent still to go.
void TSkillsWindowBody::MouseHover(int x, int y)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	if (!IsMinimized()) {
		const int ScrollY = ScrollBar->Range->Value;
		const int Bottom = Height - 1;
		const int LineHeight = GetFontLineHeight(SkinProperties().Data654.FontNumber);
		const int TextWidth = Width - SkinProperties().Data668 * 2 - ScrollBar->Width;
		int LineY = -ScrollY;
		int Progress = -1;
		for (int i = 0; i < Lines;) {
			i++;
			TFixedString<100> Text;
			int Value;
			GetLine(i, Text, Value, Progress);
			int ItemHeight = LineHeight;
			if (Progress != -1)
				ItemHeight = LineHeight + 9;
			int Top = SkinProperties().Data66C + LineY;
			if (Top + ItemHeight > 0) {
				if (Top > Bottom)
					return;
				int ClipHeight = ItemHeight;
				if (Top < 0) {
					Top = 0;
					ClipHeight = SkinProperties().Data66C + LineY + ItemHeight;
				}
				if (ClipHeight + Top > Bottom)
					ClipHeight = Bottom - Top + 1;
				if (Top <= y && y <= ClipHeight + Top && SkinProperties().Data668 <= x
					&& x <= SkinProperties().Data668 + TextWidth && Progress != -1) {
					MsvcSnprintf(GUI.SkillMouseOverText, 256, TextPercentToGo, 100 - Progress);
					SetMouseOverText(GUI.SkillMouseOverText);
				}
			}
			LineY += ItemHeight;
		}
	}
}

// 0x42eb70, line 2144. Opens the skill menu (Control.cpp's message 60).
bool TSkillsWindowBody::RightButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags)
{
	AddMessage(60, reinterpret_cast<std::intptr_t>(this), 0);
	return true;
}

// 0x42ed10, lines 2153-2160. Not higher than the content.
void TSkillsWindowBody::SetSize(int Width, int Height)
{
	const int ScrollbarX = Width - ScrollBar->Width;
	PATELLA_ASSERT_VALUES(ScrollbarX>0, ScrollbarX);
	const int Maximum = GetMaximumHeight();
	if (Height >= Maximum)
		Height = Maximum;
	TWindow::SetSize(Width, Height);
	ScrollBar->SetPosition(ScrollbarX, 0);
	ScrollBar->SetSize(ScrollBar->Width, Height);
	Arrange();
}

// 0x42efd0, line 2173. The visible part leaves the margin (Data66C) at the
// top and the bottom.
void TSkillsWindowBody::Arrange()
{
	int ContentHeight = GetContentHeight();
	const int Visible = Height - SkinProperties().Data66C * 2;
	if (ContentHeight <= Visible)
		ContentHeight = Visible;
	ScrollBar->Range->SetRange(0, ContentHeight - Visible, ScrollBar->Range->Value, Visible);
}

// 0x42f190, line 2183. At least one line.
int TSkillsWindowBody::getMinimumHeight()
{
	// The original's order: a call's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
	const int LineHeight = GetFontLineHeight(SkinProperties().Data654.FontNumber);
	return (std::max)(ScrollBar->getMinimumHeight(), LineHeight);
}

// 0x42f330, line 2196. The stored height ("ContentWindow" 6 in Tibia.cfg)
// or the full height, at least the minimum.
int TSkillsWindowBody::GetInitialHeight()
{
	int Height = GetContentWindowSize(6);
	if (Height == 0)
		Height = GetMaximumHeight();
	if (Height < getMinimumHeight())
		return getMinimumHeight();
	return Height;
}

// 0x42f500, line 2206
int TSkillsWindowBody::GetMaximumHeight()
{
	return GetContentHeight() + SkinProperties().Data66C * 2;
}

// 0x439ef0, line 2225
int TSkillsWindowBody::GetContentHeight()
{
	const int LineHeight = GetFontLineHeight(SkinProperties().Data654.FontNumber);
	int Result = 0;
	for (int i = 0; i < Lines;) {
		i++;
		TFixedString<100> Text;
		int Value, Progress;
		GetLine(i, Text, Value, Progress);
		if (Progress != -1)
			Result += 9;
		Result += LineHeight;
	}
	return Result;
}

// 0x42f6a0, lines 2234-2280. The assertion comes before the try block. The
// jump table 0x42fc44 has the lines 1-15: experience, level, hit points,
// mana, soul points, capacity, magic level, a separator (false, without the
// checks below) and the seven skills. Progress is -1 without a bar.
bool TSkillsWindowBody::GetLine(int NumberOfLine, char *Text, int &Value, int &Progress) const
{
	PATELLA_ASSERT_VALUES(In(NumberOfLine,1,Lines), NumberOfLine, Lines);
	Value = 0;
	Progress = -1;
	TStringBuffer Name(Text, 100); // every caller passes char[100]
	switch (NumberOfLine) {
	case 1:
		Name.Assign("Experience");
		Value = GetPlayerExperience();
		break;
	case 2:
		Name.Assign("Level");
		Value = GetPlayerLevel();
		if (GetLevelBar())
			Progress = GetPlayerLevelProgress();
		else
			Progress = -1;
		break;
	case 3:
		Name.Assign("Hit Points");
		Value = GetPlayerHealth();
		break;
	case 4:
		Name.Assign("Mana");
		Value = GetPlayerMana();
		break;
	case 5:
		Name.Assign("Soul Points");
		Value = GetPlayerSoul();
		break;
	case 6:
		Name.Assign("Capacity");
		Value = GetPlayerCapacity();
		break;
	case 7:
		Name.Assign("Magic Level");
		Value = GetPlayerMagicLevel();
		if (GetMagLevelBar())
			Progress = GetPlayerMagicLevelProgress();
		else
			Progress = -1;
		break;
	case 8:
		return false;
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15: {
		const int SkillNumber = NumberOfLine - 9;
		Name.Assign(GetSkillName(SkillNumber));
		Value = GetPlayerSkillValue(SkillNumber);
		if (GetSkillBar(SkillNumber))
			Progress = GetPlayerSkillProgress(SkillNumber);
		else
			Progress = -1;
		break;
	}
	default:
		break;
	}
	PATELLA_ASSERT_VALUES(Value>=0, Value, NumberOfLine);
	PATELLA_ASSERT_VALUES(In(Progress,-1,100), Progress, NumberOfLine);
	return true;
}

// --- TBattleWindowBody ------------------------------------------------------------

// 0x43a0c0, line 2300. The try block starts after the base class. The
// scroll bar moves by a third of a row.
TBattleWindowBody::TBattleWindowBody()
{
	ScrollBar = new TScrollBar(TRect(0, 0, SkinProperties().Data870, SkinProperties().Data874), 0, 0, 0, 0,
		SkinProperties().Style688.Data00 / 3);
	InsertChild(ScrollBar);
	MouseOverText[0] = 0;
	NumberOfOpponents = -1;
	Arrange();
}

// The inline destructor's work until issue #108 (0x425a70): no creature is
// highlighted any more; then the children.
void TBattleWindowBody::OnClose()
{
	try {
		SetHighlightID(0);
	} catch (...) {
		CloseWhileUnwinding([this] { TSubWindowBody::OnClose(); });
		throw;
	}
	TSubWindowBody::OnClose();
}

// 0x43a2c0, lines 2328-2452. One row per opponent: a frame in the colour of
// the mouse (the opponent under it becomes the highlighted creature), the
// attack or the follow target, the outfit, the party shield (248-251; 248
// only while the player is in no party) and the skull (240-243, blinking
// for four seconds after SkullTime), the name (shortened to the width) and
// the health bar. The children are drawn before the background.
void TBattleWindowBody::Draw(int Surface, int x, int y)
{
	SetHighlightID(0);
	const int Opponents = GetNumberOfOpponents();
	if (NumberOfOpponents != Opponents) {
		NumberOfOpponents = Opponents;
		Arrange();
	}
	if (!IsMinimized()) {
		if (!Minimized)
			TWindow::Draw(Surface, x, y);
		const int ScrollY = ScrollBar->Range->Value;
		const int Bottom = Height - 1;
		PATELLA_ASSERT_VALUES(x>=0, x);
		PATELLA_ASSERT_VALUES(y>=0, y);
		DrawSkin(Surface, x, y, Width - ScrollBar->Width, Height, 7, 0, ScrollY % Skins()[7].height);
		const int ClipWidth = Width - ScrollBar->Width;
		for (int i = 0; i < Opponents; i++) {
			TCreature *Creature = GetOpponent(i);
			const int RowY = SkinProperties().Style688.Data00 * i - ScrollY;
			int Offset = 0;
			if (SkinProperties().Style688.Data00 + RowY <= 0)
				continue;
			if (RowY > Bottom)
				return;
			int ClipY = RowY;
			int ClipHeight = SkinProperties().Style688.Data00;
			if (RowY < 0) {
				ClipY = 0;
				ClipHeight = SkinProperties().Style688.Data00 + RowY;
			}
			if (ClipHeight + ClipY > Bottom)
				ClipHeight = Bottom - ClipY + 1;
			ClipY += y;
			bool Framed = false;
			TRGBColor Color = SkinProperties().Style688.Data54.Color;
			// GetOpponentAt's arguments in the original's order, right to left (MSVC; GCC and Clang may differ, issue
			// #91).
			const auto OpponentAtMouse = [&] {
				const int MouseY = GetMouseY();
				return GetOpponentAt(GetMouseX() - x, MouseY - y);
			};
			if (In(GetMouseY(), ClipY, ClipY + ClipHeight) && In(GetMouseX(), x, ClipWidth + x) && i == OpponentAtMouse()) {
				SetHighlightID(Creature->CreatureID);
				Color = SkinProperties().HighlightColor;
				Framed = true;
			}
			if (Creature->CreatureID == GetAttackID()) {
				if (Framed)
					Color = SkinProperties().HighlightedAttackColor;
				else
					Color = SkinProperties().AttackColor;
				Framed = true;
			} else if (Creature->CreatureID == GetFollowID()) {
				if (Framed)
					Color = SkinProperties().HighlightedFollowColor;
				else
					Color = SkinProperties().FollowColor;
				Framed = true;
			}
			if (Framed)
				GetSurface()->DrawFrameClipped(Surface,
					TRect(SkinProperties().Style688.Data38.x + x,
						SkinProperties().Style688.Data38.y + RowY + y, SkinProperties().Style688.Data38.width,
						SkinProperties().Style688.Data38.height),
					1, Color, TRect(x, ClipY, ClipWidth, ClipHeight));
			GetSurface()->Flag26 = true;
			DrawOutfit(Surface, SkinProperties().Style688.Data04.x + x, SkinProperties().Style688.Data04.y + RowY + y,
				SkinProperties().Style688.Data04.width, Creature->OutfitType, Creature->Colors,
				SkinProperties().Style688.Data68, TRect(x, ClipY, ClipWidth, ClipHeight), SkinProperties().Data498);
			GetSurface()->Flag26 = false;
			if (Creature->PARTYFlag > 0) {
				int Skin = 0; // none: no shield in the party (flag 1) or an unknown flag
				switch (Creature->PARTYFlag) {
				case 1:
					if (!IsPlayerInParty())
						Skin = 248;
					break;
				case 2:
					Skin = 249;
					break;
				case 3:
					Skin = 250;
					break;
				case 4:
					Skin = 251;
					break;
				default:
					break;
				}
				if (Skin != 0) {
					DrawSkinClipped(Surface, SkinProperties().Style688.Data14.x + x, SkinProperties().Style688.Data14.y + RowY + y,
						Skins()[Skin].width, Skins()[Skin].height, Skin, x, ClipY, ClipWidth, ClipHeight, 0, 0);
					Offset = SkinProperties().Style688.Data24;
				}
			}
			if (Creature->PKFlag > 0) {
				int Skin = 0; // none: an unknown flag
				switch (Creature->PKFlag) {
				case 1:
					Skin = 240;
					break;
				case 2:
					Skin = 241;
					break;
				case 3:
					Skin = 242;
					break;
				case 4:
					Skin = 243;
					break;
				default:
					break;
				}
				if (Skin != 0) {
					if (Creature->SkullTime == 0 || !Creature->SkullKept(patella::platform::Platform().Clock.WallSeconds()) || GetBlinkState())
						DrawSkinClipped(Surface, SkinProperties().Style688.Data14.x + Offset + x,
							SkinProperties().Style688.Data14.y + RowY + y, Skins()[Skin].width, Skins()[Skin].height, Skin, x,
							ClipY, ClipWidth, ClipHeight, 0, 0);
					Offset += SkinProperties().Style688.Data24;
				}
			}
			const char *Name = Creature->Name;
			const int MaxLength = GetFittingTextLength(SkinProperties().Style688.Data54.FontNumber, Name,
				SkinProperties().Style688.Data14.width);
			TFixedString<32> Shortened;
			if (MaxLength < (int)strlen(Name)) {
				Shortened.Assign(Name);
				ShortenString(Shortened, MaxLength);
				Name = Shortened;
			}
			if (Framed)
				PrintTextClipped(Surface, SkinProperties().Style688.Data14.x + Offset + x,
					SkinProperties().Style688.Data14.y + RowY + y, SkinProperties().Style688.Data54.FontNumber, Color, Name,
					SkinProperties().Style688.Data54.Alignment, TRect(x, ClipY, ClipWidth, ClipHeight));
			else
				PrintFormattedTextClipped(Surface, SkinProperties().Style688.Data14.x + Offset + x,
					SkinProperties().Style688.Data14.y + RowY + y, SkinProperties().Style688.Data54, Name,
					TRect(x, ClipY, ClipWidth, ClipHeight));
			GetSurface()->DrawFrameClipped(Surface,
				TRect(SkinProperties().Style688.Data28.x + x,
					SkinProperties().Style688.Data28.y + RowY + y, SkinProperties().Style688.Data28.width,
					SkinProperties().Style688.Data28.height),
				1, SkinProperties().Style688.Data48,
				TRect(x, ClipY, ClipWidth, ClipHeight));
			GetSurface()->FillRectangleClipped(Surface,
				TRect(SkinProperties().Style688.Data28.x + x + 1,
					SkinProperties().Style688.Data28.y + RowY + y + 1,
					Creature->Health * (SkinProperties().Style688.Data28.width - 2) / 100,
					SkinProperties().Style688.Data28.height - 2),
				Creature->GetHealthColor(),
				TRect(x, ClipY, ClipWidth, ClipHeight));
		}
	}
}

// 0x42fc80, lines 2465-2472
void TBattleWindowBody::SetSize(int Width, int Height)
{
	const int ScrollbarX = Width - ScrollBar->Width;
	PATELLA_ASSERT_VALUES(ScrollbarX>0, ScrollbarX);
	TWindow::SetSize(Width, Height);
	ScrollBar->SetPosition(ScrollbarX, 0);
	ScrollBar->SetSize(ScrollBar->Width, Height);
	Arrange();
}

// 0x42ff30, line 2487
void TBattleWindowBody::Arrange()
{
	int ContentHeight = GetContentHeight();
	if (ContentHeight <= Height)
		ContentHeight = Height;
	ScrollBar->Range->SetRange(0, ContentHeight - Height, ScrollBar->Range->Value, Height);
}

// 0x4300e0, line 2498. A press is taken even where no child takes it.
bool TBattleWindowBody::LeftButtonDown(int x, int y, int Flags)
{
	TWindow::LeftButtonDown(x, y, Flags);
	return true;
}

// 0x43ae20, lines 2521-2544. Action state 7 uses the use object on a
// monster (not on players), 8 offers a trade to a player; a plain click is
// a right click with 0x800 (attack, RightButtonUp).
bool TBattleWindowBody::LeftButtonUp(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonUp(x, y, Flags)) {
		const int Position = GetOpponentAt(x, y);
		if (Position != -1) {
			const TCreature *Creature = GetOpponent(Position);
			if (Creature != nullptr) {
				if (GetActionState() == 7) {
					int SrcIndex, SrcX, SrcY, SrcZ;
					const TObject &SrcObj = *GetUseObject(&SrcIndex, &SrcX, &SrcY, &SrcZ);
					PATELLA_ASSERT(SrcObj!=0);
					PATELLA_ASSERT(SrcObj.Type!=CREATURE);
					if (!IsPlayer(Creature->CreatureID)) {
						SendUseObjectOnCreature(SrcX, SrcY, SrcZ, SrcObj.Type, SrcIndex, Creature->CreatureID);
						return true;
					}
					SetStatusMessage("You are not allowed to shoot directly on players.", false);
				} else if (GetActionState() == 8) {
					int TradeIndex, TradeX, TradeY, TradeZ;
					const TObject &Obj = *GetTradeObject(&TradeIndex, &TradeX, &TradeY, &TradeZ);
					PATELLA_ASSERT(Obj!=0);
					if (IsPlayer(Creature->CreatureID)) {
						SendRequestTrade(TradeX, TradeY, TradeZ, Obj.Type, TradeIndex, Creature->CreatureID);
						return true;
					}
					SetStatusMessage("Select a player to trade with.", false);
				} else if (GetActionState() != 6) {
					if (Flags != 0)
						RightButtonUp(x, y, Flags);
					else
						RightButtonUp(x, y, 0x800);
				}
			}
		}
	}
	return true;
}

// 0x43b2f0, line 2573. 0x800 (a left click) attacks the opponent or stops
// attacking it, Shift looks at it, otherwise its menu opens (message 53).
bool TBattleWindowBody::RightButtonUp(int x, int y, int Flags)
{
	if (!TWindow::RightButtonUp(x, y, Flags)) {
		const int Position = GetOpponentAt(x, y);
		if (Position != -1) {
			const TCreature *Creature = GetOpponent(Position);
			if (Creature != nullptr) {
				if ((Flags & 0x800) != 0) {
					const uint32 AttackID = GetAttackID();
					SetAttackID(Creature->CreatureID == AttackID ? 0 : Creature->CreatureID);
					SendAttack(GetAttackID());
				} else if ((Flags & 0x200) != 0) {
					SendLook(Creature->RealX, Creature->RealY, Creature->RealZ, CREATURE, 0);
				} else {
					AddMessage(53, Creature->CreatureID, 0);
				}
			}
		}
	}
	return true;
}

// 0x43b510, line 2593. A shortened name in full.
void TBattleWindowBody::MouseHover(int x, int y)
{
	const int Position = GetOpponentAt(x, y);
	if (Position != -1) {
		TCreature *Creature = GetOpponent(Position);
		if (Creature != nullptr) {
			const int MaxLength = GetFittingTextLength(SkinProperties().Style688.Data54.FontNumber, Creature->Name,
				SkinProperties().Style688.Data14.width);
			if (MaxLength < (int)strlen(Creature->Name)) {
				MouseOverText.Assign(Creature->Name);
				SetMouseOverText(MouseOverText);
			}
		}
	}
}

// 0x430280, line 2606
bool TBattleWindowBody::MouseWheel(int x, int y, int Code)
{
	if (!TWindow::MouseWheel(x, y, Code) && (Code == 1 || Code == 2))
		return ScrollBar->MouseWheel(x, y, Code);
	return true;
}

// 0x430450, lines 2616-2621
bool TBattleWindowBody::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(18);
	const TComponent *Parent = GetParent();
	PATELLA_ASSERT(Parent!=nullptr);
	SetHelpSize(Parent->Width, Parent->Height);
	MoveHelpPosition(-OffsetX, -OffsetY);
	return true;
}

// 0x4306f0, line 2631. At least one row.
int TBattleWindowBody::getMinimumHeight()
{
	return (std::max)(ScrollBar->getMinimumHeight(), SkinProperties().Style688.Data00);
}

// 0x430890, line 2644. The stored height ("ContentWindow" 7) or the content.
int TBattleWindowBody::GetInitialHeight()
{
	int Height = GetContentWindowSize(7);
	if (Height == 0)
		Height = GetContentHeight();
	if (Height < getMinimumHeight())
		Height = getMinimumHeight();
	return Height;
}

// 0x430a50, line 2654
int TBattleWindowBody::GetContentHeight()
{
	return GetNumberOfOpponents() * SkinProperties().Style688.Data00 + 1;
}

// 0x430be0, line 2668. The row under x, y; -1 for none.
int TBattleWindowBody::GetOpponentAt(int x, int y)
{
	y += ScrollBar->Range->Value;
	const int Position = y / SkinProperties().Style688.Data00;
	if (y >= 0 && x >= 0 && x <= Width - ScrollBar->Width && Position < GetNumberOfOpponents())
		return Position;
	return -1;
}

// --- TTradeOffers -----------------------------------------------------------------

// 0x430dc0, line 2687. The try block starts after the base class.
TTradeOffers::TTradeOffers()
{
	ScrollBar = new TScrollBar(TRect(0, 0, SkinProperties().Data870, SkinProperties().Data874), 0, 0, 0, 0, 10);
	InsertChild(ScrollBar);
	Separator = new TSeparator(false);
	InsertChild(Separator);
}

// 0x430fd0, lines 2698-2730. Both offers side by side (Style6FC.Data68 the
// own, Data78 the partner's) in rows of DataA4 slots (skin 125), scrolled
// together.
void TTradeOffers::Draw(int Surface, int x, int y)
{
	const int ScrollY = ScrollBar->Range->Value;
	const int Bottom = Height - 1;
	PATELLA_ASSERT_VALUES(x>=0, x);
	PATELLA_ASSERT_VALUES(y>=0, y);
	DrawSkin(Surface, x, y, Width - ScrollBar->Width, Height, 7, 0, ScrollY % Skins()[7].height);
	TWindow::Draw(Surface, x, y);
	const int ClipHeight = Height;
	const int ClipWidth = Width;
	for (int Side = 0; Side < 2; Side++) {
		const TRect &Area = Side == 0 ? SkinProperties().Style6FC.Data68 : SkinProperties().Style6FC.Data78;
		const int AreaX = Area.x;
		const int AreaY = Area.y;
		const int AreaWidth = Area.width;
		const int AreaHeight = Area.height;
		// Data88: the object's offset in a slot, x/y for the own offer,
		// Width/Height for the partner's.
		const int *ObjectOffset = Side == 0 ? &SkinProperties().Style6FC.Data88.x : &SkinProperties().Style6FC.Data88.width;
		const int OffsetX = ObjectOffset[0];
		const int OffsetY = ObjectOffset[1];
		const int Count = Side == 0 ? GetOwnOfferCount() : GetCounterOfferCount();
		for (int i = 0; i < Count; i++) {
			const TObject Obj = Side == 0 ? GetOwnOfferObject(i) : GetCounterOfferObject(i);
			const int SlotX = i % SkinProperties().Style6FC.DataA4 * SkinProperties().Style6FC.DataA0;
			const int RowY = i / SkinProperties().Style6FC.DataA4 * SkinProperties().Style6FC.Data9C - ScrollY;
			if (AreaHeight + AreaY + RowY > 0 && AreaY + RowY <= Bottom) {
				const int DestX = SlotX + AreaX;
				int DestY = AreaY + RowY;
				int SlotHeight = AreaHeight;
				if (DestY < 0) {
					SlotHeight = DestY + AreaHeight;
					DestY = 0;
				}
				if (SlotHeight + DestY > Bottom)
					SlotHeight = Bottom - DestY + 1;
				PATELLA_ASSERT_VALUES(x+DestX>=0, x+DestX);
				PATELLA_ASSERT_VALUES(y+DestY>=0, y+DestY);
				DrawSkin(Surface, x + DestX, y + DestY, AreaWidth, SlotHeight, 125, 0, 0);
				DrawObject(Surface, OffsetX + SlotX + x, OffsetY + RowY + y, SkinProperties().Style6FC.Data98, Obj,
					TRGBColor(0, 0, 0), TRect(x, y, ClipWidth, ClipHeight), SkinProperties().Data498, false);
			}
		}
	}
}

// 0x4317b0, line 2745
void TTradeOffers::SetSize(int Width, int Height)
{
	const int ScrollBarWidth = ScrollBar->Width;
	TWindow::SetSize(Width, Height);
	ScrollBar->SetPosition(Width - ScrollBarWidth, 0);
	ScrollBar->SetSize(ScrollBar->Width, Height);
	SetChildArea(Separator, SkinProperties().Style6FC.DataA8);
	Arrange();
}

// 0x4319b0, line 2755
int TTradeOffers::getMinimumHeight()
{
	return ScrollBar->getMinimumHeight();
}

// 0x431b40, line 2769. The rows of the longer offer. Odd, kept: the top
// margin added is Data68.x, the left edge of the own offer.
int TTradeOffers::GetContentHeight()
{
	const int CounterCount = GetCounterOfferCount();
	const int OwnCount = GetOwnOfferCount();
	const int Count = (std::max)(OwnCount, CounterCount);
	const int Result = SkinProperties().Style6FC.Data9C
			* (Count / SkinProperties().Style6FC.DataA4 + (Count % SkinProperties().Style6FC.DataA4 > 0))
		+ 1 + SkinProperties().Style6FC.Data68.x;
	return (std::max)(getMinimumHeight(), Result);
}

// 0x431d10, line 2782
void TTradeOffers::Arrange()
{
	int ContentHeight = GetContentHeight();
	if (ContentHeight <= Height)
		ContentHeight = Height;
	ScrollBar->Range->SetRange(0, ContentHeight - Height, ScrollBar->Range->Value, Height);
}

// 0x43b710, line 2801. A click on an object of either offer inspects it.
bool TTradeOffers::LeftButtonUp(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonUp(x, y, Flags) && GetActionState() != 6 && GetActionState() != 7
		&& GetActionState() != 8) {
		int Side, Position;
		GetOfferAt(x, y, &Side, &Position);
		if (Side != -1)
			SendInspectTrade(Side, Position);
	}
	return true;
}

// 0x431ec0, line 2812
bool TTradeOffers::LeftButtonDown(int x, int y, int Flags)
{
	TWindow::LeftButtonDown(x, y, Flags);
	return true;
}

// 0x43b900, line 2831
bool TTradeOffers::RightButtonUp(int x, int y, int Flags)
{
	if (!TWindow::RightButtonUp(x, y, Flags) && GetActionState() != 6 && GetActionState() != 7
		&& GetActionState() != 8) {
		int Side, Position;
		GetOfferAt(x, y, &Side, &Position);
		if (Side != -1)
			SendInspectTrade(Side, Position);
	}
	return true;
}

// 0x432060, line 2844
bool TTradeOffers::MouseWheel(int x, int y, int Code)
{
	if (!TWindow::MouseWheel(x, y, Code) && (Code == 1 || Code == 2))
		return ScrollBar->MouseWheel(x, y, Code);
	return true;
}

// 0x432230, line 2877. The offer (0 own, 1 the partner's) and the position
// under x, y; -1, -1 for none.
void TTradeOffers::GetOfferAt(int x, int y, int *Side, int *Position) const
{
	*Position = -1;
	*Side = -1;
	for (int i = 0; i <= 1; i++) {
		const TRect &Area = i == 0 ? SkinProperties().Style6FC.Data68 : SkinProperties().Style6FC.Data78;
		const int AreaWidth = Area.width;
		const int AreaX = Area.x;
		const int AreaY = Area.y;
		const int AreaHeight = Area.height;
		const int Count = i == 0 ? GetOwnOfferCount() : GetCounterOfferCount();
		const int RelativeY = ScrollBar->Range->Value - AreaY + y;
		const int Column = (x - AreaX) / SkinProperties().Style6FC.DataA0;
		if (Column < SkinProperties().Style6FC.DataA4 && (x - AreaX) % SkinProperties().Style6FC.DataA0 <= AreaWidth
			&& RelativeY % SkinProperties().Style6FC.Data9C <= AreaHeight) {
			const int Found = RelativeY / SkinProperties().Style6FC.Data9C * SkinProperties().Style6FC.DataA4 + Column;
			if (Found < Count) {
				*Position = Found;
				*Side = i;
				return;
			}
		}
	}
}

// --- TTradeNames ------------------------------------------------------------------

// 0x432470, line 2894. The try block starts after the base class.
TTradeNames::TTradeNames()
{
	Separator = new TSeparator(true);
	InsertChild(Separator);
	const char *Name = GetOwnOfferCount() < 1 ? "" : GetOwnOfferName();
	OwnName = new TLabel(SkinProperties().Style6FC.Data14.x, SkinProperties().Style6FC.Data14.y,
		SkinProperties().Style6FC.Data14.width, SkinProperties().Style6FC.Data14.height, SkinProperties().Style6FC.Data44, Name,
		true);
	InsertChild(OwnName);
	Name = GetCounterOfferCount() < 1 ? "" : GetCounterOfferName();
	CounterName = new TLabel(SkinProperties().Style6FC.Data24.x, SkinProperties().Style6FC.Data24.y,
		SkinProperties().Style6FC.Data24.width, SkinProperties().Style6FC.Data24.height, SkinProperties().Style6FC.Data44, Name,
		true);
	InsertChild(CounterName);
}

// 0x432770, lines 2901-2907. Both assertions come before the try block.
void TTradeNames::Draw(int Surface, int x, int y)
{
	PATELLA_ASSERT_VALUES(x>=0, x);
	PATELLA_ASSERT_VALUES(y>=0, y);
	DrawSkin(Surface, x, y, Width, Height, 7, 0, 0);
	TWindow::Draw(Surface, x, y);
}

// 0x432b00, line 2919
void TTradeNames::SetSize(int Width, int Height)
{
	TWindow::SetSize(Width, Height);
	SetChildArea(OwnName, SkinProperties().Style6FC.Data14);
	SetChildArea(CounterName, SkinProperties().Style6FC.Data24);
	SetChildArea(Separator, SkinProperties().Style6FC.Data34);
}

// 0x432d40, line 2930. The names once the offers came.
void TTradeNames::Arrange()
{
	if (GetOwnOfferCount() > 0)
		OwnName->SetText(GetOwnOfferName(), true);
	if (GetCounterOfferCount() > 0)
		CounterName->SetText(GetCounterOfferName(), true);
}

// --- TTradeButtons ----------------------------------------------------------------

// 0x432f00, line 2947. The try block starts after the base class (and the
// callback member). Until the partner offers, a text asks to wait.
TTradeButtons::TTradeButtons()
	: Callback(nullptr, 0)
{
	Callback = NoCallback;
	CancelButton = nullptr;
	WaitLabel = nullptr;
	AcceptButton = nullptr;
	WaitLabel = new TLabel(SkinProperties().Style6FC.DataF8, TextWaitForCounterOffer);
	InsertChild(WaitLabel);
	Separator = new TSeparator(true);
	InsertChild(Separator);
}

// 0x433140, lines 2954-2960. Both assertions come before the try block.
void TTradeButtons::Draw(int Surface, int x, int y)
{
	PATELLA_ASSERT_VALUES(x>=0, x);
	PATELLA_ASSERT_VALUES(y>=0, y);
	DrawSkin(Surface, x, y, Width, Height, 7, 0, 0);
	TWindow::Draw(Surface, x, y);
}

// 0x4334d0, line 2974
void TTradeButtons::SetSize(int Width, int Height)
{
	TWindow::SetSize(Width, Height);
	SetChildArea(Separator, SkinProperties().Style6FC.DataE8);
	if (CancelButton != nullptr)
		SetChildPosition(CancelButton, SkinProperties().Style6FC.DataC8.width, SkinProperties().Style6FC.DataC8.height);
	if (WaitLabel != nullptr)
		SetChildArea(WaitLabel, SkinProperties().Style6FC.DataD8);
	if (AcceptButton != nullptr)
		SetChildPosition(AcceptButton, SkinProperties().Style6FC.DataC8.x, SkinProperties().Style6FC.DataC8.y);
}

// 0x433720, line 2988. "Cancel" before the partner offered, "Reject"
// after; both send Callback.
void TTradeButtons::SetCancelButton(bool Cancel)
{
	if (CancelButton != nullptr)
		DeleteChild(CancelButton);
	if (!Cancel)
		CancelButton = new TTextButton(0, 0, "Reject", Callback, TextRejectOffer);
	else
		CancelButton = new TTextButton(0, 0, "Cancel", Callback, TextCancelOffer);
	InsertChild(CancelButton);
	SetSize(Width, Height);
}

// 0x433940, lines 2995-3005. The assertion comes before the try block. The
// counter offer came: "Reject" and "Accept" (event 1 of EventComponent).
// Odd, kept: the wait text is deleted without a NULL check.
void TTradeButtons::ShowAcceptButton(TComponent *EventComponent)
{
	PATELLA_ASSERT(EventComponent!=nullptr);
	SetCancelButton(false);
	DeleteChild(WaitLabel);
	WaitLabel = nullptr;
	AcceptButton = new TTextButton(0, 0, "Accept", TCallback(EventComponent, 1), TextAcceptOffer);
	InsertChild(AcceptButton);
	SetSize(Width, Height);
	Arrange();
}

// 0x433c40, line 3020. Accepted: wait for the partner (again without a
// NULL check).
void TTradeButtons::Wait()
{
	DeleteChild(AcceptButton);
	AcceptButton = nullptr;
	WaitLabel = new TLabel(SkinProperties().Style6FC.DataF8, TextWaitForPartner);
	InsertChild(WaitLabel);
	SetSize(Width, Height);
	Arrange();
}

// --- TTradeWindowBody -------------------------------------------------------------

// 0x43baf0, line 3037. The try block starts after the base class.
TTradeWindowBody::TTradeWindowBody()
{
	Buttons = new TTradeButtons;
	Offers = new TTradeOffers;
	Names = new TTradeNames;
	InsertChild(Buttons);
	InsertChild(Offers);
	InsertChild(Names);
	SetSize(170, 100);
}

// 0x433e60, line 3048 (deleting destructor 0x43bd20). Its work is the close
// since issue #108.
TTradeWindowBody::~TTradeWindowBody()
{
	CheckClosed("TTradeWindowBody");
}

// The destructor's work until issue #108 (0x433e60): closing the window
// rejects an open offer; then the children.
void TTradeWindowBody::OnClose()
{
	try {
		if (GetOwnOfferCount() > 0)
			SendRejectTrade();
		ClearOffers();
	} catch (...) {
		CloseWhileUnwinding([this] { TSubWindowBody::OnClose(); });
		throw;
	}
	TSubWindowBody::OnClose();
}

// 0x434000 (no frame)
int TTradeWindowBody::GetContentHeight()
{
	return getMinimumHeight();
}

// 0x434010, line 3066. The height is limited to the range between the
// minimum and the maximum height (both read twice, in either order). The
// calls go in the binary's order (0x43403f-0x434064: maximum, minimum, for
// the lower bound, then again for the upper one) in their own variables:
// in one expression our compiler called them in another order (issue #34).
void TTradeWindowBody::SetSize(int Width, int Height)
{
	int Maximum = GetMaximumHeight();
	int Minimum = getMinimumHeight();
	const int Lower = (std::min)(Maximum, Minimum);
	Maximum = GetMaximumHeight();
	Minimum = getMinimumHeight();
	Limit(&Height, Lower, (std::max)(Maximum, Minimum));
	TWindow::SetSize(Width, Height);
	SetChildArea(Buttons, SkinProperties().Style6FC.DataB8);
	SetChildArea(Offers, SkinProperties().Style6FC.Data58);
	SetChildArea(Names, SkinProperties().Style6FC.Data04);
	Arrange();
}

// 0x4342b0, lines 3076-3081
bool TTradeWindowBody::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(21);
	const TComponent *Parent = GetParent();
	PATELLA_ASSERT(Parent!=nullptr);
	SetHelpSize(Parent->Width, Parent->Height);
	MoveHelpPosition(-OffsetX, -OffsetY);
	return true;
}

// 0x434550, line 3092
int TTradeWindowBody::getMinimumHeight()
{
	return (std::max)(Offers->getMinimumHeight(), SkinProperties().Style6FC.Data00);
}

// 0x4346f0, line 3102
int TTradeWindowBody::GetInitialHeight()
{
	return GetMaximumHeight();
}

// 0x434880, line 3112
int TTradeWindowBody::GetMaximumHeight()
{
	const int NamesHeight = Names->Height;
	const int ButtonsHeight = Buttons->Height;
	return Offers->GetContentHeight() + ButtonsHeight + NamesHeight;
}

// 0x434a20, line 3130. Event 21: the partner's offer came (MainWindow.cpp's
// UpdateTrade); event 1: "Accept".
void TTradeWindowBody::HandleEvent(int Event)
{
	if (Event == 21) {
		Buttons->ShowAcceptButton(this);
		Arrange();
	} else if (Event == 1) {
		SendAcceptTrade();
		Buttons->Wait();
		Arrange();
	}
}

// --- TBuddylistWindowBody ---------------------------------------------------------

// 0x43bd40, line 3150. The try block starts after the base class. The
// scroll bar moves by half a row.
TBuddylistWindowBody::TBuddylistWindowBody()
{
	ScrollBar = new TScrollBar(TRect(0, 0, SkinProperties().Data870, SkinProperties().Data874), 0, 0, 0, 0,
		SkinProperties().BuddylistWindow.Data04 / 2);
	InsertChild(ScrollBar);
	MouseOverText = nullptr;
	NumberOfBuddies = -1;
	Arrange();
}

// 0x434bf0, lines 3176-3214. One row per buddy: the icon (skins 218 and
// up), the name (shortened to the width) in the colour of its state (online,
// offline, new). Clipped to the whole window.
void TBuddylistWindowBody::Draw(int Surface, int x, int y)
{
	if (NumberOfBuddies != GetNumberOfBuddies()) {
		NumberOfBuddies = GetNumberOfBuddies();
		Arrange();
	}
	if (!IsMinimized()) {
		const int ScrollY = ScrollBar->Range->Value;
		const int Bottom = Height - 1;
		PATELLA_ASSERT_VALUES(x>=0, x);
		PATELLA_ASSERT_VALUES(y>=0, y);
		DrawSkin(Surface, x, y, Width - ScrollBar->Width, Height, 7, 0, ScrollY % Skins()[7].height);
		if (!Minimized)
			TWindow::Draw(Surface, x, y);
		const int ClipHeight = Height;
		const int ClipWidth = Width - ScrollBar->Width;
		for (int i = 0; i < GetNumberOfBuddies(); i++) {
			const int ItemX = 0;
			const int ItemY = SkinProperties().BuddylistWindow.Data04 * i - ScrollY;
			if (SkinProperties().BuddylistWindow.Data04 + ItemY <= 0)
				continue;
			if (ItemY > Bottom)
				return;
			if (GetBuddyListIcon(i) > 0) {
				const int Icon = GetBuddyListIcon(i);
				PATELLA_ASSERT_VALUES(x+ItemX+SkinProperties().BuddylistWindow.Logo.x>=0, x);
				PATELLA_ASSERT_VALUES(y+ItemY+SkinProperties().BuddylistWindow.Logo.y>=0, y);
				DrawSkinClipped(Surface, SkinProperties().BuddylistWindow.Logo.x + x,
					SkinProperties().BuddylistWindow.Logo.y + ItemY + y, SkinProperties().BuddylistWindow.Logo.width,
					SkinProperties().BuddylistWindow.Logo.height, Icon + 217, x, y, ClipWidth, ClipHeight, 0, 0);
			}
			TRGBColor Color = GetBuddyListOnline(i) ? SkinProperties().BuddylistWindow.Data3C
													: SkinProperties().BuddylistWindow.Data48;
			if (GetBuddyListNew(i))
				Color = SkinProperties().BuddylistWindow.Data54;
			const char *Name = GetBuddyListName(i);
			const int MaxLength = GetFittingTextLength(SkinProperties().BuddylistWindow.Data28.FontNumber, Name,
				SkinProperties().BuddylistWindow.Data18.width);
			TFixedString<40> Shortened;
			if (MaxLength < (int)strlen(Name)) {
				Shortened.Assign(Name);
				ShortenString(Shortened, MaxLength);
				Name = Shortened;
			}
			PrintTextClipped(Surface, SkinProperties().BuddylistWindow.Data18.x + x,
				SkinProperties().BuddylistWindow.Data18.y + ItemY + y, SkinProperties().BuddylistWindow.Data28.FontNumber,
				Color, Name, SkinProperties().BuddylistWindow.Data28.Alignment, TRect(x, y, ClipWidth, ClipHeight));
		}
	}
}

// 0x4352e0, lines 3225-3232
void TBuddylistWindowBody::SetSize(int Width, int Height)
{
	const int ScrollbarX = Width - ScrollBar->Width;
	PATELLA_ASSERT_VALUES(ScrollbarX>0, ScrollbarX);
	TWindow::SetSize(Width, Height);
	ScrollBar->SetPosition(ScrollbarX, 0);
	ScrollBar->SetSize(ScrollBar->Width, Height);
	Arrange();
}

// 0x43bf40, line 3249. The VIP menu (message 54), also for no buddy (0).
bool TBuddylistWindowBody::RightButtonUp(int x, int y, int Flags)
{
	if (!TWindow::RightButtonUp(x, y, Flags)) {
		const int Position = GetBuddyAt(x, y);
		AddMessage(54, Position == -1 ? 0 : GetBuddyListID(Position), 0);
	}
	return true;
}

// 0x43c110, line 3267. A double click opens a private channel.
bool TBuddylistWindowBody::LeftButtonDoubleClick(int x, int y, int Flags)
{
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): the original's call of the base's base (the image's)
	if (!TWindow::LeftButtonDoubleClick(x, y, Flags)) {
		const int Position = GetBuddyAt(x, y);
		if (Position != -1)
			SendOpenPrivateConversation(GetBuddyListName(Position));
	}
	return true;
}

// 0x435590, line 3280
bool TBuddylistWindowBody::MouseWheel(int x, int y, int Code)
{
	if (!TWindow::MouseWheel(x, y, Code) && (Code == 1 || Code == 2))
		return ScrollBar->MouseWheel(x, y, Code);
	return true;
}

// 0x43c2e0, line 3312. A shortened name in full, in a new buffer each time.
void TBuddylistWindowBody::MouseHover(int x, int y)
{
	const int Position = GetBuddyAt(x, y);
	if (Position != -1) {
		const char *Name = GetBuddyListName(Position);
		const int MaxLength = GetFittingTextLength(SkinProperties().BuddylistWindow.Data28.FontNumber, Name,
			SkinProperties().BuddylistWindow.Data18.width);
		if (MaxLength < (int)strlen(Name)) {
			if (MouseOverText != nullptr)
				MouseOverText.reset();
			MouseOverText = nullptr;
			MouseOverText.reset(new char[36]);
			MouseOverText[0] = 0;
			TStringBuffer(MouseOverText.get(), 36).Append(Name);
			SetMouseOverText(MouseOverText.get());
		}
	}
}

// 0x435760, lines 3322-3327
bool TBuddylistWindowBody::HelpMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
	SetHelpHandle(19);
	const TComponent *Parent = GetParent();
	PATELLA_ASSERT(Parent!=nullptr);
	SetHelpSize(Parent->Width, Parent->Height);
	MoveHelpPosition(-OffsetX, -OffsetY);
	return true;
}

// 0x435a00, line 3340
void TBuddylistWindowBody::Arrange()
{
	int ContentHeight = GetContentHeight();
	if (ContentHeight <= Height)
		ContentHeight = Height;
	ScrollBar->Range->SetRange(0, ContentHeight - Height, ScrollBar->Range->Value, Height);
}

// 0x435bb0, line 3350. At least one row.
int TBuddylistWindowBody::getMinimumHeight()
{
	return (std::max)(ScrollBar->getMinimumHeight(), SkinProperties().BuddylistWindow.Data04);
}

// 0x435d50, line 3363. The stored height ("ContentWindow" 9) or the content.
int TBuddylistWindowBody::GetInitialHeight()
{
	int Height = GetContentWindowSize(9);
	if (Height == 0)
		Height = GetContentHeight();
	if (Height < getMinimumHeight())
		return getMinimumHeight();
	return Height;
}

// 0x435f20, line 3374
int TBuddylistWindowBody::GetContentHeight()
{
	return GetNumberOfBuddies() * SkinProperties().BuddylistWindow.Data04 + SkinProperties().Data868;
}

// 0x4360c0, line 3389. The row under x, y; -1 for none. Odd, kept: y is
// not tested against the top. y gets the scroll position in place
// (0x43611f, the parameter's slot): the original's catch wrapper printed the moved
// y (issue #34).
int TBuddylistWindowBody::GetBuddyAt(int x, int y)
{
	if (!IsMinimized()) {
		y += ScrollBar->Range->Value;
		const int Position = y / SkinProperties().BuddylistWindow.Data04;
		if (x >= 0 && x <= Width - ScrollBar->Width && Position < GetNumberOfBuddies())
			return Position;
	}
	return -1;
}
// NOLINTEND(cppcoreguidelines-owning-memory,cppcoreguidelines-pro-type-vararg)
