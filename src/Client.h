// Client.h: TClient, the one object that owns the client's state (Patella-Modern issue #104, plan M7).
//
// M7 moves the original's globals (the file-scope data of each .cpp, and the shared ones of the headers) into
// subsystem objects that TClient owns and passes to each other; notes/decisions.md, "#104: TClient and the
// subsystem objects", has the design: the objects, which globals go where, the order of the moves (one issue per
// subsystem), and the bridge below.
//
// Today: TPreferences (Preferences.cpp, issue #104), TNetwork (Network.cpp), TMapState (the game state: Objects.cpp,
// Map.cpp, Creatures.cpp, Inventory.cpp, issue #106; MapState.h), TCommunication (Communication.cpp and Status.cpp's
// session, issue #105), TGraphics (graphics.cpp, Surface.cpp and Skins.cpp with GUI.cpp's SkinProperties, issue
// #116; graphics.h) and TGUI (Talk.cpp, Help.cpp, Status.cpp's user interaction and the windows' data, issue
// #107; GUI.h). The platform's services are interfaces (issue #109, platform/Platform.h): TClient takes them in its
// constructor and passes them on (TPreferences the file system, TNetwork the socket).
//
// The bridge: during the transition there is one TClient, made on first use by bridge::Client() (Client.cpp), and
// each subsystem's header declares an accessor of its own (Preferences.h: bridge::Preferences()) for the free
// functions of the original's names, which forward to the members. A file's functions stop using the bridge when
// they hold the object they need; when no file uses it, the bridge goes and the program's entry points own TClient.
// Only Client.cpp includes this header: the subsystems do not depend on each other through it.

#pragma once

#include "Communication.h"
#include "GUI.h"
#include "MapState.h"
#include "Network.h"
#include "Preferences.h"
#include "graphics.h"
#include "platform/Platform.h"

namespace patella {

class TClient {
public:
	// The platform's services (issue #109; platform/Platform.h), which the subsystems take in their constructors.
	explicit TClient(platform::TPlatform &Platform);
	TClient(const TClient &) = delete;
	TClient &operator=(const TClient &) = delete;
	TClient(TClient &&) = delete;
	TClient &operator=(TClient &&) = delete;
	~TClient() = default;

	// The subsystems, in the order they are made (a later one may take an earlier one in its constructor).
	TPreferences Preferences;
	TNetwork Network;
	TMapState MapState;
	TCommunication Communication;
	TGraphics Graphics; // issue #116
	TGUI GUI;           // issue #107
};

namespace bridge {
// The one TClient of the transition, made on first use (its members start as the original's statics did), on the
// process's platform (platform::Platform(), the entry point's executable's).
TClient &Client();
} // namespace bridge

} // namespace patella
