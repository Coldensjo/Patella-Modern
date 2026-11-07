// SdlInputWin32.cpp: the test mode's scripted window messages as the SDL3 window's input (Patella-Modern issue #123;
// SdlInputWin32.h). Not part of the original source.

#include "testmode/SdlInputWin32.h"

#include "SdlFrame.h"
#include "types/Cp1252.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

namespace patella::testmode {

namespace {

// The scan code of a key message's key: its virtual key, with the extended flag where Windows tells two keys of one
// virtual key apart by it (Return and the keypad's Enter, the navigation block and the keypad, the left and right
// Control and Alt; the right Shift by its scan code, 0x36). SDL_SCANCODE_UNKNOWN for a key the client does not tell
// apart (TranslateKey's 0, no control character).
SDL_Scancode ScancodeOf(unsigned int VirtualKey, LPARAM LParam)
{
	const bool Extended = ((static_cast<std::uint32_t>(LParam) >> 24) & 1U) != 0;
	const unsigned int Scan = (static_cast<std::uint32_t>(LParam) >> 16) & 0xffU;
	if (VirtualKey >= 'A' && VirtualKey <= 'Z')
		return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (VirtualKey - 'A'));
	if (VirtualKey >= '1' && VirtualKey <= '9')
		return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (VirtualKey - '1'));
	if (VirtualKey >= VK_F1 && VirtualKey <= VK_F12)
		return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + (VirtualKey - VK_F1));
	if (VirtualKey >= VK_NUMPAD1 && VirtualKey <= VK_NUMPAD9)
		return static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + (VirtualKey - VK_NUMPAD1));
	switch (VirtualKey) {
	case '0': return SDL_SCANCODE_0;
	case VK_NUMPAD0: return SDL_SCANCODE_KP_0;
	case VK_DECIMAL: return SDL_SCANCODE_KP_PERIOD;
	case VK_CLEAR: return SDL_SCANCODE_KP_5;
	case VK_RETURN: return Extended ? SDL_SCANCODE_KP_ENTER : SDL_SCANCODE_RETURN;
	case VK_ESCAPE: return SDL_SCANCODE_ESCAPE;
	case VK_TAB: return SDL_SCANCODE_TAB;
	case VK_BACK: return SDL_SCANCODE_BACKSPACE;
	case VK_SPACE: return SDL_SCANCODE_SPACE;
	case VK_SHIFT: return Scan == 0x36 ? SDL_SCANCODE_RSHIFT : SDL_SCANCODE_LSHIFT;
	case VK_LSHIFT: return SDL_SCANCODE_LSHIFT;
	case VK_RSHIFT: return SDL_SCANCODE_RSHIFT;
	case VK_CONTROL: return Extended ? SDL_SCANCODE_RCTRL : SDL_SCANCODE_LCTRL;
	case VK_LCONTROL: return SDL_SCANCODE_LCTRL;
	case VK_RCONTROL: return SDL_SCANCODE_RCTRL;
	case VK_MENU: return Extended ? SDL_SCANCODE_RALT : SDL_SCANCODE_LALT;
	case VK_LMENU: return SDL_SCANCODE_LALT;
	case VK_RMENU: return SDL_SCANCODE_RALT;
	case VK_PRIOR: return Extended ? SDL_SCANCODE_PAGEUP : SDL_SCANCODE_KP_9;
	case VK_NEXT: return Extended ? SDL_SCANCODE_PAGEDOWN : SDL_SCANCODE_KP_3;
	case VK_END: return Extended ? SDL_SCANCODE_END : SDL_SCANCODE_KP_1;
	case VK_HOME: return Extended ? SDL_SCANCODE_HOME : SDL_SCANCODE_KP_7;
	case VK_LEFT: return Extended ? SDL_SCANCODE_LEFT : SDL_SCANCODE_KP_4;
	case VK_UP: return Extended ? SDL_SCANCODE_UP : SDL_SCANCODE_KP_8;
	case VK_RIGHT: return Extended ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_KP_6;
	case VK_DOWN: return Extended ? SDL_SCANCODE_DOWN : SDL_SCANCODE_KP_2;
	case VK_INSERT: return Extended ? SDL_SCANCODE_INSERT : SDL_SCANCODE_KP_0;
	case VK_DELETE: return Extended ? SDL_SCANCODE_DELETE : SDL_SCANCODE_KP_PERIOD;
	default: return SDL_SCANCODE_UNKNOWN;
	}
}

// NumLock as Windows had it for the key: on for the keypad's digit keys (VK_NUMPAD0-9, VK_DECIMAL), off otherwise
// (the keypad's navigation keys are its keys with NumLock off).
bool NumLockFor(unsigned int VirtualKey)
{
	return (VirtualKey >= VK_NUMPAD0 && VirtualKey <= VK_NUMPAD9) || VirtualKey == VK_DECIMAL;
}

} // namespace

bool DeliverToSdlWindow(UINT Message, WPARAM WParam, LPARAM LParam)
{
	SDL_Event Event{};
	std::string Text; // the text event's characters, alive while the window handles it
	switch (Message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP: {
		const auto VirtualKey = static_cast<unsigned int>(WParam);
		const bool Down = Message == WM_KEYDOWN || Message == WM_SYSKEYDOWN;
		Event.type = Down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
		Event.key.scancode = ScancodeOf(VirtualKey, LParam);
		Event.key.key = SDL_GetKeyFromScancode(Event.key.scancode, SDL_KMOD_NONE, true);
		Event.key.mod = NumLockFor(VirtualKey) ? SDL_KMOD_NUM : SDL_KMOD_NONE;
		Event.key.raw = static_cast<Uint16>((static_cast<std::uint32_t>(LParam) >> 16) & 0xffU);
		Event.key.down = Down;
		Event.key.repeat = Down && ((static_cast<std::uint32_t>(LParam) >> 30) & 1U) != 0;
		break;
	}
	case WM_CHAR:
		// The MFC frame's OnChar drops a character above 255; so does the text event's way (nothing to deliver).
		if (WParam > 255)
			return true;
		Text = text::Cp1252ToUtf8(std::string(1, static_cast<char>(WParam)));
		Event.type = SDL_EVENT_TEXT_INPUT;
		Event.text.text = Text.c_str();
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK: {
		const bool Up = Message == WM_LBUTTONUP || Message == WM_RBUTTONUP;
		const bool Double = Message == WM_LBUTTONDBLCLK || Message == WM_RBUTTONDBLCLK;
		const bool Left = Message == WM_LBUTTONDOWN || Message == WM_LBUTTONUP || Message == WM_LBUTTONDBLCLK;
		Event.type = Up ? SDL_EVENT_MOUSE_BUTTON_UP : SDL_EVENT_MOUSE_BUTTON_DOWN;
		Event.button.button = Left ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
		Event.button.down = !Up;
		Event.button.clicks = Double ? 2 : 1;
		Event.button.x = static_cast<float>(static_cast<short>(LOWORD(LParam)));
		Event.button.y = static_cast<float>(static_cast<short>(HIWORD(LParam)));
		break;
	}
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
