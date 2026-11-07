// types/Callback.h: TCallback, a receiver and an event number (GUI.h's until Patella-Modern issue #110). Here so that
// Creatures.cpp's NoCallback needs no GUI.h, which brings the windows and the drawing: patella_core compiles
// Creatures.cpp (notes/decisions.md, "#110"). GUI.h exports it, so the window files see it as before. Its two
// functions are still GUI.cpp's (0x496130, 0x4a0f00).

#pragma once

class TComponent;

// A receiver and an event number (8 bytes, passed by value): Send() calls
// Receiver->HandleEvent(Event). Buttons, timers and hotkeys keep one.
struct TCallback {
	TCallback() : Receiver(nullptr), Event(0) {} // inlined (vector<THotkey>, TTimer)
	TCallback(TComponent *Receiver, int Event);  // 0x496130
	void Send() const;                           // 0x4a0f00, line 1931
	TComponent *Receiver;                        // +0x00
	int Event;                                   // +0x04
};
