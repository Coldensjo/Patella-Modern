// Clipboard.h: IClipboard, the platform's clipboard (Patella-Modern issue #119, plan M7; notes/decisions.md, "#119:
// the platform interfaces, part 2"). Not part of the original source. Portable: the standard library only.
//
// sysinc.cpp's IsClipboardEmpty, GetClipboardText and SetClipboardText (the paste, CopyToClipboard,
// CheckClipboardText, CopyTaintedClipboardText) ask the process's IClipboard (platform/Platform.h). The text is the
// client's (Windows-1252, issue #112) on both sides; the implementation converts it to and from the platform's:
//
//	TWin32Clipboard   CF_UNICODETEXT (issue #112: '?' for a character Windows-1252 does not have; Win32Platform.h):
//	                  Patella.exe's, the golden runner's and PatellaTest.exe's without a script
//	TMemoryClipboard  a clipboard of the process's own (below): PatellaTest.exe's with a script, so that the
//	                  desktop's clipboard (another program's, a parallel test client's) does not reach a scripted
//	                  run and a scripted run does not change the desktop's; the unit tests'
//
// sysinc.cpp keeps its buffer and the original's odd CR removal; only the clipboard's own calls are here. M8: SDL3's
// clipboard (UTF-8, through types/Cp1252.h).

#pragma once

#include <optional>
#include <string>
#include <utility>

namespace patella::platform {

class IClipboard {
public:
	IClipboard() = default;
	IClipboard(const IClipboard &) = delete;
	IClipboard &operator=(const IClipboard &) = delete;
	IClipboard(IClipboard &&) = delete;
	IClipboard &operator=(IClipboard &&) = delete;

	// True when the clipboard has text (Win32: IsClipboardFormatAvailable(CF_UNICODETEXT)).
	[[nodiscard]] virtual bool HasText() = 0;
	// The clipboard's text up to its first NUL, as the client's text: nullopt where the clipboard cannot be opened,
	// empty where it has no text.
	[[nodiscard]] virtual std::optional<std::string> Text() = 0;
	// Puts Text (the client's) on the clipboard instead of what it had: false where that failed.
	virtual bool SetText(const char *Text) = 0;

protected:
	// Never destroyed through the interface (Clock.h's IClock).
	~IClipboard() = default;
};

// The process's own clipboard: what SetText put there, nothing before the first SetText. Never fails.
class TMemoryClipboard final : public IClipboard {
public:
	[[nodiscard]] bool HasText() override { return Content.has_value(); }
	[[nodiscard]] std::optional<std::string> Text() override { return Content.value_or(std::string()); }
	bool SetText(const char *Text) override
	{
		std::string Copy(Text);
		Content = std::move(Copy);
		return true;
	}

private:
	std::optional<std::string> Content;
};

} // namespace patella::platform
