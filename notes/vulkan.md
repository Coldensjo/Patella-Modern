# The Vulkan engine (Patella-Modern issue #19)

The Vulkan 1.3 engine of notes/modernization-plan.md, 6.2 (decision D5), in the Windows x64 client on the MFC
window. It draws what surfaceDX9.cpp draws, the reference engine of the frames (plan D7), call for call, and it
renders off screen with a read-back, which the frame dump of the test mode (issue #22) will use. The choices and
their reasons: notes/decisions.md, "The Vulkan engine (Patella-Modern issue #19)".

## Files

| File | What |
| :--- | :--- |
| `src/vulkan/SurfaceVulkan.h`, `.cpp` | `TSurfaceVulkan`, the `TSurface` of the client: surfaceDX9.cpp's logic with the Direct3D 9 calls replaced; the device choice (`PATELLA_VULKAN`); the Win32 surface; the display modes |
| `src/vulkan/Renderer.h`, `.cpp` | `patella::vulkan::TRenderer`, the back end: no window system, no MFC (portable C++23 with volk, vk-bootstrap and VMA) |
| `src/vulkan/Quads.h`, `.cpp` | how Direct3D 9's quads become Vulkan vertices; no Vulkan at all (tests_render on every preset) |
| `src/vulkan/FilterMask.h`, `.cpp` | `TFilterMasks`: which pixels come from the device's filter (the comparison's tolerance); no Vulkan (tests_render) |
| `src/vulkan/Vma.cpp` | VMA's implementation |
| `src/vulkan/shaders/*` | `Quad.vert` (every pipeline), `Quad.frag` (texture times vertex colour), `Fill.frag`, `ColorKey.frag`, `Constants.glsl` (the push constants) |
| `cmake/Vulkan.cmake` | the pinned dependencies, the shaders (glslang), `patella_vulkan_deps`, `patella_render_vulkan`, the CPU driver, tests/vulkan |
| `tests/vulkan/render_test.cpp` | `tests_vulkan`: the back end headless against a model of Direct3D 9's pixels |
| `tests/unit/render_quads_test.cpp`, `render_filtermask_test.cpp` | `tests_render`: Quads.cpp and FilterMask.cpp |

None of it is in the oracle-x86 build: its glob is `src/*.cpp`, and the only changes to reconstructed files are
the x64 client's own GetSurface and SurfaceInitInstance (Surface.cpp) and ChangeEngine (Control.cpp), behind
`#if defined(PATELLA_VULKAN)`, which only the x64 client defines (pecompare: the oracle's Patella.exe is
unchanged).

## Choosing the engine

Since issue #20 **the Vulkan engine is the Windows x64 client's only engine**: SurfaceDX.cpp (DirectDraw),
surfaceDX9.cpp (Direct3D 9) and SurfaceGL.cpp (OpenGL) are not compiled into it, and it imports neither ddraw.dll
nor opengl32.dll nor glu32.dll (nor loads d3d9.dll). The oracle-x86 build keeps the three until M6. The x64 client
has its own GetSurface and SurfaceInitInstance (Surface.cpp) and ChangeEngine (Control.cpp):

- **curEngine is always 2**, the number of the original's DirectX 9, the engine whose frames the Vulkan engine
  reproduces: everything that asks for the engine (graphics.cpp's colour keys, the FPS line of Ctrl+F, which says
  "DX9", error.txt's "Graphic Engine: 2") behaves as the original's DirectX 9.
- **Tibia.cfg's `Engine` keeps the original's number for Tibia.exe** (plan 6.3: "Tibia.cfg keeps an engine number
  in the original's range"). A number 0-2 from Tibia.cfg, or the command line's `engine N` (0-2), is stored as it
  is; none (-1) or one out of range becomes 2. That is what Tibia.exe writes on a machine where its engines work
  (this one: DirectX 9 and OpenGL), so compare_files.py sees the same Tibia.cfg.
- **The advanced graphics options are unchanged** (the same three buttons, enabled as Tibia.exe's on a machine
  with Direct3D 9 and OpenGL, "DirectX9" selected), so the frames stay Tibia.exe's. A button stores only its
  number (DirectX5 0, DirectX9 2, OpenGL 1) in Tibia.cfg, as the original stores it after a switch; the drawing
  stays Vulkan and the selection stays on "DirectX9". As in the original, a command-line engine skips the test
  and leaves the DirectX9 and OpenGL buttons disabled.
- **Without a Vulkan loader** (vulkan-1.dll, which every GPU driver with Vulkan installs) or any device there is
  no engine to fall back to: the start fails with PROGRAM_ERROR 12 ("Video surface allocation failed"), as the
  original's when its device could not be created.

Plan 6.3's device choice in the dialog ("Automatic", one entry per GPU, "CPU (compatibility)", stored in the
fork's settings file, D12) is not built yet (its layout, texts and the settings file need the user's decision;
notes/decisions.md, issue #20). Until then the environment variable `PATELLA_VULKAN` chooses the device:

| `PATELLA_VULKAN` | Device |
| :--- | :--- |
| unset, empty, `auto` (anything not below) | the best GPU with Vulkan 1.3 (discrete, integrated, virtual, other), else the CPU driver |
| `gpu`, `gpu0`, `gpu1` ... (`gpu:1`) | a GPU only: the best one, or the n-th in the loader's order |
| `cpu` | the CPU driver only |

`off` (with `0`, `no`, `dx9`), which gave engine 2 back to Direct3D 9 until issue #20, is automatic now.
`PATELLA_VULKAN_VALIDATION=1` asks for the validation layers (they come with the LunarG Vulkan SDK, which this
machine does not have; without them the request is ignored). `TDeviceRequest` (Renderer.h) already has plan
6.3's three kinds.

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
every device. The engine knows those pixels: while a frame observer (patcap.dll) watches, `TFilterMasks`
(FilterMask.cpp) follows its fills, uploads and quads with a byte per pixel of every image, "filtered" and
"transparent" (alpha 0). Uploads and fills set it exactly; a copy carries the source's bytes over, texel for texel
as the engine samples it, except through transparent texels (which leave the target's pixel); every other quad
marks every pixel it covers. So a glyph over the stretched start screen is exact where its texels are opaque, and
the lit map copied unscaled to the screen ("no stretching") keeps its marks. The captured frame's mask goes to
patcap.dll (below); compare_rendering.py applies `--region-tolerance` (default 2) on it only. One case is wider
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

tools/capture/patcap.c reads the other engines' frames by hooking their APIs. A Vulkan swapchain image cannot be
read back from outside the engine (the injected DLL would need the engine's device, queue and a copy usage of the
swapchain), so the engine calls patcap.dll itself when it is loaded (GetModuleHandle "patcap.dll"):
`PatcapVulkanDevice(name, type, cpu)` once it has a device, `PatcapVulkanFrame(width, height)` before every
frame is shown (patcap counts the frame, API 5, and answers whether it wants it), and, when wanted,
`PatcapVulkanMask` (the frame's device-filtered pixels, a byte each, written raw to the capture's path with
`.mask` appended) and `PatcapVulkanPixels` with SCREEN_SURFACE's rectangle read back, the picture Present shows.
Shared memory version 3 (`MaskPixels` at 1016: the marked pixels of the last mask). Without patcap.dll the engine
looks it up once per device, calls nothing and keeps no masks. These calls stay until the test mode's frame
dump (issue #22) replaces them; patcap's hooks of the original engines (DirectDrawCreate, Direct3DCreate9,
wglMakeCurrent, SwapBuffers) stay for Tibia.exe and the oracle. Since issue #20 the x64 client imports none of
those (patcap reports the seven others as hooked); compare_rendering.py needs none of them for it.

## Checks

- `tests_render` (every product preset, tests/unit): Quads.cpp and FilterMask.cpp, 12 test cases, 289
  assertions: when a quad is a copy or an Edge copy, the texel-centred coordinates at Vulkan's pixel centres,
  Direct3D 9's coordinates and colours at the pixel centres for the rest; the masks of fills, uploads,
  stretches, copies through transparent texels, the light, the automap's Edge copy and a self-draw.
- `tests_vulkan` (windows-x64 with the client; CTest, exit 77 = skipped without a loader): the back end
  headless, never a window, on the CPU driver next to it and on the machine's best GPU. Fills, 400 random
  unscaled copies of a texture with transparent texels, the automap's upload (a texture of texel coordinates:
  every pixel must show texel max(k - 1, 0)), stretches, the colour key pass, the light, a draw that samples its
  target, six frames of 4.8 MB uploads and a resized image, each read back and compared with a model of Direct3D
  9's pixels (tests/vulkan/render_test.cpp `ModelDraw`). On November 3, 2026: 9 test cases, 58 assertions; on
  llvmpipe (Mesa 26.2.5, LLVM 23.1.4) and on the NVIDIA GeForce RTX 5080 (Vulkan 1.4.357): fills, copies, the
  automap upload and the colour key 0 pixels differ; stretches, the automap quad sampled linearly and the light at
  most 1 per channel from the model (CPU driver: 1,251, 11,155 and 996 pixels; GPU: 932, 11,040 and 27,944).
- The live comparison against Tibia.exe (the orchestrator's, November 3, 2026, before the Edge copy):
  `python -I tools/compare_rendering.py --engines vulkan --modes windowed --no-repeat --patella
  build\windows-x64\Release\Patella.exe --recording build\traffic\result-x64\orig-live.jsonl`: on the RTX 5080
  all 26 scenes identical but the minimap (the Edge copy since) and advanced-options inside its frame-rate mask;
  with `--engines vulkan-cpu` the same minimap difference and the filtered stretches at most 2 per channel.

