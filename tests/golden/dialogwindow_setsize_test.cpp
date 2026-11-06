// dialogwindow_setsize (issue #65): TDialogWindow::SetSize when a child's own SetSize deletes children.
//
// A sub-window's SetSize ends with its column's Rearrange(8) (FitChildren), which deletes the children that do not
// fit the column's new height, the child being sized among them. The original's SetSize (0x4857b0) then went on with
// the deleted child's Next: a use after free, which read NULL (RemoveChild had cleared it) as long as the heap left
// the freed object as it was. AddressSanitizer reports that read (heap-use-after-free) in the build before the fix;
// these cases check that the fix keeps the original's outcome: the loop ends at a deleted child (the windows below it
// keep their widths), and goes on past a live one.
//
// The column and its windows are this build's own (DialogWindow.cpp, GUI.cpp), with a body of this file; no corpus
// and no image. The executable links the golden runner's objects (the client's code and the stubs it needs) with
// this file in place of runner.cpp, so it defines runner.cpp's globals (golden_test.h's declarations; that header is
// not included, its CHECK is doctest's here).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ContentWindows.h"
#include "DialogWindow.h"
#include "GUI.h"
#include "Skins.h"
#include "common.h"

#include <memory>

int Failures = 0;
int Checks = 0;
int FailuresPrinted = 20;
namespace {
uint32 Seed = 12345;
} // namespace
uint32 Random()
{
	Seed = Seed * 1103515245 + 12345;
	return Seed >> 8;
}
uint32 GetRandomSeed() { return Seed; }
void SetRandomSeed(uint32 Value) { Seed = Value; }
void RandomText(char *Text, int Length)
{
	for (int i = 0; i < Length; i++)
		Text[i] = 'a';
	Text[Length] = 0;
}

namespace {

// A body with a fixed minimum height.
class TTestBody : public TSubWindowBody {
public:
	explicit TTestBody(int _MinimumHeight) : MinimumHeight(_MinimumHeight) {}
	int getMinimumHeight() override { return MinimumHeight; }
	int GetInitialHeight() override { return MinimumHeight; }
	int GetContentHeight() override { return MinimumHeight; }

private:
	int MinimumHeight;
};

constexpr int OldWidth = 176;
constexpr int NewWidth = 180;

// An empty column, OldWidth wide and 1000 pixels high.
struct TColumn {
	TColumn()
	{
		InitSkins();
		InitSkinProperties();
		Column = std::make_unique<TDialogWindow>();
		Column->SetSize(OldWidth, 1000);
	}
	std::unique_ptr<TDialogWindow> Column;
};

// A sub-window (identifier 10 or more, below 0x40: no container, no stored size) of the body's minimum height plus
// the frame, Extra pixels higher than that, appended to the column, which owns it from then on (as it owns its body).
TSubWindow *Append(TDialogWindow &Column, int Identifier, int BodyMinimum, int Extra)
{
	auto Window = std::make_unique<TSubWindow>(Identifier, std::make_unique<TTestBody>(BodyMinimum).release(), false,
		false);
	Window->SetSize(OldWidth, TSubWindow::GetFrameHeight() + BodyMinimum + Extra);
	TSubWindow *Result = Window.get();
	Column.AppendChild(Window.release());
	return Result;
}

[[nodiscard]] int Count(const TDialogWindow &Column)
{
	int n = 0;
	for (const TComponent *Child = Column.FirstChild; Child != nullptr; Child = Child->Next)
		n++;
	return n;
}

} // namespace

TEST_CASE("the column's SetSize sizes every child that fits")
{
	const TColumn C;
	const TSubWindow *A = Append(*C.Column, 10, 20, 0);
	const TSubWindow *B = Append(*C.Column, 11, 20, 0);
	REQUIRE(Count(*C.Column) == 2);
	C.Column->SetSize(NewWidth, 500);
	CHECK(Count(*C.Column) == 2);
	CHECK(A->Width == NewWidth);
	CHECK(B->Width == NewWidth);
	CHECK(B->OffsetY == A->Height);
}

TEST_CASE("a child deleted by an earlier child's SetSize is not sized")
{
	const TColumn C;
	const TSubWindow *A = Append(*C.Column, 10, 1, 4);
	Append(*C.Column, 11, 100, 0);
	REQUIRE(Count(*C.Column) == 2);
	// A fits; B's minimum height does not fit below it: A's SetSize deletes B, and the loop ends after A.
	C.Column->SetSize(NewWidth, TSubWindow::GetFrameHeight() + 10);
	CHECK(Count(*C.Column) == 1);
	CHECK(C.Column->FirstChild == A);
	CHECK(A->Width == NewWidth);
	CHECK(A->Next == nullptr);
}

TEST_CASE("a child deleted by its own SetSize ends the loop, as in the original")
{
	const TColumn C;
	Append(*C.Column, 10, 100, 0);
	const TSubWindow *B = Append(*C.Column, 11, 1, 4);
	REQUIRE(Count(*C.Column) == 2);
	// A's minimum height does not fit the new height: A's own SetSize deletes A (FitChildren) and keeps B, which
	// fits. The original's loop then read the deleted A's Next (NULL) and ended: B is not sized by it and keeps its
	// width; the closing Rearrange(8) moves it to the top. Before the fix ASan reports the read of A.
	C.Column->SetSize(NewWidth, TSubWindow::GetFrameHeight() + 10);
	CHECK(Count(*C.Column) == 1);
	CHECK(C.Column->FirstChild == B);
	CHECK(B->Width == OldWidth);
	CHECK(B->OffsetY == 0);
	CHECK(C.Column->Width == NewWidth);
	CHECK(C.Column->Area.height == TSubWindow::GetFrameHeight() + 10);
}
