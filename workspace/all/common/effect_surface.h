/**
 * effect_surface.h - CPU-based effect pattern generation and tiling
 *
 * For SDL1 platforms (miyoomini, trimuismart, rg35xx).
 * All effects (LINE, GRID, GRILLE, SLOT) are procedurally generated.
 */

#ifndef __EFFECT_SURFACE_H__
#define __EFFECT_SURFACE_H__

#include "sdl.h"

/**
 * Creates an effect surface using procedural generation.
 *
 * For LINE, GRID, CRT, and SLOT effects. Generates the pattern directly into
 * the surface pixels without loading any files.
 *
 * @param type     Effect type (EFFECT_LINE, EFFECT_GRID, EFFECT_GRILLE, EFFECT_SLOT)
 * @param scale    Content-to-screen scale factor
 * @param target_w Target surface width
 * @param target_h Target surface height
 * @return SDL_Surface with generated pattern (caller must SDL_FreeSurface), or NULL on error
 */
SDL_Surface* EFFECT_createGeneratedSurface(int type, int scale, int target_w, int target_h);

/**
 * Creates an effect surface using procedural generation with color tinting.
 *
 * Same as EFFECT_createGeneratedSurface but with color support for GRID effect.
 * Used for Game Boy DMG palette matching.
 *
 * @param type     Effect type (EFFECT_LINE, EFFECT_GRID, EFFECT_GRILLE, EFFECT_SLOT)
 * @param scale    Content-to-screen scale factor
 * @param target_w Target surface width
 * @param target_h Target surface height
 * @param color    RGB565 color for GRID lines (0 = black, only affects GRID)
 * @return SDL_Surface with generated pattern (caller must SDL_FreeSurface), or NULL on error
 */
SDL_Surface* EFFECT_createGeneratedSurfaceWithColor(int type, int scale, int target_w, int target_h,
                                                    int color);

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
