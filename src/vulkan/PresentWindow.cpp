// PresentWindow.cpp: the windows the Vulkan engine presents in (PresentWindow.h; issue #52).

#include "vulkan/PresentWindow.h"

#if defined(_WIN32)
#include <commctrl.h>
#else
#include "compat/win32_types.h"
#include <SDL3/SDL_video.h>
#endif

namespace patella::vulkan {

#if defined(_WIN32)

namespace {

constexpr const char *ClassName = "PatellaVulkanPresent";
constexpr UINT_PTR SubclassId = 71;

// Mouse messages go to the window below (the main window, on the same thread); the window paints
// nothing (the device's swapchain shows its pictures), and never erases its background.
LRESULT CALLBACK PresentWindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	switch (Message) {
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		ValidateRect(Window, nullptr);
		return 0;
	default:
		return DefWindowProcA(Window, Message, WParam, LParam);
	}
}

bool RegisterPresentClass()
{
	static const bool Registered = [] {
		WNDCLASSA Class = {};
		Class.lpfnWndProc = PresentWindowProc;
		Class.hInstance = GetModuleHandleA(nullptr);
		Class.lpszClassName = ClassName;
		return RegisterClassA(&Class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}();
	return Registered;
}

HWND CreateChild(HWND Parent, bool Visible)
{
	if (Parent == nullptr || !RegisterPresentClass())
		return nullptr;
	RECT Client = {};
	GetClientRect(Parent, &Client);
	const DWORD Style = WS_CHILD | WS_CLIPSIBLINGS | (Visible ? WS_VISIBLE : 0);
	return CreateWindowExA(WS_EX_NOPARENTNOTIFY, ClassName, "", Style, 0, 0, Client.right, Client.bottom, Parent,
		nullptr, GetModuleHandleA(nullptr), nullptr);
}

void DestroyChild(HWND &Window)
{
	if (Window != nullptr && IsWindow(Window))
		DestroyWindow(Window);
	Window = nullptr;
}

// A shown device window's rectangle taken out of Context's clipping region (Parent's client coordinates); its
// own WS_VISIBLE, not IsWindowVisible, which a hidden main window (tests_vulkan's) would make false.
void ExcludeChild(HDC Context, HWND Parent, HWND Window)
{
	if (Window == nullptr || !IsWindow(Window) || (GetWindowLongPtrA(Window, GWL_STYLE) & WS_VISIBLE) == 0)
		return;
	RECT Rect = {};
	GetWindowRect(Window, &Rect);
	MapWindowPoints(nullptr, Parent, reinterpret_cast<POINT *>(&Rect), 2);
	ExcludeClipRect(Context, Rect.left, Rect.top, Rect.right, Rect.bottom);
}

} // namespace

// The main window's erase fills what the device windows do not cover; its end takes the subclass away.
LRESULT CALLBACK TPresentWindows::ParentProc(
	HWND Window, UINT Message, WPARAM WParam, LPARAM LParam, [[maybe_unused]] UINT_PTR Id, DWORD_PTR Self)
{
	auto *Windows = reinterpret_cast<TPresentWindows *>(Self);
	switch (Message) {
	case WM_ERASEBKGND:
		Windows->FillUncovered(reinterpret_cast<HDC>(WParam));
		return 1;
	case WM_NCDESTROY:
		Windows->Unsubclass();
		break;
	default:
		break;
	}
	return DefSubclassProc(Window, Message, WParam, LParam);
}

void TPresentWindows::Subclass(HWND Window)
{
	if (Window == SubclassedWindow)
		return;
	Unsubclass();
	if (Window != nullptr && SetWindowSubclass(Window, ParentProc, SubclassId, reinterpret_cast<DWORD_PTR>(this)))
		SubclassedWindow = Window;
}

void TPresentWindows::Unsubclass()
{
	if (SubclassedWindow != nullptr && IsWindow(SubclassedWindow))
		RemoveWindowSubclass(SubclassedWindow, ParentProc, SubclassId);
	SubclassedWindow = nullptr;
}

void TPresentWindows::FillUncovered(HDC Context) const
{
	if (SubclassedWindow == nullptr || Context == nullptr)
		return;
	RECT Client = {};
	GetClientRect(SubclassedWindow, &Client);
	const int Saved = SaveDC(Context);
	ExcludeChild(Context, SubclassedWindow, CurrentWindow);
	ExcludeChild(Context, SubclassedWindow, RetiredWindow);
	FillRect(Context, &Client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
	RestoreDC(Context, Saved);
}

TPresentWindows::~TPresentWindows()
{
	Close();
}

HWND TPresentWindows::Open(HWND Parent)
{
	Retire();
	Subclass(Parent);
	CurrentWindow = CreateChild(Parent, true);
	CurrentPresented = false;
	if (CurrentWindow != nullptr)
		SetWindowPos(CurrentWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	return CurrentWindow;
}

void TPresentWindows::Fit()
{
	if (CurrentWindow == nullptr)
		return;
	const HWND Parent = GetParent(CurrentWindow);
	RECT Wanted = {}, Now = {};
	GetClientRect(Parent, &Wanted);
	GetClientRect(CurrentWindow, &Now);
	if (Wanted.right != Now.right || Wanted.bottom != Now.bottom)
		SetWindowPos(CurrentWindow, nullptr, 0, 0, Wanted.right, Wanted.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
}

void TPresentWindows::Presented()
{
	if (CurrentWindow == nullptr)
		return;
	CurrentPresented = true;
	DestroyChild(RetiredWindow);
}

void TPresentWindows::Retire()
{
	if (CurrentWindow == nullptr)
		return;
	if (CurrentPresented) {
		DestroyChild(RetiredWindow);
		RetiredWindow = CurrentWindow;
	} else {
		DestroyChild(CurrentWindow);
	}
	CurrentWindow = nullptr;
	CurrentPresented = false;
}

void TPresentWindows::Close()
{
	DestroyChild(CurrentWindow);
	DestroyChild(RetiredWindow);
	CurrentPresented = false;
	Unsubclass();
}

TTestWindow::TTestWindow(HWND Parent) : Window(CreateChild(Parent, false))
{
}

TTestWindow::~TTestWindow()
{
	DestroyChild(Window);
}

#else // Linux (issue #131): the device's window is the main window

void TPresentWindows::Subclass(HWND Window)
{
	SubclassedWindow = Window;
}

void TPresentWindows::Unsubclass()
{
	SubclassedWindow = nullptr;
}

TPresentWindows::~TPresentWindows()
{
	Close();
}

HWND TPresentWindows::Open(HWND Parent)
{
	Retire();
	Subclass(Parent);
	CurrentWindow = Parent;
	CurrentPresented = false;
	return CurrentWindow;
}

void TPresentWindows::Fit()
{
}

void TPresentWindows::Presented()
{
	if (CurrentWindow != nullptr)
		CurrentPresented = true;
}

void TPresentWindows::Retire()
{
	CurrentWindow = nullptr;
	CurrentPresented = false;
}

void TPresentWindows::Close()
{
	Retire();
	Unsubclass();
}

void TPresentWindows::FillUncovered(HDC Context) const
{
	static_cast<void>(Context);
}

TTestWindow::TTestWindow(HWND Parent)
{
	static_cast<void>(Parent);
	// The device test's own surface: a hidden window of its own, Vulkan, never shown.
	Window = reinterpret_cast<HWND>(SDL_CreateWindow("", 1, 1, SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN));
}

TTestWindow::~TTestWindow()
{
	SDL_DestroyWindow(reinterpret_cast<SDL_Window *>(Window));
}

#endif // _WIN32

} // namespace patella::vulkan
