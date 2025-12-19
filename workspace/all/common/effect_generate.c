/**
 * effect_generate.c - Procedural generation of CRT/LCD effect patterns
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
 * Simulates Sony Trinitron-style displays. Symmetric pattern matching
 * zfast_crt shader behavior: darkest at pixel boundaries, brightest at center.
 *
 * Each pixel is {R, G, B, A} where alpha controls darkening.
 * Higher alpha = more darkening, lower alpha = more light through.
 *
 * Rows:
 *   0: Dark scanline (edge - top of content pixel)
 *   1: Bright phosphor center (lowest alpha, most light)
 *   2: Dark scanline (edge - bottom of content pixel)
 *
 * Columns simulate RGB aperture grille:
 *   0: Cyan-ish phosphor
 *   1: Blue phosphor
 *   2: Red phosphor
 */
static const uint8_t GRILLE_TILE[3][3][4] = {
    // Row 0: dark scanline (top edge) - alpha 90 = 35% darkening
    {{0, 1, 1, 90}, {1, 0, 3, 90}, {2, 0, 0, 90}},
    // Row 1: phosphor, low alpha (brightest center)
    {{0, 252, 245, 5}, {0, 0, 243, 6}, {236, 1, 0, 10}},
    // Row 2: dark scanline (bottom edge) - alpha 90 = 35% darkening
    {{0, 1, 1, 90}, {1, 0, 3, 90}, {2, 0, 0, 90}}};

/**
 * Simple scanline pattern - 1x3 tile (no horizontal variation)
 *
 * Symmetric pattern matching zfast_crt shader behavior:
 *   0: Edge (dark) - top of content pixel
 *   1: Center (bright) - middle of content pixel
 *   2: Edge (dark) - bottom of content pixel
 *
 * The shader darkens at pixel boundaries, brightest at center.
 * Alpha 90 = 35% darkening at edges.
 */
static const uint8_t LINE_ALPHA[3] = {90, 2, 90};

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

	// Alpha values matched to MinUI's weighted blending:
	// - 75% brightness = 25% darkening = alpha 64 (scale 2)
	// - 60% brightness = 40% darkening = alpha 102 (scale 3+)
	// - 40% brightness = 60% darkening = alpha 153 (scale 3+ corner)

	for (int y = 0; y < height; y++) {
		int cell_y = y % scale;
		int is_bottom = (cell_y == scale - 1);
		uint32_t* row = pixels + (size_t)y * pitch_pixels;

		for (int x = 0; x < width; x++) {
			int cell_x = x % scale;
			int is_left = (cell_x == 0);

			uint8_t alpha;
			if (scale == 2) {
				// Scale 2: top row and left column dimmed, bottom-right clear
				int is_interior = (cell_x == scale - 1) && (cell_y == scale - 1);
				alpha = is_interior ? 0 : 64;
			} else {
				// Scale 3+: left column and bottom row dimmed, corner extra dark
				if (is_left && is_bottom) {
					alpha = 153; // 40% brightness (corner)
				} else if (is_left || is_bottom) {
					alpha = 102; // 60% brightness (edges)
				} else {
					alpha = 0; // 100% brightness (interior)
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

	// Slot mask pattern scaled to content pixels (like LINE/CRT):
	// - One slot "half" per content pixel row
	// - Horizontal border at top of each content pixel
	// - Vertical border alternates sides for stagger effect
	// - Graduated alpha like GRID for softer edges
	// - Glow row below horizontal border (scale 3+)

	// Alpha values matched to GRID for consistency:
	// - 64 = 25% darkening (scale 2, all borders same)
	// - 102 = 40% darkening (scale 3+, edges)
	// - 153 = 60% darkening (scale 3+, corners/intersections)
	// - 60 = subtle phosphor glow (~24% darkening, softer than edges)
	uint8_t edge_alpha = (scale == 2) ? 64 : 102;
	uint8_t corner_alpha = (scale == 2) ? 64 : 153;
	uint8_t glow_alpha = 60;

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
