// WindowIconLinux.cpp: the Linux client's window icon through SDL3 (Patella-Modern issue #162; notes/decisions.md, "#162").
// Not part of the original source. On X11 SDL writes the window's _NET_WM_ICON property; on Wayland it needs the compositor's
// xdg_toplevel_icon_v1 protocol and fails without it, which is logged and changes nothing else.

#include "platform/WindowIcon.h"

#include "log/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <optional>

namespace patella::platform {

void ApplyWindowIcon(SDL_Window *Window)
{
	std::optional<TIconImage> Icon = DecodeIconImage(EmbeddedIcon());
	if (!Icon) {
		log::Warn("the built-in window icon is not a palette icon: the window has no icon");
		return;
	}
	SDL_Surface *Surface = SDL_CreateSurfaceFrom(Icon->Width, Icon->Height, SDL_PIXELFORMAT_ARGB8888, Icon->Pixels.data(), Icon->Width * 4);
	if (Surface == nullptr) {
		log::Warn("the window icon's surface could not be made: {}", SDL_GetError());
		return;
	}
	if (!SDL_SetWindowIcon(Window, Surface))
		log::Warn("the window icon could not be set: {}", SDL_GetError());
	SDL_DestroySurface(Surface);
}

} // namespace patella::platform
