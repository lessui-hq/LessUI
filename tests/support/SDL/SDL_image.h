/**
 * SDL/SDL_image.h - Shim header for unit testing
 *
 * Redirects #include <SDL/SDL_image.h> to our stubs.
 */

#ifndef SDL_IMAGE_H_SHIM
#define SDL_IMAGE_H_SHIM

#include "../sdl_stubs.h"

// IMG_Load is declared in sdl_fakes.h (fff fake)

#endif // SDL_IMAGE_H_SHIM
