// ClientStateWin32.h: the state dump's reading of the client (issue #22; StateDump.h). Test build only.

#pragma once

#include "testmode/StateDump.h"

#include <vector>

namespace patella::testmode {

// client_state.py's state(): the client's data, read in the process (src/testmode/ClientView.h).
[[nodiscard]] TClientState ReadClientState();
// client_state.py's gui(): the window tree under MainWindow and ModalWindow.
[[nodiscard]] std::vector<TGuiNode> ReadGui();

} // namespace patella::testmode
