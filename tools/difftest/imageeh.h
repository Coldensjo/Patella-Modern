// Exceptions of the original code in the difftest child (issue #22).
//
// EnableImageExceptions makes the image's own C++ exception machinery work:
// its throw (_CxxThrowException, RaiseException), its frame handler
// (__CxxFrameHandler, catch blocks, unwinding), and the C++ library code its
// TMyException constructors need (std::stringstream with its locale). The
// image's code then throws, catches and wraps (TEND) as in the client;
// ImageThrow catches what leaves the image at the boundary and converts it
// into a TThrown, OurThrow does the same for the reconstructed code.

#pragma once

#include "difftest.h"
#include "../../tests/golden/golden_eh.h" // TThrown, CompareThrown, TErrorRound, OurThrow (issue #54)
#include <string>

// Binds the imports the image's exception and iostream code calls, patches its
// locks and per-thread data; DisableImageExceptions undoes every change.
void EnableImageExceptions();
void DisableImageExceptions();

// Patches that DisableImageExceptions undoes as well: a jump at the start of
// an image function, a jump at the start of one of our functions, and an
// entry of difftest.exe's import table.
void PatchImageJump(uint32 Address, void *Target);
void PatchOwnCode(void *Function, void *Target);
void PatchOwnImport(const char *Dll, const char *Name, void *Target);
void BindImageImport(const char *Dll, const char *Name, void *Target); // an import slot of the image

// The image's TMyException object (vtable, Message +0x04, Origin +0x20, both
// VC7.1 std::strings of 0x1c bytes) and its strings.
std::string ImageString(const void *String);
void MakeImageString(void *String, const char *Text); // a VC7.1 string with the image's malloc
void FreeImageString(void *String);

// Calls Function(Context) and returns what escaped from the image.
TThrown ImageThrowCall(void (*Function)(void *), void *Context);
template <class F> static void ThrowThunk(void *Context) { (*(F *)Context)(); }
template <class F> TThrown ImageThrow(F Function) { return ImageThrowCall(ThrowThunk<F>, &Function); }

// The two sides run one after the other (ours first); EH_TRACE=2 names each
// call before it runs.
#define ROUND_COMPARE(Round, What, OurCall, ImageCall) \
	do { \
		(Round).Calling(What, "ours"); \
		TThrown Ours_ = OurThrow([&] { OurCall; }); \
		(Round).Calling(What, "the original"); \
		(Round).Compare(What, Ours_, ImageThrow([&] { ImageCall; })); \
	} while (0)

// The vtable of a test class's object on the image side in an error round:
// our class's vtable (Slots entries) with every entry it inherits unchanged
// from our base class (OurBase, BaseSlots entries) replaced by the image's
// base class's entry (the vtable at ImageBase). The image's code then calls
// the image's base code for what the test class does not override: our
// base code (GUI.cpp's TWindow::Draw, HelpMouseMove...) would catch the
// image's exception in its TEND, read it with our layout and throw ours on.
// One copy per vtable, kept for the run.
void *ImageSideVtable(const void *OurVtable, const void *OurBase, uint32 ImageBase, int BaseSlots = 40,
	int Slots = 64);

// A function that starts with a jump now is a test's fake (earlier tests
// redirect some functions for good): such comparisons are skipped.
bool Redirected(const void *Code);
bool ImageRedirected(uint32 Address);
