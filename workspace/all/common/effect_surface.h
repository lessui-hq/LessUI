/**
 * effect_surface.h - CPU-based effect pattern scaling and tiling
 *
 * For platforms that don't use SDL renderers (miyoomini, trimuismart, rg35xx).
 * Simple nearest-neighbor scaling and tiling using SDL_Surface only.
 */

#ifndef __EFFECT_SURFACE_H__
#define __EFFECT_SURFACE_H__

#include "sdl.h"

/**
 * Loads a base pattern PNG, scales it, and tiles it into an SDL_Surface.
 *
 * Uses simple pixel replication for scaling (nearest-neighbor).
 * Tiles the scaled pattern across target dimensions.
 *
 * @param pattern_path Path to base pattern PNG
 * @param scale Integer scale factor (2-11)
 * @param target_w Target surface width
 * @param target_h Target surface height
 * @return SDL_Surface with tiled pattern (caller must SDL_FreeSurface), or NULL on error
 */
SDL_Surface* EFFECT_createTiledSurface(const char* pattern_path, int scale, int target_w,
                                       int target_h);

/**
 * Loads a base pattern PNG, applies color tinting, scales it, and tiles it into an SDL_Surface.
 *
 * Same as EFFECT_createTiledSurface but with color replacement for non-transparent pixels.
 * Used for GameBoy DMG color palettes where the grid effect is tinted to match.
 *
 * @param pattern_path Path to base pattern PNG
 * @param scale Integer scale factor (2-11)
 * @param target_w Target surface width
 * @param target_h Target surface height
 * @param color RGB565 color to tint non-transparent pixels (0 = no tinting, use black)
 * @return SDL_Surface with tiled pattern (caller must SDL_FreeSurface), or NULL on error
 */
SDL_Surface* EFFECT_createTiledSurfaceWithColor(const char* pattern_path, int scale, int target_w,
                                                int target_h, int color);

#endif
