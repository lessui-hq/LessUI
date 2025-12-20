/**
 * SDL/SDL_ttf.h - Shim header for unit testing
 *
 * Redirects #include <SDL/SDL_ttf.h> to our stubs.
 */

#ifndef SDL_TTF_H_SHIM
#define SDL_TTF_H_SHIM

#include "../sdl_stubs.h"

// TTF functions are declared in sdl_fakes.h (fff fakes)

#endif // SDL_TTF_H_SHIM
