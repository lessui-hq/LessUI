/**
 * effect_generate.c - Procedural generation of retro display effect patterns
 *
 * All patterns write directly to ARGB8888 pixel buffers. The overlay is always
 * pixel-perfect to the screen - no scaling of the overlay itself. Band sizes
 * are determined by the content-to-screen scale factor.
 */

#include "effect_generate.h"

#include <stddef.h>

/**
 * Aperture grille pattern - 3x3 tile
 *
 * Simulates Sony Trinitron-style displays with RGB phosphor tints.
 * Per-pixel alpha values provide pattern structure; global alpha (128) controls
 * overall visibility. Pattern is scaled to match content pixel size.
 *
 * Rows:
 *   0: Dark scanline (edge) - alpha 255
 *   1: Bright phosphor center - alpha 14-28
 *   2: Dark scanline (edge) - alpha 255
 *
 * Columns: Cyan, Blue, Red phosphor tints
 */
static const uint8_t GRILLE_TILE[3][3][4] = {
    // Row 0: dark scanline (top edge)
    {{0, 1, 1, 255}, {1, 0, 3, 255}, {2, 0, 0, 255}},
    // Row 1: phosphor with RGB tints (alpha: 14, 17, 28)
    {{0, 252, 245, 14}, {0, 0, 243, 17}, {236, 1, 0, 28}},
    // Row 2: dark scanline (bottom edge)
    {{0, 1, 1, 255}, {1, 0, 3, 255}, {2, 0, 0, 255}}};

/**
 * Simple scanline pattern - 1x3 tile (no horizontal variation)
 *
 * Symmetric scanlines with per-pixel alpha. Global alpha (128) controls
 * overall visibility. Pattern is scaled to match content pixel size.
 *
 * Pattern: {255, 6, 255} - dark edges, transparent center
 */
static const uint8_t LINE_ALPHA[3] = {255, 6, 255};

void EFFECT_generateGrille(uint32_t* pixels, int width, int height, int pitch, int scale) {
	if (!pixels || width <= 0 || height <= 0 || scale < 1)
		return;

	int pitch_pixels = pitch / 4;

	for (int y = 0; y < height; y++) {
		// Map position within each content pixel to tile row (0, 1, or 2)
		int pos_in_pixel = y % scale;
		int tile_row = (pos_in_pixel * 3) / scale;
		uint32_t* row = pixels + (size_t)y * pitch_pixels;

		for (int x = 0; x < width; x++) {
			int pos_in_pixel_x = x % scale;
			int tile_col = (pos_in_pixel_x * 3) / scale;
			const uint8_t* p = GRILLE_TILE[tile_row][tile_col];
			// ARGB8888: alpha in high byte
			row[x] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
		}
	}
}

void EFFECT_generateLine(uint32_t* pixels, int width, int height, int pitch, int scale) {
	if (!pixels || width <= 0 || height <= 0 || scale < 1)
		return;

	int pitch_pixels = pitch / 4;

	for (int y = 0; y < height; y++) {
		// Map position within each content pixel to tile row (0, 1, or 2)
		int pos_in_pixel = y % scale;
		int tile_row = (pos_in_pixel * 3) / scale;
		uint8_t alpha = LINE_ALPHA[tile_row];
		uint32_t pixel = (uint32_t)alpha << 24; // Black with alpha
		uint32_t* row = pixels + (size_t)y * pitch_pixels;

		for (int x = 0; x < width; x++) {
			row[x] = pixel;
		}
	}
}

void EFFECT_generateGridWithColor(uint32_t* pixels, int width, int height, int pitch, int scale,
                                  int color) {
	if (!pixels || width <= 0 || height <= 0 || scale < 1)
		return;

	int pitch_pixels = pitch / 4;

	// Convert RGB565 color to RGB components (0 = black)
	uint8_t r = 0, g = 0, b = 0;
	if (color != 0) {
		// RGB565: RRRRRGGG GGGBBBBB
		uint8_t r5 = (color >> 11) & 0x1F;
		uint8_t g6 = (color >> 5) & 0x3F;
		uint8_t b5 = color & 0x1F;
		// Expand to 8-bit
		r = (r5 << 3) | (r5 >> 2);
		g = (g6 << 2) | (g6 >> 4);
		b = (b5 << 3) | (b5 >> 2);
	}

	// Grid pattern: graduated alpha for soft edges. Per-pixel alpha provides
	// pattern structure; global alpha (128) controls overall visibility.

	for (int y = 0; y < height; y++) {
		int cell_y = y % scale;
		int is_bottom = (cell_y == scale - 1);
		uint32_t* row = pixels + (size_t)y * pitch_pixels;

		for (int x = 0; x < width; x++) {
			int cell_x = x % scale;
			int is_left = (cell_x == 0);

			uint8_t alpha;
			if (scale == 2) {
				// Scale 2: lighter borders
				int is_interior = (cell_x == scale - 1) && (cell_y == scale - 1);
				alpha = is_interior ? 0 : 181;
			} else {
				// Scale 3+: graduated alpha for softer edges
				if (is_left && is_bottom) {
					alpha = 255; // Corner
				} else if (is_left || is_bottom) {
					alpha = 255; // Edge
				} else {
					alpha = 0; // Interior (transparent)
				}
			}

			// ARGB8888: alpha in high byte, then R, G, B
			row[x] = ((uint32_t)alpha << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
		}
	}
}

void EFFECT_generateGrid(uint32_t* pixels, int width, int height, int pitch, int scale) {
	EFFECT_generateGridWithColor(pixels, width, height, pitch, scale, 0);
}

void EFFECT_generateSlot(uint32_t* pixels, int width, int height, int pitch, int scale) {
	if (!pixels || width <= 0 || height <= 0 || scale < 1)
		return;

	int pitch_pixels = pitch / 4;

	// Slot mask pattern: staggered brick layout with per-pixel alpha.
	// - Horizontal border at top of each content pixel
	// - Vertical border alternates sides for stagger effect
	// - Phosphor glow below borders (scale 3+)

	uint8_t edge_alpha = (scale == 2) ? 181 : 255;
	uint8_t corner_alpha = (scale == 2) ? 181 : 255;
	uint8_t glow_alpha = 170;

	for (int y = 0; y < height; y++) {
		int content_row = y / scale; // Which content pixel row
		int pos_in_pixel = y % scale; // Position within content pixel (0 to scale-1)
		int is_bottom_half = content_row % 2; // Alternate halves for stagger
		uint32_t* row = pixels + (size_t)y * pitch_pixels;

		for (int x = 0; x < width; x++) {
			int pos_in_pixel_x = x % scale;

			// Vertical border position alternates for stagger effect
			int is_vertical_border;
			if (is_bottom_half) {
				is_vertical_border = (pos_in_pixel_x == scale - 1); // Right border
			} else {
				is_vertical_border = (pos_in_pixel_x == 0); // Left border
			}

			// Horizontal border at top of each content pixel
			if (pos_in_pixel == 0) {
				// Corner where horizontal meets vertical border
				uint8_t alpha = is_vertical_border ? corner_alpha : edge_alpha;
				row[x] = (uint32_t)alpha << 24;
				continue;
			}

			if (is_vertical_border) {
				row[x] = (uint32_t)edge_alpha << 24;
			} else if (pos_in_pixel == 1 && scale >= 3) {
				row[x] = (uint32_t)glow_alpha << 24; // Glow row below horizontal border
			} else {
				row[x] = 0x00000000; // Transparent (slot opening)
			}
		}
	}
}
