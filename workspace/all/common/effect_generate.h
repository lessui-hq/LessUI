/**
 * effect_generate.h - Procedural generation of CRT/LCD effect patterns
 *
 * Generates effect overlays directly into pixel buffers without loading PNGs.
 * All patterns are screen-native (1:1 pixel mapping) with band sizes based on
 * the content-to-screen scale factor.
 *
 * Pattern types (all use opaque black with global alpha control):
 * - LINE: Simple horizontal scanlines (opaque black borders, transparent center)
 * - GRID: LCD pixel borders (opaque black borders, transparent interior)
 * - GRILLE: Aperture grille with RGB phosphor tints + opaque black scanlines
 * - SLOT: Staggered brick/slot mask pattern (opaque black borders, transparent openings)
 *
 * Global alpha (scale-dependent) controls effect intensity: 30 + (scale * 20)
 */

#ifndef __EFFECT_GENERATE_H__
#define __EFFECT_GENERATE_H__

#include <stdint.h>

/**
 * Generates aperture grille pattern with RGB phosphor tints.
 *
 * Simulates Trinitron-style displays with 3x3 repeating tile.
 * Alpha values scaled up (×2.833) to compensate for global alpha.
 *
 * Pattern:
 * - Rows 0,2: Dark scanlines (alpha=255, was 90)
 * - Row 1: RGB phosphor tints (alpha=14-28, was 5-10)
 * - Columns: Cyan, Blue, Red phosphor variation
 * - Global alpha controls overall darkness (scale-dependent)
 *
 * @param pixels  ARGB8888 pixel buffer to write into
 * @param width   Buffer width in pixels
 * @param height  Buffer height in pixels
 * @param pitch   Buffer pitch in bytes (may differ from width*4)
 * @param scale   Content-to-screen scale factor
 */
void EFFECT_generateGrille(uint32_t* pixels, int width, int height, int pitch, int scale);

/**
 * Generates simple horizontal scanline pattern.
 *
 * Black-only pattern (no phosphor tints) - symmetric scanlines:
 * - Rows 0,2: Opaque black scanlines at pixel edges (alpha 255)
 * - Row 1: Transparent center (alpha 0, shows content)
 * - Global alpha controls overall darkness (scale-dependent)
 *
 * @param pixels  ARGB8888 pixel buffer to write into
 * @param width   Buffer width in pixels
 * @param height  Buffer height in pixels
 * @param pitch   Buffer pitch in bytes
 * @param scale   Content-to-screen scale factor
 */
void EFFECT_generateLine(uint32_t* pixels, int width, int height, int pitch, int scale);

/**
 * Generates LCD pixel grid pattern.
 *
 * Each content pixel gets graduated alpha borders on left and bottom edges.
 * Alpha values scaled up (×2.833) to compensate for global alpha:
 * - Scale 2: 181 (was 64)
 * - Scale 3+: 255 for edges/corners (was 102/153, capped at 255)
 *
 * @param pixels  ARGB8888 pixel buffer to write into
 * @param width   Buffer width in pixels
 * @param height  Buffer height in pixels
 * @param pitch   Buffer pitch in bytes
 * @param scale   Content-to-screen scale factor
 */
void EFFECT_generateGrid(uint32_t* pixels, int width, int height, int pitch, int scale);

/**
 * Generates LCD pixel grid pattern with color tint.
 *
 * Same as EFFECT_generateGrid but uses the specified RGB565 color
 * instead of black for the grid lines. Used for Game Boy DMG palette matching.
 *
 * @param pixels  ARGB8888 pixel buffer to write into
 * @param width   Buffer width in pixels
 * @param height  Buffer height in pixels
 * @param pitch   Buffer pitch in bytes
 * @param scale   Content-to-screen scale factor
 * @param color   RGB565 color for grid lines (0 = black)
 */
void EFFECT_generateGridWithColor(uint32_t* pixels, int width, int height, int pitch, int scale,
                                  int color);

/**
 * Generates staggered slot mask pattern.
 *
 * Like GRID but with alternating rows offset by half a cell width,
 * creating a brick/honeycomb pattern. Uses opaque black borders with
 * transparent slot openings. Global alpha controls overall darkness.
 *
 * @param pixels  ARGB8888 pixel buffer to write into
 * @param width   Buffer width in pixels
 * @param height  Buffer height in pixels
 * @param pitch   Buffer pitch in bytes
 * @param scale   Content-to-screen scale factor
 */
void EFFECT_generateSlot(uint32_t* pixels, int width, int height, int pitch, int scale);

#endif /* __EFFECT_GENERATE_H__ */
