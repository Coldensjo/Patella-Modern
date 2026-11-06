// Client.h: TClient, the one object that owns the client's state (Patella-Modern issue #104, plan M7).
//
// M7 moves the original's globals (the file-scope data of each .cpp, and the shared ones of the headers) into
// subsystem objects that TClient owns and passes to each other; notes/decisions.md, "#104: TClient and the
// subsystem objects", has the design: the objects, which globals go where, the order of the moves (one issue per
// subsystem), and the bridge below.
//
// Today: TPreferences (Preferences.cpp, issue #104), TNetwork (Network.cpp), TMapState (the game state: Objects.cpp,
// Map.cpp, Creatures.cpp, Inventory.cpp, issue #106; MapState.h) and TCommunication (Communication.cpp and Status.cpp's
// session, issue #105). Next: TGUI (#107); the platform's services come in as interfaces (#109).
//
// The bridge: during the transition there is one TClient, made on first use by bridge::Client() (Client.cpp), and
// each subsystem's header declares an accessor of its own (Preferences.h: bridge::Preferences()) for the free
// functions of the original's names, which forward to the members. A file's functions stop using the bridge when
// they hold the object they need; when no file uses it, the bridge goes and the program's entry points own TClient.
// Only Client.cpp includes this header: the subsystems do not depend on each other through it.

#pragma once

#include "Communication.h"
#include "MapState.h"
#include "Network.h"
#include "Preferences.h"

namespace patella {

class TClient {
public:
	TClient();
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
};

namespace bridge {
// The one TClient of the transition, made on first use (its members start as the original's statics did).
TClient &Client();
} // namespace bridge

} // namespace patella
