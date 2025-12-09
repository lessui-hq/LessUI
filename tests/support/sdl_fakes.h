/**
 * sdl_fakes.h - SDL function fakes using fff framework
 *
 * Declares fake SDL functions for unit testing. Uses the Fake Function
 * Framework (fff) to create mockable versions of SDL functions.
 *
 * Functions are organized by SDL subsystem:
 * - Event system: SDL_PollEvent, SDL_GetTicks
 * - Surface management: SDL_CreateRGBSurface, SDL_FreeSurface, SDL_BlitSurface, etc.
 * - TTF (fonts): TTF_RenderUTF8_Blended, TTF_SizeUTF8
 * - Image loading: IMG_Load
 * - File I/O: SDL_RWFromFile, SDL_SaveBMP_RW
 *
 * Usage:
 *   #include "sdl_fakes.h"
 *   RESET_FAKE(SDL_PollEvent);  // Reset before each test
 *   SDL_PollEvent_fake.return_val = 1;  // Configure mock behavior
 *
 * For tests that need surfaces, use the mock surface helpers:
 *   SDL_Surface* mock = mock_sdl_create_surface(640, 480);
 *   SDL_CreateRGBSurface_fake.return_val = mock;
 */

#ifndef SDL_FAKES_H
#define SDL_FAKES_H

#include "fff/fff.h"
#include "sdl_stubs.h"

///////////////////////////////
// Mock Surface Helpers
///////////////////////////////

/**
 * Create a mock SDL_Surface for testing.
 * The surface has minimal fields populated.
 * Caller should free with mock_sdl_free_surface().
 */
SDL_Surface* mock_sdl_create_surface(int w, int h);

/**
 * Free a mock SDL_Surface created by mock_sdl_create_surface().
 */
void mock_sdl_free_surface(SDL_Surface* surface);

/**
 * Reset all SDL fakes to initial state.
 * Call this in setUp() before each test.
 */
void reset_all_sdl_fakes(void);

///////////////////////////////
// Event System Fakes
///////////////////////////////

/**
 * Fake for SDL_PollEvent - Used by PAD input system
 */
DECLARE_FAKE_VALUE_FUNC(int, SDL_PollEvent, SDL_Event*);

/**
 * Fake for SDL_GetTicks - Returns milliseconds since SDL init
 */
DECLARE_FAKE_VALUE_FUNC(Uint32, SDL_GetTicks);

///////////////////////////////
// Surface Management Fakes
///////////////////////////////

/**
 * Fake for SDL_CreateRGBSurface - Create a new surface
 * Args: flags, width, height, depth, Rmask, Gmask, Bmask, Amask
 */
DECLARE_FAKE_VALUE_FUNC(SDL_Surface*, SDL_CreateRGBSurface,
                        Uint32, int, int, int, Uint32, Uint32, Uint32, Uint32);

/**
 * Fake for SDL_CreateRGBSurfaceFrom - Create surface from existing pixels
 * Args: pixels, width, height, depth, pitch, Rmask, Gmask, Bmask, Amask
 */
DECLARE_FAKE_VALUE_FUNC(SDL_Surface*, SDL_CreateRGBSurfaceFrom,
                        void*, int, int, int, int, Uint32, Uint32, Uint32, Uint32);

/**
 * Fake for SDL_ConvertSurface - Convert surface to different format
 */
DECLARE_FAKE_VALUE_FUNC(SDL_Surface*, SDL_ConvertSurface,
                        SDL_Surface*, SDL_PixelFormat*, Uint32);

/**
 * Fake for SDL_FreeSurface - Free a surface
 */
DECLARE_FAKE_VOID_FUNC(SDL_FreeSurface, SDL_Surface*);

/**
 * Fake for SDL_FillRect - Fill rectangle with color
 */
DECLARE_FAKE_VALUE_FUNC(int, SDL_FillRect, SDL_Surface*, SDL_Rect*, Uint32);

/**
 * Fake for SDL_BlitSurface - Copy one surface to another
 */
DECLARE_FAKE_VALUE_FUNC(int, SDL_BlitSurface,
                        SDL_Surface*, SDL_Rect*, SDL_Surface*, SDL_Rect*);

/**
 * Fake for SDLX_SetAlpha - Custom alpha setting (api.c wrapper)
 */
DECLARE_FAKE_VALUE_FUNC(int, SDLX_SetAlpha, SDL_Surface*, Uint32, Uint8);

///////////////////////////////
// TTF (TrueType Font) Fakes
///////////////////////////////

/**
 * Fake for TTF_RenderUTF8_Blended - Render text to surface
 */
DECLARE_FAKE_VALUE_FUNC(SDL_Surface*, TTF_RenderUTF8_Blended,
                        TTF_Font*, const char*, SDL_Color);

/**
 * Fake for TTF_SizeUTF8 - Get size of rendered text
 */
DECLARE_FAKE_VALUE_FUNC(int, TTF_SizeUTF8, TTF_Font*, const char*, int*, int*);

///////////////////////////////
// SDL_image Fakes
///////////////////////////////

/**
 * Fake for IMG_Load - Load image from file
 */
DECLARE_FAKE_VALUE_FUNC(SDL_Surface*, IMG_Load, const char*);

///////////////////////////////
// File I/O Fakes
///////////////////////////////

/**
 * Fake for SDL_RWFromFile - Open file for read/write
 */
DECLARE_FAKE_VALUE_FUNC(SDL_RWops*, SDL_RWFromFile, const char*, const char*);

/**
 * Fake for SDL_SaveBMP_RW - Save surface as BMP
 */
DECLARE_FAKE_VALUE_FUNC(int, SDL_SaveBMP_RW, SDL_Surface*, SDL_RWops*, int);

///////////////////////////////
// Convenience Reset Macro
///////////////////////////////

/**
 * Reset all SDL fakes - call in setUp()
 */
#define RESET_ALL_SDL_FAKES() reset_all_sdl_fakes()

#endif // SDL_FAKES_H
