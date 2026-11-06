// Platform.h: TPlatform, the platform's services of the process (Patella-Modern issue #109, plan M7;
// notes/decisions.md, "#109: the platform interfaces"). Not part of the original source. Portable: no platform header.
//
// The interfaces: IClock (Clock.h), IFileSystem (FileSystem.h), ISocket (Socket.h). The entry point's executable
// makes one implementation of each and hands them to TClient's constructor (Client.cpp), which passes them on: TNetwork
// ISocket, TPreferences IFileSystem. The files that are not yet an object's members, or whose objects TClient does
// not make (sysinc.cpp's clock, Script.cpp's files, the wall clock of Talk.cpp, Creatures.cpp, ConsoleWindow.cpp and
// ContentWindows.cpp, the Vulkan engine's frame timer), reach them through Platform(), as the subsystems' free
// functions reach TClient through the bridge (Client.h); when they hold their object, they use its references.
//
// Platform() is defined once per executable, by its entry point's side:
//
//	Patella.exe, the golden runner     platform/Win32Platform.cpp: TWin32Clock, TSystemFileSystem, TWin32Socket
//	PatellaTest.exe                    testmode/TestModeWin32.cpp: the test mode's clock (TVirtualClock with
//	                                   --test-script, else TWin32Clock), TSystemFileSystem, TWin32Socket
//
// Each is made on first use (a static initialiser, sysinc.cpp's StartTime, reads the clock before main) and never
// destroyed (the client reads the clocks until its very end, static destructors included).

#pragma once

#include "platform/Clock.h"      // IWYU pragma: export
#include "platform/FileSystem.h" // IWYU pragma: export
#include "platform/Socket.h"     // IWYU pragma: export

namespace patella::platform {

struct TPlatform {
	IClock &Clock;
	IFileSystem &FileSystem;
	ISocket &Socket;
};

// The process's platform (above).
TPlatform &Platform();

} // namespace patella::platform
