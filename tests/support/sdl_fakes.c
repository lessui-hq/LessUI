/**
 * sdl_fakes.c - SDL function fake implementations
 *
 * Defines the actual fake function instances using fff macros.
 * This file must be included/compiled exactly once in the test suite.
 *
 * Provides comprehensive SDL mocking for unit testing Player/Launcher
 * components that depend on SDL without requiring actual SDL.
 */

#include "sdl_fakes.h"

#include <stdlib.h>
#include <string.h>

// Define fff globals (must be done once per test executable)
DEFINE_FFF_GLOBALS;

///////////////////////////////
// Mock Surface Implementation
///////////////////////////////

// Static format used by mock surfaces
static SDL_PixelFormat mock_pixel_format = {
    .BitsPerPixel = 16,
    .BytesPerPixel = 2,
    .Rmask = 0xF800,
    .Gmask = 0x07E0,
    .Bmask = 0x001F,
    .Amask = 0x0000,
};

SDL_Surface* mock_sdl_create_surface(int w, int h) {
	SDL_Surface* surface = (SDL_Surface*)calloc(1, sizeof(SDL_Surface));
	if (surface) {
		surface->w = w;
		surface->h = h;
		surface->pitch = w * 2; // RGB565
		surface->format = &mock_pixel_format;
		surface->pixels = calloc(1, (size_t)(w * h * 2));
		surface->refcount = 1;
	}
	return surface;
}

void mock_sdl_free_surface(SDL_Surface* surface) {
	if (surface) {
		if (surface->pixels) {
			free(surface->pixels);
		}
		free(surface);
	}
}

///////////////////////////////
// Event System Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(int, SDL_PollEvent, SDL_Event*);
DEFINE_FAKE_VALUE_FUNC(Uint32, SDL_GetTicks);

///////////////////////////////
// Surface Management Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(SDL_Surface*, SDL_CreateRGBSurface,
                       Uint32, int, int, int, Uint32, Uint32, Uint32, Uint32);

DEFINE_FAKE_VALUE_FUNC(SDL_Surface*, SDL_CreateRGBSurfaceFrom,
                       void*, int, int, int, int, Uint32, Uint32, Uint32, Uint32);

DEFINE_FAKE_VALUE_FUNC(SDL_Surface*, SDL_ConvertSurface,
                       SDL_Surface*, SDL_PixelFormat*, Uint32);

DEFINE_FAKE_VOID_FUNC(SDL_FreeSurface, SDL_Surface*);

DEFINE_FAKE_VALUE_FUNC(int, SDL_FillRect, SDL_Surface*, SDL_Rect*, Uint32);

DEFINE_FAKE_VALUE_FUNC(int, SDL_BlitSurface,
                       SDL_Surface*, SDL_Rect*, SDL_Surface*, SDL_Rect*);

DEFINE_FAKE_VALUE_FUNC(int, SDLX_SetAlpha, SDL_Surface*, Uint32, Uint8);

///////////////////////////////
// TTF Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(SDL_Surface*, TTF_RenderUTF8_Blended,
                       TTF_Font*, const char*, SDL_Color);

DEFINE_FAKE_VALUE_FUNC(int, TTF_SizeUTF8, TTF_Font*, const char*, int*, int*);

///////////////////////////////
// SDL_image Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(SDL_Surface*, IMG_Load, const char*);

///////////////////////////////
// File I/O Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(SDL_RWops*, SDL_RWFromFile, const char*, const char*);
DEFINE_FAKE_VALUE_FUNC(int, SDL_SaveBMP_RW, SDL_Surface*, SDL_RWops*, int);

///////////////////////////////
// Reset All Fakes
///////////////////////////////

void reset_all_sdl_fakes(void) {
	// Event system
	RESET_FAKE(SDL_PollEvent);
	RESET_FAKE(SDL_GetTicks);

	// Surface management
	RESET_FAKE(SDL_CreateRGBSurface);
	RESET_FAKE(SDL_CreateRGBSurfaceFrom);
	RESET_FAKE(SDL_ConvertSurface);
	RESET_FAKE(SDL_FreeSurface);
	RESET_FAKE(SDL_FillRect);
	RESET_FAKE(SDL_BlitSurface);
	RESET_FAKE(SDLX_SetAlpha);

	// TTF
	RESET_FAKE(TTF_RenderUTF8_Blended);
	RESET_FAKE(TTF_SizeUTF8);

	// SDL_image
	RESET_FAKE(IMG_Load);

	// File I/O
	RESET_FAKE(SDL_RWFromFile);
	RESET_FAKE(SDL_SaveBMP_RW);
}
