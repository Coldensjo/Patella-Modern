# Rendering comparison (issue #20)

Patella.exe and the original Tibia.exe draw the same scenes with each graphics engine (DirectX 5:
SurfaceDX.cpp, DirectX 9: surfaceDX9.cpp, OpenGL: SurfaceGL.cpp), windowed and fullscreen; the frame
each client composes is read back from the engine's own surfaces just before it is shown and the
pictures are compared pixel by pixel. Choices and their reasons: notes/decisions.md, "Rendering
comparison (issue #20)". Builds on issue #19's replay (notes/traffic.md: tools/proxy.py, the session
driver, the memory reader) and the run folders of issues #18 and #21.

## Running it

```
python -I tools/build.py windows-x64
python -I tools/build.py windows-x64                # only for --patella with the x64 client
python -I tools/compare_rendering.py [--out build\render\result] [--engines dx5,dx9,gl | vulkan,vulkan-cpu]
	[--modes windowed,fullscreen] [--no-repeat] [--recording build\render\session.jsonl] [--compare-only]
	[--patella build\windows-x64\Release\Patella.exe] [--tolerance N]
```

- `--engines vulkan,vulkan-cpu` (Patella-Modern issue #19; notes/vulkan.md): Patella.exe, the x64 client, runs its
  Vulkan engine (src/vulkan/SurfaceVulkan.cpp, its engine 2) on a GPU (`PATELLA_VULKAN=gpu` in its environment) or
  on the CPU driver next to it (`PATELLA_VULKAN=cpu`; setup() copies vulkan_lvp.dll and lvp_icd.x86_64.json from
  the `--patella` executable's folder into the run folder), Tibia.exe its DirectX 9 engine, the reference (plan
  D7); both get `Engine = 2`. A Patella.exe run must show every frame through the Vulkan engine (API "Vulkan") on
  the kind of device asked for, else the run reports a problem; run.json's facts name the device. Only the x64
  Patella.exe has the engine, so these engines need `--patella build\windows-x64\Release\Patella.exe`.
- Since Patella-Modern issue #20 the x64 client has only the Vulkan engine: the original engines (`dx5`, `dx9`,
  `gl`) need the oracle's Patella.exe, and the script refuses them for an x64 one (except with `--compare-only`).
  The default `--engines` follows `--patella`: `dx5,dx9,gl` for the oracle's, `vulkan,vulkan-cpu` for an x64
  one. Tibia.exe's runs are the same as before.
- `--region-tolerance N` (default 2; the user's decision of November 3, 2026): on the pixels the Vulkan engine
  marks as device-filtered for each captured frame (the light and the bilinear stretches: the start screen, the
  stretched map view, scaled item pictures, and exact copies of them; notes/vulkan.md, "Pixels"), a pixel whose
  colour channels all differ by at most N counts as equal; everywhere else the comparison is exact. The marks are
  stored next to Patella.exe's pictures as `<scene>.filtered.png` (white where marked).
- `--tolerance N`: the same everywhere (default 0, exact). Every report line of a differing scene gives the largest
  channel difference outside the masks either way.

- `--patella <exe>` (Patella-Modern issue #18): the x64 Patella.exe compared with Tibia.exe, default
  build\windows-x64\Release\Patella.exe since issue #94 (its test build runs, issue #92). Until #94 the default was
  the 32-bit oracle's Patella.exe with the three original engines (dx5, dx9, gl), which left the fork with it.

- No server: every game scene comes from a recorded session replayed by tools/proxy.py, so both clients
  get the same server bytes. The recording is issue #19's live session of Tibia.exe
  (compare_traffic.py's `orig-live.jsonl`: login, walking, talking, looking, a container, a trade with
  "Patella Second", logout); copy it to `build\render\session.jsonl` or pass `--recording`. A new one:
  `python -I tools/compare_traffic.py --out <dir>` (starts and stops the test server itself), then use
  `<dir>\orig-live.jsonl`. The session's look step is compare_traffic.py's, "look at own field" (map field (8,6),
  issues #137, #140; LOOK in the script). Before any run the script reads the recording's look step (the look
  packet's field against the player's start field, session_client.recorded_look) and refuses a recording with
  another one: a recording made before #137 (it looks north, field (8,5)) would replay one client packet off, and the
  proxy would report it as a difference of the client. The frozen reference recordings keep "look north":
  tools/reference_record.py passes LOOK_NORTH to the session.
- The capture helper is loaded into the client, so it has the client's bitness (`cmake/Capture.cmake`):
  `build\capture-x86\Release\patcap.dll`, the patcap target of the capture-x86 preset (issue #94; the oracle-x86
  preset's until then), for Tibia.exe. The script builds the target itself when the DLL is missing or older than
  tools/capture/patcap.c (configuring the preset first if needed). Before issue #10 tools\build-capture.cmd built
  build\capture\patcap.dll. From Patella-Modern issue #18 to #92 the windows-x64 preset built a 64-bit one for an
  x64 Patella.exe; since #92 the x64 client is never hooked: its test build, PatellaTest.exe, runs in its place
  with patcap's time course from its own test mode (`clock manual`, `cursor 1020 760`, then advance, frame and post
  lines through its control channel; the frame dump's picture and mask are the scene and its `.filtered.png`;
  notes/testmode.md, "The control channel"), and patcap.c does not build for x64.
- Run folders `build\render\orig`, `build\render\patella` and `build\render\orig-repeat` (the
  second Tibia.exe run's own since Patella-Modern issue #12; before, it ran in build\render\orig)
  (compare_traffic.py's set-up through its set_folder: copies of the data files, the clients patched by
  patch_client.py; then the import of patcap.dll, below). Before every run the folder is emptied but for
  the data files, the client and patcap.dll (compare_traffic.clean_run_dir: the automap files and
  error.txt of an earlier run went into the next before #12). Tibia.cfg is compare_traffic.py's with
  `Engine = N` (and `FullscreenMode` for the fullscreen runs).
- Per engine and mode: Tibia.exe, Patella.exe, then Tibia.exe again (`orig-repeat`, the evidence that
  what differs between two runs of the same client is masked; `--no-repeat` skips it). About
  26 minutes for all 18 runs (three engines, two modes, three runs each);
  `--engines dx5 --modes windowed --no-repeat` takes about 3 minutes.
- Output in `--out`: `<engine>[-fullscreen]\<run>\<scene>.png` (the captured frames), `<run>\run.json`
  (scene digests, masks, frames, ticks, the API that showed the frames, proxy problems),
  `<run>\replay.jsonl` (the replay's packets), `diff-<run>-<scene>.png` (the first picture darkened, the
  differing pixels red) and `report.txt`.
- Exit code 0 when every scene is identical (outside the masks below), 1 on a difference, 2 when a run
  failed.
- The fullscreen runs use the desktop's own mode (1024x768x32, the mode the monitor-off display has), so
  no display mode really changes; the script refuses them when the desktop is in another mode, checks the
  mode before and after every run and stops when it changed. So they cannot run on an ordinary desktop: on
  the 5120x1440 one (Patella-Modern issue #13) the script stops before any client with "the desktop is in
  (5120, 1440, 32, 120), not (1024, 768, 32): the fullscreen runs would change the display mode", and
  `--modes windowed` is what runs there. Letting the clients change the mode instead would be a real mode
  switch of the user's monitor (slow, rearranges the other windows) and is not what the comparison needs;
  decision D11 (notes/modernization-plan.md: borderless fullscreen at the desktop's resolution, no
  exclusive display modes) removes the mode change from the client later, and the fullscreen runs can then
  use any desktop.
- The clients' windows get posted messages only, but real input reaches them too: keep off the mouse and
  the keyboard during a run (notes/reconstruction.md "Test server"; a click during a run on November 2
  crashed Tibia.exe with GUI.cpp 1633 "Capture==NULL").

## When a run fails (Patella-Modern issue #98)

A run that fails names its cause: the message of a failed start, a capture that got no frame, a minimized window or
a failed session step ends with the window's state (visible or hidden, minimized or not, its client area, which
window is the foreground and whether that one covers its monitor: another program in full-screen), the display's
(on, off or dimmed: GUID_CONSOLE_DISPLAY_STATE; the user's idle time against the power scheme's "turn off the
display after"; the input desktop, `Winlogon` while the session is locked; the monitors; the mode) and, for
Tibia.exe, patcap.dll's Direct3D 9 course (Direct3DCreate9 called or not, CreateDevice's result or that it has not
returned, TestCooperativeLevel's last result, the Presents: how many, the first and the last time, the last result).
patcap.dll writes its rare events with the milliseconds since its load to `patcap.log` in the run folder (the load,
each Direct3DCreate9 and CreateDevice, the first frame, a change of TestCooperativeLevel's or Present's result,
every capture asked for and done); the tool keeps it as `<out>\<engine>\orig\patcap.log`. Every run's log starts with
a `display:` line. `python -I tools/compare_rendering.py --self-test` checks the shared memory's offsets against
patcap.c and the diagnosis texts (no client).

What #98 found, and what the tool does about it:

- **Tibia.exe's start hangs in the driver, once in a while.** Tibia.exe makes its Direct3D 9 device twice at start-up
  (Direct3DCreate9 and CreateDevice at about 80 ms, again at about 480 ms; the first frame at about 560 ms). In one
  start of 15 here (November 6, 2026, the first start after the windows-x64 build; the display on, the window
  visible, Chrome in the foreground) the second CreateDevice was called and did not return: no TestCooperativeLevel,
  no Present, so the first capture timed out with "frames 0" (#92's, #94's and #97's failures; #97's first
  compare_traffic.py run, whose helper Tibia.exe got no game connection within 10 s, fits too: the same start with
  the same `Engine = 2`, without patcap.dll). The next start worked every time. So Driver.start waits for the first
  frame (up to 30 s, FIRST_FRAME_TIMEOUT; not a fixed 2 s), and a client that shows none is ended and started once
  more (START_ATTEMPTS = 2): nothing has been sent to it yet, so the run is the same. The log says `start 1 of 2
  failed, starting the client again: ...` with the diagnosis. A second failure fails the run with it.
- **The display most likely turned off during #94's GPU pass** (nothing recorded the display's state then; the
  `display:` lines do now). The tool's input is posted window messages, which Windows does not
  count as the user's: the power scheme's display timeout (900 s on AC here) runs out during the runs when nobody
  touches the machine. The monitor is a DisplayPort one (Samsung, 5120x1440): asleep, it is gone for Windows, the
  desktop falls back to 1024x768 (above: "How the pixels are read without a display") and Windows minimizes the
  windows of a monitor that went away (Settings, System, Display, "Minimize windows when a monitor is
  disconnected"). A minimized window has no client area: in #94's GPU pass the x64 client's frames were 0x0 from
  "walk south" on, the map window had no area, the click on the trade partner hit nothing, the client sent no trade
  request (the proxy: recorded 0x7d, the client sent nothing) and "no window of class TTradeOffers (#0) within 5 s"
  followed. The trade window was not late: there was none. What made the window minimized is inferred, not seen: a
  minimized window is the only way to a 0x0 client area here, and the display's timeout is the one cause of a
  minimize that nobody saw; on November 6 at 17:10 the display was on after 1705 s without input (something held it
  on then), so it does not always happen. The tool keeps the display (and the system) on while it
  runs (SetThreadExecutionState with ES_DISPLAY_REQUIRED, as a video player does; a display already off comes back
  on; no input is sent), stops a run at a scene whose window is minimized ("the client's window is minimized; ...")
  and at a 0x0 frame dump, with the diagnosis.
- **Not done**: bringing the window to the foreground. The clients draw and the captures work without it (every
  passing run of #98 had another program in the foreground), and taking the foreground would take the user's
  keyboard. Restoring a minimized window either: the scene would follow other window messages than the other
  client's; the run fails instead.

How to avoid the rest: keep the session unlocked; no other program in exclusive full-screen; for unattended runs
also Windows' display timeout off (`powercfg /change monitor-timeout-ac 0`) or "Minimize windows when a monitor is
disconnected" off, if anything else could let the monitor sleep; run no second live check at the same time (another
worktree's ctest starts PatellaTest.exe too).

## How the pixels are read without a display

The monitor is off (issue #18: screenshots of the window came out white; with no monitor the desktop falls
back to 1024x768 and DWM composes nothing). The clients' own frames do not need the display: each engine
composes the frame in a surface of its own and only then shows it. tools/capture/patcap.c is a DLL in the
client's process that reads that surface at the moment the client shows it:

| Engine | The client shows the frame with | patcap.dll reads |
| :--- | :--- | :--- |
| DirectX 5 (SurfaceDX.cpp FinishFrame) | windowed: `IDirectDrawSurface4::Blt` of SCREEN_SURFACE into the primary; fullscreen: `Flip` | the Blt's source surface and rectangle (Lock); for Flip the back buffer |
| DirectX 9 (surfaceDX9.cpp FinishFrame) | `IDirect3DDevice9::Present(&Rect[SCREEN_SURFACE], ...)` | the back buffer through `GetRenderTargetData`, the Present's source rectangle |
| OpenGL (SurfaceGL.cpp FinishFrame) | `SwapBuffers` | `glReadPixels` of the back buffer (the client area), in the window's context |
| Vulkan (x64 only; src/vulkan/SurfaceVulkan.cpp FinishFrame, Patella-Modern issue #19) | `vkQueuePresentKHR` of a blit of the canvas | nothing: since issue #92 the test mode's frame dump, the engine's own observer (the picture and its device-filtered pixels). Until #92 the engine called patcap's exports `PatcapVulkanFrame`, `PatcapVulkanMask` (`<capture>.mask`), `PatcapVulkanPixels` and `PatcapVulkanDevice`; its lookup of patcap.dll left with #97, the exports with #98 |

The DLL patches the vtable entries when the client creates the objects (DirectDrawCreate and
IDirectDraw4::CreateSurface; GetProcAddress("Direct3DCreate9") and IDirect3D9::CreateDevice;
wglMakeCurrent tells which context belongs to the window's DC). The same code runs in both clients; it
sees only what the client passes to the API. A capture writes a 24-bit picture (16-bit formats would be
widened, the display is 32-bit); the script stores PNGs.

**Loading the DLL**: the run folders' copies of the clients get one more import descriptor (patcap.dll,
by name `PatcapLoaded`) in a new last section `.patcap` (compare_rendering.add_import: the old
descriptors unchanged, the data directory pointed at the new table, the bound import directory cleared;
it checks that no other byte changed). It takes PE32 and, since Patella-Modern issue #18, PE32+ images:
the data directories start 16 bytes later in a PE32+ optional header, and the new lookup and address
tables have 64-bit thunks, 8-byte aligned; for a PE32 image the output is byte for byte what it was. The Windows loader then loads the DLL with the client's other
dependencies and runs its DllMain before the client's entry point, so the hooks are in place before the
first static initialiser. A first version started the client suspended and loaded the DLL with a remote
thread (CreateRemoteThread on LoadLibraryA); Windows Defender's behaviour monitoring stopped that injector
(Behavior:Win32/DefenseEvasion.A!ml on build\capture\inject.exe, November 2, 2026, 13:20) and it was
deleted, not worked around.

## Making time equal

What the client draws depends on two clocks (from src/):

- **timeGetTime**: sysinc.cpp GetMilliseconds (StartTime is taken by a static initialiser) for creature
  steps (Creatures.cpp StepEnd), missiles and effects (Map.cpp), talk boxes (Talk.cpp UpdateTalkboxes),
  the blinking buttons (DialogElements.cpp TBlinkingButton) and skull blinking (Status.cpp
  UpdateBlinkState); the engines' frame timing (WaitForNextFrame); srand(timeGetTime()) (the only random
  numbers, Communication.cpp/Network.cpp: XTEA keys and padding). The wall clock (_time32: talk lines'
  time stamps, skull times) comes from GetSystemTimeAsFileTime.
- **ActionNumber**: the main frame's 100 ms timer (MainFrm.cpp `SetTimer(m_hWnd, 1, 100, NULL)`, OnTimer,
  Control.cpp TimerEvent) counts it; object and outfit animation phases (`GetActionNumber() / 5 % Phases`,
  `/ 3 %`), the edit line's cursor (`/ 5 % 2`), the console's tab blinking, mouse hover (ten ticks after
  the last move) and the ambient light (every tenth tick) follow it.

patcap.dll replaces both (import address table of the main module, so only the client's own calls):

- timeGetTime returns a virtual time: 100000 at the first call (StartTime), then it stands still and
  moves only when the script says so. GetSystemTimeAsFileTime (and GetSystemTimePreciseAsFileTime fetched
  through GetProcAddress) return 2026-11-02 12:00:00 UTC plus the same virtual time.
- The main timer is not created; SetTimer/KillTimer of id 1 only record it. Its ticks are WM_TIMER
  messages the DLL sends to the main frame (OnTimer, TimerEvent) when the script says so.
- GetCursorPos (OnIdle polls it for mouse moves) returns a fixed point outside the window (1020, 760).
- Commands come through a named file mapping (`Local\PatellaCapture-<pid>`) and are carried out in the
  main thread's PeekMessageA (the MFC idle loop), never inside a frame.

The script drives every step the same way in every run: the input (posted window messages, as in issues
#18-#21), then until the replay is idle, then **2000 ms and 20 ticks** (one tick and 100 ms at a time,
in one go), until the replay is idle again; the capture is the third frame shown after that. Packets and
input are handled while the clock stands still, so a creature step, a talk box or an animation phase
start at the same virtual time in both clients, and the advance brings them to the same point. Over a
session both clients get 60 s of virtual time and 600 ticks (windowed); srand's seed is the same, so the
clients even make the same XTEA keys (the replays' login and game keys are identical in all runs).

## The scenes

Windowed (the window of compare_traffic.py's Tibia.cfg, (10,10,800,600): a 780x557 client area), 26 scenes in
this order, each captured after its step and the advance:

| Scene | What is on the screen |
| :--- | :--- |
| start | the start screen (background picture, the five buttons) |
| options, graphics-options, advanced-options | the option dialogs over the start screen: the resolution list, the engine buttons with the current one selected (in the x64 client the graphics device choice, masked for vulkan and vulkan-cpu: notes/vulkan.md, "Checks", issue #31), check boxes, the frame rate slider |
| login | the login dialog with the account typed and the password as stars, the edit cursor |
| charlist | the character list from the replayed login server answer |
| game | the game view after entering: map, light, creatures with names and health bars, side bar (minimap, health/mana, inventory, combat controls, buttons), console with the "last visit" text |
| automap-zoom | the minimap zoomed in |
| walk north, walk east, walk south, walk west, turn south | the player after each step (and Cipfried, the rabbits, Patella Second as the server moved them) |
| say | the talk box "hello world" over the player, Cipfried's answer, the console lines |
| look at own field | the look text (the player) over the map and in the console ("look north" until issue #140, and in the frozen reference recordings) |
| open container, look in container, use in container | the bag window in the side bar, its apple, the apple eaten |
| context menu | the body slot's context menu ("Trade with ...") |
| trade with | the trade cursor state after choosing the entry |
| trade request, look at offer, cancel trade | the trade window with the own offer, the look text, the window closed |
| close container | the bag window closed |
| side windows | the Skills, Battle (Cipfried, Patella Second) and VIP windows in the side bar |
| logout | the character list again after Ctrl+L |

Fullscreen (1024x768): the same scenes except the three option dialogs (they are used to switch to fullscreen:
Options, Graphics, the Fullscreen check box, OK), 23 scenes; "start" is the first frame in fullscreen mode.

## Results

`python -I tools/compare_rendering.py` (out build\render\result, November 2, 2026, 13:48-14:13, 25 min 48 s): exit
code 0, **every scene of every engine and mode identical** between Tibia.exe and Patella.exe, outside one mask:

| Engine | Mode | API that showed the frames | Scenes | Patella.exe against Tibia.exe | Tibia.exe against its repeat |
| :--- | :--- | :--- | ---: | :--- | :--- |
| DirectX 5 (SurfaceDX.cpp) | windowed | DirectDraw Blt | 26 | identical (advanced-options: 37 pixels inside the mask) | identical |
| DirectX 9 (surfaceDX9.cpp) | windowed | Direct3D 9 Present | 26 | identical (advanced-options: 40 inside the mask) | identical |
| OpenGL (SurfaceGL.cpp) | windowed | OpenGL SwapBuffers | 26 | identical (advanced-options: 149 inside the mask) | identical |
| DirectX 5 | fullscreen 1024x768x32 | DirectDraw Flip | 23 | identical | identical |
| DirectX 9 | fullscreen | Direct3D 9 Present | 23 | identical | identical |
| OpenGL | fullscreen | OpenGL SwapBuffers | 23 | identical | identical |

Every run: 600 ticks and 60 s of virtual time windowed (500 and 50 s fullscreen), no proxy mismatch (every
client packet as in the recording), the display mode 1024x768x32 at 60 Hz before and after each run.

**The one accepted difference: the measured frame rate.** The advanced graphics options print "Current
framerate: %2.2f" (Dialogs.cpp TAdvancedGraphicsDialog::Draw, the engine's GetFPS): the frames the engine drew per
second of its clock (WaitForNextFrame). With the clock standing still between steps that is the number of frames
the machine drew in real time, divided by the 2 s of the advance, so it measures the machine and the build, not
what the client draws. Evidence:

- two runs of Tibia.exe differ: DirectX 5 showed 126.00 (try1), 125.50 and 126.00 (try2, the first run and its
  repeat: 75 pixels), 126.00 (try4), 125.50 (try5 and both runs of the final result);
- Patella.exe showed 128.50 once and 127.50 four times on DirectX 5, 134.50 against 132.50 on DirectX 9, 133.00
  against 131.50 on OpenGL: Patella draws about 1.5 % more frames in the same time, and over a whole session it
  draws as many more (DirectX 5 9,102 frames against Tibia.exe's 8,939 and 8,955; DirectX 9 9,572 against 9,362
  and 9,352; OpenGL 9,462 against 9,284 and 9,273): a faster build (MSVC 14 against 7.1), not other frame pacing;
- the frame limiter and the FPS count themselves are compared by the difftest with a scripted clock in all three
  engines (surfacedxtest.cpp's timer and clockwrap phases, surfacedx9test.cpp and surfacegltest.cpp's
  WaitForNextFrame), with no difference.

The mask covers the number only (100x16 pixels from SkinProperties.Style1394.Data70 + 112 of the dialog, the
place read from each client's memory; "Current framerate:" itself is compared); a differing pixel outside it fails
the comparison.

**Client bugs found**: none; no change to src/.

**Harness problems on the way** (no client difference): relative output paths (the DLL writes from the client's
folder); the two session steps run_session takes no snapshot of (context menu, trade with) are captured in its wait;
the side windows, opened and closed before the game steps, left the Battle window open once and the trade window
then pushed the bag window out of the side bar: they are now opened at the end. The injector stopped by Defender
(above).

**Not compared**: other fullscreen resolutions (a real display mode switch; the issue's "fullscreen" is covered in
the desktop's mode, the only one that changes nothing on this machine); the windows-drawn mouse pointer option
("WindowsMousepointer = no": the client then draws its own pointer; the session uses the Windows pointer, as
compare_traffic.py's Tibia.cfg); the frames between the steps (only the frame after each advance is captured).

**Side effects**: none on the server (it was not started: every game scene is a replay). The desktop's display
mode was the same before and after every run. Windows Defender recorded one detection (the deleted injector).
