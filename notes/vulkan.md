# The Vulkan engine (Patella-Modern issue #19)

The Vulkan 1.3 engine of notes/modernization-plan.md, 6.2 (decision D5), in the Windows x64 client on the MFC
window. It draws what surfaceDX9.cpp draws, the reference engine of the frames (plan D7), call for call, and it
renders off screen with a read-back, which the frame dump of the test mode (issue #22) will use. The choices and
their reasons: notes/decisions.md, "The Vulkan engine (Patella-Modern issue #19)".

## Files

| File | What |
| :--- | :--- |
| `src/vulkan/SurfaceVulkan.h`, `.cpp` | `TSurfaceVulkan`, the `TSurface` of the client: surfaceDX9.cpp's logic with the Direct3D 9 calls replaced; the device request; the Win32 surface; the display modes |
| `src/vulkan/DeviceSettings.h`, `.cpp` | the client's graphics device choice (issue #31): `PATELLA_VULKAN`, Patella.ini's `Device`, the GPU list, the CPU driver's manifest |
| `src/vulkan/DeviceChoice.h`, `.cpp` | the choice's words (`auto`, `gpu`, `gpuN`, `cpu`) and what each button chooses; no Vulkan (tests_render on every preset) |
| `src/settings/Settings.h`, `.cpp` | Patella.ini's parser and writer (D12), in patella_core (tests_core on every preset) |
| `src/vulkan/PresentWindow.h`, `.cpp` | the windows the devices present in (issue #52): one child window of the main window per device, retired with its device, destroyed once the next device has shown a frame; the device test's hidden window; the main window's erase, which fills what they do not cover black (issue #71) |
| `src/vulkan/Renderer.h`, `.cpp` | `patella::vulkan::TRenderer`, the back end: no window system, no MFC (portable C++23 with volk, vk-bootstrap and VMA) |
| `src/vulkan/Quads.h`, `.cpp` | how Direct3D 9's quads become Vulkan vertices; no Vulkan at all (tests_render on every preset) |
| `src/vulkan/FilterMask.h`, `.cpp` | `TFilterMasks`: which pixels come from the device's filter (the comparison's tolerance); no Vulkan (tests_render) |
| `src/vulkan/Vma.cpp` | VMA's implementation |
| `src/vulkan/shaders/*` | `Quad.vert` (every pipeline), `Quad.frag` (texture times vertex colour), `Fill.frag`, `ColorKey.frag`, `Constants.glsl` (the push constants) |
| `cmake/Vulkan.cmake` | the pinned dependencies, the shaders (glslang), `patella_vulkan_deps`, `patella_render_vulkan`, the CPU driver, tests/vulkan |
| `tests/vulkan/render_test.cpp` | `tests_vulkan`: the back end headless against a model of Direct3D 9's pixels |
| `tests/unit/render_quads_test.cpp`, `render_filtermask_test.cpp`, `render_device_choice_test.cpp` | `tests_render`: Quads.cpp, FilterMask.cpp and DeviceChoice.cpp |
| `tests/unit/core_settings_test.cpp` | `tests_core`: Patella.ini's parser and writer |

None of it was in the 32-bit oracle build (until issue #94): its glob was `src/*.cpp`, and the only changes to reconstructed files are
the x64 client's own GetSurface and SurfaceInitInstance (Surface.cpp), ChangeEngine (Control.cpp) and the advanced
graphics options' device choice (Dialogs.cpp, Dialogs.h: TAdvancedGraphicsDialog), behind
`#if defined(PATELLA_VULKAN)`, which only the x64 client defines (pecompare: the oracle's Patella.exe is
unchanged).

## Choosing the engine

Since issue #20 **the Vulkan engine is the Windows x64 client's only engine**: SurfaceDX.cpp (DirectDraw),
surfaceDX9.cpp (Direct3D 9) and SurfaceGL.cpp (OpenGL) are not compiled into it, and it imports neither ddraw.dll
nor opengl32.dll nor glu32.dll (nor loads d3d9.dll). The oracle kept the three until M6; they left the fork with it (issue #94). The x64 client
has its own GetSurface and SurfaceInitInstance (Surface.cpp) and ChangeEngine (Control.cpp):

- **curEngine is always 2**, the number of the original's DirectX 9, the engine whose frames the Vulkan engine
  reproduces: everything that asks for the engine (graphics.cpp's colour keys, the FPS line of Ctrl+F, which says
  "DX9", error.txt's "Graphic Engine: 2") behaves as the original's DirectX 9.
- **Tibia.cfg's `Engine` keeps the original's number for Tibia.exe** (plan 6.3: "Tibia.cfg keeps an engine number
  in the original's range"). A number 0-2 from Tibia.cfg, or the command line's `engine N` (0-2), is stored as it
  is; none (-1) or one out of range becomes 2. That is what Tibia.exe writes on a machine where its engines work
  (this one: DirectX 9 and OpenGL), so compare_files.py sees the same Tibia.cfg.
- **The advanced graphics options' three engine buttons choose the device** (issue #31, below): they keep the
  original's places, events and Tibia.cfg numbers (DirectX5's place 0, DirectX9's 2, OpenGL's 1).
- **Without a Vulkan loader** (vulkan-1.dll, which every GPU driver with Vulkan installs) or any device there is
  no engine to fall back to: the start fails with PROGRAM_ERROR 12 ("Video surface allocation failed"), as the
  original's when its device could not be created.

## Choosing the device (issue #31)

Plan 6.3 with the user's decisions of November 3, 2026: in the x64 client the advanced graphics options' three
engine buttons are a **graphics device choice**, the choice is stored in **Patella.ini** (D12), and it is
applied at once. A recorded exception to tier A (plan 3, 6.3): the dialog looks different from Tibia.exe's
there, nothing else does.

**The dialog** (Dialogs.cpp TAdvancedGraphicsDialog, x64 branches). The skin's three button places (58x20
pixels, skin 16) hold, from the left:

| Place (original) | Label | Help text | Chooses | Tibia.cfg `Engine` | Enabled |
| :--- | :--- | :--- | :--- | :--- | :--- |
| DirectX5 (event 800) | Automatic | Use the best graphics device: a GPU, else the CPU | `auto` | 0 | always |
| DirectX9 (event 801) | GPU | Use the graphics card (GPU); with several, each click takes the next | `gpu`, then `gpu1`, `gpu0` ... | 2 | with a GPU |
| OpenGL (event 802) | CPU | CPU (compatibility): draw without the graphics card | `cpu` | 1 | with the CPU driver next to the executable |

"CPU (compatibility)" (87 pixels in the buttons' font) does not fit the 58-pixel button, so the button says "CPU"
and its help text the rest. The line above the buttons (the original's "Select graphics engine:", now as wide as
the dialog) names the device in use, `Device: NVIDIA GeForce RTX 5080` (exactly the 200 pixels here); a longer
name is shortened with "..." and shown whole as the line's help text; before a device exists it says "Select
graphics device:". The button of the current choice is selected. In fullscreen mode the three are disabled, as
the original's engine buttons. **GPU** chooses the best GPU first (the one Automatic takes); on a machine with
several GPUs every further click takes the next one in the list, round; with one GPU a further click changes
nothing. This machine (November 3, 2026) has two: gpu0 NVIDIA GeForce RTX 5080 (discrete, the best) and gpu1 AMD
Radeon(TM) Graphics (integrated).

**Applying** (Control.cpp ChangeEngine, x64 branch): a click that changes the choice is applied as the original's
engine switch: the surface's device is released (ExitSurfaces, SurfaceExitInstance), the new device tested on a
new surface (TestEngine) and the surfaces created on it at the next CheckSurface (the video cache is rebuilt
there, as after the original's switch). When the test fails, the old choice stays, as the original keeps the old
engine. A click that changes nothing (Automatic again, GPU with one GPU) only stores the number.

**Tibia.cfg**: every button stores the engine number of its place (0, 2, 1), as the original's button there
stores it after a switch, so Tibia.cfg stays what Tibia.exe writes after the same clicks on a machine where its
engines work; compare_files.py's options session clicks the third place (OpenGL's, now CPU), and both clients
write `Engine = 1`. A failed switch stores nothing, as the original's. Tibia.cfg's `Engine` does not choose the
device; at start-up it is kept as #20 keeps it (0-2 as it is, else 2).

**Patella.ini** (src/settings/Settings.h): the fork's own settings file, in the working directory next to
Tibia.cfg, plain `key=value`, a parser of our own. Only one key so far:

```
# Patella-Modern's own settings (Tibia.cfg stays the original client's)
Device=gpu1
```

`Device` is `auto`, `gpu` (the best GPU), `gpuN` (the n-th GPU from 0 in the list below; `gpu:N` is read too) or
`cpu`, without regard to case; anything else (or no file) is `auto`. The client reads it once at start-up
(SurfaceInitInstance) and writes it when a click changes the choice and the new device works: the key's line is
replaced or appended, every other line (comments, blank lines, unknown keys, lines without `=`) and the file's
line ends stay as they are; a new file gets the comment line above and CRLF, as Tibia.cfg. Keys are compared
without regard to ASCII case, the first line with a key counts. Patella.ini is never compared with Tibia.exe's
files (Tibia.exe has none). The other D12 keys (servers, scale, vsync, data folder, log level) come with their
issues.

**The GPU list** (TRenderer's `ListGpus`, read once at start-up): the system's Vulkan devices that are not CPUs and
have Vulkan 1.3 with dynamic rendering and synchronization2 and VK_KHR_swapchain, in the loader's order
(vk-bootstrap's select_devices), from an instance of its own without a window. `gpuN` counts in that list, so the
same index means the same GPU with or without a window; whether it can present to the window is checked when its
device is created (otherwise the switch or the start-up test fails). "The best GPU" is the first of the best kind
(discrete, integrated, virtual, other), as before.

**Every instance enables the same instance extensions** (issue-31-fix; Renderer.cpp NewInstanceBuilder):
VK_KHR_surface, VK_KHR_win32_surface and VK_EXT_debug_utils where the loader offers them, with or without a
window. vk-bootstrap loads the instance functions it calls itself once per process, from the first instance it
builds; the GPU list's headless instance, first at start-up since issue #31, had left its surface functions null
and the window's device selection crashed (0xC0000409 at start-up, on main 26ac7b7). tests_vulkan's test "a device
for a window after headless instances" covers that order with a window that is never shown.

**At start-up**, when the chosen device fails the engine test (a GPU that is gone, an index out of range, the CPU
driver missing), the client runs on the automatic choice, as the original falls back to another engine;
Patella.ini keeps the choice (the dialog shows Automatic). The command line's `engine N` skips the test, as in the
original.

**`PATELLA_VULKAN`** stays, as an **override of Patella.ini for the comparisons and the tests**: when it is set and
not empty, its value (the same words; one it does not know, such as `off`, is `auto`) is the choice at start-up
instead of Patella.ini's, and it is never written. A click in the dialog still switches and writes Patella.ini.
compare_rendering.py (`gpu`, `cpu`) and testmode_check.py (`cpu`) set it; compare_files.py and
compare_traffic.py do not, so their clients start with Patella.ini's choice (none in their clean run folders:
`auto`).

| `PATELLA_VULKAN` (else Patella.ini's `Device`) | Device |
| :--- | :--- |
| unset or empty: Patella.ini's `Device`; no file or no key: `auto` | |
| `auto` (and any value not below) | the best GPU with Vulkan 1.3 (discrete, integrated, virtual, other), else the CPU driver |
| `gpu`, `gpu0`, `gpu1` ... (`gpu:1`) | a GPU only: the best one, or the n-th of the GPU list |
| `cpu` | the CPU driver only |

`PATELLA_VULKAN_VALIDATION=1` asks for the validation layers (they come with the LunarG Vulkan SDK, which this
machine does not have; without them the request is ignored).

## One window per device (issue #52)

**What happened.** The user, in game with the advanced options open, pressed Automatic, GPU and CPU quickly, 5 to
10 times (November 4, 2026; `build\manual-test`, main f2b3374): the picture froze for good, the window's X did
nothing. A full dump (Task Manager) and a live inspection showed the client alive and well: the main thread drawing
about 1,000 frames a second (stack samples in different drawing functions, user time growing about 1 s per second),
pumping its messages (IsHungAppWindow 0), one TSurfaceVulkan with one renderer on the RTX 5080, one swapchain of the
window's client size (IMMEDIATE, 3 images, not stale), every `TRenderer::Present` returning true. Yet the screen
showed an old frame: resizing the window stretched that one picture, even after the resize had recreated the
swapchain. The frozen picture was the last frame of the AMD Radeon iGPU ("Device: AMD Radeon(TM) Graphics", "Current
framerate: 157.98"), whose swapchain presents cross-adapter here (the machine's one monitor, 5120x1440, hangs on the
RTX; the Radeon's frames go through NVIDIA's D3D driver, `nvwgf2umx.dll`, 31 threads while it runs). The X: in game
with a dialog open, CloseRequest (Control.cpp) asks nothing and refuses, as the original does; Task Manager's End
task then kills the process, which Windows reports as "Application Hang" (the AppHangB1 reports of that night).

**The cause.** After the Radeon's device, swapchain, surface and instance were destroyed (in that order, after
vkDeviceWaitIdle; nothing of them was left in the process, and no other presentation object: no second swapchain,
no DirectComposition target, the driver threads as after a healthy device), its presentation stayed bound to the main
window outside the process, and the compositor kept showing it over every later device's swapchain on the same
window. Each device presented into the main window, so one device's leftover could cover all the others.

**The change.** Each device the engine creates presents into a child window of its own (src/vulkan/PresentWindow.h,
`TPresentWindows`), never into the main window:

- CreateSurfaces opens a new window for the device, over the main window's client area and above the retired one;
  it paints nothing and lets every mouse message through to the main window (WM_NCHITTEST: HTTRANSPARENT), so input,
  the cursor and the client's own mouse pointer behave as before.
- FinishFrame fits the window to the main window's client area (the size the main window's surface had) before each
  Present; when a Present shows the frame, the window of the device before it is destroyed.
- When the device goes (ExitDisplay: a switch, a change of the view mode, the end; a failed CreateSurfaces), the
  device is destroyed first, then its window retired: kept, still showing the device's last frame, until the next
  device's first frame, or destroyed at once when it never showed one. A retired window that showed a frame is kept
  instead of one that did not; at most two device windows exist.
- TestEngine creates its test device on a hidden child window of its own (`TTestWindow`), destroyed with the test.

Destroying a window releases whatever the system or a driver keeps bound to it for presentation, so no device's
leftover can outlive the next device's first frame, whichever driver keeps it. The windows outlive the surface object
(SurfaceVulkan.cpp keeps them, as surfaceDX9.cpp's statics). Nothing else changes: the frames, the files and the
packets are the same; on screen, a switch now keeps the old picture until the new device's first frame.

**Checked.** tests_vulkan's "quick device switches" runs the client's sequence (ExitDisplay, TestEngine,
CreateSurfaces, frames) through TPresentWindows on every device of the machine (Automatic, gpu0 RTX 5080, gpu1
Radeon, the CPU driver), 48 switches in a fixed order, 21 of them before the switch's device existed, with the main
window resized between frames: no device ever has a surface on the main window, every device presents in a new
window, the test devices' windows are hidden and gone afterwards, an old device's window is gone with the next
device's first frame, the windows follow the main window's size and let hits through (969 assertions, about 18 s).
The windows are never shown, so the test cannot show the stale picture itself: a hidden window is not composed. The
live check is the user's repro.

**Input** (the reference compare after the change, November 4, 2026: in the session `traffic` the scripted click
on "Graphics" was lost, the state dumps 3 and 4 still on the Options dialog, the rest equal). The device windows do
not take input: the test mode dispatches its clicks to the main window itself (no hit test), real clicks pass
through them (HTTRANSPARENT), and tests_vulkan's "scripted clicks reach the main window while device windows come
and go" holds a click (dispatched as the test mode does, the window taking the capture as CMainFrame does) across
everything the engine does with its windows (a device and its window, frames on the CPU driver and on the GPU, a
resize, a device switch with the old window destroyed, the device test's window): no WM_CAPTURECHANGED, the
capture still the main window's at the release. A click is lost only when the main window loses the capture
between press and release, which the desktop does (another window activated, the user's own clicks; the run before
it had met the user's own client still running). With a script the test mode now ignores that loss
(notes/testmode.md, PATELLA_TEST_CAPTURE_CHANGED), as it drops the desktop's queued input.

**Found on the way.** Each device on the CPU driver leaves about 25 process handles, a switch to it twice that
(the device test and the device): lavapipe's own, see "Handles the drivers leave (issue #53)" below.

## The main window's background during a live resize (issue #71)

**What happened.** The owner (November 5, 2026, build\manual-test\Patella.exe on the intro screen): while the window's
border is dragged larger, the newly uncovered client area shows stretched stripes of old pixels, the right and bottom
edges smeared outwards; when the mouse is released the picture is whole again. Since the x64 client exists.

**The cause.** While the border is held, Windows runs its modal size loop: the client's message loop, and with it
OnIdle, which draws the frames, does not run (only WM_TIMER's TimerEvent, which draws nothing). The device's window
keeps its size until the next frame's Fit (FinishFrame), so what a larger window uncovers is the main window's own
client area, which nothing paints: its class has no background brush (Player.cpp InitInstance) and CMainFrame
handles neither WM_ERASEBKGND nor WM_PAINT, as the original's. The compositor shows what is left in the window's
surface: each size step's edge, the stripes.

**Tibia.exe** shows the same stripes in the same situation (its DirectX 9 and OpenGL engines, a live drag of the
bottom-right border with simulated mouse input, November 5, 2026): the same class, the same message map, no frame
during the loop. notes/decisions.md, "The main window's background during a live resize", has the run.

**The change.** TPresentWindows subclasses the main window (SetWindowSubclass) from its first Open until Close or
the main window's WM_NCDESTROY. Its WM_ERASEBKGND fills the client area outside the device windows (the current one
and a retired one still showing its last frame; their own WS_VISIBLE) black and returns 1; every other message goes
on to MFC. Each size step's invalidation (CS_HREDRAW, CS_VREDRAW) brings the erase, and the modal loop dispatches
WM_PAINT, so the uncovered area is black during the drag; the device window keeps its size and its last picture
until the first frame after the release fits it again. A deliberate on-screen difference from Tibia.exe (black
instead of stripes). The frames (the engine's images, read back for the dumps and the reference compare), the files
and the packets do not change; the reconstructed files and the oracle are untouched.

**Checked.** tests_vulkan's "the main window's area the device windows do not cover is filled black (issue #71)",
with no device: a hidden main window of the client's class style, WM_ERASEBKGND sent with a memory DC of white
pixels; the class's own erase before the first Open and after Close (nothing painted); the uncovered part black and
the device window's part untouched through eight size steps without a Fit, after the Fit, with a retired window
and the next one, and with no device window (all black); the subclass gone with the main window and taken by the
next one. Live: main's and the branch's Patella.exe on the intro screen, the border dragged by simulated mouse
input (build\logs\issue-71\scripts
esize_repro.py, local): stripes before, black after, on the CPU driver and on
the GPU.

## How it works

**The surfaces** are images (B8G8R8A8_UNORM, the memory layout of D3DCOLOR, all in the GENERAL layout), with
surfaceDX9.cpp's sizes: PRIMARY_SURFACE and TEMP_SURFACE 640x480, STRETCH_SURFACE 64x64, CACHE_SURFACE 512x384,
AUTOMAP_SURFACE 106x106, the 16 video cache surfaces 512x512, SCREEN_SURFACE's 106x106 texture, and SCREEN_SURFACE
itself, the canvas: Direct3D 9's back buffer, the screen's size in windowed mode (GetSystemMetrics; grown when the
client area is larger), the mode's size in fullscreen mode. New images are cleared to 0; PRIMARY_SURFACE is filled
red, as surfaceDX9.cpp's ColorFill does.

**Drawing** follows surfaceDX9.cpp exactly: quads are queued (`AddQuad`) and drawn when the texture or the
target changes, when 9,600 vertices are queued and at the end of the frame; the sampler state (linear, point for
the automap's upload) applies when they are drawn, as in Direct3D 9. The state surfaceDX9.cpp keeps in file
statics (the current target and texture numbers, the colour keys, the queued quads, the frame counter) is kept in
file statics, so it outlives the surface object as there. Fills, lines and pixels are Direct3D 9's ColorFill:
`TRenderer::Fill`, a quad without blending; a rectangle that does not lie inside the surface is refused, as
Direct3D 9 refuses it. Uploads (`WriteImage`, `WriteMaskedImage`, Direct3D 9's LockRect) go through a staging
buffer. `ApplyColorKey` (Direct3D 9: GetRenderTargetData, a loop on the CPU, UpdateSurface) is a pass on the GPU:
the 64x64 corner is copied to a scratch image and drawn back with every pixel that is exactly the key (alpha
included) replaced by transparent white. The light (`ShadeRectangles`) is surfaceDX9.cpp's: TEMP_SURFACE copied
to PRIMARY_SURFACE, then quads with the corner colours into TEMP_SURFACE, the texture times the colour, blended
with the source alpha.

**The back end** records one command buffer per frame, two frames in flight. A target is drawn in a dynamic
rendering pass (Vulkan 1.3: no render passes, no framebuffers); a pass ends when the target changes or a
transfer (an upload, a copy, a read-back) comes, with one global synchronization2 barrier between the steps. A
draw that samples its own target reads a copy taken before it. Three pipelines: the textured quad (source-alpha
blending, D3DRS_SRCBLEND/D3DRS_DESTBLEND, for alpha too; a texel of alpha 0 is discarded, which leaves the same
pixel as blending it), the fill and the colour key pass (no blending). Vertices and staging data live in
host-visible buffers of the frame (VMA).

**Presenting**: the rectangle of SCREEN_SURFACE that surfaceDX9.cpp presents (`Present(&Rect[SCREEN_SURFACE],
...)`) is blitted into a swapchain image of the client area's size (VK_KHR_win32_surface on the MFC window) and
presented. The present mode is IMMEDIATE (Direct3D 9: D3DPRESENT_INTERVAL_IMMEDIATE; the frame limiter is the
client's), MAILBOX or FIFO where IMMEDIATE is missing; vsync by default (D13) comes with the settings file. The
swapchain is created again when the client area's size changes or Vulkan reports it out of date; a minimised
window shows nothing. Fullscreen mode sets the display mode with ChangeDisplaySettings (Direct3D 9's fullscreen
device does that itself) and sets it back when the device goes; the window is the client's topmost popup
(MainFrm.cpp SetDisplayMode).

**Display modes** (issue #20): surfaceDX9.cpp listed them with DirectDraw 4 (`EnumDisplayModes(0)`, no refresh
rates) and its callback (at least 640x480 and 16 bits; the frequency always 0, as the callback tests the refresh
rate's flag among the pixel format's flags). The engine lists EnumDisplaySettings' modes in their order, each
width, height and depth once, with the same filter and frequency 0: on this machine (RTX 5080, November 3, 2026)
both lists are the same 24 modes in the same order (a probe that asks both, build\logs\issue-20\display-modes-probe.log),
so the graphics options show the same list and compare_files.py's next resolution is the same. The desktop's depth
check (DirectDraw's GetDisplayMode, PROGRAM_ERROR 8 below 16 bits) is EnumDisplaySettings' current mode.
CanStretch (DirectDraw's four stretch and shrink capabilities, which set the default of "Don't stretch/shrink
the Map") is true: Vulkan stretches on every device, and DirectDraw reports all four here too.

**Read-back**: `TRenderer::ReadBack(image, rectangle)` submits what is recorded, waits, and copies the pixels
(D3DCOLOR, rows top-down); `TSurfaceVulkan::ReadScreen` reads the rectangle the next frame shows. That is the
frame dump's source for issue #22 (plan 4.4), on any device, without a window system.

## Pixels

Direct3D 9 puts pixel centres on whole coordinates, the hardware (and Vulkan) at + 0.5; a Direct3D 9 driver moves
every vertex half a pixel. surfaceDX9.cpp's quads have whole corners and texture coordinates `(x + 0.5) / width`,
the texel centres. Quads.cpp turns a quad into vertices in one of three ways:

- **A copy** (one colour, texels one to one onto pixels: every unscaled blit): the corners stay on whole pixels,
  so no pixel centre lies on an edge, and the coordinates put the texel edges on the pixel edges, so nearest
  sampling reads the texel centres: the exact texels on every device, whatever its filter rounds. Linear
  filtering at a texel centre reads only that texel too, so this is what Direct3D 9 computes.
- **An Edge copy**: point sampling whose samples fall on the texel edges, from edge 0. The one case is the
  automap's upload (SurfaceVulkan.cpp WriteImage: ScreenTexture's 106x106 drawn to AUTOMAP_SURFACE with u and v
  from 0 to 1). The interpolated coordinate lands a hair below each edge except at the corner, so Direct3D 9 on
  the NVIDIA GPU shows texel max(k - 1, 0) at pixel k: issue #19's live comparison showed the minimap one pixel
  off against Tibia.exe until this rule was in (up to 255 per channel in every game scene). tests_vulkan shows
  the same mapping on that GPU through Vulkan, and lavapipe rounding to texel k; the engine gives every device
  the rule as a copy (texel k - 1 through a nearest, clamp-to-edge sampler). Samples on the edges from another
  start go to the hardware as below.
- **Everything else** (a stretch, the light's coloured corners, a coordinate between texels) goes to the hardware
  as a Direct3D 9 driver gives it: the corners half a pixel on, the attributes as they are, the linear or point
  sampler with wrapping (D3DTADDRESS_WRAP). The hardware then covers the same pixels (the top-left rule on the
  edges), interpolates the same values and filters them with its own bilinear filter.

So the pixels the client copies, fills, uploads and colour-keys are exactly what Direct3D 9 gives on every
device. Those of **the light** (plan 6.1) and of the **bilinear stretches** depend on the device's filter and
rounding: the start screen's picture (640x480 stretched over the window, MainWindow.cpp TStartWindow::Draw: the
start, login and option dialog scenes), the map view (480x352 stretched to the map window, MapWindow.cpp, unless
"no stretching" is set: the game scenes) and the scaled item pictures (graphics.cpp StretchBlit). Direct3D 9
filters them with the GPU too, so Tibia.exe's own frames depend on the graphics card there. The live comparison
(November 3, 2026): on the RTX 5080 they are identical to Tibia.exe's DirectX 9 frames (the engine gives the
hardware what a Direct3D 9 driver gives it); on the CPU driver they differ by at most 2 per channel (mean about
0.35).

**The tolerance (the user's decision of November 3, 2026):** up to 2 per colour channel on the pixels whose value
comes from the device's filter or interpolation (the bilinear stretches and the light), exact everywhere else, on
every device. The engine knows those pixels: while a frame observer (the test build's frame dump) watches, `TFilterMasks`
(FilterMask.cpp) follows its fills, uploads and quads with a byte per pixel of every image, "filtered" and
"transparent" (alpha 0). Uploads and fills set it exactly; a copy carries the source's bytes over, texel for texel
as the engine samples it, except through transparent texels (which leave the target's pixel); every other quad
marks every pixel it covers. So a glyph over the stretched start screen is exact where its texels are opaque, and
the lit map copied unscaled to the screen ("no stretching") keeps its marks. The captured frame's mask goes to
the frame dump (`frame-<NNN>.filtered.png`, below); compare_rendering.py applies `--region-tolerance` (default 2) on it only. One case is wider
than needed: a scaled item picture's colour-keyed texels (ApplyColorKey's pixels are only known on the GPU) count
as filtered where they leave the target unchanged.

What the engine does not reproduce of surfaceDX9.cpp: quads still queued when the device is released (a switch
between window and fullscreen) are dropped, where Direct3D 9 would draw them from a new, undefined vertex buffer;
a new device samples opaque white until a texture is set (Direct3D 9's stage without a texture; not checked
against a real Direct3D 9 device); the canvas grows with a client area larger than the screen, where Direct3D 9's
Present would fail; an index out of range is ignored instead of the original's assertion (no TASSERT in new
code: notes/decisions.md).

## The CPU driver

Mesa 26.2.5's **lavapipe** (the `llvmpipe` device, Vulkan 1.4), the MSVC build of the mesa-dist-win project:
`vulkan_lvp.dll` (56 MB, only system DLLs as dependencies) and its manifest `lvp_icd.x86_64.json`, taken from the
release archive at configure time and copied next to Patella.exe, PatellaTest.exe and tests_vulkan.exe. The engine
loads it only through the loader's driver setting of its own process: `VK_DRIVER_FILES` (and `VK_ICD_FILENAMES`
for loaders before 1.3.207) set to the manifest next to the executable just for the creation of the instance and
device, then set back. Nothing is installed or registered; the system's Vulkan configuration and the registry
are never touched. The loader ignores these variables in an elevated process: then the CPU driver is not found
and the engine says so ("an elevated process ignores VK_DRIVER_FILES"). `auto` asks the system's drivers first and
loads the CPU driver only when they offer no Vulkan 1.3 GPU. Why lavapipe and not SwiftShader: notes/decisions.md.
Mesa is MIT-licensed; a package (M9) must carry Mesa's licence text next to the DLL.

The engine needs the Vulkan loader (vulkan-1.dll, which every GPU driver with Vulkan installs). A machine
without any Vulkan driver has no loader; the plan names no loader to ship (Vulkan-Loader is not among its
dependencies), so such a machine falls back to the original engines today (notes/decisions.md, "Found, not
done").

## Handles the drivers leave (issue #53)

Found during #52: in a headless harness the process's handle count grew by about 56 per RTX, CPU, RTX round. None
of the handles is ours; the engine destroys everything it creates (Renderer.cpp `TState::Destroy`: every image,
buffer, fence, semaphore, command pool, pipeline, sampler, descriptor object, the allocator, the swapchain, the
device, the surface and the instance; no pipeline cache, no debug messenger without validation, no thread of its
own), and the loader unloads vulkan_lvp.dll with each instance. What stays, by object type (tests_vulkan's handle
census, ntdll's handle list of the process; November 4, 2026, this machine: 24 logical processors):

| Per | Handles | Whose | Evidence |
| :--- | :--- | :--- | :--- |
| CPU driver device | 24 Thread, of ended threads | lavapipe's rasterizer threads | all 24 started in vulkan_lvp.dll (one start address, the image's c11 thread routine, the DLL unloaded afterwards); `LP_NUM_THREADS=4` makes it 4 |
| CPU driver instance | 1 Thread, of the calling thread | lavapipe's `thrd_current` | also when only listing the device (CpuDriverDevice), no device created |
| NVIDIA device | 3 Event | NVIDIA's driver | the same code on the AMD Radeon and on lavapipe leaves none; the same with the implicit layers (OBS, Steam, EOS) disabled (`VK_LOADER_LAYERS_DISABLE=~implicit~`) |

So a switch to the CPU driver (TestEngine's device, then the surfaces' device: two instances, two devices) leaves
50 handles, a switch to the RTX 5080 6: #52's 56 per round. In Mesa 26.2.5's source: `lp_rast_destroy`
(src/gallium/drivers/llvmpipe/lp_rast.c, lines 1334-1350) waits for each rasterizer thread to end on Windows but,
unlike the other platforms, never calls `thrd_join`, the only place its handle (from `_beginthreadex`) would be
closed (src/c11/impl/threads_win32.c, `thrd_join`, lines 350-369); their number is the screen's `num_threads`
(lp_screen.c, lines 1072-1076: the logical processors, or `LP_NUM_THREADS`, at most `LP_MAX_THREADS`, 32). Of the
49 threads a device starts here, the other 25 (the compute pool's 24 and one more) end with their handles closed. `thrd_current` (threads_win32.c, lines 294-324) duplicates a handle of
the calling thread into lavapipe's thread-local state, closed only by the DLL's TLS callback when that thread ends
(lines 256-264), which never runs once the DLL is unloaded.

What it costs: a handle and an ended thread's kernel object each, a few KB; 1,000 switches to the CPU driver leave
about 50,000 handles, far below the per-process limit (about 16 million). Not worked around (notes/decisions.md,
"Handles the drivers leave (Patella-Modern issue #53)"): the driver is pinned and not patched; a fix belongs
upstream. tests_vulkan's "switching to and from the CPU driver leaves no handles of ours" counts the drivers'
handles apart (their type, their start in vulkan_lvp.dll, their bounds) and requires the rest flat.

## Dependencies

FetchContent with URL and SHA256 (cmake/Vulkan.cmake), downloaded on the first configure of a build tree (about
85 MB, most of it Mesa's archive) into build\<preset>\_deps. The sources are compiled into `patella_vulkan_deps`
with the static CRT and without the product's warning settings (`/W0`; they are not ours); glslang is built with
its own CMakeLists, only its executable, warnings off.

| Library | Version | URL | SHA256 |
| :--- | :--- | :--- | :--- |
| Vulkan-Headers | vulkan-sdk-1.4.363.0 | https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz | 4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e |
| volk | vulkan-sdk-1.4.363.0 | https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz | 1547d8d74395d4048fb3f4a6313da56db626e89b55db238d0d7e8944c8a645f3 |
| VMA | v3.5.0 | https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.5.0.tar.gz | c5f3db848fca9cb9726b41f256cc3007773321fa92a86d1e0902e66474e8a9e3 |
| vk-bootstrap | v1.4.363 | https://github.com/charles-lunarg/vk-bootstrap/archive/refs/tags/v1.4.363.tar.gz | 11927b43a5c5d64646982b273d6436d2243acaa2da8834f7ba382060431155fe |
| glslang | 16.6.0 | https://github.com/KhronosGroup/glslang/archive/refs/tags/16.6.0.tar.gz | 9c09b901149c729df745057dafa815278aaa101b84d2b6e14f16a42de52f97f2 |
| Mesa lavapipe (mesa-dist-win) | 26.2.5, release-msvc | https://github.com/pal1000/mesa-dist-win/releases/download/26.2.5/mesa3d-26.2.5-release-msvc.7z | 997123ef4ce8896204ab912d5b54f19372c487ff9835bcefafa2fca23b8953c2 |

The Mesa archive's hash is the one GitHub publishes for the release asset; the others were computed on November
3, 2026 from the downloads. An update: new URL and hash in cmake/Vulkan.cmake and here, the stamp name of the
extracted CPU driver (`extracted-<first hash digits>`) with it, and tests_vulkan again.

## Capture for compare_rendering.py

A Vulkan swapchain image cannot be read back from outside the engine (an injected DLL would need the engine's
device, queue and a copy usage of the swapchain), so the engine offers its frames to a frame observer of its own:
a device call (name, type, cpu) once it has a device, a frame call (width, height) before every frame is shown
(the observer answers whether it wants it), and, when wanted, the mask (the frame's device-filtered pixels, a
byte each) and the pixels with SCREEN_SURFACE's rectangle read back, the picture Present shows. The only
observer is the test build's frame dump (src/TestHooks.h, PATELLA_TEST_FRAME_OBSERVERS; notes/testmode.md, "The
frame dump"), which compare_rendering.py reads through the control channel (issue #92). Without an observer the
engine calls nothing and keeps no masks; Patella.exe has none.

Until issue #97 the engine also looked up patcap.dll once per device (GetModuleHandle "patcap.dll") and its
exports PatcapVulkanDevice, PatcapVulkanFrame, PatcapVulkanMask and PatcapVulkanPixels (shared memory version 3),
for the 64-bit patcap.dll that compare_rendering.py loaded into the x64 client until issue #92. tools/capture/patcap.c
is Tibia.exe's only since then (capture-x86, a 32-bit DLL; it hooks the original engines: DirectDrawCreate,
Direct3DCreate9, wglMakeCurrent, SwapBuffers); its Vulkan exports have no caller.

## Checks

- `tests_render` (every product preset, tests/unit): Quads.cpp, FilterMask.cpp and DeviceChoice.cpp, 14 test
  cases, 359 assertions (12 and 289 before issue #31): when a quad is a copy or an Edge copy, the texel-centred
  coordinates at Vulkan's pixel centres, Direct3D 9's coordinates and colours at the pixel centres for the rest;
  the masks of fills, uploads, stretches, copies through transparent texels, the light, the automap's Edge copy
  and a self-draw; the device choice's words (every form, the refused ones, the round trip) and what each button
  chooses (the GPU button's round through several GPUs, from the best, an index beyond the list).
- `tests_core` (every product preset): Patella.ini's parser and writer (src/settings/Settings.cpp, issue #31),
  8 of its 25 test cases: keys without regard to case, values trimmed, `=` in a value, the first line counting, the
  round trip with unknown keys, comments and blank lines kept, CRLF and LF line ends (from the first line; a
  stray CR; no end at the end; an empty file), lines that are no `key=value` kept and never read, a byte order
  mark, the keys and values Set refuses, Load and Save (a missing file, a file too large, a folder that does not
  exist).
- `tests_vulkan` (windows-x64 with the client; CTest, exit 77 = skipped without a loader): the back end
  headless, never a window, on the CPU driver next to it and on the machine's best GPU; and the device choice's
  lists (issue #31): ListGpus' GPUs (none a CPU), BestGpu, the CPU driver's device (llvmpipe) and none for a
  missing manifest, `auto` opening the best GPU, `gpu` and every `gpuN` opening the GPU of that index, an index
  beyond the list opening nothing. On November 3, 2026: gpu0 NVIDIA GeForce RTX 5080 (discrete, the best), gpu1
  AMD Radeon(TM) Graphics (integrated), cpu llvmpipe (LLVM 23.1.4, 256 bits); and (issue-31-fix) the client's
  start-up order: headless instances first, then a renderer on each device for a window that is never shown (no
  WS_VISIBLE, no ShowWindow; destroyed at the end) and one frame presented; and (issue #52) quick device switches
  through TPresentWindows on every device and a held click across the device windows' changes ("One window per
  device" above); and (issue #53) switching to and from the CPU driver and each GPU leaves no handles of ours ("Handles
  the drivers leave" above; 228 assertions, about 26 s: 18 devices on the CPU driver, 12 on GPUs); and (issue #71) the
  main window's erase around the device windows ("The main window's background during a live resize" above, 83
  assertions, no device). 15 test cases, 1,392 assertions (14 and 1,309 before issue #71, 13 and 1,081 before issue
  #53, 11 and 88 before issue #52). Fills, 400 random
  unscaled copies of a texture with transparent texels, the automap's upload (a texture of texel coordinates:
  every pixel must show texel max(k - 1, 0)), stretches, the colour key pass, the light, a draw that samples its
  target, six frames of 4.8 MB uploads and a resized image, each read back and compared with a model of Direct3D
  9's pixels (tests/vulkan/render_test.cpp `ModelDraw`). On November 3, 2026, before issue #31: 9 test cases, 58 assertions; on
  llvmpipe (Mesa 26.2.5, LLVM 23.1.4) and on the NVIDIA GeForce RTX 5080 (Vulkan 1.4.357): fills, copies, the
  automap upload and the colour key 0 pixels differ; stretches, the automap quad sampled linearly and the light at
  most 1 per channel from the model (CPU driver: 1,251, 11,155 and 996 pixels; GPU: 932, 11,040 and 27,944).
- The advanced options' device choice (issue #31) in compare_rendering.py: for `vulkan` and `vulkan-cpu` the
  advanced-options scene masks the device choice's area in both runs (`device_choice_mask`: from the label's
  corner, SkinProperties.Style1394.Data08, to the dialog's width, Data00, and the buttons' bottom, Data10.y plus 20:
  the dialog's top 35 rows), next to the frame-rate mask; the rest of the dialog stays exact against Tibia.exe. The
  oracle's engines (dx5, dx9, gl) keep the original dialog, unmasked. The x64 client's frame of that area is the
  re-recorded reference (plan 6.3): compare_rendering.py keeps it as `<out>\vulkan\patella\advanced-options.png`
  (and vulkan-cpu's), to be looked at once by the user; it differs from Tibia.exe's only inside the mask.
- The live comparison against Tibia.exe (the orchestrator's, November 3, 2026, before the Edge copy):
  `python -I tools/compare_rendering.py --engines vulkan --modes windowed --no-repeat --patella
  build\windows-x64\Release\Patella.exe --recording build\traffic\result-x64\orig-live.jsonl`: on the RTX 5080
  all 26 scenes identical but the minimap (the Edge copy since) and advanced-options inside its frame-rate mask;
  with `--engines vulkan-cpu` the same minimap difference and the filtered stretches at most 2 per channel.

