// PresentWindow.cpp: the windows the Vulkan engine presents in (PresentWindow.h; issue #52).

#include "vulkan/PresentWindow.h"

namespace patella::vulkan {

namespace {

constexpr const char *ClassName = "PatellaVulkanPresent";

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

} // namespace

TPresentWindows::~TPresentWindows()
{
	Close();
}

HWND TPresentWindows::Open(HWND Parent)
{
	Retire();
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
}

TTestWindow::TTestWindow(HWND Parent) : Window(CreateChild(Parent, false))
{
}

TTestWindow::~TTestWindow()
{
	DestroyChild(Window);
}

} // namespace patella::vulkan
