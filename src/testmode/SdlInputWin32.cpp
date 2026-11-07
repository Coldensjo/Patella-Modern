// SdlInputWin32.cpp: the test mode's scripted window messages as the SDL3 window's input (Patella-Modern issue #123;
// SdlInputWin32.h). Not part of the original source.

#include "testmode/SdlInputWin32.h"

#include "SdlFrame.h"

#include <SDL3/SDL.h>


namespace patella::testmode {

bool DeliverToSdlWindow(UINT Message, WPARAM WParam, LPARAM LParam)
{
	SDL_Event Event{};
	switch (Message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		// The SDL3 window takes the keys, the characters and the buttons as window messages on Windows (SdlFrame.cpp
		// OnKeyMessage, OnWindowChar, OnButtonMessage): dispatched, as the desktop's come.
		return false;
	case WM_MOUSEWHEEL:
		Event.type = SDL_EVENT_MOUSE_WHEEL;
		Event.wheel.y = static_cast<float>(static_cast<short>(HIWORD(WParam))) / static_cast<float>(WHEEL_DELTA);
		Event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
		Event.wheel.mouse_x = static_cast<float>(static_cast<short>(LOWORD(LParam)));
		Event.wheel.mouse_y = static_cast<float>(static_cast<short>(HIWORD(LParam)));
		break;
	case WM_MOUSEMOVE: return true;
	default: return false;
	}
	Event.common.timestamp = SDL_GetTicksNS();
	sdlframe::ScriptedInput(Event);
	return true;
}

} // namespace patella::testmode
