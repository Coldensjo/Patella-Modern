# Rendering comparison (issue #20)

Patella.exe and the original Tibia.exe draw the same scenes with each graphics engine (DirectX 5:
SurfaceDX.cpp, DirectX 9: surfaceDX9.cpp, OpenGL: SurfaceGL.cpp), windowed and fullscreen; the frame
each client composes is read back from the engine's own surfaces just before it is shown and the
pictures are compared pixel by pixel. Choices and their reasons: notes/decisions.md, "Rendering
comparison (issue #20)". Builds on issue #19's replay (notes/traffic.md: tools/proxy.py, the session
driver, the memory reader) and the run folders of issues #18 and #21.

## Running it

```
python -I tools/build.py oracle-x86
python -I tools/compare_rendering.py [--out build\render\result] [--engines dx5,dx9,gl]
	[--modes windowed,fullscreen] [--no-repeat] [--recording build\render\session.jsonl] [--compare-only]
```

- No server: every game scene comes from a recorded session replayed by tools/proxy.py, so both clients
  get the same server bytes. The recording is issue #19's live session of Tibia.exe
  (compare_traffic.py's `orig-live.jsonl`: login, walking, talking, looking, a container, a trade with
  "Patella Second", logout); copy it to `build\render\session.jsonl` or pass `--recording`. A new one:
  `python -I tools/compare_traffic.py --out <dir>` (starts and stops the test server itself), then use
  `<dir>\orig-live.jsonl`.
- The capture helper `build\oracle-x86\Release\patcap.dll` is the patcap target of the oracle-x86 preset
  (built with the preset; the script builds the target itself when the DLL is missing or older than
  tools/capture/patcap.c; before issue #10 tools\build-capture.cmd built build\capture\patcap.dll).
- Run folders `build\render\orig` and `build\render\patella` (compare_traffic.py's set-up through its
  set_folder: copies of the data files, the clients patched by patch_client.py; then the import of
  patcap.dll, below). Tibia.cfg is compare_traffic.py's with `Engine = N` (and `FullscreenMode` for the
  fullscreen runs).
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
  mode before and after every run and stops when it changed.

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

The DLL patches the vtable entries when the client creates the objects (DirectDrawCreate and
IDirectDraw4::CreateSurface; GetProcAddress("Direct3DCreate9") and IDirect3D9::CreateDevice;
wglMakeCurrent tells which context belongs to the window's DC). The same code runs in both clients; it
sees only what the client passes to the API. A capture writes a 24-bit picture (16-bit formats would be
widened, the display is 32-bit); the script stores PNGs.

**Loading the DLL**: the run folders' copies of the clients get one more import descriptor (patcap.dll,
by name `PatcapLoaded`) in a new last section `.patcap` (compare_rendering.add_import: the old
descriptors unchanged, the data directory pointed at the new table, the bound import directory cleared;
it checks that no other byte changed). The Windows loader then loads the DLL with the client's other
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
| options, graphics-options, advanced-options | the option dialogs over the start screen: the resolution list, the engine buttons with the current one selected, check boxes, the frame rate slider |
| login | the login dialog with the account typed and the password as stars, the edit cursor |
| charlist | the character list from the replayed login server answer |
| game | the game view after entering: map, light, creatures with names and health bars, side bar (minimap, health/mana, inventory, combat controls, buttons), console with the "last visit" text |
| automap-zoom | the minimap zoomed in |
| walk north, walk east, walk south, walk west, turn south | the player after each step (and Cipfried, the rabbits, Patella Second as the server moved them) |
| say | the talk box "hello world" over the player, Cipfried's answer, the console lines |
| look north | the look text over the map and in the console |
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
