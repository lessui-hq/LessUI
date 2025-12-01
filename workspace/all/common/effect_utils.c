/**
 * effect_utils.c - Runtime effect pattern scaling and tiling
 */

#include "effect_utils.h"
#include <stdlib.h>

/**
 * Loads a base pattern PNG and creates a scaled/tiled texture.
 *
 * Uses SDL's built-in nearest-neighbor scaling and simple tiling loops.
 */
SDL_Texture* EFFECT_loadAndTile(SDL_Renderer* renderer, const char* pattern_path, int scale,
                                int target_w, int target_h) {
	if (!renderer || !pattern_path || scale < 1)
		return NULL;

	// Load base pattern PNG
	SDL_Surface* base = IMG_Load(pattern_path);
	if (!base)
		return NULL;

	int pattern_w = base->w * scale;
	int pattern_h = base->h * scale;

	// Create target surface for tiled pattern
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

	// Convert surface to texture
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, tiled);
	SDL_FreeSurface(tiled);

	return texture;
}
