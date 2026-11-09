// Talk.cpp: reconstructed from Tibia.exe 7.72, in assertion/catch line order.
// Header COMDATs are in Talk.h; Container.h supplies the filter-list vector.
// Ghidra blocks 0x502f70 and 0x5036c0 belong to Remove and GetLine.
//
// The try blocks and catch lines the comments name are the original's (its
// catch wrappers, common.h); since issue #78 the functions here have none.
#include "Talk.h"
#include "Text.h"
#include "Utils.h"
#include "graphics.h"
#include "sysinc.h"
#include "types/IndexedVector.h"
#include "Preferences.h"
#include "common.h"
#include "platform/Platform.h"
#include "types/File.h"
#include "types/FixedString.h"
#include "types/Time32.h"
#include "compat/crt71.h"
#include <ctime>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Issue #111 (clang-tidy, the first look at this file in patella_core): cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory off for the file:
// the original's malloc'd entries and texts, freed by their lists; the original's raw owners (notes/ownership.md):
// the window tree owns its children, the message queue its blocks, the lists their entries
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)

// The data (0x71dbd0-0x71eff4) is TTalk's (Talk.h, issue #107).

static_assert(sizeof(TTalkbox) == 0x1c0, "talkbox layout");
static_assert(offsetof(TTalkbox, Text) == 0x20 && offsetof(TTalkbox, Left) == 0x1b0, "talkbox lines (TFixedString, issue #57)");
// No pointers: the same on x64. Text a std::array of TFixedStrings (issue #60).
static_assert(sizeof(TTalkbox) == 0x1c0 && offsetof(TTalkbox, Text) == 0x20 && sizeof(TTalkbox::Text) == 0x190,
	"TTalkbox's layout");
static_assert(sizeof(TTalkLine::Time) == 4, "statement time: the original's 4 bytes (Time32, issue #62)");

// 0x4fd440, no line evidence; leading Talk.cpp helpers remain in link order.
bool patella::TTalk::GetTalkChanged()
{
	const bool Result = TalkChanged;
	TalkChanged = false;
	return Result;
}
// 0x4fd450
int patella::TTalk::GetNumberOfTalkboxes() const
{
	int Count = 0;
	for (int Position = 0; Position < TALKBOXES; Position++)
		if (Talkboxes[Position].State == 1)
			Count++;
	return Count;
}
// 0x4fd4c0
void patella::TTalk::SetTalkChanged() { TalkChanged = true; }
// 0x4fd4d0
const char *patella::TTalk::GetStatusMessage() const { return StatusMessageTime != 0 ? StatusMessage.Chars : nullptr; }
// 0x4fd4e0
void patella::TTalk::SetIgnoreEnabled(bool Value) { IgnoreEnabled = Value; }
// 0x4fd4f0
bool patella::TTalk::GetIgnoreEnabled() const { return IgnoreEnabled; }
// 0x4fd500
void patella::TTalk::SetBuddyIgnore(bool Value) { BuddyIgnore = Value; }
// 0x4fd510
bool patella::TTalk::GetBuddyIgnore() const { return BuddyIgnore; }
// 0x4fd520
void patella::TTalk::SetIgnoreListEnabled(bool Value) { IgnoreListEnabled = Value; }
// 0x4fd530
bool patella::TTalk::GetIgnoreListEnabled() const { return IgnoreListEnabled; }
// 0x4fd540
void patella::TTalk::SetTalkThrottle() { TalkThrottle = GetMilliseconds() + 1000; }
// 0x4fd550
bool patella::TTalk::IsTalkThrottled()
{
	if (TalkThrottle != 0) {
		if (GetMilliseconds() < TalkThrottle)
			return true;
		TalkThrottle = 0;
	}
	return false;
}
// 0x4fd580
bool patella::TTalk::GetConsoleChanged()
{
	const bool Result = ConsoleChanged;
	ConsoleChanged = false;
	return Result;
}
// 0x4fd590
void patella::TTalk::SetConsoleChanged() { ConsoleChanged = true; }

// 0x4fd9e0, lines 81-107
TRGBColor *patella::TTalk::GetTalkColor(TRGBColor *Color, int TalkMode)
{
	switch (TalkMode) {
		// clang-format off
	case 0: TERROR("none is talking");
	case 1: case 2: case 3: case 5: case 7: case 8:
		Color->R = 240; Color->G = 240; Color->B = 0; break;
	case 4:
		Color->R = 96; Color->G = 248; Color->B = 248; break;
	case 6: case 19: case 20: case 21: case 23:
		Color->R = 240; Color->G = 240; Color->B = 240; break;
	case 9: case 10: case 11: case 13: case 14: case 15: case 18: case 25:
		Color->R = 248; Color->G = 96; Color->B = 96; break;
	case 12:
		Color->R = 240; Color->G = 180; Color->B = 0; break;
	case 16: case 17:
		Color->R = 255; Color->G = 102; Color->B = 0; break;
	case 22:
		Color->R = 0; Color->G = 240; Color->B = 0; break;
	case 24:
		Color->R = 160; Color->G = 160; Color->B = 255; break;
	default: TERROR1("something unknown is talking", TalkMode);
		// clang-format on
	}
	return Color;
}
// 0x4fdd80, line 142
TTalkbox *patella::TTalk::GetTalkbox(int Number)
{
	PATELLA_ASSERT_VALUES(In(Number,1,GetNumberOfTalkboxes()), Number, GetNumberOfTalkboxes());
	int Position = 0;
	while (Talkboxes[Position].State != 1 || --Number != 0)
		Position++;
	// MSVC's analysis (C28020, issue #111) takes Position as any int: the assertion keeps Number within the open
	// talkboxes, so the loop stops at one of them.
	PATELLA_MSVC_WARNING(suppress : 28020)
	return &Talkboxes[Position];
}
// 0x4fdf10, line 158
int patella::TTalk::GetTalkboxMode(int Number)
{
	return GetTalkbox(Number)->Mode;
}
// 0x4fe0b0, line 180
void patella::TTalk::GetTalkboxPosition(int Number, int *x, int *y, int *z)
{
	const TTalkbox *Box = GetTalkbox(Number);
	const int Mode = Box->Mode;
	if (Mode == 22 || Mode == 19 || Mode == 18 || Mode == 4 || Mode == 9 || Mode == 11)
		*x = *y = *z = 0;
	else {
		*x = Box->x;
		*y = Box->y;
		*z = Box->z;
	}
}
// 0x4fe2b0, line 196
void patella::TTalk::GetTalkboxRectangle(int Number, int *x, int *y, int *Width, int *Height)
{
	const TTalkbox *Box = GetTalkbox(Number);
	*x = Box->Left;
	*y = Box->Top;
	*Width = Box->Width;
	*Height = Box->Height;
}
// 0x4fe470, line 208
TRGBColor *patella::TTalk::GetTalkboxColor(TRGBColor *Color, int Number)
{
	GetTalkColor(Color, GetTalkbox(Number)->Mode);
	return Color;
}
// 0x4fe620, line 220
int patella::TTalk::GetTalkboxLines(int Number)
{
	return GetTalkbox(Number)->Lines;
}
// 0x4fe7c0, line 232
const char *patella::TTalk::GetTalkboxLine(int Number, int Line)
{
	return GetTalkbox(Number)->Text[Line];
}
// 0x4fe960, line 246
void patella::TTalk::RemoveTalkbox(int Position)
{
	PATELLA_ASSERT_VALUES(In(Position,0,TALKBOXES-1), Position);
	TTalkbox &Box = Talkboxes[Position];
	int Oldest = TalkOrder;
	int Replacement = -1;
	for (int Search = 0; Search < TALKBOXES; Search++) {
		TTalkbox &Other = Talkboxes[Search];
		if (Search == Position || Other.State != 2)
			continue;
		bool Match = false;
		if (Box.Mode == 1 || Box.Mode == 2 || Box.Mode == 3)
			Match = Other.Mode == Box.Mode && strcmp(Other.Text[0], Box.Text[0]) == 0
				&& Other.x == Box.x && Other.y == Box.y && Other.z == Box.z;
		if (Box.Mode == 16 || Box.Mode == 17)
			Match = (Other.Mode == 16 || Other.Mode == 17)
				&& Other.x == Box.x && Other.y == Box.y && Other.z == Box.z;
		if (Box.Mode == 19 || Box.Mode == 18 || Box.Mode == 9 || Box.Mode == 11)
			Match = Other.Mode == 19 || Other.Mode == 18 || Other.Mode == 9 || Other.Mode == 11;
		if (Box.Mode == 4)
			Match = Other.Mode == 4;
		if (Match && Other.Order < Oldest) {
			Oldest = Other.Order;
			Replacement = Search;
		}
	}
	if (Replacement != -1)
		Talkboxes[Replacement].State = 1;
	Box.State = 0;
	TalkChanged = true;
}
// 0x505120, line 300; returns the free slot in EAX.
int patella::TTalk::FindFreeTalkbox()
{
	int i;
	for (;;) {
		for (i = 0; i < TALKBOXES; i++)
			if (Talkboxes[i].State == 0)
				return i;
		RemoveOldestTalkbox();
	}
}
// 0x4fecc0, line 344
void patella::TTalk::WrapTalkbox(TTalkbox *Talkbox, const char *Text)
{
	while (Talkbox->Lines < 10 && *Text != 0) {
		int Count = 0, Space = -1;
		while (Count < 40 && Text[Count] != 0 && Text[Count] != '\n') {
			if (IsSpace(Text[Count]))
				Space = Count;
			Count++;
		}
		char *Line = Talkbox->Text[Talkbox->Lines++];
		if (Count < 40) {
			memcpy(Line, Text, Count);
			Line[Count] = 0;
			Text += Count + (Text[Count] == '\n' ? 1 : 0);
		} else if (Space == -1) {
			strncpy(Line, Text, 38);
			Line[38] = '-';
			Line[39] = 0;
			Text += 38;
		} else {
			strncpy(Line, Text, Space);
			Line[Space] = 0;
			Text += Space + 1;
		}
	}
}
// 0x5052e0, lines 352-428
void patella::TTalk::AddTalkbox(int Mode, const char *Speaker, int x, int y, int z, const char *Text)
{
	PATELLA_ASSERT_VALUES(Mode==TALK_SAY || Mode==TALK_WHISPER || Mode==TALK_YELL || Mode==TALK_PRIVATE_MESSAGE || Mode==TALK_GAMEMASTER_BROADCAST || Mode==TALK_GAMEMASTER_MESSAGE || Mode==TALK_ANIMAL_LOW || Mode==TALK_ANIMAL_LOUD, Mode);
	PATELLA_ASSERT(Speaker!=nullptr);
	PATELLA_ASSERT(Text!=nullptr);
	if (Mode == 4 && !GetPrivateMessages())
		return;
	const int Position = FindFreeTalkbox();
	TTalkbox &Box = Talkboxes[Position];
	Box.Order = TalkOrder++;
	Box.Mode = Mode;
	Box.x = x;
	Box.y = y;
	Box.z = z;
	Box.Lines = 0;
	Box.RemainingTime = ((int)(strlen(Text) >> 1) + 30) * 100;
	// The first line names the speaker: sprintf with "%s says:", "%s whispers:", "%s yells:" or "%s:".
	const char *Verb = nullptr;
	switch (Mode) {
	case 1: Verb = " says:"; break;
	case 2: Verb = " whispers:"; break;
	case 3: Verb = " yells:"; break;
	case 4:
	case 9:
	case 11: Verb = ":"; break;
	default:
		break;
	}
	if (Verb != nullptr) {
		Box.Lines = 1;
		Box.Text[0].Format("{}{}", Speaker, Verb);
	}
	WrapTalkbox(&Box, Text);
	Box.State = 1;
	int Delay = 0;
	for (int Search = 0; Search < TALKBOXES; Search++) {
		TTalkbox &Other = Talkboxes[Search];
		if (Search == Position || Other.State == 0)
			continue;
		if ((Mode == 1 || Mode == 2 || Mode == 3) && Other.Mode == Mode
			&& strcmp(Other.Text[0], Box.Text[0]) == 0 && Other.x == x && Other.y == y && Other.z == z) {
			for (int Line = 1; Line < Box.Lines; Line++)
				if (Other.Lines < 10)
					Other.Text[Other.Lines++].Assign(Box.Text[Line]);
			Box.State = 2;
		}
		if ((Mode == 16 || Mode == 17) && (Other.Mode == 16 || Other.Mode == 17)
			&& Other.x == x && Other.y == y && Other.z == z) {
			for (int Line = 0; Line < Box.Lines; Line++)
				if (Other.Lines < 10)
					Other.Text[Other.Lines++].Assign(Box.Text[Line]);
			Box.State = 2;
		}
		bool Match = (Mode == 9 || Mode == 11)
			&& (Other.Mode == 19 || Other.Mode == 18 || Other.Mode == 9 || Other.Mode == 11);
		if (Mode == 4 && Other.Mode == 4)
			Match = true;
		if (Match) {
			if (Box.State == 1)
				Box.RemainingTime /= 2;
			Box.State = 2;
			if (Delay <= Other.RemainingTime)
				Delay = Other.RemainingTime;
		}
	}
	Box.RemainingTime += Delay;
	TalkChanged = true;
}
// 0x505ac0, lines 436-474
void patella::TTalk::AddMessageTalkbox(int Mode, const char *Text)
{
	PATELLA_ASSERT_VALUES(Mode==TALK_INFO_MESSAGE || Mode==TALK_EVENT_MESSAGE || Mode==TALK_ADMIN_MESSAGE, Mode);
	PATELLA_ASSERT(Text!=nullptr);
	if (Mode == 22)
		for (int Position = 0; Position < TALKBOXES; Position++)
			if (Talkboxes[Position].State != 0 && Talkboxes[Position].Mode == 22)
				RemoveTalkbox(Position);
	const int Position = FindFreeTalkbox();
	TTalkbox &Box = Talkboxes[Position];
	Box.Order = TalkOrder++;
	Box.Mode = Mode;
	Box.Lines = 0;
	Box.RemainingTime = ((int)(strlen(Text) >> 1) + 30) * (Mode == 19 || Mode == 18 ? 200 : 100);
	WrapTalkbox(&Box, Text);
	Box.State = 1;
	int Delay = 0;
	if (Mode != 22) {
		for (int Search = 0; Search < TALKBOXES; Search++) {
			const TTalkbox &Other = Talkboxes[Search];
			if (Search != Position && Other.State != 0
				&& (Other.Mode == 19 || Other.Mode == 18 || Other.Mode == 9 || Other.Mode == 11)) {
				if (Box.State == 1)
					Box.RemainingTime /= 2;
				Box.State = 2;
				if (Delay <= Other.RemainingTime)
					Delay = Other.RemainingTime;
			}
		}
	}
	Box.RemainingTime += Delay;
	TalkChanged = true;
}
// 0x4fef70, line 490
void patella::TTalk::SetTalkboxRectangle(int Number, int x, int y, int Width, int Height)
{
	TTalkbox *Box = GetTalkbox(Number);
	Box->Left = x;
	Box->Top = y;
	Box->Width = Width;
	Box->Height = Height;
}
// 0x4ff130, line 517. When GetMilliseconds throws before the loop, the original reports i
// uninitialised: MSVC's unoptimised build warns (C4701, a code generation warning, which a pragma
// turns off only outside the function); initialising i would change the oracle's code.
PATELLA_MSVC_WARNING(push)
PATELLA_MSVC_WARNING(disable : 4701)
void patella::TTalk::UpdateTalkboxes()
{
	int i;
	if (!LastTimeSet) { // the original's function-local static, initialised on the first call (issue #107)
		LastTime = GetMilliseconds();
		LastTimeSet = true;
	}
	const int DiffTime = GetMilliseconds() - LastTime;
	for (i = 0; i < TALKBOXES; i++)
		// NOLINTNEXTLINE(bugprone-assignment-in-if-condition): the original's countdown in the condition
		if (Talkboxes[i].State != 0 && (Talkboxes[i].RemainingTime -= DiffTime) < 1)
			RemoveTalkbox(i);
	LastTime = GetMilliseconds();
}
PATELLA_MSVC_WARNING(pop)
// 0x4ff330, line 536
void patella::TTalk::RemoveOldestTalkbox()
{
	int Oldest = TalkOrder, Position = -1;
	for (int Search = 0; Search < TALKBOXES; Search++)
		if (Talkboxes[Search].State != 0 && Talkboxes[Search].Order < Oldest) {
			Oldest = Talkboxes[Search].Order;
			Position = Search;
		}
	if (Position != -1)
		RemoveTalkbox(Position);
}
// 0x4ff500, lines 565-570
void patella::TTalk::SetStatusMessage(const char *Message, bool Permanent)
{
	TalkChanged = true;
	if (Message == nullptr) {
		StatusMessageTime = 0;
		return;
	}
	PATELLA_ASSERT_VALUES(strlen(Message)<STATUSMESSAGE_LENGTH, strlen(Message));
	StatusMessage.Assign(Message);
	StatusMessageTime = Permanent ? -1 : 50;
}
// 0x4ff7d0
void patella::TTalk::UpdateStatusMessage()
{
	if (StatusMessageTime > 0 && --StatusMessageTime == 0) // NOLINT(bugprone-inc-dec-in-conditions): the original's countdown
		TalkChanged = true;
}
// 0x4ff810, line 629
bool TTalkFilter::Matches(int _Mode, const char *_Addressee, int _Channel)
{
	if (_Mode == 11 || _Mode == 9 || _Mode == 10 || _Mode == 12)
		return false;
	if ((Mode != _Mode && Mode != 0) || (Channel != _Channel && Channel != -1))
		return false;
	if (strlen(Addressee) != 0 && _Addressee == nullptr)
		return false;
	if (strlen(Addressee) == 0)
		return true;
	PATELLA_ASSERT(_Addressee!=nullptr);
	return CompareStrings(Addressee, _Addressee, -1) == 0;
}
// 0x4ff9c0, line 652
int patella::TTalk::FindTalkFilter(int Mode, const char *Addressee, int Channel, TIndexedVector<TTalkFilter *> &List)
{
	for (int i = List.min; i <= List.max; i++)
		if (List(i) != nullptr && List(i)->Matches(Mode, Addressee, Channel))
			return i;
	return -1;
}
// 0x4ffb90, lines 661-672
int patella::TTalk::FindTextBuffer(TTalkBuffer *TextBuffer, TIndexedVector<TTalkFilter *> &List)
{
	PATELLA_ASSERT(TextBuffer!=nullptr);
	int i;
	for (i = List.min; i <= List.max; i++)
		if (List(i) != nullptr && List(i)->TextBuffer == TextBuffer)
			return i;
	return -1;
}
// 0x4ffe40, line 693
void patella::TTalk::AddTalkFilter(int Mode, const char *Addressee, int Channel, TTalkBuffer *TextBuffer,
	TIndexedVector<TTalkFilter *> &List, bool Permanent)
{
	int i = List.min;
	while (i <= List.max && List(i) != nullptr)
		i++;
	TTalkFilter *Filter = new (std::nothrow) TTalkFilter;
	if (Filter != nullptr) {
		Filter->TextBuffer = TextBuffer;
		Filter->Mode = Mode;
		if (Addressee == nullptr)
			Filter->Addressee[0] = 0;
		else
			CopyString(Filter->Addressee.Chars, Addressee);
		Filter->Channel = Channel;
		Filter->Permanent = Permanent;
	}
	List(i) = Filter;
}
// 0x500060, line 731. Buddy/temporary exceptions suppress the ignore list.
bool patella::TTalk::IsTalkIgnored(int Mode, const char *Name, int Channel)
{
	if (IgnoreEnabled) {
		if (BuddyIgnore)
			for (int i = TemporaryIgnoreList.min; i <= TemporaryIgnoreList.max; i++)
				if (TemporaryIgnoreList(i) != nullptr && TemporaryIgnoreList(i)->Matches(Mode, Name, Channel))
					return false;
		for (int i = BuddyExemptionList.min; i <= BuddyExemptionList.max; i++)
			if (BuddyExemptionList(i) != nullptr && BuddyExemptionList(i)->Matches(Mode, Name, Channel))
				return false;
	}
	if (IgnoreListEnabled)
		for (int i = IgnoreList.min; i <= IgnoreList.max; i++)
			if (IgnoreList(i) != nullptr && IgnoreList(i)->Matches(Mode, Name, Channel))
				return true;
	return false;
}

// 0x5002d0, line 766
char *patella::TTalk::GetBuddyIgnoreText()
{
	int Length = 0;
	for (int i = BuddyExemptionList.min; i <= BuddyExemptionList.max; i++)
		if (BuddyExemptionList(i) != nullptr)
			Length += (int)strlen(BuddyExemptionList(i)->Addressee) + 1;
	if (Length == 0)
		return nullptr;
	char *Text = new (std::nothrow) char[Length + 1];
	// MSVC's analysis (C28182, issue #111): the original's unchecked allocation (VC7.1's new returned null), kept.
	PATELLA_MSVC_WARNING(suppress : 28182)
	Text[0] = 0;
	// The original wrote each name and its line break at Text + Length, the text's length so far.
	TStringBuffer Buffer(Text, static_cast<std::size_t>(Length) + 1);
	for (int i = BuddyExemptionList.min; i <= BuddyExemptionList.max; i++)
		if (BuddyExemptionList(i) != nullptr && strlen(BuddyExemptionList(i)->Addressee) != 0)
			Buffer.AppendFormat("{}\n", BuddyExemptionList(i)->Addressee);
	return Text;
}

// 0x500520, line 782
void patella::TTalk::ClearBuddyIgnore()
{
	for (int i = BuddyExemptionList.min; i <= BuddyExemptionList.max; i++)
		if (BuddyExemptionList(i) != nullptr) {
			free(BuddyExemptionList(i));
			BuddyExemptionList(i) = nullptr;
		}
}

// 0x5006f0, line 802
void patella::TTalk::AddBuddyIgnore(const char *Addressee, bool Permanent)
{
	const int Filter = FindTalkFilter(0, Addressee, -1, BuddyExemptionList);
	if (Filter != -1) {
		free(BuddyExemptionList(Filter));
		BuddyExemptionList(Filter) = nullptr;
	}
	AddTalkFilter(0, Addressee, -1, nullptr, BuddyExemptionList, Permanent);
}
// 0x5008d0, line 816
bool patella::TTalk::IsPermanentBuddyIgnore(const char *Addressee)
{
	const int Filter = FindTalkFilter(0, Addressee, -1, BuddyExemptionList);
	return Filter != -1 ? BuddyExemptionList(Filter)->Permanent : false;
}

// 0x500a90, line 867
void patella::TTalk::ClearTemporaryIgnore()
{
	for (int i = TemporaryIgnoreList.min; i <= TemporaryIgnoreList.max; i++)
		if (TemporaryIgnoreList(i) != nullptr) {
			free(TemporaryIgnoreList(i));
			TemporaryIgnoreList(i) = nullptr;
		}
}

// 0x500c60, line 887.
void patella::TTalk::AddTemporaryIgnore(const char *Addressee)
{
	const int Filter = FindTalkFilter(0, Addressee, -1, TemporaryIgnoreList);
	if (Filter != -1) {
		free(TemporaryIgnoreList(Filter));
		TemporaryIgnoreList(Filter) = nullptr;
	}
	AddTalkFilter(0, Addressee, -1, nullptr, TemporaryIgnoreList, false);
}
// 0x500e40, line 903
void patella::TTalk::RemoveTemporaryIgnore(const char *Addressee)
{
	const int Filter = FindTalkFilter(0, Addressee, -1, TemporaryIgnoreList);
	if (Filter != -1) {
		free(TemporaryIgnoreList(Filter));
		TemporaryIgnoreList(Filter) = nullptr;
	}
}
// 0x501010, line 954
char *patella::TTalk::GetIgnoreListText()
{
	int Length = 0;
	for (int i = IgnoreList.min; i <= IgnoreList.max; i++)
		if (IgnoreList(i) != nullptr)
			Length += (int)strlen(IgnoreList(i)->Addressee) + 1;
	if (Length == 0)
		return nullptr;
	char *Text = new (std::nothrow) char[Length + 1];
	// MSVC's analysis (C28182, issue #111): the original's unchecked allocation (VC7.1's new returned null), kept.
	PATELLA_MSVC_WARNING(suppress : 28182)
	Text[0] = 0;
	// The original wrote each name and its line break at Text + Length, the text's length so far.
	TStringBuffer Buffer(Text, static_cast<std::size_t>(Length) + 1);
	for (int i = IgnoreList.min; i <= IgnoreList.max; i++)
		if (IgnoreList(i) != nullptr && strlen(IgnoreList(i)->Addressee) != 0)
			Buffer.AppendFormat("{}\n", IgnoreList(i)->Addressee);
	return Text;
}

// 0x501260, line 966
bool patella::TTalk::IsModeIgnored(int Mode)
{
	return FindTalkFilter(Mode, nullptr, -1, IgnoreList) != -1;
}

// 0x501400, line 982
void patella::TTalk::ClearIgnoreList()
{
	for (int i = IgnoreList.min; i <= IgnoreList.max; i++)
		if (IgnoreList(i) != nullptr) {
			free(IgnoreList(i));
			IgnoreList(i) = nullptr;
		}
}

// 0x5015d0, line 1002
void patella::TTalk::AddIgnoreFilter(int Mode, const char *Addressee, bool Permanent)
{
	const int Filter = FindTalkFilter(Mode, Addressee, -1, IgnoreList);
	if (Filter != -1) {
		free(IgnoreList(Filter));
		IgnoreList(Filter) = nullptr;
	}
	AddTalkFilter(Mode, Addressee, -1, nullptr, IgnoreList, Permanent);
}
// 0x5017b0, line 1012
void patella::TTalk::IgnoreMode(int Mode)
{
	AddIgnoreFilter(Mode, nullptr, false);
}
// 0x501940, line 1022
void patella::TTalk::IgnoreName(const char *Addressee, bool Permanent)
{
	AddIgnoreFilter(0, Addressee, Permanent);
}
// 0x501ae0, line 1036
bool patella::TTalk::IsPermanentIgnore(const char *Addressee)
{
	const int Filter = FindTalkFilter(0, Addressee, -1, IgnoreList);
	return Filter != -1 ? IgnoreList(Filter)->Permanent : false;
}
// 0x501ca0, line 1052
void patella::TTalk::RemoveIgnoreFilter(int Mode, const char *Addressee)
{
	const int Filter = FindTalkFilter(Mode, Addressee, -1, IgnoreList);
	if (Filter != -1) {
		free(IgnoreList(Filter));
		IgnoreList(Filter) = nullptr;
	}
}
// 0x501e70, line 1062
void patella::TTalk::UnignoreMode(int Mode)
{
	RemoveIgnoreFilter(Mode, nullptr);
}
// 0x502010, line 1072
void patella::TTalk::UnignoreName(const char *Addressee)
{
	RemoveIgnoreFilter(0, Addressee);
}
// 0x5076e0, lines 1144-1149
void patella::TTalk::ShowPermanentIgnoreWarning()
{
	int o, Counter = 0;
	for (o = IgnoreList.min; o <= IgnoreList.max; o++)
		if (IgnoreList(o) != nullptr && strlen(IgnoreList(o)->Addressee) != 0 && IgnoreList(o)->Permanent)
			Counter++;
	if (Counter > 0) {
		PATELLA_ASSERT(DefaultTextBuffer!=nullptr);
		AddTextBufferLine(DefaultTextBuffer, 0, TextPermanentlyIgnoring, 20, nullptr, 0);
		ConsoleChanged = true;
	}
}
// 0x505f10, lines 1179-1190
void patella::TTalk::AddTextBufferLine(TTalkBuffer *TargetBuffer, uint32 StatementID, const char *Text,
	int TalkMode, const char *Name, int Time)
{
	PATELLA_ASSERT(TargetBuffer!=nullptr);
	TTalkLine *Line;
	if (Name == nullptr)
		Line = new (std::nothrow) TTalkLine(StatementID, TalkMode, Text, 0);
	else
		Line = new (std::nothrow) TTalkLine(StatementID, TalkMode, Name, Text, Time);
	TargetBuffer->Append(Line);
	ConsoleChanged = true;
}
// 0x5061f0, lines 1198-1257
void patella::TTalk::AddConsoleText(uint32 StatementID, const char *Text, int TalkMode, const char *Name, int Channel, int Time)
{
	PATELLA_ASSERT(Text!=nullptr);
	bool Broadcast;
	switch (TalkMode) {
	case 18: Broadcast = true; break;
	case 19:
	case 20: Broadcast = GetEventMessages(); break;
	case 21: Broadcast = GetStatusMessages(); break;
	case 22: Broadcast = GetInfoMessages(); break;
	case 23: return;
	default: {
		TTalkBuffer *TargetBuffer = DefaultTextBuffer;
		if (TalkMode == 5 || TalkMode == 6 || TalkMode == 7 || TalkMode == 8
			|| TalkMode == 10 || TalkMode == 14 || TalkMode == 12)
			TargetBuffer = nullptr;
		if (TalkMode == 14)
			Name = nullptr;
		for (int i = ConsoleFilterList.min; i <= ConsoleFilterList.max; i++) {
			TTalkFilter *Filter = ConsoleFilterList(i);
			const int Mode = TalkMode == 10 || TalkMode == 14 || TalkMode == 12 ? 5 : TalkMode;
			if (Filter != nullptr && Filter->Matches(Mode, Name, Channel))
				TargetBuffer = Filter->TextBuffer;
		}
		if (TargetBuffer != nullptr)
			AddTextBufferLine(TargetBuffer, StatementID, Text, TalkMode, Name, Time);
		return;
	}
	}
	if (Broadcast) {
		TTalkBuffer *TargetBuffer = DefaultTextBuffer;
		PATELLA_ASSERT(TargetBuffer!=nullptr);
		AddTextBufferLine(TargetBuffer, StatementID, Text, TalkMode, Name, Time);
		for (int i = ConsoleFilterList.min; i <= ConsoleFilterList.max; i++) {
			const TTalkFilter *Filter = ConsoleFilterList(i);
			if (Filter != nullptr) {
				TargetBuffer = Filter->TextBuffer;
				PATELLA_ASSERT(TargetBuffer!=nullptr);
				if (TargetBuffer->ShowMessages && !TargetBuffer->IsRuleViolationChannel())
					AddTextBufferLine(TargetBuffer, StatementID, Text, TalkMode, Name, Time);
			}
		}
	}
}
// 0x506720, line 1279
void patella::TTalk::AddChannelNotice(const char *Addressee, const char *Text, int Mode, const char *Name, int Channel)
{
	TTalkBuffer *TargetBuffer = Mode == 5 ? nullptr : DefaultTextBuffer;
	for (int i = ConsoleFilterList.min; i <= ConsoleFilterList.max; i++) {
		TTalkFilter *Filter = ConsoleFilterList(i);
		if (Filter != nullptr && Filter->Matches(Mode, Name, Channel))
			TargetBuffer = Filter->TextBuffer;
	}
	if (TargetBuffer != nullptr)
		AddTextBufferLine(TargetBuffer, 0, Text, 24, Addressee, 0);
}
// 0x5021a0, line 1291
void patella::TTalk::ClearDefaultTextBuffer()
{
	if (DefaultTextBuffer != nullptr)
		DefaultTextBuffer->Clear();
	ConsoleChanged = true;
}
// 0x502340, lines 1299-1304
void patella::TTalk::ClearTextBuffer(TTalkBuffer *TextBuffer)
{
	PATELLA_ASSERT(TextBuffer!=nullptr);
	TextBuffer->Clear();
	ConsoleChanged = true;
}
// 0x5025c0, lines 1315-1318
TTalkBuffer *patella::TTalk::GetTextBuffer(int Mode, const char *Name, int Channel)
{
	const int Filter = FindTalkFilter(Mode, Name, Channel, ConsoleFilterList);
	if (Filter == -1)
		return nullptr;
	PATELLA_ASSERT_VALUES(ConsoleFilterList(Filter)!=nullptr, Filter);
	return ConsoleFilterList(Filter)->TextBuffer;
}
// 0x507bc0, lines 1361-1366
TTalkBuffer *patella::TTalk::CreateTextBuffer(int ReceiveTalkMode, int SendTalkMode, const char *Name, int Channel)
{
	if (FindTalkFilter(ReceiveTalkMode, Name, Channel, ConsoleFilterList) != -1)
		return nullptr;
	TTalkBuffer *TextBuffer;
	if (Name == nullptr && Channel == -1 && ReceiveTalkMode == SendTalkMode)
		TextBuffer = new (std::nothrow) TTalkBuffer(ReceiveTalkMode);
	else if (Name == nullptr && Channel != -1 && ReceiveTalkMode == SendTalkMode) {
		if (Channel < 8)
			TextBuffer = new (std::nothrow) TChannelTalkBuffer(ReceiveTalkMode, Channel);
		else
			TextBuffer = new (std::nothrow) TPrivateChannel(ReceiveTalkMode, Channel);
	} else if (Name != nullptr && Channel == -1 && ReceiveTalkMode == SendTalkMode)
		TextBuffer = new (std::nothrow) TPrivateTalkBuffer(ReceiveTalkMode, Name);
	else if (ReceiveTalkMode == 7 || ReceiveTalkMode == 8)
		TextBuffer = new (std::nothrow) TRuleViolationTalkBuffer(ReceiveTalkMode, SendTalkMode, Name);
	else {
		if (Name != nullptr || Channel == -1 || ReceiveTalkMode != 6)
			TERROR("can't create text buffer");
		if (FindTalkFilter(6, nullptr, -1, ConsoleFilterList) != -1)
			return nullptr;
		TextBuffer = new (std::nothrow) TRuleViolationChannel(6, Channel);
		Channel = -1;
	}
	AddTalkFilter(ReceiveTalkMode, Name, Channel, TextBuffer, ConsoleFilterList, false);
	return TextBuffer;
}
// 0x507fa0, lines 1377-1383
void patella::TTalk::DeleteTextBuffer(TTalkBuffer *TextBuffer)
{
	if (TextBuffer != DefaultTextBuffer) {
		const int Filter = FindTextBuffer(TextBuffer, ConsoleFilterList);
		PATELLA_ASSERT(Filter!=-1);
		free(ConsoleFilterList(Filter));
		ConsoleFilterList(Filter) = nullptr;
		if (TextBuffer != nullptr) {
			TextBuffer->TTalkBuffer::~TTalkBuffer(); // non-virtual, by its qualified name (issue #111: Clang's warning)
			free(TextBuffer);
		}
	}
}
// 0x502880, line 1411. The player owns a private channel (the selected one;
// ConsoleWindow.cpp's "#I"/"#X" commands work only then).
bool patella::TTalk::HasOwnPrivateChannel()
{
	int i;
	for (i = ConsoleFilterList.min; i <= ConsoleFilterList.max; i++) {
		const TTalkFilter *Filter = ConsoleFilterList(i);
		if (Filter != nullptr && Filter->TextBuffer != nullptr
			&& Filter->TextBuffer->GetSelected() && Filter->TextBuffer->IsPrivateChannel())
			return true;
	}
	return false;
}
// 0x506920, no line evidence; tail-calls Clear without its own EH frame.
// noexcept (issue #81; noexcept(false) until then): Clear removes the first
// line until there is none, and Remove of the first line takes its first two
// branches, never the search with its assertion; free does not throw. In a
// destructor the virtual calls are TTalkBuffer's own.
TTalkBuffer::~TTalkBuffer() { Clear(); }
// 0x502a60, line 1476
void TTalkBuffer::Clear()
{
	while (first != nullptr)
		Remove(first);
}
// 0x506930, lines 1496-1500
void TTalkBuffer::AddText(int Mode, const char *text, int DiffTime)
{
	if (Enabled) {
		PATELLA_ASSERT(text!=nullptr);
		Append(new (std::nothrow) TTalkLine(0, Mode, text, DiffTime));
	}
}
// 0x502c00, lines 1516-1536
// The original frees with free() what AddText allocates with new (VC7.1's new was malloc; notes/ownership.md): GCC's
// -Wmismatched-new-delete (mingw-w64, where it inlines AddText here) names it, allowed for this function (issue #111).
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic push")
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic ignored \"-Wmismatched-new-delete\"")
void TTalkBuffer::Append(TTalkLine *line)
{
	if (!Enabled) {
		if (line != nullptr) {
			if (line->Line != nullptr)
				free(line->Line);
			free(line);
		}
		return;
	}
	PATELLA_ASSERT(line!=nullptr);
	if (last == nullptr && first == nullptr) {
		last = first = line;
		line->Next = line->Previous = nullptr;
	} else {
		line->Next = nullptr;
		line->Previous = last;
		// MSVC's analysis (C6011, issue #111): first and last are null together (the list's ends), so last is not
		// null when first is not.
		PATELLA_MSVC_WARNING(suppress : 6011)
		last->Next = line;
		last = line;
	}
	if (++lines > limit)
		Remove(first);
	Changed = LinesChanged = true;
}
PATELLA_GCC_DIAGNOSTIC("GCC diagnostic pop")
// 0x502f00, lines 1569-1588; includes Ghidra's split block 0x502f70.
void TTalkBuffer::Remove(TTalkLine *line)
{
	if (line == first && line == last)
		first = last = nullptr;
	else if (line == first) {
		first = line->Next;
		first->Previous = nullptr;
	} else if (line == last) {
		last = line->Previous;
		last->Next = nullptr;
	} else {
		const TTalkLine *Search = first;
		for (;;) {
			PATELLA_ASSERT(Search!=nullptr);
			if (Search == line)
				break;
			Search = Search->Next;
		}
		line->Previous->Next = line->Next;
		line->Next->Previous = line->Previous;
	}
	if (cursor == line)
		cursor = line->Next;
	if (line != nullptr) {
		if (line->Line != nullptr)
			free(line->Line);
		free(line);
	}
	lines--;
	if (cursor == nullptr)
		cursor = first;
	Complete = false;
	Changed = LinesChanged = true;
}
// 0x503210, lines 1597-1608
void TTalkBuffer::Remove(int lineNumber)
{
	PATELLA_ASSERT(first!=nullptr);
	// Not const (issue #39): as a constant the compiler folds i away; the binary keeps it in a stack slot.
	int i = 0; // NOLINT(misc-const-correctness)
	TTalkLine *Search = first;
	while (i != lineNumber) {
		Search = Search->Next;
		PATELLA_ASSERT(Search!=nullptr);
		// The binary never increments i: nonzero indices eventually assert.
	}
	PATELLA_ASSERT(Search!=nullptr);
	Remove(Search);
}
// 0x503670, lines 1626-1632; includes Ghidra's split block 0x5036c0.
TTalkLine *TTalkBuffer::GetLine(int lineNumber)
{
	int i = 0;
	TTalkLine *Search = first;
	if (Search == nullptr) {
		cursor = nullptr;
		return nullptr;
	}
	while (i != lineNumber) {
		Search = Search->Next;
		PATELLA_ASSERT(Search!=nullptr);
		i++;
	}
	cursor = Search;
	return Search;
}
// 0x503920, line 1651. Returns the advanced cursor, not the previous line.
TTalkLine *TTalkBuffer::GetNext()
{
	PATELLA_ASSERT(cursor!=nullptr);
	cursor = cursor->Next;
	return cursor;
}
// 0x503a70, lines 1672-1682. bytes is the original catch name for F's slot
// (the FILE's address; only its catch wrapper named it). One line each, in the
// file's mode: SaveChannel's text file ends them in CR LF.
int TTalkBuffer::Write(TFile &F) const
{
	PATELLA_ASSERT(F!=nullptr);
	[[maybe_unused]] const std::intptr_t bytes = static_cast<std::intptr_t>(F.Address());
	for (const TTalkLine *Line = first; Line != nullptr; Line = Line->Next)
		F.Print("{}\n", Line->GetText());
	return 0;
}
// 0x506bf0, lines 1689-1693
TPrivateTalkBuffer::TPrivateTalkBuffer(int TalkMode, const char *_Addressee) : TTalkBuffer(TalkMode)
{
	PATELLA_ASSERT(_Addressee!=nullptr);
	CopyString(Addressee.Chars, _Addressee);
}
// 0x506ec0, line 1700
TChannelTalkBuffer::TChannelTalkBuffer(int TalkMode, int _ChannelID) : TTalkBuffer(TalkMode)
{
	PATELLA_ASSERT(_ChannelID!=-1);
	ChannelID = _ChannelID;
}
// 0x507070, line 1729
TRuleViolationTalkBuffer::TRuleViolationTalkBuffer(int _ReceiveTalkMode, int _SendTalkMode,
	const char *_Addressee) : TTalkBuffer(_ReceiveTalkMode)
{
	SendTalkMode = _SendTalkMode;
	Closed = false;
	if (_Addressee == nullptr)
		Addressee[0] = 0;
	else
		CopyString(Addressee.Chars, _Addressee);
	WriteEnabled = true;
}
// 0x507280, line 1740
void TRuleViolationTalkBuffer::AddText(int Mode, const char *text, int DiffTime)
{
	WriteEnabled = true;
	TTalkBuffer::AddText(Mode, text, DiffTime);
}
// 0x503d20, line 1751
void TRuleViolationTalkBuffer::Append(TTalkLine *line)
{
	WriteEnabled = true;
	TTalkBuffer::Append(line);
}
// 0x507420, line 1758
TRuleViolationChannel::TRuleViolationChannel(int TalkMode, int _ChannelID) : TTalkBuffer(TalkMode)
{
	SendTalkMode = 0;
	Closed = false;
	PATELLA_ASSERT(_ChannelID!=-1);
	ChannelID = _ChannelID;
}
// 0x503eb0, line 1773
void TRuleViolationChannel::Remove(TTalkLine *line)
{
	const bool OldChanged = Changed;
	TTalkBuffer::Remove(line);
	Changed = OldChanged;
	if (first == nullptr)
		Closed = true;
}
// 0x504050, line 1787
void TRuleViolationChannel::Remove(int lineNumber)
{
	const bool OldChanged = Changed;
	TTalkBuffer::Remove(lineNumber);
	Changed = OldChanged;
	if (first == nullptr)
		Closed = true;
}
// 0x5041f0, lines 1796-1809
void TRuleViolationChannel::Append(TTalkLine *line)
{
	PATELLA_ASSERT(line!=nullptr);
	cursor = first;
	TTalkLine *Search = first;
	while (Search != nullptr) {
		TTalkLine *Next = GetNext();
		if (CompareStrings(line->Addressee, Search->Addressee, -1) == 0)
			Remove(Search);
		Search = Next;
	}
	TTalkBuffer::Append(line);
}
namespace {

// Ours (issue #62): the local time of a talk line's 64-bit time, for its stamp: the C library's localtime
// (compat/crt71.h), which up to 2038 gives the fields _localtime32 gave. Where it gives none (before 1970 or after
// 3000, which only a time offset from the server reaches) the original read localtime's NULL and crashed; here the
// stamp is "00:00".
std::tm LineLocalTime(std::int64_t Time)
{
	std::tm Local{};
	if (!patella::compat::CRT71LocalTime(Time, Local))
		Local = std::tm{};
	return Local;
}

} // namespace

// 0x5044b0, lines 1821-1847
TTalkLine::TTalkLine(uint32 _StatementID, int _Mode, const char *_Line, int DiffTime)
{
	PATELLA_ASSERT_VALUES(_Mode>=0&&_Mode<TALK_MODE_BEYONDLAST, _Mode);
	PATELLA_ASSERT(_Line!=nullptr);
	Mode = _Mode;
	StatementID = _StatementID;
	const std::int64_t LineTime = patella::platform::Platform().Clock.WallSeconds() + DiffTime;
	Time = StoreTime32(LineTime);
	const std::tm LocalTime = LineLocalTime(LineTime);
	TFixedString<8> Stamp;
	Stamp.Format("{:02}:{:02} ", LocalTime.tm_hour, LocalTime.tm_min);
	Addressee[0] = 0;
	const std::size_t Size = strlen(Stamp) + strlen(_Line) + 1;
	Line = new (std::nothrow) char[Size];
	TStringBuffer(Line, Size).Assign(Stamp);
	TrimString(Line + strlen(Stamp), _Line);
	WithoutTime = Text = Line + strlen(Stamp);
}
// 0x5048a0, lines 1854-1889
TTalkLine::TTalkLine(uint32 _StatementID, int _Mode, const char *_Addressee, const char *_Line, int DiffTime)
{
	PATELLA_ASSERT_VALUES(_Mode>=0&&_Mode<TALK_MODE_BEYONDLAST, _Mode);
	PATELLA_ASSERT(_Line!=nullptr);
	PATELLA_ASSERT(_Addressee!=nullptr);
	Mode = _Mode;
	StatementID = _StatementID;
	const std::int64_t LineTime = patella::platform::Platform().Clock.WallSeconds() + DiffTime;
	Time = StoreTime32(LineTime);
	const std::tm LocalTime = LineLocalTime(LineTime);
	TFixedString<8> Stamp;
	Stamp.Format("{:02}:{:02} ", LocalTime.tm_hour, LocalTime.tm_min);
	CopyString(Addressee.Chars, _Addressee);
	const std::size_t Size = strlen(Stamp) + strlen(Addressee) + strlen(_Line) + 3;
	Line = new (std::nothrow) char[Size];
	TStringBuffer(Line, Size).Assign(Stamp);
	WithoutTime = Line + strlen(Stamp);
	TStringBuffer(WithoutTime, Size - strlen(Stamp)).Format("{}: ", Addressee);
	Text = WithoutTime + strlen(Addressee) + 2;
	TrimString(Text, _Line);
}
// 0x504d70, line 1922
void patella::TTalk::ResetTalk()
{
	for (int Position = 0; Position < TALKBOXES; Position++)
		Talkboxes[Position].State = 0;
	TalkChanged = true;
	TalkOrder = 0;
	SetStatusMessage(nullptr, false);
	ClearDefaultTextBuffer();
}
// 0x504f30, line 1937
void patella::TTalk::InitTalk()
{
	IgnoreEnabled = true;
	BuddyIgnore = false;
	IgnoreListEnabled = true;
	ResetTalk();
	DefaultTextBuffer = new (std::nothrow) TTalkBuffer(1);
}
// 0x5079c0, no line evidence; binary places it before ExitTalk.
TPrivateChannel::TPrivateChannel(int TalkMode, int _ChannelID) : TChannelTalkBuffer(TalkMode, _ChannelID)
{
	WriteEnabled = true;
}
// 0x507a00, line 1952
void patella::TTalk::ExitTalk()
{
	ClearBuddyIgnore();
	ClearTemporaryIgnore();
	ClearIgnoreList();
	if (DefaultTextBuffer != nullptr) {
		DefaultTextBuffer->TTalkBuffer::~TTalkBuffer(); // non-virtual, by its qualified name (issue #111)
		free(DefaultTextBuffer);
	}
	DefaultTextBuffer = nullptr;
}

// The functions of the original (issue #107): each calls its member on the one TClient's TTalk.

bool GetTalkChanged()
{
	return patella::bridge::Talk().GetTalkChanged();
}

int GetNumberOfTalkboxes()
{
	return patella::bridge::Talk().GetNumberOfTalkboxes();
}

const char *GetStatusMessage()
{
	return patella::bridge::Talk().GetStatusMessage();
}

void SetIgnoreEnabled(bool Value)
{
	patella::bridge::Talk().SetIgnoreEnabled(Value);
}

bool GetIgnoreEnabled()
{
	return patella::bridge::Talk().GetIgnoreEnabled();
}

void SetBuddyIgnore(bool Value)
{
	patella::bridge::Talk().SetBuddyIgnore(Value);
}

bool GetBuddyIgnore()
{
	return patella::bridge::Talk().GetBuddyIgnore();
}

void SetIgnoreListEnabled(bool Value)
{
	patella::bridge::Talk().SetIgnoreListEnabled(Value);
}

bool GetIgnoreListEnabled()
{
	return patella::bridge::Talk().GetIgnoreListEnabled();
}

bool IsTalkThrottled()
{
	return patella::bridge::Talk().IsTalkThrottled();
}

bool GetConsoleChanged()
{
	return patella::bridge::Talk().GetConsoleChanged();
}

TRGBColor *GetTalkColor(TRGBColor *Color, int TalkMode)
{
	return patella::bridge::Talk().GetTalkColor(Color, TalkMode);
}

TTalkbox *GetTalkbox(int Number)
{
	return patella::bridge::Talk().GetTalkbox(Number);
}

int GetTalkboxMode(int Number)
{
	return patella::bridge::Talk().GetTalkboxMode(Number);
}

void GetTalkboxPosition(int Number, int *x, int *y, int *z)
{
	patella::bridge::Talk().GetTalkboxPosition(Number, x, y, z);
}

void GetTalkboxRectangle(int Number, int *x, int *y, int *Width, int *Height)
{
	patella::bridge::Talk().GetTalkboxRectangle(Number, x, y, Width, Height);
}

TRGBColor *GetTalkboxColor(TRGBColor *Color, int Number)
{
	return patella::bridge::Talk().GetTalkboxColor(Color, Number);
}

int GetTalkboxLines(int Number)
{
	return patella::bridge::Talk().GetTalkboxLines(Number);
}

const char *GetTalkboxLine(int Number, int Line)
{
	return patella::bridge::Talk().GetTalkboxLine(Number, Line);
}

void RemoveTalkbox(int Position)
{
	patella::bridge::Talk().RemoveTalkbox(Position);
}

int FindFreeTalkbox()
{
	return patella::bridge::Talk().FindFreeTalkbox();
}

void WrapTalkbox(TTalkbox *Talkbox, const char *Text)
{
	patella::bridge::Talk().WrapTalkbox(Talkbox, Text);
}

void SetTalkboxRectangle(int Number, int x, int y, int Width, int Height)
{
	patella::bridge::Talk().SetTalkboxRectangle(Number, x, y, Width, Height);
}

void RemoveOldestTalkbox()
{
	patella::bridge::Talk().RemoveOldestTalkbox();
}

int FindTalkFilter(int Mode, const char *Addressee, int Channel, TIndexedVector<TTalkFilter *> &List)
{
	return patella::bridge::Talk().FindTalkFilter(Mode, Addressee, Channel, List);
}

int FindTextBuffer(TTalkBuffer *TextBuffer, TIndexedVector<TTalkFilter *> &List)
{
	return patella::bridge::Talk().FindTextBuffer(TextBuffer, List);
}

void AddTalkFilter(int Mode, const char *Addressee, int Channel, TTalkBuffer *TextBuffer,
	TIndexedVector<TTalkFilter *> &List, bool Permanent)
{
	patella::bridge::Talk().AddTalkFilter(Mode, Addressee, Channel, TextBuffer, List, Permanent);
}

char *GetBuddyIgnoreText()
{
	return patella::bridge::Talk().GetBuddyIgnoreText();
}

void ClearBuddyIgnore()
{
	patella::bridge::Talk().ClearBuddyIgnore();
}

void AddBuddyIgnore(const char *Addressee, bool Permanent)
{
	patella::bridge::Talk().AddBuddyIgnore(Addressee, Permanent);
}

bool IsPermanentBuddyIgnore(const char *Addressee)
{
	return patella::bridge::Talk().IsPermanentBuddyIgnore(Addressee);
}

void ClearTemporaryIgnore()
{
	patella::bridge::Talk().ClearTemporaryIgnore();
}

char *GetIgnoreListText()
{
	return patella::bridge::Talk().GetIgnoreListText();
}

bool IsModeIgnored(int Mode)
{
	return patella::bridge::Talk().IsModeIgnored(Mode);
}

void AddIgnoreFilter(int Mode, const char *Addressee, bool Permanent)
{
	patella::bridge::Talk().AddIgnoreFilter(Mode, Addressee, Permanent);
}

void IgnoreMode(int Mode)
{
	patella::bridge::Talk().IgnoreMode(Mode);
}

void IgnoreName(const char *Addressee, bool Permanent)
{
	patella::bridge::Talk().IgnoreName(Addressee, Permanent);
}

bool IsPermanentIgnore(const char *Addressee)
{
	return patella::bridge::Talk().IsPermanentIgnore(Addressee);
}

void RemoveIgnoreFilter(int Mode, const char *Addressee)
{
	patella::bridge::Talk().RemoveIgnoreFilter(Mode, Addressee);
}

void UnignoreMode(int Mode)
{
	patella::bridge::Talk().UnignoreMode(Mode);
}

void UnignoreName(const char *Addressee)
{
	patella::bridge::Talk().UnignoreName(Addressee);
}

void ClearDefaultTextBuffer()
{
	patella::bridge::Talk().ClearDefaultTextBuffer();
}

void ClearTextBuffer(TTalkBuffer *TextBuffer)
{
	patella::bridge::Talk().ClearTextBuffer(TextBuffer);
}

bool HasOwnPrivateChannel()
{
	return patella::bridge::Talk().HasOwnPrivateChannel();
}

void AddChannelNotice(const char *Addressee, const char *Text, int Mode, const char *Name, int Channel)
{
	patella::bridge::Talk().AddChannelNotice(Addressee, Text, Mode, Name, Channel);
}

void DeleteTextBuffer(TTalkBuffer *TextBuffer)
{
	patella::bridge::Talk().DeleteTextBuffer(TextBuffer);
}

TTalkBuffer *CreateTextBuffer(int ReceiveTalkMode, int SendTalkMode, const char *Name, int Channel)
{
	return patella::bridge::Talk().CreateTextBuffer(ReceiveTalkMode, SendTalkMode, Name, Channel);
}

void SetTalkChanged()
{
	patella::bridge::Talk().SetTalkChanged();
}

void SetTalkThrottle()
{
	patella::bridge::Talk().SetTalkThrottle();
}

void SetConsoleChanged()
{
	patella::bridge::Talk().SetConsoleChanged();
}

void AddTalkbox(int Mode, const char *Speaker, int x, int y, int z, const char *Text)
{
	patella::bridge::Talk().AddTalkbox(Mode, Speaker, x, y, z, Text);
}

void AddMessageTalkbox(int Mode, const char *Text)
{
	patella::bridge::Talk().AddMessageTalkbox(Mode, Text);
}

void UpdateTalkboxes()
{
	patella::bridge::Talk().UpdateTalkboxes();
}

void SetStatusMessage(const char *Message, bool Permanent)
{
	patella::bridge::Talk().SetStatusMessage(Message, Permanent);
}

void UpdateStatusMessage()
{
	patella::bridge::Talk().UpdateStatusMessage();
}

bool IsTalkIgnored(int Mode, const char *Name, int Channel)
{
	return patella::bridge::Talk().IsTalkIgnored(Mode, Name, Channel);
}

void AddTemporaryIgnore(const char *Addressee)
{
	patella::bridge::Talk().AddTemporaryIgnore(Addressee);
}

void RemoveTemporaryIgnore(const char *Addressee)
{
	patella::bridge::Talk().RemoveTemporaryIgnore(Addressee);
}

void ClearIgnoreList()
{
	patella::bridge::Talk().ClearIgnoreList();
}

void ShowPermanentIgnoreWarning()
{
	patella::bridge::Talk().ShowPermanentIgnoreWarning();
}

void AddTextBufferLine(TTalkBuffer *TargetBuffer, uint32 StatementID, const char *Text,
	int TalkMode, const char *Name, int Time)
{
	patella::bridge::Talk().AddTextBufferLine(TargetBuffer, StatementID, Text, TalkMode, Name, Time);
}

void AddConsoleText(uint32 StatementID, const char *Text, int TalkMode, const char *Name, int Channel, int Time)
{
	patella::bridge::Talk().AddConsoleText(StatementID, Text, TalkMode, Name, Channel, Time);
}

TTalkBuffer *GetTextBuffer(int Mode, const char *Name, int Channel)
{
	return patella::bridge::Talk().GetTextBuffer(Mode, Name, Channel);
}

void ResetTalk()
{
	patella::bridge::Talk().ResetTalk();
}

void InitTalk()
{
	patella::bridge::Talk().InitTalk();
}

void ExitTalk()
{
	patella::bridge::Talk().ExitTalk();
}
// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
