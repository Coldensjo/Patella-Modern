// Shared helpers of the differential tests (see difftest.cpp).

#pragma once

#include "../../src/common.h"
#include <stdio.h>

extern int Failures;
extern int Checks;
extern int FailuresPrinted; // DIFFTEST_FAILS=<n>, default 20

#define CHECK(Cond, ...) do { Checks++; if (!(Cond)) { Failures++; if (Failures <= FailuresPrinted) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

uint32 Random();
// Random()'s state: an error round (imageeh.h, issue #34) saves it and restores it, so the
// later inputs stay the same.
uint32 GetRandomSeed();
void SetRandomSeed(uint32 Value);
void RandomText(char *Text, int Length);

template <class T> static T At(uint32 Address) { return (T)(uintptr_t)Address; }
template <class T> static T &Global(uint32 Address) { return *(T *)(uintptr_t)Address; }

// Overwrites the start of a function in the image with a jump to one of ours.
// The image is a private copy, so this changes nothing on disk.
inline void PatchJump(uint32 Address, void *Target)
{
	uint8 *Code = (uint8 *)(uintptr_t)Address;
	Code[0] = 0xe9;
	*(int32_t *)(Code + 1) = (int32_t)((uintptr_t)Target - (Address + 5));
}

// tests/golden/status_tests.cpp: redirects the image's malloc/free and the helpers that
// would reach Windows; safe to call more than once.
void PatchImage();
void SetTestClock(int Value);

void TestSkins();
void TestStatus();
void TestPreferences();
void TestScript();
void PatchScript(); // tests/golden/script_tests.cpp: the image's stdio, remove, rename and sprintf
void TestPreferencesFile();
void TestPlayer();
void TestHelp();
void TestInventory();
void TestSysinc(); // sysinctest.cpp, in sysinctest.exe only
void TestTalk();
void TestObjects(const char *ExePath); // tests/golden/objects_tests.cpp, loads Tibia.dat next to Tibia.exe
void TestCreatures(const char *ExePath); // tests/golden/creatures_tests.cpp, loads Tibia.dat next to Tibia.exe
void TestGUI();
void TestMap(const char *ExePath);     // tests/golden/map_tests.cpp, also loads Tibia.dat
void TestGraphics(const char *ExePath); // graphicstest.cpp, loads Tibia.spr, Tibia.pic and Tibia.dat
void TestDialogWindow();                 // dialogwindowtest.cpp, after TestGUI (restores its patches)
void TestContextMenus();                 // contextmenustest.cpp, after TestDialogWindow (restores its patches)
void TestConsoleWindow();                // consolewindowtest.cpp, after TestContextMenus (restores its patches)
void TestContentWindows();               // contentwindowstest.cpp, after TestConsoleWindow (restores its patches)
void TestDialogElements();               // dialogelementstest.cpp, after TestContentWindows (restores its patches)
void TestSurfaceDX();                    // surfacedxtest.cpp, fake DirectDraw COM objects; restores its patches
void TestDialogs();                      // dialogstest.cpp, after TestDialogElements (restores its patches)
void TestSurfaceDX9();                   // surfacedx9test.cpp, before TestMapWindow (restores its patches)
void TestSurfaceGL();                    // surfacegltest.cpp, before TestMapWindow (restores its patches; own random numbers)
void TestContainer();                    // tests/golden/container_tests.cpp, Container.h's instances; before TestMapWindow (restores its patches)
void TestRegexp();                       // regexptest.cpp, before TestMapWindow (own random numbers)
void TestErrors(const char *ExePath);    // errortest.cpp, loads Tibia.dat;, the error paths (imageeh.cpp); before TestMapWindow (restores its patches; own random numbers)
void TestNetwork();                      // networktest.cpp (includes Network.cpp), before TestMapWindow (restores its patches)
void TestMainFrm();                      // tests/golden/mainfrm_tests.cpp (includes MainFrm.cpp), before TestMapWindow (restores its patches)
void TestMapWindow(const char *ExePath); // mapwindowtest.cpp, loads Tibia.dat
// communicationtest.exe (issue #36, DIFFTEST_COMMUNICATION): Communication.cpp and Control.cpp with every
// callee faked on both sides; in an executable of their own, because difftest.exe's other tests define
// fakes under the names of their functions.
// A test that divides by zero on purpose sets it (communicationtest.cpp: a patch
// block of a zero-sized patch, caught by both sides' catch (...)): the crash
// report then leaves such exceptions out.
extern bool ExpectDivideByZero;
void TestCommunication(const char *ExePath);// communicationtest.cpp: the server packets, the senders, ReceivePackets
void TestControl(const char *ExePath);   // controltest.cpp: input, events, the timer, start-up and shutdown

// dialogwindowtest.cpp defines the shared fakes of the classes DialogWindow.cpp
// uses from files that are not reconstructed (TSubWindowBody's members,
// TResizer, TSkinButton::SetSkins); each test that uses them sets these hooks.
class TComponent;
class TResizer;
class TSkinButton;
class TSubWindowBody;
struct TDialogFakes {
	void (*Body)(TSubWindowBody *Body, const char *Call);   // the test bodies' Minimize ("A0"), Maximize ("A4"), the container body's UpdateUpButton ("Up")
	bool (*BodyAccepts)(TSubWindowBody *Body);              // unused
	void (*NewResizer)(TResizer *Resizer, TComponent *ToResize, bool Param2, bool Param3, bool Param4);
	void (*SetSkins)(TSkinButton *Button, int ButtonUp, int ButtonDown);
};
extern TDialogFakes DialogFakes;
// dialogwindowtest.cpp: the fake of the container body's UpdateUpButton
// (0x42e4e0, called by TSubWindow::Arrange); dialogwindowtest.cpp and
// mainwindowtest.cpp redirect the image's and ours to it.
void __fastcall FakeUpdateUpButton(void *Body, void *);
// contentwindowstest.cpp: while it is set (that test), the shared fakes of the
// DialogElements.cpp classes (the constructors in consolewindowtest.cpp,
// contextmenustest.cpp, mainwindowtest.cpp and contentwindowstest.cpp)
// report every object they make, with its kind and the arguments.
extern void (*ElementCreated)(void *Object, const char *Kind, const char *Arguments);
void ReportElement(void *Object, const char *Kind, const char *Format, ...);
void TestMainWindow();                   // mainwindowtest.cpp, after TestMapWindow (patches for good as well)
