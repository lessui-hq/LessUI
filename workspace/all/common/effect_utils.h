/**
 * effect_utils.h - Runtime effect pattern scaling and tiling for SDL platforms
 *
 * Provides simple helpers to load tiny base pattern PNGs, scale them using
 * SDL's built-in scaling, and tile them across rendering areas.
 *
 * This enables retro CRT/LCD effects without large pre-scaled PNG assets.
 */

#ifndef __EFFECT_UTILS_H__
#define __EFFECT_UTILS_H__

#include "sdl.h"

/**
 * Loads a base pattern PNG and creates a scaled/tiled texture ready for rendering.
 *
 * Process: Load PNG → Scale pattern by scale factor → Tile across target dimensions → Create texture
 *
 * @param renderer SDL renderer
 * @param pattern_path Path to base pattern PNG (e.g., RES_PATH "/line.png")
 * @param scale Integer scale factor (2-11)
 * @param target_w Target texture width in pixels
 * @param target_h Target texture height in pixels
 * @return SDL texture with tiled pattern (caller must SDL_DestroyTexture), or NULL on error
 */
SDL_Texture* EFFECT_loadAndTile(SDL_Renderer* renderer, const char* pattern_path, int scale,
                                int target_w, int target_h);

#endif
