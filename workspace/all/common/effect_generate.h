/**
 * effect_generate.h - Procedural generation of CRT/LCD effect patterns
 *
 * Generates effect overlays directly into pixel buffers without loading PNGs.
 * All patterns are screen-native (1:1 pixel mapping) with band sizes based on
 * the content-to-screen scale factor.
 *
 * Pattern types:
 * - LINE: Simple horizontal scanlines (black + alpha)
 * - GRID: LCD pixel borders (1px dark border per content pixel)
 * - GRILLE: Aperture grille with RGB phosphor tints + scanlines
 * - SLOT: Staggered brick/slot mask pattern
 */

#ifndef __EFFECT_GENERATE_H__
#define __EFFECT_GENERATE_H__

#include <stdint.h>

/**
 * Generates aperture grille pattern with phosphor RGB tints.
 *
 * Simulates Sony Trinitron-style displays - vertical phosphor stripes
 * with horizontal scanlines. Based on zfast_crt shader.
 *
 * Pattern (symmetric 3x3 repeating tile):
 * - Rows 0,2: Dark scanlines at pixel edges (alpha 90 = 35% darkening)
 * - Row 1: Bright phosphor center (lowest alpha, most light through)
 * - Columns alternate: cyan, blue, red phosphor tints
 *
 * @param pixels  ARGB8888 pixel buffer to write into
 * @param width   Buffer width in pixels
 * @param height  Buffer height in pixels
 * @param pitch   Buffer pitch in bytes (may differ from width*4)
 * @param scale   Content-to-screen scale factor (each band is `scale` pixels tall)
 */
void EFFECT_generateGrille(uint32_t* pixels, int width, int height, int pitch, int scale);

/**
 * Generates simple horizontal scanline pattern.
 *
 * Black-only pattern (no phosphor tints) - symmetric like grille:
 * - Rows 0,2: Dark scanlines at pixel edges (alpha 90 = 35% darkening)
 * - Row 1: Bright center (minimal alpha, most light through)
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
 * Each content pixel gets graduated darkening on left and bottom edges,
 * simulating LCD subpixel structure.
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
 * creating a brick/honeycomb pattern.
 *
 * @param pixels  ARGB8888 pixel buffer to write into
 * @param width   Buffer width in pixels
 * @param height  Buffer height in pixels
 * @param pitch   Buffer pitch in bytes
 * @param scale   Content-to-screen scale factor
 */
void EFFECT_generateSlot(uint32_t* pixels, int width, int height, int pitch, int scale);

#endif /* __EFFECT_GENERATE_H__ */
