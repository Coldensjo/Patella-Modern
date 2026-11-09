// SdlPlatform.cpp: the SDL3 implementations of the platform's desktop and, since #159 in SdlClipboard.cpp, clipboard
// (Patella-Modern issue #124; platform/SdlPlatform.h). Not part of the original source: each function does with SDL3
// what Win32Platform.cpp's TWin32Window does with Win32's calls, for the SDL3 window's client (SdlFrame.cpp). The
// display's modes are SDL3's on every system since issue #152 (TSdlWindow, below).

#include "platform/SdlPlatform.h"

#include "log/Log.h"
#include "platform/DisplayModeList.h"
#include "platform/Platform.h"
#include "types/Cp1252.h"

#if defined(_WIN32)
#include "platform/Win32Platform.h"
#include <SDL3/SDL_properties.h>
#endif

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace patella::platform {

namespace {

bool VideoStarted()
{
	return SDL_WasInit(SDL_INIT_VIDEO) != 0;
}

// The SDL window of a native handle (the HWND on Windows: SdlFrame.cpp's window is known to the client by it, as in
// vulkan/WindowSystemSdl.cpp), else null.
SDL_Window *SdlWindowOf(TWindowHandle Handle)
{
	if (Handle == nullptr || !VideoStarted())
		return nullptr;
	SDL_Window *Found = nullptr;
	int Count = 0;
	SDL_Window **const Windows = SDL_GetWindows(&Count);
	for (int i = 0; Windows != nullptr && i < Count && Found == nullptr; i++) {
#if defined(_WIN32)
		if (SDL_GetPointerProperty(SDL_GetWindowProperties(Windows[i]), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr) == Handle)
			Found = Windows[i];
#else
		if (Windows[i] == Handle)
			Found = Windows[i];
#endif
	}
	SDL_free(static_cast<void *>(Windows)); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): SDL_GetWindows' array
	return Found;
}

} // namespace

// TWin32Window::ShowMessageBox's MessageBoxA(Owner, Text, Caption, MB_ICONHAND or MB_ICONEXCLAMATION): one OK button,
// modal, the thread's messages dispatched while it is open. Every box of the client has no owner (Player.cpp), so
// the box's parent is the SDL window of Owner where there is one, else none. The texts are the client's: in UTF-8 for SDL.
void TSdlWindow::ShowMessageBox(TWindowHandle Owner, const char *Text, const char *Caption, TMessageBoxIcon Icon)
{
	SDL_Window *const Parent = SdlWindowOf(Owner);
	const std::string Utf8Text = patella::text::Cp1252ToUtf8(Text != nullptr ? Text : "");
	const std::string Utf8Caption = patella::text::Cp1252ToUtf8(Caption != nullptr ? Caption : "");
	const SDL_MessageBoxFlags Flags = Icon == TMessageBoxIcon::Error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_WARNING;
	SDL_ShowSimpleMessageBox(Flags, Utf8Caption.c_str(), Utf8Text.c_str(), Parent);
}

#if !defined(_WIN32)
namespace {

// Issue #131: the start's count of the Tibia windows (Player.cpp CountTibiaWindows, "A Tibia client is already running!"):
// no call of SDL3 lists another process's windows, so another client of this user is told by a lock. Taken at the first
// count (the client's own, before its window shows) and held to the end of the process (exit releases it).
bool AnotherClientRuns()
{
	static const bool Result = [] {
		const char *const Runtime = std::getenv("XDG_RUNTIME_DIR");
		std::string Path = Runtime != nullptr && Runtime[0] == '/' ? std::string(Runtime) : "/tmp";
		Path += "/patella-client-" + std::to_string(::getuid()) + ".lock";
		const int File = ::open(Path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600); // NOLINT(cppcoreguidelines-pro-type-vararg): open's mode argument
		if (File < 0)
			return false;
		return ::flock(File, LOCK_EX | LOCK_NB) != 0; // the descriptor stays open: the lock lasts as long as the process
	}();
	return Result;
}

} // namespace

void TSdlWindow::VisitOwnWindows(TWindowVisitor Visit)
{
	int Count = 0;
	SDL_Window **const Windows = SDL_GetWindows(&Count);
	for (int i = 0; Windows != nullptr && i < Count; i++) {
		const char *const Title = SDL_GetWindowTitle(Windows[i]);
		Visit(Title, Title != nullptr ? static_cast<int>(std::strlen(Title)) : 0, "", 0);
	}
	SDL_free(static_cast<void *>(Windows)); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): SDL_GetWindows' array
}
#endif

void TSdlWindow::VisitWindows(TWindowVisitor Visit)
{
#if defined(_WIN32)
	Win32Window().VisitWindows(Visit);
#else
	VisitOwnWindows(Visit);
	if (AnotherClientRuns()) {
		// The other client counts as one more window of the client's title (CountTibiaWindows).
		VisitOwnWindows(Visit);
	}
#endif
}

namespace {

TDisplayMode ModeOf(const SDL_DisplayMode &Mode)
{
	TDisplayMode Result;
	Result.Width = Mode.w;
	Result.Height = Mode.h;
	Result.Depth = DisplayDepth(static_cast<int>(SDL_BITSPERPIXEL(Mode.format)), static_cast<int>(SDL_BYTESPERPIXEL(Mode.format)));
	return Result;
}

int Millihertz(float Refresh)
{
	return static_cast<int>(std::lround(static_cast<double>(Refresh) * 1000.0));
}

// The primary display's fullscreen modes, SDL's list (the original asked the primary display too). Null where SDL's
// video subsystem is not started or the list cannot be read; SDL_free it.
SDL_DisplayMode **FullscreenModes(int &Count)
{
	Count = 0;
	if (!VideoStarted())
		return nullptr;
	const SDL_DisplayID Display = SDL_GetPrimaryDisplay();
	if (Display == 0)
		return nullptr;
	return SDL_GetFullscreenDisplayModes(Display, &Count);
}

} // namespace

// The display's Index-th mode, SDL's list backwards (DisplayModeList.h).
bool SdlDisplayMode(unsigned int Index, TDisplayMode &Mode)
{
	int Count = 0;
	SDL_DisplayMode **const Modes = FullscreenModes(Count);
	if (Modes == nullptr)
		return false;
	std::size_t SdlIndex = 0;
	const bool Found = ListedModeIndex(static_cast<std::size_t>(Count), Index, SdlIndex);
	if (Found)
		Mode = ModeOf(*Modes[SdlIndex]);
	SDL_free(static_cast<void *>(Modes)); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): SDL_GetFullscreenDisplayModes' array
	return Found;
}

// The display's mode now.
bool SdlDesktopMode(TDisplayMode &Mode)
{
	if (!VideoStarted())
		return false;
	const SDL_DisplayMode *const Current = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
	if (Current == nullptr)
		return false;
	Mode = ModeOf(*Current);
	return true;
}

namespace {

// Back to the desktop's mode, with Window still fullscreen (null: the fullscreen desktop window). SDL decides whether
// the display needs changing from its own record of the current mode, and a display change that reaches SDL outside its
// own mode change (a monitor plugged in during the fullscreen mode) resets that record to the desktop's mode
// (SDL_windowsmodes.c WIN_AddDisplay: SDL_ResetFullscreenDisplayModes), after which SDL would leave the display in the
// fullscreen mode. Issue #152: with the window still in a mode of its own and SDL's record the desktop's, SDL is given
// the desktop's own entry of its list first, so that the change back really happens (SDL_SetDisplayModeForDisplay with
// that entry, then with none: ChangeDisplaySettingsExW with the desktop's mode, then with no mode, Windows' own return
// to the saved mode, the same as the original's ChangeDisplaySettingsA(nullptr, 0)).
bool ReturnToDesktopMode(SDL_Window *Window)
{
	const SDL_DisplayID Display = SDL_GetPrimaryDisplay();
	const SDL_DisplayMode *const Desktop = SDL_GetDesktopDisplayMode(Display);
	if (Desktop != nullptr && SDL_GetWindowFullscreenMode(Window) != nullptr && SDL_GetCurrentDisplayMode(Display) == Desktop) {
		int Count = 0;
		SDL_DisplayMode **const Modes = FullscreenModes(Count);
		for (int i = 0; Modes != nullptr && i < Count; i++) {
			if (Modes[i]->w == Desktop->w && Modes[i]->h == Desktop->h && Modes[i]->format == Desktop->format
				&& Modes[i]->refresh_rate == Desktop->refresh_rate) {
				const SDL_DisplayMode Entry = *Modes[i];
				log::Warn("SDL lost the fullscreen mode (a display change during it): setting the desktop's mode again");
				SDL_SetWindowFullscreenMode(Window, &Entry);
				SDL_SyncWindow(Window);
				break;
			}
		}
		SDL_free(static_cast<void *>(Modes)); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): SDL_GetFullscreenDisplayModes' array
	}
	const bool Done = SDL_SetWindowFullscreenMode(Window, nullptr);
	SDL_SyncWindow(Window);
	return Done;
}

} // namespace

// The display's mode for SDL's fullscreen window (SdlFrame.cpp SetDisplayMode made it a fullscreen desktop window):
// SDL_SetWindowFullscreenMode with the primary display's mode of that size and depth (DisplayModeList.h
// ChooseFullscreenMode), which makes it an exclusive fullscreen window and SDL switch the display; null back to the
// fullscreen desktop window and the desktop's mode. False where SDL has no window of that handle (with a mode), no such
// mode, or the display did not take it. SDL's refresh_rate is only compared here: on Windows the mode set is the entry's
// own Windows DEVMODE (notes/decisions.md, "#152").
bool SdlSetFullscreenMode(TWindowHandle Window, const TDisplayMode *Mode)
{
	SDL_Window *const Found = SdlWindowOf(Window);
	// A window SDL no longer has (destroyed: SDL gave the display its desktop mode back then) needs nothing undone.
	if (Found == nullptr)
		return Mode == nullptr;
	if (Mode == nullptr)
		return ReturnToDesktopMode(Found);
	int ModeCount = 0;
	SDL_DisplayMode **const Modes = FullscreenModes(ModeCount);
	if (Modes == nullptr)
		return false;
	std::vector<TListedMode> Listed;
	for (int i = 0; i < ModeCount; i++)
		Listed.push_back(TListedMode{ModeOf(*Modes[i]), Millihertz(Modes[i]->refresh_rate)});
	const SDL_DisplayMode *const Desktop = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	const int Chosen = ChooseFullscreenMode(Listed, *Mode, Desktop != nullptr ? Millihertz(Desktop->refresh_rate) : 0);
	bool Done = false;
	if (Chosen >= 0) {
		const SDL_DisplayMode Wanted = *Modes[Chosen];
		Done = SDL_SetWindowFullscreenMode(Found, &Wanted);
		SDL_SyncWindow(Found);
		// SDL switches the display only for a fullscreen window that is shown; it always is here (the client goes
		// fullscreen only from its shown window). Where the display did not take the mode, the change failed.
		const SDL_DisplayMode *const Current = SDL_GetCurrentDisplayMode(Wanted.displayID);
		if (Done && (SDL_GetWindowFlags(Found) & SDL_WINDOW_FULLSCREEN) != 0
			&& (Current == nullptr || Current->w != Wanted.w || Current->h != Wanted.h)) {
			SDL_SetWindowFullscreenMode(Found, nullptr);
			SDL_SyncWindow(Found);
			Done = false;
		}
	}
	SDL_free(static_cast<void *>(Modes)); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): SDL_GetFullscreenDisplayModes' array
	return Done;
}

// The display's modes are SDL3's on every system since issue #152 (notes/decisions.md, "#152").
bool TSdlWindow::DisplayMode(unsigned int Index, TDisplayMode &Mode)
{
	return SdlDisplayMode(Index, Mode);
}

bool TSdlWindow::DesktopMode(TDisplayMode &Mode)
{
	return SdlDesktopMode(Mode);
}

bool TSdlWindow::SetFullscreenMode(TWindowHandle Window, const TDisplayMode *Mode)
{
	return SdlSetFullscreenMode(Window, Mode);
}

namespace {
constinit TSdlClipboard SdlClipboardInstance;
constinit TSdlWindow SdlWindowInstance;
} // namespace

TSdlClipboard &SdlClipboard()
{
	return SdlClipboardInstance;
}

TSdlWindow &SdlWindow()
{
	return SdlWindowInstance;
}

} // namespace patella::platform
