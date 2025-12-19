/**
 * effect_utils.h - Runtime effect pattern generation for SDL2 platforms
 *
 * Provides helpers to generate and tile effect patterns using SDL2 textures.
 * All patterns are procedurally generated at the target scale.
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

/**
 * Loads a base pattern PNG, applies color tinting, and creates a scaled/tiled texture.
 *
 * Same as EFFECT_loadAndTile but with color replacement for non-transparent pixels.
 * Used for GameBoy DMG color palettes where the grid effect is tinted to match.
 *
 * @param renderer SDL renderer
 * @param pattern_path Path to base pattern PNG
 * @param scale Integer scale factor (2-11)
 * @param target_w Target texture width in pixels
 * @param target_h Target texture height in pixels
 * @param color RGB565 color to tint non-transparent pixels (0 = no tinting, use black)
 * @return SDL texture with tiled pattern (caller must SDL_DestroyTexture), or NULL on error
 */
SDL_Texture* EFFECT_loadAndTileWithColor(SDL_Renderer* renderer, const char* pattern_path,
                                         int scale, int target_w, int target_h, int color);

/**
 * Creates an effect texture using procedural generation.
 *
 * For LINE, GRID, GRILLE, and SLOT effects. Generates the pattern directly without loading files.
 *
 * @param renderer SDL renderer
 * @param type     Effect type (EFFECT_LINE, EFFECT_GRID, EFFECT_GRILLE, EFFECT_SLOT)
 * @param scale    Content-to-screen scale factor
 * @param target_w Target texture width in pixels
 * @param target_h Target texture height in pixels
 * @return SDL texture with generated pattern (caller must SDL_DestroyTexture), or NULL on error
 */
SDL_Texture* EFFECT_createGeneratedTexture(SDL_Renderer* renderer, int type, int scale,
                                           int target_w, int target_h);

/**
 * Creates an effect texture using procedural generation with color tinting.
 *
 * Same as EFFECT_createGeneratedTexture but with color support for GRID effect.
 * Used for Game Boy DMG palette matching.
 *
 * @param renderer SDL renderer
 * @param type     Effect type (EFFECT_LINE, EFFECT_GRID, EFFECT_GRILLE, EFFECT_SLOT)
 * @param scale    Content-to-screen scale factor
 * @param target_w Target texture width in pixels
 * @param target_h Target texture height in pixels
 * @param color    RGB565 color for GRID lines (0 = black, only affects GRID)
 * @return SDL texture with generated pattern (caller must SDL_DestroyTexture), or NULL on error
 */
SDL_Texture* EFFECT_createGeneratedTextureWithColor(SDL_Renderer* renderer, int type, int scale,
                                                    int target_w, int target_h, int color);

#endif
