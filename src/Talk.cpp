// Talk.cpp: reconstructed from Tibia.exe 7.72, in assertion/catch line order.
// Header COMDATs are in Talk.h; Container.h supplies the filter-list vector.
// Ghidra blocks 0x502f70 and 0x5036c0 belong to Remove and GetLine.
#include "Talk.h"
#include "Text.h"
#include "Utils.h"
#include "graphics.h"
#include "sysinc.h"
#include "Container.h"
#include "Preferences.h"
#include "common.h"
#include <cstddef>
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

static bool IgnoreListEnabled;                                       // 0x71dbd0
static bool BuddyIgnore;                                             // 0x71dbd1
static bool IgnoreEnabled;                                           // 0x71dbd2
static bool ConsoleChanged;                                          // 0x71dbd3
static int StatusMessageTime;                                        // 0x71dbdc
static char StatusMessage[STATUSMESSAGE_LENGTH];                     // 0x71dbe0
static int TalkOrder;                                                // 0x71dde0
static bool TalkChanged;                                             // 0x71dde4
static TTalkbox Talkboxes[TALKBOXES];                                // 0x71dde8
TTalkBuffer *DefaultTextBuffer;                                      // 0x71ef68
static int TalkThrottle;                                             // 0x71ef6c
static vector<TTalkFilter *> BuddyExemptionList(0, 9, 10, nullptr);  // 0x71ef70, 0x55a140
static vector<TTalkFilter *> TemporaryIgnoreList(0, 9, 10, nullptr); // 0x71ef90, 0x55a160
static vector<TTalkFilter *> IgnoreList(0, 9, 10, nullptr);          // 0x71efb0, 0x55a180
static vector<TTalkFilter *> ConsoleFilterList(0, 4, 5, nullptr);    // 0x71efd0, 0x55a1a0

static_assert(sizeof(TTalkbox) == 0x1c0, "talkbox layout");
#if PATELLA_ORIGINAL_LAYOUT
static_assert(sizeof(TTalkLine) == 0x40, "statement layout");
static_assert(sizeof(TTalkBuffer) == 0x2c, "text buffer layout");
static_assert(sizeof(TPrivateTalkBuffer) == 0x4c, "private buffer layout");
static_assert(sizeof(TChannelTalkBuffer) == 0x30, "channel buffer layout");
static_assert(sizeof(TRuleViolationTalkBuffer) == 0x4c, "rule violation buffer layout");
static_assert(sizeof(TRuleViolationChannel) == 0x30, "rule violation channel layout");
static_assert(sizeof(TPrivateChannel) == 0x34, "private channel layout");
static_assert(sizeof(TTalkFilter) == 0x30, "filter layout");
static_assert(offsetof(TTalkLine, WithoutTime) == 0x34, "statement text");
#endif

// 0x4fd440, no line evidence; leading Talk.cpp helpers remain in link order.
bool GetTalkChanged()
{
	const bool Result = TalkChanged;
	TalkChanged = false;
	return Result;
}
// 0x4fd450
int GetNumberOfTalkboxes()
{
	int Count = 0;
	for (int Position = 0; Position < TALKBOXES; Position++)
		if (Talkboxes[Position].State == 1)
			Count++;
	return Count;
}
// 0x4fd4c0
void SetTalkChanged() { TalkChanged = true; }
// 0x4fd4d0
const char *GetStatusMessage() { return StatusMessageTime != 0 ? StatusMessage : nullptr; }
// 0x4fd4e0
void SetIgnoreEnabled(bool Value) { IgnoreEnabled = Value; }
// 0x4fd4f0
bool GetIgnoreEnabled() { return IgnoreEnabled; }
// 0x4fd500
void SetBuddyIgnore(bool Value) { BuddyIgnore = Value; }
// 0x4fd510
bool GetBuddyIgnore() { return BuddyIgnore; }
// 0x4fd520
void SetIgnoreListEnabled(bool Value) { IgnoreListEnabled = Value; }
// 0x4fd530
bool GetIgnoreListEnabled() { return IgnoreListEnabled; }
// 0x4fd540
void SetTalkThrottle() { TalkThrottle = GetMilliseconds() + 1000; }
// 0x4fd550
bool IsTalkThrottled()
{
	if (TalkThrottle != 0) {
		if (GetMilliseconds() < TalkThrottle)
			return true;
		TalkThrottle = 0;
	}
	return false;
}
// 0x4fd580
bool GetConsoleChanged()
{
	const bool Result = ConsoleChanged;
	ConsoleChanged = false;
	return Result;
}
// 0x4fd590
void SetConsoleChanged() { ConsoleChanged = true; }

// 0x4fd9e0, lines 81-107
TRGBColor *GetTalkColor(TRGBColor *Color, int TalkMode)
{
	switch (TalkMode) {
		// clang-format off
	case 0: TERROR(81, "none is talking");
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
	default: TERROR1(107, "something unknown is talking", TalkMode);
		// clang-format on
	}
	return Color;
}
// 0x4fdd80, line 142
TTalkbox *GetTalkbox(int Number)
{
	TASSERT2(142, In(Number,1,GetNumberOfTalkboxes()), Number, GetNumberOfTalkboxes());
	int Position = 0;
	while (Talkboxes[Position].State != 1 || --Number != 0)
		Position++;
	return &Talkboxes[Position];
}
// 0x4fdf10, line 158
int GetTalkboxMode(int Number)
{
	TBEGIN
	return GetTalkbox(Number)->Mode;
	TEND1(158, Number)
}
// 0x4fe0b0, line 180
void GetTalkboxPosition(int Number, int *x, int *y, int *z)
{
	TBEGIN
	const TTalkbox *Box = GetTalkbox(Number);
	const int Mode = Box->Mode;
	if (Mode == 22 || Mode == 19 || Mode == 18 || Mode == 4 || Mode == 9 || Mode == 11)
		*x = *y = *z = 0;
	else {
		*x = Box->x;
		*y = Box->y;
		*z = Box->z;
	}
	TEND1(180, Number)
}
// 0x4fe2b0, line 196
void GetTalkboxRectangle(int Number, int *x, int *y, int *Width, int *Height)
{
	TBEGIN
	const TTalkbox *Box = GetTalkbox(Number);
	*x = Box->Left;
	*y = Box->Top;
	*Width = Box->Width;
	*Height = Box->Height;
	TEND1(196, Number)
}
// 0x4fe470, line 208
TRGBColor *GetTalkboxColor(TRGBColor *Color, int Number)
{
	TBEGIN
	GetTalkColor(Color, GetTalkbox(Number)->Mode);
	return Color;
	TEND1(208, Number)
}
// 0x4fe620, line 220
int GetTalkboxLines(int Number)
{
	TBEGIN
	return GetTalkbox(Number)->Lines;
	TEND1(220, Number)
}
// 0x4fe7c0, line 232
const char *GetTalkboxLine(int Number, int Line)
{
	TBEGIN
	return GetTalkbox(Number)->Text[Line];
	TEND2(232, Number, Line)
}
// 0x4fe960, line 246
void RemoveTalkbox(int Position)
{
	TASSERT1(246, In(Position,0,TALKBOXES-1), Position);
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
int FindFreeTalkbox()
{
	int i;
	TBEGIN
	for (;;) {
		for (i = 0; i < TALKBOXES; i++)
			if (Talkboxes[i].State == 0)
				return i;
		RemoveOldestTalkbox();
	}
	TEND1(300, i)
}
// 0x4fecc0, line 344
void WrapTalkbox(TTalkbox *Talkbox, const char *Text)
{
	TBEGIN
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
	TEND(344)
}
// 0x5052e0, lines 352-428
void AddTalkbox(int Mode, const char *Speaker, int x, int y, int z, const char *Text)
{
	TASSERT1(352, Mode==TALK_SAY || Mode==TALK_WHISPER || Mode==TALK_YELL || Mode==TALK_PRIVATE_MESSAGE || Mode==TALK_GAMEMASTER_BROADCAST || Mode==TALK_GAMEMASTER_MESSAGE || Mode==TALK_ANIMAL_LOW || Mode==TALK_ANIMAL_LOUD, Mode);
	TASSERT(353, Speaker!=NULL);
	TASSERT(354, Text!=NULL);
	TBEGIN
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
	const char *Format = nullptr;
	switch (Mode) {
	case 1: Format = "%s says:"; break;
	case 2: Format = "%s whispers:"; break;
	case 3: Format = "%s yells:"; break;
	case 4:
	case 9:
	case 11: Format = "%s:"; break;
	}
	if (Format != nullptr) {
		Box.Lines = 1;
		sprintf(Box.Text[0], Format, Speaker);
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
					strcpy(Other.Text[Other.Lines++], Box.Text[Line]);
			Box.State = 2;
		}
		if ((Mode == 16 || Mode == 17) && (Other.Mode == 16 || Other.Mode == 17)
			&& Other.x == x && Other.y == y && Other.z == z) {
			for (int Line = 0; Line < Box.Lines; Line++)
				if (Other.Lines < 10)
					strcpy(Other.Text[Other.Lines++], Box.Text[Line]);
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
	TEND1(428, Mode)
}
// 0x505ac0, lines 436-474
void AddMessageTalkbox(int Mode, const char *Text)
{
	TASSERT1(436, Mode==TALK_INFO_MESSAGE || Mode==TALK_EVENT_MESSAGE || Mode==TALK_ADMIN_MESSAGE, Mode);
	TASSERT(437, Text!=NULL);
	TBEGIN
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
	TEND1(474, Mode)
}
// 0x4fef70, line 490
void SetTalkboxRectangle(int Number, int x, int y, int Width, int Height)
{
	TBEGIN
	TTalkbox *Box = GetTalkbox(Number);
	Box->Left = x;
	Box->Top = y;
	Box->Width = Width;
	Box->Height = Height;
	TEND1(490, Number)
}
// 0x4ff130, line 517. When GetMilliseconds throws before the loop, the original reports i
// uninitialised: MSVC's unoptimised build warns (C4701, a code generation warning, which a pragma
// turns off only outside the function); initialising i would change the oracle's code.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4701)
#endif
void UpdateTalkboxes()
{
	int i;
	TBEGIN
	static int LastTime = GetMilliseconds(); // 0x71eff0, guard 0x71eff4
	const int DiffTime = GetMilliseconds() - LastTime;
	for (i = 0; i < TALKBOXES; i++)
		if (Talkboxes[i].State != 0 && (Talkboxes[i].RemainingTime -= DiffTime) < 1)
			RemoveTalkbox(i);
	LastTime = GetMilliseconds();
	TEND1(517, i)
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
// 0x4ff330, line 536
void RemoveOldestTalkbox()
{
	TBEGIN
	int Oldest = TalkOrder, Position = -1;
	for (int Search = 0; Search < TALKBOXES; Search++)
		if (Talkboxes[Search].State != 0 && Talkboxes[Search].Order < Oldest) {
			Oldest = Talkboxes[Search].Order;
			Position = Search;
		}
	if (Position != -1)
		RemoveTalkbox(Position);
	TEND(536)
}
// 0x4ff500, lines 565-570
void SetStatusMessage(const char *Message, bool Permanent)
{
	TBEGIN
	TalkChanged = true;
	if (Message == nullptr) {
		StatusMessageTime = 0;
		return;
	}
	TASSERT1(565, strlen(Message)<STATUSMESSAGE_LENGTH, strlen(Message));
	strcpy(StatusMessage, Message);
	StatusMessageTime = Permanent ? -1 : 50;
	TEND(570)
}
// 0x4ff7d0
void UpdateStatusMessage()
{
	if (StatusMessageTime > 0 && --StatusMessageTime == 0)
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
	TASSERT(629, _Addressee!=NULL);
	return CompareStrings(Addressee, _Addressee, -1) == 0;
}
// 0x4ff9c0, line 652
int FindTalkFilter(int Mode, const char *Addressee, int Channel, vector<TTalkFilter *> &List)
{
	TBEGIN
	for (int i = List.min; i <= List.max; i++)
		if (List(i) != nullptr && List(i)->Matches(Mode, Addressee, Channel))
			return i;
	return -1;
	TEND1(652, Channel)
}
// 0x4ffb90, lines 661-672
int FindTextBuffer(TTalkBuffer *TextBuffer, vector<TTalkFilter *> &List)
{
	TASSERT(661, TextBuffer!=NULL);
	int i;
	TBEGIN
	for (i = List.min; i <= List.max; i++)
		if (List(i) != nullptr && List(i)->TextBuffer == TextBuffer)
			return i;
	return -1;
	TEND1(672, i)
}
// 0x4ffe40, line 693
void AddTalkFilter(int Mode, const char *Addressee, int Channel, TTalkBuffer *TextBuffer,
	vector<TTalkFilter *> &List, bool Permanent)
{
	TBEGIN
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
			CopyString(Filter->Addressee, Addressee, 30);
		Filter->Channel = Channel;
		Filter->Permanent = Permanent;
	}
	List(i) = Filter;
	TEND2(693, Mode, Channel)
}
// 0x500060, line 731. Buddy/temporary exceptions suppress the ignore list.
bool IsTalkIgnored(int Mode, const char *Name, int Channel)
{
	TBEGIN
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
	TEND2(731, Mode, Channel)
}

// 0x5002d0, line 766
char *GetBuddyIgnoreText()
{
	TBEGIN
	int Length = 0;
	for (int i = BuddyExemptionList.min; i <= BuddyExemptionList.max; i++)
		if (BuddyExemptionList(i) != nullptr)
			Length += (int)strlen(BuddyExemptionList(i)->Addressee) + 1;
	if (Length == 0)
		return nullptr;
	char *Text = new (std::nothrow) char[Length + 1];
	Text[0] = 0;
	Length = 0;
	for (int i = BuddyExemptionList.min; i <= BuddyExemptionList.max; i++)
		if (BuddyExemptionList(i) != nullptr && strlen(BuddyExemptionList(i)->Addressee) != 0) {
			sprintf(Text + Length, "%s\n", BuddyExemptionList(i)->Addressee);
			Length += (int)strlen(BuddyExemptionList(i)->Addressee) + 1;
		}
	return Text;
	TEND(766)
}

// 0x500520, line 782
void ClearBuddyIgnore()
{
	TBEGIN
	for (int i = BuddyExemptionList.min; i <= BuddyExemptionList.max; i++)
		if (BuddyExemptionList(i) != nullptr) {
			free(BuddyExemptionList(i));
			BuddyExemptionList(i) = nullptr;
		}
	TEND(782)
}

// 0x5006f0, line 802
void AddBuddyIgnore(const char *Addressee, bool Permanent)
{
	TBEGIN
	const int Filter = FindTalkFilter(0, Addressee, -1, BuddyExemptionList);
	if (Filter != -1) {
		free(BuddyExemptionList(Filter));
		BuddyExemptionList(Filter) = nullptr;
	}
	AddTalkFilter(0, Addressee, -1, nullptr, BuddyExemptionList, Permanent);
	TEND(802)
}
// 0x5008d0, line 816
bool IsPermanentBuddyIgnore(const char *Addressee)
{
	TBEGIN
	const int Filter = FindTalkFilter(0, Addressee, -1, BuddyExemptionList);
	return Filter != -1 ? BuddyExemptionList(Filter)->Permanent : false;
	TEND(816)
}

// 0x500a90, line 867
void ClearTemporaryIgnore()
{
	TBEGIN
	for (int i = TemporaryIgnoreList.min; i <= TemporaryIgnoreList.max; i++)
		if (TemporaryIgnoreList(i) != nullptr) {
			free(TemporaryIgnoreList(i));
			TemporaryIgnoreList(i) = nullptr;
		}
	TEND(867)
}

// 0x500c60, line 887.
void AddTemporaryIgnore(const char *Addressee)
{
	TBEGIN
	const int Filter = FindTalkFilter(0, Addressee, -1, TemporaryIgnoreList);
	if (Filter != -1) {
		free(TemporaryIgnoreList(Filter));
		TemporaryIgnoreList(Filter) = nullptr;
	}
	AddTalkFilter(0, Addressee, -1, nullptr, TemporaryIgnoreList, false);
	TEND(887)
}
// 0x500e40, line 903
void RemoveTemporaryIgnore(const char *Addressee)
{
	TBEGIN
	const int Filter = FindTalkFilter(0, Addressee, -1, TemporaryIgnoreList);
	if (Filter != -1) {
		free(TemporaryIgnoreList(Filter));
		TemporaryIgnoreList(Filter) = nullptr;
	}
	TEND(903)
}
// 0x501010, line 954
char *GetIgnoreListText()
{
	TBEGIN
	int Length = 0;
	for (int i = IgnoreList.min; i <= IgnoreList.max; i++)
		if (IgnoreList(i) != nullptr)
			Length += (int)strlen(IgnoreList(i)->Addressee) + 1;
	if (Length == 0)
		return nullptr;
	char *Text = new (std::nothrow) char[Length + 1];
	Text[0] = 0;
	Length = 0;
	for (int i = IgnoreList.min; i <= IgnoreList.max; i++)
		if (IgnoreList(i) != nullptr && strlen(IgnoreList(i)->Addressee) != 0) {
			sprintf(Text + Length, "%s\n", IgnoreList(i)->Addressee);
			Length += (int)strlen(IgnoreList(i)->Addressee) + 1;
		}
	return Text;
	TEND(954)
}

// 0x501260, line 966
bool IsModeIgnored(int Mode)
{
	TBEGIN
	return FindTalkFilter(Mode, nullptr, -1, IgnoreList) != -1;
	TEND(966)
}

// 0x501400, line 982
void ClearIgnoreList()
{
	TBEGIN
	for (int i = IgnoreList.min; i <= IgnoreList.max; i++)
		if (IgnoreList(i) != nullptr) {
			free(IgnoreList(i));
			IgnoreList(i) = nullptr;
		}
	TEND(982)
}

// 0x5015d0, line 1002
void AddIgnoreFilter(int Mode, const char *Addressee, bool Permanent)
{
	TBEGIN
	const int Filter = FindTalkFilter(Mode, Addressee, -1, IgnoreList);
	if (Filter != -1) {
		free(IgnoreList(Filter));
		IgnoreList(Filter) = nullptr;
	}
	AddTalkFilter(Mode, Addressee, -1, nullptr, IgnoreList, Permanent);
	TEND(1002)
}
// 0x5017b0, line 1012
void IgnoreMode(int Mode)
{
	TBEGIN
	AddIgnoreFilter(Mode, nullptr, false);
	TEND(1012)
}
// 0x501940, line 1022
void IgnoreName(const char *Addressee, bool Permanent)
{
	TBEGIN
	AddIgnoreFilter(0, Addressee, Permanent);
	TEND(1022)
}
// 0x501ae0, line 1036
bool IsPermanentIgnore(const char *Addressee)
{
	TBEGIN
	const int Filter = FindTalkFilter(0, Addressee, -1, IgnoreList);
	return Filter != -1 ? IgnoreList(Filter)->Permanent : false;
	TEND(1036)
}
// 0x501ca0, line 1052
void RemoveIgnoreFilter(int Mode, const char *Addressee)
{
	TBEGIN
	const int Filter = FindTalkFilter(Mode, Addressee, -1, IgnoreList);
	if (Filter != -1) {
		free(IgnoreList(Filter));
		IgnoreList(Filter) = nullptr;
	}
	TEND1(1052, Mode)
}
// 0x501e70, line 1062
void UnignoreMode(int Mode)
{
	TBEGIN
	RemoveIgnoreFilter(Mode, nullptr);
	TEND1(1062, Mode)
}
// 0x502010, line 1072
void UnignoreName(const char *Addressee)
{
	TBEGIN
	RemoveIgnoreFilter(0, Addressee);
	TEND(1072)
}
// 0x5076e0, lines 1144-1149
void ShowPermanentIgnoreWarning()
{
	int o, Counter = 0;
	TBEGIN
	for (o = IgnoreList.min; o <= IgnoreList.max; o++)
		if (IgnoreList(o) != nullptr && strlen(IgnoreList(o)->Addressee) != 0 && IgnoreList(o)->Permanent)
			Counter++;
	if (Counter > 0) {
		TASSERT(1144, DefaultTextBuffer!=NULL);
		AddTextBufferLine(DefaultTextBuffer, 0, TextPermanentlyIgnoring, 20, nullptr, 0);
		ConsoleChanged = true;
	}
	TEND2(1149, o, Counter)
}
// 0x505f10, lines 1179-1190
void AddTextBufferLine(TTalkBuffer *TargetBuffer, uint32 StatementID, const char *Text,
	int TalkMode, const char *Name, int Time)
{
	TASSERT(1179, TargetBuffer!=NULL);
	TBEGIN
	TTalkLine *Line;
	if (Name == nullptr)
		Line = new (std::nothrow) TTalkLine(StatementID, TalkMode, Text, 0);
	else
		Line = new (std::nothrow) TTalkLine(StatementID, TalkMode, Name, Text, Time);
	TargetBuffer->Append(Line);
	ConsoleChanged = true;
	TEND2(1190, StatementID, TalkMode)
}
// 0x5061f0, lines 1198-1257
void AddConsoleText(uint32 StatementID, const char *Text, int TalkMode, const char *Name, int Channel, int Time)
{
	TASSERT(1198, Text!=NULL);
	TBEGIN
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
		TASSERT(1245, TargetBuffer!=NULL);
		AddTextBufferLine(TargetBuffer, StatementID, Text, TalkMode, Name, Time);
		for (int i = ConsoleFilterList.min; i <= ConsoleFilterList.max; i++) {
			const TTalkFilter *Filter = ConsoleFilterList(i);
			if (Filter != nullptr) {
				TargetBuffer = Filter->TextBuffer;
				TASSERT(1252, TargetBuffer!=NULL);
				if (TargetBuffer->ShowMessages && !TargetBuffer->IsRuleViolationChannel())
					AddTextBufferLine(TargetBuffer, StatementID, Text, TalkMode, Name, Time);
			}
		}
	}
	TEND1(1257, StatementID)
}
// 0x506720, line 1279
void AddChannelNotice(const char *Addressee, const char *Text, int Mode, const char *Name, int Channel)
{
	TBEGIN
	TTalkBuffer *TargetBuffer = Mode == 5 ? nullptr : DefaultTextBuffer;
	for (int i = ConsoleFilterList.min; i <= ConsoleFilterList.max; i++) {
		TTalkFilter *Filter = ConsoleFilterList(i);
		if (Filter != nullptr && Filter->Matches(Mode, Name, Channel))
			TargetBuffer = Filter->TextBuffer;
	}
	if (TargetBuffer != nullptr)
		AddTextBufferLine(TargetBuffer, 0, Text, 24, Addressee, 0);
	TEND1(1279, Channel)
}
// 0x5021a0, line 1291
void ClearDefaultTextBuffer()
{
	TBEGIN
	if (DefaultTextBuffer != nullptr)
		DefaultTextBuffer->Clear();
	ConsoleChanged = true;
	TEND(1291)
}
// 0x502340, lines 1299-1304
void ClearTextBuffer(TTalkBuffer *TextBuffer)
{
	TASSERT(1299, TextBuffer!=NULL);
	TBEGIN
	TextBuffer->Clear();
	ConsoleChanged = true;
	TEND(1304)
}
// 0x5025c0, lines 1315-1318
TTalkBuffer *GetTextBuffer(int Mode, const char *Name, int Channel)
{
	TBEGIN
	const int Filter = FindTalkFilter(Mode, Name, Channel, ConsoleFilterList);
	if (Filter == -1)
		return nullptr;
	TASSERT1(1315, ConsoleFilterList(Filter)!=NULL, Filter);
	return ConsoleFilterList(Filter)->TextBuffer;
	TEND2(1318, Mode, Channel)
}
// 0x507bc0, lines 1361-1366
TTalkBuffer *CreateTextBuffer(int ReceiveTalkMode, int SendTalkMode, const char *Name, int Channel)
{
	TBEGIN
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
			TERROR(1361, "can't create text buffer");
		if (FindTalkFilter(6, nullptr, -1, ConsoleFilterList) != -1)
			return nullptr;
		TextBuffer = new (std::nothrow) TRuleViolationChannel(6, Channel);
		Channel = -1;
	}
	AddTalkFilter(ReceiveTalkMode, Name, Channel, TextBuffer, ConsoleFilterList, false);
	return TextBuffer;
	TEND2(1366, ReceiveTalkMode, Channel)
}
// 0x507fa0, lines 1377-1383
void DeleteTextBuffer(TTalkBuffer *TextBuffer)
{
	TBEGIN
	if (TextBuffer != DefaultTextBuffer) {
		const int Filter = FindTextBuffer(TextBuffer, ConsoleFilterList);
		TASSERT(1377, Filter!=-1);
		free(ConsoleFilterList(Filter));
		ConsoleFilterList(Filter) = nullptr;
		if (TextBuffer != nullptr) {
			TextBuffer->~TTalkBuffer();
			free(TextBuffer);
		}
	}
	TEND(1383)
}
// 0x502880, line 1411. The player owns a private channel (the selected one;
// ConsoleWindow.cpp's "#I"/"#X" commands work only then).
bool HasOwnPrivateChannel()
{
	int i;
	TBEGIN
	for (i = ConsoleFilterList.min; i <= ConsoleFilterList.max; i++) {
		TTalkFilter *Filter = ConsoleFilterList(i);
		if (Filter != nullptr && Filter->TextBuffer != nullptr
			&& Filter->TextBuffer->GetSelected() && Filter->TextBuffer->IsPrivateChannel())
			return true;
	}
	return false;
	TEND1(1411, i)
}
// 0x506920, no line evidence; tail-calls Clear without its own EH frame.
TTalkBuffer::~TTalkBuffer() noexcept(false) { Clear(); }
// 0x502a60, line 1476
void TTalkBuffer::Clear()
{
	TBEGIN
	while (first != nullptr)
		Remove(first);
	TEND(1476)
}
// 0x506930, lines 1496-1500
void TTalkBuffer::AddText(int Mode, const char *text, int DiffTime)
{
	TBEGIN
	if (Enabled) {
		TASSERT(1496, text!=NULL);
		Append(new (std::nothrow) TTalkLine(0, Mode, text, DiffTime));
	}
	TEND2(1500, Mode, DiffTime)
}
// 0x502c00, lines 1516-1536
void TTalkBuffer::Append(TTalkLine *line)
{
	TBEGIN
	if (!Enabled) {
		if (line != nullptr) {
			if (line->Line != nullptr)
				free(line->Line);
			free(line);
		}
		return;
	}
	TASSERT(1516, line!=NULL);
	if (last == nullptr && first == nullptr) {
		last = first = line;
		line->Next = line->Previous = nullptr;
	} else {
		line->Next = nullptr;
		line->Previous = last;
		last->Next = line;
		last = line;
	}
	if (++lines > limit)
		Remove(first);
	Changed = LinesChanged = true;
	TEND(1536)
}
// 0x502f00, lines 1569-1588; includes Ghidra's split block 0x502f70.
void TTalkBuffer::Remove(TTalkLine *line)
{
	TBEGIN
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
			TASSERT(1569, Search!=NULL);
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
	TEND(1588)
}
// 0x503210, lines 1597-1608
void TTalkBuffer::Remove(int lineNumber)
{
	TASSERT(1597, first!=NULL);
	// Not const (issue #39): as a constant the compiler folds i away; the binary keeps it in a stack slot.
	int i = 0; // NOLINT(misc-const-correctness)
	TBEGIN
	TTalkLine *Search = first;
	while (i != lineNumber) {
		Search = Search->Next;
		TASSERT(1603, Search!=NULL);
		// The binary never increments i: nonzero indices eventually assert.
	}
	TASSERT(1605, Search!=NULL);
	Remove(Search);
	TEND2(1608, lineNumber, i)
}
// 0x503670, lines 1626-1632; includes Ghidra's split block 0x5036c0.
TTalkLine *TTalkBuffer::GetLine(int lineNumber)
{
	int i = 0;
	TBEGIN
	TTalkLine *Search = first;
	if (Search == nullptr) {
		cursor = nullptr;
		return nullptr;
	}
	while (i != lineNumber) {
		Search = Search->Next;
		TASSERT(1626, Search!=NULL);
		i++;
	}
	cursor = Search;
	return Search;
	TEND2(1632, lineNumber, i)
}
// 0x503920, line 1651. Returns the advanced cursor, not the previous line.
TTalkLine *TTalkBuffer::GetNext()
{
	TASSERT(1651, cursor!=NULL);
	cursor = cursor->Next;
	return cursor;
}
// 0x503a70, lines 1672-1682. bytes is the original catch name for F's slot.
int TTalkBuffer::Write(FILE *F) const
{
	TASSERT(1672, F!=NULL);
	const INT_PTR bytes = reinterpret_cast<INT_PTR>(F);
	TBEGIN
	for (const TTalkLine *Line = first; Line != nullptr; Line = Line->Next)
		fprintf(F, "%s\n", Line->GetText());
	return 0;
	TEND1(1682, bytes)
}
// 0x506bf0, lines 1689-1693
TPrivateTalkBuffer::TPrivateTalkBuffer(int TalkMode, const char *_Addressee) : TTalkBuffer(TalkMode)
{
	TASSERT(1689, _Addressee!=NULL);
	TBEGIN
	CopyString(Addressee, _Addressee, 30);
	TEND1(1693, TalkMode)
}
// 0x506ec0, line 1700
TChannelTalkBuffer::TChannelTalkBuffer(int TalkMode, int _ChannelID) : TTalkBuffer(TalkMode)
{
	TASSERT(1700, _ChannelID!=-1);
	ChannelID = _ChannelID;
}
// 0x507070, line 1729
TRuleViolationTalkBuffer::TRuleViolationTalkBuffer(int ReceiveTalkMode, int SendTalkMode,
	const char *_Addressee) : TTalkBuffer(ReceiveTalkMode)
{
	this->SendTalkMode = SendTalkMode;
	Closed = false;
	TBEGIN
	if (_Addressee == nullptr)
		Addressee[0] = 0;
	else
		CopyString(Addressee, _Addressee, 30);
	WriteEnabled = true;
	TEND2(1729, ReceiveTalkMode, SendTalkMode)
}
// 0x507280, line 1740
void TRuleViolationTalkBuffer::AddText(int Mode, const char *text, int DiffTime)
{
	TBEGIN
	WriteEnabled = true;
	TTalkBuffer::AddText(Mode, text, DiffTime);
	TEND1(1740, Mode)
}
// 0x503d20, line 1751
void TRuleViolationTalkBuffer::Append(TTalkLine *line)
{
	TBEGIN
	WriteEnabled = true;
	TTalkBuffer::Append(line);
	TEND(1751)
}
// 0x507420, line 1758
TRuleViolationChannel::TRuleViolationChannel(int TalkMode, int _ChannelID) : TTalkBuffer(TalkMode)
{
	SendTalkMode = 0;
	Closed = false;
	TASSERT(1758, _ChannelID!=-1);
	ChannelID = _ChannelID;
}
// 0x503eb0, line 1773
void TRuleViolationChannel::Remove(TTalkLine *line)
{
	TBEGIN
	const bool OldChanged = Changed;
	TTalkBuffer::Remove(line);
	Changed = OldChanged;
	if (first == nullptr)
		Closed = true;
	TEND(1773)
}
// 0x504050, line 1787
void TRuleViolationChannel::Remove(int lineNumber)
{
	TBEGIN
	const bool OldChanged = Changed;
	TTalkBuffer::Remove(lineNumber);
	Changed = OldChanged;
	if (first == nullptr)
		Closed = true;
	TEND(1787)
}
// 0x5041f0, lines 1796-1809
void TRuleViolationChannel::Append(TTalkLine *line)
{
	TASSERT(1796, line!=NULL);
	TBEGIN
	cursor = first;
	TTalkLine *Search = first;
	while (Search != nullptr) {
		TTalkLine *Next = GetNext();
		if (CompareStrings(line->Addressee, Search->Addressee, -1) == 0)
			Remove(Search);
		Search = Next;
	}
	TTalkBuffer::Append(line);
	TEND(1809)
}
// 0x5044b0, lines 1821-1847
TTalkLine::TTalkLine(uint32 _StatementID, int _Mode, const char *_Line, int DiffTime)
{
	TASSERT1(1821, _Mode>=0&&_Mode<TALK_MODE_BEYONDLAST, _Mode);
	TASSERT(1822, _Line!=NULL);
	TBEGIN
	Mode = _Mode;
	StatementID = _StatementID;
	Time = PATELLA_TIME32(nullptr) + DiffTime;
	const tm *LocalTime = _localtime32(&Time);
	char Stamp[8];
	sprintf(Stamp, "%02d:%02d ", LocalTime->tm_hour, LocalTime->tm_min);
	Addressee[0] = 0;
	Line = new (std::nothrow) char[strlen(Stamp) + strlen(_Line) + 1];
	strcpy(Line, Stamp);
	TrimString(Line + strlen(Stamp), _Line);
	WithoutTime = Text = Line + strlen(Stamp);
	TEND2(1847, _StatementID, _Mode)
}
// 0x5048a0, lines 1854-1889
TTalkLine::TTalkLine(uint32 _StatementID, int _Mode, const char *_Addressee, const char *_Line, int DiffTime)
{
	TASSERT1(1854, _Mode>=0&&_Mode<TALK_MODE_BEYONDLAST, _Mode);
	TASSERT(1855, _Line!=NULL);
	TASSERT(1856, _Addressee!=NULL);
	TBEGIN
	Mode = _Mode;
	StatementID = _StatementID;
	Time = PATELLA_TIME32(nullptr) + DiffTime;
	const tm *LocalTime = _localtime32(&Time);
	char Stamp[8];
	sprintf(Stamp, "%02d:%02d ", LocalTime->tm_hour, LocalTime->tm_min);
	CopyString(Addressee, _Addressee, 30);
	Line = new (std::nothrow) char[strlen(Stamp) + strlen(Addressee) + strlen(_Line) + 3];
	strcpy(Line, Stamp);
	WithoutTime = Line + strlen(Stamp);
	sprintf(WithoutTime, "%s: ", Addressee);
	Text = WithoutTime + strlen(Addressee) + 2;
	TrimString(Text, _Line);
	TEND2(1889, _StatementID, _Mode)
}
// 0x504d70, line 1922
void ResetTalk()
{
	TBEGIN
	for (int Position = 0; Position < TALKBOXES; Position++)
		Talkboxes[Position].State = 0;
	TalkChanged = true;
	TalkOrder = 0;
	SetStatusMessage(nullptr, false);
	ClearDefaultTextBuffer();
	TEND(1922)
}
// 0x504f30, line 1937
void InitTalk()
{
	TBEGIN
	IgnoreEnabled = true;
	BuddyIgnore = false;
	IgnoreListEnabled = true;
	ResetTalk();
	DefaultTextBuffer = new (std::nothrow) TTalkBuffer(1);
	TEND(1937)
}
// 0x5079c0, no line evidence; binary places it before ExitTalk.
TPrivateChannel::TPrivateChannel(int TalkMode, int _ChannelID) : TChannelTalkBuffer(TalkMode, _ChannelID)
{
	WriteEnabled = true;
}
// 0x507a00, line 1952
void ExitTalk()
{
	TBEGIN
	ClearBuddyIgnore();
	ClearTemporaryIgnore();
	ClearIgnoreList();
	if (DefaultTextBuffer != nullptr) {
		DefaultTextBuffer->~TTalkBuffer();
		free(DefaultTextBuffer);
	}
	DefaultTextBuffer = nullptr;
	TEND(1952)
}
