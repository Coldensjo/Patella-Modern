// DialogWindow.cpp: reconstructed from Tibia.exe 7.72 (0x4854b0-0x48b37f).
//
// The columns of content windows (TDialogWindow), their base class
// (TContentWindow) and the windows with a body and a frame (TSubWindow); see
// DialogWindow.h. The object opens with six functions without exception
// frames or line evidence (0x4854b0-0x4857af); they stay at the start, where
// the binary has them. GetFrameHeight and TContentWindow's constructor are
// called by ContentWindows.obj, which comes earlier in the link order, so
// they are not inline header code (a header copy would have been kept from
// that object). The other functions follow in line order. The binary moves
// four of them to the end of the object (Arrange, Shrink, Grow, Rearrange):
// each calls a function defined further down.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none.
//
// A column lays its children out from the top of Area, each as wide as the
// column. Rearrange is called after every change with a strategy: when a
// child is dragged it swaps places with a neighbour it overlaps by half; when
// a child grows the others shrink to their minimum heights from the bottom
// up; when one shrinks the windows below it take the space it gave up; when
// a new one comes the ones at the bottom shrink or close to make room; when
// the column gets smaller the windows that do not fit any more shrink or
// close. Arrange then fills the space below the last child with nine skins.

#include "DialogWindow.h"
#include "DialogElements.h"
#include "Preferences.h"
#include "Skins.h"
#include "Surface.h"
#include "Text.h" // IWYU pragma: keep (the object's own copy of the text table, see Text.h)
#include "GUI.h"
#include "common.h"

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-owning-memory off for the file:
// the original's raw owners (notes/ownership.md): the window tree owns its children, the message queue its blocks,
// the lists their entries
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

namespace {

// At 0x5cc428, constructed by the static initialiser 0x559e20 as
// TCallback(NULL, 0) and never used (MainWindow.cpp and MapWindow.cpp have
// one too); Dialogs.cpp's is the one before it (0x5cc420, initialiser
// 0x559e10).
// NOLINTNEXTLINE(bugprone-throwing-static-initialization): the binary's global; TCallback's constructor (GUI.cpp) stores two values
TCallback NoCallback(nullptr, 0);

} // namespace

// --- Functions without line evidence ---------------------------------------

// 0x4854b0. A column is as wide as the side bar.
int TDialogWindow::getMinimumWidth()
{
	return SkinProperties().Data184;
}

// 0x4854c0. Also inlined into TSubWindow's constructor.
TContentWindow::TContentWindow(int _Identifier, bool _Background)
{
	Identifier = _Identifier;
	Dragging = false;
	DragX = 0;
	DragY = 0;
	DragBottom = 0;
	BackgroundY = 0;
	Background = _Background;
}

// 0x485500 (deleting destructor 0x485790). The parts that are not children at
// the moment (a hidden "up" button, the parts of a minimized window) are
// deleted here (the close closed them); ~TWindow deletes the children.
TSubWindow::~TSubWindow()
{
	CheckClosed("TSubWindow");
	if (UpButton != nullptr && !IsChild(UpButton))
		delete UpButton;
	if (CloseButton != nullptr && !IsChild(CloseButton))
		delete CloseButton;
	if (MinimizeButton != nullptr && !IsChild(MinimizeButton))
		delete MinimizeButton;
	if (Icon != nullptr && !IsChild(Icon))
		delete Icon;
	if (Title != nullptr && !IsChild(Title))
		delete Title;
	if (Body != nullptr && !IsChild(Body))
		delete Body;
	if (Resizer1 != nullptr && !IsChild(Resizer1))
		delete Resizer1;
	if (Resizer2 != nullptr && !IsChild(Resizer2))
		delete Resizer2;
}

// The destructor's work until issue #108 (0x485500), in its order: the skill,
// battle and VIP windows store their body's height in the preferences
// ("ContentWindow"), the parts that are not children are closed where it
// deleted them (a minimized container's body sends its close); then the
// children.
void TSubWindow::OnClose()
{
	try {
		if (Identifier == 6 || Identifier == 7 || Identifier == 9)
			SetContentWindowSize(Identifier, Body->Height);
		if (UpButton != nullptr && !IsChild(UpButton))
			UpButton->Close();
		if (CloseButton != nullptr && !IsChild(CloseButton))
			CloseButton->Close();
		if (MinimizeButton != nullptr && !IsChild(MinimizeButton))
			MinimizeButton->Close();
		if (Icon != nullptr && !IsChild(Icon))
			Icon->Close();
		if (Title != nullptr && !IsChild(Title))
			Title->Close();
		if (Body != nullptr && !IsChild(Body))
			Body->Close();
		if (Resizer1 != nullptr && !IsChild(Resizer1))
			Resizer1->Close();
		if (Resizer2 != nullptr && !IsChild(Resizer2))
			Resizer2->Close();
	} catch (...) {
		CloseWhileUnwinding([this] { TContentWindow::OnClose(); });
		throw;
	}
	TContentWindow::OnClose();
}

// 0x485670. ContentWindows.cpp adds the body's height to get the window's.
int TSubWindow::GetFrameHeight()
{
	return Skins()[133].height + Skins()[132].height;
}

// 0x485690. A minimized window is its frame only.
int TSubWindow::getMinimumHeight()
{
	int Result = GetFrameHeight();
	if (Body != nullptr && !IsMinimized)
		Result += Body->getMinimumHeight();
	return Result;
}

// 0x4856c0. Nine background skins (Arrange).
TDialogWindow::TDialogWindow()
{
	Area = TRect(0, 0, 0, 0);
	Dragged = nullptr;
	Busy = false;
	FreeHeight = 0;
	UpdateFreeHeight = true;
	DrawingListLength = 9;
	DrawingList.reset(new TDrawing[9]);
}

// --- TDialogWindow ---------------------------------------------------------

// 0x4857b0, line 63. The catch blocks name no parameter.
//
// Issue #65: a sub-window's SetSize ends with the column's Rearrange(8)
// (FitChildren), which can delete the child being sized. The original then
// reads the deleted child's Next (a use after free); RemoveChild had set it
// to NULL before the delete, so its loop ends there. Ours ends there too,
// without the read: a child that is no longer in the column was deleted
// (strategies 2, 4, 8 and 32 delete children but never insert one, so the
// pointer compared is not a new child's). A child still in it goes on to its
// Next, as before.
void TDialogWindow::SetSize(int Width, int Height)
{
	TWindow::SetSize(Width, Height);
	Area.height = Height;
	Area.width = Width;
	TComponent *Child = FirstChild;
	while (Child != nullptr) {
		Child->SetSize(Width, Child->Height);
		Child = TWindow::IsChild(Child) ? Child->GetNext() : nullptr;
	}
	Rearrange(8, nullptr);
}

// 0x485970, line 86. The children's heights, whatever their minimum heights.
int TDialogWindow::getMinimumHeight()
{
	int Result = 0;
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->GetNext())
		Result += Child->Height;
	return Result;
}

// 0x485b10, line 109. The dragged child's place is a rectangle of
// SkinProperties.Data188; the child itself is drawn last, over the others.
void TDialogWindow::Draw(int Surface, int x, int y)
{
	int Position = 0;
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->GetNext()) {
		if (Child == Dragged) {
			GetSurface()->FillRectangle(Surface, TRect(x, Position + y, Width, Dragged->Height),
				SkinProperties().Data188);
		} else {
			Child->Draw(Surface, Child->OffsetX + x, Child->OffsetY + y);
		}
		Position += Child->Height;
	}
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): the original's call of the base's base (the image's)
	TComponent::Draw(Surface, x, y);
	if (Dragged != nullptr)
		Dragged->Draw(Surface, Dragged->OffsetX + x, Dragged->OffsetY + y);
}

// 0x48a490, line 169. The free space below the children (FreeHeight pixels
// from Top down) gets a frame of skins 26-33 around skin 7. The top row is
// cut to the free height, the bottom row starts below the top row's skins.
// Odd, kept: the bottom left skin (28) is as wide as the top left one (26),
// and the right edge (31) is as wide as skin 135.
void TDialogWindow::Arrange()
{
	const int Free = GetFreeHeight();
	for (int i = 0; i < DrawingListLength; i++)
		DrawingList[i] = TDrawing();
	if (Free != 0) {
		const int Top = Height - Free;
		int h = Free < Skins()[26].height ? Free : Skins()[26].height;
		if (h > 0)
			DrawingList[0] = TDrawing(26, 0, Top, Skins()[26].width, h, 0, 0);
		h = Free < Skins()[32].height ? Free : Skins()[32].height;
		if (h > 0)
			DrawingList[1] = TDrawing(32, Skins()[26].width, Top, Width - Skins()[27].width - Skins()[26].width, h, 0, 0);
		h = Free < Skins()[27].height ? Free : Skins()[27].height;
		if (h > 0)
			DrawingList[2] = TDrawing(27, Width - Skins()[27].width, Top, Skins()[27].width, h, 0, 0);
		h = Free - Skins()[26].height;
		if (h >= Skins()[28].height)
			h = Skins()[28].height;
		if (h > 0)
			DrawingList[3] = TDrawing(28, 0, Height - h, Skins()[26].width, h, 0, 0);
		h = Free - Skins()[32].height;
		if (h >= Skins()[33].height)
			h = Skins()[33].height;
		if (h > 0)
			DrawingList[4] = TDrawing(33, Skins()[28].width, Height - h, Width - Skins()[29].width - Skins()[28].width, h,
				0, 0);
		h = Free - Skins()[27].height;
		if (h >= Skins()[29].height)
			h = Skins()[29].height;
		if (h > 0)
			DrawingList[5] = TDrawing(29, Width - Skins()[29].width, Height - h, Skins()[29].width, h, 0, 0);
		h = Free - Skins()[28].height - Skins()[26].height;
		if (h > 0)
			DrawingList[6] = TDrawing(30, 0, Skins()[26].height + Top, Skins()[30].width, h, 0, 0);
		h = Free - Skins()[33].height - Skins()[32].height;
		if (h > 0)
			DrawingList[7] = TDrawing(7, Skins()[30].width, Skins()[32].height + Top,
				Width - Skins()[31].width - Skins()[30].width, h, 0, Top % Skins()[7].height);
		h = Free - Skins()[29].height - Skins()[27].height;
		if (h > 0)
			DrawingList[8] = TDrawing(31, Width - Skins()[31].width, Skins()[27].height + Top, Skins()[135].width, h, 0, 0);
	}
}

// 0x485d70, line 181. Children are as wide as the column.
void TDialogWindow::InsertChild(TComponent *Child)
{
	TWindow::InsertChild(Child);
	Child->SetSize(Width, Child->Height);
}

// 0x485f10, line 193
void TDialogWindow::AppendChild(TComponent *Child)
{
	TWindow::AppendChild(Child);
	Child->SetSize(Width, Child->Height);
}

// 0x4860b0, line 205
void TDialogWindow::DeleteChild(TComponent *Child)
{
	if (Dragged == Child)
		Dragged = nullptr;
	TWindow::DeleteChild(Child);
	Rearrange(32, nullptr);
}

// 0x486260, line 217
void TDialogWindow::InsertChildAfter(TComponent *Previous, TComponent *Child)
{
	TWindow::InsertChildAfter(Previous, Child);
	Child->SetSize(Width, Child->Height);
}

// 0x486400, line 233. The children of a column are content windows.
TContentWindow *TDialogWindow::GetContentWindow(int Identifier)
{
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->GetNext()) {
		if ((static_cast<TContentWindow *>(Child))->Identifier == Identifier)
			return static_cast<TContentWindow *>(Child);
	}
	return nullptr;
}

// 0x4865b0, line 247. The windows that the next game opens again (not the
// containers).
void TDialogWindow::StoreOpenDialogs()
{
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->GetNext()) {
		if ((static_cast<TContentWindow *>(Child))->Identifier < 0x40)
			AddOpenDialog((static_cast<TContentWindow *>(Child))->Identifier);
	}
}

// 0x48aea0, lines 271-297; jump table 0x48b338 with the byte table 0x48b354
// (Strategy-1 from 0 to 31). Calls during a rearrangement (DeleteChild in
// FitChildren and MakeRoom, the children's SetSize) return at once. Strategy
// 4 lays the children out twice.
void TDialogWindow::Rearrange(int Strategy, TComponent *Work)
{
	if (Busy)
		return;
	Busy = true;
	switch (Strategy) {
	case 1:
		DragChild(Work);
		PositionChildren(true);
		break;
	case 2:
		PositionChildren(false);
		Shrink();
		break;
	case 4:
		PATELLA_ASSERT(Work!=nullptr);
		UpdateFreeHeight = false;
		PositionChildren(false);
		Grow(Work);
		PositionChildren(false);
		break;
	case 8:
		PositionChildren(false);
		FitChildren();
		break;
	case 16:
		PATELLA_ASSERT(Work!=nullptr);
		MakeRoom(Work);
		AppendChild(Work);
		PositionChildren(false);
		break;
	case 32:
		PositionChildren(false);
		break;
	default:
		TERROR1("unknown strategy", Strategy);
	}
	Arrange();
	UpdateFreeHeight = true;
	Busy = false;
}

// 0x486760, line 317. The dragged child keeps the position the mouse gave it.
void TDialogWindow::PositionChildren(bool SkipDragged)
{
	const int x = Area.x;
	int y = Area.y;
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->GetNext()) {
		if (Child != Dragged || !SkipDragged)
			Child->SetPosition(x, y);
		y += Child->Height;
	}
}

// 0x486930, line 353 (strategy 8). The children that do not fit any more
// shrink, down to their minimum heights; those that still do not fit are
// deleted (DeleteChild's Rearrange returns at once). Clears Busy itself,
// before Rearrange does.
void TDialogWindow::FitChildren()
{
	int Space = Area.height;
	TComponent *Child = FirstChild;
	while (Child != nullptr) {
		if (Space < Child->Height) {
			TComponent *Delete = nullptr;
			if (Space < Child->getMinimumHeight()) {
				Delete = Child;
			} else if (Space < Child->Height) {
				Child->SetSize(Area.width, Space);
				Child->Arrange();
				if (Space < Child->Height)
					Delete = Child;
				else
					Space -= Child->Height;
			}
			Child = Child->GetNext();
			if (Delete != nullptr)
				DeleteChild(Delete);
		} else {
			Space -= Child->Height;
			Child = Child->GetNext();
		}
	}
	Busy = false;
}

// 0x48a990, lines 377-394 (strategy 2). While the children are higher than
// the column, the lowest child above its minimum height gives up what is
// missing (or what it can) and the children below it move up. Returns false
// when no child can shrink any more. The second GetFreeHeight cannot be
// positive here; the test is the binary's.
bool TDialogWindow::Shrink()
{
	while (GetFreeHeight() < 0) {
		TComponent *Child = GetLastChild();
		const int Missing = -GetFreeHeight();
		if (Missing < 0)
			break;
		TComponent *Shrinking;
		do {
			if (Child == nullptr)
				return false;
			Shrinking = Child;
			Child = Child->GetPrevious();
		} while (Shrinking->Height == Shrinking->getMinimumHeight());
		int DeltaHeight = Shrinking->Height - Shrinking->getMinimumHeight();
		if (Missing < DeltaHeight)
			DeltaHeight = Missing;
		PATELLA_ASSERT_VALUES(DeltaHeight>0, DeltaHeight);
		Shrinking->SetSize(Shrinking->Width, Shrinking->Height - DeltaHeight);
		for (TComponent *Next = Shrinking->GetNext(); Next != nullptr; Next = Next->GetNext())
			Next->SetPosition(Next->OffsetX, Next->OffsetY - DeltaHeight);
	}
	return true;
}

// 0x486b30, lines 419-429 (strategy 1). The dragged child swaps places with
// the child above it when its top edge is above that child's middle, and
// with the child below it when its bottom edge is below that child's middle.
void TDialogWindow::DragChild(TComponent *Work)
{
	Dragged = Work;
	if (Dragged != nullptr) {
		TComponent *Previous = nullptr;
		TComponent *Search = FirstChild;
		TComponent *Next;
		for (;;) {
			PATELLA_ASSERT(Search!=nullptr);
			Next = Search->GetNext();
			if (Dragged == Search)
				break;
			Previous = Search;
			Search = Next;
		}
		if (Previous != nullptr && Dragged->OffsetY < Previous->Height / 2 + Previous->OffsetY)
			SwapChildren(Previous, Dragged);
		if (Next != nullptr && Dragged->Height + Dragged->OffsetY > Next->Height / 2 + Next->OffsetY)
			SwapChildren(Next, Dragged);
	}
}

// 0x486e20, line 477 (strategy 16). Before Work is appended, the children
// from the bottom up shrink to make room for its height (at least its
// minimum height); a child that would have to shrink below its minimum is
// deleted. Work gets at most the space there is then.
void TDialogWindow::MakeRoom(TComponent *Work)
{
	int Space = Area.height;
	for (TComponent *Child = GetFirstChild(); Child != nullptr; Child = Child->GetNext())
		Space -= Child->Height;
	if (Space < Work->Height) {
		int Needed = Work->getMinimumHeight();
		if (Space > Needed)
			Needed = Space;
		TComponent *Child = GetLastChild();
		while (Child != nullptr && Space < Needed) {
			TComponent *Previous = Child->GetPrevious();
			const int OldHeight = Child->Height;
			Child->SetSize(Child->Width, OldHeight - Needed + Space);
			const int NewHeight = Child->Height;
			Space += OldHeight - NewHeight;
			if (Space >= Needed)
				break;
			DeleteChild(Child);
			Space += NewHeight;
			Child = Previous;
		}
		if (Space >= Work->Height)
			Space = Work->Height;
		Work->SetSize(Work->Width, Space);
	}
}

// 0x48acb0, line 505 (strategy 4). The space Work gave up (the free height
// above the one before; UpdateFreeHeight is off) goes to the children below
// it, from the bottom up.
void TDialogWindow::Grow(TComponent *Work)
{
	TComponent *Child = GetLastChild();
	if (Child != Work) {
		const int Free = GetFreeHeight();
		if (Free > FreeHeight) {
			int Delta = Free - FreeHeight;
			for (; Child != nullptr && Delta > 0 && Child != Work; Child = Child->GetPrevious()) {
				const int OldHeight = Child->Height;
				Child->SetSize(Child->Width, OldHeight + Delta);
				Delta += OldHeight - Child->Height;
			}
		}
	}
}

// 0x487060, line 524. Negative when the children do not fit.
int TDialogWindow::GetFreeHeight()
{
	const int Available = Area.height;
	int Used = 0;
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->GetNext())
		Used += Child->Height;
	if (UpdateFreeHeight)
		FreeHeight = Available - Used;
	return Available - Used;
}

// --- TContentWindow --------------------------------------------------------

// 0x487210, lines 681-692. The background is skin 7, tiled from the row that
// continues the column's pattern (BackgroundY).
void TContentWindow::Draw(int Surface, int x, int y)
{
	if (Background) {
		PATELLA_ASSERT_VALUES(x>=0, x);
		PATELLA_ASSERT_VALUES(y>=0, y);
		DrawSkin(Surface, x, y, Width, Height, 7, 0, BackgroundY);
	}
	// NOLINTNEXTLINE(bugprone-parent-virtual-call): the original's call of the base's base (the image's)
	TComponent::Draw(Surface, x, y);
	for (TComponent *Child = FirstChild; Child != nullptr; Child = Child->GetNext())
		Child->Draw(Surface, Child->OffsetX + x, Child->OffsetY + y);
}

// 0x4875a0, line 703
void TContentWindow::Arrange()
{
	TWindow::Arrange();
	BackgroundY = OffsetY % Skins()[7].height;
}

// 0x487740, line 714. The catch blocks name no parameter.
void TContentWindow::ReleaseCapture(TComponent *Component)
{
	Dragging = false;
	TWindow::ReleaseCapture(Component);
}

// 0x4878d0, line 734. While dragging, the window follows the mouse inside
// the column, which rearranges the others around it.
bool TContentWindow::MouseMove(int x, int y)
{
	if (Dragging) {
		TDialogWindow *Column = static_cast<TDialogWindow *>(GetParent());
		int NewY = OffsetY - DragY + y;
		if (NewY < 0)
			NewY = 0;
		if (NewY > Column->Height - Height)
			NewY = Column->Height - Height;
		SetPosition(OffsetX, NewY);
		Column->Rearrange(1, this);
		return true;
	}
	return TWindow::MouseMove(x, y);
}

// 0x487ad0, line 751. A press that no child takes starts dragging the
// window: SetCapture(NULL) makes the parents send every mouse event here.
bool TContentWindow::LeftButtonDown(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonDown(x, y, Flags)) {
		Dragging = true;
		SetCapture(nullptr);
		DragX = x;
		DragY = y;
		DragBottom = Height + OffsetY - y;
	}
	return true;
}

// 0x487ca0, line 768
bool TContentWindow::LeftButtonUp(int x, int y, int Flags)
{
	if (Dragging) {
		ReleaseCapture(nullptr);
		(static_cast<TDialogWindow *>(GetParent()))->Rearrange(1, nullptr);
		return true;
	}
	return TWindow::LeftButtonUp(x, y, Flags);
}

// --- TSubWindow ------------------------------------------------------------

// 0x487e80, lines 791-804. The try block starts after the base class. The
// body is a child from the start; the buttons, the icon and the title come
// later from the setters.
TSubWindow::TSubWindow(int _Identifier, TSubWindowBody *_Body, bool _Resizable, bool _Background)
	: TContentWindow(_Identifier, _Background)
{
	IsMinimized = false;
	RestoreHeight = 0;
	Data68 = 0;
	DrawingListLength = 8;
	DrawingList.reset(new TDrawing[8]);
	UpButton = nullptr;
	CloseButton = nullptr;
	MinimizeButton = nullptr;
	Icon = nullptr;
	Title = nullptr;
	PATELLA_ASSERT(_Body!=nullptr);
	Body = _Body;
	TWindow::InsertChild(Body);
	if (_Resizable) {
		Resizer1 = new TResizer(this, false, true, false);
		Resizer2 = new TResizer(this, false, true, true);
		TWindow::AppendChild(Resizer1);
		TWindow::AppendChild(Resizer2);
	} else {
		Resizer1 = nullptr;
		Resizer2 = nullptr;
	}
}

// 0x4881f0, line 861. Not lower than the minimum height; a minimized window
// keeps its frame height (Style194.Data4C). The column is told whether the
// window grew (2), shrank (4) or kept its height (32), then fits its
// children into its height (8).
void TSubWindow::SetSize(int Width, int Height)
{
	TDialogWindow *Column = static_cast<TDialogWindow *>(GetParent());
	int Change = 0;
	if (IsMinimized) {
		TWindow::SetSize(Width, SkinProperties().Style194.Data4C);
	} else {
		if (Height > this->Height)
			Change = 1;
		if (Height < this->Height)
			Change = -1;
		if (Height < getMinimumHeight())
			Height = getMinimumHeight();
		TWindow::SetSize(Width, Height);
		SetChildArea(Body, SkinProperties().Style194.Data50);
		FitToChild(Body, SkinProperties().Style194.Data50, false);
		if (Resizer1 != nullptr)
			SetChildArea(Resizer1, SkinProperties().Style194.Data60);
		if (Resizer2 != nullptr)
			SetChildArea(Resizer2, SkinProperties().Style194.Data70);
	}
	Arrange();
	if (Column != nullptr) {
		switch (Change) {
		case -1:
			Column->Rearrange(4, this);
			break;
		case 0:
			Column->Rearrange(32, nullptr);
			break;
		case 1:
			Column->Rearrange(2, this);
			break;
		default:
			break;
		}
		Column->Rearrange(8, nullptr);
	}
}

// 0x488510, lines 891-928. A container's body updates its "up" button
// first. The frame: skins 128-131 in the corners, 132 and 133 along the top
// and bottom edges, 134 and 135 along the left and right ones. Odd, kept:
// the right edge starts at the top edge's height (Skins[132].height), not
// below the top right corner.
void TSubWindow::Arrange()
{
	TContentWindow::Arrange();
	if (Identifier >= 0x40) {
		PATELLA_ASSERT(Body!=nullptr);
		UpdateContainerUpButton(Body);
	}
	DrawingList[0] = TDrawing(128, 0, 0, 0, 0, 0, 0);
	DrawingList[1] = TDrawing(132, Skins()[128].width, 0, Width - Skins()[129].width - Skins()[128].width,
		Skins()[132].height, 0, 0);
	DrawingList[2] = TDrawing(129, Width - Skins()[129].width, 0, 0, 0, 0, 0);
	DrawingList[3] = TDrawing(134, 0, Skins()[128].height, Skins()[134].width, Height - Skins()[130].height - Skins()[128].height,
		0, 0);
	DrawingList[4] = TDrawing(135, Width - Skins()[135].width, Skins()[132].height, Skins()[135].width,
		Height - Skins()[131].height - Skins()[129].height, 0, 0);
	DrawingList[5] = TDrawing(130, 0, Height - Skins()[130].height, 0, 0, 0, 0);
	DrawingList[6] = TDrawing(133, Skins()[130].width, Height - Skins()[133].height, Width - Skins()[131].width - Skins()[130].width,
		Skins()[133].height, 0, 0);
	DrawingList[7] = TDrawing(131, Width - Skins()[131].width, Height - Skins()[131].height, 0, 0, 0, 0);
}

// 0x4889c0, line 950. The resizers get the press first, even where they
// overlap the body.
bool TSubWindow::LeftButtonDown(int x, int y, int Flags)
{
	if (!IsMinimized && Resizer1 != nullptr && In(x - Resizer1->OffsetX, 0, Resizer1->Width - 1)
		&& In(y - Resizer1->OffsetY, 0, Resizer1->Height - 1)
		&& Resizer1->LeftButtonDown(x - Resizer1->OffsetX, y - Resizer1->OffsetY, Flags))
		return true;
	if (!IsMinimized && Resizer2 != nullptr && In(x - Resizer2->OffsetX, 0, Resizer2->Width - 1)
		&& In(y - Resizer2->OffsetY, 0, Resizer2->Height - 1)
		&& Resizer2->LeftButtonDown(x - Resizer2->OffsetX, y - Resizer2->OffsetY, Flags))
		return true;
	return TContentWindow::LeftButtonDown(x, y, Flags);
}

// 0x488c20, line 972
bool TSubWindow::MouseMove(int x, int y)
{
	if (!IsMinimized && Resizer1 != nullptr && In(x - Resizer1->OffsetX, 0, Resizer1->Width - 1)
		&& In(y - Resizer1->OffsetY, 0, Resizer1->Height - 1)
		&& Resizer1->MouseMove(x - Resizer1->OffsetX, y - Resizer1->OffsetY))
		return true;
	if (!IsMinimized && Resizer2 != nullptr && In(x - Resizer2->OffsetX, 0, Resizer2->Width - 1)
		&& In(y - Resizer2->OffsetY, 0, Resizer2->Height - 1)
		&& Resizer2->MouseMove(x - Resizer2->OffsetX, y - Resizer2->OffsetY))
		return true;
	return TContentWindow::MouseMove(x, y);
}

// 0x488e80, line 986. A double click that no child takes minimizes or
// restores the window (event 9).
bool TSubWindow::LeftButtonDoubleClick(int x, int y, int Flags)
{
	if (!TWindow::LeftButtonDoubleClick(x, y, Flags))
		HandleEvent(9);
	return true;
}

// 0x489030, line 1002. Event 10 (the close button) deletes the window.
void TSubWindow::HandleEvent(int Event)
{
	if (Event == 10) {
		GetParent()->DeleteChild(this);
	} else if (Event == 9) {
		if (IsMinimized)
			Maximize();
		else
			Minimize();
	}
}

// 0x489220, lines 1009-1020. The assertion comes before the try block. The
// body is told first (+0xa0, without a NULL check) and leaves the window
// with the resizers.
void TSubWindow::Minimize()
{
	PATELLA_ASSERT(IsMinimized==false);
	IsMinimized = true;
	RestoreHeight = Height;
	Body->Minimize();
	if (Resizer1 != nullptr)
		RemoveChild(Resizer1);
	if (Resizer2 != nullptr)
		RemoveChild(Resizer2);
	if (Body != nullptr)
		RemoveChild(Body);
	if (MinimizeButton != nullptr)
		MinimizeButton->SetSkins(140, 141);
	SetSize(Width, SkinProperties().Style194.Data4C);
}

// 0x489500, lines 1027-1037. The assertion comes before the try block.
void TSubWindow::Maximize()
{
	PATELLA_ASSERT(IsMinimized==true);
	IsMinimized = false;
	Body->Maximize();
	if (Body != nullptr)
		AppendChild(Body);
	if (Resizer1 != nullptr)
		AppendChild(Resizer1);
	if (Resizer2 != nullptr)
		AppendChild(Resizer2);
	if (MinimizeButton != nullptr)
		MinimizeButton->SetSkins(138, 139);
	SetSize(Width, RestoreHeight);
}

// 0x4897d0, line 1049
void TSubWindow::ShowUpButton()
{
	if (UpButton != nullptr && !IsChild(UpButton))
		InsertChild(UpButton);
}

// 0x489970, line 1061. The button stays the window's (the destructor
// deletes it).
void TSubWindow::HideUpButton()
{
	if (UpButton != nullptr && IsChild(UpButton) == true)
		RemoveChild(UpButton);
}

// 0x489b10, line 1075. Each setter deletes the part it replaces.
void TSubWindow::SetUpButton(TComponent *Button)
{
	if (UpButton != nullptr) {
		if (IsChild(UpButton))
			DeleteChild(UpButton);
		else
			DeleteComponent(UpButton);
	}
	UpButton = Button;
	InsertChild(Button);
	SetChildPosition(UpButton, SkinProperties().Style194.Data20.x, SkinProperties().Style194.Data20.y);
}

// 0x489cf0, line 1088
void TSubWindow::SetCloseButton(TComponent *Button)
{
	if (CloseButton != nullptr) {
		if (IsChild(CloseButton))
			DeleteChild(CloseButton);
		else
			DeleteComponent(CloseButton);
	}
	CloseButton = Button;
	InsertChild(Button);
	SetChildPosition(CloseButton, SkinProperties().Style194.Data10.x, SkinProperties().Style194.Data10.y);
}

// 0x489ed0, line 1101
void TSubWindow::SetMinimizeButton(TSkinButton *Button)
{
	if (MinimizeButton != nullptr) {
		if (IsChild(MinimizeButton))
			DeleteChild(MinimizeButton);
		else
			DeleteComponent(MinimizeButton);
	}
	MinimizeButton = Button;
	InsertChild(Button);
	SetChildPosition(MinimizeButton, SkinProperties().Style194.Data18.x, SkinProperties().Style194.Data18.y);
}

// 0x48a0b0, line 1114
void TSubWindow::SetIcon(TComponent *_Icon)
{
	if (Icon != nullptr) {
		if (IsChild(Icon))
			DeleteChild(Icon);
		else
			DeleteComponent(Icon);
	}
	Icon = _Icon;
	InsertChild(_Icon);
	SetChildArea(Icon, SkinProperties().Style194.Data00);
}

// 0x48a2a0, line 1127
void TSubWindow::SetTitle(TComponent *_Title)
{
	if (Title != nullptr) {
		if (IsChild(Title))
			DeleteChild(Title);
		else
			DeleteComponent(Title);
	}
	Title = _Title;
	InsertChild(_Title);
	SetChildArea(Title, SkinProperties().Style194.Data28);
}
// NOLINTEND(cppcoreguidelines-owning-memory)
