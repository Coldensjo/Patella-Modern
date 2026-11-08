#pragma once
#include "common.h"
#include "types/File.h"
#include "types/FixedString.h"
#include "types/Time32.h"
#include "types/IndexedVector.h"
#include "Preferences.h"
#include <array>
#include <type_traits>
#include <cstring>

struct TRGBColor;

enum TALK_MODE : int { // NOLINT(cppcoreguidelines-use-enum-class): int constants, the packets' talk modes ("The other ten stay unscoped")
	TALK_SAY = 1,
	TALK_WHISPER = 2,
	TALK_YELL = 3,
	TALK_PRIVATE_MESSAGE = 4,
	TALK_CHANNEL_CALL = 5,
	TALK_GAMEMASTER_REQUEST = 6,
	TALK_GAMEMASTER_ANSWER = 7,
	TALK_PLAYER_ANSWER = 8,
	TALK_GAMEMASTER_BROADCAST = 9,
	TALK_GAMEMASTER_CHANNELCALL = 10,
	TALK_GAMEMASTER_MESSAGE = 11,
	TALK_ANONYMOUS_BROADCAST = 13,
	TALK_ANONYMOUS_CHANNELCALL = 14,
	TALK_ANONYMOUS_MESSAGE = 15,
	TALK_ANIMAL_LOW = 16,
	TALK_ANIMAL_LOUD = 17,
	TALK_ADMIN_MESSAGE = 18,
	TALK_EVENT_MESSAGE = 19,
	TALK_INFO_MESSAGE = 22,
	TALK_MODE_BEYONDLAST = 27
};
static_assert(sizeof(TALK_MODE) == 4);
static_assert(std::is_same_v<std::underlying_type_t<TALK_MODE>, int>);
enum : int { // NOLINT(cppcoreguidelines-use-enum-class): int constants ("The other ten stay unscoped")
	TALKBOXES = 10,
	STATUSMESSAGE_LENGTH = 512,
};
static_assert(sizeof(decltype(TALKBOXES)) == 4);
static_assert(std::is_same_v<std::underlying_type_t<decltype(TALKBOXES)>, int>);
struct TTalkbox {
	int State, Order, RemainingTime, Mode;
	int x, y, z, Lines;
	std::array<TFixedString<40>, 10> Text;
	int Left, Top, Width, Height;
};
struct TTalkLine {
	char *Line;
	uint32 StatementID;
	int Mode;
	TTalkLine *Previous, *Next;
	TFixedString<30> Addressee;
	char *WithoutTime, *Text;
	Time32 Time; // the line's time (StoreTime32, types/Time32.h: the original's 32 bits of it; issue #62)
	TTalkLine(uint32 _StatementID, int _Mode, const char *_Line, int DiffTime);
	TTalkLine(uint32 _StatementID, int _Mode, const char *_Addressee, const char *_Line, int DiffTime);
	TTalkLine(const TTalkLine &) = delete;
	TTalkLine &operator=(const TTalkLine &) = delete;
	// 0x4fd6c0, Talk.h inline.
	[[nodiscard]] const char *GetText() const { return GetTimeStamps() ? Line : WithoutTime; }
};
class TTalkBuffer;
class TTalkBuffer {
public:
	TTalkBuffer(const TTalkBuffer &) = delete;
	TTalkBuffer &operator=(const TTalkBuffer &) = delete;
	// 0x4fd5a0, Talk.h inline; Closed is deliberately not initialized.
	TTalkBuffer(int TalkMode) : ReceiveTalkMode(TalkMode), SendTalkMode(TalkMode),
								Changed(true), first(nullptr), last(nullptr), cursor(nullptr), lines(0), limit(1000),
								ShowMessages(false), Enabled(true), Complete(true), LinesChanged(true), Locked(false), Selected(false) {}
	~TTalkBuffer(); // noexcept (issue #81)
	// 0x4fd5f0-0x4fd6b7, Talk.h inlines, in vtable order.
	virtual bool GetChanged()
	{
		const bool Result = Changed;
		Changed = false;
		return Result;
	}
	virtual bool GetClosed()
	{
		// Issue #131: Closed is never set by the constructor (the original's; the golden corpus records the bytes it leaves
		// untouched), so it holds whatever the heap had: read as the byte the original tested (a load of a bool that holds
		// another value is undefined, UBSan's report on Linux).
		unsigned char Byte = 0;
		std::memcpy(&Byte, &Closed, sizeof(Byte));
		const bool Result = Byte != 0;
		Closed = false;
		return Result;
	}
	virtual void Close() { Closed = true; }
	virtual bool CanWrite() { return true; }
	virtual bool SetWriteEnabled([[maybe_unused]] bool Value) { return true; }
	virtual void Append(TTalkLine *line);
	virtual void AddText(int Mode, const char *text, int DiffTime);
	// The binary's vtable (0x573738) has Remove(int) at +0x1c and
	// Remove(TTalkLine *) at +0x20; MSVC puts overloaded virtuals in reverse
	// order of declaration, so Remove(TTalkLine *) is declared first (issue #36).
	virtual void Remove(TTalkLine *line);
	virtual void Remove(int lineNumber);
	virtual void Clear();
	// isDefault: ConsoleWindow.cpp's assertion "getTextBuffer()->isDefault()==false".
	virtual bool isDefault(); // TTalk's DefaultTextBuffer (below)
	virtual bool AcceptMessages() { return isDefault(); }
	virtual bool IsRuleViolation() { return false; }
	virtual bool IsRuleViolationChannel() { return false; }
	virtual bool IsPrivateChannel() { return false; }
	virtual bool GetSelected() { return Selected; }
	virtual void SetSelected(bool Value) { Selected = Value; }
	virtual void SetLocked(bool Value) { Locked = Value; }
	virtual bool GetLocked() { return Locked; }
	virtual int GetReceiveTalkMode() { return ReceiveTalkMode; }
	virtual int GetSendTalkMode() { return SendTalkMode; }
	virtual const char *GetAddressee() { return nullptr; }
	virtual int GetChannelID() { return -1; }
	TTalkLine *GetLine(int lineNumber);
	TTalkLine *GetNext();
	// Inline (ConsoleWindow.cpp): the cursor moves to the first or the last line.
	TTalkLine *GetFirst()
	{
		cursor = first;
		return cursor;
	}
	TTalkLine *GetLast()
	{
		cursor = last;
		return cursor;
	}
	int Write(TFile &F) const;
	int ReceiveTalkMode, SendTalkMode;
	bool Changed, Closed;
	TTalkLine *first, *last, *cursor;
	int lines, limit;
	bool ShowMessages, Enabled, Complete, LinesChanged, Locked, Selected;
};
class TPrivateTalkBuffer final : public TTalkBuffer {
public:
	TPrivateTalkBuffer(int TalkMode, const char *_Addressee);
	// 0x506eb0, Talk.h inline.
	const char *GetAddressee() override { return Addressee; }
	TFixedString<30> Addressee;
};
class TChannelTalkBuffer : public TTalkBuffer {
public:
	TChannelTalkBuffer(int TalkMode, int _ChannelID);
	// 0x507060, Talk.h inline.
	int GetChannelID() override { return ChannelID; }
	int ChannelID;
};
class TRuleViolationTalkBuffer final : public TTalkBuffer {
public:
	TRuleViolationTalkBuffer(int ReceiveTalkMode, int SendTalkMode, const char *_Addressee);
	// 0x507260 and 0x507270, Talk.h inline.
	bool SetWriteEnabled(bool Value) override
	{
		WriteEnabled = Value;
		return Value;
	}
	bool CanWrite() override { return WriteEnabled; }
	void Append(TTalkLine *line) override;
	void AddText(int Mode, const char *text, int DiffTime) override;
	bool IsRuleViolation() override { return true; }
	const char *GetAddressee() override { return Addressee; }
	TFixedString<30> Addressee;
	bool WriteEnabled;
};
class TRuleViolationChannel final : public TTalkBuffer {
public:
	TRuleViolationChannel(int TalkMode, int _ChannelID);
	int GetChannelID() override { return ChannelID; }
	int ChannelID;
	void Append(TTalkLine *line) override;
	// 0x5075c0, Talk.h line 321.
	void AddText([[maybe_unused]] int Mode, [[maybe_unused]] const char *text, [[maybe_unused]] int DiffTime) override
	{
		TERROR("not possible");
	}
	void Remove(int lineNumber) override;
	void Remove(TTalkLine *line) override;
	bool AcceptMessages() override { return true; }
	bool IsRuleViolationChannel() override { return true; }
};
class TPrivateChannel final : public TChannelTalkBuffer {
public:
	TPrivateChannel(int TalkMode, int _ChannelID);
	// Folded with 0x467420/0x467430 from another header instantiation.
	bool SetWriteEnabled(bool Value) override
	{
		WriteEnabled = Value;
		return Value;
	}
	bool CanWrite() override { return WriteEnabled; }
	// 0x5079f0, Talk.h inline.
	bool IsPrivateChannel() override { return GetChannelID() >= 8; }
	bool WriteEnabled;
};
struct TTalkFilter {
	TTalkBuffer *TextBuffer;
	TFixedString<30> Addressee;
	int Mode, Channel;
	bool Permanent;
	bool Matches(int _Mode, const char *_Addressee, int _Channel);
};
namespace patella {

// Talk.cpp's state (issue #107, M7; notes/decisions.md, "#104: TClient and the subsystem objects" and "#107"): the
// globals 0x71dbd0-0x71eff4 of the original are the members, in its .bss order with the same initial values, and the
// file's functions the members of the same names. TGUI owns it (GUI.h). It is TPreferences' TTalkLists: the White
// and Ignore lists that the options read and write.
class TTalk final : public TTalkLists {
public:
	TTalk() = default;
	TTalk(const TTalk &) = delete;
	TTalk &operator=(const TTalk &) = delete;
	TTalk(TTalk &&) = delete;
	TTalk &operator=(TTalk &&) = delete;
	~TTalk() = default;

	bool GetTalkChanged();
	[[nodiscard]] int GetNumberOfTalkboxes() const;
	[[nodiscard]] const char *GetStatusMessage() const;
	void SetIgnoreEnabled(bool Value);
	[[nodiscard]] bool GetIgnoreEnabled() const;
	void SetBuddyIgnore(bool Value);
	[[nodiscard]] bool GetBuddyIgnore() const;
	void SetIgnoreListEnabled(bool Value);
	[[nodiscard]] bool GetIgnoreListEnabled() const;
	bool IsTalkThrottled();
	bool GetConsoleChanged();
	TRGBColor *GetTalkColor(TRGBColor *Color, int TalkMode);
	TTalkbox *GetTalkbox(int Number);
	int GetTalkboxMode(int Number);
	void GetTalkboxPosition(int Number, int *x, int *y, int *z);
	void GetTalkboxRectangle(int Number, int *x, int *y, int *Width, int *Height);
	TRGBColor *GetTalkboxColor(TRGBColor *Color, int Number);
	int GetTalkboxLines(int Number);
	const char *GetTalkboxLine(int Number, int Line);
	void RemoveTalkbox(int Position);
	int FindFreeTalkbox();
	void WrapTalkbox(TTalkbox *Talkbox, const char *Text);
	void SetTalkboxRectangle(int Number, int x, int y, int Width, int Height);
	void RemoveOldestTalkbox();
	int FindTalkFilter(int Mode, const char *Addressee, int Channel, TIndexedVector<TTalkFilter *> &List);
	int FindTextBuffer(TTalkBuffer *TextBuffer, TIndexedVector<TTalkFilter *> &List);
	void AddTalkFilter(int Mode, const char *Addressee, int Channel, TTalkBuffer *TextBuffer,
		TIndexedVector<TTalkFilter *> &List, bool Permanent);
	char *GetBuddyIgnoreText() override;
	void ClearBuddyIgnore();
	void AddBuddyIgnore(const char *Addressee, bool Permanent) override;
	bool IsPermanentBuddyIgnore(const char *Addressee) override;
	void ClearTemporaryIgnore();
	char *GetIgnoreListText() override;
	bool IsModeIgnored(int Mode);
	void AddIgnoreFilter(int Mode, const char *Addressee, bool Permanent);
	void IgnoreMode(int Mode);
	void IgnoreName(const char *Addressee, bool Permanent) override;
	bool IsPermanentIgnore(const char *Addressee) override;
	void RemoveIgnoreFilter(int Mode, const char *Addressee);
	void UnignoreMode(int Mode);
	void UnignoreName(const char *Addressee);
	void ClearDefaultTextBuffer();
	void ClearTextBuffer(TTalkBuffer *TextBuffer);
	bool HasOwnPrivateChannel();
	void AddChannelNotice(const char *Addressee, const char *Text, int Mode, const char *Name, int Channel);
	void DeleteTextBuffer(TTalkBuffer *TextBuffer);
	TTalkBuffer *CreateTextBuffer(int ReceiveTalkMode, int SendTalkMode, const char *Name, int Channel);
	void SetTalkChanged();
	void SetTalkThrottle();
	void SetConsoleChanged();
	void AddTalkbox(int Mode, const char *Speaker, int x, int y, int z, const char *Text);
	void AddMessageTalkbox(int Mode, const char *Text);
	void UpdateTalkboxes();
	void SetStatusMessage(const char *Message, bool Permanent);
	void UpdateStatusMessage();
	bool IsTalkIgnored(int Mode, const char *Name, int Channel);
	void AddTemporaryIgnore(const char *Addressee);
	void RemoveTemporaryIgnore(const char *Addressee);
	void ClearIgnoreList() override;
	void ShowPermanentIgnoreWarning();
	void AddTextBufferLine(TTalkBuffer *TargetBuffer, uint32 StatementID, const char *Text,
		int TalkMode, const char *Name, int Time);
	void AddConsoleText(uint32 StatementID, const char *Text, int TalkMode, const char *Name, int Channel, int Time);
	TTalkBuffer *GetTextBuffer(int Mode, const char *Name, int Channel);
	void ResetTalk();
	void InitTalk();
	void ExitTalk();


private:
	// Data, in the original's .bss order.
	bool IgnoreListEnabled{};                           // 0x71dbd0
	bool BuddyIgnore{};                                 // 0x71dbd1
	bool IgnoreEnabled{};                               // 0x71dbd2
	bool ConsoleChanged{};                              // 0x71dbd3
	int StatusMessageTime{};                            // 0x71dbdc
	TFixedString<STATUSMESSAGE_LENGTH> StatusMessage{}; // 0x71dbe0
	int TalkOrder{};                                    // 0x71dde0
	bool TalkChanged{};                                 // 0x71dde4
	std::array<TTalkbox, TALKBOXES> Talkboxes{};        // 0x71dde8

public:
	// 0x71ef68; public: MainWindow.cpp, Map.cpp, TTalkBuffer::isDefault, the test mode's state dump and the golden
	// Talk, MainWindow and Map tests read it.
	TTalkBuffer *DefaultTextBuffer{};

private:
	int TalkThrottle{};                                                   // 0x71ef6c
	TIndexedVector<TTalkFilter *> BuddyExemptionList{0, 9, 10, nullptr};  // 0x71ef70, initialiser 0x55a140
	TIndexedVector<TTalkFilter *> TemporaryIgnoreList{0, 9, 10, nullptr}; // 0x71ef90, initialiser 0x55a160
	TIndexedVector<TTalkFilter *> IgnoreList{0, 9, 10, nullptr};          // 0x71efb0, initialiser 0x55a180
	TIndexedVector<TTalkFilter *> ConsoleFilterList{0, 4, 5, nullptr};    // 0x71efd0, initialiser 0x55a1a0
	int LastTime{};                                                       // 0x71eff0, UpdateTalkboxes' static
	bool LastTimeSet{};                                                   // its guard (0x71eff4)

	static_assert(sizeof(Talkboxes) == TALKBOXES * sizeof(TTalkbox), "Talkboxes");
};

// The transition's access path (issue #107; notes/decisions.md, "#104: TClient and the subsystem objects"): the one
// TClient's TTalk (Client.cpp). It goes when no free function needs it.
namespace bridge {
TTalk &Talk();
} // namespace bridge

} // namespace patella

// Talk.h inline in the original (it read the global): whether this is the console's default buffer.
inline bool TTalkBuffer::isDefault()
{
	return this == patella::bridge::Talk().DefaultTextBuffer;
}

// Talk.cpp's functions (issue #107): each calls TTalk's member of the same name on bridge::Talk().
bool GetTalkChanged();
int GetNumberOfTalkboxes();
const char *GetStatusMessage();
void SetIgnoreEnabled(bool Value);
bool GetIgnoreEnabled();
void SetBuddyIgnore(bool Value);
bool GetBuddyIgnore();
void SetIgnoreListEnabled(bool Value);
bool GetIgnoreListEnabled();
bool IsTalkThrottled();
bool GetConsoleChanged();
TRGBColor *GetTalkColor(TRGBColor *Color, int TalkMode);
TTalkbox *GetTalkbox(int Number);
int GetTalkboxMode(int Number);
void GetTalkboxPosition(int Number, int *x, int *y, int *z);
void GetTalkboxRectangle(int Number, int *x, int *y, int *Width, int *Height);
TRGBColor *GetTalkboxColor(TRGBColor *Color, int Number);
int GetTalkboxLines(int Number);
const char *GetTalkboxLine(int Number, int Line);
void RemoveTalkbox(int Position);
int FindFreeTalkbox();
void WrapTalkbox(TTalkbox *Talkbox, const char *Text);
void SetTalkboxRectangle(int Number, int x, int y, int Width, int Height);
void RemoveOldestTalkbox();
int FindTalkFilter(int Mode, const char *Addressee, int Channel, TIndexedVector<TTalkFilter *> &List);
int FindTextBuffer(TTalkBuffer *TextBuffer, TIndexedVector<TTalkFilter *> &List);
void AddTalkFilter(int Mode, const char *Addressee, int Channel, TTalkBuffer *TextBuffer,
	TIndexedVector<TTalkFilter *> &List, bool Permanent);
char *GetBuddyIgnoreText();
void ClearBuddyIgnore();
void AddBuddyIgnore(const char *Addressee, bool Permanent);
bool IsPermanentBuddyIgnore(const char *Addressee);
void ClearTemporaryIgnore();
char *GetIgnoreListText();
bool IsModeIgnored(int Mode);
void AddIgnoreFilter(int Mode, const char *Addressee, bool Permanent);
void IgnoreMode(int Mode);
void IgnoreName(const char *Addressee, bool Permanent);
bool IsPermanentIgnore(const char *Addressee);
void RemoveIgnoreFilter(int Mode, const char *Addressee);
void UnignoreMode(int Mode);
void UnignoreName(const char *Addressee);
void ClearDefaultTextBuffer();
void ClearTextBuffer(TTalkBuffer *TextBuffer);
bool HasOwnPrivateChannel();
void AddChannelNotice(const char *Addressee, const char *Text, int Mode, const char *Name, int Channel);
void DeleteTextBuffer(TTalkBuffer *TextBuffer);
TTalkBuffer *CreateTextBuffer(int ReceiveTalkMode, int SendTalkMode, const char *Name, int Channel);

void SetTalkChanged();
void SetTalkThrottle();
void SetConsoleChanged();
void AddTalkbox(int Mode, const char *Speaker, int x, int y, int z, const char *Text);
void AddMessageTalkbox(int Mode, const char *Text);
void UpdateTalkboxes();
void SetStatusMessage(const char *Message, bool Permanent);
void UpdateStatusMessage();
bool IsTalkIgnored(int Mode, const char *Name, int Channel);
void AddTemporaryIgnore(const char *Addressee);
void RemoveTemporaryIgnore(const char *Addressee);
void ClearIgnoreList();
void ShowPermanentIgnoreWarning();
void AddTextBufferLine(TTalkBuffer *TargetBuffer, uint32 StatementID, const char *Text,
	int TalkMode, const char *Name, int Time);
void AddConsoleText(uint32 StatementID, const char *Text, int TalkMode, const char *Name, int Channel, int Time);
TTalkBuffer *GetTextBuffer(int Mode, const char *Name, int Channel);
void ResetTalk();
void InitTalk();
void ExitTalk();
