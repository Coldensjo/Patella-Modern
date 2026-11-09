// tests_sdl_clipboard (issue #159): TSdlClipboard (src/platform/SdlClipboard.cpp) on SDL3's dummy video driver, or on
// the driver named by the SDL_VIDEO_DRIVER environment variable (never in CTest: the real drivers change the desktop's
// clipboard). Copying an empty text keeps an empty text on the clipboard, as the original's did.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "platform/SdlPlatform.h"

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>

#include <cstdio>
#include <optional>
#include <string>

using patella::platform::TSdlClipboard;

TEST_CASE("TSdlClipboard: an empty text is a text on the clipboard, as in the original")
{
	TSdlClipboard Clipboard;
	REQUIRE(Clipboard.SetText("before"));
	CHECK(Clipboard.HasText());
	REQUIRE(Clipboard.SetText(""));
	CHECK(Clipboard.HasText());
	CHECK(Clipboard.Text() == std::optional<std::string>(std::string()));
}

TEST_CASE("TSdlClipboard: a text comes back in the client's code page")
{
	TSdlClipboard Clipboard;
	REQUIRE(Clipboard.SetText("K\xF6nig"));
	CHECK(Clipboard.HasText());
	CHECK(Clipboard.Text() == std::optional<std::string>(std::string("K\xF6nig")));
}

// Why SetText does not call SDL_SetClipboardText for an empty text: if SDL3 ever changes this, that branch can go.
TEST_CASE("SDL_SetClipboardText with an empty text empties the clipboard")
{
	REQUIRE(SDL_SetClipboardText("before"));
	REQUIRE(SDL_HasClipboardText());
	REQUIRE(SDL_SetClipboardText(""));
	CHECK_FALSE(SDL_HasClipboardText());
}

int main(int argc, char **argv)
{
	if (SDL_getenv("SDL_VIDEO_DRIVER") == nullptr)
		SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::printf("tests_sdl_clipboard: SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	std::printf("tests_sdl_clipboard: the video driver is %s\n", SDL_GetCurrentVideoDriver());
	doctest::Context Context(argc, argv);
	const int Result = Context.run();
	SDL_Quit();
	return Result;
}
