/**
 * effect_utils.c - Runtime effect pattern scaling and tiling
 */

#include "effect_utils.h"

#include <stdlib.h>

#include "render_common.h"

/**
 * Helper to apply color tint to a surface's non-transparent pixels.
 * Replaces RGB values while preserving alpha channel.
 */
static void tintSurface(SDL_Surface* surface, int color) {
	if (!surface || !color)
		return;

	// Convert RGB565 to RGB888
	uint8_t r, g, b;
	RENDER_rgb565ToRgb888(color, &r, &g, &b);

	// Lock surface for direct pixel access
	if (SDL_MUSTLOCK(surface))
		SDL_LockSurface(surface);

	uint32_t* pixels = (uint32_t*)surface->pixels;
	int pixel_count = surface->w * surface->h;

	for (int i = 0; i < pixel_count; i++) {
		uint32_t pixel = pixels[i];
		uint8_t a = (pixel >> 24) & 0xFF;
		// Only tint pixels that have alpha (are visible)
		if (a > 0) {
			pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}

	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

/**
 * Loads a base pattern PNG and creates a scaled/tiled texture.
 *
 * Uses SDL's built-in nearest-neighbor scaling and simple tiling loops.
 */
SDL_Texture* EFFECT_loadAndTile(SDL_Renderer* renderer, const char* pattern_path, int scale,
                                int target_w, int target_h) {
	return EFFECT_loadAndTileWithColor(renderer, pattern_path, scale, target_w, target_h, 0);
}

/**
 * Loads a base pattern PNG, applies color tinting, and creates a scaled/tiled texture.
 */
SDL_Texture* EFFECT_loadAndTileWithColor(SDL_Renderer* renderer, const char* pattern_path,
                                         int scale, int target_w, int target_h, int color) {
	if (!renderer || !pattern_path || scale < 1)
		return NULL;

	// Load base pattern PNG
	SDL_Surface* base = IMG_Load(pattern_path);
	if (!base)
		return NULL;

	int pattern_w = base->w * scale;
	int pattern_h = base->h * scale;

	// Create target surface for tiled pattern (ARGB8888 format)
	SDL_Surface* tiled = SDL_CreateRGBSurface(0, target_w, target_h, 32, 0x00FF0000, 0x0000FF00,
	                                          0x000000FF, 0xFF000000);
	if (!tiled) {
		SDL_FreeSurface(base);
		return NULL;
	}

	// Tile the scaled pattern across the target surface
	for (int y = 0; y < target_h; y += pattern_h) {
		for (int x = 0; x < target_w; x += pattern_w) {
			SDL_Rect dst = {x, y, pattern_w, pattern_h};
			// SDL_BlitScaled does nearest-neighbor scaling automatically
			SDL_BlitScaled(base, NULL, tiled, &dst);
		}
	}

	SDL_FreeSurface(base);

	// Apply color tinting if specified
	if (color) {
		tintSurface(tiled, color);
	}

	// Convert surface to texture
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, tiled);
	SDL_FreeSurface(tiled);

	return texture;
}
