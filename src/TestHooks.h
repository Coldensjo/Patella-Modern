// TestHooks.h: the places where the test mode reaches into the client (issue #21;
// notes/modernization-plan.md, 4.4). Included by common.h.
//
// In the product and oracle builds every hook is the system call it replaces, word for word, so they
// compile to the same code and contain nothing of the test mode. The test build (PatellaTest.exe,
// cmake/WindowsX64.cmake) defines PATELLA_TEST_MODE: the hooks go to src/testmode/TestModeWin32.cpp,
// which, with a script (--test-script <file>), drives the client from src/testmode/InputScript.h.
//
// Issue #109 (notes/decisions.md, "#109: the platform interfaces"): the clocks are no hooks any more. timeGetTime,
// time and the frame timer's Sleep (PATELLA_TIME_GET_TIME, PATELLA_TIME and PATELLA_FRAME_SLEEP until then) are the
// platform's IClock (src/platform/Platform.h), whose test implementation is the test mode's clock (TestModeWin32.cpp's
// Platform()).
//
// Issue #119 (notes/decisions.md, "#119: the platform interfaces, part 2"): the cursor, the main timer, the idle step
// and the capture change (PATELLA_GET_CURSOR_POS, PATELLA_SET_MAIN_TIMER, PATELLA_TEST_IDLE and
// PATELLA_TEST_CAPTURE_CHANGED until then) are the platform's IInput, the message boxes, the windows' count and the
// display modes (PATELLA_MESSAGE_BOX, PATELLA_ENUM_WINDOWS, PATELLA_ENUM_DISPLAY_SETTINGS) its IWindow, and the
// clipboard its IClipboard; their test implementations are TestModeWin32.cpp's (TTestInput, TTestWindow,
// TMemoryClipboard with a script). The hooks below are the test mode's observers and failures, which are not a
// platform's service (the packets, the frames, the present and InitControl failures), and the dropped desktop input
// (at the end: a filter of Win32's window messages; M8's SDL3 input has events of its own).
//
// Issue #22 (the packet log and the frame dump; notes/testmode.md), statements without a semicolon too,
// nothing at all outside the test build:
//
//	PATELLA_TEST_RSA_BLOCK(Block)      Communication.cpp SendLogin, SendGameLogin: the RSA block before
//	                                   it is encrypted (the packet log writes the first packets with it)
//	PATELLA_TEST_PACKET_SENT(Data, Size, Padded)
//	                                   Network.cpp SendCommand: SendData with the packet complete, before
//	                                   XTEA encryption (Padded: its length with the padding; 0: not
//	                                   encrypted)
//	PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize)
//	                                   Network.cpp PrepareNextPacket: ReceiveData with the next packet
//	                                   decrypted
//	PATELLA_TEST_FRAME_OBSERVERS(Frame, Pixels, Mask, Device)
//	                                   vulkan/SurfaceVulkan.cpp InitDisplay: the test mode's frame dump
//	                                   becomes the engine's frame observer (and, issue #92, its device
//	                                   observer: the control channel's probe names the device); the only
//	                                   observers since issue #97 (no patcap.dll lookup)
//
// Issue #83 (a statement without a semicolon, nothing at all outside the test build):
//
//	PATELLA_TEST_PRESENT_FAILURE(Failure)
//	                                   vulkan/SurfaceVulkan.cpp, before TRenderer::Present: with
//	                                   PATELLA_TEST_FAIL_PRESENT=1 in the environment the statement Failure
//	                                   (a TVulkanError thrown, as a failing vkQueuePresentKHR's) runs in place
//	                                   of every present, so the first one fails (the CPU driver's present on a
//	                                   hidden desktop, issue #56)
//
// Issue #90 (a statement without a semicolon, nothing at all outside the test build):
//
//	PATELLA_TEST_INIT_FAILURE(Step)    Control.cpp InitControl, before each of its 15 steps (Step 1-15) and
//	                                   after the last (16): with PATELLA_TEST_FAIL_INIT=<Step> in the
//	                                   environment InitControl throws PROGRAM_ERROR 6 (InitNetwork's "Cannot
//	                                   initialize Windows Sockets.") there, as a failing step does, so its
//	                                   catch runs ExitControl with the steps before it done
//
// The state dump reads the client's data through the View functions at the end of Creatures.cpp,
// Inventory.cpp, Map.cpp and Status.cpp (src/testmode/ClientView.h; test build only).
//
// With a script, CPlayerApp::PreTranslateMessage (test build only) drops the keyboard and mouse input
// that comes from Windows, so the desktop's own mouse and keyboard cannot reach the client.

#pragma once

#if defined(PATELLA_TEST_MODE)

// The Windows types of the declarations: MFC, as common.h includes it (before anything of Windows, which
// MFC requires; the test build has real MFC, never the shim). PATELLA_CDECL: compat/msvc.h.
// In the client's sources common.h has included both already (issue #49).
#include <afxwin.h>

#include "compat/msvc.h"
#include "compat/span.h"
#include "testmode/ClientView.h" // IWYU pragma: export

namespace patella::testmode {
// True: the message is input from Windows that the driver's script replaces (dropped).
bool DropInput(const MSG *Message);
// The buffers as spans (issue #60): the RSA block, and the whole of SendData and ReceiveData.
void RsaBlock(patella::TSpan<const unsigned char, 128> Block);
void PacketSent(patella::TSpan<const unsigned char> Data, int Size, int Padded);
void PacketReceived(patella::TSpan<const unsigned char> Data, int PacketSize);
void FrameObservers(int(PATELLA_CDECL **Frame)(int, int), void(PATELLA_CDECL **Pixels)(const void *, int, int, int),
	void(PATELLA_CDECL **Mask)(const unsigned char *, int, int), void(PATELLA_CDECL **Device)(const char *, int, int));
// True: PATELLA_TEST_FAIL_PRESENT is 1 (issue #83), every present fails.
bool PresentFails();
// True: PATELLA_TEST_FAIL_INIT is Step (issue #90), InitControl fails there.
bool InitFails(int Step);
} // namespace patella::testmode

#define PATELLA_TEST_RSA_BLOCK(Block) patella::testmode::RsaBlock(Block);
#define PATELLA_TEST_PACKET_SENT(Data, Size, Padded) patella::testmode::PacketSent(Data, Size, Padded);
#define PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize) patella::testmode::PacketReceived(Data, PacketSize);
#define PATELLA_TEST_FRAME_OBSERVERS(Frame, Pixels, Mask, Device) \
	patella::testmode::FrameObservers(&(Frame), &(Pixels), &(Mask), &(Device));
#define PATELLA_TEST_PRESENT_FAILURE(Failure) \
	if (patella::testmode::PresentFails()) { Failure; }
#define PATELLA_TEST_INIT_FAILURE(Step) \
	if (patella::testmode::InitFails(Step)) { throw TProgramError{PROGRAM_ERROR::SOCKET_INIT}; }

#else

// No statement at all: an empty one is a nop in an unoptimised MSVC build (Debug), which would move
// the code after it.
#define PATELLA_TEST_RSA_BLOCK(Block)
#define PATELLA_TEST_PACKET_SENT(Data, Size, Padded)
#define PATELLA_TEST_PACKET_RECEIVED(Data, PacketSize)
#define PATELLA_TEST_FRAME_OBSERVERS(Frame, Pixels, Mask, Device)
#define PATELLA_TEST_PRESENT_FAILURE(Failure)
#define PATELLA_TEST_INIT_FAILURE(Step)

#endif
