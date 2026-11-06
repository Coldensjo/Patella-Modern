// ClientStateWin32.h: the state dump's reading of the client (issue #22; StateDump.h). Test build only.

#pragma once

#include "testmode/StateDump.h"

#include <vector>

namespace patella::testmode {

// client_state.py's state(): the client's data, read in the process (src/testmode/ClientView.h).
[[nodiscard]] TClientState ReadClientState();
// client_state.py's gui(): the window tree under MainWindow and ModalWindow.
[[nodiscard]] std::vector<TGuiNode> ReadGui();
// The control channel's probe (issue #92): ReadClientState, ReadGui and what the tools' drivers read
// besides (the characters, the skin values, the components' members, the map's names). The device,
// the frames, the ticks, the step and the time are the caller's.
void ReadProbe(TProbe &Probe);

} // namespace patella::testmode
