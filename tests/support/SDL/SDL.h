/**
 * SDL/SDL.h - Shim header for unit testing
 *
 * Redirects #include <SDL/SDL.h> to our stub types and fff fakes.
 * Place tests/support in include path BEFORE system SDL paths.
 */

#ifndef SDL_SDL_H_SHIM
#define SDL_SDL_H_SHIM

#include "../sdl_stubs.h"

// Additional SDL constants needed by minarch code
#define SDL_SWSURFACE 0x00000000
#define SDL_SRCALPHA  0x00010000

#endif // SDL_SDL_H_SHIM
