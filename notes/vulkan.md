# The Vulkan engine (Patella-Modern issue #19)

The Vulkan 1.3 engine of notes/modernization-plan.md, 6.2 (decision D5), in the Windows x64 client on the MFC
window. It draws what surfaceDX9.cpp draws, the reference engine of the frames (plan D7), call for call, and it
renders off screen with a read-back, which the frame dump of the test mode (issue #22) will use. The choices and
their reasons: notes/decisions.md, "The Vulkan engine (Patella-Modern issue #19)".

## Files

| File | What |
| :--- | :--- |
| `src/vulkan/SurfaceVulkan.h`, `.cpp` | `TSurfaceVulkan`, the `TSurface` of the client: surfaceDX9.cpp's logic with the Direct3D 9 calls replaced; the engine choice (`PATELLA_VULKAN`); the Win32 surface |
| `src/vulkan/Renderer.h`, `.cpp` | `patella::vulkan::TRenderer`, the back end: no window system, no MFC (portable C++23 with volk, vk-bootstrap and VMA) |
| `src/vulkan/Quads.h`, `.cpp` | how Direct3D 9's quads become Vulkan vertices; no Vulkan at all (tests_render on every preset) |
| `src/vulkan/Vma.cpp` | VMA's implementation |
| `src/vulkan/shaders/*` | `Quad.vert` (every pipeline), `Quad.frag` (texture times vertex colour), `Fill.frag`, `ColorKey.frag`, `Constants.glsl` (the push constants) |
| `cmake/Vulkan.cmake` | the pinned dependencies, the shaders (glslang), `patella_vulkan_deps`, `patella_render_vulkan`, the CPU driver, tests/vulkan |
| `tests/vulkan/render_test.cpp` | `tests_vulkan`: the back end headless against a model of Direct3D 9's pixels |
| `tests/unit/render_quads_test.cpp` | `tests_render`: Quads.cpp |

None of it is in the oracle-x86 build: its glob is `src/*.cpp`, and the only change to a reconstructed file is
Surface.cpp's choice of engine 2 behind two macros that are surfaceDX9.cpp's calls without `PATELLA_VULKAN`
(pecompare: the oracle's Patella.exe is unchanged).

## Choosing the engine

Until issue #20 removes the three original engines from windows-x64, the Vulkan engine stands in for engine 2
(DirectX 9) when the environment variable `PATELLA_VULKAN` asks for it (Surface.cpp's
`PATELLA_ENGINE2_INSTALLED` and `PATELLA_CREATE_ENGINE2`):

| `PATELLA_VULKAN` | Device |
| :--- | :--- |
| unset, empty, `0`, `off`, `no` | none: the original engines, as before |
| `auto` (also `1`, `on`, anything else) | the best GPU with Vulkan 1.3 (discrete, integrated, virtual, other), else the CPU driver |
| `gpu`, `gpu0`, `gpu1` ... (`gpu:1`) | a GPU only: the best one, or the n-th in the loader's order |
| `cpu` | the CPU driver only |

So the client takes the Vulkan engine where it would take DirectX 9: Tibia.cfg's `Engine = 2` (or no engine
chosen, when Vulkan is available), or "DirectX 9" in the advanced graphics options. Tibia.cfg, its format and the
dialog are unchanged; the FPS line (Ctrl+F's overlay) still says "DX9". `PATELLA_VULKAN_VALIDATION=1` asks for the
validation layers (they come with the LunarG Vulkan SDK, which this machine does not have; without them the
request is ignored).

This is not yet the choice of plan 6.3 (the engine buttons becoming "Automatic", one entry per GPU and "CPU
(compatibility)", stored in the fork's settings file, D12): that changes the dialog the user sees, so it goes
with issue #20, when the engine buttons lose their meaning, and the settings file. `TDeviceRequest`
(Renderer.h) already has the three kinds.

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
(MainFrm.cpp SetDisplayMode). DirectDraw 4 still lists the display modes and tells the stretch capabilities, as in
surfaceDX9.cpp, so the graphics options show the same resolution list.

**Read-back**: `TRenderer::ReadBack(image, rectangle)` submits what is recorded, waits, and copies the pixels
(D3DCOLOR, rows top-down); `TSurfaceVulkan::ReadScreen` reads the rectangle the next frame shows. That is the
frame dump's source for issue #22 (plan 4.4), on any device, without a window system.

## Pixels

Direct3D 9 puts pixel centres on whole coordinates, the hardware (and Vulkan) at + 0.5; a Direct3D 9 driver moves
every vertex half a pixel. surfaceDX9.cpp's quads have whole corners and texture coordinates `(x + 0.5) / width`,
the texel centres. Quads.cpp turns a quad into vertices in one of two ways:

- **A copy** (one colour, texels one to one onto pixels: every unscaled blit, the automap's point-sampled
  106x106 upload): the corners stay on whole pixels, so no pixel centre lies on an edge, and the coordinates put
  the texel edges on the pixel edges, so nearest sampling reads the texel centres: the exact texels on every
  device, whatever its filter rounds. Linear filtering at a texel centre reads only that texel too, so this is
  what Direct3D 9 computes.
- **Everything else** (a stretch, the light's coloured corners, a coordinate between texels) goes to the hardware
  as a Direct3D 9 driver gives it: the corners half a pixel on, the attributes as they are, the linear or point
  sampler with wrapping (D3DTADDRESS_WRAP). The hardware then covers the same pixels (the top-left rule on the
  edges), interpolates the same values and filters them with its own bilinear filter.

So these pixels are exactly what Direct3D 9 gives, independent of the device: everything the client copies,
fills, uploads and colour-keys. These depend on the device's filter and rounding: **the light** (plan 6.1) and the
**bilinear stretches**: the start screen's picture (640x480 stretched over the window, MainWindow.cpp
TStartWindow::Draw: the start, login and option dialog scenes), the map view (480x352 stretched to the map
window, MapWindow.cpp, unless "no stretching" is set: the game scenes) and the scaled item pictures
(graphics.cpp StretchBlit). Direct3D 9 filters them with the GPU too, so Tibia.exe's own DirectX 9 frames depend
on the graphics card there. On the same NVIDIA GPU the Vulkan engine gives the hardware the same vertices as a
Direct3D 9 driver does, so the same pixels are likely; on the CPU driver they can differ by 1 per colour channel
(tests_vulkan below). The plan's contract (6.1, 4.5: "exact outside the light") assumed every engine operation
except the light was integer work; the bilinear stretches are not (notes/decisions.md, issue #19,
"Needs the user"). compare_rendering.py reports each scene's largest channel difference and takes `--tolerance`.

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
`PatcapVulkanPixels` with SCREEN_SURFACE's rectangle read back, the picture Present shows. Without patcap.dll the
engine looks it up once per device and calls nothing. With the original engines (issue #20) these calls go;
the test mode's frame dump (issue #22) reads the same canvas.

## Checks

- `tests_render` (every product preset, tests/unit): Quads.cpp, 8 test cases, 249 assertions: when a quad is a
  copy, the texel-centred coordinates at Vulkan's pixel centres, Direct3D 9's coordinates and colours at the
  pixel centres for the rest.
- `tests_vulkan` (windows-x64 with the client; CTest, exit 77 = skipped without a loader): the back end
  headless, never a window, on the CPU driver next to it and on the machine's best GPU. Fills, 400 random
  unscaled copies of a texture with transparent texels, the automap's point-sampled copy, stretches, the colour
  key pass, the light, a draw that samples its target, six frames of 4.8 MB uploads and a resized image, each
  read back and compared with a model of Direct3D 9's pixels (tests/vulkan/render_test.cpp `ModelDraw`). On
  November 3, 2026: 9 test cases, 60 assertions; on llvmpipe (Mesa 26.2.5, LLVM 23.1.4) and on the NVIDIA GeForce
  RTX 5080 (Vulkan 1.4.357): fills, copies, the automap copy and the colour key 0 pixels differ; stretches,
  the automap quad sampled linearly and the light at most 1 per channel (CPU driver: 1,251, 7,263 and 996
  pixels; GPU: 932, 5,768 and 27,944).
- The live comparison against Tibia.exe: `python -I tools/compare_rendering.py --engines vulkan,vulkan-cpu
  --modes windowed --patella build\windows-x64\Release\Patella.exe` (notes/rendering.md).
