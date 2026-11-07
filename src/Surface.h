// Surface.h: the graphics engine choice and its data (TGraphicsEngine, Surface.cpp's).
//
// The file name Surface.cpp is inferred: the code lies between Status.cpp and
// SurfaceDX.cpp in the alphabetical link order and is the engine-independent
// part of the surface classes (SurfaceDX.cpp, surfaceDX9.cpp, SurfaceGL.cpp
// derive from TSurface). Names from the binary: curEngine and
// SurfaceExitInstance; the rest are ours. TSurface itself, the engines' base class, is the platform's
// interface since Patella-Modern issue #138 (platform/Surface.h, exported here); its default bodies are still
// Surface.cpp's.

#pragma once

#include "platform/Surface.h" // IWYU pragma: export
#include "platform/Window.h"

// Engines (TGraphicsEngine::curEngine, below): 0 DirectX 5 (SurfaceDX.cpp), 1 OpenGL (SurfaceGL.cpp), 2 DirectX 9
// (surfaceDX9.cpp).

namespace patella {

// Surface.cpp's data (issue #116, M7; notes/decisions.md, "#104: TClient and the subsystem objects", "#116" and
// "#138"): the engine choice, the surface object of that engine, and the data the engines share (the blit statistics,
// the screen, the cache surfaces' size, the main window, the available engines: platform::TSurfaceShared, the base,
// which the engine's surface gets at its construction), with the same initial values. A part of TGraphics
// (graphics.h, its member Engine), declared here and defined in Surface.cpp, as TSession is Status.cpp's inside
// TCommunication. Its size is the same as before issue #138 (80 bytes on x64), so TGraphics' and TClient's later members
// keep their offsets. Surface.cpp's functions are its members of the same names; the free functions forward to them. The
// data the other files read or write was extern and is public (Control.cpp, Dialogs.cpp, MainFrm.cpp, Player.cpp and
// the golden tests; the Vulkan engine through its TSurfaceShared); the surface object is private, and the main window
// and the available engines are the base's, read by TSurface's TestEngine and GetAvailableEngines.
class TGraphicsEngine final : public platform::TSurfaceShared {
public:
	TGraphicsEngine() = default;
	TGraphicsEngine(const TGraphicsEngine &) = delete;
	TGraphicsEngine &operator=(const TGraphicsEngine &) = delete;
	TGraphicsEngine(TGraphicsEngine &&) = delete;
	TGraphicsEngine &operator=(TGraphicsEngine &&) = delete;
	~TGraphicsEngine() = default;

	[[nodiscard]] int GetCurrentEngine() const;               // 0x4eaa80
	TSurface *GetSurface();                                   // 0x4eaa90 (the x64 client's)
	void SurfaceExitInstance();                               // 0x4eab60
	void SurfaceInitInstance(platform::TWindowHandle Window); // the x64 client's

	// .data (the base's after the cache surfaces' size)
	int CommandLineEngine = -1; // 0x5c2564, "engine N" on the command line (Player.cpp); -1 = none
	// .bss (the base's, but for these two)
	int curEngine{}; // 0x71d0c4

private:
	TSurface *Surface{}; // 0x71d0cc, the engine's surface; raw, as the original's (deleted by SurfaceExitInstance)
};

// The transition's access path (notes/decisions.md, "#104: TClient and the subsystem objects"): the one TClient's
// TGraphicsEngine (Client.cpp). It goes when no free function needs it.
namespace bridge {
TGraphicsEngine &GraphicsEngine();
} // namespace bridge

} // namespace patella

int GetCurrentEngine();
TSurface *GetSurface();
void SurfaceExitInstance();
void SurfaceInitInstance(patella::platform::TWindowHandle Window);

// surfaceDX9.cpp (surfaceDX9.h): declared here because SurfaceDX.h's DirectX 6
// d3d.h and surfaceDX9.h's d3d9.h cannot be included together.
bool IsDirect3D9Installed();  // 0x4efd10
TSurface *CreateSurfaceDX9(); // 0x4f1430
